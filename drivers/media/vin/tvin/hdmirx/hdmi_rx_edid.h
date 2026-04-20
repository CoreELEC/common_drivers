/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_EDID_H_
#define _HDMI_RX_EDID_H_

/* 2024.07.04 do edid reset to clear segment for edid 512 by edid intr */
//2024.07.18 fix hdmi repeater cert fail item
//2024.08.28 default enable auto edid
//2024.10.23 add clr edid seg
//2024.11.01 Optimize clear of edid segment
//2024.11.06 get edid size and frl
//2024.11.20 add new edid function
//2024.11.28 change the location for get edid size and frl
//2024.12.18 add qms function
//2025.01.13 no need rm data blk before use splice_data_blk_to_edid
//2025.05.14 add edid size check before data copy
//2025.06.18 add edid protect
//2025.07.23 update qms length
//2025.11.04 fix VRR range, update Freesync range
//2025.11.24 fix DTD offset error
//2025.12.15 update vrr range once
//2026.04.01 add mute before update edid
//2026.04.09 Pull HPD after both audio and tvserver have updated the EDID
//2026.05.21 Reduce stack memory usage of rpt_edid_multi_vdb_extraction
#define RX_EDID_H_VER "ver.2026/05/21"

#define EDID_EXT_BLK_OFF	128
#define EDID_BLK_SIZE		128
#define EDID_SIZE			512
#define EDID_TOTAL_SIZE		1536
#define EDID_BUF_SIZE	(EDID_SIZE * 2)

#define EDID_HDR_SIZE		7
#define EDID_HDR_HEAD_LEN	4
#define MAX_HDR_LUMI_LEN	3
#define MAX_EDID_BUF_SIZE	(EDID_SIZE * 6)
#define MAX_PORT_NUM		3
#define LATENCY_MAX		254
#define EDID_MAX_REFRESH_RATE 123 //No use for reference board
#define EDID_OFFSET_512 0x10100
/* for clear 512 edid segment */
#define EDID_WAIT_STABLE_MAX 5
#define EDID_RST_TIMEOUT 5
#define EDID_RESET_OFFSET0 18
/* CTA-861G Table 54~57 CTA data block tag codes */
/* tag code 0x0: reserved */
#define AUDIO_TAG 0x1
#define VIDEO_TAG 0x2
#define VENDOR_TAG 0x3
/* tag of HF-VSDB is the same as VSDB
 * so add an offset(0xF0) for HF-VSDB
 */
#define HF_VSDB_OFFSET 0xF0
#define HF_VENDOR_DB_TAG (VENDOR_TAG + HF_VSDB_OFFSET)
#define SPEAKER_TAG 0x4
/* VESA Display Transfer Characteristic Data Block */
#define VDTCDB_TAG 0x5
/* tag code 0x6: reserved */
#define USE_EXTENDED_TAG 0x7

/* extended tag code */
#define VCDB_TAG 0 /* video capability data block */
#define VSVDB_TAG 1 /* Vendor-Specific Video Data Block */
#define VSVDB_OFFSET	0xF0
#define FREESYNC_OFFSET 0xE0
#define VSDB_FREESYNC_TAG (VENDOR_TAG + FREESYNC_OFFSET)
#define FSDB_PAYLOAD_LEN 28
#define VSVDB_DV_TAG	((USE_EXTENDED_TAG << 8) + VSVDB_TAG)
#define VSVDB_HDR10P_TAG ((USE_EXTENDED_TAG << 8) + VSVDB_TAG + VSVDB_OFFSET)
#define VDDDB_TAG 2 /* VESA Display Device Data Block */
#define VVTBE_TAG 3 /* VESA Video Timing Block Extension */
#define EXTENDED_VCDB_TAG ((USE_EXTENDED_TAG << 8) + VCDB_TAG)
#define EXTENDED_VSVDB_TAG ((USE_EXTENDED_TAG << 8) + VSVDB_TAG)
#define EXTENDED_VDDDB_TAG ((USE_EXTENDED_TAG << 8) + VDDDB_TAG)
#define EXTENDED_VVTBE_TAG ((USE_EXTENDED_TAG << 8) + VVTBE_TAG)
/* extend tag code 0x4: Reserved for HDMI Video Data Block */
#define CDB_TAG 0x5 /* Colorimetry Data Block */
#define HDR_STATIC_TAG 6 /* HDR Static Metadata Data Block */
#define HDR_DYNAMIC_TAG 7 /* HDR Dynamic Metadata Data Block */
#define EXTENDED_CDB_TAG ((USE_EXTENDED_TAG << 8) + CDB_TAG)
#define EXTENDED_HDR_STATIC_TAG ((USE_EXTENDED_TAG << 8) + HDR_STATIC_TAG)
#define EXTENDED_HDR_DYNAMIC_TAG ((USE_EXTENDED_TAG << 8) + HDR_DYNAMIC_TAG)
/* extend tag code 8-12: reserved */
#define VFPDB_TAG 13 /* Video Format Preference Data Block */
#define Y420VDB_TAG 14 /* YCBCR 4:2:0 Video Data Block */
#define Y420CMDB_TAG 15 /* YCBCR 4:2:0 Capability Map Data Block */
#define EXTENDED_VFPDB_TAG ((USE_EXTENDED_TAG << 8) + VFPDB_TAG)
#define EXTENDED_Y420VDB_TAG ((USE_EXTENDED_TAG << 8) + Y420VDB_TAG)
#define EXTENDED_Y420CMDB_TAG ((USE_EXTENDED_TAG << 8) + Y420CMDB_TAG)
/* extend tag code 16: Reserved for CTA Miscellaneous Audio Fields */
#define VSADB_TAG 17 /* Vendor-Specific Audio Data Block */
/* extend tag code 18: Reserved for HDMI Audio Data Block */
#define HDMI_ADB_TAG 18
#define RCDB_TAG 19 /* Room Configuration Data Block */
#define SLDB_TAG 20	/* Speaker Location Data Block */
#define EXTENDED_VSADB_TAG ((USE_EXTENDED_TAG << 8) + VSADB_TAG)
#define EXTENDED_RCDB_TAG ((USE_EXTENDED_TAG << 8) + RCDB_TAG)
#define EXTENDED_SLDB_TAG ((USE_EXTENDED_TAG << 8) + SLDB_TAG)
/* extend tag code 21~31: Reserved */
#define IFDB_TAG 32 /* infoframe data block */
#define HF_EEODB 0x78 /* HDMI forum EDID extension override data block */
#define HDMI_VIC420_OFFSET 0x100
#define HDMI_3D_OFFSET 0x180
#define HDMI_VESA_OFFSET 0x200
#define EXTENDED_IFDB_TAG ((USE_EXTENDED_TAG << 8) + IFDB_TAG)
#define IFDB_PAYLOAD_LEN 7
#define EXTENDED_HF_EEODB ((USE_EXTENDED_TAG << 8) + HF_EEODB)

