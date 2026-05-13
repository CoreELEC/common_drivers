// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_panel.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <linux/component.h>
#include <linux/iopoll.h>
#include <vout/vout_serve/vout_func.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_export.h>
#include "meson_crtc.h"
#include "meson_eDP.h"

static const char eDP_aux_name[2][10] = {"eDP-A-aux", "eDP-B-aux"};

static const char *get_eDP_aux_name(int type)
{
	if (type == DRM_MODE_CONNECTOR_MESON_EDP_B)
		return eDP_aux_name[1];
	return eDP_aux_name[0];
}

struct edptx_drv_s *meson_eDP_get_eDPTX_drv(int type)
{
#ifdef CONFIG_AMLOGIC_eDPTX
	if (type == DRM_MODE_CONNECTOR_MESON_EDP_A)
		return aml_edptx_get_driver(0);
	else if (type == DRM_MODE_CONNECTOR_MESON_EDP_B)
		return aml_edptx_get_driver(1);
	else
		return NULL;
#else
	return NULL;
#endif
}

static int meson_eDP_get_eDPTX_aux_bl_available(int type)
{
	struct edptx_drv_s *dptx = meson_eDP_get_eDPTX_drv(type);

	if (!dptx)
		return 0;
	if (dptx->setting.use_aux_bl_as_possible)
		return dptx->sink.link[0].cap.DPCD_reg_func & BIT(2);

	return 0;
}

struct meson_eDP_framerate_map {
	int lcd_frac_hint;
	int drm_fps;
};

struct meson_eDP_framerate_map eDP_framerate_map[] = {
	{36000, 360},
	{34000, 340},
	{33000, 330},
	{28800, 288},
	{24000, 240},
	{23976, 239},
	{20000, 200},
	{19200, 192},
	{19180, 191},
	{18000, 180},
	{17000, 170},
	{16500, 165},
	{14400, 144},
	{12000, 120},
	{11988, 119},
	{10000, 100},
	{9600, 96},
	{9590, 95},
	{6000, 60},
	{5994, 59},
	{5000, 50},
	{4800, 48},
	{4795, 47}
};

static int find_frac_hint_by_fps(int fps)
{
	int i;
	struct meson_eDP_framerate_map *map;
	int size = ARRAY_SIZE(eDP_framerate_map);

	for (i = 0; i < size; i++) {
		map = &eDP_framerate_map[i];
		if (map->drm_fps == fps)
			return map->lcd_frac_hint;
	}

	return 6000;
}

int meson_eDP_get_modes(struct drm_connector *connector)
{
	struct meson_eDP *am_eDP = connector_to_meson_eDP(connector);
	struct meson_panel_dev *eDP_dev = am_eDP->eDP_dev;
	struct drm_display_mode *modes, *mode;
	//struct edid *edid;
	int modes_cnt = 0;
	int i = 0, ret = 0;

	if (!eDP_dev->get_modes) {
		DRM_ERROR("No get Modes function.");
		return 0;
	}

	//edid = (struct edid *)meson_eDPTX_get_raw_edid(&tx_comm->base);
	//drm_connector_update_edid_property(connector, edid);

	if (eDP_dev->get_modes_vrr_range) {
		ret = eDP_dev->get_modes_vrr_range(eDP_dev, (void *)am_eDP->groups,
						     MAX_VRR_MODE_GROUP,
						     &am_eDP->num_vrr_group);
		if (ret || !am_eDP->num_vrr_group)
			drm_connector_set_vrr_capable_property(connector, false);
		else
			drm_connector_set_vrr_capable_property(connector, true);
	} else {
		drm_connector_set_vrr_capable_property(connector, false);
	}

	ret = eDP_dev->get_modes(eDP_dev, &modes, &modes_cnt);
	DRM_DEBUG("get modes %d, ret %d\n", modes_cnt, ret);
	if (ret == 0 && modes_cnt > 0) {
		if (!connector->display_info.width_mm || !connector->display_info.height_mm) {
			connector->display_info.width_mm = modes[i].width_mm;
			connector->display_info.height_mm = modes[i].height_mm;
		}

		for (i = 0; i < modes_cnt; i++) {
			DRM_DEBUG("[%d] mode_name-%s\n", __LINE__, modes[i].name);
			mode = drm_mode_duplicate(connector->dev, &modes[i]);
			if (mode) {
				drm_mode_probed_add(connector, mode);
				drm_mode_debug_printmodeline(mode);
			}
		}
		kfree(modes);
	}

	return modes_cnt;
}

