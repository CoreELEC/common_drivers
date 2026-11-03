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
#include "clk-mpll.h"
#include "clk-regmap.h"
#include "clk-cpu-dyndiv.h"
#include "clk-dualdiv.h"
#include "vid-pll-div.h"
#include "s4.h"
#include <dt-bindings/clock/amlogic,s4-clkc.h>

static const struct pll_params_table gp0_pll_table[] = {
	PLL_PARAMS_OD(141, 1, 2), /* DCO = 3384M OD = 2 PLL = 846M */
	PLL_PARAMS_OD(132, 1, 2), /* DCO = 3168M OD = 2 PLL = 792M */
	PLL_PARAMS_OD(248, 1, 3), /* DCO = 5952M OD = 3 PLL = 744M */
	{ /* sentinel */ }
};

static const struct reg_sequence gp0_pll_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1, .def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL2, .def = 0x00000000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3, .def = 0x48681c00 },
	{ .reg = ANACTRL_GP0PLL_CTRL4, .def = 0x88770290 },
	{ .reg = ANACTRL_GP0PLL_CTRL5, .def = 0x39272000 },
	{ .reg = ANACTRL_GP0PLL_CTRL6, .def = 0x56540000 }
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
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_GP0PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 16,
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
		.table = gp0_pll_table,
		.init_regs = gp0_pll_init_regs,
		.init_count = ARRAY_SIZE(gp0_pll_init_regs),
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
	PLL_PARAMS_OD(196, 1, 2), /* DCO = 4718.592M OD = 2 PLL = 1179.648M */
	{ /* sentinel */ }
};

/*
 * Internal hifi pll emulation configuration parameters
 */
#define S4_HIFI_PLL_DEAL

#ifndef S4_HIFI_PLL_DEAL
static const struct reg_sequence s4_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x0000ed80 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4,	.def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5,	.def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6,	.def = 0x56540000 }
};
#else
static const struct reg_sequence hifi_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL0, .def = 0X080204c4 },
	{ .reg = ANACTRL_HIFIPLL_CTRL0, .def = 0x380204c4 },
	{ .reg = ANACTRL_HIFIPLL_CTRL1, .def = 0x0000ed80 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2, .def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3, .def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4, .def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5, .def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6, .def = 0x56540000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL0, .def = 0x180204c4 }
};

#ifndef FIXED_FRAC_WEIGHT_PRECISION
#define FIXED_FRAC_WEIGHT_PRECISION	100000
#endif
#ifndef PLL_LOCK_RETRY_CNT_MAX
#define PLL_LOCK_RETRY_CNT_MAX		10
#endif

static inline struct meson_clk_pll_data *
meson_clk_pll_data(struct clk_regmap *clk)
{
	return (struct meson_clk_pll_data *)clk->data;
}

static unsigned long __pll_params_to_rate(unsigned long parent_rate,
	unsigned int m, unsigned int n,
	unsigned int frac, unsigned int od,
	struct meson_clk_pll_data *pll)
{
	u64 rate, frac_rate;
	u32 frac_precision;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
		n = (u32)BIT(n);

	if (unlikely(!n))
		return 0;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		parent_rate = parent_rate >> 1;

	rate = DIV_ROUND_UP_ULL((u64)parent_rate * m, n);
	if (frac && MESON_PARM_APPLICABLE(&pll->frac)) {
		if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
			frac_precision = FIXED_FRAC_WEIGHT_PRECISION;
		else
			frac_precision = (u32)BIT(pll->frac.width);

		frac_rate = div_u64((u64)parent_rate * frac, n);
		rate += DIV_ROUND_UP_ULL(frac_rate, frac_precision);
	}

	if (od && MESON_PARM_APPLICABLE(&pll->od))
		rate = rate >> od;

	return rate;
}

static int meson_clk_pll_wait_lock(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	int delay = 1000;

	do {
		/* Is the clock locked now ? */
		if (meson_parm_read(clk->map, &pll->l))
			return 0;
		udelay(1);
	} while (delay--);

	return -ETIMEDOUT;
}

static unsigned long meson_s4_clk_hifi_pll_recalc_rate(struct clk_hw *hw,
	unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac, od;

	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);

	frac = MESON_PARM_APPLICABLE(&pll->frac) ?
	       meson_parm_read(clk->map, &pll->frac) : 0;

	od = MESON_PARM_APPLICABLE(&pll->od) ?
	     meson_parm_read(clk->map, &pll->od) : 0;

	return __pll_params_to_rate(parent_rate, m, n, frac, od, pll);
}

static int meson_s4_clk_hifi_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* Enable and lock bit equal 1, it locks */
	if (meson_parm_read(clk->map, &pll->en) &&
	    meson_parm_read(clk->map, &pll->l))
		return 1;

	return 0;
}

static int meson_s4_clk_hifi_pll_init(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/*
	 * Keep the clock running, which was already initialized and enabled
	 * from the bootloader stage, to avoid any glitches.
	 */
	if ((pll->flags & CLK_MESON_PLL_NOINIT_ENABLED) &&
	     meson_s4_clk_hifi_pll_is_enabled(hw))
		return 0;

	if (pll->init_count) {
		meson_parm_write(clk->map, &pll->rst, 1);
		regmap_multi_reg_write(clk->map, pll->init_regs,
				pll->init_count);
		meson_parm_write(clk->map, &pll->rst, 0);
	}

	return 0;
}

static void meson_s4_clk_hifi_pll_disable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* Put the pll is in reset */
	if (MESON_PARM_APPLICABLE(&pll->rst))
		meson_parm_write(clk->map, &pll->rst, 1);

	/* Disable the pll */
	meson_parm_write(clk->map, &pll->en, 0);
}

static int meson_s4_clk_hifi_pll_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	int retry = 0;

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	do {
		/* Make sure the pll is disabled */
		meson_s4_clk_hifi_pll_is_enabled(hw);

		/* Enable the pll */
		meson_parm_write(clk->map, &pll->en, 1);

		/*
		 * Delay 50us to wait for PLL to stabilize, otherwise PLL may
		 * have a lock failure (especially in low temperature scenarios).
		 */
		udelay(50);

		/* Take the pll out reset */
		if (MESON_PARM_APPLICABLE(&pll->rst))
			meson_parm_write(clk->map, &pll->rst, 0);

		if (!meson_clk_pll_wait_lock(hw))
			return 0;
	} while (retry > PLL_LOCK_RETRY_CNT_MAX);
	pr_warn("%s: PLL lock failed!!!\n", clk_hw_get_name(hw));

	return -EIO;
}

