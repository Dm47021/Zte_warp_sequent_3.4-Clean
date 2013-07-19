/*
 * =====================================================================================
 *
 *       Filename:  lcdc_panel_wvga_oled.c
 *
 *    Description:  Samsung WVGA panel(480x800) driver 
 *
 *        Version:  1.0
 *        Created:  03/25/2010 02:05:46 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Lu Ya
 *        Company:  ZTE Corp.
 *
 * =====================================================================================
 */
/* ========================================================================================
when         who        what, where, why                                  comment tag
--------     ----       -----------------------------                --------------------------


==========================================================================================*/


#include "msm_fb.h"
#include <asm/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>

#define GPIO_LCD_BL_SC_OUT 2
#define GPIO_LCD_BL_EN
static int lcd_id_pin =120,lcd_id_val =0;//default value


static LCD_PANEL_TYPE g_lcd_panel_type = LCD_PANEL_NONE;

static boolean is_firsttime = true;		
extern u32 LcdPanleID;   
static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;
static int panel_reset;
static bool onewiremode = true;

static struct msm_panel_common_pdata * lcdc_tft_pdata;


static void lcdc_truly_nt35516_init(void);
static void lcdc_lead_nt35516_auo_init(void);
static void lcdc_boe_otm9608a_init(void);
static void lcdc_set_bl(struct msm_fb_data_type *mfd);
void lcdc_truly_sleep(void);
static void spi_init(void);
static int lcdc_panel_on(struct platform_device *pdev);
static int lcdc_panel_off(struct platform_device *pdev);
void lcd_panel_init(void);


static void select_1wire_mode(void)
{
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(120);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(280);				
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(650);				
	
}

static void send_bkl_address(void)
{
	unsigned int i,j;
	i = 0x72;
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(10);
	//printk("[LY] send_bkl_address \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(10);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(180);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(10);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);

}

static void send_bkl_data(int level)
{
	unsigned int i,j;
	i = level & 0x1F;
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
	udelay(10);
	//printk("[LY] send_bkl_data \n");
	for(j = 0; j < 8; j++)
	{
		if(i & 0x80)
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(10);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(180);
		}
		else
		{
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
			udelay(180);
			gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);
			udelay(10);
		}
		i <<= 1;
	}
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
	udelay(10);
	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 1);

}


static void lcdc_set_bl(struct msm_fb_data_type *mfd)
{
       /*value range is 1--32*/
    int current_lel = mfd->bl_level;
    unsigned long flags;


    printk("[ZYF] lcdc_set_bl level=%d, %d\n", 
		   current_lel , mfd->panel_power_on);


    if(!mfd->panel_power_on)
	{
    	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);			
	    return;
    }

    if(current_lel < 1)
    {
        current_lel = 0;
    }
    if(current_lel > 28)
    {
        current_lel = 28;
    }



   
    local_irq_save(flags);
    if(current_lel==0)
    {
    	gpio_direction_output(GPIO_LCD_BL_SC_OUT, 0);
		mdelay(3);
		onewiremode = FALSE;
			
    }
    else 
	{
		if(!onewiremode)	//select 1 wire mode
		{
	//	       mdelay(100);
			printk("[LY] before select_1wire_mode\n");
			select_1wire_mode();
			onewiremode = TRUE;
		}
		send_bkl_address();
		send_bkl_data(current_lel-1);

	}
    local_irq_restore(flags);
}

static int lcdc_panel_on(struct platform_device *pdev)
{

	spi_init();


	if(!is_firsttime)
	{
		lcd_panel_init();
		
	}
	else
	{
		is_firsttime = false;
	}

	return 0;
}
static void gpio_lcd_truly_emuspi_read_one_index(unsigned int addr,unsigned int *data)
{
	unsigned int i;
	int j;
       unsigned int dbit,bits1;
	i=0x20;
	gpio_direction_output(spi_sclk, 0);	
	gpio_direction_output(spi_cs, 0);
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x8000)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	gpio_direction_output(spi_cs, 1);
	msleep(1);
	gpio_direction_output(spi_cs, 0);
	i=0x00;
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}

	gpio_direction_output(spi_cs, 1);
	msleep(1);
	gpio_direction_output(spi_cs, 0);

	i=0xc0;
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	bits1=0;
	gpio_direction_input(spi_sdi);
	for (j = 0; j < 8; j++) {
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		dbit=gpio_get_value(spi_sdi);		
		bits1 = 2*bits1+dbit;
	}
	*data =  bits1;
	gpio_direction_output(spi_sdo,1);
	gpio_direction_output(spi_cs, 1);
}

