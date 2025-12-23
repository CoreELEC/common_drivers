// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/amlogic/media/amprime_sl/prime_sl.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#include "amprime_sl.h"
#include "amprime_sl_regs_t6w.h"
#include "amprime_sl_regs_t6x.h"

/*======================================*/
/*#define USE_TASKLET	1*/
static inline void wbits_PRIMESL_CTRL0(unsigned int primesl_en,
		unsigned int gclk_ctrl, unsigned int reg_gclk_ctrl,
		unsigned int inv_y_ratio, unsigned int inv_chroma_ratio,
		unsigned int clip_en, unsigned int legacy_mode_en)
{
	union PRIMESL_CTRL0_BITS v;

	v.b.primesl_en = primesl_en;
	v.b.gclk_ctrl = gclk_ctrl;
	v.b.reg_gclk_ctrl = reg_gclk_ctrl;
	v.b.inv_y_ratio = inv_y_ratio;
	v.b.inv_chroma_ratio = inv_chroma_ratio;
	v.b.clip_en = clip_en;
	v.b.legacy_mode_en = legacy_mode_en;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL0, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL0, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL0, v.d32);
}

static inline void wbits_PRIMESL_CTRL1(unsigned int footroom,
		unsigned int l_headroom)
{
	union PRIMESL_CTRL1_BITS v;

	v.b.footroom = footroom;
	v.b.l_headroom = l_headroom;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL1, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL1, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL1, v.d32);
}

static inline void wbits_PRIMESL_CTRL2(unsigned int c_headroom)
{
	union PRIMESL_CTRL2_BITS v;

	v.b.c_headroom = c_headroom;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL2, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL2, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL2, v.d32);
}

static inline void wbits_PRIMESL_CTRL3(unsigned int mua,
		unsigned int mub)
{
	union PRIMESL_CTRL3_BITS v;

	v.b.mua = mua;
	v.b.mub = mub;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL3, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL3, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL3, v.d32);
}

static inline void wbits_PRIMESL_CTRL4(unsigned int oct_7_0,
		unsigned int oct_7_1)
{
	union PRIMESL_CTRL4_BITS v;

	v.b.oct_7_0 = oct_7_0;
	v.b.oct_7_1 = oct_7_1;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL4, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL4, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL4, v.d32);
}

static inline void wbits_PRIMESL_CTRL5(unsigned int oct_7_2,
		unsigned int oct_7_3)
{
	union PRIMESL_CTRL5_BITS v;

	v.b.oct_7_2 = oct_7_2;
	v.b.oct_7_3 = oct_7_3;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL5, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL5, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL5, v.d32);
}

static inline void wbits_PRIMESL_CTRL6(unsigned int oct_7_4,
		unsigned int oct_7_5)
{
	union PRIMESL_CTRL6_BITS v;

	v.b.oct_7_4 = oct_7_4;
	v.b.oct_7_5 = oct_7_5;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL6, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL6, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL6, v.d32);
}

static inline void wbits_PRIMESL_CTRL7(unsigned int oct_7_6)
{
	union PRIMESL_CTRL7_BITS v;

	v.b.oct_7_6 = oct_7_6;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL7, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL7, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL7, v.d32);
}

static inline void wbits_PRIMESL_CTRL8(unsigned int d_lut_threshold_3_0,
		unsigned int d_lut_threshold_3_1)
{
	union PRIMESL_CTRL8_BITS v;

	v.b.d_lut_threshold_3_0 = d_lut_threshold_3_0;
	v.b.d_lut_threshold_3_1 = d_lut_threshold_3_1;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL8, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL8, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL8, v.d32);
}

static inline void wbits_PRIMESL_CTRL9(unsigned int d_lut_threshold_3_2)
{
	union PRIMESL_CTRL9_BITS v;

	v.b.d_lut_threshold_3_2 = d_lut_threshold_3_2;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL9, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL9, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL9, v.d32);
}

static inline void wbits_PRIMESL_CTRL10(unsigned int d_lut_step_4_0,
		unsigned int d_lut_step_4_1,
		unsigned int d_lut_step_4_2,
		unsigned int d_lut_step_4_3)
{
	union PRIMESL_CTRL10_BITS v;

	v.b.d_lut_step_4_0 = d_lut_step_4_0;
	v.b.d_lut_step_4_1 = d_lut_step_4_1;
	v.b.d_lut_step_4_2 = d_lut_step_4_2;
	v.b.d_lut_step_4_3 = d_lut_step_4_3;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL10, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL10, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL10, v.d32);
}

