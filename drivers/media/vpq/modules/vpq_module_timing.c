// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include <linux/kernel.h>
#include "vpq_module_timing.h"
#include "../vpq_printk.h"
#include "../vpq_vfm.h"
#include "../vpq_common.h"

static unsigned char nonstandard_map_count;
static struct nonstandard_timing_map_s nonstandard_maps[RESERVE_NONSTANDARD_TIMING_COUNT];

static const struct sig_info_t sig_info_table[] = {
	{"VGA",      "--",       "--",    "--"}, // PQ_SRC_INDEX_VGA

	{"SV",       "NTSC",     "--",    "--"}, // PQ_SRC_INDEX_SV_NTSC
	{"SV",       "PAL",      "--",    "--"},
	{"SV",       "PAL_M",    "--",    "--"},
	{"SV",       "SECAM",    "--",    "--"},

	{"YCBCR",    "480",      "I",     "SDR"}, // PQ_SRC_INDEX_YCBCR_480I
	{"YCBCR",    "576",      "I",     "SDR"},
	{"YCBCR",    "480",      "P",     "SDR"},
	{"YCBCR",    "576",      "P",     "SDR"},
	{"YCBCR",    "720",      "P",     "SDR"},
	{"YCBCR",    "1080",     "I",     "SDR"},
	{"YCBCR",    "1080",     "P",     "SDR"},

	{"ATV",      "NTSC",     "--",    "--"}, // PQ_SRC_INDEX_ATV_NTSC
	{"ATV",      "PAL",      "--",    "--"},
	{"ATV",      "PAL_M",    "--",    "--"},
	{"ATV",      "SECAM",    "--",    "--"},
	{"ATV",      "NTSC443",  "--",    "--"},
	{"ATV",      "PAL60",    "--",    "--"},
	{"ATV",      "NTSC50",   "--",    "--"},
	{"ATV",      "PAL_N",    "--",    "--"},

	{"AV",       "NTSC",     "--",    "--"}, // PQ_SRC_INDEX_AV_NTSC
	{"AV",       "PAL",      "--",    "--"},
	{"AV",       "PAL_M",    "--",    "--"},
	{"AV",       "SECAM",    "--",    "--"},
	{"AV",       "NTSC443",  "--",    "--"},
	{"AV",       "PAL60",    "--",    "--"},
	{"AV",       "NTSC50",   "--",    "--"},
	{"AV",       "PAL_N",    "--",    "--"},

	{"HDMI",     "480",      "I",     "SDR"}, // PQ_SRC_INDEX_HDMI_480I
	{"HDMI",     "576",      "I",     "SDR"},
	{"HDMI",     "480",      "P",     "SDR"},
	{"HDMI",     "576",      "P",     "SDR"},
	{"HDMI",     "720",      "P",     "SDR"},
	{"HDMI",     "1080",     "I",     "SDR"},
	{"HDMI",     "1080",     "P",     "SDR"},
	{"HDMI",     "2160",     "I",     "SDR"},
	{"HDMI",     "2160",     "P",     "SDR"},

	{"HDMI",     "480",      "I",     "HDR10"}, // PQ_SRC_INDEX_HDR10_HDMI_480I
	{"HDMI",     "576",      "I",     "HDR10"},
	{"HDMI",     "480",      "P",     "HDR10"},
	{"HDMI",     "576",      "P",     "HDR10"},
	{"HDMI",     "720",      "P",     "HDR10"},
	{"HDMI",     "1080",     "I",     "HDR10"},
	{"HDMI",     "1080",     "P",     "HDR10"},
	{"HDMI",     "2160",     "I",     "HDR10"},
	{"HDMI",     "2160",     "P",     "HDR10"},

	{"HDMI",     "480",      "I",     "HLG"}, // PQ_SRC_INDEX_HLG_HDMI_480I
	{"HDMI",     "576",      "I",     "HLG"},
	{"HDMI",     "480",      "P",     "HLG"},
	{"HDMI",     "576",      "P",     "HLG"},
	{"HDMI",     "720",      "P",     "HLG"},
	{"HDMI",     "1080",     "I",     "HLG"},
	{"HDMI",     "1080",     "P",     "HLG"},
	{"HDMI",     "2160",     "I",     "HLG"},
	{"HDMI",     "2160",     "P",     "HLG"},

	{"HDMI",     "480",      "I",     "DV"}, // PQ_SRC_INDEX_AMDV_HDMI_480I
	{"HDMI",     "576",      "I",     "DV"},
	{"HDMI",     "480",      "P",     "DV"},
	{"HDMI",     "576",      "P",     "DV"},
	{"HDMI",     "720",      "P",     "DV"},
	{"HDMI",     "1080",     "I",     "DV"},
	{"HDMI",     "1080",     "P",     "DV"},
	{"HDMI",     "2160",     "I",     "DV"},
	{"HDMI",     "2160",     "P",     "DV"},

	{"HDMI",     "480",      "I",     "HDR10P"}, // PQ_SRC_INDEX_HDR10p_HDMI_480I
	{"HDMI",     "576",      "I",     "HDR10P"},
	{"HDMI",     "480",      "P",     "HDR10P"},
	{"HDMI",     "576",      "P",     "HDR10P"},
	{"HDMI",     "720",      "P",     "HDR10P"},
	{"HDMI",     "1080",     "I",     "HDR10P"},
	{"HDMI",     "1080",     "P",     "HDR10P"},
	{"HDMI",     "2160",     "I",     "HDR10P"},
	{"HDMI",     "2160",     "P",     "HDR10P"},

	{"DTV",      "480",      "I",     "SDR"}, // PQ_SRC_INDEX_DTV_480I
	{"DTV",      "576",      "I",     "SDR"},
	{"DTV",      "480",      "P",     "SDR"},
	{"DTV",      "576",      "P",     "SDR"},
	{"DTV",      "720",      "P",     "SDR"},
	{"DTV",      "1080",     "I",     "SDR"},
	{"DTV",      "1080",     "P",     "SDR"},
	{"DTV",      "2160",     "I",     "SDR"},
	{"DTV",      "2160",     "P",     "SDR"},

	{"DTV",      "480",      "I",     "HDR10"}, // PQ_SRC_INDEX_HDR10_DTV_480I
	{"DTV",      "576",      "I",     "HDR10"},
	{"DTV",      "480",      "P",     "HDR10"},
	{"DTV",      "576",      "P",     "HDR10"},
	{"DTV",      "720",      "P",     "HDR10"},
	{"DTV",      "1080",     "I",     "HDR10"},
	{"DTV",      "1080",     "P",     "HDR10"},
	{"DTV",      "2160",     "I",     "HDR10"},
	{"DTV",      "2160",     "P",     "HDR10"},

	{"DTV",      "480",      "I",     "HLG"}, // PQ_SRC_INDEX_HLG_DTV_480I
	{"DTV",      "576",      "I",     "HLG"},
	{"DTV",      "480",      "P",     "HLG"},
	{"DTV",      "576",      "P",     "HLG"},
	{"DTV",      "720",      "P",     "HLG"},
	{"DTV",      "1080",     "I",     "HLG"},
	{"DTV",      "1080",     "P",     "HLG"},
	{"DTV",      "4K2K",     "I",     "HLG"},
	{"DTV",      "4K2K",     "P",     "HLG"},

	{"DTV",      "480",      "I",     "DV"}, // PQ_SRC_INDEX_AMDV_DTV_480I
	{"DTV",      "576",      "I",     "DV"},
	{"DTV",      "480",      "P",     "DV"},
	{"DTV",      "576",      "P",     "DV"},
	{"DTV",      "720",      "P",     "DV"},
	{"DTV",      "1080",     "I",     "DV"},
	{"DTV",      "1080",     "P",     "DV"},
	{"DTV",      "2160",     "I",     "DV"},
	{"DTV",      "2160",     "P",     "DV"},

	{"DTV",      "480",      "I",     "HDR10P"}, // PQ_SRC_INDEX_HDR10p_DTV_480I
	{"DTV",      "576",      "I",     "HDR10P"},
	{"DTV",      "480",      "P",     "HDR10P"},
	{"DTV",      "576",      "P",     "HDR10P"},
	{"DTV",      "720",      "P",     "HDR10P"},
	{"DTV",      "1080",     "I",     "HDR10P"},
	{"DTV",      "1080",     "P",     "HDR10P"},
	{"DTV",      "2160",     "I",     "HDR10P"},
	{"DTV",      "2160",     "P",     "HDR10P"},

	{"MPEG",     "480",      "I",     "SDR"}, // PQ_SRC_INDEX_MPEG_480I
	{"MPEG",     "576",      "I",     "SDR"},
	{"MPEG",     "480",      "P",     "SDR"},
	{"MPEG",     "576",      "P",     "SDR"},
	{"MPEG",     "720",      "P",     "SDR"},
	{"MPEG",     "1080",     "I",     "SDR"},
	{"MPEG",     "1080",     "P",     "SDR"},
	{"MPEG",     "2160",     "I",     "SDR"},
	{"MPEG",     "2160",     "P",     "SDR"},

	{"MPEG",     "480",      "I",     "HDR10"}, // PQ_SRC_INDEX_HDR10_MPEG_480I
	{"MPEG",     "576",      "I",     "HDR10"},
	{"MPEG",     "480",      "P",     "HDR10"},
	{"MPEG",     "576",      "P",     "HDR10"},
	{"MPEG",     "720",      "P",     "HDR10"},
	{"MPEG",     "1080",     "I",     "HDR10"},
	{"MPEG",     "1080",     "P",     "HDR10"},
	{"MPEG",     "2160",     "I",     "HDR10"},
	{"MPEG",     "2160",     "P",     "HDR10"},

	{"MPEG",     "480",      "I",     "HLG"}, // PQ_SRC_INDEX_HLG_MPEG_480I
	{"MPEG",     "576",      "I",     "HLG"},
	{"MPEG",     "480",      "P",     "HLG"},
	{"MPEG",     "576",      "P",     "HLG"},
	{"MPEG",     "720",      "P",     "HLG"},
	{"MPEG",     "1080",     "I",     "HLG"},
	{"MPEG",     "1080",     "P",     "HLG"},
	{"MPEG",     "2160",     "I",     "HLG"},
	{"MPEG",     "2160",     "P",     "HLG"},

	{"MPEG",     "480",      "I",     "DV"}, // PQ_SRC_INDEX_AMDV_MPEG_480I
	{"MPEG",     "576",      "I",     "DV"},
	{"MPEG",     "480",      "P",     "DV"},
	{"MPEG",     "576",      "P",     "DV"},
	{"MPEG",     "720",      "P",     "DV"},
	{"MPEG",     "1080",     "I",     "DV"},
	{"MPEG",     "1080",     "P",     "DV"},
	{"MPEG",     "2160",     "I",     "DV"},
	{"MPEG",     "2160",     "P",     "DV"},

	{"MPEG",     "480",      "I",     "HDR10P"}, // PQ_SRC_INDEX_HDR10p_MPEG_480I
	{"MPEG",     "576",      "I",     "HDR10P"},
	{"MPEG",     "480",      "P",     "HDR10P"},
	{"MPEG",     "576",      "P",     "HDR10P"},
	{"MPEG",     "720",      "P",     "HDR10P"},
	{"MPEG",     "1080",     "I",     "HDR10P"},
	{"MPEG",     "1080",     "P",     "HDR10P"},
	{"MPEG",     "2160",     "I",     "HDR10P"},
	{"MPEG",     "2160",     "P",     "HDR10P"},
};

