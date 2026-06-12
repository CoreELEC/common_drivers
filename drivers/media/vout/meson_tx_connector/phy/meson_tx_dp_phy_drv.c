// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/usb/typec.h>
#include <linux/amlogic/typec_mux_meson.h>
#include <linux/amlogic/media/vout/meson_tx_connector/phy/meson_tx_phy_core.h>
#include <linux/amlogic/efuse.h>
#include <linux/amlogic/clk_measure.h>
#include "meson_tx_dp_phy_register.h"

#define DPPHY_PREFIX "[dptx] phy "
#define DEVICE_NAME "tx_phy"

static inline void dptx_phy_notify_hpd_plugin(struct meson_tx_phy *tx_phy);
static inline void dptx_phy_notify_hpd_plugout(struct meson_tx_phy *tx_phy);
static inline void dptx_phy_notify_hpd_irq(struct meson_tx_phy *tx_phy);
static inline void dptx_phy_notify_lane_remap(struct meson_tx_phy *tx_phy);
static const char *typec_dp_get_devices_connect_st(struct meson_tx_phy *tx_phy);
static void dptx_phy_set_prem_vswing(void *data, u32 reg, u32 lane, u32 vswing, u32 pre);

static void dptx_phy_usbdp_write(struct meson_tx_phy *tx_phy, u32 offset, u32 val)
{
	int ret;

	ret = regmap_write(tx_phy->regmap_dpphy_ctrl, offset, val);

	if (ret < 0)
		dev_err(&tx_phy->pdev->dev, "failed to write dpphy_ctrl 0x%x\n", offset);
}

static u32 dptx_phy_usbdp_read(struct meson_tx_phy *tx_phy, u32 offset)
{
	int ret;
	u32 value = 0;

	ret = regmap_read(tx_phy->regmap_dpphy_ctrl, offset, &value);

	if (ret < 0)
		dev_err(&tx_phy->pdev->dev, "failed to read dpphy_ctrl 0x%x\n", offset);

	return value;
}

static void dptx_phy_usbdp_setbits(struct meson_tx_phy *tx_phy, u32 offset, u32 mask, u32 value)
{
	int ret;

	ret = regmap_update_bits(tx_phy->regmap_dpphy_ctrl, offset, mask, value);

	if (ret < 0)
		dev_err(&tx_phy->pdev->dev, "failed to update_bits dpphy_ctrl 0x%x\n", offset);
}

static void dptx_phy_ana_write(struct meson_tx_phy *tx_phy, u32 offset, u32 val)
{
	int ret;

	ret = regmap_write(tx_phy->regmap_anactrl_typec_dp, offset, val);

	if (ret < 0)
		dev_err(&tx_phy->pdev->dev, "failed to write anactrl_typec_dp 0x%x\n", offset);
}

static u32 dptx_phy_ana_read(struct meson_tx_phy *tx_phy, u32 offset)
{
	int ret;
	u32 value = 0;

	ret = regmap_read(tx_phy->regmap_anactrl_typec_dp, offset, &value);

	if (ret < 0)
		dev_err(&tx_phy->pdev->dev, "failed to read anactrl_typec_dp 0x%x\n", offset);
	return value;
}

/*
 * please refer to phy register documents, and get the related efuse control
 */
enum dptx_trim_item {
	DPTX_TRIM_EFUSE_AUXTX,
	DPTX_TRIM_EFUSE_TX_IO_7_5,
	DPTX_TRIM_EFUSE_TX_IO_10_8,
	DPTX_TRIM_EFUSE_TX_BOOTST,
	DPTX_TRIM_EFUSE_DCHA,
	DPTX_TRIM_MAX,
};

struct dptx_trim_s {
	enum dptx_trim_item item;
	u32 efuse_id;
	u32 valid_bits;
	u32 start_bit;
	u32 default_val;
};

static u32 get_trim_value(struct dptx_trim_s *trim)
{
	int ret;

	if (!trim)
		return 0;

	ret = efuse_amlogic_cali_item_read(trim->efuse_id);
	if (ret < 0)
		return trim->default_val;

	return (ret >> trim->start_bit) & ((1 << trim->valid_bits) - 1);
}

static u32 dptx_get_efuse_trim(enum dptx_trim_item item)
{
	int i;
	/*
	 * if get the efuse failed from efuse_amlogic_cali_item_read(), then use the default value
	 */
	struct dptx_trim_s trim_lists[DPTX_TRIM_MAX] = {
		{DPTX_TRIM_EFUSE_AUXTX, EFUSE_CALI_SUBITEM_DPAUX_TX, 3, 0, 0x4},
		{DPTX_TRIM_EFUSE_TX_IO_7_5, EFUSE_CALI_SUBITEM_UPCDP_TX_IO, 3, 0, 0x2},
		{DPTX_TRIM_EFUSE_TX_IO_10_8, EFUSE_CALI_SUBITEM_UPCDP_TX_IO, 3, 3, 0x2},
		{DPTX_TRIM_EFUSE_TX_BOOTST, EFUSE_CALI_SUBITEM_UPCDP_TX_BOOTST, 3, 0, 0x3},
		{DPTX_TRIM_EFUSE_DCHA, EFUSE_CALI_SUBITEM_UPCDP_REG_DCHA, 4, 0, 0x7},
	};

	for (i = 0; i < ARRAY_SIZE(trim_lists); i++) {
		if (item == trim_lists[i].item)
			break;
	}

	if (i >= DPTX_TRIM_MAX)
		return 0;

	return get_trim_value(&trim_lists[i]);
}

static bool typec_dp_get_hpd_plug_state(struct meson_tx_phy *tx_phy)
{
	return !!(tx_phy->usb_dp_state.dp_state_data.status & BIT(7));
}

static bool typec_dp_get_hpd_irq_state(struct meson_tx_phy *tx_phy)
{
	return !!(tx_phy->usb_dp_state.dp_state_data.status & BIT(8));
}

static int meson_tx_dp_phy_init(void *data)
{
	return 0;
}

static void dptx_phy_disable(void *data)
{
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)data;

	/*
	 * Below two bits will be set when phy disables
	 * [5] upctx_sel_dc_mode: TX dc output enable
	 * [20] upctx_low_amp_en: TX output swing amplitude contrl
	 */
	pr_info(DPPHY_PREFIX "disable phy\n");
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE0_CFG0, BIT(5) | BIT(20));
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE1_CFG0, BIT(5) | BIT(20));
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE2_CFG0, BIT(5) | BIT(20));
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE3_CFG0, BIT(5) | BIT(20));

	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE0_CFG1, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE1_CFG1, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE2_CFG1, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE3_CFG1, 0);
	dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, 0);
	dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL1, 0);
	dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL0, 0);
	dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL1, 0);
	dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, 0);
}

