// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"

static int meson_usb2phy_s6_set_pll(struct amlogic_usb_v2 *phy)
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

static int meson_usb2phy_s6_cali_disc_squelch
			(struct amlogic_usb_v2 *mphy)
{
/* S6
 * usb2_squelch_trim: reg32_03[3:0] (MSB->LSB) default 0b0111
 * usb2_disc_trim: reg32_03[6:4] (MSB->LSB) default 0b000
 */
#define PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0 0x39
#define USB2_REG_CFG_DIS	27

	/* The s7d default value 0b000 of usb2_disc_trim leads to hs handshake err.
	 * S6 shares the params with s7d.
	 */
	writel(PHY_CRG_DRD_TUNING_DISCONNECT_THRESHOLD_BIT6_0, mphy->phy_cfg[0] + 0xC);
	/* The USB2_REG_CFG_DIS is not used but default set. Clear it.*/
	writel(readl(mphy->phy_cfg[0] + 0x38) & ~BIT(USB2_REG_CFG_DIS),
			mphy->phy_cfg[0] + 0x38);

	return 0;
}

static int meson_usb2phy_s6_set_misc(struct amlogic_usb_v2 *mphy)
{
	/* edgedrv cali for signal quality. */
	writel(mphy->pll_setting[3], mphy->phy_cfg[0] + 0x50);

	return 0;
}

static void meson_usb2phy_s6_cali(struct amlogic_usb_v2 *phy)
{
	meson_usb2phy_set_calibration_trim(phy);
	meson_usb2phy_s6_cali_disc_squelch(phy);
	meson_usb2phy_s6_set_misc(phy);
}

static struct meson_u2phy_priv meson_u2phy_s6_priv = {
	.set_mode = meson_u2phy_set_mode,
	.cali = meson_usb2phy_s6_cali,
	.set_pll = meson_usb2phy_s6_set_pll,
};

static int meson_u2phy_s6_set_mode(void *phy, enum meson_uphy_mode mode)
{
	return meson_u2phy_set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_s6_init(void *phy)
{
	return meson_u2phy_aml_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_s6_priv);
}

static int meson_u2phy_s6_exit(void *phy)
{
	return meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
}

static int meson_u2phy_s6_power_on(void *phy)
{
	return meson_u2phy_power_on((struct amlogic_usb_v2 *)phy);
}

static int meson_u2phy_s6_power_off(void *phy)
{
	return meson_u2phy_power_off((struct amlogic_usb_v2 *)phy);
}

static struct meson_uphy_ops meson_u2phy_s6_ops = {
	.init = meson_u2phy_s6_init,
	.exit = meson_u2phy_s6_exit,
	.power_on = meson_u2phy_s6_power_on,
	.power_off = meson_u2phy_s6_power_off,
	.set_mode = meson_u2phy_s6_set_mode,
};

static bool meson_u3phy_s6_wait_pll_locked(struct aml_usb3_phy *phy)
{
	bool ret = false;
	int plldone_i;
	u32 pll_locked_mask = 0;
#define U3P2PLL1_PLL_LOCK_FLAG	24
#define U3P2PLL0_PLL_DONE 25
#define PLL_STAT_REG_OFF	0X154

	pll_locked_mask = BIT(U3P2PLL1_PLL_LOCK_FLAG) | BIT(U3P2PLL0_PLL_DONE);
	for (plldone_i = 100; plldone_i > 0; plldone_i--) {
		usleep_range(100, 200);
		if ((readl(phy->ctrl_reg + PLL_STAT_REG_OFF) & pll_locked_mask)
				== pll_locked_mask) {
			ret = true;
			break;
		}
	}

	if (!ret)
		dev_warn(phy->dev, "0x%x PLL init failed. Checkout timeout val?\n",
				readl(phy->ctrl_reg + PLL_STAT_REG_OFF));
	else
		dev_dbg(phy->dev, "0x%x PLL init done.\n",
			readl(phy->ctrl_reg + PLL_STAT_REG_OFF));

	return ret;
}

