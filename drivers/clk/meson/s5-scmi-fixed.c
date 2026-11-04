// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Copyright (c) 2026 Team CoreELEC
 *
 * Read-only S5 base clocks on the SCMI clock node, for a BL31 without the
 * SCMI clock protocol. sys_clk carries the name the audio clocks use as
 * parent name, the clock controller builds the fclk gates on top of
 * fixed_pll itself. The a55, a76 and dsu clocks go through the legacy
 * secure calls of the 5.15 S5 driver.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "clk-cpu-dyndiv.h"
#include "clk-pll.h"
#include "clk-regmap.h"
#include <dt-bindings/clock/amlogic,s5-scmi-clkc.h>

#define CLKCTRL_SYS_CLK_CTRL0		(0x0010 << 2)
#define CLKCTRL_FIXPLL_CTRL0		(0x0220 << 2)
#define CLKCTRL_FIXPLL_STS		(0x0227 << 2)
#define CLKCTRL_SYS0PLL_CTRL0		(0x0200 << 2)
#define CLKCTRL_SYS0PLL_STS		(0x0204 << 2)
#define CLKCTRL_SYS1PLL_CTRL0		(0x0210 << 2)
#define CLKCTRL_SYS1PLL_STS		(0x0214 << 2)
#define CLKCTRL_GP1PLL_CTRL0		(0x0280 << 2)
#define CLKCTRL_GP1PLL_STS		(0x0284 << 2)

#define SECURE_PLL_CLK			0x82000098
#define SECURE_CPU_CLK			0x82000099

/* sec ids of the legacy calls, from the 5.15 s5.h */
enum {
	S5_SECID_SYS0_DCO_PLL = 0,
	S5_SECID_SYS0_DCO_PLL_DIS = 1,
	S5_SECID_SYS0_PLL_OD = 2,
	S5_SECID_SYS1_DCO_PLL = 3,
	S5_SECID_SYS1_DCO_PLL_DIS = 4,
	S5_SECID_SYS1_PLL_OD = 5,
	S5_SECID_GP1_DCO_PLL = 6,
	S5_SECID_GP1_DCO_PLL_DIS = 7,
	S5_SECID_GP1_PLL_OD = 8,
	S5_SECID_CPU_CLK_SEL = 9,
	S5_SECID_CPU_CLK_RD = 10,
	S5_SECID_CPU_CLK_DYN = 11,
	S5_SECID_A76_CLK_SEL = 12,
	S5_SECID_A76_CLK_RD = 13,
	S5_SECID_A76_CLK_DYN = 14,
	S5_SECID_DSU_CLK_SEL = 15,
	S5_SECID_DSU_CLK_RD = 16,
	S5_SECID_DSU_CLK_DYN = 17,
};

static struct clk_regmap s5_fw_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_FIXPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "s5_fw_fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fw_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_FIXPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "s5_fw_fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fw_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

#define S5_FW_FCLK(_name, _divider)					\
static struct clk_fixed_factor s5_fw_##_name = {			\
	.mult = 1,							\
	.div = _divider,						\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name,				\
		.ops = &clk_fixed_factor_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_fixed_pll.hw				\
		},							\
		.num_parents = 1,					\
	},								\
}

S5_FW_FCLK(fclk_div2, 2);
S5_FW_FCLK(fclk_div3, 3);
S5_FW_FCLK(fclk_div4, 4);
S5_FW_FCLK(fclk_div5, 5);

static u32 mux_table_sys_clk_sel[] = { 0, 1, 2, 3, 4 };
static const struct clk_parent_data s5_fw_sys_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fw_fclk_div2.hw },
	{ .hw = &s5_fw_fclk_div3.hw },
	{ .hw = &s5_fw_fclk_div4.hw },
	{ .hw = &s5_fw_fclk_div5.hw },
};

#define S5_FW_SYSCLK(_n, _sel_shift, _div_shift, _gate_bit)		\
static struct clk_regmap s5_fw_sysclk_##_n##_sel = {			\
	.data = &(struct clk_regmap_mux_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.mask = 0x7,						\
		.shift = _sel_shift,					\
		.table = mux_table_sys_clk_sel,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n "_sel",			\
		.ops = &clk_regmap_mux_ro_ops,				\
		.parent_data = s5_fw_sys_clk_sel,			\
		.num_parents = ARRAY_SIZE(s5_fw_sys_clk_sel),		\
	},								\
};									\
									\
