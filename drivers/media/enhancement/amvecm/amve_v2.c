// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
/* #include <linux/amlogic/aml_common.h> */
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/utils/amstream.h>
#include "arch/vpp_regs_v2.h"
#include "amve_v2.h"
#include <linux/io.h>
#include "reg_helper.h"
#include "local_contrast.h"
#include "am_dma_ctrl.h"
#include "set_hdr2_v0.h"
#include "arch/vpp_s5_hdr_regs.h"

int vev2_dbg;

#define pr_amve_v2(fmt, args...)\
	do {\
		if (vev2_dbg & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\

int multi_picture_case;
int multi_slice_case;
int dnlp_slice_num_changed;
int lc_slice_num_changed;
int hist_dma_case = 1;
int delay = 4;
int counter = 4;

/*ve module slice0 offset*/
unsigned int ve_addr_offset = 0x600;

/*ve module slice1~slice3 offset*/
unsigned int ve_reg_ofst[3] = {
	0x0, 0x100, 0x200
};

unsigned int pst_reg_ofst[4] = {
	0x0, 0x100, 0x700, 0x1900
};

/*sr sharpness module slice0~slice1 offset*/
unsigned int sr_sharp_reg_ofst[4] = {
	0x0, 0x2500, 0x0, 0x0
};

/*lc curve module slice0~slice1 offset*/
unsigned int lc_reg_ofst[4] = {
	0x0, 0x40, 0x0, 0x0
};

int lc_h_count_ini_phs;
static struct vd_proc_amvecm_info_t *vd_info;
static int vi_hist_en;
static int eye_proc_en;

unsigned int lc_overlap_s0 = 32;
unsigned int lc_overlap = 32;

int get_slice_max(void)
{
	if (chip_type_id == chip_s5)
		return SLICE_MAX;
	else if (chip_type_id == chip_t3x)
		return SLICE2;
	else
		return SLICE1;
}

static void ve_brightness_cfg(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int vpp_index)
{
	int reg;
	int slice_max;

	slice_max = get_slice_max();
	if (slice >= slice_max)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0)
			if (chip_type_id == chip_t3x)
				reg = VPP_SLICE1_VADJ1_Y + ve_addr_offset;
			else
				reg = VPP_VADJ1_Y;
		else
			reg = VPP_SLICE1_VADJ1_Y + ve_reg_ofst[slice - 1];
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_Y + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_BITS_S5(reg, val, 8, 11);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, val, 8, 11, vpp_index);

	pr_amve_v2("brigtness: val = %d, slice = %d\n", val, slice);
	pr_amve_v2("brigtness: addr = %x\n", reg);
}

static void ve_contrast_cfg(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int vpp_index)
{
	int reg;
	int slice_max;

	slice_max = get_slice_max();

	if (slice >= slice_max)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0)
			if (chip_type_id == chip_t3x)
				reg = VPP_SLICE1_VADJ1_Y + ve_addr_offset;
			else
				reg = VPP_VADJ1_Y;
		else
			reg = VPP_SLICE1_VADJ1_Y + ve_reg_ofst[slice - 1];
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_Y + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_BITS_S5(reg, val, 0, 8);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, val, 0, 8, vpp_index);

	pr_amve_v2("contrast: val = %d, slice = %d\n", val, slice);
	pr_amve_v2("contrast: addr = %x\n", reg);
}

static void ve_sat_hue_mab_cfg(int mab,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int vpp_index)
{
	int reg_mab;
	int slice_max;

	slice_max = get_slice_max();

	if (slice >= slice_max)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			if (chip_type_id == chip_t3x)
				reg_mab = VPP_SLICE1_VADJ1_MA_MB + ve_addr_offset;
			else
				reg_mab = VPP_VADJ1_MA_MB;
		} else {
			reg_mab = VPP_SLICE1_VADJ1_MA_MB + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		reg_mab = VPP_VADJ2_MA_MB + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_S5(reg_mab, mab);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_mab, mab, vpp_index);

	pr_amve_v2("sat_hue: mab = %d, slice = %d\n", mab, slice);
	pr_amve_v2("sat_hue: addr = %x\n", reg_mab);
}

static void ve_sat_hue_mcd_cfg(int mcd,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice,
	int vpp_index)
{
	int reg_mcd;
	int slice_max;

	slice_max = get_slice_max();

	if (slice >= slice_max)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			if (chip_type_id == chip_t3x)
				reg_mcd = VPP_SLICE1_VADJ1_MC_MD + ve_addr_offset;
			else
				reg_mcd = VPP_VADJ1_MC_MD;
		} else {
			reg_mcd = VPP_SLICE1_VADJ1_MC_MD + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		reg_mcd = VPP_VADJ2_MC_MD + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_S5(reg_mcd, mcd);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_mcd, mcd, vpp_index);

	pr_amve_v2("sat_hue: mcd = %d, slice = %d\n", mcd, slice);
	pr_amve_v2("sat_hue: addr = %x\n", reg_mcd);
}

void ve_brigtness_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode,
	int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		ve_brightness_cfg(val, mode, vadj_idx, i, vpp_index);
}

void ve_contrast_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode,
	int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		ve_contrast_cfg(val, mode, vadj_idx, i, vpp_index);
}

void ve_color_mab_set(int mab,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode,
	int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		ve_sat_hue_mab_cfg(mab, mode, vadj_idx, i, vpp_index);
}

void ve_color_mcd_set(int mcd,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode,
	int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		ve_sat_hue_mcd_cfg(mcd, mode, vadj_idx, i, vpp_index);
}

int ve_brightness_contrast_get(enum vadj_index_e vadj_idx)
{
	int val = 0;
	unsigned int reg = VPP_VADJ1_Y;

	if (vadj_idx == VE_VADJ1)
		if (chip_type_id == chip_t3x)
			reg = VPP_SLICE1_VADJ1_Y + ve_addr_offset;
		else
			reg = VPP_VADJ1_Y;
	else if (vadj_idx == VE_VADJ2)
		reg = VPP_VADJ2_Y;

	val = READ_VPP_REG(reg);
	pr_amve_v2("get vadj_y: addr = %x\n", reg);
	return val;
}

void vpp_mtx_config_v2(struct matrix_coef_s *coef,
	enum wr_md_e mode, enum vpp_slice_e slice,
	enum vpp_matrix_e mtx_sel,
	int vpp_index)
{
	int reg_pre_offset0_1 = 0;
	int reg_pre_offset2 = 0;
	int reg_coef00_01 = 0;
	int reg_coef02_10 = 0;
	int reg_coef11_12 = 0;
	int reg_coef20_21 = 0;
	int reg_coef22 = 0;
	int reg_offset0_1 = 0;
	int reg_offset2 = 0;
	int reg_en_ctl = 0;
	int slice_max;

	slice_max = get_slice_max();

	switch (slice) {
	case SLICE0:
		if (mtx_sel == VD1_MTX) {
			if (chip_type_id == chip_t3x) {
				reg_pre_offset0_1 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET0_1 +
					ve_addr_offset;
				reg_pre_offset2 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET2 +
					ve_addr_offset;
				reg_coef00_01 = VPP_SLICE1_VD1_MATRIX_COEF00_01 +
					ve_addr_offset;
				reg_coef02_10 = VPP_SLICE1_VD1_MATRIX_COEF02_10 +
					ve_addr_offset;
				reg_coef11_12 = VPP_SLICE1_VD1_MATRIX_COEF11_12 +
					ve_addr_offset;
				reg_coef20_21 = VPP_SLICE1_VD1_MATRIX_COEF20_21 +
					ve_addr_offset;
				reg_coef22 = VPP_SLICE1_VD1_MATRIX_COEF22 +
					ve_addr_offset;
				reg_offset0_1 = VPP_SLICE1_VD1_MATRIX_OFFSET0_1 +
					ve_addr_offset;
				reg_offset2 = VPP_SLICE1_VD1_MATRIX_OFFSET2 +
					ve_addr_offset;
				reg_en_ctl = VPP_SLICE1_VD1_MATRIX_EN_CTRL +
					ve_addr_offset;
			} else {
				reg_pre_offset0_1 = VPP_VD1_MATRIX_PRE_OFFSET0_1;
				reg_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
				reg_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
				reg_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
				reg_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
				reg_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
				reg_coef22 = VPP_VD1_MATRIX_COEF22;
				reg_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
				reg_offset2 = VPP_VD1_MATRIX_OFFSET2;
				reg_en_ctl = VPP_VD1_MATRIX_EN_CTRL;
			}
		} else if (mtx_sel == POST2_MTX) {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
			reg_coef22 = VPP_POST2_MATRIX_COEF22;
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2;
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL;
		} else if (mtx_sel == POST_MTX) {
			reg_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
			reg_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_POST_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_POST_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_POST_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_POST_MATRIX_COEF20_21;
			reg_coef22 = VPP_POST_MATRIX_COEF22;
			reg_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_POST_MATRIX_OFFSET2;
			reg_en_ctl = VPP_POST_MATRIX_EN_CTRL;
		} else {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
			reg_coef22 = VPP_POST2_MATRIX_COEF22;
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2;
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL;
		}
		break;
	case SLICE1:
	case SLICE2:
	case SLICE3:
		if (mtx_sel == VD1_MTX) {
			reg_pre_offset0_1 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET0_1 +
				ve_reg_ofst[slice - 1];
			reg_pre_offset2 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET2 +
				ve_reg_ofst[slice - 1];
			reg_coef00_01 = VPP_SLICE1_VD1_MATRIX_COEF00_01 +
				ve_reg_ofst[slice - 1];
			reg_coef02_10 = VPP_SLICE1_VD1_MATRIX_COEF02_10 +
				ve_reg_ofst[slice - 1];
			reg_coef11_12 = VPP_SLICE1_VD1_MATRIX_COEF11_12 +
				ve_reg_ofst[slice - 1];
			reg_coef20_21 = VPP_SLICE1_VD1_MATRIX_COEF20_21 +
				ve_reg_ofst[slice - 1];
			reg_coef22 = VPP_SLICE1_VD1_MATRIX_COEF22 +
				ve_reg_ofst[slice - 1];
			reg_offset0_1 = VPP_SLICE1_VD1_MATRIX_OFFSET0_1 +
				ve_reg_ofst[slice - 1];
			reg_offset2 = VPP_SLICE1_VD1_MATRIX_OFFSET2 +
				ve_reg_ofst[slice - 1];
			reg_en_ctl = VPP_SLICE1_VD1_MATRIX_EN_CTRL +
				ve_reg_ofst[slice - 1];
		} else if (mtx_sel == POST2_MTX) {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2 +
				pst_reg_ofst[slice];
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10 +
				pst_reg_ofst[slice];
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12 +
				pst_reg_ofst[slice];
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21 +
				pst_reg_ofst[slice];
			reg_coef22 = VPP_POST2_MATRIX_COEF22 +
				pst_reg_ofst[slice];
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2 +
				pst_reg_ofst[slice];
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL +
				pst_reg_ofst[slice];
		} else if (mtx_sel == POST_MTX) {
			reg_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2 +
				pst_reg_ofst[slice];
			reg_coef00_01 = VPP_POST_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_coef02_10 = VPP_POST_MATRIX_COEF02_10 +
				pst_reg_ofst[slice];
			reg_coef11_12 = VPP_POST_MATRIX_COEF11_12 +
				pst_reg_ofst[slice];
			reg_coef20_21 = VPP_POST_MATRIX_COEF20_21 +
				pst_reg_ofst[slice];
			reg_coef22 = VPP_POST_MATRIX_COEF22 +
				pst_reg_ofst[slice];
			reg_offset0_1 = VPP_POST_MATRIX_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_offset2 = VPP_POST_MATRIX_OFFSET2 +
				pst_reg_ofst[slice];
			reg_en_ctl = VPP_POST_MATRIX_EN_CTRL +
				pst_reg_ofst[slice];
		} else {
			reg_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2 +
				pst_reg_ofst[slice];
			reg_coef00_01 = VPP_POST2_MATRIX_COEF00_01 +
				pst_reg_ofst[slice];
			reg_coef02_10 = VPP_POST2_MATRIX_COEF02_10 +
				pst_reg_ofst[slice];
			reg_coef11_12 = VPP_POST2_MATRIX_COEF11_12 +
				pst_reg_ofst[slice];
			reg_coef20_21 = VPP_POST2_MATRIX_COEF20_21 +
				pst_reg_ofst[slice];
			reg_coef22 = VPP_POST2_MATRIX_COEF22 +
				pst_reg_ofst[slice];
			reg_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1 +
				pst_reg_ofst[slice];
			reg_offset2 = VPP_POST2_MATRIX_OFFSET2 +
				pst_reg_ofst[slice];
			reg_en_ctl = VPP_POST2_MATRIX_EN_CTRL +
				pst_reg_ofst[slice];
		}
		break;
	default:
		return;
	}

	switch (mode) {
	case WR_VCB:
		WRITE_VPP_REG(reg_pre_offset0_1,
			(coef->pre_offset[0] << 16) | coef->pre_offset[1]);
		WRITE_VPP_REG(reg_pre_offset2, coef->pre_offset[2]);
		WRITE_VPP_REG(reg_coef00_01,
			(coef->matrix_coef[0][0] << 16) | coef->matrix_coef[0][1]);
		WRITE_VPP_REG(reg_coef02_10,
			(coef->matrix_coef[0][2] << 16) | coef->matrix_coef[1][0]);
		WRITE_VPP_REG(reg_coef11_12,
			(coef->matrix_coef[1][1] << 16) | coef->matrix_coef[1][2]);
		WRITE_VPP_REG(reg_coef20_21,
			(coef->matrix_coef[2][0] << 16) | coef->matrix_coef[2][1]);
		WRITE_VPP_REG(reg_coef22, coef->matrix_coef[2][2]);
		WRITE_VPP_REG(reg_offset0_1,
			(coef->post_offset[0] << 16) | coef->post_offset[1]);
		WRITE_VPP_REG(reg_offset2, coef->post_offset[2]);
		WRITE_VPP_REG_BITS(reg_en_ctl, coef->en, 0, 1);
		break;
	case WR_DMA:
		VSYNC_WRITE_VPP_REG_EX(reg_pre_offset0_1,
			(coef->pre_offset[0] << 16) | coef->pre_offset[1], 0);
		VSYNC_WRITE_VPP_REG_EX(reg_pre_offset2, coef->pre_offset[2],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_coef00_01,
			(coef->matrix_coef[0][0] << 16) | coef->matrix_coef[0][1],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_coef02_10,
			(coef->matrix_coef[0][2] << 16) | coef->matrix_coef[1][0],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_coef11_12,
			(coef->matrix_coef[1][1] << 16) | coef->matrix_coef[1][2],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_coef20_21,
			(coef->matrix_coef[2][0] << 16) | coef->matrix_coef[2][1],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_coef22, coef->matrix_coef[2][2],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_offset0_1,
			(coef->post_offset[0] << 16) | coef->post_offset[1],
			0);
		VSYNC_WRITE_VPP_REG_EX(reg_offset2, coef->post_offset[2],
			0);
		VSYNC_WRITE_VPP_REG_BITS_EX(reg_en_ctl, coef->en, 0, 1,
			0);
		break;
	default:
		return;
	}
}

void mtx_setting_v2(enum vpp_matrix_e mtx_sel,
	enum wr_md_e mode,
	enum mtx_csc_e mtx_csc,
	int mtx_on,
	enum vpp_slice_e slice, int vpp_index)
{
	struct matrix_coef_s coef;

	switch (mtx_csc) {
	case MATRIX_RGB_YUV709:
		coef.matrix_coef[0][0] = 0xbb;
		coef.matrix_coef[0][1] = 0x275;
		coef.matrix_coef[0][2] = 0x3f;
		coef.matrix_coef[1][0] = 0x1f99;
		coef.matrix_coef[1][1] = 0x1ea6;
		coef.matrix_coef[1][2] = 0x1c2;
		coef.matrix_coef[2][0] = 0x1c2;
		coef.matrix_coef[2][1] = 0x1e67;
		coef.matrix_coef[2][2] = 0x1fd7;

		coef.pre_offset[0] = 0;
		coef.pre_offset[1] = 0;
		coef.pre_offset[2] = 0;
		coef.post_offset[0] = 0x40;
		coef.post_offset[1] = 0x200;
		coef.post_offset[2] = 0x200;
		coef.en = mtx_on;
		break;
	case MATRIX_YUV709_RGB:
		coef.matrix_coef[0][0] = 0x4ac;
		coef.matrix_coef[0][1] = 0x0;
		coef.matrix_coef[0][2] = 0x731;
		coef.matrix_coef[1][0] = 0x4ac;
		coef.matrix_coef[1][1] = 0x1f25;
		coef.matrix_coef[1][2] = 0x1ddd;
		coef.matrix_coef[2][0] = 0x4ac;
		coef.matrix_coef[2][1] = 0x879;
		coef.matrix_coef[2][2] = 0x0;

		coef.pre_offset[0] = 0x7c0;
		coef.pre_offset[1] = 0x600;
		coef.pre_offset[2] = 0x600;
		coef.post_offset[0] = 0x0;
		coef.post_offset[1] = 0x0;
		coef.post_offset[2] = 0x0;
		coef.en = mtx_on;
		break;
	case MATRIX_YUV709F_RGB:/*full to full*/
		coef.matrix_coef[0][0] = 0x400;
		coef.matrix_coef[0][1] = 0x0;
		coef.matrix_coef[0][2] = 0x64D;
		coef.matrix_coef[1][0] = 0x400;
		coef.matrix_coef[1][1] = 0x1F41;
		coef.matrix_coef[1][2] = 0x1E21;
		coef.matrix_coef[2][0] = 0x400;
		coef.matrix_coef[2][1] = 0x76D;
		coef.matrix_coef[2][2] = 0x0;

		coef.pre_offset[0] = 0x0;
		coef.pre_offset[1] = 0x600;
		coef.pre_offset[2] = 0x600;
		coef.post_offset[0] = 0x0;
		coef.post_offset[1] = 0x0;
		coef.post_offset[2] = 0x0;
		coef.en = mtx_on;
		break;
	case MATRIX_NULL:
		coef.matrix_coef[0][0] = 0;
		coef.matrix_coef[0][1] = 0;
		coef.matrix_coef[0][2] = 0;
		coef.matrix_coef[1][0] = 0;
		coef.matrix_coef[1][1] = 0;
		coef.matrix_coef[1][2] = 0;
		coef.matrix_coef[2][0] = 0;
		coef.matrix_coef[2][1] = 0;
		coef.matrix_coef[2][2] = 0;

		coef.pre_offset[0] = 0;
		coef.pre_offset[1] = 0;
		coef.pre_offset[2] = 0;
		coef.post_offset[0] = 0;
		coef.post_offset[1] = 0;
		coef.post_offset[2] = 0;
		coef.en = mtx_on;
		break;
	default:
		return;
	}

