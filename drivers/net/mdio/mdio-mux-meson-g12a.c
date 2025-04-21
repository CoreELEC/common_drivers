// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mdio-mux.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/platform_device.h>

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
#include <linux/phy.h>
#include <linux/of_mdio.h>
#include <linux/amlogic/aml_phy_debug.h>
#endif

#define ETH_PHY_DBG_CFG0	0x08
#define ETH_PLL_STS		0x40
#define ETH_PLL_CTL0		0x44
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
#define  PLL_CTL0_LOCK_FLAG	BIT(31)
#endif
#define  PLL_CTL0_LOCK_DIG	BIT(30)
#define  PLL_CTL0_RST		BIT(29)
#define  PLL_CTL0_EN		BIT(28)
#define  PLL_CTL0_SEL		BIT(23)
#define  PLL_CTL0_N		GENMASK(14, 10)
#define  PLL_CTL0_M		GENMASK(8, 0)
#define  PLL_LOCK_TIMEOUT	1000000
#define  PLL_MUX_NUM_PARENT	2
#define ETH_PLL_CTL1		0x48
#define ETH_PLL_CTL2		0x4c
#define ETH_PLL_CTL3		0x50
#define ETH_PLL_CTL4		0x54
#define ETH_PLL_CTL5		0x58
#define ETH_PLL_CTL6		0x5c
#define ETH_PLL_CTL7		0x60

#define ETH_PHY_CNTL0		0x80
#define   EPHY_G12A_ID		0x33010180
#define ETH_PHY_CNTL1		0x84
#define  PHY_CNTL1_ST_MODE	GENMASK(2, 0)
#define  PHY_CNTL1_ST_PHYADD	GENMASK(7, 3)
#define   EPHY_DFLT_ADD		8
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
#define  PHY_CNTL1_AUTOMDIX_EN	BIT(8)
#endif
#define  PHY_CNTL1_MII_MODE	GENMASK(15, 14)
#define   EPHY_MODE_MII		0x0
#define   EPHY_MODE_RMII	0x1
#define  PHY_CNTL1_CLK_EN	BIT(16)
#define  PHY_CNTL1_CLKFREQ	BIT(17)
#define  PHY_CNTL1_PHY_ENB	BIT(18)
#define ETH_PHY_CNTL2		0x88
#define  PHY_CNTL2_USE_INTERNAL	BIT(5)
#define  PHY_CNTL2_SMI_SRC_MAC	BIT(6)
#define  PHY_CNTL2_RX_DV_COL	BIT(7)
#define  PHY_CNTL2_RX_CLK_EPHY	BIT(9)

#define ETH_PHY_CLK_GATE	0x144

#define MESON_G12A_MDIO_EXTERNAL_ID 0
#define MESON_G12A_MDIO_INTERNAL_ID 1

struct g12a_mdio_mux {
	bool pll_is_enabled;
	void __iomem *regs;
	void *mux_handle;
	struct clk *pclk;
	struct clk *pll;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	struct device *dev;
	struct clk *ethrmii;
#endif
};

struct g12a_ephy_pll {
	void __iomem *base;
	struct clk_hw hw;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	struct aml_eth_priv *eth_priv;
#endif
};

#define g12a_ephy_pll_to_dev(_hw)			\
	container_of(_hw, struct g12a_ephy_pll, hw)

static unsigned long g12a_ephy_pll_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct g12a_ephy_pll *pll = g12a_ephy_pll_to_dev(hw);
	u32 val, m, n;

	val = readl(pll->base + ETH_PLL_CTL0);
	m = FIELD_GET(PLL_CTL0_M, val);
	n = FIELD_GET(PLL_CTL0_N, val);

	return parent_rate * m / n;
}