static struct clk_regmap s5_fw_sysclk_##_n##_div = {			\
	.data = &(struct clk_regmap_div_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.shift = _div_shift,					\
		.width = 10,						\
		.flags = CLK_DIVIDER_READ_ONLY,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n "_div",			\
		.ops = &clk_regmap_divider_ro_ops,			\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_sysclk_##_n##_sel.hw			\
		},							\
		.num_parents = 1,					\
	},								\
};									\
									\
static struct clk_regmap s5_fw_sysclk_##_n = {				\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.bit_idx = _gate_bit,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n,				\
		.ops = &clk_regmap_gate_ro_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_sysclk_##_n##_div.hw			\
		},							\
		.num_parents = 1,					\
	},								\
}

S5_FW_SYSCLK(0, 10, 0, 13);
S5_FW_SYSCLK(1, 26, 16, 29);

static struct clk_regmap s5_fw_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fw_sysclk_0.hw,
			&s5_fw_sysclk_1.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_fixed_factor s5_fw_fclk_div2p5 = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "s5_fw_fclk_div2p5",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fw_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table s5_fw_sys_pll_params_table[] = {
	PLL_PARAMS(100, 1), /* DCO=2400M OD=1200M */
	PLL_PARAMS(116, 1), /* DCO=2784M OD=1392M */
	PLL_PARAMS(126, 1), /* DCO=3024M OD=1512M */
	PLL_PARAMS(67, 1), /* DCO=1608M OD=1608M */
	PLL_PARAMS(71, 1), /* DCO=1704M OD=1704M */
	PLL_PARAMS(75, 1), /* DCO=1800M OD=1800M */
	PLL_PARAMS(79, 1), /* DCO=1896M OD=1896M */
	PLL_PARAMS(84, 1), /* DCO=2016M OD=2016M */
	{ /* sentinel */ }
};

static const struct pll_params_table s5_fw_sys1_pll_params_table[] = {
	PLL_PARAMS(100, 1), /* DCO=2400M OD=1200M */
	PLL_PARAMS(116, 1), /* DCO=2784M OD=1392M */
	PLL_PARAMS(126, 1), /* DCO=3024M OD=1512M */
	PLL_PARAMS(67, 1), /* DCO=1608M OD=1608M */
	PLL_PARAMS(71, 1), /* DCO=1704M OD=1704M */
	PLL_PARAMS(75, 1), /* DCO=1800M OD=1800M */
	PLL_PARAMS(79, 1), /* DCO=1896M OD=1896M */
	PLL_PARAMS(84, 1), /* DCO=2016M OD=2016M */
	PLL_PARAMS(87, 1), /* DCO=2088M OD=2088M */
	{ /* sentinel */ }
};

static const struct pll_params_table s5_fw_gp1_pll_params_table[] = {
	PLL_PARAMS(100, 1), /* DCO=2400M OD=1200M */
	PLL_PARAMS(116, 1), /* DCO=2784M OD=1392M */
	PLL_PARAMS(125, 1), /* DCO=3000M OD=1500M */
	PLL_PARAMS(67, 1), /* DCO=1608M OD=1608M */
	PLL_PARAMS(71, 1), /* DCO=1704M OD=1704M */
	PLL_PARAMS(75, 1), /* DCO=1800M OD=1800M */
	PLL_PARAMS(79, 1), /* DCO=1896M OD=1896M */
	PLL_PARAMS(81, 1), /* DCO=1944M OD=1944M */
	{ /* sentinel */ }
};

static const struct clk_div_table s5_fw_pll_od_table[] = {
	{ 0, 1 },
	{ 1, 2 },
	{ 2, 4 },
	{ 3, 8 },
	{ 4, 16 },
	{ /* sentinel */ }
};

#define S5_FW_CPU_PLL(_name, _ctrl, _sts, _table, _secid_dco, _secid_dis, \
		      _secid_od)					\
static struct clk_regmap s5_fw_##_name##_dco = {			\
	.data = &(struct meson_clk_pll_data){				\
		.en = {							\
			.reg_off = _ctrl,				\
			.shift   = 28,					\
			.width   = 1,					\
		},							\
		.m = {							\
			.reg_off = _ctrl,				\
			.shift   = 0,					\
			.width   = 8,					\
		},							\
		.n = {							\
			.reg_off = _ctrl,				\
			.shift   = 16,					\
			.width   = 5,					\
		},							\
		.table = _table,					\
		.l = {							\
			.reg_off = _sts,				\
			.shift   = 31,					\
			.width   = 1,					\
		},							\
		.rst = {						\
			.reg_off = _ctrl,				\
			.shift   = 29,					\
			.width   = 1,					\
		},							\
		.smc_id = SECURE_PLL_CLK,				\
		.secid_disable = _secid_dis,				\
		.secid = _secid_dco,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name "_dco",				\
		.ops = &meson_secure_pll_v2_ops,			\
		.parent_data = &(const struct clk_parent_data) {	\
			.fw_name = "xtal",				\
		},							\
		.num_parents = 1,					\
		.flags = CLK_GET_RATE_NOCACHE | CLK_IS_CRITICAL,	\
	},								\
};									\
									\
