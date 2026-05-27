// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_edid.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "hdmitx_log.h"
#include "hdmitx_check_valid.h"
#include "hdmitx_vout.h"
#include "meson_tx_internal.h"

const struct tx_timing *meson_tx_mode_match_timing_name(const char *name);

static void hdmi_physical_size_to_vinfo(struct hdmitx_common *tx_comm)
{
	u32 width, height;
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->hdmitx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	if (info->mode == VMODE_HDMI) {
		width = prxcap->physical_width;
		height = prxcap->physical_height;
		if (width == 0 || height == 0) {
			info->screen_real_width = info->aspect_ratio_num;
			info->screen_real_height = info->aspect_ratio_den;
		} else {
			info->screen_real_width = width;
			info->screen_real_height = height;
		}
		HDMITX_DEBUG("update physical size: %d %d\n",
			info->screen_real_width, info->screen_real_height);
	}
}

static void rxlatency_to_vinfo(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->hdmitx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	info->rx_latency.vLatency = prxcap->vLatency;
	info->rx_latency.aLatency = prxcap->aLatency;
	info->rx_latency.i_vLatency = prxcap->i_vLatency;
	info->rx_latency.i_aLatency = prxcap->i_aLatency;
}

static void edidinfo_attach_to_vinfo(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("[%s]: tx_comm is NULL\n", __func__);
		return;
	}
	info = &tx_comm->hdmitx_vinfo;
	prxcap = &tx_comm->base.rxcap;
	rxlatency_to_vinfo(tx_comm);
	hdmi_physical_size_to_vinfo(tx_comm);
	memcpy(info->hdmichecksum, prxcap->hdmichecksum, 10);
}

static void edidinfo_detach_to_vinfo(struct vinfo_s *info)
{
	if (!info) {
		HDMITX_ERROR("[%s]: info is NULL\n", __func__);
		return;
	}
	memset(&info->dv_info, 0, sizeof(info->dv_info));
	memset(&info->hdr_info, 0, sizeof(info->hdr_info));
	memset(&info->rx_latency, 0, sizeof(info->rx_latency));

	info->screen_real_width = 0;
	info->screen_real_height = 0;
	memset(info->hdmichecksum, 0, sizeof(info->hdmichecksum));
}

static int calc_vinfo_from_hdmi_timing(struct hdmitx_common *tx_comm,
		const struct tx_timing *timing, struct vinfo_s *tx_vinfo)
{
	if (!tx_comm || !timing || !tx_vinfo) {
		HDMITX_ERROR("[%s]: input param is NULL\n", __func__);
		return -EINVAL;
	}
	/* manually assign hdmitx_vinfo from timing */
	tx_vinfo->name = timing->sname ? timing->sname : timing->name;
	tx_vinfo->mode = VMODE_HDMI;
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
	tx_vinfo->fr_adj_type = VOUT_FR_ADJ_HDMI;
	tx_vinfo->viu_color_fmt = COLOR_FMT_YUV444;
	tx_vinfo->viu_mux = timing->pi_mode ? VIU_MUX_ENCP : VIU_MUX_ENCI;
	/* 1080i use the ENCP, not ENCI */
	if (timing->name && strstr(timing->name, "1080i"))
		tx_vinfo->viu_mux = VIU_MUX_ENCP;
	tx_vinfo->viu_mux |= tx_comm->enc_idx << 4;

	return 0;
}

static void update_vinfo_from_format_para(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *vinfo = NULL;
	struct meson_tx_format_para *fmtpara = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("[%s]: input param is NULL\n", __func__);
		return;
	}
	vinfo = &tx_comm->hdmitx_vinfo;
	fmtpara = &tx_comm->fmt_para;
	/* update vinfo for out device */
	calc_vinfo_from_hdmi_timing(tx_comm, &fmtpara->timing, vinfo);
	vinfo->vout_device = tx_comm->vdev;
	/*
	 * vinfo->info_3d = NON_3D;
	 * if (fmtpara->flag_3dfp)
	 *	 vinfo->info_3d = FP_3D;
	 * if (fmtpara->flag_3dtb)
	 *	 vinfo->info_3d = TB_3D;
	 * if (fmtpara->flag_3dss)
	 *	 vinfo->info_3d = SS_3D;
	 */
	/* dynamic info, always need set */
	vinfo->cs = fmtpara->cs;
	vinfo->cd = fmtpara->cd;
	/* update ppc and color fmt info for vpp, only for FRL/DSC */
	if (fmtpara->tx_hw_para.hdmitx_hw_para.frl_rate > FRL_NONE)
		vinfo->cur_enc_ppc = 4;
	else
		vinfo->cur_enc_ppc = 1;
#ifdef CONFIG_AMLOGIC_DSC
	if (fmtpara->tx_hw_para.hdmitx_hw_para.dsc_en) {
		if (tx_comm->fmt_para.cs == HDMI_COLORSPACE_RGB)
			vinfo->vpp_post_out_color_fmt = 1;
		else
			vinfo->vpp_post_out_color_fmt = 0;
	} else {
		vinfo->vpp_post_out_color_fmt = 0;
	}
#endif
	HDMITX_INFO("vinfo: set cur_enc_ppc as %d, vpp color: %d\n",
		vinfo->cur_enc_ppc, vinfo->vpp_post_out_color_fmt);
}

void hdmitx_update_vinfo(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}

	/* vinfo->mode need use, so there need update format para firstly */
	update_vinfo_from_format_para(tx_comm);
	edidinfo_attach_to_vinfo(tx_comm);
}

