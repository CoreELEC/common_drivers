// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>

#include "clk-basic.h"
#include "of-clk-common.h"

/* Amlogic basic clock types */
#define AML_CLKTYPE_MUX			1
#define AML_CLKTYPE_DIV			2
#define AML_CLKTYPE_GATE		3

/*
 * It works on following logic:
 *
 * For enabling clock, enable = 1
 *	set2dis = 1	-> clear bit	-> set = 0
 *	set2dis = 0	-> set bit	-> set = 1
 *
 * For disabling clock, enable = 0
 *	set2dis = 1	-> set bit	-> set = 1
 *	set2dis = 0	-> clear bit	-> set = 0
 *
 * So, result is always: enable xor set2dis.
 */
static int aml_clk_gate_endisable(struct clk_hw *hw, int enable)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_gate_data *gate = (struct aml_clk_gate_data *)clk->data;
	int set = gate->flags & CLK_GATE_SET_TO_DISABLE ? 1 : 0;

	set ^= enable;

	return regmap_update_bits(clk->map, gate->reg_offset, BIT(gate->bit_idx),
				  set ? BIT(gate->bit_idx) : 0);
}

static int aml_clk_gate_enable(struct clk_hw *hw)
{
	return aml_clk_gate_endisable(hw, 1);
}

static void aml_clk_gate_disable(struct clk_hw *hw)
{
	aml_clk_gate_endisable(hw, 0);
}

static int aml_clk_gate_is_enabled(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_gate_data *gate = (struct aml_clk_gate_data *)clk->data;
	unsigned int val;

	regmap_read(clk->map, gate->reg_offset, &val);
	if (gate->flags & CLK_GATE_SET_TO_DISABLE)
		val ^= BIT(gate->bit_idx);

	val &= BIT(gate->bit_idx);

	return val ? 1 : 0;
}

const struct clk_ops aml_clk_gate_ops = {
	.enable = aml_clk_gate_enable,
	.disable = aml_clk_gate_disable,
	.is_enabled = aml_clk_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(aml_clk_gate_ops);

const struct clk_ops aml_clk_gate_ro_ops = {
	.is_enabled = aml_clk_gate_is_enabled,
};
EXPORT_SYMBOL_GPL(aml_clk_gate_ro_ops);

static unsigned long aml_clk_div_recalc_rate(struct clk_hw *hw,
						unsigned long prate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_divider_data *div = (struct aml_clk_divider_data *)clk->data;
	unsigned int val;
	int ret;

	ret = regmap_read(clk->map, div->reg_offset, &val);
	if (ret)
		/* Gives a hint that something is wrong */
		return 0;

	val >>= div->shift;
	val &= clk_div_mask(div->width);

	return divider_recalc_rate(hw, prate, val, div->table, div->flags,
				   div->width);
}

static int aml_clk_div_determine_rate(struct clk_hw *hw,
					 struct clk_rate_request *req)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_divider_data *div = (struct aml_clk_divider_data *)clk->data;
	unsigned int val;
	int ret;

	/* if read only, just return current value */
	if (div->flags & CLK_DIVIDER_READ_ONLY) {
		ret = regmap_read(clk->map, div->reg_offset, &val);
		if (ret)
			return ret;

		val >>= div->shift;
		val &= clk_div_mask(div->width);

		return divider_ro_determine_rate(hw, req, div->table,
						 div->width, div->flags, val);
	}

	return divider_determine_rate(hw, req, div->table, div->width,
				      div->flags);
}

static int aml_clk_div_set_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_divider_data *div = (struct aml_clk_divider_data *)clk->data;
	unsigned int val;
	int ret;

	ret = divider_get_val(rate, parent_rate, div->table, div->width,
			      div->flags);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret << div->shift;

	return regmap_update_bits(clk->map, div->reg_offset,
				  clk_div_mask(div->width) << div->shift, val);
};

/* Would prefer aml_clk_div_ro_ops but clashes with qcom */

const struct clk_ops aml_clk_divider_ops = {
	.recalc_rate = aml_clk_div_recalc_rate,
	.determine_rate = aml_clk_div_determine_rate,
	.set_rate = aml_clk_div_set_rate,
};
EXPORT_SYMBOL_GPL(aml_clk_divider_ops);

const struct clk_ops aml_clk_divider_ro_ops = {
	.recalc_rate = aml_clk_div_recalc_rate,
	.determine_rate = aml_clk_div_determine_rate,
};
EXPORT_SYMBOL_GPL(aml_clk_divider_ro_ops);

static u8 aml_clk_mux_get_parent(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_mux_data *mux = (struct aml_clk_mux_data *)clk->data;
	unsigned int val;
	int ret;

	ret = regmap_read(clk->map, mux->reg_offset, &val);
	if (ret)
		return ret;

	val >>= mux->shift;
	val &= mux->mask;
	return clk_mux_val_to_index(hw, mux->table, mux->flags, val);
}

