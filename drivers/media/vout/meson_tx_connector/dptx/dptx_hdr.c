// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include "dptx_log.h"
#include "dptx_infoframe.h"
#include "dptx_hdr.h"

/* Need to be called in edid_spinlock */
static void dptx_set_sdr_pkt(struct dptx_common *tx_comm, struct drm_infoframe *info)
{
	struct dptx_hw_common *hw_comm = NULL;

	if (!tx_comm || !info) {
		DPTX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	hw_comm = tx_comm->hw_comm;
	if (!hw_comm) {
		DPTX_ERROR("hdr: [%s]: null hw_comm param\n", __func__);
		return;
	}

	if (tx_comm->hdr_transfer_feature == T_BT709 &&
			tx_comm->hdr_color_feature == C_BT709) {
		if (tx_comm->base.rxcap.hdr_info.hdr_support & 0x1) {
			/* edid supports SDR */
			DPTX_INFO("hdr: [%s]: send zero drm pkt\n", __func__);
			dptx_drm_infoframe_set(hw_comm, 0, info);
		} else {
			/* edid does not support SDR */
			DPTX_INFO("hdr: [%s]: disabled drm pkt\n", __func__);
			dptx_drm_infoframe_set(hw_comm, 0, NULL);
		}
	}
}

static int dptx_drm_infoframe_set_hdr_metadata(struct drm_infoframe *frame,
						struct master_display_info_s *data)
{
	int ret = 0;

	if (!frame || !data)
		return -EINVAL;

	ret = drm_infoframe_init(frame);
	if (ret < 0)
		return ret;

	frame->metadata_type = STATIC_METADATA_TYPE1;
	frame->display_primaries[0].x = data->primaries[0][0];
	frame->display_primaries[0].y = data->primaries[0][1];
	frame->display_primaries[1].x = data->primaries[1][0];
	frame->display_primaries[1].y = data->primaries[1][1];
	frame->display_primaries[2].x = data->primaries[2][0];
	frame->display_primaries[2].y = data->primaries[2][1];
	frame->white_point.x = data->white_point[0];
	frame->white_point.y = data->white_point[1];
	frame->max_display_mastering_luminance = data->luminance[0];
	frame->min_display_mastering_luminance = data->luminance[1];
	frame->max_cll = data->max_content;
	frame->max_fall = data->max_frame_average;

	return ret;
}

void dptx_set_drm_pkt(void *tx_instance, struct master_display_info_s *data)
{
	struct dptx_common *tx_comm = (struct dptx_common *)tx_instance;
	struct dptx_hw_common *hw_comm = NULL;
	struct rx_cap *prxcap = NULL;
	struct hdr_info *hdr_info = NULL;
	enum hdmi_hdr_transfer hdr_transfer_feature = T_UNKNOWN;
	enum hdmi_hdr_color hdr_color_feature = C_UNKNOWN;
	unsigned int colormetry = 0;
	enum hdr_mode hdr_mode = OTHER;
	struct drm_infoframe info = {};
	struct meson_tx_format_para *para = NULL;
	unsigned long flags = 0;

	if (!tx_comm) {
		DPTX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	hw_comm = tx_comm->hw_comm;
	if (!hw_comm) {
		DPTX_ERROR("hdr: [%s]: null hw_comm param\n", __func__);
		return;
	}

	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);
	prxcap = &tx_comm->base.rxcap;
	hdr_info = &prxcap->hdr_info;
	para = &tx_comm->base.sw_fmt_para;

	DPTX_DEBUG_PACKET("%s[%d]\n", __func__, __LINE__);
	if (data) {
		hdr_transfer_feature = (data->features >> 8) & 0xff;
		hdr_color_feature = (data->features >> 16) & 0xff;
		colormetry = (data->features >> 30) & 0x1;
	}

	if (!data || !tx_comm->base.rxcap.hdr_info.hdr_support) {
		dptx_drm_infoframe_set(hw_comm, 0, NULL);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_CLR_BT2020, (void *)&para, NULL);
		tx_comm->hdr_transfer_feature = T_UNKNOWN;
		tx_comm->hdr_color_feature = C_UNKNOWN;
		tx_comm->colormetry = 0;
		DPTX_INFO("hdr: [%s]: disable drm pkt\n", __func__);
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	//if (tx_comm->base.ready == 0) {
		//spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		//return;
	//}

	if (tx_comm->hdr_transfer_feature != hdr_transfer_feature ||
			tx_comm->hdr_color_feature != hdr_color_feature ||
			tx_comm->colormetry != colormetry) {
		tx_comm->hdr_transfer_feature = hdr_transfer_feature;
		tx_comm->hdr_color_feature = hdr_color_feature;
		tx_comm->colormetry = colormetry;
		DPTX_INFO("hdr: [%s]: tf = %d, cf = %d, colormetry = %d\n",
				    __func__, tx_comm->hdr_transfer_feature,
				    tx_comm->hdr_color_feature,
					tx_comm->colormetry);
		if (tx_comm->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			tx_comm->hdr_color_feature == C_BT2020)
			DPTX_INFO("hdr: [%s]: SDR->HDR10\n", __func__);
		else if (tx_comm->hdr_transfer_feature == T_HLG &&
				 tx_comm->hdr_color_feature == C_BT2020)
			DPTX_INFO("hdr: [%s]: SDR->HLG\n", __func__);
	}

	/* int drm infoframe */
	dptx_drm_infoframe_set_hdr_metadata(&info, data);
	/* SDR */
	if (tx_comm->hdr_transfer_feature == T_BT709 &&
		tx_comm->hdr_color_feature == C_BT709) {
		hdr_mode = SDR_WITH_BT709;
	}
	/* HDR10 and (BT2020 or NON_STANDARD) */
	if (tx_comm->hdr_transfer_feature == T_SMPTE_ST_2084) {
		if (tx_comm->hdr_color_feature == C_BT2020)
			hdr_mode = HDR10_WITH_BT2020;
		else if (tx_comm->hdr_color_feature != C_BT2020)
			hdr_mode = HDR10_WITH_BT709;
	}
	/* HLG and BT2020 */
	if (tx_comm->hdr_color_feature == C_BT2020 &&
			(tx_comm->hdr_transfer_feature == T_BT2020_10 ||
			 tx_comm->hdr_transfer_feature == T_HLG))
		hdr_mode = HLG_WITH_BT2020;

	/* bt2020 + gamma transfer */
	if (tx_comm->hdr_transfer_feature == T_BT709 &&
			tx_comm->hdr_color_feature == C_BT2020)
		hdr_mode = SDR_WITH_BT2020;

	switch (hdr_mode) {
	case SDR_WITH_BT709:
		info.eotf = EOTF_TRADITIONAL_GAMMA_SDR;
		tx_comm->colormetry = 0;
		dptx_set_sdr_pkt(tx_comm, &info);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_CLR_BT2020, (void *)&para, NULL);
		break;
	case HDR10_WITH_BT2020:
		/* standard HDR SMPTE ST 2084 */
		info.eotf = EOTF_SMPTE_ST2084;
		dptx_drm_infoframe_set(hw_comm, 0, &info);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_SET_BT2020, (void *)&para, NULL);
		break;
	case HDR10_WITH_BT709:
		/* non standard SMPTE ST 2084 */
		info.eotf = EOTF_SMPTE_ST2084;
		dptx_drm_infoframe_set(hw_comm, 0, &info);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_CLR_BT2020, (void *)&para, NULL);
		break;
	case HLG_WITH_BT2020:
		/* HLG */
		info.eotf = EOTF_BT_2100_HLG;
		dptx_drm_infoframe_set(hw_comm, 0, &info);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_SET_BT2020, (void *)&para, NULL);
		break;
	case SDR_WITH_BT2020:
		dptx_drm_infoframe_set(hw_comm, 0, NULL);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_SET_BT2020, (void *)&para, NULL);
		break;
	default:
		/* other case */
		dptx_drm_infoframe_set(hw_comm, 0, NULL);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, AUX_PKT_CLR_BT2020, (void *)&para, NULL);
		break;
	}

	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
}

void dptx_enable_tx_packet(void *tx_instance, union tx_packet *data)
{
	struct dptx_common *tx_comm = (struct dptx_common *)tx_instance;

	if (!tx_comm || !data)
		return;

	switch (data->any.type) {
	case DISPLAY_HDR:
		dptx_set_drm_pkt(tx_comm, &data->display_hdr);
		break;
	default:
		break;
	}
};

void dptx_disable_tx_packet(void *tx_instance, union tx_packet *data)
{
	struct dptx_common *tx_comm = (struct dptx_common *)tx_instance;

	if (!tx_comm || !data)
		return;

	switch (data->any.type) {
	case DISPLAY_HDR:
		dptx_set_drm_pkt(tx_comm, NULL);
		break;
	default:
		break;
	}

};
