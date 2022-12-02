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

	if (!pdrv)
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

static void lcd_venc_gamma_check_en(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1))
		pdrv->gamma_en_flag = 1;
	else
		pdrv->gamma_en_flag = 0;
	LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, pdrv->gamma_en_flag);
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag) {
		if (pdrv->gamma_en_flag) {
			if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1) == 0) {
				lcd_vcbus_setb(L_GAMMA_CNTL_PORT, 1, 0, 1);
				LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
			}
		}
	} else {
		if (pdrv->gamma_en_flag) {
			if (lcd_vcbus_getb(L_GAMMA_CNTL_PORT, 0, 1)) {
				lcd_vcbus_setb(L_GAMMA_CNTL_PORT, 0, 0, 1);
				LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
			}
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

static int lcd_venc_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_active, video_on_pixel;

	if (num >= LCD_ENC_TST_NUM_MAX)
		return -1;

	lcd_queue_work(&pdrv->test_check_work);

	h_active = pdrv->config.basic.h_active;
	video_on_pixel = pdrv->config.timing.hstart;
	if (num > 0)
		lcd_venc_gamma_debug_test_en(pdrv, 0);
	else
		lcd_venc_gamma_debug_test_en(pdrv, 1);

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
		LCDPR("[%d]: show test pattern: %s\n", pdrv->index, lcd_enc_tst_str[num]);

	return 0;
}

static void lcd_venc_gamma_init(struct aml_lcd_drv_s *pdrv)
{
	int index = pdrv->index;

	if (pdrv->lcd_pxp)
		return;

	aml_lcd_notifier_call_chain(LCD_EVENT_GAMMA_UPDATE, &index);
	lcd_venc_gamma_check_en(pdrv);
}

static void lcd_venc_set_tcon(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;

	lcd_vcbus_write(L_RGB_BASE_ADDR, 0x0);
	lcd_vcbus_write(L_RGB_COEFF_ADDR, 0x400);

	switch (pconf->basic.lcd_bits) {
	case 6:
		lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x600);
		break;
	case 8:
		lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x400);
		break;
	case 10:
	default:
		lcd_vcbus_write(L_DITH_CNTL_ADDR,  0x0);
		break;
	}

	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 1);
		if (pconf->timing.vsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 1, 1);
		break;
	case LCD_VBYONE:
		if (pconf->timing.hsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 1);
		if (pconf->timing.vsync_pol)
			lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 1, 1);
		break;
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
	case LCD_EDP:
		lcd_vcbus_setb(L_POL_CNTL_ADDR, 1, 0, 1);
		break;
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
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int hstart, hend, vstart, vend;
	unsigned int pre_de_vs, pre_de_ve, pre_de_hs, pre_de_he;

	hstart = pconf->timing.hstart;
	hend = pconf->timing.hend;
	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT, pconf->basic.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, pconf->basic.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN, hstart);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END,   hend);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE, vstart);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE, vend);
	if (pconf->basic.lcd_type == LCD_P2P ||
	    pconf->basic.lcd_type == LCD_MLVDS) {
		switch (pdrv->data->chip_type) {
		case LCD_CHIP_TL1:
		case LCD_CHIP_TM2:
			pre_de_vs = vstart - 1 - 4;
			pre_de_ve = vstart - 1;
			pre_de_hs = hstart + PRE_DE_DELAY;
			pre_de_he = pconf->basic.h_active - 1 + pre_de_hs;
			break;
		default:
			pre_de_vs = vstart - 8;
			pre_de_ve = pconf->basic.v_active + pre_de_vs;
			pre_de_hs = hstart + PRE_DE_DELAY;
			pre_de_he = pconf->basic.h_active - 1 + pre_de_hs;
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
	lcd_vcbus_write(ENCL_INBUF_CNTL1, (2 << 14) | (pconf->basic.h_active - 1));
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
	/* default black pattern */
	lcd_vcbus_write(ENCL_TST_MDSEL, 0);
	lcd_vcbus_write(ENCL_TST_Y, 0);
	lcd_vcbus_write(ENCL_TST_CB, 0);
	lcd_vcbus_write(ENCL_TST_CR, 0);
	lcd_vcbus_write(ENCL_TST_EN, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, 0, 3, 1);

	lcd_vcbus_write(ENCL_VIDEO_EN, 1);

	lcd_venc_gamma_init(pdrv);
}

static void lcd_venc_change_timing(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal;

	if (pdrv->vmode_update) {
		lcd_timing_init_config(pdrv);
		lcd_venc_set_timing(pdrv);
	} else {
		htotal = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
		vtotal = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

		if (pdrv->config.basic.h_period != htotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT,
					pdrv->config.basic.h_period - 1);
		}
		if (pdrv->config.basic.v_period != vtotal) {
			lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT,
					pdrv->config.basic.v_period - 1);
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: venc changed: %d,%d\n",
			      pdrv->index,
			      pdrv->config.basic.h_period,
			      pdrv->config.basic.v_period);
		}
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

static void lcd_venc_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag)
{
	if (flag) {
		lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL, 3);
		lcd_vcbus_write(ENCL_TST_MDSEL, 0);
		lcd_vcbus_write(ENCL_TST_Y, 0);
		lcd_vcbus_write(ENCL_TST_CB, 0);
		lcd_vcbus_write(ENCL_TST_CR, 0);
		lcd_vcbus_write(ENCL_TST_EN, 1);
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, 0, 3, 1);
	} else {
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV, 1, 3, 1);
		lcd_vcbus_write(ENCL_TST_EN, 0);
	}
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int init_state;

	pconf->basic.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END)
		- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN) + 1;
	pconf->basic.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE)
		- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE) + 1;
	pconf->basic.h_period = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT) + 1;
	pconf->basic.v_period = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;

	lcd_venc_gamma_check_en(pdrv);

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN);
	return init_state;
}

static void lcd_test_pattern_check(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(work, struct aml_lcd_drv_s, test_check_work);
	aml_lcd_notifier_call_chain(LCD_EVENT_TEST_PATTERN, (void *)pdrv);
}

static void lcd_venc_set_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal = pdrv->config.basic.v_period;

	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT, vtotal);
}

static unsigned int lcd_venc_get_encl_lint_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt = lcd_vcbus_getb(ENCL_INFO_READ, 16, 13);

	return line_cnt;
}

int lcd_venc_op_init_dft(struct aml_lcd_drv_s *pdrv, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = lcd_venc_wait_vsync;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_debug_test;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_change = lcd_venc_change_timing;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->mute_set = lcd_venc_mute_set;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_set_vrr_recovery;
	venc_op->get_encl_lint_cnt = lcd_venc_get_encl_lint_cnt;

	INIT_WORK(&pdrv->test_check_work, lcd_test_pattern_check);

	return 0;
};
