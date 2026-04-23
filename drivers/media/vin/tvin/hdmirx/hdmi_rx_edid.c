// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>

/* Local include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"

enum edid_delivery_mothed_e edid_delivery_mothed;
/* buff to store downstream EDID or EDID load from bin */
static char edid_buf1[EDID_BUF_SIZE] = {0};
static char edid_buf2[EDID_BUF_SIZE] = {0};
static char edid_buf3[EDID_BUF_SIZE] = {0};
static char edid_buf4[EDID_BUF_SIZE] = {0};
u8 edid_port_type[4] = {0};
char edid_cur[E_PORT_NUM * EDID_SIZE] = {0};
#ifdef CONFIG_AMLOGIC_HDMITX
//0: hdmi repeater
//1: use tx edid directly
//2: use default edid.

u32 tx_hdr_priority;
unsigned char tx_vic_list[31] = { 0 };
unsigned char def_vic_list[31] = { 0 };
static unsigned char edid_tx[EDID_SIZE];
//hdmi repeater cert item HF2-31
//if no intersection between 420vdb, it should remove dc_36bit_420 dc_30bit_420 in vshf
static bool vshf_dc_30_36_420_support = true;
#endif

u8 need_support_atmos_bit = 0xff;
static unsigned char recv_earc_cap_ds[EARC_CAP_DS_MAX_LENGTH] = {0};
static u8 com_aud[DB_LEN_MAX - 1] = {0};
/* use vsvdb of edid bin by default, but
 * after DV enable/disable setting, use
 * vsvdb from DV module or remove vsvdb
 */
static unsigned char recv_vsvdb_len = 0xFF;
static unsigned char recv_vsvdb[VSVDB_LEN] = {0};
u32 vsvdb_update_hpd_en = 1;

u8 port_hpd_rst_flag;

int phy_addr_map;
u8 cta_flag[4] = {0};

static u_char tmp_sad_len;
static u_char tmp_sad[30];
bool sad_set_hpd;
/* if free space is not enough in edid to do
 * data blk splice, then remove the last dtd(s)
 */
u32 en_take_dtd_space = 1;
/*
 * 1:reset hpd after cap ds edid update
 * 0:not reset hpd after cap ds edid update
 */
u32 earc_cap_ds_update_hpd_en = 1;

unsigned int edid_select;
unsigned int edid_reset_max = 500;

/* hdmi1.4 edid */
static unsigned char edid_dbg[512] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78, 0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74, 0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x6F, 0xC2, 0x00, 0xA0, 0xA0, 0xA0, 0x55, 0x50,
0x30, 0x20, 0x35, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x30, 0xA5, 0x1F, 0x8C, 0x3C, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xFE,
0x02, 0x03, 0x7A, 0xF0, 0x5F, 0x5F, 0x10, 0x1F, 0x14, 0x05, 0x13, 0x04, 0x20, 0x22, 0x12, 0x16,
0x03, 0x07, 0x11, 0x15, 0x02, 0x06, 0x01, 0x61, 0x5D, 0x64, 0x65, 0x66, 0x62, 0x60, 0x5E, 0x6A,
0x6B, 0x3F, 0x75, 0x76, 0x32, 0x09, 0x7F, 0x05, 0x15, 0x07, 0x50, 0x57, 0x06, 0x01, 0x3D, 0x07,
0xC0, 0x5F, 0x7E, 0x01, 0x67, 0x04, 0x03, 0x83, 0x01, 0x00, 0x00, 0x6E, 0x03, 0x0C, 0x00, 0x30,
0x00, 0xB8, 0x3C, 0x2F, 0x80, 0x80, 0x01, 0x02, 0x03, 0x04, 0x6D, 0xD8, 0x5D, 0xC4, 0x01, 0x78,
0x88, 0x63, 0x02, 0x30, 0x3C, 0xCF, 0x67, 0x1F, 0xE3, 0x05, 0x40, 0x01, 0xE5, 0x0F, 0x00, 0x00,
0x64, 0x0D, 0xE3, 0x06, 0x0D, 0x01, 0xE5, 0x01, 0x8B, 0x84, 0x90, 0x01, 0x6D, 0x1A, 0x00, 0x00,
0x02, 0x0B, 0x30, 0x78, 0x00, 0x04, 0x50, 0x4C, 0x5A, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C,
};

static unsigned char edid_dbg1[512] = {
0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
0x05, 0xAC, 0x30, 0x00, 0x01, 0x00, 0x00, 0x00,
0x20, 0x19, 0x01, 0x03, 0x80, 0x73, 0x41, 0x78,
0x0A, 0xCF, 0x74, 0xA3, 0x57, 0x4C, 0xB0, 0x23,
0x09, 0x48, 0x4C, 0x21, 0x08, 0x00, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x04, 0x74,
0x00, 0x30, 0xF2, 0x70, 0x5A, 0x80, 0xB0, 0x58,
0x8A, 0x00, 0x20, 0xC2, 0x31, 0x00, 0x00, 0x1E,
0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40,
0x58, 0x2C, 0x45, 0x00, 0x20, 0xC2, 0x31, 0x00,
0x00, 0x1E, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x41,
0x4D, 0x4C, 0x20, 0x54, 0x56, 0x0A, 0x20, 0x20,
0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFD,
0x00, 0x3B, 0x46, 0x1F, 0x8C, 0x3C, 0x00, 0x0A,
0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x01, 0xDA,
0x02, 0x03, 0x58, 0xF0, 0x4E, 0x60, 0x61, 0x5F,
0x5E, 0x5D, 0x10, 0x1F, 0x22, 0x21, 0x20, 0x04,
0x13, 0x03, 0x12, 0x2C, 0x0F, 0x7F, 0x05, 0x15,
0x07, 0x50, 0x57, 0x06, 0x01, 0x67, 0x54, 0x03,
0x83, 0x01, 0x00, 0x00, 0x6D, 0x03, 0x0C, 0x00,
0x10, 0x00, 0xB8, 0x3C, 0x2F, 0x80, 0x60, 0x01,
0x02, 0x03, 0x68, 0xD8, 0x5D, 0xC4, 0x01, 0x78,
0x80, 0x03, 0x02, 0xE3, 0x05, 0x40, 0x01, 0xE2,
0x0F, 0x03, 0xE3, 0x06, 0x0D, 0x01, 0xEB, 0x01,
0x46, 0xD0, 0x00, 0x44, 0x54, 0x23, 0x88, 0x52,
0x6B, 0x94, 0xE5, 0x01, 0x8B, 0x84, 0x90, 0x01,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDD,
};

/* correspond to enum hdmi_vic_e */
const char *hdmi_fmt[] = {
	"HDMI_UNKNOWN",
	"HDMI_640x480p60_4x3",
	"HDMI_720x480p60_4x3",
	"HDMI_720x480p60_16x9",
	"HDMI_1280x720p60_16x9",
	"HDMI_1920x1080i60_16x9",
	"HDMI_720x480i60_4x3",
	"HDMI_720x480i60_16x9",
	"HDMI_720x240p60_4x3",
	"HDMI_720x240p60_16x9",
	"HDMI_2880x480i60_4x3",
	"HDMI_2880x480i60_16x9",
	"HDMI_2880x240p60_4x3",
	"HDMI_2880x240p60_16x9",
	"HDMI_1440x480p60_4x3",
	"HDMI_1440x480p60_16x9",
	"HDMI_1920x1080p60_16x9",
	"HDMI_720x576p50_4x3",
	"HDMI_720x576p50_16x9",
	"HDMI_1280x720p50_16x9",
	"HDMI_1920x1080i50_16x9",
	"HDMI_720x576i50_4x3",
	"HDMI_720x576i50_16x9",
	"HDMI_720x288p_4x3",
	"HDMI_720x288p_16x9",
	"HDMI_2880x576i50_4x3",
	"HDMI_2880x576i50_16x9",
	"HDMI_2880x288p50_4x3",
	"HDMI_2880x288p50_16x9",
	"HDMI_1440x576p_4x3",
	"HDMI_1440x576p_16x9",
	"HDMI_1920x1080p50_16x9",
	"HDMI_1920x1080p24_16x9",
	"HDMI_1920x1080p25_16x9",
	"HDMI_1920x1080p30_16x9",
	"HDMI_2880x480p60_4x3",
	"HDMI_2880x480p60_16x9",
	"HDMI_2880x576p50_4x3",
	"HDMI_2880x576p50_16x9",
	"HDMI_1920x1080i_t1250_50_16x9",
	"HDMI_1920x1080i100_16x9",
	"HDMI_1280x720p100_16x9",
	"HDMI_720x576p100_4x3",
	"HDMI_720x576p100_16x9",
	"HDMI_720x576i100_4x3",
	"HDMI_720x576i100_16x9",
	"HDMI_1920x1080i120_16x9",
	"HDMI_1280x720p120_16x9",
	"HDMI_720x480p120_4x3",
	"HDMI_720x480p120_16x9",
	"HDMI_720x480i120_4x3",
	"HDMI_720x480i120_16x9",
	"HDMI_720x576p200_4x3",
	"HDMI_720x576p200_16x9",
	"HDMI_720x576i200_4x3",
	"HDMI_720x576i200_16x9",
	"HDMI_720x480p240_4x3",
	"HDMI_720x480p240_16x9",
	"HDMI_720x480i240_4x3",
	"HDMI_720x480i240_16x9",
	/* Refer to CEA 861-F */
	"HDMI_1280x720p24_16x9",
	"HDMI_1280x720p25_16x9",
	"HDMI_1280x720p30_16x9",
	"HDMI_1920x1080p120_16x9",
	"HDMI_1920x1080p100_16x9",
	"HDMI_1280x720p24_64x27",
	"HDMI_1280x720p25_64x27",
	"HDMI_1280x720p30_64x27",
	"HDMI_1280x720p50_64x27",
	"HDMI_1280x720p60_64x27",
	"HDMI_1280x720p100_64x27",
	"HDMI_1280x720p120_64x27",
	"HDMI_1920x1080p24_64x27",
	"HDMI_1920x1080p25_64x27",
	"HDMI_1920x1080p30_64x27",
	"HDMI_1920x1080p50_64x27",
	"HDMI_1920x1080p60_64x27",
	"HDMI_1920x1080p100_64x27",
	"HDMI_1920x1080p120_64x27",
	"HDMI_1680x720p24_64x27",
	"HDMI_1680x720p25_64x27",
	"HDMI_1680x720p30_64x27",
	"HDMI_1680x720p50_64x27",
	"HDMI_1680x720p60_64x27",
	"HDMI_1680x720p100_64x27",
	"HDMI_1680x720p120_64x27",
	"HDMI_2560x1080p24_64x27",
	"HDMI_2560x1080p25_64x27",
	"HDMI_2560x1080p30_64x27",
	"HDMI_2560x1080p50_64x27",
	"HDMI_2560x1080p60_64x27",
	"HDMI_2560x1080p100_64x27",
	"HDMI_2560x1080p120_64x27",
	"HDMI_3840x2160p24_16x9",
	"HDMI_3840x2160p25_16x9",
	"HDMI_3840x2160p30_16x9",
	"HDMI_3840x2160p50_16x9",
	"HDMI_3840x2160p60_16x9",
	"HDMI_4096x2160p24_256x135",
	"HDMI_4096x2160p25_256x135",
	"HDMI_4096x2160p30_256x135",
	"HDMI_4096x2160p50_256x135",
	"HDMI_4096x2160p60_256x135",
	"HDMI_3840x2160p24_64x27",
	"HDMI_3840x2160p25_64x27",
	"HDMI_3840x2160p30_64x27",
	"HDMI_3840x2160p50_64x27",
	"HDMI_3840x2160p60_64x27",
	"HDMI_1280x720p48_16x9",
	"HDMI_1280x720p48_64x27",
	"HDMI_1680x720p48_64x27",
	"HDMI_1920x1080p48_16x9",
	"HDMI_1920x1080p48_64x27",
	"HDMI_2560x1080p48_64x27",
	"HDMI_3840x2160p48_16x9",
	"HDMI_4096x2160p48_256x135",
	"HDMI_3840x2160p48_64x27",
	"HDMI_3840x2160p100_16x9",
	"HDMI_3840x2160p120_16x9",
	"HDMI_3840x2160p100_64x27",
	"HDMI_3840x2160p120_64x27",
	"HDMI_5120x2160p24_64x27",
	"HDMI_5120x2160p25_64x27",
	"HDMI_5120x2160p30_64x27",
	"HDMI_5120x2160p48_64x27",
	"HDMI_5120x2160p50_64x27",
	"HDMI_5120x2160p60_64x27",
	"HDMI_5120x2160p100_64x27",
	"HDMI_5120x2160p120_64x27",
	"HDMI_7680x4320p24_16x9",
	"HDMI_7680x4320p25_16x9",
	"HDMI_7680x4320p30_16x9",
	"HDMI_7680x4320p48_16x9",
	"HDMI_7680x4320p50_16x9",
	"HDMI_7680x4320p60_16x9",
	"HDMI_7680x4320p100_16x9",
	"HDMI_7680x4320p120_16x9",
	"HDMI_7680x4320p24_64x27",
	"HDMI_7680x4320p25_64x27",
	"HDMI_7680x4320p30_64x27",
	"HDMI_7680x4320p48_64x27",
	"HDMI_7680x4320p50_64x27",
	"HDMI_7680x4320p60_64x27",
	"HDMI_7680x4320p100_64x27",
	"HDMI_7680x4320p120_64x27",
	"HDMI_10240x4320p24_64x27",
	"HDMI_10240x4320p25_64x27",
	"HDMI_10240x4320p30_64x27",
	"HDMI_10240x4320p48_64x27",
	"HDMI_10240x4320p50_64x27",
	"HDMI_10240x4320p60_64x27",
	"HDMI_10240x4320p100_64x27",
	"HDMI_10240x4320p120_64x27",
	"HDMI_4096x4320p100_256x135",
	"HDMI_4096x4320p120_256x135",
	"HDMI_RESERVED",
};

const char *_3d_structure[] = {
	/* hdmi1.4 spec, Table H-7 */
	/* value: 0x0000 */
	"Frame packing",
	"Field alternative",
	"Line alternative",
	"Side-by-Side(Full)",
	"L + depth",
	"L + depth + graphics + graphics-depth",
	"Top-and-Bottom",
	/* value 0x0111: Reserved for future use */
	"Resrvd",
	/* value 0x1000 */
	"Side-by-Side(Half) with horizontal sub-sampling",
	/* value 0x1001-0x1110:Reserved for future use */
	"Resrvd",
	"Resrvd",
	"Resrvd",
	"Resrvd",
	"Resrvd",
	"Resrvd",
	"Side-by-Side(Half) with all quincunx sub-sampling",
};

const char *speaker_place[] = {
	"Front Left",
	"Front Right",
	"Front Center",
	"Low Frequency Effects 1",
	"Back Left",
	"Back Right",
	"Front Left of Center",
	"Front Right of Center",
	"Back Center",
	"Low Frequency Effects 2",
	"Side Left",
	"Side Right",
	"Top Front Left",
	"Top Front Right",
	"Top Front Center",
	"Top Center",
	"Top Back Left",
	"Top Back Right",
	"Top Side Left",
	"Top Side Right",
	"Top Back Center",
	"Bottom Front Center",
	"Bottom Front Left",
	"Bottom Front Right",
	"Front Left Wide",
	"Front Right Wide",
	"Left Surround",
	"Right Surround",
};

const char *aspect_ratio[] = {
	"1:1",
	"5:4",
	"4:3",
	"15:9",
	"16:9",
	"16:10",
	"64:27",
	"256:135",
	"Calculated by Hactive and Vactive",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
};

const char *_3d_detail_x[] = {
	/* hdmi1.4 spec, Table H-8 */
	/* value: 0x0000 */
	"All horizontal sub-sampling and quincunx matrix",
	"Horizontal sub-sampling",
	"Not_in_Use1",
	"Not_in_Use2",
	"Not_in_Use3",
	"Not_in_Use4",
	"All_Quincunx",
	"ODD_left_ODD_right",
	"ODD_left_EVEN_right",
	"EVEN_left_ODD_right",
	"EVEN_left_EVEN_right",
	"Resrvd1",
	"Resrvd2",
	"Resrvd3",
	"Resrvd4",
	"Resrvd5",
};

const char *aud_fmt[] = {
	"HEADER",
	"L-PCM",
	"AC3",
	"MPEG1",
	"MP3",
	"MPEG2",
	"AAC",
	"DTS",
	"ATRAC",
	"OBA",
	"DDP", /* E_AC3 */
	"DTS_HD",
	"MAT", /* TRUE_HD*/
	"DST",
	"WMAPRO",
};

unsigned char *edid_list[] = {
	edid_buf1,
	edid_buf2,
	edid_buf3,
	edid_buf4,
	edid_dbg,
};

static struct cta_blk_pair cta_blk[] = {
	{
		.tag_code = AUDIO_TAG,
		.blk_name = "Audio_DB",
	},
	{
		.tag_code = VIDEO_TAG,
		.blk_name = "Video_DB",
	},
	{
		.tag_code = VENDOR_TAG,
		.blk_name = "Vendor_Specific_DB",
	},
	{
		.tag_code = HF_VENDOR_DB_TAG,
		.blk_name = "HF_Vendor_Specific_DB",
	},
	{
		.tag_code = SPEAKER_TAG,
		.blk_name = "Speaker_Alloc_DB",
	},
	{
		.tag_code = VDTCDB_TAG,
		.blk_name = "VESA_DTC_DB",
	},
	{
		.tag_code = EXTENDED_VCDB_TAG,
		.blk_name = "Video_Cap_DB",
	},
	{
		.tag_code = VSVDB_DV_TAG,
		.blk_name = "VSVDB_DV",
	},
	{
		.tag_code = VSVDB_HDR10P_TAG,
		.blk_name = "VSVDB_HDR10P",
	},
	{
		.tag_code = VSDB_FREESYNC_TAG,
		.blk_name = "FREESYNC_DB",
	},
	{
		.tag_code = EXTENDED_VDDDB_TAG,
		.blk_name = "VESA_Display_Device_DB",
	},
	{
		.tag_code = EXTENDED_VVTBE_TAG,
		.blk_name = "VESA_Video_Timing_Blk_Ext",
	},
	{
		.tag_code = EXTENDED_CDB_TAG,
		.blk_name = "Colorimetry_DB",
	},
	{
		.tag_code = EXTENDED_HDR_STATIC_TAG,
		.blk_name = "HDR_Static_Metadata",
	},
	{
		.tag_code = EXTENDED_HDR_DYNAMIC_TAG,
		.blk_name = "HDR_Dynamic_Metadata",
	},
	{
		.tag_code = EXTENDED_VFPDB_TAG,
		.blk_name = "Video_Fmt_Preference_DB",
	},
	{
		.tag_code = EXTENDED_Y420VDB_TAG,
		.blk_name = "YCbCr_420_Video_DB",
	},
	{
		.tag_code = EXTENDED_Y420CMDB_TAG,
		.blk_name = "YCbCr_420_Cap_Map_DB",
	},
	{
		.tag_code = EXTENDED_VSADB_TAG,
		.blk_name = "Vendor_Specific_Audio_DB",
	},
	{
		.tag_code = EXTENDED_RCDB_TAG,
		.blk_name = "Room_Configuration_DB",
	},
	{
		.tag_code = EXTENDED_SLDB_TAG,
		.blk_name = "Speaker_Location_DB",
	},
	{
		.tag_code = EXTENDED_IFDB_TAG,
		.blk_name = "Infoframe_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | HDMI_ADB_TAG,
		.blk_name = "HDMI Audio (Multi-Stream/3D)",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | SBTM_TAG,
		.blk_name = "SBTM_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | HF_EEODB,
		.blk_name = "EEODB_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | SCDB_TAG,
		.blk_name = "Sink_Capabilities_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | T7VTDB_TAG,
		.blk_name = "T7VT_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | T8VTDB_TAG,
		.blk_name = "T8VT_DB",
	},
	{
		.tag_code = (USE_EXTENDED_TAG << 8) | T10VTDB_TAG,
		.blk_name = "T10VT_DB",
	},
	{
		.tag_code = TAG_MAX,
		.blk_name = "Unrecognized_DB",
	},
	{},
};

static u_int edid_addr[E_PORT_NUM] = {
	TOP_EDID_ADDR_S,
	TOP_EDID_PORT2_ADDR_S,
	TOP_EDID_PORT3_ADDR_S,
	TOP_EDID_PORT4_ADDR_S
};

char *rx_get_cta_blk_name(u16 tag)
{
	int i = 0;

	for (i = 0; cta_blk[i].tag_code != TAG_MAX; i++) {
		if (tag == cta_blk[i].tag_code)
			break;
	}
	return cta_blk[i].blk_name;
}

bool is_valid_edid_data(unsigned char *p_edid)
{
	bool ret = false;

	if (!p_edid)
		return ret;
	if (p_edid[0] == 0 &&
		p_edid[1] == 0xff &&
		p_edid[2] == 0xff &&
		p_edid[3] == 0xff &&
		p_edid[4] == 0xff &&
		p_edid[5] == 0xff &&
		p_edid[6] == 0xff &&
		p_edid[7] == 0)
		ret = true;

	return ret;
}

void hdmirx_fill_edid_buf(const char *buf, int size)
{
	bool err_chk = false;
	u32 u_offset = 0;
	u32 i = 0;

	if (edid_delivery_mothed == EDID_DELIVERY_NULL)
		edid_delivery_mothed = EDID_DELIVERY_ALL_PORT;
	rx_pr("EDID size1 =%d\n", size);
	//return;
	//old delivery method. the size of each EDID is 256 BYTE
	if (size == EDID_TOTAL_SIZE ||
		size == 256 * rx_info.port_num * 2) {
		rx_pr("EDID size2 =%d\n", size);
		//port0
		memcpy(edid_buf1, buf, 256);
		if (!is_valid_edid_data(edid_buf1))
			err_chk = true;
		u_offset = 256;
		memcpy(edid_buf1 + EDID_SIZE, buf + u_offset, 256);
		if (!is_valid_edid_data(edid_buf1 + EDID_SIZE))
			err_chk = true;
		//port1
		u_offset += 256;
		memcpy(edid_buf2, buf + u_offset, 256);
		if (!is_valid_edid_data(edid_buf2))
			err_chk = true;
		u_offset += 256;
		memcpy(edid_buf2 + EDID_SIZE, buf + u_offset, 256);
		if (!is_valid_edid_data(edid_buf2 + EDID_SIZE))
			err_chk = true;
		//port2
		u_offset += 256;
		memcpy(edid_buf3, buf + u_offset, 256);
		if (!is_valid_edid_data(edid_buf3))
			err_chk = true;
		u_offset += 256;
		memcpy(edid_buf3 + EDID_SIZE, buf + u_offset, 256);
		if (!is_valid_edid_data(edid_buf3 + EDID_SIZE))
			err_chk = true;
		//port3
		memcpy(edid_buf4, edid_buf1, EDID_BUF_SIZE);
	}
	if (err_chk && rx_info.boot_flag) {
		rx_pr("EDID size =%d\n", size);
		for (i = 0; i < size; i++) {
			pr_cont("%02x", buf[i]);
			if (i % 128)
				rx_pr("");
		}
	} else {
		rx_pr("HDMIRX: fill edid buf, size %d\n", size);
	}
}

bool is_edid_size_valid(u8 type, int size)
{
	bool ret = false;

	switch (type) {
	case EDID_TYPE_256_PLUS_256:
		if (size == EDID_SIZE_256_PLUS_256)
			ret = true;
		break;
	case EDID_TYPE_512_PLUS_512:
		if (size == EDID_SIZE_512_PLUS_512)
			ret = true;
		break;
	case EDID_TYPE_256_PLUS_512:
		if (size == EDID_SIZE_256_PLUS_512)
			ret = true;
		break;
	case EDID_TYPE_256_PLUS_256_PLUS_256:
		if (size == EDID_SIZE_256_PLUS_256_PLUS_256)
			ret = true;
		break;
	case EDID_TYPE_256_PLUS_512_PLUS_256:
	case EDID_TYPE_256_PLUS_512_PLUS_512:
		if (size == EDID_SIZE_256_PLUS_512_PLUS_256)
			ret = true;
		break;
	default:
		break;
	}
	return ret;
}

void update_edid_type_cfg(unsigned int val)
{
	/* edid select for portD/C/B/A */
	rx[0].edid_type.cfg = (enum edid_ver_e)(val & 0xF);
	rx[1].edid_type.cfg = (enum edid_ver_e)((val >> 4) & 0xF);
	rx[2].edid_type.cfg = (enum edid_ver_e)((val >> 8) & 0xF);
	rx[3].edid_type.cfg = (enum edid_ver_e)((val >> 12) & 0xF);
	edid_type_init();
}

static void get_edid_phy_addr(u8 *edid, u8 port)
{
	int addr = 0;
	struct data_block_location_s ret = {0};

	if (!edid)
		return;

	ret = rx_get_cea_tag_offset(edid, VENDOR_TAG);
	if (ret.num != 0) {
		addr = edid[ret.pos[0] + 4] >> 4;
		phy_addr_map &= ~(0xf << port * 4);
		phy_addr_map |= addr << port * 4;
	}
}

void hdmirx_fill_edid_with_port_buf(const char *buf, int size)
{
	u8 port_num = 0;
	u8 edid_type = 0;
	u32 i = 0;
	u32 j = 0;
	u32 k = 0;
	unsigned char buff[EDID_SIZE] = {0};
	int edid_count = 0;
	u8 *edid_buf = NULL;
	int edid_sizes[MAX_PORT_NUM] = {0};
	int edid_offsets[MAX_PORT_NUM] = {0};
	int dest_offset = 0;

	port_num = (buf[0] & 0x0f) - 1;
	edid_type = buf[0] >> 0x4;
	switch (port_num) {
	case 0:
		edid_buf = edid_buf1;
		break;
	case 1:
		edid_buf = edid_buf2;
		break;
	case 2:
		edid_buf = edid_buf3;
		break;
	case 3:
		edid_buf = edid_buf4;
		break;
	default:
		rx_pr("Invalid port number: %d\n", port_num);
		return;
	}

	edid_port_type[port_num] = edid_type;
	if (size > sizeof(edid_buf1) + 1) {
		rx_pr("edid size overflow\n");
		size = sizeof(edid_buf1) + 1;
	}
	if (size < EDID_BLK_SIZE * 2 + 1) {
		rx_pr("Incomplete edid\n");
		return;
	}

	if (edid_type < EDID_TYPE_MAX && is_edid_size_valid(edid_type, size)) {
		switch (edid_type) {
		case EDID_TYPE_256_PLUS_256:
			memcpy(edid_buf, buf + 1, 256);
			memcpy(edid_buf + 256, buf + 257, 256);
			break;
		case EDID_TYPE_256_PLUS_512:
			memcpy(edid_buf, buf + 1, 256);
			memcpy(edid_buf + 256, buf + 257, size - 257);
			break;
		case EDID_TYPE_256_PLUS_256_PLUS_256:
			memcpy(edid_buf, buf + 1, 256);
			memcpy(edid_buf + 256, buf + 257, 256);
			memcpy(edid_buf + 768, buf + 513, 256);
			break;
		case EDID_TYPE_256_PLUS_512_PLUS_256:
		case EDID_TYPE_256_PLUS_512_PLUS_512:
			memcpy(edid_buf, buf + 1, 256);
			memcpy(edid_buf + 256, buf + 257, 512);
			memcpy(edid_buf + 768, buf + 513, 256);
			break;
		default:
			rx_pr("port %d: unknown edid_type %d\n", port_num, edid_type);
		}
	} else {
		rx_pr("port %d: unknown edid_type %d, auto-parsing EDID\n", port_num, edid_type);
		edid_count = 0;
		for (i = 1; (i < size - 8) && (edid_count < MAX_PORT_NUM); i++) {
			if (buf[i] == 0x00 && buf[i + 1] == 0xff && buf[i + 2] == 0xff &&
				buf[i + 3] == 0xff && buf[i + 4] == 0xff && buf[i + 5] == 0xff &&
				buf[i + 6] == 0xff && buf[i + 7] == 0x00) {
				if (edid_count < MAX_PORT_NUM)
					edid_offsets[edid_count++] = i;
			}
		}
		if (edid_count == 0) {
			rx_pr("No valid EDID headers found\n");
			return;
		}
		for (i = 0; i < edid_count; i++) {
			if (i < edid_count - 1)
				edid_sizes[i] = edid_offsets[i + 1] - edid_offsets[i];
			else
				edid_sizes[i] = size - edid_offsets[i];
			if (edid_sizes[i] > EDID_BLK_SIZE * 4)
				edid_sizes[i] = EDID_BLK_SIZE * 4;
		}
		memset(edid_buf, 0, sizeof(edid_buf1));
		for (i = 0; i < edid_count; i++) {
			if (i == 0)
				dest_offset = 0;
			else if (i == 1)
				dest_offset = EDID_BLK_SIZE * 2;
			else
				dest_offset = EDID_BLK_SIZE * 6;
			if (dest_offset + edid_sizes[i] <= sizeof(edid_buf1)) {
				memcpy(edid_buf + dest_offset,
					buf + edid_offsets[i], edid_sizes[i]);
			} else {
				rx_pr("EDID data exceeds buffer size at index %d\n", i);
				break;
			}
		}
	}
	if (!is_valid_edid_data(edid_buf))
		rx_pr("port %d: 1.4 edid error\n", port_num);
	if (!is_valid_edid_data(edid_buf + 256))
		rx_pr("port %d: 2.1 edid error\n", port_num);
	if (!is_valid_edid_data(edid_buf + 768) && edid_type > EDID_TYPE_256_PLUS_512)
		rx_pr("port %d: 2.0 edid error\n", port_num);
	get_edid_phy_addr(edid_buf, port_num);
	rx_sprintf(&boot_info_num, "EDID port%d - size %d\n", port_num, size);
	if (log_level & EDID_LOG) {
		for (i = 0; i * 16 < size; i++) {
			for (j = 0; j < 16; j++)
				k += sprintf(buff + k, "%02x", buf[i * 16 + j]);
			pr_info("%s", buff);
			k = 0;
		}
	}
}

void rx_edid_update_hdr_dv_info(unsigned char *p_edid)
{
	//if (hdmirx_repeat_support())
		//return;
#ifdef CONFIG_AMLOGIC_HDMITX
	if (tx_hdr_priority == 1) {
		//remove DV
		edid_rm_db_by_tag(p_edid, EXTENDED_VSVDB_TAG);
	} else if (tx_hdr_priority == 2) {
		//remove dv+hdr+hdr10p
		edid_rm_db_by_tag(p_edid, EXTENDED_HDR_STATIC_TAG);
		edid_rm_db_by_tag(p_edid, VSVDB_HDR10P_TAG);
		edid_rm_db_by_tag(p_edid, VSVDB_DV_TAG);
	}
#endif
}

void rx_edid_update_freesync_info(unsigned char *p_edid)
{
	u_int vsdb_start = 0;
	u8 version = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid || vrr_func_en != 1)
		return;
	ret = rx_get_cea_tag_offset(p_edid, VSDB_FREESYNC_TAG);
	if (ret.num < 1)
		return;
	vsdb_start = ret.pos[ret.num - 1];
	if (rx_info.vrr_min == 0)
		return;
	p_edid[vsdb_start + 6] = rx_info.vrr_min;
	version = p_edid[vsdb_start + 4];
	if (rx_info.vrr_max <= 255) {
		p_edid[vsdb_start + 7] = rx_info.vrr_max;
		if (version == 0x3) {
			p_edid[vsdb_start + 14] = rx_info.vrr_max;
			p_edid[vsdb_start + 15] = 0;
		}
	} else {
		if (version == 0x3) {
			p_edid[vsdb_start + 7] = 0xf0; //protocol requirement
			p_edid[vsdb_start + 14] = rx_info.vrr_max & 0xff;
			p_edid[vsdb_start + 15] = (rx_info.vrr_max & 0x300) >> 8;
		} else {
			rx_pr("!!!!error version\n");
		}
	}
	if (log_level & EDID_LOG)
		rx_pr("modify freesync min = %d, freesync max = %d\n",
			rx_info.vrr_min, rx_info.vrr_max);
}

void rx_edid_update_vrr_info(unsigned char *p_edid)
{
	u_int hf_vsdb_start = 0;
	u8 tag_len = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;
	ret = rx_get_cea_tag_offset(p_edid, HF_VENDOR_DB_TAG);
	if (ret.num < 1)
		return;
	hf_vsdb_start = ret.pos[ret.num - 1];
	if (!hf_vsdb_start)
		return;

	tag_len = (p_edid[hf_vsdb_start] & 0x1f) + 1;
	if (tag_len < VRR_OFFSET || vrr_func_en == 0xff)
		return;
	if (log_level & EDID_LOG)
		rx_pr("tag_len = %d", tag_len);
	if (vrr_func_en) {
		if (rx_info.vrr_min == 0)
			return;
		p_edid[hf_vsdb_start + 9] = rx_info.vrr_min & 0x3f;
		if (rx_info.vrr_max < 100) {
			p_edid[hf_vsdb_start + 10] = 0;
		} else if (rx_info.vrr_max < 256) {
			p_edid[hf_vsdb_start + 10] = rx_info.vrr_max;
		} else {
			p_edid[hf_vsdb_start + 10] = rx_info.vrr_max & 0xff;
			p_edid[hf_vsdb_start + 9] |= (rx_info.vrr_max & 0x300) >> 2;
		}
		if (log_level & EDID_LOG)
			rx_pr("modify vrr min = %d, vrr_max = %d\n",
				  rx_info.vrr_min, rx_info.vrr_max);
	} else {
		edid_rm_db_by_tag(p_edid, VSDB_FREESYNC_TAG);
		p_edid[hf_vsdb_start + 9] = 0;
		p_edid[hf_vsdb_start + 10] = 0;
	}
}

void rx_edid_update_allm_info(unsigned char *p_edid)
{
	u_int hf_vsdb_start = 0;
	u8 tag_len = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;
	ret = rx_get_cea_tag_offset(p_edid, HF_VENDOR_DB_TAG);
	if (ret.num < 1)
		return;
	hf_vsdb_start = ret.pos[ret.num - 1];
	if (!hf_vsdb_start)
		return;
	tag_len = p_edid[hf_vsdb_start] & 0x1f;
	if (tag_len < ADD_FIELD_OFFSET || allm_func_en == 0xff)
		return;
	if (log_level & EDID_LOG)
		rx_pr("tag_len = %d", tag_len);

	if (allm_func_en == 1) {
		p_edid[hf_vsdb_start + 8] |= 0x2;
		if (log_level & EDID_LOG)
			rx_pr("enable allm.\n");
	} else if (allm_func_en == 0) {
		p_edid[hf_vsdb_start + 8] &= ~0x2;
		if (log_level & EDID_LOG)
			rx_pr("disable allm.\n");
	} else {
		rx_pr("invalid allm_func_en: %d.\n", allm_func_en);
	}
}

void rx_edid_update_qms_info(unsigned char *p_edid)
{
	u_int hf_vsdb_start = 0;
	u8 tag_len = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;
	ret = rx_get_cea_tag_offset(p_edid, HF_VENDOR_DB_TAG);
	if (ret.num < 1)
		return;
	hf_vsdb_start = ret.pos[ret.num - 1];
	if (!hf_vsdb_start)
		return;
	tag_len = p_edid[hf_vsdb_start] & 0x1f;
	if (tag_len < DSC_QMS_OFFSET || qms_func_en == 0xff)
		return;
	if (log_level & EDID_LOG)
		rx_pr("tag_len = %d", tag_len);
	if (qms_func_en == 0) {
		p_edid[hf_vsdb_start + 8] &= ~0x40;
		p_edid[hf_vsdb_start + 11] &= ~0x10;
		p_edid[hf_vsdb_start + 11] &= ~0x20;
		if (log_level & EDID_LOG)
			rx_pr("disable qms.\n");
	}
}

unsigned int rx_exchange_bits(unsigned int value)
{
	unsigned int temp = 0;

	temp = value & 0xF;
	value = (((value >> 4) & 0xF) | (value & 0xFFF0));
	value = ((value & 0xFF0F) | (temp << 4));
	temp = value & 0xF00;
	value = (((value >> 4) & 0xF00) | (value & 0xF0FF));
	value = ((value & 0x0FFF) | (temp << 4));
	return value;
}

bool get_basic_dtd_data(u8 *p_edid, struct edid_info_s *edid_info)
{
	unsigned int htotal = 1;
	unsigned int vtotal = 1;
	bool ret = false;

	if (!p_edid)
		return 0;

	if (p_edid[0x36] == 0 && p_edid[0x37] == 0) {
		rx_pr("descriptor1 is not a DTD!\n");
		goto _dtd2;
	}
	ret = true;
	edid_info->descriptor1.pixel_clk = (p_edid[0x36] + (p_edid[0x37] << 8)) / 100;
	edid_info->descriptor1.hactive = p_edid[0x38] + (((p_edid[0x3A] >> 4) & 0xF) << 8);
	edid_info->descriptor1.vactive = p_edid[0x3B] + (((p_edid[0x3D] >> 4) & 0xF) << 8);
	edid_info->descriptor1.hblank = ((p_edid[0x3A] & 0xF) << 8) + p_edid[0x39];
	edid_info->descriptor1.vblank = ((p_edid[0x3D] & 0xF) << 8) + p_edid[0x3C];
	edid_info->descriptor1.hfront = p_edid[0x3E] | ((p_edid[0x41] & 0xC0) << 8);
	edid_info->descriptor1.hsync_width = p_edid[0x3F] | ((p_edid[0x41] & 0x30) << 8);
	edid_info->descriptor1.vfront = (p_edid[0x40] >> 4) | ((p_edid[0x41] & 0xC) << 8);
	edid_info->descriptor1.vsync_width = (p_edid[0x40] & 0xF) | ((p_edid[0x41] & 0x3) << 8);
	htotal = edid_info->descriptor1.hactive + edid_info->descriptor1.hblank;
	vtotal = edid_info->descriptor1.vactive + edid_info->descriptor1.vblank;
	edid_info->descriptor1.htotal =
		edid_info->descriptor1.hactive + edid_info->descriptor1.hblank;
	edid_info->descriptor1.vtotal =
		edid_info->descriptor1.vactive + edid_info->descriptor1.vblank;
	edid_info->descriptor1.framerate =
		edid_info->descriptor1.pixel_clk * 1000 / htotal * 1000 / vtotal;

_dtd2:
	if (p_edid[0x48] == 0 && p_edid[0x49] == 0) {
		rx_pr("descriptor2 is not a DTD!\n");
		return ret;
	}
	ret = true;
	edid_info->descriptor2.pixel_clk = (p_edid[0x48] + (p_edid[0x49] << 8)) / 100;
	edid_info->descriptor2.hactive = p_edid[0x4A] + (((p_edid[0x4C] >> 4) & 0xF) << 8);
	edid_info->descriptor2.vactive = p_edid[0x4D] + (((p_edid[0x4F] >> 4) & 0xF) << 8);
	edid_info->descriptor2.hblank = ((p_edid[0x4C] & 0xF) << 8) + p_edid[0x4B];
	edid_info->descriptor2.vblank = ((p_edid[0x4F] & 0xF) << 8) + p_edid[0x4E];
	edid_info->descriptor2.hfront = p_edid[0x50] | ((p_edid[0x53] & 0xC0) << 8);
	edid_info->descriptor2.hsync_width = p_edid[0x51] | ((p_edid[0x53] & 0x30) << 8);
	edid_info->descriptor2.vfront = (p_edid[0x52] >> 4) | ((p_edid[0x53] & 0xC) << 8);
	edid_info->descriptor2.vsync_width = (p_edid[0x52] & 0xF) | ((p_edid[0x53] & 0x3) << 8);
	htotal = edid_info->descriptor2.hactive + edid_info->descriptor2.hblank;
	vtotal = edid_info->descriptor2.vactive + edid_info->descriptor2.vblank;
	edid_info->descriptor2.htotal = htotal;
	edid_info->descriptor2.vtotal = vtotal;
	edid_info->descriptor2.framerate =
		edid_info->descriptor2.pixel_clk * 1000 / htotal * 1000 / vtotal;
	return ret;
}

