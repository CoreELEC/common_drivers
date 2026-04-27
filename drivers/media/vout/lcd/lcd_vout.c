// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_device.h>
#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "lcd_reg.h"
#include "lcd_common.h"
#include "./connectors/lcd_connector.h"
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>

#define LCD_CDEV_NAME  "lcd"

unsigned int lcd_debug_print_flag;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char lcd_global_init_flag;
static unsigned int lcd_drv_init_state;
static struct aml_lcd_drv_s *lcd_driver[LCD_MAX_DRV];
static struct workqueue_struct *lcd_workqueue;

/* 1: unlocked, 0: locked, negative: locked, possible waiters */
struct mutex lcd_vout_mutex;
/* 1: unlocked, 0: locked, negative: locked, possible waiters */
struct mutex lcd_power_mutex;
EXPORT_SYMBOL(lcd_power_mutex);

int lcd_vout_serve_bypass;
static void lcd_power_if_off(struct aml_lcd_drv_s *pdrv);
static void lcd_set_encl_dummy(struct aml_lcd_drv_s *pdrv);

struct lcd_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct lcd_cdev_s *lcd_cdev;
static char lcd_propname[LCD_MAX_DRV][24] = {"mipi_0", "null", "null"};
// static char lcd_panel_name[LCD_MAX_DRV][24] = {"null", "null", "null"};

#define LCD_VSYNC_NONE_INTERVAL     msecs_to_jiffies(500)
/* *********************************************************
 * lcd config define
 * *********************************************************
 */
static struct lcd_boot_ctrl_s lcd_boot_ctrl_config[LCD_MAX_DRV] = {
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
		.ppc = 1,
	},
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
		.ppc = 1,
	},
	{
		.lcd_type = LCD_TYPE_MAX,
		.lcd_bits = 0,
		.advanced_flag = 0,
		.init_level = 0,
		.ppc = 1,
	}
};

static struct lcd_debug_ctrl_s lcd_debug_ctrl_config = {
	.debug_print_flag = 0,
	.debug_test_pattern = 0,
	.debug_para_source = 0,
	.debug_lcd_mode = 0,
};

int lcd_get_dbg_source(void)
{
	return lcd_debug_ctrl_config.debug_para_source;
}

static struct aml_lcd_drv_s *lcd_driver_add(int index)
{
	struct aml_lcd_drv_s *pdrv = NULL;

	if (index >= LCD_MAX_DRV)
		return NULL;

	pdrv = kzalloc(sizeof(*pdrv), GFP_KERNEL);
	if (!pdrv)
		return NULL;

	pdrv->index = index;

	/* default vinfo */
	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.frac = 0;
	pdrv->vinfo.viu_color_fmt = COLOR_FMT_RGB444;
	pdrv->vinfo.viu_mux = ((index << 4) | VIU_MUX_ENCL);
	pdrv->vinfo.vout_device = NULL;

	/* boot ctrl */
	pdrv->boot_ctrl = &lcd_boot_ctrl_config[index];
	pdrv->debug_ctrl = &lcd_debug_ctrl_config;

	return pdrv;
}

struct aml_lcd_drv_s *aml_lcd_get_driver(int index)
{
	if (index >= LCD_MAX_DRV)
		return NULL;

	return lcd_driver[index];
}
EXPORT_SYMBOL(aml_lcd_get_driver);

static void lcd_bootargs_debug_load(void)
{
	struct lcd_debug_ctrl_s *debug_ctrl = &lcd_debug_ctrl_config;
	unsigned char *p;
	int size;

	p = lrm_bootargs_get_data("lcd_debug", &size);
	if (p) {
		if (size != sizeof(struct lcd_debug_ctrl_s))
			return;

		memcpy((void *)debug_ctrl, p, size);
	}
}

static void lcd_bootargs_ctrl_load(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_boot_ctrl_s *boot_ctrl;
	char arg_name[16], *str;
	unsigned char *p;
	int size = 0;
	char *token;

	if (pdrv->index >= LCD_MAX_DRV)
		return;

	//sprintf(arg_name, "panel%d_name", pdrv->index);
	//p = lrm_bootargs_get_data(arg_name, &size);
	//if (p) {
	//	str = (char *)p;
	//	sprintf(lcd_panel_name[pdrv->index], "%s", str);
	//	if (lcd_debug_print_flag & LCD_DBG_PR_MEM)
	//		LCDPR("%s: %s\n", arg_name, lcd_panel_name[pdrv->index]);
	//}

	sprintf(arg_name, "lcd%d_dev_sel", pdrv->index);
	p = lrm_bootargs_get_data(arg_name, &size);
	if (p) {
		str = (char *)p;
		if (kstrtou8(str, 10, &pdrv->device_mgr.uboot_dev_idx))
			pdrv->device_mgr.uboot_dev_idx = 0xff;
	}

	sprintf(arg_name, "lcd%d_devs", pdrv->index);
	p = lrm_bootargs_get_data(arg_name, &size);
	if (p) {
		str = (char *)p;
		size = 0;
		while ((token = strsep(&str, ",")) != NULL && size < LCD_MULTI_DEV_MAX_CNT) {
			strscpy(pdrv->device_mgr.uboot_pre_devs[size], token, 15);
			// Ensure null-termination
			// uboot_pre_devs[i][sizeof(uboot_pre_devs[i]) - 1] = '\0';

			size++; // Move to the next index in our array

			// Get the next token
			// token = strsep(token , delimiter);
		}

		if (lcd_debug_print_flag & LCD_DBG_PR_MEM) {
			LCD_PR(pdrv, "load multi-device: using[%d] ([%s],[%s],[%s],[%s])",
				pdrv->device_mgr.uboot_dev_idx,
				pdrv->device_mgr.uboot_pre_devs[0],
				pdrv->device_mgr.uboot_pre_devs[1],
				pdrv->device_mgr.uboot_pre_devs[2],
				pdrv->device_mgr.uboot_pre_devs[3]);
		}
	}

	sprintf(arg_name, "panel%d_type", pdrv->index);
	p = lrm_bootargs_get_data(arg_name, &size);
	if (p) {
		str = (char *)p;
		sprintf(lcd_propname[pdrv->index], "%s", str);

		if (pdrv->device_mgr.uboot_dev_idx == 0xff) {
			pdrv->device_mgr.uboot_dev_idx = 0;
			sprintf(pdrv->device_mgr.uboot_pre_devs[0], "%s", str);
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_MEM)
			LCD_PR(pdrv, "%s: %s", arg_name, lcd_propname[pdrv->index]);
	}

	sprintf(arg_name, "lcd%d_attr", pdrv->index);
	p = lrm_bootargs_get_data(arg_name, &size);
	if (p) {
		if (size != sizeof(struct lcd_boot_ctrl_s))
			return;
		boot_ctrl = &lcd_boot_ctrl_config[pdrv->index];
		memcpy((void *)boot_ctrl, p, size);

		if (lcd_debug_print_flag & LCD_DBG_PR_MEM)
			LCD_PR(pdrv, "%s: %s", __func__, arg_name);
	}
}

static struct lcd_resource_s *lcd_resource_new(unsigned int res_type, unsigned int res_index)
{
	struct lcd_resource_s *pres;

	pres = kzalloc(sizeof(*pres), GFP_KERNEL);
	if (!pres)
		return NULL;
	pres->type = res_type;
	pres->index = res_index;

	return pres;
}

void lcd_resource_free(struct aml_lcd_drv_s *pdrv, unsigned int res_type, unsigned int res_index)
{
	struct lcd_resource_s *res_next, *res;

	if (!pdrv || !pdrv->resource)
		return;

	res = pdrv->resource;
	if (res->type == res_type && res->index == res_index) {
		pdrv->resource = res->next_res;
		kfree(res);
		return;
	}

	while (res->next_res) {
		if (res->next_res->type == res_type && res->next_res->index == res_index) {
			res_next = res->next_res->next_res;
			kfree(res->next_res);
			res->next_res = res_next;
			break;
		}
	}
}

void lcd_resource_add(struct aml_lcd_drv_s *pdrv, unsigned int res_type, unsigned int res_index)
{
	struct lcd_resource_s *res_i, *res_tail, *pres;

	if (!pdrv)
		return;

	mutex_lock(&lcd_power_mutex);
	pres = lcd_resource_new(res_type, res_index);
	if (!pres) {
		LCD_PR(pdrv, "%s: res_type[idx]: %d[%d]", __func__, res_type, res_index);
		mutex_unlock(&lcd_power_mutex);
		return;
	}

	if (!pdrv->resource) {
		pdrv->resource = pres;
		goto lcd_resource_add_success;
	}

	res_i = pdrv->resource;
	while (res_i) {
		if (res_i->type == res_type && res_i->index == res_index) {
			LCD_DBG(pdrv, "%s: already exist, res_type[idx]: %d[%d], rdy %d",
				__func__, res_type, res_index, res_i->ready);
			kfree(pres);
			mutex_unlock(&lcd_power_mutex);
			return;
		}
		res_tail = res_i;
		res_i = res_i->next_res;
	}
	res_tail->next_res = pres;

lcd_resource_add_success:
	LCD_DBG(pdrv, "%s: done: res_type[idx]: %d[%d]",  __func__, res_type, res_index);

	mutex_unlock(&lcd_power_mutex);
}

void lcd_resource_ready(int drv_index, unsigned int res_type, unsigned int res_index)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(drv_index);
	struct lcd_resource_s *res_i, *res_tail = NULL, *pres;

	if (!pdrv)
		return;

	mutex_lock(&lcd_power_mutex);
	res_i = pdrv->resource;
	while (res_i) {
		if (res_i->type == res_type && res_i->index == res_index) {
			res_i->ready = 1;
			goto lcd_resource_ready_success;
		}
		res_tail = res_i;
		res_i = res_i->next_res;
	}

	//when ahead of lcd_resource_add with lcd config probe
	pres = lcd_resource_new(res_type, res_index);
	if (!pres) {
		LCD_PR(pdrv, " %s: res_type[idx]: %d[%d]", __func__, res_type, res_index);
		mutex_unlock(&lcd_power_mutex);
		return;
	}
	pres->ready = 1;
	if (res_tail)
		res_tail->next_res = pres;
	else
		pdrv->resource = pres;

lcd_resource_ready_success:
	LCD_DBG(pdrv, "%s: done: res_type[idx]: %d[%d]", __func__, res_type, res_index);

	mutex_unlock(&lcd_power_mutex);
}

static int lcd_resource_status_check(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_resource_s *res_i;

	if (!pdrv)
		return 0;

	res_i = pdrv->resource;
	while (res_i) {
		if (res_i->ready == 0) {
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
				LCD_ERR(pdrv, "%s: not ready: res_type[index]: %d[%d]",
					__func__, res_i->type, res_i->index);
			}
			return 0;
		}
		res_i = res_i->next_res;
	}

	return 1;
}

int lcd_resource_is_ready(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	if (!pdrv)
		return 0;

	mutex_lock(&lcd_power_mutex);
	ret = lcd_resource_status_check(pdrv);
	mutex_unlock(&lcd_power_mutex);
	return ret;
}

void lcd_resource_remove_all(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_resource_s *res, *res_next;

	if (!pdrv || !pdrv->resource)
		return;
	res = pdrv->resource;
	while (res) {
		res_next = res->next_res;
		kfree(res);
		res = res_next;
	}
	pdrv->resource = NULL;
}

int get_lcd_config_load_ready(unsigned char index)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = aml_lcd_get_driver(index);
	if (pdrv)
		return pdrv->probe_done;

	return 0;
}

