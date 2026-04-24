// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "phy-meson-usb.h"

inline bool meson_uphy_of_device_pci_available(void)
{
	struct device_node *np = NULL;
	bool ret = false;

	while ((np = of_find_node_by_name(np, "pcie"))) {
		if (of_device_is_available(np)) {
			ret = true;
			goto done;
		}
	}

	while ((np = of_find_node_by_type(np, "pci"))) {
		if (of_device_is_available(np)) {
			ret = true;
			break;
		}
	}

done:
	of_node_put(np);
	return ret;
}

inline bool meson_uphy_of_device_eth_hsgmii_phy_available(void)
{
	struct device_node *np = NULL;
	bool ret = false;

	while ((np = of_find_node_by_name(np, "hsgmii-phy"))) {
		if (of_device_is_available(np)) {
			ret = true;
			goto done;
		}
	}

	while ((np = of_find_node_by_type(np, "hsgmii-phy"))) {
		if (of_device_is_available(np)) {
			ret = true;
			break;
		}
	}

done:
	of_node_put(np);
	return ret;
}
