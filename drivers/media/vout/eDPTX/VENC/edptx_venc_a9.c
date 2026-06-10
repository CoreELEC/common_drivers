// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef CONFIG_AML_VPP
#include <linux/amlogic/media/vpp/vpp.h>
#endif
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "edptx_venc.h"
#include "../edptx_reg_op.h"
#include "../edptx_common.h"

static void dptx_venc_wait_vsync(struct edptx_drv_s *edp)
{
	unsigned int offset, reg;
	int line_cnt, line_cnt_previous;
	int i = 0;

	offset = edp->data->offset_venc[edp->idx];
	reg = VPU_VENCP_STAT + offset;

	line_cnt_previous = edp_vcbus_getb(reg, 16, 13);
	while (i++ < DPTX_WAIT_VSYNC_TIMEOUT) {
		line_cnt = edp_vcbus_getb(reg, 16, 13);
		if (line_cnt < line_cnt_previous)
			break;
		line_cnt_previous = line_cnt;
		edptx_delay_us(1);
	}
	/*eDP_PR(edp, "line_cnt=%d, line_cnt_previous=%d, i=%d\n",
	 *	line_cnt, line_cnt_previous, i);
	 */
}

static unsigned int dptx_venc_get_max_lint_cnt(struct edptx_drv_s *edp)
{
	unsigned int line_cnt;

	line_cnt = edp_vcbus_read(ENCL_VIDEO_MAX_LNCNT + edp->data->offset_venc[edp->idx]) + 1;
	/*eDP_PR(edp, "[%d]: %s: line_cnt=%d", edp->idx, __func__, line_cnt); */

	return line_cnt;
}

#define LCD_ENC_TST_NUM_MAX    9

struct venc_test_pattern_s {
	char *name;
	u8 mode, tst_en, vfifo_en, rgbin;
	u16 Y, Cb, Cr;
};

static struct venc_test_pattern_s dptx_enc_tst[] = {
/*       name,          mode, tst_en, vfifo_en, rgbin, Y,     Cb,    Cr,   */
	{"0-None",      0,    0,      1,        3,     0x200, 0x200, 0x200},  /* 0 */
	{"1-Color Bar", 1,    1,      0,        1,     0x200, 0x200, 0x200},  /* 1 */
	{"2-Thin Line", 2,    1,      0,        1,     0x200, 0x200, 0x200},  /* 2 */
	{"3-Dot Grid",  3,    1,      0,        1,     0x200, 0x200, 0x200},  /* 3 */
	{"4-Gray",      0,    1,      0,        3,     0x1ff, 0x1ff, 0x1ff},  /* 4 */
	{"5-Red",       0,    1,      0,        3,     0x3ff,   0x0,   0x0},  /* 5 */
	{"6-Green",     0,    1,      0,        3,       0x0, 0x3ff,   0x0},  /* 6 */
	{"7-Blue",      0,    1,      0,        3,       0x0,   0x0, 0x3ff},  /* 7 */
	{"8-Black",     0,    1,      0,        3,       0x0,   0x0,   0x0},  /* 8 */
	{"9-White",     0,    1,      0,        3,     0x3ff, 0x3ff, 0x3ff},  /* 9 */
};

static int dptx_venc_debug_test(struct edptx_drv_s *edp, u8 num)
{
	u32 start, width, offset;

	if (num > LCD_ENC_TST_NUM_MAX)
		return -1;

	offset = edp->data->offset_venc[edp->idx];
	start = edp->venc_cfg.hstart;
	width = edp->act_timing.h_act / 8;

	dptx_venc_wait_vsync(edp);
	edp_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, dptx_enc_tst[num].rgbin);
	edp_vcbus_write(ENCL_TST_MDSEL        + offset, dptx_enc_tst[num].mode);
	edp_vcbus_write(ENCL_TST_Y            + offset, dptx_enc_tst[num].Y);
	edp_vcbus_write(ENCL_TST_CB           + offset, dptx_enc_tst[num].Cb);
	edp_vcbus_write(ENCL_TST_CR           + offset, dptx_enc_tst[num].Cr);
	edp_vcbus_write(ENCL_TST_CLRBAR_STRT  + offset, start);
	edp_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, width);
	edp_vcbus_write(ENCL_TST_EN           + offset, dptx_enc_tst[num].tst_en);
	edp_vcbus_setb(ENCL_VIDEO_MODE_ADV    + offset, dptx_enc_tst[num].vfifo_en, 3, 1);
	if (num > 0)
		eDP_PR(edp, "show test pattern: %s", dptx_enc_tst[num].name);

	return 0;
}

