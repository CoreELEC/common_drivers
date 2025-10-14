// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "../lcd_common.h"
#include "lcd_venc.h"

static struct lcd_venc_op_s lcd_venc_op = {
	.init_flag = 0,
	.wait_vsync = NULL,
	.get_max_lcnt = NULL,
	.gamma_test_en = NULL,
	.venc_debug_test = NULL,
	.venc_bist_change = NULL,
	.mute_set = NULL,
	.venc_set_timing = NULL,
	.venc_set = NULL,
	.venc_set_dummy = NULL,
	.venc_change = NULL,
	.venc_enable = NULL,
	.get_venc_state = NULL,
	.get_venc_init_config = NULL,
	.venc_vrr_recovery = NULL,
	.get_encl_line_cnt = NULL,
	.get_encl_frm_cnt = NULL,
	.venc_set_htotal = NULL,
	.venc_set_vtotal = NULL,
	.venc_reg_dump = NULL,
};

void lcd_wait_vsync(struct aml_lcd_drv_s *pdrv)
{
	if (pdrv->lcd_pxp)
		return;
	if (!lcd_venc_op.wait_vsync)
		return;

	lcd_venc_op.wait_vsync(pdrv);
}

unsigned int lcd_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int lcnt;

	if (!lcd_venc_op.get_encl_line_cnt)
		return 0;
	if (!pdrv)
		return 0;

	lcnt = lcd_venc_op.get_encl_line_cnt(pdrv);
	return lcnt;
}

unsigned int lcd_get_max_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int max_lcnt;

	if (!lcd_venc_op.get_max_lcnt)
		return 0;
	if (!pdrv)
		return 0;

	max_lcnt = lcd_venc_op.get_max_lcnt(pdrv);
	LCD_DBG(pdrv, "%s: %d", __func__, max_lcnt);

	return max_lcnt;
}

void lcd_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (!lcd_venc_op.gamma_test_en)
		return;

	lcd_venc_op.gamma_test_en(pdrv, flag);
}

void lcd_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	int ret;

	if (!lcd_venc_op.venc_debug_test) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return;
	}

	ret = lcd_venc_op.venc_debug_test(pdrv, num);
	if (ret) {
		LCD_ERR(pdrv, "%s: %d not support", __func__, num);
		return;
	}

	if (num == 0)
		LCD_PR(pdrv, "disable test pattern");
}

void lcd_window_attr_change(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	if (!lcd_venc_op.window_attr_change) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	ret = lcd_venc_op.window_attr_change(pdrv);
	if (ret) {
		LCDERR("[%d]: %s: window attr change failed\n", pdrv->index, __func__);
		return;
	}
}

void lcd_debug_cursor(struct aml_lcd_drv_s *pdrv, struct lcd_cursor_attr_s *cursor_attr)
{
	int ret;

	if (!lcd_venc_op.venc_probe_cursor) {
		LCDERR("[%d]: %s: invalid\n", pdrv->index, __func__);
		return;
	}

	ret = lcd_venc_op.venc_probe_cursor(pdrv, cursor_attr);
	if (ret) {
		LCDERR("[%d]: %s: cursor set failed\n", pdrv->index, __func__);
		return;
	}
}

void lcd_screen_restore(struct aml_lcd_drv_s *pdrv)
{
	unsigned long flags = 0;

	if (lcd_venc_op.mute_set) {
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		pdrv->mute_flag = 0;
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		LCD_PR(pdrv, "%s", __func__);
	} else if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		set_output_mute(false);
		pdrv->mute_flag = 0;
		LCD_PR(pdrv, "%s", __func__);
#endif
	}
}

void lcd_screen_black(struct aml_lcd_drv_s *pdrv)
{
	unsigned long flags = 0;

	if (lcd_venc_op.mute_set) {
		spin_lock_irqsave(&pdrv->isr_lock, flags);
		pdrv->mute_flag = 1;
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	} else if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		set_output_mute(true);
		pdrv->mute_flag = 1;
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
#endif
	}
}

//vsync isr stage
void lcd_bist_change(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
		     unsigned int level_g, unsigned int level_b)
{
	if (!lcd_venc_op.venc_bist_change)
		return;

	lcd_venc_op.venc_bist_change(pdrv, level_r, level_g, level_b);
}

void lcd_mute_set(struct aml_lcd_drv_s *pdrv,  unsigned char flag)
{
	if (lcd_venc_op.mute_set) {
		if (pdrv->vmode_switch) {
			if (flag) {
				pdrv->test_state = 0;
				lcd_venc_op.mute_set(pdrv, flag);
			} else {
				if (pdrv->test_flag) {
					pdrv->test_state = pdrv->test_flag;
					lcd_debug_test(pdrv, pdrv->test_state);
				} else {
					lcd_venc_op.mute_set(pdrv, flag);
				}
			}
		} else {
			lcd_venc_op.mute_set(pdrv, flag);
		}
		LCD_PR(pdrv, "%s, venc mute %s", __func__, flag ? "ON" : "OFF");
	} else if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		if (pdrv->vmode_switch) {
			if (flag) {
				set_output_mute(1);
				if (pdrv->test_flag) {
					pdrv->test_state = 0;
					lcd_debug_test(pdrv, pdrv->test_state);
				}
			} else {
				if (pdrv->test_flag) {
					pdrv->test_state = pdrv->test_flag;
					lcd_debug_test(pdrv, pdrv->test_state);
				}
				set_output_mute(flag);
			}
		} else {
			set_output_mute(flag);
		}
		LCD_DBG(pdrv, "%s, vpp mute %s", __func__, flag ? "ON" : "OFF");
#endif
	}
}

