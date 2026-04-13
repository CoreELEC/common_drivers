// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/of_platform.h>
#include <linux/spinlock.h>

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>
#include <drm/amlogic/meson_connector_dev.h>
#include <drm/display/drm_hdcp_helper.h>

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

static int meson_dptx_mode_probed_add(int count, struct tx_timing *timings,
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

static int meson_dptx_get_modes(struct drm_connector *connector)
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
	unsigned long flags = 0;
	u32 vrr_cap = 0;

	if (!meson_dptx || !meson_dptx->dptx_dev || !tx_comm)
		return count;

	raw_edid = meson_tx_get_raw_edid(&tx_comm->base);
	if (raw_edid) {
		edid = (struct edid *)raw_edid;
		drm_connector_update_edid_property(connector, edid);
	}

	sequence_id = dptx_state->base_state.sequence_id;
	new_sequence_id = meson_tx_get_hpd_hw_sequence_id(&tx_comm->base);
	/*
	 * After set mode, hwc will update the connector.
	 * In order to prevent the edid from being parsed every time,
	 * the sequence_id judgment is added, and the edid is only parsed
	 * when the hot_plug time occurs.
	 */
	if (sequence_id != new_sequence_id) {
		dptx_state->base_state.sequence_id = new_sequence_id;
		spin_lock_irqsave(&tx_comm->edid_spinlock, flags);
		meson_tx_clear_rx_cap(&tx_comm->base.rxcap);
		meson_tx_edid_parse(&tx_comm->base.rxcap, tx_comm->base.edid_buf,
			tx_comm->base.edid_parse_mask);
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
	}

	/* get vrr capability */
	vrr_cap = dptx_common_get_vrr_cap(tx_comm);
	DRM_DEBUG("support vrr_cap[%d]\n", vrr_cap);
	drm_connector_set_vrr_capable_property(connector, vrr_cap);

	count = dptx_get_mode_list(tx_comm, &timing_list);
	if (count > 0) {
		meson_dptx_mode_probed_add(count, timing_list, connector);
		vfree(timing_list);
		timing_list = NULL;
	}

	DRM_INFO("%s get %d modes\n", __func__, count);
	return 0;
}

static enum drm_mode_status meson_dptx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static int meson_dptx_atomic_check(struct drm_connector *connector,
	struct drm_atomic_state *state)
{
	struct meson_dptx_connector_state *new_state, *curr_state;
	struct drm_crtc_state *new_crtc_state = NULL;
	struct am_meson_crtc_state *meson_crtc_state;
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	unsigned int dptx_content_type = dptx_common_get_content_types(tx_comm);

	if (!state) {
		DRM_ERROR("state is NULL.\n");
		return -EINVAL;
	}

	curr_state = to_dptx_connector_state
		(drm_atomic_get_old_connector_state(state, connector));
	new_state = to_dptx_connector_state
		(drm_atomic_get_new_connector_state(state, connector));

	/*check content type.*/
	if (((1 << new_state->base.content_type) &
		dptx_content_type) == 0) {
		DRM_ERROR("check content type[%d-%u] fail\n",
			new_state->base.content_type,
			dptx_content_type);
		return -EINVAL;
	}

	if (new_state->base.crtc)
		new_crtc_state = drm_atomic_get_new_crtc_state(state,
			new_state->base.crtc);
	else
		return 0;

	meson_crtc_state = to_am_meson_crtc_state(new_crtc_state);

	if (state->allow_modeset && new_crtc_state) {
		DRM_DEBUG("update[%d]\n",
			new_state->update);

		if (new_state->update)
			new_crtc_state->connectors_changed = true;
	}
	DRM_DEBUG("%px %px\n", curr_state, new_state);

	return 0;
}

static struct drm_encoder *meson_dptx_best_encoder(struct drm_connector *connector)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);

	return &meson_dptx->encoder;
}

static const struct drm_connector_helper_funcs meson_dptx_connector_helper_funcs = {
	.get_modes = meson_dptx_get_modes,
	.mode_valid = meson_dptx_check_mode,
	.atomic_check	= meson_dptx_atomic_check,
	.best_encoder = meson_dptx_best_encoder,
};

static enum drm_connector_status meson_dptx_connector_detect
	(struct drm_connector *connector, bool force)
{
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	int hpd_state = meson_tx_get_hpd_state(&tx_comm->base);

	DRM_DEBUG("dptx_connector_detect [%d]\n", hpd_state);
	return hpd_state == 1 ?
		connector_status_connected : connector_status_disconnected;
}

static int _get_hdr_info(const struct hdr_info *hdr)
{
	int hdr_cap_value = 0;
	const struct hdr10_plus_info *hdr10p = &hdr->hdr10plus_info;

	/*
	 * hdr_cap_value:
	 *   bit0 SDR
	 *   bit1 HDR
	 *   bit2 SMPTE2084
	 *   bit3 HLG
	 *   bit4 HDR10PLUS
	 */
	hdr_cap_value = hdr->hdr_support;
	/* hdr10plus_supported */
	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdr10p->application_version != 0xFF)
		hdr_cap_value |= BIT(4);

	return hdr_cap_value;
}

