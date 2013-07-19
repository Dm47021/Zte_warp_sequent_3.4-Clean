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

static LCD_PANEL_TYPE g_lcd_panel_type = LCD_PANEL_NONE;

static boolean is_firsttime = true;
extern u32 LcdPanleID;

//lxt add for get lcd id
extern int get_lcd_id_from_tag(void);
//lxt end

static int spi_cs;
static int spi_sclk;
static int spi_sdi;
static int spi_sdo;
static int panel_reset;
static bool onewiremode = true;
static struct msm_panel_common_pdata * lcdc_tft_pdata;

static void gpio_lcd_truly_emuspi_write_one_index(unsigned short addr);
static void gpio_lcd_truly_emuspi_write_one_data(unsigned short data);
static void gpio_lcd_lead_emuspi_write_one_index(unsigned int addr,unsigned short data);
//static void lcdc_lead_init(void);
//static void lcdc_yushun_init(void);
static void lcd_panel_init(void);
static void lcdc_set_bl(struct msm_fb_data_type *mfd);
void lcdc_lead_sleep(void);
void lcdc_truly_sleep(void);
static void spi_init(void);
static int lcdc_panel_on(struct platform_device *pdev);
static int lcdc_panel_off(struct platform_device *pdev);
static void SPI_Start(void)
{
	gpio_direction_output(spi_cs, 0);
}
static void SPI_Stop(void)
{
	gpio_direction_output(spi_cs, 1);
}

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
	printk("[LY] send_bkl_address \n");
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
	printk("[LY] send_bkl_data \n");
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
	
	if(current_lel > 32)
    {
        current_lel = 32;
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
    pr_err("%s enter!", __func__);
	spi_init();

   
	if(!is_firsttime)
	{
	    printk("WVGA not_firsttime\n");
		lcd_panel_init();
		
	}
	else
	{
	    printk("WVGA is_firsttime\n");		
		//lcd_panel_init();
		is_firsttime = false;
	}
	return 0;
}

static unsigned int gpio_lcd_lead_emuspi_read_one_byte(void)
{
    int j;
    unsigned int dbit,bits1;
    bits1=0;
	gpio_direction_input(spi_sdi);
	for (j = 0; j < 8; j++) {
		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		dbit=gpio_get_value(spi_sdi);
		bits1 = 2*bits1+dbit;
	}
	return (bits1 & 0x000000ff);
}

static void gpio_lcd_lead_emuspi_read_one_index(unsigned int addr,unsigned int *data)
{
	unsigned int i;
	int j;
    unsigned int bits1,bits2,bits3,bits4;
	
	
	gpio_direction_output(spi_cs, 1);
	gpio_direction_output(spi_sclk, 0);	
	gpio_direction_output(spi_cs, 0);
	
	gpio_direction_output(spi_sdo, 0);  // write cmd
	gpio_direction_output(spi_sclk, 1);	

	i = addr ;
	for (j = 0; j < 8; j++) {
        gpio_direction_output(spi_sclk, 0);
		if (i & 0x80)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		//gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);	
		i <<= 1;
	}

	bits1 = gpio_lcd_lead_emuspi_read_one_byte();
	bits1 = gpio_lcd_lead_emuspi_read_one_byte();
	bits2 = gpio_lcd_lead_emuspi_read_one_byte();
	bits3 = gpio_lcd_lead_emuspi_read_one_byte();
	bits4 = gpio_lcd_lead_emuspi_read_one_byte();
	
	*data =  (bits1 << 24) | (bits2 << 16) | (bits3 << 8) | bits4;
	gpio_direction_output(spi_cs, 1);
}

static void gpio_lcd_OTM8018B_emuspi_read_one_index(unsigned int addr,unsigned int *data)
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


static void gpio_lcd_truly_emuspi_write_one_index(unsigned short addr)
{
	unsigned int i;
	int j;

	i = addr | 0x7000;
	for (j = 0; j < 9; j++) {

		if (i & 0x0100)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		/*udelay(4);*/
		gpio_direction_output(spi_sclk, 1);	
		/*udelay(4);*/
		i <<= 1;
	}
}


static void gpio_lcd_lead_emuspi_write_one_index(unsigned int addr,unsigned short data)
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


//lxt add
static void gpio_lcd_write_one_addr(unsigned int addr)
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

}

