// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>

#include "hdmitx_sysfs_common.h"
#include "hdmitx_log.h"
#include "hdmitx_compliance.h"
#include "hdmitx_check_valid.h"
#include "hdmitx_module.h"

/************************common sysfs*************************/

static ssize_t hpd_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d",
		tx_comm->hpd_state);
	return pos;
}

static DEVICE_ATTR_RO(hpd_state);

/* rawedid attr */
static ssize_t _rawedid_show(struct device *dev,
			    struct device_attribute *attr, char *buf, int size)
{
	int pos = 0;
	int i;
	int num;
	int block_num = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	block_num = hdmitx_edid_valid_block_num(tx_comm->edid_buf);
	if (block_num <= 8)
		num = block_num * 128;
	else
		num = 8 * 128;

	for (i = 0; i < num; i++)
		pos += snprintf(buf + pos, size - pos, "%02x",
			tx_comm->edid_buf[i]);

	pos += snprintf(buf + pos, size - pos, "\n");
	return pos;
}

static ssize_t rawedid_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return _rawedid_show(dev, attr, buf, PAGE_SIZE);
}

static DEVICE_ATTR_RO(rawedid);

int hdmitx_edid_print_sink_cap(const struct rx_cap *prxcap,
		char *buffer, int buffer_len)
{
	int i, pos = 0;

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Manufacturer Name: %s\n", prxcap->IDManufacturerName);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Product Code: %02x%02x\n",
		prxcap->IDProductCode[0], prxcap->IDProductCode[1]);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Serial Number: %02x%02x%02x%02x\n",
		prxcap->IDSerialNumber[0],
		prxcap->IDSerialNumber[1],
		prxcap->IDSerialNumber[2],
		prxcap->IDSerialNumber[3]);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Rx Product Name: %s\n", prxcap->ReceiverProductName);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Manufacture Week: %d\n", prxcap->manufacture_week);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Manufacture Year: %d\n", prxcap->manufacture_year + 1990);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Physical size(mm): %d x %d\n",
		prxcap->physical_width, prxcap->physical_height);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"EDID Version: %d.%d\n",
		prxcap->edid_version, prxcap->edid_revision);

/*
 *	pos += snprintf(buffer + pos, buffer_len - pos,
 *		"EDID block number: 0x%x\n", tx_comm->edid_buf[0x7e]);
 */

	pos += snprintf(buffer + pos, buffer_len - pos,
		"Source Physical Address[a.b.c.d]: %x.%x.%x.%x\n",
		prxcap->vsdb_phy_addr.a,
		prxcap->vsdb_phy_addr.b,
		prxcap->vsdb_phy_addr.c,
		prxcap->vsdb_phy_addr.d);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"native Mode %x, VIC (native %d):\n",
		prxcap->native_Mode, prxcap->native_vic);

	pos += snprintf(buffer + pos, buffer_len - pos,
		"ColorDeepSupport %x\n", prxcap->ColorDeepSupport);

	for (i = 0; i < prxcap->VIC_count ; i++) {
		pos += snprintf(buffer + pos, buffer_len - pos, "%d ",
		prxcap->VIC[i]);
	}
	pos += snprintf(buffer + pos, buffer_len - pos, "\n");
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Audio {format, channel, freq, cce}\n");
	for (i = 0; i < prxcap->AUD_count; i++) {
		pos += snprintf(buffer + pos, buffer_len - pos,
			"{%d, %d, %x, %x}\n",
			prxcap->RxAudioCap[i].audio_format_code,
			prxcap->RxAudioCap[i].channel_num_max,
			prxcap->RxAudioCap[i].freq_cc,
			prxcap->RxAudioCap[i].cc3);
	}
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Speaker Allocation: %x\n", prxcap->RxSpeakerAllocation);
	pos += snprintf(buffer + pos, buffer_len - pos,
		"Vendor: 0x%x ( %s device)\n",
		prxcap->ieeeoui, (prxcap->ieeeoui) ? "HDMI" : "DVI");

	pos += snprintf(buffer + pos, buffer_len - pos,
		"MaxTMDSClock1 %d MHz\n", prxcap->Max_TMDS_Clock1 * 5);

	if (prxcap->hf_ieeeoui) {
		pos += snprintf(buffer + pos, buffer_len - pos, "Vendor2: 0x%x\n",
			prxcap->hf_ieeeoui);
		pos += snprintf(buffer + pos, buffer_len - pos, "MaxTMDSClock2 %d MHz\n",
			prxcap->Max_TMDS_Clock2 * 5);
	}

	pos += snprintf(buffer + pos, buffer_len - pos, "MaxFRLRate: %d\n",
		prxcap->max_frl_rate);

	if (prxcap->allm)
		pos += snprintf(buffer + pos, buffer_len - pos, "ALLM: %x\n",
				prxcap->allm);

	if (prxcap->cnc3)
		pos += snprintf(buffer + pos, buffer_len - pos, "Game/CNC3: %x\n",
				prxcap->cnc3);

	pos += snprintf(buffer + pos, buffer_len - pos, "vLatency: ");
	if (prxcap->vLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->vLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos,
			" %d\n", prxcap->vLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "aLatency: ");
	if (prxcap->aLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->aLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->aLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "i_vLatency: ");
	if (prxcap->i_vLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->i_vLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->i_vLatency);

	pos += snprintf(buffer + pos, buffer_len - pos, "i_aLatency: ");
	if (prxcap->i_aLatency == LATENCY_INVALID_UNKNOWN)
		pos += snprintf(buffer + pos, buffer_len - pos,
				" Invalid/Unknown\n");
	else if (prxcap->i_aLatency == LATENCY_NOT_SUPPORT)
		pos += snprintf(buffer + pos, buffer_len - pos,
			" UnSupported\n");
	else
		pos += snprintf(buffer + pos, buffer_len - pos, " %d\n",
			prxcap->i_aLatency);

	if (prxcap->video_capability_data) {
		pos += snprintf(buffer + pos, buffer_len - pos,
				"Video_Capability_Data: 0x%x\n", prxcap->video_capability_data);

		pos += snprintf(buffer + pos, buffer_len - pos, "PT over/underscan\n");
		switch (prxcap->video_capability_data & 0x30) {
		case PT_VIDEO_FORMAT_NO_DATA:
			pos += snprintf(buffer + pos, buffer_len - pos, " refer to S_CE or S_IT\n");
			break;
		case PT_VIDEO_FORMAT_ALWAYS_OVERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always overscan\n");
			break;
		case PT_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always underscan\n");
			break;
		case PT_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos,
					" both over-and underscan\n");
			break;
		}

		pos += snprintf(buffer + pos, buffer_len - pos, "IT over/underscan\n");
		switch (prxcap->video_capability_data & 0xC) {
		case IT_VIDEO_FORMAT_NOT_SUPPORT:
			pos += snprintf(buffer + pos, buffer_len - pos, " IT not support\n");
			break;
		case IT_VIDEO_FORMAT_ALWAYS_OVERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always overscan\n");
			break;
		case IT_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always underscan\n");
			break;
		case IT_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos,
					" both over-and underscan\n");
			break;
		}

		pos += snprintf(buffer + pos, buffer_len - pos, "CT over/underscan\n");
		switch (prxcap->video_capability_data & 0x3) {
		case CE_VIDEO_FORMAT_NOT_SUPPORT:
			pos += snprintf(buffer + pos, buffer_len - pos, " CE not support\n");
			break;
		case CE_VIDEO_FORMAT_ALWAYS_OVERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always overscan\n");
			break;
		case CE_VIDEO_FORMAT_ALWAYS_UNDERSCANSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos, " always underscan\n");
			break;
		case CE_VIDEO_FORMAT_BOTH_OVER_UNDERSCAN:
			pos += snprintf(buffer + pos, buffer_len - pos,
					" both over-and underscan\n");
			break;
		}
	}
	if (prxcap->colorimetry_data)
		pos += snprintf(buffer + pos, buffer_len - pos,
			"ColorMetry: 0x%x\n", prxcap->colorimetry_data);

	pos += snprintf(buffer + pos, buffer_len - pos, "SCDC: %x\n",
		prxcap->scdc_present);

	pos += snprintf(buffer + pos, buffer_len - pos, "RR_Cap: %x\n",
		prxcap->scdc_rr_capable);
	pos += snprintf(buffer + pos, buffer_len - pos, "LTE_340M_Scramble: %x\n",
		prxcap->lte_340mcsc_scramble);

	/* dsc capability */
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_10bpc: %d\n",
		 prxcap->dsc_10bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_12bpc: %d\n",
		 prxcap->dsc_12bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_16bpc: %d\n",
		 prxcap->dsc_16bpc);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_all_bpp: %d\n",
		 prxcap->dsc_all_bpp);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_native_420: %d\n",
		 prxcap->dsc_native_420);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_1p2: %d\n",
		 prxcap->dsc_1p2);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_max_slices: 0x%x(%d slices)\n",
		 prxcap->dsc_max_slices, dsc_max_slices_num[prxcap->dsc_max_slices]);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_max_frl_rate: 0x%x\n",
		 prxcap->dsc_max_frl_rate);
	pos += snprintf(buffer + pos, buffer_len - pos, "dsc_total_chunk_bytes: 0x%x\n",
		 prxcap->dsc_total_chunk_bytes);

	if (prxcap->dv_info.ieeeoui == DOVI_IEEEOUI)
		pos += snprintf(buffer + pos, buffer_len - pos,
			"  DolbyVision%d", prxcap->dv_info.ver);

	if (prxcap->hdr_info.hdr_support)
		pos += snprintf(buffer + pos, buffer_len - pos, "  HDR/%d",
			prxcap->hdr_info.hdr_support);
	if (prxcap->hdr_info.sbtm_info.sbtm_support)
		pos += snprintf(buffer + pos, buffer_len - pos, "  SBTM");
	if (prxcap->dc_y444 || prxcap->dc_30bit || prxcap->dc_30bit_420)
		pos += snprintf(buffer + pos, buffer_len - pos, "  DeepColor");
	pos += snprintf(buffer + pos, buffer_len - pos, "\n");
	pos += snprintf(buffer + pos, buffer_len - pos, "additional_vsif_num: %d\n",
			prxcap->additional_vsif_num);
	pos += snprintf(buffer + pos, buffer_len - pos, "ifdb_present: %d\n",
			prxcap->ifdb_present);
	/*
	 * for checkvalue which maybe used by application to adjust
	 * whether edid is changed
	 */
	pos += snprintf(buffer + pos, buffer_len - pos,
			"checkvalue: %s\n", prxcap->hdmichecksum);

	return pos;
}