static void gpio_lcd_truly_emuspi_write_one_index(unsigned int addr,unsigned short data)
{
	unsigned int i;
	int j;

	i=0x20;
	gpio_direction_output(spi_sclk, 0);	
	gpio_direction_output(spi_cs, 0);
	for (j = 0; j < 8; j++) 
	{
		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x8000)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	gpio_direction_output(spi_cs, 1);
	gpio_direction_output(spi_cs, 0);

	i=0x00;
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		//gpio_direction_output(spi_sclk, 0);	
		/*udelay(4);*/
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}

	gpio_direction_output(spi_cs, 1);
	gpio_direction_output(spi_cs, 0);
	i=0x40;
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}
	i = data ;
	for (j = 0; j < 8; j++) {

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}
	gpio_direction_output(spi_cs, 1);
}
static void gpio_lcd_truly_emuspi_write_cmd(unsigned int addr,unsigned short data)
{
	unsigned int i;
	int j;

	i=0x20;
	gpio_direction_output(spi_sclk, 0);	
	gpio_direction_output(spi_cs, 0);
	for (j = 0; j < 8; j++) 
	{
		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x8000)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	gpio_direction_output(spi_cs, 1);
	gpio_direction_output(spi_cs, 0);

	i=0x00;
	for (j = 0; j < 8; j++) 
	{

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	i = addr ;
	for (j = 0; j < 8; j++) {

		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}
	gpio_direction_output(spi_cs, 1);
	
}

void lcdc_truly_nt35516_init(void)
{
#if 1
   printk("lcd init start\n");
gpio_lcd_truly_emuspi_write_one_index(0xFF00,0xAA);
gpio_lcd_truly_emuspi_write_one_index(0xFF01,0x55);
gpio_lcd_truly_emuspi_write_one_index(0xFF02,0x25);
gpio_lcd_truly_emuspi_write_one_index(0xFF03,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xFF04,0x01);

gpio_lcd_truly_emuspi_write_one_index(0xF200,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF201,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF202,0x4A);
gpio_lcd_truly_emuspi_write_one_index(0xF203,0x0A);
gpio_lcd_truly_emuspi_write_one_index(0xF204,0xA8);
gpio_lcd_truly_emuspi_write_one_index(0xF205,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF206,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF207,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF208,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF209,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20A,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20B,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20C,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20D,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20E,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF20F,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF210,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF211,0x0B);
gpio_lcd_truly_emuspi_write_one_index(0xF212,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF213,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF214,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF215,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF216,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF217,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF218,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF219,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF21A,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF21B,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF21C,0x40);
gpio_lcd_truly_emuspi_write_one_index(0xF21D,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xF21E,0x51);
gpio_lcd_truly_emuspi_write_one_index(0xF21F,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF220,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xF221,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xF222,0x01);

gpio_lcd_truly_emuspi_write_one_index(0xF300,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xF301,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xF302,0x07);
gpio_lcd_truly_emuspi_write_one_index(0xF303,0x45);
gpio_lcd_truly_emuspi_write_one_index(0xF304,0x88);
gpio_lcd_truly_emuspi_write_one_index(0xF305,0xD1);
gpio_lcd_truly_emuspi_write_one_index(0xF306,0x0D);

gpio_lcd_truly_emuspi_write_one_index(0xF000,0x55);
gpio_lcd_truly_emuspi_write_one_index(0xF001,0xAA);
gpio_lcd_truly_emuspi_write_one_index(0xF002,0x52);
gpio_lcd_truly_emuspi_write_one_index(0xF003,0x08);
gpio_lcd_truly_emuspi_write_one_index(0xF004,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xb000,0x78);

gpio_lcd_truly_emuspi_write_one_index(0xB100,0xCC);
gpio_lcd_truly_emuspi_write_one_index(0xB101,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xB102,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xB800,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xB801,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xB802,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xB803,0x02);

gpio_lcd_truly_emuspi_write_one_index(0xBC00,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBC01,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBC02,0x00);


gpio_lcd_truly_emuspi_write_one_index(0xC900,0x63);
gpio_lcd_truly_emuspi_write_one_index(0xC901,0x06);
gpio_lcd_truly_emuspi_write_one_index(0xC902,0x0D);
gpio_lcd_truly_emuspi_write_one_index(0xC903,0x1A);
gpio_lcd_truly_emuspi_write_one_index(0xC904,0x17);
gpio_lcd_truly_emuspi_write_one_index(0xC905,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xF000,0x55);
gpio_lcd_truly_emuspi_write_one_index(0xF001,0xAA);
gpio_lcd_truly_emuspi_write_one_index(0xF002,0x52);
gpio_lcd_truly_emuspi_write_one_index(0xF003,0x08);
gpio_lcd_truly_emuspi_write_one_index(0xF004,0x01);

gpio_lcd_truly_emuspi_write_one_index(0xB000,0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB001,0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB002,0x05);

gpio_lcd_truly_emuspi_write_one_index(0xB100,0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB101,0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB102,0x05);

gpio_lcd_truly_emuspi_write_one_index(0xB200,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xB201,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xB202,0x01);

gpio_lcd_truly_emuspi_write_one_index(0xB300,0x0E);
gpio_lcd_truly_emuspi_write_one_index(0xB301,0x0E);
gpio_lcd_truly_emuspi_write_one_index(0xB302,0x0E);

gpio_lcd_truly_emuspi_write_one_index(0xB400,0x0a);//
gpio_lcd_truly_emuspi_write_one_index(0xB401,0x0a);
gpio_lcd_truly_emuspi_write_one_index(0xB402,0x0a);

gpio_lcd_truly_emuspi_write_one_index(0xB600,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xB601,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xB602,0x44);

gpio_lcd_truly_emuspi_write_one_index(0xB700,0x34);
gpio_lcd_truly_emuspi_write_one_index(0xB701,0x34);
gpio_lcd_truly_emuspi_write_one_index(0xB702,0x34);

gpio_lcd_truly_emuspi_write_one_index(0xB800,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xB801,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xB802,0x10);

gpio_lcd_truly_emuspi_write_one_index(0xB900,0x26);
gpio_lcd_truly_emuspi_write_one_index(0xB901,0x26);
gpio_lcd_truly_emuspi_write_one_index(0xB902,0x26);

gpio_lcd_truly_emuspi_write_one_index(0xBA00,0x34);
gpio_lcd_truly_emuspi_write_one_index(0xBA01,0x34);
gpio_lcd_truly_emuspi_write_one_index(0xBA02,0x34);

gpio_lcd_truly_emuspi_write_one_index(0xBC00,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBC01,0xC8);
gpio_lcd_truly_emuspi_write_one_index(0xBC02,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xBD00,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBD01,0xC8);
gpio_lcd_truly_emuspi_write_one_index(0xBD02,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xBE00,0x71);

gpio_lcd_truly_emuspi_write_one_index(0xC000,0x04);
gpio_lcd_truly_emuspi_write_one_index(0xC001,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCA00,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xD000,0x0A);
gpio_lcd_truly_emuspi_write_one_index(0xD001,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xD002,0x0D);
gpio_lcd_truly_emuspi_write_one_index(0xD003,0x0F);

gpio_lcd_truly_emuspi_write_one_index(0xD100,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD101,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xD102,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD103,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xD104,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD105,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xD106,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD107,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xD108,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD109,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xD10A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD10B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xD10C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD10D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xD10E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD10F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xD200,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD201,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xD202,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD203,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xD204,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD205,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xD206,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD207,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xD208,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD209,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xD20A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD20B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xD20C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD20D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xD20E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD20F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xD300,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD301,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xD302,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD303,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xD304,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD305,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xD306,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD307,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xD308,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD309,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xD30A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD30B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xD30C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD30D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xD30E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD30F,0xDF);

gpio_lcd_truly_emuspi_write_one_index(0xD400,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD401,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xD402,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD403,0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xD500,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD501,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xD502,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD503,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xD504,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD505,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xD506,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD507,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xD508,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD509,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xD50A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD50B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xD50C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD50D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xD50E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD50F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xD600,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD601,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xD602,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD603,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xD604,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD605,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xD606,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD607,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xD608,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD609,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xD60A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD60B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xD60C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD60D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xD60E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD60F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xD700,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD701,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xD702,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD703,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xD704,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD705,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xD706,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD707,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xD708,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD709,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xD70A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD70B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xD70C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD70D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xD70E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD70F,0xDF);

gpio_lcd_truly_emuspi_write_one_index(0xD800,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD801,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xD802,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xD803,0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xD900,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD901,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xD902,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD903,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xD904,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD905,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xD906,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD907,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xD908,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD909,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xD90A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD90B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xD90C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD90D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xD90E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xD90F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xDD00,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xDD01,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xDD02,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xDD03,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xDD04,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD05,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xDD06,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD07,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xDD08,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD09,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xDD0A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD0B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xDD0C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD0D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xDD0E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xDD0F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xDE00,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE01,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xDE02,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE03,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xDE04,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE05,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xDE06,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE07,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xDE08,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE09,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xDE0A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE0B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xDE0C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE0D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xDE0E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDE0F,0xDF);

gpio_lcd_truly_emuspi_write_one_index(0xDF00,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDF01,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xDF02,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xDF03,0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xE000,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE001,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xE002,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE003,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xE004,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE005,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xE006,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE007,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xE008,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE009,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xE00A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE00B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xE00C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE00D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xE00E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE00F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xE100,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE101,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xE102,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE103,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xE104,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE105,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xE106,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE107,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xE108,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE109,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xE10A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE10B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xE10C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE10D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xE10E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE10F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xE200,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE201,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xE202,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE203,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xE204,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE205,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xE206,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE207,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xE208,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE209,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xE20A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE20B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xE20C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE20D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xE20E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE20F,0xDF);

gpio_lcd_truly_emuspi_write_one_index(0xE300,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE301,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xE302,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE303,0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xE400,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE401,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xE402,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE403,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xE404,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE405,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xE406,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE407,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xE408,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE409,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xE40A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE40B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xE40C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE40D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xE40E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE40F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xE500,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE501,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xE502,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE503,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xE504,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE505,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xE506,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE507,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xE508,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE509,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xE50A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE50B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xE50C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE50D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xE50E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE50F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xE600,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE601,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xE602,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE603,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xE604,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE605,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xE606,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE607,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xE608,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE609,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xE60A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE60B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xE60C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE60D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xE60E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE60F,0xDF);

gpio_lcd_truly_emuspi_write_one_index(0xE700,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE701,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xE702,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xE703,0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xE800,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE801,0x70);
gpio_lcd_truly_emuspi_write_one_index(0xE802,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE803,0xCE);
gpio_lcd_truly_emuspi_write_one_index(0xE804,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE805,0xF7);
gpio_lcd_truly_emuspi_write_one_index(0xE806,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE807,0x10);
gpio_lcd_truly_emuspi_write_one_index(0xE808,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE809,0x21);
gpio_lcd_truly_emuspi_write_one_index(0xE80A,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE80B,0x44);
gpio_lcd_truly_emuspi_write_one_index(0xE80C,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE80D,0x62);
gpio_lcd_truly_emuspi_write_one_index(0xE80E,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE80F,0x8D);

gpio_lcd_truly_emuspi_write_one_index(0xE900,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE901,0xAF);
gpio_lcd_truly_emuspi_write_one_index(0xE902,0x01);
gpio_lcd_truly_emuspi_write_one_index(0xE903,0xE4);
gpio_lcd_truly_emuspi_write_one_index(0xE904,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE905,0x0C);
gpio_lcd_truly_emuspi_write_one_index(0xE906,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE907,0x4D);
gpio_lcd_truly_emuspi_write_one_index(0xE908,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE909,0x82);
gpio_lcd_truly_emuspi_write_one_index(0xE90A,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE90B,0x84);
gpio_lcd_truly_emuspi_write_one_index(0xE90C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE90D,0xB8);
gpio_lcd_truly_emuspi_write_one_index(0xE90E,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xE90F,0xF0);

gpio_lcd_truly_emuspi_write_one_index(0xEA00,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA01,0x14);
gpio_lcd_truly_emuspi_write_one_index(0xEA02,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA03,0x42);
gpio_lcd_truly_emuspi_write_one_index(0xEA04,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA05,0x5E);
gpio_lcd_truly_emuspi_write_one_index(0xEA06,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA07,0x80);
gpio_lcd_truly_emuspi_write_one_index(0xEA08,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA09,0x97);
gpio_lcd_truly_emuspi_write_one_index(0xEA0A,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA0B,0xB0);
gpio_lcd_truly_emuspi_write_one_index(0xEA0C,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA0D,0xC0);
gpio_lcd_truly_emuspi_write_one_index(0xEA0E,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEA0F,0xDF);


gpio_lcd_truly_emuspi_write_one_index(0xEB00,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEB01,0xFD);
gpio_lcd_truly_emuspi_write_one_index(0xEB02,0x03);
gpio_lcd_truly_emuspi_write_one_index(0xEB03,0xFF);


gpio_lcd_truly_emuspi_write_one_index(0x3500,0x00);
gpio_lcd_truly_emuspi_write_one_index(0x3a00,0x77);


gpio_lcd_truly_emuspi_write_one_index(0x3600,0x00);//Ðý×ª0XD0
gpio_lcd_truly_emuspi_write_cmd(0x1100,0x00); //Sleep Out
msleep(120);

gpio_lcd_truly_emuspi_write_cmd(0x2900,0x00); //Display On
#endif
}
static void lcdc_lead_nt35516_auo_init(void)
{

//*************************************
// Select CMD2, Page 0
//*************************************
gpio_lcd_truly_emuspi_write_one_index(0xF000, 0x55);
gpio_lcd_truly_emuspi_write_one_index(0xF001, 0xAA);
gpio_lcd_truly_emuspi_write_one_index(0xF002, 0x52);
gpio_lcd_truly_emuspi_write_one_index(0xF003, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xF004, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xb000,0x08);
// Source EQ
gpio_lcd_truly_emuspi_write_one_index(0xB800, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xB801, 0x02);
gpio_lcd_truly_emuspi_write_one_index(0xB802, 0x02);
gpio_lcd_truly_emuspi_write_one_index(0xB803, 0x02);

// Z Inversion
gpio_lcd_truly_emuspi_write_one_index(0xBC00, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xBC01, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xBC02, 0x05);

//*************************************
// Select CMD2, Page 1
//*************************************
gpio_lcd_truly_emuspi_write_one_index(0xF000, 0x55);
gpio_lcd_truly_emuspi_write_one_index(0xF001, 0xAA);
gpio_lcd_truly_emuspi_write_one_index(0xF002, 0x52);
gpio_lcd_truly_emuspi_write_one_index(0xF003, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xF004, 0x01);

// AVDD: 6.0V
gpio_lcd_truly_emuspi_write_one_index(0xB000, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB001, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB002, 0x05);

// AVEE: -6.0V
gpio_lcd_truly_emuspi_write_one_index(0xB100, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB101, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xB102, 0x05);

// AVDD: 2.5x VPNL
gpio_lcd_truly_emuspi_write_one_index(0xB600, 0x44);
gpio_lcd_truly_emuspi_write_one_index(0xB601, 0x44);
gpio_lcd_truly_emuspi_write_one_index(0xB602, 0x44);

// AVEE: -2.5x VPNL
gpio_lcd_truly_emuspi_write_one_index(0xB700, 0x34);
gpio_lcd_truly_emuspi_write_one_index(0xB701, 0x34);
gpio_lcd_truly_emuspi_write_one_index(0xB702, 0x34);

// VGLX: AVEE - AVDD + VCL
gpio_lcd_truly_emuspi_write_one_index(0xBA00, 0x24);
gpio_lcd_truly_emuspi_write_one_index(0xBA01, 0x24);
gpio_lcd_truly_emuspi_write_one_index(0xBA02, 0x24);

// VGMP: 4.7V, VGSP=0V
gpio_lcd_truly_emuspi_write_one_index(0xBC00, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBC01, 0x88);
gpio_lcd_truly_emuspi_write_one_index(0xBC02, 0x00);

// VGMN: -4.7V, VGSN=-0V
gpio_lcd_truly_emuspi_write_one_index(0xBD00, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xBD01, 0x88);
gpio_lcd_truly_emuspi_write_one_index(0xBD02, 0x00);

// VCOM
gpio_lcd_truly_emuspi_write_one_index(0xBE00, 0x4F);

//*************************************
// Gamma Code
//*************************************

// Positive Red Gamma
gpio_lcd_truly_emuspi_write_one_index(0xD100,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD101,0x29); 
gpio_lcd_truly_emuspi_write_one_index(0xD102,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD103,0x32); 
gpio_lcd_truly_emuspi_write_one_index(0xD104,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD105,0x46); 
gpio_lcd_truly_emuspi_write_one_index(0xD106,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD107,0x59); 
gpio_lcd_truly_emuspi_write_one_index(0xD108,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD109,0x69); 
gpio_lcd_truly_emuspi_write_one_index(0xD10A,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD10B,0x86); 
gpio_lcd_truly_emuspi_write_one_index(0xD10C,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD10D,0x9C); 
gpio_lcd_truly_emuspi_write_one_index(0xD10E,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD10F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xD200,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD201,0xE6); 
gpio_lcd_truly_emuspi_write_one_index(0xD202,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD203,0x1B); 
gpio_lcd_truly_emuspi_write_one_index(0xD204,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD205,0x43); 
gpio_lcd_truly_emuspi_write_one_index(0xD206,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD207,0x83); 
gpio_lcd_truly_emuspi_write_one_index(0xD208,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD209,0xB2); 
gpio_lcd_truly_emuspi_write_one_index(0xD20A,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD20B,0xB3); 
gpio_lcd_truly_emuspi_write_one_index(0xD20C,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD20D,0xDE); 
gpio_lcd_truly_emuspi_write_one_index(0xD20E,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD20F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xD300,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD301,0x20); 
gpio_lcd_truly_emuspi_write_one_index(0xD302,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD303,0x3F); 
gpio_lcd_truly_emuspi_write_one_index(0xD304,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD305,0x53); 
gpio_lcd_truly_emuspi_write_one_index(0xD306,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD307,0x78); 
gpio_lcd_truly_emuspi_write_one_index(0xD308,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD309,0x95); 
gpio_lcd_truly_emuspi_write_one_index(0xD30A,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD30B,0xC6); 
gpio_lcd_truly_emuspi_write_one_index(0xD30C,0x02);
gpio_lcd_truly_emuspi_write_one_index(0xD30D,0xEE); 
gpio_lcd_truly_emuspi_write_one_index(0xD30E,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD30F,0x30);
 
gpio_lcd_truly_emuspi_write_one_index(0xD400,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD401,0xF0); 
gpio_lcd_truly_emuspi_write_one_index(0xD402,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD403,0xF4);              
                 
// Positive Green Gamma
gpio_lcd_truly_emuspi_write_one_index(0xD500,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD501,0x29); 
gpio_lcd_truly_emuspi_write_one_index(0xD502,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD503,0x32); 
gpio_lcd_truly_emuspi_write_one_index(0xD504,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD505,0x46); 
gpio_lcd_truly_emuspi_write_one_index(0xD506,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD507,0x59); 
gpio_lcd_truly_emuspi_write_one_index(0xD508,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD509,0x69); 
gpio_lcd_truly_emuspi_write_one_index(0xD50A,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD50B,0x86); 
gpio_lcd_truly_emuspi_write_one_index(0xD50C,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD50D,0x9C); 
gpio_lcd_truly_emuspi_write_one_index(0xD50E,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD50F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xD600,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD601,0xE6); 
gpio_lcd_truly_emuspi_write_one_index(0xD602,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD603,0x1B); 
gpio_lcd_truly_emuspi_write_one_index(0xD604,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD605,0x43); 
gpio_lcd_truly_emuspi_write_one_index(0xD606,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD607,0x83); 
gpio_lcd_truly_emuspi_write_one_index(0xD608,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD609,0xB2); 
gpio_lcd_truly_emuspi_write_one_index(0xD60A,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD60B,0xB3); 
gpio_lcd_truly_emuspi_write_one_index(0xD60C,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xD60D,0xDE); 
gpio_lcd_truly_emuspi_write_one_index(0xD60E,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD60F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xD700,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD701,0x20); 
gpio_lcd_truly_emuspi_write_one_index(0xD702,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD703,0x3F); 
gpio_lcd_truly_emuspi_write_one_index(0xD704,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD705,0x53); 
gpio_lcd_truly_emuspi_write_one_index(0xD706,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD707,0x78); 
gpio_lcd_truly_emuspi_write_one_index(0xD708,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD709,0x95); 
gpio_lcd_truly_emuspi_write_one_index(0xD70A,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD70B,0xC6); 
gpio_lcd_truly_emuspi_write_one_index(0xD70C,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xD70D,0xEE); 
gpio_lcd_truly_emuspi_write_one_index(0xD70E,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD70F,0x30);
 
gpio_lcd_truly_emuspi_write_one_index(0xD800,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD801,0xF0); 
gpio_lcd_truly_emuspi_write_one_index(0xD802,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xD803,0xF4);              
                 
// Positive Blue Gamma
gpio_lcd_truly_emuspi_write_one_index(0xD900,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD901,0x29); 
gpio_lcd_truly_emuspi_write_one_index(0xD902,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD903,0x32); 
gpio_lcd_truly_emuspi_write_one_index(0xD904,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD905,0x46); 
gpio_lcd_truly_emuspi_write_one_index(0xD906,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD907,0x59); 
gpio_lcd_truly_emuspi_write_one_index(0xD908,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD909,0x69); 
gpio_lcd_truly_emuspi_write_one_index(0xD90A,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD90B,0x86); 
gpio_lcd_truly_emuspi_write_one_index(0xD90C,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xD90D,0x9C);
gpio_lcd_truly_emuspi_write_one_index(0xD90E,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xD90F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xDD00,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xDD01,0xE6); 
gpio_lcd_truly_emuspi_write_one_index(0xDD02,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD03,0x1B); 
gpio_lcd_truly_emuspi_write_one_index(0xDD04,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD05,0x43); 
gpio_lcd_truly_emuspi_write_one_index(0xDD06,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD07,0x83); 
gpio_lcd_truly_emuspi_write_one_index(0xDD08,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD09,0xB2);
gpio_lcd_truly_emuspi_write_one_index(0xDD0A,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD0B,0xB3); 
gpio_lcd_truly_emuspi_write_one_index(0xDD0C,0x01); 
gpio_lcd_truly_emuspi_write_one_index(0xDD0D,0xDE); 
gpio_lcd_truly_emuspi_write_one_index(0xDD0E,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDD0F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xDE00,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE01,0x20); 
gpio_lcd_truly_emuspi_write_one_index(0xDE02,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE03,0x3F); 
gpio_lcd_truly_emuspi_write_one_index(0xDE04,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE05,0x53); 
gpio_lcd_truly_emuspi_write_one_index(0xDE06,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE07,0x78); 
gpio_lcd_truly_emuspi_write_one_index(0xDE08,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE09,0x95); 
gpio_lcd_truly_emuspi_write_one_index(0xDE0A,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE0B,0xC6); 
gpio_lcd_truly_emuspi_write_one_index(0xDE0C,0x02); 
gpio_lcd_truly_emuspi_write_one_index(0xDE0D,0xEE);
gpio_lcd_truly_emuspi_write_one_index(0xDE0E,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xDE0F,0x30);
 
gpio_lcd_truly_emuspi_write_one_index(0xDF00,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xDF01,0xF0); 
gpio_lcd_truly_emuspi_write_one_index(0xDF02,0x03); 
gpio_lcd_truly_emuspi_write_one_index(0xDF03,0xF4);             
                 
// Negative Red Gamma
gpio_lcd_truly_emuspi_write_one_index(0xE000,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE001,0x29);
gpio_lcd_truly_emuspi_write_one_index(0xE002,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE003,0x32); 
gpio_lcd_truly_emuspi_write_one_index(0xE004,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE005,0x46); 
gpio_lcd_truly_emuspi_write_one_index(0xE006,0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE007,0x59);
gpio_lcd_truly_emuspi_write_one_index(0xE008,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE009,0x69); 
gpio_lcd_truly_emuspi_write_one_index(0xE00A,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE00B,0x86); 
gpio_lcd_truly_emuspi_write_one_index(0xE00C,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE00D,0x9C); 
gpio_lcd_truly_emuspi_write_one_index(0xE00E,0x00); 
gpio_lcd_truly_emuspi_write_one_index(0xE00F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xE100,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE101,0xE6 );
gpio_lcd_truly_emuspi_write_one_index(0xE102,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE103,0x1B );
gpio_lcd_truly_emuspi_write_one_index(0xE104,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE105,0x43 );
gpio_lcd_truly_emuspi_write_one_index(0xE106,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE107,0x83 );
gpio_lcd_truly_emuspi_write_one_index(0xE108,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE109,0xB2 );
gpio_lcd_truly_emuspi_write_one_index(0xE10A,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE10B,0xB3 );
gpio_lcd_truly_emuspi_write_one_index(0xE10C,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE10D,0xDE );
gpio_lcd_truly_emuspi_write_one_index(0xE10E,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE10F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xE200,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE201,0x20 );
gpio_lcd_truly_emuspi_write_one_index(0xE202,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE203,0x3F );
gpio_lcd_truly_emuspi_write_one_index(0xE204,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE205,0x53 );
gpio_lcd_truly_emuspi_write_one_index(0xE206,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE207,0x78 );
gpio_lcd_truly_emuspi_write_one_index(0xE208,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE209,0x95 );
gpio_lcd_truly_emuspi_write_one_index(0xE20A,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE20B,0xC6 );
gpio_lcd_truly_emuspi_write_one_index(0xE20C,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE20D,0xEE );
gpio_lcd_truly_emuspi_write_one_index(0xE20E,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE20F,0x30);

gpio_lcd_truly_emuspi_write_one_index(0xE300,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE301,0xF0 );
gpio_lcd_truly_emuspi_write_one_index(0xE302,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE303,0xF4 );           
                 
// Negative Green Gamma
gpio_lcd_truly_emuspi_write_one_index(0xE400,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE401,0x29 );
gpio_lcd_truly_emuspi_write_one_index(0xE402,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE403,0x32 );
gpio_lcd_truly_emuspi_write_one_index(0xE404,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE405,0x46 );
gpio_lcd_truly_emuspi_write_one_index(0xE406,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE407,0x59 );
gpio_lcd_truly_emuspi_write_one_index(0xE408,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE409,0x69 );
gpio_lcd_truly_emuspi_write_one_index(0xE40A,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE40B,0x86 );
gpio_lcd_truly_emuspi_write_one_index(0xE40C,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE40D,0x9C );
gpio_lcd_truly_emuspi_write_one_index(0xE40E,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE40F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xE500,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE501,0xE6 );
gpio_lcd_truly_emuspi_write_one_index(0xE502,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE503,0x1B );
gpio_lcd_truly_emuspi_write_one_index(0xE504,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE505,0x43 );
gpio_lcd_truly_emuspi_write_one_index(0xE506,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE507,0x83 );
gpio_lcd_truly_emuspi_write_one_index(0xE508,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE509,0xB2 );
gpio_lcd_truly_emuspi_write_one_index(0xE50A,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE50B,0xB3 );
gpio_lcd_truly_emuspi_write_one_index(0xE50C,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE50D,0xDE );
gpio_lcd_truly_emuspi_write_one_index(0xE50E,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE50F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xE600,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE601,0x20 );
gpio_lcd_truly_emuspi_write_one_index(0xE602,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE603,0x3F );
gpio_lcd_truly_emuspi_write_one_index(0xE604,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE605,0x53 );
gpio_lcd_truly_emuspi_write_one_index(0xE606,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE607,0x78 );
gpio_lcd_truly_emuspi_write_one_index(0xE608,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE609,0x95 );
gpio_lcd_truly_emuspi_write_one_index(0xE60A,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE60B,0xC6 );
gpio_lcd_truly_emuspi_write_one_index(0xE60C,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE60D,0xEE );
gpio_lcd_truly_emuspi_write_one_index(0xE60E,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE60F,0x30);
 
gpio_lcd_truly_emuspi_write_one_index(0xE700,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE701,0xF0 );
gpio_lcd_truly_emuspi_write_one_index(0xE702,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xE703,0xF4 );           
                 
// Negative Blue Gamma
gpio_lcd_truly_emuspi_write_one_index(0xE800,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE801,0x29 );
gpio_lcd_truly_emuspi_write_one_index(0xE802,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE803,0x32 );
gpio_lcd_truly_emuspi_write_one_index(0xE804,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE805,0x46 );
gpio_lcd_truly_emuspi_write_one_index(0xE806,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE807,0x59 );
gpio_lcd_truly_emuspi_write_one_index(0xE808,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE809,0x69 );
gpio_lcd_truly_emuspi_write_one_index(0xE80A,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE80B,0x86 );
gpio_lcd_truly_emuspi_write_one_index(0xE80C,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE80D,0x9C );
gpio_lcd_truly_emuspi_write_one_index(0xE80E,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE80F,0xC3);

gpio_lcd_truly_emuspi_write_one_index(0xE900,0x00 );
gpio_lcd_truly_emuspi_write_one_index(0xE901,0xE6 );
gpio_lcd_truly_emuspi_write_one_index(0xE902,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE903,0x1B );
gpio_lcd_truly_emuspi_write_one_index(0xE904,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE905,0x43 );
gpio_lcd_truly_emuspi_write_one_index(0xE906,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE907,0x83 );
gpio_lcd_truly_emuspi_write_one_index(0xE908,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE909,0xB2 );
gpio_lcd_truly_emuspi_write_one_index(0xE90A,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE90B,0xB3 );
gpio_lcd_truly_emuspi_write_one_index(0xE90C,0x01 );
gpio_lcd_truly_emuspi_write_one_index(0xE90D,0xDE );
gpio_lcd_truly_emuspi_write_one_index(0xE90E,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xE90F,0x07);
 
gpio_lcd_truly_emuspi_write_one_index(0xEA00,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA01,0x20 );
gpio_lcd_truly_emuspi_write_one_index(0xEA02,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA03,0x3F );
gpio_lcd_truly_emuspi_write_one_index(0xEA04,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA05,0x53 );
gpio_lcd_truly_emuspi_write_one_index(0xEA06,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA07,0x78 );
gpio_lcd_truly_emuspi_write_one_index(0xEA08,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA09,0x95 );
gpio_lcd_truly_emuspi_write_one_index(0xEA0A,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA0B,0xC6 );
gpio_lcd_truly_emuspi_write_one_index(0xEA0C,0x02 );
gpio_lcd_truly_emuspi_write_one_index(0xEA0D,0xEE );
gpio_lcd_truly_emuspi_write_one_index(0xEA0E,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xEA0F,0x30);
 
gpio_lcd_truly_emuspi_write_one_index(0xEB00,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xEB01,0xF0 );
gpio_lcd_truly_emuspi_write_one_index(0xEB02,0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xEB03,0xF4 );           

//*************************************
// TE On
//*************************************
gpio_lcd_truly_emuspi_write_one_index(0x3500,0x00);

//*************************************
// Sleep Out
//*************************************
gpio_lcd_truly_emuspi_write_one_index(0x1100,0x00);
msleep(120);

//*************************************
// Display On
//*************************************
gpio_lcd_truly_emuspi_write_one_index(0x2900,0x00);
}

void lcdc_boe_otm9608a_init(void)
{
gpio_lcd_truly_emuspi_write_one_index(0xFF00, 0x96);
gpio_lcd_truly_emuspi_write_one_index(0xFF01, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xFF02, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xFF80, 0x96);
gpio_lcd_truly_emuspi_write_one_index(0xFF81, 0x08);

gpio_lcd_truly_emuspi_write_one_index(0xA000,0x00);

gpio_lcd_truly_emuspi_write_one_index(0xB282, 0x20);//pclk pol

gpio_lcd_truly_emuspi_write_one_index(0xB380, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xB381, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xB382, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xB383, 0x21);
gpio_lcd_truly_emuspi_write_one_index(0xB384, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xB392, 0x01);

gpio_lcd_truly_emuspi_write_one_index(0xB3C0, 0x19);//20120703

gpio_lcd_truly_emuspi_write_one_index(0xC080, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC081, 0x48);
gpio_lcd_truly_emuspi_write_one_index(0xC082, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC083, 0x08);//0X10
gpio_lcd_truly_emuspi_write_one_index(0xC084, 0x08);//0X10
gpio_lcd_truly_emuspi_write_one_index(0xC085, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC086, 0x48);
gpio_lcd_truly_emuspi_write_one_index(0xC087, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xC088, 0x10);

gpio_lcd_truly_emuspi_write_one_index(0xC092, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC093, 0x17);
gpio_lcd_truly_emuspi_write_one_index(0xC094, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC095, 0x1A);

gpio_lcd_truly_emuspi_write_one_index(0xC0A2, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xC0A3, 0x10);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xC0A4, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xC0B3, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC0B4, 0x50);

gpio_lcd_truly_emuspi_write_one_index(0xC181,0x66);//20120703   0x77);//for frame rate  0x55);//ox55
gpio_lcd_truly_emuspi_write_one_index(0xC1A1, 0x08);//pan add 

gpio_lcd_truly_emuspi_write_one_index(0xC480, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xC481, 0x00);//20120703  0x84);
gpio_lcd_truly_emuspi_write_one_index(0xC482, 0xFA);

gpio_lcd_truly_emuspi_write_one_index(0xC4A0, 0x33);
gpio_lcd_truly_emuspi_write_one_index(0xC4A1, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xC4A2, 0x90);
gpio_lcd_truly_emuspi_write_one_index(0xC4A3, 0x2B);
gpio_lcd_truly_emuspi_write_one_index(0xC4A4, 0x33);
gpio_lcd_truly_emuspi_write_one_index(0xC4A5, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xC4A6, 0x90);
gpio_lcd_truly_emuspi_write_one_index(0xC4A7, 0x54);

gpio_lcd_truly_emuspi_write_one_index(0xC580, 0x08);//0X08 0x0c
gpio_lcd_truly_emuspi_write_one_index(0xC581, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC582, 0x90);
gpio_lcd_truly_emuspi_write_one_index(0xC583, 0x11);

gpio_lcd_truly_emuspi_write_one_index(0xC590, 0x96);//20120703   0xd6
gpio_lcd_truly_emuspi_write_one_index(0xC591, 0x07);//20120717//20120703//0X76 ,0x17
gpio_lcd_truly_emuspi_write_one_index(0xC592, 0x00);//0X06
gpio_lcd_truly_emuspi_write_one_index(0xC593, 0x77);//20120703//0X76
gpio_lcd_truly_emuspi_write_one_index(0xC594, 0x13);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xC595, 0x33);
gpio_lcd_truly_emuspi_write_one_index(0xC596, 0x34);

