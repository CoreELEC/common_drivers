/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <uapi/amlogic/amvecm_ext.h>

#ifndef HDR10_TMO
#define HDR10_TMO

#define HDR_MODE_HDR102SDR_SR_MODE  0
#define HDR_MODE_SDR2HDR10_SR_MODE  24
#define HDR_MODE_HDR102HDR10_SR_MODE  25

#ifndef MAX
#define MAX(a, b) ({ \
			const typeof(a) _a = a; \
			const typeof(b) _b = b; \
			_a > _b ? _a : _b; \
		})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			const typeof(a) _a = a; \
			const typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

#ifndef ABS
#define ABS(x) ({ \
			const typeof(x) _x = x; \
			_x > 0 ? _x : -_x; \
		})
#endif

#define SOFT_CLIP 0

struct aml_hw_reg_s {
	unsigned int hw_ctl_enable;
	unsigned int force_reg_update;

	unsigned int reg_cgain_oft2; //DOLBY_HDR2_CGAIN_OFFT  0x4c1c 26:16
	unsigned int reg_cgain_oft1; //DOLBY_HDR2_CGAIN_OFFT  0x4c1c 10:0

	unsigned int reg_coef0; //DOLBY_HDR2_CGAIN_COEF0 0x4c24  27:16
	unsigned int reg_coef1; //DOLBY_HDR2_CGAIN_COEF0 0x4c24  11:0

	unsigned int sel_opt;   //DOLBY_HDR2_CGAIN_COEF1 0x4c25  31:31
	unsigned int reg_maxrgb; //DOLBY_HDR2_CGAIN_COEF1 0x4c25  27:16
	unsigned int reg_c_gain_lim_coef_2; //DOLBY_HDR2_CGAIN_COEF1 0x4c25  11:0

	unsigned int reg_adpscl1_sft; //DOLBY_HDR2_ADPS_CTRL 0x4c28  23:20
	unsigned int reg_ogain_blend_mode; //DOLBY_HDR2_ADPS_CTRL 0x4c28  17:17
	unsigned int reg_adpscl_sel_opt; //DOLBY_HDR2_ADPS_CTRL 0x4c28  16:16
	unsigned int reg_adpscl_max;    //DOLBY_HDR2_ADPS_CTRL 0x4c28  13:8
	unsigned int reg_adpscl_clip_en; //DOLBY_HDR2_ADPS_CTRL 0x4c28  7:7
	unsigned int reg_adpscl_bypass2; //DOLBY_HDR2_ADPS_CTRL 0x4c28  6:6
	unsigned int reg_adpscl_bypass1; //DOLBY_HDR2_ADPS_CTRL 0x4c28  5:5
	unsigned int reg_adpscl_bypass0; //DOLBY_HDR2_ADPS_CTRL 0x4c28  4:4
	unsigned int reg_adpscl1_mode; //DOLBY_HDR2_ADPS_CTRL 0x4c28  3:2
	unsigned int reg_adpscl_mode; //DOLBY_HDR2_ADPS_CTRL 0x4c28  1:0

	unsigned int reg_adpscl_alpha1; //DOLBY_HDR2_ADPS_ALPHA0 0x4c29 29:16
	unsigned int reg_adpscl_alpha0; //DOLBY_HDR2_ADPS_ALPHA0 0x4c29 13:0

	unsigned int reg_adpscl_shift0; //DOLBY_HDR2_ADPS_ALPHA1 0x4c2a 31:28
	unsigned int reg_adpscl_shift1; //DOLBY_HDR2_ADPS_ALPHA1 0x4c2a 24:20
	unsigned int reg_adpscl_shift2; //DOLBY_HDR2_ADPS_ALPHA1 0x4c2a 19:16
	unsigned int reg_adpscl_alpha2; //DOLBY_HDR2_ADPS_ALPHA1 0x4c2a 13:0

	unsigned int reg_adpscl_beta0; //DOLBY_HDR2_ADPS_BETA0 0x4c2b 20:0
	unsigned int reg_adpscl_beta1; //DOLBY_HDR2_ADPS_BETA1 0x4c2c 20:0
	unsigned int reg_adpscl_beta2; //DOLBY_HDR2_ADPS_BETA2 0x4c2d 20:0

	unsigned int reg_adpscl_ys_coef1; //DOLBY_HDR2_ADPS_COEF0 0x4c2e  27:16
	unsigned int reg_adpscl_ys_coef0; //DOLBY_HDR2_ADPS_COEF0 0x4c2e  11:0
	unsigned int reg_adpscl_ys_coef2; //DOLBY_HDR2_ADPS_COEF1 0x4c2f  11:0

	unsigned int reg_new_mode; //DOLBY_HDR2_GMUT_CTRL 0x4c30 4:4
	unsigned int reg_gmut_shift; //DOLBY_HDR2_GMUT_CTRL 0x4c30 3:0

	unsigned int reg_hist_enable; //DOLBY_HDR2_HIST_CTRL 0x4c3d 16:16  //part bits
	unsigned int reg_maxrgb_rshift; //DOLBY_HDR2_HIST_CTRL 0x4c3d 3:3
	unsigned int reg_maxrgb_sel;  //DOLBY_HDR2_HIST_CTRL 0x4c3d 2:0