static ssize_t edid_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	return hdmitx_edid_print_sink_cap(&tx_comm->rxcap, buf, PAGE_SIZE);
}

static int load_edid_string_data(struct hdmitx_common *tx_comm, char *string)
{
	size_t str_len;
	int i;
	bool valid_len;
	unsigned char *buf = NULL;
	bool ret;
	size_t edid_len;
	unsigned char tmp[3];

	if (!string)
		return 0;

	str_len = strlen(string);
	valid_len = 0;
	for (i = 1; i <= EDID_MAX_BLOCK; i++) {
		if (str_len == (256 * i + 1)) {
			valid_len = 1;
			break;
		}
	}
	if (valid_len == 0)
		return 0;

	edid_len = (str_len - 1) / 2;
	buf = kmalloc(edid_len, GFP_KERNEL);
	if (!buf)
		return 0;
	memset(buf, 0, edid_len);
	/* convert the edid string to hex data */
	for (i = 0; i < edid_len; i++) {
		tmp[0] = string[i * 2];
		tmp[1] = string[i * 2 + 1];
		tmp[2] = '\0';
		ret = kstrtou8(tmp, 16, &buf[i]);
		if (ret)
			HDMITX_INFO("%s[%d] covert error %c%c ret = %d\n", __func__, __LINE__,
				string[i * 2], string[i * 2 + 1], ret);
	}

	hdmitx_edid_buffer_clear(tx_comm->edid_buf, sizeof(tx_comm->edid_buf));
	memcpy(tx_comm->edid_buf, buf, edid_len);

	kfree(buf);
	HDMITX_INFO("%s: %zu bytes loaded from edid string\n", __func__, str_len);
	return 1;
}

static int hdmitx_load_edid_file(struct hdmitx_common *tx_comm, u32 type, char *path)
{
	if (type == 1)
		return load_edid_string_data(tx_comm, path);
	return 0;
}

static int hdmitx_save_edid_file(unsigned char *rawedid, char *path)
{
#ifdef CONFIG_AMLOGIC_ENABLE_MEDIA_FILE
	struct file *filp = NULL;
	loff_t pos = 0;
	char line[128] = {0};
	u32 i = 0, j = 0, k = 0, size = 0, block_cnt = 0;
	u32 index = 0, tmp = 0;

	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(filp)) {
		HDMITX_ERROR("[%s] failed to open/create file: |%s|\n",
			__func__, path);
		goto PROCESS_END;
	}

	block_cnt = rawedid[0x7e] + 1;
	if (rawedid[0x7e] && rawedid[128 + 4] == EXTENSION_EEODB_EXT_TAG &&
		rawedid[128 + 5] == EXTENSION_EEODB_EXT_CODE)
		block_cnt = rawedid[128 + 6] + 1;

	/* dump as txt file*/
	for (i = 0; i < block_cnt; i++) {
		for (j = 0; j < 8; j++) {
			for (k = 0; k < 16; k++) {
				index = i * 128 + j * 16 + k;
				tmp = rawedid[index];
				snprintf((char *)&line[k * 6], 7,
					 "0x%02x, ",
					 tmp);
			}
			line[16 * 6 - 1] = '\n';
			line[16 * 6] = 0x0;
			pos = (i * 8 + j) * 16 * 6;
		}
	}

	HDMITX_INFO("[%s] write %d bytes to file %s\n", __func__, size, path);

	vfs_fsync(filp, 0);
	filp_close(filp, NULL);

PROCESS_END:
#else
	HDMITX_ERROR("Not support write file.\n");
#endif
	return 0;
}

static ssize_t edid_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	u32 argn = 0;
	char *p = NULL, *para = NULL, *temp_p = NULL, *argv[8] = {NULL};
	u32 path_length = 0;
	int ret = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	p = kstrdup(buf, GFP_KERNEL);
	temp_p = p;
	if (!p)
		return count;

	do {
		para = strsep(&p, " ");
		if (para) {
			argv[argn] = para;
			argn++;
			if (argn > 7)
				break;
		}
	} while (para);

	if (!strncmp(argv[0], "save", strlen("save"))) {
		u32 type = 0;

		if (argn != 3) {
			HDMITX_INFO("[%s] cmd format: save bin/txt edid_file_path\n",
				__func__);
			goto PROCESS_END;
		}
		if (!strncmp(argv[1], "bin", strlen("bin")))
			type = 1;
		else if (!strncmp(argv[1], "txt", strlen("txt")))
			type = 2;

		if (type == 1 || type == 2) {
			/* clean '\n' from file path*/
			path_length = strlen(argv[2]);
			if (argv[2][path_length - 1] == '\n')
				argv[2][path_length - 1] = 0x0;

			hdmitx_save_edid_file(tx_comm->edid_buf, argv[2]);
		}
	} else if (!strncmp(argv[0], "load", strlen("load"))) {
		if (argn != 2) {
			HDMITX_INFO("[%s] cmd format: load edid_file_path\n",
				__func__);
			goto PROCESS_END;
		}

		/* get the EDID from current RX device */
		if (strncmp(argv[1], "0000000000000000", 16) == 0) {
			HDMITX_INFO("%s[%d] get current RX edid\n", __func__, __LINE__);
			tx_comm->forced_edid = 0;
			/* read EDID with DDC */
			hdmitx_common_get_edid(tx_comm);
			/* parse whole edid */
			hdmitx_edid_process(tx_comm, true, false);
			goto PROCESS_END;
		}

		/* clean '\n' from file path*/
		path_length = strlen(argv[1]);
		/* edid data as string for debug */
		ret = hdmitx_load_edid_file(tx_comm, 1, argv[1]);
		if (ret == 1) {
			tx_comm->forced_edid = 1;
			/* parse whole edid */
			hdmitx_edid_process(tx_comm, true, false);
			hdmitx_edid_print(tx_comm->edid_buf);
			HDMITX_INFO("%s[%d] using the fixed edid\n", __func__, __LINE__);
		}
	}

PROCESS_END:
	kfree(temp_p);
	return count;
}
static DEVICE_ATTR_RW(edid);