static void gpio_lcd_write_one_cmd(unsigned short data)
{
	unsigned int i;
		int j;

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

//end 

static void gpio_lcd_truly_emuspi_write_one_data(unsigned short data)
{
	unsigned int i;
	int j;

	i = data | 0x7100;

	for (j = 0; j < 9; j++) {

		if (i & 0x0100)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		/*udelay(4);*/
		gpio_direction_output(spi_sclk, 1);	
		/*udelay(4);*/
		i <<= 1;
	}
}
/*

static void gpio_lcd_truly_emuspi_read_one_para(unsigned short addr, unsigned int *data1)
{
	
	int j,i;
	unsigned int dbit,bits1;		
	i = addr | 0x7000;

	for (j = 0; j < 9; j++) {

		if (i & 0x0100)
			gpio_direction_output(spi_sdo, 1);	
		else
			gpio_direction_output(spi_sdo, 0);	

		gpio_direction_output(spi_sclk, 0);	
		gpio_direction_output(spi_sclk, 1);
		i <<= 1;
	}

	//ret = gpio_config(spi_sdi,0);	

	bits1=0;
	gpio_direction_input(spi_sdi);

	for (j = 0; j < 16; j++) {
 
		gpio_direction_output(spi_sclk, 0);
        dbit=gpio_get_value(spi_sdi);
        gpio_direction_output(spi_sclk, 1);
		bits1 = 2*bits1+dbit;
		
	}
	*data1 =  bits1;
}


static void lcdc_tianma_init(void)
{
//int i = 0;
     // printk("WVGA HX8369+LG lcdc_lead_init\n");	 
	
	SPI_Start(); 
	//LV2 Page 1 enable
gpio_lcd_truly_emuspi_write_one_index(0xF0);
gpio_lcd_truly_emuspi_write_one_data(0x55);
gpio_lcd_truly_emuspi_write_one_data(0xAA);
gpio_lcd_truly_emuspi_write_one_data(0x52);
gpio_lcd_truly_emuspi_write_one_data(0x08);
gpio_lcd_truly_emuspi_write_one_data(0x01);
SPI_Stop(); 
	SPI_Start(); 
//AVDD Set AVDD 5.2V
gpio_lcd_truly_emuspi_write_one_index(0xB0);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
SPI_Stop(); 
	SPI_Start(); 
//AVDD ratio
gpio_lcd_truly_emuspi_write_one_index(0xB6);
gpio_lcd_truly_emuspi_write_one_data(0x44);
gpio_lcd_truly_emuspi_write_one_data(0x44);
gpio_lcd_truly_emuspi_write_one_data(0x44);
 SPI_Stop(); 
	SPI_Start(); 
//AVEE  -5.2V
gpio_lcd_truly_emuspi_write_one_index(0xB1);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
gpio_lcd_truly_emuspi_write_one_data(0x0A);
SPI_Stop(); 
	SPI_Start(); 
//AVEE ratio
gpio_lcd_truly_emuspi_write_one_index(0xB7);
gpio_lcd_truly_emuspi_write_one_data(0x34);
gpio_lcd_truly_emuspi_write_one_data(0x34);
gpio_lcd_truly_emuspi_write_one_data(0x34);
SPI_Stop(); 
	SPI_Start(); 
//VCL  -2.5V
gpio_lcd_truly_emuspi_write_one_index(0xB2);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
//VCL ratio
gpio_lcd_truly_emuspi_write_one_index(0xB8);
gpio_lcd_truly_emuspi_write_one_data(0x34);
gpio_lcd_truly_emuspi_write_one_data(0x34);
gpio_lcd_truly_emuspi_write_one_data(0x34);
SPI_Stop(); 
	SPI_Start(); 
//VGL_REG -10V
gpio_lcd_truly_emuspi_write_one_index(0xB5);
gpio_lcd_truly_emuspi_write_one_data(0x08);
gpio_lcd_truly_emuspi_write_one_data(0x08);
gpio_lcd_truly_emuspi_write_one_data(0x08);
SPI_Stop(); 
	SPI_Start(); 
//VGH 15V  
gpio_lcd_truly_emuspi_write_one_index(0xBF);
gpio_lcd_truly_emuspi_write_one_data(0x01);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xB3);
gpio_lcd_truly_emuspi_write_one_data(0x06);
gpio_lcd_truly_emuspi_write_one_data(0x06);
gpio_lcd_truly_emuspi_write_one_data(0x06);
SPI_Stop(); 
	SPI_Start(); 
//VGH ratio
gpio_lcd_truly_emuspi_write_one_index(0xB9);
gpio_lcd_truly_emuspi_write_one_data(0x24);
gpio_lcd_truly_emuspi_write_one_data(0x24);
gpio_lcd_truly_emuspi_write_one_data(0x24);
SPI_Stop(); 
	SPI_Start(); 
//VGLX ratio
gpio_lcd_truly_emuspi_write_one_index(0xBA);
gpio_lcd_truly_emuspi_write_one_data(0x14);
gpio_lcd_truly_emuspi_write_one_data(0x14);
gpio_lcd_truly_emuspi_write_one_data(0x14);
SPI_Stop(); 
	SPI_Start(); 
//VGMP/VGSP 4.7V/0V
gpio_lcd_truly_emuspi_write_one_index(0xBC);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
//VGMN/VGSN -4.7V/0V
gpio_lcd_truly_emuspi_write_one_index(0xBD);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
//VCOM 1.525V 
gpio_lcd_truly_emuspi_write_one_index(0xBE);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x75);
SPI_Stop(); 
	SPI_Start(); 

//Gamma Setting
gpio_lcd_truly_emuspi_write_one_index(0xD1);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xD2);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xD3);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xD4);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xD5);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
 SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xD6);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x2C);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x84);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0xEA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x11);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x39);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6A);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0xF7);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x3E);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x76);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x78);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xAF);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0xE8);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x0F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x3D);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x5F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x8F);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xA7);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xCB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xDB);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xF4);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFE);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0xFF);
SPI_Stop(); 
	SPI_Start(); 

//LV2 Page 0 enable
gpio_lcd_truly_emuspi_write_one_index(0xF0);
gpio_lcd_truly_emuspi_write_one_data(0x55);
gpio_lcd_truly_emuspi_write_one_data(0xAA);
gpio_lcd_truly_emuspi_write_one_data(0x52);
gpio_lcd_truly_emuspi_write_one_data(0x08);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xC7); 
gpio_lcd_truly_emuspi_write_one_data(0x70);

gpio_lcd_truly_emuspi_write_one_index(0xCA);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x1B);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xBA);
gpio_lcd_truly_emuspi_write_one_data(0x05);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xCC);
gpio_lcd_truly_emuspi_write_one_data(0x01);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xB0); 
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x05);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x05);
gpio_lcd_truly_emuspi_write_one_data(0x02);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xBB);
gpio_lcd_truly_emuspi_write_one_data(0x53);
gpio_lcd_truly_emuspi_write_one_data(0x03);
gpio_lcd_truly_emuspi_write_one_data(0x53);
SPI_Stop(); 
	SPI_Start(); 
//Display control
gpio_lcd_truly_emuspi_write_one_index(0xB1);
gpio_lcd_truly_emuspi_write_one_data(0xFC);
gpio_lcd_truly_emuspi_write_one_data(0x06);
SPI_Stop(); 
	SPI_Start(); 
//Source hold time
gpio_lcd_truly_emuspi_write_one_index(0xB6);
gpio_lcd_truly_emuspi_write_one_data(0x05);
SPI_Stop(); 
	SPI_Start(); 
//Gate EQ control
gpio_lcd_truly_emuspi_write_one_index(0xB7);
gpio_lcd_truly_emuspi_write_one_data(0x80);
gpio_lcd_truly_emuspi_write_one_data(0x80);
SPI_Stop(); 
	SPI_Start(); 
//Source EQ control (Mode 2)
gpio_lcd_truly_emuspi_write_one_index(0xB8);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x07);
gpio_lcd_truly_emuspi_write_one_data(0x07);
gpio_lcd_truly_emuspi_write_one_data(0x07);
SPI_Stop(); 
	SPI_Start(); 
//Inversion mode  (Column)
gpio_lcd_truly_emuspi_write_one_index(0xBC);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
//Frame rate
gpio_lcd_truly_emuspi_write_one_index(0xBD);
gpio_lcd_truly_emuspi_write_one_data(0x01);
gpio_lcd_truly_emuspi_write_one_data(0x6C);
gpio_lcd_truly_emuspi_write_one_data(0x1E);
gpio_lcd_truly_emuspi_write_one_data(0x1D);
gpio_lcd_truly_emuspi_write_one_data(0x00);
SPI_Stop(); 
	SPI_Start(); 
//Timing control 8phase dual side/4H/4delay/RST_EN
gpio_lcd_truly_emuspi_write_one_index(0xC9);
gpio_lcd_truly_emuspi_write_one_data(0x00);
gpio_lcd_truly_emuspi_write_one_data(0x02);
gpio_lcd_truly_emuspi_write_one_data(0x50);
gpio_lcd_truly_emuspi_write_one_data(0x50);
gpio_lcd_truly_emuspi_write_one_data(0x50);
SPI_Stop(); 
	SPI_Start(); 
	
gpio_lcd_truly_emuspi_write_one_index(0x11); //Sleep Out 
SPI_Stop(); 

mdelay(120); 
SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x29); //Display On 
	   SPI_Stop(); 
mdelay(150);
	
	
}

*/

static void lcdc_OTM8018B_CMI(void)
{
//OTM8018B and OTM8009_CMI
gpio_lcd_lead_emuspi_write_one_index(0xFF00, 0x80);
gpio_lcd_lead_emuspi_write_one_index(0xFF01, 0x09);
gpio_lcd_lead_emuspi_write_one_index(0xFF02, 0x01);

gpio_lcd_lead_emuspi_write_one_index(0xFF80, 0x80);
gpio_lcd_lead_emuspi_write_one_index(0xFF81, 0x09);

gpio_lcd_lead_emuspi_write_one_index(0xFF03, 0x01);

gpio_lcd_lead_emuspi_write_one_index(0xC090, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC091, 0x44);
gpio_lcd_lead_emuspi_write_one_index(0xC092, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC093, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC094, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC095, 0x03);


gpio_lcd_lead_emuspi_write_one_index(0xC0A3, 0x1B);

gpio_lcd_lead_emuspi_write_one_index(0xC0B4, 0x55);

gpio_lcd_lead_emuspi_write_one_index(0xC181, 0x66);

gpio_lcd_lead_emuspi_write_one_index(0xC1A1, 0x08);

gpio_lcd_lead_emuspi_write_one_index(0xC1A6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC1A7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC1A8, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xC480, 0x30);
gpio_lcd_lead_emuspi_write_one_index(0xC481, 0x83);

gpio_lcd_lead_emuspi_write_one_index(0xC48a, 0x40);
gpio_lcd_lead_emuspi_write_one_index(0xC582, 0xa3);

gpio_lcd_lead_emuspi_write_one_index(0xC590, 0xd6);
gpio_lcd_lead_emuspi_write_one_index(0xC591, 0x76);
gpio_lcd_lead_emuspi_write_one_index(0xC592, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xC5b1, 0xa9);

gpio_lcd_lead_emuspi_write_one_index(0xB282, 0x20);//lxt add

gpio_lcd_lead_emuspi_write_one_index(0xF580, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xF581, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF582, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xF583, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF584, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xF585, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF586, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xF587, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF588, 0x0E);
gpio_lcd_lead_emuspi_write_one_index(0xF589, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF58A, 0x0F);
gpio_lcd_lead_emuspi_write_one_index(0xF58B, 0x20);

gpio_lcd_lead_emuspi_write_one_index(0xF590, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xF591, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF592, 0x08);
gpio_lcd_lead_emuspi_write_one_index(0xF593, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF594, 0x06);
gpio_lcd_lead_emuspi_write_one_index(0xF595, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF596, 0x0D);
gpio_lcd_lead_emuspi_write_one_index(0xF597, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF598, 0x0B);
gpio_lcd_lead_emuspi_write_one_index(0xF599, 0x18);

gpio_lcd_lead_emuspi_write_one_index(0xF5A0, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xF5A1, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5A2, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xF5A3, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5A4, 0x14);
gpio_lcd_lead_emuspi_write_one_index(0xF5A5, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5A6, 0x14);
gpio_lcd_lead_emuspi_write_one_index(0xF5A7, 0x18);

gpio_lcd_lead_emuspi_write_one_index(0xF5B0, 0x14);
gpio_lcd_lead_emuspi_write_one_index(0xF5B1, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5B2, 0x12);
gpio_lcd_lead_emuspi_write_one_index(0xF5B3, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5B4, 0x13);
gpio_lcd_lead_emuspi_write_one_index(0xF5B5, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5B6, 0x11);
gpio_lcd_lead_emuspi_write_one_index(0xF5B7, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5B8, 0x13);
gpio_lcd_lead_emuspi_write_one_index(0xF5B9, 0x18);
gpio_lcd_lead_emuspi_write_one_index(0xF5BA, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xF5BB, 0x00);


gpio_lcd_lead_emuspi_write_one_index(0xCE80, 0x85);
gpio_lcd_lead_emuspi_write_one_index(0xCE81, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCE82, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCE83, 0x84);
gpio_lcd_lead_emuspi_write_one_index(0xCE84, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCE85, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCE90, 0x33);
gpio_lcd_lead_emuspi_write_one_index(0xCE91, 0x26);
gpio_lcd_lead_emuspi_write_one_index(0xCE92, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCE93, 0x33);
gpio_lcd_lead_emuspi_write_one_index(0xCE94, 0x27);
gpio_lcd_lead_emuspi_write_one_index(0xCE95, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCEA0, 0x38);
gpio_lcd_lead_emuspi_write_one_index(0xCEA1, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEA2, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEA3, 0x20);
gpio_lcd_lead_emuspi_write_one_index(0xCEA4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEA5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEA6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEA7, 0x38);
gpio_lcd_lead_emuspi_write_one_index(0xCEA8, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xCEA9, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEAA, 0x21);
gpio_lcd_lead_emuspi_write_one_index(0xCEAB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEAC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEAD, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCEB0, 0x38);
gpio_lcd_lead_emuspi_write_one_index(0xCEB1, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xCEB2, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEB3, 0x22);
gpio_lcd_lead_emuspi_write_one_index(0xCEB4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEB5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEB6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEB7, 0x38);
gpio_lcd_lead_emuspi_write_one_index(0xCEB8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEB9, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEBA, 0x23);
gpio_lcd_lead_emuspi_write_one_index(0xCEBB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEBC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEBD, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCEC0, 0x30);
gpio_lcd_lead_emuspi_write_one_index(0xCEC1, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEC2, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEC3, 0x24);
gpio_lcd_lead_emuspi_write_one_index(0xCEC4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEC5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEC6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEC7, 0x30);
gpio_lcd_lead_emuspi_write_one_index(0xCEC8, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xCEC9, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCECA, 0x25);
gpio_lcd_lead_emuspi_write_one_index(0xCECB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCECC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCECD, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCED0, 0x30);
gpio_lcd_lead_emuspi_write_one_index(0xCED1, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xCED2, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCED3, 0x26);
gpio_lcd_lead_emuspi_write_one_index(0xCED4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCED5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCED6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCED7, 0x30);
gpio_lcd_lead_emuspi_write_one_index(0xCED8, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCED9, 0x03);
gpio_lcd_lead_emuspi_write_one_index(0xCEDA, 0x27);
gpio_lcd_lead_emuspi_write_one_index(0xCEDB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEDC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCEDD, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCFC7, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCBC0, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBC1, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBC2, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBC3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBC4, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBC5, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBC6, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBC7, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBC8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBC9, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBCA, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBCB, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBCC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBCD, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBCE, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCBD0, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD1, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD2, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBD9, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBDA, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBDB, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBDC, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBDD, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBDE, 0x54);

gpio_lcd_lead_emuspi_write_one_index(0xCBE0, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE1, 0x54);
gpio_lcd_lead_emuspi_write_one_index(0xCBE2, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCBE8, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCC80, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC81, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC82, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC83, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC84, 0x0C);
gpio_lcd_lead_emuspi_write_one_index(0xCC85, 0x0A);
gpio_lcd_lead_emuspi_write_one_index(0xCC86, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xCC87, 0x0E);
gpio_lcd_lead_emuspi_write_one_index(0xCC88, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC89, 0x02);

gpio_lcd_lead_emuspi_write_one_index(0xCC90, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC91, 0x06);
gpio_lcd_lead_emuspi_write_one_index(0xCC92, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC93, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC94, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC95, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC96, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC97, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC98, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC99, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC9A, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC9B, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC9C, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC9D, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCC9E, 0x0B);

gpio_lcd_lead_emuspi_write_one_index(0xCCA0, 0x09);
gpio_lcd_lead_emuspi_write_one_index(0xCCA1, 0x0F);
gpio_lcd_lead_emuspi_write_one_index(0xCCA2, 0x0D);
gpio_lcd_lead_emuspi_write_one_index(0xCCA3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCA4, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xCCA5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCA6, 0x05);
gpio_lcd_lead_emuspi_write_one_index(0xCCA7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCA8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCA9, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCAA, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCAB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCAC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCAD, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCAE, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xCCB0, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCB1, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCB2, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCB3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCB4, 0x0D);
gpio_lcd_lead_emuspi_write_one_index(0xCCB5, 0x0F);
gpio_lcd_lead_emuspi_write_one_index(0xCCB6, 0x09);
gpio_lcd_lead_emuspi_write_one_index(0xCCB7, 0x0B);
gpio_lcd_lead_emuspi_write_one_index(0xCCB8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCB9, 0x05);

gpio_lcd_lead_emuspi_write_one_index(0xCCC0, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC1, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xCCC2, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC4, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC6, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCC9, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCCA, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCCB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCCC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCCD, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCCE, 0x0E);

gpio_lcd_lead_emuspi_write_one_index(0xCCD0, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xCCD1, 0x0A);
gpio_lcd_lead_emuspi_write_one_index(0xCCD2, 0x0C);
gpio_lcd_lead_emuspi_write_one_index(0xCCD3, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCD4, 0x06);
gpio_lcd_lead_emuspi_write_one_index(0xCCD5, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCD6, 0x02);
gpio_lcd_lead_emuspi_write_one_index(0xCCD7, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCD8, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCD9, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCDA, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCDB, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCDC, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCDD, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xCCDE, 0x00);
gpio_lcd_lead_emuspi_write_one_index(0xC480, 0x00);

gpio_lcd_lead_emuspi_write_one_index(0xD800, 0x8F);
gpio_lcd_lead_emuspi_write_one_index(0xD801, 0x8F);
gpio_lcd_lead_emuspi_write_one_index(0xD900, 0x7E);

gpio_lcd_lead_emuspi_write_one_index(0xE100, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xE101, 0x0a);
gpio_lcd_lead_emuspi_write_one_index(0xE102, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xE103, 0x0d);
gpio_lcd_lead_emuspi_write_one_index(0xE104, 0x06);
gpio_lcd_lead_emuspi_write_one_index(0xE105, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xE106, 0x0a);
gpio_lcd_lead_emuspi_write_one_index(0xE107, 0x09);
gpio_lcd_lead_emuspi_write_one_index(0xE108, 0x04);
gpio_lcd_lead_emuspi_write_one_index(0xE109, 0x07);
gpio_lcd_lead_emuspi_write_one_index(0xE10A, 0x0c);
gpio_lcd_lead_emuspi_write_one_index(0xE10B, 0x08);
gpio_lcd_lead_emuspi_write_one_index(0xE10C, 0x0F);
gpio_lcd_lead_emuspi_write_one_index(0xE10D, 0x11);
gpio_lcd_lead_emuspi_write_one_index(0xE10E, 0x0D);
gpio_lcd_lead_emuspi_write_one_index(0xE10F, 0x06);

gpio_lcd_lead_emuspi_write_one_index(0xE200, 0x01);
gpio_lcd_lead_emuspi_write_one_index(0xE201, 0x0a);
gpio_lcd_lead_emuspi_write_one_index(0xE202, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xE203, 0x0d);
gpio_lcd_lead_emuspi_write_one_index(0xE204, 0x06);
gpio_lcd_lead_emuspi_write_one_index(0xE205, 0x10);
gpio_lcd_lead_emuspi_write_one_index(0xE206, 0x0a);
gpio_lcd_lead_emuspi_write_one_index(0xE207, 0x09);
gpio_lcd_lead_emuspi_write_one_index(0xE208, 0x04);
gpio_lcd_lead_emuspi_write_one_index(0xE209, 0x07);
gpio_lcd_lead_emuspi_write_one_index(0xE20A, 0x0c);
gpio_lcd_lead_emuspi_write_one_index(0xE20B, 0x08);
gpio_lcd_lead_emuspi_write_one_index(0xE20C, 0x0f);
gpio_lcd_lead_emuspi_write_one_index(0xE20D, 0x11);
gpio_lcd_lead_emuspi_write_one_index(0xE20E, 0x0d);
gpio_lcd_lead_emuspi_write_one_index(0xE20F, 0x06);

gpio_lcd_write_one_addr(0xD400);
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	
gpio_lcd_write_one_cmd(0x00);	
gpio_lcd_write_one_cmd(0x40);	

gpio_lcd_write_one_addr(0xD500);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x60);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x60);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x5f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x60);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x61);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x62);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x63);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x63);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x64);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x65);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x66);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x67);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x68);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x69);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x69);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x70);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x71);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x72);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x73);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x74);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x74);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x75);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x76);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x77);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x78);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x78);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x79);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x7a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x79);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x78);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x77);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x76);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x76);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x75);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x74);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x73);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x72);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x71);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x71);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x70);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6f);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6e);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6d);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6c);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6b);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x6a);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x69);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x68);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x67);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x66);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x66);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x66);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x65);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x65);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x64);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x64);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x63);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x63);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x63);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x62);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x62);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x61);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x61);
gpio_lcd_write_one_cmd(0x00);
gpio_lcd_write_one_cmd(0x60);


