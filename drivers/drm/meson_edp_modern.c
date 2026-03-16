// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/of_platform.h>

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>

#include <drm/amlogic/meson_connector_dev.h>
#include <vout/vout_serve/vout_func.h>

#include "meson_crtc.h"
#include "meson_dptx.h"
#include "meson_venc.h"

static struct dptx_common *connector_to_dptx_common(struct drm_connector *connector)
{
	struct meson_dp_tx *meson_dptx = NULL;

	meson_dptx = connector_to_meson_dptx(connector);

	return conn_to_dptx_common(meson_dptx->dptx_dev);
}

static struct meson_tx_dev *conn_dev_to_meson_tx_dev(struct device *dev)
{
	struct dptx_common *dptx_comm = NULL;
	struct drm_connector *connector = dev_get_drvdata(dev);

	dptx_comm = connector_to_dptx_common(connector);

	return &dptx_comm->base;
}

static int meson_edp_mode_probed_add(int count, struct tx_timing *timings,
	struct drm_connector *connector)
{
	int i;
	struct drm_display_mode *mode;
	struct tx_timing *timing = timings;

	for (i = 0; i < count; i++, timing++) {
		mode = drm_mode_create(connector->dev);
		if (!mode) {
			DRM_ERROR("drm mode create failed.\n");
			continue;
		}

		meson_connector_fill_mode_timing(mode, timing, false);

		drm_mode_probed_add(connector, mode);
		DRM_DEBUG("add mode [%s]\n", mode->name);
	}

	return 0;
}

static int meson_edp_get_modes(struct drm_connector *connector)
{
	struct edid *edid;
	unsigned char *raw_edid = NULL;
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(connector->state);
	struct tx_timing *timing_list = NULL;
	u64 sequence_id, new_sequence_id;
	int count = 0;

	if (!meson_dptx || !meson_dptx->dptx_dev || !tx_comm)
		return count;

	raw_edid = meson_tx_get_raw_edid(&tx_comm->base);
	if (raw_edid) {
		edid = (struct edid *)raw_edid;
		drm_connector_update_edid_property(connector, edid);
	}

	sequence_id = dptx_state->base_state.sequence_id;
	new_sequence_id = meson_tx_get_hpd_hw_sequence_id(&tx_comm->base);

	if (sequence_id != new_sequence_id)
		dptx_state->base_state.sequence_id = new_sequence_id;

	count = dptx_get_mode_list(tx_comm, &timing_list);
	if (count > 0) {
		meson_edp_mode_probed_add(count, timing_list, connector);
		vfree(timing_list);
		timing_list = NULL;
	}

	DRM_INFO("%s get %d modes\n", __func__, count);
	return 0;
}

static enum drm_mode_status meson_edp_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static int meson_edp_atomic_check(struct drm_connector *connector,
	struct drm_atomic_state *state)
{
	struct meson_dptx_connector_state *new_state, *curr_state;

	if (!state) {
		DRM_ERROR("state is NULL.\n");
		return -EINVAL;
	}

	curr_state = to_dptx_connector_state
		(drm_atomic_get_old_connector_state(state, connector));
	new_state = to_dptx_connector_state
		(drm_atomic_get_new_connector_state(state, connector));

	DRM_DEBUG("%px %px\n", curr_state, new_state);

	return 0;
}

static struct drm_encoder *meson_edp_best_encoder(struct drm_connector *connector)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);

	return &meson_dptx->encoder;
}

static const struct drm_connector_helper_funcs meson_edp_connector_helper_funcs = {
	.get_modes = meson_edp_get_modes,
	.mode_valid = meson_edp_check_mode,
	.atomic_check	= meson_edp_atomic_check,
	.best_encoder = meson_edp_best_encoder,
};

static enum drm_connector_status meson_edp_connector_detect
	(struct drm_connector *connector, bool force)
{
	return connector_status_connected;
}

static int meson_edp_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(state);
	struct tx_color_attr *attr = &dptx_state->color_attr_para;

	if (property == meson_dptx->color_depth_prop)
		attr->bitdepth = val;
	else if (property == meson_dptx->color_space_prop)
		attr->colorformat = val;
	else
		return -EINVAL;

	return 0;
}