static ssize_t _hdr_cap_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf,
			     const struct hdr_info *hdr, int size)
{
	int pos = 0;
	unsigned int i, j;
	int hdr10plus_supported = 0;
	const struct cuva_info *cuva = &hdr->cuva_info;
	const struct hdr10_plus_info *hdr10p = &hdr->hdr10plus_info;
	const struct sbtm_info *sbtm = &hdr->sbtm_info;

	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
			hdr10p->application_version != 0xFF)
		hdr10plus_supported = 1;
	pos += snprintf(buf + pos, size - pos, "HDR10Plus Supported: %d\n",
		hdr10plus_supported);
	pos += snprintf(buf + pos, size - pos, "HDR Static Metadata:\n");
	pos += snprintf(buf + pos, size - pos, "    Supported EOTF:\n");
	pos += snprintf(buf + pos, size - pos, "        Traditional SDR: %d\n",
		!!(hdr->hdr_support & 0x1));
	pos += snprintf(buf + pos, size - pos, "        Traditional HDR: %d\n",
		!!(hdr->hdr_support & 0x2));
	pos += snprintf(buf + pos, size - pos, "        SMPTE ST 2084: %d\n",
		!!(hdr->hdr_support & 0x4));
	pos += snprintf(buf + pos, size - pos, "        Hybrid Log-Gamma: %d\n",
		!!(hdr->hdr_support & 0x8));
	pos += snprintf(buf + pos, size - pos, "    Supported SMD type1: %d\n",
		hdr->static_metadata_type1);
	pos += snprintf(buf + pos, size - pos, "    Luminance Data\n");
	pos += snprintf(buf + pos, size - pos, "        Max: %d\n",
		hdr->lumi_max);
	pos += snprintf(buf + pos, size - pos, "        Avg: %d\n",
		hdr->lumi_avg);
	pos += snprintf(buf + pos, size - pos, "        Min: %d\n\n",
		hdr->lumi_min);
	pos += snprintf(buf + pos, size - pos, "HDR Dynamic Metadata:");

	for (i = 0; i < 4; i++) {
		if (hdr->dynamic_info[i].type == 0)
			continue;
		pos += snprintf(buf + pos, size - pos,
			"\n    metadata_version: %x\n",
			hdr->dynamic_info[i].type);
		pos += snprintf(buf + pos, size - pos,
			"        support_flags: %x\n",
			hdr->dynamic_info[i].support_flags);
		pos += snprintf(buf + pos, size - pos,
			"        optional_fields:");
		if (hdr->dynamic_info[i].of_len >= 3)
			for (j = 0; j < (hdr->dynamic_info[i].of_len - 3); j++)
				pos += snprintf(buf + pos, size - pos, " %x",
					hdr->dynamic_info[i].optional_fields[j]);
	}

	pos += snprintf(buf + pos, size - pos, "\n\ncolorimetry_data: %x\n",
		hdr->colorimetry_support);
	if (cuva->ieeeoui == CUVA_IEEEOUI) {
		pos += snprintf(buf + pos, size - pos, "CUVA supported: 1\n");
		pos += snprintf(buf + pos, size - pos,
			"  system_start_code: %u\n", cuva->system_start_code);
		pos += snprintf(buf + pos, size - pos,
			"  version_code: %u\n", cuva->version_code);
		pos += snprintf(buf + pos, size - pos,
			"  display_maximum_luminance: %u\n",
			cuva->display_max_lum);
		pos += snprintf(buf + pos, size - pos,
			"  display_minimum_luminance: %u\n",
			cuva->display_min_lum);
		pos += snprintf(buf + pos, size - pos,
			"  monitor_mode_support: %u\n", cuva->monitor_mode_sup);
		pos += snprintf(buf + pos, size - pos,
			"  rx_mode_support: %u\n", cuva->rx_mode_sup);
		for (i = 0; i < (cuva->length + 1); i++)
			pos += snprintf(buf + pos, size - pos, "%02x",
				cuva->rawdata[i]);
		pos += snprintf(buf + pos, size - pos, "\n");
	}
	/* sbtm capability show */
	if (sbtm->sbtm_support) {
		pos += snprintf(buf + pos, size - pos, "SBTM supported: 1\n");
		if (sbtm->max_sbtm_ver)
			pos += snprintf(buf + pos, size - pos, "  Max_SBTM_Ver: 0x%x\n",
				sbtm->max_sbtm_ver);
		if (sbtm->grdm_support)
			pos += snprintf(buf + pos, size - pos, "  grdm_support: 0x%x\n",
				sbtm->grdm_support);
		if (sbtm->drdm_ind)
			pos += snprintf(buf + pos, size - pos, "  drdm_ind: 0x%x\n",
				sbtm->drdm_ind);
		if (sbtm->hgig_cat_drdm_sel)
			pos += snprintf(buf + pos, size - pos, "  hgig_cat_drdm_sel: 0x%x\n",
				sbtm->hgig_cat_drdm_sel);
		if (sbtm->use_hgig_drdm)
			pos += snprintf(buf + pos, size - pos, "  use_hgig_drdm: 0x%x\n",
				sbtm->use_hgig_drdm);
		if (sbtm->maxrgb)
			pos += snprintf(buf + pos, size - pos, "  maxrgb: 0x%x\n",
				sbtm->maxrgb);
		if (sbtm->gamut)
			pos += snprintf(buf + pos, size - pos, "  gamut: 0x%x\n",
				sbtm->gamut);
		if (sbtm->red_x)
			pos += snprintf(buf + pos, size - pos, "  red_x: 0x%x\n",
				sbtm->red_x);
		if (sbtm->red_y)
			pos += snprintf(buf + pos, size - pos, "  red_y: 0x%x\n",
				sbtm->red_y);
		if (sbtm->green_x)
			pos += snprintf(buf + pos, size - pos, "  green_x: 0x%x\n",
				sbtm->green_x);
		if (sbtm->green_y)
			pos += snprintf(buf + pos, size - pos, "  green_y: 0x%x\n",
				sbtm->green_y);
		if (sbtm->blue_x)
			pos += snprintf(buf + pos, size - pos, "  blue_x: 0x%x\n",
				sbtm->blue_x);
		if (sbtm->blue_y)
			pos += snprintf(buf + pos, size - pos, "  blue_y: 0x%x\n",
				sbtm->blue_y);
		if (sbtm->white_x)
			pos += snprintf(buf + pos, size - pos, "  white_x: 0x%x\n",
				sbtm->white_x);
		if (sbtm->white_y)
			pos += snprintf(buf + pos, size - pos, "  white_y: 0x%x\n",
				sbtm->white_y);
		if (sbtm->min_bright_10)
			pos += snprintf(buf + pos, size - pos, "  min_bright_10: 0x%x\n",
				sbtm->min_bright_10);
		if (sbtm->peak_bright_100)
			pos += snprintf(buf + pos, size - pos, "  peak_bright_100: 0x%x\n",
				sbtm->peak_bright_100);
		if (sbtm->p0_exp)
			pos += snprintf(buf + pos, size - pos, "  p0_exp: 0x%x\n",
				sbtm->p0_exp);
		if (sbtm->p0_mant)
			pos += snprintf(buf + pos, size - pos, "  p0_mant: 0x%x\n",
				sbtm->p0_mant);
		if (sbtm->peak_bright_p0)
			pos += snprintf(buf + pos, size - pos, "  peak_bright_p0: 0x%x\n",
				sbtm->peak_bright_p0);
		if (sbtm->p1_exp)
			pos += snprintf(buf + pos, size - pos, "  p1_exp: 0x%x\n",
				sbtm->p1_exp);
		if (sbtm->p1_mant)
			pos += snprintf(buf + pos, size - pos, "  p1_mant: 0x%x\n",
				sbtm->p1_mant);
		if (sbtm->peak_bright_p1)
			pos += snprintf(buf + pos, size - pos, "  peak_bright_p1: 0x%x\n",
				sbtm->peak_bright_p1);
		if (sbtm->p2_exp)
			pos += snprintf(buf + pos, size - pos, "  p2_exp: 0x%x\n",
				sbtm->p2_exp);
		if (sbtm->p2_mant)
			pos += snprintf(buf + pos, size - pos, "  p2_mant: 0x%x\n",
				sbtm->p2_mant);
		if (sbtm->peak_bright_p2)
			pos += snprintf(buf + pos, size - pos, "  peak_bright_p2: 0x%x\n",
				sbtm->peak_bright_p2);
		if (sbtm->p3_exp)
			pos += snprintf(buf + pos, size - pos, "  p3_exp: 0x%x\n",
				sbtm->p3_exp);
		if (sbtm->p3_mant)
			pos += snprintf(buf + pos, size - pos, "  p3_mant: 0x%x\n",
				sbtm->p3_mant);
		if (sbtm->peak_bright_p3)
			pos += snprintf(buf + pos, size - pos, "  peak_bright_p3: 0x%x\n",
				sbtm->peak_bright_p3);
	}
	return pos;
}

static ssize_t hdr_cap_rx_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	const struct hdr_info *info = &tx_comm->rxcap.hdr_info;

	return _hdr_cap_show(dev, attr, buf, info, PAGE_SIZE);
}

static DEVICE_ATTR_RO(hdr_cap_rx);

static ssize_t _show_dv_cap(struct device *dev,
			    struct device_attribute *attr,
			    char *buf,
			    const struct dv_info *dv, int size)
{
	int pos = 0;
	int i;

	if (dv->ieeeoui != DV_IEEE_OUI || dv->block_flag != CORRECT) {
		pos += snprintf(buf + pos, size - pos,
			"The Rx don't support DolbyVision\n");
		return pos;
	}
	pos += snprintf(buf + pos, size - pos,
		"DolbyVision RX support list:\n");

	if (dv->ver == 0) {
		pos += snprintf(buf + pos, size - pos,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, size - pos,
			"2160p%shz: 1\n",
			dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, size - pos,
			"Support mode:\n");
		pos += snprintf(buf + pos, size - pos,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, size - pos,
				"  DV_YCbCr_422_12BIT\n");
	}
	if (dv->ver == 1) {
		pos += snprintf(buf + pos, size - pos,
			"VSVDB Version: V%d(%d-byte)\n",
			dv->ver, dv->length + 1);
		if (dv->length == 0xB) {
			pos += snprintf(buf + pos, size - pos,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, size - pos,
			"Support mode:\n");
		pos += snprintf(buf + pos, size - pos,
			"  DV_RGB_444_8BIT\n");
		if (dv->sup_yuv422_12bit)
			pos += snprintf(buf + pos, size - pos,
			"  DV_YCbCr_422_12BIT\n");
		if (dv->low_latency == 0x01)
			pos += snprintf(buf + pos, size - pos,
				"  LL_YCbCr_422_12BIT\n");
		}

		if (dv->length == 0xE) {
			pos += snprintf(buf + pos, size - pos,
				"2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
			pos += snprintf(buf + pos, size - pos,
				"Support mode:\n");
			pos += snprintf(buf + pos, size - pos,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, size - pos,
				"  DV_YCbCr_422_12BIT\n");
		}
	}
	if (dv->ver == 2) {
		pos += snprintf(buf + pos, size - pos,
			"VSVDB Version: V%d\n", dv->ver);
		pos += snprintf(buf + pos, size - pos,
			"2160p%shz: 1\n",
			dv->sup_2160p60hz ? "60" : "30");
		pos += snprintf(buf + pos, size - pos,
			"Parity: %d\n", dv->parity);
		pos += snprintf(buf + pos, size - pos,
			"Support mode:\n");
		if (dv->Interface != 0x00 && dv->Interface != 0x01) {
			pos += snprintf(buf + pos, size - pos,
				"  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				pos += snprintf(buf + pos, size - pos,
					"  DV_YCbCr_422_12BIT\n");
		}
		pos += snprintf(buf + pos, size - pos,
			"  LL_YCbCr_422_12BIT\n");
		if (dv->Interface == 0x01 || dv->Interface == 0x03) {
			if (dv->sup_10b_12b_444 == 0x1) {
				pos += snprintf(buf + pos, size - pos,
					"  LL_RGB_444_10BIT\n");
			}
			if (dv->sup_10b_12b_444 == 0x2) {
				pos += snprintf(buf + pos, size - pos,
					"  LL_RGB_444_12BIT\n");
			}
		}
	}
	pos += snprintf(buf + pos, size - pos,
		"IEEEOUI: 0x%06x\n", dv->ieeeoui);
	pos += snprintf(buf + pos, size - pos,
		"EMP: %d\n", dv->dv_emp_cap);
	pos += snprintf(buf + pos, size - pos, "VSVDB: ");
	for (i = 0; i < (dv->length + 1); i++)
		pos += snprintf(buf + pos, size - pos, "%02x",
		dv->rawdata[i]);
	pos += snprintf(buf + pos, size - pos, "\n");
	return pos;
}

static ssize_t dv_cap_rx_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	const struct dv_info *dv = &tx_comm->rxcap.dv_info;

	return _show_dv_cap(dev, attr, buf, dv, PAGE_SIZE);
}

