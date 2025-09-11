// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-basic.h"
#include "of-clk-common.h"
#include "clk-dualdiv-mux.h"

#define AML_DUALDIV_REG0_OFFSET			(0)
#define AML_DUALDIV_REG1_OFFSET			(4)

struct aml_dualdiv_mux_reg_parms {
	union {
		struct {
			u32 n0		:12;  /* bit0 - bit11 */
			u32 n1		:12;  /* bit12 - bit23 */
			u32 reserved0	:4;  /* bit24 - bit27 */
			u32 div_mode	:2;  /* bit28 - bit29 */
			u32 clkout_gate	:1;  /* bit30 */
			u32 clkin_gate	:1;  /* bit31 */
		} bits;
		u32 val;
	} reg0;
	union {
		struct {
			u32 m0		:12;  /* bit0 - bit11 */
			u32 m1		:12;  /* bit12 - bit23 */
			u32 force_clkout	:1;  /* bit24 */
			u32 reserved0	:5;  /* bit25 - bit29 */
			u32 out_mux	:2;  /* bit30 - bit31 */

		} bits;
		u32 val;
	} reg1;
};

static inline unsigned int _get_div_mult(unsigned int val)
{
	return val + 1;
}

static unsigned long
aml_dualdiv_param_to_rate(unsigned long parent_rate,
			  const struct aml_clk_dualdiv_param *p)
{
	unsigned int n0, n1, m0, m1;

	n0 = _get_div_mult(p->n0);
	if (!p->div_mode)
		return DIV_ROUND_CLOSEST(parent_rate, n0);

	n1 = _get_div_mult(p->n1);
	m0 = _get_div_mult(p->m0);
	m1 = _get_div_mult(p->m1);

	return DIV_ROUND_CLOSEST(parent_rate * (m0 + m1), n0 * m0 + n1 * m1);
}

static unsigned long aml_clk_dualdiv_recalc_rate(struct clk_hw *hw,
						   unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;
	struct aml_clk_dualdiv_param parm;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG1_OFFSET,
		    &reg.reg1.val);
	if (reg.reg1.bits.force_clkout == 1)
		return parent_rate;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		    &reg.reg0.val);
	parm.div_mode = reg.reg0.bits.div_mode;
	parm.n0 = reg.reg0.bits.n0;
	parm.n1 = reg.reg0.bits.n1;
	parm.m0 = reg.reg1.bits.m0;
	parm.m1 = reg.reg1.bits.m1;

	return aml_dualdiv_param_to_rate(parent_rate, &parm);
}

static const struct aml_clk_dualdiv_param *
aml_clk_dualdiv_get_setting(unsigned long rate, unsigned long parent_rate,
			    struct aml_clk_dualdiv_data *dualdiv)
{
	const struct aml_clk_dualdiv_param *table = dualdiv->table;
	unsigned long best = 0, now = 0;
	unsigned int i, best_i = 0;

	if (!table)
		return NULL;

	for (i = 0; i < dualdiv->table_count; i++) {
		now = aml_dualdiv_param_to_rate(parent_rate, &table[i]);

		/* If we get an exact match, don't bother any further */
		if (now == rate) {
			return &table[i];
		} else if (abs(now - rate) < abs(best - rate)) {
			best = now;
			best_i = i;
		}
	}

	return (struct aml_clk_dualdiv_param *)&table[best_i];
}

static int aml_clk_dualdiv_determine_rate(struct clk_hw *hw,
					  struct clk_rate_request *req)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	const struct aml_clk_dualdiv_param *setting;

	/* Just need to set "force_clkout" = 1 */
	if (req->rate == req->best_parent_rate) {
		req->rate = req->best_parent_rate;

		return 0;
	}

	setting = aml_clk_dualdiv_get_setting(req->rate, req->best_parent_rate,
					      dualdiv);
	if (setting)
		req->rate = aml_dualdiv_param_to_rate(req->best_parent_rate,
						      setting);
	else
		req->rate = aml_clk_dualdiv_recalc_rate(hw,
							req->best_parent_rate);

	return 0;
}