static void edptx_venc_set_tcon(struct edptx_drv_s *edp)
{
	unsigned int offset_if, offset_data;

	offset_data   = edp->data->offset_venc_data[edp->idx];
	offset_if    = edp->data->offset_venc_if[edp->idx];

	edp_vcbus_write(LCD_RGB_BASE_ADDR  + offset_data, 0x0);
	edp_vcbus_write(LCD_RGB_COEFF_ADDR + offset_data, 0x400);

	edp_vcbus_write(LCD_DITH_CNTL_ADDR + offset_data, 0x0);
	edp_vcbus_setb(LCD_POL_CNTL_ADDR + offset_data, 1, 0, 1);

	/* DE signal */
	edp_vcbus_write(DE_HS_ADDR + offset_if, edp->venc_cfg.de_hs_addr);
	edp_vcbus_write(DE_HE_ADDR + offset_if, edp->venc_cfg.de_he_addr);
	edp_vcbus_write(DE_VS_ADDR + offset_if, edp->venc_cfg.de_vs_addr);
	edp_vcbus_write(DE_VE_ADDR + offset_if, edp->venc_cfg.de_ve_addr);
	/* Hsync signal */
	edp_vcbus_write(HSYNC_HS_ADDR + offset_if, edp->venc_cfg.hs_hs_addr);
	edp_vcbus_write(HSYNC_HE_ADDR + offset_if, edp->venc_cfg.hs_he_addr);
	edp_vcbus_write(HSYNC_VS_ADDR + offset_if, edp->venc_cfg.hs_vs_addr);
	edp_vcbus_write(HSYNC_VE_ADDR + offset_if, edp->venc_cfg.hs_ve_addr);
	/* Vsync signal */
	edp_vcbus_write(VSYNC_HS_ADDR + offset_if, edp->venc_cfg.vs_hs_addr);
	edp_vcbus_write(VSYNC_HE_ADDR + offset_if, edp->venc_cfg.vs_he_addr);
	edp_vcbus_write(VSYNC_VS_ADDR + offset_if, edp->venc_cfg.vs_vs_addr);
	edp_vcbus_write(VSYNC_VE_ADDR + offset_if, edp->venc_cfg.vs_ve_addr);
}

static void edptx_venc_set_timing(struct edptx_drv_s *edp)
{
	unsigned int offset = edp->data->offset_venc[edp->idx];

	edp_vcbus_write(ENCL_VIDEO_MAX_PXCNT   + offset, edp->act_timing.h_period - 1);
	edp_vcbus_write(ENCL_VIDEO_MAX_LNCNT   + offset, edp->act_timing.v_period - 1);
	edp_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, edp->venc_cfg.hstart);
	edp_vcbus_write(ENCL_VIDEO_HAVON_END   + offset, edp->venc_cfg.hend);
	edp_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, edp->venc_cfg.vstart);
	edp_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, edp->venc_cfg.vend);

	edp_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, edp->venc_cfg.hs_hs_addr);
	edp_vcbus_write(ENCL_VIDEO_HSO_END   + offset, edp->venc_cfg.hs_he_addr);
	edp_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, edp->venc_cfg.vs_hs_addr);
	edp_vcbus_write(ENCL_VIDEO_VSO_END   + offset, edp->venc_cfg.vs_he_addr);
	edp_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, edp->venc_cfg.vs_vs_addr);
	edp_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, edp->venc_cfg.vs_ve_addr);

	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		edp_vcbus_write(ENCL_INBUF_CNTL1 + offset,
			(5 << 13) | (edp->act_timing.h_act - 1));
		edp_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);
		break;
	default:
		break;
	}

	edptx_venc_set_tcon(edp);
}

static void dptx_dual_set_t7(struct edptx_drv_s *edp, u8 en, u8 to_port, u8 dual_mode)
{
	// u16 single_ha = pdrv->config.timing.act_timing.h_active / 2;

	if (edp->idx != 0) {
		eDP_PR(edp, "dual-port on VENC %u not supported", edp->idx);
		return;
	}

	if (to_port != 1) {
		eDP_PR(edp, "dual-port to PORT-%c not supported", 'A' + to_port);
		return;
	}

	if (en == 0) {
		edp_vcbus_write(VPU_VENC_RGN_CTRL, 0);
		return;
	}

	eDP_PR(edp, "set dual split %s mode (%c->[%c] | %c->[%c])",
		(dual_mode == DPTX_DUAL_PORT_L_R || dual_mode == DPTX_DUAL_PORT_R_L) ?
			"Left-Right" : "Odd-Even",
		dual_mode == DPTX_DUAL_PORT_L_R ? 'L' :
			(dual_mode == DPTX_DUAL_PORT_R_L ? 'R' :
				(dual_mode == DPTX_DUAL_PORT_O_E ? 'O' : 'E')), 'A' + edp->idx,
		dual_mode == DPTX_DUAL_PORT_L_R ? 'R' :
			(dual_mode == DPTX_DUAL_PORT_R_L ? 'L' :
				(dual_mode == DPTX_DUAL_PORT_O_E ? 'E' : 'O')), 'A' + to_port);

	if (edp->act_timing.h_act % 2 || edp->act_timing.h_pw % 2 ||
		edp->act_timing.h_bp % 2 || edp->act_timing.h_fp % 2) {
		eDP_ERR(edp, "dual-port H-active/bp/fp/sync should be even value");
	}

	/* @reg: VPU_VENC_RGN_RSIZE 0x278a
	 * [23:12] reg_region0_size: output region0 size (hsize+1)/2
	 * [11:0]  reg_region1_size: output region0 size hsize - reg_region0_size
	 */

	edp_vcbus_write(VPU_VENC_RGN_RSIZE,
		((edp->act_timing.h_act / 2) << 12) | (edp->act_timing.h_act / 2));
	/* @reg: VPU_DISP_WRAP_CTRL 0x278b
	 * [7:5] reg_difx_link_prot: 0:disp0 sync venc0 1:disp1 sync venc0 2:disp2 sync venc0
	 * [4] reg_splt2_mode: enable 1ppc->2ppc
	 * [2:0] reg_venc0_difx_link[2:0]:
	 */
	edp_vcbus_write(VPU_DISP_WRAP_CTRL, 0x13);

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
	edp_vcbus_write(VPU_VENC_RGN_CTRL,
		((dual_mode == DPTX_DUAL_PORT_L_R || dual_mode == DPTX_DUAL_PORT_R_L) << 0 |
		 (dual_mode == DPTX_DUAL_PORT_O_E || dual_mode == DPTX_DUAL_PORT_E_O) << 1 |
		 (dual_mode == DPTX_DUAL_PORT_R_L || dual_mode == DPTX_DUAL_PORT_E_O) << 7));
}

