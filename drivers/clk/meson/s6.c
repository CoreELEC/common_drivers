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
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "clk-dualdiv.h"
#include "s6.h"
#include <dt-bindings/clock/amlogic,s6-clkc.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(192, 0, 1), /* DCO = 2304M OD = 2 PLL = 1152M */
	{ /* sentinel */ }
};

static const struct reg_sequence gp0_pll_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL0, .def = 0x01010000 },
	{ .reg = ANACTRL_GP0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2, .def = 0x1215b010 },
	{ .reg = ANACTRL_GP0PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_GP0PLL_CTRL4, .def = 0x00001000 }
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
			.width   = 19,
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
			.reg_off = ANACTRL_GP0PLL_CTRL3,
			.shift   = 29,
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
			.fw_name = "gp0_pll_src",
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
	{ .reg = ANACTRL_HIFI0PLL_CTRL0, .def = 0x01010000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL2, .def = 0x121db010 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL4, .def = 0x00001000 }
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
			.width   = 19,
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
			.reg_off = ANACTRL_HIFI0PLL_CTRL3,
			.shift   = 29,
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
			.fw_name = "hifi_pll_src",
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
	{ .reg = ANACTRL_HIFI1PLL_CTRL0, .def = 0x01010000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL2, .def = 0x121db010 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL4, .def = 0x00001000 }
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
			.width   = 19,
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
			.reg_off = ANACTRL_HIFI1PLL_CTRL3,
			.shift   = 29,
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
			.fw_name = "hifi1_pll_src",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table mclk_pll_table[] = {
	PLL_PARAMS_OD(198, 0, 1), /* VCO = 2376M, CLK_OUT = 1188M */
	PLL_PARAMS_OD(200, 0, 1), /* VCO = 2400M, CLK_OUT = 1200M */
	{ /* sentinel */ }
};

static const struct reg_sequence mclk_pll_init_regs[] = {
	{ .reg = ANACTRL_CSIPLL_CTRL1, .def = 0x20627000 },
	{ .reg = ANACTRL_CSIPLL_CTRL2, .def = 0x60000201 },
	{ .reg = ANACTRL_CSIPLL_CTRL3, .def = 0x03180260 }
};

static struct reg_sequence mclk_pll_save_regs[] = {
	{ .reg = ANACTRL_CSIPLL_CTRL1 },
	{ .reg = ANACTRL_CSIPLL_CTRL3 }
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
			.shift   = 16,
			.width   = 3,
		},
		.od = {
			.reg_off = ANACTRL_CSIPLL_CTRL1,
			.shift   = 0,
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
			.reg_off = ANACTRL_CSIPLL_CTRL1,
			.shift   = 29,
			.width   = 1,
		},
		.table = mclk_pll_table,
		.init_regs = mclk_pll_init_regs,
		.init_count = ARRAY_SIZE(mclk_pll_init_regs),
		.save_regs = mclk_pll_save_regs,
		.save_reg_count = ARRAY_SIZE(mclk_pll_save_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_RSTN |
			 CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "mclk_pll_src",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mclk_pll_clk = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_CSIPLL_CTRL1,
		.shift = 3,
		.width = 5,
		.flags = CLK_DIVIDER_MAX_AT_ZERO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk_pll_clk",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_pll.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk0_div = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0_div",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk_pll_clk.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor mclk0_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_CSIPLL_CTRL3,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk0_div2.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor fclk50m_div = {
	.mult = 1,
	.div = 40,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk50m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk50m",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk50m_div.hw,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 20,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 8,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 16,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 17,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 18,
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
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 19,
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
		.offset = CLKCTRL_RTC_CTRL,
		.mask = 0x3,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rtc_clk_parent_data,
		.num_parents = ARRAY_SIZE(rtc_clk_parent_data),
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

static const struct clk_parent_data clk_12m_parent_data[] = {
	{ .hw = &clk_24m_in.hw },
	{ .hw = &clk_12m_div.hw }
};

static struct clk_regmap clk_12m = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CLK12_24_CTRL,
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "clk_12m",
		.ops = &clk_regmap_mux_ops,
		.parent_data = clk_12m_parent_data,
		.num_parents = ARRAY_SIZE(clk_12m_parent_data),
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

static struct clk_regmap cecb_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param cecb_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap cecb_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 28,
			.width   = 2,
		},
		.table = cecb_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cecb_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data cecb_dual_parent_data[] = {
	{ .hw = &cecb_dual_div.hw },
	{ .hw = &cecb_dual_clkin.hw }
};

static struct clk_regmap cecb_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cecb_dual_parent_data,
		.num_parents = ARRAY_SIZE(cecb_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap cecb_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cecb_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data cecb_clk_parent_data[] = {
	{ .hw = &cecb_dual.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap cecb_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cecb_clk_parent_data,
		.num_parents = ARRAY_SIZE(cecb_clk_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sc_pre_parent_data[] = {
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
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

static const struct clk_parent_data cdac_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div5.hw }
};

static struct clk_regmap cdac_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cdac_parent_data,
		.num_parents = ARRAY_SIZE(cdac_parent_data),
	},
};

static struct clk_regmap cdac_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.shift = 0,
		.width = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cdac_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap cdac = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cdac_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 video_src_01_parent_table[] = { 1, 2, 4, 5, 6, 7 };
static const struct clk_parent_data video_src_01_parent_data[] = {
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
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

static struct clk_regmap video0_div2_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div2_gate",
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
			&video0_div2_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div4_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div4_gate",
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
			&video0_div4_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div6_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div6_gate",
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
			&video0_div6_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video0_div12_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video0_div12_gate",
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
			&video0_div12_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video2_div1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap video2_div2_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div2_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video2_div2_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video2_div4_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div4_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video2_div4_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video2_div6_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div6_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video2_div6_gate.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap video2_div12_gate = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VIID_CLK_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div12_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video_src1.hw,
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor video2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data) {
		.name = "video2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&video2_div12_gate.hw,
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
	{ .hw = &video2_div1.hw },
	{ .hw = &video2_div2.hw },
	{ .hw = &video2_div4.hw },
	{ .hw = &video2_div6.hw },
	{ .hw = &video2_div12.hw }
};

static struct clk_regmap enci_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enci_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap enci = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&enci_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap encp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 24,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "encp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap encp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&encp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap encl_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VIID_CLK_DIV,
		.mask = 0xf,
		.shift = 12,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "encl_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap encl = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "encl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&encl_mux.hw,
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

static const struct clk_parent_data mali_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
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

static const struct clk_parent_data mali_stack_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
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
		.parent_data = mali_stack_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_stack_01_parent_data),
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
		.parent_data = mali_stack_01_parent_data,
		.num_parents = ARRAY_SIZE(mali_stack_01_parent_data),
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

static const struct clk_parent_data vdec_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap vdec_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vdec_01_parent_data,
		.num_parents = ARRAY_SIZE(vdec_01_parent_data),
	},
};

