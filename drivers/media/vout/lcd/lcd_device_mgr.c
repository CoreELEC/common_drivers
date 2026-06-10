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
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
#endif
#include "./lcd_common.h"

struct aml_lcd_device_s *lcd_device_append_new(struct aml_lcd_drv_s *pdrv, char *device_cfg_name)
{
	int ret = -1;
	struct aml_lcd_device_s *dev_p;
	u8 i = 0;

	for (i = 0; i < LCD_MULTI_DEV_MAX_CNT; i++) {
		if (!pdrv->device_mgr.dev_list[i])
			break;
	}
	if (i == LCD_MULTI_DEV_MAX_CNT) {
		LCD_ERR(pdrv, "MAX supported device count (%u) reached", LCD_MULTI_DEV_MAX_CNT);
		return NULL;
	}

	LCD_DEV_DBG(pdrv, i, "create new lcd device");
	dev_p = kzalloc(sizeof(*dev_p), GFP_KERNEL);
	if (!dev_p) {
		kfree(dev_p);
		return NULL;
	}

	/* default config */
	strscpy(dev_p->dev_propname, device_cfg_name, 24);
	dev_p->dev_cfg.basic.lcd_type = LCD_TYPE_MAX;
	dev_p->dev_cfg.power.power_on_step[0].type = LCD_POWER_TYPE_MAX;
	dev_p->dev_cfg.power.power_off_step[0].type = LCD_POWER_TYPE_MAX;
	strscpy(dev_p->dev_cfg.cus_pinmux_name, "null", CUS_PINMUX_NAME_MAX);
	dev_p->dev_cfg.backlight_index = 0xff;

	ret = lcd_load_device_config(pdrv, dev_p, device_cfg_name);
	if (ret) {
		LCD_DEV_ERR(pdrv, i, "device load config[%s] failed", device_cfg_name);
		kfree(dev_p);
		return NULL;
	}

	pdrv->device_mgr.dev_list[i] = dev_p;
	pdrv->device_mgr.dev_list[i]->idx = i;
	pdrv->device_mgr.dev_cnt++;

	return dev_p;
}

void lcd_device_pop_last(struct aml_lcd_drv_s *pdrv)
{
	u8 i = 0;
	struct aml_lcd_device_s *dev_p;

	for (i = LCD_MULTI_DEV_MAX_CNT - 1; i; i--) {
		if (!pdrv->device_mgr.dev_list[i])
			continue;
		dev_p = pdrv->device_mgr.dev_list[i];
		LCD_DEV_PR(pdrv, dev_p->idx, "remove last lcd device (%s)", dev_p->dev_propname);

		kfree(dev_p);
		pdrv->device_mgr.dev_cnt--;
		pdrv->device_mgr.dev_list[i] = NULL;
		return;
	}
	LCD_DEV_ERR(pdrv, 0, "no device consist");
}

struct aml_lcd_device_s *lcd_device_assign(struct aml_lcd_drv_s *pdrv, unsigned char dev_idx)
{
	if (dev_idx >= LCD_MULTI_DEV_MAX_CNT)
		dev_idx = 0;

	if (pdrv->device_mgr.dev_list[dev_idx]) {
		pdrv->curr_dev = pdrv->device_mgr.dev_list[dev_idx];
		LCD_DEV_PR(pdrv, dev_idx, "driver assign to (%s)",
			pdrv->curr_dev->dev_propname);
		return pdrv->device_mgr.dev_list[dev_idx];
	}

	LCD_DEV_ERR(pdrv, dev_idx, "device unavailable");
	pdrv->curr_dev = NULL;

	return NULL;
}

void lcd_device_list(struct aml_lcd_drv_s *pdrv)
{
	u8 i = 0;
	struct aml_lcd_device_s *dev_p;

	for (i = 0; i < LCD_MULTI_DEV_MAX_CNT; i++) {
		if (!pdrv->device_mgr.dev_list[i])
			break;
		dev_p = pdrv->device_mgr.dev_list[i];
		LCD_DEV_PR(pdrv, dev_p->idx, "config info: (%s)---------", dev_p->dev_propname);

		lcd_config_load_print(pdrv, dev_p);
	}
}

void lcd_device_switch(struct aml_lcd_drv_s *pdrv, unsigned char target_dev_idx)
{
	if (target_dev_idx >= LCD_MULTI_DEV_MAX_CNT)
		target_dev_idx = 0;

	if (pdrv->device_mgr.dev_list[target_dev_idx]) {
		pdrv->curr_dev = pdrv->device_mgr.dev_list[target_dev_idx];
		LCD_DEV_PR(pdrv, target_dev_idx, "assign to (%s)",
			pdrv->curr_dev->dev_propname);

		return;
	}

	LCD_DEV_ERR(pdrv, target_dev_idx, "device unavailable");
	pdrv->curr_dev = NULL;
}
