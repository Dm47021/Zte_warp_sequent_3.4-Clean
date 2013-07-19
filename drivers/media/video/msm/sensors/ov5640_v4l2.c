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
#include "ov5640_reg.h"

#define SENSOR_NAME "ov5640"

static int is_init=0;

int32_t ov5640_TouchAF_x = 40;//40;//-1;
int32_t ov5640_TouchAF_y = 30;//30;//-1;

uint16_t YAVG; //kenxu add for improve noise.
uint16_t WB_T; //neil add for detect wb temperature
int preview_sysclk, preview_HTS;

static unsigned int ov5640_preview_exposure;
static uint16_t ov5640_gain;
static unsigned short ov5640_preview_maxlines;
static int  zte_effect=0xff;
static int  zte_sat=0xff;
static int  zte_contrast=0xff;
static int  zte_sharpness=0xff;
static int  zte_afmode = 0xff;
static int8_t  zte_af_force_write=0;
static int  zte_iso=0xff;
static int  zte_exposure= 0xff;
static int  zte_awb= 0xff;
static int  zte_antibanding= 0xff;
//static int8_t  zte_brightness=3;

static uint16_t reg_0x3400;
static uint16_t reg_0x3401;
static uint16_t reg_0x3402;
static uint16_t reg_0x3403;
static uint16_t reg_0x3404;
static uint16_t reg_0x3405;

extern void zte_flash_auto_flag_set_value(int);

static int32_t ov5640_i2c_write_table(struct msm_sensor_ctrl_t *s_ctrl, struct ov5640_i2c_reg_conf const *reg_conf_tbl,int len)
{
	uint32_t i;
	int32_t rc = 0;
	
	for (i = 0; i < len; i++)
	{
		rc=msm_camera_i2c_write(s_ctrl->sensor_i2c_client, reg_conf_tbl[i].waddr, reg_conf_tbl[i].wdata, MSM_CAMERA_I2C_BYTE_DATA);
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

DEFINE_MUTEX(ov5640_mut);
static struct msm_sensor_ctrl_t ov5640_s_ctrl;

static struct msm_camera_i2c_reg_conf ov5640_start_settings[] = {
	{0x3017, 0xff},
	{0x3018, 0xff},
};

static struct msm_camera_i2c_reg_conf ov5640_stop_settings[] = {
	{0x3017, 0x00},
	{0x3018, 0x00},
};

static struct msm_camera_i2c_reg_conf ov5640_reset_settings[] = {
};

static struct msm_camera_i2c_reg_conf ov5640_prev_settings[] = {
  
};

static struct msm_camera_i2c_reg_conf ov5640_snap_settings[] = { 
};



static struct v4l2_subdev_info ov5640_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};


static struct msm_camera_i2c_conf_array ov5640_init_conf[] = {
     {&ov5640_reset_settings[0],
	ARRAY_SIZE(ov5640_reset_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_conf_array  ov5640_confs[] = {
	{& ov5640_snap_settings[0],
	ARRAY_SIZE(ov5640_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{& ov5640_prev_settings[0],
	ARRAY_SIZE(ov5640_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};
static struct msm_sensor_output_info_t ov5640_dimensions[] = {

	{
		.x_output = 0xA20,
		.y_output = 0x798,
		.line_length_pclk = 0xb1c,
		.frame_length_lines = 0x7b0,
		.vt_pixel_clk = 192000000,
		.op_pixel_clk = 192000000,
		.binning_factor = 1,
	},
      {
		.x_output = 0x500,
		.y_output = 0x3C0,
		.line_length_pclk = 0x500,
		.frame_length_lines = 0x3C0,
		.vt_pixel_clk = 192000000,
		.op_pixel_clk =192000000,
		.binning_factor = 1,
	},

};
#if 1
static struct msm_camera_csi_params ov5640_csi_params = {
	.data_format = CSI_8BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 0x6,
};

static struct msm_camera_csi_params *ov5640_csi_params_array[] = {
	&ov5640_csi_params,
	&ov5640_csi_params,
};
#endif
static struct msm_sensor_id_info_t ov5640_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x5640,
};

static const struct i2c_device_id ov5640_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov5640_s_ctrl},
	{ }
};
static struct msm_cam_clk_info cam_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_sensor_output_reg_addr_t ov5640_reg_addr = {
	.x_output = 0,//0x3808,
	.y_output = 0,//0x380A,
	.line_length_pclk = 0,//0x3808,
	.frame_length_lines = 0,//0x380A,
};


int32_t ov5640_sensor_i2c_probe_for_init(struct i2c_client *client,
	const struct i2c_device_id *id)
{

	int rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct msm_camera_sensor_info *data;
	CDBG("%s %s_i2c_probe called\n", __func__, client->name);
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

	msleep(1);

	gpio_direction_output(data->sensor_pwd, 0);
	msleep(1);
	
	gpio_direction_output(data->sensor_reset, 1);
	msleep(5);
	
	gpio_direction_output(data->sensor_reset, 0);
	msleep(5);
	
	gpio_direction_output(data->sensor_reset, 1);
	msleep(5);

	if (s_ctrl->func_tbl->sensor_match_id)
		rc = s_ctrl->func_tbl->sensor_match_id(s_ctrl);
	else
		rc = msm_sensor_match_id(s_ctrl);
	if (rc < 0)
		goto probe_fail;

#ifdef CONFIG_SENSOR_INFO
	msm_sensorinfo_set_sensor_id(0x5640);
#endif

	if (s_ctrl->func_tbl->sensor_csi_setting(s_ctrl, MSM_SENSOR_REG_INIT, 0) < 0)
	{
		pr_err("%s  sensor_setting init  failed\n",__func__);
		return rc;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);
	goto power_down;
probe_fail:
	pr_err("%s %s_i2c_probe failed\n", __func__, client->name);
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
config_gpio_failed:
	msm_camera_enable_vreg(&s_ctrl->sensor_i2c_client->client->dev,
			s_ctrl->sensordata->sensor_platform_info->cam_vreg,
			s_ctrl->sensordata->sensor_platform_info->num_vreg,
			s_ctrl->reg_ptr, 0);

enable_vreg_failed:
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


static struct i2c_driver ov5640_i2c_driver = {
	.id_table = ov5640_i2c_id,
	.probe  = ov5640_sensor_i2c_probe_for_init,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov5640_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	int rc = 0;
	CDBG("ov5640\n");

	rc = i2c_add_driver(&ov5640_i2c_driver);

	return rc;
}

static struct v4l2_subdev_core_ops ov5640_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov5640_subdev_ops = {
	.core = &ov5640_subdev_core_ops,
	.video  = &ov5640_subdev_video_ops,
};

static void ov5640_start_stream(struct msm_sensor_ctrl_t *s_ctrl) 
{
  pr_err("%s\n", __func__);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3017,0xff,MSM_CAMERA_I2C_BYTE_DATA);  
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3018,0xff,MSM_CAMERA_I2C_BYTE_DATA);  
}

static void ov5640_stop_stream(struct msm_sensor_ctrl_t *s_ctrl) 
	
{

  pr_err("%s\n", __func__);
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3017,0x00,MSM_CAMERA_I2C_BYTE_DATA);  
  msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3018,0x00,MSM_CAMERA_I2C_BYTE_DATA);   
}


int32_t msm_ov5640_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	rc = msm_camera_request_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}
	rc = msm_camera_config_gpio_table(data, 1);
	if (rc < 0) {
		pr_err("%s: config gpio failed\n", __func__);
		return rc;
	}

	if (s_ctrl->clk_rate != 0)
		cam_clk_info->clk_rate = s_ctrl->clk_rate;

	rc = msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 1);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto enable_clk_failed;
	}
	msleep(10);
	gpio_direction_output(data->sensor_pwd, 0);
	msleep(10);
	
	   	
	if (rc < 0) {
		pr_err("%s: i2c write failed\n", __func__);
		return rc;
	}

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(1);

	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_enable_i2c_mux(data->sensor_platform_info->i2c_conf);
		
	return rc;

