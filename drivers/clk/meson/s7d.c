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
#include <linux/syscore_ops.h>
#include <linux/suspend.h>

#include "meson-clkc-utils.h"
#include "clk-pll.h"
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "clk-dualdiv.h"
#include "s7d.h"
#include <dt-bindings/clock/amlogic,s7d-clkc.h>

#include <linux/amlogic/cpu_version.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(128, 0, 0), /* DCO = 1536M OD = 0 PLL = 1536M */
	PLL_PARAMS_OD(192, 0, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
	{ /* sentinel */ }
};

static const struct reg_sequence gp0_pll_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL0, .def = 0x00010000 },
	{ .reg = ANACTRL_GP0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_GP0PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_GP0PLL_CTRL4, .def = 0x1f01110f }
};

static struct clk_regmap gp0_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_GP0PLL_CTRL1,
			.shift   = 27,
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
			.reg_off = ANACTRL_GP0PLL_CTRL3,
			.shift   = 29,
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
			.fw_name = "gp0_pll_src",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi_pll_table[] = {
	PLL_PARAMS_OD(150, 0, 0), /* DCO = 1800M OD = 0 PLL = 1800M */
	PLL_PARAMS_OD(150, 0, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 0, 2), /* DCO = 1944M OD = 4 PLL = 486M */
	{ /* sentinel */ }
};

static const struct reg_sequence hifi_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFI0PLL_CTRL0, .def = 0x00010000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_HIFI0PLL_CTRL4, .def = 0x1f01110f }
};

static struct clk_regmap hifi_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFI0PLL_CTRL1,
			.shift   = 27,
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
			.reg_off = ANACTRL_HIFI0PLL_CTRL3,
			.shift   = 29,
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
			.fw_name = "hifi0_pll_src",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi1_pll_table[] = {
	PLL_PARAMS_OD(150, 0, 0), /* DCO = 1800M OD = 0 PLL = 1800M */
	PLL_PARAMS_OD(150, 0, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 0, 2), /* DCO = 1944M OD = 4 PLL = 486M */
	{ /* sentinel */ }
};

static const struct reg_sequence hifi1_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFI1PLL_CTRL0, .def = 0x00010000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL1, .def = 0x11480000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL2, .def = 0x1219b010 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL3, .def = 0x20008010 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL4, .def = 0x1f01110f }
};

static struct clk_regmap hifi1_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL1,
			.shift   = 27,
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
			.reg_off = ANACTRL_HIFI1PLL_CTRL3,
			.shift   = 29,
			.width   = 1,
		},
		.table = hifi1_pll_table,
		.init_regs = hifi1_pll_init_regs,
		.init_count = ARRAY_SIZE(hifi1_pll_init_regs),
		.flags = CLK_MESON_PLL_POWER_OF_TWO |
			 CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_RSTN | CLK_MESON_PLL_L_RSTN,
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

static struct clk_fixed_factor fclk_50m_div = {
	.mult = 1,
	.div = 40,
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap fclk_50m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_FIXPLL_CTRL4,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "fclk_50m",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_50m_div.hw,
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
		.offset = CLKCTRL_CECB_CTRL0,
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

static const struct clk_parent_data sc_parent_data[] = {
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap sc_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = sc_parent_data,
		.num_parents = ARRAY_SIZE(sc_parent_data),
	},
};

static struct clk_regmap sc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc_div.hw,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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

static const struct clk_parent_data hcodec_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
	{ .fw_name = "xtal" }
};

static struct clk_regmap hcodec_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
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
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_VDEC_CLK_CTRL,
		.bit_idx = 24,
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
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_PARENT,
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
		.flags = CLK_SET_RATE_NO_REPARENT |
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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
		.flags = CLK_SET_RATE_PARENT | CLK_SET_RATE_GATE,
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

static u32 pwm_c_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_c_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = pwm_c_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_c_parent_data,
		.num_parents = ARRAY_SIZE(pwm_c_parent_data),
	},
};

static struct clk_regmap pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_c_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 pwm_d_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_d_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = pwm_d_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_d_parent_data,
		.num_parents = ARRAY_SIZE(pwm_d_parent_data),
	},
};

static struct clk_regmap pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_d_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 pwm_e_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_e_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = pwm_e_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_e_parent_data,
		.num_parents = ARRAY_SIZE(pwm_e_parent_data),
	},
};

static struct clk_regmap pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&pwm_e_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 pwm_f_parent_table[] = { 0, 2, 3 };
static const struct clk_parent_data pwm_f_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = pwm_f_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_f_parent_data,
		.num_parents = ARRAY_SIZE(pwm_f_parent_data),
	},
};

static struct clk_regmap pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
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

