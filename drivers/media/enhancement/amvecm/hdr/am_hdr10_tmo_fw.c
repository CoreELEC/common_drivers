// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amvecm/hdr10_tmo_alg.h>

#include "am_hdr10_tm.h"
#include "am_hdr10_tmo_fw.h"
#include "../set_hdr2_v0.h"
#include "../s5_set_hdr2_v0.h"
#include "am_hdr_sbtm.h"

int pr_tmo_en;

unsigned int tmo_force_ootf1_mode;/*0:off  1:force_ootf1_val, 2:force_ootf_lut1*/
module_param(tmo_force_ootf1_mode, uint, 0664);
MODULE_PARM_DESC(tmo_force_ootf1_mode, "\n force ootf mode\n");

unsigned int tmo_force_ootf1_val;
module_param(tmo_force_ootf1_val, uint, 0664);
MODULE_PARM_DESC(tmo_force_ootf1_val, "\n force ootf val\n");

#define pr_tmo_dbg(fmt, args...)\
	do {\
		if (pr_tmo_en)\
			pr_info("HDR10_TMO: " fmt, ##args);\
	} while (0)

static unsigned int eo_lut_28[143] = {
	0, 1, 4, 7, 12, 17, 24, 31, 39, 49, 59, 71, 83, 97,
	111, 127, 144, 320, 578, 931, 1392, 1977, 2701, 3582,
	4638, 5891, 7362, 9076, 11058, 13337, 15942, 18907,
	22266, 26058, 30324, 35106, 40454, 46418, 53052, 60415,
	68572, 77590, 87542, 98508, 110570, 123821, 138356, 154282,
	171708, 190757, 211556, 234243, 258967, 285886, 315170, 347002,
	381578, 419105, 459810, 503932, 551729, 603476, 659470, 720026,
	785484, 856207, 932584, 1015032, 1103997, 1199956, 1303424,
	1414947, 1535113, 1664552, 1803938, 1953994, 2115494, 2289267,
	2476202, 2677251, 2893435, 3125848, 3375662, 3644136, 3932616,
	4242547, 4575481, 4933078, 5317122, 5729526, 6172341, 6647771,
	7158178, 7706100, 8294261, 8925586, 9603216, 10330526, 11111139,
	11948953, 12848153, 13813241, 14849055, 15960799, 17154072, 18434897,
	19809755, 21285624, 22870015, 24571018, 26397348, 28358395, 30464279,
	32725910, 35155053, 37764399, 40567642, 43579559, 46816107, 50294518,
	54033406, 58052888, 62374710, 67022388, 72021358, 77399146, 83185550,
	89412833, 96115952, 103332784, 111104394, 119475320, 128493881, 138212527,
	148688209, 159982795, 172163524, 185303500, 199482248, 214786307,
	231309901, 249155666, 268435456
};

int init_lut[13] = {
	0, 128, 256, 384, 512, 576, 640, 704, 768, 832, 896, 960, 1024
};

int fcurve_adj_sync_idx[10] = {
	100, 100, 100, 100, 113, 113, 113, 113, 113, 113
};  //100 ~ 1000 nit

int fcurve_adj_gain[10] = {
	200, 200, 200, 200, 256, 256, 256, 256, 256, 256
};  //100 ~ 1000 nit

int clip_val[10][4] = {
	{1200, 1000, 700, 400,},  //100nit
	{1200, 1000, 700, 400,},  //200nit
	{1200, 1000, 700, 400,},  //300nit
	{650, 600, 500, 300,},  //400nit
	{650, 600, 500, 300,},  //500nit
	{1000, 650, 500, 300,},  //600nit
	{1000, 650, 500, 300,},  //700nit
	{1000, 650, 500, 300,},  //800nit
	{1000, 650, 500, 300,},  //900nit
	{1000, 650, 500, 300,},  //1000nit
};

struct aml_hw_reg_s pq_hw_regs = {
	.hw_ctl_enable = 0,
	.force_reg_update = 0,

	.reg_cgain_oft2 = 0x200,
	.reg_cgain_oft1 = 0x200,
	.reg_coef0 = 0x948,
	.reg_coef1 = 0x398,

	.sel_opt = 1,
	.reg_maxrgb = 0x3ff,
	.reg_c_gain_lim_coef_2 = 0xd0,

	.reg_adpscl1_sft = 0xc,
	.reg_ogain_blend_mode = 0,
	.reg_adpscl_sel_opt = 1,
	.reg_adpscl_max = 0x20,
	.reg_adpscl_clip_en = 1,
	.reg_adpscl_bypass2 = 0,
	.reg_adpscl_bypass1 = 0,
	.reg_adpscl_bypass0 = 0,
	.reg_adpscl1_mode = 1,
	.reg_adpscl_mode = 1,

	.reg_adpscl_alpha1 = 0x400,
	.reg_adpscl_alpha0 = 0x400,

	.reg_adpscl_shift0 = 0xa,
	.reg_adpscl_shift1 = 0x6,
	.reg_adpscl_shift2 = 0xa,
	.reg_adpscl_alpha2 = 0x400,

	.reg_adpscl_beta0 = 0x0,
	.reg_adpscl_beta1 = 0x0,
	.reg_adpscl_beta2 = 0x0,

	.reg_adpscl_ys_coef1 = 0x400,
	.reg_adpscl_ys_coef0 = 0x400,
	.reg_adpscl_ys_coef2 = 0x400,

	.reg_new_mode = 0x1,
	.reg_gmut_shift = 0x8,

	.reg_hist_enable = 0x1,
	.reg_maxrgb_rshift = 0x0,
	.reg_maxrgb_sel = 0x1,

	.reg_omax_sync_gain_sft = 0x0,
	.reg_omax_sync_gain = 0x0,
	.reg_cgain_pos = 0x0,
	.reg_ogain_inser = 0x0,

	.reg_hdr2_gm_comp_en = 0x0,
	.reg_hdr_comp_ofst_r = 0x14f8c,
	.reg_hdr_comp_ofst_g = 0x14f8c,
	.reg_hdr_comp_ofst_b = 0x14f8c,
	.reg_hdr_comp_min_r = 0x7c849,
	.reg_hdr_comp_min_g = 0x73785,
	.reg_hdr_comp_min_b = 0x7204b,
	.reg_hdr_comp_rat_r = 0x2522c,
	.reg_hdr_comp_rat_g = 0x73785,
	.reg_hdr_comp_rat_b = 0x7204b,

	.reg_adpscl_ys_coef1_1 = 0x0,
	.reg_adpscl_ys_coef1_0 = 0x0,

	.reg_bypass_ootf2_gain = 0x0,
	.reg_adpscl_ys_coef1_2 = 0x0,

	.reg_adpscl_mode_gm = 0x0,
	.reg_rgb_gm_mode = 0x0,
	.reg_rgb_gm_en = 0x0,
};

unsigned int adpt_addr[10] = {0};
unsigned int adpt_val[10] = {0};

struct adpt_regs_s _adpt_regs = {
	.en = 0,
	.num = 0,
	.addr = adpt_addr,
	.val = adpt_val,
};

struct aml_tmo_reg_sw tmo_reg = {
	.tmo_en = 1,
	.tmo_display_e = 625,
	.tmo_display_e_bc = 500,
	.tmo_max_th1 = 50,
	.tmo_max_th2 = 30,
	.tmo_max_th3 = -200,
	.tmo_max_th4 = 153,
	.tmo_highlight = 305,
	.tmo_highlight_bc = 832,
	.tmo_ratio = 870,
	.tmo_light_th = 110,
	.tmo_hist_th = 120,
	.tmo_highlight_th1 = 70,
	.tmo_highlight_th2 = 65,
	.tmo_display_adj = 115,
	.tmo_avg_th = 35,
	.tmo_avg_adj = 150,
	.tmo_hl0 = 100,
	.tmo_hl1 = 125,
	.tmo_hl2 = 80,
	.tmo_hl3 = 110,
	.tmo_hl0_bc = 60,
	.tmo_hl1_bc = 38,
	.tmo_hl2_bc = 40,
	.tmo_hl3_bc = 60,
	.tmo_pnum_th = 5000,
	.tmo_thold1 = 300,
	.tmo_thold2 = 30,
	.tmo_thold3 = 100,
	.tmo_thold4 = 260,
	.tmo_middle_th = 500,
	.tmo_middle_a = 77,
	.tmo_middle_a_bc = 75,
	.tmo_middle_a_adj = 205,
	.tmo_middle_b = 27,
	.tmo_middle_s = 64,
	.tmo_low_adj = 38,
	.tmo_low_adj_bc = 30,
	.tmo_high_en = 0,
	.tmo_olut_bit_mode = 0,
	.tmo_high_adj1 = 13,
	.tmo_high_adj2 = 77,
	.tmo_high_maxdiff = 64,
	.tmo_high_mindiff = 8,
	.tmo_curve_smo_mode = 1,
	.tmo_soft_clip_th = 600,
	.tmo_r0_32 = 400,
	.tmo_r1_32 = 256,
	.tmo_y_highlight_ratio = 10,
	.tmo_ratio_sat = 1024,
	.tmo_avg_lum_ratio = 512,
	.tmo_alpha = 250,
	.tmo_model = 0,
	.tmo_highlight_lut_gain = 150,
	.tmo_highlight_lut_clip = 800,
	.tmo_force_xy_point_en = 0,
	.tmo_highlight_lut_x = 148,
	.tmo_highlight_lut_y = 300,
	.tmo_highlight_lut_pst_gain_en = 1,
	.tmo_highlight_lut_pst_gain = 256,
	.tmo_highlight_y_clip_pos_low = 42,
	.tmo_highlight_y_clip_pos_high = 47,
	.tmo_highlight_y_clip_pos_sup_high = 55,
	.tmo_highlight_y_clip_en = 1,
	.tmo_fcurv_low_th = 0,
	.tmo_fcurv_mid1_th = 24,
	.tmo_fcurv_mid2_th = 36,
	.tmo_fcurv_high1_th = 48,
	.tmo_fcurv_high2_th = 60,
	.tmo_fcurv_high3_th = 62,
	.tmo_avg_lum_th0 = 100,
	.tmo_avg_lum_th1 = 300,
	.tmo_avg_lum_th2 = 800,
	.tmo_avg_lum_alpha0 = 256,
	.tmo_avg_lum_alpha1 = 256,
	.tmo_avg_lum_alpha2 = 256,
	.tmo_avg_lum_alpha3 = 256,
	.tmo_fcurve_adj_en = 1,
	.tmo_fcurve_adj_sync_idx = fcurve_adj_sync_idx,
	.tmo_fcurve_adj_gain = fcurve_adj_gain,
	.tmo_avg_lum_alpha_sc_flag = 250,
	.tmo_sc_hist_th = 682,
	.tmo_sc_maxl_th = 16,
	.tmo_fcurv_clip_val = clip_val,
	.tmo_single_bin_th = 99,
	.tmo_eo_lut = eo_lut_28,
	.tmo_oo_init_lut = init_lut,
	.tmo_full_black_th = 10,
	.tmo_full_white_th0 = 700,
	.tmo_full_white_th1 = 950,
	.tmo_special_pat1_th = 80,
	.tmo_special_pat2_th = 600,
	.tmo_blend_dark_gain = 350,
	.tmo_blend_dark_th0 = 128,
	.tmo_blend_dark_th1 = 216,
	.tmo_force_ootf1_mode = &tmo_force_ootf1_mode,
	.tmo_force_ootf1_val = &tmo_force_ootf1_val,
	.hw_regs = &pq_hw_regs,
	.adpt_regs = &_adpt_regs,
	/*param for alg func*/
	.w = 3840,
	.h = 2160,
	.hdr_ogain_shift = 6,
	.hdr_hist_sel = 0,
	.pre_hdr10_tmo_alg = NULL,
	.fw_tmo_dbg = NULL,
	.fw_tmo_dbg_show = NULL,
};

void change_param(int val)
{
	if (val == 0 || val == 1) {
		tmo_reg.tmo_highlight = 305;
		tmo_reg.tmo_middle_a = 77;
		tmo_reg.tmo_low_adj = 38;
		tmo_reg.tmo_hl0 = 100;
		tmo_reg.tmo_hl1 = 125;
		tmo_reg.tmo_hl2 = 80;
		tmo_reg.tmo_hl3 = 110;
	} else if (val == 2) {
		tmo_reg.tmo_highlight = 832;
		tmo_reg.tmo_middle_a = 80;
		tmo_reg.tmo_low_adj = 30;
		tmo_reg.tmo_hl0 = 40;
		tmo_reg.tmo_hl1 = 60;
		tmo_reg.tmo_hl2 = 80;
		tmo_reg.tmo_hl3 = 90;
	}
	pr_info("tmo_reg changed from %d to %d\n", tmo_reg.tmo_en, val);
}

void tmo_parm_show_pre(struct hdr_tmo_sw *pre_tmo_reg)
{
	if (!pre_tmo_reg) {
		pr_tmo_dbg("pre_tmo_reg is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	pr_tmo_dbg("tmo_en = %d\n", pre_tmo_reg->tmo_en);
	pr_tmo_dbg("reg_highlight = %d\n", pre_tmo_reg->reg_highlight);
	pr_tmo_dbg("reg_light_th = %d\n", pre_tmo_reg->reg_light_th);
	pr_tmo_dbg("reg_hist_th = %d\n", pre_tmo_reg->reg_hist_th);
	pr_tmo_dbg("reg_highlight_th1 = %d\n", pre_tmo_reg->reg_highlight_th1);
	pr_tmo_dbg("reg_highlight_th2 = %d\n", pre_tmo_reg->reg_highlight_th2);
	pr_tmo_dbg("reg_display_e = %d\n", pre_tmo_reg->reg_display_e);
	pr_tmo_dbg("reg_middle_a = %d\n", pre_tmo_reg->reg_middle_a);
	pr_tmo_dbg("reg_middle_a_adj = %d\n", pre_tmo_reg->reg_middle_a_adj);
	pr_tmo_dbg("reg_middle_b = %d\n", pre_tmo_reg->reg_middle_b);
	pr_tmo_dbg("reg_middle_s = %d\n", pre_tmo_reg->reg_middle_s);
	pr_tmo_dbg("reg_max_th1 = %d\n", pre_tmo_reg->reg_max_th1);
	pr_tmo_dbg("reg_middle_th = %d\n", pre_tmo_reg->reg_middle_th);
	pr_tmo_dbg("reg_thold2 = %d\n", pre_tmo_reg->reg_thold2);
	pr_tmo_dbg("reg_thold3 = %d\n", pre_tmo_reg->reg_thold3);
	pr_tmo_dbg("reg_thold4 = %d\n", pre_tmo_reg->reg_thold4);
	pr_tmo_dbg("reg_thold1 = %d\n", pre_tmo_reg->reg_thold1);
	pr_tmo_dbg("reg_max_th2 = %d\n", pre_tmo_reg->reg_max_th2);
	pr_tmo_dbg("reg_pnum_th = %d\n", pre_tmo_reg->reg_pnum_th);
	pr_tmo_dbg("reg_hl0 = %d\n", pre_tmo_reg->reg_hl0);
	pr_tmo_dbg("reg_hl1 = %d\n", pre_tmo_reg->reg_hl1);
	pr_tmo_dbg("reg_hl2 = %d\n", pre_tmo_reg->reg_hl2);
	pr_tmo_dbg("reg_hl3 = %d\n", pre_tmo_reg->reg_hl3);
	pr_tmo_dbg("reg_display_adj = %d\n", pre_tmo_reg->reg_display_adj);
	pr_tmo_dbg("reg_low_adj = %d\n", pre_tmo_reg->reg_low_adj);
	pr_tmo_dbg("reg_high_en = %d\n", pre_tmo_reg->reg_high_en);
	pr_tmo_dbg("reg_high_adj1 = %d\n", pre_tmo_reg->reg_high_adj1);
	pr_tmo_dbg("reg_high_adj2 = %d\n", pre_tmo_reg->reg_high_adj2);
	pr_tmo_dbg("reg_high_maxdiff = %d\n", pre_tmo_reg->reg_high_maxdiff);
	pr_tmo_dbg("reg_high_mindiff = %d\n", pre_tmo_reg->reg_high_mindiff);
	pr_tmo_dbg("reg_avg_th = %d\n", pre_tmo_reg->reg_avg_th);
	pr_tmo_dbg("reg_avg_adj = %d\n", pre_tmo_reg->reg_avg_adj);
	pr_tmo_dbg("alpha = %d\n", pre_tmo_reg->alpha);
	pr_tmo_dbg("reg_ratio = %d\n", pre_tmo_reg->reg_ratio);
	pr_tmo_dbg("reg_max_th3 = %d\n", pre_tmo_reg->reg_max_th3);
	pr_tmo_dbg("oo_init_lut = %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		pre_tmo_reg->oo_init_lut[0], pre_tmo_reg->oo_init_lut[1],
		pre_tmo_reg->oo_init_lut[2], pre_tmo_reg->oo_init_lut[3],
		pre_tmo_reg->oo_init_lut[4], pre_tmo_reg->oo_init_lut[5],
		pre_tmo_reg->oo_init_lut[6], pre_tmo_reg->oo_init_lut[7],
		pre_tmo_reg->oo_init_lut[8], pre_tmo_reg->oo_init_lut[9],
		pre_tmo_reg->oo_init_lut[10], pre_tmo_reg->oo_init_lut[11],
		pre_tmo_reg->oo_init_lut[12]);
}

void tmo_parm_ext_show_pre(struct hdr_tmo_sw_ext *pre_tmo_reg_ext)
{
	int i;

	if (!pre_tmo_reg_ext) {
		pr_tmo_dbg("pre_tmo_reg_ext is NULL\n");
		return;
	}

	pr_info("reg_display_e_bc = %d\n", pre_tmo_reg_ext->reg_display_e_bc);
	pr_info("reg_max_th4 = %d\n", pre_tmo_reg_ext->reg_max_th4);
	pr_info("reg_highlight_bc = %d\n", pre_tmo_reg_ext->reg_highlight_bc);
	pr_info("reg_hl0_bc = %d\n", pre_tmo_reg_ext->reg_hl0_bc);
	pr_info("reg_hl1_bc = %d\n", pre_tmo_reg_ext->reg_hl1_bc);
	pr_info("reg_hl2_bc = %d\n", pre_tmo_reg_ext->reg_hl2_bc);
	pr_info("reg_hl3_bc = %d\n", pre_tmo_reg_ext->reg_hl3_bc);
	pr_info("reg_middle_a_bc = %d\n", pre_tmo_reg_ext->reg_middle_a_bc);
	pr_info("reg_low_adj_bc = %d\n", pre_tmo_reg_ext->reg_low_adj_bc);
	pr_info("reg_olut_bit_mode = %d\n", pre_tmo_reg_ext->reg_olut_bit_mode);
	pr_info("reg_curve_smo_mode = %d\n", pre_tmo_reg_ext->reg_curve_smo_mode);
	pr_info("reg_soft_clip_th = %d\n", pre_tmo_reg_ext->reg_soft_clip_th);
	pr_info("reg_r0_32 = %d\n", pre_tmo_reg_ext->reg_r0_32);
	pr_info("reg_r1_32 = %d\n", pre_tmo_reg_ext->reg_r1_32);
	pr_info("reg_y_highlight_ratio = %d\n", pre_tmo_reg_ext->reg_y_highlight_ratio);
	pr_info("reg_ratio_sat = %d\n", pre_tmo_reg_ext->reg_ratio_sat);
	pr_info("reg_avg_lum_ratio = %d\n", pre_tmo_reg_ext->reg_avg_lum_ratio);
	pr_info("reg_model = %d\n", pre_tmo_reg_ext->reg_model);
	pr_info("reg_highlight_lut_gain = %d\n", pre_tmo_reg_ext->reg_highlight_lut_gain);
	pr_info("reg_highlight_lut_clip = %d\n", pre_tmo_reg_ext->reg_highlight_lut_clip);
	pr_info("reg_force_xy_point_en = %d\n", pre_tmo_reg_ext->reg_force_xy_point_en);
	pr_info("reg_highlight_lut_x = %d\n", pre_tmo_reg_ext->reg_highlight_lut_x);
	pr_info("reg_highlight_lut_y = %d\n", pre_tmo_reg_ext->reg_highlight_lut_y);
	pr_info("reg_highlight_lut_pst_gain_en = %d\n",
		pre_tmo_reg_ext->reg_highlight_lut_pst_gain_en);
	pr_info("reg_highlight_lut_pst_gain = %d\n",
		pre_tmo_reg_ext->reg_highlight_lut_pst_gain);
	pr_info("reg_highlight_y_clip_pos_low = %d\n",
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_low);
	pr_info("reg_highlight_y_clip_pos_high = %d\n",
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_high);
	pr_info("reg_highlight_y_clip_pos_sup_high = %d\n",
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_sup_high);
	pr_info("reg_highlight_y_clip_en = %d\n", pre_tmo_reg_ext->reg_highlight_y_clip_en);
	pr_info("reg_fcurv_low_th = %d\n", pre_tmo_reg_ext->reg_fcurv_low_th);
	pr_info("reg_fcurv_mid1_th = %d\n", pre_tmo_reg_ext->reg_fcurv_mid1_th);
	pr_info("reg_fcurv_mid2_th = %d\n", pre_tmo_reg_ext->reg_fcurv_mid2_th);
	pr_info("reg_fcurv_high1_th = %d\n", pre_tmo_reg_ext->reg_fcurv_high1_th);
	pr_info("reg_fcurv_high2_th = %d\n", pre_tmo_reg_ext->reg_fcurv_high2_th);
	pr_info("reg_fcurv_high3_th = %d\n", pre_tmo_reg_ext->reg_fcurv_high3_th);
	pr_info("reg_avg_lum_th0 = %d\n", pre_tmo_reg_ext->reg_avg_lum_th0);
	pr_info("reg_avg_lum_th1 = %d\n", pre_tmo_reg_ext->reg_avg_lum_th1);
	pr_info("reg_avg_lum_th2 = %d\n", pre_tmo_reg_ext->reg_avg_lum_th2);
	pr_info("reg_avg_lum_alpha0 = %d\n", pre_tmo_reg_ext->reg_avg_lum_alpha0);
	pr_info("reg_avg_lum_alpha1 = %d\n", pre_tmo_reg_ext->reg_avg_lum_alpha1);
	pr_info("reg_avg_lum_alpha2 = %d\n", pre_tmo_reg_ext->reg_avg_lum_alpha2);
	pr_info("reg_avg_lum_alpha3 = %d\n", pre_tmo_reg_ext->reg_avg_lum_alpha3);
	pr_info("reg_fcurve_adj_en = %d\n", pre_tmo_reg_ext->reg_fcurve_adj_en);
	pr_info("reg_fcurve_adj_sync_idx = %d %d %d %d %d %d %d %d %d %d\n",
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[0],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[1],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[2],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[3],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[4],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[5],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[6],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[7],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[8],
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[9]);
	pr_info("reg_fcurve_adj_gain = %d %d %d %d %d %d %d %d %d %d\n",
		pre_tmo_reg_ext->reg_fcurve_adj_gain[0], pre_tmo_reg_ext->reg_fcurve_adj_gain[1],
		pre_tmo_reg_ext->reg_fcurve_adj_gain[2], pre_tmo_reg_ext->reg_fcurve_adj_gain[3],
		pre_tmo_reg_ext->reg_fcurve_adj_gain[4], pre_tmo_reg_ext->reg_fcurve_adj_gain[5],
		pre_tmo_reg_ext->reg_fcurve_adj_gain[6], pre_tmo_reg_ext->reg_fcurve_adj_gain[7],
		pre_tmo_reg_ext->reg_fcurve_adj_gain[8], pre_tmo_reg_ext->reg_fcurve_adj_gain[9]);
	pr_info("reg_avg_lum_alpha_sc_flag = %d\n", pre_tmo_reg_ext->reg_avg_lum_alpha_sc_flag);
	pr_info("reg_sc_hist_th = %d\n", pre_tmo_reg_ext->reg_sc_hist_th);
	pr_info("reg_sc_maxl_th = %d\n", pre_tmo_reg_ext->reg_sc_maxl_th);
	for (i = 0; i < 10; i++)
		pr_info("reg_fcurv_clip_val[%d][0~3] = %d %d %d %d\n", i,
			pre_tmo_reg_ext->reg_fcurv_clip_val[i][0],
			pre_tmo_reg_ext->reg_fcurv_clip_val[i][1],
			pre_tmo_reg_ext->reg_fcurv_clip_val[i][2],
			pre_tmo_reg_ext->reg_fcurv_clip_val[i][3]);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_single_bin_th);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_full_black_th);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_full_white_th0);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_full_white_th1);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_special_pat1_th);
	pr_info("reg_single_bin_th = %d\n", pre_tmo_reg_ext->reg_special_pat2_th);
}

