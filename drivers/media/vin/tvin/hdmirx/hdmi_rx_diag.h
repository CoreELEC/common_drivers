/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_RX_IOC_H__
#define __HDMI_RX_IOC_H__

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_drv_ext.h"

extern int dwc_rst_wait_cnt_max;
extern int aud_sr_stb_max;

/* HDMI */
#define HDMIRX_PACKET_DATA_LENGTH 28
#define HDMIRX_INFOFRAME_PACKET_LEN 28
#define HDMIRX_VENDOR_SPECIFIC_REGID_LEN 3
#define HDMIRX_VENDOR_SPECIFIC_PAYLOAD_LEN                              \
	(HDMIRX_INFOFRAME_PACKET_LEN -                                      \
	HDMIRX_VENDOR_SPECIFIC_REGID_LEN)

#define HDMIRX_SPD_IF_VENDOR_LEN 8
#define HDMIRX_SPD_IF_DESC_LEN 16
#define HDMIRX_TMDS_CH_NUM 4

enum hdmirx_input_port_e {
	HDMIRX_INPUT_PORT_NONE = 0, /**< enum value none */
	HDMIRX_INPUT_PORT_1,        /**< enum value port1 */
	HDMIRX_INPUT_PORT_2,        /**< enum value port2 */
	HDMIRX_INPUT_PORT_3,        /**< enum value port3 */
	HDMIRX_INPUT_PORT_4,        /**< enum value port4 */
	HDMIRX_INPUT_PORT_ALL,      /**< enum value all */
};

enum hdmirx_mode_e {
	HDMIRX_MODE_DVI = 0, /**< enum value dvi */
	HDMIRX_MODE_HDMI,    /**< enum value hdmi */
};

enum hdmirx_color_depth_e {
	HDMIRX_COLOR_DEPTH_8BIT = 0, /**< enum value 8bit */
	HDMIRX_COLOR_DEPTH_10BIT,    /**< enum value 10bit */
	HDMIRX_COLOR_DEPTH_12BIT,    /**< enum value 12bit  */
	HDMIRX_COLOR_DEPTH_16BIT,    /**< enum value 16bit  */
	HDMIRX_COLOR_DEPTH_RESERVED  /**< enum value reserved */
};

struct hdmirx_video_rect_s {
	unsigned short x;
	unsigned short y;
	unsigned short w;
	unsigned short h;
};

struct hdmirx_timing_info_s {
	enum hdmirx_input_port_e port;
	unsigned short h_freq;
	unsigned short v_vreq;
	unsigned short h_total;
	unsigned short v_total;
	unsigned short h_porch;
	unsigned short v_porch;
	struct hdmirx_video_rect_s active;
	unsigned short scan_type;
	enum hdmirx_mode_e dvi_hdmi;
	enum hdmirx_color_depth_e color_depth;
	unsigned char allm_mode;
};

enum hdmirx_override_eotf_e {
	HDMIRX_OVERRIDE_EOTF_SDR_LUMINANCE_RANGE = 0,
	HDMIRX_OVERRIDE_EOTF_HDR_LUMINANCE_RANGE,
	HDMIRX_OVERRIDE_EOTF_SMPTE_ST_2084,
	HDMIRX_OVERRIDE_EOTF_HLG,
	HDMIRX_OVERRIDE_EOTF_RESERVED_4,
	HDMIRX_OVERRIDE_EOTF_RESERVED_5,
	HDMIRX_OVERRIDE_EOTF_RESERVED_6,
	HDMIRX_OVERRIDE_EOTF_RESERVED_7,
	HDMIRX_OVERRIDE_EOTF_AUTO,
};

struct hdmirx_override_drm_info_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_override_eotf_e override_eotf;
};

enum hdmirx_drm_eotf_e {
	HDMIRX_DRM_EOTF_SDR_LUMINANCE_RANGE =
		0, /**< enum value sdr luminance range */
	HDMIRX_DRM_EOTF_HDR_LUMINANCE_RANGE, /**< enum value hdr luminance range */
	HDMIRX_DRM_EOTF_SMPTE_ST_2084,       /**< enum value smpte st 2084 */
	HDMIRX_DRM_EOTF_HLG,                 /**< enum value hlg */
	HDMIRX_DRM_EOTF_RESERVED_4,          /**< enum value reserved */
	HDMIRX_DRM_EOTF_RESERVED_5,          /**< enum value reserved */
	HDMIRX_DRM_EOTF_RESERVED_6,          /**< enum value reserved */
	HDMIRX_DRM_EOTF_RESERVED_7,          /**< enum value reserved */
};

