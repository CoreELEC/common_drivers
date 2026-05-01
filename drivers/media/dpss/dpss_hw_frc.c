// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "./sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#endif
#include "dpss_base.h"
#include "dpss_hw.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_rdma_frc.h"
#include "dpss_s_frc.h"
#include "dpss_hw_frc.h"

#include "./hw/define.h"
#include "./hw/dpss.h"
#include "./hw/dpss_intf.h"
#include "./hw/dpss_mc.h"
#include "./hw/vfcd.h"
#include "hw/dpss_lib.h"
#include "dpss_func.h"

unsigned int used_rdma = 1; //force g_dpss_tst_case
module_param_named(used_rdma, used_rdma, uint, 0664);

u32 update_reg_val(u32 reg_val, u32 val, u8 start, u8 len)
{
	u32 mask = (((1L << len) - 1) << start);
	u32 tmp;

	tmp = reg_val & ~mask;
	tmp |= (val << start) & mask;
	return tmp;
}

void hw_cfg_dpss_mc_ini(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr)
{
	bool		   game_mode	 = prm_top->dpe_game_mode & 0x1;
	bool		   me_mc_link_en = prm_top->me_mc_link_en & 0x1;
	bool		   mc_lp_mode	 = prm_top->mc_lp_mode & 0x1;
	// struct DPSS_MC0_TYPE prm_mc;
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	prm_mc = &pchip->prm_mc;
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 4, 4, 13);
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 3, 0, 2);
	// w_reg_bit(DPSS_DPE_MC_START_CTRL, 4, 4, 13);
	//w_reg_bit(DPSS_DPE_MC_TOP_GCLK, 0xF, 8, 4);
	//w_reg_bit(DPSS_DPE_MC_TOP_GCLK, 2, 16, 2);
	wr(DPSS_DPE_MC_TOP_GCLK, 0x20f00);
	// game mode cfg start
	// w_reg_bit(FRC_MC_HW_CTRL0, (game_mode | mc_lp_mode), 3, 1);	// lp_mode
	if (game_mode)
		w_reg_bit(FRC_MC_HW_CTRL0, 0, 3, 1); // game mode disable mc lp mode
	else
		w_reg_bit(FRC_MC_HW_CTRL0, mc_lp_mode, 3, 1);	//lp_mode

	if (game_mode && prm_top->frm_hsize * prm_top->frm_hsize < 1920 * 1080) {
		// for game 720P mv err
		w_reg_bit(FRC_MC_MVRD_CTRL, 1, 0, 1);
		w_reg_bit(FRC_MC_MVRD_CTRL, 1, 1, 1);
	} else {
		w_reg_bit(FRC_MC_MVRD_CTRL, !(game_mode || me_mc_link_en), 0, 1);
		w_reg_bit(FRC_MC_MVRD_CTRL, !game_mode, 1, 1);
	}

	// game mode cfg end
	prm_mc->mmu_mode_en = prm_top->mif_mmu_mode_en;
	prm_mc->mc_rev_mode = prm_top->frc_rev_mode;
	prm_mc->pad_en	   = prm_top->auto_alig_en;
	/*tmp change for I, if nr fix I can not auto_alig_en, need remove*/
	if (prm_top->is_i)
		prm_mc->pad_en = 1;
	prm_mc->pad_hmode   = prm_top->alig_hmode;
	prm_mc->pad_vmode   = prm_top->alig_vmode;

	if (prm_top->mc_auto_en == 1) {						   // fpga en ctrl
		w_reg_bit(DPSS_FRC_ONE_STEP_DISP_EN, 1, 0, 1);   // change MC TBC to auto mode;
		w_reg_bit(DPSS_TOP_APB_TBC_SEL_CTRL, 1, 0, 1);   // open MC port TBC apb
	} else {
		w_reg_bit(DPSS_FRC_ONE_STEP_DISP_EN, 0, 0, 1);
		w_reg_bit(DPSS_TOP_APB_TBC_SEL_CTRL, 0, 0, 1);
	}
	hw_cfg_dpss_mc0(prm_top, src_from_nr, prm_mc);
	hw_cfg_dpss_mc_intf(prm_top, prm_mc);
}

