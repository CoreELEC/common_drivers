// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/uaccess.h>
#include "arch/vpp_regs.h"
#include "reg_helper.h"
#include "local_contrast.h"
#include "amve_v2.h"
#include "am_dma_ctrl.h"
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#include <linux/amlogic/media/frc/frc_common.h>
#endif
#include <linux/amlogic/media/amvecm/dnlp_alg.h>

/*curve_node_total*/
#define LC_HIST_SIZE 1632
/*curve_node_total*/
#define LC_CURV_SIZE 585
#define LC_BLK_H_NUM 12
#define LC_BLK_V_NUM 8

int amlc_debug;
int lc_pr_lmt = 0x10;

#define pr_amlc_dbg(fmt, args...)\
	do {\
		if (amlc_debug & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)

int lc_en = 1;
int lc_bitdepth = 10;
int lc_curve_isr_defined;
int use_lc_curve_isr = 1;
int lc_rdma_mode = 1;
int lc_skip_iir;
int lc_demo_mode;
int lc_force_ctrl;
int dbg_monitor_ctrl;
int skip_num = 4;
int width_limit = 1300;
int height_limit = 720;
int lc_reset_done;
int lc_en_chflg = 0xff;
static int lc_flag = 0xff;
static int lc_bypass_flag = 0xff;
int osd_iir_en = 1;
int amlc_iir_debug_en;
/*osd related setting */
int vnum_start_below = 5;
int vnum_end_below = 6;
int vnum_start_above = 1;
int vnum_end_above = 2;
int invalid_blk = 2;
/*u10,7000/21600=0.324*1024=331 */
int min_bv_percent_th = 331;

/*control the refresh speed*/
int alpha1 = 512;
int alpha2 = 512;
int refresh_bit = 12;
int ts = 6;
/*need tuning according to real situation ! (0~512)*/
int scene_change_th = 50;

/*chose block to get hist*/
unsigned int lc_hist_vs;
unsigned int lc_hist_ve;
unsigned int lc_hist_hs;
unsigned int lc_hist_he;
/*lc curve data and hist data*/
int *lc_szcurve;/*12*8*6+9*/
int *lc_szcurve_slice1;/*12*8*6+9*/
int *curve_nodes_cur;
static int *curve_nodes_pre;
static s64 *curve_nodes_pre_raw;
int *lc_hist;/*12*8*17*/
int *lc_hist_slice1;/*12*8*17*/

static bool lc_malloc_ok;

/*print one or more frame data*/
unsigned int lc_hist_prcnt;

unsigned int lc_node_prcnt;
unsigned int lc_node_pos_h;
unsigned int lc_node_pos_v;
unsigned int lc_curve_prcnt;
bool lc_curve_fresh = true;

struct ve_lc_curve_parm_s lc_curve_parm_load;
struct lc_alg_param_s lc_alg_parm;
struct lc_curve_tune_param_s lc_tune_curve;
static struct vd_proc_amvecm_info_t *lc_vd_info;

int lc_pattern_detect_log1;
int lc_read_curve_nodes_changed_en;
int lc_change_curve_nodes_en;
int lc_pattern_detect_log1_cnt;
int lc_special_pattern_detect;

/*lc saturation gain, low parameters*/
/*static unsigned int lc_satur_gain[63] = {
 *	51, 104, 158, 213, 269, 325, 382, 440, 498,
 *	556, 615, 674, 734, 794, 854, 915, 976, 1037,
 *	1099, 1161, 1223, 1286, 1348, 1411, 1475, 1538,
 *	1602, 1666, 1730, 1795, 1859, 1924, 1989, 2054
 *	2120, 2186, 2251, 2318, 2384, 2450, 2517, 2584,
 *	2651, 2718, 2785, 2853, 2921, 2988, 3057, 3125,
 *	3193, 3262, 3330, 3399, 3468, 3537, 3607, 3676,
 *	3746, 3815, 3885, 3955, 4026
 *};
 */

/*lc saturation gain, off parameters*/
static unsigned int lc_satur_off[63] = {
	64, 128, 192, 256, 320, 384, 448, 512, 576, 640,
	704, 768, 832, 896, 960, 1024, 1088, 1152, 1216,
	1280, 1344, 1408, 1472, 1536, 1600, 1664, 1728,
	1792, 1856, 1920, 1984, 2048, 2112, 2176, 2240,
	2304, 2368, 2432, 2496, 2560, 2624, 2688, 2752,
	2816, 2880, 2944, 3008, 3072, 3136, 3200, 3264,
	3328, 3392, 3456, 3520, 3584, 3648, 3712, 3776,
	3840, 3904, 3968, 4032
};

/*lc saturation gain, low parameters*/
/*static unsigned int lc_satur_low[63] = {*/
/*	84, 167, 249, 330, 411, 490, 568, 646, 723, 798,*/
/*	873, 947, 1020, 1092, 1163, 1234, 1303, 1372, 1440,*/
/*	1506, 1572, 1638, 1702, 1766, 1828, 1890, 1951,*/
/*	2011, 2071, 2130, 2187, 2245, 2301, 2356, 2411,*/
/*	2465, 2518, 2571, 2623, 2674, 2724, 2774, 2822,*/
/*	2871, 2919, 2967, 3016, 3065, 3115, 3165, 3217,*/
/*	3271, 3326, 3384, 3443, 3506, 3571, 3639, 3710,*/
/*	3785, 3863, 3945, 4032*/
/*};*/

int tune_curve_en = 2;
int detect_signal_range_en = 2;
int detect_signal_range_threshold_black = 1600;
int detect_signal_range_threshold_white = 3200;

static unsigned int input_422_flag = 1;
int pre_input_422_flag = 0xff;

/*for debug*/
int lc_dbg_flag;
enum lc_reg_lut_e reg_sel;
int lc_temp;
char lc_dbg_curve[100];
int debug_update_dma;

struct aml_lc_drv_param_s lc_drv_param;

struct aml_lc_drv_param_s *lc_drv_param_get(void)
{
	return &lc_drv_param;
}
EXPORT_SYMBOL(lc_drv_param_get);

/*local contrast begin*/
void lc_curve_get_blk_num(int *h_num, int *v_num)
{
	int dwtemp;

	if (!h_num || !v_num)
		return;

	dwtemp = READ_VPP_REG(LC_CURVE_HV_NUM);
	*h_num = (dwtemp >> 8) & 0x1f;
	*v_num = dwtemp & 0x1f;
}

static void lc_mtx_set(enum lc_mtx_sel_e mtx_sel,
	enum lc_mtx_csc_e mtx_csc,
	int mtx_en,
	int bitdepth, int rdma_mode, int vpp_index)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	int coef00_01 = 0;
	int coef02_10 = 0;
	int coef11_12 = 0;
	int coef20_21 = 0;
	int coef22 = 0;
	int clip = 0;
	int offset0_1 = 0;
	int offset2 = 0;
	int pre_offset0_1 = 0;
	int pre_offset2 = 0;

	switch (mtx_sel) {
	case INP_MTX:
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x) {
			matrix_coef00_01 = VPP_YUV2RGB_MAT_0;
			matrix_coef02_10 = VPP_YUV2RGB_MAT_1;
			matrix_coef11_12 = VPP_YUV2RGB_MAT_2;
			matrix_coef20_21 = VPP_YUV2RGB_MAT_3;
			matrix_coef22 = VPP_YUV2RGB_MAT_F;
			matrix_pre_offset0_1 = VPP_LC_YUV2RGB_OFSET;
			matrix_clip = VPP_LC_YUV2RGB_CLIP;
		} else {
			matrix_coef00_01 = SRSHARP1_LC_YUV2RGB_MAT_0_1;
			matrix_coef02_10 = SRSHARP1_LC_YUV2RGB_MAT_2_3;
			matrix_coef11_12 = SRSHARP1_LC_YUV2RGB_MAT_4_5;
			matrix_coef20_21 = SRSHARP1_LC_YUV2RGB_MAT_6_7;
			matrix_coef22 = SRSHARP1_LC_YUV2RGB_MAT_8;
			matrix_pre_offset0_1 = SRSHARP1_LC_YUV2RGB_OFST;
			matrix_clip = SRSHARP1_LC_YUV2RGB_CLIP;
		}
		break;
	case OUTP_MTX:
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x) {
			matrix_coef00_01 = VPP_RGB2YUV_MAT_0;
			matrix_coef02_10 = VPP_RGB2YUV_MAT_1;
			matrix_coef11_12 = VPP_RGB2YUV_MAT_2;
			matrix_coef20_21 = VPP_RGB2YUV_MAT_3;
			matrix_coef22 = VPP_RGB2YUV_MAT_F;
			matrix_offset0_1 = VPP_LC_RGB2YUV_OFSET;
			matrix_clip = VPP_LC_RGB2YUV_CLIP;
		} else {
			matrix_coef00_01 = SRSHARP1_LC_RGB2YUV_MAT_0_1;
			matrix_coef02_10 = SRSHARP1_LC_RGB2YUV_MAT_2_3;
			matrix_coef11_12 = SRSHARP1_LC_RGB2YUV_MAT_4_5;
			matrix_coef20_21 = SRSHARP1_LC_RGB2YUV_MAT_6_7;
			matrix_coef22 = SRSHARP1_LC_RGB2YUV_MAT_8;
			matrix_offset0_1 = SRSHARP1_LC_RGB2YUV_OFST;
			matrix_clip = SRSHARP1_LC_RGB2YUV_CLIP;
		}
		break;
	case STAT_MTX:
		matrix_coef00_01 = LC_STTS_MATRIX_COEF00_01;
		matrix_coef02_10 = LC_STTS_MATRIX_COEF02_10;
		matrix_coef11_12 = LC_STTS_MATRIX_COEF11_12;
		matrix_coef20_21 = LC_STTS_MATRIX_COEF20_21;
		matrix_coef22 = LC_STTS_MATRIX_COEF22;
		matrix_offset0_1 = LC_STTS_MATRIX_OFFSET0_1;
		matrix_offset2 = LC_STTS_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = LC_STTS_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = LC_STTS_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = LC_STTS_CTRL0;
		break;
	default:
		return;
	}

	if (mtx_sel & STAT_MTX) {
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_en_ctrl, mtx_en, 2, 1,
			vpp_index);
		else
			WRITE_VPP_REG_BITS(matrix_en_ctrl, mtx_en, 2, 1);
	}

	if (!mtx_en)
		return;

	switch (mtx_csc) {
	case LC_MTX_RGB_YUV601L:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
				coef00_01 = 0x02040107;
				coef02_10 = 0x1f680064;
				coef11_12 = 0x01c21ed6;
				coef20_21 = 0x1e8701c2;
				coef22 = 0x00001fb7;
				if (bitdepth == 10) {
					offset0_1 = 0x0200040;
					clip = 0x3ff000;
				} else {
					offset0_1 = 0x01000800;
					clip = 0xfff;
				}
			} else {
				coef00_01 =  0x01070204;
				coef02_10 = 0x00641f68;
				coef11_12 = 0x1ed601c2;
				coef20_21 = 0x01c21e87;
				coef22 =  0x00001fb7;
				if (bitdepth == 10) {
					offset0_1 = 0x00400200;
					clip = 0x3ff;
				} else {
					offset0_1 =  0x01000800;
					clip = 0xfff;
				}
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x00bb0275;
			coef02_10 = 0x003f1f99;
			coef11_12 = 0x1ea601c2;
			coef20_21 = 0x01c21e67;
			coef22 = 0x00001fd7;
			offset0_1 =  0x00400200;
			offset2 = 0x00000200;
			pre_offset0_1 = 0x0;
			pre_offset2 = 0x0;
		}
		break;
	case LC_MTX_YUV601L_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
					chip_type_id == chip_t5m ||
					chip_type_id == chip_txhd2) {
					coef00_01 = 0x04A80000;
					coef02_10 = 0x066204a8;
					coef11_12 = 0x1e701cbf;
					coef20_21 = 0x04a80812;
					coef22 = 0x0;
				} else {
					if (chip_type_id == chip_t6d ||
						chip_type_id == chip_t6w ||
						chip_type_id == chip_t6x) {
						coef00_01 = 0x000004A8;
						coef02_10 = 0x04a80662;
						coef11_12 = 0x1cbf1e70;
						coef20_21 = 0x081204a8;
						coef22 = 0x0;
					} else {
						coef00_01 = 0x12a0000;
						coef02_10 = 0x198012a;
						coef11_12 = 0xf9c0f30;
						coef20_21 = 0x12a0204;
						coef22 = 0x0;
					}
				}

				if (chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x) {
					pre_offset0_1 = 0x0200040;
					clip = 0x3ff000;
				} else {
					pre_offset0_1 = 0x00400200;
					clip = 0x3ff;
				}
			} else {
				if (cpu_after_eq_t7()) {
					coef00_01 = 0x04A80000;
					coef02_10 = 0x066204a8;
					coef11_12 = 0x1e701cbf;
					coef20_21 = 0x04a80812;
					coef22 = 0x0;
				} else if (is_meson_tm2_cpu()) {
					coef00_01 = 0x012a0000;
					coef02_10 = 0x198012a;
					coef11_12 = 0xf9c0f30;
					coef20_21 = 0x12a0204;
					coef22 = 0x0;
				} else {
					coef00_01 = 0x04A80000;
					coef02_10 = 0x066204a8;
					coef11_12 = 0x1e701cbf;
					coef20_21 = 0x04a80812;
					coef22 = 0x0;
				}
				pre_offset0_1 = 0x01000800;
				clip = 0xfff;
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x04A80000;
			coef02_10 = 0x072C04A8;
			coef11_12 = 0x1F261DDD;
			coef20_21 = 0x04A80876;
			coef22 = 0x0;
			offset0_1 = 0x0;
			offset2 = 0x0;
			pre_offset0_1 = 0x7c00600;
			pre_offset2 = 0x00000600;
		}
		break;
	case LC_MTX_RGB_YUV709L:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
				coef00_01 = 0x027500bb;
				coef02_10 = 0x1f99003f;
				coef11_12 = 0x01c21ea6;
				coef20_21 = 0x1e6701c2;
				coef22 = 0x00001fd7;
				if (bitdepth == 10) {
					offset0_1 = 0x200040;
					clip = 0x3ff000;
				} else {
					offset0_1 = 0x01000800;
					clip = 0xfff;
				}
			} else {
				coef00_01 = 0x00bb0275;
				coef02_10 = 0x003f1f99;
				coef11_12 = 0x1ea601c2;
				coef20_21 = 0x01c21e67;
				coef22 = 0x00001fd7;
				if (bitdepth == 10) {
					offset0_1 = 0x00400200;
					clip = 0x3ff;
				} else {
					offset0_1 = 0x01000800;
					clip = 0xfff;
				}
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x00bb0275;
			coef02_10 = 0x003f1f99;
			coef11_12 = 0x1ea601c2;
			coef20_21 = 0x01c21e67;
			coef22 = 0x00001fd7;
			offset0_1 = 0x00400200;
			offset2 = 0x00000200;
			pre_offset0_1 = 0x0;
			pre_offset2 = 0x0;
		}
		break;
	case LC_MTX_YUV709L_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
					chip_type_id == chip_t5m ||
					chip_type_id == chip_txhd2) {
					coef00_01 = 0x04A80000;
					coef02_10 = 0x072C04A8;
					coef11_12 = 0x1F261DDD;
					coef20_21 = 0x04A80876;
					coef22 = 0x0;
				} else {
					if (chip_type_id == chip_t6d ||
						chip_type_id == chip_t6w ||
						chip_type_id == chip_t6x) {
						coef00_01 = 0x000004A8;
						coef02_10 = 0x04A8072C;
						coef11_12 = 0x1DDD1F26;
						coef20_21 = 0x087604A8;
						coef22 = 0x0;
					} else {
						coef00_01 = 0x012a0000;
						coef02_10 = 0x01cb012a;
						coef11_12 = 0x1fc90f77;
						coef20_21 = 0x012a021d;
						coef22 = 0x0;
					}
				}
				if (chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x) {
					pre_offset0_1 = 0x200040;
					clip = 0x3ff000;
				} else {
					pre_offset0_1 = 0x400200;
					clip = 0x3ff;
				}
			} else {
				coef00_01 = 0x04A80000;
				coef02_10 = 0x072C04A8;
				coef11_12 = 0x1F261DDD;
				coef20_21 = 0x04A80876;
				coef22 = 0x0;

				pre_offset0_1 = 0x01000800;
				clip = 0xfff;
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x04A80000;
			coef02_10 = 0x072C04A8;
			coef11_12 = 0x1F261DDD;
			coef20_21 = 0x04A80876;
			coef22 = 0x0;
			offset0_1 = 0x0;
			offset2 = 0x0;
			pre_offset0_1 = 0x7c00600;
			pre_offset2 = 0x00000600;
		}
		break;
	case LC_MTX_RGB_YUV709:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
				coef00_01 = 0x02dc00da;
				coef02_10 = 0x1f8a004a;
				coef11_12 = 0x02001e76;
				coef20_21 = 0x1e2f0200;
				coef22 = 0x00001fd1;
				if (bitdepth == 10) {
					offset0_1 = 0x200000;
					clip = 0x3ff000;
				} else {
					offset0_1 = 0x800;
					clip = 0xfff;
				}
			} else {
				coef00_01 = 0x00da02dc;
				coef02_10 = 0x004a1f8a;
				coef11_12 = 0x1e760200;
				coef20_21 = 0x02001e2f;
				coef22 = 0x00001fd1;
				if (bitdepth == 10) {
					offset0_1 = 0x200;
					clip = 0x3ff;
				} else {
					offset0_1 = 0x800;
					clip = 0xfff;
				}
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x00bb0275;
			coef02_10 = 0x003f1f99;
			coef11_12 = 0x1ea601c2;
			coef20_21 = 0x01c21e67;
			coef22 = 0x00001fd7;
			offset0_1 = 0x00400200;
			offset2 = 0x00000200;
			pre_offset0_1 = 0x0;
			pre_offset2 = 0x0;
		}
		break;
	case LC_MTX_YUV709_RGB:
		if (mtx_sel & (INP_MTX | OUTP_MTX)) {
			if (bitdepth == 10) {
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
					chip_type_id == chip_t5m ||
					chip_type_id == chip_txhd2) {
					coef00_01 = 0x04000000;
					coef02_10 = 0x064d0400;
					coef11_12 = 0x1f411e21;
					coef20_21 = 0x0400076d;
					coef22 = 0x0;
				} else {
					if (chip_type_id == chip_t6d ||
						chip_type_id == chip_t6w ||
						chip_type_id == chip_t6x) {
						coef00_01 = 0x00000400;
						coef02_10 = 0x0400064d;
						coef11_12 = 0x1e211f41;
						coef20_21 = 0x076d0400;
						coef22 =  0x0;
					} else {
						coef00_01 = 0x01000000;
						coef02_10 = 0x01930100;
						coef11_12 = 0x1fd01f88;
						coef20_21 = 0x010001db;
						coef22 = 0x0;
					}
				}
				if (chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
					pre_offset0_1 = 0x200000;
					clip = 0x3ff000;
				} else {
					pre_offset0_1 = 0x200;
					clip = 0x3ff;
				}
			} else {
				if (cpu_after_eq_t7()) {
					coef00_01 = 0x04000000;
					coef02_10 = 0x064d0400;
					coef11_12 = 0x1f411e21;
					coef20_21 = 0x0400076d;
					coef22 = 0x0;
				} else if (is_meson_tm2_cpu()) {
					coef00_01 = 0x01000000;
					coef02_10 = 0x01930100;
					coef11_12 = 0x1fd01f88;
					coef20_21 = 0x010001db;
					coef22 = 0x0;
				} else {
					coef00_01 = 0x04000000;
					coef02_10 = 0x064d0400;
					coef11_12 = 0x1f411e21;
					coef20_21 = 0x0400076d;
					coef22 = 0x0;
				}
				pre_offset0_1 = 0x800;
				clip = 0xfff;
			}
		} else if (mtx_sel & STAT_MTX) {
			coef00_01 = 0x04000000;
			coef02_10 = 0x064d0400;
			coef11_12 = 0x1f411e21;
			coef20_21 = 0x0400076d;
			coef22 = 0x0;
			offset0_1 = 0x0;
			offset2 = 0x0;
			pre_offset0_1 = 0x00000600;
			pre_offset2 =  0x00000600;
		}
		break;
	case LC_MTX_NULL:
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
			coef00_01 = 0x00000400;
			coef02_10 = 0x0;
			coef11_12 = 0x00000400;
			coef20_21 = 0x0;
			coef22 = 0x400;
			offset0_1 = 0x0;
		} else {
			coef00_01 = 0x04000000;
			coef02_10 = 0x0;
			coef11_12 = 0x04000000;
			coef20_21 = 0x0;
			coef22 = 0x400;
			offset0_1 = 0x0;
		}
		if (mtx_sel & STAT_MTX) {
			offset2 = 0x0;
			pre_offset0_1 = 0x0;
			pre_offset2 = 0x0;
		}
		break;
	default:
		break;
	}

	if (rdma_mode) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01, coef00_01, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10, coef02_10, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12, coef11_12, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21, coef20_21, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, coef22, vpp_index);

		if (mtx_sel == INP_MTX) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, pre_offset0_1,
			vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_clip, clip, vpp_index);
		} else if (mtx_sel == OUTP_MTX) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, offset0_1,
				vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_clip, clip, vpp_index);
		} else if (mtx_sel == STAT_MTX) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, offset0_1,
				vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, offset2,
				vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, pre_offset0_1,
				vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, pre_offset2,
				vpp_index);
		}
	} else {
		WRITE_VPP_REG(matrix_coef00_01, coef00_01);
		WRITE_VPP_REG(matrix_coef02_10, coef02_10);
		WRITE_VPP_REG(matrix_coef11_12, coef11_12);
		WRITE_VPP_REG(matrix_coef20_21, coef20_21);
		WRITE_VPP_REG(matrix_coef22, coef22);

		if (mtx_sel == INP_MTX) {
			WRITE_VPP_REG(matrix_pre_offset0_1, pre_offset0_1);
			WRITE_VPP_REG(matrix_clip, clip);
		} else if (mtx_sel == OUTP_MTX) {
			WRITE_VPP_REG(matrix_offset0_1, offset0_1);
			WRITE_VPP_REG(matrix_clip, clip);
		} else if (mtx_sel == STAT_MTX) {
			WRITE_VPP_REG(matrix_offset0_1, offset0_1);
			WRITE_VPP_REG(matrix_offset2, offset2);
			WRITE_VPP_REG(matrix_pre_offset0_1, pre_offset0_1);
			WRITE_VPP_REG(matrix_pre_offset2, pre_offset2);
		}
	}
}

