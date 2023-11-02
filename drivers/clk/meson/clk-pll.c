// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 Endless Mobile, Inc.
 * Author: Carlo Caione <carlo@endlessm.com>
 *
 * Copyright (c) 2018 Baylibre, SAS.
 * Author: Jerome Brunet <jbrunet@baylibre.com>
 */

/*
 * In the most basic form, a Meson PLL is composed as follows:
 *
 *                     PLL
 *        +--------------------------------+
 *        |                                |
 *        |             +--+               |
 *  in >>-----[ /N ]--->|  |      +-----+  |
 *        |             |  |------| DCO |---->> out
 *        |  +--------->|  |      +--v--+  |
 *        |  |          +--+         |     |
 *        |  |                       |     |
 *        |  +--[ *(M + (F/Fmax) ]<--+     |
 *        |                                |
 *        +--------------------------------+
 *
 * out = in * (m + frac / frac_max) / n
 */

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/rational.h>
#include "clk-regmap.h"
#include "clk-secure.h"
#include "clk-pll.h"
#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/arm-smccc.h>
#endif

#define FIXED_FRAC_WEIGHT_PRECISION	100000
#define MESON_PLL_THRESHOLD_RATE	1500000000
/*
 * SoC list with threshold rate :
 *	TXHD2
 */

static inline struct meson_clk_pll_data *
meson_clk_pll_data(struct clk_regmap *clk)
{
	return (struct meson_clk_pll_data *)clk->data;
}

static int __pll_round_closest_mult(struct meson_clk_pll_data *pll)
{
	if ((pll->flags & CLK_MESON_PLL_ROUND_CLOSEST) &&
	    !MESON_PARM_APPLICABLE(&pll->frac))
		return 1;

	return 0;
}

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static unsigned long __pll_params_to_rate(unsigned long parent_rate,
					  unsigned int m, unsigned int n,
					  unsigned int frac,
					  struct meson_clk_pll_data *pll,
					  unsigned int od)
#else
static unsigned long __pll_params_to_rate(unsigned long parent_rate,
					  unsigned int m, unsigned int n,
					  unsigned int frac,
					  struct meson_clk_pll_data *pll)
#endif
{
	u64 rate;
	u64 frac_rate;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		parent_rate = parent_rate >> 1;

	rate = (u64)parent_rate * m;
	if (frac && pll->frac.width > 2) {
		frac_rate = (u64)parent_rate * frac;
		if (frac & (1 << (pll->frac.width - 1))) {
			if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
				rate -= DIV_ROUND_UP_ULL(frac_rate, FIXED_FRAC_WEIGHT_PRECISION);
			else
				rate -= DIV_ROUND_UP_ULL(frac_rate,
						 ((u32)1 << (pll->frac.width - 2)));
		} else {
			if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
				rate += DIV_ROUND_UP_ULL(frac_rate, FIXED_FRAC_WEIGHT_PRECISION);
			else
				rate += DIV_ROUND_UP_ULL(frac_rate,
						 ((u32)1 << (pll->frac.width - 2)));
		}
	}

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
		n = 1 << n;

	if (n == 0)
		return 0;

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	return DIV_ROUND_UP_ULL(rate, n) >> od;
#else
	return DIV_ROUND_UP_ULL(rate, n);
#endif
}

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static unsigned long meson_clk_pll_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac, od;
	unsigned long rate;

	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);
	od = MESON_PARM_APPLICABLE(&pll->od) ?
		meson_parm_read(clk->map, &pll->od) :
		0;

	frac = MESON_PARM_APPLICABLE(&pll->frac) ?
		meson_parm_read(clk->map, &pll->frac) :
		0;

	rate = __pll_params_to_rate(parent_rate, m, n, frac, pll, od);

	return rate;
}
#else
static unsigned long meson_clk_pll_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac;
	unsigned long rate;

	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);

	frac = MESON_PARM_APPLICABLE(&pll->frac) ?
		meson_parm_read(clk->map, &pll->frac) :
		0;

	rate = __pll_params_to_rate(parent_rate, m, n, frac, pll);

	return rate;
}
#endif

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static unsigned int __pll_params_with_frac(unsigned long rate,
					   unsigned long parent_rate,
					   unsigned int m,
					   unsigned int n,
					   unsigned int od,
					   struct meson_clk_pll_data *pll)
{
	unsigned int frac_max;
	u64 val;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		parent_rate = parent_rate >> 1;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO) {
		val = (u64)rate << n;
		if (rate < ((parent_rate >> n) * m >> od))
			return 0;
	} else {
		val = (u64)rate * n;
		/* Bail out if we are already over the requested rate */
		if (rate < (div_u64((u64)parent_rate * m, n) >> od))
			return 0;
	}

	if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
		frac_max = FIXED_FRAC_WEIGHT_PRECISION;
	else
		frac_max = (1 << (pll->frac.width - 2));

	val = val * (1 << od);
	if (pll->flags & CLK_MESON_PLL_ROUND_CLOSEST)
		val = DIV_ROUND_CLOSEST_ULL(val * frac_max, parent_rate);
	else
		val = div_u64(val * frac_max, parent_rate);

	val -= m * frac_max;

	return min((unsigned int)val, (frac_max - 1));
}
#else
static unsigned int __pll_params_with_frac(unsigned long rate,
					   unsigned long parent_rate,
					   unsigned int m,
					   unsigned int n,
					   struct meson_clk_pll_data *pll)
{
	unsigned int frac_max;
	u64 val;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		parent_rate = parent_rate >> 1;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO) {
		val = (u64)rate << n;
		if (rate < ((parent_rate >> n) * m))
			return 0;
	} else {
		val = (u64)rate * n;
		/* Bail out if we are already over the requested rate */
		if (rate < (div_u64((u64)parent_rate * m, n)))
			return 0;
	}

	if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
		frac_max = FIXED_FRAC_WEIGHT_PRECISION;
	else
		frac_max = (1 << (pll->frac.width - 2));

	if (pll->flags & CLK_MESON_PLL_ROUND_CLOSEST)
		val = DIV_ROUND_CLOSEST_ULL(val * frac_max, parent_rate);
	else
		val = div_u64(val * frac_max, parent_rate);

	val -= m * frac_max;

	return min((unsigned int)val, (frac_max - 1));
}
#endif