static struct clk_regmap vdec_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdec_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vdec_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdec_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap vdec_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vdec_01_parent_data,
		.num_parents = ARRAY_SIZE(vdec_01_parent_data),
	},
};

static struct clk_regmap vdec_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdec_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vdec_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vdec_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data vdec_parent_data[] = {
	{ .hw = &vdec_0.hw },
	{ .hw = &vdec_1.hw }
};

static struct clk_regmap vdec = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdec",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vdec_parent_data,
		.num_parents = ARRAY_SIZE(vdec_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data hevcf_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap hevcf_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
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
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
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
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
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

static u32 vpu_01_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data vpu_01_parent_data[] = {
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &hifi_pll.hw },
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
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
};

static struct clk_regmap vpu_clkb_tmp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKB_CTRL,
		.mask = 0x3,
		.shift = 20,
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

static u32 vpu_clkc_01_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data vpu_clkc_01_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" }
};

static struct clk_regmap vpu_clkc_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = vpu_clkc_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_clkc_01_parent_data,
		.num_parents = ARRAY_SIZE(vpu_clkc_01_parent_data),
	},
};

static struct clk_regmap vpu_clkc_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkc_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_clkc_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkc_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap vpu_clkc_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = vpu_clkc_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_clkc_01_parent_data,
		.num_parents = ARRAY_SIZE(vpu_clkc_01_parent_data),
	},
};

static struct clk_regmap vpu_clkc_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkc_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vpu_clkc_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vpu_clkc_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data vpu_clkc_parent_data[] = {
	{ .hw = &vpu_clkc_0.hw },
	{ .hw = &vpu_clkc_1.hw }
};

static struct clk_regmap vpu_clkc = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_clkc",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vpu_clkc_parent_data,
		.num_parents = ARRAY_SIZE(vpu_clkc_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
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

static const struct clk_parent_data dewarpa_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
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

static const struct clk_parent_data cmpr_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
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

static const struct clk_parent_data hdmitx_sys_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
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
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
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
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
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
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
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

static u32 pwm_a_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_a_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = pwm_a_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_a_parent_data,
		.num_parents = ARRAY_SIZE(pwm_a_parent_data),
	},
};

static struct clk_regmap pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_a_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_a_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 pwm_b_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_b_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = pwm_b_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_b_parent_data,
		.num_parents = ARRAY_SIZE(pwm_b_parent_data),
	},
};

static struct clk_regmap pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_b_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pwm_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_b_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 pwm_g_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_g_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = pwm_g_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_g_parent_data,
		.num_parents = ARRAY_SIZE(pwm_g_parent_data),
	},
};

static struct clk_regmap pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
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

static u32 pwm_h_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_h_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = pwm_h_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_h_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_h_parent_data,
		.num_parents = ARRAY_SIZE(pwm_h_parent_data),
	},
};

static struct clk_regmap pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
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

static u32 pwm_i_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_i_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_i_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = pwm_i_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_i_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_i_parent_data,
		.num_parents = ARRAY_SIZE(pwm_i_parent_data),
	},
};

static struct clk_regmap pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
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

static u32 pwm_j_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_j_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = pwm_j_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_j_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_j_parent_data,
		.num_parents = ARRAY_SIZE(pwm_j_parent_data),
	},
};

static struct clk_regmap pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
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

static const struct clk_parent_data spicc0_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .fw_name = "gp1_pll" }
};

static struct clk_regmap spicc0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = spicc0_parent_data,
		.num_parents = ARRAY_SIZE(spicc0_parent_data),
	},
};

static struct clk_regmap spicc0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spicc0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spicc0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SPICC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spicc0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 sd_emmc_a_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sd_emmc_a_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = sd_emmc_a_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_a_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_a_parent_data),
	},
};

static struct clk_regmap sd_emmc_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
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
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
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

static u32 sd_emmc_b_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sd_emmc_b_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = sd_emmc_b_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_b_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_b_parent_data),
	},
};

static struct clk_regmap sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
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
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
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

static u32 sd_emmc_c_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sd_emmc_c_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = sd_emmc_c_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sd_emmc_c_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_c_parent_data),
	},
};

static struct clk_regmap sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
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
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
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
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
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
	{ .hw = &encl.hw },
	{ .hw = &enci.hw },
	{ .hw = &encp.hw }
};

static struct clk_regmap vid_lock_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.mask = 0x3,
		.shift = 8,
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
		.bit_idx = 7,
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

static const struct clk_parent_data eth_clk_rmii_parent_data[] = {
	{ .hw = &fclk_div2.hw }
};

static struct clk_regmap eth_clk_rmii_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk_rmii_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = eth_clk_rmii_parent_data,
		.num_parents = ARRAY_SIZE(eth_clk_rmii_parent_data),
	},
};