static void lc_stts_blk_config(int enable,
			       unsigned int height, unsigned int width,
			       int rdma_mode, int vpp_index)
{
	int h_num, v_num;
	int blk_height,  blk_width;
	int row_start, col_start;
	int data32;
	int hend0, hend1, hend2, hend3, hend4, hend5, hend6;
	int hend7, hend8, hend9, hend10, hend11;
	int vend0, vend1, vend2, vend3, vend4, vend5, vend6, vend7;

	row_start = 0;
	col_start = 0;
	h_num = 12;
	v_num = 8;
	blk_height = height / v_num;
	blk_width = width / h_num;

	hend0 = col_start + blk_width - 1;
	hend1 = hend0 + blk_width;
	hend2 = hend1 + blk_width;
	hend3 = hend2 + blk_width;
	hend4 = hend3 + blk_width;
	hend5 = hend4 + blk_width;
	hend6 = hend5 + blk_width;
	hend7 = hend6 + blk_width;
	hend8 = hend7 + blk_width;
	hend9 = hend8 + blk_width;
	hend10 = hend9 + blk_width;
	hend11 = width - 1;

	vend0 = row_start + blk_height - 1;
	vend1 = vend0 + blk_height;
	vend2 = vend1 + blk_height;
	vend3 = vend2 + blk_height;
	vend4 = vend3 + blk_height;
	vend5 = vend4 + blk_height;
	vend6 = vend5 + blk_height;
	vend7 = height - 1;

	if (rdma_mode) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_HIST_REGION_IDX,
			0, 16, 5, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((((row_start & 0x1fff) << 16) & 0xffff0000) |
			(col_start & 0x1fff)), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((hend1 & 0x1fff) << 16) | (hend0 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((vend1 & 0x1fff) << 16) | (vend0 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((hend3 & 0x1fff) << 16) | (hend2 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((vend3 & 0x1fff) << 16) | (vend2 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			      ((hend5 & 0x1fff) << 16) | (hend4 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((vend5 & 0x1fff) << 16) | (vend4 & 0x1fff), vpp_index);

		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			      ((hend7 & 0x1fff) << 16) | (hend6 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((vend7 & 0x1fff) << 16) | (vend6 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((hend9 & 0x1fff) << 16) | (hend8 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION,
			((hend11 & 0x1fff) << 16) | (hend10 & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_HIST_SET_REGION, h_num, vpp_index);
	} else {
		data32 = READ_VPP_REG(LC_STTS_HIST_REGION_IDX);
		WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, 0xffe0ffff & data32);
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((((row_start & 0x1fff) << 16) & 0xffff0000) |
			(col_start & 0x1fff)));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((hend1 & 0x1fff) << 16) | (hend0 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((vend1 & 0x1fff) << 16) | (vend0 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((hend3 & 0x1fff) << 16) | (hend2 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((vend3 & 0x1fff) << 16) | (vend2 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
				  ((hend5 & 0x1fff) << 16) | (hend4 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((vend5 & 0x1fff) << 16) | (vend4 & 0x1fff));

		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
				  ((hend7 & 0x1fff) << 16) | (hend6 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((vend7 & 0x1fff) << 16) | (vend6 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((hend9 & 0x1fff) << 16) | (hend8 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION,
			((hend11 & 0x1fff) << 16) | (hend10 & 0x1fff));
		WRITE_VPP_REG(LC_STTS_HIST_SET_REGION, h_num);
	}
}

static void lc_stts_en(int enable,
	unsigned int height,
	unsigned int width,
	int pix_drop_mode,
	int eol_en,
	int hist_mode,
	int lpf_en,
	int din_sel,
	int bitdepth,
	int flag,
	int flag_full,
	int rdma_mode, int vpp_index)
{
	int data32;

	if (flag == 0x3) {
		lc_mtx_set(STAT_MTX, LC_MTX_YUV601L_RGB,
			enable, bitdepth, rdma_mode, vpp_index);
	} else {
		if (flag_full == 1)
			lc_mtx_set(STAT_MTX, LC_MTX_YUV709_RGB,
				enable, bitdepth, rdma_mode, vpp_index);
		else
			lc_mtx_set(STAT_MTX, LC_MTX_YUV709L_RGB,
				enable, bitdepth, rdma_mode, vpp_index);
	}

	if (rdma_mode) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_GCLK_CTRL0, 0x0, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(LC_STTS_WIDTHM1_HEIGHTM1,
			((width - 1) << 16) | (height - 1), vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_HIST_REGION_IDX,
			(pix_drop_mode & 0x3) << 1 | (eol_en & 0x1), 28, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_HIST_REGION_IDX,
			(hist_mode & 0x3) << 1 | (lpf_en & 0x1), 21, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_CTRL0, din_sel, 3, 3,
			vpp_index);
		/*lc input probe enable*/
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_CTRL0, 1, 10, 1, vpp_index);
		/*lc hist stts enable*/
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_HIST_REGION_IDX, enable, 31, 1,
			vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_BLACK_INFO,
			lc_tune_curve.lc_reg_thd_black,
			0, 8, vpp_index);
	} else {
		WRITE_VPP_REG(LC_STTS_GCLK_CTRL0, 0x0);
		WRITE_VPP_REG(LC_STTS_WIDTHM1_HEIGHTM1,
			((width - 1) << 16) | (height - 1));
		data32 = 0x80000000 |  ((pix_drop_mode & 0x3) << 29);
		data32 = data32 | ((eol_en & 0x1) << 28);
		data32 = data32 | ((hist_mode & 0x3) << 22);
		data32 = data32 | ((lpf_en & 0x1) << 21);
		WRITE_VPP_REG(LC_STTS_HIST_REGION_IDX, data32);

		WRITE_VPP_REG_BITS(LC_STTS_CTRL0, din_sel, 3, 3);
		/*lc input probe enable*/
		WRITE_VPP_REG_BITS(LC_STTS_CTRL0, 1, 10, 1);

		/*lc hist stts enable*/
		WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, enable, 31, 1);
		WRITE_VPP_REG_BITS(LC_STTS_BLACK_INFO,
			lc_tune_curve.lc_reg_thd_black,
			0, 8);
	}
}

static void lc_curve_ctrl_config(int enable,
	unsigned int height, unsigned int width, int rdma_mode, int vpp_index)
{
	unsigned int lc_histvld_thrd;
	unsigned int lc_blackbar_mute_thrd;
	unsigned int lmtrat_minmax;
	int h_num, v_num;

	h_num = 12;
	v_num = 8;

	lmtrat_minmax = (READ_VPP_REG(LC_CURVE_LMT_RAT) >> 8) & 0xff;
	lc_histvld_thrd =  ((height * width) /
		(h_num * v_num) * lmtrat_minmax) >> 10;
	lc_blackbar_mute_thrd = ((height * width) / (h_num * v_num)) >> 3;

	if (rdma_mode) {
		if (!enable) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_CURVE_CTRL, enable, 0, 1,
				vpp_index);
		} else {
			VSYNC_WRITE_VPP_REG_VPP_SEL(LC_CURVE_HV_NUM,
				(h_num << 8) | v_num, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(LC_CURVE_HISTVLD_THRD,
				lc_histvld_thrd, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(LC_CURVE_BB_MUTE_THRD,
				lc_blackbar_mute_thrd, vpp_index);

			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_CURVE_CTRL, enable, 0, 1,
				vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_CURVE_CTRL, enable, 31, 1,
				vpp_index);
		}
	} else {
		if (!enable) {
			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
		} else {
			WRITE_VPP_REG(LC_CURVE_HV_NUM, (h_num << 8) | v_num);
			WRITE_VPP_REG(LC_CURVE_HISTVLD_THRD, lc_histvld_thrd);
			WRITE_VPP_REG(LC_CURVE_BB_MUTE_THRD, lc_blackbar_mute_thrd);

			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 0, 1);
			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, enable, 31, 1);
		}
	}
}

static void lc_blk_bdry_config(unsigned int height, unsigned int width,
	int rdma_mode, int vpp_index)
{
	unsigned int value;
	unsigned int reg_hid0 = SRSHARP1_LC_CURVE_BLK_HIDX_0_1;
	unsigned int reg_hid1 = SRSHARP1_LC_CURVE_BLK_HIDX_2_3;
	unsigned int reg_hid2 = SRSHARP1_LC_CURVE_BLK_HIDX_4_5;
	unsigned int reg_hid3 = SRSHARP1_LC_CURVE_BLK_HIDX_6_7;
	unsigned int reg_hid4 = SRSHARP1_LC_CURVE_BLK_HIDX_8_9;
	unsigned int reg_hid5 = SRSHARP1_LC_CURVE_BLK_HIDX_10_11;
	unsigned int reg_hid6 = SRSHARP1_LC_CURVE_BLK_HIDX_12;

	unsigned int reg_vid0 = SRSHARP1_LC_CURVE_BLK_VIDX_0_1;
	unsigned int reg_vid1 = SRSHARP1_LC_CURVE_BLK_VIDX_2_3;
	unsigned int reg_vid2 = SRSHARP1_LC_CURVE_BLK_VIDX_4_5;
	unsigned int reg_vid3 = SRSHARP1_LC_CURVE_BLK_VIDX_6_7;
	unsigned int reg_vid4 = SRSHARP1_LC_CURVE_BLK_VIDX_8;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
		reg_hid0 = VPP_LC_BLK_HIDX_0;
		reg_hid1 = VPP_LC_BLK_HIDX_1;
		reg_hid2 = VPP_LC_BLK_HIDX_2;
		reg_hid3 = VPP_LC_BLK_HIDX_3;
		reg_hid4 = VPP_LC_BLK_HIDX_4;
		reg_hid5 = VPP_LC_BLK_HIDX_5;
		reg_hid6 = VPP_LC_BLK_HIDX_F;

		reg_vid0 = VPP_LC_BLK_VIDX_0;
		reg_vid1 = VPP_LC_BLK_VIDX_1;
		reg_vid2 = VPP_LC_BLK_VIDX_2;
		reg_vid3 = VPP_LC_BLK_VIDX_3;
		reg_vid4 = VPP_LC_BLK_VIDX_F;
	}

	width /= 12;
	height /= 8;

	if (rdma_mode) {
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
			value = (0 << 16) & GET_BITS(0, 14);
			value |= (width << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid0, value, vpp_index);
			value = (width * 2) & GET_BITS(0, 14);
			value |= ((width * 3) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid1, value, vpp_index);
			value = (width * 4) & GET_BITS(0, 14);
			value |= ((width * 5) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid2, value, vpp_index);
			value = (width * 6) & GET_BITS(0, 14);
			value |= ((width * 7) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid3, value, vpp_index);
			value = (width * 8) & GET_BITS(0, 14);
			value |= ((width * 9) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid4, value, vpp_index);
			value = (width * 10) & GET_BITS(0, 14);
			value |= ((width * 11) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid5, value, vpp_index);
			value = (width * 12) & GET_BITS(0, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid6, value, vpp_index);

			value = (0 << 16) & GET_BITS(0, 14);
			value |= (height << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid0, value, vpp_index);
			value = (height * 2) & GET_BITS(0, 14);
			value |= ((height * 3) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid1, value, vpp_index);
			value = (height * 4) & GET_BITS(0, 14);
			value |= ((height * 5) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid2, value, vpp_index);
			value = (height * 6) & GET_BITS(0, 14);
			value |= ((height * 7) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid3, value, vpp_index);
			value = (height * 8) & GET_BITS(0, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid4, value, vpp_index);
		} else {
				/*lc curve mapping block IDX default 4k panel*/
			value = width & GET_BITS(0, 14);
			value |= (0 << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid0, value, vpp_index);
			value = (width * 3) & GET_BITS(0, 14);
			value |= ((width * 2) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid1, value, vpp_index);
			value = (width * 5) & GET_BITS(0, 14);
			value |= ((width * 4) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid2, value, vpp_index);
			value = (width * 7) & GET_BITS(0, 14);
			value |= ((width * 6) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid3, value, vpp_index);
			value = (width * 9) & GET_BITS(0, 14);
			value |= ((width * 8) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid4, value, vpp_index);
			value = (width * 11) & GET_BITS(0, 14);
			value |= ((width * 10) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid5, value, vpp_index);
			value = width & GET_BITS(0, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_hid6, value, vpp_index);

			value = height & GET_BITS(0, 14);
			value |= (0 << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid0, value, vpp_index);
			value = (height * 3) & GET_BITS(0, 14);
			value |= ((height * 2) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid1, value, vpp_index);
			value = (height * 5) & GET_BITS(0, 14);
			value |= ((height * 4) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid2, value, vpp_index);
			value = (height * 7) & GET_BITS(0, 14);
			value |= ((height * 6) << 16) & GET_BITS(16, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid3, value, vpp_index);
			value = height & GET_BITS(0, 14);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_vid4, value, vpp_index);
		}
	} else {
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
			value = (0 << 16) & GET_BITS(0, 14);
			value |= (width << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid0, value);
			value = (width * 2) & GET_BITS(0, 14);
			value |= ((width * 3) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid1, value);
			value = (width * 4) & GET_BITS(0, 14);
			value |= ((width * 5) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid2, value);
			value = (width * 6) & GET_BITS(0, 14);
			value |= ((width * 7) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid3, value);
			value = (width * 8) & GET_BITS(0, 14);
			value |= ((width * 9) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid4, value);
			value = (width * 10) & GET_BITS(0, 14);
			value |= ((width * 11) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid5, value);
			value = (width * 12) & GET_BITS(0, 14);
			WRITE_VPP_REG(reg_hid6, value);

			value = (0 << 16) & GET_BITS(0, 14);
			value |= (height << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid0, value);
			value = (height * 2) & GET_BITS(0, 14);
			value |= ((height * 3) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid1, value);
			value = (height * 4) & GET_BITS(0, 14);
			value |= ((height * 5) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid2, value);
			value = (height * 6) & GET_BITS(0, 14);
			value |= ((height * 7) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid3, value);
			value = (height * 8) & GET_BITS(0, 14);
			WRITE_VPP_REG(reg_vid4, value);
		} else {
			value = width & GET_BITS(0, 14);
			value |= (0 << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid0, value);
			value = (width * 3) & GET_BITS(0, 14);
			value |= ((width * 2) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid1, value);
			value = (width * 5) & GET_BITS(0, 14);
			value |= ((width * 4) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid2, value);
			value = (width * 7) & GET_BITS(0, 14);
			value |= ((width * 6) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid3, value);
			value = (width * 9) & GET_BITS(0, 14);
			value |= ((width * 8) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid4, value);
			value = (width * 11) & GET_BITS(0, 14);
			value |= ((width * 10) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_hid5, value);
			value = width & GET_BITS(0, 14);
			WRITE_VPP_REG(reg_hid6, value);

			value = height & GET_BITS(0, 14);
			value |= (0 << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid0, value);
			value = (height * 3) & GET_BITS(0, 14);
			value |= ((height * 2) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid1, value);
			value = (height * 5) & GET_BITS(0, 14);
			value |= ((height * 4) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid2, value);
			value = (height * 7) & GET_BITS(0, 14);
			value |= ((height * 6) << 16) & GET_BITS(16, 14);
			WRITE_VPP_REG(reg_vid3, value);
			value = height & GET_BITS(0, 14);
			WRITE_VPP_REG(reg_vid4, value);
		}
	}
}

