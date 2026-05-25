/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_HW_H
#define __MESON_TX_HW_H

#include <linux/types.h>
#include <linux/amlogic/media/vout/meson_tx_connector/phy/meson_tx_phy_core.h>
#include <linux/amlogic/media/vout/meson_tx_connector/clk/meson_tx_clk.h>

struct reg_map {
	u32 phy_addr;
	u32 size;
	void __iomem *p;
};

struct tx_task_manager;
struct dptx_common;

struct meson_hw_event_ops {
	int (*queue_event)(struct tx_task_manager *mgr, u32 type, u32 delay);
	int (*cancel_event)(struct tx_task_manager *mgr, u32 type, bool sync);
	struct tx_task_manager *data;
};

struct meson_tx_hw {
	int type;
	int (*init_tx_hw)(struct dptx_common *tx_comm, struct meson_tx_hw *tx_hw);
	int (*uninit_tx_hw)(struct dptx_common *tx_comm, struct meson_tx_hw *tx_hw);
	int (*hw_cntl)(struct meson_tx_hw *tx_hw, u32 cmd,
		void *input_argv, void *output_struct);

	struct meson_tx_log *tx_log;
	struct meson_tx_phy *tx_phy;
	/* tx phy configuration */
	struct meson_tx_phy_cfg_opts tx_phy_conf;
	struct meson_tx_clk *tx_clk;

	/* for HW queue event */
	struct meson_hw_event_ops *event_ops;

	/* for HW iomem access */
	struct reg_map *regs_region;

	/* for manual lane remap */
	s32 lane_remap;

	bool pxp_mode;
};

#endif
