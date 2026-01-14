// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_eq.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_drv_ext.h"
#include "hdmi_rx_diag.h"

//user space load edid data
static unsigned char load_edid[512] = {0x0};
struct drm_infoframe_st over_eotf_pkg;
struct hdmirx_sleep_s sleep_info = {
	.port = 0,
	.mode = HDMIRX_WAKEUP_MODE,
};

static const char *color_format_str[4] = {
	[HDMIRX_AVI_CSC_RGB]                   = "RGB",
	[HDMIRX_AVI_CSC_YCBCR422]              = "YCBCR422",
	[HDMIRX_AVI_CSC_YCBCR444]              = "YCBCR444",
	[HDMIRX_AVI_CSC_YCBCR420]              = "YCBCR420",
};

static const char *const aud_format_str[] = {
	"REFER_TO_HEADER",
	"LPCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"ONE_BIT_AUDIO",
	"DDP",
	"DTS_HD",
	"MAT",
	"DST",
	"WMA_PRO"
};

static const char *hdr_type_str[HDMIRX_HDR_MODE_MAX] = {
	[HDMIRX_HDR_MODE_SDR]               = "SDR",
	[HDMIRX_HDR_MODE_AMDV]              = "DV",
	[HDMIRX_HDR_MODE_HDR10]             = "HDR10",
	[HDMIRX_HDR_MODE_HLG]              = "HLG",
	[HDMIRX_HDR_MODE_TECHNICOLOR]      = "TECHNICOLOR",
	[HDMIRX_HDR_MODE_HDREFFECT]      = "HDREFFECT",
};

static const char *vrr_type_str[HDMIRX_VRR_TYPE_MAX] = {
	[HDMIRX_VRR_TYPE_NONE]     = "none",
	[HDMIRX_VRR_TYPE_GAMING]   = "Gaming",
	[HDMIRX_VRR_TYPE_QMS]      = "QMS",
	[HDMIRX_VRR_TYPE_QMS_PLUS] = "QMS Plus",
};

static const char *emp_type_str[8] = {
	[EMP_NULL]     = " ",
	[EMP_VTEM_CLASS0]   = "VTEM_CLASS0",
	[EMP_VTEM_CLASS1]      = "VTEM_CLASS1",
	[EMP_SBTM] = "SBTM",
	[EMP_AMDV] = "AMDV",
	[EMP_CUVA] = "CUVA",
	[EMP_CVTEM] = "CVTEM",
	[EMP_QMS_PLUS] = "QMS Plus",
};

static const char *qms_plus_type_str[5] = {
	[QMS_PLUS_DISABLE]     = "disable",
	[QMS_PLUS_VSIF]   = "VSIF",
	[QMS_PLUS_EMP]      = "EMP",
	[QMS_PLUS_SCDC] = "SCDC",
	[QMS_PLUS_VTEM] = "VTEM",
};

static const char *hdcp_version_str[3] = {
	[HDMIRX_HDCP_VERSION_14]     = "hdcp1.4",
	[HDMIRX_HDCP_VERSION_22]   = "hdcp2.2",
	[HDMIRX_HDCP_VERSION_RESERVED]  = "none",
};

static const char *avi_bar_info_str[4] = {
	[HDMIRX_AVI_BAR_INFO_INVALID]     = "INVALID",
	[HDMIRX_AVI_BAR_INFO_VERTICALVALID]   = "VERTICALVALID",
	[HDMIRX_AVI_BAR_INFO_HORIZVALID]      = "HORIZVALID",
	[HDMIRX_AVI_BAR_INFO_VERTHORIZVALID] = "VERTHORIZVALID",
};

static const char *avi_scaling_str[4] = {
	[HDMIRX_AVI_SCALING_NOSCALING]     = "NOSCALING",
	[HDMIRX_AVI_SCALING_HSCALING]   = "HSCALING",
	[HDMIRX_AVI_SCALING_VSCALING]      = "VSCALING",
	[HDMIRX_AVI_SCALING_HVSCALING] = "HVSCALING",
};

static const char *avi_scan_info_str[4] = {
	[HDMIRX_AVI_SCAN_INFO_NODATA]     = "NODATA",
	[HDMIRX_AVI_SCAN_INFO_OVERSCANNED]   = "OVERSCANNED",
	[HDMIRX_AVI_SCAN_INFO_UNDERSCANNED]      = "UNDERSCANNED",
	[HDMIRX_AVI_SCAN_INFO_FUTURE] = "FUTURE",
};

static const char *avi_colorimetry_str[4] = {
	[HDMIRX_AVI_COLORIMETRY_NODATA]     = "NODATA",
	[HDMIRX_AVI_COLORIMETRY_SMPTE170]   = "SMPTE170",
	[HDMIRX_AVI_COLORIMETRY_ITU709]      = "ITU709",
	[HDMIRX_AVI_COLORIMETRY_FUTURE] = "FUTURE",
};

static const char *avi_picture_ar_str[4] = {
	[HDMIRX_AVI_COLORIMETRY_NODATA]     = "NODATA",
	[HDMIRX_AVI_PICTURE_ARC_4_3]   = "4:3",
	[HDMIRX_AVI_PICTURE_ARC_16_9]      = "16:9",
	[HDMIRX_AVI_PICTURE_ARC_FUTURE] = "FUTURE",
};

static const char *avi_active_format_str[12] = {
	[HDMIRX_AVI_ACTIVE_FORMAT_ARC_OTHER]     = "OTHER",
	[HDMIRX_AVI_ACTIVE_FORMAT_ARC_PICTURE]   = "PICTURE",
	[HDMIRX_AVI_ACTIVE_FORMAT_ARC_4_3CENTER]      = "4:3 CENTER",
	[HDMIRX_AVI_ACTIVE_FORMAT_ARC_16_9CENTER] = "16:9 CENTER",
	[HDMIRX_AVI_ACTIVE_FORMAT_ARC_14_9CENTER] = "14:9 CENTER",
};

static const char *avi_content_type_str[4] = {
	[HDMIRX_AVI_CONTENT_TYPE_GRAPHICS]     = "GRAPHICS",
	[HDMIRX_AVI_CONTENT_TYPE_PHOTO]   = "PHOTO",
	[HDMIRX_AVI_CONTENT_TYPE_CINEMA]      = "CINEMA",
	[HDMIRX_AVI_CONTENT_TYPE_GAME] = "GAME",
};

static const char *avi_ext_colorimetry_str[8] = {
	[HDMIRX_AVI_EXT_COLORIMETRY_XVYCC601]     = "XVYCC601",
	[HDMIRX_AVI_EXT_COLORIMETRY_XVYCC709]   = "XVYCC709",
	[HDMIRX_AVI_EXT_COLORIMETRY_SYCC601]      = "SYCC601",
	[HDMIRX_AVI_EXT_COLORIMETRY_ADOBEYCC601] = "ADOBEYCC601",
	[HDMIRX_AVI_EXT_COLORIMETRY_ADOBERGB]     = "ADOBERGB",
	[HDMIRX_AVI_EXT_COLORIMETRY_BT2020_YCCBCCRC]   = "BT2020_YCCBCCRC",
	[HDMIRX_AVI_EXT_COLORIMETRY_BT2020_RGBORYCBCR]      = "BT2020_RGBORYCBCR",
	[HDMIRX_AVI_EXT_COLORIMETRY_XVRESERED] = "XVRESERED",
};

static const char *avi_rgb_quantization_range_str[4] = {
	[HDMIRX_AVI_RGB_QUANTIZATION_RANGE_DEFAULT]     = "DEFAULT",
	[HDMIRX_AVI_RGB_QUANTIZATION_RANGE_LIMITEDRANGE]   = "LIMITED",
	[HDMIRX_AVI_RGB_QUANTIZATION_RANGE_FULLRANGE]      = "FULL",
	[HDMIRX_AVI_RGB_QUANTIZATION_RANGE_RESERVED] = "RESERVED",
};