enum drm_mode_status meson_eDP_check_mode(struct drm_connector *connector,
	struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *meson_eDP_best_encoder
	(struct drm_connector *connector)
{
	struct meson_eDP *am_eDP = connector_to_meson_eDP(connector);

	return &am_eDP->encoder;
}

static const struct drm_connector_helper_funcs meson_eDP_helper_funcs = {
	.get_modes = meson_eDP_get_modes,
	.mode_valid = meson_eDP_check_mode,
	.best_encoder = meson_eDP_best_encoder,
};

static enum drm_connector_status meson_eDP_detect(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static void am_eDP_connector_destroy(struct drm_connector *connector)
{
	DRM_DEBUG("[%d] called\n", __LINE__);

	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static void am_eDP_connector_reset(struct drm_connector *connector)
{
	DRM_DEBUG("[%d] called\n", __LINE__);

	drm_atomic_helper_connector_reset(connector);
}

static struct drm_connector_state *
am_eDP_connector_duplicate_state(struct drm_connector *connector)
{
	struct drm_connector_state *state;

	DRM_DEBUG("[%d] called\n",  __LINE__);

	state = drm_atomic_helper_connector_duplicate_state(connector);
	return state;
}

static void am_eDP_connector_destroy_state(struct drm_connector *connector,
					   struct drm_connector_state *state)
{
	DRM_DEBUG("[%d] called\n", __LINE__);

	drm_atomic_helper_connector_destroy_state(connector, state);
}

int meson_eDP_atomic_set_property(struct drm_connector *connector,
			   struct drm_connector_state *state,
			   struct drm_property *property,
			   uint64_t val)
{
	return -EINVAL;
}

int meson_eDP_atomic_get_property(struct drm_connector *connector,
			   const struct drm_connector_state *state,
			   struct drm_property *property, uint64_t *val)
{
	struct meson_eDP *am_eDP = connector_to_meson_eDP(connector);

	if (property == am_eDP->panel_type_prop) {
		*val = am_eDP->panel_type;
		return 0;
	}
	// } else if (property == am_eDP->panel_type_prop)

	return -EINVAL;
	// return 0;
}

void meson_eDP_atomic_print_state(struct drm_printer *p,
				    const struct drm_connector_state *state)
{
	int i;
	struct drm_vrr_mode_group *group;
	struct meson_eDP *am_eDP;
	struct drm_connector *conn = state->connector;

	if (!conn)
		return;

	am_eDP = connector_to_meson_eDP(conn);
	drm_printf(p, "\tdrm eDP state:\n");
	drm_printf(p, "\t\t  vrr range :\n");
	for (i = 0; i < am_eDP->num_vrr_group; i++) {
		group = &am_eDP->groups[i];
		drm_printf(p, "\t\t %u,%u,%u-%u,%s\n", group->width, group->height,
			   group->vrr_min, group->vrr_max, group->modename);
	}
}

static const struct drm_connector_funcs meson_eDP_funcs = {
	.detect			= meson_eDP_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.destroy		= am_eDP_connector_destroy,
	.reset			= am_eDP_connector_reset,
	.atomic_duplicate_state	= am_eDP_connector_duplicate_state,
	.atomic_destroy_state	= am_eDP_connector_destroy_state,
	.atomic_set_property	= meson_eDP_atomic_set_property,
	.atomic_get_property	= meson_eDP_atomic_get_property,
	.atomic_print_state = meson_eDP_atomic_print_state,
};

static void meson_eDP_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc_state->crtc);
	struct meson_eDP *am_eDP = encoder_to_meson_eDP(encoder);
	struct meson_connector_dev *con_dev = am_eDP->con_dev;

	update_curr_vout_server(amcrtc->vout_index, con_dev->vout_serv);
}

static void meson_eDP_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	int lcd_frac_hint;
	struct am_meson_crtc_state *meson_crtc_state =
				to_am_meson_crtc_state(encoder->crtc->state);
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(encoder->crtc);
	struct drm_display_mode *mode = &meson_crtc_state->base.adjusted_mode;
	struct drm_crtc_state *old_crtc_state;
	struct am_meson_crtc_state *meson_old_crtc_state;
	enum vmode_e vmode = meson_crtc_state->vmode;
	unsigned int vrefresh = drm_mode_vrefresh(mode);
	struct meson_eDP *am_eDP = encoder_to_meson_eDP(encoder);
	struct meson_panel_dev *eDP_dev = am_eDP->eDP_dev;

	crtc = encoder->crtc;

	if ((vmode & VMODE_MODE_BIT_MASK) != VMODE_eDP) {
		DRM_DEBUG("enable fail! vmode:%d\n", vmode);
		return;
	}

	old_crtc_state = drm_atomic_get_old_crtc_state(state, crtc);
	if (!old_crtc_state) {
		DRM_ERROR("crtc state is NULL!\n");
		return;
	}
	meson_old_crtc_state = to_am_meson_crtc_state(old_crtc_state);

	if (meson_crtc_state->uboot_mode_init == 1)
		vmode |= VMODE_INIT_BIT_MASK;

	DRM_INFO("[%s]-[%d] called, %d, %d\n", __func__, __LINE__,
		 meson_crtc_state->prev_vrefresh, vrefresh);

	if (encoder->crtc->state->vrr_enabled) {
		/*for keystone projector, switch loopback may change connector's crtc*/
		if (strcmp(meson_crtc_state->brr_mode, meson_old_crtc_state->brr_mode) ||
			encoder->crtc->state->connectors_changed ||
			encoder->crtc->state->active_changed) {
			meson_vout_notify_mode_change(amcrtc->vout_index,
				vmode, EVENT_MODE_SET_START);
			/*set mode timing*/
			eDP_dev->set_mode_timing(eDP_dev, mode, vmode);
			meson_vout_notify_mode_change(amcrtc->vout_index,
				vmode, EVENT_MODE_SET_FINISH);
			meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "eDP");
		}

		if (vrefresh != meson_crtc_state->brr ||
		    meson_crtc_state->prev_vrefresh != vrefresh ||
			encoder->crtc->state->connectors_changed ||
			encoder->crtc->state->active_changed) {
			lcd_frac_hint = find_frac_hint_by_fps(vrefresh);
			vout_func_set_vframe_rate_hint(amcrtc->vout_index, lcd_frac_hint);
			DRM_INFO("vrr mode, use set_vframe_rate_hint, %s-%s, %d-%d, %d-%d\n",
				 meson_crtc_state->brr_mode, meson_old_crtc_state->brr_mode,
				 meson_crtc_state->brr, lcd_frac_hint,
				 encoder->crtc->state->connectors_changed,
				 encoder->crtc->state->active_changed);
		}

		meson_crtc_state->prev_vrefresh = vrefresh;

		return;
	}

	meson_crtc_state->prev_vrefresh = vrefresh;
	meson_crtc_state->prev_height = mode->vdisplay;
	meson_vout_notify_mode_change(amcrtc->vout_index,
		vmode, EVENT_MODE_SET_START);
	/*set mode timing*/
	eDP_dev->set_mode_timing(eDP_dev, mode, vmode);
	meson_vout_notify_mode_change(amcrtc->vout_index,
		vmode, EVENT_MODE_SET_FINISH);
	meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "eDP");

	DRM_DEBUG("[%d] exit\n", __LINE__);
}

