// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_aa_pps.h"
#ifdef RUN_ON_ARM
#include <linux/math64.h>
#endif

unsigned int test_v_tap = 4;
module_param_named(test_v_tap, test_v_tap, uint, 0664);

unsigned int test_h_tap = 4;
module_param_named(test_h_tap, test_h_tap, uint, 0664);

static const int pps_lut_tap2[33][2] = {{128, 0},
	{127, 1},
	{126, 2},
	{124, 4},
	{123, 5},
	{122, 6},
	{120, 8},
	{118, 10},
	{116, 12},
	{114, 14},
	{112, 16},
	{110, 18},
	{108, 20},
	{106, 22},
	{104, 24},
	{101, 27},
	{99, 29},
	{97, 31},
	{94, 34},
	{91, 37},
	{89, 39},
	{86, 42},
	{84, 44},
	{81, 47},
	{78, 50},
	{75, 53},
	{73, 55},
	{70, 58},
	{68, 60},
	{67, 61},
	{66, 62},
	{65, 63},
	{64, 64}
};

static const int pps_lut_tap4[33][4] = {{0, 128,  0,  0},
	{ 0, 127, 1, 0},
	{-1, 127, 2, 0},
	{-3, 127, 4, 0},
	{-4, 127, 5, 0},
	{-4, 126, 6, 0},
	{-5, 126, 8, -1},
	{-6, 125, 10, -1},
	{-6, 123, 12, -1},
	{-7, 122, 14, -1},
	{-7, 120, 16, -1},
	{-8, 120, 18, -2},
	{-8, 118, 20, -2},
	{-8, 116, 22, -2},
	{-9, 115, 24, -2},
	{-9, 113, 27, -3},
	{-9, 111, 29, -3},
	{-9, 109, 31, -3},
	{-9, 107, 34, -4},
	{-9, 104, 37, -4},
	{-9, 102, 39, -4},
	{-9, 100, 42, -5},
	{-9,  98, 44, -5},
	{-9,  95, 47, -5},
	{-9,  93, 50, -6},
	{-9,  90, 53, -6},
	{-9,  88, 55, -6},
	{-9,  86, 58, -7},
	{-9,  83, 61, -7},
	{-9,  80, 64, -7},
	{-8,  77, 66, -7},
	{-8,  75, 69, -8},
	{-8,  72, 72, -8}
};

static const int pps_lut_tap8[33][8] = { {0,    0,   0, 128, 0, 0, 0, 0},
	{-1, 1, 0, 127, 2, -1, 1, -1},
	{-1, 2, -2, 127, 4, -2, 1, -1},
	{-2, 3, -4, 127, 6, -3, 2, -1},
	{-3, 4, -7, 127, 10, -4, 3, -2},
	{-3, 5, -9, 127, 12, -5, 3, -2},
	{-4, 6, -11, 127, 15, -6, 4, -3},
	{-4, 7, -13, 127, 16, -7, 5, -3},
	{-5, 7, -14, 127, 20, -8, 5, -4},
	{-5, 8, -16, 127, 21, -9, 6, -4},
	{-6, 9, -17, 127, 24, -11, 7, -5},
	{-6, 10, -18, 126, 26, -12, 7, -5},
	{-7, 10, -20, 127, 29, -13, 8, -6},
	{-7, 11, -21, 124, 32, -14, 9, -6},
	{-8, 12, -22, 124, 35, -15, 9, -7},
	{-8, 12, -23, 123, 37, -16, 10, -7},
	{-9, 13, -24, 121, 40, -17, 11, -7},
	{-9, 14, -25, 120, 43, -18, 11, -8},
	{-10, 14, -26, 119, 46, -19, 12, -8},
	{-10, 15, -27, 118, 49, -20, 12, -9},
	{-10, 15, -27, 115, 52, -21, 13, -9},
	{-10, 15, -28, 114, 55, -22, 13, -9},
	{-11, 16, -28, 112, 58, -23, 14, -10},
	{-11, 16, -29, 111, 61, -24, 14, -10},
	{-11, 16, -29, 107, 64, -24, 15, -10},
	{-11, 17, -29, 104, 67, -25, 15, -10},
	{-11, 17, -29, 103, 70, -26, 15, -11},
	{-12, 17, -29, 100, 73, -26, 16, -11},
	{-12, 17, -29,  98, 76, -27, 16, -11},
	{-12, 17, -29,  96, 79, -28, 16, -11},
	{-12, 17, -29,  92, 82, -28, 17, -11},
	{-12, 17, -29,  90, 85, -29, 17, -11},
	{-12, 17, -29,  88, 88, -29, 17, -12}
};