gpio_lcd_lead_emuspi_write_one_index(0xD680, 0x08);
gpio_lcd_lead_emuspi_write_one_index(0xFF00, 0x61);
gpio_lcd_lead_emuspi_write_one_index(0xFF01, 0xFF);
gpio_lcd_lead_emuspi_write_one_index(0xFF02, 0xFF);

gpio_lcd_lead_emuspi_write_one_index(0x3a00, 0x77);

gpio_lcd_lead_emuspi_write_one_index(0x1100,0x0); 
msleep(100);
gpio_lcd_lead_emuspi_write_one_index(0x2900,0x0); 
//gpio_lcd_lead_emuspi_write_one_index(0x2c00,0x0); 


}

static void lcdc_R61408E_LG_init(void)
{
SPI_Start(); 
printk("lcdc_R61408E_LG_init enter\n");
    gpio_lcd_truly_emuspi_write_one_index(0x00B0); //B0h
gpio_lcd_truly_emuspi_write_one_data(0x0004);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00B3); //B3h
gpio_lcd_truly_emuspi_write_one_data(0x0010);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00BD); //BDh
gpio_lcd_truly_emuspi_write_one_data(0x0000);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00C0);//
gpio_lcd_truly_emuspi_write_one_data(0x001B);//VSPL[4]; HSPL[3]; 0; EPL[1]; DPL[0]; 
gpio_lcd_truly_emuspi_write_one_data(0x0066);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00C1);
gpio_lcd_truly_emuspi_write_one_data(0x0023); //SEQ_SEL WCVDC2 EN_6Fh xx REV xx BGR SS
gpio_lcd_truly_emuspi_write_one_data(0x0031); //NL[5:0);
gpio_lcd_truly_emuspi_write_one_data(0x0099); //BLREV[1:0] PTREV GIPPAT[2:0] GIPMODE[1:0]
gpio_lcd_truly_emuspi_write_one_data(0x0021); //GSPF[5:0]
gpio_lcd_truly_emuspi_write_one_data(0x0020); //GSPS[5:0]
gpio_lcd_truly_emuspi_write_one_data(0x0000); //STVG[1:0] STVGA[1:0] xx T_GALH GLOL[1:0]
gpio_lcd_truly_emuspi_write_one_data(0x0010); //DVI[3:0] xx xx FL1 xx
gpio_lcd_truly_emuspi_write_one_data(0x0028); //RTN
gpio_lcd_truly_emuspi_write_one_data(0x000c); //BPX16 VBPE xx BP[4:0]
gpio_lcd_truly_emuspi_write_one_data(0x000C); //EFX16 xx xx FP[4:0]
gpio_lcd_truly_emuspi_write_one_data(0x0000); //ACBF1[1:0] ACF[1:0] ACBR[1:0] ACR[1:0]
gpio_lcd_truly_emuspi_write_one_data(0x0000); //ACBF2[1:0] ACF2[1:0] ACBR2[1:0] ACR2[1:0]
gpio_lcd_truly_emuspi_write_one_data(0x0000); //ACCYC[1:0] VGSET ACFIX[1:0]
gpio_lcd_truly_emuspi_write_one_data(0x0021);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00C2); //C2h inversion
gpio_lcd_truly_emuspi_write_one_data(0x0010);//0x00 1-line;  0x28 Column inversion
gpio_lcd_truly_emuspi_write_one_data(0x0006);
gpio_lcd_truly_emuspi_write_one_data(0x0006);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0003);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
SPI_Stop(); 
	SPI_Start(); 
 
