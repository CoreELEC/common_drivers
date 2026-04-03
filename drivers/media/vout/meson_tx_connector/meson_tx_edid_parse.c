// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* #define DEBUG */
/*
 * uboot has __UBOOT__ macro and __KERNEL__ macro.
 * kernel only has __KERNEL__ macro
 */
#ifdef __UBOOT__
#include <linux/kernel.h>
#include <linux/compat.h>
#include <linux/types.h>
#include <common.h>
#include <linux/stddef.h>
#include <amlogic/media/vout/aml_vinfo.h>
#include <amlogic/media/vout/meson_tx-connector/meson_tx_edid.h>
#include "hdmitx_log.h"
#include <amlogic/media/vout/dsc.h>
#include "hdmitx_check_valid.h"
#elif __KERNEL__
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/gcd.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_edid.h>
#endif

#undef pr_fmt
#define pr_fmt(fmt) "tx_edid: " fmt

#define CEA_DATA_BLOCK_COLLECTION_ADDR_1STP 0x04
#define VIDEO_TAG 0x40
#define AUDIO_TAG 0x20
#define VENDOR_TAG 0x60
#define SPEAKER_TAG 0x80
#define EXTENSION_IFDB_TAG	0x20

#define HDMI_EDID_BLOCK_TYPE_RESERVED		0
#define HDMI_EDID_BLOCK_TYPE_AUDIO		1
#define HDMI_EDID_BLOCK_TYPE_VIDEO		2
#define HDMI_EDID_BLOCK_TYPE_VENDER		3
#define HDMI_EDID_BLOCK_TYPE_SPEAKER		4
#define HDMI_EDID_BLOCK_TYPE_VESA		5
#define HDMI_EDID_BLOCK_TYPE_RESERVED2		6
#define HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG	7

#define EXTENSION_VIDEO_CAPABILITY_TAG 0x0
#define EXTENSION_VENDOR_SPECIFIC_TAG 0x1
#define EXTENSION_COLORMETRY_TAG 0x5
/* DRM stands for "Dynamic Range and Mastering " */
#define EXTENSION_DRM_STATIC_TAG    0x6
#define EXTENSION_DRM_DYNAMIC_TAG   0x7
   #define TYPE_1_HDR_METADATA_TYPE    0x0001
   #define TS_103_433_SPEC_TYPE        0x0002
   #define ITU_T_H265_SPEC_TYPE        0x0003
   #define TYPE_4_HDR_METADATA_TYPE    0x0004
/* Video Format Preference Data block */
#define EXTENSION_VFPDB_TAG	0xd
#define EXTENSION_Y420_VDB_TAG	0xe
#define EXTENSION_Y420_CMDB_TAG	0xf
#define EXTENSION_DOLBY_VSADB	0x11
/* HDMI Forum Sink Capability Data Block */
#define EXTENSION_SCDB_EXT_TAG	0x79
/* 122 */
#define EXTENSION_SBTM_EXT_TAG	0x7a

#define EDID_DETAILED_TIMING_DES_BLOCK0_POS 0x36
#define EDID_DETAILED_TIMING_DES_BLOCK1_POS 0x48
#define EDID_DETAILED_TIMING_DES_BLOCK2_POS 0x5A
#define EDID_DETAILED_TIMING_DES_BLOCK3_POS 0x6C

/* EDID Descriptor Tag */
#define TAG_PRODUCT_SERIAL_NUMBER 0xFF
#define TAG_ALPHA_DATA_STRING 0xFE
#define TAG_RANGE_LIMITS 0xFD
/* MONITOR NAME */
#define TAG_DISPLAY_PRODUCT_NAME_STRING 0xFC
#define TAG_COLOR_POINT_DATA 0xFB
#define TAG_STANDARD_TIMINGS 0xFA
#define TAG_DISPLAY_COLOR_MANAGEMENT 0xF9
#define TAG_CVT_TIMING_CODES 0xF8
#define TAG_ESTABLISHED_TIMING_III 0xF7
#define TAG_DUMMY_DES 0x10

#define GET_BITS_FILED(val, start, len) \
	(((val) >> (start)) & ((1 << (len)) - 1))

static int display_id_parse(struct display_id_cap *disp_id_cap, u8 *disp_id_raw);
static void display_id_cap_clear(struct display_id_cap *disp_id_cap);
static void edid_dtd_parsing(struct rx_cap *prxcap, unsigned char *data);
/* specially for hdmitx */
static void hdmitx_edid_set_default_aud(struct rx_cap *prxcap);
static void hdmitx_edid_check_pcm_declare(struct rx_cap *prxcap);

/*
 * cec_get_edid_spa_location() - find location of the Source Physical Address
 *
 * @edid: the EDID
 * @size: the size of the EDID
 *
 * This EDID is expected to be a CEA-861 compliant, which means that there are
 * at least two blocks and one or more of the extensions blocks are CEA-861
 * blocks.
 *
 * The returned location is guaranteed to be <= size-2.
 *
 * This is an inline function since it is used by both CEC and V4L2.
 * Ideally this would go in a module shared by both, but it is overkill to do
 * that for just a single function.
 */
static unsigned int cec_get_edid_spa_location(const u8 *edid, unsigned int size)
{
	unsigned int blocks = size / 128;
	unsigned int block;
	u8 d;

	/* Sanity check: at least 2 blocks and a multiple of the block size */
	if (blocks < 2 || size % 128)
		return 0;

	/*
	 * If there are fewer extension blocks than the size, then update
	 * 'blocks'. It is allowed to have more extension blocks than the size,
	 * since some hardware can only read e.g. 256 bytes of the EDID, even
	 * though more blocks are present. The first CEA-861 extension block
	 * should normally be in block 1 anyway.
	 */
	if (edid[0x7e] + 1 < blocks)
		blocks = edid[0x7e] + 1;

	for (block = 1; block < blocks; block++) {
		unsigned int offset = block * 128;

		/* Skip any non-CEA-861 extension blocks */
		if (edid[offset] != 0x02 || edid[offset + 1] != 0x03)
			continue;

		/*
		 * When the descriptor offset is greater than 127,
		 * the CTA block needs to parse 127 bytes of valid data
		 */
		d = (edid[offset + 2] > 0x7f) ? 0x7f : (edid[offset + 2] & 0x7f);
		/* Check if there are Data Blocks */
		if (d <= 4)
			continue;

		/* search Vendor Specific Data Block (tag 3) */
		if (d > 4) {
			unsigned int i = offset + 4;
			unsigned int end = offset + d;

			/* Note: 'end' is always < 'size' */
			do {
				u8 tag = edid[i] >> 5;
				u8 len = edid[i] & 0x1f;

				if (tag == 3 && len >= 5 && i + len <= end &&
				    edid[i + 1] == 0x03 &&
				    edid[i + 2] == 0x0c &&
				    edid[i + 3] == 0x00)
					return i + 4;
				i += len + 1;
			} while (i < end);
		}
	}
	return 0;
}

static u16 tx_get_edid_phy_addr(const u8 *edid, unsigned int size)
{
	unsigned int loc = cec_get_edid_spa_location(edid, size);

	if (loc == 0)
		return 0xffff;
	return (edid[loc] << 8) | edid[loc + 1];
}

void meson_tx_edid_phy_addr_parse(struct rx_cap *prxcap, u8 *edid_buf)
{
	u16 pa = 0xffff;

	if (!prxcap || !edid_buf)
		return;

	if (edid_buf[0x7e]) {
		pa = tx_get_edid_phy_addr((const u8 *)edid_buf,
				128 * (edid_buf[0x7e] + 1));
		prxcap->vsdb_phy_addr.a = (pa >> 12) & 0xf;
		prxcap->vsdb_phy_addr.b = (pa >> 8) & 0xf;
		prxcap->vsdb_phy_addr.c = (pa >> 4) & 0xf;
		prxcap->vsdb_phy_addr.d = (pa >> 0) & 0xf;
		if (pa != 0xffff)
			prxcap->vsdb_phy_addr.valid = 1;
	}
}

/* return the blocks of extension CTA */
static unsigned char tx_edid_get_cta_block_count(const u8 *edid_buf)
{
	unsigned char cta_block_count = 0;

	if (!edid_buf)
		return 0;

	cta_block_count = edid_buf[0x7E];
	/* HFR-EEODB */
	if (cta_block_count && edid_buf[128 + 4] == EXTENSION_EEODB_EXT_TAG &&
		edid_buf[128 + 5] == EXTENSION_EEODB_EXT_CODE)
		cta_block_count = edid_buf[128 + 6];
	/* limit cta_block_count to EDID_MAX_BLOCK - 1 */
	if (cta_block_count > EDID_MAX_BLOCK - 1)
		cta_block_count = EDID_MAX_BLOCK - 1;

	return cta_block_count;
}

/* return all the blocks of the EDID */
static unsigned char tx_edid_get_block_count(const u8 *edid_buf)
{
	if (!edid_buf)
		return 0;

	return tx_edid_get_cta_block_count(edid_buf) + 1;
}

bool meson_tx_edid_is_all_zeros(u8 *rawedid)
{
	unsigned int i = 0, j = 0;
	unsigned int chksum = 0;
	unsigned char block_count = 1;

	if (!rawedid)
		return false;

	block_count = tx_edid_get_block_count(rawedid);
	for (j = 0; j < block_count; j++) {
		chksum = 0;
		for (i = 0; i < 128; i++)
			chksum += rawedid[i + j * 128];
		if (chksum != 0)
			return false;
	}
	return true;
}

/* check the checksum for each sub block */
static bool _check_edid_blk_chksum(u8 edid_check, unsigned char *block)
{
	unsigned int chksum = 0;
	unsigned int i = 0;

	if (!block)
		return false;

	if (edid_check & 0x02)
		return true;

	for (chksum = 0, i = 0; i < 0x80; i++)
		chksum += block[i];
	if ((chksum & 0xff) != 0)
		return false;
	else
		return true;
}

/* check the first edid block */
static bool _check_base_structure(u8 edid_check, unsigned char *buf)
{
	unsigned int i = 0;

	if (!buf)
		return false;

	if (!(edid_check & 0x01)) {
		/* check block 0 first 8 bytes */
		if (buf[0] != 0 || buf[7] != 0)
			return false;

		for (i = 1; i < 7; i++) {
			if (buf[i] != 0xff)
				return false;
		}
	}

	if (_check_edid_blk_chksum(edid_check, buf) == false)
		return false;

	return true;
}

/*
 * check the EDID validity
 * base structure: header, checksum
 * extension: the first non-zero byte, checksum
 */
bool meson_tx_edid_check_valid(struct rx_cap *prxcap, unsigned char *buf)
{
	int i;
	int blk_cnt = 1;
	u8 edid_check = 0;

	if (!prxcap || !buf)
		return false;

	edid_check = prxcap->edid_check;
	blk_cnt = tx_edid_get_block_count(buf);

	/* check block 0 */
	if (_check_base_structure(edid_check, &buf[0]) == false) {
		prxcap->head_err = 1;
		return false;
	}

	if (blk_cnt == 1)
		return true;
	/* check block 1 extension tag */
	if (!(edid_check & 0x01)) {
		if (!(buf[0x80] == 0x2 || buf[0x80] == 0xf0))
			return false;
	}
	/* check extension block 1 and more */
	for (i = 1; i < blk_cnt; i++) {
		if (!(edid_check & 0x01)) {
			if (buf[i * 0x80] == 0)
				return false;
		}
		if (_check_edid_blk_chksum(edid_check, &buf[i * 0x80]) == false) {
			prxcap->chksum_err = 1;
			return false;
		}
	}

	return true;
}

static void edid_parsing_id_manufacturer_name(struct rx_cap *prxcap,
					   u8 *data)
{
	int i;
	u8 uppercase[26] = { 0 };
	u8 brand[3];

	if (!prxcap || !data)
		return;

	/* Fill array uppercase with 'A' to 'Z' */
	for (i = 0; i < 26; i++)
		uppercase[i] = 'A' + i;

	brand[0] = data[0] >> 2;
	brand[1] = ((data[0] & 0x3) << 3) + (data[1] >> 5);
	brand[2] = data[1] & 0x1f;

	if (brand[0] > 26 || brand[0] == 0 ||
	    brand[1] > 26 || brand[1] == 0 ||
	    brand[2] > 26 || brand[2] == 0)
		return;
	for (i = 0; i < 3; i++)
		prxcap->IDManufacturerName[i] = uppercase[brand[i] - 1];
}

static void edid_parsing_id_product_code(struct rx_cap *prxcap,
				      u8 *data)
{
	if (!prxcap || !data)
		return;

	prxcap->IDProductCode[0] = data[1];
	prxcap->IDProductCode[1] = data[0];
}

static void edid_parsing_id_serial_number(struct rx_cap *prxcap,
				       u8 *data)
{
	int i;

	if (!prxcap || !data)
		return;

	for (i = 0; i < 4; i++)
		prxcap->IDSerialNumber[i] = data[3 - i];
}

/*
 * store the idx of vesa_timing[32], which is 0
 * note: only save vesa mode, for compliance with uboot.
 * uboot not parse standard timing, or CVT block.
 * as disp_cap will check all mode in rx_cap->VIC[],
 * and all mode in vesa_timing[], if CEA mode is
 * stored in vesa_timing[], it will cause kernel
 * support more CEA mode than uboot.
 */
static void store_vesa_idx(struct rx_cap *prxcap, enum hdmi_vic vesa_timing)
{
	int i;
	int already = 0;

	if (!prxcap)
		return;

	for (i = 0; i < VESA_MAX_TIMING && prxcap->vesa_timing[i]; i++) {
		if (prxcap->vesa_timing[i] == vesa_timing) {
			already = 1;
			break;
		}
	}

	if (!already && i != VESA_MAX_TIMING) {
		prxcap->vesa_timing[i] = vesa_timing;
		prxcap->vesa_timing_count++;
	}
}

static void store_cea_idx(struct rx_cap *prxcap, enum hdmi_vic vic)
{
	int i;
	int already = 0;

	if (!prxcap)
		return;

	/* don't support 64:27 aspect ratio */
	if (meson_tx_mode_get_vic_aspect_ratio(vic) == HDMI_PICTURE_ASPECT_64_27) {
		pr_debug("aspect ratio is 64:27, vic=%d bypass\n", vic);
		return;
	}

	for (i = 0; (i < VIC_MAX_NUM) && (i < prxcap->VIC_count); i++) {
		if (vic == prxcap->VIC[i]) {
			already = 1;
			break;
		}
	}
	if (!already) {
		prxcap->VIC[prxcap->VIC_count] = vic;
		prxcap->VIC_count++;
	}
}

static void store_y420_idx(struct rx_cap *prxcap, enum hdmi_vic vic)
{
	int i;
	int already = 0;

	if (!prxcap)
		return;

	/* don't support 64:27 aspect ratio */
	if (meson_tx_mode_get_vic_aspect_ratio(vic) == HDMI_PICTURE_ASPECT_64_27) {
		pr_debug("aspect ratio is 64:27, vic=%d bypass\n", vic);
		return;
	}

	/* Y420 is claimed in Y420VDB, y420_vic[] will list in dc_cap */
	for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
		if (vic == prxcap->y420_vic[i]) {
			already = 1;
			break;
		}
	}
	if (!already) {
		for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
			if (prxcap->y420_vic[i] == 0) {
				prxcap->y420_vic[i] = vic;
				break;
			}
		}
	}
}

static void edid_established_timings(struct rx_cap *prxcap, u8 *data)
{
	if (!prxcap || !data)
		return;

	if (data[0] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_800x600p60hz);
	if (data[1] & (1 << 3))
		store_vesa_idx(prxcap, HDMIV_1024x768p60hz);
}

static void edid_standard_timing_iii(struct rx_cap *prxcap, u8 *data)
{
	if (!prxcap || !data)
		return;

	if (data[0] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_1152x864p75hz);
	if (data[1] & (1 << 6))
		store_vesa_idx(prxcap, HDMIV_1280x768p60hz);
	if (data[1] & (1 << 2))
		store_vesa_idx(prxcap, HDMIV_1280x960p60hz);
	if (data[1] & (1 << 1))
		store_vesa_idx(prxcap, HDMIV_1280x1024p60hz);
	if (data[2] & (1 << 7))
		store_vesa_idx(prxcap, HDMIV_1360x768p60hz);
	if (data[2] & (1 << 1))
		store_vesa_idx(prxcap, HDMIV_1400x1050p60hz);
	if (data[3] & (1 << 5))
		store_vesa_idx(prxcap, HDMIV_1680x1050p60hz);
	if (data[3] & (1 << 2))
		store_vesa_idx(prxcap, HDMIV_1600x1200p60hz);
	if (data[4] & (1 << 0))
		store_vesa_idx(prxcap, HDMIV_1920x1200p60hz);
}

