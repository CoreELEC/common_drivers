/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_REG_HEADER_HH
#define VIDEO_REG_HEADER_HH

#include <linux/string.h>
#include <linux/ctype.h>

#define MAX_VD_LAYER_G12 2
#define MAX_SR_NUM   2

#define MAX_VD_LAYER_T7 3
#define MAX_VPP_NUM     3

#define MAX_VD_LAYER_C3 1
struct hw_vd_reg_s {
	u32 vd_if0_gen_reg; /* VD1_IF0_GEN_REG */
	u32 vd_if0_canvas0;/* VD1_IF0_CANVAS0 */
	u32 vd_if0_canvas1;/* VD1_IF0_CANVAS1 */
	u32 vd_if0_luma_x0;/* VD1_IF0_LUMA_X0 */
	u32 vd_if0_luma_y0; /* VD1_IF0_LUMA_Y0 */
	u32 vd_if0_chroma_x0; /* VD1_IF0_CHROMA_X0 */
	u32 vd_if0_chroma_y0; /* VD1_IF0_CHROMA_Y0 */
	u32 vd_if0_luma_x1; /* VD1_IF0_LUMA_X1 */
	u32 vd_if0_luma_y1;/* VD1_IF0_LUMA_Y1 */
	u32 vd_if0_chroma_x1;/* VD1_IF0_CHROMA_X1 */
	u32 vd_if0_chroma_y1;/* VD1_IF0_CHROMA_Y1 */
	u32 vd_if0_rpt_loop; /* VD1_IF0_RPT_LOOP */
	u32 vd_if0_luma0_rpt_pat;/* VD1_IF0_LUMA0_RPT_PAT */
	u32 vd_if0_chroma0_rpt_pat;/* VD1_IF0_CHROMA0_RPT_PAT */
	u32 vd_if0_luma1_rpt_pat;/* VD1_IF0_LUMA1_RPT_PAT */
	u32 vd_if0_chroma1_rpt_pat;/* VD1_IF0_CHROMA1_RPT_PAT */
	u32 vd_if0_luma_psel;/* VD1_IF0_LUMA_PSEL */
	u32 vd_if0_chroma_psel;/* VD1_IF0_CHROMA_PSEL */
	u32 vd_if0_luma_fifo_size;/* VD1_IF0_LUMA_FIFO_SIZE */

	u32 vd_if0_gen_reg2;/* VD1_IF0_GEN_REG2 */
	u32 vd_if0_gen_reg3;/* VD1_IF0_GEN_REG3 */
	u32 viu_vd_fmt_ctrl;/* VIU_VD1_FMT_CTRL */
	u32 viu_vd_fmt_w;/* VIU_VD1_FMT_W */
};

struct hw_vd_linear_reg_s {
	u32 vd_if0_baddr_y;
	u32 vd_if0_baddr_cb;
	u32 vd_if0_baddr_cr;
	u32 vd_if0_stride_0;
	u32 vd_if0_stride_1;
	u32 vd_if0_baddr_y_f1;
	u32 vd_if0_baddr_cb_f1;
	u32 vd_if0_baddr_cr_f1;
	u32 vd_if0_stride_0_f1;
	u32 vd_if0_stride_1_f1;
};