static int get_hdr_info_rx(struct dptx_common *tx_comm)
{
	const struct hdr_info *hdr = dptx_common_get_hdr_info_rx(tx_comm);

	return _get_hdr_info(hdr);
}

static int get_sink_type(struct dptx_common *tx_comm)
{
	/*
	 * TODO: Repeater check for HDMI sink.
	 * Need confirm for DP.
	 */
	return BIT(2);
}

static int get_dc_cap(struct dptx_common *tx_comm)
{
	struct rx_cap *prxcap = &tx_comm->base.rxcap;
	const struct dv_info *dv =  &prxcap->dv_info;
	int i, dc_cap_mask = 0;

	if (prxcap->dc_36bit_420)
		/* 420,12bit */
		dc_cap_mask |= BIT(COLOR_YCBCR420_12BIT);
	if (prxcap->dc_30bit_420)
		/* 420,10bit */
		dc_cap_mask |= BIT(COLOR_YCBCR420_10BIT);

	for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
		if (prxcap->y420_vic[i]) {
			/* 420,8bit */
			dc_cap_mask |= BIT(COLOR_YCBCR420_8BIT);
			break;
		}
	}

	if (prxcap->native_Mode & CAP_BIT5_YCBCR_444_MASK) {
		if (prxcap->dc_y444) {
			if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
				/* 444,12bit */
				dc_cap_mask |= BIT(COLOR_YCBCR444_12BIT);
			if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1) {
				/* 444,10bit */
				dc_cap_mask |= BIT(COLOR_YCBCR444_10BIT);
			}
		}
		/* 444,8bit */
		dc_cap_mask |= BIT(COLOR_YCBCR444_8BIT);
	}
	/* y422, not check dc */
	if (prxcap->native_Mode & CAP_BIT4_YCBCR_422_MASK)
		/* 422,12bit */
		dc_cap_mask |= BIT(COLOR_YCBCR422_12BIT);

	if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
		/* rgb,12bit */
		dc_cap_mask |= BIT(COLOR_RGB_12BIT);
	if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
		/* rgb,10bit */
		dc_cap_mask |= BIT(COLOR_RGB_10BIT);
	/* rgb,8bit */
	dc_cap_mask |= BIT(COLOR_RGB_8BIT);
	return dc_cap_mask;
}

void get_dptx_metadata(struct dptx_common *tx_comm,
	struct meson_hdr_static_metadata *mHdrMetaDataValue)
{
	const struct hdr_info hdrinfo = tx_comm->base.rxcap.hdr_info;

	mHdrMetaDataValue->lumi_max = hdrinfo.lumi_max;
	mHdrMetaDataValue->lumi_min = hdrinfo.lumi_min;
	mHdrMetaDataValue->lumi_avg = hdrinfo.lumi_avg;
}

static int meson_dptx_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(state);
	struct tx_color_attr *attr = &dptx_state->color_attr_para;
	struct meson_tx_state *tx_state = &dptx_state->base_state;

	if (property == meson_dptx->color_depth_prop)
		attr->bitdepth = val;
	else if (property == meson_dptx->color_space_prop)
		attr->colorformat = val;
	else if (property == meson_dptx->hdcp_content_type0_pri_prop) {
		meson_dptx->hdcp_content_type0_pri = val;
		dptx_state->hdcp_force_st = true;
	} else if (property == meson_dptx->hdcp_content_type0_pri_store_prop)
		meson_dptx->hdcp_content_type0_pri = val;
	else if (property == meson_dptx->update_attr_prop)
		dptx_state->update = true;
	else if (property == meson_dptx->avmute_prop)
		dptx_state->avmute = val;
	else if (property == meson_dptx->hdr_priority_prop)
		tx_state->hdr_priority = val;
	else
		return -EINVAL;

	return 0;
}

