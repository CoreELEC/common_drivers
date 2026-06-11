// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#ifdef CONFIG_AMLOGIC_CLK_DEBUG
#include <linux/clkdev.h>
#endif
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "meson-clkc-utils.h"
#include "clk-pll.h"
#include "clk-dualdiv.h"
#include "clk-regmap.h"
#include "a9.h"
#include <dt-bindings/clock/amlogic,a9-clkc.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(192, 0, 1), /* DCO = 2304M OD = 2 PLL = 1152M */
	PLL_PARAMS_OD(256, 1, 1), /* DCO = 3072M OD = 1 PLL = 1536M */
	{ /* sentinel */ }
};

static const struct reg_sequence gp0_pll_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL0, .def = 0x08010000 },
	{ .reg = ANACTRL_GP0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_GP0PLL_CTRL3, .def = 0x00008010 }
};

static struct clk_regmap gp0_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.frac = {
			.reg_off = ANACTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 20,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 30,
			.width   = 1,
		},
		.table = gp0_pll_table,
		.init_regs = gp0_pll_init_regs,
		.init_count = ARRAY_SIZE(gp0_pll_init_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "gp0_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi_pll_table[] = {
	PLL_PARAMS_OD(150, 0, 0), /* DCO = 1800M OD = 0 PLL = 1800M */
	PLL_PARAMS_OD(150, 0, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 0, 2), /* DCO = 1956M OD = 4 PLL = 489M */
	{ /* sentinel */ }
};

static const struct reg_sequence hifi_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFI0PLL_CTRL0, .def = 0x08010000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL3, .def = 0x00008010 }
};

static struct clk_regmap hifi_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.frac = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 20,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL0,
			.shift   = 30,
			.width   = 1,
		},
		.table = hifi_pll_table,
		.init_regs = hifi_pll_init_regs,
		.init_count = ARRAY_SIZE(hifi_pll_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST |
			 CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "hifi_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi1_pll_table[] = {
	PLL_PARAMS_OD(150, 0, 0), /* DCO = 1800M OD = 0 PLL = 1800M */
	PLL_PARAMS_OD(150, 0, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 0, 2), /* DCO = 1956M OD = 4 PLL = 489M */
	{ /* sentinel */ }
};

static const struct reg_sequence hifi1_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFI1PLL_CTRL0, .def = 0x08010000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL3, .def = 0x00008010 }
};

static struct clk_regmap hifi1_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.frac = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 20,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 30,
			.width   = 1,
		},
		.table = hifi1_pll_table,
		.init_regs = hifi1_pll_init_regs,
		.init_count = ARRAY_SIZE(hifi1_pll_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST |
			 CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct pll_mult_range mclk_pll_range = {
	.min = 117,
	.max = 233
};

static const struct reg_sequence mclk_pll_init_regs[] = {
	{ .reg = ANACTRL_CSIPLL_CTRL1, .def = 0x00422000 },
	{ .reg = ANACTRL_CSIPLL_CTRL2, .def = 0x60000100 },
	{ .reg = ANACTRL_CSIPLL_CTRL3, .def = 0x02000200 },
	{ .reg = ANACTRL_CSIPLL_CTRL4, .def = 0xd616d616 }
};

static struct clk_regmap mclk_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_CSIPLL_CTRL0,
			.shift   = 30,
			.width   = 1,
		},
		.range = &mclk_pll_range,
		.init_regs = mclk_pll_init_regs,
		.init_count = ARRAY_SIZE(mclk_pll_init_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_FIXED_EN0P5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "mclk_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_mult_range mclk1_pll_range = {
	.min = 117,
	.max = 233
};

static const struct reg_sequence mclk1_pll_init_regs[] = {
	{ .reg = ANACTRL_CSIPLL_CTRL6, .def = 0x00422000 },
	{ .reg = ANACTRL_CSIPLL_CTRL7, .def = 0x60000100 },
	{ .reg = ANACTRL_CSIPLL_CTRL8, .def = 0x02000200 },
	{ .reg = ANACTRL_CSIPLL_CTRL9, .def = 0xd616d616 }
};

static struct clk_regmap mclk1_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 12,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_CSIPLL_CTRL5,
			.shift   = 30,
			.width   = 1,
		},
		.range = &mclk1_pll_range,
		.init_regs = mclk1_pll_init_regs,
		.init_count = ARRAY_SIZE(mclk1_pll_init_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_FIXED_EN0P5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "mclk_osc",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk_0_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 0,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_pll.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk_0_pre = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 3,
		.width = 5,
		.flags = CLK_DIVIDER_MAX_AT_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_pre",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_0_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mclk_0_parent_data[] = {
	{ .hw = &mclk_0_pre.hw },
	{ .fw_name = "mclk_osc" },
	{ .fw_name = "fclk_50m" }
};

static struct clk_regmap mclk_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.mask = 0x7,
		.shift = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk_0_parent_data,
		.num_parents = ARRAY_SIZE(mclk_0_parent_data),
	},
};

static struct clk_regmap mclk_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 10,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk_1_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_pll.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk_1_pre = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 19,
		.width = 5,
		.flags = CLK_DIVIDER_MAX_AT_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_pre",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_1_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mclk_1_parent_data[] = {
	{ .hw = &mclk_1_pre.hw },
	{ .fw_name = "mclk_osc" },
	{ .fw_name = "fclk_50m" }
};

static struct clk_regmap mclk_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.mask = 0x7,
		.shift = 28,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk_1_parent_data,
		.num_parents = ARRAY_SIZE(mclk_1_parent_data),
	},
};

static struct clk_regmap mclk_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 26,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk1_0_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 0,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_0_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_pll.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk1_0_pre = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 3,
		.width = 5,
		.flags = CLK_DIVIDER_MAX_AT_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_0_pre",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_0_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mclk1_0_parent_data[] = {
	{ .hw = &mclk1_0_pre.hw },
	{ .fw_name = "mclk_osc" },
	{ .fw_name = "fclk_50m" }
};

static struct clk_regmap mclk1_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.mask = 0x7,
		.shift = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk1_0_parent_data,
		.num_parents = ARRAY_SIZE(mclk1_0_parent_data),
	},
};

static struct clk_regmap mclk1_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 10,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk1_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk1_1_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_1_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_pll.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk1_1_pre = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 19,
		.width = 5,
		.flags = CLK_DIVIDER_MAX_AT_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_1_pre",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_1_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mclk1_1_parent_data[] = {
	{ .hw = &mclk1_1_pre.hw },
	{ .fw_name = "mclk_osc" },
	{ .fw_name = "fclk_50m" }
};

static struct clk_regmap mclk1_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.mask = 0x7,
		.shift = 28,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk1_1_parent_data,
		.num_parents = ARRAY_SIZE(mclk1_1_parent_data),
	},
};

static struct clk_regmap mclk1_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.shift = 26,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk1_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL8,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sar_adc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" }
};

static struct clk_regmap sar_adc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sar_adc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sar_adc_parent_data,
		.num_parents = ARRAY_SIZE(sar_adc_parent_data),
	},
};

static struct clk_regmap sar_adc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sar_adc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sar_adc_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sar_adc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sar_adc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sar_adc_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc_pre_parent_data[] = {
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap sc_pre_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_pre_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc_pre_parent_data,
		.num_parents = ARRAY_SIZE(sc_pre_parent_data),
	},
};

static struct clk_regmap sc_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc_pre_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc_pre = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc_pre.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 video_src_01_parent_table[] = { 4, 5, 6 };
static const struct clk_parent_data video_src_01_parent_data[] = {
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap video_src0_in_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = video_src_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src0_in_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = video_src_01_parent_data,
		.num_parents = ARRAY_SIZE(video_src_01_parent_data),
	},
};

static struct clk_regmap video_src0_in = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src0_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0_in_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video_src0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0_in.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap video_src0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video_src1_in_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = video_src_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src1_in_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = video_src_01_parent_data,
		.num_parents = ARRAY_SIZE(video_src_01_parent_data),
	},
};

static struct clk_regmap video_src1_in = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src1_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1_in_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video_src1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1_in.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap video_src1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video_src1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video0_div1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video0_div2_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video0_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video0_div2_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div4_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video0_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video0_div4_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div6_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video0_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video0_div6_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div12_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src0.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video0_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video0_div12_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video1_div1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video1_div2_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video1_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video1_div2_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video1_div4_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video1_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video1_div4_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video1_div6_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video1_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video1_div6_en.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video1_div12_en = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video1_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data) {
		.name = "video1_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video1_div12_en.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data lcd_an_parent_data[] = {
	{ .hw = &video0_div6.hw },
	{ .hw = &video0_div12.hw }
};

static struct clk_regmap lcd_an_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.mask = 0x1,
		.shift = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "lcd_an_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = lcd_an_parent_data,
		.num_parents = ARRAY_SIZE(lcd_an_parent_data),
	},
};

static struct clk_regmap lcd_an = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 14,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "lcd_an",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&lcd_an_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap lcd_an_ph2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "lcd_an_ph2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&lcd_an.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap lcd_an_ph3 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "lcd_an_ph3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&lcd_an.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 hdmitx_parent_table[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_parent_data hdmitx_parent_data[] = {
	{ .hw = &video0_div1.hw },
	{ .hw = &video0_div2.hw },
	{ .hw = &video0_div4.hw },
	{ .hw = &video0_div6.hw },
	{ .hw = &video0_div12.hw },
	{ .hw = &video1_div1.hw },
	{ .hw = &video1_div2.hw },
	{ .hw = &video1_div4.hw },
	{ .hw = &video1_div6.hw },
	{ .hw = &video1_div12.hw }
};

static struct clk_regmap enc0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 12,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap enc0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&enc0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap enc1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 8,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap enc1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&enc1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap vdac_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdac_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap vdac = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdac_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap hdmitx_pixel_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_pixel_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap hdmitx_pixel = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_pixel",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_pixel_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap hdmitx_fe_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 20,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_fe_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap hdmitx_fe = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_fe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_fe_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap hdmitx1_pixel_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 24,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx1_pixel_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap hdmitx1_pixel = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx1_pixel",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx1_pixel_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap hdmitx1_fe_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 28,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx1_fe_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap hdmitx1_fe = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx1_fe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx1_fe_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data mali_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" }
};