static int meson_tx_dp_phy_exit(void *data)
{
	dptx_phy_disable(data);
	return 0;
}

static int meson_tx_dp_phy_power_on(void *data)
{
	return 0;
}

static int meson_tx_dp_phy_power_off(void *data)
{
	dptx_phy_disable(data);
	return 0;
}

/*
 * please refer to upctx_efuse<10:0> docs
 */
static u32 dptx_get_upctx_efuse(struct meson_tx_phy *tx_phy)
{
	u32 data32 = 0;

	data32 |= (dptx_get_efuse_trim(DPTX_TRIM_EFUSE_TX_BOOTST) << 0);
	data32 |= (dptx_get_efuse_trim(DPTX_TRIM_EFUSE_TX_IO_7_5) << 5);
	data32 |= (dptx_get_efuse_trim(DPTX_TRIM_EFUSE_TX_IO_10_8) << 8);

	return data32;
}

/* orient: 0 normal,  1 reverse */
static void dptx_aux_phy_output_enable(struct meson_tx_phy *tx_phy, bool orient, bool en)
{
	u32 val = 0x2481;

	val &= ~(0x7 << 7);
	val |= dptx_get_efuse_trim(DPTX_TRIM_EFUSE_AUXTX) << 7;
	if (orient)
		val |= BIT(2) | BIT(1);
	dptx_phy_usbdp_write(tx_phy, DP_RG_AUX, en ? val : 0);
}

/* 0: gpio output low;  1: gpio output high;  2: gpio output hi-z */
static int meson_tx_dp_phy_set_aux_gpio(void *data, u32 gpio1, u32 gpio2)
{
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)data;

	pr_info(DPPHY_PREFIX "aux gpio1 %d gpio2 %d\n", gpio1, gpio2);
	switch (gpio1) {
	case 0 ... 1:
		gpiod_direction_output(tx_phy->aux_gpio[0], gpio1);
		break;
	default:
		gpiod_direction_input(tx_phy->aux_gpio[0]);
	}
	switch (gpio2) {
	case 0 ... 1:
		gpiod_direction_output(tx_phy->aux_gpio[1], gpio2);
		break;
	default:
		gpiod_direction_input(tx_phy->aux_gpio[1]);
	}
	return 0;
}

static void dptx_phy_usbdp_waitplllock(struct meson_tx_phy *tx_phy, u32 st_reg, u32 lock_bit)
{
	unsigned long lock_timeout = jiffies + msecs_to_jiffies(100);
	u32 lock_st;

	while (time_before(jiffies, lock_timeout)) {
		lock_st = dptx_phy_usbdp_read(tx_phy, USBCDP_PLL_REG0) & lock_bit;
		if (lock_st)
			return;
	}
	pr_info(DPPHY_PREFIX "pll %#x[%d] lock timeout\n", st_reg, __ffs(lock_bit));
}

static void bypass_phy_to_normal_path(struct meson_tx_phy *tx_phy)
{
	int i;
	u32 phy_val;

	for (i = 0; i < 8; i++) {
		phy_val = dptx_phy_usbdp_read(tx_phy, BYPASS_LANE0_CFG0 + i * 4);
		dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE0_CFG0 + i * 4, phy_val);
	}
}

static int dptx_phy_clk_sel = -1;
module_param(dptx_phy_clk_sel, int, 0644);
MODULE_PARM_DESC(dptx_phy_clk_sel, "dptx phy clock selection(0~3)");