gpio_lcd_truly_emuspi_write_one_index(0x00C8); //c8h
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0017);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x004B);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0015);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x0047);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00C9); //c9h
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0017);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x004B);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0015);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x0047);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00CA); //CAh
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0017);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x004B);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x000E);
gpio_lcd_truly_emuspi_write_one_data(0x0015);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x002E);
gpio_lcd_truly_emuspi_write_one_data(0x0047);
gpio_lcd_truly_emuspi_write_one_data(0x003B);
gpio_lcd_truly_emuspi_write_one_data(0x0028);
gpio_lcd_truly_emuspi_write_one_data(0x0019);
gpio_lcd_truly_emuspi_write_one_data(0x0011);
gpio_lcd_truly_emuspi_write_one_data(0x000A);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00D0); //D0h
gpio_lcd_truly_emuspi_write_one_data(0x0029);
gpio_lcd_truly_emuspi_write_one_data(0x0003);
gpio_lcd_truly_emuspi_write_one_data(0x00CE);
gpio_lcd_truly_emuspi_write_one_data(0x00A6);
gpio_lcd_truly_emuspi_write_one_data(0x000C);
gpio_lcd_truly_emuspi_write_one_data(0x0043);
gpio_lcd_truly_emuspi_write_one_data(0x0020);
gpio_lcd_truly_emuspi_write_one_data(0x0010);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0003);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00D1); //D1h
gpio_lcd_truly_emuspi_write_one_data(0x0018);
gpio_lcd_truly_emuspi_write_one_data(0x000C);
gpio_lcd_truly_emuspi_write_one_data(0x0023);
gpio_lcd_truly_emuspi_write_one_data(0x0003);
gpio_lcd_truly_emuspi_write_one_data(0x0075);
gpio_lcd_truly_emuspi_write_one_data(0x0002);
gpio_lcd_truly_emuspi_write_one_data(0x0050);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00D3); //D3h
gpio_lcd_truly_emuspi_write_one_data(0x0033);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00D5); //D5h
gpio_lcd_truly_emuspi_write_one_data(0x002A);
gpio_lcd_truly_emuspi_write_one_data(0x002A);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00DE); //DEh
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x0051);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00E6); //E6h
gpio_lcd_truly_emuspi_write_one_data(0x0051);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00FA); //FAh
gpio_lcd_truly_emuspi_write_one_data(0x0003);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x00D6); //D6h
gpio_lcd_truly_emuspi_write_one_data(0x0028);
SPI_Stop(); 
mdelay(100);

	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x002A);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0001);