// get source type string
static const char *get_source_type_string(vpq_vfm_source_type_e src_type)
{
	switch (src_type) {
	case VFRAME_SOURCE_TYPE_TUNER:
		return "DTV";
	case VFRAME_SOURCE_TYPE_CVBS:
		return "AV";
	case VFRAME_SOURCE_TYPE_OTHERS:
		return "MPEG";
	case VFRAME_SOURCE_TYPE_HDMI:
		return "HDMI";
	default:
		return "MPEG";
	}
}

// get resolution string
static const char *get_resolution_string(unsigned int height)
{
	if (height <= 480)
		return "480";
	else if (height <= 576)
		return "576";
	else if (height <= 720)
		return "720";
	else if (height <= 1080)
		return "1080";
	else
		return "2160";
}

// get hdr type string
static const char *get_hdr_type_string(enum vpq_vfm_hdr_type_e hdr_type)
{
	switch (hdr_type) {
	case VPQ_VFM_HDR_TYPE_HDR10:
		return "HDR10";
	case VPQ_VFM_HDR_TYPE_HDR10PLUS:
		return "HDR10P";
	case VPQ_VFM_HDR_TYPE_AMDV:
		return "DV";
	case VPQ_VFM_HDR_TYPE_HLG:
		return "HLG";
	default:
		return "SDR";
	}
}

