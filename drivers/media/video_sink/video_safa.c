// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include "video_priv.h"
#include "video_reg.h"
#include "video_common.h"

#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include "../enhancement/amdolby_vision/amdv_uevent.h"
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#include "video_safa_reg.h"

static unsigned int skip_safa_speed_up;
static unsigned int g_postsc_en = 0xff;
static unsigned int g_preh_en = 0xff;
static unsigned int g_preh_rate  = 0xff;
static unsigned int g_prev_en = 0xff;
static unsigned int g_prev_rate  = 0xff;
static unsigned int g_axi_rps_ratio  = 0xff;
static unsigned int asf_enable = 0x1;

u8 safa_dir_interp_en = 1;
bool dejaggy_en = true;
u32 yuv_sharp_en = 1;

struct pi_reg_s {
	u32 pi_dic_num;
	u32 pi_out_scl_mode;
	u32 pi_out_win;
	u32 pi_in_win;
	u32 pi_out_ofst;
};

static struct pi_reg_s pi_reg[3] = {
	{32, 0, 4, 3, 1},
	{48, 1, 6, 3, 2},
	{64, 2, 8, 3, 2}
};

int edge_avgstd_lut2d[5][7] = {
	{0,   0,   0,	0,	 0,   0,   0},	//0
	{63,  48,  32,	8,	 0,   0,   0},	//16
	{63,  63,  48,	16,  8,   0,   0},	//32
	{63,  63,  63,	48,  32,  16,  0},	//48
	{63,  63,  63,	56,  48,  32,  0}	//63
};

int adp_maxsad_gamma_lut2d[9][9] = {
	{ 0,   0,	4,	26,  48,  56,  48,	48,  48},	 //0  gamma
	{ 0,   4,	8,	28,  48,  56,  48,	48,  48},	 //8
	{ 0,   8,  24,	36,  48,  56,  48,	48,  56},	 //16
	{ 0,  24,  32,	40,  48,  56,  48,	56,  56},	 //24
	{32,  40,  40,	44,  48,  48,  56,	56,  56},	 //32
	{40,  48,  48,	48,  48,  48,  56,	56,  56},	 //40
	{48,  48,  48,	52,  56,  56,  56,	56,  56},	 //48
	{56,  56,  56,	56,  56,  56,  56,	56,  63},	 //56
	{63,  63,  63,	63,  63,  63,  63,	63,  63}	 //64
};

#define VSR_TOP_REG_CNT 2
#define VSR_PI_REG_CNT 12
#define VSR_PPS_REG_CNT 19

void dump_vd_vsr_safa_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	struct hw_vsr_safa_reg_s *vsr_reg;
	u32 vsr_top_reg[VSR_TOP_REG_CNT];
	u32 vsr_pi_reg[VSR_PI_REG_CNT];
	u32 vsr_pps_reg[VSR_PPS_REG_CNT];
#if DUMP_REG_NAME
	static const char * const vsr_top_reg_name[] = {
		"vpp_vsr_top_c42c44_mode",
		"vpp_vsr_top_misc",
	};
	static const char * const vsr_pi_reg_name[] = {
		"vpp_pi_en_mode",
		"vpp_pi_misc",
		"vpp_pi_dict_num",
		"vpp_pi_win_ofst",
		"vpp_pi_in_hsc_part",
		"vpp_pi_in_hsc_ini",
		"vpp_pi_in_vsc_part",
		"vpp_pi_in_vsc_ini",
		"vpp_pi_hf_hsc_part",
		"vpp_pi_hf_hsc_ini",
		"vpp_pi_hf_vsc_part",
		"vpp_pi_hf_vsc_ini",
	};
	static const char * const vsr_pps_reg_name[] = {
		"safa_pps_sr_422_en",
		"safa_pps_pre_scale",
		"safa_pps_pre_hscale_coef_y1",
		"safa_pps_pre_hscale_coef_y0",
		"safa_pps_pre_hscale_coef_c1",
		"safa_pps_pre_hscale_coef_c0",
		"safa_pps_pre_vscale_coef",
		"safa_pps_hw_ctrl",
		"safa_pps_interp_en_mode",
		"safa_pps_yuv_sharpen_en",
		"safa_pps_dir_en_mode",
		"safa_pps_sr_alp_info",
		"safa_pps_pi_info",
		"safa_pps_hsc_start_phase_step",
		"safa_pps_vsc_start_phase_step",
		"safa_pps_vsc_init",
		"safa_pps_hsc_init",
		"safa_pps_sc_misc",
		"vpp_sr_en",
	};
#endif

	vsr_reg = &vd_layer[0].vsr_safa_reg;

	pr_info("vsr top regs:\n");
	vsr_top_reg[i++] = vsr_reg->vpp_vsr_top_c42c44_mode;
	vsr_top_reg[i++] = vsr_reg->vpp_vsr_top_misc;
	for (i = 0; i < VSR_TOP_REG_CNT; i++) {
		reg_addr = vsr_top_reg[i];
		if (!reg_addr)
			continue;
		reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
		pr_info("[0x%x] = 0x%x [%s]\n",
			reg_addr, reg_val, vsr_top_reg_name[i]);
#else
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
#endif
	}

	pr_info("vsr pi regs:\n");
	i = 0;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_en_mode;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_misc;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_dict_num;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_win_ofst;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_in_hsc_part;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_in_hsc_ini;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_in_vsc_part;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_in_vsc_ini;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_hf_hsc_part;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_hf_hsc_ini;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_hf_vsc_part;
	vsr_pi_reg[i++] = vsr_reg->vpp_pi_hf_vsc_ini;
	for (i = 0; i < VSR_PI_REG_CNT; i++) {
		reg_addr = vsr_pi_reg[i];
		if (!reg_addr)
			continue;
		reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
		pr_info("[0x%x] = 0x%x [%s]\n",
			reg_addr, reg_val, vsr_pi_reg_name[i]);
#else
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
#endif
	}
	pr_info("vsr safa regs:\n");
	i = 0;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_sr_422_en;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_scale;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_hscale_coef_y1;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_hscale_coef_y0;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_hscale_coef_c1;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_hscale_coef_c0;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pre_vscale_coef;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_hw_ctrl;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_interp_en_mode;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_yuv_sharpen_en;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_dir_en_mode;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_sr_alp_info;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_pi_info;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_hsc_start_phase_step;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_vsc_start_phase_step;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_vsc_init;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_hsc_init;
	vsr_pps_reg[i++] = vsr_reg->safa_pps_sc_misc;
	vsr_pps_reg[i++] = vsr_reg->vpp_sr_en;

	for (i = 0; i < VSR_PPS_REG_CNT; i++) {
		reg_addr = vsr_pps_reg[i];
		if (!reg_addr)
			continue;
		reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
		pr_info("[0x%x] = 0x%x [%s]\n",
			reg_addr, reg_val, vsr_pps_reg_name[i]);
#else
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
#endif
	}
	if (cur_dev->dejaggy_support) {
		reg_addr = vsr_reg->safa_pps_dejaggy_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
	}
	if (cur_dev->frm2fld_support) {
		reg_addr = VPP_P2I_H_V_SIZE;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
	}
};

#define VSR_NONLINEAR_REG_CNT 7

void dump_vd_vsr_safa_nonlinear_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	struct hw_vsr_safa_nonlinear_reg_s *vsr_non_linear_reg;
	u32 vsr_nonlinear_reg[VSR_NONLINEAR_REG_CNT];

	if (!cur_dev->vsr_nonlinear_support)
		return;
	vsr_non_linear_reg = &vd_layer[0].vsr_safa_nonlinear_reg;

	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region12_startp;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region34_startp;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region4_endp;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region0_phase_slop;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region1_phase_slop;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region3_phase_slop;
	vsr_nonlinear_reg[i++] = vsr_non_linear_reg->safa_pps_hsc_region4_phase_slop;
	for (i = 0; i < VSR_NONLINEAR_REG_CNT; i++) {
		reg_addr = vsr_nonlinear_reg[i];
		if (!reg_addr)
			continue;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%x\n",
			reg_addr, reg_val);
	}
}

static int pps_lut_tap4_s11_default[33][4] = {
	{0,   512,	0,	 0},
	{-5,  512,	5,	 0},
	{-10, 511, 11,	 0},
	{-14, 510, 17,	-1},
	{-18, 508, 23,	-1},
	{-22, 506, 29,	-1},
	{-25, 503, 36,	-2},
	{-28, 500, 43,	-3},
	{-32, 496, 51,	-3},
	{-34, 491, 59,	-4},
	{-37, 487, 67,	-5},
	{-39, 482, 75,	-6},
	{-41, 476, 84,	-7},
	{-42, 470, 92,	-8},
	{-44, 463, 102,  -9},
	{-45, 456, 111, -10},
	{-45, 449, 120, -12},
	{-47, 442, 130, -13},
	{-47, 434, 140, -15},
	{-47, 425, 151, -17},
	{-47, 416, 161, -18},
	{-47, 407, 172, -20},
	{-47, 398, 182, -21},
	{-47, 389, 193, -23},
	{-46, 379, 204, -25},
	{-45, 369, 215, -27},
	{-44, 358, 226, -28},
	{-43, 348, 237, -30},
	{-43, 337, 249, -31},
	{-41, 326, 260, -33},
	{-40, 316, 271, -35},
	{-39, 305, 282, -36},
	{-37, 293, 293, -37}
};

