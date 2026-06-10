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
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <drm/amlogic/meson_connector_dev.h>
#ifdef CONFIG_AMLOGIC_BACKLIGHT
// #include <linux/amlogic/media/vout/backlight/aml_bl.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>
#endif
#include "./lcd_reg.h"
#include "./lcd_common.h"
#include <linux/sched/clock.h>
#include "./connectors/lcd_connector.h"

static int lcd_mode_get_vframe_rate_hint(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return 0;

	return pdrv->fr_duration;
}

static void lcd_mode_vout_debug_test(unsigned int num, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return;

	lcd_debug_test(pdrv, num);
}

static void lcd_mode_vout_set_bl_brightness(int brightness, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	aml_bl_set_level_brightness(pdrv->index, brightness);
#endif
}

static int lcd_mode_vout_get_bl_brightness(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	int ret = 0;

	if (!pdrv)
		return 0;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	ret = aml_bl_get_level_brightness(pdrv->index);
#endif
	return ret;
}

static int lcd_mode_suspend(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_power_mutex);
	lcd_proc_time_clear(pdrv);
	pdrv->init_flag = 0;
	pdrv->status &= ~LCD_CTRL_POWER;
	pdrv->status |= LCD_CTRL_DUMMY;
	aml_lcd_notifier_call_chain(LCD_EVENT_POWER_OFF | LCD_EVENT_ENCL_DUMMY, (void *)pdrv);
	LCD_PR(pdrv, "early_suspend finished, status=0x%x", pdrv->status);
	mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int lcd_mode_resume(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	if ((pdrv->status & LCD_CTRL_VMODE_ACTIVE) == 0)
		return 0;

	if (pdrv->status & LCD_CTRL_RESUME_DRCT) {
		LCD_PR(pdrv, "directly lcd late_resume, status=0x%x", pdrv->status);
		lcd_late_resume(pdrv);
	} else {
		lcd_queue_delayed_work(&pdrv->late_resume_dly_work, 0);
	}

	return 0;
}

static struct vinfo_s *lcd_mode_get_current_info(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return NULL;
	return &pdrv->vinfo;
}

static int lcd_mode_check_same_vmodeattr(char *mode, void *data)
{
	return 1;
}

static int lcd_mode_vmode_is_supported(enum vmode_e mode, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	mode &= VMODE_MODE_BIT_MASK;
	LCD_DBG(pdrv, "mode check: vmode=%d lcd_vmode=%d", mode, VMODE_LCD);

	if (mode == VMODE_LCD)
		return true;

	return false;
}

static int lcd_mode_vout_set_state(int index, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_vout_mutex);
	pdrv->vout_state |= (1 << index);
	LCD_DBG(pdrv, "viu set: [%d -> %d]", pdrv->viu_sel, index);
	pdrv->viu_sel = index;
	mutex_unlock(&lcd_vout_mutex);

	return 0;
}

static int lcd_mode_vout_clr_state(int index, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	mutex_lock(&lcd_vout_mutex);
	pdrv->vout_state &= ~(1 << index);
	LCD_PR(pdrv, "viu clr: viu=%d clear=%d", pdrv->viu_sel, index);
	if (pdrv->viu_sel == index)
		pdrv->viu_sel = LCD_VIU_SEL_NONE;
	mutex_unlock(&lcd_vout_mutex);

	return 0;
}

static int lcd_mode_vout_get_state(void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	return pdrv->vout_state;
}

int lcd_vmode_remove_all(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_vmode_list_s *cur_list;
	struct lcd_vmode_list_s *next_list;

	cur_list = pdrv->vmode_mgr.vmode_list_header;
	while (cur_list) {
		next_list = cur_list->next;
		kfree(cur_list->info);
		kfree(cur_list);
		cur_list = next_list;
	}
	pdrv->vmode_mgr.vmode_list_header = NULL;
	pdrv->vmode_mgr.cur_vmode_info = NULL;
	pdrv->vmode_mgr.next_vmode_info = NULL;
	pdrv->vmode_mgr.vmode_cnt = 0;

	return 0;
}