static int meson_tx_dp_phy_set_mode(void *data, enum meson_tx_phy_mode mode,
	struct meson_tx_phy_cfg_opts *opts)
{
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)data;
	u32 div2 = 0;
	u32 pll_dp_m    = 0;
	u32 pll_dp_frac = 0;
	u32 pll1_0_en   = 0;
	u32 pll1_0_m    = 0;
	u32 pll1_0_frac = 0;
	u32 pll1_1_en   = 0;
	u32 pll1_1_m    = 0;
	u32 pll1_1_frac = 0;
	u32 alt_mux_dec = 0;
	u32 u3dp_sel    = 2;
	u32 flip = 0;
	u32 data32 = 0;
	// CE: DP only,   E: dp + usb   0/1: dp only; 2: dp 2lane
	u32 alt_mux = 0;

	if (!data || mode != PHY_DP || !opts)
		return -1;

	data32 = 0x34ff;
	if (tx_phy->usb_dp_state.flip == TYPEC_ORIENTATION_REVERSE)
		data32 = 0x13ff;
	switch (dptx_phy_clk_sel) {
	case 0:
		data32 = 0x13ff;
		break;
	case 1:
		data32 = 0x11ff;
		break;
	case 2:
		data32 = 0x34ff;
		break;
	case 3:
		data32 = 0x3cff;
		break;
	default:
		break;
	}
	/* TODO, below should be controlled by USB PD and need remove later */
	dptx_phy_ana_write(tx_phy, ANACTRL_USBCDP_CTRL, 0x138);
	dptx_phy_usbdp_write(tx_phy, DATA_MUX_CFG0, data32);
	dptx_phy_usbdp_write(tx_phy, DATA_MUX_CFG1, 0x5000);

	pr_info(DPPHY_PREFIX "alt_mux %d linkrate %d lanecount %d\n",
		alt_mux, opts->dp_ops.link_rate, opts->dp_ops.lanes);
	if (!opts->dp_ops.link_rate || !opts->dp_ops.lanes) {
		pr_info(DPPHY_PREFIX "need check parameters, use fixed lc=1 lr=6\n");
		opts->dp_ops.link_rate = 1620;
		opts->dp_ops.lanes = 1;
	}
	if (tx_phy->usb_dp_state.flip == TYPEC_ORIENTATION_REVERSE)
		flip = 1;
	/* below value needs fine tune */
	if (tx_phy->usb_dp_state.pin_state == TYPEC_DP_STATE_E)
		alt_mux = 2;
	if (tx_phy->usb_dp_state.pin_state == TYPEC_DP_STATE_D)
		alt_mux = 1;
	if (tx_phy->usb_dp_state.pin_state == TYPEC_DP_STATE_C)
		alt_mux = 3;
	/* PLL for A9 */
	/* bit[15] is div 2 */
	switch (opts->dp_ops.link_rate) {
	case 1620:
		pll_dp_m    = 32;
		pll_dp_frac = 0;
		div2        = BIT(15);
		break;
	case 2700:
		pll_dp_m    = 54;
		pll_dp_frac = 0;
		div2        = BIT(15);
		break;
	case 5400:
		pll_dp_m    = 54;
		pll_dp_frac = 0;
		div2        = 0x00000000;
		break;
	default:
		pll_dp_m    = 54;
		pll_dp_frac = 0;
		div2        = 0x00000000;
		break;
	}

	/*
	 * alt_mux: 0 = dp_only use pll1_0
	 *          1 = dp_only use pll1_1
	 *          2 = dp(pll1_0) + usb(pll1_1)
	 *          3 = dp(pll1_1) + usb(pll1_0)
	 */
	switch (alt_mux) {
	case 0: /* dp_only pll1_0 */
		pll1_0_en   = 1;
		pll1_0_m    = pll_dp_m;
		pll1_0_frac = pll_dp_frac;
		alt_mux_dec = 0x38;
		break;
	case 1:/* dp_only pll1_1 */
		pll1_1_en   = 1;
		pll1_1_m    = pll_dp_m;
		pll1_1_frac = pll_dp_frac;
		alt_mux_dec = 0x34;
		break;
	case 2: /* dp(pll1_0) + usb(pll1_1) */
		u3dp_sel = 3;
		if (flip == 1) {
			pll1_0_en   = 1;
			/* DP */
			pll1_0_m    = pll_dp_m;
			pll1_0_frac = pll_dp_frac;
			pll1_1_en   = 1;
			/* USB */
			pll1_1_m    = 50;
			pll1_1_frac = 0;
			alt_mux_dec = 0x10;
		} else {
			/* USB */
			pll1_0_en   = 1;
			pll1_0_m    = 50;
			pll1_0_frac = 0;
			/* DP */
			pll1_1_en   = 1;
			pll1_1_m    = pll_dp_m;
			pll1_1_frac = pll_dp_frac;
			alt_mux_dec = 0x20;
		}
		break;
	/* usb_only */
	default:
		pr_info(DPPHY_PREFIX "for usb case\n");
		break;
	}

	/*
	 * typec_dp ctrl, this will be controlled by USB PD driver
	 * dptx_phy_ana_write(tx_phy, ANACTRL_USBCDP_CTRL, (u3dp_sel << 7) |
	 *				   ((flip & 0x1) << 6) |
	 *				   ((alt_mux_dec & 0x3f) << 0));
	 */

	/* PLL for A9, pll pre */
	pr_info(DPPHY_PREFIX "A9 PLL addr = %x\n", DP_RG_USBCDP_PLL0_CNTL0);
	if (opts->dp_ops.link_rate == 1620) {
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, 0x016855e9);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL1, 0x0009e665);
		/* enable bias_en */
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, BIT(2), BIT(2));
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL1, BIT(27), BIT(27));
		/* rstn and lock_en */
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, GENMASK(27, 26),
				       GENMASK(27, 26));
		dptx_phy_usbdp_waitplllock(tx_phy, USBCDP_PLL_REG0, BIT(0));
	} else { /* for 2.7G or 5.4G */
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, 0x01f455e9);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL0_CNTL1, 0x0009e7e5);
		/* enable bias_en */
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, BIT(2), BIT(2));
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL1, BIT(27), BIT(27));
		/* rstn and lock_en */
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL0_CNTL0, GENMASK(27, 26),
				       GENMASK(27, 26));
		dptx_phy_usbdp_waitplllock(tx_phy, USBCDP_PLL_REG0, BIT(0));
	}
	usleep_range(100, 120);
	/* pll1_0 */
	if (pll1_0_en == 1) {
		u32 pll1_0_cntl1 = 0x1842a360;

		if (opts->dp_ops.link_rate == 1620)
			pll1_0_cntl1 = 0x1842a240;

		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL0, 0x40000071);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL1, pll1_0_cntl1);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, 0x00000050);
		usleep_range(10, 12);
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL0, GENMASK(3, 2),
				       GENMASK(3, 2));
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, 0x00002050);
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, BIT(14), BIT(14));
		if (opts->dp_ops.ssc) {
			/* 4500ppm 31k */
			dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, BIT(13), BIT(13));
			dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, BIT(15), BIT(15));
			dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL2, GENMASK(31, 16),
					       0x2330 << 16);
			/* for usb3 dp5.4G 2.7G, update to 0x4800007d; for dp 1.62G, 0x4c00007d */
			if (opts->dp_ops.link_rate == 1620)
				dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL0,
						       GENMASK(28, 26), 3 << 26);
			else
				dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL0,
						       GENMASK(28, 26), 2 << 26);
		}
		dptx_phy_usbdp_setbits(tx_phy, DP_RG_USBCDP_PLL1_0_CNTL1, BIT(19), BIT(19));
		dptx_phy_usbdp_waitplllock(tx_phy, USBCDP_PLL_REG0, BIT(1));
	}

	/* pll1_1 */
	if (pll1_1_en == 1) {
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL0,
			(0x40000071 & 0xFC00007F) | (pll1_1_frac << 7));
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL1,
			(0x1842a360 & 0xFFFFE00F) | (pll1_1_m << 4));
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL2, 0x00000040);
		usleep_range(10, 12);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL0,
			(0x4000007d & 0xFC00007F) | (pll1_1_frac << 7));
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL2, 0x00002050);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL2, 0x00006050);
		dptx_phy_usbdp_write(tx_phy, DP_RG_USBCDP_PLL1_1_CNTL1,
			(0x184aa360 & 0xFFFFE00F) | (pll1_1_m << 4));
	}

	/* initial value for bypass phy */
	data32 = dptx_get_upctx_efuse(tx_phy);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE0_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE1_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE2_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE3_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG0, 0x4D1C90);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE1_CFG0, 0x4D1C90);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE1_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE2_CFG0, 0x4D1C90);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE2_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE3_CFG0, 0x4D1C90);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE3_CFG1, data32);

	/* init phy setting as vswing0 and preemp0, and keep bit12 and bit15 as 0 */
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE0_CFG0, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE1_CFG0, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE2_CFG0, 0);
	dptx_phy_usbdp_write(tx_phy, USBCDPTX_LANE3_CFG0, 0);
	dptx_phy_set_prem_vswing(tx_phy, USBCDPTX_LANE0_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, USBCDPTX_LANE1_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, USBCDPTX_LANE2_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, USBCDPTX_LANE3_CFG0, 0, 0, 0);

	/* init bypass phy setting as vswing0 and preemp0 */
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG0, BIT(12) | div2);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE1_CFG0, BIT(12) | div2);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE2_CFG0, BIT(12) | div2);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE3_CFG0, BIT(12) | div2);
	data32 = dptx_get_upctx_efuse(tx_phy);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG1, data32);
	dptx_phy_usbdp_write(tx_phy, BYPASS_LANE0_CFG1, data32);
	dptx_phy_set_prem_vswing(tx_phy, BYPASS_LANE0_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, BYPASS_LANE1_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, BYPASS_LANE2_CFG0, 0, 0, 0);
	dptx_phy_set_prem_vswing(tx_phy, BYPASS_LANE3_CFG0, 0, 0, 0);

	dptx_phy_usbdp_write(tx_phy, DATA_MUX_CFG1, 0x5f00); // enable phy by bypass configure
	dptx_phy_usbdp_write(tx_phy, DATA_MUX_CFG1, 0xff00); // enable pll gate
	bypass_phy_to_normal_path(tx_phy);
	dptx_phy_usbdp_write(tx_phy, DATA_MUX_CFG1, 0xf000); // switch to original path
	pr_info(DPPHY_PREFIX "[%d] %lld %s\n", 144, meson_clk_measure(144), "dptx_lnk_clk");
	pr_info(DPPHY_PREFIX "set mode done\n");
	return 0;
}