static int aml_clk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_mux_data *mux = (struct aml_clk_mux_data *)clk->data;
	unsigned int val = clk_mux_index_to_val(mux->table, mux->flags, index);

	return regmap_update_bits(clk->map, mux->reg_offset,
				  mux->mask << mux->shift,
				  val << mux->shift);
}

static int aml_clk_mux_determine_rate(struct clk_hw *hw,
				      struct clk_rate_request *req)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_mux_data *mux = (struct aml_clk_mux_data *)clk->data;

	return clk_mux_determine_rate_flags(hw, req, mux->flags);
}

const struct clk_ops aml_clk_mux_ops = {
	.get_parent = aml_clk_mux_get_parent,
	.set_parent = aml_clk_mux_set_parent,
	.determine_rate = aml_clk_mux_determine_rate,
};
EXPORT_SYMBOL_GPL(aml_clk_mux_ops);

const struct clk_ops aml_clk_mux_ro_ops = {
	.get_parent = aml_clk_mux_get_parent,
};
EXPORT_SYMBOL_GPL(aml_clk_mux_ro_ops);

#ifdef CONFIG_OF
static int of_get_aml_clk_basic_reg_index(struct device_node *np, u32 index,
					  u32 *offset, u32 *shift, u32 *width)
{
	const __be32 *prop;
	unsigned int psize;
	int elems_size = 3 * sizeof(u32);
	u32 u32_index = 3 * index;

	prop = of_get_property(np, "clock-reg-offsets", &psize);
	if (!prop || psize < (index + 1) * elems_size)
		return -EINVAL;

	*offset = be32_to_cpu(prop[u32_index]);
	*shift = be32_to_cpu(prop[u32_index + 1]);
	*width = be32_to_cpu(prop[u32_index + 2]);

	return 0;
}

/*
 * The "clock-basic-types" property definition has two valid forms:
 * 1) Single definition: Applies to all clocks under the device.
 * 2) Per-clock definition: Must match clk_num count (one entry
 * per clock).
 */
static int of_get_aml_clk_basic_type_index(const struct device_node *np,
					   u32 index)
{
	int ret;
	u32 clktype;

	ret = of_property_count_u32_elems(np, "clock-basic-types");
	if (ret == 1)
		ret = of_property_read_u32_index(np, "clock-basic-types", 0,
						 &clktype);
	else
		ret = of_property_read_u32_index(np, "clock-basic-types", index,
						 &clktype);
	if (ret)
		return ret;

	return clktype;
}

static struct aml_clk_mux_data *
of_get_aml_clk_mux_data_index(struct device *dev, u32 data_flags, u32 index)
{
	struct device_node *dev_np = dev_of_node(dev);
	u32 offset, shift, width;
	struct aml_clk_mux_data *mux;
	int ret;

	ret = of_get_aml_clk_basic_reg_index(dev_np, index,
					     &offset, &shift, &width);
	if (ret)
		return ERR_PTR(ret);

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->reg_offset = offset;
	mux->shift = shift;
	mux->mask = GENMASK(width - 1, 0);
	mux->table = of_get_aml_clk_parent_table(dev, index);
	mux->flags = data_flags;

	return mux;
}

static struct aml_clk_divider_data *
of_get_aml_clk_div_data_index(struct device *dev, u32 data_flags, u32 index)
{
	struct device_node *dev_np = dev_of_node(dev);
	u32 offset, shift, width;
	struct aml_clk_divider_data *div;
	int ret;

	ret = of_get_aml_clk_basic_reg_index(dev_np, index,
					     &offset, &shift, &width);
	if (ret)
		return ERR_PTR(ret);

	div = devm_kzalloc(dev, sizeof(*div), GFP_KERNEL);
	if (!div)
		return ERR_PTR(-ENOMEM);

	div->reg_offset = offset;
	div->shift = shift;
	div->width = width;
	div->flags = data_flags;

	return div;
}

static struct aml_clk_gate_data *
of_get_aml_clk_gate_data_index(struct device *dev, u32 data_flags, u32 index)
{
	struct device_node *dev_np = dev_of_node(dev);
	u32 offset, shift, width;
	struct aml_clk_gate_data *gate;
	int ret;

	ret = of_get_aml_clk_basic_reg_index(dev_np, index,
					     &offset, &shift, &width);
	if (ret)
		return ERR_PTR(ret);

	gate = devm_kzalloc(dev, sizeof(*gate), GFP_KERNEL);
	if (!gate)
		return ERR_PTR(-ENOMEM);

	/* reg_width is useless for gate */
	gate->reg_offset = offset;
	gate->bit_idx = shift;
	gate->flags = data_flags;

	return gate;
}