const struct clk_ops meson_s4_clk_hifi_pll_ops = {
	.init		= meson_s4_clk_hifi_pll_init,
	.recalc_rate	= meson_s4_clk_hifi_pll_recalc_rate,
	.is_enabled	= meson_s4_clk_hifi_pll_is_enabled,
	.enable		= meson_s4_clk_hifi_pll_enable,
	.disable	= meson_s4_clk_hifi_pll_disable,
};
#endif

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
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 16,
			.width   = 2,
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
		.table = hifi_pll_table,
		.init_regs = hifi_pll_init_regs,
		.init_count = ARRAY_SIZE(hifi_pll_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			 CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi_pll",
#ifdef S4_HIFI_PLL_DEAL
		.ops = &meson_s4_clk_hifi_pll_ops,
#else
		.ops = &meson_clk_pll_ops,
#endif
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table s4d_hifi_pll_table[] = {
	PLL_PARAMS_OD(163, 1, 1), /* DCO = 3932.16M OD = 2 PLL = 1966.08M */
	PLL_PARAMS_OD(196, 1, 2), /* DCO = 4718.592M OD = 2 PLL = 1179.648M */
	{ /* sentinel */ }
};

static const struct reg_sequence s4d_hifi_pll_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1, .def = 0x0000ed80 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2, .def = 0x00000000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3, .def = 0x6a285c00 },
	{ .reg = ANACTRL_HIFIPLL_CTRL4, .def = 0x65771290 },
	{ .reg = ANACTRL_HIFIPLL_CTRL5, .def = 0x39272000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL6, .def = 0x56540000 }
};

static struct clk_regmap s4d_hifi_pll = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 16,
			.width   = 2,
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
		.table = s4d_hifi_pll_table,
		.init_regs = s4d_hifi_pll_init_regs,
		.init_count = ARRAY_SIZE(s4d_hifi_pll_init_regs),
		.flags = CLK_MESON_PLL_ROUND_CLOSEST |
			 CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
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

static struct clk_regmap hdmi_pll_dco = {
	.data = &(struct meson_clk_pll_data) {
		.en = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_HDMIPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.l = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HDMIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_pll_dco",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor mpll_prediv = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data) {
		.name = "mpll_prediv",
		.ops = &clk_fixed_factor_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "fixed_pll_dco"
		},
		.num_parents = 1,
	},
};

static DEFINE_SPINLOCK(meson_clk_lock);

static const struct reg_sequence mpll0_div_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL0, .def = 0x00000543 },/*write one time for mpll0/1/2/3*/
	{ .reg = ANACTRL_MPLL_CTRL2, .def = 0x40000033 }
};

static struct clk_regmap mpll0_div = {
	.data = &(struct meson_clk_mpll_data) {
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 30,
			.width   = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL1,
			.shift   = 29,
			.width   = 1,
		},
		.init_regs = mpll0_div_init_regs,
		.init_count = ARRAY_SIZE(mpll0_div_init_regs),
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll0_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll_prediv.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mpll0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence mpll1_div_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL4, .def = 0x40000033 }
};

static struct clk_regmap mpll1_div = {
	.data = &(struct meson_clk_mpll_data) {
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 30,
			.width   = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL3,
			.shift   = 29,
			.width   = 1,
		},
		.init_regs = mpll1_div_init_regs,
		.init_count = ARRAY_SIZE(mpll1_div_init_regs),
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll1_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll_prediv.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mpll1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL3,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence mpll2_div_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL6, .def = 0x40000033 }
};

static struct clk_regmap mpll2_div = {
	.data = &(struct meson_clk_mpll_data) {
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 30,
			.width   = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL5,
			.shift   = 29,
			.width   = 1,
		},
		.init_regs = mpll2_div_init_regs,
		.init_count = ARRAY_SIZE(mpll2_div_init_regs),
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll2_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll_prediv.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mpll2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL5,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll2_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct reg_sequence mpll3_div_init_regs[] = {
	{ .reg = ANACTRL_MPLL_CTRL8, .def = 0x40000033 }
};

static struct clk_regmap mpll3_div = {
	.data = &(struct meson_clk_mpll_data) {
		.sdm = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 0,
			.width   = 14,
		},
		.sdm_en = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 30,
			.width   = 1,
		},
		.n2 = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 20,
			.width   = 9,
		},
		.ssen = {
			.reg_off = ANACTRL_MPLL_CTRL7,
			.shift   = 29,
			.width   = 1,
		},
		.init_regs = mpll3_div_init_regs,
		.init_count = ARRAY_SIZE(mpll3_div_init_regs),
		.lock = &meson_clk_lock,
		.flags = CLK_MESON_MPLL_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll3_div",
		.ops = &meson_clk_mpll_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll_prediv.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap mpll3 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = ANACTRL_MPLL_CTRL7,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mpll3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&mpll3_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdmi_pll_od = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_HDMIPLL_CTRL0,
		.shift = 16,
		.width = 4,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_pll_od",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmi_pll_dco.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap hdmi_pll = {
	.data = &(struct clk_regmap_div_data) {
		.offset = ANACTRL_HDMIPLL_CTRL0,
		.shift = 20,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_pll",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmi_pll_od.hw,
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
		.bit_idx = 24,
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
		.bit_idx = 25,
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
		.bit_idx = 20,
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
		.bit_idx = 21,
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
		.bit_idx = 22,
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
		.bit_idx = 23,
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
			.width   = 1,
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

static struct clk_regmap ceca_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param ceca_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap ceca_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECA_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECA_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = ceca_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ceca_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data ceca_dual_parent_data[] = {
	{ .hw = &ceca_dual_div.hw },
	{ .hw = &ceca_dual_clkin.hw }
};

static struct clk_regmap ceca_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ceca_dual_parent_data,
		.num_parents = ARRAY_SIZE(ceca_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ceca_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_CECA_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ceca_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ceca_clk_parent_data[] = {
	{ .hw = &ceca_dual.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap ceca_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECA_CTRL1,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ceca_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ceca_clk_parent_data,
		.num_parents = ARRAY_SIZE(ceca_clk_parent_data),
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
			.width   = 1,
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

static u32 rama_01_parent_table[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data rama_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &rtc_clk.hw }
};

static struct clk_regmap rama_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = rama_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_0_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rama_01_parent_data,
		.num_parents = ARRAY_SIZE(rama_01_parent_data),
	},
};

static struct clk_regmap rama_0_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rama_0_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap rama_0 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rama_0_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap rama_1_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = rama_01_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_1_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rama_01_parent_data,
		.num_parents = ARRAY_SIZE(rama_01_parent_data),
	},
};

static struct clk_regmap rama_1_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rama_1_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap rama_1 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&rama_1_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_SET_RATE_GATE | CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data rama_parent_data[] = {
	{ .hw = &rama_0.hw },
	{ .hw = &rama_1.hw }
};

static struct clk_regmap rama = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RAMA_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rama",
		.ops = &clk_regmap_mux_ops,
		.parent_data = rama_parent_data,
		.num_parents = ARRAY_SIZE(rama_parent_data),
		/**
		 * clk driver built-in loaded, critical clks cannot be
		 * disabled by clk_disable_unused(). for glitch-free clocks,
		 * CLK_OPS_PARENT_ENABLE flag needs to be removed.
		 */
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