static void lc_top_config(int enable, int h_num, int v_num,
	unsigned int height, unsigned int width, int bitdepth,
	int flag, int flag_full, int rdma_mode, int vpp_index)
{
	unsigned int lc_input_sel_y = 6;
	unsigned int lc_input_sel_c = 6;
	unsigned int lc_sr_in = 0;

	pr_amlc_dbg("[%s]:enable/h_num/v_num/height/width: %d/%d/%d/%d/%d\n",
		__func__, enable, h_num, v_num, height, width);
	pr_amlc_dbg("[%s]:bitdepth/flag/flag_full: %d/%d/%d\n",
		__func__, bitdepth, flag, flag_full);

	if (chip_type_id != chip_t3x) {
		/*lc curve mapping  config*/
		lc_blk_bdry_config(height, width, rdma_mode, vpp_index);
		if (flag == 0x3) {
			/* bt601 use 601 matrix */
			lc_mtx_set(INP_MTX, LC_MTX_YUV601L_RGB, 1, bitdepth,
				rdma_mode, vpp_index);
			lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV601L, 1, bitdepth,
				rdma_mode, vpp_index);
		} else {
			/* all other cases use 709 by default */
			/* to do, should we handle bg2020 separately? */
			/* for special signal, keep full range to avoid clipping */
			if (flag_full == 1) {
				lc_mtx_set(INP_MTX, LC_MTX_YUV709_RGB, 1, bitdepth,
					rdma_mode, vpp_index);
				lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709, 1, bitdepth,
					rdma_mode, vpp_index);
			} else {
				lc_mtx_set(INP_MTX, LC_MTX_YUV709L_RGB, 1, bitdepth,
					rdma_mode, vpp_index);
				lc_mtx_set(OUTP_MTX, LC_MTX_RGB_YUV709L, 1, bitdepth,
					rdma_mode, vpp_index);
			}
		}

		if (chip_type_id >= chip_t6d) {
			if (input_422_flag) {
				//lc_input_sel_y = 5;
				//lc_input_sel_c = 5;
				lc_sr_in = 0;
			} else {
				//lc_input_sel_y = 6;
				//lc_input_sel_c = 6;
				lc_sr_in = 1;
			}
		}

		if (rdma_mode) {
			if (chip_type_id >= chip_t6d) {
				/*lcinput_ysel*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_INPUT_SEL,
					lc_input_sel_y, 4, 3, vpp_index);
				/*lcinput_csel*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_INPUT_SEL,
					lc_input_sel_c, 0, 3, vpp_index);
				/*lc ram write h num*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_BLK_HV_NUM, h_num,
					8, 8, vpp_index);
				/*lc ram write v num*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_BLK_HV_NUM, v_num,
					0, 8, vpp_index);
				/*lc blend mode,default 1*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_MODE, 1, 0, 1,
					vpp_index);
				/*LC sync ctl*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_MODE,
					lc_sr_in, 8, 1, vpp_index);
				/*lc enable need set at last*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_MODE, enable, 4, 1,
					vpp_index);
			} else {
				/*lcinput_ysel*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_INPUT_MUX,
					5, 4, 3, vpp_index);
				/*lcinput_csel*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_INPUT_MUX,
					5, 0, 3, vpp_index);
				/*lc ram write h num*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_HV_NUM,
					h_num, 8, 5, vpp_index);
				/*lc ram write v num*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_HV_NUM,
					v_num, 0, 5, vpp_index);
				/*lc hblank*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_TOP_CTRL,
					8, 8, 8, vpp_index);
				/*lc blend mode,default 1*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_TOP_CTRL,
					1, 0, 1, vpp_index);
				/*LC sync ctl*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_TOP_CTRL,
					0, 16, 1, vpp_index);
				/*lc enable need set at last*/
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_LC_TOP_CTRL,
					enable, 4, 1, vpp_index);
			}
		} else {
			if (chip_type_id >= chip_t6d) {
				/*lcinput_ysel*/
				WRITE_VPP_REG_BITS(VPP_LC_INPUT_SEL,
					lc_input_sel_y, 4, 3);
				/*lcinput_csel*/
				WRITE_VPP_REG_BITS(VPP_LC_INPUT_SEL,
					lc_input_sel_c, 0, 3);
				/*lc ram write h num*/
				WRITE_VPP_REG_BITS(VPP_LC_BLK_HV_NUM, h_num, 8, 8);
				/*lc ram write v num*/
				WRITE_VPP_REG_BITS(VPP_LC_BLK_HV_NUM, v_num, 0, 8);
				/*lc hblank*/
	//			WRITE_VPP_REG_BITS(VPP_LC_MODE, 8, 8, 8);
				/*lc blend mode,default 1*/
				WRITE_VPP_REG_BITS(VPP_LC_MODE, 1, 0, 1);
				/*LC sync ctl*/
				WRITE_VPP_REG_BITS(VPP_LC_MODE,
					lc_sr_in, 8, 1);
				/*lc enable need set at last*/
				WRITE_VPP_REG_BITS(VPP_LC_MODE, enable, 4, 1);
			} else {
				/*lcinput_ysel*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 4, 3);
				/*lcinput_csel*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_INPUT_MUX, 5, 0, 3);
				/*lc ram write h num*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, h_num, 8, 5);
				/*lc ram write v num*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_HV_NUM, v_num, 0, 5);
				/*lc hblank*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 8, 8, 8);
				/*lc blend mode,default 1*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 1, 0, 1);
				/*LC sync ctl*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, 0, 16, 1);
				/*lc enable need set at last*/
				WRITE_VPP_REG_BITS(SRSHARP1_LC_TOP_CTRL, enable, 4, 1);
			}
		}
	} else {
		ve_lc_top_cfg(enable, h_num, v_num,
			height, width, bitdepth, 1, 0, rdma_mode, vpp_index);
	}
}

static void lc_dma_config(int enable, int rdma_mode, int vpp_index)
{
	unsigned int reg_val = 0;

	if (chip_type_id != chip_t6x)
		return;

	if (flag_cm_lc_dma_en & 0x2) {
		dma_lc_en = enable;
		reg_val = enable ? 0x3 : 0;
		if (rdma_mode) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_STTS_CTRL0, reg_val, 22, 2,
				vpp_index);//lc_dma_en
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(LC_CURVE_CTRL, reg_val, 16, 2,
				vpp_index);//curve dma en
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VIU_MISC_CTRL0, reg_val, 3, 2,
				vpp_index);
		} else {
			WRITE_VPP_REG_BITS(LC_STTS_CTRL0, reg_val, 22, 2);//lc_dma_en
			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, reg_val, 16, 2);//curve dma en
			WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, reg_val, 3, 2);
		}
		am_dma_update_mif_wr_cm_lc(rdma_mode);
	}
}

void lc_disable(int rdma_mode, int vpp_index)
{
	unsigned int reg_ctrl = SRSHARP1_LC_TOP_CTRL;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		reg_ctrl = VPP_LC_MODE;

	if (chip_type_id != chip_t3x) {
		lc_dma_config(0, rdma_mode, vpp_index);
		if (rdma_mode) {
			/*lc enable need set at last*/
			VSYNC_WRITE_VPP_REG_BITS(reg_ctrl, 0, 4, 1);
			VSYNC_WRITE_VPP_REG_BITS(LC_CURVE_CTRL, 0, 0, 1);
			VSYNC_WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);
			/*lc hist stts enable*/
			VSYNC_WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 0, 31, 1);
		} else {
			WRITE_VPP_REG_BITS(reg_ctrl, 0, 4, 1);
			WRITE_VPP_REG_BITS(LC_CURVE_CTRL, 0, 0, 1);
			WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);
			/*lc hist stts enable*/
			WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 0, 31, 1);
		}
	} else {
		ve_lc_disable(rdma_mode, vpp_index);
		lc_reset_done = 0;
	}

	if (!lc_malloc_ok) {
		lc_en_chflg = 0x0;
		return;
	}

	memset(lc_hist, 0, LC_HIST_SIZE * sizeof(int));
	memset(curve_nodes_cur, 0, LC_CURV_SIZE * sizeof(int));
	memset(curve_nodes_pre, 0, LC_CURV_SIZE * sizeof(int));
	memset(curve_nodes_pre_raw, 0, LC_CURV_SIZE * sizeof(int64_t));

	if (chip_type_id == chip_t3x) {
		memset(lc_hist_slice1, 0, LC_HIST_SIZE * sizeof(int));
		memset(lc_szcurve_slice1, 0, LC_CURV_SIZE * sizeof(int));
	} else {
		memset(lc_szcurve, 0, LC_CURV_SIZE * sizeof(int));
	}

	lc_en_chflg = 0x0;
}

/* detect super white and super black currently */
static int signal_detect(unsigned short *hist)
{
	unsigned short bin0, bin1, bin62, bin63;

	bin0 = hist[0];
	bin1 = hist[1];
	bin62 = hist[62];
	bin63 = hist[63];

	if (amlc_debug == 0xe) {
		pr_info("bin0=%d bin1=%d bin62=%d bin63=%d\n",
			bin0, bin1, bin62, bin63);
		amlc_debug = 0x0;
	}

	if (((bin0 + bin1) > detect_signal_range_threshold_black) ||
		((bin62 + bin63) > detect_signal_range_threshold_white))
		return 1;

	return 0;
}

void lc_config(int enable,
	struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en,
	unsigned int sps_w_in,
	unsigned int sps_h_in,
	int bitdepth,
	int vpp_index,
	struct vpp_hist_param_s *vp)
{
	int h_num, v_num;
	unsigned int height, width;
	static unsigned int height_pre, width_pre;
	static unsigned int vf_height, vf_width, flag_full_pre;
	static unsigned int sps_w_in_pre, sps_h_in_pre;
	unsigned int flag, flag_full;
	int lc_en_ctrl = enable;
	int in_sel;
	unsigned int limit_full = 0;
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;

	h_num = LC_BLK_H_NUM;
	v_num = LC_BLK_V_NUM;

	if (!vf) {
		vf_height = 0;
		vf_width = 0;
		return;
	}

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	if (h_rate)
		src_h = vf->height / (h_rate + 1);
	else
		src_h = vf->height;

	if (w_rate)
		src_w = vf->width / (w_rate + 1);
	else
		src_w = vf->width;