static int pps_lut_tap6_s11_default[33][6] = {
	{ 0,   0, 512,	 0,   0,  0},
	{ 2,  -6, 512,	 7,  -2, -1},
	{ 3, -13, 511,	14,  -3,  0},
	{ 5, -19, 510,	21,  -5,  0},
	{ 6, -24, 508,	29,  -7,  0},
	{ 7, -29, 507,	37,  -9, -1},
	{ 8, -34, 504,	45, -11,  0},
	{ 9, -39, 501,	53, -13,  1},
	{10, -44, 498,	62, -16,  2},
	{11, -48, 494,	71, -18,  2},
	{12, -51, 490,	80, -20,  1},
	{13, -55, 486,	89, -23,  2},
	{14, -58, 481,	99, -25,  1},
	{14, -61, 475, 108, -27,  3},
	{15, -64, 470, 118, -30,  3},
	{15, -66, 464, 128, -32,  3},
	{15, -68, 457, 139, -35,  4},
	{16, -70, 450, 149, -37,  4},
	{16, -72, 443, 160, -40,  5},
	{16, -73, 436, 170, -42,  5},
	{16, -74, 428, 181, -45,  6},
	{16, -75, 419, 192, -47,  7},
	{16, -75, 411, 203, -50,  7},
	{16, -76, 402, 214, -52,  8},
	{16, -76, 393, 225, -54,  8},
	{15, -76, 384, 236, -56,  9},
	{15, -75, 374, 247, -59, 10},
	{15, -75, 365, 258, -61, 10},
	{14, -74, 355, 269, -63, 11},
	{14, -73, 344, 281, -65, 11},
	{13, -72, 334, 291, -66, 12},
	{13, -71, 324, 302, -68, 12},
	{13, -70, 313, 313, -70, 13}
};

static int pps_lut_tap8_s11_default[33][8] = {
	{0, 0,	0, 512, 0,	0, 0, 0},
	{-1,  3,  -7, 512,	 7,  -3,  1,  0},
	{-2,  5, -14, 511,	15,  -5,  2,  0},
	{-2,  7, -20, 510,	23,  -8,  3, -1},
	{-3, 10, -27, 509,	31, -11,  3,  0},
	{-4, 12, -32, 507,	39, -14,  4,  0},
	{-4, 14, -38, 504,	48, -17,  5,  0},
	{-5, 16, -43, 501,	57, -19,  6, -1},
	{-5, 18, -49, 498,	66, -22,  7, -1},
	{-6, 19, -53, 495,	75, -26,  8,  0},
	{-6, 21, -58, 490,	85, -29, 10, -1},
	{-6, 22, -62, 486,	94, -32, 11, -1},
	{-7, 24, -66, 481, 104, -35, 12, -1},
	{-7, 25, -69, 476, 114, -38, 13, -2},
	{-7, 26, -72, 470, 124, -41, 14, -2},
	{-8, 27, -75, 464, 134, -44, 15, -1},
	{-8, 28, -78, 458, 145, -47, 16, -2},
	{-8, 29, -80, 451, 155, -50, 17, -2},
	{-8, 30, -83, 444, 166, -53, 18, -2},
	{-8, 31, -84, 437, 177, -56, 19, -4},
	{-8, 31, -86, 429, 188, -59, 20, -3},
	{-9, 32, -87, 421, 199, -62, 22, -4},
	{-8, 32, -88, 413, 209, -64, 23, -5},
	{-8, 32, -89, 405, 220, -67, 24, -5},
	{-9, 33, -89, 396, 231, -69, 25, -6},
	{-8, 33, -90, 387, 242, -72, 25, -5},
	{-8, 33, -90, 377, 253, -74, 26, -5},
	{-8, 32, -89, 368, 264, -76, 27, -6},
	{-8, 32, -89, 358, 275, -78, 28, -6},
	{-8, 32, -88, 348, 286, -80, 29, -7},
	{-7, 32, -88, 338, 297, -82, 29, -7},
	{-7, 31, -86, 328, 307, -84, 30, -7},
	{-8, 31, -85, 318, 318, -85, 31, -8}
};

static void safa_pps_scale_set_coef(u32 SAFA_PPS_CNTL_SCALE_COEF_IDX,
	u32 SAFA_PPS_CNTL_SCALE_COEF,
	u32 cpnt_idx)
{
	int i;

	/*postsc coef lut*/
	/* dir 4tap */
	WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0000);
	for (i = 0; i < 33; i++) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][0] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][0]  & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap4_s11_default[i][1]  & 0xff) << 0));
		/* 4tap */
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][2] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][2]  & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][3] >> 8) & 0xff) << 8) |
			((pps_lut_tap4_s11_default[i][3]  & 0xff) << 0));
	}

	if (cpnt_idx ==  1) {
		/* hor 8tap */
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0080);
		for (i = 0; i < 33; i++) {
			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap8_s11_default[i][0] >> 8) & 0xff) << 24) |
				((pps_lut_tap8_s11_default[i][0]  & 0xff) << 16) |
				(((pps_lut_tap8_s11_default[i][1] >> 8) & 0xff) << 8) |
				((pps_lut_tap8_s11_default[i][1] & 0xff) << 0));

			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap8_s11_default[i][2] >> 8) & 0xff) << 24) |
				((pps_lut_tap8_s11_default[i][2] & 0xff) << 16) |
				(((pps_lut_tap8_s11_default[i][3] >> 8) & 0xff) << 8) |
				((pps_lut_tap8_s11_default[i][3] & 0xff) << 0));
		}

		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0100);
		for (i = 0; i < 33; i++) {
			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap8_s11_default[i][4] >> 8) & 0xff) << 24) |
				((pps_lut_tap8_s11_default[i][4] & 0xff) << 16) |
				(((pps_lut_tap8_s11_default[i][5] >> 8) & 0xff) << 8) |
				((pps_lut_tap8_s11_default[i][5] & 0xff) << 0));
			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap8_s11_default[i][6] >> 8) & 0xff) << 24) |
				((pps_lut_tap8_s11_default[i][6] & 0xff) << 16) |
				(((pps_lut_tap8_s11_default[i][7] >> 8) & 0xff) << 8) |
				((pps_lut_tap8_s11_default[i][7] & 0xff) << 0));
		}
	} else if (cpnt_idx ==  2) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0080);
		for (i = 0; i < 33; i++) {
			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap4_s11_default[i][0] >> 8) & 0xff) << 24) |
				((pps_lut_tap4_s11_default[i][0] & 0xff) << 16) |
				(((pps_lut_tap4_s11_default[i][1] >> 8) & 0xff) << 8) |
				((pps_lut_tap4_s11_default[i][1] & 0xff) << 0));
			WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
				(((pps_lut_tap4_s11_default[i][2] >> 8) & 0xff) << 24) |
				((pps_lut_tap4_s11_default[i][2] & 0xff) << 16) |
				(((pps_lut_tap4_s11_default[i][3] >> 8) & 0xff) << 8) |
				((pps_lut_tap4_s11_default[i][3] & 0xff) << 0));
		}

	}
	/* hor 4tap */
	WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0180);
	for (i = 0; i < 33; i++) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][0] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][0] & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap4_s11_default[i][1] & 0xff) << 0));
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][2] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][2] & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][3] >> 8) & 0xff) << 8) |
			((pps_lut_tap4_s11_default[i][3] & 0xff) << 0));
	}

	/* ver 6tap */
	WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0200);
	for (i = 0; i < 33; i++) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap6_s11_default[i][0] >> 8) & 0xff) << 24) |
			((pps_lut_tap6_s11_default[i][0]  & 0xff) << 16) |
			(((pps_lut_tap6_s11_default[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap6_s11_default[i][1] & 0xff) << 0));

		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap6_s11_default[i][2] >> 8) & 0xff) << 24) |
			((pps_lut_tap6_s11_default[i][2] & 0xff) << 16) |
			(((pps_lut_tap6_s11_default[i][3] >> 8) & 0xff) << 8) |
			((pps_lut_tap6_s11_default[i][3] & 0xff) << 0));
	}

	WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0280);
	for (i = 0; i < 33; i++) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap6_s11_default[i][4] >> 8) & 0xff) << 24) |
			((pps_lut_tap6_s11_default[i][4] & 0xff) << 16) |
			(((pps_lut_tap6_s11_default[i][5] >> 8) & 0xff) << 8) |
			((pps_lut_tap6_s11_default[i][5] & 0xff) << 0));
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap6_s11_default[i][4] >> 8) & 0xff) << 24) |
			((pps_lut_tap6_s11_default[i][4] & 0xff) << 16) |
			(((pps_lut_tap6_s11_default[i][5] >> 8) & 0xff) << 8) |
			((pps_lut_tap6_s11_default[i][5] & 0xff) << 0));
	}

	/* ver 4tap */
	WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF_IDX, 0x0300);
	for (i = 0; i < 33; i++) {
		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][0] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][0] & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap4_s11_default[i][1] & 0xff) << 0));

		WRITE_VCBUS_REG(SAFA_PPS_CNTL_SCALE_COEF,
			(((pps_lut_tap4_s11_default[i][2] >> 8) & 0xff) << 24) |
			((pps_lut_tap4_s11_default[i][2] & 0xff) << 16) |
			(((pps_lut_tap4_s11_default[i][3] >> 8) & 0xff) << 8)  |
			((pps_lut_tap4_s11_default[i][3] & 0xff) << 0));
	}
}

void safa_postsc_coef_lut_config(void)
{
	struct hw_vsr_safa_reg_s *vsr_reg;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	//postsc coef lut config
	if (video_is_after_meson_t6w_cpu()) {
		safa_pps_scale_set_coef(vsr_reg->safa_pps_cntl_scale_coef_idx_luma,
			vsr_reg->safa_pps_cntl_scale_coef_luma, 1);
		safa_pps_scale_set_coef(vsr_reg->safa_pps_cntl_scale_coef_idx_chro,
			vsr_reg->safa_pps_cntl_scale_coef_chro, 2);
	} else {
		safa_pps_scale_set_coef(vsr_reg->safa_pps_cntl_scale_coef_idx_luma,
			vsr_reg->safa_pps_cntl_scale_coef_luma, 1);
		safa_pps_scale_set_coef(vsr_reg->safa_pps_cntl_scale_coef_idx_chro,
			vsr_reg->safa_pps_cntl_scale_coef_chro, 1);
	}
}

