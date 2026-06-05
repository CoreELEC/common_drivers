// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"
#include <linux/clk/clk-conf.h>
#include <linux/amlogic/cpu_version.h>

/* Amlogic usb2 phy. */

static inline bool meson_u2phy_t6x_hsp(struct amlogic_usb_v2 *mphy)
{
	return mphy->sw_hsp && meson_u2phy_960m;
}

static int meson_u2phy_t6x_enable_clock_src(struct amlogic_usb_v2 *mhy)
{
	int ret = 0, rty = 3;

	dev_dbg(mhy->dev, "enable clock num: %d.", mhy->clk_num);

	do {
		ret = clk_bulk_prepare_enable(mhy->clk_num, mhy->clks);
		if (ret)
			dev_err(mhy->dev, "retry enable usb2 phy bus.\n");
		else
			dev_dbg(mhy->dev, "enable usb2 phy bus clk ok.\n");
	} while (ret && rty--);

	if (ret)
		dev_err(mhy->dev, "Failed to enable usb2 phy bus clock %d.\n", ret);

	return ret;
}

static int meson_u2phy_t6x_set_clock_src(struct amlogic_usb_v2 *mphy)
{
	u32 val;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* T6D specific 960M clk source config. */
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS) {
		dev_dbg(mphy->dev, "%s setmode HSP.\n", __func__);
		/* usbpll_reve[0]: configure analog and pll to 960m usb2T mode. */
		val = readl(cfg + 0x44);
		val |= BIT(10);
		writel(val, cfg + 0x44);
		usleep_range(20, 30);
		/* Configure controller to 48M. */
		usleep_range(20, 30);
		val = readl(mphy->regs + 0x84);
		if (mphy->clk_mux == 2) {
			/* Use SoC 48M ref_clk */
			val |= BIT(1);
		} else if (mphy->clk_mux == 1) {
			/* Use phy PLL hs_clk. */
			val |= BIT(0);
		}
		writel(val, mphy->regs + 0x84);
	}

	return 0;
}