static int g12a_ephy_pll_enable(struct clk_hw *hw)
{
	struct g12a_ephy_pll *pll = g12a_ephy_pll_to_dev(hw);
	u32 val = readl(pll->base + ETH_PLL_CTL0);

	/* Apply both enable an reset */
	val |= PLL_CTL0_RST | PLL_CTL0_EN;
	writel(val, pll->base + ETH_PLL_CTL0);

	/* Clear the reset to let PLL lock */
	val &= ~PLL_CTL0_RST;
	writel(val, pll->base + ETH_PLL_CTL0);

	/* Poll on the digital lock instead of the usual analog lock
	 * This is done because bit 31 is unreliable on some SoC. Bit
	 * 31 may indicate that the PLL is not lock even though the clock
	 * is actually running
	 */
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)

	if (pll->eth_priv->phy_pll_mode == 1 || pll->eth_priv->phy_pll_mode == 4) {
		return readl_poll_timeout(pll->base + ETH_PLL_CTL0, val,
				  val & PLL_CTL0_LOCK_FLAG, 0, PLL_LOCK_TIMEOUT);
	}
	return readl_poll_timeout(pll->base + ETH_PLL_CTL0, val,
				  val & PLL_CTL0_LOCK_DIG, 0, PLL_LOCK_TIMEOUT);
#else
	return readl_poll_timeout(pll->base + ETH_PLL_CTL0, val,
				  val & PLL_CTL0_LOCK_DIG, 0, PLL_LOCK_TIMEOUT);
#endif
}

static void g12a_ephy_pll_disable(struct clk_hw *hw)
{
	struct g12a_ephy_pll *pll = g12a_ephy_pll_to_dev(hw);
	u32 val;

	val = readl(pll->base + ETH_PLL_CTL0);
	val &= ~PLL_CTL0_EN;
	val |= PLL_CTL0_RST;
	writel(val, pll->base + ETH_PLL_CTL0);
}

static int g12a_ephy_pll_is_enabled(struct clk_hw *hw)
{
	struct g12a_ephy_pll *pll = g12a_ephy_pll_to_dev(hw);
	unsigned int val;

	val = readl(pll->base + ETH_PLL_CTL0);

	return (val & PLL_CTL0_LOCK_DIG) ? 1 : 0;
}

static int g12a_ephy_pll_init(struct clk_hw *hw)
{
	struct g12a_ephy_pll *pll = g12a_ephy_pll_to_dev(hw);
	/* Apply PLL HW settings */
	/*12nm*/

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	if (pll->eth_priv->phy_pll_mode == 0) {
		writel(0x29c0040a, pll->base + ETH_PLL_CTL0);
		writel(0x927e0000, pll->base + ETH_PLL_CTL1);
		writel(0xac5f49e5, pll->base + ETH_PLL_CTL2);
		writel(0x00000000, pll->base + ETH_PLL_CTL3);
		writel(0x00000000, pll->base + ETH_PLL_CTL4);
		writel(0x20200000, pll->base + ETH_PLL_CTL5);
		writel(0x0000cf02, pll->base + ETH_PLL_CTL6);
		writel(0x00000023, pll->base + ETH_PLL_CTL7);
	}
	/*22nm*/
	if (pll->eth_priv->phy_pll_mode == 1) {
		writel(0x608200a0, pll->base + ETH_PLL_CTL0);
		writel(0xea002000, pll->base + ETH_PLL_CTL1);
		writel(0x00000150, pll->base + ETH_PLL_CTL2);
		writel(0x00000000, pll->base + ETH_PLL_CTL3);
		writel(0x708200a0, pll->base + ETH_PLL_CTL0);
		usleep_range(100, 200);
		writel(0x508200a0, pll->base + ETH_PLL_CTL0);
		writel(0x00000110, pll->base + ETH_PLL_CTL2);
	}
	/*s7*/
	if (pll->eth_priv->phy_pll_mode == 2) {
		writel(0x00510630, pll->base + ETH_PLL_CTL0);
		writel(0x222210a0, pll->base + ETH_PLL_CTL1);
		writel(0x00518630, pll->base + ETH_PLL_CTL0);
		usleep_range(100, 200);
		writel(0x222200a0, pll->base + ETH_PLL_CTL1);
		usleep_range(100, 200);
		writel(0x00118630, pll->base + ETH_PLL_CTL0);
	}
	/*s7d*/
	if (pll->eth_priv->phy_pll_mode == 3) {
		writel(readl(pll->base + ETH_PLL_CTL3) & 0xfffffffc, pll->base + ETH_PLL_CTL3);
		writel(0x00c091a2, pll->base + ETH_PLL_CTL0);
		writel(0x01111140, pll->base + ETH_PLL_CTL1);
		writel(readl(pll->base + ETH_PLL_CTL3) | 0x2, pll->base + ETH_PLL_CTL3);
		usleep_range(100, 200);
		writel(readl(pll->base + ETH_PLL_CTL3) | 0x3, pll->base + ETH_PLL_CTL3);
		usleep_range(100, 200);
		writel(0x00e091a2, pll->base + ETH_PLL_CTL0);
		usleep_range(100, 200);
		writel(0x00007423, pll->base + ETH_PLL_CTL7);
	}
	/*t6x-24m*/
	if (pll->eth_priv->phy_pll_mode == 4) {
		writel(0x07d21003, pll->base + ETH_PLL_CTL0);
		writel(0x000008c0, pll->base + ETH_PLL_CTL1);
		usleep_range(100, 200);
		writel(0x07d21007, pll->base + ETH_PLL_CTL0);
		usleep_range(100, 200);
		writel(0x07d21006, pll->base + ETH_PLL_CTL0);
		usleep_range(100, 200);
		writel(0x07d21004, pll->base + ETH_PLL_CTL0);
	}
#else

	writel(0x29c0040a, pll->base + ETH_PLL_CTL0);
	writel(0x927e0000, pll->base + ETH_PLL_CTL1);
	writel(0xac5f49e5, pll->base + ETH_PLL_CTL2);
	writel(0x00000000, pll->base + ETH_PLL_CTL3);
	writel(0x00000000, pll->base + ETH_PLL_CTL4);
	writel(0x20200000, pll->base + ETH_PLL_CTL5);
	writel(0x0000c002, pll->base + ETH_PLL_CTL6);
	writel(0x00000023, pll->base + ETH_PLL_CTL7);
#endif
	return 0;
}