static int meson_edp_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(state);
	struct tx_color_attr *attr = &dptx_state->color_attr_para;

	if (property == meson_dptx->color_depth_prop)
		*val = attr->bitdepth;
	else if (property == meson_dptx->color_space_prop)
		*val = attr->colorformat;
	else if (property == meson_dptx->type_prop)
		*val = meson_dptx->dptx_type;
	else
		return -EINVAL;

	return 0;
}

static void meson_edp_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static void meson_edp_reset(struct drm_connector *connector)
{
	struct dptx_common *dptx_comm;
	struct meson_tx_state *tx_state;
	struct meson_dptx_connector_state *dptx_state;

	dptx_comm = connector_to_dptx_common(connector);

	dptx_state = kzalloc(sizeof(*dptx_state), GFP_KERNEL);
	if (!dptx_state)
		return;

	if (connector->state)
		__drm_atomic_helper_connector_destroy_state(connector->state);
	kfree(connector->state);

	__drm_atomic_helper_connector_reset(connector, &dptx_state->base);

	tx_state = &dptx_state->base_state;
	tx_state->sequence_id = ~0ULL;
}

struct drm_connector_state *meson_edp_atomic_duplicate_state
	(struct drm_connector *connector)
{
	struct meson_tx_state *tx_state, *curr_tx_state;
	struct meson_dptx_connector_state *new_state;
	struct meson_dptx_connector_state *curr_state =
		to_dptx_connector_state(connector->state);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector,
		&new_state->base);

	tx_state = &new_state->base_state;
	curr_tx_state = &curr_state->base_state;
	tx_state->sequence_id = curr_tx_state->sequence_id;
	return &new_state->base;
}

static void meson_edp_atomic_destroy_state(struct drm_connector *connector,
	 struct drm_connector_state *state)
{
	struct meson_dptx_connector_state *dptx_state;

	dptx_state = to_dptx_connector_state(state);
	__drm_atomic_helper_connector_destroy_state(&dptx_state->base);
	kfree(dptx_state);
}

static void meson_edp_atomic_print_state(struct drm_printer *p,
	const struct drm_connector_state *state)
{
}

static ssize_t venc_cfg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "no support show\n");
}

/* bit[3] bist type, bit[2] VENC type, bit[1]: venc_index, bit[0]: enable
 * e.g. echo 1003 > venc_cfg for red pattern
 */
static ssize_t venc_cfg_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct drm_connector *connector = dev_get_drvdata(dev);
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	u32 enc_type = VENC_ENCP;
	u32 bist_type = VENC_BIST_PTTN_COLORBAR;

	if (buf[1] == '0' || buf[1] == '1')
		meson_dptx->enc_idx = buf[1] - '0';
	else
		meson_dptx->enc_idx = 0;
	if (buf[2] == '0' || buf[2] == '1')
		enc_type = buf[2] - '0';
	else
		enc_type = VENC_ENCP;
	if (buf[3])
		bist_type = buf[3] - '0';
	else
		bist_type = VENC_BIST_PTTN_COLORBAR;
	DRM_INFO("cfg venc_idx: %d, cfg venc_type: %s, bist type: %d\n",
		meson_dptx->enc_idx, enc_type == VENC_ENCL ? "ENCL" : "ENCP", bist_type);

	if (buf[0] == '1') {
		meson_venc_mode_set(meson_dptx->venc, meson_dptx->enc_idx,
			enc_type, bist_type, &dptx_comm->base.sw_fmt_para);
		DRM_INFO("enable venc\n");
	} else if (buf[0] == '0') {
		meson_venc_mode_disable(meson_dptx->venc, meson_dptx->enc_idx, enc_type);
		DRM_INFO("disable venc\n");
	} else {
		return -EINVAL;
	}

	return count;
}
static DEVICE_ATTR_RW(venc_cfg);

static int meson_edp_late_register(struct drm_connector *connector)
{
	int ret;
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);

	meson_tx_sysfs_create(&dptx_comm->base);
	ret = device_create_file(connector->kdev, &dev_attr_venc_cfg);
	if (ret)
		DRM_ERROR("Failed to create sysfs node: %d\n", ret);

	return 0;
}

static void meson_edp_early_unregister(struct drm_connector *connector)
{
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);

	device_remove_file(connector->kdev, &dev_attr_venc_cfg);
	meson_tx_sysfs_remove(&dptx_comm->base);
}