static bool meson_clk_pll_is_better(unsigned long rate,
				    unsigned long best,
				    unsigned long now,
				    struct meson_clk_pll_data *pll)
{
	if (__pll_round_closest_mult(pll)) {
		/* Round Closest */
		if (abs(now - rate) < abs(best - rate))
			return true;
	} else {
		/* Round down */
		if (now <= rate && best < now)
			return true;
	}

	return false;
}

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static int meson_clk_get_pll_table_index(unsigned int index,
					 unsigned int *m,
					 unsigned int *n,
					 struct meson_clk_pll_data *pll,
					 unsigned int *od)
#else
static int meson_clk_get_pll_table_index(unsigned int index,
					 unsigned int *m,
					 unsigned int *n,
					 struct meson_clk_pll_data *pll)
#endif
{
	/* In some SoCs, n = 0, so check m here */
	if (!pll->table[index].m) {
		pr_debug("%s, table idx = %u, m = %u\n", __func__,
			index, pll->table[index].m);
		return -EINVAL;
	}

	*m = pll->table[index].m;
	*n = pll->table[index].n;
#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	*od = pll->table[index].od;
#endif

	return 0;
}

static unsigned int meson_clk_get_pll_range_m(unsigned long long rate,
					      unsigned long parent_rate,
					      unsigned int n,
					      struct meson_clk_pll_data *pll)
{
	u64 val;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
		val = rate << n;
	else
		val = rate * n;
	if (__pll_round_closest_mult(pll))
		return DIV_ROUND_CLOSEST_ULL(val, parent_rate);

	return div_u64(val,  parent_rate);
}

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static int meson_clk_get_pll_range_index(unsigned long rate,
					 unsigned long parent_rate,
					 unsigned int index,
					 unsigned int *m,
					 unsigned int *n,
					 struct meson_clk_pll_data *pll,
					 unsigned int *od)
{
	*od = index;
	if (*od >= (1 << pll->od.width))/*n fixed only check od*/
		return -EINVAL;

	if (pll->flags & CLK_MESON_PLL_FIXED_N) {
		*n = pll->fixed_n;
	} else {
		if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
			*n = 0;
		else
			*n = 1;
	}
	if (rate > __pll_params_to_rate(parent_rate,
		pll->range->max, *n, 0, pll, 0)) {
		*m = pll->range->max;
		*od = 0;
		return -ENODATA;
	} else if (rate < __pll_params_to_rate(parent_rate,
			pll->range->min, *n, 0, pll, ((1 << pll->od.width) - 1))) {
		*m = pll->range->min;
		*od = (1 << pll->od.width) - 1;
		return -ENODATA;
	}
	/*cal param according to dco rate range, rate must be dco out*/
	*m = meson_clk_get_pll_range_m((u64)rate << *od, parent_rate, *n, pll);
	if (*m > pll->range->max || *m < pll->range->min)
		return -EAGAIN;/*if m out of range,should try again according next od*/

	return 0;
}

static int meson_clk_get_pll_get_index(unsigned long rate,
				       unsigned long parent_rate,
				       unsigned int index,
				       unsigned int *m,
				       unsigned int *n,
				       struct meson_clk_pll_data *pll,
				       unsigned int *od)
{
	if (pll->range)
		return meson_clk_get_pll_range_index(rate, parent_rate,
						     index, m, n, pll, od);
	else if (pll->table)
		return meson_clk_get_pll_table_index(index, m, n, pll, od);

	return -EINVAL;
}
#else
static int meson_clk_get_pll_range_index(unsigned long rate,
					 unsigned long parent_rate,
					 unsigned int index,
					 unsigned int *m,
					 unsigned int *n,
					 struct meson_clk_pll_data *pll)
{
	*n = index + 1;

	/* Check the predivider range */
	if (*n >= (1 << pll->n.width))
		return -EINVAL;

	if (*n == 1) {
		/* Get the boundaries out the way */
		if (rate <= pll->range->min * parent_rate) {
			*m = pll->range->min;
			return -ENODATA;
		} else if (rate >= pll->range->max * parent_rate) {
			*m = pll->range->max;
			return -ENODATA;
		}
	}

	*m = meson_clk_get_pll_range_m(rate, parent_rate, *n, pll);

	/* the pre-divider gives a multiplier too big - stop */
	if (*m >= (1 << pll->m.width))
		return -EINVAL;

	return 0;
}

static int meson_clk_get_pll_get_index(unsigned long rate,
				       unsigned long parent_rate,
				       unsigned int index,
				       unsigned int *m,
				       unsigned int *n,
				       struct meson_clk_pll_data *pll)
{
	if (pll->range)
		return meson_clk_get_pll_range_index(rate, parent_rate,
						     index, m, n, pll);
	else if (pll->table)
		return meson_clk_get_pll_table_index(index, m, n, pll);

	return -EINVAL;
}
#endif

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static int meson_clk_get_pll_settings(unsigned long rate,
				      unsigned long parent_rate,
				      unsigned int *best_m,
				      unsigned int *best_n,
				      struct meson_clk_pll_data *pll,
				      unsigned int *best_od)
{
	unsigned long best = 0, now = 0;
	unsigned int i, m, n, od;
	int ret;

	for (i = 0, ret = 0; (ret == 0 || ret == -EAGAIN); i++) {
		ret = meson_clk_get_pll_get_index(rate, parent_rate,
						  i, &m, &n, pll, &od);
		if (ret == -EINVAL)
			break;
		if (ret == -EAGAIN)
			continue;
		now = __pll_params_to_rate(parent_rate, m, n, 0, pll, od);
		if (meson_clk_pll_is_better(rate, best, now, pll)) {
			best = now;
			*best_m = m;
			*best_n = n;
			*best_od = od;
			if (now == rate)
				break;
		}
	}

	return best ? 0 : -EINVAL;
}
#else
static int meson_clk_get_pll_settings(unsigned long rate,
				      unsigned long parent_rate,
				      unsigned int *best_m,
				      unsigned int *best_n,
				      struct meson_clk_pll_data *pll)
{
	unsigned long best = 0, now = 0;
	unsigned int i, m, n;
	int ret;