gpio_lcd_truly_emuspi_write_one_data(0x00DF);
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x002B);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
gpio_lcd_truly_emuspi_write_one_data(0x0003);
gpio_lcd_truly_emuspi_write_one_data(0x001F);
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x0036);
gpio_lcd_truly_emuspi_write_one_data(0x0000);
 SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x003A);
gpio_lcd_truly_emuspi_write_one_data(0x0077);
 SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x0011);
SPI_Stop(); 
	
mdelay(200);
SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x0029);
SPI_Stop(); 
mdelay(20);
 SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0x002C);
SPI_Stop();
}


static void lcdc_HX8369a_init(void)
{
//int i = 0;
      printk("WVGA HX8369+LG lcdc_lead_init\n");	 
	
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xB9); //Set_EXTC 
gpio_lcd_truly_emuspi_write_one_data(0xFF); 
gpio_lcd_truly_emuspi_write_one_data(0x83); 
gpio_lcd_truly_emuspi_write_one_data(0x69); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xB1); //Set Power 
gpio_lcd_truly_emuspi_write_one_data(0x01); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x34); 
gpio_lcd_truly_emuspi_write_one_data(0x07); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x0F); 
gpio_lcd_truly_emuspi_write_one_data(0x0F); 
gpio_lcd_truly_emuspi_write_one_data(0x21); 
gpio_lcd_truly_emuspi_write_one_data(0x28); 
gpio_lcd_truly_emuspi_write_one_data(0x3F); 
gpio_lcd_truly_emuspi_write_one_data(0x3F); 
gpio_lcd_truly_emuspi_write_one_data(0x07); 
gpio_lcd_truly_emuspi_write_one_data(0x23); 
gpio_lcd_truly_emuspi_write_one_data(0x01); 
gpio_lcd_truly_emuspi_write_one_data(0xE6); 
gpio_lcd_truly_emuspi_write_one_data(0xE6); 
gpio_lcd_truly_emuspi_write_one_data(0xE6); 
gpio_lcd_truly_emuspi_write_one_data(0xE6); 
gpio_lcd_truly_emuspi_write_one_data(0xE6); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xB2); // SET Display 480x800 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x28); 
gpio_lcd_truly_emuspi_write_one_data(0x0A); 
gpio_lcd_truly_emuspi_write_one_data(0x0A); 
gpio_lcd_truly_emuspi_write_one_data(0x70); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0xFF); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x03); 
gpio_lcd_truly_emuspi_write_one_data(0x03); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x01); 
SPI_Stop(); 
	SPI_Start(); 
gpio_lcd_truly_emuspi_write_one_index(0xB3);
gpio_lcd_truly_emuspi_write_one_data(0x09); 
		
SPI_Stop(); 
SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xB4); // SET Display 480x800 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x18); 
gpio_lcd_truly_emuspi_write_one_data(0x80); 
gpio_lcd_truly_emuspi_write_one_data(0x10); 
gpio_lcd_truly_emuspi_write_one_data(0x01); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xB6); // SET VCOM 
gpio_lcd_truly_emuspi_write_one_data(0x2C); 
gpio_lcd_truly_emuspi_write_one_data(0x2C); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xD5); //SET GIP 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x05); 
gpio_lcd_truly_emuspi_write_one_data(0x03); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x01); 
gpio_lcd_truly_emuspi_write_one_data(0x09); 
gpio_lcd_truly_emuspi_write_one_data(0x10); 
gpio_lcd_truly_emuspi_write_one_data(0x80); 
gpio_lcd_truly_emuspi_write_one_data(0x37); 
gpio_lcd_truly_emuspi_write_one_data(0x37); 
gpio_lcd_truly_emuspi_write_one_data(0x20); 
gpio_lcd_truly_emuspi_write_one_data(0x31); 
gpio_lcd_truly_emuspi_write_one_data(0x46); 
gpio_lcd_truly_emuspi_write_one_data(0x8A); 
gpio_lcd_truly_emuspi_write_one_data(0x57); 
gpio_lcd_truly_emuspi_write_one_data(0x9B); 
gpio_lcd_truly_emuspi_write_one_data(0x20); 
gpio_lcd_truly_emuspi_write_one_data(0x31); 
gpio_lcd_truly_emuspi_write_one_data(0x46); 
gpio_lcd_truly_emuspi_write_one_data(0x8A); 
gpio_lcd_truly_emuspi_write_one_data(0x57); 
gpio_lcd_truly_emuspi_write_one_data(0x9B); 
gpio_lcd_truly_emuspi_write_one_data(0x07); 
gpio_lcd_truly_emuspi_write_one_data(0x0F); 
gpio_lcd_truly_emuspi_write_one_data(0x02); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0xE0); //SET GAMMA 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x06); 
gpio_lcd_truly_emuspi_write_one_data(0x06); 
gpio_lcd_truly_emuspi_write_one_data(0x29); 
gpio_lcd_truly_emuspi_write_one_data(0x2D); 
gpio_lcd_truly_emuspi_write_one_data(0x3F); 
gpio_lcd_truly_emuspi_write_one_data(0x13); 
gpio_lcd_truly_emuspi_write_one_data(0x32); 
gpio_lcd_truly_emuspi_write_one_data(0x08); 
gpio_lcd_truly_emuspi_write_one_data(0x0C); 
gpio_lcd_truly_emuspi_write_one_data(0x0D); 
gpio_lcd_truly_emuspi_write_one_data(0x11); 
gpio_lcd_truly_emuspi_write_one_data(0x14); 
gpio_lcd_truly_emuspi_write_one_data(0x11); 
gpio_lcd_truly_emuspi_write_one_data(0x14); 
gpio_lcd_truly_emuspi_write_one_data(0x0E); 
gpio_lcd_truly_emuspi_write_one_data(0x15); 
gpio_lcd_truly_emuspi_write_one_data(0x00); 
gpio_lcd_truly_emuspi_write_one_data(0x06); 

gpio_lcd_truly_emuspi_write_one_data(0x06); 
gpio_lcd_truly_emuspi_write_one_data(0x29); 
gpio_lcd_truly_emuspi_write_one_data(0x2D); 
gpio_lcd_truly_emuspi_write_one_data(0x3F); 
gpio_lcd_truly_emuspi_write_one_data(0x13); 
gpio_lcd_truly_emuspi_write_one_data(0x32); 
gpio_lcd_truly_emuspi_write_one_data(0x08); 
gpio_lcd_truly_emuspi_write_one_data(0x0C); 
gpio_lcd_truly_emuspi_write_one_data(0x0D); 
gpio_lcd_truly_emuspi_write_one_data(0x11); 
gpio_lcd_truly_emuspi_write_one_data(0x14); 
gpio_lcd_truly_emuspi_write_one_data(0x11); 
gpio_lcd_truly_emuspi_write_one_data(0x14); 
gpio_lcd_truly_emuspi_write_one_data(0x11); 
gpio_lcd_truly_emuspi_write_one_data(0x18); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x3A); //Set COLMOD 
gpio_lcd_truly_emuspi_write_one_data(0x77); 
SPI_Stop(); 
	SPI_Start(); 

gpio_lcd_truly_emuspi_write_one_index(0x11); //Sleep Out 
SPI_Stop(); 

mdelay(120); 
SPI_Start(); 


gpio_lcd_truly_emuspi_write_one_index(0x29); //Display On 
gpio_lcd_truly_emuspi_write_one_index(0x2C); 

	   SPI_Stop(); 
