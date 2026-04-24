// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>

#define CRG_DRD_INDEX_MASK ((u32)GENMASK(14, 13))
#define CRG_DRD_INDEX_SHIFT 13
/* MAX 4 drd indexes because we only have 3 bits left in the trim reg. */
#define CRG_DRD_INDEX_VALID_DEF (BIT(15))

struct crg_drd_glue {
	struct device		*dev;
	void __iomem *mode;
	u32 def_drd_controller;
	struct property *props;
};

#define NUM_PROPS ((3 * 2) + 1)
#define COMPATIBLE_CRG_UDC "amlogic, crg_udc"
#define COMPATIBLE_CRG_DRD "amlogic, crg-drd"
#define COMPATIBLE_CRG_HOST "amlogic, crg-host-drd"

static int crg_drd_glue_add_compatible_prop(struct crg_drd_glue *priv, struct device_node	*np)
{
	u32 controller_index = 0;
	u32 drd_index = (readl(priv->mode) & (CRG_DRD_INDEX_MASK)) >> CRG_DRD_INDEX_SHIFT;
	bool drd_index_valid = readl(priv->mode) & CRG_DRD_INDEX_VALID_DEF;
	struct property *props = NULL;
	static int i;
	int ret = 0;

	dev_dbg(priv->dev, "%s checking.\n", np->full_name);

	if (!drd_index_valid)
		drd_index = priv->def_drd_controller;

	ret = of_property_read_u32(np, "index", &controller_index);
	if (ret < 0) {
		dev_err(priv->dev, "%s no controller-index prop.\n", np->full_name);
		return ret;
	}

	dev_dbg(priv->dev, "%s index %d expect %d(%s).\n", np->full_name,
		controller_index, drd_index, drd_index_valid ? "from mode reg" : "default");
	/* MAX index 3 as in the trim reg. */
	WARN_ON(controller_index > 3);

	if (!priv->props) {
		props = devm_kcalloc(priv->dev, NUM_PROPS, sizeof(*props), GFP_KERNEL);
		if (!props) {
			ret = -ENOMEM;
			goto err;
		}
		priv->props = props;
	}
	props = priv->props;

	if (drd_index == controller_index) {
		if (strstr(np->name, "usbphy")) {
			struct device_node	*child_np = NULL;

			for_each_child_of_node(np, child_np) {
				if (strstr(child_np->name, "u2p")) {
					props[i].name = devm_kstrdup(priv->dev, "phy-otg",
										GFP_KERNEL);
					ret = of_add_property(child_np, &props[i]);
					if (ret)
						goto err;
					dev_dbg(priv->dev, "%s add prop %s.\n",
								child_np->full_name, props[i].name);
					i++;
				}
			}
		} else if (strstr(np->name, "udc")) {
			props[i].name = devm_kstrdup(priv->dev, "compatible", GFP_KERNEL);
			props[i].value = devm_kstrdup(priv->dev, COMPATIBLE_CRG_UDC, GFP_KERNEL);
			if (!props[i].name || !props[i].value) {
				ret = -ENOMEM;
				goto err;
			}
			props[i].length = sizeof(COMPATIBLE_CRG_UDC);
			ret = of_add_property(np, &props[i]);
			if (ret)
				goto err;
			dev_dbg(priv->dev, "%s add prop %s val:%s.\n", np->full_name,
					props[i].name, (char *)props[i].value);
			i++;
		} else if (strstr(np->name, "drd")) {
			props[i].name = devm_kstrdup(priv->dev, "compatible", GFP_KERNEL);
			props[i].value = devm_kstrdup(priv->dev, COMPATIBLE_CRG_DRD, GFP_KERNEL);
			if (!props[i].name || !props[i].value) {
				ret = -ENOMEM;
				goto err;
			}
			props[i].length = sizeof(COMPATIBLE_CRG_DRD);
			ret = of_add_property(np, &props[i]);
			if (ret)
				goto err;
			dev_dbg(priv->dev, "%s add prop %s val:%s.\n", np->full_name,
					props[i].name, (char *)props[i].value);
			i++;
		}
	} else {
		if (strstr(np->name, "drd")) {
			props[i].name = devm_kstrdup(priv->dev, "compatible", GFP_KERNEL);
			props[i].value = devm_kstrdup(priv->dev, COMPATIBLE_CRG_HOST, GFP_KERNEL);
			if (!props[i].name || !props[i].value) {
				ret = -ENOMEM;
				goto err;
			}
			props[i].length = sizeof(COMPATIBLE_CRG_HOST);
			ret = of_add_property(np, &props[i]);
			if (ret)
				goto err;
			dev_dbg(priv->dev, "%s add prop %s val:%s.\n", np->full_name,
					props[i].name, (char *)props[i].value);
			i++;
		}
	}

	WARN_ON(i > NUM_PROPS);

err:
	return ret;
}

static int crg_drd_glue_probe(struct platform_device *pdev)
{
	struct crg_drd_glue	*priv;
	struct device		*dev = &pdev->dev;
	struct device_node	*np = dev->of_node;
	struct device_node	*child_np = NULL;
	int ret;
	struct resource	*res;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	priv->mode = devm_ioremap(dev, res->start, resource_size(res));
	if (!priv->mode) {
		dev_err(dev, "ioremap failed\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "def-drd-controller", &priv->def_drd_controller);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	for_each_child_of_node(np, child_np) {
		ret = crg_drd_glue_add_compatible_prop(priv, child_np);
		if (ret) {
			dev_err(dev, "fail to process node %s. ret %d\n", child_np->full_name, ret);
			return ret;
		}
	}

	/*
	 * of_device_make_bus_id Use the device node data to assign a unique name
	 * This routine will first try using the translated bus address to
	 * derive a unique name. If it cannot, then it will prepend names from
	 * parent nodes until a unique name can be derived.
	 */
	ret = of_platform_populate(np, NULL, NULL, NULL);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	return ret;
}

static void crg_drd_glue_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	of_platform_depopulate(dev);

	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
	pm_runtime_set_suspended(dev);
}

static int __maybe_unused crg_drd_glue_runtime_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused crg_drd_glue_runtime_resume(struct device *dev)
{
	return 0;
}

static int __maybe_unused crg_drd_glue_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused crg_drd_glue_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops crg_drd_glue_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(crg_drd_glue_suspend, crg_drd_glue_resume)
	SET_RUNTIME_PM_OPS(crg_drd_glue_runtime_suspend,
			   crg_drd_glue_runtime_resume, NULL)
};

static const struct of_device_id crg_drd_glue_match[] = {
	{
		.compatible = "amlogic, crg-drd-glue",
	},
	{ /* Sentinel */ }
};
MODULE_DEVICE_TABLE(of, crg_drd_glue_match);

static struct platform_driver crg_drd_glue_driver = {
	.probe		= crg_drd_glue_probe,
	.remove		= crg_drd_glue_remove,
	.driver		= {
		.name	= "crg-drd-glue",
		.of_match_table = crg_drd_glue_match,
		.pm	= &crg_drd_glue_dev_pm_ops,
	},
};

int __init crg_drd_glue_init(void)
{
	platform_driver_register(&crg_drd_glue_driver);

	return 0;
}
