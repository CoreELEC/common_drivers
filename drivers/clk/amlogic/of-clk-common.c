// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/err.h>

#include "of-clk-common.h"

struct regmap *aml_clk_regmap_init(struct platform_device *pdev)
{
	void __iomem *base;
	struct resource *res;
	struct regmap_config clkc_regmap_config = {
		.reg_bits	= 32,
		.val_bits	= 32,
		.reg_stride	= 4,
	};

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(base))
		return NULL;

	clkc_regmap_config.max_register = resource_size(res) - 4;
	if (!clkc_regmap_config.max_register)
		clkc_regmap_config.max_register_is_0 = true;

	return devm_regmap_init_mmio(&pdev->dev, base, &clkc_regmap_config);
}
EXPORT_SYMBOL_GPL(aml_clk_regmap_init);

#ifdef CONFIG_DEBUG_FS
static const struct {
	unsigned int type;
	const char *name;
} clk_types[] = {
#define ENTRY(f) { f, #f }
	ENTRY(AML_CLK_MUX),
	ENTRY(AML_CLK_DIV),
	ENTRY(AML_CLK_GATE),
	ENTRY(AML_CLK_DUALDIV),
	ENTRY(AML_CLK_GENERAL),
	ENTRY(AML_CLK_NOGLITCH),
	ENTRY(AML_CLK_PLL),
#undef ENTRY
};

static int aml_clk_types_show(struct seq_file *s, void *data)
{
	struct clk_hw *hw = s->private;
	struct aml_clk *clk = to_aml_clk(hw);
	int i;

	for (i = 0; i < ARRAY_SIZE(clk_types); i++) {
		if (clk_types[i].type == clk->type) {
			seq_printf(s, "%s\n", clk_types[i].name);
			return 0;
		}
	}

	seq_puts(s, "UNKNOWN\n");

	return -EINVAL;
}

static int aml_clk_types_open(struct inode *inode, struct file *file)
{
	return single_open(file, aml_clk_types_show, inode->i_private);
}