static DEVICE_ATTR_RO(dv_cap_rx);

/*
 *  1: enable hdmitx phy
 *  0: disable hdmitx phy
 */
static ssize_t phy_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int mute_us;
	int cnt = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	u32 arg = 0;
	/*
	 * special WHALEY WTV55K1J TV, need to wait for > 3 frames
	 * after phy disable and before set new mode
	 */
	int delay_frame = 5;

	tx_hw->tmds_phy_op = TMDS_PHY_NONE;

	HDMITX_INFO("%s %s\n", __func__, buf);
	mute_us = hdmitx_get_frame_duration(tx_comm);
	if (strncmp(buf, "0", 1) == 0) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		/* for s5 frl mode */
		hdmitx_hw_cntl(tx_hw, FRL_DISABLE_WORK, NULL, NULL);
#endif
		tx_hw->tmds_phy_op = TMDS_PHY_DISABLE;
		/*
		 * It is necessary to finish disable phy during the vsync interrupt
		 * before performing other actions. If the vsync interrupt does not come,
		 * there is a 3-frame timeout mechanism.
		 */
		while (tx_hw->tmds_phy_op) {
			usleep_range(mute_us, mute_us + 10);
			cnt++;
			if (cnt > 3) {
				HDMITX_INFO("not have vsync intr, manually turn off phy\n");
				arg = TMDS_PHY_DISABLE;
				hdmitx_hw_cntl(tx_hw, PLATFORM_PHY_OP, (void *)&arg, NULL);
				tx_hw->tmds_phy_op = TMDS_PHY_NONE;
				break;
			}
		}
		if (hdmitx_find_vendor_phy_delay(tx_comm->edid_buf)) {
			usleep_range(delay_frame * mute_us, delay_frame * mute_us + 10);
			HDMITX_DEBUG("delay %d frame after phy disable\n", delay_frame);
		}
	} else if (strncmp(buf, "1", 1) == 0) {
		tx_hw->tmds_phy_op = TMDS_PHY_ENABLE;
		arg = TMDS_PHY_ENABLE;
		hdmitx_hw_cntl(tx_hw, PLATFORM_PHY_OP, (void *)&arg, NULL);
	} else {
		HDMITX_INFO("set phy wrong: %s\n", buf);
	}
	return count;
}

static ssize_t phy_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int state = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	state = hdmitx_hw_cntl(tx_hw, PLATFORM_GET_PHY_STAT, NULL, NULL);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", state);

	return pos;
}

static DEVICE_ATTR_RW(phy);

static ssize_t _contenttype_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	static char * const ct_names[] = {
		"off",
		"game",
		"graphics",
		"photo",
		"cinema",
	};

	if (tx_comm->ct_mode < ARRAY_SIZE(ct_names))
		pos += snprintf(buf + pos, PAGE_SIZE, "%s\n\r",
					ct_names[tx_comm->ct_mode]);

	return pos;
}

static inline int com_str(const char *buf, const char *str)
{
	return strncmp(buf, str, strlen(str)) == 0;
}

static ssize_t _contenttype_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u32 ct_mode = SET_CT_OFF;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	u32 arg = 0;

	HDMITX_INFO("store contenttype_mode as %s\n", buf);

	if (tx_comm->allm_mode == 1) {
		tx_comm->allm_mode = 0;
		hdmitx_common_setup_vsif_packet(tx_comm, VT_ALLM, 0, NULL);
	}
	/* recover hdmi1.4 vsif */
	if (hdmitx_edid_get_hdmi14_4k_vic(tx_comm->fmt_para.vic) &&
		!hdmitx_dv_en(tx_comm->tx_hw) &&
		!hdmitx_hdr10p_en(tx_comm->tx_hw))
		hdmitx_common_setup_vsif_packet(tx_comm, VT_HDMI14_4K, 1, NULL);

	if (com_str(buf, "0") || com_str(buf, "off")) {
		ct_mode = SET_CT_OFF;
		tx_comm->it_content = 0;
	} else if (com_str(buf, "1") || com_str(buf, "game")) {
		ct_mode = SET_CT_GAME;
		tx_comm->it_content = 1;
	} else if (com_str(buf, "2") || com_str(buf, "graphics")) {
		ct_mode = SET_CT_GRAPHICS;
		tx_comm->it_content = 1;
	} else if (com_str(buf, "3") || com_str(buf, "photo")) {
		ct_mode = SET_CT_PHOTO;
		tx_comm->it_content = 1;
	} else if (com_str(buf, "4") || com_str(buf, "cinema")) {
		ct_mode = SET_CT_CINEMA;
		tx_comm->it_content = 1;
	}
	arg = tx_comm->it_content << 4 | ct_mode;
	hdmitx_hw_cntl(tx_hw, AUX_PKT_CONF_AVI_CT, (void *)&arg, NULL);
	tx_comm->ct_mode = ct_mode;

	return count;
}

static ssize_t _allm_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r",
		tx_comm->allm_mode);

	return pos;
}

static ssize_t _llm_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct rx_cap *prxcap = &tx_comm->rxcap;

	if (prxcap->allm)
		return _allm_mode_show(dev, attr, buf);
	if (prxcap->cnc3)
		return _contenttype_mode_show(dev, attr, buf);
	return pos;
}

static ssize_t _allm_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	int mode = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	HDMITX_INFO("store allm_mode as %s\n", buf);

	if (com_str(buf, "0"))
		mode = 0;
	else if (com_str(buf, "1"))
		mode = 1;
	else if (com_str(buf, "-1"))
		mode = -1;

	hdmitx_common_set_allm_mode(tx_comm, mode);

	return count;
}

static ssize_t _llm_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct rx_cap *prxcap = &tx_comm->rxcap;

	if (prxcap->allm)
		return _allm_mode_store(dev, attr, buf, count);
	if (prxcap->cnc3)
		return _contenttype_mode_store(dev, attr, buf, count);
	return count;
}

static ssize_t allm_mode_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	return _llm_mode_show(dev, attr, buf);
}

static ssize_t allm_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf,
			       size_t count)
{
	return _llm_mode_store(dev, attr, buf, count);
}

static DEVICE_ATTR_RW(allm_mode);

static ssize_t vesa_cap_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	int i;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	enum hdmi_vic *vesa_t = &tx_comm->rxcap.vesa_timing[0];
	int pos = 0;

	for (i = 0; i < VESA_MAX_TIMING && vesa_t[i]; i++) {
		const struct hdmi_timing *timing = hdmitx_mode_vic_to_hdmi_timing(vesa_t[i]);

		if (timing && timing->vic >= HDMITX_VESA_OFFSET)
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "%s\n",
					timing->name);
	}
	return pos;
}

static DEVICE_ATTR_RO(vesa_cap);

static void _show_pcm_ch(struct rx_cap *prxcap, int i,
			 int *ppos, char *buf)
{
	const char * const aud_sample_size[] = {"ReferToStreamHeader",
		"16", "20", "24", NULL};
	int j = 0;

	for (j = 0; j < 3; j++) {
		if (prxcap->RxAudioCap[i].cc3 & (1 << j))
			*ppos += snprintf(buf + *ppos, PAGE_SIZE, "%s/",
				aud_sample_size[j + 1]);
	}
	*ppos += snprintf(buf + *ppos - 1, PAGE_SIZE, " bit\n") - 1;
}