static const char *avi_ycc_quantization_range_str[3] = {
	[HDMIRX_AVI_YCC_QUANTIZATION_RANGE_LIMITEDRANGE]     = "LIMITED",
	[HDMIRX_AVI_YCC_QUANTIZATION_RANGE_FULLRANGE]   = "FULL",
	[HDMIRX_AVI_YCC_QUANTIZATION_RANGE_RESERVED]      = "RESERVED",
};

static enum hdmirx_color_depth_e get_hdmirx_color_depth(u8 color_depth)
{
	u8 ret = 0;

	switch (color_depth) {
	case E_COLORDEPTH_8:
		ret = HDMIRX_COLOR_DEPTH_8BIT;
		break;
	case E_COLORDEPTH_10:
		ret = HDMIRX_COLOR_DEPTH_10BIT;
		break;
	case E_COLORDEPTH_12:
		ret = HDMIRX_COLOR_DEPTH_12BIT;
		break;
	case E_COLORDEPTH_16:
		ret = HDMIRX_COLOR_DEPTH_16BIT;
		break;
	default:
		ret = HDMIRX_COLOR_DEPTH_RESERVED;
		break;
	}

	return ret;
}

void get_hdmirx_timing_info(struct hdmirx_timing_info_s *info)
{
	if (!info)
		return;

	info->h_freq = rx[rx_info.main_port].cur.vtotal * rx[rx_info.main_port].cur.frame_rate;
	info->v_vreq = rx[rx_info.main_port].cur.frame_rate;
	info->h_total = rx[rx_info.main_port].cur.htotal;
	info->v_total = rx[rx_info.main_port].cur.vtotal;
	info->h_porch = rx[rx_info.main_port].cur.htotal - rx[rx_info.main_port].cur.hactive;
	info->v_porch = rx[rx_info.main_port].cur.vtotal - rx[rx_info.main_port].cur.vactive;
	info->active.x = 0;//need to check
	info->active.y = 0;//need to check
	info->active.w = rx[rx_info.main_port].cur.hactive;
	info->active.h = rx[rx_info.main_port].cur.vactive;
	info->scan_type = rx[rx_info.main_port].cur.interlaced ? 0 : 1;
	if (rx[rx_info.main_port].cur.hw_dvi)
		info->dvi_hdmi = HDMIRX_MODE_DVI;
	else
		info->dvi_hdmi = HDMIRX_MODE_HDMI;
	info->color_depth = get_hdmirx_color_depth(rx[rx_info.main_port].cur.colordepth);
	info->allm_mode = rx[rx_info.main_port].vs_info_details.dv_allm |
		rx[rx_info.main_port].vs_info_details.hdmi_allm;
	if (log_level & DBG1_LOG) {
		rx_pr("hfreq:%d, vfreq:%d, htotal:%d, vtotal:%d, hactive:%d, vactive:%d\n",
			info->h_freq, info->v_vreq, info->h_total, info->v_total,
			info->active.w, info->active.h);
		rx_pr("dvi:%d, color_depth:%d, allm:%d\n",
			info->dvi_hdmi, info->color_depth, info->allm_mode);
	}
}

void get_hdmirx_drm_info(struct hdmirx_drm_info_s *info)
{
	if (!info)
		return;

	struct drm_infoframe_st drm_pkt;

	memcpy(&drm_pkt, &rx_pkt[rx_info.main_port].drm_info, sizeof(struct drm_infoframe_st));
	if (log_level & DBG1_LOG)
		rx_pr("port:%d over_eotf:%d\n", info->port, rx[rx_info.main_port].var.over_eotf);

	if (rx[rx_info.main_port].var.over_eotf) {
		info->version   = over_eotf_pkg.version;
		info->eotf_type = over_eotf_pkg.des_u.tp1.eotf;
	} else {
		info->version   = drm_pkt.version;
		info->eotf_type = drm_pkt.des_u.tp1.eotf;
	}

	info->length  = drm_pkt.length;
	info->meta_desc = drm_pkt.des_u.tp1.meta_des_id;
	info->display_primaries_x0 = drm_pkt.des_u.tp1.dis_pri_x0;
	info->display_primaries_x1 = drm_pkt.des_u.tp1.dis_pri_x1;
	info->display_primaries_x2 = drm_pkt.des_u.tp1.dis_pri_x2;
	info->display_primaries_y0 = drm_pkt.des_u.tp1.dis_pri_y0;
	info->display_primaries_y1 = drm_pkt.des_u.tp1.dis_pri_y1;
	info->display_primaries_y2 = drm_pkt.des_u.tp1.dis_pri_y2;
	info->white_point_x = drm_pkt.des_u.tp1.white_points_x;
	info->white_point_y = drm_pkt.des_u.tp1.white_points_y;
	info->max_display_mastering_luminance = drm_pkt.des_u.tp1.max_dislum;
	info->min_display_mastering_luminance = drm_pkt.des_u.tp1.min_dislum;
	info->maximum_content_light_level = drm_pkt.des_u.tp1.max_light_lvl;
	info->maximum_frame_average_light_level = drm_pkt.des_u.tp1.max_fa_light_lvl;
	if (log_level & DBG1_LOG)
		rx_pr("len:%d, eotf:%d\n", info->length, info->eotf_type);
}

static void rx_print_pkt(struct hdmirx_in_packet_s pkt)
{
	int i = 0;

	rx_pr("type:0x%x\n", pkt.type);
	rx_pr("version:0x%x\n", pkt.version);
	rx_pr("len:%d\n", pkt.length);
	for (i = 0; i < HDMIRX_PACKET_DATA_LENGTH; i++)
		rx_pr("data[%d]:0x%x\n", i, pkt.data_bytes[i]);
}

void get_hdmirx_vsi_info(struct hdmirx_vsi_info_s *info)
{
	if (!info)
		return;

	struct vsi_infoframe_st vsi_pkt;
	u8 i;

	memcpy(&vsi_pkt, &rx_pkt[rx_info.main_port].vs_info, sizeof(struct vsi_infoframe_st));
	info->video_format = (vsi_pkt.sbpkt.vsi_st.data[0]) >> 5;
	info->st_3d = (vsi_pkt.sbpkt.vsi_st.data[1]) >> 4;
	info->ext_data_3d = (vsi_pkt.sbpkt.vsi_st.data[2]) >> 4;
	info->vic = vsi_pkt.sbpkt.vsi_st.data[1];
	info->ieee = vsi_pkt.ieee;
	info->regid[0] = vsi_pkt.ieee & 0xff;
	info->regid[1] = (vsi_pkt.ieee & 0xff00) >> 8;
	info->regid[2] = (vsi_pkt.ieee & 0xff0000) >> 16;
	for (i = 0; i < 24; i++) {//need to check payload
		info->payload[i] = vsi_pkt.sbpkt.vsi_st.data[i];
	}
	//info->packet.type = vsi_pkt.pkttype;
	info->packet.type = 0x81;
	info->packet.version = vsi_pkt.ver_st.version;//need to check chgbit
	info->packet.length = vsi_pkt.length;
	info->packet.data_bytes[0] = vsi_pkt.checksum;
	info->packet.data_bytes[1] = vsi_pkt.ieee & 0xff;
	info->packet.data_bytes[2] = (vsi_pkt.ieee & 0xff00) >> 8;
	info->packet.data_bytes[3] = (vsi_pkt.ieee & 0xff0000) >> 16;
	for (i = 4; i < 28; i++)
		info->packet.data_bytes[i] = vsi_pkt.sbpkt.vsi_st.data[i - 4];
	info->packet_status = rx[rx_info.main_port].vs_info_details.pkt_status;
	if (log_level & DBG1_LOG) {
		rx_pr("fmt:0x%x, vic:%d, ieee:0x%x, pkt_status:%d\n",
			info->video_format, info->vic, vsi_pkt.ieee, info->packet_status);
		rx_print_pkt(info->packet);
	}
}