static const struct clk_ops g12a_ephy_pll_ops = {
	.recalc_rate	= g12a_ephy_pll_recalc_rate,
	.is_enabled	= g12a_ephy_pll_is_enabled,
	.enable		= g12a_ephy_pll_enable,
	.disable	= g12a_ephy_pll_disable,
	.init		= g12a_ephy_pll_init,
};

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
static struct aml_eth_priv *get_eth_priv(struct device_node *np)
{
	struct device_node *parent_bus_node;
	struct mii_bus *parent_bus;
	struct aml_eth_priv *eth_priv;
	int ret = 0;

	parent_bus_node = of_parse_phandle(np, "mdio-parent-bus", 0);
	if (!parent_bus_node)
		return ERR_PTR(-ENODEV);

	parent_bus = of_mdio_find_bus(parent_bus_node);
	if (!parent_bus) {
		ret = -EPROBE_DEFER;
		goto out0;
	}

	eth_priv = aml_get_eth_priv_by_ndev(parent_bus->parent);

	put_device(&parent_bus->dev);
out0:
	of_node_put(parent_bus_node);

	if (ret)
		return ERR_PTR(ret);

	return eth_priv;
}
#endif

static int g12a_enable_internal_mdio(struct g12a_mdio_mux *priv)
{
	u32 value;
	int ret;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	void __iomem *tx_amp_src = NULL;
	struct device_node *np = priv->dev->of_node;
	struct aml_eth_priv *eth_priv;
	unsigned int tx_amp_addr = 0;
	unsigned int st_mode = 0;
	unsigned int rx_R = 0;
	unsigned int tx_R = 0;
	unsigned int efuse_get_tmp = 0;
	unsigned int led_setting = 0;
	unsigned int ephy_clk_gate = 0;

	eth_priv = get_eth_priv(np);
	if (IS_ERR_OR_NULL(eth_priv))
		return PTR_ERR(eth_priv);

	if (of_property_read_u32(np, "tx_amp_src", &tx_amp_addr) != 0)
		pr_info("no amp setting\n");

	if (tx_amp_addr != 0) {
		pr_info("tx_amp_addr %x\n", tx_amp_addr);
		tx_amp_src = devm_ioremap(priv->dev,
				(resource_size_t)tx_amp_addr, sizeof(resource_size_t));

		pr_info("txamp 0x%x\n", readl(tx_amp_src));
		if (((readl(tx_amp_src) & 0x3f) >> 4) & 0x3)
			eth_priv->tx_amp_bl2 = (readl(tx_amp_src) & 0x3f);
		else /*efuse not set use default*/
			eth_priv->tx_amp_bl2 = (0x180018 & 0x3f);

		/* default 0
		 * 1 means voltage phy t3
		 * 2 means voltage phy txhd2
		 */
		if (of_property_read_u32(np, "phy_mode", &eth_priv->phy_mode) == 0) {
			pr_info("phy_mode %d\n", eth_priv->phy_mode);
			/*t3x*/
			if (eth_priv->phy_mode == 1) {
				/*bit[20:16] bit20 valid, bit[19:16] rx value*/
				rx_R = (readl(tx_amp_src) & 0x1f0000);
		//		rx_R = (0x180018 & 0x1f0000);
				if (rx_R >> 0x14) { /*bit20 is valid*/
					pr_info("ETH_RES_CTL %x\n", (((rx_R & 0xf0000) << 8)
							| 0xe0fe0000
							| (readl(priv->regs + ETH_PLL_CTL3)
								& 0xffff)));
					writel((((rx_R & 0xf0000) << 8) | 0xe0fe0000)
						| (readl(priv->regs + ETH_PLL_CTL3) & 0xffff),
						priv->regs + ETH_PLL_CTL3);
				} else { /*efuse not set use default value*/
					pr_debug("no efuse setting use default\n");
					writel(0xe8fe0000, priv->regs + ETH_PLL_CTL3);
				}
			}

			/*txhd2*/
			if (eth_priv->phy_mode == 2) {
				/*bit[24:16]: bit24 valid, bit[23:20] tx, bit[19:16] rx*/
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
//				efuse_get_tmp = 0x1990019;
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					tx_R = (efuse_get_tmp & 0xf00000) >> 20;
					rx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x0802cf01),
						priv->regs + ETH_PLL_CTL6);
				} else {
					pr_debug("no efuse setting use default\n");
					writel(0x8882cf01, priv->regs + ETH_PLL_CTL6);
				}
				writel(0x20220000, priv->regs + ETH_PLL_CTL5);
				/*writel(0x8a82cf01, priv->regs + ETH_PLL_CTL6);*/
				writel(0x00000023, priv->regs + ETH_PLL_CTL7);
			}

			/*s1a*/
			if (eth_priv->phy_mode == 3) {
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					tx_R = (efuse_get_tmp & 0xf00000) >> 20;
					rx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x08020000),
						priv->regs + ETH_PLL_CTL3);
				} else {
					pr_debug("no efuse setting use default\n");
					writel(0xa8820000, priv->regs + ETH_PLL_CTL3);
				}
				writel(0xcf01, priv->regs + ETH_PLL_CTL6);
				writel(0x20220000, priv->regs + ETH_PLL_CTL5);
				writel(0x00000023, priv->regs + ETH_PLL_CTL7);
			}

			/*s7*/
			if (eth_priv->phy_mode == 4) {
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					rx_R = (efuse_get_tmp & 0xf00000) >> 20;
					tx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x0a060000),
						priv->regs + ETH_PLL_CTL3);
				} else {
					pr_debug("no efuse setting use default\n");
					writel(0xaa860000, priv->regs + ETH_PLL_CTL3);
				}
				writel(0x4001, priv->regs + ETH_PLL_CTL6);
				writel(0x20000000, priv->regs + ETH_PLL_CTL5);
				writel(0x00000023, priv->regs + ETH_PLL_CTL7);
			}
			/*s7d s6*/
			if (eth_priv->phy_mode == 5 || eth_priv->phy_mode == 6) {
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					rx_R = (efuse_get_tmp & 0xf00000) >> 20;
					tx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x05000003),
						priv->regs + ETH_PLL_CTL3);
				} else {
					pr_debug("no efuse setting use default\n");
					writel((readl(priv->regs + ETH_PLL_CTL3) & 0x3) |
						(0x8580 << 16),
						priv->regs + ETH_PLL_CTL3);
				}
				writel(0x4001, priv->regs + ETH_PLL_CTL6);
				writel(0x84001580, priv->regs + ETH_PLL_CTL5);
