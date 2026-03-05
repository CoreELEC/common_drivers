/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __MESON_TX_EDID_H_
#define __MESON_TX_EDID_H_

#ifdef __UBOOT__
#include <linux/types.h>
#include <amlogic/media/vout/aml_vinfo.h>
#include <amlogic/media/vout/hdmitx21/hdmitx_ext.h>
#include <amlogic/media/vout/hdmitx21/hdmi_common.h>
#elif __KERNEL__
#include <linux/types.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_mode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_format_para.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx.h>
#endif

/* displayID start */
#define SECTION_MAX_LENGTH 256
#define MAX_SECTION_COUNT 256

#define BLK_TIMING_CNT	12

#define VESA_IEEE_OUI				0x3a0292

/* DisplayID Structure versions */
#define DISPLAY_ID_STRUCTURE_VER_20		0x20

/* DisplayID Structure v1r2 Data Blocks */
#define DATA_BLOCK_PRODUCT_ID			0x00
#define DATA_BLOCK_DISPLAY_PARAMETERS		0x01
#define DATA_BLOCK_COLOR_CHARACTERISTICS	0x02
#define DATA_BLOCK_TYPE_1_DETAILED_TIMING	0x03
#define DATA_BLOCK_TYPE_2_DETAILED_TIMING	0x04
#define DATA_BLOCK_TYPE_3_SHORT_TIMING		0x05
#define DATA_BLOCK_TYPE_4_DMT_TIMING		0x06
#define DATA_BLOCK_VESA_TIMING			0x07
#define DATA_BLOCK_CEA_TIMING			0x08
#define DATA_BLOCK_VIDEO_TIMING_RANGE		0x09
#define DATA_BLOCK_PRODUCT_SERIAL_NUMBER	0x0a
#define DATA_BLOCK_GP_ASCII_STRING		0x0b
#define DATA_BLOCK_DISPLAY_DEVICE_DATA		0x0c
#define DATA_BLOCK_INTERFACE_POWER_SEQUENCING	0x0d
#define DATA_BLOCK_TRANSFER_CHARACTERISTICS	0x0e
#define DATA_BLOCK_DISPLAY_INTERFACE		0x0f
#define DATA_BLOCK_STEREO_DISPLAY_INTERFACE	0x10
#define DATA_BLOCK_TILED_DISPLAY		0x12
#define DATA_BLOCK_VENDOR_SPECIFIC		0x7f
#define DATA_BLOCK_CTA				0x81

/* DisplayID Structure v2r0 Data Blocks */
#define DATA_BLOCK_2_PRODUCT_ID			0x20
#define DATA_BLOCK_2_DISPLAY_PARAMETERS		0x21
#define DATA_BLOCK_2_TYPE_7_DETAILED_TIMING	0x22
#define DATA_BLOCK_2_TYPE_8_ENUMERATED_TIMING	0x23
#define DATA_BLOCK_2_TYPE_9_FORMULA_TIMING	0x24
#define DATA_BLOCK_2_DYNAMIC_VIDEO_TIMING	0x25
#define DATA_BLOCK_2_DISPLAY_INTERFACE_FEATURES	0x26
#define DATA_BLOCK_2_STEREO_DISPLAY_INTERFACE	0x27
#define DATA_BLOCK_2_TILED_DISPLAY_TOPOLOGY	0x28
#define DATA_BLOCK_2_CONTAINER_ID		0x29
#define DATA_BLOCK_2_VENDOR_SPECIFIC		0x7e
#define DATA_BLOCK_2_CTA_DISPLAY_ID		0x81

/* DisplayID Structure v1r2 Product Type */
#define PRODUCT_TYPE_EXTENSION			0
#define PRODUCT_TYPE_TEST			1
#define PRODUCT_TYPE_PANEL			2
#define PRODUCT_TYPE_MONITOR			3
#define PRODUCT_TYPE_TV				4
#define PRODUCT_TYPE_REPEATER			5
#define PRODUCT_TYPE_DIRECT_DRIVE		6

/* DisplayID Structure v2r0 Display Product Primary Use Case (~Product Type) */
#define PRIMARY_USE_EXTENSION			0
#define PRIMARY_USE_TEST			1
#define PRIMARY_USE_GENERIC			2
#define PRIMARY_USE_TV				3
#define PRIMARY_USE_DESKTOP_PRODUCTIVITY	4
#define PRIMARY_USE_DESKTOP_GAMING		5
#define PRIMARY_USE_PRESENTATION		6
#define PRIMARY_USE_HEAD_MOUNTED_VR		7
#define PRIMARY_USE_HEAD_MOUNTED_AR		8

