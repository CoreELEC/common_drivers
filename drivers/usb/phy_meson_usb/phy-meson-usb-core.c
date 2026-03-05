// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "phy-meson-usb.h"

static int meson_uphy_init(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = -EINVAL;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->init)
			ret = ppool->pdata->u2phy_ops->init(instance->meson_uphy);
	} else {
		if (ppool->pdata->u3phy_ops->init)
			ret = ppool->pdata->u3phy_ops->init(instance->meson_uphy);
	}

	if (ret == -EINVAL) {
		dev_err(&phy->dev,
			"%s suspect no matched callback. Please double check.",
				__func__);
		ret = 0;
	}

	return ret;
}

static int meson_uphy_power_on(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->power_on)
			ret = ppool->pdata->u2phy_ops->power_on(instance->meson_uphy);
	} else {
		if (ppool->pdata->u3phy_ops->power_on)
			ret = ppool->pdata->u3phy_ops->power_on(instance->meson_uphy);
	}

	return ret;
}

static int meson_uphy_power_off(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->power_off)
			ret = ppool->pdata->u2phy_ops->power_off(instance->meson_uphy);
	} else {
		if (ppool->pdata->u3phy_ops->power_off)
			ret = ppool->pdata->u3phy_ops->power_off(instance->meson_uphy);
	}

	return ret;
}

static int meson_uphy_exit(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->exit)
			ret = ppool->pdata->u2phy_ops->exit(instance->meson_uphy);
	} else {
		if (ppool->pdata->u3phy_ops->exit)
			ret = ppool->pdata->u3phy_ops->exit(instance->meson_uphy);
	}

	return ret;
}

static int meson_uphy_set_mode(struct phy *phy, enum phy_mode mode, int submode)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;
	enum meson_uphy_mode mup_mode;

	if (unlikely(submode))
		return -EINVAL;

	switch (mode) {
	case PHY_MODE_USB_HOST:
		mup_mode = MESON_USB_MODE_HOST;
		break;
	case PHY_MODE_USB_DEVICE:
		mup_mode = MESON_USB_MODE_DEVICE;
		break;
	case PHY_MODE_USB_OTG:
		mup_mode = MESON_USB_MODE_OTG;
		break;
	default:
		return -EINVAL;
	}

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->set_mode)
			ret = ppool->pdata->u2phy_ops->set_mode(instance->meson_uphy, mup_mode);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->set_mode)
			ret = ppool->pdata->u3phy_ops->set_mode(instance->meson_uphy, mup_mode);
	}

	return ret;
}

static int meson_uphy_configure(struct phy *phy, union phy_configure_opts *opts)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->configure)
			ret = ppool->pdata->u2phy_ops->configure(instance->meson_uphy,
					(struct meson_uphy_configure_opts *)opts);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->configure)
			ret = ppool->pdata->u3phy_ops->configure(instance->meson_uphy,
					(struct meson_uphy_configure_opts *)opts);
	}

	return ret;
}

static int meson_uphy_validate(struct phy *phy, enum phy_mode mode, int submode,
			    union phy_configure_opts *opts)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;
	enum meson_uphy_mode mup_mode;

	if (unlikely(submode))
		return -EINVAL;

	switch (mode) {
	case PHY_MODE_USB_HOST:
		mup_mode = MESON_USB_MODE_HOST;
		break;
	case PHY_MODE_USB_DEVICE:
		mup_mode = MESON_USB_MODE_DEVICE;
		break;
	case PHY_MODE_USB_OTG:
		mup_mode = MESON_USB_MODE_OTG;
		break;
	default:
		return -EINVAL;
	}

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->validate)
			ret = ppool->pdata->u2phy_ops->validate(instance->meson_uphy, mup_mode,
					(struct meson_uphy_configure_opts *)opts);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->validate)
			ret = ppool->pdata->u3phy_ops->validate(instance->meson_uphy, mup_mode,
					(struct meson_uphy_configure_opts *)opts);
	}

	return ret;
}

