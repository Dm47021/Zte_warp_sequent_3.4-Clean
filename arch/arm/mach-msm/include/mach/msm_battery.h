/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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


#ifndef __MSM_BATTERY_H__
#define __MSM_BATTERY_H__

#define AC_CHG     0x00000001
#define USB_CHG    0x00000002

struct msm_psy_batt_pdata {
	u32 voltage_max_design;
	u32 voltage_min_design;
	u32 voltage_fail_safe;
	u32 avail_chg_sources;
	u32 batt_technology;
	u32 (*calculate_capacity)(u32 voltage);
};

//YINTIANCI_BAT_20101101 begin
struct __attribute__((packed)) smem_batt_chg_t
{
	u8 charger_type;
	u8 charger_status;
	u8 charging;
	u8 chg_fulled;
	u8 battery_status;
	u8 battery_level;
	u16 battery_voltage;
	s16 battery_temp;
	u8 battery_capacity;
			
	u8 curr_capacity;	
	u8 curr_temp;
	u8 temp_data;
	u16 curr_voltage;
};

//YINTIANCI_BAT_20101101 end

#endif
