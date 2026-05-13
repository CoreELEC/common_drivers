// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_timing.h>
#include "edptx_common.h"

#define DPTX_EDID_READ_RETRY_MAX   5
#define DPTX_AUX_I2C_READ_BYTES    16

#define DPTX_EDID_SIZE             128

static const unsigned char base_block_header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};

//EDID Base Block Identification                     ****
#define EDID_BASE_BLOCK_ID_SN                         0xff
#define EDID_BASE_BLOCK_ID_ASCII_STR                  0xfe
#define EDID_BASE_BLOCK_ID_RANGE_TIMING               0xfd
#define EDID_BASE_BLOCK_ID_PRODUCT_NAME               0xfc
#define EDID_BASE_BLOCK_ID_DETAIL_TIMING              0x01
//EDID Extended Block Header                         ****
#define EXTENDED_HEADER_EDID_CEA_861                  0x02
#define EXTENDED_HEADER_DisplayID                     0x70
//DisplayID 1.3/2.3 Product Identification           ****
#define DisplayID_Display_Parameters_1                0x01
#define DisplayID_Color_Characteristics               0x02
#define DisplayID_Type_I_Timing                       0x03
#define DisplayID_Type_II_Timing                      0x04
#define DisplayID_Type_III_Timing                     0x05
#define DisplayID_Type_IV_Timing                      0x06
#define DisplayID_VESA_Timing_Standard                0x07
#define DisplayID_CEA_Timing_Standard                 0x08
#define DisplayID_Video_Timing_Range                  0x09
#define DisplayID_Product_SerialNumber                0x0A
#define DisplayID_General_Purpose_ASCII_String	      0x0B
#define DisplayID_Display_Device_Data                 0x0C
#define DisplayID_Interface_Power_Sequencing          0x0D
#define DisplayID_Transfer_Characteristics            0x0E
#define DisplayID_Display_Interface_Data              0x0F
#define DisplayID_Stereo_Display_Interface_1          0x10
#define DisplayID_Type_V_Timing                       0x11
#define DisplayID_Type_VI_Timing                      0x13
#define DisplayID_Vendor_Specific_1                   0x7F
#define DisplayID_Product_Identification              0x20
#define DisplayID_Display_Parameters_2                0x21
#define DisplayID_Type_VII_Detailed_Timing            0x22
#define DisplayID_Type_VIII_Enumerated_Timing_Code    0x23
#define DisplayID_Type_IX_Formula_Based_Timing        0x24
#define DisplayID_Dynamic_Video_Timing_Range_Limits   0x25
#define DisplayID_Display_Interface_Features          0x26
#define DisplayID_Stereo_Display_Interface_2          0x27
#define DisplayID_Tiled_Display_Topology              0x28
#define DisplayID_ContainerID                         0x29
#define DisplayID_Vendor_Specific_2                   0x7E
#define DisplayID_CTA_DisplayID                       0x81
//EDID CEA-861 Block Identification                  ****
#define EDID_CEA_861_TAG_AUDIO                        0x01
#define EDID_CEA_861_TAG_VIDEO                        0x02
#define EDID_CEA_861_TAG_VENDOR_SPEC                  0x03
#define EDID_CEA_861_TAG_SPEAKER_ALLOCATION           0x04
#define EDID_CEA_861_TAG_VESA_DSIP_TRANS              0x05
#define EDID_CEA_861_TAG_EXT_FLAG                     0x07

void dptx_edid_print_raw(unsigned char *_buf)
{
	unsigned short c;
	unsigned char i, j, n, edid_idx, edid_blocks;
	char edid_pr_buf[64];

	edid_blocks = 1 + _buf[126];

	for (edid_idx = 0; edid_idx < edid_blocks; edid_idx++) {
		pr_info("EDID block %u Raw data:\n", edid_idx);
		for (i = 0; i < 128; i += 16) {
			n = 0;
			memset(edid_pr_buf, 0, 64);
			for (j = 0; j < 16; j++) {
				c = edid_idx * 128 + i + j;
				n += sprintf(edid_pr_buf + n, " %02x", _buf[c]);
			}
			pr_info("%s\n", edid_pr_buf);
		}
	}
}