static const struct drm_connector_funcs meson_edp_connector_funcs = {
	.detect			= meson_edp_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.atomic_set_property	= meson_edp_connector_atomic_set_property,
	.atomic_get_property	= meson_edp_connector_atomic_get_property,
	.destroy		= meson_edp_connector_destroy,
	.reset			= meson_edp_reset,
	.atomic_duplicate_state	= meson_edp_atomic_duplicate_state,
	.atomic_destroy_state	= meson_edp_atomic_destroy_state,
	.atomic_print_state	= meson_edp_atomic_print_state,
	.late_register		= meson_edp_late_register,
	.early_unregister	= meson_edp_early_unregister,
};

static void meson_edp_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc_state->crtc);
	struct meson_dp_tx *meson_dptx = encoder_to_meson_dptx(encoder);
	struct meson_connector_dev *tx_dev = meson_dptx->dptx_dev;

	update_curr_vout_server(amcrtc->base.index + 1, tx_dev->vout_serv);
}

static void meson_edp_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct drm_connector_state *conn_state, *old_conn_state;
	struct meson_dptx_connector_state *dptx_conn_state, *old_dptx_conn_state;
	struct drm_crtc_state *crtc_state;
	struct am_meson_crtc_state *meson_crtc_state;
	struct am_meson_crtc *amcrtc;
	struct drm_display_mode *mode;
	struct meson_dp_tx *meson_dptx = encoder_to_meson_dptx(encoder);
	struct meson_connector_dev *conn_dev = meson_dptx->dptx_dev;
	struct meson_tx_dev *tx_dev = to_meson_tx_dev(conn_dev);
	struct drm_connector *conn = &meson_dptx->base.connector;

	conn_state = drm_atomic_get_new_connector_state(state, conn);
	dptx_conn_state = to_dptx_connector_state(conn_state);
	old_conn_state = drm_atomic_get_old_connector_state(state, conn);
	old_dptx_conn_state = to_dptx_connector_state(old_conn_state);

	amcrtc = to_am_meson_crtc(conn_state->crtc);
	crtc_state =  drm_atomic_get_new_crtc_state(state, conn_state->crtc);
	mode = &crtc_state->adjusted_mode;
	meson_crtc_state = to_am_meson_crtc_state(crtc_state);

	meson_vout_notify_mode_change(amcrtc->vout_index,
				      meson_crtc_state->vmode, EVENT_MODE_SET_START);
	/* assume hdmi_if_index = venc_idx by default */
	meson_tx_build_clk_param(tx_dev, &dptx_conn_state->base_state.para,
		meson_dptx->enc_idx, meson_dptx->enc_idx);
	meson_tx_do_mode_setting(tx_dev, &dptx_conn_state->base_state,
				 &old_dptx_conn_state->base_state);
	meson_venc_mode_set(meson_dptx->venc, meson_dptx->enc_idx, VENC_ENCL,
			    VENC_BIST_PTTN_OFF, &dptx_conn_state->base_state.para);
	meson_vout_notify_mode_change(amcrtc->vout_index,
				      meson_crtc_state->vmode, EVENT_MODE_SET_FINISH);
	meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "edp");
}

static void meson_edp_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_meson_crtc *amcrtc;
	struct drm_connector_state *old_conn_state;
	struct meson_dp_tx *meson_dptx = encoder_to_meson_dptx(encoder);
	struct drm_connector *conn = &meson_dptx->base.connector;

	old_conn_state = drm_atomic_get_old_connector_state(state, conn);
	amcrtc = to_am_meson_crtc(old_conn_state->crtc);

	meson_venc_mode_disable(meson_dptx->venc, meson_dptx->enc_idx, VENC_ENCL);
}

static enum drm_mode_status meson_edp_encoder_mode_valid(struct drm_encoder *crtc,
				   const struct drm_display_mode *mode)
{
	return MODE_OK;
}

