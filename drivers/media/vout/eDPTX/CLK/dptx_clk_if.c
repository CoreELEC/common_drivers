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
#include "../dptx_common.h"
#include "./dptx_clk_ctrl.h"

struct dptx_clk_op_s *dptx_clk_op_s;

void dptx_clk_set_vid_clk(struct dptx_drv_s *dptx, u32 pixel_clk)
{
	u8 cnt = 0;

	if (dptx_clk_op_s->vid_clk_config)
		dptx_clk_op_s->vid_clk_config(dptx, pixel_clk);
	if (dptx_clk_op_s->link_clk_set)
		dptx_clk_op_s->vid_clk_set(dptx);

	dptx_delay_ms(1);

	while (dptx_clk_msr_check(dptx->data->venc_clk_msr_id[dptx->idx], pixel_clk)) {
		if (cnt++ >= 10) {
			DPTX_ERR(dptx, "%s timeout", __func__);
			break;
		}
	}
}

void dptx_clk_set_link_clk(struct dptx_drv_s *dptx, u8 port, u8 dptx_link_rate)
{
	if (dptx_clk_op_s->link_clk_config)
		dptx_clk_op_s->link_clk_config(dptx, port, dptx_link_rate);

	if (dptx_clk_op_s->clktree_set)
		dptx_clk_op_s->clktree_set(dptx);

	if (dptx_clk_op_s->link_clk_set)
		dptx_clk_op_s->link_clk_set(dptx, port);
}

void dptx_clk_config_print(struct dptx_drv_s *dptx)
{
	if (dptx_clk_op_s->clk_config_print)
		dptx_clk_op_s->clk_config_print(dptx);
}

void dptx_clk_set_ssc(struct dptx_drv_s *dptx, u8 status)
{
}

void dptx_clk_config_probe(struct dptx_drv_s *dptx)
{
	switch (dptx->data->chip_type) {
	case eDPTX_CHIP_T7:
		dptx_clk_op_s = dptx_clk_op_init_t7(dptx);
		break;
	case eDPTX_CHIP_A9:
		dptx_clk_op_s = dptx_clk_op_init_a9(dptx);
		break;
	default:
		DPTX_ERR(dptx, "%s: invalid chip type", __func__);
		return;
	}
}

void dptx_clk_init(void)
{
}