void dptx_edid_print_parsed(struct edptx_drv_s *edp, u8 port)
{
	unsigned char i;
	struct edptx_edid_info_s *edid;

	edid = &edp->sink.edid;

	pr_info("Manufacturer: %s\n"
		"Product ID:   0x%04x\n"
		"Product SN:   0x%08x\n"
		"Product Time: week %d of %d\n"
		"EDID Version: %04x\n",
		edid->manufacturer_id, edid->product_id, edid->product_sn,
		edid->week, edid->year + 1990, edid->version);
	if (edid->name[0])
		pr_info("Monitor Name: %s\n", edid->name);
	if (edid->asc_string[0])
		pr_info("Monitor Text: %s\n", edid->asc_string);
	if (edid->serial_num[0])
		pr_info("Monitor SN:   %s\n", edid->serial_num);
	if (edid->range.vfreq[1]) {
		pr_info("Range Limit:\n"
			"  V freq:     %u - %u Hz  (Min blank: %u)\n"
			"  H freq:     %u - %u kHz (Min blank: %u)\n"
			"  Pixel Clk:  %u - %u kHz\n",
			edid->range.vfreq[0], edid->range.vfreq[1], edid->range.v_blank_min,
			edid->range.hfreq[0], edid->range.hfreq[1], edid->range.h_blank_min,
			edid->range.pclk[0] / 1000, edid->range.pclk[1] / 1000);
	}
	for (i = 0; i < edp->panel_data.timing_cnt; i++) {
		pr_info("Detail Timing Descriptor[%d]:  * Pixel Clock: %u.%u MHz\n"
			"  H: Active:%4u Blank:%4u FP:%3u PW:%2u %s Size:%4umm Border:%u\n"
			"  V: Active:%4u Blank:%4u FP:%3u PW:%2u %s Size:%4umm Border:%u\n",
			i, edp->panel_data.timing[i].pclk / 1000000,
			(edp->panel_data.timing[i].pclk % 1000000) / 1000,
			edp->panel_data.timing[i].h_act, edp->panel_data.timing[i].h_blank,
			edp->panel_data.timing[i].h_fp, edp->panel_data.timing[i].h_pw,
			edp->panel_data.timing[i].ctrl & 0x1 ? "P" : "N",
			edp->panel_data.timing[i].h_size,
			//edp->panel_data.timing[i].h_border,
			0,
			edp->panel_data.timing[i].v_act, edp->panel_data.timing[i].v_blank,
			edp->panel_data.timing[i].v_fp, edp->panel_data.timing[i].v_pw,
			edp->panel_data.timing[i].ctrl & 0x2 ? "P" : "N",
			edp->panel_data.timing[i].v_size,
			//edp->panel_data.timing[i].v_border);
			0);
	}
}

// need fill bp, total
// return 0=good. 1=warn. 2=severe-fix. 3=error
static u8 dptx_edid_dtd_complete(struct edptx_drv_s *edp, struct dptx_detail_timing_s *timing)
{
	u32 fp, blank, pw, ret = 0;

	if (timing->h_act == 0 || timing->v_act == 0) {
		eDP_ERR(edp, "checked invalid dtd para act=%ux%u", timing->h_act, timing->v_act);
		return 4;
	}

	fp = timing->h_fp;
	pw = timing->h_pw;
	blank = timing->h_blank;
	if (fp == 0) {
		eDP_ERR(edp, "invalid H-fp 0, force to 12");
		fp = 12;
		ret = 3;
	}
	if (fp < 8 || fp > 512) {
		eDP_DBG(edp, " ! fp=%u", fp);
		ret = 2;
	}

	if (pw == 0) {
		eDP_ERR(edp, "invalid H-pw 0, force to 8");
		pw = 8;
		ret = 3;
	}
	if (pw < 4 || pw > 512) {
		eDP_DBG(edp, " ! pw=%u", fp);
		ret = 2;
	}

	if (blank == 0 || (blank <= (fp + pw))) {
		eDP_ERR(edp, "invalid H-blank(%u), force to pw + fp + 8", blank);
		blank = pw + fp + 8;
		ret = 3;
	}
	if (blank < (pw + fp + 8) || blank > 1024) {
		eDP_DBG(edp, " ! H-blank=%u", blank);
		ret = 2;
	}

	timing->h_fp = fp;
	timing->h_pw = pw;
	timing->h_blank = blank;
	timing->h_bp = blank - pw - fp;
	timing->h_period = blank + timing->h_act;

	fp = timing->v_fp;
	pw = timing->v_pw;
	blank = timing->v_blank;
	if (fp == 0) {
		eDP_ERR(edp, "invalid V-fp 0, force to 12");
		fp = 12;
		ret = 3;
	}
	if (fp < 8 || fp > 512) {
		eDP_DBG(edp, " ! fp=%u", fp);
		ret = 2;
	}

	if (pw == 0) {
		eDP_ERR(edp, "invalid V-pw 0, force to 8");
		pw = 8;
		ret = 3;
	}
	if (pw < 4 || pw > 512) {
		eDP_DBG(edp, " ! pw=%u", fp);
		ret = 2;
	}

	if (blank == 0 || (blank <= (fp + pw))) {
		eDP_ERR(edp, "invalid V-blank(%u), force to pw + fp + 8", blank);
		blank = pw + fp + 8;
		ret = 3;
	}
	if (blank < (pw + fp + 8) || blank > 1024) {
		eDP_DBG(edp, " ! V-blank=%u", blank);
		ret = 2;
	}

	timing->v_fp = fp;
	timing->v_pw = pw;
	timing->v_blank = blank;
	timing->v_bp = blank - pw - fp;
	timing->v_period = blank + timing->v_act;

	return ret;
}

