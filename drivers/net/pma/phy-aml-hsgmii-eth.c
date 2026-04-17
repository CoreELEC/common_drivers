// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2026 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/ethtool.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "phy-aml-hsgmii.h"

#define HSG_UPCTX_BOOST_EN			BIT(22)
#define HSG_UPCTX_BOOST_CTRL_MASK		GENMASK(4, 0)
#define HSG_UPCTX_BOOST_CTRL_SHIFT		0
#define HSG_UPCTX_PUIO_MAIN_MASK		GENMASK(7, 5)
#define HSG_UPCTX_PUIO_MAIN_SHIFT		5
#define HSG_UPCTX_PDIO_MAIN_MASK		GENMASK(10, 8)
#define HSG_UPCTX_PDIO_MAIN_SHIFT		8
#define HSG_DCHA_AFE_UPCRX_MASK			GENMASK(7, 4)
#define HSG_DCHA_AFE_UPCRX_SHIFT		4

#define HSG_RESET_TIME_US			100

/* Only for PHY Sync Link mode */
#define HSG_WAIT_SPEED_DETECT_TIMER		0xA2C3

// #define HSG_PHY_DEBUG
#ifdef HSG_PHY_DEBUG
#define hsg_debug(fmt, args...)	pr_info(fmt, ##args)
#else
#define hsg_debug(fmt, args...)	pr_debug(fmt, ##args)
#endif

struct amlogic_hsgmii_phy_data {
	struct regmap		*regmap;
	struct clk		*hsg_general_clk;
	struct reset_control	*hsg_reset;	/* The Logic reset of HSGMII PHY */
	struct reset_control	*hsg_apb_reset;	/* The Reg reset of HSGMII PHY */
	void __iomem		*hsg_trim_reg;	/* HSGMII trim reg base */
	void __iomem		*u3hsg_combo_reg; /* U3 HSGMII combo reg base */
	u32			combo_hsg_sel_val; /* Value to select HSGMII mode */

	/* Current speed of HSGMII PHY */
	int			speed;
	/* True: PHY Sync Link, False: Fast Link */
	bool			psl_mode;
	/* Only for Fast Link mode. If set, phy speed change will lead to Rx fsm restart */
	bool			rx_fsm_restart;
	/* Only for Fast Link mode. If set, phy speed change will lead to pll fsm restart */
	bool			pll_fsm_restart;
#ifdef HSG_PHY_DEBUG
	struct hsg_phy_stats	hsg_stats;
#endif
};

static int aml_regmap_update_bits(struct regmap *map, unsigned int reg,
			    unsigned int mask, unsigned int val)
{
	return regmap_update_bits_base(map, reg, mask, val, NULL, false, true);
}

static void amlogic_hsgmii_phy_init_1g(struct regmap *regmap)
{
	/* TODO */
}

static void amlogic_hsgmii_phy_init_2p5g(struct regmap *regmap)
{
	/* TODO */
}

static inline int
amlogic_hsgmii_phy_poll_status(struct regmap *regmap, unsigned int reg,
			       unsigned int bit)
{
	unsigned int val;

	return regmap_read_poll_timeout(regmap, reg, val,
					val & bit, 1500, 750000);
}

static inline int amlogic_hsgmii_phy_reset(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);

	/* Reset hsgmii initial fsm and flow */
	aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
			       HSG_INIT_CTRL_RESET,
			       HSG_INIT_CTRL_RESET);

	udelay(HSG_RESET_TIME_US);

	aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
			       HSG_INIT_CTRL_RESET,
			       0);

	return 0;
}

