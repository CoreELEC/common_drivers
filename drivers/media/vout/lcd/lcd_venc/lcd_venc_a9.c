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
	unsigned int reg, offset;
	int i = 0;

	if (!pdrv)
		return;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCL_STAT + offset;

	line_cnt = 0x1fff;
	line_cnt_previous = lcd_vcbus_getb(reg, 16, 13);
	while (i++ < LCD_WAIT_VSYNC_TIMEOUT) {
		line_cnt = lcd_vcbus_getb(reg, 16, 13);
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
	unsigned int offset, reg, line_cnt;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = ENCL_VIDEO_MAX_LNCNT + offset;

	line_cnt = lcd_vcbus_read(reg) + 1;
	/*LCDPR("[%d]: %s: line_cnt=%d", pdrv->index, __func__, line_cnt); */

	return line_cnt;
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int reg, offset;

	offset = pdrv->data->offset_venc_data[pdrv->index];
	reg = LCD_GAMMA_CNTL_PORT0 + offset;

	if (flag) {
		if (lcd_vcbus_getb(reg, 0, 1) == 0) {
			lcd_vcbus_setb(reg, 1, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	} else {
		if (lcd_vcbus_getb(reg, 0, 1)) {
			lcd_vcbus_setb(reg, 0, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	}
}

struct lcd_enc_test_t {
	char *name;
	unsigned int mode;
	unsigned int y;
	unsigned int cb;
	unsigned int cr;
	unsigned int en;
	unsigned int vfifo_en;
	unsigned int rgb_in;
};

static struct lcd_enc_test_t lcd_enc_tst_a9[] = {
	{"0-None",         0,    0x200,   0x200, 0x200, 0, 1, 3},  /* 0 */
	{"1-Color Bar",    1,    0x200,   0x200, 0x200, 1, 0, 1},  /* 1 */
	{"2-Thin Line",    2,    0x200,   0x200, 0x200, 1, 0, 1},  /* 2 */
	{"3-Dot Grid",     3,    0x200,   0x200, 0x200, 1, 0, 1},  /* 3 */
	{"4-Gray",         0,    0x1ff,   0x1ff, 0x1ff, 1, 0, 3},  /* 4 */
	{"5-Red",          0,    0x3ff,     0x0,   0x0, 1, 0, 3},  /* 5 */
	{"6-Green",        0,      0x0,   0x3ff,   0x0, 1, 0, 3},  /* 6 */
	{"7-Blue",         0,      0x0,     0x0, 0x3ff, 1, 0, 3},  /* 7 */
	{"8-Black",        0,      0x0,     0x0,   0x0, 1, 0, 3},  /* 8 */
	{"9-X icon",       4,    0x3ff,    0x10,   0x9, 1, 0, 2},  /* 9 */
	{"10-Gray Scale",  5, 0xffffffff,   0x7,   0x7, 1, 0, 3},  /* 10 */
	{"11-Red Scale",   5,      0x1,     0x0,   0x1, 1, 0, 3},  /* 11 */
	{"12-Green Scale", 5,      0x1,     0x0,   0x2, 1, 0, 3},  /* 12 */
	{"13-Blue Scale",  5,      0x1,     0x0,   0x4, 1, 0, 3},  /* 13 */
	{"14-Window",      6,    0x3ff,    0x10,   0x9, 1, 0, 3},  /* 14 */
};

static int lcd_venc_bist_set(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int start, width, height, offset;
	unsigned int gcd_num;

	offset = pdrv->data->offset_venc[pdrv->index];
	start = pdrv->curr_dev->dev_cfg.timing.hstart;
	width =  pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 9;

	lcd_venc_wait_vsync(pdrv);
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, lcd_enc_tst_a9[num].rgb_in);
	lcd_vcbus_write(ENCL_TST_Y + offset, lcd_enc_tst_a9[num].y);
	if (num == 9) {
		width = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
		height = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
		gcd_num = gcd(width, height);
		lcd_vcbus_write(ENCL_TST_CB + offset, width / gcd_num);
		lcd_vcbus_write(ENCL_TST_CR + offset, height / gcd_num);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, height);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, width);
	} else if (num == 14) {
		lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 1);
		lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 2);
		pdrv->window_attr.pos.start_x =
				pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 3;
		pdrv->window_attr.pos.start_y =
				pdrv->curr_dev->dev_cfg.timing.act_timing.v_active / 3;
		pdrv->window_attr.color_in.red = 0;
		pdrv->window_attr.color_in.green = 0;
		pdrv->window_attr.color_in.blue = 0;
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, pdrv->window_attr.pos.start_x);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, pdrv->window_attr.pos.start_y);
		lcd_vcbus_write(ENCL_TST_Y + offset, pdrv->window_attr.color_in.red);
		lcd_vcbus_write(ENCL_TST_CB + offset, pdrv->window_attr.color_in.green);
		lcd_vcbus_write(ENCL_TST_CR + offset, pdrv->window_attr.color_in.blue);
		LCDPR("start point x=%d y=%d\n",
			pdrv->window_attr.pos.start_x, pdrv->window_attr.pos.start_y);

		pdrv->window_attr.pos.end_x =
				pdrv->curr_dev->dev_cfg.timing.act_timing.h_active * 2 / 3;
		pdrv->window_attr.pos.end_y =
				pdrv->curr_dev->dev_cfg.timing.act_timing.v_active * 2 / 3;
		pdrv->window_attr.color_out.red = 0x3ff;
		pdrv->window_attr.color_out.green = 0x3ff;
		pdrv->window_attr.color_out.blue = 0x3ff;
		lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 3);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, pdrv->window_attr.pos.end_x);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, pdrv->window_attr.pos.end_y);
		lcd_vcbus_write(ENCL_TST_Y + offset, pdrv->window_attr.color_out.red);
		lcd_vcbus_write(ENCL_TST_CB + offset, pdrv->window_attr.color_out.green);
		lcd_vcbus_write(ENCL_TST_CR + offset, pdrv->window_attr.color_out.blue);
		LCDPR("end point x=%d y=%d\n",
			pdrv->window_attr.pos.end_x, pdrv->window_attr.pos.end_y);

		width = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
		height = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
		lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 0);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, height);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, width);
	} else {
		lcd_vcbus_write(ENCL_TST_CB + offset, lcd_enc_tst_a9[num].cb);
		lcd_vcbus_write(ENCL_TST_CR + offset, lcd_enc_tst_a9[num].cr);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, start - 2);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, width);
	}
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, lcd_enc_tst_a9[num].mode);
	lcd_vcbus_setb(ENCL_TST_EN + offset, lcd_enc_tst_a9[num].en, 0, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, lcd_enc_tst_a9[num].vfifo_en, 3, 1);
	if (num > 0) {
		LCDPR("[%d]: show test pattern: %s\n",
		      pdrv->index, lcd_enc_tst_a9[num].name);
	}

	return 0;
}