void get_hdmirx_spd_info(struct hdmirx_spd_info_s *info)
{
	if (!info)
		return;

	struct pd_infoframe_s pkt_info;
	struct spd_infoframe_st spd_pkt;
	u8 i;

	memset(&pkt_info, 0, sizeof(pkt_info));
	rx_pkt_get_spd_ex(&pkt_info, rx_info.main_port);
	memcpy(&spd_pkt, &pkt_info,
		   sizeof(struct spd_infoframe_st));
	for (i = 0; i < 8; i++)
		info->vendor_name[i] = spd_pkt.des_u.spddata.vendor_name[i];
	for (i = 0; i < 16; i++)
		info->product_description[i] = spd_pkt.des_u.spddata.product_des[i];
	info->source_device_info = spd_pkt.des_u.spddata.source_info;
	//info->packet.type = spd_pkt.pkttype;
	info->packet.type = 0x83;
	info->packet.version = spd_pkt.version;
	info->packet.length = spd_pkt.length;
	info->packet.data_bytes[0] = spd_pkt.checksum;
	for (i = 1; i < 9; i++)
		info->packet.data_bytes[i] = spd_pkt.des_u.spddata.vendor_name[i - 1];
	for (i = 9; i < 25; i++)
		info->packet.data_bytes[i] = spd_pkt.des_u.spddata.product_des[i - 9];
	info->packet.data_bytes[25] = spd_pkt.des_u.spddata.source_info;
	info->packet_status = rx[rx_info.main_port].spd_pkt_st;
	if (log_level & DBG1_LOG) {
		rx_pr("vendor:%s, device_info:0x%x, status:%d\n",
			info->vendor_name, info->source_device_info, info->packet_status);
		rx_print_pkt(info->packet);
	}
}

void get_hdmirx_avi_info(struct hdmirx_avi_info_s *info)
{
	if (!info)
		return;

	struct avi_infoframe_st avi_pkt;

	rx_pkt_get_avi_ex(&avi_pkt, rx_info.main_port);
	if (rx[rx_info.main_port].cur.hw_dvi)
		info->mode = HDMIRX_MODE_DVI;
	else
		info->mode = HDMIRX_MODE_HDMI;
	info->pixel_encoding = avi_pkt.cont.v4.colorindicator;
	info->active_info = avi_pkt.cont.v4.activeinfo;
	info->bar_info = avi_pkt.cont.v4.barinfo;
	info->scan_info = avi_pkt.cont.v4.scaninfo;
	info->colorimetry = avi_pkt.cont.v4.colorimetry;
	info->picture_aspect_ratio = avi_pkt.cont.v4.pic_ration;
	info->active_format_aspect_ratio = avi_pkt.cont.v4.fmt_ration;
	info->scaling = avi_pkt.cont.v4.pic_scaling;
	info->it_content = avi_pkt.cont.v4.it_content;
	info->extended_colorimetry = avi_pkt.cont.v4.ext_color;
	info->rgb_quantization_range = avi_pkt.cont.v4.qt_range;
	info->ycc_quantization_range = avi_pkt.cont.v4.ycc_range;
	info->content_type = avi_pkt.cont.v4.content_type;
	info->additional_colorimetry = avi_pkt.additional_colorimetry;
	info->vic = avi_pkt.cont.v4.vic;
	info->pixel_repeat = avi_pkt.cont.v4.pix_repeat;
	info->top_bar_end_line_number =	avi_pkt.line_num_end_topbar;
	info->bottom_bar_start_line_number = avi_pkt.line_num_start_btmbar;
	info->left_bar_end_pixel_number = avi_pkt.pix_num_left_bar;
	info->right_bar_end_pixel_number = avi_pkt.pix_num_right_bar;
	info->packet.type = avi_pkt.pkttype;
	info->packet.version = avi_pkt.version;
	info->packet.length = avi_pkt.length;
	info->packet.data_bytes[0] = avi_pkt.checksum;
	info->packet_status = rx[rx_info.main_port].avi_pkt_st;
	info->packet.data_bytes[1] = hdmirx_rd_cor(AVIRX_DBYTE1_DP2_IVCRX, rx_info.main_port);
	info->packet.data_bytes[2] = hdmirx_rd_cor(AVIRX_DBYTE2_DP2_IVCRX, rx_info.main_port);
	info->packet.data_bytes[3] = hdmirx_rd_cor(AVIRX_DBYTE3_DP2_IVCRX, rx_info.main_port);
	info->packet.data_bytes[4] = hdmirx_rd_cor(AVIRX_DBYTE4_DP2_IVCRX, rx_info.main_port);
	info->packet.data_bytes[5] = hdmirx_rd_cor(AVIRX_DBYTE5_DP2_IVCRX, rx_info.main_port);
	info->packet.data_bytes[6] = avi_pkt.line_num_end_topbar & 0xff;
	info->packet.data_bytes[7] = (avi_pkt.line_num_end_topbar & 0xff00) >> 8;
	info->packet.data_bytes[8] = avi_pkt.line_num_start_btmbar & 0xff;
	info->packet.data_bytes[9] = (avi_pkt.line_num_start_btmbar & 0xff00) >> 8;
	info->packet.data_bytes[10] = avi_pkt.pix_num_left_bar & 0xff;
	info->packet.data_bytes[11] = (avi_pkt.pix_num_left_bar & 0xff00) >> 8;
	info->packet.data_bytes[12] = avi_pkt.pix_num_right_bar & 0xff;
	info->packet.data_bytes[13] = (avi_pkt.pix_num_right_bar & 0xff00) >> 8;
	if (log_level & DBG1_LOG) {
		rx_pr("qt_range:%d, it_content:%d, vic:%d, cn_type:%d, status:%d\n",
			info->rgb_quantization_range, info->it_content, info->vic,
			info->content_type, info->packet_status);
		rx_print_pkt(info->packet);
	}
}

void get_hdmirx_dolby_hdr_info(struct hdmirx_dolby_hdr_s *info)
{
	if (!info)
		return;
	if (rx[rx_info.main_port].amdv_type) {
		if (rx[rx_info.main_port].vs_info_details.dv_allm)
			info->type = HDMIRX_DOLBY_HDR_TYPE_LOW_LATENCY;
		else if (rx[rx_info.main_port].amdv_type == DV_VSIF)
			info->type = HDMIRX_DOLBY_HDR_TYPE_STANDARD_VSIF_2;
	} else {
		info->type = HDMIRX_DOLBY_HDR_TYPE_SDR;//need to check hdr,2022/08/01 start
	}
	if (log_level & DBG1_LOG)
		rx_pr("dolby_hdr_info:%d\n", info->type);
}

void get_hdmirx_edid_info(struct hdmirx_edid_s *info)
{
	int edid_size = 0;

	if (!info) {
		pr_err("info is NULL\n");
		return;
	}
	if (info->size == HDMIRX_EDID_SIZE_128) {
		edid_size = 128;
	} else if (info->size == HDMIRX_EDID_SIZE_256) {
		edid_size = 256;
	} else if (info->size == HDMIRX_EDID_SIZE_512) {
		edid_size = 512;
	} else {
		pr_err("%s input edid->size:%d is invalid\n", __func__, info->size);
		return;
	}

	info->pdata = load_edid;
}

void set_hdmirx_edid_info(struct hdmirx_edid_s info)
{
	int size = 256;
	int i = 0;

	if (info.size == HDMIRX_EDID_SIZE_128) {
		size = 128;
	} else if (info.size == HDMIRX_EDID_SIZE_256) {
		size = 256;
	} else if (info.size == HDMIRX_EDID_SIZE_512) {
		size = 512;
	} else {
		pr_err("%s input edid->size:%d is invalid\n", __func__, info.size);
		return;
	}

	memset(load_edid, 0, 512);
	memcpy(load_edid, info.pdata, size);
	if (log_level & EDID_LOG) {
		for (i = 0; i < size; i++)
			if (log_level & REG_LOG)
				rx_pr("%s input info.pData[%d]:%d\n", __func__, i, info.pdata[i]);
	}

	hdmirx_fill_edid_buf(info.pdata, size);
}

