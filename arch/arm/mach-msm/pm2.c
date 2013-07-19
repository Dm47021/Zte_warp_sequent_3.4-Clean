/* arch/arm/mach-msm/pm2.c
 *
 * MSM Power Management Routines
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2012 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
  /*
 when		 who	what, where, why
 --------	 ---	----------------------------------------------------------
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/io.h>
#include <linux/tick.h>
#include <linux/memory.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_CPU_V7
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#endif
#ifdef CONFIG_CACHE_L2X0
#include <asm/hardware/cache-l2x0.h>
#endif
#ifdef CONFIG_VFP
#include <asm/vfp.h>
#endif

#ifdef CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_DEEP_POWER_DOWN
#include <mach/msm_migrate_pages.h>
#endif
#include <mach/socinfo.h>
#include <mach/proc_comm.h>
#include <asm/smp_scu.h>

#include "smd_private.h"
#include "smd_rpcrouter.h"
#include "acpuclock.h"
#include "clock.h"
#include "idle.h"
#include "irq.h"
#include "gpio.h"
#include "timer.h"
#include "pm.h"
#include "spm.h"
#include "sirc.h"
#include "pm-boot.h"
#include "devices-msm7x2xa.h"

//add by stone 2011_1212
#define CONFIG_ZTE_ALARM


/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_PM_DEBUG_SUSPEND = BIT(0),
	MSM_PM_DEBUG_POWER_COLLAPSE = BIT(1),
	MSM_PM_DEBUG_STATE = BIT(2),
	MSM_PM_DEBUG_CLOCK = BIT(3),
	MSM_PM_DEBUG_RESET_VECTOR = BIT(4),
	MSM_PM_DEBUG_SMSM_STATE = BIT(5),
	MSM_PM_DEBUG_IDLE = BIT(6),
	MSM_PM_DEBUG_HOTPLUG = BIT(7),
};

/* caozy_debug_20091229 */
//static int msm_pm_debug_mask;
static int msm_pm_debug_mask = MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE;

int power_collapsed;
module_param_named(
	debug_mask, msm_pm_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

#define MSM_PM_DPRINTK(mask, level, message, ...) \
	do { \
		if ((mask) & msm_pm_debug_mask) \
			printk(level message, ## __VA_ARGS__); \
	} while (0)

#define MSM_PM_DEBUG_PRINT_STATE(tag) \
	do { \
		MSM_PM_DPRINTK(MSM_PM_DEBUG_STATE, \
			KERN_INFO, "%s: " \
			"APPS_CLK_SLEEP_EN %x, APPS_PWRDOWN %x, " \
			"SMSM_POWER_MASTER_DEM %x, SMSM_MODEM_STATE %x, " \
			"SMSM_APPS_DEM %x\n", \
			tag, \
			__raw_readl(APPS_CLK_SLEEP_EN), \
			__raw_readl(APPS_PWRDOWN), \
			smsm_get_state(SMSM_POWER_MASTER_DEM), \
			smsm_get_state(SMSM_MODEM_STATE), \
			smsm_get_state(SMSM_APPS_DEM)); \
	} while (0)

#define MSM_PM_DEBUG_PRINT_SLEEP_INFO() \
	do { \
		if (msm_pm_debug_mask & MSM_PM_DEBUG_SMSM_STATE) \
			smsm_print_sleep_info(msm_pm_smem_data->sleep_time, \
				msm_pm_smem_data->resources_used, \
				msm_pm_smem_data->irq_mask, \
				msm_pm_smem_data->wakeup_reason, \
				msm_pm_smem_data->pending_irqs); \
	} while (0)

#define MAX_NR_CLKS 33

/******************************************************************************
 * Sleep Modes and Parameters
 *****************************************************************************/

static int msm_pm_idle_sleep_min_time = CONFIG_MSM7X00A_IDLE_SLEEP_MIN_TIME;
module_param_named(
	idle_sleep_min_time, msm_pm_idle_sleep_min_time,
	int, S_IRUGO | S_IWUSR | S_IWGRP
);

enum {
	MSM_PM_MODE_ATTR_SUSPEND,
	MSM_PM_MODE_ATTR_IDLE,
	MSM_PM_MODE_ATTR_LATENCY,
	MSM_PM_MODE_ATTR_RESIDENCY,
	MSM_PM_MODE_ATTR_NR,
};

static char *msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_NR] = {
	[MSM_PM_MODE_ATTR_SUSPEND] = "suspend_enabled",
	[MSM_PM_MODE_ATTR_IDLE] = "idle_enabled",
	[MSM_PM_MODE_ATTR_LATENCY] = "latency",
	[MSM_PM_MODE_ATTR_RESIDENCY] = "residency",
};

static char *msm_pm_sleep_mode_labels[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_SUSPEND] = " ",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = "power_collapse",
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] =
		"ramp_down_and_wfi",
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = "wfi",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] =
		"power_collapse_no_xo_shutdown",
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] =
		"standalone_power_collapse",
};

static struct msm_pm_platform_data *msm_pm_modes;
static struct msm_pm_irq_calls *msm_pm_irq_extns;
static struct msm_pm_cpr_ops *msm_cpr_ops;

struct msm_pm_kobj_attribute {
	unsigned int cpu;
	struct kobj_attribute ka;
};

#define GET_CPU_OF_ATTR(attr) \
	(container_of(attr, struct msm_pm_kobj_attribute, ka)->cpu)

struct msm_pm_sysfs_sleep_mode {
	struct kobject *kobj;
	struct attribute_group attr_group;
	struct attribute *attrs[MSM_PM_MODE_ATTR_NR + 1];
	struct msm_pm_kobj_attribute kas[MSM_PM_MODE_ATTR_NR];
};

/*
 * Write out the attribute.
 */
static ssize_t msm_pm_mode_attr_show(
	struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			u32 arg = mode->suspend_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			u32 arg = mode->idle_enabled;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_LATENCY])) {
			u32 arg = mode->latency;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_RESIDENCY])) {
			u32 arg = mode->residency;
			kp.arg = &arg;
			ret = param_get_ulong(buf, &kp);
		}

		break;
	}

	if (ret > 0) {
		strlcat(buf, "\n", PAGE_SIZE);
		ret++;
	}

	return ret;
}

/*
 * Read in the new attribute value.
 */
static ssize_t msm_pm_mode_attr_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct kernel_param kp;
		unsigned int cpu;
		struct msm_pm_platform_data *mode;

		if (msm_pm_sleep_mode_labels[i] == NULL)
			continue;

		if (strcmp(kobj->name, msm_pm_sleep_mode_labels[i]))
			continue;

		cpu = GET_CPU_OF_ATTR(attr);
		mode = &msm_pm_modes[MSM_PM_MODE(cpu, i)];

		if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_SUSPEND])) {
			kp.arg = &mode->suspend_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_IDLE])) {
			kp.arg = &mode->idle_enabled;
			ret = param_set_byte(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_LATENCY])) {
			kp.arg = &mode->latency;
			ret = param_set_ulong(buf, &kp);
		} else if (!strcmp(attr->attr.name,
			msm_pm_mode_attr_labels[MSM_PM_MODE_ATTR_RESIDENCY])) {
			kp.arg = &mode->residency;
			ret = param_set_ulong(buf, &kp);
		}

		break;
	}

	return ret ? ret : count;
}

 /* Add sysfs entries for one cpu. */