enable_clk_failed:
		msm_camera_config_gpio_table(data, 0);
		return rc;
	
}



int32_t msm_ov5640_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	pr_err("%s\n", __func__);
	if (data->sensor_platform_info->i2c_conf &&
		data->sensor_platform_info->i2c_conf->use_i2c_mux)
		msm_sensor_disable_i2c_mux(
			data->sensor_platform_info->i2c_conf);

	if (data->sensor_platform_info->ext_power_ctrl != NULL)
		data->sensor_platform_info->ext_power_ctrl(0);
   
    ov5640_stop_stream(s_ctrl) ;
	gpio_direction_output(data->sensor_pwd, 1);

	msleep(30);

	zte_effect=0xff;
	zte_sat=0xff;
	zte_contrast=0xff;
	zte_sharpness=0xff;
	zte_afmode = 0xff;
	zte_iso=0xff;
	zte_exposure= 0xff;
	zte_awb= 0xff;
	zte_antibanding= 0xff;
	
	msm_cam_clk_enable(&s_ctrl->sensor_i2c_client->client->dev,
		cam_clk_info, &s_ctrl->cam_clk, ARRAY_SIZE(cam_clk_info), 0);
	msm_camera_config_gpio_table(data, 0);
	msm_camera_request_gpio_table(data, 0);
	return 0;
}


//add params setting

int ov5640_set_effect(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{

    int rc = 0;
    uint16_t tmp_reg = 0;
	  
    pr_err("%s   value=%d\n", __func__, value);
      
    switch (value)
    {
        case CAMERA_EFFECT_OFF:
        {

            msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x000f, MSM_CAMERA_I2C_BYTE_DATA);
	        if (rc < 0)
	        {
		      return rc;
	        }
		    if( zte_effect != 0xff)
		    {
		      pr_err("%s  msleep 100\n", __func__);
	          msleep(100);
		    }
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x001e, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg &= 0x0006;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }   
		    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
		    if (rc < 0)
		    {
			  return rc;
		    }
        }
        break;

        case CAMERA_EFFECT_MONO:
        {
	        msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x000f, MSM_CAMERA_I2C_BYTE_DATA);
		    if (rc < 0)
	        {
		      return rc;
	        }
		    if( zte_effect != 0xff)
		    msleep(100);
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0080, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0080, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0018;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
	        rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
	        if (rc < 0)
		    {
			   return rc;
		    }
        }
        break;
        
        case CAMERA_EFFECT_NEGATIVE:
        {
	        msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x000f, MSM_CAMERA_I2C_BYTE_DATA);
			if( zte_effect != 0xff)
			msleep(100);
		    if (rc < 0)
		    {
			   return rc;
		    }
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0040;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
		    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
	        if (rc < 0)
	        {
		      return rc;
	        }
        }
        break;          
        
        case CAMERA_EFFECT_SEPIA:
        {
	        msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x000f, MSM_CAMERA_I2C_BYTE_DATA);
		    if( zte_effect != 0xff)
		     msleep(100);
	        if (rc < 0)
	        {
		      return rc;
	        }
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x00a0, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0018;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
	        rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x4202, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
	        if (rc < 0)
	        {
		      return rc;
	        }
        }
        break;
        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }

       }

	if( rc == 0)
		zte_effect = value;

	return rc;
}


int ov5640_set_sharpness(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	pr_err("%s   value=%d\n", __func__, value);
	
    if(zte_effect != CAMERA_EFFECT_OFF)
    {
       pr_err("%s: return quickly\n", __func__);
       return rc;
    }

	if( zte_sharpness== value)
	{
	    pr_err("%s: return quickly\n", __func__);
	    return rc;
	}
		
    switch (value)
    {
        case MSM_V4L2_SHARPNESS_L0:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5308 ,0x0065, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5302 ,0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5303 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_SHARPNESS_L1:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5308 ,0x0065, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5302 ,0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5303 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_SHARPNESS_L2:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5308 ,0x0025, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5302 ,0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5303 ,0x0008, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;
        
        case MSM_V4L2_SHARPNESS_L3:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5308 ,0x0065, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5302 ,0x0008, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5303 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break; 

        case MSM_V4L2_SHARPNESS_L4:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5308 ,0x0065, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5302 ,0x0002, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5303 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;        
        
        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }     
    }

	if( rc == 0)
		zte_sharpness= value;
		
	return rc;
}


int ov5640_set_saturation(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	uint16_t tmp_reg = 0;
    pr_err("%s   value=%d\n", __func__, value);
	   
    
    switch(value)
    {
        case MSM_V4L2_SATURATION_L0:
        {
            #if 0	
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

			/*
			* ZTE_LJ_CAM_20101026
			* fix bug of no preview image after changing effect mode repeatedly
			*/
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			#endif
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            

			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0002;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}    
			tmp_reg = 0;

			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0040;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}              
			}
			break;

        case MSM_V4L2_SATURATION_L1:
        {
		   #if 0
	            //WT_CAM_20110421
	            tmp_reg = 0;
	            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
	            if (rc < 0)
	            {
	               return rc;
	            }

			/*
			* ZTE_LJ_CAM_20101026
			* fix bug of no preview image after changing effect mode repeatedly
			*/
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			#endif
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0038, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            

			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0002;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}  
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0040;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}      
			}
			break;

	case MSM_V4L2_SATURATION_L2:
	{
			#if 0
			//WT_CAM_20110421
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}

			/*
			* ZTE_LJ_CAM_20101026
			* fix bug of no preview image after changing effect mode repeatedly
			*/
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			#endif
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x001e, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0002;

			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			} 
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg &= 0x00bf;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}      
	}
	break;

	case MSM_V4L2_SATURATION_L3:
	{
			#if 0
			//WT_CAM_20110421
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}

			/*
			* ZTE_LJ_CAM_20101026
			* fix bug of no preview image after changing effect mode repeatedly
			*/
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			#endif
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0050, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            

			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0038, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0002;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}  
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0040;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}      
	}
	break;

	case MSM_V4L2_SATURATION_L4:
	{
			#if 0
			//WT_CAM_20110421
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}

			/*
			* ZTE_LJ_CAM_20101026
			* fix bug of no preview image after changing effect mode repeatedly
			*/
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			#endif
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5583, 0x0060, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}            

			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5584, 0x0048, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0002;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}  
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg &= 0x00FF;
			tmp_reg |= 0x0040;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}      
	}        
	break;

	default:
	{
			pr_err("%s: parameter error!\n", __func__);
			return -EFAULT;
	}            
	}
    if( rc == 0)
		zte_sat= value;
       
	return rc;
}