const struct file_operations aml_clk_type_fops = {
	.owner		= THIS_MODULE,
	.open		= aml_clk_types_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
EXPORT_SYMBOL_GPL(aml_clk_type_fops);
#endif

#ifdef CONFIG_OF
static int of_aml_clk_get_real_parent_index(struct device_node *np, int index)
{
	int count = of_count_phandle_with_args(np, "clock-parents",
					       "#clock-cells");

	if (count == 0)
		return -ENOENT;

	/*
	 * If only a single 'clock-parents' property is defined under the
	 * current device_node, this property is used to specify the common
	 * parent clock for all clocks under the current device.
	 */
	if (count == 1)
		index = 0;

	return index;
}

unsigned int of_aml_clk_get_parent_count(struct device *dev, int index)
{
	int count, ret;
	struct device_node *dev_np = dev_of_node(dev);
	struct of_phandle_args parent_args;

	/*
	 * If there is only one "clocks" property defined on the current device
	 * node, then the clock specified by "clocks" is the parent clock of the
	 * current device.
	 */
	if (of_property_present(dev_np, "clocks") &&
	    !of_property_present(dev_np, "clock-parents") &&
	    (of_count_phandle_with_args(dev_np, "clocks", "#clock-cells") == 1))
		return 1;

	index = of_aml_clk_get_real_parent_index(dev_np, index);
	if (index < 0)
		return 0;

	ret = of_parse_phandle_with_args(dev_np, "clock-parents", "#clock-cells",
					 index, &parent_args);
	if (ret)
		return 0;

	/*
	 * The parent clock(s) of each clock in the DT are defined according to
	 * the following rules:
	 * 1 One-to-One Relationship:
	 *
	 * Example:
	 *
	 * phandle1: clock-controller@1 {
	 *	#clock-cells = <1>;
	 * }
	 *
	 * phandle2: clock-controller@2 {
	 *	clock-output-names = "clock1";
	 *	clock-parents = <&phandle1 0>;
	 * }
	 *
	 * The parent clock of "clock1" is "<&phandle1 0>".
	 *
	 * 2 One-to-Multiple Relationship:
	 *
	 * Example:
	 *
	 * phandle1: clock-controller@1 {
	 *	#clock-cells = <1>;
	 * }
	 *
	 * clock1_parents: clock-parents@1 {
	 *	clock-parents = <&phandle1 0>,
	 *			<&phandle1 1>,
	 *			<&phandle1 2>;
	 *	#clock-cells = <0>;
	 * }
	 *
	 * phandle2: clock-controller@2 {
	 *	clock-output-names = "clock1";
	 *	clock-parents = <&clock1_parents>;
	 * }
	 *
	 * The parent clock of "clock1" is "<&phandle1 0>", "<&phandle1 1>" and
	 * "<&phandle1 2>".
	 */
	if (parent_args.np != dev_np &&
	    of_property_present(parent_args.np, "clock-mult-parents"))
		count = of_count_phandle_with_args(parent_args.np,
						   "clock-parents",
						   "#clock-cells");
	else
		count = 1;

	if (count < 0)
		count = 0;

	of_node_put(parent_args.np);

	return count;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_parent_count);

static struct clk_hw *of_aml_clk_get_hw_with_args(struct of_phandle_args *args)
{
	struct clk *clk = of_clk_get_from_provider(args);
	struct clk_hw *clk_hw;

	if (IS_ERR(clk)) {
		if (PTR_ERR(clk) != -EPROBE_DEFER)
			pr_warn("clk: couldn't get clock for %pOF\n", args->np);

		return ERR_CAST(clk);
	}

	clk_hw = __clk_get_hw(clk);
	clk_put(clk);

	return clk_hw;
}

struct clk_parent_data *of_aml_clk_get_parent_data(struct device *dev,
						   struct clk_hw **dev_hws,
						   int index)
{
	struct device_node *dev_np = dev_of_node(dev);
	int i, ret, parent_num;
	struct clk_parent_data *parent_data;
	struct of_phandle_args parent_args, out_args;
	struct clk_hw *hw;

	parent_num = of_aml_clk_get_parent_count(dev, index);
	if (parent_num == 0) {
		dev_err(dev, "Invalid clock parents number, index = %d\n", index);
		return ERR_PTR(-EINVAL);
	}

	parent_data = devm_kcalloc(dev, parent_num, sizeof(*parent_data),
				   GFP_KERNEL);
	if (!parent_data) {
		ret = -ENOMEM;
		goto fail_node_put;
	}

	if (of_property_present(dev_np, "clocks") &&
	    !of_property_present(dev_np, "clock-parents") &&
	    (of_count_phandle_with_args(dev_np, "clocks", "#clock-cells") == 1)) {
		ret = of_parse_phandle_with_args(dev_np, "clocks",
						 "#clock-cells",
						 0, &out_args);
		if (ret)
			goto fail_out;

		if (out_args.np == dev_np) {
			parent_data[0].hw = dev_hws[out_args.args[0]];
		} else {
			hw = of_aml_clk_get_hw_with_args(&out_args);
			if (IS_ERR(hw)) {
				ret = PTR_ERR(hw);
				goto fail_out;
			}

			parent_data[0].hw = hw;
		}

		of_node_put(out_args.np);

		return parent_data;
	}

	index = of_aml_clk_get_real_parent_index(dev_np, index);
	if (index < 0)
		return ERR_PTR(index);

	ret = of_parse_phandle_with_args(dev_np, "clock-parents", "#clock-cells",
					 index, &parent_args);
	if (ret)
		return ERR_PTR(ret);

	for (i = 0; i < parent_num; i++) {
		if (parent_args.np == dev_np) {
			parent_data[i].hw = dev_hws[parent_args.args[0]];
		} else {
			if (!of_property_present(parent_args.np, "clock-mult-parents")) {
				hw = of_aml_clk_get_hw_with_args(&parent_args);
				if (IS_ERR(hw)) {
					ret = PTR_ERR(hw);
					goto fail_out;
				}

				parent_data[i].hw = hw;
			} else {
				ret = of_parse_phandle_with_args(parent_args.np,
								 "clock-parents",
								 "#clock-cells",
								 i, &out_args);
				if (ret)
					goto fail_out;

				if (out_args.np == dev_np) {
					parent_data[i].hw = dev_hws[out_args.args[0]];
				} else {
					hw = of_aml_clk_get_hw_with_args(&out_args);
					if (IS_ERR(hw)) {
						ret = PTR_ERR(hw);
						goto fail_out;
					}

					parent_data[i].hw = hw;
				}

				of_node_put(out_args.np);
			}
		}
	}

	of_node_put(parent_args.np);

	return parent_data;

fail_out:
	devm_kfree(dev, parent_data);

fail_node_put:
	of_node_put(parent_args.np);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_parent_data);

void of_aml_clk_parent_data_release(struct device *dev,
				    struct clk_parent_data *data)
{
	devm_kfree(dev, data);
}
EXPORT_SYMBOL_GPL(of_aml_clk_parent_data_release);

u32 *of_aml_clk_get_parent_table(struct device *dev, int index)
{
	struct device_node *dev_np = dev_of_node(dev);
	struct of_phandle_args parent_args;
	int i, ret, parent_num, index_num;
	u32 *mux_tab;

	index = of_aml_clk_get_real_parent_index(dev_np, index);
	if (index < 0)
		return ERR_PTR(index);

	ret = of_parse_phandle_with_args(dev_np, "clock-parents", "#clock-cells",
					 index, &parent_args);
	if (ret)
		return ERR_PTR(ret);

	index_num = of_property_count_u32_elems(parent_args.np,
						"clock-parent-indices");
	if (index_num < 0) {
		ret = 0;
		goto fail_node_put;
	}

	parent_num = of_aml_clk_get_parent_count(dev, index);
	if (index_num != parent_num) {
		dev_err(dev, "Invalid clock parent indices, index = %d\n", index);
		ret = -EINVAL;
		goto fail_node_put;
	}

	mux_tab = devm_kcalloc(dev, parent_num, sizeof(*mux_tab), GFP_KERNEL);
	if (!mux_tab) {
		ret = -ENOMEM;
		goto fail_node_put;
	}

	for (i = 0; i < parent_num; i++) {
		ret = of_property_read_u32_index(parent_args.np,
						 "clock-parent-indices",
						 i, &mux_tab[i]);
		if (ret)
			goto fail_out;
	}

	of_node_put(parent_args.np);

	return mux_tab;

fail_out:
	devm_kfree(dev, mux_tab);

fail_node_put:
	of_node_put(parent_args.np);

	if (!ret)
		return NULL;
	else
		return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_parent_table);

u32 of_aml_clk_get_count(struct device_node *np)
{
	int ret = of_property_count_strings(np, "clock-output-names");

	if (ret < 0) {
		pr_err("<%pOFn>: Invalid clock-output-names property\n", np);
		return 0;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_count);

int of_aml_clk_get_clkid_index(struct device_node *np, u32 index)
{
	u32 clks_count = of_aml_clk_get_count(np);
	u32 clkid;
	int ret = of_property_read_u32_index(np, "clock-indices", index, &clkid);

	if ((ret && index > 0) || clkid >= clks_count) {
		pr_err("<%pOFn>: Invalid clock-indices property, index = %d\n",
		       np, index);
		return -EINVAL;
	}

	return clkid;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_clkid_index);

const char *of_aml_clk_get_name_index(struct device_node *np, u32 index)
{
	const char *name;

	if (of_property_read_string_index(np, "clock-output-names", index, &name)) {
		pr_err("<%pOFn>: Invalid clock-output-names property, index = %d\n",
		       np, index);
		return NULL;
	}

	return name;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_name_index);

/*
 * The 'clock-flags' property can be defined in three forms:
 * 1) Undefined: No flags needed for any clocks.
 * 2) Single definition: Applies to all clocks under the device.
 * 3) Per-clock definition: Must match clk_num count (one entry per clock).
 */
int of_aml_clk_get_flag_index(struct device_node *np, u32 index, u32 *d_flag,
			      u32 *hw_flag)
{
	const __be32 *prop;
	unsigned int psize;
	int elems_size = 2 * sizeof(u32);
	u32 u32_index = 2 * index;
	int flags_count = of_property_count_elems_of_size(np, "clock-flags",
							  elems_size);

	prop = of_get_property(np, "clock-flags", &psize);

	if (flags_count < 0) {
		*d_flag = 0;
		*hw_flag = 0;

		return 0;
	} else if (flags_count == 1) {
		*d_flag = be32_to_cpu(prop[0]);
		*hw_flag = be32_to_cpu(prop[1]);

		return 0;
	} else if (psize >= (index + 1) * elems_size) {
		*d_flag = be32_to_cpu(prop[u32_index]);
		*hw_flag = be32_to_cpu(prop[u32_index + 1]);

		return 0;
	}

	pr_err("<%pOFn>: Invalid clock-flags property, index = %d\n", np, index);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_flag_index);

/*
 * The 'clock-div-width' property can be defined in three forms:
 * 1) Undefined: The divider register bit width uses the default value.
 * 2) Single definition: Specify the divider register bit width for all clocks
 * under the current device.
 * 3) Per-clock definition: Individually specify the divider register bit width
 * for each clock under the current device.
 */
int of_aml_clk_get_div_width_index(struct device_node *np, u32 index)
{
	const __be32 *prop;
	unsigned int psize;
	int prop_count = of_property_count_elems_of_size(np, "clock-div-width",
							 sizeof(u32));

	prop = of_get_property(np, "clock-div-width", &psize);

	if (prop_count < 0)
		return 0;
	else if (prop_count == 1)
		return be32_to_cpu(prop[0]);
	else if (psize >= (index + 1) * sizeof(u32))
		return be32_to_cpu(prop[index]);

	pr_err("<%pOFn>: Invalid clock-div-width property, index = %d\n", np,
	       index);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_div_width_index);

int of_aml_clk_get_init_reg_count(struct device_node *np)
{
	return of_property_count_elems_of_size(np, "clock-init-regs",
					       sizeof(struct reg_sequence));
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_init_reg_count);

int of_aml_clk_get_init_reg(struct device_node *np, int reg_count,
			    struct reg_sequence *init_regs)
{
	return of_property_read_u32_array(np, "clock-init-regs", (u32 *)init_regs,
					  3 * reg_count);
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_init_reg);

int of_aml_clk_get_rate_range(struct device_node *np, u32 index, u64 *min, u64 *max)
{
	const __be64 *val;
	unsigned int psize;
	int elems_size = 2 * sizeof(u64);
	u32 val_index = 2 * index;

	val = of_get_property(np, "clock-rate-range", &psize);
	if (!val)
		return -EEXIST;

	if (psize % elems_size)
		return -EINVAL;

	if (psize == elems_size) {
		*min = be64_to_cpu(val[0]);
		*max = be64_to_cpu(val[1]);

		return 0;
	} else if (psize >= (index + 1) * elems_size) {
		*min = be64_to_cpu(val[val_index]);
		*max = be64_to_cpu(val[val_index + 1]);

		return 0;
	}

	pr_err("<%pOFn>: Invalid clock-rate-range property, index = %d\n", np, index);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(of_aml_clk_get_rate_range);
#endif  /* CONFIG_OF */

MODULE_DESCRIPTION("Amlogic Of Common Clock driver");
MODULE_AUTHOR("Chuan Liu <chuan.liu@amlogic.com>");
MODULE_IMPORT_NS("CLK_AMLOGIC");
MODULE_LICENSE("GPL v2");