struct hw_afbc_reg_s {
	u32 afbc_enable; /* AFBC_ENABLE */
	u32 afbc_mode;/* AFBC_MODE */
	u32 afbc_size_in;/* AFBC_SIZE_IN */
	u32 afbc_dec_def_color;/* AFBC_DEC_DEF_COLOR */
	u32 afbc_conv_ctrl; /* AFBC_CONV_CTRL */
	u32 afbc_lbuf_depth; /* AFBC_LBUF_DEPTH */
	u32 afbc_head_baddr; /* AFBC_HEAD_BADDR */
	u32 afbc_body_baddr; /* AFBC_BODY_BADDR */
	u32 afbc_size_out;/* AFBC_SIZE_OUT */
	u32 afbc_out_yscope;/* AFBC_OUT_YSCOPE */
	u32 afbc_stat;/* AFBC_STAT */
	u32 afbc_vd_cfmt_ctrl; /* AFBC_VD_CFMT_CTRL */
	u32 afbc_vd_cfmt_w;/* AFBC_VD_CFMT_W */
	u32 afbc_mif_hor_scope;/* AFBC_MIF_HOR_SCOPE */
	u32 afbc_mif_ver_scope;/* AFBC_MIF_VER_SCOPE */
	u32 afbc_pixel_hor_scope;/* AFBC_PIXEL_HOR_SCOPE */
	u32 afbc_pixel_ver_scope;/* AFBC_PIXEL_VER_SCOPE */
	u32 afbc_vd_cfmt_h;/* AFBC_VD_CFMT_H */
	u32 afbcdec_iquant_enable; /* AFBCDEC_IQUANT_ENABLE */
	u32 afbcdec_iquant_lut_1; /* AFBCDEC_IQUANT_LUT_1 */
	u32 afbcdec_iquant_lut_2; /* AFBCDEC_IQUANT_LUT_2 */
	u32 afbcdec_iquant_lut_3; /* AFBCDEC_IQUANT_LUT_3 */
	u32 afbcdec_iquant_lut_4; /* AFBCDEC_IQUANT_LUT_4 */
	u32 afbc_top_ctrl;
};

struct hw_sr_reg_s {
	u32 sr_sharp_sync_ctrl;
};

struct hw_fg_reg_s {
	u32 fgrain_ctrl;
	u32 fgrain_win_h;
	u32 fgrain_win_v;
};

struct hw_pps_reg_s {
	u32 vd_scale_coef_idx;
	u32 vd_scale_coef;
	u32 vd_vsc_region12_startp;
	u32 vd_vsc_region34_startp;
	u32 vd_vsc_region4_endp;
	u32 vd_vsc_start_phase_step;
	u32 vd_vsc_region1_phase_slope;
	u32 vd_vsc_region3_phase_slope;
	u32 vd_vsc_phase_ctrl;
	u32 vd_vsc_init_phase;
	u32 vd_hsc_region12_startp;
	u32 vd_hsc_region34_startp;
	u32 vd_hsc_region4_endp;
	u32 vd_hsc_start_phase_step;
	u32 vd_hsc_region1_phase_slope;
	u32 vd_hsc_region3_phase_slope;
	u32 vd_hsc_phase_ctrl;
	u32 vd_sc_misc;
	u32 vd_hsc_phase_ctrl1;
	u32 vd_prehsc_coef;
	u32 vd_pre_scale_ctrl;/* sc2 VPP_PREHSC_CTRL */
	u32 vd_prevsc_coef;
	u32 vd_prehsc_coef1;
};

struct hw_vpp_blend_reg_s {
	u32 preblend_h_start_end;
	u32 preblend_v_start_end;
	u32 preblend_h_size;
	u32 postblend_h_start_end;
	u32 postblend_v_start_end;
	/* pip window */
	u32 vd_pip_alph_ctrl;
	u32 vd_pip_alph_scp_h;
	u32 vd_pip_alph_scp_v;
};

struct hw_vppx_blend_reg_s {
	u32 vpp_bld_din0_hscope;
	u32 vpp_bld_din0_vscope;
	u32 vpp_bld_out_size;
	u32 vpp_bld_ctrl;
	u32 vpp_bld_dummy_data;
	u32 vpp_bld_dummy_alpha;
	u32 vpp_blend_ctrl;
};

struct hw_viu_misc_reg_s   {
	u32 mali_afbcd_top_ctrl;
	u32 mali_afbcd1_top_ctrl;
	u32 mali_afbcd2_top_ctrl;
	u32 vpp_vd1_top_ctrl;
	u32 vpp_vd2_top_ctrl;
	u32 vpp_vd3_top_ctrl;
	u32 vd_path_misc_ctrl;
	u32 path_start_sel;
	u32 vpp_misc;
	u32 vpp_misc1;
	u32 vd1_clip_misc0;
	u32 vd1_clip_misc1;
	u32 vd2_clip_misc0;
	u32 vd2_clip_misc1;
	u32 vd3_clip_misc0;
	u32 vd3_clip_misc1;
};