static int amlogic_hsgmii_phy_calibrate(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);
	struct device *dev = phy->dev.parent;
	int ret;

	switch (data->speed) {
	case SPEED_10:
	case SPEED_100:
	case SPEED_1000:
		amlogic_hsgmii_phy_init_1g(data->regmap);
		break;
	case SPEED_2500:
		amlogic_hsgmii_phy_init_2p5g(data->regmap);
		break;
	}

	/* HSGMII reset */
	// ret = amlogic_hsgmii_phy_reset(phy);
	ret = reset_control_reset(data->hsg_reset);
	if (ret) {
		dev_err(dev, "failed to reset HSGMII PHY: %d\n", ret);
		return ret;
	}

	if (amlogic_hsgmii_phy_poll_status(data->regmap,
					   AML_PHY_HSG_PLL_INIT_DEBUG,
					   HSG_PLL_INIT_DEBUG_PLL1_LOCK)) {
		dev_err(dev, "HSGMII_PLL1_LOCK timed-out");
		return -ETIMEDOUT;
	}

	if (amlogic_hsgmii_phy_poll_status(data->regmap,
					   AML_PHY_HSG_RX_CTRL_DEBUG,
					   HSG_RX_CTRL_DEBUG_RX_FSM_READY)) {
		dev_err(dev, "HSGMII_RX_FSM_READY timed-out");
		return -ETIMEDOUT;
	}

	if (amlogic_hsgmii_phy_poll_status(data->regmap,
					   AML_PHY_HSG_RX_CTRL_DEBUG,
					   HSG_RX_CTRL_DEBUG_UPCRX_CDRLOCK_SYNC)) {
		dev_err(dev, "HSGMII_UPCRX_CDRLOCK_SYNC timed-out");
		return -ETIMEDOUT;
	}

	if (amlogic_hsgmii_phy_poll_status(data->regmap,
					   AML_PHY_HSG_RX_CTRL_DEBUG,
					   HSG_RX_CTRL_DEBUG_PCS_SYNC_CDC)) {
		dev_err(dev, "HSGMII_PCS_SYNC_CDC timed-out");
		return -ETIMEDOUT;
	}

	return 0;
}

static int amlogic_hsgmii_phy_trim(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);
	struct device *dev = phy->dev.parent;
	u32 raw, val, otp_val;

	if (!data->hsg_trim_reg)
		return -EINVAL;

	raw = readl(data->hsg_trim_reg);

	if (!(raw & BIT(6))) {
		dev_err(dev, "No HSGMII PHY TX impedance trim? Skip.\n");
	} else {
		val = readl(data->hsg_trim_reg + AML_PHY_HSG_TX_CFG2);
		val &= (~HSG_UPCTX_PDIO_MAIN_MASK) & (~HSG_UPCTX_PUIO_MAIN_MASK);

		/* set upctx_pdio_main in tx_cfg2. */
		otp_val = (raw & (u32)GENMASK(2, 0)) >> 0;
		val |= (otp_val << HSG_UPCTX_PDIO_MAIN_SHIFT);
		/* set upctx_puio_main in tx_cfg2. */
		otp_val = (raw & (u32)GENMASK(5, 3)) >> 3;
		val |= (otp_val << HSG_UPCTX_PUIO_MAIN_SHIFT);

		writel(val, data->hsg_trim_reg + AML_PHY_HSG_TX_CFG2);
	}

	if (!(raw & BIT(11))) {
		dev_err(dev, "No HSGMII PHY TX amplitude trim? Skip.\n");
	} else {
		/* set upctx_boost_ctrl in tx_cfg2. */
		otp_val = (raw & (u32)GENMASK(10, 7)) >> 7;
		val = readl(data->hsg_trim_reg + AML_PHY_HSG_TX_CFG2);
		val &= ~HSG_UPCTX_BOOST_CTRL_MASK;
		writel(val | (otp_val << HSG_UPCTX_BOOST_CTRL_SHIFT),
				data->hsg_trim_reg + AML_PHY_HSG_TX_CFG2);

		/* set upctx_boost_en in tx_cfg0. */
		val = readl(data->hsg_trim_reg + AML_PHY_HSG_TX_CFG0);
		writel(val | HSG_UPCTX_BOOST_EN, data->hsg_trim_reg + AML_PHY_HSG_TX_CFG0);
		/* set upctx_boost_en in tx_cfg1. */
		val = readl(data->hsg_trim_reg + AML_PHY_HSG_TX_CFG1);
		writel(val | HSG_UPCTX_BOOST_EN, data->hsg_trim_reg + AML_PHY_HSG_TX_CFG1);
	}

	if (!(raw & BIT(16))) {
		dev_err(dev, "No HSGMII PHY RX impedance trim? Skip.\n");
	} else {
		/* set usb3_upc_reg_dxxx in rx_cfg1. */
		otp_val = (raw & (u32)GENMASK(15, 12)) >> 12;
		val = readl(data->hsg_trim_reg + AML_PHY_HSG_RX_CFG1);
		val &= ~HSG_DCHA_AFE_UPCRX_MASK;
		writel(val | (otp_val << HSG_DCHA_AFE_UPCRX_SHIFT),
				data->hsg_trim_reg + AML_PHY_HSG_RX_CFG1);
	}

	return 0;
}