	for (i = 0, ret = 0; !ret; i++) {
		ret = meson_clk_get_pll_get_index(rate, parent_rate,
						  i, &m, &n, pll);
		if (ret == -EINVAL)
			break;

		now = __pll_params_to_rate(parent_rate, m, n, 0, pll);
		if (meson_clk_pll_is_better(rate, best, now, pll)) {
			best = now;
			*best_m = m;
			*best_n = n;

			if (now == rate)
				break;
		}
	}

	return best ? 0 : -EINVAL;
}
#endif

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
static long meson_clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac, od;
	unsigned long round;
	int ret;

	ret = meson_clk_get_pll_settings(rate, *parent_rate, &m, &n, pll, &od);
	if (ret)
		return meson_clk_pll_recalc_rate(hw, *parent_rate);

	round = __pll_params_to_rate(*parent_rate, m, n, 0, pll, od);

	if (!MESON_PARM_APPLICABLE(&pll->frac) || rate == round)
		return round;

	/*
	 * The rate provided by the setting is not an exact match, let's
	 * try to improve the result using the fractional parameter
	 */
	frac = __pll_params_with_frac(rate, *parent_rate, m, n, od, pll);

	return __pll_params_to_rate(*parent_rate, m, n, frac, pll, od);
}
#else
static long meson_clk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac;
	unsigned long round;
	int ret;

	ret = meson_clk_get_pll_settings(rate, *parent_rate, &m, &n, pll);
	if (ret)
		return meson_clk_pll_recalc_rate(hw, *parent_rate);

	round = __pll_params_to_rate(*parent_rate, m, n, 0, pll);

	if (!MESON_PARM_APPLICABLE(&pll->frac) || rate == round)
		return round;

	/*
	 * The rate provided by the setting is not an exact match, let's
	 * try to improve the result using the fractional parameter
	 */
	frac = __pll_params_with_frac(rate, *parent_rate, m, n, pll);

	return __pll_params_to_rate(*parent_rate, m, n, frac, pll);
}
#endif

static int meson_clk_pll_wait_lock(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
#ifdef CONFIG_AMLOGIC_MODIFY
	int delay = 1000;

	do {
		/* Is the clock locked now ? */
		if (meson_parm_read(clk->map, &pll->l))
			return 0;
		udelay(1);
	} while (delay--);
#endif

	return -ETIMEDOUT;
}

static int meson_clk_pll_init(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/*
	 * Do not init pll
	 * 1. it will gate pll which is needed in RTOS
	 * 2. it will gate sys pll who is feeding CPU
	 */
	if (pll->flags & CLK_MESON_PLL_IGNORE_INIT) {
		pr_warn("ignore %s clock init\n", clk_hw_get_name(hw));
		return 0;
	}

	if (pll->init_count) {
		meson_parm_write(clk->map, &pll->rst, 1);
		regmap_multi_reg_write(clk->map, pll->init_regs,
				       pll->init_count);
		meson_parm_write(clk->map, &pll->rst, 0);
	}

	return 0;
}

static int meson_clk_pll_is_enabled(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* Enable and lock bit equal 1, it locks */
	if (meson_parm_read(clk->map, &pll->en) &&
	    meson_parm_read(clk->map, &pll->l))
		return 1;

	return 0;
}

static int meson_clk_pcie_pll_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	int retry = 10, ret = 10;

	do {
		meson_clk_pll_init(hw);
		if (!meson_clk_pll_wait_lock(hw)) {
			/* what means here ? */
			if (!MESON_PARM_APPLICABLE(&pll->pcie_hcsl))
				return 0;
			break;
		}
		pr_info("%s:%d retry = %d\n", __func__, __LINE__, retry);
	} while (retry--);

	if (retry <= 0)
		return -EIO;

	if (!MESON_PARM_APPLICABLE(&pll->pcie_hcsl))
		return 0;

	/*pcie pll clk share use for usb phy, so add this operation from ASIC*/
	do {
		if (meson_parm_read(clk->map, &pll->pcie_hcsl)) {
			meson_parm_write(clk->map, &pll->pcie_exen, 0);
			return 0;
		}
		udelay(1);
	} while (ret--);

	if (ret <= 0)
		pr_info("%s:%d pcie reg1 clear bit29 failed\n", __func__, __LINE__);

	return -EIO;
}

static int meson_clk_pll_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	/* Make sure the pll is in reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 0);
	else
		meson_parm_write(clk->map, &pll->rst, 1);

	/* Enable the pll */
	meson_parm_write(clk->map, &pll->en, 1);

	/* Make sure the pll is in lock reset */
	if (MESON_PARM_APPLICABLE(&pll->l_rst)) {
		if (pll->flags & CLK_MESON_PLL_RSTN)
			meson_parm_write(clk->map, &pll->l_rst, 0);
		else
			meson_parm_write(clk->map, &pll->l_rst, 1);
	}

	/*
	 * Make the PLL more stable, if not,
	 * It will probably lock failed (GP0 PLL)
	 */
#ifdef CONFIG_AMLOGIC_MODIFY
	udelay(50);