static int __init msm_pm_mode_sysfs_add_cpu(
	unsigned int cpu, struct kobject *modes_kobj)
{
	char cpu_name[8];
	struct kobject *cpu_kobj;
	struct msm_pm_sysfs_sleep_mode *mode = NULL;
	int i, j, k;
	int ret;

	snprintf(cpu_name, sizeof(cpu_name), "cpu%u", cpu);
	cpu_kobj = kobject_create_and_add(cpu_name, modes_kobj);
	if (!cpu_kobj) {
		pr_err("%s: cannot create %s kobject\n", __func__, cpu_name);
		ret = -ENOMEM;
		goto mode_sysfs_add_cpu_exit;
	}

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		int idx = MSM_PM_MODE(cpu, i);

		if ((!msm_pm_modes[idx].suspend_supported) &&
				(!msm_pm_modes[idx].idle_supported))
			continue;

		mode = kzalloc(sizeof(*mode), GFP_KERNEL);
		if (!mode) {
			pr_err("%s: cannot allocate memory for attributes\n",
				__func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		mode->kobj = kobject_create_and_add(
				msm_pm_sleep_mode_labels[i], cpu_kobj);
		if (!mode->kobj) {
			pr_err("%s: cannot create kobject\n", __func__);
			ret = -ENOMEM;
			goto mode_sysfs_add_cpu_exit;
		}

		for (k = 0, j = 0; k < MSM_PM_MODE_ATTR_NR; k++) {
			if ((k == MSM_PM_MODE_ATTR_IDLE) &&
				!msm_pm_modes[idx].idle_supported)
				continue;
			if ((k == MSM_PM_MODE_ATTR_SUSPEND) &&
			     !msm_pm_modes[idx].suspend_supported)
				continue;
			mode->kas[j].cpu = cpu;
			mode->kas[j].ka.attr.mode = 0644;
			mode->kas[j].ka.show = msm_pm_mode_attr_show;
			mode->kas[j].ka.store = msm_pm_mode_attr_store;
			mode->kas[j].ka.attr.name = msm_pm_mode_attr_labels[k];
			mode->attrs[j] = &mode->kas[j].ka.attr;
			j++;
		}
		mode->attrs[j] = NULL;

		mode->attr_group.attrs = mode->attrs;
		ret = sysfs_create_group(mode->kobj, &mode->attr_group);
		if (ret) {
			printk(KERN_ERR
				"%s: cannot create kobject attribute group\n",
				__func__);
			goto mode_sysfs_add_cpu_exit;
		}
	}

	ret = 0;

mode_sysfs_add_cpu_exit:
	if (ret) {
		if (mode && mode->kobj)
			kobject_del(mode->kobj);
		kfree(mode);
	}

	return ret;
}

/*
 * Add sysfs entries for the sleep modes.
 */
static int __init msm_pm_mode_sysfs_add(void)
{
	struct kobject *module_kobj = NULL;
	struct kobject *modes_kobj = NULL;
	unsigned int cpu;
	int ret;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		printk(KERN_ERR "%s: cannot find kobject for module %s\n",
			__func__, KBUILD_MODNAME);
		ret = -ENOENT;
		goto mode_sysfs_add_exit;
	}

	modes_kobj = kobject_create_and_add("modes", module_kobj);
	if (!modes_kobj) {
		printk(KERN_ERR "%s: cannot create modes kobject\n", __func__);
		ret = -ENOMEM;
		goto mode_sysfs_add_exit;
	}

	for_each_possible_cpu(cpu) {
		ret = msm_pm_mode_sysfs_add_cpu(cpu, modes_kobj);
		if (ret)
			goto mode_sysfs_add_exit;
	}

	ret = 0;

mode_sysfs_add_exit:
	return ret;
}

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	int i = MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN);
	return msm_pm_modes[i].latency - 1;
}

void __init msm_pm_set_platform_data(
	struct msm_pm_platform_data *data, int count)
{
	BUG_ON(MSM_PM_SLEEP_MODE_NR * num_possible_cpus() > count);
	msm_pm_modes = data;
}

void __init msm_pm_set_irq_extns(struct msm_pm_irq_calls *irq_calls)
{
	/* sanity check */
	BUG_ON(irq_calls == NULL || irq_calls->irq_pending == NULL ||
		irq_calls->idle_sleep_allowed == NULL ||
		irq_calls->enter_sleep1 == NULL ||
		irq_calls->enter_sleep2 == NULL ||
		irq_calls->exit_sleep1 == NULL ||
		irq_calls->exit_sleep2 == NULL ||
		irq_calls->exit_sleep3 == NULL);

	msm_pm_irq_extns = irq_calls;
}

void __init msm_pm_set_cpr_ops(struct msm_pm_cpr_ops *ops)
{
	msm_cpr_ops = ops;
}

/******************************************************************************
 * Sleep Limitations
 *****************************************************************************/
enum {
	SLEEP_LIMIT_NONE = 0,
	SLEEP_LIMIT_NO_TCXO_SHUTDOWN = 2,
	SLEEP_LIMIT_MASK = 0x03,
};

static uint32_t msm_pm_sleep_limit = SLEEP_LIMIT_NONE;
#ifdef CONFIG_MSM_MEMORY_LOW_POWER_MODE
enum {
	SLEEP_RESOURCE_MEMORY_BIT0 = 0x0200,
	SLEEP_RESOURCE_MEMORY_BIT1 = 0x0010,
};
#endif


/******************************************************************************
 * Configure Hardware for Power Down/Up
 *****************************************************************************/

#if defined(CONFIG_ARCH_MSM7X30)
#define APPS_CLK_SLEEP_EN (MSM_APCS_GCC_BASE + 0x020)
#define APPS_PWRDOWN      (MSM_ACC0_BASE + 0x01c)
#define APPS_SECOP        (MSM_TCSR_BASE + 0x038)
#define APPS_STANDBY_CTL  NULL
#else
#define APPS_CLK_SLEEP_EN (MSM_CSR_BASE + 0x11c)
#define APPS_PWRDOWN      (MSM_CSR_BASE + 0x440)
#define APPS_STANDBY_CTL  (MSM_CSR_BASE + 0x108)
#define APPS_SECOP	  NULL
#endif

/*
 * Configure hardware registers in preparation for Apps power down.
 */
static void msm_pm_config_hw_before_power_down(void)
{
	if (cpu_is_msm7x30() || cpu_is_msm8x55()) {
		__raw_writel(4, APPS_SECOP);
	} else if (cpu_is_msm7x27()) {
		__raw_writel(0x1f, APPS_CLK_SLEEP_EN);
	} else if (cpu_is_msm7x27a() || cpu_is_msm7x27aa() ||
		   cpu_is_msm7x25a() || cpu_is_msm7x25aa() ||
		   cpu_is_msm7x25ab()) {
		__raw_writel(0x7, APPS_CLK_SLEEP_EN);
	} else if (cpu_is_qsd8x50()) {
		__raw_writel(0x1f, APPS_CLK_SLEEP_EN);
		mb();
		__raw_writel(0, APPS_STANDBY_CTL);
	}
	mb();
	__raw_writel(1, APPS_PWRDOWN);
	mb();
}

/*
 * Program the top csr from core0 context to put the
 * core1 into GDFS, as core1 is not running yet.
 */
static void configure_top_csr(void)
{
	void __iomem *base_ptr;
	unsigned int value = 0;

	base_ptr = core1_reset_base();
	if (!base_ptr)
		return;

	/* bring the core1 out of reset */
	__raw_writel(0x3, base_ptr);
	mb();
	/*
	 * override DBGNOPOWERDN and program the GDFS
	 * count val
	 */

	 __raw_writel(0x00030002, (MSM_CFG_CTL_BASE + 0x38));
	mb();

	/* Initialize the SPM0 and SPM1 registers */
	msm_spm_reinit();

	/* enable TCSR for core1 */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value |= BIT(22);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();

	/* set reset bit for SPM1 */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value |= BIT(20);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();

	/* set CLK_OFF bit */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value |= BIT(18);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();

	/* set clamps bit */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value |= BIT(21);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();

	/* set power_up bit */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value |= BIT(19);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();

	/* Disable TSCR for core0 */
	value = __raw_readl((MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG));
	value &= ~BIT(22);
	__raw_writel(value,  MSM_CFG_CTL_BASE + MPA5_CFG_CTL_REG);
	mb();
	__raw_writel(0x0, base_ptr);
	mb();
}

/*
 * Clear hardware registers after Apps powers up.
 */
static void msm_pm_config_hw_after_power_up(void)
{

	if (cpu_is_msm7x30() || cpu_is_msm8x55()) {
		__raw_writel(0, APPS_SECOP);
		mb();
		__raw_writel(0, APPS_PWRDOWN);
		mb();
		msm_spm_reinit();
	} else if (cpu_is_msm8625()) {
		__raw_writel(0, APPS_PWRDOWN);
		mb();

		if (power_collapsed) {
			/*
			 * enable the SCU while coming out of power
			 * collapse.
			 */
			scu_enable(MSM_SCU_BASE);
			/*
			 * Program the top csr to put the core1 into GDFS.
			 */
			configure_top_csr();
		}
	} else {
		__raw_writel(0, APPS_PWRDOWN);
		mb();
		__raw_writel(0, APPS_CLK_SLEEP_EN);
		mb();
	}
}