struct hw_vpp_path_size_s {
	u32 vd1_hdr_in_size;
	u32 vd2_hdr_in_size;
	u32 vd3_hdr_in_size;
	u32 vpp_line_in_length;
	u32 vpp_pic_in_height;
	u32 vd1_sc_h_startp;
	u32 vd1_sc_h_endp;
	u32 vd1_sc_v_startp;
	u32 vd1_sc_v_endp;
	u32 vd2_sc_h_startp;
	u32 vd2_sc_h_endp;
	u32 vd2_sc_v_startp;
	u32 vd2_sc_v_endp;
	u32 vd3_sc_h_startp;
	u32 vd3_sc_h_endp;
	u32 vd3_sc_v_startp;
	u32 vd3_sc_v_endp;
	u32 schn_sc_h_startp;
	u32 schn_sc_h_endp;
	u32 schn_sc_v_startp;
	u32 schn_sc_v_endp;
	u32 sr0_in_size;
	u32 sr1_in_size;
	u32 aisr_post_size;
	u32 vpp_preblend_h_size;
	u32 preblend_vd1_h_start_end;
	u32 preblend_vd1_v_start_end;
	u32 vpp_ve_h_v_size;
	u32 vpp_postblend_h_size;
	u32 vpp_out_h_v_size;
	u32 postblend_vd1_h_start_end;
	u32 postlend_vd1_v_start_end;
	u32 blend_vd2_h_start_end;
	u32 blend_vd2_v_start_end;
	u32 blend_vd3_h_start_end;
	u32 blend_vd3_v_start_end;
	u32 vd1_sco_fifo_ctrl;
	u32 vd2_sco_fifo_ctrl;
};

struct hw_aisr_reshape_reg_s {
	u32 aisr_reshape_ctrl0;
	u32 aisr_reshape_ctrl1;
	u32 aisr_reshape_scope_x;
	u32 aisr_reshape_scope_y;
	u32 aisr_reshape_baddr00;
	u32 aisr_reshape_baddr01;
	u32 aisr_reshape_baddr02;
	u32 aisr_reshape_baddr03;
	u32 aisr_reshape_baddr10;
	u32 aisr_reshape_baddr11;
	u32 aisr_reshape_baddr12;
	u32 aisr_reshape_baddr13;
	u32 aisr_reshape_baddr20;
	u32 aisr_reshape_baddr21;
	u32 aisr_reshape_baddr22;
	u32 aisr_reshape_baddr23;
	u32 aisr_reshape_baddr30;
	u32 aisr_reshape_baddr31;
	u32 aisr_reshape_baddr32;
	u32 aisr_reshape_baddr33;
	u32 aisr_post_ctrl;
	u32 aisr_post_size;
	u32 aisr_sr1_nn_post_top;
};

struct hw_vsr_safa_reg_s {
	/* top */
	u32 vpp_vsr_top_c42c44_mode;
	u32 vpp_vsr_top_misc;
	/* pi */
	u32 vpp_pi_en_mode;
	u32 vpp_pi_misc;
	u32 vpp_pi_dict_num;
	u32 vpp_pi_win_ofst;
	u32 vpp_pi_in_hsc_part;
	u32 vpp_pi_in_hsc_ini;
	u32 vpp_pi_in_vsc_part;
	u32 vpp_pi_in_vsc_ini;
	u32 vpp_pi_hf_hsc_part;
	u32 vpp_pi_hf_hsc_ini;
	u32 vpp_pi_hf_vsc_part;
	u32 vpp_pi_hf_vsc_ini;
	/* safa */
	u32 safa_pps_sr_422_en;
	u32 safa_pps_pre_scale;
	u32 safa_pps_pre_hscale_coef_y1;
	u32 safa_pps_pre_hscale_coef_y0;
	u32 safa_pps_pre_hscale_coef_c1;
	u32 safa_pps_pre_hscale_coef_c0;
	u32 safa_pps_pre_vscale_coef;
	u32 safa_pps_hw_ctrl;
	u32 safa_pps_interp_en_mode;
	u32 safa_pps_yuv_sharpen_en;
	u32 safa_pps_dir_en_mode;
	u32 safa_pps_sr_alp_info;
	u32 safa_pps_pi_info;
	u32 safa_pps_hsc_start_phase_step;
	u32 safa_pps_vsc_start_phase_step;
	u32 safa_pps_vsc_init;
	u32 safa_pps_hsc_init;
	u32 safa_pps_bot_vsc_init;
	u32 safa_pps_sc_misc;
	u32 safa_pps_cntl_scale_coef_idx_luma;
	u32 safa_pps_cntl_scale_coef_luma;
	u32 safa_pps_cntl_scale_coef_idx_chro;
	u32 safa_pps_cntl_scale_coef_chro;
	u32 vpp_sr_en;
	u32 safa_pps_dejaggy_ctrl;
	u32 safa_pps_hsc_init_interget;
	u32 safa_pps_hv_adp_tap_wind;
	u32 vpp_vsr_ofifo;
};