static int lcd_mode_timing_switch_update(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *ptiming;
	unsigned int pre_pclk;
	unsigned char switch_type, switch_type_pre;
	unsigned long flags = 0;

	if (!pdrv->curr_dev || !pdrv->curr_dev->dev_cfg.timing.base_timing)
		return -1;
	switch_type_pre = pdrv->curr_dev->dev_cfg.timing.act_timing.switch_type;
	/* clear clk_change flag */
	pdrv->curr_dev->dev_cfg.timing.clk_change &= ~(LCD_CLK_PLL_RESET);
	if (!pdrv->vmode_mgr.next_vmode_info)
		return 0;

	spin_lock_irqsave(&pdrv->sw_vrr.set_lock, flags);
	pdrv->vmode_switch = 1;
	spin_unlock_irqrestore(&pdrv->sw_vrr.set_lock, flags);

	pre_pclk = pdrv->curr_dev->dev_cfg.timing.base_timing->pixel_clk;
	pdrv->vmode_mgr.cur_vmode_info = pdrv->vmode_mgr.next_vmode_info;
	pdrv->vmode_mgr.next_vmode_info = NULL;

	pdrv->std_duration = pdrv->vmode_mgr.cur_vmode_info->duration;
	ptiming = pdrv->vmode_mgr.cur_vmode_info->dft_timing;
	pdrv->curr_dev->dev_cfg.timing.base_timing = ptiming;

	lcd_base_to_act_timing_init_config(pdrv);
	lcd_sw_vrr_disable_reset(pdrv);
	if (pdrv->curr_dev->dev_cfg.timing.base_timing->pixel_clk != pre_pclk) {
		pdrv->curr_dev->dev_cfg.timing.clk_change |=
			(LCD_CLK_PLL_RESET | LCD_CLK_PLL_CHANGE);
		lcd_clk_generate_parameter(pdrv);
	}

	switch_type = pdrv->curr_dev->dev_cfg.timing.act_timing.switch_type;
	switch (switch_type_pre) {
	case LCD_VMODE_SWITCH_FULL:
		pdrv->curr_dev->dev_cfg.timing.switch_type = switch_type_pre;
		break;
	case LCD_VMODE_SWITCH_LIMIT:
		if (switch_type == LCD_VMODE_SWITCH_FULL)
			pdrv->curr_dev->dev_cfg.timing.switch_type = LCD_VMODE_SWITCH_FULL;
		else
			pdrv->curr_dev->dev_cfg.timing.switch_type = switch_type_pre;
		break;
	case LCD_VMODE_SWITCH_MIN:
	case LCD_VMODE_SWITCH_MIN_WO_TCON_RST:
		if (switch_type == LCD_VMODE_SWITCH_FULL)
			pdrv->curr_dev->dev_cfg.timing.switch_type = LCD_VMODE_SWITCH_FULL;
		else if (switch_type == LCD_VMODE_SWITCH_LIMIT)
			pdrv->curr_dev->dev_cfg.timing.switch_type = LCD_VMODE_SWITCH_LIMIT;
		else
			pdrv->curr_dev->dev_cfg.timing.switch_type = switch_type_pre;
		break;
	default:
		pdrv->curr_dev->dev_cfg.timing.switch_type = switch_type;
		break;
	}
	LCD_DBG(pdrv, "%s: switch_type: pre: %d, cur: %d, final: %d, lcd_status: 0x%x",
		__func__, switch_type_pre, switch_type,
		pdrv->curr_dev->dev_cfg.timing.switch_type, pdrv->status);

	if (pdrv->curr_dev->dev_cfg.timing.switch_type_dbg) {
		pdrv->curr_dev->dev_cfg.timing.switch_type =
			pdrv->curr_dev->dev_cfg.timing.switch_type_dbg;
		LCD_PR(pdrv, "%s: switch_type: force dbg to final: %d",
			__func__, pdrv->curr_dev->dev_cfg.timing.switch_type);
	}

	switch (pdrv->curr_dev->dev_cfg.timing.switch_type) {
	case LCD_VMODE_SWITCH_MIN:
	case LCD_VMODE_SWITCH_MIN_WO_TCON_RST:
		pdrv->switch_off_event = LCD_EVENT_MDSW_MIN_OFF;
		pdrv->switch_on_event = LCD_EVENT_MDSW_MIN_ON;
		break;
	case LCD_VMODE_SWITCH_LIMIT:
		pdrv->switch_off_event = LCD_EVENT_MDSW_LIMIT_OFF;
		pdrv->switch_on_event = LCD_EVENT_MDSW_LIMIT_ON;
		break;
	case LCD_VMODE_SWITCH_FULL:
	default:
		pdrv->switch_off_event = LCD_EVENT_POWER_OFF;
		pdrv->switch_on_event = LCD_EVENT_POWER_ON;
		break;
	}

	return 0;
}

void lcd_mode_driver_dummy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	int enc_st = lcd_get_venc_state(pdrv);

	//discontinuous clk and vsync, no need disable venc
	if (enc_st) {
		lcd_venc_enable(pdrv, 0);
		lcd_delay_ms(30);
	}
	if (status) {
		lcd_clk_set_dummy(pdrv);
		lcd_set_venc_dummy(pdrv, 1);
		pdrv->status |= LCD_STATUS_ENCL_DUMMY;
	} else {
		lcd_set_clk(pdrv);
		lcd_set_venc_dummy(pdrv, 0);
		pdrv->status &= ~LCD_STATUS_ENCL_DUMMY;
	}
	if (enc_st) {
		lcd_delay_ms(10);
		lcd_venc_enable(pdrv, 1);
	}
}

static void lcd_mode_vmode_timing_set(struct aml_lcd_drv_s *pdrv)
{
	int enc_st = lcd_get_venc_state(pdrv);
	unsigned long long local_time[2];

	mutex_lock(&lcd_vout_mutex);
	if (pdrv->status & LCD_STATUS_ENCL_DUMMY) {
		mutex_unlock(&lcd_vout_mutex);
		return;
	}

	local_time[0] = sched_clock();

	LCD_PR(pdrv, "vmode_timing_set(ver %s): %s",
	      LCD_DRV_VERSION, lcd_type_type_to_str(pdrv->curr_dev->dev_cfg.basic.lcd_type));

	if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE)
			lcd_vbyone_interrupt_enable(pdrv, 0);
#endif
	}

	//discontinuous clk and vsync, no need disable venc
	if (enc_st) {
		lcd_venc_enable(pdrv, 0);
		lcd_delay_ms(30);
	}
	lcd_clk_change(pdrv);
	lcd_set_venc_timing(pdrv);
	if (enc_st) {
		lcd_delay_ms(10);
		lcd_venc_enable(pdrv, 1);
	}

	if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE) {
			lcd_vbyone_wait_stable(pdrv);
			lcd_vbyone_interrupt_enable(pdrv, 1);
		}
#endif
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.timing_change_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	mutex_unlock(&lcd_vout_mutex);
}

static void lcd_mode_vmode_switch(struct aml_lcd_drv_s *pdrv)
{
	unsigned long long local_time[2];
	unsigned long flags = 0;

	if (pdrv->curr_dev->dev_cfg.timing.switch_type == LCD_VMODE_SWITCH_NONE)
		return;

	//lock bist, input data change is abandoned
	spin_lock_irqsave(&pdrv->isr_lock, flags);
	pdrv->test_locked = 1;
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	//step 1: switch off
	local_time[0] = sched_clock();
	if (pdrv->status & LCD_CTRL_POWER) {
		/* include lcd_vout_mutex */
		aml_lcd_notifier_call_chain(pdrv->switch_off_event, (void *)pdrv);
	}
	local_time[1] = sched_clock();
	pdrv->proc_time.switch_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	//step 2: driver change
	if (pdrv->status & LCD_CTRL_PREPARE)
		lcd_mode_vmode_timing_set(pdrv);

	//step 3: switch on
	lcd_queue_work(&pdrv->mode_switch_on_work);
}

void lcd_mode_auto_framerate_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int clk_change = 0;
	unsigned long long local_time[2];

	if (!pdrv || !pdrv->curr_dev)
		return;

	LCD_DBG(pdrv, "auto_framerate_set(ver %s): %s, status:0x%x",
		LCD_DRV_VERSION,
		lcd_type_type_to_str(pdrv->curr_dev->dev_cfg.basic.lcd_type),
		pdrv->status);
	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return;
	if (pdrv->status & LCD_STATUS_ENCL_DUMMY)
		return;

	local_time[0] = sched_clock();

	//continuous clk and vsync, no need disable venc
	clk_change = pdrv->curr_dev->dev_cfg.timing.clk_change;
	if (clk_change & LCD_CLK_CHANGE) {
		if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
			if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE)
				lcd_vbyone_interrupt_enable(pdrv, 0);