/*
 * Configure hardware registers in preparation for SWFI.
 */
static void msm_pm_config_hw_before_swfi(void)
{
	if (cpu_is_qsd8x50()) {
		__raw_writel(0x1f, APPS_CLK_SLEEP_EN);
		mb();
	} else if (cpu_is_msm7x27()) {
		__raw_writel(0x0f, APPS_CLK_SLEEP_EN);
		mb();
	} else if (cpu_is_msm7x27a() || cpu_is_msm7x27aa() ||
		   cpu_is_msm7x25a() || cpu_is_msm7x25aa() ||
		   cpu_is_msm7x25ab()) {
		__raw_writel(0x7, APPS_CLK_SLEEP_EN);
		mb();
	}
}

/*
 * Respond to timing out waiting for Modem
 *
 * NOTE: The function never returns.
 */
static void msm_pm_timeout(void)
{
#if defined(CONFIG_MSM_PM_TIMEOUT_RESET_CHIP)
	printk(KERN_EMERG "%s(): resetting chip\n", __func__);
	msm_proc_comm(PCOM_RESET_CHIP_IMM, NULL, NULL);
#elif defined(CONFIG_MSM_PM_TIMEOUT_RESET_MODEM)
	printk(KERN_EMERG "%s(): resetting modem\n", __func__);
	msm_proc_comm_reset_modem_now();
#elif defined(CONFIG_MSM_PM_TIMEOUT_HALT)
	printk(KERN_EMERG "%s(): halting\n", __func__);
#endif
	for (;;)
		;
}


/******************************************************************************
 * State Polling Definitions
 *****************************************************************************/

struct msm_pm_polled_group {
	uint32_t group_id;

	uint32_t bits_all_set;
	uint32_t bits_all_clear;
	uint32_t bits_any_set;
	uint32_t bits_any_clear;

	uint32_t value_read;
};

/*
 * Return true if all bits indicated by flag are set in source.
 */
static inline bool msm_pm_all_set(uint32_t source, uint32_t flag)
{
	return (source & flag) == flag;
}

/*
 * Return true if any bit indicated by flag are set in source.
 */
static inline bool msm_pm_any_set(uint32_t source, uint32_t flag)
{
	return !flag || (source & flag);
}

/*
 * Return true if all bits indicated by flag are cleared in source.
 */
static inline bool msm_pm_all_clear(uint32_t source, uint32_t flag)
{
	return (~source & flag) == flag;
}

/*
 * Return true if any bit indicated by flag are cleared in source.
 */
static inline bool msm_pm_any_clear(uint32_t source, uint32_t flag)
{
	return !flag || (~source & flag);
}

/*
 * Poll the shared memory states as indicated by the poll groups.
 *
 * nr_grps: number of groups in the array
 * grps: array of groups
 *
 * The function returns when conditions specified by any of the poll
 * groups become true.  The conditions specified by a poll group are
 * deemed true when 1) at least one bit from bits_any_set is set OR one
 * bit from bits_any_clear is cleared; and 2) all bits in bits_all_set
 * are set; and 3) all bits in bits_all_clear are cleared.
 *
 * Return value:
 *      >=0: index of the poll group whose conditions have become true
 *      -ETIMEDOUT: timed out
 */
static int msm_pm_poll_state(int nr_grps, struct msm_pm_polled_group *grps)
{
	int i, k;

	for (i = 0; i < 50000; i++) {
		for (k = 0; k < nr_grps; k++) {
			bool all_set, all_clear;
			bool any_set, any_clear;

			grps[k].value_read = smsm_get_state(grps[k].group_id);

			all_set = msm_pm_all_set(grps[k].value_read,
					grps[k].bits_all_set);
			all_clear = msm_pm_all_clear(grps[k].value_read,
					grps[k].bits_all_clear);
			any_set = msm_pm_any_set(grps[k].value_read,
					grps[k].bits_any_set);
			any_clear = msm_pm_any_clear(grps[k].value_read,
					grps[k].bits_any_clear);

			if (all_set && all_clear && (any_set || any_clear))
				return k;
		}
		udelay(50);
	}

	printk(KERN_ERR "%s failed:\n", __func__);
	for (k = 0; k < nr_grps; k++)
		printk(KERN_ERR "(%x, %x, %x, %x) %x\n",
			grps[k].bits_all_set, grps[k].bits_all_clear,
			grps[k].bits_any_set, grps[k].bits_any_clear,
			grps[k].value_read);

	return -ETIMEDOUT;
}


/******************************************************************************
 * Suspend Max Sleep Time
 *****************************************************************************/

#define SCLK_HZ (32768)
#define MSM_PM_SLEEP_TICK_LIMIT (0x54600000)

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
static int msm_pm_sleep_time_override;
module_param_named(sleep_time_override,
	msm_pm_sleep_time_override, int, S_IRUGO | S_IWUSR | S_IWGRP);
#endif

static uint32_t msm_pm_max_sleep_time;

/*
 * Convert time from nanoseconds to slow clock ticks, then cap it to the
 * specified limit
 */
static int64_t msm_pm_convert_and_cap_time(int64_t time_ns, int64_t limit)
{
	do_div(time_ns, NSEC_PER_SEC / SCLK_HZ);
	return (time_ns > limit) ? limit : time_ns;
}

/*
 * Set the sleep time for suspend.  0 means infinite sleep time.
 */
void msm_pm_set_max_sleep_time(int64_t max_sleep_time_ns)
{
	unsigned long flags;

	local_irq_save(flags);
	if (max_sleep_time_ns == 0) {
		msm_pm_max_sleep_time = 0;
	} else {
		msm_pm_max_sleep_time = (uint32_t)msm_pm_convert_and_cap_time(
			max_sleep_time_ns, MSM_PM_SLEEP_TICK_LIMIT);

		if (msm_pm_max_sleep_time == 0)
			msm_pm_max_sleep_time = 1;
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): Requested %lld ns Giving %u sclk ticks (= %d s)\n", __func__,
		max_sleep_time_ns, msm_pm_max_sleep_time,msm_pm_max_sleep_time>>15);
	local_irq_restore(flags);
}
EXPORT_SYMBOL(msm_pm_set_max_sleep_time);


/******************************************************************************
 * Shared Memory Bits
 *****************************************************************************/

#define DEM_MASTER_BITS_PER_CPU             6

