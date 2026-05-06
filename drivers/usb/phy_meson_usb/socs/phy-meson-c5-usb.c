// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"

static int meson_usb2phy_c5_set_pll(struct amlogic_usb_v2 *phy)
{
#define USB2_MPPLL_EN_CTRL_BIT	27
#define USBPLL_BIAS_EN_BIT	26
#define USB2_PLL_RSTN_BIT	25
#define USB2_PLL_LOCK_EN_BIT	24
#define USB2_PLL_DONE		31
	u32 retry = 5;
	int plldone_i;
	u32 pll_val;
	/* default value
	 * USB2_MPPLL_EN_CTRL	0
	 * USBPLL_BIAS_EN	0
	 * USB2_PLL_RSTN	0
	 * USB2_PLL_LOCK_EN	0
	 */
	u32 def_val = 0x549540;
	void __iomem *pll_cfg = phy->phy_cfg[0] + 0x40;

	pll_val = readl(pll_cfg);
	/*Enable PLL Control*/
	pll_val |= BIT(USB2_MPPLL_EN_CTRL_BIT);
	writel(pll_val, pll_cfg);
	usleep_range(10, 20);

	while (retry--) {
		/* Reset Register value */
		pll_val = def_val | BIT(USB2_MPPLL_EN_CTRL_BIT);
		writel(pll_val, pll_cfg);
		usleep_range(10, 20);
		/* assert usb_pll_bias_en */
		pll_val |= BIT(USBPLL_BIAS_EN_BIT);
		writel(pll_val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 30);
		/* assert usb_pll_rstn */
		pll_val |= BIT(USB2_PLL_RSTN_BIT);
		writel(pll_val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 30);
		/* assert usb_pll_lock_en */
		pll_val |= BIT(USB2_PLL_LOCK_EN_BIT);
		writel(pll_val, pll_cfg);
		/* check lock bit */
		for (plldone_i = 5; plldone_i > 0; plldone_i--) {
			usleep_range(20, 30);
			if (readl(pll_cfg) & BIT(USB2_PLL_DONE))
				goto okay;
		}
		dev_dbg(phy->dev, "usb2 pll not locked, retry. val: 0x%08x\n", readl(pll_cfg));
	}
	dev_err(phy->dev, "%s set pll failed, exit, val:0x%08x.\n", __func__, readl(pll_cfg));
	return -ETIMEDOUT;
okay:
	dev_dbg(phy->dev, "usb2 pll init done, val: 0x%08x\n", readl(pll_cfg));
	return 0;
}

static int meson_usb2phy_c5_cali_disc_squelch
			(struct amlogic_usb_v2 *mphy)
{
	u32 val = 0;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* usb2_squelch_trim:
	 * usb2.0: reg32_03[3:0] (MSB->LSB) default 4'b0110.
	 * usb2t: reg32_24[11:8] default 4b'0101.
	 * usb2_disc_trim:
	 *	usb2.0 & 2t: reg32_03[6:4] (MSB->LSB) default 3b'110.
	 */
#define DISCONNECT_MASK ((u32)GENMASK(6, 4))
#define DISCONNECT_VAL(x) ((0x7 & (x)) << 4)
#define USB2_REG_CFG_DIS	27

	val = readl(cfg + 0xc);
	/* Set to the MAX. */
	val = (val & ~DISCONNECT_MASK) | DISCONNECT_VAL(0x3);
	writel(val, cfg + 0xc);

	/* Trimming for squelch detect threshold. */
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS) {
		val = readl(cfg + 0x60);
		val = (val & ~((u32)GENMASK(11, 8))) | (0xf << 0);
		writel(val, cfg + 0x60);
	} else if (mphy->portspeed == MESON_USB_SPEED_HIGH) {
		val = readl(cfg + 0xc);
		val = (val & ~((u32)GENMASK(3, 0))) | (0x9 << 0);
		writel(val, cfg + 0xc);
	}
	writel(readl(cfg + 0x38) & ~BIT(USB2_REG_CFG_DIS),
			cfg + 0x38);

	return 0;
}

static int meson_usb2phy_c5_set_misc(struct amlogic_usb_v2 *mphy)
{
	/* edgedrv cali for signal quality. */
	writel(mphy->pll_setting[3], mphy->phy_cfg[0] + 0x50);

	return 0;
}

static int meson_usb2phy_c5_set_mode
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

static void meson_usb2phy_c5_cali(struct amlogic_usb_v2 *phy)
{
	meson_usb2phy_set_calibration_trim(phy);
	meson_usb2phy_c5_cali_disc_squelch(phy);
	meson_usb2phy_c5_set_misc(phy);
}