#define DISPLAY_ID_TYPE_VII_ONLY_SUPPORT_DSC         BIT(0)
#define DISPLAY_ID_TYPE_VII_YUV420                   BIT(1)

struct displayid_header {
	u8 rev;
	u8 bytes;
	u8 prod_id;
	u8 ext_count;
} __packed;

struct displayid_block {
	u8 tag;
	u8 rev;
	u8 num_bytes;
} __packed;

enum display_id_aspect_ratio {
	DISPLAY_ID_ASPECT_RATIO_1_1 = 0,
	DISPLAY_ID_ASPECT_RATIO_5_4,
	DISPLAY_ID_ASPECT_RATIO_4_3,
	DISPLAY_ID_ASPECT_RATIO_15_9,
	DISPLAY_ID_ASPECT_RATIO_16_9,
	DISPLAY_ID_ASPECT_RATIO_16_10,
	DISPLAY_ID_ASPECT_RATIO_64_27,
	DISPLAY_ID_ASPECT_RATIO_256_135,
	DISPLAY_ID_ASPECT_RATIO_NOT_DEFINED,
	DISPLAY_ID_ASPECT_RATIO_RESERVED,
};

enum disp_interface_type {
	INTERFACE_ANALOG                         = 0x00,
	INTERFACE_LVDS                           = 0x01,
	INTERFACE_TMDS                           = 0x02,
	INTERFACE_RSDS                           = 0x03,
	INTERFACE_DVI_D                          = 0x04,
	INTERFACE_DVI_I_ANALOG_SECTION           = 0x05,
	INTERFACE_DVI_I_DIGITAL_SECTION          = 0x06,
	INTERFACE_HDMI_A                         = 0x07,
	INTERFACE_HDMI_B                         = 0x08,
	INTERFACE_MDDI                           = 0x09,
	INTERFACE_DISPLAY_PORT                   = 0x0A,
	INTERFACE_PROPRIETARY_DIGITAL_INTERFACE  = 0x0B,
};

enum number_of_links {
	DISPLAY_LINK_0     = 0x00,
	DISPLAY_LINK_1     = 0x01,
	DISPLAY_LINK_2     = 0x02,
	DISPLAY_LINK_4     = 0x04,
};

enum analog_interface_sub_type_codes {
	ANALOG_15HD_OR_VGA = 0x00,
	ANALOG_VESA_NAVI_V = 0x01,
	ANALOG_VESA_NAVI_D = 0x02,
};

union disp_number_of_links {
	enum number_of_links links;
	enum analog_interface_sub_type_codes analog_sub_type_codes;
};

struct rgb_444_color_depth_flags {
	u8 bpc6    : 1;
	u8 bpc8    : 1;
	u8 bpc10   : 1;
	u8 bpc12   : 1;
	u8 bpc14   : 1;
	u8 bpc16   : 1;
	u8 reserved: 2;
};

struct y422_420_color_depth_flags {
	u8 bpc8    : 1;
	u8 bpc10   : 1;
	u8 bpc12   : 1;
	u8 bpc14   : 1;
	u8 bpc16   : 1;
	u8 reserved: 3;
};

struct disp_color_depth {
	struct rgb_444_color_depth_flags rgb_color_depth;
	struct rgb_444_color_depth_flags yuv444_color_depth;
	struct y422_420_color_depth_flags yuv422_color_depth;
	/* v2.1 */
	struct y422_420_color_depth_flags yuv420_color_depth;
};

enum content_protection {
	NO_CONTENT_PROTECTION_SUPPORT   = 0,
	HDCP                            = 1,
	DTCP                            = 2,
	DPCP                            = 3,
};

struct disp_support_content_protection {
	enum content_protection protection;
	u8 version  : 4;
	u8 revision : 4;
};

enum spread_supported_type {
	NOT_SPREAD_SUPPORT = 0,
	DOWN_SPREAD,
	CENTER_SPREAD,
	RESERVED,
};

struct disp_spread_spectrum_info {
	enum spread_supported_type spread_type;
	u8 spread_percentage : 4;
};

struct disp_eotf_and_color_space {
	u8 eotf        : 4;
	u8 color_space : 4;
};

struct display_interface_info {
	enum disp_interface_type type;
	union disp_number_of_links link;
	u8 interface_version: 4;
	u8 interface_revision: 4;
	struct disp_color_depth color_depth;