/* Power Master State Bits - Per CPU */
#define DEM_MASTER_SMSM_RUN \
	(0x01UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_RSA \
	(0x02UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_PWRC_EARLY_EXIT \
	(0x04UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_SLEEP_EXIT \
	(0x08UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_READY \
	(0x10UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))
#define DEM_MASTER_SMSM_SLEEP \
	(0x20UL << (DEM_MASTER_BITS_PER_CPU * SMSM_APPS_STATE))

/* Power Slave State Bits */
#define DEM_SLAVE_SMSM_RUN                  (0x0001)
#define DEM_SLAVE_SMSM_PWRC                 (0x0002)
#define DEM_SLAVE_SMSM_PWRC_DELAY           (0x0004)
#define DEM_SLAVE_SMSM_PWRC_EARLY_EXIT      (0x0008)
#define DEM_SLAVE_SMSM_WFPI                 (0x0010)
#define DEM_SLAVE_SMSM_SLEEP                (0x0020)
#define DEM_SLAVE_SMSM_SLEEP_EXIT           (0x0040)
#define DEM_SLAVE_SMSM_MSGS_REDUCED         (0x0080)
#define DEM_SLAVE_SMSM_RESET                (0x0100)
#define DEM_SLAVE_SMSM_PWRC_SUSPEND         (0x0200)


/******************************************************************************
 * Shared Memory Data
 *****************************************************************************/

#define DEM_MAX_PORT_NAME_LEN (20)

struct msm_pm_smem_t {
	uint32_t sleep_time;
	uint32_t irq_mask;
	uint32_t resources_used;
	uint32_t reserved1;

	uint32_t wakeup_reason;
	uint32_t pending_irqs;
	uint32_t rpc_prog;
	uint32_t rpc_proc;
	char     smd_port_name[DEM_MAX_PORT_NAME_LEN];
	uint32_t reserved2;
};


/******************************************************************************
 *
 *****************************************************************************/
static struct msm_pm_smem_t *msm_pm_smem_data;
static atomic_t msm_pm_init_done = ATOMIC_INIT(0);

static int msm_pm_modem_busy(void)
{
	if (!(smsm_get_state(SMSM_POWER_MASTER_DEM) & DEM_MASTER_SMSM_READY)) {
#ifndef CONFIG_ZTE_PLATFORM
		  if (!from_idle)
    
		MSM_PM_DPRINTK(MSM_PM_DEBUG_POWER_COLLAPSE,
			         /KERN_INFO, "%s(): master not ready1\n", __func__);
#endif
		return -EBUSY;
	}

	return 0;
}


#ifdef CONFIG_ZTE_PLATFORM

#ifdef CONFIG_ZTE_SUSPEND_WAKEUP_MONITOR
struct msm_pm_smem_t * get_msm_pm_smem_data(void)
{
	return msm_pm_smem_data;
}
#endif


#define ZTE_PM_DUMP_NOT_OKTS	//LHX_PM_20120211_01 while modem can't enter TCXO_SHUTDOWN,dump the not_okts clients, if clients include CLKREGIM,need to dump enabled clk.
#ifdef ZTE_PM_DUMP_NOT_OKTS
#define ZTE_PROC_COMM_CMD3_DUMP_NOT_OKTS 18		//proc command used to dump not_okts clients
#define ZTE_PROC_COMM_CMD3_DUMP_NOT_OKTS_CLK 19	//proc command used to dump clk if not_okts clients include CLKREGIM
#define NOT_OKTS_CLKREGIM  0x4	//maybe different,should adjust according to "Voters:CLKREGIM ADCDRV APROC NLED QDSP HS CE_DM DIAG UIM GL1 WL1 CHG GPS  " 

static int msm_pm_idle_get_not_okts = 0;
module_param_named(idle_enable_get_not_okts, msm_pm_idle_get_not_okts,
	int, S_IRUGO | S_IWUSR | S_IWGRP
);

void zte_get_not_okts_proccomm(bool from_idle)		// false to dump when suspend,true for  idle-->> pc
{
	uint32_t modem_not_okts =0;
	uint32_t subcmd=ZTE_PROC_COMM_CMD3_DUMP_NOT_OKTS;
	msm_proc_comm(PCOM_CUSTOMER_CMD3, &modem_not_okts, &subcmd);
	if(modem_not_okts)		//modem can't enter TCXO_SHUTDOWN
	{
		printk("PM: modem can't enter TCXO_SHUTDOWN cause of clients: 0x%x (Ignore 0x80 for APROC)while APP enter %s\n",modem_not_okts,from_idle?"IDLE-->>PC":"SUSPEND");
	}
	if(modem_not_okts & NOT_OKTS_CLKREGIM)	//modem can't enter TCXO_SHUTDOWN cause of CLKREGIM,need to dump enabled clks.
	{
		if(subcmd > 0) 
		{
			pr_info("[TCXO] enabled clock id :%d+ 1's position(begin 0) in 0x%x\n",(32*0),subcmd);
		}
		 subcmd=ZTE_PROC_COMM_CMD3_DUMP_NOT_OKTS_CLK;
		msm_proc_comm(PCOM_CUSTOMER_CMD3, &modem_not_okts, &subcmd);
		if(modem_not_okts > 0) 
		{
			pr_info("[TCXO] enabled clock id :%d+ 1's position(begin 0) in 0x%x\n",(32*1),modem_not_okts);
		}
		if(subcmd > 0) 
		{
			pr_info("[TCXO] enabled clock id :%d+ 1's position(begin 0) in 0x%x\n",(32*2),subcmd);
		}
		
	}
}

#endif	//ZTE_PM_DUMP_NOT_OKTS

#define ZTE_CLK_BY_MOD_UART_CLOCK_NO_SLEEP	
#ifdef ZTE_CLK_BY_MOD_UART_CLOCK_NO_SLEEP
#define ZTE_PROC_COMM_CMD3_RECORD_CLK_DISABLE 4
#define ZTE_PROC_COMM_CMD3_RECORD_CLK_ENABLE 5
static int clk_record_enable = 0;
static int clk_record_enable_pr = 0;
module_param(clk_record_enable, int, S_IRUGO | S_IWUSR | S_IWGRP);

static int msm_batt_enable_2_record_clk_proccomm(void)
{
	int config_last = 0;
	int record_enable;
	if(clk_record_enable){
		record_enable =ZTE_PROC_COMM_CMD3_RECORD_CLK_ENABLE;
		clk_record_enable=1;
		clk_record_enable_pr = 1;
	}else{
		record_enable=ZTE_PROC_COMM_CMD3_RECORD_CLK_DISABLE;
		clk_record_enable=0;
		clk_record_enable_pr = 0;
	}
	pr_info(" enalbe to record NO CLOSE CLK (4dis 5enable):%d\n",record_enable);
	return msm_proc_comm(PCOM_CUSTOMER_CMD3, &config_last, &record_enable);
}

#endif

long lateresume_2_earlysuspend_time_s = 0;		//LHX_PM_20110411_01 time to record how long it takes to earlysuspend after last resume. namely,to record how long the LCD keeps on.
void zte_update_lateresume_2_earlysuspend_time(bool resume_or_earlysuspend)	// LHX_PM_20110411_01 resume_or_earlysuspend? lateresume : earlysuspend
{
	if(resume_or_earlysuspend)//lateresume,need to record when the lcd is turned on
	{
		lateresume_2_earlysuspend_time_s = current_kernel_time().tv_sec;
	}
	else	//earlysuspend,need to record when the lcd is turned off
	{
		lateresume_2_earlysuspend_time_s = current_kernel_time().tv_sec - lateresume_2_earlysuspend_time_s;	//record how long the lcd keeps on
	}
}

extern unsigned pm_modem_sleep_time_get(void);
struct timespec time_updated_when_sleep_awake;
int	sleep_ok_to_print_wakeup_info=0;
void record_sleep_awake_time(bool record_sleep_awake)
{
	struct timespec ts;
	int time_updated_when_sleep_awake_s;
	int time_updated_when_sleep_awake_ms;
	long time_updated_when_sleep_awake_ms_temp;
	static unsigned amss_sleep_time_ms = 0;
	unsigned amss_sleep_time_ms_temp = 0;
	int percentage_amss_not_sleep_while_app_suspend = 0;	//the percentage of modem awake while app suspend in %o;
	static bool sleep_success_flag = false;  //set true while msm_pm_collapse returned 1 by passing record_sleep_awake as true;


	ts = current_kernel_time();
	time_updated_when_sleep_awake_ms_temp = (long)((ts.tv_sec - time_updated_when_sleep_awake.tv_sec) * MSEC_PER_SEC + (int)((ts.tv_nsec / NSEC_PER_MSEC) - (time_updated_when_sleep_awake.tv_nsec / NSEC_PER_MSEC)));
	time_updated_when_sleep_awake_s = (int)(time_updated_when_sleep_awake_ms_temp/MSEC_PER_SEC);
	time_updated_when_sleep_awake_ms = (int)(time_updated_when_sleep_awake_ms_temp - time_updated_when_sleep_awake_s * MSEC_PER_SEC);
	if(record_sleep_awake)//record awake time	//called in pm2.c when returned from power_collapse.
	{
		sleep_success_flag = true;
		sleep_ok_to_print_wakeup_info = 1;
		MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO, "%s(): APP keep: %5d.%03d s !!!!!!!!!!awake   lcd on for %5d s %3d %%\n", __func__,time_updated_when_sleep_awake_s,time_updated_when_sleep_awake_ms,(int)lateresume_2_earlysuspend_time_s,(int)(lateresume_2_earlysuspend_time_s*100/(time_updated_when_sleep_awake_s + 1)));//in case Division by zero, +1
		time_updated_when_sleep_awake = ts; 
		lateresume_2_earlysuspend_time_s = 0;	//LHX_PM_20110411_01 clear how long the lcd keeps on
	}
	else	//record sleep time	//called in suspend.c when exit suspend state!
	{
		if(!sleep_success_flag) //only record sleep time while really resume after successfully suspend/sleep;
		{
			printk("%s: modem sleep: resume after fail to suspend\n",__func__);
			return;
		}
		sleep_success_flag = false;
		amss_sleep_time_ms_temp = amss_sleep_time_ms;	//backup previous total sleep time
		amss_sleep_time_ms  = pm_modem_sleep_time_get();	//get new total sleep time
		amss_sleep_time_ms_temp = amss_sleep_time_ms - amss_sleep_time_ms_temp; //get the sleep time through last sleep
		amss_sleep_time_ms_temp = time_updated_when_sleep_awake_s - amss_sleep_time_ms_temp / MSEC_PER_SEC;	//get modem awake time while APP sleep IN second
		percentage_amss_not_sleep_while_app_suspend = (int)amss_sleep_time_ms_temp*1000/(time_updated_when_sleep_awake_s + 1);
		MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO, "%s(): APP keep: %5d.%03d s !!!!!!!!!!sleep modem awake %5d s %4d %%o\n", __func__,time_updated_when_sleep_awake_s,time_updated_when_sleep_awake_ms,(int)amss_sleep_time_ms_temp,percentage_amss_not_sleep_while_app_suspend);//modem keep awake normally about 2% while app sleeps
		time_updated_when_sleep_awake = ts; 
		#ifdef ZTE_PM_DUMP_NOT_OKTS
		if(percentage_amss_not_sleep_while_app_suspend >=100)
			zte_get_not_okts_proccomm(false);
		#endif
	}

}
extern void zte_need_2_prink_rpc_while_wakeup(void);
#endif	//CONFIG_ZTE_PLATFORM

/*
 * Power collapse the Apps processor.  This function executes the handshake
 * protocol with Modem.
 *
 * Return value:
 *      -EAGAIN: modem reset occurred or early exit from power collapse
 *      -EBUSY: modem not ready for our power collapse -- no power loss
 *      -ETIMEDOUT: timed out waiting for modem's handshake -- no power loss
 *      0: success
 */
static int msm_pm_power_collapse
	(bool from_idle, uint32_t sleep_delay, uint32_t sleep_limit)
{
	struct msm_pm_polled_group state_grps[2];
	unsigned long saved_acpuclk_rate;
	int collapsed = 0;
	int ret;
	int val;
	int modem_early_exit = 0;

#ifdef ZTE_CLK_BY_MOD_UART_CLOCK_NO_SLEEP
	if(clk_record_enable != clk_record_enable_pr)
		msm_batt_enable_2_record_clk_proccomm();	//enable modem record active clk when app sleep
#endif

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	{
	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO, "%s(): idle %d, delay %u, limit %u\n", __func__,
		(int)from_idle, sleep_delay, sleep_limit);
	}
	if (!(smsm_get_state(SMSM_POWER_MASTER_DEM) & DEM_MASTER_SMSM_READY)) {
#ifdef CONFIG_ZTE_PLATFORM
		if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO, "%s(): master not ready2\n", __func__);
		ret = -EBUSY;
		goto power_collapse_bail;
	}

	memset(msm_pm_smem_data, 0, sizeof(*msm_pm_smem_data));

	if (cpu_is_msm8625()) {
		/* Program the SPM */
		ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_POWER_COLLAPSE,
									false);
		WARN_ON(ret);
	}

	/* Call CPR suspend only for "idlePC" case */
	if (msm_cpr_ops && from_idle)
		msm_cpr_ops->cpr_suspend();

	msm_pm_irq_extns->enter_sleep1(true, from_idle,
						&msm_pm_smem_data->irq_mask);
	msm_sirc_enter_sleep();
	msm_gpio_enter_sleep(from_idle);

	msm_pm_smem_data->sleep_time = sleep_delay;
	msm_pm_smem_data->resources_used = sleep_limit;

	/* Enter PWRC/PWRC_SUSPEND */

	if (from_idle)
		smsm_change_state(SMSM_APPS_DEM, DEM_SLAVE_SMSM_RUN,
			DEM_SLAVE_SMSM_PWRC);
	else
		smsm_change_state(SMSM_APPS_DEM, DEM_SLAVE_SMSM_RUN,
			DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND);

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): PWRC");
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_SLEEP_INFO();

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_RSA;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
#ifdef CONFIG_ZTE_PLATFORM
		if (!from_idle)