enum hdmirx_drm_meta_desc_e {
	HDMIRX_DRM_META_DESC_TYPE1 = 0,
	HDMIRX_DRM_META_DESC_RESERVED1,
	HDMIRX_DRM_META_DESC_RESERVED2,
	HDMIRX_DRM_META_DESC_RESERVED3,
	HDMIRX_DRM_META_DESC_RESERVED4,
	HDMIRX_DRM_META_DESC_RESERVED5,
	HDMIRX_DRM_META_DESC_RESERVED6,
	HDMIRX_DRM_META_DESC_RESERVED7,
};

struct hdmirx_drm_info_s {
	enum hdmirx_input_port_e port;
	unsigned char version;
	unsigned char length;
	enum hdmirx_drm_eotf_e eotf_type;
	enum hdmirx_drm_meta_desc_e meta_desc;
	unsigned short display_primaries_x0;
	unsigned short display_primaries_y0;
	unsigned short display_primaries_x1;
	unsigned short display_primaries_y1;
	unsigned short display_primaries_x2;
	unsigned short display_primaries_y2;
	unsigned short white_point_x;
	unsigned short white_point_y;
	unsigned short max_display_mastering_luminance;
	unsigned short min_display_mastering_luminance;
	unsigned short maximum_content_light_level;
	unsigned short maximum_frame_average_light_level;
};

enum hdmirx_vsi_video_format_e {
	HDMIRX_VSI_VIDEO_FORMAT_NO_ADDITIONAL_FORMAT = 0,
	HDMIRX_VSI_VIDEO_FORMAT_EXTENDED_RESOLUTION_FORMAT,
	HDMIRX_VSI_VIDEO_FORMAT_3D_FORMAT,
};

enum hdmirx_vsi_3d_structure_e {
	HDMIRX_VSI_3D_STRUCTURE_FRAME_PACKING = 0,
	HDMIRX_VSI_3D_STRUCTURE_FIELD_ALTERNATIVE,
	HDMIRX_VSI_3D_STRUCTURE_LINE_ALTERNATIVE,
	HDMIRX_VSI_3D_STRUCTURE_SIDEBYSIDE_FULL,
	HDMIRX_VSI_3D_STRUCTURE_L_DEPTH,
	HDMIRX_VSI_3D_STRUCTURE_L_DEPTH_GRAPHICS,
	HDMIRX_VSI_3D_STRUCTURE_TOP_BOTTOM,
	/*reserved 7*/
	HDMIRX_VSI_3D_STRUCTURE_SIDEBYSIDE_HALF	= 0x08,
	HDMIRX_VSI_3D_STRUCTURE_TOP_BOTTOM_DIRECTV = 0x09,
};

enum hdmirx_vsi_3d_ext_data_e {
	HDMIRX_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_0 = 0,
	HDMIRX_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_1,
	HDMIRX_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_2,
	HDMIRX_VSI_3D_EXT_DATA_HOR_SUB_SAMPL_3,
	HDMIRX_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_0,
	HDMIRX_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_1,
	HDMIRX_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_2,
	HDMIRX_VSI_3D_EXT_DATA_QUINCUNX_MATRIX_3,
};

enum hdmirx_vsi_vic_e {
	HDMIRX_VSI_VIC_RESERVED = 0,
	HDMIRX_VSI_VIC_4K2K_30HZ,
	HDMIRX_VSI_VIC_4K2K_25HZ,
	HDMIRX_VSI_VIC_4K2K_24HZ,
	HDMIRX_VSI_VIC_4K2K_24HZ_SMPTE,
};

enum hdmirx_packet_status_e {
	HDMIRX_PACKET_STATUS_NOT_RECEIVED = 0,
	HDMIRX_PACKET_STATUS_STOPPED,
	HDMIRX_PACKET_STATUS_UPDATED,
};

struct hdmirx_in_packet_s {
	unsigned char type;
	unsigned char version;
	unsigned char length;
	unsigned char data_bytes[HDMIRX_PACKET_DATA_LENGTH];
};