void get_hdmirx_connect_state_info(struct hdmirx_connection_state_s *info)
{
	if (!info)
		return;
	info->state = rx[rx_info.main_port].cur_5v_sts;
	if (log_level & REG_LOG)
		rx_pr("port:%d state:%d\n", info->port, info->state);
}

void get_hdmirx_hpd_info(struct hdmirx_hpd_s *info)
{
	if (!info)
		return;
	unsigned int status = rx_get_hpd_sts(rx_info.main_port);

	switch (info->port) {
	case HDMIRX_INPUT_PORT_1:
		status = (status >> 0) & 0x1;
		break;
	case HDMIRX_INPUT_PORT_2:
		status = (status >> 1) & 0x1;
		break;
	case HDMIRX_INPUT_PORT_3:
		status = (status >> 2) & 0x1;
		break;
	case HDMIRX_INPUT_PORT_4:
	    status = (status >> 3) & 0x1;
		break;
	default:
		status = 0;
		break;
	}

	if (status == 0)
		info->hpd_state = HDMIRX_HPD_DISABLE;
	else
		info->hpd_state = HDMIRX_HPD_ENABLE;
	if (log_level & REG_LOG)
		rx_pr("port:%d hpd:%d\n", info->port, info->hpd_state);
}

void set_hdmirx_hpd_info(struct hdmirx_hpd_s info)
{
	u8 port_id = HDMIRX_INPUT_PORT_1;
	struct hdmirx_hpd_s cur_info;

	if (info.port >= HDMIRX_INPUT_PORT_1 &&
		info.port <= HDMIRX_INPUT_PORT_4) {
		if (log_level & REG_LOG)
			rx_pr("port:%d hpd:%d\n", info.port, info.hpd_state);

		port_id = info.port - HDMIRX_INPUT_PORT_1;
		cur_info.port = HDMIRX_INPUT_PORT_1;
		get_hdmirx_hpd_info(&cur_info);
		if (info.hpd_state == HDMIRX_HPD_DISABLE) {
			if (cur_info.hpd_state == HDMIRX_HPD_ENABLE)
				rx_set_ext_state(port_id, FSM_HPD_LOW);
		} else if (info.hpd_state == HDMIRX_HPD_ENABLE) {
			if (cur_info.hpd_state == HDMIRX_HPD_DISABLE)
				rx_set_ext_state(port_id, FSM_HPD_HIGH);
		} else if (info.hpd_state == HDMIRX_HPD_RESTART) {
			if (cur_info.hpd_state == HDMIRX_HPD_ENABLE) {
				rx_set_port_hpd(port_id, 0);
				rx_set_port_hpd(port_id, 1);
			} else {
				if (log_level & REG_LOG)
					rx_pr("port:%d is disable\n", info.port);
			}
		}
	} else {
		pr_err("input port:%d is invalid\n", info.port);
	}
}

void get_hdmirx_vrr_info(struct hdmirx_vrr_frequency_s *info)
{
	if (!info)
		return;
	info->port = rx_info.main_port;
	info->frequency = rx[rx_info.main_port].cur.frame_rate;
	info->m_const = rx[info->port].vtem_info.m_const;
	info->next_tfr = rx[info->port].vtem_info.next_tfr;
	info->ieee = rx[info->port].vtem_info.ieee;
	info->base_vfront = rx[info->port].vtem_info.base_vfront;
	info->base_framerate = rx[info->port].vtem_info.base_framerate;
	if (rx[info->port].qms_plus_flag) {
		info->vrr_type = HDMIRX_VRR_TYPE_QMS_PLUS;
		info->ieee = IEEE_QMS_PLUS;
		info->qms_plus_type = rx[info->port].qms_plus_flag;
	} else if (rx[info->port].vtem_info.qms_en) {
		info->vrr_type = HDMIRX_VRR_TYPE_QMS;
	} else if (rx[info->port].vtem_info.vrr_en) {
		info->vrr_type = HDMIRX_VRR_TYPE_GAMING;
	} else {
		info->vrr_type = HDMIRX_VRR_TYPE_NONE;
	}
	if (log_level & DBG1_LOG)
		rx_pr("port:%d vrr frequency:%d\n", info->port, info->frequency);
}

void get_hdmirx_emp_info(struct hdmirx_emp_info_s *info)
{
	int i = 0;

	if (!info)
		return;
	info->port = rx_info.main_port;
	info->total_packet_number = rx[rx_info.main_port].emp_dsf_info[0].pkt_cnt;
	info->current_packet_index = 0; //TODO
	info->type = rx[rx_info.main_port].emp_type;
	memcpy(info->data, emp_buf[rx[rx_info.main_port].emp_vid_idx], 31);
	if (log_level & DBG1_LOG) {
		rx_pr("pkt_num:%d, type:%d\n", info->total_packet_number, info->type);
		for (i = 0; i < 31; i++)
			rx_pr("data[%d]:0x%x\n", i, info->data[i]);
	}
}

static u64 get_hdmi_capability_status(void)
{
	u64 data64 = 0;

	data64 |= 0ULL << 38;//V4L2_EXT_HDMI_DSC
	data64 |= 0ULL << 37;//V4L2_EXT_HDMI_HDCP_REPEATER
	data64 |= 1ULL << 36;//V4L2_EXT_HDMI_EMPACKET
	data64 |= 0ULL << 35;//V4L2_EXT_HDMI_512BYTE_EDID?
	data64 |= 1ULL << 34;//V4L2_EXT_HDMI_DOLBY_VISION_HDMI21
	data64 |= 1ULL << 33;//V4L2_EXT_HDMI_DOLBY_VISION_LL
	data64 |= 1ULL << 32;//V4L2_EXT_HDMI_DOLBY_VISION
	data64 |= 0ULL << 31;//V4L2_EXT_HDMI_CINEMAVRR
	data64 |= 1ULL << 30;//V4L2_EXT_HDMI_VRR
	data64 |= 0ULL << 29;//V4L2_EXT_HDMI_FVA
	data64 |= 0ULL << 28;//V4L2_EXT_HDMI_BACKGROUND_TIMING_DETECT?
	data64 |= 0ULL << 27;//V4L2_EXT_HDMI_FRL_4L_12G
	data64 |= 0ULL << 26;//V4L2_EXT_HDMI_FRL_4L_10G
	data64 |= 0ULL << 25;//V4L2_EXT_HDMI_FRL_4L_8G
	data64 |= 0ULL << 24;//V4L2_EXT_HDMI_FRL_4L_6G
	data64 |= 0ULL << 23;//V4L2_EXT_HDMI_FRL_3L_6G
	data64 |= 0ULL << 22;//V4L2_EXT_HDMI_FRL_3L_3G
	data64 |= 1ULL << 21;//V4L2_EXT_HDMI_TMDS_6G
	data64 |= 1ULL << 20;//V4L2_EXT_HDMI_TMDS_3G
	data64 |= 1ULL << 14;//V4L2_EXT_HDMI_HDMI21
	data64 |= 1ULL << 13;//V4L2_EXT_HDMI_HDMI20
	data64 |= 1ULL << 12;//V4L2_EXT_HDMI_HDMI14
	data64 |= 0ULL << 3;// V4L2_EXT_HDMI_HDCP23_KEY_OTP
	data64 |= 1ULL << 2;//V4L2_EXT_HDMI_HDCP23
	data64 |= 0ULL << 1;//V4L2_EXT_HDMI_HDCP14_KEY_OTP
	data64 |= 1ULL;//V4L2_EXT_HDMI_HDCP14
	return data64;
}

void get_hdmirx_querycap_info(struct hdmirx_querycap_s *info)
{
	if (!info)
		return;
	switch (rx_info.chip_id) {
	case CHIP_ID_T3:
		strcpy(info->hdmi_capability.chip, "T3");
		break;
	case CHIP_ID_T5W:
		strcpy(info->hdmi_capability.chip, "T5W");
		break;
	default:
		strcpy(info->hdmi_capability.chip, "unsupport-chip");
		break;
	}
	info->hdmi_capability.version = 0; //TODO
	info->hdmi_capability.capabilities = get_hdmi_capability_status();
	if (log_level & DBG1_LOG)
		rx_pr("hdmi_capability.chip:%s, cap:%llu\n", info->hdmi_capability.chip,
			info->hdmi_capability.capabilities);
}