void hw_cfg_dpss_mc_intf(struct PRM_DPSS_TOP *prm_top, struct DPSS_MC0_TYPE *prm_mc)
{
	u32 fmt		   = prm_mc->pix_fmt == YUV444 ? 0 : prm_mc->pix_fmt == YUV422 ? 1 : 2;
	// u32 fmt444_out = 0;
	u32 fmt444_out = ((prm_top->frc_vfcd_vfmt == 1) && (fmt == 2)) ? 2 : 0;
	//    u32 bit  = prm_mc.pix_bit == BIT_8  ? 0 : prm_mc.pix_bit == BIT_10 ? 1 : 2 ;
	u32 regs_ofst;
	bool cut_win_en = prm_top->cut_win_en;
	//bool mc_cut_position = prm_top->cut_win_position;
	u32  mc_skip_mode = prm_top->mc_skip_mode;
	//u32 frm_hsize = (mc_skip_mode > 0) ? prm_top->frm_hsize : prm_mc->frm_hsize;
	//u32 frm_vsize = (mc_skip_mode > 0) ? prm_top->frm_vsize : prm_mc->frm_vsize;
	u32  mc_skip_mode_h = (mc_skip_mode >> 1) & 0x1;
	u32  mc_skip_mode_v = mc_skip_mode & 0x1;
	//    struct PRM_INTF_TYPE pix_rmif;
	//    memset((void *)(&pix_rmif), 0, sizeof(struct PRM_INTF_TYPE)); //ary add
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct PRM_INTF_TYPE *pix_rmif;
	struct VFCD_t *vfcd;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	pix_rmif = &pchip->mc_pix_rmif;
	vfcd = &pchip->mc_vfcd;

	dbg_h2("%s fmt444_out = %d, h_skip:%d, v_skip:%d\n", __func__,
			fmt444_out, mc_skip_mode_h, mc_skip_mode_v);
	pchip->fmt444_out = fmt444_out;
	pix_rmif->src_fmt	= prm_mc->pix_fmt;	  // YUV444/YUV422/YUV420/RGB
	pix_rmif->src_plan	= prm_mc->pix_plan;	  // PLANAR_X1/X2
	pix_rmif->src_bit	= prm_mc->pix_bit;	  // BIT_8/10
	pix_rmif->src_cmpr	= CMPR_UN;			  // un/afbc/afrc
	pix_rmif->interlace = IS_PSRC;			  // IS_PSRC/IS_ISRC

	pix_rmif->src_hsize = cut_win_en ? prm_top->prm_cut_win.frm_hsize : prm_top->frm_hsize;
	pix_rmif->src_vsize = cut_win_en ? prm_top->prm_cut_win.frm_vsize : prm_top->frm_vsize;
	//pix_rmif->src_hsize = cut_win_en ? prm_top->prm_cut_win.frm_hsize : frm_hsize;
	//pix_rmif->src_vsize = cut_win_en ? prm_top->prm_cut_win.frm_vsize : frm_vsize;

	pix_rmif->reverse[0]  = prm_mc->mc_rev_mode & 0x1;
	pix_rmif->reverse[1]  = (prm_mc->mc_rev_mode & 0x2) >> 1;
	pix_rmif->mmu_mode_en = prm_mc->mmu_mode_en;
	pix_rmif->pad_en	  = prm_mc->pad_en && (!mc_skip_mode);
	pix_rmif->pad_hmode	  = prm_mc->pad_hmode;
	pix_rmif->pad_vmode	  = prm_mc->pad_vmode;

	pix_rmif->slc_x_st[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn_align : 0;
	pix_rmif->slc_x_ed[0] =
		cut_win_en ? prm_top->prm_cut_win.win_hend_align : prm_top->frm_hsize - 1;
	pix_rmif->slc_y_st[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn_align : 0;
	pix_rmif->slc_y_ed[0] =
		cut_win_en ? prm_top->prm_cut_win.win_vend_align : prm_top->frm_vsize - 1;
	pix_rmif->skip_line = mc_skip_mode_v;
	pix_rmif->cut_win_en	= cut_win_en;

	if (pps->pps_en) {
		pix_rmif->src_hsize = cut_win_en ?
				prm_top->prm_cut_win.frm_hsize : prm_mc->frm_hsize;
		pix_rmif->src_vsize = cut_win_en ?
				prm_top->prm_cut_win.frm_vsize : prm_mc->frm_vsize;
		pix_rmif->slc_x_st[0] = cut_win_en ?
				prm_top->prm_cut_win.win_hbgn_align : 0;
		pix_rmif->slc_x_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_hend_align : prm_mc->frm_hsize - 1;
		pix_rmif->slc_y_st[0] = cut_win_en ?
				prm_top->prm_cut_win.win_vbgn_align : 0;
		pix_rmif->slc_y_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_vend_align : prm_mc->frm_vsize - 1;
	}
	dbg_i0("%s:is_pps top ch=%d,=%d\n", __func__, prm_top->ch, pps->pps_en);
	// nr+frc 422 10bit 2plane
	if (prm_top->dpss_mode == DPSS_FRC_MODE) {
		pix_rmif->little_endian = 0;
		pix_rmif->swap_64bit = 1;
		//pix_rmif->uv_swap = 0;//prm_top->uv_swap; // 1;
	} else {
		pix_rmif->little_endian = prm_top->l_endian;
		pix_rmif->swap_64bit = prm_top->swap_64bit;//1;
		//pix_rmif->uv_swap = 0;
	}
	dbg_h2("%s src_bit %d pix_bit %d\n", __func__, pix_rmif->src_bit, prm_mc->pix_bit);
	//cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, 0);
	//cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif, 0);
	//dbg_f2("dpss_init_done=%d\n", prm_top->is_dpss_init_done);
//	if (prm_top->is_dpss_init_done)
//		cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, fmt444_out);
//	cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif, fmt444_out);
	if (mc_skip_mode_h) {
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC0);
		w_reg_bit_vc(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC1);
		w_reg_bit_vc(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
	}

	init_dpss_vfcd(0, vfcd);

	vfcd->cmpr_sel	   = prm_mc->src_cmpr == CMPR_AFBC ? 1 :
				prm_mc->src_cmpr == CMPR_AFRC ? 2 : 0;
	vfcd->frm_en_sel = 1;
	u32 dec_src_bit	   = prm_mc->src_cmpr == CMPR_UN  ? 10
						 : prm_mc->pix_bit == BIT_008 ? 8
						 : prm_mc->pix_bit == BIT_012 ? 12
						 : 10;
	vfcd->cut_win_en   = cut_win_en || mc_skip_mode_h || mc_skip_mode_v;
	vfcd->src_hsize	   = cut_win_en ? prm_top->prm_cut_win.frm_hsize : prm_top->frm_hsize;
	vfcd->src_vsize	   = cut_win_en ? prm_top->prm_cut_win.frm_vsize : prm_top->frm_vsize;
	vfcd->fmt444_out   = fmt444_out;
	vfcd->win_bgn_h[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn_align : 0;
	vfcd->win_end_h[0] =
		cut_win_en ? prm_top->prm_cut_win.win_hend_align : prm_top->frm_hsize - 1;
	vfcd->win_bgn_v[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn_align : 0;
	vfcd->win_end_v[0] =
		cut_win_en ? prm_top->prm_cut_win.win_vend_align : prm_top->frm_vsize - 1;
	vfcd->compbits_y =
		dec_src_bit;// pix_rmif.src_bit == BIT_8 ? 8 :pix_rmif.src_bit == BIT_12 ? 12 :10 ;
	vfcd->compbits_c =
		dec_src_bit;// pix_rmif.src_bit == BIT_8 ? 8 :pix_rmif.src_bit == BIT_12 ? 12 :10 ;
	// vfcd->head_baddr   = 0;//need_add
	// vfcd->body_baddr   = 0;//need_add

	if (pps->pps_en) {
		vfcd->src_hsize    = cut_win_en ?
			prm_top->prm_cut_win.frm_hsize : prm_mc->frm_hsize;
		vfcd->src_vsize    = cut_win_en ?
			prm_top->prm_cut_win.frm_vsize : prm_mc->frm_vsize;
		vfcd->win_end_h[0] = cut_win_en ?
			prm_top->prm_cut_win.win_hend_align : prm_mc->frm_hsize - 1;
		vfcd->win_end_v[0] = cut_win_en ?
			prm_top->prm_cut_win.win_vend_align : prm_mc->frm_vsize - 1;
	}

	vfcd->src_fmt	  = fmt;
	vfcd->hfmt_en      = 0;
	vfcd->vfmt_en      = (fmt444_out != 0) ? prm_mc->pix_fmt == YUV420 : 0;
	vfcd->rev_mode	  = prm_mc->mc_rev_mode;
	vfcd->y_h_skip_en = mc_skip_mode_h;
	vfcd->y_v_skip_en = mc_skip_mode_v;
	vfcd->c_h_skip_en = mc_skip_mode_h;
	vfcd->c_v_skip_en = mc_skip_mode_v;
	vfcd->yc_rst_sep  = prm_mc->src_cmpr == CMPR_AFBC ? 0 : 1;
	vfcd->mmu_mode_en =
		prm_mc->comp_setting.afrc_head_mode == 0 ? prm_mc->comp_setting.mmu_mode_en : 0;
	vfcd->mmu_page_mode = prm_mc->comp_setting.mmu_page_mode;

	vfcd->afrcd.luma_comp_target = prm_mc->comp_setting.afrc_target_byte_y;
	vfcd->afrcd.luma_header_en	 = prm_mc->comp_setting.afrc_head_mode;

	vfcd->afrcd.chrm_comp_target = prm_mc->comp_setting.afrc_target_byte_c;
	vfcd->afrcd.chrm_header_en	 = prm_mc->comp_setting.afrc_head_mode;

	vfcd->afrcd.luma_dict_en = prm_top->comp_setting.afrc_dict_mode_y;
	vfcd->afrcd.chrm_dict_en = prm_top->comp_setting.afrc_dict_mode_c;

	dbg_h2("%s dict_en:%d %d\n", __func__, vfcd->afrcd.luma_dict_en, vfcd->afrcd.chrm_dict_en);
	dbg_h2("%s cut_win_en:%d\n", __func__, cut_win_en);
	dbg_h2("%s vfcd->src_h/v size:(%d,%d)\n", __func__, vfcd->src_hsize, vfcd->src_vsize);
	dbg_h2("%s pix_rmif.slc_x:(%d,%d), .slc_y:(%d,%d)\n", __func__, vfcd->win_bgn_h[0],
		   vfcd->win_end_h[0], vfcd->win_bgn_v[0], vfcd->win_end_v[0]);
//	if (prm_top->is_dpss_init_done)
//		cfg_vfcd_dec(4, vfcd);	 // mc0
//	cfg_vfcd_dec(5, vfcd);	 // mc1

	if (prm_top->is_dpss_init_done) {
		// mc din vfcd mode
		if (prm_mc->src_cmpr == CMPR_AFBC) {
			cfg_vfcd_cmpr_enable(4, 1);	  // vfcd_chose cur_afbc
			cfg_vfcd_cmpr_enable(5, 1);	  // vfcd_chose pre_afbc
			wr_vc(4 * 256 + VFCD_TOP_CTRL3, 0xa);
			wr_vc(5 * 256 + VFCD_TOP_CTRL3, 0xa);
		} else if (prm_mc->src_cmpr == CMPR_AFRC) {
			cfg_vfcd_cmpr_enable(4, 2);	  // vfcd_chose cur_afbc
			cfg_vfcd_cmpr_enable(5, 2);	  // vfcd_chose pre_afbc
		} else {
			cfg_vfcd_cmpr_enable(4, 0);	  // vfcd_chose cur_mif
			cfg_vfcd_cmpr_enable(5, 0);	  // vfcd_chose pre_mif
		}
	}

	u32 x_scl = prm_top->dpe_dw_dsx;
	u32 y_scl = prm_top->dpe_dw_dsy;
	u32 pre_logo_rmif_bits = 1;
	u32 cur_logo_rmif_bits = 1;
	u32 mv_rmif_bits = 64;
	u32 me_logo_rmif_bits  = 1;

	u32 pre_logo_hsize = prm_top->frm_hsize >> x_scl;
	u32 cur_logo_hsize = prm_top->frm_hsize >> x_scl;
	u32 mv_hsize	   = pre_logo_hsize >> 2;
	u32 me_logo_hsize  = pre_logo_hsize >> 2;
	u32 mc_out_hbgn = 0;
	u32 mc_out_hend = 0;
	u32 mc_out_vbgn = 0;
	u32 mc_out_vend = 0;
	struct PRM_INTF_TYPE *pre_logo_rmif = &pchip->pre_logo_rmif;
	struct PRM_INTF_TYPE *cur_logo_rmif = &pchip->cur_logo_rmif;
	struct PRM_INTF_TYPE *mv_rmif = &pchip->mv_rmif;
	struct PRM_INTF_TYPE *me_logo_rmif = &pchip->me_logo_rmif;

	memset((void *)(pre_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(cur_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(mv_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(me_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));

	pre_logo_rmif->burst_len = 2;
	cur_logo_rmif->burst_len = 2;
	mv_rmif->burst_len		= 2;
	me_logo_rmif->burst_len	= 2;
	if (cut_win_en) {
		pre_logo_rmif->bits_mode = 12;
		cur_logo_rmif->bits_mode = 12;
		mv_rmif->bits_mode		= 4;
		me_logo_rmif->bits_mode	= 12;

		pre_logo_rmif->pack_mode = 4;
		cur_logo_rmif->pack_mode = 4;
		mv_rmif->pack_mode		= 4;
		me_logo_rmif->pack_mode	= 0;

		pre_logo_rmif->slc_x_st[0] = prm_top->prm_cut_win.win_hbgn_align >> x_scl;
		pre_logo_rmif->slc_x_ed[0] = prm_top->prm_cut_win.win_hend_align >> x_scl;
		pre_logo_rmif->slc_y_st[0] = prm_top->prm_cut_win.win_vbgn_align >> y_scl;
		pre_logo_rmif->slc_y_ed[0] = prm_top->prm_cut_win.win_vend_align >> y_scl;

		cur_logo_rmif->slc_x_st[0] = prm_top->prm_cut_win.win_hbgn_align >> x_scl;
		cur_logo_rmif->slc_x_ed[0] = prm_top->prm_cut_win.win_hend_align >> x_scl;
		cur_logo_rmif->slc_y_st[0] = prm_top->prm_cut_win.win_vbgn_align >> y_scl;
		cur_logo_rmif->slc_y_ed[0] = prm_top->prm_cut_win.win_vend_align >> y_scl;

		mv_rmif->slc_x_st[0] = pre_logo_rmif->slc_x_st[0] >> 2;
		mv_rmif->slc_x_ed[0] = pre_logo_rmif->slc_x_ed[0] >> 2;
		mv_rmif->slc_y_st[0] = pre_logo_rmif->slc_y_st[0] >> 2;
		mv_rmif->slc_y_ed[0] = pre_logo_rmif->slc_y_ed[0] >> 2;

		me_logo_rmif->slc_x_st[0] = pre_logo_rmif->slc_x_st[0] >> 2;
		me_logo_rmif->slc_x_ed[0] = pre_logo_rmif->slc_x_ed[0] >> 2;
		me_logo_rmif->slc_y_st[0] = pre_logo_rmif->slc_y_st[0] >> 2;
		me_logo_rmif->slc_y_ed[0] = pre_logo_rmif->slc_y_ed[0] >> 2;

		pre_logo_rmif->stride = (((pre_logo_hsize * pre_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;
		cur_logo_rmif->stride = (((cur_logo_hsize * cur_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;
		mv_rmif->stride = (((mv_hsize * mv_rmif_bits + 127) >> 7) + 3) >> 2 << 2;
		me_logo_rmif->stride = (((me_logo_hsize * me_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;

		cfg_mc_sub_rdmif(pre_logo_rmif, 0, 0);
		cfg_mc_sub_rdmif(cur_logo_rmif, 1, 0);
		cfg_mc_sub_rdmif(mv_rmif, 2, 0);
		cfg_mc_sub_rdmif(me_logo_rmif, 3, 0);

		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0xf, 3, 4);
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x1, 8, 1);

		mc_out_hbgn = prm_top->prm_cut_win.win_hbgn;
		mc_out_hend = prm_top->prm_cut_win.win_hend;
		mc_out_vbgn = prm_top->prm_cut_win.win_vbgn;
		mc_out_vend = prm_top->prm_cut_win.win_vend;

		w_reg_bit(DPSS_DPE_MC_WIN_CUT_H, mc_out_hbgn, 0, 13);
		w_reg_bit(DPSS_DPE_MC_WIN_CUT_H, mc_out_hend, 16, 13);
		w_reg_bit(DPSS_DPE_MC_WIN_CUT_V, mc_out_vbgn, 0, 13);
		w_reg_bit(DPSS_DPE_MC_WIN_CUT_V, mc_out_vend, 16, 13);
	} else {
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x0, 8, 1);
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x0, 3, 4);
	}
}

void hw_cfg_dpss_mc0(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr, struct DPSS_MC0_TYPE *prm_mc)
{	// cfg @pre_go_field

	u32 src_hsize;	  //= prm_top.org_hsize;
	u32 src_vsize;	  //= prm_top.org_vsize;
	bool cut_win_en;
	//bool mc_cut_position;
	u32  mc_skip_mode;
	u32 win_hsize;
	u32 win_vsize;
	u32 mc_size_sel;
	u32 frm_hsize;
	u32 frm_vsize;
	int i;
	u32 me_dsx;
	u32 me_dsy;
	u32 blk_xscale;
	u32 blk_yscale;
	u32 me_hsize;
	u32 me_vsize;
	u32 me_blk_hsize;
	u32 me_blk_vsize;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;

	cut_win_en	 = prm_top->cut_win_en;
	//mc_cut_position = prm_top->cut_win_position;
	mc_skip_mode = prm_top->mc_skip_mode;
	win_hsize	 = prm_top->prm_cut_win.win_hsize;
	win_vsize	 = prm_top->prm_cut_win.win_vsize;
	mc_size_sel = prm_top->nr_hscale_on || pps->pps_en ||
				prm_top->frc_ds_scale_en || mc_skip_mode;
	frm_hsize = cut_win_en ? win_hsize : mc_size_sel ?
		prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
	frm_vsize = cut_win_en ? win_vsize : mc_size_sel ?
		prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;

	dbg_h2("pps ch=%d,mc_size_sel=%d,pps=%d, frm_hsize=%d\n",
		prm_top->ch, mc_size_sel, pps->pps_en, frm_hsize);

	//if (mc_cut_position && cut_win_en) {
	//	frm_hsize = prm_top->frm_hsize;
	//	frm_vsize = prm_top->frm_vsize;
	//}
	frm_hsize = frm_hsize > 1920 ? (frm_hsize + 15) / 16 * 16 : (frm_hsize + 7) / 8 * 8;
	//frm_hsize = frm_hsize > 1920 ? (frm_hsize + 15) / 16 * 16 : (frm_hsize + 7) / 8 * 8;

	if (prm_mc->pad_en && prm_top->org_hsize != 0xffff) {
		src_hsize = cut_win_en	  ? win_hsize
					: mc_size_sel ? prm_top->dpe_mc_size.src_hsize
								  : prm_top->org_hsize;
		src_vsize = cut_win_en	  ? win_vsize
					: mc_size_sel ? prm_top->dpe_mc_size.src_vsize
								  : prm_top->org_vsize;
	} else {
		src_hsize = cut_win_en	  ? win_hsize
					: mc_size_sel ? prm_top->dpe_mc_size.frm_hsize
								  : prm_top->frm_hsize;
		src_vsize = cut_win_en	  ? win_vsize
					: mc_size_sel ? prm_top->dpe_mc_size.frm_vsize
								  : prm_top->frm_vsize;
	}
	//if (mc_cut_position && cut_win_en) {
	//	src_hsize = prm_top->org_hsize;
	//	src_vsize = prm_top->org_vsize;
	//}

	u32 mvx_div_mode = prm_top->mvx_div_mode;
	u32 mvy_div_mode = prm_top->mvy_div_mode;
	u32 blk_mode	 = 0;
	u32 bbd_en		 = 1;

	if (prm_top->dpss_mode == DPSS_FRC_MODE) {
		me_dsx = prm_top->dae_dsx_scale;
		me_dsy = prm_top->dae_dsy_scale;
	} else {
		me_dsx = prm_top->dpe_dw_dsx;
		me_dsy = prm_top->dpe_dw_dsy;
	}

	blk_xscale = me_dsx + 2;
	blk_yscale = me_dsy + 2;
	me_hsize	 = (frm_hsize + (1 << me_dsx) - 1) >> me_dsx;
	me_vsize	 = (frm_vsize + (1 << me_dsy) - 1) >> me_dsy;
	me_blk_hsize = (me_hsize + 3) >> 2;
	me_blk_vsize = (me_vsize + 3) >> 2;

	if (me_dsx == 2 && me_dsy == 2) {
		blk_mode = 1;	// 16x16, 4k2k
	} else if (me_dsx == 2 && me_dsy == 1) {
		blk_mode = 2;	// 16x8, 4k1k
	} else if (me_dsx == 1 && me_dsy == 1) {
		blk_mode = 0;	// 8x8, 1920x1080
	} else { // no support
		dbg_h2("me_dsx/dsy set fault!!\n");
	}

	if (me_dsx == 2 && prm_top->dlg_stats == 1) {
		frm_hsize = (frm_hsize + 15) & (~0xf);
		prm_mc->pad_hmode = 1;
	} else if (frm_hsize <= 1920 || frm_vsize < 1080) {
		prm_mc->pad_hmode  = 0;
	}

	// enum vid_src_fmt pix_fmt, pix_bit, pix_plan, pix_cmpr;
	enum vid_src_fmt pix_fmt, pix_fmt_src, pix_bit, pix_plan, pix_cmpr;

	if (src_from_nr) {
		pix_fmt = ((prm_top->nro_fs_fmt.src_fmt == YUV420) && prm_top->frc_vfcd_vfmt) ?
					YUV422 : prm_top->nro_fs_fmt.src_fmt;
		pix_fmt_src = prm_top->nro_fs_fmt.src_fmt;
		pix_bit	 = prm_top->nro_fs_fmt.src_bit;
		pix_plan = prm_top->nro_fs_fmt.src_plan;
		pix_cmpr = prm_top->nro_fs_fmt.src_cmpr;
	} else {
		pix_fmt = ((prm_top->src_fs_fmt.src_fmt == YUV420) && prm_top->frc_vfcd_vfmt) ?
					YUV422 : prm_top->src_fs_fmt.src_fmt;
		pix_fmt_src = prm_top->src_fs_fmt.src_fmt;
		pix_bit	 = prm_top->src_fs_fmt.src_bit;
		pix_plan = prm_top->src_fs_fmt.src_plan;
		pix_cmpr = prm_top->src_fs_fmt.src_cmpr;
	}
	dbg_h2("src_from_nr= %d, pix_bit= %d, pad_hmode= %d\n",
			src_from_nr, pix_bit, prm_mc->pad_hmode);
	w_reg_bit(DPSS_DPE_MC_CTRL0, 0, 0, 2);			   // reg_mc_gcb_bypass_mc_en
	w_reg_bit(DPSS_DPE_MC_CTRL0, blk_xscale, 8, 3);	   // reg_mc_blksize_xscale
	w_reg_bit(DPSS_DPE_MC_CTRL0, blk_yscale, 4, 3);	   // reg_mc_blksize_yscale

	w_reg_bit(DPSS_DPE_MC_BLKBAR_X, src_hsize - 1, 16, 13);	// reg_blackbar_xyxy2
	w_reg_bit(DPSS_DPE_MC_BLKBAR_X, 0, 0, 13);		// reg_blackbar_xyxy0
	w_reg_bit(DPSS_DPE_MC_BLKBAR_Y, src_vsize - 1, 16, 13);	// reg_blackbar_xyxy3
	w_reg_bit(DPSS_DPE_MC_BLKBAR_Y, 0, 0, 13);	// reg_blackbar_xyxy1

	//=============================//
	// read dpss mc addr info here

	// uint64_t luma0_baddr = 0;
	// uint64_t luma1_baddr = 0;
	// uint64_t chrm0_baddr = 0;
	// uint64_t chrm1_baddr = 0;
	// u32 logo0_baddr = 0;
	// u32 logo1_baddr = 0;
	// u32 mv_baddr    = 0;
	// u32 melg_baddr  = 0;
	// u32 phase       = 0;
	// u32 gcb_bypass  = 0;

	//============================//
	dbg_h2("debug frm_size= [%d, %d], src_size= [%d, %d]\n",
			frm_hsize, frm_vsize, src_hsize, src_vsize);
	wr(DPSS_DPE_MC_TOP_FSIZE, (frm_hsize << 16) | frm_vsize);
	wr(DPSS_DPE_MC_SRC_FSIZE, (src_hsize << 16) | src_vsize);

	// wr(DPSS_DPE_MC_DIN_LUMA0_BADDR0, luma0_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_LUMA0_BADDR1,(luma0_baddr>>32)&0xffff);
	// wr(DPSS_DPE_MC_DIN_LUMA1_BADDR0, luma1_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_LUMA1_BADDR1,(luma1_baddr>>32)&0xffff);
	//
	// wr(DPSS_DPE_MC_DIN_CHRM0_BADDR0, chrm0_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_CHRM0_BADDR1,(chrm0_baddr>>32)&0xffff);
	// wr(DPSS_DPE_MC_DIN_CHRM1_BADDR0, chrm1_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_CHRM1_BADDR1,(chrm1_baddr>>32)&0xffff);

	// wr(DPSS_DPE_MC_DIN_LOGO_BADDR0, logo0_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_LOGO_BADDR1, logo1_baddr&0xffff);
	// wr(DPSS_DPE_MC_DIN_MELG_BADDR1, melg_baddr &0xffff);
	// wr(DPSS_DPE_MC_DIN_MV_BADDR1  , melg_baddr &0xffff);

	w_reg_bit(DPSS_DPE_MC_CTRL0, blk_mode & 0x3, 28, 2);
	// w_reg_bit(DPSS_DPE_MC_CTRL0, phase&0xff, 20, 8);
	//w_reg_bit(DPSS_DPE_MC_CTRL0, mvx_div_mode & 0x3, 16, 2);
	//w_reg_bit(DPSS_DPE_MC_CTRL0, mvy_div_mode & 0x3, 12, 2);
	w_reg_bit(DPSS_DPE_MC_CTRL0, 2, 16, 2);
	w_reg_bit(DPSS_DPE_MC_CTRL0, 2, 12, 2);
	w_reg_bit(DPSS_DPE_MC_CTRL0, blk_xscale & 0x7, 8, 3);
	w_reg_bit(DPSS_DPE_MC_CTRL0, blk_yscale & 0x7, 4, 3);
	// w_reg_bit(DPSS_DPE_MC_CTRL0, gcb_bypass&0x3, 0, 2);

	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 0,
				1);	  // reg_mode bit0, ctrl mc_top signals like baddr/phase
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0xff, 20, 8);   // rdmif_enable

	if (pix_fmt == YUV422)
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 16, 2);
	else if (pix_fmt == YUV420)
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 2, 16, 2);
	else
		dbg_h2("ERROR: MC CONFIG WRONG FMT\n");

	// config_mc_slice
	hw_cfg_dpss_mc_slice(frm_hsize, frm_vsize, 1);	 // slc_num);
	hw_cfg_dpss_mc_bbd(frm_hsize, frm_vsize, bbd_en, 0x0, src_hsize, src_vsize);

	//
	if (me_dsx == 2) {
		w_reg_bit(FRC_MC_SETTING2, 9, 8, 6);	  // reg_mc_fetch_size
		w_reg_bit(FRC_MC_SETTING2, 16, 0, 8);	  // reg_mc_blk_x
	} else {
		w_reg_bit(FRC_MC_SETTING2, 5, 8, 6);	 // reg_mc_fetch_size
		w_reg_bit(FRC_MC_SETTING2, 8, 0, 8);	 // reg_mc_blk_x
	}
	w_reg_bit(FRC_MC_SETTING1, 0, 24, 1);		  // reg_mc_bb_inner_en
	w_reg_bit(FRC_MC_SETTING1, me_dsx, 8, 4);	  // reg_mc_mvx_scale
	w_reg_bit(FRC_MC_SETTING1, me_dsy, 0, 4);	  // reg_mc_mvy_scale

	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, 0, 16,
				16);   // reg_mc_org_me_blk_bb_xyxy_0
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, 0, 0, 16);// reg_mc_org_me_blk_bb_xyxy_1
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (src_hsize >> me_dsx) - 1, 16,
				16);   // reg_mc_org_me_blk_bb_xyxy_2  //org me size
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (src_vsize >> me_dsy) - 1, 0,
				16);   // reg_mc_org_me_blk_bb_xyxy_3   //org me size
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 16,
				16);   // reg_mc_org_me_blk_bb_xyxy_0
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 0,
				16);   // reg_mc_org_me_blk_bb_xyxy_1
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, ((src_hsize >> me_dsx) >> 2) - 1,
				16, 16);   // reg_mc_org_me_blk_bb_xyxy_2 //org blk size
	w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, ((src_vsize >> me_dsy) >> 2) - 1,
				0, 16);	  // reg_mc_org_me_blk_bb_xyxy_3  //org blk size
	w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 16, 16);// reg_mc_me_blk_bb_xyxy_0
	w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 0, 16);// reg_mc_me_blk_bb_xyxy_1
	w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_hsize - 1, 16,
				16);   // reg_mc_me_blk_bb_xyxy_2
	w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_vsize - 1, 0,
				16);   // reg_mc_me_blk_bb_xyxy_3

	w_reg_bit(FRC_MC_WRAP_BB_0, 0, 16, 13);				// reg_blackbar_xyxy0
	w_reg_bit(FRC_MC_WRAP_BB_0, 0, 0, 13);				// reg_blackbar_xyxy1
	w_reg_bit(FRC_MC_WRAP_BB_1, src_hsize - 1, 16, 13);	// reg_blackbar_xyxy2
	w_reg_bit(FRC_MC_WRAP_BB_1, src_vsize - 1, 0, 13);	// reg_blackbar_xyxy3

	w_reg_bit(FRC_NOW_SRCH_REG, 64, 8, 8);   // reg_mc_vsrch_rng_luma //todo
	w_reg_bit(FRC_NOW_SRCH_REG, 32, 0, 8);   // reg_mc_vsrch_rng_chrm

	w_reg_bit(FRC_MC_FORCE_BV, 0, 0, 1);	   // reg_mc_force_bv_en
	w_reg_bit(FRC_MC_FORCE_BV, 0, 16, 13);   // reg_mc_force_mvx
	w_reg_bit(FRC_MC_FORCE_BV, 0, 4, 12);	   // reg_mc_force_mvy

	w_reg_bit(FRC_SRCH_RNG_MODE, prm_top->mc_setting.chrm_srng_mode, 0, 4);
	w_reg_bit(FRC_SRCH_RNG_MODE, prm_top->mc_setting.luma_srng_mode, 4, 4);
	if (pix_fmt_src == YUV420 && pix_cmpr == CMPR_AFBC) {			// add afbc
		if (prm_top->mc_setting.chrm_srng_mode < SRNG_NORMAL_V2) { // ||
			 //prm_top.mc_setting.luma_srng_mode < SRNG_NORMAL_V2)
			// dbg_h2("ERROR: mc srng mode set error\n");
			dbg_h2("Just force to set SRNG_NORMAL_V2\n");
			w_reg_bit(FRC_SRCH_RNG_MODE, SRNG_NORMAL_V2, 0, 4);
			//            w_reg_bit(FRC_SRCH_RNG_MODE, SRNG_NORMAL_V2, 4, 4);
		}
		w_reg_bit(FRC_MC_H2V2_SETTING, 0, 31,
					1);
		w_reg_bit(FRC_MC_H2V2_SETTING, 1, 24, 1);	  // reg_mc_srch_rng_chrm_scale_en
		w_reg_bit(FRC_MC_H2V2_SETTING, 1, 19, 1);	  // reg_mc_srch_rng_chrm_blw_yscale
		w_reg_bit(FRC_MC_H2V2_SETTING, 1, 21, 1);	  // reg_mc_srch_rng_chrm_abv_yscale
		// setting search range
		for (i = 0; i < 18; i = i + 1) {
			if (i % 2 == 0) {
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -8, 0,
							9);	  // pre_chrm	//ary addr change *4
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 16,
							9);	  // pre_luma	//ary addr change *4
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 8, 0,
							9);	  // cur_chrm	//ary addr change *4
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 16,
							9);	  // cur_luma	//ary addr change *4
			} else {
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -4, 0, 9);
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -8, 16, 9);
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 4, 0, 9);
				w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 8, 16, 9);
			}
		}
	}
	if (pix_fmt_src == YUV422 && pix_cmpr == CMPR_AFBC) {	// add afbc
		// setting search range
		for (i = 0; i < 18; i = i + 1) {
			w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 0,
						9);	  // pre_chrm	//ary addr change *4
			w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 16,
						9);	  // pre_luma	//ary addr change *4
			w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 0,
						9);	  // cur_chrm	//ary addr change *4
			w_reg_bit(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 16,
						9);	  // cur_luma		//ary addr change *4
		}
	}

	// DPSS_MC0_t mc_info;

	prm_mc->frm_hsize	 = frm_hsize;
	prm_mc->frm_vsize	 = frm_vsize;
	prm_mc->src_hsize	 = src_hsize;
	prm_mc->src_vsize	 = src_vsize;
	prm_mc->mvx_div_mode = mvx_div_mode;
	prm_mc->mvy_div_mode = mvy_div_mode;
	// prm_mc->luma0_baddr  = luma0_baddr;
	// prm_mc->luma1_baddr  = luma1_baddr;
	// prm_mc->chrm0_baddr  = chrm0_baddr;
	// prm_mc->chrm1_baddr  = chrm1_baddr;
	// prm_mc->logo0_baddr  = logo0_baddr;
	// prm_mc->logo1_baddr  = logo1_baddr;
	// prm_mc->mv_baddr     = mv_baddr;
	// prm_mc->melg_baddr   = melg_baddr;
	// prm_mc->phase        = phase;
	prm_mc->pix_fmt	 = pix_fmt_src;
	prm_mc->pix_bit	 = pix_bit;
	prm_mc->pix_plan = pix_plan;
	prm_mc->src_cmpr = pix_cmpr;

	prm_mc->comp_setting.mmu_mode_en	  = prm_top->comp_setting.mmu_mode_en;
	prm_mc->comp_setting.mmu_page_mode	  = prm_top->comp_setting.mmu_page_mode;
	prm_mc->comp_setting.afrc_head_mode	  = prm_top->comp_setting.afrc_head_mode;
	prm_mc->comp_setting.afrc_target_byte_y = prm_top->comp_setting.afrc_target_byte_y;
	prm_mc->comp_setting.afrc_target_byte_c = prm_top->comp_setting.afrc_target_byte_c;

	//==============================================================
	// if DPE pps enable, pps wrmif use di waddr, MC need use di output
	//==============================================================
	// reg_aa_pps_mode == 3 -> nr aapps 4k1k
	bool nr_aapps_4k1k_en;// = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	if (pps->pps_en)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_4k1k_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	if (prm_top->nr_aapps_up ||
		(nr_aapps_4k1k_en && prm_top->dolby_mode != DOLBY_DPSS_PRL_MODE))
		w_reg_bit(DPSS_FBUF_TOP_CTRL, 1, 3, 1);	// dpe_pps_en
	else
		w_reg_bit(DPSS_FBUF_TOP_CTRL, 0, 3, 1);
}