void temp_timing_add_to_timing_list(struct edptx_drv_s *edp, struct dptx_detail_timing_s *timing)
{
	u16 h_period, h_active, hsync_width, hsync_bp;//, hsync_fp;//, h_blank;//, hsync_pol;
	u16 v_period, v_active, vsync_width, vsync_bp;//, vsync_fp;//, v_blank;//, vsync_pol;
	u64 fr_temp;
	u8 port_cnt, i, k;

	port_cnt = count_bit(edp->sink.port_mask);
	timing->h_period *= port_cnt;
	timing->h_act    *= port_cnt;
	timing->h_bp     *= port_cnt;
	timing->h_pw     *= port_cnt;
	timing->h_fp     *= port_cnt;
	timing->h_blank  *= port_cnt;
	timing->pclk     *= port_cnt;

	for (i = 0; i < edp->panel_data.timing_cnt; i++) {
		h_period    = edp->panel_data.timing[i].h_period;
		h_active    = edp->panel_data.timing[i].h_act;
		hsync_bp    = edp->panel_data.timing[i].h_bp;
		hsync_width = edp->panel_data.timing[i].h_bp;
		// hsync_fp    = edp->panel_data.timing[i].h_fp;
		// h_blank     = edp->panel_data.timing[i].h_blank;
		// hsync_pol   = edp->panel_data.timing[i].hsync_pol;

		v_period    = edp->panel_data.timing[i].v_period;
		v_active    = edp->panel_data.timing[i].v_act;
		vsync_bp    = edp->panel_data.timing[i].v_bp;
		vsync_width = edp->panel_data.timing[i].v_pw;
		// vsync_fp    = edp->panel_data.timing[i].v_fp;
		// v_blank     = edp->panel_data.timing[i].v_blank;
		// vsync_pol   = edp->panel_data.timing[i].vsync_pol;

		// pclk        = edp->panel_data.timing[i].pclk;

		if (timing->h_period == h_period && timing->h_act == h_active &&
		    timing->h_bp == hsync_bp && timing->h_pw  == hsync_width &&
		    timing->v_period == v_period && timing->v_act == v_active &&
		    timing->v_bp == vsync_bp && timing->v_pw  == vsync_width) {
			fr_temp = 1000U;
			fr_temp = fr_temp * timing->pclk;
			fr_temp = dptx_div_around(fr_temp, timing->h_period);
			fr_temp = dptx_div_around(fr_temp, timing->v_period);
			fr_temp = dptx_div_around(fr_temp, 1000);

			for (k = 0; k < 8; k++) {
				if (edp->panel_data.timing[i].vmode_add_fr[k] == fr_temp)
					return;
				if (edp->panel_data.timing[i].vmode_add_fr[k] == 0) {
					edp->panel_data.timing[i].vmode_add_fr[k] = fr_temp;
					eDP_DBG(edp, "%s: EDID timing[%u] add fr[%u]=%llu",
						__func__, i, k, fr_temp);
					return;
				}
			}
		}
	}
	//add new timing
	if (edp->panel_data.timing_cnt == DPTX_DRV_TIMING_MAX) {
		eDP_ERR(edp, "%s: EDID timing full", __func__);
		return;
	}

	memcpy(&edp->panel_data.timing[i], timing, sizeof(struct dptx_detail_timing_s));
	eDP_DBG(edp, "%s: EDID add timing[%u]", __func__, edp->panel_data.timing_cnt);
	edp->panel_data.timing_cnt++;
}