	unsigned int reg_omax_sync_gain_sft; //DOLBY_HDR2_CGAIN_VIVID 0x4c42 29:26
	unsigned int reg_omax_sync_gain; //DOLBY_HDR2_CGAIN_VIVID 0x4c42 25:16
	unsigned int reg_cgain_pos; //DOLBY_HDR2_CGAIN_VIVID 0x4c42 3:2
	unsigned int reg_ogain_inser; //DOLBY_HDR2_CGAIN_VIVID 0x4c42 1:1

	unsigned int reg_hdr2_gm_comp_en; //DOLBY_HDR2_GMUT_COMP0 0x4c43  31:31
	unsigned int reg_hdr_comp_ofst_r; //DOLBY_HDR2_GMUT_COMP0 0x4c43  27:8
	unsigned int reg_hdr_comp_ofst_g; //DOLBY_HDR2_GMUT_COMP1 0x4c44  27:8
	unsigned int reg_hdr_comp_ofst_b; //DOLBY_HDR2_GMUT_COMP2 0x4c45  27:8
	unsigned int reg_hdr_comp_min_r;  //DOLBY_HDR2_GMUT_COMP3 0x4c46  27:8
	unsigned int reg_hdr_comp_min_g;  //DOLBY_HDR2_GMUT_COMP4 0x4c47  27:8
	unsigned int reg_hdr_comp_min_b;  //DOLBY_HDR2_GMUT_COMP5 0x4c48  27:8
	unsigned int reg_hdr_comp_rat_r;  //DOLBY_HDR2_GMUT_COMP6 0x4c49  29:8
	unsigned int reg_hdr_comp_rat_g;  //DOLBY_HDR2_GMUT_COMP7 0x4c4a  29:8
	unsigned int reg_hdr_comp_rat_b;  //DOLBY_HDR2_GMUT_COMP8 0x4c4b  29:8

	unsigned int reg_adpscl_ys_coef1_1;  //DOLBY_HDR2_ADPSCL1_COEF0 0x4c50 27:16
	unsigned int reg_adpscl_ys_coef1_0;  //DOLBY_HDR2_ADPSCL1_COEF0 0x4c50 11:0

	unsigned int reg_bypass_ootf2_gain; //DOLBY_HDR2_ADPSCL1_COEF1 0x4c51  31:16
	unsigned int reg_adpscl_ys_coef1_2; //DOLBY_HDR2_ADPSCL1_COEF1 0x4c51  11:0

	unsigned int reg_adpscl_mode_gm;  //DOLBY_HDR2_RGB_GM_CTRL 0x4c52 2:2
	unsigned int reg_rgb_gm_mode;     //DOLBY_HDR2_RGB_GM_CTRL 0x4c52 1:1
	unsigned int reg_rgb_gm_en;       //DOLBY_HDR2_RGB_GM_CTRL 0x4c52 0:0
};

struct adpt_regs_s {
	unsigned int en;
	unsigned int num;//max num = 10
	unsigned int *addr;
	unsigned int *val;
};

struct aml_hdr_prm_s {
	u32 hdr_ogain_lut[149];
	u32 hdr_cgain_lut[65];
	u32 hdr_ogain_lut1[149];
};

struct aml_special_pattern_s {
	int special_hist_idx[2];
	int special_pattern_status;
};

struct aml_tmo_hdr_hdr_reg_sw {
	int tmo_en;
	int tmo_highlight;
	int tmo_sc_hist_th;
	int *tmo_oo_init_lut;
	u32 *sbtm_pbpct;                /*pct * 10000*/
	u32 *sbtm_pbnits;               /*lum*/
	u32 sbtm_numpb;
	u32 sbtm_frmpblimit;
	u32 signal_limit;
	u32 sbtm_pst_adj_en;
	u32 sbtm_pst_adj_thr;
	u32 *sbtm_adj_point_y_arr;

	/*param for tmo alg*/
	int w;
	int h;
	int hdr_ogain_shift;
	int hdr_hist_sel;
};