static int lcd_venc_window_attr_change(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	if (lcd_vcbus_getb(ENCL_TST_MDSEL + offset, 0, 3) != 6) {
		LCDPR("not in window pattern mode\n");
		return -1;
	}
	lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 1); /* clear line and row cnt */
	lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 2); /* window in */
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, pdrv->window_attr.pos.start_x);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, pdrv->window_attr.pos.start_y);
	lcd_vcbus_write(ENCL_TST_Y + offset, pdrv->window_attr.color_in.red);
	lcd_vcbus_write(ENCL_TST_CB + offset, pdrv->window_attr.color_in.green);
	lcd_vcbus_write(ENCL_TST_CR + offset, pdrv->window_attr.color_in.blue);
	LCDPR("start point x=%d y=%d\n",
		pdrv->window_attr.pos.start_x,
		pdrv->window_attr.pos.start_y);

	lcd_vcbus_write(ENCL_TST_VDCNT_STSET + offset, 3); /* window out */
	lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, pdrv->window_attr.pos.end_x);
	lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, pdrv->window_attr.pos.end_y);
	lcd_vcbus_write(ENCL_TST_Y + offset, pdrv->window_attr.color_out.red);
	lcd_vcbus_write(ENCL_TST_CB + offset, pdrv->window_attr.color_out.green);
	lcd_vcbus_write(ENCL_TST_CR + offset, pdrv->window_attr.color_out.blue);
	LCDPR("end point x=%d y=%d\n",
		pdrv->window_attr.pos.end_x,
		pdrv->window_attr.pos.end_y);
	return 0;
}