ssize_t _show_aud_cap(struct rx_cap *prxcap, char *buf, int size)
{
	int i, pos = 0, j;
	struct dolby_vsadb_cap *cap = &prxcap->dolby_vsadb_cap;

	static const char * const aud_ct[] =  {
		"ReferToStreamHeader", "PCM", "AC-3", "MPEG1", "MP3",
		"MPEG2", "AAC", "DTS", "ATRAC",	"OneBitAudio",
		"Dolby_Digital+", "DTS-HD", "MAT", "DST", "WMA_Pro",
		"Reserved", NULL};
	static const char * const aud_sampling_frequency[] = {
		"ReferToStreamHeader", "32", "44.1", "48", "88.2", "96",
		"176.4", "192", NULL};

	pos += snprintf(buf + pos, size - pos,
		"CodingType MaxChannels SamplingFreq SampleSize\n");
	for (i = 0; i < prxcap->AUD_count; i++) {
		if (prxcap->RxAudioCap[i].audio_format_code == CT_CXT) {
			if ((prxcap->RxAudioCap[i].cc3 >> 3) == 0xb) {
				pos += snprintf(buf + pos, size - pos, "MPEG-H, 8ch, ");
				for (j = 0; j < 7; j++) {
					if (prxcap->RxAudioCap[i].freq_cc & (1 << j))
						pos += snprintf(buf + pos, size - pos, "%s/",
							aud_sampling_frequency[j + 1]);
				}
				pos += snprintf(buf + pos - 1, size - pos, " kHz\n");
			}
			continue;
		}
		pos += snprintf(buf + pos, size - pos, "%s",
			aud_ct[prxcap->RxAudioCap[i].audio_format_code]);
		if (prxcap->RxAudioCap[i].audio_format_code == CT_DD_P &&
		    (prxcap->RxAudioCap[i].cc3 & 1))
			pos += snprintf(buf + pos, size - pos, "/ATMOS");
		if (prxcap->RxAudioCap[i].audio_format_code != CT_CXT)
			pos += snprintf(buf + pos, size - pos, ", %d ch, ",
				prxcap->RxAudioCap[i].channel_num_max + 1);
		for (j = 0; j < 7; j++) {
			if (prxcap->RxAudioCap[i].freq_cc & (1 << j))
				pos += snprintf(buf + pos, size - pos, "%s/",
					aud_sampling_frequency[j + 1]);
		}
		pos += snprintf(buf + pos - 1, size - pos, " kHz, ") - 1;
		switch (prxcap->RxAudioCap[i].audio_format_code) {
		case CT_PCM:
			_show_pcm_ch(prxcap, i, &pos, buf);
			break;
		case CT_AC_3:
		case CT_MPEG1:
		case CT_MP3:
		case CT_MPEG2:
		case CT_AAC:
		case CT_DTS:
		case CT_ATRAC:
		case CT_ONE_BIT_AUDIO:
			pos += snprintf(buf + pos, size - pos,
				"MaxBitRate %dkHz\n",
				prxcap->RxAudioCap[i].cc3 * 8);
			break;
		case CT_DD_P:
		case CT_DTS_HD:
		case CT_MAT:
		case CT_DST:
			pos += snprintf(buf + pos, size - pos, "DepValue 0x%x\n",
				prxcap->RxAudioCap[i].cc3);
			break;
		case CT_WMA:
		default:
			break;
		}
	}

	if (cap->ieeeoui == DOVI_IEEEOUI) {
		/*
		 * Dolby Vendor Specific:
		 *  headphone_playback_only:0,
		 *  center_speaker:1,
		 *  surround_speaker:1,
		 *  height_speaker:1,
		 *  Ver:1.0,
		 *  MAT_PCM_48kHz_only:1,
		 *  e61146d0007001,
		 */
		pos += snprintf(buf + pos, size - pos,
				"Dolby Vendor Specific:\n");
		if (cap->dolby_vsadb_ver == 0)
			pos += snprintf(buf + pos, size - pos, "  Ver:1.0,\n");
		else
			pos += snprintf(buf + pos, size - pos,
				"  Ver:Reversed,\n");
		pos += snprintf(buf + pos, size - pos,
			"  center_speaker:%d,\n", cap->spk_center);
		pos += snprintf(buf + pos, size - pos,
			"  surround_speaker:%d,\n", cap->spk_surround);
		pos += snprintf(buf + pos, size - pos,
			"  height_speaker:%d,\n", cap->spk_height);
		pos += snprintf(buf + pos, size - pos,
			"  headphone_playback_only:%d,\n", cap->headphone_only);
		pos += snprintf(buf + pos, size - pos,
			"  MAT_PCM_48kHz_only:%d,\n", cap->mat_48k_pcm_only);

		pos += snprintf(buf + pos, size - pos, "  ");
		for (i = 0; i < 7; i++)
			pos += snprintf(buf + pos, size - pos, "%02x",
				cap->rawdata[i]);
		pos += snprintf(buf + pos, size - pos, ",\n");
	}
	return pos;
}

static ssize_t aud_cap_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct rx_cap *prxcap = &tx_comm->rxcap;

	return _show_aud_cap(prxcap, buf, PAGE_SIZE);
}

static DEVICE_ATTR_RO(aud_cap);

static ssize_t preferred_mode_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct rx_cap *prxcap = &tx_comm->rxcap;
	const char *modename =
		hdmitx_mode_get_timing_name(prxcap->preferred_mode);

	pos += snprintf(buf + pos, PAGE_SIZE, "%s\n", modename);

	return pos;
}

static DEVICE_ATTR_RO(preferred_mode);

static bool pre_process_str(const char *name, char *mode, char *attr)
{
	int i;
	const char *color_format[4] = {"444", "422", "420", "rgb"};
	char *search_pos = 0;

	if (!mode || !attr)
		return false;

	for (i = 0 ; i < 4 ; i++) {
		search_pos = strstr(name, color_format[i]);
		if (search_pos)
			break;
	}
	/* no cs parsed, return error */
	if (!search_pos)
		return false;

	/* search remaining color_formats, if have more than one cs string, return error */
	i++;
	for (; i < 4 ; i++) {
		if (strstr(search_pos, color_format[i]))
			return false;
	}

	/*copy mode name;*/
	memcpy(mode, name, search_pos - name);
	/*copy attr str;*/
	memcpy(attr, search_pos, strlen(search_pos));

	//HDMITX_INFO("%s parse (%s,%s) from (%s)\n", __func__, mode, attr, name);

	return true;
}

/* validation step:
 * step1, check if mode related VIC is supported in EDID
 * step2, check if VIC is supported by SOC hdmitx
 * step3, build format with mode/attr and check if it's
 * supported by EDID/hdmitx_cap
 */
static ssize_t valid_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	bool valid_mode = false;
	char cvalid_mode[32];
	char modename[32], attrstr[32];
	struct hdmi_format_para tst_para;
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	char conv_name[32] = {0};
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	mutex_lock(&tx_comm->valid_mutex);
	memset(modename, 0, sizeof(modename));
	memset(attrstr, 0, sizeof(attrstr));
	memset(cvalid_mode, 0, sizeof(cvalid_mode));

	strncpy(cvalid_mode, buf, sizeof(cvalid_mode));
	cvalid_mode[31] = '\0';
	if (cvalid_mode[0])
		valid_mode = pre_process_str(cvalid_mode, modename, attrstr);

	/*
	 * hwc and system control use the valid mode node
	 * will use the fraction mode name check
	 * and need transfer to int and go on
	 */
	if (tx_comm->frac_enable && is_mode_name_frac(modename)) {
		convert_name_frac2int(modename, conv_name);
		strncpy(modename, conv_name, sizeof(conv_name));
	}

	if (valid_mode) {
		vic = hdmitx_common_parse_vic_in_edid(tx_comm, modename);
		if (vic == HDMI_0_UNKNOWN) {
			HDMITX_DEBUG("parse vic fail or vic not in EDID %s\n", modename);
			valid_mode = false;
		} else {
			ret = hdmitx_common_validate_vic(tx_comm, vic);
			if (ret != 0) {
				HDMITX_DEBUG("vic %d not supported by hdmitx,ret: %d\n", vic, ret);
				valid_mode = false;
			}
		}
	}

	if (valid_mode) {
		hdmitx_parse_color_attr(attrstr, &tst_para.cs, &tst_para.cd, &tst_para.cr);
		HDMITX_DEBUG("parse cs %d cd %d\n", tst_para.cs, tst_para.cd);
		ret = hdmitx_common_build_format_para(tx_comm,
			&tst_para, vic, 0,
			tst_para.cs, tst_para.cd, tst_para.cr);
		if (ret != 0) {
			HDMITX_DEBUG("build format para failed %d\n", ret);
			hdmitx_format_para_reset(&tst_para);
			valid_mode = false;
		}
	}

	if (valid_mode) {
		ret = hdmitx_common_validate_format_para(tx_comm, &tst_para);
		if (ret != 0) {
			HDMITX_DEBUG("validate format para failed %d\n", ret);
			valid_mode = false;
		}
	}

	if (valid_mode) {
		ret = count;
	} else {
		HDMITX_DEBUG("invalid_mode input:%s\n", cvalid_mode);
		ret = -1;
	}

	mutex_unlock(&tx_comm->valid_mutex);
	return ret;
}

static DEVICE_ATTR_WO(valid_mode);

static ssize_t hdmitx_cur_status_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos = hdmitx_tracer_read_event(tx_comm->tx_tracer,
		buf, PAGE_SIZE);
	return pos;
}

static DEVICE_ATTR_RO(hdmitx_cur_status);

static ssize_t debug_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "Usage:\n");
	pos += snprintf(buf + pos, PAGE_SIZE,
			"  echo xxx > /sys/class/amhdmitx/amhdmitx0/debug\n");

	return pos;
}

static ssize_t debug_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	if (strncmp(buf, "hw ", 3) == 0) {
		tx_hw->debug_func(tx_hw, buf + 3);
	} else if (strncmp(buf, "sw ", 3) == 0) {
		tx_hw->chip_data->hdmitx_ops->sw_debug_func(tx_comm, buf + 3);
		hdmitx_common_sw_debug_func(tx_comm, buf + 3);
	/* Compatible with the original commonly used debug cmd */
	} else if ((strncmp(buf, "bist", 4) == 0) ||
		(strncmp(buf, "hpd_lock", 8) == 0) ||
		(strncmp(buf, "set_div40", 9) == 0) ||
		(strncmp(buf, "hdcp_off", 8) == 0) ||
		(strncmp(buf, "aud_mute", 8) == 0) ||
		(strncmp(buf, "scdc", 4) == 0)) {
		tx_hw->debug_func(tx_hw, buf);
	}
	return count;
}

static DEVICE_ATTR_RW(debug);

/* Indicate whether a rptx under repeater */
static ssize_t hdmi_repeater_tx_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		!!tx_comm->hdcptx_comm.hdcp_rpt_en);

	return pos;
}

static DEVICE_ATTR_RO(hdmi_repeater_tx);

static ssize_t avmute_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	ret = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_AVMUTE_ST, NULL, NULL);
	pos += snprintf(buf + pos, PAGE_SIZE, "%d", ret);

	return pos;
}

/*
 *  1: set avmute
 * -1: clear avmute
 *  0: off avmute
 */