static int amlogic_hsgmii_phy_power_on(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);
	struct device *dev = phy->dev.parent;
	int ret;

	/* HSGMII PMA default parameters optimization */
	/* RX */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG1, 0xE9943075); /* reg_dcha_afe */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG8, 0x5C144268); /* reg_dcha_dfe_h */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG9, 0x2C0014D4); /* reg_dchd_cdr_h */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG2, 0x54164268); /* reg_dcha_dfe */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG3, 0x2C001CD4); /* reg_dchd_cdr */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG4, 0x2011104F); /* reg_dchd_eq */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG0, 0x00007600); /* reg_misc */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG7, 0x0078E80A); /* upcrx_eq_byp_val */
	/* TX */
	regmap_write(data->regmap, AML_PHY_HSG_TX_CFG0, 0x001D3C5A); /* 2.5G */
	regmap_write(data->regmap, AML_PHY_HSG_TX_CFG1, 0x001DFC5A); /* 1G */
	/* FSM ctrl */
	aml_regmap_update_bits(data->regmap, AML_PHY_HSG_BYPASS_PLL_CTRL,
			       HSG_BYPASS_PLL_CTRL_DEBUG_MODE,
			       0);

	if (data->psl_mode) { /* PHY Sync Link mode */
		aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
				HSG_INIT_CTRL_INITIAL_MODE,
				HSG_INIT_CTRL_INITIAL_MODE);
		/* Config timer to wait phy speed stable */
		aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
				HSG_INIT_CTRL_SPEED_DETECT_TIMER_MASK,
				HSG_WAIT_SPEED_DETECT_TIMER <<
				HSG_INIT_CTRL_SPEED_DETECT_TIMER_SHIFT);
		/* Force enable wait speed stable */
		aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
				HSG_INIT_CTRL_WAIT_SPEED_EN,
				HSG_INIT_CTRL_WAIT_SPEED_EN);
	} else { /* Fast Link mode */
		aml_regmap_update_bits(data->regmap, AML_PHY_HSG_INIT_CTRL,
				HSG_INIT_CTRL_INITIAL_MODE,
				0);

		if (data->rx_fsm_restart)
			aml_regmap_update_bits(data->regmap, AML_PHY_HSG_RX_FSM_CTRL,
					HSG_RX_FSM_CTRL_RESTART,
					HSG_RX_FSM_CTRL_RESTART);
		if (data->pll_fsm_restart)
			aml_regmap_update_bits(data->regmap, AML_PHY_HSG_PLL_FSM_CTRL,
					HSG_PLL_FSM_CTRL_RESTART,
					HSG_PLL_FSM_CTRL_RESTART);
	}

	ret = amlogic_hsgmii_phy_trim(phy);
	if (ret)
		dev_err(dev, "failed to trim HSGMII PHY: %d\n", ret);

	amlogic_hsgmii_phy_reset(phy);

	/* Switch u3hsg combo to select HSGMII mode */
	if (data->u3hsg_combo_reg)
		writel(data->combo_hsg_sel_val, data->u3hsg_combo_reg);

	return 0;
}

