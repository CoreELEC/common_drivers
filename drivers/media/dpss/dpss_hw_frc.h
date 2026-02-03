/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_HW_FRC_H__
#define __DPSS_HW_FRC_H__

void frc_me_cut(struct PRM_DPSS_TOP *prm_top);
void hw_cfg_dpss_mc_ini(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr);
void hw_cfg_dpss_mc_intf(struct PRM_DPSS_TOP *prm_top, struct DPSS_MC0_TYPE *prm_mc);
void hw_cfg_dpss_mc0(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr, struct DPSS_MC0_TYPE *prm_mc);
void hw_cfg_dpss_mc_pre_triggle(void);
void hw_cfg_dpss_mc_triggle(void);
void hw_cfg_dpss_mc_slice(u32 frm_hsize, u32 frm_vsize, u32 slice_num);
void hw_cfg_dpss_mc_bbd(u32 frm_hsize, u32 frm_vsize, u32 bbd_en, u32 bbd_sel,
			u32 src_hsize, u32 src_vsize);
bool hw_dpss_dpe_info_cfg(struct PRM_DPSS_TOP *prm_top, bool obuf_rdy);
void hw_dpss_dpe_info_cfg_sw(u32 *base_addr, u32 *base_oft, u32 buf_num, u32 out_addr_en,
			struct PRM_DPSS_TOP *prm_top, u32 dae_frm_idx, u32 vdin_frm_idx);
void hw_mc_vfcd_cfg_update(void);
void dpss_dpe_dbg_cfg(void);
void hw_afbcd_420_mc_bypass_cfg(struct PRM_DPSS_TOP *prm_top);
void hw_config_dae_loop_tab(enum DPSS_FRC_RATIO frc_ratio, enum DPSS_FILM_MODE film_mode,
			 u32 cfg_at_inp_site);
void frc_mc_cut_0(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr);
void frc_mc_cut_1(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr);
void hw_update_display_info(struct display_buffer_info_s *display_buf_info);
void hw_mc_reverse_update(struct PRM_DPSS_TOP *prm_top, struct DPSS_MC0_TYPE *prm_mc);

#endif
