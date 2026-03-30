/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DECONTOUR_H_
#define _DECONTOUR_H_

void dpss_ini_dcntr_pre(int hsize, int vsize, int grd_num_mode, int
	in_fmt_mode, int grd_444to422_mode, int grd_422to420_mode, int dsx, int dsy);
void ini_dcntr_post(int hsize, int vsize, int grd_num_mode, int dsx, int dsy, u32 *grid_stride);

void set_dcntr_grid_mif(u32 x_start,
	u32 x_end,
	u32 y_start,
	u32 y_end,
	// 1 = RGB/YCBCR (3 bytes per pixel)
	u32 mode,
	// 0 = 4:2:2 (2 bytes per pixel)
	u64 canvas_addr0,
	// 2:420,two canvas(nv21) 3:420,three canvas
	u64 canvas_addr1,
	u64 canvas_addr2,
	// 00: frame, 10: top_field, 11: bot_field
	u32 pic_struct,
	u32 h_avg);

void set_dcntr_grid_fmt(u32 hfmt_en,
	u32 hz_yc_ratio,
	u32 hz_ini_phase,
	u32 vfmt_en,
	u32 vt_yc_ratio,
	u32 vt_ini_phase,
	u32 y_length);

void dcntr_grid_rdmif(int canvas_id0,
	int canvas_id1,
	int canvas_id2,
	u64 canvas_baddr0,
	u64 canvas_baddr1,
	u64 canvas_baddr2,
	int src_hsize,
	int src_vsize,
	int src_fmt,
// 1 = RGB/YCBCR (3 bytes/pixel)
//0 = 422 (2 bytes/pixel)  2:420,two canvas(nv21)  3:420,three canvas
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	// 0 : no reverse
	int mif_reverse,
	//00: frame, 10: top_field, 11: bot_field
	int pic_struct,
	// 0: no avg   1:y_avg  2:c_avg  3:y&c avg
	int h_avg);

void dcntr_grid_wrmif(int mif_index,//0:cds  1:grd   2:yds
	int mem_mode,//0:linear address mode  1:canvas mode
	int src_fmt,//0:4bit 1:8bit 2:16bit 3:32bit 4:64bit 5:128bit
	int canvas_id,
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse,  //0 : no reverse
	u64 linear_baddr,
	int linear_length);// DDR read length = linear_length*128 bits

void dcntr_post_rdmif(int mif_index,  //0:divr  1:grid  2:yflt  3:cflt
	int mem_mode,  //0:linear address mode  1:canvas mode
	int canvas_id,
	int src_fmt, // 1 = RGB/YCBCR (3 bytes/pixel), 0 = 422 (2 bytes/pixel)
	int mif_x_start,
	int mif_x_end,
	int mif_y_start,
	int mif_y_end,
	int mif_reverse, // 0 : no reverse
	int vstep, //
	u64 linear_baddr,
	int linear_length);

void dcntr_post_rdmif_block(int mif_index, //0:divr  1:grid  2:yflt  3:cflt
	int block_mode); //0:linear address mode  1:canvas mode

struct DCNTR_SLC {
	u32 slc_mode; //0:auto switch 1:vpss control slice switch
	u32 hsize;
	u32 vsize;
	u32 olap;
	enum vid_src_fmt ds_fmt;//0:444 1:422 2:420
	enum vid_src_fmt ds_bit;
	enum vid_src_fmt ds_plane;
	bool nr_di_bypass;
	u32 ds_mode;//0: ds 1: no ds
	u32 grid_num_mode; //0:40x23 1:60x34 2:80x45
	u32 grid_baddr;
	u32 divr_baddr;
	u32 yflt_baddr;
	u32 cflt_baddr;
	u32 grid_stride;
	u32 divr_stride;
	u32 yflt_stride;
	u32 cflt_stride;
	bool frame_mode;
	u32 mif_mmu_mode_en;
	u32 uv_swap;
	u32 swap_64bit;
	u32 canvas_hsize;
	u32 block_mode;
};

void dcntr_pst_slc(struct DCNTR_SLC *dcntr_slc, u32 dpe_pad, u32 *dpe_pad_l,
	u32 *dpe_pad_r, u32 frm_hsize_sel, u32 slc_num, u32 dsx, u32 dsy, u32 dct_ahead_dv_mode);
//ary 2025-03-23	extern void dcntr_cfg_baddr(void);

#endif