static void calc_timing(struct rx_cap *prxcap, u8 *data, struct vesa_standard_timing *t)
{
	const struct tx_timing *standard_timing = NULL;

	if (!prxcap || !data || !t)
		return;
	if (data[0] < 2 && data[1] < 2)
		return;

	t->hactive = (data[0] + 31) * 8;
	switch ((data[1] >> 6) & 0x3) {
	case 0:
		t->vactive = t->hactive * 5 / 8;
		break;
	case 1:
		t->vactive = t->hactive * 3 / 4;
		break;
	case 2:
		t->vactive = t->hactive * 4 / 5;
		break;
	case 3:
	default:
		t->vactive = t->hactive * 9 / 16;
		break;
	}
	t->vsync = (data[1] & 0x3f) + 60;
	standard_timing = meson_tx_mode_match_vesa_timing(t);
	if (standard_timing) {
		/* prefer 16x9 mode */
		if (standard_timing->vic == HDMI_6_720x480i60_4x3 ||
			standard_timing->vic == HDMI_2_720x480p60_4x3 ||
			standard_timing->vic == HDMI_21_720x576i50_4x3 ||
			standard_timing->vic == HDMI_17_720x576p50_4x3)
			t->vesa_timing = standard_timing->vic + 1;
		else
			t->vesa_timing = standard_timing->vic;

		if (t->vesa_timing < HDMITX_VESA_OFFSET) {
			/*
			 * for compliance with uboot, don't
			 * save CEA mode in standard_timing block.
			 * uboot don't parse standard_timing block
			 */
			/* store_cea_idx(prxcap, t->vesa_timing); */
		} else {
			store_vesa_idx(prxcap, t->vesa_timing);
		}
	}
}

static void edid_standard_timing(struct rx_cap *prxcap, u8 *data,
				int max_num)
{
	int i;
	struct vesa_standard_timing timing;

	if (!prxcap || !data)
		return;
	for (i = 0; i < max_num; i++) {
		memset(&timing, 0, sizeof(struct vesa_standard_timing));
		calc_timing(prxcap, &data[i * 2], &timing);
	}
}

static void edid_receiverproductnameparse(struct rx_cap *prxcap,
					  u8 *data)
{
	int i = 0;

	if (!prxcap || !data)
		return;
	/*
	 * ASCII coded monitor name (13 bytes max)
	 * if name < 13 bytes, terminate name with 0x0A
	 * and fill remainder of 13 bytes with 0x20
	 */
	while ((i < 13) && (data[i] != 0x0a)) {
		prxcap->ReceiverProductName[i] = data[i];
		i++;
	}
	prxcap->ReceiverProductName[i] = '\0';
}

/* ----------------------------------------------------------- */
static void edid_parseceatiming(struct rx_cap *prxcap,
	unsigned char *buff)
{
	int i;
	unsigned char *dtd_base = buff;

	if (!prxcap || !buff)
		return;

	for (i = 0; i < 4; i++) {
		edid_dtd_parsing(prxcap, dtd_base);
		dtd_base += 0x12;
	}
}

static void set_vsdb_dc_cap(struct rx_cap *prxcap)
{
	if (!prxcap)
		return;

	prxcap->dc_y444 = !!(prxcap->ColorDeepSupport & (1 << 3));
	prxcap->dc_30bit = !!(prxcap->ColorDeepSupport & (1 << 4));
	prxcap->dc_36bit = !!(prxcap->ColorDeepSupport & (1 << 5));
	prxcap->dc_48bit = !!(prxcap->ColorDeepSupport & (1 << 6));
}

static void _edid_parsingvendspec(struct dv_info *dv,
				  struct hdr10_plus_info *hdr10_plus,
				  struct cuva_info *cuva,
				 u8 *buf)
{
	u8 *dat = buf;
	u8 *cuva_dat = buf;
	u8 pos = 0;
	u32 ieeeoui = 0;
	u8 length = 0;

	if (!dv || !hdr10_plus || !cuva || !buf)
		return;

	length = dat[pos] & 0x1f;
	pos++;

	if (dat[pos] != 1) {
		pr_err("parsing fail %s[%d]\n", __func__,
			__LINE__);
		return;
	}

	pos++;
	ieeeoui = dat[pos++];
	ieeeoui += dat[pos++] << 8;
	ieeeoui += dat[pos++] << 16;
	pr_debug("Edid_ParsingVendSpec:ieeeoui=0x%x,len=%u\n", ieeeoui, length);

	/* HDR10+ use vsvdb */
	if (ieeeoui == HDR10_PLUS_IEEE_OUI) {
		memset(hdr10_plus, 0, sizeof(struct hdr10_plus_info));
		hdr10_plus->length = length;
		hdr10_plus->ieeeoui = ieeeoui;
		hdr10_plus->application_version = dat[pos] & 0x3;
		pos++;
		return;
	}
	if (ieeeoui == CUVA_IEEEOUI) {
		/* 15, fixed length */
		memcpy(cuva->rawdata, cuva_dat, 15);
		cuva->length = cuva_dat[0] & 0x1f;
		cuva->ieeeoui = cuva_dat[2] |
				(cuva_dat[3] << 8) |
				(cuva_dat[4] << 16);
		cuva->system_start_code = cuva_dat[5];
		cuva->version_code = cuva_dat[6] >> 4;
		cuva->display_max_lum = cuva_dat[7] |
					(cuva_dat[8] << 8) |
					(cuva_dat[9] << 16) |
					(cuva_dat[10] << 24);
		cuva->display_min_lum = cuva_dat[11] | (cuva_dat[12] << 8);
		cuva->rx_mode_sup = (cuva_dat[13] >> 6) & 0x1;
		cuva->monitor_mode_sup = (cuva_dat[13] >> 7) & 0x1;
		return;
	}

	if (ieeeoui == DV_IEEE_OUI) {
		/* it is a Dovi block */
		memset(dv, 0, sizeof(struct dv_info));
		dv->block_flag = CORRECT;
		dv->length = length;
		memcpy(dv->rawdata, dat, dv->length + 1);
		dv->ieeeoui = ieeeoui;
		dv->ver = (dat[pos] >> 5) & 0x7;
		if (dv->ver > 2) {
			dv->block_flag = ERROR_VER;
			return;
		}
		/* Refer to DV 2.9 Page 27 */
		if (dv->ver == 0) {
			if (dv->length == 0x19) {
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
				pos++;
				dv->Rx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Ry =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Gx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Gy =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Bx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->By =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->Wx =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->Wy =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->tminPQ =
					(dat[pos + 1] << 4) | (dat[pos] >> 4);
				dv->tmaxPQ =
					(dat[pos + 2] << 4) | (dat[pos] & 0xf);
				pos += 3;
				dv->dm_major_ver = dat[pos] >> 4;
				dv->dm_minor_ver = dat[pos] & 0xf;
				pos++;
				pr_debug("v0 VSVDB: len=%d, sup_2160p60hz=%d\n",
					dv->length, dv->sup_2160p60hz);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}

		if (dv->ver == 1) {
			if (dv->length == 0x0B) {
				/* Refer to DV 2.9 Page 33 */
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = dat[pos] & 0x1;
				dv->tmax_lum = dat[pos] >> 1;
				pos++;
				dv->colorimetry = dat[pos] & 0x1;
				dv->tmin_lum = dat[pos] >> 1;
				pos++;
				dv->low_latency = dat[pos] & 0x3;
				dv->Bx = 0x20 | ((dat[pos] >> 5) & 0x7);
				dv->By = 0x08 | ((dat[pos] >> 2) & 0x7);
				pos++;
				dv->Gx = 0x00 | (dat[pos] >> 1);
				dv->Ry = 0x40 | ((dat[pos] & 0x1) |
					((dat[pos + 1] & 0x1) << 1) |
					((dat[pos + 2] & 0x3) << 2));
				pos++;
				dv->Gy = 0x80 | (dat[pos] >> 1);
				pos++;
				dv->Rx = 0xA0 | (dat[pos] >> 3);
				pos++;
				pr_debug("v1 VSVDB: len=%d, sup_2160p60hz=%d, ll=%d\n",
					dv->length, dv->sup_2160p60hz, dv->low_latency);
			} else if (dv->length == 0x0E) {
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_2160p60hz = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = dat[pos] & 0x1;
				dv->tmax_lum = dat[pos] >> 1;
				pos++;
				dv->colorimetry = dat[pos] & 0x1;
				dv->tmin_lum = dat[pos] >> 1;
				/* byte8 is reserved as 0 */
				pos += 2;
				dv->Rx = dat[pos++];
				dv->Ry = dat[pos++];
				dv->Gx = dat[pos++];
				dv->Gy = dat[pos++];
				dv->Bx = dat[pos++];
				dv->By = dat[pos++];
				pr_debug("v1 VSVDB: len=%d, sup_2160p60hz=%d\n",
					dv->length, dv->sup_2160p60hz);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}
		if (dv->ver == 2) {
			/*
			 * v2 VSVDB length could be greater than 0xB
			 * and should not be treated as unrecognized
			 * block. Instead, we should parse it as a regular
			 * v2 VSVDB using just the remaining 11 bytes here
			 */
			if (dv->length >= 0x0B) {
				/* default */
				dv->sup_2160p60hz = 0x1;
				dv->dm_version = (dat[pos] >> 2) & 0x7;
				dv->sup_yuv422_12bit = dat[pos] & 0x1;
				dv->sup_backlight_control = (dat[pos] >> 1) & 0x1;
				pos++;
				dv->sup_global_dimming = (dat[pos] >> 2) & 0x1;
				dv->backlt_min_luma = dat[pos] & 0x3;
				dv->tminPQ = dat[pos] >> 3;
				pos++;
				dv->Interface = dat[pos] & 0x3;
				dv->parity = (dat[pos] >> 2) & 0x1;
				/* if parity = 0, then not support > 60hz nor 8k */
				dv->sup_1080p120hz = dv->parity;
				dv->tmaxPQ = dat[pos] >> 3;
				pos++;
				dv->sup_10b_12b_444 = ((dat[pos] & 0x1) << 1) |
					(dat[pos + 1] & 0x1);
				dv->Gx = 0x00 | (dat[pos] >> 1);
				pos++;
				dv->Gy = 0x80 | (dat[pos] >> 1);
				pos++;
				dv->Rx = 0xA0 | (dat[pos] >> 3);
				dv->Bx = 0x20 | (dat[pos] & 0x7);
				pos++;
				dv->Ry = 0x40  | (dat[pos] >> 3);
				dv->By = 0x08  | (dat[pos] & 0x7);
				pos++;
				pr_debug("v2 VSVDB: len=%d, 2160p60hz=%d, Interface=%d\n",
					dv->length, dv->sup_2160p60hz, dv->Interface);
			} else {
				dv->block_flag = ERROR_LENGTH;
			}
		}

		if (pos > (dv->length + 1))
			pr_debug("hdmitx: maybe invalid dv%d data\n", dv->ver);
		return;
	}
	/* future: other new VSVDB add here: */
}

static void edid_parsingvendspec(struct rx_cap *prxcap, u8 *buf)
{
	struct dv_info *dv;
	struct hdr10_plus_info *hdr10_plus;
	struct cuva_info *cuva;

	u8 pos = 0;
	u32 ieeeoui = 0;

	if (!prxcap || !buf)
		return;

	dv = &prxcap->dv_info;
	hdr10_plus = &prxcap->hdr_info.hdr10plus_info;
	cuva = &prxcap->hdr_info.cuva_info;
	pos++;

	if (buf[pos] != 1) {
		pr_err("parsing fail %s[%d]\n", __func__, __LINE__);
		return;
	}

	pos++;
	ieeeoui = buf[pos++];
	ieeeoui += buf[pos++] << 8;
	ieeeoui += buf[pos++] << 16;

	_edid_parsingvendspec(dv, hdr10_plus, cuva, buf);
}

/* check vic support y420 or not */
bool meson_tx_validate_y420_vic(enum hdmi_vic vic)
{
	const struct tx_timing *timing;

	/* In Spec2.1 Table 7-34, greater than 2160p30hz will support y420 */
	timing = meson_tx_mode_vic_to_timing(vic);
	if (!timing)
		return false;
	if (timing->v_active >= 2160 && timing->v_freq > 30000)
		return true;
	if (timing->v_active >= 4320)
		return true;

	return false;
}

/* ----------------------------------------------------------- */
static int edid_parsingy420vdb(struct rx_cap *prxcap, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;

	if (!prxcap || !buf)
		return -1;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f) + 1;
	pos++;
	ext_tag = buf[pos];

	if (tag != 0x7 || ext_tag != 0xe)
		goto INVALID_Y420VDB;

	pos++;
	while (pos < data_end) {
		if (prxcap->VIC_count < VIC_MAX_NUM) {
			if (meson_tx_validate_y420_vic(buf[pos])) {
				store_cea_idx(prxcap, buf[pos]);
				store_y420_idx(prxcap, buf[pos]);
			}
		}
		pos++;
	}

	return 0;

INVALID_Y420VDB:
	pr_err("[%s] it's not a valid y420vdb!\n", __func__);
	return -1;
}

static int _edid_parsedrmsb(struct hdr_info *info, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;

	if (!info || !buf)
		return -1;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f);
	memset(info->rawdata, 0, 7);
	memcpy(info->rawdata, buf, data_end + 1);
	pos++;
	ext_tag = buf[pos];
	if (tag != HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG ||
	    ext_tag != EXTENSION_DRM_STATIC_TAG)
		goto INVALID_DRM_STATIC;
	pos++;
	info->hdr_support = buf[pos];
	pos++;
	info->static_metadata_type1 = buf[pos] & 0x1;
	pos++;
	if (data_end == 3)
		return 0;
	if (data_end == 4) {
		info->lumi_max = buf[pos];
		return 0;
	}
	if (data_end == 5) {
		info->lumi_max = buf[pos];
		info->lumi_avg = buf[pos + 1];
		return 0;
	}
	if (data_end == 6) {
		info->lumi_max = buf[pos];
		info->lumi_avg = buf[pos + 1];
		info->lumi_min = buf[pos + 2];
		return 0;
	}
	return 0;
INVALID_DRM_STATIC:
	pr_err("[%s] it's not a valid DRM STATIC BLOCK\n", __func__);
	return -1;
}

static int edid_parsedrmsb(struct rx_cap *prxcap, u8 *buf)
{
	struct hdr_info *hdr;

	if (!prxcap || !buf)
		return -1;

	hdr = &prxcap->hdr_info;
	_edid_parsedrmsb(hdr, buf);
	return 0;
}

static int _edid_parsedrmdb(struct hdr_info *info, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, data_end = 0;
	u32 pos = 0;
	u32 type;
	u32 type_length;
	u32 i;
	u32 num;

	if (!info || !buf)
		return -1;

	tag = (buf[pos] >> 5) & 0x7;
	data_end = (buf[pos] & 0x1f);
	pos++;
	ext_tag = buf[pos];
	if (tag != HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG ||
	    ext_tag != EXTENSION_DRM_DYNAMIC_TAG)
		goto INVALID_DRM_DYNAMIC;
	pos++;
	data_end--;

	while (data_end) {
		type_length = buf[pos];
		pos++;
		type = (buf[pos + 1] << 8) | buf[pos];
		pos += 2;
		switch (type) {
		case TS_103_433_SPEC_TYPE:
			num = 1;
			break;
		case ITU_T_H265_SPEC_TYPE:
			num = 2;
			break;
		case TYPE_4_HDR_METADATA_TYPE:
			num = 3;
			break;
		case TYPE_1_HDR_METADATA_TYPE:
		default:
			num = 0;
			break;
		}
		info->dynamic_info[num].of_len = type_length;
		info->dynamic_info[num].type = type;
		info->dynamic_info[num].support_flags = buf[pos];
		pos++;
		for (i = 0; i < type_length - 3; i++) {
			info->dynamic_info[num].optional_fields[i] =
			buf[pos];
			pos++;
		}
		if (data_end >= (type_length + 1))
			data_end = data_end - (type_length + 1);
		else
			data_end = 0;
	}

	return 0;
INVALID_DRM_DYNAMIC:
	pr_err("[%s] it's not a valid DRM DYNAMIC BLOCK\n", __func__);
	return -1;
}