static struct clk_regmap eth_clk_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_clk_rmii_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap eth_clk_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_clk_rmii_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_fixed_factor eth_clk125m_div = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk125m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap eth_clk125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_clk125m_div.hw,
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
		.shift = 7,
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
		.bit_idx = 6,
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

static const struct clk_parent_data csi2_adapt_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap csi2_adapt_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CSI2_ADAPT_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi2_adapt_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = csi2_adapt_parent_data,
		.num_parents = ARRAY_SIZE(csi2_adapt_parent_data),
	},
};

static struct clk_regmap csi2_adapt_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CSI2_ADAPT_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi2_adapt_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csi2_adapt_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap csi2_adapt = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CSI2_ADAPT_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "csi2_adapt",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&csi2_adapt_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mipi_csi_phy_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw }
};

static struct clk_regmap mipi_csi_phy_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mipi_csi_phy_01_parent_data,
		.num_parents = ARRAY_SIZE(mipi_csi_phy_01_parent_data),
	},
};

static struct clk_regmap mipi_csi_phy_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_csi_phy_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mipi_csi_phy_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_csi_phy_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static struct clk_regmap mipi_csi_phy_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mipi_csi_phy_01_parent_data,
		.num_parents = ARRAY_SIZE(mipi_csi_phy_01_parent_data),
	},
};

static struct clk_regmap mipi_csi_phy_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_csi_phy_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mipi_csi_phy_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_csi_phy_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE,
	},
};

static const struct clk_parent_data mipi_csi_phy_parent_data[] = {
	{ .hw = &mipi_csi_phy_0.hw },
	{ .hw = &mipi_csi_phy_1.hw }
};

static struct clk_regmap mipi_csi_phy = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_MIPI_CSI_PHY_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_csi_phy",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mipi_csi_phy_parent_data,
		.num_parents = ARRAY_SIZE(mipi_csi_phy_parent_data),
		.flags = CLK_SET_RATE_PARENT |
			 CLK_OPS_PARENT_ENABLE,
	},
};

static u32 mipi_dsi_meas_parent_table[] = { 0, 1, 2, 3, 5, 6, 7 };
static const struct clk_parent_data mipi_dsi_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div7.hw }
};

static struct clk_regmap mipi_dsi_meas_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 21,
		.table = mipi_dsi_meas_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_dsi_meas_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mipi_dsi_meas_parent_data,
		.num_parents = ARRAY_SIZE(mipi_dsi_meas_parent_data),
	},
};

static struct clk_regmap mipi_dsi_meas_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 12,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_dsi_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_dsi_meas_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mipi_dsi_meas = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mipi_dsi_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mipi_dsi_meas_div.hw,
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
	{ .fw_name = "gp1_pll" },
	{ .hw = &hifi_pll.hw }
};

static struct clk_regmap nna_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = nna_parent_data,
		.num_parents = ARRAY_SIZE(nna_parent_data),
	},
};

static struct clk_regmap nna_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap nna = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_NNA_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "nna",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&nna_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vc9000e_axi_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap vc9000e_axi_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_axi_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vc9000e_axi_parent_data,
		.num_parents = ARRAY_SIZE(vc9000e_axi_parent_data),
	},
};

static struct clk_regmap vc9000e_axi_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_axi_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vc9000e_axi_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vc9000e_axi = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_axi",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vc9000e_axi_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data vc9000e_core_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap vc9000e_core_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vc9000e_core_parent_data,
		.num_parents = ARRAY_SIZE(vc9000e_core_parent_data),
	},
};

static struct clk_regmap vc9000e_core_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vc9000e_core_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap vc9000e_core = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VC9000E_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vc9000e_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vc9000e_core_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dspa_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .fw_name = "gp2_pll" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap dspa_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DSPA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
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
		.width = 10,
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
		.bit_idx = 13,
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
		.shift = 26,
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
		.width = 10,
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
		.bit_idx = 29,
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
		.shift = 15,
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

static const struct clk_parent_data pcie_tl_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "sys_clk" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap pcie_tl_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pcie_tl_parent_data,
		.num_parents = ARRAY_SIZE(pcie_tl_parent_data),
	},
};

static struct clk_regmap pcie_tl_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie_tl_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap pcie_tl = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PCIE_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pcie_tl",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pcie_tl_div.hw,
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