#endif
		printk(KERN_EMERG "%s(): power collapse entry "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
#ifdef CONFIG_ZTE_PLATFORM
		if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		goto power_collapse_early_exit;
	}

	/* DEM Master in RSA */

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): PWRC RSA");

	ret = msm_pm_irq_extns->enter_sleep2(true, from_idle);
	if (ret < 0) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_irq_enter_sleep2 aborted, %d\n", __func__,
			ret);
		goto power_collapse_early_exit;
	}

	msm_pm_config_hw_before_power_down();
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): pre power down");

	saved_acpuclk_rate = acpuclk_power_collapse();
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
		"%s(): change clock rate (old rate = %lu)\n", __func__,
		saved_acpuclk_rate);

	if (saved_acpuclk_rate == 0) {
		msm_pm_config_hw_after_power_up();
		goto power_collapse_early_exit;
	}

	msm_pm_boot_config_before_pc(smp_processor_id(),
			virt_to_phys(msm_pm_collapse_exit));

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DPRINTK(MSM_PM_DEBUG_RESET_VECTOR, KERN_INFO,
		"%s(): vector undeclare:x x -> x x, fill them later\n", __func__);

#ifdef CONFIG_VFP
	if (from_idle)
		vfp_pm_suspend();
#endif

#ifdef CONFIG_CACHE_L2X0
	if (!cpu_is_msm8625())
		l2cc_suspend();
	else
		apps_power_collapse = 1;
#endif

	collapsed = msm_pm_collapse();

	/*
	 * TBD: Currently recognise the MODEM early exit
	 * path by reading the MPA5_GDFS_CNT_VAL register.
	 */
	if (cpu_is_msm8625()) {
		/*
		 * on system reset, default value of MPA5_GDFS_CNT_VAL
		 * is = 0x0, later modem reprogram this value to
		 * 0x00030004. Once APPS did a power collapse and
		 * coming out of it expected value of this register
		 * always be 0x00030004. Incase if APPS sees the value
		 * as 0x00030002 consider this case as a modem early
		 * exit.
		 */
		val = __raw_readl(MSM_CFG_CTL_BASE + 0x38);
		if (val != 0x00030002)
			power_collapsed = 1;
		else
			modem_early_exit = 1;
	}

#ifdef CONFIG_CACHE_L2X0
	if (!cpu_is_msm8625())
		l2cc_resume();
	else
		apps_power_collapse = 0;
#endif

	msm_pm_boot_config_after_pc(smp_processor_id());

	if (collapsed) {
#ifdef CONFIG_VFP
		if (from_idle)
			vfp_pm_resume();
#endif
		cpu_init();
		local_fiq_enable();
	}

	if (!from_idle)
		{
	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO,
		"%s(): msm_pm_collapse returned %d\n", __func__, collapsed);
	}

#ifdef CONFIG_ZTE_PLATFORM
		if((collapsed == 1)&&(!from_idle))
		{
			record_sleep_awake_time(true);
			zte_need_2_prink_rpc_while_wakeup();
		}
#endif

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
		"%s(): restore clock rate to %lu\n", __func__,
		saved_acpuclk_rate);
	if (acpuclk_set_rate(smp_processor_id(), saved_acpuclk_rate,
			SETRATE_PC) < 0)
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		printk(KERN_ERR "%s(): failed to restore clock rate(%lu)\n",
			__func__, saved_acpuclk_rate);

	msm_pm_irq_extns->exit_sleep1(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);

	msm_pm_config_hw_after_power_up();
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): post power up");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_any_set =
		DEM_MASTER_SMSM_RSA | DEM_MASTER_SMSM_PWRC_EARLY_EXIT;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		printk(KERN_EMERG "%s(): power collapse exit "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		goto power_collapse_early_exit;
	}

	/* Sanity check */
	if (collapsed && !modem_early_exit) {
		BUG_ON(!(state_grps[0].value_read & DEM_MASTER_SMSM_RSA));
	} else {
		BUG_ON(!(state_grps[0].value_read &
			DEM_MASTER_SMSM_PWRC_EARLY_EXIT));
		goto power_collapse_early_exit;
	}

	/* Enter WFPI */

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND,
		DEM_SLAVE_SMSM_WFPI);

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): WFPI");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_RUN;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);

	if (ret < 0) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		printk(KERN_EMERG "%s(): power collapse WFPI "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
		ret = -EAGAIN;
		goto power_collapse_restore_gpio_bail;
	}

	/* DEM Master == RUN */

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): WFPI RUN");
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_SLEEP_INFO();

	msm_pm_irq_extns->exit_sleep2(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);
	msm_pm_irq_extns->exit_sleep3(msm_pm_smem_data->irq_mask,
		msm_pm_smem_data->wakeup_reason,
		msm_pm_smem_data->pending_irqs);
	msm_gpio_exit_sleep();
	msm_sirc_exit_sleep();

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_WFPI, DEM_SLAVE_SMSM_RUN);
#ifdef ZTE_PM_DUMP_NOT_OKTS
if(from_idle && msm_pm_idle_get_not_okts)
	zte_get_not_okts_proccomm(from_idle);