static int meson_u2phy_t6x_set_pll(struct amlogic_usb_v2 *mphy)
{
	void __iomem *pll_cfg = mphy->phy_cfg[0] + 0x40;
	void __iomem *pll_cfg_x = mphy->phy_cfg[0] + 0x5c;
	u32 retry = 5;
	int plldone_i;
	u32 val;

	while (retry--) {
		/* assert usb_mppll_rst and usb_lock_rst */
#define USB_MPPLL_RST	BIT(0)
#define USB_LOCK_RST	BIT(1)
		val = readl(pll_cfg);
		val |= USB_MPPLL_RST | USB_LOCK_RST;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
		/* enable usb2_bgr_en off 0x5c */
#define USB2_BGR_EN	BIT(0)
		val = readl(pll_cfg_x);
		val |= USB2_BGR_EN;
		writel(val, pll_cfg_x);
		/* delay 20μs */
		usleep_range(20, 50);
		/* enable usb_mppll_en_ctrl */
#define USB_MPPLL_EN_CTRL	BIT(29)
		val = readl(pll_cfg);
		val |= USB_MPPLL_EN_CTRL;
		writel(val, pll_cfg);
		/* enable usb_mppll_en */
#define USB_MPPLL_EN	BIT(2)
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
		val &= ~USB_LOCK_RST;
		writel(val, pll_cfg);
#define	USBPLL_LOCK_FLAG BIT(31)
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

static int meson_u2phy_t6x_cali_disc_squelch
			(struct amlogic_usb_v2 *mphy)
{
	u32 val = 0;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* usb2_squelch_trim:
	 * usb2.0: reg32_03[3:0] (MSB->LSB) default 4b'0111.
	 * usb2t: reg32_23[28]##reg32_14[31:29] default 4b'0111.
	 * usb2_disc_trim:
	 *	usb2.0 & 2t: reg32_03[6:4] (MSB->LSB) default 3b'110.
	 */
#define DISCONNECT_MASK ((u32)GENMASK(6, 4))
#define DISCONNECT_VAL(x) ((0x7 & (x)) << 4)
	val = readl(cfg + 0xc);
	/* Set to the MAX. */
	val = (val & ~DISCONNECT_MASK) | DISCONNECT_VAL(0x7);
	writel(val, cfg + 0xc);

	/* Trimming for squelch detect threshold. */
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS) {
		val = readl(cfg + 0x38);
		val = (val & ~((u32)GENMASK(31, 29))) | (0x7 << 29);
		writel(val, cfg + 0x38);
		val = readl(cfg + 0x5c);
		val |= (u32)BIT(28);
		writel(val, cfg + 0x5c);
	} else if (mphy->portspeed == MESON_USB_SPEED_HIGH) {
		val = readl(cfg + 0xc);
		val = (val & ~((u32)GENMASK(3, 0))) | (0xf << 0);
		writel(val, cfg + 0xc);
	}

	return 0;
}

static int meson_u2phy_t6x_set_misc(struct amlogic_usb_v2 *mphy)
{
	return 0;
}

static void meson_u2phy_t6x_cali(struct amlogic_usb_v2 *mphy)
{
	meson_usb2phy_set_calibration_trim(mphy);
	meson_u2phy_t6x_cali_disc_squelch(mphy);
	meson_u2phy_t6x_set_misc(mphy);
}

static struct meson_u2phy_priv meson_u2phy_t6x_priv = {
	.set_mode = meson_u2phy_set_mode,
	.cali = meson_u2phy_t6x_cali,
	.set_pll = meson_u2phy_t6x_set_pll,
	.enable_clock_src = meson_u2phy_t6x_enable_clock_src,
	.set_clock_src = meson_u2phy_t6x_set_clock_src,
};

/* FIXME: Don't call meson_u2phy_u3drdu2o_set for setting secondary pipe clk.
 * Set it in usb3 phy, refer to a9 phy driver for how.
 */
static int meson_u2phy_t6x_set_mode(void *phy, enum meson_uphy_mode mode)
{
	int ret;

	if (mode == MESON_USB_MODE_DEVICE) {
		ret = meson_u2phy_u3drdu2o_set((struct amlogic_usb_v2 *)phy, true);
		if (ret)
			return ret;
	}

	return meson_u2phy_t6x_priv.set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_t6x_init(void *phy)
{
	return meson_u2phy_aml_2t_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_t6x_priv);
}

static int meson_u2phy_t6x_exit(void *phy)
{
	int ret;

	ret = meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
	if (ret >= 0) {
		if (((struct amlogic_usb_v2 *)phy)->current_mode == MESON_USB_MODE_DEVICE)
			ret = meson_u2phy_u3drdu2o_set((struct amlogic_usb_v2 *)phy, false);
	}
	return ret;
}

static int meson_u2phy_t6x_power_on(void *phy)
{
	return meson_u2phy_power_on((struct amlogic_usb_v2 *)phy);
}

static int meson_u2phy_t6x_power_off(void *phy)
{
	return meson_u2phy_power_off((struct amlogic_usb_v2 *)phy);
}

static struct meson_uphy_ops meson_u2phy_t6x_ops = {
	.init = meson_u2phy_t6x_init,
	.exit = meson_u2phy_t6x_exit,
	.power_on = meson_u2phy_t6x_power_on,
	.power_off = meson_u2phy_t6x_power_off,
	.set_mode = meson_u2phy_t6x_set_mode,
};

/* The parent of clk_usb2_48m has to be clk_usb2_48m_pre which is base on 1000M.
 * Don't directly set clk rate of clk_usb2_48m to 48M. Otherwise the clock core
 * will automatically set clk_usb2_48m_tmp as parent which is based on 1152M but
 * reserved by others.
 * It is workaround by setting the parent of clk_usb2_48m to clk_usb2_48m_pre then
 * set the clock rate of clk_usb2_48m_pre to 48M. See "usb2_phyc" in mesont6x.dtsi.
 */
int meson_aml_u2phy_t6x_init_clks(struct amlogic_usb_v2 *mphy)
{
	int ret = 0;

	if  (meson_u2phy_t6x_hsp(mphy)) {
		mphy->portspeed = MESON_USB_SPEED_HIGH_PLUS;
		mphy->clk_mux = 0;
		dev_info(mphy->dev, "usb2t_mode %d forces portspeed to %d.",
						meson_u2phy_960m, mphy->portspeed);
	}

	if (mphy->clk_mux != 0 && mphy->portspeed != MESON_USB_SPEED_HIGH_PLUS) {
		dev_err(mphy->dev, "wrong clk-mux=%d in normal HS mode, default to 0.",
						mphy->clk_mux);
		mphy->clk_mux = 0;
	}

	/* Choose 48M soc digital clk src as default.
	 * Note: When use phy PLL hs_clk for USB2T mode, usb clk will be disabled in
	 * suspend mode, software shall enable PLL manually to resume USB!
	 * So it is recommended to use the clock from the SoC by default in USB2T.
	 */
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS && mphy->clk_mux == 0) {
		mphy->clk_mux = 2;
		dev_info(mphy->dev, "clk-mux=0 in HSP mode, default to %d.", mphy->clk_mux);
	}

	dev_info(mphy->dev, "USB2 phy speed: %d, clk mux: %d.\n", mphy->portspeed, mphy->clk_mux);

	switch (mphy->clk_mux) {
	/* Normal HS mode. */
	case 0:
		mphy->clk_num = 1;
		/* T6X usb drd controller still needs 250m clk in usb2.0 device mode. */
		if (mphy->pipe_clk_sel != -1U && mphy->phy_clk_sel_reg_off != -1U) {
			int i = mphy->clk_num;

			while (i--) {
				if (strstr(mphy->clks[i].id, "usb_clk_250m"))
					break;
			}

			if (!i) {
				dev_err(mphy->dev, "no usb_clk_250m.");
				return -ENODEV;
			}
		}
		break;
	/* HSP mode.*/
	/* 48M phy digital clk src. */
	case 1:
		mphy->clk_num = 1;
		break;
	/* 48M soc digital clk src. */
	case 2:
		mphy->clk_num = 3;
		break;
	/* 48M soc analog clk src. */
	case 3:
		dev_err(mphy->dev, "don't support clk mux: %d.\n", mphy->clk_mux);
		return -EINVAL;
	}

	ret = of_clk_set_defaults(mphy->dev->of_node, false);
	if (ret < 0)
		dev_err(mphy->dev, "set assigned clock parents and rates failed %d.\n", ret);

	return ret;
}

