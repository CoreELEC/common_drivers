/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_VE_H
#define __AM_VE_H

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/ve.h>
#include "linux/amlogic/media/amvecm/cm.h"
#include "linux/amlogic/media/amvecm/amvecm.h"

/*lut3d*/
#define LUT3D_SIZE 14739 /* 17*17*17*3 */
#define LUT3D_SINGLE_SIZE 4913 /* 17*17*17 */
#define LUT3D_POINTS 17
#define LUT3D_SIZE9 2187 /* 9*9*9*3 */
#define LUT3D_SINGLE_SIZE9 729 /* 9*9*9 */
#define LUT3D_POINTS9 9

struct ve_regs_s {
	unsigned int val:32;
	unsigned int reg:14;
	unsigned int port:2;
			/* 0    NA  NA direct access */
			/* 1    VPP_CHROMA_ADDR_PORT */
			/* VPP_CHROMA_DATA_PORT CM port registers */
			/* 2    NA NA reserved */
			/* 3    NA NA reserved */
	unsigned int bit:5;
	unsigned int wid:5;
	unsigned int mode:1;
	unsigned int rsv:5;
};

enum pst_hist_mod {
	HIST_VB = 0,
	HIST_UG,
	HIST_YR,
	HIST_MAXRGB
};

enum pst_hist_pos {
	BEFORE_POST2_MTX = 0,
	AFTER_POST2_MTX,
	POS_MAX
};

enum pw_state_e {
	PW_ON = 0,
	PW_OFF,
	PW_MAX,
};

enum vsr_pq_cfg_e {
	DEFAULT_PARAM = 0,
	RES_480P,
	RES_480I,
	RES_720P,
	RES_720I,
	RES_1080P,
	RES_1080I,
};

extern struct ve_hist_s video_ve_hist;
void ve_hist_gamma_reset(void);
extern unsigned int ve_size;
extern struct ve_dnlp_s am_ve_dnlp;
extern struct tcon_gamma_table_s video_gamma_table_r;
extern struct tcon_gamma_table_s video_gamma_table_g;
extern struct tcon_gamma_table_s video_gamma_table_b;
extern struct tcon_gamma_table_s video_gamma_table_ioctl_set;
extern struct tcon_gamma_table_s video_gamma_table_r_adj;
extern struct tcon_gamma_table_s video_gamma_table_g_adj;
extern struct tcon_gamma_table_s video_gamma_table_b_adj;
extern struct tcon_rgb_ogo_s     video_rgb_ogo;
extern struct gm_tbl_s gt;
extern unsigned int gamma_index;
extern unsigned int gamma_index_sub;
extern unsigned int gm_par_idx;
extern unsigned int *plut3d;
extern unsigned int *plut3d_base;

extern struct tcon_gamma_table_s video_gamma_table_r_sub;
extern struct tcon_gamma_table_s video_gamma_table_g_sub;
extern struct tcon_gamma_table_s video_gamma_table_b_sub;
extern struct tcon_rgb_ogo_s video_rgb_ogo_sub;

extern spinlock_t vpp_lcd_gamma_lock;
extern struct mutex vpp_lut3d_lock;
extern int lut3d_en;/*0:disable;1:enable */
extern int lut3d_order;/* 0 RGB 1 GBR */
extern int lut3d_debug;

extern u16 gamma_data_r[257];
extern u16 gamma_data_g[257];
extern u16 gamma_data_b[257];
void vpp_get_lcd_gamma_table(u32 rgb_mask);
void vpp_get_lcd_gamma_table_sub(void);

void ve_on_vs(struct vframe_s *vf, int vpp_index, struct vpp_hist_param_s *vp);
void dnlp_en_update(int vpp_index);

void ve_set_bext(struct ve_bext_s *p);
void ve_set_dnlp(struct ve_dnlp_s *p);
void ve_set_dnlp_2(void);
void ve_set_hsvs(struct ve_hsvs_s *p);
void ve_set_ccor(struct ve_ccor_s *p);
void ve_set_benh(struct ve_benh_s *p);
void ve_set_demo(struct ve_demo_s *p);
void ve_set_regs(struct ve_regs_s *p);
void ve_set_regmap(struct ve_regmap_s *p);