#endif

	/* Take the pll out reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 1);
	else
		meson_parm_write(clk->map, &pll->rst, 0);

	/* Take the pll out lock reset */
	if (MESON_PARM_APPLICABLE(&pll->l_rst)) {
		udelay(20);
		if (pll->flags & CLK_MESON_PLL_RSTN)
			meson_parm_write(clk->map, &pll->l_rst, 1);
		else
			meson_parm_write(clk->map, &pll->l_rst, 0);
	}

	if (meson_clk_pll_wait_lock(hw))
		return -EIO;

	return 0;
}

static void meson_clk_pll_disable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	if (bypass_clk_disable)
		return;

	/* Put the pll is in reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 0);
	else
		meson_parm_write(clk->map, &pll->rst, 1);

	/* Disable the pll */
	meson_parm_write(clk->map, &pll->en, 0);
}

static int meson_clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int enabled, m, n, frac = 0;
	unsigned long old_rate;
	int ret;
#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	unsigned int od;
#endif

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	old_rate = clk_hw_get_rate(hw);

#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll, &od);
#else
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll);
#endif
	if (ret)
		return ret;

	enabled = meson_parm_read(clk->map, &pll->en);
#ifdef CONFIG_AMLOGIC_MODIFY
	/* Don't disable pll if it's just changing frac */
	if ((meson_parm_read(clk->map, &pll->m) != m ||
	     meson_parm_read(clk->map, &pll->n) != n) && enabled)
		meson_clk_pll_disable(hw);
#endif
	meson_parm_write(clk->map, &pll->n, n);
	meson_parm_write(clk->map, &pll->m, m);
#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
	meson_parm_write(clk->map, &pll->od, od);
#endif

	if (MESON_PARM_APPLICABLE(&pll->frac)) {
#if defined CONFIG_AMLOGIC_MODIFY && defined CONFIG_ARM
		frac = __pll_params_with_frac(rate, parent_rate, m, n, od, pll);
#else
		frac = __pll_params_with_frac(rate, parent_rate, m, n, pll);
#endif
		meson_parm_write(clk->map, &pll->frac, frac);
	}

	/*
	 * The PLL should set together required by the
	 * PLL sequence.
	 * This scenes will cause PLL lock failed
	 *  clk_set_rate(pll);
	 *  wait for a long time, several seconds
	 *  clk_prepare_enable(pll);
	 */
	/* If the pll is stopped, bail out now */
#ifndef CONFIG_AMLOGIC_MODIFY
	if (!enabled)
		return 0;
#endif

	ret = meson_clk_pll_enable(hw);
	if (ret) {
		pr_warn("%s: pll did not lock, trying to restore old rate %lu\n",
			__func__, old_rate);
		/*
		 * FIXME: Do we really need/want this HACK ?
		 * It looks unsafe. what happens if the clock gets into a
		 * broken state and we can't lock back on the old_rate ? Looks
		 * like an infinite recursion is possible
		 */
		meson_clk_pll_set_rate(hw, old_rate, parent_rate);
	}

	return ret;
}

#ifdef CONFIG_AMLOGIC_MODIFY
static int meson_secure_clk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
					 unsigned long parent_rate)
{
	int i = 0, ret = 0;
	struct arm_smccc_res res;

	if (!strcmp(clk_hw_get_name(hw), "sys_pll_dco")) {
		arm_smccc_smc(CLK_SECURE_RW, SYS_PLL_STEP0,
			      rate, 0, 0, 0, 0, 0, &res);
	} else if (!strcmp(clk_hw_get_name(hw), "gp1_pll_dco")) {
		arm_smccc_smc(CLK_SECURE_RW, GP1_PLL_STEP0,
			      rate, 0, 0, 0, 0, 0, &res);
	} else {
		pr_err("%s: %s pll not found!!!\n",
		       __func__, clk_hw_get_name(hw));
		return -EINVAL;
	}

	/* waiting for 10us to rewrite */
	udelay(10);

	if (!strcmp(clk_hw_get_name(hw), "sys_pll_dco"))
		arm_smccc_smc(CLK_SECURE_RW, SYS_PLL_STEP1,
			      0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(CLK_SECURE_RW, GP1_PLL_STEP1,
			      0, 0, 0, 0, 0, 0, &res);

	ret = meson_clk_pll_wait_lock(hw);

	if (ret) {
		pr_info("%s: %s did not lock, trying to lock rate %lu again\n",
			__func__, clk_hw_get_name(hw), rate);
		if (i++ < 10)
			meson_secure_clk_pll_set_rate(hw, rate, parent_rate);
	}
	return ret;
}

static long meson_secure_clk_pll_round_rate(struct clk_hw *hw,
					    unsigned long rate,
					    unsigned long *parent_rate)
{
	return rate;
}

static int meson_secure_clk_pll_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int first_set = 1, val;
	struct clk_hw *parent = clk_hw_get_parent(hw);
	u64 rate;

	if (meson_parm_read(clk->map, &pll->en))
		return 0;

	if (!strcmp(clk_hw_get_name(hw), "sys_pll_dco") ||
	    !strcmp(clk_hw_get_name(hw), "gp1_pll_dco")) {
		regmap_read(clk->map, (&pll->en)->reg_off + (6 * 4), &val);
		if (val == 0x56540000)
			first_set = 0;
	}

	/*First init, just set minimal rate.*/
	if (first_set) {
#ifdef CONFIG_ARM
		rate = __pll_params_to_rate(clk_hw_get_rate(parent),
					    pll->table[0].m,
					    pll->table[0].n, 0, pll,
					    pll->table[0].od);
#else
		rate = __pll_params_to_rate(clk_hw_get_rate(parent),
					    pll->table[0].m,
					    pll->table[0].n, 0, pll);
#endif
	} else {
		rate = meson_clk_pll_recalc_rate(hw, clk_hw_get_rate(parent));
		rate = meson_secure_clk_pll_round_rate(hw, rate, NULL);
	}

	return meson_secure_clk_pll_set_rate(hw, rate, clk_hw_get_rate(parent));
}