static int meson_edp_encoder_atomic_check(struct drm_encoder *encoder,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state)
{
	int ret = 0;
	int frac_mode = 0;
	struct tx_timing *timing;
	struct dptx_common *dptx_comm = connector_to_dptx_common(conn_state->connector);
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(conn_state);
	struct meson_tx_state *tx_state = &dptx_state->base_state;
	struct tx_color_attr *attr = &dptx_state->color_attr_para;

	dptx_state = to_dptx_connector_state(conn_state);

	timing = kzalloc(sizeof(*timing), GFP_KERNEL);
	if (!timing)
		return -ENOMEM;

	meson_drm_mode_build_tx_timing(adj_mode, timing);

	meson_tx_format_para_init(&dptx_comm->base, timing,
				  frac_mode, attr->colorformat,
				  bitdepth_to_colordepth(attr->bitdepth), 0, &tx_state->para);
	ret = meson_tx_validate_mode(&dptx_comm->base, tx_state);
	if (!ret)
		meson_crtc_state->preset_vmode = VMODE_DPTX;
	else
		DRM_ERROR("validate fail %d\n", ret);

	kfree(timing);

	return ret;
}

static const struct drm_encoder_helper_funcs meson_edp_encoder_helper_funcs = {
	.atomic_mode_set	= meson_edp_encoder_atomic_mode_set,
	.atomic_enable		= meson_edp_encoder_atomic_enable,
	.atomic_disable		= meson_edp_encoder_atomic_disable,
	.atomic_check		= meson_edp_encoder_atomic_check,
	.mode_valid			= meson_edp_encoder_mode_valid,
};

static const struct drm_encoder_funcs meson_edp_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static void meson_edp_update(struct drm_connector_state *new_state,
			struct drm_connector_state *old_state)
{
}

/* Optional colorspace properties. */
static const struct drm_prop_enum_list color_space_enum_list[] = {
	{ HDMI_COLORSPACE_RGB, "RGB" },
	{ HDMI_COLORSPACE_YUV422, "422" },
	{ HDMI_COLORSPACE_YUV444, "444" },
	{ HDMI_COLORSPACE_YUV420, "420" },
	{ HDMI_COLORSPACE_RESERVED6, "HDMI_COLORSPACE_RESERVED6" }
};

static void meson_edp_init_colorspace_property(struct drm_device *drm_dev,
						struct meson_dp_tx *meson_dptx,
						struct meson_tx_state *state)
{
	struct drm_property *prop;
	struct dptx_common *dptx_comm;
	int colorformat = state->para.cs;

	dptx_comm = conn_to_dptx_common(meson_dptx->dptx_dev);

	prop = drm_property_create_enum(drm_dev, 0, "color_space",
					color_space_enum_list,
					ARRAY_SIZE(color_space_enum_list));
	if (prop) {
		meson_dptx->color_space_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop,
		colorformat);
	} else {
		DRM_ERROR("Failed to color_space property\n");
	}
}

static void meson_edp_init_colordepth_property(struct drm_device *drm_dev,
						struct meson_dp_tx *meson_dptx,
						struct meson_tx_state *state)
{
	struct drm_property *prop;
	struct dptx_common *dptx_comm;
	int bitdepth = state->para.cd;

	dptx_comm = conn_to_dptx_common(meson_dptx->dptx_dev);

	prop = drm_property_create_range(drm_dev, 0,
			"color_depth", 0, 16);

	if (prop) {
		meson_dptx->color_depth_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop,
		colordepth_to_bitdepth(bitdepth));
	} else {
		DRM_ERROR("Failed to color_depth property\n");
	}
}

static int meson_drm_probe_venc(struct meson_dp_tx *meson_dptx, struct device *dev)
{
	struct platform_device *venc_pdev;
	struct device_node *venc_node;

	venc_node = of_parse_phandle(dev->of_node, "tx_venc", 0);
	if (!venc_node) {
		dev_err(dev, "cannot find venc device\n");
		return -ENXIO;
	}

	venc_pdev = of_find_device_by_node(venc_node);
	if (venc_pdev) {
		meson_dptx->venc = platform_get_drvdata(venc_pdev);
		meson_dptx->venc_pdev = venc_pdev;
	}

	of_node_put(venc_node);

	if (!venc_pdev) {
		dev_err(dev, "%s: venc driver is not ready\n", __func__);
		return -EPROBE_DEFER;
	}
	if (!meson_dptx->venc) {
		put_device(&venc_pdev->dev);
		dev_err(dev, "%s: venc driver is not ready\n", __func__);
		return -EPROBE_DEFER;
	}

	return 0;
}

static void meson_edp_parse_venc_idx(struct device *dev, u32 *enc_idx)
{
	int ret;

	ret = of_property_read_u32(dev->of_node, "enc_idx", enc_idx);
	if (ret)
		DRM_ERROR("enc_idx invalid\n");
}

