/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_S_FRC_H__
#define __DPSS_S_FRC_H__

#include <linux/amlogic/media/dpss/dpss_frc.h>

#define FRC_ME_FUNC     0x10
#define FRC_VP_FUNC     0x11
#define FRC_BBD_FUNC    0x12
#define FRC_IPLG_FUNC   0x13
#define FRC_MELG_FUNC   0x14

#define DAE0_FREEZE_CNT    2

// #define DPSS_FRC_FW_VER			"20250816 dpss_frc_driver update release"
// #define DPSS_FRC_KERDRV_VER		1000
void dpss_h_update_frc(struct vframe_s *vfm);
void init_frc_pre_light(void);
void init_frc_pre(struct dpss_sub_vf_s *vfs, struct dpss_ch_s *pch);
void init_frc_post(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs);
void frc_only_int(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs, struct vframe_s *vf);
bool frc_only_input_buf(struct dpss_ch_s *pch, struct dpss_frc_i_s *frc_i);
void dpss_inp_reg_monitor(void);
void dpss_dae0_reg_monitor(void);
void dpss_pre_vs_reg_monitor(void);
void dpss_vsync_reg_monitor(void);
void frc_unreg_hw(u32 ch);
void frc_unreg_clear_state(u32 ch);
void frc_reg_update_state(u32 ch);
void dpss_enable_mc_link(bool enable);

void check_dpss_frc_status(void);
void frc_undone_check(void);
void frc_set_mc_bypass(u8 bypass);
void frc_set_mc_bypass_rdma(u8 bypass);
void dpss_frc_check_reg_stats(void);
void dpss_frc_check_undone_stats(void);

void dpss_frc_get_init_csc(void);
void dpss_frc_set_mc_pattern(u8 enpat);
void dpss_frc_pattern_dbg_ctrl(void);
void set_delta_ms(int in_fps, int out_fps, int delta);
void dump_delta_ms(void);
void dpss_set_mc_link_state(u8 en);
void dpss_frc_set_memc_enable(u8 function, u8 enable);
void update_n2m_info_to_fw(enum DPSS_FRC_RATIO frc_ratio);
void frc_dbg_read_table_value(void);
void frc_dbg_table_print(struct work_struct *work);
void get_n2m_value_from_frc_ratio(enum DPSS_FRC_RATIO frc_ratio, u32 *inp_n, u32 *outp_m);
bool is_vd1_link_state(void);
void dpss_frc_prob(struct dpss_dev_s *devp);
void dpss_frc_remove(void);
int dpss_frc_tell_alg_vendor(u8 vendor_info);
void dpss_frc_get_memc_gmv(struct memc_gmv_s *memc_gmv);
void dpss_frc_resume(void);
int dpss_frc_fpp_memc_set_level(u8 level, u8 num);
void update_frc_state(void);
struct vframe_s *frc_get_vfm(void);
void mc_res_switch_begin(void);

//crop
void detect_axis_change(struct pvpp_dis_frc_para_in_s *in_para);
void detect_crop_change(struct frc_win_s *vpp_win, struct vframe_s *vfm);
void update_frc_screen_vinfo(struct pvpp_dis_frc_para_in_s *in_para);
void dpss_frc_sync_status(u8 working);

// frc game
void dpss_game_reinit_reg_tab(void);

//fw
void update_n2m_info_to_fw(enum DPSS_FRC_RATIO frc_ratio);
void update_mc_cut_size_to_fw(u16 mc_hsize, u16 mc_vsize,
			u16 crop_top, u16 crop_left, u16 crop_bottom, u16 crop_right);

#endif