// get scan mode string
static const char *get_scan_mode_string(enum vpq_vfm_scan_mode_e scan_mode)
{
	return (scan_mode == VPQ_VFM_SCAN_MODE_PROGRESSIVE) ? "P" : "I";
}

// map signal info to string
static void map_signal_info(unsigned int height,
	vpq_vfm_source_type_e src_type,
	enum vpq_vfm_scan_mode_e scan_mode,
	enum vpq_vfm_hdr_type_e hdr_type,
	struct sig_info_t *sig_info)
{
	sig_info->source = get_source_type_string(src_type);
	sig_info->height = get_resolution_string(height);
	sig_info->scan_mode = get_scan_mode_string(scan_mode);
	sig_info->hdr_type = get_hdr_type_string(hdr_type);
}

// compare signal info
static int compare_signal_info(const struct sig_info_t *info_in,
	const struct sig_info_t *info)
{
	return (strcmp(info_in->source, info->source) == 0 &&
			strcmp(info_in->height, info->height) == 0 &&
			strcmp(info_in->scan_mode, info->scan_mode) == 0 &&
			strcmp(info_in->hdr_type, info->hdr_type) == 0);
}

// find mapping timing index
static enum pq_source_timing_e find_timing_index(const struct sig_info_t *info)
{
	int i = 0;
	int table_size = ARRAY_SIZE(sig_info_table);