	vpp_mtx_config_v2(&coef, mode, slice, mtx_sel, vpp_index);
}

int ve_mtx_mul_mtx(int (*mtx_a)[3], int (*mtx_b)[3], int (*mtx_out)[3])
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			mtx_out[i][j] = 0;
			for (k = 0; k < 3; k++)
				mtx_out[i][j] += mtx_a[i][k] * mtx_b[k][j];
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			mtx_out[i][j] = (mtx_out[i][j] + (1 << 9)) >> 10;
	}

	return 0;
}

int ve_mtx_multi(int mtx_ep[][4], int (*mtx_out)[3])
{
	int i, j;
	int mtx_rgb[3][3] = {0};
	int mtx_in[3][3] = {0};
	int mtx_rgbto709l[3][3] = {
		{187, 629, 63},
		{-103, -346, 450},
		{450, -409, -41},
	};
	int mtx_709ltorgb[3][3] = {
		{1192, 0, 1836},
		{1192, -218, -547},
		{1192, 2166, 0},
	};

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			pr_amve_v2("mtx_out[%d][%d] = %d\n",
			i, j, mtx_ep[i][j]);
	}

	/* because use only one matrix, so Tr/Tg/Tb can not be used,
	 * if add Tr/Tg/Tb in RGB out, need two matrix to workaround
	 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			mtx_in[i][j] = mtx_ep[j][i];

	ve_mtx_mul_mtx(mtx_in, mtx_709ltorgb, mtx_rgb);
	ve_mtx_mul_mtx(mtx_rgbto709l, mtx_rgb, mtx_out);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			pr_amve_v2("mtx_out[%d][%d] = 0x%x\n",
			i, j, mtx_out[i][j]);
	}
	return 0;
}

void ve_eye_proc(int mtx_ep[][4], int mtx_on, int vpp_index)
{
	int i;
	int slice_max = get_slice_max();
	unsigned int temp;
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_coef13_14 = 0;
	unsigned int matrix_coef23_24 = 0;
	unsigned int matrix_coef15_25 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;
	int mtx_out[3][3] = {0};
	int pre_offset[3] = {
		-64, -512, -512
	};

	int offset[3] = {
		64, 512, 512
	};

	if (vinfo_lcd_support() ||
		vinfo_hdmi_out_fmt())
		return;

	for (i = SLICE0; i < slice_max; i++) {
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01 +
			pst_reg_ofst[i];
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10 +
			pst_reg_ofst[i];
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12 +
			pst_reg_ofst[i];
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21 +
			pst_reg_ofst[i];
		matrix_coef22 = VPP_POST_MATRIX_COEF22 +
			pst_reg_ofst[i];
		matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14 +
			pst_reg_ofst[i];
		matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24 +
			pst_reg_ofst[i];
		matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25 +
			pst_reg_ofst[i];
		matrix_clip = VPP_POST_MATRIX_CLIP +
			pst_reg_ofst[i];
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1 +
			pst_reg_ofst[i];
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2 +
			pst_reg_ofst[i];
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1 +
			pst_reg_ofst[i];
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2 +
			pst_reg_ofst[i];
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL +
			pst_reg_ofst[i];

		temp = VSYNC_READ_VPP_REG(matrix_en_ctrl);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_en_ctrl,
			(temp & 0xfffffffe) | (mtx_on & 0x1), vpp_index);

		if (!mtx_on)
			continue;

		ve_mtx_multi(mtx_ep, mtx_out);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01,
			((mtx_out[0][0] & 0x1fff) << 16) |
			(mtx_out[0][1] & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10,
			((mtx_out[0][2] & 0x1fff) << 16) |
			(mtx_out[1][0] & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12,
			((mtx_out[1][1] & 0x1fff) << 16) |
			(mtx_out[1][2] & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21,
			((mtx_out[2][0] & 0x1fff) << 16) |
			(mtx_out[2][1] & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22,
			(mtx_out[2][2] & 0x1fff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1,
			((offset[0] & 0x7ff) << 16) |
			(offset[1] & 0x7ff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2,
			(offset[2] & 0x7ff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1,
			((pre_offset[0] & 0x7ff) << 16) |
			(pre_offset[1] & 0x7ff), vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2,
			(pre_offset[2] & 0x7ff), vpp_index);
	}
	eye_proc_en = mtx_on;
}

int get_eye_pro_en(void)
{
	return eye_proc_en;
}

void cm_top_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	unsigned int reg = VPP_VE_ENABLE_CTRL;

	if (chip_type_id == chip_t3x)
		reg = VPP_SLICE1_VE_ENABLE_CTRL + ve_addr_offset;

	switch (mode) {
	case WR_VCB:
		WRITE_VPP_REG_BITS(reg, en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL,
			en, 4, 1);
		if (chip_type_id == chip_s5) {
			WRITE_VPP_REG_BITS(VPP_SLICE2_VE_ENABLE_CTRL,
				en, 4, 1);
			WRITE_VPP_REG_BITS(VPP_SLICE3_VE_ENABLE_CTRL,
				en, 4, 1);
		}
		break;
	case WR_DMA:
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, en, 4, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE1_VE_ENABLE_CTRL,
			en, 4, 1, vpp_index);
		if (chip_type_id == chip_s5) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE2_VE_ENABLE_CTRL,
				en, 4, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE3_VE_ENABLE_CTRL,
				en, 4, 1, vpp_index);
		}
		break;
	default:
		break;
	}
}

struct cm_port_s get_cm_port(void)
{
	struct cm_port_s port;

	if (chip_type_id == chip_t3x) {
		port.cm_addr_port[0] = VPP_SLICE1_CHROMA_ADDR_PORT +
			ve_addr_offset;
		port.cm_data_port[0] = VPP_SLICE1_CHROMA_DATA_PORT +
			ve_addr_offset;
	} else {
		port.cm_addr_port[0] = VPP_CHROMA_ADDR_PORT;
		port.cm_data_port[0] = VPP_CHROMA_DATA_PORT;
	}

	port.cm_addr_port[1] = VPP_SLICE1_CHROMA_ADDR_PORT;
	port.cm_data_port[1] = VPP_SLICE1_CHROMA_DATA_PORT;
	port.cm_addr_port[2] = VPP_SLICE2_CHROMA_ADDR_PORT;
	port.cm_data_port[2] = VPP_SLICE2_CHROMA_DATA_PORT;
	port.cm_addr_port[3] = VPP_SLICE3_CHROMA_ADDR_PORT;
	port.cm_data_port[3] = VPP_SLICE3_CHROMA_DATA_PORT;

	return port;
}

void cm_hist_get(struct vpp_hist_param_s *vp, unsigned int hue_bin0, unsigned int sat_bin0)
{
	int i;
	struct cm_port_s port;
	unsigned int reg_addr_s0;
	unsigned int reg_addr_s1;
	unsigned int reg_data_s0;
	unsigned int reg_data_s1;
	unsigned int val_s0 = 0;
	unsigned int val_s1 = 0;
	int slice_case;

	if (!hue_bin0 || !sat_bin0)
		return;

	slice_case = ve_multi_slice_case_get();

	port = get_cm_port();
	reg_addr_s0 = port.cm_addr_port[0];
	reg_addr_s1 = port.cm_addr_port[1];
	reg_data_s0 = port.cm_data_port[0];
	reg_data_s1 = port.cm_data_port[1];

	for (i = 0; i < 32; i++) {
		WRITE_VPP_REG(reg_addr_s0, hue_bin0 + i);
		val_s0 = READ_VPP_REG(reg_data_s0);

		if (chip_type_id == chip_t3x && slice_case) {
			WRITE_VPP_REG(reg_addr_s1, hue_bin0 + i);
			val_s1 = READ_VPP_REG(reg_data_s1);
		}

		vp->vpp_hue_gamma[i] = val_s0 + val_s1;
	}

	val_s0 = 0;
	val_s1 = 0;

	for (i = 0; i < 32; i++) {
		WRITE_VPP_REG(reg_addr_s0, sat_bin0 + i);
		val_s0 = READ_VPP_REG(reg_data_s0);

		if (chip_type_id == chip_t3x && slice_case) {
			WRITE_VPP_REG(reg_addr_s1, sat_bin0 + i);
			val_s1 = READ_VPP_REG(reg_data_s1);
		}

		vp->vpp_sat_gamma[i] = val_s0 + val_s1;
	}
}

void cm_hist_by_type_get(enum cm_hist_e hist_sel,
	unsigned int *data, unsigned int length,
	unsigned int addr_bin0)
{
	int i;
	struct cm_port_s port;
	unsigned int reg_addr_s0;
	unsigned int reg_addr_s1;
	unsigned int reg_data_s0;
	unsigned int reg_data_s1;
	unsigned int val_s0 = 0;
	unsigned int val_s1 = 0;
	int slice_case;

	if (!data || !length || !addr_bin0)
		return;

	slice_case = ve_multi_slice_case_get();

	port = get_cm_port();
	reg_addr_s0 = port.cm_addr_port[0];
	reg_addr_s1 = port.cm_addr_port[1];
	reg_data_s0 = port.cm_data_port[0];
	reg_data_s1 = port.cm_data_port[1];

	for (i = 0; i < length; i++) {
		WRITE_VPP_REG(reg_addr_s0, addr_bin0 + i);
		val_s0 = READ_VPP_REG(reg_data_s0);

		if (chip_type_id == chip_t3x && slice_case) {
			WRITE_VPP_REG(reg_addr_s1, addr_bin0 + i);
			val_s1 = READ_VPP_REG(reg_data_s1);
		}

		data[i] = val_s0 + val_s1;
	}
}

/*modules after post matrix*/
void post_gainoff_cfg(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode, enum vpp_slice_e slice,
	int vpp_index)
{
	unsigned int reg_ctl0;
	unsigned int reg_ctl1;
	unsigned int reg_ctl2;
	unsigned int reg_ctl3;
	unsigned int reg_ctl4;
	int t;

	reg_ctl0 = VPP_GAINOFF_CTRL0 + pst_reg_ofst[slice];
	reg_ctl1 = VPP_GAINOFF_CTRL1 + pst_reg_ofst[slice];
	reg_ctl2 = VPP_GAINOFF_CTRL2 + pst_reg_ofst[slice];
	reg_ctl3 = VPP_GAINOFF_CTRL3 + pst_reg_ofst[slice];
	reg_ctl4 = VPP_GAINOFF_CTRL4 + pst_reg_ofst[slice];

	if (chip_type_id == chip_t3x)
		t = 0;
	else
		t = 1;

	if (mode == WR_VCB) {
		WRITE_VPP_REG(reg_ctl0,
			((p->en << 31) & 0x80000000) |
			((t << 30) & 0x40000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain <<  0) & 0x000007ff));
		WRITE_VPP_REG(reg_ctl1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl3,
			((p->r_pre_offset  << 16) & 0x1fff0000) |
			((p->g_pre_offset  <<  0) & 0x00001fff));
		WRITE_VPP_REG(reg_ctl4,
			((p->b_pre_offset  <<  0) & 0x00001fff));
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctl0,
			((p->en << 31) & 0x80000000) |
			((t << 30) & 0x40000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain <<	0) & 0x000007ff),
			vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctl1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset <<  0) & 0x00001fff),
			vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctl2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset <<  0) & 0x00001fff),
			vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctl3,
			((p->r_pre_offset  << 16) & 0x1fff0000) |
			((p->g_pre_offset  <<  0) & 0x00001fff),
			vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctl4,
			((p->b_pre_offset  <<  0) & 0x00001fff),
			vpp_index);
	}

	pr_amve_v2("go_en: %d, slice = %d\n",
		p->en, slice);
	pr_amve_v2("go_gain: %d, %d, %d\n",
		p->r_gain, p->g_gain, p->b_gain);
	pr_amve_v2("go_pre_offset: %d, %d, %d\n",
		p->r_pre_offset, p->g_pre_offset, p->b_pre_offset);
	pr_amve_v2("go_post_offset: %d, %d, %d\n",
		p->r_post_offset, p->g_post_offset, p->b_post_offset);
}

void post_gainoff_set(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode,
	int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		post_gainoff_cfg(p, mode, i, vpp_index);
}

void post_pre_gamma_set(int *lut)
{
	int i, j;
	unsigned int ctl_port;
	unsigned int addr_port;
	unsigned int data_port;
	int slice_max;

	slice_max = get_slice_max();

	for (j = SLICE0; j < slice_max; j++) {
		ctl_port = VPP_GAMMA_CTRL + pst_reg_ofst[j];
		addr_port = VPP_GAMMA_BIN_ADDR + pst_reg_ofst[j];
		data_port = VPP_GAMMA_BIN_DATA + pst_reg_ofst[j];
		WRITE_VPP_REG(addr_port, 0);
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(data_port,
				      (((lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				      ((lut[i * 2] << 2) & 0xffff)));
		WRITE_VPP_REG_BITS(ctl_port, 0x3, 0, 2);
	}
}

/*vpp module enable/disable control*/
void ve_vadj_ctl(enum wr_md_e mode, enum vadj_index_e vadj_idx, int en,
	int vpp_index)
{
	int i;
	int slice_max;
	unsigned int reg = VPP_VADJ1_MISC;

	if (chip_type_id == chip_t3x)
		reg = VPP_SLICE1_VADJ1_MISC + ve_addr_offset;

	slice_max = get_slice_max();

	switch (mode) {
	case WR_VCB:
		if (vadj_idx == VE_VADJ1) {
			WRITE_VPP_REG_BITS(reg, en, 0, 1);
			for (i = SLICE1; i < slice_max; i++)
				WRITE_VPP_REG_BITS(VPP_SLICE1_VADJ1_MISC + ve_reg_ofst[i - 1],
					en, 0, 1);
		} else if (vadj_idx == VE_VADJ2) {
			for (i = SLICE0; i < slice_max; i++)
				WRITE_VPP_REG_BITS(VPP_VADJ2_MISC + pst_reg_ofst[i],
					en, 0, 1);
		}
		break;
	case WR_DMA:
		if (vadj_idx == VE_VADJ1) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, en, 0, 1, vpp_index);
			for (i = SLICE1; i < slice_max; i++)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE1_VADJ1_MISC +
					ve_reg_ofst[i - 1], en, 0, 1, vpp_index);
		} else if (vadj_idx == VE_VADJ2) {
			for (i = SLICE0; i < slice_max; i++)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ2_MISC +
					pst_reg_ofst[i], en, 0, 1, vpp_index);
		}
		break;
	default:
		break;
	}

	pr_amve_v2("vadj_ctl: en = %d, vadj_idx = %d\n", en, vadj_idx);
}

/*blue stretch can only use on slice0 on s5*/
void ve_bs_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;
	unsigned int reg_bs_0 = VPP_BLUE_STRETCH_1;
	unsigned int reg_bs_0_slice1 = VPP_SLICE1_BLUE_STRETCH_1;

	if (chip_type_id == chip_t3x) {
		reg_bs_0_slice1 = 0x2878;
		reg_bs_0 = reg_bs_0_slice1 + ve_addr_offset;
	}

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(reg_bs_0, en, 31, 1);
		if (chip_type_id == chip_t3x) {
			for (i = SLICE1; i < slice_max; i++)
				WRITE_VPP_REG_BITS(reg_bs_0_slice1 +
					ve_reg_ofst[i - 1], en, 31, 1);
		}
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg_bs_0, en, 31, 1,
			vpp_index);
		if (chip_type_id == chip_t3x) {
			for (i = SLICE1; i < slice_max; i++)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg_bs_0_slice1 +
					ve_reg_ofst[i - 1], en, 31, 1, vpp_index);
		}
	}

	pr_amve_v2("bs_ctl: en = %d\n", en);
}