static u32 safa_speed_up_handle(struct vsr_setting_s *vsr)
{
	struct vsr_safa_setting_s *vsr_safa = &vsr->vsr_safa;

	u32 hsize_in = vsr->vsr_top.hsize_in;
	u32 vsize_in = vsr->vsr_top.vsize_in;
	u32 hsize_out = vsr->vsr_top.hsize_out;
	u32 vsize_out = vsr->vsr_top.vsize_out;
	u32 pre_hsize = 0;
	const struct vinfo_s *vinfo = get_current_vinfo();
	bool performance_skip = true, performance_hit = false;
	u32 display_active_hsize = 3840, display_active_vsize = 2160;
	u32 display_total_hsize = 4400;
	u32 axi_rps_ratio = 100, mux_in_vsize = 0;
	u32 input_time = 0, display_time = 0;
	u32 clk_in_pps = vpu_clk_get();

	if ((vinfo->width >= 3840 && vinfo->height == 2160) &&
		!skip_safa_speed_up)
		performance_skip = false;

	if (performance_skip)
		return 0;
	display_active_hsize = vinfo->width;
	display_active_vsize = vinfo->height;
	display_total_hsize = vinfo->htotal;

	if (vsr->vsr_top.vskip_cnt) {
		vsize_in = (vsize_in << vsr->vsr_top.vskip_cnt) * 9 / 10;
		if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
			pr_info("%s vsize in adjust :vskip_cnt=%d, vsize_in=%d\n",
				__func__,
				vsr->vsr_top.vskip_cnt,
				vsize_in);
	}

	if (is_amdv_enable())
		axi_rps_ratio = 106;
	if (vsize_out > vsize_in)
		mux_in_vsize = vsize_out;
	else
		mux_in_vsize = vsize_in;

	input_time = ((hsize_in + (display_active_hsize - hsize_out)) *
		mux_in_vsize * 100);
	clk_in_pps /= 1000000;
	if (!clk_in_pps)
		return 0;
	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
		pr_info("input_time=%d, clk_in_pps=%d, display_active_hsize=%d, display_total_hsize=%d\n",
			input_time, clk_in_pps, display_active_hsize,
			display_total_hsize);
	if (g_axi_rps_ratio != 0xff)
		axi_rps_ratio = g_axi_rps_ratio;

	input_time = (input_time / clk_in_pps) * axi_rps_ratio / 10;
	display_time = (display_total_hsize * vsize_out  * 100 / 594 * 10);

	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
		pr_info("%s: input_time=%d, display_time=%d\n",
				__func__,
				input_time, display_time);

	if (input_time >= display_time)
		performance_hit = true;

	if (performance_hit) {
		if (vsr_safa->preh_en &&
			!vsr_safa->postsc_en) {
			vsr_safa->postsc_en = 1;
			if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
				pr_info("%s: preh_en only, force postsc_en\n", __func__);
		}
		if (!vsr_safa->preh_en) {
			vsr_safa->preh_en = 1;
			pre_hsize = (hsize_in + 1) >> 1;
			vsr_safa->preh_ratio = 1;
			if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
				pr_info("%s:preh_en + postcaler case, hsize_in=%d, hsize_out=%d, pre_hsize=%d\n",
					__func__,
					hsize_in, hsize_out, pre_hsize);
		}
	}
	return pre_hsize;
}

static bool is_greater_2k1k380hz_out(const struct vinfo_s *vinfo)
{
	if (vinfo->width >= 1920 && vinfo->height >= 1080 &&
		(vinfo->brr_duration >= 380 ||
		 ((vinfo->sync_duration_num / vinfo->sync_duration_den) > 380)))
		return true;
	else
		return false;
}

