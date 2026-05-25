/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_PHY_CORE_H
#define __MESON_TX_PHY_CORE_H

#include <linux/platform_device.h>
#include <linux/phy/phy.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/of_address.h>
#include <linux/notifier.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>
#include <linux/gpio/consumer.h>

enum meson_tx_phy_mode {
	PHY_DP,
	PHY_HDMI,
};

struct meson_tx_phy_cfg_opts {
	u32 phy_clock;
	u32 force_ssc;
	struct phy_configure_opts_dp dp_ops;
};

struct typec_dp_state {
	/*
	 * flip: 0 means the usb orientation of cable is normal, otherwise reverse
	 */
	u32 flip;
	/*
	 * refer to include/linux/usb/typec_dp.h
	 * For dp alt mode, only cares
	 *     TYPEC_DP_STATE_C
	 *     TYPEC_DP_STATE_D
	 *     TYPEC_DP_STATE_E
	 * For C/E, dp will use 4 lanes, and for D, dp will use only 2 lanes
	 */
	u32 pin_state;
	/*
	 * Refer to DPAM V1.4 Spec, the definition of
	 *     DisplayPort Status Update and SOP DisplayPort Configurations
	 */
	struct typec_displayport_data dp_state_data;
};

struct meson_tx_phy_ops {
	int (*init)(void *phy);
	int (*exit)(void *phy);
	int (*power_on)(void *phy);
	int (*power_off)(void *phy);
	int (*set_aux_gpio)(void *phy, u32 gpio0, u32 gpio1);
	int (*set_mode)(void *phy, enum meson_tx_phy_mode mode,
				struct meson_tx_phy_cfg_opts *opts);
	int (*configure)(void *phy, struct meson_tx_phy_cfg_opts *opts);
	int (*toggle)(void *phy);
	int (*validate)(void *phy, enum meson_tx_phy_mode mode,
				struct meson_tx_phy_cfg_opts *opts);
	/* USB has the calibration function */
	int (*calibrate)(void *phy);
	int (*connect)(void *phy, int port);
	int (*disconnect)(void *phy, int port);
	int (*dump_reg)(void *phy, struct seq_file *s);
	int (*prbs_test)(void *phy);
};

struct meson_tx_phy {
	struct platform_device *pdev;
	struct meson_tx_phy_ops *ops;
	struct phy *phy;
	/* point to dptx or hdmitx device */
	void *tx_dev;
	struct notifier_block usbpd_to_dp_nb;
	struct typec_dp_state usb_dp_state;
	/* for dpphy and anactrl registers' access */
	struct regmap *regmap_dpphy_ctrl;
	void __iomem *base_dpphy_ctrl;
	struct regmap *regmap_anactrl_typec_dp;
	void __iomem *base_anactrl_typec_dp;
	struct gpio_desc *aux_gpio[2];
};

struct meson_tx_phy_data {
	struct meson_tx_phy_ops *ops;
	void *para;
};

int meson_tx_phy_init(struct meson_tx_phy  *phy);
int meson_tx_phy_exit(struct meson_tx_phy  *phy);
int meson_tx_phy_power_on(struct meson_tx_phy  *phy);
int meson_tx_phy_power_off(struct meson_tx_phy  *phy);
int meson_tx_phy_set_aux_gpio(struct meson_tx_phy  *phy, u32 gpio1, u32 gpio2);
int meson_tx_phy_set_mode(struct meson_tx_phy  *phy, enum meson_tx_phy_mode mode,
	struct meson_tx_phy_cfg_opts *opts);
int meson_tx_phy_configure(struct meson_tx_phy  *phy, struct meson_tx_phy_cfg_opts *opts);
int meson_tx_phy_toggle(struct meson_tx_phy  *phy);
int meson_tx_phy_validate(struct meson_tx_phy  *phy, enum meson_tx_phy_mode mode,
			struct meson_tx_phy_cfg_opts *opts);
int meson_tx_phy_calibrate(struct meson_tx_phy  *phy);
int meson_tx_phy_dump_reg(struct meson_tx_phy *phy, struct seq_file *s);

/*
 * DPPHY notify to DPTx core
 * DPTx core should use meson_tx_dpphy_register_notify() to register
 */
#define MESON_TX_CONN_DPPHY_HPD_PLUGIN    BIT(1)
#define MESON_TX_CONN_DPPHY_HPD_PLUGOUT   BIT(2)
#define MESON_TX_CONN_DPPHY_HPD_IRQ       BIT(3)
#define MESON_TX_CONN_DPPHY_LANE_REMAP    BIT(4)

void meson_tx_dpphy_register_notify(struct notifier_block *nb);
void meson_tx_dpphy_unregister_notify(struct notifier_block *nb);

#endif
