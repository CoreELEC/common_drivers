// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "of-clk-common.h"
#include "clk-pll.h"

#define DEFAULT_AML_PLL_FRAC_MAX		(131071)  /* 2^17 - 1 */

#define AML_PLL_PARM_M_MAX			(511)  /* 2^9 - 1 */

#define AML_PLL_REG0_OFFSET			(0)
#define AML_PLL_REG1_OFFSET			(4)

struct aml_pll_reg_parms {
	union {
		struct {
			u32 m		:9;  /* bit0 - bit8 */
			u32 reserved	:3;  /* bit9 - bit11 */
			u32 n		:3;  /* bit12 - bit14 */
			u32 reserved1	:5;  /* bit15 - bit19 */
			u32 od		:3;  /* bit20 - bit22 */
			u32 reserved2	:3;  /* bit23 - bit25 */
			u32 force_lock	:1;  /* bit26 */
			u32 div0p5		:1;  /* bit27 */
			u32 en		:1;  /* bit28 */
			u32 rstn		:1;  /* bit29 */
			u32 l_detect_en	:1;  /* bit30 */
			u32 lock		:1;  /* bit31 */
		} bits;
		u32 val;
	} reg0;
	union {
		struct {
			u32 frac		:17;  /* bit0 - bit16 */
			u32 reserved	:15;  /* bit17 - bit31 */
		} bits;
		u32 val;
	} reg1;
};

static int __aml_pll_round_closest_mult(struct aml_pll_data *pll)
{
	if ((pll->flags & AML_PLL_ROUND_CLOSEST) &&
	    (pll->parm_flags & AML_PLL_SPEC_PARM_FRAC))
		return 1;

	return 0;
}

static unsigned long __aml_pll_params_to_rate(unsigned long parent_rate,
					      unsigned int m, unsigned int n,
					      unsigned int frac, unsigned int od,
					      struct aml_pll_data *pll)
{
	u64 rate = (u64)parent_rate * m;
	u64 frac_rate;
	u32 frac_max = pll->frac_max ? pll->frac_max : DEFAULT_AML_PLL_FRAC_MAX;

	if (pll->flags & AML_PLL_M_EN0P5)
		rate = rate >> 1;

	if (frac && (pll->parm_flags & AML_PLL_SPEC_PARM_FRAC)) {
		frac_rate = DIV_ROUND_UP_ULL((u64)parent_rate * frac, frac_max);
		if (pll->flags & AML_PLL_M_EN0P5)
			frac_rate = frac_rate >> 1;

		rate += frac_rate;
	}

	/* The 'n' divider has fixed power-of-two property */
	if (n && (pll->parm_flags & AML_PLL_SPEC_PARM_N))
		rate = rate >> n;

	if (od && (pll->parm_flags & AML_PLL_SPEC_PARM_OD))
		rate = rate >> od;

	/*
	 * FIXME: CCF uses 'unsigned long' for rate values, which may overflow
	 * on 32-bit systems.
	 */
	return (unsigned long)rate;
}

static unsigned long aml_pll_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	struct aml_pll_reg_parms regs;

	regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);
	regmap_read(clk->map, AML_PLL_REG1_OFFSET, &regs.reg1.val);

	return __aml_pll_params_to_rate(parent_rate, regs.reg0.bits.m,
				    regs.reg0.bits.n, regs.reg1.bits.frac,
				    regs.reg0.bits.od, pll);
}

static unsigned int __aml_pll_params_with_frac(unsigned long rate,
					       unsigned long parent_rate,
					       unsigned int m,
					       unsigned int n,
					       unsigned int od,
					       struct aml_pll_data *pll)
{
	u32 frac_max = pll->frac_max ? pll->frac_max : DEFAULT_AML_PLL_FRAC_MAX;
	u64 val;
	u64 dco_rate = rate;

	if (pll->parm_flags & AML_PLL_SPEC_PARM_OD)
		dco_rate = dco_rate << od;

	if (pll->flags & AML_PLL_M_EN0P5)
		parent_rate = parent_rate >> 1;

	if (pll->parm_flags & AML_PLL_SPEC_PARM_N) {
		dco_rate = dco_rate >> n;
		/* Bail out if we are already over the requested rate */
		if (dco_rate < ((parent_rate >> n) * m))
			return 0;
	}