static void  meson_u3phy_s6_set_power(struct aml_usb3_phy *phy, bool on)
{
	/* The vbus power is controlled by the companion usb2 phy. */
	if (on) {
		/* Make sure reset is not held. */
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
				BIT(phy->usb3_apb_reset_bit) | BIT(phy->usb3_phy_reset_bit),
				BIT(phy->usb3_apb_reset_bit) | BIT(phy->usb3_phy_reset_bit));
	} else {
		/* Hold reset to power off. */
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
				BIT(phy->usb3_apb_reset_bit) | BIT(phy->usb3_phy_reset_bit), 0);

		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
					BIT(phy->usb3_apb_reset_bit),
					BIT(phy->usb3_apb_reset_bit));
		dev_dbg(phy->dev, "s6 set power quirks.");
		/*  The ctrl reg default value is power consuming. */
		meson_aml_u3phy_write_reg32(phy, 0x71, phy->ctrl_reg + 0x4);
		meson_aml_u3phy_write_reg32(phy, 0x0, phy->ctrl_reg + 0x8);
		meson_aml_u3phy_write_reg32(phy, 0x0, phy->ctrl_reg + 0x10);
		meson_aml_u3phy_write_reg32(phy, 0x0, phy->ctrl_reg + 0x14);
		usleep_range(100, 200);
	}
}

static void  meson_u3phy_s6_trim(struct aml_usb3_phy *phy)
{
	u32 raw = meson_aml_u3phy_read_reg32(phy, phy->trim_reg);
	u32 val = 0;

	/* tx termination impedance. */
	if (raw & BIT(5))
		val = raw & 0x1f;
	else
		val = 0x10;

	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x16c, 0xff << 24, val << 24);

	/* rx termination impedance. */
	if (raw & BIT(10))
		val = (raw & 0x3c0) >> 6;
	else
		val = 0x7;

	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x4, 0xf << 4, val << 4);
}