void frc_me_cut(struct PRM_DPSS_TOP *prm_top)
{
	u32 x_scl = prm_top->dpe_dw_dsx;
	u32 y_scl = prm_top->dpe_dw_dsy;
	u32 pre_logo_rmif_bits = 1;
	u32 cur_logo_rmif_bits = 1;
	u32 mv_rmif_bits = 64;
	u32 me_logo_rmif_bits  = 1;

	u32 frm_hsize;
	u32 pre_logo_hsize;
	u32 cur_logo_hsize;
	u32 mv_hsize;
	u32 me_logo_hsize;
	u32 mc_out_hbgn = 0;
	u32 mc_out_hend = 0;
	u32 mc_out_vbgn = 0;
	u32 mc_out_vend = 0;
	u32 tmp_reg_value;
	struct PRM_INTF_TYPE *pre_logo_rmif;
	struct PRM_INTF_TYPE *cur_logo_rmif;
	struct PRM_INTF_TYPE *mv_rmif;
	struct PRM_INTF_TYPE *me_logo_rmif;
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;
	bool cut_win_en;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	prm_mc = &pchip->prm_mc;
	pre_logo_rmif = &pchip->pre_logo_rmif;
	cur_logo_rmif = &pchip->cur_logo_rmif;
	mv_rmif = &pchip->mv_rmif;
	me_logo_rmif = &pchip->me_logo_rmif;

	frm_hsize = pps->pps_en ? prm_mc->frm_hsize : prm_top->frm_hsize;
	pre_logo_hsize = frm_hsize >> x_scl;
	cur_logo_hsize = frm_hsize >> x_scl;
	mv_hsize	   = pre_logo_hsize >> 2;
	me_logo_hsize  = pre_logo_hsize >> 2;
	cut_win_en = prm_top->cut_win_en;

	memset((void *)(pre_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(cur_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(mv_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(me_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));

	pre_logo_rmif->burst_len = 2;
	cur_logo_rmif->burst_len = 2;
	mv_rmif->burst_len		= 2;
	me_logo_rmif->burst_len	= 2;
	if (cut_win_en) {
		pre_logo_rmif->bits_mode = 12;
		cur_logo_rmif->bits_mode = 12;
		mv_rmif->bits_mode		= 4;
		me_logo_rmif->bits_mode	= 12;

		pre_logo_rmif->pack_mode = 4;
		cur_logo_rmif->pack_mode = 4;
		mv_rmif->pack_mode		= 4;
		me_logo_rmif->pack_mode	= 0;

		pre_logo_rmif->slc_x_st[0] = prm_top->prm_cut_win.win_hbgn_align >> x_scl;
		pre_logo_rmif->slc_x_ed[0] = prm_top->prm_cut_win.win_hend_align >> x_scl;
		pre_logo_rmif->slc_y_st[0] = prm_top->prm_cut_win.win_vbgn_align >> y_scl;
		pre_logo_rmif->slc_y_ed[0] = prm_top->prm_cut_win.win_vend_align >> y_scl;

		cur_logo_rmif->slc_x_st[0] = prm_top->prm_cut_win.win_hbgn_align >> x_scl;
		cur_logo_rmif->slc_x_ed[0] = prm_top->prm_cut_win.win_hend_align >> x_scl;
		cur_logo_rmif->slc_y_st[0] = prm_top->prm_cut_win.win_vbgn_align >> y_scl;
		cur_logo_rmif->slc_y_ed[0] = prm_top->prm_cut_win.win_vend_align >> y_scl;

		mv_rmif->slc_x_st[0] = pre_logo_rmif->slc_x_st[0] >> 2;
		mv_rmif->slc_x_ed[0] = pre_logo_rmif->slc_x_ed[0] >> 2;
		mv_rmif->slc_y_st[0] = pre_logo_rmif->slc_y_st[0] >> 2;
		mv_rmif->slc_y_ed[0] = pre_logo_rmif->slc_y_ed[0] >> 2;

		me_logo_rmif->slc_x_st[0] = pre_logo_rmif->slc_x_st[0] >> 2;
		me_logo_rmif->slc_x_ed[0] = pre_logo_rmif->slc_x_ed[0] >> 2;
		me_logo_rmif->slc_y_st[0] = pre_logo_rmif->slc_y_st[0] >> 2;
		me_logo_rmif->slc_y_ed[0] = pre_logo_rmif->slc_y_ed[0] >> 2;

		pre_logo_rmif->stride = (((pre_logo_hsize * pre_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;
		cur_logo_rmif->stride = (((cur_logo_hsize * cur_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;
		mv_rmif->stride = (((mv_hsize * mv_rmif_bits + 127) >> 7) + 3) >> 2 << 2;
		me_logo_rmif->stride = (((me_logo_hsize * me_logo_rmif_bits + 127) >>
						7) + 3) >> 2 << 2;

		hw_cfg_mc_sub_rdmif(pre_logo_rmif, 0, 0);
		hw_cfg_mc_sub_rdmif(cur_logo_rmif, 1, 0);
		hw_cfg_mc_sub_rdmif(mv_rmif, 2, 0);
		hw_cfg_mc_sub_rdmif(me_logo_rmif, 3, 0);

		//w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0xf, 3, 4);
		//w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x1, 8, 1);
		tmp_reg_value = rd(DPSS_DPE_MC_MIF_CTRL0);
		tmp_reg_value &= 0xfffffe87;
		tmp_reg_value |= 0x178;
		DPSS_RDMA_WR_VS(DPSS_DPE_MC_MIF_CTRL0, tmp_reg_value);

		mc_out_hbgn = prm_top->prm_cut_win.win_hbgn;
		mc_out_hend = prm_top->prm_cut_win.win_hend;
		mc_out_vbgn = prm_top->prm_cut_win.win_vbgn;
		mc_out_vend = prm_top->prm_cut_win.win_vend;
		//w_reg_bit(DPSS_DPE_MC_WIN_CUT_H, mc_out_hbgn, 0, 13);
		//w_reg_bit(DPSS_DPE_MC_WIN_CUT_H, mc_out_hend, 16, 13);
		//w_reg_bit(DPSS_DPE_MC_WIN_CUT_V, mc_out_vbgn, 0, 13);
		//w_reg_bit(DPSS_DPE_MC_WIN_CUT_V, mc_out_vend, 16, 13);
		tmp_reg_value = mc_out_hbgn | (mc_out_hend << 16);
		DPSS_RDMA_WR_VS(DPSS_DPE_MC_WIN_CUT_H, tmp_reg_value);
		tmp_reg_value = mc_out_vbgn | (mc_out_vend << 16);
		DPSS_RDMA_WR_VS(DPSS_DPE_MC_WIN_CUT_V, tmp_reg_value);
	} else {
		tmp_reg_value = rd(DPSS_DPE_MC_MIF_CTRL0);
		tmp_reg_value &= 0xfffffe87;
		tmp_reg_value |= 0x01;
		DPSS_RDMA_WR_VS(DPSS_DPE_MC_MIF_CTRL0, tmp_reg_value);
	}
}

void hw_cfg_dpss_mc_pre_triggle(void)
{	// cfg @go_filed
	// w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 0, 1);
	// w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 29, 1);
#ifdef USE_FRC_PRE_VS_RDMA
	if (used_rdma)
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_START_CTRL, 0x40000042);
	else
		w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 30, 1);
#else
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 30, 1);
#endif
}

void hw_cfg_dpss_mc_triggle(void)
{	// cfg @go_filed
	// w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 0, 1);
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 31, 1);
}

void hw_cfg_dpss_mc_slice(u32 frm_hsize, u32 frm_vsize, u32 slice_num)
{
	int i;
	u32 mc_slice_h_overlap = 32;
	u32 mc_slice_lpad[4]   = {0, 0, 0, 0};
	u32 mc_slice_rpad[4]   = {0, 0, 0, 0};

	u32 mc_slice_x_st[4]  = {0, 0, 0, 0};
	u32 mc_slice_x_end[4] = {frm_hsize - 1, frm_hsize - 1, frm_hsize - 1, frm_hsize - 1};

	for (i = 0; i < slice_num; i++) {
		mc_slice_lpad[i] = i == 0 ? 0 : mc_slice_h_overlap;
		mc_slice_rpad[i] = i == slice_num ? 0 : mc_slice_h_overlap;
	}

	if (slice_num == 4) {
		mc_slice_x_st[0]  = 0;
		mc_slice_x_end[0] = frm_hsize / 4 + mc_slice_h_overlap - 1;
		mc_slice_x_st[1]  = frm_hsize / 4 - mc_slice_h_overlap;
		mc_slice_x_end[1] = frm_hsize / 2 + mc_slice_h_overlap - 1;
		mc_slice_x_st[2]  = frm_hsize / 2 - mc_slice_h_overlap;
		mc_slice_x_end[2] = frm_hsize * 3 / 4 + mc_slice_h_overlap - 1;
		mc_slice_x_st[3]  = frm_hsize * 3 / 4 - mc_slice_h_overlap;
		mc_slice_x_end[3] = frm_hsize - 1;
	} else if (slice_num == 2) {
		mc_slice_x_st[0]  = 0;
		mc_slice_x_end[0] = frm_hsize / 2 + mc_slice_h_overlap - 1;
		mc_slice_x_st[1]  = frm_hsize / 2 - mc_slice_h_overlap;
		mc_slice_x_end[1] = frm_hsize - 1;
		mc_slice_x_st[2]  = 0;
		mc_slice_x_end[2] = 0;
		mc_slice_x_st[3]  = 0;
		mc_slice_x_end[3] = 0;
	} else {
		mc_slice_x_st[0]  = 0;
		mc_slice_x_end[0] = frm_hsize - 1;
		mc_slice_x_st[1]  = 0;
		mc_slice_x_end[1] = 0;
		mc_slice_x_st[2]  = 0;
		mc_slice_x_end[2] = 0;
		mc_slice_x_st[3]  = 0;
		mc_slice_x_end[3] = 0;
	}

	w_reg_bit(FRC_MC_SLC_CTRL1, mc_slice_lpad[0], 24, 8);	   // reg_slc0_lpad
	w_reg_bit(FRC_MC_SLC_CTRL1, mc_slice_rpad[0], 16, 8);	   // reg_slc0_rpad
	w_reg_bit(FRC_MC_SLC_CTRL1, mc_slice_lpad[1], 8, 8);	   // reg_slc1_lpad
	w_reg_bit(FRC_MC_SLC_CTRL1, mc_slice_rpad[1], 0, 8);	   // reg_slc1_rpad
	w_reg_bit(FRC_MC_SLC_CTRL2, mc_slice_lpad[2], 24, 8);	   // reg_slc2_lpad
	w_reg_bit(FRC_MC_SLC_CTRL2, mc_slice_rpad[2], 16, 8);	   // reg_slc2_rpad
	w_reg_bit(FRC_MC_SLC_CTRL2, mc_slice_lpad[3], 8, 8);	   // reg_slc3_lpad
	w_reg_bit(FRC_MC_SLC_CTRL2, mc_slice_rpad[3], 0, 8);	   // reg_slc3_rpad
	w_reg_bit(FRC_MC_SLC_CTRL3, mc_slice_x_st[0], 16, 16);   // reg_slc0_xbgn
	w_reg_bit(FRC_MC_SLC_CTRL3, mc_slice_x_end[0], 0, 16);   // reg_slc0_xend
	w_reg_bit(FRC_MC_SLC_CTRL4, mc_slice_x_st[1], 16, 16);   // reg_slc1_xbgn
	w_reg_bit(FRC_MC_SLC_CTRL4, mc_slice_x_end[1], 0, 16);   // reg_slc1_xend
	w_reg_bit(FRC_MC_SLC_CTRL5, mc_slice_x_st[2], 16, 16);   // reg_slc2_xbgn
	w_reg_bit(FRC_MC_SLC_CTRL5, mc_slice_x_end[2], 0, 16);   // reg_slc2_xend
	w_reg_bit(FRC_MC_SLC_CTRL6, mc_slice_x_st[3], 16, 16);   // reg_slc3_xbgn
	w_reg_bit(FRC_MC_SLC_CTRL6, mc_slice_x_end[3], 0, 16);   // reg_slc3_xend

	w_reg_bit(FRC_MC_SLC_CTRL0, (slice_num > 1), 0, 1);	// reg_slc_mode
	// w_reg_bit(FRC_MC_SLC_CTRL0,0,1,1); //reg_prephs_en_sel (todo?)
	w_reg_bit(FRC_MC_SLC_CTRL0, slice_num - 1, 2, 2);	  // reg_slc_num
	// w_reg_bit(FRC_MC_SLC_CTRL0,mc_proc_alternate_mode,4,2); //reg_mc_intp_lnum
}

void hw_cfg_dpss_mc_bbd(u32 frm_hsize, u32 frm_vsize, u32 bbd_en, u32 bbd_sel, u32 src_hsize,
						u32 src_vsize)
{
	w_reg_bit(FRC_BBD_CTRL, bbd_en, 16, 1);	// reg_bb_bbd_spatial_enable
	w_reg_bit(FRC_BBD_CTRL, bbd_en, 17, 1);	// reg_bb_detail_en
	w_reg_bit(FRC_BBD_DETECT_REGION_TOP2BOT, src_vsize - 1, 0, 16);	// reg_bb_det_bot
	w_reg_bit(FRC_BBD_DETECT_REGION_LFT2RIT, src_hsize - 1, 0, 16);	// reg_bb_det_rit
	w_reg_bit(FRC_BBD_DETECT_DETAIL_TOP2BOT, src_vsize - 1, 0, 16);	// reg_bb_det_detail_bot
	w_reg_bit(FRC_BBD_DETECT_DETAIL_LFT2RIT, src_hsize - 1, 0, 16);	// reg_bb_det_detail_rit
	w_reg_bit(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, src_hsize - 1, 16, 16);// reg_bb_oob_detail_xyxy_2
	w_reg_bit(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT, src_vsize - 1, 0, 16);// reg_bb_oob_detail_xyxy_3
	w_reg_bit(FRC_BBD_VER_TH, 8 * frm_hsize / 1920, 16, 16);// reg_bb_ver_th1
	w_reg_bit(FRC_BBD_VER_TH, 560 * frm_hsize / 1920, 0, 16);// reg_bb_ver_th2
	w_reg_bit(FRC_BBD_FINER_HOR_TH, 200 * frm_vsize / 1080, 16, 16);// reg_bb_hor_th1
	w_reg_bit(FRC_BBD_FINER_HOR_TH, 400 * frm_vsize / 1080, 0, 16);// reg_bb_hor_th2
	w_reg_bit(FRC_BBD_TOP_BOT_EDGE_TH, 154 * frm_hsize / 1920, 16,
				12);   // reg_bb_top_bot_edge_th1
	w_reg_bit(FRC_BBD_TOP_BOT_EDGE_TH, 256 * frm_hsize / 1920, 0, 12);// reg_bb_top_bot_edge_th2
	w_reg_bit(FRC_BBD_LFT_RIT_EDGE_TH, 87 * frm_vsize / 1080, 16, 12);// reg_bb_lft_rit_edge_th1
	w_reg_bit(FRC_BBD_LFT_RIT_EDGE_TH, 144 * frm_vsize / 1080, 0, 12);// reg_bb_lft_rit_edge_th2
	w_reg_bit(FRC_BBD_TOP_BOT_EDGE_FIRST_TH, 154 * frm_hsize / 1920, 16,
				12);   // reg_bb_top_bot_edge_first_th
	w_reg_bit(FRC_BBD_TOP_BOT_EDGE_FIRST_TH, 87 * frm_vsize / 1080, 0,
				12);   // reg_bb_lft_rit_edge_first_th
	w_reg_bit(FRC_BBD_TOP_EDGE_DET_STT_END, 405 * frm_vsize / 1080, 0,
				16);   // reg_bb_top_edge_det_end
	w_reg_bit(FRC_BBD_BOT_EDGE_DET_STT_END, 674 * src_vsize / 1080, 16,
				16);
	w_reg_bit(FRC_BBD_BOT_EDGE_DET_STT_END, frm_vsize - 1, 0, 16);   // reg_bb_bot_edge_det_end
	w_reg_bit(FRC_BBD_LFT_EDGE_DET_STT_END, 720 * frm_hsize / 1920, 0,
				16);   // reg_bb_lft_edge_det_end
	w_reg_bit(FRC_BBD_RIT_EDGE_DET_STT_END, 1199 * frm_hsize / 1920, 16,
				16);
	w_reg_bit(FRC_BBD_RIT_EDGE_DET_STT_END, src_hsize - 1, 0, 16);   // reg_bb_rit_edge_det_end
	w_reg_bit(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, src_hsize - 1, 16, 16);// reg_bb_oob_apl_xyxy_2
	w_reg_bit(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE, src_vsize - 1, 0, 16);// reg_bb_oob_apl_xyxy_3
	w_reg_bit(FRC_BBD_APL_HIST_WIN_RIT_BOT, src_hsize - 1, 16, 16);	// reg_bb_apl_hist_xyxy_2
	w_reg_bit(FRC_BBD_APL_HIST_WIN_RIT_BOT, src_vsize - 1, 0, 16);	// reg_bb_apl_hist_xyxy_3
	w_reg_bit(FRC_BBD_EDGE_SETTING, 0, 16, 2);   // reg_bb_edge2_detect_range_mode
	w_reg_bit(FRC_BBD_EDGE_SETTING, 0, 18, 2);   // reg_bb_edge1_detect_range_mode
	// cfg bbd sel, 0-mc.mif0;1-mc.mif1,2-nr.mif
	w_reg_bit(DPSS_DPE_BBD_CTRL, bbd_sel, 0, 2);	 // reg_bb_edge1_detect_range_mode
	// cfg bbd source idx, src0 or src1
	u32 reg_fnr_src_idx = (rd(DPSS_TOP_FUNC_CTRL) >> 24) & 0xF;

	w_reg_bit(DPSS_BBD_ONLY_CTRL, reg_fnr_src_idx, 4, 4);	  // bbd_src_idx
	w_reg_bit(FRC_BBD_MISC, 10, 4, 4);
}

bool hw_dpss_dpe_info_cfg(struct PRM_DPSS_TOP *prm_top, bool obuf_rdy)
{
	u32 data32;
	u32 data32_1;
	u32 dpe_pre_pixl_buf;
	u32 dpe_cur_pixl_buf;
	u32 dpe_plogo_buf;
	u32 dpe_clogo_buf;
	u32 dpe_intp_phs;
	u32 dpe_mvlogo_buf;
	u32 dpe_mv_cur_frm_idx;
	u32 dpe_image_cur_frm_idx;
	u32 dpe_image_pre_frm_idx;

	u32 mc_inp_body_ybuf_addr[2];
	u32 mc_inp_body_cbuf_addr[2];
	u32 mc_inp_head_ybuf_addr;
	u32 mc_inp_head_cbuf_addr;

	u32 mc_inp_head_ybuf_step;
	u32 mc_inp_head_cbuf_step;

	u32 mc_pix_logo_buf;
	u32 mc_blk_logo_buf;
	u32 mc_vpmc_mv_buf;

	u32 mc_pix_logo_step;
	u32 mc_blk_logo_step;
	u32 mc_vpmc_mv_step;
	u32 game_mode_ntm;

	u32 mc_luma0_rdmif_baddr0;
	u32 mc_luma0_rdmif_baddr1;
	u32 mc_luma1_rdmif_baddr0;
	u32 mc_luma1_rdmif_baddr1;

	u32 mc_chrm0_rdmif_baddr0;
	u32 mc_chrm0_rdmif_baddr1;
	u32 mc_chrm1_rdmif_baddr0;
	u32 mc_chrm1_rdmif_baddr1;

	u32 ip_logo0_rdmif_baddr;
	u32 ip_logo1_rdmif_baddr;
	u32 dpe_melogo_baddr;
	u32 dpe_vpmc_mv_baddr;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	bool is_pause_state = false;
	u8 display_mc_idx;
	u8 tmp_idx = 0;

	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct display_buffer_info_s *display_buf_info;

	if (!pchip_st)
		return false;

	state_st = &pchip_st->state_st;
	if (state_st->mc_disp_st.step == 0) {
		state_st->mc_disp_st.step = 1;
	} else {
		state_st->mc_disp_st.disp_pre_idx = state_st->mc_disp_st.wr_pre_idx;
		state_st->mc_disp_st.disp_cur_idx = state_st->mc_disp_st.wr_cur_idx;
	}

	if (!obuf_rdy &&
		(state_st->force_disable_check_fallback || state_st->check_fallback != 1)) {
		if (!state_st->enable_last_drop)
			return false;
		is_pause_state = true;
	}

	data32		  = rd(DPSS_FRC_MC_IUFF_STATUS);
	game_mode_ntm = (rd(FRC_DPSS_LLM) >> 2) & 0x1;

	data32 = display_buf_q.mc_idx;
	if (data32 >= DPSS_QUEEN_NUM || data32 == display_buf_q.inp_idx) {
		pr_frc(2, "display_buf_q: mc_idx=%d, inp_idx=%d, enable_drop=%d\n",
				data32, display_buf_q.inp_idx, state_st->enable_last_drop);
		if (!state_st->enable_last_drop &&
			(state_st->force_disable_check_fallback || state_st->check_fallback != 1))
			return false;
		display_queue_use_last(&display_buf_q, 2);
		data32 = display_buf_q.mc_idx;
	}
	if (data32 < DPSS_QUEEN_NUM) {
		display_buf_info = &display_buf_q.data[data32];
	} else {
		pr_frc(2, "data32 (%d) > DPSS_QUEEN_NUM, reset to 0\n", data32);
		display_buf_info = &display_buf_q.data[0];
	}

	if (state_st->check_fallback == 2 && display_buf_info->p == state_st->force_mc_cur_idx)
		state_st->check_fallback = 3;

	if (display_buf_info->dae_mix) {
		data32 = (display_buf_q.mc_idx + 1) % DPSS_QUEEN_NUM;
		if (data32 == display_buf_q.inp_idx) {
			pr_frc(2, "can not do dae mix:mc_idx=%d, inp_idx=%d\n",
				display_buf_q.mc_idx, display_buf_q.inp_idx);
			return false;
		}
		display_buf_q.mc_idx = (display_buf_q.mc_idx + 1) % DPSS_QUEEN_NUM;
		if (state_st->dpe_ready && display_buf_q.disp_idx != display_buf_q.mc_idx)
			display_buf_q.disp_idx = (display_buf_q.disp_idx + 1) % DPSS_QUEEN_NUM;
		display_buf_info = &display_buf_q.data[data32];
	}
	if (state_st->dpe_mix) {
		data32 = (display_buf_q.mc_idx + 1) % DPSS_QUEEN_NUM;
		state_st->dpe_mix = false;
		if (data32 == display_buf_q.inp_idx) {
			pr_frc(2, "can not do dpe mix:mc_idx=%d, inp_idx=%d\n",
				display_buf_q.mc_idx, display_buf_q.inp_idx);
			return false;
		}
		display_buf_q.mc_idx = (display_buf_q.mc_idx + 1) % DPSS_QUEEN_NUM;
		if (state_st->dpe_ready && display_buf_q.disp_idx != display_buf_q.mc_idx)
			display_buf_q.disp_idx = (display_buf_q.disp_idx + 1) % DPSS_QUEEN_NUM;
		display_buf_info = &display_buf_q.data[data32];
	}
	display_mc_idx = display_buf_q.mc_idx;
	dpe_pre_pixl_buf = display_buf_info->p;
	dpe_cur_pixl_buf = display_buf_info->c;
	dpe_plogo_buf    = display_buf_info->logo_pre;
	dpe_clogo_buf    = display_buf_info->logo_cur;
	dpe_intp_phs     = display_buf_info->mc_phase;
	dpe_mvlogo_buf   = display_buf_info->mv_buf;

	dpe_mv_cur_frm_idx	  = dpe_mvlogo_buf;
	dpe_image_cur_frm_idx = dpe_cur_pixl_buf;
	dpe_image_pre_frm_idx = dpe_pre_pixl_buf;

	display_buf_q.mc_idx = (display_buf_q.mc_idx + 1) % DPSS_QUEEN_NUM;
	if (state_st->mc_last_ready && display_buf_q.disp_idx != display_buf_q.mc_idx)
		display_buf_q.disp_idx = (display_buf_q.disp_idx + 1) % DPSS_QUEEN_NUM;
	if (display_buf_q.disp_idx == display_buf_q.mc_idx)
		state_st->mc_last_ready = false;

	pr_frc(1, "is_pause_state=%d, %d\n", is_pause_state, state_st->is_pause_state_last_frmae);

	if (is_pause_state) {
		tmp_idx = display_buf_q.inp_idx == 0 ?
					DPSS_QUEEN_NUM - 1 : display_buf_q.inp_idx - 1;
		dpe_pre_pixl_buf = display_buf_q.data[tmp_idx].n;
		dpe_cur_pixl_buf = dpe_pre_pixl_buf;
		if (state_st->is_pause_state_last_frmae &&
				state_st->mc_last_idx == dpe_pre_pixl_buf)
			return false;
		state_st->mc_last_idx = dpe_pre_pixl_buf;
		dpe_intp_phs = 0;
		state_st->is_pause_state_last_frmae = true;
	} else if (state_st->is_pause_state_last_frmae && state_st->mc_last_idx != 0xff &&
		dpe_pre_pixl_buf != state_st->mc_last_idx) {
		dpe_pre_pixl_buf = state_st->mc_last_idx;
		dpe_cur_pixl_buf = state_st->mc_last_idx;
		dpe_intp_phs = 0;
	} else {
		state_st->is_pause_state_last_frmae = false;
	}

	if (!state_st->force_disable_check_fallback && state_st->check_fallback == 1) {
		dpe_cur_pixl_buf = state_st->force_mc_cur_idx;
		pr_frc(1, "need force mc cur id=%d\n", dpe_cur_pixl_buf);
		state_st->check_fallback = 2;
	} else if (state_st->check_fallback == 2) {
		if (dpe_pre_pixl_buf != state_st->force_mc_cur_idx)
			dpe_cur_pixl_buf = state_st->force_mc_cur_idx;
		else
			state_st->check_fallback = 3;
	} else if (state_st->force_disable_check_fallback) {
		state_st->check_fallback = 0;
	}

	state_st->dpe_ready = true;
	if (display_buf_q.disp_idx != display_buf_q.mc_idx)
		state_st->mc_last_ready = true;
	if (state_st->fast_pat_set_phase0 || state_st->mc_bypass || state_st->force_mc_phase0)
		dpe_intp_phs = 0;
	state_st->dpe_intp_phs = dpe_intp_phs;

	state_st->mc_disp_st.wr_pre_idx = dpe_pre_pixl_buf;
	state_st->mc_disp_st.wr_cur_idx = dpe_cur_pixl_buf;
	mc_res_switch_begin();
	if (state_st->need_force_mc_pre) {
		if (state_st->force_mc_pre_idx == dpe_pre_pixl_buf)
			state_st->need_force_mc_pre = 0;
		else
			dpe_pre_pixl_buf = state_st->force_mc_pre_idx;
	}

	pr_frc(1, "display_buf_q:drop_idx=%d, disp_idx=%d, mc_idx=%d, inp_idx=%d\n",
		display_buf_q.drop_idx, display_buf_q.disp_idx,
		display_buf_q.mc_idx, display_buf_q.inp_idx);
	pr_frc(1, "mc_last_idx=%d, display_mc_idx=%d, dpe_pre=%d, dpe_cur=%d, pause_st_last=%d\n",
		state_st->mc_last_idx, display_mc_idx, dpe_pre_pixl_buf, dpe_cur_pixl_buf,
		state_st->is_pause_state_last_frmae);
	pr_frc(1, "mc:pc=%d,%d, logo=%d,%d, mv=%d, mc_phs=%3d, fast_pat=%d\n",
		dpe_pre_pixl_buf, dpe_cur_pixl_buf, dpe_plogo_buf,
		dpe_clogo_buf, dpe_mvlogo_buf, dpe_intp_phs, state_st->fast_pat_set_phase0);

	data32 = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	data32_1 = rd(FRC_DPSS_DISP_BUFF_INFO_1);
	pr_frc(1, "ro:pc=%d,%d, logo=%d,%d, mv=%d, mc_phs=%3d\n",
		(data32 >> 2) & 0xf, (data32 >> 8) & 0xf, (data32 >> 14) & 0xf,
		(data32 >> 18) & 0xf, data32_1 & 0xf, (data32 >> 22) & 0xff);

	bool mc_mif_reg_mode = (rd(DPSS_DPE_MC_MIF_CTRL0) >> 2) & 0x1;
	u32	 dpe_pixl_buf_idx0 =
		 mc_mif_reg_mode && dpe_intp_phs <= 64 ? dpe_cur_pixl_buf : dpe_pre_pixl_buf;
	u32 dpe_pixl_buf_idx1 =
		mc_mif_reg_mode && dpe_intp_phs <= 64 ? dpe_pre_pixl_buf : dpe_cur_pixl_buf;

	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;
	bool nr_aapps_4k1k_en;
	bool nr_cmodel_byp_mode;

	nr_cmodel_byp_mode = 0;

	u32 src0_fbuf_yaddr[2];
	u32 src0_fbuf_caddr[2];
	u32 src0_nro_fbuf_yaddr[2];
	u32 src0_nro_fbuf_caddr[2];
	u32 src0_dio_fbuf_yaddr[2];
	u32 src0_dio_fbuf_caddr[2];

	if (pps->pps_en)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_4k1k_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	if (src0_nrdi_frc_en) {
		src0_dio_fbuf_yaddr[0]	 = rd(DPSS_SRC0_DIO_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_dio_fbuf_caddr[0]	 = rd(DPSS_SRC0_DIO_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_dio_fbuf_yaddr[1]	 = rd(DPSS_SRC0_DIO_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_dio_fbuf_caddr[1]	 = rd(DPSS_SRC0_DIO_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		if (pps->di2pps_en) {
			src0_dio_fbuf_yaddr[0] = prm_top->src0_di2pps_buf_yaddr[dpe_pixl_buf_idx0];
			src0_dio_fbuf_caddr[0] = prm_top->src0_di2pps_buf_caddr[dpe_pixl_buf_idx0];
			src0_dio_fbuf_yaddr[1] = prm_top->src0_di2pps_buf_yaddr[dpe_pixl_buf_idx1];
			src0_dio_fbuf_caddr[1] = prm_top->src0_di2pps_buf_caddr[dpe_pixl_buf_idx1];
		}
		mc_inp_body_ybuf_addr[0] = src0_dio_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_dio_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_dio_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_dio_fbuf_caddr[1];
		mc_inp_head_ybuf_addr	 = rd(DPSS_SRC0_DIOUT_YHEAD_ADDR);
		mc_inp_head_cbuf_addr	 = rd(DPSS_SRC0_DIOUT_CHEAD_ADDR);
		mc_inp_head_ybuf_step	 = rd(DPSS_SRC0_DIOUT_YHEAD_STEP);
		mc_inp_head_cbuf_step	 = rd(DPSS_SRC0_DIOUT_CHEAD_STEP);
	} else if (game_mode_ntm) {
		src0_fbuf_yaddr[0]		 = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_fbuf_caddr[0]		 = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_fbuf_yaddr[1]		 = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_fbuf_caddr[1]		 = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		mc_inp_body_ybuf_addr[0] = src0_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_fbuf_caddr[1];
		mc_inp_head_ybuf_addr = 0;
		mc_inp_head_cbuf_addr = 0;
		mc_inp_head_ybuf_step = 0;
		mc_inp_head_cbuf_step = 0;
	} else {
		src0_nro_fbuf_yaddr[0]	 = rd(DPSS_SRC0_NRO_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_nro_fbuf_caddr[0]	 = rd(DPSS_SRC0_NRO_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_nro_fbuf_yaddr[1]	 = rd(DPSS_SRC0_NRO_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_nro_fbuf_caddr[1]	 = rd(DPSS_SRC0_NRO_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		// mif chg
		if (dpss_light_chg & C_BIT0) {
			if (pchip_st && pchip_st->state_st.compr_sel == DDR_AFRC) {
				src0_nro_fbuf_yaddr[0] =
					prm_top->src0_nro_fbuf_af_y[0 + dpe_pixl_buf_idx0];
				src0_nro_fbuf_caddr[0] =
					prm_top->src0_nro_fbuf_af_c[0 + dpe_pixl_buf_idx0];
				src0_nro_fbuf_yaddr[1] =
					prm_top->src0_nro_fbuf_af_y[0 + dpe_pixl_buf_idx1];
				src0_nro_fbuf_caddr[1] =
					prm_top->src0_nro_fbuf_af_c[0 + dpe_pixl_buf_idx1];
			} else {
				src0_nro_fbuf_yaddr[0] =
					prm_top->src0_nro_fbuf_m_y[0 + dpe_pixl_buf_idx0];
				src0_nro_fbuf_caddr[0] =
					prm_top->src0_nro_fbuf_m_c[0 + dpe_pixl_buf_idx0];
				src0_nro_fbuf_yaddr[1] =
					prm_top->src0_nro_fbuf_m_y[0 + dpe_pixl_buf_idx1];
				src0_nro_fbuf_caddr[1] =
					prm_top->src0_nro_fbuf_m_c[0 + dpe_pixl_buf_idx1];
			}
		}
		mc_inp_body_ybuf_addr[0] = src0_nro_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_nro_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_nro_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_nro_fbuf_caddr[1];

		mc_inp_head_ybuf_addr	 = rd(DPSS_SRC0_NROUT_YHEAD_ADDR);
		mc_inp_head_cbuf_addr	 = rd(DPSS_SRC0_NROUT_CHEAD_ADDR);
		mc_inp_head_ybuf_step	 = rd(DPSS_SRC0_NROUT_YHEAD_STEP);
		mc_inp_head_cbuf_step	 = rd(DPSS_SRC0_NROUT_CHEAD_STEP);
		if (pchip_st && pchip_st->state_st.pps_frc) {
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx0]) {
				mc_inp_body_ybuf_addr[0] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx0];
				mc_inp_body_cbuf_addr[0] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx0];
			}
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx1]) {
				mc_inp_body_ybuf_addr[1] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx1];
				mc_inp_body_cbuf_addr[1] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx1];
			}
			dbg_pps0("pps frc di s0 0x%x,0x%x\n", mc_inp_body_ybuf_addr[0] << 5,
				mc_inp_body_ybuf_addr[1] << 5);
		} else if (pps->trig_pps == 1 && pps->pps_en) {
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx0]) {
				mc_inp_body_ybuf_addr[0] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx0];
				mc_inp_body_cbuf_addr[0] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx0];
			}
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx1]) {
				mc_inp_body_ybuf_addr[1] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx1];
				mc_inp_body_cbuf_addr[1] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx1];
			}
			dbg_pps0("pps frc di s1 0x%x,0x%x\n", mc_inp_body_ybuf_addr[0] << 5,
				mc_inp_body_ybuf_addr[1] << 5);
		} else if (pps->trig_pps == 2 || pps->trig_pps == 5) {
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx0]) {
				mc_inp_body_ybuf_addr[0] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx0];
				mc_inp_body_cbuf_addr[0] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx0];
			}
			if (prm_top->fbuf_is_pps[dpe_pixl_buf_idx1]) {
				mc_inp_body_ybuf_addr[1] =
					prm_top->src0_diopps_fbuf_yaddr[dpe_pixl_buf_idx1];
				mc_inp_body_cbuf_addr[1] =
					prm_top->src0_diopps_fbuf_caddr[dpe_pixl_buf_idx1];
			}
			dbg_pps0("pps frc di s2_5 0x%x,0x%x\n", mc_inp_body_ybuf_addr[0] << 5,
				mc_inp_body_ybuf_addr[1] << 5);
		} else {
			dbg_pps0("no pps frc di 0x%x,0x%x,%d\n", mc_inp_body_ybuf_addr[0] << 5,
					mc_inp_body_ybuf_addr[1] << 5, pps->trig_pps);
		}
	}

	mc_pix_logo_buf = rd(DPSS_SRC0_PIX_LOGO_ADDR);
	mc_blk_logo_buf = rd(DPSS_SRC0_VP_MC_LOGO_ADDR);
	mc_vpmc_mv_buf	= rd(DPSS_SRC0_VP_MC_MV_ADDR);

	mc_pix_logo_step = rd(DPSS_SRC0_PIX_LOGO_STEP);
	mc_blk_logo_step = rd(DPSS_SRC0_VP_MC_LOGO_STEP);
	mc_vpmc_mv_step	 = rd(DPSS_SRC0_VP_MC_MV_STEP);

	//=====================================================================================////
	// dpe input data
	//=====================================================================================////
	mc_luma0_rdmif_baddr0 = mc_inp_body_ybuf_addr[0] << 5;
	if (game_mode_ntm)
		mc_luma0_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx0];
	else
		mc_luma0_rdmif_baddr1 = mc_inp_head_ybuf_addr +
			mc_inp_head_ybuf_step * dpe_pixl_buf_idx0;

	mc_luma1_rdmif_baddr0 = mc_inp_body_ybuf_addr[1] << 5;
	if (game_mode_ntm)
		mc_luma1_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx1];
	else
		mc_luma1_rdmif_baddr1 = mc_inp_head_ybuf_addr +
			mc_inp_head_ybuf_step * dpe_pixl_buf_idx1;

	mc_chrm0_rdmif_baddr0 = mc_inp_body_cbuf_addr[0] << 5;
	mc_chrm0_rdmif_baddr1 =
		mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx0;

	mc_chrm1_rdmif_baddr0 = mc_inp_body_cbuf_addr[1] << 5;
	mc_chrm1_rdmif_baddr1 =
		mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx1;
	if (prm_top->dpss_mode == DPSS_FRC_MODE) {
		mc_luma0_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx0];
		mc_luma0_rdmif_baddr0 = prm_top->src0_fbuf_yaddr[dpe_pixl_buf_idx0];
		mc_chrm0_rdmif_baddr1 = prm_top->src0_head_caddr[dpe_pixl_buf_idx0];
		mc_chrm0_rdmif_baddr0 = prm_top->src0_fbuf_caddr[dpe_pixl_buf_idx0];
		mc_luma1_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx1];
		mc_luma1_rdmif_baddr0 = prm_top->src0_fbuf_yaddr[dpe_pixl_buf_idx1];
		mc_chrm1_rdmif_baddr1 = prm_top->src0_head_caddr[dpe_pixl_buf_idx1];
		mc_chrm1_rdmif_baddr0 = prm_top->src0_fbuf_caddr[dpe_pixl_buf_idx1];
	}
	ip_logo0_rdmif_baddr = mc_pix_logo_buf + dpe_plogo_buf * mc_pix_logo_step;
	ip_logo1_rdmif_baddr = mc_pix_logo_buf + dpe_clogo_buf * mc_pix_logo_step;
	dpe_melogo_baddr	 = mc_blk_logo_buf + dpe_mvlogo_buf * mc_blk_logo_step;
	dpe_vpmc_mv_baddr	 = mc_vpmc_mv_buf + dpe_mvlogo_buf * mc_vpmc_mv_step;

	if (state_st->check_frc_status_en) {
		pr_frc(1, "%s mc_luma0_rdmif_baddr1= %x\n", __func__, mc_luma0_rdmif_baddr1);
		pr_frc(1, "%s mc_luma0_rdmif_baddr0= %x\n", __func__, mc_luma0_rdmif_baddr0);
		pr_frc(1, "%s mc_chrm0_rdmif_baddr1= %x\n", __func__, mc_chrm0_rdmif_baddr1);
		pr_frc(1, "%s mc_chrm0_rdmif_baddr0= %x\n", __func__, mc_chrm0_rdmif_baddr0);
		pr_frc(1, "%s mc_luma1_rdmif_baddr1= %x\n", __func__, mc_luma1_rdmif_baddr1);
		pr_frc(1, "%s mc_luma1_rdmif_baddr0= %x\n", __func__, mc_luma1_rdmif_baddr0);
		pr_frc(1, "%s mc_chrm1_rdmif_baddr1= %x\n", __func__, mc_chrm1_rdmif_baddr1);
		pr_frc(1, "%s mc_chrm1_rdmif_baddr0= %x\n", __func__, mc_chrm1_rdmif_baddr0);

		pr_frc(1, "%s ip_logo0_rdmif_baddr = %x\n", __func__, ip_logo0_rdmif_baddr);
		pr_frc(1, "%s ip_logo1_rdmif_baddr = %x\n", __func__, ip_logo1_rdmif_baddr);
		pr_frc(1, "%s dpe_melogo_baddr     = %x\n", __func__, dpe_melogo_baddr);
		pr_frc(1, "%s dpe_vpmc_mv_baddr    = %x\n", __func__, dpe_vpmc_mv_baddr);
	}

	////==========================================================////
	//// dpe input data
	////==========================================================////

	u32 mc_use_tbc_addr = rd(DPSS_TOP_APB_TBC_SEL_CTRL) & 0x1;

	if (mc_use_tbc_addr == 0) {
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LUMA0_BADDR0, mc_luma0_rdmif_baddr0);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LUMA0_BADDR1, mc_luma0_rdmif_baddr1);

		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LUMA1_BADDR0, mc_luma1_rdmif_baddr0);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LUMA1_BADDR1, mc_luma1_rdmif_baddr1);

		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_CHRM0_BADDR0, mc_chrm0_rdmif_baddr0);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_CHRM0_BADDR1, mc_chrm0_rdmif_baddr1);

		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_CHRM1_BADDR0, mc_chrm1_rdmif_baddr0);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_CHRM1_BADDR1, mc_chrm1_rdmif_baddr1);
		if (prm_top->dpss_mode == DPSS_FRC_MODE &&
			pchip_st->state_st.compr_sel != DDR_MIF) {
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFBC_HEAD_BADDR, mc_luma0_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFBC_BODY_BADDR, mc_luma0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFRC_CHRM_HEAD_BADDR, mc_chrm0_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFRC_CHRM_BODY_BADDR, mc_chrm0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFBC_HEAD_BADDR, mc_luma1_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFBC_BODY_BADDR, mc_luma1_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFRC_CHRM_HEAD_BADDR, mc_chrm1_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFRC_CHRM_BODY_BADDR, mc_chrm1_rdmif_baddr0);
		}

		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LOGO_BADDR0, ip_logo0_rdmif_baddr);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_LOGO_BADDR1, ip_logo1_rdmif_baddr);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_MELG_BADDR1, dpe_melogo_baddr);
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_DIN_MV_BADDR1, dpe_vpmc_mv_baddr);
		DPSS_RDMA_WR_BIT_PRE_VS(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_pre_frm_idx, 8, 4);
		DPSS_RDMA_WR_BIT_PRE_VS(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_cur_frm_idx, 4, 4);
		DPSS_RDMA_WR_BIT_PRE_VS(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_mv_cur_frm_idx, 0, 4);
		// w_reg_bit(DPSS_DPE_MC_CTRL0,dpe_intp_phs,20,8);
		DPSS_RDMA_WR_BIT_PRE_VS(DPSS_DPE_MC_PHASE, dpe_intp_phs, 0, 8);

		dpss_dpe_dbg_cfg();
	}

	bool cut_win_en = prm_top->cut_win_en;
	u32 get_pix_logo_mode;

	struct PRM_INTF_TYPE *pre_logo_rmif = &pchip_st->pre_logo_rmif;
	struct PRM_INTF_TYPE *cur_logo_rmif = &pchip_st->cur_logo_rmif;
	struct PRM_INTF_TYPE *mv_rmif = &pchip_st->mv_rmif;
	struct PRM_INTF_TYPE *me_logo_rmif = &pchip_st->me_logo_rmif;

	memset((void *)(pre_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(cur_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(mv_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(me_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));

	pre_logo_rmif->burst_len = 2;   // 1: burst4, 2: brst8, 3: burst16
	cur_logo_rmif->burst_len = 2;
	mv_rmif->burst_len		= 2;
	me_logo_rmif->burst_len	= 2;
	if (cut_win_en) {
		get_pix_logo_mode = rd(FRC_MC_LOGO_OPTION);
		pre_logo_rmif->src_baddr[0] =
			get_pix_logo_mode == 2 ? ip_logo1_rdmif_baddr : ip_logo0_rdmif_baddr;
		cur_logo_rmif->src_baddr[0] =
			get_pix_logo_mode == 1 ? ip_logo0_rdmif_baddr : ip_logo1_rdmif_baddr;
		mv_rmif->src_baddr[0]	  = dpe_vpmc_mv_baddr;
		me_logo_rmif->src_baddr[0] = dpe_melogo_baddr;

		cfg_mc_sub_rdmif(pre_logo_rmif, 0, 1);
		cfg_mc_sub_rdmif(cur_logo_rmif, 1, 1);
		cfg_mc_sub_rdmif(mv_rmif, 2, 1);
		cfg_mc_sub_rdmif(me_logo_rmif, 3, 1);
	}

	return true;
}

void hw_dpss_dpe_game_info_cfg(struct PRM_DPSS_TOP *prm_top, bool obuf_rdy)
{
	u32 data32;
	u32 dpe_pre_pixl_buf;
	u32 dpe_cur_pixl_buf;
	u32 dpe_plogo_buf;
	u32 dpe_clogo_buf;
	u32 dpe_intp_phs;
	u32 dpe_byp_en;
	u32 dpe_mvlogo_buf;
	u32 dpe_mv_cur_frm_idx;
	u32 dpe_image_cur_frm_idx;
	u32 dpe_image_pre_frm_idx;

	u32 mc_inp_body_ybuf_addr[2];
	u32 mc_inp_body_cbuf_addr[2];
	u32 mc_inp_head_ybuf_addr;
	u32 mc_inp_head_cbuf_addr;

	u32 mc_inp_head_ybuf_step;
	u32 mc_inp_head_cbuf_step;

	u32 mc_pix_logo_buf;
	u32 mc_blk_logo_buf;
	u32 mc_vpmc_mv_buf;

	u32 mc_pix_logo_step;
	u32 mc_blk_logo_step;
	u32 mc_vpmc_mv_step;
	u32 mc_ibuf_vld;
	u32 game_mode_ntm;

	u32 mc_luma0_rdmif_baddr0;
	u32 mc_luma0_rdmif_baddr1;
	u32 mc_luma1_rdmif_baddr0;
	u32 mc_luma1_rdmif_baddr1;

	u32 mc_chrm0_rdmif_baddr0;
	u32 mc_chrm0_rdmif_baddr1;
	u32 mc_chrm1_rdmif_baddr0;
	u32 mc_chrm1_rdmif_baddr1;

	u32 ip_logo0_rdmif_baddr;
	u32 ip_logo1_rdmif_baddr;
	u32 dpe_melogo_baddr;
	u32 dpe_vpmc_mv_baddr;

	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	if (!pchip_st || !obuf_rdy)
		return;

	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	data32		  = rd(DPSS_FRC_MC_IUFF_STATUS);
	mc_ibuf_vld	  = (data32 >> 4) & 0x1;
	game_mode_ntm = (rd(FRC_DPSS_LLM) >> 2) & 0x1;

	data32			 = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	dpe_pre_pixl_buf = (data32 >> 2) & 0xf;
	dpe_cur_pixl_buf = (data32 >> 8) & 0xf;

	dpe_plogo_buf		 = (data32 >> 14) & 0xf;
	dpe_clogo_buf		 = (data32 >> 18) & 0xf;
	dpe_intp_phs		 = (mc_ibuf_vld) ? (data32 >> 22) & 0xff : 0;
	dpe_byp_en			 = (data32 >> 30) & 0x1;

	if (pchip_st && (pchip_st->state_st.mc_bypass || pchip_st->state_st.force_mc_phase0))
		dpe_intp_phs = 0;

	state_st->dpe_ready = true;
	pr_frc(1, "game frc dpe_pre_pixl_buf=%d, dpe_cur_pixl_buf=%d\n",
		dpe_pre_pixl_buf, dpe_cur_pixl_buf);

	pr_frc(1, "display buff info: %x\n", data32);
	pr_frc(1, "dpe_plogo_buf        = %#x\n", dpe_plogo_buf);
	pr_frc(1, "dpe_clogo_buf        = %#x\n", dpe_clogo_buf);
	pr_frc(1, "dpe_intp_phs         = %#x\n", dpe_intp_phs);

	data32				  = rd(FRC_DPSS_DISP_BUFF_INFO_1);
	dpe_mvlogo_buf		  = (data32 >> 0) & 0xf;
	dpe_mv_cur_frm_idx	  = dpe_mvlogo_buf;
	dpe_image_cur_frm_idx = dpe_cur_pixl_buf;
	dpe_image_pre_frm_idx = dpe_pre_pixl_buf;
	pr_frc(1, "dpe_mv_cur_frm_idx      = %#x\n", dpe_mv_cur_frm_idx);

	bool mc_mif_reg_mode = (rd(DPSS_DPE_MC_MIF_CTRL0) >> 2) & 0x1;
	u32	 dpe_pixl_buf_idx0 =
		 mc_mif_reg_mode && dpe_intp_phs <= 64 ? dpe_cur_pixl_buf : dpe_pre_pixl_buf;
	u32 dpe_pixl_buf_idx1 =
		mc_mif_reg_mode && dpe_intp_phs <= 64 ? dpe_pre_pixl_buf : dpe_cur_pixl_buf;

	u32 src0_fbuf_yaddr[2];
	u32 src0_fbuf_caddr[2];

	w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	src0_fbuf_yaddr[0]		 = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx0);
	src0_fbuf_caddr[0]		 = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx0);
	src0_fbuf_yaddr[1]		 = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx1);
	src0_fbuf_caddr[1]		 = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx1);
	mc_inp_body_ybuf_addr[0] = src0_fbuf_yaddr[0];
	mc_inp_body_cbuf_addr[0] = src0_fbuf_caddr[0];
	mc_inp_body_ybuf_addr[1] = src0_fbuf_yaddr[1];
	mc_inp_body_cbuf_addr[1] = src0_fbuf_caddr[1];
	mc_inp_head_ybuf_addr = 0;
	mc_inp_head_cbuf_addr = 0;
	mc_inp_head_ybuf_step = 0;
	mc_inp_head_cbuf_step = 0;

	mc_pix_logo_buf = rd(DPSS_SRC0_PIX_LOGO_ADDR);
	mc_blk_logo_buf = rd(DPSS_SRC0_VP_MC_LOGO_ADDR);
	mc_vpmc_mv_buf	= rd(DPSS_SRC0_VP_MC_MV_ADDR);

	mc_pix_logo_step = rd(DPSS_SRC0_PIX_LOGO_STEP);
	mc_blk_logo_step = rd(DPSS_SRC0_VP_MC_LOGO_STEP);
	mc_vpmc_mv_step	 = rd(DPSS_SRC0_VP_MC_MV_STEP);

	//=====================================================================================////
	// dpe input data
	//=====================================================================================////
	mc_luma0_rdmif_baddr0 = mc_inp_body_ybuf_addr[0] << 5;
	if (game_mode_ntm)
		mc_luma0_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx0];
	else
		mc_luma0_rdmif_baddr1 = mc_inp_head_ybuf_addr +
			mc_inp_head_ybuf_step * dpe_pixl_buf_idx0;

	mc_luma1_rdmif_baddr0 = mc_inp_body_ybuf_addr[1] << 5;
	if (game_mode_ntm)
		mc_luma1_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx1];
	else
		mc_luma1_rdmif_baddr1 = mc_inp_head_ybuf_addr +
			mc_inp_head_ybuf_step * dpe_pixl_buf_idx1;

	mc_chrm0_rdmif_baddr0 = mc_inp_body_cbuf_addr[0] << 5;
	mc_chrm0_rdmif_baddr1 =
		mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx0;

	mc_chrm1_rdmif_baddr0 = mc_inp_body_cbuf_addr[1] << 5;
	mc_chrm1_rdmif_baddr1 =
		mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx1;
	if (prm_top->dpss_mode == DPSS_FRC_MODE) {
		mc_luma0_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx0];
		mc_luma0_rdmif_baddr0 = prm_top->src0_fbuf_yaddr[dpe_pixl_buf_idx0];
		mc_chrm0_rdmif_baddr1 = prm_top->src0_head_caddr[dpe_pixl_buf_idx0];
		mc_chrm0_rdmif_baddr0 = prm_top->src0_fbuf_caddr[dpe_pixl_buf_idx0];
		mc_luma1_rdmif_baddr1 = prm_top->src0_head_yaddr[dpe_pixl_buf_idx1];
		mc_luma1_rdmif_baddr0 = prm_top->src0_fbuf_yaddr[dpe_pixl_buf_idx1];
		mc_chrm1_rdmif_baddr1 = prm_top->src0_head_caddr[dpe_pixl_buf_idx1];
		mc_chrm1_rdmif_baddr0 = prm_top->src0_fbuf_caddr[dpe_pixl_buf_idx1];
	}
	ip_logo0_rdmif_baddr = mc_pix_logo_buf + dpe_plogo_buf * mc_pix_logo_step;
	ip_logo1_rdmif_baddr = mc_pix_logo_buf + dpe_clogo_buf * mc_pix_logo_step;
	dpe_melogo_baddr	 = mc_blk_logo_buf + dpe_mvlogo_buf * mc_blk_logo_step;
	dpe_vpmc_mv_baddr	 = mc_vpmc_mv_buf + dpe_mvlogo_buf * mc_vpmc_mv_step;

	pr_frc(1, "mc_luma0_rdmif_baddr1= %x\n", mc_luma0_rdmif_baddr1);
	pr_frc(1, "mc_luma0_rdmif_baddr0= %x\n", mc_luma0_rdmif_baddr0);
	pr_frc(1, "mc_chrm0_rdmif_baddr1= %x\n", mc_chrm0_rdmif_baddr1);
	pr_frc(1, "mc_chrm0_rdmif_baddr0= %x\n", mc_chrm0_rdmif_baddr0);
	pr_frc(1, "mc_luma1_rdmif_baddr1= %x\n", mc_luma1_rdmif_baddr1);
	pr_frc(1, "mc_luma1_rdmif_baddr0= %x\n", mc_luma1_rdmif_baddr0);
	pr_frc(1, "mc_chrm1_rdmif_baddr1= %x\n", mc_chrm1_rdmif_baddr1);
	pr_frc(1, "mc_chrm1_rdmif_baddr0= %x\n", mc_chrm1_rdmif_baddr0);

	pr_frc(1, "ip_logo0_rdmif_baddr = %x\n", ip_logo0_rdmif_baddr);
	pr_frc(1, "ip_logo1_rdmif_baddr = %x\n", ip_logo1_rdmif_baddr);
	pr_frc(1, "dpe_melogo_baddr     = %x\n", dpe_melogo_baddr);
	pr_frc(1, "dpe_vpmc_mv_baddr    = %x\n", dpe_vpmc_mv_baddr);

	////==========================================================////
	//// dpe input data
	////==========================================================////

	u32 mc_use_tbc_addr = rd(DPSS_TOP_APB_TBC_SEL_CTRL) & 0x1;

	if (mc_use_tbc_addr == 0) {
		pr_frc(1, "MC use driver config\n");

		wr(DPSS_DPE_MC_DIN_LUMA0_BADDR0, mc_luma0_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_LUMA0_BADDR1, mc_luma0_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_LUMA1_BADDR0, mc_luma1_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_LUMA1_BADDR1, mc_luma1_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_CHRM0_BADDR0, mc_chrm0_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_CHRM0_BADDR1, mc_chrm0_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_CHRM1_BADDR0, mc_chrm1_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_CHRM1_BADDR1, mc_chrm1_rdmif_baddr1);
		if (prm_top->dpss_mode == DPSS_FRC_MODE &&
			pchip_st->state_st.compr_sel != DDR_MIF) {
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFBC_HEAD_BADDR, mc_luma0_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFBC_BODY_BADDR, mc_luma0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFRC_CHRM_HEAD_BADDR, mc_chrm0_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
				VFCD_AFRC_CHRM_BODY_BADDR, mc_chrm0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFBC_HEAD_BADDR, mc_luma1_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFBC_BODY_BADDR, mc_luma1_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFRC_CHRM_HEAD_BADDR, mc_chrm1_rdmif_baddr1);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
				VFCD_AFRC_CHRM_BODY_BADDR, mc_chrm1_rdmif_baddr0);
			pr_frc(1, "MC vfcd afbc/afrc config\n");
		}

		wr(DPSS_DPE_MC_DIN_LOGO_BADDR0, ip_logo0_rdmif_baddr);
		wr(DPSS_DPE_MC_DIN_LOGO_BADDR1, ip_logo1_rdmif_baddr);
		wr(DPSS_DPE_MC_DIN_MELG_BADDR1, dpe_melogo_baddr);
		wr(DPSS_DPE_MC_DIN_MV_BADDR1, dpe_vpmc_mv_baddr);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_pre_frm_idx, 8, 4);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_cur_frm_idx, 4, 4);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_mv_cur_frm_idx, 0, 4);
		// w_reg_bit(DPSS_DPE_MC_CTRL0,dpe_intp_phs,20,8);
		if (dpe_intp_phs == 0x0)
			w_reg_bit(DPSS_DPE_MC_PHASE, dpe_intp_phs, 0, 8);
		else if (dpe_intp_phs == 0x40) // for game frc deblur
			w_reg_bit(DPSS_DPE_MC_PHASE, game_st->game_deblur, 0, 8);

		dpss_dpe_dbg_cfg();
	} else {
		pr_frc(1, "MC use tbc config\n");
	}

	bool cut_win_en = prm_top->cut_win_en;
	u32 get_pix_logo_mode;
	struct frc_chip_st *pchip = dpss_get_frc_st();

	if (unlikely(!pchip)) {
		DBG_ERR("dpe cfg null pointer: pchip is NULL\n");
		return;
	}

	struct PRM_INTF_TYPE *pre_logo_rmif = &pchip->pre_logo_rmif;
	struct PRM_INTF_TYPE *cur_logo_rmif = &pchip->cur_logo_rmif;
	struct PRM_INTF_TYPE *mv_rmif = &pchip->mv_rmif;
	struct PRM_INTF_TYPE *me_logo_rmif = &pchip->me_logo_rmif;

	memset((void *)(pre_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(cur_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(mv_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(me_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));

	pre_logo_rmif->burst_len = 2;   // 1: burst4, 2: brst8, 3: burst16
	cur_logo_rmif->burst_len = 2;
	mv_rmif->burst_len		= 2;
	me_logo_rmif->burst_len	= 2;
	if (cut_win_en) {
		get_pix_logo_mode = rd(FRC_MC_LOGO_OPTION);
		pre_logo_rmif->src_baddr[0] =
			get_pix_logo_mode == 2 ? ip_logo1_rdmif_baddr : ip_logo0_rdmif_baddr;
		cur_logo_rmif->src_baddr[0] =
			get_pix_logo_mode == 1 ? ip_logo0_rdmif_baddr : ip_logo1_rdmif_baddr;
		mv_rmif->src_baddr[0]	  = dpe_vpmc_mv_baddr;
		me_logo_rmif->src_baddr[0] = dpe_melogo_baddr;

		cfg_mc_sub_rdmif(pre_logo_rmif, 0, 1);
		cfg_mc_sub_rdmif(cur_logo_rmif, 1, 1);
		cfg_mc_sub_rdmif(mv_rmif, 2, 1);
		cfg_mc_sub_rdmif(me_logo_rmif, 3, 1);
	}
}

#define PIC_ORG_Y_BADDR 0x72000000
#define PIC_ORG_C_BADDR 0x82000000
#define PIC_HEADY_BADDR 0xa4200000
#define PIC_HEADC_BADDR 0xa4600000

#define PIC_ORG_DIY_BADDR 0xC5400000
#define PIC_ORG_DIC_BADDR 0xC7400000
#define PIC_DI_HEADY_BADDR 0xe1a00000
#define PIC_DI_HEADC_BADDR 0xe1e00000
#define PIC_ORG_Y_OFST 0x01000000
#define PIC_ORG_C_OFST 0x01000000
#define PIC_ORG_DIY_OFST 0x00200000
#define PIC_ORG_DIC_OFST 0x00200000

void hw_dpss_dpe_info_cfg_sw(u32 *base_addr, u32 *base_oft, u32 buf_num,
				u32 out_addr_en, struct PRM_DPSS_TOP *prm_top,
				u32 dae_frm_idx, u32 vdin_frm_idx)
{
	////====================================================================================////
	//// dpe input data
	////====================================================================================////
	static u32 phs_cnt;
	static u32 mc_frm_cnt = 1;	 // prm_top.dpe_game_mode ? 0 : 1;
	static u32 iplogo_frm_cnt;
	static u32 mv_frm_cnt;
	static u32 vdin_buf_num = 16;
	static u32 mv_phs_idx;

	mc_frm_cnt = prm_top->dpe_game_mode ? 0 : 1;

	// ary no use    u32 data32;
	u32 dpe_pre_pixl_buf;	//= (data32>>2 )&0xf;
	u32 dpe_cur_pixl_buf;	//= (data32>>8 )&0xf;
	// ary no use    u32 dpe_ibuf_nr_byps_cur ;//= (data32>>13)&0x1;
	u32 dpe_plogo_buf;			 //= (data32>>14)&0xf;
	u32 dpe_clogo_buf;			 //= (data32>>18)&0xf;
	u32 dpe_intp_phs;			 //= (data32>>22)&0xff;
	u32 dpe_byp_en;				 //= (data32>>30)&0x1;
	u32 dpe_mvlogo_buf;			 //= (data32>> 0)&0xf;
	u32 dpe_mv_cur_frm_idx;		 //= (data32>> 0)&0xf;
	u32 dpe_image_cur_frm_idx;	 //= (data32>> 0)&0xf;
	u32 dpe_image_pre_frm_idx;	 //= (data32>> 0)&0xf;

	u32 mc_inp_luma_addr_body;
	u32 mc_inp_chrm_addr_body;
	u32 mc_in_luma_step_body;
	u32 mc_in_chrm_step_body;
	u32 mc_inp_luma_addr_head;
	u32 mc_inp_chrm_addr_head;
	u32 mc_in_luma_step_head;
	u32 mc_in_chrm_step_head;

	u32 mc_pix_logo_buf;   //= rd(DPSS_SRC0_PIX_LOGO_ADDR);
	u32 mc_blk_logo_buf;   //= rd(DPSS_SRC0_VP_MC_LOGO_ADDR);
	u32 mc_vpmc_mv_buf;	   //= rd(DPSS_SRC0_VP_MC_MV_ADDR);

	u32 mc_pix_logo_step;	//= rd(DPSS_SRC0_PIX_LOGO_STEP);
	u32 mc_blk_logo_step;	//= rd(DPSS_SRC0_VP_MC_LOGO_STEP);
	u32 mc_vpmc_mv_step;	//= rd(DPSS_SRC0_VP_MC_MV_STEP);

	////====================================================================////
	//// dpe input data
	////===================================================================////

	u32 mc_luma0_rdmif_baddr0;
	u32 mc_luma0_rdmif_baddr1;
	u32 mc_luma1_rdmif_baddr0;
	u32 mc_luma1_rdmif_baddr1;

	u32 mc_chrm0_rdmif_baddr0;
	u32 mc_chrm0_rdmif_baddr1;
	u32 mc_chrm1_rdmif_baddr0;
	u32 mc_chrm1_rdmif_baddr1;

	u32 ip_logo0_rdmif_baddr;	//= mc_pix_logo_buf + dpe_plogo_buf*mc_pix_logo_step;
	u32 ip_logo1_rdmif_baddr;	//= mc_pix_logo_buf + dpe_clogo_buf*mc_pix_logo_step;
	u32 dpe_melogo_baddr;		//= mc_blk_logo_buf + dpe_mvlogo_buf*mc_blk_logo_step;
	u32 dpe_vpmc_mv_baddr;		//= mc_vpmc_mv_buf  + dpe_mvlogo_buf*mc_vpmc_mv_step ;
	u32 phs_step;
	u32 phs_num;
	u32 mc_phs;
	u32 pre_logo_fid;
	u32 cur_logo_fid;

	mc_inp_luma_addr_body = out_addr_en ? base_addr[0] : PIC_ORG_Y_BADDR >> 4;
	mc_inp_chrm_addr_body = out_addr_en ? base_addr[1] : PIC_ORG_C_BADDR >> 4;
	mc_inp_luma_addr_head = out_addr_en ? base_addr[2] : 0xa420000;
	mc_inp_chrm_addr_head = out_addr_en ? base_addr[3] : 0xa460000;
	mc_pix_logo_buf		  = out_addr_en ? base_addr[4] : 0x43b8000;
	mc_blk_logo_buf		  = out_addr_en ? base_addr[5] : 0x4438000;
	mc_vpmc_mv_buf		  = out_addr_en ? base_addr[6] : 0x43f8000;

	mc_in_luma_step_body = out_addr_en ? base_oft[0] : PIC_ORG_Y_OFST >> 4;
	mc_in_chrm_step_body = out_addr_en ? base_oft[1] : PIC_ORG_C_OFST >> 4;
	mc_in_luma_step_head = out_addr_en ? base_oft[2] : 0x4000;
	mc_in_chrm_step_head = out_addr_en ? base_oft[3] : 0x4000;
	mc_pix_logo_step	 = out_addr_en ? base_oft[4] : 0x8000;
	mc_blk_logo_step	 = out_addr_en ? base_oft[5] : 0x8000;
	mc_vpmc_mv_step		 = out_addr_en ? base_oft[6] : 0x8000;
	if (prm_top->frc_ratio == DPSS_FRC_RATIO_1_2) {
		phs_step = 10 / 2 * 128 / 10;
		phs_num	 = 2;
	} else if (prm_top->frc_ratio == DPSS_FRC_RATIO_2_5) {
		phs_step = 20 / 5 * 128 / 10;
		phs_num	 = 5;
	} else if (prm_top->frc_ratio == DPSS_FRC_RATIO_1_4) {
		phs_step = 10 / 4 * 128 / 10;
		phs_num	 = 4;
	} else {
		phs_step = 64;
		phs_num	 = 2;
	}

	mc_phs		 = phs_step * phs_cnt;
	dpe_intp_phs = mc_phs % 128;
	dpe_byp_en	 = 0;	// TODO

	dpe_pre_pixl_buf = mc_frm_cnt % vdin_buf_num;
	dpe_cur_pixl_buf = (mc_frm_cnt + 1) % vdin_buf_num;

	pre_logo_fid = iplogo_frm_cnt < 1 ? buf_num - 1 : iplogo_frm_cnt - 1;
	cur_logo_fid = iplogo_frm_cnt;

	dpe_plogo_buf =
		pre_logo_fid;	//(2==(rd(FRC_MC_LOGO_OPTION)&0xf)) ? cur_logo_fid : pre_logo_fid;
	dpe_clogo_buf =
		cur_logo_fid;	//(1==(rd(FRC_MC_LOGO_OPTION)&0xf)) ? pre_logo_fid : cur_logo_fid;
	dpe_mvlogo_buf		  = mv_frm_cnt;
	dpe_mv_cur_frm_idx	  = mv_phs_idx;			// dae_frm_idx%buf_num;
	dpe_image_cur_frm_idx = dpe_cur_pixl_buf;	// vdin_frm_idx%vdin_buf_num;
	dpe_image_pre_frm_idx = dpe_pre_pixl_buf;	// vdin_frm_idx%vdin_buf_num;
	pr_frc(1, "dpe_pre_pixl_buf        : %x\n", dpe_pre_pixl_buf);
	pr_frc(1, "dpe_cur_pixl_buf        : %x\n", dpe_cur_pixl_buf);
	pr_frc(1, "dpe_plogo_buf           = %x\n", dpe_plogo_buf);
	pr_frc(1, "dpe_clogo_buf           = %x\n", dpe_clogo_buf);
	pr_frc(1, "dpe_mvlogo_buf          = %x\n", dpe_mvlogo_buf);
	pr_frc(1, "dpe_mv_cur_frm_idx      = %x\n", dpe_mv_cur_frm_idx);
	pr_frc(1, "dpe_image_cur_frm_idx   = %x\n", dpe_image_cur_frm_idx);
	////====================================////
	//// dpe input data
	////====================================////

	mc_luma0_rdmif_baddr0 = mc_inp_luma_addr_body + mc_in_luma_step_body * dpe_pre_pixl_buf;
	mc_luma0_rdmif_baddr1 = mc_inp_luma_addr_head + mc_in_luma_step_head * dpe_pre_pixl_buf;
	mc_luma1_rdmif_baddr0 = mc_inp_luma_addr_body + mc_in_luma_step_body * dpe_cur_pixl_buf;
	mc_luma1_rdmif_baddr1 = mc_inp_luma_addr_head + mc_in_luma_step_head * dpe_cur_pixl_buf;

	mc_chrm0_rdmif_baddr0 = mc_inp_chrm_addr_body + mc_in_chrm_step_body * dpe_pre_pixl_buf;
	mc_chrm0_rdmif_baddr1 = mc_inp_chrm_addr_head + mc_in_chrm_step_head * dpe_pre_pixl_buf;
	mc_chrm1_rdmif_baddr0 = mc_inp_chrm_addr_body + mc_in_chrm_step_body * dpe_cur_pixl_buf;
	mc_chrm1_rdmif_baddr1 = mc_inp_chrm_addr_head + mc_in_chrm_step_head * dpe_cur_pixl_buf;

	ip_logo0_rdmif_baddr = mc_pix_logo_buf + dpe_plogo_buf * mc_pix_logo_step;
	ip_logo1_rdmif_baddr = mc_pix_logo_buf + dpe_clogo_buf * mc_pix_logo_step;
	dpe_melogo_baddr	 = mc_blk_logo_buf + dpe_mvlogo_buf * mc_blk_logo_step;
	dpe_vpmc_mv_baddr	 = mc_vpmc_mv_buf + dpe_mvlogo_buf * mc_vpmc_mv_step;

	////===================================////
	//// dpe input data
	////===================================////

	// w_reg_bit(DPSS_DPE_TOP_SW_SEL0,0x7 ,23,3); //for nr, not for mc
	// w_reg_bit(DPSS_DPE_TOP_SW_SEL0,0x1 ,31,1);
	// w_reg_bit(DPSS_DPE_TOP_SW_SEL1,0x3f,3 ,6);

	w_reg_bit(DPSS_DPE_MC_DIN_LUMA0_BADDR0, mc_luma0_rdmif_baddr0, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_LUMA0_BADDR1, mc_luma0_rdmif_baddr1, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_LUMA1_BADDR0, mc_luma1_rdmif_baddr0, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_LUMA1_BADDR1, mc_luma1_rdmif_baddr1, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_CHRM0_BADDR0, mc_chrm0_rdmif_baddr0, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_CHRM0_BADDR1, mc_chrm0_rdmif_baddr1, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_CHRM1_BADDR0, mc_chrm1_rdmif_baddr0, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_CHRM1_BADDR1, mc_chrm1_rdmif_baddr1, 0, 32);

	w_reg_bit(DPSS_DPE_MC_DIN_LOGO_BADDR0, ip_logo0_rdmif_baddr, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_LOGO_BADDR1, ip_logo1_rdmif_baddr, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_MELG_BADDR1, dpe_melogo_baddr, 0, 32);
	w_reg_bit(DPSS_DPE_MC_DIN_MV_BADDR1, dpe_vpmc_mv_baddr, 0, 32);

	w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_pre_frm_idx, 8, 4);
	w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_cur_frm_idx, 4, 4);
	w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_mv_cur_frm_idx, 0, 4);

	w_reg_bit(DPSS_DPE_MC_CTRL0, dpe_intp_phs, 20, 8);
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 15, 1);	// enable reg_vdin_frm_idx
	w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX1, vdin_frm_idx % vdin_buf_num, 4, 4);
	w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX1, dae_frm_idx % buf_num, 0, 4);	  //
	//

	// w_reg_bit(DPSS_DPE_DIN_SEL_CTRL0,1           ,4 ,4);//reg_dpe_src_idx
	w_reg_bit(FRC_MC_HW_CTRL0, dpe_byp_en, 20, 1);   // reg_bypass_mc_core_en

	mv_phs_idx = (mv_phs_idx + 1) % buf_num;
	phs_cnt	   = (phs_cnt + 1) % phs_num;
	mv_frm_cnt = (dpe_intp_phs == 0) ? mv_frm_cnt : (mv_frm_cnt + 1) % buf_num;
	if ((mc_phs + phs_step) >= 128) {
		mc_frm_cnt	   = (mc_frm_cnt + 1) % vdin_buf_num;
		iplogo_frm_cnt = (iplogo_frm_cnt + 1) % buf_num;
	}
}