static int meson_uphy_calibrate(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->calibrate)
			ret = ppool->pdata->u2phy_ops->calibrate(instance->meson_uphy);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->calibrate)
			ret = ppool->pdata->u3phy_ops->calibrate(instance->meson_uphy);
	}

	return ret;
}

static int meson_uphy_connect(struct phy *phy, int port)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->connect)
			ret = ppool->pdata->u2phy_ops->connect(instance->meson_uphy, port);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->connect)
			ret = ppool->pdata->u3phy_ops->connect(instance->meson_uphy, port);
	}

	return ret;
}

static int meson_uphy_disconnect(struct phy *phy, int port)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_ops && ppool->pdata->u2phy_ops->disconnect)
			ret = ppool->pdata->u2phy_ops->disconnect(instance->meson_uphy, port);
	} else {
		if (ppool->pdata->u3phy_ops && ppool->pdata->u3phy_ops->disconnect)
			ret = ppool->pdata->u3phy_ops->disconnect(instance->meson_uphy, port);
	}

	return ret;
}

static struct phy *meson_uphy_xlate(struct device *dev,
					const struct of_phandle_args *args)
{
	struct meson_uphy_pool *ppool = dev_get_drvdata(dev);
	struct meson_uphy_instance *instance = NULL;
	struct device_node *phy_np = args->np;
	int index;
	//int ret;

	for (index = 0; index < ppool->nphys; index++)
		if (ppool->phys[index] &&
			ppool->phys[index]->phy &&
			phy_np == ppool->phys[index]->phy->dev.of_node) {
			instance = ppool->phys[index];
			break;
		}

	if (!instance) {
		dev_err(dev, "failed to find appropriate phy\n");
		return ERR_PTR(-ENODEV);
	}

	return instance->phy;
}

static int meson_uphy_parse(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (instance->speed < MESON_USB_SPEED_SUPER) {
		if (ppool->pdata->u2phy_parse)
			ret = ppool->pdata->u2phy_parse(&phy->dev, instance);
	} else {
		if (ppool->pdata->u3phy_parse)
			ret = ppool->pdata->u3phy_parse(&phy->dev, instance);
	}

	return ret;
}

static int meson_uphy_otg_parse(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);
	int ret = 0;

	if (ppool->pdata->otg_parse && instance->speed < MESON_USB_SPEED_SUPER)
		ret = ppool->pdata->otg_parse(&phy->dev, instance);

	return ret;
}

static void meson_uphy_otg_remove(struct phy *phy)
{
	struct meson_uphy_instance *instance = phy_get_drvdata(phy);
	struct meson_uphy_pool *ppool = dev_get_drvdata(phy->dev.parent);

	if (ppool->pdata->otg_remove && instance->speed < MESON_USB_SPEED_SUPER)
		ppool->pdata->otg_remove(instance);
}

static const struct phy_ops meson_uphy_generic_ops = {
	.init		= meson_uphy_init,
	.exit		= meson_uphy_exit,
	.power_on	= meson_uphy_power_on,
	.power_off	= meson_uphy_power_off,
	.set_mode	= meson_uphy_set_mode,
	.configure = meson_uphy_configure,
	.validate = meson_uphy_validate,
	.calibrate = meson_uphy_calibrate,
	.connect = meson_uphy_connect,
	.disconnect = meson_uphy_disconnect,
};