#endif
		}
		lcd_clk_change(pdrv);
	}

	lcd_venc_fr_change(pdrv);

	if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE) {
			if (clk_change & LCD_CLK_PLL_CHANGE)
				lcd_vbyone_wait_stable(pdrv);
			if (clk_change & LCD_CLK_CHANGE)
				lcd_vbyone_interrupt_enable(pdrv, 1);
		}
#endif
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.timing_change_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

static struct lcd_duration_s lcd_std_fr[] = {
	{360, 360,    1,  },
	{340, 340,    1,  },
	{330, 330,    1,  },
	{288, 288,    1,  },
	{240, 240,    1,  },
	{239, 240000, 1001},
	{200, 200,    1,  },
	{192, 192,    1,  },
	{191, 192000, 1001},
	{180, 180,    1,  },
	{170, 170,    1,  },
	{165, 165,    1,  },
	{144, 144,    1,  },
	{120, 120,    1,  },
	{119, 120000, 1001},
	{100, 100,    1,  },
	{96,  96,     1,  },
	{95,  96000,  1001},
	{60,  60,     1,  },
	{59,  60000,  1001},
	{50,  50,     1,  },
	{48,  48,     1,  },
	{47,  48000,  1001},
};

static int lcd_framerate_auto_std_duration_index(struct aml_lcd_drv_s *pdrv,
						 struct lcd_duration_s *dur_table,
						 int size, int duration)
{
	unsigned int frame_rate;
	int i;

	for (i = 0; i < size; i++) {
		frame_rate = dur_table[i].duration_num * 100 / dur_table[i].duration_den;
		if (duration == frame_rate)
			return i;
	}

	return 0xff;
}

static int lcd_outputmode_str_check(struct aml_lcd_drv_s *pdrv, const char *mode)
{
	struct lcd_vmode_list_s *temp_list = pdrv->vmode_mgr.vmode_list_header;
	unsigned short h, v, fr;
	u8 vmode_list_idx = 0;
	int i;

	if (!mode || !temp_list)
		return -1;

	i = str_parse_vmode(mode, &h, &v, &fr);
	if (!i) {
		LCD_ERR(pdrv, "invalid str parse: %s", mode);
		return -1;
	}

	while (temp_list) {
		if (!(h == temp_list->info->width && v == temp_list->info->height)) {
			temp_list = temp_list->next;
			continue;
		}

		if (fr == temp_list->info->base_fr && pdrv->mode == LCD_MODE_TABLET) {
			temp_list->info->duration_index = 0xff;
			if (pdrv->vmode_mgr.cur_vmode_info != temp_list->info)
				pdrv->vmode_mgr.next_vmode_info = temp_list->info;

			LCD_DBG(pdrv, "mode check: list[%u] %dx%d@%dhz",
				vmode_list_idx,
				temp_list->info->width, temp_list->info->height,
				temp_list->info->base_fr);
			return 0;
		}

		for (i = 0; i < LCD_DURATION_MAX; i++) {
			if (temp_list->info->duration[i].frame_rate == 0)
				break;
			if (fr != temp_list->info->duration[i].frame_rate)
				continue;

			LCD_DBG(pdrv, "mode check: list[%u] %dx%d@%dhz",
				vmode_list_idx,
				temp_list->info->width, temp_list->info->height,
				temp_list->info->duration[i].frame_rate);

			temp_list->info->duration_index = i;
			if (pdrv->vmode_mgr.cur_vmode_info != temp_list->info)
				pdrv->vmode_mgr.next_vmode_info = temp_list->info;
			return 0;
		}
		temp_list = temp_list->next;
		vmode_list_idx++;
	}

	LCD_ERR(pdrv, "mode check invalid: %s", mode);
	return -1;
}

/* ************************************************** *
 * vout server api
 * **************************************************
 */
enum vmode_e lcd_mode_validate_vmode(char *mode, unsigned int frac, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	int ret;

	if (!pdrv || !mode)
		return VMODE_MAX;

	if (lcd_vout_serve_bypass) {
		LCD_PR(pdrv, "vout_serve bypass");
		return VMODE_MAX;
	}

	LCD_DBG(pdrv, "mode validate: display_mode=%s, drv_mode=%s", mode, pdrv->vinfo.name);

	mutex_lock(&lcd_vout_mutex);
	ret = lcd_outputmode_str_check(pdrv, mode);
	if (ret == 0) {
		mutex_unlock(&lcd_vout_mutex);
		return VMODE_LCD;
	}

	mutex_unlock(&lcd_vout_mutex);
	return VMODE_MAX;
}

static int lcd_framerate_support_check(struct aml_lcd_drv_s *pdrv, int duration)
{
	//unsigned int fr, ht, vt;
	//unsigned char i;
	struct lcd_detail_timing_s *dtm = &pdrv->curr_dev->dev_cfg.timing.act_timing;

	//if (dtm->fixed_type != 0xff) {
	//	for (i = 0; i < 8; i++) {
	//		if (dtm->fixed_val_set[i] == 0)
	//			break;
	//		if (n >= LCD_DURATION_MAX)
	//			break;
	//		fr = (dtm->fixed_type == 0) ? dtm->fixed_val_set[i] : dtm->pixel_clk;
	//		ht = (dtm->fixed_type == 1) ? dtm->fixed_val_set[i] : dtm->h_period;
	//		vt = (dtm->fixed_type == 2) ? dtm->fixed_val_set[i] : dtm->v_period;
	//		fr = ((fr / ht) * 100) / vt;
	//		if (fr == duration)
	//			return 1;
	//	}
	//}

	if ((duration <= dtm->frame_rate_max * 100) && (duration >= dtm->frame_rate_min * 100))
		return 1;

	return 0;
}