int meson_edp_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	int ret;
	struct meson_drm *priv = drm->dev_private;
	struct meson_dp_tx *meson_dptx;
	struct meson_tx_dev *tx_dev;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct meson_connector *mesonconn;
	struct dptx_common *tx_comm;
	int encoder_type = DRM_MODE_ENCODER_TMDS;
	struct meson_tx_state *tx_state;
	int connector_type = type;
	char *connector_name = NULL;
	struct drm_property *type_prop = NULL;

	meson_dptx = devm_kzalloc(drm->dev, sizeof(*meson_dptx), GFP_KERNEL);
	if (!meson_dptx)
		return -ENOMEM;

	meson_dptx->dptx_dev = intf;
	tx_dev  = to_meson_tx_dev(intf);
	tx_comm = to_dptx_common(tx_dev);

	mesonconn = &meson_dptx->base;
	mesonconn->drm_priv = priv;
	mesonconn->update = meson_edp_update;
	mesonconn->connector_type = type;
	mesonconn->data = tx_dev;
	encoder = &meson_dptx->encoder;
	connector = &meson_dptx->base.connector;
	intf->conn = connector;
	meson_dptx->dptx_type = type;

	DRM_INFO("[%s]: type = %d\n", __func__, type);

	switch (type) {
	case DRM_MODE_CONNECTOR_MESON_TX_EDP_A:
		connector_type = DRM_MODE_CONNECTOR_eDP;
		connector_name = "TX-EDP-A";
		break;
	case DRM_MODE_CONNECTOR_MESON_TX_EDP_B:
		connector_type = DRM_MODE_CONNECTOR_eDP;
		connector_name = "TX-EDP-B";
		break;
	case DRM_MODE_CONNECTOR_MESON_TX_EDP_C:
		connector_type = DRM_MODE_CONNECTOR_eDP;
		connector_name = "TX-EDP-C";
		break;
	default:
		connector_type = DRM_MODE_CONNECTOR_Unknown;
		encoder_type = DRM_MODE_ENCODER_NONE;
		break;
	};

	intf->connector_type = connector_type;

	meson_drm_probe_venc(meson_dptx, priv->dev);
	meson_edp_parse_venc_idx(tx_dev->pdev, &meson_dptx->enc_idx);

	/* Connector */
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->ycbcr_420_allowed = true;
	drm_connector_helper_add(connector,
				 &meson_edp_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &meson_edp_connector_funcs,
				 connector_type);
	if (ret) {
		dev_err(priv->dev, "Failed to init dp tx connector\n");
		return ret;
	}

	/*update name to amlogic name*/
	if (connector_name) {
		kfree(connector->name);
		connector->name = kasprintf(GFP_KERNEL, "%s", connector_name);
		if (!connector->name)
			DRM_ERROR("alloc name failed\n");
	}

	/* Encoder */
	encoder->possible_crtcs = priv->of_conf.crtc_masks[ENCODER_HDMI];
	drm_encoder_helper_add(encoder, &meson_edp_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_edp_encoder_funcs,
			       encoder_type, "dptx_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}

	drm_connector_attach_encoder(connector, encoder);

	/* register connector sysfs device translate to meson_tx_dev */
	dptx_register_dev_to_txdev_xlate(tx_dev, conn_dev_to_meson_tx_dev);

	/* init dptx property */
	tx_state = devm_kzalloc(drm->dev, sizeof(*tx_state), GFP_KERNEL);
	if (!tx_state)
		return -ENOMEM;

	meson_tx_get_init_state(tx_dev, tx_state);
	meson_edp_init_colordepth_property(drm, meson_dptx, tx_state);
	meson_edp_init_colorspace_property(drm, meson_dptx, tx_state);
	/*prop for userspace to acquire prop*/
	type_prop = drm_property_create_range(drm, DRM_MODE_PROP_IMMUTABLE,
		MESON_CONNECTOR_TYPE_PROP_NAME, 0, INT_MAX);
	if (type_prop) {
		meson_dptx->type_prop = type_prop;
		drm_object_attach_property(&meson_dptx->base.connector.base,
			type_prop, type);
	} else {
		DRM_ERROR("Failed to create property %s\n",
			MESON_CONNECTOR_TYPE_PROP_NAME);
	}

	kfree(tx_state);

	return 0;
}

int meson_edp_dev_unbind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	return 0;
}