static int edid_parsedrmdb(struct rx_cap *prxcap, u8 *buf)
{
	struct hdr_info *hdr;

	if (!prxcap || !buf)
		return -1;

	hdr = &prxcap->hdr_info;
	_edid_parsedrmdb(hdr, buf);
	return 0;
}

static int edid_parsingvfpdb(struct rx_cap *prxcap, u8 *buf)
{
	u32 len;
	enum hdmi_vic svr = HDMI_0_UNKNOWN;

	if (!prxcap || !buf)
		return 0;

	len = buf[0] & 0x1f;
	if (buf[1] != EXTENSION_VFPDB_TAG)
		return 0;
	if (len < 2)
		return 0;

	svr = buf[2];
	if ((svr >= 1 && svr <= 127) ||
	    (svr >= 193 && svr <= 253)) {
		prxcap->flag_vfpdb = 1;
		prxcap->preferred_mode = svr;
		pr_debug("preferred mode 0 srv %d\n", prxcap->preferred_mode);
		return 1;
	}
	if (svr >= 129 && svr <= 144) {
		prxcap->flag_vfpdb = 1;
		prxcap->preferred_mode = prxcap->dtd[svr - 129].vic;
		pr_debug("preferred mode 0 dtd %d\n", prxcap->preferred_mode);
		return 1;
	}
	return 0;
}

/* ----------------------------------------------------------- */
static int edid_parsingy420cmdb(struct rx_cap *prxcap, u8 *buf)
{
	u8 tag = 0, ext_tag = 0, length = 0, data_end = 0;
	u32 pos = 0, i = 0;

	if (!prxcap || !buf)
		return -1;

	tag = (buf[pos] >> 5) & 0x7;
	length = buf[pos] & 0x1f;
	data_end = length + 1;
	pos++;
	ext_tag = buf[pos];

	if (tag != 0x7 || ext_tag != 0xf)
		goto INVALID_Y420CMDB;

	if (length == 1) {
		prxcap->y420_all_vic = 1;
		return 0;
	}

	prxcap->bitmap_length = 0;
	prxcap->bitmap_valid = 0;
	memset(prxcap->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);

	pos++;
	if (pos < data_end) {
		prxcap->bitmap_length = data_end - pos;
		prxcap->bitmap_valid = 1;
	}
	while (pos < data_end) {
		if (i < Y420CMDB_MAX)
			prxcap->y420cmdb_bitmap[i] = buf[pos];
		pos++;
		i++;
	}

	return 0;

INVALID_Y420CMDB:
	pr_err("[%s] it's not a valid y420cmdb!\n", __func__);
	return -1;
}

static void edid_parsingdolbyvsadb(struct rx_cap *prxcap, unsigned char *buf)
{
	unsigned char length = 0;
	unsigned char pos = 0;
	unsigned int ieeeoui;
	struct dolby_vsadb_cap *cap;

	if (!prxcap || !buf)
		return;

	cap = &prxcap->dolby_vsadb_cap;
	memset(cap->rawdata, 0, sizeof(cap->rawdata));
	/* fixed 7 bytes */
	memcpy(cap->rawdata, buf, 7);

	pos = 0;
	length = buf[pos] & 0x1f;
	if (length != 0x06)
		pr_debug("%s[%d]: the length is %d, should be 6 bytes\n",
			__func__, __LINE__, length);

	cap->length = length;
	pos += 2;
	ieeeoui = buf[pos] + (buf[pos + 1] << 8) + (buf[pos + 2] << 16);
	if (ieeeoui != DOVI_IEEEOUI)
		pr_debug("%s[%d]: the ieeeoui is 0x%x, should be 0x%x\n",
			__func__, __LINE__, ieeeoui, DOVI_IEEEOUI);
	cap->ieeeoui = ieeeoui;

	pos += 3;
	cap->dolby_vsadb_ver = buf[pos] & 0x7;
	if (cap->dolby_vsadb_ver)
		pr_debug("%s[%d]: the version is 0x%x, should be 0x0\n",
			__func__, __LINE__, cap->dolby_vsadb_ver);

	cap->spk_center = (buf[pos] >> 4) & 1;
	cap->spk_surround = (buf[pos] >> 5) & 1;
	cap->spk_height = (buf[pos] >> 6) & 1;
	cap->headphone_only = (buf[pos] >> 7) & 1;

	pos++;
	cap->mat_48k_pcm_only = (buf[pos] >> 0) & 1;
}

static bool edid_y420cmdb_fill_all_vic(struct rx_cap *rxcap)
{
	u32 count;
	u32 a, b;

	if (!rxcap)
		return false;

	count = rxcap->SVD_VIC_count;

	if (rxcap->y420_all_vic != 1)
		return false;

	a = count / 8;
	a = (a >= Y420CMDB_MAX) ? Y420CMDB_MAX : a;
	b = count % 8;

	if (a > 0)
		memset(&rxcap->y420cmdb_bitmap[0], 0xff, a);

	if (b != 0 && a < Y420CMDB_MAX)
		rxcap->y420cmdb_bitmap[a] = (1 << b) - 1;

	rxcap->bitmap_length = (b == 0) ? a : (a + 1);
	rxcap->bitmap_valid = (rxcap->bitmap_length != 0) ? 1 : 0;

	return true;
}

static bool edid_y420cmdb_postprocess(struct rx_cap *rxcap)
{
	u32 i = 0, j = 0, valid = 0;
	u8 *p = NULL;
	enum hdmi_vic vic;

	if (!rxcap)
		return false;

	if (rxcap->y420_all_vic == 1)
		edid_y420cmdb_fill_all_vic(rxcap);

	if (rxcap->bitmap_valid == 0)
		goto PROCESS_END;

	for (i = 0; i < rxcap->bitmap_length; i++) {
		p = &rxcap->y420cmdb_bitmap[i];
		for (j = 0; j < 8; j++) {
			valid = ((*p >> j) & 0x1);
			vic = rxcap->SVD_VIC[i * 8 + j];
			if (valid != 0 &&
			    meson_tx_validate_y420_vic(vic)) {
				store_y420_idx(rxcap, vic);
			}
		}
	}
	return true;
PROCESS_END:
	return false;
}

static void edid_y420cmdb_reset(struct rx_cap *prxcap)
{
	if (!prxcap)
		return;

	prxcap->bitmap_valid = 0;
	prxcap->bitmap_length = 0;
	prxcap->y420_all_vic = 0;
	memset(prxcap->y420cmdb_bitmap, 0x00, Y420CMDB_MAX);
}

/* ----------------------------------------------------------- */
static void hdmitx_3d_update(struct rx_cap *prxcap)
{
	int j = 0;

	if (!prxcap)
		return;

	for (j = 0; j < 16; j++) {
		if ((prxcap->threeD_MASK_15_0 >> j) & 0x1) {
			/* frame packing */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 0))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.frame_packing = 1;
			/* top and bottom */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 6))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.top_and_bottom = 1;
			/* top and bottom */
			if (prxcap->threeD_Structure_ALL_15_0
				& (1 << 8))
				prxcap->support_3d_format[prxcap->VIC[j]]
				.side_by_side = 1;
		}
	}
}

/* parse Sink 3D information */
static bool tx_edid_3d_parse(struct rx_cap *prxcap, u8 *dat,
				u32 size)
{
	int j = 0;
	u32 base = 0;
	u32 pos = base + 1;
	u32 index = 0;

	if (!prxcap || !dat)
		return false;

	if (dat[base] & (1 << 7))
		pos += 2;
	if (dat[base] & (1 << 6))
		pos += 2;
	if (dat[base] & (1 << 5)) {
		prxcap->threeD_present = dat[pos] >> 7;
		prxcap->threeD_Multi_present = (dat[pos] >> 5) & 0x3;
		pos += 1;
		prxcap->hdmi_vic_LEN = dat[pos] >> 5;
		prxcap->HDMI_3D_LEN = dat[pos] & 0x1f;
		pos += prxcap->hdmi_vic_LEN + 1;

		if (prxcap->threeD_Multi_present == 0x01) {
			prxcap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos + 1];
			prxcap->threeD_MASK_15_0 = 0;
			pos += 2;
		}
		if (prxcap->threeD_Multi_present == 0x02) {
			prxcap->threeD_Structure_ALL_15_0 =
				(dat[pos] << 8) + dat[pos + 1];
			pos += 2;
			prxcap->threeD_MASK_15_0 =
			(dat[pos] << 8) + dat[pos + 1];
			pos += 2;
		}
	}
	while (pos < size) {
		if ((dat[pos] & 0xf) < 0x8) {
			/* frame packing */
			if ((dat[pos] & 0xf) == T3D_FRAME_PACKING)
				prxcap->support_3d_format[prxcap->VIC[((dat[pos]
					& 0xf0) >> 4)]].frame_packing = 1;
			/* top and bottom */
			if ((dat[pos] & 0xf) == T3D_TAB)
				prxcap->support_3d_format[prxcap->VIC[((dat[pos]
					& 0xf0) >> 4)]].top_and_bottom = 1;
			pos += 1;
		} else {
			/* SidebySide */
			if ((dat[pos] & 0xf) == T3D_SBS_HALF &&
			    (dat[pos + 1] >> 4) < 0xb) {
				index = (dat[pos] & 0xf0) >> 4;
				prxcap->support_3d_format[prxcap->VIC[index]]
				.side_by_side = 1;
			}
			pos += 2;
		}
	}
	if (prxcap->threeD_MASK_15_0 == 0) {
		for (j = 0; (j < 16) && (j < prxcap->VIC_count); j++) {
			prxcap->support_3d_format[prxcap->VIC[j]]
			.frame_packing = 1;
			prxcap->support_3d_format[prxcap->VIC[j]]
			.top_and_bottom = 1;
			prxcap->support_3d_format[prxcap->VIC[j]]
			.side_by_side = 1;
		}
	} else {
		hdmitx_3d_update(prxcap);
	}
	return true;
}

/* parse Sink 4k2k information */
static void tx_edid_4k2k_parse(struct rx_cap *prxcap, u8 *dat,
				   u32 size)
{
	if (!prxcap || !dat)
		return;

	if (size > 4 || size == 0) {
		pr_debug("4k2k in edid out of range, SIZE = %d\n", size);
		return;
	}
	while (size--) {
		if (*dat == 1)
			store_cea_idx(prxcap, HDMI_95_3840x2160p30_16x9);
		else if (*dat == 2)
			store_cea_idx(prxcap, HDMI_94_3840x2160p25_16x9);
		else if (*dat == 3)
			store_cea_idx(prxcap, HDMI_93_3840x2160p24_16x9);
		else if (*dat == 4)
			store_cea_idx(prxcap, HDMI_98_4096x2160p24_256x135);
		dat++;
	}
}

static void get_latency(struct rx_cap *prxcap, u8 *val)
{
	if (!prxcap || !val)
		return;

	if (val[0] == 0)
		prxcap->vLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[0] == 0xFF)
		prxcap->vLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->vLatency = (val[0] - 1) * 2;

	if (val[1] == 0)
		prxcap->aLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[1] == 0xFF)
		prxcap->aLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->aLatency = (val[1] - 1) * 2;
}

static void get_ilatency(struct rx_cap *prxcap, u8 *val)
{
	if (!prxcap || !val)
		return;

	if (val[0] == 0)
		prxcap->i_vLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[0] == 0xFF)
		prxcap->i_vLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->i_vLatency = val[0] * 2 - 1;

	if (val[1] == 0)
		prxcap->i_aLatency = LATENCY_INVALID_UNKNOWN;
	else if (val[1] == 0xFF)
		prxcap->i_aLatency = LATENCY_NOT_SUPPORT;
	else
		prxcap->i_aLatency = val[1] * 2 - 1;
}

static void tx_edid_parse_hdmi14(struct rx_cap *prxcap,
	u8 offset, u8 *block_buf, u8 count)
{
	int idx = 0, tmp = 0;

	if (!prxcap || !block_buf)
		return;

	prxcap->ieeeoui = HDMI_IEEE_OUI;

	prxcap->ColorDeepSupport = (count > 5) ? block_buf[offset + 5] : 0;
	set_vsdb_dc_cap(prxcap);
	prxcap->Max_TMDS_Clock1 =
		(count > 6) ? block_buf[offset + 6] : DEFAULT_MAX_TMDS_CLK;
	if (count > 7) {
		tmp = block_buf[offset + 7];
		idx = offset + 8;
		if (tmp & (1 << 6)) {
			u8 val[2];

			val[0] = block_buf[idx];
			val[1] = block_buf[idx + 1];
			get_latency(prxcap, val);
			idx += 2;
			val[0] = block_buf[idx];
			val[1] = block_buf[idx + 1];
			get_ilatency(prxcap, val);
			idx += 2;
		} else if (tmp & (1 << 7)) {
			unsigned char val[2];

			val[0] = block_buf[idx];
			val[1] = block_buf[idx + 1];
			get_latency(prxcap, val);
			idx += 2;
		}
		prxcap->cnc0 = (tmp >> 0) & 1;
		prxcap->cnc1 = (tmp >> 1) & 1;
		prxcap->cnc2 = (tmp >> 2) & 1;
		prxcap->cnc3 = (tmp >> 3) & 1;
		if (tmp & (1 << 5)) {
			idx += 1;
			/* valid 4k */
			if (block_buf[idx] & 0xe0) {
				tx_edid_4k2k_parse(prxcap,
						       &block_buf[idx + 1],
				block_buf[idx] >> 5);
			}
			/* valid 3D */
			if (block_buf[idx - 1] & 0xe0) {
				tx_edid_3d_parse(prxcap, &block_buf[offset + 7],
				count - 7);
			}
		}
	}
}

static void tx_edid_parse_ifdb(struct rx_cap *prxcap, u8 *blockbuf)
{
	u8 payload_len;
	u8 len;
	u8 sum_len = 0;

	if (!prxcap || !blockbuf)
		return;

	payload_len = blockbuf[0] & 0x1f;
	/* no additional bytes after extended tag code */
	if (payload_len <= 1)
		return;

	/* begin with an InfoFrame Processing Descriptor */
	if ((blockbuf[2] & 0x1f) != 0)
		pr_err("ERR: IFDB not begin with InfoFrame Processing Descriptor\n");
	/* Extended Tag Code len */
	sum_len = 1;

	len = (blockbuf[2] >> 5) & 0x7;
	sum_len += (len + 2);
	if (payload_len < sum_len) {
		pr_err("ERR: IFDB length abnormal: %d exceed playload len %d\n",
			sum_len, payload_len);
		return;
	}

	prxcap->additional_vsif_num = blockbuf[3];
	if (payload_len == sum_len)
		return;

	while (sum_len < payload_len) {
		if ((blockbuf[sum_len + 1] & 0x1f) == 1) {
			/* Short Vendor-Specific InfoFrame Descriptor */
			/* pr_info(EDID "InfoFrame Type Code: 0x1, len: %d, IEEE: %x\n", */
				/* len, blockbuf[sum_len + 4] << 16 | */
				/* blockbuf[sum_len + 3] << 8 | blockbuf[sum_len + 2]); */
			/* number of additional bytes following the 3-byte OUI */
			len = (blockbuf[sum_len + 1] >> 5) & 0x7;
			sum_len += (len + 1 + 3);
		} else {
			/* Short InfoFrame Descriptor */
			/* pr_info(EDID "InfoFrame Type Code: %x, len: %d\n", */
				/* blockbuf[sum_len + 1] & 0x1f, len); */
			len = (blockbuf[sum_len + 1] >> 5) & 0x7;
			sum_len += (len + 1);
		}
	}
}

