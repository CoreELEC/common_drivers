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
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_venc.h"

static void lcd_venc_wait_vsync(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt, line_cnt_previous;
	int i = 0;

	if (!pdrv || pdrv->lcd_pxp)
		return;

	line_cnt = 0x1fff;
	line_cnt_previous = lcd_vcbus_getb(ENCL_INFO_READ, 16, 13);
	while (i++ < LCD_WAIT_VSYNC_TIMEOUT) {
		line_cnt = lcd_vcbus_getb(ENCL_INFO_READ, 16, 13);
		if (line_cnt < line_cnt_previous)
			break;
		line_cnt_previous = line_cnt;
		udelay(2);
	}
	/*LCDPR("line_cnt=%d, line_cnt_previous=%d, i=%d\n",
	 *	line_cnt, line_cnt_previous, i);
	 */
}

static unsigned int lcd_venc_get_max_lint_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt;

	line_cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
	/*LCDPR("[%d]: %s: line_cnt=%d", pdrv->index, __func__, line_cnt); */

	return line_cnt;
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag) {
		if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1) == 0) {
			lcd_vcbus_setb(L_GAMMA_CNTL_PORT, 1, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	} else {
		if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1)) {
			lcd_vcbus_setb(L_GAMMA_CNTL_PORT, 0, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	}
}

#define LCD_ENC_TST_NUM_MAX    9
static char *lcd_enc_tst_str[] = {
	"0-None",        /* 0 */
	"1-Color Bar",   /* 1 */
	"2-Thin Line",   /* 2 */
	"3-Dot Grid",    /* 3 */
	"4-Gray",        /* 4 */
	"5-Red",         /* 5 */
	"6-Green",       /* 6 */
	"7-Blue",        /* 7 */
	"8-Black",       /* 8 */
};

static unsigned int lcd_enc_tst[][7] = {
/*tst_mode,    Y,       Cb,     Cr,     tst_en,  vfifo_en  rgbin*/
	{0,    0x200,   0x200,  0x200,   0,      1,        3},  /* 0 */
	{1,    0x200,   0x200,  0x200,   1,      0,        1},  /* 1 */
	{2,    0x200,   0x200,  0x200,   1,      0,        1},  /* 2 */
	{3,    0x200,   0x200,  0x200,   1,      0,        1},  /* 3 */
	{0,    0x1ff,   0x1ff,  0x1ff,   1,      0,        3},  /* 4 */
	{0,    0x3ff,     0x0,    0x0,   1,      0,        3},  /* 5 */
	{0,      0x0,   0x3ff,    0x0,   1,      0,        3},  /* 6 */
	{0,      0x0,     0x0,  0x3ff,   1,      0,        3},  /* 7 */
	{0,      0x0,     0x0,    0x0,   1,      0,        3},  /* 8 */
};

static int lcd_venc_bist_set(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_active, video_on_pixel;

	if (num >= LCD_ENC_TST_NUM_MAX)
		return -1;

	h_active = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	video_on_pixel = pdrv->curr_dev->dev_cfg.timing.hstart;

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, lcd_enc_tst[num][6]);
	lcd_vcbus_write(ENCL_TST_MDSEL, lcd_enc_tst[num][0]);
	lcd_vcbus_write(ENCL_TST_Y, lcd_enc_tst[num][1]);
	lcd_vcbus_write(ENCL_TST_CB, lcd_enc_tst[num][2]);
	lcd_vcbus_write(ENCL_TST_CR, lcd_enc_tst[num][3]);
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT, video_on_pixel);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH, (h_active / 9));
	lcd_vcbus_write(ENCL_TST_EN, lcd_enc_tst[num][4]);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, lcd_enc_tst[num][5], 3, 1);

	if (num > 0)
		LCD_PR(pdrv, "show test pattern: %s", lcd_enc_tst_str[num]);

	return 0;
}

static void lcd_venc_gamma_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int data[2];
	int index = pdrv->index;

	if (pdrv->lcd_pxp)
		return;

	data[0] = index;
	data[1] = 0xff; //default gamma lut
	aml_lcd_atomic_notifier_call_chain(LCD_EVENT_GAMMA_UPDATE, (void *)data);
}

