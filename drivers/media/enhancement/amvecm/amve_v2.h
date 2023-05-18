/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AMVE_V2_H
#define AMVE_V2_H

struct cm_port_s {
	int cm_addr_port[4];
	int cm_data_port[4];
};

int get_slice_max(void);

struct cm_port_s get_cm_port(void);
void cm_hist_get(struct vframe_s *vf,
	unsigned int hue_bin0, unsigned int sat_bin0);
void cm_hist_by_type_get(enum cm_hist_e hist_sel,
	unsigned int *data, unsigned int length,
	unsigned int addr_bin0);

void post_gainoff_set(struct tcon_rgb_ogo_s *p,
	enum wr_md_e mode);
void ve_brigtness_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_contrast_set(int val,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_color_mab_set(int mab,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
void ve_color_mcd_set(int mcd,
	enum vadj_index_e vadj_idx,
	enum wr_md_e mode);
int ve_brightness_contrast_get(enum vadj_index_e vadj_idx);

void vpp_mtx_config_v2(struct matrix_coef_s *coef,
	enum wr_md_e mode, enum vpp_slice_e slice,
	enum vpp_matrix_e mtx_sel);

/*vpp module control*/
void ve_vadj_ctl(enum wr_md_e mode, enum vadj_index_e vadj_idx, int en);
void ve_bs_ctl(enum wr_md_e mode, int en);
void ve_ble_ctl(enum wr_md_e mode, int en);
void ve_cc_ctl(enum wr_md_e mode, int en);
void post_wb_ctl(enum wr_md_e mode, int en);
void post_pre_gamma_ctl(enum wr_md_e mode, int en);
void post_pre_gamma_set(int *lut);
void vpp_luma_hist_init(void);
void get_luma_hist(struct vframe_s *vf);
void cm_top_ctl(enum wr_md_e mode, int en);

void ve_multi_slice_case_set(int enable);
int ve_multi_slice_case_get(void);

void ve_vadj_misc_set(int val,
	enum wr_md_e mode, enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int start, int len);
int ve_vadj_misc_get(enum vadj_index_e vadj_idx,
	enum vpp_slice_e slice, int start, int len);
void ve_mtrx_setting(enum vpp_matrix_e mtx_sel,
	int mtx_csc, int mtx_on, enum vpp_slice_e slice);

void ve_dnlp_set(ulong *data);
void ve_dnlp_ctl(int enable);
void ve_dnlp_sat_set(unsigned int value);

void ve_lc_stts_blk_cfg(unsigned int height,
	unsigned int width);
void ve_lc_stts_en(int enable,
	unsigned int height, unsigned int width,
	int pix_drop_mode, int eol_en, int hist_mode,
	int lpf_en, int din_sel, int bitdepth,
	int flag, int flag_full, int thd_black);
void ve_lc_blk_num_get(int *h_num, int *v_num,
	int slice);
void ve_lc_disable(void);
void ve_lc_curve_ctrl_cfg(int enable,
	unsigned int height, unsigned int width);
void ve_lc_top_cfg(int enable, int h_num, int v_num,
	unsigned int height, unsigned int width, int bitdepth,
	int flag, int flag_full);
void ve_lc_sat_lut_set(int *data);
void ve_lc_ymin_lmt_set(int *data);
void ve_lc_ymax_lmt_set(int *data);
void ve_lc_ypkbv_lmt_set(int *data);
void ve_lc_ypkbv_rat_set(int *data);
void ve_lc_ypkbv_slp_lmt_set(int *data);
void ve_lc_contrast_lmt_set(int *data);
void ve_lc_curve_set(int init_flag,
	int demo_mode, int *data, int slice);
void ve_lc_base_init(void);
void ve_lc_region_read(int blk_vnum, int blk_hnum,
	int slice, int *black_count,
	int *curve_data, int *hist_data);

void post_lut3d_ctl(enum wr_md_e mode, int en);
void post_lut3d_update(unsigned int *lut3d_data);
void post_lut3d_set(unsigned int *lut3d_data);
void post_lut3d_section_write(int index, int section_len,
	unsigned int *lut3d_data_in);
void post_lut3d_section_read(int index, int section_len,
	unsigned int *lut3d_data_out);

#endif