static void  meson_u3phy_s6_txrx_cali(struct aml_usb3_phy *phy)
{
	/* rx cali.*/
	meson_aml_u3phy_write_reg32(phy, 0x240014A4, phy->ctrl_reg + 0x10);

	/* tx cali.*/
	meson_aml_u3phy_write_reg32(phy, 0x003154DA, phy->ctrl_reg + 0xcc);
	meson_aml_u3phy_write_reg32(phy, 0x003154DA, phy->ctrl_reg + 0xf0);
	meson_aml_u3phy_write_reg32(phy, 0x00F154DA, phy->ctrl_reg + 0xfc);
	#define TXDETRX_FIRST_TIMER ((u32)GENMASK(15, 0))
	/* txdetrx_1st_timer -> 5us. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x104,
					TXDETRX_FIRST_TIMER, 0x78);
}

static int meson_u3phy_s6_pll_init(struct aml_usb3_phy *phy)
{
	int plldone_i;

#define U3P2PLL0_BIAS_EN 9
#define U3P2PLL0_RSTN 10
#define U3P2PLL0_LOCK_EN 11
#define U3P2PLL_HCSL_LDO_EN_TM 16
#define U3P2PLL_HCSL_DREN0_TM 17
#define U3P2PLL1_BIAS_EN 20
#define U3P2PLL1_RSTN 21
#define U3P2PLL1_RSTN_LOCK 22
#define U3P2PLL1_LOCK_START 23
#define U3P2PLL1_PLL_LOCK_FLAG  24
#define U3P2PLL0_PLL_DONE 25
#define PLL_LOCKED_MASK (BIT(U3P2PLL1_PLL_LOCK_FLAG) | BIT(U3P2PLL0_PLL_DONE))
#define IS_PLL_LOCKED(x) (((x) & PLL_LOCKED_MASK) == PLL_LOCKED_MASK)
	if (phy->pll_sw_cfg) {
		/* The speed-drop usb devices TX maybe unstable at insertion,
		 * leading to CDR KI overload. Delay freq tracking start point
		 * by modifying fr_en delay 1us->300us.
		 * Training timer change fr_en delay 1us->300us (unit: 41.668ns).
		 */
		meson_aml_u3phy_write_reg32(phy, 0x1C20, phy->ctrl_reg + 0x9C);
		/* Enable sw configure upcrx_igen_en & upcrx_ldo_en. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x20,
								0x3 << 14, 0x3 << 14);
		/* Enable upcrx_igen_en & upcrx_ldo_en. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x28, 0x3, 0x3);
		/* Enable sw configure pll. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x50, 0x7f << 25,
									0x7f << 25);
		/* Configure pll_cfg 0~5. */
		meson_aml_u3phy_write_reg32(phy, 0x00FA114D, phy->ctrl_reg + 0x2C);
		meson_aml_u3phy_write_reg32(phy, 0x72007820, phy->ctrl_reg + 0x30);
		meson_aml_u3phy_write_reg32(phy, 0xA0900000, phy->ctrl_reg + 0x34);
		meson_aml_u3phy_write_reg32(phy, 0x081C1C23, phy->ctrl_reg + 0x38);
		meson_aml_u3phy_write_reg32(phy, 0x1435DC92, phy->ctrl_reg + 0x3C);
		meson_aml_u3phy_write_reg32(phy, 0x00000000, phy->ctrl_reg + 0x40);

		/* Configure pll_cfg 6. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL0_BIAS_EN),
						BIT(U3P2PLL0_BIAS_EN));
		udelay(5);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL0_RSTN), BIT(U3P2PLL0_RSTN));
		usleep_range(40, 140);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL0_LOCK_EN),
						BIT(U3P2PLL0_LOCK_EN));
		usleep_range(160, 260);
		/* Off unused usb3phy digital 100M clk: U3P2PLL_HCSL_LDO_EN_TM -> 0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
					BIT(U3P2PLL_HCSL_LDO_EN_TM) |
					BIT(U3P2PLL_HCSL_DREN0_TM),
					0);
		usleep_range(20, 120);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL1_BIAS_EN),
						BIT(U3P2PLL1_BIAS_EN));
		usleep_range(50, 150);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL1_RSTN),
						BIT(U3P2PLL1_RSTN));
		usleep_range(40, 140);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL1_RSTN_LOCK),
						BIT(U3P2PLL1_RSTN_LOCK));
		udelay(1);
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
						BIT(U3P2PLL1_LOCK_START),
						BIT(U3P2PLL1_LOCK_START));
	} else {
		/* Training timer change fr_en delay 1us->300us (unit: 41.668ns). */
		meson_aml_u3phy_write_reg32(phy, 0x1C20, phy->ctrl_reg + 0x9C);
		/* Enable sw configure upcrx_igen_en & upcrx_ldo_en. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x20, 0x3 << 14,
								0x3 << 14);
		/* Enable upcrx_igen_en & upcrx_ldo_en. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x28, 0x3, 0x3);
		/* Configure pll_cfg 0~5. */
		meson_aml_u3phy_write_reg32(phy, 0x00FA114D, phy->ctrl_reg + 0x2C);
		meson_aml_u3phy_write_reg32(phy, 0x72007820, phy->ctrl_reg + 0x30);
		meson_aml_u3phy_write_reg32(phy, 0xA0900000, phy->ctrl_reg + 0x34);
		meson_aml_u3phy_write_reg32(phy, 0x081C1C23, phy->ctrl_reg + 0x38);
		meson_aml_u3phy_write_reg32(phy, 0x1435DC92, phy->ctrl_reg + 0x3C);
		meson_aml_u3phy_write_reg32(phy, 0x00000000, phy->ctrl_reg + 0x40);
		/* Off unused usb3phy digital 100M clk: U3P2PLL_HCSL_LDO_EN_TM -> 0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x44,
					BIT(U3P2PLL_HCSL_LDO_EN_TM) |
					BIT(U3P2PLL_HCSL_DREN0_TM),
					0);
	}

	for (plldone_i = 5; plldone_i > 0; plldone_i--) {
		usleep_range(20, 120);
		if (IS_PLL_LOCKED(readl(phy->ctrl_reg + 0x154)))
			goto done;
	}

	dev_err(phy->dev, "usb3 pll not locked. val: 0x%08x\n",
						readl(phy->ctrl_reg + 0x154));
	return -1;

done:
	return 0;
}

static struct meson_aml_u3phy_priv meson_u3phy_s6_priv = {
	.set_power = meson_u3phy_s6_set_power,
	.trim = meson_u3phy_s6_trim,
	.txrx_cali = meson_u3phy_s6_txrx_cali,
	.pll_init = meson_u3phy_s6_pll_init,
	.wait_pll_locked = meson_u3phy_s6_wait_pll_locked,
};

static int meson_u3phy_s6_init(void *phy)
{
	return meson_aml_u3phy_init((struct aml_usb3_phy *)phy);
}

static int meson_u3phy_s6_exit(void *phy)
{
	return meson_aml_u3phy_exit((struct aml_usb3_phy *)phy);
}

static int meson_aml_u3phy_s6_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	return meson_aml_u3phy_parse(dev, instance, &meson_u3phy_s6_priv);
}

static struct meson_uphy_ops meson_u3phy_s6_ops = {
	.init = meson_u3phy_s6_init,
	.exit = meson_u3phy_s6_exit,
};

struct meson_uphy_pdata meson_uphy_s6_pdata = {
	.u2phy_ops = &meson_u2phy_s6_ops,
	.u3phy_ops = &meson_u3phy_s6_ops,
	.u2phy_parse = meson_aml_u2phy_parse,
	.u3phy_parse = meson_aml_u3phy_s6_parse,
	.otg_parse = meson_u2phy_crg_otg_parse,
	.otg_remove = meson_u2phy_crg_otg_remove,
};