	if (pll->flags & AML_PLL_ROUND_CLOSEST)
		val = DIV_ROUND_CLOSEST_ULL(dco_rate * frac_max, parent_rate);
	else
		val = div_u64(dco_rate * frac_max, parent_rate);

	val -= m * frac_max;

	return min((unsigned int)val, (frac_max - 1));
}

static bool aml_pll_is_better(unsigned long rate, unsigned long best,
			      unsigned long now, struct aml_pll_data *pll)
{
	if (__aml_pll_round_closest_mult(pll)) {
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

static int aml_get_pll_table(unsigned long rate, unsigned long parent_rate,
			     struct aml_pll_parms_table *parm,
			     struct aml_pll_data *pll)
{
	unsigned int idx, best_idx;
	unsigned long now, best = 0;

	for (idx = 0; idx < pll->table_count; idx++) {
		now = __aml_pll_params_to_rate(parent_rate, pll->table[idx].m,
					       pll->table[idx].n, 0,
					       pll->table[idx].od, pll);
		if (aml_pll_is_better(rate, best, now, pll)) {
			best = now;
			best_idx = idx;

			if (now == rate)
				break;
		}
	}

	if (idx >= pll->table_count)
		return -EINVAL;

	parm->m = pll->table[idx].m;
	parm->n = pll->table[idx].n;
	parm->od = pll->table[idx].od;

	return 0;
}

static int aml_get_pll_range(unsigned long rate, unsigned long parent_rate,
			     struct aml_pll_parms_table *parm,
			     struct aml_pll_data *pll)
{
	unsigned int idx, t_m;
	u64 vco_rate = rate;

	if (pll->flags & AML_PLL_M_EN0P5)
		parent_rate = parent_rate >> 1;

	if (pll->parm_flags & AML_PLL_SPEC_PARM_OD) {
		for (idx = 0; idx <= pll->od_max; idx++) {
			vco_rate = (u64)rate << idx;
			if (vco_rate > pll->range.min &&
			    vco_rate < pll->range.max)
				break;

			if (vco_rate < pll->range.min)
				continue;

			if (vco_rate > pll->range.max)
				return -EINVAL;
		}
	}
	if (vco_rate < pll->range.min || vco_rate > pll->range.max)
		return -EINVAL;

	if (__aml_pll_round_closest_mult(pll))
		t_m = DIV_ROUND_CLOSEST_ULL(vco_rate, parent_rate);
	else
		t_m = div_u64(vco_rate,  parent_rate);

	/* the multiplier too big - stop */
	if (t_m > AML_PLL_PARM_M_MAX)
		return -EINVAL;

	parm->m = t_m;
	/*
	 * NOTE: Configuring the 'n' divider affects PLL loop bandwidth and
	 * phase margin, making its use strongly discouraged.
	 *
	 * If absolutely required, rigorously assess the risks and only
	 * implement via 'pll->table' configuration.
	 */
	parm->n = 0;
	parm->od = idx;

	return 0;
}

static int aml_get_pll_settings(unsigned long rate,
				unsigned long parent_rate,
				struct aml_pll_parms_table *parm,
				struct aml_pll_data *pll)
{
	unsigned long range_rate = 0, table_rate = 0;
	struct aml_pll_parms_table range_parm, table_parm;
	int ret;

	ret = aml_get_pll_range(rate, parent_rate, &range_parm, pll);
	if (!ret)
		range_rate = __aml_pll_params_to_rate(parent_rate, range_parm.m,
						      range_parm.n, 0,
						      range_parm.od, pll);

	ret = aml_get_pll_table(rate, parent_rate, &table_parm, pll);
	if (!ret)
		table_rate = __aml_pll_params_to_rate(parent_rate, table_parm.m,
						      table_parm.n, 0,
						      table_parm.od, pll);

	if (!range_rate && !table_rate)
		return -EINVAL;

	if (aml_pll_is_better(rate, range_rate, table_rate, pll)) {
		parm->m = table_parm.m;
		parm->n = table_parm.n;
		parm->od = table_parm.od;
	} else {
		parm->m = range_parm.m;
		parm->n = range_parm.n;
		parm->od = range_parm.od;
	}

	return 0;
}

static int aml_pll_determine_rate(struct clk_hw *hw,
				  struct clk_rate_request *req)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	unsigned int frac = 0;
	struct aml_pll_parms_table parm;
	int ret;

	if (pll->flags & AML_PLL_READ_ONLY) {
		req->rate = clk_hw_get_rate(hw);
		return 0;
	}

	ret = aml_get_pll_settings(req->rate, req->best_parent_rate, &parm,
				   pll);
	if (ret)
		return ret;

	if (pll->parm_flags & AML_PLL_SPEC_PARM_FRAC)
		frac = __aml_pll_params_with_frac(req->rate,
						  req->best_parent_rate,
						  parm.m, parm.n, parm.od, pll);

	req->rate = __aml_pll_params_to_rate(req->best_parent_rate, parm.m,
					     parm.n, frac, parm.od, pll);

	return 0;
}

static int aml_pll_wait_lock(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	int delay = 1000;
	struct aml_pll_reg_parms regs;

	do {
		regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);
		/* Is the clock locked now ? */
		if (regs.reg0.bits.lock)
			return 0;

		udelay(1);
	} while (delay--);

	return -ETIMEDOUT;
}

