// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_common.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
#include <linux/amlogic/media/amprime_sl/prime_sl.h>
#endif
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/video_sink/video.h>
#include "arch/vpp_regs.h"
#include "reg_helper.h"
#include "amcsc.h"
#include "set_hdr2_v0.h"
#include "hdr/am_hdr10_plus.h"
#include "hdr/gamut_convert.h"
#include "amve_v2.h"
#include "hdr/am_hdr_sbtm.h"
#include <linux/amlogic/media/di/dpss_interface.h>
#include "hdr/am_hdr10_tmo_fw.h"

static enum output_format_e target_format[VD_PATH_MAX];
static enum hdr_type_e cur_source_format[VD_PATH_MAX];
enum output_format_e output_format;
uint sbtm_pkt_clear;

/*bit4: force mode; bit 0-3 slice num */
uint slice_set = 0x14;
u32 prime_mode = BIT(0);

#define INORM  50000
static u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},	/* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},	/* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},	/* R */
};

static u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static const char *module_str[HDR_MAX] = {
	"UNKNOWN",
	"VD1",
	"VD2",
	"VD3",
	"OSD1",
	"OSD2",
	"VDIN0",
	"VDIN1",
	"DI_HDR",
	"DI_M2M_HDR",
	"OSD3",
	"S5_VD1_S1",
	"S5_VD1_S2",
	"S5_VD1_S3",
	"VICP",
	"VDIN2_HDR",
	"VD1_1_HDR",
	"VD2_1_HDR",
};

static const char *process_str[34] = {
	"UNKNOWN",
	"HDR_BYPASS",
	"HDR_SDR",
	"SDR_HDR",
	"HLG_BYPASS",
	"HLG_SDR",
	"HLG_HDR",
	"SDR_HLG",
	"SDR_IPT",
	"HDR_IPT",
	"HLG_IPT",
	"HDR_HLG",
	"HDR10P_SDR",
	"SDR_GMT_CONVERT",
	"IPT_MAP",
	"IPT_SDR",
	"CUVA_BYPASS",
	"CUVA_SDR",
	"CUVA_HDR",
	"CUVA_HLG",
	"CUVA_IPT",
	"SDR_CUVA",
	"HDR_CUVA",
	"HLG_CUVA",
	"CUVAHLG_SDR",
	"CUVAHLG_HDR",
	"CUVAHLG_HLG",
	"CUVAHLG_CUVA",
	"SDR_AC_ENH",
	"HDR_HDR",
	"RGB_OSD",
	"RGB_VDIN",
	"FULL_VDIN",
	"HLGP_SDR"
};

static const char *policy_str[3] = {
	"follow_sink",
	"follow_source",
	"force_output"
};

static const char *input_str[13] = {
	"NONE",
	"HDR",
	"HDR+",
	"DOVI",
	"PRIME",
	"HLG",
	"SDR",
	"MVC",
	"CUVA_HDR",
	"CUVA_HLG",
	"SDR_BT2020",
	"HDR10_709",
	"HLG+"
};

/* output_format_e */
static const char *output_str[12] = {
	"UNKNOWN",
	"709",
	"709_HDR",
	"2020",
	"HDR",
	"HDR+",
	"HLG",
	"IPT",
	"CUVA_HDR",
	"CUVA_HLG",
	"HLG+",
	"BYPASS"
};

static const char *dv_output_str[6] = {
	"IPT",
	"TUNNEL",
	"HDR10",
	"SDR10",
	"SDR8",
	"BYPASS"
};

static int process_id[VD_PATH_MAX];
static enum vd_path_e cur_vd_path;

void hdr_proc(struct vframe_s *vf,
	      enum hdr_module_sel module_sel,
	      u32 hdr_process_select,
	      struct vinfo_s *vinfo,
	      struct matrix_s *gmt_mtx,
	      enum vpp_index_e vpp_index)
{
	enum hdr_process_sel cur_hdr_process;
	int limit_full = 0;
	int i, index;
	unsigned int is_dd_frame = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_enable() && vf && is_amdv_frame(vf))
		is_dd_frame = 1;
#endif
	if ((mosaic_mode &&  cur_vd_path != VD1_PATH) &&
		(module_sel == OSD1_HDR || module_sel == OSD2_HDR ||
		module_sel == OSD3_HDR)) //in mosaic mode, only vd1 process osd hdr
		return;

	if (vf) {
		limit_full = (vf->signal_type >> 25) & 0x01;
		pr_csc(128, "signal_type/type= 0x%x/0x%x,dpss_flg=0x%x\n",
			vf->signal_type, vf->type, vf->dpss_flg);
	}

//	pr_csc(128, "module_sel = %d, limit_full = %d\n",
//		module_sel, limit_full);

	/* RGB / YUV vdin input handling  prepare extra op code or info */
	if (vf && vf->type & VIDTYPE_RGB_444 && !is_amdv_on())
		hdr_process_select |= RGB_VDIN;

	if (limit_full && !is_amdv_on())
		hdr_process_select |= FULL_VDIN;
	/* RGB / YUV input handling */

//	pr_csc(128, "hdr_process_select = 0x%08x, pre_/dpss_mode = %d/%d\n",
//		hdr_process_select, pre_dpss_mode, dpss_mode);

	if (vf && !dpss_mode &&
		!is_dd_frame && module_sel == VD1_HDR &&
		!fake_frame_flag[VD1_PATH])
		set_muxio_link_mode(1, vf, vpp_index);

/*
 *	if (vf && !pre_dpss_mode && !dpss_mode &&
 *		!is_dd_frame && module_sel == VD1_HDR &&
 *		!(hdr_process_select & HDR_BYPASS)) {
 *		if ((vf->dpss_flg & DPSS_FLG_HDR) ||
 *		(vf->dpss_flg & DPSS_FLG_DDD)) {
 *			pr_csc(128, "%s: skip dpss vf(%p) index=%d, frame_index=%d\n",
 *				__func__, vf, vf->index, vf->frame_index);
 *			pr_csc(128, "%s: dpss_flg = 0x%x\n", __func__, vf->dpss_flg);
 *		} else {
 *			set_muxio_link_mode(1, vf, vpp_index);
 *		}
 *	}
 */
	if (hdr_process_select & HDR10P_SDR)
		cur_hdr_process = hdr10p_func(module_sel,
			hdr_process_select, vinfo, gmt_mtx, vpp_index);
	else
		cur_hdr_process = hdr_func(module_sel,
			hdr_process_select, vinfo, gmt_mtx, vpp_index);

	index = 0;
	for (i = 0; i < 32; i++) {
		if (BIT(i) == (hdr_process_select & 0xffffffff)) {
			index = i + 1;
			break;
		}
	}

	pr_csc(8, "am_vecm: hdr module=%s, process=%s, select=%x\n",
		module_str[module_sel],
		process_str[index],
		hdr_process_select);

	if (module_sel == VD1_HDR)
		process_id[0] = index;
	else if (module_sel == VD2_HDR)
		process_id[1] = index;
	else if (module_sel == VD3_HDR)
		process_id[2] = index;
}

void hdr_proc_ext(struct vframe_s *vf,
	      enum hdr_module_sel module_sel,
	      u32 hdr_process_select,
	      struct vinfo_s *vinfo,
	      struct matrix_s *gmt_mtx,
	      enum vpp_index_e vpp_index)
{
	enum hdr_process_sel cur_hdr_process;
	int limit_full = 0;
	int index = 33;

	if (vf)
		limit_full = (vf->signal_type >> 25) & 0x01;

	/* RGB / YUV vdin input handling  prepare extra op code or info */
	if (vf && vf->type & VIDTYPE_RGB_444 && !is_amdv_on())
		hdr_process_select |= RGB_VDIN;

	if (limit_full && !is_amdv_on())
		hdr_process_select |= FULL_VDIN;
	/* RGB / YUV input handling */
//	pr_csc(128, "%s: hdr_process_select = 0x%08x, module_sel = %d, limit_full = %d\n",
//		__func__, hdr_process_select, module_sel, limit_full);

	if (hdr_process_select & HLGP_SDR)
		cur_hdr_process =
		hlgp_func(module_sel, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	else
		return;

	if (module_sel == VD1_HDR)
		process_id[0] = index;
	else if (module_sel == VD2_HDR)
		process_id[1] = index;
	else if (module_sel == VD3_HDR)
		process_id[2] = index;
}

int get_max_slice(void)
{
	int slice_max = 1;

	if (is_meson_t3x_cpu())
		slice_max = 2;
	else if (is_meson_s5_cpu())
		slice_max = 4;

	return slice_max;
}

void hdr_proc_multi_slices(struct vframe_s *vf,
	      enum hdr_module_sel module_sel,
	      u32 hdr_process_select,
	      struct vinfo_s *vinfo,
	      struct matrix_s *gmt_mtx,
	      s32 slice_mode,
	      enum vpp_index_e vpp_index)
{
	u32 slice_max = 0;

	if (module_sel != VD1_HDR) {
		pr_csc(8, "am_vecm: hdr module=%s %d has no multi-slice\n",
			module_str[module_sel], module_sel);
		return;
	}