static struct clk_regmap s5_fw_##_name = {				\
	.data = &(struct clk_regmap_div_data){				\
		.offset = _ctrl,					\
		.shift = 12,						\
		.width = 3,						\
		.table = s5_fw_pll_od_table,				\
		.smc_id = SECURE_PLL_CLK,				\
		.secid = _secid_od,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name,				\
		.ops = &clk_regmap_secure_v2_divider_ops,		\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_##_name##_dco.hw				\
		},							\
		.num_parents = 1,					\
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,	\
	},								\
}

S5_FW_CPU_PLL(sys_pll, CLKCTRL_SYS0PLL_CTRL0, CLKCTRL_SYS0PLL_STS,
	      s5_fw_sys_pll_params_table, S5_SECID_SYS0_DCO_PLL,
	      S5_SECID_SYS0_DCO_PLL_DIS, S5_SECID_SYS0_PLL_OD);
S5_FW_CPU_PLL(sys1_pll, CLKCTRL_SYS1PLL_CTRL0, CLKCTRL_SYS1PLL_STS,
	      s5_fw_sys1_pll_params_table, S5_SECID_SYS1_DCO_PLL,
	      S5_SECID_SYS1_DCO_PLL_DIS, S5_SECID_SYS1_PLL_OD);
S5_FW_CPU_PLL(gp1_pll, CLKCTRL_GP1PLL_CTRL0, CLKCTRL_GP1PLL_STS,
	      s5_fw_gp1_pll_params_table, S5_SECID_GP1_DCO_PLL,
	      S5_SECID_GP1_DCO_PLL_DIS, S5_SECID_GP1_PLL_OD);

static const struct cpu_dyn_table s5_fw_cpu_dyn_table[] = {
	CPU_LOW_PARAMS(24000000, 0, 0, 0),
	CPU_LOW_PARAMS(100000000, 1, 1, 9),
	CPU_LOW_PARAMS(250000000, 1, 1, 3),
	CPU_LOW_PARAMS(333333333, 2, 1, 1),
	CPU_LOW_PARAMS(500000000, 1, 1, 1),
	CPU_LOW_PARAMS(666666666, 2, 0, 0),
	CPU_LOW_PARAMS(1000000000, 1, 0, 0)
};

static const struct clk_parent_data s5_fw_cpu_dyn_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fw_fclk_div2.hw },
	{ .hw = &s5_fw_fclk_div3.hw },
	{ .hw = &s5_fw_fclk_div2p5.hw },
};

#define S5_FW_CPU_CLK(_name, _pll, _secid_sel, _secid_rd, _secid_dyn)	\
static struct clk_regmap s5_fw_##_name##_dyn = {			\
	.data = &(struct meson_clk_cpu_dyn_data){			\
		.table = s5_fw_cpu_dyn_table,				\
		.table_cnt = ARRAY_SIZE(s5_fw_cpu_dyn_table),		\
		.smc_id = SECURE_CPU_CLK,				\
		.secid_dyn_rd = _secid_rd,				\
		.secid_dyn = _secid_dyn,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name "_dyn",				\
		.ops = &meson_clk_cpu_dyn_ops,				\
		.parent_data = s5_fw_cpu_dyn_sel,			\
		.num_parents = ARRAY_SIZE(s5_fw_cpu_dyn_sel),		\
	},								\
};									\
									\
static struct clk_regmap s5_fw_##_name = {				\
	.data = &(struct clk_regmap_mux_data){				\
		.mask = 0x1,						\
		.shift = 11,						\
		.flags = CLK_MUX_ROUND_CLOSEST,				\
		.smc_id = SECURE_CPU_CLK,				\
		.secid = _secid_sel,					\
		.secid_rd = _secid_rd,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name,				\
		.ops = &clk_regmap_mux_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_##_name##_dyn.hw,			\
			&s5_fw_##_pll.hw,				\
		},							\
		.num_parents = 2,					\
		.flags = CLK_SET_RATE_PARENT,				\
	},								\
}