gpio_lcd_truly_emuspi_write_one_index(0xC5A0, 0xD6);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xC5A1, 0x0A);//20120703//0X76
gpio_lcd_truly_emuspi_write_one_index(0xC5A2, 0x00);//20120703//0X06
gpio_lcd_truly_emuspi_write_one_index(0xC5A3, 0x77);////201207030X76
gpio_lcd_truly_emuspi_write_one_index(0xC5A4, 0x13);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xC5A5, 0x33);
gpio_lcd_truly_emuspi_write_one_index(0xC5A6, 0x34);

gpio_lcd_truly_emuspi_write_one_index(0xC5B0, 0x04);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xC5B1,0xF9);//20120717//20120703//20120703//pan for frequency 0x2D);


gpio_lcd_truly_emuspi_write_one_index(0xC680, 0x64);

gpio_lcd_truly_emuspi_write_one_index(0xC6B0, 0x03);
gpio_lcd_truly_emuspi_write_one_index(0xC6B1, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xC6B2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xC6B3, 0x1F);
gpio_lcd_truly_emuspi_write_one_index(0xC6B4, 0x12);

gpio_lcd_truly_emuspi_write_one_index(0xC0E1, 0x9F);

gpio_lcd_truly_emuspi_write_one_index(0xD000, 0x01);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xD100, 0x01);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xD101, 0x01);//20120703