void ve_enable_dnlp(void);
void ve_disable_dnlp(void);

void ve_dnlp_ctrl_vsync(int enable);

int vpp_get_encl_viu_mux(void);
int vpp_get_vout_viu_mux(void);
void vpp_enable_lcd_gamma_table(int viu_sel, int rdma_write, int vpp_index);
void vpp_disable_lcd_gamma_table(int viu_sel, int rdma_write, int vpp_index);
void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask, int viu_sel);
void amve_write_gamma_table(u16 *data, u32 rgb_mask);
void amve_write_gamma_table_sub(u16 *data, u32 rgb_mask);
void vpp_set_rgb_ogo_sub(struct tcon_rgb_ogo_s *p);
void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p, int vpp_index);
void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool lock50,
			  unsigned int range_fast,
			  unsigned int range_slow);
/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESON6TVD) */
void ve_frame_size_patch(unsigned int width, unsigned int height);
/* #endif */
void ve_dnlp_latch_process(void);
void ve_lcd_gamma_process(int vpp_index);
void lvds_freq_process(void);
void ve_dnlp_param_update(void);
void ve_new_dnlp_param_update(void);
void ve_lc_curve_update(void);
void ve_lc_latch_process(void);
void ve_ogo_param_update(void);
void ve_ogo_param_update_sub(void);
void amvecm_bricon_process(signed int bri_val,
			   signed int cont_val,
			   struct vframe_s *vf, int vpp_index);
void amvecm_color_process(signed int sat_val,
			  signed int hue_val,
			  struct vframe_s *vf, int vpp_index);
void amvecm_3d_black_process(void);
void amvecm_3d_sync_process(void);
extern unsigned int vecm_latch_flag;
extern unsigned int cm_size;
extern unsigned int sync_3d_h_start;
extern unsigned int sync_3d_h_end;
extern unsigned int sync_3d_v_start;
extern unsigned int sync_3d_v_end;
extern unsigned int sync_3d_polarity;
extern unsigned int sync_3d_out_inv;
extern unsigned int sync_3d_black_color;
extern unsigned int sync_3d_sync_to_vbo;

extern int fmeter_slt;
extern int fmeter_en;
extern int cur_sr_level;
extern int pre_fmeter_level, fmeter_flag;
extern unsigned int fmeter_debug;
extern struct fmeter_data_s data_meter;
extern struct aml_fmeter_drv_param_s fmeter_drv_param;

void amve_fmeter_init(int enable);
void amve_fmetersize_config(u32 sr0_w, u32 sr0_h,
	u32 sr1_w, u32 sr1_h, int vpp_index);

/* #if defined(CONFIG_ARCH_MESON2) */
/* unsigned long long ve_get_vs_cnt(void); */
/* #endif */

#define GAMMA_SIZE 256

extern unsigned int dnlp_sel;
void ve_dnlp_load_reg(void);

/*gxlx sr adaptive setting*/
void amve_sharpness_adaptive_setting(struct vframe_s *vf,
				     unsigned int sps_h_en,
				     unsigned int sps_v_en,
				     int vpp_index);
void amve_sharpness_init(int vpp_index);
extern struct am_regs_s sr1reg_sd_scale;
extern struct am_regs_s sr1reg_hd_scale;
extern struct am_regs_s sr1reg_cvbs;
extern struct am_regs_s sr1reg_hv_noscale;
void amvecm_fresh_overscan(struct vframe_s *vf, struct vframe_s *vf_rpt);
void amvecm_reset_overscan(void);
void ve_hist_gamma_tgt(struct vframe_s *vf, struct vpp_hist_param_s *vp);
int vpp_write_lut3d_section(int index,
			    int section_len,
			    unsigned int *p3dlut_section_in);
int vpp_read_lut3d_section(int index,
			   int section_len,
			   unsigned int *p3dlut_section_out);