void hdmitx_reset_vinfo(struct vinfo_s *tx_vinfo)
{
	tx_vinfo->name = "invalid";
	tx_vinfo->mode = VMODE_MAX;

	edidinfo_detach_to_vinfo(tx_vinfo);
}

/*
 * only bootup and update vinfo;
 * mode setting will not update here, will update vinfo in post enable mode setting
 */
void hdmitx_update_vinfo_duration(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *vinfo = &tx_comm->hdmitx_vinfo;
	const struct tx_timing *timing = 0;
	struct tx_timing t;

	if (!tx_comm || !tx_comm->tx_hw)
		return;

	timing = meson_tx_mode_vic_to_timing(tx_comm->hw_fmt_para.vic);
	if (!timing) {
		HDMITX_ERROR("%s Invalid VIC %d\n", __func__, tx_comm->hw_fmt_para.vic);
		return;
	}

	t = *timing;
	if (tx_comm->fmt_para.frac_mode)
		meson_tx_mode_update_timing(&t, true);

	if (t.v_freq % 1000 == 0) {
		vinfo->sync_duration_num = t.v_freq / 1000;
		vinfo->sync_duration_den = 1;
	} else {
		vinfo->sync_duration_num = t.v_freq;
		vinfo->sync_duration_den = 1000;
	}
}

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;
	struct hdmitx_common *tx_comm = tx_dev_to_hdmitx_common(tx_dev);

	if (!data) {
		HDMITX_ERROR("%s tx_comm instance is null\n", __func__);
		return NULL;
	}

	tx_comm->hdmitx_vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_HDMIA_A
		+ tx_comm->enc_idx;
	/* update hdr_info and dv_info */
	hdmitx_set_hdr_priority(tx_comm, tx_comm->hdr_priority,
			&tx_comm->hdmitx_vinfo.hdr_info,
			&tx_comm->hdmitx_vinfo.dv_info);

	return &tx_comm->hdmitx_vinfo;
}

static enum vmode_e hdmitx_validate_vmode(char *_mode, unsigned int frac, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;
	struct hdmitx_common *tx_comm = tx_dev_to_hdmitx_common(tx_dev);
	struct vinfo_s *vinfo = NULL;
	const struct tx_timing *timing = 0;
	struct tx_timing t;
	char conv_name[32] = {0};
	char *mode = _mode;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return VMODE_MAX;
	}
	vinfo = &tx_comm->hdmitx_vinfo;
	/* vout validate vmode only used to confirm the mode is
	 * supported by this server. And don't check with edid,
	 * maybe we don't have edid when this function called.
	 */
	if (hdmitx_is_mode_name_frac(mode)) {
		hdmitx_convert_name_frac2int(mode, conv_name);
		mode = conv_name;
	}
	timing = meson_tx_mode_match_timing_name(mode);
	t = *timing;
	if (frac)
		meson_tx_mode_update_timing(&t, frac);
	if (hdmitx_common_validate_vic(tx_comm, timing->vic) == 0) {
		/* probe will check valid mode, there needn't update vinfo */
		calc_vinfo_from_hdmi_timing(tx_comm, &t, vinfo);
		vinfo->vout_device = tx_comm->vdev;
		return VMODE_HDMI;
	}

	HDMITX_ERROR("%s validate %s fail\n", __func__, _mode);
	return VMODE_MAX;
}

static unsigned int hdmitx_get_viu_mux(char *_mode, void *data)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;
	const struct tx_timing *timing = 0;
	char conv_name[32] = {0};
	char *mode = _mode;
	unsigned int viu_mux;

	/* vout validate vmode only used to confirm the mode is
	 * supported by this server. And don't check with edid,
	 * maybe we don't have edid when this function called.
	 */
	if (hdmitx_is_mode_name_frac(mode)) {
		hdmitx_convert_name_frac2int(mode, conv_name);
		mode = conv_name;
	}
	timing = meson_tx_mode_match_timing_name(mode);

	if (hdmitx_common_validate_vic(tx_comm, timing->vic) == 0) {
		viu_mux = timing->pi_mode ? VIU_MUX_ENCP : VIU_MUX_ENCI;
		/* 1080i use the ENCP, not ENCI */
		if (timing->name && strstr(timing->name, "1080i"))
			viu_mux = VIU_MUX_ENCP;
		viu_mux |= tx_comm->enc_idx << 4;
		return viu_mux;
	}

	HDMITX_ERROR("%s validate %s fail\n", __func__, _mode);
	return 0;
}

static int hdmitx_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI)
		return true;
	else
		return false;
}

static int hdmitx_module_disable(enum vmode_e cur_vmod, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;
	struct hdmitx_common *tx_comm = tx_dev_to_hdmitx_common(tx_dev);

	hdmitx_common_disable_mode(tx_comm, NULL);
	return 0;
}

#else
static struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	return NULL;
}
#endif

void hdmitx_vout_init(struct hdmitx_common *tx_comm, struct hdmitx_hw_common *tx_hw)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	struct vout_op_s hdmitx_vout_ops = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = meson_tx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = NULL,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = meson_tx_vout_set_state,
		.clr_state = meson_tx_vout_clr_state,
		.get_state = meson_tx_vout_get_state,
		.get_disp_cap = NULL,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = NULL,
		.get_viu_mux = hdmitx_get_viu_mux,
		.vout_suspend = NULL,
		.vout_resume = NULL,
	};

	meson_tx_vout_init(&tx_comm->base, &hdmitx_vout_ops);
#endif
}

void hdmitx_vout_uninit(struct hdmitx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	meson_tx_vout_uninit(&tx_comm->base);
#endif
}
