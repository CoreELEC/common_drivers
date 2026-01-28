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
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>
#include "hdmitx_log.h"
#include "hdmitx_infoframe.h"
#include "meson_tx_task_mgr.h"
#include "hdmitx_hdr.h"
#include "hdmitx_vout.h"
#include "meson_tx_event_mgr.h"

/* AUX_PKT_CONF_AVI_Q01 */
#define RGB_RANGE_DEFAULT   0
#define RGB_RANGE_LIM       1
#define RGB_RANGE_FUL       2
#define RGB_RANGE_RSVD      3
/* AUX_PKT_CONF_AVI_YQ01 */
#define YCC_RANGE_LIM       0
#define YCC_RANGE_FUL       1
#define YCC_RANGE_RSVD      2

static void hdmitx_video_mute_op(struct hdmitx_common *tx_comm, bool flag)
{
	if (!tx_comm || !tx_comm->hdmi_init)
		return;

	/* video mute is handled in the vsync_intr_handler interface */
	if (flag)
		tx_comm->hdr_state->vid_mute_op = VIDEO_MUTE;
	else
		tx_comm->hdr_state->vid_mute_op = VIDEO_UNMUTE;
}

void hdr_unmute_work_func(void *para)
{
	unsigned int mute_us;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)para;
	struct meson_tx_hdr *tx_hdr = tx_comm->hdr_state;
	struct tx_timing *timing = &tx_hdr->bind_instance->fmt_para.timing;

	if (tx_hdr->hdr_mute_frame) {
		mute_us = tx_hdr->hdr_mute_frame * meson_tx_mode_get_frame_duration(timing);
		usleep_range(mute_us, mute_us + 10);
		hdmitx_video_mute_op(tx_comm, false);
	}
}

void hdr_work_func(void *para)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)para;
	struct meson_tx_hdr *tx_hdr = tx_comm->hdr_state;

	if (tx_hdr->hdmi_current_hdr_mode != 0)
		/* SDR -> HDR */
		meson_tx_event_mgr_send_uevent(tx_hdr->bind_instance->base.event_mgr,
				TX_HDR_EVENT, 1, false);
	else if (tx_hdr->hdmi_current_hdr_mode == 0)
		/* HDR -> SDR */
		meson_tx_event_mgr_send_uevent(tx_hdr->bind_instance->base.event_mgr,
				TX_HDR_EVENT, 0, false);
}

/* Need to be called in edid_spinlock */
static void hdmitx_set_sdr_pkt(struct hdmitx_common *tx_comm)
{
	struct hdmitx_hw_common *tx_hw = NULL;
	struct meson_tx_hdr *tx_hdr = NULL;
	unsigned char buffer[31] = {0x87, 0x1, 26};
	u32 arg = 0;

	if (!tx_comm || !tx_comm->hdr_state) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	tx_hdr = tx_comm->hdr_state;

	if (tx_hdr->hdr_transfer_feature == T_BT709 &&
			tx_hdr->hdr_color_feature == C_BT709) {
		if (tx_comm->base.rxcap.hdr_info.hdr_support & 0x1) {
			/* edid supports SDR */
			HDMITX_INFO("hdr: [%s]: send zero drm pkt\n", __func__);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, buffer, NULL);
		} else {
			/* edid does not support SDR */
			HDMITX_INFO("hdr: [%s]: disabled drm pkt\n", __func__);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
		}
		arg = tx_hdr->colormetry;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);

		/*
		 * disable DRM packets completely ONLY if hdr transfer
		 * feature and color feature still demand SDR.
		 */
		if (tx_hdr->all_zero_hdr10plus_pkt) {
			/* zero hdr10plus VSIF being sent - disable it */
			HDMITX_INFO("hdr: [%s]: disable hdr10plus vsif pkt\n", __func__);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
			tx_hdr->all_zero_hdr10plus_pkt = false;
		}
		/* update hdr mode flag */
		tx_hdr->hdmi_current_hdr_mode = 0;
		tx_hdr->hdmi_last_hdr_mode = tx_hdr->hdmi_current_hdr_mode;
	}
}