//				writel(0x00007423, priv->regs + ETH_PLL_CTL7);
			}
			/*t6d*/
			if (eth_priv->phy_mode == 7) {
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					rx_R = (efuse_get_tmp & 0xf00000) >> 20;
					tx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x08060000),
						priv->regs + ETH_PLL_CTL3);
				} else {
					pr_debug("no efuse setting use default\n");
					writel(0xa8860000, priv->regs + ETH_PLL_CTL3);
				}
				writel(0x4001, priv->regs + ETH_PLL_CTL6);
				writel(0x20000000, priv->regs + ETH_PLL_CTL5);
				writel(0x00003623, priv->regs + ETH_PLL_CTL7);
			}
			/*t6x*/
			if (eth_priv->phy_mode == 8) {
				efuse_get_tmp = (readl(tx_amp_src) & 0x1ff0000);
				if (efuse_get_tmp >> 0x18) { /*bit24 is valid*/
					rx_R = (efuse_get_tmp & 0xf00000) >> 20;
					tx_R = (efuse_get_tmp & 0x0f0000) >> 16;
					writel(((tx_R << 28) | (rx_R << 20))
						| (0x09060000),
						priv->regs + ETH_PLL_CTL3);
				} else {
					pr_info("no efuse setting use default\n");
					writel(0xa9860000, priv->regs + ETH_PLL_CTL3);
				}
				writel(0x4001, priv->regs + ETH_PLL_CTL6);
				writel(0x10800000, priv->regs + ETH_PLL_CTL5);
				writel(0x00003623, priv->regs + ETH_PLL_CTL7);
			}
		}
	}

	if (of_property_read_u32(np, "st_mode", &st_mode) != 0) {
		pr_info("use default st_mode\n");
		st_mode = 7;
	}
	if (of_property_read_u32(np, "ephy_clk_gate", &ephy_clk_gate) == 0) {
		pr_info("config ephy_clk_gate as 0x%x\n", ephy_clk_gate);
		/* ephy_clk_gate for internal phy */
		writel(ephy_clk_gate, priv->regs + ETH_PHY_CLK_GATE);
	}
	if (of_property_read_u32(np, "ephy_eee_support", &eth_priv->ephy_eee_support) == 0) {
		if (eth_priv->ephy_eee_support)
			pr_info("inphy EEE Feature supported\n");
	} else {
		eth_priv->ephy_eee_support = 0;
	}
	if (of_property_read_u32(np, "led_setting", &led_setting) == 0) {
		/*led setting 10bit ETH_PHY_CNTL0 bit[31:23] and ETH_PLL_CTL4 bit0*/
		/*led_cfg_1 in ETH_PLL_CTL4*/
		if ((led_setting >> 9) == 1)
			writel(0x1, priv->regs + ETH_PLL_CTL4);
		/*ETH_PHY_CNTL0 bit[31:23]*/
		led_setting = ((led_setting & 0x1ff) << 23);
		pr_info("led setting 0x%x\n", led_setting);
	}
