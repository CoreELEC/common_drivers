// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "c5.h"
#include <dt-bindings/clock/amlogic,c5-clkc.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(128, 0, 0), /* DCO = 1536M OD = 0 PLL = 1536M */
	PLL_PARAMS_OD(192, 0, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
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
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_NOINIT_ENABLED |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "gp0_pll_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi_pll_table[] = {
	PLL_PARAMS_OD(150, 0, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 0, 2), /* DCO = 1944M OD = 4 PLL = 486M */
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
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "hifi_pll_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_mult_range dsi_pll_range = {
	.min = 116,
	.max = 234
};

static const struct reg_sequence dsi_pll_init_regs[] = {
	{ .reg = ANACTRL_DSIPLL_CTRL0, .def = 0x01010000 },
	{ .reg = ANACTRL_DSIPLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_DSIPLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_DSIPLL_CTRL3, .def = 0x00008010 }
};

static struct clk_regmap dsi_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.frac = {
			.reg_off = ANACTRL_DSIPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 20,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_DSIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_DSIPLL_CTRL3,
			.shift   = 29,
			.width   = 1,
		},
		.range = &dsi_pll_range,
		.init_regs = dsi_pll_init_regs,
		.init_count = ARRAY_SIZE(dsi_pll_init_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsi_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "dsi_pll_osc",
		},
		.num_parents = 1,
	},
};

static const struct pll_mult_range mclk_pll_range = {
	.min = 116,
	.max = 234
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
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct pll_mult_range mclk1_pll_range = {
	.min = 116,
	.max = 234
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
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div50m_div = {
	.mult = 1,
	.div = 40,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div50m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div50m",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div50m_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 28,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2p5_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div3 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div3_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div4 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div4_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div5 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 26,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div5_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_div7 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div7_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk_0_pre_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.shift = 0,
		.width = 3,
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
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div50m.hw }
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
		.width = 3,
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
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div50m.hw }
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
		.width = 3,
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
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div50m.hw }
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
		.width = 3,
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
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div50m.hw }
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

static struct clk_regmap rtc_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param rtc_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap rtc_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 28,
			.width   = 2,
		},
		.table = rtc_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rtc_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data rtc_dual_parent_data[] = {
	{ .hw = &rtc_dual_div.hw },
	{ .hw = &rtc_dual_clkin.hw }
};

static struct clk_regmap rtc_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rtc_dual_parent_data,
		.num_parents = ARRAY_SIZE(rtc_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap rtc_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rtc_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data rtc_clk_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &rtc_dual.hw }
};

static struct clk_regmap rtc_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.mask = 0x3,
		.shift = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rtc_clk_parent_data,
		.num_parents = ARRAY_SIZE(rtc_clk_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sar_adc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw }
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

static const struct clk_parent_data pwm_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_C_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_C_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_c_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_C_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_c_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_D_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_D_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_d_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_D_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_d_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_E_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_E_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_e_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_E_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_e_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_F_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_F_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_f_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_F_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_f_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_G_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_G_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_g_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_G_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_g_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
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

static struct clk_regmap pwm_o_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_O_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_o_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_o_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_O_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_o_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_o_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_o = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_O_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_o",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_o_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_p_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_P_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_p_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_p_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_P_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_p_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_p_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_p = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_P_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_p",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_p_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_q_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_Q_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_q_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_q_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_Q_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_q_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_q_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_q = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_Q_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_q",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_q_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_r_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_R_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_r_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_r_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_R_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_r_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_r_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_r = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_R_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_r",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_r_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_s_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_S_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_s_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_s_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_S_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_s_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_s_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_s = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_S_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_s",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_s_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_t_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_T_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_t_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
	},
};

static struct clk_regmap pwm_t_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_T_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_t_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_t_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_t = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_T_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_t",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_t_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data spisg_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
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

static const struct clk_parent_data emmc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL0,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = emmc_parent_data,
		.num_parents = ARRAY_SIZE(emmc_parent_data),
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
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = emmc_parent_data,
		.num_parents = ARRAY_SIZE(emmc_parent_data),
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
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = emmc_parent_data,
		.num_parents = ARRAY_SIZE(emmc_parent_data),
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
			&fclk_div2.hw,
		},
		.num_parents = 1,
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

static struct clk_fixed_factor eth_125M_div = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data) {
		.name = "eth_125M_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap eth_125M = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_125M",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_125M_div.hw,
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

static u32 can_pe_parent_table[] = { 0, 1, 3 };
static const struct clk_parent_data can_pe_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div5.hw }
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

