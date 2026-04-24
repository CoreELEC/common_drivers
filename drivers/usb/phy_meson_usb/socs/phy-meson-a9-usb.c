// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "../phy-meson-usb.h"
#include <linux/clk/clk-conf.h>

static int meson_u2phy_a9_set_clock_src(struct amlogic_usb_v2 *mphy)
{
	u32 val;
	void __iomem *cfg = mphy->phy_cfg[0];

	/* A9 specific 960M clk source config. The 960M clk is from phy pll, not sysclk.*/
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS) {
		dev_dbg(mphy->dev, "%s setmode HSP.\n", __func__);
		/* usbpll_reve[0]: configure analog and pll to 960m usb2T mode. */
		val = readl(cfg + 0x44);
		val |= BIT(2);
		writel(val, cfg + 0x44);
		usleep_range(20, 30);
	}

	return 0;
}

static int meson_u2phy_a9_set_pll(struct amlogic_usb_v2 *mphy)
{
	void __iomem *pll_cfg = mphy->phy_cfg[0] + 0x40;
	u32 retry = 5;
	int plldone_i;
	u32 val;

	while (retry--) {
		/* assert usb_mppll_rst and usb_lock_rst */
#define USB2_MPLL_EN_CTRL	BIT(27)
		val = readl(pll_cfg);
		val |= USB2_MPLL_EN_CTRL;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
#define USB2_PLL_BIAS_EN	BIT(26)
		val = readl(pll_cfg);
		val |= USB2_PLL_BIAS_EN;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
#define USB2_PLL_RSTN	BIT(25)
		val = readl(pll_cfg);
		val |= USB2_PLL_RSTN;
		writel(val, pll_cfg);
		/* delay 20μs */
		usleep_range(20, 50);
#define USB2_PLL_LOCK_EN	BIT(24)
		val = readl(pll_cfg);
		val |= USB2_PLL_LOCK_EN;
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

static int meson_u2phy_a9_cali_disc_squelch
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
	val = readl(cfg + 0xc);
	/* Set to the MAX. */
	val = (val & ~DISCONNECT_MASK) | DISCONNECT_VAL(0x7);
	writel(val, cfg + 0xc);

	/* Trimming for squelch detect threshold. */
	if (mphy->portspeed == MESON_USB_SPEED_HIGH_PLUS) {
		val = readl(cfg + 0x60);
		val = (val & ~((u32)GENMASK(11, 8))) | (0xf << 0);
		writel(val, cfg + 0x60);
	} else if (mphy->portspeed == MESON_USB_SPEED_HIGH) {
		val = readl(cfg + 0xc);
		val = (val & ~((u32)GENMASK(3, 0))) | (0xf << 0);
		writel(val, cfg + 0xc);
	}

	return 0;
}

static void meson_u2phy_a9_cali(struct amlogic_usb_v2 *mphy)
{
	meson_usb2phy_set_calibration_trim(mphy);
	meson_u2phy_a9_cali_disc_squelch(mphy);
}

static struct meson_u2phy_priv meson_u2phy_a9_priv = {
	.set_mode = meson_u2phy_set_mode,
	.cali = meson_u2phy_a9_cali,
	.set_pll = meson_u2phy_a9_set_pll,
	.set_clock_src = meson_u2phy_a9_set_clock_src,
};

static int meson_u2phy_a9_set_mode(void *phy, enum meson_uphy_mode mode)
{
	u32 val = 0;
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)phy;
	void __iomem *reg = mphy->regs;
#define UTMISRP_BVALID  (BIT(25))

	val = readl(reg + 0x88);
	/* DWC3 ip specific requirements. It indicates that the B-Session is valid
	 * (i.e. VBUS voltage from the host is above the B-device threshold,
	 * typically ~2.0 V or higher per USB OTG/USB 2.0 specs).
	 * TODO: Maybe setting GCTL.MASTERFILTBYPASS in dwc3 also works?
	 */
	if (mode == MESON_USB_MODE_HOST) {
		val &= ~UTMISRP_BVALID;
		writel(val, reg + 0x88);
	} else {
		val |= UTMISRP_BVALID;
		writel(val, reg + 0x88);
	}
	usleep_range(20, 50);

	return meson_u2phy_a9_priv.set_mode((struct amlogic_usb_v2 *)phy, mode);
}