int lcd_mode_set_vframe_rate_hint(int duration, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct lcd_config_s *pconf;
	struct vinfo_s *info;
	unsigned int duration_num = 60, duration_den = 1, duration_frac = 0;
	struct lcd_duration_s *set_dur;
	u8 find, duration_in_range;
	unsigned int frame_rate;

	if (!pdrv || !pdrv->curr_dev)
		return -1;
	if (pdrv->probe_done == 0)
		return -1;

	if (lcd_vout_serve_bypass) {
		LCD_PR(pdrv, "vout_serve bypass");
		return 0;
	}
	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
		LCD_PR(pdrv, "fr hint: lcd is disabled");
		return -1;
	}

	pconf = &pdrv->curr_dev->dev_cfg;
	if (lcd_fr_is_fixed(pdrv)) {
		LCD_PR(pdrv, "fr hint: fixed timing");
		return -1;
	}
	if (pconf->fr_auto_flag == 0) {
		LCD_PR(pdrv, "fr hint: fr_auto_flag = 0");
		return 0;
	}

	mutex_lock(&lcd_vout_mutex);

	info = &pdrv->vinfo;

	if (duration == 0) { /* end hint */
		LCD_PR(pdrv, "fr hint: return mode: %s, fr_adj_type: %d",
			info->name, pconf->timing.act_timing.fr_adjust_type);

		pdrv->fr_duration = 0;
		if (pdrv->fr_mode == 0) {
			LCD_PR(pdrv, "fr hint: fr_mode is invalid");
			mutex_unlock(&lcd_vout_mutex);
			return 0;
		}

		/* update target frame rate */
		if (pconf->timing.base_timing) {
			pconf->timing.frame_rate = pconf->timing.base_timing->frame_rate;
			pconf->timing.sync_duration_num =
				pconf->timing.base_timing->sync_duration_num;
			pconf->timing.sync_duration_den =
				pconf->timing.base_timing->sync_duration_den;
			pconf->timing.frac = pconf->timing.base_timing->frac;
			pdrv->fr_mode = 0;
		}
	} else {
		// for tv: only find pdrv->std_duration
		// for tablet: find lcd_std_fr, if no, check in range
		if (pdrv->mode == LCD_MODE_TV) {
			find = lcd_framerate_auto_std_duration_index(pdrv,
				pdrv->std_duration, LCD_DURATION_MAX, duration);
			duration_in_range = 0;
			if (find != 0xff)
				set_dur = &pdrv->std_duration[find];
		} else { //tablet
			find = lcd_framerate_auto_std_duration_index(pdrv,
				lcd_std_fr, ARRAY_SIZE(lcd_std_fr), duration);
			duration_in_range = lcd_framerate_support_check(pdrv, duration);
			if (find != 0xff)
				set_dur = &lcd_std_fr[find];
		}
		if (find == 0xff && duration_in_range == 0) {
			LCD_ERR(pdrv, "fr hint: can't support duration %d", duration);
			mutex_unlock(&lcd_vout_mutex);
			return -1;
		}
		if (duration_in_range) {
			frame_rate    = duration / 100;
			duration_num  = duration;
			duration_den  = 100;
			duration_frac = (bool)(duration % 100);
		} else {
			frame_rate    = set_dur->frame_rate;
			duration_num  = set_dur->duration_num;
			duration_den  = set_dur->duration_den;
			duration_frac = set_dur->duration_num % set_dur->duration_den;
		}

		LCD_PR(pdrv, "fr hint: duration=%d frame_rate=%d fr_adj_type=%d",
		      duration, duration / 100,
		      pconf->timing.act_timing.fr_adjust_type);

		pdrv->fr_duration = duration;
		/* if the sync_duration is same as current */
		if (duration_num == pconf->timing.act_timing.sync_duration_num &&
		    duration_den == pconf->timing.act_timing.sync_duration_den) {
			LCD_ERR(pdrv, "fr hint: sync_duration is same");
			mutex_unlock(&lcd_vout_mutex);
			return 0;
		}

		/* update target frame rate */
		pconf->timing.frame_rate = frame_rate;
		pconf->timing.sync_duration_num = duration_num;
		pconf->timing.sync_duration_den = duration_den;
		pconf->timing.frac = duration_frac;
		pdrv->fr_mode = 1;
	}

	lcd_vout_notify_mode_change_pre(pdrv);
	lcd_frame_rate_change(pdrv);
	lcd_mode_auto_framerate_set(pdrv);
	lcd_vout_notify_mode_change(pdrv);

	mutex_unlock(&lcd_vout_mutex);

	return 0;
}

// * act_timing to pdrv->vinfo (lcd_config_init)
static void lcd_act_timing_update_to_vinfo(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *ptiming;

	if (!pdrv->vmode_mgr.cur_vmode_info)
		return;
	ptiming = &pdrv->curr_dev->dev_cfg.timing.act_timing;
	memset(pdrv->output_name, 0, sizeof(pdrv->output_name));

	if (pdrv->mode == LCD_MODE_TABLET) {
		str_add_vmode(pdrv->output_name, 0, ptiming->h_active, ptiming->v_active,
			ptiming->frame_rate);
	} else {
		str_add_vmode(pdrv->output_name, 0, ptiming->h_active, ptiming->v_active,
			pdrv->vmode_mgr.cur_vmode_info->base_fr);
	}

	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.width = ptiming->h_active;
	pdrv->vinfo.height = ptiming->v_active;
	pdrv->vinfo.field_height = ptiming->v_active;
	pdrv->vinfo.aspect_ratio_num = pdrv->curr_dev->dev_cfg.basic.screen_width;
	pdrv->vinfo.aspect_ratio_den = pdrv->curr_dev->dev_cfg.basic.screen_height;
	pdrv->vinfo.screen_real_width = pdrv->curr_dev->dev_cfg.basic.screen_width;
	pdrv->vinfo.screen_real_height = pdrv->curr_dev->dev_cfg.basic.screen_height;
	pdrv->vinfo.sync_duration_num = ptiming->sync_duration_num;
	pdrv->vinfo.sync_duration_den = ptiming->sync_duration_den;
	pdrv->vinfo.frac = ptiming->frac;
	pdrv->vinfo.brr_duration = pdrv->vmode_mgr.cur_vmode_info->base_fr;
	pdrv->vinfo.std_duration = ptiming->frame_rate;
	pdrv->vinfo.vfreq_max = ptiming->frame_rate_max;
	pdrv->vinfo.vfreq_min = ptiming->frame_rate_min;
	pdrv->vinfo.video_clk = pdrv->curr_dev->dev_cfg.timing.enc_clk;
	pdrv->vinfo.htotal = ptiming->h_period;
	pdrv->vinfo.vtotal = ptiming->v_period;
	pdrv->vinfo.hsw = ptiming->hsync_width;
	pdrv->vinfo.hbp = ptiming->hsync_bp;
	pdrv->vinfo.hfp = ptiming->hsync_fp;
	pdrv->vinfo.vsw = ptiming->vsync_width;
	pdrv->vinfo.vbp = ptiming->vsync_bp;
	pdrv->vinfo.vfp = ptiming->vsync_fp;
	pdrv->vinfo.viu_mux = VIU_MUX_ENCL | (pdrv->index << 4);
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_MLVDS:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_A + pdrv->index;
		break;
	case LCD_VBYONE:
	case LCD_P2P:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_VBYONE_A + pdrv->index;
		break;
	case LCD_MIPI:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_MIPI_A + pdrv->index;
		break;
	default:
		pdrv->vinfo.connector_type = 0 + pdrv->index;
		break;
	}

	pdrv->vinfo.cur_enc_ppc = pdrv->curr_dev->dev_cfg.timing.ppc;
	pdrv->vinfo.asf_mode = ptiming->asf_mode;
	pdrv->vinfo.ufr_mode = ptiming->ufr_mode;
	switch (ptiming->fr_adjust_type) {
	case 0:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_CLK;
		break;
	case 1:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_HTOTAL;
		break;
	case 2:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_VTOTAL;
		break;
	case 3:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_COMBO;
		break;
	case 4:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_HDMI;
		break;
	case 5:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_FREERUN;
		break;
	default:
		pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
		break;
	}

	lcd_optical_vinfo_update(pdrv);
}