static int aml_clk_basic_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_np = dev_of_node(dev);
	struct regmap *regmap;
	int clkid, ret, i, clktype;
	u32 d_flag, hw_flag;
	int clk_num;
	const char *clk_name;
	struct clk_hw_onecell_data *clk_data;
	struct aml_clk *clk;
	struct clk_parent_data *parent_data;
	struct aml_clk_mux_data *mux;
	struct aml_clk_divider_data *div;
	struct aml_clk_gate_data *gate;
	struct clk_init_data init = { 0 };

	clk_num = of_get_aml_clk_count(dev_np);
	if (clk_num == 0)
		return -EINVAL;

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws, clk_num),
				GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->num = clk_num;
	clk = devm_kcalloc(dev, clk_num, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	for (i = 0; i < clk_num; i++)
		clk_data->hws[i] = &clk[i].hw;

	regmap = aml_clk_regmap_init(pdev);
	if (IS_ERR_OR_NULL(regmap))
		return -EIO;

	for (i = 0, clkid = 0; i < clk_num; i++) {
		clkid = of_get_aml_clk_clkid_index(dev_np, i);
		if (clkid < 0 && clk_num > 1)
			return -EINVAL;

		clk_name = of_get_aml_clk_name_index(dev_np, i);
		if (!clk_name)
			return -EINVAL;

		init.name = clk_name;
		init.num_parents = of_get_aml_clk_parent_count(dev, i);
		parent_data = of_get_aml_clk_parent_data(dev, clk_data->hws, i);
		if (IS_ERR(parent_data)) {
			if (PTR_ERR(parent_data) != -EPROBE_DEFER)
				dev_err(dev, "Get parent_data failed!\n");

			return PTR_ERR(parent_data);
		}
		init.parent_data = parent_data;
		ret = of_get_aml_clk_flag_index(dev_np, i, &d_flag, &hw_flag);
		if (ret)
			return ret;

		init.flags = hw_flag;

		clk[clkid].hw.init = &init;
		clk[clkid].map = regmap;

		clktype = of_get_aml_clk_basic_type_index(dev_np, i);
		if (clktype == AML_CLKTYPE_MUX) {
			mux = of_get_aml_clk_mux_data_index(dev, d_flag, i);
			if (IS_ERR(mux))
				return PTR_ERR(mux);

			clk[clkid].data = mux;
			if (mux->flags & CLK_MUX_READ_ONLY)
				init.ops = &aml_clk_mux_ro_ops;
			else
				init.ops = &aml_clk_mux_ops;
		} else if (clktype == AML_CLKTYPE_DIV) {
			div = of_get_aml_clk_div_data_index(dev, d_flag, i);
			if (IS_ERR(div))
				return PTR_ERR(div);

			clk[clkid].data = div;
			if (div->flags & CLK_DIVIDER_READ_ONLY)
				init.ops = &aml_clk_divider_ro_ops;
			else
				init.ops = &aml_clk_divider_ops;
		} else if (clktype == AML_CLKTYPE_GATE) {
			gate = of_get_aml_clk_gate_data_index(dev, d_flag, i);
			if (IS_ERR(gate))
				return PTR_ERR(gate);

			clk[clkid].data = gate;
			init.ops = &aml_clk_gate_ops;
		}

		ret = devm_clk_hw_register(dev, &clk[clkid].hw);
		if (ret)
			return ret;

		of_aml_clk_parent_data_release(dev, parent_data);
	}

	if (clk_num == 1)
		return devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get,
						   &clk->hw);
	else
		return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
						   clk_data);
}

static const struct of_device_id aml_clk_basic_match_table[] = {
	{
		.compatible = "amlogic,basic-clk",
	},
	{}
};

static struct platform_driver aml_clk_basic_driver = {
	.probe		= aml_clk_basic_probe,
	.driver		= {
		.name	= "basic-clk",
		.of_match_table = aml_clk_basic_match_table,
	},
};

#endif  /* CONFIG_OF */

#ifdef CONFIG_AMLOGIC_MODIFY
int __init clk_basic_init(void)
{
	return platform_driver_register(&aml_clk_basic_driver);
}

void __exit clk_basic_exit(void)
{
}
#else
MODULE_DEVICE_TABLE(of, aml_clk_basic_match_table);
module_platform_driver(aml_clk_basic_driver);

MODULE_DESCRIPTION("Amlogic Basic Clock Driver");
MODULE_AUTHOR("Chuan Liu <chuan.liu@amlogic.com>");
MODULE_IMPORT_NS("CLK_AMLOGIC");
MODULE_LICENSE("GPL v2");
#endif