#define T7VTDB_TAG 0x22 /* DisplayID Type VII Video Timing Data Block */
#define T8VTDB_TAG 0x23 /* DisplayID Type VIII Video Timing Data Block */
#define T10VTDB_TAG 0x2a /* DisplayID Type X Video Timing Data Block */
#define SCDB_TAG 0x79 /* HDMI forum Sink Capabilities data block*/
#define SBTM_TAG 0x7a /* HDMI forum Source-Based Tone Mapping data block*/

/* eARC Rx Capabilities Data Structure version */
#define CAP_DS_VER 0x1
/* eARC Rx Capabilities Data Structure Maximum Length */
#define EARC_CAP_DS_MAX_LENGTH	256
/* eARC Rx Capability data structure End Marker */
#define EARC_CAP_DS_END_MARKER 0x00
#define EARC_CAP_BLOCK_MAX 3
#define DATA_BLK_MAX_NUM 32
#define TAG_MAX 0xFF
/* data block max length(include head): 0x1F+1 */
#define DB_LEN_MAX 32
/* short audio descriptor length */
#define SAD_LEN 3
#define BLK_LENGTH(a) ((a) & 0x1F)
/* maximum VSVDB length is VSVDB V0: 26bytes */
#define VSVDB_LEN	26
#define MAX_AUDIO_BLK_LEN 31
/* max pixel clk H or V active framerate */
#define MAX_PIXEL_CLK 600
#define MAX_H_ACTIVE 3840
#define MAX_V_ACTIVE 2160
#define MAX_FRAME_RATE 60
#define REFRESH_RATE 60

#define EDID_SIZE_256_PLUS_256 513
#define EDID_SIZE_512_PLUS_512 1025
#define EDID_SIZE_256_PLUS_512 769
#define EDID_SIZE_256_PLUS_256_PLUS_256 769
#define EDID_SIZE_256_PLUS_512_PLUS_256 1025
//#define EDID_SIZE_256_PLUS_512_PLUS_512 1281
#define VRR_OFFSET 10
#define ADD_FIELD_OFFSET 8
#define DSC_QMS_OFFSET 11
#define DSC_FIELD_OFFSET 13
#define END_OF_BLK(x) (((x) + 1) * EDID_BLK_SIZE - 1)

enum edid_type_e {
	EDID_TYPE_256_PLUS_256 = 0,
	EDID_TYPE_512_PLUS_512 = 1,
	EDID_TYPE_256_PLUS_512 = 2,
	EDID_TYPE_256_PLUS_256_PLUS_256 = 3,
	EDID_TYPE_256_PLUS_512_PLUS_256 = 4,
	EDID_TYPE_256_PLUS_512_PLUS_512 = 5,
	EDID_TYPE_MAX,
};

enum edid_audio_format_e {
	AUDIO_FORMAT_HEADER,
	AUDIO_FORMAT_LPCM,
	AUDIO_FORMAT_AC3,
	AUDIO_FORMAT_MPEG1,
	AUDIO_FORMAT_MP3,
	AUDIO_FORMAT_MPEG2,
	AUDIO_FORMAT_AAC,
	AUDIO_FORMAT_DTS,
	AUDIO_FORMAT_ATRAC,
	AUDIO_FORMAT_OBA,
	AUDIO_FORMAT_DDP,
	AUDIO_FORMAT_DTSHD,
	AUDIO_FORMAT_MAT,
	AUDIO_FORMAT_DST,
	AUDIO_FORMAT_WMAPRO,
};

enum edid_tag_e {
	EDID_TAG_NONE,
	EDID_TAG_AUDIO = 1,
	EDID_TAG_VIDEO = 2,
	EDID_TAG_VSDB = 3,
	EDID_TAG_HF_VSDB = 0xf3,
	EDID_TAG_HDR = ((0x7 << 8) | 6),
};

enum edid_delivery_mothed_e {
	EDID_DELIVERY_NULL,
	EDID_DELIVERY_ALL_PORT,
	EDID_DELIVERY_ONE_PORT,
};

union bit_rate_u {
	struct pcm_t {
		unsigned char size_16bit:1;
		unsigned char size_20bit:1;
		unsigned char size_24bit:1;
		unsigned char size_reserved:5;
	} pcm;
	unsigned char others;
};

struct edid_audio_block_t {
	unsigned char max_channel:3;
	unsigned char format_code:4;
	unsigned char fmt_code_reserved:1;
	union u_sr {
		unsigned char freq_list;
		struct s_sr {
			unsigned char freq_32khz:1;
			unsigned char freq_44_1khz:1;
			unsigned char freq_48khz:1;
			unsigned char freq_88_2khz:1;
			unsigned char freq_96khz:1;
			unsigned char freq_176_4khz:1;
			unsigned char freq_192khz:1;
			unsigned char freq_reserved:1;
		} ssr;
	} usr;
	union bit_rate_u bit_rate;
};

struct edid_hdr_block_t {
	unsigned char ext_tag_code;
	unsigned char sdr:1;
	unsigned char hdr:1;
	unsigned char smtpe_2048:1;
	unsigned char future:5;
	unsigned char meta_des_type1:1;
	unsigned char reserved:7;
	unsigned char max_lumi;
	unsigned char avg_lumi;
	unsigned char min_lumi;
};

enum edid_ver_e {
	EDID_V14 = 0x0,
	EDID_V21 = 0x1,
	EDID_AUTO20 = 0x2,
	EDID_AUTO14 = 0x4,
	EDID_V20 = 0x8
};

enum edid_support_e {
	HF_VRR,
	HF_ALLM,
	HF_QMS,
	HF_DB,
	VSDV_DB,
	HDR10P_DB,
	HDR_STATIC_DB,
	HDR_DYNAMIC_DB,
	FREESYNC_DB,
	TIMING_4K,
	SUP_MAX
};

struct detailed_timing_desc {
	unsigned int pixel_clk;
	unsigned int hactive;
	unsigned int vactive;
	unsigned int hblank;
	unsigned int vblank;
	unsigned int htotal;
	unsigned int vtotal;
	unsigned int hfront;
	unsigned int hsync_width;
	unsigned int vfront;
	unsigned int vsync_width;
	unsigned int framerate;
};

struct video_db_s {
	/* video data block: 31 bytes long maximum*/
	unsigned char svd_num;
	unsigned char hdmi_vic[31];
};

/* audio data block*/
struct audio_db_s {
	/* support for below audio format:
	 * 14 audio fmt + 1 header = 15
	 * uncompressed audio:lpcm
	 * compressed audio: others
	 */
	unsigned char aud_fmt_sup[15];
	/* short audio descriptor */
	struct edid_audio_block_t sad[15];
};

/* may need extend spker alloc from CTA-SPEC */
/* speaker allocation data block: 3 bytes*/
struct speaker_alloc_db_s {
	unsigned char flw_frw:1;
	unsigned char flc_frc:1;
	unsigned char bc:1;
	unsigned char bl_br:1;
	unsigned char fc:1;
	unsigned char lfe1:1;
	unsigned char fl_fr:1;

