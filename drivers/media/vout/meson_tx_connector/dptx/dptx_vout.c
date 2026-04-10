// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_tx_internal.h"
#include "dptx_internal.h"
#include "dptx_hw_opcode.h"
#include "dptx_log.h"
#include "dptx_infoframe.h"

const struct tx_timing *meson_tx_mode_match_timing_name(const char *name);

static void dptx_physical_size_to_vinfo(struct dptx_common *tx_comm)
{
	u32 width, height;
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		DPTX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->base.tx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	if (info->mode == VMODE_DPTX) {
		width = prxcap->physical_width;
		height = prxcap->physical_height;
		if (width == 0 || height == 0) {
			info->screen_real_width = info->aspect_ratio_num;
			info->screen_real_height = info->aspect_ratio_den;
		} else {
			info->screen_real_width = width;
			info->screen_real_height = height;
		}
		DPTX_DEBUG("update physical size: %d %d\n",
			info->screen_real_width, info->screen_real_height);
	}
}

static void rxlatency_to_vinfo(struct dptx_common *tx_comm)
{
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		DPTX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->base.tx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	info->rx_latency.vLatency = prxcap->vLatency;
	info->rx_latency.aLatency = prxcap->aLatency;
	info->rx_latency.i_vLatency = prxcap->i_vLatency;
	info->rx_latency.i_aLatency = prxcap->i_aLatency;
}

static void edidinfo_attach_to_vinfo(struct dptx_common *tx_comm)
{
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		DPTX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->base.tx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	rxlatency_to_vinfo(tx_comm);
	dptx_physical_size_to_vinfo(tx_comm);
}

static void edidinfo_detach_to_vinfo(struct vinfo_s *info)
{
	if (!info) {
		DPTX_ERROR("[%s]: info is NULL\n", __func__);
		return;
	}
	memset(&info->dv_info, 0, sizeof(info->dv_info));
	memset(&info->hdr_info, 0, sizeof(info->hdr_info));
	memset(&info->rx_latency, 0, sizeof(info->rx_latency));

	info->screen_real_width = 0;
	info->screen_real_height = 0;
}

static int calc_vinfo_from_dp_timing(struct dptx_common *tx_comm,
		const struct tx_timing *timing, struct vinfo_s *tx_vinfo)
{
	if (!tx_comm || !timing || !tx_vinfo) {
		DPTX_ERROR("[%s]: input param is NULL\n", __func__);
		return -EINVAL;
	}
	/* manually assign dptx_vinfo from timing */
	tx_vinfo->name = timing->sname ? timing->sname : timing->name;
	tx_vinfo->mode = VMODE_DPTX;
	tx_vinfo->frac = 0; /* TODO */
	if (timing->pixel_repetition_factor)
		tx_vinfo->width = timing->h_active >> 1;
	else
		tx_vinfo->width = timing->h_active;
	tx_vinfo->height = timing->v_active;
	tx_vinfo->field_height = timing->pi_mode ?
		timing->v_active : timing->v_active / 2;
	tx_vinfo->aspect_ratio_num = timing->h_pict;
	tx_vinfo->aspect_ratio_den = timing->v_pict;
	if (timing->v_freq % 1000 == 0) {
		tx_vinfo->sync_duration_num = timing->v_freq / 1000;
		tx_vinfo->sync_duration_den = 1;
	} else {
		tx_vinfo->sync_duration_num = timing->v_freq;
		tx_vinfo->sync_duration_den = 1000;
	}
	tx_vinfo->brr_duration = 0;
	tx_vinfo->video_clk = timing->pixel_freq;
	tx_vinfo->htotal = timing->h_total;
	tx_vinfo->vtotal = timing->v_total;
	tx_vinfo->viu_color_fmt = COLOR_FMT_YUV444;
	tx_vinfo->viu_mux = timing->pi_mode ? VIU_MUX_ENCP : VIU_MUX_ENCI;
	/* 1080i use the ENCP, not ENCI */
	if (timing->name && strstr(timing->name, "1080i"))
		tx_vinfo->viu_mux = VIU_MUX_ENCP;
	tx_vinfo->viu_mux |= tx_comm->enc_idx << 4;

	return 0;
}