	if (chip_type_id == chip_t3x &&
		vf->source_type == VFRAME_SOURCE_TYPE_HWC) {
		vf_height = 0;
		vf_width = 0;
		pr_amlc_dbg("%s: VFRAME_SOURCE_TYPE_HWC\n", __func__);
		return;
	}

	/* try to detect out of spec signal level */
	flag_full = 0;
	limit_full = (vf->signal_type >> 25) & 0x01;
	if (detect_signal_range_en == 2 &&
		chip_type_id != chip_t3x) {
		flag_full = signal_detect(vp->vpp_gamma);
		if (vf->type & VIDTYPE_RGB_444 ||
			limit_full)
			flag_full = 1;
	} else {
		flag_full = detect_signal_range_en;
	}

	if ((chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w || chip_type_id == chip_t6x) &&
		lc_vd_info) {
		sps_h_in = lc_vd_info->vd1_in_vsize;
		sps_w_in = lc_vd_info->vd1_in_hsize;
		height = lc_vd_info->vd1_dout_vsize;
		width = lc_vd_info->vd1_dout_hsize;
	} else {
		height = sps_h_in << sps_v_en;
		width = sps_w_in << sps_h_en;
	}

	if (flag_full != flag_full_pre ||
		sps_h_in_pre != sps_h_in ||
		sps_w_in_pre != sps_w_in ||
		width_pre != width ||
		height_pre != height) {
		pr_amlc_dbg("signal/sps_w_in/sps_h_in changed,\n");
		pr_amlc_dbg("flag_full:%d->%d sps_w_in:%d->%d sps_h_in:%d->%d\n",
			flag_full_pre,
			flag_full,
			sps_w_in_pre,
			sps_w_in,
			sps_h_in_pre,
			sps_h_in);
	}

	if (vf_height == src_h &&
		vf_width == src_w &&
		flag_full == flag_full_pre &&
		sps_w_in_pre == sps_w_in &&
		sps_h_in_pre == sps_h_in &&
		width_pre == width &&
		height_pre == height &&
		lc_en_chflg && !lc_slice_num_changed &&
		input_422_flag == pre_input_422_flag) {
		/*pr_amlc_dbg("%s: lc_en_chflg = %d\n",*/
		/*	__func__, lc_en_chflg);*/
		/*pr_amlc_dbg("%s: lc_slice_num_changed = %d\n",*/
		/*	__func__, lc_slice_num_changed);*/
		return;
	}

	flag_full_pre = flag_full;
	height_pre = height;
	width_pre = width;
	sps_w_in_pre = sps_w_in;
	sps_h_in_pre = sps_h_in;

	vf_height = src_h;
	vf_width = src_w;
	pre_input_422_flag = input_422_flag;

	lc_en_chflg = 0xff;

	if (chip_type_id == chip_t3x) {
		if (lc_slice_num_changed) {
			lc_slice_num_changed = 0;
			pr_amlc_dbg("%s: lc_slice_num_changed 1->0\n",
				__func__);
		}
	}

/*
 * bit 29: present_flag
 * bit 23-16: color_primaries
 * "unknown", "bt709", "undef", "bt601",
 * "bt470m", "bt470bg", "smpte170m", "smpte240m",
 * "film", "bt2020"
 */
	if (vf->signal_type & (1 << 29))
		/* this field is present */
		flag = ((vf->signal_type >> 16) & 0xff);
	else
		/* signal_type is not present */
		/* use default value bt709 */
		flag = 0x1;

	lc_top_config(lc_en_ctrl, h_num, v_num, height,
		width, bitdepth, flag, flag_full,
		lc_rdma_mode, vpp_index);

	/*config stts and curve size*/
	if (chip_type_id == chip_txhd2 ||
		chip_type_id == chip_t6d) {
		in_sel = 5;//5:vd1
		if (lc_vd_info && chip_type_id == chip_t6d) {
			height = lc_vd_info->vd1_in_vsize;
			width = lc_vd_info->vd1_in_hsize;
		} else {
			width = src_h;
			height = src_w;
		}
	} else {
		in_sel = 4;// 4: vsr_sr_din
	}
	if (chip_type_id != chip_t3x) {
		lc_curve_ctrl_config(lc_en_ctrl, height, width,
			lc_rdma_mode, vpp_index);
		lc_stts_blk_config(lc_en_ctrl, height, width,
			lc_rdma_mode, vpp_index);
		lc_stts_en(lc_en_ctrl, height, width,
			0, 0, 1, 1, in_sel,
			bitdepth, flag, flag_full, lc_rdma_mode, vpp_index);
		lc_dma_config(enable, lc_rdma_mode, vpp_index);
	} else {
		ve_lc_curve_ctrl_cfg(lc_en_ctrl, lc_rdma_mode, vpp_index);
		ve_lc_stts_en(lc_en_ctrl,
			0, 0, 1, 1, 0,
			bitdepth, flag, flag_full,
			lc_tune_curve.lc_reg_thd_black,
			lc_rdma_mode, vpp_index);
	}
}

static void tune_nodes_patch_init(void)
{
	lc_tune_curve.lc_reg_lmtrat_sigbin = 1024;
	lc_tune_curve.lc_reg_lmtrat_thd_max = 950;
	lc_tune_curve.lc_reg_lmtrat_thd_black = 200;
	lc_tune_curve.lc_reg_thd_black = 0x20;
	lc_tune_curve.ypkbv_black_thd = 500;
	lc_tune_curve.yminv_black_thd = 256;
}

static void linear_nodes_patch(int *omap)
{
	omap[0] = 0;
	omap[1] = 0;
	omap[2] = 512;
	omap[3] = 1023;
	omap[4] = 1023;
	omap[5] = 512;
}

static void lc_demo_wr_curve(int h_num, int v_num, int vpp_index)
{
	int i, j, temp1, temp2;

	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 *
					(i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 *
					(i * h_num + j) + 2] << 20);
			temp2 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 *
					(i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 *
					(i * h_num + j) + 5] << 20);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_LC_MAP_RAM_DATA,
				temp1, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_LC_MAP_RAM_DATA,
				temp2, vpp_index);
		}
		for (j = h_num / 2; j < h_num; j++) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_LC_MAP_RAM_DATA,
				(0 | (0 << 10) | (512 << 20)), vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_LC_MAP_RAM_DATA,
				(1023 | (1023 << 10) | (512 << 20)), vpp_index);
		}
	}
}

static int lc_demo_check_curve(int h_num, int v_num)
{
	int i, j, temp, temp1, flag;

	flag = 0;
	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num / 2; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 0] |
				(lc_szcurve[6 * (i * h_num + j) + 1] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 2] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);

			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			temp1 = lc_szcurve[6 * (i * h_num + j) + 3] |
				(lc_szcurve[6 * (i * h_num + j) + 4] << 10) |
				(lc_szcurve[6 * (i * h_num + j) + 5] << 20);
			if (temp != temp1)
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
		for (j = h_num / 2; j < h_num; j++) {
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (0 | (0 << 10) | (512 << 20)))
				flag = (2 * (i * h_num + j) + 0) | (1 << 31);
			temp = READ_VPP_REG(SRSHARP1_LC_MAP_RAM_DATA);
			if (temp != (1023 | (1023 << 10) | (512 << 20)))
				flag = (2 * (i * h_num + j) + 1) | (1 << 31);
		}
	}
	return flag;
}

static int set_lc_curve(int binit, int bcheck, int vpp_index)
{
	int i, h_num, v_num;
	unsigned int hvtemp;
	int rflag;
	int temp, temp1;
	unsigned int reg_ctrl = SRSHARP1_LC_MAP_RAM_CTRL;
	unsigned int reg_addr = SRSHARP1_LC_MAP_RAM_ADDR;
	unsigned int reg_data = SRSHARP1_LC_MAP_RAM_DATA;
	unsigned int reg_num = SRSHARP1_LC_HV_NUM;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg_ctrl = VPP_LC_MAP_RAM_CTRL;
		reg_addr = VPP_LC_MAP_RAM_ADDR;
		reg_data = VPP_LC_MAP_RAM_DATA;
		reg_num = VPP_LC_BLK_HV_NUM;
	}

	rflag = 0;
	hvtemp = READ_VPP_REG(reg_num);
	h_num = (hvtemp >> 8) & 0x1f;
	v_num = hvtemp & 0x1f;

	/*data sequence: ymin/minBv/pkBv/maxBv/ymaxv/ypkBv*/
	if (binit) {
		WRITE_VPP_REG(reg_ctrl, 1);
		WRITE_VPP_REG(reg_addr, 0);
		for (i = 0; i < h_num * v_num; i++) {
			WRITE_VPP_REG(reg_data,
				(0 | (0 << 10) | (512 << 20)));
			WRITE_VPP_REG(reg_data,
				(1023 | (1023 << 10) | (512 << 20)));
		}
		WRITE_VPP_REG(reg_ctrl, 0);
	} else {
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl, 1,
			vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_addr, 0,
			vpp_index);
		if (lc_demo_mode)
			lc_demo_wr_curve(h_num, v_num, vpp_index);
		else
			for (i = 0; i < h_num * v_num; i++) {
				VSYNC_WRITE_VPP_REG_VPP_SEL(reg_data,
					lc_szcurve[6 * i + 0] |
					(lc_szcurve[6 * i + 1] << 10) |
					(lc_szcurve[6 * i + 2] << 20),
					vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(reg_data,
					lc_szcurve[6 * i + 3] |
					(lc_szcurve[6 * i + 4] << 10) |
					(lc_szcurve[6 * i + 5] << 20),
					vpp_index);
			}
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl, 0,
			vpp_index);
	}

	if (bcheck) {
		if (binit) {
			WRITE_VPP_REG(reg_ctrl, 1);
			WRITE_VPP_REG(reg_addr,
				0 | (1 << 31));
			for (i = 0; i < h_num * v_num; i++) {
				temp = READ_VPP_REG(reg_data);
				if (temp != (0 | (0 << 10) | (512 << 20)))
					rflag = (2 * i + 0) | (1 << 31);
				temp = READ_VPP_REG(reg_data);
				if (temp != (1023 | (1023 << 10) | (512 << 20)))
					rflag = (2 * i + 1) | (1 << 31);
			}
			WRITE_VPP_REG(reg_ctrl, 0);
		} else {
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl, 1,
				vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_addr,
				0 | (1 << 31), vpp_index);
			if (lc_demo_mode)
				rflag = lc_demo_check_curve(h_num, v_num);
			else
				for (i = 0; i < h_num * v_num; i++) {
					temp =
					READ_VPP_REG(reg_data);
					temp1 =
					lc_szcurve[6 * i + 0] |
					(lc_szcurve[6 * i + 1] << 10) |
					(lc_szcurve[6 * i + 2] << 20);
					if (temp != temp1)
						rflag =
						(2 * i + 0) | (1 << 31);
					temp =
					READ_VPP_REG(reg_data);
					temp1 =
					lc_szcurve[6 * i + 3] |
					(lc_szcurve[6 * i + 4] << 10) |
					(lc_szcurve[6 * i + 5] << 20);
					if (temp != temp1)
						rflag =
						(2 * i + 1) | (1 << 31);
				}
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl, 0,
				vpp_index);
		}
	}

	return rflag;
}

static void _read_region(int blk_vnum, int blk_hnum)
{
	int i, j, k;
	int data32;
	int rgb_min, rgb_max;
	unsigned int temp1, temp2, cur_block;

	lc_tune_curve.lc_reg_black_count =
		READ_VPP_REG_BITS(LC_STTS_BLACK_INFO, 8, 24);
	lc_tune_curve.lc_reg_black_count =
		lc_tune_curve.lc_reg_black_count / 96;

	if ((flag_cm_lc_dma_en & 0x2) && chip_type_id == chip_t6x &&
		!check_dma_status(EN_DMA_WR_ID_CM2_LC)) {
		am_dma_get_mif_data_lc_stts(0, lc_hist, 12 * 8 * 17);
		am_dma_get_mif_data_lc_curve(lc_szcurve, 12, 8);
		/*part3: add tune curve node patch--by vlsi-guopan*/
		if (tune_curve_en == 2 && lc_drv_param.tune_nodes_patch) {
			for (i = 0; i < blk_vnum; i++) {
				for (j = 0; j < blk_hnum; j++) {
					cur_block = i * blk_hnum + j;
					lc_drv_param.tune_nodes_patch(&lc_szcurve[cur_block * 6],
						&lc_hist[cur_block * 17],
						i, j);
				}
			}
		} else if (tune_curve_en == 1) {
			for (i = 0; i < blk_vnum; i++) {
				for (j = 0; j < blk_hnum; j++) {
					cur_block = i * blk_hnum + j;
					linear_nodes_patch(&lc_szcurve[cur_block * 6]);
				}
			}
		}
		return;
	}

	WRITE_VPP_REG_BITS(LC_STTS_HIST_REGION_IDX, 1, 14, 1);
	data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);

	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 1);
	WRITE_VPP_REG(LC_CURVE_RAM_ADDR, 0);
	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			cur_block = i * blk_hnum + j;
			/*part1: get lc curve node*/
			temp1 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			temp2 = READ_VPP_REG(LC_CURVE_RAM_DATA);
			lc_szcurve[cur_block * 6 + 0] =
				temp1 & 0x3ff;/*bit0:9*/
			lc_szcurve[cur_block * 6 + 1] =
				(temp1 >> 10) & 0x3ff;/*bit10:19*/
			lc_szcurve[cur_block * 6 + 2] =
				(temp1 >> 20) & 0x3ff;/*bit20:29*/
			lc_szcurve[cur_block * 6 + 3] =
				temp2 & 0x3ff;/*bit0:9*/
			lc_szcurve[cur_block * 6 + 4] =
				(temp2 >> 10) & 0x3ff;/*bit10:19*/
			lc_szcurve[cur_block * 6 + 5] =
				(temp2 >> 20) & 0x3ff;/*bit20:29*/

			/*part2: get lc hist*/
			data32 = READ_VPP_REG(LC_STTS_HIST_START_RD_REGION);
			if (i >= lc_hist_vs && i <= lc_hist_ve &&
				j >= lc_hist_hs && j <= lc_hist_he &&
				amlc_debug == 0x6 && lc_hist_prcnt)
				pr_info("========[r,c](%2d,%2d)======\n", i, j);

			for (k = 0; k < 17; k++) {
				data32 = READ_VPP_REG(LC_STTS_HIST_READ_REGION);
				lc_hist[cur_block * 17 + k] = data32;
				if (i >= lc_hist_vs && i <= lc_hist_ve &&
					j >= lc_hist_hs && j <= lc_hist_he &&
					amlc_debug == 0x6 && lc_hist_prcnt) {
					/*print chosen hist*/
					if (k == 16) {/*last bin*/
						rgb_min =
							(data32 >> 10) & 0x3ff;
						rgb_max = (data32) & 0x3ff;
						pr_info("[%2d]:%d,%d\n",
							k, rgb_min, rgb_max);
					} else {
						pr_info("[%2d]:%d\n",
							k, data32);
					}
				}
			}

			/*part3: add tune curve node patch--by vlsi-guopan*/
			if (tune_curve_en == 2) {
				if (lc_drv_param.tune_nodes_patch)
					lc_drv_param.tune_nodes_patch(&lc_szcurve[cur_block * 6],
						&lc_hist[cur_block * 17],
						i, j);
			} else if (tune_curve_en == 1) {
				linear_nodes_patch(&lc_szcurve[cur_block * 6]);
			}
		}
	}
	WRITE_VPP_REG(LC_CURVE_RAM_CTRL, 0);
}