int ov5640_set_contrast(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	uint16_t tmp_reg = 0;
	pr_err("%s   value=%d\n", __func__, value);

   
    switch(value)
    {
        case CAMERA_CONTRAST_0:
        {
	        #if 0
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5586, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5585, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_1:
        {
	        #if 0
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5586, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5585, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_2:
        {
	        #if 0
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5586, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5585, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_3:
        {
	        #if 0
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5586, 0x0024, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5585, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_4:
        {
	        #if 0
            //WT_CAM_20110421
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5586, 0x002c, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5585, 0x001c, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
        }        
        break;

        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }            
    }
    if( rc == 0)
		zte_contrast= value;
	return rc;
}

int ov5640_set_wb(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	pr_err("%s   value=%d\n", __func__, value);
 

    switch(value)
    {
        case MSM_V4L2_WB_AUTO:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3406, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_WB_DAYLIGHT:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3406 ,0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3400 ,0x0006, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3401 ,0x001c, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3402 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3403 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3404 ,0x0005, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3405 ,0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
          
        }
        break;

        case MSM_V4L2_WB_INCANDESCENT:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3406 ,0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3400 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3401 ,0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3402 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3403 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3404 ,0x0008, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3405 ,0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;
        
        case MSM_V4L2_WB_FLUORESCENT:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3406 ,0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3400 ,0x0005, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3401 ,0x0048, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3402 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3403 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3404 ,0x0007, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3405 ,0x00c0, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break; 

        case MSM_V4L2_WB_CLOUDY_DAYLIGHT:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3406 ,0x0001, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3400 ,0x0006, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3401 ,0x0048, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3402 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3403 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3404 ,0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3405 ,0x00d3, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;
        #if 0
        case CAMERA_WB_MODE_NIGHT:
        {
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3a00, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a00, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a02 ,0x000b, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a03 ,0x0088, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a14 ,0x000b, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }

            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a15 ,0x0088, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            /*
               * Change preview FPS from 1500 to 375
               */
//            g_preview_frame_rate  = 375;
        }
        break;
        #endif
        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }     
    }

	zte_awb = value;

	return rc;
}

int ov5640_set_exposure_compensation(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	pr_err("%s   value=%d\n", __func__, value);

	if( zte_exposure == value)
	{
	    pr_err("%s: return quickly\n", __func__);
	    return rc;
	}

    switch(value)
    {
        case MSM_V4L2_EXPOSURE_N2:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a0f, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a10, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a11, 0x0050, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1b, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1e, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1f, 0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }        
        }
        break;
        
        case MSM_V4L2_EXPOSURE_N1:
        {	
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a0f, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a10, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a11, 0x0050, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1b, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1e, 0x0018, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1f, 0x0008, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }       
        }
        break;

        case MSM_V4L2_EXPOSURE_D:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a0f, 0x0038, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a10, 0x0030, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a11, 0x0060, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1b, 0x0038, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1e, 0x0030, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1f, 0x0014, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_EXPOSURE_P1:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a0f, 0x0048, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a10, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a11, 0x0070, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1b, 0x0048, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1e, 0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1f, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }        
        }
        break;

        case MSM_V4L2_EXPOSURE_P2:
        {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a0f, 0x0058, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a10, 0x0050, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a11, 0x0080, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1b, 0x0058, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1e, 0x0050, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3a1f, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }       
        }
        break;
        
        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }        
    }

    zte_exposure = value;
		
	return rc;
}

int ov5640_set_iso(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc = 0;
	pr_err("%s   value=%d\n", __func__, value);

	if( zte_iso == value)
	{
	    pr_err("%s: return quickly\n", __func__);
	    return rc;
	}

    switch (value)
    {
        case MSM_V4L2_ISO_AUTO:
        {
            //WT_CAM_20110428 iso value
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A18 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            #if 1 // 主观测试版本 2011-06-16 ken
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x00f8, MSM_CAMERA_I2C_BYTE_DATA);	
            #else
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            #endif
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_ISO_100:
        {
	        #if 0
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0002, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x350b ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }		  
	        msleep(100);	
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A18 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
        }
        break;

        case MSM_V4L2_ISO_200:
        {
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0002, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x350b ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }		  
	        msleep(100);
            #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A18 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x0040, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }		
	        #endif
        }
        break;

        case MSM_V4L2_ISO_400:
        {
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0002, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x350b ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }		  
	        msleep(100);
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A18 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x0080, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
	        #endif
        }
        break;

        case MSM_V4L2_ISO_800:
        {
	        #if 0
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0002, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
    	    rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x350b ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }		  
		    msleep(100);	
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A18 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A19 ,0x00f8, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #if 0
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3503 ,0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }	
	        #endif
        }
        break;

        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }    
    }

    if( rc==0 )
	   zte_iso = value;
	return rc;
}

int ov5640_set_touch_af_ae( struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	
	int x=value>>21;
	int y=(value&0x1ffc00)>>11;  

	pr_err("ov5640 %s  x=%d y=%d   value=%d     \n", __func__ ,x,y,value);

	if(x>0 || y>0)
	{
	ov5640_TouchAF_x = x/8;
	ov5640_TouchAF_y = y/8;	
	}
	
	return 0;
}


int ov5640_set_brightness( struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{

    long rc = 0;
    uint16_t tmp_reg = 0;

    pr_err("%s: entry: brightness=%d\n", __func__, value);

    switch(value)
    {
        case MSM_V4L2_BRIGHTNESS_L0:
        {
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0030, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L1:
        {
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L2:
        {
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }            
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }  
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L3:
        {
	        #if 0
        	tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
                  
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L4:
        {
	        #if 0
        	tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0010, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            } 
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L5:
        {
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0020, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case MSM_V4L2_BRIGHTNESS_L6:
        {
	        #if 0
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5001, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5001, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
	        #endif
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5587, 0x0030, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            //WT_CAM_20110411 write 5580
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5580, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0004;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5580, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x5588, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x5588, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }  
        }
        break;

        default:
        {
            pr_err("%s: parameter error!\n", __func__);
            return -EFAULT;
        }            
    }

    return rc;

}



int ov5640_set_af_mode( struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{

    long rc = 0;
    uint16_t af_ack=0x0002;
    uint32_t i;

    pr_err("%s: value:%d\n", __func__,value);

   if( zte_af_force_write)
   {
   }
   else if( zte_afmode == value )
   {
       pr_err("%s: return quickly\n", __func__);
       return rc;
   }

    if( value == 2 )//AF_MODE_AUTO
    {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3027, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3028, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
    }
    else if(  value == 1 )//AF_MODE_MACRO
    {
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3027, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3028, 0xff, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
			} 
    else
    {
	        return -EFAULT;
    }
	
     rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3023, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
     if (rc < 0)
     {
       return rc;
     }
     rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3022, 0x1a, MSM_CAMERA_I2C_BYTE_DATA);
     if (rc < 0)
     {
       return rc;
     }

	for (i = 0; (i < 100) && (0x0000 != af_ack); ++i)
	{
		af_ack = 0x0002;
		rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3023, &af_ack, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
		pr_err("%s: rc < 0\n", __func__);
		break;
		} 
	        mdelay(15);
       }

	zte_afmode = value;
    return rc;

}


int ov5640_set_antibanding(struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	long rc = 0;
    uint16_t tmp_reg = 0;
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
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3c01, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3C01, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }

            
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3C00, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A0A, 0x0000, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A0B, 0x00f6, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3A0D, 0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
	  	break;
	  }
	  case CAMERA_ANTIBANDING_50HZ:
	  {
            tmp_reg = 0;
            rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3c01, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg |= 0x0080;
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,  0x3C01, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
            rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,  0x3C00, 0x0004, MSM_CAMERA_I2C_BYTE_DATA);
            if (rc < 0)
            {
                return rc;
            }
	  	break;
	  }
	  case CAMERA_ANTIBANDING_OFF:
	  case CAMERA_ANTIBANDING_AUTO:
	  {
			tmp_reg = 0;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3c01, &tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			tmp_reg |= 0x0080;
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3C01, tmp_reg, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			return rc;
			}
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3C00, 0x0004, MSM_CAMERA_I2C_BYTE_DATA);
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