static void meson_secure_clk_pll_disable(struct clk_hw *hw)
{
	struct arm_smccc_res res;

	if (!strcmp(clk_hw_get_name(hw), "sys_pll_dco"))
		arm_smccc_smc(CLK_SECURE_RW, SYS_PLL_DISABLE,
			      0, 0, 0, 0, 0, 0, &res);
	else
		arm_smccc_smc(CLK_SECURE_RW, GP1_PLL_DISABLE,
			      0, 0, 0, 0, 0, 0, &res);
}
#endif

/*
 * The Meson G12A PCIE PLL is fined tuned to deliver a very precise
 * 100MHz reference clock for the PCIe Analog PHY, and thus requires
 * a strict register sequence to enable the PLL.
 * To simplify, re-use the _init() op to enable the PLL and keep
 * the other ops except set_rate since the rate is fixed.
 */
const struct clk_ops meson_clk_pcie_pll_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
	.enable		= meson_clk_pcie_pll_enable,
	.disable	= meson_clk_pll_disable
};
EXPORT_SYMBOL_GPL(meson_clk_pcie_pll_ops);

const struct clk_ops meson_clk_pll_ops = {
	.init		= meson_clk_pll_init,
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate	= meson_clk_pll_set_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
	.enable		= meson_clk_pll_enable,
	.disable	= meson_clk_pll_disable
};
EXPORT_SYMBOL_GPL(meson_clk_pll_ops);

#ifdef CONFIG_AMLOGIC_MODIFY
const struct clk_ops meson_secure_clk_pll_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_secure_clk_pll_round_rate,
	.set_rate	= meson_secure_clk_pll_set_rate,
	.enable		= meson_secure_clk_pll_enable,
	.disable	= meson_secure_clk_pll_disable
};
EXPORT_SYMBOL_GPL(meson_secure_clk_pll_ops);

static void meson_secure_pll_v2_disable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	struct arm_smccc_res res;

	if (bypass_clk_disable)
		return;

	arm_smccc_smc(pll->smc_id, pll->secid_disable,
		      0, 0, 0, 0, 0, 0, &res);
}

static int meson_secure_pll_v2_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	struct arm_smccc_res res;
	unsigned int m, n, ret = 0;
#ifdef CONFIG_ARM
	unsigned int od;
#endif

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

#ifdef CONFIG_ARM
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll, &od);
#else
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll);
#endif
	if (ret)
		return ret;

	if (meson_parm_read(clk->map, &pll->en))
		meson_secure_pll_v2_disable(hw);

#ifdef CONFIG_ARM
	arm_smccc_smc(pll->smc_id, pll->secid,
		      m, n, od, 0, 0, 0, &res);
#else
	arm_smccc_smc(pll->smc_id, pll->secid,
		      m, n, 0, 0, 0, 0, &res);
#endif

	return 0;
}

static int meson_secure_pll_v2_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, od;
	struct arm_smccc_res res;

	/*
	 * In most case,  do nothing if the PLL is already enabled
	 */
	if (clk_hw_is_enabled(hw))
		return 0;

	/* If PLL is not enabled because setting the same rate,
	 * Enable it again, CCF will return when set the same rate
	 */
	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);
	/* od is required in arm64 and arm */
	od = meson_parm_read(clk->map, &pll->od);

	arm_smccc_smc(pll->smc_id, pll->secid,
		      m, n, od, 0, 0, 0, &res);

	return 0;
}

const struct clk_ops meson_secure_pll_v2_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate	= meson_secure_pll_v2_set_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
	.enable		= meson_secure_pll_v2_enable,
	.disable	= meson_secure_pll_v2_disable
};
EXPORT_SYMBOL_GPL(meson_secure_pll_v2_ops);

static int meson_clk_pll_v3_set_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long parent_rate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	struct parm *pm = &pll->m;
	struct parm *pn = &pll->n;
	struct parm *pth = &pll->th;
	struct parm *pfrac = &pll->frac;
	struct parm *pfl = &pll->fl;
	unsigned int m, n, frac;
	unsigned int val;
	const struct reg_sequence *init_regs = pll->init_regs;
	int i, ret = 0, retry = 10;
#ifdef CONFIG_ARM
	unsigned int od;
	struct parm *pod = &pll->od;
#endif

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	/* calculate M, N, OD*/
#ifdef CONFIG_ARM
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll, &od);
#else
	ret = meson_clk_get_pll_settings(rate, parent_rate, &m, &n, pll);