#endif
	/* Enable the phy clock */
	if (!priv->pll_is_enabled) {
		ret = clk_prepare_enable(priv->pll);
		if (ret)
			return ret;
	}

	priv->pll_is_enabled = true;

	/* Initialize ephy control */
	writel(EPHY_G12A_ID, priv->regs + ETH_PHY_CNTL0);
	/* Make sure we get a 0 -> 1 transition on the enable bit */
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	value = FIELD_PREP(PHY_CNTL1_ST_MODE, st_mode) |
#else
	value = FIELD_PREP(PHY_CNTL1_ST_MODE, 3) |
#endif
		FIELD_PREP(PHY_CNTL1_ST_PHYADD, EPHY_DFLT_ADD) |
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
		PHY_CNTL1_AUTOMDIX_EN |
#endif
		FIELD_PREP(PHY_CNTL1_MII_MODE, EPHY_MODE_RMII) |
		PHY_CNTL1_CLK_EN |
		PHY_CNTL1_CLKFREQ;
	writel(value, priv->regs + ETH_PHY_CNTL1);
	writel(PHY_CNTL2_USE_INTERNAL |
	       PHY_CNTL2_SMI_SRC_MAC |
	       PHY_CNTL2_RX_CLK_EPHY,
	       priv->regs + ETH_PHY_CNTL2);
	if (eth_priv->ephy_eee_support && eth_priv->inphy_eee_enable) {
		writel(readl(priv->regs + ETH_PHY_CNTL2) |
			PHY_CNTL2_RX_DV_COL,
			priv->regs + ETH_PHY_CNTL2); // cover for mii
		value &= ~PHY_CNTL1_MII_MODE; // cover for mii
	}
	value |= PHY_CNTL1_PHY_ENB;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	value |= led_setting;
	if (eth_priv->phy_mode == 5)
		value |= 0x200000;
	writel(value, priv->regs + ETH_PHY_CNTL1);
