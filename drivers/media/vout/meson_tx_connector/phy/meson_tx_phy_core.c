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

int meson_tx_phy_init(struct meson_tx_phy *phy)
{
	if (phy->ops->init)
		phy->ops->init(phy);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_exit(struct meson_tx_phy *phy)
{
	if (phy->ops->exit)
		phy->ops->exit(phy);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_power_on(struct meson_tx_phy *phy)
{
	if (phy->ops->power_on)
		phy->ops->power_on(phy);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_power_off(struct meson_tx_phy *phy)
{
	if (phy->ops->power_off)
		phy->ops->power_off(phy);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_set_mode(struct meson_tx_phy *phy, enum meson_tx_phy_mode mode,
	struct meson_tx_phy_cfg_opts *opts)
{
	if (phy->ops->set_mode)
		phy->ops->set_mode(phy, mode, opts);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_set_aux_gpio(struct meson_tx_phy *phy, u32 gpio1, u32 gpio2)
{
	if (phy->ops->set_aux_gpio)
		phy->ops->set_aux_gpio(phy, gpio1, gpio2);

	return 0;
}

int meson_tx_phy_configure(struct meson_tx_phy *phy, struct meson_tx_phy_cfg_opts *opts)
{
	if (phy->ops->configure)
		phy->ops->configure(phy, opts);

	return 0;
}

int meson_tx_phy_toggle(struct meson_tx_phy *phy)
{
	if (phy->ops->toggle)
		return phy->ops->toggle(phy);

	return 0;
}

int meson_tx_phy_validate(struct meson_tx_phy *phy, enum meson_tx_phy_mode mode,
			struct meson_tx_phy_cfg_opts *opts)
{
	if (phy->ops->validate)
		return phy->ops->validate(phy, mode, opts);

	pr_info("%s\n", __func__);
	return -1;
}

int meson_tx_phy_calibrate(struct meson_tx_phy *phy)
{
	if (phy->ops->calibrate)
		phy->ops->calibrate(phy);

	pr_info("%s\n", __func__);
	return 0;
}

int meson_tx_phy_dump_reg(struct meson_tx_phy *phy, struct seq_file *s)
{
	if (phy->ops->dump_reg)
		phy->ops->dump_reg(phy, s);

	return 0;
}