static int meson_uphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct phy_provider *provider;
	struct meson_uphy_pool *ppool;
	struct meson_uphy_instance *instance;
	struct phy *phy;
	int i = 0, ret = -EINVAL, u2port = 0, u3port = 0, r_i;

	ppool = kzalloc(sizeof(*ppool), GFP_KERNEL);
	if (!ppool)
		return -ENOMEM;

	ppool->pdata = of_device_get_match_data(dev);
	if (!ppool->pdata)
		return -EINVAL;

	ppool->nphys = of_get_child_count(np);
	ppool->phys = kcalloc(ppool->nphys,
				       sizeof(*ppool->phys), GFP_KERNEL);
	if (!ppool->phys)
		return -ENOMEM;

	ppool->dev = dev;
	platform_set_drvdata(pdev, ppool);

	for_each_child_of_node_scoped(np, child_np) {
		instance = kzalloc(sizeof(*instance), GFP_KERNEL);
		if (!instance)
			return -ENOMEM;

		ppool->phys[i] = instance;
		instance->domain_pool = ppool;

		phy = phy_create(dev, child_np, &meson_uphy_generic_ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "failed to create phy\n");
			ret = PTR_ERR(phy);
			goto err;
		}

		phy_set_drvdata(phy, instance);
		instance->phy = phy;
		ret = of_property_read_u32(phy->dev.of_node, "port-speed", &instance->port_speed);
		if (ret < 0) {
			dev_err(dev, "err port-speed prop not set.\n");
			goto err_phy;
		}

		if (instance->port_speed < MESON_USB_SPEED_SUPER)
			instance->index = u2port++;
		else
			instance->index = u3port++;

		ret = meson_uphy_parse(phy);
		if (ret) {
			dev_err(&phy->dev, "parse_props ret %d.\n", ret);
			goto err_parse;
		}

		if (of_property_read_bool(phy->dev.of_node, "phy-otg")) {
			ret = meson_uphy_otg_parse(phy);
			if (ret) {
				dev_err(&phy->dev, "parse_otg ret %d.\n", ret);
				goto err_otg_parse;
			}
		}
		i++;
	}

	provider = devm_of_phy_provider_register(dev, meson_uphy_xlate);

	return PTR_ERR_OR_ZERO(provider);

err_otg_parse:
	for (r_i = i;  r_i >= 0; r_i--)
		meson_uphy_otg_remove(ppool->phys[r_i]->phy);
err_parse:
	for (r_i = i;  r_i >= 0; r_i--) {
		put_device(&ppool->phys[r_i]->phy->dev);
		kfree(ppool->phys[r_i]->meson_uphy);
	}
err_phy:
	for (r_i = i;  r_i >= 0; r_i--) {
		phy_set_drvdata(ppool->phys[r_i]->phy, NULL);
		phy_destroy(ppool->phys[r_i]->phy);
		ppool->phys[r_i]->phy = NULL;
	}
err:
	for (r_i = i;  r_i >= 0; r_i--) {
		kfree(ppool->phys[r_i]);
		ppool->phys[r_i] = NULL;
	}
	return ret;
}

static void meson_uphy_remove(struct platform_device *pdev)
{
	int i;
	struct meson_uphy_pool *ppool = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s started.\n", __func__);
	for (i = 0; i < ppool->nphys; i++) {
		dev_dbg(&ppool->phys[i]->phy->dev, "phy idx:%d ptr %px ref %d\n",
				i, &ppool->phys[i]->phy->dev,
				kref_read(&ppool->phys[i]->phy->dev.kobj.kref));

		meson_uphy_otg_remove(ppool->phys[i]->phy);

		/* FIXME: All low level phy structs now are allocated all-in-one.
		 * Refactor to call private phy remove callback once things get
		 * more complex.
		 */
		kfree(ppool->phys[i]->meson_uphy);
		if (ppool->phys[i]->meson_uphy)
			put_device(&ppool->phys[i]->phy->dev);

		dev_dbg(&ppool->phys[i]->phy->dev, "ref %d\n",
				kref_read(&ppool->phys[i]->phy->dev.kobj.kref));
		phy_set_drvdata(ppool->phys[i]->phy, NULL);
		phy_destroy(ppool->phys[i]->phy);
		dev_dbg(&ppool->phys[i]->phy->dev, "ref %d\n",
				kref_read(&ppool->phys[i]->phy->dev.kobj.kref));
		ppool->phys[i]->phy = NULL;

		kfree(ppool->phys[i]);
		ppool->phys[i] = NULL;
		dev_dbg(&pdev->dev, "%s inst %d destroyed.\n", __func__, i);
	}
	platform_set_drvdata(pdev, NULL);
	kfree(ppool);
	dev_dbg(&pdev->dev, "%s finished.\n", __func__);
}