static void tx_edid_parse_hf_scdb(struct rx_cap *prxcap,
	u8 *blockbuf, u8 count)
{
	if (!prxcap || !blockbuf)
		return;

	/* the minimum length of the SCDS is 4, and the maximum length is 28 */
	if (count < 4)
		return;
	prxcap->hf_ieeeoui = HDMI_FORUM_IEEE_OUI;
	prxcap->Max_TMDS_Clock2 = blockbuf[1];
	prxcap->scdc_present = !!(blockbuf[2] & (1 << 7));
	prxcap->scdc_rr_capable = !!(blockbuf[2] & (1 << 6));
	prxcap->lte_340mcsc_scramble = !!(blockbuf[2] & (1 << 3));
	prxcap->max_frl_rate = (blockbuf[3] & 0xf0) >> 4;
	prxcap->dc_30bit_420 = !!(blockbuf[3] & (1 << 0));
	prxcap->dc_36bit_420 = !!(blockbuf[3] & (1 << 1));
	prxcap->dc_48bit_420 = !!(blockbuf[3] & (1 << 2));

	if (count < 5)
		return;
	prxcap->fapa_start_loc = !!(blockbuf[4] & (1 << 0));
	prxcap->allm = !!(blockbuf[4] & (1 << 1));
	prxcap->fva = !!(blockbuf[4] & (1 << 2));
	prxcap->neg_mvrr = !!(blockbuf[4] & (1 << 3));
	prxcap->mdelta = !!(blockbuf[4] & (1 << 5));
	prxcap->qms = !!(blockbuf[4] & (1 << 6));
	prxcap->fapa_end_extended = !!(blockbuf[4] & (1 << 7));

	if (count < 6)
		return;
	/*
	 * HDMI_Spec_V2.1b. Chapter 6.5.1.4.5
	 * Values of 49~63 are reserved.
	 * Source shall interpret non-zero values higher than 48 as a value of 48
	 */
	/*
	 * GCTS 2.1j for HDMI 2.1 Sources r8.docx Table 4-145
	 * BVRR50-60：Bad VRRMIN; VRR disabled.
	 * The HDMI_Spec_V2.1b standard is used here
	 */
	prxcap->vrr_min = (blockbuf[5] & 0x3f);
	if (prxcap->vrr_min > 48) {
		pr_info("vrr_min is reserved value %d\n", prxcap->vrr_min);
		prxcap->vrr_min = 48;
	}

	if (count < 7)
		return;
	prxcap->vrr_max = (((blockbuf[5] & 0xc0) >> 6) << 8) +
				blockbuf[6];
	/*
	 * HDMI_Spec_V2.1b. Chapter 6.5.1.4.5
	 * Values of 1~99 are reserved.
	 * Source shall interpret non-zero values less than 100 as a value of 100
	 */
	/*
	 * GCTS 2.1j for HDMI 2.1 Sources r8.docx Table 4-145
	 * BVRR48-85:Bad VRRMAX —ignored. Range=48-BRR.
	 * The HDMI_Spec_V2.1b standard is used here
	 */
	if (prxcap->vrr_max > 0 && prxcap->vrr_max < 100) {
		pr_info("vrr_max is reserved value %d\n", prxcap->vrr_max);
		prxcap->vrr_max = 100;
	}

	if (count < 8)
		return;
	prxcap->dsc_10bpc = !!(blockbuf[7] & (1 << 0));
	prxcap->dsc_12bpc = !!(blockbuf[7] & (1 << 1));
	prxcap->dsc_16bpc = !!(blockbuf[7] & (1 << 2));
	prxcap->dsc_all_bpp = !!(blockbuf[7] & (1 << 3));
	prxcap->qms_tfr_min = !!(blockbuf[7] & (1 << 4));
	prxcap->qms_tfr_max = !!(blockbuf[7] & (1 << 5));
	prxcap->dsc_native_420 = !!(blockbuf[7] & (1 << 6));
	prxcap->dsc_1p2 = !!(blockbuf[7] & (1 << 7));
	/*
	 * dsc_1p2 shall be cleared (=0) for devices that
	 * do not support FRL (i.e. Max_FRL_Rate=0).
	 */
	if (prxcap->max_frl_rate == 0)
		prxcap->dsc_1p2 = 0;
	if (prxcap->dsc_1p2) {
		if (count < 10) {
			pr_info("error: dsc_1p2 support, but dsc not complete\n");
			prxcap->dsc_1p2 = 0;
			return;
		}
		/*
		 * 3: up to 4 slices and up to (340 MHz/K SliceAdjust)
		 * pixel clock per slice
		 * 4: up to 8 slices and up to (340 MHz/K SliceAdjust)
		 * pixel clock per slice
		 */
		prxcap->dsc_max_slices = blockbuf[8] & 0xf;
		/*
		 * This is value shall be the same or lower than
		 * the physical maximum rate specified by the
		 * Max_FRL_Rate field.
		 */
		prxcap->dsc_max_frl_rate = (blockbuf[8] >> 4) & 0xf;
		/*
		 * The number of bytes is computed as:
		 * 1024 x (1+DSC_ TotalChunkKBytes)
		 */
		prxcap->dsc_total_chunk_bytes = blockbuf[9] & 0x3f;
	}
}

static inline unsigned short get_2_bytes(u8 *addr)
{
	if (!addr)
		return 0;
	return addr[0] | (addr[1] << 8);
}

static void tx_edid_parse_sbtm_db(struct rx_cap *prxcap,
	u8 offset, u8 *blockbuf, u8 len)
{
	struct sbtm_info *info;

	if (!prxcap || !blockbuf || !len)
		return;
	if (len < 2)
		return;
	blockbuf = blockbuf + offset;
	/* length should be 2, 3, 5, 7, ... or 29 */
	if (!(len == 2 || (len <= 29 && ((len % 2) == 1))))
		pr_info("%s[%d] len is %d\n", __func__, __LINE__, len);
	info = &prxcap->hdr_info.sbtm_info;

	blockbuf++;
	len--;
	if (blockbuf[0])
		info->sbtm_support = 1;
	if (!info->sbtm_support)
		return;
	info->max_sbtm_ver = GET_BITS_FILED(blockbuf[0], 0, 4);
	info->grdm_support = GET_BITS_FILED(blockbuf[0], 5, 2);
	info->drdm_ind = GET_BITS_FILED(blockbuf[0], 7, 1);
	if (info->drdm_ind == 0 || len < 2)
		return;
	info->hgig_cat_drdm_sel = GET_BITS_FILED(blockbuf[1], 0, 3);
	info->use_hgig_drdm = GET_BITS_FILED(blockbuf[1], 4, 1);
	info->maxrgb = GET_BITS_FILED(blockbuf[1], 5, 1);
	info->gamut = GET_BITS_FILED(blockbuf[1], 6, 2);
	blockbuf += 2;
	len -= 2;
	if (info->drdm_ind && !info->gamut && len >= 16) {
		info->red_x = get_2_bytes(&blockbuf[0]);
		info->red_y = get_2_bytes(&blockbuf[2]);
		info->green_x = get_2_bytes(&blockbuf[4]);
		info->green_y = get_2_bytes(&blockbuf[6]);
		info->blue_x = get_2_bytes(&blockbuf[8]);
		info->blue_y = get_2_bytes(&blockbuf[10]);
		info->white_x = get_2_bytes(&blockbuf[12]);
		info->white_y = get_2_bytes(&blockbuf[14]);
		len -= 16;
		blockbuf += 16;
	}
	if (info->drdm_ind && !info->use_hgig_drdm && len >= 2) {
		info->min_bright_10 = blockbuf[0];
		info->peak_bright_100 = blockbuf[1];
		len -= 2;
		blockbuf += 2;
		if (len >= 2) {
			info->p0_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p0_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p0 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p1_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p1_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p1 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p2_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p2_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p2 = blockbuf[1];
			len -= 2;
			blockbuf += 2;
		}
		if (len >= 2) {
			info->p3_exp = GET_BITS_FILED(blockbuf[0], 0, 2);
			info->p3_mant = GET_BITS_FILED(blockbuf[0], 2, 6);
			info->peak_bright_p3 = blockbuf[1];
			/* end parsing */
		}
	}
}

/* refer to CEA-861-G 7.5.1 video data block */
static void _store_vics(struct rx_cap *prxcap, u8 vic_dat)
{
	u8 vic_bit6_0 = vic_dat & (~0x80);
	u8 vic_bit7 = !!(vic_dat & 0x80);

	if (!prxcap)
		return;

	if (vic_bit6_0 >= 1 && vic_bit6_0 <= 64) {
		prxcap->SVD_VIC[prxcap->SVD_VIC_count] = vic_bit6_0;
		prxcap->SVD_VIC_count++;
		/* don't support 640x480p60 */
		if (vic_bit6_0 > 1)
			store_cea_idx(prxcap, vic_bit6_0);
		if (vic_bit7) {
			if (prxcap->native_vic && !prxcap->native_vic2)
				prxcap->native_vic2 = vic_bit6_0;
			if (!prxcap->native_vic)
				prxcap->native_vic = vic_bit6_0;
		}
	} else {
		prxcap->SVD_VIC[prxcap->SVD_VIC_count] = vic_dat;
		prxcap->SVD_VIC_count++;
		store_cea_idx(prxcap, vic_dat);
	}
}

static int tx_edid_audio_block_parse(struct rx_cap *prxcap, u8 *block_buf)
{
	u8 offset, end;
	u8 count;
	u8 tag;
	int i, tmp, idx;

	if (!prxcap || !block_buf)
		return -1;

	/*
	 * CEA description
	 * When the descriptor offset is greater than 127,
	 * the CTA block needs to parse 127 bytes of valid data
	 */
	end = (block_buf[2] > 0x7f) ? 0x7f : (block_buf[2] & 0x7f);

	/* this loop should be parsing when revision number is larger than 2 */
	for (offset = 4 ; offset < end ; ) {
		tag = block_buf[offset] >> 5;
		count = block_buf[offset] & 0x1f;
		/* The maximum offset address range of cta data blocks is 4~126 */
		if (offset + count > 126)
			break;

		switch (tag) {
		case HDMI_EDID_BLOCK_TYPE_AUDIO:
			tmp = count / 3;
			idx = prxcap->AUD_count;
			prxcap->AUD_count += tmp;
			offset++;
			for (i = 0; i < tmp; i++) {
				prxcap->RxAudioCap[idx + i].audio_format_code =
					(block_buf[offset + i * 3] >> 3) & 0xf;
				prxcap->RxAudioCap[idx + i].channel_num_max =
					block_buf[offset + i * 3] & 0x7;
				prxcap->RxAudioCap[idx + i].freq_cc =
					block_buf[offset + i * 3 + 1] & 0x7f;
				prxcap->RxAudioCap[idx + i].cc3 =
					block_buf[offset + i * 3 + 2];
			}
			offset += count;
			break;
		default:
			offset++;
			offset += count;
			break;
		}
	}

	return 0;
}

int meson_tx_edid_audio_parse(struct rx_cap *prxcap, u8 *block_buf)
{
	int i;
	unsigned char cta_block_count;

	cta_block_count = tx_edid_get_cta_block_count(block_buf);
	for (i = 1; i <= cta_block_count; i++) {
		if (block_buf[i * 0x80] == 0x02 || (prxcap->edid_check & 0x01 &&
			block_buf[i * 0x80] != 0x70))
			tx_edid_audio_block_parse(prxcap, &block_buf[i * 0x80]);
	}
	/*
	 * CEA-861F 7.5.2  If only Basic Audio is supported,
	 * no Short Audio Descriptors are necessary.
	 */
	if (!prxcap->AUD_count)
		hdmitx_edid_set_default_aud(prxcap);
	hdmitx_edid_check_pcm_declare(prxcap);

	return 0;
}

static int tx_edid_cta_block_parse(struct rx_cap *prxcap, u8 *block_buf)
{
	u8 offset, end;
	u8 count;
	u8 tag;
	int i;
	u8 *vfpdb_offset = NULL;

	if (!prxcap || !block_buf)
		return -1;

	/*
	 * CEA description
	 * When the descriptor offset is greater than 127,
	 * the CTA block needs to parse 127 bytes of valid data
	 */
	end = (block_buf[2] > 0x7f) ? 0x7f : (block_buf[2] & 0x7f);
	prxcap->native_Mode = block_buf[1] >= 2 ? block_buf[3] : 0;
	prxcap->underscan = (prxcap->native_Mode & CAP_BIT7_UNDERSCAN_MASK) >> 7;
	prxcap->number_of_dtd += block_buf[1] >= 2 ? (block_buf[3] & CAP_LOWER_4BITS_DTD_MASK) : 0;
	/* Initialize SVD_VIC used for SVD storage in the video data block */
	prxcap->SVD_VIC_count = 0;
	memset(prxcap->SVD_VIC, 0, sizeof(prxcap->SVD_VIC));
	/*
	 * do not reset anything during parsing as there could be
	 * more than one block. Below variable should be reset once
	 * before parsing and are already being reset before parse
	 * call
	 */
	/* prxcap->native_vic = 0;*/
	/* prxcap->native_vic2 = 0;*/
	/* prxcap->AUD_count = 0;*/

	edid_y420cmdb_reset(prxcap);

	if (block_buf[1] <= 2) {
		/* skip below for loop */
		goto next;
	}
	/* this loop should be parsing when revision number is larger than 2 */
	for (offset = 4 ; offset < end ; ) {
		tag = block_buf[offset] >> 5;
		count = block_buf[offset] & 0x1f;
		/* The maximum offset address range of cta data blocks is 4~126 */
		if (offset + count > 126)
			break;

		switch (tag) {
		case HDMI_EDID_BLOCK_TYPE_AUDIO:
			/* The audio data block is parsed outside here */
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VIDEO:
			offset++;
			for (i = 0; i < count ; i++) {
				/*
				 * The SVD in the video data block is stored in SVD_VIC
				 * and mapped with 420 CMDB
				 */
				_store_vics(prxcap, block_buf[offset + i]);
			}
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VENDER:
			offset++;
			if (block_buf[offset] == 0x03 &&
			    block_buf[offset + 1] == 0x0c &&
			    block_buf[offset + 2] == 0x00) {
				tx_edid_parse_hdmi14(prxcap, offset,
							 block_buf, count);
			} else if ((block_buf[offset] == 0xd8) &&
				(block_buf[offset + 1] == 0x5d) &&
				(block_buf[offset + 2] == 0xc4)) {
				if (count < 3)
					break;
				tx_edid_parse_hf_scdb(prxcap, &block_buf[offset + 3], count - 3);
				if (prxcap->qms)
					pr_info("qms: qms/tfr_min/max/vrr_min/max %d %d %d %d %d\n",
						prxcap->qms,
						prxcap->qms_tfr_min, prxcap->qms_tfr_max,
						prxcap->vrr_min, prxcap->vrr_max);
			}
			/* ignore the remains. */
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_SPEAKER:
			offset++;
			prxcap->RxSpeakerAllocation = block_buf[offset];
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_VESA:
			offset++;
			offset += count;
			break;

		case HDMI_EDID_BLOCK_TYPE_EXTENDED_TAG:
			{
				u8 ext_tag = 0;

				ext_tag = block_buf[offset + 1];
				switch (ext_tag) {
				case EXTENSION_VIDEO_CAPABILITY_TAG:
					prxcap->video_capability_data = block_buf[offset + 2];
					break;
				case EXTENSION_VENDOR_SPECIFIC_TAG:
					edid_parsingvendspec(prxcap, &block_buf[offset]);
					break;
				case EXTENSION_COLORMETRY_TAG:
					prxcap->colorimetry_data =
						block_buf[offset + 2];
					prxcap->colorimetry_data2 =
						block_buf[offset + 2];
					break;
				case EXTENSION_DRM_STATIC_TAG:
					edid_parsedrmsb(prxcap,
							&block_buf[offset]);
					rx_set_hdr_lumi(&block_buf[offset],
							(block_buf[offset] &
							 0x1f) + 1);
					break;
				case EXTENSION_DRM_DYNAMIC_TAG:
					edid_parsedrmdb(prxcap, &block_buf[offset]);
					break;
				case EXTENSION_VFPDB_TAG:
/*
 * Just record VFPDB offset address, call Edid_ParsingVFPDB() after DTD
 * parsing, in case that
 * SVR >=129 and SVR <=144, Interpret as the Kth DTD in the EDID,
 * where K = SVR C 128 (for K=1 to 16)
 */
					vfpdb_offset = &block_buf[offset];
					break;
				case EXTENSION_Y420_VDB_TAG:
					edid_parsingy420vdb(prxcap, &block_buf[offset]);
					break;
				case EXTENSION_Y420_CMDB_TAG:
					edid_parsingy420cmdb(prxcap, &block_buf[offset]);
					break;
				case EXTENSION_SCDB_EXT_TAG:
					if (count < 3)
						break;
					tx_edid_parse_hf_scdb(prxcap, &block_buf[offset + 4],
						count - 3);
					break;
				case EXTENSION_SBTM_EXT_TAG:
					tx_edid_parse_sbtm_db(prxcap, offset + 1,
							 block_buf, count);
					break;
				case EXTENSION_DOLBY_VSADB:
					edid_parsingdolbyvsadb(prxcap, &block_buf[offset]);
					break;
				case EXTENSION_IFDB_TAG:
					prxcap->ifdb_present = true;
					tx_edid_parse_ifdb(prxcap, &block_buf[offset]);
					break;
				default:
					pr_info("warn: Unknown extended tag: %d, offset: %d\n",
						ext_tag, offset);
					break;
				}
			}
			offset += count + 1;
			break;

		case HDMI_EDID_BLOCK_TYPE_RESERVED:
		case HDMI_EDID_BLOCK_TYPE_RESERVED2:
		default:
			pr_info("warn: Unknown EDID block type. tag: %d, offset: %d\n",
				tag, offset);
			offset++;
			offset += count;
			break;
		}
	}
next:
	edid_y420cmdb_postprocess(prxcap);

	/* dtds in extended blocks */
	i = 0;
	offset = block_buf[2] + i * 18;
	for ( ; (offset + 18) < 0x7f; i++) {
		edid_dtd_parsing(prxcap, &block_buf[offset]);
		offset += 18;
	}

	if (vfpdb_offset)
		edid_parsingvfpdb(prxcap, vfpdb_offset);

	return 0;
}