#define GET_LOW8BIT(a)	((a) & 0xff)
#define GET_HIGH8BIT(a)	(((a) >> 8) & 0xff)
void hdmitx_set_drm_pkt(void *tx_instance, struct master_display_info_s *data)
{
	unsigned char buffer[31] = {0x87, 0x1, 26};
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;
	struct hdmitx_hw_common *tx_hw = NULL;
	struct meson_tx_hdr *tx_hdr = NULL;
	unsigned long flags;
	struct rx_cap *prxcap = NULL;
	enum hdmi_tf_type hdmi_hdr_status = HDMI_NONE;
	struct hdr_info *hdr_info = NULL;
	u32 save_last_hdr_mode;
	enum hdmi_hdr_transfer hdr_transfer_feature = T_UNKNOWN;
	enum hdmi_hdr_color hdr_color_feature = C_UNKNOWN;
	unsigned int colormetry = 0;
	u32 arg = 0;

	if (!tx_comm || !tx_comm->hdr_state) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	tx_hdr = tx_comm->hdr_state;
	prxcap = &tx_comm->base.rxcap;
	hdr_info = &prxcap->hdr_info;
	hdmi_hdr_status = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_HDR_ST, NULL, NULL);

	HDMITX_DEBUG_PACKET("%s[%d]\n", __func__, __LINE__);
	if (data) {
		memcpy(&tx_hdr->drm_config_data, data,
				sizeof(struct master_display_info_s));
		hdr_transfer_feature = (data->features >> 8) & 0xff;
		hdr_color_feature = (data->features >> 16) & 0xff;
		colormetry = (data->features >> 30) & 0x1;
	} else {
		memset(&tx_hdr->drm_config_data, 0, sizeof(struct master_display_info_s));
	}

	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);

	if (!data || !tx_comm->base.rxcap.hdr_info.hdr_support) {
		buffer[1] = 0;
		buffer[2] = 0;
		buffer[4] = 0;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		tx_hdr->hdr_transfer_feature = T_UNKNOWN;
		tx_hdr->hdr_color_feature = C_UNKNOWN;
		tx_hdr->colormetry = 0;
		tx_hdr->hdmi_current_hdr_mode = 0;
		tx_hdr->hdmi_last_hdr_mode = 0;
		HDMITX_INFO("hdr: [%s]: disable drm pkt\n", __func__);
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	if (tx_comm->ready == 0) {
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	/*
	 * if currently output 8bit, and EDID don't support Y422, and config_csc_en is 0,
	 * switch to SDR output; if hdr_8bit_en is 1, switch to HDR output
	 */
	if (tx_comm->fmt_para.cd == COLORDEPTH_24B && !tx_hdr->hdr_8bit_en) {
		if (!(tx_hdr->config_csc_en && meson_tx_edid_support_y422(prxcap))) {
			tx_hdr->hdr_transfer_feature = T_BT709;
			tx_hdr->hdr_color_feature = C_BT709;
			tx_hdr->colormetry = 0;
			hdmitx_set_sdr_pkt(tx_comm);
			tx_task_mgr_queue_task(tx_comm->base.task_mgr, HDR_TASK, 0);
			meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_SDR);
			spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
			return;
		}
	}

	if (tx_hdr->all_zero_hdr10plus_pkt) {
		/* zero hdr10plus VSIF being sent - disable it */
		HDMITX_INFO("hdr: [%s]: disable hdr10plus zero vsif pkt\n", __func__);
		/* TODO, maybe need recover hdmi1.4b_vsif when 4k */
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
		tx_hdr->all_zero_hdr10plus_pkt = false;
	}

	if (tx_hdr->hdr_transfer_feature != hdr_transfer_feature ||
			tx_hdr->hdr_color_feature != hdr_color_feature ||
			tx_hdr->colormetry != colormetry) {
		tx_hdr->hdr_transfer_feature = hdr_transfer_feature;
		tx_hdr->hdr_color_feature = hdr_color_feature;
		tx_hdr->colormetry = colormetry;
		HDMITX_INFO("hdr: [%s]: tf = %d, cf = %d, colormetry = %d\n",
				    __func__, tx_hdr->hdr_transfer_feature,
				    tx_hdr->hdr_color_feature,
					tx_hdr->colormetry);
		if (tx_hdr->hdr_transfer_feature == T_SMPTE_ST_2084 &&
			tx_hdr->hdr_color_feature == C_BT2020)
			HDMITX_INFO("hdr: [%s]: SDR->HDR10\n", __func__);
		else if (tx_hdr->hdr_transfer_feature == T_HLG &&
				 tx_hdr->hdr_color_feature == C_BT2020)
			HDMITX_INFO("hdr: [%s]: SDR->HLG\n", __func__);
	}

	/* if VSIF/DV or VSIF/HDR10P packet is enabled, disable it */
	if (hdmitx_dv_en(tx_hw)) {
		arg = tx_comm->fmt_para.cs;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS,
			(void *)&arg, NULL);
		/* if using VSIF/DOVI, then only clear DV_VS10_SIG, else disable VSIF */
		if (hdmitx_hw_cntl(tx_hw, AUX_PKT_CLR_DV_VS10_SIG, NULL, NULL) == 0)
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
	}

	/* hdr10+ content on a hdr10 sink case */
	if (tx_hdr->hdr_transfer_feature == 0x30) {
		if (hdr_info->hdr10plus_info.ieeeoui != 0x90848B ||
				hdr_info->hdr10plus_info.application_version != 1) {
			tx_hdr->hdr_transfer_feature = T_SMPTE_ST_2084;
			HDMITX_INFO("hdr: [%s]: hdr10plus not supported, treat as hdr10\n",
				__func__);
		}
	}

	/* SDR */
	if (tx_hdr->hdr_transfer_feature == T_BT709 &&
		tx_hdr->hdr_color_feature == C_BT709) {
		/* send zero drm packet on HDR->SDR transition */
		if (hdmi_hdr_status == HDMI_HDR_SMPTE_2084 || hdmi_hdr_status == HDMI_HDR_HLG) {
			if (hdmi_hdr_status == HDMI_HDR_SMPTE_2084)
				HDMITX_INFO("hdr: [%s]: HDR10->SDR\n", __func__);
			else
				HDMITX_INFO("hdr: [%s]: HLG->SDR\n", __func__);
			tx_hdr->colormetry = 0;
			hdmitx_set_sdr_pkt(tx_comm);
			tx_task_mgr_queue_task(tx_comm->base.task_mgr, HDR_TASK, 0);
			meson_tx_tracer_write_event(tx_comm->base.tx_tracer, TX_HDR_MODE_SDR);
			buffer[4] = 0;
			/* if 8bit hdr mode is enabled, no need small mode */
			if (!tx_hdr->hdr_8bit_en) {
				/* back to previous cs */
				/*
				 * currently output y444,8bit or rgb,8bit, if exit playing,
				 * then switch back to 8bit mode
				 */
				if (tx_comm->fmt_para.cs == HDMI_COLORSPACE_YUV444 &&
					tx_comm->fmt_para.cd == COLORDEPTH_24B) {
					arg = CSC_Y444_8BIT | CSC_UPDATE_AVI_CS;
					hdmitx_hw_cntl(tx_hw, VPU_CONFIG_CSC,
						(void *)&arg, NULL);
					HDMITX_INFO("hdr: [%s]: switch back to cs: %d, cd: %d\n",
						__func__, tx_comm->fmt_para.cs,
						tx_comm->fmt_para.cd);
				} else if (tx_comm->fmt_para.cs == HDMI_COLORSPACE_RGB &&
					tx_comm->fmt_para.cd == COLORDEPTH_24B) {
					arg = CSC_RGB_8BIT | CSC_UPDATE_AVI_CS;
						hdmitx_hw_cntl(tx_hw, VPU_CONFIG_CSC,
						(void *)&arg, NULL);
					HDMITX_INFO("hdr: [%s]: switch back to cs: %d, cd: %d\n",
						__func__, tx_comm->fmt_para.cs,
						tx_comm->fmt_para.cd);
				}
			}
		}
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	buffer[5] = 0x0;
	buffer[6] = GET_LOW8BIT(data->primaries[0][0]);
	buffer[7] = GET_HIGH8BIT(data->primaries[0][0]);
	buffer[8] = GET_LOW8BIT(data->primaries[0][1]);
	buffer[9] = GET_HIGH8BIT(data->primaries[0][1]);
	buffer[10] = GET_LOW8BIT(data->primaries[1][0]);
	buffer[11] = GET_HIGH8BIT(data->primaries[1][0]);
	buffer[12] = GET_LOW8BIT(data->primaries[1][1]);
	buffer[13] = GET_HIGH8BIT(data->primaries[1][1]);
	buffer[14] = GET_LOW8BIT(data->primaries[2][0]);
	buffer[15] = GET_HIGH8BIT(data->primaries[2][0]);
	buffer[16] = GET_LOW8BIT(data->primaries[2][1]);
	buffer[17] = GET_HIGH8BIT(data->primaries[2][1]);
	buffer[18] = GET_LOW8BIT(data->white_point[0]);
	buffer[19] = GET_HIGH8BIT(data->white_point[0]);
	buffer[20] = GET_LOW8BIT(data->white_point[1]);
	buffer[21] = GET_HIGH8BIT(data->white_point[1]);
	buffer[22] = GET_LOW8BIT(data->luminance[0]);
	buffer[23] = GET_HIGH8BIT(data->luminance[0]);
	buffer[24] = GET_LOW8BIT(data->luminance[1]);
	buffer[25] = GET_HIGH8BIT(data->luminance[1]);
	buffer[26] = GET_LOW8BIT(data->max_content);
	buffer[27] = GET_HIGH8BIT(data->max_content);
	buffer[28] = GET_LOW8BIT(data->max_frame_average);
	buffer[29] = GET_HIGH8BIT(data->max_frame_average);

	/* bt2020 + gamma transfer */
	if (tx_hdr->hdr_transfer_feature == T_BT709 &&
			tx_hdr->hdr_color_feature == C_BT2020) {
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
		arg = SET_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	/* must clear hdr mode */
	tx_hdr->hdmi_current_hdr_mode = 0;

	/* HDR10 and (BT2020 or NON_STANDARD) */
	if (tx_hdr->hdr_transfer_feature == T_SMPTE_ST_2084) {
		if (tx_hdr->hdr_color_feature == C_BT2020)
			tx_hdr->hdmi_current_hdr_mode = 1;
		else if (tx_hdr->hdr_color_feature != C_BT2020)
			tx_hdr->hdmi_current_hdr_mode = 2;
	}
	/* HLG and BT2020 */
	if (tx_hdr->hdr_color_feature == C_BT2020 &&
			(tx_hdr->hdr_transfer_feature == T_BT2020_10 ||
			 tx_hdr->hdr_transfer_feature == T_HLG))
		tx_hdr->hdmi_current_hdr_mode = 3;

	switch (tx_hdr->hdmi_current_hdr_mode) {
	case 1:
		/* standard HDR SMPTE ST 2084 */
		buffer[4] = 0x02;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, buffer, NULL);
		arg = SET_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		meson_tx_tracer_write_event(tx_comm->base.tx_tracer, TX_HDR_MODE_SMPTE2084);
		break;
	case 2:
		/* non standard SMPTE ST 2084 */
		buffer[4] = 0x02;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, buffer, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		break;
	case 3:
		/* HLG */
		buffer[4] = 0x03;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, buffer, NULL);
		arg = SET_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		meson_tx_tracer_write_event(tx_comm->base.tx_tracer, TX_HDR_MODE_HLG);
		break;
	case 0:
	default:
		/* other case */
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		break;
	}

	if (tx_hdr->hdmi_current_hdr_mode != tx_hdr->hdmi_last_hdr_mode) {
		save_last_hdr_mode = tx_hdr->hdmi_last_hdr_mode;
		/* NOTE: for HDR <-> HLG, also need update last mode */
		tx_hdr->hdmi_last_hdr_mode = tx_hdr->hdmi_current_hdr_mode;
		/* only SDR->HDR10/SDR->HLG need mute */
		if (tx_hdr->hdr_mute_frame && save_last_hdr_mode == 0) {
			hdmitx_video_mute_op(tx_comm, true);
			HDMITX_INFO("hdr: video mute %d frames\n",
				tx_hdr->hdr_mute_frame);
			/*
			 * force unmute after specific frames,
			 * no need to check hdr status when unmute
			 */
			tx_task_mgr_queue_task(tx_comm->base.task_mgr, HDR_UNMUTE, 0);
		}
		tx_task_mgr_queue_task(tx_comm->base.task_mgr, HDR_TASK, 0);
	}
	/* if 8bit hdr mode is enabled, no need small mode switch to y422,12bit */
	if (tx_hdr->hdr_8bit_en) {
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}
	if (tx_hdr->hdmi_current_hdr_mode == 1 ||
		tx_hdr->hdmi_current_hdr_mode == 2 ||
		tx_hdr->hdmi_current_hdr_mode == 3) {
		/*
		 * currently output y444,8bit or rgb,8bit, and EDID
		 * support Y422, then switch to y422,12bit mode
		 */
		if ((tx_comm->fmt_para.cs == HDMI_COLORSPACE_YUV444 ||
			tx_comm->fmt_para.cs == HDMI_COLORSPACE_RGB) &&
			tx_comm->fmt_para.cd == COLORDEPTH_24B &&
			meson_tx_edid_support_y422(prxcap)) {
			arg = CSC_Y422_12BIT | CSC_UPDATE_AVI_CS;
			hdmitx_hw_cntl(tx_hw, VPU_CONFIG_CSC,
				(void *)&arg, NULL);
			HDMITX_DEBUG_PACKET("hdr: [%s]: switch to 422,12bit\n", __func__);
		}
	}
	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
}

