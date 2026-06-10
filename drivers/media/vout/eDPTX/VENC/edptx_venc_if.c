// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#ifdef CONFIG_AML_VPP
#include <linux/amlogic/media/vpp/vpp.h>
#endif
#include "edptx_venc.h"
#include "../edptx_reg_op.h"

static struct edptx_venc_op_s edptx_venc_op;

void dptx_wait_vsync(struct edptx_drv_s *edp)
{
	if (!edptx_venc_op.wait_vsync || !edp)
		return;

	edptx_venc_op.wait_vsync(edp);
}

unsigned int dptx_get_encl_line_cnt(struct edptx_drv_s *edp)
{
	unsigned int lcnt;

	if (!edp)
		return 0;
	if (!edptx_venc_op.get_encl_line_cnt)
		return 0;

	lcnt = edptx_venc_op.get_encl_line_cnt(edp);
	return lcnt;
}

unsigned int dptx_get_max_line_cnt(struct edptx_drv_s *edp)
{
	unsigned int lcnt;

	if (!edp)
		return 0;
	if (!edptx_venc_op.get_max_lcnt) {
		eDP_ERR(edp, "%s: invalid\n", __func__);
		return 0;
	}

	lcnt = edptx_venc_op.get_max_lcnt(edp);
	return lcnt;
}

void dptx_debug_test(struct edptx_drv_s *edp, u8 num)
{
	int ret;

	if (!edp)
		return;
	if (!edptx_venc_op.venc_debug_test) {
		eDP_ERR(edp, "%s: invalid", __func__);
		return;
	}

	ret = edptx_venc_op.venc_debug_test(edp, num);
	if (ret) {
		eDP_ERR(edp, "%s: %d not support", __func__, num);
		return;
	}

	if (num == 0)
		eDP_ERR(edp, "disable test pattern");
}

//static void dptx_gamma_init(struct edptx_drv_s *edp)
//{
//	if (!edp)
//		return;
//#ifdef CONFIG_AML_VPP
//	dptx_wait_vsync(edp);
//	vpp_disable_lcd_gamma_table(edp->idx);

//	vpp_init_lcd_gamma_table(edp->idx);

//	dptx_wait_vsync(edp);
//	vpp_enable_lcd_gamma_table(edp->idx);
//#endif
//}

void dptx_set_venc_timing(struct edptx_drv_s *edp)
{
	if (!edp)
		return;
	if (!edptx_venc_op.venc_set_timing) {
		eDP_ERR(edp, "%s: invalid", __func__);
		return;
	}

	edptx_venc_op.venc_set_timing(edp);
}

void dptx_set_venc(struct edptx_drv_s *edp)
{
	if (!edp)
		return;
	if (!edptx_venc_op.venc_set) {
		eDP_ERR(edp, "%s: invalid", __func__);
		return;
	}

	edptx_venc_op.venc_set(edp);
	//lcd_gamma_init(edp);
}

void edptx_venc_enable(struct edptx_drv_s *edp, u8 flag)
{
	if (!edp)
		return;
	if (!edptx_venc_op.venc_switch) {
		eDP_ERR(edp, "%s: invalid", __func__);
		return;
	}

	eDP_PR(edp, "%s: %d", __func__, flag);
	edptx_venc_op.venc_switch(edp, flag);
}

void edptx_mute_set(struct edptx_drv_s *edp, u8 flag)
{
	if (!edp)
		return;
	if (!edptx_venc_op.mute_set) {
		eDP_ERR(edp, "%s: invalid", __func__);
		return;
	}

	eDP_PR(edp, "%s: %d", __func__, flag);
	edptx_venc_op.mute_set(edp, flag);
}

int edptx_venc_probe(struct edptx_drv_s *edp)
{
	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		edptx_venc_op_init_t7(&edptx_venc_op);
		break;
	case eDPTX_CHIP_A9:
		edptx_venc_op_init_a9(&edptx_venc_op);
		break;
	default:
		break;
	}

	return 0;
}