static void hdmitx_edid_set_default_aud(struct rx_cap *prxcap)
{
	/* if AUD_count not equal to 0, no need default value */
	if (!prxcap || prxcap->AUD_count)
		return;

	prxcap->AUD_count = 1;
	/* PCM */
	prxcap->RxAudioCap[0].audio_format_code = 1;
	/* 2ch */
	prxcap->RxAudioCap[0].channel_num_max = 1;
	/* 32/44.1/48 kHz */
	prxcap->RxAudioCap[0].freq_cc = 7;
	/* 16bit */
	prxcap->RxAudioCap[0].cc3 = 1;
}

/*
 * for below cases:
 * for exception process: no CEA vic in parse result
 * DVI case(only one block), HDMI/HDCP CTS(TODO)
 */
static void hdmitx_edid_set_default_vic(struct rx_cap *prxcap)
{
	if (!prxcap)
		return;

	prxcap->VIC_count = 0x4;
	prxcap->VIC[0] = HDMI_3_720x480p60_16x9;
	prxcap->VIC[1] = HDMI_4_1280x720p60_16x9;
	prxcap->VIC[2] = HDMI_5_1920x1080i60_16x9;
	prxcap->VIC[3] = HDMI_16_1920x1080p60_16x9;
	prxcap->native_vic = HDMI_3_720x480p60_16x9;
	pr_info("set default vic\n");
}

/* specially for hdmi */
static bool hdmitx_edid_search_IEEEOUI(char *buf)
{
	int i;

	if (!buf)
		return false;

	for (i = 0; i < (EDID_MAX_BLOCK - 1) * EDID_BLK_SIZE - 2; i++) {
		if (buf[i] == 0x03 && buf[i + 1] == 0x0c &&
		    buf[i + 2] == 0x00)
			return true;
	}
	return false;
}

static void edid_manufacture_date_parse(struct rx_cap *prxcap,
				      u8 *data)
{
	if (!data)
		return;

	/*
	 * week:
	 *	0: not specified
	 *	0x1~0x35: valid week
	 *	0x37~0xfe: reserved
	 *	0xff: model year is specified
	 */
	if (data[0] == 0 || (data[0] >= 0x36 && data[0] <= 0xfe))
		prxcap->manufacture_week = 0;
	else
		prxcap->manufacture_week = data[0];

	/*
	 * year:
	 *	0x0~0xf: reserved
	 *	0x10~0xff: year of manufacture,
	 *		or model year(if specified by week=0xff)
	 */
	prxcap->manufacture_year =
		(data[1] <= 0xf) ? 0 : data[1];
}

static void edid_version_parse(struct rx_cap *prxcap,
			      u8 *data)
{
	if (!data)
		return;

	/*
	 *	0x1: edid version 1
	 *	0x0,0x2~0xff: reserved
	 */
	prxcap->edid_version = (data[0] == 0x1) ? 1 : 0;

	/*
	 *	0x0~0x4: revision number
	 *	0x5~0xff: reserved
	 */
	prxcap->edid_revision = (data[1] < 0x5) ? data[1] : 0;
}

static void edid_physical_size_parse(struct rx_cap *prxcap,
				   u8 *data)
{
	if (!prxcap || !data)
		return;

	if (data[0] != 0 && data[1] != 0) {
		/* Here the unit is cm, transfer to mm */
		prxcap->physical_width  = data[0] * 10;
		prxcap->physical_height = data[1] * 10;
	}
}

static void dump_dtd_info(struct dtd *t)
{
	if (!t)
		return;

	if (0) {
		pr_debug("%s[%d]\n", __func__, __LINE__);
		pr_debug("pixel_clock: %d\n", t->pixel_clock);
		pr_debug("h_active: %d\n", t->h_active);
		pr_debug("v_active: %d\n", t->v_active);
		pr_debug("v_blank: %d\n", t->v_blank);
		pr_debug("h_sync_offset: %d\n", t->h_sync_offset);
		pr_debug("h_sync: %d\n", t->h_sync);
		pr_debug("v_sync_offset: %d\n", t->v_sync_offset);
		pr_debug("v_sync: %d\n", t->v_sync);
	}
}