static int meson_dptx_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(state);
	struct tx_color_attr *attr = &dptx_state->color_attr_para;
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	struct meson_tx_state *tx_state = &dptx_state->base_state;
	struct meson_hdr_static_metadata mHdrMetaDataValue;
	struct drm_property_blob *new_metadata, *old_metadata;
	bool replaced;

	if (property == meson_dptx->color_depth_prop)
		*val = attr->bitdepth;
	else if (property == meson_dptx->color_space_prop)
		*val = attr->colorformat;
	else if (property == meson_dptx->type_prop)
		*val = meson_dptx->dptx_type;
	else if (property == meson_dptx->hdcp_mode_prop)
		*val = dptx_common_get_hdcp_mode(tx_comm);
	else if (property == meson_dptx->hdcp_content_type0_pri_prop)
		*val = meson_dptx->hdcp_content_type0_pri;
	else if (property == meson_dptx->hdcp_content_type0_pri_store_prop)
		*val = meson_dptx->hdcp_content_type0_pri;
	else if (property == meson_dptx->hdcp_user_prop)
		*val = dptx_common_get_hdcp_user_state(tx_comm);
	else if (property == meson_dptx->hdr_cap_rx_prop)
		*val = get_hdr_info_rx(tx_comm);
	else if (property == meson_dptx->ready_prop)
		*val = dptx_common_get_ready_state(tx_comm);
	else if (property == meson_dptx->update_attr_prop)
		*val = 0;
	else if (property == meson_dptx->avmute_prop)
		*val = dptx_state->avmute;
	else if (property == meson_dptx->dc_cap_prop)
		*val = get_dc_cap(tx_comm);
	else if (property == meson_dptx->edid_valid_prop)
		*val = dptx_common_get_edid_valid_state(tx_comm);
	else if (property == meson_dptx->dptx_hdr_status_prop)
		*val = dptx_common_get_hdr_status(tx_comm);
	else if (property == meson_dptx->hdr_priority_prop)
		*val = tx_state->hdr_priority;
	else if (property == meson_dptx->sink_type_prop)
		*val = get_sink_type(tx_comm);
	else if (property == meson_dptx->contenttype_cap_prop)
		*val = dptx_common_get_content_types(tx_comm);
	else if (property == meson_dptx->static_meta_prop) {
		get_dptx_metadata(tx_comm, &mHdrMetaDataValue);
		old_metadata = dptx_state->metadata;
		new_metadata = drm_property_create_blob(connector->dev,
			sizeof(mHdrMetaDataValue), &mHdrMetaDataValue);
		if (IS_ERR(new_metadata)) {
			DRM_ERROR("create metadata blob fail.\n");
			return -EINVAL;
		}
		replaced = drm_property_replace_blob(&dptx_state->metadata,
			new_metadata);
		if (replaced && old_metadata)
			drm_property_blob_put(old_metadata);

		if (!replaced && new_metadata)
			drm_property_blob_put(new_metadata);
		*val = (dptx_state->metadata) ? dptx_state->metadata->base.id : 0;
	} else {
		return -EINVAL;
	}
	return 0;
}

static void meson_dptx_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

static void meson_dptx_reset(struct drm_connector *connector)
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

struct drm_connector_state *meson_dptx_atomic_duplicate_state
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
	new_state->update = false;
	tx_state->hdr_priority = curr_tx_state->hdr_priority;
	new_state->metadata = curr_state->metadata;

	return &new_state->base;
}

static void meson_dptx_atomic_destroy_state(struct drm_connector *connector,
	 struct drm_connector_state *state)
{
	struct meson_dptx_connector_state *dptx_state;

	dptx_state = to_dptx_connector_state(state);
	__drm_atomic_helper_connector_destroy_state(&dptx_state->base);
	kfree(dptx_state);
}

static void meson_dptx_atomic_print_state(struct drm_printer *p,
	const struct drm_connector_state *state)
{
	struct drm_connector *connector = state->connector;
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *dptx_state =
		to_dptx_connector_state(state);
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	struct meson_connector_dev *conn_dev = meson_dptx->dptx_dev;
	struct meson_tx_dev *tx_dev = to_meson_tx_dev(conn_dev);
	struct meson_tx_state *tx_state = &dptx_state->base_state;

	drm_printf(p, "\t\t update:[%d]\n", dptx_state->update);
	drm_printf(p, "\t\t\t cs: [%d], cd: [%d]\n",
		   dptx_state->color_attr_para.colorformat,
		   dptx_state->color_attr_para.bitdepth);
	drm_printf(p, "\t\t update:[%d]\n", dptx_state->update);
	drm_printf(p, "\t\t avmute:[%d]\n", dptx_state->avmute);
	drm_printf(p, "\t\t hdr_cap_rx:[%d]\n", get_hdr_info_rx(tx_comm));
	drm_printf(p, "\t\t ready:[%d]\n", tx_dev->ready);
	drm_printf(p, "\t\t VRR_CAP:[%u]\n", dptx_common_get_vrr_cap(tx_comm));

	drm_printf(p, "\t\t hdcp_user:[%d]\n", dptx_common_get_hdcp_user_state(tx_comm));
	drm_printf(p, "\t\t dc_cap:[%d]\n", get_dc_cap(tx_comm));
	drm_printf(p, "\t\t edid_valid:[%d]\n", dptx_common_get_edid_valid_state(tx_comm));
	drm_printf(p, "\t\t hdr_priority:[%d]\n", tx_state->hdr_priority);
	drm_printf(p, "\t\t dptx_hdr_status:[%d]\n", dptx_common_get_hdr_status(tx_comm));
	drm_printf(p, "\t\t sink_type:[%d]\n", get_sink_type(tx_comm));
	drm_printf(p, "\t\t contenttype_cap:[%d]\n", dptx_common_get_content_types(tx_comm));

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

static int meson_dptx_late_register(struct drm_connector *connector)
{
	int ret;
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);

	meson_tx_sysfs_create(&dptx_comm->base);

	ret = device_create_file(connector->kdev, &dev_attr_venc_cfg);
	if (ret)
		DRM_ERROR("Failed to create sysfs node: %d\n", ret);

	return ret;
}

static void meson_dptx_early_unregister(struct drm_connector *connector)
{
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);

	device_remove_file(connector->kdev, &dev_attr_venc_cfg);
	meson_tx_sysfs_remove(&dptx_comm->base);
}