static void set_cfg_pi_safa(struct vsr_setting_s *vsr)
{
	struct vsr_pi_setting_s *vsr_pi = &vsr->vsr_pi;
	struct vsr_safa_setting_s *vsr_safa = &vsr->vsr_safa;
	struct vsr_top_setting_s *vsr_top = &vsr->vsr_top;
	u32 hsize_in = vsr->vsr_top.hsize_in;
	u32 vsize_in = vsr->vsr_top.vsize_in;
	u32 hsize_out = vsr->vsr_top.hsize_out;
	u32 vsize_out = vsr->vsr_top.vsize_out;
	u32 out_pi_xsize, out_pi_ysize;
	u32 pre_hsize = 0, pre_vsize = 0;
	u32 pi_scl_rate;
	u32 ratio = 2, ret = 0;
	u32 hsc_ini_phase = 0, hsc_ini_integer = 0;

	if (vsr->vsr_top.input_422_en)
		hsize_in = hsize_in % 2 ? hsize_in + 1 : hsize_in;
	if (!hsize_in)
		hsize_in = 1;
	if (!vsize_in)
		vsize_in = 1;
	if (!hsize_out)
		hsize_out = 1;
	if (!vsize_out)
		vsize_out = 1;
	if (vsr_pi->pi_en) {
		ratio = MIN((vsr_pi->hsize_out / hsize_in),
			(vsr_pi->vsize_out / vsr_pi->vsize_in));
		if (ratio > 4)
			ratio = 4;
		if ((ratio < 2 && vsr->vsr_top.vsr_en) ||
			vsr_pi->hsize_in != vsr->vsr_top.hsize_in) {
			vsr_pi->pi_en = 0;
			if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
				pr_info("%s: the ratio is too small, disable PI\n", __func__);
		}
	}

	if (vsr_pi->pi_en) {
		if (ratio < 2)
			ratio = 2;
		vsr_pi->pi_dict_num = pi_reg[ratio - 2].pi_dic_num;
		vsr_pi->pi_out_scl_mode = pi_reg[ratio - 2].pi_out_scl_mode;
		vsr_pi->pi_out_win = pi_reg[ratio - 2].pi_out_win;
		vsr_pi->pi_in_win = pi_reg[ratio - 2].pi_in_win;
		vsr_pi->pi_out_ofst = pi_reg[ratio - 2].pi_out_ofst;
	} else {
		vsr_pi->pi_out_scl_mode = 0;
	}

	pi_scl_rate = (vsr_pi->pi_out_scl_mode == 0) ?
		2 : (vsr_pi->pi_out_scl_mode == 1) ? 3 : 4;

	out_pi_xsize = hsize_in * pi_scl_rate;
	out_pi_ysize = vsize_in * pi_scl_rate;
	vsr_pi->pi_hf_hsc_integer_part = out_pi_xsize / hsize_out;
	vsr_pi->pi_hf_hsc_fraction_part = div_u64((u64)out_pi_xsize << 24,
		hsize_out) - (vsr_pi->pi_hf_hsc_integer_part << 24);
	vsr_pi->pi_hf_hsc_ini_phase = (div_u64((u64)out_pi_xsize << 16,
		hsize_out) + (1 << 16)) / 2;
	vsr_pi->pi_hf_vsc_integer_part = out_pi_ysize / vsize_out;
	vsr_pi->pi_hf_vsc_fraction_part = div_u64((u64)out_pi_ysize << 24,
		vsize_out) - (vsr_pi->pi_hf_vsc_integer_part << 24);
	vsr_pi->pi_hf_vsc_ini_phase = (div_u64((u64)out_pi_ysize << 16,
		vsize_out) + (1 << 16)) / 2;
	pre_hsize = vsr_safa->preh_en ? vsr_safa->preh_ratio == 0 ?
		hsize_in : (vsr_safa->preh_ratio == 1) ? ((hsize_in + 1) >> 1) :
		vsr_safa->preh_ratio == 2 ? ((hsize_in + 3) >> 2) :
		((hsize_in + 7) >> 3) : hsize_in;
	/*prev en limited by pre_hsize*/
	if (pre_hsize > (glayer_info[0].src_width_max / 2))
		vsr_safa->prev_en = 0;
	pre_vsize  = vsr_safa->prev_en ? (vsr_safa->prev_ratio == 0) ?
		vsize_in : (vsr_safa->prev_ratio == 1) ? ((vsize_in + 1) >> 1) :
		(vsr_safa->prev_ratio == 2) ? ((vsize_in + 3) >> 2) :
		((vsize_in + 7) >> 3) : vsize_in;

	if (video_is_meson_s7d_cpu() ||
		video_is_meson_s6_cpu())
		ret = safa_speed_up_handle(vsr);
	if (ret)
		pre_hsize = ret;
	/*asf need safa enable postsc */
	if (vsr->vsr_top.asf_mode_en)
		vsr_safa->postsc_en = 1;
	if (g_postsc_en != 0xff)
		vsr_safa->postsc_en = g_postsc_en;
	if (g_preh_rate != 0xff)
		vsr_safa->preh_ratio = g_preh_rate;
	if (g_preh_en != 0xff) {
		vsr_safa->preh_en = g_preh_en;
		pre_hsize = vsr_safa->preh_en ? vsr_safa->preh_ratio == 0 ?
			hsize_in : (vsr_safa->preh_ratio == 1) ? ((hsize_in + 1) >> 1) :
			vsr_safa->preh_ratio == 2 ? ((hsize_in + 3) >> 2) :
			((hsize_in + 7) >> 3) : hsize_in;
	}
	if (g_prev_rate != 0xff)
		vsr_safa->prev_ratio = g_prev_rate;
	if (g_prev_en != 0xff) {
		vsr_safa->prev_en = g_prev_en;
		pre_vsize  = vsr_safa->prev_en ? (vsr_safa->prev_ratio == 0) ?
			vsize_in : (vsr_safa->prev_ratio == 1) ? ((vsize_in + 1) >> 1) :
			(vsr_safa->prev_ratio == 2) ? ((vsize_in + 3) >> 2) :
			((vsize_in + 7) >> 3) : vsize_in;
	}

	vsr_safa->pre_hsize = pre_hsize;
	vsr_safa->pre_vsize = pre_vsize;
	vsr_top->pi_safa_hsc_integer_part = pre_hsize / hsize_out;
	vsr_top->pi_safa_hsc_fraction_part = div_u64((u64)pre_hsize << 24,
		hsize_out) - (vsr_top->pi_safa_hsc_integer_part << 24);
	vsr_top->pi_safa_hsc_ini_phase = (div_u64((u64)pre_hsize << 16,
		hsize_out) + (1 << 16)) / 2;
	vsr_top->pi_safa_vsc_integer_part  = pre_vsize / vsize_out;
	vsr_top->pi_safa_vsc_fraction_part = div_u64((u64)pre_vsize << 24,
		vsize_out) - (vsr_top->pi_safa_vsc_integer_part << 24);
	vsr_top->pi_safa_vsc_ini_phase = (div_u64((u64)pre_vsize << 16,
		vsize_out) + (1 << 16)) / 2;
	vsr_top->pi_safa_hsc_ini_integer = 0x1f;
	vsr_top->pi_safa_vsc_ini_integer = 0x1f;

	/* when safa size <= 2048 and scaler up, dejaggy_enable */
	if (cur_dev->dejaggy_support &&
		(hsize_out >= hsize_in || vsize_out >= vsize_in) &&
		!vsr_safa->prev_en) {
		if ((video_is_meson_s6_cpu() ||
			video_is_meson_t6w_cpu() ||
			video_is_meson_t6x_cpu() ||
			video_is_meson_a9_cpu()) &&
			hsize_in <= 2048)
			vsr_safa->dejaggy_en = true & dejaggy_en;
		else if (video_is_meson_t6d_cpu() &&
				(hsize_in <= 1024 ||
				(vsr_top->input_422_en && hsize_in <= 2048)))
			vsr_safa->dejaggy_en = true & dejaggy_en;
		else
			vsr_safa->dejaggy_en = false & dejaggy_en;
	} else {
		vsr_safa->dejaggy_en = false & dejaggy_en;
	}

	if (hsize_out <= 66 || hsize_in <= 35)
		vsr_top->sharpness_en = false & super_scaler;
	else
		vsr_top->sharpness_en = true & super_scaler;

	if (video_is_after_meson_t6w_cpu()) {
		if (pre_hsize < hsize_out) {
			vsr_top->pi_safa_hsc_ini_integer = 0x1f;
			hsc_ini_phase = (div_u64((u64)pre_hsize << 16,
				hsize_out) + (1 << 16)) / 2;
			if (vsr_top->input_422_en) {
				vsr_top->pi_safa_hsc_ini_integer_luma = 0x1f;
				vsr_top->pi_safa_hsc_ini_phase_luma = (hsc_ini_phase / 2) * 2;
				vsr_top->pi_safa_hsc_ini_integer_chrm = 0x1f;
				vsr_top->pi_safa_hsc_ini_phase_chrm  =
					vsr_top->pi_safa_hsc_ini_phase_luma;
			} else {
				vsr_top->pi_safa_hsc_ini_integer_luma = 0x1f;
				vsr_top->pi_safa_hsc_ini_phase_luma = hsc_ini_phase;
				vsr_top->pi_safa_hsc_ini_integer_chrm = 0x1f;
				vsr_top->pi_safa_hsc_ini_phase_chrm = hsc_ini_phase;
			}
		} else {
			hsc_ini_integer = ((div_u64((u64)pre_hsize << 16,
				hsize_out) - (1 << 16)) / 2) >> 16;
			hsc_ini_phase = (div_u64((u64)pre_hsize << 16,
				hsize_out) - (1 << 16)) / 2 -
				(hsc_ini_integer << 16);

			if (vsr_top->input_422_en) {
				vsr_top->pi_safa_hsc_ini_integer_luma = hsc_ini_integer;
				vsr_top->pi_safa_hsc_ini_phase_luma = (hsc_ini_phase / 2) * 2;
				vsr_top->pi_safa_hsc_ini_integer_chrm = hsc_ini_integer / 2;
				vsr_top->pi_safa_hsc_ini_phase_chrm =
					vsr_top->pi_safa_hsc_ini_phase_luma;
			} else {
				vsr_top->pi_safa_hsc_ini_integer_luma = hsc_ini_integer;
				vsr_top->pi_safa_hsc_ini_phase_luma = hsc_ini_phase;
				vsr_top->pi_safa_hsc_ini_integer_chrm = hsc_ini_integer;
				vsr_top->pi_safa_hsc_ini_phase_chrm = hsc_ini_phase;
			}
		}
	} else {
		if (pre_hsize < hsize_out) {
			vsr_top->pi_safa_hsc_ini_integer = 0x1f;
			vsr_top->pi_safa_hsc_ini_phase = (div_u64((u64)pre_hsize << 16,
				hsize_out) + (1 << 16)) / 2;
		} else {
			vsr_top->pi_safa_hsc_ini_integer = ((div_u64((u64)pre_hsize << 16,
				hsize_out) - (1 << 16)) / 2) >> 16;
			vsr_top->pi_safa_hsc_ini_phase = (div_u64((u64)pre_hsize << 16,
				hsize_out) - (1 << 16)) / 2 -
				(vsr_top->pi_safa_hsc_ini_integer << 16);
		}
	}
	if (pre_vsize < vsize_out) {
		vsr_top->pi_safa_vsc_ini_integer = 0x1f;
		vsr_top->pi_safa_vsc_ini_phase = (div_u64((u64)pre_vsize << 16,
			vsize_out) + (1 << 16)) / 2;
	} else {
		vsr_top->pi_safa_vsc_ini_integer = ((div_u64((u64)pre_vsize << 16,
			vsize_out) - (1 << 16)) / 2) >> 16;
		vsr_top->pi_safa_vsc_ini_phase = (div_u64((u64)pre_vsize << 16,
			vsize_out) - (1 << 16)) / 2 -
			(vsr_top->pi_safa_vsc_ini_integer << 16);
	}
	if (vsr_top->asf_mode_en) {
		u32 interlace_safa_vsc_integer_part;
		u32 interlace_safa_vsc_fraction_part;

		vsr_top->safa_vsc_ini_phase = 0;
		vsr_top->safa_vsc_ini_integer = 0;
		interlace_safa_vsc_integer_part = pre_vsize / (vsize_out * 2);
		interlace_safa_vsc_fraction_part = div_u64((u64)pre_vsize << 24,
			vsize_out * 2) - (interlace_safa_vsc_integer_part << 24);
		vsr_top->safa_bot_vsc_ini_phase =
			interlace_safa_vsc_fraction_part & 0xffffff;
		vsr_top->safa_bot_vsc_ini_integer = interlace_safa_vsc_integer_part & 0x1f;
		vsr_top->safa_vsc_integer_part = ((((u64)interlace_safa_vsc_integer_part << 24) +
			 interlace_safa_vsc_fraction_part) * 2) >> 24;
		vsr_top->safa_vsc_fraction_part = ((((u64)interlace_safa_vsc_integer_part << 24) +
			 interlace_safa_vsc_fraction_part) * 2) & 0xffffff;
		if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
			pr_info("%s: interlace integer/fraction:%d, %d, bot_vsc_ini_phase/integer:%d,%d,vsc_integer/fraction:%d,%d\n",
				__func__,
				interlace_safa_vsc_integer_part,
				interlace_safa_vsc_fraction_part,
				vsr_top->safa_bot_vsc_ini_phase,
				vsr_top->safa_bot_vsc_ini_integer,
				vsr_top->safa_vsc_integer_part,
				vsr_top->safa_vsc_fraction_part);
	}
	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA) {
		pr_info("%s:vsr top: h/vsize_in:%d,%d, h/vsize_out:%d, %d, dejaggy_en=%d, is_interlaced=%d, sharpness_en=%d\n",
			__func__,
			hsize_in,
			vsize_in,
			hsize_out,
			vsize_out,
			vsr_safa->dejaggy_en,
			vsr_top->is_interlaced,
			vsr_top->sharpness_en);
		pr_info("%s:safa pre_scaler pre_h/vsize:%d, %d\n",
			__func__,
			pre_hsize,
			pre_vsize);
		pr_info("%s:pi_en:%d, h/vsize_in:%d, %d, h/vsize_out:%d, %d, ratio:%d\n",
			__func__,
			vsr_pi->pi_en,
			vsr_pi->hsize_in,
			vsr_pi->vsize_in,
			vsr_pi->hsize_out,
			vsr_pi->vsize_out,
			ratio);
		pr_info("%s:pi_scl_rate:%d, out_pi_x/ysize:%d, %d\n",
			__func__,
			pi_scl_rate,
			out_pi_xsize,
			out_pi_ysize);
		pr_info("%s pi_hf_sc integer_part: h = %u v= %u, fraction_part: h = %u v = %u, ini_phase: h = %u v = %u\n",
			__func__,
			vsr_pi->pi_hf_hsc_integer_part,
			vsr_pi->pi_hf_vsc_integer_part,
			vsr_pi->pi_hf_hsc_fraction_part,
			vsr_pi->pi_hf_vsc_fraction_part,
			vsr_pi->pi_hf_hsc_ini_phase,
			vsr_pi->pi_hf_vsc_ini_phase);
		pr_info("%s pi_safa_sc integer_part: h = %u v= %u, fraction_part: h = %u v = %u, ini_phase: h = %u v = %u, ini_integer: h = %u v = %u\n",
			__func__,
			vsr_top->pi_safa_hsc_integer_part,
			vsr_top->pi_safa_vsc_integer_part,
			vsr_top->pi_safa_hsc_fraction_part,
			vsr_top->pi_safa_vsc_fraction_part,
			vsr_top->pi_safa_hsc_ini_phase,
			vsr_top->pi_safa_vsc_ini_phase,
			vsr_top->pi_safa_hsc_ini_integer,
			vsr_top->pi_safa_vsc_ini_integer);
	}
}