void hdmitx_set_hdr10plus_pkt(void *tx_instance, unsigned int flag, struct hdr10plus_para *data)
{
	unsigned char buffer[31] = {0x81, 0x01, 0x1b};
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;
	struct hdmitx_hw_common *tx_hw = NULL;
	u32 vic = 0;
	u32 hdmi_vic_4k_flag = 0;
	struct rx_cap *prxcap = &tx_comm->base.rxcap;
	u32 arg = 0;
	struct meson_tx_hdr *tx_hdr = NULL;

	if (!tx_comm || !tx_comm->hdr_state) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	tx_hdr = tx_comm->hdr_state;
	vic = tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic;

	HDMITX_DEBUG_PACKET("hdr: [%s]: [%d]\n", __func__, __LINE__);
	if (data)
		memcpy(&tx_hdr->hdr10p_config_data, data, sizeof(struct hdr10plus_para));
	else
		memset(&tx_hdr->hdr10p_config_data, 0, sizeof(struct hdr10plus_para));

	/* if ready is 0, only can clear pkt */
	if (tx_comm->ready == 0)
		return;

	/* save vic to AVI when send HDR10P_VSIF */
	if (vic == HDMI_95_3840x2160p30_16x9 ||
		vic == HDMI_94_3840x2160p25_16x9 ||
		vic == HDMI_93_3840x2160p24_16x9 ||
		vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;

	if (flag == HDR10_PLUS_ZERO_VSIF) {
		/* needed during hdr10plus to sdr transition */
		HDMITX_INFO("hdr: [%s]: HDR10PLUS->SDR, send zero vsif pkt\n", __func__);
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		tx_hdr->hdr10plus_feature = 0;
		if (hdmi_vic_4k_flag) {
			arg = vic & 0xff;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
		}
		tx_hdr->all_zero_hdr10plus_pkt = true;
		/* When hdr10plus mode ends, clear hdr10plus_event flag */
		meson_tx_tracer_clean_hdr10plus_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_HDR10PLUS);
		meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_SDR);
		return;
	}

	if (tx_hdr->hdr10plus_feature != flag) {
		tx_hdr->hdr10plus_feature = flag;
		if (flag == HDR10_PLUS_ENABLE_VSIF)
			HDMITX_INFO("hdr: [%s]: SDR->HDR10PLUS\n", __func__);
		else if (flag == HDR10_PLUS_DISABLE_VSIF)
			HDMITX_INFO("hdr: [%s]: HDR10PLUS->SDR\n", __func__);
	}

	if (!data || !flag) {
		HDMITX_INFO("hdr: [%s]: disable hdr10plus vsif pkt\n", __func__);
		/* TODO, maybe need recover hdmi1.4b_vsif when 4k */
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1,  NULL, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		tx_hdr->hdr10plus_feature = 0;
		/* When hdr10plus mode ends, clear hdr10plus_event flag */
		meson_tx_tracer_clean_hdr10plus_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_HDR10PLUS);
		meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_SDR);

		return;
	}

	/*
	 * if currently output 8bit, and EDID don't support Y422, and config_csc_en is 0,
	 * switch to SDR output; if hdr_8bit_en is 1, switch to HDR output
	 */
	if (tx_comm->fmt_para.cd == COLORDEPTH_24B && !tx_hdr->hdr_8bit_en) {
		if (!(tx_hdr->config_csc_en && meson_tx_edid_support_y422(prxcap))) {
			tx_hdr->hdr_transfer_feature = T_BT709;
			tx_hdr->hdr_color_feature = C_BT709;
			tx_hdr->colormetry = 0;
			hdmitx_set_sdr_pkt(tx_comm);
			tx_task_mgr_queue_task(tx_comm->base.task_mgr, HDR_TASK, 0);
			return;
		}
	}

	buffer[4] = 0x8b;
	buffer[5] = 0x84;
	buffer[6] = 0x90;

	buffer[7] = ((data->application_version & 0x3) << 6) |
		((data->targeted_max_lum & 0x1f) << 1);
	buffer[8] = data->average_maxrgb;
	buffer[9] = data->distribution_values[0];
	buffer[10] = data->distribution_values[1];
	buffer[11] = data->distribution_values[2];
	buffer[12] = data->distribution_values[3];
	buffer[13] = data->distribution_values[4];
	buffer[14] = data->distribution_values[5];
	buffer[15] = data->distribution_values[6];
	buffer[16] = data->distribution_values[7];
	buffer[17] = data->distribution_values[8];
	buffer[18] = ((data->num_bezier_curve_anchors & 0xf) << 4) |
		((data->knee_point_x >> 6) & 0xf);
	buffer[19] = ((data->knee_point_x & 0x3f) << 2) |
		((data->knee_point_y >> 8) & 0x3);
	buffer[20] = data->knee_point_y  & 0xff;
	buffer[21] = data->bezier_curve_anchors[0];
	buffer[22] = data->bezier_curve_anchors[1];
	buffer[23] = data->bezier_curve_anchors[2];
	buffer[24] = data->bezier_curve_anchors[3];
	buffer[25] = data->bezier_curve_anchors[4];
	buffer[26] = data->bezier_curve_anchors[5];
	buffer[27] = data->bezier_curve_anchors[6];
	buffer[28] = data->bezier_curve_anchors[7];
	buffer[29] = data->bezier_curve_anchors[8];
	buffer[30] = ((data->graphics_overlay_flag & 0x1) << 7) |
		((data->no_delay_flag & 0x1) << 6);

	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer, NULL);
	arg = SET_AVI_BT2020;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
	if (hdmi_vic_4k_flag) {
		arg = vic & 0xff;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
	}
	meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
				TX_HDR_MODE_HDR10PLUS);
}