gpio_lcd_truly_emuspi_write_one_index(0xB0B7, 0x10);

gpio_lcd_truly_emuspi_write_one_index(0xB0C0, 0x55);

gpio_lcd_truly_emuspi_write_one_index(0xB0B1, 0x03);
gpio_lcd_truly_emuspi_write_one_index(0xB0B2, 0x06);

gpio_lcd_truly_emuspi_write_one_index(0xCBC0, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBC1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBC2, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBC3, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBC4, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBC5, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBC6, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBC7, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBC8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBC9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBCA, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBCB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBCC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBCD, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBCE, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCBD0, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD3, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBD7, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBD8, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBD9, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBDA, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBDB, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xCBDC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBDD, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCBDE, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCBF0, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF1, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF2, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF3, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF4, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF5, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF6, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF7, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF8, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xCBF9, 0xFF);

gpio_lcd_truly_emuspi_write_one_index(0xCC80, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC81, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC82, 0x09);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCC83, 0x0B);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCC84, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xCC85, 0x25);
gpio_lcd_truly_emuspi_write_one_index(0xCC86, 0x26);
gpio_lcd_truly_emuspi_write_one_index(0xCC87, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC88, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC89, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCC90, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC91, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC92, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC93, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC94, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC95, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC96, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC97, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC98, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC99, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC9A, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC9B, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCC9C, 0x0A);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCC9D, 0x0C);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCC9E, 0x02);

