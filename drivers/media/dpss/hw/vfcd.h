/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _VFCD__H_
#define _VFCD__H_

struct AFBCD {
	u32 hsize;
	u32 vsize;
	u32 compbits;
	u32 head_baddr;
	u32 body_baddr;
	u32 fmt_mode;
	u32 ddr_sz_mode;
	u32 fmt444_comb;
	u32 dos_uncomp;
	u32 rot_en;
	u32 rot_hbgn;
	u32 rot_vbgn;
	u32 y_h_skip_en;
	u32 y_v_skip_en;
	u32 c_h_skip_en;
	u32 c_v_skip_en;
	u32 rev_mode;
//    u32  hfmt_en          ;
//    u32  vfmt_en          ;
	u32 lossy_en;
	u32 def_color_y;
	u32 def_color_u;
	u32 def_color_v;
	u32 win_bgn_h;
	u32 win_end_h;
	u32 win_bgn_v;
	u32 win_end_v;
	u32 pip_src_mode;
	u32 rot_vshrk;
	u32 rot_hshrk;
	u32 rot_drop_mode;
	u32 rot_ofmt_mode;
	u32 rot_ocompbit;

	u32 fix_cr_en;
	u32 brst_len_add_en;
	u32 brst_len_add_value;
	u32 ofset_brst4_en;
};

struct AFRCD_TYPE {
	u32 input_fmt_mode;	//0:rgb 1:yuv 2:bayer
	u32 input_bayer_mode;	//0:mono 1: bayer_2x2
	u32 yc_plane_sel;	//0:y_plane_en 1:c_plane_en
	u32 raw_8x16_en;	//when_raw : 1:8x16 for npu  0:2x32

	u64 luma_head_addr;
	u64 luma_body_addr;
	u32 luma_dict_en;
	u32 luma_comp_target;	//12,16,20...48,step =4
	u32 compbits_luma;	//0:8 1:9 2:10 3:12
	u32 luma_header_en;
	u32 luma_cu_bits;
	u32 luma_pixel_bits;

	u64 chrm_head_addr;
	u64 chrm_body_addr;
	u32 chrm_dict_en;
	u32 chrm_comp_target;
	u32 compbits_chrm;
	u32 chrm_header_en;
	u32 chrm_cu_bits;
	u32 chrm_pixel_bits;

	u32 mmu_mode_en;
	u32 mmu_page_mode;
	u32 mmu_baddr0;
	u32 mmu_baddr1;
};

struct VFCD_t {
	u32 cmpr_sel;	//0:bc; 1:rc
	u32 frm_en_sel;	//0:frm_start_in  1:go_field else:inner_reg_start
	u32 fmt444_out;
	u32 src_hsize;
	u32 src_vsize;
	u32 win_bgn_h[4];
	u32 win_end_h[4];
	u32 win_bgn_v[4];
	u32 win_end_v[4];
	u32 luma_head_baddr;
	u32 luma_body_baddr;
	u32 chrm_head_baddr;
	u32 chrm_body_baddr;

	u32 src_fmt;
	u32 rev_mode;
	u32 hfmt_en;
	u32 vfmt_en;
	u32 compbits_y;
	u32 compbits_c;
	u32 y_h_skip_en;
	u32 y_v_skip_en;
	u32 c_h_skip_en;
	u32 c_v_skip_en;
	u32 slc_num;
	u32 film_grain_en;
	u32 pad_en;
	u32 pad_hmode;	//0;alg8 1:alg16
	u32 pad_vmode;	//0;alg8 1:alg16
	u32 mmu_mode_en;
	u32 mmu_page_mode;
	u32 mmu_baddr0;
	u32 mmu_baddr1;
	u32 mif_burst_len;
	u32 yc_rst_sep;
	u32 cut_win_en;
	u32 hfmt_mode;

	bool src_is_i; //0911
	struct AFBCD afbcd;
	struct AFRCD_TYPE afrcd;
	bool adj_offset;
};

void init_dpss_afbcd(int mode, struct AFBCD *prm);
void init_dpss_afrcd(int mode, struct AFRCD_TYPE *prm);
void init_dpss_vfcd(int mode, struct VFCD_t *prm);
void cfg_dpss_afbcd(u32 index, u32 hfmt_en, u32 vfmt_en,
		struct AFBCD *dpss_afbcd);

