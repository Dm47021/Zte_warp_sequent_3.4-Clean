/* ========================================================================
Copyright (c) 2001-2009 by ZTE Corporation.  All Rights Reserved.        

-------------------------------------------------------------------------------------------------
   Modify History
-------------------------------------------------------------------------------------------------
When           Who                   What 
=========================================================================== */
#ifndef ZTE_MEMLOG_H
#define ZTE_MEMLOG_H
#include <mach/msm_iomap.h>
#include <mach/msm_battery.h>
#define SMEM_LOG_INFO_BASE    MSM_SMEM_RAM_PHYS
#define SMEM_LOG_GLOBAL_BASE  (MSM_SMEM_RAM_PHYS + PAGE_SIZE)

#define SMEM_LOG_ENTRY_OFFSET (64*PAGE_SIZE)
#define SMEM_LOG_ENTRY_BASE   (MSM_SMEM_RAM_PHYS + SMEM_LOG_ENTRY_OFFSET)

#define ERR_DATA_MAX_SIZE 0x4000

#define MAGIC_VOLUME_DOWN_KEY 0x75898668
#define MAGIC_VOLUME_UP_KEY 0x75898680

typedef struct
{
	unsigned char display_flag;
	unsigned char img_id;
	unsigned char chg_fulled;
	unsigned char battery_capacity;
	unsigned char battery_valid;
} power_off_supply_status;

typedef struct {
  unsigned int ftm;
  unsigned int boot_reason;
  unsigned int reset_reason;
  unsigned int chg_count;
  unsigned int f3log;
  unsigned int err_fatal;
  unsigned int err_dload;
  unsigned int boot_pressed_keys[2];
  char err_log[ERR_DATA_MAX_SIZE];
  unsigned char flash_id[2];
  unsigned char sdrem_length;

  unsigned char reset_flag;
  unsigned char boot_success;
  unsigned char power_off_charge;
	power_off_supply_status power_off_charge_info;

  unsigned char pm_reason;
  
  unsigned int mboard_id;

 unsigned int key_is_on;
 unsigned int rtc_alarm;
  struct smem_batt_chg_t batchginfo;
  unsigned int secboot_enable;

} smem_global;

#endif