void ve_ble_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;
	unsigned int reg = VPP_VE_ENABLE_CTRL;

	if (chip_type_id == chip_t3x)
		reg = VPP_SLICE1_VE_ENABLE_CTRL + ve_addr_offset;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(reg, en, 3, 1);
		for (i = SLICE1; i < slice_max; i++)
			WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 3, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, en, 3, 1, vpp_index);
		for (i = SLICE1; i < slice_max; i++)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 3, 1, vpp_index);
	}

	pr_amve_v2("ble_ctl: en = %d\n", en);
}

void ve_cc_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;
	unsigned int reg = VPP_VE_ENABLE_CTRL;

	if (chip_type_id == chip_t3x)
		reg = VPP_SLICE1_VE_ENABLE_CTRL + ve_addr_offset;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(reg, en, 1, 1);
		for (i = SLICE1; i < slice_max; i++)
			WRITE_VPP_REG_BITS(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 1, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, en, 1, 1, vpp_index);
		for (i = SLICE1; i < slice_max; i++)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SLICE1_VE_ENABLE_CTRL +
				ve_reg_ofst[i - 1], en, 1, 1, vpp_index);
	}

	pr_amve_v2("cc_ctl: en = %d\n", en);
}

void post_wb_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		for (i = SLICE0; i < slice_max; i++)
			WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0 +
				pst_reg_ofst[i], en, 31, 1);
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < slice_max; i++)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_GAINOFF_CTRL0 +
				pst_reg_ofst[i], en, 31, 1, vpp_index);
	}

	pr_amve_v2("wb_ctl: en = %d\n", en);
}

void post_pre_gamma_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		for (i = SLICE0; i < slice_max; i++)
			WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL +
				pst_reg_ofst[i], en, 0, 1);
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < slice_max; i++)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_GAMMA_CTRL +
				pst_reg_ofst[i], en, 0, 1, vpp_index);
	}

	pr_amve_v2("pre_gamma_ctl: en = %d\n", en);
}

int post_pre_gamma_get(enum vpp_slice_e slice, int start, int len)
{
	return READ_VPP_REG_BITS(VPP_GAMMA_CTRL +
				pst_reg_ofst[slice], start, len);
}

void vpp_luma_hist_en(int slice_case,
	enum wr_md_e mode, int vpp_index)
{
	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 0, 1);

		if (slice_case)
			WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 0, 0, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VI_HIST_CTRL,
			1, 0, 1, vpp_index);

		if (slice_case)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VI_HIST_CTRL + 0x30,
				1, 0, 1, vpp_index);
		else
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VI_HIST_CTRL + 0x30,
				0, 0, 1, vpp_index);
	}

	vi_hist_en = 1;
}

void vpp_luma_hist_init(void)
{
	if (chip_type_id == chip_t3x) {
		vd_info = get_vd_proc_amvecm_info();

		/*WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 1, 1);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 2, 1);
		/*0:vpp_dout/post_vd1(2ppc) 1:vd1_slice0_din*/
		/*2:vd1_slice1_din 3:vd1_slice2_din 4:vd1_slice_din*/
		/*5:vd2_din 6:osd1_din 7:osd2_din*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 11, 3);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 3, 18, 2);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 24, 8);
		WRITE_VPP_REG(VI_HIST_H_START_END, 0x2d0);
		WRITE_VPP_REG(VI_HIST_V_START_END, 0x1e0);
		WRITE_VPP_REG(VI_HIST_PIC_SIZE,
			0x2d0 | (0x1e0 << 16));
		/*7:5 hist_dnlp_low the real pixels in each bins got*/
		/*by VI_DNLP_HISTXX should multiple with 2^(dnlp_low+3)*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, 5, 3);
		/*enable*/
		/*WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 0, 1);*/

		/*slice1*/
		/*WRITE_VPP_REG(VI_HIST_GCLK_CTRL + 0x30, 0xffffffff);*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 1, 1, 1);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 1, 2, 1);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 2, 11, 3);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 3, 18, 2);
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 1, 24, 8);
		WRITE_VPP_REG(VI_HIST_H_START_END + 0x30, 0x2d0);
		WRITE_VPP_REG(VI_HIST_V_START_END + 0x30, 0x1e0);
		WRITE_VPP_REG(VI_HIST_PIC_SIZE + 0x30,
			0x2d0 | (0x1e0 << 16));
		WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 2, 5, 3);
		/*WRITE_VPP_REG_BITS(VI_HIST_CTRL + 0x30, 1, 0, 1);*/
	} else {
		/*13:11 hist_din_sel, 0: from vdin0 dout, 1: vdin1, 2: nr dout,*/
		/*3: di output, 4: vpp output, 5: vd1_din, 6: vd2_din, 7:osd1_dout*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 11, 3);
		/*3:2 hist_din_sel the source used for hist statistics.*/
		/*00: from matrix0 dout, 01: vsc_dout, 10: matrix1 dout, 11: matrix1 din*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0, 2, 1);
		/*0:full picture, 1:window*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0, 1, 1);

		/*7:5 hist_dnlp_low the real pixels in each bins got*/
		/*by VI_DNLP_HISTXX should multiple with 2^(dnlp_low+3)*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, 5, 3);
		/*enable*/
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 1, 0, 1);
	}
}

void get_luma_hist(struct vframe_s *vf,
	struct vpp_hist_param_s *vp, int vpp_index)
{
	static int pre_w, pre_h;
	int width, height;
	int i;
	int overlap = 64;
	int slice_case;
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;
	unsigned int src_compheight = 0;
	unsigned int src_compwidth = 0;

	if (!vf)
		return;

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	if (h_rate) {
		src_h = vf->height / (h_rate + 1);
		src_compheight = vf->compHeight / (h_rate + 1);
	} else {
		src_h = vf->height;
		src_compheight = vf->compHeight;
	}

	if (w_rate) {
		src_w = vf->width / (w_rate + 1);
		src_compwidth = vf->compWidth / (w_rate + 1);
	} else {
		src_w = vf->width;
		src_compwidth = vf->compWidth;
	}

	slice_case = ve_multi_slice_case_get();

	if (chip_type_id == chip_t3x &&
		(!vi_hist_en || dnlp_slice_num_changed)) {
		vpp_luma_hist_en(slice_case, WR_DMA, vpp_index);
		dnlp_slice_num_changed = 0;
		pr_amve_v2("%s: dnlp_slice_num_changed 1->0\n",
			__func__);
	}

	if (vd_info) {
		if (slice_case)
			overlap = vd_info->slice[0].vd1_overlap;
		else
			overlap = 0;

		width = vd_info->slice[0].vd1_slice_in_hsize - overlap;
		height = vd_info->slice[0].vd1_slice_in_vsize;
	} else {
		width = (vf->type & VIDTYPE_COMPRESS) ?
			src_compwidth : src_w;
		height =  (vf->type & VIDTYPE_COMPRESS) ?
			src_compheight : src_h;

		if (slice_case)
			width = width >> 1;
	}

	if (pre_w != width || pre_h != height) {
		WRITE_VPP_REG(VI_HIST_PIC_SIZE,
			width | (height << 16));

		WRITE_VPP_REG(VI_HIST_H_START_END, width);
		WRITE_VPP_REG(VI_HIST_V_START_END, height);

		if (chip_type_id == chip_t3x &&
			slice_case) {
			WRITE_VPP_REG(VI_HIST_PIC_SIZE + 0x30,
				width | (height << 16));

			WRITE_VPP_REG(VI_HIST_H_START_END + 0x30, width);
			WRITE_VPP_REG(VI_HIST_V_START_END + 0x30, height);
		}

		pre_w = width;
		pre_h = height;
	}

	vp->vpp_luma_sum = READ_VPP_REG(VI_HIST_SPL_VAL);
	vp->vpp_pixel_sum = READ_VPP_REG(VI_HIST_SPL_PIX_CNT);
	vp->vpp_chroma_sum = READ_VPP_REG(VI_HIST_CHROMA_SUM);
	vp->vpp_height =
		READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			VI_HIST_PIC_HEIGHT_BIT, VI_HIST_PIC_HEIGHT_WID);
	vp->vpp_width =
		READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			VI_HIST_PIC_WIDTH_BIT, VI_HIST_PIC_WIDTH_WID);
	vp->vpp_luma_max =
		READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			VI_HIST_MAX_BIT, VI_HIST_MAX_WID);
	vp->vpp_luma_min =
		READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			VI_HIST_MIN_BIT, VI_HIST_MIN_WID);

	if (chip_type_id == chip_t3x) {
		if (hist_dma_case) {
			if (slice_case) {
				am_dma_get_blend_vi_hist(vp->vpp_gamma,
					64);
				am_dma_get_blend_vi_hist_low(vp->vpp_dark_hist,
					64);
			} else {
				am_dma_get_mif_data_vi_hist(0,
					vp->vpp_gamma, 64);
				am_dma_get_mif_data_vi_hist_low(0,
					vp->vpp_dark_hist, 64);
			}

			return;
		}

		WRITE_VPP_REG(VI_RO_HIST_LOW_IDX, 1 << 6);
		for (i = 0; i < 64; i++)
			vp->vpp_dark_hist[i] = READ_VPP_REG(VI_RO_HIST_LOW);
	}

	vp->vpp_gamma[0]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
	vp->vpp_gamma[1]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
	vp->vpp_gamma[2]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
	vp->vpp_gamma[3]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
	vp->vpp_gamma[4]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
	vp->vpp_gamma[5]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	vp->vpp_gamma[6]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_06_BIT, VI_HIST_ON_BIN_06_WID);
	vp->vpp_gamma[7]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_07_BIT, VI_HIST_ON_BIN_07_WID);
	vp->vpp_gamma[8]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_08_BIT, VI_HIST_ON_BIN_08_WID);
	vp->vpp_gamma[9]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_09_BIT, VI_HIST_ON_BIN_09_WID);
	vp->vpp_gamma[10]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_10_BIT, VI_HIST_ON_BIN_10_WID);
	vp->vpp_gamma[11]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_11_BIT, VI_HIST_ON_BIN_11_WID);
	vp->vpp_gamma[12]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_12_BIT, VI_HIST_ON_BIN_12_WID);
	vp->vpp_gamma[13]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_13_BIT, VI_HIST_ON_BIN_13_WID);
	vp->vpp_gamma[14]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_14_BIT, VI_HIST_ON_BIN_14_WID);
	vp->vpp_gamma[15]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_15_BIT, VI_HIST_ON_BIN_15_WID);
	vp->vpp_gamma[16]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_16_BIT, VI_HIST_ON_BIN_16_WID);
	vp->vpp_gamma[17]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_17_BIT, VI_HIST_ON_BIN_17_WID);
	vp->vpp_gamma[18]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_18_BIT, VI_HIST_ON_BIN_18_WID);
	vp->vpp_gamma[19]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_19_BIT, VI_HIST_ON_BIN_19_WID);
	vp->vpp_gamma[20]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_20_BIT, VI_HIST_ON_BIN_20_WID);
	vp->vpp_gamma[21]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_21_BIT, VI_HIST_ON_BIN_21_WID);
	vp->vpp_gamma[22]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_22_BIT, VI_HIST_ON_BIN_22_WID);
	vp->vpp_gamma[23]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_23_BIT, VI_HIST_ON_BIN_23_WID);
	vp->vpp_gamma[24]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_24_BIT, VI_HIST_ON_BIN_24_WID);
	vp->vpp_gamma[25]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_25_BIT, VI_HIST_ON_BIN_25_WID);
	vp->vpp_gamma[26]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_26_BIT, VI_HIST_ON_BIN_26_WID);
	vp->vpp_gamma[27]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_27_BIT, VI_HIST_ON_BIN_27_WID);
	vp->vpp_gamma[28]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_28_BIT, VI_HIST_ON_BIN_28_WID);
	vp->vpp_gamma[29]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_29_BIT, VI_HIST_ON_BIN_29_WID);
	vp->vpp_gamma[30]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_30_BIT, VI_HIST_ON_BIN_30_WID);
	vp->vpp_gamma[31]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_31_BIT, VI_HIST_ON_BIN_31_WID);
	vp->vpp_gamma[32]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_32_BIT, VI_HIST_ON_BIN_32_WID);
	vp->vpp_gamma[33]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_33_BIT, VI_HIST_ON_BIN_33_WID);
	vp->vpp_gamma[34]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_34_BIT, VI_HIST_ON_BIN_34_WID);
	vp->vpp_gamma[35]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_35_BIT, VI_HIST_ON_BIN_35_WID);
	vp->vpp_gamma[36]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_36_BIT, VI_HIST_ON_BIN_36_WID);
	vp->vpp_gamma[37]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_37_BIT, VI_HIST_ON_BIN_37_WID);
	vp->vpp_gamma[38]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_38_BIT, VI_HIST_ON_BIN_38_WID);
	vp->vpp_gamma[39]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_39_BIT, VI_HIST_ON_BIN_39_WID);
	vp->vpp_gamma[40]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_40_BIT, VI_HIST_ON_BIN_40_WID);
	vp->vpp_gamma[41]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_41_BIT, VI_HIST_ON_BIN_41_WID);
	vp->vpp_gamma[42]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_42_BIT, VI_HIST_ON_BIN_42_WID);
	vp->vpp_gamma[43]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_43_BIT, VI_HIST_ON_BIN_43_WID);
	vp->vpp_gamma[44]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_44_BIT, VI_HIST_ON_BIN_44_WID);
	vp->vpp_gamma[45]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_45_BIT, VI_HIST_ON_BIN_45_WID);
	vp->vpp_gamma[46]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_46_BIT, VI_HIST_ON_BIN_46_WID);
	vp->vpp_gamma[47]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_47_BIT, VI_HIST_ON_BIN_47_WID);
	vp->vpp_gamma[48]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_48_BIT, VI_HIST_ON_BIN_48_WID);
	vp->vpp_gamma[49]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_49_BIT, VI_HIST_ON_BIN_49_WID);
	vp->vpp_gamma[50]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_50_BIT, VI_HIST_ON_BIN_50_WID);
	vp->vpp_gamma[51]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_51_BIT, VI_HIST_ON_BIN_51_WID);
	vp->vpp_gamma[52]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_52_BIT, VI_HIST_ON_BIN_52_WID);
	vp->vpp_gamma[53]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_53_BIT, VI_HIST_ON_BIN_53_WID);
	vp->vpp_gamma[54]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_54_BIT, VI_HIST_ON_BIN_54_WID);
	vp->vpp_gamma[55]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_55_BIT, VI_HIST_ON_BIN_55_WID);
	vp->vpp_gamma[56]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_56_BIT, VI_HIST_ON_BIN_56_WID);
	vp->vpp_gamma[57]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_57_BIT, VI_HIST_ON_BIN_57_WID);
	vp->vpp_gamma[58]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_58_BIT, VI_HIST_ON_BIN_58_WID);
	vp->vpp_gamma[59]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_59_BIT, VI_HIST_ON_BIN_59_WID);
	vp->vpp_gamma[60]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_60_BIT, VI_HIST_ON_BIN_60_WID);
	vp->vpp_gamma[61]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_61_BIT, VI_HIST_ON_BIN_61_WID);
	vp->vpp_gamma[62]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_62_BIT, VI_HIST_ON_BIN_62_WID);
	vp->vpp_gamma[63]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_63_BIT, VI_HIST_ON_BIN_63_WID);
}

void ve_multi_picture_case_set(int enable)
{
	multi_picture_case = enable;
	pr_amve_v2("%s: multi_picture_case = %d", __func__, enable);
}

int ve_multi_picture_case_get(void)
{
	return multi_picture_case;
}

int ve_multi_slice_case_get(void)
{
	int slice_case;

	vd_info = get_vd_proc_amvecm_info();
	if (vd_info && vd_info->slice_num > 1)
		slice_case = 1;
	else
		slice_case = 0;

	if (slice_case != multi_slice_case) {
		multi_slice_case = slice_case;
		dnlp_slice_num_changed = 1;
		lc_slice_num_changed = 1;
	}

	return slice_case;
}

void ve_vadj_misc_set(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int start, int len,
	int vpp_index)
{
	int reg;
	int slice_max;

	slice_max = get_slice_max();
	if (slice >= slice_max)
		return;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			if (chip_type_id == chip_t3x)
				reg = VPP_SLICE1_VADJ1_MISC +
					ve_addr_offset;
			else
				reg = VPP_VADJ1_MISC;
		} else {
			reg = VPP_SLICE1_VADJ1_MISC + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_MISC + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	if (mode == WR_VCB)
		WRITE_VPP_REG_BITS(reg, val, start, len);
	else if (mode == WR_DMA)
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg, val, start, len, vpp_index);
}

int ve_vadj_misc_get(enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int start, int len)
{
	int reg;
	int slice_max;

	slice_max = get_slice_max();
	if (slice >= slice_max)
		return 0;

	switch (vadj_idx) {
	case VE_VADJ1:
		if (slice == SLICE0) {
			if (chip_type_id == chip_t3x)
				reg = VPP_SLICE1_VADJ1_MISC +
					ve_addr_offset;
			else
				reg = VPP_VADJ1_MISC;
		} else {
			reg = VPP_SLICE1_VADJ1_MISC + ve_reg_ofst[slice - 1];
		}
		break;
	case VE_VADJ2:
		reg = VPP_VADJ2_MISC + pst_reg_ofst[slice];
		break;
	default:
		break;
	}

	return READ_VPP_REG_BITS(reg, start, len);
}