void hdmitx_set_vsif_pkt(void *tx_instance, enum eotf_type type,
				enum mode_type tunnel_mode,
				struct dv_vsif_para *data,
				bool signal_sdr)
{
	struct dv_vsif_para para = {0};
	unsigned char buffer1[31] = {0x81, 0x01};
	unsigned char buffer2[31] = {0x81, 0x01};
	unsigned char len = 0;
	unsigned int vic = 0;
	unsigned int hdmi_vic_4k_flag = 0;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;
	struct hdmitx_hw_common *tx_hw = NULL;
	unsigned long flags;
	enum hdmi_tf_type hdr_type = HDMI_NONE;
	u32 arg = 0;
	struct meson_tx_hdr *tx_hdr = NULL;

	if (!tx_comm || !tx_comm->hdr_state) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	tx_hdr = tx_comm->hdr_state;

	HDMITX_DEBUG_PACKET("hdr: [%s]: [%d]\n", __func__, __LINE__);
	if (!data)
		memcpy(&tx_hdr->vsif_debug_info.data, &para, sizeof(struct dv_vsif_para));
	else
		memcpy(&tx_hdr->vsif_debug_info.data, data, sizeof(struct dv_vsif_para));

	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);
	vic = tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic;
	/* if ready is 0, only can clear pkt */
	if (tx_comm->ready == 0 && type != EOTF_T_NULL) {
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	if (tx_hdr->hdmi_current_eotf_type != type ||
		tx_hdr->hdmi_current_tunnel_mode != tunnel_mode ||
		tx_hdr->hdmi_current_signal_sdr != signal_sdr) {
		tx_hdr->hdmi_current_eotf_type = type;
		tx_hdr->hdmi_current_tunnel_mode = tunnel_mode;
		tx_hdr->hdmi_current_signal_sdr = signal_sdr;
		HDMITX_INFO("hdr: [%s]: type = %d, tunnel_mode = %d, signal_sdr = %d\n",
			__func__, type, tunnel_mode, signal_sdr);
		if (type == EOTF_T_DOLBYVISION &&
				tunnel_mode == RGB_8BIT && signal_sdr != true)
			HDMITX_INFO("hdr: [%s]: SDR->MESON_DOLBYVISION Sink-led\n", __func__);
		else if (type == EOTF_T_LL_MODE &&
				tunnel_mode == YUV422_BIT12 && signal_sdr != true)
			HDMITX_INFO("hdr: [%s]: SDR->MESON_DOLBYVISION Source-led\n", __func__);
	}

	/* if DRM/HDR packet is enabled but next will output DV, then disable it */
	hdr_type = hdmitx_hw_get_hdr_st(tx_hw);
	if (hdr_type != HDMI_NONE &&
		(type == EOTF_T_DV_AHEAD ||
		type == EOTF_T_DOLBYVISION ||
		type == EOTF_T_LL_MODE)) {
		tx_hdr->hdr_transfer_feature = T_BT709;
		tx_hdr->hdr_color_feature = C_BT709;
		tx_hdr->colormetry = 0;
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
	}

	if (vic == HDMI_95_3840x2160p30_16x9 || vic == HDMI_94_3840x2160p25_16x9 ||
			vic == HDMI_93_3840x2160p24_16x9 || vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;

	/* ver0 and ver1_15 and ver1_12bit with ll= 0 use hdmi 1.4b VSIF */
	if (tx_comm->base.rxcap.dv_info.ver == 0 ||
			(tx_comm->base.rxcap.dv_info.ver == 1 &&
			 tx_comm->base.rxcap.dv_info.length == 0xE) ||
			(tx_comm->base.rxcap.dv_info.ver == 1 &&
			 tx_comm->base.rxcap.dv_info.length == 0xB &&
			 tx_comm->base.rxcap.dv_info.low_latency == 0)) {
		switch (type) {
		case EOTF_T_DOLBYVISION:
			len = 0x18;
			break;
		case EOTF_T_HDR10:
		case EOTF_T_SDR:
		case EOTF_T_NULL:
		default:
			len = 0x05;
			break;
		}

		buffer1[2] = len;
		buffer1[4] = 0x03;
		buffer1[5] = 0x0c;
		buffer1[6] = 0x00;
		buffer1[7] = 0x00;

		if (hdmi_vic_4k_flag) {
			buffer1[7] = 0x20;
			if (vic == HDMI_95_3840x2160p30_16x9)
				buffer1[8] = 0x1;
			else if (vic == HDMI_94_3840x2160p25_16x9)
				buffer1[8] = 0x2;
			else if (vic == HDMI_93_3840x2160p24_16x9)
				buffer1[8] = 0x3;
			else if (vic == HDMI_98_4096x2160p24_256x135)
				buffer1[8] = 0x4;
		}
		if (type == EOTF_T_DV_AHEAD) {
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer1, NULL);
			spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
			return;
		}
		if (type == EOTF_T_DOLBYVISION) {
			/*
			 * disable forced gaming in this mode because if we are
			 * here with forced gaming on, it means TV is not DV LL
			 * capable
			 */
			if (tx_comm->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
					(tx_comm->allm_mode == 1 ||
					 tx_comm->ct_mode == 1)) {
				HDMITX_DEBUG_PACKET("hdr: amdv H14b VSIF, disable game mode\n");
				arg = DISABLE_ALLM;
				hdmitx_hw_cntl(tx_hw, ALLM_CONFIG, (void *)&arg, NULL);
			}
			/* first disable drm package */
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer1, NULL);
			/*
			 * amdolby_vision Source System-on-Chip Platform Kit Version 2.6:
			 * 4.4.1 Expected AVI-IF for Dolby Vision output, need BT2020 for DV
			 */
			arg = SET_AVI_BT2020;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
			if (tunnel_mode == RGB_8BIT) {
				arg = HDMI_COLORSPACE_RGB;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS,
						(void *)&arg, NULL);
				arg = RGB_RANGE_FUL;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_Q01,
						(void *)&arg, NULL);
				/* to test, if needed */
				/* hdev->hwop.cntlconfig(hdev, VPU_CONFIG_CSC, CSC_Y444_8BIT); */
				/* if (log_level == 0xfd) */
					/* HDMITX_INFO("Dolby H14b VSIF, */
					/* switch to y444 csc\n"); */
				meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
						TX_HDR_MODE_DV_STD);
			} else {
				arg = HDMI_COLORSPACE_YUV422;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS,
						(void *)&arg, NULL);
				arg = YCC_RANGE_FUL;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01,
						(void *)&arg, NULL);
				meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
						TX_HDR_MODE_DV_LL);
			}
			if (hdmi_vic_4k_flag) {
				arg = vic & 0xff;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
			}
		} else {
			if (hdmi_vic_4k_flag) {
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer1, NULL);
				/* clear vic from AVI */
				arg = 0;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
				HDMITX_DEBUG_PACKET("hdr: amdv exit, send H14b VSIF with vic: %d\n",
						buffer1[8]);
			} else {
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
				HDMITX_INFO("hdr: amdv exit, disable H14b VSIF\n");
			}
			if (signal_sdr) {
				HDMITX_INFO("hdr: [%s]: switch signal to SDR\n", __func__);
				arg = tx_comm->fmt_para.cs;
				hdmitx_hw_cntl(tx_hw,
					AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = RGB_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_Q01, (void *)&arg, NULL);
				arg = YCC_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01, (void *)&arg, NULL);
				/* BT709 */
				arg = CLR_AVI_BT2020;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
				/* if TV support traditional SDR, then recover hdr.sdr packet */
				/* if (hdev->tx_comm.base.rxcap.hdr_info.hdr_support & 0x1) { */
				/* HDMITX_DEBUG_PACKET("%s: recover hdr.sdr pkt\n", __func__); */
				/* hdmi_drm_infoframe_init(&hdev->infoframe.drm.drm); */
				/* hdmi_drm_infoframe_set(&hdev->infoframe.drm.drm); */
				/* } */
				/* re-enable forced game mode if selected by the user */
				if (tx_comm->ll_user_set_mode == HDMI_LL_MODE_ENABLE) {
					HDMITX_INFO("hdr: amdv H14b VSIF OFF, enable game mode\n");
					arg = ENABLE_ALLM;
					hdmitx_hw_cntl(tx_hw, ALLM_CONFIG, (void *)&arg, NULL);
				}
				meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_SDR);
			}
		}
	}
	/* ver1_12  with low_latency = 1 and ver2 use Dolby VSIF */
	if (tx_comm->base.rxcap.dv_info.ver == 2 ||
			(tx_comm->base.rxcap.dv_info.ver == 1 &&
			 tx_comm->base.rxcap.dv_info.length == 0xB &&
			 tx_comm->base.rxcap.dv_info.low_latency == 1) ||
			type == EOTF_T_LL_MODE) {
		if (!data)
			data = &para;
		len = 0x1b;

		buffer2[2] = len;
		buffer2[4] = 0x46;
		buffer2[5] = 0xd0;
		buffer2[6] = 0x00;
		if (data->ver2_l11_flag == 1) {
			buffer2[7] = data->vers.ver2_l11.low_latency |
				     data->vers.ver2_l11.dobly_vision_signal << 1 |
				     data->vers.ver2_l11.src_dm_version << 5;
			/* L11_MD_Present */
			buffer2[8] = data->vers.ver2_l11.eff_tmax_PQ_hi
				     | data->vers.ver2_l11.auxiliary_MD_present << 6
				     | data->vers.ver2_l11.backlt_ctrl_MD_present << 7
				     | 0x20;
			buffer2[9] = data->vers.ver2_l11.eff_tmax_PQ_low;
			buffer2[10] = data->vers.ver2_l11.auxiliary_runmode;
			buffer2[11] = data->vers.ver2_l11.auxiliary_runversion;
			buffer2[12] = data->vers.ver2_l11.auxiliary_debug0;
			buffer2[13] = (data->vers.ver2_l11.content_type)
				| (data->vers.ver2_l11.content_sub_type << 4);
			buffer2[14] = (data->vers.ver2_l11.intended_white_point)
				| (data->vers.ver2_l11.crf << 4);
			buffer2[15] = data->vers.ver2_l11.l11_byte2;
			buffer2[16] = data->vers.ver2_l11.l11_byte3;
		} else {
			buffer2[7] = (data->vers.ver2.low_latency) |
				(data->vers.ver2.dobly_vision_signal << 1) |
				(data->vers.ver2.src_dm_version << 5);
			buffer2[8] = (data->vers.ver2.eff_tmax_PQ_hi)
				| (data->vers.ver2.auxiliary_MD_present << 6)
				| (data->vers.ver2.backlt_ctrl_MD_present << 7);
			buffer2[9] = data->vers.ver2.eff_tmax_PQ_low;
			buffer2[10] = data->vers.ver2.auxiliary_runmode;
			buffer2[11] = data->vers.ver2.auxiliary_runversion;
			buffer2[12] = data->vers.ver2.auxiliary_debug0;
		}
		if (type == EOTF_T_DV_AHEAD) {
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer2, NULL);
			spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
			return;
		}
		/* Dolby Vision standard case */
		if (type == EOTF_T_DOLBYVISION) {
			/*
			 * disable forced gaming in this mode because if we are
			 * here with forced gaming on, it means TV is not DV LL
			 * capable
			 */
			if (tx_comm->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
					(tx_comm->allm_mode == 1 ||
					 tx_comm->ct_mode == 1)) {
				HDMITX_DEBUG_PACKET("hdr: amdv VSIF, disable game mode\n");
				arg = DISABLE_ALLM;
				hdmitx_hw_cntl(tx_hw, ALLM_CONFIG, (void *)&arg, NULL);
			}
			/* first disable drm package */
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer2, NULL);
			/* Dolby Vision Source System-on-Chip Platform Kit Version 2.6:
			 * 4.4.1 Expected AVI-IF for Dolby Vision output, need BT2020 for DV
			 */
			arg = SET_AVI_BT2020;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
			/* RGB444 */
			if (tunnel_mode == RGB_8BIT) {
				arg = HDMI_COLORSPACE_RGB;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = RGB_RANGE_FUL;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_Q01, (void *)&arg, NULL);
				meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_DV_STD);
				/* to test, if needed */
				/* hdev->hwop.cntlconfig(hdev, VPU_CONFIG_CSC, CSC_Y444_8BIT); */
				/* if (log_level == 0xfd) */
					/*HDMITX_INFO("Dolby STD, switch to y444 csc\n");*/
			} else {
				/* YUV422 */
				arg = HDMI_COLORSPACE_YUV422;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = YCC_RANGE_FUL;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01, (void *)&arg, NULL);
			}
			if (hdmi_vic_4k_flag) {
				arg = vic & 0xff;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
			}
		/* Dolby Vision low-latency case */
		} else if (type == EOTF_T_LL_MODE) {
			/*
			 * make sure forced game mode is enabled as there could be DV std
			 * to DV LL transition during uboot to kernel transition because
			 * of game mode forced enabled by user.
			 */
			if (tx_comm->ll_user_set_mode == HDMI_LL_MODE_ENABLE &&
					tx_comm->allm_mode == 0 &&
					tx_comm->ct_mode == 0) {
				HDMITX_DEBUG_PACKET("hdr: amdv LL VSIF, enable forced game mode\n");
				arg = ENABLE_ALLM;
				hdmitx_hw_cntl(tx_hw, ALLM_CONFIG, (void *)&arg, NULL);
			}
			/* first disable drm package */
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer2, NULL);
			/* Dolby vision HDMI Signaling Case25,
			 * UCD323 not declare bt2020 colorimetry,
			 * need to forcely send BT.2020
			 */
			arg = SET_AVI_BT2020;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
			/* 10/12bit RGB444 */
			if (tunnel_mode == RGB_10_12BIT) {
				arg = HDMI_COLORSPACE_RGB;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = RGB_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_Q01, (void *)&arg, NULL);
			} else if (tunnel_mode == YUV444_10_12BIT) {
				/* 10/12bit YUV444 */
				arg = HDMI_COLORSPACE_YUV444;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = YCC_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01, (void *)&arg, NULL);
			} else {
				/* YUV422 */
				arg = HDMI_COLORSPACE_YUV422;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_CS,
						(void *)&arg, NULL);
				arg = YCC_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01,
						(void *)&arg, NULL);
			}
			if (hdmi_vic_4k_flag) {
				arg = vic & 0xff;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
			}
			meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_DV_LL);
		} else {
			/* SDR case */
			if (hdmi_vic_4k_flag) {
				/* recover HDMI1.4b_VSIF */
				buffer1[2] = 0x5;
				buffer1[4] = 0x03;
				buffer1[5] = 0x0c;
				buffer1[6] = 0x00;
				buffer1[7] = 0x20;

				if (vic == HDMI_95_3840x2160p30_16x9)
					buffer1[8] = 0x1;
				else if (vic == HDMI_94_3840x2160p25_16x9)
					buffer1[8] = 0x2;
				else if (vic == HDMI_93_3840x2160p24_16x9)
					buffer1[8] = 0x3;
				else if (vic == HDMI_98_4096x2160p24_256x135)
					buffer1[8] = 0x4;

				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer1, NULL);
				HDMITX_DEBUG_PACKET("hdr: amdv exit, send H14b VSIF with vic: %d\n",
						buffer1[8]);
				/* clear vic from AVI */
				arg = 0;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
			} else {
				HDMITX_INFO("hdr: amdv exit, amdv_signal = %d\n", buffer2[7]);
				hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer2, NULL);
			}

			if (signal_sdr) {
				HDMITX_INFO("hdr: [%s]: switch signal to SDR\n", __func__);
				arg = tx_comm->fmt_para.cs;
				hdmitx_hw_cntl(tx_hw,
					AUX_PKT_SET_AVI_CS, (void *)&arg, NULL);
				arg = RGB_RANGE_DEFAULT;
				hdmitx_hw_cntl(tx_hw,
					AUX_PKT_CONF_AVI_Q01, (void *)&arg, NULL);
				arg = YCC_RANGE_LIM;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_YQ01, (void *)&arg, NULL);
				/* BT709 */
				arg = CLR_AVI_BT2020;
				hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
				meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
					TX_HDR_MODE_SDR);
				/* if TV support traditional SDR, then recover hdr.sdr packet */
				/* if (hdev->tx_comm.base.rxcap.hdr_info.hdr_support & 0x1) { */
				/* HDMITX_DEBUG_PACKET("%s: recover hdr.sdr pkt\n", __func__); */
				/* hdmi_drm_infoframe_init(&hdev->infoframe.drm.drm); */
				/* hdmi_drm_infoframe_set(&hdev->infoframe.drm.drm); */
				/* } */
				/* re-enable forced game mode if selected by the user */
				if (tx_comm->ll_user_set_mode == HDMI_LL_MODE_ENABLE) {
					HDMITX_INFO("hdr: amdv VSIF disabled, enable game mode\n");
					arg = ENABLE_ALLM;
					hdmitx_hw_cntl(tx_hw, ALLM_CONFIG, (void *)&arg, NULL);
				}
			}
		}
	}

	/* config dither */
	arg = 0;
	hdmitx_hw_cntl(tx_hw, VPU_CONFIG_DITHER, (void *)&arg, NULL);
	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
}