static struct meson_u2phy_priv meson_u2phy_c5_priv = {
	.set_mode = meson_u2phy_set_mode,
	.cali = meson_usb2phy_c5_cali,
	.set_pll = meson_usb2phy_c5_set_pll,
};

static int meson_u2phy_c5_set_mode(void *phy, enum meson_uphy_mode mode)
{
	meson_usb2phy_c5_set_mode((struct amlogic_usb_v2 *)phy, mode);
	return meson_u2phy_set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_c5_init(void *phy)
{
	return meson_u2phy_aml_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_c5_priv);
}

static int meson_u2phy_c5_exit(void *phy)
{
	return meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
}

static int meson_u2phy_c5_power_on(void *phy)
{
	return meson_u2phy_power_on((struct amlogic_usb_v2 *)phy);
}

static int meson_u2phy_c5_power_off(void *phy)
{
	return meson_u2phy_power_off((struct amlogic_usb_v2 *)phy);
}

static struct meson_uphy_ops meson_u2phy_c5_ops = {
	.init = meson_u2phy_c5_init,
	.exit = meson_u2phy_c5_exit,
	.power_on = meson_u2phy_c5_power_on,
	.power_off = meson_u2phy_c5_power_off,
	.set_mode = meson_u2phy_c5_set_mode,
};

static void  meson_u3phy_c5_txrx_cali(struct aml_usb3_phy *phy)
{
		/* RX */
	/* reg_dcha_dfe<13:9> dfe_tap_en -> 0x10*/
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x08,
					GENMASK(13, 9), 0x1 << 9);
	//default set
	//#define PI_DOUBLEDGE_SAMPLING_CLK_SELECT BIT(20)
	//#define PI_CAP_VAL_CTRL ((u32)GENMASK(27, 26))
	//		aml_usb3_phy_modify_reg32(phy, phy->ctrl_reg + 0x08,
	//						PI_DOUBLEDGE_SAMPLING_CLK_SELECT,
	//						PI_DOUBLEDGE_SAMPLING_CLK_SELECT);
	//		aml_usb3_phy_modify_reg32(phy, phy->ctrl_reg + 0x08,
	//						PI_CAP_VAL_CTRL, 0x1 << 26);

	/* reg_dchd_cdr<2:0> cdr_ph_div Kp -> 0x4.*/
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
			GENMASK(2, 0), 0x4);
	/* reg_dchd_cdr<5:4> cdr_fr_div Ki -> 0x2.*/
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
			GENMASK(5, 4), 0x2 << 4);
	/* reg_dchd_cdr<11:10> cdr_pi_div -> 0x1.*/
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
			GENMASK(11, 10), 0x1 << 10);

	/* reg_dchd_eq<16> dfe_en -> 0. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x14,
			BIT(16), 0);

	/* rx_cfg8<9> upcrx_eq_byp_val enable. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x20,
			BIT(9), BIT(9));
	/* rx_cfg9<18:14> upcrx_eq_byp_val. Set to 0. I.e. eq training start from 0. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x24,
			GENMASK(18, 14), 0x0 << 14);

	/* TX */
	/* u3 upctx_cfg1<5>upctx_sel_dc_mode:TX dc output enable. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xe4,
			BIT(3), BIT(3));
	/* upctx_cfg2<4>upctx_low_amp_en: TX output swing amplitude control. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xe0,
			BIT(29), BIT(29));
}

static struct meson_aml_u3phy_priv meson_u3phy_c5_priv = {
	.trim = NULL,
	.txrx_cali = meson_u3phy_c5_txrx_cali,
};

static int meson_u3phy_c5_init(void *phy)
{
	return meson_aml_u3phy_init((struct aml_usb3_phy *)phy);
}

static int meson_u3phy_c5_exit(void *phy)
{
	return meson_aml_u3phy_exit((struct aml_usb3_phy *)phy);
}

static int meson_aml_u3phy_c5_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	return meson_aml_u3phy_parse(dev, instance, &meson_u3phy_c5_priv);
}

static struct meson_uphy_ops meson_u3phy_c5_ops = {
	.init = meson_u3phy_c5_init,
	.exit = meson_u3phy_c5_exit,
};

struct meson_uphy_pdata meson_uphy_c5_pdata = {
	.u2phy_ops = &meson_u2phy_c5_ops,
	.u3phy_ops = &meson_u3phy_c5_ops,
	.u2phy_parse = meson_aml_u2phy_parse,
	.u3phy_parse = meson_aml_u3phy_c5_parse,
};