static ssize_t avmute_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int cmd = OFF_AVMUTE;
	static int mask0;
	static int mask1;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	HDMITX_INFO("%s %s\n", __func__, buf);
	if (strncmp(buf, "-1", 2) == 0) {
		cmd = CLR_AVMUTE;
		mask0 = -1;
	} else if (strncmp(buf, "0", 1) == 0) {
		cmd = OFF_AVMUTE;
		mask0 = 0;
	} else if (strncmp(buf, "1", 1) == 0) {
		cmd = SET_AVMUTE;
		mask0 = 1;
	}
	if (strncmp(buf, "r-1", 3) == 0) {
		cmd = CLR_AVMUTE;
		mask1 = -1;
	} else if (strncmp(buf, "r0", 2) == 0) {
		cmd = OFF_AVMUTE;
		mask1 = 0;
	} else if (strncmp(buf, "r1", 2) == 0) {
		cmd = SET_AVMUTE;
		mask1 = 1;
	}
	if (mask0 == 1 || mask1 == 1)
		cmd = SET_AVMUTE;
	else if ((mask0 == -1) && (mask1 == -1))
		cmd = CLR_AVMUTE;

	hdmitx_common_avmute_locked(tx_comm, cmd, AVMUTE_PATH_1);

	return count;
}

static DEVICE_ATTR_RW(avmute);

static ssize_t config_csc_en_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r",
		tx_comm->config_csc_en);

	return pos;
}

static ssize_t config_csc_en_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	u32 csc_en = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	HDMITX_INFO("store config_csc_en as %s\n", buf);

	if (com_str(buf, "0"))
		csc_en = 0;
	else if (com_str(buf, "1"))
		csc_en = 1;

	hdmitx_hw_cntl(tx_hw, VPU_CONFIG_CSC_EN, (void *)&csc_en, NULL);

	return count;
}

static DEVICE_ATTR_RW(config_csc_en);

static ssize_t soundbar_en_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n\r",
		tx_comm->event_mgr->soundbar_en_flag);

	return pos;
}

/* For controlling the soundbar switch on Android.
 * 1: turn on
 * 0: turn off
 */
static ssize_t soundbar_en_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	int soundbar_en = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	mutex_lock(&tx_comm->hdmimode_mutex);
	if (com_str(buf, "0")) {
		soundbar_en = 0;
		if (tx_comm->hpd_state)
			hdmitx_event_mgr_send_uevent(tx_comm->event_mgr,
					HDMITX_SOUNDBAR_EVENT, 1, 1);
	} else if (com_str(buf, "1")) {
		soundbar_en = 1;
		if (tx_comm->hpd_state)
			hdmitx_event_mgr_send_uevent(tx_comm->event_mgr,
					HDMITX_SOUNDBAR_EVENT, 0, 1);
	}
	tx_comm->event_mgr->soundbar_en_flag = soundbar_en;
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return count;
}

static DEVICE_ATTR_RW(soundbar_en);

static ssize_t hdmi_hdr_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	enum hdmi_tf_type type = HDMI_NONE;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	type = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_CUVA_ST, NULL, NULL);
	if (type) {
		if (type == HDMI_CUVA_TYPE) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "CUVA/VIVID-VSIF-OR-EMP");
			return pos;
		}
	}
	type = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_HDR10P_ST, NULL, NULL);
	if (type) {
		if (type == HDMI_HDR10P_DV_VSIF) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "HDR10Plus-VSIF");
			return pos;
		}
	}
	type = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_AMDV_ST, NULL, NULL);
	if (type) {
		if (type == HDMI_DV_VSIF_STD) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "DolbyVision-Std");
			return pos;
		} else if (type == HDMI_DV_VSIF_LL) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "DolbyVision-Lowlatency");
			return pos;
		}
	}
	type = hdmitx_hw_cntl(tx_hw, AUX_PKT_GET_HDR_ST, NULL, NULL);
	if (type) {
		if (type == HDMI_HDR_SMPTE_2084) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "HDR10-GAMMA_ST2084");
			return pos;
		} else if (type == HDMI_HDR_HLG) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "HDR10-GAMMA_HLG");
			return pos;
		} else if (type == HDMI_HDR_HDR) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "HDR10-others");
			return pos;
		}
	}
	/* default is SDR */
	pos += snprintf(buf + pos, PAGE_SIZE - pos, "SDR");

	return pos;
}

static DEVICE_ATTR_RO(hdmi_hdr_status);

static ssize_t rxsense_policy_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int val = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		HDMITX_INFO("set rxsense_policy as %d\n", val);
		if (val == 0 || val == 1)
			tx_comm->rxsense_policy = val;
		else
			HDMITX_INFO("only accept as 0 or 1\n");
	}
	if (tx_comm->rxsense_policy)
		queue_delayed_work(tx_comm->rxsense_wq,
				   &tx_comm->work_rxsense, 0);
	else
		cancel_delayed_work(&tx_comm->work_rxsense);

	return count;
}

static ssize_t rxsense_policy_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		tx_comm->rxsense_policy);

	return pos;
}

static DEVICE_ATTR_RW(rxsense_policy);

static ssize_t rxsense_state_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int sense;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	sense = hdmitx_hw_cntl(tx_hw, PLATFORM_RXSENSE, NULL, NULL);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d", sense);
	return pos;
}

static DEVICE_ATTR_RO(rxsense_state);

/*
 * cedst_policy: 0, no CED feature
 *	       1, auto mode, depends on RX scdc_present
 *	       2, forced CED feature
 */
static ssize_t cedst_policy_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	int val = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (isdigit(buf[0])) {
		val = buf[0] - '0';
		HDMITX_INFO("set cedst_policy as %d\n", val);
		if (val == 0 || val == 1 || val == 2) {
			tx_comm->cedst_policy = val;
			/* Auto mode, depends on Rx */
			if (val == 1) {
				/* check RX scdc_present */
				if (tx_comm->rxcap.scdc_present)
					tx_comm->cedst_en = 1;
				else
					tx_comm->cedst_en = 0;
			} else if (val == 2) {
				tx_comm->cedst_en = 1;
			} else {
				tx_comm->cedst_en = 0;
			}
		} else {
			HDMITX_INFO("only accept as 0, 1(auto), or 2(force)\n");
		}
	}
	if (tx_comm->cedst_en)
		queue_delayed_work(tx_comm->cedst_wq, &tx_comm->work_cedst, 0);
	else
		cancel_delayed_work(&tx_comm->work_cedst);

	return count;
}

static ssize_t cedst_policy_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
		tx_comm->cedst_policy);

	return pos;
}

static DEVICE_ATTR_RW(cedst_policy);

static ssize_t ready_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		tx_comm->ready);
	return pos;
}

static DEVICE_ATTR_RO(ready);

static ssize_t hdr_priority_mode_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		tx_comm->hdr_priority);

	return pos;
}

/*
 * hide or enable HDR capabilities.
 * 0 : No HDR capabilities are hidden
 * 1 : DV Capabilities are hidden
 * 2 : All HDR capabilities are hidden
 */
static ssize_t hdr_priority_mode_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	unsigned int val = 0;
	struct vinfo_s *info = NULL;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	HDMITX_INFO("%s[%d] buf:%s hdr_priority:0x%x\n", __func__, __LINE__, buf,
		tx_comm->hdr_priority);
	if ((strncmp("0", buf, 1) == 0) || (strncmp("1", buf, 1) == 0) ||
	    (strncmp("2", buf, 1) == 0)) {
		val = buf[0] - '0';
	}

	if (val == tx_comm->hdr_priority)
		return count;
	info = hdmitx_get_current_vinfo(tx_comm);
	if (!info)
		return count;
	mutex_lock(&tx_comm->hdmimode_mutex);
	tx_comm->hdr_priority = val;
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return count;
}

static DEVICE_ATTR_RW(hdr_priority_mode);

static ssize_t lipsync_cap_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct rx_cap *prxcap = &tx_comm->rxcap;

	pos += snprintf(buf + pos, PAGE_SIZE - pos, "Lip_sync(in ms)\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos, "%d, %d\n",
				prxcap->vLatency, prxcap->aLatency);
	return pos;
}

static DEVICE_ATTR_RO(lipsync_cap);

ssize_t hdcp_lstore_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	u32 lstore = tx_comm->hdcptx_comm.hdcp_lstore;

	if (lstore < 0x10) {
		lstore = 0;
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_14_LSTORE, NULL, NULL))
			lstore |= BIT(0);
		else
			hdmitx_current_status(tx_comm, HDMITX_HDCP_AUTH_NO_14_KEYS_ERROR);
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_22_LSTORE, NULL, NULL))
			lstore |= BIT(1);
		else
			hdmitx_current_status(tx_comm, HDMITX_HDCP_AUTH_NO_22_KEYS_ERROR);
	}
	if ((lstore & 0x3) == 0x3) {
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "14+22\n");
	} else {
		if (lstore & 0x1)
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "14\n");
		if (lstore & 0x2)
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "22\n");
		if ((lstore & 0xf) == 0)
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "00\n");
	}
	return pos;
}

static ssize_t hdcp_lstore_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	HDMITX_INFO("hdcp: set lstore as %s\n", buf);
	if (strncmp(buf, "-1", 2) == 0)
		tx_comm->hdcptx_comm.hdcp_lstore = 0x0;
	if (strncmp(buf, "0", 1) == 0)
		tx_comm->hdcptx_comm.hdcp_lstore = 0x10;
	if (strncmp(buf, "11", 2) == 0)
		tx_comm->hdcptx_comm.hdcp_lstore = 0x11;
	if (strncmp(buf, "12", 2) == 0)
		tx_comm->hdcptx_comm.hdcp_lstore = 0x12;
	if (strncmp(buf, "13", 2) == 0)
		tx_comm->hdcptx_comm.hdcp_lstore = 0x13;

	return count;
}