void ve_mtrx_setting(enum vpp_matrix_e mtx_sel,
	int mtx_csc, int mtx_on, enum vpp_slice_e slice)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_coef13_14 = 0;
	unsigned int matrix_coef23_24 = 0;
	unsigned int matrix_coef15_25 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;
	int vpp_sel = VPP_TOP0;
	int offset = 0x100;

	if (mtx_sel == VD1_MTX) {
		matrix_coef00_01 = VPP_SLICE1_VD1_MATRIX_COEF00_01 +
			ve_addr_offset;
		matrix_coef02_10 = VPP_SLICE1_VD1_MATRIX_COEF02_10 +
			ve_addr_offset;
		matrix_coef11_12 = VPP_SLICE1_VD1_MATRIX_COEF11_12 +
			ve_addr_offset;
		matrix_coef20_21 = VPP_SLICE1_VD1_MATRIX_COEF20_21 +
			ve_addr_offset;
		matrix_coef22 = VPP_SLICE1_VD1_MATRIX_COEF22 +
			ve_addr_offset;
		matrix_coef13_14 = VPP_SLICE1_VD1_MATRIX_COEF13_14 +
			ve_addr_offset;
		matrix_coef23_24 = VPP_SLICE1_VD1_MATRIX_COEF23_24 +
			ve_addr_offset;
		matrix_coef15_25 = VPP_SLICE1_VD1_MATRIX_COEF15_25 +
			ve_addr_offset;
		matrix_clip = VPP_SLICE1_VD1_MATRIX_CLIP +
			ve_addr_offset;
		matrix_offset0_1 = VPP_SLICE1_VD1_MATRIX_OFFSET0_1 +
			ve_addr_offset;
		matrix_offset2 = VPP_SLICE1_VD1_MATRIX_OFFSET2 +
			ve_addr_offset;
		matrix_pre_offset0_1 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET0_1 +
			ve_addr_offset;
		matrix_pre_offset2 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET2 +
			ve_addr_offset;
		matrix_en_ctrl = VPP_SLICE1_VD1_MATRIX_EN_CTRL +
			ve_addr_offset;
	} else if (mtx_sel == POST2_MTX) {
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST2_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST2_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST2_MATRIX_COEF15_25;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
	} else if (mtx_sel == POST_MTX) {
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
	} else {
		return;
	}

	if (slice > SLICE0) {
		if (mtx_sel == VD1_MTX) {
			matrix_coef00_01 = VPP_SLICE1_VD1_MATRIX_COEF00_01;
			matrix_coef02_10 = VPP_SLICE1_VD1_MATRIX_COEF02_10;
			matrix_coef11_12 = VPP_SLICE1_VD1_MATRIX_COEF11_12;
			matrix_coef20_21 = VPP_SLICE1_VD1_MATRIX_COEF20_21;
			matrix_coef22 = VPP_SLICE1_VD1_MATRIX_COEF22;
			matrix_coef13_14 = VPP_SLICE1_VD1_MATRIX_COEF13_14;
			matrix_coef23_24 = VPP_SLICE1_VD1_MATRIX_COEF23_24;
			matrix_coef15_25 = VPP_SLICE1_VD1_MATRIX_COEF15_25;
			matrix_clip = VPP_SLICE1_VD1_MATRIX_CLIP;
			matrix_offset0_1 = VPP_SLICE1_VD1_MATRIX_OFFSET0_1;
			matrix_offset2 = VPP_SLICE1_VD1_MATRIX_OFFSET2;
			matrix_pre_offset0_1 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET0_1;
			matrix_pre_offset2 = VPP_SLICE1_VD1_MATRIX_PRE_OFFSET2;
			matrix_en_ctrl = VPP_SLICE1_VD1_MATRIX_EN_CTRL;
		} else {
			matrix_coef00_01 += offset;
			matrix_coef02_10 += offset;
			matrix_coef11_12 += offset;
			matrix_coef20_21 += offset;
			matrix_coef22 += offset;
			matrix_coef13_14 += offset;
			matrix_coef23_24 += offset;
			matrix_coef15_25 += offset;
			matrix_clip += offset;
			matrix_offset0_1 += offset;
			matrix_offset2 += offset;
			matrix_pre_offset0_1 += offset;
			matrix_pre_offset2 += offset;
			matrix_en_ctrl += offset;
		}
	}

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_en_ctrl, mtx_on, 0, 1, vpp_sel);

	if (!mtx_on)
		return;

	switch (mtx_csc) {
	case MATRIX_RGB_YUV709:
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01, 0x00bb0275, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10, 0x003f1f99, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12, 0x1ea601c2, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21, 0x01c21e67, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, 0x00001fd7, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, 0x00400200, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, 0x00000200, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, 0x0, vpp_sel);
		break;
	case MATRIX_YUV709_RGB:
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01, 0x04ac0000, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10, 0x073104ac, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12, 0x1f251ddd, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21, 0x04ac0879, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, 0x7c00600, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, 0x00000600, vpp_sel);
		break;
	case MATRIX_YUV709F_RGB:/*full to full*/
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01, 0x04000000, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10, 0x064D0400, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12, 0x1F411E21, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21, 0x0400076D, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, 0x0, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, 0x0000600, vpp_sel);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, 0x00000600, vpp_sel);
		break;
	default:
		break;
	}

	/*pr_info("mtx_sel:%d, mtx_csc:0x%x\n", mtx_sel, mtx_csc);*/
}

void ve_sharpness_gain_set(int sr0_gain, int sr1_gain,
	enum wr_md_e mode, int vpp_index)
{
	int i;
	int slice_max;
	int sr0_reg = VPP_SRSHARP0_PK_FINALGAIN_HP_BP;
	int sr1_reg = VPP_SRSHARP1_PK_FINALGAIN_HP_BP;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		for (i = SLICE0; i < slice_max; i++) {
			WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[i],
				sr0_gain, 0, 16);
			WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[i],
				sr1_gain, 0, 16);

			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr0_reg + sr_sharp_reg_ofst[i], sr0_gain);
			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr1_reg + sr_sharp_reg_ofst[i], sr1_gain);
		}
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < slice_max; i++) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(sr0_reg + sr_sharp_reg_ofst[i],
				sr0_gain, 0, 16, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(sr1_reg + sr_sharp_reg_ofst[i],
				sr1_gain, 0, 16, vpp_index);

			pr_amve_v2("%s: vpp_index = %d\n", __func__, vpp_index);
			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr0_reg + sr_sharp_reg_ofst[i], sr0_gain);
			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr1_reg + sr_sharp_reg_ofst[i], sr1_gain);
		}
	}
}

void ve_sharpness_ctl(enum wr_md_e mode, int sr0_en,
	int sr1_en, int vpp_index)
{
	int i;
	int slice_max;
	int sr0_reg = VPP_SRSHARP0_PK_NR_EN;
	int sr1_reg = VPP_SRSHARP1_PK_NR_EN;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		for (i = SLICE0; i < slice_max; i++) {
			WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[i],
				sr0_en, 1, 1);
			WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[i],
				sr1_en, 1, 1);

			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr0_reg + sr_sharp_reg_ofst[i], sr0_en);
			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr1_reg + sr_sharp_reg_ofst[i], sr1_en);
		}
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < slice_max; i++) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(sr0_reg + sr_sharp_reg_ofst[i],
				sr0_en, 1, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(sr1_reg + sr_sharp_reg_ofst[i],
				sr1_en, 1, 1, vpp_index);

			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr0_reg + sr_sharp_reg_ofst[i], sr0_en);
			pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
				sr1_reg + sr_sharp_reg_ofst[i], sr1_en);
		}
	}
}

void ve_sharpness_enable(int sel)
{
	int sr0_reg = VPP_SRSHARP0_PK_NR_EN;
	int sr1_reg = VPP_SRSHARP1_PK_NR_EN;

	/*0:peaking enable   1:peaking disable*/
	/*2:lti/cti enable   3:lti/cti disable*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 1, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 1, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 1, 1);
		break;
	case 2:
		sr0_reg = VPP_SRSHARP0_HCTI_FLT_CLP_DC;
		sr1_reg = VPP_SRSHARP1_HCTI_FLT_CLP_DC;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 28, 1);
		sr0_reg = VPP_SRSHARP0_HLTI_FLT_CLP_DC;
		sr1_reg = VPP_SRSHARP1_HLTI_FLT_CLP_DC;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 28, 1);
		sr0_reg = VPP_SRSHARP0_VLTI_FLT_CON_CLP;
		sr1_reg = VPP_SRSHARP1_VLTI_FLT_CON_CLP;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 14, 1);
		sr0_reg = VPP_SRSHARP0_VCTI_FLT_CON_CLP;
		sr1_reg = VPP_SRSHARP1_VCTI_FLT_CON_CLP;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 14, 1);
		break;
	case 3:
		sr0_reg = VPP_SRSHARP0_HCTI_FLT_CLP_DC;
		sr1_reg = VPP_SRSHARP1_HCTI_FLT_CLP_DC;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 28, 1);
		sr0_reg = VPP_SRSHARP0_HLTI_FLT_CLP_DC;
		sr1_reg = VPP_SRSHARP1_HLTI_FLT_CLP_DC;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 28, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 28, 1);
		sr0_reg = VPP_SRSHARP0_VLTI_FLT_CON_CLP;
		sr1_reg = VPP_SRSHARP1_VLTI_FLT_CON_CLP;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 14, 1);
		sr0_reg = VPP_SRSHARP0_VCTI_FLT_CON_CLP;
		sr1_reg = VPP_SRSHARP1_VCTI_FLT_CON_CLP;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 14, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 14, 1);
		break;
	/*sr4 drtlpf theta en*/
	case 4:
		sr0_reg = VPP_SRSHARP0_SR3_DRTLPF_EN;
		sr1_reg = VPP_SRSHARP1_SR3_DRTLPF_EN;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			7, 4, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			7, 3, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			7, 4, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			7, 3, 3);
		break;
	case 5:
		sr0_reg = VPP_SRSHARP0_SR3_DRTLPF_EN;
		sr1_reg = VPP_SRSHARP1_SR3_DRTLPF_EN;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 4, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 3, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 4, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 3, 3);
		break;
	/*sr4 debanding en*/
	case 6:
		sr0_reg = VPP_SRSHARP0_DB_FLT_CTRL;
		sr1_reg = VPP_SRSHARP1_DB_FLT_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 23, 1);
		break;
	case 7:
		sr0_reg = VPP_SRSHARP0_DB_FLT_CTRL;
		sr1_reg = VPP_SRSHARP1_DB_FLT_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 23, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 4, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 5, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 22, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 23, 1);
		break;
	/*sr3 dejaggy en*/
	case 8:
		sr0_reg = VPP_SRSHARP0_DEJ_CTRL;
		sr1_reg = VPP_SRSHARP1_DEJ_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 0, 1);
		break;
	case 9:
		sr0_reg = VPP_SRSHARP0_DEJ_CTRL;
		sr1_reg = VPP_SRSHARP1_DEJ_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 0, 1);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 0, 1);
		break;
	/*sr3 dering en*/
	case 10:
		sr0_reg = VPP_SRSHARP0_SR3_DERING_CTRL;
		sr1_reg = VPP_SRSHARP1_SR3_DERING_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			1, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			1, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			1, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			1, 28, 3);
		break;
	case 11:
		sr0_reg = VPP_SRSHARP0_SR3_DERING_CTRL;
		sr1_reg = VPP_SRSHARP1_SR3_DERING_CTRL;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 28, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 28, 3);
		break;
	/*sr3 direction lpf en*/
	case 12:
		sr0_reg = VPP_SRSHARP0_SR3_DRTLPF_EN;
		sr1_reg = VPP_SRSHARP1_SR3_DRTLPF_EN;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			7, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			7, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			7, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			7, 0, 3);
		break;
	case 13:
		sr0_reg = VPP_SRSHARP0_SR3_DRTLPF_EN;
		sr1_reg = VPP_SRSHARP1_SR3_DRTLPF_EN;
		WRITE_VPP_REG_BITS_S5(sr0_reg,
			0, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg,
			0, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr0_reg + sr_sharp_reg_ofst[1],
			0, 0, 3);
		WRITE_VPP_REG_BITS_S5(sr1_reg + sr_sharp_reg_ofst[1],
			0, 0, 3);
		break;
	default:
		break;
	}
}

void ve_dnlp_set(ulong *data)
{
	int i;
	int j;
	int slice_max;
	int dnlp_reg = VPP_SRSHARP1_DNLP_00;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++)
		for (j = 0; j < 32; j++)
			WRITE_VPP_REG_S5(dnlp_reg + j + sr_sharp_reg_ofst[i],
				data[j]);
}

void ve_dnlp_ctl(int enable)
{
	int i;
	int slice_max;
	int dnlp_reg = VPP_SRSHARP1_DNLP_EN;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		WRITE_VPP_REG_BITS_S5(dnlp_reg + sr_sharp_reg_ofst[i],
			enable, 0, 1);
		pr_amve_v2("%s: addr = %x, val = %d\n", __func__,
			dnlp_reg + sr_sharp_reg_ofst[i], enable);
	}
}

void ve_dnlp_sat_set(unsigned int value, int vpp_index)
{
	int i;
	int slice_max;
	unsigned int reg_value;
	struct cm_port_s cm_port;
	int addr_reg;
	int data_reg;

	slice_max = get_slice_max();

	cm_port = get_cm_port();

	for (i = SLICE0; i < slice_max; i++) {
		addr_reg = cm_port.cm_addr_port[i];
		data_reg = cm_port.cm_data_port[i];

		VSYNC_WRITE_VPP_REG_VPP_SEL(addr_reg, 0x207, vpp_index);
		reg_value =
			VSYNC_READ_VPP_REG_VPP_SEL(data_reg, vpp_index);
		reg_value = (reg_value & 0xf000ffff) | (value << 16);

		VSYNC_WRITE_VPP_REG_VPP_SEL(addr_reg,
			0x207, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(data_reg,
			reg_value, vpp_index);
	}

	pr_amve_v2("%s: val = %d", __func__, value);
}

static void _lc_mtrx_set(enum lc_mtx_sel_e mtrx_sel,
	enum lc_mtx_csc_e mtrx_csc, int mtrx_en,
	int bitdepth, int slice_idx)
{
	unsigned int mtrx_coef00_01 = 0;
	unsigned int mtrx_coef02_10 = 0;
	unsigned int mtrx_coef11_12 = 0;
	unsigned int mtrx_coef20_21 = 0;
	unsigned int mtrx_coef22 = 0;
	unsigned int mtrx_clip = 0;
	unsigned int mtrx_offset0_1 = 0;
	unsigned int mtrx_offset2 = 0;
	unsigned int mtrx_pre_offset0_1 = 0;
	unsigned int mtrx_pre_offset2 = 0;
	unsigned int mtrx_en_ctrl = 0;
	int slice_max;

	slice_max = get_slice_max();
	if (slice_idx > slice_max - 1)
		slice_idx = slice_max - 1;

	switch (mtrx_sel) {
	case INP_MTX:
		mtrx_coef00_01 = VPP_SRSHARP1_LC_YUV2RGB_MAT_0_1 +
			sr_sharp_reg_ofst[slice_idx];
		mtrx_coef02_10 = mtrx_coef00_01 + 1;
		mtrx_coef11_12 = mtrx_coef02_10 + 1;
		mtrx_coef20_21 = mtrx_coef11_12 + 1;
		mtrx_coef22 = mtrx_coef20_21 + 1;
		mtrx_pre_offset0_1 = VPP_SRSHARP1_LC_YUV2RGB_OFST +
			sr_sharp_reg_ofst[slice_idx];
		mtrx_clip = VPP_SRSHARP1_LC_YUV2RGB_CLIP +
			sr_sharp_reg_ofst[slice_idx];
		break;
	case OUTP_MTX:
		mtrx_coef00_01 = VPP_SRSHARP1_LC_RGB2YUV_MAT_0_1 +
			sr_sharp_reg_ofst[slice_idx];
		mtrx_coef02_10 = mtrx_coef00_01 + 1;
		mtrx_coef11_12 = mtrx_coef02_10 + 1;
		mtrx_coef20_21 = mtrx_coef11_12 + 1;
		mtrx_coef22 = mtrx_coef20_21 + 1;
		mtrx_offset0_1 = VPP_SRSHARP1_LC_RGB2YUV_OFST +
			sr_sharp_reg_ofst[slice_idx];
		mtrx_clip = VPP_SRSHARP1_LC_RGB2YUV_CLIP +
			sr_sharp_reg_ofst[slice_idx];
		break;
	case STAT_MTX:
		mtrx_coef00_01 = VPP_LC_STTS_MATRIX_COEF00_01;
		mtrx_coef02_10 = mtrx_coef00_01 + 1;
		mtrx_coef11_12 = mtrx_coef02_10 + 1;
		mtrx_coef20_21 = mtrx_coef11_12 + 1;
		mtrx_coef22 = VPP_LC_STTS_MATRIX_COEF22;
		mtrx_offset0_1 = VPP_LC_STTS_MATRIX_OFFSET0_1;
		mtrx_offset2 = VPP_LC_STTS_MATRIX_OFFSET2;
		mtrx_pre_offset0_1 = VPP_LC_STTS_MATRIX_PRE_OFFSET0_1;
		mtrx_pre_offset2 = VPP_LC_STTS_MATRIX_PRE_OFFSET2;
		if (slice_idx == 0)
			mtrx_en_ctrl = VPP_LC_STTS_CTRL0;
		else
			mtrx_en_ctrl = VPP_LC_STTS_CTRL1;
		break;
	default:
		return;
	}

	if (mtrx_sel & STAT_MTX)
		WRITE_VPP_REG_BITS_S5(mtrx_en_ctrl, mtrx_en, 2, 1);

	if (!mtrx_en)
		return;

	switch (mtrx_csc) {
	case LC_MTX_RGB_YUV601L:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x01070204);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x00640f68);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x0ed601c2);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x01c20e87);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000fb7);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x00400200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x01000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x00bb0275);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x003f1f99);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1ea601c2);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x01c21e67);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00001fd7);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00400200);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000200);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000000);
		}
		break;
	case LC_MTX_YUV601L_RGB:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04a80000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x066204a8);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1e701cbf);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x04a80812);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x00400200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x01000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04a80000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x072c04a8);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1f261ddd);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x04a80876);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x07c00600);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000600);
		}
		break;
	case LC_MTX_RGB_YUV709L:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x00bb0275);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x003f1f99);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1ea601c2);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x01c21e67);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00001fd7);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x00400200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x01000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x00bb0275);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x003f1f99);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1ea601c2);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x01c21e67);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00001fd7);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00400200);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000200);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000000);
		}
		break;
	case LC_MTX_YUV709L_RGB:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04a80000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x072c04a8);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1f261ddd);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x04a80876);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x00400200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x01000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04a80000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x072c04a8);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1f261ddd);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x04a80876);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x07c00600);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000600);
		}
		break;
	case LC_MTX_RGB_YUV709:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x00da02dc);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x004a1f8a);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1e760200);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x02001e2f);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00001fd1);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x00000200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_offset0_1,
					0x00000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x00bb0275);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x003f1f99);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1ea601c2);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x01c21e67);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00001fd7);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00400200);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000200);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000000);
		}
		break;
	case LC_MTX_YUV709_RGB:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x064d0400);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1f411e21);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x0400076d);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			if (bitdepth == 10) {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x00000200);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x000003ff);
			} else {
				WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
					0x00000800);
				WRITE_VPP_REG_S5(mtrx_clip,
					0x00000fff);
			}
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x064d0400);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x1f411e21);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x0400076d);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x00000600);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000600);
		}
		break;
	case LC_MTX_NULL:
		if (mtrx_sel & (INP_MTX | OUTP_MTX)) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000400);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00000000);
		} else if (mtrx_sel & STAT_MTX) {
			WRITE_VPP_REG_S5(mtrx_coef00_01,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef02_10,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_coef11_12,
				0x04000000);
			WRITE_VPP_REG_S5(mtrx_coef20_21,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_coef22,
				0x00000400);
			WRITE_VPP_REG_S5(mtrx_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_offset2,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset0_1,
				0x00000000);
			WRITE_VPP_REG_S5(mtrx_pre_offset2,
				0x00000000);
		}
		break;
	default:
		break;
	}
}

