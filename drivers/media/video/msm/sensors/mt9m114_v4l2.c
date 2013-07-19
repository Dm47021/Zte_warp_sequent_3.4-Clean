/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#define SENSOR_NAME "mt9m114"

DEFINE_MUTEX(mt9m114_mut);
static struct msm_sensor_ctrl_t mt9m114_s_ctrl;

static int8_t  zte_force_write=0;
static int  zte_iso=0xff;
static int  zte_brightness=0xff;
static int  zte_awb=0xff;
static int  zte_antibanding=0xff;

enum mt9m114_width_t {
    WORD_LEN,
    BYTE_LEN
};

struct mt9m114_i2c_reg_conf {
    uint16_t  waddr;
    uint16_t wdata;
    enum mt9m114_width_t width;
    unsigned short mdelay_time;
};
//static int is_init=0;
static int csi_config=0;


static int32_t mt9m114_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl, struct mt9m114_i2c_reg_conf const *reg_conf_tbl,int len)
{
	uint32_t i;
	int32_t rc = 0;
	
	for (i = 0; i < len; i++)
	{
		if(  reg_conf_tbl[i].width == BYTE_LEN)
		    rc=msm_camera_i2c_write(s_ctrl->sensor_i2c_client, reg_conf_tbl[i].waddr, reg_conf_tbl[i].wdata, MSM_CAMERA_I2C_BYTE_DATA);
		else 
		    rc=msm_camera_i2c_write(s_ctrl->sensor_i2c_client,reg_conf_tbl[i].waddr, reg_conf_tbl[i].wdata, MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0)
		{
		    break;
		}

		if (reg_conf_tbl[i].mdelay_time != 0)
		{
		    msleep(reg_conf_tbl[i].mdelay_time);
		}
     }
    return 0;
}


static struct msm_camera_i2c_reg_conf mt9m114_recommend_settings[] = {
};