static const struct drm_connector_funcs meson_dptx_connector_funcs = {
	.detect			= meson_dptx_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.atomic_set_property	= meson_dptx_connector_atomic_set_property,
	.atomic_get_property	= meson_dptx_connector_atomic_get_property,
	.destroy		= meson_dptx_connector_destroy,
	.reset			= meson_dptx_reset,
	.atomic_duplicate_state	= meson_dptx_atomic_duplicate_state,
	.atomic_destroy_state	= meson_dptx_atomic_destroy_state,
	.atomic_print_state	= meson_dptx_atomic_print_state,
	.late_register		= meson_dptx_late_register,
	.early_unregister	= meson_dptx_early_unregister,
};

static void meson_dptx_set_hdcp_result(struct meson_dp_tx *am_dptx, int result)
{
	struct drm_connector *connector = &am_dptx->base.connector;

	if (result == HDCP_AUTH_OK) {
		am_dptx->hdcp_state = DP_HDCP_RESULT_SUCCESS;
		am_dptx->content_protection = DRM_MODE_CONTENT_PROTECTION_ENABLED;
		schedule_work(&am_dptx->hdcp_update_work);
		DRM_DEBUG("hdcp [%d] set result ok.\n", am_dptx->hdcp_mode);
	} else if (result == HDCP_AUTH_FAIL) {
		am_dptx->hdcp_state = DP_HDCP_RESULT_FAIL;
		/*no event needed when fail.*/
		DRM_DEBUG("hdcp [%d] set result fail.\n", am_dptx->hdcp_mode);
	} else if (result == HDCP_AUTH_UNKNOWN) {
		/*reset property value to DESIRED.*/
		if (connector->state &&
			connector->state->content_protection ==
			DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			am_dptx->content_protection = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&am_dptx->hdcp_update_work);
		}
	}
}

static int meson_dptx_get_hdcp_request(struct meson_dp_tx *am_dptx,
	int request_type_mask)
{
	unsigned int hdcp_rx_type = am_dptx->hdcp_rx_type;
	struct drm_connector *connector = &am_dptx->base.connector;
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	unsigned int hdcp_tx_type;

	hdcp_tx_type = drm_dptx_common_get_tx_hdcp_cap(tx_comm);
	/*
	 * for bootup case, some project not have tee notify to load hdcp key,
	 * need to get rx_cap by dptx api.
	 * for hotplug case, rx_cap is updated in hpd callback.
	 */
	if (hdcp_rx_type == 0)
		hdcp_rx_type = drm_dptx_common_get_rx_hdcp_cap(tx_comm);

	DRM_INFO("%s usr_type: %d, hdcp cap: %d,%d\n",
			__func__, request_type_mask,
			hdcp_tx_type, hdcp_rx_type);

	return hdcp_tx_type & hdcp_rx_type & request_type_mask;
}