static int aml_clk_dualdiv_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;
	const struct aml_clk_dualdiv_param *setting;

	/* Just need to set "force_clkout" = 1 */
	if (rate == parent_rate) {
		regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG1_OFFSET,
			    &reg.reg1.val);
		reg.reg1.bits.force_clkout = 1;
		regmap_write(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG1_OFFSET,
				     reg.reg1.val);

		return 0;
	}

	setting = aml_clk_dualdiv_get_setting(rate, parent_rate, dualdiv);
	if (!setting)
		return -EINVAL;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		    &reg.reg0.val);
	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG1_OFFSET,
		    &reg.reg1.val);
	reg.reg1.bits.force_clkout = 0;

	reg.reg0.bits.div_mode = setting->div_mode;
	reg.reg0.bits.n0 = setting->n0;
	reg.reg0.bits.n1 = setting->n1;
	reg.reg1.bits.m0 = setting->m0;
	reg.reg1.bits.m1 = setting->m1;

	regmap_write(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		     reg.reg0.val);
	regmap_write(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG1_OFFSET,
		     reg.reg1.val);

	return 0;
}

static int aml_clk_dualdiv_enable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		    &reg.reg0.val);
	reg.reg0.bits.clkin_gate = 1;
	reg.reg0.bits.clkout_gate = 1;

	regmap_write(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		     reg.reg0.val);

	return 0;
}

static void aml_clk_dualdiv_disable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		    &reg.reg0.val);
	reg.reg0.bits.clkin_gate = 0;
	reg.reg0.bits.clkout_gate = 0;

	regmap_write(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		     reg.reg0.val);
}

static int aml_clk_dualdiv_is_enabled(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;

	regmap_read(clk->map, dualdiv->reg_offset + AML_DUALDIV_REG0_OFFSET,
		    &reg.reg0.val);

	if (reg.reg0.bits.clkin_gate && reg.reg0.bits.clkout_gate)
		return 1;
	else
		return 0;
}

#ifdef CONFIG_DEBUG_FS
static int aml_clk_dualdiv_available_rates_show(struct seq_file *s, void *data)
{
	struct clk_hw *hw = s->private;
	struct clk_hw *phw = clk_hw_get_parent(hw);
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_data *dualdiv = clk->data;
	unsigned long rate, prate;
	unsigned long core_min_rate, core_max_rate;
	int i;

	if (!phw) {
		pr_err("%s: can't get parent\n", clk_hw_get_name(hw));

		return -ENOENT;
	}

	prate = clk_hw_get_rate(phw);
	clk_hw_get_rate_range(hw, &core_min_rate, &core_max_rate);
	if (dualdiv->table) {
		for (i = 0; i < dualdiv->table_count; i++) {
			rate = aml_dualdiv_param_to_rate(prate,
							 &dualdiv->table[i]);
			if (rate < core_min_rate || rate > core_max_rate)
				continue;

			seq_printf(s, "%ld\n", (unsigned long)rate);
		}
	} else {
		seq_printf(s, "%ld\n", prate);
	}

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(aml_clk_dualdiv_available_rates);

static void aml_clk_dualdiv_mux_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct aml_clk *clk = to_aml_clk(hw);

	debugfs_create_file("clk_type", 0444, dentry, hw, &aml_clk_type_fops);
	if (clk->type == AML_CLK_DUALDIV)
		debugfs_create_file("clk_available_rates", 0444, dentry, hw,
				    &aml_clk_dualdiv_available_rates_fops);
}
#endif

const struct clk_ops aml_clk_dualdiv_ops = {
	.enable		= aml_clk_dualdiv_enable,
	.disable	= aml_clk_dualdiv_disable,
	.is_enabled	= aml_clk_dualdiv_is_enabled,
	.recalc_rate	= aml_clk_dualdiv_recalc_rate,
	.determine_rate	= aml_clk_dualdiv_determine_rate,
	.set_rate	= aml_clk_dualdiv_set_rate,
#ifdef CONFIG_DEBUG_FS
	.debug_init = aml_clk_dualdiv_mux_debug_init,
#endif
};
EXPORT_SYMBOL_GPL(aml_clk_dualdiv_ops);

const struct clk_ops aml_clk_dualdiv_ro_ops = {
	.is_enabled	= aml_clk_dualdiv_is_enabled,
	.recalc_rate	= aml_clk_dualdiv_recalc_rate,
#ifdef CONFIG_DEBUG_FS
	.debug_init = aml_clk_dualdiv_mux_debug_init,
#endif
};
EXPORT_SYMBOL_GPL(aml_clk_dualdiv_ro_ops);