void hdmitx_set_cuva_hdr_vsif(void *tx_instance, struct cuva_hdr_vsif_para *data)
{
	unsigned long flags = 0;
	unsigned char buffer[31] = {0x81, 0x01, 0x1b};
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;
	struct hdmitx_hw_common *tx_hw = NULL;
	unsigned int vic;
	unsigned int hdmi_vic_4k_flag = 0;
	u32 arg = 0;

	if (!tx_comm) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);
	vic = tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic;
	if (vic == HDMI_95_3840x2160p30_16x9 || vic == HDMI_94_3840x2160p25_16x9 ||
			vic == HDMI_93_3840x2160p24_16x9 || vic == HDMI_98_4096x2160p24_256x135)
		hdmi_vic_4k_flag = 1;
	if (!data) {
		if (hdmi_vic_4k_flag) {
			buffer[2] = 0x5;
			buffer[4] = 0x03;
			buffer[5] = 0x0c;
			buffer[6] = 0x00;
			buffer[7] = 0x20;
			if (vic == HDMI_95_3840x2160p30_16x9)
				buffer[8] = 0x1;
			else if (vic == HDMI_94_3840x2160p25_16x9)
				buffer[8] = 0x2;
			else if (vic == HDMI_93_3840x2160p24_16x9)
				buffer[8] = 0x3;
			else if (vic == HDMI_98_4096x2160p24_256x135)
				buffer[8] = 0x4;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer, NULL);
			/* clear vic from AVI */
			arg = 0;
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
			HDMITX_INFO("hdr: [%s]: recover hdmi1.4b_vsif\n", __func__);
		} else {
			hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
			HDMITX_INFO("hdr: [%s]: clear vendor infoframe\n", __func__);
		}
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}
	buffer[4] = GET_OUI_BYTE0(CUVA_IEEEOUI);
	buffer[5] = GET_OUI_BYTE1(CUVA_IEEEOUI);
	buffer[6] = GET_OUI_BYTE2(CUVA_IEEEOUI);
	buffer[7] = data->system_start_code;
	buffer[8] = (data->version_code & 0xf) << 4 | (data->monitor_mode_en & 0x1) << 3;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, buffer, NULL);
	arg = SET_AVI_BT2020;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
	if (hdmi_vic_4k_flag) {
		arg = vic & 0xff;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_AVI_VIC, (void *)&arg, NULL);
	}
	meson_tx_tracer_write_event(tx_comm->base.tx_tracer,
				TX_HDR_MODE_CUVA);
	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
}