	unsigned char tpsil_tpsir:1;
	unsigned char sil_sir:1;
	unsigned char tpbc:1;
	unsigned char lfe2:1;
	unsigned char ls_rs:1;
	unsigned char tpfc:1;
	unsigned char tpc:1;
	unsigned char tpfl_tpfr:1;

	unsigned char tpbl_tpbr:1;
	unsigned char btfc:1;
	unsigned char btfl_btfr:1;
	unsigned char resrvd2:4;
};

struct specific_vic_3d {
	unsigned char _2d_vic_order:4;
	unsigned char _3d_struct:4;
	unsigned char _3d_detail:4;
	unsigned char reserved:4;
};

struct hdmi_3d_ad_s {
	u8 format:4;
	u8 max_channel_num:5;
	u8 freq_32khz:1;
	u8 freq_44_1khz:1;
	u8 freq_48khz:1;
	u8 freq_88_2khz:1;
	u8 freq_96khz:1;
	u8 freq_176_4khz:1;
	u8 freq_192khz:1;
	u8 bit24:1;
	u8 bit20:1;
	u8 bit16:1;
	u8 format_value;
};

struct hdmi_adb_s {
	u8 sup_ms_nonmixed:1;
	u8 max_stream_cnt:2;
	u8 num_3d_ad:3;
	struct hdmi_3d_ad_s hdmi_3d_ad[6];
	u8 acat:4;
	struct speaker_alloc_db_s sadb;
};

struct vsdb_s {
	u8 len:5;
	u8 tag:3;
	u8 ieee_third;
	u8 ieee_second;
	u8 ieee_first;
	//unsigned int ieee_oui;
	/* phy addr 2 bytes */
	unsigned char b:4;
	unsigned char a:4;
	unsigned char d:4;
	unsigned char c:4;

	/* Set if Sink supports DVI dual-link operation */
	unsigned char dvi_dual:1;
	unsigned char rsv_1:2;
	/* DC_Y444: Set if supports YCbCb4:4:4 Deep Color modes */
	unsigned char dc_y444:1;
	/* the three DC_XXbit bits above only indicate support
	 * for RGB4:4:4 at that pixel size.Support for YCbCb4:4:4
	 * in Deep Color modes is indicated with the DC_Y444 bit.
	 * If DC_Y444 is set, then YCbCr4:4:4 is supported for
	 * all modes indicated by the DC_XXbit flags.
	 */
	unsigned char dc_30bit:1;
	unsigned char dc_36bit:1;
	unsigned char dc_48bit:1;
	/* support_AI: Set to 1 if the Sink supports at least
	 * one function that uses information carried by the
	 * ACP, ISRC1, or ISRC2 packets.
	 */
	unsigned char support_ai:1;
	//pb4
	unsigned char max_tmds_clk;
	//pb5
	/* each bit indicates support for particular Content Type */
	unsigned char cnc0:1;/* graphics */
	unsigned char cnc1:1;/* photo*/
	unsigned char cnc2:1;/* cinema */
	unsigned char cnc3:1;/* game */
	unsigned char rsv_2:1;
	unsigned char hdmi_video_present:1;
	unsigned char i_latency_fields_present:1;
	unsigned char latency_fields_present:1;
	//pb6
	unsigned char video_latency;
	unsigned char audio_latency;
	unsigned char interlaced_video_latency;
	unsigned char interlaced_audio_latency;
	//pb10
	unsigned char resrvd3:3;
	unsigned char image_size:2;
	unsigned char _3d_multi_present:2;
	unsigned char _3d_present:1;
	//pb11
	unsigned char hdmi_3d_len:5;
	unsigned char hdmi_vic_len:3;
	unsigned char hdmi_4k2k_30hz_sup;
	unsigned char hdmi_4k2k_25hz_sup;
	unsigned char hdmi_4k2k_24hz_sup;
	unsigned char hdmi_smpte_sup;
	/*3D*/
	u16 _3d_struct_all;
	u16 _3d_mask_15_0;
	struct specific_vic_3d _2d_vic[16];
	unsigned char _2d_vic_num;
};

struct hf_s {
	//pb1
	unsigned char version;
	//pb2
	unsigned char max_tmds_rate;
	//pb3
	/* if set, the sink is capable of initiating an SCDC read request */
	unsigned char _3d_osd_disparity:1;
	unsigned char dual_view:1;
	unsigned char independ_view:1;
	unsigned char lte_340m_scramble:1;
	unsigned char ccbpci:1;
	unsigned char cable_status:1;
	unsigned char rr_cap:1;
	unsigned char scdc_present:1;
	//pb4
	unsigned char dc_30bit_420:1;
	unsigned char dc_36bit_420:1;
	unsigned char dc_48bit_420:1;
	unsigned char uhd_vic:1;
	unsigned char max_frl_rate:4;
	//pb5
	unsigned char fapa_start_location:1;
	unsigned char allm:1;
	unsigned char fva:1;
	unsigned char neg_mvrr:1;
	unsigned char cinema_vrr:1;
	unsigned char m_delta:1;
	unsigned char qms:1;
	unsigned char fapa_end_extended:1;
	//pb6
	unsigned char vrr_min:6;
	unsigned char vrr_max_hi:2;//bit[9:8]
	//pb7
	unsigned char vrr_max_lo;
	//pb8
	unsigned char dsc_10bpc:1;
	unsigned char dsc_12bpc:1;
	unsigned char dsc_16bpc:1;//=0
	unsigned char dsc_all_bpp:1;
	unsigned char qms_tfr_max:1;
	unsigned char qms_tfr_min:1;
	unsigned char dsc_native_420:1;
	unsigned char dsc_1p2:1;
	//pb9
	unsigned char dsc_max_slices:4;
	unsigned char dsc_max_frl_rate:4;
	//pb10
	unsigned char dsc_total_chunk_kbytes:6;
	unsigned char rsv_1:2;
	//pb11-28
	unsigned char rsv[18];
};

struct hf_vsdb_s {
	u8 len:5;
	u8 tag:3;
	u8 ieee_third;
	u8 ieee_second;
	u8 ieee_first;
	struct hf_s hf_db;
};

struct hf_scdb_s {
	u8 len:5;
	u8 tag:3;
	u8 resved0;
	u8 resved1;
	struct hf_s hf_db;
};

struct hf_sbtm_s {
	u8 len:5;
	u8 tag:3;
	u8 drdm_ind:1;
	u8 grdm_support:2;
	u8 resrvd0:1;
	u8 max_sbtm_ver:4;

	u8 gamut:2;
	u8 max_rgb:1;
	u8 use_hgig_drdm:1;
	u8 resrvd1:1;
	u8 hgig_cat_drdm_sel:3;