static const struct clk_parent_data can_filter_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw }
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
		.parent_data = can_filter_parent_data,
		.num_parents = ARRAY_SIZE(can_filter_parent_data),
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

static const struct clk_parent_data can_fliter_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw }
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
		.parent_data = can_fliter_parent_data,
		.num_parents = ARRAY_SIZE(can_fliter_parent_data),
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
	{ .hw = &fclk_div5.hw }
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

static const struct clk_parent_data usb_48m_parent_data[] = {
	{ .hw = &gp0_pll.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div2.hw }
};

static struct clk_regmap usb_48m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = usb_48m_parent_data,
		.num_parents = ARRAY_SIZE(usb_48m_parent_data),
	},
};

static struct clk_regmap usb_48m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap usb_48m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_USB_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb_48m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&usb_48m_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 usb_250m_parent_table[] = { 0, 1, 2, 3, 4, 7 };
static const struct clk_parent_data usb_250m_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw }
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

static const struct clk_parent_data pcie0_tl_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div2p5.hw },
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
		.parent_data = pcie0_tl_parent_data,
		.num_parents = ARRAY_SIZE(pcie0_tl_parent_data),
	},
};

static struct clk_regmap pcie0_tl_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.shift = 0,
		.width = 8,
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

static const struct clk_parent_data amfc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
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

static const struct clk_parent_data cmpr_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw }
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

static const struct clk_parent_data cve_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &rtc_clk.hw }
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
		.parent_data = cve_parent_data,
		.num_parents = ARRAY_SIZE(cve_parent_data),
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
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw }
};

static struct clk_regmap dewarpa_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.mask = 0x3,
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

static const struct clk_parent_data nna_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div7.hw },
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
		.parent_data = nna_parent_data,
		.num_parents = ARRAY_SIZE(nna_parent_data),
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

static struct clk_regmap nna2_core_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna2_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = nna_parent_data,
		.num_parents = ARRAY_SIZE(nna_parent_data),
	},
};

static struct clk_regmap nna2_core_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna2_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna2_core_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap nna2_core = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna2_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna2_core_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 vapb_01_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data vapb_01_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw }
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
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

static const struct clk_parent_data isp_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap isp0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = isp_parent_data,
		.num_parents = ARRAY_SIZE(isp_parent_data),
	},
};

static struct clk_regmap isp0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&isp0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap isp0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "isp0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&isp0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ai_isp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ai_isp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = isp_parent_data,
		.num_parents = ARRAY_SIZE(isp_parent_data),
	},
};

static struct clk_regmap ai_isp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ai_isp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ai_isp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ai_isp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ai_isp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ai_isp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 glb_parent_table[] = { 0, 1, 2, 3, 4, 5, 7 };
static const struct clk_parent_data glb_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &isp0.hw },
	{ .hw = &rtc_clk.hw }
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

static struct clk_regmap csiphy0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = isp_parent_data,
		.num_parents = ARRAY_SIZE(isp_parent_data),
	},
};

static struct clk_regmap csiphy0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csiphy0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap csiphy0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csiphy0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csiphy0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data hevcf_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw },
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data hcodec_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data vout_mclk_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap vout_mclk_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_mclk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vout_mclk_parent_data,
		.num_parents = ARRAY_SIZE(vout_mclk_parent_data),
	},
};

static struct clk_regmap vout_mclk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_mclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vout_mclk_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vout_mclk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_mclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vout_mclk_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vout_venc_mclk_parent_data[] = {
	{ .hw = &dsi_pll.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap vout_venc_mclk_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_venc_mclk_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vout_venc_mclk_parent_data,
		.num_parents = ARRAY_SIZE(vout_venc_mclk_parent_data),
	},
};

static struct clk_regmap vout_venc_mclk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_venc_mclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vout_venc_mclk_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vout_venc_mclk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VOUTENC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vout_venc_mclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vout_venc_mclk_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dsi_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &dsi_pll.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw }
};

static struct clk_regmap dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSI_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static struct clk_regmap clk_24m_in = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "clk_24m_in",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor clk_12m_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "clk_12m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&clk_24m_in.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data clk_12_24m_parent_data[] = {
	{ .hw = &clk_24m_in.hw },
	{ .hw = &clk_12m_div.hw }
};

static struct clk_regmap clk_12_24m = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CLK12_24_CTRL,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "clk_12_24m",
		.ops = &clk_regmap_mux_ops,
		.parent_data = clk_12_24m_parent_data,
		.num_parents = ARRAY_SIZE(clk_12_24m_parent_data),
	},
};

