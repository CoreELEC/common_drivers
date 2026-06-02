/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_D_PQ_MGR_H__
#define __DPSS_D_PQ_MGR_H__
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/amlogic/iomap.h>

#define DI_PAGE_REG_COUNT_MAX (256)
#define DI_PAGE_TBL_COUNT_MAX (1)/*(100)*/
#define DI_MASK_COUNT_MAX (256)

enum DI_DBLK_PARAM_E {
	REG_DELTA_OUT_ALPHA,
	REG_BLK_H_STRENGTH,
	REG_BLK_V_STRENGTH,
	REG_DBLK_V_DELTA_GAIN_1,
	REG_DBLK_V_DELTA_GAIN_0,
	REG_DBLK_V_DELTA_MAX_1,
	REG_DBLK_V_DELTA_MAX_0,
	REG_DBLK_H_DELTA_GAIN_1,
	REG_DBLK_H_DELTA_GAIN_0,
	REG_DBLK_H_DELTA_MAX_1,
	REG_DBLK_H_DELTA_MAX_0,
	REG_DBLK_MAX,
};

enum DI_DEMOSQUITO_PARAM_E {
	REG_DM_EDGE_BLEND_RATE,
	REG_DM_EDGE_LEVEL_GAIN_15,
	REG_DM_EDGE_LEVEL_GAIN_14,
	REG_DM_EDGE_LEVEL_GAIN_13,
	REG_DM_EDGE_LEVEL_GAIN_12,
	REG_DM_EDGE_LEVEL_GAIN_11,
	REG_DM_EDGE_LEVEL_GAIN_10,
	REG_DM_EDGE_LEVEL_GAIN_9,
	REG_DM_EDGE_LEVEL_GAIN_8,
	REG_DM_EDGE_LEVEL_GAIN_7,
	REG_DM_EDGE_LEVEL_GAIN_6,
	REG_DM_EDGE_LEVEL_GAIN_5,
	REG_DM_EDGE_LEVEL_GAIN_4,
	REG_DM_EDGE_LEVEL_GAIN_3,
	REG_DM_EDGE_LEVEL_GAIN_2,
	REG_DM_EDGE_LEVEL_GAIN_1,
	REG_DM_EDGE_LEVEL_GAIN_0,
	REG_DM_STRENGTH_VS_MOS_LEVEL_7,
	REG_DM_STRENGTH_VS_MOS_LEVEL_6,
	REG_DM_STRENGTH_VS_MOS_LEVEL_5,
	REG_DM_STRENGTH_VS_MOS_LEVEL_4,
	REG_DM_STRENGTH_VS_MOS_LEVEL_3,
	REG_DM_STRENGTH_VS_MOS_LEVEL_2,
	REG_DM_STRENGTH_VS_MOS_LEVEL_1,
	REG_DM_STRENGTH_VS_MOS_LEVEL_0,
	REG_DM_STRENGTH_CHROMA_GAIN,
	REG_DM_ME_SUM_SAD_THD,
	REG_DM_ME_AVG_H_SAD_THD,
	REG_DM_FLAT_THD2,
	REG_DM_FLAT_THD1,
	REG_DM_FLAT_THD0,
	REG_DM_EDGE_TH,
	REG_DM_FLAT_MAX_SAD_GAIN1,
	REG_DM_FLAT_MAX_SAD_GAIN0,
	REG_DM_FLAT_MAX_SAD_THN,
	REG_DM_FLAT_MAX_SAD_TH0,
	REG_DM_EDGE_MIN_SAD_GAIN1,
	REG_DM_EDGE_MIN_SAD_GAIN0,
	REG_DM_EDGE_MIN_SAD_THN,
	REG_DM_EDGE_MIN_SAD_TH0,
	REG_DM_SGM_LEVEL_MIN_THD,
	REG_DM_SGM_LEVEL_MAX_THD2,
	REG_DM_SGM_LEVEL_MAX_THD,
	REG_DM_SGM_LEVEL_EDGE_OFST,
	REG_DM_MAX,//DPSS_DB_DM_NUM need update
};