mdelay(150);
	
	
}
/*
void lcdc_yushun_init(void)
{	//for LG+R61408
    printk("WVGA lcdc_yushun_init\n");
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xB0);  // SET password 
        gpio_lcd_truly_emuspi_write_one_data(0x04);   
	SPI_Stop(); 
	SPI_Start(); 
	   gpio_lcd_truly_emuspi_write_one_index(0xB3);  //Set Power 
        gpio_lcd_truly_emuspi_write_one_data(0x12); //                          
        gpio_lcd_truly_emuspi_write_one_data(0x00);
  
                                     
        gpio_lcd_truly_emuspi_write_one_index(0xB6);  //Set Power 
        gpio_lcd_truly_emuspi_write_one_data(0x85); //                          
        gpio_lcd_truly_emuspi_write_one_data(0x00);      
                     
	SPI_Stop(); 

	SPI_Start(); 
       gpio_lcd_truly_emuspi_write_one_index(0xB7);  // SET Display  480x800 
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x80);   
        gpio_lcd_truly_emuspi_write_one_data(0x15);   
        gpio_lcd_truly_emuspi_write_one_data(0x25);      
	SPI_Stop(); 

	SPI_Start(); 
  
        gpio_lcd_truly_emuspi_write_one_index(0xB8);  // SET Display  480x800 
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x0f);   
        gpio_lcd_truly_emuspi_write_one_data(0x0f); 
        gpio_lcd_truly_emuspi_write_one_data(0xff);  
        gpio_lcd_truly_emuspi_write_one_data(0xff);   
		gpio_lcd_truly_emuspi_write_one_data(0xc8);   
        gpio_lcd_truly_emuspi_write_one_data(0xc8);   
        gpio_lcd_truly_emuspi_write_one_data(0x0f); 
        gpio_lcd_truly_emuspi_write_one_data(0x1f);  
        gpio_lcd_truly_emuspi_write_one_data(0x10); 
		gpio_lcd_truly_emuspi_write_one_data(0x10);   
        gpio_lcd_truly_emuspi_write_one_data(0x37);   
        gpio_lcd_truly_emuspi_write_one_data(0x5a); 
        gpio_lcd_truly_emuspi_write_one_data(0x87);  
        gpio_lcd_truly_emuspi_write_one_data(0xde);   
		gpio_lcd_truly_emuspi_write_one_data(0xff);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
        gpio_lcd_truly_emuspi_write_one_data(0x00);  
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
  
	SPI_Stop(); 

	SPI_Start(); 
 
        gpio_lcd_truly_emuspi_write_one_index(0xB9);  // SET VCOM 
        gpio_lcd_truly_emuspi_write_one_data(0x00);          
        gpio_lcd_truly_emuspi_write_one_data(0x00);  
		gpio_lcd_truly_emuspi_write_one_data(0x02);          
        gpio_lcd_truly_emuspi_write_one_data(0x08); 
	SPI_Stop(); 

	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xbd);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);
		SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0xc0);   
        gpio_lcd_truly_emuspi_write_one_data(0x1a);
		gpio_lcd_truly_emuspi_write_one_data(0x87);
		SPI_Stop(); 

	SPI_Start(); 
		
	gpio_lcd_truly_emuspi_write_one_index(0xc1);   
        gpio_lcd_truly_emuspi_write_one_data(0x23);   
        gpio_lcd_truly_emuspi_write_one_data(0x31);   
        gpio_lcd_truly_emuspi_write_one_data(0x99);   
        gpio_lcd_truly_emuspi_write_one_data(0x21);   
        gpio_lcd_truly_emuspi_write_one_data(0x20);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x10);   
        gpio_lcd_truly_emuspi_write_one_data(0x28);   
        gpio_lcd_truly_emuspi_write_one_data(0x0f);   
        gpio_lcd_truly_emuspi_write_one_data(0x0c);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x00);   
        gpio_lcd_truly_emuspi_write_one_data(0x21);   
        gpio_lcd_truly_emuspi_write_one_data(0x01);               
	SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0xc2); 
        gpio_lcd_truly_emuspi_write_one_data(0x28); 
        gpio_lcd_truly_emuspi_write_one_data(0x03); 
        gpio_lcd_truly_emuspi_write_one_data(0x03); 
        gpio_lcd_truly_emuspi_write_one_data(0x02); 
        gpio_lcd_truly_emuspi_write_one_data(0x03); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
		
	SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0xc3); 
        gpio_lcd_truly_emuspi_write_one_data(0x40); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
        gpio_lcd_truly_emuspi_write_one_data(0x03);
		
	SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0x6f); 
        gpio_lcd_truly_emuspi_write_one_data(0x03); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
	SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0xc4); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
        gpio_lcd_truly_emuspi_write_one_data(0x01); 
	SPI_Stop(); 

	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0xc6); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
        gpio_lcd_truly_emuspi_write_one_data(0x00); 
	SPI_Stop(); 

	SPI_Start(); 
         
        gpio_lcd_truly_emuspi_write_one_index(0xc7); 
        gpio_lcd_truly_emuspi_write_one_data(0x11); 
        gpio_lcd_truly_emuspi_write_one_data(0x8d); 
        gpio_lcd_truly_emuspi_write_one_data(0xa0); 
        gpio_lcd_truly_emuspi_write_one_data(0xf5); 
        gpio_lcd_truly_emuspi_write_one_data(0x27); 
		SPI_Stop(); 

	SPI_Start(); 
         
        gpio_lcd_truly_emuspi_write_one_index(0xc8); 
        gpio_lcd_truly_emuspi_write_one_data(0x01); 
        gpio_lcd_truly_emuspi_write_one_data(0x0a); 
        gpio_lcd_truly_emuspi_write_one_data(0x12); 
        gpio_lcd_truly_emuspi_write_one_data(0x1c); 
        gpio_lcd_truly_emuspi_write_one_data(0x2b); 
        gpio_lcd_truly_emuspi_write_one_data(0x45); 
        gpio_lcd_truly_emuspi_write_one_data(0x3f); 
        gpio_lcd_truly_emuspi_write_one_data(0x29); 
        gpio_lcd_truly_emuspi_write_one_data(0x17); 
        gpio_lcd_truly_emuspi_write_one_data(0x13); 
        gpio_lcd_truly_emuspi_write_one_data(0x0f); 
        gpio_lcd_truly_emuspi_write_one_data(0x04); 
        gpio_lcd_truly_emuspi_write_one_data(0x01); 
        gpio_lcd_truly_emuspi_write_one_data(0x0a); 
        gpio_lcd_truly_emuspi_write_one_data(0x12); 
        gpio_lcd_truly_emuspi_write_one_data(0x1c); 
        gpio_lcd_truly_emuspi_write_one_data(0x2b); 		
        gpio_lcd_truly_emuspi_write_one_data(0x45); 
        gpio_lcd_truly_emuspi_write_one_data(0x3f); 
        gpio_lcd_truly_emuspi_write_one_data(0x29); 
        gpio_lcd_truly_emuspi_write_one_data(0x17); 
        gpio_lcd_truly_emuspi_write_one_data(0x13); 
        gpio_lcd_truly_emuspi_write_one_data(0x0f); 
        gpio_lcd_truly_emuspi_write_one_data(0x04);        
	SPI_Stop(); 

	SPI_Start(); 
	  gpio_lcd_truly_emuspi_write_one_index(0xC9); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); //enable DGC function
      gpio_lcd_truly_emuspi_write_one_data(0x0a); //SET R-GAMMA
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      

      gpio_lcd_truly_emuspi_write_one_index(0xCA); 
	  gpio_lcd_truly_emuspi_write_one_data(0x01); //SET G-Gamma
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04);


      gpio_lcd_truly_emuspi_write_one_index(0xC8); 	  
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13);
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
	  gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 

	  gpio_lcd_truly_emuspi_write_one_index(0xC9); 	
      gpio_lcd_truly_emuspi_write_one_data(0x01); //SET B-Gamma
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04);

      gpio_lcd_truly_emuspi_write_one_index(0xCa); 		  
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
      gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13);
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x0a); 
      gpio_lcd_truly_emuspi_write_one_data(0x12); 
      gpio_lcd_truly_emuspi_write_one_data(0x1c); 
      gpio_lcd_truly_emuspi_write_one_data(0x2b); 
      gpio_lcd_truly_emuspi_write_one_data(0x45); 
	  gpio_lcd_truly_emuspi_write_one_data(0x3f); 
      gpio_lcd_truly_emuspi_write_one_data(0x29); 
      gpio_lcd_truly_emuspi_write_one_data(0x17); 
      gpio_lcd_truly_emuspi_write_one_data(0x13); 
      gpio_lcd_truly_emuspi_write_one_data(0x0f); 
      gpio_lcd_truly_emuspi_write_one_data(0x04); 

	   gpio_lcd_truly_emuspi_write_one_index(0xd0); 		  
      gpio_lcd_truly_emuspi_write_one_data(0x99); 
      gpio_lcd_truly_emuspi_write_one_data(0x03); 
      gpio_lcd_truly_emuspi_write_one_data(0xce); 
      gpio_lcd_truly_emuspi_write_one_data(0xa6); 
      gpio_lcd_truly_emuspi_write_one_data(0x0c); 
      gpio_lcd_truly_emuspi_write_one_data(0x32); 
      gpio_lcd_truly_emuspi_write_one_data(0x20); 
      gpio_lcd_truly_emuspi_write_one_data(0x10); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x00);
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x00); 
      gpio_lcd_truly_emuspi_write_one_data(0x03); 
      gpio_lcd_truly_emuspi_write_one_data(0x01); 
      gpio_lcd_truly_emuspi_write_one_data(0x00);    

      gpio_lcd_truly_emuspi_write_one_index(0xd1); 		  
      gpio_lcd_truly_emuspi_write_one_data(0x18); 
      gpio_lcd_truly_emuspi_write_one_data(0x0c); 
      gpio_lcd_truly_emuspi_write_one_data(0x23); 
      gpio_lcd_truly_emuspi_write_one_data(0x03); 
      gpio_lcd_truly_emuspi_write_one_data(0x75); 
      gpio_lcd_truly_emuspi_write_one_data(0x02); 
      gpio_lcd_truly_emuspi_write_one_data(0x50); 	  
	  
	SPI_Stop(); 
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xd3); 
	gpio_lcd_truly_emuspi_write_one_data(0x33);   //77:24bit    
	SPI_Stop(); 
	
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xd5);     //INVON
	gpio_lcd_truly_emuspi_write_one_data(0x2a); 
    gpio_lcd_truly_emuspi_write_one_data(0x2a); 	
	SPI_Stop(); 
	
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0xd6);  // SET Display  480x800 
	gpio_lcd_truly_emuspi_write_one_data(0xa8); 
	
	gpio_lcd_truly_emuspi_write_one_index(0xD7);
	gpio_lcd_truly_emuspi_write_one_data(0x01);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0xAA);
	gpio_lcd_truly_emuspi_write_one_data(0xC0);
	gpio_lcd_truly_emuspi_write_one_data(0x2A);
	gpio_lcd_truly_emuspi_write_one_data(0x2C);
	gpio_lcd_truly_emuspi_write_one_data(0x22);
	gpio_lcd_truly_emuspi_write_one_data(0x12);
	gpio_lcd_truly_emuspi_write_one_data(0x71);
	gpio_lcd_truly_emuspi_write_one_data(0x0A);
	gpio_lcd_truly_emuspi_write_one_data(0x12);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0xA0);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x03);

gpio_lcd_truly_emuspi_write_one_index(0xD8);
	gpio_lcd_truly_emuspi_write_one_data(0x44);
	gpio_lcd_truly_emuspi_write_one_data(0x44);
	gpio_lcd_truly_emuspi_write_one_data(0x22);
	gpio_lcd_truly_emuspi_write_one_data(0x44);
	gpio_lcd_truly_emuspi_write_one_data(0x21);
	gpio_lcd_truly_emuspi_write_one_data(0x46);
	gpio_lcd_truly_emuspi_write_one_data(0x42);
	gpio_lcd_truly_emuspi_write_one_data(0x40);

gpio_lcd_truly_emuspi_write_one_index(0xD9);
	gpio_lcd_truly_emuspi_write_one_data(0xCF);
	gpio_lcd_truly_emuspi_write_one_data(0x2D);
	gpio_lcd_truly_emuspi_write_one_data(0x51);

gpio_lcd_truly_emuspi_write_one_index(0xDA);
	gpio_lcd_truly_emuspi_write_one_data(0x01);

gpio_lcd_truly_emuspi_write_one_index(0xDE);
	gpio_lcd_truly_emuspi_write_one_data(0x01);
	gpio_lcd_truly_emuspi_write_one_data(0x41);//VDC

gpio_lcd_truly_emuspi_write_one_index(0xE1);
	gpio_lcd_truly_emuspi_write_one_data(0x03);
	gpio_lcd_truly_emuspi_write_one_data(0x11);
	gpio_lcd_truly_emuspi_write_one_data(0x11);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

gpio_lcd_truly_emuspi_write_one_index(0xE6);
	gpio_lcd_truly_emuspi_write_one_data(0x51);//VDC2

gpio_lcd_truly_emuspi_write_one_index(0xF3);
	gpio_lcd_truly_emuspi_write_one_data(0x06);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x24);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

gpio_lcd_truly_emuspi_write_one_index(0xF8);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

gpio_lcd_truly_emuspi_write_one_index(0xFA);//VDC_SEL Setting
	gpio_lcd_truly_emuspi_write_one_data(0x01);	  //0x03

gpio_lcd_truly_emuspi_write_one_index(0xFB);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

gpio_lcd_truly_emuspi_write_one_index(0xFC);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

gpio_lcd_truly_emuspi_write_one_index(0xFD);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x00);
	gpio_lcd_truly_emuspi_write_one_data(0x70);
	gpio_lcd_truly_emuspi_write_one_data(0x00);

	SPI_Stop(); 
	
	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0x11);  
	SPI_Stop(); 
	msleep(120);

	SPI_Start(); 
	gpio_lcd_truly_emuspi_write_one_index(0x29);
     SPI_Stop(); 
	 msleep(50);

}
*/

