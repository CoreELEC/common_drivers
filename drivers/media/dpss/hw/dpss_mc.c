// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include "../sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#endif
#include "../dpss_base.h"
#include "../dpss_hw.h"
// #include "../dpss_hw_frc.h"
#include "../dpss_s.h"
#include "../dpss_sys.h"
#include "../dpss_rdma_frc.h"
#include "../dpss_hw_frc.h"
#include "../dpss_s_frc.h"
#include "../dpss_func.h"

#include "dpss.h"
#include "dpss_mc.h"
#include "define.h"
#include "dpss_intf.h"
#include "vfcd.h"
#include "dpss_param.h"

int mc_pre_triger;
int mc_trigger;

struct PRM_INTF_TYPE pre_logo_rmif;
struct PRM_INTF_TYPE cur_logo_rmif;
struct PRM_INTF_TYPE mv_rmif;
struct PRM_INTF_TYPE me_logo_rmif;

void cfg_dpss_mc_ini(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr)
{
	struct frc_chip_st *pchip = dpss_get_frc_st();
	struct DPSS_MC0_TYPE *prm_mc;
	struct frc_cut_win_s *frc_win;

	if (!pchip) {
		dbg_h2("%s pchip is null\n", __func__);
		return;
	}
	prm_mc = &pchip->prm_mc;
	frc_win = &pchip->win_st;
	frc_win->win_hbgn = 0;
	frc_win->win_hend = 0;
	frc_win->win_vbgn = 0;
	frc_win->win_vend = 0;
	cfg_dpss_mc0(prm_top, src_from_nr, prm_mc);
	cfg_dpss_mc_intf(prm_top, prm_mc);
}

void cfg_dpss_mc_intf(struct PRM_DPSS_TOP *prm_top, struct DPSS_MC0_TYPE *prm_mc)
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

	pix_rmif->slc_x_st[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn : 0;
	pix_rmif->slc_x_ed[0] = cut_win_en ? prm_top->prm_cut_win.win_hend :
		prm_top->frm_hsize - 1;
	//pix_rmif->slc_x_ed[0] = cut_win_en ? prm_top->prm_cut_win.win_hend : frm_hsize - 1;
	pix_rmif->slc_y_st[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn : 0;
	pix_rmif->slc_y_ed[0] = cut_win_en ? prm_top->prm_cut_win.win_vend :
		prm_top->frm_vsize - 1;
	//pix_rmif->slc_y_ed[0] = cut_win_en ? prm_top->prm_cut_win.win_vend : frm_vsize - 1;
	pix_rmif->skip_line = mc_skip_mode_v;
	pix_rmif->cut_win_en	= cut_win_en;

	if (pps->pps_en) {
		pix_rmif->src_hsize = cut_win_en ?
				prm_top->prm_cut_win.frm_hsize : prm_mc->frm_hsize;
		pix_rmif->src_vsize = cut_win_en ?
				prm_top->prm_cut_win.frm_vsize : prm_mc->frm_vsize;
		pix_rmif->slc_x_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_hend : prm_mc->frm_hsize - 1;
		pix_rmif->slc_y_ed[0] = cut_win_en ?
				prm_top->prm_cut_win.win_vend : prm_mc->frm_vsize - 1;
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
	if (prm_top->is_dpss_init_done)
		frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, fmt444_out);
	frc_cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif, fmt444_out);
	if (mc_skip_mode_h) {
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC0);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
		regs_ofst = get_vfcd_regs_ofst(DPSS_RMIF_MC1);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(RMIF_TOP_CTRL + regs_ofst, 1, 13, 1);
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
	vfcd->win_bgn_h[0] = cut_win_en ? prm_top->prm_cut_win.win_hbgn : 0;
	vfcd->win_end_h[0] = cut_win_en ? prm_top->prm_cut_win.win_hend : prm_top->frm_hsize - 1;
	vfcd->win_bgn_v[0] = cut_win_en ? prm_top->prm_cut_win.win_vbgn : 0;
	vfcd->win_end_v[0] = cut_win_en ? prm_top->prm_cut_win.win_vend : prm_top->frm_vsize - 1;
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
			prm_top->prm_cut_win.win_hend : prm_mc->frm_hsize - 1;
		vfcd->win_end_v[0] = cut_win_en ?
			prm_top->prm_cut_win.win_vend : prm_mc->frm_vsize - 1;
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

	if (prm_top->is_dpss_init_done)
		frc_cfg_vfcd_dec(4, vfcd);	 // mc0
	frc_cfg_vfcd_dec(5, vfcd);	 // mc1

	if (prm_top->is_dpss_init_done) {
		// mc din vfcd mode
		if (prm_mc->src_cmpr == CMPR_AFBC) {
			frc_cfg_vfcd_cmpr_enable(4, 1);	  // vfcd_chose cur_afbc
			frc_cfg_vfcd_cmpr_enable(5, 1);	  // vfcd_chose pre_afbc
			VSYNC_WR_VIDEO_TABLE_REG(4 * 256 + VFCD_TOP_CTRL3, 0xa);
			VSYNC_WR_VIDEO_TABLE_REG(5 * 256 + VFCD_TOP_CTRL3, 0xa);
		} else if (prm_mc->src_cmpr == CMPR_AFRC) {
			frc_cfg_vfcd_cmpr_enable(4, 2);	  // vfcd_chose cur_afbc
			frc_cfg_vfcd_cmpr_enable(5, 2);	  // vfcd_chose pre_afbc
		} else {
			frc_cfg_vfcd_cmpr_enable(4, 0);	  // vfcd_chose cur_mif
			frc_cfg_vfcd_cmpr_enable(5, 0);	  // vfcd_chose pre_mif
		}
	}

	frc_me_cut(prm_top);
}