struct hdmirx_vsi_info_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_vsi_video_format_e video_format;
	enum hdmirx_vsi_3d_structure_e st_3d;
	enum hdmirx_vsi_3d_ext_data_e ext_data_3d;
	enum hdmirx_vsi_vic_e vic;
	unsigned int ieee;
	unsigned char regid[HDMIRX_VENDOR_SPECIFIC_REGID_LEN];
	unsigned char payload[HDMIRX_VENDOR_SPECIFIC_PAYLOAD_LEN];
	enum hdmirx_packet_status_e packet_status;
	struct hdmirx_in_packet_s packet;
};

struct hdmirx_spd_info_s {
	enum hdmirx_input_port_e port;
	unsigned char vendor_name[HDMIRX_SPD_IF_VENDOR_LEN + 1];
	unsigned char product_description[HDMIRX_SPD_IF_DESC_LEN + 1];
	unsigned char source_device_info;
	enum hdmirx_packet_status_e packet_status;
	struct hdmirx_in_packet_s packet;
};

enum hdmirx_avi_csc_e {
	HDMIRX_AVI_CSC_RGB = 0,
	HDMIRX_AVI_CSC_YCBCR422,
	HDMIRX_AVI_CSC_YCBCR444,
	HDMIRX_AVI_CSC_YCBCR420,
};

enum hdmirx_avi_active_info_e {
	HDMIRX_AVI_ACTIVE_INFO_INVALID = 0,
	HDMIRX_AVI_ACTIVE_INFO_VALID,
};

enum hdmirx_avi_bar_info_e {
	HDMIRX_AVI_BAR_INFO_INVALID = 0,
	HDMIRX_AVI_BAR_INFO_VERTICALVALID,
	HDMIRX_AVI_BAR_INFO_HORIZVALID,
	HDMIRX_AVI_BAR_INFO_VERTHORIZVALID,
};

enum hdmirx_avi_scan_info_e {
	HDMIRX_AVI_SCAN_INFO_NODATA = 0,
	HDMIRX_AVI_SCAN_INFO_OVERSCANNED,
	HDMIRX_AVI_SCAN_INFO_UNDERSCANNED,
	HDMIRX_AVI_SCAN_INFO_FUTURE,
};

enum hdmirx_avi_colorimetry_e {
	HDMIRX_AVI_COLORIMETRY_NODATA = 0,
	HDMIRX_AVI_COLORIMETRY_SMPTE170,
	HDMIRX_AVI_COLORIMETRY_ITU709,
	HDMIRX_AVI_COLORIMETRY_FUTURE,
	HDMIRX_AVI_COLORIMETRY_EXTENDED =
		HDMIRX_AVI_COLORIMETRY_FUTURE,

};

enum hdmirx_avi_picture_arc_e {
	HDMIRX_AVI_PICTURE_ARC_NODATA = 0,
	HDMIRX_AVI_PICTURE_ARC_4_3,
	HDMIRX_AVI_PICTURE_ARC_16_9,
	HDMIRX_AVI_PICTURE_ARC_FUTURE,
};

enum hdmirx_avi_active_format_arc_e {
	HDMIRX_AVI_ACTIVE_FORMAT_ARC_PICTURE	= 8,
	HDMIRX_AVI_ACTIVE_FORMAT_ARC_4_3CENTER  = 9,
	HDMIRX_AVI_ACTIVE_FORMAT_ARC_16_9CENTER = 10,
	HDMIRX_AVI_ACTIVE_FORMAT_ARC_14_9CENTER = 11,
	HDMIRX_AVI_ACTIVE_FORMAT_ARC_OTHER	  = 0,
};

enum hdmirx_avi_scaling_e {
	HDMIRX_AVI_SCALING_NOSCALING = 0,
	HDMIRX_AVI_SCALING_HSCALING,
	HDMIRX_AVI_SCALING_VSCALING,
	HDMIRX_AVI_SCALING_HVSCALING,
};

enum hdmirx_avi_it_content_e {
	HDMIRX_AVI_IT_CONTENT_NODATA = 0,
	HDMIRX_AVI_IT_CONTENT_ITCONTENT,
};