void lcd_mode_dft_timing_update_vinfo(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *ptiming = &pdrv->curr_dev->dev_cfg.timing.act_timing;

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return;

	memset(pdrv->output_name, 0, sizeof(pdrv->output_name));

	str_add_vmode(pdrv->output_name, 0,
		ptiming->h_active, ptiming->v_active, ptiming->frame_rate);

	pdrv->vinfo.name = pdrv->output_name;
	pdrv->vinfo.mode = VMODE_LCD;
	pdrv->vinfo.width =  ptiming->h_active;
	pdrv->vinfo.height = ptiming->v_active;
	pdrv->vinfo.field_height = ptiming->v_active;
	pdrv->vinfo.aspect_ratio_num =  ptiming->h_active;
	pdrv->vinfo.aspect_ratio_den = ptiming->v_active;
	pdrv->vinfo.screen_real_width =  ptiming->h_active;
	pdrv->vinfo.screen_real_height = ptiming->v_active;
	pdrv->vinfo.sync_duration_num = ptiming->frame_rate;
	pdrv->vinfo.sync_duration_den = 1;
	pdrv->vinfo.frac = ptiming->frac;
	pdrv->vinfo.std_duration = ptiming->frame_rate;
	pdrv->vinfo.vfreq_max = ptiming->frame_rate;
	pdrv->vinfo.vfreq_min = ptiming->frame_rate;
	pdrv->vinfo.video_clk = 0;
	pdrv->vinfo.htotal = ptiming->h_period;
	pdrv->vinfo.vtotal = ptiming->v_period;
	pdrv->vinfo.cur_enc_ppc = pdrv->curr_dev->dev_cfg.timing.ppc;
	pdrv->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_MLVDS:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_A + pdrv->index;
		break;
	case LCD_VBYONE:
	case LCD_P2P:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_VBYONE_A + pdrv->index;
		break;
	case LCD_MIPI:
		pdrv->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_MIPI_A + pdrv->index;
		break;
	default:
		pdrv->vinfo.connector_type = 0 + pdrv->index;
		break;
	}
}

static void lcd_timing_update_vmode(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_detail_timing_s *ptiming;
	int dur_index, ret;

	ret = lcd_mode_timing_switch_update(pdrv);
	if (ret)
		return;

	if (!pdrv->vmode_mgr.cur_vmode_info || !pdrv->std_duration) {
		LCD_ERR(pdrv, "%s: cur_vmode_info or std_duration is null", __func__);
		return;
	}

	dur_index = pdrv->vmode_mgr.cur_vmode_info->duration_index;

	/* update target frame rate */
	ptiming = &pdrv->curr_dev->dev_cfg.timing.act_timing;
	if (pdrv->mode == LCD_MODE_TABLET && dur_index == 0xff) {
		pdrv->curr_dev->dev_cfg.timing.sync_duration_num =
			pdrv->vmode_mgr.cur_vmode_info->base_fr;
		pdrv->curr_dev->dev_cfg.timing.sync_duration_den = 1;
		pdrv->curr_dev->dev_cfg.timing.frac = 0;
		pdrv->curr_dev->dev_cfg.timing.frame_rate =
			pdrv->vmode_mgr.cur_vmode_info->base_fr;
	} else {
		pdrv->curr_dev->dev_cfg.timing.sync_duration_num =
			pdrv->std_duration[dur_index].duration_num;
		pdrv->curr_dev->dev_cfg.timing.sync_duration_den =
			pdrv->std_duration[dur_index].duration_den;
		pdrv->curr_dev->dev_cfg.timing.frac =
			pdrv->std_duration[dur_index].duration_num %
			pdrv->std_duration[dur_index].duration_den ? 1 : 0;
		pdrv->curr_dev->dev_cfg.timing.frame_rate =
			pdrv->std_duration[dur_index].frame_rate;
	}
	lcd_frame_rate_change(pdrv);

	LCD_DBG(pdrv, "update_vmode: %dx%dp%dhz, duration=%d:%d, dur_index=%d, clk_change=0x%x",
		ptiming->h_active, ptiming->v_active, ptiming->frame_rate,
		ptiming->sync_duration_num, ptiming->sync_duration_den,
		dur_index, pdrv->curr_dev->dev_cfg.timing.clk_change);
}