//cfg @pre_go_field
void cfg_dpss_mc0(struct PRM_DPSS_TOP *prm_top, u32 src_from_nr,
	     struct DPSS_MC0_TYPE *prm_mc)
{
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
		prm_mc->pad_hmode  = 1;
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
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, 0, 0, 2);// reg_mc_gcb_bypass_mc_en
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, blk_xscale, 8, 3);// reg_mc_blksize_xscale
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, blk_yscale, 4, 3);// reg_mc_blksize_yscale

	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_BLKBAR_X, src_hsize - 1, 16, 13);// reg_blackbar_xyxy2
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_BLKBAR_X, 0, 0, 13);// reg_blackbar_xyxy0
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_BLKBAR_Y, src_vsize - 1, 16, 13);// reg_blackbar_xyxy3
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_BLKBAR_Y, 0, 0, 13);// reg_blackbar_xyxy1

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
	dbg_h2("debug_0 frm_size= [%d, %d], src_size= [%d, %d]\n",
			frm_hsize, frm_vsize, src_hsize, src_vsize);
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_TOP_FSIZE, (frm_hsize << 16) | frm_vsize);
	DPSS_RDMA_WR_VS(DPSS_DPE_MC_SRC_FSIZE, (src_hsize << 16) | src_vsize);

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

	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, blk_mode & 0x3, 28, 2);
	// w_reg_bit(DPSS_DPE_MC_CTRL0, phase&0xff, 20, 8);
	//w_reg_bit(DPSS_DPE_MC_CTRL0, mvx_div_mode & 0x3, 16, 2);
	//w_reg_bit(DPSS_DPE_MC_CTRL0, mvy_div_mode & 0x3, 12, 2);
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, 2, 16, 2);
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, 2, 12, 2);
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, blk_xscale & 0x7, 8, 3);
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_CTRL0, blk_yscale & 0x7, 4, 3);
	// w_reg_bit(DPSS_DPE_MC_CTRL0, gcb_bypass&0x3, 0, 2);

	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_MIF_CTRL0, 1, 0,
				1);	 // reg_mode bit0, ctrl mc_top signals like baddr/phase
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_MIF_CTRL0, 0xff, 20, 8);// rdmif_enable

	if (pix_fmt == YUV422)
		DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_MIF_CTRL0, 1, 16, 2);
	else if (pix_fmt == YUV420)
		DPSS_RDMA_WR_BIT_VS(DPSS_DPE_MC_MIF_CTRL0, 2, 16, 2);
	else
		dbg_h2("ERROR: MC CONFIG WRONG FMT\n");

	// config_mc_slice
	cfg_dpss_mc_slice(frm_hsize, frm_vsize, 1);	 // slc_num);
	cfg_dpss_mc_bbd(frm_hsize, frm_vsize, bbd_en, 0x0, src_hsize, src_vsize);

	//
	if (me_dsx == 2) {
		DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING2, 9, 8, 6);  // reg_mc_fetch_size
		DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING2, 16, 0, 8); // reg_mc_blk_x
	} else {
		DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING2, 5, 8, 6);// reg_mc_fetch_size
		DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING2, 8, 0, 8);// reg_mc_blk_x
	}
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING1, 0, 24, 1);// reg_mc_bb_inner_en
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING1, me_dsx, 8, 4);// reg_mc_mvx_scale
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SETTING1, me_dsy, 0, 4);// reg_mc_mvy_scale

	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP, 0, 16,
				16);// reg_mc_org_me_blk_bb_xyxy_0
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_LEFT_TOP,
		0, 0, 16);// reg_mc_org_me_blk_bb_xyxy_1
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT,
		(src_hsize >> me_dsx) - 1, 16, 16);//reg_mc_org_me_blk_bb_xyxy_2//org me size
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BB_XYXY_RIGHT_BOT,
		(src_vsize >> me_dsy) - 1, 0, 16);//reg_mc_org_me_blk_bb_xyxy_3//org me size
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 16,
				16);// reg_mc_org_me_blk_bb_xyxy_0
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_LFT_AND_TOP, 0, 0,
				16);// reg_mc_org_me_blk_bb_xyxy_1
	DPSS_RDMA_WR_BIT_VS(//reg_mc_org_me_blk_bb_xyxy_2//org blk size
		FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT,
		((src_hsize >> me_dsx) >> 2) - 1, 16, 16);
	DPSS_RDMA_WR_BIT_VS(//reg_mc_org_me_blk_bb_xyxy_3//org blk size
		FRC_MC_BB_HANDLE_ORG_ME_BLK_BB_XYXY_RIT_AND_BOT,
		((src_vsize >> me_dsy) >> 2) - 1, 0, 16);
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_LFT_AND_TOP,
		0, 16, 16);// reg_mc_me_blk_bb_xyxy_0
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_LFT_AND_TOP,
		0, 0, 16);// reg_mc_me_blk_bb_xyxy_1
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT,
		me_blk_hsize - 1, 16, 16);// reg_mc_me_blk_bb_xyxy_2
	DPSS_RDMA_WR_BIT_VS(FRC_MC_BB_HANDLE_ME_BLK_BB_XYXY_RIT_AND_BOT,
		me_blk_vsize - 1, 0, 16);// reg_mc_me_blk_bb_xyxy_3

	DPSS_RDMA_WR_BIT_VS(FRC_MC_WRAP_BB_0, 0, 16, 13);// reg_blackbar_xyxy0
	DPSS_RDMA_WR_BIT_VS(FRC_MC_WRAP_BB_0, 0, 0, 13);// reg_blackbar_xyxy1
	DPSS_RDMA_WR_BIT_VS(FRC_MC_WRAP_BB_1, src_hsize - 1, 16, 13);// reg_blackbar_xyxy2
	DPSS_RDMA_WR_BIT_VS(FRC_MC_WRAP_BB_1, src_vsize - 1, 0, 13);// reg_blackbar_xyxy3

	DPSS_RDMA_WR_BIT_VS(FRC_NOW_SRCH_REG, 64, 8, 8);// reg_mc_vsrch_rng_luma //todo
	DPSS_RDMA_WR_BIT_VS(FRC_NOW_SRCH_REG, 32, 0, 8);// reg_mc_vsrch_rng_chrm

	DPSS_RDMA_WR_BIT_VS(FRC_MC_FORCE_BV, 0, 0, 1);// reg_mc_force_bv_en
	DPSS_RDMA_WR_BIT_VS(FRC_MC_FORCE_BV, 0, 16, 13);// reg_mc_force_mvx
	DPSS_RDMA_WR_BIT_VS(FRC_MC_FORCE_BV, 0, 4, 12);	// reg_mc_force_mvy

	DPSS_RDMA_WR_BIT_VS(FRC_SRCH_RNG_MODE, prm_top->mc_setting.chrm_srng_mode, 0, 4);
	DPSS_RDMA_WR_BIT_VS(FRC_SRCH_RNG_MODE, prm_top->mc_setting.luma_srng_mode, 4, 4);
	if (pix_fmt_src == YUV420 && pix_cmpr == CMPR_AFBC) {// add afbc
		if (prm_top->mc_setting.chrm_srng_mode < SRNG_NORMAL_V2) { //||
			 //prm_top.mc_setting.luma_srng_mode < SRNG_NORMAL_V2)
			// dbg_h2("ERROR: mc srng mode set error\n");
			dbg_h2("Just force to set SRNG_NORMAL_V2\n");
			DPSS_RDMA_WR_BIT_VS(FRC_SRCH_RNG_MODE, SRNG_NORMAL_V2, 0, 4);
			//            w_reg_bit(FRC_SRCH_RNG_MODE, SRNG_NORMAL_V2, 4, 4);
		}
		DPSS_RDMA_WR_BIT_VS(FRC_MC_H2V2_SETTING, 0, 31,
					1);
		DPSS_RDMA_WR_BIT_VS(FRC_MC_H2V2_SETTING, 1, 24, 1);// reg_mc_srch_rng_chrm_scale_en
		DPSS_RDMA_WR_BIT_VS(FRC_MC_H2V2_SETTING,
			1, 19, 1);// reg_mc_srch_rng_chrm_blw_yscale
		DPSS_RDMA_WR_BIT_VS(FRC_MC_H2V2_SETTING,
			1, 21, 1);// reg_mc_srch_rng_chrm_abv_yscale
		// setting search range
		for (i = 0; i < 18; i = i + 1) {
			if (i % 2 == 0) {
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -8, 0,
							9);	  // pre_chrm	//ary addr change *4
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 16,
							9);	  // pre_luma	//ary addr change *4
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 8, 0,
							9);	  // cur_chrm	//ary addr change *4
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 16,
							9);	  // cur_luma	//ary addr change *4
			} else {
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0),
					-4, 0, 9);
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0),
					-8, 16, 9);
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1),
					4, 0, 9);
				DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1),
					8, 16, 9);
			}
		}
	}
	if (pix_fmt_src == YUV422 && pix_cmpr == CMPR_AFBC) {	// add afbc
		// setting search range
		for (i = 0; i < 18; i = i + 1) {
			DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 0,
						9);	  // pre_chrm	//ary addr change *4
			DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 0), -16, 16,
						9);	  // pre_luma	//ary addr change *4
			DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 0,
						9);	  // cur_chrm	//ary addr change *4
			DPSS_RDMA_WR_BIT_VS(FRC_MC_RANGE_NORM_LUT_0 + (2 * i + 1), 16, 16,
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
		DPSS_RDMA_WR_BIT_VS(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		DPSS_RDMA_WR_BIT_VS(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_4k1k_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	if (prm_top->nr_aapps_up ||
		(nr_aapps_4k1k_en && prm_top->dolby_mode != DOLBY_DPSS_PRL_MODE))
		DPSS_RDMA_WR_BIT_VS(DPSS_FBUF_TOP_CTRL, 1, 3, 1);	// dpe_pps_en
	else
		DPSS_RDMA_WR_BIT_VS(DPSS_FBUF_TOP_CTRL, 0, 3, 1);
	dbg_h2("%s me_size(%d, %d), me_blk(%d, %d)\n", __func__,
	       me_hsize, me_vsize, me_blk_hsize, me_blk_vsize);
	dbg_h2
	    ("%s src_from_nr : %d, pix(fmt: %d, bit: %d, plan: %d, cmp: %d\n",
	     __func__, src_from_nr, pix_fmt, pix_bit, pix_plan, pix_cmpr);
	dbg_h2("%s frm_size( %d, %d), src_size( %d, %d)\n", __func__,
	       prm_mc->frm_hsize, prm_mc->frm_vsize, prm_mc->src_hsize,
	       prm_mc->src_vsize);
}

void cfg_dpss_mc_pre_triggle(void)
{				//cfg @go_filed
	//w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 0, 1);
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 29, 1);
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 30, 1);
	mc_pre_triger++;
	dbg_h2("%s call %d\n", __func__, mc_pre_triger);
}