static struct clk_regmap mali_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_01_parent_data),
	},
};

static struct clk_regmap mali_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mali_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap mali_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_01_parent_data),
	},
};

static struct clk_regmap mali_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mali_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data mali_parent_data[] = {
	{ .hw = &mali_0.hw },
	{ .hw = &mali_1.hw }
};

static struct clk_regmap mali = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_parent_data,
		.num_parents = ARRAY_SIZE(mali_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap mali_stack_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_01_parent_data),
	},
};

static struct clk_regmap mali_stack_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_stack_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mali_stack_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_stack_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap mali_stack_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_01_parent_data),
	},
};

static struct clk_regmap mali_stack_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_stack_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mali_stack_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mali_stack_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data mali_stack_parent_data[] = {
	{ .hw = &mali_stack_0.hw },
	{ .hw = &mali_stack_1.hw }
};

static struct clk_regmap mali_stack = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MALI_STACK_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mali_stack",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mali_stack_parent_data,
		.num_parents = ARRAY_SIZE(mali_stack_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data hevcf_01_parent_data[] = {
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap hevcf_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hevcf_01_parent_data,
		.num_parents = ARRAY_SIZE(hevcf_01_parent_data),
	},
};

static struct clk_regmap hevcf_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hevcf_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hevcf_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hevcf_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap hevcf_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hevcf_01_parent_data,
		.num_parents = ARRAY_SIZE(hevcf_01_parent_data),
	},
};

static struct clk_regmap hevcf_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hevcf_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hevcf_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hevcf_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data hevcf_parent_data[] = {
	{ .hw = &hevcf_0.hw },
	{ .hw = &hevcf_1.hw }
};

static struct clk_regmap hevcf = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HEVCF_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcf",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hevcf_parent_data,
		.num_parents = ARRAY_SIZE(hevcf_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data hcodec_01_parent_data[] = {
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap hcodec_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hcodec_01_parent_data,
		.num_parents = ARRAY_SIZE(hcodec_01_parent_data),
	},
};

static struct clk_regmap hcodec_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hcodec_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hcodec_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hcodec_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap hcodec_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hcodec_01_parent_data,
		.num_parents = ARRAY_SIZE(hcodec_01_parent_data),
	},
};

static struct clk_regmap hcodec_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hcodec_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hcodec_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hcodec_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data hcodec_parent_data[] = {
	{ .hw = &hcodec_0.hw },
	{ .hw = &hcodec_1.hw }
};

static struct clk_regmap hcodec = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HCODEC_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hcodec_parent_data,
		.num_parents = ARRAY_SIZE(hcodec_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static u32 vpu_01_parent_table[] = { 0, 1, 2, 4, 7 };
static const struct clk_parent_data vpu_01_parent_data[] = {
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "gp1_pll" }
};

static struct clk_regmap vpu_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = vpu_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_01_parent_data,
		.num_parents = ARRAY_SIZE(vpu_01_parent_data),
	},
};

static struct clk_regmap vpu_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap vpu_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = vpu_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_01_parent_data,
		.num_parents = ARRAY_SIZE(vpu_01_parent_data),
	},
};

static struct clk_regmap vpu_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data vpu_parent_data[] = {
	{ .hw = &vpu_0.hw },
	{ .hw = &vpu_1.hw }
};

static struct clk_regmap vpu = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_parent_data,
		.num_parents = ARRAY_SIZE(vpu_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data vpu_clkb_tmp_parent_data[] = {
	{ .hw = &vpu.hw },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" }
};

static struct clk_regmap vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_clkb_tmp_parent_data,
		.num_parents = ARRAY_SIZE(vpu_clkb_tmp_parent_data),
	},
};

static struct clk_regmap vpu_clkb_tmp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 16,
		.width = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkb_tmp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_clkb_tmp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkb_tmp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_clkb_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkb_tmp.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_clkb = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkb_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 vapb_01_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data vapb_01_parent_data[] = {
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .fw_name = "fclk_div2" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div2p5" }
};

static struct clk_regmap vapb_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = vapb_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vapb_01_parent_data,
		.num_parents = ARRAY_SIZE(vapb_01_parent_data),
	},
};

static struct clk_regmap vapb_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vapb_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vapb_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vapb_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap vapb_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = vapb_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vapb_01_parent_data,
		.num_parents = ARRAY_SIZE(vapb_01_parent_data),
	},
};

static struct clk_regmap vapb_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vapb_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vapb_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vapb_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data vapb_parent_data[] = {
	{ .hw = &vapb_0.hw },
	{ .hw = &vapb_1.hw }
};

static struct clk_regmap vapb = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vapb_parent_data,
		.num_parents = ARRAY_SIZE(vapb_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap ge2d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vapb.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data hdmitx_sys_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap hdmitx_sys_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_sys_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_sys_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_sys_parent_data),
	},
};

static struct clk_regmap hdmitx_sys_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_sys_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_sys_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmitx_sys = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_sys",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_sys_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data hdmitx_prif_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap hdmitx_prif_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_prif_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_prif_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_prif_parent_data),
	},
};

static struct clk_regmap hdmitx_prif_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_prif_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_prif_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmitx_prif = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_prif",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_prif_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data hdmitx_200m_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap hdmitx_200m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_200m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_200m_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_200m_parent_data),
	},
};

static struct clk_regmap hdmitx_200m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_200m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_200m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmitx_200m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_200m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_200m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data hdmitx_aud_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap hdmitx_aud_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_aud_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_aud_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_aud_parent_data),
	},
};

static struct clk_regmap hdmitx_aud_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_aud_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_aud_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmitx_aud = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HTX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmitx_aud",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmitx_aud_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data pwm_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" }
};

static struct clk_regmap pwm_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_H_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_h_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_H_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_h_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_h = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_H_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_h",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_h_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_i_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_I_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_i_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_I_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_i_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_i = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_I_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_i",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_i_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_J_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_j_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_J_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_j_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_j = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_J_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_j",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_j_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_k_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_K_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_k_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_k_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_K_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_k_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_k_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_k = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_K_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_k_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_l_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_L_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_l_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_l_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_L_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_l_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_l_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_l = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_L_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_l",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_l_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_M_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_M_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_M_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_n_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_N_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_n_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_n_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_N_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_n_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_n_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_n = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_N_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_n",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_n_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data spisg_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap spisg0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = spisg_parent_data,
		.num_parents = ARRAY_SIZE(spisg_parent_data),
	},
};

static struct clk_regmap spisg0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spisg0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spisg1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = spisg_parent_data,
		.num_parents = ARRAY_SIZE(spisg_parent_data),
	},
};

static struct clk_regmap spisg1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spisg1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spisg2_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg2_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = spisg_parent_data,
		.num_parents = ARRAY_SIZE(spisg_parent_data),
	},
};

static struct clk_regmap spisg2_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg2_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spisg2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spisg2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spisg2_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 sd_emmc_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sd_emmc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div3" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "gp1_pll" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.mask = 0x7,
		.shift = 9,
		.table = sd_emmc_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_data),
	},
};

static struct clk_regmap sd_emmc_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_a_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sd_emmc_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_a_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sd_emmc_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.mask = 0x7,
		.shift = 25,
		.table = sd_emmc_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_data),
	},
};

static struct clk_regmap sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_b_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sd_emmc_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_b_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sd_emmc_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL1,
		.mask = 0x7,
		.shift = 9,
		.table = sd_emmc_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_parent_data),
	},
};

static struct clk_regmap sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_c_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sd_emmc_c_div.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data vdin_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap vdin_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vdin_meas_parent_data,
		.num_parents = ARRAY_SIZE(vdin_meas_parent_data),
	},
};

static struct clk_regmap vdin_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdin_meas_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vdin_meas = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdin_meas_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vid_lock_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &enc0.hw },
	{ .hw = &enc1.hw }
};

static struct clk_regmap vid_lock_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vid_lock_parent_data,
		.num_parents = ARRAY_SIZE(vid_lock_parent_data),
	},
};

static struct clk_regmap vid_lock_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vid_lock_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vid_lock = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vid_lock_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data eth_clk_parent_data[] = {
	{ .fw_name = "fclk_div2" }
};

static struct clk_regmap eth_rmii_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_rmii_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = eth_clk_parent_data,
		.num_parents = ARRAY_SIZE(eth_clk_parent_data),
	},
};

static struct clk_regmap eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_rmii_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_rmii_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor eth_125m_div = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data) {
		.name = "eth_125m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fclk_div2"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_125m_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap ts_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_TS_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ts_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap ts = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ts",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ts_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data amfc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" }
};

static struct clk_regmap amfc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = amfc_parent_data,
		.num_parents = ARRAY_SIZE(amfc_parent_data),
	},
};

static struct clk_regmap amfc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&amfc_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap amfc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&amfc_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data pcie_tl_parent_data[] = {
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div2p5" },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap pcie0_tl_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie0_tl_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pcie_tl_parent_data,
		.num_parents = ARRAY_SIZE(pcie_tl_parent_data),
	},
};

static struct clk_regmap pcie0_tl_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie0_tl_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie0_tl_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pcie0_tl = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie0_tl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie0_tl_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pcie1_tl_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie1_tl_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pcie_tl_parent_data,
		.num_parents = ARRAY_SIZE(pcie_tl_parent_data),
	},
};

static struct clk_regmap pcie1_tl_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie1_tl_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie1_tl_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pcie1_tl = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie1_tl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie1_tl_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data pp_parent_data[] = {
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div2p5" },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap pp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PP_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pp_parent_data,
		.num_parents = ARRAY_SIZE(pp_parent_data),
	},
};

static struct clk_regmap pp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PP_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PP_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data isp_parent_data[] = {
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap isp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = isp_parent_data,
		.num_parents = ARRAY_SIZE(isp_parent_data),
	},
};