int lcd_mute_state_get(struct aml_lcd_drv_s *pdrv)
{
	int flag = 0;
	unsigned long irq_flags = 0;

	if (lcd_venc_op.mute_set) {
		spin_lock_irqsave(&pdrv->isr_lock, irq_flags);
		flag = pdrv->mute_state;
		spin_unlock_irqrestore(&pdrv->isr_lock, irq_flags);
	} else if (pdrv->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		flag = get_output_mute();
#endif
	}
	return flag;
}

void lcd_set_venc_timing(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_set_timing)
		return;

	LCD_DBG(pdrv, "%s", __func__);
	lcd_venc_op.venc_set_timing(pdrv);

	lcd_act_timing_dbg_print(pdrv);
}

void lcd_set_venc(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_set) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return;
	}

	LCD_DBG(pdrv, "%s", __func__);
	lcd_venc_op.venc_set(pdrv);

	lcd_act_timing_dbg_print(pdrv);
}

void lcd_venc_change(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_change) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return;
	}

	if ((pdrv->status & LCD_STATUS_ENCL_ON) == 0)
		return;
	LCD_DBG(pdrv, "%s", __func__);
	lcd_venc_op.venc_change(pdrv);

	lcd_act_timing_dbg_print(pdrv);
}

void lcd_venc_enable(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (!lcd_venc_op.venc_enable) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return;
	}

	LCD_DBG(pdrv, "%s: %d", __func__, flag);
	lcd_venc_op.venc_enable(pdrv, flag);
}

int lcd_get_venc_state(struct aml_lcd_drv_s *pdrv)
{
	int ret = 0;

	if (!lcd_venc_op.get_venc_state) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return 0;
	}

	ret = lcd_venc_op.get_venc_state(pdrv);
	return ret;
}

int lcd_get_venc_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_boot_ctrl_s *boot_ctrl = pdrv->boot_ctrl;
	int ret = 0;

	if (!lcd_venc_op.get_venc_init_config) {
		LCD_ERR(pdrv, "%s: invalid", __func__);
		return 0;
	}

	ret = lcd_venc_op.get_venc_init_config(pdrv);
	if (ret & 0x2) {
		LCD_DBG_ADV(pdrv, "%s: load boot_ctrl from regs:\n"
			"\tlcd_type        : %d\n"
			"\tadvanced_flag   : %d\n"
			"\tdccd_flag       : %d\n"
			"\tmute_flag       : %d\n"
			"\tppc             : %d\n"
			"\tclk_mode        : %d\n"
			"\tframe_rate      : %d\n"
			"\tinit_level      : %d\n"
			"\tif_state        : %d\n"
			"\tbl_state        : %d",
			__func__,
			boot_ctrl->lcd_type,
			boot_ctrl->advanced_flag,
			boot_ctrl->dccd_flag,
			boot_ctrl->mute_flag,
			boot_ctrl->ppc,
			boot_ctrl->clk_mode,
			boot_ctrl->frame_rate,
			boot_ctrl->init_level,
			boot_ctrl->if_state,
			boot_ctrl->bl_state);
	}

	return ret;
}

unsigned int lcd_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cnt = 0;

	if (!lcd_venc_op.get_encl_frm_cnt)
		return 0;

	cnt = lcd_venc_op.get_encl_frm_cnt(pdrv);

	return cnt;
}

void lcd_venc_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_vrr_recovery)
		return;

	LCD_DBG(pdrv, "%s", __func__);

	lcd_venc_op.venc_vrr_recovery(pdrv);
}

/*vsync stage*/
void lcd_venc_adj_htotal(struct aml_lcd_drv_s *pdrv, unsigned int htotal)
{
	if (!lcd_venc_op.venc_set_htotal)
		return;

	lcd_venc_op.venc_set_htotal(pdrv, htotal);
}

/*vsync stage*/
void lcd_venc_adj_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal)
{
	if (!lcd_venc_op.venc_set_vtotal)
		return;

	lcd_venc_op.venc_set_vtotal(pdrv, vtotal);
}

void lcd_venc_set_dummy(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_venc_op.venc_set_dummy)
		return;

	LCD_DBG(pdrv, "%s", __func__);

	lcd_venc_op.venc_set_dummy(pdrv);
}

int lcd_venc_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	if (!pdrv)
		return 0;
	if (!lcd_venc_op.venc_reg_dump) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: venc_reg_dump is null\n", __func__);
		return len;
	}

	n = lcd_debug_info_len(len + offset);
	len = snprintf(buf, n, "\nencl regs:\n");
	len += lcd_venc_op.venc_reg_dump(pdrv, (buf + len), (len + offset));
	return len;
}

int lcd_venc_config_init(struct lcd_data_s *pdata)
{
	int ret;

	if (lcd_venc_op.init_flag)
		return 0;

	switch (pdata->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6D:
	case LCD_CHIP_T6W:
		ret = lcd_venc_op_init_t7(pdata, &lcd_venc_op);
		break;
	case LCD_CHIP_C3:
		ret = lcd_venc_op_init_c3(pdata, &lcd_venc_op);
		break;
	case LCD_CHIP_T3X:
		ret = lcd_venc_op_init_t3x(pdata, &lcd_venc_op);
		break;
	case LCD_CHIP_T6X:
		ret = lcd_venc_op_init_t6x(pdata, &lcd_venc_op);
		break;
	case LCD_CHIP_A9:
		ret = lcd_venc_op_init_a9(pdata, &lcd_venc_op);
		break;
	case LCD_CHIP_TXHD2:
	case LCD_CHIP_S6:
	default:
		ret = lcd_venc_op_init_dft(pdata, &lcd_venc_op);
		break;
	}
	if (ret) {
		LCD_ERR(NULL, "%s: failed", __func__);
		return -1;
	}

	lcd_venc_op.init_flag = 1;

	return 0;
}
