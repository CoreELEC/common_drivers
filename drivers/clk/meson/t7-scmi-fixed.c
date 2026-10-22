// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Copyright (c) 2026 Team CoreELEC
 *
 * Read-only T7 base clocks on the SCMI clock node, for a BL31 without the
 * SCMI clock protocol. The fclk gates and sys_clk carry the names the
 * audio clocks use as parent names.
 */

#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "clk-pll.h"
#include "clk-regmap.h"
#include "t7.h"
#include <dt-bindings/clock/amlogic,t7-scmi-clkc.h>

static struct clk_regmap t7_fw_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_FIXPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.l = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "t7_fw_fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t7_fw_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_FIXPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "t7_fw_fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fw_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

#define T7_FW_FCLK(_name, _clk_name, _mult, _divider, _bit)		\
static struct clk_fixed_factor t7_fw_##_name##_div = {			\
	.mult = _mult,							\
	.div = _divider,						\
	.hw.init = &(struct clk_init_data){				\
		.name = "t7_fw_" #_name "_div",				\
		.ops = &clk_fixed_factor_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t7_fw_fixed_pll.hw				\
		},							\
		.num_parents = 1,					\
	},								\
};									\
									\
static struct clk_regmap t7_fw_##_name = {				\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = ANACTRL_FIXPLL_CTRL1,				\
		.bit_idx = _bit,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = _clk_name,					\
		.ops = &clk_regmap_gate_ro_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t7_fw_##_name##_div.hw				\
		},							\
		.num_parents = 1,					\
	},								\
}

T7_FW_FCLK(fclk_div2, "fdiv2", 1, 2, 24);
T7_FW_FCLK(fclk_div2p5, "fdiv2p5", 2, 5, 25);
T7_FW_FCLK(fclk_div3, "fdiv3", 1, 3, 20);
T7_FW_FCLK(fclk_div4, "fdiv4", 1, 4, 21);
T7_FW_FCLK(fclk_div5, "fdiv5", 1, 5, 22);
T7_FW_FCLK(fclk_div7, "fdiv7", 1, 7, 23);

static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 5, 7 };
static const struct clk_parent_data t7_fw_sys_ab_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t7_fw_fclk_div2.hw },
	{ .hw = &t7_fw_fclk_div3.hw },
	{ .hw = &t7_fw_fclk_div4.hw },
	{ .hw = &t7_fw_fclk_div5.hw },
	{ .fw_name = "axi_clk_frcpu", },
	{ .fw_name = "rtc", },
};

#define T7_FW_SYSCLK(_ab, _sel_shift, _div_shift, _gate_bit)		\
static struct clk_regmap t7_fw_sysclk_##_ab##_sel = {			\
	.data = &(struct clk_regmap_mux_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.mask = 0x7,						\
		.shift = _sel_shift,					\
		.table = mux_table_sys_ab_clk_sel,			\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "t7_fw_sysclk_" #_ab "_sel",			\
		.ops = &clk_regmap_mux_ro_ops,				\
		.parent_data = t7_fw_sys_ab_clk_sel,			\
		.num_parents = ARRAY_SIZE(t7_fw_sys_ab_clk_sel),	\
	},								\
};									\
									\
static struct clk_regmap t7_fw_sysclk_##_ab##_div = {			\
	.data = &(struct clk_regmap_div_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.shift = _div_shift,					\
		.width = 10,						\
		.flags = CLK_DIVIDER_READ_ONLY,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "t7_fw_sysclk_" #_ab "_div",			\
		.ops = &clk_regmap_divider_ro_ops,			\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t7_fw_sysclk_##_ab##_sel.hw			\
		},							\
		.num_parents = 1,					\
	},								\
};									\
									\
static struct clk_regmap t7_fw_sysclk_##_ab = {				\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.bit_idx = _gate_bit,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "t7_fw_sysclk_" #_ab,				\
		.ops = &clk_regmap_gate_ro_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t7_fw_sysclk_##_ab##_div.hw			\
		},							\
		.num_parents = 1,					\
	},								\
}

T7_FW_SYSCLK(a, 10, 0, 13);
T7_FW_SYSCLK(b, 26, 16, 29);

static struct clk_regmap t7_fw_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t7_fw_sysclk_a.hw,
			&t7_fw_sysclk_b.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_regmap *const t7_fw_pll_regmaps[] = {
	&t7_fw_fixed_pll_dco,
	&t7_fw_fixed_pll,
	&t7_fw_fclk_div2,
	&t7_fw_fclk_div2p5,
	&t7_fw_fclk_div3,
	&t7_fw_fclk_div4,
	&t7_fw_fclk_div5,
	&t7_fw_fclk_div7,
};