static u8 aml_clk_dualdiv_mux_get_parent(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_mux_data *mux = clk->data;
	struct aml_dualdiv_mux_reg_parms reg;

	regmap_read(clk->map, mux->reg_offset + AML_DUALDIV_REG1_OFFSET,
		    &reg.reg1.val);

	return clk_mux_val_to_index(hw, mux->table, 0, reg.reg1.bits.out_mux);
}

static int aml_clk_dualdiv_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_dualdiv_mux_data *mux = clk->data;
	unsigned int val = clk_mux_index_to_val(mux->table, 0, index);
	struct aml_dualdiv_mux_reg_parms reg;

	regmap_read(clk->map, mux->reg_offset + AML_DUALDIV_REG1_OFFSET,
		    &reg.reg1.val);
	reg.reg1.bits.out_mux = val;

	regmap_write(clk->map, mux->reg_offset + AML_DUALDIV_REG1_OFFSET,
		     reg.reg1.val);

	return 0;
}

static int aml_clk_dualdiv_mux_determine_rate(struct clk_hw *hw,
					      struct clk_rate_request *req)
{
	return clk_mux_determine_rate_flags(hw, req, 0);
}

const struct clk_ops aml_clk_dualdiv_mux_ops = {
	.get_parent = aml_clk_dualdiv_mux_get_parent,
	.set_parent = aml_clk_dualdiv_mux_set_parent,
	.determine_rate = aml_clk_dualdiv_mux_determine_rate,
#ifdef CONFIG_DEBUG_FS
	.debug_init = aml_clk_dualdiv_mux_debug_init,
#endif
};
EXPORT_SYMBOL_GPL(aml_clk_dualdiv_mux_ops);

const struct clk_ops aml_clk_dualdiv_mux_ro_ops = {
	.get_parent = aml_clk_dualdiv_mux_get_parent,
#ifdef CONFIG_DEBUG_FS
	.debug_init = aml_clk_dualdiv_mux_debug_init,
#endif
};
EXPORT_SYMBOL_GPL(aml_clk_dualdiv_mux_ro_ops);

#ifdef CONFIG_OF
#define AML_DUALDIV_MUX_CLK_NUB			2

enum aml_dualdiv_mux_index {
	DUALDIV		= 0,
	MUX		= 1
};

static struct aml_clk_dualdiv_data *
of_aml_clk_get_dualdiv_data(struct device *dev)
{
	struct device_node *dev_np = dev_of_node(dev);
	struct aml_clk_dualdiv_data *dualdiv;
	int ret;

	dualdiv = devm_kzalloc(dev, sizeof(*dualdiv), GFP_KERNEL);
	if (!dualdiv)
		return ERR_PTR(-ENOMEM);

	ret = of_property_count_elems_of_size(dev_np, "clock-dualdiv-tables",
					      sizeof(*dualdiv->table));
	if (ret > 0) {
		dualdiv->table_count = ret;
		dualdiv->table = devm_kcalloc(dev, dualdiv->table_count,
					      sizeof(*dualdiv->table),
					      GFP_KERNEL);
		if (!dualdiv->table) {
			ret = -ENOMEM;
			goto free_duandiv;
		}

		ret = of_property_read_u32_array(dev_np, "clock-dualdiv-tables",
						 &dualdiv->table->div_mode,
						 5 * dualdiv->table_count);
		if (ret)
			goto free_dualdiv_tab;
	}

	return dualdiv;

free_dualdiv_tab:
	devm_kfree(dev, dualdiv->table);

free_duandiv:
	devm_kfree(dev, dualdiv);

	return ERR_PTR(ret);
}

static struct aml_clk_dualdiv_mux_data *
of_aml_clk_get_dualdiv_mux_data(struct device *dev)
{
	struct aml_clk_dualdiv_mux_data *mux;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->table = of_aml_clk_get_parent_table(dev, MUX);

	return mux;
}