static DEVICE_ATTR_RW(hdcp_lstore);

ssize_t hdcp_mode_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int pos = 0;
	unsigned int hdcp_ret = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	switch (tx_comm->hdcptx_comm.hdcp_mode) {
	case 1:
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "14");
		break;
	case 2:
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "22");
		break;
	default:
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "off");
		break;
	}
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0 &&
	    tx_comm->hdcptx_comm.hdcp_mode > 0) {
		hdcp_ret = hdmitx_hw_cntl(tx_hw, HDCP_GET_AUTH_RESULT, NULL, NULL);
		if (hdcp_ret == 1)
			pos += snprintf(buf + pos, PAGE_SIZE - pos, ": succeed\n");
		else
			pos += snprintf(buf + pos, PAGE_SIZE - pos, ": fail\n");
	}

	return pos;
}

static ssize_t hdcp_mode_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	hdmitx_set_hdcp_mode(tx_comm, buf);
	return count;
}

static DEVICE_ATTR_RW(hdcp_mode);

ssize_t hdcp_ver_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	return hdmitx_get_hdcp_ver(tx_comm, buf, PAGE_SIZE);
}

static DEVICE_ATTR_RO(hdcp_ver);

#ifdef CONFIG_AMLOGIC_HDMITX21
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t vrr_cap_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX21)
		return _vrr_cap_show(dev, attr, buf);
	return 0;
}

static DEVICE_ATTR_RO(vrr_cap);
#endif
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
static ssize_t hdcp_clkdis_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	bool arg = false;

	if (tx_comm->hdmi_init == HDMITX20) {
		arg = buf[0] == '1';
		hdmitx_hw_cntl(tx_hw, HDCP_CLKDIS, (void *)&arg, NULL);
	}
	return count;
}

static DEVICE_ATTR_WO(hdcp_clkdis);

static ssize_t hdcp_pwr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	if (tx_comm->hdmi_init == HDMITX20) {
		if (buf[0] == '1') {
			tx_comm->hdcp_tst_sig = 1;
			HDMITX_INFO(SYS "set hdcp_pwr 1\n");
		}
		hdmitx_hw_cntl(tx_hw, HDCP_ESM_KILL, NULL, NULL);
	}
	return count;
}

static ssize_t hdcp_pwr_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20) {
		if (tx_comm->hdcp_tst_sig == 1) {
			pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
				tx_comm->hdcp_tst_sig);
			tx_comm->hdcp_tst_sig = 0;
			HDMITX_INFO(SYS "restore hdcp_pwr 0\n");
		}
	}

	return pos;
}

static DEVICE_ATTR_RW(hdcp_pwr);

static ssize_t hdcp_type_policy_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20) {
		if (strncmp(buf, "0", 1) == 0)
			val = 0;
		if (strncmp(buf, "1", 1) == 0)
			val = 1;
		if (strncmp(buf, "2", 1) == 0)
			val = 2;
		if (strncmp(buf, "-1", 2) == 0)
			val = -1;
		HDMITX_INFO(SYS "set hdcp_type_policy as %d\n", val);
		tx_comm->hdcp_type_policy = val;
	}
	return count;
}

static ssize_t hdcp_type_policy_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20) {
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
			tx_comm->hdcp_type_policy);
	}

	return pos;
}

static DEVICE_ATTR_RW(hdcp_type_policy);

/*
 * hdcp_topo_info attr
 * For hdcp 22, hdcp_tx22 will write to hdcp_topo_info_store
 * For hdcp 14, directly get from HW
 */

static ssize_t hdcp_topo_info_show(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	struct hdcprp_topo *topoinfo = NULL;

	if (tx_comm->hdmi_init == HDMITX20) {
		topoinfo = tx_comm->topo_info;
		if (!tx_comm->hdcptx_comm.hdcp_mode) {
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "hdcp mode: 0\n");
			return pos;
		}
		if (!topoinfo)
			return pos;

		if (tx_comm->hdcptx_comm.hdcp_mode == 1) {
			memset(topoinfo, 0, sizeof(struct hdcprp_topo));
			hdmitx_hw_cntl(tx_hw, HDCP14_GET_TOPO_INFO, NULL,
				(void *)&topoinfo->topo.topo14);
		}

		pos += snprintf(buf + pos, PAGE_SIZE - pos, "hdcp mode: %s\n",
			tx_comm->hdcptx_comm.hdcp_mode == 1 ? "14" : "22");
		if (tx_comm->hdcptx_comm.hdcp_mode == 2) {
			topoinfo->hdcp_ver = HDCPVER_22;
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "max_devs_exceeded: %d\n",
				topoinfo->topo.topo22.max_devs_exceeded);
			pos += snprintf(buf + pos, PAGE_SIZE - pos,
				"max_cascade_exceeded: %d\n",
				topoinfo->topo.topo22.max_cascade_exceeded);
			pos += snprintf(buf + pos, PAGE_SIZE - pos,
					"v2_0_repeater_down: %d\n",
				topoinfo->topo.topo22.v2_0_repeater_down);
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "v1_X_device_down: %d\n",
				topoinfo->topo.topo22.v1_X_device_down);
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "device_count: %d\n",
				topoinfo->topo.topo22.device_count);
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "depth: %d\n",
				topoinfo->topo.topo22.depth);
			return pos;
		}
		if (tx_comm->hdcptx_comm.hdcp_mode == 1) {
			topoinfo->hdcp_ver = HDCPVER_14;
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "max_devs_exceeded: %d\n",
				topoinfo->topo.topo14.max_devs_exceeded);
			pos += snprintf(buf + pos, PAGE_SIZE - pos,
				"max_cascade_exceeded: %d\n",
				topoinfo->topo.topo14.max_cascade_exceeded);
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "device_count: %d\n",
				topoinfo->topo.topo14.device_count);
			pos += snprintf(buf + pos, PAGE_SIZE - pos, "depth: %d\n",
				topoinfo->topo.topo14.depth);
			return pos;
		}
	}

	return pos;
}

static ssize_t hdcp_topo_info_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct hdcprp_topo *topoinfo = NULL;
	int cnt;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20) {
		topoinfo = tx_comm->topo_info;
		if (!topoinfo)
			return count;

		if (tx_comm->hdcptx_comm.hdcp_mode == 2) {
			memset(topoinfo, 0, sizeof(struct hdcprp_topo));
			cnt = sscanf(buf, "%x %x %x %x %x %x",
					(int *)&topoinfo->topo.topo22.max_devs_exceeded,
					(int *)&topoinfo->topo.topo22.max_cascade_exceeded,
					(int *)&topoinfo->topo.topo22.v2_0_repeater_down,
					(int *)&topoinfo->topo.topo22.v1_X_device_down,
					(int *)&topoinfo->topo.topo22.device_count,
					(int *)&topoinfo->topo.topo22.depth);
			if (cnt < 0)
				return count;
		}
	}

	return count;
}

static DEVICE_ATTR_RW(hdcp_topo_info);

static ssize_t hdcp22_type_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20)
		pos += snprintf(buf + pos, PAGE_SIZE, "%d\n", tx_comm->hdcp22_type);
	return pos;
}

static ssize_t hdcp22_type_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u32 type = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	if (tx_comm->hdmi_init == HDMITX20) {
		if (buf[0] == '1')
			type = 1;
		else
			type = 0;
		tx_comm->hdcp22_type = type;

		HDMITX_INFO("set hdcp22 content type %d\n", type);
		hdmitx_hw_cntl(tx_hw, HDCP_SET_TOPO_INFO, (void *)&type, NULL);
	}

	return count;
}

static DEVICE_ATTR_RW(hdcp22_type);

static ssize_t hdcp22_base_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdmi_init == HDMITX20)
		pos += snprintf(buf + pos, PAGE_SIZE, "0x%x\n", get_hdcp22_base());
	return pos;
}

static DEVICE_ATTR_RO(hdcp22_base);

static ssize_t hdcp22_stopauth_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\r\n",
		is_hdcp22_stop_state(tx_comm));
	return pos;
}

static DEVICE_ATTR_RO(hdcp22_stopauth);

/* the workaround is enabled only for special TV
 * it can also be forcely enabled for debug on other TV by
 * echo poll_en1 > /sys/class/amhdmitx/amhdmitx0/debug
 */
static ssize_t reset_tv_hdcp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u8 data;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (!tx_comm->en_poll_rx_status) {
		if (!hdmitx_find_vendor_hdcp22_non_std(tx_comm->edid_buf))
			return count;
	}
	if (strncmp(buf, "1", 1) == 0) {
		if (tx_comm->poll_rx_status_mtd == 0) {
			HDMITX_INFO("force poll rx_status to reset TV hdcp\n");
			hdmitx_reset_tv_hdcp();
		} else if (tx_comm->poll_rx_status_mtd == 1) {
			HDMITX_INFO("force read 1byte hdcp msg\n");
			ddc_read_1byte(HDCP_SLAVE, HDCP2_RD_MSG, &data);
		}
	}
	return count;
}

static DEVICE_ATTR_WO(reset_tv_hdcp);

/*
 * hdcp_repeater attr
 * For hdcp 22, hdcp_tx22 will write to hdcp_repeater_store
 * For hdcp 14, directly get bcaps bit
 */
static ssize_t hdcp_repeater_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	int pos = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;

	if (tx_comm->hdcptx_comm.hdcp_mode == 1)
		tx_comm->hdcptx_comm.hdcp_bcaps_repeater = hdmitx_hw_cntl(tx_hw,
			HDCP14_GET_BCAPS_RP, NULL, NULL);

	pos += snprintf(buf + pos, PAGE_SIZE, "%d\n",
			tx_comm->hdcptx_comm.hdcp_bcaps_repeater);

	return pos;
}