void hdr10_tmo_reg_set(struct hdr_tmo_sw *pre_tmo_reg)
{
	if (!pre_tmo_reg) {
		pr_tmo_dbg("pre_tmo_reg is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	tmo_parm_show_pre(pre_tmo_reg);

	if (tmo_reg.tmo_en != pre_tmo_reg->tmo_en)
		change_param((int)pre_tmo_reg->tmo_en);
	tmo_reg.tmo_en = pre_tmo_reg->tmo_en;
	tmo_reg.tmo_highlight = pre_tmo_reg->reg_highlight;
	tmo_reg.tmo_hist_th = pre_tmo_reg->reg_hist_th;
	tmo_reg.tmo_light_th = pre_tmo_reg->reg_light_th;
	tmo_reg.tmo_highlight_th1 = pre_tmo_reg->reg_highlight_th1;
	tmo_reg.tmo_highlight_th2 = pre_tmo_reg->reg_highlight_th2;
	tmo_reg.tmo_display_e = pre_tmo_reg->reg_display_e;
	tmo_reg.tmo_middle_a = pre_tmo_reg->reg_middle_a;
	tmo_reg.tmo_middle_a_adj = pre_tmo_reg->reg_middle_a_adj;
	tmo_reg.tmo_middle_b = pre_tmo_reg->reg_middle_b;
	tmo_reg.tmo_middle_s = pre_tmo_reg->reg_middle_s;
	tmo_reg.tmo_max_th1 = pre_tmo_reg->reg_max_th1;
	tmo_reg.tmo_middle_th = pre_tmo_reg->reg_middle_th;
	tmo_reg.tmo_thold1 = pre_tmo_reg->reg_thold1;
	tmo_reg.tmo_thold2 = pre_tmo_reg->reg_thold2;
	tmo_reg.tmo_thold3 = pre_tmo_reg->reg_thold3;
	tmo_reg.tmo_thold4 = pre_tmo_reg->reg_thold4;
	tmo_reg.tmo_max_th2 = pre_tmo_reg->reg_max_th2;
	tmo_reg.tmo_pnum_th = pre_tmo_reg->reg_pnum_th;
	tmo_reg.tmo_hl0 = pre_tmo_reg->reg_hl0;
	tmo_reg.tmo_hl1 = pre_tmo_reg->reg_hl1;
	tmo_reg.tmo_hl2 = pre_tmo_reg->reg_hl2;
	tmo_reg.tmo_hl3 = pre_tmo_reg->reg_hl3;
	tmo_reg.tmo_display_adj = pre_tmo_reg->reg_display_adj;
	tmo_reg.tmo_avg_th = pre_tmo_reg->reg_avg_th;
	tmo_reg.tmo_avg_adj = pre_tmo_reg->reg_avg_adj;
	tmo_reg.tmo_low_adj = pre_tmo_reg->reg_low_adj;
	tmo_reg.tmo_high_en = pre_tmo_reg->reg_high_en;
	tmo_reg.tmo_high_adj1 = pre_tmo_reg->reg_high_adj1;
	tmo_reg.tmo_high_adj2 = pre_tmo_reg->reg_high_adj2;
	tmo_reg.tmo_high_maxdiff = pre_tmo_reg->reg_high_maxdiff;
	tmo_reg.tmo_high_mindiff = pre_tmo_reg->reg_high_mindiff;
	tmo_reg.tmo_alpha = pre_tmo_reg->alpha;
	tmo_reg.tmo_ratio = pre_tmo_reg->reg_ratio;
	tmo_reg.tmo_max_th3 = pre_tmo_reg->reg_max_th3;
	memcpy(tmo_reg.tmo_oo_init_lut, pre_tmo_reg->oo_init_lut, 13 * sizeof(int));
}

void hdr10_tmo_reg_ext_set(struct hdr_tmo_sw_ext *pre_tmo_reg_ext)
{
	if (!pre_tmo_reg_ext) {
		pr_tmo_dbg("pre_tmo_reg_ext is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	if (pr_tmo_en)
		tmo_parm_ext_show_pre(pre_tmo_reg_ext);

	if (pre_tmo_reg_ext->reg_display_e_bc == 0 &&
		pre_tmo_reg_ext->reg_max_th4 == 0 &&
		pre_tmo_reg_ext->reg_highlight_bc == 0 &&
		pre_tmo_reg_ext->reg_hl0_bc == 0 &&
		pre_tmo_reg_ext->reg_hl1_bc == 0 &&
		pre_tmo_reg_ext->reg_hl2_bc == 0 &&
		pre_tmo_reg_ext->reg_hl3_bc == 0 &&
		pre_tmo_reg_ext->reg_middle_a_bc == 0 &&
		pre_tmo_reg_ext->reg_low_adj_bc == 0 &&
		pre_tmo_reg_ext->reg_olut_bit_mode == 0 &&
		pre_tmo_reg_ext->reg_curve_smo_mode == 0 &&
		pre_tmo_reg_ext->reg_soft_clip_th == 0 &&
		pre_tmo_reg_ext->reg_r0_32 == 0 &&
		pre_tmo_reg_ext->reg_r1_32 == 0 &&
		pre_tmo_reg_ext->reg_y_highlight_ratio == 0 &&
		pre_tmo_reg_ext->reg_ratio_sat == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_ratio == 0 &&
		pre_tmo_reg_ext->reg_model == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_gain == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_clip == 0 &&
		pre_tmo_reg_ext->reg_force_xy_point_en == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_x == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_y == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_pst_gain_en == 0 &&
		pre_tmo_reg_ext->reg_highlight_lut_pst_gain == 0 &&
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_low == 0 &&
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_high == 0 &&
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_sup_high == 0 &&
		pre_tmo_reg_ext->reg_highlight_y_clip_en == 0 &&
		pre_tmo_reg_ext->reg_fcurv_low_th == 0 &&
		pre_tmo_reg_ext->reg_fcurv_mid1_th == 0 &&
		pre_tmo_reg_ext->reg_fcurv_mid2_th == 0 &&
		pre_tmo_reg_ext->reg_fcurv_high1_th == 0 &&
		pre_tmo_reg_ext->reg_fcurv_high2_th == 0 &&
		pre_tmo_reg_ext->reg_fcurv_high3_th == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_th0 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_th1 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_th2 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_alpha0 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_alpha1 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_alpha2 == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_alpha3 == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_en == 0 &&
		pre_tmo_reg_ext->reg_avg_lum_alpha_sc_flag == 0 &&
		pre_tmo_reg_ext->reg_sc_hist_th == 0 &&
		pre_tmo_reg_ext->reg_sc_maxl_th == 0 &&
		pre_tmo_reg_ext->reg_single_bin_th == 0 &&
		pre_tmo_reg_ext->reg_full_black_th == 0 &&
		pre_tmo_reg_ext->reg_full_white_th0 == 0 &&
		pre_tmo_reg_ext->reg_full_white_th1 == 0 &&
		pre_tmo_reg_ext->reg_special_pat1_th == 0 &&
		pre_tmo_reg_ext->reg_special_pat2_th == 0 &&
		pre_tmo_reg_ext->reg_blend_dark_gain == 0 &&
		pre_tmo_reg_ext->reg_blend_dark_th0 == 0 &&
		pre_tmo_reg_ext->reg_blend_dark_th1 == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[0] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[1] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[2] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[3] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[4] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[5] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[6] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[7] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[8] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx[9] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[0] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[1] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[2] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[3] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[4] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[5] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[6] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[7] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[8] == 0 &&
		pre_tmo_reg_ext->reg_fcurve_adj_gain[9] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[0][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[0][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[0][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[0][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[1][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[1][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[1][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[1][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[2][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[2][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[2][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[2][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[3][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[3][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[3][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[3][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[4][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[4][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[4][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[4][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[5][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[5][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[5][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[5][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[6][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[6][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[6][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[6][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[7][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[7][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[7][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[7][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[8][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[8][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[8][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[8][3] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[9][0] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[9][1] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[9][2] == 0 &&
		pre_tmo_reg_ext->reg_fcurv_clip_val[9][3] == 0) {
		pr_tmo_dbg("pre_tmo_reg_ext invalid\n");
		return;
	}

	tmo_reg.tmo_display_e_bc = pre_tmo_reg_ext->reg_display_e_bc;
	tmo_reg.tmo_max_th4 = pre_tmo_reg_ext->reg_max_th4;
	tmo_reg.tmo_highlight_bc = pre_tmo_reg_ext->reg_highlight_bc;
	tmo_reg.tmo_hl0_bc = pre_tmo_reg_ext->reg_hl0_bc;
	tmo_reg.tmo_hl1_bc = pre_tmo_reg_ext->reg_hl1_bc;
	tmo_reg.tmo_hl2_bc = pre_tmo_reg_ext->reg_hl2_bc;
	tmo_reg.tmo_hl3_bc = pre_tmo_reg_ext->reg_hl3_bc;
	tmo_reg.tmo_middle_a_bc = pre_tmo_reg_ext->reg_middle_a_bc;
	tmo_reg.tmo_low_adj_bc = pre_tmo_reg_ext->reg_low_adj_bc;
	tmo_reg.tmo_olut_bit_mode = pre_tmo_reg_ext->reg_olut_bit_mode;
	tmo_reg.tmo_curve_smo_mode = pre_tmo_reg_ext->reg_curve_smo_mode;
	tmo_reg.tmo_soft_clip_th = pre_tmo_reg_ext->reg_soft_clip_th;
	tmo_reg.tmo_r0_32 = pre_tmo_reg_ext->reg_r0_32;
	tmo_reg.tmo_r1_32 = pre_tmo_reg_ext->reg_r1_32;
	tmo_reg.tmo_y_highlight_ratio = pre_tmo_reg_ext->reg_y_highlight_ratio;
	tmo_reg.tmo_ratio_sat = pre_tmo_reg_ext->reg_ratio_sat;
	tmo_reg.tmo_avg_lum_ratio = pre_tmo_reg_ext->reg_avg_lum_ratio;
	tmo_reg.tmo_model = pre_tmo_reg_ext->reg_model;
	tmo_reg.tmo_highlight_lut_gain = pre_tmo_reg_ext->reg_highlight_lut_gain;
	tmo_reg.tmo_highlight_lut_clip = pre_tmo_reg_ext->reg_highlight_lut_clip;
	tmo_reg.tmo_force_xy_point_en = pre_tmo_reg_ext->reg_force_xy_point_en;
	tmo_reg.tmo_highlight_lut_x = pre_tmo_reg_ext->reg_highlight_lut_x;
	tmo_reg.tmo_highlight_lut_y = pre_tmo_reg_ext->reg_highlight_lut_y;
	tmo_reg.tmo_highlight_lut_pst_gain_en =
		pre_tmo_reg_ext->reg_highlight_lut_pst_gain_en;
	tmo_reg.tmo_highlight_lut_pst_gain = pre_tmo_reg_ext->reg_highlight_lut_pst_gain;
	tmo_reg.tmo_highlight_y_clip_pos_low =
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_low;
	tmo_reg.tmo_highlight_y_clip_pos_high =
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_high;
	tmo_reg.tmo_highlight_y_clip_pos_sup_high =
		pre_tmo_reg_ext->reg_highlight_y_clip_pos_sup_high;
	tmo_reg.tmo_highlight_y_clip_en = pre_tmo_reg_ext->reg_highlight_y_clip_en;
	//tmo_reg.tmo_fcurv_low_th = pre_tmo_reg_ext->reg_fcurv_low_th;
	tmo_reg.tmo_fcurv_mid1_th = pre_tmo_reg_ext->reg_fcurv_mid1_th;
	tmo_reg.tmo_fcurv_mid2_th = pre_tmo_reg_ext->reg_fcurv_mid2_th;
	tmo_reg.tmo_fcurv_high1_th = pre_tmo_reg_ext->reg_fcurv_high1_th;
	tmo_reg.tmo_fcurv_high2_th = pre_tmo_reg_ext->reg_fcurv_high2_th;
	tmo_reg.tmo_fcurv_high3_th = pre_tmo_reg_ext->reg_fcurv_high3_th;
	tmo_reg.tmo_avg_lum_th0 = pre_tmo_reg_ext->reg_avg_lum_th0;
	tmo_reg.tmo_avg_lum_th1 = pre_tmo_reg_ext->reg_avg_lum_th1;
	tmo_reg.tmo_avg_lum_th2 = pre_tmo_reg_ext->reg_avg_lum_th2;
	tmo_reg.tmo_avg_lum_alpha0 = pre_tmo_reg_ext->reg_avg_lum_alpha0;
	tmo_reg.tmo_avg_lum_alpha1 = pre_tmo_reg_ext->reg_avg_lum_alpha1;
	tmo_reg.tmo_avg_lum_alpha2 = pre_tmo_reg_ext->reg_avg_lum_alpha2;
	tmo_reg.tmo_avg_lum_alpha3 = pre_tmo_reg_ext->reg_avg_lum_alpha3;
	tmo_reg.tmo_fcurve_adj_en = pre_tmo_reg_ext->reg_fcurve_adj_en;
	tmo_reg.tmo_avg_lum_alpha_sc_flag = pre_tmo_reg_ext->reg_avg_lum_alpha_sc_flag;
	tmo_reg.tmo_sc_hist_th = pre_tmo_reg_ext->reg_sc_hist_th;
	tmo_reg.tmo_sc_maxl_th = pre_tmo_reg_ext->reg_sc_maxl_th;
	tmo_reg.tmo_single_bin_th = pre_tmo_reg_ext->reg_single_bin_th;
	tmo_reg.tmo_full_black_th = pre_tmo_reg_ext->reg_full_black_th;
	tmo_reg.tmo_full_white_th0 = pre_tmo_reg_ext->reg_full_white_th0;
	tmo_reg.tmo_full_white_th1 = pre_tmo_reg_ext->reg_full_white_th1;
	tmo_reg.tmo_special_pat1_th = pre_tmo_reg_ext->reg_special_pat1_th;
	tmo_reg.tmo_special_pat2_th = pre_tmo_reg_ext->reg_special_pat2_th;
	tmo_reg.tmo_blend_dark_gain = pre_tmo_reg_ext->reg_blend_dark_gain;
	tmo_reg.tmo_blend_dark_th0 = pre_tmo_reg_ext->reg_blend_dark_th0;
	tmo_reg.tmo_blend_dark_th1 = pre_tmo_reg_ext->reg_blend_dark_th1;
	memcpy(tmo_reg.tmo_fcurve_adj_sync_idx,
		pre_tmo_reg_ext->reg_fcurve_adj_sync_idx, 10 * sizeof(int));
	memcpy(tmo_reg.tmo_fcurve_adj_gain,
		pre_tmo_reg_ext->reg_fcurve_adj_gain, 10 * sizeof(int));
	memcpy(tmo_reg.tmo_fcurv_clip_val,
		pre_tmo_reg_ext->reg_fcurv_clip_val, 40 * sizeof(int));
}

void hdr10_tmo_reg_get(struct hdr_tmo_sw *pre_tmo_reg_s)
{
	if (!pre_tmo_reg_s) {
		pr_tmo_dbg("pre_tmo_reg_s is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	pre_tmo_reg_s->tmo_en = tmo_reg.tmo_en;
	pre_tmo_reg_s->reg_highlight = tmo_reg.tmo_highlight;
	pre_tmo_reg_s->reg_hist_th = tmo_reg.tmo_hist_th;
	pre_tmo_reg_s->reg_light_th = tmo_reg.tmo_light_th;
	pre_tmo_reg_s->reg_highlight_th1 = tmo_reg.tmo_highlight_th1;
	pre_tmo_reg_s->reg_highlight_th2 = tmo_reg.tmo_highlight_th2;
	pre_tmo_reg_s->reg_display_e = tmo_reg.tmo_display_e;
	pre_tmo_reg_s->reg_middle_a = tmo_reg.tmo_middle_a;
	pre_tmo_reg_s->reg_middle_a_adj = tmo_reg.tmo_middle_a_adj;
	pre_tmo_reg_s->reg_middle_b = tmo_reg.tmo_middle_b;
	pre_tmo_reg_s->reg_middle_s = tmo_reg.tmo_middle_s;
	pre_tmo_reg_s->reg_max_th1 = tmo_reg.tmo_max_th1;
	pre_tmo_reg_s->reg_middle_th = tmo_reg.tmo_middle_th;
	pre_tmo_reg_s->reg_thold1 = tmo_reg.tmo_thold1;
	pre_tmo_reg_s->reg_thold2 = tmo_reg.tmo_thold2;
	pre_tmo_reg_s->reg_thold3 = tmo_reg.tmo_thold3;
	pre_tmo_reg_s->reg_thold4 = tmo_reg.tmo_thold4;
	pre_tmo_reg_s->reg_max_th2 = tmo_reg.tmo_max_th2;
	pre_tmo_reg_s->reg_pnum_th = tmo_reg.tmo_pnum_th;
	pre_tmo_reg_s->reg_hl0 = tmo_reg.tmo_hl0;
	pre_tmo_reg_s->reg_hl1 = tmo_reg.tmo_hl1;
	pre_tmo_reg_s->reg_hl2 = tmo_reg.tmo_hl2;
	pre_tmo_reg_s->reg_hl3 = tmo_reg.tmo_hl3;
	pre_tmo_reg_s->reg_display_adj = tmo_reg.tmo_display_adj;
	pre_tmo_reg_s->reg_avg_th = tmo_reg.tmo_avg_th;
	pre_tmo_reg_s->reg_avg_adj = tmo_reg.tmo_avg_adj;
	pre_tmo_reg_s->reg_low_adj = tmo_reg.tmo_low_adj;
	pre_tmo_reg_s->reg_high_en = tmo_reg.tmo_high_en;
	pre_tmo_reg_s->reg_high_adj1 = tmo_reg.tmo_high_adj1;
	pre_tmo_reg_s->reg_high_adj2 = tmo_reg.tmo_high_adj2;
	pre_tmo_reg_s->reg_high_maxdiff = tmo_reg.tmo_high_maxdiff;
	pre_tmo_reg_s->reg_high_mindiff = tmo_reg.tmo_high_mindiff;
	pre_tmo_reg_s->alpha = tmo_reg.tmo_alpha;
	pre_tmo_reg_s->reg_ratio = tmo_reg.tmo_ratio;
	pre_tmo_reg_s->reg_max_th3 = tmo_reg.tmo_max_th3;
	memcpy(pre_tmo_reg_s->oo_init_lut, tmo_reg.tmo_oo_init_lut, 13 * sizeof(int));
}

void hdr10_tmo_reg_ext_get(struct hdr_tmo_sw_ext *pre_tmo_reg_ext_s)
{
	if (!pre_tmo_reg_ext_s) {
		pr_tmo_dbg("pre_tmo_reg_ext_s is NULL\n");
		pr_tmo_en = 0;
		return;
	}

	pre_tmo_reg_ext_s->reg_display_e_bc  = tmo_reg.tmo_display_e_bc;
	pre_tmo_reg_ext_s->reg_max_th4  = tmo_reg.tmo_max_th4;
	pre_tmo_reg_ext_s->reg_highlight_bc  = tmo_reg.tmo_highlight_bc;
	pre_tmo_reg_ext_s->reg_hl0_bc  = tmo_reg.tmo_hl0_bc;
	pre_tmo_reg_ext_s->reg_hl1_bc  = tmo_reg.tmo_hl1_bc;
	pre_tmo_reg_ext_s->reg_hl2_bc  = tmo_reg.tmo_hl2_bc;
	pre_tmo_reg_ext_s->reg_hl3_bc  = tmo_reg.tmo_hl3_bc;
	pre_tmo_reg_ext_s->reg_middle_a_bc  = tmo_reg.tmo_middle_a_bc;
	pre_tmo_reg_ext_s->reg_low_adj_bc  = tmo_reg.tmo_low_adj_bc;
	pre_tmo_reg_ext_s->reg_olut_bit_mode  = tmo_reg.tmo_olut_bit_mode;
	pre_tmo_reg_ext_s->reg_curve_smo_mode  = tmo_reg.tmo_curve_smo_mode;
	pre_tmo_reg_ext_s->reg_soft_clip_th  = tmo_reg.tmo_soft_clip_th;
	pre_tmo_reg_ext_s->reg_r0_32  = tmo_reg.tmo_r0_32;
	pre_tmo_reg_ext_s->reg_r1_32  = tmo_reg.tmo_r1_32;
	pre_tmo_reg_ext_s->reg_y_highlight_ratio  = tmo_reg.tmo_y_highlight_ratio;
	pre_tmo_reg_ext_s->reg_ratio_sat  = tmo_reg.tmo_ratio_sat;
	pre_tmo_reg_ext_s->reg_avg_lum_ratio  = tmo_reg.tmo_avg_lum_ratio;
	pre_tmo_reg_ext_s->reg_model  = tmo_reg.tmo_model;
	pre_tmo_reg_ext_s->reg_highlight_lut_gain  = tmo_reg.tmo_highlight_lut_gain;
	pre_tmo_reg_ext_s->reg_highlight_lut_clip  = tmo_reg.tmo_highlight_lut_clip;
	pre_tmo_reg_ext_s->reg_force_xy_point_en  = tmo_reg.tmo_force_xy_point_en;
	pre_tmo_reg_ext_s->reg_highlight_lut_x  = tmo_reg.tmo_highlight_lut_x;
	pre_tmo_reg_ext_s->reg_highlight_lut_y  = tmo_reg.tmo_highlight_lut_y;
	pre_tmo_reg_ext_s->reg_highlight_lut_pst_gain_en  = tmo_reg.tmo_highlight_lut_pst_gain_en;
	pre_tmo_reg_ext_s->reg_highlight_lut_pst_gain  = tmo_reg.tmo_highlight_lut_pst_gain;
	pre_tmo_reg_ext_s->reg_highlight_y_clip_pos_low  = tmo_reg.tmo_highlight_y_clip_pos_low;
	pre_tmo_reg_ext_s->reg_highlight_y_clip_pos_high  = tmo_reg.tmo_highlight_y_clip_pos_high;
	pre_tmo_reg_ext_s->reg_highlight_y_clip_pos_sup_high  =
		tmo_reg.tmo_highlight_y_clip_pos_sup_high;
	pre_tmo_reg_ext_s->reg_highlight_y_clip_en  = tmo_reg.tmo_highlight_y_clip_en;
	pre_tmo_reg_ext_s->reg_fcurv_low_th  = tmo_reg.tmo_fcurv_low_th;
	pre_tmo_reg_ext_s->reg_fcurv_mid1_th  = tmo_reg.tmo_fcurv_mid1_th;
	pre_tmo_reg_ext_s->reg_fcurv_mid2_th  = tmo_reg.tmo_fcurv_mid2_th;
	pre_tmo_reg_ext_s->reg_fcurv_high1_th  = tmo_reg.tmo_fcurv_high1_th;
	pre_tmo_reg_ext_s->reg_fcurv_high2_th  = tmo_reg.tmo_fcurv_high2_th;
	pre_tmo_reg_ext_s->reg_fcurv_high3_th  = tmo_reg.tmo_fcurv_high3_th;
	pre_tmo_reg_ext_s->reg_avg_lum_th0  = tmo_reg.tmo_avg_lum_th0;
	pre_tmo_reg_ext_s->reg_avg_lum_th1  = tmo_reg.tmo_avg_lum_th1;
	pre_tmo_reg_ext_s->reg_avg_lum_th2  = tmo_reg.tmo_avg_lum_th2;
	pre_tmo_reg_ext_s->reg_avg_lum_alpha0  = tmo_reg.tmo_avg_lum_alpha0;
	pre_tmo_reg_ext_s->reg_avg_lum_alpha1  = tmo_reg.tmo_avg_lum_alpha1;
	pre_tmo_reg_ext_s->reg_avg_lum_alpha2  = tmo_reg.tmo_avg_lum_alpha2;
	pre_tmo_reg_ext_s->reg_avg_lum_alpha3  = tmo_reg.tmo_avg_lum_alpha3;
	pre_tmo_reg_ext_s->reg_fcurve_adj_en  = tmo_reg.tmo_fcurve_adj_en;
	pre_tmo_reg_ext_s->reg_avg_lum_alpha_sc_flag  = tmo_reg.tmo_avg_lum_alpha_sc_flag;
	pre_tmo_reg_ext_s->reg_sc_hist_th  = tmo_reg.tmo_sc_hist_th;
	pre_tmo_reg_ext_s->reg_sc_maxl_th  = tmo_reg.tmo_sc_maxl_th;
	pre_tmo_reg_ext_s->reg_single_bin_th  = tmo_reg.tmo_single_bin_th;
	pre_tmo_reg_ext_s->reg_full_black_th = tmo_reg.tmo_full_black_th;
	pre_tmo_reg_ext_s->reg_full_white_th0 = tmo_reg.tmo_full_white_th0;
	pre_tmo_reg_ext_s->reg_full_white_th1 = tmo_reg.tmo_full_white_th1;
	pre_tmo_reg_ext_s->reg_special_pat1_th = tmo_reg.tmo_special_pat1_th;
	pre_tmo_reg_ext_s->reg_special_pat2_th = tmo_reg.tmo_special_pat2_th;
	pre_tmo_reg_ext_s->reg_blend_dark_gain = tmo_reg.tmo_blend_dark_gain;
	pre_tmo_reg_ext_s->reg_blend_dark_th0 = tmo_reg.tmo_blend_dark_th0;
	pre_tmo_reg_ext_s->reg_blend_dark_th1 = tmo_reg.tmo_blend_dark_th1;
	memcpy(pre_tmo_reg_ext_s->reg_fcurve_adj_sync_idx,
		tmo_reg.tmo_fcurve_adj_sync_idx, 10 * sizeof(int));
	memcpy(pre_tmo_reg_ext_s->reg_fcurve_adj_gain,
		tmo_reg.tmo_fcurve_adj_gain, 10 * sizeof(int));
	memcpy(pre_tmo_reg_ext_s->reg_fcurv_clip_val,
		tmo_reg.tmo_fcurv_clip_val, 40 * sizeof(int));
}

static char hdr_tmo_debug_usage_str[TMO_PARAM_CNT][LINE_SIZE] = {
	"tmo_en = ",
	"display_e = ",
	"display_e_bc = ",
	"max_th1 = ",
	"max_th2 = ",
	"max_th3 = ",
	"max_th4 = ",
	"highlight = ",
	"highlight_bc = ",
	"ratio = ",
	"light_th = ",
	"hist_th = ",
	"highlight_th1 = ",
	"highlight_th2 = ",
	"display_adj = ",
	"avg_th = ",
	"avg_adj = ",
	"hl0 = ",
	"hl1 = ",
	"hl2 = ",
	"hl3 = ",
	"hl0_bc = ",
	"hl1_bc = ",
	"hl2_bc = ",
	"hl3_bc = ",
	"pnum_th = ",
	"thold1 = ",
	"thold2 = ",
	"thold3 = ",
	"thold4 = ",
	"middle_th = ",
	"middle_a = ",
	"middle_a_bc = ",
	"middle_a_adj = ",
	"middle_b = ",
	"middle_s = ",
	"low_adj = ",
	"low_adj_bc = ",
	"high_en = ",
	"olut_bit_mode = ",
	"high_adj1 = ",
	"high_adj2 = ",
	"high_maxdiff = ",
	"high_mindiff = ",
	"curve_smo_mode = ",
	"soft_clip_th = ",
	"r0_32 = ",
	"r1_32 = ",
	"y_highlight_ratio = ",
	"ratio_sat = ",
	"avg_lum_ratio = ",
	"alpha = ",
	"model = ",
	"highlight_lut_gain = ",
	"highlight_lut_clip = ",
	"force_xy_point_en = ",
	"highlight_lut_x = ",
	"highlight_lut_y = ",
	"highlight_lut_pst_gain_en = ",
	"highlight_lut_pst_gain = ",
	"highlight_y_clip_pos_low = ",
	"highlight_y_clip_pos_high = ",
	"highlight_y_clip_pos_sup_high = ",
	"highlight_y_clip_en = ",
	"fcurv_low_th = ",
	"fcurv_mid1_th = ",
	"fcurv_mid2_th = ",
	"fcurv_high1_th = ",
	"fcurv_high2_th = ",
	"fcurv_high3_th = ",
	"avg_lum_th0 = ",
	"avg_lum_th1 = ",
	"avg_lum_th2 = ",
	"avg_lum_alpha0 = ",
	"avg_lum_alpha1 = ",
	"avg_lum_alpha2 = ",
	"avg_lum_alpha3 = ",
	"fcurve_adj_en = ",
	"fcurve_adj_sync_idx[0] = ",
	"fcurve_adj_sync_idx[1] = ",
	"fcurve_adj_sync_idx[2] = ",
	"fcurve_adj_sync_idx[3] = ",
	"fcurve_adj_sync_idx[4] = ",
	"fcurve_adj_sync_idx[5] = ",
	"fcurve_adj_sync_idx[6] = ",
	"fcurve_adj_sync_idx[7] = ",
	"fcurve_adj_sync_idx[8] = ",
	"fcurve_adj_sync_idx[9] = ",
	"fcurve_adj_gain[0] = ",
	"fcurve_adj_gain[1] = ",
	"fcurve_adj_gain[2] = ",
	"fcurve_adj_gain[3] = ",
	"fcurve_adj_gain[4] = ",
	"fcurve_adj_gain[5] = ",
	"fcurve_adj_gain[6] = ",
	"fcurve_adj_gain[7] = ",
	"fcurve_adj_gain[8] = ",
	"fcurve_adj_gain[9] = ",
	"avg_lum_alpha_sc_flag = ",
	"sc_hist_th = ",
	"sc_maxl_th = ",
	"fcurv_clip_val[0][0] = ",
	"fcurv_clip_val[0][1] = ",
	"fcurv_clip_val[0][2] = ",
	"fcurv_clip_val[0][3] = ",
	"fcurv_clip_val[1][0] = ",
	"fcurv_clip_val[1][1] = ",
	"fcurv_clip_val[1][2] = ",
	"fcurv_clip_val[1][3] = ",
	"fcurv_clip_val[2][0] = ",
	"fcurv_clip_val[2][1] = ",
	"fcurv_clip_val[2][2] = ",
	"fcurv_clip_val[2][3] = ",
	"fcurv_clip_val[3][0] = ",
	"fcurv_clip_val[3][1] = ",
	"fcurv_clip_val[3][2] = ",
	"fcurv_clip_val[3][3] = ",
	"fcurv_clip_val[4][0] = ",
	"fcurv_clip_val[4][1] = ",
	"fcurv_clip_val[4][2] = ",
	"fcurv_clip_val[4][3] = ",
	"fcurv_clip_val[5][0] = ",
	"fcurv_clip_val[5][1] = ",
	"fcurv_clip_val[5][2] = ",
	"fcurv_clip_val[5][3] = ",
	"fcurv_clip_val[6][0] = ",
	"fcurv_clip_val[6][1] = ",
	"fcurv_clip_val[6][2] = ",
	"fcurv_clip_val[6][3] = ",
	"fcurv_clip_val[7][0] = ",
	"fcurv_clip_val[7][1] = ",
	"fcurv_clip_val[7][2] = ",
	"fcurv_clip_val[7][3] = ",
	"fcurv_clip_val[8][0] = ",
	"fcurv_clip_val[8][1] = ",
	"fcurv_clip_val[8][2] = ",
	"fcurv_clip_val[8][3] = ",
	"fcurv_clip_val[9][0] = ",
	"fcurv_clip_val[9][1] = ",
	"fcurv_clip_val[9][2] = ",
	"fcurv_clip_val[9][3] = ",
	"single_bin_th = ",
	"oo_init_lut[0] = ",
	"oo_init_lut[1] = ",
	"oo_init_lut[2] = ",
	"oo_init_lut[3] = ",
	"oo_init_lut[4] = ",
	"oo_init_lut[5] = ",
	"oo_init_lut[6] = ",
	"oo_init_lut[7] = ",
	"oo_init_lut[8] = ",
	"oo_init_lut[9] = ",
	"oo_init_lut[10] = ",
	"oo_init_lut[11] = ",
	"oo_init_lut[12] = ",
	"full_black_th = ",
	"full_white_th0 = ",
	"full_white_th1 = ",
	"special_pat1_th = ",
	"special_pat2_th = ",
	"blend_dark_gain = ",
	"blend_dark_th0 = ",
	"blend_dark_th1 = ",
};

void hdr10_tmo_reg_get_arr(int *arry)
{
	arry[0] = tmo_reg.tmo_en;
	arry[1] = tmo_reg.tmo_display_e;
	arry[2] = tmo_reg.tmo_display_e_bc;
	arry[3] = tmo_reg.tmo_max_th1;
	arry[4] = tmo_reg.tmo_max_th2;
	arry[5] = tmo_reg.tmo_max_th3;
	arry[6] = tmo_reg.tmo_max_th4;
	arry[7] = tmo_reg.tmo_highlight;
	arry[8] = tmo_reg.tmo_highlight_bc;
	arry[9] = tmo_reg.tmo_ratio;
	arry[10] = tmo_reg.tmo_light_th;
	arry[11] = tmo_reg.tmo_hist_th;
	arry[12] = tmo_reg.tmo_highlight_th1;
	arry[13] = tmo_reg.tmo_highlight_th2;
	arry[14] = tmo_reg.tmo_display_adj;
	arry[15] = tmo_reg.tmo_avg_th;
	arry[16] = tmo_reg.tmo_avg_adj;
	arry[17] = tmo_reg.tmo_hl0;
	arry[18] = tmo_reg.tmo_hl1;
	arry[19] = tmo_reg.tmo_hl2;
	arry[20] = tmo_reg.tmo_hl3;
	arry[21] = tmo_reg.tmo_hl0_bc;
	arry[22] = tmo_reg.tmo_hl1_bc;
	arry[23] = tmo_reg.tmo_hl2_bc;
	arry[24] = tmo_reg.tmo_hl3_bc;
	arry[25] = tmo_reg.tmo_pnum_th;
	arry[26] = tmo_reg.tmo_thold1;
	arry[27] = tmo_reg.tmo_thold2;
	arry[28] = tmo_reg.tmo_thold3;
	arry[29] = tmo_reg.tmo_thold4;
	arry[30] = tmo_reg.tmo_middle_th;
	arry[31] = tmo_reg.tmo_middle_a;
	arry[32] = tmo_reg.tmo_middle_a_bc;
	arry[33] = tmo_reg.tmo_middle_a_adj;
	arry[34] = tmo_reg.tmo_middle_b;
	arry[35] = tmo_reg.tmo_middle_s;
	arry[36] = tmo_reg.tmo_low_adj;
	arry[37] = tmo_reg.tmo_low_adj_bc;
	arry[38] = tmo_reg.tmo_high_en;
	arry[39] = tmo_reg.tmo_olut_bit_mode;
	arry[40] = tmo_reg.tmo_high_adj1;
	arry[41] = tmo_reg.tmo_high_adj2;
	arry[42] = tmo_reg.tmo_high_maxdiff;
	arry[43] = tmo_reg.tmo_high_mindiff;
	arry[44] = tmo_reg.tmo_curve_smo_mode;
	arry[45] = tmo_reg.tmo_soft_clip_th;
	arry[46] = tmo_reg.tmo_r0_32;
	arry[47] = tmo_reg.tmo_r1_32;
	arry[48] = tmo_reg.tmo_y_highlight_ratio;
	arry[49] = tmo_reg.tmo_ratio_sat;
	arry[50] = tmo_reg.tmo_avg_lum_ratio;
	arry[51] = tmo_reg.tmo_alpha;
	arry[52] = tmo_reg.tmo_model;
	arry[53] = tmo_reg.tmo_highlight_lut_gain;
	arry[54] = tmo_reg.tmo_highlight_lut_clip;
	arry[55] = tmo_reg.tmo_force_xy_point_en;
	arry[56] = tmo_reg.tmo_highlight_lut_x;
	arry[57] = tmo_reg.tmo_highlight_lut_y;
	arry[58] = tmo_reg.tmo_highlight_lut_pst_gain_en;
	arry[59] = tmo_reg.tmo_highlight_lut_pst_gain;
	arry[60] = tmo_reg.tmo_highlight_y_clip_pos_low;
	arry[61] = tmo_reg.tmo_highlight_y_clip_pos_high;
	arry[62] = tmo_reg.tmo_highlight_y_clip_pos_sup_high;
	arry[63] = tmo_reg.tmo_highlight_y_clip_en;
	arry[64] = tmo_reg.tmo_fcurv_low_th;
	arry[65] = tmo_reg.tmo_fcurv_mid1_th;
	arry[66] = tmo_reg.tmo_fcurv_mid2_th;
	arry[67] = tmo_reg.tmo_fcurv_high1_th;
	arry[68] = tmo_reg.tmo_fcurv_high2_th;
	arry[69] = tmo_reg.tmo_fcurv_high3_th;
	arry[70] = tmo_reg.tmo_avg_lum_th0;
	arry[71] = tmo_reg.tmo_avg_lum_th1;
	arry[72] = tmo_reg.tmo_avg_lum_th2;
	arry[73] = tmo_reg.tmo_avg_lum_alpha0;
	arry[74] = tmo_reg.tmo_avg_lum_alpha1;
	arry[75] = tmo_reg.tmo_avg_lum_alpha2;
	arry[76] = tmo_reg.tmo_avg_lum_alpha3;
	arry[77] = tmo_reg.tmo_fcurve_adj_en;
	arry[78] = tmo_reg.tmo_fcurve_adj_sync_idx[0];
	arry[79] = tmo_reg.tmo_fcurve_adj_sync_idx[1];
	arry[80] = tmo_reg.tmo_fcurve_adj_sync_idx[2];
	arry[81] = tmo_reg.tmo_fcurve_adj_sync_idx[3];
	arry[82] = tmo_reg.tmo_fcurve_adj_sync_idx[4];
	arry[83] = tmo_reg.tmo_fcurve_adj_sync_idx[5];
	arry[84] = tmo_reg.tmo_fcurve_adj_sync_idx[6];
	arry[85] = tmo_reg.tmo_fcurve_adj_sync_idx[7];
	arry[86] = tmo_reg.tmo_fcurve_adj_sync_idx[8];
	arry[87] = tmo_reg.tmo_fcurve_adj_sync_idx[9];
	arry[88] = tmo_reg.tmo_fcurve_adj_gain[0];
	arry[89] = tmo_reg.tmo_fcurve_adj_gain[1];
	arry[90] = tmo_reg.tmo_fcurve_adj_gain[2];
	arry[91] = tmo_reg.tmo_fcurve_adj_gain[3];
	arry[92] = tmo_reg.tmo_fcurve_adj_gain[4];
	arry[93] = tmo_reg.tmo_fcurve_adj_gain[5];
	arry[94] = tmo_reg.tmo_fcurve_adj_gain[6];
	arry[95] = tmo_reg.tmo_fcurve_adj_gain[7];
	arry[96] = tmo_reg.tmo_fcurve_adj_gain[8];
	arry[97] = tmo_reg.tmo_fcurve_adj_gain[9];
	arry[98] = tmo_reg.tmo_avg_lum_alpha_sc_flag;
	arry[99] = tmo_reg.tmo_sc_hist_th;
	arry[100] = tmo_reg.tmo_sc_maxl_th;
	arry[101] = tmo_reg.tmo_fcurv_clip_val[0][0];
	arry[102] = tmo_reg.tmo_fcurv_clip_val[0][1];
	arry[103] = tmo_reg.tmo_fcurv_clip_val[0][2];
	arry[104] = tmo_reg.tmo_fcurv_clip_val[0][3];
	arry[105] = tmo_reg.tmo_fcurv_clip_val[1][0];
	arry[106] = tmo_reg.tmo_fcurv_clip_val[1][1];
	arry[107] = tmo_reg.tmo_fcurv_clip_val[1][2];
	arry[108] = tmo_reg.tmo_fcurv_clip_val[1][3];
	arry[109] = tmo_reg.tmo_fcurv_clip_val[2][0];
	arry[110] = tmo_reg.tmo_fcurv_clip_val[2][1];
	arry[111] = tmo_reg.tmo_fcurv_clip_val[2][2];
	arry[112] = tmo_reg.tmo_fcurv_clip_val[2][3];
	arry[113] = tmo_reg.tmo_fcurv_clip_val[3][0];
	arry[114] = tmo_reg.tmo_fcurv_clip_val[3][1];
	arry[115] = tmo_reg.tmo_fcurv_clip_val[3][2];
	arry[116] = tmo_reg.tmo_fcurv_clip_val[3][3];
	arry[117] = tmo_reg.tmo_fcurv_clip_val[4][0];
	arry[118] = tmo_reg.tmo_fcurv_clip_val[4][1];
	arry[119] = tmo_reg.tmo_fcurv_clip_val[4][2];
	arry[120] = tmo_reg.tmo_fcurv_clip_val[4][3];
	arry[121] = tmo_reg.tmo_fcurv_clip_val[5][0];
	arry[122] = tmo_reg.tmo_fcurv_clip_val[5][1];
	arry[123] = tmo_reg.tmo_fcurv_clip_val[5][2];
	arry[124] = tmo_reg.tmo_fcurv_clip_val[5][3];
	arry[125] = tmo_reg.tmo_fcurv_clip_val[6][0];
	arry[126] = tmo_reg.tmo_fcurv_clip_val[6][1];
	arry[127] = tmo_reg.tmo_fcurv_clip_val[6][2];
	arry[128] = tmo_reg.tmo_fcurv_clip_val[6][3];
	arry[129] = tmo_reg.tmo_fcurv_clip_val[7][0];
	arry[130] = tmo_reg.tmo_fcurv_clip_val[7][1];
	arry[131] = tmo_reg.tmo_fcurv_clip_val[7][2];
	arry[132] = tmo_reg.tmo_fcurv_clip_val[7][3];
	arry[133] = tmo_reg.tmo_fcurv_clip_val[8][0];
	arry[134] = tmo_reg.tmo_fcurv_clip_val[8][1];
	arry[135] = tmo_reg.tmo_fcurv_clip_val[8][2];
	arry[136] = tmo_reg.tmo_fcurv_clip_val[8][3];
	arry[137] = tmo_reg.tmo_fcurv_clip_val[9][0];
	arry[138] = tmo_reg.tmo_fcurv_clip_val[9][1];
	arry[139] = tmo_reg.tmo_fcurv_clip_val[9][2];
	arry[140] = tmo_reg.tmo_fcurv_clip_val[9][3];
	arry[141] = tmo_reg.tmo_single_bin_th;
	arry[142] = tmo_reg.tmo_oo_init_lut[0];
	arry[143] = tmo_reg.tmo_oo_init_lut[1];
	arry[144] = tmo_reg.tmo_oo_init_lut[2];
	arry[145] = tmo_reg.tmo_oo_init_lut[3];
	arry[146] = tmo_reg.tmo_oo_init_lut[4];
	arry[147] = tmo_reg.tmo_oo_init_lut[5];
	arry[148] = tmo_reg.tmo_oo_init_lut[6];
	arry[149] = tmo_reg.tmo_oo_init_lut[7];
	arry[150] = tmo_reg.tmo_oo_init_lut[8];
	arry[151] = tmo_reg.tmo_oo_init_lut[9];
	arry[152] = tmo_reg.tmo_oo_init_lut[10];
	arry[153] = tmo_reg.tmo_oo_init_lut[11];
	arry[154] = tmo_reg.tmo_oo_init_lut[12];
	arry[155] = tmo_reg.tmo_full_black_th;
	arry[156] = tmo_reg.tmo_full_white_th0;
	arry[157] = tmo_reg.tmo_full_white_th1;
	arry[158] = tmo_reg.tmo_special_pat1_th;
	arry[159] = tmo_reg.tmo_special_pat2_th;
	arry[160] = tmo_reg.tmo_blend_dark_gain;
	arry[161] = tmo_reg.tmo_blend_dark_th0;
	arry[162] = tmo_reg.tmo_blend_dark_th1;
}

int hdr_tmo_adb_show(char *str)
{
	int tmo_val[TMO_PARAM_CNT];
	int i = 0;
	int len1 = 0;

	hdr10_tmo_reg_get_arr(tmo_val);

	for (i = 0; i < TMO_PARAM_CNT; i++) {
		len1 += snprintf(str + len1, LINE_SIZE, "%s", hdr_tmo_debug_usage_str[i]);
		len1 += snprintf(str + len1, LINE_SIZE, "%d\n", tmo_val[i]);
	}

	return len1;
}

struct aml_tmo_reg_sw *tmo_fw_param_get(void)
{
	return &tmo_reg;
};
EXPORT_SYMBOL(tmo_fw_param_get);

int set_hdr_tmo_reg(void)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;

	pre_tmo_reg = tmo_fw_param_get();

	if (chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		pre_tmo_reg->tmo_model = 2;

	return 0;
}

void hdr_first_frame_flag_update(void)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;

	pre_tmo_reg = tmo_fw_param_get();
	if (!(pre_tmo_reg->tmo_fcurv_low_th)) {
		pre_tmo_reg->tmo_fcurv_low_th = 1;
		pr_tmo_dbg("hdr first frame flag: tmo_fcurv_low_th = %x\n",
			pre_tmo_reg->tmo_fcurv_low_th);
	}
}

int check_hist_available(int *hist_hdr)
{
	int i = 0;
	int ret = 0;

	if (hist_hdr) {
		for (i = 0; i < 128; i++) {
			if (hist_hdr[i] != 0) {
				ret = 1;
				break;
			}
		}
	}

	return ret;
}

void hdr10_tmo_gen(u32 *oo_gain, u32 *cgain, u32 *oo_gain1, u32 hdr_convert_mode, int hist_flag)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;
	struct vpp_hist_param_s *p = get_vpp_hist();
	unsigned short *vpp_hist = p->vpp_gamma;
	struct aml_hdr_prm_s hdr_luts = {{0}, {0}};
	int *hist_hdr = NULL;
	struct aml_tmo_hdr_hdr_reg_sw *sbtm_hdr_tmo_reg = tmo_hdr_hdr_reg_get();

	pre_tmo_reg = tmo_fw_param_get();
	if (!pre_tmo_reg->pre_hdr10_tmo_alg) {
		pr_tmo_dbg("hdr10_tmo alg func is NULL\n");
		return;
	}

	if (hist_flag) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_t3x)
			hist_hdr = &s5_hdr_hist[NUM_HDR_HIST - 1][0];
		else
#endif
			hist_hdr = &hdr_hist[NUM_HDR_HIST - 1][0];

		if (!check_hist_available(hist_hdr)) {
			pr_tmo_dbg("hdr_hist unavailable\n");
			return;
		}
	}

	tmo_reg.pre_hdr10_tmo_alg(&hdr_luts, pre_tmo_reg, sbtm_hdr_tmo_reg, hist_hdr,
		vpp_hist, hdr_convert_mode);

	memcpy(oo_gain, hdr_luts.hdr_ogain_lut, sizeof(int) * 149);
	memcpy(cgain, hdr_luts.hdr_cgain_lut, sizeof(int) * 65);
	if (tmo_force_ootf1_mode)
		memcpy(oo_gain1, hdr_luts.hdr_ogain_lut1, sizeof(int) * 149);
}

void hdr10_tmo_parm_show(void)
{
	int i;

	pr_info("tmo_en = %d\n", tmo_reg.tmo_en);
	pr_info("tmo_display_e = %d\n", tmo_reg.tmo_display_e);
	pr_info("tmo_display_e_bc = %d\n", tmo_reg.tmo_display_e_bc);
	pr_info("tmo_max_th1 = %d\n", tmo_reg.tmo_max_th1);
	pr_info("tmo_max_th2 = %d\n", tmo_reg.tmo_max_th2);
	pr_info("tmo_max_th3 = %d\n", tmo_reg.tmo_max_th3);
	pr_info("tmo_max_th4 = %d\n", tmo_reg.tmo_max_th4);
	pr_info("tmo_highlight = %d\n", tmo_reg.tmo_highlight);
	pr_info("tmo_highlight_bc = %d\n", tmo_reg.tmo_highlight_bc);
	pr_info("tmo_ratio = %d\n", tmo_reg.tmo_ratio);
	pr_info("tmo_light_th = %d\n", tmo_reg.tmo_light_th);
	pr_info("tmo_hist_th = %d\n", tmo_reg.tmo_hist_th);
	pr_info("tmo_highlight_th1 = %d\n", tmo_reg.tmo_highlight_th1);
	pr_info("tmo_highlight_th2 = %d\n", tmo_reg.tmo_highlight_th2);
	pr_info("tmo_display_adj = %d\n", tmo_reg.tmo_display_adj);
	pr_info("tmo_avg_th = %d\n", tmo_reg.tmo_avg_th);
	pr_info("tmo_avg_adj = %d\n", tmo_reg.tmo_avg_adj);
	pr_info("tmo_hl0 = %d\n", tmo_reg.tmo_hl0);
	pr_info("tmo_hl1 = %d\n", tmo_reg.tmo_hl1);
	pr_info("tmo_hl2 = %d\n", tmo_reg.tmo_hl2);
	pr_info("tmo_hl3 = %d\n", tmo_reg.tmo_hl3);
	pr_info("tmo_hl0_bc = %d\n", tmo_reg.tmo_hl0_bc);
	pr_info("tmo_hl1_bc = %d\n", tmo_reg.tmo_hl1_bc);
	pr_info("tmo_hl2_bc = %d\n", tmo_reg.tmo_hl2_bc);
	pr_info("tmo_hl3_bc = %d\n", tmo_reg.tmo_hl3_bc);
	pr_info("tmo_pnum_th = %d\n", tmo_reg.tmo_pnum_th);
	pr_info("tmo_thold1 = %d\n", tmo_reg.tmo_thold1);
	pr_info("tmo_thold2 = %d\n", tmo_reg.tmo_thold2);
	pr_info("tmo_thold3 = %d\n", tmo_reg.tmo_thold3);
	pr_info("tmo_thold4 = %d\n", tmo_reg.tmo_thold4);
	pr_info("tmo_middle_th = %d\n", tmo_reg.tmo_middle_th);
	pr_info("tmo_middle_a = %d\n", tmo_reg.tmo_middle_a);
	pr_info("tmo_middle_a_bc = %d\n", tmo_reg.tmo_middle_a_bc);
	pr_info("tmo_middle_a_adj = %d\n", tmo_reg.tmo_middle_a_adj);
	pr_info("tmo_middle_b = %d\n", tmo_reg.tmo_middle_b);
	pr_info("tmo_middle_s = %d\n", tmo_reg.tmo_middle_s);
	pr_info("tmo_low_adj = %d\n", tmo_reg.tmo_low_adj);
	pr_info("tmo_low_adj_bc = %d\n", tmo_reg.tmo_low_adj_bc);
	pr_info("tmo_high_en = %d\n", tmo_reg.tmo_high_en);
	pr_info("tmo_olut_bit_mode = %d\n", tmo_reg.tmo_olut_bit_mode);
	pr_info("tmo_high_adj1 = %d\n", tmo_reg.tmo_high_adj1);
	pr_info("tmo_high_adj2 = %d\n", tmo_reg.tmo_high_adj2);
	pr_info("tmo_high_maxdiff = %d\n", tmo_reg.tmo_high_maxdiff);
	pr_info("tmo_high_mindiff = %d\n", tmo_reg.tmo_high_mindiff);
	pr_info("tmo_curve_smo_mode = %d\n", tmo_reg.tmo_curve_smo_mode);
	pr_info("tmo_soft_clip_th = %d\n", tmo_reg.tmo_soft_clip_th);
	pr_info("tmo_r0_32 = %d\n", tmo_reg.tmo_r0_32);
	pr_info("tmo_r1_32 = %d\n", tmo_reg.tmo_r1_32);
	pr_info("tmo_y_highlight_ratio = %d\n", tmo_reg.tmo_y_highlight_ratio);
	pr_info("tmo_ratio_sat = %d\n", tmo_reg.tmo_ratio_sat);
	pr_info("tmo_avg_lum_ratio = %d\n", tmo_reg.tmo_avg_lum_ratio);
	pr_info("tmo_alpha = %d\n", tmo_reg.tmo_alpha);
	pr_info("tmo_model = %d\n", tmo_reg.tmo_model);
	pr_info("tmo_highlight_lut_gain = %d\n", tmo_reg.tmo_highlight_lut_gain);
	pr_info("tmo_highlight_lut_clip = %d\n", tmo_reg.tmo_highlight_lut_clip);
	pr_info("tmo_force_xy_point_en = %d\n", tmo_reg.tmo_force_xy_point_en);
	pr_info("tmo_highlight_lut_x = %d\n", tmo_reg.tmo_highlight_lut_x);
	pr_info("tmo_highlight_lut_y = %d\n", tmo_reg.tmo_highlight_lut_y);
	pr_info("tmo_highlight_lut_pst_gain_en = %d\n", tmo_reg.tmo_highlight_lut_pst_gain_en);
	pr_info("tmo_highlight_lut_pst_gain = %d\n", tmo_reg.tmo_highlight_lut_pst_gain);
	pr_info("tmo_highlight_y_clip_pos_low = %d\n", tmo_reg.tmo_highlight_y_clip_pos_low);
	pr_info("tmo_highlight_y_clip_pos_high = %d\n", tmo_reg.tmo_highlight_y_clip_pos_high);
	pr_info("tmo_highlight_y_clip_pos_sup_high = %d\n",
		tmo_reg.tmo_highlight_y_clip_pos_sup_high);
	pr_info("tmo_highlight_y_clip_en = %d\n", tmo_reg.tmo_highlight_y_clip_en);
	pr_info("tmo_fcurv_low_th = %d\n", tmo_reg.tmo_fcurv_low_th);
	pr_info("tmo_fcurv_mid1_th = %d\n", tmo_reg.tmo_fcurv_mid1_th);
	pr_info("tmo_fcurv_mid2_th = %d\n", tmo_reg.tmo_fcurv_mid2_th);
	pr_info("tmo_fcurv_high1_th = %d\n", tmo_reg.tmo_fcurv_high1_th);
	pr_info("tmo_fcurv_high2_th = %d\n", tmo_reg.tmo_fcurv_high2_th);
	pr_info("tmo_fcurv_high3_th = %d\n", tmo_reg.tmo_fcurv_high3_th);
	pr_info("tmo_avg_lum_th0 = %d\n", tmo_reg.tmo_avg_lum_th0);
	pr_info("tmo_avg_lum_th1 = %d\n", tmo_reg.tmo_avg_lum_th1);
	pr_info("tmo_avg_lum_th2 = %d\n", tmo_reg.tmo_avg_lum_th2);
	pr_info("tmo_avg_lum_alpha0 = %d\n", tmo_reg.tmo_avg_lum_alpha0);
	pr_info("tmo_avg_lum_alpha1 = %d\n", tmo_reg.tmo_avg_lum_alpha1);
	pr_info("tmo_avg_lum_alpha2 = %d\n", tmo_reg.tmo_avg_lum_alpha2);
	pr_info("tmo_avg_lum_alpha3 = %d\n", tmo_reg.tmo_avg_lum_alpha3);
	pr_info("tmo_fcurve_adj_en = %d\n", tmo_reg.tmo_fcurve_adj_en);
	pr_info("tmo_fcurve_adj_sync_idx = %d %d %d %d %d %d %d %d %d %d\n",
		tmo_reg.tmo_fcurve_adj_sync_idx[0], tmo_reg.tmo_fcurve_adj_sync_idx[1],
		tmo_reg.tmo_fcurve_adj_sync_idx[2], tmo_reg.tmo_fcurve_adj_sync_idx[3],
		tmo_reg.tmo_fcurve_adj_sync_idx[4], tmo_reg.tmo_fcurve_adj_sync_idx[5],
		tmo_reg.tmo_fcurve_adj_sync_idx[6], tmo_reg.tmo_fcurve_adj_sync_idx[7],
		tmo_reg.tmo_fcurve_adj_sync_idx[8], tmo_reg.tmo_fcurve_adj_sync_idx[9]);
	pr_info("tmo_fcurve_adj_gain = %d %d %d %d %d %d %d %d %d %d\n",
		tmo_reg.tmo_fcurve_adj_gain[0], tmo_reg.tmo_fcurve_adj_gain[1],
		tmo_reg.tmo_fcurve_adj_gain[2], tmo_reg.tmo_fcurve_adj_gain[3],
		tmo_reg.tmo_fcurve_adj_gain[4], tmo_reg.tmo_fcurve_adj_gain[5],
		tmo_reg.tmo_fcurve_adj_gain[6], tmo_reg.tmo_fcurve_adj_gain[7],
		tmo_reg.tmo_fcurve_adj_gain[8], tmo_reg.tmo_fcurve_adj_gain[9]);
	pr_info("tmo_avg_lum_alpha_sc_flag = %d\n", tmo_reg.tmo_avg_lum_alpha_sc_flag);
	pr_info("tmo_sc_hist_th = %d\n", tmo_reg.tmo_sc_hist_th);
	pr_info("tmo_sc_maxl_th = %d\n", tmo_reg.tmo_sc_maxl_th);
	for (i = 0; i < 10; i++)
		pr_info("tmo_fcurv_clip_val[%d][0~3] = %d %d %d %d\n", i,
			tmo_reg.tmo_fcurv_clip_val[i][0], tmo_reg.tmo_fcurv_clip_val[i][1],
			tmo_reg.tmo_fcurv_clip_val[i][2], tmo_reg.tmo_fcurv_clip_val[i][3]);
	pr_info("tmo_single_bin_th = %d\n", tmo_reg.tmo_single_bin_th);
	pr_info("tmo_full_black_th = %d\n", tmo_reg.tmo_full_black_th);
	pr_info("tmo_full_white_th0 = %d\n", tmo_reg.tmo_full_white_th0);
	pr_info("tmo_full_white_th1 = %d\n", tmo_reg.tmo_full_white_th1);
	pr_info("tmo_special_pat1_th = %d\n", tmo_reg.tmo_special_pat1_th);
	pr_info("tmo_special_pat2_th = %d\n", tmo_reg.tmo_special_pat2_th);
	pr_info("tmo_oo_init_lut = %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		tmo_reg.tmo_oo_init_lut[0], tmo_reg.tmo_oo_init_lut[1],
		tmo_reg.tmo_oo_init_lut[2], tmo_reg.tmo_oo_init_lut[3],
		tmo_reg.tmo_oo_init_lut[4], tmo_reg.tmo_oo_init_lut[5],
		tmo_reg.tmo_oo_init_lut[6], tmo_reg.tmo_oo_init_lut[7],
		tmo_reg.tmo_oo_init_lut[8], tmo_reg.tmo_oo_init_lut[9],
		tmo_reg.tmo_oo_init_lut[10], tmo_reg.tmo_oo_init_lut[11],
		tmo_reg.tmo_oo_init_lut[12]);
	pr_info("tmo_blend_dark_gain = %d\n", tmo_reg.tmo_blend_dark_gain);
	pr_info("tmo_blend_dark_th0 = %d\n", tmo_reg.tmo_blend_dark_th0);
	pr_info("tmo_blend_dark_th1 = %d\n", tmo_reg.tmo_blend_dark_th1);

	pr_info("w = %d\n", tmo_reg.w);
	pr_info("h = %d\n", tmo_reg.h);
	pr_info("hdr_ogain_shift = %d\n", tmo_reg.hdr_ogain_shift);
	pr_info("hdr_hist_sel = %d\n", tmo_reg.hdr_hist_sel);
}

int hdr10_tmo_dbg(char **parm)
{
	long val = 0;
	int idx = 0;
	int idx1 = 0;
	int tmp = 0;

	if (!strcmp(parm[0], "tmo_en"))  {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		if (tmo_reg.tmo_en != (int)val)
			change_param((int)val);
		tmo_reg.tmo_en = (int)val;
		pr_tmo_dbg("tmo_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "display_e")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_display_e = (int)val;
		pr_tmo_dbg("tmo_display_e = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "display_e_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_display_e_bc = (int)val;
		pr_tmo_dbg("tmo_display_e_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_max_th1 = (int)val;
		pr_tmo_dbg("tmo_max_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_max_th2 = (int)val;
		pr_tmo_dbg("tmo_max_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th3")) {
		if (kstrtoint(parm[1], 10, &tmp) < 0)
			goto error;
		tmo_reg.tmo_max_th3 = tmp;
		pr_tmo_dbg("tmo_max_th3 = %d\n", tmp);
		pr_info("\n");
	} else if (!strcmp(parm[0], "max_th4")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_max_th4 = (int)val;
		pr_tmo_dbg("tmo_max_th4 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight = (int)val;
		pr_tmo_dbg("tmo_highlight = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_bc = (int)val;
		pr_tmo_dbg("tmo_highlight_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "ratio")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_ratio = (int)val;
		pr_tmo_dbg("tmo_ratio = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "light_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_light_th = (int)val;
		pr_tmo_dbg("tmo_light_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hist_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hist_th = (int)val;
		pr_tmo_dbg("tmo_hist_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_th1 = (int)val;
		pr_tmo_dbg("tmo_highlight_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_th2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_th2 = (int)val;
		pr_tmo_dbg("tmo_highlight_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "display_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_display_adj = (int)val;
		pr_tmo_dbg("tmo_display_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_th = (int)val;
		pr_tmo_dbg("tmo_avg_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_adj = (int)val;
		pr_tmo_dbg("tmo_avg_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl0 = (int)val;
		pr_tmo_dbg("tmo_hl0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl1 = (int)val;
		pr_tmo_dbg("tmo_hl1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl2 = (int)val;
		pr_tmo_dbg("tmo_hl2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl3 = (int)val;
		pr_tmo_dbg("tmo_hl3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl0_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl0_bc = (int)val;
		pr_tmo_dbg("tmo_hl0_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl1_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl1_bc = (int)val;
		pr_tmo_dbg("tmo_hl1_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl2_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl2_bc = (int)val;
		pr_tmo_dbg("tmo_hl2_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "hl3_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_hl3_bc = (int)val;
		pr_tmo_dbg("tmo_hl3_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "pnum_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_pnum_th = (int)val;
		pr_tmo_dbg("tmo_pnum_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_thold1 = (int)val;
		pr_tmo_dbg("tmo_thold1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_thold2 = (int)val;
		pr_tmo_dbg("tmo_thold2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_thold3 = (int)val;
		pr_tmo_dbg("tmo_thold3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "thold4")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_thold4 = (int)val;
		pr_tmo_dbg("tmo_thold4 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_th = (int)val;
		pr_tmo_dbg("tmo_middle_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_a")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_a = (int)val;
		pr_tmo_dbg("tmo_middle_a = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_a_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_a_bc = (int)val;
		pr_tmo_dbg("tmo_middle_a_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_a_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_a_adj = (int)val;
		pr_tmo_dbg("tmo_middle_a_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_b")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_b = (int)val;
		pr_tmo_dbg("tmo_middle_b = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "middle_s")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_middle_s = (int)val;
		pr_tmo_dbg("tmo_middle_s = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "low_adj")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_low_adj = (int)val;
		pr_tmo_dbg("tmo_low_adj = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "low_adj_bc")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_low_adj_bc = (int)val;
		pr_tmo_dbg("tmo_low_adj_bc = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_high_en = (int)val;
		pr_tmo_dbg("tmo_high_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "olut_bit_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_olut_bit_mode = (int)val;
		pr_tmo_dbg("tmo_olut_bit_mode = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_adj1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_high_adj1 = (int)val;
		pr_tmo_dbg("tmo_high_adj1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_adj2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_high_adj2 = (int)val;
		pr_tmo_dbg("tmo_high_adj2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_maxdiff")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_high_maxdiff = (int)val;
		pr_tmo_dbg("tmo_high_maxdiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "high_mindiff")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_high_mindiff = (int)val;
		pr_tmo_dbg("tmo_high_mindiff = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "curve_smo_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_curve_smo_mode = (int)val;
		pr_tmo_dbg("tmo_curve_smo_mode = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "soft_clip_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_soft_clip_th = (int)val;
		pr_tmo_dbg("tmo_soft_clip_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "r0_32")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_r0_32 = (int)val;
		pr_tmo_dbg("tmo_r0_32 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "r1_32")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_r1_32 = (int)val;
		pr_tmo_dbg("tmo_r1_32 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "y_highlight_ratio")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_y_highlight_ratio = (int)val;
		pr_tmo_dbg("tmo_y_highlight_ratio = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "ratio_sat")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_ratio_sat = (int)val;
		pr_tmo_dbg("tmo_ratio_sat = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_ratio")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_ratio = (int)val;
		pr_tmo_dbg("tmo_avg_lum_ratio = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "alpha")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_alpha = (int)val;
		pr_tmo_dbg("tmo_alpha = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "model")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_model = (int)val;
		pr_tmo_dbg("tmo_model = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_gain")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_gain = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_gain = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_clip")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_clip = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_clip = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "force_xy_point_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_force_xy_point_en = (int)val;
		pr_tmo_dbg("tmo_force_xy_point_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_x")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_x = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_x = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_y")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_y = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_y = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_pst_gain_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_pst_gain_en = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_pst_gain_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_lut_pst_gain")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_lut_pst_gain = (int)val;
		pr_tmo_dbg("tmo_highlight_lut_pst_gain = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_y_clip_pos_low")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_y_clip_pos_low = (int)val;
		pr_tmo_dbg("tmo_highlight_y_clip_pos_low = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_y_clip_pos_high")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_y_clip_pos_high = (int)val;
		pr_tmo_dbg("tmo_highlight_y_clip_pos_high = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_y_clip_pos_sup_high")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_y_clip_pos_sup_high = (int)val;
		pr_tmo_dbg("tmo_highlight_y_clip_pos_sup_high = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "highlight_y_clip_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_highlight_y_clip_en = (int)val;
		pr_tmo_dbg("tmo_highlight_y_clip_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_low_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_low_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_low_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_mid1_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_mid1_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_mid1_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_mid2_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_mid2_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_mid2_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_high1_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_high1_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_high1_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_high2_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_high2_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_high2_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_high3_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurv_high3_th = (int)val;
		pr_tmo_dbg("tmo_fcurv_high3_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_th0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_th0 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_th0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_th1 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_th2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_th2 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_th2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_alpha0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_alpha0 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_alpha0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_alpha1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_alpha1 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_alpha1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_alpha2")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_alpha2 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_alpha2 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_alpha3")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_alpha3 = (int)val;
		pr_tmo_dbg("tmo_avg_lum_alpha3 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurve_adj_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_fcurve_adj_en = (int)val;
		pr_tmo_dbg("tmo_fcurve_adj_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "avg_lum_alpha_sc_flag")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_avg_lum_alpha_sc_flag = (int)val;
		pr_tmo_dbg("tmo_avg_lum_alpha_sc_flag = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "sc_hist_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_sc_hist_th = (int)val;
		pr_tmo_dbg("tmo_sc_hist_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "sc_maxl_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_sc_maxl_th = (int)val;
		pr_tmo_dbg("tmo_sc_maxl_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "single_bin_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_single_bin_th = (int)val;
		pr_tmo_dbg("tmo_single_bin_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "full_black_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_full_black_th = (int)val;
		pr_tmo_dbg("tmo_full_black_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "full_white_th0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_full_white_th0 = (int)val;
		pr_tmo_dbg("tmo_full_white_th0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "full_white_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_full_white_th1 = (int)val;
		pr_tmo_dbg("tmo_full_white_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "special_pat1_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_special_pat1_th = (int)val;
		pr_tmo_dbg("tmo_special_pat1_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "special_pat2_th")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_special_pat2_th = (int)val;
		pr_tmo_dbg("tmo_special_pat2_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "blend_dark_gain")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_blend_dark_gain = (int)val;
		pr_tmo_dbg("tmo_blend_dark_gain = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "blend_dark_th0")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_blend_dark_th0 = (int)val;
		pr_tmo_dbg("tmo_blend_dark_th0 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "blend_dark_th1")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		tmo_reg.tmo_blend_dark_th1 = (int)val;
		pr_tmo_dbg("tmo_blend_dark_th1 = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurve_adj_sync_idx")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (idx > 9)
			goto error;
		tmo_reg.tmo_fcurve_adj_sync_idx[idx] = (int)val;
		pr_tmo_dbg("tmo_fcurve_adj_sync_idx[%d] = %d\n", idx, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurve_adj_gain")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (idx > 9)
			goto error;
		tmo_reg.tmo_fcurve_adj_gain[idx] = (int)val;
		pr_tmo_dbg("tmo_fcurve_adj_gain[%d] = %d\n", idx, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "fcurv_clip_val")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		idx1 = (int)val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto error;
		if (idx > 9)
			goto error;
		if (idx1 > 3)
			goto error;
		tmo_reg.tmo_fcurv_clip_val[idx][idx1] = (int)val;
		pr_tmo_dbg("tmo_fcurve_adj_gain[%d][%d] = %d\n", idx, idx1, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "oo_init_lut")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (idx > 12)
			goto error;
		tmo_reg.tmo_oo_init_lut[idx] = (int)val;
		pr_tmo_dbg("tmo_oo_init_lut[%d] = %d\n", idx, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "read_param")) {
		hdr10_tmo_parm_show();
	}

	return 0;

error:
	return -1;
}

int hdr_tmo_alg_dbg(char **parm)
{
	int ret = 0;

	if (tmo_reg.fw_tmo_dbg)
		ret = tmo_reg.fw_tmo_dbg(parm);
	else
		pr_info("%s: No hdr ko, Null Func\n", __func__);

	return ret;
}

int hdr_tmo_alg_dbg_show(char *str)
{
	int len = 0;

	if (tmo_reg.fw_tmo_dbg_show)
		len = tmo_reg.fw_tmo_dbg_show(str);
	else
		pr_info("%s: No hdr ko, Null Func\n", __func__);

	return len;
}

#endif