enum DI_DNR_PARAM_E {
	REG_SNR_LPF_ALPHA_CURV2,
	REG_SNR_LPF_ALPHA_CURV1,
	REG_SNR_LPF_ALPHA_CURV0,
	REG_SNR_LPF_CUR_NOISE_GAIN,
	REG_SNR_LPF_CUR_NOISE_OFST,
	REG_SNR_LPF_PRE_NOISE_GAIN,
	REG_SNR_LPF_PRE_NOISE_OFST,
	REG_SNR_LPF_IIR_NOISE_GAIN,
	REG_SNR_LPF_IIR_NOISE_OFST,
	REG_TNR_NE_FINAL_GAIN,
	REG_TNR_NE_FINAL_OFST,
	REG_TNR_NE_FORCE_Z_SAD_OFST_3,
	REG_TNR_NE_FORCE_Z_SAD_OFST_2,
	REG_TNR_NE_FORCE_Z_SAD_OFST_1,
	REG_TNR_NE_FORCE_Z_SAD_OFST_0,
	REG_TNR_NE_FORCE_Z_MIN_NOISE_LEVEL,
	REG_TNR_MV_VALID_ALPHA_LUT_7,
	REG_TNR_MV_VALID_ALPHA_LUT_6,
	REG_TNR_MV_VALID_ALPHA_LUT_5,
	REG_TNR_MV_VALID_ALPHA_LUT_4,
	REG_TNR_MV_VALID_ALPHA_LUT_3,
	REG_TNR_MV_VALID_ALPHA_LUT_2,
	REG_TNR_MV_VALID_ALPHA_LUT_1,
	REG_TNR_MV_VALID_ALPHA_LUT_0,
	REG_TNR_IIR_GAIN_VSLUMA_7,
	REG_TNR_IIR_GAIN_VSLUMA_6,
	REG_TNR_IIR_GAIN_VSLUMA_5,
	REG_TNR_IIR_GAIN_VSLUMA_4,
	REG_TNR_IIR_GAIN_VSLUMA_3,
	REG_TNR_IIR_GAIN_VSLUMA_2,
	REG_TNR_IIR_GAIN_VSLUMA_1,
	REG_TNR_IIR_GAIN_VSLUMA_0,
	REG_TNR_MV_VALID_CHECK_EN,
	REG_TNR_MV_VALID_EDGE_CHK_EN,
	REG_SNR_LPF_STRENGTH_VSLUMA_7,
	REG_SNR_LPF_STRENGTH_VSLUMA_6,
	REG_SNR_LPF_STRENGTH_VSLUMA_5,
	REG_SNR_LPF_STRENGTH_VSLUMA_4,
	REG_SNR_LPF_STRENGTH_VSLUMA_3,
	REG_SNR_LPF_STRENGTH_VSLUMA_2,
	REG_SNR_LPF_STRENGTH_VSLUMA_1,
	REG_SNR_LPF_STRENGTH_VSLUMA_0,
	REG_DNR_MAX,
};

enum DI_DCT_PARAM_E {
	REG_DECONTOUR_ENABLE_0,
	REG_DECONTOUR_ENABLE_1,
	REG_DIRMAP_ENABLE,
	REG_DIR_SMGRD_EN,
	REG_DIR_NOISE_TH,
	REG_DIR_GAIN,
	REG_DIR_WIN,
	REG_DIR_MAG,
	REG_AVG_COR_TH,
	REG_BIG_COR_TH,
	REG_FLT_COR_EN,
	REG_MAP_BLD_MODE,
	REG_MAP_BLD_ALPHA,
	REG_BLD1_MODE,
	REG_PMAP_MANUAL_ALP,
	REG_PMAP_DETAIL_ENABLE,
	REG_PMAP_LUMA_SCL_ENABLE,
	REG_PMAP_COLORPRT_ENABLE,
	REG_PMAP_MANUAL_ENABLE,
	REG_PMAP_LUMA_SCL_SEL,
	REG_PMAP_COLORPROTECT_SEL,
	REG_PMAP_LUMA_GAIN,
	REG_PMAP_LUMA_MAG,
	REG_PMAP_COLORPROTECT_GAIN,
	REG_PMAP_COLORPROTECT_MAG,
	REG_PMAP_LUMA_SCL_LUT_0,
	REG_PMAP_LUMA_SCL_LUT_1,
	REG_PMAP_LUMA_SCL_LUT_2,
	REG_PMAP_LUMA_SCL_LUT_3,
	REG_PMAP_LUMA_SCL_LUT_4,
	REG_PMAP_LUMA_SCL_LUT_5,
	REG_PMAP_LUMA_SCL_LUT_6,
	REG_PMAP_LUMA_SCL_LUT_7,
	REG_PMAP_LUMA_SCL_LUT_8,
	REG_DCT_MAX,
};

enum DI_XLR_PARAM_E {
	REG_XLR_EN,
	REG_XLR_SIDE_EN,
	REG_XLR_OOPLP_GAIN,
	REG_XLR_DISLP_THRD,
	REG_XLR_SIMLP_GAIN,
	REG_XLR_TXT_CORE,
	REG_XLR_ERR_THRD_2,
	REG_XLR_ERR_THRD_1,
	REG_XLR_ERR_THRD_0,
	REG_XLR_HCT_THR,
	REG_XLR_SAT_THR,
	REG_XLR_HMARGIN,
	REG_XLR_HCT_STEP,
	REG_XLR_HCT_LPF,
	REG_XLR_HPF_ONLY,
	REG_XLR_MAX,
};

