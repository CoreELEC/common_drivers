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
#include "clk-dualdiv.h"
#include "clk-regmap.h"
#include "c4.h"
#include <dt-bindings/clock/amlogic,c4-clkc.h>
#include <linux/amlogic/pm.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(192, 1, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
	PLL_PARAMS_OD(256, 1, 1), /* DCO = 3072M OD = 1 PLL = 1536M */
	{ /* sentinel */ }
};

static const struct reg_sequence gp0_pll_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1, .def = 0x03a00000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2, .def = 0x00040000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3, .def = 0x0f0da200 }
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
			.shift   = 16,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 10,
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
			.shift   = 9,
			.width   = 1,
		},
		.table = gp0_pll_table,
		.init_regs = gp0_pll_init_regs,
		.init_count = ARRAY_SIZE(gp0_pll_init_regs),
		.flags = CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_NOINIT_ENABLED |
			 CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table hifi_pll_table[] = {
	PLL_PARAMS_OD(150, 1, 1), /* DCO = 1800M OD = 1 PLL = 1800M */
	PLL_PARAMS_OD(150, 1, 2), /* DCO = 1800M OD = 2 PLL = 450M */
	PLL_PARAMS_OD(163, 1, 2), /* DCO = 1956M OD = 2 PLL = 489M */
	{ /* sentinel */ }
};

static const struct reg_sequence hifi_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1, .def = 0x03a00000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2, .def = 0x00040000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3, .def = 0x0f0da200 }
};

static struct clk_regmap hifi_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 10,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_HIFIPLL_CTRL3,
			.shift   = 9,
			.width   = 1,
		},
		.table = hifi_pll_table,
		.init_regs = hifi_pll_init_regs,
		.init_count = ARRAY_SIZE(hifi_pll_init_regs),
		.flags = CLK_MESON_PLL_FIXED_EN0P5 |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_L_RSTN |
			 CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table mclk_pll_table[] = {
	PLL_PARAMS_OD(99, 1, 1), /* VCO = 2376M, CLK_OUT = 1188M */
	PLL_PARAMS_OD(100, 1, 1), /* VCO = 2400M, CLK_OUT = 1200M */
	{ /* sentinel */ }
};

static const struct reg_sequence mclk_pll_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL1, .def = 0x0480500f },
	{ .reg = ANACTRL_MPLL_CTRL2, .def = 0x00023001 },
	{ .reg = ANACTRL_MPLL_CTRL3, .def = 0x18180000 },
	{ .reg = ANACTRL_MPLL_CTRL4, .def = 0x00000202 }
};

static struct reg_sequence mclk_pll_save_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL4, .def = 0x00000202 }
};

static struct clk_regmap mclk_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 12,
			.width   = 3,
		},
		.l = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_MPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_MPLL_CTRL2,
			.shift   = 6,
			.width   = 1,
		},
		.table = mclk_pll_table,
		.init_regs = mclk_pll_init_regs,
		.init_count = ARRAY_SIZE(mclk_pll_init_regs),
		.save_regs = mclk_pll_save_regs,
		.save_reg_count = ARRAY_SIZE(mclk_pll_save_regs),
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
		.bit_idx = 9,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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
		.offset = ANACTRL_FIXPLL_CTRL3,
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

static struct clk_regmap mclk_pll_clk = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.shift = 16,
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

static const struct clk_parent_data mclk0_parent_data[] = {
	{ .hw = &mclk_pll_clk.hw },
	{ .fw_name = "oscin" },
	{ .hw = &fclk_50m.hw }
};

static struct clk_regmap mclk0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.mask = 0x3,
		.shift = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk0_parent_data,
		.num_parents = ARRAY_SIZE(mclk0_parent_data),
	},
};

static struct clk_regmap mclk0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.shift = 2,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data mclk1_parent_data[] = {
	{ .hw = &mclk_pll_clk.hw },
	{ .fw_name = "oscin" },
	{ .hw = &fclk_50m.hw }
};

