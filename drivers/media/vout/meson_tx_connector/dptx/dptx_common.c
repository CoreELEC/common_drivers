// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/delay.h>

#include "dptx_infoframe.h"
#include "meson_tx_internal.h"
#include "meson_tx_event_mgr.h"
#include "dptx_log.h"
#include "dptx_internal.h"
#include "dptx_aux_helper.h"
#include "dptx_link.h"
#include "dpcd_reg.h"
#include "meson_tx_task_mgr.h"
#include "dptx_timer.h"
#include "dptx_gtc.h"
#include "dptx_hw_opcode.h"
#include "dptx_hdr.h"

static struct vout_device_s dptx_vdev = {
	.enable_tx_packet = dptx_enable_tx_packet,
	.disable_tx_packet = dptx_disable_tx_packet,
};

/* parse DPCD cap per DP1.4 spec chapter2.9.32 */
static void dptx_cap_parse(u8 *dpcd_buf, struct dpcd_cap *tx_dpcd_cap)
{
	if (!dpcd_buf || !tx_dpcd_cap) {
		DPTX_INFO("dptx cap parse: buf NULL\n");
		return;
	}

	tx_dpcd_cap->dpcd_ver = (dpcd_buf[0] >> 0x4);
	tx_dpcd_cap->max_link_rate = dpcd_buf[1];
	tx_dpcd_cap->max_lane_count = dpcd_buf[2] & 0x1F;
}
/*
 * Per dp1.4a spec chapter 5.1.4:
 * The Source device shall respond to Hot Plug event/Hot Re-plug
 * event by first reading DPCD Link/Sink Device Status registers
 * at DPCD Addresses 00200h through 00205h or DPCD Addresses
 * 02002h through 0200Fh. If the link is unstable or lost, the
 * Source device then reads the DPCD Receiver Capabilities registers
 * at DPCD Addresses 00000h through 0000Fh to determine
 * the appropriate information needed to train the link.
 * The Source device shall then initiate Link Training.
 */
int dptx_hpd_plugin(struct meson_tx_dev *tx_dev)
{
	struct dptx_common *tx_comm = to_dptx_common(tx_dev);
	int ret = 0;
	int i;

	DPTX_INFO("%s %px\n", __func__, tx_dev);

	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_SINK_COUNT,
			tx_comm->link_sink_status,
			sizeof(tx_comm->link_sink_status));
	if (ret < 0)
		DPTX_ERROR("read dpcd link/sink status failed:%d\n", ret);

	ret = dptx_aux_read_dpcd_caps(tx_comm->tx_aux,
		tx_dev->dpcd_buf, sizeof(tx_dev->dpcd_buf));
	if (ret < 0)
		DPTX_INFO("read dpcd cap failed\n");
	else
		dptx_cap_parse(tx_dev->dpcd_buf, &tx_dev->dpcd_cap);
	for (i = 0; i < EDID_READ_MAX_RETRY; i++) {
		ret = dptx_aux_read_edid_data(tx_comm->tx_aux, tx_dev->edid_buf,
			sizeof(tx_dev->edid_buf));
		if (ret < 0)
			DPTX_INFO("read edid failed\n");
		else
			break;
	}
	return 0;
}