struct pq_demosquito_param_s {
	unsigned int reg_dm_edge_blend_rate;
	unsigned int reg_dm_edge_level_gain_15;
	unsigned int reg_dm_edge_level_gain_14;
	unsigned int reg_dm_edge_level_gain_13;
	unsigned int reg_dm_edge_level_gain_12;
	unsigned int reg_dm_edge_level_gain_11;
	unsigned int reg_dm_edge_level_gain_10;
	unsigned int reg_dm_edge_level_gain_9;
	unsigned int reg_dm_edge_level_gain_8;
	unsigned int reg_dm_edge_level_gain_7;
	unsigned int reg_dm_edge_level_gain_6;
	unsigned int reg_dm_edge_level_gain_5;
	unsigned int reg_dm_edge_level_gain_4;
	unsigned int reg_dm_edge_level_gain_3;
	unsigned int reg_dm_edge_level_gain_2;
	unsigned int reg_dm_edge_level_gain_1;
	unsigned int reg_dm_edge_level_gain_0;
	unsigned int reg_dm_strength_vs_mos_level_7;
	unsigned int reg_dm_strength_vs_mos_level_6;
	unsigned int reg_dm_strength_vs_mos_level_5;
	unsigned int reg_dm_strength_vs_mos_level_4;
	unsigned int reg_dm_strength_vs_mos_level_3;
	unsigned int reg_dm_strength_vs_mos_level_2;
	unsigned int reg_dm_strength_vs_mos_level_1;
	unsigned int reg_dm_strength_vs_mos_level_0;
	unsigned int reg_dm_strength_chroma_gain;
	unsigned int reg_dm_me_sum_sad_thd;
	unsigned int reg_dm_me_avg_h_sad_thd;
};

struct pq_dblk_param_s {
	unsigned int reg_delta_out_alpha;
	unsigned int reg_blk_h_strength;
	unsigned int reg_blk_v_strength;
	unsigned int reg_dblk_v_delta_gain_1;
	unsigned int reg_dblk_v_delta_gain_0;
	unsigned int reg_dblk_v_delta_max_1;
	unsigned int reg_dblk_v_delta_max_0;
	unsigned int reg_dblk_h_delta_gain_1;
	unsigned int reg_dblk_h_delta_gain_0;
	unsigned int reg_dblk_h_delta_max_1;
	unsigned int reg_dblk_h_delta_max_0;
};

struct pq_dnr_param_s {
	unsigned int reg_snr_lpf_alpha_curv2;
	unsigned int reg_snr_lpf_alpha_curv1;
	unsigned int reg_snr_lpf_alpha_curv0;
	unsigned int reg_snr_lpf_cur_noise_gain;
	unsigned int reg_snr_lpf_cur_noise_ofst;
	unsigned int reg_snr_lpf_pre_noise_gain;
	unsigned int reg_snr_lpf_pre_noise_ofst;
	unsigned int reg_snr_lpf_iir_noise_gain;
	unsigned int reg_snr_lpf_iir_noise_ofst;
	unsigned int reg_tnr_ne_final_gain;
	unsigned int reg_tnr_ne_final_ofst;
	unsigned int reg_tnr_ne_force_z_sad_ofst_3;
	unsigned int reg_tnr_ne_force_z_sad_ofst_2;
	unsigned int reg_tnr_ne_force_z_sad_ofst_1;
	unsigned int reg_tnr_ne_force_z_sad_ofst_0;
	unsigned int reg_tnr_ne_force_z_min_noise_level;
	unsigned int reg_tnr_mv_valid_alpha_lut_7;
	unsigned int reg_tnr_mv_valid_alpha_lut_6;
	unsigned int reg_tnr_mv_valid_alpha_lut_5;
	unsigned int reg_tnr_mv_valid_alpha_lut_4;
	unsigned int reg_tnr_mv_valid_alpha_lut_3;
	unsigned int reg_tnr_mv_valid_alpha_lut_2;
	unsigned int reg_tnr_mv_valid_alpha_lut_1;
	unsigned int reg_tnr_mv_valid_alpha_lut_0;
	unsigned int reg_tnr_iir_gain_vsluma_7;
	unsigned int reg_tnr_iir_gain_vsluma_6;
	unsigned int reg_tnr_iir_gain_vsluma_5;
	unsigned int reg_tnr_iir_gain_vsluma_4;
	unsigned int reg_tnr_iir_gain_vsluma_3;
	unsigned int reg_tnr_iir_gain_vsluma_2;
	unsigned int reg_tnr_iir_gain_vsluma_1;
	unsigned int reg_tnr_iir_gain_vsluma_0;
};