#endif
	if (ret)
		return ret;

	/* calute frac */
	if (MESON_PARM_APPLICABLE(&pll->frac)) {
#ifdef CONFIG_ARM
		frac = __pll_params_with_frac(rate, parent_rate, m, n, od, pll);
#else
		frac = __pll_params_with_frac(rate, parent_rate, m, n, pll);
#endif
		/*
		 * Can update the register corresponding to frac directly
		 * if just change frac and pll to enable.
		 */
		if (meson_parm_read(clk->map, &pll->m) == m &&
		meson_parm_read(clk->map, &pll->n) == n &&
#ifdef CONFIG_ARM
		meson_parm_read(clk->map, &pll->od) == od &&
#endif
		meson_parm_read(clk->map, &pll->en)) {
			regmap_read(clk->map, pfrac->reg_off, &val);
			/* Clear Frac bits and Update frac value */
			val &= CLRPMASK(pfrac->width, pfrac->shift);
			val |= frac << pfrac->shift;
			regmap_write(clk->map, pfrac->reg_off, val);

			if (!meson_clk_pll_wait_lock(hw))
				return 0;
		}
	}

	if (meson_parm_read(clk->map, &pll->en))
		meson_clk_pll_disable(hw);

	do {
		for (i = 0; i < pll->init_count; i++) {
			if (pn->reg_off == init_regs[i].reg) {
				/* Clear M N bits and Update M N value */
				val = init_regs[i].def;
				if (MESON_PARM_APPLICABLE(&pll->th)) {
					val &= CLRPMASK(pth->width, pth->shift);
#ifdef CONFIG_ARM
					if (__pll_params_to_rate(parent_rate, m, n, frac, pll, 0)
						>= MESON_PLL_THRESHOLD_RATE)
						val |= 1 << pth->shift;
#else
					if (__pll_params_to_rate(parent_rate, m, n, frac, pll)
						>= MESON_PLL_THRESHOLD_RATE)
						val |= 1 << pth->shift;
#endif
				}
				val &= CLRPMASK(pn->width, pn->shift);
				val &= CLRPMASK(pm->width, pm->shift);
				val |= n << pn->shift;
				val |= m << pm->shift;
#ifdef CONFIG_ARM
				val &= CLRPMASK(pod->width, pod->shift);
				val |= od << pod->shift;
#endif
				regmap_write(clk->map, pn->reg_off, val);
			} else if (pfrac->reg_off == init_regs[i].reg &&
				(MESON_PARM_APPLICABLE(&pll->frac))) {
				/* Clear Frac bits and Update Frac value */
				val = init_regs[i].def;
				val &= CLRPMASK(pfrac->width, pfrac->shift);
				val |= frac << pfrac->shift;
				regmap_write(clk->map, pfrac->reg_off, val);
			} else if (MESON_PARM_APPLICABLE(&pll->fl) &&
				   pfl->reg_off == init_regs[i].reg) {
				val = init_regs[i].def;
				val &= CLRPMASK(pfl->width, pfl->shift);
				regmap_write(clk->map, pfl->reg_off, val);
			} else {
				val = init_regs[i].def;
				regmap_write(clk->map, init_regs[i].reg, val);
			}
			if (init_regs[i].delay_us)
				udelay(init_regs[i].delay_us);
		}

		if (!meson_clk_pll_wait_lock(hw)) {
			/*
			 * In special scenarios (such as ESD interference), the pll clock
			 * source may fluctuate due to interference, and the fluctuations
			 * will recover in a short time. enable the fl bit to prevent the
			 * pll from ignoring interference and force lock
			 */
			if (MESON_PARM_APPLICABLE(&pll->fl))
				regmap_update_bits(clk->map, pfl->reg_off, BIT(pfl->shift),
						   BIT(pfl->shift));

			break;
		}
		retry--;
	} while (retry > 0);

	if (retry == 0) {
		pr_err("%s pll locked failed\n", clk_hw_get_name(hw));
		ret = -EIO;
	}

	return ret;
}

static int meson_clk_pll_v3_enable(struct clk_hw *hw)
{
	unsigned long rate, parent_rate;

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	/* Deal clk_set_rate return when set the same rate */
	parent_rate = clk_hw_get_rate(clk_hw_get_parent(hw));
	rate = meson_clk_pll_recalc_rate(hw, parent_rate);
	meson_clk_pll_v3_set_rate(hw, rate, parent_rate);

	if (meson_clk_pll_wait_lock(hw))
		return -EIO;

	return 0;
}

const struct clk_ops meson_clk_pll_v3_ops = {
	/* walk the init regs each time when set a new rate,
	 * init callback is not useful for v3 ops
	 */
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.round_rate	= meson_clk_pll_round_rate,
	.set_rate	= meson_clk_pll_v3_set_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
	.enable		= meson_clk_pll_v3_enable,
	.disable	= meson_clk_pll_disable
};
EXPORT_SYMBOL_GPL(meson_clk_pll_v3_ops);

static unsigned long meson_clk_pll_params_to_rate(struct meson_clk_pll_data *pll,
						  unsigned long prate,
						  u32 m, u32 n, u32 frac, u32 od)
{
	u64 vco_rate, frac_rate;
	u32 frac_precision;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
		n = (u32)BIT(n);

	if (unlikely(!n))
		return 0;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		prate = prate >> 1;

	prate = DIV_ROUND_UP_ULL(prate, n);
	vco_rate = (u64)prate * m;

	if (frac) {
		if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
			frac_precision = FIXED_FRAC_WEIGHT_PRECISION;
		else
			frac_precision = (u32)BIT(pll->frac.width);

		frac_rate = (u64)prate * frac;
		vco_rate += DIV_ROUND_UP_ULL(frac_rate, frac_precision);
	}

	return vco_rate >> od;
}

static unsigned long meson_clk_pll_v4_recalc_rate(struct clk_hw *hw, unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, frac, od;

	n = meson_parm_read(clk->map, &pll->n);
	m = meson_parm_read(clk->map, &pll->m);
	od = meson_parm_read(clk->map, &pll->od);

	if (MESON_PARM_APPLICABLE(&pll->frac))
		frac = meson_parm_read(clk->map, &pll->frac);
	else
		frac = 0;

	return meson_clk_pll_params_to_rate(pll, prate, m, n, frac, od);
}

static unsigned int meson_clk_pll_v4_get_range_m(unsigned long long rate,
					      unsigned long parent_rate,
					      unsigned int n,
					      struct meson_clk_pll_data *pll)
{
	u64 val;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
		val = rate << n;
	else
		val = rate * n;
	if (__pll_round_closest_mult(pll))
		return DIV_ROUND_CLOSEST_ULL(val, parent_rate);

	return div_u64(val,  parent_rate);
}

static int meson_clk_pll_get_range_value(struct meson_clk_pll_data *pll,
					 unsigned long rate,
					 unsigned long prate,
					 unsigned int *m,
					 unsigned int *n,
					 unsigned int *od)
{
	u64 vco_rate;
	u32 idx;
	unsigned int t_m;
	unsigned long now, best = 0;
	unsigned long t_prate = prate;
	u32 od_max = BIT(pll->od.width) - 1;

	/* limit od max value for hw limit */
	if (unlikely(!!pll->od_max))
		od_max = pll->od_max;

	if (pll->flags & CLK_MESON_PLL_FIXED_N) {
		*n = pll->fixed_n;
	} else {
		if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO)
			*n = 0;
		else
			*n = 1;
	}

	for (idx = 0; idx <= od_max; idx++) {
		vco_rate = (u64)rate << idx;
		if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
			t_prate = prate >> 1;
		t_m = meson_clk_pll_v4_get_range_m(vco_rate, t_prate, *n, pll);
		if (t_m > pll->range->max)
			break;
		if (t_m < pll->range->min)
			continue;

		now = meson_clk_pll_params_to_rate(pll, prate, t_m, *n, 0, idx);
		if (meson_clk_pll_is_better(rate, best, now, pll)) {
			best = now;
			*m = t_m;
			*od = idx;
			if (now == rate)
				break;
		}
	}

	return 0;
}

