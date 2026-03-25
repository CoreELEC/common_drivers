/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMDV_HW5_T6W_H_
#define _AMDV_HW5_T6W_H_

#include <linux/types.h>

/*copy from dpss_intf.h*/
enum vid_src_fmt {
	YUV444    = 0,
	YUV422    = 1,
	YUV420    = 2,
	RGB       = 3,
	PLANAR_X1 = 5,
	PLANAR_X2 = 6,
	PLANAR_X3 = 7,
	BIT_8       = 10,
	BIT_10      = 11,
	BIT_12      = 12,
	BIT_14      = 13,
	BIT_16      = 14,
	BIT_P010    = 15,//10IN16
	BIT_10IN12  = 16,//10IN12
	CMPR_UN   = 20,
	CMPR_AFBC = 21,
	CMPR_AFRC = 22,
	IS_PSRC   = 30, //progressive src
	IS_ISRC   = 31  //interlace   src
};

struct prm_dolby_top {
	bool       fst_frm_ini;//1:first frm ini
	bool       fst_frm_ini_top2;
	u32        byps_mode;//1:core_internal_byps 2:top2 wrap_byps
	enum dolby_work_mode dolby5_mode;//VDIN/VD1/DPSS
	u32        core1_py_level;//0:6py 1:7py 2:nopy
	u32        core1_src_rbaddr[2];
	u32        core1_pyr_wbaddr[7];
	u32        core1_wdma_wbaddr;
	u32        core1_src_hsize;
	u32        core1_src_vsize;
	enum vid_src_fmt     core1_src_bit;
	enum vid_src_fmt     core1_src_fmt;
	enum vid_src_fmt     core1_src_plane;
	u32        core1_mif_mmu_ena;
	u32        corep_py_level;//0:6py 1:7py 2:nopy
	u32        corep_up_wbaddr[2];//pyr_up_wr,[0]for corep write,[1] for core2 read
	u32        corep_hsize;
	u32        corep_vsize;
	u32        core2_hsize_of_corep;/*core2 size corresponding to the current corep frame*/
	u32        core2_vsize_of_corep;/*update one frame ahead of core2_hsize*/
	u32        core2_detunnel;
	u32        core2_hsize;/*current core2 size*/
	u32        core2_vsize;
	u32        core2_py_level;
	u32        core2_slice_num;
	u32        core2_pad_mode;//1: dpss.nr on,need process nr_ovlp
	u32        py_weight[7];
	u32        frm_hsize_sel;//0:4byte align;3:64byte align;4:128byte align
	bool       dpss_tbc_mode;
	bool       dpss_direct_mode;
	bool       core2_slc_mode;/*for t6x core2 2slice,2 core2*/
	bool       mosaic_mode;/*for t6x 2x2 mode*/
	bool       core2_prl_mode;/*for t6x 2 core2, only write one set regs*/
	u32        vds_4k1k_en;
	bool       dpss_dct_ahead_dv;/*0:dv+dct,1:dct+dv*/
	u32        dpss_real_h;
	u32        dpss_real_v;
};

struct prm_intf_type {
	enum vid_src_fmt src_fmt;//YUV444/YUV422/YUV420/RGB
	enum vid_src_fmt src_plan;//PLANAR_X1/X2/X3
	enum vid_src_fmt src_bit;//BIT_8/10/12
	enum vid_src_fmt src_cmpr;//un/afbc/afrc
	enum vid_src_fmt interlace;//IS_PSRC/IS_ISRC
	u32 src_hsize;
	u32 src_vsize;
	u32 src_baddr[3];//[0]:y/head [1]:u/c/body [2]:v
	u32 y_crop[4];//[0]:x_bg   [1]:x_ed     [2]:y_bg  [3]:y_ed
	u32 u_crop[4];//[0]:x_bg   [1]:x_ed     [2]:y_bg  [3]:y_ed
	u32 v_crop[4];//[0]:x_bg   [1]:x_ed     [2]:y_bg  [3]:y_ed
	u32 reverse[2];//[0]:x      [1]:y
	u32 skip[2];//[0]:x      [1]:y
	u32 bits_mode;//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	u32 pack_mode;//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x
	u32 slc_x_st[4];
	u32 slc_x_ed[4];
	u32 slc_y_st[4];
	u32 slc_y_ed[4];
	u32 stride;
	u32 luma_wr_x_st[4];
	u32 luma_wr_x_ed[4];
	u32 luma_wr_y_st[4];
	u32 luma_wr_y_ed[4];
	u32 chrm_wr_x_st[4];
	u32 chrm_wr_x_ed[4];
	u32 chrm_wr_y_st[4];
	u32 chrm_wr_y_ed[4];
	u32 mmu_mode_en;
	u32 skip_line;//0:no skip 1: skip 1 line 2: skip 2line .. 7:skip 7 line
	u32 pad_en;
	u32 pad_hmode;//0:8 1:16
	u32 pad_vmode;//0:8 1:16
	u32 cut_win_en;
	u32 burst_len;//default set 3
	u32 blk_mode;
	u32 endian;
	u32 linear_mode;
	u32 reg_mode;/*default 0*/
};

extern u32 dump_pyramid;
extern u32 top1_crc_rd;
extern bool fake_top1_on;
extern bool pre_set_done;
extern bool hw5_clk_on;
void cfg_dolby_ini(struct prm_dolby_top *prm_dolby, struct dpss_info_s *dpss_info);
void cfg_dolby_update(struct prm_dolby_top *prm_dolby,
			bool enable_detunnel,
			u32 core2_hsize_for_corep,
			u32 core2_vsize_for_corep,
			u32 hsize,
			u32 vsize,
			u8 update,
			bool reset);
int cfg_dolby_top(struct prm_dolby_top *prm_dolby,
				 u64 *top1_reg,
				 u64 *top1b_reg,
				 u64 *top2_reg,
			     int video_enable,
			     bool reset,
			     bool toggle,
			     bool pr_done,
			     bool set_top1,
			     bool set_top2);
void dump_pyr_up(void);
void cfg_dolby_pre_set(struct prm_dolby_top *prm_dolby);
void cfg_dd_path_off(void);

#endif