static const struct clk_parent_data sc_parent_data[] = {
	{ .hw = &fclk_div4.hw },
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

static struct clk_regmap vid_pll_div = {
	.data = &(struct meson_vid_pll_div_data){
		.val = {
			.reg_off = CLKCTRL_VID_PLL_CLK_DIV,
			.shift   = 0,
			.width   = 15,
		},
		.sel = {
			.reg_off = CLKCTRL_VID_PLL_CLK_DIV,
			.shift   = 16,
			.width   = 2,
		},
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll_div",
		.ops = &meson_vid_pll_div_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdmi_pll.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_parent_data vid_pll_sel_parent_data[] = {
	{ .hw = &vid_pll_div.hw },
	{ .hw = &hdmi_pll.hw }
};

static struct clk_regmap vid_pll_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_PLL_CLK_DIV,
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = vid_pll_sel_parent_data,
		.num_parents = ARRAY_SIZE(vid_pll_sel_parent_data),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap vid_pll = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_PLL_CLK_DIV,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&vid_pll_sel.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT |
			 CLK_IGNORE_UNUSED,
	},
};

static const struct clk_parent_data video_src_01_parent_data[] = {
	{ .hw = &vid_pll.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &mpll1.hw },
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

static struct clk_regmap enct_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VID_CLK_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = hdmitx_parent_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enct_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdmitx_parent_data,
		.num_parents = ARRAY_SIZE(hdmitx_parent_data),
	},
};

static struct clk_regmap enct = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_VID_CLK_CTRL2,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "enct",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&enct_mux.hw,
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

static const struct clk_parent_data mali_01_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &gp0_pll.hw },
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

static const struct clk_parent_data vdec_01_parent_data[] = {
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw },
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
	{ .hw = &gp0_pll.hw },
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

static const struct clk_parent_data vpu_01_parent_data[] = {
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &mpll1.hw },
	{ .hw = &vid_pll.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap vpu_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static const struct clk_parent_data vpu_clkc_01_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &mpll1.hw },
	{ .hw = &vid_pll.hw },
	{ .hw = &mpll2.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap vpu_clkc_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VPU_CLKC_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static const struct clk_parent_data vapb_01_parent_data[] = {
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &mpll1.hw },
	{ .hw = &vid_pll.hw },
	{ .hw = &mpll2.hw },
	{ .hw = &fclk_div2p5.hw }
};

static struct clk_regmap vapb_0_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static const struct clk_parent_data vdin_meas_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &vid_pll.hw }
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

static const struct clk_parent_data sd_emmc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &hifi_pll.hw },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &mpll2.hw },
	{ .hw = &mpll3.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap sd_emmc_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static struct clk_regmap sd_emmc_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
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

static struct clk_regmap sd_emmc_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
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

static const struct clk_parent_data s4d_sd_emmc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div3.hw },
	{ .fw_name = "gp1_pll" },
	{ .hw = &fclk_div2p5.hw },
	{ .hw = &mpll2.hw },
	{ .hw = &mpll3.hw },
	{ .hw = &gp0_pll.hw }
};

static struct clk_regmap s4d_sd_emmc_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_parent_data),
	},
};

static struct clk_regmap s4d_sd_emmc_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_a_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_sd_emmc_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_a_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_sd_emmc_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_parent_data),
	},
};

static struct clk_regmap s4d_sd_emmc_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_b_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_sd_emmc_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SD_EMMC_CLK_CTRL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_b_div.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_sd_emmc_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(s4d_sd_emmc_parent_data),
	},
};

static struct clk_regmap s4d_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_c_mux.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s4d_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_sd_emmc_c_div.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data spicc_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "sys_clk" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div2.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &gp0_pll.hw }
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
		.parent_data = spicc_parent_data,
		.num_parents = ARRAY_SIZE(spicc_parent_data),
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

static const struct clk_parent_data pwm_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &vid_pll.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw }
};

static struct clk_regmap pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_AB_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
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
		.parent_data = pwm_parent_data,
		.num_parents = ARRAY_SIZE(pwm_parent_data),
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
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
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

static const struct clk_parent_data hdcp22_esm_parent_data[] = {
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
};

static struct clk_regmap hdcp22_esm_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esm_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdcp22_esm_parent_data,
		.num_parents = ARRAY_SIZE(hdcp22_esm_parent_data),
	},
};

static struct clk_regmap hdcp22_esm_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esm_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdcp22_esm_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdcp22_esm = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_esm",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdcp22_esm_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data hdcp22_skp_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw }
};

static struct clk_regmap hdcp22_skp_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skp_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = hdcp22_skp_parent_data,
		.num_parents = ARRAY_SIZE(hdcp22_skp_parent_data),
	},
};

static struct clk_regmap hdcp22_skp_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdcp22_skp_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap hdcp22_skp = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_HDCP22_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdcp22_skp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&hdcp22_skp_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data demod_core_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div7.hw },
	{ .hw = &fclk_div4.hw }
};

static struct clk_regmap demod_core_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = demod_core_parent_data,
		.num_parents = ARRAY_SIZE(demod_core_parent_data),
	},
};

static struct clk_regmap demod_core_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&demod_core_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap demod_core = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&demod_core_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data s4d_demod_core_t2_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div4.hw }
};

static struct clk_regmap s4d_demod_core_t2_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_t2_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_demod_core_t2_parent_data,
		.num_parents = ARRAY_SIZE(s4d_demod_core_t2_parent_data),
	},
};

static struct clk_regmap s4d_demod_core_t2_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_t2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_core_t2_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4d_demod_core_t2 = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_core_t2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_core_t2_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4d_demod_32k_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_32K_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param s4d_demod_32k_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap s4d_demod_32k_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_DEMOD_32K_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = s4d_demod_32k_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_32k_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data s4d_demod_32k_parent_data[] = {
	{ .hw = &s4d_demod_32k_div.hw },
	{ .hw = &s4d_demod_32k_clkin.hw }
};

static struct clk_regmap s4d_demod_32k_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_32K_CTRL0,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = s4d_demod_32k_parent_data,
		.num_parents = ARRAY_SIZE(s4d_demod_32k_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap s4d_demod_32k = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_32K_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s4d_demod_32k_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data adc_extclk_in_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &fclk_div4.hw },
	{ .hw = &fclk_div3.hw },
	{ .hw = &fclk_div5.hw },
	{ .hw = &fclk_div7.hw },
	{ .hw = &mpll2.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw }
};

static struct clk_regmap adc_extclk_in_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_in_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = adc_extclk_in_parent_data,
		.num_parents = ARRAY_SIZE(adc_extclk_in_parent_data),
	},
};

static struct clk_regmap adc_extclk_in_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_in_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&adc_extclk_in_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap adc_extclk_in = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_DEMOD_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&adc_extclk_in_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
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
			&fclk_div2.hw,
		},
		.num_parents = 1,
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

static struct clk_fixed_factor eth_clk125M_div = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk125M_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&fclk_div2.hw,
		},
		.num_parents = 1,
	},
};

static struct clk_regmap eth_clk125M = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "eth_clk125M",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&eth_clk125M_div.hw,
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

static u32 gen_parent_table[] = { 0, 1, 2, 4, 5, 7, 17, 19, 20, 21, 22,
				 23, 24 };
static const struct clk_parent_data gen_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &rtc_clk.hw },
	{ .fw_name = "sys_pll_div16" },
	{ .hw = &vid_pll.hw },
	{ .hw = &gp0_pll.hw },
	{ .hw = &hifi_pll.hw },
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