static void meson_eDP_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
}

#if 0
static void meson_eDP_cal_brr(struct meson_eDP *am_lcd,
				struct am_meson_crtc_state *crtc_state,
				struct drm_display_mode *adj_mode)
{
	int i;
	struct drm_vrr_mode_group *group;

	for (i = 0; i < am_lcd->num_vrr_group; i++) {
		group = &am_lcd->groups[i];
		DRM_DEBUG("%s %u-%u,%u-%u,%u-%u,%u\n", __func__, group->width,
			  adj_mode->hdisplay, group->height, adj_mode->vdisplay,
			  group->vrr_min, group->vrr_max,
			  drm_mode_vrefresh(adj_mode));
		if (group->width == adj_mode->hdisplay &&
		    group->height == adj_mode->vdisplay &&
		    group->vrr_min / VRR_DIV <= drm_mode_vrefresh(adj_mode) &&
		    group->vrr_max / VRR_DIV >= drm_mode_vrefresh(adj_mode)) {
			break;
		}
	}

	DRM_DEBUG("%s, %d, %d\n", __func__, i, am_lcd->num_vrr_group);
	if (i != am_lcd->num_vrr_group) {
		strncpy(crtc_state->brr_mode, group->modename, DRM_DISPLAY_MODE_LEN);
		crtc_state->brr_mode[DRM_DISPLAY_MODE_LEN - 1] = '\0';
		/*group->brr only used by lcd, not div 100*/
		crtc_state->brr = group->brr;
		crtc_state->valid_brr = 1;
	} else {
		DRM_ERROR("%s, get panel brr fail\n", adj_mode->name);
	}
}