void hw_mc_vfcd_cfg_update(void)
{
	u32 regs_ofst;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st) {
		dbg_h0("pchip_st is null\n");
		return;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		dbg_h0("pfw_data is null\n");
		return;
	}
	frc_top = &pfw_data->frc_top_type;

	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, &pchip_st->mc_pix_rmif, pchip_st->fmt444_out);
	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, &pchip_st->mc_pix_rmif, pchip_st->fmt444_out);
	if ((frc_top->mc_skip_mode & 0x2) ==  0x2) {
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC0);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC1);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
	}
	frc_cfg_vfcd_dec(4, &pchip_st->mc_vfcd);   // mc0
	frc_cfg_vfcd_dec(5, &pchip_st->mc_vfcd);   // mc1

	if (pchip_st->prm_mc.src_cmpr == CMPR_AFBC) {
		frc_cfg_vfcd_cmpr_enable(4, 1);
		frc_cfg_vfcd_cmpr_enable(5, 1);
		VSYNC_WR_VIDEO_TABLE_REG(4 * 256 + VFCD_TOP_CTRL3, 0xa);
		VSYNC_WR_VIDEO_TABLE_REG(5 * 256 + VFCD_TOP_CTRL3, 0xa);
	} else if (pchip_st->prm_mc.src_cmpr == CMPR_AFRC) {
		frc_cfg_vfcd_cmpr_enable(4, 2);
		frc_cfg_vfcd_cmpr_enable(5, 2);
	} else {
		frc_cfg_vfcd_cmpr_enable(4, 0);
		frc_cfg_vfcd_cmpr_enable(5, 0);
	}

	regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_RPT_LOOP + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F0_LUMA_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F0_CHRM_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F1_LUMA_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F1_CHRM_RPT_PAT + regs_ofst, 0);

	//afrc not need cfg 444_comb
	VSYNC_WR_VIDEO_TABLE_REG_BITS(VFCD_AFBC_ENABLE + regs_ofst, 0, 20, 1);
}