MESON_CLK_GATE_SYS_CLK(sys_ddr, CLKCTRL_SYS_CLK_EN0_REG0, 0);
MESON_CLK_GATE_SYS_CLK(sys_dos, CLKCTRL_SYS_CLK_EN0_REG0, 1);
MESON_CLK_GATE_SYS_CLK(sys_eth_phy, CLKCTRL_SYS_CLK_EN0_REG0, 4);
MESON_CLK_GATE_SYS_CLK(sys_mali, CLKCTRL_SYS_CLK_EN0_REG0, 6);
MESON_CLK_GATE_SYS_CLK(sys_aocpu, CLKCTRL_SYS_CLK_EN0_REG0, 13);
MESON_CLK_GATE_SYS_CLK(sys_aucpu, CLKCTRL_SYS_CLK_EN0_REG0, 14);
MESON_CLK_GATE_SYS_CLK(sys_cec, CLKCTRL_SYS_CLK_EN0_REG0, 16);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_a, CLKCTRL_SYS_CLK_EN0_REG0, 24);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_b, CLKCTRL_SYS_CLK_EN0_REG0, 25);
MESON_CLK_GATE_SYS_CLK(sys_sd_emmc_c, CLKCTRL_SYS_CLK_EN0_REG0, 26);
MESON_CLK_GATE_SYS_CLK(sys_smartcard, CLKCTRL_SYS_CLK_EN0_REG0, 27);
MESON_CLK_GATE_SYS_CLK(sys_acodec, CLKCTRL_SYS_CLK_EN0_REG0, 28);
MESON_CLK_GATE_SYS_CLK(sys_spifc, CLKCTRL_SYS_CLK_EN0_REG0, 29);
MESON_CLK_GATE_SYS_CLK(sys_msr_clk, CLKCTRL_SYS_CLK_EN0_REG0, 30);
MESON_CLK_GATE_SYS_CLK(sys_ir_ctrl, CLKCTRL_SYS_CLK_EN0_REG0, 31);
MESON_CLK_GATE_SYS_CLK(sys_audio, CLKCTRL_SYS_CLK_EN0_REG1, 0);
MESON_CLK_GATE_SYS_CLK(sys_eth, CLKCTRL_SYS_CLK_EN0_REG1, 3);
MESON_CLK_GATE_SYS_CLK(sys_uart_a, CLKCTRL_SYS_CLK_EN0_REG1, 5);
MESON_CLK_GATE_SYS_CLK(sys_uart_b, CLKCTRL_SYS_CLK_EN0_REG1, 6);
MESON_CLK_GATE_SYS_CLK(sys_uart_c, CLKCTRL_SYS_CLK_EN0_REG1, 7);
MESON_CLK_GATE_SYS_CLK(sys_uart_d, CLKCTRL_SYS_CLK_EN0_REG1, 8);
MESON_CLK_GATE_SYS_CLK(sys_uart_e, CLKCTRL_SYS_CLK_EN0_REG1, 9);
MESON_CLK_GATE_SYS_CLK(sys_aififo, CLKCTRL_SYS_CLK_EN0_REG1, 11);
MESON_CLK_GATE_SYS_CLK(sys_ts_ddr, CLKCTRL_SYS_CLK_EN0_REG1, 15);
MESON_CLK_GATE_SYS_CLK(sys_ts_pll, CLKCTRL_SYS_CLK_EN0_REG1, 16);
MESON_CLK_GATE_SYS_CLK(sys_ge2d, CLKCTRL_SYS_CLK_EN0_REG1, 20);
MESON_CLK_GATE_SYS_CLK(sys_spicc0, CLKCTRL_SYS_CLK_EN0_REG1, 21);
MESON_CLK_GATE_SYS_CLK(sys_usb, CLKCTRL_SYS_CLK_EN0_REG1, 26);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_a, CLKCTRL_SYS_CLK_EN0_REG1, 30);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_b, CLKCTRL_SYS_CLK_EN0_REG1, 31);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_c, CLKCTRL_SYS_CLK_EN0_REG2, 0);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_d, CLKCTRL_SYS_CLK_EN0_REG2, 1);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_e, CLKCTRL_SYS_CLK_EN0_REG2, 2);
MESON_CLK_GATE_SYS_CLK(sys_i2c_m_f, CLKCTRL_SYS_CLK_EN0_REG2, 3);
MESON_CLK_GATE_SYS_CLK(sys_hdmitx_apb, CLKCTRL_SYS_CLK_EN0_REG2, 4);
MESON_CLK_GATE_SYS_CLK(sys_i2c_s_a, CLKCTRL_SYS_CLK_EN0_REG2, 5);
MESON_CLK_GATE_SYS_CLK(sys_usb1_to_ddr, CLKCTRL_SYS_CLK_EN0_REG2, 8);
MESON_CLK_GATE_SYS_CLK(sys_hdcp22, CLKCTRL_SYS_CLK_EN0_REG2, 10);
MESON_CLK_GATE_SYS_CLK(sys_mmc_apb, CLKCTRL_SYS_CLK_EN0_REG2, 11);
MESON_CLK_GATE_SYS_CLK(sys_rsa, CLKCTRL_SYS_CLK_EN0_REG2, 18);
MESON_CLK_GATE_SYS_CLK(sys_cpu_debug, CLKCTRL_SYS_CLK_EN0_REG2, 19);
MESON_CLK_GATE_SYS_CLK(sys_vpu_intr, CLKCTRL_SYS_CLK_EN0_REG2, 25);
MESON_CLK_GATE_SYS_CLK(sys_demod, CLKCTRL_SYS_CLK_EN0_REG2, 27);
MESON_CLK_GATE_SYS_CLK(sys_sar_adc, CLKCTRL_SYS_CLK_EN0_REG2, 28);
MESON_CLK_GATE_SYS_CLK(sys_gic, CLKCTRL_SYS_CLK_EN0_REG2, 30);
MESON_CLK_GATE_SYS_CLK(sys_pwm_ab, CLKCTRL_SYS_CLK_EN0_REG3, 7);
MESON_CLK_GATE_SYS_CLK(sys_pwm_cd, CLKCTRL_SYS_CLK_EN0_REG3, 8);
MESON_CLK_GATE_SYS_CLK(sys_pwm_ef, CLKCTRL_SYS_CLK_EN0_REG3, 9);
MESON_CLK_GATE_SYS_CLK(sys_pwm_gh, CLKCTRL_SYS_CLK_EN0_REG3, 10);
MESON_CLK_GATE_SYS_CLK(sys_pwm_ij, CLKCTRL_SYS_CLK_EN0_REG3, 11);