static
int meson_encoder_vrr_change(struct drm_encoder *encoder,
			     struct drm_atomic_state *state)
{
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_state, *old_state;
	struct drm_display_mode *new_mode, *old_mode;
	struct am_meson_crtc_state *meson_crtc_state;

	connector = drm_atomic_get_new_connector_for_encoder(state, encoder);
	if (!connector)
		return 0;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state)
		return 0;

	crtc = conn_state->crtc;
	new_state = drm_atomic_get_new_crtc_state(state, crtc);
	old_state = drm_atomic_get_old_crtc_state(state, crtc);

	if (!new_state || !old_state) {
		DRM_INFO("%s crtc state is NULL!\n", __func__);
		return 0;
	}
	new_mode = &new_state->adjusted_mode;
	old_mode = &old_state->adjusted_mode;
	meson_crtc_state = to_am_meson_crtc_state(new_state);

	if (new_state->vrr_enabled &&
		new_mode->hdisplay == old_mode->hdisplay &&
		new_mode->vdisplay == old_mode->vdisplay &&
		!(new_mode->flags & DRM_MODE_FLAG_INTERLACE)) {
		meson_crtc_state->seamless = true;
	} else {
		meson_crtc_state->seamless = false;
	}

	DRM_DEBUG("[%s], seamless is %d\n", __func__, meson_crtc_state->seamless);
	return meson_crtc_state->seamless;
}

static int meson_eDP_encoder_atomic_check(struct drm_encoder *encoder,
				       struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct meson_eDP *am_lcd = encoder_to_meson_eDP(encoder);
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);

	if (!crtc_state->vrr_enabled)
		memset(meson_crtc_state->brr_mode, 0, DRM_DISPLAY_MODE_LEN);

	if (crtc_state->vrr_enabled || meson_crtc_state->uboot_mode_init)
		meson_eDP_cal_brr(am_lcd, meson_crtc_state, adj_mode);

	meson_encoder_vrr_change(encoder, conn_state->state);
	DRM_DEBUG("[%s]-[%d] called\n", __func__, __LINE__);
	return 0;
}
#endif

static int meson_eDP_encoder_atomic_check(struct drm_encoder *encoder,
				       struct drm_crtc_state *crtc_state,
				struct drm_connector_state *conn_state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);

	/*drm not find connector by vout_validate_vmode*/
	meson_crtc_state->preset_vmode = VMODE_eDP;

	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

static const struct drm_encoder_helper_funcs meson_eDP_encoder_helper_funcs = {
	.atomic_mode_set = meson_eDP_encoder_atomic_mode_set,
	.atomic_enable = meson_eDP_encoder_atomic_enable,
	.atomic_disable = meson_eDP_encoder_atomic_disable,
	.atomic_check = meson_eDP_encoder_atomic_check,
};

static const struct drm_encoder_funcs meson_eDP_encoder_funcs = {
	.destroy        = drm_encoder_cleanup,
};

int eDP_panel_prepare(struct drm_panel *panel)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int eDP_panel_enable(struct drm_panel *panel)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int eDP_panel_disable(struct drm_panel *panel)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int eDP_panel_unprepare(struct drm_panel *panel)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int eDP_panel_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int eDP_panel_get_orientation(struct drm_panel *panel)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return DRM_MODE_PANEL_ORIENTATION_NORMAL;
}

int eDP_panel_get_timings(struct drm_panel *panel, unsigned int num_timings,
				struct display_timing *timings)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

void eDP_panel_debugfs_init(struct drm_panel *panel, struct dentry *root)
{
	DRM_DEBUG("[%d] called\n", __LINE__);
}

