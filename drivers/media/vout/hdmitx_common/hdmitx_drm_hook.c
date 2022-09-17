// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/amlogic/meson_drm_bind.h>
#include <linux/component.h>
#include "hdmitx_drm_hook.h"

/*!!Only one instance supported.*/
struct hdmitx_common *global_tx_base;
static struct meson_hdmitx_dev hdmitx_drm_instance;

static int drm_hdmitx_id;

static int drm_hdmitx_get_hpd_state(void)
{
	return global_tx_base->hpd_state;
}

static int drm_hdmitx_register_hpd_cb(struct connector_hpd_cb *hpd_cb)
{
	return hdmitx_register_hpd_cb(global_tx_base, hpd_cb);
}

static void drm_hdmitx_setup_attr(const char *buf)
{
	hdmitx_setup_attr(global_tx_base, buf);
}

static void drm_hdmitx_get_attr(char attr[16])
{
	hdmitx_get_attr(global_tx_base, attr);
}

static int drm_hdmitx_get_hdr_priority(void)
{
	return global_tx_base->hdr_priority;
}

static int meson_hdmitx_bind(struct device *dev,
			      struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;

	if (bound_data->connector_component_bind) {
		drm_hdmitx_id = bound_data->connector_component_bind
			(bound_data->drm,
			DRM_MODE_CONNECTOR_HDMIA,
			&hdmitx_drm_instance.base);
		pr_err("%s hdmi [%d]\n", __func__, drm_hdmitx_id);
	} else {
		pr_err("no bind func from drm.\n");
	}

	return 0;
}

static void meson_hdmitx_unbind(struct device *dev,
				 struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			DRM_MODE_CONNECTOR_HDMIA, drm_hdmitx_id);
		pr_err("%s hdmi [%d]\n", __func__, drm_hdmitx_id);
	} else {
		pr_err("no unbind func.\n");
	}

	drm_hdmitx_id = 0;
	global_tx_base = 0;
}

/*drm component bind ops*/
static const struct component_ops meson_hdmitx_bind_ops = {
	.bind	= meson_hdmitx_bind,
	.unbind	= meson_hdmitx_unbind,
};

int hdmitx_bind_meson_drm(struct device *device,
	struct hdmitx_common *tx_base,
	struct meson_hdmitx_dev *diff)
{
	if (global_tx_base)
		pr_err("global_tx_base [%p] already hooked.\n", global_tx_base);

	global_tx_base = tx_base;

	hdmitx_drm_instance = *diff;
	hdmitx_drm_instance.base.ver = MESON_DRM_CONNECTOR_V10;

	hdmitx_drm_instance.detect = drm_hdmitx_get_hpd_state;
	hdmitx_drm_instance.register_hpd_cb = drm_hdmitx_register_hpd_cb;

	hdmitx_drm_instance.setup_attr = drm_hdmitx_setup_attr;
	hdmitx_drm_instance.get_attr = drm_hdmitx_get_attr;
	hdmitx_drm_instance.get_hdr_priority = drm_hdmitx_get_hdr_priority;

	return component_add(device, &meson_hdmitx_bind_ops);
}

int hdmitx_unbind_meson_drm(struct device *device,
	struct hdmitx_common *tx_base,
	struct meson_hdmitx_dev *diff)
{
	if (drm_hdmitx_id != 0)
		component_del(device, &meson_hdmitx_bind_ops);
	global_tx_base = 0;
	return 0;
}