void cfg_dpss_mc_triggle(void)
{				//cfg @go_filed
	//w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 0, 1);
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 31, 1);
	mc_trigger++;
	dbg_h2("%s call %d\n", __func__, mc_trigger);
}

void cfg_dpss_mc_slice(u32 frm_hsize, u32 frm_vsize, u32 slice_num)
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

	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL1, mc_slice_lpad[0], 24, 8);	   // reg_slc0_lpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL1, mc_slice_rpad[0], 16, 8);	   // reg_slc0_rpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL1, mc_slice_lpad[1], 8, 8);	   // reg_slc1_lpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL1, mc_slice_rpad[1], 0, 8);	   // reg_slc1_rpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL2, mc_slice_lpad[2], 24, 8);	   // reg_slc2_lpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL2, mc_slice_rpad[2], 16, 8);	   // reg_slc2_rpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL2, mc_slice_lpad[3], 8, 8);	   // reg_slc3_lpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL2, mc_slice_rpad[3], 0, 8);	   // reg_slc3_rpad
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL3, mc_slice_x_st[0], 16, 16);   // reg_slc0_xbgn
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL3, mc_slice_x_end[0], 0, 16);   // reg_slc0_xend
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL4, mc_slice_x_st[1], 16, 16);   // reg_slc1_xbgn
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL4, mc_slice_x_end[1], 0, 16);   // reg_slc1_xend
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL5, mc_slice_x_st[2], 16, 16);   // reg_slc2_xbgn
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL5, mc_slice_x_end[2], 0, 16);   // reg_slc2_xend
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL6, mc_slice_x_st[3], 16, 16);   // reg_slc3_xbgn
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL6, mc_slice_x_end[3], 0, 16);   // reg_slc3_xend

	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL0, (slice_num > 1), 0, 1);	// reg_slc_mode
	// w_reg_bit(FRC_MC_SLC_CTRL0,0,1,1); //reg_prephs_en_sel (todo?)
	DPSS_RDMA_WR_BIT_VS(FRC_MC_SLC_CTRL0, slice_num - 1, 2, 2);	  // reg_slc_num
	// w_reg_bit(FRC_MC_SLC_CTRL0,mc_proc_alternate_mode,4,2); //reg_mc_intp_lnum
}