static void spi_init(void)
{
	spi_sclk = *(lcdc_tft_pdata->gpio_num+ 5);
	spi_cs   = *(lcdc_tft_pdata->gpio_num + 3);
	spi_sdo  = *(lcdc_tft_pdata->gpio_num + 2);
	spi_sdi  = *(lcdc_tft_pdata->gpio_num + 4);
	printk("spi_sdi = %d\n",spi_sdi);
	// GPIO_CFG(121, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA);
	panel_reset = *(lcdc_tft_pdata->gpio_num + 6);
	printk("spi_init\n!");

	gpio_set_value(spi_sclk, 1);
	gpio_set_value(spi_sdo, 1);
	gpio_set_value(spi_cs, 1);
	msleep(20);

}
void lcdc_truly_sleep(void)
{
	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0x28);  
	SPI_Stop(); 
	msleep(20);
	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0x10);  
	SPI_Stop(); 
	msleep(120);
}
void lcdc_lead_sleep(void)
{
	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0x28);  
	SPI_Stop(); 
	msleep(20);
	SPI_Start(); 
	    gpio_lcd_truly_emuspi_write_one_index(0x10);  
	SPI_Stop(); 
	msleep(120);
}
static int lcdc_panel_off(struct platform_device *pdev)
{
	printk("lcdc_panel_off , g_lcd_panel_type is %d(1 LEAD. 2 TRULY. 3 OLED. )\n",g_lcd_panel_type);
	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_R61408e_WVGA:
			lcdc_truly_sleep();
			break;
		case LCD_PANEL_LEAD_OTM8009_WVGA:
			lcdc_lead_sleep();
			break;
		default:
			break;
	}
	
	gpio_direction_output(panel_reset, 1);
	gpio_direction_output(spi_sclk, 0);
	//gpio_direction_output(spi_sdi, 0);
	gpio_direction_output(spi_sdo, 0);
	gpio_direction_output(spi_cs, 0);
	return 0;
}