static struct clk_fixed_factor eth_125m_div = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data) {
		.name = "eth_125m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2.hw,
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

static u32 gen_parent_table[] = { 0, 1, 5, 6, 7, 8, 19, 20, 21, 22, 23,
				 24 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &rtc_clk.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi1_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .fw_name = "gp1_pll" },
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
MESON_CLK_GATE_SYS_CLK(sys_ethphy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
MESON_CLK_GATE_SYS_CLK(sys_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
MESON_CLK_GATE_SYS_CLK(sys_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
MESON_CLK_GATE_SYS_CLK(sys_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
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
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_SYS_CLK(sys_ge2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
MESON_CLK_GATE_SYS_CLK(sys_usb, CLKCTRL_SYS_CLK_EN0_REG1, 26);
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
MESON_CLK_GATE_SYS_CLK(sys_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_SYS_CLK(sys_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
MESON_CLK_GATE_SYS_CLK(sys_hdmi20_aes, CLKCTRL_SYS_CLK_EN0_REG2, 9);
MESON_CLK_GATE_SYS_CLK(sys_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
MESON_CLK_GATE_SYS_CLK(sys_cpu_apb, CLKCTRL_SYS_CLK_EN0_REG2, 19);
MESON_CLK_GATE_SYS_CLK(sys_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
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

MESON_CLK_GATE_AXI_CLK(axi_sys_nic, CLKCTRL_AXI_CLK_EN0, 0);
MESON_CLK_GATE_AXI_CLK(axi_rama, CLKCTRL_AXI_CLK_EN0, 1);
MESON_CLK_GATE_AXI_CLK(axi_dev0_mmc, CLKCTRL_AXI_CLK_EN0, 2);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&hifi1_pll,
	&fclk_50m,
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
	&sar_adc_mux,
	&sar_adc_div,
	&sar_adc,
	&cecb_dual_clkin,
	&cecb_dual_div,
	&cecb_dual_mux,
	&cecb_dual,
	&cecb_clk,
	&sc_mux,
	&sc_div,
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
	&eth_rmii_div,
	&eth_rmii,
	&eth_125m,
	&ts_div,
	&ts,
	&amfc_mux,
	&amfc_div,
	&amfc,
	&clk_24m_in,
	&clk_12m,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_am_axi,
	&sys_dos,
	&sys_ethphy,
	&sys_mali,
	&sys_aocpu,
	&sys_aucpu,
	&sys_cec,
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
	&sys_ts_pll,
	&sys_ge2d,
	&sys_spicc0,
	&sys_usb,
	&sys_i2c_m_a,
	&sys_i2c_m_b,
	&sys_i2c_m_c,
	&sys_i2c_m_d,
	&sys_i2c_m_e,
	&sys_hdmitx_apb,
	&sys_i2c_s_a,
	&sys_hdmi20_aes,
	&sys_mmc_apb,
	&sys_cpu_apb,
	&sys_vpu_intr,
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
	&axi_sys_nic,
	&axi_rama,
	&axi_dev0_mmc
};

static struct clk_hw *s7d_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_HIFI1_PLL]             = &hifi1_pll.hw,
	[CLKID_FCLK_50M_DIV]          = &fclk_50m_div.hw,
	[CLKID_FCLK_50M]              = &fclk_50m.hw,
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
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_CECB_DUAL_CLKIN]       = &cecb_dual_clkin.hw,
	[CLKID_CECB_DUAL_DIV]         = &cecb_dual_div.hw,
	[CLKID_CECB_DUAL_MUX]         = &cecb_dual_mux.hw,
	[CLKID_CECB_DUAL]             = &cecb_dual.hw,
	[CLKID_CECB_CLK]              = &cecb_clk.hw,
	[CLKID_SC_MUX]                = &sc_mux.hw,
	[CLKID_SC_DIV]                = &sc_div.hw,
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
	[CLKID_ETH_RMII_DIV]          = &eth_rmii_div.hw,
	[CLKID_ETH_RMII]              = &eth_rmii.hw,
	[CLKID_ETH_125M_DIV]          = &eth_125m_div.hw,
	[CLKID_ETH_125M]              = &eth_125m.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_AMFC_MUX]              = &amfc_mux.hw,
	[CLKID_AMFC_DIV]              = &amfc_div.hw,
	[CLKID_AMFC]                  = &amfc.hw,
	[CLKID_CLK_24M_IN]            = &clk_24m_in.hw,
	[CLKID_CLK_12M_DIV]           = &clk_12m_div.hw,
	[CLKID_CLK_12M]               = &clk_12m.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_AM_AXI]            = &sys_am_axi.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_ETHPHY]            = &sys_ethphy.hw,
	[CLKID_SYS_MALI]              = &sys_mali.hw,
	[CLKID_SYS_AOCPU]             = &sys_aocpu.hw,
	[CLKID_SYS_AUCPU]             = &sys_aucpu.hw,
	[CLKID_SYS_CEC]               = &sys_cec.hw,
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
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPICC0]            = &sys_spicc0.hw,
	[CLKID_SYS_USB]               = &sys_usb.hw,
	[CLKID_SYS_I2C_M_A]           = &sys_i2c_m_a.hw,
	[CLKID_SYS_I2C_M_B]           = &sys_i2c_m_b.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_HDMITX_APB]        = &sys_hdmitx_apb.hw,
	[CLKID_SYS_I2C_S_A]           = &sys_i2c_s_a.hw,
	[CLKID_SYS_HDMI20_AES]        = &sys_hdmi20_aes.hw,
	[CLKID_SYS_MMC_APB]           = &sys_mmc_apb.hw,
	[CLKID_SYS_CPU_APB]           = &sys_cpu_apb.hw,
	[CLKID_SYS_VPU_INTR]          = &sys_vpu_intr.hw,
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
	[CLKID_AXI_SYS_NIC]           = &axi_sys_nic.hw,
	[CLKID_AXI_RAMA]              = &axi_rama.hw,
	[CLKID_AXI_DEV0_MMC]          = &axi_dev0_mmc.hw
};

static const struct meson_clk_hw_data s7d_clks = {
	.hws = s7d_hw_clks,
	.num = ARRAY_SIZE(s7d_hw_clks),
};

static const char * const scmi_clk_name[] = {"cpu_clk", "dsu_clk", "gp1_pll",
					     "pwm_g", "pwm_i"};
static unsigned long scmi_clk_saved_rate[ARRAY_SIZE(scmi_clk_name)];
/* Distinguish between std and str, syscore_ops is no need called when str */
static int hib_enable;

static int meson_s7d_syscore_suspend(void)
{
	int ret = 0;

	if (hib_enable) {
		ret = clk_save_context();
		if (ret)
			return ret;
	}

	return ret;
}

static void meson_s7d_syscore_resume(void)
{
	if (hib_enable)
		clk_restore_context();
}

static struct syscore_ops meson_s7d_syscore_ops = {
	.suspend	= meson_s7d_syscore_suspend,
	.resume		= meson_s7d_syscore_resume,
};

static int meson_s7d_pm_notify(struct notifier_block *notifier,
			      unsigned long pm_event,
			      void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		hib_enable = 1;
		break;
	case PM_POST_HIBERNATION:
		hib_enable = 0;
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block meson_s7d_pm_nb = {
	.notifier_call = meson_s7d_pm_notify,
};

static int meson_s7d_freeze(struct device *dev)
{
	struct clk *scmi_clk;
	int i;

	/*
	 * scmi clk does not provide save_context/restore_context ops,
	 * so saved scmi clk rate at freeze and set it back at restore.
	 */
	for (i = 0; i < ARRAY_SIZE(scmi_clk_name); ++i) {
		scmi_clk = devm_clk_get(dev, scmi_clk_name[i]);
		if (IS_ERR(scmi_clk))
			return PTR_ERR(scmi_clk);
		scmi_clk_saved_rate[i] = clk_get_rate(scmi_clk);
		devm_clk_put(dev, scmi_clk);
	}

	return 0;
}

static int meson_s7d_restore(struct device *dev)
{
	struct clk *scmi_clk;
	int i;

	for (i = 0; i < ARRAY_SIZE(scmi_clk_name); ++i) {
		scmi_clk = devm_clk_get(dev, scmi_clk_name[i]);
		if (IS_ERR(scmi_clk))
			return PTR_ERR(scmi_clk);
		/*
		 * CLK_SET_RATE_NOCACHE flag does not take effect when clk_set_rate,
		 * setting rate will be compared with the cached rate,  actual rate cannot
		 * be setted. before setting rate, using clk_get_rate to recalculate
		 * the scmi clk rate and update the cache.
		 */
		clk_get_rate(scmi_clk);
		clk_set_rate(scmi_clk, scmi_clk_saved_rate[i]);
		devm_clk_put(dev, scmi_clk);
	}

	return 0;
}

/*
 * scmi clk rate needs to be saved using clk_get_rate(), but interrupts are
 * enabled when the scmi clk rate is fetched, and syscore_suspend() is called
 * when interrupts are turned off, so scmi clk needs to be placed in freeze_noirq
 * to save state.
 */
const struct dev_pm_ops meson_s7d_pm_ops = {
	.freeze_noirq	= meson_s7d_freeze,
	.restore_noirq	= meson_s7d_restore,
};

static int meson_s7d_probe(struct platform_device *pdev)
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

		/*
		 * HACK: S7D fclk_div3 design intent should have been div3
		 * output 666M, in fact revA designed to div2 output 1G, the
		 * issue has been fixed in revB.
		 */
		if (data->hws[i] == &fclk_div3_div.hw &&
		    is_meson_rev_a())
			fclk_div3_div.div = 2;

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

	ret = devm_of_clk_add_hw_provider(dev, meson_clk_hw_get,
					   (void *)data);
	if (ret)
		return ret;

	/* register syscore ops to save clk status at std */
	register_syscore_ops(&meson_s7d_syscore_ops);
	/*
	 * register pm notify, distinguish between std and str, and ensure
	 * that syscore_ops is called only when std is used.
	 */
	register_pm_notifier(&meson_s7d_pm_nb);

	return ret;
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,s7d-clkc",
		.data = &s7d_clks
	},
	{}
};

static struct platform_driver s7d_driver = {
	.probe			= meson_s7d_probe,
	.driver			= {
		.name		= "s7d-clkc",
		.of_match_table	= clkc_match_table,
		.pm		= &meson_s7d_pm_ops,
	},
};

builtin_platform_driver(s7d_driver);
MODULE_LICENSE("GPL v2");