void
cfg_dpss_mc_bbd(u32 frm_hsize, u32 frm_vsize, u32 bbd_en,
		u32 bbd_sel, u32 src_hsize, u32 src_vsize)
{
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_CTRL, bbd_en, 16, 1);// reg_bb_bbd_spatial_enable
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_CTRL, bbd_en, 17, 1);// reg_bb_detail_en
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_DETECT_REGION_TOP2BOT,
		src_vsize - 1, 0, 16);// reg_bb_det_bot
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_DETECT_REGION_LFT2RIT,
		src_hsize - 1, 0, 16);// reg_bb_det_rit
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_DETECT_DETAIL_TOP2BOT,
		src_vsize - 1, 0, 16);// reg_bb_det_detail_bot
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_DETECT_DETAIL_LFT2RIT,
		src_hsize - 1, 0, 16);// reg_bb_det_detail_rit
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT,
		src_hsize - 1, 16, 16);// reg_bb_oob_detail_xyxy_2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_OOB_DETAIL_WIN_RIT_BOT,
		src_vsize - 1, 0, 16);// reg_bb_oob_detail_xyxy_3
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_VER_TH,
		8 * frm_hsize / 1920, 16, 16);// reg_bb_ver_th1
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_VER_TH,
		560 * frm_hsize / 1920, 0, 16);// reg_bb_ver_th2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_FINER_HOR_TH,
		200 * frm_vsize / 1080, 16, 16);// reg_bb_hor_th1
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_FINER_HOR_TH,
		400 * frm_vsize / 1080, 0, 16);// reg_bb_hor_th2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_TOP_BOT_EDGE_TH,
		154 * frm_hsize / 1920, 16, 12);  // reg_bb_top_bot_edge_th1
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_TOP_BOT_EDGE_TH,
		256 * frm_hsize / 1920, 0, 12);// reg_bb_top_bot_edge_th2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_LFT_RIT_EDGE_TH,
		87 * frm_vsize / 1080, 16, 12);// reg_bb_lft_rit_edge_th1
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_LFT_RIT_EDGE_TH,
		144 * frm_vsize / 1080, 0, 12);// reg_bb_lft_rit_edge_th2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_TOP_BOT_EDGE_FIRST_TH,
		154 * frm_hsize / 1920, 16, 12);// reg_bb_top_bot_edge_first_th
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_TOP_BOT_EDGE_FIRST_TH,
		87 * frm_vsize / 1080, 0, 12);// reg_bb_lft_rit_edge_first_th
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_TOP_EDGE_DET_STT_END,
		405 * frm_vsize / 1080, 0, 16);// reg_bb_top_edge_det_end
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_BOT_EDGE_DET_STT_END,
		674 * src_vsize / 1080, 16, 16);
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_BOT_EDGE_DET_STT_END,
		frm_vsize - 1, 0, 16);// reg_bb_bot_edge_det_end
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_LFT_EDGE_DET_STT_END,
		720 * frm_hsize / 1920, 0, 16);// reg_bb_lft_edge_det_end
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_RIT_EDGE_DET_STT_END,
		1199 * frm_hsize / 1920, 16, 16);
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_RIT_EDGE_DET_STT_END,
		src_hsize - 1, 0, 16);// reg_bb_rit_edge_det_end
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE,
		src_hsize - 1, 16, 16);// reg_bb_oob_apl_xyxy_2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_OOB_APL_CAL_RIT_BOT_RANGE,
		src_vsize - 1, 0, 16);// reg_bb_oob_apl_xyxy_3
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_APL_HIST_WIN_RIT_BOT,
		src_hsize - 1, 16, 16);// reg_bb_apl_hist_xyxy_2
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_APL_HIST_WIN_RIT_BOT,
		src_vsize - 1, 0, 16);// reg_bb_apl_hist_xyxy_3
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_EDGE_SETTING, 0, 16, 2);// reg_bb_edge2_detect_range_mode
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_EDGE_SETTING, 0, 18, 2);// reg_bb_edge1_detect_range_mode
	// cfg bbd sel, 0-mc.mif0;1-mc.mif1,2-nr.mif
	DPSS_RDMA_WR_BIT_VS(DPSS_DPE_BBD_CTRL, bbd_sel, 0, 2);// reg_bb_edge1_detect_range_mode
	// cfg bbd source idx, src0 or src1
	u32 reg_fnr_src_idx = (rd(DPSS_TOP_FUNC_CTRL) >> 24) & 0xF;

	DPSS_RDMA_WR_BIT_VS(DPSS_BBD_ONLY_CTRL, reg_fnr_src_idx, 4, 4);// bbd_src_idx
	DPSS_RDMA_WR_BIT_VS(FRC_BBD_MISC, 10, 4, 4);
}