	if (slice_mode == VD1_1SLICE) {
		pr_csc(8, "am_vecm: hdr module=%s %d, sel %d, slice_num = %d\n",
			module_str[module_sel], module_sel, hdr_process_select, slice_mode);
		hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == VD1_2SLICE) {
		hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == VD1_4SLICE) {
		hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc(vf, S5_VD1_SLICE2, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc(vf, S5_VD1_SLICE3, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == 0) {/*vd1 off, still need to handle vd1 process*/
		slice_max = get_max_slice();
		pr_csc(8, "am_vecm: hdr module=%s %d, slice_max %d\n",
			module_str[module_sel], module_sel, slice_max);
		if (slice_max == 1) {
			hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		} else if (slice_max == 2) {
			hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		} else if (slice_max == 4) {
			hdr_proc(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc(vf, S5_VD1_SLICE2, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc(vf, S5_VD1_SLICE3, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		}
	}
}

void hdr_proc_multi_slices_ext(struct vframe_s *vf,
	      enum hdr_module_sel module_sel,
	      u32 hdr_process_select,
	      struct vinfo_s *vinfo,
	      struct matrix_s *gmt_mtx,
	      s32 slice_mode,
	      enum vpp_index_e vpp_index)
{
	u32 slice_max = 0;

	if (module_sel != VD1_HDR) {
		pr_csc(8, "am_vecm: hdr module=%s %d has no multi-slice\n",
			module_str[module_sel], module_sel);
		return;
	}

	if (slice_mode == VD1_1SLICE) {
		pr_csc(8, "am_vecm: hdr module=%s %d, sel %d, slice_num = %d\n",
			module_str[module_sel], module_sel, hdr_process_select, slice_mode);
		hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == VD1_2SLICE) {
		hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc_ext(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == VD1_4SLICE) {
		hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc_ext(vf, S5_VD1_SLICE1, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc_ext(vf, S5_VD1_SLICE2, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		hdr_proc_ext(vf, S5_VD1_SLICE3, hdr_process_select, vinfo, gmt_mtx, vpp_index);
	} else if (slice_mode == 0) {/*vd1 off, still need to handle vd1 process*/
		slice_max = get_max_slice();
		pr_csc(8, "am_vecm: hdr module=%s %d, slice_max %d\n",
			module_str[module_sel], module_sel, slice_max);
		if (slice_max == 1) {
			hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
		} else if (slice_max == 2) {
			hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc_ext(vf, S5_VD1_SLICE1,
				hdr_process_select, vinfo, gmt_mtx, vpp_index);
		} else if (slice_max == 4) {
			hdr_proc_ext(vf, VD1_HDR, hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc_ext(vf, S5_VD1_SLICE1,
				hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc_ext(vf, S5_VD1_SLICE2,
				hdr_process_select, vinfo, gmt_mtx, vpp_index);
			hdr_proc_ext(vf, S5_VD1_SLICE3,
				hdr_process_select, vinfo, gmt_mtx, vpp_index);
		}
	}
}
#endif

void get_hdr_process_name(int id, char *name, char *output_fmt)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int index;

	if (id > VD_PATH_MAX - 1)
		return;
	index = process_id[id];
	memcpy(name, process_str[index], strlen(process_str[index]) + 1);
	memcpy(output_fmt, output_str[target_format[id]],
	       strlen(output_str[target_format[id]]) + 1);
#endif
}
EXPORT_SYMBOL(get_hdr_process_name);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifdef T7_BRINGUP_MULTI_VPP
int hdr_policy_process_t7(struct vinfo_s *vinfo,
		       enum hdr_type_e *source_format,
		       enum vd_path_e vd_path)
{
	if (get_cpu_type() != MESON_CPU_MAJOR_ID_T7)
		return 0;

	return 0;
}
#endif

void vd2_map_top1_policy_process(struct vinfo_s *vinfo,
		       enum hdr_type_e *source_format,
		       enum vd_path_e vd_path,
		       enum vpp_index_e vpp_index,
		       struct vframe_s *vf)
{
	int dv_hdr_policy = 0;
	int cur_hdr_policy = 0;
	bool hdr10_plus_support =
		sink_hdr_support(vinfo) & HDRP_SUPPORT;
	bool cuva_support =
		sink_hdr_support(vinfo) & CUVA_SUPPORT;
	bool output_mode = 0;
	bool hlg_plus_support =
		sink_hdr_support(vinfo) & HLGP_SUPPORT;

	if (vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
		vinfo->mode == VMODE_DUMMY_ENCP)
		output_mode = 1;//panel
	else
		output_mode = 0;//tx output

	cur_hdr_policy = get_hdr_policy();

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	int dv_format = 0;
	int dv_mode = 0;
	int dv_policy = 0;

	if (is_amdv_enable()) {
		/* sync hdr_policy with dolby_vision_policy */
		/* get current dolby_vision_mode */
		dv_policy = get_amdv_policy();
		dv_mode = get_amdv_target_mode();
		dv_format = get_amdv_src_format(vd_path, vf);
		dv_hdr_policy = get_amdv_hdr_policy(vf);
		pr_csc(32, "policy: dv_policy/_mode/_format/_hdr_policy/_on=%d/%d/%d/%d/%d\n",
			dv_policy, dv_mode, dv_format, dv_hdr_policy, is_amdv_on());
	}
#endif

	pr_csc(32, "vd2 top1: hdr_policy/src_format/output_mode/support/status=0x%x/%x/%x/%x/%x\n",
		cur_hdr_policy, source_format[vd_path], output_mode,
		sink_hdr_support(vinfo), get_hdr_module_status(vd_path, vpp_index));

	if (get_hdr_module_status(vd_path, vpp_index) != HDR_MODULE_ON && cur_hdr_policy != 2) {
		/* hdr module off or bypass */
		sdr_process_mode[vd_path] = PROC_BYPASS;
		hdr_process_mode[vd_path] = PROC_BYPASS;
		hlg_process_mode[vd_path] = PROC_BYPASS;
		hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
		hlg_plus_process_mode[vd_path] = PROC_BYPASS;
		cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
		cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
		target_format[vd_path] = BT709;
	} else if (cur_hdr_policy == 0) {
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
		} else if (is_amdv_enable() &&
		   !is_amdv_on() &&
		   ((get_dv_support_info() & 7) == 7) &&
		   (source_format[vd_path] == HDRTYPE_DOVI ||
			((source_format[vd_path] == HDRTYPE_HDR10 ||
			source_format[vd_path] == HDRTYPE_HDR10_709) &&
			 (dv_hdr_policy & 1)) ||
			(source_format[vd_path] == HDRTYPE_HLG &&
			 (dv_hdr_policy & 2)) ||
			(source_format[vd_path] == HDRTYPE_SDR &&
			 (dv_hdr_policy & 0x20)))) {
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			amdv_set_toggle_flag(1);
		} else if (source_format[vd_path] == HDRTYPE_HLG &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_HLG;
		} else if (source_format[vd_path] == HDRTYPE_HDR10PLUS &&
			hdr10_plus_support) {
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_PQ_DYNAMIC;
		} else if (source_format[vd_path] == HDRTYPE_HLGPLUS &&
			hlg_plus_support) {
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_PQ_DYNAMIC;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			cuva_support) {
			/* vd2 bypass cuva */
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			(sink_hdr_support(vinfo) & HDR_SUPPORT)) {
			/* cuva_hdr to hdr > cuva_hdr to hlg */
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
			target_format[vd_path] = BT2020_HLG;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			cuva_support) {
			/* TODO: check the cuva_hlg policy */
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
			target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			(sink_hdr_support(vinfo) & HDR_SUPPORT)) {
			/* TODO: check the cuva_hlg policy */
			/* cuva_hlg to hdr > cuva_hlg to hlg */
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else if (source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
			target_format[vd_path] = BT2020_HLG;
		} else if (is_amdv_on() && is_amdv_stb_mode()) {
			hdr_process_mode[vd_path] = PROC_MATCH;
			hlg_process_mode[vd_path] = PROC_MATCH;
			sdr_process_mode[vd_path] = PROC_MATCH;
			hdr10_plus_process_mode[vd_path] = PROC_MATCH;
			hlg_plus_process_mode[vd_path] = PROC_MATCH;
			cuva_hdr_process_mode[vd_path] = PROC_MATCH;
			cuva_hlg_process_mode[vd_path] = PROC_MATCH;
			target_format[vd_path] = BT2100_IPT;
		} else if ((sink_hdr_support(vinfo) & HDR_SUPPORT) &&
			    (((source_format[vd_path] != HDRTYPE_HLG) &&
			    (source_format[vd_path] != HDRTYPE_HLGPLUS)) ||
			    (hdr_flag & 0x10))) {
			/* *->hdr */
			sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
			hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else {
			/* *->sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			if (source_format[vd_path] == HDRTYPE_SDR &&
			    get_hdr_module_status(vd_path, vpp_index) == HDR_MODULE_ON &&
			    gamut_conv_enable)
				sdr_process_mode[vd_path] = PROC_SDR_TO_TRG;
			if (ai_color_enable)
				sdr_process_mode[vd_path] = PROC_SDR_AC_SDR;
			hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
			hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
			hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
			if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
				(source_format[vd_path] == HDRTYPE_HLG ||
				 (source_format[vd_path] == HDRTYPE_HDR10 ||
				 source_format[vd_path] == HDRTYPE_HDR10_709))) {
				if (source_format[vd_path] == HDRTYPE_HDR10)
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709_HDR;
			} else {
				target_format[vd_path] = BT709;
			}
#else
			if (source_format[vd_path] == HDRTYPE_SDR2020 &&
				!output_mode)
				target_format[vd_path] = BT2020;
			else
				target_format[vd_path] = BT709;
#endif
		}
	} else if (cur_hdr_policy == 1) {
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
		} else if (is_amdv_enable() &&
		   !is_amdv_on() &&
		   ((get_dv_support_info() & 7) == 7) &&
		   ((source_format[vd_path] == HDRTYPE_DOVI) ||
			(((source_format[vd_path] == HDRTYPE_HDR10) ||
			(source_format[vd_path] == HDRTYPE_HDR10_709)) &&
			 (dv_hdr_policy & 1)) ||
			((source_format[vd_path] == HDRTYPE_HLG) &&
			 (dv_hdr_policy & 2)))) {
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			amdv_set_toggle_flag(1);
		} else if (vd_path == VD2_PATH &&
			   is_amdv_on() &&
			   is_amdv_stb_mode()) {
			/* VD2 with VD1 in DV mode */
			hdr_process_mode[vd_path] = PROC_MATCH;
			hlg_process_mode[vd_path] = PROC_MATCH;
			sdr_process_mode[vd_path] = PROC_MATCH; /* *->ipt */
			cuva_hdr_process_mode[vd_path] = PROC_MATCH;
			cuva_hlg_process_mode[vd_path] = PROC_MATCH;
			target_format[vd_path] = BT2100_IPT;
		} else {
			switch (source_format[vd_path]) {
			case HDRTYPE_SDR:
			case HDRTYPE_SDR2020:
				/* sdr->sdr */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				if (ai_color_enable)
					sdr_process_mode[vd_path] = PROC_SDR_AC_SDR;
				if (source_format[vd_path] == HDRTYPE_SDR2020 &&
					!output_mode)
					target_format[vd_path] = BT2020;
				else
					target_format[vd_path] = BT709;
				break;
			case HDRTYPE_HLG:
				/* source HLG */
				if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hlg->hlg */
					hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_HLG;
				} else if ((sink_hdr_support(vinfo) & HDR_SUPPORT) &&
					   (hdr_flag & 0x10)) {
					/* hlg->hdr */
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (cuva_support) {
					/* hlg->cuva */
					hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hlg->sdr */
					hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if (sink_hdr_support(vinfo) & BT2020_SUPPORT)
						target_format[vd_path] = BT2020_PQ;
					else
						target_format[vd_path] = BT709;
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10:
			case HDRTYPE_HDR10_709:
				/* source HDR10 */
				if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr bypass */
					hdr_process_mode[vd_path] = PROC_BYPASS;
					if (source_format[vd_path] == HDRTYPE_HDR10)
						target_format[vd_path] = BT2020_PQ;
					else
						target_format[vd_path] = BT709_HDR;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hdr->hlg */
					hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (cuva_support) {
					/* cuva bypass */
					hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hdr ->sdr */
					hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if (sink_hdr_support(vinfo) & BT2020_SUPPORT)
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10PLUS:
				/* source HDR10+ */
				if (hdr10_plus_support) {
					/* hdr+ bypass */
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_PQ_DYNAMIC;
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr+->hdr */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hdr+->hlg */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (cuva_support) {
					/* cuva bypass */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hdr+ *->sdr */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if (sink_hdr_support(vinfo) & BT2020_SUPPORT)
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HLGPLUS:
				/* source HLG10+ */
				if (hlg_plus_support) {
					/* hlg+ bypass */
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_HLG_DYNAMIC;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hlg+ ->hlg */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr+->hdr */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (cuva_support) {
					/* hlg+ -> cuva */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hlg+ ->sdr */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if (sink_hdr_support(vinfo) & BT2020_SUPPORT)
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_CUVA_HDR:
				/* source cuva */
				if (cuva_support) {
					/* bypass */
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* cuva->hdr */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* cuva->hlg */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else {
					/* cuva *->sdr */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
					target_format[vd_path] = BT709;
				}
				break;
			case HDRTYPE_CUVA_HLG:
				/* source CUVA_HLG */
				/* TODO: confirm the policy cuva_hlg->cuva */
				if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* cuva_hlg->hlg */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if ((sink_hdr_support(vinfo)	& HDR_SUPPORT) &&
					(hdr_flag & 0x10)) {
					/* cuva_hlg->hdr */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else {
					/* cuva_hlg->sdr */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
					    !is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020_PQ;
					else
						target_format[vd_path] = BT709;
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			default:
				break;
			}
		}
	} else if (cur_hdr_policy == 2 &&
		   !is_amdv_enable()) {
		/* dv off, and policy == debug */
		/* *->force_output */
		if (vd_path == VD2_PATH) {
			target_format[vd_path] = get_force_output();
			switch (target_format[vd_path]) {
			case BT709:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT709_HDR:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT2020_PQ:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_PQ_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_HLG:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2020_HLG_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2100_IPT:
				/* hdr module not handle dv output */
				break;
			case BT_BYPASS:
				/* force bypass all process */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
				break;
			case BT2020YUV_BT2020RGB_CUVA:
				sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
				hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
				hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				break;
			default:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			}
		}
	}
}

int hdr_policy_process(struct vinfo_s *vinfo,
		       enum hdr_type_e *source_format,
		       enum vd_path_e vd_path,
		       enum vpp_index_e vpp_index,
		       struct vframe_s *vf)
{
	int change_flag = 0;
	enum vd_path_e oth_path =
		(vd_path == VD1_PATH) ? VD2_PATH : VD1_PATH;
	int cur_hdr_policy = 0;
	int dv_policy = 0;
	int dv_hdr_policy = 0;
	int dv_mode = 0;
	int dv_format = 0;
	bool hdr10_plus_support =
		sink_hdr_support(vinfo) & HDRP_SUPPORT;
	bool hlg_plus_support =
		sink_hdr_support(vinfo) & HLGP_SUPPORT;
	bool cuva_support =
		sink_hdr_support(vinfo) & CUVA_SUPPORT;
	bool output_mode = 0;
	unsigned int ext_signal_type = 0;
	enum vpp_matrix_ext_csc_e ext_csc_type = VPP_MATRIX_EXT_NULL;

	if (vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
		vinfo->mode == VMODE_DUMMY_ENCP)
		output_mode = 1;//panel
	else
		output_mode = 0;//tx output

	if (vd_path == VD1_PATH &&
		chip_cls_id == TV_CHIP &&
		hdr_policy_bak != 0x0 &&
		hdr_policy != hdr_policy_bak) {
		pr_csc(128, "%s: hdr_policy=%d->%d, hdr_policy_bak=%d->0xff\n",
			__func__, hdr_policy, hdr_policy_bak, hdr_policy_bak);
		hdr_policy = hdr_policy_bak;
		hdr_policy_bak = 0x0;
	}

	pr_csc(32, "policy: vd_path=%d is vpp(%d %d) vpp_index/output_mode/hdr_policy=%d/%d/%d\n",
		vd_path,
		is_vpp0(VD1_PATH),
		is_vpp1(VD2_PATH),
		vpp_index, output_mode, hdr_policy);

	cur_hdr_policy = get_hdr_policy();

	if (is_vpp1(VD2_PATH) &&
		vd_path == VD2_PATH &&
		vpp_index == VPP_TOP1) {
		vd2_map_top1_policy_process(vinfo, source_format, vd_path, vpp_index, vf);
		goto out;
	} else if (is_vpp1(VD2_PATH) &&
		vd_path == VD2_PATH &&
		vpp_index != VPP_TOP1) {
		pr_info("policy: error!!! expect amvecm_matrix_process had returned\n");
	}

	tx_hdr10_plus_support = hdr10_plus_support;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_enable()) {
		/* sync hdr_policy with dolby_vision_policy */
		/* get current dolby_vision_mode */
		dv_policy = get_amdv_policy();
		dv_mode = get_amdv_target_mode();
		dv_format = get_amdv_src_format(vd_path, vf);
		dv_hdr_policy = get_amdv_hdr_policy(vf);
		pr_csc(32, "policy: dv_policy/_mode/_format/_hdr_policy/_on=%d/%d/%d/%d/%d\n",
			dv_policy, dv_mode, dv_format, dv_hdr_policy, is_amdv_on());
	}
#endif

	ext_signal_type = get_cur_vd_ext_signal_type(vd_path);
	if (ext_signal_type) {
		ext_csc_type = get_ext_csc_type();
		pr_csc(32, "policy: ext_signal_type=0x%x, ext_csc_type=0x%x\n",
			ext_signal_type, ext_csc_type);
	}

	pr_csc(32, "policy: cur_hdr_policy/source_format/hdr_flag/support=0x%x/%x/%x/%x\n",
		cur_hdr_policy, source_format[vd_path], hdr_flag, sink_hdr_support(vinfo));

	if (get_hdr_module_status(vd_path, vpp_index) != HDR_MODULE_ON && cur_hdr_policy != 2) {
		/* hdr module off or bypass */
		sdr_process_mode[vd_path] = PROC_BYPASS;
		hdr_process_mode[vd_path] = PROC_BYPASS;
		hlg_process_mode[vd_path] = PROC_BYPASS;
		hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
		hlg_plus_process_mode[vd_path] = PROC_BYPASS;
		cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
		cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
		target_format[vd_path] = BT709;
	} else if (cur_hdr_policy == 0) {
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			if (!is_vpp1(VD2_PATH)) {
				sdr_process_mode[oth_path] = PROC_BYPASS;
				hdr_process_mode[oth_path] = PROC_BYPASS;
				hlg_process_mode[oth_path] = PROC_BYPASS;
				hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[oth_path] = PROC_BYPASS;
				cuva_hlg_process_mode[oth_path] = PROC_BYPASS;
				target_format[oth_path] = BT709;
			}
			target_format[vd_path] = BT709;
		} else if (source_format[vd_path] == HDRTYPE_PRIMESL) {
			/* primesl process */
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
			if (get_prime_sl_hdr_process_policy(vf) == PRIME_SL_HDR_MODE_2020_TO_709) {
				/* same as hdr ->sdr */
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				target_format[vd_path] = BT709;
			} else if (get_prime_sl_hdr_process_policy(vf) ==
				PRIME_SL_HDR_MODE_709_TO_2020) {
				/* same as sdr ->hdr */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else {
				/* bypass */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				if (get_prime_sl_hdr_process_policy(vf) ==
					PRIME_SL_HDR_MODE_BYPASS_2020)
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709;
			}
#endif
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
		} else if (vd_path == VD1_PATH &&
			   is_amdv_enable() &&
			   !is_amdv_on() &&
			   ((get_dv_support_info() & 7) == 7) &&
			   (source_format[vd_path] == HDRTYPE_DOVI ||
			    ((source_format[vd_path] == HDRTYPE_HDR10 ||
				source_format[vd_path] == HDRTYPE_HDR10_709) &&
			     (dv_hdr_policy & 1)) ||
			    (source_format[vd_path] == HDRTYPE_HLG &&
			     (dv_hdr_policy & 2)) ||
			    (source_format[vd_path] == HDRTYPE_SDR &&
			     (dv_hdr_policy & 0x20)))) {
			/* vd1 follow sink: dv handle sdr/hdr/hlg/dovi */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			amdv_set_toggle_flag(1);
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_HLG &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			/* vd1 bypass hlg */
			hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_HLG;
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_HLG &&
			is_amdv_enable() &&
			(sink_hdr_support_ori_cap(vinfo) & HLG_SUPPORT)) {
			/* vd1 bypass hlg */
			hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_HLG;
		} else if (vd_path == VD1_PATH &&
			(!is_video_layer_on(VD2_PATH) || is_vpp1(VD2_PATH)) &&
			source_format[vd_path] == HDRTYPE_HDR10PLUS &&
			hdr10_plus_support) {
			/* vd1 bypass hdr+ when vd2 off */
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_PQ_DYNAMIC;
		} else if (vd_path == VD1_PATH &&
			(!is_video_layer_on(VD2_PATH) || is_vpp1(VD2_PATH)) &&
			source_format[vd_path] == HDRTYPE_HLGPLUS &&
			hlg_plus_support) {
			/* vd1 bypass hlg+ when vd2 off */
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT2020_HLG_DYNAMIC;
		} else if (vd_path == VD1_PATH &&
			(!is_video_layer_on(VD2_PATH) || is_vpp1(VD2_PATH)) &&
			source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			cuva_support) {
			if (get_tx_cuva_mode(vinfo) == CUVA_MONITOR) {
				/* only support monitor, cuva_hdr to hdr */
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else {
				/* vd1 bypass cuva when vd2 off */
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
			}
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			(sink_hdr_support(vinfo) & HDR_SUPPORT)) {
			/* cuva_hdr to hdr > cuva_hdr to hlg */
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_CUVA_HDR &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
			target_format[vd_path] = BT2020_HLG;
		} else if (vd_path == VD1_PATH &&
			(!is_video_layer_on(VD2_PATH) || is_vpp1(VD2_PATH)) &&
			source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			cuva_support) {
			if (get_tx_cuva_mode(vinfo) == CUVA_MONITOR) {
				/* only support monitor, cuva_hlg to hdr */
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else {
				/* TODO: check the cuva_hlg policy */
				/* vd1 cuva_hlg to cuva when vd2 off */
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
			}
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			(sink_hdr_support(vinfo) & HDR_SUPPORT)) {
			/* TODO: check the cuva_hlg policy */
			/* cuva_hlg to hdr > cuva_hlg to hlg */
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else if (vd_path == VD1_PATH &&
			source_format[vd_path] == HDRTYPE_CUVA_HLG &&
			(sink_hdr_support(vinfo) & HLG_SUPPORT)) {
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
			target_format[vd_path] = BT2020_HLG;
		} else if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			if (!support_multi_core1()) {
				/* vd2 *->ipt when vd1 dolby on */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				hdr10_plus_process_mode[vd_path] = PROC_MATCH;
				hlg_plus_process_mode[vd_path] = PROC_MATCH;
				cuva_hdr_process_mode[vd_path] = PROC_MATCH;
				cuva_hlg_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {/*multi dv core1, processed by dv*/
				if (source_format[vd_path] == HDRTYPE_DOVI ||
				    ((source_format[vd_path] == HDRTYPE_HDR10 ||
				    source_format[vd_path] == HDRTYPE_HDR10_709) &&
				    (dv_hdr_policy & 1)) ||
				    (source_format[vd_path] == HDRTYPE_HLG &&
				    (dv_hdr_policy & 2)) ||
				    (source_format[vd_path] == HDRTYPE_SDR/* &&*/
				    /* (dv_hdr_policy & 0x20)*/)) {
					/* vd2 follow sink: dv handle sdr/hdr/hlg/dovi */
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT709;
					set_hdr_module_status(vd_path, HDR_MODULE_OFF);
					amdv_set_toggle_flag(1);
				} else {
					/* vd2 *->ipt when vd1 dolby on */
					hdr_process_mode[vd_path] = PROC_MATCH;
					hlg_process_mode[vd_path] = PROC_MATCH;
					sdr_process_mode[vd_path] = PROC_MATCH;
					hdr10_plus_process_mode[vd_path] = PROC_MATCH;
					hlg_plus_process_mode[vd_path] = PROC_MATCH;
					cuva_hdr_process_mode[vd_path] = PROC_MATCH;
					cuva_hlg_process_mode[vd_path] = PROC_MATCH;
					target_format[vd_path] = BT2100_IPT;
				}
			}
		} else if (vd_path == VD2_PATH &&
			   is_video_layer_on(VD1_PATH)) {
			/* vd1 on and vd2 follow vd1 output */
			if (target_format[VD1_PATH] == BT2020_HLG ||
				   target_format[VD1_PATH] == BT2020_HLG_DYNAMIC) {
				/* vd2 *->hlg when vd1 output hlg/hlg+ */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
				target_format[vd_path] = BT2020_HLG;
			} else if (target_format[VD1_PATH] == BT2020_PQ ||
				   target_format[VD1_PATH] == BT2020_PQ_DYNAMIC) {
				/* vd2 *->hdr when vd1 output hdr/hdr+ */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else if (target_format[VD1_PATH] == BT2020YUV_BT2020RGB_CUVA) {
				/* vd2 *->cuva when vd1 output cuva */
				sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
				hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
				hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
			} else {
				/* vd2 *->sdr when vd1 output sdr */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				if (target_format[VD1_PATH] == BT2020_PQ &&
				    source_format[vd_path] == HDRTYPE_HLG)
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709;
			}
		} else if ((sink_hdr_support(vinfo) & HDR_SUPPORT) &&
			(((source_format[vd_path] != HDRTYPE_HLG) &&
			(source_format[vd_path] != HDRTYPE_HLGPLUS)) ||
			(hdr_flag & 0x10))) {
			/* *->hdr */
			sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
			hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
			target_format[vd_path] = BT2020_PQ;
		} else if ((sink_hdr_support(vinfo) & HLG_SUPPORT) &&
			(((source_format[vd_path] != HDRTYPE_HDR10) &&
			(source_format[vd_path] != HDRTYPE_HDR10_709)) ||
			((source_format[vd_path] == HDRTYPE_HDR10 ||
			source_format[vd_path] == HDRTYPE_HDR10_709) &&
			(hdr_flag & 0x10)))) {
			/* *->hlg */
			sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
			hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
			hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
			target_format[vd_path] = BT2020_HLG;
		} else {
			/* *->sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			if (source_format[vd_path] == HDRTYPE_SDR &&
			    get_hdr_module_status(vd_path, vpp_index) == HDR_MODULE_ON &&
			    (gamut_conv_enable &&
			    (vpp_color_pri_sel || !gamut_mapping1_en)))
				sdr_process_mode[vd_path] = PROC_SDR_TO_TRG;
			if (ai_color_enable)
				sdr_process_mode[vd_path] = PROC_SDR_AC_SDR;
			hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
			hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
			hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
			if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
			    (source_format[vd_path] == HDRTYPE_HLG ||
			     (source_format[vd_path] == HDRTYPE_HDR10 ||
			     source_format[vd_path] == HDRTYPE_HDR10_709)) &&
			    !is_video_layer_on(oth_path)) {
				if (source_format[vd_path] == HDRTYPE_HDR10)
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709_HDR;
			} else {
				target_format[vd_path] = BT709;
			}
#else
			if (source_format[vd_path] == HDRTYPE_SDR2020 &&
				!output_mode)
				target_format[vd_path] = BT2020;
			else
				target_format[vd_path] = BT709;
#endif
		}
	} else if (cur_hdr_policy == 1) {
		pr_csc(2, "am_vecm: vd%d source_format %d, is_amdv_on %d\n",
			       vd_path + 1,
			       source_format[vd_path], is_amdv_on());
		if (source_format[vd_path] == HDRTYPE_MVC) {
			/* hdr bypass output need sdr */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			if (!is_vpp1(VD2_PATH)) {
				sdr_process_mode[oth_path] = PROC_BYPASS;
				hdr_process_mode[oth_path] = PROC_BYPASS;
				hlg_process_mode[oth_path] = PROC_BYPASS;
				hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[oth_path] = PROC_BYPASS;
				cuva_hlg_process_mode[oth_path] = PROC_BYPASS;
				target_format[oth_path] = BT709;
			}
			target_format[vd_path] = BT709;
		} else if (source_format[vd_path] == HDRTYPE_PRIMESL) {
			/* primesl process */
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
			if (get_prime_sl_hdr_process_policy(vf) == PRIME_SL_HDR_MODE_2020_TO_709) {
				/* same as hdr ->sdr */
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				target_format[vd_path] = BT709;
			} else if (get_prime_sl_hdr_process_policy(vf) ==
				PRIME_SL_HDR_MODE_709_TO_2020) {
				/* same as sdr ->hdr */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else {
				/* bypass */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				if (get_prime_sl_hdr_process_policy(vf) ==
					PRIME_SL_HDR_MODE_BYPASS_2020)
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709;
			}
#endif
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
		} else if (vd_path == VD1_PATH &&
			   is_amdv_enable() &&
			   !is_amdv_on() &&
			   ((get_dv_support_info() & 7) == 7) &&
			   ((source_format[vd_path] == HDRTYPE_DOVI) ||
			    (((source_format[vd_path] == HDRTYPE_HDR10) ||
			    (source_format[vd_path] == HDRTYPE_HDR10_709)) &&
			     (dv_hdr_policy & 1)) ||
			    ((source_format[vd_path] == HDRTYPE_HLG) &&
			     (dv_hdr_policy & 2)))) {
			/* vd1 follow source: dv handle dovi */
			/* dv handle hdr/hlg according to policy */
			sdr_process_mode[vd_path] = PROC_BYPASS;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hlg_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
			hlg_plus_process_mode[vd_path] = PROC_BYPASS;
			cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
			cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
			target_format[vd_path] = BT709;
			set_hdr_module_status(vd_path, HDR_MODULE_OFF);
			amdv_set_toggle_flag(1);
		} else if (vd_path == VD2_PATH &&
			   is_amdv_on() &&
			   /*!is_amdv_on() &&*/
			   is_amdv_stb_mode()) {
			if (!support_multi_core1()) {
				/* VD2 with VD1 in DV mode */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				cuva_hdr_process_mode[vd_path] = PROC_MATCH;
				cuva_hlg_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {/*multi dv core1*/
				/* vd1 follow source: dv handle dovi */
				/* dv handle hdr/hlg according to policy */
				if (source_format[vd_path] == HDRTYPE_DOVI ||
				    ((source_format[vd_path] == HDRTYPE_HDR10 ||
				    source_format[vd_path] == HDRTYPE_HDR10_709) &&
				    (dv_hdr_policy & 1)) ||
				    (source_format[vd_path] == HDRTYPE_HLG &&
				    (dv_hdr_policy & 2))) {
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT709;
					set_hdr_module_status(vd_path, HDR_MODULE_OFF);
					amdv_set_toggle_flag(1);
				} else {
					/* VD2 with VD1 in DV mode */
					hdr_process_mode[vd_path] = PROC_MATCH;
					hlg_process_mode[vd_path] = PROC_MATCH;
					sdr_process_mode[vd_path] = PROC_MATCH;
					cuva_hdr_process_mode[vd_path] = PROC_MATCH;
					cuva_hlg_process_mode[vd_path] = PROC_MATCH;
					target_format[vd_path] = BT2100_IPT;
				}
			}
		} else if (vd_path == VD2_PATH &&
					is_video_layer_on(VD1_PATH)) {
			/* vd1 on and vd2 follow vd1 output */
			if (target_format[VD1_PATH] == BT2020_HLG ||
				target_format[VD1_PATH] == BT2020_HLG_DYNAMIC) {
				/* vd2 *->hlg when vd1 output hlg/hlg+ */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
				target_format[vd_path] = BT2020_HLG;
			} else if (target_format[VD1_PATH] == BT2020_PQ ||
				   target_format[VD1_PATH] == BT2020_PQ_DYNAMIC) {
				/* vd2 *->hdr when vd1 output hdr/hdr+ */
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				target_format[vd_path] = BT2020_PQ;
			} else if (target_format[VD1_PATH] == BT2020YUV_BT2020RGB_CUVA) {
				/* vd2 *->cuva when vd1 output cuva */
				sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
				hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
				hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
			} else {
				pr_csc(32, "%d %s: vd_path=%d  vpp_index = %d\n",
					__LINE__,
					__func__,
					vd_path,
					vpp_index);
				/* vd2 *->sdr when vd1 output sdr */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				if (target_format[VD1_PATH] == BT2020 &&
				    source_format[vd_path] == HDRTYPE_HLG)
					target_format[vd_path] = BT2020;
				else
					target_format[vd_path] = BT709;
			}
		} else if (vd_path == VD1_PATH ||
			   (vd_path == VD2_PATH &&
			    !is_video_layer_on(VD1_PATH))) {
			/* VD1(with/without VD2) */
			/* or VD2(without VD1) <= should switch to VD1 */
			switch (source_format[vd_path]) {
			case HDRTYPE_SDR:
			case HDRTYPE_SDR2020:
				if (is_video_layer_on(oth_path)) {
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG) {
						/* other layer output hlg */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA && cuva_support) {
						/* sdr *->cuva */
						sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] = PROC_BYPASS;
						if (ai_color_enable)
							sdr_process_mode[vd_path] = PROC_SDR_AC_SDR;
						target_format[vd_path] = BT709;
					}
				} else {
					/* sdr->sdr */
					sdr_process_mode[vd_path] = PROC_BYPASS;
					if (ai_color_enable)
						sdr_process_mode[vd_path] = PROC_SDR_AC_SDR;
					if (source_format[vd_path] == HDRTYPE_SDR2020 &&
						!output_mode)
						target_format[vd_path] = BT2020;
					else
						target_format[vd_path] = BT709;
				}
				break;
			case HDRTYPE_HLG:
				/* source HLG */
				if (is_video_layer_on(oth_path) &&
				    (target_format[oth_path] == BT2020_PQ ||
				     target_format[oth_path] == BT2020_PQ_DYNAMIC)) {
					/* hlg->hdr */
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hlg->hlg */
					hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_HLG;
				} else if ((sink_hdr_support(vinfo) & HDR_SUPPORT) &&
					   (hdr_flag & 0x10)) {
					/* hlg->hdr */
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (cuva_support) {
					/* hlg->cuva */
					hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hlg->sdr */
					hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
					    !is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020_PQ;
					else
						target_format[vd_path] = BT709;
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10:
			case HDRTYPE_HDR10_709:
				/* source HDR10 */
				if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr bypass */
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
					if (source_format[vd_path] == HDRTYPE_HDR10)
						target_format[vd_path] = BT2020_PQ;
					else
						target_format[vd_path] = BT709_HDR;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hdr->hlg */
					hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (cuva_support) {
					/* cuva bypass */
					hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hdr ->sdr */
					hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
					    !is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HDR10PLUS:
				/* source HDR10+ */
				if (hdr10_plus_support && !is_video_layer_on(oth_path)) {
					/* hdr+ bypass */
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_PQ_DYNAMIC;
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr+->hdr */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hdr+->hlg */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (cuva_support) {
					/* cuva bypass */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hdr+ *->sdr */
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
					    !is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_HLGPLUS:
				/* source HDR10+ */
				if (hlg_plus_support && !is_video_layer_on(oth_path)) {
					/* hlg+ bypass */
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020_HLG_DYNAMIC;
				} else if (hdr10_plus_support) {
					/* hlg+ -> hdr	  if need hlg+ -> hdr+? */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					target_format[vd_path] = BT2020_PQ_DYNAMIC;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* hdr+->hlg */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT) {
					/* hdr+->hdr */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (cuva_support) {
					/* cuva bypass */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* hdr+ *->sdr */
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
						!is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020_PQ;
					else
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			case HDRTYPE_CUVA_HDR:
				/* source cuva */
				if (cuva_support && !is_video_layer_on(oth_path)) {
					if (get_cuva_mode() == CUVA_MONITOR ||
					(get_cuva_mode() == CUVA_AUTO &&
					(get_tx_cuva_mode(vinfo) == CUVA_MONITOR)) ||
					(get_cuva_mode() == CUVA_SPECIAL_NO &&
					(get_force_output() == BT2020_PQ))) {
					/* monitor, cuva->hdr*/
					/* auto tx only monitor, cuva->hdr*/
					/* hdr10 mode(not special), cuva->hdr*/
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else {
					/* receiver, bypass */
					/* auto receiver, bypass */
					/* not force output, bypass */
						cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					}
				} else if (sink_hdr_support(vinfo) & HDR_SUPPORT ||
					(get_force_output() == BT2020_PQ)) {
					/* cuva->hdr */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* cuva->hlg */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else if (cuva_support) {
					/* cuva bypass */
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
				} else {
					/* cuva *->sdr */
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
					target_format[vd_path] = BT709;
				}
				break;
			case HDRTYPE_CUVA_HLG:
				/* source CUVA_HLG */
				/* TODO: confirm the policy cuva_hlg->cuva */
				if (is_video_layer_on(oth_path) &&
					(target_format[oth_path] == BT2020_PQ ||
					 target_format[oth_path] == BT2020_PQ_DYNAMIC)) {
					/* cuva_hlg->hdr */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (cuva_support) {
					if (get_cuva_mode() == CUVA_MONITOR ||
					(get_cuva_mode() == CUVA_AUTO &&
					(get_tx_cuva_mode(vinfo) == CUVA_MONITOR)) ||
					(get_cuva_mode() == CUVA_SPECIAL_NO &&
					(get_force_output() == BT2020_PQ))) {
						/* monitor, cuva->hdr*/
						/* auto tx only monitor, cuva->hdr*/
						/* hdr10 mode(not special), cuva->hdr*/
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else {
						/* receiver, bypass */
						/* auto tx receiver, bypass */
						/* not force output, bypass */
						/* cuva_hlg->cuva same as bypass */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					}
				} else if ((sink_hdr_support(vinfo)	& HDR_SUPPORT) &&
					(hdr_flag & 0x10)) {
					/* cuva_hlg->hdr */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					target_format[vd_path] = BT2020_PQ;
				} else if (sink_hdr_support(vinfo) & HLG_SUPPORT) {
					/* cuva_hlg->hlg */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
					target_format[vd_path] = BT2020_HLG;
				} else {
					/* cuva_hlg->sdr */
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
#ifdef AMCSC_DEBUG_TEST
					if ((sink_hdr_support(vinfo) & BT2020_SUPPORT) &&
					    !is_video_layer_on(oth_path))
						target_format[vd_path] = BT2020;
					else
						target_format[vd_path] = BT709;
#else
					target_format[vd_path] = BT709;
#endif
				}
				break;
			default:
				break;
			}
		} else {
			/* VD2 with VD1 */
			if (is_amdv_on() &&
			    (vd_path == VD1_PATH || is_amdv_stb_mode())) {
				/* VD1 is dolby vision */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else if (!is_vpp1(VD2_PATH)) {
				oth_path = VD1_PATH;
				switch (source_format[vd_path]) {
				case HDRTYPE_SDR:
				case HDRTYPE_SDR2020:
					/* VD2 source SDR */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* other layer on and not sdr */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* other layer on and not sdr */
						/* sdr *->cuva */
						sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] = PROC_BYPASS;
						if (source_format[vd_path] == HDRTYPE_SDR2020 &&
							!output_mode)
							target_format[vd_path] = BT2020;
						else
							target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLG:
					/* VD2 source HLG */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg->hlg */
						hlg_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
						   target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg->hdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg->cuva */
						hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT709) {
						/* hlg->sdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10:
				case HDRTYPE_HDR10_709:
					/* VD2 source HDR10 */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr->cuva */
						hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10PLUS:
					/* VD2 source HDR10+ */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr->cuva */
						hdr10_plus_process_mode[vd_path] =
						PROC_HDRP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLGPLUS:
					/* VD2 source HLG10+ */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg+ -> hlg */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg+ -> hdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg+ -> cuva */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hlg+ -> sdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				default:
					break;
				}
			}
		}
	} else if (cur_hdr_policy == 2 &&
		   !is_amdv_enable()) {
		/* dv off, and policy == debug */
		/* *->force_output */
		if (vd_path == VD1_PATH ||
		    ((vd_path == VD2_PATH || vd_path == VD3_PATH) &&
		     !is_video_layer_on(VD1_PATH))) {
			/* VD1 or VD2 without VD1 */
			target_format[vd_path] = get_force_output();
			switch (target_format[vd_path]) {
			case BT709:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT709_HDR:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020:
				sdr_process_mode[vd_path] =
					PROC_BYPASS;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] =
					PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] =
					PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT2020_PQ:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_PQ_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_HLG:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2020_HLG_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2100_IPT:
				/* hdr module not handle dv output */
				break;
			case BT_BYPASS:
				/* force bypass all process */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
				break;
			case BT2020YUV_BT2020RGB_CUVA:
				sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
				hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
				hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_CUVA;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				break;
			default:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			}
		} else {
			/* VD2 with VD1 on */
			if (is_amdv_on() &&
			    (vd_path == VD1_PATH || is_amdv_stb_mode())) {
				/* VD1 is dolby vision */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else if (!is_vpp1(VD2_PATH)) {
				oth_path = VD1_PATH;
				switch (source_format[vd_path]) {
				case HDRTYPE_SDR:
				case HDRTYPE_SDR2020:
					/* VD2 source SDR */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
						target_format[vd_path] = target_format[oth_path];
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* other layer on and not sdr */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* other layer on and not sdr */
						/* sdr *->cuva */
						sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] = PROC_BYPASS;
						if (source_format[vd_path] == HDRTYPE_SDR2020 &&
							!output_mode)
							target_format[vd_path] = BT2020;
						else
							target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLG:
					/* VD2 source HLG */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg->hlg */
						hlg_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
						   target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg->hdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg->cuva */
						hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT709) {
						/* hlg->sdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10:
				case HDRTYPE_HDR10_709:
					/* VD2 source HDR10 */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* cuva_hdr bypass*/
						hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10PLUS:
					/* VD2 source HDR10+ */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr->cuva */
						hdr10_plus_process_mode[vd_path] =
							PROC_HDRP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLGPLUS:
					/* VD2 source HLG10+ */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg+ -> hlg */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg+ -> hdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg+ -> cuva */
						hlg_plus_process_mode[vd_path] =
							PROC_HLGP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hlg+ -> sdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_CUVA_HDR:
					/* VD2 source cuva */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->cuva */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr10->cuva */
						cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* cuva->hlg */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else {
						/* cuva->sdr */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_CUVA_HLG:
					/* VD2 source cuva_hlg */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdrhlg->hdr */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* cuva_hlg->cuva */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* cuva hlg->hlg */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else {
						/* cuva->sdr */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				default:
					break;
				}
			}
		}
	} else if (cur_hdr_policy == 2 &&
		is_amdv_enable()) {
		pr_csc(1, "vd%d policy 2 && dv_enable, sbtm(%d), src_fmt:%d\n",
			vd_path, is_sbtm_on(), source_format[vd_path]);
		if (is_sbtm_on() && source_format[vd_path] != HDRTYPE_DOVI) {
			sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
			hdr_process_mode[vd_path] = PROC_BYPASS;
			hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
			cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
			cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
			//set_hdr_module_status(vd_path, HDR_MODULE_ON);
			target_format[vd_path] = BT2020_PQ;
			amdv_set_toggle_flag(0);
			pr_csc(1, "sbtm mode use hdr core\n");
		} else if (!is_sbtm_on() && cur_sbtm_mode[vd_path] && !is_amdv_on()) {
			amdv_set_toggle_flag(1);
			pr_csc(1, "sbtm mode off use dv core\n");
		} else if (vd_path == VD1_PATH && !is_amdv_on()) {
			if (source_format[vd_path] == HDRTYPE_DOVI ||
			    ((source_format[vd_path] == HDRTYPE_HDR10 ||
			    source_format[vd_path] == HDRTYPE_HDR10_709) &&
			    (dv_hdr_policy & 1)) ||
			    (source_format[vd_path] == HDRTYPE_HLG &&
			    (dv_hdr_policy & 2)) ||
			    (source_format[vd_path] == HDRTYPE_SDR &&
			    (dv_hdr_policy & 0x60))) {
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
				target_format[vd_path] = BT709;
				set_hdr_module_status(vd_path, HDR_MODULE_OFF);
				amdv_set_toggle_flag(1);
			} else {
				target_format[vd_path] = get_force_output();
				switch (target_format[vd_path]) {
				case BT709:
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
					hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
					break;
				case BT709_HDR:
					sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					break;
				case BT2020:
					sdr_process_mode[vd_path] =
						PROC_BYPASS;
					hlg_process_mode[vd_path] =
						PROC_HLG_TO_SDR;
					hdr_process_mode[vd_path] =
						PROC_HDR_TO_SDR;
					hdr10_plus_process_mode[vd_path] =
						PROC_HDRP_TO_SDR;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
					break;
				case BT2020_PQ:
					sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					break;
				case BT2020_PQ_DYNAMIC:
					sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
					hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
					break;
				case BT2020_HLG:
					sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
					hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
					break;
				case BT2020_HLG_DYNAMIC:
					sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
					break;
				case BT2100_IPT:
					/* hdr module not handle dv output */
					break;
				case BT_BYPASS:
					/* force bypass all process */
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
					break;
				case BT2020YUV_BT2020RGB_CUVA:
					sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
					hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
					hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
					hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
					break;
				default:
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
					hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
					hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
					hlg_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
					cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
					cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
					break;
				}
			}
		} else if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			if (!support_multi_core1()) {
				/* vd2 *->ipt when vd1 dolby on */
				pr_csc(1, "vd2 *->ipt when vd1 dolby on\n");
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				hdr10_plus_process_mode[vd_path] = PROC_MATCH;
				hlg_plus_process_mode[vd_path] = PROC_MATCH;
				cuva_hdr_process_mode[vd_path] = PROC_MATCH;
				cuva_hlg_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {/*multi dv core1, processed by dv*/
				if (source_format[vd_path] == HDRTYPE_DOVI ||
				    ((source_format[vd_path] == HDRTYPE_HDR10 ||
				    source_format[vd_path] == HDRTYPE_HDR10_709) &&
				    (dv_hdr_policy & 1)) ||
				    (source_format[vd_path] == HDRTYPE_HLG &&
				    (dv_hdr_policy & 2)) ||
				    (source_format[vd_path] == HDRTYPE_SDR/* &&*/
				    /* (dv_hdr_policy & 0x20)*/)) {
					/* vd2 follow sink: dv handle sdr/hdr/hlg/dovi */
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT709;
					set_hdr_module_status(vd_path, HDR_MODULE_OFF);
					amdv_set_toggle_flag(1);
				} else {
					/* vd2 *->ipt when vd1 dolby on */
					hdr_process_mode[vd_path] = PROC_MATCH;
					hlg_process_mode[vd_path] = PROC_MATCH;
					sdr_process_mode[vd_path] = PROC_MATCH;
					hdr10_plus_process_mode[vd_path] = PROC_MATCH;
					hlg_plus_process_mode[vd_path] = PROC_MATCH;
					cuva_hdr_process_mode[vd_path] = PROC_MATCH;
					cuva_hlg_process_mode[vd_path] = PROC_MATCH;
					target_format[vd_path] = BT2100_IPT;
				}
			}
		}
	} else if (cur_hdr_policy == 4 &&
		!is_amdv_enable()) {
		/* dv off, *->force_output for android u */
		/* support hlg, hdr10 and sdr at present. */
		if (vd_path == VD1_PATH ||
		    ((vd_path == VD2_PATH || vd_path == VD3_PATH) &&
		     !is_video_layer_on(VD1_PATH))) {
			/* VD1 or VD2 without VD1 */
			target_format[vd_path] = get_force_output();
			if (target_format[vd_path] == BT2020_PQ) {
				if (sink_hdr_support(vinfo) & HDR_SUPPORT)
					target_format[vd_path] = BT2020_PQ;
				else if ((sink_hdr_support(vinfo) & HLG_SUPPORT))
					target_format[vd_path] = BT2020_HLG;
				else
					target_format[vd_path] = BT709;
			}
			if (target_format[vd_path] == BT2020_HLG) {
				if (sink_hdr_support(vinfo) & HLG_SUPPORT)
					target_format[vd_path] = BT2020_HLG;
				else if ((sink_hdr_support(vinfo) & HDR_SUPPORT))
					target_format[vd_path] = BT2020_PQ;
				else
					target_format[vd_path] = BT709;
			}
			/* ui set force hdr10/hlg, output hdr10+ if tv supports hdr10+ */
			if (source_format[vd_path] == HDRTYPE_HDR10PLUS &&
				sink_hdr_support(vinfo) & HDRP_SUPPORT)
				target_format[vd_path] = BT2020_PQ_DYNAMIC;
			if (source_format[vd_path] == HDRTYPE_HLGPLUS &&
				sink_hdr_support(vinfo) & HLGP_SUPPORT)
				target_format[vd_path] = BT2020_HLG_DYNAMIC;
			switch (target_format[vd_path]) {
			case BT709:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT709_HDR:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020:
				sdr_process_mode[vd_path] =
					PROC_BYPASS;
				hlg_process_mode[vd_path] =
					PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] =
					PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] =
					PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			case BT2020_PQ:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_PQ_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
				hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HDR;
				break;
			case BT2020_HLG:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2020_HLG_DYNAMIC:
				sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_HLG;
				break;
			case BT2100_IPT:
				/* hdr module not handle dv output */
				break;
			case BT_BYPASS:
				/* force bypass all process */
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
				break;
			case BT2020YUV_BT2020RGB_CUVA:
				sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
				hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
				hdr_process_mode[vd_path] = PROC_HDR_TO_CUVA;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_CUVA;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_CUVA;
				break;
			default:
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
				hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
				hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
				cuva_hlg_process_mode[vd_path] = PROC_CUVAHLG_TO_SDR;
				break;
			}
		} else {
			/* VD2 with VD1 on, VD2 follow VD1 */
			if (!is_vpp1(VD2_PATH)) {
				oth_path = VD1_PATH;
				switch (source_format[vd_path]) {
				case HDRTYPE_SDR:
					/* VD2 source SDR */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* other layer output HDR */
						/* sdr *->hdr */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HDR;
						target_format[vd_path] = target_format[oth_path];
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* other layer on and not sdr */
						/* sdr *->hlg */
						sdr_process_mode[vd_path] = PROC_SDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* other layer on and not sdr */
						/* sdr *->cuva */
						sdr_process_mode[vd_path] = PROC_SDR_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* sdr->sdr */
						sdr_process_mode[vd_path] = PROC_BYPASS;
						if (target_format[oth_path] == BT2020)
							target_format[vd_path] = BT2020;
						else
							target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLG:
					/* VD2 source HLG */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg->hlg */
						hlg_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
						   target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg->hdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg->cuva */
						hlg_process_mode[vd_path] = PROC_HLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT709) {
						/* hlg->sdr */
						hlg_process_mode[vd_path] = PROC_HLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10:
				case HDRTYPE_HDR10_709:
					/* VD2 source HDR10 */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr_process_mode[vd_path] = PROC_HDR_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* cuva_hdr bypass*/
						hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr_process_mode[vd_path] = PROC_HDR_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HDR10PLUS:
					/* VD2 source HDR10+ */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->hdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr->cuva */
						hdr10_plus_process_mode[vd_path] =
							PROC_HDRP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hdr->sdr */
						hdr10_plus_process_mode[vd_path] = PROC_HDRP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_HLGPLUS:
					/* VD2 source HLG+ */
					if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hlg+->hlg */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else if (target_format[oth_path] == BT2020_PQ ||
						target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hlg+->hdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hlg+->cuva */
						hlg_plus_process_mode[vd_path] =
							PROC_HLGP_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else {
						/* hlg+->sdr */
						hlg_plus_process_mode[vd_path] = PROC_HLGP_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_CUVA_HDR:
					/* VD2 source cuva */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdr->cuva */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* hdr10->cuva */
						cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else {
						/* cuva->sdr */
						cuva_hdr_process_mode[vd_path] = PROC_CUVA_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				case HDRTYPE_CUVA_HLG:
					/* VD2 source cuva_hlg */
					if (target_format[oth_path] == BT2020_PQ ||
					    target_format[oth_path] == BT2020_PQ_DYNAMIC) {
						/* hdrhlg->hdr */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_HDR;
						target_format[vd_path] = BT2020_PQ;
					} else if (target_format[oth_path] ==
						BT2020YUV_BT2020RGB_CUVA) {
						/* cuva_hlg->cuva */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_CUVA;
						target_format[vd_path] = BT2020YUV_BT2020RGB_CUVA;
					} else if (target_format[oth_path] == BT2020_HLG ||
						target_format[oth_path] == BT2020_HLG_DYNAMIC) {
						/* hdr->hlg */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_HLG;
						target_format[vd_path] = BT2020_HLG;
					} else {
						/* cuva->sdr */
						cuva_hlg_process_mode[vd_path] =
							PROC_CUVAHLG_TO_SDR;
						target_format[vd_path] = BT709;
					}
					break;
				default:
					break;
				}
			}
		}
	} else if (cur_hdr_policy == 4 &&
		is_amdv_enable()) {
		if (vd_path == VD1_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			if (source_format[vd_path] == HDRTYPE_DOVI ||
			    ((source_format[vd_path] == HDRTYPE_HDR10 ||
			    source_format[vd_path] == HDRTYPE_HDR10_709) &&
			    (dv_hdr_policy & 1)) ||
			    (source_format[vd_path] == HDRTYPE_HLG &&
			    (dv_hdr_policy & 2)) ||
			    (source_format[vd_path] == HDRTYPE_SDR/* &&*/
			    /* (dv_hdr_policy & 0x20)*/)) {
				sdr_process_mode[vd_path] = PROC_BYPASS;
				hdr_process_mode[vd_path] = PROC_BYPASS;
				hlg_process_mode[vd_path] = PROC_BYPASS;
				hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
				hlg_plus_process_mode[vd_path] = PROC_BYPASS;
				cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
				cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
				target_format[vd_path] = BT709;
				set_hdr_module_status(vd_path, HDR_MODULE_OFF);
				amdv_set_toggle_flag(1);
			}
		} else if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			if (!support_multi_core1()) {
				/* vd2 *->ipt when vd1 dolby on */
				hdr_process_mode[vd_path] = PROC_MATCH;
				hlg_process_mode[vd_path] = PROC_MATCH;
				sdr_process_mode[vd_path] = PROC_MATCH;
				hdr10_plus_process_mode[vd_path] = PROC_MATCH;
				hlg_plus_process_mode[vd_path] = PROC_MATCH;
				cuva_hdr_process_mode[vd_path] = PROC_MATCH;
				cuva_hlg_process_mode[vd_path] = PROC_MATCH;
				target_format[vd_path] = BT2100_IPT;
			} else {/*multi dv core1, processed by dv*/
				if (source_format[vd_path] == HDRTYPE_DOVI ||
				    ((source_format[vd_path] == HDRTYPE_HDR10 ||
				    source_format[vd_path] == HDRTYPE_HDR10_709) &&
				    (dv_hdr_policy & 1)) ||
				    (source_format[vd_path] == HDRTYPE_HLG &&
				    (dv_hdr_policy & 2)) ||
				    (source_format[vd_path] == HDRTYPE_SDR/* &&*/
				    /* (dv_hdr_policy & 0x20)*/)) {
					/* vd2 follow sink: dv handle sdr/hdr/hlg/dovi */
					sdr_process_mode[vd_path] = PROC_BYPASS;
					hdr_process_mode[vd_path] = PROC_BYPASS;
					hlg_process_mode[vd_path] = PROC_BYPASS;
					hdr10_plus_process_mode[vd_path] = PROC_BYPASS;
					hlg_plus_process_mode[vd_path] = PROC_BYPASS;
					cuva_hdr_process_mode[vd_path] = PROC_BYPASS;
					cuva_hlg_process_mode[vd_path] = PROC_BYPASS;
					target_format[vd_path] = BT709;
					set_hdr_module_status(vd_path, HDR_MODULE_OFF);
					amdv_set_toggle_flag(1);
				} else {
					/* vd2 *->ipt when vd1 dolby on */
					hdr_process_mode[vd_path] = PROC_MATCH;
					hlg_process_mode[vd_path] = PROC_MATCH;
					sdr_process_mode[vd_path] = PROC_MATCH;
					hdr10_plus_process_mode[vd_path] = PROC_MATCH;
					hlg_plus_process_mode[vd_path] = PROC_MATCH;
					cuva_hdr_process_mode[vd_path] = PROC_MATCH;
					cuva_hlg_process_mode[vd_path] = PROC_MATCH;
					target_format[vd_path] = BT2100_IPT;
				}
			}
		}
	}

out:
	/* update change flags */
	if (is_amdv_on() &&
	    vd_path == VD1_PATH) {
		pr_csc(4, "am_vecm: vd%d: (%s) %s->%s.\n",
		       vd_path + 1,
		       policy_str[dv_policy],
		       input_str[dv_format],
		       dv_output_str[dv_mode]);
	} else {
		if (cur_hdr10_plus_process_mode[vd_path]
		    != hdr10_plus_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_hlg_plus_process_mode[vd_path]
		    != hlg_plus_process_mode[vd_path])
			change_flag |= SIG_HLG_MODE;
		if (cur_cuva_hdr_process_mode[vd_path]
		    != cuva_hdr_process_mode[vd_path])
			change_flag |= SIG_CUVA_HDR_MODE;
		if (cur_cuva_hlg_process_mode[vd_path]
		    != cuva_hlg_process_mode[vd_path])
			change_flag |= SIG_CUVA_HLG_MODE;
		if (cur_hdr_process_mode[vd_path]
		    != hdr_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_hlg_process_mode[vd_path]
		    != hlg_process_mode[vd_path])
			change_flag |= SIG_HLG_MODE;
		if (cur_sdr_process_mode[vd_path]
		    != sdr_process_mode[vd_path])
			change_flag |= SIG_HDR_MODE;
		if (cur_source_format[vd_path]
		    != source_format[vd_path])
			change_flag |= SIG_SRC_CHG;
		if (change_flag)
			pr_csc(4, "am_vecm: vd%d: (%s) %s->%s (%s).\n",
			       vd_path + 1,
			       policy_str[cur_hdr_policy],
			       input_str[source_format[vd_path]],
			       output_str[target_format[vd_path]],
			       is_amdv_on() ?
			       dv_output_str[dv_mode] :
			       output_str[output_format]);
	}

	cur_source_format[vd_path] = source_format[vd_path];
	if (is_amdv_on() &&
	    is_amdv_stb_mode() &&
	    vd_path == VD2_PATH &&
	    is_video_layer_on(VD2_PATH) &&
	    target_format[vd_path] != BT2100_IPT &&
	   !is_sbtm_on()) {
		if (!support_multi_core1()) {
			pr_csc(4, "am_vecm: vd%d output mode not match to dolby %s.\n",
			       vd_path + 1,
			       output_str[target_format[vd_path]]);
			change_flag |= SIG_OUTPUT_MODE_CHG;
		}
	} else if (!is_amdv_on() &&
		   is_video_layer_on(VD1_PATH) &&
		   (is_video_layer_on(VD2_PATH) &&
		   !is_vpp1(VD2_PATH)) &&
		   (target_format[vd_path] != target_format[oth_path])) {
		pr_csc(4, "am_vecm: vd%d output mode not match %s %s.\n",
		       vd_path + 1,
		       output_str[target_format[vd_path]],
		       output_str[target_format[oth_path]]);
		change_flag |= SIG_OUTPUT_MODE_CHG;
	}
	if (change_flag & SIG_OUTPUT_MODE_CHG) {
		/* need change video process for another path */
		switch (cur_source_format[oth_path]) {
		case HDRTYPE_HDR10PLUS:
			cur_hdr10_plus_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_HLGPLUS:
			cur_hlg_plus_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_CUVA_HDR:
			cur_cuva_hdr_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_CUVA_HLG:
			cur_cuva_hlg_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_HDR10:
		case HDRTYPE_HDR10_709:
			cur_hdr_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_HLG:
			cur_hlg_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_SDR:
			cur_sdr_process_mode[oth_path] = PROC_OFF;
			break;
		case HDRTYPE_MVC:
			cur_sdr_process_mode[oth_path] = PROC_BYPASS;
			cur_hdr_process_mode[oth_path] = PROC_BYPASS;
			cur_hlg_process_mode[oth_path] = PROC_BYPASS;
			cur_hdr10_plus_process_mode[oth_path] = PROC_BYPASS;
			cur_hlg_plus_process_mode[oth_path] = PROC_BYPASS;
			cur_cuva_hdr_process_mode[oth_path] = PROC_BYPASS;
			cur_cuva_hlg_process_mode[oth_path] = PROC_BYPASS;
			break;
		default:
			break;
		}
	}
	return change_flag;
}

static void prepare_hdr_info(struct master_display_info_s *hdr_data,
			     struct vframe_master_display_colour_s *p,
			     enum vd_path_e vd_path,
			     enum hdr_type_e *source_type)
{
	struct sbtmdb_tgt_parm *tgt_parm = NULL;

	hdr_data->present_flag = 0;
	memset(hdr_data->primaries, 0, sizeof(hdr_data->primaries));
	if (((hdr_data->features >> 16) & 0xff) == 9) {
		if (p->present_flag & 1) {
			memcpy(hdr_data->primaries,
			       p->primaries, sizeof(u32) * 6);
			memcpy(hdr_data->white_point,
			       p->white_point, sizeof(u32) * 2);
			hdr_data->luminance[0] =
				p->luminance[0];
			hdr_data->luminance[1] =
				p->luminance[1];
			if (p->content_light_level.present_flag == 1) {
				hdr_data->max_content =
					p->content_light_level.max_content;
				hdr_data->max_frame_average =
					p->content_light_level.max_pic_average;
			} else {
				hdr_data->max_content = 0;
				hdr_data->max_frame_average = 0;
			}
			hdr_data->luminance[0] = hdr_data->luminance[0] / 10000;
			hdr_data->present_flag = 1;
		} else if (source_type[vd_path] == HDRTYPE_SDR) {
			memcpy(hdr_data->primaries,
			       bt2020_primaries, sizeof(u32) * 6);
			memcpy(hdr_data->white_point,
			       bt2020_white_point, sizeof(u32) * 2);
			/* default luminance */
			hdr_data->luminance[0] = 1000 * 10000;
			hdr_data->luminance[1] = 50;

			/* content_light_level */
			hdr_data->max_content = 0;
			hdr_data->max_frame_average = 0;
			hdr_data->luminance[0] = hdr_data->luminance[0] / 10000;
			hdr_data->present_flag = 1;
		}
		if (is_sbtm_on() &&
			(source_type[vd_path] == HDRTYPE_SDR ||
			source_type[vd_path] == HDRTYPE_HDR10)) {
			tgt_parm = sbtm_sbtmdb_tgt_get();
			hdr_data->luminance[0] = tgt_parm->frmpblimit;
			hdr_data->luminance[1] = tgt_parm->minnits * 10; /*step 0.0001*/
			pr_csc(16, "lum max,min (%08x, %08x)\n",
				hdr_data->luminance[0],
				hdr_data->luminance[1]);
		}
	}
}

static int notify_vd_signal_to_amvideo(struct vd_signal_info_s *vd_signal,
	enum vpp_index_e vpp_index)
{
	static int pre_signal = -1;

	if (vpp_index != VPP_TOP0 &&
		vpp_index != VPP_PRE_VS)
		return -1;

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	if (pre_signal != vd_signal->signal_type) {
		vd_signal->vd1_signal_type =
			vd_signal->signal_type;
		if (is_vpp0(VD2_PATH))
			vd_signal->vd2_signal_type =
				vd_signal->signal_type;
		else
			vd_signal->vd2_signal_type = -1;
		pr_csc(8,
			"%s:line=%d, signal_type=%x, vd1_signal_type=%x, vd2_signal_type=%x\n",
			__func__, __LINE__,
			vd_signal->signal_type,
			vd_signal->vd1_signal_type,
			vd_signal->vd2_signal_type);
		amvideo_notifier_call_chain
			(AMVIDEO_UPDATE_SIGNAL_MODE,
			(void *)vd_signal);
	}
#endif
	pre_signal = vd_signal->signal_type;
	return 0;
}

static unsigned int content_max_lumin[VD_PATH_MAX];

void hdmi_packet_process(int signal_change_flag,
			 struct vinfo_s *vinfo,
			 struct vframe_master_display_colour_s *p,
			 struct hdr10plus_para *hdmitx_hdr10plus_param,
			 struct cuva_hdr_vsif_para *hdmitx_vsif_param,
			 struct cuva_hdr_vs_emds_para *hdmitx_edms_param,
			 enum vd_path_e vd_path,
			 enum hdr_type_e *source_type, enum vpp_index_e vpp_index)
{
	struct vout_device_s *vdev = NULL;
	struct master_display_info_s send_info = {0};
	enum output_format_e cur_output_format = output_format;
	void (*f_h10)(void *tx_instance, unsigned int flag,
		      struct hdr10plus_para *data);
	void (*f_h)(void *tx_instance, struct master_display_info_s *data);
	struct hdr10plus_para *h10_para;
	struct vd_signal_info_s vd_signal;
	enum hdr_type_e src_type[VD_PATH_MAX];
	static enum hdr_type_e cur_src_type[VD_PATH_MAX];
	static int cuva_drm_pkt_null;

	src_type[vd_path] = source_type[vd_path];

	f_h = NULL;
	f_h10 = NULL;
	h10_para = NULL;

	if (customer_hdr_clipping)
		content_max_lumin[vd_path] =
			customer_hdr_clipping;
	else if (p->luminance[0])
		content_max_lumin[vd_path] =
			p->luminance[0] / 10000;
	else
		content_max_lumin[vd_path] = 1250;

	if (!vinfo)
		return;

	if (vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
		vinfo->mode == VMODE_DUMMY_ENCP)
		return;

	if (is_meson_t7_cpu() && vinfo_lcd_support() &&
					vd_path != VD2_PATH) {
		pr_csc(8, "t7c BDS not send pkt in vd1/vd3\n");
		return;
	}

	if (!vinfo->vout_device) {
		/* pr_info("vinfo->vout_device is null, return\n"); */
		if (vpp_index == VPP_TOP0 ||
			vpp_index == VPP_PRE_VS) {
			vd_signal.signal_type = SIGNAL_SDR;
			notify_vd_signal_to_amvideo(&vd_signal, vpp_index);
		}
		output_format = BT709;
		pr_csc(4,
			"am_vecm: vd%d %s %s, vd2 %s %s, update output_format %s => %s\n",
			vd_path + 1,
			is_video_layer_on(VD1_PATH) ? "on" : "off",
			output_str[target_format[VD1_PATH]],
			is_video_layer_on(VD2_PATH) ? "on" : "off",
			output_str[target_format[VD2_PATH]],
			output_str[cur_output_format],
			output_str[output_format]);
		return;
	}

	if (vinfo->vout_device)
		vdev = vinfo->vout_device;

	if (vdev) {
		f_h = vdev->fresh_tx_hdr_pkt;
		f_h10 = vdev->fresh_tx_hdr10plus_pkt;
		h10_para = hdmitx_hdr10plus_param;
	}

	if (vdev && !f_h) {
		pr_info("vdev->fresh_tx_hdr_pkt is null, return\n");
		/* continue */
	}
	pr_csc(16,
	       "am_vecm: vd%d %s %s, vd2 %s %s, output_format %s,%s, flag 0x%x, hdr_cap = 0x%x\n",
	       vd_path + 1,
	       is_video_layer_on(VD1_PATH) ? "on" : "off",
	       output_str[target_format[VD1_PATH]],
	       is_video_layer_on(VD2_PATH) ? "on" : "off",
	       output_str[target_format[VD2_PATH]],
	       output_str[cur_output_format],
	       output_str[target_format[vd_path]],
	       signal_change_flag,
	       sink_hdr_support(vinfo));

	/*sbtm*/
	if (sbtm_mode_support_get() && sbtm_mode_get() &&
		(src_type[vd_path] == HDR10_SOURCE ||
		src_type[vd_path] == SDR_SOURCE) &&
		target_format[vd_path] == BT2020_PQ){
		sbtm_send_sbtmem_pkt(vinfo, 0);
		sbtm_pkt_clear = 1;
		pr_csc(1, "fresh_tx_sbtm_pkt\n");
	} else if (sbtm_pkt_clear) {
		sbtm_send_sbtmem_pkt(vinfo, 1);
		sbtm_pkt_clear = 0;
	}

	if (target_format[vd_path] == cur_output_format &&
		cur_output_format != BT2020_PQ_DYNAMIC &&
		src_type[vd_path] != CUVA_HDR_SOURCE &&
		src_type[vd_path] != CUVA_HLG_SOURCE &&
		!(signal_change_flag & SIG_FORCE_CHG) &&
		!(signal_change_flag & SIG_PRI_INFO) &&
		(cur_output_format == BT2020_PQ ||
		cur_output_format == BT2020_HLG ||
		cur_output_format == BT2020 ||
		cur_output_format == BT709 ||
		cur_output_format == BT_BYPASS) &&
		!(signal_change_flag & SIG_SRC_OUTPUT_CHG) &&
		(!is_sbtm_on() ||
		(is_sbtm_on() &&
		src_type[vd_path] != HDR10_SOURCE &&
		src_type[vd_path] != SDR_SOURCE)) &&
		(cur_src_type[vd_path] == src_type[vd_path]))
		return;

	/* clean hdr10plus packet when switch to others */
	if (target_format[vd_path] != BT2020_PQ_DYNAMIC &&
	    cur_output_format == BT2020_PQ_DYNAMIC) {
		if (get_hdr10_plus_pkt_delay()) {
			update_hdr10_plus_pkt(false,
					      (void *)NULL,
					      (void *)NULL,
					      vpp_index);
		} else if (f_h10) {
			f_h10(vdev->tx_instance, 0, h10_para);
		}
		pr_csc(4, "am_vecm: vd%d hdmi clean hdr10+ pkt\n",
		       vd_path + 1);
	}

	/* clean cuva packet when switch to others */
	/* src levea cuva or monitor/receiver exit*/
	if ((target_format[vd_path] != BT2020YUV_BT2020RGB_CUVA &&
		cur_output_format == BT2020YUV_BT2020RGB_CUVA) ||
		((cur_src_type[vd_path] == CUVA_HDR_SOURCE ||
		cur_src_type[vd_path] == CUVA_HLG_SOURCE) &&
		(src_type[vd_path] != CUVA_HDR_SOURCE &&
		src_type[vd_path] != CUVA_HLG_SOURCE)) ||
		((src_type[vd_path] == CUVA_HDR_SOURCE ||
		src_type[vd_path] == CUVA_HLG_SOURCE) &&
		cur_output_format == BT2020_PQ &&
		target_format[vd_path] != BT2020_PQ))  {
		if (get_cuva_pkt_delay()) {
			update_cuva_pkt(false,
				(void *)NULL,
				(void *)NULL,
				(void *)NULL,
				vpp_index);
		} else {
			if (get_tx_cuva_mode(vinfo) == CUVA_MONITOR) {
				if (vdev->fresh_tx_cuva_hdr_vsif)
					vdev->fresh_tx_cuva_hdr_vsif(vdev->tx_instance, NULL);
			} else if (get_tx_cuva_mode(vinfo) == CUVA_RECEIVER ||
				get_tx_cuva_mode(vinfo) == CUVA_AUTO) {
				if (vdev->fresh_tx_cuva_hdr_vs_emds)
					vdev->fresh_tx_cuva_hdr_vs_emds(vdev->tx_instance, NULL);
			}
		}
		pr_csc(4, "am_vecm: vd%d hdmi clean cuva vsif/emds pkt\n",
			vd_path + 1);
	}

	if (output_format != target_format[vd_path]) {
		pr_csc(4,
		       "am_vecm: vd%d %s %s, vd2 %s %s, output_format %s => %s\n",
		       vd_path + 1,
		       is_video_layer_on(VD1_PATH) ? "on" : "off",
		       output_str[target_format[VD1_PATH]],
		       is_video_layer_on(VD2_PATH) ? "on" : "off",
		       output_str[target_format[VD2_PATH]],
		       output_str[cur_output_format],
		       output_str[target_format[vd_path]]);
		output_format = target_format[vd_path];
	}

	if (cur_src_type[vd_path] != src_type[vd_path]) {
		pr_csc(4, "am_vecm: vd%d src type changed from %d to %d\n",
			vd_path + 1, cur_src_type[vd_path], src_type[vd_path]);
		cur_src_type[vd_path] = src_type[vd_path];
	}

	switch (output_format) {
	case BT709:
	case BT_BYPASS:
		send_info.features =
			/* default 709 limit */
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (1 << 16)	/* bt709 */
			| (1 << 8)	/* bt709 */
			| (1 << 0);	/* bt709 */
		vd_signal.signal_type = SIGNAL_SDR;
		pr_csc(16, "%s: SIGNAL_SDR vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT709_HDR:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (1 << 16)
			| (16 << 8)
			| (10 << 0);	/* bt2020c */
		vd_signal.signal_type = SIGNAL_HDR10;
		pr_csc(16, "%s: SIGNAL_HDR10_709 vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020:
		send_info.features =
			/* default 709 full */
			(1 << 30) /*sdr output 709*/
			| (1 << 29) /*video available*/
			| (5 << 26) /* unspecified */
			| (0 << 25) /* limit */
			| (1 << 24) /*color available*/
			| (9 << 16) /* 2020 */
			| (1 << 8)	/* bt709 */
			| (10 << 0);
		vd_signal.signal_type = SIGNAL_SDR;
		pr_csc(16, "%s: SIGNAL_SDR2020 vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020_PQ:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (16 << 8)
			| (10 << 0);	/* bt2020c */
		vd_signal.signal_type = SIGNAL_HDR10;
		pr_csc(16, "%s: SIGNAL_HDR10 vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020_HLG:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (18 << 8)
			| (10 << 0);
		vd_signal.signal_type = SIGNAL_HLG;
		pr_csc(16, "%s: SIGNAL_HLG vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020_PQ_DYNAMIC:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (16 << 8)  /* Always HDR10 */
			| (10 << 0); /* bt2020c */
		vd_signal.signal_type = SIGNAL_HDR10PLUS;
		pr_csc(16, "%s: SIGNAL_HDR10PLUS vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020_HLG_DYNAMIC:
		send_info.features =
			(0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (18 << 8)
			| (10 << 0);
		vd_signal.signal_type = SIGNAL_HLG;
		pr_csc(16, "%s: SIGNAL_HLGPLUS vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case BT2020YUV_BT2020RGB_CUVA:
			/* same as hdr10 */
			send_info.features =
			(1 << 31) /*signal_cuva*/
			| (0 << 30) /*sdr output 709*/
			| (1 << 29)	/*video available*/
			| (5 << 26)	/* unspecified */
			| (0 << 25)	/* limit */
			| (1 << 24)	/*color available*/
			| (9 << 16)
			| (16 << 8)
			| (10 << 0);	/* bt2020c */
		vd_signal.signal_type = SIGNAL_CUVA;
		pr_csc(16, "%s:SIGNAL_CUVA vpp_index = %d\n",
			__func__, vpp_index);
		break;
	case UNKNOWN_FMT:
	case BT2100_IPT:
		/* handle by dolby vision */
		return;
	}

	/* drm */
	prepare_hdr_info(&send_info, p, vd_path, source_type);
	memcpy(&dbg_hdr_send, &send_info,
	       sizeof(struct master_display_info_s));

	/* hdr10+ /hlg+ */
	if ((output_format == BT2020_PQ_DYNAMIC ||
		output_format == BT2020_HLG_DYNAMIC) &&
		hdmitx_hdr10plus_param) {
		if (get_hdr10_plus_pkt_delay()) {
			update_hdr10_plus_pkt(true,
					      (void *)h10_para,
					      (void *)&send_info,
					      vpp_index);
		} else {
			if (f_h)
				f_h(vdev->tx_instance, &send_info);
			if (f_h10)
				f_h10(vdev->tx_instance, 1, h10_para);
		}
		notify_vd_signal_to_amvideo(&vd_signal, vpp_index);
		return;
	}
	/* cuva */
	if ((src_type[vd_path] == CUVA_HDR_SOURCE ||
		src_type[vd_path] == CUVA_HLG_SOURCE) &&
	    hdmitx_vsif_param && hdmitx_edms_param) {
		if (get_cuva_pkt_delay()) {
			update_cuva_pkt(true,
				(void *)hdmitx_vsif_param,
				(void *)hdmitx_edms_param,
				(void *)&send_info,
				vpp_index);
		} else {
			//pr_csc(2, "vd%d fresh_tx pkt cuva_mode = %d, tx_cuva_mode = %d\n",
			//	vd_path + 1, get_cuva_mode(), get_tx_cuva_mode(vinfo));
			if ((get_cuva_mode() == CUVA_MONITOR) ||
				(get_cuva_mode() == CUVA_AUTO &&
				get_tx_cuva_mode(vinfo) == CUVA_MONITOR)) {
				if (!cuva_drm_pkt_null) {
					if (f_h)
						f_h(vdev->tx_instance, NULL);
					cuva_drm_pkt_null = 1;
				}
				if (vdev->fresh_tx_cuva_hdr_vsif)
					vdev->fresh_tx_cuva_hdr_vsif(vdev->tx_instance,
						hdmitx_vsif_param);
			} else if (output_format == BT2020YUV_BT2020RGB_CUVA) {
				/* cuva bypass,receiver mode,send emds*/
				if (!cuva_drm_pkt_null) {
					if (f_h)
						f_h(vdev->tx_instance, NULL);
					cuva_drm_pkt_null = 1;
				}
				if (vdev->fresh_tx_cuva_hdr_vs_emds) {
					vdev->fresh_tx_cuva_hdr_vs_emds(vdev->tx_instance,
						hdmitx_edms_param);
					pr_csc(2, "vd%d fresh_tx_cuva_hdr_vs_emds pkt ma(%d)\n",
						vd_path + 1, hdmitx_edms_param->base_param_m_a);
				}
			} else {
				if (vdev->fresh_tx_hdr_pkt)
					vdev->fresh_tx_hdr_pkt(vdev->tx_instance, &send_info);
				cuva_drm_pkt_null = 0;
			}
		}
		notify_vd_signal_to_amvideo(&vd_signal, vpp_index);
		return;
	}
	/* none hdr+ and cuva*/
	if (f_h) {
		if ((src_type[vd_path] == CUVA_HDR_SOURCE ||
			src_type[vd_path] == CUVA_HLG_SOURCE)) {
			return;
		}
		f_h(vdev->tx_instance, &send_info);
		notify_vd_signal_to_amvideo(&vd_signal, vpp_index);
		cuva_drm_pkt_null = 0;
	}
}

#ifdef T7_BRINGUP_MULTI_VPP
void video_post_process_t7(struct vframe_s *vf,
			enum vpp_matrix_csc_e csc_type,
			struct vinfo_s *vinfo,
			enum vd_path_e vd_path,
			struct vframe_master_display_colour_s *master_info,
			enum hdr_type_e *source_type)
{
	//special for t7
	// since in with t7, there will be more difference combination
	// of vd input and osd input, vd2 or vd3 is possible be the main video

	//vd1+osd0+osd1+vpp0+venc0 as 1st/main display process path
	//vd2+osd3+vpp1+venc1 as 2nd/assistant display process path
	//vd3+osd4+vpp2+venc2 as 3th/assistant display process path

	// if 3 vpp is enabled
	// vd2 could be the main video of vpp1 typically
	// vd3 could be the main video of vpp2 typically
	if (get_cpu_type() != MESON_CPU_MAJOR_ID_T7)
		return 0;
}
#endif

int get_s5_slice_mode(void)
{
	int slice_number;

	if (is_meson_t3x_cpu())
		return get_slice_num(0);

	if (!is_meson_s5_cpu())
		return 1;

	if (slice_set & 0x10)
		slice_number = slice_set & 0x0f;
	else
		slice_number = get_slice_num(0);

	pr_csc(64, "slice number %d\n", slice_number);

	if (slice_number == 0 || slice_number > 4)
		slice_number = 1;

	return slice_number;
}

void output_color_fmt_convert(int vpp_index)
{
	if (get_eye_pro_en())
		return;

	if (vinfo_hdmi_out_fmt()) {
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_YUV709_RGB, MTX_ON, SLICE0, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_YUV709_RGB, MTX_ON, SLICE1, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_YUV709_RGB, MTX_ON, SLICE2, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_YUV709_RGB, MTX_ON, SLICE3, vpp_index);
	} else {
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_NULL, MTX_OFF, SLICE0, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_NULL, MTX_OFF, SLICE1, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_NULL, MTX_OFF, SLICE2, vpp_index);
		mtx_setting_v2(POST_MTX, WR_DMA,
			MATRIX_NULL, MTX_OFF, SLICE3, vpp_index);
	}
	pr_csc(12, "%s: post matrix convert color format for hdmi out. fmt: %d\n",
		__func__, vinfo_hdmi_out_fmt());
}

static void setup_gamut_wraper_params(struct gamut_mapping_s *params,
	struct matrix_s *matrix,
	int mtx_depth,
	int eotf_en,
	int oetf_en,
	int mtx_en)
{
	int i = 0;

	if (!params || !matrix)
		return;

	for (i = 0; i < 3; i++) {
		params->mtx[i][0] = matrix->matrix[i][0];
		params->mtx[i][1] = matrix->matrix[i][1];
		params->mtx[i][2] = matrix->matrix[i][2];
		params->mtx_pos_offset[i] = 0;
		params->mtx_pre_offset[i] = 0;
	}
	params->mtx_rs = mtx_depth - 8; /*mtx_rs:0=8bit,1=9bit*/
	params->flag_update_mtx = 1;
	params->eotf_en = eotf_en;
	params->oetf_en = oetf_en;
	params->mtx_en = mtx_en;
}

void cal_gamut_mapping_wrapper_matrix(enum hdr_type_e source_type,
	struct vinfo_s *vinfo, int bypass_gamut, enum vpp_index_e vpp_index)
{
	int mtx_depth = 11;
	struct matrix_s m = {
		{0, 0, 0},
		{
			{0x800, 0, 0},
			{0, 0x800, 0},
			{0, 0, 0x800},
		},
		{0, 0, 0},
		3
	};

	struct matrix_s m0 = {
		{0, 0, 0},
		{
			{0x800, 0, 0},
			{0, 0x800, 0},
			{0, 0, 0x800},
		},
		{0, 0, 0},
		3
	};

	if (chip_type_id != chip_t6x)
		return;

	pr_csc(32,
		"gamut_src_type=%d, gamut_mapping1_en=%d, bypass_gamut=%d, vpp_color_pri_sel=%d\n",
		source_type, gamut_mapping1_en, bypass_gamut, vpp_color_pri_sel);

	gamut_mapping0_en = gamut_mapping1_en;
	gamut_mapping0_param.gmt0_after_osd = 1;
	if (vpp_color_pri_sel) {
		/*gamut1*/
		gamut_mode_process(vinfo, HDRTYPE_HDR10, &m, mtx_depth, DEST_PANEL,
			GAMUT_WRAPPER1);
		setup_gamut_wraper_params(&gamut_mapping1_param, &m,
			mtx_depth, 1, 1, 1);
		set_gamut_mapping_wrapper(1, vpp_index);

		/*gamut0*/
		gamut_mode_process(vinfo, HDRTYPE_SDR, &m0, mtx_depth, DEST_HDR10,
			GAMUT_WRAPPER0);
		setup_gamut_wraper_params(&gamut_mapping0_param, &m0,
			mtx_depth, 1, 1, 1);
		set_gamut_mapping_wrapper(0, vpp_index);
	} else {
		/*gamut1*/
		if (!bypass_gamut)
			gamut_mode_process(vinfo, source_type, &m, mtx_depth, DEST_PANEL,
				GAMUT_WRAPPER1);
		setup_gamut_wraper_params(&gamut_mapping1_param, &m,
			mtx_depth, !bypass_gamut, !bypass_gamut, !bypass_gamut);
		set_gamut_mapping_wrapper(1, vpp_index);

		/*gamut0*/
		gamut_mode_process(vinfo, HDRTYPE_SDR, &m0, mtx_depth,
			bypass_gamut ? DEST_PANEL : DEST_SDR,
			GAMUT_WRAPPER0);
		setup_gamut_wraper_params(&gamut_mapping0_param, &m0,
			mtx_depth, 1, 1, 1);
		set_gamut_mapping_wrapper(0, vpp_index);
	}
}

void video_post_process(struct vframe_s *vf,
			enum vpp_matrix_csc_e csc_type,
			struct vinfo_s *vinfo,
			enum vd_path_e vd_path,
			struct vframe_master_display_colour_s *master_info,
			enum hdr_type_e *source_type, enum vpp_index_e vpp_index)
{
	enum hdr_type_e src_format = cur_source_format[vd_path];
	/*eo clip select: 0->23bit eo; 1->32 bit eo*/
	unsigned int eo_sel = 0;
	enum mtx_csc_e mtx_csc = MATRIX_NULL;
	int s5_silce_mode = get_s5_slice_mode();
	struct matrix_s *mtx = NULL;
	struct matrix_s m = {
		{0, 0, 0},
		{
			{0x0d49, 0x1b4d, 0x1f6b},
			{0x1f01, 0x0910, 0x1fef},
			{0x1fdb, 0x1f32, 0x08f3},
		},
		{0, 0, 0},
		1
	};
	enum hdr_type_e gamut_src_format = HDRTYPE_SDR;
	int bypass_gamut_wrapper = 0;
	pr_csc(32, "%d  %s: vd_path=%d is vpp(%d %d) vpp_index = %d\n",
		__LINE__,
		__func__,
		vd_path,
		is_vpp0(VD1_PATH),
		is_vpp1(VD2_PATH),
		vpp_index);
	pr_csc(32, "vd_path=%d, vd1 %s, vd2 %s, hdr_module = %d src = %d\n",
		vd_path,
		is_video_layer_on(VD1_PATH) ? "on" : "off",
		is_video_layer_on(VD2_PATH) ? "on" : "off",
		get_hdr_module_status(vd_path, vpp_index),
		cur_source_format[vd_path]);
	cur_vd_path = vd_path;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
		eo_sel = 1;

	if (get_hdr_module_status(vd_path, vpp_index) == HDR_MODULE_OFF) {
		if (vd_path == VD1_PATH)
			hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
				vinfo, NULL, s5_silce_mode, vpp_index);
		else
			hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS, vinfo, NULL, vpp_index);
		if (!osd_gamut_conv_type &&
			((vd_path == VD1_PATH &&
			!is_video_layer_on(VD2_PATH)) ||
			(vd_path == VD2_PATH &&
			!is_video_layer_on(VD1_PATH)) ||
			(vd_path == VD2_PATH &&
			is_vpp1(VD2_PATH)))) {
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (!osd_gamut_conv_type &&
			get_cpu_type() == MESON_CPU_MAJOR_ID_T7) {
			pr_csc(32, "vd_path=%d vpp_index=%d, hdr_proc osd\n",
				vd_path,
				vpp_index);
			if (vd_path == VD1_PATH && !is_video_layer_on(VD1_PATH))
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			else if (vd_path == VD2_PATH && !is_video_layer_on(VD2_PATH))
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		}
		src_format = HDRTYPE_NONE;
	}

	switch (src_format) {
	case HDRTYPE_PRIMESL:
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
		if (get_prime_sl_hdr_process_policy(vf) == PRIME_SL_HDR_MODE_2020_TO_709) {
			gamut_convert_process(vinfo, source_type, vd_path, &m, 8, DEST_NONE);
			eo_clip_proc(master_info, eo_sel);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_GMT_CONVERT,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_SDR,
					vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (get_prime_sl_hdr_process_policy(vf) == PRIME_SL_HDR_MODE_709_TO_2020) {
			gamut_src_format = HDRTYPE_HDR10;
			if (chip_cls_id == TV_CHIP) {
				/*for tv chip need sd2hdr function*/
				if (gamut_conv_enable) {
					gamut_convert_process(vinfo, source_type,
						vd_path, &m, 11, DEST_NONE);
					bypass_gamut_wrapper = 1;
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, SDR_HDR,
						vinfo, gamut_conv_enable ? &m : NULL,
						s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), SDR_HDR,
						vinfo, gamut_conv_enable ? &m : NULL,
						vpp_index);
				if (!osd_gamut_conv_type) {
					hdr_proc(vf, OSD1_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
				}
			} else {
				if (sbtm_mode_support_get() && sbtm_mode_get()) {
					pr_csc(12, "sbtm:vd%d,PROC_SDR_TO_HDR\n", vd_path + 1);
					sbtm_convert_process(SBTM_SDR_HDR_PROC,
						source_type[vd_path], vinfo, &m, 11);
					mtx = &m;
				} else  {
					mtx = NULL;
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, SDR_HDR,
						vinfo, mtx, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), SDR_HDR,
						vinfo, mtx, vpp_index);
				if (vd_path == VD1_PATH ||
					(vd_path == VD2_PATH &&
					is_video_layer_on(VD2_PATH) &&
					!is_video_layer_on(VD1_PATH))) {
					if (sbtm_mode_support_get() && sbtm_mode_get()) {
						pr_csc(12, "sbtm:OSD,PROC_SDR_TO_HDR\n");
						sbtm_convert_process(SBTM_OSD_PROC,
							source_type[vd_path], vinfo, &m, 11);
						mtx = &m;
					} else {
						mtx = NULL;
					}
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, mtx, vpp_index);
				}
			}
		} else if (get_prime_sl_hdr_process_policy(vf) == PRIME_SL_HDR_MODE_BYPASS_2020) {
			if (sbtm_mode_support_get()) {
				if (sbtm_mode_get()) {
					pr_csc(12, "sbtm:vd_path=%d, --HDR_HDR_PROC\n",
						vd_path);
					sbtm_convert_process(SBTM_HDR_HDR_PROC,
						source_type[vd_path], vinfo, &m, 11);
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, HDR_HDR,
						vinfo, &m, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), HDR_HDR,
						vinfo, &m, vpp_index);
				if (vd_path == VD1_PATH ||
					(vd_path == VD2_PATH &&
					is_video_layer_on(VD2_PATH) &&
					!is_video_layer_on(VD1_PATH))) {
					if (sbtm_mode_get()) {
						pr_csc(12, "sbtm:OSD, video HDR_HDR_PROC\n");
						sbtm_convert_process(SBTM_OSD_PROC,
							source_type[vd_path], vinfo, &m, 11);
						mtx = &m;
					} else {
						mtx = NULL;
					}
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, mtx, vpp_index);
				}
			}  else {
				gamut_src_format = src_format;
				bypass_gamut_wrapper = 1;
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
						vinfo, NULL, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
						vinfo, NULL, vpp_index);
				if ((get_hdr_policy() == 2 &&
					target_format[vd_path] == BT_BYPASS) ||
					chip_cls_id == TV_CHIP) {
					hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				} else {
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
				}
			}
		} else {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
					vinfo, NULL, vpp_index);
			if ((vd_path == VD1_PATH &&
				!is_video_layer_on(VD2_PATH)) ||
			     (vd_path == VD2_PATH &&
			     !is_video_layer_on(VD1_PATH))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					vpp_index);
			}
		}
#endif
		break;
	case HDRTYPE_SDR:
		if (!is_sbtm_on() && vd_path == VD2_PATH &&
		    is_amdv_on() && !support_multi_core1() &&
		    is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, SDR_IPT, vinfo, NULL, vpp_index);
		} else if (sdr_process_mode[vd_path] == PROC_BYPASS) {
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
					vinfo, NULL, vpp_index);
			if (!osd_gamut_conv_type &&
				((vd_path == VD1_PATH &&
				!is_video_layer_on(VD2_PATH)) ||
				(vd_path == VD2_PATH &&
				!is_video_layer_on(VD1_PATH)))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_TRG) {
			if (gamut_conv_enable) {
				gamut_convert_process(
					vinfo, source_type, vd_path, &m, 11, DEST_NONE);
			}
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR,
					gamut_conv_enable ? SDR_GMT_CONVERT : HDR_BYPASS,
					vinfo,
					gamut_conv_enable ? &m : NULL,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path),
					gamut_conv_enable ? SDR_GMT_CONVERT : HDR_BYPASS,
					vinfo,
					gamut_conv_enable ? &m : NULL, vpp_index);
			if (!osd_gamut_conv_type &&
				((vd_path == VD1_PATH &&
				!is_video_layer_on(VD2_PATH)) ||
				(vd_path == VD2_PATH &&
				!is_video_layer_on(VD1_PATH)))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HDR) {
			gamut_src_format = HDRTYPE_HDR10;
			bypass_gamut_wrapper = 1;
			if (chip_cls_id == TV_CHIP) {
				/*for tv chip need sd2hdr function*/
				if (gamut_conv_enable) {
					gamut_convert_process(vinfo, source_type,
						vd_path, &m, 11, DEST_NONE);
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, SDR_HDR,
						vinfo, gamut_conv_enable ? &m : NULL,
						s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), SDR_HDR,
						vinfo, gamut_conv_enable ? &m : NULL,
						vpp_index);
				if (!osd_gamut_conv_type) {
					hdr_proc(vf, OSD1_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, HDR_BYPASS,
						vinfo, NULL, vpp_index);
					}
			} else {
				if (sbtm_mode_support_get() && sbtm_mode_get()) {
					pr_csc(12, "sbtm:vd%d PROC_SDR_TO_HDR\n", vd_path + 1);
					sbtm_convert_process(SBTM_SDR_HDR_PROC,
						source_type[vd_path], vinfo, &m, 11);
					mtx = &m;
				} else {
					mtx = NULL;
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, SDR_HDR,
						vinfo, mtx, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), SDR_HDR,
						vinfo, mtx, vpp_index);
				if (vd_path == VD1_PATH ||
					(vd_path == VD2_PATH &&
					is_video_layer_on(VD2_PATH) &&
					!is_video_layer_on(VD1_PATH))) {
					if (sbtm_mode_support_get() && sbtm_mode_get()) {
						pr_csc(12, "sbtm:OSD,PROC_SDR_TO_HDR\n");
						sbtm_convert_process(SBTM_OSD_PROC,
							source_type[vd_path], vinfo, &m, 11);
						mtx = &m;
					} else {
						mtx = NULL;
					}
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, mtx, vpp_index);
				}
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_HLG,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), SDR_HLG,
					vinfo, NULL, vpp_index);

			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		} else if (sdr_process_mode[vd_path] == PROC_SDR_AC_SDR) {
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_AC_ENH,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), SDR_AC_ENH, vinfo, NULL,
					vpp_index);
			if (!osd_gamut_conv_type &&
				((vd_path == VD1_PATH &&
				!is_video_layer_on(VD2_PATH)) ||
				(vd_path == VD2_PATH &&
				!is_video_layer_on(VD1_PATH)))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		}
		break;
	case HDRTYPE_SDR2020:
		if (vd_path == VD2_PATH &&
		    is_amdv_on() && !support_multi_core1() &&
		    is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, SDR_IPT, vinfo, NULL, vpp_index);
		} else if (sdr_process_mode[vd_path] == PROC_BYPASS &&
			target_format[vd_path] == BT2020) {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS, vinfo, NULL,
					vpp_index);
			if ((vd_path == VD1_PATH &&
			     !is_video_layer_on(VD2_PATH)) ||
			     (vd_path == VD2_PATH &&
			     !is_video_layer_on(VD1_PATH))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		} else if (sdr_process_mode[vd_path] == PROC_BYPASS &&
			target_format[vd_path] == BT709) {
			gamut_convert_process(vinfo, source_type,
				vd_path, &m, 11, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR,
					SDR_GMT_CONVERT,
					vinfo,
					&m,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path),
					SDR_GMT_CONVERT,
					vinfo,
					&m, vpp_index);
			if ((vd_path == VD1_PATH &&
				 !is_video_layer_on(VD2_PATH)) ||
				 (vd_path == VD2_PATH &&
				 !is_video_layer_on(VD1_PATH))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_TRG) {
			if (gamut_conv_enable) {
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 11, DEST_NONE);
			}
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR,
					gamut_conv_enable ? SDR_GMT_CONVERT : HDR_BYPASS,
					vinfo,
					gamut_conv_enable ? &m : NULL,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path),
					gamut_conv_enable ? SDR_GMT_CONVERT : HDR_BYPASS,
					vinfo,
					gamut_conv_enable ? &m : NULL, vpp_index);
			if ((vd_path == VD1_PATH &&
			     !is_video_layer_on(VD2_PATH)) ||
			     (vd_path == VD2_PATH &&
			     !is_video_layer_on(VD1_PATH))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HDR) {
			gamut_src_format = HDRTYPE_HDR10;
			bypass_gamut_wrapper = 1;
			if (sbtm_mode_support_get() && sbtm_mode_get()) {
				pr_csc(12, "sbtm:vd%d PROC_SDR_TO_HDR\n", vd_path + 1);
				sbtm_convert_process(SBTM_SDR_HDR_PROC,
					source_type[vd_path], vinfo, &m, 11);
				mtx = &m;
			} else {
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 11, DEST_HDR10);
				mtx = &m;
			}
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_HDR,
					vinfo, mtx, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), SDR_HDR,
					vinfo, mtx, vpp_index);
			if (chip_cls_id == TV_CHIP) {
				/*for tv chip need sd2hdr function*/
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				if (vd_path == VD1_PATH ||
					(vd_path == VD2_PATH &&
					is_video_layer_on(VD2_PATH) &&
					!is_video_layer_on(VD1_PATH))) {
					if (sbtm_mode_support_get() && sbtm_mode_get()) {
						pr_csc(12, "sbtm:OSD,PROC_SDR_TO_HDR\n");
						sbtm_convert_process(SBTM_OSD_PROC,
							source_type[vd_path], vinfo, &m, 11);
						mtx = &m;
					} else {
						mtx = NULL;
					}
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, mtx, vpp_index);
				}
			}
		} else if (sdr_process_mode[vd_path] == PROC_SDR_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			gamut_convert_process(vinfo, source_type,
				vd_path, &m, 11, DEST_HDR10);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_HLG,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), SDR_HLG,
					vinfo, &m, vpp_index);

			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		} else if (sdr_process_mode[vd_path] == PROC_SDR_AC_SDR) {
			gamut_convert_process(vinfo, source_type,
				vd_path, &m, 11, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, SDR_AC_ENH,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), SDR_AC_ENH,
					vinfo, &m, vpp_index);
			if ((vd_path == VD1_PATH &&
			     !is_video_layer_on(VD2_PATH)) ||
			     (vd_path == VD2_PATH &&
			     !is_video_layer_on(VD1_PATH))) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			} else if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL,
					 vpp_index);
			}
		}
		break;
	case HDRTYPE_HDR10:
	case HDRTYPE_HDR10_709:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, HDR_IPT, vinfo, NULL, vpp_index);
		} else if (hdr_process_mode[vd_path] == PROC_BYPASS) {
			if (sbtm_mode_support_get()) {
				if (sbtm_mode_get()) {
					pr_csc(12, "sbtm:vd_path=%d, --HDR_HDR_PROC\n",
						vd_path);
					sbtm_convert_process(SBTM_HDR_HDR_PROC,
						source_type[vd_path], vinfo, &m, 11);
				}
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, HDR_HDR,
						vinfo, &m, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), HDR_HDR,
						vinfo, &m, vpp_index);
				if (vd_path == VD1_PATH ||
					(vd_path == VD2_PATH &&
					is_video_layer_on(VD2_PATH) &&
					!is_video_layer_on(VD1_PATH))) {
					if (sbtm_mode_get()) {
						pr_csc(12, "sbtm:OSD, video HDR_HDR_PROC\n");
						sbtm_convert_process(SBTM_OSD_PROC,
							source_type[vd_path], vinfo, &m, 11);
						mtx = &m;
					} else {
						mtx = NULL;
					}
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, mtx, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, mtx, vpp_index);
				}
			} else {
				gamut_src_format = src_format;
				bypass_gamut_wrapper = 1;
				if (vd_path == VD1_PATH)
					hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
						vinfo, NULL, s5_silce_mode, vpp_index);
				else
					hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
						vinfo, NULL, vpp_index);
				if ((get_hdr_policy() == 2 &&
					target_format[vd_path] == BT_BYPASS) ||
					chip_cls_id == TV_CHIP) {
					hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				} else {
					hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
					hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
				}
			}
		} else if (hdr_process_mode[vd_path] == PROC_HDR_TO_SDR) {
			gamut_convert_process(vinfo, source_type, vd_path, &m, 8, DEST_NONE);
			eo_clip_proc(master_info, eo_sel);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_SDR,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_SDR,
					vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (hdr_process_mode[vd_path] == PROC_HDR_TO_HLG) {
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_HLG, vinfo,
					NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_HLG, vinfo,
					NULL, vpp_index);

			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_HLG:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, HLG_IPT, vinfo, NULL, vpp_index);
		} else if (hlg_process_mode[vd_path] == PROC_BYPASS) {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
					vinfo, NULL, vpp_index);
			if (get_hdr_policy() == 2 &&
			    target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			}
		} else if (hlg_process_mode[vd_path] == PROC_HLG_TO_SDR) {
			if (gamut_conv_enable &&
				chip_type_id > chip_t6d)
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 14, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HLG_SDR,
					vinfo,
					(gamut_conv_enable &&
					chip_type_id > chip_t6d) ? &m : NULL,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HLG_SDR,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (hlg_process_mode[vd_path] == PROC_HLG_TO_HDR) {
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HLG_HDR,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HLG_HDR,
					vinfo, NULL, vpp_index);

			hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_HDR10PLUS:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, HDR_IPT, vinfo, NULL, vpp_index);
		} else if (hdr10_plus_process_mode[vd_path] == PROC_BYPASS) {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
					vinfo, NULL, vpp_index);
			if ((get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS) ||
				chip_cls_id == TV_CHIP) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			}
		} else if (hdr10_plus_process_mode[vd_path] == PROC_HDRP_TO_HDR) {
			gamut_src_format = src_format;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_BYPASS,
					vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
		} else if (hdr10_plus_process_mode[vd_path] == PROC_HDRP_TO_SDR) {
			gamut_convert_process(vinfo, source_type, vd_path, &m, 13, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR10P_SDR,
					vinfo, &m, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR10P_SDR,
					vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (hdr10_plus_process_mode[vd_path] == PROC_HDRP_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_HLG,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), HDR_HLG,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_HLGPLUS:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, HLG_IPT, vinfo, NULL, vpp_index);
		} else if (hlg_plus_process_mode[vd_path] == PROC_BYPASS) {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else if (vd_path == VD2_PATH)
				hdr_proc(vf, VD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			else if (vd_path == VD3_PATH)
				hdr_proc(vf, VD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			if (get_hdr_policy() == 2 &&
			    target_format[vd_path] == BT_BYPASS) {
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			}
		} else if (hlg_plus_process_mode[vd_path] == PROC_HLGP_TO_HDR) {
			gamut_src_format = HDRTYPE_HDR10;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HLG_HDR,
					vinfo, &m, s5_silce_mode, vpp_index);
			else if (vd_path == VD2_PATH)
				hdr_proc(vf, VD2_HDR, HLG_HDR, vinfo, &m, vpp_index);
			else if (vd_path == VD3_PATH)
				hdr_proc(vf, VD3_HDR, HLG_HDR, vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
		} else if (hlg_plus_process_mode[vd_path] == PROC_HLGP_TO_SDR) {
			if (gamut_conv_enable)
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 13, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HLGP_SDR,
					vinfo,
					gamut_conv_enable ? &m : NULL,
					s5_silce_mode, vpp_index);
			else if (vd_path == VD2_PATH)
				hdr_proc_ext(vf, VD2_HDR, HLGP_SDR, vinfo, &m, vpp_index);
			else if (vd_path == VD3_PATH)
				hdr_proc_ext(vf, VD3_HDR, HLGP_SDR, vinfo, &m, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (hlg_plus_process_mode[vd_path] == PROC_HLGP_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, HDR_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else if (vd_path == VD2_PATH)
				hdr_proc(vf, VD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			else if (vd_path == VD3_PATH)
				hdr_proc(vf, VD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_CUVA_HDR:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, CUVA_IPT, vinfo, NULL, vpp_index);
		} else if (cuva_hdr_process_mode[vd_path] == PROC_BYPASS) {
			pr_csc(1, "cuva bypass(cuva to cuva)\n");
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVA_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVA_BYPASS,
					vinfo, NULL, vpp_index);
			if (get_hdr_policy() == 2 &&
			    target_format[vd_path] == BT_BYPASS){
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				hdr_proc(vf, OSD1_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
			}
		} else if (cuva_hdr_process_mode[vd_path] == PROC_CUVA_TO_SDR) {
			if (gamut_conv_enable)
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 14, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVA_SDR,
					vinfo,
					gamut_conv_enable ? &m : NULL,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVA_SDR,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (cuva_hdr_process_mode[vd_path] == PROC_CUVA_TO_HDR) {
			gamut_src_format = HDRTYPE_HDR10;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVA_HDR,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVA_HDR,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
		} else if (cuva_hdr_process_mode[vd_path] == PROC_CUVA_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVA_HLG,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVA_HLG,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_CUVA_HLG:
		if (vd_path == VD2_PATH && is_amdv_on() && is_amdv_stb_mode()) {
			hdr_proc(vf, VD2_HDR, CUVA_IPT, vinfo, NULL, vpp_index);
		} else if (cuva_hlg_process_mode[vd_path] == PROC_BYPASS) {
			gamut_src_format = src_format;
			bypass_gamut_wrapper = 1;
			pr_csc(1, "%s cuva hlg bypass.(cuva hlg to cuva hlg)\n", __func__);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVA_BYPASS,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVA_BYPASS,
					vinfo, NULL, vpp_index);
			if (get_hdr_policy() == 2 &&
				target_format[vd_path] == BT_BYPASS){
				hdr_proc(vf, OSD1_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			} else {
				hdr_proc(vf, OSD1_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD2_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
				hdr_proc(vf, OSD3_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
			}
		} else if (cuva_hlg_process_mode[vd_path] == PROC_CUVAHLG_TO_SDR) {
			if (gamut_conv_enable)
				gamut_convert_process(vinfo, source_type,
					vd_path, &m, 14, DEST_NONE);
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVAHLG_SDR,
					vinfo,
					gamut_conv_enable ? &m : NULL,
					s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVAHLG_SDR,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, HDR_BYPASS, vinfo, NULL, vpp_index);
		} else if (cuva_hlg_process_mode[vd_path] == PROC_CUVAHLG_TO_HDR) {
			gamut_src_format = HDRTYPE_HDR10;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVAHLG_HDR,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVAHLG_HDR,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HDR, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HDR, vinfo, NULL, vpp_index);
		} else if (cuva_hlg_process_mode[vd_path] == PROC_CUVAHLG_TO_HLG) {
			gamut_src_format = HDRTYPE_HLG;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVAHLG_HLG,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVAHLG_HLG,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_HLG, vinfo, NULL, vpp_index);
		} else if (cuva_hlg_process_mode[vd_path] == PROC_CUVAHLG_TO_CUVA) {
			pr_csc(1, "cuva hlg bypass(cuva hlg to cuva)\n");
			gamut_src_format = HDRTYPE_CUVA_HDR;
			bypass_gamut_wrapper = 1;
			if (vd_path == VD1_PATH)
				hdr_proc_multi_slices(vf, VD1_HDR, CUVAHLG_CUVA,
					vinfo, NULL, s5_silce_mode, vpp_index);
			else
				hdr_proc(vf, get_hdr_module(vd_path), CUVAHLG_CUVA,
					vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD1_HDR, SDR_HLG, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD2_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
			hdr_proc(vf, OSD3_HDR, SDR_CUVA, vinfo, NULL, vpp_index);
		}
		break;
	case HDRTYPE_MVC:
		hdr_osd_off(vpp_index);
		hdr_vd1_off(vpp_index);
		hdr_vd2_off(vpp_index);
		break;
	default:
		break;
	}

	if (dpss_mode)
		goto mode_change;

	if (src_format != HDRTYPE_SDR && !vpp_color_pri_sel)
		bypass_gamut_wrapper = 1;
	if (vd_path == VD1_PATH)
		cal_gamut_mapping_wrapper_matrix(gamut_src_format, vinfo,
			bypass_gamut_wrapper, vpp_index);

	if (chip_type_id == chip_a4) {
		if (!(vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
			vinfo->mode == VMODE_DUMMY_ENCP))
			mtx_setting(POST2_MTX, MATRIX_NULL, MTX_OFF);
		else
			mtx_setting(POST2_MTX, MATRIX_YUV709_RGB, MTX_ON);
	} else if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A &&
		chip_type_id != chip_t3x) {
		if (!(vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
			vinfo->mode == VMODE_DUMMY_ENCP)) {
			pr_csc(12, "%s: vpp_index = %d mode = %d [%d]\n",
				__func__,
			    vpp_index,
			    vinfo->mode,
			    __LINE__);
			pr_csc(128, "%s: no lcd csc_type = %d\n",
				__func__, csc_type);
			if (chip_type_id == chip_s5) {
				output_color_fmt_convert(vpp_index);
			} else {
				if (vpp_index == VPP_TOP1) {
					mtx_setting(VPP1_POST2_MTX, MATRIX_NULL, MTX_OFF);
				} else if (vpp_index == VPP_TOP2) {
					mtx_setting(VPP2_POST2_MTX, MATRIX_NULL, MTX_OFF);
				} else {
					if (!get_ep_ipt_flag())
						mtx_setting(POST2_MTX, MATRIX_NULL, MTX_OFF);
				}
			}
		} else {
			pr_csc(12, "vpp_index = %d mode = %d\n", vpp_index, vinfo->mode);
			if ((vf && vf->type & VIDTYPE_RGB_444) &&
			    source_type[vd_path] == HDRTYPE_SDR &&
			    (get_hdr_module_status(vd_path, vpp_index) != HDR_MODULE_OFF)) {
				pr_csc(12, "%s: type[vd%d]=%d\n",
					__func__,
					vd_path + 1,
					source_type[vd_path]);
				pr_csc(128, "sdr rgb444 csc_type = %d\n", csc_type);

				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
					0, 1, 1, vpp_index);
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ2_MISC,
					0, 1, 1, vpp_index);

				if (!watermark_support) {
					if (vpp_index == VPP_TOP1)
						mtx_setting(VPP1_POST2_MTX,
							MATRIX_YUV709F_RGB, MTX_ON);
					else if (vpp_index == VPP_TOP2)
						mtx_setting(VPP2_POST2_MTX,
							MATRIX_YUV709F_RGB, MTX_ON);
					else
						mtx_setting(POST2_MTX,
							MATRIX_YUV709F_RGB, MTX_ON);
				} else {
					mtx_setting(POST2_MTX,
							MATRIX_YUV709F_RGB, MTX_OFF);
					mtx_setting(POST_MTX,
							MATRIX_YUV709F_RGB, MTX_ON);
				}
			} else {
				pr_csc(12, "type[vd%d]=%d\n",
					vd_path + 1,
					source_type[vd_path]);
				pr_csc(128, " others csc_type = %d\n", csc_type);
				/*VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 1, 1);*/
				/*VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 1, 1);*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
					0, 1, 1, vpp_index);
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ2_MISC,
					0, 1, 1, vpp_index);

				if ((csc_type == VPP_MATRIX_YUV709F_RGB  ||
					csc_type == VPP_MATRIX_YUV601F_RGB) &&
					source_type[vd_path] != HDRTYPE_DOVI)
					mtx_csc = MATRIX_YUV709F_RGB;
				else
					mtx_csc = MATRIX_YUV709_RGB;

				if (!watermark_support) {
					if (vpp_index == VPP_TOP1)
						mtx_setting(VPP1_POST2_MTX,
							mtx_csc, MTX_ON);
					else if (vpp_index == VPP_TOP2)
						mtx_setting(VPP2_POST2_MTX,
							mtx_csc, MTX_ON);
					else
						mtx_setting(POST2_MTX,
							mtx_csc, MTX_ON);
				} else {
					mtx_setting(POST2_MTX,
						mtx_csc, MTX_OFF);
					mtx_setting(POST_MTX,
						mtx_csc, MTX_ON);
				}
			}
		}
	}

mode_change:
	if (cur_sdr_process_mode[vd_path] !=
		sdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_SDR)
			pr_csc(1,
			       "am_vecm: vd%d: sdr_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_sdr_process_mode[vd_path],
			       sdr_process_mode[vd_path]);
		if (cur_source_format[vd_path] == HDRTYPE_MVC)
			pr_csc(1,
			       "am_vecm: vd%d: mvc_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_sdr_process_mode[vd_path],
			       sdr_process_mode[vd_path]);
		cur_sdr_process_mode[vd_path] =
			sdr_process_mode[vd_path];
	}
	if (cur_hdr_process_mode[vd_path] !=
		hdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HDR10)
			pr_csc(1,
			       "am_vecm: vd%d: hdr2020_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_hdr_process_mode[vd_path],
			       hdr_process_mode[vd_path]);
		cur_hdr_process_mode[vd_path] =
			hdr_process_mode[vd_path];
	}
	if (cur_hdr_process_mode[vd_path] !=
		hdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HDR10_709)
			pr_csc(1,
			       "am_vecm: vd%d: hdr709_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_hdr_process_mode[vd_path],
			       hdr_process_mode[vd_path]);
		cur_hdr_process_mode[vd_path] =
			hdr_process_mode[vd_path];
	}
	if (cur_hlg_process_mode[vd_path] !=
		hlg_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HLG)
			pr_csc(1,
			       "am_vecm: vd%d: hlg_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_hlg_process_mode[vd_path],
			       hlg_process_mode[vd_path]);
		cur_hlg_process_mode[vd_path] =
			hlg_process_mode[vd_path];
	}
	if (cur_hdr10_plus_process_mode[vd_path] !=
		hdr10_plus_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HDR10PLUS)
			pr_csc(1,
			       "am_vecm: vd%d: hdr10_plus_process_mode %d to %d\n",
			       vd_path + 1,
			       cur_hdr10_plus_process_mode[vd_path],
			       hdr10_plus_process_mode[vd_path]);
		cur_hdr10_plus_process_mode[vd_path] =
			hdr10_plus_process_mode[vd_path];
	}

	if (cur_hlg_plus_process_mode[vd_path]
		!= hlg_plus_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_HLGPLUS)
			pr_csc(1,
				"am_vecm: vd%d: hlg_plus_process_mode %d to %d\n",
				vd_path + 1,
				cur_hlg_plus_process_mode[vd_path],
				hlg_plus_process_mode[vd_path]);

		cur_hlg_plus_process_mode[vd_path] =
			hlg_plus_process_mode[vd_path];
	}

	if (cur_cuva_hdr_process_mode[vd_path] !=
		cuva_hdr_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_CUVA_HDR)
			pr_csc(1,
				"am_vecm: vd%d: cuva_hdr_process_mode %d to %d\n",
				vd_path + 1,
				cur_cuva_hdr_process_mode[vd_path],
				cuva_hdr_process_mode[vd_path]);
		cur_cuva_hdr_process_mode[vd_path] =
			cuva_hdr_process_mode[vd_path];
	}
	if (cur_cuva_hlg_process_mode[vd_path] !=
		cuva_hlg_process_mode[vd_path]) {
		if (cur_source_format[vd_path] == HDRTYPE_CUVA_HLG)
			pr_csc(1,
				"am_vecm: vd%d: cuva_hlg_process_mode %d to %d\n",
				vd_path + 1,
				cur_cuva_hlg_process_mode[vd_path],
				cuva_hlg_process_mode[vd_path]);
		cur_cuva_hlg_process_mode[vd_path] =
			cuva_hlg_process_mode[vd_path];
	}
	if (cur_csc_type[vd_path] != csc_type) {
		pr_csc(1, "am_vecm: vd%d: csc from 0x%x to 0x%x.\n",
		       vd_path + 1, cur_csc_type[vd_path], csc_type);
		if (csc_type == VPP_MATRIX_BT2020YUV_BT2020RGB &&
			vd_path == VD1_PATH)
			hdr_first_frame_flag_update();
		cur_csc_type[vd_path] = csc_type;
	}
}
#endif