static int amlogic_hsgmii_phy_power_off(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);

	/* bypass FSM ctrl */
	aml_regmap_update_bits(data->regmap, AML_PHY_HSG_BYPASS_PLL_CTRL,
			       HSG_BYPASS_PLL_CTRL_DEBUG_MODE,
			       HSG_BYPASS_PLL_CTRL_DEBUG_MODE);
	/* PLL */
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_HCSL_H_REG0, 0x80899788); /* 2.5G */
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_H_REG0, 0x02EE55E9);
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_H_REG1, 0x002A4085);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_H_REG0, 0x40000071);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_H_REG1, 0x1842A320);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_H_REG2, 0x00000050);
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_HCSL_L_REG0, 0x80899788); /* 1G */
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_L_REG0, 0x01F455E9);
	regmap_write(data->regmap, AML_PHY_HSG_PLL0_L_REG1, 0x0009E7E5);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_L_REG0, 0x40000071);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_L_REG1, 0x1842A320);
	regmap_write(data->regmap, AML_PHY_HSG_PLL1_L_REG2, 0x00000050);
	/* RX */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG4, 0x00000000);
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG9, 0x00000000); /* 2.5G */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG8, 0x00000200);
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG3, 0x00000000); /* 1G */
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG2, 0x00000200);
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG1, 0x00000075);
	regmap_write(data->regmap, AML_PHY_HSG_RX_CFG0, 0x0000F600);
	/* TX */
	regmap_write(data->regmap, AML_PHY_HSG_TX_CFG0, 0x001800B0); /* 2.5G */
	regmap_write(data->regmap, AML_PHY_HSG_TX_CFG1, 0x001800B0); /* 1G */

	return 0;
}

static int amlogic_hsgmii_phy_set_speed(struct phy *phy, int speed)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);

	if (speed != data->speed)
		data->speed = speed;

	return amlogic_hsgmii_phy_calibrate(phy);
}

static int amlogic_hsgmii_phy_init(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);
	int ret;

	ret = clk_prepare_enable(data->hsg_general_clk);
	if (ret)
		return ret;

	ret = reset_control_reset(data->hsg_reset);
	if (ret) {
		clk_disable_unprepare(data->hsg_general_clk);
		return ret;
	}

	udelay(HSG_RESET_TIME_US);

	return 0;
}

static int amlogic_hsgmii_phy_exit(struct phy *phy)
{
	struct amlogic_hsgmii_phy_data *data = phy_get_drvdata(phy);
	int ret;

	ret = reset_control_reset(data->hsg_reset);
	if (!ret)
		clk_disable_unprepare(data->hsg_general_clk);

	return ret;
}

static const struct phy_ops amlogic_hsgmii_phy_ops = {
	.init		= amlogic_hsgmii_phy_init,
	.exit		= amlogic_hsgmii_phy_exit,
	.reset		= amlogic_hsgmii_phy_reset,
	.power_on	= amlogic_hsgmii_phy_power_on,
	.power_off	= amlogic_hsgmii_phy_power_off,
	.set_speed	= amlogic_hsgmii_phy_set_speed,
	.calibrate	= amlogic_hsgmii_phy_calibrate,
	.owner		= THIS_MODULE,
};

static const struct regmap_config amlogic_hsgmii_phy_regmap_cfg = {
	.reg_bits		= 32,
	.val_bits		= 32,
	.reg_stride		= 4,
	.use_relaxed_mmio	= true,
	.disable_locking	= true,
};

static void amlogic_serdes_config_dt(struct platform_device *pdev,
		struct amlogic_hsgmii_phy_data *data)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	void __iomem *addr = NULL;
	unsigned int reg;

	data->psl_mode = of_property_read_bool(np, "amlogic,psl-mode");
	data->rx_fsm_restart = of_property_read_bool(np, "amlogic,rx-fsm-restart");
	data->pll_fsm_restart = of_property_read_bool(np, "amlogic,pll-fsm-restart");
	if (data->psl_mode && (data->rx_fsm_restart || data->pll_fsm_restart))
		dev_warn(dev, "rx_fsm_restart and pll_fsm_restart have no effect in PSL mode\n");

	if (of_property_read_u32(np, "u3hsg-combo-reg", &reg) == 0) {
		hsg_debug("u3hsg combo reg=0x%x\n", reg);
		addr = devm_ioremap(dev, reg, 4);
		if (IS_ERR_OR_NULL(addr)) {
			dev_err(dev, "failed to request u3hsg combo reg\n");
			data->u3hsg_combo_reg = NULL;
		} else {
			data->u3hsg_combo_reg = addr;
			if (of_property_read_u32(np,
					"combo-hsg-sel-val", &data->combo_hsg_sel_val) != 0)
				data->combo_hsg_sel_val = 0;
			hsg_debug("combo_hsg_sel_val=0x%x\n", data->combo_hsg_sel_val);
		}
	} else {
		data->u3hsg_combo_reg = NULL;
	}

	if (of_property_read_u32(np, "hsg-trim-reg", &reg) == 0) {
		hsg_debug("hsg trim reg=0x%x\n", reg);
		addr = devm_ioremap(dev, reg, 4);
		if (IS_ERR_OR_NULL(addr)) {
			dev_err(dev, "failed to request hsg trim reg\n");
			data->hsg_trim_reg = NULL;
		} else {
			data->hsg_trim_reg = addr;
		}
	} else {
		data->hsg_trim_reg = NULL;
	}
}

