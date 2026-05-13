// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "edptx_phy_config.h"
#include "../edptx_common.h"

static struct edptx_phy_ctrl_s *dptx_phy_ctrl;

void edptx_phy_enable(struct edptx_drv_s *edp, u8 port)
{
	if (!dptx_phy_ctrl->phy_enable) {
		eDP_P_PR(edp, port, "%s: phy_enable is null", __func__);
		return;
	}
	dptx_phy_ctrl->phy_enable(edp, port);
}

void edptx_phy_disable(struct edptx_drv_s *edp, u8 port)
{
	if (!dptx_phy_ctrl->phy_disable) {
		eDP_P_PR(edp, port, "%s: phy_disable is null", __func__);
		return;
	}

	dptx_phy_ctrl->phy_enable(edp, port);
}

void edptx_phy_set_lane(struct edptx_drv_s *edp, u8 port, u8 lane_mask)
{
	if (!dptx_phy_ctrl->phy_disable) {
		eDP_P_PR(edp, port, "%s: phy_disable is null", __func__);
		return;
	}

	eDP_P_DBG(edp, port, "%s [0x%x]", __func__, lane_mask);
	dptx_phy_ctrl->phy_set_lane(edp, port, lane_mask);
}

void edptx_phy_probe(struct edptx_drv_s *edp)
{
	dptx_phy_ctrl = NULL;

	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		dptx_phy_ctrl = edptx_phy_config_init_t7();
		break;
	case eDPTX_CHIP_A9:
		dptx_phy_ctrl = edptx_phy_config_init_a9();
		break;
	default:
		break;
	}
}