static ssize_t hdcp_repeater_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	if (tx_comm->hdcptx_comm.hdcp_mode == 2)
		tx_comm->hdcptx_comm.hdcp_bcaps_repeater = (buf[0] == '1');

	return count;
}

static DEVICE_ATTR_RW(hdcp_repeater);

static ssize_t hdcp_ctrl_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	u32 arg;

	if (hdmitx_hw_cntl(tx_hw, HDCP_14_LSTORE, NULL, NULL) == 0)
		return count;

	/* for repeater */
	if (tx_comm->hdcptx_comm.hdcp_rpt_en) {
		HDMITX_DEBUG_HDCP("hdmitx20: %s\n", buf);
		if (strncmp(buf, "rstop", 5) == 0) {
			if (strncmp(buf + 5, "14", 2) == 0) {
				arg = HDCP14_OFF;
				hdmitx_hw_cntl(tx_hw,
					HDCP_MODE_OP, (void *)&arg, NULL);
			} else if (strncmp(buf + 5, "22", 2) == 0) {
				arg = HDCP22_OFF;
				hdmitx_hw_cntl(tx_hw,
					HDCP_MODE_OP, (void *)&arg, NULL);
			}
			tx_comm->hdcptx_comm.hdcp_mode = 0;
			hdmitx_hdcp_do_work(tx_comm);
		}
		return count;
	}
	/* for non repeater */
	if (strncmp(buf, "stop", 4) == 0) {
		HDMITX_DEBUG_HDCP("hdmitx20: %s\n", buf);
		if (strncmp(buf + 4, "14", 2) == 0) {
			arg = HDCP14_OFF;
			hdmitx_hw_cntl(tx_hw,
				HDCP_MODE_OP, (void *)&arg, NULL);
		} else if (strncmp(buf + 4, "22", 2) == 0) {
			arg = HDCP22_OFF;
			hdmitx_hw_cntl(tx_hw,
				HDCP_MODE_OP, (void *)&arg, NULL);
		}
		tx_comm->hdcptx_comm.hdcp_mode = 0;
		hdmitx_hdcp_do_work(tx_comm);
	}

	return count;
}

static DEVICE_ATTR_WO(hdcp_ctrl);

static ssize_t hdcp22_top_reset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	bool arg = true;

	mutex_lock(&tx_comm->hdmimode_mutex);
	/* should not reset hdcp2.2 after hdcp2.2 auth start */
	if (tx_comm->ready) {
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return count;
	}
	HDMITX_INFO("reset hdcp2.2 module after exit hdcp2.2 auth\n");
	hdmitx_hw_cntl(tx_hw, HDCP_CLKDIS, (void *)&arg, NULL);
	hdmitx_hw_cntl(tx_hw, HDCP_RESET, NULL, NULL);
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return count;
}

static DEVICE_ATTR_WO(hdcp22_top_reset);

/*For hdcp daemon, don't del.*/
static ssize_t hdmitx_drm_flag_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pos = 0;
	int flag = 0;
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	/* notify hdcp_tx22: use flow of drm */
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0)
		flag = 1;
	else
		flag = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "%d", flag);
	return pos;
}

static DEVICE_ATTR_RO(hdmitx_drm_flag);

#endif

/*********************************************************/
int hdmitx_sysfs_common_create(struct device *dev,
		struct hdmitx_common *tx_comm,
		struct hdmitx_hw_common *tx_hw)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_hpd_state);

	ret = device_create_file(dev, &dev_attr_rawedid);
	ret = device_create_file(dev, &dev_attr_edid);
	ret = device_create_file(dev, &dev_attr_preferred_mode);
	ret = device_create_file(dev, &dev_attr_vesa_cap);
	ret = device_create_file(dev, &dev_attr_aud_cap);
	ret = device_create_file(dev, &dev_attr_valid_mode);

	ret = device_create_file(dev, &dev_attr_hdr_cap_rx);
	ret = device_create_file(dev, &dev_attr_dv_cap_rx);

	ret = device_create_file(dev, &dev_attr_phy);
	ret = device_create_file(dev, &dev_attr_avmute);

	ret = device_create_file(dev, &dev_attr_allm_mode);

	ret = device_create_file(dev, &dev_attr_hdmitx_cur_status);
	ret = device_create_file(dev, &dev_attr_hdmi_repeater_tx);

	ret = device_create_file(dev, &dev_attr_debug);
	ret = device_create_file(dev, &dev_attr_config_csc_en);
	ret = device_create_file(dev, &dev_attr_soundbar_en);
	ret = device_create_file(dev, &dev_attr_hdmi_hdr_status);
	ret = device_create_file(dev, &dev_attr_rxsense_policy);
	ret = device_create_file(dev, &dev_attr_rxsense_state);
	ret = device_create_file(dev, &dev_attr_cedst_policy);
	ret = device_create_file(dev, &dev_attr_ready);
	ret = device_create_file(dev, &dev_attr_hdr_priority_mode);
	ret = device_create_file(dev, &dev_attr_lipsync_cap);
	ret = device_create_file(dev, &dev_attr_hdcp_lstore);
	ret = device_create_file(dev, &dev_attr_hdcp_mode);
	ret = device_create_file(dev, &dev_attr_hdcp_ver);
#ifdef CONFIG_AMLOGIC_HDMITX21
	if (tx_comm->hdmi_init == HDMITX21)
		ret = device_create_file(dev, &dev_attr_vrr_cap);
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
	if (tx_comm->hdmi_init == HDMITX20) {
		ret = device_create_file(dev, &dev_attr_hdcp_clkdis);
		ret = device_create_file(dev, &dev_attr_hdcp_pwr);
		ret = device_create_file(dev, &dev_attr_hdcp_type_policy);
		ret = device_create_file(dev, &dev_attr_hdcp_topo_info);
		ret = device_create_file(dev, &dev_attr_hdcp22_type);
		ret = device_create_file(dev, &dev_attr_hdcp22_base);
		ret = device_create_file(dev, &dev_attr_hdcp22_stopauth);
		ret = device_create_file(dev, &dev_attr_reset_tv_hdcp);
		ret = device_create_file(dev, &dev_attr_hdcp_repeater);
		ret = device_create_file(dev, &dev_attr_hdcp_ctrl);
		ret = device_create_file(dev, &dev_attr_hdcp22_top_reset);
		ret = device_create_file(dev, &dev_attr_hdmitx_drm_flag);
	}
#endif
	return ret;
}

int hdmitx_sysfs_common_destroy(struct device *dev)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(dev);

	device_remove_file(dev, &dev_attr_hpd_state);

	device_remove_file(dev, &dev_attr_rawedid);
	device_remove_file(dev, &dev_attr_edid);
	device_remove_file(dev, &dev_attr_preferred_mode);
	device_remove_file(dev, &dev_attr_vesa_cap);
	device_remove_file(dev, &dev_attr_aud_cap);
	device_remove_file(dev, &dev_attr_valid_mode);

	device_remove_file(dev, &dev_attr_hdr_cap_rx);
	device_remove_file(dev, &dev_attr_dv_cap_rx);

	device_remove_file(dev, &dev_attr_phy);
	device_remove_file(dev, &dev_attr_avmute);

	device_remove_file(dev, &dev_attr_allm_mode);

	device_remove_file(dev, &dev_attr_hdmitx_cur_status);
	device_remove_file(dev, &dev_attr_hdmi_repeater_tx);

	device_remove_file(dev, &dev_attr_debug);
	device_remove_file(dev, &dev_attr_config_csc_en);
	device_remove_file(dev, &dev_attr_soundbar_en);
	device_remove_file(dev, &dev_attr_hdmi_hdr_status);
	device_remove_file(dev, &dev_attr_rxsense_policy);
	device_remove_file(dev, &dev_attr_rxsense_state);
	device_remove_file(dev, &dev_attr_cedst_policy);
	device_remove_file(dev, &dev_attr_ready);
	device_remove_file(dev, &dev_attr_hdr_priority_mode);
	device_remove_file(dev, &dev_attr_lipsync_cap);
	device_remove_file(dev, &dev_attr_hdcp_lstore);
	device_remove_file(dev, &dev_attr_hdcp_mode);
	device_remove_file(dev, &dev_attr_hdcp_ver);
#ifdef CONFIG_AMLOGIC_HDMITX21
	if (tx_comm->hdmi_init == HDMITX21)
		device_remove_file(dev, &dev_attr_vrr_cap);
#endif

#ifdef CONFIG_AMLOGIC_HDMITX
	if (tx_comm->hdmi_init == HDMITX20) {
		device_remove_file(dev, &dev_attr_hdcp_clkdis);
		device_remove_file(dev, &dev_attr_hdcp_pwr);
		device_remove_file(dev, &dev_attr_hdcp_type_policy);
		device_remove_file(dev, &dev_attr_hdcp_topo_info);
		device_remove_file(dev, &dev_attr_hdcp22_type);
		device_remove_file(dev, &dev_attr_hdcp22_base);
		device_remove_file(dev, &dev_attr_hdcp22_stopauth);
		device_remove_file(dev, &dev_attr_reset_tv_hdcp);
		device_remove_file(dev, &dev_attr_hdcp_repeater);
		device_remove_file(dev, &dev_attr_hdcp_ctrl);
		device_remove_file(dev, &dev_attr_hdcp22_top_reset);
		device_remove_file(dev, &dev_attr_hdmitx_drm_flag);
	}
#endif

	return 0;
}