static int meson_u2phy_a9_init(void *phy)
{
	int ret;

	ret = meson_u2phy_aml_2t_init((struct amlogic_usb_v2 *)phy, &meson_u2phy_a9_priv);
	if (ret)
		return ret;

	ret = meson_u2phy_u3drdu2o_set((struct amlogic_usb_v2 *)phy, true);
	return ret;
}

static int meson_u2phy_a9_exit(void *phy)
{
	int ret;

	ret = meson_u2phy_exit((struct amlogic_usb_v2 *)phy);
	if (ret >= 0) {
		if (((struct amlogic_usb_v2 *)phy)->current_mode == MESON_USB_MODE_DEVICE)
			ret = meson_u2phy_u3drdu2o_set((struct amlogic_usb_v2 *)phy, false);
	}
	return ret;
}

static struct meson_uphy_ops meson_u2phy_a9_ops = {
	.init = meson_u2phy_a9_init,
	.exit = meson_u2phy_a9_exit,
	.set_mode = meson_u2phy_a9_set_mode,
};

/* A9 dwc3 controller 2t clk is from doubled phy PLL. When using phy PLL hs_clk for
 * USB2T mode for the corigine controller, usb clk will be disabled in suspend mode,
 * the software shall enable PLL manually to resume USB, but for dwc3 controller,
 * when suspended the clock domain is different thus no such consideration.
 */
static int meson_aml_u2phy_a9_init_clks(struct amlogic_usb_v2 *mphy)
{
	int ret = 0;

	if (mphy->sw_hsp && meson_u2phy_960m) {
		mphy->portspeed = MESON_USB_SPEED_HIGH_PLUS;
		dev_info(mphy->dev, "usb2t_mode %d forces portspeed to %d.",
						meson_u2phy_960m, mphy->portspeed);
	}

	dev_info(mphy->dev, "USB2 phy speed: %d, clk mux: %d.\n", mphy->portspeed, mphy->clk_mux);

	/* usb3 drd controller still needs 250m clk in usb2.0 device mode. */
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

	ret = of_clk_set_defaults(mphy->dev->of_node, false);
	if (ret < 0)
		dev_err(mphy->dev, "set assigned clock parents and rates failed %d.\n", ret);

	return ret;
}

int meson_aml_u2phy_a9_parse(struct device *dev, struct meson_uphy_instance *instance)
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

	ret = meson_aml_u2phy_a9_init_clks(mphy);

	return ret;
}

/* usb3 phy*/

/* RX config from vlsi:
 * reg_dcha_afe 0x4 0xE9943075 -> 0xe994_3075 default ok
 * reg_dcha_dfe 0x8 0x54147E68 -> 0x5414_4268
 * reg_dchd_cdr 0x10 0x240018D3 -> 0x2400_14e4
 * reg_dchd_eq 0x14 0x2011104F -> 0x2011_104f default ok (but better clear dfe_en)
 * reg_misc 0x0 0x00007600 -> 0X0000_7600 default ok
 */
static void  meson_u3phy_a9_txrx_cali(struct aml_usb3_phy *phy)
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
//default set?
//#define PI_DOUBLEDGE_SAMPLING BIT(6)
//		aml_usb3_phy_modify_reg32(phy, phy->ctrl_reg + 0x10,
//						PI_DOUBLEDGE_SAMPLING, PI_DOUBLEDGE_SAMPLING);

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

static void meson_u3phy_a9_ssc_set(struct aml_usb3_phy *phy, bool on)
{
//	if (on) {
		/* [29]sec_spll_lock force -> 1'b1 && [5,4]secpll lock window->2'b11 */
//		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x2C, GENMASK(5, 4) |
//			BIT(29), GENMASK(5, 4) | BIT(29));
		/* [23:20] SSC amplitude set to 1*9*500 ppm. [19] ssc_on -> 1. */
//		meson_aml_u3phy_write_reg32(phy, 0x51980000, phy->ctrl_reg + 0x34);
//		meson_aml_u3phy_write_reg32(phy, 0x10020, phy->ctrl_reg + 0x38);

		/* [25] clk power source 1: Avdd 1.8V, 0:Vddee 0.8V. */
//		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x40, BIT(25), BIT(25));
//	} else {
//		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x34, BIT(29), 0);
//	}
}