void cfg_dpss_afbcd_mult(u32 module_sel, u32 hold_line_num,
	u32 blk_mem_mode, u32 compbits_y, u32 compbits_u,
	u32 compbits_v, u32 hsize_in, u32 vsize_in, u32 mif_info_baddr,
	u32 mif_data_baddr, u32 out_horz_bgn, u32 out_vert_bgn,
	u32 out_horz_end, u32 out_vert_end, u32 hz_ini_phase,
	u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 rev_mode, u32 vert_skip_y, u32 horz_skip_y, u32 vert_skip_uv,
	u32 horz_skip_uv, u32 hfmt_en, u32 vfmt_en, u32 def_color_y,
	u32 def_color_u, u32 def_color_v, u32 reg_lossy_en, u32 ddr_sz_mode,
	u32 fmt_mode, u32 reg_fmt444_comb, u32 reg_dos_uncomp_mode,
	u32 pip_src_mode, u32 rot_ofmt_mode, u32 rot_ocompbit, u32 rot_drop_mode,
	u32 rot_vshrk, u32 rot_hshrk, u32 rot_hbgn, u32 rot_vbgn, u32 rot_en,
	u32 fix_cr_en, u32 brst_len_add_en, u32 brst_len_add_value, u32 ofset_brst4_en);

void cfg_vfcd_afrcd(u32 arc_sel, struct AFRCD_TYPE *afrcd);
void cfg_vfcd_fmtup(u32 cmpr_sel,	//1:afbc ;2:afrc
	u32 regs_ofst, u32 fmt_mode, u32 hfmt_en, u32 vfmt_en,
	u32 hfmt_mode, u32 vfmt_mode, u32 horz_skip_y,
	u32 vert_skip_y, u32 horz_skip_uv, u32 vert_skip_uv,
	u32 rev_mode_h, u32 rev_mode_v, u32 out_horz_bgn,
	u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 pad_en, u32 pad_hmode,	//0:alg 8 1:alg16
	u32 pad_vmode,	//0:alg 8 1:alg16
	u32 hz_ini_phase,	// 4 bits
	u32 vt_ini_phase,	// 4 bits
	u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 hsize_y_last_slc);

void cfg_vfcd_dec(u32 index, struct VFCD_t *vfcd);
void cfg_vfcd_cmpr_enable(u32 index, u32 cmp_sel);

void frc_cfg_dpss_afbcd(u32 index, u32 hfmt_en, u32 vfmt_en,
		struct AFBCD *dpss_afbcd);

void frc_cfg_dpss_afbcd_mult(u32 module_sel, u32 hold_line_num,
	u32 blk_mem_mode, u32 compbits_y, u32 compbits_u,
	u32 compbits_v, u32 hsize_in, u32 vsize_in, u32 mif_info_baddr,
	u32 mif_data_baddr, u32 out_horz_bgn, u32 out_vert_bgn,
	u32 out_horz_end, u32 out_vert_end, u32 hz_ini_phase,
	u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 rev_mode, u32 vert_skip_y, u32 horz_skip_y, u32 vert_skip_uv,
	u32 horz_skip_uv, u32 hfmt_en, u32 vfmt_en, u32 def_color_y,
	u32 def_color_u, u32 def_color_v, u32 reg_lossy_en, u32 ddr_sz_mode,
	u32 fmt_mode, u32 reg_fmt444_comb, u32 reg_dos_uncomp_mode,
	u32 pip_src_mode, u32 rot_ofmt_mode, u32 rot_ocompbit, u32 rot_drop_mode,
	u32 rot_vshrk, u32 rot_hshrk, u32 rot_hbgn, u32 rot_vbgn, u32 rot_en,
	u32 fix_cr_en, u32 brst_len_add_en, u32 brst_len_add_value, u32 ofset_brst4_en);

void frc_cfg_vfcd_afrcd(u32 arc_sel, struct AFRCD_TYPE *afrcd);
void frc_cfg_vfcd_fmtup(u32 cmpr_sel,	//1:afbc ;2:afrc
	u32 regs_ofst, u32 fmt_mode, u32 hfmt_en, u32 vfmt_en,
	u32 hfmt_mode, u32 vfmt_mode, u32 horz_skip_y,
	u32 vert_skip_y, u32 horz_skip_uv, u32 vert_skip_uv,
	u32 rev_mode_h, u32 rev_mode_v, u32 out_horz_bgn,
	u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 pad_en, u32 pad_hmode,	//0:alg 8 1:alg16
	u32 pad_vmode,	//0:alg 8 1:alg16
	u32 hz_ini_phase,	// 4 bits
	u32 vt_ini_phase,	// 4 bits
	u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en);

void frc_cfg_vfcd_dec(u32 index, struct VFCD_t *vfcd);
void frc_cfg_vfcd_cmpr_enable(u32 index, u32 cmp_sel);
extern unsigned int dpss_dbg_0709;
#endif