int lcd_mode_timing_set_next(struct aml_lcd_drv_s *pdrv, enum vmode_e vmode)
{
	unsigned long long local_time[2];
	int ret = 0;

	if (!pdrv || !pdrv->curr_dev)
		return -1;

	LCD_DBG(pdrv, "mode timing set: mode=0x%x", vmode);
	if (lcd_vout_serve_bypass) {
		LCD_PR(pdrv, "vout_serve bypass");
		return 0;
	}

	if (pdrv->mode == LCD_MODE_TV) {
		if (lcd_fr_is_fixed(pdrv)) {
			if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
				//workaround for drm resume
				pdrv->status |= LCD_CTRL_PREPARE;
				aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);

				lcd_power_if_early_on(pdrv);
			}
			pdrv->status |= LCD_CTRL_VMODE_ACTIVE;
			LCD_PR(pdrv, "fixed timing, exit vmode change");
			return -1;
		}
	}

	mutex_lock(&lcd_power_mutex);

	lcd_proc_time_clear(pdrv);
	local_time[0] = sched_clock();
	pdrv->proc_time.switch_start_time = local_time[0];
	/* clear fr*/
	pdrv->fr_duration = 0;
	pdrv->fr_mode = 0;

	mutex_lock(&lcd_vout_mutex);
	lcd_timing_update_vmode(pdrv);
	lcd_act_timing_update_to_vinfo(pdrv);
	mutex_unlock(&lcd_vout_mutex);

	lcd_vrr_dev_register(pdrv);
	if (vmode & VMODE_INIT_BIT_MASK) { // first boot: set clktree
		lcd_clk_gate_switch(pdrv, 1);
	} else if (pdrv->status & LCD_STATUS_ENCL_ON) { // mode change by vout
		if (pdrv->vmode_switch) {
			lcd_mode_vmode_switch(pdrv);
		} else {
			mutex_lock(&lcd_vout_mutex);
			lcd_mode_auto_framerate_set(pdrv);
			mutex_unlock(&lcd_vout_mutex);
		}
	} else {// if (!(pdrv->status & LCD_STATUS_ENCL_ON)) {
		if (pdrv->mode == LCD_MODE_TABLET) {
			// mode change by drm (disable + mode_change)
			pdrv->status |= (LCD_CTRL_PREPARE | LCD_CTRL_POWER);
			aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
			//aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
			//pdrv->status |= LCD_STATE_PREPARE;
		} else {  // TV
			//workaround for drm resume (vsync)
			pdrv->status |= LCD_CTRL_PREPARE;
			aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
		}
	}

	pdrv->vmode_switch = 0;
	local_time[1] = sched_clock();
	pdrv->proc_time.switch_full_time = local_time[1] - local_time[0];

	/* must update vrr dev after driver change for panel parameters update */
	lcd_vrr_dev_update(pdrv);

	pdrv->status |= LCD_CTRL_VMODE_ACTIVE;

	mutex_unlock(&lcd_power_mutex);
	return ret;
}

static void lcd_mode_viu_init(struct aml_lcd_drv_s *pdrv)
{
	char *curr_vout_connector, *curr_vout_mode;
	char lcd_connector[10];
	unsigned short h, v, fr, ret = 0;
	enum vmode_e vmode;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_MLVDS:
		snprintf(lcd_connector, 10, "LVDS-%c", 'A' + pdrv->index);
		break;
	case LCD_VBYONE:
	case LCD_P2P:
		snprintf(lcd_connector, 10, "VBYONE-%c", 'A' + pdrv->index);
		break;
	case LCD_MIPI:
		snprintf(lcd_connector, 10, "MIPI-%c", 'A' + pdrv->index);
		break;
	default:
		snprintf(lcd_connector, 10, "LCD-%c", 'A' + pdrv->index);
		break;
	}

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	curr_vout_connector = get_uboot_connector0_type();
	curr_vout_mode = get_vout_mode_uboot();
	if (!strcmp(lcd_connector, curr_vout_connector))
		pdrv->viu_sel = 1;
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	if (!pdrv->viu_sel) {
		curr_vout_connector = get_uboot_connector1_type();
		curr_vout_mode = get_vout2_mode_uboot();
		if (!strcmp(lcd_connector, curr_vout_connector))
			pdrv->viu_sel = 2;
	}
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	if (!pdrv->viu_sel) {
		curr_vout_connector = get_uboot_connector2_type();
		curr_vout_mode = get_vout3_mode_uboot();
		if (!strcmp(lcd_connector, curr_vout_connector))
			pdrv->viu_sel = 3;
	}
#endif

	ret = str_parse_vmode(curr_vout_mode, &h, &v, &fr);
	LCD_PR(pdrv, "mode viu init: viu{%u} connector=%s, mode:%s",
		pdrv->viu_sel, curr_vout_connector, curr_vout_mode);

	if (ret && pdrv->viu_sel) {
		pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate = fr;
		LCD_PR(pdrv, "mode viu init: update act_timing {%u,%u} %uhz", h, v, fr);

		vmode = lcd_mode_validate_vmode(curr_vout_mode, 0, (void *)pdrv);
		mutex_lock(&lcd_vout_mutex);
		if (vmode == VMODE_LCD)
			lcd_timing_update_vmode(pdrv);
		lcd_act_timing_update_to_vinfo(pdrv);
		pdrv->vmode_switch = 0;
		mutex_unlock(&lcd_vout_mutex);
	}
}

