// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/amlogic/media/vout/meson_tx_connector/phy/meson_tx_phy_core.h>

int meson_tx_dummy_phy_init(void *data)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_exit(void *data)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_power_on(void *data)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_power_off(void *data)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_set_mode(void *data, enum meson_tx_phy_mode mode,
	struct meson_tx_phy_cfg_opts *opts)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_configure(void *phy, struct meson_tx_phy_cfg_opts *opts)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_validate(void *phy, enum meson_tx_phy_mode mode,
			struct meson_tx_phy_cfg_opts *opts)
{
	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_dummy_phy_calibrate(void *phy)
{
	pr_info("%s\n", __func__);
	return 0;
}

struct meson_tx_phy_ops dummy_phy_ops = {
	.init = meson_tx_dummy_phy_init,
	.exit = meson_tx_dummy_phy_exit,
	.power_on = meson_tx_dummy_phy_power_on,
	.power_off = meson_tx_dummy_phy_power_off,
	.set_mode = meson_tx_dummy_phy_set_mode,
	.configure = meson_tx_dummy_phy_configure,
	.validate = meson_tx_dummy_phy_validate,
	.calibrate = meson_tx_dummy_phy_calibrate,
};

int meson_tx_bind_phy(void *param)
{
	struct platform_device *pdev;
	struct platform_device_info pdevinfo;
	struct meson_tx_phy_data data;

	memset(&pdevinfo, 0, sizeof(struct platform_device_info));

	data.para = param;
	data.ops = &dummy_phy_ops;
	pdevinfo.name = "meson_tx_phy";
	pdevinfo.data = &data;
	pdevinfo.size_data = sizeof(data);
	pdevinfo.dma_mask = 0;

	pdev = platform_device_register_full(&pdevinfo);

	pr_info("%s %px\n", __func__, pdev);

	return 0;
}