void get_hdmirx_phy_status_info(struct hdmirx_phy_status_s *info)
{
	int i;
	u8 port = rx_info.main_port;

	if (!info || rx_info.chip_id != CHIP_ID_T5M)
		return;
	dump_phy_status(port);
	info->lock_status = rx[port].phy_sts.lock_status;
	info->link_lane = rx[port].phy_sts.link_lane;
	info->link_rate = rx[port].phy_sts.link_rate;
	info->link_type = rx[port].phy_sts.link_type;
	info->tmds_clk_khz = rx[port].phy_sts.tmds_clk_khz;
	memcpy(info->ctle_eq_max_range, rx[port].phy_sts.ctle_eq_max_range,
		sizeof(unsigned int) * HDMIRX_TMDS_CH_NUM);
	memcpy(info->ctle_eq_min_range, rx[port].phy_sts.ctle_eq_min_range,
		sizeof(unsigned int) * HDMIRX_TMDS_CH_NUM);
	memcpy(info->ctle_eq_result, rx[port].phy_sts.ctle_eq_result,
		sizeof(unsigned int) * HDMIRX_TMDS_CH_NUM);
	memcpy(info->error, rx[port].phy_sts.error, sizeof(unsigned int) * HDMIRX_TMDS_CH_NUM);
	if (log_level & DBG1_LOG) {
		rx_pr("---phy info---\n");
		rx_pr("lock:%d, lane:%d, rate:%d, type:%d, clk:%d\n",
			info->lock_status, info->link_lane, info->link_rate,
			info->link_type, info->tmds_clk_khz);
		for (i = 0; i < HDMIRX_TMDS_CH_NUM; i++) {
			rx_pr("eq_max_range[%d]:%d, eq_min_range[%d]:%d, eq_result[%d]:%d\n",
				i, info->ctle_eq_max_range[i],
				i, info->ctle_eq_min_range[i],
				i, info->ctle_eq_result[i]);
			rx_pr("err[%d]:%d ", i, info->error[i]);
		}
	}
}

void get_hdmirx_link_status_info(struct hdmirx_link_status_s *info)
{
	if (!info)
		return;
	rx_get_video_info(rx_info.main_port);
	info->port = rx_info.main_port;
	info->hpd = rx_get_hpd_sts(rx_info.main_port);
	info->hdmi_5v = (rx_get_hdmi5v_sts() >> rx_info.main_port) & 0x1;
	info->rx_sense = rx_info.aml_phy.rterm_val;
	info->frame_rate_x100_hz = rx[info->port].cur.frame_rate;
	info->dvi_hdmi_mode = !rx_get_dvi_mode(rx_info.main_port);
	info->interlaced = rx[info->port].cur.interlaced;
	info->video_width = rx[info->port].cur.hactive;
	info->video_height = rx[info->port].cur.vactive;
	info->color_space = rx[info->port].cur.colorspace;
	info->color_depth = rx[info->port].cur.colordepth;
	info->colorimetry = rx[info->port].cur.colorimetry;
	info->ext_colorimetry = rx[info->port].cur.ext_colorimetry;
	info->audio_format = rx[info->port].aud_info.coding_type;
	info->audio_sampling_freq = rx[info->port].aud_info.sample_frequency;
	info->audio_channel_number = rx[info->port].aud_info.channel_count;
	info->hdr_type = rx[info->port].hdr_info.hdr_type;
	if (log_level & DBG1_LOG) {
		rx_pr("---link info---\n");
		rx_pr("port:%d, hpd:%d, 5v:%d, rx_sense:%d, fr:%d, width:%d, height:%d\n",
			info->port, info->hpd, info->hdmi_5v, info->rx_sense,
			info->frame_rate_x100_hz, info->video_width, info->video_height);
		rx_pr("colorspace:%d, colorimetry:%d, ext_colorimetry:%d, hdr_type:%d\n",
			info->color_space, info->colorimetry,
			info->ext_colorimetry, info->hdr_type);
		rx_pr("audio_format:%d, audio_sampling_freq:%d, audio_channel_number:%d\n",
			info->audio_format, info->audio_sampling_freq, info->audio_channel_number);
	}
}

void get_hdmirx_video_status_info(struct hdmirx_video_status_s *info)
{
	if (!info)
		return;
	info->port = rx_info.main_port;
	info->video_height_real = rx[info->port].cur.vactive;
	info->video_width_real = rx[info->port].cur.hactive;
	info->video_htotal_real = rx[info->port].cur.htotal;
	info->video_vtotal_real = rx[info->port].cur.vtotal;
	info->pixel_clock_khz = rx[info->port].cur.htotal * rx[info->port].cur.vtotal *
		rx[info->port].cur.frame_rate / 1000;
	info->current_vrr_refresh_rate = rx[info->port].cur.frame_rate;
	if (log_level & DBG1_LOG) {
		rx_pr("---video info---\n");
		rx_pr("port:%d, height:%d, width:%d, htotal:%d, vtotal:%d, clk:%d, fr:%d\n",
			info->port, info->video_height_real, info->video_width_real,
			info->video_htotal_real, info->video_vtotal_real, info->pixel_clock_khz,
			info->current_vrr_refresh_rate);
	}
}

void get_hdmirx_audio_status_info(struct hdmirx_audio_status_s *info)
{
	if (!info)
		return;
	info->port = rx_info.main_port;
	info->pcm_N = rx[info->port].aud_info.n;
	info->pcm_CTS = rx[info->port].aud_info.cts;
	info->layoutbitvalue = rx[info->port].aud_info.auds_layout;
	info->channelstatusbits = rx[info->port].aud_info.ch_sts_bits; //bit24-27
	if (log_level & DBG1_LOG) {
		rx_pr("---audio info---\n");
		rx_pr("port:%d, N:%d, CTS:%d, layout:%d, ch_sts:0x%x\n",
			info->port, info->pcm_N, info->pcm_CTS,
			info->layoutbitvalue, info->channelstatusbits);
	}
}

void rx_get_hdcp14_aksv(unsigned char *buf)
{
	int pos = 0;

	if (rx_info.chip_id >= CHIP_ID_T7) {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AKSV5_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AKSV4_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AKSV3_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AKSV2_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AKSV1_HDCP1X_IVCRX, rx_info.main_port));
	} else {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_AKSV1));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_AKSV0));
	}
}

void rx_get_hdcp14_bksv(unsigned char *buf)
{
	int pos = 0;

	if (rx_info.chip_id >= CHIP_ID_T7) {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BKSV_5_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BKSV_4_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BKSV_3_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BKSV_2_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BKSV_1_HDCP1X_IVCRX, rx_info.main_port));
	} else {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_BKSV1));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_BKSV0));
	}
}

void rx_get_hdcp14_an(unsigned char *buf)
{
	int pos = 0;

	if (rx_info.chip_id >= CHIP_ID_T7) {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN8_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN7_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN6_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN5_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN4_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN3_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN2_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_AN1_HDCP1X_IVCRX, rx_info.main_port));
	} else {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_AN1));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_dwc(DWC_HDCP_AN0));
	}
}

unsigned int rx_get_hdcp14_bcaps(void)
{
	if (rx_info.chip_id >= CHIP_ID_T7)
		return hdmirx_rd_cor(RX_BCAPS_SET_HDCP1X_IVCRX, rx_info.main_port);
	else
		return hdmirx_rd_dwc(HDCP_BCAPS);
}

void rx_get_hdcp14_bstatus(unsigned char *buf)
{
	int pos = 0;

	if (rx_info.chip_id >= CHIP_ID_T7) {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BSTATUS2_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_BSTATUS1_HDCP1X_IVCRX, rx_info.main_port));
	} else {
		//todo
	}
}

