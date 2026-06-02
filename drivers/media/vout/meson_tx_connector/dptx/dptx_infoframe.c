// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include "dptx_infoframe.h"
#include "dptx_hw_opcode.h"

static void dptx_infoframe_send(struct dptx_hw_common *hw_comm, u8 vc_id,
	enum infoframe_type type, u8 *data);

/*
 * Function: dptx_construct_sdp_header
 * INFOFRAME SDP v1.2 shall be used to convey Audio INFOFRAME control information,
 * as specified in CEA-861-F and CEA-861.2.
 * Other INFOFRAME coding types, as specified in CEA-861-F, Table 5, and CEA-861.3,
 * shall use INFOFRAME SDP v1.3.
 */
void dptx_construct_sdp_header(struct dp_sdp_header *header, enum infoframe_type type)
{
	if (!header)
		return;

	header->HB0 = 0;
	header->HB1 = type;
	if (type == INFOFRAME_TYPE_AUDIO) {
		header->HB2 = DP_SDP_AUDIO_INFOFRAME_HB2;
		header->HB3 = 0x12 << 2;
	} else {
		header->HB2 = DP_SDP_NON_AUDIO_INFOFRAME_HB2;
		header->HB3 = 0x13 << 2;
	}
}

/*
 * Payload byte mapping of a non-audio CEA INFOFRAME.
 * v1.3 shall be used for non-audio CEA INFOFRAME encapsulation.
 * DB[0] -- CEA Header Byte 2 (INFOFRAME Version Number)
 * DB[1] -- CEA Header Byte 3 (Length of INFOFRAME)
 * DB[2] -- CEA Payload Byte 1
 * DB[3] -- CEA Payload Byte 2
 * DB[29] -- CEA Payload Byte 28
 */
void dptx_avi_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct avi_infoframe *info)
{
	struct dp_sdp sdp_info;
	u8 data[DP_INFOFRAME_TOTAL_SZ];

	if (!info) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	memset(data, 0, sizeof(data));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_AVI);
	avi_infoframe_pack(info, &data, sizeof(data));
	sdp_info.data[0] = info->version;
	sdp_info.data[1] = info->length;
	memcpy(&sdp_info.data[2], &data[4], AVI_INFOFRAME_SIZE); /* ignore the checksum */
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AVI, (u8 *)&sdp_info);
}

/* note that pb[0] is checksum byte */
void dptx_avi_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb)
{
	struct dp_sdp sdp_info;

	if (!hb || !pb) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_AVI);
	sdp_info.data[0] = hb[1];
	sdp_info.data[1] = hb[2];
	memcpy(&sdp_info.data[2], pb + 1, AVI_INFOFRAME_SIZE);
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AVI, (u8 *)&sdp_info);
}

void dptx_spd_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct spd_infoframe *info)
{
	struct dp_sdp sdp_info;
	u8 data[DP_INFOFRAME_TOTAL_SZ];

	if (!info) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_SPD, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	memset(data, 0, sizeof(data));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_SPD);
	spd_infoframe_pack(info, &data, sizeof(data));
	sdp_info.data[0] = info->version;
	sdp_info.data[1] = info->length;
	memcpy(&sdp_info.data[2], &data[4], SPD_INFOFRAME_SIZE); /* ignore the checksum */
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_SPD, (u8 *)&sdp_info);
}

/* note that pb[0] is checksum byte */
void dptx_spd_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb)
{
	struct dp_sdp sdp_info;

	if (!hb || !pb) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_SPD, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_SPD);
	sdp_info.data[0] = hb[1];
	sdp_info.data[1] = hb[2];
	memcpy(&sdp_info.data[2], pb + 1, SPD_INFOFRAME_SIZE);
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_SPD, (u8 *)&sdp_info);
}

/*
 * The DB1 ~ DBN (Data Byte 1 ~ Data Byte N), as specified in CEA-861-F, are mapped
 * to SDP DB0 ~ DB [N -1]. The unused bytes shall be zero-padded.
 * DB[0] -- CEA Payload Byte 1
 * DB[1] -- CEA Payload Byte 2
 * DB[27] -- CEA Payload Byte 28
 */
void dptx_audio_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id,
	struct audio_infoframe *info)
{
	struct dp_sdp sdp_info;
	u8 data[DP_INFOFRAME_TOTAL_SZ];

	if (!info) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	memset(data, 0, sizeof(data));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_AUDIO);
	audio_infoframe_pack(info, &data, sizeof(data));
	memcpy(&sdp_info.data, &data[4], AUDIO_INFOFRAME_SIZE); /* ignore the checksum */
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AUDIO, (u8 *)&sdp_info);
}

/* note that pb[0] is checksum byte */
void dptx_audio_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb)
{
	struct dp_sdp sdp_info;

	if (!hb || !pb) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_AUDIO);
	memcpy(&sdp_info.data, pb + 1, AUDIO_INFOFRAME_SIZE); /* ignore the checksum */
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_AUDIO, (u8 *)&sdp_info);
}

void dptx_drm_infoframe_set(struct dptx_hw_common *hw_comm, u8 vc_id, struct drm_infoframe *info)
{
	struct dp_sdp sdp_info;
	u8 data[DP_INFOFRAME_TOTAL_SZ];

	if (!info) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	memset(data, 0, sizeof(data));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_DRM);
	drm_infoframe_pack(info, &data, sizeof(data));
	sdp_info.data[0] = info->version;
	sdp_info.data[1] = info->length;
	/* ignore the checksum */
	memcpy(&sdp_info.data[2], &data[INFOFRAME_HEADER_SIZE], DRM_INFOFRAME_SIZE);
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_DRM, (u8 *)&sdp_info);
}

/* note that pb[0] is checksum byte */
void dptx_drm_infoframe_rawset(struct dptx_hw_common *hw_comm, u8 vc_id, u8 *hb, u8 *pb)
{
	struct dp_sdp sdp_info;

	if (!hb || !pb) {
		dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	memset(&sdp_info, 0, sizeof(sdp_info));
	dptx_construct_sdp_header(&sdp_info.sdp_header, INFOFRAME_TYPE_DRM);
	sdp_info.data[0] = hb[1];
	sdp_info.data[1] = hb[2];
	memcpy(&sdp_info.data[2], pb + 1, DRM_INFOFRAME_SIZE);
	dptx_infoframe_send(hw_comm, vc_id, INFOFRAME_TYPE_DRM, (u8 *)&sdp_info);
}

static void dptx_infoframe_send(struct dptx_hw_common *hw_comm, u8 vc_id,
	enum infoframe_type type, u8 *data)
{
	struct aux_pkt_sdp_param param = {vc_id, type, data};

	dptx_hw_cntl(&hw_comm->hw_base, AUX_PKT_SET_SDP, (void *)&param, NULL);
}