struct hw_aisr_demo_reg_s {
	u32 vpp_pi_debug_demo_wnd_en;
	u32 vpp_pi_debug_demo_wnd_coef_1;
	u32 vpp_pi_debug_demo_wnd_coef_0;
	u32 safa_pps_debug_demo_en;
	u32 safa_pps_debug_demo_wnd_coef_1;
	u32 safa_pps_debug_demo_wnd_coef_0;
	u32 vpp_sr_debug_demo_wnd_en;
	u32 vpp_sr_debug_demo_wnd_coef_1;
	u32 vpp_sr_debug_demo_wnd_coef_0;
};

struct hw_vsr_safa_nonlinear_reg_s {
	u32 safa_pps_hsc_region12_startp;
	u32 safa_pps_hsc_region34_startp;
	u32 safa_pps_hsc_region4_endp;
	u32 safa_pps_hsc_region0_phase_slop;
	u32 safa_pps_hsc_region1_phase_slop;
	u32 safa_pps_hsc_region3_phase_slop;
	u32 safa_pps_hsc_region4_phase_slop;
};

struct hw_mif_reg_s {
	u32 vfcd_top_ctrl0;
	u32 vfcd_top_ctrl2;
	u32 rmif_top_ctrl;
	u32 mif0_rmif_ctrl1;
	u32 mif0_rmif_ctrl2;
	u32 mif0_rmif_ctrl3;
	u32 mif0_rmif_ctrl4;
	u32 mif0_rmif_ctrl5;
	u32 mif1_rmif_ctrl1;
	u32 mif1_rmif_ctrl2;
	u32 mif1_rmif_ctrl3;
	u32 mif1_rmif_ctrl4;
	u32 mif1_rmif_ctrl5;
	u32 mif2_rmif_ctrl;
	u32 mif2_rmif_baddr;
	u32 mif2_rmif_stride;
	u32 luma_rmif_scope_x;
	u32 luma_rmif_scope_y;
	u32 chrm_rmif_scope_x;
	u32 chrm_rmif_scope_y;
	u32 rmif_dummy_pixel;
	u32 rmif_rpt_loop;
	u32 rmif_f0_luma_rpt_pat;
	u32 rmif_f0_chrm_rpt_pat;
	u32 rmif_f1_luma_rpt_pat;
	u32 rmif_f1_chrm_rpt_pat;
	u32 rmif_f1_luma_ysize;
	u32 rmif_f1_chrm_ysize;
	u32 rmif_f1_mif0_baddr;
	u32 rmif_f1_mif1_baddr;
	u32 rmif_f1_mif2_baddr;
	u32 rmif_f1_stride0;
	u32 rmif_f1_stride1;
	u32 rmif_fifo_size;
	u32 rmif_chrm_psel;
	u32 vfcd_afbc_conv_ctrl;
	u32 vfcd_afbc_vd_cfmt_ctrl;
	u32 vfcd_afbc_vd_cfmt_w;
	u32 vfcd_afbc_vd_cfmt_h;
	u32 vfcd_post_luma_size;
	u32 vfcd_conv_buf_lens;
	u32 vfcd_afbc_lbuf_depth;
	u32 vfcd_luma_pic_xpos;
	u32 vfcd_luma_pic_ypos;
	u32 vfcd_chrm_pic_xpos;
	u32 vfcd_chrm_pic_ypos;
	u32 vfcd_mmu_ctrl;
	u32 vfcd_mmu_baddr0;
	u32 vfcd_mmu_baddr1;
	u32 vfcd_mmu_baddr2;
};