static void _lc_blk_bdry_cfg(unsigned int height,
	unsigned int width, int h_num, int v_num, int rdma_mode,
	int vpp_index)
{
	int lc_reg;
	unsigned int value;
	int slice_case = ve_multi_slice_case_get();

	if (!h_num || !v_num)
		return;

	width /= h_num;
	height /= v_num;

	/*lc curve mapping block IDX default 4k panel*/
	/*slice0*/
	lc_reg = VPP_SRSHARP1_LC_CURVE_BLK_HIDX_0_1 +
		sr_sharp_reg_ofst[0];
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= width & GET_BITS(0, 14);
	value |= (0 << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (width * 3) & GET_BITS(0, 14);
	value |= ((width * 2) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (width * 5) & GET_BITS(0, 14);
	value |= ((width * 4) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	if (slice_case)
		value |= (width * 6 - lc_overlap) & GET_BITS(0, 14);
	else
		value |= (width * 7) & GET_BITS(0, 14);
	value |= ((width * 6) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	if (slice_case) {
		value |= (width * 8) & GET_BITS(0, 14);
		value |= ((width * 7) << 16) & GET_BITS(16, 14);
	} else {
		value |= (width * 9) & GET_BITS(0, 14);
		value |= ((width * 8) << 16) & GET_BITS(16, 14);
	}
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (width * 11) & GET_BITS(0, 14);
	value |= ((width * 10) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xffffc000;
	value |= (width * h_num) & GET_BITS(0, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg = VPP_SRSHARP1_LC_CURVE_BLK_VIDX_0_1 +
		sr_sharp_reg_ofst[0];
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= height & GET_BITS(0, 14);
	value |= (0 << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (height * 3) & GET_BITS(0, 14);
	value |= ((height * 2) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (height * 5) & GET_BITS(0, 14);
	value |= ((height * 4) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xc000c000;
	value |= (height * 7) & GET_BITS(0, 14);
	value |= ((height * 6) << 16) & GET_BITS(16, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	lc_reg += 1;
	value = READ_VPP_REG_S5(lc_reg);
	value &= 0xffffc000;
	value |= (height * v_num) & GET_BITS(0, 14);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, value);

	/*slice1*/
	if (slice_case) {
		lc_reg = VPP_SRSHARP1_LC_CURVE_BLK_HIDX_0_1 +
			sr_sharp_reg_ofst[1];
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= lc_overlap & GET_BITS(0, 14);
		value |= (0 << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (width * 2 + lc_overlap) & GET_BITS(0, 14);
		value |= ((width * 1 + lc_overlap) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (width * 4 + lc_overlap) & GET_BITS(0, 14);
		value |= ((width * 3 + lc_overlap) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (width * 6 + lc_overlap) & GET_BITS(0, 14);
		value |= ((width * 5 + lc_overlap) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (width * 8 + lc_overlap) & GET_BITS(0, 14);
		value |= ((width * 7 + lc_overlap) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (width * 10 + lc_overlap) & GET_BITS(0, 14);
		value |= ((width * 9 + lc_overlap) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xffffc000;
		value |= (width * h_num) & GET_BITS(0, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg = VPP_SRSHARP1_LC_CURVE_BLK_VIDX_0_1 +
			sr_sharp_reg_ofst[1];
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= height & GET_BITS(0, 14);
		value |= (0 << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (height * 3) & GET_BITS(0, 14);
		value |= ((height * 2) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (height * 5) & GET_BITS(0, 14);
		value |= ((height * 4) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xc000c000;
		value |= (height * 7) & GET_BITS(0, 14);
		value |= ((height * 6) << 16) & GET_BITS(16, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);

		lc_reg += 1;
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xffffc000;
		value |= (height * v_num) & GET_BITS(0, 14);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);
	}
}

void ve_lc_stts_blk_cfg(struct lc_stts_config_s cfg_data,
	unsigned int multi_slice_type, int rdma_mode, int vpp_index)
{
	int lc_reg = 0;
	int row_start = 0;
	int col_start = 0;
	int blk_height = 0, blk_width = 0;
	int data32 = 0;
	int hend0 = 0, hend1 = 0, hend2 = 0, hend3 = 0;
	int hend4 = 0, hend5 = 0, hend6 = 0;
	int hend7 = 0, hend8 = 0, hend9 = 0, hend10 = 0, hend11 = 0;
	int vend0 = 0, vend1 = 0, vend2 = 0, vend3 = 0;
	int vend4 = 0, vend5 = 0, vend6 = 0, vend7 = 0;
	int row_start_s1 = 0;
	int col_start_s1 = 0;
	int blk_height_s1 = 0, blk_width_s1 = 0;
	int hend0_s1 = 0, hend1_s1 = 0, hend2_s1 = 0, hend3_s1 = 0;
	int hend4_s1 = 0, hend5_s1 = 0, hend6_s1 = 0, hend7_s1 = 0;
	int hend8_s1 = 0, hend9_s1 = 0, hend10_s1 = 0, hend11_s1 = 0;
	int vend0_s1 = 0, vend1_s1 = 0, vend2_s1 = 0, vend3_s1 = 0;
	int vend4_s1 = 0, vend5_s1 = 0, vend6_s1 = 0, vend7_s1 = 0;

	pr_amve_v2("[%s] multi_slice_type %d\n",
		__func__, multi_slice_type);
	pr_amve_v2("[%s] size s0 h/v 0x%08x/0x%08x\n",
		__func__, cfg_data.hsize[0], cfg_data.vsize[0]);
	pr_amve_v2("[%s] num s0 h/v 0x%08x/0x%08x\n",
		__func__, cfg_data.h_num[0], cfg_data.v_num[0]);
	if (multi_slice_type == 1) {
		pr_amve_v2("[%s] size s1 h/v 0x%08x/0x%08x\n",
			__func__, cfg_data.hsize[1], cfg_data.vsize[1]);
		pr_amve_v2("[%s] num s1 h/v 0x%08x/0x%08x\n",
			__func__, cfg_data.h_num[1], cfg_data.v_num[1]);
	}

	blk_height = cfg_data.vsize[0] / cfg_data.v_num[0];
	blk_width = cfg_data.hsize[0] / cfg_data.h_num[0];

	hend0 = col_start + blk_width - 1;
	hend1 = hend0 + blk_width;
	hend2 = hend1 + blk_width;
	hend3 = hend2 + blk_width;
	hend4 = hend3 + blk_width;
	hend5 = hend4 + blk_width;

	if (multi_slice_type == 1) {
		hend6 = hend5;
		hend7 = hend5;
		hend8 = hend5;
		hend9 = hend5;
		hend10 = hend5;
		hend11 = hend5;
	} else {
		hend6 = hend5 + blk_width;
		hend7 = hend6 + blk_width;
		hend8 = hend7 + blk_width;
		hend9 = hend8 + blk_width;
		hend10 = hend9 + blk_width;
		hend11 = cfg_data.hsize[0] - 1;
	}

	vend0 = row_start + blk_height - 1;
	vend1 = vend0 + blk_height;
	vend2 = vend1 + blk_height;
	vend3 = vend2 + blk_height;
	vend4 = vend3 + blk_height;
	vend5 = vend4 + blk_height;
	vend6 = vend5 + blk_height;
	vend7 = cfg_data.vsize[0] - 10;

	if (multi_slice_type == 1) {
		blk_height_s1 = cfg_data.vsize[1] / cfg_data.v_num[1];
		blk_width_s1 = cfg_data.hsize[1] / cfg_data.h_num[1];
		col_start_s1 = cfg_data.overlap[1];

		hend0_s1 = col_start_s1 + blk_width_s1 - 1;
		hend1_s1 = hend0_s1 + blk_width_s1;
		hend2_s1 = hend1_s1 + blk_width_s1;
		hend3_s1 = hend2_s1 + blk_width_s1;
		hend4_s1 = hend3_s1 + blk_width_s1;
		hend5_s1 = hend4_s1 + blk_width_s1;
		hend6_s1 = hend5_s1;
		hend7_s1 = hend5_s1;
		hend8_s1 = hend5_s1;
		hend9_s1 = hend5_s1;
		hend10_s1 = hend5_s1;
		hend11_s1 = hend5_s1;

		vend0_s1 = row_start_s1 + blk_height_s1 - 1;
		vend1_s1 = vend0_s1 + blk_height_s1;
		vend2_s1 = vend1_s1 + blk_height_s1;
		vend3_s1 = vend2_s1 + blk_height_s1;
		vend4_s1 = vend3_s1 + blk_height_s1;
		vend5_s1 = vend4_s1 + blk_height_s1;
		vend6_s1 = vend5_s1 + blk_height_s1;
		vend7_s1 = cfg_data.vsize[1] - 10;
	}

	lc_reg = VPP_LC_STTS_HIST_REGION_IDX;
	data32 = READ_VPP_REG_S5(lc_reg);
	/*4:0 h_index and v_index lut start*/
	data32 = 0xffff00e0 & data32;

	if (multi_slice_type == 1) {
		/*15:12 slice1 h_num*/
		data32 = data32 | (6 << 12);
		/*11:8 slice0 h_num*/
		data32 = data32 | (6 << 8);
	} else {
		/*15:12 slice1 h_num*/
		data32 = data32 | (0 << 12);
		/*11:8 slice0 h_num*/
		data32 = data32 | (8 << 8);
		/*31 slice1 stts enable*/
		data32 = data32 & 0x7fffffff;
	}

	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);
	pr_amve_v2("[%s]: 0x%04x = 0x%08x\n", __func__, lc_reg, data32);

	lc_reg = VPP_LC_STTS_HIST_SET_REGION;
	data32 = (((row_start_s1 & 0x1fff) << 16) & 0xffff0000) |
		(row_start & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend1 & 0x1fff) << 16) | (vend0 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend3 & 0x1fff) << 16) | (vend2 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend5 & 0x1fff) << 16) | (vend4 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend7 & 0x1fff) << 16) | (vend6 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend1_s1 & 0x1fff) << 16) | (vend0_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend3_s1 & 0x1fff) << 16) | (vend2_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend5_s1 & 0x1fff) << 16) | (vend4_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((vend7_s1 & 0x1fff) << 16) | (vend6_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = (((col_start_s1 & 0x1fff) << 16) & 0xffff0000) |
		(col_start & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend1 & 0x1fff) << 16) | (hend0 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend3 & 0x1fff) << 16) | (hend2 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend5 & 0x1fff) << 16) | (hend4 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend7 & 0x1fff) << 16) | (hend6 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend9 & 0x1fff) << 16) | (hend8 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend11 & 0x1fff) << 16) | (hend10 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend1_s1 & 0x1fff) << 16) | (hend0_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend3_s1 & 0x1fff) << 16) | (hend2_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend5_s1 & 0x1fff) << 16) | (hend4_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend7_s1 & 0x1fff) << 16) | (hend6_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend9_s1 & 0x1fff) << 16) | (hend8_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	data32 = ((hend11_s1 & 0x1fff) << 16) | (hend10_s1 & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);
}

void ve_lc_stts_en(int enable,
	int pix_drop_mode, int eol_en, int hist_mode,
	int lpf_en, int din_sel, int bitdepth,
	int flag, int flag_full, int thd_black,
	int rdma_mode, int vpp_index)
{
	int i;
	int slice_max;
	int data32;
	int slice_case = ve_multi_slice_case_get();
	int lc_stts_en = enable;

	if (slice_case || multi_picture_case) {
		slice_max = get_slice_max();
	} else {
		slice_max = SLICE1;
	}

	for (i = SLICE0; i < slice_max; i++) {
		if (flag == 0x3) {
			_lc_mtrx_set(STAT_MTX, LC_MTX_YUV601L_RGB,
				enable, bitdepth, i);
		} else {
			if (flag_full == 1)
				_lc_mtrx_set(STAT_MTX, LC_MTX_YUV709_RGB,
					enable, bitdepth, i);
			else
				_lc_mtrx_set(STAT_MTX, LC_MTX_YUV709L_RGB,
					enable, bitdepth, i);
		}
	}

	data32 = READ_VPP_REG_S5(VPP_LC_STTS_CTRL0);
	data32 = (data32 & 0xffffffc7) | ((din_sel & 0x7) << 3);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_CTRL0,
			data32, vpp_index);
	else
		WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL0, din_sel, 3, 3);

	/*lc lpf enable*/
	if (lpf_en) {
		if (slice_case || multi_picture_case)
			lpf_en = 3;
		else
			lpf_en = 1;
	} else {
		lpf_en = 0;
	}

	/*lc hist stts enable*/
	if (enable) {
		if (slice_case || multi_picture_case)
			lc_stts_en = 3;
		else
			lc_stts_en = 1;
	} else {
		lc_stts_en = 0;
	}

	/*lc input probe enable*/
	/*VSYNC_WRITE_VPP_REG_BITS(VPP_LC_STTS_CTRL0, enable, 9, 2);*/

	data32 = READ_VPP_REG_S5(VPP_LC_STTS_HIST_REGION_IDX);
	data32 = (data32 & 0x0c8fffff) | ((lc_stts_en & 0x3) << 30);
	data32 = data32 | ((pix_drop_mode & 0x3) << 28);
	data32 = data32 | ((eol_en & 0x3) << 24);
	/*22: lc_hist_mode, 0 for y, 1 for maxrgb*/
	data32 = data32 | ((hist_mode & 0x1) << 22);
	data32 = data32 | ((lpf_en & 0x3) << 20);
	pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
		__func__, VPP_LC_STTS_HIST_REGION_IDX, data32);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_HIST_REGION_IDX,
			data32, vpp_index);
	else
		WRITE_VPP_REG_S5(VPP_LC_STTS_HIST_REGION_IDX, data32);

	if (rdma_mode) {
		data32 = READ_VPP_REG_S5(VPP_LC_STTS_BLACK_INFO1);
		data32 = (data32 & 0xffffff00) | (thd_black & 0xff);
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_BLACK_INFO1,
			data32, vpp_index);

		if (slice_case) {
			data32 = READ_VPP_REG_S5(VPP_LC_STTS_BLACK_INFO2);
			data32 = (data32 & 0xffffff00) | (thd_black & 0xff);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_BLACK_INFO2,
				data32, vpp_index);

			data32 = READ_VPP_REG_S5(VPP_LC_STTS_CTRL);
			data32 = (data32 & 0xfffffff8) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_CTRL,
				data32, vpp_index);
		} else if (multi_picture_case) {
			data32 = READ_VPP_REG_S5(VPP_LC_STTS_BLACK_INFO2);
			data32 = (data32 & 0xffffff00) | (thd_black & 0xff);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_BLACK_INFO2,
				data32, vpp_index);

			data32 = READ_VPP_REG_S5(VPP_LC_STTS_CTRL);
			data32 = (data32 & 0xfffffff8) | 0x6;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_CTRL,
				data32, vpp_index);
		} else {
			data32 = READ_VPP_REG_S5(VPP_LC_STTS_CTRL);
			data32 = (data32 & 0xfffffff8) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_CTRL,
				data32, vpp_index);
		}
	} else {
		WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_BLACK_INFO1,
			thd_black, 0, 8);

		if (slice_case) {
			WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_BLACK_INFO2,
				thd_black, 0, 8);
			WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL, 1, 0, 3);
		} else if (multi_picture_case) {
			WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_BLACK_INFO2,
				thd_black, 0, 8);
			WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL, 6, 0, 3);
		} else {
			WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL, 0, 0, 3);
		}
	}
}