struct pq_dct_param_s {
	unsigned int reg_decontour_enable_0;
	unsigned int reg_decontour_enable_1;
	unsigned int reg_dirmap_enable;
	unsigned int reg_dir_smgrd_en;
	unsigned int reg_dir_noise_th;
	unsigned int reg_dir_gain;
	unsigned int reg_dir_win;
	unsigned int reg_dir_mag;
	unsigned int reg_avg_cor_th;
	unsigned int reg_big_cor_th;
	unsigned int reg_flt_cor_en;
	unsigned int reg_map_bld_mode;
	unsigned int reg_map_bld_alpha;
	unsigned int reg_bld1_mode;
	unsigned int reg_pmap_manual_alp;
	unsigned int reg_pmap_detail_enable;
	unsigned int reg_pmap_luma_scl_enable;
	unsigned int reg_pmap_colorprt_enable;
	unsigned int reg_pmap_manual_enable;
	unsigned int reg_pmap_luma_scl_sel;
	unsigned int reg_pmap_colorprotect_sel;
	unsigned int reg_pmap_luma_gain;
	unsigned int reg_pmap_luma_mag;
	unsigned int reg_pmap_colorprotect_gain;
	unsigned int reg_pmap_colorprotect_mag;
	unsigned int reg_pmap_luma_scl_lut_0;
	unsigned int reg_pmap_luma_scl_lut_1;
	unsigned int reg_pmap_luma_scl_lut_2;
	unsigned int reg_pmap_luma_scl_lut_3;
	unsigned int reg_pmap_luma_scl_lut_4;
	unsigned int reg_pmap_luma_scl_lut_5;
	unsigned int reg_pmap_luma_scl_lut_6;
	unsigned int reg_pmap_luma_scl_lut_7;
	unsigned int reg_pmap_luma_scl_lut_8;
};

struct pq_xlr_param_s {
	unsigned int reg_xlr_en;
	unsigned int reg_xlr_side_en;
	unsigned int reg_xlr_ooplp_gain;
	unsigned int reg_xlr_dislp_thrd;
	unsigned int reg_xlr_simlp_gain;
	unsigned int reg_xlr_txt_core;
	unsigned int reg_xlr_err_thrd_2;
	unsigned int reg_xlr_err_thrd_1;
	unsigned int reg_xlr_err_thrd_0;
	unsigned int reg_xlr_hct_thr;
	unsigned int reg_xlr_sat_thr;
	unsigned int reg_xlr_hmargin;
	unsigned int reg_xlr_hct_step;
	unsigned int reg_xlr_hct_lpf;
	unsigned int reg_xlr_hpf_only;
};

struct di_dblk_param_s {
	int param[REG_DBLK_MAX];
};

struct di_demosquito_param_s {
	int param[REG_DM_MAX];
};

struct di_dnr_param_s {
	int param[REG_DNR_MAX];
};

struct di_dct_param_s {
	int param[REG_DCT_MAX];
};

struct di_xlr_param_s {
	int param[REG_XLR_MAX];
};

struct di_tuning_reg_s {
	unsigned int reg_addr;
	unsigned char bit_start;
	unsigned char bit_end;
	int val;
};

struct di_tuning_page_s {
	unsigned char page_addr;
	int page_idx;
	int reg_count;
	struct di_tuning_reg_s *preg_list;
};

struct di_tuning_table_s {
	int page_count;
	struct di_tuning_page_s *ppage_list;
};

enum di_page_module_e {
	DI_PAGE_MODULE_DMS = 0,
	DI_PAGE_MODULE_DBLOCK,
	DI_PAGE_MODULE_DNR,
	DI_PAGE_MODULE_DCT,
	DI_PAGE_MODULE_XLR,
	DI_PAGE_MODULE_MAX,
};

struct di_pq_reg_s {
	unsigned int addr;      /*Register address*/
	unsigned char update;    /*Update flag*/
	unsigned int mask_type; /*Mask type*/
	int  val;       /*Register value*/
	unsigned int start_bit;  /*Table start bit*/
};

struct di_pq_page_s {
	struct di_pq_reg_s reg[DI_PAGE_REG_COUNT_MAX];
};

struct di_pq_table_s {
	unsigned char page_addr; /*Page address*/
	unsigned char count;     /*Count of total reg*/
	struct di_pq_page_s page[DI_PAGE_TBL_COUNT_MAX]; /*Table selected by top layer*/
};

#ifdef CONFIG_AMLOGIC_MEDIA_DPSS
int di_pq_mgr_demosquito_param(struct di_demosquito_param_s *pdata);
int di_pq_mgr_dblk_param(struct di_dblk_param_s *pdata);
int di_pq_mgr_dnr_param(struct di_dnr_param_s *pdata);
int di_pq_mgr_dct_param(struct di_dct_param_s *pdata);
int di_pq_mgr_xlr_param(struct di_xlr_param_s *pdata);
#endif

#endif	/*__DPSS_D_PQ_MGR_H__*/