struct hw_vfcd_reg_s {
	/* common */
	u32 vfcd_top_ctrl0;
	u32 vfcd_top_ctrl2;
	u32 vfcd_mmu_ctrl;
	u32 vfcd_afrc1_param;
	u32 vfcd_afbc_mode;
	u32 vfcd_conv_buf_lens;
	u32 vfcd_afbc_lbuf_depth;
	u32 vfcd_afbc_conv_ctrl;
	u32 vfcd_frm_pic_size;
	u32 vfcd_luma_pic_xpos;
	u32 vfcd_luma_pic_ypos;
	u32 vfcd_afbc_vd_cfmt_ctrl;
	u32 vfcd_afbc_vd_cfmt_w;
	u32 vfcd_afbc_vd_cfmt_h;
	/* afbc */
	u32 vfcd_afbc_iquant_enable;
	u32 vfcd_afbc_burst_ctrl;
	u32 vfcd_afbc_dec_def_color;
	u32 vfcd_afbc_head_baddr;
	u32 vfcd_afbc_body_baddr;
	u32 vfcd_afbc_enable;
	/* afrc */
	u32 vfcd_chrm_pic_xpos;
	u32 vfcd_chrm_pic_ypos;
	u32 vfcd_slc1_luma_pic_xpos;
	u32 vfcd_slc1_chrm_pic_xpos;
	u32 vfcd_slc2_luma_pic_xpos;
	u32 vfcd_slc2_chrm_pic_xpos;
	u32 vfcd_slc3_luma_pic_xpos;
	u32 vfcd_slc3_chrm_pic_xpos;
	u32 vfcd_post_luma_size;
	u32 vfcd_post_luma_win_h;
	u32 vfcd_post_chrm_win_h;
	u32 vfcd_post_luma_win_v;
	u32 vfcd_post_chrm_win_v;
	u32 vfcd_luma_pad_size;
	u32 vfcd_luma_pad_ofst;
	u32 vfcd_chrm_pad_size;
	u32 vfcd_chrm_pad_ofst;
	u32 vfcd_pad_dumy_data;
	u32 vfcd_mif1_urgent;
	u32 vfcd_mmu_baddr0;
	u32 vfcd_mmu_baddr1;
	u32 vfcd_afrc_ctrl0;
	u32 vfcd_afrc0_ctrl;
	u32 vfcd_afrc_ctrl1;
	u32 vfcd_afrc1_ctrl;
	u32 vfcd_afrc_chrm_head_baddr;
	u32 vfcd_afrc_chrm_body_baddr;
};

struct hw_mosaic_misc_reg_s {
	u32 player_2x2_ctrl;
	u32 vfcd_top_ctrl4;
	u32 vfcd_top_ctrl4_1;
	u32 vpu_hdr2_top_ctrl;
};

struct hw_vd_csc_reg_s {
	u32 vout_vd1_csc_coef00_01;
	u32 vout_vd1_csc_coef02_10;
	u32 vout_vd1_csc_coef11_12;
	u32 vout_vd1_csc_coef20_21;
	u32 vout_vd1_csc_coef22;
	u32 vout_vd1_csc_coef13_14;
	u32 vout_vd1_csc_coef23_24;
	u32 vout_vd1_csc_coef15_25;
	u32 vout_vd1_csc_clip;
	u32 vout_vd1_csc_offset0_1;
	u32 vout_vd1_csc_offset2;
	u32 vout_vd1_csc_pre_offset0_1;
	u32 vout_vd1_csc_pre_offset2;
	u32 vout_vd1_csc_en_ctrl;
};

struct hw_vout_blend_reg_s {
	u32 vpu_vout_blend_ctrl;
	u32 vpu_vout_blend_dumdata;
	u32 vpu_vout_blend_size;
	u32 vpu_vout_bld_src0_hpos;
	u32 vpu_vout_bld_src0_vpos;
	u32 vpu_vout_bld_src1_hpos;
	u32 vpu_vout_bld_src1_vpos;
};