static struct clk_regmap mclk1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.mask = 0x3,
		.shift = 12,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = mclk1_parent_data,
		.num_parents = ARRAY_SIZE(mclk1_parent_data),
	},
};

static struct clk_regmap mclk1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.shift = 10,
		.width = 1,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap mclk1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL4,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mclk1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mclk1_div.hw,
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
			.fw_name = "oscin",
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
	{ .fw_name = "oscin" },
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
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static const struct clk_parent_data sar_adc_parent_data[] = {
	{ .fw_name = "oscin" },
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

static const struct clk_parent_data nna_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &gp0_pll.hw },
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

static const struct clk_parent_data pwm_c_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
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

static const struct clk_parent_data pwm_d_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
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

static const struct clk_parent_data pwm_e_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
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

static const struct clk_parent_data pwm_f_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
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

static const struct clk_parent_data pwm_g_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
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

static const struct clk_parent_data pwm_h_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_h_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
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

static const struct clk_parent_data pwm_i_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_i_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
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

static const struct clk_parent_data pwm_j_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
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

static const struct clk_parent_data pwm_k_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_k_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_k_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_k_parent_data,
		.num_parents = ARRAY_SIZE(pwm_k_parent_data),
	},
};

static struct clk_regmap pwm_k_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
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

static const struct clk_parent_data pwm_l_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_l_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_l_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_l_parent_data,
		.num_parents = ARRAY_SIZE(pwm_l_parent_data),
	},
};

static struct clk_regmap pwm_l_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_KL_CTRL,
		.bit_idx = 24,
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

static const struct clk_parent_data pwm_m_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_m_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_m_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_m_parent_data,
		.num_parents = ARRAY_SIZE(pwm_m_parent_data),
	},
};

static struct clk_regmap pwm_m_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
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
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
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

static const struct clk_parent_data pwm_n_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_n_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_n_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_n_parent_data,
		.num_parents = ARRAY_SIZE(pwm_n_parent_data),
	},
};

static struct clk_regmap pwm_n_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_PWM_CLK_MN_CTRL,
		.bit_idx = 24,
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

static const struct clk_parent_data spicc0_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw }
};

static struct clk_regmap spicc0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
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
		.offset = CLKCTRL_SPISG_CLK_CTRL,
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
		.offset = CLKCTRL_SPISG_CLK_CTRL,
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

static const struct clk_parent_data spicc1_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw }
};

static struct clk_regmap spicc1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = spicc1_parent_data,
		.num_parents = ARRAY_SIZE(spicc1_parent_data),
	},
};

static struct clk_regmap spicc1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spicc1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap spicc1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&spicc1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data sd_emmc_a_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
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
		.parent_data = sd_emmc_a_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_a_parent_data),
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

static const struct clk_parent_data sd_emmc_b_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw }
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
		.parent_data = sd_emmc_b_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_b_parent_data),
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

static const struct clk_parent_data sd_emmc_c_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw }
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
		.parent_data = sd_emmc_c_parent_data,
		.num_parents = ARRAY_SIZE(sd_emmc_c_parent_data),
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

static const struct clk_parent_data eth_rmii_parent_data[] = {
	{ .hw = &fclk_div2.hw }
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
		.parent_data = eth_rmii_parent_data,
		.num_parents = ARRAY_SIZE(eth_rmii_parent_data),
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
			.fw_name = "oscin"
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

static const struct clk_parent_data hcodec_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw },
	{ .fw_name = "oscin" }
};

static struct clk_regmap hcodec_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VDEC_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC_CLK_CTRL,
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
		.offset = CLKCTRL_VDEC3_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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
		.shift = 0,
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
		.bit_idx = 8,
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
		.shift = 15,
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

static const struct clk_parent_data tahoe_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap tahoe_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_TAHOE_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tahoe_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = tahoe_parent_data,
		.num_parents = ARRAY_SIZE(tahoe_parent_data),
	},
};