int dptx_hpd_plugout(struct meson_tx_dev *tx_dev)
{
	u8 en = 0;
	struct dptx_common *tx_comm = container_of(tx_dev, struct dptx_common, base);

	DPTX_INFO("%s %px\n", __func__, tx_dev);
	/* DP V1.4a chapter5.1.4, disable the Main-Link as a power-saving measure */
	meson_tx_phy_power_off(tx_dev->tx_hw_base->tx_phy);
	/* disable core output */
	if (tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable)
		tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
	dptx_hw_cntl(tx_dev->tx_hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
	/*
	 * disable the input/output of vc_id channel
	 * TODO: for MST, need disable all channel
	 */
	en = (0x0 << 0) | (tx_dev->sw_fmt_para.vc_id << 4);
	dptx_hw_cntl(tx_dev->tx_hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
	/* clear hdr packet */
	//dptx_drm_infoframe_set(tx_comm->hw_comm, 0, NULL);
	tx_comm->ready = 0;
	return 0;
}

/* Remove duplicate timing parameters and update the timing count */
static void dptx_remove_duplicates_timing(struct tx_timing *timing, int *count)
{
	int new_count = 0;
	int i;
	int duplicate = 0;

	if (!timing || !count || *count <= 1)
		return;

	for (i = 0; i < *count; i++) {
		duplicate = 0;
		for (int j = 0; j < new_count; j++) {
			if (meson_tx_mode_compare(&timing[i], &timing[j])) {
				duplicate = 1;
				break;
			}
		}
		if (!duplicate) {
			if (new_count != i)
				timing[new_count] = timing[i];
			new_count++;
		}
	}
	*count = new_count;
}

/*
 * Note: timing list memory need to be freed after DRM get mode list
 */
int dptx_get_mode_list(struct dptx_common *tx_comm, struct tx_timing **timing_list)
{
	struct rx_cap *prxcap = NULL;
	struct display_id_cap *disp_id_cap = NULL;
	int *edid_vics = NULL;
	const struct tx_timing *edid_timing;
	struct tx_timing *display_id_timing = NULL;
	u32 i = 0;
	int count = 0;
	enum hdmi_vic vic;
	int len = 0;
	struct tx_timing *dptx_timings = NULL;

	if (!tx_comm || !timing_list) {
		DPTX_ERROR("%s: invalid param\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&tx_comm->base.valid_mode_mutex);
	disp_id_cap = &tx_comm->base.rxcap.disp_id_cap;
	prxcap = &tx_comm->base.rxcap;

	if (prxcap->VIC_count + prxcap->vesa_timing_count > 0) {
		edid_vics = vzalloc((prxcap->VIC_count + prxcap->vesa_timing_count) * sizeof(u32));
		if (!edid_vics) {
			DPTX_ERROR("edid_vics alloc fail\n");
			goto out;
		}
	}

	len = disp_id_cap->timing_count + prxcap->VIC_count + prxcap->vesa_timing_count;
	if (len > 0) {
		dptx_timings = vzalloc(len * sizeof(struct tx_timing));
		if (!dptx_timings) {
			DPTX_ERROR("dptx_timing alloc fail\n");
			goto out;
		}
	}

	/* step1: only select VIC which is supported in EDID */
	/* copy edid vic list */
	if (prxcap->VIC_count > 0)
		memcpy(edid_vics, prxcap->VIC, sizeof(u32) * prxcap->VIC_count);
	if (prxcap->vesa_timing_count > 0)
		memcpy(&edid_vics[prxcap->VIC_count], prxcap->vesa_timing,
			sizeof(u32) * prxcap->vesa_timing_count);

	for (i = 0; i < prxcap->VIC_count + prxcap->vesa_timing_count; i++) {
		vic = edid_vics[i];
		if (vic == HDMI_0_UNKNOWN)
			continue;
		edid_timing = meson_tx_mode_vic_to_timing(vic);
		if (!edid_timing) {
			DPTX_INFO("%s: vic[%d] not found in edid timing list.\n", __func__, vic);
			continue;
		}
		/* always treat timing supported under pxp */
		if (!tx_comm->base.pxp_mode &&
				!dptx_validate_timing_with_basic_cs(&tx_comm->base,
					(struct tx_timing *)edid_timing))
			continue;
		memcpy(&dptx_timings[count++], edid_timing, sizeof(*edid_timing));
	}

	/* step2: filter supported timings in Display_ID */
	for (i = 0; i < disp_id_cap->timing_count; i++) {
		display_id_timing = &disp_id_cap->timing[i / BLK_TIMING_CNT][i % BLK_TIMING_CNT];
		/* always treat timing supported under pxp */
		if (!tx_comm->base.pxp_mode &&
			!dptx_validate_timing_with_basic_cs(&tx_comm->base, display_id_timing))
			continue;
		memcpy(&dptx_timings[count++], display_id_timing, sizeof(*display_id_timing));
	}
	/* remove duplicate timings */
	dptx_remove_duplicates_timing(dptx_timings, &count);

out:
	vfree(edid_vics);
	if (count == 0) {
		vfree(dptx_timings);
		*timing_list = NULL;
	} else {
		*timing_list = dptx_timings;
	}
	mutex_unlock(&tx_comm->base.valid_mode_mutex);
	return count;
}
EXPORT_SYMBOL(dptx_get_mode_list);

/* map from enum type to actual bits per color component */
u8 dptx_get_mapped_bpc(color_depth_t cd)
{
	u8 bit_depth = 0;

	switch (cd) {
	case COLORDEPTH_18B:
		bit_depth = 6;
		break;
	case COLORDEPTH_24B:
		bit_depth = 8;
		break;
	case COLORDEPTH_30B:
		bit_depth = 10;
		break;
	case COLORDEPTH_36B:
		bit_depth = 12;
		break;
	case COLORDEPTH_42B:
		bit_depth = 14;
		break;
	case COLORDEPTH_48B:
		bit_depth = 16;
		break;
	default:
		DPTX_ERROR("%s cd[%d] not find mapped bit_depth\n", __func__, cd);
		bit_depth = 8;
		break;
	}
	return bit_depth;
}

/*
 * Function: dptx_calc_bpp
 * calculate the bits per pixel from color format and depth
 */
u32 dptx_calc_bpp(enum colorimetry_format color, u8 colordepth)
{
	u8 base_bpp[COLORIMETRY_FORMAT_MAX] = {
		[COLORIMETRY_FORMAT_RGB] = 24,
		[COLORIMETRY_FORMAT_YUV444] = 24,
		[COLORIMETRY_FORMAT_YUV422] = 16,
		[COLORIMETRY_FORMAT_YUV420] = 12,
		[COLORIMETRY_FORMAT_Y_ONLY] = 8,
		[COLORIMETRY_FORMAT_RAW] = 8,
	};

	if (color >= COLORIMETRY_FORMAT_MAX) {
		DPTX_ERROR("[%s]: invalid input color format(%d)\n", __func__, color);
		return base_bpp[COLORIMETRY_FORMAT_RGB] * colordepth / 8;
	}
	return base_bpp[color] * colordepth / 8;
}

/* color component(R/G/B, Y/Cb/Cr) count per pixel */
u8 dptx_get_color_component_cnt(enum colorimetry_format color)
{
	u8 color_component[COLORIMETRY_FORMAT_MAX] = {
		[COLORIMETRY_FORMAT_RGB] = 3,
		[COLORIMETRY_FORMAT_YUV444] = 3,
		[COLORIMETRY_FORMAT_YUV422] = 2,
		[COLORIMETRY_FORMAT_YUV420] = 3,
		[COLORIMETRY_FORMAT_Y_ONLY] = 1,
		[COLORIMETRY_FORMAT_RAW] = 1,
	};

	if (color >= COLORIMETRY_FORMAT_MAX) {
		DPTX_ERROR("[%s]: invalid input color format(%d)\n", __func__, color);
		return color_component[COLORIMETRY_FORMAT_RGB];
	}
	return color_component[color];
}

bool dptx_common_get_hdcp_user_state(struct dptx_common *tx_comm)
{
	if (!tx_comm) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}
	return tx_comm->hdcptx_comm.hdcp_user;
}
EXPORT_SYMBOL(dptx_common_get_hdcp_user_state);

int dptx_get_hdrinfo_rx(struct dptx_common *tx_comm, struct hdr_info *hdrinfo)
{
	struct rx_cap *prxcap = &tx_comm->base.rxcap;

	memcpy(hdrinfo, &prxcap->hdr_info, sizeof(struct hdr_info));
	hdrinfo->colorimetry_support = prxcap->colorimetry_data;

	return 0;
}

const struct hdr_info *dptx_common_get_hdr_info_rx(struct dptx_common *tx_comm)
{
	static struct hdr_info hdrinfo;

	if (!tx_comm) {
		DPTX_ERROR("%s: invalid param\n", __func__);
		return NULL;
	}
	dptx_get_hdrinfo_rx(tx_comm, &hdrinfo);
	return &hdrinfo;
}
EXPORT_SYMBOL(dptx_common_get_hdr_info_rx);

bool dptx_common_get_edid_valid_state(struct dptx_common *tx_comm)
{
	if (!tx_comm) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}
	return tx_comm->base.rxcap.edid_parsing;
}
EXPORT_SYMBOL(dptx_common_get_edid_valid_state);

bool dptx_common_edid_check_valid(struct dptx_common *tx_comm)
{
	if (!tx_comm) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}

	tx_comm->base.rxcap.edid_parsing = meson_tx_edid_check_valid(&tx_comm->base.rxcap,
		tx_comm->base.edid_buf);
	return tx_comm->base.rxcap.edid_parsing;
}
EXPORT_SYMBOL(dptx_common_edid_check_valid);

int dptx_common_get_hdr_status(struct dptx_common *tx_comm)
{
	/* todo:hdr module development */
	return 6;
}
EXPORT_SYMBOL(dptx_common_get_hdr_status);

unsigned int dptx_common_get_content_types(struct dptx_common *tx_comm)
{
	unsigned int types = 1 << DRM_MODE_CONTENT_TYPE_NO_DATA;/*NONE DATA*/
	struct rx_cap *prxcap = &tx_comm->base.rxcap;

	if (!tx_comm) {
		DPTX_ERROR("%s: invalid param\n", __func__);
		return -EINVAL;
	}
	if (prxcap->cnc0)
		types |= 1 << DRM_MODE_CONTENT_TYPE_GRAPHICS;
	if (prxcap->cnc1)
		types |= 1 << DRM_MODE_CONTENT_TYPE_PHOTO;
	if (prxcap->cnc2)
		types |= 1 << DRM_MODE_CONTENT_TYPE_CINEMA;
	if (prxcap->cnc3)
		types |= 1 << DRM_MODE_CONTENT_TYPE_GAME;

	return types;
}
EXPORT_SYMBOL(dptx_common_get_content_types);

bool dptx_common_get_ready_state(struct dptx_common *tx_comm)
{
	if (!tx_comm) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}
	return tx_comm->ready;
}
EXPORT_SYMBOL(dptx_common_get_ready_state);