void dpss_dpe_dbg_cfg(void)
{
	u32 me_dbg1  = rd(FRC_REG_ME_DEBUG1);
	u32 me_dbg2  = rd(FRC_REG_ME_DEBUG2);
	u32 vp_dbg   = rd(FRC_REG_VP_DEBUG1);
	u32 mc_dbg   = rd(FRC_REG_MC_DEBUG1);
	u32 logo_dbg = rd(FRC_LOGO_DEBUG);
	u32 path_dbg = rd(FRC_REG_DEBUG_PATH_MV);

//    u32 reg_debug_path_en           = (me_dbg1 >> 31) & 0x1;//[31];
	u32 reg_me_debug_cn_fs_en       = (me_dbg1 >> 30) & 0x1;//[30];
	u32 reg_me_debug_nc_fs_en       = (me_dbg1 >> 29) & 0x1;//[29];
	u32 reg_me_debug_pc_prj_mode    = (me_dbg1 >> 26) & 0x7;//[28:26];
	u32 reg_me_debug_cp_prj_mode    = (me_dbg1 >> 23) & 0x7;//[25:23];
	u32 reg_me_debug_pc_sad_mode    = (me_dbg1 >> 20) & 0x7;//[22:20];
	u32 reg_me_debug_cn_sad_mode    = (me_dbg1 >> 17) & 0x7;//[19:17];
	u32 reg_me_debug_nc_sad_mode    = (me_dbg1 >> 14) & 0x7;//[16:14];
	u32 reg_me_debug_sad_div        = (me_dbg1 >> 11) & 0x7;//[13:11];
	u32 reg_me_debug_pc_acdc_flg_en = (me_dbg1 >> 10) & 0x1;//[10];
	u32 reg_me_debug_cn_acdc_flg_en = (me_dbg1 >> 9) & 0x1;//[9];
	u32 reg_me_debug_nc_acdc_flg_en = (me_dbg1 >> 8) & 0x1;//[8];
	u32 reg_me_debug_pc_smooth0_en  = (me_dbg1 >> 6) & 0x1;//[6];
	u32 reg_me_debug_cn_smooth0_en  = (me_dbg1 >> 5) & 0x1;//[5];
	u32 reg_me_debug_nc_smooth0_en  = (me_dbg1 >> 4) & 0x1;//[4];
	u32 reg_me_debug_pc_smooth1_en  = (me_dbg1 >> 2) & 0x1;//[2];
	u32 reg_me_debug_cn_smooth1_en  = (me_dbg1 >> 1) & 0x1;//[1];
	u32 reg_me_debug_nc_smooth1_en  = (me_dbg1 >> 0) & 0x1;//[0];

	u32 reg_me_debug_pc_smooth2_en    = (me_dbg2 >> 23) & 0x1;//[23];
	u32 reg_me_debug_cn_smooth2_en    = (me_dbg2 >> 22) & 0x1;//[22];
	u32 reg_me_debug_nc_smooth2_en    = (me_dbg2 >> 21) & 0x1;//[21];
	u32 reg_me_debug_pc_smobj_en      = (me_dbg2 >> 20) & 0x1;//[20];
	u32 reg_me_debug_pc_smooth3_en    = (me_dbg2 >> 18) & 0x1;//[18];
	u32 reg_me_debug_cn_smooth3_en    = (me_dbg2 >> 17) & 0x1;//[17];
	u32 reg_me_debug_nc_smooth3_en    = (me_dbg2 >> 16) & 0x1;//[16];
	u32 reg_me_debug_pc_limit_en      = (me_dbg2 >> 14) & 0x1;//[14];
	u32 reg_me_debug_cn_limit_en      = (me_dbg2 >> 13) & 0x1;//[13];
	u32 reg_me_debug_nc_limit_en      = (me_dbg2 >> 12) & 0x1;//[12];
	u32 reg_me_debug_raw_rp_flg_mode  = (me_dbg2 >> 8) & 0x7;//[10: 8];
	u32 reg_me_debug_fine_rp_flg_en   = (me_dbg2 >> 7) & 0x1;//[7];
	u32 reg_me_debug_final_rp_flg_en  = (me_dbg2 >> 6) & 0x1;//[6];
	u32 reg_me_debug_line_flg_en      = (me_dbg2 >> 5) & 0x1;//[5];
	u32 reg_me_debug_map_drt_en       = (me_dbg2 >> 4) & 0x1;//[4];
	u32 reg_me_debug_auto_flat_flg_en = (me_dbg2 >> 2) & 0x1;//[2];
	u32 reg_me_debug_hard_ab_flg_en   = (me_dbg2 >> 1) & 0x1;//[1];
	u32 reg_me_debug_fs_en_flg_en     = (me_dbg2 >> 0) & 0x1;//[0];

	u32 reg_vp_debug_occl_type_en            = (vp_dbg >> 19) & 0x1;//[19];
	u32 reg_vp_debug_cpt_8_cn_flg_en         = (vp_dbg >> 18) & 0x1;//[18];
	u32 reg_vp_debug_cnt_8_cp_flg_en         = (vp_dbg >> 17) & 0x1;//[17];
	u32 reg_vp_debug_retimer_mvs_mode        = (vp_dbg >> 13) & 0xf;//[16:13];
	u32 reg_vp_debug_dehalo_mvs_mode         = (vp_dbg >> 8) & 0x1f;//[12: 8];
	u32 reg_vp_debug_cover_mv_phs_en         = (vp_dbg >> 7) & 0x1;//[7];
	u32 reg_vp_debug_uncov_mv_phs_en         = (vp_dbg >> 6) & 0x1;//[6];
	u32 reg_vp_debug_occl_fhri_en            = (vp_dbg >> 5) & 0x1;//[5];
	u32 reg_vp_debug_type_replace_phs_mv_en  = (vp_dbg >> 4) & 0x1;//[4];
	u32 reg_vp_debug_phs_rp_flg_en           = (vp_dbg >> 3) & 0x1;//[3];
	u32 reg_vp_debug_phs_sobj_flg_en         = (vp_dbg >> 2) & 0x1;//[2];
	u32 reg_mv_debug_var_level_en            = (vp_dbg >> 1) & 0x1;//[1];
	u32 reg_mv_debug_var2_level_en           = (vp_dbg >> 0) & 0x1;//[0];

	u32 reg_mc_debug_show_blksize_en       = (mc_dbg >> 23) & 0x1;//[23];
	u32 reg_mc_debug_show_grid_en          = (mc_dbg >> 22) & 0x1;//[22];
	u32 reg_mc_debug_show_bbd_en           = (mc_dbg >> 21) & 0x1;//[21];
	u32 reg_mc_debug_show_demowindow_mode  = (mc_dbg >> 17) & 0xf;//[20:17];
	u32 reg_mc_debug_show_deflicker_pix_en = (mc_dbg >> 16) & 0x1;//[16];
	u32 reg_mc_debug_show_pts_en           = (mc_dbg >> 15) & 0x1;//[15];
	u32 reg_mc_debug_show_pts_mode         = (mc_dbg >> 12) & 0x7;//[14:12];
	u32 reg_mc_debug_show_ptb_en           = (mc_dbg >> 11) & 0x1;//[11];
	u32 reg_mc_debug_show_ptb_mode         = (mc_dbg >> 8) & 0x7;//[10: 8];
	u32 reg_mc_debug_show_ptl_en           = (mc_dbg >> 7) & 0x1;//[7];
	u32 reg_mc_debug_show_ptl_mode         = (mc_dbg >> 4) & 0x7;//[6: 4];
	u32 reg_mc_debug_show_ptw_en           = (mc_dbg >> 3) & 0x1;//[3];
	u32 reg_mc_debug_show_ptw_mode         = (mc_dbg >> 0) & 0x7;//[2: 0];

	u32 reg_logo_debug_ip_pix_logo_en   = (logo_dbg >> 19) & 0x1;//[19];
	u32 reg_logo_debug_ip_blk_logo_en   = (logo_dbg >> 18) & 0x1;//[18];
	u32 reg_logo_debug_me_blk_logo_en   = (logo_dbg >> 17) & 0x1;//[17];
	u32 reg_logo_debug_mc_logo_en       = (logo_dbg >> 16) & 0x1;//[16];
	u32 reg_iplogo_inner_pxl_debug_en   = (logo_dbg >> 15) & 0x1;//[15];
	u32 reg_iplogo_inner_pxl_debug_mode = (logo_dbg >> 10) & 0x1f;//[14:10];
	u32 reg_iplogo_pxl_fid_mode         = (logo_dbg >> 9) & 0x1;//[9];
	u32 reg_iplogo_inner_blk_debug_en   = (logo_dbg >> 8) & 0x1;//[8];
	u32 reg_iplogo_inner_blk_debug_mode = (logo_dbg >> 3) & 0x1f;//[7: 3];
	u32 reg_melogo_inner_debug_en       = (logo_dbg >> 2) & 0x1;//[2];
	u32 reg_melogo_inner_debug_mode     = (logo_dbg >> 0) & 0x3;//[1: 0];

	u32 reg_mc_debug_oct1_en       = (path_dbg >> 2) & 0x1;//[2];
	u32 reg_mc_debug_disp_pht1_en  = (path_dbg >> 0) & 0x1;//[0];

	u32 mc_mc_dbg = (reg_mc_debug_oct1_en  << 28 | //28
		reg_mc_debug_disp_pht1_en          << 26 | //26
		reg_mv_debug_var_level_en          << 25 | //25
		reg_mv_debug_var2_level_en         << 24 | //24
		reg_mc_debug_show_blksize_en       << 23 | //23
		reg_mc_debug_show_grid_en          << 22 | //22
		reg_mc_debug_show_bbd_en           << 21 | //21
		reg_mc_debug_show_demowindow_mode  << 17 | //20:17
		reg_mc_debug_show_deflicker_pix_en << 16 | //16
		reg_mc_debug_show_pts_en           << 15 | //15
		reg_mc_debug_show_pts_mode         << 12 | //14:12
		reg_mc_debug_show_ptb_en           << 11 | //11
		reg_mc_debug_show_ptb_mode         << 8  | //10:8
		reg_mc_debug_show_ptl_en           << 7  | //7
		reg_mc_debug_show_ptl_mode         << 4  | //6:4
		reg_mc_debug_show_ptw_en           << 3  | //3
		reg_mc_debug_show_ptw_mode         << 0);//2:0
	wr(DPSS_DPE_MC_MC_DBG, mc_mc_dbg);

	u32 mc_me_dbg0 = (reg_me_debug_raw_rp_flg_mode << 29 | //31:29
					reg_me_debug_fine_rp_flg_en   << 28 | //28
					reg_me_debug_final_rp_flg_en  << 27 | //27
					reg_me_debug_line_flg_en      << 26 | //26
					reg_me_debug_map_drt_en       << 25 | //25
					reg_me_debug_auto_flat_flg_en << 24 | //24
					reg_me_debug_hard_ab_flg_en   << 23 | //23
					reg_me_debug_fs_en_flg_en     << 22 | //22
					0 << 20 | //21:20
					0 << 12 | //19:12
					0 << 0);//11:0
	wr(DPSS_DPE_MC_ME_DBG0, mc_me_dbg0);

	u32 mc_me_dbg1 = ((reg_me_debug_pc_prj_mode & 0x1) << 31 | //32
					reg_me_debug_cp_prj_mode      << 28 | //30:28
					reg_me_debug_pc_sad_mode      << 25 | //27:25
					reg_me_debug_cn_sad_mode      << 22 | //24:22
					reg_me_debug_nc_sad_mode      << 19 | //21:19
					reg_me_debug_sad_div          << 16 | //18:16
					reg_me_debug_pc_acdc_flg_en   << 15 | //15
					reg_me_debug_cn_acdc_flg_en   << 14 | //14
					reg_me_debug_nc_acdc_flg_en   << 13 | //13
					reg_me_debug_pc_smooth0_en    << 12 | //12
					reg_me_debug_cn_smooth0_en    << 11 | //11
					reg_me_debug_nc_smooth0_en    << 10 | //10
					reg_me_debug_pc_smooth1_en    << 9  | //9
					reg_me_debug_cn_smooth1_en    << 8  | //8
					reg_me_debug_nc_smooth1_en    << 7  | //7
					reg_me_debug_pc_smooth2_en    << 6  | //6
					reg_me_debug_cn_smooth2_en    << 5  | //5
					reg_me_debug_nc_smooth2_en    << 4  | //4
					reg_me_debug_pc_smobj_en      << 3  | //3
					reg_me_debug_pc_limit_en      << 2  | //2
					reg_me_debug_cn_limit_en      << 1  | //1
					reg_me_debug_nc_limit_en      << 0);//0
	wr(DPSS_DPE_MC_ME_DBG1, mc_me_dbg1);

	u32 mc_me_dbg2 = (reg_me_debug_pc_smooth3_en      << 6  | //6
					reg_me_debug_cn_smooth3_en        << 5  | //5
					reg_me_debug_nc_smooth3_en        << 4  | //4
					reg_me_debug_cn_fs_en             << 3  | //3
					reg_me_debug_nc_fs_en             << 2  | //2
					((reg_me_debug_pc_prj_mode >> 1) & 0x3) << 0);//1:0
	wr(DPSS_DPE_MC_ME_DBG2, mc_me_dbg2);

	u32 mc_vp_dbg = (reg_vp_debug_occl_type_en          << 17 | //17
					reg_vp_debug_cpt_8_cn_flg_en        << 16 | //16
					reg_vp_debug_cnt_8_cp_flg_en        << 15 | //15
					reg_vp_debug_retimer_mvs_mode       << 11 | //14:11
					reg_vp_debug_dehalo_mvs_mode        << 6  | //10:6
					reg_vp_debug_cover_mv_phs_en        << 5  | //5
					reg_vp_debug_uncov_mv_phs_en        << 4  | //4
					reg_vp_debug_occl_fhri_en           << 3  | //3
					reg_vp_debug_type_replace_phs_mv_en << 2  | //2
					reg_vp_debug_phs_rp_flg_en          << 1  | //1
					reg_vp_debug_phs_sobj_flg_en        << 0);//0

	wr(DPSS_DPE_MC_VP_DBG, mc_vp_dbg);

	u32 mc_logo_dbg = (reg_logo_debug_ip_pix_logo_en   << 19 | //19
					reg_logo_debug_ip_blk_logo_en   << 18 | //18
					reg_logo_debug_me_blk_logo_en   << 17 | //17
					reg_logo_debug_mc_logo_en       << 16 | //16
					reg_iplogo_inner_pxl_debug_en   << 15 | //15
					reg_iplogo_inner_pxl_debug_mode << 10 | //14:10
					reg_iplogo_pxl_fid_mode         << 9  | //9
					reg_iplogo_inner_blk_debug_en   << 8  | //8
					reg_iplogo_inner_blk_debug_mode << 3  | //7:3
					reg_melogo_inner_debug_en       << 2  | //2
					reg_melogo_inner_debug_mode     << 0);//1:0

	wr(DPSS_DPE_MC_LOGO_DBG, mc_logo_dbg);
	dbg_h2("%s mc_logo:%d\n", __func__, mc_logo_dbg);
}