static void set_vsr_pi(struct vsr_setting_s *vsr)
{
	struct vsr_pi_setting_s *vsr_pi = &vsr->vsr_pi;
	struct vsr_top_setting_s *vsr_top = &vsr->vsr_top;
	u8 vpp_index = vsr->vpp_index;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct hw_vsr_safa_reg_s *vsr_reg;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	if (vsr_pi->pi_en) {
		u32 pi_en = 0;
		/* first disable pi_en, at last enable pi_en */
		rdma_wr(vsr_reg->vpp_pi_en_mode,
			(vsr_pi->pi_out_scl_mode << 0) |
			(pi_en << 4) |
			(0 << 8));
		rdma_wr(vsr_reg->vpp_pi_misc,
			(8 << 16) | //inp_hblan1
			(8 << 8) | //inp_hblan0
			(1 << 1) | //hf_pps_bypss_mode
			(1 << 0));//inp_hold_en
		rdma_wr(vsr_reg->vpp_pi_dict_num, vsr_pi->pi_dict_num);
		rdma_wr(vsr_reg->vpp_pi_win_ofst,
			(vsr_pi->pi_in_win << 0) |
			(vsr_pi->pi_out_ofst << 4) |
			(vsr_pi->pi_out_win << 8));

		vsr_pi->pi_hf_hsc_ini_integer = 0x1f;
		vsr_pi->pi_hf_vsc_ini_integer = 0x1f;
		if (vsr_pi->pi_hf_hsc_ini_integer == 0x1f &&
			(vsr_pi->pi_hf_hsc_ini_phase == (1 << 16))) {
			vsr_pi->pi_hf_hsc_ini_integer = 0;
			vsr_pi->pi_hf_hsc_ini_phase = 0;
		}
		if (vsr_pi->pi_hf_vsc_ini_integer == 0x1f &&
			(vsr_pi->pi_hf_vsc_ini_phase == (1 << 16))) {
			vsr_pi->pi_hf_vsc_ini_integer = 0;
			vsr_pi->pi_hf_vsc_ini_phase = 0;
		}
		rdma_wr(vsr_reg->vpp_pi_in_hsc_part,
			(vsr_top->pi_safa_hsc_fraction_part << 4) |
			(vsr_top->pi_safa_hsc_integer_part << 0));
		if (video_is_after_meson_t6w_cpu())
			rdma_wr(vsr_reg->vpp_pi_in_hsc_ini,
				(vsr_top->pi_safa_hsc_ini_integer_luma << 16) |
				(vsr_top->pi_safa_hsc_ini_phase_luma << 0));
		else
			rdma_wr(vsr_reg->vpp_pi_in_hsc_ini,
				(vsr_top->pi_safa_hsc_ini_integer << 16) |
				(vsr_top->pi_safa_hsc_ini_phase << 0));
		rdma_wr(vsr_reg->vpp_pi_in_vsc_part,
			(vsr_top->pi_safa_vsc_fraction_part << 4) |
			(vsr_top->pi_safa_vsc_integer_part << 0));
		rdma_wr(vsr_reg->vpp_pi_in_vsc_ini,
			(vsr_top->pi_safa_vsc_ini_integer << 16) |
			(vsr_top->pi_safa_vsc_ini_phase << 0));
		rdma_wr(vsr_reg->vpp_pi_hf_hsc_part,
			(vsr_pi->pi_hf_hsc_fraction_part << 4) |
			(vsr_pi->pi_hf_hsc_integer_part << 0));
		rdma_wr(vsr_reg->vpp_pi_hf_hsc_ini,
			(vsr_pi->pi_hf_hsc_ini_integer << 24) |
			(vsr_pi->pi_hf_hsc_ini_phase << 0));
		rdma_wr(vsr_reg->vpp_pi_hf_vsc_part,
			(vsr_pi->pi_hf_vsc_fraction_part << 4) |
			(vsr_pi->pi_hf_vsc_integer_part << 0));
		rdma_wr(vsr_reg->vpp_pi_hf_vsc_ini,
			(vsr_pi->pi_hf_vsc_ini_integer << 24) |
			(vsr_pi->pi_hf_vsc_ini_phase << 0));
	}
	/* at last enable pi_en */
	rdma_wr_bits(vsr_reg->vpp_pi_en_mode,
		(vsr_pi->pi_en), 4, 1);
}