void lc_read_region(int blk_vnum, int blk_hnum,
	int slice)
{
	int i;
	int j;
	int k;
	unsigned int cur_block;
	int *data_curve;
	int *data_hist;
	int prt_flag = 0;

	if (!lc_malloc_ok) {
//		pr_amlc_dbg("[%s] lc malloc fail\n", __func__);
		return;
	}

	if (chip_type_id != chip_t3x) {
		_read_region(blk_vnum, blk_hnum);
		data_hist = lc_hist;
	} else {
		if (slice == 0) {
			data_curve = lc_szcurve;
			data_hist = lc_hist;
		} else {
			data_curve = lc_szcurve_slice1;
			data_hist = lc_hist_slice1;
		}

		memcpy(lc_szcurve_slice1, lc_szcurve, LC_CURV_SIZE * sizeof(int));

		ve_lc_region_read(blk_vnum, blk_hnum, slice,
			&lc_tune_curve.lc_reg_black_count,
			data_curve, data_hist);

		for (i = 0; i < 8 * 12 * 6; i++) {
			if (data_curve[i] - lc_szcurve_slice1[i] > lc_pr_lmt) {
				prt_flag = 1;
				break;
			}
		}

		if (lc_hist_prcnt == 0x80 && prt_flag) {/*print curve data*/
			pr_info("read back curve, curve slice = %d\n", slice);
			for (i = 0; i < 4 * 6; i++) {
				pr_info("0x%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x/%x\n",
					data_curve[24 * i], data_curve[24 * i + 1],
					data_curve[24 * i + 2],
					data_curve[24 * i + 3],
					data_curve[24 * i + 4],
					data_curve[24 * i + 5],
					data_curve[24 * i + 6],
					data_curve[24 * i + 7],
					data_curve[24 * i + 8],
					data_curve[24 * i + 9],
					data_curve[24 * i + 10],
					data_curve[24 * i + 11],
					data_curve[24 * i + 12],
					data_curve[24 * i + 13],
					data_curve[24 * i + 14],
					data_curve[24 * i + 15],
					data_curve[24 * i + 16],
					data_curve[24 * i + 17],
					data_curve[24 * i + 18],
					data_curve[24 * i + 19],
					data_curve[24 * i + 20],
					data_curve[24 * i + 21],
					data_curve[24 * i + 22],
					data_curve[24 * i + 23]);
			}
		}

		/*part3: add tune curve node patch--by vlsi-guopan*/
		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				cur_block = i * blk_hnum + j;

				if (tune_curve_en == 2) {
					/*tune_nodes_patch(&data_curve[cur_block * 6],*/
					/*	&data_hist[cur_block * 17], i, j);*/
				} else if (tune_curve_en == 1) {
					linear_nodes_patch(&data_curve[cur_block * 6]);
				} else {
					break;
				}
			}
		}
	}

	if (amlc_debug == 0x8 && lc_hist_prcnt) {/*print all hist data*/
		pr_info("slice = %d\n", slice);
		for (i = 0; i < blk_vnum; i++) {
			for (j = 0; j < blk_hnum; j++) {
				cur_block = i * blk_hnum + j;
				for (k = 0; k < 17; k++)
					pr_info("[%d,%d,%d] %x\n", i, j, k,
						data_hist[cur_block * 17 + k]);
			}
		}
	}

	if (lc_hist_prcnt > 0 && lc_hist_prcnt != 0x80)
		lc_hist_prcnt--;
}

void lc_prt_curve(void)
{/*print curve node*/
	int i, j;

	if (!lc_szcurve)
		return;

	pr_amlc_dbg("======lc_prt curve node=======\n");
	for (i = 0; i < 12 * 8; i++) {
		for (j = 0; j < 6 ; j++)
			pr_amlc_dbg("[%d]%d\n",
				i * 6 + j, lc_szcurve[i * 6 + j]);
		pr_amlc_dbg("\n");
	}

	if (chip_type_id == chip_t3x && lc_szcurve_slice1) {
		pr_amlc_dbg("======lc_szcurve_slice1=======\n");
		for (i = 0; i < 12 * 8; i++) {
			for (j = 0; j < 6 ; j++)
				pr_amlc_dbg("[%d]%d\n",
					i * 6 + j, lc_szcurve_slice1[i * 6 + j]);
			pr_amlc_dbg("\n");
		}
	}
}

static void lc_drv_param_init(void)
{
	lc_drv_param.curve_nodes_cur = curve_nodes_cur;
	lc_drv_param.curve_nodes_pre = curve_nodes_pre;
	lc_drv_param.refresh_bit = &refresh_bit;
	lc_drv_param.vnum_start_below = &vnum_start_below;
	lc_drv_param.vnum_end_below = &vnum_end_below;
	lc_drv_param.vnum_start_above = &vnum_start_above;
	lc_drv_param.vnum_end_above = &vnum_end_above;
	lc_drv_param.invalid_blk = &invalid_blk;
	lc_drv_param.osd_iir_en = &osd_iir_en;
	lc_drv_param.ts = &ts;
	lc_drv_param.scene_change_th = &scene_change_th;
	lc_drv_param.alpha1 = &alpha1;
	lc_drv_param.alpha2 = &alpha2;
	lc_drv_param.min_bv_percent_th = &min_bv_percent_th;
	lc_drv_param.amlc_debug = &amlc_debug;
	lc_drv_param.amlc_iir_debug_en = &amlc_iir_debug_en;
	lc_drv_param.lc_node_prcnt = &lc_node_prcnt;
	lc_drv_param.lc_node_pos_h = &lc_node_pos_h;
	lc_drv_param.lc_node_pos_v = &lc_node_pos_v;
	lc_drv_param.lc_curve_prcnt = &lc_curve_prcnt;
	lc_drv_param.lc_curve_fresh = &lc_curve_fresh;
	lc_drv_param.curve_nodes_pre_raw = curve_nodes_pre_raw;
	lc_drv_param.lc_tune_curve = &lc_tune_curve;
}

void lc_init(int bitdepth)
{
	int h_num, v_num;
	unsigned int height, width;
	const struct vinfo_s *vinfo = get_current_vinfo();
	int i, tmp, tmp1, tmp2;
	unsigned int reg_sat0 = SRSHARP1_LC_SAT_LUT_0_1;
	unsigned int reg_sat = SRSHARP1_LC_SAT_LUT_62;
	unsigned int adr = 16;

	if (chip_type_id == chip_a9)
		return;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg_sat0 = VPP_LC_SATUR_LUT_0;
		reg_sat = VPP_LC_SATUR_LUT_F;
		adr = 12;
	}

	tune_nodes_patch_init();
	lc_bitdepth = bitdepth;

	height = vinfo->height;
	width = vinfo->width;
	h_num = LC_BLK_H_NUM;
	v_num = LC_BLK_V_NUM;

	if (!lc_szcurve)
		lc_szcurve = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_szcurve)
		return;

	if (!curve_nodes_cur)
		curve_nodes_cur = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_cur) {
		kfree(lc_szcurve);
		return;
	}

	if (!curve_nodes_pre)
		curve_nodes_pre = kcalloc(LC_CURV_SIZE, sizeof(int), GFP_KERNEL);
	if (!curve_nodes_pre) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		return;
	}

	if (!lc_hist)
		lc_hist = kcalloc(LC_HIST_SIZE, sizeof(int), GFP_KERNEL);
	if (!lc_hist) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		return;
	}

	if (!curve_nodes_pre_raw)
		curve_nodes_pre_raw = kcalloc(LC_CURV_SIZE,
			sizeof(int64_t), GFP_KERNEL);
	if (!curve_nodes_pre_raw) {
		kfree(lc_szcurve);
		kfree(curve_nodes_cur);
		kfree(curve_nodes_pre);
		kfree(lc_hist);
		return;
	}

	if (chip_type_id == chip_t3x) {
		if (!lc_szcurve_slice1)
			lc_szcurve_slice1 = kcalloc(LC_CURV_SIZE,
				sizeof(int), GFP_KERNEL);
		if (!lc_szcurve_slice1) {
			kfree(lc_szcurve);
			kfree(curve_nodes_cur);
			kfree(curve_nodes_pre);
			kfree(lc_hist);
			kfree(curve_nodes_pre_raw);
			return;
		}

		if (!lc_hist_slice1)
			lc_hist_slice1 = kcalloc(LC_HIST_SIZE,
				sizeof(int), GFP_KERNEL);
		if (!lc_hist_slice1) {
			kfree(lc_szcurve);
			kfree(curve_nodes_cur);
			kfree(curve_nodes_pre);
			kfree(lc_hist);
			kfree(curve_nodes_pre_raw);
			kfree(lc_szcurve_slice1);
			return;
		}
	}

	lc_malloc_ok = 1;
	if (!lc_en)
		return;

	lc_top_config(0, h_num, v_num,
		height, width, bitdepth, 1, 0, 0, 0);

	if (chip_type_id != chip_t3x) {
		WRITE_VPP_REG_BITS(LC_CURVE_RAM_CTRL, 0, 0, 1);

		/*default LC low parameters*/
		WRITE_VPP_REG(LC_CURVE_CONTRAST_LH, 0x000b000b);
		WRITE_VPP_REG(LC_CURVE_CONTRAST_SCL_LH, 0x00000b0b);
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x)
			WRITE_VPP_REG(LC_CURVE_MISC0, 0x13038);
		else
			WRITE_VPP_REG(LC_CURVE_MISC0, 0x00023028);
		WRITE_VPP_REG(LC_CURVE_YPKBV_RAT, 0x8cc0c060);
		if (chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x)
			WRITE_VPP_REG(LC_CURVE_YPKBV_SLP_LMT, 0x0c60);
		else
			WRITE_VPP_REG(LC_CURVE_YPKBV_SLP_LMT, 0x00000b3a);

		if (cpu_after_eq_t7()) {
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13, 0x01b001d0);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_14_15, 0x01f00210);

			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1, 0x004400b4);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_2_3, 0x00fb0123);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_4_5, 0x015901a2);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_6_7, 0x01d90208);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_8_9, 0x02400280);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_10_11, 0x02d70310);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13, 0x03400380);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_14_15, 0x03c003ff);

			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1, 0x004400b4);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_2_3, 0x00fb0123);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_4_5, 0x015901a2);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_6_7, 0x01d90208);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_8_9, 0x02400280);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_10_11, 0x02d70310);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_12_13, 0x03400380);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_14_15, 0x03c003ff);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13, 0x01b001d0);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_14_15, 0x01f00210);

			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1, 0x004400b4);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_2_3, 0x00fb0123);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_4_5, 0x015901a2);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_6_7, 0x01d90208);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_8_9, 0x02400280);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_10_11, 0x02d70310);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13, 0x03400380);
			WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_14_15, 0x03c003ff);

			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1, 0x004400b4);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_2_3, 0x00fb0123);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_4_5, 0x015901a2);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_6_7, 0x01d90208);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_8_9, 0x02400280);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_10_11, 0x02d70310);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_12_13, 0x03400380);
			WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_14_15, 0x03c003ff);
		} else {
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1, 0x0030005d);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_2_3, 0x00830091);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_4_5, 0x00a000c4);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_6_7, 0x00e00100);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_8_9, 0x01200140);
			WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_10_11, 0x01600190);

			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1, 0x004400b4);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_2_3, 0x00fb0123);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_4_5, 0x015901a2);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_6_7, 0x01d90208);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_8_9, 0x02400280);
			WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_10_11, 0x02d70310);
		}

		for (i = 0; i < 31 ; i++) {
			tmp1 = *(lc_satur_off + 2 * i);
			tmp2 = *(lc_satur_off + 2 * i + 1);
			tmp = ((tmp1 & 0xfff) << adr) | (tmp2 & 0xfff);
			WRITE_VPP_REG(reg_sat0 + i, tmp);
		}
		tmp = (*(lc_satur_off + 62)) & 0xfff;
		WRITE_VPP_REG(reg_sat, tmp);
		/*end*/

		if (set_lc_curve(1, 0, 0))
			pr_info("%s: init fail", __func__);
	} else {
		ve_lc_base_init();
		ve_lc_sat_lut_set(lc_satur_off);
		ve_lc_curve_set(1, 0, NULL, 0, 0);
		ve_lc_curve_set(1, 0, NULL, 1, 0);
	}

	lc_drv_param_init();
}

bool lc_curve_ctrl_reg_set_flag(unsigned int addr)
{
	bool lc_config_set_flag = false;

	if (addr == LC_CURVE_CTRL) {
		if (lc_en && lc_flag > 1)
			lc_config_set_flag = true;
		else
			lc_config_set_flag = false;
	}

	return lc_config_set_flag;
}

int dbg_vf_h;
int dbg_vf_w;
int dbg_sps_h_en;
int dbg_sps_v_en;
int dbg_sps_w_in;
int dbg_sps_h_in;
int dbg_slice_flag;
int dbg_hwin_begin;
int dbg_hwin_end;
int dbg_hsize_out;
int dbg_ro;
int dbg_stts_idx;
int dbg_curve_ctrl;
int dbg_curve_ram_ctrl;
int dbg_mapping_top_ctrl;
int dbg_curve_ctrl_s1;
int dbg_curve_ram_ctrl_s1;
int dbg_mapping_top_ctrl_s1;

void dbg_monitor(struct vframe_s *vf,
	unsigned int sps_h_en,
	unsigned int sps_v_en,
	unsigned int sps_w_in,
	unsigned int sps_h_in)
{
	int multi_slice_flag;
	unsigned int value;
	unsigned int hsize_out, hwin_begin, hwin_end;

	multi_slice_flag = ve_multi_slice_case_get();
	if (multi_slice_flag != dbg_slice_flag) {
		pr_info("[lc_monitor] multi_slice_flag: %d-->%d\n",
			dbg_slice_flag, multi_slice_flag);
		dbg_slice_flag = multi_slice_flag;
	}

	if (vf) {
		if (vf->height != dbg_vf_h ||
			vf->width != dbg_vf_w) {
			pr_info("[lc_monitor] vf->height/width: %d/%d\n",
				vf->height, vf->width);
			dbg_vf_h = vf->height;
			dbg_vf_w = vf->width;
		}
	}

	if (sps_h_en != dbg_sps_h_en ||
		sps_v_en != dbg_sps_v_en ||
		sps_w_in != dbg_sps_w_in ||
		sps_h_in != dbg_sps_h_in) {
		pr_info("[lc_monitor] h_en/v_en/w_in/h_in: %d/%d/%d/%d\n",
			sps_h_en, sps_v_en, sps_w_in, sps_h_in);
		dbg_sps_h_en = sps_h_en;
		dbg_sps_v_en = sps_v_en;
		dbg_sps_w_in = sps_w_in;
		dbg_sps_h_in = sps_h_in;
	}

	value = READ_VPP_REG_S5(0x2813);
	hwin_begin = (value >> 16) & 0x1fff;
	hwin_end = value & 0x1fff;
	hsize_out = READ_VPP_REG_S5(0x2808);
	hsize_out = (hsize_out >> 16) & 0x1fff;

	if (hwin_begin != dbg_hwin_begin ||
		hwin_end != dbg_hwin_end ||
		hsize_out != dbg_hsize_out) {
		pr_info("[lc_monitor] hwin_begin/end/hsize_out: %d/%d/%d\n",
			hwin_begin, hwin_end, hsize_out);
		dbg_hwin_begin = hwin_begin;
		dbg_hwin_end = hwin_end;
		dbg_hsize_out = hsize_out;
	}

	value = READ_VPP_REG_S5(0x5aef);
	if (value != dbg_ro) {
		pr_info("[lc_monitor] ro: 0x%x -> 0x%x\n",
			dbg_ro, value);
		dbg_ro = value;
	}

	value = READ_VPP_REG_S5(0x5ae9);
	if (value != dbg_stts_idx) {
		pr_info("[lc_monitor] stts_idx: 0x%x -> 0x%x\n",
			dbg_stts_idx, value);
		dbg_stts_idx = value;
	}

	value = READ_VPP_REG_S5(0x5a40);
	if (value != dbg_curve_ctrl) {
		pr_info("[lc_monitor] curve_ctrl: 0x%x -> 0x%x\n",
			dbg_curve_ctrl, value);
		dbg_curve_ctrl = value;
	}

	value = READ_VPP_REG_S5(0x5a70);
	if (value != dbg_curve_ram_ctrl) {
		pr_info("[lc_monitor] curve_ram_ctrl: 0x%x -> 0x%x\n",
			dbg_curve_ram_ctrl, value);
		dbg_curve_ram_ctrl = value;
	}

	value = READ_VPP_REG_S5(0x52c0);
	if (value != dbg_mapping_top_ctrl) {
		pr_info("[lc_monitor] mapping_top_ctrl: 0x%x -> 0x%x\n",
			dbg_mapping_top_ctrl, value);
		dbg_mapping_top_ctrl = value;
	}

	value = READ_VPP_REG_S5(0x5a80);
	if (value != dbg_curve_ctrl_s1) {
		pr_info("[lc_monitor] curve_ctrl_s1: 0x%x -> 0x%x\n",
			dbg_curve_ctrl_s1, value);
		dbg_curve_ctrl_s1 = value;
	}

	value = READ_VPP_REG_S5(0x5ab0);
	if (value != dbg_curve_ram_ctrl_s1) {
		pr_info("[lc_monitor] curve_ram_ctrl_s1: 0x%x -> 0x%x\n",
			dbg_curve_ram_ctrl_s1, value);
		dbg_curve_ram_ctrl_s1 = value;
	}

	value = READ_VPP_REG_S5(0x77c0);
	if (value != dbg_mapping_top_ctrl_s1) {
		pr_info("[lc_monitor] mapping_top_ctrl_s1: 0x%x -> 0x%x\n",
			dbg_mapping_top_ctrl_s1, value);
		dbg_mapping_top_ctrl_s1 = value;
	}
}