static void update_vinfo_from_format_para(struct dptx_common *tx_comm)
{
	struct vinfo_s *vinfo = NULL;
	struct meson_tx_format_para *fmtpara = NULL;

	if (!tx_comm) {
		DPTX_ERROR("[%s]: input param is NULL\n", __func__);
		return;
	}
	vinfo = &tx_comm->base.tx_vinfo;
	fmtpara = &tx_comm->base.sw_fmt_para;
	/* update vinfo for out device */
	calc_vinfo_from_dp_timing(tx_comm, &fmtpara->timing, vinfo);
	vinfo->vout_device = tx_comm->base.tx_vinfo.vout_device;
	/* dynamic info, always need set */
	vinfo->cs = fmtpara->cs;
	vinfo->cd = fmtpara->cd;
	/* update ppc and color fmt info for vpp, only for FRL/DSC */
	vinfo->cur_enc_ppc = 1;
	DPTX_INFO("vinfo: set cur_enc_ppc as %d, vpp color: %d\n",
		vinfo->cur_enc_ppc, vinfo->vpp_post_out_color_fmt);
}

void dptx_update_vinfo(struct dptx_common *tx_comm)
{
	if (!tx_comm) {
		DPTX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}

	/* vinfo->mode need use, so there need update format para firstly */
	update_vinfo_from_format_para(tx_comm);
	edidinfo_attach_to_vinfo(tx_comm);
}

void dptx_reset_vinfo(struct vinfo_s *tx_vinfo)
{
	tx_vinfo->name = "invalid";
	tx_vinfo->mode = VMODE_MAX;

	edidinfo_detach_to_vinfo(tx_vinfo);
}

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
struct vinfo_s *dptx_get_current_vinfo(void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;

	/* TODO dptx_set_hdr_priority */

	return &tx_dev->tx_vinfo;
}

static enum vmode_e dptx_validate_vmode(char *_mode, unsigned int frac, void *data)
{
	//TODO
	return VMODE_MAX;
}

static int dptx_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DPTX)
		return true;
	else
		return false;
}

static int dptx_module_disable(enum vmode_e cur_vmod, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;
	u8 en = 0;
	struct dptx_common *tx_comm = container_of(tx_dev, struct dptx_common, base);

	/* disable phy */
	meson_tx_phy_power_off(tx_dev->tx_hw_base->tx_phy);
	/* disable core output */
	if (tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable)
		tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
	dptx_hw_cntl(tx_dev->tx_hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
	dptx_hw_cntl(tx_dev->tx_hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
	/* clear vinfo */
	dptx_reset_vinfo(&tx_comm->base.tx_vinfo);
	/* clear hdr packet */
	dptx_drm_infoframe_set(tx_comm->hw_comm, 0, NULL);
	return 0;
}
#endif

void dptx_vout_init(struct dptx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	struct vout_op_s dptx_vout_ops = {
		.get_vinfo	= dptx_get_current_vinfo,
		.set_vmode	= meson_tx_set_current_vmode,
		.validate_vmode = dptx_validate_vmode,
		.check_same_vmodeattr	= NULL,
		.vmode_is_supported = dptx_vmode_is_supported,
		.disable	= dptx_module_disable,
		.set_state	= meson_tx_vout_set_state,
		.clr_state	= meson_tx_vout_clr_state,
		.get_state	= meson_tx_vout_get_state,
		.get_disp_cap	= NULL,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = NULL,
		.vout_suspend = NULL,
		.vout_resume = NULL,
	};

	/* for eDPTX, register vout server by external edptx driver */
	if (tx_comm->is_edp && tx_comm->ext_edptx_drv_en)
		return;
	meson_tx_vout_init(&tx_comm->base, &dptx_vout_ops);
#endif
}

void dptx_vout_uninit(struct dptx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	/* for eDPTX, register vout server by external edptx driver */
	if (tx_comm->is_edp && tx_comm->ext_edptx_drv_en)
		return;
	meson_tx_vout_uninit(&tx_comm->base);
#endif
}
