// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <drm/amlogic/meson_drm_bind.h>
#include "meson_hdmi.h"
#include "meson_cvbs.h"
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "meson_lcd.h"
#endif
#include "meson_dummyl.h"
#include "meson_dummyp.h"

int meson_connector_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	/*amlogic extend lcd*/
	if (type > DRM_MODE_MESON_CONNECTOR_PANEL_START &&
		type < DRM_MODE_MESON_CONNECTOR_PANEL_END) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		return meson_panel_dev_bind(drm, type, intf);
#else
		pr_err("Panel connector is not supported!\n");
		return -1;
#endif
	}

	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_bind(drm, type, intf);

	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_bind(drm, type, intf);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DRM_MODE_CONNECTOR_LVDS:
	case DRM_MODE_CONNECTOR_DSI:
	case DRM_MODE_CONNECTOR_eDP:
		return meson_panel_dev_bind(drm, type, intf);
#endif

	case DRM_MODE_CONNECTOR_MESON_DUMMY_L:
		return meson_dummyl_dev_bind(drm, type, intf);

	case DRM_MODE_CONNECTOR_MESON_DUMMY_P:
		return meson_dummyp_dev_bind(drm, type, intf);

	default:
		pr_err("unknown connector tye %d\n", type);
		return 0;
	};

	return 0;
}
EXPORT_SYMBOL(meson_connector_dev_bind);

int meson_connector_dev_unbind(struct drm_device *drm,
	int type, int connector_id)
{
	/*amlogic extend lcd*/
	if (type > DRM_MODE_MESON_CONNECTOR_PANEL_START &&
		type < DRM_MODE_MESON_CONNECTOR_PANEL_END) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		return meson_panel_dev_unbind(drm, type, connector_id);
#else
		pr_err("Panel connector is not supported!\n");
		return -1;
#endif
	}

	switch (type) {
	case DRM_MODE_CONNECTOR_HDMIA:
	case DRM_MODE_CONNECTOR_HDMIB:
		return meson_hdmitx_dev_unbind(drm, type, connector_id);

	case DRM_MODE_CONNECTOR_TV:
		return meson_cvbs_dev_unbind(drm, type, connector_id);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DRM_MODE_CONNECTOR_LVDS:
	case DRM_MODE_CONNECTOR_DSI:
	case DRM_MODE_CONNECTOR_eDP:
		return meson_panel_dev_unbind(drm, type, connector_id);
#endif

	case DRM_MODE_CONNECTOR_MESON_DUMMY_L:
		return meson_dummyl_dev_unbind(drm, type, connector_id);

	case DRM_MODE_CONNECTOR_MESON_DUMMY_P:
		return meson_dummyp_dev_unbind(drm, type, connector_id);

	default:
		pr_err("unknown connector tye %d\n", type);
		return 0;
	};
}
EXPORT_SYMBOL(meson_connector_dev_unbind);