static struct clk_regmap isp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&isp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap isp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&isp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dspa_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "gp2_pll" },
	{ .fw_name = "fclk_div4" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "rtc_clk" }
};

static struct clk_regmap dspa_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dspa_01_parent_data,
		.num_parents = ARRAY_SIZE(dspa_01_parent_data),
	},
};

static struct clk_regmap dspa_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dspa_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dspa_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dspa_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap dspa_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dspa_01_parent_data,
		.num_parents = ARRAY_SIZE(dspa_01_parent_data),
	},
};

static struct clk_regmap dspa_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dspa_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dspa_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dspa_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data dspa_parent_data[] = {
	{ .hw = &dspa_0.hw },
	{ .hw = &dspa_1.hw }
};

static struct clk_regmap dspa = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dspa",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dspa_parent_data,
		.num_parents = ARRAY_SIZE(dspa_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static u32 glb_parent_table[] = { 0, 1, 2, 3, 4, 5, 7 };
static const struct clk_parent_data glb_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &dspa.hw },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .hw = &isp.hw },
	{ .fw_name = "rtc_clk" }
};

static struct clk_regmap glb_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_GLBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = glb_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "glb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = glb_parent_data,
		.num_parents = ARRAY_SIZE(glb_parent_data),
	},
};

static struct clk_regmap glb_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_GLBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "glb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&glb_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap glb = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_GLBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "glb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&glb_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 can_pe_parent_table[] = { 0, 1, 3 };
static const struct clk_parent_data can_pe_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap can0_pe_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
		.table = can_pe_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_pe_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = can_pe_parent_data,
		.num_parents = ARRAY_SIZE(can_pe_parent_data),
	},
};

static struct clk_regmap can0_pe_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_pe_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can0_pe_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap can0_pe = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_pe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can0_pe_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap can1_pe_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
		.table = can_pe_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_pe_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = can_pe_parent_data,
		.num_parents = ARRAY_SIZE(can_pe_parent_data),
	},
};

static struct clk_regmap can1_pe_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_pe_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can1_pe_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap can1_pe = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_pe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can1_pe_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data can_filer_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap can0_filter_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_filter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = can_filer_parent_data,
		.num_parents = ARRAY_SIZE(can_filer_parent_data),
	},
};

static struct clk_regmap can0_filter_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_filter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can0_filter_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap can0_filter = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can0_filter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can0_filter_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data can_filter_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap can1_filter_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_filter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = can_filter_parent_data,
		.num_parents = ARRAY_SIZE(can_filter_parent_data),
	},
};

static struct clk_regmap can1_filter_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_filter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can1_filter_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap can1_filter = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CAN_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "can1_filter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&can1_filter_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data i3c_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap i3c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_I3C_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "i3c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = i3c_parent_data,
		.num_parents = ARRAY_SIZE(i3c_parent_data),
	},
};

static struct clk_regmap i3c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_I3C_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "i3c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&i3c_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap i3c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_I3C_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "i3c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&i3c_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data cmpr_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" }
};

static struct clk_regmap cmpr_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cmpr_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cmpr_parent_data,
		.num_parents = ARRAY_SIZE(cmpr_parent_data),
	},
};

static struct clk_regmap cmpr_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cmpr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cmpr_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap cmpr = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CMPR_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cmpr",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cmpr_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vge_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "rtc_clk" }
};

static struct clk_regmap vge_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vge_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vge_parent_data,
		.num_parents = ARRAY_SIZE(vge_parent_data),
	},
};

static struct clk_regmap vge_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vge_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vge_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vge = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vge",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vge_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data cve_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "rtc_clk" }
};

static struct clk_regmap cve_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cve_parent_data,
		.num_parents = ARRAY_SIZE(cve_parent_data),
	},
};

static struct clk_regmap cve_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cve_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap cve = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CVE_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cve_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dewarpa_parent_data[] = {
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" }
};

static struct clk_regmap dewarpa_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpa_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dewarpa_parent_data,
		.num_parents = ARRAY_SIZE(dewarpa_parent_data),
	},
};

static struct clk_regmap dewarpa_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpa_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dewarpa_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dewarpa = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarpa",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dewarpa_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dptx_apb2_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap dptx_apb2_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_apb2_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dptx_apb2_parent_data,
		.num_parents = ARRAY_SIZE(dptx_apb2_parent_data),
	},
};

static struct clk_regmap dptx_apb2_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_apb2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dptx_apb2_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dptx_apb2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_apb2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dptx_apb2_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dptx_aud_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" }
};

static struct clk_regmap dptx_aud_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_aud_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dptx_aud_parent_data,
		.num_parents = ARRAY_SIZE(dptx_aud_parent_data),
	},
};

static struct clk_regmap dptx_aud_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_aud_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dptx_aud_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dptx_aud = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DPTX_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dptx_aud",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dptx_aud_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 dsi_meas_parent_table[] = { 0, 1, 2, 3, 5 };
static const struct clk_parent_data dsi_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = dsi_meas_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dsi_meas_parent_data,
		.num_parents = ARRAY_SIZE(dsi_meas_parent_data),
	},
};

static struct clk_regmap dsi_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dsi_meas_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dsi_meas = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dsi_meas_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 dsi_b_meas_parent_table[] = { 0, 1, 2, 3, 5 };
static const struct clk_parent_data dsi_b_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap dsi_b_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = dsi_b_meas_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_b_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dsi_b_meas_parent_data,
		.num_parents = ARRAY_SIZE(dsi_b_meas_parent_data),
	},
};

static struct clk_regmap dsi_b_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_b_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dsi_b_meas_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dsi_b_meas = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_b_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dsi_b_meas_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data hdmirx_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" }
};

static struct clk_regmap hdmirx_2m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_2m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_2m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_2m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_2m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_5m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_5m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_5m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_5m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_5m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_cfg_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_cfg_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_cfg = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_cfg_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_hdcp2x_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp2x_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_hdcp2x_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp2x_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_hdcp2x_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_hdcp2x = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp2x",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_hdcp2x_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_acr_ref_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_ref_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_acr_ref_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_ref_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_acr_ref_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_acr_ref = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr_ref",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_acr_ref_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_meter_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmirx_parent_data,
		.num_parents = ARRAY_SIZE(hdmirx_parent_data),
	},
};

static struct clk_regmap hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_meter_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmirx_meter = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmirx_meter_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 usb_250m_parent_table[] = { 0, 1, 2, 3, 4, 7 };
static const struct clk_parent_data usb_250m_parent_data[] = {
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div7" },
	{ .fw_name = "fclk_div2p5" }
};

static struct clk_regmap usb_250m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = usb_250m_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = usb_250m_parent_data,
		.num_parents = ARRAY_SIZE(usb_250m_parent_data),
	},
};

static struct clk_regmap usb_250m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_250m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap usb_250m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_250m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_250m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data nna_core_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "gp2_pll" },
	{ .hw = &hifi_pll.hw }
};

static struct clk_regmap nna_core_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = nna_core_parent_data,
		.num_parents = ARRAY_SIZE(nna_core_parent_data),
	},
};

static struct clk_regmap nna_core_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna_core_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap nna_core = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna_core_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data csi_phy_parent_data[] = {
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap csi_phy_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi_phy_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = csi_phy_parent_data,
		.num_parents = ARRAY_SIZE(csi_phy_parent_data),
	},
};

static struct clk_regmap csi_phy_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi_phy_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csi_phy_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap csi_phy = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi_phy",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csi_phy_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data usb_48m_pre_parent_data[] = {
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "gp2_pll" },
	{ .fw_name = "fclk_div2" }
};

static struct clk_regmap usb_48m_pre_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_pre_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = usb_48m_pre_parent_data,
		.num_parents = ARRAY_SIZE(usb_48m_pre_parent_data),
	},
};

static struct clk_regmap usb_48m_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_pre_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_pre_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap usb_48m_pre = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_pre_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap usb_48m_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_pre.hw,
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param usb_48m_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap usb_48m_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 28,
			.width   = 2,
		},
		.table = usb_48m_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data usb_48m_dual_parent_data[] = {
	{ .hw = &usb_48m_dual_div.hw },
	{ .hw = &usb_48m_dual_clkin.hw }
};

static struct clk_regmap usb_48m_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = usb_48m_dual_parent_data,
		.num_parents = ARRAY_SIZE(usb_48m_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap usb_48m_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data usb_48m_parent_data[] = {
	{ .hw = &usb_48m_pre.hw },
	{ .hw = &usb_48m_dual.hw }
};

static struct clk_regmap usb_48m = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x3,
		.shift = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m",
		.ops = &clk_regmap_mux_ops,
		.parent_data = usb_48m_parent_data,
		.num_parents = ARRAY_SIZE(usb_48m_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 gen_parent_table[] = { 0, 1, 2, 5, 6, 7, 8, 9, 15, 17, 18, 19,
				 20, 21, 22, 23, 24 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "rtc_clk" },
	{ .fw_name = "sys_pll_div16" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi1_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "gp2_pll" },
	{ .fw_name = "dsu_clk_div16" },
	{ .fw_name = "cpu_clk_div16" },
	{ .fw_name = "a78_clk_div16" },
	{ .fw_name = "fclk_div2" },
	{ .fw_name = "fclk_div2p5" },
	{ .fw_name = "fclk_div3" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div7" }
};

static struct clk_regmap gen_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = gen_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = gen_parent_data,
		.num_parents = ARRAY_SIZE(gen_parent_data),
	},
};

static struct clk_regmap gen_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&gen_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap gen = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&gen_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_CLK_GATE_SYS_CLK(_name, _reg, _bit)                     \
static struct clk_regmap _name = {                                           \
	.data = &(struct clk_regmap_gate_data) {                      \
		.offset = (_reg),                                     \
		.bit_idx = (_bit),                                    \
	},                                                            \
	.hw.init = &(struct clk_init_data) {                          \
		.name = #_name,                                       \
		.ops = &clk_regmap_gate_ops,                          \
		.parent_data = &(const struct clk_parent_data) {      \
			.fw_name = "sys_clk"                          \
		},                                                    \
		.num_parents = 1,                                     \
		.flags = CLK_IGNORE_UNUSED,                           \
	},                                                            \
}