void hw_afbcd_420_mc_bypass_cfg(struct PRM_DPSS_TOP *prm_top)
{
	u32 mc_bypass = 0;
	static char mc_bypass_pre;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st) {
		dbg_h0("%s pchip_st is null\n", __func__);
		return;
	}
	if (prm_top->src_fs_fmt.src_fmt == YUV420 &&
			prm_top->src_fs_fmt.src_cmpr == CMPR_AFBC) {
		mc_bypass  = rd(FRC_MC_HW_CTRL0) & 0x1;
		if (mc_bypass != mc_bypass_pre) {
			if (mc_bypass) {
				prm_top->frc_vfcd_vfmt = 1;
				hw_cfg_dpss_mc_intf(prm_top, &pchip_st->prm_mc);
			} else {
				prm_top->frc_vfcd_vfmt = 0;
				hw_cfg_dpss_mc_intf(prm_top, &pchip_st->prm_mc);
			}
			mc_bypass_pre = mc_bypass;
		}
	}
}

void hw_cfg_dae_ofrm_step_at_dae_site(u32 dae_ofrm_step)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	//dbg_h2("<config_dae_ofrm_step_at_dae_site>: dae_ofrm_step : %d\n",dae_ofrm_step);
	if (prm_top->fw_en == 0)
		w_reg_bit(FRC_REG_TOP_CTRL7, dae_ofrm_step, 0, 20);
}