static const struct drm_panel_funcs meson_eDP_panel_funcs = {
	.prepare         = eDP_panel_prepare,
	.enable          = eDP_panel_enable,
	.disable         = eDP_panel_disable,
	.unprepare       = eDP_panel_unprepare,
	.get_modes       = eDP_panel_get_modes,
	.get_orientation = eDP_panel_get_orientation,
	.get_timings     = eDP_panel_get_timings,
	.debugfs_init    = eDP_panel_debugfs_init,
};

ssize_t eDP_aux_transfer(struct drm_dp_aux *aux, struct drm_dp_aux_msg *msg)
{
	//struct edptx_drv_s *dptx_drv;
	struct meson_eDP *am_eDP = aux_to_meson_eDP(aux);

	DRM_DEBUG("[%d] called[%u] [0x%x] req=%u rep=%u size=%zu\n", __LINE__,
		am_eDP->panel_type, msg->address, msg->request, msg->reply, msg->size);

	return msg->size;
}

// This is mainly useful for eDP panels drivers to wait for an eDP panel to finish powering on
int  eDP_wait_hpd_asserted(struct drm_dp_aux *aux, unsigned long wait_us)
{
	struct edptx_drv_s *edp_drv;
	struct meson_eDP *am_eDP = aux_to_meson_eDP(aux);
	int hpd_level, readx;

	DRM_DEBUG("[%d] called %d, %lus\n", __LINE__, am_eDP->panel_type, wait_us);

	edp_drv = meson_eDP_get_eDPTX_drv(am_eDP->panel_type);
	if (!edp_drv)
		return -ETIMEDOUT;

	wait_us = (wait_us == 0 || wait_us > 1000) ? 1000 : wait_us;

	readx = readx_poll_timeout(aml_edptx_get_hpd_state, edp_drv->idx,
				hpd_level, hpd_level == 1, 10, wait_us);

	return readx;
}

int meson_eDP_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL;
	struct meson_eDP *am_eDP = NULL;
	struct drm_property *type_prop = NULL;
	struct meson_drm *priv = drm->dev_private;
	char *connector_name = NULL;
	int connector_type = type;
	int encoder_type = DRM_MODE_ENCODER_TMDS;
	int ret = 0;
	struct edptx_drv_s *dptx_drv = meson_eDP_get_eDPTX_drv(type);

	DRM_INFO("[%s]-[%d] type=%d\n", __func__, __LINE__, type);

	am_eDP = kzalloc(sizeof(*am_eDP), GFP_KERNEL);
	if (!am_eDP) {
		DRM_ERROR("alloc am_eDP failed\n");
		return -ENOMEM;
	}

	am_eDP->panel_type = type;
	am_eDP->drm = drm;
	am_eDP->eDP_dev = to_meson_panel_dev(intf);
	am_eDP->base.connector_type = type;
	am_eDP->base.drm_priv = priv;
	am_eDP->con_dev = intf;
	encoder = &am_eDP->encoder;
	if (!encoder)
		return -EINVAL;

	connector = &am_eDP->base.connector;
	intf->conn = connector;

	switch (type) {
	case DRM_MODE_CONNECTOR_MESON_EDP_A:
		connector_type = DRM_MODE_CONNECTOR_eDP;
		encoder_type = DRM_MODE_ENCODER_TMDS;
		connector_name = "EDP-A";
		break;
	case DRM_MODE_CONNECTOR_MESON_EDP_B:
		connector_type = DRM_MODE_CONNECTOR_eDP;
		encoder_type = DRM_MODE_ENCODER_TMDS;
		connector_name = "EDP-B";
		break;
	default:
		connector_type = DRM_MODE_CONNECTOR_Unknown;
		encoder_type = DRM_MODE_ENCODER_NONE;
		break;
	};

	DRM_INFO("%s: bind %d -> connector-%s-%d,encoder-%d\n",
		__func__, type, connector_name, connector_type, encoder_type);

	/* Encoder */
	encoder->possible_crtcs = priv->of_conf.crtc_masks[ENCODER_LCD];
	drm_encoder_helper_add(encoder, &meson_eDP_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_eDP_encoder_funcs,
			       encoder_type, "am_eDP_encoder");
	if (ret) {
		DRM_ERROR("error:%d: Failed to init eDP encoder\n", __LINE__);
		goto free_resource;
	}

	/* Connector */
	drm_connector_helper_add(connector, &meson_eDP_helper_funcs);
	ret = drm_connector_init(drm, connector, &meson_eDP_funcs, connector_type);
	if (ret) {
		DRM_ERROR("%d: Failed to init eDP connector\n", __LINE__);
		goto free_resource;
	}

	/*update name to amlogic name*/
	if (connector_name) {
		kfree(connector->name);
		connector->name = kasprintf(GFP_KERNEL, "%s", connector_name);
		if (!connector->name)
			DRM_ERROR("alloc name failed\n");
	}

	ret = drm_connector_attach_encoder(connector, encoder);
	if (ret != 0) {
		DRM_ERROR("%d: attach failed.\n", __LINE__);
		goto free_resource;
	}

	/*prop for userspace to acquire prop*/
	type_prop = drm_property_create_range(drm, DRM_MODE_PROP_IMMUTABLE,
		MESON_CONNECTOR_TYPE_PROP_NAME, 0, INT_MAX);
	if (type_prop) {
		am_eDP->panel_type_prop = type_prop;
		drm_object_attach_property(&am_eDP->base.connector.base, type_prop, type);
	} else {
		DRM_ERROR("Failed to create property %s\n",
			MESON_CONNECTOR_TYPE_PROP_NAME);
	}

	drm_connector_attach_vrr_capable_property(connector);

	am_eDP->aux.drm_dev = drm;
	am_eDP->aux.name    = get_eDP_aux_name(type);
	//am_eDP->aux.ddc     = drm;
	am_eDP->aux.dev     = &dptx_drv->pdev->dev;
	am_eDP->aux.transfer = eDP_aux_transfer;
	am_eDP->aux.wait_hpd_asserted = eDP_wait_hpd_asserted;
	am_eDP->aux.is_remote = 0;
	// am_eDP->aux.drm_dev = drm;
	ret = drm_dp_aux_register(&am_eDP->aux);
	if (ret)
		DRM_ERROR("Failed to drm_dp_aux_register");

	drm_panel_init(&am_eDP->panel, &dptx_drv->pdev->dev,
			&meson_eDP_panel_funcs, DRM_MODE_CONNECTOR_eDP);

	if (meson_eDP_get_eDPTX_aux_bl_available(type)) {
		ret = drm_panel_dp_aux_backlight(&am_eDP->panel, &am_eDP->aux);
		if (ret)
			DRM_ERROR("Failed to get aux backlight\n");
	} else {
		ret = drm_panel_of_backlight(&am_eDP->panel);
		if (ret)
			DRM_ERROR("Failed to get backlight\n");
	}

	am_eDP->panel.funcs = &meson_eDP_panel_funcs;

	drm_panel_add(&am_eDP->panel);

	DRM_INFO("%s: bind %d success\n", __func__, type);
	return 0;

