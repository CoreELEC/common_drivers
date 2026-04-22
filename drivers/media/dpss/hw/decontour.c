// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kfifo.h>
#include "define.h"
#include "dpss_intf.h"
//#include "dpss_baddr.h"
#include "decontour.h"
#include "../drv/dpss_pq_drv.h"

#ifdef FUNC_EN_DCT
unsigned int  dpss_dct_en = 1;

void calc_slc_info(u32 frm_hsize, u32 frm_hsize_sel, u32 slc_num,
	u32 *slc0_xbgn, u32 *slc0_xend,
	u32 *slc1_xbgn, u32 *slc1_xend,
	u32 *slc2_xbgn, u32 *slc2_xend,
	u32 *slc3_xbgn, u32 *slc3_xend)
{
	//just calc slc active region
	u32 frm_hsize_d4 = (frm_hsize + 3) >> 2;
	u32 frm_hsize_d4_d16 = (frm_hsize_d4 + 15) >> 4;
	u32 frm_hsize_d4_d32 = (frm_hsize_d4 + 31) >> 5;
	u32 frm_hsize_d4_d64 = (frm_hsize_d4 + 63) >> 6;
	u32 frm_hsize_d4_d128 = (frm_hsize_d4 + 127) >> 7;
	u32 frm_hsize_m1 = frm_hsize - 1;

	u32 frm_hsize_d4_rnd = (frm_hsize_sel == 4) ? frm_hsize_d4_d128 << 7 :
		(frm_hsize_sel == 3) ? frm_hsize_d4_d64 << 6 :
		(frm_hsize_sel == 2) ? frm_hsize_d4_d32 << 5 :
		(frm_hsize_sel == 1) ? frm_hsize_d4_d16 << 4 :
		frm_hsize_d4;

	u32 frm_hsize_t0 = frm_hsize_d4_rnd;
	u32 frm_hsize_t1 = frm_hsize_d4_rnd << 1;
	u32 frm_hsize_t2 = frm_hsize_t0 + frm_hsize_t1;

	*slc0_xbgn = 0;
	*slc0_xend = slc_num == 4 ? frm_hsize_t0 - 1 :
		slc_num == 2 ? frm_hsize_t1 - 1 : frm_hsize_m1;

	*slc1_xbgn = slc_num == 4 ? frm_hsize_t0 : frm_hsize_t1;
	*slc1_xend = slc_num == 4 ? frm_hsize_t1 - 1 : frm_hsize_m1;

	*slc2_xbgn = frm_hsize_t1;
	*slc2_xend = frm_hsize_t2 - 1;
	*slc3_xbgn = frm_hsize_t2;
	*slc3_xend = frm_hsize_m1;
}