long ov5640_flash_auto_mode_flag_judge(struct msm_sensor_ctrl_t *s_ctrl)
{

	long rc = 0;
    uint16_t g_preview_gain_flash;
    uint16_t ov5640_auto_flash_threshold = 0xF0;
       
    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x350b, &g_preview_gain_flash, MSM_CAMERA_I2C_BYTE_DATA);

    if (rc < 0)
    {
        return rc;
    }
	
    if( g_preview_gain_flash >= ov5640_auto_flash_threshold )
  	{
       zte_flash_auto_flag_set_value(1);
    }
    else
  	{
  	   zte_flash_auto_flag_set_value(0);
    }
    return 0;  

}


static int32_t ov5640_i2c_write_b_sensor(struct msm_sensor_ctrl_t *s_ctrl,unsigned int waddr, unsigned short bdata)
{
   return msm_camera_i2c_write(s_ctrl->sensor_i2c_client,waddr, bdata,MSM_CAMERA_I2C_BYTE_DATA);  
}
static int32_t ov5640_i2c_read_byte(struct msm_sensor_ctrl_t *s_ctrl,unsigned short raddr, unsigned short *rdata)
{
   return msm_camera_i2c_read(s_ctrl->sensor_i2c_client,raddr, rdata,MSM_CAMERA_I2C_BYTE_DATA);
}

//===============AE start=================


int XVCLK = 2400;	// real clock/10000

static int ov5640_get_sysclk(struct msm_sensor_ctrl_t *s_ctrl)
{
//	int8_t buf[2];
	unsigned short  buf[2];
	 // calculate sysclk
	 int Multiplier, PreDiv, VCO, SysDiv, Pll_rdiv, Bit_div2x = 1, sclk_rdiv, sysclk;

	 int sclk_rdiv_map[] = { 1, 2, 4, 8};

	ov5640_i2c_read_byte(s_ctrl,0x3034, buf);
	
	buf[1] = buf[0] & 0x0f;
	 if (buf[1] == 8 || buf[1] == 10) {
		 Bit_div2x = buf[1] / 2;
	 }

	ov5640_i2c_read_byte(s_ctrl,0x3035, buf);
	 SysDiv = buf[0] >>4;
	 if(SysDiv == 0) {
		 SysDiv = 16;
	 }

	ov5640_i2c_read_byte(s_ctrl,0x3036, buf);
	 Multiplier = buf[0];

	ov5640_i2c_read_byte(s_ctrl,0x3037, buf);
	 PreDiv = buf[0] & 0x0f;
	 Pll_rdiv = ((buf[0] >> 4) & 0x01) + 1;

	ov5640_i2c_read_byte(s_ctrl,0x3108, buf);
	 buf[1] = buf[0] & 0x03;
	 sclk_rdiv = sclk_rdiv_map[buf[1]]; 

	 VCO = XVCLK * Multiplier / PreDiv;

	 sysclk = VCO / SysDiv / Pll_rdiv * 2 / Bit_div2x / sclk_rdiv;

	 return sysclk;
}

static int ov5640_get_HTS(struct msm_sensor_ctrl_t *s_ctrl)
{
	//int8_t buf[2];
	unsigned short buf[2];
	 // read HTS from register settings
	 int HTS;

	ov5640_i2c_read_byte(s_ctrl,0x380c, buf);
	ov5640_i2c_read_byte(s_ctrl,0x380d, &buf[1]);
	 HTS=buf[0];
	 HTS = (HTS<<8) + buf[1];

	 return HTS;
}

static int ov5640_get_VTS(struct msm_sensor_ctrl_t *s_ctrl)
{
	//int8_t buf[2];
	unsigned short  buf[2];
	 // read VTS from register settings
	 int VTS;

	ov5640_i2c_read_byte(s_ctrl,0x380e, buf);
	printk("%s: 0x380e=0x%x\n", __func__, buf[0]);
	ov5640_i2c_read_byte(s_ctrl,0x380f, &buf[1]);
	printk("%s: 0x380e=0x%x\n", __func__, buf[1]);
	VTS = buf[0];
	VTS = VTS<<8;
	VTS += (unsigned char)buf[1];
	printk("%s: VTS=0x%x\n", __func__, VTS);

	 return VTS;
}

static int ov5640_set_VTS(struct msm_sensor_ctrl_t *s_ctrl,int VTS)
{
	unsigned short buf[2];
	 // write VTS to registers
	 

	 //temp = VTS & 0xff;
	 buf[0] = VTS & 0xff;
	 printk("%s: VTS & oxff = 0x%x\n", __func__, buf[0]);
	 ov5640_i2c_write_b_sensor(s_ctrl,0x380f, buf[0]);

	 buf[0] = VTS>>8;
	 printk("%s: VTS>>8 = 0x%x\n", __func__, buf[0]);
	 ov5640_i2c_write_b_sensor(s_ctrl,0x380e, buf[0]);

	 return 0;
}

static int ov5640_set_shutter(struct msm_sensor_ctrl_t *s_ctrl,int shutter)
{
	 // write shutter, in number of line period
	unsigned short buf[2];
	 int temp;
	 
	 shutter = shutter & 0xffff;

	 temp = shutter & 0x0f;
	 buf[0] = temp<<4;
	 printk("%s: shutter&0x0f <<4 0x%x\n", __func__, buf[0]);
	 ov5640_i2c_write_b_sensor(s_ctrl,0x3502, buf[0]);

	 temp = shutter & 0xfff;
	 buf[0] = temp>>4;
	 printk("%s: shutter&0xfff >>4 0x%x\n", __func__, buf[0]);
	 ov5640_i2c_write_b_sensor(s_ctrl,0x3501,buf[0]);

	 temp = shutter>>12;
	 buf[0] = temp;
	 printk("%s: shutter>>12 0x%x\n", __func__, buf[0]);
	 ov5640_i2c_write_b_sensor(s_ctrl,0x3500, buf[0]);

	 return 0;
}

static int ov5640_set_gain16(struct msm_sensor_ctrl_t *s_ctrl,int gain16)
{
	 // write gain, 16 = 1x
	 int16_t buf[2];
	 //unsigned short buf[2];
	 
	 gain16 = gain16 & 0x3ff;

	 buf[0] = gain16 & 0xff;
	 ov5640_i2c_write_b_sensor(s_ctrl,0x350b, buf[0]);

	 buf[0] = gain16>>8;
	 ov5640_i2c_write_b_sensor(s_ctrl,0x350a, buf[0]);

	 return 0;
}

static int ov5640_get_light_frequency(struct msm_sensor_ctrl_t *s_ctrl)
{
// get banding filter value
int light_frequency = 0;
unsigned short buf[2];

ov5640_i2c_read_byte(s_ctrl,0x3c01, buf);

 if (buf[0] & 0x80) {
	 // manual
	 ov5640_i2c_read_byte(s_ctrl,0x3c00, &buf[1]);
	 if (buf[1] & 0x04) {
		 // 50Hz
		 light_frequency = 50;
	 }
	 else {
		 // 60Hz
		 light_frequency = 60;
	 }
 }
 else {
	 // auto
	 ov5640_i2c_read_byte(s_ctrl,0x3c0c, &buf[1]);
	 if (buf[1] & 0x01) {
		 // 50Hz
		 light_frequency = 50;
	 }
	 else {
		 // 60Hz
	 }
 }
 return light_frequency;
}