static void lcd_venc_set_tcon(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;

	lcd_vcbus_write(L_RGB_BASE_ADDR, 0x0);
	lcd_vcbus_write(L_RGB_COEFF_ADDR, 0x400);

	if (pconf->basic.lcd_type != LCD_P2P &&
		pconf->basic.lcd_type != LCD_MLVDS) {
		switch (pconf->timing.act_timing.lcd_bits) {
		case 18:
			lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x600);
			break;
		case 24:
			lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x400);
			break;
		case 30:
		default:
			lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x0);
			break;
		}
	} else {
		lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x0);
	}

	switch (pconf->basic.lcd_type) {
#if defined(CONFIG_AMLOGIC_LCD_LVDS)
	case LCD_LVDS:
		lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 3);
		// refs to lcd_lvds.c@lcd_lvds_enable
		if (pconf->timing.act_timing.vsync_pol == pconf->timing.act_timing.hsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 1, 1);
		break;
#endif
#if defined(CONFIG_AMLOGIC_LCD_VBYONE)
	case LCD_VBYONE:
		if (pconf->timing.act_timing.hsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 1);
		if (pconf->timing.act_timing.vsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 1, 1);
		break;
#endif
#if defined(CONFIG_AMLOGIC_LCD_MIPI_DSI)
	case LCD_MIPI:
		//lcd_vcbus_setb(L_POL_CNTL_ADDR, 0x3, 0, 2);
		/*lcd_vcbus_write(L_POL_CNTL_ADDR,
		 *	(lcd_vcbus_read(L_POL_CNTL_ADDR) |
		 *	 ((0 << 2) | (vs_pol_adj << 1) | (hs_pol_adj << 0))));
		 */
		/*lcd_vcbus_write(L_POL_CNTL_ADDR, (lcd_vcbus_read(L_POL_CNTL_ADDR) |
		 *	 ((1 << LCD_TCON_DE_SEL) | (1 << LCD_TCON_VS_SEL) |
		 *	  (1 << LCD_TCON_HS_SEL))));
		 */
		break;
#endif
	default:
		break;
	}

	/* DE signal */
	lcd_vcbus_write(L_DE_HS_ADDR,    pconf->timing.de_hs_addr);
	lcd_vcbus_write(L_DE_HE_ADDR,    pconf->timing.de_he_addr);
	lcd_vcbus_write(L_DE_VS_ADDR,    pconf->timing.de_vs_addr);
	lcd_vcbus_write(L_DE_VE_ADDR,    pconf->timing.de_ve_addr);

	/* Hsync signal */
	lcd_vcbus_write(L_HSYNC_HS_ADDR, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(L_HSYNC_HE_ADDR, pconf->timing.hs_he_addr);
	lcd_vcbus_write(L_HSYNC_VS_ADDR, pconf->timing.hs_vs_addr);
	lcd_vcbus_write(L_HSYNC_VE_ADDR, pconf->timing.hs_ve_addr);

	/* Vsync signal */
	lcd_vcbus_write(L_VSYNC_HS_ADDR, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(L_VSYNC_HE_ADDR, pconf->timing.vs_he_addr);
	lcd_vcbus_write(L_VSYNC_VS_ADDR, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(L_VSYNC_VE_ADDR, pconf->timing.vs_ve_addr);
}

static void lcd_venc_set_timing(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int hstart, hend, vstart, vend;
	unsigned int pre_hde, pre_vde, pre_de_vs, pre_de_ve, pre_de_hs, pre_de_he;

	hstart = pconf->timing.hstart;
	hend = pconf->timing.hend;
	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->timing.act_timing.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->timing.act_timing.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, hstart);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END,   hend);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, vstart);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE, vend);
	if (pconf->basic.lcd_type == LCD_P2P ||
	    pconf->basic.lcd_type == LCD_MLVDS) {
		pre_hde = pconf->timing.pre_de_h ? pconf->timing.pre_de_h : PRE_DE_DELAY;
		pre_de_hs = pconf->timing.hstart + pre_hde;
		pre_de_he = pconf->timing.act_timing.h_active - 1 + pre_de_hs;
		switch (pdrv->data->chip_type) {
		case LCD_CHIP_TL1:
		case LCD_CHIP_TM2:
			pre_vde = pconf->timing.pre_de_v ? pconf->timing.pre_de_v : 5;
			pre_de_vs = pconf->timing.vstart - pre_vde;
			pre_de_ve = pre_de_vs + 4;
			break;
		default:
			pre_vde = pconf->timing.pre_de_v ? pconf->timing.pre_de_v : 8;
			pre_de_vs = pconf->timing.vstart - pre_vde;
			pre_de_ve = pconf->timing.act_timing.v_active + pre_de_vs;
			break;
		}
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_BLINE, pre_de_vs);
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_ELINE, pre_de_ve);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_BEGIN, pre_de_hs);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_END,   pre_de_he);
	}

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END,   pconf->timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END,   pconf->timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE, pconf->timing.vs_ve_addr);

	/*[15:14]: 2'b10 or 2'b01*/
	lcd_vcbus_write(ENCL_INBUF_CNTL1, (2 << 14) | (pconf->timing.act_timing.h_active - 1));
	lcd_vcbus_write(ENCL_INBUF_CNTL0, 0x200);

	lcd_venc_set_tcon(pdrv);
	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_UPDATE, (void *)pdrv);
}

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	lcd_vcbus_write(ENCL_VIDEO_EN, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE, 0x8000); /* bit[15] shadown en */
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV, 0x0418); /* Sampling rate: 1 */
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL, 0x1000); /* bypass filter */

	lcd_venc_set_timing(pdrv);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);
	//restore test pattern
	lcd_venc_bist_set(pdrv, pdrv->test_state);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	lcd_venc_gamma_init(pdrv);
}