static void edid_dtd_parsing(struct rx_cap *prxcap, u8 *data)
{
	const struct tx_timing *timing = NULL;
	struct dtd *t;

	if (!prxcap || !data)
		return;

	if (prxcap->dtd_idx >= MAX_DTD_COUNT) {
		pr_debug("dtd_idx must be less than MAX_DTD_COUNT\n");
		return;
	}
	t = &prxcap->dtd[prxcap->dtd_idx];
	/* if data[0-2] are zeroes, no need parse, and skip */
	if (data[0] == 0 && data[1] == 0 && data[2] == 0)
		return;
	memset(t, 0, sizeof(struct dtd));
	t->pixel_clock = data[0] + (data[1] << 8);
	t->h_active = (((data[4] >> 4) & 0xf) << 8) + data[2];
	t->h_blank = ((data[4] & 0xf) << 8) + data[3];
	t->v_active = (((data[7] >> 4) & 0xf) << 8) + data[5];
	t->v_blank = ((data[7] & 0xf) << 8) + data[6];
	t->h_sync_offset = (((data[11] >> 6) & 0x3) << 8) + data[8];
	t->h_sync = (((data[11] >> 4) & 0x3) << 8) + data[9];
	t->v_sync_offset = (((data[11] >> 2) & 0x3) << 4) +
		((data[10] >> 4) & 0xf);
	t->v_sync = (((data[11] >> 0) & 0x3) << 4) + ((data[10] >> 0) & 0xf);
	t->h_image_size = (((data[14] >> 4) & 0xf) << 8) + data[12];
	t->v_image_size = ((data[14] & 0xf) << 8) + data[13];
	t->flags = data[17];
/*
 * Special handling of 1080i60hz, 1080i50hz
 */
	if (t->pixel_clock == 7425 && t->h_active == 1920 &&
	    t->v_active == 1080) {
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * Special handling of 480i60hz, 576i50hz
 */
	if ((((t->flags >> 1) & 0x3) == 0) && t->h_active == 1440) {
		/* 576i50hz */
		if (t->pixel_clock == 2700)
			goto next;
		/* 480i60hz */
		if ((t->pixel_clock - 2700) < 10)
			t->pixel_clock = 2702;
next:
		t->v_active = t->v_active / 2;
		t->v_blank = t->v_blank / 2;
	}
/*
 * call meson_tx_mode_match_dtd_timing() to check t is matched with VIC
 */
	timing = meson_tx_mode_match_dtd_timing(t);
	if (timing->vic != HDMI_0_UNKNOWN) {
		/* diff 4x3 and 16x9 mode */
		if (timing->vic == HDMI_6_720x480i60_4x3 ||
			timing->vic == HDMI_2_720x480p60_4x3 ||
			timing->vic == HDMI_21_720x576i50_4x3 ||
			timing->vic == HDMI_17_720x576p50_4x3) {
			if (abs(t->v_image_size * 100 / t->h_image_size - 3 * 100 / 4) <= 2)
				t->vic = timing->vic;
			else
				t->vic = timing->vic + 1;
		} else {
			t->vic = timing->vic;
		}
		/* Select dtd0 */
		prxcap->preferred_mode = prxcap->dtd[0].vic;
		pr_debug("get dtd%d vic: %d\n",
			prxcap->dtd_idx, t->vic);
		prxcap->dtd_idx++;
		if (t->vic < HDMITX_VESA_OFFSET)
			store_cea_idx(prxcap, t->vic);
		else
			store_vesa_idx(prxcap, t->vic);
	} else {
		dump_dtd_info(t);
	}
}

static void hdmitx_edid_check_pcm_declare(struct rx_cap *prxcap)
{
	bool pcm = false;
	int i;

	if (!prxcap || !prxcap->AUD_count)
		return;

	/* Try to find more than 1 PCMs */
	for (i = 0; i < prxcap->AUD_count; i++) {
		if (prxcap->RxAudioCap[i].audio_format_code == CT_PCM) {
			pcm = true;
			break;
		}
	}

	/* not have pcm, add basic pcm cap */
	if (!pcm) {
		/* PCM */
		prxcap->RxAudioCap[prxcap->AUD_count].audio_format_code = 1;
		/* 2ch */
		prxcap->RxAudioCap[prxcap->AUD_count].channel_num_max = 1;
		/* 32/44.1/48 kHz */
		prxcap->RxAudioCap[prxcap->AUD_count].freq_cc = 7;
		/* 16bit */
		prxcap->RxAudioCap[prxcap->AUD_count].cc3 = 1;
		prxcap->AUD_count++;
	}
}

static bool is_4k60_supported(struct rx_cap *prxcap)
{
	int i = 0;

	if (!prxcap)
		return false;

	for (i = 0; (i < prxcap->VIC_count) && (i < VIC_MAX_NUM); i++) {
		if (((prxcap->VIC[i] & 0xff) == HDMI_96_3840x2160p50_16x9) ||
		    ((prxcap->VIC[i] & 0xff) == HDMI_97_3840x2160p60_16x9)) {
			return true;
		}
	}
	return false;
}

bool meson_tx_edid_support_y422(struct rx_cap *prxcap)
{
	if (!prxcap)
		return false;

	if (prxcap->native_Mode & CAP_BIT4_YCBCR_422_MASK)
		return true;

	return false;
}

static void edid_descriptor_pmt(struct rx_cap *prxcap,
				struct vesa_standard_timing *t,
				u8 *data)
{
	const struct tx_timing *timing = NULL;

	if (!prxcap || !t || !data)
		return;

	t->tmds_clk = data[0] + (data[1] << 8);
	t->hactive = data[2] + (((data[4] >> 4) & 0xf) << 8);
	t->hblank = data[3] + ((data[4] & 0xf) << 8);
	t->vactive = data[5] + (((data[7] >> 4) & 0xf) << 8);
	t->vblank = data[6] + ((data[7] & 0xf) << 8);

	timing = meson_tx_mode_match_vesa_timing(t);
	if (timing && (timing->vic < (HDMI_107_3840x2160p60_64x27 + 1))) {
		prxcap->native_vic = timing->vic;
		pr_debug("hdmitx: get PMT vic: %d\n", timing->vic);
	}
	if (timing && timing->vic >= HDMITX_VESA_OFFSET)
		store_vesa_idx(prxcap, timing->vic);
}

static void edid_descriptor_pmt2(struct rx_cap *prxcap,
				 struct vesa_standard_timing *t,
				 u8 *data)
{
	const struct tx_timing *timing = NULL;

	if (!prxcap || !t || !data)
		return;

	t->tmds_clk = data[0] + (data[1] << 8);
	t->hactive = data[2] + (((data[4] >> 4) & 0xf) << 8);
	t->hblank = data[3] + ((data[4] & 0xf) << 8);
	t->vactive = data[5] + (((data[7] >> 4) & 0xf) << 8);
	t->vblank = data[6] + ((data[7] & 0xf) << 8);

	timing = meson_tx_mode_match_vesa_timing(t);
	if (timing && timing->vic >= HDMITX_VESA_OFFSET)
		store_vesa_idx(prxcap, timing->vic);
}

static void edid_cvt_timing_3bytes(struct rx_cap *prxcap,
				   struct vesa_standard_timing *t,
				   const u8 *data)
{
	const struct tx_timing *timing = NULL;

	if (!prxcap || !t || !data)
		return;

	t->hactive = ((data[0] + (((data[1] >> 4) & 0xf) << 8)) + 1) * 2;
	switch ((data[1] >> 2) & 0x3) {
	case 0:
		t->vactive = t->hactive * 3 / 4;
		break;
	case 1:
		t->vactive = t->hactive * 9 / 16;
		break;
	case 2:
		t->vactive = t->hactive * 5 / 8;
		break;
	case 3:
	default:
		t->vactive = t->hactive * 3 / 5;
		break;
	}
	switch ((data[2] >> 5) & 0x3) {
	case 0:
		t->vsync = 50;
		break;
	case 1:
		t->vsync = 60;
		break;
	case 2:
		t->vsync = 75;
		break;
	case 3:
	default:
		t->vsync = 85;
		break;
	}
	timing = meson_tx_mode_match_vesa_timing(t);
	if (timing->vic != HDMI_0_UNKNOWN)
		t->vesa_timing = timing->vic;
}

static void edid_cvt_timing(struct rx_cap *prxcap, u8 *data)
{
	int i;
	struct vesa_standard_timing t;

	if (!prxcap || !data)
		return;

	for (i = 0; i < 4; i++) {
		memset(&t, 0, sizeof(struct vesa_standard_timing));
		edid_cvt_timing_3bytes(prxcap, &t, &data[i * 3]);
		if (t.vesa_timing) {
			if (t.vesa_timing >= HDMITX_VESA_OFFSET)
				store_vesa_idx(prxcap, t.vesa_timing);
		}
	}
}

static void check_dv_truly_support(struct rx_cap *prxcap, struct dv_info *dv)
{
	u32 max_tmds_clk = 0;

	if (!prxcap || !dv)
		return;

	if (dv->ieeeoui == DV_IEEE_OUI && dv->ver <= 2) {
		/*
		 * check max tmds rate to determine if 4k60 DV can truly be
		 * supported.
		 */
		if (prxcap->Max_TMDS_Clock2) {
			max_tmds_clk = prxcap->Max_TMDS_Clock2 * 5;
		} else {
			/* Default min is 74.25 / 5 */
			if (prxcap->Max_TMDS_Clock1 < 0xf)
				prxcap->Max_TMDS_Clock1 = DEFAULT_MAX_TMDS_CLK;
			max_tmds_clk = prxcap->Max_TMDS_Clock1 * 5;
		}
		if (dv->ver == 0)
			dv->sup_2160p60hz = dv->sup_2160p60hz &&
						(max_tmds_clk >= 594);

		if (dv->ver == 1 && dv->length == 0xB) {
			if (dv->low_latency == 0x00) {
				/* standard mode */
				dv->sup_2160p60hz = dv->sup_2160p60hz &&
							(max_tmds_clk >= 594);
			} else if (dv->low_latency == 0x01) {
				/*
				 * both standard and LL are supported. 4k60 LL
				 * DV support should/can be determined using
				 * video formats supported in the E-EDID as flag
				 * sup_2160p60hz might not be set.
				 */
				if ((dv->sup_2160p60hz ||
				     is_4k60_supported(prxcap)) &&
				    max_tmds_clk >= 594)
					dv->sup_2160p60hz = 1;
				else
					dv->sup_2160p60hz = 0;
			}
		}

		if (dv->ver == 1 && dv->length == 0xE)
			dv->sup_2160p60hz = dv->sup_2160p60hz &&
						(max_tmds_clk >= 594);

		if (dv->ver == 2) {
			/*
			 * 4k60 DV support should be determined using video
			 * formats supported in the EEDID as flag sup_2160p60hz
			 * is not applicable for VSVDB V2.
			 */
			if (is_4k60_supported(prxcap) && max_tmds_clk >= 594)
				dv->sup_2160p60hz = 1;
			else
				dv->sup_2160p60hz = 0;
		}
	}
}

static void _edid_parse_base_structure(struct rx_cap *prxcap, unsigned char *edid_buf)
{
	unsigned char checksum;
	unsigned char zero_numbers;
	unsigned char cta_block_count;
	int i;
	int idx[4];

	if (!prxcap || !edid_buf)
		return;

	edid_parsing_id_manufacturer_name(prxcap, &edid_buf[8]);
	edid_parsing_id_product_code(prxcap, &edid_buf[0x0A]);
	edid_parsing_id_serial_number(prxcap, &edid_buf[0x0C]);

	edid_established_timings(prxcap, &edid_buf[0x23]);

	edid_manufacture_date_parse(prxcap, &edid_buf[16]);

	edid_version_parse(prxcap, &edid_buf[18]);

	edid_physical_size_parse(prxcap, &edid_buf[21]);

	edid_standard_timing(prxcap, &edid_buf[0x26], 8);
	edid_parseceatiming(prxcap, &edid_buf[0x36]);
	/*
	 * Because DTDs are not able to represent some Video Formats, which can be
	 * represented as SVDs and might be preferred by Sinks, the first DTD in the
	 * base EDID data structure and the first SVD in the first CEA Extension can
	 * differ. When the first DTD and SVD do not match and the total number of
	 * DTDs defining Native Video Formats in the whole EDID is zero, the first
	 * SVD shall take precedence.
	 */
	if (!prxcap->flag_vfpdb &&
	    prxcap->preferred_mode != prxcap->VIC[0] &&
	    prxcap->number_of_dtd == 0) {
		pr_debug("change preferred_mode from %d to %d\n",
			prxcap->preferred_mode,	prxcap->VIC[0]);
		prxcap->preferred_mode = prxcap->VIC[0];
	}

	idx[0] = EDID_DETAILED_TIMING_DES_BLOCK0_POS;
	idx[1] = EDID_DETAILED_TIMING_DES_BLOCK1_POS;
	idx[2] = EDID_DETAILED_TIMING_DES_BLOCK2_POS;
	idx[3] = EDID_DETAILED_TIMING_DES_BLOCK3_POS;
	for (i = 0; i < 4; i++) {
		if ((edid_buf[idx[i]]) && (edid_buf[idx[i] + 1])) {
			struct vesa_standard_timing t;

			memset(&t, 0, sizeof(struct vesa_standard_timing));
			if (i == 0)
				edid_descriptor_pmt(prxcap, &t, &edid_buf[idx[i]]);
			if (i == 1)
				edid_descriptor_pmt2(prxcap, &t, &edid_buf[idx[i]]);
			continue;
		}
		switch (edid_buf[idx[i] + 3]) {
		case TAG_STANDARD_TIMINGS:
			edid_standard_timing(prxcap, &edid_buf[idx[i] + 5], 6);
			break;
		case TAG_CVT_TIMING_CODES:
			edid_cvt_timing(prxcap, &edid_buf[idx[i] + 6]);
			break;
		case TAG_ESTABLISHED_TIMING_III:
			edid_standard_timing_iii(prxcap, &edid_buf[idx[i] + 6]);
			break;
		case TAG_RANGE_LIMITS:
			break;
		case TAG_DISPLAY_PRODUCT_NAME_STRING:
			edid_receiverproductnameparse(prxcap, &edid_buf[idx[i] + 5]);
			break;
		default:
			break;
		}
	}
	prxcap->blk0_chksum = edid_buf[0x7F];

	cta_block_count = tx_edid_get_cta_block_count(edid_buf);

	/*
	 * only one block, need parse continue, at the end of parse, it will determine
	 * whether to use the default vic
	 */
	if (cta_block_count == 0) {
		pr_info("EDID BlockCount=0\n");
		/*
		 * DVI case judgement: only contains one block and
		 * checksum valid
		 */
		checksum = 0;
		zero_numbers = 0;
		for (i = 0; i < 128; i++) {
			checksum += edid_buf[i];
			if (edid_buf[i] == 0)
				zero_numbers++;
		}
		pr_debug("edid blk0 checksum:%d ext_flag:%d\n",
			checksum, edid_buf[0x7e]);
		if ((checksum & 0xff) == 0)
			prxcap->ieeeoui = 0;
		else
			prxcap->ieeeoui = HDMI_IEEE_OUI;
		if (zero_numbers > 120)
			prxcap->ieeeoui = HDMI_IEEE_OUI;
	}
}

static int xtochar(u8 value, u8 *checksum)
{
	if (!checksum)
		return 0;
	if (((value  >> 4) & 0xf) <= 9)
		checksum[0] = ((value  >> 4) & 0xf) + '0';
	else
		checksum[0] = ((value  >> 4) & 0xf) - 10 + 'a';

	if ((value & 0xf) <= 9)
		checksum[1] = (value & 0xf) + '0';
	else
		checksum[1] = (value & 0xf) - 10 + 'a';

	return 0;
}

static int update_edid_chksum(struct rx_cap *prxcap, u8 *edid_buf)
{
	u32 i;
	unsigned char tmp_chksum[4] = {0};

	if (!prxcap || !edid_buf)
		return -EINVAL;

	memset(prxcap->hdmichecksum, 0, sizeof(prxcap->hdmichecksum));

	/* get the first 4 sub-blocks chksum */
	for (i = 0; i < 4; i++)
		tmp_chksum[i] = edid_buf[i * 128 + 127];

	prxcap->hdmichecksum[0] = '0';
	prxcap->hdmichecksum[1] = 'x';

	for (i = 0; i < 4; i++)
		xtochar(tmp_chksum[i], &prxcap->hdmichecksum[2 * i + 2]);

	return 0;
}

static int hdmitx_edid_parse_ieeeoui(u8 *edid_buf)
{
	u32 ieeeoui;

	/* strictly DVI device judgement */
	/* valid EDID & no audio tag & no IEEEOUI */
	if (meson_tx_edid_check_valid(0, &edid_buf[0]) &&
		!hdmitx_edid_search_IEEEOUI(&edid_buf[128])) {
		ieeeoui = 0x0;
		pr_info("EDID sink is DVI device\n");
	} else {
		ieeeoui = HDMI_IEEE_OUI;
	}

	return ieeeoui;
}

int meson_tx_edid_parse(struct rx_cap *prxcap, u8 *edid_buf, u8 edid_parse_mask)
{
	unsigned char cta_block_count;
	int i;
	struct dv_info *dv;

	if (!edid_buf || !prxcap)
		return -1;

	/* case1: pure displayID for DPTX */
	if (edid_parse_mask & BIT(1)) {
		if (edid_buf[0] == 0x11 ||
			edid_buf[0] == 0x12 ||
			edid_buf[0] == 0x20)
			return display_id_parse(&prxcap->disp_id_cap, edid_buf);
	}
	/* case2: EDID raw data for both DPTX/HDMITX */
	cta_block_count = tx_edid_get_cta_block_count(edid_buf);
	dv = &prxcap->dv_info;

	pr_debug("EDID Parser:\n");

	_edid_parse_base_structure(prxcap, edid_buf);

	for (i = 1; i <= cta_block_count; i++) {
		if (edid_buf[i * 0x80] == 0x70) {
			/* skip parse displayID structure contained in EDID */
			if ((edid_parse_mask & BIT(1)) == 0)
				continue;
			display_id_parse(&prxcap->disp_id_cap, &edid_buf[i * 0x80 + 1]);
			if (i < cta_block_count)
				pr_info("warning: exceed 1 displayID block in EDID\n");
			if (prxcap->disp_id_cap.extension_count > 0)
				pr_info("warning: exceed 1 section in displayID of EDID\n");
		} else if (edid_buf[i * 0x80] == 0x02 || (prxcap->edid_check & 0x01 &&
			edid_buf[i * 0x80] != 0x70)) {
			tx_edid_cta_block_parse(prxcap, &edid_buf[i * 0x80]);
		}
	}

	prxcap->ieeeoui = hdmitx_edid_parse_ieeeoui(edid_buf);
	update_edid_chksum(prxcap, edid_buf);

	/* EDID parsing complete - check if 4k60/50 DV can be truly supported */
	dv = &prxcap->dv_info;
	check_dv_truly_support(prxcap, dv);
	/*
	 * For some receivers, they don't claim the screen size
	 * and re-calculate it from the h/v image size from dtd
	 * the unit of screen size is cm, but the unit of image size is mm
	 */
	if (prxcap->physical_width == 0 || prxcap->physical_height == 0) {
		struct dtd *t = &prxcap->dtd[0];

		prxcap->physical_width  = t->h_image_size;
		prxcap->physical_height = t->v_image_size;
	}

	if (edid_parse_mask & BIT(1)) {
		/* for DPTX, if no available timing in CTA
		 * block/displayID, then set default VESA timing
		 */
		if (prxcap->VIC_count == 0 &&
			prxcap->vesa_timing_count == 0 &&
			prxcap->disp_id_cap.timing_count == 0) {
			prxcap->vesa_timing_count = 1;
			prxcap->vesa_timing[0] = HDMI_1_640x480p60_4x3;
			pr_info("warning: no timing in EDID/DisplayID, add 640x480p60hz by default\n");
		}
	} else {
		/* for HDMITX(which won't parse displayID),
		 * if edid no VIC, set default vic
		 */
		if (prxcap->VIC_count == 0)
			hdmitx_edid_set_default_vic(prxcap);
	}
	return 0;
}
EXPORT_SYMBOL(meson_tx_edid_parse);

void meson_tx_edid_buffer_clear(u8 *edid_buf, int size)
{
	if (!edid_buf)
		return;

	memset(edid_buf, 0, size);
}

/* Clear the Parse result of HDMI Sink's EDID. */
void meson_tx_clear_rx_cap(struct rx_cap *prxcap)
{
	char tmp[2] = {0};
	u8 ret = 0;

	if (!prxcap)
		return;

	display_id_cap_clear(&prxcap->disp_id_cap);
	ret = prxcap->edid_check;
	memset(prxcap, 0, sizeof(struct rx_cap));
	prxcap->edid_check = ret;

	/*
	 * Note: in most cases, we think that rx is tv and the default
	 * IEEEOUI is HDMI Identifier
	 */
	prxcap->ieeeoui = HDMI_IEEE_OUI;

	prxcap->edid_parsing = 0;
	rx_set_hdr_lumi(&tmp[0], 2);
	/* rx_set_receiver_edid(&tmp[0], 2); */
}
EXPORT_SYMBOL(meson_tx_clear_rx_cap);

/*
 * print one block data of edid
 */
#define TMP_EDID_BUF_SIZE	(256 + 8)
static void tx_edid_blk_print(unsigned char *blk, unsigned int blk_idx)
{
	unsigned int i, pos;
	unsigned char *tmp_buf = NULL;

	if (!blk)
		return;

	tmp_buf = kmalloc(TMP_EDID_BUF_SIZE, GFP_KERNEL);
	if (!tmp_buf)
		return;

	memset(tmp_buf, 0, TMP_EDID_BUF_SIZE);
	pr_debug("blk%d raw data\n", blk_idx);
	for (i = 0, pos = 0; i < 128; i++) {
		pos += sprintf(tmp_buf + pos, "%02x", blk[i]);
		/* print 64 bytes a line */
		if (((i + 1) & 0x3f) == 0)
			pos += sprintf(tmp_buf + pos, "\n");
	}
	pr_info("%s", tmp_buf);
	kfree(tmp_buf);
}

/*
 * check EDID buf contains valid block numbers
 */
unsigned int meson_tx_edid_valid_block_num(unsigned char *edid_buf)
{
	unsigned int valid_blk_no = 0;
	unsigned int i = 0, j = 0;
	unsigned int tmp_chksum = 0;

	if (!edid_buf)
		return 0;

	for (j = 0; j < EDID_MAX_BLOCK; j++) {
		for (i = 0; i < 128; i++)
			tmp_chksum += edid_buf[i + j * 128];
		if (tmp_chksum != 0) {
			valid_blk_no++;
			if ((tmp_chksum & 0xff) == 0)
				pr_debug("check sum valid\n");
			else
				pr_err("check sum invalid\n");
		}
		tmp_chksum = 0;
	}
	/* EEODB case */
	if (edid_buf[128 + 4] == EXTENSION_EEODB_EXT_TAG &&
		edid_buf[128 + 5] == EXTENSION_EEODB_EXT_CODE)
		valid_blk_no = edid_buf[128 + 6] + 1;
	return valid_blk_no;
}

/*
 * print out edid_buf
 */
void meson_tx_edid_raw_data_print(u8 *edid_buf)
{
	u32 valid_blk_no = 0;
	u32 blk_idx = 0;

	if (!edid_buf)
		return;

	/* calculate valid edid block numbers */
	valid_blk_no = meson_tx_edid_valid_block_num(edid_buf);

	if (valid_blk_no == 0) {
		pr_debug("raw data are all zeroes\n");
	} else {
		for (blk_idx = 0; blk_idx < valid_blk_no; blk_idx++)
			tx_edid_blk_print(&edid_buf[blk_idx * 128], blk_idx);
	}
}

static struct displayid_header *displayid_get_header(u8 *displayid, int length, int index)
{
	struct displayid_header *base;

	if (sizeof(*base) > length - index)
		return NULL;

	base = (struct displayid_header *)&displayid[index];

	return base;
}

static struct displayid_block *displayid_iter_block(u8 *block_buf, u8 idx, u8 length)
{
	struct displayid_block *block;

	if (!block_buf)
		return NULL;

	block = (struct displayid_block *)block_buf;

	if (idx + sizeof(*block) <= length &&
	    idx + sizeof(*block) + block->num_bytes <= length)
		return block;

	return NULL;
}

static struct displayid_header *validate_displayid(u8 *displayid, int length, int idx)
{
	int i, disp_id_length;
	u8 csum = 0;
	struct displayid_header *base;

	base = displayid_get_header(displayid, length, idx);
	if (!base)
		return NULL;

	/* +1 for DispID checksum */
	disp_id_length = sizeof(*base) + base->bytes + 1;
	if (disp_id_length > length - idx) {
		pr_err("DisplayID bytes_length(%d) invalid\n", base->bytes);
		return NULL;
	}

	for (i = 0; i < disp_id_length; i++)
		csum += displayid[idx + i];
	if (csum) {
		pr_err("DisplayID checksum invalid, remainder is %d\n", csum);
		return NULL;
	}

	return base;
}

static int displayid_detailed(struct displayid_detailed_timings_1 *timings,
		struct tx_timing *t, bool type_7, unsigned char block_revision)
{
	unsigned int pixel_clock = 0;
	unsigned int hactive = 0;
	unsigned int hblank = 0;
	unsigned int hsync = 0;
	unsigned int hsync_width = 0;
	unsigned int vactive = 0;
	unsigned int vblank = 0;
	unsigned int vsync = 0;
	unsigned int vsync_width = 0;
	bool hsync_positive = false;
	bool vsync_positive = false;
	bool dsc_support = false;
	bool y420_support = false;
	int divisor = 0;
	int len = 0;

	if (!timings || !t)
		return -1;

	pixel_clock = (timings->pixel_clock[0] |
				(timings->pixel_clock[1] << 8) |
				(timings->pixel_clock[2] << 16)) + 1;
	hactive = (timings->hactive[0] | timings->hactive[1] << 8) + 1;
	hblank = (timings->hblank[0] | timings->hblank[1] << 8) + 1;
	hsync = (timings->hsync[0] | (timings->hsync[1] & 0x7f) << 8) + 1;
	hsync_width = (timings->hsw[0] | timings->hsw[1] << 8) + 1;
	vactive = (timings->vactive[0] | timings->vactive[1] << 8) + 1;
	vblank = (timings->vblank[0] | timings->vblank[1] << 8) + 1;
	vsync = (timings->vsync[0] | (timings->vsync[1] & 0x7f) << 8) + 1;
	vsync_width = (timings->vsw[0] | timings->vsw[1] << 8) + 1;
	hsync_positive = (timings->hsync[1] >> 7) & 0x1;
	vsync_positive = (timings->vsync[1] >> 7) & 0x1;

	/* resolution is kHz for type VII, and 10 kHz for type I */
	t->pixel_freq = type_7 ? pixel_clock : pixel_clock * 10;
	t->h_active = hactive;
	t->h_blank = hblank;
	t->h_sync = hsync;
	t->h_front = hsync_width;
	t->h_total = hactive + hblank;
	t->v_active = vactive;
	t->v_blank = vblank;
	t->v_sync = vsync;
	t->v_front = vsync_width;
	t->v_total = vactive + vblank;
	t->h_pol = hsync_positive;
	t->v_pol = vsync_positive;

	/* bit 4:
	 * Interface Frame Scanning Type
	 * 0 = Progressive scan frame.
	 * 1 = Interlaced scan frame
	 */
	if ((timings->flags & 0x10) >> 4)
		t->pi_mode = 0;
	else
		t->pi_mode = 1;

	switch (timings->flags & 0xf) {
	case DISPLAY_ID_ASPECT_RATIO_1_1:
		t->h_pict = 1;
		t->v_pict = 1;
		break;
	case DISPLAY_ID_ASPECT_RATIO_5_4:
		t->h_pict = 5;
		t->v_pict = 4;
		break;
	case DISPLAY_ID_ASPECT_RATIO_4_3:
		t->h_pict = 4;
		t->v_pict = 3;
		break;
	case DISPLAY_ID_ASPECT_RATIO_15_9:
		t->h_pict = 15;
		t->v_pict = 9;
		break;
	case DISPLAY_ID_ASPECT_RATIO_16_9:
		t->h_pict = 16;
		t->v_pict = 9;
		break;
	case DISPLAY_ID_ASPECT_RATIO_16_10:
		t->h_pict = 16;
		t->v_pict = 10;
		break;
	case DISPLAY_ID_ASPECT_RATIO_64_27:
		t->h_pict = 64;
		t->v_pict = 27;
		break;
	case DISPLAY_ID_ASPECT_RATIO_256_135:
		t->h_pict = 256;
		t->v_pict = 135;
		break;
	case DISPLAY_ID_ASPECT_RATIO_NOT_DEFINED:
		/* Aspect ratio shall be calculated by using hactive and vactive */
		divisor = gcd(hactive, vactive);
		t->h_pict = divisor ? hactive / divisor : 0;
		t->v_pict = divisor ? vactive / divisor : 0;
		break;
	case DISPLAY_ID_ASPECT_RATIO_RESERVED:
	default:
		t->h_pict = 16;
		t->v_pict = 9;
		break;
	}
	/*
	 * bit 3:
	 * For Block Revision 0
	 * RESERVED
	 * Cleared to all 0s.
	 * For Block Revisions 1 and 2
	 * Pass-through Timing Support for Target DSC Bits per Pixel
	 * 0 = Same as Revision 0 (standard timing support declaration as DisplayID v2.0).
	 * 1 = Indicates that the data block’s listed timing descriptors are supported with
	 * DSC pass-through with DSC RGB encoding and specific target DSC bits per pixel only
	 */
	dsc_support = type_7 && (block_revision & 0x3) && (block_revision & 0x8);
	if (dsc_support)
		t->flags |= DISPLAY_ID_TYPE_VII_ONLY_SUPPORT_DSC;

	/*
	 * bit 7:
	 * For Block Revisions 0 and 1
	 * Preferred Detailed Timing
	 * 1 = Preferred Detailed timing.
	 * For Block Revision 2
	 * YCC 420 Support
	 * 0 = Explicit YCbCr 4:2:0 support is not indicated for the current timing
	 * (other data blocks may be used to indicate support).
	 * 1 = Explicit YCbCr 4:2:0 support is indicated for the current timing
	 */
	y420_support = type_7 && (block_revision & 0x2) && (timings->flags & 0x80);
	if (y420_support)
		t->flags |= DISPLAY_ID_TYPE_VII_YUV420;

	t->v_freq = meson_tx_get_timing_vrefresh(t);

	/* add name for timing:
	 * assume the timings in DATA_BLOCK_TYPE_1_DETAILED_TIMING
	 * and DATA_BLOCK_2_TYPE_7_DETAILED_TIMING blocks in displayID
	 * are VESA timings, add suffix 'V' in the end of mode name;
	 * and assume all the CEA timings are included in CTA extension data
	 * blocks(128bytes) or CTA video data blocks(VDB/Y420 VDB...).
	 */
	t->name = kzalloc(32, GFP_KERNEL);
	if (t->name) {
		len = snprintf((char *)t->name, 32,  "%dx%d%s%lluhzV",
			t->h_active, t->v_active, t->pi_mode ? "p" : "i",
			DIV_ROUND_CLOSEST_ULL(t->v_freq, 1000));
		if (len >= 32)
			t->name[31] = '\0';
	} else {
		pr_err("DisplayID detailed timing alloc name mem failed\n");
	}

	return 0;
}

/* allocate memory for timing dynamically during parse displayID,
 * free the memory when plugout
 */
static int displayid_timing_mem_alloc(struct display_id_cap *disp_id_cap, u8 new_timing_cnt)
{
	int i = 0;
	/* number of BLK_TIMING_CNT already alloc for timing */
	int alloced_blocks = 0;
	/* number of BLK_TIMING_CNT need for total timing */
	int total_blocks = 0;

	if (!disp_id_cap)
		return -EINVAL;

	alloced_blocks = DIV_ROUND_UP(disp_id_cap->timing_count, BLK_TIMING_CNT);
	total_blocks = DIV_ROUND_UP(disp_id_cap->timing_count + new_timing_cnt, BLK_TIMING_CNT);

	/* Initial allocation if needed */
	if (alloced_blocks == 0) {
		disp_id_cap->timing = kcalloc(MAX_SECTION_COUNT,
			sizeof(struct tx_timing *), GFP_KERNEL);
		if (!disp_id_cap->timing)
			return -ENOMEM;

		disp_id_cap->timing[0] =
			kcalloc(BLK_TIMING_CNT, sizeof(struct tx_timing), GFP_KERNEL);
		if (!disp_id_cap->timing[0]) {
			kfree(disp_id_cap->timing);
			disp_id_cap->timing = NULL;
			pr_err("%s: alloc mem for first timing data block failed\n", __func__);
			return -ENOMEM;
		}
		alloced_blocks = 1;
	}

	/* Allocate additional blocks */
	for (i = alloced_blocks; i < total_blocks; i++) {
		if (i >= MAX_SECTION_COUNT) {
			pr_err("%s: exceed max sections (%d)\n", __func__, MAX_SECTION_COUNT);
			return -ENOMEM;
		}
		disp_id_cap->timing[i] =
			kcalloc(BLK_TIMING_CNT, sizeof(struct tx_timing), GFP_KERNEL);
		/* Note: not free previously alloced mem */
		if (!disp_id_cap->timing[i]) {
			pr_err("%s: alloc mem for new timings failed\n", __func__);
			return -ENOMEM;
		}
	}
	return 0;
}

static int displayid_add_detailed_1_modes(struct display_id_cap *disp_id_cap,
	struct displayid_block *block)
{
	struct displayid_detailed_timing_block *det = NULL;
	int i;
	int array_idx = 0;
	u8 remainder = 0;
	int ret = 0;
	int num_timings;
	bool type_7 = false;
	struct tx_timing *t = NULL;
	struct displayid_detailed_timings_1 *timings = NULL;

	if (!disp_id_cap || !block)
		return -1;

	det = (struct displayid_detailed_timing_block *)block;
	type_7 = block->tag == DATA_BLOCK_2_TYPE_7_DETAILED_TIMING;
	/* blocks must be multiple of 20 bytes length */
	if (block->num_bytes % 20) {
		pr_err("timing I/VII byte number[%d] div 20 error\n", block->num_bytes);
		return -1;
	}
	num_timings = block->num_bytes / 20;
	ret = displayid_timing_mem_alloc(disp_id_cap, num_timings);
	if (ret) {
		pr_err("%s, alloc mem failed for %d timings\n", __func__, num_timings);
		return -1;
	}

	for (i = 0; i < num_timings; i++) {
		timings = &det->timings[i];
		array_idx = disp_id_cap->timing_count / BLK_TIMING_CNT;
		remainder = disp_id_cap->timing_count % BLK_TIMING_CNT;
		t = &disp_id_cap->timing[array_idx][remainder];
		displayid_detailed(timings, t, type_7, block->rev);
		disp_id_cap->timing_count++;
	}
	return 0;
}

static int displayid_parse_display_interface_v1(struct display_id_cap *disp_id_cap,
	struct displayid_block *block)
{
	struct displayid_display_interface_feature_data_block_v1 *ddifdb_v1 = NULL;
	struct display_interface_info *display_interface = NULL;

	if (!disp_id_cap || !block)
		return -1;

	display_interface = &disp_id_cap->display_interface;
	ddifdb_v1 = (struct displayid_display_interface_feature_data_block_v1 *)block;
	display_interface->type = (ddifdb_v1->interface_v1.interface_type & 0xf0) >> 4;
	if (display_interface->type == INTERFACE_ANALOG)
		display_interface->link.analog_sub_type_codes =
				ddifdb_v1->interface_v1.interface_type & 0xf;
	else
		display_interface->link.links = ddifdb_v1->interface_v1.interface_type & 0xf;
	display_interface->interface_version =
		(ddifdb_v1->interface_v1.interface_reversion & 0xf0) >> 4;
	display_interface->interface_revision = ddifdb_v1->interface_v1.interface_reversion & 0xf;
	display_interface->color_depth.rgb_color_depth.bpc6 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(0));
	display_interface->color_depth.rgb_color_depth.bpc8 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(1));
	display_interface->color_depth.rgb_color_depth.bpc10 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(2));
	display_interface->color_depth.rgb_color_depth.bpc12 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(3));
	display_interface->color_depth.rgb_color_depth.bpc14 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(4));
	display_interface->color_depth.rgb_color_depth.bpc16 =
		!!(ddifdb_v1->interface_v1.interface_rgb_color_depth & BIT(5));

	display_interface->color_depth.yuv444_color_depth.bpc6 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(0));
	display_interface->color_depth.yuv444_color_depth.bpc8 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(1));
	display_interface->color_depth.yuv444_color_depth.bpc10 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(2));
	display_interface->color_depth.yuv444_color_depth.bpc12 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(3));
	display_interface->color_depth.yuv444_color_depth.bpc14 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(4));
	display_interface->color_depth.yuv444_color_depth.bpc16 =
		!!(ddifdb_v1->interface_v1.interface_yuv444_color_depth & BIT(5));

	display_interface->color_depth.yuv422_color_depth.bpc8 =
		!!(ddifdb_v1->interface_v1.interface_yuv422_color_depth & BIT(0));
	display_interface->color_depth.yuv422_color_depth.bpc10 =
		!!(ddifdb_v1->interface_v1.interface_yuv422_color_depth & BIT(1));
	display_interface->color_depth.yuv422_color_depth.bpc12 =
		!!(ddifdb_v1->interface_v1.interface_yuv422_color_depth & BIT(2));
	display_interface->color_depth.yuv422_color_depth.bpc14 =
		!!(ddifdb_v1->interface_v1.interface_yuv422_color_depth & BIT(3));
	display_interface->color_depth.yuv422_color_depth.bpc16 =
		!!(ddifdb_v1->interface_v1.interface_yuv422_color_depth & BIT(4));

	display_interface->content_protection.protection =
		ddifdb_v1->interface_v1.content_protection & 0x7;
	display_interface->content_protection.version =
		(ddifdb_v1->interface_v1.content_protection_version & 0xf0) >> 4;
	display_interface->content_protection.revision =
		ddifdb_v1->interface_v1.content_protection_version & 0xf;

	display_interface->spread_info.spread_type =
		(ddifdb_v1->interface_v1.spread_spectrum_info & 0xc0) >> 6;
	display_interface->spread_info.spread_percentage =
		ddifdb_v1->interface_v1.spread_spectrum_info & 0xf;

	if (display_interface->type == INTERFACE_LVDS) {
		display_interface->lvds_voltage_and_color_mapping =
			ddifdb_v1->interface_v1.interface_type_dependent_attr_1;
		display_interface->timing_dignal_settings =
			ddifdb_v1->interface_v1.interface_type_dependent_attr_2;
	} else if (display_interface->type == INTERFACE_PROPRIETARY_DIGITAL_INTERFACE) {
		display_interface->timing_dignal_settings =
			ddifdb_v1->interface_v1.interface_type_dependent_attr_1;
	}

	return 0;
}