MESON_CLK_GATE_SYS_CLK(sys_am_axi, CLKCTRL_SYS_CLK_EN0_REG0, 0);
MESON_CLK_GATE_SYS_CLK(sys_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi, CLKCTRL_SYS_CLK_EN0_REG0, 3);
MESON_CLK_GATE_SYS_CLK(sys_eth_phy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_amfc, CLKCTRL_SYS_CLK_EN0_REG0, 5);
MESON_CLK_GATE_SYS_CLK(sys_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
MESON_CLK_GATE_SYS_CLK(sys_nna, CLKCTRL_SYS_CLK_EN0_REG0, 7);
MESON_CLK_GATE_SYS_CLK(sys_eth_axi, CLKCTRL_SYS_CLK_EN0_REG0, 8);
MESON_CLK_GATE_SYS_CLK(sys_dp_apb, CLKCTRL_SYS_CLK_EN0_REG0, 9);
MESON_CLK_GATE_SYS_CLK(sys_edptx_apb, CLKCTRL_SYS_CLK_EN0_REG0, 10);
MESON_CLK_GATE_SYS_CLK(sys_u3hsg, CLKCTRL_SYS_CLK_EN0_REG0, 11);
MESON_CLK_GATE_SYS_CLK(sys_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
MESON_CLK_GATE_SYS_CLK(sys_glb, CLKCTRL_SYS_CLK_EN0_REG0, 15);
MESON_CLK_GATE_SYS_CLK(sys_combo_dphy_apb, CLKCTRL_SYS_CLK_EN0_REG0, 17);
MESON_CLK_GATE_SYS_CLK(sys_hdmirx_apb, CLKCTRL_SYS_CLK_EN0_REG0, 18);
MESON_CLK_GATE_SYS_CLK(sys_hdmirx_pclk, CLKCTRL_SYS_CLK_EN0_REG0, 19);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi_phy, CLKCTRL_SYS_CLK_EN0_REG0, 20);
MESON_CLK_GATE_SYS_CLK(sys_can1, CLKCTRL_SYS_CLK_EN0_REG0, 21);
MESON_CLK_GATE_SYS_CLK(sys_can0, CLKCTRL_SYS_CLK_EN0_REG0, 22);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_a, CLKCTRL_SYS_CLK_EN0_REG0, 24);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_b, CLKCTRL_SYS_CLK_EN0_REG0, 25);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_c, CLKCTRL_SYS_CLK_EN0_REG0, 26);
MESON_CLK_GATE_SYS_CLK(sys_sc, CLKCTRL_SYS_CLK_EN0_REG0, 27);
MESON_CLK_GATE_SYS_CLK(sys_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
MESON_CLK_GATE_SYS_CLK(sys_mipi_isp, CLKCTRL_SYS_CLK_EN0_REG0, 29);
MESON_CLK_GATE_SYS_CLK(sys_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
MESON_CLK_GATE_SYS_CLK(sys_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi_b, CLKCTRL_SYS_CLK_EN0_REG1, 1);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi1_phy, CLKCTRL_SYS_CLK_EN0_REG1, 2);
MESON_CLK_GATE_SYS_CLK(sys_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
MESON_CLK_GATE_SYS_CLK(sys_eth_1g_mac, CLKCTRL_SYS_CLK_EN0_REG1, 4);
MESON_CLK_GATE_SYS_CLK(sys_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
MESON_CLK_GATE_SYS_CLK(sys_uart_f, CLKCTRL_SYS_CLK_EN0_REG1, 10);
MESON_CLK_GATE_SYS_CLK(sys_ts_a55, CLKCTRL_SYS_CLK_EN0_REG1, 11);
MESON_CLK_GATE_SYS_CLK(sys_eth_1g_axi, CLKCTRL_SYS_CLK_EN0_REG1, 12);
MESON_CLK_GATE_SYS_CLK(sys_ts_dos, CLKCTRL_SYS_CLK_EN0_REG1, 13);
MESON_CLK_GATE_SYS_CLK(sys_usb3drd_b, CLKCTRL_SYS_CLK_EN0_REG1, 14);
MESON_CLK_GATE_SYS_CLK(sys_ts_core, CLKCTRL_SYS_CLK_EN0_REG1, 15);
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_SYS_CLK(sys_csi_dig_clkin, CLKCTRL_SYS_CLK_EN0_REG1, 18);
MESON_CLK_GATE_SYS_CLK(sys_cve, CLKCTRL_SYS_CLK_EN0_REG1, 19);
MESON_CLK_GATE_SYS_CLK(sys_ge2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_spisg0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
MESON_CLK_GATE_SYS_CLK(sys_u3drd_1, CLKCTRL_SYS_CLK_EN0_REG1, 22);
MESON_CLK_GATE_SYS_CLK(sys_u2h, CLKCTRL_SYS_CLK_EN0_REG1, 23);
MESON_CLK_GATE_SYS_CLK(sys_pciemac_a, CLKCTRL_SYS_CLK_EN0_REG1, 24);
MESON_CLK_GATE_SYS_CLK(sys_usb3drd_a, CLKCTRL_SYS_CLK_EN0_REG1, 25);
MESON_CLK_GATE_SYS_CLK(sys_usb2drd, CLKCTRL_SYS_CLK_EN0_REG1, 26);
MESON_CLK_GATE_SYS_CLK(sys_pciephy, CLKCTRL_SYS_CLK_EN0_REG1, 27);
MESON_CLK_GATE_SYS_CLK(sys_pciemac_b, CLKCTRL_SYS_CLK_EN0_REG1, 28);
MESON_CLK_GATE_SYS_CLK(sys_periph, CLKCTRL_SYS_CLK_EN0_REG1, 29);
MESON_CLK_GATE_SYS_CLK(sys_pio, CLKCTRL_SYS_CLK_EN0_REG2, 0);
MESON_CLK_GATE_SYS_CLK(sys_i3c, CLKCTRL_SYS_CLK_EN0_REG2, 1);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);

#define MESON_CLK_GATE_HW(_name, _reg, _bit, _parent_hw)              \
static struct clk_regmap _name = {                                    \
	.data = &(struct clk_regmap_gate_data) {                      \
		.offset = (_reg),                                     \
		.bit_idx = (_bit),                                    \
	},                                                            \
	.hw.init = &(struct clk_init_data) {                          \
		.name = #_name,                                       \
		.ops = &clk_regmap_gate_ops,                          \
		.parent_hws = (const struct clk_hw *[]) {             \
			&(_parent_hw)                                 \
		},                                                    \
		.num_parents = 1,                                     \
		.flags = CLK_IGNORE_UNUSED,                           \
	},                                                            \
}

MESON_CLK_GATE_HW(sys_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3, sys_i2c_m_e.hw);
MESON_CLK_GATE_SYS_CLK(sys_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_HW(sys_i2c_m_i, CLKCTRL_SYS_CLK_EN0_REG2, 5, sys_i2c_m_e.hw);
MESON_CLK_GATE_HW(sys_i2c_m_g, CLKCTRL_SYS_CLK_EN0_REG2, 6, sys_i2c_m_e.hw);
MESON_CLK_GATE_HW(sys_i2c_m_h, CLKCTRL_SYS_CLK_EN0_REG2, 7, sys_i2c_m_e.hw);
MESON_CLK_GATE_SYS_CLK(sys_hdmi20_aes, CLKCTRL_SYS_CLK_EN0_REG2, 9);
MESON_CLK_GATE_SYS_CLK(sys_csi2_host, CLKCTRL_SYS_CLK_EN0_REG2, 16);
MESON_CLK_GATE_SYS_CLK(sys_csi2_adapt, CLKCTRL_SYS_CLK_EN0_REG2, 17);
MESON_CLK_GATE_SYS_CLK(sys_dspa, CLKCTRL_SYS_CLK_EN0_REG2, 21);
MESON_CLK_GATE_SYS_CLK(sys_pp_dma, CLKCTRL_SYS_CLK_EN0_REG2, 22);
MESON_CLK_GATE_SYS_CLK(sys_pp_wrapper, CLKCTRL_SYS_CLK_EN0_REG2, 23);
MESON_CLK_GATE_SYS_CLK(sys_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
MESON_CLK_GATE_SYS_CLK(sys_csi2_phy0, CLKCTRL_SYS_CLK_EN0_REG2, 27);
MESON_CLK_GATE_SYS_CLK(sys_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);

static struct clk_regmap sys_pwm_h;
MESON_CLK_GATE_HW(sys_pwm_j, CLKCTRL_SYS_CLK_EN0_REG2, 30, sys_pwm_h.hw);
MESON_CLK_GATE_HW(sys_pwm_i, CLKCTRL_SYS_CLK_EN0_REG2, 31, sys_pwm_h.hw);
MESON_CLK_GATE_SYS_CLK(sys_pwm_h, CLKCTRL_SYS_CLK_EN0_REG3, 0);
MESON_CLK_GATE_HW(sys_pwm_n, CLKCTRL_SYS_CLK_EN0_REG3, 8, sys_pwm_h.hw);
MESON_CLK_GATE_HW(sys_pwm_m, CLKCTRL_SYS_CLK_EN0_REG3, 9, sys_pwm_h.hw);
MESON_CLK_GATE_HW(sys_pwm_l, CLKCTRL_SYS_CLK_EN0_REG3, 10, sys_pwm_h.hw);
MESON_CLK_GATE_HW(sys_pwm_k, CLKCTRL_SYS_CLK_EN0_REG3, 11, sys_pwm_h.hw);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&hifi1_pll,
	&mclk_pll,
	&mclk1_pll,
	&mclk_0_pre_div,
	&mclk_0_pre,
	&mclk_0_mux,
	&mclk_0_div,
	&mclk_0,
	&mclk_1_pre_div,
	&mclk_1_pre,
	&mclk_1_mux,
	&mclk_1_div,
	&mclk_1,
	&mclk1_0_pre_div,
	&mclk1_0_pre,
	&mclk1_0_mux,
	&mclk1_0_div,
	&mclk1_0,
	&mclk1_1_pre_div,
	&mclk1_1_pre,
	&mclk1_1_mux,
	&mclk1_1_div,
	&mclk1_1
};

static struct clk_regmap *const clk_regmaps[] = {
	&sar_adc_mux,
	&sar_adc_div,
	&sar_adc,
	&sc_pre_mux,
	&sc_pre_div,
	&sc_pre,
	&sc,
	&video_src0_in_mux,
	&video_src0_in,
	&video_src0_div,
	&video_src0,
	&video_src1_in_mux,
	&video_src1_in,
	&video_src1_div,
	&video_src1,
	&video0_div1,
	&video0_div2_en,
	&video0_div4_en,
	&video0_div6_en,
	&video0_div12_en,
	&video1_div1,
	&video1_div2_en,
	&video1_div4_en,
	&video1_div6_en,
	&video1_div12_en,
	&lcd_an_mux,
	&lcd_an,
	&lcd_an_ph2,
	&lcd_an_ph3,
	&enc0_mux,
	&enc0,
	&enc1_mux,
	&enc1,
	&vdac_mux,
	&vdac,
	&hdmitx_pixel_mux,
	&hdmitx_pixel,
	&hdmitx_fe_mux,
	&hdmitx_fe,
	&hdmitx1_pixel_mux,
	&hdmitx1_pixel,
	&hdmitx1_fe_mux,
	&hdmitx1_fe,
	&mali_0_mux,
	&mali_0_div,
	&mali_0,
	&mali_1_mux,
	&mali_1_div,
	&mali_1,
	&mali,
	&mali_stack_0_mux,
	&mali_stack_0_div,
	&mali_stack_0,
	&mali_stack_1_mux,
	&mali_stack_1_div,
	&mali_stack_1,
	&mali_stack,
	&hevcf_0_mux,
	&hevcf_0_div,
	&hevcf_0,
	&hevcf_1_mux,
	&hevcf_1_div,
	&hevcf_1,
	&hevcf,
	&hcodec_0_mux,
	&hcodec_0_div,
	&hcodec_0,
	&hcodec_1_mux,
	&hcodec_1_div,
	&hcodec_1,
	&hcodec,
	&vpu_0_mux,
	&vpu_0_div,
	&vpu_0,
	&vpu_1_mux,
	&vpu_1_div,
	&vpu_1,
	&vpu,
	&vpu_clkb_tmp_mux,
	&vpu_clkb_tmp_div,
	&vpu_clkb_tmp,
	&vpu_clkb_div,
	&vpu_clkb,
	&vapb_0_mux,
	&vapb_0_div,
	&vapb_0,
	&vapb_1_mux,
	&vapb_1_div,
	&vapb_1,
	&vapb,
	&ge2d,
	&hdmitx_sys_mux,
	&hdmitx_sys_div,
	&hdmitx_sys,
	&hdmitx_prif_mux,
	&hdmitx_prif_div,
	&hdmitx_prif,
	&hdmitx_200m_mux,
	&hdmitx_200m_div,
	&hdmitx_200m,
	&hdmitx_aud_mux,
	&hdmitx_aud_div,
	&hdmitx_aud,
	&pwm_h_mux,
	&pwm_h_div,
	&pwm_h,
	&pwm_i_mux,
	&pwm_i_div,
	&pwm_i,
	&pwm_j_mux,
	&pwm_j_div,
	&pwm_j,
	&pwm_k_mux,
	&pwm_k_div,
	&pwm_k,
	&pwm_l_mux,
	&pwm_l_div,
	&pwm_l,
	&pwm_m_mux,
	&pwm_m_div,
	&pwm_m,
	&pwm_n_mux,
	&pwm_n_div,
	&pwm_n,
	&spisg0_mux,
	&spisg0_div,
	&spisg0,
	&spisg1_mux,
	&spisg1_div,
	&spisg1,
	&spisg2_mux,
	&spisg2_div,
	&spisg2,
	&sd_emmc_a_mux,
	&sd_emmc_a_div,
	&sd_emmc_a,
	&sd_emmc_b_mux,
	&sd_emmc_b_div,
	&sd_emmc_b,
	&sd_emmc_c_mux,
	&sd_emmc_c_div,
	&sd_emmc_c,
	&vdin_meas_mux,
	&vdin_meas_div,
	&vdin_meas,
	&vid_lock_mux,
	&vid_lock_div,
	&vid_lock,
	&eth_rmii_mux,
	&eth_rmii_div,
	&eth_rmii,
	&eth_125m,
	&ts_div,
	&ts,
	&amfc_mux,
	&amfc_div,
	&amfc,
	&pcie0_tl_mux,
	&pcie0_tl_div,
	&pcie0_tl,
	&pcie1_tl_mux,
	&pcie1_tl_div,
	&pcie1_tl,
	&pp_mux,
	&pp_div,
	&pp,
	&isp_mux,
	&isp_div,
	&isp,
	&dspa_0_mux,
	&dspa_0_div,
	&dspa_0,
	&dspa_1_mux,
	&dspa_1_div,
	&dspa_1,
	&dspa,
	&glb_mux,
	&glb_div,
	&glb,
	&can0_pe_mux,
	&can0_pe_div,
	&can0_pe,
	&can1_pe_mux,
	&can1_pe_div,
	&can1_pe,
	&can0_filter_mux,
	&can0_filter_div,
	&can0_filter,
	&can1_filter_mux,
	&can1_filter_div,
	&can1_filter,
	&i3c_mux,
	&i3c_div,
	&i3c,
	&cmpr_mux,
	&cmpr_div,
	&cmpr,
	&vge_mux,
	&vge_div,
	&vge,
	&cve_mux,
	&cve_div,
	&cve,
	&dewarpa_mux,
	&dewarpa_div,
	&dewarpa,
	&dptx_apb2_mux,
	&dptx_apb2_div,
	&dptx_apb2,
	&dptx_aud_mux,
	&dptx_aud_div,
	&dptx_aud,
	&dsi_meas_mux,
	&dsi_meas_div,
	&dsi_meas,
	&dsi_b_meas_mux,
	&dsi_b_meas_div,
	&dsi_b_meas,
	&hdmirx_2m_mux,
	&hdmirx_2m_div,
	&hdmirx_2m,
	&hdmirx_5m_mux,
	&hdmirx_5m_div,
	&hdmirx_5m,
	&hdmirx_cfg_mux,
	&hdmirx_cfg_div,
	&hdmirx_cfg,
	&hdmirx_hdcp2x_mux,
	&hdmirx_hdcp2x_div,
	&hdmirx_hdcp2x,
	&hdmirx_acr_ref_mux,
	&hdmirx_acr_ref_div,
	&hdmirx_acr_ref,
	&hdmirx_meter_mux,
	&hdmirx_meter_div,
	&hdmirx_meter,
	&usb_250m_mux,
	&usb_250m_div,
	&usb_250m,
	&nna_core_mux,
	&nna_core_div,
	&nna_core,
	&csi_phy_mux,
	&csi_phy_div,
	&csi_phy,
	&usb_48m_pre_mux,
	&usb_48m_pre_div,
	&usb_48m_pre,
	&usb_48m_dual_clkin,
	&usb_48m_dual_div,
	&usb_48m_dual_mux,
	&usb_48m_dual,
	&usb_48m,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_am_axi,
	&sys_dos,
	&sys_mipi_dsi,
	&sys_eth_phy,
	&sys_amfc,
	&sys_mali,
	&sys_nna,
	&sys_eth_axi,
	&sys_dp_apb,
	&sys_edptx_apb,
	&sys_u3hsg,
	&sys_aucpu,
	&sys_glb,
	&sys_combo_dphy_apb,
	&sys_hdmirx_apb,
	&sys_hdmirx_pclk,
	&sys_mipi_dsi_phy,
	&sys_can1,
	&sys_can0,
	&sys_sd_emmc_a,
	&sys_sd_emmc_b,
	&sys_sd_emmc_c,
	&sys_sc,
	&sys_acodec,
	&sys_mipi_isp,
	&sys_msr_clk,
	&sys_audio,
	&sys_mipi_dsi_b,
	&sys_mipi_dsi1_phy,
	&sys_eth,
	&sys_eth_1g_mac,
	&sys_uart_a,
	&sys_uart_f,
	&sys_ts_a55,
	&sys_eth_1g_axi,
	&sys_ts_dos,
	&sys_usb3drd_b,
	&sys_ts_core,
	&sys_ts_pll,
	&sys_csi_dig_clkin,
	&sys_cve,
	&sys_ge2d,
	&sys_spisg0,
	&sys_u3drd_1,
	&sys_u2h,
	&sys_pciemac_a,
	&sys_usb3drd_a,
	&sys_usb2drd,
	&sys_pciephy,
	&sys_pciemac_b,
	&sys_periph,
	&sys_pio,
	&sys_i3c,
	&sys_i2c_m_e,
	&sys_i2c_m_f,
	&sys_hdmitx_apb,
	&sys_i2c_m_i,
	&sys_i2c_m_g,
	&sys_i2c_m_h,
	&sys_hdmi20_aes,
	&sys_csi2_host,
	&sys_csi2_adapt,
	&sys_dspa,
	&sys_pp_dma,
	&sys_pp_wrapper,
	&sys_vpu_intr,
	&sys_csi2_phy0,
	&sys_sar_adc,
	&sys_pwm_j,
	&sys_pwm_i,
	&sys_pwm_h,
	&sys_pwm_n,
	&sys_pwm_m,
	&sys_pwm_l,
	&sys_pwm_k
};

static struct clk_hw *a9_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_HIFI1_PLL]             = &hifi1_pll.hw,
	[CLKID_MCLK_PLL]              = &mclk_pll.hw,
	[CLKID_MCLK1_PLL]             = &mclk1_pll.hw,
	[CLKID_MCLK_0_PRE_DIV]        = &mclk_0_pre_div.hw,
	[CLKID_MCLK_0_PRE]            = &mclk_0_pre.hw,
	[CLKID_MCLK_0_MUX]            = &mclk_0_mux.hw,
	[CLKID_MCLK_0_DIV]            = &mclk_0_div.hw,
	[CLKID_MCLK_0]                = &mclk_0.hw,
	[CLKID_MCLK_1_PRE_DIV]        = &mclk_1_pre_div.hw,
	[CLKID_MCLK_1_PRE]            = &mclk_1_pre.hw,
	[CLKID_MCLK_1_MUX]            = &mclk_1_mux.hw,
	[CLKID_MCLK_1_DIV]            = &mclk_1_div.hw,
	[CLKID_MCLK_1]                = &mclk_1.hw,
	[CLKID_MCLK1_0_PRE_DIV]       = &mclk1_0_pre_div.hw,
	[CLKID_MCLK1_0_PRE]           = &mclk1_0_pre.hw,
	[CLKID_MCLK1_0_MUX]           = &mclk1_0_mux.hw,
	[CLKID_MCLK1_0_DIV]           = &mclk1_0_div.hw,
	[CLKID_MCLK1_0]               = &mclk1_0.hw,
	[CLKID_MCLK1_1_PRE_DIV]       = &mclk1_1_pre_div.hw,
	[CLKID_MCLK1_1_PRE]           = &mclk1_1_pre.hw,
	[CLKID_MCLK1_1_MUX]           = &mclk1_1_mux.hw,
	[CLKID_MCLK1_1_DIV]           = &mclk1_1_div.hw,
	[CLKID_MCLK1_1]               = &mclk1_1.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_SC_PRE_MUX]            = &sc_pre_mux.hw,
	[CLKID_SC_PRE_DIV]            = &sc_pre_div.hw,
	[CLKID_SC_PRE]                = &sc_pre.hw,
	[CLKID_SC]                    = &sc.hw,
	[CLKID_VIDEO_SRC0_IN_MUX]     = &video_src0_in_mux.hw,
	[CLKID_VIDEO_SRC0_IN]         = &video_src0_in.hw,
	[CLKID_VIDEO_SRC0_DIV]        = &video_src0_div.hw,
	[CLKID_VIDEO_SRC0]            = &video_src0.hw,
	[CLKID_VIDEO_SRC1_IN_MUX]     = &video_src1_in_mux.hw,
	[CLKID_VIDEO_SRC1_IN]         = &video_src1_in.hw,
	[CLKID_VIDEO_SRC1_DIV]        = &video_src1_div.hw,
	[CLKID_VIDEO_SRC1]            = &video_src1.hw,
	[CLKID_VIDEO0_DIV1]           = &video0_div1.hw,
	[CLKID_VIDEO0_DIV2_EN]        = &video0_div2_en.hw,
	[CLKID_VIDEO0_DIV2]           = &video0_div2.hw,
	[CLKID_VIDEO0_DIV4_EN]        = &video0_div4_en.hw,
	[CLKID_VIDEO0_DIV4]           = &video0_div4.hw,
	[CLKID_VIDEO0_DIV6_EN]        = &video0_div6_en.hw,
	[CLKID_VIDEO0_DIV6]           = &video0_div6.hw,
	[CLKID_VIDEO0_DIV12_EN]       = &video0_div12_en.hw,
	[CLKID_VIDEO0_DIV12]          = &video0_div12.hw,
	[CLKID_VIDEO1_DIV1]           = &video1_div1.hw,
	[CLKID_VIDEO1_DIV2_EN]        = &video1_div2_en.hw,
	[CLKID_VIDEO1_DIV2]           = &video1_div2.hw,
	[CLKID_VIDEO1_DIV4_EN]        = &video1_div4_en.hw,
	[CLKID_VIDEO1_DIV4]           = &video1_div4.hw,
	[CLKID_VIDEO1_DIV6_EN]        = &video1_div6_en.hw,
	[CLKID_VIDEO1_DIV6]           = &video1_div6.hw,
	[CLKID_VIDEO1_DIV12_EN]       = &video1_div12_en.hw,
	[CLKID_VIDEO1_DIV12]          = &video1_div12.hw,
	[CLKID_LCD_AN_MUX]            = &lcd_an_mux.hw,
	[CLKID_LCD_AN]                = &lcd_an.hw,
	[CLKID_LCD_AN_PH2]            = &lcd_an_ph2.hw,
	[CLKID_LCD_AN_PH3]            = &lcd_an_ph3.hw,
	[CLKID_ENC0_MUX]              = &enc0_mux.hw,
	[CLKID_ENC0]                  = &enc0.hw,
	[CLKID_ENC1_MUX]              = &enc1_mux.hw,
	[CLKID_ENC1]                  = &enc1.hw,
	[CLKID_VDAC_MUX]              = &vdac_mux.hw,
	[CLKID_VDAC]                  = &vdac.hw,
	[CLKID_HDMITX_PIXEL_MUX]      = &hdmitx_pixel_mux.hw,
	[CLKID_HDMITX_PIXEL]          = &hdmitx_pixel.hw,
	[CLKID_HDMITX_FE_MUX]         = &hdmitx_fe_mux.hw,
	[CLKID_HDMITX_FE]             = &hdmitx_fe.hw,
	[CLKID_HDMITX1_PIXEL_MUX]     = &hdmitx1_pixel_mux.hw,
	[CLKID_HDMITX1_PIXEL]         = &hdmitx1_pixel.hw,
	[CLKID_HDMITX1_FE_MUX]        = &hdmitx1_fe_mux.hw,
	[CLKID_HDMITX1_FE]            = &hdmitx1_fe.hw,
	[CLKID_MALI_0_MUX]            = &mali_0_mux.hw,
	[CLKID_MALI_0_DIV]            = &mali_0_div.hw,
	[CLKID_MALI_0]                = &mali_0.hw,
	[CLKID_MALI_1_MUX]            = &mali_1_mux.hw,
	[CLKID_MALI_1_DIV]            = &mali_1_div.hw,
	[CLKID_MALI_1]                = &mali_1.hw,
	[CLKID_MALI]                  = &mali.hw,
	[CLKID_MALI_STACK_0_MUX]      = &mali_stack_0_mux.hw,
	[CLKID_MALI_STACK_0_DIV]      = &mali_stack_0_div.hw,
	[CLKID_MALI_STACK_0]          = &mali_stack_0.hw,
	[CLKID_MALI_STACK_1_MUX]      = &mali_stack_1_mux.hw,
	[CLKID_MALI_STACK_1_DIV]      = &mali_stack_1_div.hw,
	[CLKID_MALI_STACK_1]          = &mali_stack_1.hw,
	[CLKID_MALI_STACK]            = &mali_stack.hw,
	[CLKID_HEVCF_0_MUX]           = &hevcf_0_mux.hw,
	[CLKID_HEVCF_0_DIV]           = &hevcf_0_div.hw,
	[CLKID_HEVCF_0]               = &hevcf_0.hw,
	[CLKID_HEVCF_1_MUX]           = &hevcf_1_mux.hw,
	[CLKID_HEVCF_1_DIV]           = &hevcf_1_div.hw,
	[CLKID_HEVCF_1]               = &hevcf_1.hw,
	[CLKID_HEVCF]                 = &hevcf.hw,
	[CLKID_HCODEC_0_MUX]          = &hcodec_0_mux.hw,
	[CLKID_HCODEC_0_DIV]          = &hcodec_0_div.hw,
	[CLKID_HCODEC_0]              = &hcodec_0.hw,
	[CLKID_HCODEC_1_MUX]          = &hcodec_1_mux.hw,
	[CLKID_HCODEC_1_DIV]          = &hcodec_1_div.hw,
	[CLKID_HCODEC_1]              = &hcodec_1.hw,
	[CLKID_HCODEC]                = &hcodec.hw,
	[CLKID_VPU_0_MUX]             = &vpu_0_mux.hw,
	[CLKID_VPU_0_DIV]             = &vpu_0_div.hw,
	[CLKID_VPU_0]                 = &vpu_0.hw,
	[CLKID_VPU_1_MUX]             = &vpu_1_mux.hw,
	[CLKID_VPU_1_DIV]             = &vpu_1_div.hw,
	[CLKID_VPU_1]                 = &vpu_1.hw,
	[CLKID_VPU]                   = &vpu.hw,
	[CLKID_VPU_CLKB_TMP_MUX]      = &vpu_clkb_tmp_mux.hw,
	[CLKID_VPU_CLKB_TMP_DIV]      = &vpu_clkb_tmp_div.hw,
	[CLKID_VPU_CLKB_TMP]          = &vpu_clkb_tmp.hw,
	[CLKID_VPU_CLKB_DIV]          = &vpu_clkb_div.hw,
	[CLKID_VPU_CLKB]              = &vpu_clkb.hw,
	[CLKID_VAPB_0_MUX]            = &vapb_0_mux.hw,
	[CLKID_VAPB_0_DIV]            = &vapb_0_div.hw,
	[CLKID_VAPB_0]                = &vapb_0.hw,
	[CLKID_VAPB_1_MUX]            = &vapb_1_mux.hw,
	[CLKID_VAPB_1_DIV]            = &vapb_1_div.hw,
	[CLKID_VAPB_1]                = &vapb_1.hw,
	[CLKID_VAPB]                  = &vapb.hw,
	[CLKID_GE2D]                  = &ge2d.hw,
	[CLKID_HDMITX_SYS_MUX]        = &hdmitx_sys_mux.hw,
	[CLKID_HDMITX_SYS_DIV]        = &hdmitx_sys_div.hw,
	[CLKID_HDMITX_SYS]            = &hdmitx_sys.hw,
	[CLKID_HDMITX_PRIF_MUX]       = &hdmitx_prif_mux.hw,
	[CLKID_HDMITX_PRIF_DIV]       = &hdmitx_prif_div.hw,
	[CLKID_HDMITX_PRIF]           = &hdmitx_prif.hw,
	[CLKID_HDMITX_200M_MUX]       = &hdmitx_200m_mux.hw,
	[CLKID_HDMITX_200M_DIV]       = &hdmitx_200m_div.hw,
	[CLKID_HDMITX_200M]           = &hdmitx_200m.hw,
	[CLKID_HDMITX_AUD_MUX]        = &hdmitx_aud_mux.hw,
	[CLKID_HDMITX_AUD_DIV]        = &hdmitx_aud_div.hw,
	[CLKID_HDMITX_AUD]            = &hdmitx_aud.hw,
	[CLKID_PWM_H_MUX]             = &pwm_h_mux.hw,
	[CLKID_PWM_H_DIV]             = &pwm_h_div.hw,
	[CLKID_PWM_H]                 = &pwm_h.hw,
	[CLKID_PWM_I_MUX]             = &pwm_i_mux.hw,
	[CLKID_PWM_I_DIV]             = &pwm_i_div.hw,
	[CLKID_PWM_I]                 = &pwm_i.hw,
	[CLKID_PWM_J_MUX]             = &pwm_j_mux.hw,
	[CLKID_PWM_J_DIV]             = &pwm_j_div.hw,
	[CLKID_PWM_J]                 = &pwm_j.hw,
	[CLKID_PWM_K_MUX]             = &pwm_k_mux.hw,
	[CLKID_PWM_K_DIV]             = &pwm_k_div.hw,
	[CLKID_PWM_K]                 = &pwm_k.hw,
	[CLKID_PWM_L_MUX]             = &pwm_l_mux.hw,
	[CLKID_PWM_L_DIV]             = &pwm_l_div.hw,
	[CLKID_PWM_L]                 = &pwm_l.hw,
	[CLKID_PWM_M_MUX]             = &pwm_m_mux.hw,
	[CLKID_PWM_M_DIV]             = &pwm_m_div.hw,
	[CLKID_PWM_M]                 = &pwm_m.hw,
	[CLKID_PWM_N_MUX]             = &pwm_n_mux.hw,
	[CLKID_PWM_N_DIV]             = &pwm_n_div.hw,
	[CLKID_PWM_N]                 = &pwm_n.hw,
	[CLKID_SPISG0_MUX]            = &spisg0_mux.hw,
	[CLKID_SPISG0_DIV]            = &spisg0_div.hw,
	[CLKID_SPISG0]                = &spisg0.hw,
	[CLKID_SPISG1_MUX]            = &spisg1_mux.hw,
	[CLKID_SPISG1_DIV]            = &spisg1_div.hw,
	[CLKID_SPISG1]                = &spisg1.hw,
	[CLKID_SPISG2_MUX]            = &spisg2_mux.hw,
	[CLKID_SPISG2_DIV]            = &spisg2_div.hw,
	[CLKID_SPISG2]                = &spisg2.hw,
	[CLKID_SD_EMMC_A_MUX]         = &sd_emmc_a_mux.hw,
	[CLKID_SD_EMMC_A_DIV]         = &sd_emmc_a_div.hw,
	[CLKID_SD_EMMC_A]             = &sd_emmc_a.hw,
	[CLKID_SD_EMMC_B_MUX]         = &sd_emmc_b_mux.hw,
	[CLKID_SD_EMMC_B_DIV]         = &sd_emmc_b_div.hw,
	[CLKID_SD_EMMC_B]             = &sd_emmc_b.hw,
	[CLKID_SD_EMMC_C_MUX]         = &sd_emmc_c_mux.hw,
	[CLKID_SD_EMMC_C_DIV]         = &sd_emmc_c_div.hw,
	[CLKID_SD_EMMC_C]             = &sd_emmc_c.hw,
	[CLKID_VDIN_MEAS_MUX]         = &vdin_meas_mux.hw,
	[CLKID_VDIN_MEAS_DIV]         = &vdin_meas_div.hw,
	[CLKID_VDIN_MEAS]             = &vdin_meas.hw,
	[CLKID_VID_LOCK_MUX]          = &vid_lock_mux.hw,
	[CLKID_VID_LOCK_DIV]          = &vid_lock_div.hw,
	[CLKID_VID_LOCK]              = &vid_lock.hw,
	[CLKID_ETH_RMII_MUX]          = &eth_rmii_mux.hw,
	[CLKID_ETH_RMII_DIV]          = &eth_rmii_div.hw,
	[CLKID_ETH_RMII]              = &eth_rmii.hw,
	[CLKID_ETH_125M_DIV]          = &eth_125m_div.hw,
	[CLKID_ETH_125M]              = &eth_125m.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_AMFC_MUX]              = &amfc_mux.hw,
	[CLKID_AMFC_DIV]              = &amfc_div.hw,
	[CLKID_AMFC]                  = &amfc.hw,
	[CLKID_PCIE0_TL_MUX]          = &pcie0_tl_mux.hw,
	[CLKID_PCIE0_TL_DIV]          = &pcie0_tl_div.hw,
	[CLKID_PCIE0_TL]              = &pcie0_tl.hw,
	[CLKID_PCIE1_TL_MUX]          = &pcie1_tl_mux.hw,
	[CLKID_PCIE1_TL_DIV]          = &pcie1_tl_div.hw,
	[CLKID_PCIE1_TL]              = &pcie1_tl.hw,
	[CLKID_PP_MUX]                = &pp_mux.hw,
	[CLKID_PP_DIV]                = &pp_div.hw,
	[CLKID_PP]                    = &pp.hw,
	[CLKID_ISP_MUX]               = &isp_mux.hw,
	[CLKID_ISP_DIV]               = &isp_div.hw,
	[CLKID_ISP]                   = &isp.hw,
	[CLKID_DSPA_0_MUX]            = &dspa_0_mux.hw,
	[CLKID_DSPA_0_DIV]            = &dspa_0_div.hw,
	[CLKID_DSPA_0]                = &dspa_0.hw,
	[CLKID_DSPA_1_MUX]            = &dspa_1_mux.hw,
	[CLKID_DSPA_1_DIV]            = &dspa_1_div.hw,
	[CLKID_DSPA_1]                = &dspa_1.hw,
	[CLKID_DSPA]                  = &dspa.hw,
	[CLKID_GLB_MUX]               = &glb_mux.hw,
	[CLKID_GLB_DIV]               = &glb_div.hw,
	[CLKID_GLB]                   = &glb.hw,
	[CLKID_CAN0_PE_MUX]           = &can0_pe_mux.hw,
	[CLKID_CAN0_PE_DIV]           = &can0_pe_div.hw,
	[CLKID_CAN0_PE]               = &can0_pe.hw,
	[CLKID_CAN1_PE_MUX]           = &can1_pe_mux.hw,
	[CLKID_CAN1_PE_DIV]           = &can1_pe_div.hw,
	[CLKID_CAN1_PE]               = &can1_pe.hw,
	[CLKID_CAN0_FILTER_MUX]       = &can0_filter_mux.hw,
	[CLKID_CAN0_FILTER_DIV]       = &can0_filter_div.hw,
	[CLKID_CAN0_FILTER]           = &can0_filter.hw,
	[CLKID_CAN1_FILTER_MUX]       = &can1_filter_mux.hw,
	[CLKID_CAN1_FILTER_DIV]       = &can1_filter_div.hw,
	[CLKID_CAN1_FILTER]           = &can1_filter.hw,
	[CLKID_I3C_MUX]               = &i3c_mux.hw,
	[CLKID_I3C_DIV]               = &i3c_div.hw,
	[CLKID_I3C]                   = &i3c.hw,
	[CLKID_CMPR_MUX]              = &cmpr_mux.hw,
	[CLKID_CMPR_DIV]              = &cmpr_div.hw,
	[CLKID_CMPR]                  = &cmpr.hw,
	[CLKID_VGE_MUX]               = &vge_mux.hw,
	[CLKID_VGE_DIV]               = &vge_div.hw,
	[CLKID_VGE]                   = &vge.hw,
	[CLKID_CVE_MUX]               = &cve_mux.hw,
	[CLKID_CVE_DIV]               = &cve_div.hw,
	[CLKID_CVE]                   = &cve.hw,
	[CLKID_DEWARPA_MUX]           = &dewarpa_mux.hw,
	[CLKID_DEWARPA_DIV]           = &dewarpa_div.hw,
	[CLKID_DEWARPA]               = &dewarpa.hw,
	[CLKID_DPTX_APB2_MUX]         = &dptx_apb2_mux.hw,
	[CLKID_DPTX_APB2_DIV]         = &dptx_apb2_div.hw,
	[CLKID_DPTX_APB2]             = &dptx_apb2.hw,
	[CLKID_DPTX_AUD_MUX]          = &dptx_aud_mux.hw,
	[CLKID_DPTX_AUD_DIV]          = &dptx_aud_div.hw,
	[CLKID_DPTX_AUD]              = &dptx_aud.hw,
	[CLKID_DSI_MEAS_MUX]          = &dsi_meas_mux.hw,
	[CLKID_DSI_MEAS_DIV]          = &dsi_meas_div.hw,
	[CLKID_DSI_MEAS]              = &dsi_meas.hw,
	[CLKID_DSI_B_MEAS_MUX]        = &dsi_b_meas_mux.hw,
	[CLKID_DSI_B_MEAS_DIV]        = &dsi_b_meas_div.hw,
	[CLKID_DSI_B_MEAS]            = &dsi_b_meas.hw,
	[CLKID_HDMIRX_2M_MUX]         = &hdmirx_2m_mux.hw,
	[CLKID_HDMIRX_2M_DIV]         = &hdmirx_2m_div.hw,
	[CLKID_HDMIRX_2M]             = &hdmirx_2m.hw,
	[CLKID_HDMIRX_5M_MUX]         = &hdmirx_5m_mux.hw,
	[CLKID_HDMIRX_5M_DIV]         = &hdmirx_5m_div.hw,
	[CLKID_HDMIRX_5M]             = &hdmirx_5m.hw,
	[CLKID_HDMIRX_CFG_MUX]        = &hdmirx_cfg_mux.hw,
	[CLKID_HDMIRX_CFG_DIV]        = &hdmirx_cfg_div.hw,
	[CLKID_HDMIRX_CFG]            = &hdmirx_cfg.hw,
	[CLKID_HDMIRX_HDCP2X_MUX]     = &hdmirx_hdcp2x_mux.hw,
	[CLKID_HDMIRX_HDCP2X_DIV]     = &hdmirx_hdcp2x_div.hw,
	[CLKID_HDMIRX_HDCP2X]         = &hdmirx_hdcp2x.hw,
	[CLKID_HDMIRX_ACR_REF_MUX]    = &hdmirx_acr_ref_mux.hw,
	[CLKID_HDMIRX_ACR_REF_DIV]    = &hdmirx_acr_ref_div.hw,
	[CLKID_HDMIRX_ACR_REF]        = &hdmirx_acr_ref.hw,
	[CLKID_HDMIRX_METER_MUX]      = &hdmirx_meter_mux.hw,
	[CLKID_HDMIRX_METER_DIV]      = &hdmirx_meter_div.hw,
	[CLKID_HDMIRX_METER]          = &hdmirx_meter.hw,
	[CLKID_USB_250M_MUX]          = &usb_250m_mux.hw,
	[CLKID_USB_250M_DIV]          = &usb_250m_div.hw,
	[CLKID_USB_250M]              = &usb_250m.hw,
	[CLKID_NNA_CORE_MUX]          = &nna_core_mux.hw,
	[CLKID_NNA_CORE_DIV]          = &nna_core_div.hw,
	[CLKID_NNA_CORE]              = &nna_core.hw,
	[CLKID_CSI_PHY_MUX]           = &csi_phy_mux.hw,
	[CLKID_CSI_PHY_DIV]           = &csi_phy_div.hw,
	[CLKID_CSI_PHY]               = &csi_phy.hw,
	[CLKID_USB_48M_PRE_MUX]       = &usb_48m_pre_mux.hw,
	[CLKID_USB_48M_PRE_DIV]       = &usb_48m_pre_div.hw,
	[CLKID_USB_48M_PRE]           = &usb_48m_pre.hw,
	[CLKID_USB_48M_DUAL_CLKIN]    = &usb_48m_dual_clkin.hw,
	[CLKID_USB_48M_DUAL_DIV]      = &usb_48m_dual_div.hw,
	[CLKID_USB_48M_DUAL_MUX]      = &usb_48m_dual_mux.hw,
	[CLKID_USB_48M_DUAL]          = &usb_48m_dual.hw,
	[CLKID_USB_48M]               = &usb_48m.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_AM_AXI]            = &sys_am_axi.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_MIPI_DSI]          = &sys_mipi_dsi.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_AMFC]              = &sys_amfc.hw,
	[CLKID_SYS_MALI]              = &sys_mali.hw,
	[CLKID_SYS_NNA]               = &sys_nna.hw,
	[CLKID_SYS_ETH_AXI]           = &sys_eth_axi.hw,
	[CLKID_SYS_DP_APB]            = &sys_dp_apb.hw,
	[CLKID_SYS_EDPTX_APB]         = &sys_edptx_apb.hw,
	[CLKID_SYS_U3HSG]             = &sys_u3hsg.hw,
	[CLKID_SYS_AUCPU]             = &sys_aucpu.hw,
	[CLKID_SYS_GLB]               = &sys_glb.hw,
	[CLKID_SYS_COMBO_DPHY_APB]    = &sys_combo_dphy_apb.hw,
	[CLKID_SYS_HDMIRX_APB]        = &sys_hdmirx_apb.hw,
	[CLKID_SYS_HDMIRX_PCLK]       = &sys_hdmirx_pclk.hw,
	[CLKID_SYS_MIPI_DSI_PHY]      = &sys_mipi_dsi_phy.hw,
	[CLKID_SYS_CAN1]              = &sys_can1.hw,
	[CLKID_SYS_CAN0]              = &sys_can0.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_SC]                = &sys_sc.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_MIPI_ISP]          = &sys_mipi_isp.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_MIPI_DSI_B]        = &sys_mipi_dsi_b.hw,
	[CLKID_SYS_MIPI_DSI1_PHY]     = &sys_mipi_dsi1_phy.hw,
	[CLKID_SYS_ETH]               = &sys_eth.hw,
	[CLKID_SYS_ETH_1G_MAC]        = &sys_eth_1g_mac.hw,
	[CLKID_SYS_UART_A]            = &sys_uart_a.hw,
	[CLKID_SYS_UART_F]            = &sys_uart_f.hw,
	[CLKID_SYS_TS_A55]            = &sys_ts_a55.hw,
	[CLKID_SYS_ETH_1G_AXI]        = &sys_eth_1g_axi.hw,
	[CLKID_SYS_TS_DOS]            = &sys_ts_dos.hw,
	[CLKID_SYS_USB3DRD_B]         = &sys_usb3drd_b.hw,
	[CLKID_SYS_TS_CORE]           = &sys_ts_core.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_CSI_DIG_CLKIN]     = &sys_csi_dig_clkin.hw,
	[CLKID_SYS_CVE]               = &sys_cve.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPISG0]            = &sys_spisg0.hw,
	[CLKID_SYS_U3DRD_1]           = &sys_u3drd_1.hw,
	[CLKID_SYS_U2H]               = &sys_u2h.hw,
	[CLKID_SYS_PCIEMAC_A]         = &sys_pciemac_a.hw,
	[CLKID_SYS_USB3DRD_A]         = &sys_usb3drd_a.hw,
	[CLKID_SYS_USB2DRD]           = &sys_usb2drd.hw,
	[CLKID_SYS_PCIEPHY]           = &sys_pciephy.hw,
	[CLKID_SYS_PCIEMAC_B]         = &sys_pciemac_b.hw,
	[CLKID_SYS_PERIPH]            = &sys_periph.hw,
	[CLKID_SYS_PIO]               = &sys_pio.hw,
	[CLKID_SYS_I3C]               = &sys_i3c.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_I2C_M_F]           = &sys_i2c_m_f.hw,
	[CLKID_SYS_HDMITX_APB]        = &sys_hdmitx_apb.hw,
	[CLKID_SYS_I2C_M_I]           = &sys_i2c_m_i.hw,
	[CLKID_SYS_I2C_M_G]           = &sys_i2c_m_g.hw,
	[CLKID_SYS_I2C_M_H]           = &sys_i2c_m_h.hw,
	[CLKID_SYS_HDMI20_AES]        = &sys_hdmi20_aes.hw,
	[CLKID_SYS_CSI2_HOST]         = &sys_csi2_host.hw,
	[CLKID_SYS_CSI2_ADAPT]        = &sys_csi2_adapt.hw,
	[CLKID_SYS_DSPA]              = &sys_dspa.hw,
	[CLKID_SYS_PP_DMA]            = &sys_pp_dma.hw,
	[CLKID_SYS_PP_WRAPPER]        = &sys_pp_wrapper.hw,
	[CLKID_SYS_VPU_INTR]          = &sys_vpu_intr.hw,
	[CLKID_SYS_CSI2_PHY0]         = &sys_csi2_phy0.hw,
	[CLKID_SYS_SAR_ADC]           = &sys_sar_adc.hw,
	[CLKID_SYS_PWM_J]             = &sys_pwm_j.hw,
	[CLKID_SYS_PWM_I]             = &sys_pwm_i.hw,
	[CLKID_SYS_PWM_H]             = &sys_pwm_h.hw,
	[CLKID_SYS_PWM_N]             = &sys_pwm_n.hw,
	[CLKID_SYS_PWM_M]             = &sys_pwm_m.hw,
	[CLKID_SYS_PWM_L]             = &sys_pwm_l.hw,
	[CLKID_SYS_PWM_K]             = &sys_pwm_k.hw
};