static struct clk_regmap tahoe_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_TAHOE_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tahoe_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tahoe_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap tahoe = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_TAHOE_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tahoe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&tahoe_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data cve_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap cve_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CVECLK_CTRL,
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
		.offset = CLKCTRL_CVECLK_CTRL,
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
		.offset = CLKCTRL_CVECLK_CTRL,
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

static const struct clk_parent_data cve_arb_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap cve_arb_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CVECLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve_arb_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = cve_arb_parent_data,
		.num_parents = ARRAY_SIZE(cve_arb_parent_data),
	},
};

static struct clk_regmap cve_arb_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_CVECLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve_arb_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cve_arb_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap cve_arb = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CVECLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cve_arb",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&cve_arb_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ge2d_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap ge2d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ge2d_parent_data,
		.num_parents = ARRAY_SIZE(ge2d_parent_data),
	},
};

static struct clk_regmap ge2d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ge2d_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ge2d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_GE2DCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ge2d_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data dewarp_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div7.hw }
};

static struct clk_regmap dewarp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = dewarp_parent_data,
		.num_parents = ARRAY_SIZE(dewarp_parent_data),
	},
};

static struct clk_regmap dewarp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dewarp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap dewarp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEWARPA_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dewarp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&dewarp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
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
	{ .fw_name = "oscin" }
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

static const struct clk_parent_data csi_phy_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2.hw },
	{ .fw_name = "oscin" }
};

static struct clk_regmap csi_phy_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
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
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.shift = 16,
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
		.offset = CLKCTRL_ISP0_CLK_CTRL,
		.bit_idx = 24,
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