gpio_lcd_truly_emuspi_write_one_index(0xCCA0, 0x25);
gpio_lcd_truly_emuspi_write_one_index(0xCCA1, 0x26);
gpio_lcd_truly_emuspi_write_one_index(0xCCA2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA3, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA7, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCA9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCAA, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCAB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCAC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCAD, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCAE, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCCB0, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCB1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCB2, 0x0c);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCCB3, 0x0a);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCCB4, 0x02);
gpio_lcd_truly_emuspi_write_one_index(0xCCB5, 0x26);
gpio_lcd_truly_emuspi_write_one_index(0xCCB6, 0x25);
gpio_lcd_truly_emuspi_write_one_index(0xCCB7, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCB8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCB9, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCCC0, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC3, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC7, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCC9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCCA, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCCB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCCC, 0x0b);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCCCD, 0x09);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCCCE, 0x01);

gpio_lcd_truly_emuspi_write_one_index(0xCCD0, 0x26);
gpio_lcd_truly_emuspi_write_one_index(0xCCD1, 0x25);
gpio_lcd_truly_emuspi_write_one_index(0xCCD2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD3, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD7, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCD9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCDA, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCDB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCDC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCDD, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCCDE, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCE80, 0x85);//86
gpio_lcd_truly_emuspi_write_one_index(0xCE81, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xCE82, 0x00);//0X26,0X00
gpio_lcd_truly_emuspi_write_one_index(0xCE83, 0x84);//85
gpio_lcd_truly_emuspi_write_one_index(0xCE84, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xCE85, 0x00);//0X26,0X00
gpio_lcd_truly_emuspi_write_one_index(0xCE86, 0x0f);//20120703 //0X0F
gpio_lcd_truly_emuspi_write_one_index(0xCE87, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE88, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE89, 0x0F);
gpio_lcd_truly_emuspi_write_one_index(0xCE8A, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE8B, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCE90, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCE91, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE92, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE93, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCE94, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE95, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE96, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCE97, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE98, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE99, 0xF0);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCE9A, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE9B, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE9C, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCE9D, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCEA0, 0x18);
gpio_lcd_truly_emuspi_write_one_index(0xCEA1, 0x04);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCEA2, 0x03 );
gpio_lcd_truly_emuspi_write_one_index(0xCEA3, 0xC3);//20120703//0XC1,0XC4
gpio_lcd_truly_emuspi_write_one_index(0xCEA4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEA5, 0x00);//20120703//0X26,0X80
gpio_lcd_truly_emuspi_write_one_index(0xCEA6, 0x12);//20120703//0X00,0X14
gpio_lcd_truly_emuspi_write_one_index(0xCEA7, 0x18);
gpio_lcd_truly_emuspi_write_one_index(0xCEA8, 0x03);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCEA9, 0x03);
gpio_lcd_truly_emuspi_write_one_index(0xCEAA, 0xc4);//0XC2,0XC4
gpio_lcd_truly_emuspi_write_one_index(0xCEAB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEAC, 0x00);//20120703//0X26,0X80,0X
gpio_lcd_truly_emuspi_write_one_index(0xCEAD, 0x12);//20120703//0X00,0X14