int meson_aml_u2phy_t6x_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	int ret;
	struct amlogic_usb_v2 *mphy;

	ret = meson_aml_u2phy_parse(dev, instance);
	if (ret)
		return ret;

	/* meson_uphy is valid now */
	mphy = (struct amlogic_usb_v2 *)instance->meson_uphy;

	if (!mphy)
		return -ENODEV;

	ret = meson_aml_u2phy_t6x_init_clks(mphy);

	return ret;
}

/* Amlogic usb3 phy. */

static void  meson_u3phy_t6x_txrx_cali(struct aml_usb3_phy *phy)
{
#define PI_DOUBLEDGE_SAMPLING_CLK_SELECT BIT(21)
#define PI_CAP_VAL_CTRL ((u32)GENMASK(27, 26))
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x08,
					PI_DOUBLEDGE_SAMPLING_CLK_SELECT,
					PI_DOUBLEDGE_SAMPLING_CLK_SELECT);
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x08,
					PI_CAP_VAL_CTRL, 0x1 << 26);
#define PI_DOUBLEDGE_SAMPLING BIT(6)
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
					PI_DOUBLEDGE_SAMPLING, PI_DOUBLEDGE_SAMPLING);
#define UPCTX_SEL_DRV_PRE_EN BIT(26)
#define UPCTX_SEL_DRV_PRE BIT(11)
	/* Adjust FFE gain varition. */
	/* Force setting for all ltssm states. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xbc,
					UPCTX_SEL_DRV_PRE_EN, UPCTX_SEL_DRV_PRE_EN);
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xc4,
					UPCTX_SEL_DRV_PRE, UPCTX_SEL_DRV_PRE);
	/* For the change in u0. necessary? */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xcc,
			UPCTX_SEL_DRV_PRE, UPCTX_SEL_DRV_PRE);