	struct disp_support_content_protection content_protection;
	struct disp_spread_spectrum_info spread_info;
	u8 lvds_voltage_and_color_mapping;
	u8 timing_dignal_settings;
	/* v2.1 */
	u8 yuv420_min_pixel_rate;
	/*
	 * bit 5
	 *  48-kHz Sample Rate Supported
	 *  0 = Audio shall not be supported at the 48-kHz sample rate.
	 *  1 = Audio shall be supported at the 48-kHz sample rate
	 * bit 6
	 *  44.1-kHz Sample Rate Supported
	 *  0 = Audio shall not be supported at the 44.1-kHz sample rate.
	 *  1 = Audio shall be supported at the 44.1-kHz sample rate.
	 * bit 7
	 *	32-kHz Sample Rate Supported
	 *	0 = Audio shall not be supported at the 32-kHz sample rate.
	 *	1 = Audio shall be supported at the 32-kHz sample rate.
	 */
	u8 audio_sample_rate : 3;
	/*
	 * bit 0
	 *  sRGB
	 * bit 1
	 *  ITU-R BT.601
	 * bit 2
	 *  ITU-R BT.709
	 * bit 3
	 *  Adobe RGB
	 * bit 4
	 *  DCI-P3
	 * bit 5
	 *  ITU-R BT.2020
	 * bit 6
	 *  ITU-R BT.2020 and SMPTE ST 2084
	 * bit 7
	 *  RESERVED
	 */
	u8 color_space_eotf;
	/* Additional Supported Interface Color Space and EOTF */
	struct disp_eotf_and_color_space cs_eotf[7];
};

struct displayid_detailed_timings_1 {
	u8 pixel_clock[3];
	u8 flags;
	u8 hactive[2];
	u8 hblank[2];
	u8 hsync[2];
	u8 hsw[2];
	u8 vactive[2];
	u8 vblank[2];
	u8 vsync[2];
	u8 vsw[2];
} __packed;

/* for type I and VII timing */
struct displayid_detailed_timing_block {
	struct displayid_block base;
	struct displayid_detailed_timings_1 timings[BLK_TIMING_CNT];
};

struct display_interface_v1 {
	u8 interface_type;
	u8 interface_reversion;
	u8 interface_rgb_color_depth;
	u8 interface_yuv444_color_depth;
	u8 interface_yuv422_color_depth;
	u8 content_protection;
	u8 content_protection_version;
	u8 spread_spectrum_info;
	u8 interface_type_dependent_attr_1;
	u8 interface_type_dependent_attr_2;
};

/* Table 4-51: Display Interface Data Block of displayID v1.3 */
struct displayid_display_interface_feature_data_block_v1 {
	struct displayid_block base;
	struct display_interface_v1 interface_v1;
};

struct display_interface_v2 {
	u8 interface_rgb_color_depth;
	u8 interface_yuv444_color_depth;
	u8 interface_yuv422_color_depth;
	u8 interface_yuv420_color_depth;
	u8 min_yuv420_pixel_rate;
	u8 audio_capability;
	u8 color_space_and_eotf_1;
	u8 color_space_and_eotf_2;
	u8 additional_color_space_and_eotf_count : 3;
	u8 additional_color_space_and_eotf[7];
};

/* Table 4-25: Display Interface Features Data Block of displayID v2.1a */
struct displayid_display_interface_feature_data_block_v2 {
	struct displayid_block base;
	struct display_interface_v2 interface_v2;
};

/* Table 4-1: Product Identification Data Block of displayID v1.3 */
struct product_id_data_blk {
	struct displayid_block product_id_header;
	u32 vendor_id:24;
	u16 product_id_code:16;
	u32 serial_number;
	/* Week of Manufacture/Model Tag */
	u8 manufacture_week;
	u8 manufacture_year;
	u8 product_id_string_size;
	/* product_id_string(optional): 235 bytes maximum */
};

struct dynamic_video_timing_range_limits {
	u8 pixel_clock_min[3];
	u8 pixel_clock_max[3];
	u8 vertical_refresh_rate_min;
	u8 vertical_refresh_rate_max;
	/*
	 * flags bit 1:0
	 *   For Block Revision 0
	 *   Cleared to all 0s
	 *
	 *   For Block Revision 1
	 *   Maximum Vertical Refresh Rate bit 9:8
	 *
	 * flag bit 7
	 *   0 = Seamless Dynamic Video Timing change shall not be supported
	 *   with a fixed horizontal pixel rate and dynamic vertical blanking
	 *
	 *   1 = Seamless Dynamic Video Timing change shall be supported
	 *   with a fixed horizontal pixel rate and dynamic vertical blanking
	 */
	u8 flags;
};