gpio_lcd_truly_emuspi_write_one_index(0xCEB0, 0x18);
gpio_lcd_truly_emuspi_write_one_index(0xCEB1, 0x06);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCEB2, 0x03);
gpio_lcd_truly_emuspi_write_one_index(0xCEB3, 0xC3);//20120717 //20120703//0XC3,0XC2
gpio_lcd_truly_emuspi_write_one_index(0xCEB4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEB5, 0x00);//20120703//0X26,0X80
gpio_lcd_truly_emuspi_write_one_index(0xCEB6, 0x12);//20120703//0X00,0X14
gpio_lcd_truly_emuspi_write_one_index(0xCEB7, 0x18);
gpio_lcd_truly_emuspi_write_one_index(0xCEB8, 0x05);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCEB9, 0x03);
gpio_lcd_truly_emuspi_write_one_index(0xCEBA, 0xC2);//0XC4,0XC2
gpio_lcd_truly_emuspi_write_one_index(0xCEBB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEBC, 0x00);//20120703//0x26,0X80
gpio_lcd_truly_emuspi_write_one_index(0xCEBD, 0x12);//20120703//0X00,0X14

gpio_lcd_truly_emuspi_write_one_index(0xCEC0, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCEC1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC3, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCEC4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC7, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCEC8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEC9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCECA, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCECB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCECC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCECD, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCED0, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCED1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED3, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCED4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED7, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCED8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCED9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEDA, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCEDB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEDC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCEDD, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCF80, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCF81, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF82, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF83, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCF84, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF85, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF86, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF87, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCF88, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF89, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF8A, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCF8B, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF8C, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF8D, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCF90, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCF91, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF92, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF93, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCF94, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF95, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF96, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF97, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCF98, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF99, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF9A, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCF9B, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF9C, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCF9D, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCFA0, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCFA1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA3, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCFA4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA7, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCFA8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFA9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFAA, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCFAB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFAC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFAD, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCFB0, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCFB1, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB2, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB3, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCFB4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB6, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB7, 0xF0);
gpio_lcd_truly_emuspi_write_one_index(0xCFB8, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFB9, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFBA, 0x10);
gpio_lcd_truly_emuspi_write_one_index(0xCFBB, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFBC, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFBD, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xCFC0, 0x02);
gpio_lcd_truly_emuspi_write_one_index(0xCFC1, 0x02);
gpio_lcd_truly_emuspi_write_one_index(0xCFC2, 0x10);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCFC3, 0x10);//20120703
gpio_lcd_truly_emuspi_write_one_index(0xCFC4, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFC5, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xCFC6, 0x01);
gpio_lcd_truly_emuspi_write_one_index(0xCFC7, 0x04);//20120703//0X04
gpio_lcd_truly_emuspi_write_one_index(0xCFC8, 0x00);//20120703//0X00
gpio_lcd_truly_emuspi_write_one_index(0xCFC9, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xD680, 0x00); //20120703