static const struct clk_parent_data amfc_parent_data[] = {
	{ .fw_name = "oscin" },
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

static u32 gen_parent_table[] = { 0, 1, 5, 7, 19, 20, 21, 22, 23, 24 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "oscin" },
	{ .hw = &rtc_clk.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
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

MESON_CLK_GATE_SYS_CLK(sys_clk_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 0);
MESON_CLK_GATE_SYS_CLK(sys_reset_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 1);
MESON_CLK_GATE_SYS_CLK(sys_analog_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 2);
MESON_CLK_GATE_SYS_CLK(sys_pwr_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 3);
MESON_CLK_GATE_SYS_CLK(sys_pad_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_sys_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 5);
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG0, 6);
MESON_CLK_GATE_SYS_CLK(sys_mmc_pclk, CLKCTRL_SYS_CLK_EN0_REG0, 8);
MESON_CLK_GATE_SYS_CLK(sys_capu, CLKCTRL_SYS_CLK_EN0_REG0, 9);
MESON_CLK_GATE_SYS_CLK(sys_cpu_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 11);
MESON_CLK_GATE_SYS_CLK(sys_jtag_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 12);
MESON_CLK_GATE_SYS_CLK(sys_irq_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 14);
MESON_CLK_GATE_SYS_CLK(sys_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 15);
MESON_CLK_GATE_SYS_CLK(sys_rom, CLKCTRL_SYS_CLK_EN0_REG0, 16);
MESON_CLK_GATE_SYS_CLK(sys_uart_f, CLKCTRL_SYS_CLK_EN0_REG0, 17);
MESON_CLK_GATE_SYS_CLK(sys_cpu_apb, CLKCTRL_SYS_CLK_EN0_REG0, 18);
MESON_CLK_GATE_SYS_CLK(sys_sar_adc, CLKCTRL_SYS_CLK_EN0_REG0, 20);
MESON_CLK_GATE_SYS_CLK(sys_startup, CLKCTRL_SYS_CLK_EN0_REG0, 21);
MESON_CLK_GATE_SYS_CLK(sys_secure, CLKCTRL_SYS_CLK_EN0_REG0, 22);
MESON_CLK_GATE_SYS_CLK(sys_amfc, CLKCTRL_SYS_CLK_EN0_REG0, 24);
MESON_CLK_GATE_SYS_CLK(sys_nna, CLKCTRL_SYS_CLK_EN0_REG0, 25);
MESON_CLK_GATE_SYS_CLK(sys_eth_mac, CLKCTRL_SYS_CLK_EN0_REG0, 26);
MESON_CLK_GATE_SYS_CLK(sys_gic, CLKCTRL_SYS_CLK_EN0_REG0, 27);
MESON_CLK_GATE_SYS_CLK(sys_rama, CLKCTRL_SYS_CLK_EN0_REG0, 28);
MESON_CLK_GATE_SYS_CLK(sys_big_nic, CLKCTRL_SYS_CLK_EN0_REG0, 29);
MESON_CLK_GATE_SYS_CLK(sys_audio_vad, CLKCTRL_SYS_CLK_EN0_REG0, 30);
MESON_CLK_GATE_SYS_CLK(sys_audio, CLKCTRL_SYS_CLK_EN0_REG0, 31);
MESON_CLK_GATE_SYS_CLK(sys_pwm_a, CLKCTRL_SYS_CLK_EN0_REG1, 0);

#define MESON_CLK_GATE_HW(_name, _reg, _bit, _parent_hw)              \
struct clk_regmap _name = {                                           \
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

MESON_CLK_GATE_HW(sys_pwm_b, CLKCTRL_SYS_CLK_EN0_REG1, 1, sys_pwm_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_usb, CLKCTRL_SYS_CLK_EN0_REG1, 2);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_a, CLKCTRL_SYS_CLK_EN0_REG1, 3);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_c, CLKCTRL_SYS_CLK_EN0_REG1, 4);
MESON_CLK_GATE_HW(sys_pwm_c, CLKCTRL_SYS_CLK_EN0_REG1, 5, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_d, CLKCTRL_SYS_CLK_EN0_REG1, 6, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_e, CLKCTRL_SYS_CLK_EN0_REG1, 7, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_f, CLKCTRL_SYS_CLK_EN0_REG1, 8, sys_pwm_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_pio_core, CLKCTRL_SYS_CLK_EN0_REG1, 9);
MESON_CLK_GATE_SYS_CLK(sys_spicc, CLKCTRL_SYS_CLK_EN0_REG1, 10);
MESON_CLK_GATE_SYS_CLK(sys_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 11);
MESON_CLK_GATE_SYS_CLK(sys_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 12);
MESON_CLK_GATE_SYS_CLK(sys_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 13);
MESON_CLK_GATE_SYS_CLK(sys_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 14);
MESON_CLK_GATE_SYS_CLK(sys_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 15);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_HW(sys_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 17, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG1, 18, sys_i2c_m_a.hw);
MESON_CLK_GATE_HW(sys_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG1, 19, sys_i2c_m_a.hw);
MESON_CLK_GATE_SYS_CLK(sys_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_eth_phy, CLKCTRL_SYS_CLK_EN0_REG1, 28);
MESON_CLK_GATE_SYS_CLK(sys_acodec, CLKCTRL_SYS_CLK_EN0_REG1, 29);
MESON_CLK_GATE_SYS_CLK(sys_pwm_n, CLKCTRL_SYS_CLK_EN0_REG1, 30);
MESON_CLK_GATE_SYS_CLK(sys_dos, CLKCTRL_SYS_CLK_EN0_REG1, 31);
MESON_CLK_GATE_SYS_CLK(sys_cve, CLKCTRL_SYS_CLK_EN0_REG2, 0);
MESON_CLK_GATE_SYS_CLK(sys_isp_capu, CLKCTRL_SYS_CLK_EN0_REG2, 1);
MESON_CLK_GATE_SYS_CLK(sys_tahoe, CLKCTRL_SYS_CLK_EN0_REG2, 2);
MESON_CLK_GATE_SYS_CLK(sys_cve_intr, CLKCTRL_SYS_CLK_EN0_REG2, 3);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_b, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_HW(sys_pwm_g, CLKCTRL_SYS_CLK_EN0_REG2, 5, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_h, CLKCTRL_SYS_CLK_EN0_REG2, 6, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_i, CLKCTRL_SYS_CLK_EN0_REG2, 7, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_j, CLKCTRL_SYS_CLK_EN0_REG2, 8, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_k, CLKCTRL_SYS_CLK_EN0_REG2, 9, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_l, CLKCTRL_SYS_CLK_EN0_REG2, 10, sys_pwm_a.hw);
MESON_CLK_GATE_HW(sys_pwm_m, CLKCTRL_SYS_CLK_EN0_REG2, 11, sys_pwm_a.hw);

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