/*
 * upctx_cfg2<3> upctx_cfg2<5> pre_emphasis
 *      0             0             ddb
 *      1             1             3.5db
 *      1             1             6db
 *     N/A           N/A            3/9db unsupport
 *
 * upctx_cfg2<6> upctx_cfg2<4> upctx_cfg1<3:0> uptctx_efuse<4:0> DP level
 *      1             0            4'b0000       5'b01011         950mV
 *      1             0            4'b0000       5'b00011         level_2 800mV(850mV)
 *      1             1            4'b1010       5'b11011         level_1 600mV(600mV)
 *      0             1            4'b1010       5'b01011         level_0 400mV(400mV)
 * For uptctx_efuse, <2:0> value will be getting from efuse, and <4:3> refer to above table
 */
#define PARAM_VSWING_NO 3
#define PARAM_PREEMP_NO 3
struct phy_param_table {
	u16 upctx_cfg2_7_0;
	u16 upctx_cfg1_11_0;
	u16 efuse_10_0;
};

static const struct phy_param_table phy_param_list[PARAM_VSWING_NO][PARAM_PREEMP_NO] = {
	[0][0] = {0x15, 0xC9A, 0x363},
	[0][1] = {0x0D, 0xC90, 0x363},
	[0][2] = {0x6D, 0xC90, 0x36B},
	[1][0] = {0x55, 0xC98, 0x37B},
	[1][1] = {0x4D, 0xC90, 0x363},
	[1][2] = {0x6D, 0xC90, 0x37B},
	[2][0] = {0x05, 0xC90, 0x103},
	[2][1] = {0x4D, 0xC80, 0x37B},
	/*
	 * [2][2] refer to Table Allowed Vdiff_pre_pp and Pre-emphasis Combinations
	 * no need to support vswing as 2 and preemp as 2
	 */
};

static void dptx_phy_set_prem_vswing(void *data, u32 reg, u32 lane, u32 vswing, u32 pre)
{
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)data;
	u32 data32;

	pr_info(DPPHY_PREFIX "train lane %d vswing %d pre %d\n", lane, vswing, pre);
	if (!data || (pre > 2 || vswing > 2) || (vswing == 2 && pre == 2))
		return;

	dptx_phy_usbdp_setbits(tx_phy, reg, GENMASK(23, 16),
			       phy_param_list[vswing][pre].upctx_cfg2_7_0 << 16);
	dptx_phy_usbdp_setbits(tx_phy, reg, GENMASK(11, 0),
			       phy_param_list[vswing][pre].upctx_cfg1_11_0 << 0);
	/* extra offset plus 4 */
	dptx_phy_usbdp_setbits(tx_phy, reg + 4, GENMASK(4, 3),
			       phy_param_list[vswing][pre].efuse_10_0);
	dptx_phy_usbdp_setbits(tx_phy, reg + 4, GENMASK(2, 0),
			       dptx_get_efuse_trim(DPTX_TRIM_EFUSE_TX_BOOTST) & 0x7);
	/* For vswing2 and preemp0 adjust */
	if (vswing == 2 && pre == 0) {
		dptx_phy_usbdp_write(tx_phy, reg + 4, phy_param_list[vswing][pre].efuse_10_0);
	} else {
		data32 = dptx_get_upctx_efuse(tx_phy);
		dptx_phy_usbdp_setbits(tx_phy, reg + 4, GENMASK(10, 5), data32 & GENMASK(10, 5));
	}
}

/* convert void *phy to struct meson_tx_phy *phy */
static int meson_tx_dp_phy_configure(void *phy, struct meson_tx_phy_cfg_opts *opts)
{
	int i;
	u32 link_lanes = 0;
	u32 phy_4lane_ctrl_reg[4] = {
		USBCDPTX_LANE0_CFG0,
		USBCDPTX_LANE1_CFG0,
		USBCDPTX_LANE2_CFG0,
		USBCDPTX_LANE3_CFG0,
	};

	if (!phy || !opts)
		return -1;
	link_lanes = opts->dp_ops.lanes;
	for (i = 0; i < 4; i++)
		dptx_phy_set_prem_vswing(phy, phy_4lane_ctrl_reg[i], i,
			opts->dp_ops.voltage[i], opts->dp_ops.pre[i]);

	return 0;
}

static void dptx_phy_toggle_lane(struct meson_tx_phy *tx_phy, u32 lane_reg)
{
	u32 bit12 = dptx_phy_usbdp_read(tx_phy, lane_reg) & BIT(12);
	u32 bit15 = dptx_phy_usbdp_read(tx_phy, lane_reg) & BIT(15);

	dptx_phy_usbdp_setbits(tx_phy, lane_reg, BIT(12), 0);
	dptx_phy_usbdp_setbits(tx_phy, lane_reg, BIT(15), 0);
	dptx_phy_usbdp_setbits(tx_phy, lane_reg, BIT(12), bit12);
	dptx_phy_usbdp_setbits(tx_phy, lane_reg, BIT(15), bit15);
}

static int meson_tx_dp_phy_toggle(void *phy)
{
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)phy;

	dptx_phy_toggle_lane(tx_phy, USBCDPTX_LANE0_CFG0);
	dptx_phy_toggle_lane(tx_phy, USBCDPTX_LANE1_CFG0);
	dptx_phy_toggle_lane(tx_phy, USBCDPTX_LANE2_CFG0);
	dptx_phy_toggle_lane(tx_phy, USBCDPTX_LANE3_CFG0);
	pr_info("dptxphy toggle phy test\n");
	return 0;
}