u16 rx_get_tag_code(u_char *edid_data)
{
	u16 tag_code = TAG_MAX;
	u16 tmp_tag = 0;
	unsigned int ieee_oui = 0;

	if (!edid_data)
		return tag_code;

	tmp_tag = *edid_data >> 5;
	/* extern tag */
	if (tmp_tag == USE_EXTENDED_TAG) {
		if (edid_data[1] == VSVDB_TAG) {
			ieee_oui = (edid_data[4] << 16) |
				(edid_data[3] << 8) | edid_data[2];
			if (ieee_oui == 0x00D046)
				tag_code = VSVDB_DV_TAG;
			else if (ieee_oui == 0x90848B)
				tag_code = VSVDB_HDR10P_TAG;
		} else if (edid_data[1] == HDR_STATIC_TAG) {
			tag_code = EXTENDED_HDR_STATIC_TAG;
		} else if (edid_data[1] == HDR_DYNAMIC_TAG) {
			tag_code = EXTENDED_HDR_DYNAMIC_TAG;
		} else {
			tag_code =
				(USE_EXTENDED_TAG << 8) | edid_data[1];
		}
	} else if (tmp_tag == VENDOR_TAG) {
		/* diff VSDB with HF-VSDB */
		ieee_oui = (edid_data[3] << 16) |
			(edid_data[2] << 8) | edid_data[1];
		if (ieee_oui == 0x000C03)
			tag_code = tmp_tag;
		else if (ieee_oui == 0xC45DD8)
			tag_code = HF_VENDOR_DB_TAG;
		else if (ieee_oui == 0x00001A)
			tag_code = VSDB_FREESYNC_TAG;
	} else {
		tag_code = tmp_tag;
	}

	return tag_code;
}

u_int rx_get_ceadata_offset(u_char *cur_edid, u_char *addition)
{
	u16 tag = 0;
	struct data_block_location_s ret = {0};

	if (!cur_edid || !addition)
		return 0;

	tag = rx_get_tag_code(addition);
	ret = rx_get_cea_tag_offset(cur_edid, tag);
	return ret.pos[0];
}

u_int rx_get_eeodb(u8 *cur_edid)
{
	int i = 0;
	unsigned char max_offset = 0;

	if (!cur_edid)
		return 0;
	if (!cur_edid[126])
		return 0;
	i = EDID_DEFAULT_START;/*block check start index*/
	max_offset = cur_edid[130] + EDID_EXT_BLK_OFF;

	while (i < max_offset) {
		if (rx_get_tag_code(cur_edid + i) == EXTENDED_HF_EEODB)
			return i;
		i += (1 + (*(cur_edid + i) & 0x1f));
	}

	return 0;
}

struct data_block_location_s rx_get_cea_tag_offset(u8 *cur_edid, u16 tag_code)
{
	int i = 0, j = 0;
	u16 max_offset = 0;
	u8 cta_num = 0;
	struct data_block_location_s ret = {0};

	memset(&ret, 0, sizeof(struct data_block_location_s));
	if (!cur_edid || !cur_edid[126])
		return ret;
	if (!is_valid_edid_data(cur_edid))
		return ret;

	cta_num = rx_edid_cta_blk_num(cur_edid);
	for (j = 1; j <= cta_num; ++j) {
		i = EDID_BLK_SIZE * j + 4;
		max_offset = cur_edid[j * EDID_BLK_SIZE + 2] + EDID_BLK_SIZE * j;
		while (i < max_offset) {
			if (tag_code == rx_get_tag_code(cur_edid + i)) {
				if (log_level & EDID_LOG)
					rx_pr("find tag: %#x, start addr: %d\n", tag_code, i);
				ret.pos[ret.num] = i;
				ret.num++;
				if (ret.num >= 10)
					break;
			}
			i += (1 + (*(cur_edid + i) & 0x1f));
		}
	}

	return ret;
}

bool is_cta_blk(u8 *cur_edid)
{
	bool ret = false;

	if (!cur_edid)
		return ret;
	if (cur_edid[0] == 2 && cur_edid[1] == 3)
		ret = true;

	return ret;
}

int rx_get_cta_free_size(u8 *cur_edid, unsigned int size)
{
	int block_start = 0;

	if (!cur_edid || !is_cta_blk(cur_edid))
		return -1;
	/*get description offset*/
	block_start = cur_edid[EDID_DESCRIP_OFFSET];
	if (log_level & EDID_LOG)
		rx_pr("%s block_start:%d\n", __func__, block_start);
	/*find the empty data index*/
	while ((block_start < size - 1) &&
		(cur_edid[block_start] > 0)) {
		if (log_level & EDID_LOG)
			rx_pr("%s running:%d\n", __func__, block_start);
		if ((cur_edid[block_start] & 0x1f) == 0)
			break;
		block_start += DETAILED_TIMING_LEN;
	}
	if (log_level & EDID_LOG)
		rx_pr("%s block_start end:%d\n", __func__, block_start);
	/*compute the free size*/
	if (block_start <= size - 1)
		return size - block_start - 1;
	else
		return -1;
}

bool rx_edid_cal_phy_addr(u_int up_addr,
			  u_int portmap,
			  u_char *pedid,
			  u_int *phy_offset,
			  u_int *phy_addr)

{
	u_int root_offset = 0;
	u_int vsdb_offset = 0;
	u_int i = 0;
	bool flag = false;
	struct data_block_location_s ret = {0};

	if (!(pedid && phy_offset && phy_addr))
		return flag;

	/* find phy addr offset */
	ret = rx_get_cea_tag_offset(pedid, VENDOR_TAG);
	vsdb_offset = ret.pos[0];
	if (vsdb_offset == 0x0)
		return flag;

	*phy_offset = vsdb_offset + 4;
	flag = true;
	/* reset phy addr to 0 firstly */
	pedid[vsdb_offset + 4] = 0x0;
	pedid[vsdb_offset + 5] = 0x0;
	if (1) {
		/*get the root index*/
		i = 0;
		while (i < 4) {
			if (((up_addr << (i * 4)) & 0xf000) != 0) {
				root_offset = i;
				break;
			}
			i++;
		}
		if (i == 4)
			root_offset = 4;

		for (i = 0; i < rx_info.port_num; i++) {
			if (root_offset == 0)
				phy_addr[i] = 0xFFFF;
			else
				phy_addr[i] = (up_addr |
				((((portmap >> i * 4) & 0xf) << 12) >>
				(root_offset - 1) * 4));

			phy_addr[i] = rx_exchange_bits(phy_addr[i]);
		}
	} else {
		for (i = 0; i < rx_info.port_num; i++)
			phy_addr[i] = ((portmap >> i * 4) & 0xf) << 4;
	}

	return flag;
}

void rx_edid_reset(u8 port)
{
	if (rx_info.chip_id != CHIP_ID_T6X)
		rx_edid_module_reset();
	else
		rx_edid_module_reset_t6x(port);
}

bool is_ddc_idle(unsigned char port_id)
{
	unsigned int sts = 0;
	unsigned int ddc_sts = 0;
	unsigned int ddc_offset = 0;

	switch (port_id) {
	case 0:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT, E_PORT0);
		break;
	case 1:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_B, E_PORT1);
		break;
	case 2:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_C, E_PORT2);
		break;
	case 3:
		sts = hdmirx_rd_top(TOP_EDID_GEN_STAT_D, E_PORT3);
		break;
	default:
		sts = 0;
		break;
	}

	ddc_sts = (sts >> 20) & 0x1f;
	ddc_offset = sts & 0xff;

	if (ddc_sts == 0 &&
	    (ddc_offset == 0xff ||
	     ddc_offset == 0))
		return true;

	if (log_level & VIDEO_LOG)
		rx_pr("ddc busy\n");

	return false;
}

bool is_edid_buff_normal(unsigned char port_id)
{
	unsigned int edid_sts_temp = 0;
	unsigned int edid_addr_sts = 0;

	switch (port_id) {
	case 0:
		edid_sts_temp = hdmirx_rd_top(TOP_EDID_GEN_STAT, E_PORT0);
		break;
	case 1:
		edid_sts_temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_B, E_PORT1);
		break;
	case 2:
		edid_sts_temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_C, E_PORT2);
		break;
	case 3:
		edid_sts_temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_D, E_PORT3);
		break;
	default:
		edid_sts_temp = 0;
		break;
	}

	edid_addr_sts = edid_sts_temp & 0xffff;
	if (edid_addr_sts > 0x200) {
		if (log_level & EDID_LOG)
			rx_pr("edid buff flow\n");
		return false;
	}

	return true;
}

enum edid_ver_e rx_parse_edid_ver(u8 *p_edid)
{
	struct data_block_location_s ret = {0};

	ret = rx_get_cea_tag_offset(p_edid, HF_VENDOR_DB_TAG);
	if (ret.num > 0)
		return EDID_V21;
	else
		return EDID_V14;
}

enum edid_ver_e get_edid_selection(u8 port)
{
	return rx[port].edid_type.edid_ver;
}

/* @func: seek dd+ atmos bit
 * @param:get audio type info by cec message:
 *	request short audio descriptor
 */
unsigned char rx_parse_arc_aud_type(const unsigned char *buff)
{
	unsigned char tmp[7] = {0};
	unsigned char aud_length = 0;
	unsigned char i = 0;
	unsigned int aud_data = 0;
	int ret = 0;

	aud_length = strlen(buff);
	rx_pr("length = %d\n", aud_length);

	for (i = 0; i < aud_length; i += 6) {
		tmp[6] = '\0';
		memcpy(tmp, buff + i, 6);
		/* ret = sscanf(tmp, "%x", &aud_data); */
		ret = kstrtoint(tmp, 0, &aud_data);
		if (ret != 1)
			rx_pr("kstrtoint failed\n");
		if (aud_data >> 19 == AUDIO_FORMAT_DDP)
			break;
	}
	if (i < aud_length && (aud_data & 0x1) == 0x1) {
		if (need_support_atmos_bit != 1) {
			need_support_atmos_bit = 1;
			hdmi_rx_top_edid_update(rx_info.arc_port);
			if (rx_info.main_port_open && rx_info.main_port != rx_info.arc_port) {
				rx_send_hpd_pulse(rx_info.main_port);
				rx_pr("*update edid-atmos*\n");
			} else {
				pre_port = 0xff;
				rx_pr("update atmos later, in arc port:%s\n",
				      rx_info.main_port == rx_info.arc_port ? "Y" : "N");
			}
		}
	} else {
		if (need_support_atmos_bit) {
			need_support_atmos_bit = 0;
			hdmi_rx_top_edid_update(rx_info.arc_port);
			if (rx_info.main_port_open && rx_info.main_port != rx_info.arc_port) {
				rx_send_hpd_pulse(rx_info.main_port);
				rx_pr("*update edid-no atmos*\n");
			} else {
				pre_port = 0xff;
				rx_pr("update atmos later, in arc port:%s\n",
				      rx_info.main_port == rx_info.arc_port ? "Y" : "N");
			}
		}
	}

	return 0;
}

/*
 * audio parse the atmos info and inform hdmirx via a flag
 */
void rx_set_atmos_flag(bool en)
{
}
EXPORT_SYMBOL(rx_set_atmos_flag);

bool rx_get_atmos_flag(void)
{
	return false;
}
EXPORT_SYMBOL(rx_get_atmos_flag);

unsigned char get_atmos_offset(unsigned char *p_edid)
{
	unsigned char ret = 0;
	return ret;
}

u_char rx_edid_get_aud_sad(u_char *sad_data)
{
	u8 port = rx_info.main_port; //todo
	struct data_block_location_s ret = {0};

	if (rx_info.chip_id == CHIP_ID_NONE)
		return 0;
	u_char *pedid = rx_get_cur_used_edid(port);
	ret = rx_get_cea_tag_offset(pedid, AUDIO_TAG);
	u_char offset = ret.pos[0];
	u_char len = 0;

	if (!sad_data || !offset)
		return 0;
	len = BLK_LENGTH(pedid[offset]);
	memcpy(sad_data, pedid + offset + 1, len);
	return len;
}
EXPORT_SYMBOL(rx_edid_get_aud_sad);

/* audio module parse the short audio descriptors
 * info from ARC/eARC and inform hdmirx
 * if len = 0, revert to original edid audio blk
 */
bool rx_edid_set_aud_sad(u_char *sad, u_char len)
{
	u8 i;

	if (rx_info.chip_id == CHIP_ID_NONE)
		return false;

	if (len > 30 || len % 3 != 0) {
		if (log_level & AUDIO_LOG)
			rx_pr("err sad length: %d\n", len);
		return false;
	}

	if (len == 0 || !sad)
		goto set_flag;

	memset(tmp_sad, 0, sizeof(tmp_sad));
	memcpy(tmp_sad, sad, len);
	tmp_sad_len = len;
	for (i = 0; i < rx_info.port_num; ++i) {
		if (rx[i].hpd_cntl & EDID_CNTL_HPD)
			hdmi_rx_top_edid_update(i);
		else
			break;
	}

	if (rx_info.main_port_open && rx_info.main_port != rx_info.arc_port) {
		if (rx_info.main_port_open &&
			rx[rx_info.main_port].state == FSM_SIG_READY) {
			hdmirx_mute_vpp(true, rx_info.main_port);
			rx[rx_info.main_port].vpp_mute_cnt = vpp_mute_cnt;
		}
			rx_send_hpd_pulse(rx_info.main_port);
		if (log_level & AUDIO_LOG)
			rx_pr("*update aud SAD len: %d*\n", len);
	} else {
		if (rx_info.chip_id < CHIP_ID_T3X)
			pre_port = 0xff;
		else
			sad_set_hpd = true;
		if (log_level & AUDIO_LOG)
			rx_pr("update aud SAD later, in arc port:%s\n",
			      rx_info.main_port == rx_info.arc_port ? "Y" : "N");
	}
set_flag:
	for (i = 0; i < rx_info.port_num; ++i)
		rx[i].hpd_cntl |= AUD_CNTL_HPD;
	return true;
}
EXPORT_SYMBOL(rx_edid_set_aud_sad);

void rx_earc_hpd_cntl(void)
{
	if (rx_info.chip_id == CHIP_ID_NONE)
		return;
	if (rx_info.main_port_open && rx_info.main_port == rx_info.arc_port) {
		rx_send_hpd_pulse(rx_info.main_port);
		if (log_level & AUDIO_LOG)
			rx_pr("earc_hpd_cntl\n");
	} else {
		schedule_work(&earc_hpd_dwork);
		pre_port = 0xff;
	}
}
EXPORT_SYMBOL(rx_earc_hpd_cntl);

bool rx_edid_update_aud_blk(u_char *pedid,
			    u_char *sad_data,
			    u_char len)
{
	u_char tmp_aud_blk[31] = {0};

	if (!pedid)
		return false;
	/* if len = 0, revert to original edid */
	if (len == 0 || len > 30 || len % 3 != 0) {
		if (log_level & AUDIO_LOG)
			rx_pr("err SAD length: %d\n", len);
		return false;
	}
	tmp_aud_blk[0] = (0x1 << 5) | len;
	memcpy(tmp_aud_blk + 1, sad_data, len);
	/* place aud data blk to blk index = 0x1 */
	splice_data_blk_to_edid(pedid, tmp_aud_blk, 0x1);
	return true;
}

/* update atmos bit
 * it will also update audio data blk(if required from Audio)
 */
unsigned char rx_edid_update_sad(unsigned char *p_edid)
{
	unsigned char offset = 0;
	int i = 0, k = 0;
	unsigned char buff[32] = {0};

	if (need_support_atmos_bit != 0xff)	{
		offset = get_atmos_offset(p_edid);
		if (offset == 0) {
			if (log_level & EDID_LOG)
				rx_pr("can not find atmos info\n");
		} else {
			if (need_support_atmos_bit)
				p_edid[offset] = 1;
			else
				p_edid[offset] = 0;
			if (log_level & EDID_LOG)
				rx_pr("offset = %d\n", offset);
		}
	}
	if (log_level & EDID_LOG) {
		for (i = 0; i < tmp_sad_len; ++i)
			k += sprintf(buff + k, "%02x", tmp_sad[i]);
		pr_debug("audio data:%s\n", buff);
	}
	rx_edid_update_aud_blk(p_edid, tmp_sad, tmp_sad_len);
	return 0;
}

void rx_edid_update_vsvdb(u_char *pedid,
				 u_char *add_data, u_char len)
{
	/* if len = 0xFF, revert to original edid, no change */
	if (!pedid || !add_data || len == 0xFF)
		return;

	if (hdmirx_repeat_support())
		return;

	/* if len = 0, means disable.
	 * now only consider VSVDB of DV, not HDR10+
	 */
	if (len == 0)
		edid_rm_db_by_tag(pedid, VSVDB_DV_TAG);
	else
		splice_tag_db_to_edid(pedid, add_data,
				      VSVDB_DV_TAG);
}

bool need_update_edid(u8 port)
{
	bool ret = false;

	/* for TM2 with independent edid on each port,
	 * no need to update edid unless edid change.
	 * but for chips <= TL1, or
	 * edid version is set to auto, still need to
	 * update edid on port open
	 */
	if (rx_info.chip_id <= CHIP_ID_TL1)
		ret = true;

	return ret;
}

void rx_edid_print_vic_fmt(unsigned char i,
			   unsigned char hdmi_vic)
{
	/* CTA-861-G: Table-18 */
	/* SVD = 128, 254, 255 are reserved */
	if (hdmi_vic >= 1 && hdmi_vic <= 127) {
		rx_pr("\tSVD#%2d: vic(%3d), format: %s\n",
		      i + 1, hdmi_vic, hdmi_fmt[hdmi_vic]);
	} else if (hdmi_vic >= 193 && hdmi_vic <= 219) {
		/* from first new set */
		rx_pr("\tSVD#%2d: vic(%3d), format: %s\n",
		      i + 1, hdmi_vic, hdmi_fmt[hdmi_vic - 65]);
	} else if (hdmi_vic >= 220 && hdmi_vic <= 253) {
		/* from second new set: 8bit VIC */
	}
}

/* manufacturer name
 * offset 0x8~0x9
 */
static void get_edid_manufacturer_name(unsigned char *buff,
				       unsigned char start,
				       struct edid_info_s *edid_info)
{
	int i = 0;
	unsigned char uppercase[26] = {0};
	unsigned char brand[3] = {0};

	if (!edid_info || !buff)
		return;
	/* Fill array uppercase with 'A' to 'Z' */
	for (i = 0; i < 26; i++)
		uppercase[i] = 'A' + i;
	/* three 5-byte AscII code, such as 'A' = 00001, 'C' = 00011,*/
	brand[0] = buff[start] >> 2;
	brand[1] = ((buff[start] & 0x3) << 3) + (buff[start + 1] >> 5);
	brand[2] = buff[start + 1] & 0x1f;
	for (i = 0; i < 3; i++)
		edid_info->manufacturer_name[i] = uppercase[brand[i] - 1];
}

/* product code
 * offset 0xA~0xB
 */
static void get_edid_product_code(unsigned char *buff,
				  unsigned char start,
				  struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	edid_info->product_code = buff[start + 1] << 8 | buff[start];
}

/* serial number
 * offset 0xC~0xF
 */
static void get_edid_serial_number(unsigned char *buff,
				   unsigned char start,
				   struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	edid_info->serial_number = buff[start + 3] << 24 |
				   buff[start + 2] << 16 |
				   buff[start + 1] << 8 |
				   buff[start];
}

/* manufacture date
 * offset 0x10~0x11
 */
static void get_edid_manufacture_date(unsigned char *buff,
				      unsigned char start,
				      struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	/* week of manufacture:
	 * 0: not specified
	 * 0x1~0x35: valid week
	 * 0x36~0xfe: reserved
	 * 0xff: model year is specified
	 */
	if (buff[start] == 0 ||
	    (buff[start] >= 0x36 && buff[start] <= 0xfe))
		edid_info->product_week = 0;
	else
		edid_info->product_week = buff[start];
	/* year of manufacture,
	 * or model year (if specified by week=0xff)
	 */
	edid_info->product_year = buff[start + 1] + 1990;
}

/* The EDID version in base block
 * offset 0x12 and 0x13
 *static void get_edid_version(unsigned char *buff,
 *	unsigned char start, struct edid_info_s *edid_info)
 *{
 *	if (!edid_info || !buff)
 *		return;
 *	edid_info->edid_version = buff[start];
 *	edid_info->edid_revision = buff[start+1];
 *}
 */

/* Basic Display Parameters and Features
 * offset 0x14~0x18
 */
static void get_edid_display_parameters(unsigned char *buff,
					unsigned char start,
					struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	if (buff[start] & 0x80) {
		edid_info->display_param.video_signal =
			1;
		edid_info->display_param.color_bit_depth =
			buff[start] & 0x70;
		edid_info->display_param.digital_support =
			buff[start] & 0xf;
	} else {
		edid_info->display_param.video_signal =
			0;
		edid_info->display_param.signal_level_standard =
			buff[start] & 0x60;
		edid_info->display_param.video_setup =
			buff[start] & 0x10;
		edid_info->display_param.sync_type =
			buff[start] & 0xe;
		edid_info->display_param.serrations =
			buff[start] & 0x1;
	}

	edid_info->display_param.horizontal_val = buff[start + 1];
	edid_info->display_param.vertical_val = buff[start + 2];
	edid_info->display_param.gamma = buff[start + 3];

	edid_info->display_param.display_power =
		(buff[start + 4] & 0xe0) >> 5;
	edid_info->display_param.display_color_type =
		(buff[start + 4] & 0x18) >> 3;
	edid_info->display_param.other_feature =
		buff[start + 4] & 0x7;
}

/* Color Characteristics. Color Characteristics provides information about
 * the display device's chromaticity and color temperature parameters
 * (white temperature in degrees Kelvin)
 * offset 0x19~0x22
 */
static void get_edid_color_characteristics(unsigned char *buff,
					   unsigned char start,
					   struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	edid_info->color_chara.red_x =
		((buff[start] & 0x40) >> 6) |
		((buff[start] & 0x80) >> 6) |
		(buff[start + 2] << 2);
	edid_info->color_chara.red_y =
		((buff[start] & 0x10) >> 4) |
		((buff[start] & 0x20) >> 4) |
		(buff[start + 3] << 2);
	edid_info->color_chara.green_x =
		((buff[start] & 0x4) >> 2) |
		((buff[start] & 0x8) >> 2) |
		(buff[start + 4] << 2);
	edid_info->color_chara.green_y =
		((buff[start] & 0x1) >> 0) |
		((buff[start] & 0x2) >> 0) |
		(buff[start + 5] << 2);
	edid_info->color_chara.blue_x =
		((buff[start + 1] & 0x40) >> 6) |
		((buff[start + 1] & 0x80) >> 6) |
		(buff[start + 6] << 2);
	edid_info->color_chara.blue_y =
		((buff[start + 1] & 0x10) >> 4) |
		((buff[start + 1] & 0x20) >> 4) |
		(buff[start + 7] << 2);
	edid_info->color_chara.white_x =
		((buff[start + 1] & 0x4) >> 2) |
		((buff[start + 1] & 0x8) >> 2) |
		(buff[start + 8] << 2);
	edid_info->color_chara.white_y =
		((buff[start + 1] & 0x1) >> 0) |
		((buff[start + 1] & 0x2) >> 0) |
		(buff[start + 9] << 2);
}

/* established timings are computer display timings recognized by VESA.
 * offset 0x23~0x25
 */
static void get_edid_established_timings(unsigned char *buff,
					 unsigned char start,
					 struct edid_info_s *edid_info)
{
	if (!edid_info || !buff)
		return;
	rx_pr("established timing:\n");
	/* each bit for an established timing */
	if (buff[start] & (1 << 5))
		rx_pr("\t640*480p60hz\n");
	if (buff[start] & (1 << 0))
		rx_pr("\t800*600p60hz\n");
	if (buff[start + 1] & (1 << 3))
		rx_pr("\t1024*768p60hz\n");
}

/* Standard timings are those either recognized by VESA
 * through the VESA Discrete Monitor Timing or Generalized
 * Timing Formula standards. Each timing is two bytes.
 * offset 0x26~0x35
 */
static void get_edid_standard_timing(unsigned char *buff,
				     unsigned char start,
				     unsigned int length,
				     struct edid_info_s *edid_info)
{
	unsigned int  i = 0, img_aspect_ratio = 0;
	int hactive_pixel = 0, vactive_pixel = 0, refresh_rate = 0;
	int asp_ratio[] = {
		80 * 10 / 16,
		80 * 3 / 4,
		80 * 4 / 5,
		80 * 9 / 16,
	};/*multiple 80 first*/
	if (!edid_info || !buff)
		return;
	rx_pr("standard timing:\n");
	for (i = 0; i < length; i = i + 2) {
		if (buff[start + i] != 0x01 && buff[start + i + 1] != 0x01) {
			hactive_pixel = (int)((buff[start + i] + 31) * 8);
			/* image aspect ratio:
			 * 0 -> 16:10
			 * 1 -> 4:3
			 * 2 -> 5:4
			 * 3 -> 16:9
			 */
			img_aspect_ratio = (buff[start + i + 1] >> 6) & 0x3;
			vactive_pixel =
				hactive_pixel * asp_ratio[img_aspect_ratio] / 80;
			refresh_rate = (int)(buff[start + i + 1] & 0x3F) + 60;
			rx_pr("\t%d*%dP%dHz\n", hactive_pixel,
			      vactive_pixel, refresh_rate);
		}
	}
}

static void get_edid_monitor_name(unsigned char *p_edid,
				  unsigned char start,
				  struct edid_info_s *edid_info)
{
	unsigned char i = 0, j = 0;

	if (!p_edid || !edid_info)
		return;
	/* CEA861-F Table83 */
	for (i = 0; i < 4; i++) {
		/* 0xFC denotes that last 13 bytes of this
		 * descriptor block contain Monitor name
		 */
		if (p_edid[start + i * 18 + 3] == 0xFC)
			break;
	}
	/* if name < 13 bytes, terminate name with 0x0A
	 * and fill remainder of 13 bytes with 0x20
	 */
	for (j = 0; j < 13; j++) {
		if (p_edid[start + i * 18 + 5 + j] == 0x0A)
			break;
		edid_info->monitor_name[j] = p_edid[0x36 + i * 18 + 5 + j];
	}
}

static void get_edid_range_limits(unsigned char *p_edid,
				  unsigned char start,
				  struct edid_info_s *edid_info)
{
	unsigned char i = 0;

	for (i = 0; i < 4; i++) {
		/* 0xFD denotes that last 13 bytes of this
		 * descriptor block contain monitor range limits
		 */
		if (p_edid[start + i * 18 + 3] == 0xFD)
			break;
	}
	/*maximum supported pixel clock*/
	edid_info->max_sup_pixel_clk = p_edid[0x36 + i * 18 + 9] * 10;
}

/* edid parse
 * Descriptor data
 * 0xff monitor S/N
 * 0xfe ASCII data string
 * 0xfd monitor range limits
 * 0xfc monitor name
 * 0xfb color point
 * 0xfa standard timing ID
 */
void edid_parse_block0(u8 *p_edid, struct edid_info_s *edid_info)
{

	if (!p_edid || !edid_info)
		return;
	/* manufacturer name offset 8~9 */
	get_edid_manufacturer_name(p_edid, 8, edid_info);
	/* product code offset 10~11 */
	get_edid_product_code(p_edid, 10, edid_info);
	/* serial number offset 12~15 */
	get_edid_serial_number(p_edid, 12, edid_info);

	/* product date offset 0x10~0x11 */
	get_edid_manufacture_date(p_edid, 0x10, edid_info);
	/* EDID version offset 0x12~0x13*/
	/* get_edid_version(p_edid, 0x12, edid_info); */
	/* Basic Display Parameters and Features offset 0x14~0x18 */
	get_edid_display_parameters(p_edid, 0x14, edid_info);
	/* Color Characteristics offset 0x19~0x22 */
	get_edid_color_characteristics(p_edid, 0x19, edid_info);
	/* established timing offset 0x23~0x25 */
	get_edid_established_timings(p_edid, 0x23, edid_info);
	/* standard timing offset 0x26~0x35*/
	get_edid_standard_timing(p_edid, 0x26, 16, edid_info);
	/* best resolution: hactive*vactive*/
	get_basic_dtd_data(p_edid, edid_info);
	/* monitor name */
	get_edid_monitor_name(p_edid, 0x36, edid_info);
	get_edid_range_limits(p_edid, 0x36, edid_info);
	edid_info->extension_flag = p_edid[0x7e];
	edid_info->block0_chk_sum = p_edid[0x7f];
}

static void get_edid_video_data(unsigned char *buff,
				unsigned char start,
				unsigned char len,
				struct cta_blk_parse_info *edid_info)
{
	unsigned char i = 0, num = 0;

	if (!buff || !edid_info)
		return;
	edid_info->contain_vdb = true;
	num = edid_info->video_db_num;
	edid_info->video_db[num].svd_num = len;
	for (i = 0; i < len; i++) {
		edid_info->video_db[num].hdmi_vic[i] =
			buff[i + start];
	}
	edid_info->video_db_num++;
}

static void get_edid_basic_audio_data(int start,
				enum edid_audio_format_e fmt,
				unsigned char *buff,
				struct cta_blk_parse_info *edid_info)
{
	u8 i = 0;

	i = edid_info->audio_db_num;
	edid_info->audio_db[i].sad[fmt].max_channel =
				(buff[start] & 0x7);
	if (buff[start + 1] & 0x40)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_192khz = 1;
	if (buff[start + 1] & 0x20)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_176_4khz = 1;
	if (buff[start + 1] & 0x10)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_96khz = 1;
	if (buff[start + 1] & 0x08)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_88_2khz = 1;
	if (buff[start + 1] & 0x04)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_48khz = 1;
	if (buff[start + 1] & 0x02)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_44_1khz = 1;
	if (buff[start + 1] & 0x01)
		edid_info->audio_db[i].sad[fmt].usr.ssr.freq_32khz = 1;
}

static void get_edid_audio_data(unsigned char *buff,
				unsigned char start,
				unsigned char len,
				struct cta_blk_parse_info *edid_info)
{
	enum edid_audio_format_e fmt = 0;
	int i = start, num = 0;
	struct pcm_t *pcm = NULL;

	if (!buff || !edid_info)
		return;
	edid_info->contain_adb = true;
	num = edid_info->audio_db_num;
	do {
		fmt = (buff[i] & 0x78) >> 3;/* bit6~3 */
		edid_info->audio_db[num].aud_fmt_sup[fmt] = 1;
		/* CEA-861F page 82: audio data block*/
		switch (fmt) {
		case AUDIO_FORMAT_LPCM:
			pcm = &edid_info->audio_db[num].sad[fmt].bit_rate.pcm;
			if (!pcm)
				return;
			get_edid_basic_audio_data(i, fmt, buff, edid_info);
			if (buff[i + 2] & 0x04)
				pcm->size_24bit = 1;
			if (buff[i + 2] & 0x02)
				pcm->size_20bit = 1;
			if (buff[i + 2] & 0x01)
				pcm->size_16bit = 1;
			break;
		/* CEA861F table50 fmt2~8 byte 3:
		 * Maximum bit rate divided by 8 kHz
		 */
		case AUDIO_FORMAT_AC3:
		case AUDIO_FORMAT_MPEG1:
		case AUDIO_FORMAT_MP3:
		case AUDIO_FORMAT_MPEG2:
		case AUDIO_FORMAT_AAC:
		case AUDIO_FORMAT_DTS:
		case AUDIO_FORMAT_ATRAC:
			get_edid_basic_audio_data(i, fmt, buff, edid_info);
			edid_info->audio_db[num].sad[fmt].bit_rate.others =
				buff[i + 2];
			break;
		/* for audio format code 9~13:
		 * byte3 is dependent on Audio Format Code
		 */
		case AUDIO_FORMAT_OBA:
		case AUDIO_FORMAT_DDP:
		case AUDIO_FORMAT_DTSHD:
		case AUDIO_FORMAT_MAT:
		case AUDIO_FORMAT_DST:
			get_edid_basic_audio_data(i, fmt, buff, edid_info);
			edid_info->audio_db[num].sad[fmt].bit_rate.others = buff[i + 2];
			break;
		/* audio format code 14:
		 * last 3 bits of byte3: profile
		 */
		case AUDIO_FORMAT_WMAPRO:
			get_edid_basic_audio_data(i, fmt, buff, edid_info);
			edid_info->audio_db[num].sad[fmt].bit_rate.others = buff[i + 2];
			break;
		/* case 15: audio coding extension coding type */
		default:
			break;
		}
		i += 3; /*next audio fmt*/
	} while (i < (start + len));
	edid_info->audio_db_num++;
}

static void get_speaker_allocation_data(unsigned char *buff,
				  unsigned char start,
				  struct speaker_alloc_db_s *sadb)
{
	int i = 0;

	if (!buff || !sadb)
		return;
	for (i = 1; i <= 0x80; i = i << 1) {
		switch (buff[start] & i) {
		case 0x80:
			sadb->flw_frw = 1;
			break;
		case 0x40:
			// according to CTA-861-H, rlc/rrc has been deprecated.
			// rlc_rrc is same to bl_br.
			//edid_info->speaker_alloc.rlc_rrc = 1;
			break;
		case 0x20:
			sadb->flc_frc = 1;
			break;
		case 0x10:
			sadb->bc = 1;
			break;
		case 0x08:
			sadb->bl_br = 1;
			break;
		case 0x04:
			sadb->fc = 1;
			break;
		case 0x02:
			sadb->lfe1 = 1;
			break;
		case 0x01:
			sadb->fl_fr = 1;
			break;
		default:
			break;
		}
	}
	for (i = 1; i <= 0x80; i = i << 1) {
		switch (buff[start + 1] & i) {
		case 0x80:
			sadb->tpsil_tpsir = 1;
			break;
		case 0x40:
			sadb->sil_sir = 1;
			break;
		case 0x20:
			sadb->tpbc = 1;
			break;
		case 0x10:
			sadb->lfe2 = 1;
			break;
		case 0x08:
			sadb->ls_rs = 1;
			break;
		case 0x04:
			sadb->tpfc = 1;
			break;
		case 0x02:
			sadb->tpc = 1;
			break;
		case 0x01:
			sadb->tpfl_tpfr = 1;
			break;
		default:
			break;
		}
	}
	for (i = 1; i <= 0x8; i = i << 1) {
		switch (buff[start + 2] & i) {
		case 0x8:
			// according to CTA-861-H, tpls/tprs has been deprecated.
			//edid_info->speaker_alloc.tpls_tprs = 1;
			break;
		case 0x4:
			sadb->btfl_btfr = 1;
			break;
		case 0x2:
			sadb->btfc = 1;
			break;
		case 0x1:
			sadb->tpbl_tpbr = 1;
			break;
		default:
			break;
		}
	}
}

static void get_edid_speaker_data(unsigned char *buff,
				  unsigned char start,
				  unsigned char len,
				  struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	edid_info->contain_sadb = true;
	/* speaker allocation is 3 bytes long*/
	if (len != 3) {
		rx_pr("invalid length for speaker allocation data block: %d\n",
		      len);
		return;
	}
	get_speaker_allocation_data(buff, start, &edid_info->speaker_alloc);
}

static void get_edid_fsdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u32 ieee_oui = 0;
	u8 i = 0, payload_len = 0;

	if (!buff || !edid_info)
		return;
	ieee_oui = buff[start + 2] << 16 |
		   buff[start + 1] << 8 |
		   buff[start];
	payload_len = len - 3;
	if (ieee_oui != 0x00001a) {
		rx_pr("invalid freesync ieee oui\n");
		return;
	}
	if (payload_len > 28) {
		rx_pr("invalid freesync payload length\n");
		return;
	}
	edid_info->contain_fsdb = true;
	edid_info->fsdb.payload_len = payload_len;
	for (i = 0; i < payload_len; ++i)
		edid_info->fsdb.payload[i] = buff[start + 3 + i];
}

/* if is valid vsdb, then return 1
 * if is valid hf-vsdb, then return 2
 * if is invalid db, then return 0
 */