/* Table 4-24: Dynamic Video Timing Range Limits Data Block of DisplayID v2.1a */
struct dynamic_video_timing_range_limits_data_block {
	struct displayid_block product_id_header;
	struct dynamic_video_timing_range_limits dynamic_video_timing_range;
};

struct dynamic_video_timing_range_limits_info {
	u32 pixel_clock_min:24;
	u32 pixel_clock_max:24;
	u8 vertical_refresh_rate_min;
	u16 vertical_refresh_rate_max:10;
	u8 seamless_support_flag;
};

struct display_id_cap {
	/* 1.header of base section */
	u8 version;
	/* u8 bytes_in_section; */
	u8 primary_use;
	u8 extension_count;

	/* 2.data blocks */
	/* No more than one Product Identification Data Block may be
	 * provided in any DisplayID structure.
	 */
	struct product_id_data_blk product_id_db;

	/* 3.summary information */
	/* section index/count */
	u8 section_idx;
	/* byte index of whole display id structure */
	u16 byte_idx;
	/* the number of timings declared in DISPLAY_ID */
	u32 timing_count;
	/* type 1 and type 7 timing */
	/* point to timing[MAX_SECTION_COUNT][BLK_TIMING_CNT] */
	struct tx_timing **timing;
	/* Display Interface Features Data Block */
	struct display_interface_info display_interface;
	/* Dynamic Video Timing Range Limits Data Block */
	struct dynamic_video_timing_range_limits_info dynamic_video_timing_range;
};

struct display_product_type_id {
	u8 type_id;
	unsigned char *product_type_description;
};

void display_id_cap_print(struct display_id_cap *disp_id_cap);
/* displayID end  */

/* the default max_tmds_clk is 165MHz/5 in H14b Table 8-16 */
#define DEFAULT_MAX_TMDS_CLK    33

#define EDID_BLK_SIZE		128
#define EDID_MAX_BLOCK		8
#define VESA_MAX_TIMING		64
#define AUD_MAX_NUM			60
#define MAX_RAW_LEN			64
#define MAX_DTD_COUNT		16

#define Y420CMDB_MAX	32

/* From CTA-861-I Table 60 */
enum video_capability {
	CE_VIDEO_FORMAT_NOT_SUPPORT = 0x0,
	CE_VIDEO_FORMAT_ALWAYS_OVERSCAN = 0x1,
	CE_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN = 0x2,
	CE_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN = 0x3,
	IT_VIDEO_FORMAT_NOT_SUPPORT = 0x0,
	IT_VIDEO_FORMAT_ALWAYS_OVERSCAN = 0x4,
	IT_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN = 0x8,
	IT_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN = 0xC,
	PT_VIDEO_FORMAT_NO_DATA = 0x0,
	PT_VIDEO_FORMAT_ALWAYS_OVERSCAN = 0x10,
	PT_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN = 0x20,
	PT_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN = 0x30,
	QS_NO_DATA = 0x0,
	QS_SELECTABLE = 0x40,
	QY_NO_DATA = 0x0,
	QY_SELECTABLE = 0x80,
};

enum hdmi_3d_type {
	T3D_FRAME_PACKING = 0,
	T3D_FIELD_ALTER = 1,
	T3D_LINE_ALTER = 2,
	T3D_SBS_FULL = 3,
	T3D_L_DEPTH = 4,
	T3D_L_DEPTH_GRAPHICS = 5,
	T3D_TAB = 6, /* Top and Buttom */
	T3D_RSVD = 7,
	T3D_SBS_HALF = 8,
	T3D_DISABLE,
};

struct raw_block {
	int len;
	char raw[MAX_RAW_LEN];
};

struct rx_audio_cap {
	u8 audio_format_code;
	u8 channel_num_max;
	u8 freq_cc;
	u8 cc3;
};

struct dolby_vsadb_cap {
	unsigned char rawdata[7 + 1]; // padding extra 1 byte
	unsigned int ieeeoui;
	unsigned char length;
	unsigned char dolby_vsadb_ver;
	unsigned char spk_center:1;
	unsigned char spk_surround:1;
	unsigned char spk_height:1;
	unsigned char headphone_only:1;
	unsigned char mat_48k_pcm_only:1;
};