	u8 red_x_low;
	u8 red_x_high;
	u8 red_y_low;
	u8 red_y_high;
	u8 green_x_low;
	u8 green_x_high;
	u8 green_y_low;
	u8 green_y_high;
	u8 blue_x_low;
	u8 blue_x_high;
	u8 blue_y_low;
	u8 blue_y_high;
	u8 white_x_low;
	u8 white_x_high;
	u8 white_y_low;
	u8 white_y_high;
	u8 min_bright_10;
	u8 peak_bright_100;
	u8 p0_mant:6;
	u8 p0_exp:2;
	u8 peak_bright_p0;
	u8 p1_mant:6;
	u8 p1_exp:2;
	u8 peak_bright_p1;
	u8 p2_mant:6;
	u8 p2_exp:2;
	u8 peak_bright_p2;
	u8 p3_mant:6;
	u8 p3_exp:2;
	u8 peak_bright_p3;

};

struct rcdb_s {
	u8 display:1;
	u8 speaker:1;
	u8 sld:1;
	u8 speaker_cnt:5;
	struct speaker_alloc_db_s sadb;
	u8 x_max;
	u8 y_max;
	u8 z_max;
	u8 display_x;
	u8 display_y;
	u8 display_z;
};

struct sldb_des_s {
	u8 coord:1;
	u8 active:1;
	u8 channel_idx:5;
	u8 speaker_id:5;
	u8 x;
	u8 y;
	u8 z;
};

struct sldb_s {
	u8 len;
	struct sldb_des_s sldb_des[15];
};

struct colorimetry_db_s {
	unsigned char BT2020_RGB:1;
	unsigned char BT2020_YCC:1;
	unsigned char bt2020_cycc:1;
	unsigned char adobe_rgb:1;
	unsigned char adobe_ycc601:1;
	unsigned char sycc601:1;
	unsigned char xvycc709:1;
	unsigned char xvycc601:1;
	unsigned char dci_p3:1;
	unsigned char bt2100_ictcp:1;
	unsigned char srgb:1;
	unsigned char defaultrgb:1;
	/* MDX: designated for future gamut-related metadata. As yet undefined,
	 * this metadata is carried in an interface-specific way.
	 */
	unsigned char MD3:1;
	unsigned char MD2:1;
	unsigned char MD1:1;
	unsigned char MD0:1;
};

struct freesync_db_s {
	u8 payload[FSDB_PAYLOAD_LEN];
	u8 payload_len;
};

struct vfpdb_s {
	u8 num;
	u8 svr[31];
};

struct hdr_db_s {
	/* CEA861.3 table7-9 */
	/* ET_4 to ET_5 shall be set to 0. Future
	 * Specifications may define other EOTFs
	 */
	unsigned char resrvd1: 4;
	unsigned char eotf_hlg:1;
	/* SMPTE ST 2084[2] */
	unsigned char eotf_smpte_st_2084:1;
	/* Traditional gamma - HDR Luminance Range */
	unsigned char eotf_hdr:1;
	/* Traditional gamma - SDR Luminance Range */
	unsigned char eotf_sdr:1;

	/* Static Metadata Descriptors:
	 * SM_1 to SM_7  Reserved for future use
	 */
	unsigned char hdr_SMD_other_type:7;
	unsigned char hdr_SMD_type1:1;

	unsigned char hdr_lum_max;
	unsigned char hdr_lum_avg;
	unsigned char hdr_lum_min;
};

struct hdr_sup_s {
	u8 len;
	u16 type;
	u8 flag;
	u8 field_len;
	u8 field[31];
};

struct hdr_dy_db_s {
	u8 num;
	struct hdr_sup_s hdr_sup[31];
};

struct hdr_10p_s {
	u8 hdr10p_ver:2;
	u8 full_frame_peak:2;
	u8 peak:4;
};

struct video_cap_db_s {
	/* quantization range:
	 * 0: no data
	 * 1: selectable via AVI YQ/Q
	 */
	unsigned char quanti_range_ycc:1;
	unsigned char quanti_range_rgb:1;
	/* scan behaviour */
	/* s_PT
	 * 0: No Data (refer to S_CE or S_IT fields)
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_PT:2;
	/* s_IT
	 * 0: IT Video Formats not supported
	 * 1: always overscanned
	 * 2: always underscanned
	 * 3: support both
	 */
	unsigned char s_IT:2;
	unsigned char s_CE:2;
};

struct dv_vsadb_s {
	u8 version:3;
	u8 headphone:1;
	u8 center:1;
	u8 surround:1;
	u8 height:1;
	u8 func:1;
};

struct dv_vsvdb_s {
	unsigned int ieee_oui;
	unsigned char length:5;
	unsigned char version:3;
	unsigned char DM_version:3;
	unsigned char sup_2160p60hz:1;
	unsigned char sup_yuv422_12bit:1;

	unsigned char target_max_lum:7;
	unsigned char sup_global_dimming:1;

	unsigned char target_min_lum:7;
	unsigned char colorimetry:1;
	unsigned char low_latency:2;
	unsigned char backlt_min_luma:2;
	unsigned char interface:2;
	unsigned char sup_10b_12b_444:2;
	unsigned char reserved;
	u16 Rx;
	u16 Ry;
	u16 Gx;
	u16 Gy;
	u16 Bx;
	u16 By;
	u16 Wx;
	u16 Wy;
	u16 tminPQ;
	u16 tmaxPQ;
	u8 dm_major_ver;
	u8 dm_minor_ver;
};

struct short_info_s {
	u8 payload_len:3;
	u8 type_code:5;
	u8 payload[IFDB_PAYLOAD_LEN];
};

struct short_vs_info_s {
	u8 payload_len:3;
	u32 ieee_oui;
	u8 payload[IFDB_PAYLOAD_LEN];
};

struct info_db_s {
	u8 vsif_num;
	u8 payload_len:3;
	u8 payload[IFDB_PAYLOAD_LEN];
	struct short_info_s short_info[31];
	struct short_vs_info_s short_vs_info[31];
};

struct t7vtdb_s {
	u8 t7_m:3;
	u8 dsc_pt:1;
	u8 block_revision:3;
	u32 pixel_clk;
	u8 t7y420:1;
	u8 _3d_sup:2;
	u8 t7il:1;
	u8 t7_aspect_ratio:4;
	u16 hactive;
	u16 hblank;
	u16 hoffset;
	u8 t7hsp:1;
	u16 hsync_width;
	u16 vactive;
	u16 vblank;
	u16 voffset;
	u8 t7vsp:1;
	u16 vsync_width;
	u8 fresh_rate;
};

struct t8vtdb_s {
	u8 code_tyoe:2;
	u8 t8y420:1;
	u8 tcs:1;
	u8 revision:3;
	u8 timing_num;
	u16 dmt_timing[31];
};

struct t10_desc_s {
	u8 t10_y420:1;
	u8 _3d_sup:2;
	u8 vrhb:1;
	u8 timing_formula:3;
	u16 hactive;
	u16 vactive;
	u16 fresh_rate;
};