void dpss_dpe_info_cfg(u32 dpe_phs)
{
	u32 data32;
	u32 dpe_pre_pixl_buf;	//= (data32 >> 2 ) & 0xf;
	u32 dpe_cur_pixl_buf;	//= (data32 >> 8 ) & 0xf;
	u32 dpe_ibuf_nr_byps_pre;	//= (data32 >> 12) & 0x1;
	u32 dpe_ibuf_nr_byps_cur;	//= (data32 >> 13) & 0x1;
	u32 dpe_plogo_buf;	//= (data32 >> 14) & 0xf;
	u32 dpe_clogo_buf;	//= (data32 >> 18) & 0xf;
	u32 dpe_intp_phs;	//= (data32 >> 22) & 0xff;
	u32 dpe_byp_en;	//= (data32 >> 30) & 0x1;
	u32 dpe_mvlogo_buf;	//= (data32>> 0) & 0xf;
	u32 dpe_mv_cur_frm_idx;	//= (data32>> 0) & 0xf;
	u32 dpe_image_cur_frm_idx;	//= (data32>> 0) & 0xf;
	u32 dpe_image_pre_frm_idx;	//= (data32>> 0) & 0xf;

	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_YBUF_ADDR) : rd(DPSS_SRC0_NROUT_YBUF_ADDR);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_CBUF_ADDR) : rd(DPSS_SRC0_NROUT_CBUF_ADDR);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_YBUF_ADDR) : rd(DPSS_SRC0_NROUT_YBUF_ADDR);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_CBUF_ADDR) : rd(DPSS_SRC0_NROUT_CBUF_ADDR);
	u32 mc_inp_body_ybuf_addr[2];
	u32 mc_inp_body_cbuf_addr[2];
	u32 mc_inp_head_ybuf_addr;
	u32 mc_inp_head_cbuf_addr;

	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_YBUF_STEP) : rd(DPSS_SRC0_NROUT_YBUF_STEP);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_CBUF_STEP) : rd(DPSS_SRC0_NROUT_CBUF_STEP);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_YBUF_STEP) : rd(DPSS_SRC0_NROUT_YBUF_STEP);
	//= dpe_ibuf_nr_byps_cur ? rd(DPSS_SRC0_INP_CBUF_STEP) : rd(DPSS_SRC0_NROUT_CBUF_STEP);
	//u32 mc_inp_body_ybuf_step ;
	//u32 mc_inp_body_cbuf_step ;
	u32 mc_inp_head_ybuf_step;
	u32 mc_inp_head_cbuf_step;

	u32 mc_pix_logo_buf;	//= rd(DPSS_SRC0_PIX_LOGO_ADDR);
	u32 mc_blk_logo_buf;	//= rd(DPSS_SRC0_VP_MC_LOGO_ADDR);
	u32 mc_vpmc_mv_buf;	//= rd(DPSS_SRC0_VP_MC_MV_ADDR);

	u32 mc_pix_logo_step;	//= rd(DPSS_SRC0_PIX_LOGO_STEP);
	u32 mc_blk_logo_step;	//= rd(DPSS_SRC0_VP_MC_LOGO_STEP);
	u32 mc_vpmc_mv_step;	//= rd(DPSS_SRC0_VP_MC_MV_STEP);
	u32 mc_ibuf_vld;
	u32 game_mode_ntm;

	//= mc_inp_body_ybuf_addr + mc_inp_body_ybuf_step * dpe_pre_pixl_buf;
	//= mc_inp_head_ybuf_addr + mc_inp_head_ybuf_step * dpe_pre_pixl_buf;
	//= mc_inp_body_ybuf_addr + mc_inp_body_ybuf_step * dpe_cur_pixl_buf;
	//= mc_inp_head_ybuf_addr + mc_inp_head_ybuf_step * dpe_cur_pixl_buf;
	u32 mc_luma0_rdmif_baddr0;
	u32 mc_luma0_rdmif_baddr1;
	u32 mc_luma1_rdmif_baddr0;
	u32 mc_luma1_rdmif_baddr1;

	//= mc_inp_body_cbuf_addr + mc_inp_body_cbuf_step * dpe_pre_pixl_buf;
	//= mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pre_pixl_buf;
	//= mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pre_pixl_buf;
	//= mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_cur_pixl_buf;
	u32 mc_chrm0_rdmif_baddr0;
	u32 mc_chrm0_rdmif_baddr1;
	u32 mc_chrm1_rdmif_baddr0;
	u32 mc_chrm1_rdmif_baddr1;

	//= mc_pix_logo_buf + dpe_plogo_buf * mc_pix_logo_step;
	//= mc_pix_logo_buf + dpe_clogo_buf * mc_pix_logo_step;
	//= mc_blk_logo_buf + dpe_mvlogo_buf * mc_blk_logo_step;
	//= mc_vpmc_mv_buf  + dpe_mvlogo_buf * mc_vpmc_mv_step ;
	u32 ip_logo0_rdmif_baddr;
	u32 ip_logo1_rdmif_baddr;
	u32 dpe_melogo_baddr;
	u32 dpe_vpmc_mv_baddr;

//ary set no use    u32 dpe_pre_lossy_en      ;
//ary set no use    u32 dpe_cur_lossy_en      ;

	data32 = rd(DPSS_FRC_MC_IUFF_STATUS);
	mc_ibuf_vld = (data32 >> 4) & 0x1;
	game_mode_ntm = (rd(FRC_DPSS_LLM) >> 2) & 0x1;

	dbg_h2("STATUS:MC_IUFF(%#x), SRC_BUF(%#x), DST_BUF(%#x)\n",
	       data32,
	       rd(DPSS_FRC_SRC_BUFF_STATUS), rd(DPSS_FRC_DST_BUFF_STATUS));

	data32 = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	dpe_pre_pixl_buf = (data32 >> 2) & 0xf;
	dpe_cur_pixl_buf = (data32 >> 8) & 0xf;