void rx_get_hdcp14_ri(unsigned char *buf)
{
	int pos = 0;

	if (rx_info.chip_id >= CHIP_ID_T7) {
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_RI2_HDCP1X_IVCRX, rx_info.main_port));
		pos += snprintf(buf + pos, 12 - pos, "%x",
			hdmirx_rd_cor(RX_SHD_RI2_HDCP1X_IVCRX, rx_info.main_port));
	} else {
		//todo
	}
}

void get_hdmirx_hdcp_status_info(struct hdmirx_hdcp_status_s *info)
{
	if (!info)
		return;
	info->port = rx_info.main_port;
	switch (rx[info->port].hdcp.hdcp_version) {
	case HDCP_VER_14:
		info->hdcp_version = HDMIRX_HDCP_VERSION_14;
		//rx_get_hdcp14_an(info->hdcp14_status.an);
		rx_get_hdcp14_aksv(info->hdcp14_status.aksv);
		rx_get_hdcp14_bksv(info->hdcp14_status.bksv);
		info->hdcp14_status.bcaps = rx_get_hdcp14_bcaps();
		rx_get_hdcp14_bstatus(info->hdcp14_status.bstatus);
		rx_get_hdcp14_ri(info->hdcp14_status.ri);
		break;
	case HDCP_VER_22:
		info->hdcp_version = HDMIRX_HDCP_VERSION_22;
		info->hdcp22_status.ake_init_count_since_5v = rx[info->port].hdcp.ake_init_cnt;
		info->hdcp22_status.reauth_req_count_since_5v = rx[info->port].hdcp.reauth_req_cnt;
		break;
	default:
		info->hdcp_version = HDMIRX_HDCP_VERSION_RESERVED;
		break;
	}
	if (rx_get_hdcp_auth_sts(info->port))
		info->auth_status = HDMIRX_HDCP_AUTH_STATUS_AUTHENTICATED;
	info->encen = rx_get_hdcp_auth_sts(info->port);
	/* ignore HDCP fail by environment issue */
	if (info->hdcp_version != HDMIRX_HDCP_VERSION_RESERVED &&
		info->auth_status != HDMIRX_HDCP_AUTH_STATUS_AUTHENTICATED)
		info->auth_status = HDMIRX_HDCP_AUTH_STATUS_AUTHENTICATED;

	if (log_level & DBG1_LOG) {
		rx_pr("---hdcp info---\n");
		rx_pr("port:%d, version:%d, auth_status:%d, encen:%d\n",
			info->port, info->hdcp_version, info->auth_status, info->encen);
		rx_pr("ake_init_cnt:%d, reauth_cnt:%d\n",
			info->hdcp22_status.ake_init_count_since_5v,
			info->hdcp22_status.reauth_req_count_since_5v);
	}
}

static u8 rx_get_scdc_sink_ver(u8 port)
{
	return hdmirx_rd_cor(SCDCS_SNK_VER_SCDC_IVCRX, port);
}

static u8 rx_get_scdc_src_ver(u8 port)
{
	return hdmirx_rd_cor(SCDCS_SRC_VER_SCDC_IVCRX, port);
}

void get_hdmirx_scdc_status_info(struct hdmirx_scdc_status_s *info)
{
	u32 data = 0;
	u8 port = rx_info.main_port;

	if (!info)
		return;
	info->sink_version = rx_get_scdc_sink_ver(port);
	info->source_version = rx_get_scdc_src_ver(port);

	data = hdmirx_rd_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, port);
	info->rsed_update = data & _BIT(6);
	info->flt_update = data & _BIT(5);
	info->frl_start = data & _BIT(4);
	info->source_test_update = data & _BIT(3);
	info->rr_test = data & _BIT(2);
	info->ced_update = data & _BIT(1);
	info->status_update = data & _BIT(0);

	data = hdmirx_rd_cor(SCDCS_TMDS_CONFIG_SCDC_IVCRX, port);
	info->tmds_bit_clock_ratio = data & _BIT(1);
	info->scrambling_enable = data & _BIT(0);

	info->tmds_scrambler_status = hdmirx_rd_cor(SCDCS_SCRAMBLE_STAT_SCDC_IVCRX, port);

	data = hdmirx_rd_cor(SCDCS_CONFIG0_SCDC_IVCRX, port);
	info->flt_no_retrain = data & _BIT(1);
	info->rr_enable = data & _BIT(0);

	data = hdmirx_rd_cor(SCDCS_CONFIG1_SCDC_IVCRX, port);
	info->ffe_levels = data & MSK(4, 4);
	info->frl_rate = data & MSK(4, 0);

	data = hdmirx_rd_cor(SCDCS_STATUS_FLAGS0_SCDC_IVCRX, port);
	info->dsc_decode_fail = data & _BIT(7);
	info->flt_ready = data & _BIT(6);
	info->clk_detect = data & _BIT(0);
	info->ch0_locked = data & _BIT(1);
	info->ch1_locked = data & _BIT(2);
	info->ch2_locked = data & _BIT(3);
	info->ch3_locked = data & _BIT(4);

	data = hdmirx_rd_cor(SCDCS_STATUS_FLAGS1_SCDC_IVCRX, port);
	info->lane0_ltp_request = data & MSK(4, 0);
	info->lane1_ltp_request = data & MSK(4, 4);

	data = hdmirx_rd_cor(SCDCS_STATUS_FLAGS2_SCDC_IVCRX, port);
	info->lane2_ltp_request = data & MSK(4, 0);
	info->lane3_ltp_request = data & MSK(4, 4);

	data = hdmirx_rd_cor(SCDCS_CED0_H_SCDC_IVCRX, port);
	info->ch0_ced_valid = data & _BIT(7);
	info->ch0_ced = ((data & 0x7f) << 8) | hdmirx_rd_cor(SCDCS_CED0_L_SCDC_IVCRX, port);

	data = hdmirx_rd_cor(SCDCS_CED1_H_SCDC_IVCRX, port);
	info->ch1_ced_valid = data & _BIT(7);
	info->ch1_ced = ((data & 0x7f) << 8) | hdmirx_rd_cor(SCDCS_CED1_L_SCDC_IVCRX, port);

	data = hdmirx_rd_cor(SCDCS_CED2_H_SCDC_IVCRX, port);
	info->ch2_ced_valid = data & _BIT(7);
	info->ch2_ced = ((data & 0x7f) << 8) | hdmirx_rd_cor(SCDCS_CED2_L_SCDC_IVCRX, port);

	data = hdmirx_rd_cor(SCDCS_CED_LN3_H_SCDC_IVCRX, port);
	info->ch3_ced_valid = data & _BIT(7);
	info->ch3_ced = ((data & 0x7f) << 8) | hdmirx_rd_cor(SCDCS_CED_LN3_L_SCDC_IVCRX, port);

	data = hdmirx_rd_cor(SCDCS_RS_CORR_H_SCDC_IVCRX, port);
	info->rs_correction_valid = data & _BIT(7);
	info->rs_correction_count = ((data & 0x7f) << 8) |
		hdmirx_rd_cor(SCDCS_RS_CORR_L_SCDC_IVCRX, port);
	if (log_level & DBG1_LOG) {
		rx_pr("---scdc info---\n");
		rx_pr("clk_rate:%d, scramble:%d, ch_lock:%d-%d-%d\n",
			info->tmds_bit_clock_ratio, info->tmds_scrambler_status,
			info->ch0_locked, info->ch1_locked, info->ch2_locked);
	}
}

void get_hdmirx_diagnosticts_status_info(struct hdmirx_diagnostics_status_s *info)
{
	if (!info)
		return;
	get_hdmirx_phy_status_info(&info->phy_status);
	get_hdmirx_link_status_info(&info->link_status);
	get_hdmirx_video_status_info(&info->video_status);
	get_hdmirx_audio_status_info(&info->audio_status);
	get_hdmirx_hdcp_status_info(&info->hdcp_status);
	get_hdmirx_scdc_status_info(&info->scdc_status);
}

