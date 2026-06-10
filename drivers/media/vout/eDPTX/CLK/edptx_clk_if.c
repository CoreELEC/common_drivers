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
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "../edptx_common.h"
#include "./edptx_clk_ctrl.h"

struct edptx_clk_op_s *edptx_clk_op_s;

void dptx_clk_set_vid_clk(struct edptx_drv_s *edp, u32 pixel_clk)
{
	u8 cnt = 0;

	if (edptx_clk_op_s->vid_clk_config)
		edptx_clk_op_s->vid_clk_config(edp, pixel_clk);
	if (edptx_clk_op_s->link_clk_set)
		edptx_clk_op_s->vid_clk_set(edp);

	edptx_delay_ms(1);

	while (edptx_clk_msr_check(edp->data->venc_clk_msr_id[edp->idx], pixel_clk)) {
		if (cnt++ >= 10) {
			eDP_ERR(edp, "%s timeout", __func__);
			break;
		}
	}
}

void edptx_clk_set_link_clk(struct edptx_drv_s *edp, u8 port, u8 dptx_link_rate)
{
	if (edptx_clk_op_s->link_clk_config)
		edptx_clk_op_s->link_clk_config(edp, port, dptx_link_rate);

	if (edptx_clk_op_s->clktree_set)
		edptx_clk_op_s->clktree_set(edp);

	if (edptx_clk_op_s->link_clk_set)
		edptx_clk_op_s->link_clk_set(edp, port);
}

void dptx_clk_config_print(struct edptx_drv_s *edp)
{
	if (edptx_clk_op_s->clk_config_print)
		edptx_clk_op_s->clk_config_print(edp);
}

void dptx_clk_set_ssc(struct edptx_drv_s *edp, u8 status)
{
}

void edptx_clk_config_probe(struct edptx_drv_s *edp)
{
	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		edptx_clk_op_s = edptx_clk_op_init_t7(edp);
		break;
	case eDPTX_CHIP_A9:
		edptx_clk_op_s = edptx_clk_op_init_a9(edp);
		break;
	default:
		eDP_ERR(edp, "%s: invalid chip type", __func__);
		return;
	}
}

void dptx_clk_init(void)
{
}