unsigned char get_vout_lcd_mode(unsigned char vout_index)
{
	unsigned char i;
	struct aml_lcd_drv_s *pdrv;

	for (i = 0; i < 3; i++) {
		pdrv = aml_lcd_get_driver(i);
		if (pdrv && pdrv->viu_sel == vout_index)
			return pdrv->mode;
	}
	return LCD_MODE_MAX;
}
EXPORT_SYMBOL(get_vout_lcd_mode);

inline void lcd_queue_work(struct work_struct *work)
{
	if (lcd_workqueue)
		queue_work(lcd_workqueue, work);
	else
		schedule_work(work);
}

inline void lcd_queue_delayed_work(struct delayed_work *dwork, int ms)
{
	if (lcd_workqueue)
		queue_delayed_work(lcd_workqueue, dwork, msecs_to_jiffies(ms));
	else
		schedule_delayed_work(dwork, msecs_to_jiffies(ms));
}

void lcd_proc_time_clear(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return;

	memset(&pdrv->proc_time, 0, sizeof(struct lcd_time_s));
}

/* ********************************************************* */
static void lcd_power_ctrl(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
#endif
	unsigned long long local_time[2];
	unsigned int i = 0, index, wait, delay;
	int max_step, value = -1;

	if (pdrv->lcd_pxp) {
		LCD_PR(pdrv, "%s: lcd_pxp bypass", __func__);
		return;
	}

	if (status) {
		power_step = pdrv->curr_dev->dev_cfg.power.power_on_step;
		max_step = pdrv->curr_dev->dev_cfg.power.power_on_step_max;
		//power off delay
		if (pdrv->curr_dev->dev_cfg.power.power_off_delay &&
		    pdrv->curr_dev->dev_cfg.power.off_time) {
			local_time[0] = sched_clock();
			local_time[1] = local_time[0] - pdrv->curr_dev->dev_cfg.power.off_time;
			delay = lcd_do_div(local_time[1], 1000000);
			if (delay < pdrv->curr_dev->dev_cfg.power.power_off_delay) {
				delay = pdrv->curr_dev->dev_cfg.power.power_off_delay - delay;
				LCD_PR(pdrv, "%s: adapt power off delay: %d", __func__, delay);
				lcd_delay_ms(delay);
			}
		}
	} else {
		power_step = pdrv->curr_dev->dev_cfg.power.power_off_step;
		max_step = pdrv->curr_dev->dev_cfg.power.power_off_step_max;
	}
	LCD_PR(pdrv, "%s: %d", __func__, status);

	while (i < max_step) {
		if (power_step[i].type >= LCD_POWER_TYPE_MAX)
			break;
		LCD_DBG(pdrv, "%s: step %d, type=%d, index=%d, value=%d, delay=%d", __func__, i,
			power_step[i].type, power_step[i].index,
			power_step[i].value, power_step[i].delay);
		delay = power_step[i].delay;
		switch (power_step[i].type) {
		case LCD_POWER_TYPE_GPIO:
			index = power_step[i].index;
			lcd_cpu_gpio_set(pdrv, index, power_step[i].value);
			break;
		case LCD_POWER_TYPE_SIGNAL:
			local_time[0] = sched_clock();
			if (status) {
				lcd_connector_driver_init(pdrv);
				local_time[1] = sched_clock();
				pdrv->proc_time.signal_on_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			} else {
				lcd_connector_driver_disable(pdrv);
				local_time[1] = sched_clock();
				pdrv->proc_time.signal_off_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			}
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			index = power_step[i].index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (status) {
				local_time[0] = sched_clock();
				if (edev->power_on)
					edev->power_on(edrv, edev);
				else
					LCD_ERR(pdrv, "no ext_%d power on", index);
				local_time[1] = sched_clock();
				pdrv->proc_time.extern_init_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
				else
					LCD_ERR(pdrv, "no ext_%d power off", index);
			}
			if (edrv->exit_break)
				return;
			break;
#endif
		case LCD_POWER_TYPE_WAIT_GPIO:
			index = power_step[i].index;
			delay = 0;
			lcd_cpu_gpio_set(pdrv, index, LCD_GPIO_INPUT);
			LCD_PR(pdrv, "lcd_power_type_wait_gpio wait");
			for (wait = 0; wait < power_step[i].delay; wait++) {
				value = lcd_cpu_gpio_get(pdrv, index);
				if (value == power_step[i].value) {
					LCD_PR(pdrv, "wait_gpio %d ok", value);
					break;
				}
				mdelay(1);
			}
			if (wait == power_step[i].delay) {
				LCD_ERR(pdrv, "wait_gpio %d timeout!", value);
			}
			break;
		case LCD_POWER_TYPE_CLK_SS:
			delay = 0;
			break;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		case LCD_POWER_TYPE_BACKLIGHT:
			if (status)
				bl_lcd_on_ctrl(pdrv);
			else
				bl_lcd_off_ctrl(pdrv);
			break;
#endif
		case LCD_POWER_TYPE_MUTE:
			if (power_step[i].value)
				lcd_power_screen_black(pdrv);
			else
				lcd_power_screen_restore(pdrv);
			break;
		case LCD_POWER_TYPE_OFF_DELAY:
			pdrv->curr_dev->dev_cfg.power.off_time = sched_clock();
			delay = 0;
			break;
		default:
			break;
		}
		if (delay)
			lcd_delay_ms(delay);
		i++;
	}

	LCD_DBG(pdrv, "%s: %d finished", __func__, status);
}

static void lcd_mode_switch_power_ctrl(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
	unsigned int index;
#endif
	unsigned long long local_time[2];
	int max_step, i = 0;

	LCD_PR(pdrv, "%s: %d", __func__, status);
	if (status) {
		power_step = pdrv->curr_dev->dev_cfg.power.power_on_step;
		max_step = pdrv->curr_dev->dev_cfg.power.power_on_step_max;
	} else {
		power_step = pdrv->curr_dev->dev_cfg.power.power_off_step;
		max_step = pdrv->curr_dev->dev_cfg.power.power_off_step_max;
	}
	while (i < LCD_PWR_STEP_MAX) {
		if (power_step[i].type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step[i].type) {
		case LCD_POWER_TYPE_SIGNAL:
			LCD_DBG(pdrv, "power:%s, step%d, type=%d, idx=%d, val=%d, dly=%d",
				status ? "on" : "off", i,
				power_step[i].type, power_step[i].index,
				power_step[i].value, power_step[i].delay);
			local_time[0] = sched_clock();
			if (status) {
				lcd_connector_driver_init(pdrv);
				local_time[1] = sched_clock();
				pdrv->proc_time.signal_on_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			} else {
				lcd_connector_driver_disable(pdrv);
				local_time[1] = sched_clock();
				pdrv->proc_time.signal_off_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			}
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			LCD_DBG(pdrv, "power:%s, step%d, type=%d, idx=%d, val=%d, dly=%d",
				status ? "on" : "off", i,
				power_step[i].type, power_step[i].index,
				power_step[i].value, power_step[i].delay);
			index = power_step[i].index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (status) {
				local_time[0] = sched_clock();
				if (edev->power_on)
					edev->power_on(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power on\n", pdrv->index, index);
				local_time[1] = sched_clock();
				pdrv->proc_time.extern_init_time =
					lcd_do_div(local_time[1] - local_time[0], 1000);
			} else {
				if (edev->power_off)
					edev->power_off(edrv, edev);
				else
					LCDERR("[%d]: no ext_%d power off\n", pdrv->index, index);
			}
			if (edrv->exit_break)
				return;
			break;
#endif
#ifdef CONFIG_AMLOGIC_BACKLIGHT

		case LCD_POWER_TYPE_BACKLIGHT:
			if (status)
				bl_lcd_on_ctrl(pdrv);
			else
				bl_lcd_off_ctrl(pdrv);
			break;
#endif
		case LCD_POWER_TYPE_MUTE:
			if (power_step[i].value)
				lcd_power_screen_black(pdrv);
			else
				lcd_power_screen_restore(pdrv);
			break;
		default:
			break;
		}
		i++;
	}

	LCD_DBG(pdrv, "%s: %d finished", __func__, status);
}

static void lcd_mode_switch_data_on(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_power_step_s *power_step;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	struct lcd_extern_driver_s *edrv;
	struct lcd_extern_dev_s *edev;
	unsigned int index;
	unsigned long long local_time[2];
#endif
	int max_step, i = 0;

	LCD_DBG(pdrv, "%s", __func__);

	power_step = pdrv->curr_dev->dev_cfg.power.power_on_step;
	max_step = pdrv->curr_dev->dev_cfg.power.power_on_step_max;
	while (i < max_step) {
		if (power_step[i].type >= LCD_POWER_TYPE_MAX)
			break;
		switch (power_step[i].type) {
		case LCD_POWER_TYPE_SIGNAL:
			LCD_DBG(pdrv, "switch on, step%d, type=%d, idx=%d, val=%d, dly=%d", i,
				power_step[i].type, power_step[i].index,
				power_step[i].value, power_step[i].delay);
			LCD_DBG(pdrv, "mode switch delay 20ms");
			lcd_delay_ms(20);
			// phy param maybe change update phy register
			lcd_phy_set(pdrv, LCD_PHY_UPDATE);
#ifdef CONFIG_AMLOGIC_LCD_TCON
			if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_P2P)
				lcd_tcon_reload(pdrv);
#endif
			break;
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
		case LCD_POWER_TYPE_EXTERN:
			LCD_DBG(pdrv, "witch on, step%d, type=%d, idx=%d, val=%d, dly=%d", i,
				power_step[i].type, power_step[i].index,
				power_step[i].value, power_step[i].delay);
			local_time[0] = sched_clock();
			index = power_step[i].index;
			edrv = lcd_extern_get_driver(pdrv->index);
			edev = lcd_extern_get_dev(edrv, index);
			if (!edrv || !edev)
				break;
			if (edev->power_on)
				edev->power_on(edrv, edev);
			else
				LCDERR("[%d]: no ext_%d power on\n", pdrv->index, index);
			local_time[1] = sched_clock();
			pdrv->proc_time.extern_init_time =
				lcd_do_div(local_time[1] - local_time[0], 1000);
			if (edrv->exit_break)
				return;
			break;
#endif
		case LCD_POWER_TYPE_MUTE:
			lcd_power_screen_restore(pdrv);
			break;
		default:
			break;
		}
		i++;
	}
}

static void lcd_mode_switch_data_off(struct aml_lcd_drv_s *pdrv)
{
	LCD_DBG(pdrv, "%s", __func__);

	lcd_power_screen_black(pdrv);

#ifdef CONFIG_AMLOGIC_LCD_TCON
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_P2P)
		lcd_tcon_reload_pre(pdrv);
#endif
}

static void lcd_power_encl_on(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	mutex_lock(&lcd_vout_mutex);
	if (pdrv->config_check_en == 0) {
		LCD_DBG(pdrv, "config_check disabled");
	} else {
		ret = lcd_config_timing_check(pdrv, pdrv->curr_dev,
			&pdrv->curr_dev->dev_cfg.timing.act_timing);
		if (ret & 0x55) {
			LCD_ERR(pdrv, "%s: config timing check fatal error!", __func__);
			mutex_unlock(&lcd_vout_mutex);
			return;
		}
	}

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		LCD_PR(pdrv, "%s: on already", __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}

	lcd_connector_driver_init_pre(pdrv);
	pdrv->status |= LCD_STATUS_ENCL_ON;
	mutex_unlock(&lcd_vout_mutex);

	/* vsync_none_timer conditional enabled to save cpu loading */
	if (pdrv->viu_sel == LCD_VIU_SEL_NONE) {
		if (pdrv->vsync_none_timer_flag == 0) {
			pdrv->vs_none_timer.expires = jiffies + LCD_VSYNC_NONE_INTERVAL;
			add_timer(&pdrv->vs_none_timer);
			pdrv->vsync_none_timer_flag = 1;
			LCD_PR(pdrv, "add vs_none_timer handler");
		}
	} else {
		if (pdrv->vsync_none_timer_flag) {
			del_timer_sync(&pdrv->vs_none_timer);
			pdrv->vsync_none_timer_flag = 0;
		}
	}
}