/* index is hdmi 1.4 vic in vsif, value is hdmi2.0 vic */
static const u32 dptx14_4k_vics[] = {
	/* 0 - dummy */
	0,
	/* 1 - 3840x2160@30Hz */
	95,
	/* 2 - 3840x2160@25Hz */
	94,
	/* 3 - 3840x2160@24Hz */
	93,
	/* 4 - 4096x2160@24Hz (SMPTE) */
	98,
};

/*
 * HDMI 1.4 introduces the 4 types of 4K resolutions
 * 3840x2160@30/25/24Hz and 4096x2160@24Hz
 * but no corresponding CEA-861-D VIC.
 * so it uses VSIF.hdmi_vic as 1/2/3/4 to represent 4 types
 * In CEA-861-F, here assigns the VIC 95/94/93/98 later
 * this function converts 861-F 4k vic to VSIF.hdmi_vic
 */
u32 dptx_edid_get_hdmi14_4k_vic(u32 vic)
{
	u32 ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(dptx14_4k_vics); i++) {
		if (vic == dptx14_4k_vics[i]) {
			ret = i;
			break;
		}
	}

	return ret;
}

int dptx_common_set_contenttype(struct dptx_common *tx_comm, int content_type)
{
	/* todo */
	return 0;
}
EXPORT_SYMBOL(dptx_common_set_contenttype);

u32 dptx_common_get_vrr_cap(struct dptx_common *tx_comm)
{
	struct rx_cap *prxcap = NULL;
	u32 vrr_cap = 0;

	if (!tx_comm) {
		DPTX_ERROR("%s: invalid param\n", __func__);
		return 0;
	}
	prxcap = &tx_comm->base.rxcap;

	if (prxcap->qms || prxcap->vrr_max || prxcap->vrr_min) {
		vrr_cap |= prxcap->qms ? QMS_VRR_SUP : 0;
		vrr_cap |= prxcap->vrr_min ? GAMING_VRR_SUP : 0;
		DPTX_DEBUG("%s get vrr cap : %d\n", __func__, vrr_cap);
		return vrr_cap;
	}

	return 0;
}
EXPORT_SYMBOL(dptx_common_get_vrr_cap);

