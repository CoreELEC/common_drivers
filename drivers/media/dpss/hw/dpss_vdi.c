// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_vdi.h"
#include "../drv/dpss_pq_drv.h"
#include <linux/amlogic/media/di/di_pq.h>

unsigned int dpss_nr_debug;
unsigned int dpss_tnr_en = 1;
unsigned int dpss_snr_en = 1;
unsigned int dpss_dm_en = 1;
unsigned int dpss_dblk_en = 1;
unsigned int dpss_cfr_en;
unsigned int dpss_cue_en;
unsigned int dpss_pq_en = 1;
unsigned int dpss_xlr_en = 1;
unsigned int dpss_xlr_side_en = 1;
unsigned int dpss_me_en = 1;
unsigned int dpss_lcevc_en;
unsigned int dpss_di_debug = 1;
unsigned int dpss_force_nr_debug;//i mode open nr debugs
unsigned int dpss_force_pq;//force enable nr debugs
static u32 diout_slc_xbgn[4];
static u32 diout_slc_hsize[4];
static u32 diout_slc_xend[4];

static u32 diout_demo_slc_xbgn[4];
static u32 diout_demo_slc_hsize[4];
static u32 diout_demo_slc_xend[4];

void nr_force_config(struct PRM_DPSS_TOP *prm_top, struct PRM_DPSS_DPE *prm_dpe)
{
	bool is_psrc	  = prm_top->src_fs_fmt.interlace == IS_PSRC;//interlace
	bool nr_src0_en = prm_dpe->nr_en & 0x1;
	bool nr_src1_en = (prm_dpe->nr_en >> 1) & 0x1;
	bool di_en = prm_dpe->di_en;
	unsigned int nr_debug_value;

	bool nr_cur_only_mode;//((rd(VPU_NR_TOP_MISC) >> 20) & 0x1);

	if (prm_top->mode_drct2)
		dpss_pq_en = 0;
	else
		dpss_pq_en = 1;
	if (dpss_force_pq)
		dpss_pq_en = dpss_force_pq - 1;
	if (dpss_pq_en)
		nr_cur_only_mode = 0;
	else
		nr_cur_only_mode = ((rd(VPU_NR_TOP_MISC) >> 20) & 0x1);
	bool reg_dblk_en_h		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dblk_en_v		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dblk_stat_en		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_xlr_en		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_xlr_side_en		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_nr_tnr_en	= (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_nr_snr_en		= is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dmsq_en		= (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_ne_en			= (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_cfr_en		= is_psrc ? 0 : (di_en);//open for I source
	bool reg_cue_en		= is_psrc ? 0 : (di_en);//open for I source
	bool reg_di_en		= di_en;
	//bool reg_ei_en		= di_en;
	//bool reg_mcdi_en	= di_en;
	bool reg_post_en	= di_en;
	bool reg_7_flag_en	= di_en; //TODO P source case open
	bool dblk_en = reg_dblk_en_h | reg_dblk_en_v |
		reg_xlr_en | reg_xlr_side_en | reg_dblk_stat_en;
	bool reg_snr_en, reg_tnr_en, reg_dm_en;

	reg_snr_en = reg_nr_snr_en;
	reg_tnr_en = reg_nr_tnr_en;
	reg_dm_en = reg_dmsq_en;

	if (dpss_force_nr_debug || dpss_nr_debug) {
		reg_di_en = 0;
		reg_post_en = 0;
	}

	if (dpss_pq_en) {
		if (dpss_dblk_en && nr_src0_en) {
			reg_dblk_en_v = 1;
			reg_dblk_en_h = 1;
		}
		if (dpss_dm_en && nr_src0_en)
			reg_dmsq_en = 1;
		if (dpss_tnr_en && nr_src0_en)
			reg_nr_tnr_en = 1;
		if (dpss_snr_en && nr_src0_en)
			reg_nr_snr_en = 1;
		if (dpss_xlr_en == 1 && nr_src0_en)
			reg_xlr_en = 1;
		if (dpss_xlr_side_en == 1 && nr_src0_en)
			reg_xlr_side_en = 1;
		if (!dpss_dblk_en) {
			reg_dblk_en_v = 0;
			reg_dblk_en_h = 0;
		}
		if (!dpss_dm_en)
			reg_dmsq_en = 0;
		if (!dpss_tnr_en)
			reg_nr_tnr_en = 0;
		if (!dpss_snr_en)
			reg_nr_snr_en = 0;
		if (!dpss_xlr_en)
			reg_xlr_en = 0;
		if (!dpss_xlr_side_en)
			reg_xlr_side_en = 0;
		if (dpss_nr_debug || dpss_force_nr_debug) {
			nr_debug_value = ((rd(DOS_DOWN_S_MODE) >> 8) & 0xff);
			if (nr_debug_value > 64 && nr_debug_value < 73) {//65/72
				reg_nr_snr_en = 0;
				reg_nr_tnr_en = 0;
				reg_dmsq_en = 0;
				reg_cue_en = 0;
				reg_ne_en = 0;
			}
		}
		if (!reg_nr_snr_en && !reg_nr_tnr_en &&
				 !reg_dmsq_en) {
			reg_snr_en = 0;
			reg_tnr_en = 0;
			reg_dm_en = 0;
		}
	}
	if (dblk_en)
		w_reg_bit(VPU_DBLK_MISC_CTRL, 1, 0, 1);//reg_dblk_inp_c44to42_en
	else
		w_reg_bit(VPU_DBLK_MISC_CTRL, 0, 0, 1);//reg_dblk_inp_c44to42_en

	if (reg_cue_en | reg_cfr_en | di_en |
		reg_dmsq_en | reg_nr_tnr_en | reg_nr_snr_en)
		w_reg_bit(VPU_NR_CORE_MISC_CTRL, ((1 & 0x1) << 2) |
		//reg_nr_pre2_inp_c44to42_en
				((1 & 0x1) << 1) | //reg_nr_pre1_inp_c44to42_en
				((1 & 0x1) << 0), 0, 3);//reg_nr_cur_inp_c44to42_en
	else
		w_reg_bit(VPU_NR_CORE_MISC_CTRL, ((1 & 0x1) << 2) |
		//reg_nr_pre2_inp_c44to42_en
			((1 & 0x1) << 1) | //reg_nr_pre1_inp_c44to42_en
			((0 & 0x1) << 0), 0, 3);//reg_nr_cur_inp_c44to42_en
	if (dpss_pq_en) {
		if (reg_nr_tnr_en == 0 && reg_nr_snr_en == 0 && reg_dmsq_en == 0 && !di_en)
			w_reg_bit(VPU_NR_CORE_MISC_CTRL, 0, 0, 3);
		else if (reg_nr_tnr_en == 0 && reg_nr_snr_en == 0 &&
				reg_dmsq_en == 0 && reg_cue_en == 0)
			w_reg_bit(VPU_NR_CORE_MISC_CTRL, 0, 0, 3);
		else if (nr_src0_en)
			w_reg_bit(VPU_NR_CORE_MISC_CTRL, 7, 0, 3);
	}
	if (dpss_pq_en) {
		if (reg_snr_en == 0 && reg_tnr_en == 0 &&
			reg_dm_en == 0 && !di_en) {
			w_reg_bit(VPU_NR_TOP_MISC, 1, 20, 1);
			w_reg_bit(DPSS_BBD_ONLY_CTRL, 1, 1, 1);
		} else {
			if (!prm_top->is_current) {
				w_reg_bit(VPU_NR_TOP_MISC, 0, 20, 1);
				w_reg_bit(DPSS_BBD_ONLY_CTRL, 0, 1, 1);
			}
		}
		if (!prm_top->is_current) {
			w_reg_bit(VPU_NR_ENABLE, reg_xlr_side_en, 2, 1);
			w_reg_bit(VPU_NR_ENABLE, reg_xlr_en, 3, 1);
		}
	}
	if (dpss_pq_en) {
		if (reg_dmsq_en == 0)
			w_reg_bit(DPSS_DPE_TOP_CTRL0, 0, 4, 1);
		else if (nr_src0_en)
			w_reg_bit(DPSS_DPE_TOP_CTRL0, 1, 4, 1);
	}

	//w_reg_bit(VPU_NR_ENABLE, reg_post_en, 0, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_dm_en, 1, 1);
	//w_reg_bit(VPU_NR_ENABLE, reg_xlr_side_en, 2, 1);
	//w_reg_bit(VPU_NR_ENABLE, reg_xlr_en, 3, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_dblk_stat_en, 4, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_dblk_en_v, 5, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_dblk_en_h, 6, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_snr_en, 7, 1);//snr enable

	//if (reg_nr_snr_en)
		//wr(VPU_MCNR_SNR_LPF_ALPHA, 0xa0804000);
	//else
		//wr(VPU_MCNR_SNR_LPF_ALPHA, 0x0);
	w_reg_bit(VPU_NR_ENABLE, reg_tnr_en, 8, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_di_en, 9, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_ne_en, 12, 1);
	if (dpss_cue_en) {
		//reg_cfr_en = dpss_cfr_en;
		reg_cue_en = dpss_cue_en - 1;
	}
	//w_reg_bit(VPU_NR_ENABLE, reg_cfr_en, 10, 1);
	w_reg_bit(VPU_NR_ENABLE, reg_cue_en, 11, 1);
		// Wr_reg_bits(DPSS_DPE_HW_DBG,0,2,1);//nr_done_mask
	if (dpss_force_nr_debug || dpss_nr_debug) {
		w_reg_bit(DPSS_DPE_HW_DBG, (!reg_di_en), 3, 1);//di_done_mask
		w_reg_bit(VPU_NR_ENABLE, reg_post_en, 0, 1);
	} else {
		w_reg_bit(DPSS_DPE_HW_DBG, (!di_en), 3, 1);//di_done_mask
	}
	//w_reg_bit(VPU_CUE_MODE_ENABLE, reg_cue_en, 4, 1);//reg_cue_enable_l
	//w_reg_bit(VPU_CUE_MODE_ENABLE, reg_cue_en, 0, 1);//reg_cue_enable_r
	//w_reg_bit(VPU_MCDI_EN, reg_mcdi_en, 0, 1); //reg_mcdi_en
	//w_reg_bit(VPU_DI_BLEND_EI_POST_EN_MODE, reg_ei_en, 2, 1);//reg_di_ei_en
	dbg_h2("%s:src:%d,%d,%d,%d,%d\n", __func__, rd(DPSS_DPE_HW_DBG),
		is_psrc, nr_src0_en, nr_src1_en, prm_top->is_current);
	dbg_h2("%s:VPU_NR_ENABLE:%d,%d,%d,%d,%d\n", __func__, reg_nr_tnr_en,
		reg_nr_snr_en, reg_dmsq_en, reg_dblk_en_v, dblk_en);
	dbg_h2("%s:VPU_NR_ENABLE:%d\n", __func__, rd(VPU_NR_ENABLE));
	if (dpss_nr_debug == 1) {
		//w_reg_bit(VPU_POST_NR_GRAD_EDGE_7, 0, 20, 1);//reg_7_flag_en
		w_reg_bit(DPSS_DPE_INTF_AFBCD0, 1, 22, 3);
		//Wr_reg_bits(DOS_DOWN_S_MODE, 0, 8, 8);
	} else {
		//w_reg_bit(VPU_POST_NR_GRAD_EDGE_7, reg_7_flag_en, 20, 1);//reg_7_flag_en
		//wr(DOS_DOWN_S_MODE, 0);
	}

	//wr(VPU_DI_DEMO_ENABLE, 0);//demo window disable

	u32 reg_nr_out_hs_en;
	u32 reg_post_in_hs_en;

	if (reg_di_en == 0 && (reg_post_en || reg_7_flag_en ||
		dpss_nr_debug || dpss_force_nr_debug)) {
		reg_nr_out_hs_en = 3;
		reg_post_in_hs_en = 1;
	} else {
		reg_nr_out_hs_en  = 1;
		reg_post_in_hs_en = 0;
	}

#ifndef DEBUG_MODE_TEST
	w_reg_bit(VPU_NR_TOP_MISC, reg_post_in_hs_en, 18, 1);
	//reg_post_in_hs_en
	w_reg_bit(VPU_NR_TOP_MISC, reg_nr_out_hs_en, 16, 2);
	//reg_nr_out_hs_en
#endif

	if (reg_dmsq_en)
		di_write_data_table(DI_PAGE_MODULE_DMS, 0);
	if (reg_dblk_en_h || reg_dblk_en_v)
		di_write_data_table(DI_PAGE_MODULE_DBLOCK, 0);
	if (reg_nr_snr_en || reg_nr_tnr_en)
		di_write_data_table(DI_PAGE_MODULE_DNR, 0);
	if (reg_xlr_en)
		di_write_data_table(DI_PAGE_MODULE_XLR, 0);
	di_db_dm[REG_DM_MAX].val = pq_update;
	di_db_dct[REG_DM_MAX].val = pq_update;

	if (dpss_di_debug || dpss_nr_debug || dpss_force_nr_debug) {
		wr(VPU_POST_NR_SLC_OVLP_SIZE, (((0x0 & 0xff) << 24) |
			((0x0 & 0xff) << 16) | ((0x0 & 0xff) << 8) |
			((0x0 & 0xff) << 0)));//need lusen check 2 or4???
		w_reg_bit(VPU_NR_ENABLE, 0, 0, 1);
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_10,
			((diout_demo_slc_xbgn[1] & 0x3fff) << 16) |
			((diout_demo_slc_xbgn[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_32,
			((diout_demo_slc_xbgn[3] & 0x3fff) << 16) |
			((diout_demo_slc_xbgn[2] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_10,
			((diout_demo_slc_hsize[1] & 0x3fff) << 16) |
			((diout_demo_slc_hsize[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_32,
			((diout_demo_slc_hsize[3] & 0x3fff) << 16) |
			((diout_demo_slc_hsize[2] & 0x3fff) << 0));
	} else {
		wr(VPU_POST_NR_SLC_OVLP_SIZE, (((0x2 & 0xff) << 24) |
			((0x2 & 0xff) << 16) | ((0x2 & 0xff) << 8) |
			((0x2 & 0xff) << 0)));//need lusen check 2 or4???
		w_reg_bit(VPU_NR_ENABLE, 1, 0, 1);
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_10, ((diout_slc_xbgn[1] & 0x3fff) << 16) |
		((diout_slc_xbgn[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_32, ((diout_slc_xbgn[3] & 0x3fff) << 16) |
			((diout_slc_xbgn[2] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_10,
			((diout_slc_hsize[1] & 0x3fff) << 16) |
			((diout_slc_hsize[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_32,
			((diout_slc_hsize[3] & 0x3fff) << 16) |
			((diout_slc_hsize[2] & 0x3fff) << 0));
	}
}

void cfg_dpss_vdi(struct PRM_DPSS_TOP *prm_top,
	struct PRM_DPSS_DPE *prm_dpe,
	struct AA_PPS_TOP_TYPE *nr_pps_cfg)
{
	u32 slc_num      = prm_top->dpe_slc_num;
	u32 me_dsx       = prm_top->dae_dsx_scale;
	u32 me_dsy       = prm_top->dae_dsy_scale;
	u32 mvx_div_mode = 1;
	u32 mvy_div_mode = 1;
	bool is_psrc      = prm_top->src_fs_fmt.interlace == IS_PSRC;//interlace
	// bool is_isrc = 1;
	//prm_top.src_fs_fmt.interlace==IS_ISRC;//interlace
	u32 hsize        = prm_top->nr_hscale_on |
		prm_top->comp_setting.vfcd_h_skip ?
		prm_dpe->dpe_nr_size.frm_hsize : prm_top->frm_hsize;
	u32 vsize = prm_top->nr_hscale_on |
		prm_top->comp_setting.vfcd_v_skip ?
		prm_dpe->dpe_nr_size.frm_vsize : prm_top->frm_vsize;
	bool nr_src0_en = prm_dpe->nr_en & 0x1;
	bool nr_src1_en = (prm_dpe->nr_en >> 1) & 0x1;
	bool di_en = prm_dpe->di_en;
	bool top_is_first = 0;
	bool reg_ei_en	= di_en;

	dbg_h2("%s nr_src0_en = %d nr_src1_en = %d\n", __func__, nr_src0_en, nr_src1_en);
	dbg_h2(" di_en = %d is_psrc = %0d\n", di_en, is_psrc);
	dbg_h2("<%s> hsize = %d vsize = %d\n", __func__, hsize, vsize);

//==============================================================//
// cfg vbe
//==============================================================//
	u32 slc_en    = slc_num > 1;
	u32 slc_hsize = hsize;///slc_num;
	u32 slc_vsize = vsize;

	w_reg_bit(VPU_VPSS_VBE_SLICE_CTRL, ((slc_num & 0xf) << 8) |
		//reg_slc_num //only for hist
		((1 & 0xf) << 4) | //reg_di_frm_dly
		((1 & 0xf) << 0), 8, 12);//reg_nr_frm_dly
		//w_reg_bit(VPU_VPSS_VBE_SLICE_CTRL,slc_en,0,1);//reg_slc_en
	w_reg_bit(VPU_VPSS_VBE_SLICE_CTRL, 1, 0, 1);//reg_slc_en
	//w_reg_bit(VPSS_VBE_HIST_WIN,slc_hsize,0,13);//reg_hist_act_hsize

	if (slc_en) {
		wr(VPSS_VBE_HIST_WIN_HSZ0, (slc_hsize << 16 | slc_hsize));
		wr(VPSS_VBE_HIST_WIN_HSZ1, (slc_hsize << 16 | slc_hsize));
		wr(VPSS_VBE_HIST_WIN_HBG0, 0);
		wr(VPSS_VBE_HIST_WIN_HBG1, 0);
		wr(VPSS_VBE_HIST_WIN_HED0, (slc_hsize - 1) << 16 | (slc_hsize - 1));
		wr(VPSS_VBE_HIST_WIN_HED1, (slc_hsize - 1) << 16 | (slc_hsize - 1));
	}
	wr(VPSS_VBE_HIST_WIN_VSZ0, (slc_vsize << 16 | slc_vsize));
	wr(VPSS_VBE_HIST_WIN_VSZ1, (slc_vsize << 16 | slc_vsize));
	wr(VPSS_VBE_HIST_WIN_VBG0, 0);
	wr(VPSS_VBE_HIST_WIN_VBG1, 0);
	wr(VPSS_VBE_HIST_WIN_VED0, (slc_vsize - 1) << 16 | (slc_vsize - 1));
	wr(VPSS_VBE_HIST_WIN_VED1, (slc_vsize - 1) << 16 | (slc_vsize - 1));
	//for hist check
	wr(VPU_NR_HIST_SIZE, (vsize << 16 | hsize));
	//add check for div mode must == 1;
	w_reg_bit(VPU_VBE_TOP_CTR0, mvx_div_mode, 12, 2);//reg_vbe_rmif_en
	w_reg_bit(VPU_VBE_TOP_CTR0, mvy_div_mode, 14, 1);//reg_vbe_rmif_en

//==============================================================//
// cfg vdi_ens
//==============================================================//
#ifdef HIS_MARK
	bool nr_cur_only_mode = ((rd(VPU_NR_TOP_MISC) >> 20) & 0x1);
	bool reg_dblk_en_h            = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dblk_en_v            = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dblk_stat_en         = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_xlr_en               = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_xlr_side_en          = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_nr_tnr_en            = (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_nr_snr_en            = is_psrc ? nr_src0_en : nr_src1_en;
	bool reg_dmsq_en              = (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_ne_en                = (nr_cur_only_mode == 1) ? 0 :
				(is_psrc ? nr_src0_en : nr_src1_en);
	bool reg_cfr_en               = is_psrc ? 0 : (di_en);//open for I source
	bool reg_cue_en               = is_psrc ? 0 : (di_en); //open for I source
	bool reg_di_en                = di_en;
	bool reg_ei_en                = di_en;
	bool reg_mcdi_en              = di_en;
	bool reg_post_en              = di_en;
	bool reg_7_flag_en            = di_en; //TODO P source case open
	bool dblk_en = reg_dblk_en_h | reg_dblk_en_v |
		reg_xlr_en | reg_xlr_side_en | reg_dblk_stat_en;

	if (dblk_en)
		w_reg_bit(VPU_DBLK_MISC_CTRL, 1, 0, 1);//reg_dblk_inp_c44to42_en
	else
		w_reg_bit(VPU_DBLK_MISC_CTRL, 0, 0, 1);//reg_dblk_inp_c44to42_en

	if (dblk_en | reg_cue_en | reg_cfr_en | di_en |
		reg_dmsq_en | reg_nr_tnr_en | reg_nr_snr_en)
		w_reg_bit(VPU_NR_CORE_MISC_CTRL, ((1 & 0x1) << 2) |
		//reg_nr_pre2_inp_c44to42_en
				((1 & 0x1) << 1) | //reg_nr_pre1_inp_c44to42_en
				((1 & 0x1) << 0), 0, 3);//reg_nr_cur_inp_c44to42_en
	else
		w_reg_bit(VPU_NR_CORE_MISC_CTRL, ((1 & 0x1) << 2) |
		//reg_nr_pre2_inp_c44to42_en
			((1 & 0x1) << 1) | //reg_nr_pre1_inp_c44to42_en
			((0 & 0x1) << 0), 0, 3);//reg_nr_cur_inp_c44to42_en

	wr(VPU_NR_ENABLE,
		(reg_ne_en           & 0x1) << 12 | //reg_ne_en
		(reg_cue_en          & 0x1) << 11 | //reg_cue_en
		(reg_cfr_en          & 0x1) << 10 | //reg_cfr_en
		(reg_di_en           & 0x1) << 9 | //reg_di_en
		(reg_nr_tnr_en       & 0x1) << 8 | //reg_nr_tnr_en
		(reg_nr_snr_en       & 0x1) << 7 | //reg_nr_snr_en
		(reg_dblk_en_h       & 0x1) << 6 | //reg_deblk_en_h
		(reg_dblk_en_v       & 0x1) << 5 | //reg_deblk_en_v
		(reg_dblk_stat_en    & 0x1) << 4 | //reg_deblk_statis_info_en
		(reg_xlr_en          & 0x1) << 3 | //reg_xlr_en
		(reg_xlr_side_en     & 0x1) << 2 | //reg_xlr_side_en
		(reg_dmsq_en         & 0x1) << 1 | //reg_dm_en
		(reg_post_en         & 0x1) << 0);//reg_post_process_en

		// w_reg_bit(DPSS_DPE_HW_DBG,0,2,1);//nr_done_mask
	w_reg_bit(DPSS_DPE_HW_DBG, (!di_en), 3, 1);//di_done_mask
	w_reg_bit(VPU_CUE_MODE_ENABLE, reg_cue_en, 4, 1);//reg_cue_enable_l
	w_reg_bit(VPU_CUE_MODE_ENABLE, reg_cue_en, 0, 1);//reg_cue_enable_r
	//w_reg_bit(VPU_MCDI_EN, reg_mcdi_en, 0, 1); //reg_mcdi_en
	w_reg_bit(VPU_DI_BLEND_EI_POST_EN_MODE, reg_ei_en, 2, 1);//reg_di_ei_en
	if (dpss_nr_debug == 1) {
		//w_reg_bit(VPU_POST_NR_GRAD_EDGE_7, 0, 20, 1);//reg_7_flag_en
		w_reg_bit(DPSS_DPE_INTF_AFBCD0, 1, 22, 3);
		//w_reg_bit(DOS_DOWN_S_MODE, 0, 8, 8);
	} else {
		//w_reg_bit(VPU_POST_NR_GRAD_EDGE_7, reg_7_flag_en, 20, 1);//reg_7_flag_en
		wr(DOS_DOWN_S_MODE, 0);
	}

	wr(VPU_DI_DEMO_ENABLE, 0);//demo window disable
#endif
	w_reg_bit(VPU_DI_BLEND_EI_POST_EN_MODE, reg_ei_en, 2, 1);//reg_di_ei_en
	nr_force_config(prm_top, prm_dpe);

//Wr_reg_bits(VPU_DI_DEMO_WINDOW_X,160,16,16);
//Wr_reg_bits(VPU_DI_DEMO_WINDOW_Y,120,16,16);
//==============================================================//
// cfg vdi_size/data-path
//==============================================================//
#ifdef HIS_MARK
	u32 reg_nr_out_hs_en;
	u32 reg_post_in_hs_en;

	if (reg_di_en == 0 &&
		(reg_post_en || reg_7_flag_en || dpss_nr_debug == 1)) {
		reg_nr_out_hs_en = 3;
		reg_post_in_hs_en = 1;
	} else {
		reg_nr_out_hs_en  = 1;
		reg_post_in_hs_en = 0;
	}

	wr(VPU_NR_PIC_SIZE, ((vsize & 0xffff) << 16) | //reg_pic_vsize
		((hsize & 0xffff) << 0));//reg_pic_hsize
	wr(VPU_POST_NR_SLC_PIC_SIZE, ((hsize & 0xffff) << 16) |
		((vsize & 0xffff) << 0));
#ifndef DEBUG_MODE_TEST
	w_reg_bit(VPU_NR_TOP_MISC, reg_post_in_hs_en, 18, 1);
	//reg_post_in_hs_en
	w_reg_bit(VPU_NR_TOP_MISC, reg_nr_out_hs_en, 16, 2);
	//reg_nr_out_hs_en
#endif
#endif
	wr(VPU_NR_PIC_SIZE, ((vsize & 0xffff) << 16) | //reg_pic_vsize
		((hsize & 0xffff) << 0));//reg_pic_hsize
	wr(VPU_POST_NR_SLC_PIC_SIZE, ((hsize & 0xffff) << 16) |
		((vsize & 0xffff) << 0));
	w_reg_bit(VPU_NR_INPUT_TB_INVERT_CNVT, is_psrc, 0, 1);
	//reg_input_is_progressive
	w_reg_bit(VPU_NR_INPUT_TB_INVERT_CNVT, top_is_first, 4, 1);
	//reg_input_top_is_first
	w_reg_bit(VPU_NR_DS_SCALE, me_dsx, 2, 2);//reg_me_dsx_scale
	w_reg_bit(VPU_NR_DS_SCALE, me_dsy, 0, 2);//reg_me_dsy_scale
	//w_reg_bit(VPU_NR_DS_SCALE,0,4,1); //reg_me_h_srgn_mode
	//w_reg_bit(VPU_NR_DS_SCALE,0,5,1); //reg_me_v_srgn_mode
	w_reg_bit(VPU_NR_CORE_MISC_CTRL, 25, 16, 8);
	//reg_nr_hblank_0 (reg_di_en ? 25 : 0) todo
	w_reg_bit(VPU_DI_HW_SLC_INFO, (slc_num - 1), 0, 2);//reg_slc_num
	 w_reg_bit(VPU_POST_NR_SLC_CTRL, (slc_num > 1), 0, 1);//reg_slc_en
	//if (dpss_nr_debug)
		//w_reg_bit(VPU_POST_NR_SLC_CTRL, 0, 0, 1);//reg_slc_en
	//dblk_bnd_flg_data
	int i, k;
	int data = 0x0;

	wr(VPU_DEBLK_GRAD_BND_FLAG_IDX, 0x000);
	for (i = 0; i < hsize; i++) {
		if (i == 0 || i == (hsize - 1) || (i % 8) != 0)
			k = 0;
		else
			k = 1;

		data = (data | (k << (i % 32)));
		if ((i % 32 == 31) || (i == hsize - 1)) {
			wr(VPU_DEBLK_GRAD_BND_FLAG_DATA, data);
			// printf("====hbond_data:%8x,%8x\n",i/32,data);
			data = 0x0;
		}
	}

	data = 0x0;
	wr(VPU_DEBLK_GRAD_BND_FLAG_IDX, 0x100);
	for (i = 0; i < vsize; i++) {
		if (i == 0 || i == (vsize - 1) || (i % 8) != 0)
			k = 0;
		else
			k = 1;

		data = (data | (k << (i % 32)));

		if ((i % 32 == 31) || (i == vsize - 1)) {
			wr(VPU_DEBLK_GRAD_BND_FLAG_DATA, data);
			// printf("======vbond_data:%8x,%8x\n",i/32,data);
			data = 0x0;
	}
	}
	w_reg_bit(VPU_VBE_TOP_HOLD, 1920, 0, 13);//reg_hold_hnum
	w_reg_bit(VPU_VBE_TOP_HOLD, 5, 16, 13);//reg_hold_vnum

	cfg_dpss_vdi_slice(hsize, vsize, slc_num, prm_top, nr_pps_cfg);
//	wr(DPSS_FBUF_DPE_RDMA_INFO, DPSS_HW_LOOP_IN_OUT_BUF_NUB);//reg_pic_hsize
//	w_reg_bit(DPSS_FBUF_DAE_INIT, DPSS_HW_LOOP_IN_OUT_BUF_NUB, 24, 4);
//	w_reg_bit(DPSS_FBUF_DAE_INIT + 0x300, DPSS_HW_LOOP_IN_OUT_BUF_NUB, 24, 4);
}

//==============================================================//
// cfg vdi_slice
//==============================================================//
void cfg_dpss_vdi_slice(u32 frm_hsize,
	u32 frm_vsize,
	u32 slc_num,
	struct PRM_DPSS_TOP *prm_top,
	struct AA_PPS_TOP_TYPE *nr_pps_cfg)
{
	int i; //ary add

	w_reg_bit(VPU_NR_SLC_PAD_SW_CTRL, 1, 0, 1); //reg_slc_use_sw_config
	w_reg_bit(VPU_NR_SLICE_NUM, slc_num - 1, 0, 3); //reg_slc_num_m1
	wr(VPU_NR_SLICE_IDX, 0x0);

//	u32 frm_hsize_sel = rd(DPSS_DPE_INTF_CTRL) >> 17 & 0x7;
	u32 frm_hsize_sel = prm_top->frm_hsize_sel;
	u32 frm_hsize_d4;
	u32 frm_hsize_d4_d16;
	u32 frm_hsize_d4_d32;
	u32 frm_hsize_d4_d64;
	u32 frm_hsize_d4_d128;
	u32 frm_hsize_d4_rnd;

	frm_hsize_d4      = (frm_hsize + 3) >> 2;
	frm_hsize_d4_d16  = (frm_hsize_d4 + 15) >> 4;
	frm_hsize_d4_d32  = (frm_hsize_d4 + 31) >> 5;
	frm_hsize_d4_d64  = (frm_hsize_d4 + 63) >> 6;
	frm_hsize_d4_d128 = (frm_hsize_d4 + 127) >> 7;

	frm_hsize_d4_rnd = (frm_hsize_sel == 4) ?
		frm_hsize_d4_d128 << 7 : (frm_hsize_sel == 3) ?
		frm_hsize_d4_d64 << 6 : (frm_hsize_sel == 2) ?
		frm_hsize_d4_d32 << 5 : (frm_hsize_sel == 1) ?
		frm_hsize_d4_d16 << 4 : frm_hsize_d4;

	u32 slc_act_hsize_align;
	//slc_act_hsize_align = (frm_hsize + slc_num -1)/slc_num;

	slc_act_hsize_align = slc_num == 1 ? frm_hsize :
	slc_num == 2 ? frm_hsize_d4_rnd << 1 : frm_hsize_d4_rnd;
	slc_act_hsize_align = ((slc_act_hsize_align + 1) / 2) * 2; //even align

	u32 slc_act_hsize[4];//u16
	u32 slc_act_hbgn[4] ;//u14

	for (i = 0; i < slc_num - 1; i++)
		slc_act_hsize[i] = slc_act_hsize_align;

	slc_act_hsize[slc_num - 1] = frm_hsize - slc_act_hsize_align * (slc_num - 1);

	u32 slc_act_hbgn_acc = 0;

	for (i = 0; i < slc_num; i++) {
		slc_act_hbgn[i]  = slc_act_hbgn_acc;
		slc_act_hbgn_acc += slc_act_hsize[i];
	}

//ary no use    u32 dpss_dpe_slc_out_bgn[4];
//ary no use    u32 dpss_dpe_slc_out_end[4];
//==============================================================//
//nr slice
//==============================================================//
	u32 hbord;

	for (i = 0; i < slc_num; i++) {
		if (i == 0)
			hbord = 1;
		else
			hbord = 0;

		if (i == slc_num - 1)
			hbord = hbord | 2; //right border
		else
			hbord = hbord | 0;

		wr(VPU_NR_SLICE_HDATA, ((hbord & 0x3) << 30)
			//nr_slcxn_hbord[i]<<30
			| ((slc_act_hbgn[i] & 0x3fff) << 16)
			//slc_bgn in pic_size
			| ((slc_act_hsize[i] & 0xffff)));
		wr(VPU_NR_SLICE_VDATA, ((0x3 & 0x3) << 30) |
			((0 & 0x3fff) << 16) |
			((frm_vsize & 0xffff) << 0));//no slice in veritial

		dbg_h2("= dpss_vdi_slice slc_num = %d hbord = %d =\n", slc_num, hbord);
		dbg_h2("= slc_hbgn = %d slc_hsize = %d\n", slc_act_hbgn[i], slc_act_hsize[i]);
	}
	wr(VPU_DBLK_NR_OUP_HCROP, 0x0);//crop

	//==============================================================//
	//cfg di slice
	//==============================================================//
	// u32 ext_pad = prm_top->nr_aapps_up | prm_top->nr_aapps_on ? 16 : 0;
	u32 ext_pad = nr_pps_cfg->pps_en | prm_top->nr_aapps_on ? 16 : nr_pps_cfg->aa_pad;
	// u32 ovlp_size       = slc_num == 1 ? 0 : 64 + ext_pad;
	// u32 di_out_cut_ovlp = slc_num == 1 ? 0 : 62;
	u32 ovlp_size       = slc_num == 1 ? 0 : 64;
	u32 di_out_cut_ovlp = slc_num == 1 ? 0 : 62 - ext_pad;
	u32 di_out_cut_ovlp_demo = slc_num == 1 ? 0 : 64 - ext_pad;

	u32 pix_rd_xbgn[4];
	u32 pix_rd_xend[4];
	u32 pix_slc_hsize[4];
	//u32 diout_slc_xbgn[4];
	//u32 diout_slc_xend[4];
	//u32 diout_slc_hsize[4];

	//pix_rd_xbgn[0] = slc_act_hbgn[0] - 0;
	//pix_rd_xend[0] = slc_act_hbgn[0] + slc_act_hsize[0] + ovlp_size - 1;
	//pix_rd_xbgn[1] = slc_act_hbgn[1] - ovlp_size;
	//pix_rd_xend[1] = slc_act_hbgn[1] + slc_act_hsize[1] + ovlp_size - 1;
	//pix_rd_xbgn[2] = slc_act_hbgn[2] - ovlp_size;
	//pix_rd_xend[2] = slc_act_hbgn[2] + slc_act_hsize[2] + ovlp_size - 1;
	//pix_rd_xbgn[3] = slc_act_hbgn[3] - ovlp_size;
	//pix_rd_xend[3] = slc_act_hbgn[3] + slc_act_hsize[3] - 1;

	for (i = 0; i < slc_num; i++) {
		if (i == 0)
			pix_rd_xbgn[i] = slc_act_hbgn[i] - 0;
		else
			pix_rd_xbgn[i] = slc_act_hbgn[i] - ovlp_size;

		if (i == (slc_num - 1))
			pix_rd_xend[i] = slc_act_hbgn[i] + slc_act_hsize[i] - 1;
		else
			pix_rd_xend[i] = slc_act_hbgn[i] +
				slc_act_hsize[i] + ovlp_size - 1;
	}

	diout_slc_xbgn[0] = pix_rd_xbgn[0];
	diout_slc_xend[0] = pix_rd_xend[0] - di_out_cut_ovlp;
	diout_slc_xbgn[1] = pix_rd_xbgn[1] + di_out_cut_ovlp;
	diout_slc_xend[1] = pix_rd_xend[1] - di_out_cut_ovlp;
	diout_slc_xbgn[2] = pix_rd_xbgn[2] + di_out_cut_ovlp;
	diout_slc_xend[2] = pix_rd_xend[2] - di_out_cut_ovlp;
	diout_slc_xbgn[3] = pix_rd_xbgn[3] + di_out_cut_ovlp;
	diout_slc_xend[3] = pix_rd_xend[3];

	diout_demo_slc_xbgn[0] = pix_rd_xbgn[0];
	diout_demo_slc_xend[0] = pix_rd_xend[0] - di_out_cut_ovlp_demo;
	diout_demo_slc_xbgn[1] = pix_rd_xbgn[1] + di_out_cut_ovlp_demo;
	diout_demo_slc_xend[1] = pix_rd_xend[1] - di_out_cut_ovlp_demo;
	diout_demo_slc_xbgn[2] = pix_rd_xbgn[2] + di_out_cut_ovlp_demo;
	diout_demo_slc_xend[2] = pix_rd_xend[2] - di_out_cut_ovlp_demo;
	diout_demo_slc_xbgn[3] = pix_rd_xbgn[3] + di_out_cut_ovlp_demo;
	diout_demo_slc_xend[3] = pix_rd_xend[3];

	for (i = 0; i < slc_num; i++)
		pix_slc_hsize[i] = pix_rd_xend[i] - pix_rd_xbgn[i] + 1;

	for (i = 0; i < slc_num; i++)
		diout_slc_hsize[i] = diout_slc_xend[i] -
			diout_slc_xbgn[i] + 1;
	for (i = 0; i < slc_num; i++)
		diout_demo_slc_hsize[i] = diout_demo_slc_xend[i] -
			diout_demo_slc_xbgn[i] + 1;

	//for di ro register
	wr(VPU_DI_HW_SLC_HBGN_10, ((slc_act_hbgn[1] & 0x3fff) << 16) |
		((slc_act_hbgn[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_HBGN_32, ((slc_act_hbgn[3] & 0x3fff) << 16) |
			((slc_act_hbgn[2] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_HSIZE_10, ((slc_act_hsize[1] & 0x3fff) << 16) |
			((slc_act_hsize[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_HSIZE_32, ((slc_act_hsize[3] & 0x3fff) << 16) |
			((slc_act_hsize[2] & 0x3fff) << 0));

	wr(VPU_DI_HW_SLC_DIN_HBGN_10, ((pix_rd_xbgn[1] & 0x3fff) << 16) |
		((pix_rd_xbgn[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DIN_HBGN_32, ((pix_rd_xbgn[3] & 0x3fff) << 16) |
		((pix_rd_xbgn[2] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_CUR_DIN_HBGN_10, ((pix_rd_xbgn[1] & 0x3fff) << 16) |
		((pix_rd_xbgn[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_CUR_DIN_HBGN_32, ((pix_rd_xbgn[3] & 0x3fff) << 16) |
		((pix_rd_xbgn[2] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_CUR_DIN_HSIZE_10, ((pix_slc_hsize[1] & 0x3fff) << 16) |
		((pix_slc_hsize[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_CUR_DIN_HSIZE_32, ((pix_slc_hsize[3] & 0x3fff) << 16) |
		((pix_slc_hsize[2] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DIN_HSIZE_10, ((pix_slc_hsize[1] & 0x3fff) << 16) |
		((pix_slc_hsize[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DIN_HSIZE_32, ((pix_slc_hsize[3] & 0x3fff) << 16) |
		((pix_slc_hsize[2] & 0x3fff) << 0));

	wr(VPU_DI_HW_SLC_DI_OUT_HBGN_10, ((diout_slc_xbgn[1] & 0x3fff) << 16) |
		((diout_slc_xbgn[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DI_OUT_HBGN_32, ((diout_slc_xbgn[3] & 0x3fff) << 16) |
		((diout_slc_xbgn[2] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_10,
		((diout_slc_hsize[1] & 0x3fff) << 16) |
		((diout_slc_hsize[0] & 0x3fff) << 0));
	wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_32,
		((diout_slc_hsize[3] & 0x3fff) << 16) |
		((diout_slc_hsize[2] & 0x3fff) << 0));
	if (dpss_di_debug || dpss_nr_debug || dpss_force_nr_debug) {
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_10,
			((diout_demo_slc_xbgn[1] & 0x3fff) << 16) |
			((diout_demo_slc_xbgn[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HBGN_32,
			((diout_demo_slc_xbgn[3] & 0x3fff) << 16) |
			((diout_demo_slc_xbgn[2] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_10,
			((diout_demo_slc_hsize[1] & 0x3fff) << 16) |
			((diout_demo_slc_hsize[0] & 0x3fff) << 0));
		wr(VPU_DI_HW_SLC_DI_OUT_HSIZE_32,
			((diout_demo_slc_hsize[3] & 0x3fff) << 16) |
			((diout_demo_slc_hsize[2] & 0x3fff) << 0));
	}

	if (slc_num == 1)
		wr(VPU_POST_NR_SLC_ACT_HSIZE_0, ((slc_act_hsize[1] & 0xffff) << 16) |
		((slc_act_hsize[0] & 0xffff) << 0));
	else if (slc_num == 2)
		wr(VPU_POST_NR_SLC_ACT_HSIZE_0,
			(((slc_act_hsize[1] + ext_pad) & 0xffff) << 16) |
			(((slc_act_hsize[0] + ext_pad) & 0xffff) << 0));
	else
		wr(VPU_POST_NR_SLC_ACT_HSIZE_0,
			(((slc_act_hsize[1] + ext_pad * 2)  & 0xffff) << 16) |
			(((slc_act_hsize[0] + ext_pad) & 0xffff) << 0));

	wr(VPU_POST_NR_SLC_ACT_HSIZE_1, (((slc_act_hsize[3] + ext_pad) & 0xffff) << 16) |
		(((slc_act_hsize[2] + ext_pad * 2) & 0xffff) << 0));
	w_reg_bit(VPU_POST_NR_SLC_HBEGIN_0,//from dae vlsi for di debug pattern
		slc_act_hsize[0] - 2, 0, 13);//reg_slc1_h_bgn
	w_reg_bit(VPU_POST_NR_SLC_HBEGIN_0,
		slc_act_hsize[0] + slc_act_hsize[1] - 2,
		16, 13);//reg_slc2_h_bgn
	w_reg_bit(VPU_POST_NR_SLC_HBEGIN_1,
		slc_act_hsize[0] + slc_act_hsize[1] + slc_act_hsize[2] - 2,
		0, 13);//reg_slc3_h_bgn
	if (!prm_top->is_i || dpss_di_debug || dpss_nr_debug ||
		dpss_force_nr_debug) {
		wr(VPU_POST_NR_SLC_OVLP_SIZE, (((0x0 & 0xff) << 24) |
			((0x0 & 0xff) << 16) | ((0x0 & 0xff) << 8) |
			((0x0 & 0xff) << 0)));//need lusen check 2 or4???
		w_reg_bit(VPU_NR_ENABLE, 0, 0, 1);
	} else {
		wr(VPU_POST_NR_SLC_OVLP_SIZE, (((0x2 & 0xff) << 24) |
			((0x2 & 0xff) << 16) | ((0x2 & 0xff) << 8) |
			((0x2 & 0xff) << 0)));//need lusen check 2 or4???
		w_reg_bit(VPU_NR_ENABLE, 1, 0, 1);
	}
	wr(VPU_DBLK_DI_DPOST_OUP_HCROP, 0x0);//crop

	wr(VPU_NR_SLC_CUR_PAD, (0 << 24) | (0 << 16) |
		(ovlp_size << 8) | ovlp_size);
	wr(VPU_NR_SLC_PRE1_PAD, (0 << 24) | (0 << 16) |
		(ovlp_size << 8) | ovlp_size);
	wr(VPU_NR_SLC_PRE2_PAD, (0 << 24) | (0 << 16) |
		(ovlp_size << 8) | ovlp_size);

//dmsq pad
u32 dmsq_sft = 1;
u32 reg_vbe_dmsq_pad = ovlp_size >> dmsq_sft;

	w_reg_bit(DPSS_DPE_PAD, reg_vbe_dmsq_pad, 16, 8);
	//for hist check
	wr(VPSS_VBE_HIST_WIN_HBG0, ovlp_size << 16 | 0);
	wr(VPSS_VBE_HIST_WIN_HBG1, ovlp_size << 16 | ovlp_size);
	wr(VPSS_VBE_HIST_WIN_HED0, (ovlp_size + slc_act_hsize[1] - 1) << 16 |
		(slc_act_hsize[0] - 1));
	wr(VPSS_VBE_HIST_WIN_HED1, (ovlp_size + slc_act_hsize[3] - 1) << 16 |
		(ovlp_size + slc_act_hsize[2] - 1));
	wr(VPSS_VBE_HIST_WIN_HSZ0, (pix_slc_hsize[1] << 16 | pix_slc_hsize[0]));
	wr(VPSS_VBE_HIST_WIN_HSZ1, (pix_slc_hsize[3] << 16 | pix_slc_hsize[2]));
	//wr(VPSS_VBE_HIST_WIN_VSZ0, (slc_vsize << 16 | slc_vsize));
	//wr(VPSS_VBE_HIST_WIN_VSZ1, (slc_vsize << 16 | slc_vsize));
	//wr(VPSS_VBE_HIST_WIN_VBG0, 0);
	//wr(VPSS_VBE_HIST_WIN_VBG1, 0);
	//wr(VPSS_VBE_HIST_WIN_VED0, (slc_vsize - 1) << 16 | (slc_vsize - 1));
	//wr(VPSS_VBE_HIST_WIN_VED1, (slc_vsize - 1) << 16 | (slc_vsize - 1));
} //cfg_dpss_vdi_slice

void cfg_dpss_vdi_istop(u32 di_frm_cnt, bool top)
{
	unsigned int cnt;

	cnt = !(di_frm_cnt & 0x1);

	if (top) {
		cnt = (cnt + 1) & 0x01;
		dbg_h2("%s\n", "set hw_tb");
	}
	w_reg_bit(VPU_VPSS_VBE_SLICE_CTRL, 1, 5, 1);//reg_frmidx_foc_en
	w_reg_bit(VPU_VPSS_VBE_SLICE_CTRL, (cnt << 1) |	cnt, 6, 2);//reg_frmidx_foc_val
	if (di_frm_cnt == 0)
		w_reg_bit(VPU_DI_HW_SLC_INFO, 1, 8, 1);//reg_di_fst_frm
	else
		w_reg_bit(VPU_DI_HW_SLC_INFO, 0, 8, 1);//reg_di_fst_frm
} //cfg_dpss_vdi_istop