static void lcd_venc_change_timing(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal;

	if (pdrv->vmode_switch) {
		lcd_venc_set_timing(pdrv);
	} else {
		htotal = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
		vtotal = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

		if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_period != htotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT,
					pdrv->curr_dev->dev_cfg.timing.act_timing.h_period - 1);
		}
		if (pdrv->curr_dev->dev_cfg.timing.act_timing.v_period != vtotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT,
					pdrv->curr_dev->dev_cfg.timing.act_timing.v_period - 1);
		}
		LCD_DBG(pdrv, "venc changed: %d,%d",
			pdrv->curr_dev->dev_cfg.timing.act_timing.h_period,
			pdrv->curr_dev->dev_cfg.timing.act_timing.v_period);
	}

	aml_lcd_notifier_call_chain(LCD_EVENT_BACKLIGHT_UPDATE, (void *)pdrv);
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag)
		lcd_vcbus_write(ENCL_VIDEO_EN, 1);
	else
		lcd_vcbus_write(ENCL_VIDEO_EN, 0);
}

static int lcd_venc_get_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int init_state;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN);
	return init_state;
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int init_state;
	struct lcd_boot_ctrl_s *boot_ctrl = pdrv->boot_ctrl;
	unsigned int val = 0;
	unsigned char valid;

	pconf->timing.act_timing.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
		- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	pconf->timing.act_timing.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
		- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;
	pconf->timing.act_timing.h_period = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
	pconf->timing.act_timing.v_period = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN);

	val = lcd_vcbus_read(L_STH1_HS_ADDR);
	valid = (val >> 9) & 0xf;
	if (boot_ctrl && valid == 0xa) {
		boot_ctrl->init_level = val & 0xf;
		boot_ctrl->if_state = (val >> 4) & 0x1;
		boot_ctrl->dccd_flag = (val >> 5) & 0x1;
		boot_ctrl->mute_flag = (val >> 6) & 0x1;

		val = lcd_vcbus_read(L_STH1_HE_ADDR);
		boot_ctrl->frame_rate = val & 0x1fff;

		val = lcd_vcbus_read(L_STH1_VS_ADDR);
		boot_ctrl->lcd_type = val & 0xf;
		boot_ctrl->clk_mode = (val >> 4) & 0xf;
		boot_ctrl->ppc = (val >> 8) & 0x3;
		boot_ctrl->custom_pinmux = (val >> 10) & 0x1;
		boot_ctrl->bl_state = (val >> 11) & 0x1;

		val = lcd_vcbus_read(L_STH1_VE_ADDR);
		boot_ctrl->advanced_flag = val & 0xff;

		init_state |= 0x2;
	}

	return init_state;
}

