/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_HELPER_H__
#define __MESON_TX_HELPER_H__

#include <linux/hdmi.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <drm/amlogic/meson_connector_dev.h>

struct tx_timing;

struct tx_color_attr {
	enum hdmi_colorspace colorformat;
	int bitdepth;
};

enum hdmi_color_depth bitdepth_to_colordepth(int bitdepth);
int colordepth_to_bitdepth(enum hdmi_color_depth color_depth);
void meson_connector_fill_mode_timing(struct drm_display_mode *mode,
				const struct tx_timing *timing, bool edid_vic);
void meson_drm_mode_build_tx_timing(struct drm_display_mode *mode, struct tx_timing *timing);
int meson_tx_mode_testattr_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
void meson_drm_kms_helper_hotplug_event(struct drm_connector *connector);
int meson_dptx_dev_bind(struct drm_device *dr,
			int type, struct meson_connector_dev *intf);
int meson_dptx_dev_unbind(struct drm_device *drm,
			  int type, struct meson_connector_dev *intf);

int meson_edp_dev_bind(struct drm_device *drm,
		       int type, struct meson_connector_dev *intf);
int meson_edp_dev_unbind(struct drm_device *drm,
			 int type, struct meson_connector_dev *intf);

int meson_dptx_dev_bind(struct drm_device *drm,
			int type, struct meson_connector_dev *intf);
int meson_dptx_dev_unbind(struct drm_device *drm,
			  int type, struct meson_connector_dev *intf);

int meson_edp_dev_bind(struct drm_device *drm,
		       int type, struct meson_connector_dev *intf);
int meson_edp_dev_unbind(struct drm_device *drm,
			 int type, struct meson_connector_dev *intf);

#endif