static int displayid_parse_dynamic_video_timing_range_limits(struct display_id_cap *disp_id_cap,
		struct displayid_block *block)
{
	struct dynamic_video_timing_range_limits_data_block *video_range_blk = NULL;
	struct dynamic_video_timing_range_limits_info *dynamic_video_range = NULL;

	if (!disp_id_cap || !block)
		return -1;

	dynamic_video_range = &disp_id_cap->dynamic_video_timing_range;
	video_range_blk = (struct dynamic_video_timing_range_limits_data_block *)block;
	dynamic_video_range->pixel_clock_min =
		video_range_blk->dynamic_video_timing_range.pixel_clock_min[2] << 16 |
		video_range_blk->dynamic_video_timing_range.pixel_clock_min[1] << 8 |
		video_range_blk->dynamic_video_timing_range.pixel_clock_min[0];
	dynamic_video_range->pixel_clock_max =
		video_range_blk->dynamic_video_timing_range.pixel_clock_max[2] << 16 |
		video_range_blk->dynamic_video_timing_range.pixel_clock_max[1] << 8 |
		video_range_blk->dynamic_video_timing_range.pixel_clock_max[0];
	/* The minimum value is 1, so an offset needs to be added */
	dynamic_video_range->pixel_clock_min += 1;
	dynamic_video_range->pixel_clock_max += 1;
	dynamic_video_range->vertical_refresh_rate_min =
		video_range_blk->dynamic_video_timing_range.vertical_refresh_rate_min;

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
	if (video_range_blk->product_id_header.rev)
		dynamic_video_range->vertical_refresh_rate_max =
			((video_range_blk->dynamic_video_timing_range.flags & 0x3) << 8) |
			video_range_blk->dynamic_video_timing_range.vertical_refresh_rate_max;
	else
		dynamic_video_range->vertical_refresh_rate_max =
			video_range_blk->dynamic_video_timing_range.vertical_refresh_rate_max;

	dynamic_video_range->seamless_support_flag =
		(video_range_blk->dynamic_video_timing_range.flags & BIT(7)) >> 7;

	return 0;
}

static int displayid_parse_display_interface_v2(struct display_id_cap *disp_id_cap,
	struct displayid_block *block)
{
	int i;
	struct displayid_display_interface_feature_data_block_v2 *ddifdb_v2 = NULL;
	struct display_interface_info *display_interface = NULL;
	int count = 0;

	if (!disp_id_cap || !block)
		return -1;

	display_interface = &disp_id_cap->display_interface;
	ddifdb_v2 = (struct displayid_display_interface_feature_data_block_v2 *)block;
	display_interface->color_depth.rgb_color_depth.bpc6 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(0));
	display_interface->color_depth.rgb_color_depth.bpc8 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(1));
	display_interface->color_depth.rgb_color_depth.bpc10 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(2));
	display_interface->color_depth.rgb_color_depth.bpc12 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(3));
	display_interface->color_depth.rgb_color_depth.bpc14 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(4));
	display_interface->color_depth.rgb_color_depth.bpc16 =
		!!(ddifdb_v2->interface_v2.interface_rgb_color_depth & BIT(5));

	display_interface->color_depth.yuv444_color_depth.bpc6 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(0));
	display_interface->color_depth.yuv444_color_depth.bpc8 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(1));
	display_interface->color_depth.yuv444_color_depth.bpc10 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(2));
	display_interface->color_depth.yuv444_color_depth.bpc12 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(3));
	display_interface->color_depth.yuv444_color_depth.bpc14 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(4));
	display_interface->color_depth.yuv444_color_depth.bpc16 =
		!!(ddifdb_v2->interface_v2.interface_yuv444_color_depth & BIT(5));

	display_interface->color_depth.yuv422_color_depth.bpc8 =
		!!(ddifdb_v2->interface_v2.interface_yuv422_color_depth & BIT(0));
	display_interface->color_depth.yuv422_color_depth.bpc10 =
		!!(ddifdb_v2->interface_v2.interface_yuv422_color_depth & BIT(1));
	display_interface->color_depth.yuv422_color_depth.bpc12 =
		!!(ddifdb_v2->interface_v2.interface_yuv422_color_depth & BIT(2));
	display_interface->color_depth.yuv422_color_depth.bpc14 =
		!!(ddifdb_v2->interface_v2.interface_yuv422_color_depth & BIT(3));
	display_interface->color_depth.yuv422_color_depth.bpc16 =
		!!(ddifdb_v2->interface_v2.interface_yuv422_color_depth & BIT(4));

	display_interface->color_depth.yuv420_color_depth.bpc8 =
		!!(ddifdb_v2->interface_v2.interface_yuv420_color_depth & BIT(0));
	display_interface->color_depth.yuv420_color_depth.bpc10 =
		!!(ddifdb_v2->interface_v2.interface_yuv420_color_depth & BIT(1));
	display_interface->color_depth.yuv420_color_depth.bpc12 =
		!!(ddifdb_v2->interface_v2.interface_yuv420_color_depth & BIT(2));
	display_interface->color_depth.yuv420_color_depth.bpc14 =
		!!(ddifdb_v2->interface_v2.interface_yuv420_color_depth & BIT(3));
	display_interface->color_depth.yuv420_color_depth.bpc16 =
		!!(ddifdb_v2->interface_v2.interface_yuv420_color_depth & BIT(4));

	display_interface->yuv420_min_pixel_rate =
		ddifdb_v2->interface_v2.min_yuv420_pixel_rate;
	display_interface->audio_sample_rate =
		(ddifdb_v2->interface_v2.audio_capability & 0xe0) >> 5;
	display_interface->color_space_eotf =
		ddifdb_v2->interface_v2.color_space_and_eotf_1;
	count = ddifdb_v2->interface_v2.additional_color_space_and_eotf_count;
	if (count > 0) {
		for (i = 0; i < count; i++) {
			display_interface->cs_eotf[i].eotf =
				ddifdb_v2->interface_v2.additional_color_space_and_eotf[i] & 0xf;
			display_interface->cs_eotf[i].color_space =
				(ddifdb_v2->interface_v2.additional_color_space_and_eotf[i] & 0xf0)
								>> 4;
		}
	}
	return 0;
}