static int meson_tx_dp_phy_validate(void *phy, enum meson_tx_phy_mode mode,
			struct meson_tx_phy_cfg_opts *opts)
{
	int i;
	u32 link_rate = 0;
	u32 link_lanes = 0;

	if (mode != PHY_DP || !phy || !opts)
		return -1;

	/* check link rates */
	link_rate = opts->dp_ops.link_rate;
	if (link_rate != 1620 && link_rate != 2700 && link_rate != 5400)
		return -1;

	/* check link lanes count */
	link_lanes = opts->dp_ops.lanes;
	if (link_lanes != 1 && link_lanes != 2 && link_lanes != 4)
		return -1;

	/* check Vswing and PreEmp */
	/* refer to DP1.4Spec, Table 3-1: Allowed Vdiff_pre_pp and Pre-emphasis Combinations*/
	/* currently pre only supports Level 0/1/2, and vswing supports up to level3 950mV */
	for (i = 0; i < 3; i++) {
		if (opts->dp_ops.voltage[i] > 3)
			return -1;
		if (opts->dp_ops.pre[i] > 2)
			return -1;
		if (opts->dp_ops.pre[i] == 3 && opts->dp_ops.voltage[i] >= 1)
			return -1;
		if (opts->dp_ops.pre[i] == 2 && opts->dp_ops.voltage[i] >= 2)
			return -1;
		if (opts->dp_ops.pre[i] == 1 && opts->dp_ops.voltage[i] >= 3)
			return -1;
	}

	return 0;
}

int meson_tx_dp_dump_regs(void *phy, struct seq_file *s)
{
	u32 reg;
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)phy;
	const char *msg_bar = "=====================";
	const char *info = NULL;

	if (!s)
		return -1;

	seq_printf(s, "%s start dump dp status %s\n", msg_bar, msg_bar);
	seq_puts(s, "usb pd notify status:\n");
	switch (tx_phy->usb_dp_state.flip) {
	case TYPEC_ORIENTATION_NORMAL:
		info = "normal";
		break;
	case TYPEC_ORIENTATION_REVERSE:
		info = "reverse";
		break;
	case TYPEC_ORIENTATION_NONE:
	default:
		info = "none";
		break;
	}
	seq_printf(s, "    Typec orientation: %s\n", info);
	seq_printf(s, "    Pin State: 0x%x\n", tx_phy->usb_dp_state.pin_state);
	seq_printf(s, "    Status Update command VDO content: 0x%x\n",
		tx_phy->usb_dp_state.dp_state_data.status);
	seq_printf(s, "         %s connected\n", typec_dp_get_devices_connect_st(tx_phy));
	seq_printf(s, "        hpd plug state: %d\n", typec_dp_get_hpd_plug_state(tx_phy));
	seq_printf(s, "        hpd irq state: %d\n", typec_dp_get_hpd_irq_state(tx_phy));
	seq_printf(s, "    Configure command VDO content: 0x%x\n",
		tx_phy->usb_dp_state.dp_state_data.conf);
	switch ((tx_phy->usb_dp_state.dp_state_data.conf >> 8) & 0xff) {
	case 0x00:
		info = "deselect";
		break;
	case 0x04:
		info = "C";
		break;
	case 0x08:
		info = "D";
		break;
	case 0x10:
		info = "E";
		break;
	default:
		info = "reserved";
		break;
	}
	seq_printf(s, "        Pin Assignment: %s\n", info);
	seq_puts(s, "AUX CTRL %s\n");
	reg = DP_RG_AUX;
	seq_printf(s, "  [0x%08x] = 0x%08x\n", reg, dptx_phy_usbdp_read(tx_phy, reg));
	reg = ANACTRL_USBCDP_CTRL;
	seq_printf(s, "[0x%08x] = 0x%08x\n", reg, dptx_phy_ana_read(tx_phy, reg));
	for (reg = DP_RG_USBCDP_PLL0_CNTL0; reg < USBCDPRX_LANE2_RO0 + 1; reg += 4)
		seq_printf(s, "[0x%08x] = 0x%08x\n", reg, dptx_phy_usbdp_read(tx_phy, reg));
	seq_printf(s, "%s  end dump dp status  %s\n", msg_bar, msg_bar);

	return 0;
}

/*
 * FT: external loopback, need FT TE
 * PRBS: internal loopback, pure SW control
 * USB/DP pins:
 *     Tx0     Tx1/Rx1      Tx2/Rx2       Tx3
 * Case1:
 *     Tx0 -> Rx1     Tx2 -> Rx2
 * Case2:
 *     Tx1 -> Rx1     Tx3 -> Rx2
 */
