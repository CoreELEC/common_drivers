// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"

static void meson_usb2phy_t5w_cali(struct amlogic_usb_v2 *mphy)
{
	meson_usb2phy_legacy_cali_n(mphy);
	meson_usb2phy_legacy_cali_disc_squelch(mphy);
	meson_usb2phy_set_calibration_trim(mphy);
}

static struct meson_u2phy_priv meson_u2phy_t5w_priv = {
	.set_mode = meson_u2phy_legacy_set_mode,
	.cali = meson_usb2phy_t5w_cali,
	.set_pll = meson_usb2phy_legacy_set_pll,
};

static int meson_u2phy_t5w_set_mode(void *phy, enum meson_uphy_mode mode)
{
	return meson_u2phy_legacy_set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_t5w_init(void *phy)
{
	return meson_u2phy_aml_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_t5w_priv);
}

static int meson_u2phy_t5w_exit(void *phy)
{
	return meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
}

/* For runtime port mux, the phy_power_on & phy_power_off cannot
 * be called during role switch by the controller driver. The otg
 * port power is handled by the otg driver.
 */
static int meson_u2phy_t5w_power_on(void *phy)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)phy;

	if (mphy->otg_helper.otg_port)
		return 0;

	return meson_u2phy_power_on(mphy);
}

static int meson_u2phy_t5w_power_off(void *phy)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)phy;

	if (mphy->otg_helper.otg_port)
		return 0;

	return meson_u2phy_power_off(mphy);
}

int meson_u2phy_t5w_configure(void *phy, struct meson_uphy_configure_opts *opts)
{
	int ret = 0;

	if (opts->otg_init) {
		ret = meson_uphy_rtmux_otg_init((struct amlogic_usb_v2 *)phy);
		if (ret)
			return ret;
	}

	return 0;
}

static int meson_u2phy_t5w_validate(void *phy, enum meson_uphy_mode mode,
				struct meson_uphy_configure_opts *opts)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)phy;

	if (mode == mphy->current_mode)
		return 1;
	else
		return 0;
}

static struct meson_uphy_ops meson_u2phy_t5w_ops = {
	.init = meson_u2phy_t5w_init,
	.exit = meson_u2phy_t5w_exit,
	.power_on = meson_u2phy_t5w_power_on,
	.power_off = meson_u2phy_t5w_power_off,
	.set_mode = meson_u2phy_t5w_set_mode,
	.configure = meson_u2phy_t5w_configure,
	.validate = meson_u2phy_t5w_validate,
};

struct meson_uphy_pdata meson_uphy_t5w_pdata = {
	.u2phy_ops = &meson_u2phy_t5w_ops,
	.u2phy_parse = meson_aml_u2phy_parse,
	.otg_parse = meson_uphy_rtmux_otg_parse,
	.otg_remove = meson_uphy_rtmux_otg_remove,
};
