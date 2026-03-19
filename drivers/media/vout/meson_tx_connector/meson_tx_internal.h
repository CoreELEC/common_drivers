/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_DEV_INTERNAL_H
#define __MESON_TX_DEV_INTERNAL_H

#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw.h>
#include <linux/amlogic/media/vout/vout_notify.h>

struct meson_tx_helper_ops {
	int (*create_sysfs)(struct meson_tx_dev *tx_dev);
	void (*remove_sysfs)(struct meson_tx_dev *tx_dev);
	int (*hpd_plugin)(struct meson_tx_dev *tx_dev);
	int (*hpd_plugout)(struct meson_tx_dev *tx_dev);
	int (*validate_fmt_para)(struct meson_tx_dev *tx_base, struct meson_tx_state *state);
	int (*build_hw_fmt_para)(struct meson_tx_dev *tx_base, struct meson_tx_format_para *para);
	int (*pre_mode_enable)(struct meson_tx_dev *tx_base, struct meson_tx_format_para *para);
	int (*mode_enable)(struct meson_tx_dev *tx_base, struct meson_tx_format_para *para);
	int (*post_mode_enable)(struct meson_tx_dev *tx_base, struct meson_tx_format_para *para);
};

struct meson_tx_plat_data {
	struct meson_tx_hw *(*alloc_tx_hw)(void);
	void (*free_tx_hw)(struct meson_tx_hw *tx_hw);
	u32 is_edp;
};

struct meson_tx_log *meson_get_tx_log(struct meson_tx_dev *tx_dev);
void meson_tx_plugin_handler(void *para);
void meson_tx_plugout_handler(void *para);
void meson_tx_plugin_unlocked(struct meson_tx_dev *tx_dev);
void meson_tx_plugout_unlocked(struct meson_tx_dev *tx_dev);

void meson_tx_dev_init(struct meson_tx_dev *tx_dev, struct meson_tx_hw *tx_hw,
		       struct meson_tx_helper_ops *ops);
void meson_tx_dev_release(struct meson_tx_dev *tx_dev);
int meson_tx_notify_hpd_status(struct meson_tx_dev *tx_dev, bool force_uevent);

/* meson_tx_hw interface */
int meson_tx_hw_cntl(struct meson_tx_hw *tx_hw, u32 cmd,
		     void *input_argv, void *output_struct);
/* meson_tx_hw bind with tx_phy */
void meson_tx_hw_setup_phy(struct meson_tx_hw *tx_hw,
		       struct meson_tx_phy *tx_phy);
/* meson_tx_hw bind with tx_clk */
void meson_tx_hw_setup_clk(struct meson_tx_hw *tx_hw,
			   struct meson_tx_clk *tx_clk);

bool meson_tx_edid_validate_color(struct tx_timing *timing,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd, struct rx_cap *rx_cap);

/* meson_tx_vout */
int meson_tx_set_current_vmode(enum vmode_e mode, void *data);
int meson_tx_vout_set_state(int index, void *data);
int meson_tx_vout_clr_state(int index, void *data);
int meson_tx_vout_get_state(void *data);
void meson_tx_vout_init(struct meson_tx_dev *tx_dev, struct vout_op_s *ops);
void meson_tx_vout_uninit(struct meson_tx_dev *tx_dev);
void meson_tx_update_vinfo(struct meson_tx_dev *tx_dev);

#endif