gpio_lcd_truly_emuspi_write_one_index(0xD700, 0x00);//20120703

gpio_lcd_truly_emuspi_write_one_index(0xD800, 0x75);//20120717//20120703//0X1F
gpio_lcd_truly_emuspi_write_one_index(0xD801, 0x75);//20120717//20120703//0X1F

gpio_lcd_truly_emuspi_write_one_index(0xD900, 0x67);//20120717//20120703//0X24,0X60 vcom


gpio_lcd_truly_emuspi_write_one_index(0xE100, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE101, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xE102, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xE103, 0x0b);
gpio_lcd_truly_emuspi_write_one_index(0xE104, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xE105, 0x0c);
gpio_lcd_truly_emuspi_write_one_index(0xE106, 0x0A);
gpio_lcd_truly_emuspi_write_one_index(0xE107, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xE108, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xE109, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xE10A, 0x0b);
gpio_lcd_truly_emuspi_write_one_index(0xE10B, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xE10C, 0x0D);
gpio_lcd_truly_emuspi_write_one_index(0xE10D, 0x12);
gpio_lcd_truly_emuspi_write_one_index(0xE10E, 0x0B);
gpio_lcd_truly_emuspi_write_one_index(0xE10F, 0x00);

gpio_lcd_truly_emuspi_write_one_index(0xE200, 0x00);
gpio_lcd_truly_emuspi_write_one_index(0xE201, 0x05);
gpio_lcd_truly_emuspi_write_one_index(0xE202, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xE203, 0x0b);
gpio_lcd_truly_emuspi_write_one_index(0xE204, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xE205, 0x0c);
gpio_lcd_truly_emuspi_write_one_index(0xE206, 0x0A);
gpio_lcd_truly_emuspi_write_one_index(0xE207, 0x09);
gpio_lcd_truly_emuspi_write_one_index(0xE208, 0x04);
gpio_lcd_truly_emuspi_write_one_index(0xE209, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xE20A, 0x0b);
gpio_lcd_truly_emuspi_write_one_index(0xE20B, 0x08);
gpio_lcd_truly_emuspi_write_one_index(0xE20C, 0x0D);
gpio_lcd_truly_emuspi_write_one_index(0xE20D, 0x12);
gpio_lcd_truly_emuspi_write_one_index(0xE20E, 0x0B);
gpio_lcd_truly_emuspi_write_one_index(0xE20F, 0x00);


gpio_lcd_truly_emuspi_write_one_index(0xFF00, 0xFF);//close cmd2
gpio_lcd_truly_emuspi_write_one_index(0xFF01, 0xFF);
gpio_lcd_truly_emuspi_write_one_index(0xFF02, 0xFF);

msleep(10);
//*************************************
// Sleep Out
//*************************************
gpio_lcd_truly_emuspi_write_cmd(0x1100,0x00);
msleep(100);

//*************************************
// Display On
//*************************************
gpio_lcd_truly_emuspi_write_cmd(0x2900,0x00);
msleep(50);
}