static int meson_tx_dp_prbs_test(void *phy)
{
	u32 prbs_ana0 = 0;
	u32 prbs_ana1 = 0;
	u32 prbs_stat = 0;
	u32 data32    = 0;
	struct meson_tx_phy *tx_phy = (struct meson_tx_phy *)phy;

	pr_info(DPPHY_PREFIX "wait pll1_0 lock...\n");
	data32 = dptx_phy_usbdp_read(tx_phy, USBCDP_PLL_REG0);
	while (!((data32 >> 1) & 0x1)) {
		usleep_range(10, 12);
		data32 = dptx_phy_usbdp_read(tx_phy, USBCDP_PLL_REG0);
	}
	pr_info(DPPHY_PREFIX "pll1_0 locked Now !!!\n");

	/* rx1 use pll1_0 */
	/* bit[1] = 1,rx1 use pll1_0 */
	// set by USB driver  dptx_phy_usbdp_setbits(tx_phy, ANACTRL_USBCDP_CTRL, BIT(1), BIT(1));

	/*
	 * this used in lbpk FT test case for rx0/1 phy
	 */
	data32 = 0xe9943075;
	data32 &= ~(0xf << 4);
	data32 |= dptx_get_efuse_trim(DPTX_TRIM_EFUSE_DCHA) << 4;
	dptx_phy_usbdp_write(tx_phy, DP_REG_DCHA_AFE, data32);
	dptx_phy_usbdp_write(tx_phy, DP_REG_DCHA_DFE, 0x54147e68);
	dptx_phy_usbdp_write(tx_phy, DP_REG_DCHD_CDR, 0x240014d3);
	dptx_phy_usbdp_write(tx_phy, DP_REG_DCHD_EQ, 0x2011104f);
	dptx_phy_usbdp_write(tx_phy, DP_REG_MISC, 0x00007600);

	data32  =  0;
	data32 |= (0x1 << 31);    /* dp_upcrx_vcm_en */
	data32 |= (0x1 << 29);    /* dp_upcrx_pi_clk_en */
	data32 |= (0x1 << 26);    /* dp_upcrx_ofst_rstb */
	data32 |= (0x1 << 25);    /* dp_upcrx_mux_rstb */
	data32 |= (0x1 << 23);    /* dp_upcrx_lpbk_des_en */
	data32 |= (0x1 << 21);    /* dp_upcrx_lfps_en */
	data32 |= (0x1 << 17);    /* dp_upcrx_eq_rstb */
	data32 |= (0x1 << 15);    /* dp_upcrx_eq_mode */
	data32 |= (0x1 << 14);    /* dp_upcrx_eq_dcoc_en */
	data32 |= (0xf <<  9);    /* dp_upcrx_eq_byp_val */
	data32 |= (0x1 <<  7);    /* dp_upcrx_dfe_rstb */
	data32 |= (0x1 <<  6);    /* dp_upcrx_dfe_hold */
	data32 |= (0x1 <<  4);    /* dp_upcrx_dch_rstn */
	data32 |= (0x1 <<  2);    /* dp_upcrx_cdr_rstb */
	dptx_phy_usbdp_write(tx_phy, DP_IDLE_RX_CFG0, data32);
	udelay(1);
	data32 |= (0x1 <<  5);
	/* release dch_rstn */
	dptx_phy_usbdp_write(tx_phy, DP_IDLE_RX_CFG0, data32);
	udelay(1);
	data32 |= (0x1 <<  3);
	/* release cdr_rstb */
	dptx_phy_usbdp_write(tx_phy, DP_IDLE_RX_CFG0, data32);

	/* TX0 -> RX1,   TX2->RX2; tx loopback en */
	pr_info(DPPHY_PREFIX "TX0->RX1 TX2->RX2 PRBS TEST\n");

	/* bit9 loopback en */
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE0_CFG0, BIT(9), BIT(9));
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE2_CFG0, BIT(9), BIT(9));

	/* prbs config */
	/* bit[   15] soft_reset:  1=reset  */
	/* bit[   12] data mux:    1=prbs_data, 0=dptx data */
	/* bit[7 : 6] clk mux:     0=lane0_clk, 1=lane1_clk, 2=lane2_clk, 3=lane3_clk */
	/* bit[4 : 2] prbs mode:   1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31 */
	/* bit[    1] inj_bist_err:1=inject error */
	/* bit[    0] prbs_en:     1=prbs_enable */
	dptx_phy_usbdp_write(tx_phy, DP_PRBS_TEST_CFG0,   (0x0 << 15) |
					(0x1 << 12) |
					(0x0 <<  6) |
					(0x1 <<  2) |
					(0x0 <<  1) |
					(0x1 <<  0));

	/* prbs Analog config */
	/* bit[14:12] ana1 mode: 1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31 */
	/* bit[    9] Restart prbs ana1 */
	/* bit[    8] enable prbs ana1 */
	/* bit[6 : 4] ana0 mode: 1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31 */
	/* bit[    9] Restart prbs ana0 */
	/* bit[    8] enable prbs ana0 */
	dptx_phy_usbdp_write(tx_phy, DP_PRBS_TEST_CFG1,   (0x1 << 12) |
					(0x0 <<  9) |
					(0x1 <<  8) |
					(0x1 <<  4) |
					(0x0 <<  1) |
					(0x1 <<  0));

	usleep_range(100, 120);

	/* prbs Analog check */
	prbs_ana0 = dptx_phy_usbdp_read(tx_phy, PRBS_REG_RO0);
	prbs_ana1 = dptx_phy_usbdp_read(tx_phy, PRBS_REG_RO1);

	/* [22] ana0 lock;[23] ana0 not ok;[24]ana1 lock;[25] ana1 not ok */
	prbs_stat = (prbs_ana0 >> 22) & 0xf;

	if (prbs_stat != 0x5) {
		pr_info(DPPHY_PREFIX "Error: Prbs result check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs result = %x\n", prbs_stat);
	} else {
		pr_info(DPPHY_PREFIX "Prbs result check Pass\n");
	}

	if (prbs_ana0 & 0x3fffff) {
		pr_info(DPPHY_PREFIX "Error: Prbs0 ber check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs0 ber = %x\n", prbs_ana0 & 0x3fffff);
	} else {
		pr_info(DPPHY_PREFIX "Prbs0 ber check Pass\n");
	}

	if (prbs_ana1 & 0x3fffff) {
		pr_info(DPPHY_PREFIX "Error: Prbs1 ber check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs2 ber = %x\n", prbs_ana0 & 0x3fffff);
	} else {
		pr_info(DPPHY_PREFIX "Prbs1 ber check Pass\n");
	}

	/* TX1 -> RX1, TX3->RX2; tx loopback en */
	pr_info(DPPHY_PREFIX "TX1->RX1 TX3->RX2 PRBS TEST\n");

	/* bit9 loopback en */
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE0_CFG0, BIT(9), 0);
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE2_CFG0, BIT(9), 0);

	/* bit9 loopback en */
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE1_CFG0, BIT(9), BIT(9));
	dptx_phy_usbdp_setbits(tx_phy, USBCDPTX_LANE3_CFG0, BIT(9), BIT(9));

	/* prbs config */
	/* bit[   15] soft_reset:  1=reset */
	dptx_phy_usbdp_setbits(tx_phy, DP_PRBS_TEST_CFG0, BIT(15), BIT(15));
	/* bit[   15] soft_reset:  1=reset */
	dptx_phy_usbdp_setbits(tx_phy, DP_PRBS_TEST_CFG0, BIT(15), 0);

	usleep_range(100, 120);

	/* prbs ana check */
	prbs_ana0 = dptx_phy_usbdp_read(tx_phy, PRBS_REG_RO0);
	prbs_ana1 = dptx_phy_usbdp_read(tx_phy, PRBS_REG_RO1);

	/* [22] ana0 lock;[23] ana0 not ok;[24]ana1 lock;[25] ana1 not ok */
	prbs_stat = (prbs_ana0 >> 22) & 0xf;

	if (prbs_stat != 0x5) {
		pr_info(DPPHY_PREFIX "Error: Prbs result check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs result = %x\n", prbs_stat);
	} else {
		pr_info(DPPHY_PREFIX "Prbs result check Pass\n");
	}

	if (prbs_ana0 & 0x3fffff) {
		pr_info(DPPHY_PREFIX "Error: Prbs0 ber check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs0 ber = %x\n", prbs_ana0 & 0x3fffff);
	} else {
		pr_info(DPPHY_PREFIX "Prbs0 ber check Pass\n");
	}

	if (prbs_ana1 & 0x3fffff) {
		pr_info(DPPHY_PREFIX "Error: Prbs1 ber check Fail\n");
		pr_info(DPPHY_PREFIX "Prbs2 ber = %x\n", prbs_ana0 & 0x3fffff);
	} else {
		pr_info(DPPHY_PREFIX "Prbs1 ber check Pass\n");
	}

	return 0;
}