int lcd_mode_set_current_vmode(enum vmode_e mode, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	unsigned long long local_time[2];
	int ret = 0;

	if (!pdrv)
		return -1;

	LCD_DBG(pdrv, "mode set: mode=0x%x", mode);
	if (lcd_vout_serve_bypass) {
		LCD_PR(pdrv, "vout_serve bypass");
		return 0;
	}

	if ((mode & VMODE_MODE_BIT_MASK) != VMODE_LCD) {
		LCD_PR(pdrv, "unsupport mode 0x%x", mode & VMODE_MODE_BIT_MASK);
		return -1;
	}

	if (lcd_fr_is_fixed(pdrv) && pdrv->mode == LCD_MODE_TV) {
		if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
			//workaround for drm resume
			pdrv->status |= LCD_CTRL_PREPARE;
			aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);

			lcd_power_if_early_on(pdrv);
		}
		pdrv->status |= LCD_CTRL_VMODE_ACTIVE;
		LCD_PR(pdrv, "fixed timing, exit vmode change");
		return -1;
	}

	mutex_lock(&lcd_power_mutex);

	lcd_proc_time_clear(pdrv);
	local_time[0] = sched_clock();
	pdrv->proc_time.switch_start_time = local_time[0];
	/* clear fr*/
	pdrv->fr_duration = 0;
	pdrv->fr_mode = 0;

	mutex_lock(&lcd_vout_mutex);
	lcd_timing_update_vmode(pdrv);
	lcd_act_timing_update_to_vinfo(pdrv);
	mutex_unlock(&lcd_vout_mutex);

	//if (pdrv->mode == LCD_MODE_TABLET) // ?????????????
	//	lcd_vrr_dev_register(pdrv);

	if (mode & VMODE_INIT_BIT_MASK) {
		lcd_clk_gate_switch(pdrv, 1);
	} else {
		if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0) {
			if (pdrv->mode == LCD_MODE_TABLET) {
				// mode change by drm (disable + mode_change)
				// aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
				// pdrv->status |= LCD_EVENT_ENABLE;
				pdrv->status |= LCD_CTRL_PREPARE;
				aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
			} else {
				//workaround for drm resume
				pdrv->status |= LCD_CTRL_PREPARE;
				aml_lcd_notifier_call_chain(LCD_EVENT_PREPARE, (void *)pdrv);
			}
		} else {
			if (pdrv->vmode_switch) {
				lcd_mode_vmode_switch(pdrv);
			} else {
				mutex_lock(&lcd_vout_mutex);
				lcd_mode_auto_framerate_set(pdrv);
				mutex_unlock(&lcd_vout_mutex);
			}
		}
	}

	pdrv->vmode_switch = 0;
	local_time[1] = sched_clock();
	pdrv->proc_time.switch_full_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	/* must update vrr dev after driver change for panel parameters update */
	lcd_vrr_dev_update(pdrv);

	pdrv->status |= LCD_CTRL_VMODE_ACTIVE;

	mutex_unlock(&lcd_power_mutex);

	return ret;
}

int lcd_mode_vout_disable(enum vmode_e cur_vmod, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;

	if (!pdrv)
		return -1;

	if (pdrv->mode == LCD_MODE_TV) {
		mutex_lock(&lcd_vout_mutex);
		pdrv->status &= ~LCD_CTRL_VMODE_ACTIVE;
		mutex_unlock(&lcd_vout_mutex);
	} else {
		//pdrv->status &= ~LCD_CTRL_VMODE_ACTIVE;
		lcd_vrr_dev_unregister(pdrv);
		pdrv->init_flag = 0;
		pdrv->status &= ~(LCD_CTRL_VMODE_ACTIVE | LCD_CTRL_PREPARE | LCD_CTRL_POWER);
		aml_lcd_notifier_call_chain(LCD_EVENT_DISABLE, (void *)pdrv);
		LCD_PR(pdrv, "vout_disable finished");
	}

	return 0;
}

int lcd_mode_vout_get_disp_cap(char *buf, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct lcd_vmode_list_s *temp_list;
	unsigned int frame_rate;
	int i, ret = 0;

	if (!pdrv)
		return 0;

	mutex_lock(&lcd_vout_mutex);
	temp_list = pdrv->vmode_mgr.vmode_list_header;
	while (temp_list) {
		if (!temp_list->info)
			continue;
		if (pdrv->mode == LCD_MODE_TABLET) {
			ret += str_add_vmode(buf + ret, 1, temp_list->info->width,
				temp_list->info->height, temp_list->info->base_fr);
		}

		for (i = 0; i < LCD_DURATION_MAX; i++) {
			frame_rate = temp_list->info->duration[i].frame_rate;
			if (frame_rate == 0)
				break;
			ret += str_add_vmode(buf + ret, 1, temp_list->info->width,
				temp_list->info->height, frame_rate);
		}
		temp_list = temp_list->next;
	}
	mutex_unlock(&lcd_vout_mutex);

	return ret;
}

// * bind 3 vout op
void lcd_mode_vout_server_init(struct aml_lcd_drv_s *pdrv)
{
	pdrv->vout_server = kzalloc(sizeof(*pdrv->vout_server), GFP_KERNEL);
	if (!pdrv->vout_server)
		return;
	pdrv->vout_server->name = kzalloc(32, GFP_KERNEL);
	if (!pdrv->vout_server->name) {
		kfree(pdrv->vout_server);
		return;
	}

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_MLVDS:
		pdrv->vout_server->connector_type = DRM_MODE_CONNECTOR_MESON_LVDS_A + pdrv->index;
		break;
	case LCD_VBYONE:
	case LCD_P2P:
		pdrv->vout_server->connector_type = DRM_MODE_CONNECTOR_MESON_VBYONE_A + pdrv->index;
		break;
	case LCD_MIPI:
		pdrv->vout_server->connector_type = DRM_MODE_CONNECTOR_MESON_MIPI_A + pdrv->index;
		break;
	default:
		pdrv->vout_server->connector_type = 0 + pdrv->index;
		break;
	}

	sprintf(pdrv->vout_server->name, "lcd%d_vout_server", pdrv->index);

	pdrv->vout_server->op.set_vmode            = lcd_mode_set_current_vmode;
	pdrv->vout_server->op.validate_vmode       = lcd_mode_validate_vmode;
	pdrv->vout_server->op.disable              = lcd_mode_vout_disable;
	pdrv->vout_server->op.get_disp_cap         = lcd_mode_vout_get_disp_cap;
	pdrv->vout_server->op.set_vframe_rate_hint = lcd_mode_set_vframe_rate_hint;
	pdrv->vout_server->op.get_vinfo            = lcd_mode_get_current_info;
	pdrv->vout_server->op.check_same_vmodeattr = lcd_mode_check_same_vmodeattr;
	pdrv->vout_server->op.vmode_is_supported   = lcd_mode_vmode_is_supported;
	pdrv->vout_server->op.set_state            = lcd_mode_vout_set_state;
	pdrv->vout_server->op.clr_state            = lcd_mode_vout_clr_state;
	pdrv->vout_server->op.get_state            = lcd_mode_vout_get_state;
	pdrv->vout_server->op.get_vframe_rate_hint = lcd_mode_get_vframe_rate_hint;
	pdrv->vout_server->op.set_bist             = lcd_mode_vout_debug_test;
	pdrv->vout_server->op.set_bl_brightness    = lcd_mode_vout_set_bl_brightness;
	pdrv->vout_server->op.get_bl_brightness    = lcd_mode_vout_get_bl_brightness;
	pdrv->vout_server->op.vout_suspend         = lcd_mode_suspend;
	pdrv->vout_server->op.vout_resume          = lcd_mode_resume;
	pdrv->vout_server->data = (void *)pdrv;

	LCD_DBG(pdrv, "regist: %s", pdrv->vout_server->name);

	// lcd_mode_dft_timing_update_vinfo(pdrv);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_register_server(pdrv->vout_server);