static struct clk_regmap *const pll_regmaps[] = {
	&gp0_pll,
	&hifi_pll,
	&s4d_hifi_pll,
	&hdmi_pll_dco,
	&mpll0,
	&mpll0_div,
	&mpll1,
	&mpll1_div,
	&mpll2,
	&mpll2_div,
	&mpll3,
	&mpll3_div,
	&hdmi_pll_od,
	&hdmi_pll,
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
	&ceca_dual_clkin,
	&ceca_dual_div,
	&ceca_dual_mux,
	&ceca_dual,
	&ceca_clk,
	&cecb_dual_clkin,
	&cecb_dual_div,
	&cecb_dual_mux,
	&cecb_dual,
	&cecb_clk,
	&rama_0_mux,
	&rama_0_div,
	&rama_0,
	&rama_1_mux,
	&rama_1_div,
	&rama_1,
	&rama,
	&sar_adc_mux,
	&sar_adc_div,
	&sar_adc,
	&sc_mux,
	&sc_div,
	&sc,
	&cdac_mux,
	&cdac_div,
	&cdac,
	&vid_pll_div,
	&vid_pll_sel,
	&vid_pll,
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
	&hdmitx_pixel_mux,
	&hdmitx_pixel,
	&enci_mux,
	&enci,
	&encp_mux,
	&encp,
	&enct_mux,
	&enct,
	&encl_mux,
	&encl,
	&vdac_mux,
	&vdac,
	&vid_lock_mux,
	&vid_lock_div,
	&vid_lock,
	&mali_0_mux,
	&mali_0_div,
	&mali_0,
	&mali_1_mux,
	&mali_1_div,
	&mali_1,
	&mali,
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
	&vdin_meas_mux,
	&vdin_meas_div,
	&vdin_meas,
	&sd_emmc_a_mux,
	&sd_emmc_a_div,
	&sd_emmc_a,
	&sd_emmc_b_mux,
	&sd_emmc_b_div,
	&sd_emmc_b,
	&sd_emmc_c_mux,
	&sd_emmc_c_div,
	&sd_emmc_c,
	&s4d_sd_emmc_a_mux,
	&s4d_sd_emmc_a_div,
	&s4d_sd_emmc_a,
	&s4d_sd_emmc_b_mux,
	&s4d_sd_emmc_b_div,
	&s4d_sd_emmc_b,
	&s4d_sd_emmc_c_mux,
	&s4d_sd_emmc_c_div,
	&s4d_sd_emmc_c,
	&spicc0_mux,
	&spicc0_div,
	&spicc0,
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
	&hdmitx_sys_mux,
	&hdmitx_sys_div,
	&hdmitx_sys,
	&hdcp22_esm_mux,
	&hdcp22_esm_div,
	&hdcp22_esm,
	&hdcp22_skp_mux,
	&hdcp22_skp_div,
	&hdcp22_skp,
	&demod_core_mux,
	&demod_core_div,
	&demod_core,
	&s4d_demod_core_t2_mux,
	&s4d_demod_core_t2_div,
	&s4d_demod_core_t2,
	&s4d_demod_32k_clkin,
	&s4d_demod_32k_div,
	&s4d_demod_32k_mux,
	&s4d_demod_32k,
	&adc_extclk_in_mux,
	&adc_extclk_in_div,
	&adc_extclk_in,
	&eth_clk_rmii_div,
	&eth_clk_rmii,
	&eth_clk125M,
	&ts_div,
	&ts,
	&gen_mux,
	&gen_div,
	&gen,
	&sys_ddr,
	&sys_dos,
	&sys_eth_phy,
	&sys_mali,
	&sys_aocpu,
	&sys_aucpu,
	&sys_cec,
	&sys_sd_emmc_a,
	&sys_sd_emmc_b,
	&sys_sd_emmc_c,
	&sys_smartcard,
	&sys_acodec,
	&sys_spifc,
	&sys_msr_clk,
	&sys_ir_ctrl,
	&sys_audio,
	&sys_eth,
	&sys_uart_a,
	&sys_uart_b,
	&sys_uart_c,
	&sys_uart_d,
	&sys_uart_e,
	&sys_aififo,
	&sys_ts_ddr,
	&sys_ts_pll,
	&sys_ge2d,
	&sys_spicc0,
	&sys_usb,
	&sys_i2c_m_a,
	&sys_i2c_m_b,
	&sys_i2c_m_c,
	&sys_i2c_m_d,
	&sys_i2c_m_e,
	&sys_i2c_m_f,
	&sys_hdmitx_apb,
	&sys_i2c_s_a,
	&sys_usb1_to_ddr,
	&sys_hdcp22,
	&sys_mmc_apb,
	&sys_rsa,
	&sys_cpu_debug,
	&sys_vpu_intr,
	&sys_demod,
	&sys_sar_adc,
	&sys_gic,
	&sys_pwm_ab,
	&sys_pwm_cd,
	&sys_pwm_ef,
	&sys_pwm_gh,
	&sys_pwm_ij
};