/* Preview and Snapshot Setup */
static struct mt9m114_i2c_reg_conf const prev_snap_tbl[] = {
{0x3C40, 0x003C,WORD_LEN,0},////MIPI standby off,stream on,0x783C,120828
{0x301A, 0x0230, WORD_LEN, 0},// 	// RESET_REGISTER,0x0234,120828
//TIMING
//{0xC984, 0x8041,WORD_LEN,0},//8001bu 8041 lianxu,120828
{0x098E, 0x1000, WORD_LEN, 0},

{0xC97E, 0x01,  BYTE_LEN, 0},
{0xC980, 0x0120, WORD_LEN, 0}, 	// CAM_SYSCTL_PLL_DIVIDER_M_N
{0xC982, 0x0700, WORD_LEN, 10}, 	// CAM_SYSCTL_PLL_DIVIDER_P
{0x098E, 0x1000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
{0xC800, 0x0004,WORD_LEN,0},		//cam_sensor_cfg_y_addr_start = 4
{0xC802, 0x0004,WORD_LEN,0},		//cam_sensor_cfg_x_addr_start = 4
{0xC804, 0x03CB,WORD_LEN,0},		//cam_sensor_cfg_y_addr_end = 971
{0xC806, 0x050B,WORD_LEN,0},		//cam_sensor_cfg_x_addr_end = 1291
{0xC808, 0x02DC,WORD_LEN,0},		//cam_sensor_cfg_pixclk = 48000000
{0xC80A, 0x6c00,WORD_LEN,0},   
{0xC80C, 0x0001,WORD_LEN,0},		//cam_sensor_cfg_row_speed = 1                                   
{0xC80E, 0x00DB,WORD_LEN,0},		//cam_sensor_cfg_fine_integ_time_min = 219
{0xC810, 0x05C8,WORD_LEN,0},		//cam_sensor_cfg_fine_integ_time_max = 1480
{0xC812, 0x03EE,WORD_LEN,0},		//cam_sensor_cfg_frame_length_lines = 1006
{0xC814, 0x064B,WORD_LEN,0},		//cam_sensor_cfg_line_length_pck = 1611
{0xC816, 0x0060,WORD_LEN,0},		//cam_sensor_cfg_fine_correction = 96
{0xC818, 0x03C3,WORD_LEN,0},		//cam_sensor_cfg_cpipe_last_row = 963
{0xC826, 0x0020,WORD_LEN,0},		//cam_sensor_cfg_reg_0_data = 32
{0xC834, 0x0000,WORD_LEN,0},		//yanwei 0-1mirro 0-2flip 0-3flip and 90-1 mirror  90-3 mirror
{0xC854, 0x0000,WORD_LEN,0},		//cam_crop_window_xoffset = 0
{0xC856, 0x0000,WORD_LEN,0},		//cam_crop_window_yoffset = 0
{0xC858, 0x0500,WORD_LEN,0},		//cam_crop_window_width = 1280
{0xC85A, 0x03C0,WORD_LEN,0},		//cam_crop_window_height = 960
{0xC85C, 0x03	,	BYTE_LEN,0},//cam_crop_cropmode = 3
{0xC868, 0x0500,WORD_LEN,0},		//cam_output_width = 1280
{0xC86A, 0x03C0,WORD_LEN,0},		//cam_output_height = 960
{0xC878, 0x00	,	BYTE_LEN,0},//cam_aet_aemode = 0
{0xC88C, 0x1D9E,WORD_LEN,0},		//cam_aet_max_frame_rate = 7582
{0xC88E, 0x1800,WORD_LEN,0},		//cam_aet_min_frame_rate = 6144
{0xC914, 0x0000,WORD_LEN,0},		//cam_stat_awb_clip_window_xstart = 0
{0xC916, 0x0000,WORD_LEN,0},		//cam_stat_awb_clip_window_ystart = 0
{0xC918, 0x04FF,WORD_LEN,0},		//cam_stat_awb_clip_window_xend = 1279
{0xC91A, 0x03BF,WORD_LEN,0},		//cam_stat_awb_clip_window_yend = 959
{0xC91C, 0x0000,WORD_LEN,0},		//cam_stat_ae_initial_window_xstart = 0
{0xC91E, 0x0000,WORD_LEN,0},		//cam_stat_ae_initial_window_ystart = 0
{0xC920, 0x00FF,WORD_LEN,0},		//cam_stat_ae_initial_window_xend = 255
{0xC922, 0x00BF,WORD_LEN,0},		//cam_stat_ae_initial_window_yend = 191
{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
{0x0080, 0x8002, WORD_LEN, 30}, 	// COMMAND_REGISTER

{0x098E, 0x1000, WORD_LEN, 0},
{0xC88C, 0x1D9E, WORD_LEN, 0}, 	// CAM_AET_MAX_FRAME_RATE
{0xC88E, 0x1D9E, WORD_LEN, 0}, 	// CAM_AET_MIN_FRAME_RATE 0x0F00-15fps 0x1e00 24fps

 
//{0xE801, 0x00, BYTE_LEN, 0},// 	// AUTO_BINNING_MODE



//[Step3-Recommended]
{0x316A, 0x8270, WORD_LEN, 0}, 	// DAC_TXLO_ROW
{0x316C, 0x8270, WORD_LEN, 0}, 	// DAC_TXLO
{0x3ED0, 0x2305, WORD_LEN, 0}, 	// DAC_LD_4_5
{0x3ED2, 0x77CF, WORD_LEN, 0}, 	// DAC_LD_6_7
{0x316E, 0x8202, WORD_LEN, 0}, 	// DAC_ECL
{0x3180, 0x87FF, WORD_LEN, 0}, 	// DELTA_DK_CONTROL
{0x30D4, 0x6080, WORD_LEN, 0}, 	// COLUMN_CORRECTION
{0xA802, 0x0008, WORD_LEN, 0}, 	// AE_TRACK_MODE
{0x3E14, 0xFF39, WORD_LEN, 0}, 	// SAMP_COL_PUP2
{0x31E0, 0x0001, WORD_LEN, 0}, 	// lowlux,120828


//[patch 1204]for 720P
{0x0982, 0x0001, WORD_LEN, 0}, 	// ACCESS_CTL_STAT
{0x098A, 0x60BC, WORD_LEN, 0}, 	// PHYSICAL_ADDRESS_ACCESS
{0xE0BC, 0xC0F1, WORD_LEN, 0},
{0xE0BE, 0x082A, WORD_LEN, 0},
{0xE0C0, 0x05A0, WORD_LEN, 0},
{0xE0C2, 0xD800, WORD_LEN, 0},
{0xE0C4, 0x71CF, WORD_LEN, 0},
{0xE0C6, 0xFFFF, WORD_LEN, 0},
{0xE0C8, 0xC344, WORD_LEN, 0},
{0xE0CA, 0x77CF, WORD_LEN, 0},
{0xE0CC, 0xFFFF, WORD_LEN, 0},
{0xE0CE, 0xC7C0, WORD_LEN, 0},
{0xE0D0, 0xB104, WORD_LEN, 0},
{0xE0D2, 0x8F1F, WORD_LEN, 0},
{0xE0D4, 0x75CF, WORD_LEN, 0},
{0xE0D6, 0xFFFF, WORD_LEN, 0},
{0xE0D8, 0xC84C, WORD_LEN, 0},
{0xE0DA, 0x0811, WORD_LEN, 0},
{0xE0DC, 0x005E, WORD_LEN, 0},
{0xE0DE, 0x70CF, WORD_LEN, 0},
{0xE0E0, 0x0000, WORD_LEN, 0},
{0xE0E2, 0x500E, WORD_LEN, 0},
{0xE0E4, 0x7840, WORD_LEN, 0},
{0xE0E6, 0xF019, WORD_LEN, 0},
{0xE0E8, 0x0CC6, WORD_LEN, 0},
{0xE0EA, 0x0340, WORD_LEN, 0},
{0xE0EC, 0x0E26, WORD_LEN, 0},
{0xE0EE, 0x0340, WORD_LEN, 0},
{0xE0F0, 0x95C2, WORD_LEN, 0},
{0xE0F2, 0x0E21, WORD_LEN, 0},
{0xE0F4, 0x101E, WORD_LEN, 0},
{0xE0F6, 0x0E0D, WORD_LEN, 0},
{0xE0F8, 0x119E, WORD_LEN, 0},
{0xE0FA, 0x0D56, WORD_LEN, 0},
{0xE0FC, 0x0340, WORD_LEN, 0},
{0xE0FE, 0xF008, WORD_LEN, 0},
{0xE100, 0x2650, WORD_LEN, 0},
{0xE102, 0x1040, WORD_LEN, 0},
{0xE104, 0x0AA2, WORD_LEN, 0},
{0xE106, 0x0360, WORD_LEN, 0},
{0xE108, 0xB502, WORD_LEN, 0},
{0xE10A, 0xB5C2, WORD_LEN, 0},
{0xE10C, 0x0B22, WORD_LEN, 0},
{0xE10E, 0x0400, WORD_LEN, 0},
{0xE110, 0x0CCE, WORD_LEN, 0},
{0xE112, 0x0320, WORD_LEN, 0},
{0xE114, 0xD800, WORD_LEN, 0},
{0xE116, 0x70CF, WORD_LEN, 0},
{0xE118, 0xFFFF, WORD_LEN, 0},
{0xE11A, 0xC5D4, WORD_LEN, 0},
{0xE11C, 0x902C, WORD_LEN, 0},
{0xE11E, 0x72CF, WORD_LEN, 0},
{0xE120, 0xFFFF, WORD_LEN, 0},
{0xE122, 0xE218, WORD_LEN, 0},
{0xE124, 0x9009, WORD_LEN, 0},
{0xE126, 0xE105, WORD_LEN, 0},
{0xE128, 0x73CF, WORD_LEN, 0},
{0xE12A, 0xFF00, WORD_LEN, 0},
{0xE12C, 0x2FD0, WORD_LEN, 0},
{0xE12E, 0x7822, WORD_LEN, 0},
{0xE130, 0x7910, WORD_LEN, 0},
{0xE132, 0xB202, WORD_LEN, 0},
{0xE134, 0x1382, WORD_LEN, 0},
{0xE136, 0x0700, WORD_LEN, 0},
{0xE138, 0x0815, WORD_LEN, 0},
{0xE13A, 0x03DE, WORD_LEN, 0},
{0xE13C, 0x1387, WORD_LEN, 0},
{0xE13E, 0x0700, WORD_LEN, 0},
{0xE140, 0x2102, WORD_LEN, 0},
{0xE142, 0x000A, WORD_LEN, 0},
{0xE144, 0x212F, WORD_LEN, 0},
{0xE146, 0x0288, WORD_LEN, 0},
{0xE148, 0x1A04, WORD_LEN, 0},
{0xE14A, 0x0284, WORD_LEN, 0},
{0xE14C, 0x13B9, WORD_LEN, 0},
{0xE14E, 0x0700, WORD_LEN, 0},
{0xE150, 0xB8C1, WORD_LEN, 0},
{0xE152, 0x0815, WORD_LEN, 0},
{0xE154, 0x0052, WORD_LEN, 0},
{0xE156, 0xDB00, WORD_LEN, 0},
{0xE158, 0x230F, WORD_LEN, 0},
{0xE15A, 0x0003, WORD_LEN, 0},
{0xE15C, 0x2102, WORD_LEN, 0},
{0xE15E, 0x00C0, WORD_LEN, 0},
{0xE160, 0x7910, WORD_LEN, 0},
{0xE162, 0xB202, WORD_LEN, 0},
{0xE164, 0x9507, WORD_LEN, 0},
{0xE166, 0x7822, WORD_LEN, 0},
{0xE168, 0xE080, WORD_LEN, 0},
{0xE16A, 0xD900, WORD_LEN, 0},
{0xE16C, 0x20CA, WORD_LEN, 0},
{0xE16E, 0x004B, WORD_LEN, 0},
{0xE170, 0xB805, WORD_LEN, 0},
{0xE172, 0x9533, WORD_LEN, 0},
{0xE174, 0x7815, WORD_LEN, 0},
{0xE176, 0x6038, WORD_LEN, 0},
{0xE178, 0x0FB2, WORD_LEN, 0},
{0xE17A, 0x0560, WORD_LEN, 0},
{0xE17C, 0xB861, WORD_LEN, 0},
{0xE17E, 0xB711, WORD_LEN, 0},
{0xE180, 0x0775, WORD_LEN, 0},
{0xE182, 0x0540, WORD_LEN, 0},
{0xE184, 0xD900, WORD_LEN, 0},
{0xE186, 0xF00A, WORD_LEN, 0},
{0xE188, 0x70CF, WORD_LEN, 0},
{0xE18A, 0xFFFF, WORD_LEN, 0},
{0xE18C, 0xE210, WORD_LEN, 0},
{0xE18E, 0x7835, WORD_LEN, 0},
{0xE190, 0x8041, WORD_LEN, 0},
{0xE192, 0x8000, WORD_LEN, 0},
{0xE194, 0xE102, WORD_LEN, 0},
{0xE196, 0xA040, WORD_LEN, 0},
{0xE198, 0x09F1, WORD_LEN, 0},
{0xE19A, 0x8094, WORD_LEN, 0},
{0xE19C, 0x7FE0, WORD_LEN, 0},
{0xE19E, 0xD800, WORD_LEN, 0},
{0xE1A0, 0xC0F1, WORD_LEN, 0},
{0xE1A2, 0xC5E1, WORD_LEN, 0},
{0xE1A4, 0x71CF, WORD_LEN, 0},
{0xE1A6, 0x0000, WORD_LEN, 0},
{0xE1A8, 0x45E6, WORD_LEN, 0},
{0xE1AA, 0x7960, WORD_LEN, 0},
{0xE1AC, 0x7508, WORD_LEN, 0},
{0xE1AE, 0x70CF, WORD_LEN, 0},
{0xE1B0, 0xFFFF, WORD_LEN, 0},
{0xE1B2, 0xC84C, WORD_LEN, 0},
{0xE1B4, 0x9002, WORD_LEN, 0},
{0xE1B6, 0x083D, WORD_LEN, 0},
{0xE1B8, 0x021E, WORD_LEN, 0},
{0xE1BA, 0x0D39, WORD_LEN, 0},
{0xE1BC, 0x10D1, WORD_LEN, 0},
{0xE1BE, 0x70CF, WORD_LEN, 0},
{0xE1C0, 0xFF00, WORD_LEN, 0},
{0xE1C2, 0x3354, WORD_LEN, 0},
{0xE1C4, 0x9055, WORD_LEN, 0},
{0xE1C6, 0x71CF, WORD_LEN, 0},
{0xE1C8, 0xFFFF, WORD_LEN, 0},
{0xE1CA, 0xC5D4, WORD_LEN, 0},
{0xE1CC, 0x116C, WORD_LEN, 0},
{0xE1CE, 0x0103, WORD_LEN, 0},
{0xE1D0, 0x1170, WORD_LEN, 0},
{0xE1D2, 0x00C1, WORD_LEN, 0},
{0xE1D4, 0xE381, WORD_LEN, 0},
{0xE1D6, 0x22C6, WORD_LEN, 0},
{0xE1D8, 0x0F81, WORD_LEN, 0},
{0xE1DA, 0x0000, WORD_LEN, 0},
{0xE1DC, 0x00FF, WORD_LEN, 0},
{0xE1DE, 0x22C4, WORD_LEN, 0},
{0xE1E0, 0x0F82, WORD_LEN, 0},
{0xE1E2, 0xFFFF, WORD_LEN, 0},
{0xE1E4, 0x00FF, WORD_LEN, 0},
{0xE1E6, 0x29C0, WORD_LEN, 0},
{0xE1E8, 0x0222, WORD_LEN, 0},
{0xE1EA, 0x7945, WORD_LEN, 0},
{0xE1EC, 0x7930, WORD_LEN, 0},
{0xE1EE, 0xB035, WORD_LEN, 0},
{0xE1F0, 0x0715, WORD_LEN, 0},
{0xE1F2, 0x0540, WORD_LEN, 0},
{0xE1F4, 0xD900, WORD_LEN, 0},
{0xE1F6, 0xF00A, WORD_LEN, 0},
{0xE1F8, 0x70CF, WORD_LEN, 0},
{0xE1FA, 0xFFFF, WORD_LEN, 0},
{0xE1FC, 0xE224, WORD_LEN, 0},
{0xE1FE, 0x7835, WORD_LEN, 0},
{0xE200, 0x8041, WORD_LEN, 0},
{0xE202, 0x8000, WORD_LEN, 0},
{0xE204, 0xE102, WORD_LEN, 0},
{0xE206, 0xA040, WORD_LEN, 0},
{0xE208, 0x09F1, WORD_LEN, 0},
{0xE20A, 0x8094, WORD_LEN, 0},
{0xE20C, 0x7FE0, WORD_LEN, 0},
{0xE20E, 0xD800, WORD_LEN, 0},
{0xE210, 0xFFFF, WORD_LEN, 0},
{0xE212, 0xCB40, WORD_LEN, 0},
{0xE214, 0xFFFF, WORD_LEN, 0},
{0xE216, 0xE0BC, WORD_LEN, 0},
{0xE218, 0x0000, WORD_LEN, 0},
{0xE21A, 0x0000, WORD_LEN, 0},
{0xE21C, 0x0000, WORD_LEN, 0},
{0xE21E, 0x0000, WORD_LEN, 0},
{0xE220, 0x0000, WORD_LEN, 0},
{0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
{0xE000, 0x1184, WORD_LEN, 0}, 	// PATCHLDR_LOADER_ADDRESS
{0xE002, 0x1204, WORD_LEN, 0}, 	// PATCHLDR_PATCH_ID
{0xE004, 0x4103, WORD_LEN, 0},//0202 	// PATCHLDR_FIRMWARE_ID
{0xE006, 0x0202, WORD_LEN, 0},//0202 	// PATCHLDR_FIRMWARE_ID

{0x0080, 0xFFF0, WORD_LEN, 50}, 	// COMMAND_REGISTER
//  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00
//  POLL  COMMAND_REGISTER::HOST_COMMAND_0 =>  0x00

{0x0080, 0xFFF1, WORD_LEN, 50}, 	// COMMAND_REGISTER

// AWB Start point
{0x098E,0x2c12, WORD_LEN, 0},
{0xac12,0x008f, WORD_LEN, 0},
{0xac14,0x0105, WORD_LEN, 0},

//  Lens register settings for A-1040SOC (MT9M114) REV2
{0x3640, 0x00B0, WORD_LEN, 0},//  P_G1_P0Q0
{0x3642, 0x8B8A, WORD_LEN, 0},//  P_G1_P0Q1
{0x3644, 0x39B0, WORD_LEN, 0},//  P_G1_P0Q2
{0x3646, 0xF508, WORD_LEN, 0},//  P_G1_P0Q3
{0x3648, 0x9FED, WORD_LEN, 0},//  P_G1_P0Q4
{0x364A, 0x01B0, WORD_LEN, 0},//  P_R_P0Q0
{0x364C, 0xE129, WORD_LEN, 0},//  P_R_P0Q1
{0x364E, 0x6AB0, WORD_LEN, 0},//  P_R_P0Q2
{0x3650, 0xA9CC, WORD_LEN, 0},//  P_R_P0Q3
{0x3652, 0x990F, WORD_LEN, 0},//  P_R_P0Q4
{0x3654, 0x0150, WORD_LEN, 0},//  P_B_P0Q0
{0x3656, 0x418A, WORD_LEN, 0},//  P_B_P0Q1
{0x3658, 0x2410, WORD_LEN, 0},//  P_B_P0Q2
{0x365A, 0xAFAB, WORD_LEN, 0},//  P_B_P0Q3
{0x365C, 0xE16E, WORD_LEN, 0},//  P_B_P0Q4
{0x365E, 0x00B0, WORD_LEN, 0},//  P_G2_P0Q0
{0x3660, 0x8EAA, WORD_LEN, 0},//  P_G2_P0Q1
{0x3662, 0x3990, WORD_LEN, 0},//  P_G2_P0Q2
{0x3664, 0x9BE8, WORD_LEN, 0},//  P_G2_P0Q3
{0x3666, 0xA08D, WORD_LEN, 0},//  P_G2_P0Q4
{0x3680, 0x700A, WORD_LEN, 0},//  P_G1_P1Q0
{0x3682, 0x3DEB, WORD_LEN, 0},//  P_G1_P1Q1
{0x3684, 0x024C, WORD_LEN, 0},//  P_G1_P1Q2
{0x3686, 0x1FEC, WORD_LEN, 0},//  P_G1_P1Q3
{0x3688, 0x85AD, WORD_LEN, 0},//  P_G1_P1Q4
{0x368A, 0xC909, WORD_LEN, 0},//  P_R_P1Q0
{0x368C, 0x08EC, WORD_LEN, 0},//  P_R_P1Q1
{0x368E, 0x770B, WORD_LEN, 0},//  P_R_P1Q2
{0x3690, 0x598C, WORD_LEN, 0},//  P_R_P1Q3
{0x3692, 0x6FAE, WORD_LEN, 0},//  P_R_P1Q4
{0x3694, 0x716A, WORD_LEN, 0},//  P_B_P1Q0
{0x3696, 0xE389, WORD_LEN, 0},//  P_B_P1Q1
{0x3698, 0x094C, WORD_LEN, 0},//  P_B_P1Q2
{0x369A, 0xF12B, WORD_LEN, 0},//  P_B_P1Q3
{0x369C, 0xB28A, WORD_LEN, 0},//  P_B_P1Q4
{0x369E, 0x6ECA, WORD_LEN, 0},//  P_G2_P1Q0
{0x36A0, 0x43CB, WORD_LEN, 0},//  P_G2_P1Q1
{0x36A2, 0x048C, WORD_LEN, 0},//  P_G2_P1Q2
{0x36A4, 0x1BEC, WORD_LEN, 0},//  P_G2_P1Q3
{0x36A6, 0x8A2D, WORD_LEN, 0},//  P_G2_P1Q4
{0x36C0, 0x0711, WORD_LEN, 0},//  P_G1_P2Q0
{0x36C2, 0xAC4E, WORD_LEN, 0},//  P_G1_P2Q1
{0x36C4, 0xD831, WORD_LEN, 0},//  P_G1_P2Q2
{0x36C6, 0x4570, WORD_LEN, 0},//  P_G1_P2Q3
{0x36C8, 0x3312, WORD_LEN, 0},//  P_G1_P2Q4
{0x36CA, 0x0491, WORD_LEN, 0},//  P_R_P2Q0
{0x36CC, 0xC6EE, WORD_LEN, 0},//  P_R_P2Q1
{0x36CE, 0xC0F1, WORD_LEN, 0},//  P_R_P2Q2
{0x36D0, 0x2D50, WORD_LEN, 0},//  P_R_P2Q3
{0x36D2, 0x33D2, WORD_LEN, 0},//  P_R_P2Q4
{0x36D4, 0x3530, WORD_LEN, 0},//  P_B_P2Q0
{0x36D6, 0xAF4D, WORD_LEN, 0},//  P_B_P2Q1
{0x36D8, 0xC5B0, WORD_LEN, 0},//  P_B_P2Q2
{0x36DA, 0x25AF, WORD_LEN, 0},//  P_B_P2Q3
{0x36DC, 0x13F1, WORD_LEN, 0},//  P_B_P2Q4
{0x36DE, 0x06D1, WORD_LEN, 0},//  P_G2_P2Q0
{0x36E0, 0xB10E, WORD_LEN, 0},//  P_G2_P2Q1
{0x36E2, 0xD351, WORD_LEN, 0},//  P_G2_P2Q2
{0x36E4, 0x4930, WORD_LEN, 0},//  P_G2_P2Q3
{0x36E6, 0x2CF2, WORD_LEN, 0},//  P_G2_P2Q4
{0x3700, 0x4F4D, WORD_LEN, 0},//  P_G1_P3Q0
{0x3702, 0x2D29, WORD_LEN, 0},//  P_G1_P3Q1
{0x3704, 0x85ED, WORD_LEN, 0},//  P_G1_P3Q2
{0x3706, 0xC4EA, WORD_LEN, 0},//  P_G1_P3Q3
{0x3708, 0x41D1, WORD_LEN, 0},//  P_G1_P3Q4
{0x370A, 0x262C, WORD_LEN, 0},//  P_R_P3Q0
{0x370C, 0xADCA, WORD_LEN, 0},//  P_R_P3Q1
{0x370E, 0x2B10, WORD_LEN, 0},//  P_R_P3Q2
{0x3710, 0x9C8E, WORD_LEN, 0},//  P_R_P3Q3
{0x3712, 0xBF0F, WORD_LEN, 0},//  P_R_P3Q4
{0x3714, 0x0C0D, WORD_LEN, 0},//  P_B_P3Q0
{0x3716, 0x226B, WORD_LEN, 0},//  P_B_P3Q1
{0x3718, 0x078E, WORD_LEN, 0},//  P_B_P3Q2
{0x371A, 0x5A8E, WORD_LEN, 0},//  P_B_P3Q3
{0x371C, 0xB42D, WORD_LEN, 0},//  P_B_P3Q4
{0x371E, 0x540D, WORD_LEN, 0},//  P_G2_P3Q0
{0x3720, 0x74C8, WORD_LEN, 0},//  P_G2_P3Q1
{0x3722, 0xAF0D, WORD_LEN, 0},//  P_G2_P3Q2
{0x3724, 0xD5EA, WORD_LEN, 0},//  P_G2_P3Q3
{0x3726, 0x49D1, WORD_LEN, 0},//  P_G2_P3Q4
{0x3740, 0x8F11, WORD_LEN, 0},//  P_G1_P4Q0
{0x3742, 0x2D30, WORD_LEN, 0},//  P_G1_P4Q1
{0x3744, 0xA1ED, WORD_LEN, 0},//  P_G1_P4Q2
{0x3746, 0xB392, WORD_LEN, 0},//  P_G1_P4Q3
{0x3748, 0x42D5, WORD_LEN, 0},//  P_G1_P4Q4
{0x374A, 0xD6CF, WORD_LEN, 0},//  P_R_P4Q0
{0x374C, 0x4930, WORD_LEN, 0},//  P_R_P4Q1
{0x374E, 0x8392, WORD_LEN, 0},//  P_R_P4Q2
{0x3750, 0xA012, WORD_LEN, 0},//  P_R_P4Q3
{0x3752, 0x7475, WORD_LEN, 0},//  P_R_P4Q4
{0x3754, 0xD72D, WORD_LEN, 0},//  P_B_P4Q0
{0x3756, 0x63AF, WORD_LEN, 0},//  P_B_P4Q1
{0x3758, 0xA7F2, WORD_LEN, 0},//  P_B_P4Q2
{0x375A, 0xAB91, WORD_LEN, 0},//  P_B_P4Q3
{0x375C, 0x4795, WORD_LEN, 0},//  P_B_P4Q4
{0x375E, 0x8D11, WORD_LEN, 0},//  P_G2_P4Q0
{0x3760, 0x3210, WORD_LEN, 0},//  P_G2_P4Q1
{0x3762, 0xB44F, WORD_LEN, 0},//  P_G2_P4Q2
{0x3764, 0xB6B2, WORD_LEN, 0},//  P_G2_P4Q3
{0x3766, 0x48B5, WORD_LEN, 0},//  P_G2_P4Q4
{0x3784, 0x0280, WORD_LEN, 0},//
{0x3782, 0x01E0, WORD_LEN, 0},//
{0x37C0, 0xF8A2, WORD_LEN, 0},//  P_GR_Q5
{0x37C2, 0xB8EA, WORD_LEN, 0},//  P_RD_Q5
{0x37C4, 0xBB69, WORD_LEN, 0},//  P_BL_Q5
{0x37C6, 0x2E23, WORD_LEN, 0},//  P_GB_Q5
{0x098E, 0x0000, WORD_LEN, 0},//
{0xC960, 0x0AA0, WORD_LEN, 0},//
{0xC962, 0x7380, WORD_LEN, 0},//  CAM_PGA_L_CONFIG_GREEN_RED_Q14
{0xC964, 0x5748, WORD_LEN, 0},//  CAM_PGA_L_CONFIG_RED_Q14
{0xC966, 0x73AC, WORD_LEN, 0},//  CAM_PGA_L_CONFIG_GREEN_BLUE_Q14
{0xC968, 0x6DC4, WORD_LEN, 0},//  CAM_PGA_L_CONFIG_BLUE_Q14
{0xC96A, 0x0FF0, WORD_LEN, 0},//  CAM_PGA_M_CONFIG_COLOUR_TEMP
{0xC96C, 0x7FFE, WORD_LEN, 0},//  CAM_PGA_M_CONFIG_GREEN_RED_Q14
{0xC96E, 0x7E8E, WORD_LEN, 0},//  CAM_PGA_M_CONFIG_RED_Q14
{0xC970, 0x8003, WORD_LEN, 0},//  CAM_PGA_M_CONFIG_GREEN_BLUE_Q14
{0xC972, 0x7F45, WORD_LEN, 0},//  CAM_PGA_M_CONFIG_BLUE_Q14
{0xC974, 0x1964, WORD_LEN, 0},//  CAM_PGA_R_CONFIG_COLOUR_TEMP
{0xC976, 0x7E8D, WORD_LEN, 0},//  CAM_PGA_R_CONFIG_GREEN_RED_Q14
{0xC978, 0x7D72, WORD_LEN, 0},//  CAM_PGA_R_CONFIG_RED_Q14
{0xC97A, 0x7E89, WORD_LEN, 0},//  CAM_PGA_R_CONFIG_GREEN_BLUE_Q14
{0xC97C, 0x7AF0, WORD_LEN, 0},//  CAM_PGA_R_CONFIG_BLUE_Q14
{0xC95E, 0x0003, WORD_LEN, 0},//  CAM_PGA_PGA_CONTROL


//[Step5-AWB_CCM]
{0x098E, 0x4892, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_AWB_CCM_L_0]
{0xC892, 0x0267, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_0
{0xC894, 0xFF1A, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_1
{0xC896, 0xFFB3, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_2
{0xC898, 0xFF80, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_3
{0xC89A, 0x0166, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_4
{0xC89C, 0x0003, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_5
{0xC89E, 0xFF9A, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_6
{0xC8A0, 0xFEB4, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_7
{0xC8A2, 0x024D, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_8
{0xC8A4, 0x01BF, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_0
{0xC8A6, 0xFF01, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_1
{0xC8A8, 0xFFF3, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_2
{0xC8AA, 0xFF75, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_3
{0xC8AC, 0x0198, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_4
{0xC8AE, 0xFFFD, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_5
{0xC8B0, 0xFF9A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_6
{0xC8B2, 0xFEE7, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_7
{0xC8B4, 0x02A8, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_8
{0xC8B6, 0x01D9, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_0
{0xC8B8, 0xFF26, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_1
{0xC8BA, 0xFFF3, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_2
{0xC8BC, 0xFFB3, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_3
{0xC8BE, 0x0132, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_4
{0xC8C0, 0xFFE8, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_5
{0xC8C2, 0xFFDA, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_6
{0xC8C4, 0xFECD, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_7
{0xC8C6, 0x02C2, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_8
{0xC8C8, 0x0075, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_RG_GAIN
{0xC8CA, 0x011C, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_BG_GAIN
{0xC8CC, 0x009A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_RG_GAIN
{0xC8CE, 0x0105, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_BG_GAIN
{0xC8D0, 0x00A4, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_RG_GAIN
{0xC8D2, 0x00AC, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_BG_GAIN
{0xC8D4, 0x0A8C, WORD_LEN, 0}, 	// CAM_AWB_CCM_L_CTEMP
{0xC8D6, 0x0F0A, WORD_LEN, 0}, 	// CAM_AWB_CCM_M_CTEMP
{0xC8D8, 0x1964, WORD_LEN, 0}, 	// CAM_AWB_CCM_R_CTEMP
{0xC914, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XSTART
{0xC916, 0x0000, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YSTART
{0xC918, 0x04FF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_XEND
{0xC91A, 0x02CF, WORD_LEN, 0}, 	// CAM_STAT_AWB_CLIP_WINDOW_YEND
{0xC904, 0x0033, WORD_LEN, 0}, 	// CAM_AWB_AWB_XSHIFT_PRE_ADJ
{0xC906, 0x0040, WORD_LEN, 0}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ
{0xC8F2, 0x03, BYTE_LEN, 0}, 	// CAM_AWB_AWB_XSCALE
{0xC8F3, 0x02, BYTE_LEN, 0}, 	// CAM_AWB_AWB_YSCALE
{0xC906, 0x003C, WORD_LEN, 0}, 	// CAM_AWB_AWB_YSHIFT_PRE_ADJ
{0xC8F4, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_0
{0xC8F6, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_1
{0xC8F8, 0x0000, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_2
{0xC8FA, 0xE724, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_3
{0xC8FC, 0x1583, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_4
{0xC8FE, 0x2045, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_5
{0xC900, 0x03FF, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_6
{0xC902, 0x007C, WORD_LEN, 0}, 	// CAM_AWB_AWB_WEIGHTS_7
{0xC90C, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_R_L
{0xC90D, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_G_L
{0xC90E, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_B_L
{0xC90F, 0x88, BYTE_LEN, 0}, 	// CAM_AWB_K_R_R
{0xC910, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_G_R
{0xC911, 0x80, BYTE_LEN, 0}, 	// CAM_AWB_K_B_R

//[Step7-CPIPE_Preference]
{0x098E, 0x4926, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS [CAM_LL_START_BRIGHTNESS]
{0xC926, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_BRIGHTNESS
{0xC928, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_BRIGHTNESS
{0xC946, 0x0070, WORD_LEN, 0}, 	// CAM_LL_START_GAIN_METRIC
{0xC948, 0x00F3, WORD_LEN, 0}, 	// CAM_LL_STOP_GAIN_METRIC
{0xC952, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_TARGET_LUMA_BM
{0xC954, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_TARGET_LUMA_BM
{0xC92A, 0x84, BYTE_LEN, 0}, 	// CAM_LL_START_SATURATION
{0xC92B, 0x82, BYTE_LEN, 0}, 	// CAM_LL_END_SATURATION //201208300x4B
{0xC92C, 0x00, BYTE_LEN, 0}, 	// CAM_LL_START_DESATURATION
{0xC92D, 0x60, BYTE_LEN, 0}, 	// CAM_LL_END_DESATURATION  //201208300x4B
{0xC92E, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_DEMOSAIC
{0xC92F, 0x02, BYTE_LEN, 0}, 	// CAM_LL_START_AP_GAIN
{0xC930, 0x06, BYTE_LEN, 0}, 	// CAM_LL_START_AP_THRESH
{0xC931, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_DEMOSAIC
{0xC932, 0x01, BYTE_LEN, 0}, 	// CAM_LL_STOP_AP_GAIN
{0xC933, 0x0C, BYTE_LEN, 0}, 	// CAM_LL_STOP_AP_THRESH
{0xC934, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_RED
{0xC935, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_GREEN
{0xC936, 0x3C, BYTE_LEN, 0}, 	// CAM_LL_START_NR_BLUE
{0xC937, 0x0F, BYTE_LEN, 0}, 	// CAM_LL_START_NR_THRESH
{0xC938, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_RED
{0xC939, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_GREEN
{0xC93A, 0x64, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_BLUE
{0xC93B, 0x32, BYTE_LEN, 0}, 	// CAM_LL_STOP_NR_THRESH
{0xC93C, 0x0020, WORD_LEN, 0}, 	// CAM_LL_START_CONTRAST_BM
{0xC93E, 0x009A, WORD_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_BM
{0xC940, 0x00DC, WORD_LEN, 0}, 	// CAM_LL_GAMMA
{0xC942, 0x38, BYTE_LEN, 0}, 	// CAM_LL_START_CONTRAST_GRADIENT
{0xC943, 0x30, BYTE_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_GRADIENT
{0xC944, 0x50, BYTE_LEN, 0}, 	// CAM_LL_START_CONTRAST_LUMA_PERCENTAGE
{0xC945, 0x19, BYTE_LEN, 0}, 	// CAM_LL_STOP_CONTRAST_LUMA_PERCENTAGE
{0xC94A, 0x0230, WORD_LEN, 0}, 	// CAM_LL_START_FADE_TO_BLACK_LUMA
{0xC94C, 0x0010, WORD_LEN, 0}, 	// CAM_LL_STOP_FADE_TO_BLACK_LUMA
{0xC94E, 0x01CD, WORD_LEN, 0}, 	// CAM_LL_CLUSTER_DC_TH_BM
{0xC950, 0x05, BYTE_LEN, 0}, 	// CAM_LL_CLUSTER_DC_GATE_PERCENTAGE
{0xC951, 0x40, BYTE_LEN, 0}, 	// CAM_LL_SUMMING_SENSITIVITY_FACTOR
{0xC87B, 0x1B, BYTE_LEN, 0}, 	// CAM_AET_TARGET_AVERAGE_LUMA_DARK
{0xC878, 0x0E, BYTE_LEN, 0}, 	// CAM_AET_AEMODE
{0xC890, 0x0080, WORD_LEN, 0}, 	// CAM_AET_TARGET_GAIN
{0xC886, 0x0100, WORD_LEN, 0}, 	// CAM_AET_AE_MAX_VIRT_AGAIN
{0xC87C, 0x005A, WORD_LEN, 0}, 	// CAM_AET_BLACK_CLIPPING_TARGET
{0xB42A, 0x05, BYTE_LEN, 0}, 	// CCM_DELTA_GAIN
{0xA80A, 0x20, BYTE_LEN, 0}, 	// AE_TRACK_AE_TRACKING_DAMPENING_SPEED

//[Step8-Features]
//{0x098E, 0x0000, WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS,120828
//{0xC984, 0x8040, WORD_LEN, 0}, 	// CAM_PORT_OUTPUT_CONTROL,120828
{0x001E, 0x0777, WORD_LEN, 0}, 	// PAD_SLEW
//{0xC984, 0x8040, WORD_LEN, 0},  	// CAM_PORT_OUTPUT_CONTROL, parallel, 2012-3-21 0:41,120828

// MIPI, 2012-0516
{0x098E, 0x0000,WORD_LEN, 0}, 	// LOGICAL_ADDRESS_ACCESS
{0xC984, 0x8001,WORD_LEN, 0}, 	// CAM_PORT_OUTPUT_CONTROL,0x8041,120828
{0xC988, 0x0F00,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_HS_ZERO
{0xC98A, 0x0B07,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_HS_EXIT_HS_TRAIL
{0xC98C, 0x0D01,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_CLK_POST_CLK_PRE
{0xC98E, 0x071D,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_CLK_TRAIL_CLK_ZERO
{0xC990, 0x0006,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_T_LPX
{0xC992, 0x0A0C,WORD_LEN, 0}, 	// CAM_PORT_MIPI_TIMING_INIT_TIMING
{0x3C5A, 0x0009,WORD_LEN, 0}, 	// mipi delay

//[Anti-Flicker for MT9M114][50Hz]
{0x098E, 0xC88B, WORD_LEN, 0},  // LOGICAL_ADDRESS_ACCESS [CAM_AET_FLICKER_FREQ_HZ]
{0xC88B, 0x32, BYTE_LEN, 0},    // CAM_AET_FLICKER_FREQ_HZ

// Saturation
{0xC92A,0x84, BYTE_LEN, 0}, //0x840xC0
{0xC92B,0x82, BYTE_LEN, 0}, //0x460x60  //0x46 20120830
{0xC92D,0x60, BYTE_LEN, 0},  //0xC0 20120830

// AE
//Reg = 0xC87A,0x48
{0xC87A,0x43, BYTE_LEN, 0},  //0x3C

// Sharpness
//{0x098E,0xC92F, WORD_LEN, 0}, 
//{0xC92F,0x01, BYTE_LEN, 0}, 
//{0xC932,0x00, BYTE_LEN, 0}, 


// Target Gain
{0x098E,0x4890, WORD_LEN, 0},
{0xC890,0x0040, WORD_LEN, 0},

{0xc940,0x00C8, WORD_LEN, 0},  // CAM_LL_GAMMA

// mark this settings, 2012-9-3 17:52
//{0xAC06,0x64, BYTE_LEN, 0},
//{0xAC08,0x64, BYTE_LEN, 0},

{0xC942,0x3C, BYTE_LEN, 0},
{0xC944,0x4C, BYTE_LEN, 0},

{0xBC02,0x000F, WORD_LEN, 0},
{0xC94A,0x0040, WORD_LEN, 0},
{0xC94C,0x0005, WORD_LEN, 0},

// add T82 parameters, 2012-9-3 17:52
	/*MIPI setting for SOC1040*/
	{0x3C5A, 0x0009, WORD_LEN, 0},
	{0x3C44, 0x0080, WORD_LEN, 0},/*MIPI_CUSTOM_SHORT_PKT*/
	{0xA802, 0x0008, WORD_LEN, 0},
	{0xC908, 0x01, BYTE_LEN, 0},
	{0xC879, 0x01, BYTE_LEN, 0},
	{0xC909, 0x02, BYTE_LEN, 0},
	{0xA80A, 0x18, BYTE_LEN, 0},
	{0xA80B, 0x18, BYTE_LEN, 0},
	{0xAC16, 0x18, BYTE_LEN, 0},
	{0xC878, 0x0E, BYTE_LEN, 0},
	{0xBC02, 0x0007, WORD_LEN, 0},
	
//[Change-Config]
//{0x001E, 0x0777, WORD_LEN, 0}, 	// PAD_SLEW
{0x098E, 0xDC00, WORD_LEN, 0},  	// LOGICAL_ADDRESS_ACCESS [SYSMGR_NEXT_STATE]
{0xDC00, 0x28, BYTE_LEN, 0}, 	// SYSMGR_NEXT_STATE
{0x0080, 0x8002, WORD_LEN, 100}, 	// COMMAND_REGISTER
};



static struct mt9m114_i2c_reg_conf const wb_auto_tbl[] = {
{0x098E, 0x0000, WORD_LEN, 0}, 	
{0xC909, 0x03, BYTE_LEN, 10}, 		
};


static struct mt9m114_i2c_reg_conf const wb_incandescent_tbl[] = {
{0x098E, 0x0000, WORD_LEN, 0}, 	
{0xC909, 0x01, BYTE_LEN, 0}, 	
{0xC8F0, 0x0AF0, WORD_LEN, 10}, 	
};



static struct mt9m114_i2c_reg_conf const wb_flourescant_tbl[] = {
{0x098E, 0x0000, WORD_LEN, 0}, 	
{0xC909, 0x01, BYTE_LEN, 0}, 	
{0xC8F0, 0x113D, WORD_LEN, 10}, 	
};



static struct mt9m114_i2c_reg_conf const wb_daylight_tbl[] = {
{0x098E, 0x0000, WORD_LEN, 0}, 	
{0xC909, 0x01, BYTE_LEN, 0}, 	
{0xC8F0, 0x1964, WORD_LEN, 10}, 	
};


static struct mt9m114_i2c_reg_conf const wb_cloudy_tbl[] = {
{0x098E, 0x0000, WORD_LEN, 0}, 	
{0xC909, 0x01, BYTE_LEN, 0}, 	
{0xC8F0, 0x1770, WORD_LEN, 10}, 	
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_0[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0xF8, BYTE_LEN, 0}, 	// MCU_DATA_0 //96
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_1[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x0B, BYTE_LEN, 0}, 	// MCU_DATA_0  //B6
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_2[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x14, BYTE_LEN, 0}, 	// MCU_DATA_0  //D2
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_3[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x1D, BYTE_LEN, 0}, 	// MCU_DATA_0  //00
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_4[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x26, BYTE_LEN, 0}, 	// MCU_DATA_0  //1C
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 	
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_5[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x2F, BYTE_LEN, 0}, 	// MCU_DATA_0  //38
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 	
};

static struct mt9m114_i2c_reg_conf const brightness_tbl_6[] = {
{0x098E, 0xC870, WORD_LEN, 0}, 	// MCU_ADDRESS [AE_BASETARGET]
{0xC870, 0x38, BYTE_LEN, 0}, 	// MCU_DATA_0  //54
{0xDC00, 0x28, BYTE_LEN, 0}, 	
{0x0080, 0x8002, WORD_LEN, 10}, 
};


static struct v4l2_subdev_info mt9m114_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array mt9m114_init_conf[] = {
	{mt9m114_recommend_settings,
	ARRAY_SIZE(mt9m114_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_camera_i2c_conf_array mt9m114_confs[] = {
	{mt9m114_recommend_settings,
	ARRAY_SIZE(mt9m114_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};
static struct msm_sensor_output_info_t mt9m114_dimensions[] = {
	{
		.x_output = 0x500,
		.y_output = 0x3c0,
		.line_length_pclk = 0x500,
		.frame_length_lines = 0x3c0,
		.vt_pixel_clk = 48000000,
		.op_pixel_clk = 128000000,
		.binning_factor = 1,
	},

};

static struct msm_camera_csi_params mt9m114_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 7
};

static struct msm_camera_csi_params *mt9m114_csi_params_array[] = {
	&mt9m114_csi_params,
	&mt9m114_csi_params,
};

static struct msm_sensor_output_reg_addr_t mt9m114_reg_addr = {
	.x_output = 0xC868,	
	.y_output = 0xC86A,	
	.line_length_pclk = 0xC868,	
	.frame_length_lines = 0xC86A,
};

static struct msm_sensor_id_info_t mt9m114_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x2481,
};

static const struct i2c_device_id mt9m114_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mt9m114_s_ctrl},
	{ }
};

static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};


int32_t mt9m114_sensor_i2c_probe_for_init(struct i2c_client *client,
	const struct i2c_device_id *id)
{

	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct msm_camera_sensor_info *data;
	uint8_t cam_front = 0;
	uint8_t cam_main = 0;

	msm_cam_get_camera_num( &cam_front, &cam_main);
	pr_err("%s: cam_front = %d,cam_main = %d\n",__func__,cam_front,cam_main);
	
	pr_err("%s %s_i2c_probe called\n", __func__, client->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s %s i2c_check_functionality failed\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		pr_err("%s %s sensor_i2c_client NULL\n",
			__func__, client->name);
		rc = -EFAULT;
		return rc;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	if (s_ctrl->sensordata == NULL) {
		pr_err("%s %s NULL sensor data\n", __func__, client->name);
		return -EFAULT;
	}

      data = s_ctrl->sensordata;
      s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}
       //if main camera has been up,donnot config_vreg again
    if( cam_main == 0)
    {
	rc = msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: regulator on failed\n", __func__);
		goto config_vreg_failed;
	}

	rc = msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 1);
	if (rc < 0) {
		pr_err("%s: enable regulator failed\n", __func__);
		goto enable_vreg_failed;
	}
    }
       	
	
	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	gpio_direction_output(data->sensor_reset, 1);
	msleep(5);
	
	gpio_direction_output(data->sensor_reset, 0);
	msleep(5);
	
	gpio_direction_output(data->sensor_reset, 1);
	msleep(1);

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

#ifdef CONFIG_SENSOR_INFO
	msm_sensorinfo_set_front_sensor_id(0x2481);
#endif

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	rc = msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	pr_err("%s  rc=%d\n", __func__, rc);
	goto power_down;
probe_fail:
	pr_err("%s %s_i2c_probe failed\n", __func__, client->name);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	if( cam_main == 0)
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
	if( cam_main == 0)
	msm_camera_config_vreg(&s_ctrl->sensor_i2c_client->client->dev,
		s_ctrl->sensordata->sensor_platform_info->cam_vreg,
		s_ctrl->sensordata->sensor_platform_info->num_vreg,
		s_ctrl->reg_ptr, 0);
config_vreg_failed:
	msm_camera_request_gpio_table(data, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
power_down:
	if (rc > 0)
		rc = 0;
	s_ctrl->func_tbl->sensor_power_down(s_ctrl);
	return rc;

}


static struct i2c_driver mt9m114_i2c_driver = {
	.id_table = mt9m114_i2c_id,
	.probe  = mt9m114_sensor_i2c_probe_for_init,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9m114_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("mt9m114\n");

	rc = i2c_add_driver(&mt9m114_i2c_driver);

	return rc;
}

static struct v4l2_subdev_core_ops mt9m114_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops mt9m114_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops mt9m114_subdev_ops = {
	.core = &mt9m114_subdev_core_ops,
	.video  = &mt9m114_subdev_video_ops,
};

int32_t msm_mt9m114_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{

	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	pr_err("%s: %d\n", __func__, __LINE__);
	s_ctrl->reg_ptr = kzalloc(sizeof(struct regulator *)
			* data->sensor_platform_info->num_vreg, GFP_KERNEL);
	if (!s_ctrl->reg_ptr) {
		pr_err("%s: could not allocate mem for regulators\n",
			__func__);
		return -ENOMEM;
	}

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		goto config_gpio_failed;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}

	usleep_range(1000, 2000);

	gpio_direction_output(data->sensor_reset, 1);
	msleep(10);
	gpio_direction_output(data->sensor_reset, 0);
	msleep(10);
	gpio_direction_output(data->sensor_reset, 1);
	msleep(10);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);

	return rc;

enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);
request_gpio_failed:
	kfree(s_ctrl->reg_ptr);
	return rc;
		
}

int32_t msm_mt9m114_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	pr_err("%s\n", __func__);
	csi_config = 0;
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);

        gpio_direction_output(data->sensor_reset, 0);

	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);

	msm_camera_request_gpio_table(data, 0);
	if( s_ctrl->reg_ptr)
	{
	  kfree(s_ctrl->reg_ptr);
	  s_ctrl->reg_ptr = NULL;
	}

    zte_iso=0xff;
    zte_brightness=0xff;
    zte_awb=0xff;
	zte_antibanding=0xff;
	
	return 0;
}


//add params setting
int mt9m114_set_iso(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	unsigned short ISO_lev;
	pr_err("%s   value=%d\n", __func__, value);

	if(zte_force_write)
	{
	}
	else if( value == zte_iso)
	{
	   	pr_err("%s: return quickly\n", __func__);
	    return rc;
	}

	switch (value)
	{
	case MSM_V4L2_ISO_AUTO:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E,  0x4884, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC884, 0x0020, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC886, 0x0100, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("ISO_auto %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	}
	break;

	case MSM_V4L2_ISO_100:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E,  0x4884, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC884, 0x0020, MSM_CAMERA_I2C_WORD_DATA);
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_100 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC886, 0x00F0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("ISO_100 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	}
	break;

	case MSM_V4L2_ISO_200:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E,  0x4884, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC884, 0x0028, MSM_CAMERA_I2C_WORD_DATA);
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_200 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC886, 0x00F0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }  
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_200 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	}
	break;

	case MSM_V4L2_ISO_400:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E,  0x4884, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC884, 0x0030, MSM_CAMERA_I2C_WORD_DATA);
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_400 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC886, 0x00F0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_400 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	}
	break;

	case MSM_V4L2_ISO_800:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E,  0x4884, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC884, 0x0038, MSM_CAMERA_I2C_WORD_DATA);
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_800 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC886, 0x00F0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0xC884, &ISO_lev, MSM_CAMERA_I2C_WORD_DATA);	
	    pr_err("yanwei ISO_800 %s: entry: 0xC884=%x\n", __func__, ISO_lev);			
	}
	break;
	default:
	{
	    pr_err("%s: parameter error!\n", __func__);
	    rc = -EFAULT;
	}     
	}

	/*
	* Attention
	*
	* Time delay of 100ms or more is required by sensor,
	*
	* ISO config will have no effect after setting 
	* without time delay of 100ms or more
	*/
	msleep(10);

	if( rc >=0 )
		zte_iso = value;
	return rc;

}


int mt9m114_set_effect(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{

	long rc = 0;
#if 0//this project doesnot need
	switch (value)
	{
	case MSM_V4L2_EFFECT_OFF:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E, 0xC874, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC874, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xDC00, 0x28, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0080, 0x8004, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }	
	mdelay(10);	
	}            
	break;

	case MSM_V4L2_EFFECT_MONO:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E, 0xC874, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC874, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xDC00, 0x28, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0080, 0x8004, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }    
	mdelay(10);			
	}
	break;

	case MSM_V4L2_EFFECT_NEGATIVE:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E, 0xC874, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC874, 0x03, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xDC00, 0x28, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0080, 0x8004, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }   
	    mdelay(10);			
	}
	break;

	case MSM_V4L2_EFFECT_SEPIA:
	{
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098E, 0xC874, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xC874, 0x02, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0xDC00, 0x28, MSM_CAMERA_I2C_BYTE_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0080, 0x8004, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }   
	mdelay(10);			
	}         
	break;

	default:
	{


	    return -EFAULT;
	}
	}

#endif
      return rc;
}



int mt9m114_set_brightness(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{

	long rc = 0;
	pr_err("%s   value=%d\n", __func__, value);

    if(zte_force_write)
	{
	}
	else if( value == zte_brightness)
	{
	   	pr_err("%s: return quickly\n", __func__);
	    return rc;
	}

	switch (value)
	{
	  case MSM_V4L2_BRIGHTNESS_L0:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_0, ARRAY_SIZE(brightness_tbl_0));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L1:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_1, ARRAY_SIZE(brightness_tbl_1));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L2:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_2, ARRAY_SIZE(brightness_tbl_2));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L3:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_3, ARRAY_SIZE(brightness_tbl_3));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L4:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_4, ARRAY_SIZE(brightness_tbl_4));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L5:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_5, ARRAY_SIZE(brightness_tbl_5));
	  	break;
	  }
	  case MSM_V4L2_BRIGHTNESS_L6:
	  {
	  	rc = mt9m114_i2c_write_table( s_ctrl, brightness_tbl_6, ARRAY_SIZE(brightness_tbl_6));
	  	break;
	  }
	}
	
	if( rc >=0 )
		zte_brightness = value;
      return rc;
}



int mt9m114_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	long rc = 0;
    //uint16_t tmp_reg = 0;
	pr_err("%s E,value=%d\n",__func__,value);

    if( zte_antibanding== value )
	{
	  	CDBG("%s: return quickly\n", __func__);
	    return rc;
	}

    switch( value)
    {
      case CAMERA_ANTIBANDING_60HZ:
	  {
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA118, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA11E, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA124, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA12A, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA404, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }  
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x00A0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA103, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0005, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	  	break;
	  }
	  case CAMERA_ANTIBANDING_50HZ:
	  {
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA118, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA11E, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA124, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }    
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA12A, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0002, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }    
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA404, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x00E0, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA103, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0005, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    } 
	  	break;
	  }
	  case CAMERA_ANTIBANDING_OFF:
	  case CAMERA_ANTIBANDING_AUTO:
	  {
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA118, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0001, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA11E, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0001, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    } 
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA124, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0000, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }    
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA12A, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0001, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }  
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x098C, 0xA103, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    }
	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0990, 0x0005, MSM_CAMERA_I2C_WORD_DATA);
	    if (rc < 0)
	    {
	        return rc;
	    } 
	  	break;
	  }
	  default:
	  	pr_err("%s: invalid value\n", __func__);
	  	break;
    }


    if( rc == 0)
	  zte_antibanding= value;
	
	pr_err("%s X\n",__func__);
	return rc;
}


int mt9m114_set_wb(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	pr_err("%s   value=%d\n", __func__, value);
 
	if(zte_force_write)
	{
	}
	else if( value == zte_awb)
	{
	   	pr_err("%s: return quickly\n", __func__);
	    return rc;
	}
	
    switch(value)
    {
        case MSM_V4L2_WB_AUTO:
        {
           rc = mt9m114_i2c_write_table( s_ctrl, wb_auto_tbl, ARRAY_SIZE(wb_auto_tbl));
        }
        break;
        case MSM_V4L2_WB_INCANDESCENT:
        {
            rc = mt9m114_i2c_write_table( s_ctrl, wb_incandescent_tbl, ARRAY_SIZE(wb_incandescent_tbl));
        }
        break;
        
        case MSM_V4L2_WB_FLUORESCENT:
        {
	     rc = mt9m114_i2c_write_table( s_ctrl, wb_flourescant_tbl, ARRAY_SIZE(wb_flourescant_tbl));
        }
        break; 

	    case MSM_V4L2_WB_DAYLIGHT:
        {
	     rc = mt9m114_i2c_write_table( s_ctrl, wb_daylight_tbl, ARRAY_SIZE(wb_daylight_tbl));
        }
        break; 

        case MSM_V4L2_WB_CLOUDY_DAYLIGHT:
        {
            rc = mt9m114_i2c_write_table( s_ctrl, wb_cloudy_tbl, ARRAY_SIZE(wb_cloudy_tbl));
        }
        break;
        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }     
    }

	if( rc >=0 )
		zte_awb = value;

	return rc;
}



struct msm_sensor_v4l2_ctrl_info_t mt9m114_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.s_v4l2_ctrl = mt9m114_set_effect,
	},
	{
		.ctrl_id = V4L2_CID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_800,
		.step = 1,
		.s_v4l2_ctrl = mt9m114_set_iso,
	},
	{
		.ctrl_id = V4L2_CID_BRIGHTNESS,
		.min = MSM_V4L2_BRIGHTNESS_L0,
		.max = MSM_V4L2_BRIGHTNESS_L4,
		.step = 1,
		.s_v4l2_ctrl = mt9m114_set_brightness,
	},
	{
		.ctrl_id = V4L2_CID_AUTO_WHITE_BALANCE,
		.min = MSM_V4L2_WB_AUTO,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.s_v4l2_ctrl = mt9m114_set_wb,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_OFF,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.s_v4l2_ctrl = mt9m114_set_antibanding,
	},
};


int32_t msm_mt9m114_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
	uint16_t regstatus=0;	

	 pr_err("%s: E,update_type:%d,res:%d\n", __func__,update_type,res);

	msleep(10);
	if (update_type == MSM_SENSOR_REG_INIT) {

		pr_err("%s: MSM_SENSOR_REG_INIT\n", __func__);
		s_ctrl->curr_csi_params = NULL;
		//msm_sensor_enable_debugfs(s_ctrl);

		csi_config = 0;
		//msleep(10);
		
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {

		pr_err("%s: UPDATE_PERIODIC:%d\n", __func__,res);
		if( csi_config == 0)
		{
			s_ctrl->curr_csic_params = s_ctrl->csic_params[res];
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIC_CFG,
					s_ctrl->curr_csic_params);
			pr_err("msm_mt9m114_sensor_setting CSI config is done\n");
			mb();

			rc=msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x001A, 0x0001,MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0) {
				pr_err("%s: i2c write failed\n", __func__);
				return rc;
				}
			msleep(10);
			
			rc=msm_camera_i2c_write(s_ctrl->sensor_i2c_client,0x001A, 0x0000,MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0) {
				pr_err("%s: i2c write failed\n", __func__);
				return rc;
				}
			msleep(50);
			
			rc=mt9m114_i2c_write_table( s_ctrl, prev_snap_tbl, ARRAY_SIZE(prev_snap_tbl));
			if (rc < 0)
				return rc;

			msleep(50);
			csi_config = 1;

			msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3c42,&regstatus,MSM_CAMERA_I2C_WORD_DATA);
                     pr_err("mt9m114: 0x3c42=%x\n", regstatus);	

			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			                   NOTIFY_PCLK_CHANGE,
			                   &s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);

			msleep(20);

			zte_force_write = 1;
			mt9m114_set_iso(s_ctrl,0,zte_iso);
			mt9m114_set_brightness(s_ctrl,0,zte_brightness);
			mt9m114_set_wb(s_ctrl,0,zte_awb);
			mt9m114_set_antibanding(s_ctrl,0,zte_antibanding);
			zte_force_write = 0;
		}

	}
	 pr_err("%s: X\n", __func__);
	return rc;
}


static void mt9m114_stop_stream(struct msm_sensor_ctrl_t *s_ctrl) {}
static void mt9m114_start_stream(struct msm_sensor_ctrl_t *s_ctrl) {}

static struct msm_sensor_fn_t mt9m114_func_tbl = {
	.sensor_start_stream = mt9m114_start_stream,
	.sensor_stop_stream = mt9m114_stop_stream,
	.sensor_csi_setting = msm_mt9m114_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_mt9m114_sensor_power_up,
	.sensor_power_down = msm_mt9m114_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t mt9m114_regs = {
	.default_data_type = MSM_CAMERA_I2C_WORD_DATA,
	//.start_stream_conf = mt9m114_config_change_settings,
	//.start_stream_conf_size = ARRAY_SIZE(mt9m114_config_change_settings),
	.init_settings = &mt9m114_init_conf[0],
	.init_size = ARRAY_SIZE(mt9m114_init_conf),
	.mode_settings = &mt9m114_confs[0],
	.output_settings = &mt9m114_dimensions[0],
	.num_conf = ARRAY_SIZE(mt9m114_confs),
};

static struct msm_sensor_ctrl_t mt9m114_s_ctrl = {
	.msm_sensor_reg = &mt9m114_regs,
	.sensor_i2c_client = &mt9m114_sensor_i2c_client,
	.sensor_i2c_addr = 0x90,
	.msm_sensor_v4l2_ctrl_info = mt9m114_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(mt9m114_v4l2_ctrl_info),
	.sensor_output_reg_addr = &mt9m114_reg_addr,
	.sensor_id_info = &mt9m114_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &mt9m114_csi_params_array[0],
	.msm_sensor_mutex = &mt9m114_mut,
	.sensor_i2c_driver = &mt9m114_i2c_driver,
	.sensor_v4l2_subdev_info = mt9m114_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9m114_subdev_info),
	.sensor_v4l2_subdev_ops = &mt9m114_subdev_ops,
	.func_tbl = &mt9m114_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Aptina 1.26MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