static int lcd_venc_probe_cursor(struct aml_lcd_drv_s *pdrv, struct lcd_cursor_attr_s *cursor_attr)
{
	unsigned int offset = pdrv->data->offset_venc[pdrv->index];

	if (cursor_attr->status) {
		if (cursor_attr->x >= pdrv->curr_dev->dev_cfg.timing.act_timing.h_active ||
			cursor_attr->y >= pdrv->curr_dev->dev_cfg.timing.act_timing.v_active)
			return -1;
		lcd_vcbus_setb(VPU_VENC_PROBE_CTRL + offset,
				3 | (cursor_attr->mode << 2), 0, 3);
		if (cursor_attr->x && cursor_attr->y)
			lcd_vcbus_write(VPU_VENC_PROBE_POS + offset,
					(cursor_attr->y << 16) | cursor_attr->x);
		lcd_vcbus_write(VPU_VENC_PROBE_SIZE + offset,
				pdrv->curr_dev->dev_cfg.timing.act_timing.h_active);
		lcd_vcbus_write(VPU_VENC_PROBE_HL_COL0 + offset,
				cursor_attr->green << 17 | cursor_attr->blue);
		lcd_vcbus_write(VPU_VENC_PROBE_HL_COL1 + offset, cursor_attr->red);
	} else {
		lcd_vcbus_setb(VPU_VENC_PROBE_CTRL + offset, 0, 0, 3);
	}
	return 0;
}

static void lcd_venc_bist_change(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
				 unsigned int level_g, unsigned int level_b)
{
	struct lcd_enc_test_t *pcur_test = lcd_enc_tst_a9;
	unsigned int bist_num, r_data, g_data, b_data, offset;

	if (level_r > 0x3ff) {
		bist_num = 0;
		r_data = 0;
		g_data = 0;
		b_data = 0;
	} else {
		bist_num = 8;
		r_data = level_r;
		g_data = level_g;
		b_data = level_b;
	}

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, pcur_test[bist_num].rgb_in);
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, pcur_test[bist_num].mode);
	lcd_vcbus_write(ENCL_TST_Y + offset, r_data);
	lcd_vcbus_write(ENCL_TST_CB + offset, g_data);
	lcd_vcbus_write(ENCL_TST_CR + offset, b_data);
	lcd_vcbus_setb(ENCL_TST_EN + offset, pcur_test[bist_num].en, 0, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, pcur_test[bist_num].vfifo_en, 3, 1);
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

static void lcd_venc_set_timing(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int hstart, hend, vstart, vend;
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	hstart = pconf->timing.hstart;
	hend = pconf->timing.hend;
	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 0, 15, 1); /* bit[15] shadown disable */

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, pconf->timing.act_timing.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, pconf->timing.act_timing.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, hstart);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset,   hend);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, vstart);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, vend);
	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset,   pconf->timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset,   pconf->timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, pconf->timing.vs_ve_addr);

	lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset,
			(4 << 13) | (pconf->timing.act_timing.h_active - 1));

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 1, 15, 1); /* bit[15] shadown en */
}

static void lcd_venc_set_dummy_a9(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc[pdrv->index];

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 0, 15, 1); /* bit[15] shadown disable */

	lcd_vcbus_write(ENCL_VIDEO_MODE + offset, 0x8000);
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x18);
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000);

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, 951);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, 524);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, 80);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset, 799);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, 22);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, 501);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset,   20);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset,   0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, 5);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 1, 15, 1); /* bit[15] shadown en */
}