static void get_edid_vs14(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	unsigned char _3d_present_offset;
	unsigned char hdmi_vic_len;
	unsigned char hdmi_vic_offset;
	unsigned char i, j;
	unsigned int ieee_oui;
	unsigned char _3d_struct_all_offset;
	unsigned char hdmi_3d_len;
	unsigned char _3d_struct;
	unsigned char _2d_vic_order_offset;
	unsigned char temp_3d_len;

	if (!buff || !edid_info)
		return;
	/* basic 5 bytes; others: extension fields */
	//coverity fixed
	//if (len < 5) {
		//rx_pr("invalid VSDB length: %d!\n", len);
		//return 0;
	//}
	ieee_oui = (buff[start + 2] << 16) |
		   (buff[start + 1] << 8) |
		   buff[start];
	if (ieee_oui != 0x000C03) {
		rx_pr("invalid IEEE OUI\n");
		return;
	}
	edid_info->vsdb.ieee_first = ieee_oui & 0xff;
	edid_info->vsdb.ieee_second = ieee_oui >> 8 & 0xff;
	edid_info->vsdb.ieee_third = ieee_oui >> 16 & 0xff;
	/* phy addr: 2 bytes*/
	edid_info->vsdb.a = (buff[start + 3] >> 4) & 0xf;
	edid_info->vsdb.b = buff[start + 3] & 0xf;
	edid_info->vsdb.c = (buff[start + 4] >> 4) & 0xf;
	edid_info->vsdb.d = buff[start + 4] & 0xf;
	/* after first 5 bytes: vsdb1.4 extension fields */
	if (len > 5) {
		edid_info->vsdb.support_ai = (buff[start + 5] >> 7) & 0x1;
		edid_info->vsdb.dc_48bit = (buff[start + 5] >> 6) & 0x1;
		edid_info->vsdb.dc_36bit = (buff[start + 5] >> 5) & 0x1;
		edid_info->vsdb.dc_30bit = (buff[start + 5] >> 4) & 0x1;
		edid_info->vsdb.dc_y444 = (buff[start + 5] >> 3) & 0x1;
		edid_info->vsdb.dvi_dual = buff[start + 5] & 0x1;
	}
	if (len > 6)
		edid_info->vsdb.max_tmds_clk = buff[start + 6];
	if (len > 7) {
		edid_info->vsdb.latency_fields_present =
			(buff[start + 7] >> 7) & 0x1;
		edid_info->vsdb.i_latency_fields_present =
			(buff[start + 7] >> 6) & 0x1;
		edid_info->vsdb.hdmi_video_present =
			(buff[start + 7] >> 5) & 0x1;
		edid_info->vsdb.cnc3 = (buff[start + 7] >> 3) & 0x1;
		edid_info->vsdb.cnc2 = (buff[start + 7] >> 2) & 0x1;
		edid_info->vsdb.cnc1 = (buff[start + 7] >> 1) & 0x1;
		edid_info->vsdb.cnc0 = buff[start + 7] & 0x1;
	}
	if (edid_info->vsdb.latency_fields_present) {
		if (len < 10) {
			rx_pr("invalid vsdb len for latency: %d\n", len);
			return;
		}
		edid_info->vsdb.video_latency = buff[start + 8];
		edid_info->vsdb.audio_latency = buff[start + 9];
		_3d_present_offset = 10;
	} else {
		rx_pr("latency fields not present\n");
		_3d_present_offset = 8;
	}
	if (edid_info->vsdb.i_latency_fields_present) {
		/* I_Latency_Fields_Present shall be zero
		 * if Latency_Fields_Present is zero.
		 */
		if (edid_info->vsdb.latency_fields_present) {
			rx_pr("i_latency_fields should not be set\n");
		} else if (len < 12) {
			rx_pr("invalid vsdb len for i_latency: %d\n", len);
			return;
		}
		edid_info->vsdb.interlaced_video_latency = buff[start + 10];
		edid_info->vsdb.interlaced_audio_latency = buff[start + 11];
		_3d_present_offset = 12;
	}
	/* HDMI_Video_present: If set then additional video format capabilities
	 * are described by using the fields starting after the Latency
	 * area. This consists of 4 parts with the order described below:
	 * 1 byte containing the 3D_present flag and other flags
	 * 1 byte with length fields HDMI_VIC_LEN and HDMI_3D_LEN
	 * zero or more bytes for information about HDMI_VIC formats supported
	 * (length of this field is indicated by HDMI_VIC_LEN).
	 * zero or more bytes for information about 3D formats supported
	 * (length of this field is indicated by HDMI_3D_LEN)
	 */
	if (edid_info->vsdb.hdmi_video_present) {
		/* if hdmi video present,
		 * 2 additional bytes at least will present
		 * 1 byte containing the 3D_present flag and other flags
		 * 1 byte with length fields HDMI_VIC_LEN and HDMI_3D_LEN
		 * 0 or more bytes for info about HDMI_VIC formats supported
		 * 0 or more bytes for info about 3D formats supported
		 */
		if (len < _3d_present_offset + 2) {
			rx_pr("invalid vsdb length for hdmi video: %d\n", len);
			return;
		}
		edid_info->vsdb._3d_present =
			(buff[start + _3d_present_offset] >> 7) & 0x1;
		edid_info->vsdb._3d_multi_present =
			(buff[start + _3d_present_offset] >> 5) & 0x3;
		edid_info->vsdb.image_size =
			(buff[start + _3d_present_offset] >> 3) & 0x3;
		edid_info->vsdb.hdmi_vic_len =
			(buff[start + _3d_present_offset + 1] >> 5) & 0x7;
		edid_info->vsdb.hdmi_3d_len =
			(buff[start + _3d_present_offset + 1]) & 0x1f;
		/* parse 4k2k video format, 4 4k2k format maximum*/
		hdmi_vic_offset = _3d_present_offset + 2;
		hdmi_vic_len = edid_info->vsdb.hdmi_vic_len;
		if (hdmi_vic_len > 4) {
			rx_pr("invalid hdmi vic len: %d\n",
			      edid_info->vsdb.hdmi_vic_len);
			return;
		}

		/* HDMI_VIC_LEN may be 0 */
		if (len < hdmi_vic_offset + hdmi_vic_len) {
			rx_pr("invalid length for 4k2k: %d\n", len);
			return;
		}
		for (i = 0; i < hdmi_vic_len; i++) {
			if (buff[start + hdmi_vic_offset + i] == 1)
				edid_info->vsdb.hdmi_4k2k_30hz_sup = 1;
			else if (buff[start + hdmi_vic_offset + i] == 2)
				edid_info->vsdb.hdmi_4k2k_25hz_sup = 1;
			else if (buff[start + hdmi_vic_offset + i] == 3)
				edid_info->vsdb.hdmi_4k2k_24hz_sup = 1;
			else if (buff[start + hdmi_vic_offset + i] == 4)
				edid_info->vsdb.hdmi_smpte_sup = 1;
		}

		/* 3D info parse */
		_3d_struct_all_offset =
			hdmi_vic_offset + hdmi_vic_len;
		hdmi_3d_len = edid_info->vsdb.hdmi_3d_len;
		/* there may be additional 0 present after 3D info  */
		if (len < _3d_struct_all_offset + hdmi_3d_len) {
			rx_pr("invalid vsdb length for 3d: %d\n", len);
			return;
		}
		/* 3d_multi_present: hdmi1.4 spec page155:
		 * 0: neither structure or mask present,
		 * 1: only 3D_Structure_ALL_15_0 is present
		 *    and assigns 3D formats to all of the
		 *    VICs listed in the first 16 entries
		 *    in the EDID
		 * 2: 3D_Structure_ALL_15_0 and 3D_MASK_15_0
		 *    are present and assign 3D formats to
		 *    some of the VICs listed in the first
		 *    16 entries in the EDID.
		 * 3: neither structure or mask present,
		 *    Reserved for future use.
		 */
		if (edid_info->vsdb._3d_multi_present == 1) {
			edid_info->vsdb._3d_struct_all =
				(buff[start + _3d_struct_all_offset] << 8) +
				buff[start + _3d_struct_all_offset + 1];
			_2d_vic_order_offset =
				_3d_struct_all_offset + 2;
			temp_3d_len = 2;
		} else if (edid_info->vsdb._3d_multi_present == 2) {
			edid_info->vsdb._3d_struct_all =
				(buff[start + _3d_struct_all_offset] << 8) +
				 buff[start + _3d_struct_all_offset + 1];
			edid_info->vsdb._3d_mask_15_0 =
				(buff[start + _3d_struct_all_offset + 2] << 8) +
				 buff[start + _3d_struct_all_offset + 3];
			_2d_vic_order_offset =
				_3d_struct_all_offset + 4;
			temp_3d_len = 4;
		} else {
			_2d_vic_order_offset =
				_3d_struct_all_offset;
			temp_3d_len = 0;
		}
		i = _2d_vic_order_offset;
		for (j = 0; (temp_3d_len < hdmi_3d_len) &&
		     (j < 16); j++) {
			edid_info->vsdb._2d_vic[j]._2d_vic_order =
				(buff[start + i] >> 4) & 0xF;
			edid_info->vsdb._2d_vic[j]._3d_struct =
				buff[start + i] & 0xF;
			_3d_struct =
				edid_info->vsdb._2d_vic[j]._3d_struct;
			/* hdmi1.4 spec page156
			 * if 3D_Structure_X is 0000~0111,
			 * 3D_Detail_X shall not be present,
			 * otherwise shall be present
			 */
			if (_3d_struct >= 0x8 && _3d_struct <= 0xF) {
				edid_info->vsdb._2d_vic[j]._3d_detail =
					(buff[start + i + 1] >> 4) & 0xF;
				i += 2;
				temp_3d_len += 2;
				if (temp_3d_len > hdmi_3d_len) {
					rx_pr("invalid len for 3d_detail: %d\n",
					      len);
					break;
				}
			} else {
				i++;
				temp_3d_len++;
			}
		}
		edid_info->vsdb._2d_vic_num = j;
	}
}

static void get_edid_hfdb(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct hf_s *hf_db)
{
	hf_db->version =
		buff[start + 3];
	hf_db->max_tmds_rate =
		buff[start + 4];
	//pb3
	hf_db->scdc_present =
		(buff[start + 5] >> 7) & 0x1;
	hf_db->rr_cap =
		(buff[start + 5] >> 6) & 0x1;
	hf_db->cable_status =
		(buff[start + 5] >> 5) & 0x1;
	hf_db->ccbpci =
		(buff[start + 5] >> 4) & 0x1;
	hf_db->lte_340m_scramble =
		(buff[start + 5] >> 3) & 0x1;
	hf_db->independ_view =
		(buff[start + 5] >> 2) & 0x1;
	hf_db->dual_view =
		(buff[start + 5] >> 1) & 0x1;
	hf_db->_3d_osd_disparity =
		buff[start + 5] & 0x1;
	//pb4
	hf_db->max_frl_rate =
		(buff[start + 6] >> 4) & 0x0f;
	hf_db->uhd_vic =
		(buff[start + 6] >> 3) & 0x1;
	hf_db->dc_48bit_420 =
		(buff[start + 6] >> 2) & 0x1;
	hf_db->dc_36bit_420 =
		(buff[start + 6] >> 1) & 0x1;
	hf_db->dc_30bit_420 =
		buff[start + 6] & 0x1;
	//pb5
	if (len <= 8)
		return;
	hf_db->fapa_end_extended =
		(buff[start + 7] >> 7) & 0x1;
	hf_db->qms =
		(buff[start + 7] >> 6) & 0x1;
	hf_db->m_delta =
		(buff[start + 7] >> 5) & 0x1;
	hf_db->cinema_vrr =
		(buff[start + 7] >> 4) & 0x1;
	hf_db->neg_mvrr =
		(buff[start + 7] >> 3) & 0x1;
	hf_db->fva =
		(buff[start + 7] >> 2) & 0x1;
	hf_db->allm =
		(buff[start + 7] >> 1) & 0x1;
	hf_db->fapa_start_location =
		buff[start + 7] & 0x1;
	//pb6
	if (len <= 9)
		return;
	hf_db->vrr_max_hi =
		(buff[start + 8] >> 6) & 0x03;
	hf_db->vrr_min =
		(buff[start + 8]) & 0x3f;
	//pb7
	hf_db->vrr_max_lo =
		buff[start + 9];
	//pb8
	if (len <= 11)
		return;
	hf_db->dsc_1p2 =
		(buff[start + 10] >> 7) & 0x1;
	hf_db->dsc_native_420 =
		(buff[start + 10] >> 6) & 0x1;
	hf_db->qms_tfr_max =
		(buff[start + 10] >> 5) & 0x1;
	hf_db->qms_tfr_min =
		(buff[start + 10] >> 4) & 0x1;
	hf_db->dsc_all_bpp =
		(buff[start + 10] >> 3) & 0x1;
	hf_db->dsc_16bpc =
		(buff[start + 10] >> 2) & 0x1;
	hf_db->dsc_12bpc =
		(buff[start + 10] >> 1) & 0x1;
	hf_db->dsc_10bpc =
		buff[start + 10] & 0x1;
	//pb9
	hf_db->dsc_max_frl_rate =
		(buff[start + 11] >> 4) & 0xf;
	hf_db->dsc_max_slices =
		(buff[start + 11]) & 0xf;
	//pb10
	hf_db->dsc_total_chunk_kbytes =
		(buff[start + 12]) & 0x3f;
}

static void get_edid_hf_vsdb(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	unsigned int ieee_oui = 0;

	if (!buff || !edid_info)
		return;
	if (len < 7 || len > 13) {
		rx_pr("invalid hf_vsdb\n");
		return;
	}
	ieee_oui = (buff[start + 2] << 16) |
		   (buff[start + 1] << 8) |
		   buff[start];
	if (ieee_oui != 0xC45DD8) {
		rx_pr("invalid IEEE OUI\n");
		return;
	}
	edid_info->contain_hf_vsdb = true;
	edid_info->hf_vsdb.ieee_first = ieee_oui & 0xff;
	edid_info->hf_vsdb.ieee_second = ieee_oui >> 8 & 0xff;
	edid_info->hf_vsdb.ieee_third = ieee_oui >> 16 & 0xff;
	get_edid_hfdb(buff, start, len, &edid_info->hf_vsdb.hf_db);
}

static void get_edid_hf_scdb(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	if (len < 7 || len > 13) {
		rx_pr("invalid hf_scdb\n");
		return;
	}
	edid_info->contain_hf_scdb = true;
	edid_info->hf_scdb.resved0 = buff[start];
	edid_info->hf_scdb.resved1 = buff[start + 1];
	get_edid_hfdb(buff, start - 1, len, &edid_info->hf_scdb.hf_db);
}

static void get_edid_hf_sbtm(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;

	edid_info->contain_hf_sbtm = true;
	edid_info->hf_sbtm.drdm_ind = (buff[start] & 0x80) >> 7;
	edid_info->hf_sbtm.grdm_support = (buff[start] & 0x60) >> 5;
	edid_info->hf_sbtm.max_sbtm_ver = buff[start] & 0xf;
	if (!edid_info->hf_sbtm.drdm_ind)
		return;
	edid_info->hf_sbtm.gamut = (buff[start + 1] & 0xc0) >> 6;
	edid_info->hf_sbtm.max_rgb = (buff[start + 1] & 0x20) >> 5;
	edid_info->hf_sbtm.use_hgig_drdm = (buff[start + 1] & 0x10) >> 4;
	edid_info->hf_sbtm.hgig_cat_drdm_sel = buff[start + 1] & 0x7;
	if (!edid_info->hf_sbtm.gamut) {
		edid_info->hf_sbtm.red_x_low = buff[start + 2];
		edid_info->hf_sbtm.red_x_high = buff[start + 3];
		edid_info->hf_sbtm.red_y_low = buff[start + 4];
		edid_info->hf_sbtm.red_y_high = buff[start + 5];
		edid_info->hf_sbtm.green_x_low = buff[start + 6];
		edid_info->hf_sbtm.green_x_high = buff[start + 7];
		edid_info->hf_sbtm.green_y_low = buff[start + 8];
		edid_info->hf_sbtm.green_y_high = buff[start + 9];
		edid_info->hf_sbtm.blue_x_low = buff[start + 10];
		edid_info->hf_sbtm.blue_x_high = buff[start + 11];
		edid_info->hf_sbtm.blue_y_low = buff[start + 12];
		edid_info->hf_sbtm.blue_y_high = buff[start + 13];
		edid_info->hf_sbtm.white_x_low = buff[start + 14];
		edid_info->hf_sbtm.white_x_high = buff[start + 15];
		edid_info->hf_sbtm.white_y_low = buff[start + 16];
		edid_info->hf_sbtm.white_y_high = buff[start + 17];
	} else {
		start -= 16;
	}
	if (edid_info->hf_sbtm.use_hgig_drdm)
		return;
	edid_info->hf_sbtm.min_bright_10 = buff[start + 18];
	edid_info->hf_sbtm.peak_bright_100 = buff[start + 19];
	edid_info->hf_sbtm.p0_mant = (buff[start + 20] & 0xfc) >> 2;
	edid_info->hf_sbtm.p0_exp = buff[start + 20] & 0x3;
	edid_info->hf_sbtm.peak_bright_p0 = buff[start + 21];
	edid_info->hf_sbtm.p1_mant = (buff[start + 22] & 0xfc) >> 2;
	edid_info->hf_sbtm.p1_exp = buff[start + 22] & 0x3;
	edid_info->hf_sbtm.peak_bright_p1 = buff[start + 23];
	edid_info->hf_sbtm.p2_mant = (buff[start + 24] & 0xfc) >> 2;
	edid_info->hf_sbtm.p2_exp = buff[start + 24] & 0x3;
	edid_info->hf_sbtm.peak_bright_p2 = buff[start + 25];
	edid_info->hf_sbtm.p3_mant = (buff[start + 26] & 0xfc) >> 2;
	edid_info->hf_sbtm.p3_exp = buff[start + 26] & 0x3;
	edid_info->hf_sbtm.peak_bright_p3 = buff[start + 27];
}

static int get_edid_vsdb(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	u32 ieee_oui = 0, ret = 0;

	if (!buff || !edid_info)
		return 0;
	if (len < 5)
		return 0;
	edid_info->contain_vsdb = true;
	/* basic 5 bytes; others: extension fields */
	//coverity fixed
	//if (len < 5) {
		//rx_pr("invalid VSDB length: %d!\n", len);
		//return 0;
	//}
	ieee_oui = (buff[start + 2] << 16) |
		   (buff[start + 1] << 8) |
		   buff[start];
	switch (ieee_oui) {
	case 0x000C03:
		ret = VENDOR_TAG;
		break;
	case 0xC45DD8:
		ret = HF_VENDOR_DB_TAG;
		break;
	case 0x00001A:
		ret = VSDB_FREESYNC_TAG;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void get_edid_vcdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* vcdb only contain 3 bytes data block. the source should
	 * ignore additional bytes (when present) and continue to
	 * parse the single byte as defined in CEA861-F Table 59.
	 */
	if (len != 2 - 1) {
		rx_pr("invalid length for video cap data block: %d!\n", len);
		/* return; */
	}
	edid_info->contain_vcdb = true;
	edid_info->vcdb.quanti_range_ycc = (buff[start] >> 7) & 0x1;
	edid_info->vcdb.quanti_range_rgb = (buff[start] >> 6) & 0x1;
	edid_info->vcdb.s_PT = (buff[start] >> 4) & 0x3;
	edid_info->vcdb.s_IT = (buff[start] >> 2) & 0x3;
	edid_info->vcdb.s_CE = buff[start] & 0x3;
}

static void get_edid_hdr10p_data(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u32 ieee_oui = 0;

	if (!buff || !edid_info)
		return;
	if (len != 4) {
		rx_pr("invalid HDR10P\n");
		return;
	}
	ieee_oui = buff[start + 2] << 16 |
		   buff[start + 1] << 8 |
		   buff[start];
	if (ieee_oui != 0x90848b) {
		rx_pr("invalid HDR10P ieee oui:\n");
		return;
	}
	edid_info->contain_hdr10p = true;
	edid_info->hdr10pdb.hdr10p_ver = buff[start + 3] & 3;
	edid_info->hdr10pdb.full_frame_peak = (buff[start + 3] >> 2) & 3;
	edid_info->hdr10pdb.peak = buff[start + 3] >> 4;
}

static void get_edid_dvsa_data(unsigned char *buff,
			     unsigned char start,
			     unsigned char len,
			     struct cta_blk_parse_info *edid_info)
{
	u32 ieee_oui = 0;

	if (!buff || !edid_info)
		return;
	if (len != 6) {
		rx_pr("invalid dvsa\n");
		return;
	}
	ieee_oui = buff[start + 2] << 16 |
		   buff[start + 1] << 8 |
		   buff[start];
	if (ieee_oui != 0x00D046) {
		rx_pr("invalid DV ieee oui\n");
		return;
	}
	edid_info->contain_vsadb = true;
	edid_info->dv_vsadb.version = buff[start + 3] & 0x7;
	edid_info->dv_vsadb.headphone = (buff[start + 3] & 0x80) >> 7;
	edid_info->dv_vsadb.center = (buff[start + 3] & 0x10) >> 4;
	edid_info->dv_vsadb.surround = (buff[start + 3] & 0x20) >> 5;
	edid_info->dv_vsadb.height = (buff[start + 3] & 0x40) >> 6;
	edid_info->dv_vsadb.func = buff[start + 4] & 0x1;
}

static void get_edid_dvsv_data(unsigned char *buff,
			     unsigned char start,
			     unsigned char len,
			     struct cta_blk_parse_info *edid_info)
{
	unsigned int ieee_oui = 0;

	if (!buff || !edid_info)
		return;
	if (len != 0xE - 1 &&
	    len != 0x19 - 1 &&
	    len != 0xB - 1 &&
	    len != 0x5 - 1) {
		rx_pr("invalid length for DV vsvdb:%d\n",
		      len);
		return;
	}
	ieee_oui = buff[start + 2] << 16 |
		   buff[start + 1] << 8 |
		   buff[start];
	if (ieee_oui != 0x00D046) {
		rx_pr("invalid DV ieee oui\n");
		return;
	}
	edid_info->dv_vsvdb.length = len + 1;
	edid_info->contain_vsvdb = true;
	edid_info->dv_vsvdb.ieee_oui = ieee_oui;
	edid_info->dv_vsvdb.version = buff[start + 3] >> 5;
	if (edid_info->dv_vsvdb.version == 0x0) {
		/* length except extend code */
		if (len == 0x5 - 1) {
			rx_pr("other DV version\n");
			return;
		}
		if (len != 0x19 - 1) {
			rx_pr("invalid length for DV ver0\n");
			return;
		}
		edid_info->dv_vsvdb.sup_global_dimming =
			(buff[start + 3] >> 2) & 0x1;
		edid_info->dv_vsvdb.sup_2160p60hz =
			(buff[start + 3] >> 1) & 0x1;
		edid_info->dv_vsvdb.sup_yuv422_12bit =
			buff[start + 3] & 0x1;
		edid_info->dv_vsvdb.Rx =
			((buff[start + 4] >> 4) & 0xF) + (buff[start + 5] << 4);
		edid_info->dv_vsvdb.Ry =
			(buff[start + 4] & 0xF) + (buff[start + 6] << 4);
		edid_info->dv_vsvdb.Gx =
			((buff[start + 7] >> 4) & 0xF) + (buff[start + 8] << 4);
		edid_info->dv_vsvdb.Gy =
			(buff[start + 7] & 0xF) + (buff[start + 9] << 4);
		edid_info->dv_vsvdb.Bx =
			((buff[start + 10] >> 4) & 0xF) + (buff[start + 11] << 4);
		edid_info->dv_vsvdb.By =
			(buff[start + 10] & 0xF) + (buff[start + 12] << 4);
		edid_info->dv_vsvdb.Wx =
			((buff[start + 13] >> 4) & 0xF) + (buff[start + 14] << 4);
		edid_info->dv_vsvdb.Wy =
			(buff[start + 13] & 0xF) + (buff[start + 15] << 4);
		edid_info->dv_vsvdb.tminPQ =
			((buff[start + 16] >> 4) & 0xF) + (buff[start + 17] << 4);
		edid_info->dv_vsvdb.tmaxPQ =
			(buff[start + 16] & 0xF) + (buff[start + 18] << 4);
		edid_info->dv_vsvdb.dm_major_ver =
			(buff[start + 19] >> 4) & 0xF;
		edid_info->dv_vsvdb.dm_minor_ver =
			buff[start + 19] & 0xF;
	} else if (edid_info->dv_vsvdb.version == 0x1) {
		/*length except extend code*/
		if (len == 0xB - 1) {
			edid_info->dv_vsvdb.DM_version =
				(buff[start + 3] >> 2) & 0x7;
			edid_info->dv_vsvdb.sup_yuv422_12bit =
				buff[start + 3] & 0x1;
			edid_info->dv_vsvdb.sup_2160p60hz =
				(buff[start + 3] >> 1) & 0x1;
			edid_info->dv_vsvdb.sup_global_dimming =
				buff[start + 4] & 0x1;
			edid_info->dv_vsvdb.colorimetry =
				buff[start + 5] & 0x1;
			edid_info->dv_vsvdb.low_latency =
				buff[start + 6] & 0x3;
			edid_info->dv_vsvdb.target_max_lum =
				(buff[start + 4] >> 1) & 0x7F;
			edid_info->dv_vsvdb.target_min_lum =
				(buff[start + 5] >> 1) & 0x7F;
			edid_info->dv_vsvdb.Rx =
				(buff[start + 9] >> 3) & 0x1f;
			edid_info->dv_vsvdb.Ry =
				(buff[start + 7] & 0x1) +
				((buff[start + 8] & 0x1) << 1) +
				((buff[start + 9] & 0x7) << 2);
			edid_info->dv_vsvdb.Gx =
				(buff[start + 7] >> 1) & 0x7f;
			edid_info->dv_vsvdb.Gy =
				(buff[start + 8] >> 1) & 0x7f;
			edid_info->dv_vsvdb.Bx =
				(buff[start + 6] >> 5) & 0x7;
			edid_info->dv_vsvdb.By =
				(buff[start + 6] >> 2) & 0x7;
		} else if (len == 0xE - 1) {
			edid_info->dv_vsvdb.DM_version =
				(buff[start + 3] >> 2) & 0x7;
			edid_info->dv_vsvdb.sup_2160p60hz =
				(buff[start + 3] >> 1) & 0x1;
			edid_info->dv_vsvdb.sup_yuv422_12bit =
				buff[start + 3] & 0x1;
			edid_info->dv_vsvdb.target_max_lum =
				(buff[start + 4] >> 1) & 0x7F;
			edid_info->dv_vsvdb.sup_global_dimming =
				buff[start + 4] & 0x1;
			edid_info->dv_vsvdb.target_min_lum =
				(buff[start + 5] >> 1) & 0x7F;
			edid_info->dv_vsvdb.colorimetry =
				buff[start + 5] & 0x1;
			edid_info->dv_vsvdb.Rx = buff[start + 7];
			edid_info->dv_vsvdb.Ry = buff[start + 8];
			edid_info->dv_vsvdb.Gx = buff[start + 9];
			edid_info->dv_vsvdb.Gy = buff[start + 10];
			edid_info->dv_vsvdb.Bx = buff[start + 11];
			edid_info->dv_vsvdb.By = buff[start + 12];
		} else {
			rx_pr("invalid length for DV ver1\n");
			return;
		}
	} else if (edid_info->dv_vsvdb.version == 0x2) {
		if (len != 0xb - 1) {
			rx_pr("invalid length for DV ver2\n");
			return;
		}
		edid_info->dv_vsvdb.DM_version =
			(buff[start + 3] >> 2) & 0x7;
		edid_info->dv_vsvdb.sup_yuv422_12bit =
			buff[start + 3] & 0x1;
		edid_info->dv_vsvdb.sup_2160p60hz =
			(buff[start + 3] >> 1) & 0x1;
		edid_info->dv_vsvdb.sup_global_dimming =
			(buff[start + 4] >> 2) & 0x1;
		edid_info->dv_vsvdb.backlt_min_luma =
			buff[start + 4] & 0x3;
		edid_info->dv_vsvdb.interface =
			buff[start + 5] & 0x3;
		edid_info->dv_vsvdb.sup_10b_12b_444 =
			((buff[start + 6] & 0x1) << 1) +
			(buff[start + 7] & 0x1);
		edid_info->dv_vsvdb.tminPQ =
			(buff[start + 4] >> 3) & 0x1f;
		edid_info->dv_vsvdb.tmaxPQ =
			(buff[start + 5] >> 3) & 0x1f;
		edid_info->dv_vsvdb.Rx =
			(buff[start + 8] >> 3) & 0x1f;
		edid_info->dv_vsvdb.Ry =
			(buff[start + 9] >> 3) & 0x1f;
		edid_info->dv_vsvdb.Gx =
			(buff[start + 6] >> 1) & 0x7f;
		edid_info->dv_vsvdb.Gy =
			(buff[start + 7] >> 1) & 0x7f;
		edid_info->dv_vsvdb.Bx =
			buff[start + 8] & 0x7;
		edid_info->dv_vsvdb.By =
			buff[start + 9] & 0x7;
	}
}

static void get_edid_colorimetry_data(unsigned char *buff,
				      unsigned char start,
				      unsigned char len,
				      struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* colorimetry DB only contain 3 bytes data block */
	if (len != 3 - 1) {
		rx_pr("invalid length for colorimetry data block:%d\n",
		      len);
		return;
	}
	edid_info->contain_cdb = true;
	edid_info->color_db.BT2020_RGB = (buff[start] >> 7) & 0x1;
	edid_info->color_db.BT2020_YCC = (buff[start] >> 6) & 0x1;
	edid_info->color_db.bt2020_cycc = (buff[start] >> 5) & 0x1;
	edid_info->color_db.adobe_rgb = (buff[start] >> 4) & 0x1;
	edid_info->color_db.adobe_ycc601 = (buff[start] >> 3) & 0x1;
	edid_info->color_db.sycc601 = (buff[start] >> 2) & 0x1;
	edid_info->color_db.xvycc709 = (buff[start] >> 1) & 0x1;
	edid_info->color_db.xvycc601 = buff[start] & 0x1;
	edid_info->color_db.dci_p3 = (buff[start + 1] >> 7) & 0x1;
	edid_info->color_db.bt2100_ictcp = (buff[start + 1] >> 6) & 0x1;
	edid_info->color_db.srgb = (buff[start + 1] >> 5) & 0x1;
	edid_info->color_db.defaultrgb = (buff[start + 1] >> 4) & 0x1;

	edid_info->color_db.MD3 = (buff[start + 1] >> 3) & 0x1;
	edid_info->color_db.MD2 = (buff[start + 1] >> 2) & 0x1;
	edid_info->color_db.MD1 = (buff[start + 1] >> 1) & 0x1;
	edid_info->color_db.MD0 = buff[start + 1] & 0x1;
}

static void get_edid_static_hdr_data(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	/* Bytes 5-7 are optional to declare. 3 bytes payload at least */
	if (len < 3 - 1) {
		rx_pr("invalid hdr length: %d!\n", len);
		return;
	}
	edid_info->contain_hdr_db = true;
	edid_info->hdr_db.eotf_hlg = (buff[start] >> 3) & 0x1;
	edid_info->hdr_db.eotf_smpte_st_2084 = (buff[start] >> 2) & 0x1;
	edid_info->hdr_db.eotf_hdr = (buff[start] >> 1) & 0x1;
	edid_info->hdr_db.eotf_sdr = buff[start] & 0x1;
	edid_info->hdr_db.hdr_SMD_type1 = buff[start + 1] & 0x1;
	if (len > 2)
		edid_info->hdr_db.hdr_lum_max = buff[start + 2];
	if (len > 3)
		edid_info->hdr_db.hdr_lum_avg = buff[start + 3];
	if (len > 4)
		edid_info->hdr_db.hdr_lum_min = buff[start + 4];
}

static void get_edid_dynamic_hdr_data(unsigned char *buff,
			 unsigned char start,
			 unsigned char len,
			 struct cta_blk_parse_info *edid_info)
{
	u8 i = 0, j = 0;
	u8 offset = 0, length = 0;

	if (!buff || !edid_info)
		return;
	edid_info->contain_hdr_dy = true;
	while (length < len) {
		edid_info->hdr_dy_db.hdr_sup[i].len = buff[start + length];
		edid_info->hdr_dy_db.hdr_sup[i].type =
			buff[start + length + 2] << 8 | buff[start + length + 1];
		if (edid_info->hdr_dy_db.hdr_sup[i].type != 3) {
			offset = 4;
			edid_info->hdr_dy_db.hdr_sup[i].flag = buff[start + length + 3];
		} else {
			offset = 3;
		}
		for (j = 0; j <= edid_info->hdr_dy_db.hdr_sup[i].len - offset; ++j)
			edid_info->hdr_dy_db.hdr_sup[i].field[j] =
				buff[start + length + offset + j];
		edid_info->hdr_dy_db.hdr_sup[i].field_len = j;
		length += edid_info->hdr_dy_db.hdr_sup[i].len + 1;
		++i;
	}
	edid_info->hdr_dy_db.num = i;
}

static void get_edid_y420_vid_data(unsigned char *buff,
				   unsigned char start,
				   unsigned char len,
				   struct cta_blk_parse_info *edid_info)
{
	int i = 0;

	if (!buff || !edid_info)
		return;
	if (len > 6) {
		rx_pr("y420vdb support only 4K50/60hz, smpte50/60hz, len:%d\n",
		      len);
		return;
	}
	edid_info->contain_y420_vdb = true;
	edid_info->y420_vic_len = len;
	for (i = 0; i < len; i++)
		edid_info->y420_vdb_vic[i] = buff[start + i];
}

static void get_edid_y420_cap_map_data(unsigned char *buff,
				       unsigned char start,
				       unsigned char len,
				       struct cta_blk_parse_info *edid_info)
{
	unsigned int i = 0, bit_map = 0;

	if (!buff || !edid_info)
		return;
	/* 31 SVD maximum, 4 bytes needed */
	if (len > 4) {
		rx_pr("31 SVD maximum, all-zero data not needed\n");
		len = 4;
	}
	edid_info->contain_y420_cmdb = true;
	/* When the Length field is set to L==1, the Y420_CMDB does not
	 * include a YCBCR 4:2:0 Capability Bit Map and all the SVDs in
	 * the regular Video Data Block support YCBCR 4:2:0 sampling mode.
	 */
	if (len == 0) {
		edid_info->y420_all_vic = 1;
		return;
	}
	/* bit0: first SVD, bit 1:the second SVD, and so on */
	for (i = 0; i < len; i++)
		bit_map |= (buff[start + i] << (i * 8));
	/* '1' bit in the bit map indicate corresponding SVD support Y420 */
	for (i = 0; (i < len * 8) && (i < 31); i++) {
		if ((bit_map >> i) & 0x1) {
			edid_info->y420_cmdb_vic[i] =
				edid_info->video_db[0].hdmi_vic[i];
		}
	}
}

static void get_edid_vsadb(unsigned char *buff,
			   unsigned char start,
			   unsigned char len,
			   struct cta_blk_parse_info *edid_info)
{
	int i = 0;

	if (!buff || !edid_info)
		return;
	if (len > 27 - 2) {
		/* CTA-861-G 7.5.8 */
		rx_pr("over VSADB max size(27 bytes), len:%d\n", len);
		return;
	} else if (len < 3) {
		rx_pr("no valid IEEE OUI, len:%d\n", len);
		return;
	}
	edid_info->vsadb_ieee =
		buff[start] |
		(buff[start + 1] << 8) |
		(buff[start + 2] << 16);
	if (edid_info->vsadb_ieee == 0x00D046) {
		get_edid_dvsa_data(buff, start, len, edid_info);
		return;
	}
	edid_info->contain_vsadb = true;
	for (i = 0; i < len - 3; i++)
		edid_info->vsadb_payload[i] = buff[start + 3 + i];
}

static void get_edid_hdmi_aud(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0;

	if (!buff || !edid_info)
		return;
	if (len < 3 || len > 23) {
		rx_pr("invalid hdmi audio\n");
		return;
	}
	edid_info->contain_hdmi_aud = true;
	edid_info->hdmi_adb.sup_ms_nonmixed = (buff[start] & 0x4) >> 2;
	edid_info->hdmi_adb.max_stream_cnt = buff[start] & 0x3;
	edid_info->hdmi_adb.num_3d_ad = buff[start + 1] & 0x7;
	for (i = 0; i < edid_info->hdmi_adb.num_3d_ad; ++i) {
		edid_info->hdmi_adb.hdmi_3d_ad[i].format =
			buff[start + 2 + i * 4] & 0xf;
		edid_info->hdmi_adb.hdmi_3d_ad[i].max_channel_num =
			buff[start + 3 + i * 4] & 0x1f;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_192khz =
			(buff[start + 4 + i * 4] & 0x40) >> 6;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_176_4khz =
			(buff[start + 4 + i * 4] & 0x20) >> 5;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_96khz =
			(buff[start + 4 + i * 4] & 0x10) >> 4;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_88_2khz =
			(buff[start + 4 + i * 4] & 0x8) >> 3;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_48khz =
			(buff[start + 4 + i * 4] & 0x4) >> 2;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_44_1khz =
			(buff[start + 4 + i * 4] & 0x2) >> 1;
		edid_info->hdmi_adb.hdmi_3d_ad[i].freq_32khz =
			buff[start + 4 + i * 4] & 0x1;
		if (edid_info->hdmi_adb.hdmi_3d_ad[i].format == 1) {
			edid_info->hdmi_adb.hdmi_3d_ad[i].bit24 =
				(buff[start + 5 + i * 4] & 0x4) >> 2;
			edid_info->hdmi_adb.hdmi_3d_ad[i].bit20 =
				(buff[start + 5 + i * 4] & 0x2) >> 1;
			edid_info->hdmi_adb.hdmi_3d_ad[i].bit16 =
				buff[start + 5 + i * 4] & 0x1;
		} else {
			edid_info->hdmi_adb.hdmi_3d_ad[i].format_value =
				buff[start + 5 + i * 4];
		}
	}
	if (edid_info->hdmi_adb.num_3d_ad > 0) {
		edid_info->hdmi_adb.acat = (buff[start + 6 + i * 4 + 3] & 0xf0) >> 4;
		get_speaker_allocation_data(buff, start + 6 + i * 4, &edid_info->hdmi_adb.sadb);
	}
}

static void get_edid_rcdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	if (!buff || !edid_info)
		return;
	if (len < 5 || len > 11) {
		rx_pr("invalid rcdb\n");
		return;
	}
	edid_info->contain_rcdb = true;
	edid_info->rcdb.display = (buff[start] & 0x80) >> 7;
	edid_info->rcdb.speaker = (buff[start] & 0x40) >> 6;
	edid_info->rcdb.sld = (buff[start] & 0x20) >> 5;
	edid_info->rcdb.speaker_cnt = buff[start] & 0x1f;

	edid_info->rcdb.sadb.flw_frw = (buff[start + 1] & 0x80) >> 7;
	edid_info->rcdb.sadb.flc_frc = (buff[start + 1] & 0x20) >> 5;
	edid_info->rcdb.sadb.bc = (buff[start + 1] & 0x10) >> 4;
	edid_info->rcdb.sadb.bl_br = (buff[start + 1] & 0x8) >> 3;
	edid_info->rcdb.sadb.fc = (buff[start + 1] & 0x4) >> 2;
	edid_info->rcdb.sadb.lfe1 = (buff[start + 1] & 0x2) >> 1;
	edid_info->rcdb.sadb.fl_fr = buff[start + 1] & 0x1;

	edid_info->rcdb.sadb.tpsil_tpsir = (buff[start + 2] & 0x80) >> 7;
	edid_info->rcdb.sadb.sil_sir = (buff[start + 2] & 0x40) >> 6;
	edid_info->rcdb.sadb.tpbc = (buff[start + 2] & 0x20) >> 5;
	edid_info->rcdb.sadb.lfe2 = (buff[start + 2] & 0x10) >> 4;
	edid_info->rcdb.sadb.ls_rs = (buff[start + 2] & 0x8) >> 3;
	edid_info->rcdb.sadb.tpfc = (buff[start + 2] & 0x4) >> 2;
	edid_info->rcdb.sadb.tpc = (buff[start + 2] & 0x2) >> 1;
	edid_info->rcdb.sadb.tpfl_tpfr = buff[start + 2] & 0x1;

	edid_info->rcdb.sadb.btfl_btfr = (buff[start + 3] & 0x4) >> 2;
	edid_info->rcdb.sadb.btfc = (buff[start + 3] & 0x2) >> 1;
	edid_info->rcdb.sadb.tpbl_tpbr = buff[start + 3] & 0x1;
	if (edid_info->rcdb.sld) {
		edid_info->rcdb.x_max = buff[start + 4];
		edid_info->rcdb.y_max = buff[start + 5];
		edid_info->rcdb.z_max = buff[start + 6];
	} else {
		start -= 3;
	}
	if (edid_info->rcdb.display) {
		edid_info->rcdb.display_x = buff[start + 7];
		edid_info->rcdb.display_y = buff[start + 8];
		edid_info->rcdb.display_z = buff[start + 9];
	}
}