static void lcd_power_encl_off(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCD_PR(pdrv, "%s: force power off interface ahead", __func__);
		pdrv->status &= ~LCD_STATE_POWER;
		lcd_power_if_off(pdrv);
		if (pdrv->status & LCD_STATE_DUMMY)
			lcd_set_encl_dummy(pdrv);
	}

	mutex_lock(&lcd_vout_mutex);
	if (!(pdrv->status & LCD_STATUS_ENCL_ON)) {
		LCD_PR(pdrv, "%s: off already",  __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}
	pdrv->status &= ~LCD_STATUS_ENCL_ON;

	lcd_connector_driver_disable_post(pdrv);
	mutex_unlock(&lcd_vout_mutex);

	if (pdrv->vsync_none_timer_flag) {
		del_timer_sync(&pdrv->vs_none_timer);
		pdrv->vsync_none_timer_flag = 0;
	}
}

static int lcd_driver_active(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_clk_set_dummy(pdrv, 0))
		return -1;
	lcd_set_venc(pdrv);

	return 0;
}

static int lcd_driver_dummy(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_clk_set_dummy(pdrv, 1))
		return -1;
	lcd_venc_set_dummy(pdrv);

	return 0;
}

static void lcd_set_encl_active(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	mutex_lock(&lcd_vout_mutex);
	if (!(pdrv->status & LCD_STATUS_ENCL_ON)) {
		LCD_PR(pdrv, "%s: failed when encl is off", __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}
	if (!(pdrv->status & LCD_STATUS_ENCL_DUMMY)) {
		LCD_PR(pdrv, "%s: active already", __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}
	ret = lcd_driver_active(pdrv);
	if (!ret)
		pdrv->status &= ~LCD_STATUS_ENCL_DUMMY;
	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_set_encl_dummy(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	mutex_lock(&lcd_vout_mutex);
	if (!(pdrv->status & LCD_STATUS_ENCL_ON)) {
		LCD_PR(pdrv, "%s: failed when encl is off", __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}
	if (pdrv->status & LCD_STATUS_ENCL_DUMMY) {
		LCD_PR(pdrv, "%s: dummy already", __func__);
		mutex_unlock(&lcd_vout_mutex);
		return;
	}

	ret = lcd_driver_dummy(pdrv);
	if (!ret)
		pdrv->status |= LCD_STATUS_ENCL_DUMMY;
	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_mode_switch_power_on(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCD_PR(pdrv, "interface on already");
		return;
	}

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCDERR("[%d]: %s: failed when encl is off\n", pdrv->index, __func__);
		return;
	}

	lcd_mode_switch_power_ctrl(pdrv, 1);
	pdrv->status |= LCD_STATUS_IF_ON;
	pdrv->curr_dev->dev_cfg.change_flag = 0;
}

static void lcd_mode_switch_power_off(struct aml_lcd_drv_s *pdrv)
{
	if (!(pdrv->status & LCD_STATUS_IF_ON)) {
		LCD_PR(pdrv, "interface off already");
		return;
	}

	pdrv->status &= ~LCD_STATUS_IF_ON;
	lcd_mode_switch_power_ctrl(pdrv, 0);
}

static void lcd_power_if_on(struct aml_lcd_drv_s *pdrv)
{
	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCD_PR(pdrv, "%s: force power on encl ahead", __func__);
		lcd_power_encl_on(pdrv);
		if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
			LCD_ERR(pdrv, "%s: encl_on failed!", __func__);
			return;
		}
	}
	if (pdrv->status & LCD_STATUS_ENCL_DUMMY) {
		LCD_ERR(pdrv, "%s: exit for dummy state", __func__);
		return;
	}

	if ((pdrv->status & LCD_STATUS_IF_ON)) {
		LCD_PR(pdrv, "interface on already");
		return;
	}

	lcd_power_ctrl(pdrv, 1);
	pdrv->status |= LCD_STATUS_IF_ON;
	pdrv->curr_dev->dev_cfg.change_flag = 0;
}

static void lcd_power_if_off(struct aml_lcd_drv_s *pdrv)
{
	if (!(pdrv->status & LCD_STATUS_IF_ON)) {
		LCD_PR(pdrv, "interface off already");
		return;
	}

	pdrv->status &= ~LCD_STATUS_IF_ON;
	lcd_power_ctrl(pdrv, 0);
}

void lcd_power_if_early_on(struct aml_lcd_drv_s *pdrv)
{
	if ((pdrv->resume_type & (1 << 1)) == 0 ||
		!is_screenon_resume_method())
		return;
	LCD_PR(pdrv, "%s resume_type=%u", __func__, pdrv->resume_type);

	pdrv->status |= LCD_STATE_BL_PRE_ON;
	if ((pdrv->resume_type & (1 << 0))) {
		lcd_queue_work(&pdrv->late_resume_work);
	} else {
		LCD_PR(pdrv, "directly if_early_on, status=0x%x", pdrv->status);
		/*if resource is not ready, call lcd_queue_work instead*/
		if (lcd_resource_status_check(pdrv)) {
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON | LCD_EVENT_ENCL_ACTIVE,
					(void *)pdrv);
			lcd_if_enable_retry(pdrv);
			pdrv->status |= LCD_STATE_POWER;
			pdrv->status &= ~LCD_STATE_DUMMY;
		} else {
			lcd_queue_work(&pdrv->late_resume_work);
		}
	}
}

void lcd_power_screen_black(struct aml_lcd_drv_s *pdrv)
{
	unsigned long flags = 0;
	int ret;
	unsigned long long local_time[2];

	if (!pdrv || pdrv->probe_done == 0)
		return;

	local_time[0] = sched_clock();
	reinit_completion(&pdrv->vsync_done);
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->mute_flag = 1;
	if (pdrv->mute_cnt_test)
		pdrv->mute_wait_cnt = pdrv->mute_cnt_test;
	else
		pdrv->mute_wait_cnt = pdrv->mute_cnt;
	pdrv->mute_switch = 1;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);
	if (pdrv->enter_shutdown == 0) {
		LCD_PR(pdrv, "%s: mute_wait_cnt: %d, mute_cnt: %d",
			__func__, pdrv->mute_wait_cnt, pdrv->mute_cnt);
		//wait for mute apply
		ret = wait_for_completion_timeout(&pdrv->vsync_done, msecs_to_jiffies(500));
		if (!ret)
			LCD_ERR(pdrv, "%s: wait_completion timeout", __func__);
	}
	local_time[1] = sched_clock();
	pdrv->proc_time.mute_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

void lcd_power_screen_restore(struct aml_lcd_drv_s *pdrv)
{
	unsigned long flags = 0;
	int ret;
	unsigned long long local_time[2];

	if (!pdrv || pdrv->probe_done == 0)
		return;
	if ((pdrv->status & LCD_STATE_BL_PRE_ON))
		return;

	local_time[0] = sched_clock();
	reinit_completion(&pdrv->vsync_done);
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	if (pdrv->unmute_cnt_test)
		pdrv->mute_wait_cnt = pdrv->unmute_cnt_test;
	else
		pdrv->mute_wait_cnt = pdrv->unmute_cnt + pdrv->unmute_cnt_added;
	/*unmute_cnt_added take effect only once, auto clean here*/
	pdrv->unmute_cnt_added = 0;
	pdrv->mute_switch = 1;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);
	LCD_PR(pdrv, "%s: unmute_wait_cnt: %d, unmute_cnt: %d",
		__func__, pdrv->mute_wait_cnt, pdrv->unmute_cnt);
	ret = wait_for_completion_timeout(&pdrv->vsync_done, msecs_to_jiffies(5000));
	if (!ret)
		LCD_ERR(pdrv, "%s: wait_completion timeout", __func__);
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->mute_flag = 0;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);
	local_time[1] = sched_clock();
	pdrv->proc_time.unmute_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

void lcd_module_reset(struct aml_lcd_drv_s *pdrv)
{

	pdrv->status &= ~LCD_STATUS_IF_ON;
	lcd_power_ctrl(pdrv, 0);

	lcd_delay_ms(500);

	mutex_lock(&lcd_vout_mutex);
	pdrv->status &= ~LCD_STATUS_ENCL_ON;
	lcd_connector_driver_init_pre(pdrv);
	pdrv->status |= LCD_STATUS_ENCL_ON;
	mutex_unlock(&lcd_vout_mutex);

	lcd_power_ctrl(pdrv, 1);
	pdrv->status |= LCD_STATUS_IF_ON;
	pdrv->curr_dev->dev_cfg.change_flag = 0;

}

static void lcd_mode_switch_on_work(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;
	unsigned long long local_time[2];
	unsigned long flags = 0;

	local_time[0] = sched_clock();
	pdrv = container_of(work, struct aml_lcd_drv_s, mode_switch_on_work);

	mutex_lock(&lcd_power_mutex);
	if (pdrv->status & LCD_STATE_POWER) {
		aml_lcd_notifier_call_chain(pdrv->switch_on_event, (void *)pdrv);
		if ((pdrv->switch_on_event & LCD_EVENT_MDSW_POWER_ON) ||
		    (pdrv->switch_on_event & LCD_EVENT_POWER_ON))
			lcd_if_enable_retry(pdrv);
	}
	mutex_unlock(&lcd_power_mutex);

	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->test_locked = 0;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	local_time[1] = sched_clock();
	pdrv->proc_time.switch_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);
	pdrv->proc_time.switch_full_time =
		lcd_do_div(local_time[1] - pdrv->proc_time.switch_start_time, 1000);
}

void lcd_late_resume(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return;

	if (pdrv->status & LCD_STATE_POWER) {
		if (pdrv->status & LCD_STATE_BL_PRE_ON) {
			pdrv->status &= ~LCD_STATE_BL_PRE_ON;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
			bl_lcd_on_ctrl(pdrv);
#endif
			lcd_power_screen_restore(pdrv);
		}
		return;
	}

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON | LCD_EVENT_ENCL_ACTIVE, (void *)pdrv);
	lcd_if_enable_retry(pdrv);
	pdrv->status |= LCD_STATE_POWER;
	pdrv->status &= ~LCD_STATE_DUMMY;
	LCD_PR(pdrv, "%s finished, status=0x%x", __func__, pdrv->status);
	mutex_unlock(&lcd_power_mutex);
}

static void lcd_late_resume_work(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;
	int res_ready;

	pdrv = container_of(work, struct aml_lcd_drv_s, late_resume_work);
	res_ready = lcd_resource_is_ready(pdrv);
	if (res_ready == 0) {
		LCD_DBG_ADV2(pdrv, "%s: lcd resource is not ready\n", __func__);
		lcd_queue_work(&pdrv->late_resume_work);
		return;
	}
	lcd_late_resume(pdrv);
}

static void lcd_init_on_delayed_work(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_lcd_drv_s *pdrv;
	int res_ready;

	d_work = container_of(p_work, struct delayed_work, work);
	pdrv = container_of(d_work, struct aml_lcd_drv_s, init_on_delayed_work);

	if (pdrv->init_flag == 0)
		return;

	res_ready = lcd_resource_is_ready(pdrv);
	if (res_ready == 0) {
		LCD_DBG(pdrv, "%s: lcd resource is not ready", __func__);
		lcd_queue_delayed_work(&pdrv->init_on_delayed_work, 10);
		return;
	}

	mutex_lock(&lcd_power_mutex);
	LCD_PR(pdrv, "power on for init_flag");
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
	lcd_if_enable_retry(pdrv);
	pdrv->status |= LCD_STATE_POWER;
	mutex_unlock(&lcd_power_mutex);
}