S5_FW_CPU_CLK(cpu_clk, sys_pll, S5_SECID_CPU_CLK_SEL, S5_SECID_CPU_CLK_RD,
	      S5_SECID_CPU_CLK_DYN);
S5_FW_CPU_CLK(a76_clk, sys1_pll, S5_SECID_A76_CLK_SEL, S5_SECID_A76_CLK_RD,
	      S5_SECID_A76_CLK_DYN);
S5_FW_CPU_CLK(dsu_clk, gp1_pll, S5_SECID_DSU_CLK_SEL, S5_SECID_DSU_CLK_RD,
	      S5_SECID_DSU_CLK_DYN);

struct s5_fw_pll_nb {
	struct notifier_block nb;
	struct clk_regmap *pll;
	struct clk_regmap *dyn;
	struct clk_regmap *mux;
};

/* run the cluster from the 1 GHz dyn path while its pll changes */
static int s5_fw_pll_cb(struct notifier_block *nb, unsigned long event,
			void *data)
{
	struct s5_fw_pll_nb *pnb = container_of(nb, struct s5_fw_pll_nb, nb);

	switch (event) {
	case PRE_RATE_CHANGE:
		if (clk_set_rate(pnb->dyn->hw.clk, 1000000000))
			pr_err("s5_fw: cannot set %s to 1 GHz\n",
			       clk_hw_get_name(&pnb->dyn->hw));
		clk_hw_set_parent(&pnb->mux->hw, &pnb->dyn->hw);
		udelay(5);
		return NOTIFY_OK;
	case POST_RATE_CHANGE:
		clk_hw_set_parent(&pnb->mux->hw, &pnb->pll->hw);
		udelay(5);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static struct s5_fw_pll_nb s5_fw_pll_nbs[] = {
	{
		.nb.notifier_call = s5_fw_pll_cb,
		.pll = &s5_fw_sys_pll,
		.dyn = &s5_fw_cpu_clk_dyn,
		.mux = &s5_fw_cpu_clk,
	},
	{
		.nb.notifier_call = s5_fw_pll_cb,
		.pll = &s5_fw_sys1_pll,
		.dyn = &s5_fw_a76_clk_dyn,
		.mux = &s5_fw_a76_clk,
	},
	{
		.nb.notifier_call = s5_fw_pll_cb,
		.pll = &s5_fw_gp1_pll,
		.dyn = &s5_fw_dsu_clk_dyn,
		.mux = &s5_fw_dsu_clk,
	},
};

static struct clk_regmap *const s5_fw_cpu_regmaps[] = {
	&s5_fw_sys_pll_dco,
	&s5_fw_sys_pll,
	&s5_fw_sys1_pll_dco,
	&s5_fw_sys1_pll,
	&s5_fw_gp1_pll_dco,
	&s5_fw_gp1_pll,
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const s5_fw_cpu_order[] = {
	&s5_fw_fclk_div2p5.hw,
	&s5_fw_sys_pll_dco.hw,
	&s5_fw_sys_pll.hw,
	&s5_fw_sys1_pll_dco.hw,
	&s5_fw_sys1_pll.hw,
	&s5_fw_gp1_pll_dco.hw,
	&s5_fw_gp1_pll.hw,
	&s5_fw_cpu_clk_dyn.hw,
	&s5_fw_cpu_clk.hw,
	&s5_fw_a76_clk_dyn.hw,
	&s5_fw_a76_clk.hw,
	&s5_fw_dsu_clk_dyn.hw,
	&s5_fw_dsu_clk.hw,
};

static struct clk_regmap *const s5_fw_regmaps[] = {
	&s5_fw_fixed_pll_dco,
	&s5_fw_fixed_pll,
	&s5_fw_sysclk_0_sel,
	&s5_fw_sysclk_0_div,
	&s5_fw_sysclk_0,
	&s5_fw_sysclk_1_sel,
	&s5_fw_sysclk_1_div,
	&s5_fw_sysclk_1,
	&s5_fw_sys_clk,
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const s5_fw_order[] = {
	&s5_fw_fixed_pll_dco.hw,
	&s5_fw_fixed_pll.hw,
	&s5_fw_fclk_div2.hw,
	&s5_fw_fclk_div3.hw,
	&s5_fw_fclk_div4.hw,
	&s5_fw_fclk_div5.hw,
	&s5_fw_sysclk_0_sel.hw,
	&s5_fw_sysclk_0_div.hw,
	&s5_fw_sysclk_0.hw,
	&s5_fw_sysclk_1_sel.hw,
	&s5_fw_sysclk_1_div.hw,
	&s5_fw_sysclk_1.hw,
	&s5_fw_sys_clk.hw,
};

static struct clk_hw *s5_fw_hws[] = {
	[CLKID_SYS_CLK]			= &s5_fw_sys_clk.hw,
	[CLKID_FIXED_PLL]		= &s5_fw_fixed_pll.hw,
};

/*
 * IDs not served here stay with the SCMI provider: -EPROBE_DEFER lets the
 * lookup move on to it, or wait for it like without this provider
 */
static struct clk_hw *s5_fw_clk_get(struct of_phandle_args *clkspec,
				    void *data)
{
	unsigned int idx = clkspec->args[0];

	if (idx >= ARRAY_SIZE(s5_fw_hws) || !s5_fw_hws[idx])
		return ERR_PTR(-EPROBE_DEFER);

	return s5_fw_hws[idx];
}

static const struct regmap_config s5_fw_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static const u8 s5_fw_cpu_rd_secids[] = {
	S5_SECID_CPU_CLK_RD,
	S5_SECID_A76_CLK_RD,
	S5_SECID_DSU_CLK_RD,
};

static int __init s5_fw_cpu_init(struct device_node *np, struct regmap *map)
{
	struct arm_smccc_res res;
	unsigned int i;
	int ret;

	/*
	 * The cpu tree needs the legacy calls the 5.15 S5 driver used, for
	 * every cluster. A BL31 without them returns SMCCC_RET_NOT_SUPPORTED,
	 * also as a 32 bit value. The a55 clock control the boot cpu runs on
	 * is never 0, the others read 0 while their cluster runs on xtal.
	 */
	for (i = 0; i < ARRAY_SIZE(s5_fw_cpu_rd_secids); i++) {
		arm_smccc_smc(SECURE_CPU_CLK, s5_fw_cpu_rd_secids[i], 0, 0, 0, 0,
			      0, 0, &res);
		if ((u32)res.a0 == (u32)SMCCC_RET_NOT_SUPPORTED || (!i && !res.a0))
			return 0;
	}

	for (i = 0; i < ARRAY_SIZE(s5_fw_cpu_regmaps); i++)
		s5_fw_cpu_regmaps[i]->map = map;

	for (i = 0; i < ARRAY_SIZE(s5_fw_cpu_order); i++) {
		ret = of_clk_hw_register(np, s5_fw_cpu_order[i]);
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(s5_fw_pll_nbs); i++) {
		ret = clk_notifier_register(s5_fw_pll_nbs[i].pll->hw.clk,
					    &s5_fw_pll_nbs[i].nb);
		if (ret)
			return ret;
	}

	s5_fw_hws[CLKID_CPU_CLK] = &s5_fw_cpu_clk.hw;
	s5_fw_hws[CLKID_A76_CLK] = &s5_fw_a76_clk.hw;
	s5_fw_hws[CLKID_DSU_CLK] = &s5_fw_dsu_clk.hw;

	return 0;
}

static void __init s5_fw_clk_init(struct device_node *np)
{
	struct device_node *clkc;
	void __iomem *base;
	struct regmap *map;
	unsigned int i;
	int ret;

	clkc = of_find_compatible_node(NULL, NULL, "amlogic,s5-clkc");
	if (!clkc)
		return;

	base = of_iomap(clkc, 0);
	of_node_put(clkc);
	if (!base) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	map = regmap_init_mmio(NULL, base, &s5_fw_regmap_config);
	if (IS_ERR(map)) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(s5_fw_regmaps); i++)
		s5_fw_regmaps[i]->map = map;

	for (i = 0; i < ARRAY_SIZE(s5_fw_order); i++) {
		ret = of_clk_hw_register(np, s5_fw_order[i]);
		if (ret)
			goto err;
	}

	ret = s5_fw_cpu_init(np, map);
	if (ret)
		pr_err("%pOF: failed to register cpu clocks: %d\n", np, ret);

	ret = of_clk_add_hw_provider(np, s5_fw_clk_get, NULL);
	if (!ret)
		return;
err:
	pr_err("%pOF: failed to register clocks: %d\n", np, ret);
}

CLK_OF_DECLARE(s5_scmi_fixed, "amlogic,s5-scmi-fixed-clkc", s5_fw_clk_init);