enum hdmirx_avi_ext_colorimetry_e {
	HDMIRX_AVI_EXT_COLORIMETRY_XVYCC601 = 0,
	HDMIRX_AVI_EXT_COLORIMETRY_XVYCC709,
	HDMIRX_AVI_EXT_COLORIMETRY_SYCC601,
	HDMIRX_AVI_EXT_COLORIMETRY_ADOBEYCC601,
	HDMIRX_AVI_EXT_COLORIMETRY_ADOBERGB,
	HDMIRX_AVI_EXT_COLORIMETRY_BT2020_YCCBCCRC,
	HDMIRX_AVI_EXT_COLORIMETRY_BT2020_RGBORYCBCR,
	HDMIRX_AVI_EXT_COLORIMETRY_XVRESERED,
};

enum hdmirx_avi_rgb_quantization_range_e {
	HDMIRX_AVI_RGB_QUANTIZATION_RANGE_DEFAULT = 0,
	HDMIRX_AVI_RGB_QUANTIZATION_RANGE_LIMITEDRANGE,
	HDMIRX_AVI_RGB_QUANTIZATION_RANGE_FULLRANGE,
	HDMIRX_AVI_RGB_QUANTIZATION_RANGE_RESERVED,
};

enum hdmirx_avi_ycc_quantization_range_e {
	HDMIRX_AVI_YCC_QUANTIZATION_RANGE_LIMITEDRANGE = 0,
	HDMIRX_AVI_YCC_QUANTIZATION_RANGE_FULLRANGE,
	HDMIRX_AVI_YCC_QUANTIZATION_RANGE_RESERVED,
};

enum hdmirx_avi_content_type_e {
	HDMIRX_AVI_CONTENT_TYPE_GRAPHICS = 0,
	HDMIRX_AVI_CONTENT_TYPE_PHOTO,
	HDMIRX_AVI_CONTENT_TYPE_CINEMA,
	HDMIRX_AVI_CONTENT_TYPE_GAME,
};

enum hdmirx_avi_additional_colorimetry_e {
	HDMIRX_AVI_ADDITIONAL_COLORIMETRY_DCI_P3_D65 = 0,
	HDMIRX_AVI_ADDITIONAL_COLORIMETRY_DCI_P3_THEATER,
	HDMIRX_AVI_ADDITIONAL_COLORIMETRY_BT2100,
	HDMIRX_AVI_ADDITIONAL_COLORIMETRY_RESERVED,
};

struct hdmirx_avi_info_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_mode_e mode;

	enum hdmirx_avi_csc_e pixel_encoding;
	enum hdmirx_avi_active_info_e active_info;
	enum hdmirx_avi_bar_info_e bar_info;
	enum hdmirx_avi_scan_info_e scan_info;
	enum hdmirx_avi_colorimetry_e colorimetry;
	enum hdmirx_avi_picture_arc_e picture_aspect_ratio;
	enum hdmirx_avi_active_format_arc_e active_format_aspect_ratio;
	enum hdmirx_avi_scaling_e scaling;

	unsigned char vic;
	unsigned char pixel_repeat;

	enum hdmirx_avi_it_content_e it_content;
	enum hdmirx_avi_ext_colorimetry_e extended_colorimetry;
	enum hdmirx_avi_rgb_quantization_range_e rgb_quantization_range;
	enum hdmirx_avi_ycc_quantization_range_e ycc_quantization_range;
	enum hdmirx_avi_content_type_e content_type;
	enum hdmirx_avi_additional_colorimetry_e additional_colorimetry;

	unsigned short top_bar_end_line_number;
	unsigned short bottom_bar_start_line_number;
	unsigned short left_bar_end_pixel_number;
	unsigned short right_bar_end_pixel_number;

	enum hdmirx_packet_status_e packet_status;
	struct hdmirx_in_packet_s packet;
};

struct hdmirx_packet_info_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_mode_e mode;
	struct hdmirx_avi_info_s avi;
	struct hdmirx_spd_info_s spd;
	struct hdmirx_vsi_info_s vsi;
};

enum hdmirx_dolby_hdr_type_e {
	HDMIRX_DOLBY_HDR_TYPE_SDR = 0,
	HDMIRX_DOLBY_HDR_TYPE_STANDARD_VSIF_1,
	HDMIRX_DOLBY_HDR_TYPE_STANDARD_VSIF_2,
	HDMIRX_DOLBY_HDR_TYPE_LOW_LATENCY,
};