#endif
	/* The phy needs a bit of time to power up */
	mdelay(10);
	return 0;
}

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
int g12a_resume_enable_internal_mdio(struct device *mdio_dev)
{
	struct g12a_mdio_mux *priv = dev_get_drvdata(mdio_dev);

	g12a_ephy_pll_init(__clk_get_hw(priv->pll));
	g12a_ephy_pll_enable(__clk_get_hw(priv->pll));

	return g12a_enable_internal_mdio(priv);
}
EXPORT_SYMBOL_GPL(g12a_resume_enable_internal_mdio);
#endif

static int g12a_enable_external_mdio(struct g12a_mdio_mux *priv)
{
	/* Reset the mdio bus mux */
	writel_relaxed(0x0, priv->regs + ETH_PHY_CNTL2);

	/* Disable the phy clock if enabled */
	if (priv->pll_is_enabled) {
		clk_disable_unprepare(priv->pll);
		priv->pll_is_enabled = false;
	}

	return 0;
}

static int g12a_mdio_switch_fn(int current_child, int desired_child,
			       void *data)
{
	struct g12a_mdio_mux *priv = dev_get_drvdata(data);
	if (current_child == desired_child)
		return 0;

	switch (desired_child) {
	case MESON_G12A_MDIO_EXTERNAL_ID:
		return g12a_enable_external_mdio(priv);
	case MESON_G12A_MDIO_INTERNAL_ID:
		return g12a_enable_internal_mdio(priv);
	default:
		return -EINVAL;
	}
}

static const struct of_device_id g12a_mdio_mux_match[] = {
	{ .compatible = "amlogic,g12a-mdio-mux", },
	{},
};
MODULE_DEVICE_TABLE(of, g12a_mdio_mux_match);

static int g12a_ephy_glue_clk_register(struct device *dev)
{
	struct g12a_mdio_mux *priv = dev_get_drvdata(dev);
	const char *parent_names[PLL_MUX_NUM_PARENT];
	struct clk_init_data init;
	struct g12a_ephy_pll *pll;
	struct clk_mux *mux;
	struct clk *clk;
	char *name;
	int i;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	struct device_node *np = dev->of_node;
	struct aml_eth_priv *eth_priv;

	eth_priv = get_eth_priv(np);
	if (IS_ERR_OR_NULL(eth_priv))
		return PTR_ERR(eth_priv);
#endif

	/* get the mux parents */
	for (i = 0; i < PLL_MUX_NUM_PARENT; i++) {
		char in_name[8];

		snprintf(in_name, sizeof(in_name), "clkin%d", i);
		clk = devm_clk_get(dev, in_name);
		if (IS_ERR(clk)) {
			if (PTR_ERR(clk) != -EPROBE_DEFER)
				dev_err(dev, "Missing clock %s\n", in_name);
			return PTR_ERR(clk);
		}

		parent_names[i] = __clk_get_name(clk);
	}

	/* create the input mux */
	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return -ENOMEM;

	name = kasprintf(GFP_KERNEL, "%s#mux", dev_name(dev));
	if (!name)
		return -ENOMEM;

	init.name = name;
	init.ops = &clk_mux_ro_ops;
	init.flags = 0;
	init.parent_names = parent_names;
	init.num_parents = PLL_MUX_NUM_PARENT;

	mux->reg = priv->regs + ETH_PLL_CTL0;
	mux->shift = __ffs(PLL_CTL0_SEL);
	mux->mask = PLL_CTL0_SEL >> mux->shift;
	mux->hw.init = &init;

	clk = devm_clk_register(dev, &mux->hw);
	kfree(name);
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to register input mux\n");
		return PTR_ERR(clk);
	}

	/* create the pll */
	pll = devm_kzalloc(dev, sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return -ENOMEM;

	name = kasprintf(GFP_KERNEL, "%s#pll", dev_name(dev));
	if (!name)
		return -ENOMEM;

	init.name = name;
	init.ops = &g12a_ephy_pll_ops;
	init.flags = 0;
	parent_names[0] = __clk_get_name(clk);
	init.parent_names = parent_names;
	init.num_parents = 1;

	pll->base = priv->regs;
	pll->hw.init = &init;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	pll->eth_priv = eth_priv;
#endif

	clk = devm_clk_register(dev, &pll->hw);
	kfree(name);
	if (IS_ERR(clk)) {
		dev_err(dev, "failed to register input mux\n");
		return PTR_ERR(clk);
	}

	priv->pll = clk;

	return 0;
}