#define CAP_BIT7_UNDERSCAN_MASK    0x80    // Bit 7 (MSB): Underscan flag
#define CAP_BIT6_AUDIO_MASK        0x40    // Bit 6: Basic Audio support flag
#define CAP_BIT5_YCBCR_444_MASK    0x20    // Bit 5: YCbCr 4:4:4 format support flag
#define CAP_BIT4_YCBCR_422_MASK    0x10    // Bit 4: YCbCr 4:2:2 format support flag
#define CAP_LOWER_4BITS_DTD_MASK   0x0F    // Lower 4 bits (Bits 3~0): Native DTD count
struct rx_cap {
	/*
	 * If the display does not provide a VCDB then the Source should assume that
	 * CE Video Formats are over scanned by the display and that IT Video Format
	 * behavior is indicated by CEA Extension byte 3 bit 7 (underscan).
	 * If underscan=1 then the Source should assume that IT Video Formats are
	 * underscanned and if underscan=0, that IT Video Formats are over scanned
	 */
	u8 underscan;
	/*
	 * Native video mode and capability indicator of HDMI receiver (parsed from EDID block)
	 * Each bit of this variable has a specific definition as follows:
	 * Bit 7: Underscan flag (1 = Rx underscans ITU video formats by default)
	 * Bit 6: Basic Audio support flag (1 = Rx supports Basic Audio)
	 * Bit 5: YCbCr 4:4:4 support flag (1 = Rx supports YCbCr 4:4:4 in addition to RGB)
	 * Bit 4: YCbCr 4:2:2 support flag (1 = Rx supports YCbCr 4:2:2 in addition to RGB)
	 * Bits 3~0: Total number of native DTDs (Detailed Timing Descriptors, native video formats)
	 */
	u32 native_Mode;
	/*video*/
	u32 VIC[VIC_MAX_NUM];
	/* used to store SVD in VDB */
	u32 SVD_VIC[SVD_VIC_MAX_NUM];
	u32 y420_vic[Y420_VIC_MAX_NUM];
	u32 VIC_count;
	u32 SVD_VIC_count;
	u32 native_vic;
	/* some Rx has two native mode, normally only one */
	u32 native_vic2;

	/*hdmi_vic have different define for tx20&tx21,use u32 instead here.*/
	/* Max 64 */
	u32 vesa_timing[VESA_MAX_TIMING];

	/*audio*/
	struct rx_audio_cap RxAudioCap[AUD_MAX_NUM];
	u8 AUD_count;
	u8 RxSpeakerAllocation;
	struct dolby_vsadb_cap dolby_vsadb_cap;
	/*vendor*/
	u32 ieeeoui;
	/* HDMI1.4b TMDS_CLK */
	u8 Max_TMDS_Clock1;
	/* For HDMI Forum */
	u32 hf_ieeeoui;
	/* HDMI2.0 TMDS_CLK */
	u32 Max_TMDS_Clock2;
	/* CTA-861-I, Table 85 Video Capability Data Block */
	u8 video_capability_data;
	/* CEA861-F, Table 56, Colorimetry Data Block */
	u32 colorimetry_data;
	u32 colorimetry_data2;
	u32 scdc_present:1;
	/* SCDC read request */
	u32 scdc_rr_capable:1;
	u32 lte_340mcsc_scramble:1;
	u32 dc_y444:1;
	u32 dc_30bit:1;
	u32 dc_36bit:1;
	u32 dc_48bit:1;
	u32 dc_30bit_420:1;
	u32 dc_36bit_420:1;
	u32 dc_48bit_420:1;
	enum frl_rate_enum max_frl_rate;
	/* for dsc */
	u8 dsc_10bpc:1;
	u8 dsc_12bpc:1;
	u8 dsc_16bpc:1;
	u8 dsc_all_bpp:1;
	u8 dsc_native_420:1;
	u8 dsc_1p2:1;
	u8 dsc_max_slices:4;
	enum frl_rate_enum dsc_max_frl_rate;
	u8 dsc_total_chunk_bytes:6;
	u32 cnc0:1; /* Graphics */
	u32 cnc1:1; /* Photo */
	u32 cnc2:1; /* Cinema */
	u32 cnc3:1; /* Game */
	u32 qms_tfr_max:1;
	u32 qms:1;
	u32 mdelta:1;
	u32 qms_tfr_min:1;
	u32 neg_mvrr:1;
	u32 fva:1;
	u32 allm:1;
	u32 fapa_start_loc:1;
	u32 fapa_end_extended:1;
	u32 vrr_max;
	u32 vrr_min;
	struct hdr_info hdr_info;
	struct dv_info dv_info;
	u8 IDManufacturerName[4];
	u8 IDProductCode[2];
	u8 IDSerialNumber[4];
	u8 ReceiverProductName[16];
	u8 manufacture_week;
	u8 manufacture_year;
	u16 physical_width;
	u16 physical_height;
	u8 edid_version;
	u8 edid_revision;
	u8 ColorDeepSupport;
	u32 vLatency;
	u32 aLatency;
	u32 i_vLatency;
	u32 i_aLatency;
	u32 threeD_present;
	u32 threeD_Multi_present;
	u32 hdmi_vic_LEN;
	u32 HDMI_3D_LEN;
	u32 threeD_Structure_ALL_15_0;
	u32 threeD_MASK_15_0;
	struct {
		u8 frame_packing;
		u8 top_and_bottom;
		u8 side_by_side;
	} support_3d_format[VIC_MAX_NUM];
	struct vsdb_phyaddr vsdb_phy_addr;
	/* for total = 32*8 = 256 VICs */
	/* for Y420CMDB bitmap */
	unsigned char bitmap_valid;
	unsigned char bitmap_length;
	unsigned char y420_all_vic;
	unsigned char y420cmdb_bitmap[Y420CMDB_MAX];

