/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_TX_CLK_H
#define _MESON_TX_CLK_H

#include <linux/types.h>
#include <linux/mutex_types.h>
#include <linux/kernel.h>

#define PIXEL_PLL_MASK BIT(0)
#define PIXEL_CLK_MASK BIT(1)
#define VENC_CLK_MASK BIT(2)
#define PHY_CLK_MASK BIT(3)

/* HDMITX/DPTX use different PLL and phy_clk */
enum tx_clk_type {
	CLK_HDMITX,
	CLK_DPTX
};

struct meson_tx_clk_data {
	enum tx_clk_type clk_type;
	/* vco clk for tx connector digital, such as pixel/venc clk, unit: kHz */
	u32 pixel_vco_clk;
	/* divide from pixel_vco_clk */
	u32 clk_to_crt_video;
	/* implementation-specific clk divider path */
	u32 clk_div_path;
	/* pixel/venc clk, divide from crt_video_input_clk */
	u32 pixel_clk;
	u32 venc_clk;

	/* for phy(hdmi_frl_rate/dp_link_rate) clk, divide from phy_vco
	 * when video clk and phy clk are not coherent;
	 * or for phy(tmds) clk, divide from pixel_vco when video clk
	 * and phy clk are coherent
	 */
	u32 phy_clk;
};

enum map_addr_e {
	ANACTRL_REG,
	CLKCTRL_REG,
	REG_MAX,
};

struct meson_tx_clk {
	struct platform_device *pdev;
	void __iomem *reg_io_base[REG_MAX];
	/* clk config mutex */
	struct mutex clk_set_mutex;
	const struct meson_tx_clk_ops *tx_clk_ops;
	struct meson_tx_clk_data tx_clk_cfg;
};

struct meson_tx_clk_ops {
	int (*tx_clk_set)(struct meson_tx_clk *tx_clk, u32 clk_mask);
	int (*tx_clk_disable)(struct meson_tx_clk *tx_clk, u32 clk_mask);
};

int meson_tx_clk_set(struct meson_tx_clk *tx_clk, u32 clk_mask);
int meson_tx_clk_disable(struct meson_tx_clk *tx_clk, u32 clk_mask);

#endif
