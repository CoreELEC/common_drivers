// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>
#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#endif
#ifdef CONFIG_AMLOGIC_eDPTX
#include <linux/amlogic/media/vout/eDPTX/eDPTX_export.h>
#endif
#include "lcd_bl.h"

int bl_strnum_get_num(const char *str, struct num_str_s *arr, int size_arr, int dft)
{
	int i = 0;

	if (!str || !arr)
		return dft;

	for (i = 0; i < size_arr; i++) {
		if (strcmp(str, arr[i].str) == 0)
			return arr[i].num;
	}
	return dft;
}

void bl_delay_us(u32 us)
{
	if (!us)
		return;
	else if (us <= 20)
		udelay(us);
	else if (us < 1000)
		usleep_range(us, us + 2);
	else if (us < 20000)
		usleep_range(us, us + 100);
	else
		msleep(us / 1000);
}

void bl_delay_ms(u32 ms)
{
	if (!ms)
		return;
	else if (ms < 20)
		usleep_range(ms * 1000, ms * 1000 + 100);
	else
		msleep(ms);
}

#ifdef CONFIG_AMLOGIC_LCD
void bl_display_info_acquire_from_lcd(struct aml_bl_drv_s *bdrv, struct b_disp_info *disp_info)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (pdrv) {
		disp_info->h_active = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
		disp_info->v_active = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
		disp_info->frame_rate = pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate;
		disp_info->line_time_ns = pdrv->curr_dev->dev_cfg.timing.act_timing.line_time_ns;
		disp_info->current_line = aml_lcd_get_encl_line_cnt(bdrv->index & 0xf);
		disp_info->encl_frm_cnt = aml_lcd_get_encl_frm_cnt(bdrv->index & 0xf);
		disp_info->max_line = aml_lcd_get_max_line_cnt(bdrv->index & 0xf);
		disp_info->display_status = pdrv->status;
	}
}
#endif

#ifdef CONFIG_AMLOGIC_eDPTX
void bl_display_info_acquire_from_edp(struct aml_bl_drv_s *bdrv, struct b_disp_info *disp_info)
{
	struct edptx_drv_s *edp;

	edp = aml_edptx_get_driver(bdrv->index & 0xf);
	if (edp) {
		disp_info->h_active = edp->act_timing.h_act;
		disp_info->v_active = edp->act_timing.v_act;
		disp_info->frame_rate = edp->act_timing.fr1000 / 1000;
		disp_info->line_time_ns = 1000;
		disp_info->current_line = 0;
		disp_info->encl_frm_cnt = 1000;
		disp_info->max_line = edp->act_timing.v_period;
		disp_info->display_status = edp->status & 0x10 ? 0x3 : 0x0;
	}
}
#endif

void bl_display_info_acquire(struct aml_bl_drv_s *bdrv, struct b_disp_info *disp_info)
{
	memset(disp_info, 0, sizeof(struct b_disp_info));

	if ((bdrv->bl_to_connector >> 4) == 1) {
#ifdef CONFIG_AMLOGIC_eDPTX
		bl_display_info_acquire_from_edp(bdrv, disp_info);
		//BLPR(bdrv->index, "from edp 0x%x", disp_info->display_status);
#endif
	} else {
#ifdef CONFIG_AMLOGIC_LCD
		bl_display_info_acquire_from_lcd(bdrv, disp_info);
		//BLPR(bdrv->index, "from lcd 0x%x", disp_info->display_status);
#endif
	}
}

u32 aml_bl_off_time_report(u8 bl_idx)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(bl_idx & 0xf);

	if (bdrv)
		return bdrv->bl_off_time;
	return 0;
}

u32 aml_bl_on_time_report(u8 bl_idx)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(bl_idx & 0xf);

	if (bdrv)
		return bdrv->bl_on_time;
	return 0;
}