	/*hdmi_vic have different define for tx20&tx21,use u32 instead here.*/
	//enum hdmi_vic preferred_mode;
	u32 preferred_mode;

	struct dtd dtd[MAX_DTD_COUNT];
	u8 dtd_idx;
	u8 flag_vfpdb;
	u8 number_of_dtd;
	struct raw_block asd;
	struct raw_block vsd;
	/* for DV cts */
	bool ifdb_present;
	/* IFDB, currently only use below node */
	u8 additional_vsif_num;
	/*
	 * Whether the current edid is valid
	 * 0:edid is invalid
	 * 1:edid is valid
	 */
	u8 edid_parsing;
	/*blk0 check sum*/
	u8 blk0_chksum;
	u8 hdmichecksum[11]; /* string with 0xAABBCCDD */
	u8 head_err;
	u8 chksum_err;
	u8 edid_changed;
	/* edid_check = 0 is default check
	 * Bit 0     (0x01)  don't check block header
	 * Bit 1     (0x02)  don't check edid checksum
	 * Bit 0+1   (0x03)  don't check both block header and checksum
	 */
	u8 edid_check;

	struct display_id_cap disp_id_cap;
};

/* Refer to http://standards-oui.ieee.org/oui/oui.txt */
#define DOVI_IEEEOUI		0x00D046
#define HDR10PLUS_IEEEOUI	0x90848B
#define CUVA_IEEEOUI		0x047503
#define HF_IEEEOUI		0xC45DD8

#define EXTENSION_EEODB_EXT_TAG	0xe2 /* Fixed value, HDMI EEODB Data Block*/
#define EXTENSION_EEODB_EXT_CODE	0x78 /* HDMI EEODB tag code */

#define GET_OUI_BYTE0(oui)	((oui) & 0xff) /* Little Endian */
#define GET_OUI_BYTE1(oui)	(((oui) >> 8) & 0xff)
#define GET_OUI_BYTE2(oui)	(((oui) >> 16) & 0xff)

/* edid apis */

/*edid is good return 0, otherwise return < 0.*/
bool meson_tx_edid_is_all_zeros(unsigned char *rawedid);
bool meson_tx_edid_check_valid(struct rx_cap *prxcap, unsigned char *buf);
int meson_tx_edid_parse(struct rx_cap *prxcap, u8 *edid_buf, u8 edid_parse_mask);
unsigned int meson_tx_edid_valid_block_num(unsigned char *edid_buf);
void meson_tx_edid_raw_data_print(u8 *edid_buf);
void meson_tx_edid_buffer_clear(u8 *edid_buf, int size);
void meson_tx_clear_rx_cap(struct rx_cap *prxcap);
bool meson_tx_edid_support_y422(struct rx_cap *prxcap);
void meson_tx_edid_phy_addr_parse(struct rx_cap *prxcap, u8 *edid_buf);
int meson_tx_edid_audio_parse(struct rx_cap *prxcap, u8 *block_buf);
bool meson_tx_validate_y420_vic(enum hdmi_vic vic);
bool meson_tx_edid_validate_timing(struct tx_timing *timing, struct rx_cap *rx_cap);
#endif