MESON_CLK_GATE_AXI_CLK(axi_sys_nic, CLKCTRL_AXI_CLK_EN0, 2);
MESON_CLK_GATE_AXI_CLK(axi_rama, CLKCTRL_AXI_CLK_EN0, 6);
MESON_CLK_GATE_AXI_CLK(axi_mux_nic, CLKCTRL_AXI_CLK_EN0, 10);
MESON_CLK_GATE_AXI_CLK(axi_dev_dmc, CLKCTRL_AXI_CLK_EN0, 13);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&mclk_pll,
	&fclk_50m,
	&fclk_div2,
	&fclk_div2p5,
	&fclk_div3,
	&fclk_div4,
	&fclk_div5,
	&fclk_div7,
	&mclk_pll_clk,
	&mclk0_mux,
	&mclk0_div,
	&mclk0,
	&mclk1_mux,
	&mclk1_div,
	&mclk1
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
	&nna_mux,
	&nna_div,
	&nna,
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
	&spicc0_mux,
	&spicc0_div,
	&spicc0,
	&spicc1_mux,
	&spicc1_div,
	&spicc1,
	&sd_emmc_a_mux,
	&sd_emmc_a_div,
	&sd_emmc_a,
	&sd_emmc_b_mux,
	&sd_emmc_b_div,
	&sd_emmc_b,
	&sd_emmc_c_mux,
	&sd_emmc_c_div,
	&sd_emmc_c,
	&eth_rmii_mux,
	&eth_rmii_div,
	&eth_rmii,
	&eth_125m,
	&ts_div,
	&ts,
	&hcodec_0_mux,
	&hcodec_0_div,
	&hcodec_0,
	&hcodec_1_mux,
	&hcodec_1_div,
	&hcodec_1,
	&hcodec,
	&tahoe_mux,
	&tahoe_div,
	&tahoe,
	&cve_mux,
	&cve_div,
	&cve,
	&cve_arb_mux,
	&cve_arb_div,
	&cve_arb,
	&ge2d_mux,
	&ge2d_div,
	&ge2d,
	&dewarp_mux,
	&dewarp_div,
	&dewarp,
	&isp_mux,
	&isp_div,
	&isp,
	&csi_phy_mux,
	&csi_phy_div,
	&csi_phy,
	&amfc_mux,
	&amfc_div,
	&amfc,
	&clk_24m_in,
	&clk_12m,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_clk_ctrl,
	&sys_reset_ctrl,
	&sys_analog_ctrl,
	&sys_pwr_ctrl,
	&sys_pad_ctrl,
	&sys_sys_ctrl,
	&sys_ts_pll,
	&sys_mmc_pclk,
	&sys_capu,
	&sys_cpu_ctrl,
	&sys_jtag_ctrl,
	&sys_irq_ctrl,
	&sys_msr_clk,
	&sys_rom,
	&sys_uart_f,
	&sys_cpu_apb,
	&sys_sar_adc,
	&sys_startup,
	&sys_secure,
	&sys_amfc,
	&sys_nna,
	&sys_eth_mac,
	&sys_gic,
	&sys_rama,
	&sys_big_nic,
	&sys_audio_vad,
	&sys_audio,
	&sys_pwm_a,
	&sys_pwm_b,
	&sys_usb,
	&sys_sd_emmc_a,
	&sys_sd_emmc_c,
	&sys_pwm_c,
	&sys_pwm_d,
	&sys_pwm_e,
	&sys_pwm_f,
	&sys_pio_core,
	&sys_spicc,
	&sys_uart_a,
	&sys_uart_b,
	&sys_uart_c,
	&sys_uart_d,
	&sys_uart_e,
	&sys_i2c_m_a,
	&sys_i2c_m_b,
	&sys_i2c_m_c,
	&sys_i2c_m_d,
	&sys_i2c_s_a,
	&sys_eth_phy,
	&sys_acodec,
	&sys_pwm_n,
	&sys_dos,
	&sys_cve,
	&sys_isp_capu,
	&sys_tahoe,
	&sys_cve_intr,
	&sys_sd_emmc_b,
	&sys_pwm_g,
	&sys_pwm_h,
	&sys_pwm_i,
	&sys_pwm_j,
	&sys_pwm_k,
	&sys_pwm_l,
	&sys_pwm_m,
	&axi_sys_nic,
	&axi_rama,
	&axi_mux_nic,
	&axi_dev_dmc
};