static void ov5640_set_bandingfilter(struct msm_sensor_ctrl_t *s_ctrl)
{
int16_t buf[2];
//unsigned short buf[2];
int preview_VTS;
int band_step60, max_band60, band_step50, max_band50;

 // read preview PCLK
 preview_sysclk = ov5640_get_sysclk(s_ctrl);

 // read preview HTS
 preview_HTS = ov5640_get_HTS(s_ctrl);

 // read preview VTS
 preview_VTS = ov5640_get_VTS(s_ctrl);

 // calculate banding filter
 // 60Hz
 band_step60 = preview_sysclk * 100/preview_HTS * 100/120;
 buf[0] = band_step60 >> 8;
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a0a, buf[0]);
 buf[0] = band_step60 & 0xff;
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a0b, buf[0]);

 max_band60 = (int)((preview_VTS-4)/band_step60);
 buf[0] = (int8_t)max_band60;
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a0d, buf[0]);

 // 50Hz
 band_step50 = preview_sysclk * 100/preview_HTS; 
 buf[0] = (int8_t)(band_step50 >> 8);
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a08, buf[0]);
buf[0] = (int8_t)(band_step50 & 0xff);
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a09, buf[0]);

 max_band50 = (int)((preview_VTS-4)/band_step50);
 buf[0] = (int8_t)max_band50;
 ov5640_i2c_write_b_sensor(s_ctrl,0x3a0e,buf[0]);
}

static long ov5640_hw_ae_transfer(struct msm_sensor_ctrl_t *s_ctrl)
{
    int rc = 0;
   //calculate capture exp & gain
   
	 int preview_shutter, preview_gain16;
	 int capture_gain16;
	 int capture_sysclk, capture_HTS, capture_VTS;
	 int light_frequency, capture_bandingfilter, capture_max_band;
	 long int capture_gain16_shutter,capture_shutter;
	 unsigned short average;
	 preview_shutter = ov5640_preview_exposure;	 
	 // read preview gain
        preview_gain16 = ov5640_gain;
	 printk("%s: preview_shutter=0x%x, preview_gain16=0x%x\n", __func__, preview_shutter, preview_gain16);
	//ov5640_flash_auto_mode_flag_judge();
	// read capture VTS
	 capture_VTS = ov5640_get_VTS(s_ctrl);
	 capture_HTS = ov5640_get_HTS(s_ctrl);
	 capture_sysclk = ov5640_get_sysclk(s_ctrl);
	 printk("%s: capture_VTS=0x%x, capture_HTS=0x%x, capture_sysclk=0x%x\n", __func__, capture_VTS, capture_HTS, capture_sysclk);

	 // get average	  
	 ov5640_i2c_read_byte(s_ctrl,0x56a1,&average);	 

	 // calculate capture banding filter
	 light_frequency = ov5640_get_light_frequency(s_ctrl);
	 if (light_frequency == 60) {
		 // 60Hz
		 capture_bandingfilter = capture_sysclk * 100 / capture_HTS * 100 / 120;
	 }
	 else {
		 // 50Hz
		 capture_bandingfilter = capture_sysclk * 100 / capture_HTS;
	 }
	 capture_max_band = (int)((capture_VTS - 4)/capture_bandingfilter);
	 printk("%s: light_frequency=0x%x, capture_bandingfilter=0x%x, capture_max_band=0x%x\n", __func__, light_frequency, capture_bandingfilter, capture_max_band);
      #if 0
	 // calculate capture shutter/gain16
	 if (average > AE_low && average < AE_high) {
		 // in stable range
		 capture_shutter = preview_shutter * capture_sysclk/preview_sysclk * preview_HTS/capture_HTS * AE_Target / average;
		//capture_gain16_shutter = preview_gain16 * preview_shutter /preview_sysclk * capture_sysclk /capture_HTS * preview_HTS * AE_Target / average;
	 }
	 else {
		 capture_shutter = preview_shutter * capture_sysclk/preview_sysclk * preview_HTS/capture_HTS;
		// capture_gain16_shutter = preview_gain16 * preview_shutter /preview_sysclk * capture_sysclk/capture_HTS* preview_HTS;
	 }
      #endif
	  	 capture_shutter = preview_shutter * capture_sysclk/preview_sysclk * preview_HTS/capture_HTS;
	        capture_gain16_shutter = preview_gain16 * capture_shutter;
	  
	 printk("%s:  preview_gain16=%d, preview_shutter=%d   capture_gain16_shutter=%ld\n", __func__, preview_gain16, preview_shutter,capture_gain16_shutter);
	 printk("%s: capture_sysclk=%d, preview_sysclk=%d, preview_HTS=%d\n", __func__, capture_sysclk, preview_sysclk, preview_HTS);

	 // gain to shutter
	 if(capture_gain16_shutter < (capture_bandingfilter * 16)) {
		 // shutter < 1/100
		 capture_shutter = capture_gain16_shutter/16;
		 if(capture_shutter <1)
			 capture_shutter = 1;

		 capture_gain16 = capture_gain16_shutter/capture_shutter;
		 if(capture_gain16 < 16)
			 capture_gain16 = 16;
	 printk("shutter < 1/100\n");
		 
	 }
	 else {
		 if(capture_gain16_shutter > (capture_bandingfilter*capture_max_band*16)) {
			 // exposure reach max
			 capture_shutter = capture_bandingfilter*capture_max_band;
			 capture_gain16 = capture_gain16_shutter / capture_shutter;
			 	 printk(" exposure reach max\n");
		 }
		 else {
			 // 1/100 < capture_shutter =< max, capture_shutter = n/100
			 capture_shutter = ((int)(capture_gain16_shutter/16/capture_bandingfilter)) * capture_bandingfilter;
			 capture_gain16 = capture_gain16_shutter / capture_shutter;
			  printk("1/100 < capture_shutter =< max, capture_shutter = n/100\n");
		 }
	 }

#if 0 // 主观测试版本 2011-12-21 ken
        //kenxu add for reduce noise under dark condition
        if(iCapture_Gain > 32) //gain > 2x, gain / 2, exposure * 2;
        {
          iCapture_Gain = iCapture_Gain / 2;
          ulCapture_Exposure = ulCapture_Exposure * 2;
        }
#endif
    printk("WB_T=%d\n", WB_T);
    if(WB_T == 0X02)
  	{

	ov5640_i2c_write_table( s_ctrl, ov5640_lens_shading_D65_tbl, ARRAY_SIZE(ov5640_lens_shading_D65_tbl));

  	}
    else if (WB_T == 0X06)
  	{
	ov5640_i2c_write_table( s_ctrl, ov5640_lens_shading_A_tbl, ARRAY_SIZE(ov5640_lens_shading_A_tbl));

  	}

         printk("%s: capture_gain16=0x%x\n", __func__, capture_gain16);
   // if (flash_led_enable == 0)
    	{
	    if(capture_gain16 > 48) //reach to max gain 16x, change blc to pass SNR test under low light
         {  
         printk("%s: YAVG=0x%x\n", __func__, YAVG);

      	    if(YAVG <=15) //10lux
      	 {
      	  capture_shutter = capture_shutter * 2;
          rc = ov5640_i2c_write_b_sensor(s_ctrl,0x4009, 0x50);
          if (rc < 0)
          {
            return rc;
          }
      	}
      	else if(YAVG <=23)//50lux
        {
          capture_shutter = capture_shutter * 4/3;
          rc = ov5640_i2c_write_b_sensor(s_ctrl,0x4009, 0x30);
          if (rc < 0)
          {
            return rc;
          }
        }            
     }
      	    printk("capture_shutter=%ld\n", capture_shutter);
    	}

	 // write capture gain
	 ov5640_set_gain16(s_ctrl,capture_gain16);

	// write capture shutter
	if (capture_shutter > (capture_VTS - 4)) {
	capture_VTS = capture_shutter + 4;
	ov5640_set_VTS(s_ctrl,capture_VTS);
	}
	ov5640_set_shutter(s_ctrl,capture_shutter);  
	mdelay(150);//150
	return rc;
}