struct hdmirx_dolby_hdr_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_dolby_hdr_type_e type;
};

enum hdmirx_edid_size_e {
	HDMIRX_EDID_SIZE_128 = 0,
	HDMIRX_EDID_SIZE_256,
	HDMIRX_EDID_SIZE_512,
};

struct hdmirx_edid_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_edid_size_e size;
	union {
		unsigned char *pdata;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct hdmirx_connection_state_s {
	enum hdmirx_input_port_e port;
	unsigned char state;
};

enum hdmirx_hpd_state_e {
	HDMIRX_HPD_DISABLE = 0,
	HDMIRX_HPD_ENABLE,
	HDMIRX_HPD_RESTART,
};

struct hdmirx_hpd_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_hpd_state_e hpd_state;
};

enum hdmirx_vrr_type_e {
	HDMIRX_VRR_TYPE_NONE = 0,
	HDMIRX_VRR_TYPE_GAMING,
	HDMIRX_VRR_TYPE_QMS,
	HDMIRX_VRR_TYPE_QMS_PLUS,
	HDMIRX_VRR_TYPE_MAX,
};

struct hdmirx_vrr_frequency_s {
	enum hdmirx_input_port_e port;
	unsigned short frequency;
	enum hdmirx_vrr_type_e vrr_type;
	unsigned char m_const;
	unsigned int next_tfr;
	unsigned int ieee;
	unsigned char base_vfront;
	unsigned short base_framerate;
	unsigned char qms_plus_type;
};

enum hdmirx_emp_type_e {
	HDMIRX_EMP_TYPE_UNDEFINED = 0,
	HDMIRX_EMP_TYPE_VSEM,
	HDMIRX_EMP_TYPE_DYNAMICHDREM,
	HDMIRX_EMP_TYPE_VTEM,
	HDMIRX_EMP_TYPE_CVTEM
};

struct hdmirx_emp_info_s {
	enum hdmirx_input_port_e port;
	//enum hdmirx_emp_type_e type;
	unsigned char type;
	unsigned short current_packet_index;
	unsigned short total_packet_number;
	unsigned char data[31];
};

struct hdmirx_hdcp_repeater_s {
	enum hdmirx_input_port_e port;
	unsigned char repeater_mode;
	unsigned char receiver_id[5];
	unsigned char repeater_hpd;
};

struct hdmirx_capability_s {
	unsigned char chip[16];		  // chip name. for example "e60"
	unsigned int version;			// linuxtv header version
	unsigned long long capabilities; // HDMI Capabilities Flags
	unsigned long long reserved[3];
};

struct hdmirx_querycap_s {
	enum hdmirx_input_port_e port;
	struct hdmirx_capability_s hdmi_capability;
};

struct hdmirx_hpd_low_duration_dc_on_s {
	enum hdmirx_input_port_e port;
	int hpd_low_duration;
};

enum  hdmirx_sleep_mode_e {
	HDMIRX_SLEEP_MODE = 0,
	HDMIRX_WAKEUP_MODE,
};

struct hdmirx_sleep_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_sleep_mode_e mode;
};

/* HDMI DIAGNOSTICS */
enum hdmirx_link_type_e {
	HDMIRX_LINK_TYPE_TMDS = 0,
	HDMIRX_LINK_TYPE_FRL,
};

enum hdmirx_link_lane_number_e {
	HDMIRX_LINK_LANE_NUMBER_3 = 3,
	HDMIRX_LINK_LANE_NUMBER_4 = 4,
};

enum hdmirx_link_rate_e {
	HDMIRX_LINK_RATE_3G  = 3,
	HDMIRX_LINK_RATE_6G  = 6,
	HDMIRX_LINK_RATE_8G  = 8,
	HDMIRX_LINK_RATE_10G = 10,
	HDMIRX_LINK_RATE_12G = 12,

};

struct hdmirx_phy_status_s {
	enum hdmirx_input_port_e port;
	unsigned char lock_status;
	unsigned int tmds_clk_khz;

	enum hdmirx_link_type_e link_type;
	enum hdmirx_link_lane_number_e link_lane;
	enum hdmirx_link_rate_e link_rate;

	unsigned int ctle_eq_min_range[HDMIRX_TMDS_CH_NUM];
	unsigned int ctle_eq_max_range[HDMIRX_TMDS_CH_NUM];
	unsigned int ctle_eq_result[HDMIRX_TMDS_CH_NUM];
	unsigned int error[HDMIRX_TMDS_CH_NUM];
};

enum hdmirx_audio_format_e {
	HDMIRX_AUDIO_FORMAT_UNKNOWN	  = 0x00,
	HDMIRX_AUDIO_FORMAT_PCM		  = 0x01,
	HDMIRX_AUDIO_FORMAT_AC3		  = 0x10,
	HDMIRX_AUDIO_FORMAT_EAC3		 = 0x11,
	HDMIRX_AUDIO_FORMAT_EAC3_ATMOS   = 0x12,
	HDMIRX_AUDIO_FORMAT_MAT		  = 0x15,
	HDMIRX_AUDIO_FORMAT_MAT_ATMOS	= 0x16,
	HDMIRX_AUDIO_FORMAT_TRUEHD	   = 0x17,
	HDMIRX_AUDIO_FORMAT_TRUEHD_ATMOS = 0x18,
	HDMIRX_AUDIO_FORMAT_AAC		  = 0x19,
	HDMIRX_AUDIO_FORMAT_MPEG		 = 0x20,
	HDMIRX_AUDIO_FORMAT_DTS		  = 0x30,
	HDMIRX_AUDIO_FORMAT_DTS_HD_MA	= 0x31,
	HDMIRX_AUDIO_FORMAT_DTS_EXPRESS  = 0x32,
	HDMIRX_AUDIO_FORMAT_DTS_CD	   = 0x33,
	HDMIRX_AUDIO_FORMAT_NOAUDIO	  = 0x41,
};

enum hdmirx_hdr_mode_e {
	HDMIRX_HDR_MODE_SDR,
	HDMIRX_HDR_MODE_AMDV,
	HDMIRX_HDR_MODE_HDR10,
	HDMIRX_HDR_MODE_HLG,
	HDMIRX_HDR_MODE_TECHNICOLOR,
	HDMIRX_HDR_MODE_HDREFFECT,
	HDMIRX_HDR_MODE_MAX
};

struct hdmirx_link_status_s {
	enum hdmirx_input_port_e port;
	unsigned char hpd;
	unsigned char hdmi_5v;
	unsigned char rx_sense;
	unsigned int frame_rate_x100_hz;
	enum hdmirx_mode_e dvi_hdmi_mode;

	unsigned short video_width;
	unsigned short video_height;
	unsigned char interlaced;
	enum hdmirx_avi_csc_e color_space;
	unsigned char color_depth;
	enum hdmirx_avi_colorimetry_e colorimetry;
	enum hdmirx_avi_ext_colorimetry_e ext_colorimetry;
	enum hdmirx_avi_additional_colorimetry_e additional_colorimetry;
	enum hdmirx_hdr_mode_e hdr_type;

	enum hdmirx_audio_format_e audio_format;
	unsigned int audio_sampling_freq;
	unsigned char audio_channel_number;
};

struct hdmirx_video_status_s {
	enum hdmirx_input_port_e port;
	unsigned short video_width_real;
	unsigned short video_htotal_real;
	unsigned short video_height_real;
	unsigned short video_vtotal_real;
	unsigned int pixel_clock_khz;
	unsigned int current_vrr_refresh_rate;
};

struct hdmirx_audio_status_s {
	enum hdmirx_input_port_e port;
	unsigned int pcm_N;
	unsigned int pcm_CTS;
	unsigned char layoutbitvalue;
	unsigned char channelstatusbits;
};

enum hdmirx_hdcp_auth_status_e {
	HDMIRX_HDCP_AUTH_STATUS_NO_TX_CONNECTED = 0,
	HDMIRX_HDCP_AUTH_STATUS_UNAUTHENTICATED = 1,
	HDMIRX_HDCP_AUTH_STATUS_IN_PROGRESS	 = 2,
	HDMIRX_HDCP_AUTH_STATUS_AUTHENTICATED   = 3,
};

enum hdmirx_hdcp_version_e {
	HDMIRX_HDCP_VERSION_14 = 0,
	HDMIRX_HDCP_VERSION_22,
	HDMIRX_HDCP_VERSION_RESERVED,
};

struct hdmirx_hdcp_key_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_hdcp_version_e version;
	unsigned int key_size;
	union {
		unsigned char *pdata;
		unsigned int compat_data;
		unsigned long long sizer;
	};
};

struct hdmirx_hdcp14_status_s {
	enum hdmirx_input_port_e port;
	unsigned char an[8];
	unsigned char aksv[5];
	unsigned char bksv[5];
	unsigned char ri[2];
	unsigned char bcaps;
	unsigned char bstatus[2];
};

struct hdmirx_hdcp22_status_s {
	enum hdmirx_input_port_e port;
	unsigned short ake_init_count_since_5v;
	unsigned short reauth_req_count_since_5v;
};

struct hdmirx_hdcp_status_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_hdcp_version_e hdcp_version;
	enum hdmirx_hdcp_auth_status_e auth_status;
	unsigned char encen;
	struct hdmirx_hdcp14_status_s hdcp14_status;
	struct hdmirx_hdcp22_status_s hdcp22_status;
};

struct hdmirx_hdcp_repeater_topology_s {
	enum hdmirx_input_port_e port;	 // HDMI port number
	unsigned char repeater_mode;			// 0 : receiver, 1 : repeater
	unsigned char count;					// Number of devices
	unsigned char depth;					// Depth of connected device
	unsigned char receiver_id[32][5];	   // RX IO of connected device
	unsigned char repeater_hpd;			 // HPD control
	unsigned int msg_id;	// Increase by 1 when it changes
	//any value(init value =0, default set value = 1)
};

enum hdmirx_hdcp_repeater_stream_manage_transmit_e {
	HDMIRX_STREAM_MANAGE_TRANSMIT_ALLOW,
	HDMIRX_STREAM_MANAGE_TRANSMIT_DENY,
	HDMIRX_STREAM_MANAGE_RESERVED,
};

struct hdmirx_hdcp_repeater_stream_manage_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_hdcp_repeater_stream_manage_transmit_e value;	 // 0 or 1 or reserved
};

struct hdmirx_scdc_status_s {
	enum hdmirx_input_port_e port;
	unsigned char source_version;
	unsigned char sink_version;

	unsigned char rsed_update;
	unsigned char flt_update;
	unsigned char frl_start;
	unsigned char source_test_update;
	unsigned char rr_test;
	unsigned char ced_update;
	unsigned char status_update;

	unsigned char tmds_bit_clock_ratio;
	unsigned char scrambling_enable;
	unsigned char tmds_scrambler_status;

	unsigned char flt_no_retrain;
	unsigned char rr_enable;
	unsigned char ffe_levels;
	unsigned char frl_rate;

	unsigned char dsc_decode_fail;
	unsigned char flt_ready;
	unsigned char clk_detect;
	unsigned char ch0_locked;
	unsigned char ch1_locked;
	unsigned char ch2_locked;
	unsigned char ch3_locked;

	unsigned char lane0_ltp_request;
	unsigned char lane1_ltp_request;
	unsigned char lane2_ltp_request;
	unsigned char lane3_ltp_request;

	unsigned char ch0_ced_valid;
	unsigned char ch1_ced_valid;
	unsigned char ch2_ced_valid;
	unsigned char ch3_ced_valid;
	unsigned int ch0_ced;
	unsigned int ch1_ced;
	unsigned int ch2_ced;
	unsigned int ch3_ced;
	unsigned char rs_correction_valid;
	unsigned int rs_correction_count;
};

struct hdmirx_diagnostics_status_s {
	enum hdmirx_input_port_e port;
	struct hdmirx_link_status_s link_status;
	struct hdmirx_phy_status_s phy_status;
	struct hdmirx_video_status_s video_status;
	struct hdmirx_audio_status_s audio_status;
	struct hdmirx_hdcp_status_s hdcp_status;
	struct hdmirx_scdc_status_s scdc_status;
};

enum hdmirx_error_type_e {
	HDMIRX_ERROR_TYPE_NONE		  = 0x0000,
	HDMIRX_ERROR_TYPE_GCP_ERROR	 = 0x0001,
	HDMIRX_ERROR_TYPE_HDCP22_REAUTH = 0x0002,
	HDMIRX_ERROR_TYPE_TMDS_ERROR	= 0x0004,
	HDMIRX_ERROR_TYPE_PHY_LOW_RANGE = 0x0008,
	HDMIRX_ERROR_TYPE_PHY_ABNORMAL  = 0x0010,
	HDMIRX_ERROR_TYPE_CED_ERROR	 = 0x0020,
	HDMIRX_ERROR_TYPE_AUDIO_BUFFER  = 0x0040,
	HDMIRX_ERROR_TYPE_UNSTABLE_SYNC = 0x0080,
	HDMIRX_ERROR_TYPE_BCH		   = 0x0100,
	HDMIRX_ERROR_TYPE_FLT		   = 0x0200,
	HDMIRX_ERROR_TYPE_FAILED		= 0xFFFFFFFE,
};

struct hdmirx_error_status_e {
	enum hdmirx_input_port_e port;
	u32 error;
	unsigned int param1;
	unsigned int param2;
};

enum hdmirx_expert_setting_type_e {
	HDMIRX_EXPERT_SETTING_TYPE_HPD_LOW_DURATION = 0,

	HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_MODE,
	HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH0,
	HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH1,
	HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH2,
	HDMIRX_EXPERT_SETTING_TYPE_TMDS_MANUAL_EQ_CH3,
	HDMIRX_EXPERT_SETTING_TYPE_TMDS_EQ_PERIOD,

	HDMIRX_EXPERT_SETTING_TYPE_VIDEO_STABLE_COUNT,
	HDMIRX_EXPERT_SETTING_TYPE_AUDIO_STABLE_COUNT,

	HDMIRX_EXPERT_SETTING_TYPE_DISABLE_HDCP22,
	HDMIRX_EXPERT_SETTING_TYPE_REAUTH_HDCP22,

	HDMIRX_EXPERT_SETTING_TYPE_ON_TO_RXSENSE_TIME,
	HDMIRX_EXPERT_SETTING_TYPE_RXSENSE_TO_HPD_TIME,
	HDMIRX_EXPERT_SETTING_TYPE_PCD_SERIAL_RESISTOR
};

struct hdmirx_expert_setting_s {
	enum hdmirx_input_port_e port;
	enum hdmirx_expert_setting_type_e type;
	unsigned int param1;
	unsigned int param2;
	unsigned int param3;
};

unsigned int hdmirx_show_diagnostic_info(unsigned char *buf, int size);

//function
void get_hdmirx_timing_info(struct hdmirx_timing_info_s *info);
void get_hdmirx_drm_info(struct hdmirx_drm_info_s *info);
void get_hdmirx_vsi_info(struct hdmirx_vsi_info_s *info);
void get_hdmirx_spd_info(struct hdmirx_spd_info_s *info);
void get_hdmirx_avi_info(struct hdmirx_avi_info_s *info);
void get_hdmirx_dolby_hdr_info(struct hdmirx_dolby_hdr_s *info);
void get_hdmirx_edid_info(struct hdmirx_edid_s *info);
void set_hdmirx_edid_info(struct hdmirx_edid_s info);
void get_hdmirx_connect_state_info(struct hdmirx_connection_state_s *info);
void get_hdmirx_hpd_info(struct hdmirx_hpd_s *info);
void set_hdmirx_hpd_info(struct hdmirx_hpd_s info);
void get_hdmirx_vrr_info(struct hdmirx_vrr_frequency_s *info);
void get_hdmirx_emp_info(struct hdmirx_emp_info_s *info);
void get_hdmirx_querycap_info(struct hdmirx_querycap_s *info);
void get_hdmirx_phy_status_info(struct hdmirx_phy_status_s *info);
void get_hdmirx_link_status_info(struct hdmirx_link_status_s *info);
void get_hdmirx_video_status_info(struct hdmirx_video_status_s *info);
void get_hdmirx_audio_status_info(struct hdmirx_audio_status_s *info);
void get_hdmirx_hdcp_status_info(struct hdmirx_hdcp_status_s *info);
void get_hdmirx_scdc_status_info(struct hdmirx_scdc_status_s *info);
void get_hdmirx_error_status_info(struct hdmirx_error_status_e *info);
void set_hdmirx_expert_setting_info(struct hdmirx_expert_setting_s info);
void get_hdmirx_diagnosticts_status_info(struct hdmirx_diagnostics_status_s *info);
#endif