//initial decontour pre - grid	//ini_dcntr_pre change name by ary
void dpss_ini_dcntr_pre(int hsize, int vsize, int grd_num_mode,
	int in_fmt_mode, int grd_444to422_mode, int grd_422to420_mode, int dsx, int dsy)
{
	//dct grid enable
	//   w_reg_bit(VPU_DAE_WRAP_CTRL, 1, 1, 1);
	w_reg_bit(REG_DCTR_GCLK_CTRL0, 3, 12, 2);//open free_clk for divr upsample
	//int xsize = hsize;
	//int ysize = vsize;
	dbg_h2("%s:hsize=%d, vsize=%d,grd_num_mode=%d, dsx=%d, dsy=%d\n",
		__func__, hsize, vsize, grd_num_mode, dsx, dsy);

	//if (ds_mode)
		dbg_h2("%s:addr: PIC_ORG_Y_BADDR -> DPSS_SRC0_IDW_YBUF_ADDR\n", __func__);
	//====================================================
	// top
	//====================================================
	//wr(DCTR_BGRID_TOP_FSIZE , (hsize << 13) |
	//                          (vsize      ) );
	//
	//wr(DCTR_BGRID_TOP_CTRL0, (1 << 29) | // reg_grdbuild_en
	//                         (2 << 16) | // reg_stdly_num
	//                         (0 << 4)  | // reg_hs_sel
	//                         (1 << 2)  | // reg_din_sel
	//                         (1 << 1)  | // reg_ds_mif_en
	//                         (1 << 0) ); // reg_grd_mif_en

	//====================================================
	// build wrap
	//====================================================
	int reg_in_ds_rate_x = dsx;
	int reg_in_ds_rate_y = dsy;

	int ds_r_sft_x = reg_in_ds_rate_x;
	int ds_r_sft_y = reg_in_ds_rate_y;
	int in_ds_r_x = 1 << ds_r_sft_x;
	int in_ds_r_y = 1 << ds_r_sft_y;

	//ary no use u32 grd_num = rd(DCTR_BGRID_PARAM1_PRE);//RTL

	u32 reg_grd_xnum = grd_num_mode == 0 ? 40 :
				grd_num_mode == 1 ? 60 : 80;//= (grd_num>>10) & 0x3ff;

	u32 reg_grd_ynum = grd_num_mode == 0 ? 23 :
				grd_num_mode == 1 ? 34 : 45;//= grd_num       & 0x3ff;

	u32 reg_grd_xsize_ds = 0;//ary add init val
	u32 reg_grd_ysize_ds = 0;
	u32 reg_grd_xnum_use = 0;
	u32 reg_grd_ynum_use = 0;
	u32 reg_grd_xsize = 0;
	u32 reg_grd_ysize = 0;

	int grd_path   = reg_in_ds_rate_x == 0 && reg_in_ds_rate_y == 0;
	//int grd_path   = 1;

	if (grd_path == 0) {  //grid build ds mode
		reg_grd_xsize_ds = (hsize + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize_ds = (vsize + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use =
			((hsize - reg_grd_xsize_ds / 2) +
				reg_grd_xsize_ds - 1) / (reg_grd_xsize_ds) + 1;
		reg_grd_ynum_use =
			((vsize - reg_grd_ysize_ds / 2) +
				reg_grd_ysize_ds - 1) / (reg_grd_ysize_ds) + 1;
		reg_grd_xsize = reg_grd_xsize_ds * in_ds_r_x;
		reg_grd_ysize = reg_grd_ysize_ds * in_ds_r_y;
	} else {//grid build non-ds mode
		reg_grd_xsize = (hsize + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize = (vsize + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use = ((hsize - reg_grd_xsize / 2) +
				reg_grd_xsize - 1) / (reg_grd_xsize) + 1;
		reg_grd_ynum_use = ((vsize - reg_grd_ysize / 2) +
				reg_grd_ysize - 1) / (reg_grd_ysize) + 1;
	}

	wr(DCTR_BGRID_PATH_PRE, (grd_path << 4));
	dbg_h2("%s: reg_grd_xsize_ds =%d, reg_grd_ysize_ds=%d\n",
		__func__,
		reg_grd_xsize_ds,
		reg_grd_ysize_ds);
	dbg_h2("%s: reg_grd_xnum_use=%d,reg_grd_ynum_use=%d\n",
		__func__,
		reg_grd_xnum_use,
		reg_grd_ynum_use);
	dbg_h2("%s: reg_grd_xsize=%d,reg_grd_ysize=%d\n",
		__func__,
		reg_grd_xsize,
		reg_grd_ysize);

	//wr(DCTR_DS_PRE,
	//              (0 << 11 ) |   //reg_in_ds_phs_x <= pwdata[10: 9];
	//              (0 << 9 ) |   //reg_in_ds_phs_y <= pwdata[8: 7];
	//              (8 << 5 ) |   //reg_in_ds_bit   <= pwdata[6: 3];
	//              (0 << 4 ) |   //reg_in_ds_mode  <= pwdata[2];
	//(reg_in_ds_rate_x<< 2 ) |
	// reg_in_ds_rate_y     );  //reg_in_ds_rate  <= pwdata[1: 0];

	//    wr(DPSS_FBUF_ME_SCALE,
	//                          reg_in_ds_rate_y<<4 |
	//                          reg_in_ds_rate_x);

	wr(DCTR_BGRID_PARAM2_PRE, (reg_grd_xsize << 24) |
				(reg_grd_ysize << 16) |
				(48            << 8) | //valsz
				(22));//vnum

	wr(DCTR_BGRID_PARAM3_PRE, (reg_grd_xnum_use << 16) |
				(reg_grd_ynum_use));

	wr(DCTR_BGRID_PARAM4_PRE, (reg_grd_xsize_ds << 16) |
				(reg_grd_ysize_ds));

	wr(DCTR_BGRID_WRAP_CTRL, 1);

	int reg_grd_vbin_gb_0  = 24 + 48 * 0;
	int reg_grd_vbin_gb_1  = 24 + 48 * 1;
	int reg_grd_vbin_gb_2  = 24 + 48 * 2;
	int reg_grd_vbin_gb_3  = 24 + 48 * 3;
	int reg_grd_vbin_gb_4  = 24 + 48 * 4;
	int reg_grd_vbin_gb_5  = 24 + 48 * 5;
	int reg_grd_vbin_gb_6  = 24 + 48 * 6;
	int reg_grd_vbin_gb_7  = 24 + 48 * 7;
	int reg_grd_vbin_gb_8  = 24 + 48 * 8;
	int reg_grd_vbin_gb_9  = 24 + 48 * 9;
	int reg_grd_vbin_gb_10 = 24 + 48 * 10;
	int reg_grd_vbin_gb_11 = 24 + 48 * 11;
	int reg_grd_vbin_gb_12 = 24 + 48 * 12;
	int reg_grd_vbin_gb_13 = 24 + 48 * 13;
	int reg_grd_vbin_gb_14 = 24 + 48 * 14;
	int reg_grd_vbin_gb_15 = 24 + 48 * 15;
	int reg_grd_vbin_gb_16 = 24 + 48 * 16;
	int reg_grd_vbin_gb_17 = 24 + 48 * 17;
	int reg_grd_vbin_gb_18 = 24 + 48 * 18;
	int reg_grd_vbin_gb_19 = 24 + 48 * 19;
	int reg_grd_vbin_gb_20 = 24 + 48 * 20;
	int reg_grd_vbin_gb_21 = 24 + 48 * 21;

	wr(DCTR_BGRID_PARAM5_PRE_0, (reg_grd_vbin_gb_0 << 16) | reg_grd_vbin_gb_1);
	wr(DCTR_BGRID_PARAM5_PRE_1, (reg_grd_vbin_gb_2 << 16) | reg_grd_vbin_gb_3);
	wr(DCTR_BGRID_PARAM5_PRE_2, (reg_grd_vbin_gb_4 << 16) | reg_grd_vbin_gb_5);
	wr(DCTR_BGRID_PARAM5_PRE_3, (reg_grd_vbin_gb_6 << 16) | reg_grd_vbin_gb_7);
	wr(DCTR_BGRID_PARAM5_PRE_4, (reg_grd_vbin_gb_8 << 16) | reg_grd_vbin_gb_9);
	wr(DCTR_BGRID_PARAM5_PRE_5, (reg_grd_vbin_gb_10 << 16) | reg_grd_vbin_gb_11);
	wr(DCTR_BGRID_PARAM5_PRE_6, (reg_grd_vbin_gb_12 << 16) | reg_grd_vbin_gb_13);
	wr(DCTR_BGRID_PARAM5_PRE_7, (reg_grd_vbin_gb_14 << 16) | reg_grd_vbin_gb_15);
	wr(DCTR_BGRID_PARAM5_PRE_8, (reg_grd_vbin_gb_16 << 16) | reg_grd_vbin_gb_17);
	wr(DCTR_BGRID_PARAM5_PRE_9, (reg_grd_vbin_gb_18 << 16) | reg_grd_vbin_gb_19);
	wr(DCTR_BGRID_PARAM5_PRE_10, (reg_grd_vbin_gb_20 << 16) | reg_grd_vbin_gb_21);

	w_reg_bit(VPU_DAE_WRAP_DCT_GRD, grd_444to422_mode, 4, 3);
	w_reg_bit(VPU_DAE_WRAP_DCT_GRD, grd_422to420_mode, 8, 3);
	w_reg_bit(VPU_DAE_WRAP_DCT_GRD, in_fmt_mode, 12, 2);
	dbg_h2("%s:%d\n", __func__, in_fmt_mode);
	di_write_data_table(DI_PAGE_MODULE_DCT, 0);
	//#ifdef HIS_MARK
	if (dpss_dct_en) {
		w_reg_bit(DCTR_PATH, 3, 27, 2);
		w_reg_bit(DCTR_PATH, 3, 29, 2);
	} else {
		w_reg_bit(DCTR_PATH, 0, 27, 2);
		w_reg_bit(DCTR_PATH, 0, 29, 2);
	}
	//#endif
}

//initial decontour post - core
void ini_dcntr_post(int hsize, int vsize, int grd_num_mode, int dsx, int dsy, u32 *grid_stride)
{
	//dct post enable
	w_reg_bit(VPU_VBE_TOP_CTR0, 1, 26, 1);
	w_reg_bit(REG_DCTR_PST_GCLK_CTRL0, 3, 2, 2);// open avg_flt upsample free_clk
	w_reg_bit(REG_DCTR_PST_GCLK_CTRL0, 3, 8, 2);// open 4k upsample free_clk (avg dir divr)
	wr(VPU_VBE_TOP_DCNTR_SIZE, vsize << 16 | hsize);
	dbg_h2("dcntr_post hsize=%d dcntr_post vsize=%d\n", hsize, vsize);

	//int xsize = hsize;
	//int ysize = vsize;

	int in_ds_rate_x = dsx;
	int in_ds_rate_y = dsy;

	//int sig_path = 0;
	//int in_ds_mode = 0;

	int in_ds_r_x = 1 << in_ds_rate_x;
	int in_ds_r_y = 1 << in_ds_rate_y;
	int intep_phs_x_use, intep_phs_y_use, intep_phs_x_rtl, intep_phs_y_rtl;
	int intep_phs_x_avg, intep_phs_y_avg, intep_phs_x_avg_rtl, intep_phs_y_avg_rtl;
	//int phs_x = 0;
	int phs_y = 0;

	//marked for coverity
	//interp phase calc
	//if (sig_path == 0) {  //DS mode
	//	if (in_ds_mode == 0) {
	//		intep_phs_x_use = -(2 * in_ds_r_x - 2);
	//		intep_phs_y_use = -(2 * in_ds_r_y - 2);
	//	} else { //AVG DS, need phase align
	//		intep_phs_x_use = -(phs_x * 4);
	//		intep_phs_y_use = -(phs_y * 4);
	//	} //Decimation DS, start point is point 0. or other scale // a
	//	// sig_phase
	//	intep_phs_x_avg = -(2 * in_ds_r_x - 2);
	//	intep_phs_y_avg = -(phs_y * 4);
	//	// SW also can changed to other phases according to Dos's down-sample phase setting
	//} else { //non-DS mode
	//	intep_phs_x_use = 0;
	//	intep_phs_y_use = 0;
	//	intep_phs_x_avg = 0;
	//	intep_phs_y_avg = 0;
	//}

	intep_phs_x_use = -(2 * in_ds_r_x - 2);
	intep_phs_y_use = -(2 * in_ds_r_y - 2);
	// sig_phase
	intep_phs_x_avg = -(2 * in_ds_r_x - 2);
	intep_phs_y_avg = -(phs_y * 4);

	if (in_ds_r_x == 2) {
		intep_phs_x_rtl = 2 * intep_phs_x_use;
		intep_phs_x_avg_rtl = 2 * intep_phs_x_avg;
	} else {
		intep_phs_x_rtl = intep_phs_x_use;
		intep_phs_x_avg_rtl = intep_phs_x_avg;
	}

	if (in_ds_r_y == 2) {
		intep_phs_y_rtl = 2 * intep_phs_y_use;
		intep_phs_y_avg_rtl = 2 * intep_phs_y_avg;
	} else {
		intep_phs_y_rtl = intep_phs_y_use;
		intep_phs_y_avg_rtl = intep_phs_y_avg;
	}

	wr(INTRP_PARAM, ((intep_phs_x_rtl & 0x1f) << 21) |
			((intep_phs_x_use & 0x1f) << 16) |
			((intep_phs_y_rtl & 0x1f) << 5) |
			((intep_phs_y_use & 0x1f) << 0)
			);

	wr(INTRP_PARAM_SIG, ((intep_phs_x_avg & 0x1f) << 24) |
			((intep_phs_y_avg & 0x1f) << 16) |
			((intep_phs_x_avg_rtl & 0x1f) << 8) |
			((intep_phs_y_avg_rtl & 0x1f) << 0)
			);
	int divrsmap_blk0_sft = 1;
	int divrsmap_blk1_sft = divrsmap_blk0_sft + 1;
	int divrsmap_blk2_sft = divrsmap_blk0_sft + 2;

	//w_reg_bit(DCNTR_DIVR_RMIF_CTRL2, divrsmap_blk0_sft+1, 22, 4);

	wr(DCTR_DIVR4, (divrsmap_blk0_sft << 28) |
			(divrsmap_blk1_sft << 24) |
			(divrsmap_blk2_sft << 20) |
			(0));

	int sig_ds_r_x = 2 - dsx;
	int sig_ds_r_y = 2 - dsy;

	wr(DCTR_SIGFIL, (64 << 16) |
			(2 << 8) |
			(1 << 4) |
			(sig_ds_r_x << 2) |
			(sig_ds_r_y)
			);

	//====================================================
	// slicing
	//====================================================
	//int ds_r_sft   = in_ds_rate;
	//int in_ds_r    = 1<<ds_r_sft;
	//int xds        = (xsize + in_ds_r - 1) >> ds_r_sft;
	//int yds        = (ysize + in_ds_r - 1) >> ds_r_sft;
	int reg_in_ds_rate_x = dsx;
	int reg_in_ds_rate_y = dsy;

	int ds_r_sft_x = reg_in_ds_rate_x;
	int ds_r_sft_y = reg_in_ds_rate_y;

	in_ds_r_x = 1 << ds_r_sft_x;
	in_ds_r_y = 1 << ds_r_sft_y;
	int xds = (hsize + in_ds_r_x - 1) >> ds_r_sft_x;
	int yds = (vsize + in_ds_r_y - 1) >> ds_r_sft_y;

	xds = (xds + 3) / 4 * 4;
	yds = (yds + 3) / 4 * 4;

	//ary no use u32 grd_num = Rd(DCTR_BGRID_PARAM1);

	//u32 reg_grd_xnum =80; //= (grd_num>>10) & (0x3ff);
	//u32 reg_grd_ynum =45; //= grd_num       & (0x3ff);

	u32 reg_grd_xnum = grd_num_mode  == 0 ? 40 :
				grd_num_mode == 1 ? 60 : 80; //= (grd_num>>10) & 0x3ff;

	u32 reg_grd_ynum = grd_num_mode == 0 ? 23 :
				grd_num_mode == 1 ? 34 : 45; //= grd_num       & 0x3ff;
	u32 reg_grd_xsize_ds = 0;
	u32 reg_grd_ysize_ds = 0;
	u32 reg_grd_xnum_use = 0;
	u32 reg_grd_ynum_use = 0;
	u32 reg_grd_xsize = 0;
	u32 reg_grd_ysize = 0;
	int grd_path   = reg_in_ds_rate_x == 0 && reg_in_ds_rate_y == 0;
	//int grd_path   = 0;

	if (grd_path == 0) { //grid build ds mode
		reg_grd_xsize_ds = (xds + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize_ds = (yds + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use =
			((xds - reg_grd_xsize_ds / 2) +
				reg_grd_xsize_ds - 1) / (reg_grd_xsize_ds) + 1;
		reg_grd_ynum_use =
			((yds - reg_grd_ysize_ds / 2) +
				reg_grd_ysize_ds - 1) / (reg_grd_ysize_ds) + 1;
		reg_grd_xsize   = reg_grd_xsize_ds * in_ds_r_x;
		reg_grd_ysize   = reg_grd_ysize_ds * in_ds_r_y;
	} else { //grid build non-ds mode
		reg_grd_xsize = (hsize + reg_grd_xnum - 1) / (reg_grd_xnum);
		reg_grd_ysize = (vsize + reg_grd_ynum - 1) / (reg_grd_ynum);
		reg_grd_xnum_use =
			((hsize - reg_grd_xsize / 2) + reg_grd_xsize - 1) / (reg_grd_xsize) + 1;
		reg_grd_ynum_use =
			((vsize - reg_grd_ysize / 2) + reg_grd_ysize - 1) / (reg_grd_ysize) + 1;
	}

	dbg_h2("%s: reg_grd_xsize_ds =%d, reg_grd_ysize_ds=%d\n",
		__func__,
		reg_grd_xsize_ds,
		reg_grd_ysize_ds);
	dbg_h2("%s: reg_grd_xnum_use=%d,reg_grd_ynum_use=%d\n",
		__func__,
		reg_grd_xnum_use,
		reg_grd_ynum_use);
	dbg_h2("%s: reg_grd_xsize=%d,reg_grd_ysize=%d\n",
		__func__,
		reg_grd_xsize,
		reg_grd_ysize);

	*grid_stride = reg_grd_xnum_use * 8;
	dbg_h2("%s: grid_stride = %d\n", __func__, *grid_stride);

	//wr(DCTR_PATH,(grd_path<<25));
	w_reg_bit(DCTR_PATH, reg_in_ds_rate_y, 0, 2);
	w_reg_bit(DCTR_PATH, reg_in_ds_rate_x, 16, 2);
	w_reg_bit(DCTR_PATH, 0, 2, 1);
	w_reg_bit(DCTR_PATH, 8, 3, 4);
	w_reg_bit(DCTR_PATH, 1, 18, 1);
	w_reg_bit(DCTR_PATH, 10, 19, 4);
	w_reg_bit(DCTR_PATH, grd_path, 25, 1);

	wr(DCTR_BGRID_PARAM2, (reg_grd_xsize << 24) |
				(reg_grd_ysize << 16) |
				(48 << 8) | //valsz
				(22));//vnum
	wr(DCTR_BGRID_PARAM3, (reg_grd_xnum_use << 16) |
				(reg_grd_ynum_use));

	wr(DCTR_BGRID_PARAM4, (reg_grd_xsize_ds << 16) |
				(reg_grd_ysize_ds));

	s32 reg_grd_xidx_div = (1 << 17) / (reg_grd_xsize);
	//17bits = 7bits for original implement + 10 bit for precision
	s32 reg_grd_yidx_div = (1 << 17) / (reg_grd_ysize);
	//17bits = 7bits for original implement + 10 bit for precision

	wr(DCTR_BGRID_PARAM5, reg_grd_xidx_div);
	wr(DCTR_BGRID_PARAM6, reg_grd_yidx_div);

	int reg_grd_vbin_gs_0  = 48 + 48 * 0;
	int reg_grd_vbin_gs_1  = 48 + 48 * 1;
	int reg_grd_vbin_gs_2  = 48 + 48 * 2;
	int reg_grd_vbin_gs_3  = 48 + 48 * 3;
	int reg_grd_vbin_gs_4  = 48 + 48 * 4;
	int reg_grd_vbin_gs_5  = 48 + 48 * 5;
	int reg_grd_vbin_gs_6  = 48 + 48 * 6;
	int reg_grd_vbin_gs_7  = 48 + 48 * 7;
	int reg_grd_vbin_gs_8  = 48 + 48 * 8;
	int reg_grd_vbin_gs_9  = 48 + 48 * 9;
	int reg_grd_vbin_gs_10 = 48 + 48 * 10;
	int reg_grd_vbin_gs_11 = 48 + 48 * 11;
	int reg_grd_vbin_gs_12 = 48 + 48 * 12;
	int reg_grd_vbin_gs_13 = 48 + 48 * 13;
	int reg_grd_vbin_gs_14 = 48 + 48 * 14;
	int reg_grd_vbin_gs_15 = 48 + 48 * 15;
	int reg_grd_vbin_gs_16 = 48 + 48 * 16;
	int reg_grd_vbin_gs_17 = 48 + 48 * 17;
	int reg_grd_vbin_gs_18 = 48 + 48 * 18;
	int reg_grd_vbin_gs_19 = 48 + 48 * 19;
	int reg_grd_vbin_gs_20 = 48 + 48 * 20;
	int reg_grd_vbin_gs_21 = 48 + 48 * 21;

	wr(DCTR_BGRID_PARAM8_0, (reg_grd_vbin_gs_0 << 16)  | reg_grd_vbin_gs_1);
	wr(DCTR_BGRID_PARAM8_1, (reg_grd_vbin_gs_2 << 16)  | reg_grd_vbin_gs_3);
	wr(DCTR_BGRID_PARAM8_2, (reg_grd_vbin_gs_4 << 16)  | reg_grd_vbin_gs_5);
	wr(DCTR_BGRID_PARAM8_3, (reg_grd_vbin_gs_6 << 16)  | reg_grd_vbin_gs_7);
	wr(DCTR_BGRID_PARAM8_4, (reg_grd_vbin_gs_8 << 16)  | reg_grd_vbin_gs_9);
	wr(DCTR_BGRID_PARAM8_5, (reg_grd_vbin_gs_10 << 16) | reg_grd_vbin_gs_11);
	wr(DCTR_BGRID_PARAM8_6, (reg_grd_vbin_gs_12 << 16) | reg_grd_vbin_gs_13);
	wr(DCTR_BGRID_PARAM8_7, (reg_grd_vbin_gs_14 << 16) | reg_grd_vbin_gs_15);
	wr(DCTR_BGRID_PARAM8_8, (reg_grd_vbin_gs_16 << 16) | reg_grd_vbin_gs_17);
	wr(DCTR_BGRID_PARAM8_9, (reg_grd_vbin_gs_18 << 16) | reg_grd_vbin_gs_19);
	wr(DCTR_BGRID_PARAM8_10, (reg_grd_vbin_gs_20 << 16) | reg_grd_vbin_gs_21);
	dbg_h2("%s: %d\n", __func__, reg_grd_vbin_gs_0);
}

#ifdef NO_USED
void    set_dcntr_grid_mif(u32   x_start,
	u32   x_end,
	u32   y_start,
	u32   y_end,
	u32   mode,           // 1 = RGB/YCBCR (3 bytes per pixel), 0 = 4:2:2 (2 bytes per pixel)
	u64   canvas_addr0,
	u64   canvas_addr1,
	u64   canvas_addr2,
	u32   pic_struct,
	u32   h_avg)     //00: frame, 10: top_field, 11: bot_field
{
	// General register setup
	u32 demux_mode = (mode > 1) ? 0 : mode;
	u32 bytes_per_pixel = (mode > 1) ? 0 : ((mode == 1) ? 2 : 1);	// RGB = 3 bytes per
	u32 burst_size_cr = 0;	// unused
	u32 burst_size_cb = 0;	// unused
	u32 burst_size_y = 3;	// 64x64 burst size
	u32 st_separate_en = (mode > 1);

	// ----------------------
	// General register
	// ----------------------

	wr(DCNTR_GRID_GEN_REG,      (4 << 19)               |   //hold lines
				(0 << 18)               |   // push pixel value
				(demux_mode << 16)      |
				(bytes_per_pixel << 14) |
				(burst_size_cr << 12)   |
				(burst_size_cb << 10)   |
				(burst_size_y << 8)     |
				(0 << 6)                |   // TODO: cntl_chro_rpt_lastl_ctrl
				(h_avg << 2)            |
				(st_separate_en << 1)   |
				(0 << 0)                    // cntl_enable (don't enable just yet)
	);

	// ----------------------
	// Canvas
	// ----------------------
	wr(DCNTR_GRID_CANVAS0, (canvas_addr2 << 16) |	// cntl_canvas0_addr2
	   (canvas_addr1 << 8) |	// cntl_canvas0_addr1
	   (canvas_addr0 << 0));	// cntl_canvas0_addr0

	// ----------------------
	// Picture 0 X/Y start,end
	// ----------------------
	wr(DCNTR_GRID_LUMA_X0, (x_end << 16) |	// cntl_luma_x_end0
	   (x_start << 0)	// cntl_luma_x_start0
	    );
	wr(DCNTR_GRID_LUMA_Y0, (y_end << 16) |	// cntl_luma_y_end0
	   (y_start << 0)	// cntl_luma_y_start0
	    );
	if (mode > 1) {
		wr(DCNTR_GRID_CHROMA_X0, ((((x_end + 1) >> 1) - 1) << 16) |
		   ((x_start + 1) >> 1));
		wr(DCNTR_GRID_CHROMA_Y0, ((((y_end + 1) >> 1) - 1) << 6) |
		   ((y_start + 1) >> 1));
	}

	if (pic_struct == 0) {	//frame
		wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |	// cntl_chroma1_rpt_loop
		   (0 << 16) |	// cntl_luma1_rpt_loop
		   (0 << 8) |	// cntl_chroma0_rpt_loop
		   (0 << 0));	// cntl_luma0_rpt_loop
		wr(DCNTR_GRID_LUMA0_RPT_PAT, 0x0);
		wr(DCNTR_GRID_CHROMA0_RPT_PAT, 0x0);	// unused
	} else if (pic_struct == 3) {	//bot_field
		wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |	// cntl_chroma1_rpt_loop
		   (0 << 16) |	// cntl_luma1_rpt_loop
		   (0 << 8) |	// cntl_chroma0_rpt_loop
		   (0 << 0)	// cntl_luma0_rpt_loop
		    );
		wr(DCNTR_GRID_LUMA0_RPT_PAT, 0x8);
		wr(DCNTR_GRID_CHROMA0_RPT_PAT, 0x8);	// unused
	} else if (pic_struct == 2) {//top_field
		wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |	// cntl_chroma1_rpt_loop
		   (0 << 16) |	// cntl_luma1_rpt_loop
		   (0x11 << 8) |	// cntl_chroma0_rpt_loop
		   (0x11 << 0));	// cntl_luma0_rpt_loop
		wr(DCNTR_GRID_LUMA0_RPT_PAT, 0x80);
		wr(DCNTR_GRID_CHROMA0_RPT_PAT, 0x80);	// unused
	} else if (pic_struct == 4) {	//y skip , uv no skip
		wr(DCNTR_GRID_RPT_LOOP, (0 << 24) |	// cntl_chroma1_rpt_loop
		   (0 << 16) |	// cntl_luma1_rpt_loop
		   (0x0 << 8) |	// cntl_chroma0_rpt_loop
		   (0x11 << 0)	// cntl_luma0_rpt_loop
		    );
		wr(DCNTR_GRID_LUMA0_RPT_PAT, 0x00);
		wr(DCNTR_GRID_CHROMA0_RPT_PAT, 0x80);	// unused
	}

	wr(DCNTR_GRID_DUMMY_PIXEL, 0x00808000);

	// nv21 mode
	if (mode == 2)
		wr(DCNTR_GRID_GEN_REG2, 1);	//nv21 mode

	//burst len=4; big endian mode
	wr(DCNTR_GRID_GEN_REG3, 5);

	// Enable vd_rmem_if0
	wr(DCNTR_GRID_GEN_REG, rd(DCNTR_GRID_GEN_REG) | (1 << 0));	// cntl_enable
}

//################################################################
void set_dcntr_grid_fmt(u32 hfmt_en, u32 hz_yc_ratio,
			u32 hz_ini_phase,
			u32 vfmt_en, u32 vt_yc_ratio,
			u32 vt_ini_phase,
			u32 y_length)

{
	u32 vt_phase_step = (16 >> vt_yc_ratio);

	u32 vfmt_w = (y_length >> hz_yc_ratio);

	wr(DCNTR_GRID_FMT_CTRL, (0 << 28) |	//hz rpt pixel
	   (hz_ini_phase << 24) |	//hz ini phase
	   (0 << 23) |		//repeat p0 enable
	   (hz_yc_ratio << 21) |	//hz yc ratio
	   (hfmt_en << 20) |	//hz enable
	   (1 << 17) |		//nrpt_phase0 enable
	   (0 << 16) |		//repeat l0 enable
	   (0 << 12) |		//skip line num
	   (vt_ini_phase << 8) |	//vt ini phase
	   (vt_phase_step << 1) |	//vt phase step (3.4)
	   (vfmt_en << 0));	//vt enable

	wr(DCNTR_GRID_FMT_W, (y_length << 16) |	//hz format width
	   (vfmt_w << 0));	//vt format width

}				//set_dcntr_grid_fmt

void dcntr_grid_rdmif(int canvas_id0,
	int canvas_id1,
	int canvas_id2,
	u64 canvas_baddr0,
	u64 canvas_baddr1,
	u64 canvas_baddr2,
	int src_hsize,
	int src_vsize,
	int src_fmt,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse,
	int pic_struct,
	int h_avg)
{
	int     hfmt_en;
	int     hz_yc_ratio;
	int     vfmt_en;
	int     vt_yc_ratio;
	int     fmt_hsize;
	int     canvas_w;
	int     stride_mif_y;
	int     stride_mif_c;

	canvas_w = src_fmt == 0 ? 2 :
		src_fmt == 1 ? 3 :
		src_fmt == 2 ? 1 : 1;

	// canvas: 0
	//canvas.start_addr = (canvas_baddr0 >> 3);
	//canvas.cav_width = ((src_hsize*canvas_w + 31) >> 5) << 2;
	//canvas.cav_hight =   src_vsize;
	//canvas.x_wrap_en = 0;
	//canvas.y_wrap_en = 0;
	//canvas.blk_mode  = 0;
	//config_cav_lut(canvas_id0, canvas);

	//if(src_fmt>1){
	//    canvas.start_addr = (canvas_baddr1 >> 3);
	//    canvas.cav_width = (((src_hsize >> 1)*canvas_w*(4-src_fmt) + 31) >> 5) << 2;
	//    canvas.cav_hight =   (src_vsize >> 1);
	//    canvas.x_wrap_en = 0;
	//    canvas.y_wrap_en = 0;
	//    canvas.blk_mode =  0;
	//    config_cav_lut(canvas_id1, canvas);
	//    if(src_fmt==3){
	//        canvas.start_addr = (canvas_baddr2 >> 3);
	//        canvas.cav_width = (((src_hsize >> 1)*canvas_w + 31) >> 5) << 2;
	//        canvas.cav_hight =   (src_vsize >> 1);
	//        canvas.x_wrap_en = 0;
	//        canvas.y_wrap_en = 0;
	//        canvas.blk_mode =  0;
	//        config_cav_lut(canvas_id2, canvas);
	//    }
	//}

	wr(DCNTR_GRID_BADDR_Y, canvas_baddr0 >> 4);
	wr(DCNTR_GRID_BADDR_CB, canvas_baddr1 >> 4);
	wr(DCNTR_GRID_BADDR_CR, canvas_baddr2 >> 4);

	stride_mif_y = src_fmt == 0 ? ((src_hsize * 8 * 2 + 127) >> 7) :	//422 one plane
	    src_fmt == 1 ? ((src_hsize * 8 * 3 + 127) >> 7) :	//444 one plane
	    src_fmt == 2 ? ((src_hsize * 8 + 127) >> 7) :	//420 two planes, nv21/nv12
	    ((src_hsize * 8 + 127) >> 7);	//444 three planes

	stride_mif_c = src_fmt == 0 ? stride_mif_y :	//422 one plane
	    src_fmt == 1 ? stride_mif_y :	//444 one plane
	    src_fmt == 2 ? stride_mif_y :	//420 two planes
	    stride_mif_y;	//444 three planes

	wr(DCNTR_GRID_STRIDE_0, (stride_mif_c << 16) | stride_mif_y);
	wr(DCNTR_GRID_STRIDE_1, (1 << 16) | stride_mif_c);

	if (src_fmt == 0) {	// 422
		hfmt_en = h_avg == 1 ? 0 : 1;
		hz_yc_ratio = 1;
		vfmt_en = 0;
		vt_yc_ratio = 0;
	} else if (src_fmt > 1) {	// 420
		hfmt_en = h_avg == 1 ? 0 : 1;
		hz_yc_ratio = 1;
		vfmt_en = pic_struct == 4 ? 0 : 1;
		vt_yc_ratio = 1;
	} else {		// 444
		hfmt_en = 0;
		hz_yc_ratio = 0;
		vfmt_en = 0;
		vt_yc_ratio = 0;
	}

	fmt_hsize = mif_x_end - mif_x_start + 1;
	fmt_hsize = fmt_hsize >> h_avg;

	set_dcntr_grid_mif(mif_x_start,	// u32   x_start,
			   mif_x_end,	// u32   x_end,
			   mif_y_start,	// u32   y_start,
			   mif_y_end,	// u32   y_end,
			   src_fmt,	// u32   mode,
			   canvas_id0,	// u32   canvas_addr0,
			   canvas_id1,	// u32   canvas_addr1,
			   canvas_id2,	// u32   canvas_addr2,
			   pic_struct,	// u32   pic_struct)
			   h_avg);

	set_dcntr_grid_fmt(hfmt_en,	// u32 hfmt_en,
			   hz_yc_ratio,	// u32 hz_yc_ratio,        //2bit
			   0,	// u32 hz_ini_phase,       //4bit
			   vfmt_en,	// u32 vfmt_en,
			   vt_yc_ratio,	// u32 vt_yc_ratio,        //2bit
			   0,	// u32 vt_ini_phase,       //4bit
			   fmt_hsize	// u32 y_length
	    );
}

void dcntr_grid_wrmif(int mif_index,  //0:cds  1:grd   2:yds
	int mem_mode,  //0:linear address mode  1:canvas mode
	int src_fmt,  // linear mode : 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
		// canvas mode : 1 = RGB/YCBCR (3 bytes/pixel), 0 = 422 (2 bytes/pixel)
	int canvas_id,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse,  //0 : no reverse
	u64 linear_baddr,
	int linear_length)
{
//cav_con  canvas;
	u32 INT_WMIF_CTRL1;
	u32 INT_WMIF_CTRL2;
	u32 INT_WMIF_CTRL3;
	u32 INT_WMIF_CTRL4;
	u32 INT_WMIF_SCOPE_X;
	u32 INT_WMIF_SCOPE_Y;

	u32 src_hsize = mif_x_end - mif_x_start + 1;
	u32 src_vsize = mif_y_end - mif_y_start + 1;

	if (mif_index == 0) {
		INT_WMIF_CTRL1 = DCNTR_CDS_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_CDS_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_CDS_WMIF_CTRL3;
		INT_WMIF_CTRL4 = DCNTR_CDS_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_CDS_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_CDS_WMIF_SCOPE_Y;
	} else if (mif_index == 1) {
		INT_WMIF_CTRL1 = DCNTR_GRD_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_GRD_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_GRD_WMIF_CTRL3;
		INT_WMIF_CTRL4 = DCNTR_GRD_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_GRD_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_GRD_WMIF_SCOPE_Y;
	} else {
		INT_WMIF_CTRL1 = DCNTR_YDS_WMIF_CTRL1;
		INT_WMIF_CTRL2 = DCNTR_YDS_WMIF_CTRL2;
		INT_WMIF_CTRL3 = DCNTR_YDS_WMIF_CTRL3;
		INT_WMIF_CTRL4 = DCNTR_YDS_WMIF_CTRL4;
		INT_WMIF_SCOPE_X = DCNTR_YDS_WMIF_SCOPE_X;
		INT_WMIF_SCOPE_Y = DCNTR_YDS_WMIF_SCOPE_Y;
	}

	// set canvas
//    if(mem_mode==1){
//        canvas.start_addr = (canvas_baddr >> 3);
//        canvas.cav_width  = ((src_hsize*(2+src_fmt) + 31) >> 5)<<2;
//        canvas.cav_hight  =   src_vsize;
//        canvas.x_wrap_en  = 0;
//        canvas.y_wrap_en  = 0;
//        canvas.blk_mode   = 0;
//        config_cav_lut(canvas_id, canvas);
//    }

	wr(INT_WMIF_CTRL1, (0 << 24) |	//reg_sync_sel
	   (canvas_id << 16) |	//reg_canvas_id
	   (1 << 12) |		//reg_cmd_intr_len
	   (1 << 10) |		//reg_cmd_req_size
	   (2 << 8) |		//reg_burst_len
	   (1 << 7) |		//reg_swap_64bit
	   (0 << 6) |		//reg_little_endian
	   (0 << 5) |		//reg_y_rev
	   (0 << 4) |		//reg_x_rev
	   (src_fmt << 0));	//reg_pack_mode
//            WMIF_CTRL2 : begin
//                reg_sw_rst     <= am_spdat[31:30];
//                reg_int_clr    <= am_spdat[21:20];
//                reg_gclk_ctrl  <= am_spdat[19:18];
//                reg_urgent_ctrl<= am_spdat[16:0];
//            end
	wr(INT_WMIF_CTRL3, ((mem_mode == 0) << 16) |	//reg_acc_mode  [16];
	   (linear_length << 0));	//reg_stride    [12:0];
	wr(INT_WMIF_CTRL4, linear_baddr >> 4);	//
	wr(INT_WMIF_SCOPE_X, (mif_x_end << 16) | mif_x_start);
	wr(INT_WMIF_SCOPE_Y, (mif_y_end << 16) | mif_y_start);
}

void dcntr_post_rdmif(int mif_index,  //0:divr  1:grid  2:yflt  3:cflt
	int mem_mode,  //0:linear address mode  1:canvas mode
	int canvas_id,
	int src_fmt, // 0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse, // 0 : no reverse
	int vstep, //
	u64 linear_baddr,
	int linear_length)
{
	u32 INT_RMIF_CTRL1;
	u32 INT_RMIF_CTRL2;
	u32 INT_RMIF_CTRL3;
	u32 INT_RMIF_CTRL4;
	u32 INT_RMIF_SCOPE_X;
	u32 INT_RMIF_SCOPE_Y;

	if (mif_index == 0) {
		INT_RMIF_CTRL1 = DCNTR_DIVR_RMIF_CTRL1;
		INT_RMIF_CTRL2 = DCNTR_DIVR_RMIF_CTRL2;
		INT_RMIF_CTRL3 = DCNTR_DIVR_RMIF_CTRL3;
		INT_RMIF_CTRL4 = DCNTR_DIVR_RMIF_CTRL4;
		INT_RMIF_SCOPE_X = DCNTR_DIVR_RMIF_SCOPE_X;
		INT_RMIF_SCOPE_Y = DCNTR_DIVR_RMIF_SCOPE_Y;
	} else if (mif_index == 1) {
		INT_RMIF_CTRL1 = DCNTR_GRID_RMIF_CTRL1;
		INT_RMIF_CTRL2 = DCNTR_GRID_RMIF_CTRL2;
		INT_RMIF_CTRL3 = DCNTR_GRID_RMIF_CTRL3;
		INT_RMIF_CTRL4 = DCNTR_GRID_RMIF_CTRL4;
		INT_RMIF_SCOPE_X = DCNTR_GRID_RMIF_SCOPE_X;
		INT_RMIF_SCOPE_Y = DCNTR_GRID_RMIF_SCOPE_Y;
	} else if (mif_index == 2) {
		INT_RMIF_CTRL1 = DCNTR_YFLT_RMIF_CTRL1;
		INT_RMIF_CTRL2 = DCNTR_YFLT_RMIF_CTRL2;
		INT_RMIF_CTRL3 = DCNTR_YFLT_RMIF_CTRL3;
		INT_RMIF_CTRL4 = DCNTR_YFLT_RMIF_CTRL4;
		INT_RMIF_SCOPE_X = DCNTR_YFLT_RMIF_SCOPE_X;
		INT_RMIF_SCOPE_Y = DCNTR_YFLT_RMIF_SCOPE_Y;
	} else {		// mif_index == 3
		INT_RMIF_CTRL1 = DCNTR_CFLT_RMIF_CTRL1;
		INT_RMIF_CTRL2 = DCNTR_CFLT_RMIF_CTRL2;
		INT_RMIF_CTRL3 = DCNTR_CFLT_RMIF_CTRL3;
		INT_RMIF_CTRL4 = DCNTR_CFLT_RMIF_CTRL4;
		INT_RMIF_SCOPE_X = DCNTR_CFLT_RMIF_SCOPE_X;
		INT_RMIF_SCOPE_Y = DCNTR_CFLT_RMIF_SCOPE_Y;
	}

	wr(INT_RMIF_CTRL1, (0 << 24) |	//reg_sync_sel      <= am_spdat[25:24];
	   (canvas_id << 16) |	//reg_canvas_id     <= am_spdat[23:16];
	   (1 << 12) |		//reg_cmd_intr_len  <= am_spdat[14:12];
	   (1 << 10) |		//reg_cmd_req_size  <= am_spdat[11:10];
	   (2 << 8) |		//reg_burst_len     <= am_spdat[9:8];
	   (0 << 7) |		//reg_swap_64bit    <= am_spdat[7];
	   (1 << 6) |		//reg_little_endian <= am_spdat[6];
	   (0 << 5) |		//reg_y_rev         <= am_spdat[5];
	   (0 << 4) |		//reg_x_rev         <= am_spdat[4];
	   (src_fmt << 0));	//reg_pack_mode     <= am_spdat[2:0];
	wr(INT_RMIF_CTRL2, (0 << 30) |	//reg_sw_rst      <= am_spdat[31:30];
	   (vstep << 22) |	//reg_vstep       <= am_spdat[25:22];
	   (0 << 20) |		//reg_int_clr     <= am_spdat[21:20];
	   (0 << 18) |		//reg_gclk_ctrl   <= am_spdat[19:18];
	   (0 << 0));		//reg_urgent_ctrl <= am_spdat[16:0];
	wr(INT_RMIF_CTRL3, ((mem_mode == 0) << 16) | (linear_length & 0x1fff));
	wr(INT_RMIF_CTRL4, linear_baddr >> 4);
	wr(INT_RMIF_SCOPE_X, (mif_x_end << 16) | mif_x_start);
	wr(INT_RMIF_SCOPE_Y, (mif_y_end << 16) | mif_y_start);
}

void dcntr_post_rdmif_block(int mif_index,	//0:divr  1:grid  2:yflt  3:cflt
			    int block_mode)	//0:linear address mode  1:canvas mode
{
	u32 INT_RMIF_CTRL3;

	if (mif_index == 0) {
		INT_RMIF_CTRL3 = DCNTR_DIVR_RMIF_CTRL3;
	} else if (mif_index == 1) {
		INT_RMIF_CTRL3 = DCNTR_GRID_RMIF_CTRL3;
	} else if (mif_index == 2) {
		INT_RMIF_CTRL3 = DCNTR_YFLT_RMIF_CTRL3;
	} else {		// mif_index == 3
		INT_RMIF_CTRL3 = DCNTR_CFLT_RMIF_CTRL3;
	}

	w_reg_bit(INT_RMIF_CTRL3, (block_mode << 1) | 1, 13, 3);

//   RMIF_CTRL3 :
//       reg_hold_en    <= am_spdat[30];
//       reg_pass_num   <= am_spdat[29:24];
//       reg_hold_num   <= am_spdat[23:18];
//       reg_acc_mode   <= am_spdat[16];
//       reg_block_mode <= am_spdat[15:14];
//       reg_32b_align  <= am_spdat[13];
//       reg_stride     <= am_spdat[12:0];
}
#endif

void dcntr_pst_slc(struct DCNTR_SLC *dcntr_slc, u32 dpe_pad, u32 *dpe_pad_l,
	u32 *dpe_pad_r, u32 frm_hsize_sel, u32 slc_num, u32 dsx, u32 dsy, u32 dct_ahead_dv_mode)
{
	int i;
	u32 hsize = dcntr_slc->hsize;
	u32 vsize = dcntr_slc->vsize;
	u32 olap = dpe_pad;
	u32 *olap_l = dpe_pad_l;//dcntr_slc->olap ;
	u32 *olap_r = dpe_pad_r;
	//u32 olap_small = olap+40;
	//u32 nr_di_en;
	//nr_di_en = !(dcntr_slc->nr_di_bypass);
	enum vid_src_fmt fmt = dcntr_slc->ds_fmt; //0:444 1:422 2:420
	enum vid_src_fmt bit_width = dcntr_slc->ds_bit;
	enum vid_src_fmt plane = dcntr_slc->ds_plane;
	//0: ds 1: no ds, 1080i, 2: no ds, 1080p no small pic
	u32 ds_mode = dcntr_slc->ds_mode;
	u32 grid_num_mode = dcntr_slc->grid_num_mode;
	u32 grid_stride = 0;
	u32 cds_stride_sel = (fmt == YUV444) ? 0 : 1;
	bool frame_mode = dcntr_slc->frame_mode & 0x1;

	dbg_h2("%s start frame_mode:%d\n", __func__, frame_mode);
	dbg_h2("%s:ds_mode=%d,grid_num_mode=%d,cds_stride_sel=%d\n",
		__func__, ds_mode, grid_num_mode, cds_stride_sel);
	dbg_h2("%s:frame_mode=%d,slc_num=%d, dsx=%d, dsy=%d\n",
		__func__, frame_mode, slc_num, dsx, dsy);

	ini_dcntr_post(hsize, vsize, grid_num_mode, dsx, dsy, &grid_stride);

	//divr, yflt, cflt, grd rdmif enable
	w_reg_bit(DPSS_DPE_TOP_CTRL0, 1, 15, 1);

	//only grid stride need config, other calc automatically
	w_reg_bit(DCNTR_SLC_TOP_CTRL, grid_stride, 16, 13);
	w_reg_bit(DCNTR_SLC_TOP_CTRL, cds_stride_sel, 4, 1);

	u32 slc_xbgn[4];
	u32 slc_xend[4];
	//ary no use   u32 divr_xbgn[4];
	//ary no use    u32 divr_xend[4];
	u32 yflt_xbgn[4];
	u32 yflt_xend[4];
	u32 map_xbgn[4];
	u32 map_xend[4];
	u32 yflt_xbgn_no_ds[4];
	u32 yflt_xend_no_ds[4];
	u32 cflt_xbgn[4];
	u32 cflt_xend[4];
	u32 grid_xbgn[4];
	u32 grid_xend[4];
	u32 grid_xend_t[4];
	u32 grid_hscnt[4];
	u32 skip_line = 0;
	//u32 slc_num    = frame_mode ? 1 : hsize>1920 ? 4 : hsize > 1024 ? 2 : 1;
	u32 ds_in_rate_x = dsx;
	u32 ds_in_rate_y = dsy;
	u32 ds_in_sft_x  = (1 << ds_in_rate_x) - 1;
	u32 ds_in_sft_y  = (1 << ds_in_rate_y) - 1;
	u32 ds_hsize  = (hsize + ds_in_sft_x) >> ds_in_rate_x;
	u32 ds_vsize  = (vsize + ds_in_sft_y) >> ds_in_rate_y;

	ds_vsize = (ds_vsize + 3) / 4 * 4; // =me_vsize
	ds_hsize = (ds_hsize + 3) / 4 * 4; // =me_hsize
	w_reg_bit(DCNTR_SLC_TOP_CTRL, slc_num, 0, 3);

	calc_slc_info(hsize, frm_hsize_sel, slc_num,
		&slc_xbgn[0], &slc_xend[0],
		&slc_xbgn[1], &slc_xend[1],
		&slc_xbgn[2], &slc_xend[2],
		&slc_xbgn[3], &slc_xend[3]);

	if (slc_num == 4) {
		if (dct_ahead_dv_mode) {
			slc_xend[0] = slc_xend[0] + olap_r[0];
			slc_xbgn[1] = slc_xbgn[1] - olap_l[1];
			slc_xend[1] = slc_xend[1] + olap_r[1];
			slc_xbgn[2] = slc_xbgn[2] - olap_l[2];
			slc_xend[2] = slc_xend[2] + olap_r[2];
			slc_xbgn[3] = slc_xbgn[3] - olap_l[3];
			wr(DCNTR_SLC_LPAD, (olap_l[3] << 24) |
				(olap_l[2] << 16) | (olap_l[1] << 8) | 0);
			wr(DCNTR_SLC_RPAD, (0 << 24) | (olap_r[2] << 16) |
				(olap_r[1] << 8) | olap_r[0]);
		} else {
			slc_xend[0] = slc_xend[0] + olap;
			slc_xbgn[1] = slc_xbgn[1] - olap;
			slc_xend[1] = slc_xend[1] + olap;
			slc_xbgn[2] = slc_xbgn[2] - olap;
			slc_xend[2] = slc_xend[2] + olap;
			slc_xbgn[3] = slc_xbgn[3] - olap;
			wr(DCNTR_SLC_LPAD, (olap << 24) | (olap << 16) | (olap << 8) | 0);
			wr(DCNTR_SLC_RPAD, (0 << 24) | (olap << 16) | (olap << 8) | olap);
		}
	} else if (slc_num == 2) {
		if (dct_ahead_dv_mode) {
			slc_xend[0] = slc_xend[0] + olap_r[0];
			slc_xbgn[1] = slc_xbgn[1] - olap_l[1];
			wr(DCNTR_SLC_LPAD, (olap_l[1] << 8) | 0);
			wr(DCNTR_SLC_RPAD, (0    << 8) | olap_r[0]);
		} else {
			slc_xend[0] = slc_xend[0] + olap;
			slc_xbgn[1] = slc_xbgn[1] - olap;
			wr(DCNTR_SLC_LPAD, (olap << 8) | 0);
			wr(DCNTR_SLC_RPAD, (0    << 8) | 0);
		}
	} else {
		wr(DCNTR_SLC_LPAD, 0);
		wr(DCNTR_SLC_RPAD, 0);
	}

	wr(DCNTR_SLC_XIDX0, (slc_xbgn[0] << 16) | slc_xend[0]);
	wr(DCNTR_SLC_XIDX1, (slc_xbgn[1] << 16) | slc_xend[1]);
	wr(DCNTR_SLC_XIDX2, (slc_xbgn[2] << 16) | slc_xend[2]);
	wr(DCNTR_SLC_XIDX3, (slc_xbgn[3] << 16) | slc_xend[3]);

	u32 slc_lpad;
	u32 slc_rpad;
	u32 xbgn_abs;
	u32 xend_abs;
	//ary no use    u32 divr_lext;
	//ary no use    u32 divr_rext;
	u32 yflt_lext;
	u32 yflt_rext;
	u32 yflt_pad;

	for (i = 0; i < slc_num; i++) {
		//olap;//slc mdoe always cut 8 for big pic
		slc_lpad = i == 0 ? 0 : 8;
		slc_rpad = i == slc_num - 1 ? 0 : 8;//olap;
		xbgn_abs = slc_xbgn[i] + slc_lpad;
		xend_abs = slc_xend[i] - slc_rpad;
		//avg calc = max 16pixel(orign pic)
		yflt_pad = 16;//(ds_in_rate_x == 1) ? 2*7 : //need 7
		//(ds_in_rate_x==2) ? 4*3 : //need 3
		//                    0   ;
		//upsample(need 1 pix)-->upsample(need 1 pix)(ds_in_rate_x == 1)
		//-->avg_flt(need 2 + 1 = 3pixl)
		//-->shrk(need 1 + 6 pix)(only ds_in_rate_x == 1)
		//-->yflt_in(need 7 pix)
		//marked for coverity
		//yflt_lext = slc_lpad > yflt_pad ? slc_xbgn[i] :
		//	xbgn_abs > yflt_pad ? xbgn_abs - yflt_pad : 0;
		yflt_lext = xbgn_abs > yflt_pad ? xbgn_abs - yflt_pad : 0;
		yflt_xbgn[i] = (yflt_lext >> ds_in_rate_x) >> 1 << 1;
		yflt_xbgn_no_ds[i] = yflt_lext >> 1 << 1;
		//yflt_xbgn[i] = (xbgn_abs>>ds_in_rate_x-4)>>1<<1;
		//yflt_xbgn_no_ds[i] = (slc_xbgn[i]-16)>>1<<1;

		//marked for coverity
		//yflt_rext = slc_rpad > yflt_pad ? slc_xend[i] :
		//xend_abs + yflt_pad > hsize - 1 ? hsize - 1 : xend_abs + yflt_pad;
		yflt_rext = xend_abs + yflt_pad > hsize - 1 ? hsize - 1 : xend_abs + yflt_pad;
		yflt_xend[i] = ((yflt_rext + 1 + ds_in_sft_x) >> ds_in_rate_x) - 1;
		yflt_xend_no_ds[i] = yflt_rext;
		//yflt_xend[i] = ((xend_abs+1+ds_in_sft_x)>>ds_in_rate_x+4)-1;
		//yflt_xend_no_ds[i] = ;
	}

	wr(DCNTR_SLC_YFLT_XIDX0, (yflt_xbgn[0] << 16) | yflt_xend[0]);
	wr(DCNTR_SLC_YFLT_XIDX1, (yflt_xbgn[1] << 16) | yflt_xend[1]);
	wr(DCNTR_SLC_YFLT_XIDX2, (yflt_xbgn[2] << 16) | yflt_xend[2]);
	wr(DCNTR_SLC_YFLT_XIDX3, (yflt_xbgn[3] << 16) | yflt_xend[3]);
	wr(DCNTR_SLC_YFLT_YIDX3, (0 << 16) | (ds_vsize - 1));

	cflt_xbgn[0] = fmt == YUV444 ? yflt_xbgn[0] :
		(yflt_xbgn[0]) / 2;
	cflt_xend[0] = fmt == YUV444 ? yflt_xend[0] :
		(yflt_xend[0] + 1 + 1) / 2 - 1;
	cflt_xbgn[1] = fmt == YUV444 ? yflt_xbgn[1] :
		(yflt_xbgn[1]) / 2;
	cflt_xend[1] = fmt == YUV444 ? yflt_xend[1] :
		(yflt_xend[1] + 1 + 1) / 2 - 1;
	cflt_xbgn[2] = fmt == YUV444 ? yflt_xbgn[2] :
		(yflt_xbgn[2]) / 2;
	cflt_xend[2] = fmt == YUV444 ? yflt_xend[2] :
		(yflt_xend[2] + 1 + 1) / 2 - 1;
	cflt_xbgn[3] = fmt == YUV444 ? yflt_xbgn[3] :
		(yflt_xbgn[3]) / 2;
	cflt_xend[3] = fmt == YUV444 ? yflt_xend[3] :
		(yflt_xend[3] + 1 + 1) / 2 - 1;

	wr(DCNTR_SLC_CFLT_XIDX0, (cflt_xbgn[0] << 16) | cflt_xend[0]);
	wr(DCNTR_SLC_CFLT_XIDX1, (cflt_xbgn[1] << 16) | cflt_xend[1]);
	wr(DCNTR_SLC_CFLT_XIDX2, (cflt_xbgn[2] << 16) | cflt_xend[2]);
	wr(DCNTR_SLC_CFLT_XIDX3, (cflt_xbgn[3] << 16) | cflt_xend[3]);
	if (fmt != YUV420)
		wr(DCNTR_SLC_CFLT_YIDX3, (0 << 16) | (ds_vsize - 1));
	else
		wr(DCNTR_SLC_CFLT_YIDX3, (0 << 16) | ((ds_vsize + 1) / 2 - 1));

	u32 ycflt_hsize;
	u32 ycflt_vsize;
	u32 hds_rate_x;

	if (ds_mode == 0) { //have small pic
		wr(DCNTR_SLC_FLT_XIDX0, (yflt_xbgn[0] << 16) | yflt_xend[0]);
		wr(DCNTR_SLC_FLT_XIDX1, (yflt_xbgn[1] << 16) | yflt_xend[1]);
		wr(DCNTR_SLC_FLT_XIDX2, (yflt_xbgn[2] << 16) | yflt_xend[2]);
		wr(DCNTR_SLC_FLT_XIDX3, (yflt_xbgn[3] << 16) | yflt_xend[3]);
		wr(DCNTR_SLC_FLT_YIDX3, (0 << 16) | (ds_vsize - 1));

		ycflt_hsize = ds_hsize;
		ycflt_vsize = ds_vsize;
		w_reg_bit(DCNTR_SLC_TOP_CTRL1, 0, 0, 2); //reg_flt_hds_en
	} else if (ds_mode == 1) { //1080i
		wr(DCNTR_SLC_FLT_XIDX0, (yflt_xbgn_no_ds[0] << 16) | yflt_xend_no_ds[0]);
		wr(DCNTR_SLC_FLT_XIDX1, (yflt_xbgn_no_ds[1] << 16) | yflt_xend_no_ds[1]);
		wr(DCNTR_SLC_FLT_XIDX2, (yflt_xbgn_no_ds[2] << 16) | yflt_xend_no_ds[2]);
		wr(DCNTR_SLC_FLT_XIDX3, (yflt_xbgn_no_ds[3] << 16) | yflt_xend_no_ds[3]);
		wr(DCNTR_SLC_FLT_YIDX3, (0 << 16) | (vsize - 1));

		ycflt_hsize = hsize;
		ycflt_vsize = vsize;

		w_reg_bit(DCNTR_SLC_TOP_CTRL1, 1, 0, 2); //reg_flt_hds_en
	} else { //1080p or 4k no small pic
		wr(DCNTR_SLC_FLT_XIDX0, (yflt_xbgn_no_ds[0] << 16) | yflt_xend_no_ds[0]);
		wr(DCNTR_SLC_FLT_XIDX1, (yflt_xbgn_no_ds[1] << 16) | yflt_xend_no_ds[1]);
		wr(DCNTR_SLC_FLT_XIDX2, (yflt_xbgn_no_ds[2] << 16) | yflt_xend_no_ds[2]);
		wr(DCNTR_SLC_FLT_XIDX3, (yflt_xbgn_no_ds[3] << 16) | yflt_xend_no_ds[3]);
		wr(DCNTR_SLC_FLT_YIDX3, (0 << 16) | (vsize - 1));
		dbg_h2("%s:addr: PIC_ORG_Y_BADDR -> DPSS_SRC0_IDW_YBUF_ADDR\n", __func__);
		//        w_reg_bit(DPSS_SRC0_IDW_YBUF_ADDR,PIC_ORG_Y_BADDR>>4,0,32);
		//        w_reg_bit(DPSS_SRC0_IDW_CBUF_ADDR,PIC_ORG_C_BADDR>>4,0,32);
		//       w_reg_bit(DPSS_SRC0_IDW_YBUF_STEP,PIC_ORG_Y_OFST>>4,0,32);
		//       w_reg_bit(DPSS_SRC0_IDW_CBUF_STEP,PIC_ORG_C_OFST>>4,0,32);
		//1205 dcntr_cfg_baddr();
		ycflt_hsize = hsize;
		ycflt_vsize = vsize;
		skip_line   = ds_mode == 2 ? 1 : 3;
		hds_rate_x  = ds_mode == 2 ? 1 : 2;
		w_reg_bit(DCNTR_SLC_TOP_CTRL1, hds_rate_x, 0, 2); //reg_flt_hds_en
	}

	map_xbgn[0] = ((slc_xbgn[0]) >> ds_in_rate_x) >> 2 << 2;
	map_xend[0] = ((slc_xend[0] + 1 + ds_in_sft_x) >> ds_in_rate_x) - 1;
	map_xbgn[1] = ((slc_xbgn[1]) >> ds_in_rate_x) >> 2 << 2;
	map_xend[1] = ((slc_xend[1] + 1 +  ds_in_sft_x) >> ds_in_rate_x) - 1;
	map_xbgn[2] = ((slc_xbgn[2]) >> ds_in_rate_x) >> 2 << 2;
	map_xend[2] = ((slc_xend[2] + 1 + ds_in_sft_x) >> ds_in_rate_x) - 1;
	map_xbgn[3] = ((slc_xbgn[3]) >> ds_in_rate_x) >> 2 << 2;
	map_xend[3] = ((slc_xend[3] + 1 + ds_in_sft_x) >> ds_in_rate_x) - 1;

	wr(DCNTR_SLC_MAP_XIDX0, (map_xbgn[0] << 16) | map_xend[0]);
	wr(DCNTR_SLC_MAP_XIDX1, (map_xbgn[1] << 16) | map_xend[1]);
	wr(DCNTR_SLC_MAP_XIDX2, (map_xbgn[2] << 16) | map_xend[2]);
	wr(DCNTR_SLC_MAP_XIDX3, (map_xbgn[3] << 16) | map_xend[3]);
	wr(DCNTR_SLC_MAP_YIDX3, (0 << 16) | (ds_vsize - 1));
	//=============dpe ycflt rdmif=============
	struct PRM_INTF_TYPE dct_ycflt_intf;

	memset((void *)(&dct_ycflt_intf), 0, sizeof(struct PRM_INTF_TYPE));

	dct_ycflt_intf.src_hsize = ycflt_hsize;
	dct_ycflt_intf.src_vsize = ycflt_vsize;
	dct_ycflt_intf.src_bit = bit_width;
	dct_ycflt_intf.src_plan = plane;
	dct_ycflt_intf.src_fmt = fmt;
	dct_ycflt_intf.skip_line = skip_line;
	dct_ycflt_intf.uv_swap = dcntr_slc->uv_swap;
	//dct_ycflt_intf.reverse[0]    = 0;
	//dct_ycflt_intf.reverse[1]    = 0;
	//dct_ycflt_intf.src_baddr[0] = prm_dae.dae_yuv_mif.src_baddr[0];
	//dct_ycflt_intf.src_baddr[1] = prm_dae.dae_yuv_mif.src_baddr[1];

	//dct_ycflt_intf.slc_x_st[0]  = 0;
	//dct_ycflt_intf.slc_x_ed[0]  = mix_hsize -1;
	//dct_ycflt_intf.slc_y_st[0]  = 0;
	//dct_ycflt_intf.slc_y_ed[0]  = mix_vsize -1;

	//dct_ycflt_intf.bits_mode  = prm_dae.dae_yuv_mif.bits_mode;
	//dct_ycflt_intf.pack_mode  = prm_dae.dae_yuv_mif.pack_mode;

	dct_ycflt_intf.mmu_mode_en = dcntr_slc->mif_mmu_mode_en;
	dct_ycflt_intf.swap_64bit =  dcntr_slc->swap_64bit;
	dct_ycflt_intf.block_mode = dcntr_slc->block_mode;
	dct_ycflt_intf.canvas_hsize = dcntr_slc->canvas_hsize;
	dpss_cfg_viu_pix_rdmif(&dct_ycflt_intf, 2);

	u32 reg_grid_xsize     = rd(DCTR_BGRID_PARAM2) >> 24;
	//ary no use   u32 reg_grid_xsize_ds  = rd(DCTR_BGRID_PARAM4)>>16;
	u32 reg_grid_xnum_use  = rd(DCTR_BGRID_PARAM3) >> 16;
	u32 reg_grid_ynum_use  = rd(DCTR_BGRID_PARAM3) & 0xffff;

	//u32 grid_xsize = ds_in_rate_x==0 ? reg_grid_xsize : reg_grid_xsize_ds;

	for (i = 0; i < 4; i = i + 1) {
		grid_xbgn[i] = (slc_xbgn[i]) / reg_grid_xsize;
		if (i + 1 >= slc_num) {
			grid_xend[i] = reg_grid_xnum_use - 1;
		} else {
			grid_xend_t[i] = (slc_xend[i] + reg_grid_xsize - 1) / reg_grid_xsize;
			grid_xend[i]   = grid_xend_t[i] > (reg_grid_xnum_use - 1)
					 ? (reg_grid_xnum_use - 1)
					 : grid_xend_t[i];
		}
		grid_hscnt[i] = slc_xbgn[i] - slc_xbgn[i] / reg_grid_xsize * reg_grid_xsize;
	}

	dbg_h2("slc_xend_0=%d reg_grid_xsize= %0d\n", slc_xend[0], reg_grid_xsize);
	dbg_h2("grid_xend_0=%d\n", grid_xend[0]);

	wr(DCNTR_SLC_GRID_XIDX0, (grid_xbgn[0] << 16) | grid_xend[0]);
	wr(DCNTR_SLC_GRID_XIDX1, (grid_xbgn[1] << 16) | grid_xend[1]);
	wr(DCNTR_SLC_GRID_XIDX2, (grid_xbgn[2] << 16) | grid_xend[2]);
	wr(DCNTR_SLC_GRID_XIDX3, (grid_xbgn[3] << 16) | grid_xend[3]);
	wr(DCNTR_SLC_GRID_YIDX3, (0 << 16) | (reg_grid_ynum_use - 1));
	wr(DCNTR_SLC_GRID_HSCNT, (grid_hscnt[3] << 24) | (grid_hscnt[2] << 16) |
		(grid_hscnt[1] << 8) | grid_hscnt[0]);

	bool dcntr_out_win_en;
	u32 dcntr_out_win_xbgn[4];
	u32 dcntr_out_win_xend[4];
	u32 dcntr_out_win_ybgn;
	u32 dcntr_out_win_yend;
	u32 olap_cut;

	//window cut
	if (slc_num == 1)
		dcntr_out_win_en = 0;
	else
		dcntr_out_win_en = 1;

	//if (nr_di_en)
	//    olap_cut = slc_num==1 ? 0 : olap - 64;
	//else

	olap_cut = 8;

	//nr overlap = 64
	dcntr_out_win_xbgn[0] = slc_xbgn[0];
	dcntr_out_win_xend[0] = slc_xend[0] - slc_xbgn[0] - olap_cut;
	dcntr_out_win_xbgn[1] = olap_cut;
	dcntr_out_win_xend[1] =
		(slc_num == 4) ? slc_xend[1] - slc_xbgn[1] - olap_cut
		: slc_xend[1] - slc_xbgn[1];
	dcntr_out_win_xbgn[2] = olap_cut;
	dcntr_out_win_xend[2] = slc_xend[2] - slc_xbgn[2] - olap_cut;
	dcntr_out_win_xbgn[3] = olap_cut;
	dcntr_out_win_xend[3] = slc_xend[3] - slc_xbgn[3];

	dcntr_out_win_ybgn = 0;
	dcntr_out_win_yend = vsize - 1;

	//if(slc_num==1)
	//    w_reg_bit(REG_DCTR_WIN_CTRL0, dcntr_out_win_en, 26, 1);
	w_reg_bit(REG_DCTR_WIN_CTRL0, dcntr_out_win_en, 26, 1);

	wr(REG_DCTR_WIN_CTRL1, dcntr_out_win_ybgn << 13 | dcntr_out_win_yend);
	wr(DCNTR_SLC_WIN_XIDX0, (dcntr_out_win_xbgn[0] << 16) |
		dcntr_out_win_xend[0]);
	wr(DCNTR_SLC_WIN_XIDX1, (dcntr_out_win_xbgn[1] << 16) |
		dcntr_out_win_xend[1]);
	wr(DCNTR_SLC_WIN_XIDX2, (dcntr_out_win_xbgn[2] << 16) |
		dcntr_out_win_xend[2]);
	wr(DCNTR_SLC_WIN_XIDX3, (dcntr_out_win_xbgn[3] << 16) |
		dcntr_out_win_xend[3]);
	dbg_h2("%s end %d\n", __func__, dcntr_out_win_en);

	//if(slc_num!=1) w_reg_bit(DCTR_RAND_EN, 3, 2, 2); //dither mode for bitmatch
}

#ifdef MOV	//ary 2025-03-23
void dcntr_cfg_baddr(void)
{
	u32 pic_org_y_baddr = PIC_ORG_Y_BADDR >> 4;
	u32 pic_org_c_baddr = PIC_ORG_C_BADDR >> 4;
	u32 pic_org_y_step  = PIC_ORG_Y_OFST >> 4;
	u32 pic_org_c_step  = PIC_ORG_C_OFST >> 4;
	int i;
	u32 pic_org_y_raddr[16];
	u32 pic_org_c_raddr[16];

	for (i = 0; i < 16; i++) {
		pic_org_y_raddr[i] = (pic_org_y_baddr + i * pic_org_y_step) >> 5;
		pic_org_c_raddr[i] = (pic_org_c_baddr + i * pic_org_c_step) >> 5;
		wr(DPSS_SRC0_DWBUF_YADDR0 + i, pic_org_y_raddr[i]);
		//ary addr * 4
		wr(DPSS_SRC0_DWBUF_CADDR0 + i, pic_org_c_raddr[i]);
		//ary addr * 4
	}
}
#endif

#else /* FUNC_EN_DCT */
void dpss_ini_dcntr_pre(int hsize, int vsize, int grd_num_mode,
	int in_fmt_mode, int grd_444to422_mode, int grd_422to420_mode, int dsx, int dsy)

{
}

void dcntr_pst_slc(struct DCNTR_SLC *dcntr_slc, u32 dpe_pad, int frm_hsize_sel, int slc_num,
	int dsx, int dsy)
{
}

#endif /* FUNC_EN_DCT */