static inline void wbits_PRIMESL_CTRL11(unsigned int rgb2yuv_9_1,
		unsigned int rgb2yuv_9_0)
{
	union PRIMESL_CTRL11_BITS v;

	v.b.rgb2yuv_9_1 = rgb2yuv_9_1;
	v.b.rgb2yuv_9_0 = rgb2yuv_9_0;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL11, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL11, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL11, v.d32);
}

static inline void wbits_PRIMESL_CTRL12(unsigned int rgb2yuv_9_3,
		unsigned int rgb2yuv_9_2)
{
	union PRIMESL_CTRL12_BITS v;

	v.b.rgb2yuv_9_3 = rgb2yuv_9_3;
	v.b.rgb2yuv_9_2 = rgb2yuv_9_2;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL12, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL12, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL12, v.d32);
}

static inline void wbits_PRIMESL_CTRL13(unsigned int rgb2yuv_9_5,
		unsigned int rgb2yuv_9_4)
{
	union PRIMESL_CTRL13_BITS v;

	v.b.rgb2yuv_9_5 = rgb2yuv_9_5;
	v.b.rgb2yuv_9_4 = rgb2yuv_9_4;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL13, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL13, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL13, v.d32);
}

static inline void wbits_PRIMESL_CTRL14(unsigned int rgb2yuv_9_7,
		unsigned int rgb2yuv_9_6)
{
	union PRIMESL_CTRL14_BITS v;

	v.b.rgb2yuv_9_7 = rgb2yuv_9_7;
	v.b.rgb2yuv_9_6 = rgb2yuv_9_6;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL14, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL14, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL14, v.d32);
}

static inline void wbits_PRIMESL_CTRL15(unsigned int rgb2yuv_9_8)
{
	union PRIMESL_CTRL15_BITS v;

	v.b.rgb2yuv_9_8 = rgb2yuv_9_8;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL15, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL15, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL15, v.d32);
}

static inline void wbits_PRIMESL_CTRL16(unsigned int reg_s,
		unsigned int rgb_rs, unsigned int rgb_clip, unsigned int rgb_shift,
		unsigned int uv_shift, unsigned int rgb_swap, unsigned int byp_mat,
		unsigned int byp_d_lut, unsigned int byp_s_lut)
{
	union PRIMESL_CTRL16_BITS v;

	v.b.reg_s = reg_s;
	v.b.rgb_rs = rgb_rs;
	v.b.rgb_clip = rgb_clip;
	v.b.rgb_shift = rgb_shift;
	v.b.uv_shift = uv_shift;
	v.b.rgb_swap = rgb_swap;
	v.b.byp_mat = byp_mat;
	v.b.byp_d_lut = byp_d_lut;
	v.b.byp_s_lut = byp_s_lut;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_CTRL16, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_CTRL16, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_CTRL16, v.d32);
}

static inline void wbits_PRIMESL_OMAT_OFFSET0(unsigned int pre_offset1,
		unsigned int pre_offset0)
{
	union PRIMESL_OMAT_OFFSET0_BITS v;

	v.b.pre_offset1 = pre_offset1;
	v.b.pre_offset0 = pre_offset0;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_OMAT_OFFSET0, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_OMAT_OFFSET0, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_OMAT_OFFSET0, v.d32);
}

static inline void wbits_PRIMESL_OMAT_OFFSET1(unsigned int offset0,
		unsigned int pre_offset2)
{
	union PRIMESL_OMAT_OFFSET1_BITS v;

	v.b.offset0 = offset0;
	v.b.pre_offset2 = pre_offset2;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_OMAT_OFFSET1, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_OMAT_OFFSET1, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_OMAT_OFFSET1, v.d32);
}

static inline void wbits_PRIMESL_OMAT_OFFSET2(unsigned int offset2,
		unsigned int offset1)
{
	union PRIMESL_OMAT_OFFSET2_BITS v;

	v.b.offset2 = offset2;
	v.b.offset1 = offset1;
	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w())
		SL_VSYNC_WR_MPEG_REG(PRIMESL_OMAT_OFFSET2, v.d32);
	else if (is_meson_t6w())
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_OMAT_OFFSET2, v.d32);
	else if (is_meson_t6x())
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_OMAT_OFFSET2, v.d32);
}