void hw_config_dae_loop_tab(enum DPSS_FRC_RATIO frc_ratio, enum DPSS_FILM_MODE film_mode,
			 u32 cfg_at_inp_site)
{
//==============================================================//
// FILM_MODE
//==============================================================//
	u32 irn = 1, irm = 1, frn, frm, input_n, output_m;
	u32 inp_ofrm_step;
	//u32 force_film_mode = rd(FRC_REG_TOP_RESERVE1 );
	u32 dae_step_sel = (rd(FRC_REG_TOP_CTRL7) >> 20) & 0x3;

	dbg_h2("%s frc_ratio=%d, film_mode=%d, cfg_at_inp_site=%d\n", __func__,
	       frc_ratio, film_mode, cfg_at_inp_site);

	get_n2m_value_from_frc_ratio(frc_ratio, &irn, &irm);

	switch (film_mode) {
	case DPSS_VIDEO:
		frn = 1;
		frm = 1;
		break;
	case DPSS_FILM22:
		frn = 1;
		frm = 2;
		break;
	case DPSS_FILM32:
		frn = 2;
		frm = 5;
		break;
	case DPSS_FILM3223:
		frn = 4;
		frm = 10;
		break;
	case DPSS_FILM2224:
		frn = 4;
		frm = 10;
		break;
	case DPSS_FILM32322:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM44:
		frn = 1;
		frm = 4;
		break;
	case DPSS_FILM21111:
		frn = 5;
		frm = 6;
		break;
	case DPSS_FILM23322:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM2111:
		frn = 4;
		frm = 5;
		break;
	case DPSS_FILM22224:
		frn = 5;
		frm = 12;
		break;
	case DPSS_FILM33:
		frn = 1;
		frm = 3;
		break;
	case DPSS_FILM334:
		frn = 3;
		frm = 10;
		break;
	case DPSS_FILM55:
		frn = 1;
		frm = 5;
		break;
	case DPSS_FILM64:
		frn = 2;
		frm = 10;
		break;
	case DPSS_FILM66:
		frn = 1;
		frm = 6;
		break;
	case DPSS_FILM87:
		frn = 2;
		frm = 15;
		break;
	case DPSS_FILM212:
		frn = 3;
		frm = 5;
		break;
	default:
		frn = 1;
		frm = 1;
		break;
	}
	input_n = irn * frn;
	output_m = irm * frm;
	//for C RUN,RTL don't need
	w_reg_bit(FRC_REG_PHS_TABLE, input_n, 24, 8);
	w_reg_bit(FRC_REG_PHS_TABLE, output_m, 16, 8);
	w_reg_bit(FRC_REG_OUT_FID, output_m, 24, 8);
	w_reg_bit(FRC_REG_PD_PAT_NUM, output_m, 0, 8);

	if (cfg_at_inp_site == 0 && dae_step_sel == 2) {
		inp_ofrm_step = ((input_n * 128 << 16) / output_m) >> 4;//;inp_ofrm_step_f>>16;
		hw_cfg_dae_ofrm_step_at_dae_site(inp_ofrm_step);
		wr(FRC_AUTO_MODE_LOOP_CTRL, ((output_m & 0xff) << 8) |	//reg_inp_ofrm_loop_num
		   ((output_m & 0x1f) << 0));	//reg_dae_loop_step_num

		dbg_h2("dae_ofrm inp_ofrm_step=%d\n", inp_ofrm_step);
	}
}