/* return section length if normal, return error code if abnormal */
static int display_id_section_parse(struct display_id_cap *disp_id_cap, u8 *disp_id_section)
{
	u8 idx = 0;
	int sec_length = SECTION_MAX_LENGTH;
	u8 bytes_in_section = 0;
	struct displayid_block *block;
	struct displayid_header *base;

	if (!disp_id_cap || !disp_id_section)
		return -1;

	base = validate_displayid(disp_id_section, sec_length, idx);
	if (!base)
		return -2;
	/* only parse below information in base displayID section */
	if (disp_id_cap->section_idx == 0) {
		disp_id_cap->version = base->rev;
		disp_id_cap->primary_use = base->prod_id;
		disp_id_cap->extension_count = base->ext_count;
	}
	bytes_in_section = base->bytes;
	idx = sizeof(*base);
	/* fixed 5 bytes for section header + section checksum */
	sec_length = 5 + bytes_in_section;

	/* iter until the byte before checksum byte */
	while (idx < sec_length - 1) {
		/* for fixed length section, the data after the
		 * last data block are filled data(all zero),
		 * it should be treated as normal, and exit parse
		 */
		block = displayid_iter_block(&disp_id_section[idx], idx, sec_length - 1);
		if (!block)
			break;
		switch (block->tag) {
		case DATA_BLOCK_PRODUCT_ID:
			break;
		case DATA_BLOCK_DISPLAY_PARAMETERS:
			break;
		case DATA_BLOCK_COLOR_CHARACTERISTICS:
			break;
		/* mandatory */
		case DATA_BLOCK_TYPE_1_DETAILED_TIMING:
			displayid_add_detailed_1_modes(disp_id_cap, block);
			break;
		case DATA_BLOCK_TYPE_2_DETAILED_TIMING:
			break;
		case DATA_BLOCK_TYPE_3_SHORT_TIMING:
			break;
		case DATA_BLOCK_TYPE_4_DMT_TIMING:
			break;
		case DATA_BLOCK_VESA_TIMING:
			break;
		case DATA_BLOCK_CEA_TIMING:
			break;
		case DATA_BLOCK_VIDEO_TIMING_RANGE:
			break;
		case DATA_BLOCK_PRODUCT_SERIAL_NUMBER:
			break;
		case DATA_BLOCK_GP_ASCII_STRING:
			break;
		case DATA_BLOCK_DISPLAY_DEVICE_DATA:
			break;
		case DATA_BLOCK_INTERFACE_POWER_SEQUENCING:
			break;
		case DATA_BLOCK_TRANSFER_CHARACTERISTICS:
			break;
		case DATA_BLOCK_DISPLAY_INTERFACE:
			displayid_parse_display_interface_v1(disp_id_cap, block);
			break;
		case DATA_BLOCK_STEREO_DISPLAY_INTERFACE:
			break;
		case DATA_BLOCK_TILED_DISPLAY:
			break;
		case DATA_BLOCK_VENDOR_SPECIFIC:
			break;
		case DATA_BLOCK_CTA:
			break;
		/* TODO */
		case DATA_BLOCK_2_PRODUCT_ID:
		case DATA_BLOCK_2_DISPLAY_PARAMETERS:
		/* mandatory */
		case DATA_BLOCK_2_TYPE_7_DETAILED_TIMING:
			displayid_add_detailed_1_modes(disp_id_cap, block);
			break;
		case DATA_BLOCK_2_TYPE_8_ENUMERATED_TIMING:
		case DATA_BLOCK_2_TYPE_9_FORMULA_TIMING:
			break;
		case DATA_BLOCK_2_DYNAMIC_VIDEO_TIMING:
			displayid_parse_dynamic_video_timing_range_limits(disp_id_cap, block);
			break;
		case DATA_BLOCK_2_DISPLAY_INTERFACE_FEATURES:
			displayid_parse_display_interface_v2(disp_id_cap, block);
			break;
		case DATA_BLOCK_2_STEREO_DISPLAY_INTERFACE:
		case DATA_BLOCK_2_TILED_DISPLAY_TOPOLOGY:
		case DATA_BLOCK_2_CONTAINER_ID:
		case DATA_BLOCK_2_VENDOR_SPECIFIC:
		default:
			break;
		}
		idx += sizeof(*block) + block->num_bytes;
	}
	return sec_length;
}

/* free allocated memory and clear displayID parse result */
static void display_id_cap_clear(struct display_id_cap *disp_id_cap)
{
	int i = 0;
	int j = 0;
	/* number of BLK_TIMING_CNT already alloc for timing */
	int alloced_blocks = 0;

	if (!disp_id_cap)
		return;

	if (disp_id_cap->timing) {
		alloced_blocks = DIV_ROUND_UP(disp_id_cap->timing_count, BLK_TIMING_CNT);
		/* free memory of each block */
		for (i = 0; i < alloced_blocks; i++) {
			for (j = 0; j < BLK_TIMING_CNT; j++)
				kfree(disp_id_cap->timing[i][j].name);
			kfree(disp_id_cap->timing[i]);
			disp_id_cap->timing[i] = NULL;
		}
		/* free pointer array */
		kfree(disp_id_cap->timing);
		disp_id_cap->timing = NULL;
	}
	memset(disp_id_cap, 0, sizeof(*disp_id_cap));
}

/* return error code if there's any abnormal, return 0 if normal */
static int display_id_parse(struct display_id_cap *disp_id_cap, u8 *disp_id_raw)
{
	int sec_len = 0;
	int ret = 0;

	if (!disp_id_cap || !disp_id_raw)
		return -1;

	display_id_cap_clear(disp_id_cap);
	while (disp_id_cap->section_idx <= disp_id_cap->extension_count) {
		sec_len = display_id_section_parse(disp_id_cap,
			&disp_id_raw[disp_id_cap->byte_idx]);
		if (sec_len < 0) {
			pr_err("exit parse section[%d] for error case: %d\n",
				disp_id_cap->section_idx, sec_len);
			ret = -2;
			break;
		}
		disp_id_cap->section_idx++;
		disp_id_cap->byte_idx += sec_len;
	}

	if (disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc8 == 0) {
		disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc8 = 1;
		pr_info("force support rgb,8bit\n");
	}
	return ret;
}

/* Table 2-3: DisplayID Base Structure Review of displayID v1.3 */
static const struct display_product_type_id display_type_id_v12[] = {
	{1, "Test Structure"},
	{2, "Display panel or other transducer"},
	{3, "desktop monitor, TV monitor, etc"},
	{4, "Television receiver"},
	{5, "Repeater/translator"},
	{6, "DIRECT DRIVE monitor"}
	/* 7~0xf: RESERVED */
};

/* Table 2-3: DisplayID Base Section of displayID v2.1a */
static const struct display_product_type_id display_type_id_v20[] = {
	{0, "Used for DisplayID Extension Sections"},
	{1, "Test Structure"},
	{2, "Generic display"},
	{3, "TV"},
	{4, "Desktop productivity display"},
	{5, "Desktop gaming display"},
	{6, "Presentation display"},
	{7, "Head-mounted Virtual Reality (VR) display"},
	{8, "Head-mounted Augmented Reality (AR) display"}
	/* 9~0xf: RESERVED */
};

/* Table 4-373: DisplayID Interface Type Codes of displayID v1.3 */
static const char *displayid_get_interface_type_name(enum disp_interface_type type)
{
	switch (type) {
	case INTERFACE_ANALOG:
		return "Analog";
	case INTERFACE_LVDS:
		return "LVDS";
	case INTERFACE_TMDS:
		return "TMDS";
	case INTERFACE_RSDS:
		return "RSDS";
	case INTERFACE_DVI_D:
		return "DVI-D";
	case INTERFACE_DVI_I_ANALOG_SECTION:
		return "DVI-I(analog section)";
	case INTERFACE_DVI_I_DIGITAL_SECTION:
		return "DVI-I(digital section)";
	case INTERFACE_HDMI_A:
		return "HDMI-A";
	case INTERFACE_HDMI_B:
		return "HDMI-B";
	case INTERFACE_MDDI:
		return "MDDI";
	case INTERFACE_DISPLAY_PORT:
		return "DisplayPort";
	default:
		return "Reserved";
	}
};

/* Table 4-417: DisplayID Content Protection of displayID v1.3 */
static const char *displayid_get_content_protection_name(enum content_protection content_pro)
{
	switch (content_pro) {
	case NO_CONTENT_PROTECTION_SUPPORT:
		return "NO_CONTENT_PROTECTION_SUPPORT";
	case HDCP:
		return "HDCP";
	case DTCP:
		return "DTCP";
	case DPCP:
		return "DPCP";
	default:
		return "INVALID";
	}
};

static const char *displayid_get_spread_supported_type_name(enum spread_supported_type type)
{
	switch (type) {
	case NOT_SPREAD_SUPPORT:
		return "NOT_SPREAD_SUPPORT";
	case DOWN_SPREAD:
		return "DOWN_SPREAD";
	case CENTER_SPREAD:
		return "CENTER_SPREAD";
	default:
		return "RESERVED";
	}
};

void display_id_cap_print(struct display_id_cap *disp_id_cap)
{
	u8 i = 0;
	enum spread_supported_type type = NOT_SPREAD_SUPPORT;
	enum content_protection cont_pro = NO_CONTENT_PROTECTION_SUPPORT;
	int array_idx = 0;
	u8 remainder = 0;
	struct tx_timing *t;

	if (!disp_id_cap)
		return;

	pr_info("display_id_ver: %x\n", disp_id_cap->version);
	pr_info("bytes number: %d\n", disp_id_cap->byte_idx);

	pr_info("display product primary usage:\n");
	if (disp_id_cap->version == 0x12) {
		for (i = 0; i < ARRAY_SIZE(display_type_id_v12); i++) {
			if (disp_id_cap->primary_use == display_type_id_v12[i].type_id) {
				pr_info("%s\n",
					display_type_id_v12[i].product_type_description);
				break;
			}
		}
		if (i == ARRAY_SIZE(display_type_id_v12))
			pr_info("Reserved\n");
	} else if (disp_id_cap->version == 0x20) {
		for (i = 0; i < ARRAY_SIZE(display_type_id_v20); i++) {
			if (disp_id_cap->primary_use == display_type_id_v20[i].type_id) {
				pr_info("%s\n",
					display_type_id_v20[i].product_type_description);
				break;
			}
		}
		if (i == ARRAY_SIZE(display_type_id_v20))
			pr_info("Reserved\n");
	}
	pr_info("extension_count: %d\n", disp_id_cap->extension_count);

	if (disp_id_cap->timing_count)
		pr_info("display_id type I and type VII timing para:\n");
	for (i = 0; i < disp_id_cap->timing_count; i++) {
		array_idx = i / BLK_TIMING_CNT;
		remainder = i % BLK_TIMING_CNT;
		t = &disp_id_cap->timing[array_idx][remainder];

		pr_info("timing[%d]: resolution: %dx%d, pixel_clock: %d, aspect_ratio: %d:%d\n",
				i, t->h_active, t->v_active, t->pixel_freq, t->h_pict, t->v_pict);
		if (t->pi_mode)
			pr_info("timing[%d]: is progressive scan frame\n", i);
		else
			pr_info("timing[%d]: is interlaced scan frame\n", i);
		if (t->flags & DISPLAY_ID_TYPE_VII_ONLY_SUPPORT_DSC)
			pr_info("timing[%d]: only support dsc\n", i);
		if (t->flags & DISPLAY_ID_TYPE_VII_YUV420)
			pr_info("timing[%d]: support yuv420\n", i);
	}
	if (disp_id_cap->version) {
		pr_info("interface_type = %s\n",
			displayid_get_interface_type_name(disp_id_cap->display_interface.type));
		if (disp_id_cap->display_interface.type == INTERFACE_ANALOG)
			pr_info("analog_interface_sub_type_code: %d\n",
				disp_id_cap->display_interface.link.analog_sub_type_codes);
		else
			pr_info("num_of_links: %d\n",
				disp_id_cap->display_interface.link.links);
		pr_info("interface_version: %d\n",
			disp_id_cap->display_interface.interface_version);
		pr_info("interface_revision: %d\n",
			disp_id_cap->display_interface.interface_revision);
		/* rgb color depth */
		pr_info("rgb_bpc6 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc6);
		pr_info("rgb_bpc8 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc8);
		pr_info("rgb_bpc10 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc10);
		pr_info("rgb_bpc12 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc12);
		pr_info("rgb_bpc14 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc14);
		pr_info("rgb_bpc16 = %d\n",
			disp_id_cap->display_interface.color_depth.rgb_color_depth.bpc16);
		/* yuv444 color depth */
		pr_info("yuv444_bpc6 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc6);
		pr_info("yuv444_bpc8 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc8);
		pr_info("yuv444_bpc10 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc10);
		pr_info("yuv444_bpc12 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc12);
		pr_info("yuv444_bpc14 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc14);
		pr_info("yuv444_bpc16 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv444_color_depth.bpc16);
		/* yuv422 color depth */
		pr_info("yuv422_bpc8 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv422_color_depth.bpc8);
		pr_info("yuv422_bpc10 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv422_color_depth.bpc10);
		pr_info("yuv422_bpc12 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv422_color_depth.bpc12);
		pr_info("yuv422_bpc14 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv422_color_depth.bpc14);
		pr_info("yuv422_bpc16 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv422_color_depth.bpc16);
		/* yuv420 color depth */
		pr_info("yuv420_bpc8 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv420_color_depth.bpc8);
		pr_info("yuv420_bpc10 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv420_color_depth.bpc10);
		pr_info("yuv420_bpc12 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv420_color_depth.bpc12);
		pr_info("yuv420_bpc14 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv420_color_depth.bpc14);
		pr_info("yuv420_bpc16 = %d\n",
			disp_id_cap->display_interface.color_depth.yuv420_color_depth.bpc16);

		cont_pro = disp_id_cap->display_interface.content_protection.protection;
		pr_info("content_protection = %s\n",
			displayid_get_content_protection_name(cont_pro));
		pr_info("content_protection_version: %d\n",
			disp_id_cap->display_interface.content_protection.version);
		pr_info("content_protection_revision: %d\n",
			disp_id_cap->display_interface.content_protection.revision);
		type = disp_id_cap->display_interface.spread_info.spread_type;
		pr_info("spread_info: %s\n",
			displayid_get_spread_supported_type_name(type));
		pr_info("spread_percentage: %d\n",
			disp_id_cap->display_interface.spread_info.spread_percentage);
		pr_info("yuv420_min_pixel_rate: %d\n",
			disp_id_cap->display_interface.yuv420_min_pixel_rate);
		pr_info("audio_sample_rate: %d\n",
			disp_id_cap->display_interface.audio_sample_rate);
		pr_info("color_space_eotf: %d\n",
			disp_id_cap->display_interface.color_space_eotf);
	}

	if (disp_id_cap->dynamic_video_timing_range.pixel_clock_min ||
			disp_id_cap->dynamic_video_timing_range.pixel_clock_max)
		pr_info("dynamic_video_timing_pixel_clock_range: [%dHz, %dHz]\n",
				disp_id_cap->dynamic_video_timing_range.pixel_clock_min,
				disp_id_cap->dynamic_video_timing_range.pixel_clock_max);
	if (disp_id_cap->dynamic_video_timing_range.vertical_refresh_rate_min ||
			disp_id_cap->dynamic_video_timing_range.vertical_refresh_rate_max)
		pr_info("dynamic_video_refresh_rate_range: [%dHz, %dHz]\n",
				disp_id_cap->dynamic_video_timing_range.vertical_refresh_rate_min,
				disp_id_cap->dynamic_video_timing_range.vertical_refresh_rate_max);
	pr_info("seamless_support: %d\n",
			disp_id_cap->dynamic_video_timing_range.seamless_support_flag);
}
