// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/reboot.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/thermal.h>
#include <linux/bitfield.h>
#include <linux/of_device.h>
#include <linux/amlogic/vir_tsens.h>
#include <linux/amlogic/meson_cooldev.h>
#include "thermal_core.h"

static int vir_get_temp(struct thermal_zone_device *p, int *temp)
{
	const struct vir_tsens *vir_tsens = p->devdata;
	int ret;

	ret = thermal_zone_get_temp(vir_tsens->soc_tzd, temp);
	if (ret) {
		if (ret != -EAGAIN)
			dev_err(vir_tsens->dev, "fail to read temperature %d\n", ret);
		return ret;
	}

	return 0;
}

static struct thermal_zone_device_ops vir_tsens_ops = {
	.get_temp = vir_get_temp,
};

int vir_register_device(struct vir_tsens *vir_tsens)
{
	int id;

	for (id = 0; id < vir_tsens->num_vtsens; id++) {
		vir_tsens->tzd_array[id] = devm_thermal_of_zone_register(vir_tsens->dev, id,
									 vir_tsens,
									 &vir_tsens_ops);
		if (IS_ERR(vir_tsens->tzd_array[id])) {
			for (id--; id >= 0; id--) {
				devm_thermal_of_zone_unregister(vir_tsens->dev,
								vir_tsens->tzd_array[id]);
			}
			return -EINVAL;
		}
	}

	return 0;
}

static int vir_params_init(struct vir_tsens *vir_tsens)
{
	const char *name;
	int ret;

	ret = of_property_read_u32(vir_tsens->dev->of_node, "num-vtsens", &vir_tsens->num_vtsens);
	if (ret) {
		dev_err(vir_tsens->dev, "get num-vtsensor failed\n");
		return ret;
	}

	vir_tsens->tzd_array = devm_kcalloc(vir_tsens->dev, vir_tsens->num_vtsens,
					    sizeof(struct thermal_zone_device *), GFP_KERNEL);
	if (!vir_tsens->tzd_array)
		return -ENOMEM;

	ret = of_property_read_string(vir_tsens->dev->of_node, "zone-name", &name);
	if (ret)
		return ret;
	/*for get soc_thermal temp*/
	vir_tsens->soc_tzd = thermal_zone_get_zone_by_name(name);
	if (IS_ERR(vir_tsens->soc_tzd)) {
		dev_err(vir_tsens->dev, "fail to get thermal zone %ld\n",
			PTR_ERR(vir_tsens->soc_tzd));
		return PTR_ERR(vir_tsens->soc_tzd);
	}

	return ret;
}

static int vir_tsens_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vir_tsens *vir_tsens;
	int ret;

	vir_tsens = devm_kzalloc(dev, sizeof(*vir_tsens), GFP_KERNEL);
	if (!vir_tsens)
		return -ENOMEM;

	vir_tsens->dev = dev;
	ret = vir_params_init(vir_tsens);
	if (ret)
		return ret;

	vir_register_device(vir_tsens);
	dev_set_drvdata(dev, vir_tsens);
	dev_dbg(dev, "virtual sensor probe done\n");

	return 0;
}

static void vir_tsens_remove(struct platform_device *pdev)
{
	struct vir_tsens *vir_tsens = dev_get_drvdata(&pdev->dev);
	int id;

	for (id = 0; id < vir_tsens->num_vtsens; id++) {
		if (vir_tsens->tzd_array[id])
			devm_thermal_of_zone_unregister(&pdev->dev, vir_tsens->tzd_array[id]);
	}
}

static const struct of_device_id control_of_match[] = {
	{
		.compatible = "amlogic, vir-tsens",
	},
	{},
};

struct platform_driver vir_tsens_platdrv = {
	.driver = {
		.name		= "vir-tsens",
		.owner		= THIS_MODULE,
		.of_match_table = control_of_match,
	},
	.probe	= vir_tsens_probe,
	.remove	= vir_tsens_remove,
	.shutdown = vir_tsens_remove,
};