static const struct of_device_id meson_uphy_id_table[] = {
	{ .compatible = "amlogic,uphy-sm1", .data = &meson_uphy_sm1_pdata },
	{ .compatible = "amlogic,uphy-sc2", .data = &meson_uphy_sc2_pdata },
	{ .compatible = "amlogic,u2phy-s4", .data = &meson_uphy_s4_pdata },
	{ .compatible = "amlogic,uphy-t7c", .data = &meson_uphy_t7c_pdata },
	{ .compatible = "amlogic,u2phy-a5", .data = &meson_uphy_a5_pdata },
	{ .compatible = "amlogic,u2phy-aml-s5", .data = &meson_uphy_s5_aml_pdata },
	{ .compatible = "amlogic,uphy-m31-s5", .data = &meson_uphy_s5_m31_pdata },
	{ .compatible = "amlogic,u2phy-aml-t5m", .data = &meson_uphy_t5m_aml_pdata },
	{ .compatible = "amlogic,uphy-m31-t5m", .data = &meson_uphy_t5m_m31_pdata },
	{ .compatible = "amlogic,u2phy-a4", .data = &meson_uphy_a4_pdata },
	{ .compatible = "amlogic,u2phy-txhd2", .data = &meson_uphy_txhd2_pdata },
	{ .compatible = "amlogic,u2phy-s7", .data = &meson_uphy_s7_pdata },
	{ .compatible = "amlogic,u2phy-s7d", .data = &meson_uphy_s7d_pdata },
	{ .compatible = "amlogic,uphy-s6", .data = &meson_uphy_s6_pdata },
	{ .compatible = "amlogic,u2phy-t6d", .data = &meson_uphy_t6d_pdata },
	{ .compatible = "amlogic,u2phy-t6w", .data = &meson_uphy_t6w_pdata },
	{ .compatible = "amlogic,uphy-t6x", .data = &meson_uphy_t6x_pdata },
	{ .compatible = "amlogic,u2phy-c4", .data = &meson_u2phy_c4_pdata },
	{ },
};
MODULE_DEVICE_TABLE(of, meson_uphy_id_table);

static void meson_uphy_complete(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.complete)
			ppool->phys[i]->pm_ops.complete(ppool->phys[i]);
	}
}

static int meson_uphy_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i, ret = 0;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.suspend)
			ret = ppool->phys[i]->pm_ops.suspend(ppool->phys[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static int meson_uphy_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i, ret = 0;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.resume)
			ret = ppool->phys[i]->pm_ops.resume(ppool->phys[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static int meson_uphy_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i, ret = 0;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.freeze)
			ret = ppool->phys[i]->pm_ops.freeze(ppool->phys[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static int meson_uphy_thaw(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i, ret = 0;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.thaw)
			ret = ppool->phys[i]->pm_ops.thaw(ppool->phys[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static int meson_uphy_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_uphy_pool *ppool = platform_get_drvdata(pdev);
	int i, ret = 0;

	for (i = 0; i < ppool->nphys; i++) {
		if (ppool->phys[i]->pm_ops.restore)
			ret = ppool->phys[i]->pm_ops.restore(ppool->phys[i]);
		if (ret)
			return ret;
	}
	return ret;
}

static const struct dev_pm_ops meson_uphy_pm_ops = {
	.complete = meson_uphy_complete,
	.suspend = meson_uphy_suspend,
	.resume = meson_uphy_resume,
	.freeze = meson_uphy_freeze,
	.thaw = meson_uphy_thaw,
	.restore = meson_uphy_restore,
};

static struct platform_driver meson_uphy_driver = {
	.probe		= meson_uphy_probe,
	.remove		= meson_uphy_remove,
	.driver		= {
		.name	= "meson-usbphy",
		.of_match_table = meson_uphy_id_table,
		.pm = &meson_uphy_pm_ops,
	},
};

int __init meson_uphy_drv_init(void)
{
	return platform_driver_register(&meson_uphy_driver);
}

void __exit meson_uphy_drv_exit(void)
{
	return platform_driver_unregister(&meson_uphy_driver);
}