struct meson_tx_phy_ops dptx_phy_ops = {
	.init = meson_tx_dp_phy_init,
	.exit = meson_tx_dp_phy_exit,
	.power_on = meson_tx_dp_phy_power_on,
	.power_off = meson_tx_dp_phy_power_off,
	.set_aux_gpio = meson_tx_dp_phy_set_aux_gpio,
	.set_mode = meson_tx_dp_phy_set_mode,
	.configure = meson_tx_dp_phy_configure,
	.toggle = meson_tx_dp_phy_toggle,
	.validate = meson_tx_dp_phy_validate,
	/* DPTx PHY doesn't have calibration function */
	.calibrate = NULL,
	.connect = NULL,
	.disconnect = NULL,
	.dump_reg = meson_tx_dp_dump_regs,
	.prbs_test = meson_tx_dp_prbs_test,
};

#define DPPHY_CTRL_NAME   "dpphy_ctrl_reg"
#define ANACTRL_USBCDP_NAME "anactrl_typec_dp_reg"

static const struct regmap_config dpphy_ctrl_regmap_config = {
	.name = DPPHY_CTRL_NAME,
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0xFC,
	.fast_io = true,
};

static const struct regmap_config anactrl_typec_dp_regmap_config = {
	.name = ANACTRL_USBCDP_NAME,
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0xFFC,
	.fast_io = true,
};

static int dptx_init_reg_map(struct platform_device *pdev, struct meson_tx_phy *tx_phy)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	void __iomem *base;
	struct regmap *regmap;
	int ret = 0;

	/*
	 * in the a9 dts file, register addresses will be assigned like below
	 * reg = <0x0 0xfe48c000 0x0 0x100>,
	 *       <0x0 0xfe008000 0x0 0x1000>;
	 *       reg-names = DPPHY_CTRL_NAME, ANACTRL_USBCDP_NAME;
	 */

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, DPPHY_CTRL_NAME);
	if (!res) {
		dev_err(dev, "Failed to get %s address\n", DPPHY_CTRL_NAME);
		return -ENODEV;
	}
	base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(base)) {
		dev_err(dev, "Failed to ioremap %s memory region\n", DPPHY_CTRL_NAME);
		return PTR_ERR(base);
	}
	tx_phy->base_dpphy_ctrl = base;
	regmap = devm_regmap_init_mmio(dev, base, &dpphy_ctrl_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err_probe(dev, ret, "Failed to initialize %s regmap\n", DPPHY_CTRL_NAME);
		return ret;
	}
	tx_phy->regmap_dpphy_ctrl = regmap;
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, ANACTRL_USBCDP_NAME);
	if (!res) {
		dev_err(dev, "Failed to get %s address\n", ANACTRL_USBCDP_NAME);
		return -ENODEV;
	}
	base = devm_ioremap(dev, res->start, resource_size(res));
	if (IS_ERR(base)) {
		dev_err(dev, "Failed to ioremap %s memory region\n", ANACTRL_USBCDP_NAME);
		return PTR_ERR(base);
	}
	tx_phy->base_anactrl_typec_dp = base;
	regmap = devm_regmap_init_mmio(dev, base, &anactrl_typec_dp_regmap_config);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		dev_err_probe(dev, ret, "Failed to initialize %s regmap\n", ANACTRL_USBCDP_NAME);
		return ret;
	}
	tx_phy->regmap_anactrl_typec_dp = regmap;

	return 0;
}

static BLOCKING_NOTIFIER_HEAD(dptx_phy_notifier_list);

/**
 * meson_tx_dpphy_register_notify - register a notifier callback whenever a
 * dphy change happens.
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are typec devices being added or removed.
 */
void meson_tx_dpphy_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&dptx_phy_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(meson_tx_dpphy_register_notify);

/**
 * meson_tx_dpphy_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * meson_tx_dpphy_register_notify() must have been previously called
 * for this function to work properly.
 */
void meson_tx_dpphy_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&dptx_phy_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(meson_tx_dpphy_unregister_notify);

static inline void dptx_phy_notify_hpd_plugin(struct meson_tx_phy *tx_phy)
{
	blocking_notifier_call_chain(&dptx_phy_notifier_list,
		MESON_TX_CONN_DPPHY_HPD_PLUGIN,	NULL);
}

static inline void dptx_phy_notify_hpd_plugout(struct meson_tx_phy *tx_phy)
{
	blocking_notifier_call_chain(&dptx_phy_notifier_list,
		MESON_TX_CONN_DPPHY_HPD_PLUGOUT, NULL);
}

static inline void dptx_phy_notify_hpd_irq(struct meson_tx_phy *tx_phy)
{
	blocking_notifier_call_chain(&dptx_phy_notifier_list,
		MESON_TX_CONN_DPPHY_HPD_IRQ, NULL);
}

static inline void dptx_phy_notify_lane_remap(struct meson_tx_phy *tx_phy)
{
	blocking_notifier_call_chain(&dptx_phy_notifier_list,
		MESON_TX_CONN_DPPHY_LANE_REMAP, &tx_phy->usb_dp_state);
}

static const char *typec_dp_get_devices_connect_st(struct meson_tx_phy *tx_phy)
{
	static const char *msg[4] = {
		"neither DP source nor DP sink",
		"DP source",
		"DP sink",
		"both DP source and sink",
	};

	if (!tx_phy)
		return "";
	return msg[tx_phy->usb_dp_state.dp_state_data.status & 0x3];
}

static bool typec_dp_sink_device_is_connected(struct meson_tx_phy *tx_phy)
{
	if (!tx_phy)
		return 0;
	return ((tx_phy->usb_dp_state.dp_state_data.status & 0x3) >= 2);
}

/*
 * 1. when USB plugs, CC will detect the orient
 * 2. then enumerate the DP altmode, and get the pin assignment
 * 3. last the hpd irq, hpd events, ...
 */