void vpp_lut3d_table_init(int r, int g, int b);
void vpp_lut3d_table_release(void);
int vpp_enable_lut3d(int enable);
void dump_plut3d_table(void);
void dump_plut3d_reg_table(void);

void vpp_lut3d_base_table_init(void);
void update_lut3d_base_data(unsigned int p3dlut_in[][3]);

void amvecm_gamma_init(bool en);
void set_gamma_regs(int en, int sel, int index);
void set_viu2_gamma_regs(int en, int sel);
void amvecm_wb_enable(int enable);
void amvecm_wb_enable_sub(int enable);
int vpp_pq_ctrl_config(struct pq_ctrl_s pq_cfg, enum wr_md_e md, int vpp_index);
unsigned int skip_pq_ctrl_load(struct am_reg_s *p);
void amvecm_pre_gamma_enable(int enable);
void set_pre_gamma_reg(struct pre_gamma_table_s *pre_gma_tb, int vpp_index);
void lcd_gamma_api(unsigned int index,
	u16 *r_data, u16 *g_data, u16 *b_data,
	enum wr_md_e wr_mod, enum rw_md_e rw_mod, int vpp_index);
void vpp_pst_hist_sta_config(int en,
	enum pst_hist_mod mod,
	enum pst_hist_pos pos,
	struct vinfo_s *vinfo);
void vpp_pst_hist_sta_read(unsigned int *hist);
void eye_proc(int mtx_ep[][4], int mtx_on, int vpp_index);
void set_vpp_enh_clk(struct vframe_s *vf, struct vframe_s *rpt_vf, int vpp_index);
void lut3d_update(unsigned int p3dlut_in[][3], int vpp_index);
void s7d_sharpness_init(void);
void vsr_pq_config(enum vsr_pq_cfg_e vsr_cfg, enum wr_md_e mode, int vpp_index);
void safa_pq_config(enum vsr_pq_cfg_e vsr_cfg, enum wr_md_e mode, int vpp_index);
void pi_pq_config(enum vsr_pq_cfg_e vsr_cfg, enum wr_md_e mode, int vpp_index);
void amve_sharpness_sub_ctrl(unsigned int sel, unsigned int enable);
void amve_vsr_config_update(struct vframe_s *vf, int vpp_index);

void set_sharpness_gain(int sr0_gain, int sr1_gain);
void sharpness_gain_update(int vpp_index);
void osd_sharpness_init(void);
void osd_sharpness_ctrl(unsigned int sel, unsigned int enable);
void amvecm_vadj_enable(enum vadj_index_e vadj_idx, int enable);

void init_pq_rdma_part_ins(void);

extern int amve_debug;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
extern int amve_bringup_debug;
extern int fmeter_en;
extern int fmeter_slt;
extern int dnlp_en_dsw;
extern unsigned int gamma_loadprotect_en;
#endif
extern int overscan_timing;
extern int overscan_screen_mode;
extern int overscan_disable;
extern int video_rgb_ogo_mode_sw;
extern int video_rgb_ogo_xvy_mtx;
extern int dv_pq_bypass;
extern unsigned int contrast_adj_sel;
void amve_sharpness_sub_vsync_ctrl(unsigned int enable, int vpp_index);
void amve_old_sharpness_sub_vsync_ctrl(unsigned int enable, int vpp_index);
void pre_saturation_gain_update(void);
int vpp_set_lut3d(int bfromkey,
	int keyindex,
	unsigned int p3dlut_in[][3],
	int blut3dcheck, int rdma_w);
extern struct pre_sat_data_s pre_sat_data;
int vpp_check_lut3d(void);
int lut3d_test(int test_case, int enable);

int get_skin_api(void);
void set_skin_api(int en);

void lc_evc_pq_settings(unsigned int lc_evc_src, int vpp_index);

extern int input_444_mode;
unsigned int skip_444_settings(struct am_reg_s *p);
void amve_444_config_update(struct vframe_s *vf, int vpp_index);
extern int lut3d_dma_case;
extern uint flag_lut3d_resume;
void c5_vadj_gamma_init(void);

#endif