static void edptx_venc_set(struct edptx_drv_s *edp)
{
	unsigned int reg_disp_viu_ctrl, offset;

	offset = edp->data->offset_venc[edp->idx];

	edp_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	edp_vcbus_write(ENCL_VIDEO_MODE      + offset, 0x8000); /* bit[15] shadow en */
	edp_vcbus_write(ENCL_VIDEO_MODE_ADV  + offset, 0x0418); /* Sampling rate: 1 */
	edp_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000); /* bypass filter */

	edptx_venc_set_timing(edp);

	edp_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
	edp_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	switch (edp->idx) {
	case 0:
		reg_disp_viu_ctrl = VPU_DISP_VIU0_CTRL;
		break;
	case 1:
		reg_disp_viu_ctrl = VPU_DISP_VIU1_CTRL;
		break;
	case 2:
		reg_disp_viu_ctrl = VPU_DISP_VIU2_CTRL;
		break;
	default:
		eDP_ERR(edp, "%s: invalid drv_index", __func__);
		return;
	}

	edp_vcbus_write(reg_disp_viu_ctrl, (0 << 31) | //bit31: lvds enable
					    (0 << 30) | //bit30: vx1 enable
					    (0 << 29) | //bit29: hdmitx enable
					    (1 << 28)); //bit28: dsi_edp enable

	edp_vcbus_write(VPU_VENC_CTRL + offset, 2);

	if (edp->sink.port_mask == 0x3)
		dptx_dual_set_t7(edp, 1, 1, edp->setting.dual_port_type);
}

static void edptx_venc_enable_ctrl(struct edptx_drv_s *edp, unsigned char flag)
{
	edp_vcbus_write(ENCL_VIDEO_EN + edp->data->offset_venc[edp->idx], flag ? 1 : 0);
}

static void dptx_venc_mute_set(struct edptx_drv_s *edp, unsigned char flag)
{
	unsigned int offset;

	offset = edp->data->offset_venc[edp->idx];

	dptx_venc_wait_vsync(edp);
	if (flag) {
		edp_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
		edp_vcbus_write(ENCL_TST_MDSEL        + offset, 0);
		edp_vcbus_write(ENCL_TST_Y            + offset, 0);
		edp_vcbus_write(ENCL_TST_CB           + offset, 0);
		edp_vcbus_write(ENCL_TST_CR           + offset, 0);
		edp_vcbus_write(ENCL_TST_EN           + offset, 1);
		edp_vcbus_setb(ENCL_VIDEO_MODE_ADV    + offset, 0, 3, 1);
	} else {
		edp_vcbus_setb(ENCL_VIDEO_MODE_ADV    + offset, 1, 3, 1);
		edp_vcbus_write(ENCL_TST_EN           + offset, 0);
	}
}

static unsigned int dptx_venc_get_encl_line_cnt(struct edptx_drv_s *edp)
{
	unsigned int cnt;

	cnt = edp_vcbus_getb(VPU_VENCP_STAT + edp->data->offset_venc[edp->idx], 16, 13);
	return cnt;
}

void edptx_venc_op_init_a9(struct edptx_venc_op_s *venc_op)
{
	if (!venc_op)
		return;

	venc_op->wait_vsync        = dptx_venc_wait_vsync;
	venc_op->get_max_lcnt      = dptx_venc_get_max_lint_cnt;
	venc_op->venc_debug_test   = dptx_venc_debug_test;
	venc_op->venc_set_timing   = edptx_venc_set_timing;
	venc_op->venc_set          = edptx_venc_set;
	venc_op->venc_switch       = edptx_venc_enable_ctrl;
	venc_op->mute_set          = dptx_venc_mute_set;
	venc_op->get_encl_line_cnt = dptx_venc_get_encl_line_cnt;
};