int get_lc_pattern_gain(void)
{
	int k, left, rgt;
	int idx1 = 0, idx2 = 0, idx_max = 0;
	int hcnt[4], vcnt[4], pdcnt[4], ndcnt[4], cntsum[4], integer_part[4], frac_part[4];
	int lc_gain = 0;
	int frac_bitw = 8;
	int lc_pattern_gain[100] = {
		/*0   1   2   3   4   5   6   7   8   9*/
		64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
		64, 58, 52, 38, 32, 26, 20, 12, 6,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,/*5*/
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	};/*norm 64 as 1*/

	for (k = 0; k < 4; k++) {
		hcnt[k] = 0;
		vcnt[k] = 0;
		pdcnt[k] = 0;
		ndcnt[k] = 0;
		cntsum[k] = 0;
		integer_part[k] = 0;
		frac_part[k] = 0;
	}

	hcnt[0] = (data_meter.fmeter1_hcnt[0] + 16) >> 5;
	hcnt[1] = (data_meter.fmeter1_hcnt[1] + 16) >> 5;
	hcnt[2] = (data_meter.fmeter1_hcnt[2] + 16) >> 5;
	hcnt[3] = (data_meter.fmeter1_hcnt[3] + 16) >> 5;

	vcnt[0] = (data_meter.fmeter1_vcnt[0] + 16) >> 5;
	vcnt[1] = (data_meter.fmeter1_vcnt[1] + 16) >> 5;
	vcnt[2] = (data_meter.fmeter1_vcnt[2] + 16) >> 5;
	vcnt[3] = (data_meter.fmeter1_vcnt[3] + 16) >> 5;

	pdcnt[0] = (data_meter.fmeter1_pdcnt[0] + 16) >> 5;
	pdcnt[1] = (data_meter.fmeter1_pdcnt[1] + 16) >> 5;
	pdcnt[2] = (data_meter.fmeter1_pdcnt[2] + 16) >> 5;
	pdcnt[3] = (data_meter.fmeter1_pdcnt[3] + 16) >> 5;

	ndcnt[0] = (data_meter.fmeter1_ndcnt[0] + 16) >> 5;
	ndcnt[1] = (data_meter.fmeter1_ndcnt[1] + 16) >> 5;
	ndcnt[2] = (data_meter.fmeter1_ndcnt[2] + 16) >> 5;
	ndcnt[3] = (data_meter.fmeter1_ndcnt[3] + 16) >> 5;

	cntsum[0] = hcnt[0] + hcnt[1] + hcnt[2];
	cntsum[1] = vcnt[0] + vcnt[1] + vcnt[2];
	cntsum[2] = pdcnt[0] + pdcnt[1] + pdcnt[2];
	cntsum[3] = ndcnt[0] + ndcnt[1] + ndcnt[2];

	integer_part[0] = (100 * cntsum[0]) / (cntsum[0] + hcnt[3] + 1);
	integer_part[1] = (100 * cntsum[1]) / (cntsum[1] + vcnt[3] + 1);
	integer_part[2] = (100 * cntsum[2]) / (cntsum[2] + pdcnt[3] + 1);
	integer_part[3] = (100 * cntsum[3]) / (cntsum[3] + ndcnt[3] + 1);

	frac_part[0] = (((100 * cntsum[0]) << frac_bitw) / (cntsum[0] + hcnt[3] + 1)) -
		(integer_part[0] << frac_bitw);
	frac_part[1] = (((100 * cntsum[1]) << frac_bitw) / (cntsum[1] + vcnt[3] + 1)) -
		(integer_part[1] << frac_bitw);
	frac_part[2] = (((100 * cntsum[2]) << frac_bitw) / (cntsum[2] + pdcnt[3] + 1)) -
		(integer_part[2] << frac_bitw);
	frac_part[3] = (((100 * cntsum[3]) << frac_bitw) / (cntsum[3] + ndcnt[3] + 1)) -
		(integer_part[3] << frac_bitw);

	/*get max idx*/
	idx1 = integer_part[0] > integer_part[1] ? 0 : 1;
	idx2 = integer_part[2] > integer_part[3] ? 2 : 3;
	idx_max = integer_part[idx1] > integer_part[idx2] ? idx1 : idx2;

	/*get gain*/
	left = integer_part[idx_max];
	left = min(max(left, 0), 99);
	rgt = min(left + 1, 99);
	lc_gain = lc_pattern_gain[left] +
		(((lc_pattern_gain[rgt] - lc_pattern_gain[left]) *
		frac_part[idx_max] + (1 << (frac_bitw - 1))) >> frac_bitw);

	if (lc_pattern_detect_log1) {
		lc_pattern_detect_log1_cnt++;

		if (lc_pattern_detect_log1_cnt > 1) {
			for (k = 0; k < 4; k++) {
				pr_info("hcnt[%d] = %d\n",
					k, data_meter.fmeter1_hcnt[k]);
				pr_info("vcnt[%d] = %d\n",
					k, data_meter.fmeter1_vcnt[k]);
				pr_info("pdcnt[%d] = %d\n",
					k, data_meter.fmeter1_pdcnt[k]);
				pr_info("ndcnt[%d] = %d\n",
					k, data_meter.fmeter1_ndcnt[k]);
			}

			pr_info("lc_gain = %d\n", lc_gain);
			pr_info("end...\n\n");

			lc_pattern_detect_log1_cnt = 0;
			lc_pattern_detect_log1 = 0;
		}
	}

	return lc_gain;
}

void lc_change_pattern_curve_nodes(int lc_gain,
	int blk_hnum, int blk_vnum)
{
	unsigned int i;
	unsigned int j;
	unsigned int cur_block;
	int dif_value1 = 0;
	int dif_value2 = 0;
	int dif_value3 = 0;

	if (blk_hnum <= 0 || blk_vnum <= 0)
		return;

	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			cur_block = i * blk_hnum + j;

			dif_value1 = lc_szcurve[cur_block * 6 + 1] -
				lc_szcurve[cur_block * 6 + 0];
			lc_szcurve[cur_block * 6 + 0] = lc_szcurve[cur_block * 6 + 0] +
				((lc_gain * dif_value1 + 32) >> 6);

			dif_value2 = lc_szcurve[cur_block * 6 + 2] -
				lc_szcurve[cur_block * 6 + 5];
			lc_szcurve[cur_block * 6 + 5] = lc_szcurve[cur_block * 6 + 5] +
				((lc_gain * dif_value2 + 32) >> 6);

			dif_value3 = lc_szcurve[cur_block * 6 + 3] -
				lc_szcurve[cur_block * 6 + 4];
			lc_szcurve[cur_block * 6 + 4] = lc_szcurve[cur_block * 6 + 4] +
				((lc_gain * dif_value3 + 32) >> 6);

			if (lc_read_curve_nodes_changed_en)
				pr_info("block[%d, %d] = %d, %d, %d, %d, %d, %d\n", i, j,
					lc_szcurve[cur_block * 6 + 0],
					lc_szcurve[cur_block * 6 + 1],
					lc_szcurve[cur_block * 6 + 2],
					lc_szcurve[cur_block * 6 + 3],
					lc_szcurve[cur_block * 6 + 4],
					lc_szcurve[cur_block * 6 + 5]);
		}
	}

	lc_read_curve_nodes_changed_en = 0;
}

int pre_lc_en;

static int get_average_luma(struct vpp_hist_param_s *vp, int step)
{
	int ret = 0;
	int i = 0;
	int sum = 0;

	if (!vp)
		return 0;

	for (i = 0; i < 64; i++)
		sum += vp->vpp_gamma[i];

	ret = sum / step;

	return ret;
}

static int get_average_hue(struct vpp_hist_param_s *vp, int step)
{
	int ret = 0;
	int i = 0;
	int sum = 0;

	if (!vp)
		return 0;

	for (i = 0; i < 32; i++)
		sum += vp->vpp_hue_gamma[i];

	ret = sum / step;

	return ret;
}

static int get_contiguous_zero_count_luma(struct vpp_hist_param_s *vp)
{
	int ret = 0;
	int i = 0;
	int count = 0;

	if (!vp)
		return 0;

	for (i = 4; i < 59; i++) {
		if (vp->vpp_gamma[i] == 0) {
			count++;
		} else {
			if (count > ret)
				ret = count;
			count = 0;
		}
	}

	return ret;
}

static int full_grey_detect(struct vpp_hist_param_s *vp)
{
	int ret = 1;
	int i = 0;

	if (!vp)
		return 0;

	for (i = 0; i < 32; i++) {
		if (vp->vpp_hue_gamma[i] != 0) {
			ret = 0;
			break;
		}
	}

	return ret;
}

static int single_bin_detect(struct vpp_hist_param_s *vp)
{
	int ret = 0;
	int i = 0;
	int count = 0;
	int threshold_data = 0x10;
	int threshold_bin = 3;
	int luma_min, luma_max;

	if (!vp)
		return 0;

	luma_min = vp->vpp_luma_min;
	luma_max = vp->vpp_luma_max;

	for (i = 0; i < 64; i++) {
		if (vp->vpp_gamma[i] > threshold_data)
			count++;
	}

	if (count > 0 &&
		(luma_min == luma_max || count < threshold_bin)) {
		ret = 1;
		if (amlc_debug == 0x100)
			pr_info("[detect] single bin case.\n");
	}

	return ret;
}

static int grey_level_detect(struct vpp_hist_param_s *vp)
{
	int i = 0;
	int count = 0;
	int contiguous_zero_count = 0;
	int luma_max;
	int luma_min;
	int average_val;

	if (!vp)
		return 0;

	if (!full_grey_detect(vp))
		return 0;

	luma_min = vp->vpp_luma_min;
	luma_max = vp->vpp_luma_max;
	contiguous_zero_count = get_contiguous_zero_count_luma(vp);

	if (luma_max > 0xe7 && luma_min < 0x12) {
		/*11 level*/
		average_val = get_average_luma(vp, 11);

		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > (average_val >> 1))
				count++;
		}

		if (count > 8 && count < 13 &&
			contiguous_zero_count < 6) {
//			if (amlc_debug == 0x100)
//				pr_info("[detect] 11 level case.\n");
			return 1;
		}

		/*16 level*/
		count = 0;
		average_val = get_average_luma(vp, 16);

		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > (average_val >> 1))
				count++;
		}

		if (count > 13 && count < 19 &&
			contiguous_zero_count < 4) {
//			if (amlc_debug == 0x100)
//				pr_info("[detect] 16 level case.\n");
			return 1;
		}

		/*32 level*/
		count = 0;
		average_val = get_average_luma(vp, 32);

		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > (average_val >> 1))
				count++;
		}

		if (count > 29 && count < 35 &&
			contiguous_zero_count < 2) {
//			if (amlc_debug == 0x100)
//				pr_info("[detect] 32 level case.\n");
			return 1;
		}

		/* 64 level*/
		count = 0;
		average_val = get_average_luma(vp, 64);
		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > (average_val >> 1))
				count++;
		}

		if (count > 60 && count < 68 && contiguous_zero_count < 1) {
//			if (amlc_debug == 0x100)
//				pr_info("[detect] 64 level case.\n");
			return 1;
		}
	}

	return 0;
}

static int customer_pattern_detect(struct vpp_hist_param_s *vp)
{
	int ret = 0;
	int i = 0;
	int sum = 0;
	int tmp = 0;
	int count = 0;
	int max_luma_index = 0;
	int max_no_zero_index = 0;
	int luma_max, luma_min;
	int full_grey = 0;
	int match_block[3] = {1, 1, 1};
	int average_hue = 0;
	int sat_zero_bin_count = 0;
	int bin_match_count = 0;
	int average_val = 0;

	if (!vp)
		return 0;

	luma_min = vp->vpp_luma_min;
	luma_max = vp->vpp_luma_max;
	full_grey = full_grey_detect(vp);
	average_val = get_average_luma(vp, 58);

	for (i = 0; i < 64; i++) {
		tmp = vp->vpp_gamma[i];
		if (tmp > average_val)
			count++;
		if (tmp == 0xffff)
			bin_match_count++;
	}

	if (bin_match_count > 0 &&
		bin_match_count < 5 &&
		count < 11) {
		ret = 1;
//		if (amlc_debug == 0x100)
//			pr_info("[detect] box case.\n");
	}

	tmp = 0;
	sum = 0;
	count = 0;
	if (full_grey && luma_max > 0xe7 && luma_min < 0x12) {
		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > tmp) {
				tmp = vp->vpp_gamma[i];
				max_luma_index = i;
			}

			if (i > 15)
				sum += vp->vpp_gamma[i];
		}

		tmp = sum >> 3;
		for (i = 16; i < 64; i++) {
			if (vp->vpp_gamma[i] > tmp)
				count++;
		}

		if (count > 2 && count < 6 &&
			max_luma_index < 7) {
			ret = 1;
//			if (amlc_debug == 0x100)
//				pr_info("[detect] 4 step case.\n");
		}

		for (i = 4; i < 14; i++) {
			if (vp->vpp_gamma[i] == 0) {
				match_block[0] = 0;
				break;
			}
		}

		for (i = 28; i < 35; i++) {
			if (vp->vpp_gamma[i] != 0) {
				match_block[1] = 0;
				break;
			}
		}

		for (i = 52; i < 59; i++) {
			if (vp->vpp_gamma[i] == 0) {
				match_block[2] = 0;
				break;
			}
		}

		if (match_block[0] &&
			match_block[1] &&
			match_block[2]) {
			ret = 1;
//			if (amlc_debug == 0x100)
//				pr_info("[detect] black and white blend case.\n");
		}
	}

	tmp = 0;
	if (full_grey && luma_max < 0x68 && luma_min < 0x12 &&
		luma_max != luma_min) {
		for (i = 0; i < 64; i++) {
			if (vp->vpp_gamma[i] > tmp) {
				tmp = vp->vpp_gamma[i];
				max_luma_index = i;
			}

			if (vp->vpp_gamma[i] != 0)
				max_no_zero_index = i;
		}

		if (max_no_zero_index < 26 && max_luma_index < 10) {
			ret = 1;
//			if (amlc_debug == 0x100)
//				pr_info("[detect] black step case.\n");
		}
	}

	count = 0;
	if (!full_grey && luma_max > 0xd0 && luma_min < 0x12) {
		average_hue = get_average_hue(vp, 6);

		for (i = 0; i < 32; i++) {
			if (vp->vpp_hue_gamma[i] > (average_hue >> 1))
				count++;

			if (vp->vpp_sat_gamma[i] == 0)
				sat_zero_bin_count++;
		}

		if (count == 6 && sat_zero_bin_count == 0) {
			ret = 1;
//			if (amlc_debug == 0x100)
//				pr_info("[detect] color sat 6 step case.\n");
		}
	}

	return ret;
}

static int special_pattern_detect(struct vpp_hist_param_s *vp)
{
	int ret = 0;

	if (!vp)
		return 0;

	if (customer_pattern_detect(vp))
		ret = 1;
	else if (grey_level_detect(vp))
		ret = 1;
	else if (single_bin_detect(vp))
		ret = 1;

	return ret;
}