void frc_mc_cut_0(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr)
{
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	prm_mc = &pchip->prm_mc;
	hw_cfg_dpss_mc0(prm_top, src_from_nr, prm_mc);
	hw_cfg_dpss_mc_intf(prm_top, prm_mc);
}

void frc_mc_cut_1(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr)
{
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;
	struct PRM_INTF_TYPE *pix_rmif;
	struct VFCD_t *vfcd;
	bool cut_win_en;
	u32 win_hsize;
	u32 win_vsize;
	u32 frm_hsize;
	u32 frm_vsize;
	u32 mc_size_sel;
	u32 mc_skip_mode;
	u32 tmp_reg_value;
	u32 src_hsize;	  //= prm_top.org_hsize;
	u32 src_vsize;	  //= prm_top.org_vsize;
	u32 fmt;
	u32 fmt444_out;
	u32 regs_ofst;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	pix_rmif = &pchip->mc_pix_rmif;
	vfcd = &pchip->mc_vfcd;
	prm_mc = &pchip->prm_mc;
	fmt		   = prm_mc->pix_fmt == YUV444 ? 0 : prm_mc->pix_fmt == YUV422 ? 1 : 2;
	fmt444_out = ((prm_top->frc_vfcd_vfmt == 1) && (fmt == 2)) ? 2 : 0;

	mc_skip_mode = prm_top->mc_skip_mode;
	cut_win_en = prm_top->cut_win_en;
	win_hsize	 = prm_top->prm_cut_win.win_hsize;
	win_vsize	 = prm_top->prm_cut_win.win_vsize;
	mc_size_sel = prm_top->nr_hscale_on || prm_top->nr_aapps_up ||
				prm_top->frc_ds_scale_en || mc_skip_mode;

	frm_hsize = cut_win_en ? win_hsize : mc_size_sel ?
		prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
	frm_vsize = cut_win_en ? win_vsize : mc_size_sel ?
		prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;
	frm_hsize = frm_hsize > 1920 ? (frm_hsize + 15) / 16 * 16 :
		(frm_hsize + 7) / 8 * 8;

	if (prm_mc->pad_en && prm_top->org_hsize != 0xffff) {
		src_hsize = cut_win_en	  ? win_hsize
					: mc_size_sel ? prm_top->dpe_mc_size.src_hsize
								  : prm_top->org_hsize;
		src_vsize = cut_win_en	  ? win_vsize
					: mc_size_sel ? prm_top->dpe_mc_size.src_vsize
								  : prm_top->org_vsize;
	} else {
		src_hsize = cut_win_en	  ? win_hsize
					: mc_size_sel ? prm_top->dpe_mc_size.frm_hsize
								  : prm_top->frm_hsize;
		src_vsize = cut_win_en	  ? win_vsize
					: mc_size_sel ? prm_top->dpe_mc_size.frm_vsize
								  : prm_top->frm_vsize;
	}

	u32 me_dsx;
	u32 me_dsy;
	u32 me_hsize;
	u32 me_vsize;
	u32 me_blk_hsize;
	u32 me_blk_vsize;

	if (prm_top->dpss_mode == DPSS_FRC_MODE) {
		me_dsx = prm_top->dae_dsx_scale;
		me_dsy = prm_top->dae_dsy_scale;
	} else {
		me_dsx = prm_top->dpe_dw_dsx;
		me_dsy = prm_top->dpe_dw_dsy;
	}
	me_hsize	 = (frm_hsize + (1 << me_dsx) - 1) >> me_dsx;
	me_vsize	 = (frm_vsize + (1 << me_dsy) - 1) >> me_dsy;
	me_blk_hsize = (me_hsize + 3) >> 2;
	me_blk_vsize = (me_vsize + 3) >> 2;

	//w_reg_bit(DPSS_DPE_MC_BLKBAR_X, src_hsize - 1, 16, 13); // reg_blackbar_xyxy2
	//w_reg_bit(DPSS_DPE_MC_BLKBAR_X, 0, 0, 13);		// reg_blackbar_xyxy0
	//w_reg_bit(DPSS_DPE_MC_BLKBAR_Y, src_vsize - 1, 16, 13); // reg_blackbar_xyxy3
	//w_reg_bit(DPSS_DPE_MC_BLKBAR_Y, 0, 0, 13);	// reg_blackbar_xyxy1
	tmp_reg_value = (src_hsize - 1) << 16;
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_BLKBAR_X, tmp_reg_value);
	tmp_reg_value = (src_vsize - 1) << 16;
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_BLKBAR_Y, tmp_reg_value);

	//wr(DPSS_DPE_MC_TOP_FSIZE, (frm_hsize << 16) | frm_vsize);
	//wr(DPSS_DPE_MC_SRC_FSIZE, (src_hsize << 16) | src_vsize);
	tmp_reg_value = frm_hsize << 16 | frm_vsize;
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_TOP_FSIZE, tmp_reg_value);
	tmp_reg_value = src_hsize << 16 | src_vsize;
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_SRC_FSIZE, tmp_reg_value);

	//hw_cfg_dpss_mc_slice(frm_hsize, frm_vsize, 1);	 // slc_num);
	//w_reg_bit(FRC_MC_SLC_CTRL3, mc_slice_x_end[0], 0, 16);	 // reg_slc0_xend
	tmp_reg_value = frm_hsize - 1;
	DPSS_RDMA_WR_VS(FRC_MC_SLC_CTRL3, tmp_reg_value);

	//hw_cfg_dpss_mc_bbd(frm_hsize, frm_vsize, bbd_en, 0x0, src_hsize, src_vsize);
	//w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (src_hsize >> me_dsx) - 1, 16,
	//			16);   // reg_mc_org_me_blk_bb_xyxy_2  //org me size
	//w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, (src_vsize >> me_dsy) - 1, 0,
	//			16);   // reg_mc_org_me_blk_bb_xyxy_3   //org me size
	tmp_reg_value = ((src_hsize >> me_dsx) - 1) << 16 | ((src_vsize >> me_dsy) - 1);
	DPSS_RDMA_WR_VS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT, tmp_reg_value);
	//w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT,
	//	((src_hsize >> me_dsx) >> 2) - 1, 16, 16);   // reg_mc_org_me_blk_bb_xyxy_2
	//w_reg_bit(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT,
	//	((src_vsize >> me_dsy) >> 2) - 1, 0, 16);	  // reg_mc_org_me_blk_bb_xyxy_3
	tmp_reg_value = (((src_hsize >> me_dsx) >> 2) - 1) << 16;
	tmp_reg_value |= (((src_vsize >> me_dsy) >> 2) - 1);
	DPSS_RDMA_WR_VS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT, tmp_reg_value);
	//w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_hsize - 1, 16,
	//			16);   // reg_mc_me_blk_bb_xyxy_2
	//w_reg_bit(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, me_blk_vsize - 1, 0,
	//			16);   // reg_mc_me_blk_bb_xyxy_3
	tmp_reg_value = ((me_blk_hsize - 1) << 16) | (me_blk_vsize - 1);
	DPSS_RDMA_WR_VS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT, tmp_reg_value);

	//w_reg_bit(FRC_MC_WRAP_BB_1, src_hsize - 1, 16, 13); // reg_blackbar_xyxy2
	//w_reg_bit(FRC_MC_WRAP_BB_1, src_vsize - 1, 0, 13);	// reg_blackbar_xyxy3
	tmp_reg_value = (src_hsize - 1) << 16 | (src_vsize - 1);
	DPSS_RDMA_WR_VS(FRC_MC_WRAP_BB_1, tmp_reg_value);

	//hw_cfg_dpss_mc_intf(prm_top, prm_mc); // below implement
	pix_rmif->src_hsize = cut_win_en ? prm_top->prm_cut_win.frm_hsize :
		prm_top->frm_hsize;
	pix_rmif->src_vsize = cut_win_en ? prm_top->prm_cut_win.frm_vsize :
		prm_top->frm_vsize;

	pix_rmif->slc_x_st[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn_align : 0;
	pix_rmif->slc_x_ed[0] =
		cut_win_en ? prm_top->prm_cut_win.win_hend_align : prm_top->frm_hsize - 1;
	pix_rmif->slc_y_st[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn_align : 0;
	pix_rmif->slc_y_ed[0] =
		cut_win_en ? prm_top->prm_cut_win.win_vend_align : prm_top->frm_vsize - 1;
	//pix_rmif->skip_line = mc_skip_mode_v;
	pix_rmif->cut_win_en	= cut_win_en;

	if (pps->pps_en) {
		pix_rmif->src_hsize = cut_win_en ?
				prm_top->prm_cut_win.frm_hsize : prm_mc->frm_hsize;
		pix_rmif->src_vsize = cut_win_en ?
				prm_top->prm_cut_win.frm_vsize : prm_mc->frm_vsize;
		pix_rmif->slc_x_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_hend_align : prm_mc->frm_hsize - 1;
		pix_rmif->slc_y_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_vend_align : prm_mc->frm_vsize - 1;
	}

	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, fmt444_out);
	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif, fmt444_out);
	dbg_h2("%s src_bit %d pix_bit %d\n", __func__, pix_rmif->src_bit, prm_mc->pix_bit);
	dbg_h2("%s update mif\n", __func__);
	vfcd->cut_win_en   = cut_win_en;
	vfcd->src_hsize	   = cut_win_en ? prm_top->prm_cut_win.frm_hsize : prm_top->frm_hsize;
	vfcd->src_vsize	   = cut_win_en ? prm_top->prm_cut_win.frm_vsize : prm_top->frm_vsize;
	vfcd->fmt444_out   = fmt444_out;
	vfcd->win_bgn_h[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn_align : 0;
	vfcd->win_end_h[0] =
		cut_win_en ? prm_top->prm_cut_win.win_hend_align : prm_top->frm_hsize - 1;
	vfcd->win_bgn_v[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn_align : 0;
	vfcd->win_end_v[0] =
		cut_win_en ? prm_top->prm_cut_win.win_vend_align : prm_top->frm_vsize - 1;

	if (pps->pps_en) {
		vfcd->src_hsize    = cut_win_en ?
			prm_top->prm_cut_win.frm_hsize : prm_mc->frm_hsize;
		vfcd->src_vsize    = cut_win_en ?
			prm_top->prm_cut_win.frm_vsize : prm_mc->frm_vsize;
		vfcd->win_end_h[0] = cut_win_en ?
			prm_top->prm_cut_win.win_hend_align : prm_mc->frm_hsize - 1;
		vfcd->win_end_v[0] = cut_win_en ?
			prm_top->prm_cut_win.win_vend_align : prm_mc->frm_vsize - 1;
	}
	dbg_h2("%s vfcd->src_h/v size:(%d,%d)\n", __func__, vfcd->src_hsize, vfcd->src_vsize);
	dbg_h2("%s pix_rmif.slc_x:(%d,%d), .slc_y:(%d,%d)\n", __func__, vfcd->win_bgn_h[0],
		   vfcd->win_end_h[0], vfcd->win_bgn_v[0], vfcd->win_end_v[0]);
	frc_cfg_vfcd_dec(4, vfcd);	 // mc0
	frc_cfg_vfcd_dec(5, vfcd);	 // mc1

	if (prm_mc->src_cmpr == CMPR_AFBC) {
		frc_cfg_vfcd_cmpr_enable(4, 1);
		frc_cfg_vfcd_cmpr_enable(5, 1);
		VSYNC_WR_VIDEO_TABLE_REG(4 * 256 + VFCD_TOP_CTRL3, 0xa);
		VSYNC_WR_VIDEO_TABLE_REG(5 * 256 + VFCD_TOP_CTRL3, 0xa);
	} else if (prm_mc->src_cmpr == CMPR_AFRC) {
		frc_cfg_vfcd_cmpr_enable(4, 2);
		frc_cfg_vfcd_cmpr_enable(5, 2);
	} else {
		frc_cfg_vfcd_cmpr_enable(4, 0);
		frc_cfg_vfcd_cmpr_enable(5, 0);
	}

	regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_RPT_LOOP + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F0_LUMA_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F0_CHRM_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F1_LUMA_RPT_PAT + regs_ofst, 0);
	VSYNC_WR_VIDEO_TABLE_REG(RMIF_F1_CHRM_RPT_PAT + regs_ofst, 0);

	//afrc not need cfg 444_comb
	VSYNC_WR_VIDEO_TABLE_REG_BITS(VFCD_AFBC_ENABLE + regs_ofst, 0, 20, 1);

	frc_me_cut(prm_top);
}

void hw_update_display_info(struct display_buffer_info_s *display_buf_info)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct me_pcn_s *me_pcn_st;

	if (!pchip_st || !display_buf_info) {
		dbg_h0("%s pchip_st is null\n", __func__);
		return;
	}
	state_st = &pchip_st->state_st;
	me_pcn_st = &state_st->me_pcn_st;
	display_buf_info->dae_mix = (rd(FRC_REG_CURSOR) & 0x7) == 7 ? 1 : 0;
	display_buf_info->p = me_pcn_st->p;
	display_buf_info->c = me_pcn_st->c;
	display_buf_info->n = me_pcn_st->n;
	display_buf_info->logo_pre = rd(FRC_REG_OUT_FID) >> 4 & 0xf;
	display_buf_info->logo_cur = rd(FRC_REG_OUT_FID) & 0xf;
	display_buf_info->mc_phase = rd(FRC_REG_OUT_PHS) >> 12 & 0xfff;
	if (display_buf_info->dae_mix) {
		if (state_st->mv_buf_idx == 0)
			state_st->mv_buf_idx = DPSS_SML_NUB - 1;
		else
			state_st->mv_buf_idx--;
	}
	display_buf_info->mv_buf = pchip_st->state_st.mv_buf_idx;
	state_st->mv_buf_idx = (state_st->mv_buf_idx + 1) % DPSS_SML_NUB;
	display_buf_info->frame_index = state_st->display_buf_info_cnt++;

	pr_frc(2, "%s:dae_mix=%d, pcn=(%d,%d,%d), logo=(%d,%d,%d), mc_phase=%3d, frame_idx=%8d\n",
		"set_disp_info", display_buf_info->dae_mix, display_buf_info->p,
		display_buf_info->c, display_buf_info->n, display_buf_info->logo_pre,
		display_buf_info->logo_cur, display_buf_info->mv_buf, display_buf_info->mc_phase,
		display_buf_info->frame_index);
}

void hw_mc_reverse_update(void)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;
	struct PRM_INTF_TYPE *pix_rmif;
	struct VFCD_t *vfcd;

	if (!pchip_st)
		return;
	if (!pchip_st->state_st.reverse_need_update)
		return;
	prm_mc = &pchip_st->prm_mc;
	pix_rmif = &pchip_st->mc_pix_rmif;
	pix_rmif->reverse[0]  = prm_mc->mc_rev_mode & 0x1;
	pix_rmif->reverse[1]  = (prm_mc->mc_rev_mode & 0x2) >> 1;
	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, pchip_st->fmt444_out);
	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif, pchip_st->fmt444_out);

	vfcd = &pchip_st->mc_vfcd;
	vfcd->rev_mode	  = prm_mc->mc_rev_mode;
	frc_cfg_vfcd_dec(4, vfcd);   // mc0
	frc_cfg_vfcd_dec(5, vfcd);   // mc1
	if (prm_mc->src_cmpr == CMPR_AFBC) {
		frc_cfg_vfcd_cmpr_enable(4, 1);
		frc_cfg_vfcd_cmpr_enable(5, 1);
		VSYNC_WR_VIDEO_TABLE_REG(4 * 256 + VFCD_TOP_CTRL3, 0xa);
		VSYNC_WR_VIDEO_TABLE_REG(5 * 256 + VFCD_TOP_CTRL3, 0xa);
	} else if (prm_mc->src_cmpr == CMPR_AFRC) {
		frc_cfg_vfcd_cmpr_enable(4, 2);
		frc_cfg_vfcd_cmpr_enable(5, 2);
	} else {
		frc_cfg_vfcd_cmpr_enable(4, 0);
		frc_cfg_vfcd_cmpr_enable(5, 0);
	}
	pchip_st->state_st.reverse_need_update = false;
	dbg_h2("mc reverse update vfcd done\n");
}

