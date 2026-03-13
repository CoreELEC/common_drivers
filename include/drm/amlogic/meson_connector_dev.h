/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef MESON_CONNECTOR_DEV_H_
#define MESON_CONNECTOR_DEV_H_
#include <drm/drm_connector.h>
#include <drm/drm_modes.h>
#include <uapi/amlogic/drm/meson_drm.h>
#include <uapi/amlogic/vout_ioc.h>

#define MESON_CONNECTOR_TYPE_PROP_NAME "meson.connector_type"

struct vout_server_s;
struct meson_tx_audio_ops;

enum {
	MESON_DRM_CONNECTOR_V10 = 0,
};

struct meson_connector_dev {
	int ver;
	struct drm_connector *conn;
	int connector_type;
	int vout_state;
	struct vout_server_s *vout_serv;
};

/*hdmitx specified struct*/
/*hdcp*/
enum {
	HDCP_NULL = 0,
	HDCP_MODE14 = 1 << 0,
	HDCP_MODE22 = 1 << 1,
	HDCP_KEY_UPDATE = 1 << 2
};

enum vrr_types_e {
	QMS_VRR_SUP = BIT(0),
	GAMING_VRR_SUP = BIT(1),
	UI_VRR_SUP = BIT(2),
};

enum {
	HDCP_AUTH_FAIL = 0,
	HDCP_AUTH_OK = 1,
	HDCP_AUTH_UNKNOWN = 0xff,
};

struct connector_hpd_cb {
	void (*callback)(void *data);
	void *data;
};

struct connector_hdcp_cb {
	void (*hdcp_notify)(void *data, int type, int auth_result);
	void *data;
};

/*hdmitx specified struct end.*/

/*panel specified struct*/
struct meson_panel_dev {
	struct meson_connector_dev base;
	int (*get_modes)(struct meson_panel_dev *panel, struct drm_display_mode **modes, int *num);
	int (*get_modes_vrr_range)(struct meson_panel_dev *panel, void *range, int max, int *num);
	void (*set_mode_timing)(struct meson_panel_dev *panel, struct drm_display_mode *mode,
							enum vmode_e vmode);
};

/*dummy_l specified type*/
#define DRM_MODE_CONNECTOR_MESON_DUMMY_L  200

/*dummy_p specified type*/
#define DRM_MODE_CONNECTOR_MESON_DUMMY_P  201

/*dummy_i specified type*/
#define DRM_MODE_CONNECTOR_MESON_DUMMY_I  202

#define to_meson_panel_dev(x)	container_of(x, struct meson_panel_dev, base)

/*lcd specified struct*/

/*amlogic extend connector type: for original type is not enough.
 *start from: 0xff,
 *extend connector: 99 ~ 202,
 *legacy panel type for non-drm: 0x1000 ~
 *display service limit DRM connector type to range of u8 type
 */
#define DRM_MODE_MESON_CONNECTOR_PANEL_START 99
#define DRM_MODE_MESON_CONNECTOR_PANEL_END   128

#define DRM_MODE_MESON_CONNECTOR_DP_START 149
#define DRM_MODE_MESON_CONNECTOR_DP_END   158

#define DRM_MODE_MESON_CONNECTOR_HDMI_START 159
#define DRM_MODE_MESON_CONNECTOR_HDMI_END   173

enum {
	DRM_MODE_CONNECTOR_MESON_LVDS_A = 100,
	DRM_MODE_CONNECTOR_MESON_LVDS_B = 101,
	DRM_MODE_CONNECTOR_MESON_LVDS_C = 102,

	DRM_MODE_CONNECTOR_MESON_VBYONE_A = 110,
	DRM_MODE_CONNECTOR_MESON_VBYONE_B = 111,

	DRM_MODE_CONNECTOR_MESON_MIPI_A = 120,
	DRM_MODE_CONNECTOR_MESON_MIPI_B = 121,

	DRM_MODE_CONNECTOR_MESON_EDP_A = 130,
	DRM_MODE_CONNECTOR_MESON_EDP_B = 131,

	DRM_MODE_CONNECTOR_MESON_DP_A = 150,
	DRM_MODE_CONNECTOR_MESON_DP_B = 151,
};

enum {
	DRM_MODE_CONNECTOR_MESON_HDMIA_A = 160,
	DRM_MODE_CONNECTOR_MESON_HDMIA_B = 161,
	DRM_MODE_CONNECTOR_MESON_HDMIA_C = 162,

	DRM_MODE_CONNECTOR_MESON_HDMIB_A = 170,
	DRM_MODE_CONNECTOR_MESON_HDMIB_B = 171,
	DRM_MODE_CONNECTOR_MESON_HDMIB_C = 172,

	/* for meson_tx_connector, differences from LCD*/
	DRM_MODE_CONNECTOR_MESON_TX_EDP_A = 180,
	DRM_MODE_CONNECTOR_MESON_TX_EDP_B = 181,
	DRM_MODE_CONNECTOR_MESON_TX_EDP_C = 182,

	DRM_MODE_CONNECTOR_MESON_TX_DP_A = 190,
	DRM_MODE_CONNECTOR_MESON_TX_DP_B = 191,
	DRM_MODE_CONNECTOR_MESON_TX_DP_C = 192,
};

#endif