static struct clk_hw *c4_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_MCLK_PLL]              = &mclk_pll.hw,
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
	[CLKID_MCLK_PLL_CLK]          = &mclk_pll_clk.hw,
	[CLKID_MCLK0_MUX]             = &mclk0_mux.hw,
	[CLKID_MCLK0_DIV]             = &mclk0_div.hw,
	[CLKID_MCLK0]                 = &mclk0.hw,
	[CLKID_MCLK1_MUX]             = &mclk1_mux.hw,
	[CLKID_MCLK1_DIV]             = &mclk1_div.hw,
	[CLKID_MCLK1]                 = &mclk1.hw,
	[CLKID_RTC_DUAL_CLKIN]        = &rtc_dual_clkin.hw,
	[CLKID_RTC_DUAL_DIV]          = &rtc_dual_div.hw,
	[CLKID_RTC_DUAL_MUX]          = &rtc_dual_mux.hw,
	[CLKID_RTC_DUAL]              = &rtc_dual.hw,
	[CLKID_RTC_CLK]               = &rtc_clk.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_NNA_MUX]               = &nna_mux.hw,
	[CLKID_NNA_DIV]               = &nna_div.hw,
	[CLKID_NNA]                   = &nna.hw,
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
	[CLKID_SPICC0_MUX]            = &spicc0_mux.hw,
	[CLKID_SPICC0_DIV]            = &spicc0_div.hw,
	[CLKID_SPICC0]                = &spicc0.hw,
	[CLKID_SPICC1_MUX]            = &spicc1_mux.hw,
	[CLKID_SPICC1_DIV]            = &spicc1_div.hw,
	[CLKID_SPICC1]                = &spicc1.hw,
	[CLKID_SD_EMMC_A_MUX]         = &sd_emmc_a_mux.hw,
	[CLKID_SD_EMMC_A_DIV]         = &sd_emmc_a_div.hw,
	[CLKID_SD_EMMC_A]             = &sd_emmc_a.hw,
	[CLKID_SD_EMMC_B_MUX]         = &sd_emmc_b_mux.hw,
	[CLKID_SD_EMMC_B_DIV]         = &sd_emmc_b_div.hw,
	[CLKID_SD_EMMC_B]             = &sd_emmc_b.hw,
	[CLKID_SD_EMMC_C_MUX]         = &sd_emmc_c_mux.hw,
	[CLKID_SD_EMMC_C_DIV]         = &sd_emmc_c_div.hw,
	[CLKID_SD_EMMC_C]             = &sd_emmc_c.hw,
	[CLKID_ETH_RMII_MUX]          = &eth_rmii_mux.hw,
	[CLKID_ETH_RMII_DIV]          = &eth_rmii_div.hw,
	[CLKID_ETH_RMII]              = &eth_rmii.hw,
	[CLKID_ETH_125M_DIV]          = &eth_125m_div.hw,
	[CLKID_ETH_125M]              = &eth_125m.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_HCODEC_0_MUX]          = &hcodec_0_mux.hw,
	[CLKID_HCODEC_0_DIV]          = &hcodec_0_div.hw,
	[CLKID_HCODEC_0]              = &hcodec_0.hw,
	[CLKID_HCODEC_1_MUX]          = &hcodec_1_mux.hw,
	[CLKID_HCODEC_1_DIV]          = &hcodec_1_div.hw,
	[CLKID_HCODEC_1]              = &hcodec_1.hw,
	[CLKID_HCODEC]                = &hcodec.hw,
	[CLKID_TAHOE_MUX]             = &tahoe_mux.hw,
	[CLKID_TAHOE_DIV]             = &tahoe_div.hw,
	[CLKID_TAHOE]                 = &tahoe.hw,
	[CLKID_CVE_MUX]               = &cve_mux.hw,
	[CLKID_CVE_DIV]               = &cve_div.hw,
	[CLKID_CVE]                   = &cve.hw,
	[CLKID_CVE_ARB_MUX]           = &cve_arb_mux.hw,
	[CLKID_CVE_ARB_DIV]           = &cve_arb_div.hw,
	[CLKID_CVE_ARB]               = &cve_arb.hw,
	[CLKID_GE2D_MUX]              = &ge2d_mux.hw,
	[CLKID_GE2D_DIV]              = &ge2d_div.hw,
	[CLKID_GE2D]                  = &ge2d.hw,
	[CLKID_DEWARP_MUX]            = &dewarp_mux.hw,
	[CLKID_DEWARP_DIV]            = &dewarp_div.hw,
	[CLKID_DEWARP]                = &dewarp.hw,
	[CLKID_ISP_MUX]               = &isp_mux.hw,
	[CLKID_ISP_DIV]               = &isp_div.hw,
	[CLKID_ISP]                   = &isp.hw,
	[CLKID_CSI_PHY_MUX]           = &csi_phy_mux.hw,
	[CLKID_CSI_PHY_DIV]           = &csi_phy_div.hw,
	[CLKID_CSI_PHY]               = &csi_phy.hw,
	[CLKID_AMFC_MUX]              = &amfc_mux.hw,
	[CLKID_AMFC_DIV]              = &amfc_div.hw,
	[CLKID_AMFC]                  = &amfc.hw,
	[CLKID_CLK_24M_IN]            = &clk_24m_in.hw,
	[CLKID_CLK_12M_DIV]           = &clk_12m_div.hw,
	[CLKID_CLK_12M]               = &clk_12m.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_CLK_CTRL]          = &sys_clk_ctrl.hw,
	[CLKID_SYS_RESET_CTRL]        = &sys_reset_ctrl.hw,
	[CLKID_SYS_ANALOG_CTRL]       = &sys_analog_ctrl.hw,
	[CLKID_SYS_PWR_CTRL]          = &sys_pwr_ctrl.hw,
	[CLKID_SYS_PAD_CTRL]          = &sys_pad_ctrl.hw,
	[CLKID_SYS_SYS_CTRL]          = &sys_sys_ctrl.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_MMC_PCLK]          = &sys_mmc_pclk.hw,
	[CLKID_SYS_CAPU]              = &sys_capu.hw,
	[CLKID_SYS_CPU_CTRL]          = &sys_cpu_ctrl.hw,
	[CLKID_SYS_JTAG_CTRL]         = &sys_jtag_ctrl.hw,
	[CLKID_SYS_IRQ_CTRL]          = &sys_irq_ctrl.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_ROM]               = &sys_rom.hw,
	[CLKID_SYS_UART_F]            = &sys_uart_f.hw,
	[CLKID_SYS_CPU_APB]           = &sys_cpu_apb.hw,
	[CLKID_SYS_SAR_ADC]           = &sys_sar_adc.hw,
	[CLKID_SYS_STARTUP]           = &sys_startup.hw,
	[CLKID_SYS_SECURE]            = &sys_secure.hw,
	[CLKID_SYS_AMFC]              = &sys_amfc.hw,
	[CLKID_SYS_NNA]               = &sys_nna.hw,
	[CLKID_SYS_ETH_MAC]           = &sys_eth_mac.hw,
	[CLKID_SYS_GIC]               = &sys_gic.hw,
	[CLKID_SYS_RAMA]              = &sys_rama.hw,
	[CLKID_SYS_BIG_NIC]           = &sys_big_nic.hw,
	[CLKID_SYS_AUDIO_VAD]         = &sys_audio_vad.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_PWM_A]             = &sys_pwm_a.hw,
	[CLKID_SYS_PWM_B]             = &sys_pwm_b.hw,
	[CLKID_SYS_USB]               = &sys_usb.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_PWM_C]             = &sys_pwm_c.hw,
	[CLKID_SYS_PWM_D]             = &sys_pwm_d.hw,
	[CLKID_SYS_PWM_E]             = &sys_pwm_e.hw,
	[CLKID_SYS_PWM_F]             = &sys_pwm_f.hw,
	[CLKID_SYS_PIO_CORE]          = &sys_pio_core.hw,
	[CLKID_SYS_SPICC]             = &sys_spicc.hw,
	[CLKID_SYS_UART_A]            = &sys_uart_a.hw,
	[CLKID_SYS_UART_B]            = &sys_uart_b.hw,
	[CLKID_SYS_UART_C]            = &sys_uart_c.hw,
	[CLKID_SYS_UART_D]            = &sys_uart_d.hw,
	[CLKID_SYS_UART_E]            = &sys_uart_e.hw,
	[CLKID_SYS_I2C_M_A]           = &sys_i2c_m_a.hw,
	[CLKID_SYS_I2C_M_B]           = &sys_i2c_m_b.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_S_A]           = &sys_i2c_s_a.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_PWM_N]             = &sys_pwm_n.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_CVE]               = &sys_cve.hw,
	[CLKID_SYS_ISP_CAPU]          = &sys_isp_capu.hw,
	[CLKID_SYS_TAHOE]             = &sys_tahoe.hw,
	[CLKID_SYS_CVE_INTR]          = &sys_cve_intr.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_PWM_G]             = &sys_pwm_g.hw,
	[CLKID_SYS_PWM_H]             = &sys_pwm_h.hw,
	[CLKID_SYS_PWM_I]             = &sys_pwm_i.hw,
	[CLKID_SYS_PWM_J]             = &sys_pwm_j.hw,
	[CLKID_SYS_PWM_K]             = &sys_pwm_k.hw,
	[CLKID_SYS_PWM_L]             = &sys_pwm_l.hw,
	[CLKID_SYS_PWM_M]             = &sys_pwm_m.hw,
	[CLKID_AXI_SYS_NIC]           = &axi_sys_nic.hw,
	[CLKID_AXI_RAMA]              = &axi_rama.hw,
	[CLKID_AXI_MUX_NIC]           = &axi_mux_nic.hw,
	[CLKID_AXI_DEV_DMC]           = &axi_dev_dmc.hw
};