static int meson_clk_pll_get_table_value(struct meson_clk_pll_data *pll,
					 unsigned long rate,
					 unsigned long prate,
					 unsigned int *m,
					 unsigned int *n,
					 unsigned int *od)
{
	u32 idx;
	unsigned int t_m, t_n, t_od;
	unsigned long now, best = 0;

	for (idx = 0; pll->table[idx].m; idx++) {
		t_m = pll->table[idx].m;
		t_n = pll->table[idx].n;
		t_od = pll->table[idx].od;

		now = meson_clk_pll_params_to_rate(pll, prate, t_m, t_n, 0, t_od);
		if (meson_clk_pll_is_better(rate, best, now, pll)) {
			best = now;
			*m = t_m;
			*n = t_n;
			*od = t_od;
			if (now == rate)
				break;
		}
	}

	return 0;
}

static int meson_clk_pll_get_params_table(struct meson_clk_pll_data *pll,
					  unsigned long rate,
					  unsigned long prate,
					  unsigned int *m,
					  unsigned int *n,
					  unsigned int *od)
{
	if (pll->range)
		return meson_clk_pll_get_range_value(pll, rate, prate, m, n, od);
	else if (pll->table)
		return meson_clk_pll_get_table_value(pll, rate, prate, m, n, od);

	return -EINVAL;
}

static unsigned int meson_clk_pll_get_param_frac(struct meson_clk_pll_data *pll,
						 unsigned long rate,
						 unsigned long prate,
						 unsigned int m,
						 unsigned int n,
						 unsigned int od)
{
	unsigned int frac_max;
	u64 vco_rate = (u64)rate << od;
	u64 frac;

	if (pll->flags & CLK_MESON_PLL_FIXED_EN0P5)
		prate = prate >> 1;

	if (pll->flags & CLK_MESON_PLL_POWER_OF_TWO) {
		frac = vco_rate << n;
		if (vco_rate < (((u64)prate >> n) * m))
			return 0;
	} else {
		frac = vco_rate * n;
		/* Bail out if we are already over the requested rate */
		if (vco_rate < (div_u64((u64)prate * m, n)))
			return 0;
	}
	if (pll->flags & CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION)
		frac_max = FIXED_FRAC_WEIGHT_PRECISION;
	else
		frac_max = 1 << pll->frac.width;

	if (pll->flags & CLK_MESON_PLL_ROUND_CLOSEST)
		frac = DIV_ROUND_CLOSEST_ULL(frac * frac_max, prate);
	else
		frac = div_u64(frac * frac_max, prate);

	frac -= (u64)m * frac_max;
	return min_t(unsigned int, frac, frac_max);
}

static int meson_clk_pll_v4_get_params(struct meson_clk_pll_data *pll,
					unsigned long rate,
					unsigned long prate,
					unsigned int *best_m,
					unsigned int *best_n,
					unsigned int *best_od,
					unsigned int *best_frac)
{
	int ret;

	ret = meson_clk_pll_get_params_table(pll, rate, prate, best_m, best_n, best_od);
	if (ret)
		return ret;

	if (MESON_PARM_APPLICABLE(&pll->frac))
		*best_frac = meson_clk_pll_get_param_frac(pll, rate, prate,
							  *best_m, *best_n, *best_od);

	return 0;
}

static int meson_clk_pll_v4_determine_rate(struct clk_hw *hw,
					   struct clk_rate_request *req)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, od, frac;
	int ret;

	if (pll->flags & CLK_MESON_PLL_READ_ONLY) {
		req->rate = clk_hw_get_rate(hw);
		return 0;
	}

	/* calculate M, N, OD*/
	ret = meson_clk_pll_v4_get_params(pll, req->rate, req->best_parent_rate,
					  &m, &n, &od, &frac);
	if (ret)
		return ret;

	req->rate = meson_clk_pll_params_to_rate(pll, req->best_parent_rate,
						 m, n, frac, od);

	return 0;
}

static bool meson_clk_pll_od_or_frac_correct(struct clk_regmap *clk,
					struct meson_clk_pll_data *pll,
					unsigned int m, unsigned int n)
{
	return meson_parm_read(clk->map, &pll->m) == m &&
	       meson_parm_read(clk->map, &pll->n) == n &&
	       meson_parm_read(clk->map, &pll->en);
}

static int meson_clk_pll_v4_init(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	struct pll_rate_range *rate_range = &pll->pll_range;

	if (rate_range->max_rate == 0)
		rate_range->max_rate = ULONG_MAX;

	//if (!pll->range && !pll->table)
	//	return -EINVAL;
	/*
	 * Do not init pll
	 * 1. it will gate pll which is needed in RTOS
	 * 2. it will gate sys pll who is feeding CPU
	 */
	if (pll->flags & CLK_MESON_PLL_IGNORE_INIT) {
		pr_warn("ignore %s clock init\n", clk_hw_get_name(hw));
		return 0;
	}

	if (pll->init_count) {
		if (pll->flags & CLK_MESON_PLL_RSTN) {
			meson_parm_write(clk->map, &pll->rst, 0);
			regmap_multi_reg_write(clk->map, pll->init_regs,
				       pll->init_count);
			meson_parm_write(clk->map, &pll->rst, 1);
		} else {
			meson_parm_write(clk->map, &pll->rst, 1);
			regmap_multi_reg_write(clk->map, pll->init_regs,
				       pll->init_count);
			meson_parm_write(clk->map, &pll->rst, 0);
		}
	}

	return 0;
}