static int aml_pll_is_enabled(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_reg_parms regs;

	regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);
	/* Enable and lock bit equal 1, it locks */
	if (regs.reg0.bits.en && regs.reg0.bits.lock)
		return 1;

	return 0;
}

static int aml_pll_init(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;

	/*
	 * Keep the clock running, which was already initialized and enabled
	 * from the bootloader stage, to avoid any glitches.
	 *
	 * Initialization is forced if "AML_PLL_FORCE_INIT" is configured.
	 */
	if (aml_pll_is_enabled(hw) && !(pll->flags & AML_PLL_FORCE_INIT))
		return 0;

	if (pll->init_count)
		regmap_multi_reg_write(clk->map, pll->init_regs, pll->init_count);

	return 0;
}

static int aml_pcie_pll_enable(struct clk_hw *hw)
{
	int retries = 10;

	do {
		aml_pll_init(hw);
		if (!aml_pll_wait_lock(hw))
			return 0;

		pr_info("Retry enabling PCIe PLL clock\n");
	} while (--retries);

	return -EIO;
}

static void aml_pll_disable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	struct aml_pll_reg_parms regs;

	regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);

	/* Put the pll is in reset */
	regs.reg0.bits.rstn = 0;
	regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);

	if (pll->parm_flags & AML_PLL_SPEC_PARM_LOCK_DETECT) {
		regs.reg0.bits.l_detect_en = 0;
		regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);
	}

	/* Disable the pll */
	regs.reg0.bits.en = 0;
	regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);
}

#define PLL_LOCK_RETRY_MAX		10

static int aml_pll_enable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	struct aml_pll_reg_parms regs;
	int retry = 0;

	/* do nothing if the PLL is already enabled */
	if (clk_hw_is_enabled(hw))
		return 0;

	do {
		/* Make sure the pll is disabled */
		aml_pll_disable(hw);

		regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);

		/* Enable the pll */
		regs.reg0.bits.en = 1;
		regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);

		udelay(20);

		/* Take the pll out reset */
		regs.reg0.bits.rstn = 1;
		regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);

		/* Take the pll out lock reset */
		if (pll->parm_flags & AML_PLL_SPEC_PARM_LOCK_DETECT) {
			regs.reg0.bits.l_detect_en = 1;
			regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);
		}

		if (!aml_pll_wait_lock(hw))
			return 0;
	} while (retry > PLL_LOCK_RETRY_MAX);

	pr_warn("%s: PLL lock failed!!!\n", clk_hw_get_name(hw));

	return -EIO;
}