//ary set no use    dpe_pre_lossy_en     = (data32 >> 1 ) & 0x1;
//ary set no use    dpe_cur_lossy_en     = (data32 >> 7 ) & 0x1;

	dpe_ibuf_nr_byps_pre = (data32 >> 12) & 0x1;
	dpe_ibuf_nr_byps_cur = (data32 >> 13) & 0x1;
	dpe_plogo_buf = (data32 >> 14) & 0xf;
	dpe_clogo_buf = (data32 >> 18) & 0xf;
	dpe_intp_phs = (mc_ibuf_vld) ? (data32 >> 22) & 0xff : 0;
	dpe_byp_en = (data32 >> 30) & 0x1;

	dbg_h2("display buff info: %x\n", data32);
	dbg_h2("dpe_pre_pixl_buf     = %x\n", dpe_pre_pixl_buf);
	dbg_h2("dpe_cur_pixl_buf     = %x\n", dpe_cur_pixl_buf);
	dbg_h2("dpe_ibuf_nr_byps_pre = %x\n", dpe_ibuf_nr_byps_pre);
	dbg_h2("dpe_ibuf_nr_byps_cur = %x\n", dpe_ibuf_nr_byps_cur);
	dbg_h2("dpe_plogo_buf        = %x\n", dpe_plogo_buf);
	dbg_h2("dpe_clogo_buf        = %x\n", dpe_clogo_buf);
	dbg_h2("dpe_intp_phs         = %x\n", dpe_intp_phs);
	dbg_h2("dpe_byp_en           = %x\n", dpe_byp_en);

	data32 = rd(FRC_DPSS_DISP_BUFF_INFO_1);
	dpe_mvlogo_buf = (data32 >> 0) & 0xf;
	dpe_mv_cur_frm_idx = dpe_mvlogo_buf;
	dpe_image_cur_frm_idx = dpe_cur_pixl_buf;
	dpe_image_pre_frm_idx = dpe_pre_pixl_buf;

	bool mc_mif_reg_mode = (rd(DPSS_DPE_MC_MIF_CTRL0) >> 2) & 0x1;
	u32 dpe_pixl_buf_idx0 = mc_mif_reg_mode && dpe_intp_phs <= 64 ?
		dpe_cur_pixl_buf : dpe_pre_pixl_buf;
	u32 dpe_pixl_buf_idx1 = mc_mif_reg_mode && dpe_intp_phs <= 64 ?
		dpe_pre_pixl_buf : dpe_cur_pixl_buf;

	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;
	//reg_aa_pps_mode == 3 -> nr aapps 4k1k
	bool nr_aapps_4k1k_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	// ary set and no use    bool nr_cmodel_byp_mode;
//#if 0				//ary set and no use
//#ifdef BBD_ONLY_MODE
//	nr_cmodel_byp_mode = 1;
//#else
//	nr_cmodel_byp_mode = 0;
//#endif
//#endif
	u32 src0_fbuf_yaddr[2];
	u32 src0_fbuf_caddr[2];

//ary no use    u32 src0_dwbuf_yaddr[2];
//ary no use    u32 src0_dwbuf_caddr[2];
	u32 src0_nro_fbuf_yaddr[2];
	u32 src0_nro_fbuf_caddr[2];
	u32 src0_dio_fbuf_yaddr[2];
	u32 src0_dio_fbuf_caddr[2];