void set_aa_pps_coef(u32 scale_coef_idx,
	u32 scale_coef, int hor_tap_num, int ver_tap_num)
{
	int i;

	dbg_h2("%s:%d, v:%d\n", __func__, hor_tap_num, ver_tap_num);
//=================================== postsc coef lut =========================
//ver
	if (ver_tap_num == 8) {
		w_reg_bit(scale_coef_idx, 0x000, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap8[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap8[i][0] & 0xff) << 16) |
			(((pps_lut_tap8[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap8[i][1] & 0xff) << 0));

		//dbg_h2(" 4tap = %d\n",
		//((pps_lut_tap4_s11_default[i][0]>>8)&0xff) );
			wr(scale_coef, (((pps_lut_tap8[i][2] >> 8) &
							0xff) << 24) |
				((pps_lut_tap8[i][2] & 0xff) << 16) |
				(((pps_lut_tap8[i][3] >> 8) & 0xff) << 8) |
				((pps_lut_tap8[i][3] & 0xff) << 0));
		}

		w_reg_bit(scale_coef_idx, 0x400, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap8[i][4] >> 8) &
						0xff) << 24) |
				((pps_lut_tap8[i][4] & 0xff) << 16) |
				(((pps_lut_tap8[i][5] >> 8) & 0xff) << 8) |
				((pps_lut_tap8[i][5] & 0xff) << 0));
			wr(scale_coef, (((pps_lut_tap8[i][6] >> 8) & 0xff) << 24) |
				((pps_lut_tap8[i][6] & 0xff) << 16) |
				(((pps_lut_tap8[i][7] >> 8) & 0xff) << 8) |
				((pps_lut_tap8[i][7] & 0xff) << 0));
		}
	} else if (ver_tap_num == 4) {
		w_reg_bit(scale_coef_idx, 0x000, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap4[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap4[i][0] & 0xff) << 16) |
			(((pps_lut_tap4[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap4[i][1] & 0xff) << 0));
			//dbg_h2("4tap = %d\n",
			//((pps_lut_tap4_s11_default[i][0]>>8)&0xff));
			wr(scale_coef, (((pps_lut_tap4[i][2] >> 8) &
						0xff) << 24) |
			((pps_lut_tap4[i][2] & 0xff) << 16) |
			(((pps_lut_tap4[i][3] >> 8) & 0xff) << 8)  |
			((pps_lut_tap4[i][3] & 0xff) << 0));
		}
	} else {
		w_reg_bit(scale_coef_idx, 0x000, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap2[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap2[i][0] & 0xff) << 16) |
			(((pps_lut_tap2[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap2[i][1] & 0xff) << 0));
			wr(scale_coef, (((pps_lut_tap2[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap2[i][0] & 0xff) << 16) |
			(((pps_lut_tap2[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap2[i][1] & 0xff) << 0));
		}
	}

	//hor
	if (hor_tap_num == 8) {
		w_reg_bit(scale_coef_idx, 0x800, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap8[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap8[i][0] & 0xff) << 16) |
			(((pps_lut_tap8[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap8[i][1] & 0xff) << 0));
	//dbg_h2(" 4tap = %d\n",((pps_lut_tap4_s11_default[i][0]>>8)&0xff) );
			wr(scale_coef, (((pps_lut_tap8[i][2] >> 8) &
						0xff) << 24) |
			((pps_lut_tap8[i][2] & 0xff) << 16) |
			(((pps_lut_tap8[i][3] >> 8) & 0xff) << 8) |
			((pps_lut_tap8[i][3] & 0xff) << 0));
		}

		w_reg_bit(scale_coef_idx, 0xc00, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap8[i][4] >> 8) &
						0xff) << 24) |
			((pps_lut_tap8[i][4] & 0xff) << 16) |
			(((pps_lut_tap8[i][5] >> 8) & 0xff) << 8) |
			((pps_lut_tap8[i][5] & 0xff) << 0));
			wr(scale_coef, (((pps_lut_tap8[i][6] >> 8) &
						0xff) << 24) |
			((pps_lut_tap8[i][6] & 0xff) << 16) |
			(((pps_lut_tap8[i][7] >> 8) & 0xff) << 8) |
			((pps_lut_tap8[i][7] & 0xff) << 0));
		}
	} else if (hor_tap_num == 4) {
		w_reg_bit(scale_coef_idx, 0x800, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap4[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap4[i][0] & 0xff) << 16) |
			(((pps_lut_tap4[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap4[i][1] & 0xff) << 0));
			//dbg_h2(" 4tap = %d\n",
			//((pps_lut_tap4_s11_default[i][0]>>8)&0xff) );
			wr(scale_coef, (((pps_lut_tap4[i][2] >> 8) &
						0xff) << 24) |
			((pps_lut_tap4[i][2] & 0xff) << 16) |
			(((pps_lut_tap4[i][3] >> 8) & 0xff) << 8) |
			((pps_lut_tap4[i][3] & 0xff) << 0));
		}
	} else {
		w_reg_bit(scale_coef_idx, 0x800, 0, 13);
		for (i = 0; i < 33; i++) {
			wr(scale_coef, (((pps_lut_tap2[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap2[i][0] & 0xff) << 16) |
			(((pps_lut_tap2[i][1] >> 8) & 0xff) << 8) |
			((pps_lut_tap2[i][1] & 0xff) << 0));
			wr(scale_coef, (((pps_lut_tap2[i][0] >> 8) &
						0xff) << 24) |
			((pps_lut_tap2[i][0] & 0xff) << 16) |
			(((pps_lut_tap2[i][1] >> 8) & 0xff) << 8)  |
			((pps_lut_tap2[i][1] & 0xff) << 0));
		}
	}
}

void set_aa_pps_hor_slice(int *slc_in_bgn,
	int *slc_in_end,
	int *reg_init_inte,
	int *reg_init_phase,
	int bord_bgn,
	int bord_end,
	int slc_out_bgn,
	int slc_out_end,
	int frm_size_in,
	int reg_sc_ini_integer,
	int reg_sc_ini_phase,
	int phase_step)
{
	//long din_bgn_acc,din_end_acc;
	long long din_bgn_acc, din_end_acc;
	int  slc_act_in_bgn, slc_act_in_end;
	int  slc_ovlp_lft = 4;
	int  slc_ovlp_rgt = 4;

	dbg_h2("phase_step: %d\n", phase_step);
	dbg_h2("slc_out_bgn: %d\n", slc_out_bgn);
	dbg_h2("slc_out_end: %d\n", slc_out_end);

	din_bgn_acc = ((long long)(slc_out_bgn) * phase_step) +
		(reg_sc_ini_integer << 24) + reg_sc_ini_phase;
	din_end_acc = ((long long)(slc_out_end - 1) * phase_step) +
		(reg_sc_ini_integer << 24) + reg_sc_ini_phase;
	dbg_h2("din_bgn_acc: %lld\n", din_bgn_acc);
	dbg_h2("din_end_acc: %lld\n", din_end_acc);

	slc_act_in_bgn = (int)(din_bgn_acc >> 24);
	slc_act_in_end = (int)(din_end_acc >> 24) + 1;
	dbg_h2("slc_act_in_bgn: %d\n", slc_act_in_bgn);
	dbg_h2("slc_act_in_end: %d\n", slc_act_in_end);

	*slc_in_bgn = slc_act_in_bgn - slc_ovlp_lft;
	*slc_in_end = slc_act_in_end + slc_ovlp_rgt;

	*slc_in_bgn = *slc_in_bgn - (*slc_in_bgn % 2);
	*slc_in_end = *slc_in_end + (*slc_in_end % 2);

	*slc_in_bgn = bord_bgn ? 0 : MAX(0, MIN(frm_size_in, *slc_in_bgn));
	*slc_in_end = bord_end ? frm_size_in :
		MAX(0, MIN(frm_size_in, *slc_in_end));
	dbg_h2("*slc_in_bgn: %d\n", *slc_in_bgn);
	dbg_h2("*slc_in_end: %d\n", *slc_in_end);

	*reg_init_inte = bord_bgn ? reg_sc_ini_integer :
		slc_act_in_bgn - *slc_in_bgn;
	*reg_init_phase = bord_bgn ? reg_sc_ini_phase :
		(int)(din_bgn_acc & 0xFFFFFF);
	dbg_h2("*reg_init_inte : %d\n", *reg_init_inte);
	dbg_h2("*reg_init_phase: %d\n", *reg_init_phase);
}

void set_aa_pps_sc_mux_sel(int *reg_sc_mux_sel,
	int *reg_vds_tap_num,
	int slc_hsize_in[4],
	int slc_hsize_out[4],
	int frm_hsize_in,
	int frm_hsize_out,
	int slc_mode,
	int buf_depth)
{
	int max_hsize_in, max_hsize_out;
	int slc_max_hsize_in =
		MAX(MAX(slc_hsize_in[0], slc_hsize_in[1]),
		MAX(slc_hsize_in[2], slc_hsize_in[3]));
	int slc_max_hsize_out =
		MAX(MAX(slc_hsize_out[0], slc_hsize_out[1]),
		MAX(slc_hsize_out[2], slc_hsize_out[3]));

	if (slc_mode == 1) {
		max_hsize_in  = slc_max_hsize_in;
		max_hsize_out = slc_max_hsize_out;
	} else {
		max_hsize_in  = frm_hsize_in;
		max_hsize_out = frm_hsize_out;
	}

	if (frm_hsize_in < frm_hsize_out) {
		if (max_hsize_out < buf_depth)
			*reg_sc_mux_sel = 1;
		else
			*reg_sc_mux_sel = 0;
	} else {
		*reg_sc_mux_sel = 1;
	}

	if (*reg_sc_mux_sel == 1) {
		if (max_hsize_out <= buf_depth)
			*reg_vds_tap_num = 8;
		else if (max_hsize_out <= buf_depth * 2)
			*reg_vds_tap_num = 4;
		else
			*reg_vds_tap_num = 2;
	} else {
		if (max_hsize_in <= buf_depth)
			*reg_vds_tap_num = 8;
		else if (max_hsize_in <= buf_depth * 2)
			*reg_vds_tap_num = 4;
		else
			*reg_vds_tap_num = 2;
	}
}

void aa_pps_top(struct AA_PPS_TOP_TYPE *aa_pps_top)
{
//output
	int reg_init_inte[4] = {0}, reg_init_phase[4] = {0};
	int slc_in_bgn[4] = {0}, slc_in_end[4] = {0};
	int slc_out_bgn[4] = {0}, slc_out_end[4] = {0};
	int slc_size_in[4], slc_size_out[4];
	int i = 0, bord_bgn, bord_end;
	bool wr_sel = 0;
	//ary add for register access;
	//0 is for dpss access, 1 is for vc bus access
	//logic
	int frm_hsize_in  = aa_pps_top->frm_hsize_in;
	int frm_vsize_in  = aa_pps_top->frm_vsize_in;
	int frm_hsize_out = aa_pps_top->frm_hsize_out;
	int frm_vsize_out = aa_pps_top->frm_vsize_out;
	int slc_mode = aa_pps_top->slc_mode;
	int slc_num = aa_pps_top->slc_num;
	int hsize_out_sel  = frm_hsize_out;
	int hds_en = 0;
	int vds_en = 0;
	int reg_hsc_ini_integer = 0;
	int reg_hsc_ini_phase = 0;
	int reg_vds_tap_num = 8;
	int reg_hds_tap_num = 8;
	int reg_sc_mux_sel = 1;
	int reg_hds_sel = 1;
	int reg_vds_sel = 1;
	int reg_hvds_sel = 0;

	dbg_h2("%s %d\n", __func__, reg_hvds_sel);

	slc_out_bgn[0] = aa_pps_top->slc_act_out_bgn[0];
	slc_out_bgn[1] = aa_pps_top->slc_act_out_bgn[1];
	slc_out_bgn[2] = aa_pps_top->slc_act_out_bgn[2];
	slc_out_bgn[3] = aa_pps_top->slc_act_out_bgn[3];

	slc_out_end[0] = aa_pps_top->slc_act_out_end[0];
	slc_out_end[1] = aa_pps_top->slc_act_out_end[1];
	slc_out_end[2] = aa_pps_top->slc_act_out_end[2];
	slc_out_end[3] = aa_pps_top->slc_act_out_end[3];

	slc_size_out[0] = slc_out_end[0] - slc_out_bgn[0];
	slc_size_out[1] = slc_out_end[1] - slc_out_bgn[1];
	slc_size_out[2] = slc_out_end[2] - slc_out_bgn[2];
	slc_size_out[3] = slc_out_end[3] - slc_out_bgn[3];
	//----- en
	if (frm_hsize_in != frm_hsize_out)
		hds_en = 1;
	if (frm_vsize_in != frm_vsize_out)
		vds_en = 1;
	if (slc_mode == 1)
		hsize_out_sel = slc_size_out[0];

	//----- frm step
	int hsc_integer_part = frm_hsize_in / frm_hsize_out;
#ifdef RUN_ON_PC
	int hsc_fraction_part = ((uint64_t)frm_hsize_in << 24) / frm_hsize_out -
				(hsc_integer_part << 24);
#else
	int hsc_fraction_part = div_u64((uint64_t)frm_hsize_in << 24,
				frm_hsize_out) - (hsc_integer_part << 24);
#endif
	int hor_phase_step = (hsc_integer_part << 24) + hsc_fraction_part;
	int vsc_integer_part  = frm_vsize_in / frm_vsize_out;
#ifdef RUN_ON_PC
	int vsc_fraction_part = ((uint64_t)frm_vsize_in << 24) / frm_vsize_out -
				(vsc_integer_part << 24);
#else
	int vsc_fraction_part = div_u64((uint64_t)frm_vsize_in << 24,
				frm_vsize_out) - (vsc_integer_part << 24);
#endif
	//----- slice info
	dbg_h2("aa_pps_slc_num: %d\n", slc_num);
	dbg_h2("aa_pps_slc_mode: %d\n", slc_mode);
	dbg_h2("hsc_integer_part: %d\n", hsc_integer_part);
	dbg_h2("hsc_fraction_part: %d\n", hsc_fraction_part);
	dbg_h2("hor_phase_step: %d\n", hor_phase_step);
	dbg_h2("frm_size_in: %d\n", frm_hsize_in);
	dbg_h2("frm_size_out: %d\n", frm_hsize_out);

	for (i = 0; i < slc_num; i++) {
		bord_bgn = (i == 0) ? 1 : 0;
		bord_end = (i == slc_num - 1) ? 1 : 0;

		dbg_h2("-------------slice[%d]----------------\n", i);

		dbg_h2("bord_bgn: %d\n", bord_bgn);
		dbg_h2("bord_end: %d\n", bord_end);

		set_aa_pps_hor_slice(&slc_in_bgn[i], &slc_in_end[i],
			&reg_init_inte[i], &reg_init_phase[i],
			bord_bgn, bord_end, slc_out_bgn[i],
			slc_out_end[i], frm_hsize_in,
			reg_hsc_ini_integer,
			reg_hsc_ini_phase,
			hor_phase_step);

		aa_pps_top->slc_in_bgn[i] = slc_in_bgn[i];
		aa_pps_top->slc_in_end[i] = slc_in_end[i];
		slc_size_in[i] = slc_in_end[i] - slc_in_bgn[i];
	}

	//------ sc mux and vtap and path sel
	int buf_depth = 720;

	if (aa_pps_top->apply_mode == 0) {
		reg_sc_mux_sel = 0;
		reg_vds_tap_num = 8;
		reg_hds_sel = 3;
		reg_vds_sel = 1;
		reg_hvds_sel = 1;
	} else if (aa_pps_top->apply_mode == 1) {
		reg_sc_mux_sel = 0;
		reg_vds_tap_num = 4;
		reg_hds_sel = 1;
		reg_vds_sel = 3;
		reg_hvds_sel = 0;
	} else {
		if (aa_pps_top->inst_sel == 0) {
			set_aa_pps_sc_mux_sel(&reg_sc_mux_sel,
				&reg_vds_tap_num,
				slc_size_in,
				slc_size_out,
				frm_hsize_in,
				frm_hsize_out,
				slc_mode, buf_depth);
		} else {
			reg_sc_mux_sel = 0;
			reg_vds_tap_num = test_h_tap;
			reg_hds_tap_num = test_v_tap;
		}
	}

	//------ cfg reg
	u32 PPS_HSC_START_PHASE_STEP;
	u32 PPS_VSC_START_PHASE_STEP;
	u32 PPS_HSC_PHASE_CTRL_0;
	u32 PPS_HSC_PHASE_CTRL_1;
	u32 PPS_HSC_PHASE_CTRL_2;
	u32 PPS_HSC_PHASE_CTRL_3;
	u32 PPS_COMP;
	u32 PPS_OUT_HSIZE_0;
	u32 PPS_OUT_HSIZE_1;
	u32 PPS_OUT_VSIZE_0;
	u32 PPS_CTRL;

	if (aa_pps_top->inst_sel == 0) {
		PPS_HSC_START_PHASE_STEP = FRC_AA_PPS_HSC_START_PHASE_STEP;
		PPS_VSC_START_PHASE_STEP = FRC_AA_PPS_VSC_START_PHASE_STEP;
		PPS_HSC_PHASE_CTRL_0 = FRC_AA_PPS_HSC_PHASE_CTRL_0;
		PPS_HSC_PHASE_CTRL_1 = FRC_AA_PPS_HSC_PHASE_CTRL_1;
		PPS_HSC_PHASE_CTRL_2 = FRC_AA_PPS_HSC_PHASE_CTRL_2;
		PPS_HSC_PHASE_CTRL_3 = FRC_AA_PPS_HSC_PHASE_CTRL_3;
		PPS_COMP = FRC_AA_PPS_COMP;
		PPS_OUT_HSIZE_0 = FRC_AA_PPS_OUT_HSIZE_0;
		PPS_OUT_HSIZE_1 = FRC_AA_PPS_OUT_HSIZE_1;
		PPS_OUT_VSIZE_0 = FRC_AA_PPS_OUT_VSIZE_0;
		PPS_CTRL = FRC_AA_PPS_CTRL;
		wr_sel = 0;
	} else {
		PPS_HSC_START_PHASE_STEP = LCEVC_PPS_HSC_START_PHASE_STEP;
		PPS_VSC_START_PHASE_STEP = LCEVC_PPS_VSC_START_PHASE_STEP;
		PPS_HSC_PHASE_CTRL_0 = LCEVC_PPS_HSC_PHASE_CTRL_0;
		PPS_HSC_PHASE_CTRL_1 = LCEVC_PPS_HSC_PHASE_CTRL_1;
		PPS_HSC_PHASE_CTRL_2 = LCEVC_PPS_HSC_PHASE_CTRL_2;
		PPS_HSC_PHASE_CTRL_3 = LCEVC_PPS_HSC_PHASE_CTRL_3;
		PPS_COMP = LCEVC_PPS_COMP;
		PPS_OUT_HSIZE_0 = LCEVC_PPS_OUT_HSIZE_0;
		PPS_OUT_HSIZE_1 = LCEVC_PPS_OUT_HSIZE_1;
		PPS_OUT_VSIZE_0 = LCEVC_PPS_OUT_VSIZE_0;
		PPS_CTRL = LCEVC_PPS_CTRL;
		wr_sel = 1;
	}

	if (aa_pps_top->inst_sel == 0) {
		set_aa_pps_coef(FRC_AA_PPS_COEF_IDX,
			FRC_AA_PPS_COEF, reg_hds_tap_num, reg_vds_tap_num);
	} else {
		//coef addr = 0/64
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF1, 0, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF1, 128, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF0, 0, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF0, 0, 21, 11);

		w_reg_bit_vc(LCEVC_PPS_VDS_COEF1, 0, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF1, 128, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF0, 0, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF0, 0, 21, 11);
		//coef addr = 16/48
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF3, -9, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF3, 111, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF2, 29, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF2, -3, 21, 11);

		w_reg_bit_vc(LCEVC_PPS_VDS_COEF3, -9, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF3, 111, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF2, 29, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF2, -3, 21, 11);
		//coef addr = 32
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF5, -8, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF5, 72, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF4, 72, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_HDS_COEF4, -8, 21, 11);

		w_reg_bit_vc(LCEVC_PPS_VDS_COEF5, -8, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF5, 72, 21, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF4, 72, 0, 11);
		w_reg_bit_vc(LCEVC_PPS_VDS_COEF4, -8, 21, 11);
	}

	if (wr_sel == 0) { //ary add
		w_reg_bit(PPS_HSC_START_PHASE_STEP, hsc_integer_part, 24, 4);
		w_reg_bit(PPS_HSC_START_PHASE_STEP, hsc_fraction_part, 0, 24);
		w_reg_bit(PPS_VSC_START_PHASE_STEP, vsc_integer_part, 24, 4);
		w_reg_bit(PPS_VSC_START_PHASE_STEP, vsc_fraction_part, 0, 24);
		w_reg_bit(PPS_VSC_START_PHASE_STEP, reg_sc_mux_sel, 29, 1);

		w_reg_bit(PPS_HSC_PHASE_CTRL_0, reg_init_inte[0], 24, 5);
		w_reg_bit(PPS_HSC_PHASE_CTRL_0, reg_init_phase[0], 0, 24);
		w_reg_bit(PPS_HSC_PHASE_CTRL_1, reg_init_inte[1], 24, 5);
		w_reg_bit(PPS_HSC_PHASE_CTRL_1, reg_init_phase[1], 0, 24);
		w_reg_bit(PPS_HSC_PHASE_CTRL_2, reg_init_inte[2], 24, 5);
		w_reg_bit(PPS_HSC_PHASE_CTRL_2, reg_init_phase[2], 0, 24);
		w_reg_bit(PPS_HSC_PHASE_CTRL_3, reg_init_inte[3], 24, 5);
		w_reg_bit(PPS_HSC_PHASE_CTRL_3, reg_init_phase[3], 0, 24);
		w_reg_bit(PPS_COMP, reg_vds_tap_num, 0, 4);
		w_reg_bit(PPS_COMP, reg_hds_tap_num, 4, 4);
		w_reg_bit(PPS_COMP, reg_hvds_sel, 20, 1);
		w_reg_bit(PPS_COMP, reg_vds_sel, 21, 2);
		w_reg_bit(PPS_COMP, reg_hds_sel, 23, 2);

		w_reg_bit(PPS_OUT_HSIZE_0, hsize_out_sel, 0, 16);
		w_reg_bit(PPS_OUT_HSIZE_0, slc_size_out[1], 16, 16);
		w_reg_bit(PPS_OUT_HSIZE_1, slc_size_out[2], 0, 16);
		w_reg_bit(PPS_OUT_HSIZE_1, slc_size_out[3], 16, 16);
		w_reg_bit(PPS_OUT_VSIZE_0, frm_vsize_out, 0, 16);

		w_reg_bit(PPS_CTRL, vds_en, 0, 1);
		w_reg_bit(PPS_CTRL, hds_en, 1, 1);
	} else {
		w_reg_bit_vc(PPS_HSC_START_PHASE_STEP, hsc_integer_part, 24, 4);
		w_reg_bit_vc(PPS_HSC_START_PHASE_STEP, hsc_fraction_part, 0, 24);
		w_reg_bit_vc(PPS_VSC_START_PHASE_STEP, vsc_integer_part, 24, 4);
		w_reg_bit_vc(PPS_VSC_START_PHASE_STEP, vsc_fraction_part, 0, 24);
		w_reg_bit_vc(PPS_VSC_START_PHASE_STEP, reg_sc_mux_sel, 29, 1);

		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_0, reg_init_inte[0], 24, 5);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_0, reg_init_phase[0], 0, 24);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_1, reg_init_inte[1], 24, 5);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_1, reg_init_phase[1], 0, 24);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_2, reg_init_inte[2], 24, 5);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_2, reg_init_phase[2], 0, 24);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_3, reg_init_inte[3], 24, 5);
		w_reg_bit_vc(PPS_HSC_PHASE_CTRL_3, reg_init_phase[3], 0, 24);
		w_reg_bit_vc(PPS_COMP, reg_vds_tap_num, 0, 4);
		w_reg_bit_vc(PPS_COMP, reg_hds_tap_num, 4, 4);
		w_reg_bit_vc(PPS_COMP, reg_hvds_sel, 20, 1);
		w_reg_bit_vc(PPS_COMP, reg_vds_sel, 21, 2);
		w_reg_bit_vc(PPS_COMP, reg_hds_sel, 23, 2);

		w_reg_bit_vc(PPS_OUT_HSIZE_0, hsize_out_sel, 0, 16);
		w_reg_bit_vc(PPS_OUT_HSIZE_0, slc_size_out[1], 16, 16);
		w_reg_bit_vc(PPS_OUT_HSIZE_1, slc_size_out[2], 0, 16);
		w_reg_bit_vc(PPS_OUT_HSIZE_1, slc_size_out[3], 16, 16);
		w_reg_bit_vc(PPS_OUT_VSIZE_0, frm_vsize_out, 0, 16);

		w_reg_bit_vc(PPS_CTRL, vds_en, 0, 1);
		w_reg_bit_vc(PPS_CTRL, hds_en, 1, 1);
	}
	dbg_h2("%s %d end\n", __func__, wr_sel);
}