void hdmitx_set_cuva_hdr_vs_emds(void *tx_instance, struct cuva_hdr_vs_emds_para *data)
{
	struct hdmi_cuva_emp_packet vs_emds[3];
	unsigned long flags;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;
	struct hdmitx_hw_common *tx_hw = NULL;
	struct meson_tx_hdr *tx_hdr = NULL;
	u32 arg = 0;

	if (!tx_comm || !tx_comm->hdr_state) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}
	tx_hw = tx_comm->tx_hw;
	if (!tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_hw param\n", __func__);
		return;
	}

	tx_hdr = tx_comm->hdr_state;
	memset(vs_emds, 0, sizeof(vs_emds));
	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);
	if (!data) {
		hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_EMP_CUVA, NULL, NULL);
		arg = CLR_AVI_BT2020;
		hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
		tx_hdr->cuva_dhdr_reset = false;
		spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
		return;
	}

	/* only in cuva receiver mode, set to 1 when cuva signal needs to be output */
	tx_hdr->cuva_dhdr_reset = true;

	vs_emds[0].hb[0] = 0x7f;
	vs_emds[0].hb[1] = 1 << 7;
	/* Sequence_Index */
	vs_emds[0].hb[2] = 0;
	vs_emds[0].pb[0] = (1 << 7) | (1 << 4) | (1 << 2) | (1 << 1);
	/* rsvd */
	vs_emds[0].pb[1] = 0;
	/* Organization_ID */
	vs_emds[0].pb[2] = 0;
	/* Data_Set_Tag_MSB */
	vs_emds[0].pb[3] = 0;
	/* Data_Set_Tag_LSB */
	vs_emds[0].pb[4] = 2;
	/* Data_Set_Length_MSB */
	vs_emds[0].pb[5] = 0;
	/* Data_Set_Length_LSB */
	vs_emds[0].pb[6] = 0x38;
	vs_emds[0].pb[7] = GET_OUI_BYTE0(CUVA_IEEEOUI);
	vs_emds[0].pb[8] = GET_OUI_BYTE1(CUVA_IEEEOUI);
	vs_emds[0].pb[9] = GET_OUI_BYTE2(CUVA_IEEEOUI);
	vs_emds[0].pb[10] = data->system_start_code;
	vs_emds[0].pb[11] = ((data->version_code & 0xf) << 4) |
			     ((data->min_maxrgb_pq >> 8) & 0xf);
	vs_emds[0].pb[12] = data->min_maxrgb_pq & 0xff;
	vs_emds[0].pb[13] = (data->avg_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[14] = data->avg_maxrgb_pq & 0xff;
	vs_emds[0].pb[15] = (data->var_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[16] = data->var_maxrgb_pq & 0xff;
	vs_emds[0].pb[17] = (data->max_maxrgb_pq >> 8) & 0xf;
	vs_emds[0].pb[18] = data->max_maxrgb_pq & 0xff;
	vs_emds[0].pb[19] = (data->targeted_max_lum_pq >> 8) & 0xf;
	vs_emds[0].pb[20] = data->targeted_max_lum_pq & 0xff;
	vs_emds[0].pb[21] = ((data->transfer_character & 1) << 7) |
			     ((data->base_enable_flag & 0x1) << 6) |
			     ((data->base_param_m_p >> 8) & 0x3f);
	vs_emds[0].pb[22] = data->base_param_m_p & 0xff;
	vs_emds[0].pb[23] = data->base_param_m_m & 0x3f;
	vs_emds[0].pb[24] = (data->base_param_m_a >> 8) & 0x3;
	vs_emds[0].pb[25] = data->base_param_m_a & 0xff;
	vs_emds[0].pb[26] = (data->base_param_m_b >> 8) & 0x3;
	vs_emds[0].pb[27] = data->base_param_m_b & 0xff;
	vs_emds[1].hb[0] = 0x7f;
	vs_emds[1].hb[1] = 0;
	/* Sequence_Index */
	vs_emds[1].hb[2] = 1;
	vs_emds[1].pb[0] = data->base_param_m_n & 0x3f;
	vs_emds[1].pb[1] = (((data->base_param_k[0] & 3) << 4) |
			   ((data->base_param_k[1] & 3) << 2) |
			   ((data->base_param_k[2] & 3) << 0));
	vs_emds[1].pb[2] = data->base_param_delta_enable_mode & 0x7;
	vs_emds[1].pb[3] = data->base_param_enable_delta & 0x7f;
	vs_emds[1].pb[4] = (((data->_3spline_enable_num & 0x3) << 3) |
			    ((data->_3spline_enable_flag & 1)  << 2) |
			    (data->_3spline_data[0].th_enable_mode & 0x3));
	vs_emds[1].pb[5] = data->_3spline_data[0].th_enable_mb;
	vs_emds[1].pb[6] = (data->_3spline_data[0].th_enable >> 8) & 0xf;
	vs_emds[1].pb[7] = data->_3spline_data[0].th_enable & 0xff;
	vs_emds[1].pb[8] =
		(data->_3spline_data[0].th_enable_delta[0] >> 8) & 0x3;
	vs_emds[1].pb[9] = data->_3spline_data[0].th_enable_delta[0] & 0xff;
	vs_emds[1].pb[10] =
		(data->_3spline_data[0].th_enable_delta[1] >> 8) & 0x3;
	vs_emds[1].pb[11] = data->_3spline_data[0].th_enable_delta[1] & 0xff;
	vs_emds[1].pb[12] = data->_3spline_data[0].enable_strength;
	vs_emds[1].pb[13] = data->_3spline_data[1].th_enable_mode & 0x3;
	vs_emds[1].pb[14] = data->_3spline_data[1].th_enable_mb;
	vs_emds[1].pb[15] = (data->_3spline_data[1].th_enable >> 8) & 0xf;
	vs_emds[1].pb[16] = data->_3spline_data[1].th_enable & 0xff;
	vs_emds[1].pb[17] =
		(data->_3spline_data[1].th_enable_delta[0] >> 8) & 0x3;
	vs_emds[1].pb[18] = data->_3spline_data[1].th_enable_delta[0] & 0xff;
	vs_emds[1].pb[19] =
		(data->_3spline_data[1].th_enable_delta[1] >> 8) & 0x3;
	vs_emds[1].pb[20] = data->_3spline_data[1].th_enable_delta[1] & 0xff;
	vs_emds[1].pb[21] = data->_3spline_data[1].enable_strength;
	vs_emds[1].pb[22] = data->color_saturation_num;
	vs_emds[1].pb[23] = data->color_saturation_gain[0];
	vs_emds[1].pb[24] = data->color_saturation_gain[1];
	vs_emds[1].pb[25] = data->color_saturation_gain[2];
	vs_emds[1].pb[26] = data->color_saturation_gain[3];
	vs_emds[1].pb[27] = data->color_saturation_gain[4];
	vs_emds[2].hb[0] = 0x7f;
	vs_emds[2].hb[1] = (1 << 6);
	/* Sequence_Index */
	vs_emds[2].hb[2] = 2;
	vs_emds[2].pb[0] = data->color_saturation_gain[5];
	vs_emds[2].pb[1] = data->color_saturation_gain[6];
	vs_emds[2].pb[2] = data->color_saturation_gain[7];
	vs_emds[2].pb[3] = data->graphic_src_display_value;
	/* Reserved */
	vs_emds[2].pb[4] = 0;
	vs_emds[2].pb[5] = data->max_display_mastering_lum >> 8;
	vs_emds[2].pb[6] = data->max_display_mastering_lum & 0xff;

	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_EMP_CUVA, (u8 *)&vs_emds, NULL);
	arg = SET_AVI_BT2020;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);
}