free_resource:
	if (connector)
		drm_connector_cleanup(connector);
	if (encoder)
		drm_encoder_cleanup(encoder);
	kfree(am_eDP);

	DRM_DEBUG("%d Exit\n", ret);
	return ret;
}

int meson_eDP_dev_unbind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct drm_connector *connector = intf->conn;
	struct meson_eDP *drm_eDP = 0;

	if (!connector)
		DRM_ERROR("got invalid connector id\n");

	drm_eDP = connector_to_meson_eDP(connector);
	if (!drm_eDP)
		return -EINVAL;

	drm_eDP->base.connector.funcs->destroy(&drm_eDP->base.connector);
	drm_eDP->encoder.funcs->destroy(&drm_eDP->encoder);

	kfree(drm_eDP);
	DRM_DEBUG("[%d] called\n", __LINE__);
	return 0;
}

int am_meson_eDP_get_vrr_range(struct drm_connector *connector,
			struct drm_vrr_mode_group *groups, int max_group)
{
	int ret, modes_cnt, i;
	struct meson_eDP *am_eDP = connector_to_meson_eDP(connector);
	struct meson_panel_dev *eDP_dev = am_eDP->eDP_dev;

	if (!eDP_dev->get_modes_vrr_range)
		return 0;

	ret = eDP_dev->get_modes_vrr_range(eDP_dev, (void *)groups, max_group, &modes_cnt);
	if (ret) {
		DRM_ERROR("get vrr error or not support qms\n");
		return 0;
	}

	for (i = 0; i < modes_cnt; i++) {
		groups[i].vrr_min /= VRR_DIV;
		groups[i].vrr_max /= VRR_DIV;
	}

	return modes_cnt;
}