static void lcd_venc_set_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal = pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;

	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, vtotal - 1);
}

static unsigned int lcd_venc_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cnt = lcd_vcbus_getb(ENCL_INFO_READ, 16, 13);

	return cnt;
}

static unsigned int lcd_venc_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cnt = lcd_vcbus_getb(ENCL_INFO_READ, 29, 3);

	return cnt;
}

static void lcd_venc_set_htotal(struct aml_lcd_drv_s *pdrv, unsigned int htotal)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, htotal - 1);
}

static void lcd_venc_set_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
}

static int lcd_venc_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i, n, len = 0;
	unsigned int *reg_table = NULL, *reg_ctrl = NULL, size_encl = 0, size_ctrl = 0;
	unsigned int encl_reg_dft[] = {
		VPU_VIU_VENC_MUX_CTRL,
		ENCL_VIDEO_EN,
		ENCL_VIDEO_MODE,
		ENCL_VIDEO_MODE_ADV,
		ENCL_VIDEO_MAX_PXCNT,
		ENCL_VIDEO_MAX_LNCNT,
		ENCL_VIDEO_HAVON_BEGIN,
		ENCL_VIDEO_HAVON_END,
		ENCL_VIDEO_VAVON_BLINE,
		ENCL_VIDEO_VAVON_ELINE,
		ENCL_VIDEO_HSO_BEGIN,
		ENCL_VIDEO_HSO_END,
		ENCL_VIDEO_VSO_BEGIN,
		ENCL_VIDEO_VSO_END,
		ENCL_VIDEO_VSO_BLINE,
		ENCL_VIDEO_VSO_ELINE,
		ENCL_VIDEO_H_PRE_DE_BEGIN,
		ENCL_VIDEO_H_PRE_DE_END,
		ENCL_VIDEO_V_PRE_DE_BLINE,
		ENCL_VIDEO_V_PRE_DE_ELINE,
		ENCL_VIDEO_RGBIN_CTRL,
		L_GAMMA_CNTL_PORT,
		L_RGB_BASE_ADDR,
		L_RGB_COEFF_ADDR,
		L_POL_CNTL_ADDR,
		L_DITH_CNTL_ADDR
	};
	static unsigned int ctrl_reg_tl1[] = {
		ENCL_INBUF_CNTL0,
		ENCL_INBUF_CNTL1,
	};

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
	case LCD_CHIP_TXHD2:
		reg_table = encl_reg_dft;
		size_encl = ARRAY_SIZE(encl_reg_dft);
		reg_ctrl = ctrl_reg_tl1;
		size_ctrl = ARRAY_SIZE(ctrl_reg_tl1);
		break;
	case LCD_CHIP_S6:
	default:
		reg_table = encl_reg_dft;
		size_encl = ARRAY_SIZE(encl_reg_dft);
		break;
	}

	for (i = 0; i < size_encl; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
			reg_table[i], lcd_vcbus_read(reg_table[i]));
	}
	if (reg_ctrl) {
		for (i = 0; i < size_ctrl; i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
				reg_ctrl[i], lcd_vcbus_read(reg_ctrl[i]));
		}
	}

	return len;
}

int lcd_venc_op_init_dft(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = lcd_venc_wait_vsync;
	venc_op->get_max_lcnt = lcd_venc_get_max_lint_cnt;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_bist_set;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_change = lcd_venc_change_timing;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->get_venc_state = lcd_venc_get_state;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_set_vrr_recovery;
	venc_op->get_encl_line_cnt = lcd_venc_get_encl_line_cnt;
	venc_op->get_encl_frm_cnt = lcd_venc_get_encl_frm_cnt;
	venc_op->venc_set_htotal = lcd_venc_set_htotal;
	venc_op->venc_set_vtotal = lcd_venc_set_vtotal;
	venc_op->venc_reg_dump = lcd_venc_reg_dump;

	return 0;
};