void hdmitx_set_sbtm_pkt(void *tx_instance, struct vtem_sbtm_st *data)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)tx_instance;

	if (!tx_comm || !tx_comm->tx_hw) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	hdmitx_hw_cntl(tx_comm->tx_hw, AUX_PKT_SET_EMP_SBTM, data, NULL);
}

void hdmitx_sync_input_vpp_info(void *tx_instance)
{
	struct hdmitx_common *tx_comm = NULL;
	struct vinfo_s *info = NULL;
	struct rx_cap *prxcap = NULL;
	enum hdmi_colorspace cs = HDMI_COLORSPACE_RGB;
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	struct meson_tx_hdr *tx_hdr = NULL;
	u32 data = 0;

	if (!tx_instance) {
		HDMITX_ERROR("[%s]: null tx_instance param\n", __func__);
		return;
	}

	tx_comm = (struct hdmitx_common *)tx_instance;
	info = hdmitx_get_current_vinfo(tx_comm);
	if (!info)
		return;

	cs = tx_comm->fmt_para.cs;
	vic = tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic;
	prxcap = &tx_comm->base.rxcap;
	tx_hdr = tx_comm->hdr_state;

	/* DSC and YUV mode does not require CSC */
	if (tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en || cs != HDMI_COLORSPACE_RGB) {
		data = 0;
		hdmitx_hw_cntl(tx_comm->tx_hw, CORE_MISC_VP_CMS_CSC, &data, NULL);
		return;
	}

	if (tx_hdr->in_colorimetry != info->vpp_post_out_colorimetry ||
			tx_hdr->in_color_range != info->vpp_post_out_range ||
			tx_hdr->in_color_fmt != info->vpp_post_out_color_fmt) {
		tx_hdr->in_colorimetry = info->vpp_post_out_colorimetry;
		tx_hdr->in_color_range = info->vpp_post_out_range;
		tx_hdr->in_color_fmt = info->vpp_post_out_color_fmt;
		HDMITX_INFO("%s: vpp input: color_range = %d, colorimetry = %d\n",
				__func__, tx_hdr->in_color_range, tx_hdr->in_colorimetry);

		if (tx_hdr->in_colorimetry == HDMI_709_COLORIMETRY) {
			tx_hdr->out_colorimetry = HDMI_709_COLORIMETRY;
		} else if (tx_hdr->in_colorimetry == HDMI_601_COLORIMETRY) {
			tx_hdr->out_colorimetry = HDMI_601_COLORIMETRY;
		} else if (tx_hdr->in_colorimetry == HDMI_2020_COLORIMETRY) {
			/* check if this TV supports BT2020 */
			if (prxcap->colorimetry_data & 0xc0)
				tx_hdr->out_colorimetry = HDMI_2020_COLORIMETRY;
			else
				/* default is 709 color space */
				tx_hdr->out_colorimetry = HDMI_709_COLORIMETRY;
		} else if (tx_hdr->in_colorimetry == HDMI_2020C_COLORIMETRY) {
			/* check if this TV supports BT2020C */
			if (prxcap->colorimetry_data & 0x20)
				tx_hdr->out_colorimetry = HDMI_2020C_COLORIMETRY;
			else
				/* default is 709 color space */
				tx_hdr->out_colorimetry = HDMI_709_COLORIMETRY;
		}
		/* The default IT mode of BT2020RGB is full range */
		if (vic <= HDMI_1_640x480p60_4x3 &&
				(prxcap->video_capability_data & 0x40))
			tx_hdr->out_color_range = HDMI_FULL_COLOR_RANGE;
		else
			tx_hdr->out_color_range = info->vpp_post_out_range;

		HDMITX_INFO("%s: hdmi output: color_range = %d, colorimetry = %d\n",
				__func__, tx_hdr->out_color_range, tx_hdr->out_colorimetry);

		switch (tx_comm->tx_hw->chip_data->chip_type) {
		case MESON_CPU_ID_S5:
		case MESON_CPU_ID_T7:
			if (cs == HDMI_COLORSPACE_RGB) {
				data = (tx_hdr->out_colorimetry << 2) |
						(tx_hdr->out_color_range << 4) | (1 << 5) |
						(tx_hdr->in_colorimetry << 6) |
						(tx_hdr->in_color_range << 8) |
						(tx_hdr->in_color_fmt << 9);
				hdmitx_hw_cntl(tx_comm->tx_hw, CORE_MISC_VP_CMS_CSC,
					(void *)&data, NULL);
			}
			break;
		default:
			break;
		}
	}
}

static struct tx_task_info hdr_task_infos[] = {
	{
		.name = "hdr_mute",
		.fn = hdr_work_func,
		.type = HDR_TASK,
		.queue_type = TASK_QUEUE_SYSTEM,
		.flag = TASK_FLAG_DELAY_WORK,
		.queue_name = "hdmitx_low",
		.queue_flag = WQ_FREEZABLE,
	},
	{
		.name = "hdr_unmute",
		.fn = hdr_unmute_work_func,
		.type = HDR_UNMUTE,
		.queue_type = TASK_QUEUE_SYSTEM,
		.flag = TASK_FLAG_DELAY_WORK,
		.queue_name = "hdmitx_low",
		.queue_flag = WQ_FREEZABLE,
	},
};

struct meson_tx_hdr *meson_tx_hdr_create(struct hdmitx_common *tx_comm)
{
	struct meson_tx_hdr *tx_hdr = NULL;
	int i;

	if (!tx_comm)
		return NULL;

	tx_hdr = kzalloc(sizeof(*tx_hdr), GFP_KERNEL);

	/* init hdr para */
	tx_hdr->hdr_transfer_feature = T_UNKNOWN;
	tx_hdr->hdr_color_feature = C_UNKNOWN;
	tx_hdr->colormetry = 0;
	tx_hdr->vid_mute_op = VIDEO_NONE_OP;
	tx_hdr->hdr_mute_frame = 20;
	/* init hdr10plus flag */
	tx_hdr->hdr10plus_feature = 0;
	tx_hdr->all_zero_hdr10plus_pkt = false;
	/* init amdv para */
	tx_hdr->hdmi_current_eotf_type = EOTF_T_NULL;
	tx_hdr->hdmi_current_tunnel_mode = YUV422_BIT12;
	tx_hdr->hdmi_current_signal_sdr = 0;