static int aml_clk_dualdiv_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_np = dev_of_node(dev);
	struct regmap *regmap;
	int ret, i;
	struct clk_hw_onecell_data *clk_data;
	struct aml_clk *clk;
	struct clk_parent_data *parent_data;
	struct aml_clk_dualdiv_data *dualdiv;
	struct aml_clk_dualdiv_mux_data *mux;
	u32 clkid;
	const char *clk_name;
	struct clk_init_data init = { 0 };
	u64 min_rate, max_rate;

	dualdiv = of_aml_clk_get_dualdiv_data(dev);
	if (IS_ERR(dualdiv))
		return -EINVAL;

	mux = of_aml_clk_get_dualdiv_mux_data(dev);
	if (IS_ERR(mux))
		return -EINVAL;

	regmap = aml_clk_regmap_init(pdev);
	if (IS_ERR_OR_NULL(regmap))
		return -EIO;

	ret = of_aml_clk_get_count(dev_np);
	if (ret != AML_DUALDIV_MUX_CLK_NUB)
		return -EINVAL;

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws,
				AML_DUALDIV_MUX_CLK_NUB), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk = devm_kcalloc(dev, AML_DUALDIV_MUX_CLK_NUB, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	clk_data->num = AML_DUALDIV_MUX_CLK_NUB;
	for (i = 0; i < AML_DUALDIV_MUX_CLK_NUB; i++)
		clk_data->hws[i] = &clk[i].hw;

	for (i = 0; i < AML_DUALDIV_MUX_CLK_NUB; i++) {
		clk_name = of_aml_clk_get_name_index(dev_np, i);
		if (!clk_name)
			return -EINVAL;

		clkid = of_aml_clk_get_clkid_index(dev_np, i);
		if (clkid < 0)
			return -EINVAL;

		parent_data = of_aml_clk_get_parent_data(dev, clk_data->hws, i);
		if (IS_ERR(parent_data)) {
			if (PTR_ERR(parent_data) != -EPROBE_DEFER)
				dev_err(dev, "Get parent_data failed!\n");

			return PTR_ERR(parent_data);
		}

		init.name = clk_name;
		init.num_parents = of_aml_clk_get_parent_count(dev, i);
		init.parent_data = parent_data;

		clk[clkid].hw.init = &init;
		clk[clkid].map = regmap;
		if (i == DUALDIV) {
			init.ops = &aml_clk_dualdiv_ops;
			init.flags = 0;
			clk[clkid].data = dualdiv;
			clk[clkid].type = AML_CLK_DUALDIV;
		} else {
			init.ops = &aml_clk_dualdiv_mux_ops;
			/*
			 * NOTE: The rtc_clk mux needs to ensure that both the
			 * old and new parent clocks have clock inputs in order
			 * to switch stably, similar to a glitch-free mux.
			 */
			init.flags = CLK_OPS_PARENT_ENABLE;
			clk[clkid].data = mux;
			clk[clkid].type = AML_CLK_MUX;
		}

		ret = devm_clk_hw_register(dev, &clk[clkid].hw);
		if (ret)
			return ret;

		of_aml_clk_parent_data_release(dev, parent_data);

		ret = of_aml_clk_get_rate_range(dev_np, i, &min_rate, &max_rate);
		if (ret) {
			if (ret != -EEXIST)
				return ret;
		} else {
			if (min_rate || max_rate)
				clk_hw_set_rate_range(&clk[clkid].hw, min_rate,
						      max_rate);
		}
	}

	return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get, clk_data);
}

static const struct of_device_id aml_clk_dualdiv_mux_match_table[] = {
	{
		.compatible = "amlogic,dualdiv-mux-clk",
	},
	{}
};

static struct platform_driver aml_clk_dualdiv_mux_driver = {
	.probe		= aml_clk_dualdiv_mux_probe,
	.driver		= {
		.name	= "dualdiv-mux-clk",
		.of_match_table = aml_clk_dualdiv_mux_match_table,
	},
};

#endif  /* CONFIG_OF */

#ifdef CONFIG_AMLOGIC_MODIFY
int __init clk_dualdiv_mux_init(void)
{
	return platform_driver_register(&aml_clk_dualdiv_mux_driver);
}

void __exit clk_dualdiv_mux_exit(void)
{
}
#else
MODULE_DEVICE_TABLE(of, aml_clk_dualdiv_mux_match_table);
module_platform_driver(aml_clk_basic_driver);

MODULE_DESCRIPTION("Amlogic dual divider mux driver");
MODULE_AUTHOR("Chuan Liu <chuan.liu@amlogic.com>");
MODULE_IMPORT_NS("CLK_AMLOGIC");
MODULE_LICENSE("GPL v2");
#endif  /* CONFIG_AMLOGIC_MODIFY */
