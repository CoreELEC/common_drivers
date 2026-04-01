// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"

static int meson_usb2phy_c4_set_pll(struct amlogic_usb_v2 *mphy)
{
#define USB_MPPLL_RST BIT(0)
#define USB_MPPLL_LOCK_RST BIT(1)
#define USB2_BGR_EN BIT(0)
#define USB_MPPLL_EN_CTRL BIT(29)
#define USB_MPPLL_EN BIT(2)
#define	USBPLL_LOCK_FLAG BIT(31)

	void __iomem *pll_cfg = mphy->phy_cfg[0] + 0x40;
	void __iomem *pll_cfg_x = mphy->phy_cfg[0] + 0x5c;
	u32 retry = 500;
	int plldone_i;
	u32 val;

	while (retry--) {
		/* assert usb_mppll_rst and usb_lock_rst */
		val = readl(pll_cfg);
		val |= USB_MPPLL_RST | USB_MPPLL_LOCK_RST;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
		/* enable usb2_bgr_en off 0x5c */
		val = readl(pll_cfg_x);
		val |= USB2_BGR_EN;
		writel(val, pll_cfg_x);
		/* enable usb_mppll_en_ctrl */
		val = readl(pll_cfg);
		val |= USB_MPPLL_EN_CTRL;
		writel(val, pll_cfg);
		/* enable usb_mppll_en */
		val = readl(pll_cfg);
		val |= USB_MPPLL_EN;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
		/* release usb_mppll_rst */
		val = readl(pll_cfg);
		val &= ~USB_MPPLL_RST;
		writel(val, pll_cfg);
		/* release usb_mppll_lock_rst */
		val = readl(pll_cfg);
		val &= ~USB_MPPLL_LOCK_RST;
		writel(val, pll_cfg);
		/* Check lock bit. */
		for (plldone_i = 5; plldone_i > 0; plldone_i--) {
			usleep_range(20, 30);
			if (readl(pll_cfg) & USBPLL_LOCK_FLAG)
				goto okay;
		}
		dev_dbg(mphy->dev, "usb2 pll not locked, retry. val: 0x%08x\n",
			readl(pll_cfg));
	}
	dev_err(mphy->dev, "%s set pll failed, exit, val:0x%08x.\n", __func__, readl(pll_cfg));
	return -ETIMEDOUT;
okay:
	dev_dbg(mphy->dev, "usb2 pll init done, val: 0x%08x\n", readl(pll_cfg));
	return 0;
}

static int meson_usb2phy_c4_cali_disc_squelch
			(struct amlogic_usb_v2 *mphy)
{
	u32 val = 0;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* usb2_squelch_trim:
	 * usb2.0: reg32_03[3:0] (MSB->LSB) default 4b'0111.
	 * usb2_disc_trim:
	 *	usb2.0: reg32_03[6:4] (MSB->LSB) default 3b'110.
	 */
#define DISCONNECT_MASK ((u32)GENMASK(6, 4))
#define DISCONNECT_VAL(x) ((0x7 & (x)) << 4)
	val = readl(cfg + 0xc);
	/* Set to the MAX. */
	val = (val & ~DISCONNECT_MASK) | DISCONNECT_VAL(0x7);
	writel(val, cfg + 0xc);

	/* Trimming for squelch detect threshold. */
	val = readl(cfg + 0xc);
	val = (val & ~((u32)GENMASK(3, 0))) | (0xf << 0);
	writel(val, cfg + 0xc);

	return 0;
}

static int meson_usb2phy_c4_improve_eye_height
			(struct amlogic_usb_v2 *mphy)
{
	u32 val = 0;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* improve eye height. */
	val = readl(cfg + 0x54);
	val = (val & ~((u32)GENMASK(11, 9))) | (0x4 << 9);
	writel(val, cfg + 0x54);

	return 0;
}

static int meson_usb2phy_c4_set_mode
			(struct amlogic_usb_v2 *mphy, enum meson_uphy_mode mode)
{
	u32 val = 0;
	void __iomem *cfg2_reg = mphy->regs;

	val = readl(cfg2_reg + 0x88);
	if (mode == MESON_USB_MODE_HOST) {
		val &= ~BIT(25);
		writel(val, cfg2_reg + 0x88);
		dev_info(mphy->dev, "set host mode\n");
	} else {
		val |= BIT(25);
		writel(val, cfg2_reg + 0x88);
		dev_info(mphy->dev, "set device mode\n");
	}
	usleep_range(20, 30);

	return 0;
}

static void meson_usb2phy_c4_cali(struct amlogic_usb_v2 *mphy)
{
	meson_usb2phy_set_calibration_trim(mphy);
	meson_usb2phy_c4_cali_disc_squelch(mphy);
	meson_usb2phy_c4_improve_eye_height(mphy);
}

static struct meson_u2phy_priv meson_u2phy_c4_priv = {
	.set_mode = meson_u2phy_set_mode,
	.cali = meson_usb2phy_c4_cali,
	.set_pll = meson_usb2phy_c4_set_pll,
};

static int meson_u2phy_c4_set_mode(void *phy, enum meson_uphy_mode mode)
{
	meson_usb2phy_c4_set_mode((struct amlogic_usb_v2 *)phy, mode);
	return meson_u2phy_set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_c4_init(void *phy)
{
	return meson_u2phy_aml_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_c4_priv);
}

static int meson_u2phy_c4_exit(void *phy)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)phy;
	int ret = 0;

	if (mphy->suspend_flag) {
		dev_err(mphy->dev, "%s excessive exit\n", __func__);
		return -EBUSY;
	}

	meson_u2phy_usb_hold_reset(mphy, false);
	ret = meson_u2phy_hold_reset(mphy, false);
	/* Only t6w & t6d have reg_reset. */
	ret = meson_u2phy_reg_hold_reset(mphy, false);

	/* Apb reset is proposed after c4 t6x. */
	ret = meson_u2phy_apb_hold_reset(mphy, false);
	ret = meson_u2phy_comb_hold_reset(mphy, false);

	if (mphy->suspend_flag == 0)
		clk_bulk_disable_unprepare(mphy->clk_num, mphy->clks);

	meson_u2phy_set_reset_count();
	mphy->suspend_flag = 1;

	return ret;
	//return meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
}

//static int meson_u2phy_c4_power_on(void *phy)
//{
//	return meson_u2phy_power_on((struct amlogic_usb_v2 *)phy);
//}

//static int meson_u2phy_c4_power_off(void *phy)
//{
//	return meson_u2phy_power_off((struct amlogic_usb_v2 *)phy);
//}

static struct meson_uphy_ops meson_u2phy_c4_ops = {
	.init = meson_u2phy_c4_init,
	.exit = meson_u2phy_c4_exit,
	.power_on = NULL,
	.power_off = NULL,
	.set_mode = meson_u2phy_c4_set_mode,
};

struct meson_uphy_pdata meson_u2phy_c4_pdata = {
	.u2phy_ops = &meson_u2phy_c4_ops,
	.u3phy_ops = NULL,
	.u2phy_parse = meson_aml_u2phy_parse,
	.u3phy_parse = NULL,
};
