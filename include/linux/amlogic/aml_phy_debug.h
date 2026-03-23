/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _STMMAC_MESON_PHY_DEBUG_H
#define _STMMAC_MESON_PHY_DEBUG_H

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>
#include <linux/phylink.h>
#include <linux/sysfs.h>

/**
 * struct phylink - internal data type for phylink
 */
struct phylink {
	/* private: */
	struct net_device *netdev;
	const struct phylink_mac_ops *mac_ops;
	struct phylink_config *config;
	struct phylink_pcs *pcs;
	struct device *dev;
	unsigned int old_link_state:1;

	unsigned long phylink_disable_state; /* bitmask of disables */
	struct phy_device *phydev;
	phy_interface_t link_interface;	/* PHY_INTERFACE_xxx */
	u8 cfg_link_an_mode;		/* MLO_AN_xxx */
	u8 cur_link_an_mode;
	u8 link_port;			/* The current non-phy ethtool port */
	__ETHTOOL_DECLARE_LINK_MODE_MASK(supported);

	/* The link configuration settings */
	struct phylink_link_state link_config;

	/* The current settings */
	phy_interface_t cur_interface;

	struct gpio_desc *link_gpio;
	unsigned int link_irq;
	struct timer_list link_poll;
	void (*get_fixed_state)(struct net_device *dev,
				struct phylink_link_state *s);

	struct mutex state_mutex;
	struct phylink_link_state phy_state;
	struct work_struct resolve;
	unsigned int pcs_neg_mode;
	unsigned int pcs_state;

	bool mac_link_dropped;
	bool using_mac_select_pcs;

	struct sfp_bus *sfp_bus;
	bool sfp_may_have_phy;
	DECLARE_PHY_INTERFACE_MASK(sfp_interfaces);
	__ETHTOOL_DECLARE_LINK_MODE_MASK(sfp_support);
	u8 sfp_port;
};

struct aml_wol_ops {
	bool (*not_empty)(struct device *device);
	void (*clr_all)(struct device *device);
	void (*set_all)(struct device *device);
};

struct aml_eth_priv {
	unsigned char adc_data[32 * 32];
	unsigned char adc_freq[64];
	void __iomem *PREG_ETH_REG0;
	void __iomem *PREG_ETH_REG1;
	void __iomem *ioaddr;
	struct phy_device *phydev;
	unsigned int internal_phy;
	unsigned int ephy_eee_support;
	unsigned int inphy_eee_enable;
	struct device *dev;
	unsigned int tx_amp_bl2;
	unsigned int phy_mode;
	void __iomem *phy_analog_config_addr;
	/*0 for 12nm, 1 for 22nm*/
	unsigned int phy_pll_mode;
	struct device *mdio_dev;
#if IS_ENABLED(CONFIG_PM_SLEEP)
	unsigned int wol_switch_from_user;
	unsigned int mdns_switch_from_user;
	unsigned int support_gpio_wol;
	unsigned int exphy_mdns_wkup;
	struct aml_wol_ops wol_ops;
#endif
};

int gmac_create_sysfs(struct aml_eth_priv *eth_priv);
int gmac_remove_sysfs(struct aml_eth_priv *eth_priv);
struct aml_eth_priv *aml_get_eth_priv_by_pdev(struct phy_device *phydev);
struct aml_eth_priv *aml_get_eth_priv_by_ndev(struct device *dev);
struct device *aml_get_ethmac_dev_by_priv(struct aml_eth_priv *eth_priv);

#endif