//    u32 src1_fbuf_yaddr[16];
//    u32 src1_fbuf_caddr[16];
//    u32 src1_dpeo_fbuf_yaddr[16];
//    u32 src1_dpeo_fbuf_caddr[16];

	if (src0_nrdi_frc_en | nr_aapps_4k1k_en) {
		src0_dio_fbuf_yaddr[0] = rd(DPSS_SRC0_DIO_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_dio_fbuf_caddr[0] = rd(DPSS_SRC0_DIO_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_dio_fbuf_yaddr[1] = rd(DPSS_SRC0_DIO_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_dio_fbuf_caddr[1] = rd(DPSS_SRC0_DIO_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		mc_inp_body_ybuf_addr[0] = src0_dio_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_dio_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_dio_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_dio_fbuf_caddr[1];
		mc_inp_head_ybuf_addr = rd(DPSS_SRC0_DIOUT_YHEAD_ADDR);
		mc_inp_head_cbuf_addr = rd(DPSS_SRC0_DIOUT_CHEAD_ADDR);
		mc_inp_head_ybuf_step = rd(DPSS_SRC0_DIOUT_YHEAD_STEP);
		mc_inp_head_cbuf_step = rd(DPSS_SRC0_DIOUT_CHEAD_STEP);
	} else if (dpe_ibuf_nr_byps_cur | game_mode_ntm) {
		src0_fbuf_yaddr[0] = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_fbuf_caddr[0] = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_fbuf_yaddr[1] = rd(DPSS_SRC0_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_fbuf_caddr[1] = rd(DPSS_SRC0_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		mc_inp_body_ybuf_addr[0] = src0_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_fbuf_caddr[1];
		mc_inp_head_ybuf_addr = rd(DPSS_SRC0_INP_YHEAD_ADDR);
		mc_inp_head_cbuf_addr = rd(DPSS_SRC0_INP_CHEAD_ADDR);
		mc_inp_head_ybuf_step = rd(DPSS_SRC0_INP_YHEAD_STEP);
		mc_inp_head_cbuf_step = rd(DPSS_SRC0_INP_CHEAD_STEP);
	} else {
		src0_nro_fbuf_yaddr[0] = rd(DPSS_SRC0_NRO_FBUF_YADDR0 + dpe_pixl_buf_idx0);
		src0_nro_fbuf_caddr[0] = rd(DPSS_SRC0_NRO_FBUF_CADDR0 + dpe_pixl_buf_idx0);
		src0_nro_fbuf_yaddr[1] = rd(DPSS_SRC0_NRO_FBUF_YADDR0 + dpe_pixl_buf_idx1);
		src0_nro_fbuf_caddr[1] = rd(DPSS_SRC0_NRO_FBUF_CADDR0 + dpe_pixl_buf_idx1);
		mc_inp_body_ybuf_addr[0] = src0_nro_fbuf_yaddr[0];
		mc_inp_body_cbuf_addr[0] = src0_nro_fbuf_caddr[0];
		mc_inp_body_ybuf_addr[1] = src0_nro_fbuf_yaddr[1];
		mc_inp_body_cbuf_addr[1] = src0_nro_fbuf_caddr[1];
		mc_inp_head_ybuf_addr = rd(DPSS_SRC0_NROUT_YHEAD_ADDR);
		mc_inp_head_cbuf_addr = rd(DPSS_SRC0_NROUT_CHEAD_ADDR);
		mc_inp_head_ybuf_step = rd(DPSS_SRC0_NROUT_YHEAD_STEP);
		mc_inp_head_cbuf_step = rd(DPSS_SRC0_NROUT_CHEAD_STEP);
		DBG_INF("%s:addr:nro: val:mc wr :yaddr:0x%x\n", __func__,
			mc_inp_body_ybuf_addr[0]);
	}

	mc_pix_logo_buf = rd(DPSS_SRC0_PIX_LOGO_ADDR);
	mc_blk_logo_buf = rd(DPSS_SRC0_VP_MC_LOGO_ADDR);
	mc_vpmc_mv_buf = rd(DPSS_SRC0_VP_MC_MV_ADDR);

	mc_pix_logo_step = rd(DPSS_SRC0_PIX_LOGO_STEP);
	mc_blk_logo_step = rd(DPSS_SRC0_VP_MC_LOGO_STEP);
	mc_vpmc_mv_step = rd(DPSS_SRC0_VP_MC_MV_STEP);

//    mc_luma0_rdmif_baddr0 = mc_inp_body_ybuf_addr + mc_inp_body_ybuf_step * dpe_pixl_buf_idx0;
//    mc_luma0_rdmif_baddr0 = mc_inp_body_ybuf_addr[dpe_pixl_buf_idx0] << 5;
	mc_luma0_rdmif_baddr0 = mc_inp_body_ybuf_addr[0] << 5;
	mc_luma0_rdmif_baddr1 =
		mc_inp_head_ybuf_addr + mc_inp_head_ybuf_step * dpe_pixl_buf_idx0;
//    mc_luma1_rdmif_baddr0 = mc_inp_body_ybuf_addr + mc_inp_body_ybuf_step * dpe_pixl_buf_idx1;
//    mc_luma1_rdmif_baddr0 = mc_inp_body_ybuf_addr[dpe_pixl_buf_idx1] << 5;
	mc_luma1_rdmif_baddr0 = mc_inp_body_ybuf_addr[1] << 5;
	mc_luma1_rdmif_baddr1 =
	    mc_inp_head_ybuf_addr + mc_inp_head_ybuf_step * dpe_pixl_buf_idx1;

//   mc_chrm0_rdmif_baddr0 = mc_inp_body_cbuf_addr + mc_inp_body_cbuf_step * dpe_pixl_buf_idx0;
//   mc_chrm0_rdmif_baddr0 = mc_inp_body_cbuf_addr[dpe_pixl_buf_idx0] << 5;
	mc_chrm0_rdmif_baddr0 = mc_inp_body_cbuf_addr[0] << 5;
	mc_chrm0_rdmif_baddr1 =
	    mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx0;
//   mc_chrm1_rdmif_baddr0 = mc_inp_body_cbuf_addr + mc_inp_body_cbuf_step * dpe_pixl_buf_idx1;
//   mc_chrm1_rdmif_baddr0 = mc_inp_body_cbuf_addr[dpe_pixl_buf_idx1] << 5;
	mc_chrm1_rdmif_baddr0 = mc_inp_body_cbuf_addr[1] << 5;
	mc_chrm1_rdmif_baddr1 =
	    mc_inp_head_cbuf_addr + mc_inp_head_cbuf_step * dpe_pixl_buf_idx1;

	ip_logo0_rdmif_baddr =
	    mc_pix_logo_buf + dpe_plogo_buf * mc_pix_logo_step;
	ip_logo1_rdmif_baddr =
	    mc_pix_logo_buf + dpe_clogo_buf * mc_pix_logo_step;
	dpe_melogo_baddr = mc_blk_logo_buf + dpe_mvlogo_buf * mc_blk_logo_step;
	dpe_vpmc_mv_baddr = mc_vpmc_mv_buf + dpe_mvlogo_buf * mc_vpmc_mv_step;

	dbg_f2("mc_luma0_rdmif_baddr0     = %x\n", mc_luma0_rdmif_baddr0);
	dbg_f2("mc_luma1_rdmif_baddr0     = %x\n", mc_luma1_rdmif_baddr0);

	////=======================================================////
	//// dpe input data
	////======================================================////

	//w_reg_bit(DPSS_DPE_TOP_SW_SEL0, 0x7 , 23, 3); //for nr, not for mc
	//w_reg_bit(DPSS_DPE_TOP_SW_SEL0, 0x1 , 31, 1);
	//w_reg_bit(DPSS_DPE_TOP_SW_SEL1, 0x3f, 3 , 6);

	u32 mc_use_tbc_addr = rd(DPSS_TOP_APB_TBC_SEL_CTRL) & 0x1;

	if (mc_use_tbc_addr == 0) {
		dbg_h2("MC use driver config\n");
		wr(DPSS_DPE_MC_DIN_LUMA0_BADDR0, mc_luma0_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_LUMA0_BADDR1, mc_luma0_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_LUMA1_BADDR0, mc_luma1_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_LUMA1_BADDR1, mc_luma1_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_CHRM0_BADDR0, mc_chrm0_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_CHRM0_BADDR1, mc_chrm0_rdmif_baddr1);

		wr(DPSS_DPE_MC_DIN_CHRM1_BADDR0, mc_chrm1_rdmif_baddr0);
		wr(DPSS_DPE_MC_DIN_CHRM1_BADDR1, mc_chrm1_rdmif_baddr1);

		if (mc_mif_reg_mode == 1) {
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
			      MIF0_RMIF_CTRL4, mc_luma0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC0) +
			      MIF1_RMIF_CTRL4, mc_chrm0_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
			      MIF0_RMIF_CTRL4, mc_luma1_rdmif_baddr0);
			wr_vc(get_vfcd_regs_ofst(DPSS_RMIF_MC1) +
			      MIF1_RMIF_CTRL4, mc_chrm1_rdmif_baddr0);

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

		wr(DPSS_DPE_MC_DIN_LOGO_BADDR0, ip_logo0_rdmif_baddr);
		wr(DPSS_DPE_MC_DIN_LOGO_BADDR1, ip_logo1_rdmif_baddr);
		wr(DPSS_DPE_MC_DIN_MELG_BADDR1, dpe_melogo_baddr);
		wr(DPSS_DPE_MC_DIN_MV_BADDR1, dpe_vpmc_mv_baddr);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_pre_frm_idx, 8,
			  4);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_image_cur_frm_idx, 4,
			  4);
		w_reg_bit(DPSS_DPE_MC_DIN_FRM_IDX0, dpe_mv_cur_frm_idx, 0, 4);
		//w_reg_bit(DPSS_DPE_MC_CTRL0, dpe_intp_phs, 20, 8);
		w_reg_bit(DPSS_DPE_MC_PHASE, dpe_intp_phs, 0, 8);
	} else {
		dbg_h2("MC use tbc config\n");
	}
	dbg_h2
	    ("read : MC_DIN_ADDR(E830): luma(%#x,%#x,%#x,%#x),chrm(%#x,%#x,%#x,%#x)",
	     rd(DPSS_DPE_MC_DIN_LUMA0_BADDR0),
	     rd(DPSS_DPE_MC_DIN_LUMA0_BADDR1),
	     rd(DPSS_DPE_MC_DIN_LUMA1_BADDR0),
	     rd(DPSS_DPE_MC_DIN_LUMA1_BADDR1),
	     rd(DPSS_DPE_MC_DIN_CHRM0_BADDR0),
	     rd(DPSS_DPE_MC_DIN_CHRM0_BADDR1),
	     rd(DPSS_DPE_MC_DIN_CHRM1_BADDR0),
	     rd(DPSS_DPE_MC_DIN_CHRM1_BADDR1));
	dbg_h2("read : mif_addr(%#x,%#x,%#x,%#x), afbc(%#x,%#x,%#x,%#x)",
	       rd_vc(MIF0_RMIF_CTRL4 + 0x400),
	       rd_vc(MIF1_RMIF_CTRL4 + 0x400),
	       rd_vc(MIF0_RMIF_CTRL4 + 0x500),
	       rd_vc(MIF1_RMIF_CTRL4 + 0x500),
	       rd_vc(VFCD_AFBC_HEAD_BADDR + 0x400),
	       rd_vc(VFCD_AFBC_BODY_BADDR + 0x400),
	       rd_vc(VFCD_AFBC_HEAD_BADDR + 0x500),
	       rd_vc(VFCD_AFBC_BODY_BADDR + 0x500));

	//w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1           , 0 , 4);

	//w_reg_bit(DPSS_DPE_DIN_SEL_CTRL0, 1           , 4 , 4); //reg_dpe_src_idx
//    w_reg_bit(FRC_MC_HW_CTRL0, dpe_byp_en  , 20, 1); //reg_bypass_mc_core_en
	//mc din vfcd mode

	//if(prm_mc.src_cmpr == CMPR_AFBC){
	//    dbg_h2("[MC VFCD]mc din vfcd mode: AFBC\n");
	//    cfg_vfcd_cmpr_enable(4, 1); //vfcd_chose cur_afbc
	//    cfg_vfcd_cmpr_enable(5, 1); //vfcd_chose pre_afbc
	//}
	//else if (prm_mc.src_cmpr == CMPR_AFRC){
	//    dbg_h2("[MC VFCD]mc din vfcd mode: AFRC\n");
	//    cfg_vfcd_cmpr_enable(4, 2); //vfcd_chose cur_afbc
	//    cfg_vfcd_cmpr_enable(5, 2); //vfcd_chose pre_afbc
	//}
//#if 0				//ary change : //need_check
//	struct PRM_DPSS_TOP prm_top;
//	bool cut_win_en = prm_top.cut_win_en;
//#else
	bool cut_win_en = prm_dpss_top->cut_win_en; //frc only

	// ary note:cut win should not be set in top,
	// this infor come from vpp
//#endif

	u32 get_pix_logo_mode;

	memset((void *)(&pre_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(&cur_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(&mv_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(&me_logo_rmif), 0, sizeof(struct PRM_INTF_TYPE));

	pre_logo_rmif.burst_len = 2;	//1: burst4, 2: brst8, 3: burst16
	cur_logo_rmif.burst_len = 2;
	mv_rmif.burst_len = 2;
	me_logo_rmif.burst_len = 2;
	if (cut_win_en) {
		get_pix_logo_mode = rd(FRC_MC_LOGO_OPTION);
		pre_logo_rmif.src_baddr[0] =
		    get_pix_logo_mode ==
		    2 ? ip_logo1_rdmif_baddr : ip_logo0_rdmif_baddr;
		cur_logo_rmif.src_baddr[0] =
		    get_pix_logo_mode ==
		    1 ? ip_logo0_rdmif_baddr : ip_logo1_rdmif_baddr;
		mv_rmif.src_baddr[0] = dpe_vpmc_mv_baddr;
		me_logo_rmif.src_baddr[0] = dpe_melogo_baddr;
		cfg_mc_sub_rdmif(&pre_logo_rmif, 0, 1);
		cfg_mc_sub_rdmif(&cur_logo_rmif, 1, 1);
		cfg_mc_sub_rdmif(&mv_rmif, 2, 1);
		cfg_mc_sub_rdmif(&me_logo_rmif, 3, 1);
	}
}