static void get_edid_sldb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0, length = 0;

	if (!buff || !edid_info)
		return;
	edid_info->contain_sldb = true;
	while (length < len) {
		edid_info->sldb.sldb_des[i].coord =
			(buff[start + length] & 0x40) >> 6;
		edid_info->sldb.sldb_des[i].active =
			(buff[start + length] & 0x20) >> 5;
		edid_info->sldb.sldb_des[i].channel_idx =
			buff[start + length] & 0x1f;
		edid_info->sldb.sldb_des[i].speaker_id =
			buff[start + length + 1] & 0x1f;
		if (edid_info->sldb.sldb_des[i].coord) {
			edid_info->sldb.sldb_des[i].x =
				buff[start + length + 2];
			edid_info->sldb.sldb_des[i].y =
				buff[start + length + 3];
			edid_info->sldb.sldb_des[i].z =
				buff[start + length + 4];
			length += 5;
		} else {
			length += 2;
		}
		++i;
	}
	edid_info->sldb.len = i;
}

static void get_edid_vfpdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0;

	if (!buff || !edid_info)
		return;
	edid_info->contain_vfpdb = true;
	edid_info->vfpdb.num = len;
	for (i = 0; i < len; ++i)
		edid_info->vfpdb.svr[i] = buff[start + i];
}

static void get_edid_ifdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0, j = 0;
	u8 desc_pos = 0, type = 0;
	u8 pay_len = 0, k = 0;

	if (!buff || !edid_info)
		return;
	if (len < 4 - 1) {
		rx_pr("invalid ifdb length: %d!\n", len);
		return;
	}
	edid_info->contain_ifdb = true;
	edid_info->info_db.vsif_num = buff[start + 1];
	pay_len = (buff[start] & 0xe0) >> 5;
	desc_pos = start + pay_len + 2;
	edid_info->info_db.payload_len = pay_len;
	for (k = 0; k < pay_len; ++k)
		edid_info->info_db.payload[k] = buff[start + k + 2];

	while (i + j < edid_info->info_db.vsif_num) {
		type = buff[desc_pos] & 0x1f;
		pay_len = (buff[desc_pos] & 0xe0) >> 5;
		if (type == 0x1) {
			edid_info->info_db.short_vs_info[i].payload_len = pay_len;

			edid_info->info_db.short_vs_info[i].ieee_oui =
				buff[desc_pos + 1] |
				(buff[desc_pos + 2] << 8) |
				(buff[desc_pos + 3] << 16);
			for (k = 0; k < pay_len; ++k)
				edid_info->info_db.short_vs_info[i].payload[k] =
					buff[desc_pos + 4 + k];
			desc_pos += 4 + pay_len;
			++i;
		} else {
			edid_info->info_db.short_info[j].type_code = type;
			edid_info->info_db.short_info[j].payload_len = pay_len;
			for (k = 0; k < pay_len; ++k)
				edid_info->info_db.short_info[j].payload[k] =
					buff[desc_pos + 1 + k];
			desc_pos += 1 + pay_len;
			++j;
		}
	}
}

static void get_edid_t7vtdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u32 htotal = 0, vtotal = 0;
	u8 num = 0;

	if (!buff || !edid_info)
		return;
	if (len < 20 || len > 27) {
		rx_pr("invalid t7vtdb\n");
		return;
	}
	edid_info->contain_t7vtdb = true;
	num = edid_info->t7vtdbdb_num;
	edid_info->t7vtdb[num].t7_m = (buff[start] & 0x70) >> 4;
	edid_info->t7vtdb[num].dsc_pt = (buff[start] & 0x8) >> 3;
	edid_info->t7vtdb[num].block_revision = buff[start] & 0x7;
	edid_info->t7vtdb[num].pixel_clk = buff[start + 1] |
		buff[start + 2] << 8 |
		buff[start + 3] << 16;
	edid_info->t7vtdb[num].t7y420 = (buff[start + 4] & 0x80) >> 7;
	edid_info->t7vtdb[num]._3d_sup = (buff[start + 4] & 0x60) >> 5;
	edid_info->t7vtdb[num].t7il = (buff[start + 4] & 0x10) >> 4;
	edid_info->t7vtdb[num].t7_aspect_ratio = buff[start + 4] & 0xf;
	edid_info->t7vtdb[num].hactive = (buff[start + 5] |
		buff[start + 6] << 8) + 1;
	edid_info->t7vtdb[num].hblank = (buff[start + 7] |
		buff[start + 8] << 8) + 1;
	edid_info->t7vtdb[num].hoffset = (buff[start + 9] |
		(buff[start + 10] & 0x7f) << 8) + 1;
	edid_info->t7vtdb[num].t7hsp = (buff[start + 10] & 0x80) >> 7;
	edid_info->t7vtdb[num].hsync_width = (buff[start + 11] |
		buff[start + 12] << 8) + 1;
	edid_info->t7vtdb[num].vactive = (buff[start + 13] |
		buff[start + 14] << 8) + 1;
	edid_info->t7vtdb[num].vblank = (buff[start + 15] |
		buff[start + 16]) + 1;
	edid_info->t7vtdb[num].voffset = (buff[start + 17] |
		(buff[start + 18] & 0x7f) << 8) + 1;
	edid_info->t7vtdb[num].t7vsp = (buff[start + 18] & 0x80) >> 7;
	edid_info->t7vtdb[num].vsync_width = (buff[start + 19] |
		buff[start + 20] << 8) + 1;
	htotal = edid_info->t7vtdb[num].hblank + edid_info->t7vtdb[num].hactive;
	vtotal = edid_info->t7vtdb[num].vactive + edid_info->t7vtdb[num].vblank;
	edid_info->t7vtdb[num].fresh_rate = edid_info->t7vtdb[num].pixel_clk * 1000
		/ htotal / vtotal;
	edid_info->t7vtdbdb_num++;
}

static void get_edid_t8vtdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0, offset = 0, num = 0;

	if (!buff || !edid_info)
		return;
	if (len < 2 - 1) {
		rx_pr("invalid t8vtdb length: %d!\n", len);
		return;
	}
	edid_info->contain_t8vtdb = true;
	num = edid_info->t8vtdbdb_num;
	edid_info->t8vtdb[num].code_tyoe = (buff[start] & 0xc0) >> 6;
	edid_info->t8vtdb[num].t8y420 = (buff[start] & 0x20) >> 5;
	edid_info->t8vtdb[num].tcs = (buff[start] & 0x8) >> 3;
	edid_info->t8vtdb[num].revision = buff[start] & 0x7;
	offset = edid_info->t8vtdb[num].tcs == 1 ? 2 : 1;
	edid_info->t8vtdb[num].timing_num = (len - 1) / offset;
	++start;
	if (offset == 1) {
		for (i = 0; i < edid_info->t8vtdb[num].timing_num; ++i)
			edid_info->t8vtdb[num].dmt_timing[i] = buff[start + i];
	} else {
		for (i = 0; i < edid_info->t8vtdb[num].timing_num; ++i) {
			edid_info->t8vtdb[num].dmt_timing[i] = (buff[start + 1] << 8) | buff[start];
			start += 2;
		}
	}
	edid_info->t8vtdbdb_num++;
}

static void get_edid_t10vtdb(unsigned char *buff,
			  unsigned char start,
			  unsigned char len,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0, length = 1, num = 0;

	if (!buff || !edid_info)
		return;
	if (len < 2) {
		rx_pr("invalid t10vtdb\n");
		return;
	}
	edid_info->contain_t10vtdb = true;
	num = edid_info->t10vtdbdb_num;
	edid_info->t10vtdb[num].t10_m = (buff[start] & 0x70) >> 4;
	edid_info->t10vtdb[num].block_revision = buff[start] & 0x7;
	while (length < len) {
		edid_info->t10vtdb[num].t10_desc[i].t10_y420 =
			(buff[start + length] & 0x80) >> 7;
		edid_info->t10vtdb[num].t10_desc[i]._3d_sup =
			(buff[start + length] & 0x60) >> 5;
		edid_info->t10vtdb[num].t10_desc[i].vrhb =
			(buff[start + length] & 0x10) >> 4;
		edid_info->t10vtdb[num].t10_desc[i].timing_formula =
			buff[start + length] & 0x7;
		edid_info->t10vtdb[num].t10_desc[i].hactive =
			buff[start + length + 1] |
			buff[start + length + 2] << 8;
		edid_info->t10vtdb[num].t10_desc[i].vactive =
			buff[start + length + 3] |
			buff[start + length + 4] << 8;
		if (edid_info->t10vtdb[num].t10_m)
			edid_info->t10vtdb[num].t10_desc[i].fresh_rate =
				(buff[start + length + 5] |
				(buff[start + length + 6] & 0x3) << 8) + 1;
		else
			edid_info->t10vtdb[num].t10_desc[i].fresh_rate =
				buff[start + length + 5];
		length += edid_info->t10vtdb[num].t10_m == 1 ? 7 : 6;
		++i;
	}
	edid_info->t10vtdb[num].desc_num = i;
	edid_info->t10vtdbdb_num++;
}

static void get_cta_dtd_data(unsigned char *buff,
			  unsigned char start,
			  struct cta_blk_parse_info *edid_info)
{
	u8 i = 0;

	if (!buff || !edid_info)
		return;
	for (i = 0; i < 15; ++i) {
		start += i * 18;
		if (start > 127 - 18 ||
			(buff[start] == 0 && buff[start + 1] == 0))
			break;
		edid_info->dtd[i].pixel_clk =
			(buff[start] + (buff[start + 1] << 8)) / 100;
		edid_info->dtd[i].hactive =
			buff[start + 2] + (((buff[start + 4] >> 4) & 0xF) << 8);
		edid_info->dtd[i].hblank =
			((buff[start + 4] & 0xF) << 8) + buff[start + 3];
		edid_info->dtd[i].htotal =
			edid_info->dtd[i].hactive + edid_info->dtd[i].hblank;
		edid_info->dtd[i].vactive =
			buff[start + 5] + (((buff[start + 7] >> 4) & 0xF) << 8);
		edid_info->dtd[i].vblank =
			((buff[start + 7] & 0xF) << 8) + buff[start + 6];
		edid_info->dtd[i].vtotal =
			edid_info->dtd[i].vactive + edid_info->dtd[i].vblank;
		edid_info->dtd[i].hfront =
			buff[start + 8] | ((buff[start + 11] & 0xc0) << 8);
		edid_info->dtd[i].hsync_width =
			buff[start + 9] | ((buff[start + 11] & 0x30) << 8);
		edid_info->dtd[i].vfront =
			(buff[start + 10] >> 4) | ((buff[start + 11] & 0xc) << 8);
		edid_info->dtd[i].vsync_width =
			(buff[start + 10] & 0xf) | ((buff[start + 11] & 0x3) << 8);
		edid_info->dtd[i].framerate =
			edid_info->dtd[i].pixel_clk * 1000 /
			edid_info->dtd[i].htotal * 1000 /
			edid_info->dtd[i].vtotal;
	}
	edid_info->dtd_num = i;
}

/* parse multi block data buff */
void parse_cta_data_block(u8 *p_blk_buff,
			  u8 buf_len,
			  struct cta_blk_parse_info *blk_parse_info)
{
	unsigned char tag_data = 0, tag_code = 0, extend_tag_code = 0;
	unsigned char data_blk_len = 0;
	unsigned char index = 0;
	unsigned char i = 0;
	int ret = 0;

	if (!p_blk_buff || !blk_parse_info || buf_len == 0)
		return;