void set_safa_pps(struct vsr_setting_s *vsr)
{
	struct vsr_safa_setting_s *vsr_safa = &vsr->vsr_safa;
	struct vsr_top_setting_s *vsr_top = &vsr->vsr_top;
	u32 postsc_size_mux = 0, dir_info_ds_x_en = 0;
	u32 drt_intp_en = 0, drt_intp_chrm_en = 0, sharp_en = 0;
	u32 pi_vofst = 0, adp_tap_alp_mode = 0;
	u32 beta_mode = 0, adp_ds_x_en = 0;
	u32 sr_delta_alp_mode = 0, sr_gamma_alp_mode = 0;
	u32 pi_gamma_mode = 0, pi_max_sad_mode = 0;
	u32 analy_en = vsr_safa->postsc_en;
	//1 bits prehscaler en
	u32 preh_en = vsr_safa->preh_en;
	//1 bits prevscaler en
	u32 prev_en = vsr_safa->prev_en;
	//2 bits prehor ds ratio 0:1/1 1:1/2 2:1/4 3:1/8
	u32 preh_ratio = vsr_safa->preh_ratio;
	//2 bits prever ds ratio 0:1/1 1:1/2 2:1/4 3:1/8
	u32 prev_ratio = vsr_safa->prev_ratio;
	//1 bits postscaler en
	u32 postsc_en = vsr_safa->postsc_en;
	//1 bits 0: yuv444 pc mode 1: yuv422
	u32 input_422_en = vsr_top->input_422_en;
	u32 safa_pps_top_en = postsc_en | preh_en | prev_en;
	u8 vpp_index = vsr->vpp_index;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct hw_vsr_safa_reg_s *vsr_reg;
	struct hw_vsr_safa_nonlinear_reg_s *vsr_nonlinear_reg;
	u32 filt_num_c = 0, step = 0;
	u32 safa_vsc_ini_phase;
	u32 safa_vsc_ini_integer;
	u32 safa_bot_vsc_ini_phase;
	u32 safa_bot_vsc_ini_integer;
	u32 safa_vsc_integer_part;
	u32 safa_vsc_fraction_part;
	bool is_2k1k_larger_freq;
	const struct vinfo_s *vinfo = get_current_vinfo();

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	if (cur_dev->vsr_nonlinear_support)
		vsr_nonlinear_reg = &vd_layer[0].vsr_safa_nonlinear_reg;
	adp_tap_alp_mode = 1;
	beta_mode        = 1;
	sr_delta_alp_mode = 1;
	sr_gamma_alp_mode = 1;
	pi_gamma_mode    = 1;
	pi_max_sad_mode  = 1;

	is_2k1k_larger_freq = is_greater_2k1k380hz_out(vinfo);
	if (is_2k1k_larger_freq) {
		// > 2k1k 380hz case
		if (vsr_safa->mode == 0 || vsr_safa->mode == 1) {
			step = vsr_safa->pre_hsize <= 512 ? 1 :
				(vsr_safa->pre_hsize <= 1024 ? 2 : 3);
			if (vsr_safa->mode == 1)
				adp_tap_alp_mode = 3;
		} else {
			step = vsr_safa->pre_hsize <= 256  ? 1 :
				(vsr_safa->pre_hsize <= 512 ? 2 : 3);
			drt_intp_chrm_en = 0;
		}
	} else {
		if (vsr_safa->mode == 0 || vsr_safa->mode == 1) {
			step = vsr_safa->pre_hsize <= 1024 ? 1 :
				(vsr_safa->pre_hsize <= 2048 ? 2 : 3);
			if (vsr_safa->mode == 1)
				adp_tap_alp_mode = 3;
		} else {
			step = vsr_safa->pre_hsize <= 512  ? 1 :
				(vsr_safa->pre_hsize <= 1024 ? 2 : 3);
			drt_intp_chrm_en = 0;
		}
	}
	if (vsr->vsr_top.input_422_en) {
		sharp_en = 1;
		pi_vofst = 2;
		analy_en = 1;
		//postsc hsize region
		if (step == 1) {
			postsc_size_mux = 0;
			dir_info_ds_x_en = 0;
			drt_intp_en = 1;
			adp_ds_x_en = 0;
		} else if (step == 2) {
			postsc_size_mux = 0;
			dir_info_ds_x_en = 1;
			drt_intp_en = 1;
			adp_ds_x_en = 1;
		} else {
			postsc_size_mux = 1;
			dir_info_ds_x_en = 1;
			drt_intp_en = 0;
			adp_tap_alp_mode = 3;
			beta_mode = 3;
			sr_delta_alp_mode = 3;
			sr_gamma_alp_mode = 3;
			pi_gamma_mode = 3;
			pi_max_sad_mode = 3;
			adp_ds_x_en = 0;
		}
	} else {
		analy_en = 0;
		sharp_en = 0;
		pi_vofst = 1;
		adp_tap_alp_mode = 3;
		beta_mode = 3;
		sr_delta_alp_mode = 3;
		sr_gamma_alp_mode = 3;
		pi_gamma_mode = 3;
		pi_max_sad_mode = 3;
		/* new add for t6d */
		if (step == 3)
			postsc_size_mux = 1;
		else
			postsc_size_mux = 0;
	}
	//scale down or input hsize over 1024/2048 disable analy_en
	if (is_2k1k_larger_freq) {
		// > 2k1k 380hz
		if ((vsr->vsr_top.hsize_in > vsr->vsr_top.hsize_out ||
			vsr->vsr_top.vsize_in > vsr->vsr_top.vsize_out) ||
			((glayer_info[0].src_height_max == 2160 && vsr->vsr_top.hsize_in > 1024) ||
			(glayer_info[0].src_height_max == 1088 && vsr->vsr_top.hsize_in > 512)))
			analy_en = 0;
		// 1: 1, analy_en = 0;
		if (vsr->vsr_top.hsize_in == vsr->vsr_top.hsize_out &&
			vsr->vsr_top.vsize_in == vsr->vsr_top.vsize_out)
			analy_en = 0;
	} else {
		if ((vsr->vsr_top.hsize_in > vsr->vsr_top.hsize_out ||
			vsr->vsr_top.vsize_in > vsr->vsr_top.vsize_out) ||
			((glayer_info[0].src_height_max == 2160 && vsr->vsr_top.hsize_in > 2048) ||
			(glayer_info[0].src_height_max == 1088 && vsr->vsr_top.hsize_in > 1024)))
			analy_en = 0;
	}
	if (cur_dev->dejaggy_support || cur_dev->vsr_nonlinear_support) {
		/*
		 * if input output 1:1 enable safa_pps_top_en and disable postsc_en
		 * but if enable non_linear need enable postsc_en
		 */
		if (!safa_pps_top_en) {
			safa_pps_top_en = 1;
			if (vsr_safa->nonlinear_4region_en)
				vsr_safa->postsc_en = 1;
			else
				vsr_safa->postsc_en = 0;
		}
		rdma_wr_bits(vsr_reg->safa_pps_dejaggy_ctrl,
			vsr_safa->dejaggy_en, 31, 1);
	}
	//for lc rmeter need postsc_en enable
	if (cur_dev->display_module == T6W_DISPLAY_MODULE)
		analy_en = 1;

	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
		pr_info("%s:preh/v_en:%d, %d, pre_h/vsize:%d, %d, preh/v_ratio:%d, %d, postsc_en:%d step = %d analy_en = %d\n",
			__func__,
			vsr_safa->preh_en,
			vsr_safa->prev_en,
			vsr_safa->pre_hsize,
			vsr_safa->pre_vsize,
			vsr_safa->preh_ratio,
			vsr_safa->prev_ratio,
			vsr_safa->postsc_en,
			step,
			analy_en);
	if (safa_pps_top_en) {
		if (pre_scaler[0].pre_hscaler_ntap == PRE_HSCALER_2TAP ||
			pre_scaler[0].pre_vscaler_ntap == PRE_VSCALER_2TAP)
			filt_num_c = 2;
		else
			filt_num_c = 4;

		//reg config
		rdma_wr(vsr_reg->safa_pps_pre_scale,
			(4 << 16) |
			(filt_num_c << 12) |
			(4 << 8) |
			(preh_ratio << 4) |
			(prev_ratio << 0));
		if (pre_scaler[0].pre_hscaler_ntap == PRE_HSCALER_2TAP ||
			pre_scaler[0].pre_vscaler_ntap == PRE_VSCALER_2TAP) {
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_y1,
				(0 << 16) |
				(0 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_y0,
				(0 << 16) |
				(256 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_c1,
				(0 << 16) |
				(0 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_c0,
				(0 << 16) |
				(256 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_vscale_coef,
				(0 << 16) |
				(256 << 0));
		} else {
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_y1,
				(0 << 16) |
				(0 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_y0,
				(64 << 16) |
				(192 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_c1,
				(0 << 16) |
				(0 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_hscale_coef_c0,
				(64 << 16) |
				(194 << 0));
			rdma_wr(vsr_reg->safa_pps_pre_vscale_coef,
				(64 << 16) |
				(194 << 0));
		}
		rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
			pi_vofst, 9, 2);
		rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
			adp_tap_alp_mode, 8, 2);
		rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
			beta_mode, 12, 2);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_T6D)) {
			/* changed since t6d, s7d bit24, t6d bit25 */
			rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
				drt_intp_en, 25, 1);
			/* new add for t6d */
			rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
				drt_intp_chrm_en, 24, 1);
		} else {
			rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
				drt_intp_en, 24, 1);
		}
		if (video_is_after_meson_t6w_cpu())
			rdma_wr_bits(vsr_reg->safa_pps_hv_adp_tap_wind,
				adp_ds_x_en, 0, 1);
		if (vsr_top->asf_mode_en) {
			safa_vsc_ini_phase = vsr_top->safa_vsc_ini_phase;
			safa_vsc_ini_integer = vsr_top->safa_vsc_ini_integer;
			safa_bot_vsc_ini_phase = vsr_top->safa_bot_vsc_ini_phase;
			safa_bot_vsc_ini_integer = vsr_top->safa_bot_vsc_ini_integer;
			safa_vsc_integer_part = vsr_top->safa_vsc_integer_part;
			safa_vsc_fraction_part = vsr_top->safa_vsc_fraction_part;

		} else {
			safa_vsc_ini_phase = vsr_top->pi_safa_vsc_ini_phase;
			safa_vsc_ini_integer = vsr_top->pi_safa_vsc_ini_integer;
			safa_bot_vsc_ini_phase = vsr_top->pi_safa_vsc_ini_phase << 8;
			safa_bot_vsc_ini_integer = vsr_top->pi_safa_vsc_ini_integer;
			safa_vsc_integer_part = vsr_top->pi_safa_vsc_integer_part;
			safa_vsc_fraction_part = vsr_top->pi_safa_vsc_fraction_part;
		}
		if (cur_dev->yuv_sharpen_support) {
			if (vsr->vsr_top.vsize_in > 1080)
				sharp_en = 0;
			sharp_en &= yuv_sharp_en;
		}
		if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
			pr_info("%s: vsc_ini_integer/integer:%d,%d, bot_vsc_ini_phase/integer:%d,%d,vsc_integer/fraction:%d,%d yuv_sharp_en:%d\n",
				__func__,
				safa_vsc_ini_phase,
				safa_vsc_ini_integer,
				safa_bot_vsc_ini_phase,
				safa_bot_vsc_ini_integer,
				vsr_top->safa_vsc_integer_part,
				vsr_top->safa_vsc_fraction_part,
				sharp_en);
		rdma_wr_bits(vsr_reg->safa_pps_yuv_sharpen_en,
				sharp_en, 4, 1);
		rdma_wr_bits(vsr_reg->safa_pps_dir_en_mode,
			dir_info_ds_x_en, 24, 1);
		rdma_wr_bits(vsr_reg->safa_pps_sr_alp_info,
			sr_delta_alp_mode, 24, 2);
		rdma_wr_bits(vsr_reg->safa_pps_sr_alp_info,
			sr_gamma_alp_mode, 8, 2);
		rdma_wr_bits(vsr_reg->safa_pps_pi_info,
			pi_gamma_mode, 20, 2);
		rdma_wr_bits(vsr_reg->safa_pps_pi_info,
			pi_max_sad_mode, 8, 2);
		rdma_wr(vsr_reg->safa_pps_hsc_start_phase_step,
			((vsr_top->pi_safa_hsc_integer_part & 0xf) << 24) |
			((vsr_top->pi_safa_hsc_fraction_part & 0xffffff) << 0));
		rdma_wr(vsr_reg->safa_pps_vsc_start_phase_step,
			((safa_vsc_integer_part & 0xf) << 24) |
			((safa_vsc_fraction_part & 0xffffff) << 0));
		rdma_wr_bits(vsr_reg->safa_pps_vsc_init,
			safa_vsc_ini_phase, 0, 16);
		rdma_wr_bits(vsr_reg->safa_pps_vsc_init,
			safa_vsc_ini_integer, 16, 5);

		if (video_is_after_meson_t6w_cpu()) {
			/* safa_pps_hsc_init_phase, bit0-15 init phase 0 */
			/* bit16-31 init phase 1*/
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init,
				vsr_top->pi_safa_hsc_ini_phase_luma, 0, 16);
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init,
				vsr_top->pi_safa_hsc_ini_phase_chrm, 16, 16);
			/* safa_pps_hsc_init_interget */
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init_interget,
				vsr_top->pi_safa_hsc_ini_integer_luma, 0, 16);
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init_interget,
				vsr_top->pi_safa_hsc_ini_integer_chrm, 16, 5);
			rdma_wr_bits(vsr_reg->safa_pps_bot_vsc_init,
				safa_bot_vsc_ini_phase, 0, 24);
			rdma_wr_bits(vsr_reg->safa_pps_bot_vsc_init,
				safa_bot_vsc_ini_integer, 24, 5);
			rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
				vsr_top->asf_mode_en & 3, 26, 2);
		} else {
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init,
				vsr_top->pi_safa_hsc_ini_phase, 0, 16);
			rdma_wr_bits(vsr_reg->safa_pps_hsc_init,
				vsr_top->pi_safa_hsc_ini_integer, 16, 5);
			if (!video_is_meson_s7d_cpu())
				rdma_wr_bits(vsr_reg->safa_pps_bot_vsc_init,
					vsr_top->pi_safa_vsc_ini_phase, 0, 16);
		}
	}
	rdma_wr_bits(vsr_reg->safa_pps_sr_422_en,
		input_422_en, 0, 1);
	rdma_wr_bits(vsr_reg->safa_pps_sc_misc,
		prev_en, 4, 1);
	rdma_wr_bits(vsr_reg->safa_pps_sc_misc,
		preh_en, 8, 1);
	rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
		vsr_safa->postsc_en, 2, 1);
	if (cur_dev->vsr_nonlinear_support &&
		vsr_safa->nonlinear_4region_en) {
		//Wr_reg_bits(SAFA_PPS_INTERP_EN_MODE,1,26,1);
		//Wr_reg_bits(SAFA_PPS_HW_CTRL,1,19,1);
		rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
			1, 26, 1);
		rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
			1, 19, 1);
		rdma_wr(vsr_nonlinear_reg->safa_pps_hsc_region12_startp,
			(0 << VPP_REGION1_BIT) |
			((vsr_safa->vpp_hsc_r1 & VPP_REGION_MASK)
			<< VPP_REGION2_BIT));
		rdma_wr(vsr_nonlinear_reg->safa_pps_hsc_region34_startp,
			((vsr_safa->vpp_hsc_r2 & VPP_REGION_MASK)
			<< VPP_REGION3_BIT) |
			((vsr_safa->vpp_hsc_r3 & VPP_REGION_MASK)
			<< VPP_REGION4_BIT));
		rdma_wr(vsr_nonlinear_reg->safa_pps_hsc_region4_endp,
			vsr_safa->vpp_hsc_r3);
		rdma_wr(vsr_reg->safa_pps_hsc_start_phase_step,
			vsr_safa->vpp_hf_start_phase_step);
		rdma_wr(vsr_nonlinear_reg->safa_pps_hsc_region1_phase_slop,
			vsr_safa->vpp_hf_start_phase_slope);
		rdma_wr(vsr_nonlinear_reg->safa_pps_hsc_region3_phase_slop,
			vsr_safa->vpp_hf_end_phase_slope);
	} else {
		rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
			0, 26, 1);
		rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
			0, 19, 1);
	}
	rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
		analy_en, 4, 1);
	rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
		safa_pps_top_en, 8, 1);
	rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
		postsc_size_mux, 1, 1);

	if (video_is_after_meson_t6w_cpu())
		rdma_wr_bits(vsr_reg->vpp_vsr_ofifo,
			(vsr_top->hsize_out - 1), 12, 13);
}