int meson_u3phy_secondary_pipe_clk_set(struct aml_usb3_phy *phy, bool on)
{
	int ret = 0;
	void __iomem *phy_clk_sel_reg;
	u32 pipe_clk_sel;

	if (phy->pipe_clk_sel != -1U && phy->phy_clk_sel_reg_off != -1U) {
		pipe_clk_sel = phy->pipe_clk_sel;
		phy_clk_sel_reg = phy->cfg_reg + phy->phy_clk_sel_reg_off;

		if (on) {
			writel(readl(phy_clk_sel_reg) | pipe_clk_sel, phy_clk_sel_reg);
			dev_dbg(phy->dev, "enable secondary usb_pipe_clk\n");
		} else {
			writel(readl(phy_clk_sel_reg) & ~pipe_clk_sel, phy_clk_sel_reg);
			dev_dbg(phy->dev, "disable secondary usb_pipe_clk\n");
		}
	}

	return ret;
}

static struct meson_aml_u3phy_priv meson_u3phy_a9_priv = {
	.txrx_cali = meson_u3phy_a9_txrx_cali,
	.ssc_set = meson_u3phy_a9_ssc_set,
	.secondary_pipe_clk_set = meson_u3phy_secondary_pipe_clk_set,
};

static int meson_u3phy_a9_set_mode(void *phy, enum meson_uphy_mode mode)
{
	struct aml_usb3_phy *mphy = (struct aml_usb3_phy *)phy;

	if (mode == MESON_USB_MODE_HOST) {
		if (mphy->combo_phy && mphy->combo_phy_mux_reg) {
			dev_dbg(mphy->dev, "muxed to usb.");
			meson_aml_u3phy_write_reg32(mphy, 0x2,
					mphy->combo_phy_mux_reg);
		}
	}

	return 0;
}

static int meson_u3phy_a9_init(void *phy)
{
	return meson_aml_u3phy_init((struct aml_usb3_phy *)phy);
}

static int meson_u3phy_a9_exit(void *phy)
{
	return meson_aml_u3phy_exit((struct aml_usb3_phy *)phy);
}

static struct meson_uphy_ops meson_u3phy_a9_ops = {
	.init = meson_u3phy_a9_init,
	.exit = meson_u3phy_a9_exit,
	.set_mode = meson_u3phy_a9_set_mode,
};

static int meson_aml_u3phy_a9_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	int ret;
	struct aml_usb3_phy *mphy;

	ret = meson_aml_u3phy_parse(dev, instance, &meson_u3phy_a9_priv);
	if (ret)
		return ret;

	/* meson_uphy is valid now */
	mphy = (struct aml_usb3_phy *)instance->meson_uphy;

	if (!mphy)
		return -ENODEV;

	ret = of_property_read_u32(dev->of_node, "pipe-clk-sel", &mphy->pipe_clk_sel);
	if (ret < 0)
		mphy->pipe_clk_sel = -1U;
	dev_dbg(dev, "pipe_clk_sel: 0x%x\n", mphy->pipe_clk_sel);

	ret = of_property_read_u32(dev->of_node, "phy-clk-sel-reg-off",
		&mphy->phy_clk_sel_reg_off);
	if (ret < 0)
		mphy->phy_clk_sel_reg_off = -1U;

	dev_dbg(dev, "phy-clk-sel-reg-off: 0x%x\n", mphy->phy_clk_sel_reg_off);

	ret = 0;

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

	ret = of_clk_set_defaults(mphy->dev->of_node, false);
	if (ret < 0)
		dev_err(mphy->dev, "set assigned clock parents and rates failed %d.\n", ret);

	return ret;
}

struct meson_uphy_pdata meson_uphy_a9_pdata = {
	.u2phy_ops = &meson_u2phy_a9_ops,
	.u3phy_ops = &meson_u3phy_a9_ops,
	.u2phy_parse = meson_aml_u2phy_a9_parse,
	.u3phy_parse = meson_aml_u3phy_a9_parse,
};