struct t10vtdb_s {
	u8 t10_m:3;
	u8 block_revision:3;
	u8 desc_num;
	struct t10_desc_s t10_desc[4];
};

struct cta_data_blk_info {
	unsigned char cta_blk_index;
	u16 tag_code;
	unsigned char offset;
	unsigned char blk_len;
};

struct cta_blk_parse_info {
	/* audio data block */
	bool contain_vdb;
	u8 video_db_num;
	struct video_db_s video_db[10];
	/* audio data block */
	bool contain_adb;
	u8 audio_db_num;
	struct audio_db_s audio_db[10];
	/* speaker allocation data block */
	bool contain_sadb;
	struct speaker_alloc_db_s speaker_alloc;
	/* vendor specific data block */
	bool contain_vsdb;
	struct vsdb_s vsdb;
	/* hdmi forum vendor specific data block */
	bool contain_hf_vsdb;
	struct hf_vsdb_s hf_vsdb;
	/* hdmi forum sink capabilities data block*/
	bool contain_hf_scdb;
	struct hf_scdb_s hf_scdb;
	/* hdmi forum source-based tone mapping data block*/
	bool contain_hf_sbtm;
	struct hf_sbtm_s hf_sbtm;
	/* room configuration data block*/
	bool contain_rcdb;
	struct rcdb_s rcdb;
	/* speaker location data block */
	bool contain_sldb;
	struct sldb_s sldb;
	/* video capability data block */
	bool contain_vcdb;
	struct video_cap_db_s vcdb;
	/* hdmi audio(Multi-Stream/3D) */
	bool contain_hdmi_aud;
	struct hdmi_adb_s hdmi_adb;
	/* vendor specific video data block - dolby vision */
	bool contain_vsvdb;
	struct dv_vsvdb_s dv_vsvdb;
	/* vendor specific audio data block - dolby vision */
	bool contain_dvsadb;
	struct dv_vsadb_s dv_vsadb;
	/* colorimetry data block */
	bool contain_cdb;
	struct colorimetry_db_s color_db;
	/* HDR Static Metadata Data Block */
	bool contain_hdr_db;
	struct hdr_db_s hdr_db;
	/* HDR Dynamic Metadata Data Block */
	bool contain_hdr_dy;
	struct hdr_dy_db_s hdr_dy_db;
	/* Video Format Preference Data Block */
	bool contain_vfpdb;
	struct vfpdb_s vfpdb;
	/* Y420 video data block: 6 SVD maximum:
	 * y420vdb support only 4K50/60hz, smpte50/60hz
	 * 4K50/60hz format aspect ratio: 16:9, 64:27
	 * smpte50/60hz 256:135
	 */
	bool contain_y420_vdb;
	unsigned char y420_vic_len;
	unsigned char y420_vdb_vic[6];
	/* Y420 Capability Map Data Block: 31 SVD maximum */
	bool contain_y420_cmdb;
	unsigned char y420_all_vic;
	unsigned char y420_cmdb_vic[31];

	bool contain_vsadb;
	unsigned int vsadb_ieee;
	/* CTA-861-G 7.5.8 27-2-3 */
	unsigned char vsadb_payload[22];
	/*InfoFrame Data Block*/
	bool contain_ifdb;
	struct info_db_s info_db;
	/* HDR10+ */
	bool contain_hdr10p;
	struct hdr_10p_s hdr10pdb;
	/* freesync block */
	bool contain_fsdb;
	struct freesync_db_s fsdb;
	/* DisplayID Type VIII Video Timing Data Block */
	bool contain_t7vtdb;
	u8 t7vtdbdb_num;
	struct t7vtdb_s t7vtdb[15];
	/* DisplayID Type VIII Video Timing Data Block */
	bool contain_t8vtdb;
	u8 t8vtdbdb_num;
	struct t8vtdb_s t8vtdb[15];
	/* DisplayID Type VX Video Timing Data Block */
	bool contain_t10vtdb;
	u8 t10vtdbdb_num;
	struct t10vtdb_s t10vtdb[15];
	/*dtd*/
	struct detailed_timing_desc dtd[15];
	u8 dtd_num;
	unsigned char data_blk_num;
	struct cta_data_blk_info db_info[DATA_BLK_MAX_NUM];
};

/* CEA extension */
struct cea_ext_parse_info {
	/* CEA header */
	unsigned char cea_tag;
	unsigned char cea_revision;
	unsigned char dtd_offset;
	unsigned char underscan_sup:1;
	unsigned char basic_aud_sup:1;
	unsigned char ycc444_sup:1;
	unsigned char ycc422_sup:1;
	unsigned char native_dtd_num:4;

	struct cta_blk_parse_info blk_parse_info;
	u8 free_size;
	u8 total_free_size;
	u8 dtd_size;
};

struct color_characteristic {
	u16 red_x:10;
	u16 red_y:10;
	u16 green_x:10;
	u16 green_y:10;
	u16 blue_x:10;
	u16 blue_y:10;
	u16 white_x:10;
	u16 white_y:10;
};

struct basic_display_param {
	/* video input definition */
	u8 video_signal:1;
	u8 signal_level_standard:2;
	u8 video_setup:1;
	u8 sync_type:3;
	u8 serrations:1;

	u8 color_bit_depth:3;
	u8 digital_support:4;

	u8 horizontal_val;
	u8 vertical_val;
	u8 gamma;
	u8 display_power:3;
	u8 display_color_type:2;
	u8 other_feature:3;
};

struct edid_info_s {
	/* 8 */
	unsigned char manufacturer_name[3];
	/* 10 */
	unsigned int product_code;
	/* 12 */
	unsigned int serial_number;
	/* 16 */
	unsigned char product_week;
	unsigned int product_year;
	/* unsigned char edid_version; */
	/* unsigned char edid_revision; */
	struct basic_display_param display_param;
	struct color_characteristic color_chara;
	/* 54 + 18 * x */
	struct detailed_timing_desc descriptor1;
	struct detailed_timing_desc descriptor2;
	/* 54 + 18 * x */
	unsigned char monitor_name[13];
	unsigned int max_sup_pixel_clk;
	u8 extension_flag;
	/* 127 */
	unsigned char block0_chk_sum;
	unsigned char extension_block_cnt;
	struct cea_ext_parse_info cea_ext_info;
	struct cea_ext_parse_info cea_ext_info1;
	struct cea_ext_parse_info cea_ext_info2; //debug
	//struct dp_parse_info dp_info;
};

struct edid_standard_timing {
	u8 h_active[8];
	u8 aspect_ratio[8];
	u8 refresh_rate[8];
};

struct cta_blk_pair {
	u16 tag_code;
	char *blk_name;
};

struct edid_data_s {
	unsigned char edid[256];
	unsigned int physical[4];
	unsigned char phy_offset;
	unsigned int checksum;
};

enum tx_hpd_event_e {
	E_IDLE = 0,
	E_EXE = 1,
	E_RCV = 2,
};