static void spi_init(void)
{
	spi_sclk = *(lcdc_tft_pdata->gpio_num+ 5);
	spi_cs   = *(lcdc_tft_pdata->gpio_num + 3);
	spi_sdo  = *(lcdc_tft_pdata->gpio_num + 2);
	spi_sdi  = *(lcdc_tft_pdata->gpio_num + 4);
	panel_reset = *(lcdc_tft_pdata->gpio_num + 6);
	lcd_id_pin = *(lcdc_tft_pdata->gpio_num + 1);
		
	printk("spi_init!\n");

	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdo, 1);
	gpio_set_value(spi_cs, 1);
//	mdelay(10);			
	msleep(20);				

}
void lcdc_truly_sleep(void)
{
	    gpio_lcd_truly_emuspi_write_cmd(0x2800,0x00);  
	    msleep(10);
	    gpio_lcd_truly_emuspi_write_cmd(0x1000,0x00);  
	    msleep(120);
}

static int lcdc_panel_off(struct platform_device *pdev)
{
	printk("lcdc_panel_off , g_lcd_panel_type is %d(1 LEAD. 2 TRULY. 3 OLED. )\n",g_lcd_panel_type);
	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_NT35516_QHD:
		case LCD_PANEL_LEAD_NT35516_AUO_QHD:
		case LCD_PANEL_BOE_OTM9608A_QHD:
			lcdc_truly_sleep();
			break;
		default:
			break;
	}
	
	gpio_direction_output(panel_reset, 0);
	gpio_direction_output(spi_sclk, 0);
	if(g_lcd_panel_type!= LCD_PANEL_BOE_OTM9608A_QHD)
		gpio_direction_output(spi_sdi, 0);
	gpio_direction_output(spi_sdo, 0);
	gpio_direction_output(spi_cs, 0);
	return 0;
}
static LCD_PANEL_TYPE lcd_panel_detect(void)
{
  unsigned int id;
   spi_init();
   #if 0
   gpio_lcd_truly_emuspi_write_one_index(0xF000,0x55);
   gpio_lcd_truly_emuspi_write_one_index(0xF001,0xAA);
   gpio_lcd_truly_emuspi_write_one_index(0xF002,0x52);
   gpio_lcd_truly_emuspi_write_one_index(0xF003,0x08);
    gpio_lcd_truly_emuspi_write_one_index(0xF004,0x08);
   gpio_lcd_truly_emuspi_read_one_index(0xC500,&id);
   printk("debug id=0x%x\n",id);
   gpio_lcd_truly_emuspi_read_one_index(0xC501,&id);
   printk("debug id=0x%x\n",id);
   gpio_lcd_truly_emuspi_read_one_index(0xDA00,&id);
   printk("debug id=0x%x\n",id);
   gpio_lcd_truly_emuspi_read_one_index(0xDB00,&id);
   printk("debug id=0x%x\n",id);
   #endif
   lcd_id_val=gpio_get_value(lcd_id_pin);
    printk("\n panmingdong lcd_id_val=%d",lcd_id_val);
    gpio_lcd_truly_emuspi_read_one_index(0xDA00,&id);
    printk("\n panmingdong 0xDA =%x",id);
    if(id == 0x88)
   	return LCD_PANEL_TRULY_NT35516_QHD;
    else
    	{
    		if (lcd_id_val ==0)
			return LCD_PANEL_LEAD_NT35516_AUO_QHD;
		else
			return LCD_PANEL_BOE_OTM9608A_QHD;
    	}
		
}
void lcd_panel_init(void)
{

	gpio_direction_output(panel_reset, 1);
	msleep(5);						
	gpio_direction_output(panel_reset, 0);
	msleep(10);						
	gpio_direction_output(panel_reset, 1);
	msleep(30);

	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_NT35516_QHD:
			lcdc_truly_nt35516_init();
			printk("panmingdong TRULY initialize!\n ");
			break;
		case LCD_PANEL_LEAD_NT35516_AUO_QHD:
			printk("panmingdong LEAD initialize!\n ");
			lcdc_lead_nt35516_auo_init();
			break;	
		case LCD_PANEL_BOE_OTM9608A_QHD:
			lcdc_boe_otm9608a_init();
			printk("panmingdong BOE initialize!\n ");
		default:
			break;
	}
}

static struct msm_fb_panel_data lcdc_tft_panel_data = {
       .panel_info = {.bl_max = 28},
	.on = lcdc_panel_on,
	.off = lcdc_panel_off,
       .set_backlight = lcdc_set_bl,
};

static struct platform_device this_device = {
	.name   = "lcdc_panel_qhd",
	.id	= 1,
	.dev	= {
		.platform_data = &lcdc_tft_panel_data,
	}
};
static int  lcdc_panel_probe(struct platform_device *pdev)
{
	struct msm_panel_info *pinfo;
	int ret;

	if(pdev->id == 0) {     
		lcdc_tft_pdata = pdev->dev.platform_data;
		lcdc_tft_pdata->panel_config_gpio(1);   

		g_lcd_panel_type = lcd_panel_detect();
		if(g_lcd_panel_type==LCD_PANEL_TRULY_NT35516_QHD)
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 35;//20
			pinfo->lcdc.h_front_porch = 20;
			pinfo->lcdc.h_pulse_width = 6;
			pinfo->lcdc.v_back_porch = 37;//32  
			pinfo->lcdc.v_front_porch = 32;
			pinfo->lcdc.v_pulse_width = 6;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			
		}else if(g_lcd_panel_type==LCD_PANEL_LEAD_NT35516_AUO_QHD)
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 32;//20
			pinfo->lcdc.h_front_porch = 20;
			pinfo->lcdc.h_pulse_width = 6;
			pinfo->lcdc.v_back_porch = 32;//32  
			pinfo->lcdc.v_front_porch = 20;
			pinfo->lcdc.v_pulse_width = 6;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			
		}
		else 
		{
			pinfo = &lcdc_tft_panel_data.panel_info;
			pinfo->lcdc.h_back_porch = 20;
			pinfo->lcdc.h_front_porch = 40;
			pinfo->lcdc.h_pulse_width = 20;//10
			pinfo->lcdc.v_back_porch = 40;//4,20
			pinfo->lcdc.v_front_porch = 10;//4
			pinfo->lcdc.v_pulse_width = 4;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
			
		}
		pinfo->xres = 540;
		pinfo->yres = 960;		
		pinfo->type = LCDC_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0;
		pinfo->bpp = 24;     //18
		pinfo->fb_num = 2;
		//pinfo->clk_rate = 12288000;
		//pinfo->clk_rate = 32768000;
		pinfo->clk_rate = 36864000;
		switch(g_lcd_panel_type)
		{	
			case LCD_PANEL_TRULY_NT35516_QHD:
				LcdPanleID=(u32)LCD_PANEL_4P3_NT35516_TRULY;   
				break;
			case LCD_PANEL_LEAD_NT35516_AUO_QHD:
				LcdPanleID=(u32)LCD_PANEL_4P3_NT35516_AUO;
				break;
			case LCD_PANEL_BOE_OTM9608A_QHD:
				LcdPanleID=(u32)LCD_PANEL_4P3_OTM9608A_BOE;
			default:
				break;
		}		
	ret = platform_device_register(&this_device);

    	//ret = platform_device_register(&this_device);
		
		return 0;
	 	
	}
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_panel_qhd",
	},
};



static int  lcdc_oled_panel_init(void)
{
	int ret;
	ret = platform_driver_register(&this_driver);
	return ret;
}

module_init(lcdc_oled_panel_init);

