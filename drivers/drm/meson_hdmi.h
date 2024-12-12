/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_HDMI_H__
#define __MESON_HDMI_H__

#include "meson_drv.h"
#include <uapi/amlogic/drm/meson_drm.h>
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/amlogic/meson_connector_dev.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <media/cec-notifier.h>

enum {
	HDCP_STATE_START = 0,
	HDCP_STATE_SUCCESS,
	HDCP_STATE_FAIL,
	HDCP_STATE_STOP,
	HDCP_STATE_DISCONNECT,
};

enum {
	MESON_PREF_DV = 0,
	MESON_PREF_HDR,
	MESON_PREF_SDR,
};

struct hdmitx_color_attr {
	enum hdmi_colorspace colorformat;
	enum hdmi_color_depth bitdepth;
	enum hdmi_quantization_range colorrange;
};

enum meson_color_attr_type {
	COLOR_YCBCR420_12BIT = 0,
	COLOR_YCBCR420_10BIT,
	COLOR_YCBCR420_8BIT,
	COLOR_YCBCR444_12BIT,
	COLOR_YCBCR444_10BIT,
	COLOR_YCBCR444_8BIT,
	COLOR_YCBCR422_12BIT,
	COLOR_RGB_12BIT,
	COLOR_RGB_10BIT,
	COLOR_RGB_8BIT,
	COLOR_MAX_ATTR,
};

struct am_hdmi_tx {
	struct meson_connector base;
	struct drm_encoder encoder;

	/*drm request content type.*/
	int hdcp_request_content_type;
	int hdcp_request_content_protection;
	/*current hdcp running mode, HDCP_NULL means hdcp disabled.*/
	int hdcp_mode;
	/*hdcp auth result, HDCP_AUTH_UNKNOWN means havenot finished auth.*/
	int hdcp_state;
	int hdcp_rx_type;

	int hdmitx_on;

	int min_vfreq;
	int max_vfreq;

	/* save sequence_id for drm connecter get raw edid */
	u64 sequence_id;

	/*TODO: android compatible, remove later*/
	bool android_path;
	bool recovery_mode;

	/*amlogic property: force hdmitx update
	 *colorspace/colordepth from sysfs.
	 */
	struct drm_property *update_attr_prop;
	struct drm_property *color_space_prop;
	struct drm_property *color_depth_prop;
	struct drm_property *color_force;
	struct drm_property *avmute_prop;
	struct drm_property *hdmi_hdr_status_prop;
	struct drm_property *hdr_cap_property;
	struct drm_property *dv_cap_property;
	struct drm_property *hdcp_ver_prop;
	struct drm_property *hdcp_mode_property;
	struct drm_property *hdcp_topo_property;
	struct drm_property *hdr_priority_prop;
	struct drm_property *contenttype_cap_prop;
	struct drm_property *allm_prop;
	struct drm_property *ready_prop;
	struct drm_property *type_prop;
	/*
	 * Whether the current edid is valid
	 * 0:edid is invalid
	 * 1:edid is valid
	 */
	struct drm_property *edid_valid_prop;
	/*
	 * HWC enable hdcp flow
	 * 0: IVCX chip don't need: T7/S5/S6/S7/S7D/S1A
	 * 1: SNPS chip need: SC2/S4/G12/SM1
	 */
	struct drm_property *hdcp_user_prop;
	int hdmi_type;
	struct drm_property *frac_rate_policy_prop;
	struct drm_property *allm_cap_prop;
	struct drm_property *dc_cap_prop;

#ifdef CONFIG_CEC_NOTIFIER
	struct cec_notifier	*cec_notifier;
#endif

	struct meson_hdmitx_dev *hdmitx_dev;
};

struct am_hdmitx_connector_state {
	struct drm_connector_state base;
	struct hdmitx_common_state hcs;

	/*drm hdmitx attr from external modules,
	 *ONLY used for once, and reset when duplicate.
	 */
	struct hdmitx_color_attr color_attr_para;
	/*HDR Priority: dv,hdr,sdr*/
	int pref_hdr_policy;
	u32 hdr_priority;

	bool update : 1;
	bool color_force : 1;
	bool avmute : 1;
	bool ready : 1;
	bool frac_rate_policy : 1;
	int allm_mode;
};

#define to_am_hdmitx_connector_state(x)	container_of(x, struct am_hdmitx_connector_state, base)
#define meson_connector_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, base)
#define connector_to_am_hdmi(x) \
	container_of(connector_to_meson_connector(x), struct am_hdmi_tx, base)
#define encoder_to_am_hdmi(x)	container_of(x, struct am_hdmi_tx, encoder)

int meson_hdmitx_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf);
int meson_hdmitx_dev_unbind(struct drm_device *drm,
	int type, int connector_id);

void convert_attrstr(char *attr_str, struct hdmitx_color_attr *attr_param);

int am_meson_mode_testattr_ioctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int am_meson_get_vrr_range_ioctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif
