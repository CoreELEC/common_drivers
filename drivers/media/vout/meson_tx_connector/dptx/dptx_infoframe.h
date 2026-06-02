/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_INFOFRAME_H__
#define __DPTX_INFOFRAME_H__

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_format_para.h>
#include "cea861.h"

struct dp_sdp_header {
	u8 HB0;
	u8 HB1;
	u8 HB2;
	u8 HB3;
} __packed;

struct dp_sdp {
	struct dp_sdp_header sdp_header;
	u8 data[DP_INFOFRAME_DB_MAX_SZ];
} __packed;

/* convert struct aux_pkt_sdp_param to pointer for AUX_PKT_SET_SDP and AUX_PKT_SET_SDP */
struct aux_pkt_sdp_param {
	u8 vc_id;
	enum infoframe_type type;
	u8 *data;
};

#define DP_SDP_AUDIO_INFOFRAME_HB2       0x1b
#define DP_SDP_NON_AUDIO_INFOFRAME_HB2   0x1d

/**
 * enum dp_pixelformat - drm DP Pixel encoding formats
 *
 * This enum is used to indicate DP VSC SDP Pixel encoding formats.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18]
 *
 * @DP_PIXELFORMAT_RGB: RGB pixel encoding format
 * @DP_PIXELFORMAT_YUV444: YCbCr 4:4:4 pixel encoding format
 * @DP_PIXELFORMAT_YUV422: YCbCr 4:2:2 pixel encoding format
 * @DP_PIXELFORMAT_YUV420: YCbCr 4:2:0 pixel encoding format
 * @DP_PIXELFORMAT_Y_ONLY: Y Only pixel encoding format
 * @DP_PIXELFORMAT_RAW: RAW pixel encoding format
 * @DP_PIXELFORMAT_RESERVED: Reserved pixel encoding format
 */
enum dp_pixelformat {
	DP_PIXELFORMAT_RGB = 0,
	DP_PIXELFORMAT_YUV444 = 0x1,
	DP_PIXELFORMAT_YUV422 = 0x2,
	DP_PIXELFORMAT_YUV420 = 0x3,
	DP_PIXELFORMAT_Y_ONLY = 0x4,
	DP_PIXELFORMAT_RAW = 0x5,
	DP_PIXELFORMAT_RESERVED = 0x6,
};

/**
 * enum dp_content_type - drm DP Content Type
 *
 * This enum is used to indicate DP VSC SDP Content Types.
 * It is based on DP 1.4 spec [Table 2-117: VSC SDP Payload for DB16 through
 * DB18]
 * CTA-861-G defines content types and expected processing by a sink device
 *
 * @DP_CONTENT_TYPE_NOT_DEFINED: Not defined type
 * @DP_CONTENT_TYPE_GRAPHICS: Graphics type
 * @DP_CONTENT_TYPE_PHOTO: Photo type
 * @DP_CONTENT_TYPE_VIDEO: Video type
 * @DP_CONTENT_TYPE_GAME: Game type
 */
enum dp_content_type {
	DP_CONTENT_TYPE_NOT_DEFINED = 0x00,
	DP_CONTENT_TYPE_GRAPHICS = 0x01,
	DP_CONTENT_TYPE_PHOTO = 0x02,
	DP_CONTENT_TYPE_VIDEO = 0x03,
	DP_CONTENT_TYPE_GAME = 0x04,
};

enum operation_mode {
	DP_AS_SDP_AVT_DYNAMIC_VTOTAL = 0x00,
	DP_AS_SDP_AVT_FIXED_VTOTAL = 0x01,
	DP_AS_SDP_FAVT_TRR_NOT_REACHED = 0x02,
	DP_AS_SDP_FAVT_TRR_REACHED = 0x03
};

u32 dptx_calc_bpp(enum colorimetry_format color, u8 colordepth);
u8 dptx_get_color_component_cnt(enum colorimetry_format color);
u8 dptx_get_mapped_bpc(enum hdmi_color_depth cd);
void dptx_avi_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct avi_infoframe *info);
void dptx_avi_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb);
void dptx_spd_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct spd_infoframe *info);
void dptx_spd_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb);
void dptx_audio_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id,
	struct audio_infoframe *info);
void dptx_audio_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb);
void dptx_drm_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct drm_infoframe *info);
void dptx_drm_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb);
void dptx_construct_sdp_header(struct dp_sdp_header *header, enum infoframe_type type);
#endif /* __DPTX_INFOFRAME_H__ */