static void lcd_auto_test_delayed(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_lcd_drv_s *pdrv;

	d_work = container_of(p_work, struct delayed_work, work);
	pdrv = container_of(d_work, struct aml_lcd_drv_s, test_delayed_work);

	LCD_PR(pdrv, "%s", __func__);
	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
	pdrv->status |= (LCD_STATE_PREPARE | LCD_STATE_POWER);
	mutex_unlock(&lcd_power_mutex);
}

static void lcd_auto_test_func(struct aml_lcd_drv_s *pdrv)
{
	pdrv->test_state = pdrv->auto_test;
	lcd_queue_delayed_work(&pdrv->test_delayed_work, 20000);
}

static void lcd_isr_time_sort_save(unsigned int *table, unsigned int data)
{
	int i, j;

	for (i = 4; i >= 0; i--) {
		if (data > table[i]) {
			for (j = 0; j < i; j++)
				table[j] = table[j + 1];
			table[i] = data;
			break;
		}
	}
}

static inline void lcd_vsync_handler(struct aml_lcd_drv_s *pdrv)
{
	unsigned long long local_time[2];
	unsigned long flags = 0;
	unsigned int temp;
	unsigned long long duration;
	unsigned int fr, line[2];

	if (!pdrv)
		return;

	spin_lock_irqsave(&pdrv->isr_lock, flags);
	local_time[0] = sched_clock();
	line[0] = lcd_get_encl_line_cnt(pdrv);

	lcd_fr_lock(pdrv);
	lcd_sw_vrr_proc(pdrv);

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_vsync_handler(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
	case LCD_P2P:
		lcd_tcon_vsync_isr(pdrv);
		break;
#endif
	default:
		break;
	}

	if (pdrv->fr_show) {
		fr = vout_frame_rate_measure(pdrv->viu_sel);
		duration = local_time[0] - pdrv->vs_time;
		pr_info("fr=%d.%03d, duration=%llu\n", fr / 1000, fr % 1000, duration);
	}
	if (pdrv->vsync_test[0] == LCD_VSYNC_TEST_HTOTAL) {
		if ((pdrv->vsync_cnt % pdrv->vsync_test[1]) == 0) {
			if (pdrv->vsync_test_cnt == 0) {
				lcd_venc_adj_htotal(pdrv, pdrv->vsync_test[2]);
				pdrv->vsync_test_cnt = 1;
			} else {
				lcd_venc_adj_htotal(pdrv, pdrv->vsync_test[3]);
				pdrv->vsync_test_cnt = 0;
			}
		}
	} else if (pdrv->vsync_test[0] == LCD_VSYNC_TEST_VTOTAL) {
		if ((pdrv->vsync_cnt % pdrv->vsync_test[1]) == 0) {
			if (pdrv->vsync_test_cnt == 0) {
				lcd_venc_adj_vtotal(pdrv, pdrv->vsync_test[2]);
				pdrv->vsync_test_cnt = 1;
			} else {
				lcd_venc_adj_vtotal(pdrv, pdrv->vsync_test[3]);
				pdrv->vsync_test_cnt = 0;
			}
		}
	} else if (pdrv->vsync_test[0] == LCD_VSYNC_TEST_BIST) {
		if ((pdrv->vsync_cnt % pdrv->vsync_test[1]) == 0) {
			if (pdrv->vsync_test_cnt == 0) {
				lcd_bist_change(pdrv, pdrv->vsync_test[2],
						pdrv->vsync_test[3], pdrv->vsync_test[4]);
				pdrv->vsync_test_cnt = 1;
			} else {
				lcd_bist_change(pdrv, pdrv->vsync_test[5],
						pdrv->vsync_test[6], pdrv->vsync_test[7]);
				pdrv->vsync_test_cnt = 0;
			}
		}
	}

	if (pdrv->mute_switch) {
		if (pdrv->mute_wait_cnt > 0) {
			pdrv->mute_wait_cnt--;
		} else if (pdrv->mute_wait_cnt == 0) {
			complete(&pdrv->vsync_done);
			pdrv->mute_switch = 0;
		}
	}

	if (pdrv->mute_state != pdrv->mute_flag) {
		pdrv->mute_state = pdrv->mute_flag;
		lcd_mute_set(pdrv, pdrv->mute_state);
	}

	if (!pdrv->test_locked) {
		if (pdrv->test_flag != pdrv->test_state) {
			pdrv->test_state = pdrv->test_flag;
			lcd_debug_test(pdrv, pdrv->test_state);
		}
	}

	if (pdrv->window_attr.window_attr_changed) {
		lcd_window_attr_change(pdrv);
		pdrv->window_attr.window_attr_changed = 0;
	}

	if (pdrv->vs_msr && pdrv->vs_msr_rt && pdrv->vs_msr_en) {
		temp = vout_frame_rate_msr_high_res(pdrv->viu_sel);
		pdrv->vs_msr_rt[pdrv->vs_msr_i++] = temp;
		pdrv->vs_msr_sum_temp += temp;
		if (temp > pdrv->vs_msr_max)
			pdrv->vs_msr_max = temp;
		if (temp < pdrv->vs_msr_min)
			pdrv->vs_msr_min = temp;
		if (pdrv->vs_msr_i >= pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate) {
			pdrv->vs_msr[pdrv->vs_msr_cnt++] =
				lcd_do_div(pdrv->vs_msr_sum_temp, pdrv->vs_msr_i);
			pdrv->vs_msr_sum_temp = 0;
			if (pdrv->vs_msr_cnt >= pdrv->vs_msr_cnt_max)
				pdrv->vs_msr_en = 0;
			else
				pdrv->vs_msr_i = 0;
		}
	}
	pdrv->vsync_cnt++;
	pdrv->vs_time = local_time[0];
	local_time[1] = sched_clock();
	line[1] = lcd_get_encl_line_cnt(pdrv);
	temp = lcd_do_div(local_time[1] - local_time[0], 1000);
	pdrv->proc_time.lcd_vs_isr_time = temp;
	lcd_isr_time_sort_save(pdrv->proc_time.vs_isr_time, temp);
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	if (line[1] < line[0]) {
		LCD_ERR(pdrv, "vsync_handler: over frame error! enter: %d, exit: %d",
			line[0], line[1]);
	}
}

static irqreturn_t lcd_vsync_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 1)
		lcd_vsync_handler(pdrv);
	return IRQ_HANDLED;
}

static irqreturn_t lcd_vsync2_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 2)
		lcd_vsync_handler(pdrv);
	return IRQ_HANDLED;
}

static irqreturn_t lcd_vsync3_isr(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return IRQ_HANDLED;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return IRQ_HANDLED;

	if (pdrv->viu_sel == 3)
		lcd_vsync_handler(pdrv);
	return IRQ_HANDLED;
}

static void lcd_vsync_none_timer_handler(struct timer_list *timer)
{
	struct aml_lcd_drv_s *pdrv = from_timer(pdrv, timer, vs_none_timer);

	if (pdrv->data->chip_type == LCD_CHIP_C3)
		return;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		goto lcd_vsync_none_timer_handler_end;

	if (pdrv->vsync_cnt == pdrv->vsync_cnt_previous) {
		lcd_wait_vsync(pdrv);
		lcd_vsync_handler(pdrv);
	}

	pdrv->vsync_cnt_previous = pdrv->vsync_cnt;

lcd_vsync_none_timer_handler_end:
	if (pdrv->vsync_none_timer_flag) {
		pdrv->vs_none_timer.expires =
			jiffies + LCD_VSYNC_NONE_INTERVAL;
		add_timer(&pdrv->vs_none_timer);
	}
}

/* ****************************************
 * lcd notify
 * ****************************************
 */