struct cap_block_s {
	unsigned char block_id;
	unsigned char payload_len;
	unsigned char offset;
};

struct earc_cap_ds {
	unsigned char cap_ds_len;
	unsigned char cap_ds_ver;
	struct cap_block_s cap_block[EARC_CAP_BLOCK_MAX];
	struct cta_blk_parse_info blk_parse_info;
};

enum hdmi_vic_e {
	/* Refer to CEA 861-I */
	HDMI_UNKNOWN = 0,
	HDMI_640x480p60 = 1,
	/* for video format which have two different
	 * aspect ratios, VICs list below that don't
	 * indicate aspect ratio, its aspect ratio
	 * is default. e.g:
	 * HDMI_480p60, means 480p60_4x3
	 * HDMI_720p60, means 720p60_16x9
	 * HDMI_1080p50, means 1080p50_16x9
	 */
	HDMI_480p60 = 2,
	HDMI_480p60_16x9 = 3,
	HDMI_720p60 = 4,
	HDMI_1080i60 = 5,
	HDMI_480i60 = 6,
	HDMI_480i60_16x9 = 7,
	HDMI_720x240p60 = 8,
	HDMI_720x240p60_16x9 = 9,
	HDMI_2880x480i60 = 10,
	HDMI_2880x480i60_16x9 = 11,
	HDMI_2880x240p60 = 12,
	HDMI_2880x240p60_16x9 = 13,
	HDMI_1440x480p60 = 14,
	HDMI_1440x480p60_16x9 = 15,
	HDMI_1080p60 = 16,
	HDMI_576p50 = 17,
	HDMI_576p50_16x9 = 18,
	HDMI_720p50 = 19,
	HDMI_1080i50 = 20,
	HDMI_576i50 = 21,
	HDMI_576i50_16x9 = 22,
	HDMI_1440x288p50 = 23,
	HDMI_1440x288p50_16x9 = 24,
	HDMI_2880x576i50 = 25,
	HDMI_2880x576i50_16x9 = 26,
	HDMI_2880x288p50 = 27,
	HDMI_2880x288p50_16x9 = 28,
	HDMI_1440x576p50 = 29,
	HDMI_1440x576p50_16x9 = 30,
	HDMI_1080p50 = 31,
	HDMI_1080p24 = 32,
	HDMI_1080p25 = 33,
	HDMI_1080p30 = 34,
	HDMI_2880x480p60 = 35,
	HDMI_2880x480p60_16x9 = 36,
	HDMI_2880x576p50 = 37,
	HDMI_2880x576p50_16x9 = 38,
	HDMI_1080i50_1250 = 39,
	HDMI_1080i100 = 40,
	HDMI_720p100 = 41,
	HDMI_576p100 = 42,
	HDMI_576p100_16x9 = 43,
	HDMI_576i100 = 44,
	HDMI_576i100_16x9 = 45,
	HDMI_1080i120 = 46,
	HDMI_720p120 = 47,
	HDMI_480p120 = 48,
	HDMI_480p120_16x9 = 49,
	HDMI_480i120 = 50,
	HDMI_480i120_16x9 = 51,
	HDMI_576p200 = 52,
	HDMI_576p200_16x9 = 53,
	HDMI_576i200 = 54,
	HDMI_576i200_16x9 = 55,
	HDMI_480p240 = 56,
	HDMI_480p240_16x9 = 57,
	HDMI_480i240 = 58,
	HDMI_480i240_16x9 = 59,
	/* Refer to CEA 861-I */
	HDMI_720p24 = 60,
	HDMI_720p25 = 61,
	HDMI_720p30 = 62,
	HDMI_1080p120 = 63,
	HDMI_1080p100 = 64,
	HDMI_720p24_64x27 = 65,
	HDMI_720p25_64x27 = 66,
	HDMI_720p30_64x27 = 67,
	HDMI_720p50_64x27 = 68,
	HDMI_720p60_64x27 = 69,
	HDMI_720p100_64x27 = 70,
	HDMI_720p120_64x27 = 71,
	HDMI_1080p24_64x27 = 72,
	HDMI_1080p25_64x27 = 73,
	HDMI_1080p30_64x27 = 74,
	HDMI_1080p50_64x27 = 75,
	HDMI_1080p60_64x27 = 76,
	HDMI_1080p100_64x27 = 77,
	HDMI_1080p120_64x27 = 78,
	HDMI_1680x720p24_64x27 = 79,
	HDMI_1680x720p25_64x27 = 80,
	HDMI_1680x720p30_64x27 = 81,
	HDMI_1680x720p50_64x27 = 82,
	HDMI_1680x720p60_64x27 = 83,
	HDMI_1680x720p100_64x27 = 84,
	HDMI_1680x720p120_64x27 = 85,
	HDMI_2560x1080p24_64x27 = 86,
	HDMI_2560x1080p25_64x27 = 87,
	HDMI_2560x1080p30_64x27 = 88,
	HDMI_2560x1080p50_64x27 = 89,
	HDMI_2560x1080p60_64x27 = 90,
	HDMI_2560x1080p100_64x27 = 91,
	HDMI_2560x1080p120_64x27 = 92,
	/* 3840*2160 */
	HDMI_2160p24_16x9 = 93,
	HDMI_2160p25_16x9 = 94,
	HDMI_2160p30_16x9 = 95,
	HDMI_2160p50_16x9 = 96,
	HDMI_2160p60_16x9 = 97,
	/* 4096*2160 */
	HDMI_4096p24_256x135 = 98,
	HDMI_4096p25_256x135 = 99,
	HDMI_4096p30_256x135 = 100,
	HDMI_4096p50_256x135 = 101,
	HDMI_4096p60_256x135 = 102,
	/* 3840*2160 */
	HDMI_2160p24_64x27 = 103,
	HDMI_2160p25_64x27 = 104,
	HDMI_2160p30_64x27 = 105,
	HDMI_2160p50_64x27 = 106,
	HDMI_2160p60_64x27 = 107,
	HDMI_1280x720p48_16x9 = 108,
	HDMI_1280x720p48_64x27 = 109,
	HDMI_1680x720p48 = 110,
	HDMI_1920x1080p48_16x9 = 111,
	HDMI_1920x1080p48_64x27 = 112,
	HDMI_2560x1080p48 = 113,
	HDMI_3840x2160p48 = 114,
	HDMI_4090x2160p48 = 115,
	HDMI_3840x2160p48_64x27 = 116,
	HDMI_3840x2160p100_16x9 = 117,
	HDMI_3840x2160p120_16x9 = 118,
	HDMI_3840x2160p100_64x27 = 119,
	HDMI_3840x2160p120_64x27 = 120,
	HDMI_5120x2160p24_64x27 = 121,
	HDMI_5120x2160p25_64x27 = 122,
	HDMI_5120x2160p30_64x27 = 123,
	HDMI_5120x2160p48_64x27 = 124,
	HDMI_5120x2160p50_64x27 = 125,
	HDMI_5120x2160p60_64x27 = 126,
	HDMI_5120x2160p100_64x27 = 127,
	/* VIC 128~192: Reserved for the Future */
	HDMI_5120x2160p120_64x27 = 193,
	HDMI_7680x4320p24_16x9 = 194,
	HDMI_7680x4320p25_16x9 = 195,
	HDMI_7680x4320p30_16x9 = 196,
	HDMI_7680x4320p48_16x9 = 197,
	HDMI_7680x4320p50_16x9 = 198,
	HDMI_7680x4320p60_16x9 = 199,
	HDMI_7680x4320p100_16x9 = 200,
	HDMI_7680x4320p120_16x9 = 201,
	HDMI_7680x4320p24_64x27 = 202,
	HDMI_7680x4320p25_64x27 = 203,
	HDMI_7680x4320p30_64x27 = 204,
	HDMI_7680x4320p48_64x27 = 205,
	HDMI_7680x4320p50_64x27 = 206,
	HDMI_7680x4320p60_64x27 = 207,
	HDMI_7680x4320p100_64x27 = 208,
	HDMI_7680x4320p120_64x27 = 209,
	HDMI_10240x4320p24_64x27 = 210,
	HDMI_10240x4320p25_64x27 = 211,
	HDMI_10240x4320p30_64x27 = 212,
	HDMI_10240x4320p48_64x27 = 213,
	HDMI_10240x4320p50_64x27 = 214,
	HDMI_10240x4320p60_64x27 = 215,
	HDMI_10240x4320p100_64x27 = 216,
	HDMI_10240x4320p120_64x27 = 217,
	HDMI_3840x2160p100_256x135 = 218,
	HDMI_3840x2160p120_256x135 = 219,