static void dual_set_a9(struct aml_lcd_drv_s *pdrv, u8 to_port, u8 en, u8 dual_mode)
{
	// uint16_t single_ha = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 2;
	int left_hsize, right_hsize;

	if (pdrv->index != 0) {
		LCD_PR(pdrv, "dual-port on VENC %u not supported", pdrv->index);
		return;
	}

	if (to_port != 1) {
		LCD_PR(pdrv, "dual-port to PORT-%c not supported", 'A' + to_port);
		return;
	}

	if (en == 0) {
		lcd_vcbus_write(VPU_VENC_RGN_CTRL, 0);
		return;
	}

	LCD_PR(pdrv, "set dual split %s mode (%c->[%c] | %c->[%c])",
		(dual_mode == LCD_DUAL_PORT_L_R || dual_mode == LCD_DUAL_PORT_R_L) ?
			"Left-Right" : "Odd-Even",
		dual_mode == LCD_DUAL_PORT_L_R ? 'L' :
			(dual_mode == LCD_DUAL_PORT_R_L ? 'R' :
				(dual_mode == LCD_DUAL_PORT_O_E ? 'O' : 'E')), 'A' + pdrv->index,
		dual_mode == LCD_DUAL_PORT_L_R ? 'R' :
			(dual_mode == LCD_DUAL_PORT_R_L ? 'L' :
				(dual_mode == LCD_DUAL_PORT_O_E ? 'E' : 'O')), 'A' + to_port);

	if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_active % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_width % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_bp % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_fp % 2) {
		LCD_PR(pdrv, "dual-port H-active/bp/fp/sync should be even value");
	}

	/* @reg: VPU_VENC_RGN_RSIZE 0x278a
	 * [23:12] reg_region0_size: output region0 size (hsize+1)/2
	 * [11:0]  reg_region1_size: output region0 size hsize - reg_region0_size
	 */
	left_hsize = (pdrv->curr_dev->dev_cfg.timing.act_timing.h_active + 1) >> 1;
	right_hsize = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active - left_hsize;
	lcd_vcbus_write(VPU_VENC_RGN_RSIZE, ((right_hsize << 12) | left_hsize));
	/* @reg: VPU_DISP_WRAP_CTRL 0x278b
	 * [7:5] reg_difx_link_prot: 0:disp0 sync venc0 1:disp1 sync venc0 2:disp2 sync venc0
	 * [4] reg_splt2_mode: enable 1ppc->2ppc
	 * [2:0] reg_venc0_difx_link[2:0]:
	 */
	lcd_vcbus_write(VPU_DISP_WRAP_CTRL, (1 << 4) | (1 << 1) | (1 << 0));

	/* @reg: VPU_VENC_RGN_CTRL 0x2789
	 * [11:10] reg_gclk_ctrl: ram clk
	 * [9:8] reg_gclk_ctrl: logic clk
	 * [7] reg_rgn_swap: 0:(0-left, 1right) 1:converse
	 * [6] sw_rst: rgn_buffer soft reset
	 * [5] reg_sync_ctrl: rgn buffer related en signal sync enable
	 * [4] reg_sync_ctrl: vsync polarity 0:up edge 1:down edge
	 * [3] reg_vsync_ctrl: vsync polarity 0:positive 1:negative
	 * [2] reg_hsync_ctrl: hsync polarity 0:positive 1:negative
	 * [1] oe_sp_en: odd even split enable
	 * [0] rgn_en: vbo rgn_buffer enable
	 */
	lcd_vcbus_write(VPU_VENC_RGN_CTRL,
		((dual_mode == LCD_DUAL_PORT_L_R || dual_mode == LCD_DUAL_PORT_R_L) << 0 |
		 (dual_mode == LCD_DUAL_PORT_R_L || dual_mode == LCD_DUAL_PORT_O_E) << 7 |
		(dual_mode == LCD_DUAL_PORT_O_E || dual_mode == LCD_DUAL_PORT_E_O) << 1));
}

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_disp_viu_ctrl, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	//lcd_vcbus_write(ENCL_VIDEO_MODE + offset, 0x40); /* bit[15] shadown en */
	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x18); /* Sampling rate: 1 */
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000); /* bypass filter */
	lcd_vcbus_setb(ENCL_TST_EN + offset, 3, 1, 2); /*vsync latch enable*/

	if (pdrv->status & LCD_CTRL_DUMMY)
		lcd_venc_set_dummy_a9(pdrv);
	else
		lcd_venc_set_timing(pdrv);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
	//restore test pattern
	lcd_venc_bist_set(pdrv, pdrv->test_state);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	switch (pdrv->index) {
	case 0:
		reg_disp_viu_ctrl = VPU_DISP_VIU0_CTRL;
		break;
	case 1:
		reg_disp_viu_ctrl = VPU_DISP_VIU1_CTRL;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}

	/*
	 * bit31: lvds enable
	 * bit30: vx1 enable
	 * bit29: hdmitx enable
	 * bit28: dsi_edp enable
	 */
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
		lcd_vcbus_write(reg_disp_viu_ctrl, (1 << 31) |
						(0 << 30) |
						(0 << 29) |
						(0 << 28));
		break;
	case LCD_VBYONE:
		lcd_vcbus_write(reg_disp_viu_ctrl, (0 << 31) |
						(1 << 30) |
						(0 << 29) |
						(0 << 28));
		break;
	case LCD_MIPI:
		lcd_vcbus_write(reg_disp_viu_ctrl, (0 << 31) |
						(0 << 30) |
						(0 << 29) |
						(1 << 28));
		break;
	default:
		break;
	}
	lcd_vcbus_setb(VPU_VENC_CTRL + offset, 2, 0, 2);

	lcd_venc_gamma_init(pdrv);

	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI &&
	    pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0)) {
		dual_set_a9(pdrv, 1, 1,
			(pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg >> 4) & 0Xf);
	}
}