static int aml_pll_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	struct aml_pll_reg_parms regs;
	struct aml_pll_parms_table parm;
	int ret, enabled;

	if (parent_rate == 0 || rate == 0)
		return -EINVAL;

	ret = aml_get_pll_settings(rate, parent_rate, &parm, pll);
	if (ret)
		return ret;

	enabled = aml_pll_is_enabled(hw);

	regmap_read(clk->map, AML_PLL_REG0_OFFSET, &regs.reg0.val);
	/* Don't disable pll if it's just changing frac */
	if ((regs.reg0.bits.m != parm.m || regs.reg0.bits.n != parm.n) && enabled)
		aml_pll_disable(hw);

	regs.reg0.bits.m = parm.m;
	regs.reg0.bits.n = parm.n;
	regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);

	if (pll->parm_flags & AML_PLL_SPEC_PARM_FRAC) {
		regmap_read(clk->map, AML_PLL_REG1_OFFSET, &regs.reg1.val);
		regs.reg1.bits.frac = __aml_pll_params_with_frac(rate, parent_rate,
								 parm.m, parm.n,
								 parm.od, pll);
		regmap_write(clk->map, AML_PLL_REG1_OFFSET, regs.reg1.val);
	}

	if (pll->parm_flags & AML_PLL_SPEC_PARM_OD) {
		regs.reg0.bits.od = parm.od;
		regmap_write(clk->map, AML_PLL_REG0_OFFSET, regs.reg0.val);
	}

	/* If the pll is stopped, bail out now */
	if (!enabled)
		return 0;

	return aml_pll_enable(hw);
}

static int aml_pll_save_context(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	unsigned long p_rate = clk_hw_get_rate(clk_hw_get_parent(hw));

	pll->context_is_enabled = aml_pll_is_enabled(hw);
	pll->context_rate = aml_pll_recalc_rate(hw, p_rate);

	return 0;
}

static void aml_pll_restore_context(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_pll_data *pll = (struct aml_pll_data *)clk->data;
	unsigned long p_rate = clk_hw_get_rate(clk_hw_get_parent(hw));

	if (!aml_pll_is_enabled(hw) && pll->init_count)
		regmap_multi_reg_write(clk->map, pll->init_regs, pll->init_count);

	aml_pll_set_rate(hw, pll->context_rate, p_rate);
	if (pll->context_is_enabled)
		aml_pll_enable(hw);
	else
		aml_pll_disable(hw);
}

const struct clk_ops aml_pll_ops = {
	.init		= aml_pll_init,
	.recalc_rate	= aml_pll_recalc_rate,
	.determine_rate	= aml_pll_determine_rate,
	.set_rate	= aml_pll_set_rate,
	.is_enabled	= aml_pll_is_enabled,
	.save_context	= aml_pll_save_context,
	.restore_context = aml_pll_restore_context,
	.enable		= aml_pll_enable,
	.disable	= aml_pll_disable
};
EXPORT_SYMBOL_GPL(aml_pll_ops);

const struct clk_ops aml_pll_ro_ops = {
	.recalc_rate	= aml_pll_recalc_rate,
	.is_enabled	= aml_pll_is_enabled,
};
EXPORT_SYMBOL_GPL(aml_pll_ro_ops);

const struct clk_ops aml_pcie_pll_ops = {
	.recalc_rate	= aml_pll_recalc_rate,
	.determine_rate	= aml_pll_determine_rate,
	.is_enabled	= aml_pll_is_enabled,
	.enable		= aml_pcie_pll_enable,
	.disable	= aml_pll_disable
};
EXPORT_SYMBOL_GPL(aml_pcie_pll_ops);

#ifdef CONFIG_OF
static struct aml_pll_data *of_get_aml_pll_data(struct device *dev)
{
	struct device_node *dev_np = dev_of_node(dev);
	struct aml_pll_data *pll;
	int ret;
	u32 u32_data;
	u32 d_flag, hw_flag;