void meson_dptx_update_hdcp(struct meson_dp_tx *am_dptx, int hdcp_request)
{
	int hdcp_request_mode = HDCP_NULL;
	int hdcp_request_mask = HDCP_NULL;
	int hdcp_is_same = 0;
	struct drm_connector *conn = &am_dptx->base.connector;
	struct dptx_common *dptx_comm = connector_to_dptx_common(conn);

	DRM_INFO("meson update hdcp_request:%d\n", hdcp_request);

	/*Undesired, disable hdcp.*/
	if (am_dptx->hdcp_request_content_protection == DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		hdcp_request = DP_HDCP_STATE_STOP;

	if (hdcp_request == DP_HDCP_STATE_STOP || hdcp_request == DP_HDCP_STATE_DISCONNECT) {
		dptx_common_set_hdcp_mode(dptx_comm, HDCP_NULL, hdcp_request);
		am_dptx->hdcp_mode = HDCP_NULL;
		am_dptx->hdcp_state = hdcp_request;
		meson_dptx_set_hdcp_result(am_dptx, HDCP_AUTH_UNKNOWN);
		return;
	}

	if (am_dptx->hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0)
		hdcp_request_mask = HDCP_MODE14 | HDCP_MODE22;
	else
		hdcp_request_mask = HDCP_MODE22;

	if (am_dptx->hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0) {
		if (am_dptx->hdcp_content_type0_pri)
			hdcp_request_mask = HDCP_MODE14;
	}

	hdcp_request_mode = meson_dptx_get_hdcp_request(am_dptx, hdcp_request_mask);

	/*mode is same, try to re-use last state*/
	if (hdcp_request_mode & 2) {
		if (am_dptx->hdcp_mode == HDCP_MODE22)
			hdcp_is_same = 1;
	} else {
		if (am_dptx->hdcp_mode == HDCP_MODE14 && (hdcp_request_mode & 1))
			hdcp_is_same = 1;
	}

	if (hdcp_is_same) {
		switch (am_dptx->hdcp_state) {
		case DP_HDCP_STATE_START:
			DRM_INFO("waiting hdcp result.\n");
			return;
		case DP_HDCP_RESULT_SUCCESS:
			DRM_INFO("is auth ok.\n");
			meson_dptx_set_hdcp_result(am_dptx, HDCP_AUTH_OK);
			return;
		case DP_HDCP_RESULT_FAIL:
		default:
			DRM_ERROR("meet stopped hdcp stat\n");
			break;
		};
	}

	if (am_dptx->hdcp_force) {
		am_dptx->hdcp_force = false;
		DRM_INFO("force hdcp\n");
	} else {
		dptx_common_set_hdcp_mode(dptx_comm, HDCP_NULL, DP_HDCP_STATE_STOP);
		am_dptx->hdcp_mode = HDCP_NULL;
		am_dptx->hdcp_state = DP_HDCP_STATE_STOP;
	}

	meson_dptx_set_hdcp_result(am_dptx, HDCP_AUTH_UNKNOWN);

	if (hdcp_request_mode != HDCP_NULL &&
		(hdcp_request == DP_HDCP_STATE_START || hdcp_request == DP_HDCP_FORCE ||
		hdcp_request == DP_HDCP_KEY_READY)) {
		if (hdcp_request_mode & 2)
			am_dptx->hdcp_mode = HDCP_MODE22;
		else
			am_dptx->hdcp_mode = HDCP_MODE14;

		am_dptx->hdcp_state = DP_HDCP_STATE_START;
		dptx_common_set_hdcp_mode(dptx_comm, hdcp_request_mode, hdcp_request);
	} else {
		DRM_ERROR("No valid hdcp mode exit, maybe hdcp havenot init.\n");
	}
}

static void meson_dptx_update(struct drm_connector_state *new_state,
			struct drm_connector_state *old_state)
{
	struct drm_connector *connector = new_state->connector;
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct meson_dptx_connector_state *new_dptx_state =
		to_dptx_connector_state(new_state);
	int hdcp_request;

	if (old_state->hdcp_content_type
			!= new_state->hdcp_content_type ||
		(old_state->content_protection !=
			new_state->content_protection &&
		new_state->content_protection !=
			DRM_MODE_CONTENT_PROTECTION_ENABLED) ||
			new_dptx_state->hdcp_force_st) {
		/*check hdcp property update*/
		meson_dptx->hdcp_request_content_type =
			new_state->hdcp_content_type;
		meson_dptx->hdcp_request_content_protection =
			new_state->content_protection;

		if (new_dptx_state->hdcp_force_st) {
			meson_dptx->hdcp_force = true;
			hdcp_request = DP_HDCP_FORCE;
		} else {
			hdcp_request = DP_HDCP_STATE_START;
		}

		if (new_state->crtc && !drm_atomic_crtc_needs_modeset(new_state->crtc->state))
			meson_dptx_update_hdcp(meson_dptx, hdcp_request);
	}
}

static void meson_dptx_hdcp_notify(void *data, int type, int result)
{
	struct meson_dp_tx *am_dptx = (struct meson_dp_tx *)data;
	struct drm_connector *connector = &am_dptx->base.connector;
	struct dptx_common *dptx_comm = connector_to_dptx_common(connector);
	struct drm_modeset_lock *mode_lock =
		&connector->dev->mode_config.connection_mutex;
	char event_string[64];
	char *envp[2];
	struct drm_device *dev = connector->dev;

	drm_modeset_lock(mode_lock, NULL);

	if (type == HDCP_KEY_UPDATE && result == HDCP_AUTH_UNKNOWN) {
		DRM_INFO("HDCP statue changed, need re-run hdcp\n");
		if (meson_tx_get_hpd_state(&dptx_comm->base))
			am_dptx->hdcp_rx_type = drm_dptx_common_get_rx_hdcp_cap(dptx_comm);
		if (!am_dptx->dptx_on)
			goto end;
		meson_dptx_update_hdcp(am_dptx, DP_HDCP_KEY_READY);
		goto end;
	}
	if (!am_dptx->dptx_on)
		goto end;

	am_dptx->hdcp_mode = type;
	if (0) {
		if (type != am_dptx->hdcp_mode) {
			DRM_DEBUG("notify type is mismatch[%d]-[%d]\n",
				type, am_dptx->hdcp_mode);
			goto end;
		}
	}

	if (result == HDCP_AUTH_OK)
		meson_dptx_set_hdcp_result(am_dptx, HDCP_AUTH_OK);
	else if (result == HDCP_AUTH_FAIL)
		meson_dptx_set_hdcp_result(am_dptx, HDCP_AUTH_FAIL);
	else
		DRM_ERROR("HDCP report unknown result [%d-%d]\n", type, result);

	/*hdcp uevent*/
	memset(envp, 0, sizeof(envp));
	envp[0] = event_string;
	envp[1] = NULL;
	snprintf(event_string, sizeof(event_string),
				"dptx hdcp uevent, mode=%d,result=%d", type, result);

	DRM_DEBUG("dptx hdcp uevent, mode=%d,result=%d\n", type, result);

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);
end:
	drm_modeset_unlock(mode_lock);
}

static void meson_dptx_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc_state->crtc);
	struct meson_dp_tx *meson_dptx = encoder_to_meson_dptx(encoder);
	struct meson_connector_dev *tx_dev = meson_dptx->dptx_dev;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	char *modename = adj_mode->name;

	update_curr_vout_server(amcrtc->base.index + 1, tx_dev->vout_serv);
	meson_crtc_state->viu_mux = vout_func_get_viu_mux(amcrtc->vout_index,
		tx_dev->vout_serv, modename);
}