	/* CTA Non-standard resolution */
	HDMI_1920x2160p60_16x9,
	HDMI_960x540,
	HDMI_3840x4320,
	HDMI_5120x2880,
	HDMI_2560x2880,
	HDMI_1440x240,
	HDMI_1440x480i60,
	HDMI_1440x576i50,
	HDMI_360x480i,
	HDMI_360x576i,
	HDMI_360x480p,
	HDMI_360x576p,
	HDMI_3840x1080p60,
	HDMI_2560x2160,
	HDMI_3072x2160,

	/* the following VICs are for y420 mode,
	 * they are fake VICs that used to diff
	 * from non-y420 mode, and have same VIC
	 * with normal VIC in the lower bytes.
	 */
	HDMI_VIC_Y420 = HDMI_VIC420_OFFSET,
	HDMI_2160p50_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p50_16x9,
	HDMI_2160p60_16x9_Y420	=
		HDMI_VIC420_OFFSET + HDMI_2160p60_16x9,
	HDMI_4096p50_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p50_256x135,
	HDMI_4096p60_256x135_Y420 =
		HDMI_VIC420_OFFSET + HDMI_4096p60_256x135,
	HDMI_2160p50_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p50_64x27,
	HDMI_2160p60_64x27_Y420 =
		HDMI_VIC420_OFFSET + HDMI_2160p60_64x27,
	HDMI_1080p_420,
	HDMI_VIC_Y420_MAX,

	HDMI_VIC_3D = HDMI_3D_OFFSET,
	HDMI_480p_FRAMEPACKING,
	HDMI_576p_FRAMEPACKING,
	HDMI_720p_FRAMEPACKING,
	HDMI_1080i_ALTERNATIVE,
	HDMI_1080i_FRAMEPACKING,
	HDMI_1080p_ALTERNATIVE,
	HDMI_1080p_FRAMEPACKING,

	HDMI_640_350 = HDMI_VESA_OFFSET,
	HDMI_640_400,
	HDMI_800_600,
	HDMI_848_480,
	HDMI_1024_768,
	HDMI_720_400,
	HDMI_720_350,
	HDMI_1280_768,
	HDMI_1280_800,
	HDMI_1280_960,
	HDMI_1280_1024,
	HDMI_1360_768,
	HDMI_1366_768,
	HDMI_1600_900,
	HDMI_1600_1200,
	HDMI_1792_1344,
	HDMI_1856_1392,
	HDMI_1920_1200,
	HDMI_1920_1440,
	HDMI_1440_900,
	HDMI_1400_1050,
	HDMI_1680_1050,
	HDMI_2048_1152,
	HDMI_1152_864,
	HDMI_3840_600,
	HDMI_2560_1440,
	HDMI_2560_1600,
	HDMI_2688_1520,
	HDMI_1920_1080_INTERLACED,//1920*1080*interlace
	HDMI_2048_1080,
	HDMI_UNSUPPORT,
};

/* CTA-861 I table-11 */
enum hdmi_rid_e {
	RID_NULL = 0,
	RID_1280X720P_16x9 = 1,
	RID_1280X720P_64x27 = 2,
	RID_1680x720p_64x27 = 3,
	RID_1920x1080p_16x9 = 4,
	RID_1920x1080p_64x27 = 5,
	RID_2560x1080p_64x27 = 6,
	RID_3840x1080p_32x9 = 7,
	RID_2560x14440_16x9 = 8,
	RID_3440x1440p_64x27 = 9,
	RID_5120x1440p_32x9 = 10,
	RID_3840x2160p_16x9 = 11,
	RID_3840x2160p_64x27 = 12,
	RID_5120x2160p_64x27 = 13,
	RID_7680x2160p_32x9 = 14,
	RID_5120x2880p_16x9 = 15,
	RID_5120x2880p_64x27 = 16,
	RID_6880x2880p_64x27 = 17,
	RID_10240x2880p_32x9 = 18,
	RID_7680x4320p_16x9 = 19,
	RID_7680x4320p_64x27 = 20,
	RID_10240x4320p_64x27 = 21,
	RID_15360x4320p_32x9 = 22,
	RID_11520x6480p_16x9 = 23,
	RID_11520x6480p_64x27 = 24,
	RID_15360x6480p_64x27 = 25,
	RID_15360x8640p_16x9 = 26,
	RID_15360x8640p_64x27 = 27,
	RID_20480x8640p_64x27 = 28,
	/* 29-62 reserved future use */
};

enum hdmi_aspect_ratio_e {
	HDMI_ASPECT_RATIO_NULL,
	HDMI_ASPECT_RATIO_4X3,
	HDMI_ASPECT_RATIO_16X9,
	HDMI_ASPECT_RATIO_32x9,
	HDMI_ASPECT_RATIO_64X27,
	HDMI_ASPECT_RATIO_256X135
};

/* CTA 861 Table-24 */
enum hdmi_active_aspect_ratio_e {
	ACTIVE_ASPECT_RATIO_NULL = 0,
	ACTIVE_ASPECT_RATIO_AS_PIC = 8,
	ACTIVE_ASPECT_RATIO_4X3 = 9,
	ACTIVE_ASPECT_RATIO_16X9 = 10,
	ACTIVE_ASPECT_RATIO_14X9 = 11,
};