static void set_vsr_input_format(struct vsr_setting_s *vsr)
{
	u32 vsr_en = vsr->vsr_top.vsr_en;
	u8 vpp_index = vsr->vpp_index;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	struct hw_vsr_safa_reg_s *vsr_reg;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	/* config vsr top */
	if (vsr->vsr_top.vsr_en) {
		u32 reg_inp_422 = 0, reg_use_inp_fmt = 1, reg_sync_ctrl = 0;
		u32 reg_444to422_mode = 0, reg_444to422_en = 0;
		u32 reg_422to444_mode = 0, reg_422to444_en = 0;

		if (vsr->vsr_top.input_422_en) {
			//SWPL-195913 pq request
			reg_inp_422 = 1;
			reg_444to422_mode = 0;
			reg_444to422_en = 1;
			reg_422to444_mode = 0;
			reg_422to444_en = 1;
		}
		rdma_wr(vsr_reg->vpp_vsr_top_c42c44_mode,
			(reg_444to422_mode << 0) |
			(reg_444to422_en << 2) |
			(reg_422to444_mode << 4) |
			(reg_422to444_en << 6));

		rdma_wr(vsr_reg->vpp_vsr_top_misc,
			(vsr_en << 0) |
			(reg_inp_422 << 1) |
			(reg_use_inp_fmt << 2) |
			(reg_sync_ctrl << 16));
	} else {
		rdma_wr_bits(vsr_reg->vpp_vsr_top_misc,
			vsr_en, 0, 1);
		vsr->vsr_pi.pi_en = 0;
		vsr->vsr_safa.preh_en = 0;
		vsr->vsr_safa.prev_en = 0;
		vsr->vsr_safa.postsc_en = 0;
	}
	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
		pr_info("%s:vsr_en:%d, input_422_en:%d\n",
			__func__,
			vsr_en,
			vsr->vsr_top.input_422_en);
}

static void set_vsr_input_size(struct vsr_setting_s *vsr)
{
	u8 vpp_index = vsr->vpp_index;
	u32 hsize_in = vsr->vsr_top.hsize_in;
	u32 vsize_in = vsr->vsr_top.vsize_in;
	rdma_wr_op rdma_wr = cur_dev->rdma_func[vpp_index].rdma_wr;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;
	unsigned int hdr_size_dpss_mode = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifdef AMLOGIC_MEDIA_DPSS
	hdr_size_dpss_mode = dpss_mode;
#endif
#endif
#endif

	rdma_wr(VPP_IN_H_V_SIZE,
		((hsize_in & 0x1fff) << 16)
		| (vsize_in & 0x1fff));
	if (vsr->vsr_top.vsr_en) {
		rdma_wr(VPP_LINE_IN_LENGTH, hsize_in);
		rdma_wr(VPP_PIC_IN_HEIGHT, vsize_in);
		rdma_wr(VPP_PREBLEND_H_SIZE, vsize_in << 16 | hsize_in);
	}
	if (cur_dev->display_module == T7_DISPLAY_MODULE)
		rdma_wr(VD1_HDR_IN_SIZE,
			(vsize_in << 16)
			| hsize_in);
	else if (cur_dev->display_module == T6W_DISPLAY_MODULE &&
		!hdr_size_dpss_mode) {
		if (video_is_after_meson_t6x_cpu())
			rdma_wr(VPU_HDR2_SIZE_IN + 1,
				(vsize_in << 16)
				| hsize_in);
		else
			rdma_wr(VPU_HDR2_SIZE_IN,
				(vsize_in << 16)
				| hsize_in);
	}
	if (video_is_after_meson_t6x_cpu()) {
		u32 vd1_pps_ofifo_hsize;

		vd1_pps_ofifo_hsize = (hsize_in & 0x1) == 1 ? hsize_in : (hsize_in - 1);
		rdma_wr_bits(VD1_SCO_FIFO_CTRL,
			vd1_pps_ofifo_hsize, 16, 13);
	}
}

static void set_vd1_frm2fld_en(struct vsr_setting_s *vsr)
{
	u8 vpp_index = vsr->vpp_index;
	struct vinfo_s *vinfo = get_current_vinfo();
	u32 frm2fld_en = 0;
	struct hw_vsr_safa_reg_s *vsr_reg;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	if (vinfo->field_height != vinfo->height && cur_dev->frm2fld_support)
		frm2fld_en = 1;
	if (vinfo->asf_mode && asf_enable)
		vsr->vsr_top.asf_mode_en = 1;
	else
		vsr->vsr_top.asf_mode_en = 0;
	if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA)
		pr_info("%s: asf_mode_en=%d\n",
			__func__, vsr->vsr_top.asf_mode_en);
	if (frm2fld_en) {
		/* VPP_MISC have been moved to vpp_blend_update()
		 * bit4:reg_frm2fld_en bit5:reg_bgn_bot_top
		 *
		 * cur_dev->rdma_func[vpp_index].rdma_wr_bits(VPP_MISC,
		 *	1, 4, 1);
		 * cur_dev->rdma_func[vpp_index].rdma_wr_bits(VPP_MISC,
		 *	0, 5, 1);
		 */
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_P2I_H_V_SIZE,
			vsr->vsr_top.hsize_out << 16 | vsr->vsr_top.vsize_out);
	}
	/*  frm2fld can use VPP_MISC bit4 and bit5 to set or safa_pps_hw_ctrl bit26
	 *	cur_dev->rdma_func[vpp_index].rdma_wr_bits(vsr_reg->safa_pps_hw_ctrl,
	 *	frm2fld_en, 26, 1);
	 */
}

static void sharpness_and_dir_interp_enable(struct vsr_setting_s *vsr)
{
	u8 vpp_index = VPP0;
	int dir_interp_en = safa_dir_interp_en;
	struct hw_vsr_safa_reg_s *vsr_reg;
	rdma_wr_bits_op rdma_wr_bits = cur_dev->rdma_func[vpp_index].rdma_wr_bits;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	//scale up or input size over 1024/2048 disable dir_interp_en
	if ((vsr->vsr_top.hsize_in > vsr->vsr_top.hsize_out ||
		vsr->vsr_top.vsize_in > vsr->vsr_top.vsize_out) ||
		((glayer_info[0].src_height_max == 2160 && vsr->vsr_top.hsize_in > 2048) ||
		(glayer_info[0].src_height_max == 1088 && vsr->vsr_top.hsize_in > 1024)))
		dir_interp_en = 0;
	rdma_wr_bits(vsr_reg->safa_pps_interp_en_mode,
		dir_interp_en, 25, 1);
	rdma_wr_bits(vsr_reg->vpp_sr_en,
		vsr->vsr_top.sharpness_en ? 1 : 0, 0, 1);
}