static u32 gen_parent_table[] = { 0, 1, 5, 6, 7, 8, 9, 19, 20, 21, 22, 23,
				 24, 29 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &rtc_clk.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi1_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "gp2_pll" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &mipi_csi_phy.hw }
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
MESON_CLK_GATE_SYS_CLK(sys_vc9000e, CLKCTRL_SYS_CLK_EN0_REG0, 2);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi, CLKCTRL_SYS_CLK_EN0_REG0, 3);
MESON_CLK_GATE_SYS_CLK(sys_eth_phy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_amfc, CLKCTRL_SYS_CLK_EN0_REG0, 5);
MESON_CLK_GATE_SYS_CLK(sys_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
MESON_CLK_GATE_SYS_CLK(sys_nna, CLKCTRL_SYS_CLK_EN0_REG0, 7);
MESON_CLK_GATE_SYS_CLK(sys_eth_mac, CLKCTRL_SYS_CLK_EN0_REG0, 8);
MESON_CLK_GATE_SYS_CLK(sys_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
MESON_CLK_GATE_SYS_CLK(sys_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
MESON_CLK_GATE_SYS_CLK(sys_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
MESON_CLK_GATE_SYS_CLK(sys_mipi_dsi_phy, CLKCTRL_SYS_CLK_EN0_REG0, 20);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_a, CLKCTRL_SYS_CLK_EN0_REG0, 24);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_b, CLKCTRL_SYS_CLK_EN0_REG0, 25);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_c, CLKCTRL_SYS_CLK_EN0_REG0, 26);
MESON_CLK_GATE_SYS_CLK(sys_smartcard, CLKCTRL_SYS_CLK_EN0_REG0, 27);
MESON_CLK_GATE_SYS_CLK(sys_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
MESON_CLK_GATE_SYS_CLK(sys_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
MESON_CLK_GATE_SYS_CLK(sys_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 31);
MESON_CLK_GATE_SYS_CLK(sys_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
MESON_CLK_GATE_SYS_CLK(sys_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
MESON_CLK_GATE_SYS_CLK(sys_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
MESON_CLK_GATE_SYS_CLK(sys_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
MESON_CLK_GATE_SYS_CLK(sys_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
MESON_CLK_GATE_SYS_CLK(sys_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
MESON_CLK_GATE_SYS_CLK(sys_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
MESON_CLK_GATE_SYS_CLK(sys_ts_core, CLKCTRL_SYS_CLK_EN0_REG1, 15);
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_SYS_CLK(sys_csi_dig_clkin, CLKCTRL_SYS_CLK_EN0_REG1, 18);
MESON_CLK_GATE_SYS_CLK(sys_ge2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
MESON_CLK_GATE_SYS_CLK(sys_usb3drd, CLKCTRL_SYS_CLK_EN0_REG1, 25);
MESON_CLK_GATE_SYS_CLK(sys_usb2drd, CLKCTRL_SYS_CLK_EN0_REG1, 26);
MESON_CLK_GATE_SYS_CLK(sys_pcie_phy, CLKCTRL_SYS_CLK_EN0_REG1, 27);
MESON_CLK_GATE_SYS_CLK(sys_pcie_mac, CLKCTRL_SYS_CLK_EN0_REG1, 28);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 30);

#define MESON_CLK_GATE_HW(_name, _reg, _bit, _parent_hw)              \
static struct clk_regmap _name = {                                           \
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

MESON_CLK_GATE_HW(sys_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 31, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3, sys_i2c_m_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_SYS_CLK(sys_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
MESON_CLK_GATE_SYS_CLK(sys_hdmi20_aes, CLKCTRL_SYS_CLK_EN0_REG2, 9);
MESON_CLK_GATE_SYS_CLK(sys_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
MESON_CLK_GATE_SYS_CLK(sys_csi2_host, CLKCTRL_SYS_CLK_EN0_REG2, 16);
MESON_CLK_GATE_SYS_CLK(sys_csi2_adapt, CLKCTRL_SYS_CLK_EN0_REG2, 17);
MESON_CLK_GATE_SYS_CLK(sys_cpu_apb, CLKCTRL_SYS_CLK_EN0_REG2, 19);
MESON_CLK_GATE_SYS_CLK(sys_dspa, CLKCTRL_SYS_CLK_EN0_REG2, 21);
MESON_CLK_GATE_SYS_CLK(sys_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
MESON_CLK_GATE_SYS_CLK(sys_csi2_phy0, CLKCTRL_SYS_CLK_EN0_REG2, 27);
MESON_CLK_GATE_SYS_CLK(sys_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);

static struct clk_regmap sys_pwm_a;
MESON_CLK_GATE_HW(sys_pwm_j, CLKCTRL_SYS_CLK_EN0_REG2, 29, sys_pwm_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_gic, CLKCTRL_SYS_CLK_EN0_REG2, 30);
MESON_CLK_GATE_HW(sys_pwm_i, CLKCTRL_SYS_CLK_EN0_REG2, 31, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_h, CLKCTRL_SYS_CLK_EN0_REG3, 0, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_g, CLKCTRL_SYS_CLK_EN0_REG3, 1, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_f, CLKCTRL_SYS_CLK_EN0_REG3, 2, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_e, CLKCTRL_SYS_CLK_EN0_REG3, 3, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_d, CLKCTRL_SYS_CLK_EN0_REG3, 4, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_c, CLKCTRL_SYS_CLK_EN0_REG3, 5, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_b, CLKCTRL_SYS_CLK_EN0_REG3, 6, sys_pwm_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_pwm_a, CLKCTRL_SYS_CLK_EN0_REG3, 7);

#define MESON_CLK_GATE_AXI_CLK(_name, _reg, _bit)                     \
static struct clk_regmap _name = {                                           \
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

MESON_CLK_GATE_AXI_CLK(axi_ao_nic, CLKCTRL_AXI_CLK_EN0, 0);
MESON_CLK_GATE_AXI_CLK(axi_sram, CLKCTRL_AXI_CLK_EN0, 1);
MESON_CLK_GATE_AXI_CLK(axi_dev0_mmc, CLKCTRL_AXI_CLK_EN0, 2);
MESON_CLK_GATE_AXI_CLK(axi_cpu_sram, CLKCTRL_AXI_CLK_EN0, 4);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&hifi1_pll,
	&mclk_pll,
	&mclk_pll_clk,
	&mclk0_div,
	&mclk0,
	&fclk50m,
	&fclk_div2,
	&fclk_div2p5,
	&fclk_div3,
	&fclk_div4,
	&fclk_div5,
	&fclk_div7
};

static struct clk_regmap *const clk_regmaps[] = {
	&rtc_dual_clkin,
	&rtc_dual_div,
	&rtc_dual_mux,
	&rtc_dual,
	&rtc_clk,
	&clk_24m_in,
	&clk_12m,
	&sar_adc_mux,
	&sar_adc_div,
	&sar_adc,
	&cecb_dual_clkin,
	&cecb_dual_div,
	&cecb_dual_mux,
	&cecb_dual,
	&cecb_clk,
	&sc_pre_mux,
	&sc_pre_div,
	&sc_pre,
	&sc,
	&cdac_mux,
	&cdac_div,
	&cdac,
	&video_src0_in_mux,
	&video_src0_in,
	&video_src0_div,
	&video_src0,
	&video_src1_in_mux,
	&video_src1_in,
	&video_src1_div,
	&video_src1,
	&video0_div1,
	&video0_div2_gate,
	&video0_div4_gate,
	&video0_div6_gate,
	&video0_div12_gate,
	&video2_div1,
	&video2_div2_gate,
	&video2_div4_gate,
	&video2_div6_gate,
	&video2_div12_gate,
	&lcd_an_mux,
	&lcd_an,
	&lcd_an_ph2,
	&lcd_an_ph3,
	&enci_mux,
	&enci,
	&encp_mux,
	&encp,
	&encl_mux,
	&encl,
	&vdac_mux,
	&vdac,
	&hdmitx_pixel_mux,
	&hdmitx_pixel,
	&hdmitx_fe_mux,
	&hdmitx_fe,
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
	&vdec_0_mux,
	&vdec_0_div,
	&vdec_0,
	&vdec_1_mux,
	&vdec_1_div,
	&vdec_1,
	&vdec,
	&hevcf_0_mux,
	&hevcf_0_div,
	&hevcf_0,
	&hevcf_1_mux,
	&hevcf_1_div,
	&hevcf_1,
	&hevcf,
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
	&vpu_clkc_0_mux,
	&vpu_clkc_0_div,
	&vpu_clkc_0,
	&vpu_clkc_1_mux,
	&vpu_clkc_1_div,
	&vpu_clkc_1,
	&vpu_clkc,
	&vapb_0_mux,
	&vapb_0_div,
	&vapb_0,
	&vapb_1_mux,
	&vapb_1_div,
	&vapb_1,
	&vapb,
	&ge2d,
	&dewarpa_mux,
	&dewarpa_div,
	&dewarpa,
	&cmpr_mux,
	&cmpr_div,
	&cmpr,
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
	&pwm_a_mux,
	&pwm_a_div,
	&pwm_a,
	&pwm_b_mux,
	&pwm_b_div,
	&pwm_b,
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
	&spicc0_mux,
	&spicc0_div,
	&spicc0,
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
	&eth_clk_rmii_mux,
	&eth_clk_rmii_div,
	&eth_clk_rmii,
	&eth_clk125m,
	&ts_div,
	&ts,
	&amfc_mux,
	&amfc_div,
	&amfc,
	&csi2_adapt_mux,
	&csi2_adapt_div,
	&csi2_adapt,
	&mipi_csi_phy_0_mux,
	&mipi_csi_phy_0_div,
	&mipi_csi_phy_0,
	&mipi_csi_phy_1_mux,
	&mipi_csi_phy_1_div,
	&mipi_csi_phy_1,
	&mipi_csi_phy,
	&mipi_dsi_meas_mux,
	&mipi_dsi_meas_div,
	&mipi_dsi_meas,
	&nna_mux,
	&nna_div,
	&nna,
	&vc9000e_axi_mux,
	&vc9000e_axi_div,
	&vc9000e_axi,
	&vc9000e_core_mux,
	&vc9000e_core_div,
	&vc9000e_core,
	&dspa_0_mux,
	&dspa_0_div,
	&dspa_0,
	&dspa_1_mux,
	&dspa_1_div,
	&dspa_1,
	&dspa,
	&pcie_tl_mux,
	&pcie_tl_div,
	&pcie_tl,
	&usb_250m_mux,
	&usb_250m_div,
	&usb_250m,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_am_axi,
	&sys_dos,
	&sys_vc9000e,
	&sys_mipi_dsi,
	&sys_eth_phy,
	&sys_amfc,
	&sys_mali,
	&sys_nna,
	&sys_eth_mac,
	&sys_aocpu,
	&sys_aucpu,
	&sys_cec,
	&sys_mipi_dsi_phy,
	&sys_sd_emmc_a,
	&sys_sd_emmc_b,
	&sys_sd_emmc_c,
	&sys_smartcard,
	&sys_acodec,
	&sys_msr_clk,
	&sys_ir_ctrl,
	&sys_audio,
	&sys_eth,
	&sys_uart_a,
	&sys_uart_b,
	&sys_uart_c,
	&sys_uart_d,
	&sys_uart_e,
	&sys_ts_core,
	&sys_ts_pll,
	&sys_csi_dig_clkin,
	&sys_ge2d,
	&sys_spicc0,
	&sys_usb3drd,
	&sys_usb2drd,
	&sys_pcie_phy,
	&sys_pcie_mac,
	&sys_i2c_m_a,
	&sys_i2c_m_b,
	&sys_i2c_m_c,
	&sys_i2c_m_d,
	&sys_i2c_m_e,
	&sys_i2c_m_f,
	&sys_hdmitx_apb,
	&sys_i2c_s_a,
	&sys_hdmi20_aes,
	&sys_mmc_apb,
	&sys_csi2_host,
	&sys_csi2_adapt,
	&sys_cpu_apb,
	&sys_dspa,
	&sys_vpu_intr,
	&sys_csi2_phy0,
	&sys_sar_adc,
	&sys_pwm_j,
	&sys_gic,
	&sys_pwm_i,
	&sys_pwm_h,
	&sys_pwm_g,
	&sys_pwm_f,
	&sys_pwm_e,
	&sys_pwm_d,
	&sys_pwm_c,
	&sys_pwm_b,
	&sys_pwm_a,
	&axi_ao_nic,
	&axi_sram,
	&axi_dev0_mmc,
	&axi_cpu_sram
};

static struct clk_hw *s6_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_HIFI1_PLL]             = &hifi1_pll.hw,
	[CLKID_MCLK_PLL]              = &mclk_pll.hw,
	[CLKID_MCLK_PLL_CLK]          = &mclk_pll_clk.hw,
	[CLKID_MCLK0_DIV]             = &mclk0_div.hw,
	[CLKID_MCLK0_DIV2]            = &mclk0_div2.hw,
	[CLKID_MCLK0]                 = &mclk0.hw,
	[CLKID_FCLK50M_DIV]           = &fclk50m_div.hw,
	[CLKID_FCLK50M]               = &fclk50m.hw,
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
	[CLKID_RTC_DUAL_CLKIN]        = &rtc_dual_clkin.hw,
	[CLKID_RTC_DUAL_DIV]          = &rtc_dual_div.hw,
	[CLKID_RTC_DUAL_MUX]          = &rtc_dual_mux.hw,
	[CLKID_RTC_DUAL]              = &rtc_dual.hw,
	[CLKID_RTC_CLK]               = &rtc_clk.hw,
	[CLKID_CLK_24M_IN]            = &clk_24m_in.hw,
	[CLKID_CLK_12M_DIV]           = &clk_12m_div.hw,
	[CLKID_CLK_12M]               = &clk_12m.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_CECB_DUAL_CLKIN]       = &cecb_dual_clkin.hw,
	[CLKID_CECB_DUAL_DIV]         = &cecb_dual_div.hw,
	[CLKID_CECB_DUAL_MUX]         = &cecb_dual_mux.hw,
	[CLKID_CECB_DUAL]             = &cecb_dual.hw,
	[CLKID_CECB_CLK]              = &cecb_clk.hw,
	[CLKID_SC_PRE_MUX]            = &sc_pre_mux.hw,
	[CLKID_SC_PRE_DIV]            = &sc_pre_div.hw,
	[CLKID_SC_PRE]                = &sc_pre.hw,
	[CLKID_SC]                    = &sc.hw,
	[CLKID_CDAC_MUX]              = &cdac_mux.hw,
	[CLKID_CDAC_DIV]              = &cdac_div.hw,
	[CLKID_CDAC]                  = &cdac.hw,
	[CLKID_VIDEO_SRC0_IN_MUX]     = &video_src0_in_mux.hw,
	[CLKID_VIDEO_SRC0_IN]         = &video_src0_in.hw,
	[CLKID_VIDEO_SRC0_DIV]        = &video_src0_div.hw,
	[CLKID_VIDEO_SRC0]            = &video_src0.hw,
	[CLKID_VIDEO_SRC1_IN_MUX]     = &video_src1_in_mux.hw,
	[CLKID_VIDEO_SRC1_IN]         = &video_src1_in.hw,
	[CLKID_VIDEO_SRC1_DIV]        = &video_src1_div.hw,
	[CLKID_VIDEO_SRC1]            = &video_src1.hw,
	[CLKID_VIDEO0_DIV1]           = &video0_div1.hw,
	[CLKID_VIDEO0_DIV2_GATE]      = &video0_div2_gate.hw,
	[CLKID_VIDEO0_DIV2]           = &video0_div2.hw,
	[CLKID_VIDEO0_DIV4_GATE]      = &video0_div4_gate.hw,
	[CLKID_VIDEO0_DIV4]           = &video0_div4.hw,
	[CLKID_VIDEO0_DIV6_GATE]      = &video0_div6_gate.hw,
	[CLKID_VIDEO0_DIV6]           = &video0_div6.hw,
	[CLKID_VIDEO0_DIV12_GATE]     = &video0_div12_gate.hw,
	[CLKID_VIDEO0_DIV12]          = &video0_div12.hw,
	[CLKID_VIDEO2_DIV1]           = &video2_div1.hw,
	[CLKID_VIDEO2_DIV2_GATE]      = &video2_div2_gate.hw,
	[CLKID_VIDEO2_DIV2]           = &video2_div2.hw,
	[CLKID_VIDEO2_DIV4_GATE]      = &video2_div4_gate.hw,
	[CLKID_VIDEO2_DIV4]           = &video2_div4.hw,
	[CLKID_VIDEO2_DIV6_GATE]      = &video2_div6_gate.hw,
	[CLKID_VIDEO2_DIV6]           = &video2_div6.hw,
	[CLKID_VIDEO2_DIV12_GATE]     = &video2_div12_gate.hw,
	[CLKID_VIDEO2_DIV12]          = &video2_div12.hw,
	[CLKID_LCD_AN_MUX]            = &lcd_an_mux.hw,
	[CLKID_LCD_AN]                = &lcd_an.hw,
	[CLKID_LCD_AN_PH2]            = &lcd_an_ph2.hw,
	[CLKID_LCD_AN_PH3]            = &lcd_an_ph3.hw,
	[CLKID_ENCI_MUX]              = &enci_mux.hw,
	[CLKID_ENCI]                  = &enci.hw,
	[CLKID_ENCP_MUX]              = &encp_mux.hw,
	[CLKID_ENCP]                  = &encp.hw,
	[CLKID_ENCL_MUX]              = &encl_mux.hw,
	[CLKID_ENCL]                  = &encl.hw,
	[CLKID_VDAC_MUX]              = &vdac_mux.hw,
	[CLKID_VDAC]                  = &vdac.hw,
	[CLKID_HDMITX_PIXEL_MUX]      = &hdmitx_pixel_mux.hw,
	[CLKID_HDMITX_PIXEL]          = &hdmitx_pixel.hw,
	[CLKID_HDMITX_FE_MUX]         = &hdmitx_fe_mux.hw,
	[CLKID_HDMITX_FE]             = &hdmitx_fe.hw,
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
	[CLKID_VDEC_0_MUX]            = &vdec_0_mux.hw,
	[CLKID_VDEC_0_DIV]            = &vdec_0_div.hw,
	[CLKID_VDEC_0]                = &vdec_0.hw,
	[CLKID_VDEC_1_MUX]            = &vdec_1_mux.hw,
	[CLKID_VDEC_1_DIV]            = &vdec_1_div.hw,
	[CLKID_VDEC_1]                = &vdec_1.hw,
	[CLKID_VDEC]                  = &vdec.hw,
	[CLKID_HEVCF_0_MUX]           = &hevcf_0_mux.hw,
	[CLKID_HEVCF_0_DIV]           = &hevcf_0_div.hw,
	[CLKID_HEVCF_0]               = &hevcf_0.hw,
	[CLKID_HEVCF_1_MUX]           = &hevcf_1_mux.hw,
	[CLKID_HEVCF_1_DIV]           = &hevcf_1_div.hw,
	[CLKID_HEVCF_1]               = &hevcf_1.hw,
	[CLKID_HEVCF]                 = &hevcf.hw,
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
	[CLKID_VPU_CLKC_0_MUX]        = &vpu_clkc_0_mux.hw,
	[CLKID_VPU_CLKC_0_DIV]        = &vpu_clkc_0_div.hw,
	[CLKID_VPU_CLKC_0]            = &vpu_clkc_0.hw,
	[CLKID_VPU_CLKC_1_MUX]        = &vpu_clkc_1_mux.hw,
	[CLKID_VPU_CLKC_1_DIV]        = &vpu_clkc_1_div.hw,
	[CLKID_VPU_CLKC_1]            = &vpu_clkc_1.hw,
	[CLKID_VPU_CLKC]              = &vpu_clkc.hw,
	[CLKID_VAPB_0_MUX]            = &vapb_0_mux.hw,
	[CLKID_VAPB_0_DIV]            = &vapb_0_div.hw,
	[CLKID_VAPB_0]                = &vapb_0.hw,
	[CLKID_VAPB_1_MUX]            = &vapb_1_mux.hw,
	[CLKID_VAPB_1_DIV]            = &vapb_1_div.hw,
	[CLKID_VAPB_1]                = &vapb_1.hw,
	[CLKID_VAPB]                  = &vapb.hw,
	[CLKID_GE2D]                  = &ge2d.hw,
	[CLKID_DEWARPA_MUX]           = &dewarpa_mux.hw,
	[CLKID_DEWARPA_DIV]           = &dewarpa_div.hw,
	[CLKID_DEWARPA]               = &dewarpa.hw,
	[CLKID_CMPR_MUX]              = &cmpr_mux.hw,
	[CLKID_CMPR_DIV]              = &cmpr_div.hw,
	[CLKID_CMPR]                  = &cmpr.hw,
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
	[CLKID_PWM_A_MUX]             = &pwm_a_mux.hw,
	[CLKID_PWM_A_DIV]             = &pwm_a_div.hw,
	[CLKID_PWM_A]                 = &pwm_a.hw,
	[CLKID_PWM_B_MUX]             = &pwm_b_mux.hw,
	[CLKID_PWM_B_DIV]             = &pwm_b_div.hw,
	[CLKID_PWM_B]                 = &pwm_b.hw,
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
	[CLKID_SPICC0_MUX]            = &spicc0_mux.hw,
	[CLKID_SPICC0_DIV]            = &spicc0_div.hw,
	[CLKID_SPICC0]                = &spicc0.hw,
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
	[CLKID_ETH_CLK_RMII_MUX]      = &eth_clk_rmii_mux.hw,
	[CLKID_ETH_CLK_RMII_DIV]      = &eth_clk_rmii_div.hw,
	[CLKID_ETH_CLK_RMII]          = &eth_clk_rmii.hw,
	[CLKID_ETH_CLK125M_DIV]       = &eth_clk125m_div.hw,
	[CLKID_ETH_CLK125M]           = &eth_clk125m.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_AMFC_MUX]              = &amfc_mux.hw,
	[CLKID_AMFC_DIV]              = &amfc_div.hw,
	[CLKID_AMFC]                  = &amfc.hw,
	[CLKID_CSI2_ADAPT_MUX]        = &csi2_adapt_mux.hw,
	[CLKID_CSI2_ADAPT_DIV]        = &csi2_adapt_div.hw,
	[CLKID_CSI2_ADAPT]            = &csi2_adapt.hw,
	[CLKID_MIPI_CSI_PHY_0_MUX]    = &mipi_csi_phy_0_mux.hw,
	[CLKID_MIPI_CSI_PHY_0_DIV]    = &mipi_csi_phy_0_div.hw,
	[CLKID_MIPI_CSI_PHY_0]        = &mipi_csi_phy_0.hw,
	[CLKID_MIPI_CSI_PHY_1_MUX]    = &mipi_csi_phy_1_mux.hw,
	[CLKID_MIPI_CSI_PHY_1_DIV]    = &mipi_csi_phy_1_div.hw,
	[CLKID_MIPI_CSI_PHY_1]        = &mipi_csi_phy_1.hw,
	[CLKID_MIPI_CSI_PHY]          = &mipi_csi_phy.hw,
	[CLKID_MIPI_DSI_MEAS_MUX]     = &mipi_dsi_meas_mux.hw,
	[CLKID_MIPI_DSI_MEAS_DIV]     = &mipi_dsi_meas_div.hw,
	[CLKID_MIPI_DSI_MEAS]         = &mipi_dsi_meas.hw,
	[CLKID_NNA_MUX]               = &nna_mux.hw,
	[CLKID_NNA_DIV]               = &nna_div.hw,
	[CLKID_NNA]                   = &nna.hw,
	[CLKID_VC9000E_AXI_MUX]       = &vc9000e_axi_mux.hw,
	[CLKID_VC9000E_AXI_DIV]       = &vc9000e_axi_div.hw,
	[CLKID_VC9000E_AXI]           = &vc9000e_axi.hw,
	[CLKID_VC9000E_CORE_MUX]      = &vc9000e_core_mux.hw,
	[CLKID_VC9000E_CORE_DIV]      = &vc9000e_core_div.hw,
	[CLKID_VC9000E_CORE]          = &vc9000e_core.hw,
	[CLKID_DSPA_0_MUX]            = &dspa_0_mux.hw,
	[CLKID_DSPA_0_DIV]            = &dspa_0_div.hw,
	[CLKID_DSPA_0]                = &dspa_0.hw,
	[CLKID_DSPA_1_MUX]            = &dspa_1_mux.hw,
	[CLKID_DSPA_1_DIV]            = &dspa_1_div.hw,
	[CLKID_DSPA_1]                = &dspa_1.hw,
	[CLKID_DSPA]                  = &dspa.hw,
	[CLKID_PCIE_TL_MUX]           = &pcie_tl_mux.hw,
	[CLKID_PCIE_TL_DIV]           = &pcie_tl_div.hw,
	[CLKID_PCIE_TL]               = &pcie_tl.hw,
	[CLKID_USB_250M_MUX]          = &usb_250m_mux.hw,
	[CLKID_USB_250M_DIV]          = &usb_250m_div.hw,
	[CLKID_USB_250M]              = &usb_250m.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_AM_AXI]            = &sys_am_axi.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_VC9000E]           = &sys_vc9000e.hw,
	[CLKID_SYS_MIPI_DSI]          = &sys_mipi_dsi.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_AMFC]              = &sys_amfc.hw,
	[CLKID_SYS_MALI]              = &sys_mali.hw,
	[CLKID_SYS_NNA]               = &sys_nna.hw,
	[CLKID_SYS_ETH_MAC]           = &sys_eth_mac.hw,
	[CLKID_SYS_AOCPU]             = &sys_aocpu.hw,
	[CLKID_SYS_AUCPU]             = &sys_aucpu.hw,
	[CLKID_SYS_CEC]               = &sys_cec.hw,
	[CLKID_SYS_MIPI_DSI_PHY]      = &sys_mipi_dsi_phy.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_SMARTCARD]         = &sys_smartcard.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_IR_CTRL]           = &sys_ir_ctrl.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_ETH]               = &sys_eth.hw,
	[CLKID_SYS_UART_A]            = &sys_uart_a.hw,
	[CLKID_SYS_UART_B]            = &sys_uart_b.hw,
	[CLKID_SYS_UART_C]            = &sys_uart_c.hw,
	[CLKID_SYS_UART_D]            = &sys_uart_d.hw,
	[CLKID_SYS_UART_E]            = &sys_uart_e.hw,
	[CLKID_SYS_TS_CORE]           = &sys_ts_core.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_CSI_DIG_CLKIN]     = &sys_csi_dig_clkin.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPICC0]            = &sys_spicc0.hw,
	[CLKID_SYS_USB3DRD]           = &sys_usb3drd.hw,
	[CLKID_SYS_USB2DRD]           = &sys_usb2drd.hw,
	[CLKID_SYS_PCIE_PHY]          = &sys_pcie_phy.hw,
	[CLKID_SYS_PCIE_MAC]          = &sys_pcie_mac.hw,
	[CLKID_SYS_I2C_M_A]           = &sys_i2c_m_a.hw,
	[CLKID_SYS_I2C_M_B]           = &sys_i2c_m_b.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_I2C_M_F]           = &sys_i2c_m_f.hw,
	[CLKID_SYS_HDMITX_APB]        = &sys_hdmitx_apb.hw,
	[CLKID_SYS_I2C_S_A]           = &sys_i2c_s_a.hw,
	[CLKID_SYS_HDMI20_AES]        = &sys_hdmi20_aes.hw,
	[CLKID_SYS_MMC_APB]           = &sys_mmc_apb.hw,
	[CLKID_SYS_CSI2_HOST]         = &sys_csi2_host.hw,
	[CLKID_SYS_CSI2_ADAPT]        = &sys_csi2_adapt.hw,
	[CLKID_SYS_CPU_APB]           = &sys_cpu_apb.hw,
	[CLKID_SYS_DSPA]              = &sys_dspa.hw,
	[CLKID_SYS_VPU_INTR]          = &sys_vpu_intr.hw,
	[CLKID_SYS_CSI2_PHY0]         = &sys_csi2_phy0.hw,
	[CLKID_SYS_SAR_ADC]           = &sys_sar_adc.hw,
	[CLKID_SYS_PWM_J]             = &sys_pwm_j.hw,
	[CLKID_SYS_GIC]               = &sys_gic.hw,
	[CLKID_SYS_PWM_I]             = &sys_pwm_i.hw,
	[CLKID_SYS_PWM_H]             = &sys_pwm_h.hw,
	[CLKID_SYS_PWM_G]             = &sys_pwm_g.hw,
	[CLKID_SYS_PWM_F]             = &sys_pwm_f.hw,
	[CLKID_SYS_PWM_E]             = &sys_pwm_e.hw,
	[CLKID_SYS_PWM_D]             = &sys_pwm_d.hw,
	[CLKID_SYS_PWM_C]             = &sys_pwm_c.hw,
	[CLKID_SYS_PWM_B]             = &sys_pwm_b.hw,
	[CLKID_SYS_PWM_A]             = &sys_pwm_a.hw,
	[CLKID_AXI_AO_NIC]            = &axi_ao_nic.hw,
	[CLKID_AXI_SRAM]              = &axi_sram.hw,
	[CLKID_AXI_DEV0_MMC]          = &axi_dev0_mmc.hw,
	[CLKID_AXI_CPU_SRAM]          = &axi_cpu_sram.hw
};

static const struct meson_clk_hw_data s6_clks = {
	.hws = s6_hw_clks,
	.num = ARRAY_SIZE(s6_hw_clks),
};

static int meson_s6_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct regmap *pll_map;
	struct meson_clk_hw_data *data;
	struct clk *clk;
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
		.compatible = "amlogic,s6-clkc",
		.data = &s6_clks
	},
	{}
};

static struct platform_driver s6_driver = {
	.probe			= meson_s6_probe,
	.driver			= {
		.name		= "s6-clkc",
		.of_match_table	= clkc_match_table,
	},
};

builtin_platform_driver(s6_driver);
MODULE_LICENSE("GPL v2");