static const struct meson_clk_hw_data a9_clks = {
	.hws = a9_hw_clks,
	.num = ARRAY_SIZE(a9_hw_clks),
};

static int meson_a9_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *pll_map;
	struct meson_clk_hw_data *data;
#ifdef CONFIG_AMLOGIC_CLK_DEBUG
	struct clk *clk;
#endif
	int ret, i;

	data = (struct meson_clk_hw_data *)of_device_get_match_data(dev);
	if (!data)
		return -ENODEV;

#ifdef CONFIG_AMLOGIC_CLK_DEBUG
	clk = devm_clk_get(dev, "xtal");
	if (IS_ERR(clk)) {
		pr_err("%s: clock source xtal not found\n", dev_name(&pdev->dev));
		return PTR_ERR(clk);
	}

	ret = devm_clk_hw_register_clkdev(dev, __clk_get_hw(clk),
					  NULL,
					  __clk_get_name(clk));
	if (ret < 0) {
		dev_err(dev, "Failed to clkdev register: %d\n", ret);
		return ret;
	}
#endif

	basic_map = meson_clk_regmap_resource(pdev, dev, 0);
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = meson_clk_regmap_resource(pdev, dev, 1);
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(clk_regmaps); i++)
		clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(pll_regmaps); i++)
		pll_regmaps[i]->map = pll_map;

	for (i = 0; i < data->num; i++) {
		/* array might be sparse */
		if (!data->hws[i])
			continue;

		ret = devm_clk_hw_register(dev, data->hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}

#ifdef CONFIG_AMLOGIC_CLK_DEBUG
		ret = devm_clk_hw_register_clkdev(dev, data->hws[i],
						  NULL,
						  clk_hw_get_name(data->hws[i]));
		if (ret < 0) {
			dev_err(dev, "Failed to clkdev register: %d\n", ret);
			return ret;
		}
#endif
	}

	return devm_of_clk_add_hw_provider(dev, meson_clk_hw_get,
					   (void *)data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,a9-clkc",
		.data = &a9_clks
	},
	{}
};

static struct platform_driver a9_driver = {
	.probe			= meson_a9_probe,
	.driver			= {
		.name		= "a9-clkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init a9_clk_init(void)
{
	return platform_driver_register(&a9_driver);
}

void __exit a9_clk_exit(void)
{
	platform_driver_unregister(&a9_driver);
}
#else
builtin_platform_driver(a9_driver);
MODULE_LICENSE("GPL v2");
#endif