static long ov5640_hw_ae_parameter_record(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	//uint16_t UV, temp;
	uint16_t ret_l,ret_m,ret_h, LencQ_H, LencQ_L;
      pr_err("%s: entry\n", __func__);
	ov5640_i2c_write_b_sensor(s_ctrl,0x3503, 0x07);
	ov5640_i2c_write_b_sensor(s_ctrl,0x5196, 0x23); //freeze awb kenxu update @20120516

	//================debug start========================
	#if 0
	ov5640_i2c_read_byte(s_ctrl,0x3c01, &temp);
	pr_err("0x3c01=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3c00, &temp);
	pr_err("0x3c00=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3a08, &temp);
	pr_err("0x3a08=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3a09, &temp);
	pr_err("0x3a09=0x%x\n", temp);

	ov5640_i2c_read_byte(s_ctrl,0x3a0a, &temp);
	pr_err("0x3a0a=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3a0b, &temp);
	pr_err("0x3a0b=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3a0d, &temp);
	pr_err("0x3a0d=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3a0e, &temp);
	pr_err("0x3a0e=0x%x\n", temp);


	ov5640_i2c_read_byte(s_ctrl,0x3034, &temp);
	pr_err("0x3034=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3035, &temp);
	pr_err("0x3035=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3036, &temp);
	pr_err("0x3036=0x%xn", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3037, &temp);
	pr_err("0x3037=0x%x\n", temp);


	ov5640_i2c_read_byte(s_ctrl,0x3108, &temp);
	pr_err("0x3108=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x3824, &temp);
	pr_err("0x3824=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x460c, &temp);
	pr_err("0x460c=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x300e, &temp);
	pr_err("0x300e=0x%x\n", temp);


	ov5640_i2c_read_byte(s_ctrl,0x380c, &temp);
	pr_err("0x380c=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x380d, &temp);
	pr_err("0x380d=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x380e, &temp);
	pr_err("0x380e=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x380f, &temp);
	pr_err("0x380f=0x%x\n", temp);


	ov5640_i2c_read_byte(s_ctrl,0x5588, &temp);
	pr_err("0x5588=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5583, &temp);
	pr_err("0x5583=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5584, &temp);
	pr_err("0x5584=0x%x\n", temp);

	ov5640_i2c_read_byte(s_ctrl,0x5580, &temp);
	pr_err("0x5580=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5001, &temp);
	pr_err("0x5001=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x558c, &temp);
	pr_err("0x558c=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5384, &temp);
	pr_err("0x5384=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5385, &temp);
	pr_err("0x5385=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5386, &temp);
	pr_err("0x5386=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5387, &temp);
	pr_err("0x5387=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5388, &temp);
	pr_err("0x5388=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x5389, &temp);
	pr_err("0x5389=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x538a, &temp);
	pr_err("0x538a=0x%x\n", temp);
	ov5640_i2c_read_byte(s_ctrl,0x538b, &temp);
	pr_err("0x538b=0x%x\n", temp);
       #endif
	//================debug end========================

    #if 0
	//modify for preview abnormal in mono mode after snapshot  by lijing ZTE_CAM_LJ_20120627
	if( zte_get_flash_current_state() )
	{
	if(CAMERA_EFFECT_OFF == zte_effect) {
			uint16_t UV, temp;
		//keep saturation same for preview and capture
		pr_err("lijing:effect off \n");
		ov5640_i2c_read_byte(s_ctrl,0x558c, &UV);
		ov5640_i2c_read_byte(s_ctrl,0x5588, &temp);
		temp = temp | 0x40;
		ov5640_i2c_write_b_sensor(s_ctrl,0x5588, temp); //Manual UV
		ov5640_i2c_write_b_sensor(s_ctrl,0x5583, UV);
		ov5640_i2c_write_b_sensor(s_ctrl,0x5584, UV);
		printk("preview_UV=%d\n", UV);
	}
	}
    #endif

	//keep Lenc same for preview and capture
	ov5640_i2c_read_byte(s_ctrl,0x350a, &LencQ_H);
	ov5640_i2c_read_byte(s_ctrl,0x350b, &LencQ_L);
	ov5640_i2c_write_b_sensor(s_ctrl,0x5054, LencQ_H);
	ov5640_i2c_write_b_sensor(s_ctrl,0x5055, LencQ_L);	
	ov5640_i2c_write_b_sensor(s_ctrl,0x504d, 0x02);	//Manual mini Q	

    //get preview exp & gain
    ret_h = ret_m = ret_l = 0;
    ov5640_preview_exposure = 0;
    ov5640_i2c_read_byte(s_ctrl,0x3500, &ret_h);
    ov5640_i2c_read_byte(s_ctrl,0x3501, &ret_m);
    ov5640_i2c_read_byte(s_ctrl,0x3502, &ret_l);
    ov5640_preview_exposure = (ret_h << 12) + (ret_m << 4) + (ret_l >> 4);
//    printk("preview_exposure=%d\n", ov5640_preview_exposure);

    ret_h = ret_m = ret_l = 0;
    ov5640_preview_maxlines = 0;
    ov5640_i2c_read_byte(s_ctrl,0x380e, &ret_h);
    ov5640_i2c_read_byte(s_ctrl,0x380f, &ret_l);
    ov5640_preview_maxlines = (ret_h << 8) + ret_l;
//    printk("Preview_Maxlines=%d\n", ov5640_preview_maxlines);

    //Read back AGC Gain for preview
    ov5640_gain = 0;
    ov5640_i2c_read_byte(s_ctrl,0x350b, &ov5640_gain);
//    printk("Gain,0x350b=0x%x\n", ov5640_gain);

	YAVG = 0;
	ov5640_i2c_read_byte(s_ctrl,0x56A1, &YAVG);
	WB_T = 0;
	ov5640_i2c_read_byte(s_ctrl,0x51d0, &WB_T);
	pr_err("YAVG=0x%x\n", YAVG);
	pr_err("WB_T=0x%x\n", WB_T);

 // read preview PCLK	 
 preview_sysclk = ov5640_get_sysclk(s_ctrl);	 
 // read preview HTS	 
 preview_HTS = ov5640_get_HTS(s_ctrl);
	
  #ifdef ZTE_FIX_WB_ENABLE
	zte_flash_on_fix_wb();
#endif

	return rc;
}


static long ov5640_af_hw_ae_parameter_record(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint16_t ret_l,ret_m,ret_h, LencQ_H, LencQ_L;
      pr_err("%s: entry\n", __func__);
	//ov5640_i2c_write_b_sensor(s_ctrl,0x3503, 0x07);
	//ov5640_i2c_write_b_sensor(s_ctrl,0x5196, 0x23); //freeze awb kenxu update @20120516

	//keep Lenc same for preview and capture
	ov5640_i2c_read_byte(s_ctrl,0x350a, &LencQ_H);
	ov5640_i2c_read_byte(s_ctrl,0x350b, &LencQ_L);
	ov5640_i2c_write_b_sensor(s_ctrl,0x5054, LencQ_H);
	ov5640_i2c_write_b_sensor(s_ctrl,0x5055, LencQ_L);	
	ov5640_i2c_write_b_sensor(s_ctrl,0x504d, 0x02);	//Manual mini Q	

    //get preview exp & gain
    ret_h = ret_m = ret_l = 0;
    ov5640_preview_exposure = 0;
    ov5640_i2c_read_byte(s_ctrl,0x3500, &ret_h);
    ov5640_i2c_read_byte(s_ctrl,0x3501, &ret_m);
    ov5640_i2c_read_byte(s_ctrl,0x3502, &ret_l);
    ov5640_preview_exposure = (ret_h << 12) + (ret_m << 4) + (ret_l >> 4);

    ret_h = ret_m = ret_l = 0;
    ov5640_preview_maxlines = 0;
    ov5640_i2c_read_byte(s_ctrl,0x380e, &ret_h);
    ov5640_i2c_read_byte(s_ctrl,0x380f, &ret_l);
    ov5640_preview_maxlines = (ret_h << 8) + ret_l;

    //Read back AGC Gain for preview
    ov5640_gain = 0;
    ov5640_i2c_read_byte(s_ctrl,0x350b, &ov5640_gain);

	YAVG = 0;
	ov5640_i2c_read_byte(s_ctrl,0x56A1, &YAVG);
	WB_T = 0;
	ov5640_i2c_read_byte(s_ctrl,0x51d0, &WB_T);
	pr_err("YAVG=0x%x\n", YAVG);
	pr_err("WB_T=0x%x\n", WB_T);

 // read preview PCLK	 
 preview_sysclk = ov5640_get_sysclk(s_ctrl);	 
 // read preview HTS	 
 preview_HTS = ov5640_get_HTS(s_ctrl);

    //add for improve flash pic quality
	ov5640_i2c_read_byte(s_ctrl,0x519f, &reg_0x3400);
	ov5640_i2c_read_byte(s_ctrl,0x51a0, &reg_0x3401);
	ov5640_i2c_read_byte(s_ctrl,0x51a1, &reg_0x3402);
	ov5640_i2c_read_byte(s_ctrl,0x51a2, &reg_0x3403);
	ov5640_i2c_read_byte(s_ctrl,0x51a3, &reg_0x3404);
	ov5640_i2c_read_byte(s_ctrl,0x51a4, &reg_0x3405);

	return rc;
}

/*
 * Auto Focus Trigger
 * WT_CAM_20110127 set af register to enable af function 
 */
 int32_t ov5640_af_trigger(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc=-EFAULT;
	uint16_t af_ack=0x0002;
	uint32_t i=0;

	pr_err("%s: entry\n", __func__);

	if(ov5640_TouchAF_x >= 0 && ov5640_TouchAF_y >= 0) 
	{
		//set to idle
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3023, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n", __func__, rc);
			goto done;
		}
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3022, 0x0008, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n",__func__, rc);        
			goto done;
		}
		//mdelay(15);
		for (i = 0; (i < 100) && (0x0000 != af_ack); ++i)
		{
			af_ack = 0x0002;
			mdelay(15);
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3023, &af_ack, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			  pr_err("%s: rc < 0\n", __func__);
			  goto done;
			} 	
			pr_err("Trig _1 Auto Focus  i  = %d ",i);
		}
		
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3024, (int)ov5640_TouchAF_x, MSM_CAMERA_I2C_BYTE_DATA);
		pr_err("yanwei ov5640_TouchAF_x=%d,ov5640_TouchAF_y=%d\n", ov5640_TouchAF_x,ov5640_TouchAF_y);			
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n", __func__, rc);
			goto done;
		}
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3025, (int)ov5640_TouchAF_y, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n",__func__, rc);        
			goto done;
		}

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3023, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n", __func__, rc);
			goto done;
		}
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3022, 0x0081, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n",__func__, rc);        
			goto done;
		}
		af_ack = 2;
		for (i = 0; (i < 100) && (0x0000 != af_ack); ++i)
		{
			mdelay(15);
			af_ack = 0x0002;
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3023, &af_ack, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			pr_err("%s: rc < 0\n", __func__);
			goto done;
			} 		
			pr_err("Trig _2 Auto Focus  i  = %d ",i);
		}

		//Use Trig Auto Focus command to start auto focus	
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3023, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n", __func__, rc);
			goto done;
		}

		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3022, 0x0003, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
		{
			pr_err("%s: failed, rc=%d!\n",__func__, rc);        
			goto done;
		}

	    rc = 0;
	}

	done:
	ov5640_TouchAF_x = -1;
	ov5640_TouchAF_y = -1;
	return rc;
}



/*
 * ov5640_af_get
 * Get Focus state 
 */
 int32_t ov5640_af_get(struct msm_sensor_ctrl_t *s_ctrl,int8_t *af_status)
{
	int32_t rc=0;
	uint16_t af_ack=0x0002;

	rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3023, &af_ack, MSM_CAMERA_I2C_BYTE_DATA);
    *af_status = (int8_t)af_ack;

	return rc;
}

 int32_t ov5640_af_stop(struct msm_sensor_ctrl_t *s_ctrl)
{
    int32_t rc=0;
	
	if( s_ctrl->curr_mode != 3/*SENSOR_MODE_VIDEO*/)	 
	{		 
	    if( zte_get_flash_current_state() )		 
		{		     
		  pr_err("%s: zte_get_flash_current_state\n",__func__); 	         
		  ov5640_af_hw_ae_parameter_record( s_ctrl);		 
		}		 
	}
	
	return rc;
}

 int32_t ov5640_get_flash_mode(struct msm_sensor_ctrl_t *s_ctrl,int *flashmode)
{

	*flashmode = zte_get_flash_current_state();

    pr_err("%s,flashmode:%d\n",__func__,*flashmode);
	return 0;
}

int ov5640_preview_flash_gain_value( struct msm_sensor_ctrl_t *s_ctrl,
		struct msm_sensor_v4l2_ctrl_info_t *ctrl_info, int value)
{
	int rc=0;
    uint16_t g_preview_gain_flash;
       
    rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x350b, &g_preview_gain_flash, MSM_CAMERA_I2C_BYTE_DATA);
    if (rc < 0)
    {
        return rc;
    }

	pr_err("%s: g_preview_gain_flash=0x%x \n", __func__,g_preview_gain_flash);
	
    if( g_preview_gain_flash >= 0xF0 )
  	{
       zte_flash_auto_flag_set_value(1);
    }
    else
  	{
  	   zte_flash_auto_flag_set_value(0);
    }
	return rc;
}

struct msm_sensor_v4l2_ctrl_info_t ov5640_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_AUTO_WHITE_BALANCE,
		.min = MSM_V4L2_WB_AUTO,
		.max = MSM_V4L2_WB_CLOUDY_DAYLIGHT,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_wb,
	},
	{
		.ctrl_id = V4L2_CID_SPECIAL_EFFECT,
		.min = MSM_V4L2_EFFECT_OFF,
		.max = MSM_V4L2_EFFECT_NEGATIVE,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_effect,
	},

	{
		.ctrl_id = V4L2_CID_CONTRAST,
		.min = MSM_V4L2_CONTRAST_L0,
		.max = MSM_V4L2_CONTRAST_L4,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_contrast,
	},
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L4,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_saturation,
	},
	{
		.ctrl_id = V4L2_CID_SHARPNESS,
		.min = MSM_V4L2_SHARPNESS_L0,
		.max = MSM_V4L2_SHARPNESS_L4,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_sharpness,
	},
	{
		.ctrl_id = V4L2_CID_EXPOSURE,
		.min = MSM_V4L2_EXPOSURE_N2,
		.max = MSM_V4L2_EXPOSURE_P2,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_exposure_compensation,
	},	
	{
		.ctrl_id = V4L2_CID_ISO,
		.min = MSM_V4L2_ISO_AUTO,
		.max = MSM_V4L2_ISO_800,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_iso,
	},
	{
		.ctrl_id = V4L2_CID_TOUCH_AF_AE,
		.min = 0,
		.max = 1,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_touch_af_ae,
	},
	{
		.ctrl_id = V4L2_CID_BRIGHTNESS,
		.min = MSM_V4L2_BRIGHTNESS_L0,
		.max = MSM_V4L2_BRIGHTNESS_L4,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_brightness,
	},
	{
		.ctrl_id = V4L2_CID_TOUCH_AF_MODE,
		.min = 0,
		.max = 6,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_af_mode,
	},
	{
		.ctrl_id = V4L2_CID_POWER_LINE_FREQUENCY,
		.min = MSM_V4L2_POWER_LINE_OFF,
		.max = MSM_V4L2_POWER_LINE_AUTO,
		.step = 1,
		.s_v4l2_ctrl = ov5640_set_antibanding,
	},
	{
		.ctrl_id = V4L2_CID_GET_PREVIEW_FLASH_GAIN_VALUE,
		.min = 0,
		.max = 1,
		.step = 1,
		.s_v4l2_ctrl = ov5640_preview_flash_gain_value,
	},
};

int32_t msm_ov5640_sensor_csi_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{

	int32_t rc = 0;
	uint16_t temp;
	
    pr_err("%s: E,update_type:%d,res:%d\n", __func__,update_type,res);
	  
    s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	if (update_type == MSM_SENSOR_REG_INIT) 
	{

		pr_err("%s: MSM_SENSOR_REG_INIT\n", __func__);
		//msm_sensor_enable_debugfs(s_ctrl);
		msleep(10);
	    if( is_init == 0)
        {
		  pr_err("%s:  write_init_settings\n", __func__);
          is_init =1 ;

		  ov5640_i2c_write_table( s_ctrl, init_settings_array, ARRAY_SIZE(init_settings_array));
		  msleep(5);
		  ov5640_i2c_write_table( s_ctrl, autofocus_value, ARRAY_SIZE(autofocus_value));
		  msleep(10);
        }
	} 
	else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) 
	{
	    if( res == 0)//snapshot
        {
			if( zte_get_flash_current_state() )
			{
			 pr_err("%s: zte_get_flash_current_state\n",__func__);
			 ov5640_i2c_write_b_sensor(s_ctrl,0x3503, 0x07);
			 ov5640_i2c_write_b_sensor(s_ctrl,0x5196, 0x23); //freeze awb kenxu update @20120516

			 if( zte_awb == MSM_V4L2_WB_AUTO)
			 {
               ov5640_i2c_write_b_sensor(s_ctrl,0x3406, 0x01);
			   ov5640_i2c_write_b_sensor(s_ctrl,0x3400, reg_0x3400);
		       ov5640_i2c_write_b_sensor(s_ctrl,0x3401, reg_0x3401);
		       ov5640_i2c_write_b_sensor(s_ctrl,0x3402, reg_0x3402);
		       ov5640_i2c_write_b_sensor(s_ctrl,0x3403, reg_0x3403);
		       ov5640_i2c_write_b_sensor(s_ctrl,0x3404, reg_0x3404);
		       ov5640_i2c_write_b_sensor(s_ctrl,0x3405, reg_0x3405);
			 }
			}
			else
			{
			 rc = ov5640_hw_ae_parameter_record( s_ctrl);
			}
			ov5640_i2c_write_table( s_ctrl, preview2snapshot_mode_array, ARRAY_SIZE(preview2snapshot_mode_array));
			ov5640_hw_ae_transfer(s_ctrl);
			pr_err("%s: goto snapshot mode\n", __func__);
         }
        else if( res == 1)//preview
        {
            pr_err("%s: goto preview mode\n", __func__);
		    ov5640_i2c_write_table( s_ctrl, snapshot2preview_mode_array, ARRAY_SIZE(snapshot2preview_mode_array));
		    msleep(5);

		    ov5640_i2c_read_byte(s_ctrl,0x5588, &temp);
		    temp = temp & 0xbf;
		    ov5640_i2c_write_b_sensor(s_ctrl,0x5588, temp); //Auto UV
	
		    ov5640_set_bandingfilter(s_ctrl);
			#if 0//no need to reset MCU after snapshot everytime
            msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3000,0x0020,MSM_CAMERA_I2C_BYTE_DATA);  
		    msleep(10);
		    msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3000,0x0000,MSM_CAMERA_I2C_BYTE_DATA);
            #endif
			/* Exit from AF mode */
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3023, 0x0001, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			  pr_err("%s: failed, rc=%d!\n", __func__, rc);
			  return rc;
			}
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3022, 0x0008, MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
			{
			  pr_err("%s: failed, rc=%d!\n", __func__, rc);
			  return rc;
			}
            //ov5640_set_effect(s_ctrl,0,zte_effect);
			zte_af_force_write = 1;
			ov5640_set_af_mode(s_ctrl,0,zte_afmode);
			zte_af_force_write = 0;
        }
	}
	
	s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
	msleep(10);
	
	pr_err("%s: X\n", __func__);
	return rc;
}


int32_t msm_ov5640_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
			int update_type, int res)
{
	int32_t rc = 0;
    pr_err("%s: E,update_type:%d,res:%d\n", __func__,update_type,res);
	return rc;
}

static struct msm_sensor_fn_t ov5640_func_tbl = {
	.sensor_start_stream = ov5640_start_stream,
	.sensor_stop_stream = ov5640_stop_stream,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_ov5640_sensor_power_up,
	.sensor_power_down = msm_ov5640_sensor_power_down,
	.sensor_csi_setting = msm_ov5640_sensor_csi_setting,
	.sensor_setting = msm_ov5640_sensor_setting,
	.af_trigger=ov5640_af_trigger,
	.af_get=ov5640_af_get,
	.af_stop=ov5640_af_stop,
	.get_flashmode=ov5640_get_flash_mode,
};

static struct msm_sensor_reg_t ov5640_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	 .start_stream_conf = ov5640_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov5640_start_settings),
	.stop_stream_conf = ov5640_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov5640_stop_settings),
	.init_settings = &ov5640_init_conf[0],
	.init_size = ARRAY_SIZE(ov5640_init_conf),
	.mode_settings = &ov5640_confs[0],
	.output_settings = &ov5640_dimensions[0],
	.num_conf = ARRAY_SIZE(ov5640_confs),
};

static struct msm_sensor_ctrl_t ov5640_s_ctrl = {
	.msm_sensor_reg = &ov5640_regs,
	.sensor_i2c_client = &ov5640_sensor_i2c_client,
	.sensor_i2c_addr = 0x78,
	.msm_sensor_v4l2_ctrl_info = ov5640_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(ov5640_v4l2_ctrl_info),
	.sensor_output_reg_addr = &ov5640_reg_addr,
	.sensor_id_info = &ov5640_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &ov5640_csi_params_array[0],
	.msm_sensor_mutex = &ov5640_mut,
	.sensor_i2c_driver = &ov5640_i2c_driver,
	.sensor_v4l2_subdev_info = ov5640_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5640_subdev_info),
	.sensor_v4l2_subdev_ops = &ov5640_subdev_ops,
	.func_tbl = &ov5640_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
	.is_csic = 0,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision VGA YUV sensor driver");
MODULE_LICENSE("GPL v2");