extern struct hw_vd_reg_s vd_mif_reg_legacy_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_vd_reg_s vd_mif_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_array[MAX_VD_LAYER_G12];
extern struct hw_afbc_reg_s vd_afbc_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_g12_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_sc2_array[MAX_VD_LAYER_G12];
extern struct hw_pps_reg_s pps_reg_array[MAX_VD_LAYER_G12];
extern struct hw_pps_reg_s pps_reg_array_t5d[MAX_VD_LAYER_G12];
extern struct hw_vpp_blend_reg_s vpp_blend_reg_array[MAX_VD_LAYER_G12];

extern struct hw_vd_reg_s vd_mif_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vd_linear_reg_s vd_mif_linear_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_afbc_reg_s vd_afbc_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_fg_reg_s fg_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_pps_reg_s pps_reg_t7_array[MAX_VD_LAYER_T7 + 1];
extern struct hw_pps_reg_s pps_reg_t6x_array[MAX_VD_LAYER_T7 + 1];
extern struct hw_vpp_blend_reg_s vpp_blend_reg_t7_array[MAX_VD_LAYER_T7];
extern struct hw_vpp_path_size_s vpp_path_size_reg;
extern struct hw_vpp_path_size_s vpp_path_size_reg_t6w;
extern struct hw_viu_misc_reg_s viu_misc_reg;
extern struct hw_vppx_blend_reg_s vppx_blend_reg_array[MAX_VPP_NUM - 1];
extern struct hw_vppx_blend_reg_s vppx_blend_reg_a9_array[MAX_VPP_NUM - 1];

extern struct hw_vd_reg_s vd_mif_reg_c3_array[MAX_VD_LAYER_C3];
extern struct hw_vd_linear_reg_s vd_mif_linear_reg_c3_array[MAX_VD_LAYER_C3];
extern struct hw_vd_csc_reg_s vd_csc_c3_reg[MAX_VD_LAYER_C3];
extern struct hw_vout_blend_reg_s vout_blend_c3_reg;

extern struct hw_vd_reg_s vd_mif_reg_a4_array[MAX_VD_LAYER_C3];
extern struct hw_vd_linear_reg_s vd_mif_linear_reg_a4_array[MAX_VD_LAYER_C3];
extern struct hw_pps_reg_s pps_reg_a4_array[MAX_VD_LAYER_C3];
extern struct hw_vd_csc_reg_s vd_csc_a4_reg[MAX_VD_LAYER_C3];
extern struct hw_vout_blend_reg_s vout_blend_a4_reg;

extern struct hw_aisr_reshape_reg_s aisr_reshape_reg;
extern struct hw_vsr_safa_reg_s vsr_safa_reg;
extern struct hw_vsr_safa_reg_s s6_vsr_safa_reg;
extern struct hw_vsr_safa_reg_s vsr_safa_reg_t6d;
extern struct hw_vsr_safa_nonlinear_reg_s vsr_safa_nonlinear_reg;
extern struct hw_vsr_safa_nonlinear_reg_s vsr_safa_nonlinear_reg_t6w;
extern struct hw_vsr_safa_reg_s vsr_safa_reg_t6w;
extern struct hw_aisr_demo_reg_s s7d_aisr_demo_reg;
extern struct hw_aisr_demo_reg_s t6w_aisr_demo_reg;
extern struct hw_aisr_demo_reg_s t6x_aisr_demo_reg;
extern struct hw_mif_reg_s vd_mif_reg_t6w_array[MAX_VD_LAYER_G12];
extern struct hw_vfcd_reg_s vd_vfcd_reg_t6w_array[MAX_VD_LAYER_G12];
extern struct hw_fg_reg_s fg_reg_t6w_array[MAX_VD_LAYER_G12];
extern struct hw_mif_reg_s mosaic_vd_mif_reg_t6w_array[4];
extern struct hw_vfcd_reg_s mosaic_vd_vfcd_reg_t6w_array[4];
extern struct hw_pps_reg_s mosaic_pps_reg_t6x_array[4];
extern struct hw_vpp_blend_reg_s mosaic_vpp_blend_reg_t6x_array[4];
extern struct hw_mosaic_misc_reg_s mosaic_misc_reg;
extern struct hw_fg_reg_s fg_reg_t6x_array[4];

#endif