void ve_lc_blk_num_get(int *h_num, int *v_num,
	int slice)
{
	int slice_max;
	int lc_reg;
	int dwtemp;

	if (!h_num || !v_num)
		return;

	slice_max = get_slice_max();

	if (slice >= slice_max || slice < 0)
		slice = 0;

	lc_reg = VPP_LC1_CURVE_HV_NUM +
		lc_reg_ofst[slice];
	dwtemp = READ_VPP_REG_S5(lc_reg);
	*h_num = (dwtemp >> 8) & 0x1f;
	*v_num = dwtemp & 0x1f;
}

void ve_lc_disable(int rdma_mode, int vpp_index)
{
	int lc_reg;
	int data32;

	lc_reg = VPP_LC_STTS_HIST_REGION_IDX;
	data32 = READ_VPP_REG_S5(lc_reg);
	if (data32 & 0xc0000000) {
		data32 = data32 & 0x3fffffe0;
		pr_amve_v2("[%s]: 0x%04x = 0x%08x\n", __func__, lc_reg, data32);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, data32);
	}

	/*slice 0*/
	lc_reg = VPP_SRSHARP1_LC_TOP_CTRL + sr_sharp_reg_ofst[0];
	data32 = READ_VPP_REG_S5(lc_reg);
	if (data32 & 0x10) {
		pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
			__func__, lc_reg, data32);
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 4, 1);
	}

	lc_reg = VPP_LC1_CURVE_CTRL + lc_reg_ofst[0];
	if (rdma_mode) {
		data32 = READ_VPP_REG_S5(lc_reg);
		if (data32 & 0x1) {
			data32 &= 0xfffffffe;
			pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
		}
	} else {
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);
	}

	lc_reg = VPP_LC1_CURVE_RAM_CTRL + lc_reg_ofst[0];
	if (rdma_mode) {
		data32 = READ_VPP_REG_S5(lc_reg);
		if (data32 & 0x1) {
			data32 &= 0xfffffffe;
			pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
		}
	} else {
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);
	}

	/*slice 1*/
	lc_reg = VPP_SRSHARP1_LC_TOP_CTRL + sr_sharp_reg_ofst[1];
	data32 = READ_VPP_REG_S5(lc_reg);
	if (data32 & 0x10) {
		pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
			__func__, lc_reg, data32);
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 4, 1);
	}

	lc_reg = VPP_LC1_CURVE_CTRL + lc_reg_ofst[1];
	if (rdma_mode) {
		data32 = READ_VPP_REG_S5(lc_reg);
		if (data32 & 0x1) {
			data32 &= 0xfffffffe;
			pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
		}
	} else {
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);
	}

	lc_reg = VPP_LC1_CURVE_RAM_CTRL + lc_reg_ofst[1];
	if (rdma_mode) {
		data32 = READ_VPP_REG_S5(lc_reg);
		if (data32 & 0x1) {
			data32 &= 0xfffffffe;
			pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
		}
	} else {
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);
	}
}

void ve_lc_curve_ctrl_cfg(int enable, int rdma_mode,
	int vpp_index)
{
	int i;
	int slice_max;
	int lc_reg;
	int tmp;
	int data32;
	int slice_case = ve_multi_slice_case_get();

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		if (!enable) {
			lc_reg = VPP_LC1_CURVE_CTRL + lc_reg_ofst[i];
			if (rdma_mode) {
				data32 = READ_VPP_REG_S5(lc_reg);
				data32 &= 0xfffffffe;
				pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
					__func__, lc_reg, data32);
				VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
			} else {
				WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);
			}
		} else {
			lc_reg = VPP_LC1_CURVE_CTRL + lc_reg_ofst[i];
			if (rdma_mode) {
				data32 = READ_VPP_REG_S5(lc_reg);
				data32 = (data32 & 0x7ffffffe) | 0x1 | (0x1 << 31);
				if (!slice_case && i == SLICE1)
					data32 &= 0x7ffffffe;
				pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
					__func__, lc_reg, data32);
				VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
			} else {
				tmp = 1;
				if (!slice_case && i == SLICE1)
					tmp = 0;
				WRITE_VPP_REG_BITS_S5(lc_reg, tmp, 0, 1);
				WRITE_VPP_REG_BITS_S5(lc_reg, tmp, 31, 1);
			}
		}
	}
}

void ve_lc_top_cfg(int enable, int h_num, int v_num,
	unsigned int height, unsigned int width, int bitdepth,
	int flag, int flag_full, int rdma_mode, int vpp_index)
{
	int i;
	int slice_max;
	int lc_reg;
	unsigned int value;
	int slice_case = ve_multi_slice_case_get();
	unsigned int hsize_out, hwin_begin, hwin_end;

	slice_max = get_slice_max();

	if (slice_case) {
		value = READ_VPP_REG_S5(V2_VD_S1_HWIN_CUT);
		hwin_begin = (value >> 16) & 0x1fff;
		hwin_end = value & 0x1fff;
		hsize_out = READ_VPP_REG_S5(V2_VD_PROC_S1_OUT_SIZE);
		hsize_out = (hsize_out >> 16) & 0x1fff;

		lc_overlap = hsize_out - (hwin_end - hwin_begin + 1);
		lc_overlap_s0 = lc_overlap;

		pr_amve_v2("[lc_overlap_2slice] %d/%d/h_out:%d/w_b:%d/w_e:%d\n",
			lc_overlap_s0, lc_overlap,
			hsize_out, hwin_begin, hwin_end);
	} else {
		pr_amve_v2("[lc_overlap_1slice] h/w:%d/%d\n",
			height, width);
	}

	/*lc curve mapping block IDX default 4k panel*/
	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_SRSHARP1_LC_HV_NUM +
			sr_sharp_reg_ofst[i];
		value = READ_VPP_REG_S5(lc_reg);
		value &= 0xffffe0e0;
		/*lc ram write h num and v num*/
		value |= v_num & GET_BITS(0, 5);
		value |= (h_num << 8) & GET_BITS(8, 5);
		if (rdma_mode)
			VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, value, vpp_index);
		else
			WRITE_VPP_REG_S5(lc_reg, value);
	}

	/*lc curve mapping config*/
	_lc_blk_bdry_cfg(height, width, h_num, v_num, rdma_mode, vpp_index);

	for (i = SLICE0; i < slice_max; i++) {
		if (flag == 0x3) {
			/* bt601 use 601 matrix */
			_lc_mtrx_set(INP_MTX, LC_MTX_YUV601L_RGB,
				1, bitdepth, i);
			_lc_mtrx_set(OUTP_MTX, LC_MTX_RGB_YUV601L,
				1, bitdepth, i);
		} else {
			/*all other cases use 709 by default*/
			/*to do, should we handle bg2020 separately?*/
			/*for special signal, keep full range to avoid clipping*/
			if (flag_full == 1) {
				_lc_mtrx_set(INP_MTX, LC_MTX_YUV709_RGB,
					1, bitdepth, i);
				_lc_mtrx_set(OUTP_MTX, LC_MTX_RGB_YUV709,
					1, bitdepth, i);
			} else {
				_lc_mtrx_set(INP_MTX, LC_MTX_YUV709L_RGB,
					1, bitdepth, i);
				_lc_mtrx_set(OUTP_MTX, LC_MTX_RGB_YUV709L,
					1, bitdepth, i);
			}
		}
	}
}

void ve_lc_mapping_ctrl(int enable, int rdma_mode, int vpp_index)
{
	int lc_reg;
	int data32;
	int sync_ctrl = 0; /*0: pre_frame, 1: cur_frame*/
	int slice_case = ve_multi_slice_case_get();
	int lc_mapping_en = enable;

	/*slice 0*/
	lc_reg = VPP_SRSHARP1_LC_TOP_CTRL + sr_sharp_reg_ofst[0];
	/*lc enable need set at last*/
	/*bit0: lc blend mode, default 1*/
	data32 = READ_VPP_REG_S5(lc_reg);
	if (((data32 & 0x10) >> 4) != lc_mapping_en) {
		if (!delay) {
			pr_amve_v2("[%s]: pre mapping 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			data32 = (data32 & 0xfffe00ee) |
				((lc_mapping_en & 0x1) << 4) |
				(0x8 << 8) | (0x1 << 0) |
				((sync_ctrl & 0x1) << 16);
			pr_amve_v2("[%s]: mapping 0x%04x = 0x%08x, %d\n",
				__func__, lc_reg, data32, lc_mapping_en);
			if (rdma_mode)
				VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
			else
				WRITE_VPP_REG_S5(lc_reg, data32);

			delay = counter;
		} else {
			delay--;
		}
	}

	/*slice 1*/
	lc_reg = VPP_SRSHARP1_LC_TOP_CTRL + sr_sharp_reg_ofst[1];
	data32 = READ_VPP_REG_S5(lc_reg);
	if (slice_case) {
		if (((data32 & 0x10) >> 4) != lc_mapping_en) {
			pr_amve_v2("[%s]: pre mapping 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			data32 = (data32 & 0xfffe00ee) |
			((lc_mapping_en & 0x1) << 4) |
			(0x8 << 8) | (0x1 << 0) |
			((sync_ctrl & 0x1) << 16);
			pr_amve_v2("[%s]: mapping 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			if (rdma_mode)
				VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
			else
				WRITE_VPP_REG_S5(lc_reg, data32);
		}
	} else {
		if (((data32 & 0x10) >> 4) != 0) {
			pr_amve_v2("[%s]: pre mapping 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			data32 &= 0xfffe00ee;
			pr_amve_v2("[%s]: mapping 0x%04x = 0x%08x\n",
				__func__, lc_reg, data32);
			if (rdma_mode)
				VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);
			else
				WRITE_VPP_REG_S5(lc_reg, data32);
		}
	}
}

void ve_lc_sat_lut_set(int *data)
{
	int i;
	int j;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_SRSHARP1_LC_SAT_LUT_0_1 +
			sr_sharp_reg_ofst[i];

		for (j = 0; j < 31 ; j++) {
			tmp = ((data[2 * j] & 0xfff) << 16) |
				(data[2 * j + 1] & 0xfff);
			WRITE_VPP_REG_S5(lc_reg + j, tmp);
		}

		lc_reg = VPP_SRSHARP1_LC_SAT_LUT_62 +
			sr_sharp_reg_ofst[i];
		tmp = data[62] & 0xfff;
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_ymin_lmt_set(int *data)
{
	int i;
	int j;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_YMINVAL_LMT_0_1 +
			lc_reg_ofst[i];

		for (j = 0; j < 6 ; j++) {
			tmp = ((data[2 * j] & 0x3ff) << 16) |
				(data[2 * j + 1] & 0x3ff);
			WRITE_VPP_REG_S5(lc_reg + j, tmp);
		}

		lc_reg = VPP_LC1_CURVE_YMINVAL_LMT_12_13 +
			lc_reg_ofst[i];
		tmp = ((data[2 * j] & 0x3ff) << 16) |
			(data[2 * j + 1] & 0x3ff);
		WRITE_VPP_REG_S5(lc_reg, tmp);

		j++;
		lc_reg = VPP_LC1_CURVE_YMINVAL_LMT_14_15 +
			lc_reg_ofst[i];
		tmp = ((data[2 * j] & 0x3ff) << 16) |
			(data[2 * j + 1] & 0x3ff);
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_ymax_lmt_set(int *data)
{
	int i;
	int j;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_YMAXVAL_LMT_0_1 +
			lc_reg_ofst[i];

		for (j = 0; j < 6 ; j++) {
			tmp = ((data[2 * j] & 0x3ff) << 16) |
				(data[2 * j + 1] & 0x3ff);
			WRITE_VPP_REG_S5(lc_reg + j, tmp);
		}

		lc_reg = VPP_LC1_CURVE_YMAXVAL_LMT_12_13 +
			lc_reg_ofst[i];
		tmp = ((data[2 * j] & 0x3ff) << 16) |
			(data[2 * j + 1] & 0x3ff);
		WRITE_VPP_REG_S5(lc_reg, tmp);

		j++;
		lc_reg = VPP_LC1_CURVE_YMAXVAL_LMT_14_15 +
			lc_reg_ofst[i];
		tmp = ((data[2 * j] & 0x3ff) << 16) |
			(data[2 * j + 1] & 0x3ff);
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_ypkbv_lmt_set(int *data)
{
	int i;
	int j;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_YPKBV_LMT_0_1 +
			lc_reg_ofst[i];

		for (j = 0; j < 8 ; j++) {
			tmp = ((data[2 * j] & 0x3ff) << 16) |
				(data[2 * j + 1] & 0x3ff);
			WRITE_VPP_REG_S5(lc_reg + j, tmp);
		}
	}
}

void ve_lc_ypkbv_rat_set(int *data)
{
	int i;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_YPKBV_RAT +
			lc_reg_ofst[i];
		tmp = ((data[0] & 0xff) << 24) |
			((data[1] & 0xff) << 16) |
			((data[2] & 0xff) << 8) |
			(data[3] & 0xff);
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_ypkbv_slp_lmt_set(int *data)
{
	int i;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_YPKBV_SLP_LMT +
			lc_reg_ofst[i];
		tmp = ((data[0] & 0xff) << 8) |
			(data[1] & 0xff);
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_contrast_lmt_set(int *data)
{
	int i;
	int slice_max;
	int lc_reg;
	int tmp;

	if (!data)
		return;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		lc_reg = VPP_LC1_CURVE_CONTRAST_LMT_LH +
			lc_reg_ofst[i];
		tmp = ((data[0] & 0xff) << 24) |
			((data[1] & 0xff) << 16) |
			((data[2] & 0xff) << 8) |
			(data[3] & 0xff);
		WRITE_VPP_REG_S5(lc_reg, tmp);
	}
}

void ve_lc_curve_set(int init_flag,
	int demo_mode, int *data, int slice, int vpp_index)
{
	int i, j, k;
	int slice_max;
	int lc_reg;
	int ctrl_reg;
	int addr_reg;
	int data_reg;
	int h_num, v_num;
	unsigned int tmp, tmp1;
	unsigned int lnr_data, lnr_data1;
	unsigned int offset = 0;
	unsigned int shift = 0;
	int slice_case = ve_multi_slice_case_get();

	if (!init_flag && !data)
		return;

	/*initial linear data*/
	lnr_data = 0 | (0 << 10) | (512 << 20);
	lnr_data1 = 1023 | (1023 << 10) | (512 << 20);

	slice_max = get_slice_max();

	if (slice >= slice_max || slice < 0)
		slice = 0;

	lc_reg = VPP_SRSHARP1_LC_HV_NUM +
		sr_sharp_reg_ofst[slice];
	ctrl_reg = VPP_SRSHARP1_LC_MAP_RAM_CTRL +
		sr_sharp_reg_ofst[slice];
	addr_reg = VPP_SRSHARP1_LC_MAP_RAM_ADDR +
		sr_sharp_reg_ofst[slice];
	data_reg = VPP_SRSHARP1_LC_MAP_RAM_DATA +
		sr_sharp_reg_ofst[slice];

	tmp = READ_VPP_REG_S5(lc_reg);
	h_num = (tmp >> 8) & 0x1f;
	v_num = tmp & 0x1f;

	/*data sequence: ymin/minBv/pkBv/maxBv/ymaxv/ypkBv*/
	if (init_flag) {
		WRITE_VPP_REG_S5(ctrl_reg, 1);
		WRITE_VPP_REG_S5(addr_reg, 0);

		for (i = 0; i < h_num * v_num; i++) {
			WRITE_VPP_REG_S5(data_reg, lnr_data);
			WRITE_VPP_REG_S5(data_reg, lnr_data1);
		}

		WRITE_VPP_REG_S5(ctrl_reg, 0);
		return;
	}

	if (slice == 1)
		offset = 5;

	VSYNC_WRITE_VPP_REG_VPP_SEL(ctrl_reg, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(addr_reg, 0, vpp_index);

	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num; j++) {
			switch (demo_mode) {
			case 0:/*off*/
			default:
				if (slice_case &&
					slice == 0 &&
					j > 5)
					shift = 1;
				else
					shift = 0;

				k = 6 * (i * h_num + j + offset - shift);
				tmp = data[k + 0] |
					(data[k + 1] << 10) |
					(data[k + 2] << 20);
				tmp1 = data[k + 3] |
					(data[k + 4] << 10) |
					(data[k + 5] << 20);
				break;
			case 1:/*left_side*/
				if (j < h_num / 2) {
					k = 6 * (i * h_num + j);
					tmp = data[k + 0] |
						(data[k + 1] << 10) |
						(data[k + 2] << 20);
					tmp1 = data[k + 3] |
						(data[k + 4] << 10) |
						(data[k + 5] << 20);
				} else {
					tmp = lnr_data;
					tmp1 = lnr_data1;
				}
				break;
			case 2:/*right_side*/
				if (j < h_num / 2) {
					tmp = lnr_data;
					tmp1 = lnr_data1;
				} else {
					k = 6 * (i * h_num + j);
					tmp = data[k + 0] |
						(data[k + 1] << 10) |
						(data[k + 2] << 20);
					tmp1 = data[k + 3] |
						(data[k + 4] << 10) |
						(data[k + 5] << 20);
				}
				break;
			}

			VSYNC_WRITE_VPP_REG_VPP_SEL(data_reg, tmp, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(data_reg, tmp1, vpp_index);
		}
	}

	VSYNC_WRITE_VPP_REG_VPP_SEL(ctrl_reg, 0, vpp_index);
}

void ve_lc_base_init(void)
{
	int i;
	int lc_reg;

	for (i = SLICE0; i < SLICE2; i++) {
		lc_reg = VPP_SRSHARP1_LC_TOP_CTRL +
			sr_sharp_reg_ofst[i];

		if (!slt_en)
			WRITE_VPP_REG_S5(lc_reg, 0x0);

		lc_reg = VPP_SRSHARP1_LC_INPUT_MUX +
			sr_sharp_reg_ofst[i];
		/*lc input_ysel*/
		WRITE_VPP_REG_BITS_S5(lc_reg, 5, 4, 3);
		/*lc input_csel*/
		WRITE_VPP_REG_BITS_S5(lc_reg, 5, 0, 3);

		lc_reg = VPP_LC1_CURVE_RAM_CTRL +
			lc_reg_ofst[i];
		WRITE_VPP_REG_BITS_S5(lc_reg, 0, 0, 1);

		/*default lc low parameters*/
		lc_reg = VPP_LC1_CURVE_CONTRAST_LH +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x000b000b);

		lc_reg = VPP_LC1_CURVE_CONTRAST_SCL_LH +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x00000b0b);

		lc_reg = VPP_LC1_CURVE_MISC0 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x00023028);

		lc_reg = VPP_LC1_CURVE_YPKBV_RAT +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x8cc0c060);

		lc_reg = VPP_LC1_CURVE_YPKBV_SLP_LMT +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x00000b3a);

		lc_reg = VPP_LC1_CURVE_YMINVAL_LMT_0_1 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x0030005d);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x00830091);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x00a000c4);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x00e00100);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x01200140);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x01600190);

		lc_reg = VPP_LC1_CURVE_YMINVAL_LMT_12_13 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x01b001d0);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x01f00210);

		lc_reg = VPP_LC1_CURVE_YMAXVAL_LMT_0_1 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x004400b4);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x00fb0123);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x015901a2);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x01d90208);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x02400280);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x02d70310);

		lc_reg = VPP_LC1_CURVE_YMAXVAL_LMT_12_13 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x03400380);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x03c003ff);

		lc_reg = VPP_LC1_CURVE_YPKBV_LMT_0_1 +
			lc_reg_ofst[i];
		WRITE_VPP_REG_S5(lc_reg, 0x004400b4);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x00fb0123);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x015901a2);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x01d90208);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x02400280);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x02d70310);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x03400380);
		lc_reg += 1;
		WRITE_VPP_REG_S5(lc_reg, 0x03c003ff);
	}
}