static struct clk_regmap *const t7_fw_sys_regmaps[] = {
	&t7_fw_sysclk_a_sel,
	&t7_fw_sysclk_a_div,
	&t7_fw_sysclk_a,
	&t7_fw_sysclk_b_sel,
	&t7_fw_sysclk_b_div,
	&t7_fw_sysclk_b,
	&t7_fw_sys_clk,
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const t7_fw_order[] = {
	&t7_fw_fixed_pll_dco.hw,
	&t7_fw_fixed_pll.hw,
	&t7_fw_fclk_div2_div.hw,
	&t7_fw_fclk_div2.hw,
	&t7_fw_fclk_div2p5_div.hw,
	&t7_fw_fclk_div2p5.hw,
	&t7_fw_fclk_div3_div.hw,
	&t7_fw_fclk_div3.hw,
	&t7_fw_fclk_div4_div.hw,
	&t7_fw_fclk_div4.hw,
	&t7_fw_fclk_div5_div.hw,
	&t7_fw_fclk_div5.hw,
	&t7_fw_fclk_div7_div.hw,
	&t7_fw_fclk_div7.hw,
	&t7_fw_sysclk_a_sel.hw,
	&t7_fw_sysclk_a_div.hw,
	&t7_fw_sysclk_a.hw,
	&t7_fw_sysclk_b_sel.hw,
	&t7_fw_sysclk_b_div.hw,
	&t7_fw_sysclk_b.hw,
	&t7_fw_sys_clk.hw,
};

static struct clk_hw *t7_fw_hws[] = {
	[CLKID_FIXED_PLL_DCO]		= &t7_fw_fixed_pll_dco.hw,
	[CLKID_FIXED_PLL]		= &t7_fw_fixed_pll.hw,
	[CLKID_SYS_CLK]			= &t7_fw_sys_clk.hw,
	[CLKID_FCLK_DIV2_DIV]		= &t7_fw_fclk_div2_div.hw,
	[CLKID_FCLK_DIV2]		= &t7_fw_fclk_div2.hw,
	[CLKID_FCLK_DIV2P5_DIV]		= &t7_fw_fclk_div2p5_div.hw,
	[CLKID_FCLK_DIV2P5]		= &t7_fw_fclk_div2p5.hw,
	[CLKID_FCLK_DIV3_DIV]		= &t7_fw_fclk_div3_div.hw,
	[CLKID_FCLK_DIV3]		= &t7_fw_fclk_div3.hw,
	[CLKID_FCLK_DIV4_DIV]		= &t7_fw_fclk_div4_div.hw,
	[CLKID_FCLK_DIV4]		= &t7_fw_fclk_div4.hw,
	[CLKID_FCLK_DIV5_DIV]		= &t7_fw_fclk_div5_div.hw,
	[CLKID_FCLK_DIV5]		= &t7_fw_fclk_div5.hw,
	[CLKID_FCLK_DIV7_DIV]		= &t7_fw_fclk_div7_div.hw,
	[CLKID_FCLK_DIV7]		= &t7_fw_fclk_div7.hw,
};

/*
 * IDs not served here stay with the SCMI provider: -EPROBE_DEFER lets the
 * lookup move on to it, or wait for it like without this provider
 */
static struct clk_hw *t7_fw_clk_get(struct of_phandle_args *clkspec,
				    void *data)
{
	unsigned int idx = clkspec->args[0];

	if (idx >= ARRAY_SIZE(t7_fw_hws) || !t7_fw_hws[idx])
		return ERR_PTR(-EPROBE_DEFER);

	return t7_fw_hws[idx];
}

static const struct regmap_config t7_fw_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct regmap *__init t7_fw_regmap(struct device_node *clkc,
					  int index)
{
	void __iomem *base = of_iomap(clkc, index);

	if (!base)
		return ERR_PTR(-ENOMEM);

	return regmap_init_mmio(NULL, base, &t7_fw_regmap_config);
}

static void __init t7_fw_clk_init(struct device_node *np)
{
	struct device_node *clkc;
	struct regmap *pll_map, *sys_map;
	unsigned int i;
	int ret;

	clkc = of_find_compatible_node(NULL, NULL, "amlogic,t7-clkc");
	if (!clkc)
		return;

	sys_map = t7_fw_regmap(clkc, 0);
	pll_map = t7_fw_regmap(clkc, 1);
	of_node_put(clkc);
	if (IS_ERR(sys_map) || IS_ERR(pll_map)) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(t7_fw_pll_regmaps); i++)
		t7_fw_pll_regmaps[i]->map = pll_map;
	for (i = 0; i < ARRAY_SIZE(t7_fw_sys_regmaps); i++)
		t7_fw_sys_regmaps[i]->map = sys_map;

	for (i = 0; i < ARRAY_SIZE(t7_fw_order); i++) {
		ret = of_clk_hw_register(np, t7_fw_order[i]);
		if (ret)
			goto err;
	}

	ret = of_clk_add_hw_provider(np, t7_fw_clk_get, NULL);
	if (!ret)
		return;
err:
	pr_err("%pOF: failed to register clocks: %d\n", np, ret);
}

CLK_OF_DECLARE(t7_scmi_fixed, "amlogic,t7-scmi-fixed-clkc", t7_fw_clk_init);