#define TXDETRX_FIRST_TIMER ((u32)GENMASK(15, 0))
	/* txdetrx_1st_timer -> 5us. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x104,
					TXDETRX_FIRST_TIMER, 0x78);

	/* cdr_ph_div<2:0> Kp -> 0x4. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
			GENMASK(2, 0), 0x4);
	/* cdr_ph_div<5:4> Ki -> 0x2. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
			GENMASK(5, 4), 0x2 << 4);
	/* enable upcrx_eq_byp_val. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x20,
			BIT(9), BIT(9));
	/* upcrx_eq_byp_val set to 0. I.e. eq training start from 0. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x24,
			GENMASK(18, 14), 0x0 << 14);

	/* dfe_en<16> -> 0. */
	meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x14,
			BIT(16), 0);
}

static int meson_u3phy_t6x_pll_init(struct aml_usb3_phy *phy)
{
	if (phy->pll_sw_cfg) {
		/* enable software control pll initial */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x50, 0xff << 24,
						0xff << 24);
		/* assert power_bias_en */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2c, BIT(3), BIT(3));
		/* delay 5μs */
		udelay(5);
		/* assert u3p2pll0_bias_en */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x3c, BIT(0), BIT(0));
		/* delay 40μs */
		usleep_range(40, 100);
		/* assert u3p2pll0_rstn */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x3c, BIT(1), BIT(1));
		/* delay 40μs */
		usleep_range(40, 100);
		/* assert u3p2pll0_lock_rstn */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x3c, BIT(2), BIT(2));
		/* delay 160μs */
		usleep_range(160, 250);
		/* assert u3p2pll1_bias_en */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2c, BIT(0), BIT(0));
		/* delay 50μs */
		usleep_range(50, 100);
		/* assert u3p2pll1_rstn */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2c, BIT(1), BIT(1));
		/* delay 40μs */
		usleep_range(40, 100);
		/* assert u3p2pll1_rstn_lock */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2c, BIT(30), BIT(30));
		/* delay 1μs */
		udelay(1);
		/* assert u3p2pll1_lock_start */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x30, BIT(0), BIT(0));
	} else {
	}

	return 0;
}

static void meson_u3phy_t6x_ssc_set(struct aml_usb3_phy *phy, bool on)
{
	if (on) {
		/* [29]sec_spll_lock force -> 1'b1 && [5,4]secpll lock window->2'b11 */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2C, GENMASK(5, 4) |
			BIT(29), GENMASK(5, 4) | BIT(29));
		/* [23:20] SSC amplitude set to 1*9*500 ppm. [19] ssc_on -> 1. */
		meson_aml_u3phy_write_reg32(phy, 0x51980000, phy->ctrl_reg + 0x34);
		meson_aml_u3phy_write_reg32(phy, 0x10020, phy->ctrl_reg + 0x38);

		/* [25] clk power source 1: Avdd 1.8V, 0:Vddee 0.8V. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x40, BIT(25), BIT(25));
	} else {
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x34, BIT(29), 0);
	}
}

static struct meson_aml_u3phy_priv meson_u3phy_t6x_priv = {
	.txrx_cali = meson_u3phy_t6x_txrx_cali,
	.pll_init = meson_u3phy_t6x_pll_init,
	.ssc_set = meson_u3phy_t6x_ssc_set,
};

static int meson_u3phy_t6x_init(void *phy)
{
	return meson_aml_u3phy_init((struct aml_usb3_phy *)phy);
}

static int meson_u3phy_t6x_exit(void *phy)
{
	return meson_aml_u3phy_exit((struct aml_usb3_phy *)phy);
}

static int meson_aml_u3phy_t6x_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	return meson_aml_u3phy_parse(dev, instance, &meson_u3phy_t6x_priv);
}

static struct meson_uphy_ops meson_u3phy_t6x_ops = {
	.init = meson_u3phy_t6x_init,
	.exit = meson_u3phy_t6x_exit,
};

struct meson_uphy_pdata meson_uphy_t6x_pdata = {
	.u2phy_ops = &meson_u2phy_t6x_ops,
	.u3phy_ops = &meson_u3phy_t6x_ops,
	.u2phy_parse = meson_aml_u2phy_t6x_parse,
	.u3phy_parse = meson_aml_u3phy_t6x_parse,
	.otg_parse = meson_u2phy_crg_otg_parse,
	.otg_remove = meson_u2phy_crg_otg_remove,
};