void ve_lc_region_read(int blk_vnum, int blk_hnum,
	int slice, int *black_count,
	int *curve_data, int *hist_data)
{
	int slice_max;
	unsigned int lc_reg;
	unsigned int ctrl_reg;
	unsigned int addr_reg;
	unsigned int data_reg;
	unsigned int i, j;
	unsigned int tmp, tmp1;
	unsigned int cur_block;
	unsigned int length;
	int slice_case = ve_multi_slice_case_get();

	if (!black_count || !curve_data || !hist_data ||
		slice_case)
		return;

	slice_max = get_slice_max();

	if (slice >= slice_max || slice < 0)
		slice = 0;

	if (slice == 0)
		lc_reg = VPP_LC_STTS_BLACK_INFO1;
	else
		lc_reg = VPP_LC_STTS_BLACK_INFO2;

	tmp = READ_VPP_REG_S5(lc_reg);
	*black_count = ((tmp >> 8) & 0xffffff) / 96;

	ctrl_reg = VPP_LC1_CURVE_RAM_CTRL +
		lc_reg_ofst[slice];
	addr_reg = VPP_LC1_CURVE_RAM_ADDR +
		lc_reg_ofst[slice];
	data_reg = VPP_LC1_CURVE_RAM_DATA +
		lc_reg_ofst[slice];

	/*part1: get lc curve node*/
	WRITE_VPP_REG_S5(ctrl_reg, 1);
	WRITE_VPP_REG_S5(addr_reg, 0);
	for (i = 0; i < blk_vnum; i++) {
		for (j = 0; j < blk_hnum; j++) {
			cur_block = i * blk_hnum + j;
			tmp = READ_VPP_REG_S5(data_reg);
			tmp1 = READ_VPP_REG_S5(data_reg);
			curve_data[cur_block * 6 + 0] =
				tmp & 0x3ff; /*bit0:9*/
			curve_data[cur_block * 6 + 1] =
				(tmp >> 10) & 0x3ff; /*bit10:19*/
			curve_data[cur_block * 6 + 2] =
				(tmp >> 20) & 0x3ff; /*bit20:29*/
			curve_data[cur_block * 6 + 3] =
				tmp1 & 0x3ff; /*bit0:9*/
			curve_data[cur_block * 6 + 4] =
				(tmp1 >> 10) & 0x3ff; /*bit10:19*/
			curve_data[cur_block * 6 + 5] =
				(tmp1 >> 20) & 0x3ff; /*bit20:29*/
		}
	}
	WRITE_VPP_REG_S5(ctrl_reg, 0);

	/*part2: get lc hist*/
	length = 1632; /*12*8*17*/
	/*am_dma_get_mif_data_lc_stts(slice, hist_data, length);*/
}

void ve_lc_total_en_ctrl(int enable, int rdma_mode, int vpp_index)
{
	/*int i;*/
	int lc_reg;
	/*int slice_max;*/
	int data32;
	int lc_stts_enable;
	int slice_case = ve_multi_slice_case_get();

	pr_amve_v2("[%s] vpp_index = %d\n", __func__, vpp_index);

	/*slice_max = get_slice_max();*/

	/*lc mapping enable*/
	/*for (i = SLICE0; i < slice_max; i++) {*/
	/*	lc_reg = VPP_SRSHARP1_LC_TOP_CTRL +*/
	/*		sr_sharp_reg_ofst[i];*/
	/*	data32 = READ_VPP_REG_S5(lc_reg);*/
	/*	data32 = (data32 & 0xffffffef) |*/
	/*		((enable & 0x1) << 4);*/
	/*	if (!slice_case && i == SLICE1)*/
	/*		data32 &= 0xffffffef;*/
		/*pr_amve_v2("[%s] mapping 0x%04x = 0x%08x\n",*/
		/*	__func__, lc_reg, data32);*/
		/*if (rdma_mode)*/
		/*	VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, data32, vpp_index);*/
		/*else*/
		/*	WRITE_VPP_REG_S5(lc_reg, enable);*/
	/*}*/

	/*lc dma chnl enable*/
	lc_reg = 0x27d6;
	data32 = READ_VPP_REG_S5(lc_reg);
	pr_amve_v2("[%s] before set 0x%04x data32 = 0x%08x\n",
		__func__, lc_reg, data32);
	data32 |= (enable & 0x1) << 16;
	pr_amve_v2("[%s] after set 0x%04x data32 = 0x%08x\n",
		__func__, lc_reg, data32);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg,
			data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);

	/*lc hist stts enable*/
	if (enable) {
		if (slice_case || multi_picture_case)
			lc_stts_enable = 3;
		else
			lc_stts_enable = 1;
	} else {
		lc_stts_enable = 0;
	}

	lc_reg = VPP_LC_STTS_HIST_REGION_IDX;
	data32 = READ_VPP_REG_S5(lc_reg);
	data32 = (data32 & 0x3fffffff) | ((lc_stts_enable & 0x3) << 30);
	pr_amve_v2("[%s] stts 0x%04x = 0x%08x\n",
		__func__, lc_reg, data32);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg,
			data32, vpp_index);
	else
		WRITE_VPP_REG_S5(lc_reg, data32);
}

static unsigned int pre_pps_dout_hsize[2];
static unsigned int pre_pps_dout_vsize[2];

void ve_size_info_update(int vpp_index)
{
	int rdma_mode = 1;
	int lc_reg;
	int reg_offset;
	int tmp;
	int h_num = 12;
	int v_num = 8;
	int h_num_s1 = 12;
	int v_num_s1 = 8;
	unsigned int height;
	unsigned int width;
	unsigned int histvld_thrd;
	unsigned int blackbar_mute_thrd;
	unsigned int lmtrat_minmax;
	unsigned int pps_dout_hsize[2];
	unsigned int pps_dout_vsize[2];
	unsigned int multi_slice_type = 0;
	struct vd_proc_amvecm_info_t *vd_proc_info;
	struct lc_stts_config_s stts_cfg_data;

	if (chip_type_id != chip_t3x)
		return;

	vd_proc_info = get_vd_proc_amvecm_info();
	pps_dout_hsize[0] = vd_proc_info->slice[0].pps_dout_hsize;
	pps_dout_vsize[0] = vd_proc_info->slice[0].pps_dout_vsize;
	pps_dout_hsize[1] = vd_proc_info->slice[1].pps_dout_hsize;
	pps_dout_vsize[1] = vd_proc_info->slice[1].pps_dout_vsize;

	if (pre_pps_dout_hsize[0] == pps_dout_hsize[0] &&
		pre_pps_dout_vsize[0] == pps_dout_vsize[0] &&
		pre_pps_dout_hsize[1] == pps_dout_hsize[1] &&
		pre_pps_dout_vsize[1] == pps_dout_vsize[1])
		return;

	pr_amve_v2("[%s] vd_proc_info slice_num %d\n",
		__func__, vd_proc_info->slice_num);
	pr_amve_v2("[%s] vd_proc_info s0 pps_h/v 0x%08x/0x%08x\n",
		__func__, pps_dout_hsize[0], pps_dout_vsize[0]);
	pr_amve_v2("[%s] vd_proc_info s1 pps_h/v 0x%08x/0x%08x\n",
		__func__, pps_dout_hsize[1], pps_dout_vsize[1]);

	pre_pps_dout_hsize[0] = pps_dout_hsize[0];
	pre_pps_dout_vsize[0] = pps_dout_vsize[0];
	pre_pps_dout_hsize[1] = pps_dout_hsize[1];
	pre_pps_dout_vsize[1] = pps_dout_vsize[1];

	stts_cfg_data.hsize[0] = pps_dout_hsize[0];
	stts_cfg_data.vsize[0] = pps_dout_vsize[0];
	stts_cfg_data.h_num[0] = h_num;
	stts_cfg_data.v_num[0] = v_num;

	if (vd_proc_info->slice_num > 1) {
		multi_slice_type = 1;
		stts_cfg_data.hsize[1] = pps_dout_hsize[1];
		stts_cfg_data.vsize[1] = pps_dout_vsize[1];
		stts_cfg_data.h_num[1] = h_num_s1;
		stts_cfg_data.v_num[1] = v_num_s1;
		stts_cfg_data.overlap[1] = 32;
	} else {
		multi_slice_type = 0;
	}

	ve_lc_stts_blk_cfg(stts_cfg_data, multi_slice_type,
		rdma_mode, vpp_index);

	height = pps_dout_vsize[0];
	width = pps_dout_hsize[0];
	reg_offset = lc_reg_ofst[0];

	lc_reg = VPP_LC1_CURVE_HV_NUM + reg_offset;
	VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg,
		(h_num << 8) | v_num, vpp_index);

	lc_reg = VPP_LC1_CURVE_LMT_RAT + reg_offset;
	tmp = READ_VPP_REG_S5(lc_reg);
	lmtrat_minmax = (tmp >> 8) & 0xff;
	tmp = (height * width) / (h_num * v_num);
	histvld_thrd = (tmp * lmtrat_minmax) >> 10;
	blackbar_mute_thrd = tmp >> 3;

	lc_reg = VPP_LC1_CURVE_HISTVLD_THRD + reg_offset;
	VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, histvld_thrd, vpp_index);

	lc_reg = VPP_LC1_CURVE_BB_MUTE_THRD + reg_offset;
	VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, blackbar_mute_thrd,
		vpp_index);

	tmp = ((height - 1) << 16) | (width - 1);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS1_WIDTHM1_HEIGHTM1,
		tmp, vpp_index);

	if (multi_slice_type) {
		height = pps_dout_vsize[1];
		width = pps_dout_hsize[1];
		reg_offset = lc_reg_ofst[1];

		lc_reg = VPP_LC1_CURVE_LMT_RAT + reg_offset;
		tmp = READ_VPP_REG_S5(lc_reg);
		lmtrat_minmax = (tmp >> 8) & 0xff;
		tmp = (height * width) / (h_num_s1 * v_num_s1);
		histvld_thrd = (tmp * lmtrat_minmax) >> 10;
		blackbar_mute_thrd = tmp >> 3;

		lc_reg = VPP_LC1_CURVE_HISTVLD_THRD + reg_offset;
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, histvld_thrd, vpp_index);

		lc_reg = VPP_LC1_CURVE_BB_MUTE_THRD + reg_offset;
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, blackbar_mute_thrd,
			vpp_index);

		tmp = ((height - 1) << 16) | (width - 1);
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS2_WIDTHM1_HEIGHTM1,
			tmp, vpp_index);

		/*tmp = (width / 12) & 0xff;*/
		tmp = width / 12 + width / 72 - 3;
		if (tmp > 0xff)
			tmp = 0xff;

		tmp = (0x1 << 3) | (tmp << 16);
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LC_STTS_CTRL1,
			tmp, vpp_index);

	} else {
		reg_offset = lc_reg_ofst[1];
		lc_reg = VPP_LC1_CURVE_CTRL + reg_offset;
		tmp = READ_VPP_REG_S5(lc_reg);
		tmp &= 0xfffffffe;
		pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
			__func__, lc_reg, tmp);
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, tmp, vpp_index);

		lc_reg = VPP_LC1_CURVE_RAM_CTRL + reg_offset;
		tmp = READ_VPP_REG_S5(lc_reg);
		tmp &= 0xfffffffe;
		pr_amve_v2("[%s]: 0x%04x = 0x%08x\n",
			__func__, lc_reg, tmp);
		VSYNC_WRITE_VPP_REG_VPP_SEL(lc_reg, tmp, vpp_index);
	}
}

void dump_lc_mapping_reg(void)
{
	int i;
	int slice = 0;
	int slice_max = 0;
	int ctrl_reg;
	int addr_reg;
	int data_reg;
	unsigned int ret_data, ret_data1;

	if (chip_type_id != chip_t3x)
		return;

	slice_max = get_slice_max();

	for (slice = SLICE0; slice < slice_max; slice++) {
		pr_info("########## slice[%d] curve data ###########\n",
			slice);

		ctrl_reg = VPP_SRSHARP1_LC_MAP_RAM_CTRL +
			sr_sharp_reg_ofst[slice];
		addr_reg = VPP_SRSHARP1_LC_MAP_RAM_ADDR +
			sr_sharp_reg_ofst[slice];
		data_reg = VPP_SRSHARP1_LC_MAP_RAM_DATA +
			sr_sharp_reg_ofst[slice];

		WRITE_VPP_REG_S5(ctrl_reg, 1);
		WRITE_VPP_REG_S5(addr_reg, 0 | (1 << 31));

		for (i = 0; i < 12 * 8; i++) {
			ret_data = READ_VPP_REG_S5(data_reg);
			ret_data1 = READ_VPP_REG_S5(data_reg);
			pr_info("[%d] %d/%d\n", i, ret_data, ret_data1);
		}

		WRITE_VPP_REG_S5(ctrl_reg, 0);
	}
}