struct aml_tmo_reg_sw {
	int tmo_en;                             //u1
	int tmo_display_e;                      //u10
	int tmo_display_e_bc;                   //u10
	int tmo_max_th1;                        //u10
	int tmo_max_th2;                        //u10
	int tmo_max_th3;                        //u10
	int tmo_max_th4;                        //u10
	int tmo_highlight;                      //u10: control overexposure level
	int tmo_highlight_bc;                   //u10
	int tmo_ratio;                          //u10
	int tmo_light_th;                       //u7
	int tmo_hist_th;                        //u8
	int tmo_highlight_th1;                  //u10
	int tmo_highlight_th2;                  //u10
	int tmo_display_adj;                    //u7
	int tmo_avg_th;                         //u10
	int tmo_avg_adj;                        //u8: 0 ~ 256
	int tmo_hl0;                            //u7: 0 ~ 128
	int tmo_hl1;                            //u7: 0 ~ 128
	int tmo_hl2;                            //u7: 0 ~ 128
	int tmo_hl3;                            //u7: 0 ~ 128
	int tmo_hl0_bc;                         //u7: 0 ~ 128
	int tmo_hl1_bc;                         //u7: 0 ~ 128
	int tmo_hl2_bc;                         //u7: 0 ~ 128
	int tmo_hl3_bc;                         //u7: 0 ~ 128
	int tmo_pnum_th;                        //u16
	int tmo_thold1;                         //u10
	int tmo_thold2;                         //u10
	int tmo_thold3;                         //u10
	int tmo_thold4;                         //u10
	int tmo_middle_th;                      //u10
	int tmo_middle_a;                       //u7
	int tmo_middle_a_bc;                    //u7
	int tmo_middle_a_adj;                   //u10
	int tmo_middle_b;                       //u7
	int tmo_middle_s;                       //u7
	int tmo_low_adj;                        //u7
	int tmo_low_adj_bc;                     //u7
	int tmo_high_en;                        //u2
	int tmo_olut_bit_mode;                  //u1, 0: 12 bits, 1: 16 bits
	int tmo_high_adj1;                      //u7
	int tmo_high_adj2;                      //u7
	int tmo_high_maxdiff;                   //u7
	int tmo_high_mindiff;                   //u7
	int tmo_curve_smo_mode;                 //u2
	int tmo_soft_clip_th;                   //u10
	int tmo_r0_32;                          //u10
	int tmo_r1_32;                          //u10
	int tmo_y_highlight_ratio;                  //u10
	int tmo_ratio_sat;                      //u11
	int tmo_avg_lum_ratio;                  //u10
	int tmo_alpha;                          //u8
	int tmo_model;                          //u2
	int tmo_highlight_lut_gain;             //u8
	int tmo_highlight_lut_clip;             //u12
	int tmo_force_xy_point_en;              //u1
	int tmo_highlight_lut_x;                //ogian x index value 0~148
	int tmo_highlight_lut_y;                //u12
	int tmo_highlight_lut_pst_gain_en;      //u1
	int tmo_highlight_lut_pst_gain;         //u8
	int tmo_highlight_y_clip_pos_low;       //u6
	int tmo_highlight_y_clip_pos_high;      //u6
	int tmo_highlight_y_clip_pos_sup_high;  //u6
	int tmo_highlight_y_clip_en;            //u1
	int tmo_fcurv_low_th;                   //u8
	int tmo_fcurv_mid1_th;                  //u8
	int tmo_fcurv_mid2_th;                  //u8
	int tmo_fcurv_high1_th;                 //u8
	int tmo_fcurv_high2_th;                 //u8
	int tmo_fcurv_high3_th;                 //u8
	int tmo_avg_lum_th0;                    //u10
	int tmo_avg_lum_th1;                    //u10
	int tmo_avg_lum_th2;                    //u10
	int tmo_avg_lum_alpha0;                 //u8: 0 ~ 256
	int tmo_avg_lum_alpha1;                 //u8
	int tmo_avg_lum_alpha2;                 //u8
	int tmo_avg_lum_alpha3;                 //u8
	int tmo_fcurve_adj_en;                  //u1
	int *tmo_fcurve_adj_sync_idx;           //ogian x index value 0~148, suggest 90~120
	int *tmo_fcurve_adj_gain;               //u9
	int tmo_avg_lum_alpha_sc_flag;          //u8
	int tmo_sc_hist_th;                     //u10
	int tmo_sc_maxl_th;                     //u10
	int (*tmo_fcurv_clip_val)[4];           //u12
	int tmo_single_bin_th;                  //special bin percentage 0 ~ 100
	int tmo_full_black_th;                  //u10
	int tmo_full_white_th0;                 //u10
	int tmo_full_white_th1;                 //u10
	int tmo_special_pat1_th;                //u10
	int tmo_special_pat2_th;                //u10
	int tmo_blend_dark_gain;                //u10
	int tmo_blend_dark_th0;                 //u10
	int tmo_blend_dark_th1;                 //u10
	int *tmo_force_ootf1_mode;
	int *tmo_force_ootf1_val;

	unsigned int *tmo_eo_lut;
	int *tmo_oo_init_lut;                   //u10
	struct aml_hw_reg_s *hw_regs;
	struct adpt_regs_s *adpt_regs;

	/*param for tmo alg*/
	int w;
	int h;
	int hdr_ogain_shift;
	int hdr_hist_sel;
	void (*pre_hdr10_tmo_alg)(struct aml_hdr_prm_s *hdr_reg,
		struct aml_tmo_reg_sw *tmo_reg,
		struct aml_tmo_hdr_hdr_reg_sw *tmo_hdr_hdr_reg,
		int *hdr_hist,
		unsigned short *vpp_hist,
		unsigned short hdr_convert_mode);
	int (*fw_tmo_dbg)(char **parm);
	int (*fw_tmo_dbg_show)(char *str);
};

struct aml_tmo_reg_sw *tmo_fw_param_get(void);
#endif