struct vic_aspect_ratio_s {
	enum hdmi_vic_e vic;
	enum hdmi_aspect_ratio_e pic_aspect_ratio;
};

struct rid_aspect_ratio_s {
	enum hdmi_rid_e rid;
	enum hdmi_aspect_ratio_e pic_aspect_ratio;
};

struct data_block_location_s {
	u8 num;
	unsigned int pos[10];
};

enum earc_cap_block_id {
	EARC_CAP_BLOCK_ID_0 = 0,
	EARC_CAP_BLOCK_ID_1 = 1,
	EARC_CAP_BLOCK_ID_2 = 2,
	EARC_CAP_BLOCK_ID_3 = 3
};

enum rx_edid_selection {
	//rx default edid,product:tv+tx+none_cec
	use_edid_def = 1,
	//edid extraction,product:standard repeater
	use_edid_repeater = 2,
	//edid extraction,product:repeater+audio block passthrough
	use_edid_repeater_sad_passthrough = 3,
	//product:rx edid+audio block passthrough+secondary phyaddr
	use_edid_def_sad_passthrough_secondary_phyaddr = 4,
	//product:rx edid+secondary phyaddr
	use_edid_def_secondary_phyaddr = 5,
};

enum edid_states_e {
	EDID_WAIT_READ_DONE,
	EDID_WAIT_OTHER_PORT_IDLE,
	EDID_RESET_DONE,
};

extern u8 port_hpd_rst_flag;
extern int phy_addr_map;
extern u32 en_take_dtd_space;
extern u32 earc_cap_ds_update_hpd_en;
extern unsigned int edid_select;
extern u32 vsvdb_update_hpd_en;
extern enum edid_delivery_mothed_e edid_delivery_mothed;
extern unsigned int edid_reset_max;
extern u8 edid_port_type[4];
extern u8 cta_flag[4];
extern bool sad_set_hpd;

#ifdef CONFIG_AMLOGIC_HDMITX
extern u32 tx_hdr_priority;
#endif
//edid auto start
void rx_clr_edid_type(unsigned char port);
void edid_type_init(void);
void edid_type_update(u8 port);
//edid auto end
bool get_basic_dtd_data(u8 *p_edid, struct edid_info_s *edid_info);
int rx_set_hdr_lumi(unsigned char *data, int len);
void rx_edid_physical_addr(int a, int b, int c, int d);
unsigned char rx_parse_arc_aud_type(const unsigned char *buff);
bool hdmi_rx_top_edid_update(u8 port);
unsigned char rx_get_edid_index(void);
unsigned char *rx_get_edid(int edid_index);
void edid_parse_block0(u8 *p_edid, struct edid_info_s *edid_info);
void edid_parse_cea_ext_block(u8 *p_edid,
			      struct cea_ext_parse_info *blk_parse_info);
u8 rx_edid_ext_blk_num(u8 *p_edid);
u8 rx_edid_cta_blk_num(u8 *p_edid);
bool rx_edid_parse(u8 *p_edid, struct edid_info_s *edid_info);
void rx_edid_parse_print(struct edid_info_s *edid_info);
void rx_blk_index_print(struct cta_blk_parse_info *blk_info);
int rx_get_cta_free_size(u8 *cur_edid, unsigned int size);
unsigned char rx_edid_total_free_size(unsigned char *cur_edid,
				      unsigned int size);
unsigned char rx_get_cea_dtd_size(unsigned char *cur_edid,
	unsigned int size);
bool rx_set_earc_cap_ds(unsigned char *data, unsigned int len);
void rx_prase_earc_capds_dbg(void);
void edid_splice_earc_capds(unsigned char *p_edid,
			    unsigned char *earc_cap_ds,
			    unsigned int len);
bool get_edid_support(u_char *edid_buf, enum edid_support_e func);
void edid_splice_earc_capds_dbg(u8 *p_edid);
void edid_splice_data_blk_dbg(u8 *p_edid, u8 idx);
void edid_rm_db_by_tag(u8 *p_edid, u16 tagid);
void edid_rm_db_by_idx(u8 *p_edid, u8 blk_idx);
void splice_tag_db_to_edid(u8 *p_edid, u8 *add_buf,
			   u16 tagid);
void splice_data_blk_to_edid(u_char *p_edid, u_char *add_buf,
			     u_char blk_idx);
void rx_modify_edid(unsigned char *buffer,
		    int len, unsigned char *addition);
void rx_edid_update_audio_info(unsigned char *p_edid,
			       unsigned int len);
bool is_ddc_idle(unsigned char port_id);
void rx_edid_reset(u8 port);
bool is_edid_buff_normal(unsigned char port_id);
bool need_update_edid(u8 port);
enum edid_ver_e get_edid_selection(u8 port);
enum edid_ver_e rx_parse_edid_ver(u8 *p_edid);
u_char *rx_get_cur_def_edid(u_char port);
u_char *rx_get_cur_used_edid(u_char port);

bool rx_set_vsvdb(unsigned char *data, unsigned int len);
u_char rx_edid_get_aud_sad(u_char *sad_data);
bool rx_edid_set_aud_sad(u_char *sad, u_char len);
struct data_block_location_s rx_get_cea_tag_offset(u8 *cur_edid, u16 tag_code);
void get_edid_standard_timing_info(u8 *p_edid, struct edid_standard_timing *edid_st_info);
void rm_unsupported_st(u8 *p_edid,
	struct edid_standard_timing *edid_st_info, unsigned int refresh_rate);
void hdmi_rx_frl_rate_update(u8 port, u8 new_frl_rate);

#ifdef CONFIG_AMLOGIC_HDMITX
bool rx_update_tx_edid_with_audio_block(unsigned char *edid_data,
					unsigned char *audio_block);
void rpt_edid_hf_vs_db_extraction(unsigned char *p_edid);
void rpt_edid_14_vs_db_extraction(unsigned char *p_edid);
void rpt_edid_multi_vdb_extraction(unsigned char *p_dest_edid, unsigned char *p_source_edid);
void rpt_edid_audio_db_extraction(unsigned char *p_edid);
void rpt_edid_colorimetry_db_extraction(unsigned char *p_edid);
void rpt_edid_420_vdb_extraction(unsigned char *p_edid);
void rpt_edid_hdr_static_db_extraction(unsigned char *p_edid);
void rpt_edid_extraction(unsigned char *p_edid);
#endif
void rx_edid_reset_handler(struct work_struct *work);
void rx_edid_reset_task(u8 port);
void rx_get_edid_support(u8 port);
void rx_print_edid_support(u8 port);
bool is_valid_edid_data(unsigned char *p_edid);
u32 rx_get_edid_size(u8 *pedid);
bool is_support_frl(u8 *pedid, u8 port);
enum hrtimer_restart edid_reset_callback(struct hrtimer *timer);
void update_edid_type_cfg(unsigned int val);
#endif