static int lcd_power_encl_on_notifier(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	lcd_power_encl_on(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_on_nb = {
	.notifier_call = lcd_power_encl_on_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_ON,
};

static int lcd_power_encl_off_notifier(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	lcd_power_encl_off(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_off_nb = {
	.notifier_call = lcd_power_encl_off_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_OFF,
};

static int lcd_power_encl_active_notifier(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_ACTIVE) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	lcd_set_encl_active(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_active_nb = {
	.notifier_call = lcd_power_encl_active_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_ACTIVE,
};

static int lcd_power_encl_dummy_notifier(struct notifier_block *nb,
				       unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if ((event & LCD_EVENT_ENCL_DUMMY) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	lcd_set_encl_dummy(pdrv);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_encl_dummy_nb = {
	.notifier_call = lcd_power_encl_dummy_notifier,
	.priority = LCD_PRIORITY_POWER_ENCL_DUMMY,
};

static int lcd_mode_switch_data_on_notifier(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_MDSW_DATA_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCDERR("[%d]: %s: failed when encl is off\n", pdrv->index, __func__);
		return NOTIFY_DONE;
	}

	local_time[0] = sched_clock();

	lcd_mode_switch_data_on(pdrv);

	local_time[1] = sched_clock();
	pdrv->proc_time.signal_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_mode_switch_data_on_nb = {
	.notifier_call = lcd_mode_switch_data_on_notifier,
	.priority = LCD_PRIORITY_MDSW_DATA_ON,
};

static int lcd_mode_switch_data_off_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_MDSW_DATA_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	local_time[0] = sched_clock();

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		lcd_mode_switch_data_off(pdrv);
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.signal_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_mode_switch_data_off_nb = {
	.notifier_call = lcd_mode_switch_data_off_notifier,
	.priority = LCD_PRIORITY_MDSW_DATA_OFF,
};

static int lcd_mode_switch_power_on_notifier(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_MDSW_POWER_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	local_time[0] = sched_clock();

	lcd_mode_switch_power_on(pdrv);

	local_time[1] = sched_clock();
	pdrv->proc_time.power_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_mode_switch_power_on_nb = {
	.notifier_call = lcd_mode_switch_power_on_notifier,
	.priority = LCD_PRIORITY_MDSW_POWER_ON,
};

static int lcd_mode_switch_power_off_notifier(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_MDSW_POWER_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	local_time[0] = sched_clock();

	lcd_mode_switch_power_off(pdrv);

	local_time[1] = sched_clock();
	pdrv->proc_time.power_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_mode_switch_power_off_nb = {
	.notifier_call = lcd_mode_switch_power_off_notifier,
	.priority = LCD_PRIORITY_MDSW_POWER_OFF,
};

static int lcd_power_if_on_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_IF_ON) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCD_PR(pdrv, "lcd interface is already enabled\n");
		return NOTIFY_OK;
	}

	local_time[0] = sched_clock();

	lcd_power_if_on(pdrv);

	local_time[1] = sched_clock();
	pdrv->proc_time.power_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_if_on_nb = {
	.notifier_call = lcd_power_if_on_notifier,
	.priority = LCD_PRIORITY_POWER_IF_ON,
};

static int lcd_power_if_off_notifier(struct notifier_block *nb,
				     unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];

	if ((event & LCD_EVENT_IF_OFF) == 0)
		return NOTIFY_DONE;
	if (!pdrv) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	local_time[0] = sched_clock();

	lcd_power_if_off(pdrv);

	local_time[1] = sched_clock();
	pdrv->proc_time.power_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return NOTIFY_OK;
}

static struct notifier_block lcd_power_if_off_nb = {
	.notifier_call = lcd_power_if_off_notifier,
	.priority = LCD_PRIORITY_POWER_IF_OFF,
};

static int lcd_vlock_param_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv;
	int index;
	unsigned int *param;

	if ((event & LCD_EVENT_VLOCK_PARAM) == 0)
		return NOTIFY_DONE;
	if (!data) {
		LCDERR("%s: data is null\n", __func__);
		return NOTIFY_DONE;
	}

	param = (unsigned int *)data;
	index = param[LCD_VLOCK_PARAM_NUM];
	pdrv = aml_lcd_get_driver(index);
	if (!pdrv) {
		LCDERR("[%d]: %s: drv is null\n", index, __func__);
		return NOTIFY_DONE;
	}
	if (pdrv->probe_done == 0)
		return NOTIFY_DONE;

	LCD_DBG(pdrv, "%s: 0x%lx", __func__, event);

	memcpy(param, pdrv->curr_dev->dev_cfg.vlock_param,
	       (LCD_VLOCK_PARAM_NUM * sizeof(unsigned int)));

	return NOTIFY_OK;
}

static struct notifier_block lcd_vlock_param_nb = {
	.notifier_call = lcd_vlock_param_notifier,
};

static int lcd_notifier_init(void)
{
	int ret = 0;

	ret = aml_lcd_notifier_register(&lcd_power_encl_on_nb);
	if (ret)
		LCDERR("register lcd_power_encl_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_encl_off_nb);
	if (ret)
		LCDERR("register lcd_power_encl_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_encl_active_nb);
	if (ret)
		LCDERR("register lcd_power_encl_active_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_encl_dummy_nb);
	if (ret)
		LCDERR("register lcd_power_encl_dummy_nb failed\n");

	ret = aml_lcd_notifier_register(&lcd_mode_switch_data_on_nb);
	if (ret)
		LCDERR("register lcd_mode_switch_data_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_mode_switch_data_off_nb);
	if (ret)
		LCDERR("register lcd_mode_switch_data_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_mode_switch_power_on_nb);
	if (ret)
		LCDERR("register lcd_mode_switch_power_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_mode_switch_power_off_nb);
	if (ret)
		LCDERR("register lcd_mode_switch_power_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_if_on_nb);
	if (ret)
		LCDERR("register lcd_power_if_on_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_power_if_off_nb);
	if (ret)
		LCDERR("register lcd_power_if_off_nb failed\n");
	ret = aml_lcd_notifier_register(&lcd_vlock_param_nb);
	if (ret)
		LCDERR("register lcd_vlock_param_nb failed\n");

	return 0;
}

static void lcd_notifier_remove(void)
{
	aml_lcd_notifier_unregister(&lcd_mode_switch_data_off_nb);
	aml_lcd_notifier_unregister(&lcd_mode_switch_data_on_nb);
	aml_lcd_notifier_unregister(&lcd_mode_switch_power_off_nb);
	aml_lcd_notifier_unregister(&lcd_mode_switch_power_on_nb);
	aml_lcd_notifier_unregister(&lcd_power_if_off_nb);
	aml_lcd_notifier_unregister(&lcd_power_if_on_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_dummy_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_active_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_off_nb);
	aml_lcd_notifier_unregister(&lcd_power_encl_on_nb);

	aml_lcd_notifier_unregister(&lcd_vlock_param_nb);
}

/* ************************************************************* */
/* lcd ioctl                                                     */
/* ************************************************************* */
static int lcd_io_open(struct inode *inode, struct file *file)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(inode->i_cdev, struct aml_lcd_drv_s, cdev);
	file->private_data = pdrv;

	LCD_DBG(pdrv, "%s", __func__);

	return 0;
}

static int lcd_io_release(struct inode *inode, struct file *file)
{
	struct aml_lcd_drv_s *pdrv;

	if (!file->private_data)
		return 0;

	pdrv = (struct aml_lcd_drv_s *)file->private_data;
	LCD_DBG(pdrv, "%s", __func__);
	file->private_data = NULL;
	return 0;
}

__weak long lcd_test_ioctl_handler(struct aml_lcd_drv_s *pdrv, int mcd_nr, unsigned long arg)
{
	return 0;
}

static long lcd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp;
	int mcd_nr = -1;
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)file->private_data;
	struct lcd_optical_info_s *opt_info;
	union lcd_ctrl_config_u *pctrl;
	struct lcd_config_s *pconf;
	struct phy_config_s *phy_cfg;
	struct phy_attr_s *phy;
	struct ioctl_phy_config_s ioctl_phy_cfg;
	unsigned int ss_level = 0xffffffff, ss_freq = 0xffffffff, ss_mode = 0xffffffff;
	struct aml_lcd_ss_ctl_s ss_ctl = {0xffffffff, 0xffffffff, 0xffffffff};
	struct aml_lcd_panel_info_s panel_info;
	unsigned int temp, i = 0, lane_num;
	int ret = 0;
	unsigned long flags;

	if (!pdrv || !pdrv->curr_dev)
		return -EFAULT;
	argp = (void __user *)arg;
	if (IS_ERR_OR_NULL(argp))
		return -EFAULT;

	pconf = &pdrv->curr_dev->dev_cfg;
	pctrl = &pdrv->curr_dev->dev_cfg.control;
	opt_info = &pdrv->curr_dev->dev_cfg.optical;
	mcd_nr = _IOC_NR(cmd);
	LCD_PR(pdrv, "%s: cmd_dir = 0x%x, cmd_nr = 0x%x", __func__, _IOC_DIR(cmd), mcd_nr);


	if (mcd_nr >= 0xf0 && mcd_nr <= 0xff) {
		ret = lcd_test_ioctl_handler(pdrv, mcd_nr, arg);
		goto lcd_ioctl_end;
	}

	switch (mcd_nr) {
	case LCD_IOC_NR_GET_HDR_INFO:
		if (copy_to_user(argp, opt_info, sizeof(struct lcd_optical_info_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_NR_SET_HDR_INFO:
		if (copy_from_user(opt_info, argp, sizeof(struct lcd_optical_info_s))) {
			ret = -EFAULT;
		} else {
			lcd_optical_vinfo_update(pdrv);
			LCD_DBG(pdrv, "set optical info:\n"
				      "\thdr_support          %d\n"
				      "\tfeatures             %d\n"
				      "\tprimaries_r_x        %d\n"
				      "\tprimaries_r_y        %d\n"
				      "\tprimaries_g_x        %d\n"
				      "\tprimaries_g_y        %d\n"
				      "\tprimaries_b_x        %d\n"
				      "\tprimaries_b_y        %d\n"
				      "\twhite_point_x        %d\n"
				      "\twhite_point_y        %d\n"
				      "\tluma_max             %d\n"
				      "\tluma_min             %d\n"
				      "\tluma_avg             %d\n",
				opt_info->hdr_support,
				opt_info->features,
				opt_info->primaries_r_x,
				opt_info->primaries_r_y,
				opt_info->primaries_g_x,
				opt_info->primaries_g_y,
				opt_info->primaries_b_x,
				opt_info->primaries_b_y,
				opt_info->white_point_x,
				opt_info->white_point_y,
				opt_info->luma_max,
				opt_info->luma_min,
				opt_info->luma_avg);
		}
		break;
	case LCD_IOC_POWER_CTRL:
		if (copy_from_user((void *)&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		if (temp) {
			mutex_lock(&lcd_power_mutex);
			if (pdrv->status & LCD_STATE_DUMMY) {
				LCDERR("%s: power_on exit for dummy_state: 0x%x\n",
					__func__, pdrv->status);
				ret = -EFAULT;
				break;
			}
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_ON, (void *)pdrv);
			lcd_if_enable_retry(pdrv);
			pdrv->status |= LCD_STATE_POWER;
			mutex_unlock(&lcd_power_mutex);
		} else {
			mutex_lock(&lcd_power_mutex);
			pdrv->init_flag = 0;
			pdrv->status &= ~LCD_STATE_POWER;
			aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
			mutex_unlock(&lcd_power_mutex);
		}
		break;
	case LCD_IOC_MUTE_CTRL:
		if (copy_from_user((void *)&temp, argp, sizeof(unsigned int))) {
			ret = -EFAULT;
			break;
		}
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		if (temp)
			pdrv->mute_flag = 1;
		else
			pdrv->mute_flag = 0;
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		break;
	case LCD_IOC_GET_PHY_PARAM:
		phy_cfg = &pconf->phy_cfg;
		phy = pconf->phy_cfg.act_phy;
		if (!phy) {
			ret = -EFAULT;
			break;
		}
		ioctl_phy_cfg.lane_num = phy_cfg->lane_num;
		ioctl_phy_cfg.vswing = phy->vswing;
		ioctl_phy_cfg.vcm = phy->vcm;
		ioctl_phy_cfg.odt = phy->odt;
		ioctl_phy_cfg.cv_mode = phy->cv_mode;
		lane_num = phy_cfg->lane_num > CH_LANE_MAX ? CH_LANE_MAX : phy_cfg->lane_num;
		for (i = 0; i < lane_num; i++) {
			ioctl_phy_cfg.lane[i].preem = phy->lane[i].preem;
			ioctl_phy_cfg.lane[i].amp = phy->lane[i].amp;
			ioctl_phy_cfg.lane[i].rterm = phy->lane[i].rterm;
			ioctl_phy_cfg.lane[i].data_sel = phy_cfg->ch_ctrl[i].sel;
			ioctl_phy_cfg.lane[i].pn_swap = phy_cfg->ch_ctrl[i].pn_swap;
		}
		if (copy_to_user(argp, (const void *)&ioctl_phy_cfg,
		    sizeof(struct ioctl_phy_config_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_SET_PHY_PARAM:
		memset(&ioctl_phy_cfg, 0, sizeof(struct ioctl_phy_config_s));
		if (copy_from_user((void *)&ioctl_phy_cfg, argp,
		    sizeof(struct ioctl_phy_config_s))) {
			ret = -EFAULT;
			break;
		}
		phy_cfg = &pconf->phy_cfg;
		phy = pconf->phy_cfg.act_phy;
		if (!phy) {
			ret = -EFAULT;
			break;
		}

		phy->vswing = ioctl_phy_cfg.vswing;
		phy->vcm = ioctl_phy_cfg.vcm;
		phy->odt = ioctl_phy_cfg.odt;
		phy->cv_mode = ioctl_phy_cfg.cv_mode;

		lane_num = phy_cfg->lane_num > CH_LANE_MAX ? CH_LANE_MAX : phy_cfg->lane_num;
		for (i = 0; i < lane_num; i++) {
			phy->lane[i].preem = ioctl_phy_cfg.lane[i].preem;
			phy->lane[i].amp = ioctl_phy_cfg.lane[i].amp;
			phy->lane[i].rterm = ioctl_phy_cfg.lane[i].rterm;
			phy_cfg->ch_ctrl[i].sel = ioctl_phy_cfg.lane[i].data_sel;
			phy_cfg->ch_ctrl[i].pn_swap = ioctl_phy_cfg.lane[i].pn_swap;
		}

		if (pdrv->status & LCD_STATUS_IF_ON) {
			lcd_lane_map_update(pdrv);
			lcd_lane_map_set(pdrv);
			lcd_phy_set(pdrv, LCD_PHY_UPDATE);
		}
		break;
	case LCD_IOC_GET_SS:
		ret = lcd_get_ss_num(pdrv, &ss_ctl.level, &temp, &ss_ctl.freq, &ss_ctl.mode);
		if (ret < 0) {
			ret = -EFAULT;
			break;
		}
		if (copy_to_user(argp, (const void *)&ss_ctl, sizeof(struct aml_lcd_ss_ctl_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_SET_SS:
		if (copy_from_user((void *)&ss_ctl, argp, sizeof(struct aml_lcd_ss_ctl_s))) {
			ret = -EFAULT;
			break;
		}
		if (ss_ctl.level != 0xffffffff)
			ss_level = ss_ctl.level & 0xff;
		if (ss_ctl.freq != 0xffffffff)
			ss_freq  = ss_ctl.freq & 0xff;
		if (ss_ctl.mode != 0xffffffff)
			ss_mode  = ss_ctl.mode & 0xff;
		ret = lcd_set_ss(pdrv, ss_level, ss_freq, ss_mode);
		if (ret == 0) {
			if (ss_level <= 0xff)
				pdrv->curr_dev->dev_cfg.timing.ss_level = ss_level;
			if (ss_freq <= 0xff)
				pdrv->curr_dev->dev_cfg.timing.ss_freq = ss_freq;
			if (ss_mode <= 0xff)
				pdrv->curr_dev->dev_cfg.timing.ss_mode = ss_mode;
		}
		break;
	case LCD_IOC_GET_CONFIG_READY:
		if (copy_to_user(argp, (const void *)&pdrv->probe_done, sizeof(unsigned int)))
			ret = -EFAULT;
		break;
	case LCD_IOC_GET_PANEL_INFO:
		if (!pdrv->curr_dev) {
			ret = -EFAULT;
			break;
		}
		panel_info.lcd_type = pdrv->curr_dev->dev_cfg.basic.lcd_type;
		panel_info.status = !!(pdrv->status & LCD_STATE_POWER);
		if (copy_to_user(argp, (const void *)&panel_info,
				 sizeof(struct aml_lcd_panel_info_s)))
			ret = -EFAULT;
		break;
	default:
#ifdef CONFIG_AMLOGIC_LCD_TCON
		ret = lcd_tcon_ioctl_handler(pdrv, mcd_nr, arg);
		if (ret)
			LCD_ERR(pdrv, "not support ioctl cmd_nr: 0x%x", mcd_nr);
#endif
		break;
	}

lcd_ioctl_end:
	return ret;
}

#ifdef CONFIG_COMPAT
static long lcd_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = lcd_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations lcd_fops = {
	.owner          = THIS_MODULE,
	.open           = lcd_io_open,
	.release        = lcd_io_release,
	.unlocked_ioctl = lcd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = lcd_compat_ioctl,
#endif
};

static int lcd_cdev_add(struct aml_lcd_drv_s *pdrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!pdrv) {
		LCD_ERR(pdrv, "%s: pdrv is null", __func__);
		return -1;
	}
	if (!lcd_cdev) {
		ret = 1;
		goto lcd_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(lcd_cdev->devno), pdrv->index);

	cdev_init(&pdrv->cdev, &lcd_fops);
	pdrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&pdrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto lcd_cdev_add_failed;
	}

	pdrv->dev = device_create(lcd_cdev->class, parent, devno, NULL, "lcd%d", pdrv->index);
	if (IS_ERR_OR_NULL(pdrv->dev)) {
		ret = 3;
		goto lcd_cdev_add_failed1;
	}

	dev_set_drvdata(pdrv->dev, pdrv);
	pdrv->dev->of_node = parent->of_node;

	LCD_DBG(pdrv, "%s OK", __func__);
	return 0;

lcd_cdev_add_failed1:
	cdev_del(&pdrv->cdev);
lcd_cdev_add_failed:
	LCD_ERR(pdrv, "%s: failed: %d", __func__, ret);
	return -1;
}

static void lcd_cdev_remove(struct aml_lcd_drv_s *pdrv)
{
	dev_t devno;

	if (!lcd_cdev || !pdrv)
		return;

	devno = MKDEV(MAJOR(lcd_cdev->devno), pdrv->index);
	device_destroy(lcd_cdev->class, devno);
	cdev_del(&pdrv->cdev);
}

static int lcd_global_init_once(struct lcd_data_s *pdata)
{
	int ret;

	if (lcd_global_init_flag++)
		return 0;

	lcd_bootargs_debug_load();

	mutex_init(&lcd_vout_mutex);
	mutex_init(&lcd_power_mutex);
	lcd_clk_init();
	lcd_phy_config_init(pdata);
	lcd_venc_config_init(pdata);

	lcd_notifier_init();
#ifdef CONFIG_AMLOGIC_LCD_EXTERN
	lcd_extern_init();
#endif

	/* init workqueue */
	lcd_workqueue = create_workqueue("lcd_work_queue");
	if (!lcd_workqueue) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDERR("can't create lcd workqueue\n");
	}

	lcd_cdev = kzalloc(sizeof(*lcd_cdev), GFP_KERNEL);
	if (!lcd_cdev)
		return -1;

	ret = alloc_chrdev_region(&lcd_cdev->devno, 0, LCD_MAX_DRV, LCD_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto lcd_cdev_init_once_err;
	}

	lcd_cdev->class = class_create("aml_lcd");
	if (IS_ERR_OR_NULL(lcd_cdev->class)) {
		ret = 2;
		goto lcd_cdev_init_once_err_1;
	}

	return 0;

lcd_cdev_init_once_err_1:
	unregister_chrdev_region(lcd_cdev->devno, LCD_MAX_DRV);
lcd_cdev_init_once_err:
	kfree(lcd_cdev);
	lcd_cdev = NULL;
	LCDERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void lcd_global_remove_once(void)
{
	if (lcd_global_init_flag > 1) {
		lcd_global_init_flag--;
		return;
	}
	lcd_global_init_flag--;

	lcd_notifier_remove();

	if (!lcd_cdev)
		return;

	class_destroy(lcd_cdev->class);
	unregister_chrdev_region(lcd_cdev->devno, LCD_MAX_DRV);
	kfree(lcd_cdev);
	lcd_cdev = NULL;
}

/* ************************************************************* */
static int lcd_vsync_irq_init(struct aml_lcd_drv_s *pdrv)
{
	init_completion(&pdrv->vsync_done);
	if (pdrv->drv_res.res_vsync_irq[0]) {
		snprintf(pdrv->drv_res.vsync_isr_name[0], 15, "lcd%d_vsync", pdrv->index);
		if (request_irq(pdrv->drv_res.res_vsync_irq[0], lcd_vsync_isr, IRQF_SHARED,
				pdrv->drv_res.vsync_isr_name[0], (void *)pdrv)) {
			LCD_ERR(pdrv, "can't request %s", pdrv->drv_res.vsync_isr_name[0]);
		} else {
			LCD_DBG(pdrv, "request %s successful", pdrv->drv_res.vsync_isr_name[0]);
		}
	}

	if (pdrv->drv_res.res_vsync_irq[1]) {
		snprintf(pdrv->drv_res.vsync_isr_name[1], 15, "lcd%d_vsync2", pdrv->index);
		if (request_irq(pdrv->drv_res.res_vsync_irq[1], lcd_vsync2_isr, IRQF_SHARED,
				pdrv->drv_res.vsync_isr_name[1], (void *)pdrv)) {
			LCD_ERR(pdrv, "can't request %s", pdrv->drv_res.vsync_isr_name[1]);
		} else {
			LCD_DBG(pdrv, "request %s successful", pdrv->drv_res.vsync_isr_name[1]);
		}
	}

	if (pdrv->drv_res.res_vsync_irq[2]) {
		snprintf(pdrv->drv_res.vsync_isr_name[2], 15, "lcd%d_vsync3", pdrv->index);
		if (request_irq(pdrv->drv_res.res_vsync_irq[2], lcd_vsync3_isr, IRQF_SHARED,
				pdrv->drv_res.vsync_isr_name[2], (void *)pdrv)) {
			LCD_ERR(pdrv, "can't request %s", pdrv->drv_res.vsync_isr_name[2]);
		} else {
			LCD_DBG(pdrv, "request %s successful", pdrv->drv_res.vsync_isr_name[2]);
		}
	}

	/* add timer to monitor hpll frequency */
	timer_setup(&pdrv->vs_none_timer, &lcd_vsync_none_timer_handler, 0);
	/* pdrv->vs_none_timer.data = NULL; */
	pdrv->vs_none_timer.expires = jiffies + LCD_VSYNC_NONE_INTERVAL;
	/*add_timer(&pdrv->vs_none_timer);*/
	/*LCDPR("add vs_none_timer handler\n"); */

	return 0;
}

static void lcd_vsync_irq_remove(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->drv_res.res_vsync_irq[0])
		free_irq(pdrv->drv_res.res_vsync_irq[0], (void *)pdrv);
	if (pdrv->drv_res.res_vsync_irq[1])
		free_irq(pdrv->drv_res.res_vsync_irq[1], (void *)pdrv);
	if (pdrv->drv_res.res_vsync_irq[2])
		free_irq(pdrv->drv_res.res_vsync_irq[2], (void *)pdrv);

	if (pdrv->vsync_none_timer_flag) {
		del_timer_sync(&pdrv->vs_none_timer);
		pdrv->vsync_none_timer_flag = 0;
	}
}

void lcd_device_config_pre_clean(struct aml_lcd_drv_s *pdrv)
{
	lcd_debug_remove_connector(pdrv);

	lcd_connector_config_remove(pdrv);

	// lcd_resource_remove_all(pdrv);
	// lcd_timing_free_all(pdrv, pdrv->curr_dev);
	lcd_phy_free_all(pdrv, pdrv->curr_dev);

	lcd_fr_lock_deinit(pdrv);

	lcd_mode_vout_server_remove(pdrv);
	lcd_mode_remove(pdrv);

	// lcd_clk_config_remove(pdrv);

	//lcd_mode_deinit()
	//lcd_deinit_vout
}

void lcd_device_config_post_process(struct aml_lcd_drv_s *pdrv)
{
	/* must behind lcd_config_load, for phy will probe by interface type */
	lcd_config_load_probe(pdrv);
	lcd_clk_config_probe(pdrv);
	lcd_debug_init_connector(pdrv);
	lcd_mode_init(pdrv);// after config
	lcd_phy_probe(pdrv);   // after config
	pdrv->probe_done = 1;

	//! lcd_drm_add(pdrv->dev); //once
}

static int lcd_device_probe(struct aml_lcd_drv_s *pdrv)
{
	void *ret;
	unsigned char i;

	if (!pdrv)
		return -1;

	if (!pdrv->device_mgr.uboot_pre_devs[0][0]) {
		pdrv->device_mgr.uboot_dev_idx = 0;
		strscpy(pdrv->device_mgr.uboot_pre_devs[0], lcd_propname[pdrv->index], 15);
	}

	for (i = 0; i < ARRAY_SIZE(pdrv->device_mgr.uboot_pre_devs); i++) {
		if (!pdrv->device_mgr.uboot_pre_devs[i][0])
			break;
		ret = lcd_device_append_new(pdrv, pdrv->device_mgr.uboot_pre_devs[i]);
		if (!ret)
			LCD_ERR(pdrv, "device load failed");
	}

	ret = lcd_device_assign(pdrv, pdrv->device_mgr.uboot_dev_idx);
	if (!ret)
		return 1;

	return 0;
}

static int lcd_config_remove(struct aml_lcd_drv_s *pdrv)
{
	lcd_config_load_remove(pdrv);
	lcd_fr_lock_deinit(pdrv);

	lcd_mode_vout_server_remove(pdrv);
	lcd_mode_remove(pdrv);

	lcd_clk_config_remove(pdrv);

	return 0;
}

__maybe_unused static void lcd_vout_server_remove(struct aml_lcd_drv_s *pdrv)
{
	lcd_mode_vout_server_remove(pdrv);
}

static void lcd_bootup_config_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int init_state;
	unsigned long flags;

	init_state = lcd_get_venc_init_config(pdrv);

	pdrv->init_flag = 0;
	if (pdrv->boot_ctrl->mute_flag) {
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		pdrv->mute_flag = 1;
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
	}
	pdrv->mute_state = 0;
	pdrv->mute_switch = 0;
	pdrv->mute_wait_cnt = 0;
	pdrv->mute_cnt = 3;
	pdrv->unmute_cnt = 4;
	pdrv->mute_cnt_test = 0;
	pdrv->unmute_cnt_test = 0;
	pdrv->unmute_cnt_added = 0;
	pdrv->tcon_isr_bypass = 0;
	pdrv->fr_mode = 0;
	pdrv->viu_sel = LCD_VIU_SEL_NONE;
	pdrv->vsync_none_timer_flag = 0;
	pdrv->test_flag = pdrv->debug_ctrl->debug_test_pattern;
	pdrv->test_state = pdrv->test_flag;
	pdrv->test_locked = 0;

	if (init_state & 0x1) {
		if (pdrv->boot_ctrl->dccd_flag) {
			pdrv->boot_ctrl->init_level = LCD_INIT_LEVEL_KERNEL_ON;
			LCD_PR(pdrv, "DCCD flow detected!");
		}

		switch (pdrv->boot_ctrl->init_level) {
		case LCD_INIT_LEVEL_NORMAL:
		case LCD_INIT_LEVEL_PREBOOT:
			pdrv->status = (LCD_STATUS_ON | LCD_STATE_PREPARE | LCD_STATE_POWER);
			break;
		case LCD_INIT_LEVEL_PWR_OFF:
			pdrv->status = (LCD_STATUS_ENCL_ON | LCD_STATE_PREPARE);
			break;
		case LCD_INIT_LEVEL_KERNEL_ON:
			pdrv->init_flag = 1;
			pdrv->status = (LCD_STATUS_ENCL_ON | LCD_STATE_PREPARE);
			break;
		default:
			pdrv->status = (LCD_STATUS_ON | LCD_STATE_PREPARE | LCD_STATE_POWER);
			break;
		}

		if (pdrv->boot_ctrl->if_state)
			pdrv->status |= (LCD_STATUS_IF_ON | LCD_STATE_POWER);
		else
			pdrv->status &= ~(LCD_STATUS_IF_ON | LCD_STATE_POWER);
	} else {
		pdrv->status = 0;
	}
	LCD_PR(pdrv, "init: ppc: %d, clk_mode: %d, base_fr: %d, status: 0x%x, init_flag: %d",
		pdrv->curr_dev->dev_cfg.timing.ppc,
		pdrv->curr_dev->dev_cfg.timing.act_timing.clk_mode,
		pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate,
		pdrv->status, pdrv->init_flag);
}

static int lcd_config_probe(struct aml_lcd_drv_s *pdrv, struct platform_device *pdev)
{
	int ret = 0;
	char lrm_dev_name[32] = {0};

	ret = lcd_base_config_load_from_dts(pdrv);
	if (ret)
		return -1;

	lcd_debug_probe(pdrv);

	pdrv->drv_res.res_vsync_irq[0] = platform_get_irq_byname(pdev, "vsync");
	pdrv->drv_res.res_vsync_irq[1] = platform_get_irq_byname(pdev, "vsync2");
	pdrv->drv_res.res_vsync_irq[2] = platform_get_irq_byname(pdev, "vsync3");
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	pdrv->drv_res.res_vx1_irq = platform_get_irq_byname(pdev, "vbyone");
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	pdrv->drv_res.res_tcon_irq = platform_get_irq_byname(pdev, "tcon");
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	pdrv->drv_res.res_dsi_intr_irq = platform_get_irq_byname(pdev, "dsi_intr");
#endif
	lcd_vsync_irq_init(pdrv); //once

	sprintf(lrm_dev_name, "lcd%d", pdrv->index);
	lrm_resource_device_prepare(lrm_dev_name);

	if (lcd_check_config_load(pdrv)) {
		lrm_resource_device_finish(lrm_dev_name);
		return -1;
	}

	ret = lcd_device_probe(pdrv);
	if (ret) {
		lrm_resource_device_finish(lrm_dev_name);
		LCD_ERR(pdrv, "probe exit");
		return -1;
	}

	lcd_bootup_config_init(pdrv);
	lcd_device_config_post_process(pdrv);

	lcd_mode_vout_server_init(pdrv);
	lcd_fr_lock_init(pdrv);
	lcd_drm_add(pdrv->dev);
	lrm_resource_device_finish(lrm_dev_name);

	/* lock pinmux as earlier as possible if lcd in on */
	if (pdrv->status & LCD_STATUS_IF_ON) {
		lcd_pinmux_set(pdrv, 1);
		switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
		case LCD_LVDS:
		case LCD_VBYONE:
		case LCD_MLVDS:
		case LCD_P2P:
		case LCD_MIPI:
			pdrv->curr_dev->dev_cfg.phy_cfg.state = 1;
			break;
		default:
			break;
		}
	}

	if (pdrv->init_flag)  // run once
		lcd_queue_delayed_work(&pdrv->init_on_delayed_work, 0);

	if (pdrv->auto_test) //once
		lcd_auto_test_func(pdrv);

	return 0;
}

#ifdef CONFIG_OF
static struct lcd_data_s lcd_data_g12a = {
	.chip_type = LCD_CHIP_G12A,
	.chip_name = "g12a",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_g12b = {
	.chip_type = LCD_CHIP_G12B,
	.chip_name = "g12b",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_sm1 = {
	.chip_type = LCD_CHIP_SM1,
	.chip_name = "sm1",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_tm2 = {
	.chip_type = LCD_CHIP_TM2,
	.chip_name = "tm2",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_tl1[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t5d = {
	.chip_type = LCD_CHIP_T5D,
	.chip_name = "t5d",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t7 = {
	.chip_type = LCD_CHIP_T7,
	.chip_name = "t7",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 3,
	.offset_venc = {0x0, 0x600, 0x800},
	.offset_venc_if = {0x0, 0x500, 0x600},
	.offset_venc_data = {0x0, 0x100, 0x200},
};

static struct lcd_data_s lcd_data_t3 = {
	.chip_type = LCD_CHIP_T3,
	.chip_name = "t3",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 2,
	.offset_venc = {0x0, 0x600, 0x0},
	.offset_venc_if = {0x0, 0x500, 0x0},
	.offset_venc_data = {0x0, 0x100, 0x0},
};

static struct lcd_data_s lcd_data_t5w = {
	.chip_type = LCD_CHIP_T5W,
	.chip_name = "t5w",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_c3 = {
	.chip_type = LCD_CHIP_C3,
	.chip_name = "c3",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_c3[0],
	.drv_max = 1,
	.offset_venc = {0},
	.offset_venc_if = {0},
	.offset_venc_data = {0},
};

static struct lcd_data_s lcd_data_t5m = {
	.chip_type = LCD_CHIP_T5M,
	.chip_name = "t5m",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t5[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_t3x = {
	.chip_type = LCD_CHIP_T3X,
	.chip_name = "t3x",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t3x[0],
	.drv_max = 2,
	.offset_venc = {0x0, 0x100},
	.offset_venc_if = {0x0, 0x500},
	.offset_venc_data = {0x0, 0x100},
};

static struct lcd_data_s lcd_data_txhd2 = {
	.chip_type = LCD_CHIP_TXHD2,
	.chip_name = "txhd2",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_txhd2[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_s6 = {
	.chip_type = LCD_CHIP_S6,
	.chip_name = "s6",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_g12a[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_t6d = {
	.chip_type = LCD_CHIP_T6D,
	.chip_name = "t6d",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t6d[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_t6w = {
	.chip_type = LCD_CHIP_T6W,
	.chip_name = "t6w",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t6w[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_t6x = {
	.chip_type = LCD_CHIP_T6X,
	.chip_name = "t6x",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_t6x[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static struct lcd_data_s lcd_data_a9 = {
	.chip_type = LCD_CHIP_A9,
	.chip_name = "a9",
	.rev_type = 0,
	.reg_map_table = &lcd_reg_a9[0],
	.drv_max = 1,
	.offset_venc = {0x0},
	.offset_venc_if = {0x0},
	.offset_venc_data = {0x0},
};

static const struct of_device_id lcd_dt_match_table[] = {
	{
		.compatible = "amlogic, lcd-g12a",
		.data = &lcd_data_g12a,
	},
	{
		.compatible = "amlogic, lcd-g12b",
		.data = &lcd_data_g12b,
	},
	{
		.compatible = "amlogic, lcd-sm1",
		.data = &lcd_data_sm1,
	},
	{
		.compatible = "amlogic, lcd-tm2",
		.data = &lcd_data_tm2,
	},
	{
		.compatible = "amlogic, lcd-t5d",
		.data = &lcd_data_t5d,
	},
	{
		.compatible = "amlogic, lcd-t7",
		.data = &lcd_data_t7,
	},
	{
		.compatible = "amlogic, lcd-t3",
		.data = &lcd_data_t3,
	},
	{
		.compatible = "amlogic, lcd-t5w",
		.data = &lcd_data_t5w,
	},
	{
		.compatible = "amlogic, lcd-c3",
		.data = &lcd_data_c3,
	},
	{
		.compatible = "amlogic, lcd-t5m",
		.data = &lcd_data_t5m,
	},
	{
		.compatible = "amlogic, lcd-t3x",
		.data = &lcd_data_t3x,
	},
	{
		.compatible = "amlogic, lcd-txhd2",
		.data = &lcd_data_txhd2,
	},
	{
		.compatible = "amlogic, lcd-s6",
		.data = &lcd_data_s6,
	},
	{
		.compatible = "amlogic, lcd-t6d",
		.data = &lcd_data_t6d,
	},
	{
		.compatible = "amlogic, lcd-t6w",
		.data = &lcd_data_t6w,
	},
	{
		.compatible = "amlogic, lcd-t6x",
		.data = &lcd_data_t6x,
	},
	{
		.compatible = "amlogic, lcd-a9",
		.data = &lcd_data_a9,
	},
	{}
};
#endif

static struct lcd_data_s *lcd_chip_detect(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct lcd_data_s *lcd_data = NULL;
#ifdef CONFIG_OF
	match = of_match_device(lcd_dt_match_table, &pdev->dev);
	if (!match) {
		LCDERR("%s: no match table\n", __func__);
	} else {
		lcd_data = (struct lcd_data_s *)match->data;
		lcd_data->rev_type = get_meson_cpu_version(MESON_CPU_VERSION_LVL_MINOR);
	}
#endif
	return lcd_data;
}

static int lcd_probe(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv;
	struct lcd_data_s *pdata;
	unsigned int index = 0;
	int ret = 0;

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		LCDERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (lcd_drv_init_state & (1 << index)) {
		LCDERR("%s: index %d driver already registered\n", __func__, index);
		return -1;
	}
	lcd_drv_init_state |= (1 << index);

	pdata =  lcd_chip_detect(pdev);
	if (!pdata) {
		LCDERR("%s: no match data\n", __func__);
		return -1;
	}

	lcd_global_init_once(pdata);

	if (index >= pdata->drv_max) {
		LCDERR("[%d]: %s: invalid index\n", index, __func__);
		return -1;
	}

	pdrv = lcd_driver_add(index);
	if (!pdrv)
		goto lcd_probe_err_0;

	/* set drvdata */
	lcd_driver[index] = pdrv;
	pdrv->data = pdata;
	platform_set_drvdata(pdev, pdrv);
	pdrv->pdev = pdev;

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for lcd*/
	pdrv->lcd_vpu_dev = vpu_dev_register(VPU_VENCL, LCD_CDEV_NAME);
#endif
	ret = lcd_ioremap(pdrv, pdev);
	if (ret)
		goto lcd_probe_err_1;

	spin_lock_init(&pdrv->sw_vrr.set_lock);
	spin_lock_init(&pdrv->isr_lock);
	INIT_WORK(&pdrv->late_resume_work, lcd_late_resume_work);
	INIT_WORK(&pdrv->mode_switch_on_work, lcd_mode_switch_on_work);
	INIT_DELAYED_WORK(&pdrv->init_on_delayed_work, lcd_init_on_delayed_work);
	INIT_DELAYED_WORK(&pdrv->test_delayed_work, lcd_auto_test_delayed);

	ret = lcd_cdev_add(pdrv, &pdev->dev);
	if (ret)
		goto lcd_probe_err_2;

	lcd_bootargs_ctrl_load(pdrv);
	ret = lcd_config_probe(pdrv, pdev);
	if (ret)
		goto lcd_probe_err_2;

	LCD_PR(pdrv, "%s ok, init_state:0x%x", __func__, lcd_drv_init_state);

	return 0;

lcd_probe_err_2:
	lcd_cdev_remove(pdrv);
lcd_probe_err_1:
	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	lcd_driver[index] = NULL;
	/* free drv */
	kfree(pdrv);
lcd_probe_err_0:
	lcd_drv_init_state &= ~(1 << index);
	LCDPR("[%d]: %s failed\n", index, __func__);
	return ret;
}

static void lcd_remove(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);
	int index;

	if (!pdrv)
		return;

	lcd_drm_remove(pdrv->dev);

	index = pdrv->index;

	cancel_work_sync(&pdrv->late_resume_work);
	cancel_work_sync(&pdrv->mode_switch_on_work);
	cancel_delayed_work(&pdrv->init_on_delayed_work);
	if (lcd_workqueue)
		destroy_workqueue(lcd_workqueue);

	lcd_vsync_irq_remove(pdrv);
	lcd_cdev_remove(pdrv);
	lcd_debug_remove_basic(pdrv);
	lcd_config_remove(pdrv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);

	kfree(pdrv->drv_res.reg_map);
	kfree(pdrv);
	lcd_driver[index] = NULL;
	lcd_drv_init_state &= ~(1 << index);
	lcd_global_remove_once();

	LCDPR("[%d]: %s, init_state:0x%x\n", index, __func__, lcd_drv_init_state);
}

#ifdef CONFIG_PM
static int lcd_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	if ((pdrv->status & LCD_STATE_VMODE_ACTIVE) == 0) {
		LCD_DBG(pdrv, "%s: vmode inactive, exit. status=0x%x", __func__, pdrv->status);
		return 0;
	}

	mutex_lock(&lcd_power_mutex);
	LCD_PR(pdrv, "%s, status=0x%x", __func__, pdrv->status);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
	pdrv->status |= LCD_STATE_PREPARE;
	lcd_power_if_early_on(pdrv);
	LCD_PR(pdrv, "%s finished, status=0x%x", __func__, pdrv->status);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	mutex_lock(&lcd_power_mutex);
	pdrv->init_flag = 0;
	if (pdrv->status & LCD_STATUS_IF_ON) {
		LCD_ERR(pdrv, "%s: lcd_if is still enabled, status=0x%x", __func__, pdrv->status);
	}

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		pdrv->status &= ~LCD_STATE_PREPARE;
		aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, (void *)pdrv);
		LCD_PR(pdrv, "%s finished, status=0x%x", __func__, pdrv->status);
	}
	mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int lcd_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	mutex_lock(&lcd_power_mutex);
	pdrv->init_flag = 0;
	if (pdrv->status & LCD_STATUS_IF_ON) {
		lcd_proc_time_clear(pdrv);
		pdrv->status &= ~LCD_STATE_POWER;
		aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF, (void *)pdrv);
	}

	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		pdrv->status &= ~LCD_STATE_PREPARE;
		aml_lcd_notifier_call_chain(LCD_EVENT_UNPREPARE, (void *)pdrv);
	}
	LCD_PR(pdrv, "lcd_freeze done: status=0x%x", pdrv->status);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_thaw(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	if ((pdrv->status & LCD_STATE_VMODE_ACTIVE) == 0) {
		LCD_DBG(pdrv, "lcd_thaw: vmode inactive, exit. status=0x%x", pdrv->status);
		return 0;
	}

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
	pdrv->status |= LCD_STATE_PREPARE;
	lcd_power_if_early_on(pdrv);
	LCD_PR(pdrv, "lcd_thaw done: status=0x%x", pdrv->status);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_poweroff(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	mutex_lock(&lcd_power_mutex);
	pdrv->init_flag = 0;
	lcd_proc_time_clear(pdrv);

	pdrv->status &= ~(LCD_STATE_PREPARE | LCD_STATE_POWER);
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF | LCD_EVENT_UNPREPARE, (void *)pdrv);

	LCD_PR(pdrv, "lcd_poweroff done: status=0x%x", pdrv->status);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static int lcd_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_lcd_drv_s *pdrv;

	pdrv = platform_get_drvdata(pdev);
	if (!pdrv)
		return 0;

	if ((pdrv->status & LCD_STATE_VMODE_ACTIVE) == 0) {
		LCD_DBG(pdrv, "lcd_restore: vmode inactive, exit. status=0x%x", pdrv->status);
		return 0;
	}

	mutex_lock(&lcd_power_mutex);
	aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
	pdrv->status |= LCD_STATE_PREPARE;
	lcd_power_if_early_on(pdrv);
	LCD_PR(pdrv, "lcd_restore done: status=0x%x", pdrv->status);
	mutex_unlock(&lcd_power_mutex);

	return 0;
}

static const struct dev_pm_ops lcd_pm = {
	.suspend  = lcd_suspend,
	.resume   = lcd_resume,
	.freeze   = lcd_freeze,
	.thaw     = lcd_thaw,
	.poweroff = lcd_poweroff,
	.restore  = lcd_restore,
};
#endif

static void lcd_shutdown(struct platform_device *pdev)
{
	struct aml_lcd_drv_s *pdrv = platform_get_drvdata(pdev);

	if (!pdrv)
		return;

	LCD_DBG(pdrv, "lcd_debug_print: %s", __func__);

	pdrv->init_flag = 0;
	pdrv->enter_shutdown = 1;
	if (pdrv->status & LCD_STATUS_ENCL_ON) {
		pdrv->status &= ~(LCD_STATE_PREPARE | LCD_STATE_POWER);
		aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)pdrv);
	}
	pdrv->enter_shutdown = 0;
}

static struct platform_driver lcd_platform_driver = {
	.probe = lcd_probe,
	.remove = lcd_remove,
	.shutdown = lcd_shutdown,
	.driver = {
		.name = "mesonlcd",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &lcd_pm,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lcd_dt_match_table),
#endif
	},
};

int __init lcd_init(void)
{
	if (platform_driver_register(&lcd_platform_driver)) {
		LCDERR("failed to register lcd driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit lcd_exit(void)
{
	platform_driver_unregister(&lcd_platform_driver);
}

static void lcd_boot_ctrl_dft_prepare(struct lcd_boot_ctrl_s *boot_ctrl)
{
	switch (boot_ctrl->init_level) {
	case LCD_INIT_LEVEL_PREBOOT:
		boot_ctrl->if_state = 1;
		boot_ctrl->bl_state = 0;
		break;
	case LCD_INIT_LEVEL_PWR_OFF:
		boot_ctrl->if_state = 0;
		boot_ctrl->bl_state = 0;
		break;
	case LCD_INIT_LEVEL_KERNEL_ON:
		boot_ctrl->if_state = 0;
		boot_ctrl->bl_state = 0;
		break;
	case LCD_INIT_LEVEL_NORMAL:
	default:
		boot_ctrl->if_state = 1;
		boot_ctrl->bl_state = 1;
		break;
	}
}

// lcdX={lcd_ctrl},{name},{type}
static int lcd_boot_str_setup(unsigned char idx, char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	const char *ptr, *ptr2;
	char temp_str[12];
	struct lcd_boot_ctrl_s *boot_ctrl = &lcd_boot_ctrl_config[idx];

	if (!str)
		return -EINVAL;

	ptr = strstr(str, ",");
	if (!ptr || ptr == str) {
		LCDERR("[%u]: %s: invalid boot ctrl str: %s\n", idx, __func__, str);
		return -EINVAL;
	}
	snprintf(temp_str, (ptr - str > 11) ? 11 : ptr - str + 1, "%s", str);
	ret = kstrtouint(temp_str, 16, &data32);
	if (ret) {
		LCDERR("[%u]: %s: invalid data\n", idx, __func__);
		return -EINVAL;
	}

	ptr++;
	ptr2 = strstr(ptr, ",");
	if (ptr2) {
		//if (ptr2 > ptr)
		//	snprintf(lcd_panel_name[idx], ptr2 - ptr + 1, "%s", ptr);
		ptr2++;
		if (str + strlen(str) > ptr2)
			snprintf(lcd_propname[idx], 24, "%s", ptr2);
	}

	boot_ctrl->lcd_type = data32 & 0xf;
	boot_ctrl->lcd_bits = (data32 >> 4) & 0xf;
	boot_ctrl->advanced_flag = (data32 >> 8) & 0xff;
	boot_ctrl->dccd_flag = (data32 >> 17) & 0x1;
	boot_ctrl->init_level = (data32 >> 18) & 0x3;
	boot_ctrl->ppc = (data32 >> 20) & 0x3;
	boot_ctrl->clk_mode = (data32 >> 22) & 0x3;
	boot_ctrl->frame_rate = ((data32 >> 24) & 0xff) | (((data32 >> 16) & 0x1) << 8);

	lcd_boot_ctrl_dft_prepare(boot_ctrl);

	LCDPR("[%u]: [0x%08x] [%s]\n", idx, data32, lcd_propname[idx]);
	//LCDPR("[%u]: [0x%08x] [%s] [%s]\n", idx, data32, lcd_panel_name[idx], lcd_propname[idx]);

	return 0;
}

static int lcd0_boot_setup(char *str)
{
	return lcd_boot_str_setup(0, str);
}

static int lcd1_boot_setup(char *str)
{
	return lcd_boot_str_setup(1, str);
}

static int lcd2_boot_setup(char *str)
{
	return lcd_boot_str_setup(2, str);
}

static int lcd_debug_ctrl_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	struct lcd_debug_ctrl_s *debug_ctrl = &lcd_debug_ctrl_config;

	if (!str)
		return -EINVAL;

	ret = kstrtouint(str, 16, &data32);
	if (ret) {
		LCDERR("%s:invalid data\n", __func__);
		return -EINVAL;
	}

	LCD_PR(NULL, "debug_ctrl: 0x%08x", data32);
	debug_ctrl->debug_print_flag = data32 & 0xffff;
	debug_ctrl->debug_test_pattern = (data32 >> 16) & 0xf;
	debug_ctrl->debug_para_source = (data32 >> 28) & 0x3;
	debug_ctrl->debug_lcd_mode = (data32 >> 30) & 0x3;
	lcd_debug_print_flag = debug_ctrl->debug_print_flag;

	return 1;
}

__setup("lcd0=", lcd0_boot_setup);
__setup("lcd1=", lcd1_boot_setup);
__setup("lcd2=", lcd2_boot_setup);
__setup("lcd_debug=", lcd_debug_ctrl_setup);

//MODULE_DESCRIPTION("Meson LCD Panel Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