static struct clk_hw *s4_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &hifi_pll.hw,
	[CLKID_HDMI_PLL_DCO]          = &hdmi_pll_dco.hw,
	[CLKID_MPLL_PREDIV]           = &mpll_prediv.hw,
	[CLKID_MPLL0]                 = &mpll0.hw,
	[CLKID_MPLL0_DIV]             = &mpll0_div.hw,
	[CLKID_MPLL1]                 = &mpll1.hw,
	[CLKID_MPLL1_DIV]             = &mpll1_div.hw,
	[CLKID_MPLL2]                 = &mpll2.hw,
	[CLKID_MPLL2_DIV]             = &mpll2_div.hw,
	[CLKID_MPLL3]                 = &mpll3.hw,
	[CLKID_MPLL3_DIV]             = &mpll3_div.hw,
	[CLKID_HDMI_PLL_OD]           = &hdmi_pll_od.hw,
	[CLKID_HDMI_PLL]              = &hdmi_pll.hw,
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
	[CLKID_CECA_DUAL_CLKIN]       = &ceca_dual_clkin.hw,
	[CLKID_CECA_DUAL_DIV]         = &ceca_dual_div.hw,
	[CLKID_CECA_DUAL_MUX]         = &ceca_dual_mux.hw,
	[CLKID_CECA_DUAL]             = &ceca_dual.hw,
	[CLKID_CECA_CLK]              = &ceca_clk.hw,
	[CLKID_CECB_DUAL_CLKIN]       = &cecb_dual_clkin.hw,
	[CLKID_CECB_DUAL_DIV]         = &cecb_dual_div.hw,
	[CLKID_CECB_DUAL_MUX]         = &cecb_dual_mux.hw,
	[CLKID_CECB_DUAL]             = &cecb_dual.hw,
	[CLKID_CECB_CLK]              = &cecb_clk.hw,
	[CLKID_RAMA_0_MUX]            = &rama_0_mux.hw,
	[CLKID_RAMA_0_DIV]            = &rama_0_div.hw,
	[CLKID_RAMA_0]                = &rama_0.hw,
	[CLKID_RAMA_1_MUX]            = &rama_1_mux.hw,
	[CLKID_RAMA_1_DIV]            = &rama_1_div.hw,
	[CLKID_RAMA_1]                = &rama_1.hw,
	[CLKID_RAMA]                  = &rama.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_SC_MUX]                = &sc_mux.hw,
	[CLKID_SC_DIV]                = &sc_div.hw,
	[CLKID_SC]                    = &sc.hw,
	[CLKID_CDAC_MUX]              = &cdac_mux.hw,
	[CLKID_CDAC_DIV]              = &cdac_div.hw,
	[CLKID_CDAC]                  = &cdac.hw,
	[CLKID_VID_PLL_DIV]           = &vid_pll_div.hw,
	[CLKID_VID_PLL_SEL]           = &vid_pll_sel.hw,
	[CLKID_VID_PLL]               = &vid_pll.hw,
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
	[CLKID_HDMITX_PIXEL_MUX]      = &hdmitx_pixel_mux.hw,
	[CLKID_HDMITX_PIXEL]          = &hdmitx_pixel.hw,
	[CLKID_ENCI_MUX]              = &enci_mux.hw,
	[CLKID_ENCI]                  = &enci.hw,
	[CLKID_ENCP_MUX]              = &encp_mux.hw,
	[CLKID_ENCP]                  = &encp.hw,
	[CLKID_ENCT_MUX]              = &enct_mux.hw,
	[CLKID_ENCT]                  = &enct.hw,
	[CLKID_ENCL_MUX]              = &encl_mux.hw,
	[CLKID_ENCL]                  = &encl.hw,
	[CLKID_VDAC_MUX]              = &vdac_mux.hw,
	[CLKID_VDAC]                  = &vdac.hw,
	[CLKID_VID_LOCK_MUX]          = &vid_lock_mux.hw,
	[CLKID_VID_LOCK_DIV]          = &vid_lock_div.hw,
	[CLKID_VID_LOCK]              = &vid_lock.hw,
	[CLKID_MALI_0_MUX]            = &mali_0_mux.hw,
	[CLKID_MALI_0_DIV]            = &mali_0_div.hw,
	[CLKID_MALI_0]                = &mali_0.hw,
	[CLKID_MALI_1_MUX]            = &mali_1_mux.hw,
	[CLKID_MALI_1_DIV]            = &mali_1_div.hw,
	[CLKID_MALI_1]                = &mali_1.hw,
	[CLKID_MALI]                  = &mali.hw,
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
	[CLKID_VDIN_MEAS_MUX]         = &vdin_meas_mux.hw,
	[CLKID_VDIN_MEAS_DIV]         = &vdin_meas_div.hw,
	[CLKID_VDIN_MEAS]             = &vdin_meas.hw,
	[CLKID_SD_EMMC_A_MUX]         = &sd_emmc_a_mux.hw,
	[CLKID_SD_EMMC_A_DIV]         = &sd_emmc_a_div.hw,
	[CLKID_SD_EMMC_A]             = &sd_emmc_a.hw,
	[CLKID_SD_EMMC_B_MUX]         = &sd_emmc_b_mux.hw,
	[CLKID_SD_EMMC_B_DIV]         = &sd_emmc_b_div.hw,
	[CLKID_SD_EMMC_B]             = &sd_emmc_b.hw,
	[CLKID_SD_EMMC_C_MUX]         = &sd_emmc_c_mux.hw,
	[CLKID_SD_EMMC_C_DIV]         = &sd_emmc_c_div.hw,
	[CLKID_SD_EMMC_C]             = &sd_emmc_c.hw,
	[CLKID_SPICC0_MUX]            = &spicc0_mux.hw,
	[CLKID_SPICC0_DIV]            = &spicc0_div.hw,
	[CLKID_SPICC0]                = &spicc0.hw,
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
	[CLKID_HDMITX_SYS_MUX]        = &hdmitx_sys_mux.hw,
	[CLKID_HDMITX_SYS_DIV]        = &hdmitx_sys_div.hw,
	[CLKID_HDMITX_SYS]            = &hdmitx_sys.hw,
	[CLKID_HDCP22_ESM_MUX]        = &hdcp22_esm_mux.hw,
	[CLKID_HDCP22_ESM_DIV]        = &hdcp22_esm_div.hw,
	[CLKID_HDCP22_ESM]            = &hdcp22_esm.hw,
	[CLKID_HDCP22_SKP_MUX]        = &hdcp22_skp_mux.hw,
	[CLKID_HDCP22_SKP_DIV]        = &hdcp22_skp_div.hw,
	[CLKID_HDCP22_SKP]            = &hdcp22_skp.hw,
	[CLKID_DEMOD_CORE_MUX]        = &demod_core_mux.hw,
	[CLKID_DEMOD_CORE_DIV]        = &demod_core_div.hw,
	[CLKID_DEMOD_CORE]            = &demod_core.hw,
	[CLKID_ADC_EXTCLK_IN_MUX]     = &adc_extclk_in_mux.hw,
	[CLKID_ADC_EXTCLK_IN_DIV]     = &adc_extclk_in_div.hw,
	[CLKID_ADC_EXTCLK_IN]         = &adc_extclk_in.hw,
	[CLKID_ETH_CLK_RMII_DIV]      = &eth_clk_rmii_div.hw,
	[CLKID_ETH_CLK_RMII]          = &eth_clk_rmii.hw,
	[CLKID_ETH_CLK125M_DIV]       = &eth_clk125M_div.hw,
	[CLKID_ETH_CLK125M]           = &eth_clk125M.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_DDR]               = &sys_ddr.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_MALI]              = &sys_mali.hw,
	[CLKID_SYS_AOCPU]             = &sys_aocpu.hw,
	[CLKID_SYS_AUCPU]             = &sys_aucpu.hw,
	[CLKID_SYS_CEC]               = &sys_cec.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_SMARTCARD]         = &sys_smartcard.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_SPIFC]             = &sys_spifc.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_IR_CTRL]           = &sys_ir_ctrl.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_ETH]               = &sys_eth.hw,
	[CLKID_SYS_UART_A]            = &sys_uart_a.hw,
	[CLKID_SYS_UART_B]            = &sys_uart_b.hw,
	[CLKID_SYS_UART_C]            = &sys_uart_c.hw,
	[CLKID_SYS_UART_D]            = &sys_uart_d.hw,
	[CLKID_SYS_UART_E]            = &sys_uart_e.hw,
	[CLKID_SYS_AIFIFO]            = &sys_aififo.hw,
	[CLKID_SYS_TS_DDR]            = &sys_ts_ddr.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPICC0]            = &sys_spicc0.hw,
	[CLKID_SYS_USB]               = &sys_usb.hw,
	[CLKID_SYS_I2C_M_A]           = &sys_i2c_m_a.hw,
	[CLKID_SYS_I2C_M_B]           = &sys_i2c_m_b.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_I2C_M_F]           = &sys_i2c_m_f.hw,
	[CLKID_SYS_HDMITX_APB]        = &sys_hdmitx_apb.hw,
	[CLKID_SYS_I2C_S_A]           = &sys_i2c_s_a.hw,
	[CLKID_SYS_USB1_TO_DDR]       = &sys_usb1_to_ddr.hw,
	[CLKID_SYS_HDCP22]            = &sys_hdcp22.hw,
	[CLKID_SYS_MMC_APB]           = &sys_mmc_apb.hw,
	[CLKID_SYS_RSA]               = &sys_rsa.hw,
	[CLKID_SYS_CPU_DEBUG]         = &sys_cpu_debug.hw,
	[CLKID_SYS_VPU_INTR]          = &sys_vpu_intr.hw,
	[CLKID_SYS_DEMOD]             = &sys_demod.hw,
	[CLKID_SYS_SAR_ADC]           = &sys_sar_adc.hw,
	[CLKID_SYS_GIC]               = &sys_gic.hw,
	[CLKID_SYS_PWM_AB]            = &sys_pwm_ab.hw,
	[CLKID_SYS_PWM_CD]            = &sys_pwm_cd.hw,
	[CLKID_SYS_PWM_EF]            = &sys_pwm_ef.hw,
	[CLKID_SYS_PWM_GH]            = &sys_pwm_gh.hw,
	[CLKID_SYS_PWM_IJ]            = &sys_pwm_ij.hw
};