	pll = devm_kzalloc(dev, sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	ret = of_property_count_elems_of_size(dev_np, "pll-init-regs",
					      sizeof(*pll->init_regs));
	if (ret > 0) {
		pll->init_count = ret;
		pll->init_regs = devm_kcalloc(dev, pll->init_count,
					      sizeof(*pll->init_regs),
					      GFP_KERNEL);
		if (!pll->init_regs) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = of_property_read_u32_array(dev_np,
						 "pll-init-regs",
						 &pll->init_regs->reg,
						 3 * pll->init_count);
		if (ret)
			goto fail;
	}

	ret = of_get_aml_clk_flag_index(dev_np, 0, &d_flag, &hw_flag);
	if (ret)
		goto fail;

	pll->flags = d_flag;

	of_property_read_u64_array(dev_np, "pll-dco-range", &pll->range.min, 2);
	ret = of_property_count_elems_of_size(dev_np, "pll-rate-tables",
					      sizeof(*pll->table));
	if (ret > 0) {
		pll->table_count = ret;
		pll->table = devm_kcalloc(dev, pll->table_count,
					  sizeof(*pll->table), GFP_KERNEL);
		if (!pll->table) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = of_property_read_u32_array(dev_np, "pll-rate-tables",
						 &pll->table->m,
						 3 * pll->table_count);
		if (ret)
			goto fail;
	}
	if (!pll->range.min && !pll->range.max && !pll->table)
		pll->flags |= AML_PLL_READ_ONLY;

	ret = of_property_read_u32(dev_np, "pll-special-parms", &u32_data);
	if (!ret) {
		pll->parm_flags = u32_data;
		if (pll->parm_flags & AML_PLL_SPEC_PARM_FRAC) {
			ret = of_property_read_u32(dev_np, "pll-max-frac",
						   &u32_data);
			if (!ret)
				pll->frac_max = u32_data;
		}

		if (pll->parm_flags & AML_PLL_SPEC_PARM_OD) {
			ret = of_property_read_u32(dev_np, "pll-max-od",
						   &u32_data);
			if (!ret)
				pll->od_max = u32_data;
		}
	}

	return pll;

fail:
	if (pll->init_regs)
		devm_kfree(dev, pll->init_regs);

	if (pll->table)
		devm_kfree(dev, pll->table);

	devm_kfree(dev, pll);

	return ERR_PTR(ret);
}

static int aml_pll_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_np = dev_of_node(dev);
	struct regmap *regmap;
	int ret;
	const char *clk_name;
	struct aml_clk *clk;
	struct clk_parent_data *parent_data;
	struct aml_pll_data *pll;
	struct clk_init_data init = { 0 };

	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk_name = of_get_aml_clk_name_index(dev_np, 0);
	if (!clk_name)
		return -EINVAL;

	parent_data = of_get_aml_clk_parent_data(dev, NULL, 0);
	if (IS_ERR(parent_data)) {
		if (PTR_ERR(parent_data) != -EPROBE_DEFER)
			dev_err(dev, "Get parent_data failed!\n");

		return PTR_ERR(parent_data);
	}

	init.name = clk_name;
	init.num_parents = of_get_aml_clk_parent_count(dev, 0);
	init.parent_data = parent_data;

	clk->hw.init = &init;
	regmap = aml_clk_regmap_init(pdev);
	if (IS_ERR_OR_NULL(regmap))
		return -EIO;

	clk->map = regmap;
	pll = of_get_aml_pll_data(dev);
	if (IS_ERR(pll))
		return -EINVAL;

	clk->data = pll;
	if (pll->flags & AML_PLL_READ_ONLY)
		init.ops = &aml_pll_ro_ops;
	else
		init.ops = &aml_pll_ops;

	ret = devm_clk_hw_register(dev, &clk->hw);
	if (ret)
		return ret;

	of_aml_clk_parent_data_release(dev, parent_data);

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get, &clk->hw);
}

static const struct of_device_id aml_pll_match_table[] = {
	{
		.compatible = "amlogic,pll",
	},
	{}
};

static struct platform_driver aml_pll_driver = {
	.probe		= aml_pll_probe,
	.driver		= {
		.name	= "pll",
		.of_match_table = aml_pll_match_table,
	},
};

#endif  /* CONFIG_OF */

#ifdef CONFIG_AMLOGIC_MODIFY
int __init clk_pll_init(void)
{
	return platform_driver_register(&aml_pll_driver);
}

void __exit clk_pll_exit(void)
{
}
#else
MODULE_DEVICE_TABLE(of, aml_pll_match_table);
module_platform_driver(aml_pll_driver);

MODULE_DESCRIPTION("Amlogic Common PLL driver");
MODULE_AUTHOR("Chuan Liu <chuan.liu@amlogic.com>");
MODULE_IMPORT_NS("CLK_AMLOGIC");
MODULE_LICENSE("GPL v2");
#endif  /* CONFIG_AMLOGIC_MODIFY */