static void lcd_venc_change_framerate(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	htotal = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT + offset) + 1;
	vtotal = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT + offset) + 1;

	if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_period != htotal) {
		lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset,
				pdrv->curr_dev->dev_cfg.timing.act_timing.h_period - 1);
	}
	if (pdrv->curr_dev->dev_cfg.timing.act_timing.v_period != vtotal) {
		lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset,
				pdrv->curr_dev->dev_cfg.timing.act_timing.v_period - 1);
	}
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	if (flag)
		lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);
	else
		lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);
}

static void lcd_venc_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	if (flag) {
		lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
		lcd_vcbus_write(ENCL_TST_MDSEL + offset, 0);
		lcd_vcbus_write(ENCL_TST_Y + offset, 0);
		lcd_vcbus_write(ENCL_TST_CB + offset, 0);
		lcd_vcbus_write(ENCL_TST_CR + offset, 0);
		lcd_ana_setb(ANACTRL_DIF_PHY_CNTL9, 0, 24, 2);
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, 0, 3, 1);
	} else {
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, 1, 3, 1);
		lcd_vcbus_setb(ENCL_TST_EN + offset, 0, 0, 1);
	}
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int offset, init_state;
	unsigned int val = 0;
	struct lcd_boot_ctrl_s *boot_ctrl = pdrv->boot_ctrl;
	unsigned char valid;

	offset = pdrv->data->offset_venc[pdrv->index];

	pconf->timing.act_timing.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END + offset)
		- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN + offset) + 1;
	pconf->timing.act_timing.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE + offset)
		- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE + offset) + 1;
	pconf->timing.act_timing.h_period = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT + offset) + 1;
	pconf->timing.act_timing.v_period = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT + offset) + 1;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN + offset);

	val = lcd_vcbus_read(L_STH1_HS_ADDR_T7 + offset);
	valid = (val >> 9) & 0xf;
	if (boot_ctrl && valid == 0xa) {
		boot_ctrl->init_level = val & 0xf;
		boot_ctrl->if_state = (val >> 4) & 0x1;
		boot_ctrl->dccd_flag = (val >> 5) & 0x1;
		boot_ctrl->mute_flag = (val >> 6) & 0x1;

		val = lcd_vcbus_read(L_STH1_HE_ADDR_T7 + offset);
		boot_ctrl->frame_rate = val & 0x1fff;

		val = lcd_vcbus_read(L_STH1_VS_ADDR_T7 + offset);
		boot_ctrl->lcd_type = val & 0xf;
		boot_ctrl->clk_mode = (val >> 4) & 0xf;
		boot_ctrl->ppc = (val >> 8) & 0x3;
		boot_ctrl->bl_state = (val >> 11) & 0x1;

		val = lcd_vcbus_read(L_STH1_VE_ADDR_T7 + offset);
		boot_ctrl->advanced_flag = val & 0xff;

		init_state |= 0x2;
	}

	return init_state;
}

static void lcd_venc_set_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	vtotal = pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;

	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
}