	for (i = 0; i < table_size; i++) {
		if (compare_signal_info(info, &sig_info_table[i])) {
			vpq_pr_dbg(lev_mod, "matched standard timing index:%d\n", i);
			return (enum pq_source_timing_e)i;
		}
	}

	vpq_pr_dbg(lev_mod, "no standard timing matched, use PQ_SRC_INDEX_MPEG_1080P\n");
	return PQ_SRC_INDEX_MPEG_1080P;
}

// get ATV/AV source timing index
static enum pq_source_timing_e get_analog_source_index(vpq_vfm_source_type_e src_type,
	vpq_vfm_source_mode_e src_mode)
{
	if (src_type == VFRAME_SOURCE_TYPE_TUNER) {
		if (src_mode == VFRAME_SOURCE_MODE_PAL)
			return PQ_SRC_INDEX_ATV_PAL;
		else
			return PQ_SRC_INDEX_ATV_NTSC;
	} else if (src_type == VFRAME_SOURCE_TYPE_CVBS) {
		if (src_mode == VFRAME_SOURCE_MODE_PAL)
			return PQ_SRC_INDEX_AV_PAL;
		else if (src_mode == VFRAME_SOURCE_MODE_SECAM)
			return PQ_SRC_INDEX_AV_SECAM;
		else
			return PQ_SRC_INDEX_AV_NTSC;
	}

	return PQ_SRC_INDEX_AV_NTSC;
}