#endif
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): RUN");

	smd_sleep_exit();

	if (cpu_is_msm8625()) {
		ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING,
									false);
		WARN_ON(ret);
	}

	/* Call CPR resume only for "idlePC" case */
	if (msm_cpr_ops && from_idle)
		msm_cpr_ops->cpr_resume();

	return 0;

power_collapse_early_exit:
	/* Enter PWRC_EARLY_EXIT */

	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND,
		DEM_SLAVE_SMSM_PWRC_EARLY_EXIT);

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): EARLY_EXIT");

	memset(state_grps, 0, sizeof(state_grps));
	state_grps[0].group_id = SMSM_POWER_MASTER_DEM;
	state_grps[0].bits_all_set = DEM_MASTER_SMSM_PWRC_EARLY_EXIT;
	state_grps[1].group_id = SMSM_MODEM_STATE;
	state_grps[1].bits_all_set = SMSM_RESET;

	ret = msm_pm_poll_state(ARRAY_SIZE(state_grps), state_grps);
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): EARLY_EXIT EE");

	if (ret < 0) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		printk(KERN_EMERG "%s(): power collapse EARLY_EXIT "
			"timed out waiting for Modem's response\n", __func__);
		msm_pm_timeout();
	}

	if (ret == 1) {
#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
		MSM_PM_DPRINTK(
			MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
			KERN_INFO,
			"%s(): msm_pm_poll_state detected Modem reset\n",
			__func__);
	}

	/* DEM Master == RESET or PWRC_EARLY_EXIT */

	ret = -EAGAIN;

power_collapse_restore_gpio_bail:
	msm_gpio_exit_sleep();
	msm_sirc_exit_sleep();

	/* Enter RUN */
	smsm_change_state(SMSM_APPS_DEM,
		DEM_SLAVE_SMSM_PWRC | DEM_SLAVE_SMSM_PWRC_SUSPEND |
		DEM_SLAVE_SMSM_PWRC_EARLY_EXIT, DEM_SLAVE_SMSM_RUN);

#ifdef CONFIG_ZTE_PLATFORM
	if (!from_idle)
#endif
	MSM_PM_DEBUG_PRINT_STATE("msm_pm_power_collapse(): RUN");

	if (collapsed)
		smd_sleep_exit();

	/* Call CPR resume only for "idlePC" case */
	if (msm_cpr_ops && from_idle)
		msm_cpr_ops->cpr_resume();

power_collapse_bail:
	if (cpu_is_msm8625()) {
		ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING,
									false);
		WARN_ON(ret);
	}

	return ret;
}

/*
 * Power collapse the Apps processor without involving Modem.
 *
 * Return value:
 *      0: success
 */
static int __ref msm_pm_power_collapse_standalone(bool from_idle)
{
	int collapsed = 0;
	int ret;
	void *entry;

#ifndef CONFIG_ZTE_PLATFORM
	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND|MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO, "%s()\n", __func__);
#endif

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_POWER_COLLAPSE, false);
	WARN_ON(ret);

	entry = (!smp_processor_id() || from_idle) ?
			msm_pm_collapse_exit : msm_secondary_startup;

	msm_pm_boot_config_before_pc(smp_processor_id(),
						virt_to_phys(entry));

#ifdef CONFIG_VFP
	vfp_pm_suspend();
#endif

#ifdef CONFIG_CACHE_L2X0
	if (!cpu_is_msm8625())
		l2cc_suspend();
#endif

	collapsed = msm_pm_collapse();

#ifdef CONFIG_CACHE_L2X0
	if (!cpu_is_msm8625())
		l2cc_resume();
#endif

	msm_pm_boot_config_after_pc(smp_processor_id());

	if (collapsed) {
#ifdef CONFIG_VFP
		vfp_pm_resume();
#endif
		cpu_init();
		local_fiq_enable();
	}

#ifndef CONFIG_ZTE_PLATFORM
	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND | MSM_PM_DEBUG_POWER_COLLAPSE,
		KERN_INFO,
		"%s(): msm_pm_collapse returned %d\n", __func__, collapsed);
#endif

	ret = msm_spm_set_low_power_mode(MSM_SPM_MODE_CLOCK_GATING, false);
	WARN_ON(ret);

	return !collapsed;
}

/*
 * Bring the Apps processor to SWFI.
 *
 * Return value:
 *      -EIO: could not ramp Apps processor clock
 *      0: success
 */
static int msm_pm_swfi(bool ramp_acpu)
{
	unsigned long saved_acpuclk_rate = 0;

	if (ramp_acpu) {
		saved_acpuclk_rate = acpuclk_wait_for_irq();
		MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
			"%s(): change clock rate (old rate = %lu)\n", __func__,
			saved_acpuclk_rate);

		if (!saved_acpuclk_rate)
			return -EIO;
	}

	if (!cpu_is_msm8625())
		msm_pm_config_hw_before_swfi();

	msm_arch_idle();

	if (ramp_acpu) {
		MSM_PM_DPRINTK(MSM_PM_DEBUG_CLOCK, KERN_INFO,
			"%s(): restore clock rate to %lu\n", __func__,
			saved_acpuclk_rate);
		if (acpuclk_set_rate(smp_processor_id(), saved_acpuclk_rate,
				SETRATE_SWFI) < 0)
			printk(KERN_ERR
				"%s(): failed to restore clock rate(%lu)\n",
				__func__, saved_acpuclk_rate);
	}

	return 0;
}

static int64_t msm_pm_timer_enter_suspend(int64_t *period)
{
	int time = 0;

	time = msm_timer_get_sclk_time(period);
	if (!time)
		pr_err("%s: Unable to read sclk.\n", __func__);
		return time;
}

static int64_t msm_pm_timer_exit_suspend(int64_t time, int64_t period)
{

	if (time != 0) {
		int64_t end_time = msm_timer_get_sclk_time(NULL);
		if (end_time != 0) {
			time = end_time - time;
			if (time < 0)
				time += period;
			} else
				time = 0;
		}
	return time;
}

/******************************************************************************
 * External Idle/Suspend Functions
 *****************************************************************************/

/*
 * Put CPU in low power mode.
 */