static bool rx_chk_gcp_valid(void)
{
	u32 chk = 0;
	u32 i;

	for (i = RX_GCP_TYPE_DP3_IVCRX; i <= RX_GCP_DBYTE6_DP3_IVCRX; i++)
		chk += hdmirx_rd_cor(i, rx_info.main_port);

	if (chk & 0xff)
		return false;
	else
		return true;
}

void get_hdmirx_error_status_info(struct hdmirx_error_status_e *info)
{
	u32 ced_err, bch_err;

	if (!info)
		return;
	if (rx_chk_gcp_valid())
		info->error |= HDMIRX_ERROR_TYPE_GCP_ERROR;
	if (rx[rx_info.main_port].hdcp.reauth_req_cnt)
		info->error |= HDMIRX_ERROR_TYPE_HDCP22_REAUTH;
	if (!rx[rx_info.main_port].cableclk_stb_flg)
		info->error |= HDMIRX_ERROR_TYPE_TMDS_ERROR;
	ced_err = hdmirx_rd_cor(SCDCS_CED0_L_SCDC_IVCRX, rx_info.main_port) |
		hdmirx_rd_cor(SCDCS_CED1_L_SCDC_IVCRX, rx_info.main_port) |
		hdmirx_rd_cor(SCDCS_CED2_L_SCDC_IVCRX, rx_info.main_port) |
		hdmirx_rd_cor(SCDCS_CED_LN3_L_SCDC_IVCRX, rx_info.main_port);
	if (ced_err)
		info->error |= HDMIRX_ERROR_TYPE_CED_ERROR;
	if (rx[rx_info.main_port].afifo_sts)
		info->error |= HDMIRX_ERROR_TYPE_AUDIO_BUFFER;
	bch_err = hdmirx_rd_cor(RX_BCH_ERR_DP2_IVCRX, rx_info.main_port) |
			((u32)hdmirx_rd_cor(RX_BCH_ERR2_DP2_IVCRX, rx_info.main_port) << 8);
	if (bch_err)
		info->error |= HDMIRX_ERROR_TYPE_BCH;
	// HDMIRX_ERROR_TYPE_FLT---not support FRL

	info->param1 = 0;
	info->param2 = 0;
	if (log_level & DBG1_LOG)
		rx_pr("err status:0x%x\n", info->error);
}

void set_hdmirx_expert_setting_info(struct hdmirx_expert_setting_s info)
{
	u8 data;

	if (log_level & DBG1_LOG)
		rx_pr("expert setting: %d--%d\n", info.type, info.param1);
	switch (info.type) {
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_MODE:
		// >= 4K30HZ(auto), < 4K30HZ(fix)
		rx[rx_info.main_port].aml_phy.eq_stg1 = info.param1;
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH0:
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH1:
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH2:
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH3:
		rx[rx_info.main_port].aml_phy.eq_stg1 = info.param1;
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_TMDS_EQ_PERIOD:
		//not support
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_VIDEO_STABLE_COUNT:
		dwc_rst_wait_cnt_max = info.param1;
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_AUDIO_STABLE_COUNT:
		aud_sr_stb_max = info.param1;
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_DISABLE_HDCP22:
		data = hdmirx_rd_cor(RX_HDCP2x_CTRL_PWD_IVCRX, rx_info.main_port);
		hdmirx_wr_cor(RX_HDCP2x_CTRL_PWD_IVCRX, data & 1, rx_info.main_port);
		break;
	case HDMIRX_EXPERT_SETTING_TYPE_REAUTH_HDCP22:
		rx_hdcp_22_sent_reauth(rx_info.main_port);
		break;
	default:
		break;
	}
}