#define NOT_USR_WR_T	1

/* 9byte */
static void slctr_set_rgb2yuv(const int *ptab)
{
#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL11(ptab[1], ptab[0]);
	wbits_PRIMESL_CTRL12(ptab[3], ptab[2]);
	wbits_PRIMESL_CTRL13(ptab[5], ptab[4]);
	wbits_PRIMESL_CTRL14(ptab[7], ptab[6]);
	wbits_PRIMESL_CTRL15(ptab[8]);
#else
	int i;

	for (i = 0; i < 9; i++)
		WR_T(PRIME_RGB2YUV_9_0 + i, (unsigned int)ptab[i]);
#endif
}

/* 4byte */
static void slctr_set_lutstep(const unsigned int *ptab)
{
#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL10(ptab[0], ptab[1], ptab[2], ptab[3]);
#else
	int i;

	for (i = 0; i < 4; i++)
		WR_T(PRIME_D_LUT_STEP_4_0 + i, ptab[i]);
#endif
}

/* 3byte */
static void slctr_set_lutthrd(const unsigned int *ptab)
{
#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL8(ptab[0], ptab[1]);
	wbits_PRIMESL_CTRL9(ptab[2]);
#else
	int i;

	for (i = 0; i < 3; i++)
		WR_T(PRIME_D_LUT_THRESHOLD_3_0 + i, ptab[i]);
#endif
}

/* 7byte */
static void slctr_set_oct(const int *ptab)
{
#ifdef NOT_USR_WR_T
	wbits_PRIMESL_CTRL4(ptab[0], ptab[1]);
	wbits_PRIMESL_CTRL5(ptab[2], ptab[3]);
	wbits_PRIMESL_CTRL6(ptab[4], ptab[5]);
	wbits_PRIMESL_CTRL7(ptab[6]);
#else
	int i;

	for (i = 0; i < 7; i++)
		WR_T(PRIME_OCT_7_0 + i, (unsigned int)ptab[i]);
#endif
}

/* 7byte */
static void slctr_set_lut_c(const int *ptab)
{
	int i;

	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w()) {
		SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTC_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTC_DATA_PORT, ptab[i]);
	} else if (is_meson_t6w()) {
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTC_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTC_DATA_PORT, ptab[i]);
	} else if (is_meson_t6x()) {
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTC_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTC_DATA_PORT, ptab[i]);
	}
}

/* 7byte */
static void slctr_set_lut_p(const int *ptab)
{
	int i;

	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w()) {
		SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTP_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTP_DATA_PORT, ptab[i]);
	} else if (is_meson_t6w()) {
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTP_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTP_DATA_PORT, ptab[i]);
	} else if (is_meson_t6x()) {
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTP_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTP_DATA_PORT, ptab[i]);
	}
}

/* 7byte */
static void slctr_set_lut_d(const int *ptab)
{
	int i;

	if (is_prime_sl_stb_mode() || is_meson_t5m() || is_meson_t5w()) {
		SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTD_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(PRIMESL_LUTD_DATA_PORT, ptab[i]);
	} else if (is_meson_t6w()) {
		SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTD_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6W_PRIMESL_LUTD_DATA_PORT, ptab[i]);
	} else if (is_meson_t6x()) {
		SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTD_ADDR_PORT, 0);
		for (i = 0; i < 65; i++)
			SL_VSYNC_WR_MPEG_REG(T6X_PRIMESL_LUTD_DATA_PORT, ptab[i]);
	}
}