void set_vsr_scaler(struct vsr_setting_s *vsr)
{
	vpu_module_clk_enable(vsr->vpp_index, SAFA_PI, 0);
	vpu_module_clk_enable(vsr->vpp_index, SAFA_POSTSC, 0);
	vpu_module_clk_enable(vsr->vpp_index, SAFA_PREH, 0);
	vpu_module_clk_enable(vsr->vpp_index, SAFA_PREV, 0);
	set_vsr_input_size(vsr);
	set_vsr_input_format(vsr);
	set_vd1_frm2fld_en(vsr);
	if (vsr->vsr_top.vsr_en) {
		set_cfg_pi_safa(vsr);
		set_vsr_pi(vsr);
		set_safa_pps(vsr);
	}
	sharpness_and_dir_interp_enable(vsr);
	if (!vsr->vsr_pi.pi_en)
		vpu_module_clk_disable(vsr->vpp_index, SAFA_PI, 0);
	if (!vsr->vsr_safa.postsc_en)
		vpu_module_clk_disable(vsr->vpp_index, SAFA_POSTSC, 0);
	if (!vsr->vsr_safa.preh_en)
		vpu_module_clk_disable(vsr->vpp_index, SAFA_PREH, 0);
	if (!vsr->vsr_safa.prev_en)
		vpu_module_clk_disable(vsr->vpp_index, SAFA_PREV, 0);
}

void set_dither_mode(int dither_mode)
{
	if (video_is_meson_s7d_cpu()) {
		WRITE_VCBUS_REG_BITS(VPP_VSR_DITHER_MODE,
			(dither_mode & 0x3), 0, 2);
	}
}

#ifdef lut_data_load
static void load_9x9_lut(u32 reg, int *lut_coef)
{
	int i;
	u32 lut_data[3];

	for (i = 0; i < 9; i++) {
		lut_data[2] = (u32)(lut_coef[i * 9 + 5] & 0xff) << 0 |
			(u32)(lut_coef[i * 9 + 6] & 0xff) << 8 |
			(u32)(lut_coef[i * 9 + 7] & 0xff) << 16 |
			(u32)(lut_coef[i * 9 + 8] & 0xff) << 24;

		lut_data[1] = (u32)(lut_coef[i * 9 + 1] & 0xff) << 0 |
			(u32)(lut_coef[i * 9 + 2] & 0xff) << 8 |
			(u32)(lut_coef[i * 9 + 3] & 0xff) << 16 |
			(u32)(lut_coef[i * 9 + 4] & 0xff) << 24;

		lut_data[0] = (u32)((lut_coef[i * 9] & 0xff) << 0);

		WRITE_VCBUS_REG(reg + i * 3, lut_data[2]);
		WRITE_VCBUS_REG(reg + i * 3 + 1, lut_data[1]);
		WRITE_VCBUS_REG(reg + i * 3 + 2, lut_data[0]);
		pr_info("reg[0x%x]=0x%x\n", reg + i * 3, lut_data[2]);
		pr_info("reg[0x%x]=0x%x\n", reg + i * 3 + 1, lut_data[1]);
		pr_info("reg[0x%x]=0x%x\n", reg + i * 3 + 2, lut_data[0]);
	}
}

static void load_5x7_lut(u32 reg, int *lut_coef)
{
	int i;
	u32 lut_data[2];

	for (i = 0; i < 5; i++) {
		lut_data[1] = (u32)(lut_coef[i * 7 + 3] & 0xff) << 0 |
			(u32)(lut_coef[i * 7 + 4] & 0xff) << 8 |
			(u32)(lut_coef[i * 7 + 5] & 0xff) << 16 |
			(u32)(lut_coef[i * 7 + 6] & 0xff) << 24;

		lut_data[0] = (u32)(lut_coef[i * 7] & 0xff) << 0 |
			(u32)(lut_coef[i * 7 + 1] & 0xff) << 8 |
			(u32)(lut_coef[i * 7 + 2] & 0xff) << 16;

		WRITE_VCBUS_REG(reg + i * 2, lut_data[1]);
		WRITE_VCBUS_REG(reg + i * 2 + 1, lut_data[0]);
		pr_info("reg[0x%x]=0x%x\n", reg + i * 2, lut_data[1]);
		pr_info("reg[0x%x]=0x%x\n", reg + i * 2 + 1, lut_data[0]);
	}
}
#endif

void s7d_vsr_default_init(void)
{
#ifdef lut_data_load
	load_5x7_lut(SAFA_PPS_EDGE_AVGSTD_LUT2D_0_0,
		&edge_avgstd_lut2d[0][0]);
	load_9x9_lut(VPP_PI_MAXSAD_GAMMA_LUT2D_0_0_0,
		&adp_maxsad_gamma_lut2d[0][0]);
#endif
	WRITE_VCBUS_REG(SAFA_PPS_EDGE_AVGSTD_LUT2D_F_1_0, 0x20303f);
	WRITE_VCBUS_REG(SAFA_PPS_EDGE_AVGSTD_LUT2D_F_2_0, 0x303f3f);
	WRITE_VCBUS_REG(SAFA_PPS_EDGE_AVGSTD_LUT2D_F_3_0, 0x3f3f3f);
	WRITE_VCBUS_REG(SAFA_PPS_EDGE_AVGSTD_LUT2D_F_4_0, 0x3f3f3f);

	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_0_0, 0x30303038);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_0_0, 0x301a0400);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_0_0, 0x00000000);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_1_0, 0x30303038);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_1_0, 0x301c0804);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_1_0, 0x00000000);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_2_0, 0x38303038);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_2_0, 0x30241808);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_2_0, 0x00000000);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_3_0, 0x38383038);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_3_0, 0x30282018);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_3_0, 0x00000008);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_4_0, 0x38383830);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_4_0, 0x302c2828);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_4_0, 0x00000020);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_5_0, 0x38383830);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_5_0, 0x30303030);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_5_0, 0x00000028);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_6_0, 0x38383838);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_6_0, 0x38343030);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_6_0, 0x00000030);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_7_0, 0x3f383838);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_7_0, 0x38383838);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_7_0, 0x00000038);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_8_0, 0x3f3f3f3f);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_8_0, 0x3f3f3f3f);
	WRITE_VCBUS_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_8_0, 0x0000003f);

	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_0, 0x7c7e7f80);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_1, 0x76787a7b);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_2, 0x6e707274);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_3, 0x65686a6c);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_4, 0x5b5e6163);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_5, 0x51545659);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_6, 0x46494b4e);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_7, 0x41424344);
	WRITE_VCBUS_REG(VPP_PI_HF_SCL_COEF_F, 0x00000040);
}

void vsr_default_init(void)
{
	struct hw_vsr_safa_reg_s *vsr_reg;

	vsr_reg = &vd_layer[0].vsr_safa_reg;
	/*
	 *SAFA_PPS_DIR_MIN_IDX_VALID bit0 set 1 to enhanced interpolation
	 */
	if (video_is_meson_s7d_cpu() ||
		video_is_meson_s6_cpu())
		WRITE_VCBUS_REG_BITS(SAFA_PPS_DIR_MIN_IDX_VALID, 1, 0, 1);
	if (video_is_meson_t6d_cpu()) {
		WRITE_VCBUS_REG_BITS(T6D_SAFA_PPS_DIR_MIN_IDX_VALID, 1, 0, 1);
		WRITE_VCBUS_REG_BITS(T6D_SAFA_PPS_SAD_FLAT_THD, 0x18, 0, 8);
	}
	WRITE_VCBUS_REG_BITS(vsr_reg->safa_pps_sc_misc, 1, 12, 1);
}

void vsr_debug_mode_update(u32 debug_mode, struct vsr_setting_s *vsr)
{
	u32 hsize_in = 0, vsize_in = 0;
	u32 out_size = 0, out_scope_x = 0, out_scope_y = 0;
	u32 vpp_index = 0;

	if (debug_mode >= 1 && debug_mode <= 8) {
		hsize_in = vsr->vsr_top.hsize_in;
		vsize_in = vsr->vsr_top.vsize_in;
		vpp_index = vsr->vpp_index;

		if (hsize_in > 1024) {
			out_size = (hsize_in / 2) << 16 | vsize_in;
			out_scope_x = hsize_in / 2 - 1;
			out_scope_y = vsize_in;
		} else {
			out_size = hsize_in << 16 | vsize_in;
			out_scope_x = hsize_in - 1;
			out_scope_y = vsize_in;
		}
		if (debug_common_flag & DEBUG_FLAG_COMMON_SAFA) {
			pr_info("%s, hsize_in=%d, vsize_in=%d, out_size=0x%x, out_scope_x=0x%x, out_scope_y=0x%x\n",
				__func__,
				hsize_in, vsize_in, out_size,
				out_scope_x,
				out_scope_y);
			pr_info("before setting, 0x%x, 0x%x, 0x%x, 0x%x\n",
				READ_VCBUS_REG(VPP_VE_H_V_SIZE),
				READ_VCBUS_REG(VPP_OUT_H_V_SIZE),
				READ_VCBUS_REG(VPP_POSTBLEND_VD1_H_START_END),
				READ_VCBUS_REG(VPP_POSTBLEND_VD1_V_START_END));
		}
		//cur_dev->rdma_func[vpp_index].rdma_wr(VPP_VSR_DEBUG_MODE,
		//	debug_mode);
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_VE_H_V_SIZE, out_size);
		cur_dev->rdma_func[vpp_index].rdma_wr(VPP_OUT_H_V_SIZE, out_size);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_POSTBLEND_VD1_H_START_END, out_scope_x);
		cur_dev->rdma_func[vpp_index].rdma_wr
			(VPP_POSTBLEND_VD1_V_START_END, out_scope_y);
	}
}

struct video_module_debug_s debug_video_safa[8] = {
	{"skip_safa_speed_up", &skip_safa_speed_up, 1, 0},
	{"g_postsc_en", &g_postsc_en, 1, 0},
	{"g_preh_en", &g_preh_en, 1, 0},
	{"g_preh_rate", &g_preh_rate, 1, 0},
	{"g_prev_en", &g_prev_en, 1, 0},
	{"g_prev_rate", &g_prev_rate, 1, 0},
	{"g_axi_rps_ratio", &g_axi_rps_ratio, 1, 0},
	{"asf_enable", &asf_enable, 1, 0},
};