static int usb_to_dptx_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct meson_tx_phy *tx_phy = container_of(nb, struct meson_tx_phy, usbpd_to_dp_nb);
	u32 state = 0;
	struct typec_displayport_data *dp_data = NULL;

	pr_info(DPPHY_PREFIX "notify from usb: action = %lu\n", action);
	switch (action) {
	case TYPEC_MESON_ORIENT:
	/* any USB typec device attached, then notify TYPEC_MESON_ORIENT */
		tx_phy->usb_dp_state.flip = *(enum typec_orientation *)data;
		state = tx_phy->usb_dp_state.flip;
		pr_info(DPPHY_PREFIX "orient = %d\n", state);
		/* USB device detached, will notify ORIENTATION_NONE */
		switch (state) {
		case TYPEC_ORIENTATION_NORMAL:
			meson_tx_dp_phy_set_aux_gpio(tx_phy, 0, 1);
			dptx_aux_phy_output_enable(tx_phy, 0, 1);
			break;
		case TYPEC_ORIENTATION_REVERSE:
			meson_tx_dp_phy_set_aux_gpio(tx_phy, 1, 0);
			dptx_aux_phy_output_enable(tx_phy, 1, 1);
			break;
		case TYPEC_ORIENTATION_NONE:
		default:
			meson_tx_dp_phy_set_aux_gpio(tx_phy, 2, 2);
			dptx_aux_phy_output_enable(tx_phy, 0, 0);
			dp_data = &tx_phy->usb_dp_state.dp_state_data;
			/* when the last config is DP sink, then notify plugout */
			if (DP_CONF_CURRENTLY(dp_data->conf) == DP_CONF_UFP_U_AS_UFP_D) {
				dptx_phy_notify_hpd_plugout(tx_phy);
				dp_data->conf = 0;
			}
			break;
		}
		break;
	case TYPEC_MESON_PIN_STATE:
	/*
	 * USB+DP or DP case will notify, when enter DPAM, may trigger many times
	 * this value is translated by kernel, not USB CC states
	 * this value only valid with TYPEC_DP_STATE_C/D/E, ignore other values
	 */
		tx_phy->usb_dp_state.pin_state = *(unsigned long *)data;
		state = tx_phy->usb_dp_state.pin_state;
		pr_info(DPPHY_PREFIX "pin state = %d\n", state);
		if (state == TYPEC_DP_STATE_C || state == TYPEC_DP_STATE_D ||
			state == TYPEC_DP_STATE_E) {
			dptx_phy_notify_lane_remap(tx_phy);
		}
		break;
	case TYPEC_MESON_DP_STATE_DATA:
		tx_phy->usb_dp_state.dp_state_data = *(struct typec_displayport_data *)data;
		dp_data = &tx_phy->usb_dp_state.dp_state_data;
		pr_info(DPPHY_PREFIX "dp_status = 0x%x dp_conf = 0x%x\n", dp_data->status,
			dp_data->conf);
		pr_info(DPPHY_PREFIX "%s connected\n", typec_dp_get_devices_connect_st(tx_phy));
		if (!typec_dp_sink_device_is_connected(tx_phy)) {
			dptx_phy_notify_hpd_plugin(tx_phy);
			break;
		}
		if (typec_dp_get_hpd_irq_state(tx_phy)) {
			dptx_phy_notify_hpd_irq(tx_phy);
			break;
		}
		/* hpd plug in or out will use the HPD_OVER control */
		if (typec_dp_get_hpd_plug_state(tx_phy))
			dptx_phy_notify_hpd_plugin(tx_phy);
		else
			dptx_phy_notify_hpd_plugout(tx_phy);
		/* other cases, such as power status changed */
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static int meson_tx_dp_phy_probe(struct platform_device *pdev)
{
	struct meson_tx_phy *tx_phy;
	const struct meson_tx_phy_data *data;
	int ret = 0;
	int i;
	struct gpio_desc *desc;

	pr_info(DPPHY_PREFIX "probe\n");

	tx_phy = kzalloc(sizeof(*tx_phy), GFP_KERNEL);
	if (!tx_phy)
		return -ENOMEM;

	/* 1. Try to get config from device tree match */
	data = of_device_get_match_data(&pdev->dev);
	if (data) {
		pr_info(DPPHY_PREFIX "probe use dt\n");
	} else {
		/* 2. Fall back to legacy platform_data */
		data = dev_get_platdata(&pdev->dev);
		if (data) {
			pr_info(DPPHY_PREFIX "use legacy platform_data\n");
		} else {
			pr_err("[dptx] phy No config found in DT or platform_data\n");
			return -EINVAL;
		}
	}

	/* get 2 AUX gpios desc. */
	for (i = 0; i < 2; i++) {
		desc = devm_gpiod_get_index(&pdev->dev, "dptx-aux", i, GPIOD_IN);
		if (IS_ERR_OR_NULL(desc))
			return -EINVAL;
		tx_phy->aux_gpio[i] = desc;
		gpiod_direction_input(desc);
	}

	ret = dptx_init_reg_map(pdev, tx_phy);
	if (ret < 0)
		pr_err("no register address in board dtb\n");
	tx_phy->pdev = pdev;
	tx_phy->ops = data->ops;
	dev_set_drvdata(&pdev->dev, tx_phy);
	tx_phy->usbpd_to_dp_nb.notifier_call = usb_to_dptx_notifier;
	typec_mux_meson_register_notify(&tx_phy->usbpd_to_dp_nb);

	return 0;
}

static void meson_tx_phy_remove(struct platform_device *pdev)
{
	struct device *device = &pdev->dev;

	pr_info(DPPHY_PREFIX "remove %s\n", dev_name(device));
}

static const struct meson_tx_phy_data normal_phy_data = {
	.ops = &dptx_phy_ops,
};

static const struct of_device_id meson_tx_phy_of_match[] = {
	{
		.compatible      = "amlogic, tx-phy-t7c",
		.data = &normal_phy_data,
	},
	{
		.compatible      = "amlogic, tx-phy-a9",
		.data = &normal_phy_data,
	},
	{},
};

static struct platform_driver meson_tx_phy_driver = {
	.probe   = meson_tx_dp_phy_probe,
	.remove  = meson_tx_phy_remove,
	.driver  = {
		.name = DEVICE_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(meson_tx_phy_of_match),
	}
};

int  __init meson_tx_phy_drv_init(void)
{
	return platform_driver_register(&meson_tx_phy_driver);
}

void __exit meson_tx_phy_drv_exit(void)
{
	platform_driver_unregister(&meson_tx_phy_driver);
}
