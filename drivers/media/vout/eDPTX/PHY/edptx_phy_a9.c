// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "edptx_phy_config.h"
#include "../edptx_common.h"
#include "../edptx_reg_op.h"

static void dptx_dphy_set(struct edptx_drv_s *edp, u8 port, u8 on_off)
{
	//unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	//unsigned int bit_data_in_lvds, bit_data_in_edp, bit_lane_sel;

	//if (edp->idx == 0 && port == 0) {
	//	reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
	//	reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
	//	bit_data_in_lvds = 0;
	//	bit_data_in_edp = 1;
	//	bit_lane_sel = 0;
	//} else if (edp->idx == 1 || (edp->idx == 0 && port == 1)) { //edp->idx == 1
	//	reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
	//	reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
	//	bit_data_in_lvds = 2;
	//	bit_data_in_edp = 3;
	//	bit_lane_sel = 16;
	//} else {
	//	return;
	//}

	//if (on_off) {
	//	//sel dphy data_in
	//	dptx_combo_dphy_setb(edp, COMBO_DPHY_CNTL0, 0, bit_data_in_lvds, 1);
	//	dptx_combo_dphy_setb(edp, COMBO_DPHY_CNTL0, 1, bit_data_in_edp, 1);
	//	//sel dphy lane
	//	dptx_combo_dphy_setb(edp, COMBO_DPHY_CNTL1, 0x155, bit_lane_sel, 10);
	//	//sel edp fifo clkdiv 20, enable lane
	//	dptx_combo_dphy_write(edp, reg_dphy_tx_ctrl0, ((0x4 << 5) | (0x1f << 16)));
	//	dptx_combo_dphy_setb(edp, reg_dphy_tx_ctrl1, 1, 6, 10); //fifo enable
	//	dptx_combo_dphy_setb(edp, reg_dphy_tx_ctrl1, 1, 7, 10); //fifo wr enable

	//	dptx_combo_dphy_write(edp, reg_dphy_tx_ctrl1, 0x000000d1); //!temp
	//} else {
	//	dptx_combo_dphy_setb(edp, reg_dphy_tx_ctrl1, 0, 7, 10); //fifo wr disable
	//	dptx_combo_dphy_setb(edp, reg_dphy_tx_ctrl1, 0, 6, 10); //fifo disable
	//	dptx_combo_dphy_setb(edp, reg_dphy_tx_ctrl0, 0, 16, 8); //lane disable
	//}
}

static void dptx0_ana_phy_cntl_set(struct edptx_drv_s *edp, u8 status)
{
	//struct edptx_phy_cfg_s *phy = &edp->phy_cfg;

	edp_ana_setb(ANACTRL_DIF_PHY_CNTL6_A9, 0x0027, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL7_A9, 0x4fb0, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL8_A9, 0xc026, 0, 16);
	edp_ana_write(ANACTRL_DIF_PHY_CNTL9_A9, 0x03dde450);

	edp_ana_setb(ANACTRL_DIF_PHY_CNTL1_A9, 0x3208, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL2_A9, 0x3208, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL3_A9, 0x3208, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL4_A9, 0x3208, 0, 16);
	edp_ana_setb(ANACTRL_DIF_PHY_CNTL5_A9, 0x3208, 0, 16);
}

static void dptx_ana_phy_set(struct edptx_drv_s *edp, u8 port, u8 status)
{
	//u32 flag, data_lane0_aux, data_lane1_aux, data_lane;
	// struct edptx_phy_cfg_s *phy = &edp->phy_cfg;
	// u32 vswing;

	eDP_P_PR(edp, port, "%s: %u", __func__, status);

	dptx0_ana_phy_cntl_set(edp, status);
}

static void edptx_phy_enable_a9(struct edptx_drv_s *edp, u8 port)
{
	edp->phy_cfg.vswing = 0x3;
	dptx_ana_phy_set(edp, port, 1);
	// dptx_dphy_set(edp, port, 1);
}

static void edptx_phy_disable_a9(struct edptx_drv_s *edp, u8 port)
{
	dptx_dphy_set(edp, port, 0);
	dptx_ana_phy_set(edp, port, 0);
}

static void edptx_phy_set_lane_a9(struct edptx_drv_s *edp, u8 port, u8 lane_mask)
{
	//!!!!!!!!!!!!!!!!!!!
	dptx_ana_phy_set(edp, port, 1);
}

static struct edptx_phy_ctrl_s dptx_phy_ctrl_a9 = {
	.lane_lock     = 0,
	.phy_enable    = edptx_phy_enable_a9,
	.phy_disable   = edptx_phy_disable_a9,
	.phy_set_lane  = edptx_phy_set_lane_a9,
};

struct edptx_phy_ctrl_s *edptx_phy_config_init_a9(void)
{
	return &dptx_phy_ctrl_a9;
}