void dump_lc_reg(void)
{
	int i;
	unsigned int tmp;
	int lc_reg;

	if (chip_type_id != chip_t3x)
		return;

	pr_info("multi_slice_case=%d\n", multi_slice_case);
	pr_info("hist_dma_case=%d\n", hist_dma_case);
	pr_info("lc_overlap_s0=%d\n", lc_overlap_s0);
	pr_info("lc_overlap=%d\n", lc_overlap);

	if (vd_info) {
		pr_info("vd_info slice_num=%d\n", vd_info->slice_num);
		pr_info("vd1_in_hsize=%d\n", vd_info->vd1_in_hsize);
		pr_info("vd1_in_vsize=%d\n", vd_info->vd1_in_vsize);
		pr_info("vd1_dout_hsize=%d\n", vd_info->vd1_dout_hsize);
		pr_info("vd1_dout_vsize=%d\n", vd_info->vd1_dout_vsize);
		pr_info("slice[0].hsize=%d\n",
			vd_info->slice[0].hsize);
		pr_info("slice[0].vsize=%d\n",
			vd_info->slice[0].vsize);
		pr_info("slice[0].scaler_in_hsize=%d\n",
			vd_info->slice[0].scaler_in_hsize);
		pr_info("slice[1].hsize=%d\n",
			vd_info->slice[1].hsize);
		pr_info("slice[1].vsize=%d\n",
			vd_info->slice[1].vsize);
		pr_info("slice[1].scaler_in_hsize=%d\n",
			vd_info->slice[1].scaler_in_hsize);
	}

	lc_reg = 0x5257;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x7757;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x282e;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	for (i = 0; i < 3; i++) {
		lc_reg = 0x5a40 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 3; i++) {
		lc_reg = 0x5a80 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		lc_reg = 0x5a56 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		lc_reg = 0x5a96 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 4; i++) {
		lc_reg = 0x5ad7 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		lc_reg = 0x52c0 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		lc_reg = 0x77c0 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 12; i++) {
		lc_reg = 0x52e2 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 12; i++) {
		lc_reg = 0x77e2 + i;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 44; i++) {
		lc_reg = 0x5ae9;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
		lc_reg = 0x5aea;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	for (i = 0; i < 96; i++) {
		lc_reg = 0x52fd;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
		lc_reg = 0x52fe;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
		lc_reg = 0x77fd;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
		lc_reg = 0x77fe;
		tmp = READ_VPP_REG_S5(lc_reg);
		pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
	}

	lc_reg = 0x2811;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3202;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3203;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3204;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3207;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3201;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x3241;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x2809;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x280a;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x280b;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x2808;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x2813;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);

	lc_reg = 0x28c5;
	tmp = READ_VPP_REG_S5(lc_reg);
	pr_info("[0x%04x]=0x%08x\n", lc_reg, tmp);
}

void monitor_lc_stts_overflow(void)
{
	unsigned int reg_ro = VPP_LC_STTS_DMA_ERROR_RO;
	unsigned int tmp;

	if (chip_type_id != chip_t3x)
		return;

	tmp = READ_VPP_REG_S5(reg_ro);
	if (tmp > 0) {
		pr_info("VPP_LC_STTS_DMA_ERROR_RO overflow!\n");
		pr_info("[0x%04x]=0x%08x\n", reg_ro, tmp);
	}
}

void clean_lc_stts_overflow(void)
{
	unsigned int reg_ro = VPP_LC_STTS_DMA_ERROR_RO;
	unsigned int tmp;

	if (chip_type_id != chip_t3x)
		return;

	WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL1, 1, 10, 1);
	tmp = READ_VPP_REG_S5(reg_ro);
	pr_info("[0x%04x]=0x%08x\n", reg_ro, tmp);
	pr_info("VPP_LC_STTS_CTRL1 clean error ro!\n");

	WRITE_VPP_REG_BITS_S5(VPP_LC_STTS_CTRL1, 0, 10, 1);
	tmp = READ_VPP_REG_S5(reg_ro);
	pr_info("[0x%04x]=0x%08x\n", reg_ro, tmp);
}

static void dump_convert_str(int num,
	int num_num, char cur_s[],
	int char_bit, int bit_chose)
{
	char buf[9] = {0};
	int i, count;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%x", num);

	count = strlen(buf);
	if (count > 4)
		count = 4;

	for (i = 0; i < count; i++)
		buf[i + char_bit] = buf[i];

	for (i = 0; i < char_bit; i++)
		buf[i] = '0';

	count = strlen(buf);
	for (i = 0; i < char_bit; i++)
		buf[i] = buf[count - char_bit + i];

	if (num_num > 0) {
		for (i = 0; i < char_bit; i++)
			cur_s[i + num_num * char_bit] = buf[i];
	} else {
		for (i = 0; i < char_bit; i++)
			cur_s[i] = buf[i];
	}
}

void ve_lc_rd_reg(int reg_sel, int data_type,
	char *buf, int slice_index)
{
	int i, j, tmp, tmp1, tmp2, len = 12;
	int lut_data[63] = {0};
	char *stemp = NULL;
	unsigned int reg;

	if (data_type == 1)
		goto dump_as_string;

	switch (reg_sel) {
	case SATUR_LUT:
		reg = VPP_SRSHARP1_LC_SAT_LUT_0_1 +
			sr_sharp_reg_ofst[slice_index];
		for (i = 0; i < 31; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0xfff;
			tmp2 = tmp & 0xfff;
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_satur_lut[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}

		reg = VPP_SRSHARP1_LC_SAT_LUT_62 +
			sr_sharp_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
		pr_info("reg_lc_satur_lut[62] =%4d.\n",
			tmp & 0xfff);
		break;
	case YMINVAL_LMT:
		reg = VPP_LC1_CURVE_YMINVAL_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}

		reg = VPP_LC1_CURVE_YMINVAL_LMT_12_13 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
			i++;
		}

		reg = VPP_LC1_CURVE_YMINVAL_LMT_14_15 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2 ; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_yminVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
			i++;
		}
		break;
	case YMAXVAL_LMT:
		reg = VPP_LC1_CURVE_YMAXVAL_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}

		reg = VPP_LC1_CURVE_YMAXVAL_LMT_12_13 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
			i++;
		}

		reg = VPP_LC1_CURVE_YMAXVAL_LMT_14_15 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ymaxVal_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
			i++;
		}
		break;
	case YPKBV_LMT:
		reg = VPP_LC1_CURVE_YPKBV_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 8; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
				2 * i, tmp1);
			pr_info("reg_lc_ypkBV_lmt[%d] =%4d.\n",
				2 * i + 1, tmp2);
		}
		break;
	case YPKBV_RAT:
		reg = VPP_LC1_CURVE_YPKBV_RAT + lc_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
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
		reg = VPP_LC1_CURVE_YPKBV_SLP_LMT + lc_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
		pr_info("reg_lc_ypkBV_slope_lmt[0] =%4d.\n",
			(tmp >> 8) & 0xff);
		pr_info("reg_lc_ypkBV_slope_lmt[1] =%4d.\n",
			tmp & 0xff);
		break;
	case CNTST_LMT:
		reg = VPP_LC1_CURVE_CONTRAST_LMT_LH + lc_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
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
		reg = VPP_SRSHARP1_LC_SAT_LUT_0_1 +
			sr_sharp_reg_ofst[slice_index];
		for (i = 0; i < 31; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0xfff;
			tmp2 = tmp & 0xfff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}

		reg = VPP_SRSHARP1_LC_SAT_LUT_62 +
			sr_sharp_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
		lut_data[62] = tmp & 0xfff;

		for (i = 0; i < 63; i++)
			dump_convert_str(lut_data[i], i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMINVAL_LMT:
		reg = VPP_LC1_CURVE_YMINVAL_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}

		reg = VPP_LC1_CURVE_YMINVAL_LMT_12_13 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
			i++;
		}

		reg = VPP_LC1_CURVE_YMINVAL_LMT_14_15 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
			i++;
		}

		len = 16;
		for (i = 0; i < len; i++)
			dump_convert_str(lut_data[i], i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YMAXVAL_LMT:
		reg = VPP_LC1_CURVE_YMAXVAL_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 6; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}

		reg = VPP_LC1_CURVE_YMAXVAL_LMT_12_13 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
			i++;
		}

		reg = VPP_LC1_CURVE_YMAXVAL_LMT_14_15 + lc_reg_ofst[slice_index];
		for (j = 0; j < 2; j++) {
			tmp = READ_VPP_REG(reg + j);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
			i++;
		}

		len = 16;
		for (i = 0; i < len; i++)
			dump_convert_str(lut_data[i], i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_LMT:
		reg = VPP_LC1_CURVE_YPKBV_LMT_0_1 + lc_reg_ofst[slice_index];
		for (i = 0; i < 8; i++) {
			tmp = READ_VPP_REG(reg + i);
			tmp1 = (tmp >> 16) & 0x3ff;
			tmp2 = tmp & 0x3ff;
			lut_data[2 * i] = tmp1;
			lut_data[2 * i + 1] = tmp2;
		}

		for (i = 0; i < 16; i++)
			dump_convert_str(lut_data[i], i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	case YPKBV_RAT:
		reg = VPP_LC1_CURVE_YPKBV_RAT + lc_reg_ofst[slice_index];
		tmp = READ_VPP_REG(reg);
		lut_data[0] = (tmp >> 24) & 0xff;
		lut_data[1] = (tmp >> 16) & 0xff;
		lut_data[2] = (tmp >> 8) & 0xff;
		lut_data[3] = tmp & 0xff;

		for (i = 0; i < 4; i++)
			dump_convert_str(lut_data[i], i, stemp, 4, 10);
		memcpy(buf, stemp, 300);
		break;
	default:
		break;
	}

	kfree(stemp);
}

void dump_dnlp_reg(void)
{
	unsigned int tmp;
	int dnlp_reg;
	int i;

	for (i = 0; i < 7; i++) {
		dnlp_reg = VI_HIST_CTRL + i;
		tmp = READ_VPP_REG_S5(dnlp_reg);
		pr_info("[0x%04x]=0x%08x\n", dnlp_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		dnlp_reg = VI_HIST_PIC_SIZE + i;
		tmp = READ_VPP_REG_S5(dnlp_reg);
		pr_info("[0x%04x]=0x%08x\n", dnlp_reg, tmp);
	}

	/*slice1*/
	for (i = 0; i < 7; i++) {
		dnlp_reg = VI_HIST_CTRL + 0x30 + i;
		tmp = READ_VPP_REG_S5(dnlp_reg);
		pr_info("[0x%04x]=0x%08x\n", dnlp_reg, tmp);
	}

	for (i = 0; i < 2; i++) {
		dnlp_reg = VI_HIST_PIC_SIZE + 0x30 + i;
		tmp = READ_VPP_REG_S5(dnlp_reg);
		pr_info("[0x%04x]=0x%08x\n", dnlp_reg, tmp);
	}
}

void post_lut3d_ctl(enum wr_md_e mode, int en, int vpp_index)
{
	int i;
	int slice_max;
	unsigned int tmp;

	slice_max = get_slice_max();

	if (mode == WR_VCB) {
		for (i = SLICE0; i < slice_max; i++) {
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL +
				pst_reg_ofst[i], 0);
			tmp = READ_VPP_REG(VPP_LUT3D_CTRL +
				pst_reg_ofst[i]);
			tmp = (tmp & 0xffffff8e) | (en & 0x1) | (0x7 << 4);
			WRITE_VPP_REG(VPP_LUT3D_CTRL +
				pst_reg_ofst[i], tmp);
		}
	} else if (mode == WR_DMA) {
		for (i = SLICE0; i < slice_max; i++) {
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL +
				pst_reg_ofst[i], 0);
			tmp = READ_VPP_REG(VPP_LUT3D_CTRL +
				pst_reg_ofst[i]);
			tmp = (tmp & 0xffffff8e) | (en & 0x1) | (0x7 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_CTRL +
				pst_reg_ofst[i], tmp, vpp_index);
		}
	}

	pr_amve_v2("lut3d_ctl: en = %d\n", en);
}

void post_lut3d_update(unsigned int *lut3d_data, int vpp_index)
{
	int i;
	int j;
	int slice_max;
	unsigned int reg_ram_ctrl;
	unsigned int reg_addr;
	unsigned int reg_data;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		reg_ram_ctrl = VPP_LUT3D_CBUS2RAM_CTRL + pst_reg_ofst[i];
		reg_addr = VPP_LUT3D_RAM_ADDR + pst_reg_ofst[i];
		reg_data = VPP_LUT3D_RAM_DATA + pst_reg_ofst[i];

		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ram_ctrl, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_addr, 0 | (0 << 31), vpp_index);

		for (j = 0; j < 17 * 17 * 17; j++) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_data,
				((lut3d_data[j * 3 + 1] & 0xfff) << 16) |
				(lut3d_data[j * 3 + 2] & 0xfff), vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_data,
				(lut3d_data[j * 3 + 0] & 0xfff), vpp_index); /*MSB*/
			if (vev2_dbg == 17 && (j < 17 * 17))
				pr_info("%d: %03x %03x %03x\n",
					j,
					lut3d_data[i * 3 + 0],
					lut3d_data[i * 3 + 1],
					lut3d_data[i * 3 + 2]);
		}

		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ram_ctrl, 0, vpp_index);
	}
}

void post_lut3d_set(unsigned int *lut3d_data)
{
	int i;
	int j;
	int slice_max;
	unsigned int reg_ctrl;
	unsigned int reg_ram_ctrl;
	unsigned int reg_addr;
	unsigned int reg_data;
	unsigned int tmp;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		reg_ctrl = VPP_LUT3D_CTRL + pst_reg_ofst[i];
		reg_ram_ctrl = VPP_LUT3D_CBUS2RAM_CTRL + pst_reg_ofst[i];
		reg_addr = VPP_LUT3D_RAM_ADDR + pst_reg_ofst[i];
		reg_data = VPP_LUT3D_RAM_DATA + pst_reg_ofst[i];

		tmp = READ_VPP_REG(reg_ctrl);
		WRITE_VPP_REG(reg_ctrl, tmp & 0xfffffffe);
		usleep_range(16000, 16001);

		WRITE_VPP_REG(reg_ram_ctrl, 1);
		WRITE_VPP_REG(reg_addr, 0 | (0 << 31));

		for (j = 0; j < 17 * 17 * 17; j++) {
			WRITE_VPP_REG(reg_data,
				((lut3d_data[j * 3 + 1] & 0xfff) << 16) |
				(lut3d_data[j * 3 + 2] & 0xfff));
			WRITE_VPP_REG(reg_data,
				(lut3d_data[j * 3 + 0] & 0xfff)); /*MSB*/
			if (vev2_dbg == 17 && (j < 17 * 17))
				pr_info("%d: %03x %03x %03x\n",
					j,
					lut3d_data[i * 3 + 0],
					lut3d_data[i * 3 + 1],
					lut3d_data[i * 3 + 2]);
		}

		WRITE_VPP_REG(reg_ram_ctrl, 0);
		WRITE_VPP_REG(reg_ctrl, tmp);
	}
}

void post_lut3d_section_write(int index, int section_len,
	unsigned int *lut3d_data_in)
{
	int i;
	int j;
	int slice_max;
	unsigned int reg_ctrl;
	unsigned int reg_ram_ctrl;
	unsigned int reg_addr;
	unsigned int reg_data;
	unsigned int tmp;
	int r_offset, g_offset;

	index = index * section_len / 17;

	g_offset = index % 17;
	r_offset = index / 17;

	slice_max = get_slice_max();

	for (i = SLICE0; i < slice_max; i++) {
		reg_ctrl = VPP_LUT3D_CTRL + pst_reg_ofst[i];
		reg_ram_ctrl = VPP_LUT3D_CBUS2RAM_CTRL + pst_reg_ofst[i];
		reg_addr = VPP_LUT3D_RAM_ADDR + pst_reg_ofst[i];
		reg_data = VPP_LUT3D_RAM_DATA + pst_reg_ofst[i];

		tmp = READ_VPP_REG(reg_ctrl);
		WRITE_VPP_REG(reg_ctrl, tmp & 0xfffffffe);
		usleep_range(16000, 16001);

		WRITE_VPP_REG(reg_ram_ctrl, 1);
		/*bit20:16 R, bit12:8 G, bit4:0 B*/
		WRITE_VPP_REG(reg_addr,
			(r_offset << 16) | (g_offset << 8) | (0 << 31));

		for (j = 0; j < section_len; j++) {
			WRITE_VPP_REG(reg_data,
				((lut3d_data_in[j * 3 + 1] & 0xfff) << 16) |
				(lut3d_data_in[j * 3 + 2] & 0xfff));
			WRITE_VPP_REG(reg_data,
				(lut3d_data_in[j * 3 + 0] & 0xfff)); /*MSB*/
		}

		WRITE_VPP_REG(reg_ram_ctrl, 0);
		WRITE_VPP_REG(reg_ctrl, tmp);
	}
}

void post_lut3d_section_read(int index, int section_len,
	unsigned int *lut3d_data_out)
{
	int i;
	unsigned int reg_ram_ctrl;
	unsigned int reg_addr;
	unsigned int reg_data;
	unsigned int tmp;
	int r_offset, g_offset;

	index = index * section_len / 17;

	g_offset = index % 17;
	r_offset = index / 17;

	reg_ram_ctrl = VPP_LUT3D_CBUS2RAM_CTRL;
	reg_addr = VPP_LUT3D_RAM_ADDR;
	reg_data = VPP_LUT3D_RAM_DATA;

	WRITE_VPP_REG(reg_ram_ctrl, 1);
	/*bit20:16 R, bit12:8 G, bit4:0 B*/
	WRITE_VPP_REG(reg_addr,
		(r_offset << 16) | (g_offset << 8) | (1 << 31));

	for (i = 0; i < section_len; i++) {
		tmp = READ_VPP_REG(reg_data);
		lut3d_data_out[i * 3 + 2] = tmp & 0xfff;
		lut3d_data_out[i * 3 + 1] = (tmp >> 16) & 0xfff;
		tmp = READ_VPP_REG(reg_data);
		lut3d_data_out[i * 3 + 0] = tmp & 0xfff;
	}

	WRITE_VPP_REG(reg_ram_ctrl, 0);
}
#endif