static int amlogic_hsgmii_phy_probe(struct platform_device *pdev)
{
	struct amlogic_hsgmii_phy_data *data;
	struct device *dev = &pdev->dev;
	struct phy *phy;
	struct phy_provider *provider;
	void __iomem *base;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	data->hsg_general_clk = devm_clk_get(dev, "hsg_general");
	if (IS_ERR(data->hsg_general_clk))
		return PTR_ERR(data->hsg_general_clk);

	data->hsg_reset = devm_reset_control_get_exclusive(dev, "hsg_rst");
	if (IS_ERR(data->hsg_reset))
		return PTR_ERR(data->hsg_reset);

	data->hsg_apb_reset = devm_reset_control_get_exclusive(dev, "hsg_apb_rst");
	if (IS_ERR(data->hsg_apb_reset))
		return PTR_ERR(data->hsg_apb_reset);

	data->regmap = devm_regmap_init_mmio(dev, base,
					     &amlogic_hsgmii_phy_regmap_cfg);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	data->speed = SPEED_1000;

	amlogic_serdes_config_dt(pdev, data);

	phy = devm_phy_create(dev, NULL, &amlogic_hsgmii_phy_ops);
	if (IS_ERR(phy))
		return PTR_ERR(phy);

	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(provider))
		return PTR_ERR(provider);

	phy_set_drvdata(phy, data);
	dev_set_drvdata(dev, data);

	return 0;
}

#ifdef HSG_PHY_DEBUG
#define HSG_PRINT_REG(var, ex) do { \
	int _ret = snprintf(buf + len, PAGE_SIZE - len, \
			"\n=== " #ex " (0x%03x) = 0x%08x ===\n", \
			AML_PHY_HSG_##ex, (stats->var).val); \
	len += _ret; \
} while (0)

#define HSG_PRINT_FIELD(var, field) do { \
	int _ret = snprintf(buf + len, PAGE_SIZE - len, \
			"  %-24s = %u\n", \
			#field, ((stats->var).reg).field); \
	len += _ret; \
} while (0)

static void hsg_update_debug_stats(struct amlogic_hsgmii_phy_data *data)
{
	struct hsg_phy_stats *stats = &data->hsg_stats;

	regmap_read(data->regmap, AML_PHY_HSG_MAC_STATUS, &stats->mac_status.val);
	regmap_read(data->regmap, AML_PHY_HSG_INIT_DEBUG, &stats->init_dbg.val);
	regmap_read(data->regmap, AML_PHY_HSG_PLL_INIT_DEBUG, &stats->pll_dbg.val);
	regmap_read(data->regmap, AML_PHY_HSG_RX_CTRL_DEBUG, &stats->rx_ctrl_dbg.val);
	regmap_read(data->regmap, AML_PHY_HSG_RX_DEBUG, &stats->rx_dbg.val);
}