static const struct meson_clk_hw_data c4_clks = {
	.hws = c4_hw_clks,
	.num = ARRAY_SIZE(c4_hw_clks),
};

static int meson_c4_syscore_suspend(void)
{
	if (ee_support_powerdown())
		return clk_save_context();

	return 0;
}

static void meson_c4_syscore_resume(void)
{
	if (ee_support_powerdown())
		clk_restore_context();
}

static struct syscore_ops meson_c4_syscore_ops = {
	.suspend	= meson_c4_syscore_suspend,
	.resume		= meson_c4_syscore_resume,
};

static int meson_c4_probe(struct platform_device *pdev)
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

	register_syscore_ops(&meson_c4_syscore_ops);

	return devm_of_clk_add_hw_provider(dev, meson_clk_hw_get,
					   (void *)data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,c4-clkc",
		.data = &c4_clks
	},
	{}
};

static struct platform_driver c4_driver = {
	.probe			= meson_c4_probe,
	.driver			= {
		.name		= "c4-clkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init c4_clk_init(void)
{
	return platform_driver_register(&c4_driver);
}

void __exit c4_clk_exit(void)
{
	platform_driver_unregister(&c4_driver);
}
#else
builtin_platform_driver(c4_driver);
MODULE_LICENSE("GPL v2");
#endif