static void meson_dptx_encoder_atomic_enable(struct drm_encoder *encoder,
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

	meson_dptx_update_hdcp(meson_dptx, DP_HDCP_STATE_START);

	meson_vout_notify_mode_change(amcrtc->vout_index,
				      meson_crtc_state->vmode, EVENT_MODE_SET_START);
	/* assume hdmi_if_index = venc_idx by default */
	meson_tx_build_clk_param(tx_dev, &dptx_conn_state->base_state.para,
		meson_dptx->enc_idx, meson_dptx->enc_idx);
	meson_tx_do_mode_setting(tx_dev, &dptx_conn_state->base_state,
				 &old_dptx_conn_state->base_state);
	meson_venc_mode_set(meson_dptx->venc, meson_dptx->enc_idx, VENC_ENCP,
			    VENC_BIST_PTTN_OFF, &dptx_conn_state->base_state.para);
	meson_vout_notify_mode_change(amcrtc->vout_index,
				      meson_crtc_state->vmode, EVENT_MODE_SET_FINISH);
	meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "dptx");
	meson_dptx->dptx_on = 1;
}

static void meson_dptx_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_meson_crtc *amcrtc;
	struct drm_connector_state *old_conn_state;
	struct meson_dp_tx *meson_dptx = encoder_to_meson_dptx(encoder);
	struct drm_connector *conn = &meson_dptx->base.connector;

	DRM_INFO("[%s]\n", __func__);

	old_conn_state = drm_atomic_get_old_connector_state(state, conn);
	amcrtc = to_am_meson_crtc(old_conn_state->crtc);
	meson_dptx->dptx_on = 0;

	meson_venc_mode_disable(meson_dptx->venc, meson_dptx->enc_idx, VENC_ENCP);
	meson_dptx_update_hdcp(meson_dptx, DP_HDCP_STATE_STOP);
}

static enum drm_mode_status meson_dptx_encoder_mode_valid(struct drm_encoder *crtc,
				   const struct drm_display_mode *mode)
{
	return MODE_OK;
}

static int meson_dptx_encoder_atomic_check(struct drm_encoder *encoder,
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

static const struct drm_encoder_helper_funcs meson_dptx_encoder_helper_funcs = {
	.atomic_mode_set	= meson_dptx_encoder_atomic_mode_set,
	.atomic_enable		= meson_dptx_encoder_atomic_enable,
	.atomic_disable		= meson_dptx_encoder_atomic_disable,
	.atomic_check		= meson_dptx_encoder_atomic_check,
	.mode_valid			= meson_dptx_encoder_mode_valid,
};

static const struct drm_encoder_funcs meson_dptx_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static void meson_dptx_hdcp_work_func(struct work_struct *work)
{
	struct meson_dp_tx *am_dptx = container_of(work, struct meson_dp_tx, hdcp_update_work);
	struct drm_connector *connector = &am_dptx->base.connector;
	struct drm_modeset_lock *mode_lock = &connector->dev->mode_config.connection_mutex;

	drm_modeset_lock(mode_lock, NULL);
	drm_hdcp_update_content_protection(connector, am_dptx->content_protection);
	drm_modeset_unlock(mode_lock);
}

static void meson_dptx_hpd_cb(void *data)
{
	struct meson_connector *meson_con = (struct meson_connector *)data;
	struct drm_connector *connector = &meson_con->connector;
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);
	struct dptx_common *tx_comm = connector_to_dptx_common(connector);
	struct drm_modeset_lock *mode_lock =
			&connector->dev->mode_config.connection_mutex;

	DRM_INFO("drm dptx hpd notify\n");

	if (!meson_tx_get_hpd_state(&tx_comm->base)) {
		drm_modeset_lock(mode_lock, NULL);
		meson_dptx_update_hdcp(meson_dptx, DP_HDCP_STATE_DISCONNECT);
		meson_dptx->dptx_on = 0;
		drm_modeset_unlock(mode_lock);
	}

	meson_drm_kms_helper_hotplug_event(connector);
}

struct meson_range_prop_info {
	u64 init_val;
	u64 min;
	u64 max;
	u32 flags;
	char *prop_name;
};

static struct meson_range_prop_info range_props[] = {
	{0, 0, 1, 0, "hdcp_user"},
	{0, 0, 64, 0, "hdr_cap_rx"},
	{0, 0,  1 << COLOR_MAX_ATTR, 0, "dc_cap"},
	{0, 0, 1023, 0, "contenttype_cap"},
	{0, 0, 1, 0, "UPDATE"},
	{0, 0, 1, 0, "MESON_DRM_DPTX_PROP_AVMUTE"},
	{0, 0, 1, 0, "ready"},
	{0, 0, 1, 0, "edid_valid"},
	{0, 0, 8, 0, "sink_type"},
};

static void meson_dptx_create_range_property(struct drm_device *drm_dev,
						  struct meson_dp_tx *meson_dptx)
{
	int i;
	struct meson_range_prop_info *info;
	struct drm_property *prop;
	struct drm_property **props = &meson_dptx->hdcp_user_prop;