static ssize_t status_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct amlogic_hsgmii_phy_data *data = dev_get_drvdata(dev);
	struct hsg_phy_stats *stats = &data->hsg_stats;
	ssize_t len = 0;

	hsg_update_debug_stats(data);

	len += snprintf(buf + len, PAGE_SIZE - len, "=== PHY Config ===\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "Speed: %d Mbps\n", data->speed);
	len += snprintf(buf + len, PAGE_SIZE - len, "PSL Mode: %s\n",
			data->psl_mode ? "PHY Sync" : "Fast Link");
	len += snprintf(buf + len, PAGE_SIZE - len, "Rx Fsm Restart Mode: %s\n",
			data->rx_fsm_restart ? "Y" : "N");
	len += snprintf(buf + len, PAGE_SIZE - len, "PLL Fsm Restart Mode: %s\n",
			data->pll_fsm_restart ? "Y" : "N");

	/* MAC Status */
	HSG_PRINT_REG(mac_status, MAC_STATUS);
	HSG_PRINT_FIELD(mac_status, rx_fsm_ready);
	HSG_PRINT_FIELD(mac_status, pcs_acquired_sync);
	HSG_PRINT_FIELD(mac_status, sgmii_link_spd);
	HSG_PRINT_FIELD(mac_status, pcs_ewrap);
	HSG_PRINT_FIELD(mac_status, pcs_en_cdet);
	HSG_PRINT_FIELD(mac_status, pcs_lck_ref);

	/* INIT Debug */
	HSG_PRINT_REG(init_dbg, INIT_DEBUG);
	HSG_PRINT_FIELD(init_dbg, hsgmii_mode_rst_n);
	HSG_PRINT_FIELD(init_dbg, hsgmii_clk_en);
	HSG_PRINT_FIELD(init_dbg, phy_link_status_final);
	HSG_PRINT_FIELD(init_dbg, hsgmii_speed);
	HSG_PRINT_FIELD(init_dbg, phy_speed_final);
	HSG_PRINT_FIELD(init_dbg, hsi_cstate);

	/* PLL Debug */
	HSG_PRINT_REG(pll_dbg, PLL_INIT_DEBUG);
	HSG_PRINT_FIELD(pll_dbg, pll0_lock);
	HSG_PRINT_FIELD(pll_dbg, pll1_lock);
	HSG_PRINT_FIELD(pll_dbg, pll0_fail_cnt);
	HSG_PRINT_FIELD(pll_dbg, pll1_fail_cnt);
	HSG_PRINT_FIELD(pll_dbg, pll_cstate);

	/* RX Ctrl Debug */
	HSG_PRINT_REG(rx_ctrl_dbg, RX_CTRL_DEBUG);
	HSG_PRINT_FIELD(rx_ctrl_dbg, pcs_acquired_sync_cdc);
	HSG_PRINT_FIELD(rx_ctrl_dbg, upcrx_cdlock_sync);
	HSG_PRINT_FIELD(rx_ctrl_dbg, rx_fsm_ready);
	HSG_PRINT_FIELD(rx_ctrl_dbg, lock_fail_cnt);
	HSG_PRINT_FIELD(rx_ctrl_dbg, rx_fsm_cstate);

	/* RX Debug */
	HSG_PRINT_REG(rx_dbg, RX_DEBUG);
	HSG_PRINT_FIELD(rx_dbg, upcrx_state);
	HSG_PRINT_FIELD(rx_dbg, upcrx_dbg_eq);

	return len;
}

static DEVICE_ATTR_RO(status);

static struct attribute *hsgmii_phy_dev_attrs[] = {
	&dev_attr_status.attr,
	NULL,
};
ATTRIBUTE_GROUPS(hsgmii_phy_dev);
#endif

static const struct of_device_id amlogic_hsgmii_phy_of_match[] = {
	{ .compatible = "amlogic,hsgmii-phy" },
	{ },
};
MODULE_DEVICE_TABLE(of, amlogic_hsgmii_phy_of_match);

static struct platform_driver amlogic_hsgmii_phy_driver = {
	.probe	= amlogic_hsgmii_phy_probe,
	.driver = {
		.name	= "amlogic-hsgmii-phy",
		.of_match_table	= amlogic_hsgmii_phy_of_match,
#ifdef HSG_PHY_DEBUG
		.dev_groups = hsgmii_phy_dev_groups,
#endif
	},
};

module_platform_driver(amlogic_hsgmii_phy_driver);

MODULE_DESCRIPTION("Amlogic HSGMII PHY driver");
MODULE_LICENSE("GPL");