static int meson_clk_pll_v4_enable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	/* Make sure the pll is in reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 0);
	else
		meson_parm_write(clk->map, &pll->rst, 1);

	/* Enable the pll */
	meson_parm_write(clk->map, &pll->en, 1);

	/* Make sure the pll is in lock reset */
	if (MESON_PARM_APPLICABLE(&pll->l_rst)) {
		if (pll->flags & CLK_MESON_PLL_RSTN)
			meson_parm_write(clk->map, &pll->l_rst, 0);
		else
			meson_parm_write(clk->map, &pll->l_rst, 1);
	}

	/* Make sure the pll force lock is clear */
	if (MESON_PARM_APPLICABLE(&pll->fl))
		meson_parm_write(clk->map, &pll->fl, 0);

	/*
	 * Make the PLL more stable, if not,
	 * It will probably lock failed (GP0 PLL)
	 */
	udelay(50);

	/* Take the pll out reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 1);
	else
		meson_parm_write(clk->map, &pll->rst, 0);

	/* Take the pll out lock reset */
	if (MESON_PARM_APPLICABLE(&pll->l_rst)) {
		udelay(20);
		if (pll->flags & CLK_MESON_PLL_RSTN)
			meson_parm_write(clk->map, &pll->l_rst, 1);
		else
			meson_parm_write(clk->map, &pll->l_rst, 0);
	}

	if (meson_clk_pll_wait_lock(hw))
		return -EIO;

	/* Make sure the pll force lock is set */
	if (MESON_PARM_APPLICABLE(&pll->fl))
		meson_parm_write(clk->map, &pll->fl, 1);

	return 0;
}

static void meson_clk_pll_v4_disable(struct clk_hw *hw)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);

	if (bypass_clk_disable)
		return;

	/* Put the pll is in reset */
	if (pll->flags & CLK_MESON_PLL_RSTN)
		meson_parm_write(clk->map, &pll->rst, 0);
	else
		meson_parm_write(clk->map, &pll->rst, 1);

	/* Disable the pll */
	meson_parm_write(clk->map, &pll->en, 0);
}

static int meson_clk_pll_v4_set_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long prate)
{
	struct clk_regmap *clk = to_clk_regmap(hw);
	struct meson_clk_pll_data *pll = meson_clk_pll_data(clk);
	unsigned int m, n, od, frac;
	unsigned int enabled;
	unsigned long old_rate, vco_rate;
	int ret = 0;

	if (prate == 0 || rate == 0)
		return -EINVAL;

	old_rate = clk_hw_get_rate(hw);

	/* calculate M, N, OD*/
	ret = meson_clk_pll_v4_get_params(pll, rate, prate, &m, &n, &od, &frac);
	if (ret)
		return ret;

	/* only frac correct, only write frac register only */
	if (meson_clk_pll_od_or_frac_correct(clk, pll, m, n)) {
		if (unlikely(!!frac) && frac != meson_parm_read(clk->map, &pll->frac))
			/* Clear frac bits and Update frac value */
			meson_parm_write(clk->map, &pll->frac, frac);
		if (unlikely(!!od) && od != meson_parm_read(clk->map, &pll->od))
			/* Clear od bits and Update od value */
			meson_parm_write(clk->map, &pll->od, od);

		return 0;
	}

	enabled = meson_parm_read(clk->map, &pll->en);
	if (enabled)
		meson_clk_pll_v4_disable(hw);

	if (MESON_PARM_APPLICABLE(&pll->th)) {
		vco_rate = meson_clk_pll_params_to_rate(pll, prate, m, n, frac, 0);
		if (vco_rate >= MESON_PLL_THRESHOLD_RATE)
			meson_parm_write(clk->map, &pll->th, 1);
		else
			meson_parm_write(clk->map, &pll->th, 0);
	}

	meson_parm_write(clk->map, &pll->n, n);
	meson_parm_write(clk->map, &pll->m, m);
	meson_parm_write(clk->map, &pll->od, od);
	if (MESON_PARM_APPLICABLE(&pll->frac))
		meson_parm_write(clk->map, &pll->frac, frac);

	/* If the pll is stopped, bail out now */
	if (!enabled)
		return 0;

	ret = meson_clk_pll_v4_enable(hw);
	if (ret) {
		pr_warn("%s: pll did not lock, trying to restore old rate %lu\n",
			__func__, old_rate);
		/*
		 * FIXME: Do we really need/want this HACK ?
		 * It looks unsafe. what happens if the clock gets into a
		 * broken state and we can't lock back on the old_rate ? Looks
		 * like an infinite recursion is possible
		 */
		meson_clk_pll_v4_set_rate(hw, old_rate, prate);
	}

	return ret;
}

const struct clk_ops meson_clk_pll_v4_ops = {
	/* walk the init regs each time when set a new rate,
	 * init callback is not useful for v3 ops
	 */
	.init		= meson_clk_pll_v4_init,
	.recalc_rate	= meson_clk_pll_v4_recalc_rate,
	.determine_rate	= meson_clk_pll_v4_determine_rate,
	.set_rate	= meson_clk_pll_v4_set_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
	.enable		= meson_clk_pll_v4_enable,
	.disable	= meson_clk_pll_v4_disable
};
EXPORT_SYMBOL_GPL(meson_clk_pll_v4_ops);
#endif

const struct clk_ops meson_clk_pll_ro_ops = {
	.recalc_rate	= meson_clk_pll_recalc_rate,
	.is_enabled	= meson_clk_pll_is_enabled,
};
EXPORT_SYMBOL_GPL(meson_clk_pll_ro_ops);

MODULE_DESCRIPTION("Amlogic PLL driver");
MODULE_AUTHOR("Carlo Caione <carlo@endlessm.com>");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("GPL v2");