	for (i = 0; i < ARRAY_SIZE(range_props); i++, props++) {
		info = &range_props[i];
		prop = drm_property_create_range(drm_dev, info->flags, info->prop_name,
						 info->min, info->max);
		if (prop) {
			*(props) = prop;
			drm_object_attach_property(&meson_dptx->base.connector.base, prop,
						   info->init_val);
		} else {
			DRM_ERROR("Failed to %s property\n", info->prop_name);
		}
	}
}

static const struct drm_prop_enum_list dptx_hdr_status_enum_list[] = {
	{ HDR10PLUS_VSIF, "HDR10Plus-VSIF" },
	{ DOLBYVISION_STD, "DolbyVision-Std" },
	{ DOLBYVISION_LOWLATENCY, "DolbyVision-Lowlatency" },
	{ HDR10_GAMMA_ST2084, "HDR10-GAMMA_ST2084" },
	{ HDR10_OTHERS, "HDR10-others" },
	{ HDR10_GAMMA_HLG, "HDR10-GAMMA_HLG" },
	{ SDR, "SDR" }
};

static void meson_dptx_init_dptx_hdr_status_property(struct drm_device *drm_dev,
						  struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "dptx_hdr_status",
					dptx_hdr_status_enum_list,
					ARRAY_SIZE(dptx_hdr_status_enum_list));
	if (prop) {
		meson_dptx->dptx_hdr_status_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dptx_hdr_status property\n");
	}
}

/* Optional colorspace properties. */
static const struct drm_prop_enum_list color_space_enum_list[] = {
	{ HDMI_COLORSPACE_RGB, "RGB" },
	{ HDMI_COLORSPACE_YUV422, "422" },
	{ HDMI_COLORSPACE_YUV444, "444" },
	{ HDMI_COLORSPACE_YUV420, "420" },
	{ HDMI_COLORSPACE_RESERVED6, "HDMI_COLORSPACE_RESERVED6" }
};

static void meson_dptx_init_colorspace_property(struct drm_device *drm_dev,
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

static void meson_dptx_init_colordepth_property(struct drm_device *drm_dev,
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

static void meson_dptx_init_hdcp_content_type0_pri_property(struct drm_device *drm_dev,
						struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "HDCP_CONTENT_TYPE0_PRIORITY");
	if (prop) {
		meson_dptx->hdcp_content_type0_pri_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to HDCP_CONTENT_TYPE0_PRIORITY property\n");
	}
}

static void meson_dptx_init_hdcp_content_type0_pri_store_property(struct drm_device *drm_dev,
						struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "HDCP_CONTENT_TYPE0_PRI_STORE");
	if (prop) {
		meson_dptx->hdcp_content_type0_pri_store_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to HDCP_CONTENT_TYPE0_PRI_STORE property\n");
	}
}

static void meson_hdmitx_init_hdcp_mode_property(struct drm_device *drm_dev,
						struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "hdcp_mode", 0, 36);

	if (prop) {
		meson_dptx->hdcp_mode_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdcp_mode property\n");
	}
}

static void meson_dptx_init_hdr_priority_property(struct drm_device *drm_dev,
						  struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;
	u32 hdr_priority;
	struct dptx_common *dptx_comm = conn_to_dptx_common(meson_dptx->dptx_dev);

	dptx_get_hdr_priority(dptx_comm, &hdr_priority);
	prop = drm_property_create_range(drm_dev, 0,
			"hdr_priority", 0, UINT_MAX);

	if (prop) {
		meson_dptx->hdr_priority_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, hdr_priority);
	} else {
		DRM_ERROR("Failed to allm property\n");
	}
}

static void meson_dptx_init_static_meta_property(struct drm_device *drm_dev,
						  struct meson_dp_tx *meson_dptx)
{
	struct drm_property *prop;