static char dptx_edid_base_block_parse(struct edptx_drv_s *edp, u8 port, unsigned char *_buf)
{
	// struct edptx_link_cfg_s *link = edp->sink.link[port];
	struct dptx_edid_range_limit_s *range = &edp->sink.edid.range;
	// struct dptx_detail_timing_s *timing;
	u64 temp1;
	unsigned int temp;
	unsigned int checksum = 0;
	unsigned short i, j;
	char *ret;
	struct dptx_detail_timing_s tmg_t;

	if (memcmp(_buf, base_block_header, 8)) {
		eDP_P_ERR(edp, port, "%s: invalid EDID header", __func__);
		return 1;
	}

	for (i = 0; i < 128; i++)
		checksum += _buf[i];
	if ((checksum & 0xff)) {
		eDP_P_ERR(edp, port, "%s: EDID checksum Wrong", __func__);
		return 1;
	}

	memset(&edp->sink.edid, 0, sizeof(struct edptx_edid_info_s));

	temp = ((_buf[8] << 8) | _buf[9]);
	for (i = 0; i < 3; i++)
		edp->sink.edid.manufacturer_id[i] = (((temp >> ((2 - i) * 5)) & 0x1f) - 1) + 'A';

	edp->sink.edid.manufacturer_id[3] = '\0';
	temp = ((_buf[11] << 8) | _buf[10]);
	edp->sink.edid.product_id = temp;
	temp = ((_buf[12] << 24) | (_buf[13] << 16) | (_buf[14] << 8) | _buf[15]);
	edp->sink.edid.product_sn = temp;
	edp->sink.edid.week = _buf[16];
	edp->sink.edid.year = _buf[17];
	temp = ((_buf[18] << 8) | _buf[19]);
	edp->sink.edid.version = temp;

	if (!(_buf[20] & 0x80) || ((_buf[20] & 0xf) != 0x5))
		eDP_P_ERR(edp, port, "non digital or not DP device [%2x]", _buf[20]);

	edp->sink.edid.cfmt_support = 1 << DPTX_CFMT_RGB_6bit;
	//[20]BIT[4:6]: bpc: 000=undefined, 001=6, 010=8, 011=10, 100=12, 101=14, 110=16
	//[24]BIT[3:4]: RGB + BIT[0]=YUV444/BIT[1]=YUV422
	if (((_buf[20] >> 4) & 0x7) >= 0x4) {
		edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_RGB_12bit;
		if (_buf[24] & 0x08)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr444_12bit;
		if (_buf[24] & 0x10)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr422_12bit;
	}
	if (((_buf[20] >> 4) & 0x7) >= 0x3) {
		edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_RGB_10bit;
		if (_buf[24] & 0x08)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr444_10bit;
		if (_buf[24] & 0x10)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr422_10bit;
	}
	if (((_buf[20] >> 4) & 0x7) >= 0x2) {
		edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_RGB_8bit;
		if (_buf[24] & 0x08)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr444_8bit;
		if (_buf[24] & 0x10)
			edp->sink.edid.cfmt_support |= 1 << DPTX_CFMT_YCbCr422_8bit;
	}

	for (i = 0; i < 4; i++) {
		j = 54 + i * 18;
		if (_buf[j] || _buf[j + 1]) {//detail timing
			memset(&tmg_t, 0, sizeof(struct dptx_detail_timing_s));

			tmg_t.pclk    = ((_buf[j + 1] << 8) | (_buf[j])) * 10000;
			tmg_t.h_act   = (((_buf[j + 4] >> 4) & 0xf) << 8) | _buf[j + 2];
			tmg_t.h_blank = (((_buf[j + 4] >> 0) & 0xf) << 8) | _buf[j + 3];
			tmg_t.v_act   = (((_buf[j + 7] >> 4) & 0xf) << 8) | _buf[j + 5];
			tmg_t.v_blank = (((_buf[j + 7] >> 0) & 0xf) << 8) | _buf[j + 6];
			tmg_t.h_fp    = (((_buf[j + 11] >> 6) & 0x3) << 8) | _buf[j + 8];
			tmg_t.h_pw    = (((_buf[j + 11] >> 4) & 0x3) << 8) | _buf[j + 9];
			tmg_t.v_fp    = (((_buf[j + 11] >> 2) & 0x3) << 4) |
						((_buf[j + 10] >> 4) & 0xf);
			tmg_t.v_pw    = (((_buf[j + 11] >> 0) & 0x3) << 4) |
						((_buf[j + 10] >> 0) & 0xf);
			tmg_t.h_size  = (((_buf[j + 14] >> 4) & 0xf) << 8) | _buf[j + 12];
			tmg_t.v_size  = (((_buf[j + 14] >> 0) & 0xf) << 8) | _buf[j + 13];
			//tmg_t.h_border = _buf[j + 15];
			//tmg_t.v_border = _buf[j + 16];
			tmg_t.ctrl   |= _buf[j + 17] & 0x1 ? CTRL_HSYNC_POS : 0;
			tmg_t.ctrl   |= _buf[j + 17] & 0x2 ? CTRL_VSYNC_POS : 0;

			dptx_edid_dtd_complete(edp, &tmg_t);

			temp1 = tmg_t.pclk;
			temp1 *= 1000;
			temp  = tmg_t.h_period * tmg_t.v_period;
			tmg_t.fr1000 = dptx_div_around(temp1, temp);

			temp_timing_add_to_timing_list(edp, &tmg_t);
		}

		//some panel didn`t follow spec, keep compatibility
		//if (!(_buf[j] || _buf[j + 1] || _buf[j + 2] || _buf[j + 4])) {
		if (!(_buf[j] || _buf[j + 1] || _buf[j + 2])) {
			switch (_buf[j + 3]) {
			case EDID_BASE_BLOCK_ID_PRODUCT_NAME: //monitor name
				memcpy(edp->sink.edid.name, &_buf[j + 5], 13);
				ret = strstr(edp->sink.edid.name, "\n");
				if (ret)
					edp->sink.edid.name[ret - edp->sink.edid.name] = '\0';
				edp->sink.edid.name[13] = '\0';
				break;
			case EDID_BASE_BLOCK_ID_RANGE_TIMING: //monitor range limits
				range->vfreq[0] = _buf[j + 5] + (_buf[j + 4] & 0x1 ? 255 : 0);
				range->vfreq[1] = _buf[j + 6] + (_buf[j + 4] & 0x2 ? 255 : 0);
				range->hfreq[0] = _buf[j + 7] + (_buf[j + 4] & 0x8 ? 255 : 0);
				range->hfreq[1] = _buf[j + 8] + (_buf[j + 4] & 0x4 ? 255 : 0);
				range->pclk[1]  = _buf[j + 9] * 10 * 1000000; //Hz
				range->pclk[0]  = 0; //Hz
				break;
			case EDID_BASE_BLOCK_ID_ASCII_STR: //ascii string
				memcpy(edp->sink.edid.asc_string, &_buf[j + 5], 13);
				ret = strstr(edp->sink.edid.asc_string, "\n");
				if (ret)
					edp->sink.edid.asc_string
						[ret - edp->sink.edid.asc_string] = '\0';
				edp->sink.edid.asc_string[13] = '\0';
				break;
			case EDID_BASE_BLOCK_ID_SN: //monitor serial num
				memcpy(edp->sink.edid.serial_num, &_buf[j + 5], 13);
				ret = strstr(edp->sink.edid.serial_num, "\n");
				if (ret)
					edp->sink.edid.serial_num
						[ret - edp->sink.edid.serial_num] = '\0';
				edp->sink.edid.serial_num[13] = '\0';
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static char dptx_edid_DisplayID_parse(struct edptx_drv_s *edp, u8 port, unsigned char *_buf)
{
	struct dptx_detail_timing_s tmg_t;
	unsigned int checksum = 0, temp;
	// struct edptx_link_cfg_s *link = edp->sink.link[port];
	u64 temp1;
	int i;
	unsigned char tag_ofst, d_pos, dtd_cnt;

	for (i = 0; i < 128; i++)
		checksum += _buf[i];
	if ((checksum & 0xff)) {
		eDP_P_ERR(edp, port, "%s: DisplayID checksum Wrong\n", __func__);
		return -1;
	}

	for (tag_ofst = 5; tag_ofst < 126;) {
		switch (_buf[tag_ofst]) {
		//DisplayID v1.x
		case DisplayID_Type_I_Timing: //03h
			dtd_cnt = _buf[tag_ofst + 2] / 20;

			for (i = 0; i < dtd_cnt; i++) { //Type I Detailed Timing Descriptor
				d_pos = tag_ofst + 3 + 20 * i;
				memset(&tmg_t, 0, sizeof(struct dptx_detail_timing_s));

				tmg_t.pclk = ((_buf[d_pos + 2] << 16) | (_buf[d_pos + 1] << 8)  |
						(_buf[d_pos + 0] << 0)) * 10000;
				tmg_t.ctrl |= _buf[d_pos + 3] & 0x80 ? CTRL_PREFERRED_TIMING : 0;
				tmg_t.h_act   = 1 +  (_buf[d_pos + 4] | (_buf[d_pos + 5] << 8));
				tmg_t.h_blank = 1 +  (_buf[d_pos + 6] | (_buf[d_pos + 7] << 8));
				tmg_t.h_fp    = 1 +  (_buf[d_pos + 8] |
								((_buf[d_pos + 9] & 0x7f) << 8));
				tmg_t.h_pw    = 1 + (_buf[d_pos + 10] | (_buf[d_pos + 11] << 8));
				tmg_t.v_act   = 1 + (_buf[d_pos + 12] | (_buf[d_pos + 13] << 8));
				tmg_t.v_blank = 1 + (_buf[d_pos + 14] | (_buf[d_pos + 15] << 8));
				tmg_t.v_fp    = 1 + (_buf[d_pos + 16] |
								((_buf[d_pos + 17] & 0x7f) << 8));
				tmg_t.v_pw    = 1 + (_buf[d_pos + 18] |
								((_buf[d_pos + 19] & 0x7f) << 8));
				tmg_t.ctrl |=  (_buf[d_pos + 9] & 0x80) ? CTRL_HSYNC_POS : 0;
				tmg_t.ctrl |= (_buf[d_pos + 17] & 0x80) ? CTRL_VSYNC_POS : 0;

				dptx_edid_dtd_complete(edp, &tmg_t);

				temp1 = 1000ULL * tmg_t.pclk;
				temp  = tmg_t.h_period * tmg_t.v_period;
				tmg_t.fr1000 = dptx_div_around(temp1, temp);

				temp_timing_add_to_timing_list(edp, &tmg_t);
			}
			break;
		case DisplayID_Video_Timing_Range: //09h
			//range->pclk[0] = ((_buf[tag_ofst + 5] << 16) |
			//		  (_buf[tag_ofst + 4] << 8)  |

			break;
		case DisplayID_Type_II_Timing:
		case DisplayID_Type_IV_Timing:
		case DisplayID_Type_III_Timing:
		case DisplayID_Display_Parameters_1:
		case DisplayID_Color_Characteristics:
		case DisplayID_VESA_Timing_Standard:
		case DisplayID_CEA_Timing_Standard:
		case DisplayID_Product_SerialNumber:
		case DisplayID_General_Purpose_ASCII_String:
		case DisplayID_Display_Device_Data:
		case DisplayID_Interface_Power_Sequencing:
		case DisplayID_Transfer_Characteristics:
		case DisplayID_Display_Interface_Data:
		case DisplayID_Stereo_Display_Interface_1:
		case DisplayID_Type_V_Timing:
		case DisplayID_Type_VI_Timing:
		case DisplayID_Vendor_Specific_1:
			break;
		//DisplayID v2.x
		case DisplayID_Type_VII_Detailed_Timing:
		case DisplayID_Display_Interface_Features:
			break;
		case DisplayID_Product_Identification:
		case DisplayID_Display_Parameters_2:
		case DisplayID_Type_VIII_Enumerated_Timing_Code:
		case DisplayID_Type_IX_Formula_Based_Timing:
		case DisplayID_Dynamic_Video_Timing_Range_Limits:
		case DisplayID_Stereo_Display_Interface_2:
		case DisplayID_Tiled_Display_Topology:
		case DisplayID_ContainerID:
		case DisplayID_Vendor_Specific_2:
		case DisplayID_CTA_DisplayID:
			break;
		default:
			tag_ofst = 126;
			break;
		}
		tag_ofst += 2 + _buf[tag_ofst + 2];
	}

	return 0;
}

static char dptx_edid_CEA_861_parse(struct edptx_drv_s *edp, u8 port, unsigned char *_buf)
{
	u32 temp, checksum = 0;
	u64 temp1;
	u8 i, j;
	unsigned char dtd_ofst, tag, count;//, native_dtd_cnt;
	struct dptx_detail_timing_s tmg_t;

	for (i = 0; i < 128; i++)
		checksum += _buf[i];
	if ((checksum & 0xff)) {
		eDP_P_ERR(edp, port, "%s: EDID_CEA_861 checksum Wrong\n", __func__);
		// return -1;
	}

	dtd_ofst = _buf[2];

	for (j = 4; j < dtd_ofst;) {
		tag = _buf[j] >> 5;
		count = _buf[j] & 0x1f;
		switch (tag) {
		case EDID_CEA_861_TAG_VIDEO:
			//TODO
			j += count + 1;
			break;
		case EDID_CEA_861_TAG_AUDIO:
		case EDID_CEA_861_TAG_VENDOR_SPEC:
		case EDID_CEA_861_TAG_SPEAKER_ALLOCATION:
		case EDID_CEA_861_TAG_VESA_DSIP_TRANS:
		default:
			j += count + 1;
			break;
		}
	}

	for (i = 0; i < 8; i++) {
		j = dtd_ofst + 18 * i;

		if (j > 128 || !_buf[j])
			break;

		memset(&tmg_t, 0, sizeof(struct dptx_detail_timing_s));

		tmg_t.pclk    =                ((_buf[j + 1] << 8) | _buf[j]) * 10000;
		tmg_t.h_act   =  (((_buf[j + 4] >> 4) & 0xf) << 8) | _buf[j + 2];
		tmg_t.h_blank =  (((_buf[j + 4] >> 0) & 0xf) << 8) | _buf[j + 3];
		tmg_t.v_act   =  (((_buf[j + 7] >> 4) & 0xf) << 8) | _buf[j + 5];
		tmg_t.v_blank =  (((_buf[j + 7] >> 0) & 0xf) << 8) | _buf[j + 6];
		tmg_t.h_fp    = (((_buf[j + 11] >> 6) & 0x3) << 8) | _buf[j + 8];
		tmg_t.h_pw    = (((_buf[j + 11] >> 4) & 0x3) << 8) | _buf[j + 9];
		tmg_t.v_fp    = (((_buf[j + 11] >> 2) & 0x3) << 4) | ((_buf[j + 10] >> 4) & 0xf);
		tmg_t.v_pw    = (((_buf[j + 11] >> 0) & 0x3) << 4) | ((_buf[j + 10] >> 0) & 0xf);
		tmg_t.h_size  = (((_buf[j + 14] >> 4) & 0xf) << 8) | _buf[j + 12];
		tmg_t.v_size  = (((_buf[j + 14] >> 0) & 0xf) << 8) | _buf[j + 13];
		//timing->h_border = _buf[j + 15];
		//timing->v_border = _buf[j + 16];
		tmg_t.ctrl = (_buf[j + 17] & 0x1 ? CTRL_HSYNC_POS : 0) |
				(_buf[j + 17] & 0x2 ? CTRL_VSYNC_POS : 0);

		dptx_edid_dtd_complete(edp, &tmg_t);

		temp1 = 1000ULL * tmg_t.pclk;
		temp  = tmg_t.h_period * tmg_t.v_period;
		tmg_t.fr1000 = dptx_div_around(temp1, temp);

		temp_timing_add_to_timing_list(edp, &tmg_t);
	}
	return 0;
}

static char dptx_edid_ext_block_parse(struct edptx_drv_s *edp, u8 port, unsigned char *_buf)
{
	if (_buf[0] == EXTENDED_HEADER_DisplayID)
		return dptx_edid_DisplayID_parse(edp, port, _buf);
	else if (_buf[0] == EXTENDED_HEADER_EDID_CEA_861)
		return dptx_edid_CEA_861_parse(edp, port, _buf);
	else if (_buf[0] == 0x0)
		return 0;

	eDP_P_ERR(edp, port, "%s: not a DisplayID or EDID CEA-861", __func__);
	return 0;
}

static int dptx_read_edid(struct edptx_drv_s *edp, u8 port, unsigned char *edid_buf)
{
	int i, ret;
	unsigned char aux_data;
	unsigned char read_count = 128 / DPTX_AUX_I2C_READ_BYTES;
	unsigned char ext_block_cnt = 0, retry_cnt;

	if (!edid_buf) {
		eDP_ERR(edp, "%s: edid buf is null", __func__);
		return -1;
	}

	retry_cnt = 0;
edid_read_retry_p0:
	aux_data = 0x00;
	ret = edptx_if_aux_i2c_op(edp, port, eDPTX_AUX_CMD_I2C_WRITE_MOT, 0x50, 1, &aux_data);
	if (ret)
		return ret;

	for (i = 0; i < read_count; i++) {
		ret = edptx_if_aux_i2c_op(edp, port,
			(i == (read_count - 1)) ?
				eDPTX_AUX_CMD_I2C_READ : eDPTX_AUX_CMD_I2C_READ_MOT,
			0x50, 16, &edid_buf[i * 16]);
		if (ret && (retry_cnt++ < 7))
			goto edid_read_retry_p0;
	}
	ret = dptx_edid_base_block_parse(edp, port, edid_buf);
	if (ret) {
		if (retry_cnt++ < 7)
			goto edid_read_retry_p0;
		else
			return -1;
	}

	ext_block_cnt = edid_buf[126];
	if (ext_block_cnt > 8)
		return -1;

	retry_cnt = 0;
edid_read_retry_p1:
	if (ext_block_cnt >= 1) {
		aux_data = 0x80; //read from 0x80
		ret = edptx_if_aux_i2c_op(edp, port,
					eDPTX_AUX_CMD_I2C_WRITE_MOT, 0x50, 1, &aux_data);
		if (ret)
			return ret;
		for (i = 0; i < read_count; i++) {
			ret = edptx_if_aux_i2c_op(edp, port,
				(i == (read_count - 1)) ?
					eDPTX_AUX_CMD_I2C_READ : eDPTX_AUX_CMD_I2C_READ_MOT,
				0x50, 16, &edid_buf[128 + i * 16]);
			if (ret && (retry_cnt++ < 7))
				goto edid_read_retry_p1;
		}
		dptx_edid_ext_block_parse(edp, port, edid_buf + 128);
		if (ret) {
			if (retry_cnt++ < 7)
				goto edid_read_retry_p1;
			else
				return -1;
		}
	}

	retry_cnt = 0;
edid_read_retry_p2:
	if (ext_block_cnt >= 2) {
		aux_data = 0x01; //switch, do as Intel do.
		ret = edptx_if_aux_i2c_op(edp, port,
					eDPTX_AUX_CMD_I2C_WRITE_MOT, 0x30, 1, &aux_data);
		if (ret)
			return ret;
		aux_data = 0x00; //read from 0x80
		ret = edptx_if_aux_i2c_op(edp, port,
					eDPTX_AUX_CMD_I2C_WRITE_MOT, 0x50, 1, &aux_data);
		if (ret)
			return ret;
		for (i = 0; i < read_count; i++) {
			ret = edptx_if_aux_i2c_op(edp, port,
				(i == (read_count - 1)) ?
					eDPTX_AUX_CMD_I2C_READ : eDPTX_AUX_CMD_I2C_READ_MOT,
				0x50, 16, &edid_buf[256 + i * 16]);
			if (ret && (retry_cnt++ < 7))
				goto edid_read_retry_p2;
		}
		dptx_edid_ext_block_parse(edp, port, edid_buf + 256);
		if (ret) {
			if (retry_cnt++ < 7)
				goto edid_read_retry_p2;
			else
				return -1;
		}
	}

	retry_cnt = 0;
edid_read_retry_p3:
	if (ext_block_cnt >= 3) {
		aux_data = 0x80; //read from 0x80
		ret = edptx_if_aux_i2c_op(edp, port,
					eDPTX_AUX_CMD_I2C_WRITE_MOT, 0x50, 1, &aux_data);
		if (ret)
			return ret;
		for (i = 0; i < read_count; i++) {
			ret = edptx_if_aux_i2c_op(edp, port,
				(i == (read_count - 1)) ?
					eDPTX_AUX_CMD_I2C_READ : eDPTX_AUX_CMD_I2C_READ_MOT,
				0x50, 16, &edid_buf[384 + i * 16]);
			if (ret && (retry_cnt++ < 7))
				goto edid_read_retry_p3;
		}
		dptx_edid_ext_block_parse(edp, port, edid_buf + 256);
		if (ret) {
			if (retry_cnt++ < 7)
				goto edid_read_retry_p3;
			else
				return -1;
		}
	}

	return ret;
}

/*
 * static void edptx_edid_crc_cal(struct edptx_drv_s *edp)
 * {
 *	u8 i, crc = 0;
 *
 *	for (i = 0; i < 14; i++)
 *		crc += edp->sink.exp_edid.name[i];
 *
 *	crc += edp->sink.exp_edid.range.vfreq[0];
 *	crc += edp->sink.exp_edid.range.vfreq[1];
 *
 *	crc += edp->sink.exp_edid.dtd_cnt;
 *
 *	for (i = 0; i < edp->sink.exp_edid.dtd_cnt; i++) {
 *		crc += edp->sink.exp_edid.dtd_timing[i].h_period;
 *		crc += edp->sink.exp_edid.dtd_timing[i].h_act;
 *		crc += edp->sink.exp_edid.dtd_timing[i].v_period;
 *		crc += edp->sink.exp_edid.dtd_timing[i].v_act;
 *		crc += edp->sink.exp_edid.dtd_timing[i].pclk;
 *	}
 *	eDP_DBG(edp, "sink EDID crc: 0x%2x", crc);
 *	edp->sink.exp_edid.edid_crc = crc;
 * }
 */

//read & parse EDID
int edptx_EDID_probe(struct edptx_drv_s *edp, u8 check_crc)
{
	unsigned char *edid_buf;
	int ret;
	u8 retry_cnt = 0, port = 0, res = 0;

	if (!edp)
		return -1;

	// read port 0 EDID only
	edid_buf = kzalloc(512 * sizeof(unsigned char), GFP_KERNEL);
	if (!edid_buf) {
		eDP_ERR(edp, "EDID buffer malloc failed");
		return -1;
	}

dptx_EDID_probe_retry:
	memset(edid_buf, 0, 512 * sizeof(unsigned char));
	ret = dptx_read_edid(edp, port, edid_buf);
	if (ret) {
		eDP_P_ERR(edp, port, "%s: read fail @%d", __func__, retry_cnt);
		if (retry_cnt++ < DPTX_EDID_READ_RETRY_MAX)
			goto dptx_EDID_probe_retry;
	}

	if (edptx_print_level >= LOG_V) {
		dptx_edid_print_raw(edid_buf);
		dptx_edid_print_parsed(edp, port);
	}

	if (ret) {
		eDP_ERR(edp, "%s failed, port result = 0x%x", __func__, res);
		kfree(edid_buf);
		return -1;
	}

	// edptx_edid_crc_cal(edp);
	// dptx_edid_store(edp, edid_buf);

	eDP_PR(edp, "%s ok", __func__);
	kfree(edid_buf);
	return 0;
}