void lc_process(struct vframe_s *vf,
	struct vpq_size_s *pvpq_size,
	int vpp_index,
	struct vpp_hist_param_s *vp)
{
	int blk_hnum, blk_vnum;
	int multi_pic_flag = 0;
	int multi_slice_flag = 0;
	int lc_bypass_th = 0;
	unsigned int height, width;
	int lc_gain = 0;
	unsigned int sps_h_en;
	unsigned int sps_v_en;
	unsigned int sps_w_in;
	unsigned int sps_h_in;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_TL1 ||
		chip_type_id == chip_s5 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_a9 ||
		lc_force_ctrl)
		return;

	sps_h_en = pvpq_size->sr1_hsc_en;
	sps_v_en = pvpq_size->sr1_vsc_en;
	sps_w_in = pvpq_size->sr1_in_hsize;
	sps_h_in = pvpq_size->sr1_in_vsize;

	if (amlc_debug == 0xf0)
		monitor_lc_stts_overflow();

	if (!lc_malloc_ok) {
		pr_amlc_dbg("[%s] lc malloc fail\n", __func__);
		return;
	}

	if (!slt_en && chip_type_id == chip_t3x) {
		multi_pic_flag = ve_multi_picture_case_get();
		multi_slice_flag = ve_multi_slice_case_get();

		if (dbg_monitor_ctrl)
			dbg_monitor(vf, sps_h_en, sps_v_en, sps_w_in, sps_h_in);

		if (multi_slice_flag || multi_pic_flag) {
			lc_disable(lc_rdma_mode, vpp_index);
			return;
		}
	}

	lc_vd_info = get_vd_proc_amvecm_info();

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		sps_h_in = 1080;
		sps_w_in = 1920;
	}

	if ((chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) && lc_vd_info) {
		height = lc_vd_info->vd1_dout_vsize;
		width = lc_vd_info->vd1_dout_hsize;

		if (amlc_debug == 0x10) {
			pr_info("[lc_vd_info] out_v/hsize: %d/%d\n",
				lc_vd_info->vd1_dout_vsize,
				lc_vd_info->vd1_dout_hsize);
			pr_info("[lc_vd_info] in_v/hsize: %d/%d\n",
				lc_vd_info->vd1_in_vsize,
				lc_vd_info->vd1_in_hsize);
		}
	} else {
		height = sps_h_in << sps_v_en;
		width = sps_w_in << sps_h_en;
	}

	if (pre_lc_en != lc_en) {
//		pr_amlc_dbg("[lc] lc_en from %d to %d\n", pre_lc_en, lc_en);
		pre_lc_en = lc_en;
	}

	if (!lc_en  ||
		(vf && width < width_limit && height < height_limit)) {
		if (lc_en_chflg == 0xff)
			lc_disable(lc_rdma_mode, vpp_index);
		return;
	}

	if (!vf) {
		if (lc_flag == 0xff) {
			lc_disable(lc_rdma_mode, vpp_index);
			lc_bypass_flag = 0x0;
			lc_flag = 0x0;
		}
		return;
	}

	if (vf->source_type == VFRAME_SOURCE_TYPE_HWC) {
		pr_amlc_dbg("\n[%s] HWC skip.\n", __func__);
		return;
	}

	if (chip_type_id != chip_t3x)
		lc_bypass_th = 0;//0:skip 1 frame
	else
		lc_bypass_th = skip_num;

	if (lc_flag <= lc_bypass_th) {
		lc_flag++;
		pr_amlc_dbg("[%s] lc_flag = %d\n", __func__, lc_flag);
		return;
	}

	if ((vf->type & VIDTYPE_VIU_444) &&
		((vf->flag & VFRAME_FLAG_PC_MODE) || (vf->type & VIDTYPE_PIC) ||
		(vf->flag & VFRAME_FLAG_GAME_MODE)))
		input_422_flag = 0;
	else
		input_422_flag = 1;

	lc_config(lc_en, vf, sps_h_en, sps_v_en,
		sps_w_in, sps_h_in, lc_bitdepth, vpp_index, vp);

	if (lc_bypass_flag <= 0) {
		if (chip_type_id != chip_t3x) {
			set_lc_curve(1, 0, vpp_index);
		} else {
			ve_lc_curve_set(1, 0, lc_szcurve, 0, vpp_index);
		}

		lc_bypass_flag++;
		return;
	}

	if (chip_type_id != chip_t3x) {
		lc_curve_get_blk_num(&blk_hnum, &blk_vnum);

		/*get hist & curve node*/
		if (!use_lc_curve_isr || !lc_curve_isr_defined)
			lc_read_region(blk_vnum, blk_hnum, 0);

		if (lc_change_curve_nodes_en) {
			lc_gain = get_lc_pattern_gain();
			lc_change_pattern_curve_nodes(lc_gain,
				blk_hnum, blk_vnum);
		}

		/*skip lc curve when special pattern*/
		if (lc_special_pattern_detect && special_pattern_detect(vp)) {
			set_lc_curve(1, 0, vpp_index);
			lc_flag = 0xff;
			lc_bypass_flag = 0xff;
			return;
		}

		if (vf && !lc_skip_iir && lc_drv_param.lc_fw_curve_iir)
			/*do time domain iir*/
			lc_drv_param.lc_fw_curve_iir(lc_hist,
				lc_szcurve, blk_vnum, blk_hnum, chip_type_id);

		if (lc_curve_prcnt > 0) { /*debug lc curve node*/
			lc_prt_curve();
			lc_curve_prcnt--;
		}

		if (set_lc_curve(0, 0, vpp_index))
			pr_amlc_dbg("[%s] set lc curve fail\n", __func__);
	} else {
		ve_lc_mapping_ctrl(lc_en, lc_rdma_mode, vpp_index);

		ve_lc_blk_num_get(&blk_hnum, &blk_vnum, 0);

		/*get hist & curve node*/
		/*if (!use_lc_curve_isr || !lc_curve_isr_defined) {*/
		/*	lc_read_region(blk_vnum, blk_hnum, 0);*/
		/*	if (multi_pic_flag)*/
		/*		lc_read_region(blk_vnum, blk_hnum, 1);*/
		/*} else {*/
		/*	pr_amlc_dbg("%s: use_lc_curve_isr/defined: %d/%d\n",*/
		/*		__func__, use_lc_curve_isr, lc_curve_isr_defined);*/
		/*}*/
		lc_read_region(blk_vnum, blk_hnum, 0);

		if (vf && !lc_skip_iir && lc_drv_param.lc_fw_curve_iir)
			/*do time domain iir*/
			lc_drv_param.lc_fw_curve_iir(lc_hist,
				lc_szcurve, blk_vnum, blk_hnum, chip_type_id);

		if (lc_curve_prcnt > 0) { /*debug lc curve node*/
			lc_prt_curve();
			lc_curve_prcnt--;
		}

		ve_lc_curve_set(0, lc_demo_mode, lc_szcurve, 0, vpp_index);
	}

	if (debug_update_dma) {
		VSYNC_WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, dma_cm_en, 2, 1);
		VSYNC_WRITE_VPP_REG_BITS(LC_STTS_CTRL0, dma_lc_en == 1 ? 3 : 0,
			22, 2);//lc_dma_en
		VSYNC_WRITE_VPP_REG_BITS(LC_CURVE_CTRL, dma_lc_en == 1 ? 3 : 0,
			16, 2);//curve dma en
		VSYNC_WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, dma_lc_en == 1 ? 3 : 0,
			3, 2);
		am_dma_update_mif_wr_cm_lc(WR_DMA);
		debug_update_dma = 0;
	}

	if (amlc_debug == 0xc &&
		lc_node_prcnt > 0) {
		lc_node_prcnt--;
		if (lc_node_prcnt == 0)
			amlc_debug = 0x0;
	}

	lc_flag = 0xff;
	lc_bypass_flag = 0xff;
}

void lc_free(void)
{
	kfree(lc_szcurve);
	kfree(curve_nodes_cur);
	kfree(curve_nodes_pre);
	kfree(curve_nodes_pre_raw);
	kfree(lc_hist);

	if (chip_type_id == chip_t3x) {
		kfree(lc_szcurve_slice1);
		kfree(lc_hist_slice1);
	}

	lc_malloc_ok = 0;
}

void lc_slt_en(int enable)
{
	if (enable) {
		lc_init(lc_bitdepth);
		tune_curve_en = 1;
		lc_skip_iir = 1;
		lc_en = 1;

		if (chip_type_id == chip_t3x)
			ve_lc_mapping_ctrl(lc_en, 1, 0);

	} else {
		lc_skip_iir = 0;
		tune_curve_en = 2;
	}
}

void lc_rd_reg(enum lc_reg_lut_e reg_sel, int data_type, char *buf)
{
	int i, j, tmp, tmp1, tmp2, len = 12;
	int lut_data[63] = {0};
	char *stemp = NULL;
	int slice_index = 0;
	unsigned int reg_sat0 = SRSHARP1_LC_SAT_LUT_0_1;
	unsigned int reg_sat = SRSHARP1_LC_SAT_LUT_62;
	unsigned int adr = 16;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg_sat0 = VPP_LC_SATUR_LUT_0;
		reg_sat = VPP_LC_SATUR_LUT_F;
		adr = 12;
	}

	if (chip_type_id == chip_t3x) {
		ve_lc_rd_reg(reg_sel, data_type, buf, slice_index);
		return;
	}

	if (data_type == 1)
		goto dump_as_string;

	switch (reg_sel) {
	case SATUR_LUT:
		for (i = 0; i < 31 ; i++) {
			tmp = READ_VPP_REG(reg_sat0 + i);
			if (chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
				tmp2 = (tmp >> adr) & 0xfff;
				tmp1 = tmp & 0xfff;
			} else {
				tmp1 = (tmp >> adr) & 0xfff;
				tmp2 = tmp & 0xfff;
			}
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		tmp = READ_VPP_REG(reg_sat);
		pr_info("reg_lc_satur_lut[62] =%4d.\n",
			tmp & 0xfff);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_yminVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_yminVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
				i++;
			}
		}
		break;
	case YPKBV_YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			break;

		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ypkBV_ymaxVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ypkBV_ymaxVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		break;
	case YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 6 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
			}

			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
				i++;
			}
		}
		break;
	case YPKBV_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 8 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
					2 * i, tmp1);
				pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
					2 * i + 1, tmp2);
			}
		}
		break;
	case YPKBV_RAT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_RAT);
		pr_info("reg_lc_ypkBV_ratio[0] =%4d.\n",
			(tmp >> 24) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[1] =%4d.\n",
			(tmp >> 16) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[2] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_ypkBV_ratio[3] =%4d.\n",
			tmp & 0xff);
		break;
	case YPKBV_SLP_LMT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_SLP_LMT);
		pr_info("reg_lc_ypkBV_slope_lmt[0] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_ypkBV_slope_lmt[1] =%4d.\n",
			tmp & 0xff);
		break;
	case CNTST_LMT:
		tmp = READ_VPP_REG(LC_CURVE_CONTRAST__LMT_LH);
		pr_info("reg_lc_cntstlmt_low[0] =%4d.\n",
			(tmp >> 24) & 0xff);
		pr_info("reg_lc_cntstlmt_high[0] =%4d.\n",
			(tmp >> 16) & 0xff);
		pr_info("reg_lc_cntstlmt_low[1] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_cntstlmt_high[1] =%4d.\n",
			tmp & 0xff);
		break;
	default:
		break;
	}
	return;

dump_as_string:
	stemp = kzalloc(300, GFP_KERNEL);
	if (!stemp)
		return;
	memset(stemp, 0, 300);
	switch (reg_sel) {
	case SATUR_LUT:
		for (i = 0; i < 31 ; i++) {
			tmp = READ_VPP_REG(reg_sat0 + i);
			if (chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
				tmp2 = (tmp >> adr) & 0xfff;
				tmp1 = tmp & 0xfff;
			} else {
				tmp1 = (tmp >> adr) & 0xfff;
				tmp2 = tmp & 0xfff;
			}
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		tmp = READ_VPP_REG(reg_sat);
		lut_data[62] = tmp & 0xfff;
		for (i = 0; i < 63 ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMINVAL_LMT:
		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			len = 16;
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
				i++;
			}
		}
		for (i = 0; i < len ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			break;

		for (i = 0; i < 6 ; i++) {
			tmp = READ_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}
		for (i = 0; i < 12 ; i++)
			d_convert_str(lut_data[i],
				      i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMAXVAL_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 6 ; i++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
			}

			len = 16;
			for (j = 0; j < 2 ; j++) {
				tmp =
				READ_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
				i++;
			}
		}
		for (i = 0; i < len ; i++)
			d_convert_str(lut_data[i],
				      i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_LMT:
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			for (i = 0; i < 8 ; i++) {
				tmp = READ_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i);
				tmp1 = (tmp >> 16) & 0x3ff;
				tmp2 = tmp & 0x3ff;
				lut_data[2 * i] = tmp1;
				lut_data[2 * i + 1] = tmp2;
			}
			for (i = 0; i < 16 ; i++)
				d_convert_str(lut_data[i],
					      i, stemp, 4, 10);
			memcpy(buf, stemp, 300);
		}
		break;
	case YPKBV_RAT:
		tmp = READ_VPP_REG(LC_CURVE_YPKBV_RAT);
		lut_data[0] = (tmp >> 24) & 0xff;
		lut_data[1] = (tmp >> 16) & 0xff;
		lut_data[2] = (tmp >> 8) & 0xff;
		lut_data[3] = tmp & 0xff;
		for (i = 0; i < 4 ; i++)
			d_convert_str(lut_data[i],
						i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	default:
		break;
	}
	kfree(stemp);
}

void lc_wr_reg(int *p, enum lc_reg_lut_e reg_sel)
{
	int i, j, tmp, tmp1, tmp2;
	unsigned int reg_sat0 = SRSHARP1_LC_SAT_LUT_0_1;
	unsigned int reg_sat = SRSHARP1_LC_SAT_LUT_62;
	unsigned int adr = 16;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg_sat0 = VPP_LC_SATUR_LUT_0;
		reg_sat = VPP_LC_SATUR_LUT_F;
		adr = 12;
	}

	if (chip_type_id != chip_t3x) {
		switch (reg_sel) {
		case SATUR_LUT:
			for (i = 0; i < 31; i++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				if (chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x)
					tmp = ((tmp2 & 0xfff) << adr) | (tmp1 & 0xfff);
				else
					tmp = ((tmp1 & 0xfff) << adr) | (tmp2 & 0xfff);
				WRITE_VPP_REG(reg_sat0 + i, tmp);
			}
			tmp = (*(p + 62)) & 0xfff;
			WRITE_VPP_REG(reg_sat, tmp);
			break;
		case YMINVAL_LMT:
			for (i = 0; i < 6; i++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_0_1 + i, tmp);
			}
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
				for (j = 0; j < 2; j++) {
					tmp1 = *(p + 2 * i);
					tmp2 = *(p + 2 * i + 1);
					tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
					WRITE_VPP_REG(LC_CURVE_YMINVAL_LMT_12_13 + j,
						tmp);
					i++;
				}
			}
			break;
		case YPKBV_YMAXVAL_LMT:
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
				break;

			for (i = 0; i < 6; i++) {
				tmp1 = *(p + 2 * i);
				tmp2 = *(p + 2 * i + 1);
				tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
				WRITE_VPP_REG(LC_CURVE_YPKBV_YMAXVAL_LMT_0_1 + i, tmp);
			}
			break;
		case YMAXVAL_LMT:
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
				for (i = 0; i < 6; i++) {
					tmp1 = *(p + 2 * i);
					tmp2 = *(p + 2 * i + 1);
					tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
					WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_0_1 + i,
						tmp);
				}

				for (j = 0; j < 2; j++) {
					tmp1 = *(p + 2 * i);
					tmp2 = *(p + 2 * i + 1);
					tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
					WRITE_VPP_REG(LC_CURVE_YMAXVAL_LMT_12_13 + j,
						tmp);
					i++;
				}
			}
			break;
		case YPKBV_LMT:
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
				for (i = 0; i < 8; i++) {
					tmp1 = *(p + 2 * i);
					tmp2 = *(p + 2 * i + 1);
					tmp = ((tmp1 & 0x3ff) << 16) | (tmp2 & 0x3ff);
					WRITE_VPP_REG(LC_CURVE_YPKBV_LMT_0_1 + i,
						tmp);
				}
			}
			break;
		case YPKBV_RAT:
			tmp = (*p & 0xff) << 24 | (*(p + 1) & 0xff) << 16 |
				(*(p + 2) & 0xff) << 8 | (*(p + 3) & 0xff);
			WRITE_VPP_REG(LC_CURVE_YPKBV_RAT, tmp);
			break;
		case YPKBV_SLP_LMT:
			tmp = (*p & 0xff) << 8 | (*(p + 1) & 0xff);
			WRITE_VPP_REG(LC_CURVE_YPKBV_SLP_LMT, tmp);
			break;
		case CNTST_LMT:
			tmp = (*p & 0xff) << 24 | (*(p + 1) & 0xff) << 16 |
				(*(p + 2) & 0xff) << 8 | (*(p + 3) & 0xff);
			WRITE_VPP_REG(LC_CURVE_CONTRAST__LMT_LH, tmp);
			break;
		default:
			break;
		}
	} else {
		switch (reg_sel) {
		case SATUR_LUT:
			ve_lc_sat_lut_set(p);
			break;
		case YMINVAL_LMT:
			ve_lc_ymin_lmt_set(p);
			break;
		case YMAXVAL_LMT:
			ve_lc_ymax_lmt_set(p);
			break;
		case YPKBV_LMT:
			ve_lc_ypkbv_lmt_set(p);
			break;
		case YPKBV_RAT:
			ve_lc_ypkbv_rat_set(p);
			break;
		case YPKBV_SLP_LMT:
			ve_lc_ypkbv_slp_lmt_set(p);
			break;
		case CNTST_LMT:
			ve_lc_contrast_lmt_set(p);
			break;
		default:
			break;
		}
	}
}