void arch_idle(void)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;

	int64_t timer_expiration;
	int latency_qos;
	int ret;
	int i;
	unsigned int cpu;
	int64_t t1;
	static DEFINE_PER_CPU(int64_t, t2);
	int exit_stat;
	bool low_power = false;

	if (!atomic_read(&msm_pm_init_done))
		return;

	cpu = smp_processor_id();
	latency_qos = pm_qos_request(PM_QOS_CPU_DMA_LATENCY);
	/* get the next timer expiration */
	timer_expiration = msm_timer_enter_idle();

	t1 = ktime_to_ns(ktime_get());
	msm_pm_add_stat(MSM_PM_STAT_NOT_IDLE, t1 - __get_cpu_var(t2));
	msm_pm_add_stat(MSM_PM_STAT_REQUESTED_IDLE, timer_expiration);
	exit_stat = MSM_PM_STAT_IDLE_SPIN;

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		allow[i] = true;

	if (num_online_cpus() > 1 ||
		(timer_expiration < msm_pm_idle_sleep_min_time) ||
		!msm_pm_irq_extns->idle_sleep_allowed()) {
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = false;
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = false;
	}

	for (i = 0; i < ARRAY_SIZE(allow); i++) {
		struct msm_pm_platform_data *mode =
					&msm_pm_modes[MSM_PM_MODE(cpu, i)];
		if (!mode->idle_supported || !mode->idle_enabled ||
			mode->latency >= latency_qos ||
			mode->residency * 1000ULL >= timer_expiration)
			allow[i] = false;
	}

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] ||
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]) {
		uint32_t wait_us = CONFIG_MSM_IDLE_WAIT_ON_MODEM;
		while (msm_pm_modem_busy() && wait_us) {
			if (wait_us > 100) {
				udelay(100);
				wait_us -= 100;
			} else {
				udelay(wait_us);
				wait_us = 0;
			}
		}

		if (msm_pm_modem_busy()) {
			allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = false;
			allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]
				= false;
		}
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_IDLE, KERN_INFO,
		"%s(): latency qos %d, next timer %lld, sleep limit %u\n",
		__func__, latency_qos, timer_expiration, sleep_limit);

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		MSM_PM_DPRINTK(MSM_PM_DEBUG_IDLE, KERN_INFO,
			"%s(): allow %s: %d\n", __func__,
			msm_pm_sleep_mode_labels[i], (int)allow[i]);

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] ||
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]) {
		/* Sync the timer with SCLK, it is needed only for modem
		 * assissted pollapse case.
		 */
		uint32_t sleep_delay;

		sleep_delay = (uint32_t) msm_pm_convert_and_cap_time(
			timer_expiration, MSM_PM_SLEEP_TICK_LIMIT);

		if (sleep_delay == 0) /* 0 would mean infinite time */
			sleep_delay = 1;

		if (!allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
			sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;

#if defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_IDLE_ACTIVE)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT1;
#elif defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_IDLE_RETENTION)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT0;
#endif

		ret = msm_pm_power_collapse(true, sleep_delay, sleep_limit);
		low_power = (ret != -EBUSY && ret != -ETIMEDOUT);

		if (ret)
			exit_stat = MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE;
		else {
			exit_stat = MSM_PM_STAT_IDLE_POWER_COLLAPSE;
			msm_pm_sleep_limit = sleep_limit;
		}
	} else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		ret = msm_pm_power_collapse_standalone(true);
		exit_stat = ret ?
			MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE :
			MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE;
	} else if (allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT]) {
		ret = msm_pm_swfi(true);
		if (ret)
			while (!msm_pm_irq_extns->irq_pending())
				udelay(1);
		exit_stat = ret ? MSM_PM_STAT_IDLE_SPIN : MSM_PM_STAT_IDLE_WFI;
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		msm_pm_swfi(false);
		exit_stat = MSM_PM_STAT_IDLE_WFI;
	} else {
		while (!msm_pm_irq_extns->irq_pending())
			udelay(1);
		exit_stat = MSM_PM_STAT_IDLE_SPIN;
	}

	msm_timer_exit_idle(low_power);
	__get_cpu_var(t2) = ktime_to_ns(ktime_get());
	msm_pm_add_stat(exit_stat, __get_cpu_var(t2) - t1);
}

/*
 * Suspend the Apps processor.
 *
 * Return value:
 *	-EPERM: Suspend happened by a not permitted core
 *      -EAGAIN: modem reset occurred or early exit from suspend
 *      -EBUSY: modem not ready for our suspend
 *      -EINVAL: invalid sleep mode
 *      -EIO: could not ramp Apps processor clock
 *      -ETIMEDOUT: timed out waiting for modem's handshake
 *      0: success
 */
static int msm_pm_enter(suspend_state_t state)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	uint32_t sleep_limit = SLEEP_LIMIT_NONE;
	int ret = -EPERM;
	int i;
	int64_t period = 0;
	int64_t time = 0;

	/* Must executed by CORE0 */
	if (smp_processor_id()) {
		__WARN();
		goto suspend_exit;
	}

	time = msm_pm_timer_enter_suspend(&period);

	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): sleep limit %u\n", __func__, sleep_limit);

	for (i = 0; i < ARRAY_SIZE(allow); i++)
		allow[i] = true;

	for (i = 0; i < ARRAY_SIZE(allow); i++) {
		struct msm_pm_platform_data *mode;
		mode = &msm_pm_modes[MSM_PM_MODE(0, i)];
		if (!mode->suspend_supported || !mode->suspend_enabled)
			allow[i] = false;
	}

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] ||
		allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]) {
		enum msm_pm_time_stats_id id;

		clock_debug_print_enabled();

#ifdef CONFIG_MSM_SLEEP_TIME_OVERRIDE
		if (msm_pm_sleep_time_override > 0) {
			int64_t ns;
			ns = NSEC_PER_SEC * (int64_t)msm_pm_sleep_time_override;
			msm_pm_set_max_sleep_time(ns);
			msm_pm_sleep_time_override = 0;
		}
#endif
		if (!allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE])
			sleep_limit = SLEEP_LIMIT_NO_TCXO_SHUTDOWN;

#if defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_ACTIVE)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT1;
#elif defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_RETENTION)
		sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT0;
#elif defined(CONFIG_MSM_MEMORY_LOW_POWER_MODE_SUSPEND_DEEP_POWER_DOWN)
		if (get_msm_migrate_pages_status() != MEM_OFFLINE)
			sleep_limit |= SLEEP_RESOURCE_MEMORY_BIT0;
#endif

		for (i = 0; i < 30 && msm_pm_modem_busy(); i++)
			udelay(500);

		ret = msm_pm_power_collapse(
			false, msm_pm_max_sleep_time, sleep_limit);

		if (ret)
			id = MSM_PM_STAT_FAILED_SUSPEND;
		else {
			id = MSM_PM_STAT_SUSPEND;
			msm_pm_sleep_limit = sleep_limit;
		}

		time = msm_pm_timer_exit_suspend(time, period);
		msm_pm_add_stat(id, time);
	} else if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		ret = msm_pm_power_collapse_standalone(false);
	} else if (allow[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT]) {
		ret = msm_pm_swfi(true);
		if (ret)
			while (!msm_pm_irq_extns->irq_pending())
				udelay(1);
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		msm_pm_swfi(false);
	}

suspend_exit:
	MSM_PM_DPRINTK(MSM_PM_DEBUG_SUSPEND, KERN_INFO,
		"%s(): return %d\n", __func__, ret);

	return ret;
}

static struct platform_suspend_ops msm_pm_ops = {
	.enter = msm_pm_enter,
	.valid = suspend_valid_only_mem,
};

/* Hotplug the "non boot" CPU's and put
 * the cores into low power mode
 */
void msm_pm_cpu_enter_lowpower(unsigned int cpu)
{
	bool allow[MSM_PM_SLEEP_MODE_NR];
	int i;

	for (i = 0; i < MSM_PM_SLEEP_MODE_NR; i++) {
		struct msm_pm_platform_data *mode;

		mode = &msm_pm_modes[MSM_PM_MODE(cpu, i)];
		allow[i] = mode->suspend_supported && mode->suspend_enabled;
	}

	MSM_PM_DPRINTK(MSM_PM_DEBUG_HOTPLUG, KERN_INFO,
		"CPU%u: %s: shutting down cpu\n", cpu, __func__);

	if (allow[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE]) {
		msm_pm_power_collapse_standalone(false);
	} else if (allow[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT]) {
		msm_pm_swfi(false);
	} else {
		MSM_PM_DPRINTK(MSM_PM_DEBUG_HOTPLUG, KERN_INFO,
			"CPU%u: %s: shutting down failed!!!\n", cpu, __func__);
	}
}

#ifdef CONFIG_ZTE_ALARM
#include <linux/zte_hibernate.h>
struct hb_alarm {
	s64 set;
	s64 expire;
};
static struct hb_alarm  hb_alarm = {0};

void clear_hb_alarm(void)
{
	hb_alarm.set = 0;
	hb_alarm.expire = 0;
}
EXPORT_SYMBOL(clear_hb_alarm);