	while (index < buf_len && i < DATA_BLK_MAX_NUM) {
		tag_data = p_blk_buff[index];
		tag_code = (tag_data & 0xE0) >> 5;
		/* data block payload length */
		data_blk_len = tag_data & 0x1F;
		/* length beyond max offset, force to break */
		if ((index + data_blk_len + 1) > buf_len) {
			rx_pr("unexpected data blk len\n");
			break;
		}
		blk_parse_info->db_info[i].cta_blk_index = i;
		blk_parse_info->db_info[i].tag_code = tag_code;
		blk_parse_info->db_info[i].offset = index;
		/* length including header */
		blk_parse_info->db_info[i].blk_len = data_blk_len + 1;
		blk_parse_info->data_blk_num = i + 1;
		switch (tag_code) {
		/* data block payload offset: index+1
		 * length: payload length
		 */
		case VIDEO_TAG:
			get_edid_video_data(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
			break;
		case AUDIO_TAG:
			get_edid_audio_data(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
			break;
		case SPEAKER_TAG:
			get_edid_speaker_data(p_blk_buff,
					      index + 1,
					      data_blk_len,
					      blk_parse_info);
			break;
		case VENDOR_TAG:
			ret = get_edid_vsdb(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
			switch (ret) {
			case VENDOR_TAG:
				get_edid_vs14(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
				break;
			case HF_VENDOR_DB_TAG:
				get_edid_hf_vsdb(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
				blk_parse_info->db_info[i].tag_code =
					HF_VENDOR_DB_TAG;
				break;
			case VSDB_FREESYNC_TAG:
				get_edid_fsdb(p_blk_buff,
					    index + 1,
					    data_blk_len,
					    blk_parse_info);
				blk_parse_info->db_info[i].tag_code =
					VSDB_FREESYNC_TAG;
				break;
			default:
				rx_pr("invalid VSDB\n");
				break;
			}
			break;
		case VDTCDB_TAG:
			break;
		case USE_EXTENDED_TAG:
			extend_tag_code = p_blk_buff[index + 1];
			blk_parse_info->db_info[i].tag_code =
				(USE_EXTENDED_TAG << 8) | extend_tag_code;
			switch (extend_tag_code) {
			/* offset: start after extended tag code
			 * length: payload length except extend tag
			 */
			case VCDB_TAG:
				get_edid_vcdb(p_blk_buff,
					      index + 2,
					      data_blk_len - 1,
					      blk_parse_info);
				break;
			case VSVDB_TAG:
				/* hdr10p and dv video have same tag code */
				if (p_blk_buff[index + 2] == 0x8b) {
					/* todohrd10p*/
					blk_parse_info->db_info[i].tag_code =
						VSVDB_HDR10P_TAG;
					get_edid_hdr10p_data(p_blk_buff,
							 index + 2,
							 data_blk_len - 1,
							 blk_parse_info);
				} else {
					get_edid_dvsv_data(p_blk_buff,
							 index + 2,
							 data_blk_len - 1,
							 blk_parse_info);
				}
				break;
			case CDB_TAG:
				get_edid_colorimetry_data(p_blk_buff,
							  index + 2,
							  data_blk_len - 1,
							  blk_parse_info);
				break;
			case HDR_STATIC_TAG:
				get_edid_static_hdr_data(p_blk_buff,
					     index + 2,
					     data_blk_len - 1,
					     blk_parse_info);
				break;
			case HDR_DYNAMIC_TAG:
				get_edid_dynamic_hdr_data(p_blk_buff,
					     index + 2,
					     data_blk_len - 1,
					     blk_parse_info);
				break;
			case Y420VDB_TAG:
				get_edid_y420_vid_data(p_blk_buff, index + 2,
						       data_blk_len - 1,
						       blk_parse_info);
				break;
			case Y420CMDB_TAG:
				get_edid_y420_cap_map_data(p_blk_buff,
							   index + 2,
							   data_blk_len - 1,
							   blk_parse_info);
				break;
			case VSADB_TAG:
				get_edid_vsadb(p_blk_buff, index + 2,
					       data_blk_len - 1, blk_parse_info);
				break;
			case HDMI_ADB_TAG:
				get_edid_hdmi_aud(p_blk_buff, index + 2,
					       data_blk_len - 1, blk_parse_info);
				break;
			case RCDB_TAG:
				get_edid_rcdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case SLDB_TAG:
				get_edid_sldb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case VFPDB_TAG:
				get_edid_vfpdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case SCDB_TAG:
				/* index + 1 applicable to hf_db*/
				get_edid_hf_scdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case SBTM_TAG:
				get_edid_hf_sbtm(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case IFDB_TAG:
				get_edid_ifdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case T7VTDB_TAG:
				get_edid_t7vtdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case T8VTDB_TAG:
				get_edid_t8vtdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			case T10VTDB_TAG:
				get_edid_t10vtdb(p_blk_buff, index + 2,
					      data_blk_len - 1, blk_parse_info);
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
		/* next tag offset */
		index += (data_blk_len + 1);
		i++;
	}
}

/* parse CEA extension block */
void edid_parse_cea_ext_block(u8 *p_edid,
			      struct cea_ext_parse_info *edid_info)
{
	unsigned int max_offset = 0;
	unsigned int blk_start_offset = 0;
	unsigned char data_blk_total_len = 0;
	unsigned char i = 0;

	if (!p_edid || !edid_info)
		return;
	if (p_edid[0] != 0x02) {
		rx_pr("not a valid CEA block!\n");
		return;
	}
	edid_info->cea_tag = p_edid[0];
	edid_info->cea_revision = p_edid[1];
	max_offset = p_edid[2];
	edid_info->dtd_offset = max_offset;
	edid_info->underscan_sup = (p_edid[3] >> 7) & 0x1;
	edid_info->basic_aud_sup = (p_edid[3] >> 6) & 0x1;
	edid_info->ycc444_sup = (p_edid[3] >> 5) & 0x1;
	edid_info->ycc422_sup = (p_edid[3] >> 4) & 0x1;
	edid_info->native_dtd_num = p_edid[3] & 0xF;

	blk_start_offset = 4;
	data_blk_total_len = max_offset - blk_start_offset;
	parse_cta_data_block(&p_edid[blk_start_offset],
			     data_blk_total_len,
			     &edid_info->blk_parse_info);
	/*dtd*/
	get_cta_dtd_data(p_edid, max_offset, &edid_info->blk_parse_info);
	for (i = 0; i < edid_info->blk_parse_info.data_blk_num; i++)
		edid_info->blk_parse_info.db_info[i].offset += blk_start_offset;

	edid_info->free_size = rx_get_cta_free_size(p_edid, EDID_BLK_SIZE);
	edid_info->total_free_size = rx_edid_total_free_size(p_edid, EDID_BLK_SIZE);
	edid_info->dtd_size = rx_get_cea_dtd_size(p_edid, EDID_BLK_SIZE);
}

u8 rx_edid_ext_blk_num(u8 *pedid)
{
	u32 blk_num = 0, index = 0;

	if (!pedid)
		return 0;
	index = rx_get_eeodb(pedid);
	if (index)
		blk_num = pedid[index + 2];
	else
		blk_num = pedid[126];

	return blk_num;
}

u8 rx_edid_cta_blk_num(u8 *p_edid)
{
	u8 blk_num = 0, i = 0, cta_num = 0;

	if (!p_edid)
		return 0;
	blk_num = rx_edid_ext_blk_num(p_edid);
	for (i = 1; i <= blk_num; ++i) {
		if (p_edid[EDID_BLK_SIZE * i] == 0x2)
			cta_num++;
	}
	return cta_num;
}

bool is_support_frl(u8 *pedid, u8 port)
{
	u32 hf_vsdb_start = 0;
	bool frl_rate = 0, ret = false;
	struct data_block_location_s data_block = {0};

	data_block = rx_get_cea_tag_offset(pedid, HF_VENDOR_DB_TAG);
	hf_vsdb_start = data_block.pos[0];
	frl_rate = pedid[hf_vsdb_start + 7] >> 4;
	if (frl_rate) {
		if (rx_info.chip_id == CHIP_ID_T3X &&
			(port == E_PORT2 || port == E_PORT3)) {
			ret = true;
		} else {
			if (log_level & EDID_LOG)
				rx_pr("chip no support frl, edid error.\n");
			ret = false;
		}
	} else {
		ret = false;
	}
	return ret;
}

bool rx_edid_parse(u8 *p_edid, struct edid_info_s *edid_info)
{
	if (!p_edid || !edid_info)
		return false;
	if (!is_valid_edid_data(p_edid)) {
		rx_pr("can't parse invalid edid\n");
		return false;
	}
	memset(cta_flag, 0, sizeof(cta_flag));
	edid_info->extension_block_cnt = rx_edid_ext_blk_num(p_edid);
	rx_pr("extension_block_cnt = %d\n", edid_info->extension_block_cnt);
	edid_parse_block0(p_edid, edid_info);
	if (edid_info->extension_block_cnt > 0 && p_edid[EDID_EXT_BLK_OFF] == 0x2) {
		edid_parse_cea_ext_block(p_edid + EDID_EXT_BLK_OFF,
			&edid_info->cea_ext_info);
		cta_flag[1] = 1;
	}
	if (edid_info->extension_block_cnt > 1 && p_edid[EDID_EXT_BLK_OFF * 2] == 0x2) {
		edid_parse_cea_ext_block(p_edid + EDID_EXT_BLK_OFF * 2,
			&edid_info->cea_ext_info1);
		cta_flag[2] = 1;
	}
	if (edid_info->extension_block_cnt > 2 && p_edid[EDID_EXT_BLK_OFF * 3] == 0x2) {
		edid_parse_cea_ext_block(p_edid + EDID_EXT_BLK_OFF * 3,
			&edid_info->cea_ext_info2);
		cta_flag[3] = 1;
	}

	return true;
}

static void rx_print_display_param(struct basic_display_param *param)
{
	u8 sync_type = param->sync_type;
	u8 display_power = param->display_power;
	u8 other_feature = param->other_feature;

	rx_pr("\n");
	rx_pr("****Display param****\n");
	/* video input define */
	if (param->video_signal) {
		rx_pr("Digital video input\n");
		if (param->color_bit_depth == 0x7)
			rx_pr("reserved color bit depth\n");
		else if (param->color_bit_depth == 0x0)
			rx_pr("color bit depth is undefined\n");
		else
			rx_pr("%d bits per primary color\n",
				4 + param->color_bit_depth * 2);
		switch (param->digital_support) {
		case 0:
			rx_pr("Digital interface is not defined\n");
			break;
		case 1:
			rx_pr("DVI is supported\n");
			break;
		case 2:
			rx_pr("HDMI-a is supported\n");
			break;
		case 3:
			rx_pr("HDMI-b is supported\n");
			break;
		case 4:
			rx_pr("MDDI is supported\n");
			break;
		case 5:
			rx_pr("DisplayPort is supported\n");
			break;
		default:
			rx_pr("Reserved feature\n");
			break;
		}
	} else {
		rx_pr("Analog video input\n");
		rx_pr("Video : Sync : Total is\n");
		switch (param->signal_level_standard) {
		case 0:
			rx_pr("0.700 : 0.300 : 1.000\n");
			break;
		case 1:
			rx_pr("0.714 : 0.286 : 1.000\n");
			break;
		case 2:
			rx_pr("1.000 : 0.400 : 1.400\n");
			break;
		case 3:
			rx_pr("0.700 : 0.000 : 0.700\n");
			break;
		default:
			break;
		}

		if (param->video_setup)
			rx_pr("Blank Level = Black Level\n");
		else
			rx_pr("Blank-to-Black setup or pedestal\n");

		if (sync_type & 0x4)
			rx_pr("Separate Sync H&V Signals are supported\n");
		else
			rx_pr("Separate Sync H&V Signals are not supported\n");
		if (sync_type & 0x2)
			rx_pr("Composite Sync Signal on Horizontal is supported\n");
		else
			rx_pr("Composite Sync Signal on Horizontal is not supported\n");
		if (sync_type & 0x1)
			rx_pr("Composite Sync Signal on Green Video is supported\n");
		else
			rx_pr("Composite Sync Signal on Green Video is not supported\n");

		if (param->serrations)
			rx_pr("Composite Sync Signal on Vertical Sync is supported\n");
		else
			rx_pr("Composite Sync Signal on Vertical Sync is not supported\n");
	}
	/*  Horizontal and Vertical Screen Size or Aspect Ratio */
	if (param->horizontal_val == 0 && param->vertical_val == 0)
		rx_pr("the screen size or aspect ratio are unknown or undefined\n");
	else if (param->horizontal_val == 0)
		rx_pr("Aspect Ratio(Portrait):%d\n", param->vertical_val);
	else if (param->vertical_val == 0)
		rx_pr("Aspect Ratio(Landscape):%d\n", param->horizontal_val);
	else
		rx_pr("horizontal size:%d cm, vertical size:%d cm\n",
			param->horizontal_val, param->vertical_val);
	/* GAMMA */
	rx_pr("NOTE:stored val = (gamma * 100) - 100\n");
	if (param->gamma == 0xff)
		rx_pr("gamma shall be stored in an extension block\n");
	else
		rx_pr("gamma is %d / 100\n", param->gamma + 100);
	/* support feature */
	if (display_power & 0x4)
		rx_pr("Standby Mode is supported\n");
	else
		rx_pr("Standby Mode is not supported\n");
	if (display_power & 0x2)
		rx_pr("Suspend Mode is supported\n");
	else
		rx_pr("Suspend Mode is not supported\n");
	if (display_power & 0x1)
		rx_pr("Very Low Power is supported\n");
	else
		rx_pr("Very Low Power is not supported\n");

	if (param->video_signal) {
		switch (param->display_color_type) {
		case 0:
			rx_pr("support RGB444.\n");
			break;
		case 1:
			rx_pr("support RGB444 & YUV444.\n");
			break;
		case 2:
			rx_pr("support RGB444 & YUV422.\n");
			break;
		case 3:
			rx_pr("support RGB444 & YUV444 & YUV422.\n");
			break;
		default:
			break;
		}
	} else {
		switch (param->display_color_type) {
		case 0:
			rx_pr("color type:Monochrome or Grayscale display.\n");
			break;
		case 1:
			rx_pr("color type:RGB color display.\n");
			break;
		case 2:
			rx_pr("color type:non-RGB color display.\n");
			break;
		default:
			rx_pr("Display Color Type is Undefined\n");
			break;
		}
	}
	if (other_feature & 0x4)
		rx_pr("sRGB is default color space\n");
	else
		rx_pr("sRGB is not default color space\n");
	if (other_feature & 0x2) {
		rx_pr("Preferred Timing Mode includes the native pixel format");
		rx_pr("and preferred refresh rate of the display device\n");
	} else {
		rx_pr("Preferred Timing Mode not includes the native pixel format");
		rx_pr("and preferred refresh rate of the display device\n");
	}
	if (other_feature & 0x1)
		rx_pr("Defalit GTF is supported\n");
	else
		rx_pr("Defalit GTF is not supported\n");
}

//static double rx_cal_color_characteristic(u16 color)
//{
//	double ret = 0.0;
//	int bit_pos = 10;
//	for (; color != 0; color >>= 1) {
//		if (color & 1)
//			ret += (1 << -bit_pos);
//		--bit_pos;
//	}
//	return ret;
//}

static void rx_print_color_characteristic
	(struct color_characteristic *color_chara)
{
	if (!color_chara)
		return;
	rx_pr("\n");
	rx_pr("****Color characteristic****\n");
	rx_pr("NOTE:from bit9~bit0, val = sum(bitx * 2^(x - 10))\n");
	rx_pr("red_x: %d\n", color_chara->red_x);
	rx_pr("red_y: %d\n", color_chara->red_y);
	rx_pr("green_x: %d\n", color_chara->green_x);
	rx_pr("green_y: %d\n", color_chara->green_y);
	rx_pr("blue_x: %d\n", color_chara->blue_x);
	rx_pr("blue_y: %d\n", color_chara->blue_y);
	rx_pr("white_x: %d\n", color_chara->white_x);
	rx_pr("white_y: %d\n", color_chara->white_y);
}

static void rx_parse_print_dtd(struct detailed_timing_desc *dtd)
{
	if (!dtd)
		return;
	rx_pr("\n");
	rx_pr("****CTA DTD****\n");
	rx_pr("\thactive: %d\n", dtd->hactive);
	rx_pr("\thblank: %d\n", dtd->hblank);
	rx_pr("\thfront: %d\n", dtd->hfront);
	rx_pr("\thsync_width: %d\n", dtd->hsync_width);
	rx_pr("\thtotal: %d\n", dtd->htotal);
	rx_pr("\tvactive: %d\n", dtd->vactive);
	rx_pr("\tvblank: %d\n", dtd->vblank);
	rx_pr("\tvfront: %d\n", dtd->vfront);
	rx_pr("\tvsync_width: %d\n", dtd->vsync_width);
	rx_pr("\tvtotal: %d\n", dtd->vtotal);
	rx_pr("\tpix clk: %dMHz\n", dtd->pixel_clk);
	rx_pr("\tframe rate: %d\n", dtd->framerate);
}

void rx_parse_blk0_print(struct edid_info_s *edid_info)
{
	if (!edid_info)
		return;
	rx_pr("\n");
	rx_pr("****EDID Basic Block****\n");
	rx_pr("manufacturer_name: %s\n", edid_info->manufacturer_name);
	rx_pr("product code: 0x%04x\n", edid_info->product_code);
	rx_pr("serial_number: 0x%08x\n", edid_info->serial_number);
	rx_pr("product week: %d\n", edid_info->product_week);
	rx_pr("product year: %d\n", edid_info->product_year);
	rx_print_display_param(&edid_info->display_param);
	rx_print_color_characteristic(&edid_info->color_chara);
	rx_pr("Descriptor1:\n");
	rx_parse_print_dtd(&edid_info->descriptor1);
	rx_pr("Descriptor2:\n");
	rx_parse_print_dtd(&edid_info->descriptor2);
	rx_pr("monitor name: %s\n", edid_info->monitor_name);
	rx_pr("max support pixel clock: %dMhz\n",
	      edid_info->max_sup_pixel_clk);
	rx_pr("extension_flag: %d\n", edid_info->extension_flag);
	rx_pr("block0_chk_sum: 0x%x\n", edid_info->block0_chk_sum);
}

void rx_parse_print_vdb(struct video_db_s *video_db)
{
	unsigned char i = 0;
	unsigned char hdmi_vic = 0;

	if (!video_db)
		return;
	rx_pr("\n");
	rx_pr("****Video Data Block****\n");
	rx_pr("support SVD list:\n");
	for (i = 0; i < video_db->svd_num; i++) {
		hdmi_vic = video_db->hdmi_vic[i];
		rx_edid_print_vic_fmt(i, hdmi_vic);
	}
}

void rx_parse_print_adb(struct audio_db_s *audio_db)
{
	enum edid_audio_format_e fmt = 0;
	union bit_rate_u *bit_rate = 0;

	if (!audio_db)
		return;
	rx_pr("\n");
	rx_pr("****Audio Data Block****\n");
	for (fmt = AUDIO_FORMAT_LPCM; fmt <= AUDIO_FORMAT_WMAPRO; fmt++) {
		if (audio_db->aud_fmt_sup[fmt]) {
			rx_pr("audio fmt: %s\n", aud_fmt[fmt]);
			rx_pr("\tmax channel: %d\n",
			      audio_db->sad[fmt].max_channel + 1);
			if (audio_db->sad[fmt].usr.ssr.freq_192khz)
				rx_pr("\tfreq_192khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_176_4khz)
				rx_pr("\tfreq_176.4khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_96khz)
				rx_pr("\tfreq_96khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_88_2khz)
				rx_pr("\tfreq_88.2khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_48khz)
				rx_pr("\tfreq_48khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_44_1khz)
				rx_pr("\tfreq_44.1khz\n");
			if (audio_db->sad[fmt].usr.ssr.freq_32khz)
				rx_pr("\tfreq_32khz\n");
			bit_rate = &audio_db->sad[fmt].bit_rate;
			if (fmt == AUDIO_FORMAT_LPCM) {
				rx_pr("sample size:\n");
				if (bit_rate->pcm.size_16bit)
					rx_pr("\t16bit\n");
				if (bit_rate->pcm.size_20bit)
					rx_pr("\t20bit\n");
				if (bit_rate->pcm.size_24bit)
					rx_pr("\t24bit\n");
			} else if ((fmt >= AUDIO_FORMAT_AC3) &&
				(fmt <= AUDIO_FORMAT_ATRAC)) {
				rx_pr("\tmax bit rate: %dkHz\n",
				      bit_rate->others * 8);
			} else if (fmt == AUDIO_FORMAT_DDP) {
				if (bit_rate->others & 0x1)
					rx_pr("\tSupport Dolby Atmos decoding\n");
				else if (bit_rate->others & 0x2)
					rx_pr("\tSupport Dolby Atmos acmod28\n");
			} else if (fmt == AUDIO_FORMAT_MAT) {
				if (bit_rate->others & 0x1)
					rx_pr("\tsupport Dolby trueHD\n");
				else
					rx_pr("\tsupport Dolby trueHD, object audio PCM\n");
				if (bit_rate->others & 0x2)
					rx_pr("\trequired by the Dolby MAT 2.x decoder\n");
				else
					rx_pr("\tnot required by the Dolby MAT 2.x decoder\n");
			} else {
				rx_pr("\tformat dependent value: 0x%x\n",
				      bit_rate->others);
			}
		}
	}
}

static void rx_parse_print_sadb(struct speaker_alloc_db_s *spk_alloc)
{
	if (!spk_alloc)
		return;
	rx_pr("\n");
	rx_pr("****Speaker Allocation Data Block****\n");
	if (spk_alloc->flw_frw)
		rx_pr("\tFLW/FRW\n");
	if (spk_alloc->flc_frc)
		rx_pr("\tFLC/FRC\n");
	if (spk_alloc->bc)
		rx_pr("\tBC\n");
	if (spk_alloc->bl_br)
		rx_pr("\tBL/BR\n");
	if (spk_alloc->fc)
		rx_pr("\tFC\n");
	if (spk_alloc->lfe1)
		rx_pr("\tLFE1\n");
	if (spk_alloc->fl_fr)
		rx_pr("\tFL/FR\n");

	if (spk_alloc->tpsil_tpsir)
		rx_pr("\tTpSiL/TpSiR\n");
	if (spk_alloc->sil_sir)
		rx_pr("\tSiL/SiR\n");
	if (spk_alloc->tpbc)
		rx_pr("\tTpBC\n");
	if (spk_alloc->lfe2)
		rx_pr("\tLFE2\n");
	if (spk_alloc->ls_rs)
		rx_pr("\tLS/RS\n");
	if (spk_alloc->tpfc)
		rx_pr("\tTpFC\n");
	if (spk_alloc->tpc)
		rx_pr("\tTpC\n");
	if (spk_alloc->tpfl_tpfr)
		rx_pr("\tTpFL/TpFR\n");

	if (spk_alloc->tpbl_tpbr)
		rx_pr("\tTpBL/TpBR\n");
	if (spk_alloc->btfc)
		rx_pr("\tBtFC\n");
	if (spk_alloc->btfl_btfr)
		rx_pr("\tBtFL/BtFR\n");
}

/* may need extend spker alloc */
void rx_parse_print_spk_alloc(struct speaker_alloc_db_s *spk_alloc)
{
	if (!spk_alloc)
		return;
	rx_parse_print_sadb(spk_alloc);
}

void rx_parse_print_hdmi_aud(struct hdmi_adb_s *hdmi_adb)
{
	u8 i = 0;

	if (!hdmi_adb)
		return;
	rx_pr("\n");
	rx_pr("****HDMI Audio (Multi-Stream/3D)****\n");
	rx_pr("\tSupports MS NonMixed: %d\n", hdmi_adb->sup_ms_nonmixed);
	switch (hdmi_adb->max_stream_cnt) {
	case 0:
		rx_pr("\tSink does not support Multi-Stream Audio\n");
		break;
	case 1:
		rx_pr("\tSink supports Multi-Stream Audio with 2 audio streams\n");
		break;
	case 2:
		rx_pr("\t Sink supports Multi-Stream Audio with 2 or 3 audio streams\n");
		break;
	case 3:
		rx_pr("\t Sink supports Multi-Stream Audio with 2, 3, or 4 audio streams\n");
		break;
	default:
		break;
	}
	for (i = 0; i < hdmi_adb->num_3d_ad; ++i) {
		rx_pr("\tDescriptor %d:\n", i);
		if (hdmi_adb->hdmi_3d_ad[i].format == 1) {
			rx_pr("\tFormat Code: L-PCM Audio\n");
			rx_pr("\tbit rate:\n");
			if (hdmi_adb->hdmi_3d_ad[i].bit24)
				rx_pr("\t24 bit\n");
			if (hdmi_adb->hdmi_3d_ad[i].bit20)
				rx_pr("\t20 bit\n");
			if (hdmi_adb->hdmi_3d_ad[i].bit16)
				rx_pr("\t16 bit\n");
		} else if (hdmi_adb->hdmi_3d_ad[i].format == 9) {
			rx_pr("\tFormat Code: One Bit Audio\n");
			rx_pr("\tFormat Code Dependent Value: %d\n",
				hdmi_adb->hdmi_3d_ad[i].format_value);
		}
		rx_pr("\tMax Number of Channels: %d\n",
			hdmi_adb->hdmi_3d_ad[i].max_channel_num + 1);
		if (hdmi_adb->hdmi_3d_ad[i].freq_192khz)
			rx_pr("\tSupport 192kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_176_4khz)
			rx_pr("\tSupport 176.4kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_96khz)
			rx_pr("\tSupport 96kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_88_2khz)
			rx_pr("\tSupport 88.2kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_48khz)
			rx_pr("\tSupport 48kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_44_1khz)
			rx_pr("\tSupport 44.1kHz\n");
		if (hdmi_adb->hdmi_3d_ad[i].freq_32khz)
			rx_pr("\tSupport 32kHz\n");
		rx_pr("\n");
	}
	switch (hdmi_adb->acat) {
	case 0:
		rx_pr("\tAudio Channel Allocation Type: Not support\n");
		break;
	case 1:
		rx_pr("\tAudio Channel Allocation Type: 10.2 channels\n");
		break;
	case 2:
		rx_pr("\tAudio Channel Allocation Type: 22.2 channels\n");
		break;
	case 3:
		rx_pr("\tAudio Channel Allocation Type: 30.2 channels\n");
		break;
	default:
		rx_pr("\tAudio Channel Allocation Type: Reserved\n");
		break;
	}
	if (hdmi_adb->num_3d_ad > 0)
		rx_parse_print_sadb(&hdmi_adb->sadb);
}

void rx_parse_print_vsdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i = 0;
	unsigned char hdmi_vic = 0;
	unsigned char svd_num = 0;
	unsigned char _2d_vic_order = 0;
	unsigned char _3d_struct = 0;
	unsigned char _3d_detail = 0;

	if (!edid_info)
		return;

	svd_num = edid_info->video_db[0].svd_num;
	rx_pr("\n");
	rx_pr("****Vender Specific Data Block****\n");
	rx_pr("phy addr: %d.%d.%d.%d\n",
	      edid_info->vsdb.a, edid_info->vsdb.b,
	      edid_info->vsdb.c, edid_info->vsdb.d);
	if (edid_info->vsdb.support_ai)
		rx_pr("support AI\n");
	rx_pr("support deep clor:\n");
	if (edid_info->vsdb.dc_48bit)
		rx_pr("\t16bit\n");
	if (edid_info->vsdb.dc_36bit)
		rx_pr("\t12bit\n");
	if (edid_info->vsdb.dc_30bit)
		rx_pr("\t10bit\n");
	if (edid_info->vsdb.dvi_dual)
		rx_pr("support dvi dual channel\n");
	if (edid_info->vsdb.max_tmds_clk > 0)
		rx_pr("max tmds clk supported: %dMHz\n",
		      edid_info->vsdb.max_tmds_clk * 5);
	rx_pr("hdmi_video_present: %d\n",
	      edid_info->vsdb.hdmi_video_present);
	rx_pr("Content types:\n");
	if (edid_info->vsdb.cnc3)
		rx_pr("\tcnc3: Game\n");
	if (edid_info->vsdb.cnc2)
		rx_pr("\tcnc2: Cinema\n");
	if (edid_info->vsdb.cnc1)
		rx_pr("\tcnc1: Photo\n");
	if (edid_info->vsdb.cnc0)
		rx_pr("\tcnc0: Graphics(text)\n");
	if (edid_info->vsdb.hdmi_vic_len > 0)
		rx_pr("supported 4k2k format:\n");
	if (edid_info->vsdb.hdmi_4k2k_30hz_sup)
		rx_pr("\thdmi vic1: 4k30hz\n");
	if (edid_info->vsdb.hdmi_4k2k_25hz_sup)
		rx_pr("\thdmi vic2: 4k25hz\n");
	if (edid_info->vsdb.hdmi_4k2k_24hz_sup)
		rx_pr("\thdmi vic3: 4k24hz\n");
	if (edid_info->vsdb.hdmi_smpte_sup)
		rx_pr("\thdmi vic4: smpte\n");
	/* Mandatory 3D format: HDMI1.4 spec page157 */
	if (edid_info->vsdb._3d_present) {
		rx_pr("Basic(Mandatory) 3D formats supported\n");
		rx_pr("Image Size:\n");
		switch (edid_info->vsdb.image_size) {
		case 0:
			rx_pr("\tNo additional information\n");
			break;
		case 1:
			rx_pr("\tOnly indicate correct aspect ratio\n");
			break;
		case 2:
			rx_pr("\tCorrect size: Accurate to 1(cm)\n");
			break;
		case 3:
			rx_pr("\tCorrect size: multiply by 5(cm)\n");
			break;
		default:
			break;
		}
	} else {
		rx_pr("No 3D support\n");
	}
	if (edid_info->vsdb._3d_multi_present == 1) {
		/* For each bit in _3d_struct which is set (=1),
		 * Sink supports the corresponding 3D_Structure
		 * for all of the VICs listed in the first 16
		 * entries in the EDID.
		 */
		rx_pr("General 3D format, on the first 16 SVDs:\n");
		for (i = 0; i < 16; i++) {
			if ((edid_info->vsdb._3d_struct_all >> i) & 0x1)
				rx_pr("\t%s\n",	_3d_structure[i]);
		}
	} else if (edid_info->vsdb._3d_multi_present == 2) {
		/* Where a bit is set (=1), for the corresponding
		 * VIC within the first 16 entries in the EDID,
		 * the Sink indicates 3D support as designate
		 * by the 3D_Structure_ALL_15.0 field.
		 */
		rx_pr("General 3D format, on the SVDs below:\n");
		for (i = 0; i < 16; i++) {
			if ((edid_info->vsdb._3d_struct_all >> i) & 0x1)
				rx_pr("\t%s\n",	_3d_structure[i]);
		}
		rx_pr("For SVDs:\n");
		for (i = 0; (i < svd_num) && (i < 16); i++) {
			hdmi_vic = edid_info->video_db[0].hdmi_vic[i];
			if ((edid_info->vsdb._3d_mask_15_0 >> i) & 0x1)
				rx_edid_print_vic_fmt(i, hdmi_vic);
		}
	}

	if (edid_info->vsdb._2d_vic_num > 0)
		rx_pr("Specific VIC 3D information:\n");
	for (i = 0; (i < edid_info->vsdb._2d_vic_num) &&
	     (i < svd_num) && (i < 16); i++) {
		_2d_vic_order =
			edid_info->vsdb._2d_vic[i]._2d_vic_order;
		_3d_struct =
			edid_info->vsdb._2d_vic[i]._3d_struct;
		hdmi_vic =
			edid_info->video_db[0].hdmi_vic[_2d_vic_order];
		rx_edid_print_vic_fmt(_2d_vic_order, hdmi_vic);
		rx_pr("\t\t3d format: %s\n", _3d_structure[_3d_struct]);
		if (_3d_struct >= 0x8 && _3d_struct <= 0xF) {
			_3d_detail =
				edid_info->vsdb._2d_vic[i]._3d_detail;
			rx_pr("\t\t3d_detail: %s\n",
			      _3d_detail_x[_3d_detail]);
		}
	}
}

static void rx_parse_print_hf(struct hf_s *hf_db)
{
	rx_pr("hf-db version: %d\n",
	      hf_db->version);
	rx_pr("max_tmds_rate: %dMHz\n",
	      hf_db->max_tmds_rate * 5);
	rx_pr("scdc_present: %d\n",
	      hf_db->scdc_present);
	rx_pr("rr_cap: %d\n",
	      hf_db->rr_cap);
	rx_pr("cable_status: %d\n",
	      hf_db->cable_status);
	rx_pr("ccbpci: %d\n",
	      hf_db->ccbpci);
	rx_pr("lte_340m_scramble: %d\n",
	      hf_db->lte_340m_scramble);
	rx_pr("independ_view: %d\n",
	      hf_db->independ_view);
	rx_pr("dual_view: %d\n",
	      hf_db->dual_view);
	rx_pr("_3d_osd_disparity: %d\n",
	      hf_db->_3d_osd_disparity);
	//pb4
	switch (hf_db->max_frl_rate) {
	case 0:
		rx_pr("not support FRL\n");
		break;
	case 1:
		rx_pr("max_frl_rate: 3G3L\n");
		break;
	case 2:
		rx_pr("max_frl_rate: 6G3L\n");
		break;
	case 3:
		rx_pr("max_frl_rate: 6G4L\n");
		break;
	case 4:
		rx_pr("max_frl_rate: 8G4L\n");
		break;
	case 5:
		rx_pr("max_frl_rate: 10G4L\n");
		break;
	case 6:
		rx_pr("max_frl_rate: 12G4L\n");
		break;
	default:
		rx_pr("max_frl_rate: reserved\n");
		break;
	}
	rx_pr("uhd_vic: %d\n",
	      hf_db->uhd_vic);
	rx_pr("48bit 420 endode: %d\n",
	      hf_db->dc_48bit_420);
	rx_pr("36bit 420 endode: %d\n",
	      hf_db->dc_36bit_420);
	rx_pr("30bit 420 endode: %d\n",
	      hf_db->dc_30bit_420);
	//pb5
	rx_pr("fapa_end_extended: %d\n",
	      hf_db->fapa_end_extended);
	rx_pr("qms: %d\n",
	      hf_db->qms);
	rx_pr("m_delta: %d\n",
	      hf_db->m_delta);
	rx_pr("cinema_vrr: %d\n",
	      hf_db->cinema_vrr);
	rx_pr("neg_mvrr: %d\n",
	      hf_db->neg_mvrr);
	rx_pr("fva: %d\n",
	      hf_db->fva);
	rx_pr("allm: %d\n",
	      hf_db->allm);
	rx_pr("fapa_start_location: %d\n",
	      hf_db->fapa_start_location);
	//pb6
	rx_pr("vrr_min: %d, 0 means not support Gaming-VRR\n",
	      hf_db->vrr_min);
	//pb7
	rx_pr("vrr_max: %d, 0 represents BRR\n",
	      (hf_db->vrr_max_hi << 8) + hf_db->vrr_max_lo);
	//pb8
	rx_pr("dsc_1p2: %d\n",
	      hf_db->dsc_1p2);
	rx_pr("dsc_native_420: %d\n",
	      hf_db->dsc_native_420);
	rx_pr("qms_tfr_max: %d\n",
	      hf_db->qms_tfr_max);
	rx_pr("qms_tfr_min: %d\n",
	      hf_db->qms_tfr_min);
	rx_pr("dsc_all_bpp: %d\n",
	      hf_db->dsc_all_bpp);
	rx_pr("dsc_16bpc: %d\n",
	      hf_db->dsc_16bpc);
	rx_pr("dsc_12bpc: %d\n",
	      hf_db->dsc_12bpc);
	rx_pr("dsc_10bpc: %d\n",
	      hf_db->dsc_10bpc);
	//pb9
	switch (hf_db->dsc_max_frl_rate) {
	case 0:
		rx_pr("not support dsc_max_frl_rate\n");
		break;
	case 1:
		rx_pr("dsc_max_frl_rate: 3G\n");
		break;
	case 2:
		rx_pr("dsc_max_frl_rate: 6G3L\n");
		break;
	case 3:
		rx_pr("dsc_max_frl_rate: 6G4L\n");
		break;
	case 4:
		rx_pr("dsc_max_frl_rate: 8G\n");
		break;
	case 5:
		rx_pr("dsc_max_frl_rate: 10G\n");
		break;
	case 6:
		rx_pr("dsc_max_frl_rate: 12G\n");
		break;
	default:
		rx_pr("dsc_max_frl_rate: reserved\n");
		break;
	}
	switch (hf_db->dsc_max_slices) {
	case 0:
		rx_pr("not support dsc_max_slices\n");
		break;
	case 1:
		rx_pr("dsc_max_slices: 1 slice and 340MHz/KSliceAdjust\n");
		break;
	case 2:
		rx_pr("dsc_max_slices: 2 slice and 340MHz/KSliceAdjust\n");
		break;
	case 3:
		rx_pr("dsc_max_slices: 4 slice and 340MHz/KSliceAdjust\n");
		break;
	case 4:
		rx_pr("dsc_max_slices: 8 slice and 340MHz/KSliceAdjust\n");
		break;
	case 5:
		rx_pr("dsc_max_slices: 8 slice and 400MHz/KSliceAdjust\n");
		break;
	case 6:
		rx_pr("dsc_max_slices: 12 slice and 400MHz/KSliceAdjust\n");
		break;
	case 7: //refer to hdmi2.1a, this is 16 slices and 400MHz/KSliceAdjust
		rx_pr("dsc_max_slices: 12 slice and 600MHz/KSliceAdjust\n");
		break;
	}
	rx_pr("dsc_max_slices: %d\n", hf_db->dsc_max_slices);
	//pb10
	rx_pr("dsc_total_chunk_kbytes: %d\n",
	      1024 * (hf_db->dsc_total_chunk_kbytes + 1));
}

void rx_parse_print_hf_vsdb(struct hf_vsdb_s *hf_vsdb)
{
	if (!hf_vsdb)
		return;
	rx_pr("\n");
	rx_pr("****HF-VSDB****\n");
	rx_pr("IEEE OUI: %06X%X%X\n",
	      hf_vsdb->ieee_first,
	      hf_vsdb->ieee_second,
	      hf_vsdb->ieee_third);
	rx_parse_print_hf(&hf_vsdb->hf_db);
}

void rx_parse_print_hf_scdb(struct hf_scdb_s *hf_scdb)
{
	if (!hf_scdb)
		return;
	rx_pr("\n");
	rx_pr("****HF-SCDB****\n");
	rx_parse_print_hf(&hf_scdb->hf_db);
}

void rx_parse_print_hf_sbtm(struct hf_sbtm_s *hf_sbtm)
{
	if (!hf_sbtm)
		return;
	rx_pr("\n");
	rx_pr("****HF-SBTM****\n");
	rx_pr("drdm_ind:%d\n", hf_sbtm->drdm_ind);
	rx_pr("grdm_support:\n");
	switch (hf_sbtm->grdm_support) {
	case 0:
		rx_pr("\tNot Supported\n");
		break;
	case 1:
		rx_pr("\tSDR-range General RDM format\n");
		break;
	case 2:
		rx_pr("\tHDR General RDM format\n");
		break;
	default:
		rx_pr("\tReserved\n");
		break;
	}
	rx_pr("max_sbtm_ver:%d\n", hf_sbtm->max_sbtm_ver);
	if (!hf_sbtm->drdm_ind)
		return;
	rx_pr("max_rgb:%d\n", hf_sbtm->max_rgb);
	rx_pr("use_hgig_drdm:%d\n", hf_sbtm->use_hgig_drdm);
	rx_pr("gamut:");
	switch (hf_sbtm->gamut) {
	case 0:
		rx_pr("\tIN EDID\n");
		break;
	case 1:
		rx_pr("\tREC_ITU_R_BT_709\n");
		break;
	case 2:
		rx_pr("\tSMPTE_ST_2113\n");
		break;
	case 3:
		rx_pr("\tREC_ITU_R_BT_2020\n");
		break;
	default:
		break;
	}
	rx_pr("hgig_cat_drdm_sel:");
	switch (hf_sbtm->hgig_cat_drdm_sel) {
	case 0:
		rx_pr("\tNot Used\n");
		break;
	case 1:
		rx_pr("\tnumPB = 0, PBpct[0] = 100, PBnits[0] = 600 cd/m^2,");
		rx_pr("MinNits = 0.1 cd/m^2\n");
		break;
	case 2:
		rx_pr("\tnumPB = 1, PBpct[0] = 10, PBnits[0] = 1000 cd/m^2,");
		rx_pr("PBpct[1] = 100, PBnits[1] = 600 cd/m^2, MinNits = 0.1 cd/m^2\n");
		break;
	case 3:
		rx_pr("\tnumPB = 1, PBpct[0] = 10, PBnits[0] = 4000 cd/m^2,");
		rx_pr("PBpct[1] = 100, PBnits[1] = 600 cd/m^2, MinNits = 0.1 cd/m^2\n");
		break;
	case 4:
		rx_pr("\tnumPB = 1, PBpct[0] = 10, PBnits[0] = 10000 cd/m^2,");
		rx_pr("PBpct[1] = 100, PBnits[1] = 600 cd/m^2, MinNits = 0 cd/m^2\n");
		break;
	default:
		break;
	}
	if (!hf_sbtm->gamut) {
		rx_pr("red_x:%d\n", (hf_sbtm->red_x_high << 8) + hf_sbtm->red_x_low);
		rx_pr("red_y:%d\n", (hf_sbtm->red_y_high << 8) + hf_sbtm->red_y_low);
		rx_pr("green_x:%d\n", (hf_sbtm->green_x_high << 8) + hf_sbtm->green_x_low);
		rx_pr("green_y:%d\n", (hf_sbtm->green_y_high << 8) + hf_sbtm->green_y_low);
		rx_pr("blue_x:%d\n", (hf_sbtm->blue_x_high << 8) + hf_sbtm->blue_x_low);
		rx_pr("blue_y:%d\n", (hf_sbtm->blue_y_high << 8) + hf_sbtm->blue_y_low);
		rx_pr("white_x:%d\n", (hf_sbtm->white_x_high << 8) + hf_sbtm->white_x_low);
		rx_pr("white_y:%d\n", (hf_sbtm->white_y_high << 8) + hf_sbtm->white_y_low);
	}
	if (hf_sbtm->use_hgig_drdm)
		return;
	rx_pr("min_bright_10:%d\n", hf_sbtm->min_bright_10);
	rx_pr("peak_bright_100:%d\n", hf_sbtm->peak_bright_100);
	rx_pr("p0_mant:%d\n", hf_sbtm->p0_mant);
	rx_pr("p0_exp:%d\n", hf_sbtm->p0_exp);
	rx_pr("peak_bright_p0:%d\n", hf_sbtm->peak_bright_p0);
	rx_pr("p1_mant:%d\n", hf_sbtm->p1_mant);
	rx_pr("p1_exp:%d\n", hf_sbtm->p1_exp);
	rx_pr("peak_bright_p1:%d\n", hf_sbtm->peak_bright_p1);
	rx_pr("p2_mant:%d\n", hf_sbtm->p2_mant);
	rx_pr("p2_exp:%d\n", hf_sbtm->p2_exp);
	rx_pr("peak_bright_p2:%d\n", hf_sbtm->peak_bright_p2);
	rx_pr("p3_mant:%d\n", hf_sbtm->p3_mant);
	rx_pr("p3_exp:%d\n", hf_sbtm->p3_exp);
	rx_pr("peak_bright_p3:%d\n", hf_sbtm->peak_bright_p3);
}

void rx_parse_print_sldb(struct sldb_s *sldb)
{
	u8 i = 0;

	if (!sldb)
		return;
	rx_pr("\n");
	rx_pr("****Speaker Location Data Block****\n");
	rx_pr("SLDB has %d descriptop:\n", sldb->len);
	for (i = 0; i < sldb->len; ++i) {
		rx_pr("Descriptor %d\n", i);
		rx_pr("\tcoord:%d\n", sldb->sldb_des[i].coord);
		rx_pr("\tactive:%d\n", sldb->sldb_des[i].active);
		rx_pr("\tchannel id:%d\n",
			sldb->sldb_des[i].channel_idx);
		rx_pr("\tspeaker id:%d, %s\n",
			sldb->sldb_des[i].speaker_id,
			speaker_place[sldb->sldb_des[i].speaker_id]);
		if (sldb->sldb_des[i].coord) {
			rx_pr("\tcoord x:%d\n", sldb->sldb_des[i].x);
			rx_pr("\tcoord y:%d\n", sldb->sldb_des[i].y);
			rx_pr("\tcoord z:%d\n", sldb->sldb_des[i].z);
		}
	}
}

void rx_parse_print_vfpdb(struct vfpdb_s *vfpdb)
{
	u8 i = 0, svr = 0;

	if (!vfpdb)
		return;
	rx_pr("\n");
	rx_pr("****Video Format Preference Data Block****\n");
	rx_pr("VFPDB has %d svr\n", vfpdb->num);
	for (i = 0; i < vfpdb->num; ++i) {
		svr = vfpdb->svr[i];
		if (svr == 0 || svr == 128 || svr == 255)
			continue;
		else if (svr >= 1 && svr <= 127)
			rx_edid_print_vic_fmt(i, svr);
		else if (svr >= 129 && svr <= 144)
			rx_pr("\tDTD%d\n", svr - 128);
		else if (svr >= 145 && svr <= 160)
			rx_pr("\tth Type VII or Type X Video Timing Data Block\n");
		else if (svr >= 161 && svr <= 192)
			continue;
		else if (svr >= 193 && svr <= 253)
			rx_edid_print_vic_fmt(i, svr);
		else if (svr == 254)
			rx_pr("\tDisplayID Type VIII Video Timing Data Block\n");
	}
}

void rx_parse_print_rcdb(struct rcdb_s *rcdb)
{
	if (!rcdb)
		return;
	rx_pr("\n");
	rx_pr("****Room Configuration Data Block****\n");
	rx_pr("display:%d\n", rcdb->display);
	rx_pr("speaker:%d\n", rcdb->speaker);
	if (rcdb->speaker)
		rx_pr("speaker_cnt:%d\n", rcdb->speaker_cnt + 1);
	else
		rx_pr("speaker_cnt:0\n");
	rx_pr("sld:%d\n", rcdb->sld);
	rx_pr("SPM:\n");
	rx_pr("\tflw_frw:%d\n", rcdb->sadb.flw_frw);
	rx_pr("\tflc_frc:%d\n", rcdb->sadb.flc_frc);
	rx_pr("\tbc:%d\n", rcdb->sadb.bc);
	rx_pr("\tbl_br:%d\n", rcdb->sadb.bl_br);
	rx_pr("\tfc:%d\n", rcdb->sadb.fc);
	rx_pr("\tlfe1:%d\n", rcdb->sadb.lfe1);
	rx_pr("\tfl_fr:%d\n", rcdb->sadb.fl_fr);
	rx_pr("\ttpsil_tpsir:%d\n", rcdb->sadb.tpsil_tpsir);
	rx_pr("\tsil_sir:%d\n", rcdb->sadb.sil_sir);
	rx_pr("\ttpbc:%d\n", rcdb->sadb.tpbc);
	rx_pr("\tlfe2:%d\n", rcdb->sadb.lfe2);
	rx_pr("\tls_rs:%d\n", rcdb->sadb.ls_rs);
	rx_pr("\ttpfc:%d\n", rcdb->sadb.tpfc);
	rx_pr("\ttpc:%d\n", rcdb->sadb.tpc);
	rx_pr("\ttpfl_tpfr:%d\n", rcdb->sadb.tpfl_tpfr);
	rx_pr("\tbtfl_btfr:%d\n", rcdb->sadb.btfl_btfr);
	rx_pr("\tbtfc:%d\n", rcdb->sadb.btfc);
	rx_pr("\ttpbl_tpbr:%d\n", rcdb->sadb.tpbl_tpbr);
	if (rcdb->sld) {
		rx_pr("x_max: %d dm\n", rcdb->x_max);
		rx_pr("y_max: %d dm\n", rcdb->y_max);
		rx_pr("z_max: %d dm\n", rcdb->z_max);
	} else {
		rx_pr("x_max: 32 dm\n");
		rx_pr("y_max: 32 dm\n");
		rx_pr("z_max: 16 dm\n");
	}
	if (rcdb->display) {
		rx_pr("bit7 is sign bit; bit6 is integer portion; bit5~0 is fractional portion\n");
		rx_pr("display_x: 0x%x dm\n", rcdb->display_x);
		rx_pr("display_y: 0x%x dm\n", rcdb->display_y);
		rx_pr("display_z: 0x%x dm\n", rcdb->display_z);
	} else {
		rx_pr("bit7 is sign bit; bit6 is integer portion; bit5~0 is fractional portion\n");
		rx_pr("display_x: 0x00 dm\n");
		rx_pr("display_y: 0x40 dm\n");
		rx_pr("display_z: 0x00 dm\n");
	}
}

void rx_parse_print_vcdb(struct video_cap_db_s *vcdb)
{
	if (!vcdb)
		return;
	rx_pr("\n");
	rx_pr("****Video Cap Data Block****\n");
	rx_pr("YCC Quant Range:\n");
	if (vcdb->quanti_range_ycc)
		rx_pr("\tSelectable(via AVI YQ)\n");
	else
		rx_pr("\tNo Data\n");

	rx_pr("RGB Quant Range:\n");
	if (vcdb->quanti_range_rgb)
		rx_pr("\tSelectable(via AVI Q)\n");
	else
		rx_pr("\tNo Data\n");

	rx_pr("PT Scan behavior:\n");
	switch (vcdb->s_PT) {
	case 0:
		rx_pr("\t transfer to CE/IT fields\n");
		break;
	case 1:
		rx_pr("\t Always Overscanned\n");
		break;
	case 2:
		rx_pr("\t Always Underscanned\n");
		break;
	case 3:
		rx_pr("\t Support both over and underscan\n");
		break;
	default:
		break;
	}
	rx_pr("IT Scan behavior:\n");
	switch (vcdb->s_IT) {
	case 0:
		rx_pr("\tIT video format not support\n");
		break;
	case 1:
		rx_pr("\tAlways Overscanned\n");
		break;
	case 2:
		rx_pr("\tAlways Underscanned\n");
		break;
	case 3:
		rx_pr("\tSupport both over and underscan\n");
		break;
	default:
		break;
	}
	rx_pr("CE Scan behavior:\n");
	switch (vcdb->s_CE) {
	case 0:
		rx_pr("\tCE video format not support\n");
		break;
	case 1:
		rx_pr("\tAlways Overscanned\n");
		break;
	case 2:
		rx_pr("\tAlways Underscanned\n");
		break;
	case 3:
		rx_pr("\tSupport both over and underscan\n");
		break;
	default:
		break;
	}
}

void rx_parse_print_dvsadb(struct dv_vsadb_s *dv_vsadb)
{
	if (!dv_vsadb)
		return;
	rx_pr("\n");
	rx_pr("****Dolby Vendor Specific Audio****\n");
	if (dv_vsadb->version == 0)
		rx_pr("\tversion: %d\n", dv_vsadb->version);
	else
		rx_pr("\tversion: reserved\n");
	if (dv_vsadb->headphone)
		rx_pr("\tHeadphone playback only\n");
	if (dv_vsadb->center)
		rx_pr("\tCenter speaker zone present\n");
	if (dv_vsadb->surround)
		rx_pr("\tSurround speaker zone present\n");
	if (dv_vsadb->height)
		rx_pr("\tHeight speaker zone present\n");
	if (dv_vsadb->func)
		rx_pr("\tRefer to Dolby MAT short audio descriptors\n");
	else
		rx_pr("\tSupports Dolby MAT PCM 48kHz only, not support Dolby TrueHD\n");
}

void rx_parse_print_dvsvdb(struct dv_vsvdb_s *dv_vsvdb)
{
	if (!dv_vsvdb)
		return;
	rx_pr("\n");
	rx_pr("****Dolby Vendor Specific Video****\n");
	rx_pr("\tIEEE_OUI: %06X\n",
	      dv_vsvdb->ieee_oui);
	rx_pr("\tvsvdb version: %d\n",
	      dv_vsvdb->version);
	rx_pr("\tsup_global_dimming: %d\n",
	      dv_vsvdb->sup_global_dimming);
	rx_pr("\tsup_2160p60hz: %d\n",
	      dv_vsvdb->sup_2160p60hz);
	rx_pr("\tsup_yuv422_12bit: %d\n",
	      dv_vsvdb->sup_yuv422_12bit);
	rx_pr("\tRx: 0x%x\n", dv_vsvdb->Rx);
	rx_pr("\tRy: 0x%x\n", dv_vsvdb->Ry);
	rx_pr("\tGx: 0x%x\n", dv_vsvdb->Gx);
	rx_pr("\tGy: 0x%x\n", dv_vsvdb->Gy);
	rx_pr("\tBx: 0x%x\n", dv_vsvdb->Bx);
	rx_pr("\tBy: 0x%x\n", dv_vsvdb->By);
	if (dv_vsvdb->version == 0) {
		rx_pr("\tWx: 0x%x\n", dv_vsvdb->Wx);
		rx_pr("\tWy: 0x%x\n", dv_vsvdb->Wy);
		rx_pr("\ttarget max pq: 0x%x\n",
		      dv_vsvdb->tmaxPQ);
		rx_pr("\ttarget min pq: 0x%x\n",
		      dv_vsvdb->tminPQ);
		rx_pr("\tdm_major_ver: 0x%x\n",
		      dv_vsvdb->dm_major_ver);
		rx_pr("\tdm_minor_ver: 0x%x\n",
		      dv_vsvdb->dm_minor_ver);
	} else if (dv_vsvdb->version == 1) {
		rx_pr("\tDM_version: 0x%x\n",
		      dv_vsvdb->DM_version);
		if (dv_vsvdb->length == 0xb) {
			rx_pr("\tLow_latency:\n");
			switch (dv_vsvdb->low_latency) {
			case 0:
				rx_pr("\tOnly Standard\n");
				break;
			case 1:
				rx_pr("\tStandard + Low_Latency\n");
				break;
			default:
				rx_pr("\tReserved\n");
				break;
			}
		}
		rx_pr("\ttarget_max_lum: 0x%x\n",
		      dv_vsvdb->target_max_lum);
		rx_pr("\ttarget_min_lum: 0x%x\n",
		      dv_vsvdb->target_min_lum);
		rx_pr("\tcolorimetry: 0x%x\n",
		      dv_vsvdb->colorimetry);
	} else if (dv_vsvdb->version == 2) {
		rx_pr("\tDM_version: 0x%x\n",
		      dv_vsvdb->DM_version);
		rx_pr("\tbacklt_min_luma:\n");
		switch (dv_vsvdb->backlt_min_luma) {
		case 0:
			rx_pr("\t25cd/m^2\n");
			break;
		case 1:
			rx_pr("\t50cd/m^2\n");
			break;
		case 2:
			rx_pr("\t75cd/m^2\n");
			break;
		case 3:
			rx_pr("\t100cd/m^2\n");
			break;
		default:
			break;
		}
		rx_pr("\tinterface:\n");
		switch (dv_vsvdb->interface) {
		case 0:
			rx_pr("\tLow_Latency\n");
			break;
		case 1:
			rx_pr("\tLow_Latency + Low_Latency_HDMI\n");
			break;
		case 2:
			rx_pr("\tStandard + Low_Latency\n");
			break;
		case 3:
			rx_pr("\tStandard + Low_Latency + Low_Latency_HDMI\n");
			break;
		default:
			break;
		}
		rx_pr("\tsupports_10b_12b_444:\n");
		switch (dv_vsvdb->sup_10b_12b_444) {
		case 0:
			rx_pr("\tNot support\n");
			break;
		case 1:
			rx_pr("\t10bit\n");
			break;
		case 2:
			rx_pr("\t12bit\n");
			break;
		case 3:
			rx_pr("\tReserved\n");
			break;
		default:
			break;
		}
		rx_pr("\ttarget_max_pq_v2: 0x%x\n",
		      dv_vsvdb->tmaxPQ);
		rx_pr("\ttarget_min_pq_v2: 0x%x\n",
		      dv_vsvdb->tminPQ);
	}
}

void rx_parse_print_cdb(struct colorimetry_db_s *color_db)
{
	if (!color_db)
		return;
	rx_pr("\n");
	rx_pr("****Colorimetry Data Block****\n");
	rx_pr("supported colorimetry:\n");
	if (color_db->bt2100_ictcp)
		rx_pr("\tBT2100_ICtCp\n");
	if (color_db->dci_p3)
		rx_pr("\tDCI_P3\n");
	if (color_db->BT2020_RGB)
		rx_pr("\tBT2020_RGB\n");
	if (color_db->BT2020_YCC)
		rx_pr("\tBT2020_YCC\n");
	if (color_db->bt2020_cycc)
		rx_pr("\tbt2020_cycc\n");
	if (color_db->adobe_rgb)
		rx_pr("\tadobe_rgb\n");
	if (color_db->adobe_ycc601)
		rx_pr("\tadobe_ycc601\n");
	if (color_db->sycc601)
		rx_pr("\tsycc601\n");
	if (color_db->xvycc709)
		rx_pr("\txvycc709\n");
	if (color_db->xvycc601)
		rx_pr("\txvycc601\n");
	if (color_db->srgb)
		rx_pr("\tsRGB\n");
	if (color_db->defaultrgb)
		rx_pr("\tdefaultRGB\n");


	rx_pr("supported colorimetry metadata:\n");
	if (color_db->MD3)
		rx_pr("\tMD3\n");
	if (color_db->MD2)
		rx_pr("\tMD2\n");
	if (color_db->MD1)
		rx_pr("\tMD1\n");
	if (color_db->MD0)
		rx_pr("\tIDO-defined gamut-related metadata\n");
}

void rx_parse_print_hdr_static(struct hdr_db_s *hdr_db)
{
	if (!hdr_db)
		return;
	rx_pr("\n");
	rx_pr("****HDR Static Metadata Data Block****\n");
	rx_pr("eotf_hlg: %d\n",
	      hdr_db->eotf_hlg);
	rx_pr("eotf_smpte_st_2084: %d\n",
	      hdr_db->eotf_smpte_st_2084);
	rx_pr("eotf_hdr: %d\n",
	      hdr_db->eotf_hdr);
	rx_pr("eotf_sdr: %d\n",
	      hdr_db->eotf_sdr);
	rx_pr("hdr_SMD_type1: %d\n",
	      hdr_db->hdr_SMD_type1);
	if (hdr_db->hdr_lum_max)
		rx_pr("Desired Content Max Luminance: 0x%x\n",
		      hdr_db->hdr_lum_max);
	if (hdr_db->hdr_lum_avg)
		rx_pr("Desired Content Max Frame-avg Luminance: 0x%x\n",
		      hdr_db->hdr_lum_avg);
	if (hdr_db->hdr_lum_min)
		rx_pr("Desired Content Min Luminance: 0x%x\n",
		      hdr_db->hdr_lum_min);
}

void rx_parse_print_hdr_dynamic(struct hdr_dy_db_s *hdr_dy)
{
	u8 i = 0, j = 0;
	u8 version = 0, sl_hdr_mode_sup = 0;
	char buffer[15] = {0};
	int offset = 0;

	if (!hdr_dy)
		return;
	rx_pr("\n");
	rx_pr("****HDR Dynamic Metadata Data Block****\n");
	for (i = 0; i < hdr_dy->num; ++i) {
		rx_pr("Supported Type %d:\n", i);
		memset(buffer, 0, 15);
		offset = 0;
		for (j = 0; j < hdr_dy->hdr_sup[i].field_len; ++j)
			offset += sprintf(buffer + offset, "%02x", hdr_dy->hdr_sup[i].field[j]);
		version = hdr_dy->hdr_sup[i].flag & 0xf;
		switch (hdr_dy->hdr_sup[i].type) {
		case 1:
			rx_pr("\tType 1[CTA-861-G, Annex R]\n");
			rx_pr("\ttype_1_hdr_metadata_version: %d\n", version);
			break;
		case 2:
			rx_pr("\tType 2[ETSI TS 103 433]\n");
			rx_pr("\ts_103_433_spec_version: %d\n", version);
			sl_hdr_mode_sup = (hdr_dy->hdr_sup[i].flag & 0x70) >> 4;
			rx_pr("\tsl_hdr_mode_sup: %d\n", sl_hdr_mode_sup);
			break;
		case 3:
			rx_pr("\tType 3[ITU-T H.265]\n");
			break;
		case 4:
			rx_pr("\tType 4[CTA-861-G, Annex S]\n");
			rx_pr("\type_4_hdr_metadata_version: %d\n", version);
			break;
		case 0x100:
			rx_pr("\tgraphics_overlay_flag_version: %d\n", version);
			break;
		default:
			break;
		}
		rx_pr("\tFields:0x%s\n", buffer);
	}
}

void rx_parse_print_y420vdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i = 0;

	if (!edid_info)
		return;
	rx_pr("\n");
	rx_pr("****Y420 Video Data Block****\n");
	for (i = 0; i < edid_info->y420_vic_len; i++)
		rx_edid_print_vic_fmt(i, edid_info->y420_vdb_vic[i]);
}

void rx_parse_print_y420cmdb(struct cta_blk_parse_info *edid_info)
{
	unsigned char i = 0;
	unsigned char hdmi_vic = 0;

	if (!edid_info)
		return;
	rx_pr("\n");
	rx_pr("****Yc420 capability map Data Block****\n");
	if (edid_info->y420_all_vic) {
		rx_pr("all vic support y420\n");
	} else {
		for (i = 0; i < 31; i++) {
			hdmi_vic = edid_info->y420_cmdb_vic[i];
			if (hdmi_vic)
				rx_edid_print_vic_fmt(i, hdmi_vic);
		}
	}
}

void rx_parse_print_hdr10p(struct cta_blk_parse_info *edid_info)
{
	if (!edid_info)
		return;
	rx_pr("\n");
	rx_pr("****HDR10P Data Block****\n");
	rx_pr("Application version:%d\n", edid_info->hdr10pdb.hdr10p_ver);
	rx_pr("Full Frame Peak Luminance Index:%d\n", edid_info->hdr10pdb.full_frame_peak);
	rx_pr("Peak Luminance Index:%d\n", edid_info->hdr10pdb.peak);
}

void rx_parse_print_fsdb(struct freesync_db_s *fs_db)
{
	u8 i = 0;
	char buffer[FSDB_PAYLOAD_LEN + 1] = {0};
	int offset = 0;

	if (!fs_db)
		return;
	memset(buffer, 0, FSDB_PAYLOAD_LEN + 1);
	rx_pr("\n");
	rx_pr("****Freesync Data Block****\n");
	for (i = 0; i < fs_db->payload_len; i++)
		offset += sprintf(buffer + offset, "%02x", fs_db->payload[i]);
	rx_pr("freesync payload: 0x%s\n", buffer);
}

void rx_parse_print_ifdb(struct info_db_s *if_db)
{
	u8 i = 0, j = 0;
	char buffer[IFDB_PAYLOAD_LEN + 1] = {0};
	int offset = 0;

	if (!if_db)
		return;
	rx_pr("\n");
	rx_pr("****InfoFrame Data Block****\n");
	rx_pr("VSIF num: %d\n", if_db->vsif_num);
	rx_pr("payload_len: %d\n", if_db->payload_len);
	memset(buffer, 0, IFDB_PAYLOAD_LEN + 1);
	for (j = 0; j < if_db->payload_len; j++)
		offset += sprintf(buffer + offset, "%02x", if_db->payload[j]);

	rx_pr("header payload: %s\n", buffer);

	for (i = 0; i < 31; ++i) {
		if (if_db->short_info[i].type_code == 0)
			break;
		rx_pr("short_info[%d] payload length:%d\n", i,
			if_db->short_info[i].payload_len);
		memset(buffer, 0, IFDB_PAYLOAD_LEN + 1);
		offset = 0;
		for (j = 0; j < if_db->short_info[i].payload_len; j++)
			offset += sprintf(buffer + offset, "%02x",
				if_db->short_info[i].payload[j]);

		switch (if_db->short_info[i].type_code) {
		case 2:
			rx_pr("\tSupport AVI\n");
			break;
		case 3:
			rx_pr("\tSupport SPD\n");
			break;
		case 4:
			rx_pr("\tSupport Audio\n");
			break;
		case 5:
			rx_pr("\tSupport MEPG\n");
			break;
		case 6:
			rx_pr("\tSupport NTSC VBI\n");
			break;
		case 7:
			rx_pr("\tOthers\n");
			break;
		default:
			break;
		}
		rx_pr("\tpayload: %s\n", buffer);
	}
	for (i = 0; i < 31; ++i) {
		if (if_db->short_vs_info[i].ieee_oui == 0)
			break;
		memset(buffer, 0, IFDB_PAYLOAD_LEN + 1);
		offset = 0;
		rx_pr("short_vs_info[%d] payload length:%d\n", j,
			if_db->short_vs_info[i].payload_len);
		for (j = 0; j < if_db->short_info[i].payload_len; j++)
			offset += sprintf(buffer + offset, "%02x",
				if_db->short_vs_info[i].payload[j]);

		switch (if_db->short_vs_info[i].ieee_oui) {
		case 0x00d046:
			rx_pr("\tSupport DV video db\n");
			break;
		case 0x90848b:
			rx_pr("\tSupport HDR10P db\n");
			break;
		case 0x000c03:
			rx_pr("\tSupport VS HDMI1.4b db\n");
			break;
		case 0xc45dd8:
			rx_pr("\tSupport HF db\n");
			break;
		case 0x00001a:
			rx_pr("\tSupport freesync db\n");
			break;
		default:
			break;
		}
		rx_pr("\tpayload: %s\n", buffer);
	}
}

static void rx_parse_print_3d_sup(u8 sup)
{
	rx_pr("\t3D support:");
	switch (sup) {
	case 0:
		rx_pr("\tShall always be displayed in mono\n");
		break;
	case 1:
		rx_pr("\tShall always be displayed in stereo\n");
		break;
	case 2:
		rx_pr("\tShall be displayed in mono or stereo, depending on a user action\n");
		break;
	default:
		rx_pr("\tReserved\n");
		break;
	}
}

void rx_parse_print_t7vtdb(struct t7vtdb_s *t7vfdb)
{
	if (!t7vfdb)
		return;

	rx_pr("\n");
	rx_pr("****DisplayID Type VII Video Timing Data Block****\n");
	rx_pr("NOTE:In CTA-861, Descriptor length shall be 20\n");
	rx_pr("DSC pass-through support shall be 0\n");
	rx_pr("Block revision shall be 010b\n");
	rx_pr("\tDescriptor length: %d\n", 20 + t7vfdb->t7_m);
	rx_pr("\tDSC pass-through support: %d\n", t7vfdb->dsc_pt);
	rx_pr("\tBlock revision: %d\n", t7vfdb->block_revision);
	rx_pr("\tPixel clock: %dKHz\n", t7vfdb->pixel_clk);
	if (t7vfdb->t7y420)
		rx_pr("\tSupport RGB and Y420\n");
	else
		rx_pr("\tNot support Y420\n");
	rx_parse_print_3d_sup(t7vfdb->_3d_sup);
	if (t7vfdb->t7il)
		rx_pr("\tTiming is interlaced\n");
	else
		rx_pr("\tTiming is progressive\n");
	rx_pr("\tAspect ratio: %s\n", aspect_ratio[t7vfdb->t7_aspect_ratio]);
	rx_pr("\tHactive: %d\n", t7vfdb->hactive);
	rx_pr("\tHblank: %d\n", t7vfdb->hblank);
	rx_pr("\tHfront porch: %d\n", t7vfdb->hoffset);
	rx_pr("\tHsync width: %d\n", t7vfdb->hsync_width);
	if (t7vfdb->t7hsp)
		rx_pr("\tHorizontal sync polarity: positive\n");
	else
		rx_pr("\tHorizontal sync polarity: negative\n");
	if (t7vfdb->t7vsp)
		rx_pr("\tVertical sync polarity: positive\n");
	else
		rx_pr("\tVertical sync polarity: negative\n");
	rx_pr("\tVactive: %d\n", t7vfdb->vactive);
	rx_pr("\tVblank: %d\n", t7vfdb->vblank);
	rx_pr("\tVfront porch: %d\n", t7vfdb->voffset);
	rx_pr("\tVsync width: %d\n", t7vfdb->vsync_width);
	rx_pr("\tRefresh rate: %d\n", t7vfdb->fresh_rate);
}

void rx_parse_print_t8vtdb(struct t8vtdb_s *t8vfdb)
{
	u8 i = 0;

	if (!t8vfdb)
		return;
	rx_pr("\n");
	rx_pr("****DisplayID Type VIII Video Timing Data Block****\n");
	if (t8vfdb->code_tyoe == 0)
		rx_pr("\tuse DMT timing\n");
	rx_pr("\tcode size is %d bytes\n", t8vfdb->tcs == 1 ? 2 : 1);
	rx_pr("\tYCbCr420 supported: %d\n", t8vfdb->t8y420);
	for (i = 0; i < t8vfdb->timing_num; ++i)
		rx_pr("\tDTM Timing Code: %d\n", t8vfdb->dmt_timing[i]);
}

void rx_parse_print_t10vtdb(struct t10vtdb_s *t10vtdb)
{
	u8 i = 0;

	if (!t10vtdb)
		return;
	rx_pr("\n");
	rx_pr("****DisplayID Type X Video Timing Data Block****\n");
	rx_pr("\tDescriptor length: %d\n", 6 + t10vtdb->t10_m);
	rx_pr("\tBlock revision: %d\n", t10vtdb->block_revision);
	for (i = 0; i < t10vtdb->desc_num; ++i) {
		rx_pr("\tDescriptor %d:\n", i);
		switch (t10vtdb->t10_desc[i].timing_formula) {
		case 0:
			rx_pr("\tFormula: v1.2 standard timing\n");
			rx_pr("\tCVT Algorithm: Reserved\n");
			break;
		case 1:
			rx_pr("\tFormula: v1.1 reduced-blanking(RB1)\n");
			rx_pr("\tCVT Algorithm: Reserved\n");
			break;
		case 2:
			rx_pr("\tFormula: v1.2 reduced-blanking(RB2)\n");
			if (t10vtdb->t10_desc[i].vrhb)
				rx_pr("\tCVT Algorithm: Support refresh Rate x 1000/1001\n");
			else
				rx_pr("\tCVT Algorithm: Not support refresh Rate x 1000/1001\n");
			break;
		case 3:
			rx_pr("\tFormula: v1.3 reduced-blanking(RB3)\n");
			if (t10vtdb->t10_desc[i].vrhb)
				rx_pr("\tCVT Algorithm: Horizontal Blank is 160 pixels\n");
			else
				rx_pr("\tCVT Algorithm: Horizontal Blank is 80 pixels\n");
			break;
		default:
			rx_pr("\tFormula: Reserved\n");
			rx_pr("\tCVT Algorithm: Reserved\n");
			break;
		}
		if (t10vtdb->t10_desc[i].t10_y420)
			rx_pr("\tSupport RGB and Y420\n");
		else
			rx_pr("\tNot support Y420\n");
		rx_parse_print_3d_sup(t10vtdb->t10_desc[i]._3d_sup);
		rx_pr("\tHactive: %d\n", t10vtdb->t10_desc[i].hactive + 1);
		rx_pr("\tVactive: %d\n", t10vtdb->t10_desc[i].vactive + 1);
		rx_pr("\tRefresh rate: %d\n", t10vtdb->t10_desc[i].fresh_rate + 1);
		rx_pr("\n");
	}
}

void rx_cea_ext_parse_print(struct cea_ext_parse_info *cea_ext_info)
{
	u8 i = 0;

	if (!cea_ext_info)
		return;
	rx_pr("\n");
	rx_pr("****CEA Extension Block Header****\n");
	rx_pr("cea_tag: 0x%x\n", cea_ext_info->cea_tag);
	rx_pr("cea_revision: 0x%x\n", cea_ext_info->cea_revision);
	rx_pr("dtd offset: %d\n", cea_ext_info->dtd_offset);
	rx_pr("underscan_sup: %d\n", cea_ext_info->underscan_sup);
	rx_pr("basic_aud_sup: %d\n", cea_ext_info->basic_aud_sup);
	rx_pr("ycc444_sup: %d\n", cea_ext_info->ycc444_sup);
	rx_pr("ycc422_sup: %d\n", cea_ext_info->ycc422_sup);
	rx_pr("native_dtd_num: %d\n", cea_ext_info->native_dtd_num);

	if (cea_ext_info->blk_parse_info.contain_vdb) {
		for (i = 0; i < cea_ext_info->blk_parse_info.video_db_num; ++i)
			rx_parse_print_vdb(&cea_ext_info->blk_parse_info.video_db[i]);
	}
	if (cea_ext_info->blk_parse_info.contain_adb) {
		for (i = 0; i < cea_ext_info->blk_parse_info.audio_db_num; ++i)
			rx_parse_print_adb(&cea_ext_info->blk_parse_info.audio_db[i]);
	}
	if (cea_ext_info->blk_parse_info.contain_sadb)
		rx_parse_print_spk_alloc(&cea_ext_info->blk_parse_info.speaker_alloc);
	if (cea_ext_info->blk_parse_info.contain_vsdb)
		rx_parse_print_vsdb(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_hf_vsdb)
		rx_parse_print_hf_vsdb(&cea_ext_info->blk_parse_info.hf_vsdb);
	if (cea_ext_info->blk_parse_info.contain_hf_scdb)
		rx_parse_print_hf_scdb(&cea_ext_info->blk_parse_info.hf_scdb);
	if (cea_ext_info->blk_parse_info.contain_hf_sbtm)
		rx_parse_print_hf_sbtm(&cea_ext_info->blk_parse_info.hf_sbtm);
	if (cea_ext_info->blk_parse_info.contain_vcdb)
		rx_parse_print_vcdb(&cea_ext_info->blk_parse_info.vcdb);
	if (cea_ext_info->blk_parse_info.contain_rcdb)
		rx_parse_print_rcdb(&cea_ext_info->blk_parse_info.rcdb);
	if (cea_ext_info->blk_parse_info.contain_sldb)
		rx_parse_print_sldb(&cea_ext_info->blk_parse_info.sldb);
	if (cea_ext_info->blk_parse_info.contain_cdb)
		rx_parse_print_cdb(&cea_ext_info->blk_parse_info.color_db);
	if (cea_ext_info->blk_parse_info.contain_hdmi_aud)
		rx_parse_print_hdmi_aud(&cea_ext_info->blk_parse_info.hdmi_adb);
	if (cea_ext_info->blk_parse_info.contain_vsvdb)
		rx_parse_print_dvsvdb(&cea_ext_info->blk_parse_info.dv_vsvdb);
	if (cea_ext_info->blk_parse_info.contain_vsadb)
		rx_parse_print_dvsadb(&cea_ext_info->blk_parse_info.dv_vsadb);
	if (cea_ext_info->blk_parse_info.contain_hdr_db)
		rx_parse_print_hdr_static(&cea_ext_info->blk_parse_info.hdr_db);
	if (cea_ext_info->blk_parse_info.contain_hdr_dy)
		rx_parse_print_hdr_dynamic(&cea_ext_info->blk_parse_info.hdr_dy_db);
	if (cea_ext_info->blk_parse_info.contain_y420_vdb)
		rx_parse_print_y420vdb(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_hdr10p)
		rx_parse_print_hdr10p(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_y420_cmdb)
		rx_parse_print_y420cmdb(&cea_ext_info->blk_parse_info);
	if (cea_ext_info->blk_parse_info.contain_ifdb)
		rx_parse_print_ifdb(&cea_ext_info->blk_parse_info.info_db);
	if (cea_ext_info->blk_parse_info.contain_fsdb)
		rx_parse_print_fsdb(&cea_ext_info->blk_parse_info.fsdb);
	if (cea_ext_info->blk_parse_info.contain_vfpdb)
		rx_parse_print_vfpdb(&cea_ext_info->blk_parse_info.vfpdb);
	if (cea_ext_info->blk_parse_info.contain_t7vtdb) {
		for (i = 0; i < cea_ext_info->blk_parse_info.t7vtdbdb_num; ++i)
			rx_parse_print_t7vtdb(&cea_ext_info->blk_parse_info.t7vtdb[i]);
	}
	if (cea_ext_info->blk_parse_info.contain_t8vtdb) {
		for (i = 0; i < cea_ext_info->blk_parse_info.t8vtdbdb_num; ++i)
			rx_parse_print_t8vtdb(&cea_ext_info->blk_parse_info.t8vtdb[i]);
	}
	if (cea_ext_info->blk_parse_info.contain_t10vtdb) {
		for (i = 0; i < cea_ext_info->blk_parse_info.t10vtdbdb_num; ++i)
			rx_parse_print_t10vtdb(&cea_ext_info->blk_parse_info.t10vtdb[i]);
	}
	for (i = 0; i < cea_ext_info->blk_parse_info.dtd_num; ++i)
		rx_parse_print_dtd(&cea_ext_info->blk_parse_info.dtd[i]);
}

static void rx_edid_print_cta(struct cea_ext_parse_info *cea_ext_info, u8 cta_pos)
{
	if (cta_pos >= sizeof(cta_flag) || cta_flag[cta_pos] == 0)
		return;
	rx_cea_ext_parse_print(cea_ext_info);
	rx_blk_index_print(&cea_ext_info->blk_parse_info);
	rx_pr("CEA ext blk free size: %d\n", cea_ext_info->free_size);
	rx_pr("CEA ext blk total free size(include dtd size): %d\n",
	      cea_ext_info->total_free_size);
	rx_pr("CEA ext blk dtd size: %d\n", cea_ext_info->dtd_size);
	rx_pr("\n");
}

void rx_edid_parse_print(struct edid_info_s *edid_info)
{
	if (!edid_info)
		return;

	rx_parse_blk0_print(edid_info);
	rx_edid_print_cta(&edid_info->cea_ext_info, 1);
	rx_edid_print_cta(&edid_info->cea_ext_info1, 2);
	rx_edid_print_cta(&edid_info->cea_ext_info2, 3);
}

void rx_data_blk_index_print(struct cta_data_blk_info *db_info)
{
	if (!db_info)
		return;
	rx_pr("%-7d\t 0x%-5X\t %-30s\t 0x%-8X\t %d\n",
	      db_info->cta_blk_index,
	      db_info->tag_code,
	      rx_get_cta_blk_name(db_info->tag_code),
	      db_info->offset,
	      db_info->blk_len);
}

void rx_blk_index_print(struct cta_blk_parse_info *blk_info)
{
	int i = 0;

	if (!blk_info)
		return;
	rx_pr("\n");
	rx_pr("****CTA Data Block Index****\n");
	rx_pr("%-7s\t %-7s\t %-30s\t %-10s\t %s\n",
	      "blk_idx", "blk_tag", "blk_name",
	      "blk_offset", "blk_len");
	for (i = 0; i < blk_info->data_blk_num; i++)
		rx_data_blk_index_print(&blk_info->db_info[i]);
}

#ifdef CONFIG_AMLOGIC_HDMITX
void rx_edid_physical_addr(int a, int b, int c, int d)
{
	//tx_hpd_event = E_RCV;
	//if (edid_from_tx & 2) {
		//rx_pr("whole edid from tx. no need to update physical addr");
		//return;
	//}
	up_phy_addr = ((d & 0xf) << 12) |
		   ((c & 0xf) <<  8) |
		   ((b & 0xf) <<  4) |
		   ((a & 0xf) <<  0);

	/* if (log_level & EDID_LOG) */
	rx_pr("\nup_phy_addr = %x\n", up_phy_addr);
}
EXPORT_SYMBOL(rx_edid_physical_addr);
#endif

unsigned char rx_get_cea_dtd_size(unsigned char *cur_edid, unsigned int size)
{
	unsigned char dtd_block_offset = 0;
	unsigned char dtd_size = 0;

	if (!cur_edid)
		return 0;
	/* get description offset */
	dtd_block_offset = cur_edid[EDID_DESCRIP_OFFSET];
	if (log_level & EDID_LOG)
		rx_pr("%s dtd offset start:%d\n", __func__, dtd_block_offset);
	/* dtd first two bytes are pixel clk != 0 */
	while ((dtd_block_offset + 1 < size - 1) &&
	       (cur_edid[dtd_block_offset] ||
		cur_edid[dtd_block_offset + 1])) {
		dtd_block_offset += DETAILED_TIMING_LEN;
		if (dtd_block_offset > size - 1)
			break;
		dtd_size += DETAILED_TIMING_LEN;
	}
	if (log_level & EDID_LOG)
		rx_pr("%s block_start end:%d\n", __func__, dtd_block_offset);

	return dtd_size;
}

/* rx_get_total_free_size
 * get total free size including dtd
 */
unsigned char rx_edid_total_free_size(unsigned char *cur_edid,
				      unsigned int size)
{
	unsigned char dtd_block_offset = 0;

	if (!cur_edid)
		return 0;
	/* get description offset */
	dtd_block_offset = cur_edid[EDID_DESCRIP_OFFSET];
	if (log_level & EDID_LOG)
		rx_pr("%s total free size: %d\n", __func__,
		      size - dtd_block_offset - 1);
	/* free size except checksum */
	return size - dtd_block_offset - 1;
}

/* extract data block with certain tag from block buf */
u8 *data_blk_extract(u8 *p_buf, u16 tagid)
{
	unsigned int index = 0;
	u8 tag_length = 0;
	u16 tag_code = 0;

	if (!p_buf)
		return NULL;
	//TODO: for 512byte edid
	while (index < EDID_BLK_SIZE) {
		/* Get the tag and length */
		tag_code = rx_get_tag_code(p_buf + index);
		tag_length = BLK_LENGTH(p_buf[index]);
		if (tagid == tag_code)
			return &p_buf[index];
		index += tag_length + 1;
	}
	return NULL;
}

/* extract data block with certain tag from EDID */
u8 *edid_tag_extract(u8 *p_edid, u16 tagid)
{
	unsigned int index = EDID_EXT_BLK_OFF;
	u8 tag_length = 0;
	u16 tag_code = 0;
	unsigned char max_offset = 0;

	if (!p_edid)
		return NULL;
	 /* check if a cea extension block */
	if (p_edid[126]) {
		index += 4;
		max_offset = p_edid[130] + EDID_EXT_BLK_OFF;
		while (index < max_offset) {
			tag_code = rx_get_tag_code(p_edid + index);
			tag_length = BLK_LENGTH(p_edid[index]);
			if (tag_code == tagid)
				return &p_edid[index];
			index += tag_length + 1;
		}
	}
	return NULL;
}

bool rx_set_earc_cap_ds(unsigned char *data, unsigned int len)
{
	if (rx_info.chip_id == CHIP_ID_NONE)
		return false;
	memset(recv_earc_cap_ds, 0, sizeof(recv_earc_cap_ds));
	if (!data || len > EARC_CAP_DS_MAX_LENGTH)
		return false;

	memcpy(recv_earc_cap_ds, data, len);

	rx_pr("*update earc cap_ds to edid*\n");
	hdmi_rx_top_edid_update(rx_info.arc_port);
	/* if currently in arc port, don't reset hpd */
	if (rx_info.main_port_open && rx_info.main_port != rx_info.arc_port) {
		if (earc_cap_ds_update_hpd_en)
			rx_send_hpd_pulse(rx_info.main_port);
	} else {
		pre_port = 0xff;
		rx_pr("update cap_ds later, in ARC port:%s\n",
		      rx_info.main_port == rx_info.arc_port ? "Y" : "N");
	}
	return true;
}
EXPORT_SYMBOL(rx_set_earc_cap_ds);

/* now only consider VSVDB_DV. vsvdb length:
 * v0:26bytes, v1-15:15bytes, v1-12/v2 12bytes
 */
bool rx_set_vsvdb(unsigned char *data, unsigned int len)
{
	/* len = 0, disable/remove VSVDB in edid
	 * len = 0xFF, revert to original VSVDB in edid
	 * len = 12/15/26, replace VSVDB with param[data]
	 */
	if (rx_info.chip_id == CHIP_ID_NONE)
		return false;
	if (len > VSVDB_LEN && len != 0xFF) {
		rx_pr("err: invalid vsvdb length:%d\n", len);
		return false;
	}
	memset(recv_vsvdb, 0, sizeof(recv_vsvdb));
	if (data && len <= VSVDB_LEN)
		memcpy(recv_vsvdb, data, len);
	recv_vsvdb_len = len;
	rx_pr("*update vsvdb by DV, len=%d*\n", len);
	hdmi_rx_top_edid_update(rx_info.main_port);
	if (rx_info.main_port_open) {
		if (vsvdb_update_hpd_en)
			rx_send_hpd_pulse(rx_info.main_port);
	} else {
		pre_port = 0xff;
		rx_pr("update vsvdb later\n");
	}
	return true;
}
EXPORT_SYMBOL(rx_set_vsvdb);

#ifdef CONFIG_AMLOGIC_HDMITX
bool rx_update_tx_edid_with_audio_block(unsigned char *edid_data, unsigned char *audio_block)
{
	if (rx_info.chip_id == CHIP_ID_NONE)
		return false;
	if (!edid_data) {
		memset(edid_tx, 0, EDID_SIZE);
		return false;
	}

	memcpy(edid_tx, edid_data, EDID_SIZE);
	return true;
}
EXPORT_SYMBOL(rx_update_tx_edid_with_audio_block);
#endif

bool rx_edid_support_4k(u8 *p_edid)
{
	unsigned int hactive = 0, i = 0, j = 0, len = 0, pos = 0, vic = 0;
	unsigned int offset = 0, cta_num = 0;
	struct data_block_location_s ret = {0};

	cta_num = rx_edid_cta_blk_num(p_edid);
	for (i = 0; i <= 7; i++) {
		if (p_edid[0x26 + i * 2] != 0x01 && p_edid[0x27 + i * 2] != 0x01) {
			hactive = (p_edid[0x26 + i * 2] + 31) * 8;
			if (hactive >= 3840)
				return true;
		}
	}
	//descriptor1
	if (p_edid[0x36] != 0 || p_edid[0x37] != 0) {
		hactive = p_edid[0x38] + (((p_edid[0x3A] >> 4) & 0xF) << 8);
		if (hactive >= 3840)
			return true;
	}
	//descriptor2
	if (p_edid[0x48] != 0 || p_edid[0x49] != 0) {
		hactive = p_edid[0x4A] + (((p_edid[0x4C] >> 4) & 0xF) << 8);
		if (hactive >= 3840)
			return true;
	}
	//vdb
	i = 0;
	ret = rx_get_cea_tag_offset(p_edid, VIDEO_TAG);
	while (i++ < ret.num) {
		if (ret.pos[i] == 0)
			break;
		len = p_edid[ret.pos[i]] & 0x1F;
		for (j = 0; j < len; ++j) {
			vic = p_edid[ret.pos[j] + j + 1];
			if ((vic >= 93 && vic <= 107) ||
				(vic >= 114 && vic <= 120) ||
				vic == 218 || vic == 219)
				return true;
		}
	}

	//dtd
	for (i = 1; i <= cta_num; ++i) {
		offset = p_edid[128 * i + 2];
		pos = 128 * i + offset;
		while (pos + 18 < 128 * i + 127) {
			if (p_edid[pos] == 0 && p_edid[pos + 1] == 0)
				break;
			hactive = p_edid[pos + 2] + (((p_edid[pos + 4] >> 4) & 0xF) << 8);
			if (hactive >= 3840)
				return true;
			pos += 18;
		}
	}
	//other
	i = 0;
	ret = rx_get_cea_tag_offset(p_edid, T7VTDB_TAG);
	while (i++ < ret.num) {
		if (ret.pos[i] == 0)
			break;
		hactive = p_edid[ret.pos[i] + 7] + (p_edid[ret.pos[i] + 8] << 8) + 1;
		if (hactive >= 3840)
			return true;
	}

	return false;
}

bool get_edid_support(u_char *edid_buf, enum edid_support_e func)
{
	u32 hf_vsdb_start = 0;
	u8 tag_len = 0;
	struct data_block_location_s ret = {0};

	if (!edid_buf)
		return false;
	if (func < HF_DB) {
		ret = rx_get_cea_tag_offset(edid_buf, HF_VENDOR_DB_TAG);
		hf_vsdb_start = ret.pos[0];
		if (!hf_vsdb_start)
			return false;
		tag_len = edid_buf[hf_vsdb_start] & 0x1f;
	}
	switch (func) {
	case HF_VRR:
		if (tag_len < 9)
			return false;
		if (edid_buf[hf_vsdb_start  + 9] > 0)
			return true;
		break;
	case HF_ALLM:
		if (tag_len < 8)
			return false;
		if (edid_buf[hf_vsdb_start  + 8] & 0x2)
			return true;
		break;
	case HF_QMS:
		if (tag_len < 8)
			return false;
		if (edid_buf[hf_vsdb_start  + 8] & 0x40)
			return true;
		break;
	case HF_DB:
		ret = rx_get_cea_tag_offset(edid_buf, HF_VENDOR_DB_TAG);
		if (ret.num > 0)
			return true;
		break;
	case VSDV_DB:
		ret = rx_get_cea_tag_offset(edid_buf, VSVDB_DV_TAG);
		if (ret.num > 0)
			return true;
		break;
	case HDR10P_DB:
		ret = rx_get_cea_tag_offset(edid_buf, VSVDB_HDR10P_TAG);
		if (ret.num > 0)
			return true;
		break;
	case HDR_STATIC_DB:
		ret = rx_get_cea_tag_offset(edid_buf, EXTENDED_HDR_STATIC_TAG);
		if (ret.num > 0)
			return true;
		break;
	case HDR_DYNAMIC_DB:
		ret = rx_get_cea_tag_offset(edid_buf, EXTENDED_HDR_DYNAMIC_TAG);
		if (ret.num > 0)
			return true;
		break;
	case FREESYNC_DB:
		ret = rx_get_cea_tag_offset(edid_buf, VSDB_FREESYNC_TAG);
		if (ret.num > 0)
			return true;
		break;
	case TIMING_4K:
		return rx_edid_support_4k(edid_buf);
	default:
		break;
	}
	return false;
}

/* combine short audio descriptors,
 * see FigureH-3 of HDMI2.1 SPEC
 */
unsigned char *compose_audio_db(u8 *aud_db, u8 *add_buf)
{
	u8 aud_db_len = 0;
	u8 add_buf_len = 0;
	u8 payload_len = 0;
	u8 tmp_aud[DB_LEN_MAX - 1] = {0};
	u8 *tmp_buf = add_buf;

	u8 i = 0, j = 0;
	u8 idx = 1;
	enum edid_audio_format_e aud_fmt = 0;
	enum edid_audio_format_e tmp_fmt = 0;

	if (!aud_db || !add_buf)
		return NULL;
	memset(com_aud, 0, sizeof(com_aud));
	aud_db_len = BLK_LENGTH(aud_db[0]) + 1;
	add_buf_len = BLK_LENGTH(add_buf[0]) + 1;

	for (i = 1; (i < aud_db_len) && (idx < DB_LEN_MAX - 1); i += SAD_LEN) {
		aud_fmt = (aud_db[i] & 0x78) >> 3;
		for (j = 1; j < add_buf_len; j += SAD_LEN) {
			tmp_fmt = (tmp_buf[j] & 0x78) >> 3;
			if (aud_fmt == tmp_fmt)
				break;
		}
		/* copy this audio data to payload */
		if (j == add_buf_len) {
			/* not find this fmt in add_buf */
			memcpy(com_aud + idx, aud_db + i, SAD_LEN);
		} else {
			/* find this fmt in add_buf */
			memcpy(com_aud + idx, tmp_buf + j, SAD_LEN);
			/* delete this fmt from add_buf */
			memcpy(tmp_aud + 1, tmp_buf + 1, j - 1);
			memcpy(tmp_aud + j, tmp_buf + j + SAD_LEN,
			       add_buf_len - j - SAD_LEN);
			add_buf_len -= SAD_LEN;
			tmp_buf = tmp_aud;
		}
		idx += SAD_LEN;
	}
	/* copy remain Short Audio Descriptors
	 * in add_buf, except blk header
	 */
	if (idx < sizeof(com_aud))
		if (idx + add_buf_len - 1 <= sizeof(com_aud))
			memcpy(com_aud + idx, tmp_buf + 1, add_buf_len - 1);
	payload_len = (idx - 1) + (add_buf_len - 1);
	/* data blk header */
	com_aud[0] = (AUDIO_TAG << 5) | payload_len;

	if (log_level & EDID_DATA_LOG) {
		rx_pr("++++after compose, audio data blk:\n");
		for (i = 0; i < payload_len + 1; i++)
			rx_pr("%02x", com_aud[i]);
		rx_pr("\n");
	}
	return com_aud;
}

/* param[add_buf]: contain only one data blk.
 * param[blk_idx]: sequence number of the data blk
 * 1.if the data blk is not present in edid, then
 * add this data blk according to add_blk_idx,
 * otherwise compose and replace this data blk.
 * 2.if blk_idx is 0xFF, then add this data blk
 * after the last data blk, otherwise insert it
 * in the blk_idx place.
 */
void splice_data_blk_to_edid(u_char *p_edid, u_char *add_buf,
			     u_char blk_idx)
{
	/* u8 *tag_data_blk = NULL; */
	u8 *add_data_blk = NULL;
	u8 tag_db_len = 0;
	u8 add_db_len = 0;
	u8 diff_len = 0;
	u16 tag_code = 0;
	u8 tag_offset = 0;
	u8 block_pos = 1;
	int free_size = 0, tmp_size = 0;
	u8 total_free_size = 0;
	u8 free_space_off = 0;
	u32 i = 0;
	u32 blk_end = 0, edid_size = 0;
	struct cea_ext_parse_info *cea_ext = NULL;
	u8 num = 0;

	if (!p_edid || !add_buf)
		return;

	edid_size = rx_get_edid_size(p_edid);

	total_free_size =
		rx_edid_total_free_size(p_edid + EDID_BLK_SIZE * block_pos,
		EDID_BLK_SIZE);
	blk_end = EDID_BLK_SIZE * (block_pos + 1) - 1;
	tag_code = (add_buf[0] >> 5) & 0x7;
	if (tag_code == USE_EXTENDED_TAG)
		tag_code = (tag_code << 8) | add_buf[1];
	/* tag_data_blk = edid_tag_extract(p_edid, tag_code); */
	tag_offset = rx_get_ceadata_offset(p_edid, add_buf);

	/* if (!tag_data_blk) { */
	if (tag_offset == 0) {
		//TODO
		cea_ext = kzalloc(sizeof(*cea_ext), GFP_KERNEL);
		if (!cea_ext)
			return;
		/* not find the data blk, add it to edid */
		add_db_len = BLK_LENGTH(add_buf[0]) + 1;
		edid_parse_cea_ext_block(p_edid + EDID_EXT_BLK_OFF,
					 cea_ext);
		/* decide to add db after which data blk */
		num = cea_ext->blk_parse_info.data_blk_num;
		if (num > 3 || blk_idx > 3) {
			kfree(cea_ext);
			return;
		}
		if (blk_idx >= num)
			/* add after the last data blk */
			tag_offset =
				cea_ext->blk_parse_info.db_info[num - 1].offset +
				cea_ext->blk_parse_info.db_info[num - 1].blk_len;
		else
			/* insert in blk_idx */
			tag_offset =
				cea_ext->blk_parse_info.db_info[blk_idx].offset;
		tag_offset += EDID_BLOCK1_OFFSET;
		kfree(cea_ext);
		//block_pos:Select which block to add
		for (; block_pos < edid_size / EDID_BLK_SIZE; ++block_pos) {
			free_size = rx_get_cta_free_size(p_edid + EDID_BLK_SIZE * block_pos,
				EDID_BLK_SIZE);
			if (add_db_len <= free_size)
				break;
		}
		if (add_db_len <= free_size) {
			/* move data behind added data block, except checksum */
			for (i = 0; i < blk_end - tag_offset - add_db_len; i++)
				p_edid[blk_end - i - 1] =
					p_edid[blk_end - i - 1 - add_db_len];
		} else if (en_take_dtd_space) {
			if (add_db_len > total_free_size) {
				rx_pr("no enough space for splicing3, abort\n");
				return;
			}
			/* actually, total free size = free_size + dtd_size,
			 * add_db_len won't excess 32bytes, so may excess
			 * one dtd length, but mustn't excess 2 dtd length.
			 * need clear the replaced dtd to 0
			 */
			if (add_db_len <= free_size + DETAILED_TIMING_LEN) {
				free_space_off =
					blk_end - free_size - DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
				       DETAILED_TIMING_LEN);
			} else {
				free_space_off =
					blk_end - free_size - 2 * DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
				       2 * DETAILED_TIMING_LEN);
			}
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < blk_end - tag_offset - add_db_len; i++)
				p_edid[blk_end - i - 1] =
					p_edid[blk_end - i - 1 - add_db_len];
		} else {
			rx_pr("no enough space for splicing4, abort\n");
			return;
		}
		/* dtd offset modify */
		p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] +=
			add_db_len;
		/* copy added data block */
		memcpy(p_edid + tag_offset, add_buf, add_db_len);
	} else {
		/* tag_db_len = BLK_LENGTH(tag_data_blk[0])+1; */
		tag_db_len = BLK_LENGTH(p_edid[tag_offset]) + 1;
		add_data_blk = add_buf;
		/* replace data blk */
		add_db_len = BLK_LENGTH(add_data_blk[0]) + 1;
		tmp_size = rx_get_cta_free_size(p_edid + EDID_BLK_SIZE *
				(tag_offset / EDID_BLK_SIZE), EDID_BLK_SIZE);
		if (tmp_size < 0)
			free_size = 0;
		else
			free_size = tmp_size;
		if (tag_db_len >= add_db_len) {
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < blk_end - tag_offset - tag_db_len; i++)
				p_edid[tag_offset + i + add_db_len] =
					p_edid[tag_offset + i + tag_db_len];
			/* need clear the new free space to 0 */
			free_size += (tag_db_len - add_db_len);
			free_space_off = blk_end - free_size;
			if (free_size > 0 && free_size < 32)
				memset(&p_edid[free_space_off], 0, free_size);
		} else if (add_db_len - tag_db_len <= free_size) {
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < blk_end - tag_offset - add_db_len; i++)
				p_edid[blk_end - i - 1] =
					p_edid[blk_end - i - 1 - (add_db_len - tag_db_len)];
		} else if (en_take_dtd_space) {
			diff_len = add_db_len - tag_db_len;
			if (diff_len > total_free_size) {
				rx_pr("no enough space for splicing, abort\n");
				return;
			}
			/* actually, total free size = free_size + dtd_size,
			 * diff_len won't excess 31bytes, so may excess
			 * one dtd length, but mustn't excess 2 dtd length.
			 * need clear the replaced dtd to 0
			 */
			if (diff_len <= free_size + DETAILED_TIMING_LEN) {
				free_space_off =
					blk_end - free_size - DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
				       DETAILED_TIMING_LEN);
			} else {
				free_space_off =
					blk_end - free_size - 2 * DETAILED_TIMING_LEN;
				memset(&p_edid[free_space_off], 0,
				       2 * DETAILED_TIMING_LEN);
			}
			/* move data behind current data
			 * block, except checksum
			 */
			for (i = 0; i < blk_end - tag_offset - add_db_len; i++)
				p_edid[blk_end - i - 1] =
					p_edid[blk_end - i - 1 - (add_db_len - tag_db_len)];
		} else {
			rx_pr("no enough space for splicing2, abort\n");
			return;
		}
		/* dtd offset modify */
		p_edid[EDID_BLK_SIZE * (tag_offset / EDID_BLK_SIZE)
			+ EDID_DESCRIP_OFFSET] +=
			(add_db_len - tag_db_len);
		/* copy added data block */
		memcpy(p_edid + tag_offset, add_data_blk, add_db_len);
	}
	if (log_level & EDID_DATA_LOG) {
		rx_pr("++++edid data after splice:\n");
		for (i = 128 * block_pos; i < 128 * (block_pos + 1); i++)
			pr_cont("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

/* param[add_buf] may contain multi data blk,
 * search the blk filtered by tag, and then
 * splice it with edid
 */
void splice_tag_db_to_edid(u8 *p_edid, u8 *add_buf,
			   u16 tagid)
{
	u8 *tag_data_blk = NULL;
	unsigned int i = 0;

	if (!p_edid || !add_buf)
		return;
	tag_data_blk = data_blk_extract(add_buf, tagid);
	if (!tag_data_blk) {
		rx_pr("no this data blk in add buf\n");
		return;
	}
	if (log_level & EDID_LOG) {
		rx_pr("++++extracted data blk(tag=0x%x):\n", tagid);
		for (i = 0; i < BLK_LENGTH(tag_data_blk[0]) + 1; i++)
			pr_cont("%02x", tag_data_blk[i]);
		rx_pr("\n");
	}
	/* if db not exist in edid, then add it to the end */
	splice_data_blk_to_edid(p_edid, tag_data_blk, 0xFF);
}

/* remove cta data blk which tag = tagid */
void edid_rm_db_by_tag(u8 *p_edid, u16 tagid)
{
	int tag_offset = 0;
	u8 tag_len = 0, cta_blk = 0;
	unsigned int i = 0;
	u16 end_pos = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;
	//remove first block
	ret = rx_get_cea_tag_offset(p_edid, tagid);
	tag_offset = ret.pos[0];
	cta_blk = tag_offset / EDID_BLK_SIZE;
	if (!tag_offset) {
		if (log_level & EDID_LOG)
			rx_pr("no this data blk in edid %d\n", tagid);
		return;
	}
	tag_len = BLK_LENGTH(p_edid[tag_offset]) + 1;
	end_pos = (1 + cta_blk) * EDID_BLK_SIZE - 1;
	for (i = tag_offset; i < end_pos - tag_len; i++)
		p_edid[i] = p_edid[i + tag_len];
	/* clear new free space to zero */
	memset(&p_edid[i], 0, tag_len);
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET * cta_blk + EDID_DESCRIP_OFFSET] -= tag_len;
	if (log_level & EDID_DATA_LOG) {
		rx_pr("++++edid data after rm db by %s:\n", rx_get_cta_blk_name(tagid));
		rx_pr("tag_offset:%d, tag_len:%d\n", tag_offset, tag_len);
		for (i = 128 * cta_blk; i < 128 * (cta_blk + 1); i++)
			pr_cont("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

/* param[blk_idx]: start from 0
 * the sequence index of the data blk to be removed
 */
void edid_rm_db_by_idx(u8 *p_edid, u8 blk_idx)
{
	struct cea_ext_parse_info *ext_blk_info = NULL;
	int tag_offset = 0;
	unsigned char tag_len = 0;
	unsigned int i = 0;

	if (!p_edid)
		return;
	ext_blk_info = kzalloc(sizeof(*ext_blk_info), GFP_KERNEL);
	if (!ext_blk_info)
		return;
	edid_parse_cea_ext_block(p_edid + EDID_EXT_BLK_OFF,
				 ext_blk_info);
	if (blk_idx >= ext_blk_info->blk_parse_info.data_blk_num) {
		rx_pr("no this index data blk in edid\n");
		kfree(ext_blk_info);
		return;
	}
	tag_offset = EDID_EXT_BLK_OFF +
		ext_blk_info->blk_parse_info.db_info[blk_idx].offset;
	tag_len =
		ext_blk_info->blk_parse_info.db_info[blk_idx].blk_len;
	/* move data behind the removed data block
	 * forward, except checksum & free size
	 */
	kfree(ext_blk_info);
	for (i = tag_offset; i < 255 - tag_len; i++)
		p_edid[i] = p_edid[i + tag_len];
	/* clear new free space to zero */
	memset(&p_edid[i + 1], 0, tag_len);
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tag_len;
	if (log_level & EDID_DATA_LOG) {
		rx_pr("++++edid data after rm db by idx:\n");
		for (i = 128; i < 256; i++)
			pr_cont("%02x", p_edid[i]);
		rx_pr("\n");
	}
}

#ifdef CONFIG_AMLOGIC_HDMITX
static void rpt_edid_extension_num_extraction(unsigned char *p_edid)
{
	u_int i = 0;
	u_int checksum = 0;

	if (!p_edid)
		return;
	if (log_level & EDID_LOG)
		rx_pr("extension_block_num = %d\n", p_edid[126]);
	/* currently we only handle 2 EDID blocks, ignore > 2 blocks */
	if (p_edid[0x7e] > 1) {
		p_edid[0x7e] = 1;
		for (i = 0; i <= 0x7F; i++) {
			if (i < 0x7F) {
				checksum += p_edid[i];
				checksum &= 0xFF;
			} else {
				checksum = (0x100 - checksum) & 0xFF;
			}
		}
		p_edid[0x7f] = checksum & 0xFF;
	}
}

static void rpt_edid_rm_hf_eeodb(unsigned char *p_edid)
{
	/* HF-EEODB occupy byte 4~6 of block1, fixed location */
	edid_rm_db_by_tag(p_edid, EXTENDED_HF_EEODB);
}

void rpt_edid_hf_vs_db_extraction(unsigned char *p_edid)
{
	u8 hf_vsdb_start = 0;
	u8 tx_hf_vsdb_start = 0;
	struct hf_vsdb_s hf_vsdb_tx = {0}, hf_vsdb_def = {0}, hf_vsdb_dest = {0};
	u8 tag_len_tx = 0, tag_len_def = 0, i = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, HF_VENDOR_DB_TAG);
	hf_vsdb_start = ret.pos[0];
	if (!hf_vsdb_start) {
		rx_pr("unsupported hf-vsdb!!!\n");
		return;
	}
	tag_len_def = p_edid[hf_vsdb_start] & 0x1f;
	memcpy(&hf_vsdb_def, p_edid + hf_vsdb_start, tag_len_def + 1);

	ret = rx_get_cea_tag_offset(edid_tx, HF_VENDOR_DB_TAG);
	tx_hf_vsdb_start = ret.pos[0];
	if (!tx_hf_vsdb_start) {
		if (log_level & EDID_LOG)
			rx_pr("no hf-vsdb\n");
		edid_rm_db_by_tag(p_edid, HF_VENDOR_DB_TAG);
		return;
	}
	tag_len_tx = edid_tx[tx_hf_vsdb_start] & 0x1f;
	memcpy(&hf_vsdb_tx, edid_tx + tx_hf_vsdb_start, tag_len_tx + 1);

	hf_vsdb_dest.tag = hf_vsdb_tx.tag;
	hf_vsdb_dest.hf_db.version = hf_vsdb_tx.hf_db.version;
	hf_vsdb_dest.len = tag_len_tx > tag_len_def ? tag_len_def : tag_len_tx;
	if (hf_vsdb_dest.len < 7)
		rx_pr("hfvsdb size err\n");
	hf_vsdb_dest.ieee_first = hf_vsdb_tx.ieee_first;
	hf_vsdb_dest.ieee_second = hf_vsdb_tx.ieee_second;
	hf_vsdb_dest.ieee_third = hf_vsdb_tx.ieee_third;
	if (hf_vsdb_def.hf_db.max_tmds_rate > hf_vsdb_tx.hf_db.max_tmds_rate)
		hf_vsdb_dest.hf_db.max_tmds_rate = hf_vsdb_tx.hf_db.max_tmds_rate;
	else
		hf_vsdb_dest.hf_db.max_tmds_rate = hf_vsdb_def.hf_db.max_tmds_rate;

	hf_vsdb_dest.hf_db.scdc_present =
		hf_vsdb_tx.hf_db.scdc_present & hf_vsdb_def.hf_db.scdc_present;
	hf_vsdb_dest.hf_db.rr_cap = hf_vsdb_tx.hf_db.rr_cap & hf_vsdb_def.hf_db.rr_cap;
	hf_vsdb_dest.hf_db.cable_status =
		hf_vsdb_tx.hf_db.cable_status & hf_vsdb_def.hf_db.cable_status;
	hf_vsdb_dest.hf_db.ccbpci = hf_vsdb_tx.hf_db.ccbpci & hf_vsdb_def.hf_db.ccbpci;
	hf_vsdb_dest.hf_db.lte_340m_scramble =
		hf_vsdb_tx.hf_db.lte_340m_scramble & hf_vsdb_def.hf_db.lte_340m_scramble;
	hf_vsdb_dest.hf_db.independ_view =
		hf_vsdb_tx.hf_db.independ_view & hf_vsdb_def.hf_db.independ_view;
	hf_vsdb_dest.hf_db.dual_view =
		hf_vsdb_tx.hf_db.dual_view & hf_vsdb_def.hf_db.dual_view;
	hf_vsdb_dest.hf_db._3d_osd_disparity =
		hf_vsdb_tx.hf_db._3d_osd_disparity & hf_vsdb_def.hf_db._3d_osd_disparity;
	if (hf_vsdb_def.hf_db.max_frl_rate > hf_vsdb_tx.hf_db.max_frl_rate)
		hf_vsdb_dest.hf_db.max_frl_rate = hf_vsdb_tx.hf_db.max_frl_rate;
	else
		hf_vsdb_dest.hf_db.max_frl_rate = hf_vsdb_def.hf_db.max_frl_rate;
	hf_vsdb_dest.hf_db.uhd_vic =
		hf_vsdb_tx.hf_db.uhd_vic & hf_vsdb_def.hf_db.uhd_vic;
	hf_vsdb_dest.hf_db.dc_48bit_420 =
		hf_vsdb_tx.hf_db.dc_48bit_420 & hf_vsdb_def.hf_db.dc_48bit_420;
	hf_vsdb_dest.hf_db.dc_36bit_420 =
		hf_vsdb_tx.hf_db.dc_36bit_420 & hf_vsdb_def.hf_db.dc_36bit_420;
	hf_vsdb_dest.hf_db.dc_30bit_420 =
		hf_vsdb_tx.hf_db.dc_30bit_420 & hf_vsdb_def.hf_db.dc_30bit_420;
	if (!vshf_dc_30_36_420_support) {
		hf_vsdb_dest.hf_db.dc_36bit_420 = 0;
		hf_vsdb_dest.hf_db.dc_30bit_420 = 0;
		vshf_dc_30_36_420_support = true;
	}
	hf_vsdb_dest.hf_db.qms_tfr_max =
		hf_vsdb_tx.hf_db.qms_tfr_max & hf_vsdb_def.hf_db.qms_tfr_max;
	hf_vsdb_dest.hf_db.qms = hf_vsdb_tx.hf_db.qms & hf_vsdb_def.hf_db.qms;
	hf_vsdb_dest.hf_db.m_delta =
		hf_vsdb_tx.hf_db.m_delta & hf_vsdb_def.hf_db.m_delta;
	hf_vsdb_dest.hf_db.qms_tfr_min =
		hf_vsdb_tx.hf_db.qms_tfr_min & hf_vsdb_def.hf_db.qms_tfr_min;
	hf_vsdb_dest.hf_db.neg_mvrr =
		hf_vsdb_tx.hf_db.neg_mvrr & hf_vsdb_def.hf_db.neg_mvrr;
	hf_vsdb_dest.hf_db.fva = hf_vsdb_tx.hf_db.fva & hf_vsdb_def.hf_db.fva;
	hf_vsdb_dest.hf_db.allm = hf_vsdb_tx.hf_db.allm & hf_vsdb_def.hf_db.allm;
	hf_vsdb_dest.hf_db.fapa_start_location =
		hf_vsdb_tx.hf_db.fapa_start_location & hf_vsdb_def.hf_db.fapa_start_location;
	if (hf_vsdb_dest.len < 10)
		goto _end;
	if (hf_vsdb_def.hf_db.vrr_min > hf_vsdb_tx.hf_db.vrr_min)
		hf_vsdb_dest.hf_db.vrr_min = hf_vsdb_tx.hf_db.vrr_min;
	else
		hf_vsdb_dest.hf_db.vrr_min = hf_vsdb_def.hf_db.vrr_min;
	if (((hf_vsdb_def.hf_db.vrr_max_lo + hf_vsdb_def.hf_db.vrr_max_hi) << 8) >
		((hf_vsdb_tx.hf_db.vrr_max_lo + hf_vsdb_tx.hf_db.vrr_max_hi) << 8)) {
		hf_vsdb_dest.hf_db.vrr_max_lo = hf_vsdb_tx.hf_db.vrr_max_lo;
		hf_vsdb_dest.hf_db.vrr_max_hi = hf_vsdb_tx.hf_db.vrr_max_hi;
	} else {
		hf_vsdb_dest.hf_db.vrr_max_lo = hf_vsdb_def.hf_db.vrr_max_lo;
		hf_vsdb_dest.hf_db.vrr_max_hi = hf_vsdb_def.hf_db.vrr_max_hi;
	}
	if (hf_vsdb_dest.len < 13)
		goto _end;
	hf_vsdb_dest.hf_db.dsc_1p2 =
		hf_vsdb_tx.hf_db.dsc_1p2 & hf_vsdb_def.hf_db.dsc_1p2;
	hf_vsdb_dest.hf_db.dsc_native_420 =
		hf_vsdb_tx.hf_db.dsc_native_420 & hf_vsdb_def.hf_db.dsc_native_420;
	hf_vsdb_dest.hf_db.dsc_all_bpp =
		hf_vsdb_tx.hf_db.dsc_all_bpp & hf_vsdb_def.hf_db.dsc_all_bpp;
	hf_vsdb_dest.hf_db.dsc_16bpc =
		hf_vsdb_tx.hf_db.dsc_16bpc & hf_vsdb_def.hf_db.dsc_16bpc;
	hf_vsdb_dest.hf_db.dsc_12bpc =
		hf_vsdb_tx.hf_db.dsc_12bpc & hf_vsdb_def.hf_db.dsc_12bpc;
	hf_vsdb_dest.hf_db.dsc_10bpc =
		hf_vsdb_tx.hf_db.dsc_10bpc & hf_vsdb_def.hf_db.dsc_10bpc;

	if (hf_vsdb_def.hf_db.dsc_max_frl_rate > hf_vsdb_tx.hf_db.dsc_max_frl_rate)
		hf_vsdb_dest.hf_db.dsc_max_frl_rate = hf_vsdb_tx.hf_db.dsc_max_frl_rate;
	else
		hf_vsdb_dest.hf_db.dsc_max_frl_rate = hf_vsdb_def.hf_db.dsc_max_frl_rate;
	if (hf_vsdb_def.hf_db.dsc_max_slices > hf_vsdb_tx.hf_db.dsc_max_slices)
		hf_vsdb_dest.hf_db.dsc_max_slices = hf_vsdb_tx.hf_db.dsc_max_slices;
	else
		hf_vsdb_dest.hf_db.dsc_max_slices = hf_vsdb_def.hf_db.dsc_max_slices;

	if (hf_vsdb_def.hf_db.dsc_total_chunk_kbytes >
			hf_vsdb_tx.hf_db.dsc_total_chunk_kbytes)
		hf_vsdb_dest.hf_db.dsc_total_chunk_kbytes =
			hf_vsdb_tx.hf_db.dsc_total_chunk_kbytes;
	else
		hf_vsdb_dest.hf_db.dsc_total_chunk_kbytes =
			hf_vsdb_def.hf_db.dsc_total_chunk_kbytes;

	hf_vsdb_dest.hf_db.fapa_end_extended =
		hf_vsdb_tx.hf_db.fapa_end_extended & hf_vsdb_def.hf_db.fapa_end_extended;

_end:
	//p_edid[hf_vsdb_start] = 0x60 + hf_vsdb_dest.len;
	memcpy(p_edid + hf_vsdb_start, &hf_vsdb_dest, hf_vsdb_dest.len + 1);
	i = hf_vsdb_start + hf_vsdb_dest.len + 1;
	for (; i < 255 - tag_len_def + hf_vsdb_dest.len; i++)
		p_edid[i] = p_edid[i + tag_len_def - hf_vsdb_dest.len];
	for (; i < 255; i++)
		p_edid[i] = 0;
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tag_len_def - hf_vsdb_dest.len;
}

//hdmi repeater cert phyaddr test rule
//sink phyaddr  --- source phyaddr(Repeater_PA_Increment)
//	1.0.0.0   --- 1.M.0.0
//	2.0.0.0   --- 2.M.0.0
//	2.3.0.0   --- 2.3.M.0
//	3.4.5.0   --- 3.4.5.M
//	1.1.1.1   --- F.F.F.F
//	F.F.F.F   --- F.F.F.F
static void edid_secondaryphyaddr(unsigned char *p_edid_dest, unsigned char *p_edid_src)
{
	u8 src_vsdb_offset = 0;
	u8 def_vsdb_offset = 0;
	struct vsdb_s vsdb_src = {0}, vsdb_def = {0}, vsdb_dest = {0};
	u8 *p_vsdb_src = (u8 *)&vsdb_src;
	u8 *p_vsdb_def = (u8 *)&vsdb_def;
	struct data_block_location_s ret = {0};

	ret = rx_get_cea_tag_offset(p_edid_src, VENDOR_TAG);
	src_vsdb_offset = ret.pos[0];
	if (!src_vsdb_offset)
		rx_pr("src_vsdb_offset error\n");
	ret = rx_get_cea_tag_offset(p_edid_dest, VENDOR_TAG);
	def_vsdb_offset = ret.pos[0];
	if (!def_vsdb_offset)
		rx_pr("def_vsdb_offset error\n");
	memcpy(p_vsdb_src + 4, &p_edid_src[src_vsdb_offset + 4], 2);
	memcpy(p_vsdb_def + 4, &p_edid_dest[def_vsdb_offset + 4], 2);
	if (vsdb_src.a == 0) {
		vsdb_dest.b = vsdb_def.b;
		vsdb_dest.a = vsdb_def.a;
		vsdb_dest.d = vsdb_def.d;
		vsdb_dest.c = vsdb_def.c;
	} else if (vsdb_src.b == 0) {
		vsdb_dest.b = vsdb_def.a;
		vsdb_dest.a = vsdb_src.a;
		vsdb_dest.d = 0;
		vsdb_dest.c = 0;
	} else if (vsdb_src.c == 0) {
		vsdb_dest.b = vsdb_src.b;
		vsdb_dest.a = vsdb_src.a;
		vsdb_dest.d = 0;
		vsdb_dest.c = vsdb_def.a;
	} else if (vsdb_src.d == 0) {
		vsdb_dest.b = vsdb_src.b;
		vsdb_dest.a = vsdb_src.a;
		vsdb_dest.d = vsdb_def.a;
		vsdb_dest.c = vsdb_src.c;
	} else {
		vsdb_dest.b = 0xf;
		vsdb_dest.a = 0xf;
		vsdb_dest.d = 0xf;
		vsdb_dest.c = 0xf;
	}
	p_vsdb_def = (u8 *)&vsdb_dest;
	memcpy(&p_edid_dest[def_vsdb_offset + 4], p_vsdb_def + 4, 2);
	if (log_level & EDID_LOG)
		rx_pr("dest addr:0x%x 0x%x",
			p_edid_dest[def_vsdb_offset + 4], p_edid_dest[def_vsdb_offset + 5]);
}

void rpt_edid_14_vs_db_extraction(unsigned char *p_edid)
{
	u8 vsdb_start = 0;
	u8 tx_vsdb_start = 0;
	struct vsdb_s vsdb_tx = {0}, vsdb_def = {0}, vsdb_dest = {0};
	u8 *p_vsdb_dest = (u8 *)&vsdb_dest;
	u8 tag_len_tx = 0, tag_len_def = 0, i = 0, j = 0, k = 0;
	u8 tx_hdmi_vic[4] = {0};
	u8 def_hdmi_vic[4] = {0};
	struct data_block_location_s ret = {0};

	memset(&vsdb_tx, 0, sizeof(struct vsdb_s));
	memset(&vsdb_def, 0, sizeof(struct vsdb_s));
	memset(&vsdb_dest, 0, sizeof(struct vsdb_s));
	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, VENDOR_TAG);
	vsdb_start = ret.pos[0];
	if (!vsdb_start) {
		//abnormal condition, only for errordebug
		rx_pr("unsupported vsdb!!\n");
		return;
	}
	tag_len_def = p_edid[vsdb_start] & 0x1f;
	memcpy(&vsdb_def, p_edid + vsdb_start, tag_len_def + 1);
	//Default edid parsing
	i = 8;
	if (tag_len_def <= i)
		goto _end1;
	if (vsdb_def.latency_fields_present) {
		i++;
		vsdb_def.video_latency = p_edid[vsdb_start + i];
		i++;
		vsdb_def.video_latency = p_edid[vsdb_start + i];
	}
	if (vsdb_def.i_latency_fields_present) {
		i++;
		vsdb_def.interlaced_video_latency = p_edid[vsdb_start + i];
		i++;
		vsdb_def.interlaced_audio_latency = p_edid[vsdb_start + i];
	}
	if (tag_len_def <= i)
		goto _end1;
	if (vsdb_def.hdmi_video_present) {
		i++;
		vsdb_def.image_size = (p_edid[vsdb_start + i] >> 3) & 3;
		vsdb_def._3d_multi_present = (p_edid[vsdb_start + i] >> 5) & 3;
		vsdb_def._3d_present = (p_edid[vsdb_start + i] >> 7) & 1;
		i++;
		vsdb_def.hdmi_3d_len = p_edid[vsdb_start + i] & 0x1f;
		vsdb_def.hdmi_vic_len = (p_edid[vsdb_start + i] >> 5) & 0x07;
	}
	if (vsdb_def.hdmi_vic_len) {
		i++;
		for (j = 0; j < vsdb_def.hdmi_vic_len; j++)
			def_hdmi_vic[j] = (p_edid[vsdb_start + i + j]);
	}
	if (vsdb_def.hdmi_3d_len) {
		//todo
		//unsupport 3d format by default
	}
_end1:
	ret = rx_get_cea_tag_offset(edid_tx, VENDOR_TAG);
	tx_vsdb_start = ret.pos[0];
	if (!tx_vsdb_start) {
		if (log_level & EDID_LOG)
			rx_pr("no vsdb\n");
		edid_rm_db_by_tag(p_edid, VENDOR_TAG);
		return;
	}
	tag_len_tx = edid_tx[tx_vsdb_start] & 0x1f;
	memcpy(&vsdb_tx, edid_tx + tx_vsdb_start, tag_len_tx + 1);
	//TX edid parsing
	i = 5;
	if (tag_len_tx < i) {
		if (log_level & EDID_LOG)
			rx_pr("vsdb length error\n");
		return;
	}
	if (tag_len_tx == i) {
		if (log_level & EDID_LOG)
			rx_pr("vsdb only physcal address info\n");
		edid_secondaryphyaddr(p_edid, edid_tx);
		return;
	}
	i = 8;
	if (tag_len_tx <= i)
		goto _end2;
	if (vsdb_tx.latency_fields_present) {
		i++;
		vsdb_tx.video_latency = edid_tx[tx_vsdb_start + i];
		i++;
		vsdb_tx.audio_latency = edid_tx[tx_vsdb_start + i];
	}
	if (vsdb_tx.i_latency_fields_present) {
		i++;
		vsdb_tx.interlaced_video_latency = edid_tx[tx_vsdb_start + i];
		i++;
		vsdb_tx.interlaced_audio_latency = edid_tx[tx_vsdb_start + i];
	}
	if (tag_len_tx <= i)
		goto _end2;
	if (vsdb_tx.hdmi_video_present) {
		i++;
		vsdb_tx.image_size = (edid_tx[tx_vsdb_start + i] >> 3) & 3;
		vsdb_tx._3d_multi_present = (edid_tx[tx_vsdb_start + i] >> 5) & 3;
		vsdb_tx._3d_present = (edid_tx[tx_vsdb_start + i] >> 7) & 1;
		i++;
		vsdb_tx.hdmi_3d_len = edid_tx[tx_vsdb_start + i] & 0x1f;
		vsdb_tx.hdmi_vic_len = (edid_tx[tx_vsdb_start + i] >> 5) & 0x07;
	}
	if (vsdb_tx.hdmi_vic_len) {
		i++;
		for (j = 0; j < vsdb_tx.hdmi_vic_len; j++)
			tx_hdmi_vic[j] = (edid_tx[tx_vsdb_start + i + j]);
	}
	//if (vsdb_tx.hdmi_3d_len)
		//todo
_end2:
	vsdb_dest.len = tag_len_tx > tag_len_def ? tag_len_def : tag_len_tx;
	if (log_level & EDID_LOG)
		rx_pr("len=%d-%d-%d\n", vsdb_dest.len, tag_len_tx, tag_len_def);
	vsdb_dest.tag = vsdb_tx.tag;
	vsdb_dest.ieee_first = vsdb_tx.ieee_first;
	vsdb_dest.ieee_second = vsdb_tx.ieee_second;
	vsdb_dest.ieee_third = vsdb_tx.ieee_third;
	edid_secondaryphyaddr(p_edid, edid_tx);
	memcpy(p_vsdb_dest + 4, &p_edid[vsdb_start + 4], 2);
	if (vsdb_dest.len <= 5)
		return;
	vsdb_dest.dvi_dual = vsdb_tx.dvi_dual & vsdb_def.dvi_dual;
	vsdb_dest.dc_y444 = vsdb_tx.dc_y444 & vsdb_def.dc_y444;
	vsdb_dest.dc_30bit = vsdb_tx.dc_30bit & vsdb_def.dc_30bit;
	vsdb_dest.dc_36bit = vsdb_tx.dc_36bit & vsdb_def.dc_36bit;
	vsdb_dest.dc_48bit = vsdb_tx.dc_48bit & vsdb_def.dc_48bit;
	vsdb_dest.support_ai = vsdb_tx.support_ai & vsdb_def.support_ai;
	if (vsdb_dest.len <= 6)
		goto _end3;
	if (vsdb_def.max_tmds_clk > vsdb_tx.max_tmds_clk)
		vsdb_dest.max_tmds_clk = vsdb_tx.max_tmds_clk;
	else
		vsdb_dest.max_tmds_clk = vsdb_def.max_tmds_clk;
	//pb5
	if (vsdb_dest.len <= 7)
		goto _end3;
	vsdb_dest.cnc0 = vsdb_tx.cnc0 & vsdb_def.cnc0;
	vsdb_dest.cnc1 = vsdb_tx.cnc1 & vsdb_def.cnc1;
	vsdb_dest.cnc2 = vsdb_tx.cnc2 & vsdb_def.cnc2;
	vsdb_dest.cnc3 = vsdb_tx.cnc3 & vsdb_def.cnc3;
	vsdb_dest.hdmi_video_present =
		vsdb_tx.hdmi_video_present & vsdb_def.hdmi_video_present;
	vsdb_dest.i_latency_fields_present =
		vsdb_tx.i_latency_fields_present & vsdb_def.i_latency_fields_present;
	vsdb_dest.latency_fields_present =
		vsdb_tx.latency_fields_present & vsdb_def.latency_fields_present;
	if (vsdb_dest.len <= 8)
		goto _end3;
	//pb6
	if (vsdb_dest.latency_fields_present) {
		if (LATENCY_MAX - vsdb_tx.video_latency > vsdb_def.video_latency)
			vsdb_dest.video_latency = vsdb_tx.video_latency + vsdb_def.video_latency;
		else
			vsdb_dest.video_latency = LATENCY_MAX;
		if (LATENCY_MAX - vsdb_tx.audio_latency > vsdb_def.audio_latency)
			vsdb_dest.audio_latency = vsdb_tx.audio_latency + vsdb_def.audio_latency;
		else
			vsdb_dest.audio_latency = LATENCY_MAX;
	}
	if (vsdb_dest.i_latency_fields_present) {
		if (LATENCY_MAX - vsdb_tx.interlaced_video_latency >
			vsdb_def.interlaced_video_latency)
			vsdb_dest.interlaced_video_latency =
				vsdb_tx.interlaced_video_latency +
				vsdb_def.interlaced_video_latency;
		else
			vsdb_dest.interlaced_video_latency = LATENCY_MAX;
		if (LATENCY_MAX - vsdb_tx.interlaced_audio_latency >
			vsdb_def.interlaced_audio_latency)
			vsdb_dest.interlaced_audio_latency =
			vsdb_tx.interlaced_audio_latency + vsdb_def.interlaced_audio_latency;
		else
			vsdb_dest.interlaced_audio_latency = LATENCY_MAX;
	}
	if (vsdb_dest.hdmi_video_present) {
		if (vsdb_def.image_size > vsdb_tx.image_size)
			vsdb_dest.image_size = vsdb_tx.image_size;
		else
			vsdb_dest.image_size = vsdb_def.image_size;
	}
	if (vsdb_def.hdmi_vic_len >= vsdb_tx.hdmi_vic_len)
		vsdb_dest.hdmi_vic_len = vsdb_tx.hdmi_vic_len;
	else
		vsdb_dest.hdmi_vic_len = vsdb_def.hdmi_vic_len;
_end3:
	i = 5;
	//memcpy(p_edid + vsdb_start, &vsdb_dest, i + 1);
	if (vsdb_dest.dvi_dual ||
		vsdb_dest.dc_y444 ||
		vsdb_dest.dc_30bit ||
		vsdb_dest.dc_36bit ||
		vsdb_dest.dc_48bit ||
		vsdb_dest.support_ai)
		i = 6;
	if (vsdb_dest.max_tmds_clk)
		i = 7;
	memcpy(p_edid + vsdb_start, &vsdb_dest, i + 1);
	if (vsdb_dest.cnc0 || vsdb_dest.cnc1 || vsdb_dest.cnc2 || vsdb_dest.cnc3 ||
		vsdb_dest.hdmi_video_present ||
		vsdb_dest.i_latency_fields_present ||
		vsdb_dest.latency_fields_present) {
		i++;
		p_edid[vsdb_start + i] =
			vsdb_dest.cnc0 +
			(vsdb_dest.cnc1 << 1) +
			(vsdb_dest.cnc2 << 2) +
			(vsdb_dest.cnc3 << 3) +
			(vsdb_dest.hdmi_video_present << 5) +
			(vsdb_dest.i_latency_fields_present << 6) +
			(vsdb_dest.latency_fields_present << 7);
		//memcpy(p_edid + vsdb_start + i, &vsdb_dest + i, 1);
	}
	if (vsdb_dest.latency_fields_present) {
		i++;
		p_edid[vsdb_start + i] = vsdb_dest.video_latency;
		i++;
		p_edid[vsdb_start + i] = vsdb_dest.audio_latency;
	}
	if (vsdb_dest.i_latency_fields_present) {
		i++;
		p_edid[vsdb_start + i] = vsdb_dest.interlaced_video_latency;
		i++;
		p_edid[vsdb_start + i] = vsdb_dest.interlaced_audio_latency;
	}
	if (vsdb_dest.hdmi_video_present) {
		i++;
		p_edid[vsdb_start + i] = (vsdb_dest.image_size << 3) +
								(vsdb_dest._3d_multi_present << 5) +
								(vsdb_dest._3d_present << 7);
		i++;
		if (vsdb_dest.hdmi_vic_len) {
			vsdb_dest.hdmi_vic_len = 0;
			for (j = 0; j < vsdb_tx.hdmi_vic_len; j++) {
				for (k = 0; k < vsdb_def.hdmi_vic_len; k++) {
					if (tx_hdmi_vic[j] == def_hdmi_vic[k]) {
						i++;
						vsdb_dest.hdmi_vic_len++;
						p_edid[vsdb_start + i] = tx_hdmi_vic[j];
						break;
					}
				}
			}
			p_edid[vsdb_start + i - vsdb_dest.hdmi_vic_len] =
				(vsdb_dest.hdmi_vic_len << 5) +
				vsdb_dest.hdmi_3d_len;
		}
		//3D related/ todo
	}
	if (i != vsdb_dest.len) {
		if (log_level & EDID_LOG)
			rx_pr("vsdb length warning-%d-%d\n",
				  i, vsdb_dest.len);
		vsdb_dest.len = i > vsdb_dest.len ? vsdb_dest.len : i;
	}
	p_edid[vsdb_start] = (VENDOR_TAG << 5) + vsdb_dest.len;
	for (i = vsdb_start + vsdb_dest.len + 1; i < 255 - tag_len_def + vsdb_dest.len; i++)
		p_edid[i] = p_edid[i + tag_len_def - vsdb_dest.len];
	for (; i < 255; i++)
		p_edid[i] = 0;
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tag_len_def - vsdb_dest.len;
}

void rpt_edid_video_db_extraction(unsigned char *p_dest_edid, unsigned char *p_source_edid)
{
	u8 vdb_start = 0;
	u8 tx_vdb_start = 0;
	u8 vdb_dest[31] = {0};
	u8 tx_native_list[31] = {0};
	u8 tag_len_tx = 0, tag_len_def = 0;
	u8 i, j, tag_len_dest = 0;
	u8 native_cnt = 0;
	struct data_block_location_s ret = {0};

	if (!p_dest_edid)
		return;

	ret = rx_get_cea_tag_offset(p_dest_edid, VIDEO_TAG);
	vdb_start = ret.pos[0];
	if (!vdb_start) {
		//abnormal condition, only for errordebug
		rx_pr("unsupported vdb!!\n");
		return;
	}
	tag_len_def = p_dest_edid[vdb_start] & 0x1f;
	memset(def_vic_list, 0, 31);
	memcpy(def_vic_list, p_dest_edid + vdb_start + 1, tag_len_def);
	for (i = 0; i < tag_len_def; i++) {
		if (def_vic_list[i] >= 129 && def_vic_list[i] <= 192)
			def_vic_list[i] = def_vic_list[i] & 0x7f;
	}
	ret = rx_get_cea_tag_offset(p_source_edid, VIDEO_TAG);
	tx_vdb_start = ret.pos[0];
	if (!tx_vdb_start) {
		if (log_level & EDID_LOG)
			rx_pr("no vdb\n");
		return;
	}
	tag_len_tx = p_source_edid[tx_vdb_start] & 0x1f;
	memset(tx_vic_list, 0, 31);
	memcpy(tx_vic_list, p_source_edid + tx_vdb_start + 1, tag_len_tx);
	for (i = 0; i < tag_len_tx; i++) {
		if (tx_vic_list[i] >= 129 && tx_vic_list[i] <= 192) {
			tx_vic_list[i] = tx_vic_list[i] & 0x7f;
			tx_native_list[native_cnt] = tx_vic_list[i];
			native_cnt++;
		}
	}
	for (j = 0; j < tag_len_tx; j++) {
		for (i = 0; i < tag_len_def; i++) {
			if (tx_vic_list[j] == def_vic_list[i]) {
				vdb_dest[tag_len_dest] = tx_vic_list[j];
				tag_len_dest++;
				break;
			}
		}
	}
	for (j = 0; j < tag_len_dest; j++) {
		for (i = 0; i < native_cnt; i++) {
			if (vdb_dest[j] == tx_native_list[i])
				vdb_dest[j] = vdb_dest[j] | 0x80;
		}
	}
	if (log_level & EDID_LOG)
		rx_pr("vdb_size = %d\n", tag_len_dest);
	p_dest_edid[vdb_start] = (VIDEO_TAG << 5) + tag_len_dest;
	memcpy(p_dest_edid + vdb_start + 1, vdb_dest, tag_len_dest);
	for (i = vdb_start + tag_len_dest + 1; i < 255 - tag_len_def + tag_len_dest; i++)
		p_dest_edid[i] = p_dest_edid[i + tag_len_def - tag_len_dest];
	for (; i < 255; i++)
		p_dest_edid[i] = 0;
	/* dtd offset modify */
	p_dest_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tag_len_def - tag_len_dest;
}

//this function implement single or multi vdb extraction
//(9-3)hdmi repeater cert center use multi vdb ref edid to test block1 VDB
void rpt_edid_multi_vdb_extraction(unsigned char *p_dest_edid, unsigned char *p_source_edid)
{
	u8 vdb_start = 0;
	u8 i = 0;
	u8 p_dest_tmp[EDID_SIZE] = {0};
	u8 p_source_tmp[EDID_SIZE] = {0};
	u8 vdb_dest[128] = {0};
	u8 vdb_size = 0;
	struct data_block_location_s ret = {0};

	memcpy(p_source_tmp, p_source_edid, EDID_SIZE);
	while (edid_tag_extract(p_source_tmp, VIDEO_TAG)) {
		i++;
		memcpy(p_dest_tmp, p_dest_edid, EDID_SIZE);
		rpt_edid_video_db_extraction(p_dest_tmp, p_source_tmp);
		//remove vdb after extract once
		edid_rm_db_by_tag(p_source_tmp, VIDEO_TAG);
		if (!edid_tag_extract(p_source_tmp, VIDEO_TAG) && i == 1) {
			ret = rx_get_cea_tag_offset(p_dest_tmp, VIDEO_TAG);
			vdb_start = ret.pos[0];
			vdb_size = BLK_LENGTH(p_dest_tmp[vdb_start]);
			memcpy(&vdb_dest[1], &p_dest_tmp[vdb_start + 1], vdb_size);
//hdmi repeater cert item(9-3 basic format support requirements), test block0 dtd timing
//modify suggestion from cert center:
//1.if there is no intersection between dtd resolution,
//can fill in the intersection resolution of VDB
//2.we could fill in VIC1(640*480) to dtd,all rx product should support it.
//we choose solution 2 to pass this item, when VDB not include VIC1 after VDB extraction,
//it should fill in VIC1 to VDB for match block0 dtd timing(VIC1).
//if can not pass this item, it should modify block0 dtd from rx edid.bin
			for (i = 0; i < vdb_size; i++) {
				if (vdb_dest[1 + i] == HDMI_640x480p60)
					break;
			}
			if (vdb_size == i) {
				vdb_size += 1;
				vdb_dest[0] = VIDEO_TAG << 5 | vdb_size;
				vdb_dest[vdb_size] = HDMI_640x480p60;//add VIC1
				splice_data_blk_to_edid(p_dest_tmp, &vdb_dest[0], VIDEO_TAG);
			}
			memcpy(p_dest_edid, p_dest_tmp, EDID_SIZE);
			return;
		}
		//capture all vdb block
		ret = rx_get_cea_tag_offset(p_dest_tmp, VIDEO_TAG);
		vdb_start = ret.pos[0];
		memcpy(&vdb_dest[1 + vdb_size], &p_dest_tmp[vdb_start + 1],
			BLK_LENGTH(p_dest_tmp[vdb_start]));
		vdb_size += BLK_LENGTH(p_dest_tmp[vdb_start]);
		if (log_level & EDID_LOG)
			rx_pr("found multi vdb:%d start:0x%x blk_length:%d\n", i, vdb_start,
				BLK_LENGTH(p_dest_tmp[vdb_start]));
	}
	vdb_dest[0] = VIDEO_TAG << 5 | vdb_size;
//hdmi repeater cert item(9-3 basic format support requirements), test block0 dtd timing
//modify suggestion from cert center:
//1.if there is no intersection between dtd resolution,
//can fill in the intersection resolution of VDB
//2.we could fill in VIC1(640*480) to dtd,all rx product should support it.
//we choose solution 2 to pass this item, when VDB not include VIC1 after VDB extraction,
//it should fill in VIC1 to VDB for match block0 dtd timing(VIC1).
//if can not pass this item, it should modify block0 dtd from rx edid.bin
	for (i = 0; i < vdb_size; i++) {
		if (vdb_dest[1 + i] == HDMI_640x480p60)
			break;
	}
	if (vdb_size == i) {
		vdb_size += 1;
		vdb_dest[0] = VIDEO_TAG << 5 | vdb_size;
		vdb_dest[vdb_size] = HDMI_640x480p60;//add VIC1
	}
	splice_data_blk_to_edid(p_dest_edid, &vdb_dest[0], VIDEO_TAG);
}

void rpt_edid_audio_db_extraction(unsigned char *p_edid)
{
	u8 adb_start = 0;
	u8 tx_adb_start = 0;
	struct edid_audio_block_t adb_tx[10] = {0}, adb_def[10] = {0}, adb_dest[10] = {0};
	u8 tag_len_tx = 0, tag_len_def = 0;
	u8 i = 0, j = 0, tag_len_dest = 0;
	//some device such as arc, it's edid include several same audio format
	//can not judge which format is the best format when extract rx edid
	//when found tx edid has duplicat audio format,just use rx audio format
	int tx_duplicate_audio_format = 0;
	int tx_audio_format = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, AUDIO_TAG);
	adb_start = ret.pos[0];
	if (!adb_start) {
		//abnormal condition, only for errordebug
		rx_pr("unsupported adb!!\n");
		return;
	}
	tag_len_def = p_edid[adb_start] & 0x1f;
	if (tag_len_def % 3) {
		if (log_level & EDID_LOG)
			rx_pr("aud db err\n");
		return;
	}
	memcpy((u8 *)adb_def, p_edid + adb_start + 1, tag_len_def);
	ret = rx_get_cea_tag_offset(edid_tx, AUDIO_TAG);
	tx_adb_start = ret.pos[0];
	if (!tx_adb_start) {
		if (log_level & EDID_LOG)
			rx_pr("no adb\n");
		return;
	}
	tag_len_tx = edid_tx[tx_adb_start] & 0x1f;
	if (tag_len_tx % 3) {
		if (log_level & EDID_LOG)
			rx_pr("aud db err\n");
		return;
	}
	memcpy((u8 *)adb_tx, edid_tx + tx_adb_start + 1, tag_len_tx);
	for (i = 0; i < tag_len_def / 3; i++) {
		for (j = 0; j < tag_len_tx / 3; j++) {
			if (adb_tx[j].format_code == adb_def[i].format_code) {
				if (tx_audio_format & (1 << adb_tx[j].format_code)) {
					if (log_level & EDID_LOG)
						rx_pr("skip same audio format %d",
							adb_tx[j].format_code);
					tx_duplicate_audio_format |= (1 << adb_tx[j].format_code);
					break;
				}
				adb_dest[tag_len_dest / 3].format_code = adb_tx[j].format_code;
				tx_audio_format |= (1 << adb_tx[j].format_code);
				//max channel
				if (adb_def[i].max_channel >
						adb_tx[j].max_channel)
					adb_dest[tag_len_dest / 3].max_channel =
						adb_tx[j].max_channel;
				else
					adb_dest[tag_len_dest / 3].max_channel =
						adb_def[i].max_channel;

				//frequency list
				adb_dest[tag_len_dest / 3].usr.freq_list =
					adb_tx[j].usr.freq_list &
					adb_def[i].usr.freq_list;
				//PCM bit rate
				//2-8 max bit rate
				//9-13 dependent value
				//14~ todo
				if (adb_dest[tag_len_dest / 3].format_code == AUDIO_FORMAT_LPCM) {
					adb_dest[tag_len_dest / 3].bit_rate.others =
						adb_tx[j].bit_rate.others &
						adb_def[i].bit_rate.others;
				} else if ((adb_dest[tag_len_dest / 3].format_code >=
						AUDIO_FORMAT_AC3) &&
						(adb_dest[tag_len_dest / 3].format_code <=
						AUDIO_FORMAT_ATRAC)) {
					if (adb_tx[j].bit_rate.others >
							adb_def[i].bit_rate.others)
						adb_dest[tag_len_dest / 3].bit_rate.others =
							adb_def[i].bit_rate.others;
					else
						adb_dest[tag_len_dest / 3].bit_rate.others =
							adb_tx[j].bit_rate.others;
				} else if ((adb_dest[tag_len_dest / 3].format_code >=
							AUDIO_FORMAT_OBA) &&
						(adb_dest[tag_len_dest / 3].format_code <=
							AUDIO_FORMAT_DST)) {
					adb_dest[tag_len_dest / 3].bit_rate.others =
						adb_tx[j].bit_rate.others &
						adb_def[i].bit_rate.others;
				} else {
					//todo
				}
				tag_len_dest += 3;
			}
		}
	}
	//recover duplicate audio format to rx format
	for (i = 0; i < tag_len_dest / 3; i++) {
		if (tx_duplicate_audio_format & (1 << adb_dest[i].format_code)) {
			for (j = 0; j < tag_len_def / 3; j++) {
				if (adb_dest[i].format_code == adb_def[j].format_code) {
					memcpy(&adb_dest[i], &adb_def[j],
							sizeof(struct edid_audio_block_t));
					break;
				}
			}
		}
	}
	if (log_level & EDID_LOG)
		rx_pr("adb_size = %d %d %d\n", tag_len_dest, tag_len_def, tag_len_tx);
	p_edid[adb_start] = (AUDIO_TAG << 5) + tag_len_dest;
	memcpy(p_edid + adb_start + 1, adb_dest, tag_len_dest);
	for (i = adb_start + tag_len_dest + 1; i < 255 - tag_len_def + tag_len_dest; i++)
		p_edid[i] = p_edid[i + tag_len_def - tag_len_dest];
	for (; i < 255; i++)
		p_edid[i] = 0;
		/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tag_len_def - tag_len_dest;
}

void rpt_edid_colorimetry_db_extraction(unsigned char *p_edid)
{
	u8 db_start = 0;
	u8 tx_db_start = 0;
	u8 colorimetry_tx[2] = {0}, colorimetry_def[2] = {0}, colorimetry_dest[2] = {0};
	u8 def_tag_len = 0;
	u8 i = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, EXTENDED_CDB_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		//abnormal condition, only for errordebug
		if (log_level & EDID_LOG)
			rx_pr("unsupported colorimetry!!\n");
		return;
	} else {
		def_tag_len = 3;
		memcpy(colorimetry_def, p_edid + db_start + 2, 2);
	}
	ret = rx_get_cea_tag_offset(edid_tx, EXTENDED_CDB_TAG);
	tx_db_start = ret.pos[0];
	if (!tx_db_start) {
		if (log_level & EDID_LOG)
			rx_pr("no colorimetry\n");
		edid_rm_db_by_tag(p_edid, EXTENDED_CDB_TAG);
		return;
	}
	memcpy(colorimetry_tx, edid_tx + tx_db_start + 2, 2);
	for (i = 0; i < 2; i++)
		colorimetry_dest[i] = colorimetry_tx[i] & colorimetry_def[i];
	memcpy(p_edid + db_start + 2, colorimetry_dest, 2);
}

void rpt_edid_420_vdb_extraction(unsigned char *p_edid)
{
	u8 _420_vdb_start = 0;
	u8 _420_cmdb_start = 0;
	u8 tx_420_vdb_start = 0;
	u8 tx_420_cmdb_start = 0;
	u8 vdb_start = 0;
	u8 tx_420vic_list[31] = { 0 };
	u8 def_420vic_list[31] = { 0 };
	u8 dest_420vic_list[31] = { 0 };
	u8 new_vic_list[31] = { 0 };
	u8 new_420vdb_list[31] = { 0 };
	u8 def_vdb_len = 0;
	u8 tx_420_vdb_len = 0;
	u8 tx_cmdb_len = 0;
	u8 def_420_vdb_len = 0;
	u8 def_cmdb_len = 0;
	u8 i = 0;
	u8 j = 0;
	u8 k = 0;
	u8 l = 0;
	u32 cmdb_map = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	//420cmdb
	ret = rx_get_cea_tag_offset(p_edid, EXTENDED_Y420CMDB_TAG);
	_420_cmdb_start = ret.pos[0];
	if (_420_cmdb_start) {
		def_cmdb_len = (p_edid[_420_cmdb_start]) & 0x1f;
		if (def_cmdb_len > 5) {
			if (log_level & EDID_LOG)
				rx_pr("cmdb len error:%d\n", def_cmdb_len);
			def_cmdb_len = 5;
		}
		for (i = 0; i < def_cmdb_len - 1; i++)
			cmdb_map += p_edid[_420_cmdb_start + 2 + i] << (i * 8);
		for (i = 0; i < 31; i++) {
			if ((cmdb_map >> i) & 1) {
				def_420vic_list[j] = def_vic_list[i];
				if (def_420vic_list[j])
					j++;
			}
		}
	}
	//420vdb
	ret = rx_get_cea_tag_offset(p_edid, EXTENDED_Y420VDB_TAG);
	_420_vdb_start = ret.pos[0];
	if (_420_vdb_start) {
		def_420_vdb_len = (p_edid[_420_vdb_start]) & 0x1f;
		if (j + def_420_vdb_len - 1 > 31) {
			rx_pr("420DB error\n");
			return;
		}
		memcpy(def_420vic_list + j, p_edid + _420_vdb_start + 2, def_420_vdb_len - 1);
	}

	if (def_420_vdb_len == 0 && def_cmdb_len == 0) {
		if (log_level & EDID_LOG)
			rx_pr("unsupported 420\n");
		return;
	}
	//TX EDID parsing
	//420cmdb
	j = 0;
	ret = rx_get_cea_tag_offset(edid_tx, EXTENDED_Y420CMDB_TAG);
	tx_420_cmdb_start = ret.pos[0];
	if (tx_420_cmdb_start) {
		tx_cmdb_len = (edid_tx[tx_420_cmdb_start]) & 0x1f;
		if (tx_cmdb_len > 5) {
			if (log_level & EDID_LOG)
				rx_pr("cmdb len error:%d\n", tx_cmdb_len);
			tx_cmdb_len = 5;
		}
		cmdb_map = 0;
		for (i = 0; i <  tx_cmdb_len - 1; i++)
			cmdb_map += edid_tx[tx_420_cmdb_start + 2 + i] << (i * 8);
		j = 0;
		for (i = 0; i < 31; i++) {
			if ((cmdb_map >> i) & 1) {
				tx_420vic_list[j] = tx_vic_list[i];
				if (tx_420vic_list[j])
					j++;
			}
		}
	}
	//420vdb
	ret = rx_get_cea_tag_offset(edid_tx, EXTENDED_Y420VDB_TAG);
	tx_420_vdb_start = ret.pos[0];
	if (tx_420_vdb_start) {
		tx_420_vdb_len = (edid_tx[tx_420_vdb_start]) & 0x1f;
		if (j + tx_420_vdb_len - 1 > 31) {
			rx_pr("420DB error\n");
			return;
		}
		memcpy(tx_420vic_list + j, edid_tx + tx_420_vdb_start + 2, tx_420_vdb_len - 1);
	}
	if (tx_420_vdb_len == 0 && tx_cmdb_len == 0) {
		if (log_level & EDID_LOG)
			rx_pr("no 420 block\n");
		edid_rm_db_by_tag(p_edid, EXTENDED_Y420CMDB_TAG);
		edid_rm_db_by_tag(p_edid, EXTENDED_Y420VDB_TAG);
		return;
	}
	k = 0;
	for (j = 0; tx_420vic_list[j]; j++)
		for (i = 0; def_420vic_list[i]; i++)
			if (def_420vic_list[i] == tx_420vic_list[j] && tx_420vic_list[j] != 0) {
				dest_420vic_list[k] = tx_420vic_list[j];
				k++;
				break;
			}
	//no dest 420vic list, need remove dc_36bit_420 dc_30bit_420 in vshf
	if (k == 0)
		vshf_dc_30_36_420_support = false;
	if (def_cmdb_len) {
		ret = rx_get_cea_tag_offset(p_edid, VIDEO_TAG);
		vdb_start = ret.pos[0];
		if (!vdb_start && (log_level & EDID_LOG))
			rx_pr("cannot find vdb\n");
		def_vdb_len = p_edid[vdb_start] & 0x1f;
		memset(def_vic_list, 0, 31);
		memcpy(def_vic_list, p_edid + vdb_start + 1, def_vdb_len);
		k = 0;
		for (i = 0; dest_420vic_list[i]; i++) {
			for (j = 0; j < def_vdb_len; j++) {
				if (dest_420vic_list[i] == def_vic_list[j]) {
					new_vic_list[k] = def_vic_list[j];
					k++;
					break;
				} else if (j == def_vdb_len - 1) {
					new_420vdb_list[l] = dest_420vic_list[i];
					l++;
				}
			}
		}
		if (log_level & EDID_LOG) {
			for (i = 0; i < 10; i++)
				rx_pr("new_tx_vic_list[%d]=%x\n", i, new_vic_list[i]);
			for (i = 0; i < 10; i++)
				rx_pr("new_420vdb_list[%d]=%x\n", i, new_420vdb_list[i]);
		}
		if (k == 0) {
			cmdb_map = 0;
			edid_rm_db_by_tag(p_edid, EXTENDED_Y420CMDB_TAG);
			tx_cmdb_len = 0;
			goto end1;
		} else {
			cmdb_map = (1 << k) - 1;
		}
		for (i = 0; def_vic_list[i]; i++) {
			for (j = 0; j < k; j++) {
				if (new_vic_list[j] == def_vic_list[i])
					break;
			}
			if (j == k) {
				new_vic_list[k] = def_vic_list[i];
				k++;
			}
		}
		j = 0;
		while (cmdb_map & 0xff) {
			p_edid[_420_cmdb_start + 2 + j] = cmdb_map & 0xff;
			cmdb_map = cmdb_map >> 8;
			j++;
		}
		memcpy(p_edid + vdb_start + 1, new_vic_list, def_vdb_len);
		p_edid[_420_cmdb_start] = (USE_EXTENDED_TAG << 5) + j + 1;
		for (i = _420_cmdb_start + j + 2; i < 255 - def_cmdb_len + j + 1; i++)
			p_edid[i] = p_edid[i + def_cmdb_len - j - 1];
		for (; i < 255; i++)
			p_edid[i] = 0;
		/* dtd offset modify */
		p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= def_cmdb_len - j - 1;
end1:
		if (def_420_vdb_len) {
			if (new_420vdb_list[0] && l) {
				ret = rx_get_cea_tag_offset(p_edid, EXTENDED_Y420VDB_TAG);
				_420_vdb_start = ret.pos[0];
				p_edid[_420_vdb_start] = (USE_EXTENDED_TAG << 5) + l + 1;
				memcpy(p_edid + _420_vdb_start + 2, new_420vdb_list, l);
				for (i = _420_vdb_start + l + 1;
					i < 255 - def_420_vdb_len + l + 1; i++)
					p_edid[i] = p_edid[i + def_420_vdb_len - l - 1];
				for (; i < 255; i++)
					p_edid[i] = 0;
				/* dtd offset modify */
				p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -=
					def_420_vdb_len - l - 1;
			} else {
				edid_rm_db_by_tag(p_edid, EXTENDED_Y420VDB_TAG);
			}
		}
	} else if (def_420_vdb_len) {
		if (dest_420vic_list[0] && k) {
			p_edid[_420_vdb_start] = (USE_EXTENDED_TAG << 5) + k + 1;
			memcpy(p_edid + _420_vdb_start + 2, dest_420vic_list, k);
			for (i = _420_vdb_start + k + 2; i < 255 - def_420_vdb_len + k + 1; i++)
				p_edid[i] = p_edid[i + def_420_vdb_len - k - 1];
			for (; i < 255; i++)
				p_edid[i] = 0;
			/* dtd offset modify */
			p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -=
				def_420_vdb_len - k - 1;
		} else {
			edid_rm_db_by_tag(p_edid, EXTENDED_Y420VDB_TAG);
		}
	}
}

void rpt_edid_hdr_static_db_extraction(unsigned char *p_edid)
{
	u8 db_start = 0;
	u8 tx_db_start = 0;
	u8 tx_db[31] = {0}, def_db[31] = {0};
	u8 tx_len = 0;
	u8 def_len = 0;
	u8 i = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;
	ret = rx_get_cea_tag_offset(p_edid, EXTENDED_HDR_STATIC_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("unsupported HDR!!\n");
		return;
	} else {
		def_len = (p_edid[db_start]) & 0x1f;
		memcpy(def_db, p_edid + db_start + 2, def_len - 1);
	}
	ret = rx_get_cea_tag_offset(edid_tx, EXTENDED_HDR_STATIC_TAG);
	tx_db_start = ret.pos[0];
	if (!tx_db_start) {
		if (log_level & EDID_LOG)
			rx_pr("no HDR!!\n");
		//remove p_edid's block, not edid_tx!!!!!!
		edid_rm_db_by_tag(p_edid, EXTENDED_HDR_STATIC_TAG);
		return;
	} else {
		tx_len = (edid_tx[tx_db_start]) & 0x1f;
		memcpy(tx_db, edid_tx + tx_db_start + 2, tx_len - 1);
	}
	/*
	for (i = 0; i < 2; i++)
		tx_db[i] = tx_db[i] & def_db[i];
	*/
	if (def_len && tx_len) {
		//keep using tx data block, only BYTE 3 && 4 changed.
		for (i = 0; i < 2; i++)
			p_edid[db_start + i + 2] = tx_db[i] & def_db[i];
	} else if (def_len == 0) {
		//remove HDR block
		for (i = db_start; i < 254 - tx_len; i++)
			p_edid[i] = p_edid[i + tx_len];
		for (; i < 255; i++)
			p_edid[i] = 0;
	}
	/* dtd offset modify */
	p_edid[EDID_BLOCK1_OFFSET + EDID_DESCRIP_OFFSET] -= tx_len - def_len;
}

void rpt_edid_vsv_hdr10plus_extraction(unsigned char *p_edid)
{
	u8 db_start = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, VSVDB_HDR10P_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("rx no vsv hdr10+!!\n");
		return;
	}

	ret = rx_get_cea_tag_offset(edid_tx, VSVDB_HDR10P_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("tx no vsv hdr10+, need remove rx vsv hdr10+\n");
		edid_rm_db_by_tag(p_edid, VSVDB_HDR10P_TAG);
	}
}

void rpt_edid_vsv_db_extraction(unsigned char *p_edid)
{
	u8 db_start = 0;
	u8 tx_db_start = 0;
	u8 tx_len = 0;
	u8 db_version = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, VSVDB_DV_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("rx no vsvdb!!\n");
		return;
	}

	tx_len = (p_edid[db_start]) & 0x1f;
	if (tx_len < 11)
		return;

	db_version = (p_edid[db_start + 5] >> 5) & 0x07;
	if (db_version != 2)
		return;

	p_edid[db_start + 7] &= 0xFE; //remove interface 420
	p_edid[db_start + 8] &= 0xFE; //remove 444 12bit
	p_edid[db_start + 9] &= 0xFE; //remove 444 10bit

	ret = rx_get_cea_tag_offset(edid_tx, VSVDB_DV_TAG);
	tx_db_start = ret.pos[0];
	if (!tx_db_start) {
		if (log_level & EDID_LOG)
			rx_pr("tx no vsvdb!!,need remove rx vsvdb\n");
		edid_rm_db_by_tag(p_edid, VSVDB_DV_TAG);
	}
}

void rpt_edid_vsg_freesync_extraction(unsigned char *p_edid)
{
	u8 db_start = 0;
	struct data_block_location_s ret = {0};

	if (!p_edid)
		return;

	ret = rx_get_cea_tag_offset(p_edid, VSDB_FREESYNC_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("rx no vsg freesync!!\n");
		return;
	}

	ret = rx_get_cea_tag_offset(edid_tx, VSDB_FREESYNC_TAG);
	db_start = ret.pos[0];
	if (!db_start) {
		if (log_level & EDID_LOG)
			rx_pr("tx no vsg freesync, need remove rx vsg freesync\n");
		edid_rm_db_by_tag(p_edid, VSDB_FREESYNC_TAG);
	}
}

void rpt_edid_update_dtd(u8 *p_edid)
{
	int i;
	bool checksum_update = false;
	struct edid_info_s *edid_info = NULL;
	//default 1080P dtd
	u8 def_dtd[18] = {0x02, 0x3A, 0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58,
					  0x2C, 0x45, 0x00, 0x40, 0x84, 0x63, 0x00, 0x00, 0x1E};
	edid_info = kzalloc(sizeof(*edid_info), GFP_KERNEL);
	if (!edid_info)
		return;
	if (!get_basic_dtd_data(p_edid, edid_info)) {
		kfree(edid_info);
		return;
	}
	if (edid_info->descriptor2.pixel_clk > MAX_PIXEL_CLK ||
		edid_info->descriptor2.hactive > MAX_H_ACTIVE ||
		edid_info->descriptor2.vactive > MAX_V_ACTIVE ||
		edid_info->descriptor2.framerate > MAX_FRAME_RATE) {
		//clear to reserved
		edid_info->descriptor2.pixel_clk = 0;
		for (i = 0; i < 18; i++)
			p_edid[0x48 + i] = 0;
		p_edid[0x4b] = 0x11;
		checksum_update = true;
	}
	if (edid_info->descriptor1.pixel_clk > MAX_PIXEL_CLK ||
		edid_info->descriptor1.hactive > MAX_H_ACTIVE ||
		edid_info->descriptor1.vactive > MAX_V_ACTIVE ||
		edid_info->descriptor1.framerate > MAX_FRAME_RATE) {
		if (edid_info->descriptor2.pixel_clk) {
			//first dtd = second dtd
			for (i = 0; i < 18; i++)
				p_edid[0x36 + i] = p_edid[0x48 + i];
		} else {
			//use default setting
			for (i = 0; i < 18; i++)
				p_edid[0x36 + i] = def_dtd[i];
		}
		checksum_update = true;
	}
	kfree(edid_info);
	if (checksum_update) {
		p_edid[127] = 0;
		for (i = 0; i < 127; i++) {
			p_edid[127] += p_edid[i];
			p_edid[127] &= 0xFF;
		}
		p_edid[127] = (0x100 - p_edid[127]) & 0xFF;
	}
}

void get_edid_standard_timing_info(u8 *p_edid, struct edid_standard_timing *edid_st_info)
{
	int i = 0;

	if (!p_edid)
		return;

	for (i = 0; i <= 7; i++) {
		if (p_edid[0x26 + i * 2] != 0x01 && p_edid[0x27 + i * 2] != 0x01) {
			edid_st_info->refresh_rate[i] = (int)(p_edid[0x27 + i * 2] & 0x3F) + 60;
			edid_st_info->h_active[i] = ((int)(p_edid[0x26 + i * 2]) + 31) * 8;
			edid_st_info->aspect_ratio[i]  = (int)((p_edid[0x27 + i * 2] >> 5) & 0x03);
		}
	}
}

void rpt_edid_update_stand_timing(u8 *p_edid)
{
	u8 i = 0;
	struct edid_standard_timing edid_st_info = {0};

	get_edid_standard_timing_info(p_edid, &edid_st_info);
	for (i = 0; i <= 7; i++) {
		if (edid_st_info.refresh_rate[i] > EDID_MAX_REFRESH_RATE) {
			if (log_level & EDID_LOG)
				rx_pr("sti%d is not supported\n", i);
			p_edid[0x26 + i * 2] = 0x01;
			p_edid[0x27 + i * 2] = 0x01;
		}
	}
}

static void edid_sad_passthrough(unsigned char *p_edid_dest, unsigned char *p_edid_src)
{
	u8 adb_start = 0;
	struct data_block_location_s ret = {0};

	ret = rx_get_cea_tag_offset(p_edid_src, AUDIO_TAG);
	adb_start = ret.pos[0];
	if (adb_start) {
		/* place tv aud data blk to blk index = 0x1 */
		splice_data_blk_to_edid(p_edid_dest, &p_edid_src[adb_start], 0x01);
	}
}

void rpt_edid_extraction(unsigned char *p_edid)
{
	if (!is_valid_edid_data(edid_tx))
		return;
	if (rpt_edid_selection == use_edid_def_secondary_phyaddr) {
		edid_secondaryphyaddr(p_edid, edid_tx);
		return;
	} else if (rpt_edid_selection == use_edid_def_sad_passthrough_secondary_phyaddr) {
		edid_sad_passthrough(p_edid, edid_tx);
		edid_secondaryphyaddr(p_edid, edid_tx);
		return;
	} else if (rpt_edid_selection == use_edid_def) {
		return;
	}

	//block0
	rpt_edid_update_stand_timing(p_edid);
	rpt_edid_update_dtd(p_edid);
	//block1
	rpt_edid_extension_num_extraction(p_edid);
	/* refer to hdmi2.1 spec 10.3.6 Repeaters that include
	 * an HF-EEODB in their E-EDID shall set the EDID Extension
	 * Block Countfield to a non-zero value . Repeaters that do
	 * not have the ability to forward more than two E-EDID blocks
	 * to theupstream Source shall either (1) remove the HF-EEODB
	 * before forwarding the E-EDID to the Source , or (2) set
	 * the EDID Extension Block Count to 1 in the EDID Extension
	 * Block Count. Option (2) is useful for Repeaters that do
	 * not have the ability to forward more than two E-EDID Blocks
	 * to the Source , and do not wish to reorder the existing
	 * Data Blocks in the E-EDID prior to forwarding
	 */
	rpt_edid_rm_hf_eeodb(p_edid);
	rpt_edid_multi_vdb_extraction(p_edid, edid_tx);
	//OTT-56704 need pass through tv audio block
	if (rpt_edid_selection == use_edid_repeater_sad_passthrough)
		edid_sad_passthrough(p_edid, edid_tx);
	else
		rpt_edid_audio_db_extraction(p_edid);
	rpt_edid_14_vs_db_extraction(p_edid);
	rpt_edid_420_vdb_extraction(p_edid);
	rpt_edid_hf_vs_db_extraction(p_edid);
	rpt_edid_colorimetry_db_extraction(p_edid);
	rpt_edid_hdr_static_db_extraction(p_edid);
	rpt_edid_vsv_hdr10plus_extraction(p_edid);
	rpt_edid_vsg_freesync_extraction(p_edid);
	rpt_edid_vsv_db_extraction(p_edid);
}
#endif

u8 rx_get_dtd_offset(u_char *pedid, u8 blk_num)
{
	u32 start = blk_num * EDID_BLK_SIZE + 4;
	u32 end = END_OF_BLK(blk_num);
	u8 offset = 4, length = 0;

	if (!pedid)
		return 0;
	while (start < end) {
		if (pedid[start] == 0)
			break;
		length = (pedid[start] & 0x1f) + 1;
		offset += length;
		start += length;
	}
	return offset;
}

u_char rx_edid_calc_cksum(u_char *pedid, u8 blk_num)
{
	u_int i = 0;
	u_int checksum = 0;
	u32 start = blk_num * EDID_BLK_SIZE;
	u32 end = END_OF_BLK(blk_num);

	if (!pedid)
		return 0;
	for (i = start; i <= end; i++) {
		if (i < end) {
			checksum += pedid[i];
			checksum &= 0xFF;
		} else {
			checksum = (0x100 - checksum) & 0xFF;
		}
	}
	return checksum;
}

u32 rx_get_edid_size(u8 *pedid)
{
	u32 blk_num = 0, index = 0;
	struct data_block_location_s ret = {0};

	ret = rx_get_cea_tag_offset(pedid, EXTENDED_HF_EEODB);
	index = ret.pos[0];
	if (index)
		blk_num = pedid[index + 2] + 1;
	else
		blk_num = pedid[126] + 1;

	if (!blk_num)
		blk_num = 2;

	return blk_num * EDID_BLK_SIZE;
}

u_char *rx_get_cur_def_edid(u_char port)
{
	u32 edid_offset = 0;
	enum edid_ver_e ver = get_edid_selection(port);

	if (ver == EDID_V21)
		edid_offset = 256;
	else if (ver == EDID_V20)
		edid_offset = 768;

	switch (port) {
	case 0:
		memcpy(edid_cur, edid_buf1 + edid_offset, ver == EDID_V21 ? 512 : 256);
		break;
	case 1:
		memcpy(edid_cur + EDID_SIZE, edid_buf2 + edid_offset,
			ver == EDID_V21 ? 512 : 256);
		break;
	case 2:
		memcpy(edid_cur + EDID_SIZE * 2, edid_buf3 + edid_offset,
			ver == EDID_V21 ? 512 : 256);
		break;
	case 3:
		memcpy(edid_cur + EDID_SIZE * 3, edid_buf4 + edid_offset,
			ver == EDID_V21 ? 512 : 256);
		break;
	default:
		break;
	}

	return &edid_cur[EDID_SIZE * port];
}

u_char *rx_get_cur_used_edid(u_char port)
{
	if (port >= rx_info.port_num)
		return NULL;

	return &edid_cur[port * EDID_SIZE];
}

void rx_print_edid_support(u8 port)
{
	rx_pr("****port %d Capacity info****\n", port);
	rx_pr("support vrr: %d\n", rx[port].edid_cap.vrr);
	rx_pr("support allm: %d\n", rx[port].edid_cap.allm);
	rx_pr("support qms: %d\n", rx[port].edid_cap.qms);
	rx_pr("support 4k: %d\n", rx[port].edid_cap.timing_4k);
	rx_pr("support HF_DB: %d\n", rx[port].edid_cap.hf_db);
	rx_pr("support DV_DB: %d\n", rx[port].edid_cap.vsdv_db);
	rx_pr("support HDR10P_DB: %d\n", rx[port].edid_cap.hdr10p_db);
	rx_pr("support HDR_STATIC_DB: %d\n", rx[port].edid_cap.hdr_static_db);
	rx_pr("support HDR_DYNAMIC_DB: %d\n", rx[port].edid_cap.hdr_dynamic_db);
	rx_pr("support FREESYNC_DB: %d\n", rx[port].edid_cap.freesync_db);
}

void rx_get_edid_support(u8 port)
{
	u_char *edid_buf = NULL;

	edid_buf = rx_get_cur_used_edid(port);
	if (!edid_buf)
		return;
	rx[port].edid_cap.vrr = get_edid_support(edid_buf, HF_VRR);
	rx[port].edid_cap.allm = get_edid_support(edid_buf, HF_ALLM);
	rx[port].edid_cap.qms = get_edid_support(edid_buf, HF_QMS);
	rx[port].edid_cap.hf_db = get_edid_support(edid_buf, HF_DB);
	rx[port].edid_cap.vsdv_db = get_edid_support(edid_buf, VSDV_DB);
	rx[port].edid_cap.hdr10p_db = get_edid_support(edid_buf, HDR10P_DB);
	rx[port].edid_cap.hdr_static_db = get_edid_support(edid_buf, HDR_STATIC_DB);
	rx[port].edid_cap.hdr_dynamic_db = get_edid_support(edid_buf, HDR_DYNAMIC_DB);
	rx[port].edid_cap.freesync_db = get_edid_support(edid_buf, FREESYNC_DB);
	rx[port].edid_cap.timing_4k = get_edid_support(edid_buf, TIMING_4K);
}

bool hdmi_rx_top_edid_update(u8 port)
{
	//u_char *pedid = NULL;
	u_int i = 0;
	//u8 ext_blk_num = 1;
	static int edid_reset_cnt;

	if (rx_info.chip_id != CHIP_ID_T6X)
		rx_edid_module_reset();
	while (edid_reset_cnt <= edid_reset_max)
		edid_reset_cnt++;
	edid_reset_cnt = 0;

	for (i = 0; i < 512; i++)
		hdmirx_wr_top(edid_addr[port] + i, edid_dbg1[i], port);

	if (log_level & EDID_LOG)
		rx_print_edid_support(port);
	return true;
}

void rx_clr_edid_type(unsigned char port)
{
	if (port >= rx_info.port_num)
		return;

	rx[port].edid_type.need_update = false;
	if (log_level & EDID_LOG)
		rx_pr("edid_auto_sel:%d, port:%d, cfg:%d\n",
			edid_auto_sel, port, rx[port].edid_type.cfg);
	switch (rx[port].edid_type.cfg) {
	case EDID_V21:
		rx[port].edid_type.edid_ver = EDID_V21;
		break;
	case EDID_V20:
		rx[port].edid_type.edid_ver = EDID_V20;
		break;
	case EDID_AUTO20:
		if (rx[port].tx_type == DEV_HDMI14)
			rx[port].edid_type.edid_ver = EDID_V14;
		else
			rx[port].edid_type.edid_ver = EDID_V21;
		break;
	case EDID_AUTO14:
		if (rx[port].tx_type == DEV_HDMI20)
			rx[port].edid_type.edid_ver = EDID_V21;
		else
			rx[port].edid_type.edid_ver = EDID_V14;
		break;
	case EDID_V14:
	default:
		rx[port].edid_type.edid_ver = EDID_V14;
		break;
	}
}

void edid_type_init(void)
{
	unsigned char i = 0;

	for (i = 0; i < rx_info.port_num; i++)
		rx_clr_edid_type(i);
}

void edid_type_update(u8 port)
{
	switch (rx[port].edid_type.cfg) {
	case EDID_V14:
	case EDID_V20:
		break;
	case EDID_AUTO14:
		if ((edid_auto_sel & rx[port].edid_type.cfg) == 0)
			break;
		if (rx[port].tx_type == DEV_HDMI20 && rx[port].edid_type.edid_ver != EDID_V21) {
			rx[port].edid_type.edid_ver = EDID_V21;
			rx[port].edid_type.need_update = true;
		}
		break;
	case EDID_AUTO20:
		if ((edid_auto_sel & rx[port].edid_type.cfg) == 0)
			break;
		if (rx[port].tx_type == DEV_HDMI14 || rx[port].tx_type == DEV_HDMI14_UNKNOWN_PORT) {
			if (rx[port].edid_type.edid_ver != EDID_V14) {
				rx[port].edid_type.edid_ver = EDID_V14;
				rx[port].edid_type.need_update = true;
			}
		} else if (rx[port].tx_type == DEV_ABNORMAL_SCDC) {
			if (port == rx_info.main_port) {
				if (rx[port].edid_type.edid_ver != EDID_V21) {
					rx[port].edid_type.edid_ver = EDID_V21;
					rx[port].edid_type.need_update = true;
				}
				rx[port].tx_type = DEV_HDMI20;
			} else {
				if (rx[port].edid_type.edid_ver != EDID_V14) {
					rx[port].edid_type.edid_ver = EDID_V14;
					rx[port].edid_type.need_update = true;
				}
			}
		} else {
			if (rx[port].edid_type.edid_ver != EDID_V21) {
				rx[port].edid_type.edid_ver = EDID_V21;
				rx[port].edid_type.need_update = true;
			}
		}
		break;
	default:
		break;
	}
	if (rx[port].edid_type.need_update) {
		edid_update_dwork.port = port;
		schedule_work(&edid_update_dwork.work);
		rx[port].edid_type.need_update = false;
	}
}

void rx_edid_reset_task(u8 port)
{
	edid_reset_work.port = port;
	edid_reset_work.state[port] = EDID_WAIT_READ_DONE;
	schedule_delayed_work(&edid_reset_work.delayed_work, msecs_to_jiffies(40));
}

u8 rx_read_edid_offset(u8 port)
{
	u32 temp = 0;

	switch (rx_info.chip_id) {
	case CHIP_ID_T3X:
		temp = hdmirx_rd_top(TOP_EDID_GEN_STAT, port) & 0x1ff;
		break;
	case CHIP_ID_TXHD2:
	case CHIP_ID_T5M:
	case CHIP_ID_TL1:
	case CHIP_ID_TM2:
	case CHIP_ID_T5:
	case CHIP_ID_T5D:
	case CHIP_ID_T7:
	case CHIP_ID_T3:
	case CHIP_ID_T5W:
	default:
		switch (port) {
		case E_PORT0:
			temp = hdmirx_rd_top(TOP_EDID_GEN_STAT, E_PORT0) & 0x1ff;
			break;
		case E_PORT1:
			temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_B, E_PORT1) & 0x1ff;
			break;
		case E_PORT2:
			temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_C, E_PORT2) & 0x1ff;
			break;
		case E_PORT3:
			temp = hdmirx_rd_top(TOP_EDID_GEN_STAT_D, E_PORT3) & 0x1ff;
			break;
		default:
			break;
		}
		break;
	}
	return temp;
}

void rx_edid_reset_handler(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct edid_delayed_work_data *dwd =
		container_of(dw, struct edid_delayed_work_data, delayed_work);
	int i = 0;
	bool rst_flg = true;
	static u8 rst_cnt[E_PORT_NUM];

	switch (dwd->state[dwd->port]) {
	case EDID_WAIT_READ_DONE:
		if (rx_is_edid_read_done(dwd->port))
			dwd->state[dwd->port] = EDID_WAIT_OTHER_PORT_IDLE;
		break;
	case EDID_WAIT_OTHER_PORT_IDLE:
		for (i = 0; i < rx_info.port_num; i++) {
			if (i == dwd->port)
				continue;
			dwd->edid_offset_cur[i] = rx_read_edid_offset(i) & 0x1ff;
			if (dwd->edid_offset_cur[i] != 0)
				rst_flg = false;
		}
		if (rst_flg || rst_cnt[dwd->port] >= EDID_RST_TIMEOUT) {
			rx_edid_reset(dwd->port);
			if (rx_read_edid_offset(dwd->port) == 0) {
				edid_seg_flag[dwd->port] = 0;
				dwd->state[dwd->port] = EDID_RESET_DONE;
			} else {
				rx_pr("edid reset fail\n");
			}
			rst_cnt[dwd->port] = 0;
		} else {
			rst_cnt[dwd->port]++;
		}
		break;
	case EDID_RESET_DONE:
		break;
	default:
		break;
	}
	if (edid_seg_flag[dwd->port])
		schedule_delayed_work(&edid_reset_work.delayed_work, msecs_to_jiffies(40));
}

enum hrtimer_restart edid_reset_callback(struct hrtimer *timer)
{
	struct edid_timer_data *dwd = container_of(timer, struct edid_timer_data, timer);

	if (edid_seg_flag[dwd->port] == 0) {
		rx_pr("EDID reset completed, stopping timer for port %d\n", dwd->port);
		return HRTIMER_NORESTART;
	}
	switch (dwd->state[dwd->port]) {
	case EDID_WAIT_READ_DONE:
		if (rx_is_edid_read_done(dwd->port)) {
			rx_edid_reset(dwd->port);
			dwd->state[dwd->port] = EDID_RESET_DONE;
		}
		break;
	case EDID_RESET_DONE:
		if (rx_read_edid_offset(dwd->port) == 0) {
			rx_pr("EDID reset done for port %d\n", dwd->port);
			edid_seg_flag[dwd->port] = 0;
		}
		break;
	default:
		break;
	}
	interval = ktime_set(0, 3000000); // 3ms
	hrtimer_forward_now(timer, interval);
	return HRTIMER_RESTART;
}