#endif
}

void lcd_mode_vout_server_remove(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_unregister_server(pdrv->vout_server);
#endif

	kfree(pdrv->vout_server->name);
	kfree(pdrv->vout_server);
	pdrv->vout_server = NULL;
}

void lcd_mode_config_init(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv || !pdrv->curr_dev)
		return;

	mutex_lock(&lcd_vout_mutex);
	lcd_base_to_act_timing_init_config(pdrv);
	lcd_clk_config_parameter_init(pdrv);
	mutex_unlock(&lcd_vout_mutex);

	lcd_mode_viu_init(pdrv);

	mutex_lock(&lcd_vout_mutex);
	lcd_frame_rate_change(pdrv);
	lcd_clk_generate_parameter(pdrv);
	pdrv->curr_dev->dev_cfg.timing.clk_change = 0; /* clear clk_change flag */
	mutex_unlock(&lcd_vout_mutex);
}

int lcd_mode_init(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv)
		return -1;

	lcd_mode_config_init(pdrv);

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_interrupt_up(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		lcd_dsi_if_bind(pdrv);
		lcd_dsi_post_config_load(pdrv);
		lcd_dsi_interrupt_ctrl(pdrv, 1);
		break;
#endif
	default:
		break;
	}

	lcd_vrr_dev_register(pdrv);

	return 0;
}

int lcd_mode_remove(struct aml_lcd_drv_s *pdrv)
{
	lcd_vrr_dev_unregister(pdrv);

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_interrupt_down(pdrv);
		break;
#endif
	default:
		break;
	}
	// lcd_vmode_remove_all(pdrv);

	return 0;
}

// * append to pdrv->vmode_mgr.vmode_list_header
static int lcd_vmode_add_to_list(struct aml_lcd_drv_s *pdrv, struct lcd_vmode_info_s *vmode_info)
{
	struct lcd_vmode_list_s *temp_list = NULL;
	struct lcd_vmode_list_s *cur_list;
	u8 vmode_list_idx = 0;

	if (!vmode_info)
		return -1;

	/* create list */
	cur_list = kzalloc(sizeof(*cur_list), GFP_KERNEL);
	if (!cur_list)
		return -1;
	cur_list->info = vmode_info;

	if (!pdrv->vmode_mgr.vmode_list_header) {
		pdrv->vmode_mgr.vmode_list_header = cur_list;
	} else {
		temp_list = pdrv->vmode_mgr.vmode_list_header;
		while (temp_list->next) {
			temp_list = temp_list->next;
			vmode_list_idx++;
		}
		temp_list->next = cur_list;
	}
	pdrv->vmode_mgr.vmode_cnt += vmode_info->duration_cnt + 1;

	LCD_PR(pdrv, "mode add: list[%u] %d*%d, base_fr:%dhz, vmode_cnt: %d",
		vmode_list_idx, cur_list->info->width, cur_list->info->height,
		cur_list->info->base_fr, pdrv->vmode_mgr.vmode_cnt);

	return 0;
}

static void lcd_vmode_append_duration(struct lcd_vmode_info_s *vmode,
					struct lcd_detail_timing_s *dtm)
{
	unsigned int n = 0, i;//, fr, ht, vt;
	//unsigned short fr_min, fr_max;

	//fr_min = div_around(div_around(dtm->pclk_min, dtm->v_period_max), dtm->h_period_max);
	//fr_max = div_around(div_around(dtm->pclk_max, dtm->v_period_min), dtm->h_period_min);

	memset(vmode->duration, 0, sizeof(struct lcd_duration_s) * LCD_DURATION_MAX);

	for (i = 0; i < ARRAY_SIZE(lcd_std_fr); i++) {
		if (dtm->fr_adjust_type == 0xff)
			break;
		if (lcd_std_fr[i].frame_rate == 0)
			break;
		if (lcd_std_fr[i].frame_rate > vmode->dft_timing->frame_rate_max)
			continue;
		if (lcd_std_fr[i].frame_rate < vmode->dft_timing->frame_rate_min)
			break;
		//if ((vmode->base_fr * lcd_std_fr[i].duration_den == lcd_std_fr[i].duration_num) &&
		//	(pdrv->mode == LCD_MODE_TABLET))
		//	continue;

		vmode->duration[n].frame_rate = lcd_std_fr[i].frame_rate;
		vmode->duration[n].duration_num = lcd_std_fr[i].duration_num;
		vmode->duration[n].duration_den = lcd_std_fr[i].duration_den;
		n++;
		if (n >= LCD_DURATION_MAX)
			break;
	}
	vmode->duration_cnt = n;
}

static struct lcd_vmode_info_s *lcd_detail_timing_to_vmode(struct lcd_detail_timing_s *ptiming)
{
	struct lcd_vmode_info_s *vmode_find = NULL;

	vmode_find = kzalloc(sizeof(*vmode_find), GFP_KERNEL);
	if (!vmode_find)
		return NULL;
	vmode_find->width = ptiming->h_active;
	vmode_find->height = ptiming->v_active;
	vmode_find->base_fr = ptiming->frame_rate;
	vmode_find->dft_timing = ptiming;
	lcd_vmode_append_duration(vmode_find, ptiming);

	return vmode_find;
}

void lcd_mode_add_device_vmode(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct lcd_vmode_info_s *vmode_find = NULL;
	int i;

	if (!pdrv)
		return;

	// lcd_vmode_remove_all(pdrv);
	// ! multi device temp
	// all device vmode list export

	for (i = 0; i < dev_p->dev_cfg.timing.num_timings; i++) {
		vmode_find = lcd_detail_timing_to_vmode(dev_p->dev_cfg.timing.timings[i]);
		if (vmode_find)
			lcd_vmode_add_to_list(pdrv, vmode_find);
	}
}