/*
static bool lcd_panel_detect_hx8369a(void)
{
	unsigned int id= 0;
	
    SPI_Start(); 
    gpio_lcd_truly_emuspi_write_one_index(0xB9);  // SET password 
    gpio_lcd_truly_emuspi_write_one_data(0xFF);   
    gpio_lcd_truly_emuspi_write_one_data(0x83);   
    gpio_lcd_truly_emuspi_write_one_data(0x69);   
  	SPI_Stop(); 
	
	//Read from RF4H=R69H 
	//mdelay(100); 
	SPI_Start();
    gpio_lcd_truly_emuspi_write_one_index(0xFE);  // read id
	gpio_lcd_truly_emuspi_write_one_data(0xF4);   
  	SPI_Stop(); 

	SPI_Start();
	gpio_lcd_truly_emuspi_read_one_para(0xFF,&id);
	SPI_Stop();		
	printk("lead id is 0x%x\n",id);
	
	if (id == 0x6902)
	{
	    return true;
	}
	
	return false;
}
*/
static bool lcd_panel_detect_r61408e(void)
{
	unsigned int id= 0;
	
	SPI_Start(); 
    gpio_lcd_truly_emuspi_write_one_index(0xb0);   
    gpio_lcd_truly_emuspi_write_one_data(0x04); 
    SPI_Stop(); 
				 
    SPI_Start(); 
    gpio_lcd_truly_emuspi_write_one_index(0xf5);   
    SPI_Stop();  				 

    SPI_Start(); 
    gpio_lcd_lead_emuspi_read_one_index(0xBF,&id); 
	printk("truly id is 0x%x\n",id);
    SPI_Stop(); 
	
	if (id == 0x1221408)
	{
	    return true;
	}
	
	return false;
}

static bool lcd_panel_detect_OTM8018B(void)
{
	unsigned int id= 0, id1 = 0, id2 = 0;	

    SPI_Start();  

	gpio_lcd_lead_emuspi_write_one_index(0xC1A1, 0x08);
	
	gpio_lcd_OTM8018B_emuspi_read_one_index(0xA102,&id1);
	gpio_lcd_OTM8018B_emuspi_read_one_index(0xA103,&id2);
	
    SPI_Stop(); 

	id = (id1 << 8) + id2;
	
	if (id == 0x8009)
	{
	    return true;
	}
	
	return false;
}



static LCD_PANEL_TYPE lcd_panel_detect(void)
{
	LCD_PANEL_TYPE panel_type = LCD_PANEL_TRULY_R61408e_WVGA;
	spi_init();	
	
	if (lcd_panel_detect_r61408e())
	{
		printk("WVGA r61408e_lg detected\n");
		panel_type = LCD_PANEL_TRULY_R61408e_WVGA;
	}
	else if(lcd_panel_detect_OTM8018B())
	{
		printk("WVGA otm8018b_CMI detected\n");
		panel_type = LCD_PANEL_LEAD_OTM8009_WVGA;
	}	
	else
	{
	    printk("default WVGA HX8369\n");
		panel_type = LCD_PANEL_LEAD_HX8369_WVGA;
	}
	
	return panel_type;
}


void lcd_panel_init(void)
{
    pr_err("%s enter!", __func__);
	gpio_direction_output(panel_reset, 1);
	msleep(10);
	gpio_direction_output(panel_reset, 0);
	msleep(50);
	gpio_direction_output(panel_reset, 1);
	msleep(50);	

	switch(g_lcd_panel_type)
	{
		case LCD_PANEL_TRULY_R61408e_WVGA:
			printk("lcdc_R61408E_LG_init()\n");
			lcdc_R61408E_LG_init();
			break;
		case LCD_PANEL_LEAD_OTM8009_WVGA:
			printk("lcdc_OTM8009_init()\n");			
			lcdc_OTM8018B_CMI();
			break;
		default:
			printk("lcdc_hx8369a_init()\n");
		    lcdc_HX8369a_init();
			break;
	}    

}

static struct msm_fb_panel_data lcdc_tft_panel_data = {
       .panel_info = {.bl_max = 32},
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
	    
		g_lcd_panel_type = get_lcd_id_from_tag();
		
		if(g_lcd_panel_type == LCD_PANEL_NONE)
	    {
		   g_lcd_panel_type = lcd_panel_detect();
	    }
		
		printk("lcdc_panel_probe g_lcd_panel_type = %d!\n",g_lcd_panel_type);

		if(g_lcd_panel_type==LCD_PANEL_LEAD_OTM8009_WVGA)//OTM8018B
		{
		    printk("g_lcd_panel_type==LCD_PANEL_otm8009_WVGA!!!!\n");
            
			//gpio_lcd_lead_emuspi_write_one_index(0xC1A1, 0x08);//lxt add for change to use external clock
			
			pinfo = &lcdc_tft_panel_data.panel_info;
				pinfo->lcdc.h_back_porch = 40;
				pinfo->lcdc.h_front_porch =30;
				pinfo->lcdc.h_pulse_width = 8;
		    	pinfo->lcdc.v_back_porch = 40;
				pinfo->lcdc.v_front_porch =22;
				pinfo->lcdc.v_pulse_width = 2;
				pinfo->lcdc.border_clr = 0; /* blk */
				pinfo->lcdc.underflow_clr = 0xffff; /* blue */
				pinfo->lcdc.hsync_skew = 0;
		}
		else if(g_lcd_panel_type == LCD_PANEL_LEAD_HX8369_WVGA)
		{
			//hx8369a
		    pinfo = &lcdc_tft_panel_data.panel_info;
		    pinfo->lcdc.h_back_porch = 5;
			pinfo->lcdc.h_front_porch = 5;
			pinfo->lcdc.h_pulse_width = 5;
			pinfo->lcdc.v_back_porch = 8;
			pinfo->lcdc.v_front_porch =8;
			pinfo->lcdc.v_pulse_width = 4;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
		}
		else
		{
			pinfo = &lcdc_tft_panel_data.panel_info;//r61408e
			pinfo->lcdc.h_back_porch = 10;
			pinfo->lcdc.h_front_porch = 120;
			pinfo->lcdc.h_pulse_width = 8;
			pinfo->lcdc.v_back_porch = 12;
			pinfo->lcdc.v_front_porch =12;
			pinfo->lcdc.v_pulse_width = 2;
			pinfo->lcdc.border_clr = 0;	/* blk */
			pinfo->lcdc.underflow_clr = 0xffff;	/* blue */
			pinfo->lcdc.hsync_skew = 0;
		}

		switch(g_lcd_panel_type)
		{
			case LCD_PANEL_TRULY_R61408e_WVGA:
				LcdPanleID=(u32)115;   		
				break;
			case LCD_PANEL_LEAD_OTM8009_WVGA:				
				LcdPanleID=(u32)114;   		
				break;
			case LCD_PANEL_LEAD_HX8369_WVGA:
				LcdPanleID=(u32)116;   		
				break;
			default:
				break;
		}	
			pinfo->xres = 480;
		    pinfo->yres = 800;		
		    pinfo->type = LCDC_PANEL;
		    pinfo->pdest = DISPLAY_1;
		    pinfo->wait_cycle = 0;
		    pinfo->bpp = 24;
		    pinfo->fb_num = 2;
		
        pinfo->clk_rate = 24576000;//28000000
		ret = platform_device_register(&this_device);
		return 0;
	 	
	}

	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcdc_panel_probe,
	.driver = {
		.name   = "lcdc_panel_qhd",//"lcdc_panel_wvga",
	},
};


static int  lcdc_oled_panel_init(void)
{
	int ret;
	
	printk("WVGA lcdc_oled_panel_init\n");

	ret = platform_driver_register(&this_driver);

	return ret;
}

module_init(lcdc_oled_panel_init);