void lc_load_curve(struct ve_lc_curve_parm_s *p)
{
	/*load lc parms*/
	lc_alg_parm.dbg_parm0 = p->param[lc_dbg_parm0];
	lc_alg_parm.dbg_parm1 = p->param[lc_dbg_parm1];
	lc_alg_parm.dbg_parm2 = p->param[lc_dbg_parm2];
	lc_alg_parm.dbg_parm3 = p->param[lc_dbg_parm3];
	lc_alg_parm.dbg_parm4 = p->param[lc_dbg_parm4];

	if (chip_type_id != chip_t3x) {
		/*load lc_saturation curve*/
		lc_wr_reg(p->ve_lc_saturation, 0x1);
		/*load lc_yminval_lmt*/
		lc_wr_reg(p->ve_lc_yminval_lmt, 0x2);
		/*load lc_ypkbv_ymaxval_lmt*/
		lc_wr_reg(p->ve_lc_ypkbv_ymaxval_lmt, 0x4);
		/*load lc_ypkbv_ratio*/
		lc_wr_reg(p->ve_lc_ypkbv_ratio, 0x8);
		/*load lc_ymaxval_ratio*/
		lc_wr_reg(p->ve_lc_ymaxval_lmt, 0x10);
		/*load lc_ypkbv_lmt*/
		lc_wr_reg(p->ve_lc_ypkbv_lmt, 0x20);
	} else {
		ve_lc_sat_lut_set(p->ve_lc_saturation);
		ve_lc_ymin_lmt_set(p->ve_lc_yminval_lmt);
		ve_lc_ymax_lmt_set(p->ve_lc_ymaxval_lmt);
		ve_lc_ypkbv_lmt_set(p->ve_lc_ypkbv_lmt);
		ve_lc_ypkbv_rat_set(p->ve_lc_ypkbv_ratio);
	}
}

int lc_debug_store(char **parm)
{
	int reg_lut[63] = {0};
	int h, v, i, start_point;
	long val = 0;
	int curve_val[6] = {0};
	char *stemp = NULL;

	stemp = kzalloc(100, GFP_KERNEL);
	if (!stemp)
		return 0;

	memset(stemp, 0, 100);
	memset(lc_dbg_curve, 0, sizeof(char) * 100);

	if (!strcmp(parm[0], "lc")) {
		if (!strcmp(parm[1], "enable"))
			lc_en = 1;
		else if (!strcmp(parm[1], "disable"))
			lc_en = 0;
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "lc_dump_reg")) {
		dump_lc_reg();
	} else if (!strcmp(parm[0], "lc_version")) {
		pr_info("lc driver version :  %s\n", LC_VER);
	} else if (!strcmp(parm[0], "lc_dbg")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		amlc_debug = val;
	} else if (!strcmp(parm[0], "lc_curve_isr")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		use_lc_curve_isr = val;
	} else if (!strcmp(parm[0], "lc_demo_mode")) {
		if (!strcmp(parm[1], "enable"))
			lc_demo_mode = 1;
		else if (!strcmp(parm[1], "disable"))
			lc_demo_mode = 0;
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_data")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (reg_sel == SATUR_LUT)
			lc_rd_reg(SATUR_LUT, 0, NULL);
		else if (reg_sel == YMINVAL_LMT)
			lc_rd_reg(YMINVAL_LMT, 0, NULL);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 0, NULL);
		else if (reg_sel == YMAXVAL_LMT)
			lc_rd_reg(YMAXVAL_LMT, 0, NULL);
		else if (reg_sel == YPKBV_LMT)
			lc_rd_reg(YPKBV_LMT, 0, NULL);
		else if (reg_sel == YPKBV_RAT)
			lc_rd_reg(YPKBV_RAT, 0, NULL);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_rd_reg(YPKBV_SLP_LMT, 0, NULL);
		else if (reg_sel == CNTST_LMT)
			lc_rd_reg(CNTST_LMT, 0, NULL);
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_lut_str")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		lc_dbg_flag |= LC_CUR_RD_UPDATE;
	} else if (!strcmp(parm[0], "lc_wr_lut")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg_sel = val;
		if (!parm[2])
			goto free_buf;
		str_sapr_to_d(parm[2], reg_lut, 5);
		if (reg_sel == SATUR_LUT)
			lc_wr_reg(reg_lut, SATUR_LUT);
		else if (reg_sel == YMINVAL_LMT)
			lc_wr_reg(reg_lut, YMINVAL_LMT);
		else if (reg_sel == YPKBV_YMAXVAL_LMT)
			lc_wr_reg(reg_lut, YPKBV_YMAXVAL_LMT);
		else if (reg_sel == YMAXVAL_LMT)
			lc_wr_reg(reg_lut, YMAXVAL_LMT);
		else if (reg_sel == YPKBV_LMT)
			lc_wr_reg(reg_lut, YPKBV_LMT);
		else if (reg_sel == YPKBV_RAT)
			lc_wr_reg(reg_lut, YPKBV_RAT);
		else if (reg_sel == YPKBV_SLP_LMT)
			lc_wr_reg(reg_lut, YPKBV_SLP_LMT);
		else if (reg_sel == CNTST_LMT)
			lc_wr_reg(reg_lut, CNTST_LMT);
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dump_lc_curve")) {
		if (!strcmp(parm[1], "all")) {
			/*dump curve of one frame*/
			if (ve_multi_picture_case_get() > 0)
				lc_curve_prcnt = 2;
			else
				lc_curve_prcnt = 1;
			amlc_debug = 0x1;
		} else if (!strcmp(parm[1], "slice0")) {
			lc_read_region(8, 12, 0);
			lc_prt_curve();
		} else if (!strcmp(parm[1], "slice1")) {
			lc_read_region(8, 12, 1);
			lc_prt_curve();
		}
	} else if (!strcmp(parm[0], "dump_hist")) {
		if (!strcmp(parm[1], "all")) {
			/*dump all hist of one frame*/
			if (ve_multi_picture_case_get() > 0)
				lc_hist_prcnt = 2;
			else
				lc_hist_prcnt = 1;
			amlc_debug = 0x8;
		} else if (!strcmp(parm[1], "chosen")) {
			/*dump multiple frames in selected area*/
			if (!parm[6])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			lc_hist_hs = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			lc_hist_he = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			lc_hist_vs = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			lc_hist_ve = val;
			if (kstrtoul(parm[6], 10, &val) < 0)
				goto free_buf;
			lc_hist_prcnt = val;
			amlc_debug = 0x6;
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "dump_curve")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_curve_prcnt = val;
	} else if (!strcmp(parm[0], "lc_osd_setting")) {
		if (!strcmp(parm[1], "show")) {
			pr_info("VNUM_STRT_BELOW: %d, VNUM_END_BELOW: %d\n",
				vnum_start_below, vnum_end_below);
			pr_info("VNUM_STRT_ABOVE: %d, VNUM_END_ABOVE: %d\n",
				vnum_start_above, vnum_end_above);
			pr_info("INVALID_BLK: %d, MIN_BV_PERCENT_TH: %d\n",
				invalid_blk, min_bv_percent_th);
		} else if (!strcmp(parm[1], "set")) {
			if (!parm[7])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			vnum_start_below = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			vnum_end_below = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			vnum_start_above = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			vnum_end_above = val;
			if (kstrtoul(parm[6], 10, &val) < 0)
				goto free_buf;
			invalid_blk = val;
			if (kstrtoul(parm[7], 10, &val) < 0)
				goto free_buf;
			min_bv_percent_th = val;
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "osd_iir_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		osd_iir_en = val;
	} else if (!strcmp(parm[0], "iir_refresh")) {
		if (!strcmp(parm[1], "show")) {
			pr_info("current state: alpha1: %d, alpha2: %d, refresh_bit: %d, ts: %d\n",
				alpha1, alpha2, refresh_bit, ts);
		} else if (!strcmp(parm[1], "set")) {
			if (!parm[5])
				goto free_buf;
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			alpha1 = val;
			if (kstrtoul(parm[3], 10, &val) < 0)
				goto free_buf;
			alpha2 = val;
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto free_buf;
			refresh_bit = val;
			if (kstrtoul(parm[5], 10, &val) < 0)
				goto free_buf;
			ts = val;
			pr_info("after setting: alpha1: %d, alpha2: %d, refresh_bit: %d, ts: %d\n",
				alpha1, alpha2, refresh_bit, ts);
		}  else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "scene_change_th")) {
		pr_info("current value: %d\n", scene_change_th);
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		scene_change_th = val;
		pr_info("setting value: %d\n", scene_change_th);
	} else if (!strcmp(parm[0], "irr_dbg_en")) {
		pr_info("current value: %d\n", amlc_iir_debug_en);
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		amlc_iir_debug_en = val;
		pr_info("setting value: %d\n", amlc_iir_debug_en);
	} else if (!strcmp(parm[0], "get_blk_region")) {
		if (chip_type_id == chip_t3x) {
			ve_lc_blk_num_get(&h, &v, 0);
		} else {
			val = READ_VPP_REG(LC_CURVE_HV_NUM);
			h = (val >> 8) & 0x1f;
			v = (val) & 0x1f;
		}
		if (!strcmp(parm[1], "htotal")) {
			lc_temp = h;
			lc_dbg_flag |= LC_PARAM_RD_UPDATE;
		} else if (!strcmp(parm[1], "vtotal")) {
			lc_temp = v;
			lc_dbg_flag |= LC_PARAM_RD_UPDATE;
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "get_hist")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 17;
		for (i = 0; i < 17; i++)
			d_convert_str(lc_hist[start_point + i] >> 4,
						i, stemp, 4, 10);
		memcpy(lc_dbg_curve, stemp, 100);
		lc_dbg_flag |= LC_CUR2_RD_UPDATE;
	} else if (!strcmp(parm[0], "get_curve")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 6;
		for (i = 0; i < 6; i++)
			d_convert_str(curve_nodes_cur[start_point + i],
						i, stemp, 4, 10);
		memcpy(lc_dbg_curve, stemp, 100);
		lc_dbg_flag |= LC_CUR2_RD_UPDATE;
	} else if (!strcmp(parm[0], "set_curve")) {
		if (!parm[3])
			goto free_buf;
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		h = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		v = val;
		if (h > 11 || v > 7)
			goto free_buf;
		start_point = (12 * v + h) * 6;
		str_sapr_to_d(parm[3], curve_val, 5);
		for (i = 0; i < 6; i++)
			curve_nodes_cur[start_point + i] =
			curve_val[i];
	} else if (!strcmp(parm[0], "stop_refresh")) {
		lc_curve_fresh = false;
	} else if (!strcmp(parm[0], "refresh_curve")) {
		lc_curve_fresh = true;
	} else if (!strcmp(parm[0], "reg_lmtrat_sigbin")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_sigbin = val;
		pr_info("reg_lmtrat_sigbin = %d\n",
			lc_tune_curve.lc_reg_lmtrat_sigbin);
	} else if (!strcmp(parm[0], "reg_lmtrat_thd_max")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_thd_max = val;
		pr_info("reg_lmtrat_thd_max = %d\n",
			lc_tune_curve.lc_reg_lmtrat_thd_max);
	} else if (!strcmp(parm[0], "reg_lmtrat_thd_black")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_lmtrat_thd_black = val;
		pr_info("reg_lmtrat_thd_black = %d\n",
			lc_tune_curve.lc_reg_lmtrat_thd_black);
	} else if (!strcmp(parm[0], "reg_thd_black")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.lc_reg_thd_black = val;
		WRITE_VPP_REG_BITS(LC_STTS_BLACK_INFO,
				   lc_tune_curve.lc_reg_thd_black, 0, 8);
		pr_info("reg_thd_black = %d\n",
			lc_tune_curve.lc_reg_thd_black);
	} else if (!strcmp(parm[0], "ypkBV_black_thd")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.ypkbv_black_thd = val;
		pr_info("ypkBV_black_thd = %d\n",
			lc_tune_curve.ypkbv_black_thd);
	} else if (!strcmp(parm[0], "yminV_black_thd")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_tune_curve.yminv_black_thd = val;
		pr_info("yminV_black_thd = %d\n",
			lc_tune_curve.yminv_black_thd);
	} else if (!strcmp(parm[0], "tune_curve_debug")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		lc_node_pos_v = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		lc_node_pos_h = val;
		pr_info("tune_curve_debug pos = v %d h %d\n",
			lc_node_pos_v, lc_node_pos_h);
	} else if (!strcmp(parm[0], "tune_curve_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		tune_curve_en = val;
		amlc_debug = 0xc;
		lc_node_prcnt = 10;
		pr_info("tune_curve_en = %d\n", tune_curve_en);
	} else if (!strcmp(parm[0], "detect_signal_range_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		amlc_debug = 0xe;
		detect_signal_range_en = val;
		pr_info("detect_signal_range_en = %d\n",
			detect_signal_range_en);
	} else if (!strcmp(parm[0], "detect_signal_range_threshold")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		detect_signal_range_threshold_black = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		detect_signal_range_threshold_white = val;
		amlc_debug = 0xe;
		pr_info("detect_signal_range_threshold = %d %d\n",
			detect_signal_range_threshold_black,
			detect_signal_range_threshold_white);
	} else if (!strcmp(parm[0], "clean_error_ro")) {
		clean_lc_stts_overflow();
	} else if (!strcmp(parm[0], "lc_dma_reset")) {
		if (!strcmp(parm[1], "enable"))
			am_dma_reset_lc(1, 1, 3);
		else if (!strcmp(parm[1], "disable"))
			am_dma_reset_lc(0, 1, 3);
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "dma_rdma")) {
		if (!parm[1])
			goto free_buf;
		if (!parm[2])
			goto free_buf;

		if (!strcmp(parm[1], "lc")) {
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			dma_lc_en = val;
			debug_update_dma = 1;
		} else if (!strcmp(parm[1], "cm")) {
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto free_buf;
			dma_cm_en = val;
			debug_update_dma = 1;
		}
	} else {
		pr_info("unsupport cmd!\n");
	}
	kfree(stemp);
	return 0;

free_buf:
	pr_info("Missing parameters !\n");
	kfree(stemp);
	return -EINVAL;
}

#endif