static int g12a_mdio_mux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct g12a_mdio_mux *priv;
	int ret;
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	struct device_node *np = dev->of_node;
	struct aml_eth_priv *eth_priv;

	eth_priv = get_eth_priv(np);
	if (IS_ERR_OR_NULL(eth_priv))
		return PTR_ERR(eth_priv);
#endif
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	eth_priv->mdio_dev = dev;
#endif
	platform_set_drvdata(pdev, priv);

	priv->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	eth_priv->phy_analog_config_addr = priv->regs;
	priv->dev = dev;
	if (of_property_read_u32(np, "phy_pll_mode", &eth_priv->phy_pll_mode) != 0)
		pr_debug("default as 12nm setting\n");
#endif
	priv->pclk = devm_clk_get(dev, "pclk");
	if (IS_ERR(priv->pclk)) {
		ret = PTR_ERR(priv->pclk);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to get peripheral clock\n");
		return ret;
	}

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
	priv->ethrmii = devm_clk_get(dev, "ethrmii");
	if (IS_ERR(priv->ethrmii)) {
		ret = PTR_ERR(priv->ethrmii);
		if (ret == -EPROBE_DEFER)
			dev_info(dev, "probe will defer due to ethrmii clock\n");
	} else {
		ret = clk_prepare_enable(priv->ethrmii);
		if (ret)
			dev_err(dev, "failed to enable ethrmii clock");
	}
#endif
	/* Make sure the device registers are clocked */
	ret = clk_prepare_enable(priv->pclk);
	if (ret) {
		dev_err(dev, "failed to enable peripheral clock");
		return ret;
	}

	/* Register PLL in CCF */
	ret = g12a_ephy_glue_clk_register(dev);
	if (ret)
		goto err;

	ret = mdio_mux_init(dev, dev->of_node, g12a_mdio_switch_fn,
			    &priv->mux_handle, dev, NULL);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "mdio multiplexer init failed: %d", ret);
		goto err;
	}

	return 0;

err:
	clk_disable_unprepare(priv->pclk);
	return ret;
}

static void g12a_mdio_mux_remove(struct platform_device *pdev)
{
	struct g12a_mdio_mux *priv = platform_get_drvdata(pdev);

	mdio_mux_uninit(priv->mux_handle);

	if (priv->pll_is_enabled)
		clk_disable_unprepare(priv->pll);

	clk_disable_unprepare(priv->pclk);
}

static struct platform_driver g12a_mdio_mux_driver = {
	.probe		= g12a_mdio_mux_probe,
	.remove		= g12a_mdio_mux_remove,
	.driver		= {
		.name	= "g12a-mdio_mux",
		.of_match_table = g12a_mdio_mux_match,
	},
};
module_platform_driver(g12a_mdio_mux_driver);

MODULE_DESCRIPTION("Amlogic G12a MDIO multiplexer driver");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("GPL v2");