static u32 gen_parent_table[] = { 0, 1, 2, 5, 7, 14, 17, 19, 20, 21, 22,
				 23, 24 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &rtc_clk.hw },
	{ .fw_name = "sys_pll_div16" },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &mclk_pll.hw },
	{ .fw_name = "cpu_clk_div16" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
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
struct clk_regmap _name = {                                           \
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

MESON_CLK_GATE_SYS_CLK(sys_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
MESON_CLK_GATE_SYS_CLK(sys_tahoe, CLKCTRL_SYS_CLK_EN0_REG0, 2);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi, CLKCTRL_SYS_CLK_EN0_REG0, 3);
MESON_CLK_GATE_SYS_CLK(sys_eth_phy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_amfc, CLKCTRL_SYS_CLK_EN0_REG0, 5);
MESON_CLK_GATE_SYS_CLK(sys_nna, CLKCTRL_SYS_CLK_EN0_REG0, 7);
MESON_CLK_GATE_SYS_CLK(sys_pcie, CLKCTRL_SYS_CLK_EN0_REG0, 8);
MESON_CLK_GATE_SYS_CLK(sys_pciemac, CLKCTRL_SYS_CLK_EN0_REG0, 9);
MESON_CLK_GATE_SYS_CLK(sys_glb, CLKCTRL_SYS_CLK_EN0_REG0, 19);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi_phy, CLKCTRL_SYS_CLK_EN0_REG0, 20);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_a, CLKCTRL_SYS_CLK_EN0_REG0, 24);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_b, CLKCTRL_SYS_CLK_EN0_REG0, 25);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_c, CLKCTRL_SYS_CLK_EN0_REG0, 26);
MESON_CLK_GATE_SYS_CLK(sys_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
MESON_CLK_GATE_SYS_CLK(sys_mipi_isp, CLKCTRL_SYS_CLK_EN0_REG0, 29);
MESON_CLK_GATE_SYS_CLK(sys_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
MESON_CLK_GATE_SYS_CLK(sys_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
MESON_CLK_GATE_SYS_CLK(sys_eth_mac, CLKCTRL_SYS_CLK_EN0_REG1, 4);
MESON_CLK_GATE_SYS_CLK(sys_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
MESON_CLK_GATE_SYS_CLK(sys_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
MESON_CLK_GATE_SYS_CLK(sys_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
MESON_CLK_GATE_SYS_CLK(sys_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
MESON_CLK_GATE_SYS_CLK(sys_uart_f, CLKCTRL_SYS_CLK_EN0_REG1, 10);
MESON_CLK_GATE_SYS_CLK(sys_eth_axi, CLKCTRL_SYS_CLK_EN0_REG1, 12);
MESON_CLK_GATE_SYS_CLK(sys_ts_isp, CLKCTRL_SYS_CLK_EN0_REG1, 13);
MESON_CLK_GATE_SYS_CLK(sys_ts_nna, CLKCTRL_SYS_CLK_EN0_REG1, 15);
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_SYS_CLK(sys_pwm_m, CLKCTRL_SYS_CLK_EN0_REG1, 17);
MESON_CLK_GATE_SYS_CLK(sys_pwm_n, CLKCTRL_SYS_CLK_EN0_REG1, 18);
MESON_CLK_GATE_SYS_CLK(sys_cve, CLKCTRL_SYS_CLK_EN0_REG1, 19);
MESON_CLK_GATE_SYS_CLK(sys_ge2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_spisg0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
MESON_CLK_GATE_SYS_CLK(sys_usb_u2h, CLKCTRL_SYS_CLK_EN0_REG1, 22);
MESON_CLK_GATE_SYS_CLK(sys_usb_u3drd, CLKCTRL_SYS_CLK_EN0_REG1, 23);
MESON_CLK_GATE_SYS_CLK(sys_periph, CLKCTRL_SYS_CLK_EN0_REG1, 24);
MESON_CLK_GATE_SYS_CLK(sys_pwm_h, CLKCTRL_SYS_CLK_EN0_REG1, 25);
MESON_CLK_GATE_SYS_CLK(sys_pwm_i, CLKCTRL_SYS_CLK_EN0_REG1, 26);
MESON_CLK_GATE_SYS_CLK(sys_pwm_j, CLKCTRL_SYS_CLK_EN0_REG1, 27);
MESON_CLK_GATE_SYS_CLK(sys_pwm_k, CLKCTRL_SYS_CLK_EN0_REG1, 28);
MESON_CLK_GATE_SYS_CLK(sys_pwm_l, CLKCTRL_SYS_CLK_EN0_REG1, 29);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_g, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_SYS_CLK(sys_pio, CLKCTRL_SYS_CLK_EN0_REG2, 5);
MESON_CLK_GATE_SYS_CLK(sys_pwm_g, CLKCTRL_SYS_CLK_EN0_REG2, 6);
MESON_CLK_GATE_SYS_CLK(sys_pwm_f, CLKCTRL_SYS_CLK_EN0_REG2, 7);
MESON_CLK_GATE_SYS_CLK(sys_pwm_e, CLKCTRL_SYS_CLK_EN0_REG2, 8);
MESON_CLK_GATE_SYS_CLK(sys_pwm_d, CLKCTRL_SYS_CLK_EN0_REG2, 9);
MESON_CLK_GATE_SYS_CLK(sys_pwm_c, CLKCTRL_SYS_CLK_EN0_REG2, 10);
MESON_CLK_GATE_SYS_CLK(sys_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 13);
MESON_CLK_GATE_SYS_CLK(sys_cpu_apb, CLKCTRL_SYS_CLK_EN0_REG2, 14);
MESON_CLK_GATE_SYS_CLK(sys_pp_dma, CLKCTRL_SYS_CLK_EN0_REG2, 15);
MESON_CLK_GATE_SYS_CLK(sys_pp_wrapper, CLKCTRL_SYS_CLK_EN0_REG2, 16);
MESON_CLK_GATE_SYS_CLK(sys_csi2_phy0_apb, CLKCTRL_SYS_CLK_EN0_REG2, 17);
MESON_CLK_GATE_SYS_CLK(sys_aux_adc, CLKCTRL_SYS_CLK_EN0_REG2, 18);
MESON_CLK_GATE_SYS_CLK(sys_gic, CLKCTRL_SYS_CLK_EN0_REG2, 19);
MESON_CLK_GATE_SYS_CLK(sys_pwm_t, CLKCTRL_SYS_CLK_EN0_REG2, 20);
MESON_CLK_GATE_SYS_CLK(sys_pwm_s, CLKCTRL_SYS_CLK_EN0_REG2, 21);
MESON_CLK_GATE_SYS_CLK(sys_pwm_r, CLKCTRL_SYS_CLK_EN0_REG2, 22);
MESON_CLK_GATE_SYS_CLK(sys_pwm_q, CLKCTRL_SYS_CLK_EN0_REG2, 23);
MESON_CLK_GATE_SYS_CLK(sys_pwm_p, CLKCTRL_SYS_CLK_EN0_REG2, 24);
MESON_CLK_GATE_SYS_CLK(sys_pwm_o, CLKCTRL_SYS_CLK_EN0_REG2, 25);
MESON_CLK_GATE_SYS_CLK(sys_can0, CLKCTRL_SYS_CLK_EN0_REG2, 26);
MESON_CLK_GATE_SYS_CLK(sys_can1, CLKCTRL_SYS_CLK_EN0_REG2, 27);
MESON_CLK_GATE_SYS_CLK(sys_i3c, CLKCTRL_SYS_CLK_EN0_REG2, 28);

#define MESON_CLK_GATE_AXI_CLK(_name, _reg, _bit)                     \
struct clk_regmap _name = {                                           \
	.data = &(struct clk_regmap_gate_data) {                      \
		.offset = (_reg),                                     \
		.bit_idx = (_bit),                                    \
	},                                                            \
	.hw.init = &(struct clk_init_data) {                          \
		.name = #_name,                                       \
		.ops = &clk_regmap_gate_ops,                          \
		.parent_data = &(const struct clk_parent_data) {      \
			.fw_name = "axi_clk"                          \
		},                                                    \
		.num_parents = 1,                                     \
		.flags = CLK_IGNORE_UNUSED,                           \
	},                                                            \
}

MESON_CLK_GATE_AXI_CLK(axi_sys_nic, CLKCTRL_AXI_CLK_EN0, 0);
MESON_CLK_GATE_AXI_CLK(axi_rama, CLKCTRL_AXI_CLK_EN0, 1);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&dsi_pll,
	&mclk_pll,
	&mclk1_pll,
	&fclk_div50m,
	&fclk_div2,
	&fclk_div2p5,
	&fclk_div3,
	&fclk_div4,
	&fclk_div5,
	&fclk_div7,
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
	&rtc_dual_clkin,
	&rtc_dual_div,
	&rtc_dual_mux,
	&rtc_dual,
	&rtc_clk,
	&sar_adc_mux,
	&sar_adc_div,
	&sar_adc,
	&pwm_c_mux,
	&pwm_c_div,
	&pwm_c,
	&pwm_d_mux,
	&pwm_d_div,
	&pwm_d,
	&pwm_e_mux,
	&pwm_e_div,
	&pwm_e,
	&pwm_f_mux,
	&pwm_f_div,
	&pwm_f,
	&pwm_g_mux,
	&pwm_g_div,
	&pwm_g,
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
	&pwm_o_mux,
	&pwm_o_div,
	&pwm_o,
	&pwm_p_mux,
	&pwm_p_div,
	&pwm_p,
	&pwm_q_mux,
	&pwm_q_div,
	&pwm_q,
	&pwm_r_mux,
	&pwm_r_div,
	&pwm_r,
	&pwm_s_mux,
	&pwm_s_div,
	&pwm_s,
	&pwm_t_mux,
	&pwm_t_div,
	&pwm_t,
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
	&eth_rmii_div,
	&eth_rmii,
	&eth_125M,
	&ts_div,
	&ts,
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
	&usb_48m_mux,
	&usb_48m_div,
	&usb_48m,
	&usb_250m_mux,
	&usb_250m_div,
	&usb_250m,
	&pcie0_tl_mux,
	&pcie0_tl_div,
	&pcie0_tl,
	&amfc_mux,
	&amfc_div,
	&amfc,
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
	&nna_core_mux,
	&nna_core_div,
	&nna_core,
	&nna2_core_mux,
	&nna2_core_div,
	&nna2_core,
	&vapb_0_mux,
	&vapb_0_div,
	&vapb_0,
	&vapb_1_mux,
	&vapb_1_div,
	&vapb_1,
	&vapb,
	&ge2d,
	&isp0_mux,
	&isp0_div,
	&isp0,
	&ai_isp_mux,
	&ai_isp_div,
	&ai_isp,
	&glb_mux,
	&glb_div,
	&glb,
	&csiphy0_mux,
	&csiphy0_div,
	&csiphy0,
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
	&vout_mclk_mux,
	&vout_mclk_div,
	&vout_mclk,
	&vout_venc_mclk_mux,
	&vout_venc_mclk_div,
	&vout_venc_mclk,
	&dsi_meas_mux,
	&dsi_meas_div,
	&dsi_meas,
	&clk_24m_in,
	&clk_12_24m,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_dos,
	&sys_tahoe,
	&sys_mipi_dsi,
	&sys_eth_phy,
	&sys_amfc,
	&sys_nna,
	&sys_pcie,
	&sys_pciemac,
	&sys_glb,
	&sys_mipi_dsi_phy,
	&sys_sd_emmc_a,
	&sys_sd_emmc_b,
	&sys_sd_emmc_c,
	&sys_acodec,
	&sys_mipi_isp,
	&sys_msr_clk,
	&sys_audio,
	&sys_eth_mac,
	&sys_uart_b,
	&sys_uart_c,
	&sys_uart_d,
	&sys_uart_e,
	&sys_uart_f,
	&sys_eth_axi,
	&sys_ts_isp,
	&sys_ts_nna,
	&sys_ts_pll,
	&sys_pwm_m,
	&sys_pwm_n,
	&sys_cve,
	&sys_ge2d,
	&sys_spisg0,
	&sys_usb_u2h,
	&sys_usb_u3drd,
	&sys_periph,
	&sys_pwm_h,
	&sys_pwm_i,
	&sys_pwm_j,
	&sys_pwm_k,
	&sys_pwm_l,
	&sys_i2c_m_c,
	&sys_i2c_m_d,
	&sys_i2c_m_e,
	&sys_i2c_m_f,
	&sys_i2c_m_g,
	&sys_pio,
	&sys_pwm_g,
	&sys_pwm_f,
	&sys_pwm_e,
	&sys_pwm_d,
	&sys_pwm_c,
	&sys_mmc_apb,
	&sys_cpu_apb,
	&sys_pp_dma,
	&sys_pp_wrapper,
	&sys_csi2_phy0_apb,
	&sys_aux_adc,
	&sys_gic,
	&sys_pwm_t,
	&sys_pwm_s,
	&sys_pwm_r,
	&sys_pwm_q,
	&sys_pwm_p,
	&sys_pwm_o,
	&sys_can0,
	&sys_can1,
	&sys_i3c,
	&axi_sys_nic,
	&axi_rama
};

static struct clk_hw *c5_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_DSI_PLL]               = &dsi_pll.hw,
	[CLKID_MCLK_PLL]              = &mclk_pll.hw,
	[CLKID_MCLK1_PLL]             = &mclk1_pll.hw,
	[CLKID_FCLK_DIV50M_DIV]       = &fclk_div50m_div.hw,
	[CLKID_FCLK_DIV50M]           = &fclk_div50m.hw,
	[CLKID_FCLK_DIV2_DIV]         = &fclk_div2_div.hw,
	[CLKID_FCLK_DIV2]             = &fclk_div2.hw,
	[CLKID_FCLK_DIV2P5_DIV]       = &fclk_div2p5_div.hw,
	[CLKID_FCLK_DIV2P5]           = &fclk_div2p5.hw,
	[CLKID_FCLK_DIV3_DIV]         = &fclk_div3_div.hw,
	[CLKID_FCLK_DIV3]             = &fclk_div3.hw,
	[CLKID_FCLK_DIV4_DIV]         = &fclk_div4_div.hw,
	[CLKID_FCLK_DIV4]             = &fclk_div4.hw,
	[CLKID_FCLK_DIV5_DIV]         = &fclk_div5_div.hw,
	[CLKID_FCLK_DIV5]             = &fclk_div5.hw,
	[CLKID_FCLK_DIV7_DIV]         = &fclk_div7_div.hw,
	[CLKID_FCLK_DIV7]             = &fclk_div7.hw,
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
	[CLKID_RTC_DUAL_CLKIN]        = &rtc_dual_clkin.hw,
	[CLKID_RTC_DUAL_DIV]          = &rtc_dual_div.hw,
	[CLKID_RTC_DUAL_MUX]          = &rtc_dual_mux.hw,
	[CLKID_RTC_DUAL]              = &rtc_dual.hw,
	[CLKID_RTC_CLK]               = &rtc_clk.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_PWM_C_MUX]             = &pwm_c_mux.hw,
	[CLKID_PWM_C_DIV]             = &pwm_c_div.hw,
	[CLKID_PWM_C]                 = &pwm_c.hw,
	[CLKID_PWM_D_MUX]             = &pwm_d_mux.hw,
	[CLKID_PWM_D_DIV]             = &pwm_d_div.hw,
	[CLKID_PWM_D]                 = &pwm_d.hw,
	[CLKID_PWM_E_MUX]             = &pwm_e_mux.hw,
	[CLKID_PWM_E_DIV]             = &pwm_e_div.hw,
	[CLKID_PWM_E]                 = &pwm_e.hw,
	[CLKID_PWM_F_MUX]             = &pwm_f_mux.hw,
	[CLKID_PWM_F_DIV]             = &pwm_f_div.hw,
	[CLKID_PWM_F]                 = &pwm_f.hw,
	[CLKID_PWM_G_MUX]             = &pwm_g_mux.hw,
	[CLKID_PWM_G_DIV]             = &pwm_g_div.hw,
	[CLKID_PWM_G]                 = &pwm_g.hw,
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
	[CLKID_PWM_O_MUX]             = &pwm_o_mux.hw,
	[CLKID_PWM_O_DIV]             = &pwm_o_div.hw,
	[CLKID_PWM_O]                 = &pwm_o.hw,
	[CLKID_PWM_P_MUX]             = &pwm_p_mux.hw,
	[CLKID_PWM_P_DIV]             = &pwm_p_div.hw,
	[CLKID_PWM_P]                 = &pwm_p.hw,
	[CLKID_PWM_Q_MUX]             = &pwm_q_mux.hw,
	[CLKID_PWM_Q_DIV]             = &pwm_q_div.hw,
	[CLKID_PWM_Q]                 = &pwm_q.hw,
	[CLKID_PWM_R_MUX]             = &pwm_r_mux.hw,
	[CLKID_PWM_R_DIV]             = &pwm_r_div.hw,
	[CLKID_PWM_R]                 = &pwm_r.hw,
	[CLKID_PWM_S_MUX]             = &pwm_s_mux.hw,
	[CLKID_PWM_S_DIV]             = &pwm_s_div.hw,
	[CLKID_PWM_S]                 = &pwm_s.hw,
	[CLKID_PWM_T_MUX]             = &pwm_t_mux.hw,
	[CLKID_PWM_T_DIV]             = &pwm_t_div.hw,
	[CLKID_PWM_T]                 = &pwm_t.hw,
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
	[CLKID_ETH_RMII_DIV]          = &eth_rmii_div.hw,
	[CLKID_ETH_RMII]              = &eth_rmii.hw,
	[CLKID_ETH_125M_DIV]          = &eth_125M_div.hw,
	[CLKID_ETH_125M]              = &eth_125M.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
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
	[CLKID_USB_48M_MUX]           = &usb_48m_mux.hw,
	[CLKID_USB_48M_DIV]           = &usb_48m_div.hw,
	[CLKID_USB_48M]               = &usb_48m.hw,
	[CLKID_USB_250M_MUX]          = &usb_250m_mux.hw,
	[CLKID_USB_250M_DIV]          = &usb_250m_div.hw,
	[CLKID_USB_250M]              = &usb_250m.hw,
	[CLKID_PCIE0_TL_MUX]          = &pcie0_tl_mux.hw,
	[CLKID_PCIE0_TL_DIV]          = &pcie0_tl_div.hw,
	[CLKID_PCIE0_TL]              = &pcie0_tl.hw,
	[CLKID_AMFC_MUX]              = &amfc_mux.hw,
	[CLKID_AMFC_DIV]              = &amfc_div.hw,
	[CLKID_AMFC]                  = &amfc.hw,
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
	[CLKID_NNA_CORE_MUX]          = &nna_core_mux.hw,
	[CLKID_NNA_CORE_DIV]          = &nna_core_div.hw,
	[CLKID_NNA_CORE]              = &nna_core.hw,
	[CLKID_NNA2_CORE_MUX]         = &nna2_core_mux.hw,
	[CLKID_NNA2_CORE_DIV]         = &nna2_core_div.hw,
	[CLKID_NNA2_CORE]             = &nna2_core.hw,
	[CLKID_VAPB_0_MUX]            = &vapb_0_mux.hw,
	[CLKID_VAPB_0_DIV]            = &vapb_0_div.hw,
	[CLKID_VAPB_0]                = &vapb_0.hw,
	[CLKID_VAPB_1_MUX]            = &vapb_1_mux.hw,
	[CLKID_VAPB_1_DIV]            = &vapb_1_div.hw,
	[CLKID_VAPB_1]                = &vapb_1.hw,
	[CLKID_VAPB]                  = &vapb.hw,
	[CLKID_GE2D]                  = &ge2d.hw,
	[CLKID_ISP0_MUX]              = &isp0_mux.hw,
	[CLKID_ISP0_DIV]              = &isp0_div.hw,
	[CLKID_ISP0]                  = &isp0.hw,
	[CLKID_AI_ISP_MUX]            = &ai_isp_mux.hw,
	[CLKID_AI_ISP_DIV]            = &ai_isp_div.hw,
	[CLKID_AI_ISP]                = &ai_isp.hw,
	[CLKID_GLB_MUX]               = &glb_mux.hw,
	[CLKID_GLB_DIV]               = &glb_div.hw,
	[CLKID_GLB]                   = &glb.hw,
	[CLKID_CSIPHY0_MUX]           = &csiphy0_mux.hw,
	[CLKID_CSIPHY0_DIV]           = &csiphy0_div.hw,
	[CLKID_CSIPHY0]               = &csiphy0.hw,
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
	[CLKID_VOUT_MCLK_MUX]         = &vout_mclk_mux.hw,
	[CLKID_VOUT_MCLK_DIV]         = &vout_mclk_div.hw,
	[CLKID_VOUT_MCLK]             = &vout_mclk.hw,
	[CLKID_VOUT_VENC_MCLK_MUX]    = &vout_venc_mclk_mux.hw,
	[CLKID_VOUT_VENC_MCLK_DIV]    = &vout_venc_mclk_div.hw,
	[CLKID_VOUT_VENC_MCLK]        = &vout_venc_mclk.hw,
	[CLKID_DSI_MEAS_MUX]          = &dsi_meas_mux.hw,
	[CLKID_DSI_MEAS_DIV]          = &dsi_meas_div.hw,
	[CLKID_DSI_MEAS]              = &dsi_meas.hw,
	[CLKID_CLK_24M_IN]            = &clk_24m_in.hw,
	[CLKID_CLK_12M_DIV]           = &clk_12m_div.hw,
	[CLKID_CLK_12_24M]            = &clk_12_24m.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_TAHOE]             = &sys_tahoe.hw,
	[CLKID_SYS_MIPI_DSI]          = &sys_mipi_dsi.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_AMFC]              = &sys_amfc.hw,
	[CLKID_SYS_NNA]               = &sys_nna.hw,
	[CLKID_SYS_PCIE]              = &sys_pcie.hw,
	[CLKID_SYS_PCIEMAC]           = &sys_pciemac.hw,
	[CLKID_SYS_GLB]               = &sys_glb.hw,
	[CLKID_SYS_MIPI_DSI_PHY]      = &sys_mipi_dsi_phy.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_MIPI_ISP]          = &sys_mipi_isp.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_ETH_MAC]           = &sys_eth_mac.hw,
	[CLKID_SYS_UART_B]            = &sys_uart_b.hw,
	[CLKID_SYS_UART_C]            = &sys_uart_c.hw,
	[CLKID_SYS_UART_D]            = &sys_uart_d.hw,
	[CLKID_SYS_UART_E]            = &sys_uart_e.hw,
	[CLKID_SYS_UART_F]            = &sys_uart_f.hw,
	[CLKID_SYS_ETH_AXI]           = &sys_eth_axi.hw,
	[CLKID_SYS_TS_ISP]            = &sys_ts_isp.hw,
	[CLKID_SYS_TS_NNA]            = &sys_ts_nna.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_PWM_M]             = &sys_pwm_m.hw,
	[CLKID_SYS_PWM_N]             = &sys_pwm_n.hw,
	[CLKID_SYS_CVE]               = &sys_cve.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPISG0]            = &sys_spisg0.hw,
	[CLKID_SYS_USB_U2H]           = &sys_usb_u2h.hw,
	[CLKID_SYS_USB_U3DRD]         = &sys_usb_u3drd.hw,
	[CLKID_SYS_PERIPH]            = &sys_periph.hw,
	[CLKID_SYS_PWM_H]             = &sys_pwm_h.hw,
	[CLKID_SYS_PWM_I]             = &sys_pwm_i.hw,
	[CLKID_SYS_PWM_J]             = &sys_pwm_j.hw,
	[CLKID_SYS_PWM_K]             = &sys_pwm_k.hw,
	[CLKID_SYS_PWM_L]             = &sys_pwm_l.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_I2C_M_F]           = &sys_i2c_m_f.hw,
	[CLKID_SYS_I2C_M_G]           = &sys_i2c_m_g.hw,
	[CLKID_SYS_PIO]               = &sys_pio.hw,
	[CLKID_SYS_PWM_G]             = &sys_pwm_g.hw,
	[CLKID_SYS_PWM_F]             = &sys_pwm_f.hw,
	[CLKID_SYS_PWM_E]             = &sys_pwm_e.hw,
	[CLKID_SYS_PWM_D]             = &sys_pwm_d.hw,
	[CLKID_SYS_PWM_C]             = &sys_pwm_c.hw,
	[CLKID_SYS_MMC_APB]           = &sys_mmc_apb.hw,
	[CLKID_SYS_CPU_APB]           = &sys_cpu_apb.hw,
	[CLKID_SYS_PP_DMA]            = &sys_pp_dma.hw,
	[CLKID_SYS_PP_WRAPPER]        = &sys_pp_wrapper.hw,
	[CLKID_SYS_CSI2_PHY0_APB]     = &sys_csi2_phy0_apb.hw,
	[CLKID_SYS_AUX_ADC]           = &sys_aux_adc.hw,
	[CLKID_SYS_GIC]               = &sys_gic.hw,
	[CLKID_SYS_PWM_T]             = &sys_pwm_t.hw,
	[CLKID_SYS_PWM_S]             = &sys_pwm_s.hw,
	[CLKID_SYS_PWM_R]             = &sys_pwm_r.hw,
	[CLKID_SYS_PWM_Q]             = &sys_pwm_q.hw,
	[CLKID_SYS_PWM_P]             = &sys_pwm_p.hw,
	[CLKID_SYS_PWM_O]             = &sys_pwm_o.hw,
	[CLKID_SYS_CAN0]              = &sys_can0.hw,
	[CLKID_SYS_CAN1]              = &sys_can1.hw,
	[CLKID_SYS_I3C]               = &sys_i3c.hw,
	[CLKID_AXI_SYS_NIC]           = &axi_sys_nic.hw,
	[CLKID_AXI_RAMA]              = &axi_rama.hw
};

static const struct meson_clk_hw_data c5_clks = {
	.hws = c5_hw_clks,
	.num = ARRAY_SIZE(c5_hw_clks),
};

static int meson_c5_probe(struct platform_device *pdev)
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
		.compatible = "amlogic,c5-clkc",
		.data = &c5_clks
	},
	{}
};

static struct platform_driver c5_driver = {
	.probe			= meson_c5_probe,
	.driver			= {
		.name		= "c5-clkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init c5_clk_init(void)
{
	return platform_driver_register(&c5_driver);
}

void __exit c5_clk_exit(void)
{
	platform_driver_unregister(&c5_driver);
}
#else
builtin_platform_driver(c5_driver);
MODULE_LICENSE("GPL v2");
#endif
