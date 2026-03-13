/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_DEV_H
#define __MESON_TX_DEV_H

#include <drm/amlogic/meson_connector_dev.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_edid.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_format_para.h>
#include <linux/amlogic/media/vout/meson_tx_connector/clk/meson_tx_clk.h>
#include "meson_tx_dpcd.h"

/* for notify to cec/dprx/hdmirx */
#define TX_PLUG             1
#define TX_UNPLUG           2
#define TX_PHY_ADDR_VALID   3
#define TX_KSVLIST          4

typedef struct meson_tx_dev *(*conn_dev_to_txdev)(struct device *dev);

struct meson_tx_helper_ops;
struct tx_task_manager;
struct meson_tx_event_mgr;
struct meson_tx_tracer;
struct meson_tx_audio;

struct meson_tx_state {
	enum vmode_e mode;
	u32 hdr_priority;
	/* save sequence id for drm connecter state */
	u64 sequence_id;
	struct meson_tx_format_para para;
};

struct meson_tx_dev {
	struct meson_connector_dev conn_dev;
	struct meson_tx_audio *tx_aud;
	/* connector device */
	struct device *pdev;
	/* drm connector kdev */
	struct device *dev;
	struct meson_tx_log tx_log;

	/* vout related */
	struct vinfo_s tx_vinfo;

	/* save the lastest plug_in time from interrupt */
	u64 hw_sequence_id;
	/* hpd related */
	bool hpd_state;
	/* indicate hdmitx output ready, sw/hw mode setting done */
	bool ready;
	/* if hdmitx is in suspend state. */
	bool suspend_flag;

	/*
	 * if HDMI plugin even once time, then set 1
	 * if never hdmi plugin, then keep as 0
	 * only for android ott.
	 */
	u32 already_used_state;
	/* only for tx common task(hotplug) */
	struct tx_task_manager *task_mgr;
	struct connector_hpd_cb drm_hpd_cb;

	struct meson_tx_tracer *tx_tracer;
	struct meson_tx_event_mgr *event_mgr;

	/* edid related */
	/* edid data for HDMI, and displayid data for DP */
	unsigned char edid_buf[EDID_MAX_BLOCK * 128];
	/* 00000h~000ffh */
	unsigned char dpcd_buf[DPTX_RECEIVER_CAP_SIZE];
	struct rx_cap rxcap;
	struct dpcd_cap dpcd_cap;
	/* only for bootup case */
	bool parse_edid_by_tx_cfg;
	/* bit1: displayID, bit0: EDID; 1:enable parse, 0:skip parse */
	u8 edid_parse_mask;

	struct meson_tx_hw *tx_hw_base;
	/* for hot_plug/mode_set */
	struct mutex set_mode_mutex;
	/* mode valid check during mode set */
	struct mutex valid_mode_mutex;
	struct meson_tx_format_para sw_fmt_para;

	void *tx_cap;
	struct meson_tx_helper_ops *ops;

	/* set to true if hpd become low or sequence_id not match
	 * during video mode set; still set venc clk for vsync,
	 * skip phy(and core) setting
	 */
	bool skip_phy_setting;

	bool pxp_mode;
};

struct parse_cd {
	enum hdmi_color_depth cd;
	const char *name;
};

struct parse_cs {
	enum hdmi_colorspace cs;
	const char *name;
};

struct parse_cr {
	enum hdmi_quantization_range cr;
	const char *name;
};

#define to_meson_tx_dev(x)	container_of(x, struct meson_tx_dev, conn_dev)
#define to_meson_tx_state(x)	container_of(x, struct meson_tx_state, para)

void meson_tx_fire_drm_hpd_cb_unlocked(struct meson_tx_dev *tx_base);

/* for drm */
int meson_tx_register_hpd_cb(struct meson_tx_dev *tx_base, struct connector_hpd_cb *hpd_cb);
int meson_tx_get_hpd_state(struct meson_tx_dev *tx_base);
u64 meson_tx_get_hpd_hw_sequence_id(struct meson_tx_dev *tx_base);
bool meson_tx_get_used_state(struct meson_tx_dev *base);
int meson_tx_sysfs_create(struct meson_tx_dev *tx_dev);
void meson_tx_sysfs_remove(struct meson_tx_dev *tx_dev);
struct meson_tx_log *meson_get_tx_log(struct meson_tx_dev *tx_dev);
unsigned char *meson_tx_get_raw_edid(struct meson_tx_dev *tx_dev);
void meson_tx_get_init_state(struct meson_tx_dev *tx_dev,
			   struct meson_tx_state *state);
void meson_tx_parse_color_attr(char const *attr_str,
	enum hdmi_colorspace *cs, enum hdmi_color_depth *cd,
	enum hdmi_quantization_range *cr);
int meson_tx_validate_mode(struct meson_tx_dev *tx_dev, struct meson_tx_state *new_state);
int meson_tx_format_para_init(struct meson_tx_dev *tx_dev, struct tx_timing *timing,
	u32 frac_mode, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd, enum hdmi_quantization_range cr,
	struct meson_tx_format_para *para);
int meson_tx_build_clk_param(struct meson_tx_dev *tx_dev,
	struct meson_tx_format_para *para, u32 enc_idx, u32 hdmi_if_idx);

int meson_tx_do_mode_setting(struct meson_tx_dev *tx_dev,
				  struct meson_tx_state *new_state,
				  struct meson_tx_state *old_state);
#endif