	for (i = 0; i < ARRAY_SIZE(hdr_task_infos); i++)
		tx_task_mgr_setup_task(tx_comm->base.task_mgr, &hdr_task_infos[i], tx_comm);

	/* reset hdmitx csc para */
	tx_hdr->in_colorimetry = 0xff;
	tx_hdr->out_colorimetry = 0xff;
	tx_hdr->in_color_range = 0xff;
	tx_hdr->out_color_range = 0xff;
	tx_hdr->in_color_fmt = 0xff;

	tx_hdr->bind_instance = tx_comm;

	return tx_hdr;
}

int meson_tx_hdr_destroy(struct meson_tx_hdr *tx_hdr)
{
	if (!tx_hdr)
		return 0;

	kfree(tx_hdr);
	return 0;
}

int meson_tx_hdr_reset(struct meson_tx_hdr *tx_hdr)
{
	if (!tx_hdr)
		return 0;

	tx_hdr->hdr_transfer_feature = T_UNKNOWN;
	tx_hdr->hdr_color_feature = C_UNKNOWN;
	tx_hdr->colormetry = 0;
	tx_hdr->hdmi_current_hdr_mode = 0;
	tx_hdr->hdmi_last_hdr_mode = 0;
	tx_hdr->hdr10plus_feature = 0;
	tx_hdr->cuva_dhdr_reset = false;

	/* reset hdmitx csc para */
	tx_hdr->in_colorimetry = 0xff;
	tx_hdr->out_colorimetry = 0xff;
	tx_hdr->in_color_range = 0xff;
	tx_hdr->out_color_range = 0xff;
	tx_hdr->in_color_fmt = 0xff;
	return 0;
}

int meson_tx_hdr_disable(struct meson_tx_hdr *tx_hdr)
{
	struct hdmitx_hw_common *tx_hw = NULL;
	struct hdmitx_common *tx_comm = NULL;
	unsigned long flags;
	u32 arg = 0;

	if (!tx_hdr || !tx_hdr->bind_instance)
		return 0;

	tx_comm = tx_hdr->bind_instance;
	tx_hw = tx_comm->tx_hw;
	spin_lock_irqsave(&tx_comm->edid_spinlock, flags);

	HDMITX_INFO("hdr: clear all hdmitx infoframe\n");
	/* step1 HW: clear packets */
	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_DRM, NULL, NULL);
	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF1, NULL, NULL);
	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_VSIF2, NULL, NULL);
	arg = CLR_AVI_BT2020;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CLR_AVI, NULL, NULL);
	hdmitx_hw_cntl(tx_hw, AUX_PKT_SET_EMP_CUVA, NULL, NULL);
	/* step2 SW: reset para */
	meson_tx_hdr_reset(tx_comm->hdr_state);

	spin_unlock_irqrestore(&tx_comm->edid_spinlock, flags);

	return 0;
}

int meson_tx_hdr_dump(struct meson_tx_hdr *tx_hdr, struct seq_file *s)
{
	struct master_display_info_s *drm_pkt_cfg;
	struct hdr10plus_para *hdr10p_cfg;
	struct vsif_debug_save *vsif_debug_cfg;
	enum hdmi_hdr_transfer hdr_transfer_feature;
	enum hdmi_hdr_color hdr_color_feature;
	struct dv_vsif_para *data;
	u32 colormetry;
	u32 hcnt, vcnt;
	u8 *tmp;

	if (!tx_hdr || !s)
		return 0;

	seq_puts(s, "***drm_config_data***\n");
	drm_pkt_cfg = &tx_hdr->drm_config_data;
	hdr_transfer_feature = (drm_pkt_cfg->features >> 8) & 0xff;
	hdr_color_feature = (drm_pkt_cfg->features >> 16) & 0xff;
	colormetry = (drm_pkt_cfg->features >> 30) & 0x1;
	seq_printf(s, "tf=%u, cf=%u, colormetry=%u\n", hdr_transfer_feature, hdr_color_feature,
		colormetry);
	seq_puts(s, "primaries:\n");
	for (vcnt = 0; vcnt < 3; vcnt++) {
		for (hcnt = 0; hcnt < 2; hcnt++)
			seq_printf(s, "%u, ",
			drm_pkt_cfg->primaries[vcnt][hcnt]);
		seq_puts(s, "\n");
	}
	seq_puts(s, "white_point: ");
	for (hcnt = 0; hcnt < 2; hcnt++)
		seq_printf(s, "%u, ", drm_pkt_cfg->white_point[hcnt]);
	seq_puts(s, "\n");
	seq_puts(s, "luminance: ");
	for (hcnt = 0; hcnt < 2; hcnt++)
		seq_printf(s, "%u, ",
		drm_pkt_cfg->luminance[hcnt]);
	seq_puts(s, "\n");
	seq_printf(s, "max_content: %u, ",
		drm_pkt_cfg->max_content);
	seq_printf(s, "max_frame_average: %u\n",
		drm_pkt_cfg->max_frame_average);
	seq_puts(s, "\n");

	seq_puts(s, "***hdr10p_config_data***\n");
	hdr10p_cfg = &tx_hdr->hdr10p_config_data;
	seq_printf(s, "appver: %u, tlum: %u, avgrgb: %u\n",
		hdr10p_cfg->application_version,
		hdr10p_cfg->targeted_max_lum,
		hdr10p_cfg->average_maxrgb);
	tmp = hdr10p_cfg->distribution_values;
	seq_puts(s, "distribution_values:\n");
	for (vcnt = 0; vcnt < 3; vcnt++) {
		for (hcnt = 0; hcnt < 3; hcnt++)
			seq_printf(s, "%u, ", tmp[vcnt * 3 + hcnt]);
		seq_puts(s, "\n");
	}
	seq_printf(s, "nbca: %u, knpx: %u, knpy: %u\n",
		hdr10p_cfg->num_bezier_curve_anchors,
		hdr10p_cfg->knee_point_x,
		hdr10p_cfg->knee_point_y);
	tmp = hdr10p_cfg->bezier_curve_anchors;
	seq_puts(s, "bezier_curve_anchors:\n");
	for (vcnt = 0; vcnt < 3; vcnt++) {
		for (hcnt = 0; hcnt < 3; hcnt++)
			seq_printf(s, "%u, ", tmp[vcnt * 3 + hcnt]);
		seq_puts(s, "\n");
	}
	seq_printf(s, "gof: %u, ndf: %u\n",
		hdr10p_cfg->graphics_overlay_flag,
		hdr10p_cfg->no_delay_flag);
	seq_puts(s, "\n");
	seq_puts(s, "******dv_vsif_info******\n");
	vsif_debug_cfg = &tx_hdr->vsif_debug_info;
	data = &vsif_debug_cfg->data;
	seq_printf(s, "type: %u, tunnel: %u, sigsdr: %u\n",
		vsif_debug_cfg->type,
		vsif_debug_cfg->tunnel_mode,
		vsif_debug_cfg->signal_sdr);
	seq_puts(s, "dv_vsif_para:\n");
	seq_printf(s, "ver: %u len: %u\n", data->ver, data->length);
	seq_printf(s, "ll: %u dvsig: %u\n", data->vers.ver2.low_latency,
		data->vers.ver2.dobly_vision_signal);
	seq_printf(s, "bcMD: %u axMD: %u\n", data->vers.ver2.backlt_ctrl_MD_present,
		data->vers.ver2.auxiliary_MD_present);
	seq_printf(s, "PQhi: %u PQlow: %u\n", data->vers.ver2.eff_tmax_PQ_hi,
		data->vers.ver2.eff_tmax_PQ_low);
	seq_printf(s, "auxiliary_runmode: %u, auxiliary_runversion: %u, ",
		data->vers.ver2.auxiliary_runmode,
		data->vers.ver2.auxiliary_runversion);
	seq_printf(s, "axdbg: %u\n", data->vers.ver2.auxiliary_debug0);
	seq_puts(s, "\n");

	return 0;
}