int is_hb_alarm_expire(void)
{
	s64 nsec;
	struct timespec now = {0};
	if (hb_alarm.set == 0 || hb_alarm.expire == 0) {
		return 0;
	}
	now = current_kernel_time();
	nsec = 	timespec_to_ns(&now);
	if (is_in_hibernate()) {	
		printk(KERN_ERR"hb:%s %d %lld %lld now %lld %lld\n", __FUNCTION__, __LINE__, hb_alarm.set, hb_alarm.expire, nsec, nsec - hb_alarm.set);
	}
	//with a tolerance of less than 3s
	if (nsec - hb_alarm.set + (s64)3000*NSEC_PER_MSEC >= hb_alarm.expire) {
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(is_hb_alarm_expire);
static void set_hb_alarm(unsigned int expire)
{
	struct timespec now = current_kernel_time();
	if (0 == expire) {
		clear_hb_alarm();
		return ;
	}
	hb_alarm.set = timespec_to_ns(&now);
	hb_alarm.expire = (s64)expire*NSEC_PER_MSEC;
	printk(KERN_ERR"hb:%s %d %lld %lld %u\n", __FUNCTION__, __LINE__, hb_alarm.set, hb_alarm.expire, expire);
}

#else
void clear_hb_alarm(void){;}
EXPORT_SYMBOL(clear_hb_alarm);
static void set_hb_alarm(unsigned int expire) {;}
int is_hb_alarm_expire(void) {return 0;}
EXPORT_SYMBOL(is_hb_alarm_expire);
#endif
/* end */
/******************************************************************************
 *
 *****************************************************************************/
#ifdef CONFIG_ZTE_ALARM
#define ZTE_PROC_COMM_CMD3_RTC_ALARM_DISABLE 6
#define ZTE_PROC_COMM_CMD3_RTC_ALARM_ENABLE 7
	static int zte_alarm_read_proc
		(char *page, char **start, off_t off, int count, int *eof, void *data)
	{
		 int len = 0;
		 uint32_t *expiration_ms=(uint32_t*)data;
		 
		 printk("ZTE rtc alarm read: the expiration time is =%d ms\n",*expiration_ms);
		
		  len = sprintf(page, "%d\n",*expiration_ms);
		  return len;
	}
	
	static int zte_alarm_write_proc(struct file *file, const char __user *buffer,
		unsigned long count, void *data)
	{
		char tmp[16] = {0};
		unsigned int  proc_id = 0;
		uint32_t *expiration_ms=(uint32_t*)data;
		
		if (count < 1)
			return -EINVAL;
		
		if(copy_from_user(tmp, buffer, count))
					return -EFAULT;
	
		//To do something here
		*expiration_ms=(uint32_t) (simple_strtol(tmp, NULL, 10));
	
		  printk("ZTE rtc alarm write: the expiration time is =%d ms\n",*expiration_ms);
		  
            /* ruanmeisi hibernate alarm */
           set_hb_alarm(*expiration_ms);

		  if(*expiration_ms==0)
			{
				proc_id=ZTE_PROC_COMM_CMD3_RTC_ALARM_DISABLE;
			msm_proc_comm(PCOM_CUSTOMER_CMD3, expiration_ms, &proc_id);
			}
		 else
		 {
			proc_id=ZTE_PROC_COMM_CMD3_RTC_ALARM_ENABLE;
			msm_proc_comm(PCOM_CUSTOMER_CMD3, expiration_ms, &proc_id);
		 }
		
		return count;
	
	}
#endif

/*
 * Initialize the power management subsystem.
 *
 * Return value:
 *      -ENODEV: initialization failed
 *      0: success
 */
static int __init msm_pm_init(void)
{
	int ret;
	int val;
	enum msm_pm_time_stats_id enable_stats[] = {
		MSM_PM_STAT_REQUESTED_IDLE,
		MSM_PM_STAT_IDLE_SPIN,
		MSM_PM_STAT_IDLE_WFI,
		MSM_PM_STAT_IDLE_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_FAILED_STANDALONE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_POWER_COLLAPSE,
		MSM_PM_STAT_IDLE_FAILED_POWER_COLLAPSE,
		MSM_PM_STAT_SUSPEND,
		MSM_PM_STAT_FAILED_SUSPEND,
		MSM_PM_STAT_NOT_IDLE,
	};

#ifdef CONFIG_CPU_V7
	pgd_t *pc_pgd;
	pmd_t *pmd;
	unsigned long pmdval;
	unsigned long exit_phys;

	exit_phys = virt_to_phys(msm_pm_collapse_exit);

	/* Page table for cores to come back up safely. */
	pc_pgd = pgd_alloc(&init_mm);
	if (!pc_pgd)
		return -ENOMEM;
	pmd = pmd_offset(pud_offset(pc_pgd + pgd_index(exit_phys), exit_phys),
			 exit_phys);
	pmdval = (exit_phys & PGDIR_MASK) |
		     PMD_TYPE_SECT | PMD_SECT_AP_WRITE;
	pmd[0] = __pmd(pmdval);
	pmd[1] = __pmd(pmdval + (1 << (PGDIR_SHIFT - 1)));

	msm_saved_state_phys =
		allocate_contiguous_ebi_nomap(CPU_SAVED_STATE_SIZE *
					      num_possible_cpus(), 4);
	if (!msm_saved_state_phys)
		return -ENOMEM;
	msm_saved_state = ioremap_nocache(msm_saved_state_phys,
					  CPU_SAVED_STATE_SIZE *
					  num_possible_cpus());
	if (!msm_saved_state)
		return -ENOMEM;

	/* It is remotely possible that the code in msm_pm_collapse_exit()
	 * which turns on the MMU with this mapping is in the
	 * next even-numbered megabyte beyond the
	 * start of msm_pm_collapse_exit().
	 * Map this megabyte in as well.
	 */
	pmd[2] = __pmd(pmdval + (2 << (PGDIR_SHIFT - 1)));
	flush_pmd_entry(pmd);
	msm_pm_pc_pgd = virt_to_phys(pc_pgd);
	clean_caches((unsigned long)&msm_pm_pc_pgd, sizeof(msm_pm_pc_pgd),
		     virt_to_phys(&msm_pm_pc_pgd));
#endif

	msm_pm_smem_data = smem_alloc(SMEM_APPS_DEM_SLAVE_DATA,
		sizeof(*msm_pm_smem_data));
	if (msm_pm_smem_data == NULL) {
		printk(KERN_ERR "%s: failed to get smsm_data\n", __func__);
		return -ENODEV;
	}

	ret = msm_timer_init_time_sync(msm_pm_timeout);
	if (ret)
		return ret;

	ret = smsm_change_intr_mask(SMSM_POWER_MASTER_DEM, 0xFFFFFFFF, 0);
	if (ret) {
		printk(KERN_ERR "%s: failed to clear interrupt mask, %d\n",
			__func__, ret);
		return ret;
	}

	if (cpu_is_msm8625()) {
		target_type = TARGET_IS_8625;
		clean_caches((unsigned long)&target_type, sizeof(target_type),
				virt_to_phys(&target_type));

		/*
		 * Configure the MPA5_GDFS_CNT_VAL register for
		 * DBGPWRUPEREQ_OVERRIDE[17:16] = Override the
		 * DBGNOPOWERDN for each cpu.
		 * MPA5_GDFS_CNT_VAL[9:0] = Delay counter for
		 * GDFS control.
		 */
		val = 0x00030002;
		__raw_writel(val, (MSM_CFG_CTL_BASE + 0x38));

		l2x0_base_addr = MSM_L2CC_BASE;
	}

#ifdef CONFIG_MSM_MEMORY_LOW_POWER_MODE
	/* The wakeup_reason field is overloaded during initialization time
	   to signal Modem that Apps will control the low power modes of
	   the memory.
	 */
	msm_pm_smem_data->wakeup_reason = 1;
	smsm_change_state(SMSM_APPS_DEM, 0, DEM_SLAVE_SMSM_RUN);
#endif

	BUG_ON(msm_pm_modes == NULL);

	suspend_set_ops(&msm_pm_ops);

	msm_pm_mode_sysfs_add();
	msm_pm_add_stats(enable_stats, ARRAY_SIZE(enable_stats));

	atomic_set(&msm_pm_init_done, 1);

#ifdef CONFIG_ZTE_ALARM
	{
		   uint32_t *new_data;
		   struct proc_dir_entry *d_entry2;

		if ((new_data = kmalloc(sizeof(uint32_t), GFP_KERNEL)) == NULL)
			{
			 printk(KERN_ERR "%s: failed to create_proc_entry of zte_alarm\n",
			__func__);
				return -ENOMEM;
			}
		*new_data  =0;
			printk(KERN_ERR "%s: shuangdan_begin to create_proc_entry of zte_alarm\n",
			__func__); 
		   d_entry2 = create_proc_entry("zte_alarm",
				S_IRUGO | S_IWUGO, NULL);
		   
		if (d_entry2) {
			d_entry2->read_proc = zte_alarm_read_proc;
			d_entry2->write_proc = zte_alarm_write_proc;
			d_entry2->data = (void *)new_data;
		}
		printk(KERN_ERR "%s: shuangdan_end to create_proc_entry of zte_alarm\n",
			__func__); 
	}	
#endif
	return 0;
}

late_initcall_sync(msm_pm_init);