int vpq_module_timing_set_nonstandard_map(unsigned char value,
	struct nonstandard_timing_map_s *pdata)
{
	unsigned char i = 0;

	if (!pdata)
		return RET_NULL_POINT;

	nonstandard_map_count = value;
	for (i = 0; i < value; i++) {
		nonstandard_maps[i] =  pdata[i];
		vpq_pr_info("non-standard timing map[%d]:%d,%d, %s,%s,%d\n",
			i, pdata[i].height, pdata[i].width, pdata[i].hdr_string,
			pdata[i].src_string, pdata[i].timing_index);
	}

	return 0;
}

static enum pq_source_timing_e check_nonstandard_timing(unsigned int height,
	unsigned int width,
	const char *hdr_string,
	const char *src_string)
{
	unsigned char i = 0;

	for (i = 0; i < nonstandard_map_count; i++) {
		if (nonstandard_maps[i].height == height &&
			nonstandard_maps[i].width == width &&
			strcmp(nonstandard_maps[i].hdr_string, hdr_string) == 0 &&
			strcmp(nonstandard_maps[i].src_string, src_string) == 0) {
			vpq_pr_dbg(lev_mod, "matched non-standard timing: %d, %d, %s, %s, %d\n",
				height, width, hdr_string, src_string,
				nonstandard_maps[i].timing_index);
			return (enum pq_source_timing_e)nonstandard_maps[i].timing_index;
		}
	}

	vpq_pr_dbg(lev_mod, "no non-standard timing index\n");
	return PQ_SRC_INDEX_MAX;
}

// get timing index
unsigned char vpq_module_timing_table_index(enum pq_table_module_index_e module)
{
	unsigned char tab_index;
	unsigned int height, width;
	vpq_vfm_source_type_e src_type;
	vpq_vfm_source_mode_e src_mode;
	enum vpq_vfm_scan_mode_e scan_mode;
	enum vpq_vfm_hdr_type_e hdr_type;
	enum pq_source_timing_e timing_index;
	struct sig_info_t cur_sig_info;

	// get current vframe signal info
	src_type = vpq_vfm_get_source_type();
	vpq_vfm_get_height_width(&height, &width);
	scan_mode = vpq_vfm_get_signal_scan_mode();
	hdr_type = vpq_vfm_get_hdr_type();
	vpq_pr_dbg(lev_mod, "current signal source/h-w/scan/hdr:%d, %d, %d, %d, %d\n",
		src_type, height, width, scan_mode, hdr_type);

	// get standard and non-standard source timing index
	if (src_type == VFRAME_SOURCE_TYPE_TUNER ||
		src_type == VFRAME_SOURCE_TYPE_CVBS) {
		src_mode = vpq_vfm_get_source_mode();
		timing_index = get_analog_source_index(src_type, src_mode);
	} else {
		// check non-standard timing index process
		const char *hdr_string = get_hdr_type_string(hdr_type);
		const char *src_string = get_source_type_string(src_type);
		int nonstandard_index = check_nonstandard_timing(height,
			width, hdr_string, src_string);

		if (nonstandard_index >= PQ_SRC_INDEX_RESERVE_1 &&
			nonstandard_index <= PQ_SRC_INDEX_RESERVE_1) {
			timing_index = nonstandard_index;
			vpq_pr_dbg(lev_mod, "use non-standard timing index:%d\n", timing_index);
			goto get_table_index;
		}

		// check standard timing index process
		map_signal_info(height, src_type, scan_mode, hdr_type, &cur_sig_info);
		timing_index = find_timing_index(&cur_sig_info);
	}

get_table_index:
	// get pq table index
	tab_index = pq_table_param.pq_index_table0[timing_index][module];
	vpq_pr_dbg(lev_mod, "module:%d final table index:%d\n", module, tab_index);

	return tab_index;
}