static struct clk_hw *s4d_hw_clks[] = {
	[CLKID_GP0_PLL]               = &gp0_pll.hw,
	[CLKID_HIFI_PLL]              = &s4d_hifi_pll.hw,
	[CLKID_HDMI_PLL_DCO]          = &hdmi_pll_dco.hw,
	[CLKID_MPLL_PREDIV]           = &mpll_prediv.hw,
	[CLKID_MPLL0]                 = &mpll0.hw,
	[CLKID_MPLL0_DIV]             = &mpll0_div.hw,
	[CLKID_MPLL1]                 = &mpll1.hw,
	[CLKID_MPLL1_DIV]             = &mpll1_div.hw,
	[CLKID_MPLL2]                 = &mpll2.hw,
	[CLKID_MPLL2_DIV]             = &mpll2_div.hw,
	[CLKID_MPLL3]                 = &mpll3.hw,
	[CLKID_MPLL3_DIV]             = &mpll3_div.hw,
	[CLKID_HDMI_PLL_OD]           = &hdmi_pll_od.hw,
	[CLKID_HDMI_PLL]              = &hdmi_pll.hw,
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
	[CLKID_CECA_DUAL_CLKIN]       = &ceca_dual_clkin.hw,
	[CLKID_CECA_DUAL_DIV]         = &ceca_dual_div.hw,
	[CLKID_CECA_DUAL_MUX]         = &ceca_dual_mux.hw,
	[CLKID_CECA_DUAL]             = &ceca_dual.hw,
	[CLKID_CECA_CLK]              = &ceca_clk.hw,
	[CLKID_CECB_DUAL_CLKIN]       = &cecb_dual_clkin.hw,
	[CLKID_CECB_DUAL_DIV]         = &cecb_dual_div.hw,
	[CLKID_CECB_DUAL_MUX]         = &cecb_dual_mux.hw,
	[CLKID_CECB_DUAL]             = &cecb_dual.hw,
	[CLKID_CECB_CLK]              = &cecb_clk.hw,
	[CLKID_RAMA_0_MUX]            = &rama_0_mux.hw,
	[CLKID_RAMA_0_DIV]            = &rama_0_div.hw,
	[CLKID_RAMA_0]                = &rama_0.hw,
	[CLKID_RAMA_1_MUX]            = &rama_1_mux.hw,
	[CLKID_RAMA_1_DIV]            = &rama_1_div.hw,
	[CLKID_RAMA_1]                = &rama_1.hw,
	[CLKID_RAMA]                  = &rama.hw,
	[CLKID_SAR_ADC_MUX]           = &sar_adc_mux.hw,
	[CLKID_SAR_ADC_DIV]           = &sar_adc_div.hw,
	[CLKID_SAR_ADC]               = &sar_adc.hw,
	[CLKID_SC_MUX]                = &sc_mux.hw,
	[CLKID_SC_DIV]                = &sc_div.hw,
	[CLKID_SC]                    = &sc.hw,
	[CLKID_CDAC_MUX]              = &cdac_mux.hw,
	[CLKID_CDAC_DIV]              = &cdac_div.hw,
	[CLKID_CDAC]                  = &cdac.hw,
	[CLKID_VID_PLL_DIV]           = &vid_pll_div.hw,
	[CLKID_VID_PLL_SEL]           = &vid_pll_sel.hw,
	[CLKID_VID_PLL]               = &vid_pll.hw,
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
	[CLKID_HDMITX_PIXEL_MUX]      = &hdmitx_pixel_mux.hw,
	[CLKID_HDMITX_PIXEL]          = &hdmitx_pixel.hw,
	[CLKID_ENCI_MUX]              = &enci_mux.hw,
	[CLKID_ENCI]                  = &enci.hw,
	[CLKID_ENCP_MUX]              = &encp_mux.hw,
	[CLKID_ENCP]                  = &encp.hw,
	[CLKID_ENCT_MUX]              = &enct_mux.hw,
	[CLKID_ENCT]                  = &enct.hw,
	[CLKID_ENCL_MUX]              = &encl_mux.hw,
	[CLKID_ENCL]                  = &encl.hw,
	[CLKID_VDAC_MUX]              = &vdac_mux.hw,
	[CLKID_VDAC]                  = &vdac.hw,
	[CLKID_VID_LOCK_MUX]          = &vid_lock_mux.hw,
	[CLKID_VID_LOCK_DIV]          = &vid_lock_div.hw,
	[CLKID_VID_LOCK]              = &vid_lock.hw,
	[CLKID_MALI_0_MUX]            = &mali_0_mux.hw,
	[CLKID_MALI_0_DIV]            = &mali_0_div.hw,
	[CLKID_MALI_0]                = &mali_0.hw,
	[CLKID_MALI_1_MUX]            = &mali_1_mux.hw,
	[CLKID_MALI_1_DIV]            = &mali_1_div.hw,
	[CLKID_MALI_1]                = &mali_1.hw,
	[CLKID_MALI]                  = &mali.hw,
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
	[CLKID_VDIN_MEAS_MUX]         = &vdin_meas_mux.hw,
	[CLKID_VDIN_MEAS_DIV]         = &vdin_meas_div.hw,
	[CLKID_VDIN_MEAS]             = &vdin_meas.hw,
	[CLKID_SD_EMMC_A_MUX]         = &s4d_sd_emmc_a_mux.hw,
	[CLKID_SD_EMMC_A_DIV]         = &s4d_sd_emmc_a_div.hw,
	[CLKID_SD_EMMC_A]             = &s4d_sd_emmc_a.hw,
	[CLKID_SD_EMMC_B_MUX]         = &s4d_sd_emmc_b_mux.hw,
	[CLKID_SD_EMMC_B_DIV]         = &s4d_sd_emmc_b_div.hw,
	[CLKID_SD_EMMC_B]             = &s4d_sd_emmc_b.hw,
	[CLKID_SD_EMMC_C_MUX]         = &s4d_sd_emmc_c_mux.hw,
	[CLKID_SD_EMMC_C_DIV]         = &s4d_sd_emmc_c_div.hw,
	[CLKID_SD_EMMC_C]             = &s4d_sd_emmc_c.hw,
	[CLKID_SPICC0_MUX]            = &spicc0_mux.hw,
	[CLKID_SPICC0_DIV]            = &spicc0_div.hw,
	[CLKID_SPICC0]                = &spicc0.hw,
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
	[CLKID_HDMITX_SYS_MUX]        = &hdmitx_sys_mux.hw,
	[CLKID_HDMITX_SYS_DIV]        = &hdmitx_sys_div.hw,
	[CLKID_HDMITX_SYS]            = &hdmitx_sys.hw,
	[CLKID_HDCP22_ESM_MUX]        = &hdcp22_esm_mux.hw,
	[CLKID_HDCP22_ESM_DIV]        = &hdcp22_esm_div.hw,
	[CLKID_HDCP22_ESM]            = &hdcp22_esm.hw,
	[CLKID_HDCP22_SKP_MUX]        = &hdcp22_skp_mux.hw,
	[CLKID_HDCP22_SKP_DIV]        = &hdcp22_skp_div.hw,
	[CLKID_HDCP22_SKP]            = &hdcp22_skp.hw,
	[CLKID_DEMOD_CORE_MUX]        = &demod_core_mux.hw,
	[CLKID_DEMOD_CORE_DIV]        = &demod_core_div.hw,
	[CLKID_DEMOD_CORE]            = &demod_core.hw,
	[CLKID_DEMOD_CORE_T2_MUX]     = &s4d_demod_core_t2_mux.hw,
	[CLKID_DEMOD_CORE_T2_DIV]     = &s4d_demod_core_t2_div.hw,
	[CLKID_DEMOD_CORE_T2]         = &s4d_demod_core_t2.hw,
	[CLKID_DEMOD_32K_CLKIN]       = &s4d_demod_32k_clkin.hw,
	[CLKID_DEMOD_32K_DIV]         = &s4d_demod_32k_div.hw,
	[CLKID_DEMOD_32K_MUX]         = &s4d_demod_32k_mux.hw,
	[CLKID_DEMOD_32K]             = &s4d_demod_32k.hw,
	[CLKID_ADC_EXTCLK_IN_MUX]     = &adc_extclk_in_mux.hw,
	[CLKID_ADC_EXTCLK_IN_DIV]     = &adc_extclk_in_div.hw,
	[CLKID_ADC_EXTCLK_IN]         = &adc_extclk_in.hw,
	[CLKID_ETH_CLK_RMII_DIV]      = &eth_clk_rmii_div.hw,
	[CLKID_ETH_CLK_RMII]          = &eth_clk_rmii.hw,
	[CLKID_ETH_CLK125M_DIV]       = &eth_clk125M_div.hw,
	[CLKID_ETH_CLK125M]           = &eth_clk125M.hw,
	[CLKID_TS_DIV]                = &ts_div.hw,
	[CLKID_TS]                    = &ts.hw,
	[CLKID_GEN_MUX]               = &gen_mux.hw,
	[CLKID_GEN_DIV]               = &gen_div.hw,
	[CLKID_GEN]                   = &gen.hw,
	[CLKID_SYS_DDR]               = &sys_ddr.hw,
	[CLKID_SYS_DOS]               = &sys_dos.hw,
	[CLKID_SYS_ETH_PHY]           = &sys_eth_phy.hw,
	[CLKID_SYS_MALI]              = &sys_mali.hw,
	[CLKID_SYS_AOCPU]             = &sys_aocpu.hw,
	[CLKID_SYS_AUCPU]             = &sys_aucpu.hw,
	[CLKID_SYS_CEC]               = &sys_cec.hw,
	[CLKID_SYS_SD_EMMC_A]         = &sys_sd_emmc_a.hw,
	[CLKID_SYS_SD_EMMC_B]         = &sys_sd_emmc_b.hw,
	[CLKID_SYS_SD_EMMC_C]         = &sys_sd_emmc_c.hw,
	[CLKID_SYS_SMARTCARD]         = &sys_smartcard.hw,
	[CLKID_SYS_ACODEC]            = &sys_acodec.hw,
	[CLKID_SYS_SPIFC]             = &sys_spifc.hw,
	[CLKID_SYS_MSR_CLK]           = &sys_msr_clk.hw,
	[CLKID_SYS_IR_CTRL]           = &sys_ir_ctrl.hw,
	[CLKID_SYS_AUDIO]             = &sys_audio.hw,
	[CLKID_SYS_ETH]               = &sys_eth.hw,
	[CLKID_SYS_UART_A]            = &sys_uart_a.hw,
	[CLKID_SYS_UART_B]            = &sys_uart_b.hw,
	[CLKID_SYS_UART_C]            = &sys_uart_c.hw,
	[CLKID_SYS_UART_D]            = &sys_uart_d.hw,
	[CLKID_SYS_UART_E]            = &sys_uart_e.hw,
	[CLKID_SYS_AIFIFO]            = &sys_aififo.hw,
	[CLKID_SYS_TS_DDR]            = &sys_ts_ddr.hw,
	[CLKID_SYS_TS_PLL]            = &sys_ts_pll.hw,
	[CLKID_SYS_GE2D]              = &sys_ge2d.hw,
	[CLKID_SYS_SPICC0]            = &sys_spicc0.hw,
	[CLKID_SYS_USB]               = &sys_usb.hw,
	[CLKID_SYS_I2C_M_A]           = &sys_i2c_m_a.hw,
	[CLKID_SYS_I2C_M_B]           = &sys_i2c_m_b.hw,
	[CLKID_SYS_I2C_M_C]           = &sys_i2c_m_c.hw,
	[CLKID_SYS_I2C_M_D]           = &sys_i2c_m_d.hw,
	[CLKID_SYS_I2C_M_E]           = &sys_i2c_m_e.hw,
	[CLKID_SYS_I2C_M_F]           = &sys_i2c_m_f.hw,
	[CLKID_SYS_HDMITX_APB]        = &sys_hdmitx_apb.hw,
	[CLKID_SYS_I2C_S_A]           = &sys_i2c_s_a.hw,
	[CLKID_SYS_USB1_TO_DDR]       = &sys_usb1_to_ddr.hw,
	[CLKID_SYS_HDCP22]            = &sys_hdcp22.hw,
	[CLKID_SYS_MMC_APB]           = &sys_mmc_apb.hw,
	[CLKID_SYS_RSA]               = &sys_rsa.hw,
	[CLKID_SYS_CPU_DEBUG]         = &sys_cpu_debug.hw,
	[CLKID_SYS_VPU_INTR]          = &sys_vpu_intr.hw,
	[CLKID_SYS_DEMOD]             = &sys_demod.hw,
	[CLKID_SYS_SAR_ADC]           = &sys_sar_adc.hw,
	[CLKID_SYS_GIC]               = &sys_gic.hw,
	[CLKID_SYS_PWM_AB]            = &sys_pwm_ab.hw,
	[CLKID_SYS_PWM_CD]            = &sys_pwm_cd.hw,
	[CLKID_SYS_PWM_EF]            = &sys_pwm_ef.hw,
	[CLKID_SYS_PWM_GH]            = &sys_pwm_gh.hw,
	[CLKID_SYS_PWM_IJ]            = &sys_pwm_ij.hw
};

static const struct meson_clk_hw_data s4_clks = {
	.hws = s4_hw_clks,
	.num = ARRAY_SIZE(s4_hw_clks),
};

static const struct meson_clk_hw_data s4d_clks = {
	.hws = s4d_hw_clks,
	.num = ARRAY_SIZE(s4d_hw_clks),
};

static int meson_s4_probe(struct platform_device *pdev)
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
		.compatible = "amlogic,s4-clkc",
		.data = &s4_clks
	},
	{
		.compatible = "amlogic,s4d-clkc",
		.data = &s4d_clks
	},
	{}
};

static struct platform_driver s4_driver = {
	.probe			= meson_s4_probe,
	.driver			= {
		.name		= "s4-clkc",
		.of_match_table	= clkc_match_table,
	},
};

builtin_platform_driver(s4_driver);
MODULE_LICENSE("GPL v2");