//ui show diagnostic info
unsigned int hdmirx_show_diagnostic_info(unsigned char *buf, int size)
{
	int pos = 0;
	int i = 0;
	struct hdmirx_diagnostics_status_s info;
	struct hdmirx_vrr_frequency_s vrr_info;
	struct hdmirx_emp_info_s emp_info;
	struct hdmirx_avi_info_s avi_info;
	struct hdmirx_vsi_info_s vsi_info;
	struct vsi_info_s vs_info_details;
	struct hdmirx_spd_info_s spd_info;

	get_hdmirx_diagnosticts_status_info(&info);
	get_hdmirx_avi_info(&avi_info);
	get_hdmirx_vrr_info(&vrr_info);
	get_hdmirx_emp_info(&emp_info);
	get_hdmirx_vsi_info(&vsi_info);
	get_hdmirx_spd_info(&spd_info);
	memcpy(&vs_info_details, &rx[rx_info.main_port].vs_info_details,
	sizeof(struct vsi_info_s));
	pos += snprintf(buf + pos, size - pos,
			"HDMI Basic Info:");
	//port hpd 5v rx_sense
	pos += snprintf(buf + pos, size - pos,
			"HDMI%d HPD=%s 5V=%s,", info.link_status.port,
			info.link_status.hpd ? "HIGH" : "LOW",
			info.link_status.hdmi_5v ? "HIGH" : "LOW");
	//timing @ framerate color_format color_depth
	pos += snprintf(buf + pos, size - pos,
			"%dx%d%s @ %dHZ %s %dBIT;", info.link_status.video_width,
			info.link_status.video_height,
			info.link_status.interlaced ? "i" : "p",
			info.link_status.frame_rate_x100_hz,
			color_format_str[info.link_status.color_space],
			info.link_status.color_depth);
	pos += snprintf(buf + pos, size - pos,
			"HDMI Video Info:");
	pos += snprintf(buf + pos, size - pos,
			"Active res=%dx%d %s %dBIT,",
			info.video_status.video_width_real, info.video_status.video_height_real,
			color_format_str[info.link_status.color_space],
			info.link_status.color_depth);
	//mode hdr_type
	pos += snprintf(buf + pos, size - pos,
			"HDMI Mode=%s,HDR Type=%s,",
			info.link_status.dvi_hdmi_mode ? "HDMI" : "DVI",
			hdr_type_str[info.link_status.hdr_type]);
	//VRR
	if (vrr_info.vrr_type != HDMIRX_VRR_TYPE_NONE) {
		pos += snprintf(buf + pos, size - pos,
			"VRR= %s,", vrr_type_str[vrr_info.vrr_type]);
		if (vrr_info.vrr_type == HDMIRX_VRR_TYPE_QMS) {
			//m_const next_tfr base_framerate
			pos += snprintf(buf + pos, size - pos,
				"m_const=%d,next_tfr=%d,base_framerate=%d,",
				vrr_info.m_const, vrr_info.next_tfr,
				vrr_info.base_framerate);
		} else if (vrr_info.vrr_type == HDMIRX_VRR_TYPE_QMS_PLUS) {
			//ieee, qms_plus_type
			pos += snprintf(buf + pos, size - pos,
				"ieee=0x%x,qms_plus_type=%s,",
				vrr_info.ieee, qms_plus_type_str[vrr_info.qms_plus_type]);
			pos += snprintf(buf + pos, size - pos,
				"next_tfr=%d,base_framerate=%d,",
				vrr_info.next_tfr, vrr_info.base_framerate);
		}
		pos += snprintf(buf + pos, size - pos,
			"VRR_frequency=%dHz;", info.video_status.current_vrr_refresh_rate);
	} else {
		pos += snprintf(buf + pos, size - pos,
			"VRR= %s;", vrr_type_str[vrr_info.vrr_type]);
	}
	pos += snprintf(buf + pos, size - pos,
			"HDMI Audio Info:");
	//aud_type ch samplerate
	pos += snprintf(buf + pos, size - pos,
			"%s %dCh %dHZ;", aud_format_str[info.link_status.audio_format],
			info.link_status.audio_channel_number,
			info.link_status.audio_sampling_freq);
	pos += snprintf(buf + pos, size - pos,
			"PHY Info:");
	pos += snprintf(buf + pos, size - pos,
				"lock_status=%d, link_lane=%d,",
				info.phy_status.lock_status, info.phy_status.link_lane);
	pos += snprintf(buf + pos, size - pos,
				"link_rate=%d, tmds_clk_khz=%d;",
				info.phy_status.link_rate, info.phy_status.tmds_clk_khz);
	pos += snprintf(buf + pos, size - pos,
			"HDCP Info:");
	pos += snprintf(buf + pos, size - pos,
				"version=%s, status=%d, enc_en=%d;",
				hdcp_version_str[info.hdcp_status.hdcp_version],
				info.hdcp_status.auth_status, info.hdcp_status.encen);
	pos += snprintf(buf + pos, size - pos,
			"SCDC Info:");
	pos += snprintf(buf + pos, size - pos,
				"scrambling_enable=%d,tmds_bit_clock_ratio=%d,",
				info.scdc_status.scrambling_enable,
				info.scdc_status.tmds_bit_clock_ratio);
	pos += snprintf(buf + pos, size - pos,
				"scrambling_status=%d,clock_detected=%d,",
				info.scdc_status.tmds_scrambler_status,
				info.scdc_status.clk_detect);
	pos += snprintf(buf + pos, size - pos,
				"ch0_lock=%d,ch1_lock=%d,ch2_lock=%d,ch3_lock=%d,flt_ready=%d,",
				info.scdc_status.ch0_locked, info.scdc_status.ch1_locked,
				info.scdc_status.ch2_locked, info.scdc_status.ch3_locked,
				info.scdc_status.flt_ready);
	pos += snprintf(buf + pos, size - pos,
				"rs_correction_count=%d,dsc_fail=%d,ch0=%d,ch1=%d,ch2=%d,ch3=%d,",
				info.scdc_status.rs_correction_count,
				info.scdc_status.dsc_decode_fail,
				info.scdc_status.ch0_ced, info.scdc_status.ch1_ced,
				info.scdc_status.ch2_ced, info.scdc_status.ch3_ced);
	pos += snprintf(buf + pos, size - pos,
				"lane_ltp_req ,ln0=%d,ln1=%d,ln2=%d,ln3=%d,",
				info.scdc_status.lane0_ltp_request,
				info.scdc_status.lane1_ltp_request,
				info.scdc_status.lane2_ltp_request,
				info.scdc_status.lane3_ltp_request);
	pos += snprintf(buf + pos, size - pos,
				"frl_rate=%d,ffe_level=%d,frl_start=%d,flt_update=%d;",
				info.scdc_status.frl_rate, info.scdc_status.ffe_levels,
				info.scdc_status.frl_start, info.scdc_status.flt_update);
	pos += snprintf(buf + pos, size - pos,
			"AVI Info-Frame:");
	pos += snprintf(buf + pos, size - pos,
			"raw data=%02x %02x %02x ", avi_info.packet.type,
			avi_info.packet.version, avi_info.packet.length);
	for (i = 0; i < HDMIRX_PACKET_DATA_LENGTH; i++)
		pos += snprintf(buf + pos, size - pos, "%02x ", avi_info.packet.data_bytes[i]);
	//Pixel Encoding,Active Info,Bar Info,Scaling
	pos += snprintf(buf + pos, size - pos,
			"Pixel Encoding=%s,Active Info=%s,Bar Info=%s,Scaling=%s,",
			color_format_str[avi_info.pixel_encoding],
			avi_info.active_info ? "VALID" : "INVALID",
			avi_bar_info_str[avi_info.bar_info],
			avi_scaling_str[avi_info.scaling]);
	//Scan Info,Colorimetry,Picture AR,Pixel Repeat
	pos += snprintf(buf + pos, size - pos,
			"Scan Info=%s,Colorimetry=%s,Picture AR=%s,Pixel Repeat=%d,",
			avi_scan_info_str[avi_info.scan_info],
			avi_colorimetry_str[avi_info.colorimetry],
			avi_picture_ar_str[avi_info.picture_aspect_ratio],
			avi_info.pixel_repeat);
	//AFD,IT Content,Content Type
	pos += snprintf(buf + pos, size - pos,
			"AFD=%s,IT Content=%s,Content Type=%s,",
			avi_active_format_str[avi_info.active_format_aspect_ratio],
			avi_info.it_content ? "ITCONTENT" : "NODATA",
			avi_content_type_str[avi_info.content_type]);
	//Extended Colorimetry, VIC
	pos += snprintf(buf + pos, size - pos,
			"Extended Colorimetry=%s,VIC=%d,",
			avi_ext_colorimetry_str[avi_info.extended_colorimetry],
			avi_info.vic);
	//RGB Quantization Range,YCC Quantization Range
	pos += snprintf(buf + pos, size - pos,
			"RGB Quantization Range=%s,YCC Quantization Range=%s;",
			avi_rgb_quantization_range_str[avi_info.rgb_quantization_range],
			avi_ycc_quantization_range_str[avi_info.ycc_quantization_range]);
	pos += snprintf(buf + pos, size - pos,
			"VSIF Info-Frame:");
	pos += snprintf(buf + pos, size - pos,
			"raw data=%02x %02x %02x ", vsi_info.packet.type,
			vsi_info.packet.version, vsi_info.packet.length);
	for (i = 0; i < HDMIRX_PACKET_DATA_LENGTH; i++)
		pos += snprintf(buf + pos, size - pos, "%02x ", vsi_info.packet.data_bytes[i]);
	switch (vsi_info.ieee) {
	case IEEE_DV15:
		pos += snprintf(buf + pos, size - pos,
			"[DOLBY VSIF]IEEE OUI=%02x%02x%02x,", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0]);
		pos += snprintf(buf + pos, size - pos,
			"low_latency=%d,dv_allm=%d;", vs_info_details.low_latency,
			vs_info_details.dv_allm);
		break;

	case IEEE_HDR10PLUS:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x HDR10PLUS=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0], vs_info_details.hdr10plus);
		break;

	case IEEE_CUVAHDR:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x CUVAHDR=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0], vs_info_details.cuva_hdr);
		break;

	case IEEE_FILMMAKER:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x FILMMAKER=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0], vs_info_details.filmmaker);
		break;

	case IEEE_VSI14:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x DV10=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0],
			rx[rx_info.main_port].amdv_type == DV_VSIF ? 1 : 0);
		break;

	case IEEE_VSI21:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x hdmi_allm=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0], vs_info_details.hdmi_allm);
		break;

	case IEEE_QMS_PLUS:
		pos += snprintf(buf + pos, size - pos,
			"[HF VSIF]IEEE OUI=%02x%02x%02x qms_plus=%d;", vsi_info.regid[2],
			vsi_info.regid[1], vsi_info.regid[0], vs_info_details.qms_plus);
		break;

	default:
		pos += snprintf(buf + pos, size - pos, ";");
		break;
	}
	pos += snprintf(buf + pos, size - pos,
			"EMP Info-Frame:");
	pos += snprintf(buf + pos, size - pos,
			"EMP type=%s,", emp_type_str[emp_info.type]);
	pos += snprintf(buf + pos, size - pos,
			"raw data=");
	for (i = 0; i < 31; i++)
		pos += snprintf(buf + pos, size - pos, "%02x ;", emp_info.data[i]);
	pos += snprintf(buf + pos, size - pos,
			"SPD Info-Frame:");
	pos += snprintf(buf + pos, size - pos,
			"raw data=%02x %02x %02x ", spd_info.packet.type,
			spd_info.packet.version, spd_info.packet.length);
	for (i = 0; i < HDMIRX_PACKET_DATA_LENGTH; i++)
		pos += snprintf(buf + pos, size - pos, "%02x ,", spd_info.packet.data_bytes[i]);
	pos += snprintf(buf + pos, size - pos,
			"Product Description=%s,", spd_info.product_description);
	pos += snprintf(buf + pos, size - pos,
			"vendor_name=%s, Source Info=0x%x;", spd_info.vendor_name,
			spd_info.source_device_info);
	return pos;
}