static unsigned int lcd_venc_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, offset, cnt;

	if (!pdrv)
		return 0;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCL_STAT + offset;

	cnt = lcd_vcbus_getb(reg, 16, 13);
	return cnt;
}

static unsigned int lcd_venc_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, offset, cnt;

	if (!pdrv)
		return 0;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCL_STAT + offset;

	cnt = lcd_vcbus_getb(reg, 13, 3);
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
	unsigned int *reg_table = NULL, size_encl = 0;
	unsigned int encl_0_reg[] = {
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
		ENCL_VIDEO_RGBIN_CTRL,
		ENCL_INBUF_CNTL0,
		ENCL_INBUF_CNTL1,
		VPU_DISP_VIU0_CTRL,
		VPU_VENC_CTRL,
		LCD_GAMMA_CNTL_PORT0,
		LCD_RGB_BASE_ADDR,
		LCD_RGB_COEFF_ADDR,
		LCD_POL_CNTL_ADDR,
		LCD_DITH_CNTL_ADDR
	};
	unsigned int encl_1_reg[] = {
		VPU_VIU_VENC_MUX_CTRL,
		ENCL_VIDEO_EN + 0x600,
		ENCL_VIDEO_MODE + 0x600,
		ENCL_VIDEO_MODE_ADV + 0x600,
		ENCL_VIDEO_MAX_PXCNT + 0x600,
		ENCL_VIDEO_MAX_LNCNT + 0x600,
		ENCL_VIDEO_HAVON_BEGIN + 0x600,
		ENCL_VIDEO_HAVON_END + 0x600,
		ENCL_VIDEO_VAVON_BLINE + 0x600,
		ENCL_VIDEO_VAVON_ELINE + 0x600,
		ENCL_VIDEO_HSO_BEGIN + 0x600,
		ENCL_VIDEO_HSO_END + 0x600,
		ENCL_VIDEO_VSO_BEGIN + 0x600,
		ENCL_VIDEO_VSO_END + 0x600,
		ENCL_VIDEO_VSO_BLINE + 0x600,
		ENCL_VIDEO_VSO_ELINE + 0x600,
		ENCL_VIDEO_RGBIN_CTRL + 0x600,
		ENCL_INBUF_CNTL0 + 0x600,
		ENCL_INBUF_CNTL1 + 0x600,
		VPU_DISP_VIU1_CTRL,
		VPU_VENC_CTRL + 0x600,
		LCD_GAMMA_CNTL_PORT0 + 0x100,
		LCD_RGB_BASE_ADDR + 0x100,
		LCD_RGB_COEFF_ADDR + 0x100,
		LCD_POL_CNTL_ADDR + 0x100,
		LCD_DITH_CNTL_ADDR + 0x100
	};

	if (!pdrv || !pdrv->data)
		return 0;

	if (pdrv->index == 1) {
		reg_table = encl_1_reg;
		size_encl = ARRAY_SIZE(encl_1_reg);
	} else {
		reg_table = encl_0_reg;
		size_encl = ARRAY_SIZE(encl_0_reg);
	}
	for (i = 0; i < size_encl; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
			reg_table[i], lcd_vcbus_read(reg_table[i]));
	}

	return len;
}

int lcd_venc_op_init_a9(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = lcd_venc_wait_vsync;
	venc_op->get_max_lcnt = lcd_venc_get_max_lint_cnt;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_bist_set;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_fr_change = lcd_venc_change_framerate;
	venc_op->venc_set_dummy = lcd_venc_set_dummy_a9;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_set_vrr_recovery;
	venc_op->get_encl_line_cnt = lcd_venc_get_encl_line_cnt;
	venc_op->get_encl_frm_cnt = lcd_venc_get_encl_frm_cnt;
	venc_op->venc_set_htotal = lcd_venc_set_htotal;
	venc_op->venc_set_vtotal = lcd_venc_set_vtotal;
	venc_op->venc_reg_dump = lcd_venc_reg_dump;
	venc_op->venc_bist_change = lcd_venc_bist_change;
	venc_op->mute_set = lcd_venc_mute_set;
	venc_op->window_attr_change = lcd_venc_window_attr_change;
	venc_op->venc_probe_cursor = lcd_venc_probe_cursor;

	return 0;
};