	prop = drm_property_create(drm_dev, DRM_MODE_PROP_BLOB, "HDR_STATIC_META", 0);
	if (prop) {
		meson_dptx->static_meta_prop = prop;
		drm_object_attach_property(&meson_dptx->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to create hdr static metadata property\n");
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

static void meson_dptx_parse_venc_idx(struct device *dev, u32 *enc_idx)
{
	int ret;

	ret = of_property_read_u32(dev->of_node, "enc_idx", enc_idx);
	if (ret)
		DRM_ERROR("enc_idx invalid\n");
}

int meson_dptx_dev_bind(struct drm_device *drm,
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
	struct connector_hpd_cb hpd_cb;
	struct meson_tx_state *tx_state;
	int connector_type = type;
	char *connector_name = NULL;
	struct drm_property *type_prop = NULL;
	struct connector_hdcp_cb hdcp_cb;
	int hdcp_ctl_lvl;

	meson_dptx = devm_kzalloc(drm->dev, sizeof(*meson_dptx), GFP_KERNEL);
	if (!meson_dptx)
		return -ENOMEM;

	meson_dptx->dptx_dev = intf;
	tx_dev  = to_meson_tx_dev(intf);
	tx_comm = to_dptx_common(tx_dev);

	hdcp_ctl_lvl = tx_comm->hdcptx_comm.hdcp_ctl_lvl;
	DRM_DEBUG("hdcp_ctl_lvl=%d\n", hdcp_ctl_lvl);

	if (hdcp_ctl_lvl == 0) {
		meson_dptx->hdcp_request_content_type =
				DRM_MODE_HDCP_CONTENT_TYPE0;
		meson_dptx->hdcp_request_content_protection =
				DRM_MODE_CONTENT_PROTECTION_DESIRED;
	} else if (hdcp_ctl_lvl == 1) {
		meson_dptx->hdcp_request_content_type =
			DRM_MODE_HDCP_CONTENT_TYPE0;
		meson_dptx->hdcp_request_content_protection =
			DRM_MODE_CONTENT_PROTECTION_DESIRED;
	} else if (hdcp_ctl_lvl == 2) {
		meson_dptx->hdcp_request_content_type =
			DRM_MODE_HDCP_CONTENT_TYPE0;
		meson_dptx->hdcp_request_content_protection =
			DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
	} else {
		DRM_ERROR("no HDCP func registered.\n");
	}

	mesonconn = &meson_dptx->base;
	mesonconn->drm_priv = priv;
	mesonconn->update = meson_dptx_update;
	mesonconn->connector_type = type;
	mesonconn->data = tx_dev;
	encoder = &meson_dptx->encoder;
	connector = &meson_dptx->base.connector;
	intf->conn = connector;
	meson_dptx->dptx_type = type;
	INIT_WORK(&meson_dptx->hdcp_update_work, meson_dptx_hdcp_work_func);

	DRM_INFO("[%s]: type = %d\n", __func__, type);

	switch (type) {
	case DRM_MODE_CONNECTOR_MESON_TX_DP_A:
		connector_type = DRM_MODE_CONNECTOR_DisplayPort;
		connector_name = "DP-A";
		break;
	case DRM_MODE_CONNECTOR_MESON_TX_DP_B:
		connector_type = DRM_MODE_CONNECTOR_DisplayPort;
		connector_name = "DP-B";
		break;
	case DRM_MODE_CONNECTOR_MESON_TX_DP_C:
		connector_type = DRM_MODE_CONNECTOR_DisplayPort;
		connector_name = "DP-C";
		break;
	default:
		connector_type = DRM_MODE_CONNECTOR_Unknown;
		encoder_type = DRM_MODE_ENCODER_NONE;
		break;
	};

	intf->connector_type = type;

	meson_drm_probe_venc(meson_dptx, priv->dev);
	meson_dptx_parse_venc_idx(tx_dev->pdev, &meson_dptx->enc_idx);

	/* Connector */
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->ycbcr_420_allowed = true;
	drm_connector_helper_add(connector,
				 &meson_dptx_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &meson_dptx_connector_funcs,
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
	drm_encoder_helper_add(encoder, &meson_dptx_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_dptx_encoder_funcs,
			       encoder_type, "dptx_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}

	drm_connector_attach_encoder(connector, encoder);

	/* register hpd callback */
	hpd_cb.callback = meson_dptx_hpd_cb;
	hpd_cb.data = mesonconn;
	meson_tx_register_hpd_cb(tx_dev, &hpd_cb);

	/* register connector sysfs device translate to meson_tx_dev */
	dptx_register_dev_to_txdev_xlate(tx_dev, conn_dev_to_meson_tx_dev);

	/* init dptx property */
	tx_state = devm_kzalloc(drm->dev, sizeof(*tx_state), GFP_KERNEL);
	if (!tx_state)
		return -ENOMEM;
	/*hdcp init, default is disable state*/
	drm_connector_attach_content_protection_property(connector, true);
	hdcp_cb.hdcp_notify = meson_dptx_hdcp_notify;
	hdcp_cb.data = meson_dptx;
	drm_dptx_common_register_hdcp_notify(tx_comm, &hdcp_cb);
	meson_dptx->hdcp_mode = HDCP_NULL;
	meson_dptx->hdcp_state = DP_HDCP_STATE_DISCONNECT;

	drm_connector_attach_content_type_property(connector);
	drm_connector_attach_vrr_capable_property(connector);

	meson_tx_get_init_state(tx_dev, tx_state);
	meson_dptx_init_dptx_hdr_status_property(drm, meson_dptx);
	meson_dptx_init_colordepth_property(drm, meson_dptx, tx_state);
	meson_dptx_init_colorspace_property(drm, meson_dptx, tx_state);
	meson_dptx_init_hdcp_content_type0_pri_property(drm, meson_dptx);
	meson_dptx_init_hdcp_content_type0_pri_store_property(drm, meson_dptx);
	meson_hdmitx_init_hdcp_mode_property(drm, meson_dptx);
	meson_dptx_create_range_property(drm, meson_dptx);
	meson_dptx_init_static_meta_property(drm, meson_dptx);
	meson_dptx_init_hdr_priority_property(drm, meson_dptx);

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

int meson_dptx_dev_unbind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct drm_connector *connector = intf->conn;
	struct meson_dp_tx *meson_dptx = connector_to_meson_dptx(connector);

	cancel_work_sync(&meson_dptx->hdcp_update_work);
	return 0;
}