void prime_sl_set_reg(const struct prime_sl_t *ps)
{
	wbits_PRIMESL_CTRL1(ps->footroom, ps->l_headroom);
	wbits_PRIMESL_CTRL2(ps->c_headroom);
	wbits_PRIMESL_CTRL3(ps->mua, ps->mub);
	slctr_set_oct(&ps->oct[0]);
	slctr_set_lutthrd(&ps->d_lut_threshold[0]);
	slctr_set_lutstep(&ps->d_lut_step[0]);
	slctr_set_rgb2yuv(&ps->rgb2yuv[0]);
	slctr_set_lut_c(&ps->lut_c[0]);
	slctr_set_lut_p(&ps->lut_p[0]);
	slctr_set_lut_d(&ps->lut_d[0]);
	wbits_PRIMESL_CTRL16(1024, 0, 3,
			0, 0, 0,
			0, 0, 0);
	wbits_PRIMESL_CTRL0(1, 0, 0, ps->inv_y_ratio,
			ps->inv_chroma_ratio, 0, 0);
	if (is_meson_tm2() || is_meson_sc2() || is_meson_s7d() || is_meson_s6()) {
		wbits_PRIMESL_OMAT_OFFSET0(512, 512);
		wbits_PRIMESL_OMAT_OFFSET1(256, 512);
		wbits_PRIMESL_OMAT_OFFSET2(2048, 2048);

		u32 data32;
		/* path select */
		data32 = VSYNC_RD_TABLE_REG(VIDEO_PARTITION_TABLE, AMDV_PATH_CTRL);
		data32 &= ~(3 << 24);
		data32 &= ~(1 << 22);
		data32 &= ~(1 << 18);
		data32 &= ~(1 << 12);
		data32 &= ~(3 << 8);
		data32 &= ~(1 << 0);
		data32 |= 2 << 24;
		data32 |= 2 << 8;
		WRITE_VCBUS_REG(AMDV_PATH_CTRL, data32);
		pr_sl(1, "stb prime path reg set\n");
	} else if (is_meson_a9()) {
		wbits_PRIMESL_OMAT_OFFSET0(512, 512);
		wbits_PRIMESL_OMAT_OFFSET1(256, 512);
		wbits_PRIMESL_OMAT_OFFSET2(2048, 2048);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL1, 0x2002);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL2, 0x0);
		SL_VSYNC_WR_MPEG_REG(VIU_VD1_PATH_CTRL, 0x0);
	} else if (is_meson_t5m()) {
		wbits_PRIMESL_OMAT_OFFSET0(2048, 2048);
		wbits_PRIMESL_OMAT_OFFSET1(64, 2048);
		wbits_PRIMESL_OMAT_OFFSET2(512, 512);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL1, 0x2002);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL2, 0x0);
		SL_VSYNC_WR_MPEG_REG(VIU_VD1_PATH_CTRL, 0x0);
		pr_sl(1, "t5m prime path reg set\n");
	} else if (is_meson_t6w()) {
		wbits_PRIMESL_OMAT_OFFSET0(2048, 2048);
		wbits_PRIMESL_OMAT_OFFSET1(64, 2048);
		wbits_PRIMESL_OMAT_OFFSET2(512, 512);
		pr_sl(1, "t6w prime path reg set\n");
	} else if (is_meson_t6x()) {
		wbits_PRIMESL_OMAT_OFFSET0(2048, 2048);
		wbits_PRIMESL_OMAT_OFFSET1(64, 2048);
		wbits_PRIMESL_OMAT_OFFSET2(512, 512);
		pr_sl(1, "t6x prime path reg set\n");
	}
}

void prime_sl_close(void)
{
	wbits_PRIMESL_CTRL0(0, 0, 0, 0, 0, 0, 0);
	if (is_meson_tm2() || is_meson_sc2() || is_meson_s7d() || is_meson_s6()) {
		u32 data32;

		/* path select */
		data32 = VSYNC_RD_TABLE_REG(VIDEO_PARTITION_TABLE, AMDV_PATH_CTRL);
		data32 &= ~(3 << 24);
		data32 &= ~(3 << 8);
		data32 |= 1 << 0;
		VSYNC_WR_TABLE_REG(VIDEO_PARTITION_TABLE, AMDV_PATH_CTRL, data32);
		pr_sl(1, "stb prime reg close\n");
	} else if (is_meson_t5m()) {
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL1, 0x0);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL2, 0x0);
		SL_VSYNC_WR_MPEG_REG(VIU_VD1_PATH_CTRL, 0x10000);
		pr_sl(1, "t5m prime reg close\n");
	} else if (is_meson_t6w()) {
		pr_sl(1, "t6w prime reg close\n");
	} else if (is_meson_t6x()) {
		pr_sl(1, "t6x prime reg close\n");
	} else if (is_meson_t5m() || is_meson_a9()) {
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL1, 0x0);
		SL_VSYNC_WR_MPEG_REG(AMDV_PATH_SWAP_CTRL2, 0x0);
		SL_VSYNC_WR_MPEG_REG(VIU_VD1_PATH_CTRL, 0x10000);
	}
}