int dptx_common_avmute_locked(struct dptx_common *tx_comm,
	int mute_flag, int mute_path_hint)
{
	static DEFINE_MUTEX(avmute_mutex);
	static unsigned int global_avmute_mask;
	unsigned int mute_us = 0;
	unsigned int avmute_frame = 0;
	u32 arg = 0;
	struct meson_tx_dev *tx_dev = &tx_comm->base;

	if (!tx_comm || !tx_comm->hw_comm) {
		DPTX_ERROR("%s: invalid param\n", __func__);
		return -EINVAL;
	}
	avmute_frame = tx_comm->debug_param.avmute_frame;
	mute_us =
		avmute_frame * meson_tx_mode_get_frame_duration(&tx_dev->sw_fmt_para.timing);
	mutex_lock(&avmute_mutex);
	if (mute_flag == SET_AVMUTE) {
		global_avmute_mask |= mute_path_hint;
		DPTX_DEBUG("%s: AVMUTE path=0x%x\n", __func__, mute_path_hint);
		arg = SET_AVMUTE;
		dptx_hw_cntl(tx_dev->tx_hw_base, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
	} else if (mute_flag == CLR_AVMUTE) {
		global_avmute_mask &= ~mute_path_hint;
		/* unmute only if none of the paths are muted */
		if (global_avmute_mask == 0) {
			DPTX_DEBUG("%s: AV UNMUTE path=0x%x\n", __func__, mute_path_hint);
			arg = CLR_AVMUTE;
			dptx_hw_cntl(tx_dev->tx_hw_base, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
		}
	} else if (mute_flag == OFF_AVMUTE) {
		arg = OFF_AVMUTE;
		dptx_hw_cntl(tx_dev->tx_hw_base, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
	}
	if (mute_flag == SET_AVMUTE) {
		if (tx_comm->debug_param.avmute_frame > 0)
			msleep(mute_us / 1000);
		else if (mute_path_hint == AVMUTE_PATH_DPTX)
			msleep(100);
	}

	mutex_unlock(&avmute_mutex);

	return 0;
}
EXPORT_SYMBOL(dptx_common_avmute_locked);

int dptx_get_hdr_priority(struct dptx_common *tx_comm, u32 *hdr_priority)
{
	if (!tx_comm || !hdr_priority) {
		DPTX_ERROR("%s fail\n", __func__);
		return -EINVAL;
	}
	*hdr_priority = tx_comm->hdr_priority;
	return 0;
}
EXPORT_SYMBOL(dptx_get_hdr_priority);

/* return pixel clk(bits per second) with specific cs/cd */
static u32 calc_video_fmt_bw(u32 pixel_clk, enum colorimetry_format cs, color_depth_t cd)
{
	return pixel_clk * dptx_calc_bpp(cs, dptx_get_mapped_bpc(cd));
}

/* calculate HW format para from sw para, such as tmds_clk, lane_count/link_rate */
int dptx_calc_hw_fmt_para(struct meson_tx_dev *tx_base, struct meson_tx_format_para *sw_para,
	struct dptx_hw_fmt_para *hw_para)
{
	u32 bandwidth;
	struct tx_timing *t;
	enum dp_link_rate_e rate = DPTX_LINK_RATE_1P62GHZ;
	enum dp_lane_count_e count = DPTX_LANE_COUNT_1;
	struct dptx_common *tx_comm = to_dptx_common(tx_base);

	if (!tx_comm || !sw_para || !hw_para) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return -EINVAL;
	}

	t = &sw_para->timing;
	/* calculate the current resolution bandwidth with colorspace and depth */
	bandwidth = calc_video_fmt_bw(t->pixel_freq, (enum colorimetry_format)sw_para->cs,
		sw_para->cd);
	/* if use 128b132b coding will cost 0.30%, bandwidth = bandwidth * 33 / 32; */
	/* 8b10b coding will cost 20% */
	bandwidth *= 10;
	bandwidth /= 8;

	/* downspread overhead 0.5% */
	bandwidth += bandwidth / 199;
	/* FEC overhead 0.5% */
	bandwidth += bandwidth * 3 / 256;

	hw_para->total_bandwidth = bandwidth;
	/* choose low rate firstly, then count */
	/*
	if (bandwidth <= 1620000) {
		rate = DPTX_LINK_RATE_1P62GHZ;
		count = DPTX_LANE_COUNT_1;
	} else if (bandwidth <= 1620000 * 2) {
		rate = DPTX_LINK_RATE_1P62GHZ;
		count = DPTX_LANE_COUNT_2;
	} else if (bandwidth <= 1620000 * 4) {
		rate = DPTX_LINK_RATE_1P62GHZ;
		count = DPTX_LANE_COUNT_4;
	} else if (bandwidth <= 2700000 * 4) {
		rate = DPTX_LINK_RATE_2P70GHZ;
		count = DPTX_LANE_COUNT_4;
	} else if (bandwidth <= 5400000 * 4) {
		rate = DPTX_LINK_RATE_5P40GHZ;
		count = DPTX_LANE_COUNT_4;
	} else if (bandwidth <= 8100000 * 4) {
		rate = DPTX_LINK_RATE_8P10GHZ;
		count = DPTX_LANE_COUNT_4;
	} else {
		DPTX_ERROR("Too high bandwidth %d %s cs %d cd %d\n", bandwidth, sw_para->name,
			sw_para->cs, sw_para->cd);
		return -1;
	}
	*/

	/* remove later, currently choose 1 lane firstly */
	if (bandwidth <= 1620000) {
		rate = DPTX_LINK_RATE_1P62GHZ;
		count = DPTX_LANE_COUNT_1;
	} else if (bandwidth <= 2700000) {
		rate = DPTX_LINK_RATE_2P70GHZ;
		count = DPTX_LANE_COUNT_1;
	} else if (bandwidth <= 5400000) {
		rate = DPTX_LINK_RATE_5P40GHZ;
		count = DPTX_LANE_COUNT_1;
	} else {
		DPTX_ERROR("Only 1 lane, high bandwidth %d %s cs %d cd %d\n", bandwidth,
			   sw_para->name, sw_para->cs, sw_para->cd);
		return -1;
	}

	DPTX_INFO("bandwidth: %s cs %d cd %d bandwidth %dkHz Rate %d Count %d\n",
		sw_para->name, sw_para->cs, sw_para->cd, bandwidth, rate, count);

	/* use force link rate and lane count if it's enabled */
	if (tx_comm->link_train->force_lr >= DPTX_LINK_RATE_1P62GHZ &&
		tx_comm->link_train->force_lr < DPTX_LINK_RATE_MAX)
		hw_para->link_rate = tx_comm->link_train->force_lr;
	else
		hw_para->link_rate = rate;
	if (tx_comm->link_train->force_lc >= DPTX_LANE_COUNT_1 &&
		tx_comm->link_train->force_lc < DPTX_LANE_COUNT_MAX)
		hw_para->lane_count = tx_comm->link_train->force_lc;
	else
		hw_para->lane_count = count;

	DPTX_INFO("finial use bandwidth %dkHz Rate %d Count %d\n",
		bandwidth, hw_para->link_rate, hw_para->lane_count);

	return 0;
}

static int dptx_pre_mode_enable(struct meson_tx_dev *tx_dev, struct meson_tx_format_para *para)
{
	return dptx_hw_cntl(tx_dev->tx_hw_base, MODE_FLOW_PRE_ENABLE_MODE, para, NULL);
}

static int dptx_hw_fmt_para_to_phy_conf(struct dptx_hw_fmt_para *hw_para,
					struct meson_tx_phy_cfg_opts *phy_conf)
{
	if (!hw_para || !phy_conf)
		return -1;
	/* note: in the dp_ops, here need to multiple 10 */
	phy_conf->dp_ops.link_rate = hw_para->link_rate * 270;
	phy_conf->dp_ops.lanes = hw_para->lane_count;
	return 0;
}

static int dptx_mode_enable(struct meson_tx_dev *tx_dev, struct meson_tx_format_para *para)
{
	struct dptx_common *tx_comm = NULL;
	struct meson_tx_clk *tx_clk = NULL;
	int ret = -EINVAL;
	u8 i = 0;
	struct meson_tx_hw *tx_hw_base;
	struct meson_tx_phy_cfg_opts *ops;
	u32 ssc_en = 0;

	if (!tx_dev || !para)
		return ret;
	/* skip core & phy setting */
	if (tx_dev->skip_phy_setting)
		return ret;

	tx_comm = to_dptx_common(tx_dev);
	tx_clk = tx_dev->tx_hw_base->tx_clk;
	tx_hw_base = tx_dev->tx_hw_base;

	dptx_hw_fmt_para_to_phy_conf(&tx_dev->sw_fmt_para.tx_hw_para.dptx_hw_para,
				     &tx_hw_base->tx_phy_conf);

	/* check the parameters of tx_hw_base->tx_phy_conf is valid or not */
	ret = meson_tx_phy_validate(tx_hw_base->tx_phy, PHY_DP, &tx_hw_base->tx_phy_conf);
	if (ret < 0) {
		DPTX_ERROR("Need check the phy parameters\n");
		dump_stack();
		return -EINVAL;
	}
	ops = &tx_hw_base->tx_phy_conf;
	ssc_en = !!(tx_dev->dpcd_buf[DP_MAX_DOWNSPREAD] & DP_SUP_MAX_DOWNSPREAD);
	if (tx_hw_base->tx_phy_conf.force_ssc)
		ssc_en = ops->dp_ops.ssc;
	ops->dp_ops.ssc = ssc_en;
	pr_info("dptx %s[%d] lr %d lc %d ssc %d\n", __func__, __LINE__,
		ops->dp_ops.link_rate, ops->dp_ops.lanes, ssc_en);
	meson_tx_phy_set_mode(tx_hw_base->tx_phy, PHY_DP, &tx_hw_base->tx_phy_conf);

	for (i = 0; i <= tx_comm->link_train->lt_retry_cnt; i++) {
		ret = dptx_link_training(tx_comm->link_train);
		if (ret == 0)
			break;
	}
	if (ret != 0) {
		/* ignore link training fail under pxp mode */
		if (!tx_comm->base.pxp_mode) {
			DPTX_ERROR("%s link training failed, abort mode set\n", __func__);
			return ret;
		}
	} else {
		/* update the link rate/lane count as they may be
		 * changed after link training
		 */
		dptx_update_link_fmt_para(tx_comm, para);
		DPTX_INFO("%s update link rate:0x%x, lane count:0x%x\n",
			__func__, para->tx_hw_para.dptx_hw_para.link_rate,
			para->tx_hw_para.dptx_hw_para.lane_count);
	}

	return dptx_hw_cntl(tx_dev->tx_hw_base, MODE_FLOW_ENABLE_MODE, para, NULL);
}

static int dptx_post_mode_enable(struct meson_tx_dev *tx_dev, struct meson_tx_format_para *para)
{
	int ret = -EINVAL;
	struct dptx_common *tx_comm = NULL;

	if (!tx_dev || !para)
		return ret;

	dptx_hw_cntl(tx_dev->tx_hw_base, MODE_FLOW_POST_ENABLE_MODE, para, NULL);

	tx_comm = to_dptx_common(tx_dev);
	/*
	 * attach vinfo, if hdr_cap and dv_cap change, the HDR/DV module will
	 * call the packet sending function, need to set ready flag to 1 first
	 */
	tx_comm->ready = 1;
	dptx_update_vinfo(tx_comm);

	return 0;
}

unsigned int dptx_common_get_hdcp_mode(struct dptx_common *tx_comm)
{
	unsigned int hdcp_mode_flag = 0;

	if (tx_comm->hdcptx_comm.hdcp_mode) {
		hdcp_mode_flag |= tx_comm->hdcptx_comm.hdcp_mode;
		if (tx_comm->hdcptx_comm.hdcp_auth_result == HDCP_AUTH_OK)
			hdcp_mode_flag |= 1 << 3;
		else if (tx_comm->hdcptx_comm.hdcp_auth_result == HDCP_AUTH_FAIL)
			hdcp_mode_flag |= 0 << 3;
	} else {
		return 0;
	}
	return hdcp_mode_flag;
}
EXPORT_SYMBOL(dptx_common_get_hdcp_mode);

unsigned int dptx_common_set_hdcp_mode(struct dptx_common *tx_comm, u8 hdcp_mode,
			enum dp_hdcp_state hdcp_state)
{
	struct meson_tx_hw *tx_hw = NULL;
	u8 en;

	DPTX_INFO("set hdcp_mode: %d, hdcp_state:%d\n", hdcp_mode, hdcp_state);
	if (!tx_comm) {
		DPTX_ERROR("NULL tx_comm instance\n");
		return -EINVAL;
	}
	tx_hw = &tx_comm->hw_comm->hw_base;

	if ((hdcp_mode & HDCP_MODE22) && (hdcp_mode & HDCP_MODE14))
		tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_AUTO;
	else if (hdcp_mode & HDCP_MODE22)
		tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_HDCP2X;
	else if (hdcp_mode & HDCP_MODE14)
		tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_HDCP1X;
	else
		tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_NONE;

	if (tx_comm->hdcptx_comm.hdcptx_start_type == HDCP_START_AUTO)
		tx_comm->hdcptx_comm.hdcp_mode_fallback_state = HDCP_FALLBACK_NONE;
	else
		tx_comm->hdcptx_comm.hdcp_mode_fallback_state = HDCP_FALLBACK_LOCKED;

	if (hdcp_state == DP_HDCP_FORCE || hdcp_state == DP_HDCP_KEY_READY) {
		en = 0;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_HDCP2X_AUTH_DISABLE, tx_comm, NULL);
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
		switch (tx_comm->hdcptx_comm.hdcptx_start_type) {
		case HDCP_START_HDCP2X:
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_HDCP2X_AUTH_ENABLE,
						tx_comm, NULL);
			break;
		case HDCP_START_HDCP1X:
			en = 1;
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
			break;
		case HDCP_START_NONE:
			break;
		default:
			DPTX_DEBUG("%s don't use hdcp_state:%d\n", __func__, hdcp_state);
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(dptx_common_set_hdcp_mode);

static void dump_raw_dpcd(u8 *buf, u8 len, u32 addr)
{
	u8 *tmp_buf;
	u32 i, pos;

	tmp_buf = kmalloc(2048, GFP_KERNEL);
	if (!tmp_buf)
		return;

	for (i = 0, pos = 0; i < len; i++) {
		if (!buf[i])
			continue;
		pos += sprintf(tmp_buf + pos, "[%05x] %02x\n", addr + i, buf[i]);
	}
	pr_info("%s", tmp_buf);
	kfree(tmp_buf);
}

static void dptx_phy_automated_test(struct dptx_common *tx_comm)
{
	struct meson_tx_hw *tx_hw_base;
	struct meson_tx_phy_cfg_opts *phy_conf;
	u8 dpcd_buf218[DP_TEST_LANE_COUNT - DP_TEST_REQUEST + 1] = {0};
	u8 dpcd_buf248[1] = {0};
	u8 dpcd_buf250[DP_TEST_80BIT_PTTN_AUXCH_CTRL - DP_TEST_80BIT_CUSTOM_PATTERN_7_0 + 1] = {0};
	u8 dpcd_tmp;
	u8 lr;
	u8 lc;
	u8 phy_test_pattern = 0;
	u8 temp;

	if (!tx_comm)
		return;
	tx_hw_base = tx_comm->base.tx_hw_base;
	phy_conf = &tx_hw_base->tx_phy_conf;
	dptx_aux_read_dpcd(tx_comm->tx_aux, DP_TEST_REQUEST, dpcd_buf218, sizeof(dpcd_buf218));
	dump_raw_dpcd(dpcd_buf218, sizeof(dpcd_buf218), DP_TEST_REQUEST);
	DPTX_INFO("TEST_REQUEST: %#x\n", dpcd_buf218[0]);

	if (!(dpcd_buf218[0] & (DP_TEST_LINK_TRAINING | DP_TEST_LINK_PHY_TEST_PATTERN))) {
		dpcd_tmp = DP_TEST_NAK;
		dptx_aux_write_dpcd(tx_comm->tx_aux, DP_TEST_RESPONSE, &dpcd_tmp, 1);
		pr_info("DP_TEST_NAK\n");
		return;
	}
	dpcd_tmp = DP_TEST_ACK;
	dptx_aux_write_dpcd(tx_comm->tx_aux, DP_TEST_RESPONSE, &dpcd_tmp, 1);
	pr_info("DP_TEST_ACK\n");

	lr = dpcd_buf218[DP_TEST_LINK_RATE - DP_TEST_REQUEST];
	lc = dpcd_buf218[DP_TEST_LANE_COUNT - DP_TEST_REQUEST] & 0x1f;
	tx_comm->link_train->force_lr = lr;
	phy_conf->dp_ops.link_rate = lr * 270;
	tx_comm->link_train->force_lc = lc;
	phy_conf->dp_ops.lanes = lc;
	meson_tx_phy_set_mode(tx_hw_base->tx_phy, PHY_DP, phy_conf);

	if (dpcd_buf218[0] & DP_TEST_LINK_TRAINING) {
		pr_info("DP_TEST_LINK_TRAINING\n");
		if (dptx_link_training(tx_comm->link_train) != 0)
			DPTX_ERROR("link training failed during dp test\n");
	}

	if (dpcd_buf218[0] & DP_TEST_LINK_PHY_TEST_PATTERN) {
		pr_info("DP_TEST_LINK_PHY_TEST_PATTERN\n");
		dptx_aux_read_dpcd(tx_comm->tx_aux, DP_PHY_TEST_PATTERN, dpcd_buf248,
				   sizeof(dpcd_buf248));
		dptx_aux_read_dpcd(tx_comm->tx_aux, DP_TEST_80BIT_CUSTOM_PATTERN_7_0, dpcd_buf250,
				   sizeof(dpcd_buf250));
		dump_raw_dpcd(dpcd_buf248, sizeof(dpcd_buf248), DP_PHY_TEST_PATTERN);
		dump_raw_dpcd(dpcd_buf250, sizeof(dpcd_buf250), DP_TEST_80BIT_CUSTOM_PATTERN_7_0);
		if (lc == 1)
			phy_test_pattern = dpcd_buf248[0];
		if (lc == 2)
			phy_test_pattern = (dpcd_buf248[0] << 8) | dpcd_buf248[0];
		if (lc == 4)
			phy_test_pattern = (dpcd_buf248[0] << 24) | (dpcd_buf248[0] << 16) |
					   (dpcd_buf248[0] << 8) | dpcd_buf248[0];
		temp = dpcd_buf250[DP_TEST_80BIT_AUX_CH_CAP - DP_TEST_80BIT_CUSTOM_PATTERN_7_0];
		if (temp & DP_TEST_80BIT_AUX_CH_CAP_SUP) {
			u32 cust_80bit[3];

			pr_info("DP_TEST_80BIT_AUX_CH_CAP_SUP\n");
			cust_80bit[0] = dpcd_buf250[3] | dpcd_buf250[2] | dpcd_buf250[1] |
					dpcd_buf250[0];
			cust_80bit[1] = dpcd_buf250[7] | dpcd_buf250[6] | dpcd_buf250[5] |
					dpcd_buf250[4];
			cust_80bit[2] = dpcd_buf250[9] | dpcd_buf250[8];
			temp = dpcd_buf250[DP_TEST_80BIT_PTTN_AUXCH_CTRL -
				DP_TEST_80BIT_CUSTOM_PATTERN_7_0];
			dpcd_tmp = DP_TEST_80BIT_PTTN_AUXCH_CTRL_EN |
				   (temp & DP_TEST_DURATION_CTRL);
			dptx_aux_write_dpcd(tx_comm->tx_aux, DP_TEST_80BIT_PTTN_AUXCH_CTRL,
					    &dpcd_tmp, 1);
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_QUAL_PATTERN,
				     &phy_test_pattern, cust_80bit);
		} else {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_QUAL_PATTERN,
				     &phy_test_pattern, NULL);
		}
	}
}

struct meson_tx_helper_ops dptx_common_helper_ops = {
	.create_sysfs	= dptx_create_sysfs,
	.remove_sysfs	= dptx_remove_sysfs,
	.hpd_plugin		= dptx_hpd_plugin,
	.hpd_plugout	= dptx_hpd_plugout,
	.validate_fmt_para	= dptx_validate_tx_state_fmt_para,
	.pre_mode_enable	= dptx_pre_mode_enable,
	.mode_enable		= dptx_mode_enable,
	.post_mode_enable	= dptx_post_mode_enable,
};

/* hpd_irq interrupt bottom handler */
static void dptx_irq_hpd_process(void *para)
{
	struct dptx_common *tx_comm = (struct dptx_common *)para;
	ssize_t ret = 0;
	/* 00200h~00205h */
	u8 link_sink_status_buf[6] = {0};
	/* 02002h~0200fh */
	u8 dprx_esi_buf[14] = {0};
	u8 sink_count = 0;
	u8 rxstatus = 0;
	/* hdcp1.x */
	u8 bstatus = 0;

	if (!tx_comm) {
		DPTX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}

	/*
	 * 1.per DisplayPort Standard v1.4a chapter5.1.4,
	 * The Source device shall respond to an IRQ_HPD pulse by
	 * starting to read the registers located at DPCD Addresses
	 * 00200h through 00205h -or- DPCD Addresses 02002h through
	 * 0200Fh, by way of AUX read transactions within 100ms of
	 * the HPD signal's rising edge.
	 *
	 * 2. TODO for MST case:
	 * Per Table2-170: An MST upstream device shall use 02002h
	 * through 0200Fh instead of the Link/Sink Device Status field
	 * registers, starting from DPCD Address 00200h.
	 */
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_SINK_COUNT, link_sink_status_buf,
		sizeof(link_sink_status_buf));
	if (ret < 0) {
		DPTX_ERROR("%s link/sink device status read ret:%d\n", __func__, ret);
		return;
	}

	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_SINK_COUNT_ESI, dprx_esi_buf,
		sizeof(dprx_esi_buf));
	if (ret < 0) {
		DPTX_ERROR("%s DPRX Event Status Indicator read ret:%d\n", __func__, ret);
		return;
	}

	/* Automated test request from TE */
	if (link_sink_status_buf[1] & DP_AUTOMATED_TEST_REQUEST)
		dptx_phy_automated_test(tx_comm);

	/* cp_irq */
	if (link_sink_status_buf[1] & DP_CP_IRQ) {
		if (tx_comm->hdcptx_comm.hdcp_mode == 1) {
			/* refer to DP HDCP1.1 spec chapter2.7 step1~8 */
			ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_AUX_HDCP_BSTATUS,
				&bstatus, 1);
			if (ret != 1) {
				DPTX_ERROR("bstatus read ret:%d\n", ret);
				return;
			}
			tx_comm->hw_comm->dptx_hdcp1x_inst->bstatus = bstatus;
			if ((bstatus & DP_BSTATUS_LINK_FAILURE) ||
				(bstatus & DP_BSTATUS_REAUTH_REQ)) {
				/* Bstatus REAUTH_REQ is only for hdcp repeater */
				dptx_hw_cntl(&tx_comm->hw_comm->hw_base,
					DP_HDCP1X_REAUTH, NULL, NULL);
			} else if (bstatus & DP_BSTATUS_READY) {
				dptx_hw_cntl(&tx_comm->hw_comm->hw_base,
					DP_HDCP1X_KSV_LIST_READY, NULL, NULL);
			} else if (bstatus & DP_BSTATUS_R0_PRIME_READY) {
				dptx_hw_cntl(&tx_comm->hw_comm->hw_base,
					DP_HDCP1X_R0_PRIME_READY, NULL, NULL);
			}
			/* ignore other interrupt and continue HDCP session without aborting */
		} else if (tx_comm->hdcptx_comm.hdcp_mode == 2) {
			ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_RXSTATUS_OFFSET,
					 &rxstatus, sizeof(rxstatus));
			if (ret < 0) {
				DPTX_ERROR("%s rxstatus read ret:%d\n", __func__, ret);
				return;
			}
			tx_comm->hw_comm->dp_hdcp->rxstatus = rxstatus;

			/*
			 * When set to one, indicates that loss of
			 * cipher synchronization was detected at
			 * the HDCP Receiver during a link integrity check.
			 * This value must be reset by the HDCP Receiver on
			 * every new authentication request by the HDCP Transmitter
			 * as indicated by a write of the AKE_Init message.
			 */
			if (rxstatus & DP_RXSTATUS_LINK_INTEGRITY_FAILURE) {
				//todo
				//stop hdcp
				//resend ake_init
			}

			/* When set to one, indicates that the upstream side
			 * of the HDCP Repeater has transitioned in to an
			 * unauthenticated state from State C5, when State C5 is
			 * implemented in parallel with State C8, or from State
			 * C6 (See Section 2.10.3). The HDCP Transmitter may
			 * initiate re-authentication with the HDCP Repeater.
			 * This value must be reset by the HDCP Receiver on
			 * every new authentication request by the HDCP Transmitter
			 * as indicated by a write of the AKE_Init message.
			 */
			if (rxstatus & DP_RXSTATUS_REAUTH_REQ) {
				//todo
				//stop hdcp
				//resend ake_init
			}

			/*
			 * When set to one, indicates that Ekh(km) is available
			 * for reading at the HDCP Receiver. This value must be
			 * reset by the HDCP Receiver as soon as Ekh(km)is read
			 * by the HDCP Transmitter.
			 *
			 * PAIRING_AVAILABLE and read AKE_Send_Pairing_Info,
			 * 200ms ready, 5ms read done
			 */
			if (rxstatus & DP_RXSTATUS_PAIRING_AVAILABLE)
				complete(&tx_comm->hw_comm->dp_hdcp->pairing_available_completion);

			/* H_AVAILABLE read AKE_Send_H_prime, 1s ake_no_store_km,
			 * 200ms ake_store_km
			 */
			if (rxstatus & DP_RXSTATUS_H_AVAILABLE)
				complete(&tx_comm->hw_comm->dp_hdcp->h_available_completion);

			/* READY */
			if (rxstatus & DP_RXSTATUS_READY)
				complete(&tx_comm->hw_comm->dp_hdcp->ready_completion);
		} else {
			DPTX_INFO("should ignore CP_IRQ as hdcp_mode: %d\n",
				tx_comm->hdcptx_comm.hdcp_mode);
		}
	}

	/* DEVICE_SERVICE_IRQ_VECTOR: 00201h */
	if (link_sink_status_buf[1] & BIT(4)) {
		DPTX_INFO("%s DOWN_REP_MSG_RDY, TODO for MST\n", __func__);
		/* TODO for MST
		 * per chapter 2.11.4.1.3 of dp1.4a, the Sideband DOWN_REP_MSG is read from
		 * the MST DP RX DOWN_REP DPCD locations. The DPTX device shall clear the
		 * DOWN_REP_MSG_RDY bit to 0 when reading of the message is complete
		 */
	}
	if (link_sink_status_buf[1] & BIT(5)) {
		DPTX_INFO("%s UP_REQ_MSG_RDY, TODO for MST\n", __func__);
		/* TODO for MST
		 * per chapter 2.11.4.1.4 of dp1.4a, the Sideband UP_REQ_MSG is read from
		 * the MST DP RX UP_REQ DPCD locations. The MST DPTX device shall clear the
		 * UP_REQ_MSG_RDY bit to 0 when reading of the message is complete.
		 */
	}

	/* DEVICE_SERVICE_IRQ_VECTOR: 00201h */
	if (link_sink_status_buf[1] & BIT(6))
		DPTX_INFO("%s sink Device-specific IRQ, ignore\n", __func__);

	/* LANE_ALIGN_STATUS_UPDATED 0x00204h
	 * refer to chapter 5.3.3.1 of dp1.4a spec and Table 2-163
	 */
	if (link_sink_status_buf[4] & BIT(6)) {
		/* SINK_COUNT: 0x00200h, bit7,5:0 */
		sink_count = (link_sink_status_buf[2] >> 7) << 6 |
			(link_sink_status_buf[2] & 0x3f);
		DPTX_INFO("%s DOWNSTREAM_PORT_STATUS_CHANGED to: %d\n", __func__, sink_count);
	}
	if (link_sink_status_buf[4] & BIT(7)) {
		u8 rx_lane_st[6] = {0};
		int i;

		DPTX_INFO("%s LINK_STATUS_UPDATED %d\n", __func__, sink_count);
		ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_LANE0_1_STATUS, &rx_lane_st,
					 sizeof(rx_lane_st));
		if (ret < 0) {
			DPTX_ERROR("dpcd read lane status failed: %d\n", __func__, ret);
		} else {
			for (i = 0; i < sizeof(rx_lane_st); i++)
				DPTX_INFO("DPCD[0x%05x] = 0x%x", DP_LANE0_1_STATUS + i,
					  rx_lane_st[i]);
		}
		/* re-training */
		if (dptx_link_training(tx_comm->link_train) != 0)
			DPTX_ERROR("%s link training failed, abort mode set\n", __func__);
	}

	/* DEVICE_SERVICE_IRQ_VECTOR_ESI1: 02004 */
	if (dprx_esi_buf[2] & DP_RX_GTC_MSTR_REQ_STATUS_CHANGE) {
		/* Table 2-170 of dp1.4a spec */
		dptx_gtc_stop(tx_comm);
		DPTX_INFO("RX gtc master request change\n");
	}
	if (dprx_esi_buf[2] & DP_LOCK_ACQUISITION_REQUEST) {
		/* Table 2-170 of dp1.4a spec */
		dptx_gtc_stop(tx_comm);
		DPTX_INFO("RX lock acquisition request\n");
		dptx_gtc_start(tx_comm);
	}

	if (dprx_esi_buf[2] & BIT(2)) {
		/* Table 2-170 of dp1.4a spec */
		DPTX_INFO("%s CEC-Tunneling-over-AUX status changed.\n", __func__);
	}

	/* LINK_SERVICE_IRQ_VECTOR_ESI0: 02005h */
	if (dprx_esi_buf[3] & BIT(0)) {
		DPTX_INFO("%s RX_CAP_CHANGED: TODO\n", __func__);
		/*
		 * refer to Table 2-170, Upstream device shall read the Receiver
		 * Capability filed(00000h throuth 000ffh)
		 */
		ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_DPCD_REV, tx_comm->base.dpcd_buf,
			sizeof(tx_comm->base.dpcd_buf));
		if (ret < 0) {
			DPTX_ERROR("%s link/sink device status read ret:%d\n", __func__, ret);
			return;
		}
		/*
		 * TODO: need to do re-training.
		 * whether need to notify system to change mode?
		 */
	}
	if (dprx_esi_buf[3] & BIT(1)) {
		DPTX_INFO("%s LINK_STATUS_CHANGED\n", __func__);
		/*
		 * per Table 2-170, Upstream device shall read the
		 * Link/Sink Device Status field (DPCD Addresses
		 * 00200h through 002FFh.
		 * Note: spec do not say other actions besides DPCD read
		 */
		ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_DPCD_REV, tx_comm->base.dpcd_buf,
			sizeof(tx_comm->base.dpcd_buf));
		if (ret < 0) {
			DPTX_ERROR("%s link/sink device status read ret:%d\n", __func__, ret);
			return;
		}
	}
	if (dprx_esi_buf[3] & BIT(2)) {
		DPTX_INFO("%s STREAM_STATUS_CHANGED: TODO\n", __func__);
		/*
		 * per Table 2-170, Upstream Source device shall
		 * re-check the Stream Status with the
		 * QUERY_STREAM_ENCRYPTION_STATUS message.
		 */
	}
	if (dprx_esi_buf[3] & BIT(3)) {
		DPTX_INFO("%s HDMI_LINK_STATUS_CHANGED: TODO\n", __func__);
		/*
		 * per Table 2-170, Upstream DP device shall read
		 * the DP-to-HDMI protocol converter's downstream
		 * HDMI link status registers.
		 */
	}
	if (dprx_esi_buf[3] & BIT(4)) {
		DPTX_INFO("%s CONNECTED_OFF_ENTRY_REQUESTED\n", __func__);
		/* per Table 2-170, write 1 to l clear this bit in DPRX */
	}

	/* DSC_STATUS: 0020Fh and 02011h */
	if (dprx_esi_buf[13] & BIT(0)) {
		DPTX_INFO("%s RC Buffer Under-run, TODO for DSC\n", __func__);
		/* per Table 2-163, Sticky until cleared by a write of 1 */
	}
	if (dprx_esi_buf[13] & BIT(1)) {
		DPTX_INFO("%s RC Buffer Overflow, TODO for DSC\n", __func__);
		/* per Table 2-163, Sticky until cleared by a write of 1 */
	}
	if (dprx_esi_buf[13] & BIT(2)) {
		DPTX_INFO("%s Chunk Length mismatch with PPS, TODO for DSC\n", __func__);
		/* per Table 2-163, Sticky until cleared by a write of 1 */
	}
}

static struct tx_task_info dptx_common_task_infos[] = {
	{
		.name = "irq_hpd_task",
		.fn = dptx_irq_hpd_process,
		.type = IRQ_HPD_TASK,
		.queue_type = TASK_QUEUE_HPD_IRQ,
		.flag = TASK_FLAG_DELAY_WORK,
		.init_queue_name = "irq_hpd",
		.queue_name = "irq_hpd",
		.queue_flag = WQ_HIGHPRI | WQ_CPU_INTENSIVE,
	}
};

/* task init for dptx common */
static int dptx_common_task_init(struct dptx_common *tx_comm)
{
	int i;
	int ret = 0;

	if (!tx_comm) {
		DPTX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(dptx_common_task_infos); i++) {
		sprintf(dptx_common_task_infos[i].queue_name, "dptx%d_%s", tx_comm->dev_idx,
			dptx_common_task_infos[i].init_queue_name);
		ret = tx_task_mgr_setup_task(tx_comm->base.task_mgr,
			&dptx_common_task_infos[i], tx_comm);
		if (ret)
			DPTX_ERROR("%s setup task[%s] failed\n",
				__func__, dptx_common_task_infos[i].name);
	}
	return 0;
}

int dptx_common_init(struct dptx_common *tx_comm, struct dptx_hw_common *hw_comm)
{
	struct meson_tx_dev *tx_dev = &tx_comm->base;
	int ret = 0;

	if (!tx_comm || !hw_comm) {
		DPTX_ERROR("%s: invalid input pointer\n", __func__);
		return -EINVAL;
	}
	/* meson_tx_dev common init */
	meson_tx_dev_init(tx_dev, &hw_comm->hw_base, &dptx_common_helper_ops);
	mutex_init(&tx_dev->set_mode_mutex);
	mutex_init(&tx_dev->valid_mode_mutex);

	spin_lock_init(&tx_comm->edid_spinlock);
	/* parse rx cap in both EDID and displayID */
	tx_dev->edid_parse_mask = 0x3;
	tx_comm->base.tx_vinfo.vout_device = &dptx_vdev;
	tx_comm->base.tx_vinfo.vout_device->tx_instance = tx_comm;
	/* init the timers' mutex */
	dptx_timer_init(hw_comm);
	/* dptx aux init */
	tx_comm->tx_aux = dptx_aux_init(hw_comm);
	tx_comm->ready = 0;
	tx_comm->hdcptx_comm.hdcp_user = 0;
	if (!tx_comm->tx_aux)
		return -ENOMEM;

	/* pass aux instance to HW side for aux access during mode set */
	tx_comm->hw_comm->tx_aux = tx_comm->tx_aux;
	tx_comm->link_train = dptx_link_train_init(tx_comm);
	if (!tx_comm->link_train)
		return -ENOMEM;
	hw_comm->is_edp = tx_comm->is_edp;
	hw_comm->ext_edptx_ctrl_en = tx_comm->ext_edptx_drv_en;
	hw_comm->hw_base.lane_remap = -1;

	/* init connector_type */
	tx_comm->base.conn_dev.connector_type =
		(tx_comm->is_edp ? DRM_MODE_CONNECTOR_MESON_TX_EDP_A :
		DRM_MODE_CONNECTOR_MESON_TX_DP_A)
		+ tx_comm->enc_idx;
	/* dptx vout init */
	dptx_vout_init(tx_comm);
	/* dptx common event/task init */
	ret = dptx_common_task_init(tx_comm);
	if (ret < 0)
		return ret;

	return 0;
}

int dptx_common_uninit(struct dptx_common *tx_comm)
{
	if (!tx_comm)
		return -EINVAL;

	dptx_vout_uninit(tx_comm);
	dptx_link_train_uninit(tx_comm->link_train);
	kfree(tx_comm->base.tx_cap);
	tx_comm->base.tx_cap = NULL;
	kfree(tx_comm->tx_aux);
	tx_comm->tx_aux = NULL;
	dptx_timer_uninit(tx_comm->hw_comm);
	meson_tx_tracer_destroy(tx_comm->base.tx_tracer);
	meson_tx_event_mgr_destroy(tx_comm->base.event_mgr);
	meson_tx_dev_release(&tx_comm->base);
	return 0;
}

