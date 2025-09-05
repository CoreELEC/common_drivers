// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_intf.h"
#include "dpss_dae.h"
#include "../drv/dpss_pq_drv.h"

extern unsigned int dpss_dbg_uv_swap;
bool ini_cfg_dae_in_nr_frc;
int frc_fst_frm_in_nr;
static void cfg_dae_secure(unsigned int dae_mode,
		struct PRM_DPSS_TOP  *prm_top,
		struct PRM_DPSS_DAE  *prm_dae);

// for nr dae 1 interrupt  in nr_frc case
void hw_cfg_dpss_dae(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae)
{
	dbg_w2("%s:dae_mode = %d,prm_dpss_dae:0x%px\n", "cfg_dae", dae_mode, prm_dae);
	struct PRM_INTF_TYPE *dae_yuv;

	u32 org_hsize;
	u32 org_vsize;
	bool     din_pad_en = 0;
	//ary no use    u32 rdata32 = rd(DPSS_SRC_INDEX);
	//ary no use    u32 dae_src_idx = (rdata32>>16)&0xf;
	bool game_mode   = prm_top->dpe_game_mode;
	bool src_is_frc  = (dae_mode == DAE_FRC_MODE) || game_mode;
	//dae_src_idx==SRC_IDX_FRC;
	bool src_is_nr   = dae_mode == DAE_NR_MODE;//dae_src_idx==SRC_IDX_NR;
	bool src_is_di   = (dae_mode == DAE_VDI_MODE ||  dae_mode == DAE_NRDI_MODE);
	//dae_src_idx==SRC_IDX_DI;
	bool src_is_byps = dae_mode == DAE_BYPS_MODE;//dae_src_idx==SRC_IDX_DI;
	bool size_in = prm_top->size_as_in; //ary
	bool frc_ds_scale_en = prm_top->frc_ds_scale_en;
	bool dae_sw_en   = prm_top->sw_ctrl_en & 0x1;
	bool dae_hpps_en = prm_top->nr_hscale_on & 0x1;
	bool dae_auto_align = prm_top->auto_alig_en & 0x1;
	bool dae_align_hmode = prm_top->alig_hmode & 0x1;
	u32 nr_4k1k_vds_en = prm_top->vds_4k1k_en && src_is_nr;
	//bool dae_align_vmode = prm_top->alig_vmode & 0x1;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	u32 dpe_pps_en = (pps->pps_en | prm_top->nr_aapps_on) & src_is_frc;
	int i;
	bool dae_nosrc_en;
	u32 inp_hsize, inp_vsize, me_hsize, me_vsize;
	u32 me_hvds_en;
	u32 dae_grd_num_mode;

	dbg_h2("pps ch=%d,nr_hscale_on=%d dae_hpps_en=%d,dae_auto_align=%d,%d\n", prm_top->ch,
		prm_top->nr_hscale_on, dae_hpps_en, dae_auto_align, dpe_pps_en);
	dbg_h2("dae_align_hmode=%d alig_hmode=%d\n", dae_align_hmode, prm_top->alig_hmode);

	w_reg_bit(VPU_DAE_WRAP_CTRL, src_is_byps, 31, 1);//reg_dae_byps

	if (src_is_byps) {
		prm_top->dpss_nr_byp = 1;
		dbg_h2("[dpss_dae.c], dae bypass\n");
		return;
	} else {
		prm_top->dpss_nr_byp = 0;
	}

	enum vid_src_fmt src_fmt    = src_is_frc ? prm_top->nro_ds_fmt.src_fmt :
				src_is_nr  ? prm_top->src_ds_fmt.src_fmt :
					prm_top->src_fs_fmt.src_fmt/*i_src*/;
	enum vid_src_fmt src_bit    = src_is_frc ? prm_top->nro_ds_fmt.src_bit :
				src_is_nr  ? prm_top->src_ds_fmt.src_bit :
					prm_top->src_fs_fmt.src_bit/*i_src*/;

	enum vid_src_fmt src_plane  = src_is_frc ? prm_top->nro_ds_fmt.src_plan :
				src_is_nr  ? prm_top->src_ds_fmt.src_plan :
					prm_top->src_fs_fmt.src_plan/*i_src*/;

	dae_nosrc_en           = ((prm_top->dpss_mode == DPSS_FRC_NR_MODE) ||
				(prm_top->dpss_mode == DPSS_FRC_NRBYPS_MODE)) & src_is_frc;

	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize    = prm_top->org_hsize;
		org_vsize    = prm_top->org_vsize;
	} else {
		org_hsize    = dpe_pps_en ? prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
		org_vsize    = dpe_pps_en ? prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;
	}

	u32 me_dsx        = prm_top->dae_dsx_scale;
	u32 me_dsy        = prm_top->dae_dsy_scale;
	// u32 me_hvds_en    = src_is_di | frc_ds_scale_en | prm_top->no_ds;

	if (src_is_frc) {
		me_hvds_en	  = src_is_di | frc_ds_scale_en;
		inp_hsize     = (dae_hpps_en & dae_nosrc_en) ? ((org_hsize == 4096) ? 960  : 480) :
				(org_hsize + (1 << me_dsx) - 1) >> me_dsx;
		inp_vsize     = (org_vsize + (1 << me_dsy) - 1) >> me_dsy;
	} else { // dae_mode == DAE_NR_MODE，me_hvds_en: enable dae ds inside
		me_hvds_en	  = src_is_di | prm_top->no_ds | size_in;
		inp_hsize	  = me_hvds_en ?	org_hsize :
			(dae_hpps_en & dae_nosrc_en) ? ((org_hsize == 4096) ? 960  : 480) :
			(org_hsize + (1 << me_dsx) - 1) >> me_dsx;
		inp_vsize	  = me_hvds_en ?	org_vsize :
			(org_vsize + (1 << me_dsy) - 1) >> me_dsy;
	}
	dbg_h2("pps inp_hsize=%d, inp_vsize=%d,no_ds=%d size_in =%d,%d,%d\n",
		inp_hsize, inp_vsize, prm_top->no_ds,
		prm_top->size_as_in, org_hsize, org_vsize);

	u32 reg_mix_hds_en = (prm_top->no_ds || inp_hsize > 960) ? me_dsx : 0;
	u32 reg_mix_vds_en = (prm_top->no_ds || inp_vsize > 540) ? me_dsy : 0;

	if (src_is_frc) {
		me_hsize = dae_hpps_en ? ((org_hsize == 4096) ? 960 : 480) :
			(me_hvds_en) ? (inp_hsize + (1 << me_dsx) - 1) >> me_dsx : inp_hsize;
		me_vsize = me_hvds_en ? (inp_vsize + (1 << me_dsy) - 1) >> me_dsy : inp_vsize;
	} else {
		me_hsize =	(dae_hpps_en) ? ((org_hsize == 4096) ? 960 : 480) :
			(me_hvds_en) ? (inp_hsize + (1 << me_dsx) - 1) >> me_dsx : inp_hsize;
		me_vsize =	(me_hvds_en) ?
			(inp_vsize + (1 << me_dsy) - 1) >> me_dsy : inp_vsize;
	}

	//bool     dae_inds2_hen = (prm_top->frc_ds_scale_en == 0) & (me_dsx == 1);
	//bool     dae_inds4_hen = (prm_top->frc_ds_scale_en == 0) & (me_dsx == 2);
	//bool     dae_inds2_ven = (prm_top->frc_ds_scale_en==0) & (me_dsy==1);
	//bool     dae_inds4_ven = (prm_top->frc_ds_scale_en==0) & (me_dsy==2);

	if (dae_auto_align || nr_4k1k_vds_en) {
#ifdef _HIS_CODE_
		me_hsize = ((reg_mix_hds_en == 1) | dae_inds2_hen) ?
			((dae_align_hmode) ? (me_hsize + 7) / 8 * 8 : (me_hsize + 3) / 4 * 4) :
			((reg_mix_hds_en == 2) | dae_inds4_hen) ?
				((dae_align_hmode) ? (me_hsize + 3) /
					4 * 4 : (me_hsize + 3) / 4 * 4) :
				(dae_align_hmode) ? (me_hsize + 15) /
					16 * 16 : (me_hsize + 7) / 8 * 8;
		me_vsize = ((reg_mix_vds_en == 1) | dae_inds2_ven) ?
			((dae_align_vmode) ? (me_vsize + 7) / 8 * 8 : (me_vsize + 3) / 4 * 4) :
			((reg_mix_vds_en == 2) | dae_inds4_ven) ?
				((dae_align_vmode) ? (me_vsize + 3) /
					4 * 4 : (me_vsize + 3) / 4 * 4) :
				(dae_align_vmode) ? (me_vsize + 15) /
					16 * 16 : (me_vsize + 7) / 8 * 8;
#else
		me_hsize = (me_hsize + 3) / 4 * 4;
		me_vsize = (me_vsize + 3) / 4 * 4;
#endif
	}
	dbg_h2("inp<%d,%d>  me:<%d, %d>  mix_ds_en:<%d,%d>\n",
		inp_hsize, inp_vsize,
		me_hsize, me_vsize,
		reg_mix_hds_en, reg_mix_vds_en);
	u32 me_blk_hsize  = (me_hsize + 3) >> 2;
	u32 me_blk_vsize  = (me_vsize + 3) >> 2;
	u32 mvx_div_mode  = 1;
	u32 mvy_div_mode  = 1;

	if (me_hsize < 80)
		dae_grd_num_mode = 0;
	else
		dae_grd_num_mode = 2;

	if (dae_hpps_en & !dae_nosrc_en)
		dpss_dae_hpps(inp_hsize, me_hsize);

	struct PRM_DPSS_SIZE *prm_size_ext;

	prm_size_ext = prm_dae->prm_size_ext;
	//ary move to glb: struct PRM_DPSS_SIZE prm_size;
	prm_size_ext->frm_hsize       = inp_hsize;
	prm_size_ext->frm_vsize       = inp_vsize;
	prm_size_ext->me_hsize        = me_hsize;
	prm_size_ext->me_vsize        = me_vsize;
	prm_size_ext->me_blk_hsize    = me_blk_hsize;
	prm_size_ext->me_blk_vsize    = me_blk_vsize;
	prm_dae->prm_size         = *prm_size_ext;
	dbg_h2("%s:prm_dpss_dae: update prm_size\n", __func__);

	cfg_dae_secure(dae_mode, prm_top, prm_dae); //secure
	//==============================================================//
	// dae_size
	//==============================================================//

	wr(VPU_DAE_WRAP_SW_SIZE0,	(inp_vsize   & 0x1fff) << 16 | //inp_size
					(inp_hsize   & 0x1fff));//inp_size
	wr(VPU_DAE_WRAP_SW_SIZE1,	(me_vsize    & 0x1fff) << 16 | //frm_size
					(me_hsize    & 0x1fff));//frm_size
	wr(VPU_DAE_WRAP_SW_SIZE2,	(me_blk_vsize & 0x1fff) << 16 | //blk_size
					(me_blk_hsize & 0x1fff));//blk_size
	w_reg_bit(VPU_DAE_WRAP_SW_SEL0, 0x3f, 9, 6);//reg_mode
	//==============================================================//
	// dae baddr,only cur
	// 0x82-MC reg
	// 0x83-NR reg
	//==============================================================//
	u32 mc_dae_proc_st = (rd(DPSS_FRC_PROC_STATUS) >> 8) & 0x7;
	u32 nr_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS) >> 16) & 0x7;
	u32 di_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS + (0x86 - 0x83) * 256) >> 16) & 0x7;
	//ary addr change *4
	u32 src0_i = 0;
	//ary no use    u32 src0_p = 0;
	u32 src1_i = 1;
	//ary no use    u32 src1_p = 0;
	u32 luma_baddr_x16[16];
	u32 chrm_baddr_x16[16];
	u32 dae_frc_loop = mc_dae_proc_st == 3 ? 0 :	//ary:frc me? is 3
				nr_dae_proc_st == 3 ? 1 :	//
				di_dae_proc_st == 3 ? 2 : 0;

	dbg_h2("%s:reg:st:mc=0x%x, nr=0x%x, di = 0x%x\n", __func__,
		mc_dae_proc_st, nr_dae_proc_st, di_dae_proc_st);
#ifdef ARY_SIM
	//dae_frc_loop = g_dae_frc_loop;
#endif
	u32 nr_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_FRC_NR_MODE ||
				(prm_top->dpss_mode == DPSS_FRC_NRBYPS_MODE &&
				(~prm_top->bbd_only)));
	u32 di_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_NRDI_FRC_SRC0_MODE ||
				prm_top->dpss_mode == DPSS_DI_FRC_SRC0_MODE);
	for (i = 0; i < 16; i++) {
		luma_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_yaddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_yaddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_yaddr[i] :
					dae_frc_loop == 0x1 ?
						(src0_i ? prm_top->src0_fbuf_yaddr[i] :
							prm_top->src0_dwbuf_yaddr[i]) ://src0_p
					(src1_i ? prm_top->src1_fbuf_yaddr[i] :
					prm_top->src1_dwbuf_yaddr[i]);//src1_p
		chrm_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_caddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_caddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_caddr[i] :
					dae_frc_loop == 0x1 ?
						(src0_i ? prm_top->src0_fbuf_caddr[i] :
					prm_top->src0_dwbuf_caddr[i]) ://src0_p
					(src1_i ? prm_top->src1_fbuf_caddr[i] :
					prm_top->src1_dwbuf_caddr[i]);//src1_p
	}
	//    u32 luma_step  = nr_frc_link ? rd(DPSS_SRC0_NRODW_YBUF_STEP) :
	//dae_frc_loop <= 0x1 ? rd(DPSS_SRC0_IDW_YBUF_STEP) : rd(DPSS_SRC1_INP_YBUF_STEP);
	//    u32 chrm_step  = nr_frc_link ? rd(DPSS_SRC0_NRODW_CBUF_STEP) :
	//dae_frc_loop <= 0x1 ? rd(DPSS_SRC0_IDW_CBUF_STEP) : rd(DPSS_SRC1_INP_CBUF_STEP);
	u32 mc_rd_idx  = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	u32 nr_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX) >> 20) & 0xf;
	u32 di_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX + (0x86 - 0x83) * 256) >> 20) & 0xf;
	//ary addr change *4
	u32 cur_rd_idx =	dae_frc_loop == 0 ? mc_rd_idx :
				dae_frc_loop == 1 ? nr_rd_idx :
				dae_frc_loop == 2 ? di_rd_idx : 0;

	dbg_h2("%s:reg:idx:mc=0x%x, nr=0x%x, di = 0x%x, cur=0x%x\n", __func__,
		mc_rd_idx, nr_rd_idx, di_rd_idx, cur_rd_idx);
#ifdef ARY_SIM
	cur_rd_idx = g_dae_index;
#endif

	//u32 luma_raddr = luma_baddr + luma_step * cur_rd_idx ;
	//u32 chrm_raddr = chrm_baddr + chrm_step * cur_rd_idx ;
	u32 luma_raddr = luma_baddr_x16[cur_rd_idx] << 5;
	u32 chrm_raddr = chrm_baddr_x16[cur_rd_idx] << 5;

	if (dae_sw_en == 0 && prm_top->fnr_sw_mode == 0) { //ary ? need set?? for auto mode
		dbg_h2("%s:y_addr = 0x%x, uv_addr = 0x%x\n", __func__, luma_raddr, chrm_raddr);
		dbg_h2("%s: DAE_WRAP_SW_ADDR11 <= 0x%x, next_y_addr?\n", __func__, luma_raddr);
		wr(VPU_DAE_WRAP_SW_ADDR11, luma_raddr);//nxt_luma_addr
		wr(VPU_DAE_WRAP_SW_ADDR12, chrm_raddr);//nxt_chrm_addr
		//        w_reg_bit(VPU_DAE_WRAP_SW_SEL1,3,1,2);//sel
	}

	//Augustine: add for mif mmu mode
	prm_dae->dae_yuv_mif.src_baddr[0] = luma_raddr << 4;
	prm_dae->dae_yuv_mif.src_baddr[1] = chrm_raddr << 4;

	dbg_h2("dae_frc_loop=%d cur_rd_idx=%d\n", dae_frc_loop, cur_rd_idx);
	dbg_h2("%s: nr_4k1k_vds_en=%d.\n\n", __func__, nr_4k1k_vds_en);
	dbg_h2("luma_raddr=0x%x chrm_raddr=0x%x\n", luma_raddr, chrm_raddr);
	//    dbg_h2("luma_step=0x%x chrm_step=0x%x\n",luma_step,chrm_step);

	//ary move to glob: struct PRM_INTF_TYPE dae_yuv_intf;
	//ary move memset((void*)(&dae_yuv_intf), 0, sizeof(struct PRM_INTF_TYPE));
	if (dae_mode == DAE_FRC_MODE)
		dae_yuv = prm_dae->ext_yuv_intf;
	else
		dae_yuv = &prm_dae->nr_yuv_intf;

	dae_yuv->src_hsize = inp_hsize;
	dae_yuv->src_vsize = inp_vsize;
	dae_yuv->src_bit   = src_bit;
	dae_yuv->src_plan  = src_plane;
	dae_yuv->src_fmt   = src_fmt;
	dae_yuv->src_baddr[0] = prm_dae->dae_yuv_mif.src_baddr[0];
	dae_yuv->src_baddr[1] = prm_dae->dae_yuv_mif.src_baddr[1];

	dae_yuv->slc_x_st[0]  = 0;
	dae_yuv->slc_x_ed[0]  = inp_hsize - 1;
	dae_yuv->slc_y_st[0]  = 0;
	//yu.zong 2024-12-06	dae_yuv_intf.slc_y_ed[0]  = inp_vsize -1;
	dae_yuv->slc_y_ed[0] = ((prm_top->di_interlace | nr_4k1k_vds_en) ? inp_vsize * 2 :
				inp_vsize) - 1;
	dbg_h2("%s: x:%d %d, y:%d %d.\n",
		__func__,
		dae_yuv->slc_x_st[0],
		dae_yuv->slc_x_ed[0],
		dae_yuv->slc_y_st[0],
		dae_yuv->slc_y_ed[0]);

	dae_yuv->bits_mode  = prm_dae->dae_yuv_mif.bits_mode;
	dae_yuv->pack_mode  = prm_dae->dae_yuv_mif.pack_mode;

	dae_yuv->reverse[0] = (prm_top->frc_rev_mode & 0x1) |
				(prm_top->comp_setting.vfcd_rev_mode & 0x1);
	dae_yuv->reverse[1] = ((prm_top->frc_rev_mode & 0x2) >> 1) |
			(prm_top->comp_setting.vfcd_rev_mode & 0x2) >> 1;
	//yu.zong 2024-12-06	dae_yuv_intf.skip_line =0 ;
	if (prm_top->di_line_sel) {
		dae_yuv->skip_line = nr_4k1k_vds_en ? 1 : 0;
		dae_yuv->reg_mode   = (prm_top->di_interlace | nr_4k1k_vds_en) ? 0x1 : 0x0;
		dae_yuv->field_mode = prm_top->di_interlace;
		//0 ;//0:close 1:odd->even 2:even->odd	//yu.zong 2024-12-06
	} else {
		dae_yuv->skip_line = (prm_top->di_interlace | nr_4k1k_vds_en) ? 0x1 : 0x0;
		dae_yuv->reg_mode  = (prm_top->di_interlace | nr_4k1k_vds_en) ? 0x1 : 0x0;
		dae_yuv->field_mode = 0;
	}

	dbg_h2("skip_line=%d, reg_mode=%d, field_mode=%d\n",
		dae_yuv->skip_line,
		dae_yuv->reg_mode,
		dae_yuv->field_mode);
	dae_yuv->mmu_mode_en = prm_top->ds_mif_mmu_mode_en;
//	if (dpss_dbg_uv_swap & C_BIT0) {
//		dae_yuv->uv_swap = true;
//	}
	dae_yuv->uv_swap = prm_top->src_dw_uv_swap;//dpss_dbg_uv_swap & C_BIT0;
	dbg_h2("dae dwuv sw:%d\n", dae_yuv_intf.uv_swap);
	//ary add default is 1, dae_yuv->little_endian
	//dae_yuv->field_mode = 0;//0:close 1:odd->even 2:even->odd //12-17
	if (dpss_dbg_uv_swap & C_BIT2)
		dae_yuv->swap_64bit = dpss_dbg_uv_swap & C_BIT2;
	if (dpss_dbg_uv_swap & C_BIT1)
		dae_yuv->little_endian = dpss_dbg_uv_swap & C_BIT1;
	if (dpss_dbg_uv_swap & C_BIT0)
		dae_yuv->little_endian = dpss_dbg_uv_swap & C_BIT0;
	dae_yuv->block_mode = prm_top->block_mode;
	if (prm_top->src_is_1080p_nods) {
		dae_yuv->little_endian = prm_top->l_endian;//dpss_dbg_uv_swap & C_BIT1;
		dae_yuv->swap_64bit = prm_top->swap_64bit;//dpss_dbg_uv_swap & C_BIT2;
	}
	dae_yuv->canvas_hsize = prm_top->nr_src_canvas_hsize;

	dpss_cfg_viu_pix_rdmif(dae_yuv, 1);
	if (dae_mode != DAE_FRC_MODE) {	//05-01
		dae_yuv->back_2_y = dae_yuv->back_1_y;
		dae_yuv->back_2_u = dae_yuv->back_1_u;
	}

	//bool ini_cfg_frc_dae_in_nr = 1;//todo
	ini_cfg_dae_in_nr_frc = 1;
	//w_reg_bit(VPU_DAE_WRAP_CTRL,1, 0,1);// reg_dae_start_sel   0:hw_auto  1:pls

	//==============================================================//
	// dae_wrap
	//==============================================================//
	prm_dae->tfbf_en = 1;
	bool tfbf_en    = prm_dae->tfbf_en   & src_is_di;
	bool dctgrd_en  = prm_dae->dctgrd_en;

	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, tfbf_en, 31, 1); //reg_tfbf_en
	if (tfbf_en) {
		if (prm_top->frame_count < 5)
			w_reg_bit(VPU_DAE_WRAP_MIX_DS, 0, 6, 1);
		cfg_tfbf(inp_hsize, inp_vsize);
	}
	//Use Aligned Size:
	//if(tfbf_en) cfg_tfbf(me_hsize, me_vsize);

	//YUV422;//todo, dpe_ds420==mix422 ????
	u32 di_in_mode     = src_fmt == YUV444 ? 0 : src_fmt == YUV422 ? 1 : 2;
	u32 di_bit_tight   = (src_bit == BIT_010);//0:10 in 16bit 1:10 in 10bit
	u32 di_bit         = (src_bit == BIT_010) | (src_bit == BIT_P010);

	din_pad_en = ((inp_hsize != (me_hsize * (1 << reg_mix_hds_en))) |
		(inp_vsize != (me_vsize * (1 << reg_mix_vds_en)))) & (dae_hpps_en == 0);

	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, game_mode, 0, 1);//reg_game_mode
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE1, game_mode, 4, 1);//reg_llm_en

	w_reg_bit(VPU_DAE_WRAP_DI, ((di_bit_tight & 0x1) << 4) |
					((di_in_mode & 0x3) << 0), 18, 5);
					//((di_ds_mode & 0x3) << 0), 16, 7);

	w_reg_bit(VPU_DAE_WRAP_DI, di_bit & 0x1, 4, 1);

	if ((prm_top->frame_count < 5 && dpss_frc_bypass) || !dpss_frc_bypass)
		w_reg_bit(VPU_DAE_WRAP_MIX_DS, (reg_mix_vds_en & 0x3) << 2 |
			(reg_mix_hds_en & 0x3), 0, 4);

	w_reg_bit(VPU_DAE_WRAP_DI, src_plane == PLANAR_X1, 24, 1);//reg_yuv444_one_cavs
	if (src_is_frc) {//frc or nr
#ifdef SRC1_PSRC_NR
		w_reg_bit(VPU_DAE_WRAP_DI, 0, 27, 1);//reg_me_bld_inp_mode
		w_reg_bit(VPU_DAE_WRAP_DI, 0x1, 28, 4);//reg_me_bld_y_gain
		w_reg_bit(VPU_DAE_WRAP_DI, 0x22, 8, 8);
		//reg_me_bld_v_gain/reg_me_bld_u_gain
#else
		w_reg_bit(VPU_DAE_WRAP_DI, 1, 27, 1);//reg_me_bld_inp_mode
		w_reg_bit(VPU_DAE_WRAP_DI, 0xd, 28, 4);//reg_me_bld_y_gain
		w_reg_bit(VPU_DAE_WRAP_DI, 0x44, 8, 8);
		//reg_me_bld_v_gain/reg_me_bld_u_gain
#endif
	} else {//di
		w_reg_bit(VPU_DAE_WRAP_DI, 0, 27, 1);//reg_me_bld_inp_mode
		w_reg_bit(VPU_DAE_WRAP_DI, 0x1, 28, 4);//reg_me_bld_y_gain
		w_reg_bit(VPU_DAE_WRAP_DI, 0x22, 8, 8);
		//reg_me_bld_v_gain/reg_me_bld_u_gain
	}

	w_reg_bit(VPU_DAE_WRAP_PAD_CTRL, din_pad_en, 0, 1);

	if (din_pad_en || dctgrd_en)
		prm_dae->dae_fmt_up_repeat = 1;

	if (di_in_mode == YUV420)
		w_reg_bit(DAE_VD_CFMT_CTRL, 1, 28, 1); //fmt up repeat
	else
		w_reg_bit(DAE_VD_CFMT_CTRL, prm_dae->dae_fmt_up_repeat, 28, 1);
	//fmt up repeat

	//if (ini_cfg_dae_in_nr_frc == 1 && frc_fst_frm_in_nr > 1)
	//	return;
	//else if (ini_cfg_dae_in_nr_frc == 0 && frc_fst_frm_in_nr == 0)
	//	frc_fst_frm_in_nr++;

	// ini_cfg_dae_in_nr_frc = 1;
	//==============================================================//
	// frc_me/vp/melogo/iplogo/bbd enable
	//==============================================================//
	bool frc_me_en;
	bool vp_en;
	bool melogo_en;
	bool iplogo_en;
	bool bbd_en;
	bool dcntr_en = 0;
	bool mv_seed_update = 0;//for rnd_mv

	if (src_is_frc) {
		frc_me_en    = 1;
		vp_en        = !game_mode;
		melogo_en    = 1;
		iplogo_en    = 1;
		bbd_en       = 1;
	} else {
		frc_me_en    = 0;
		vp_en        = 0;
		melogo_en    = 0;
		iplogo_en    = 0;
		bbd_en       = 0;
	}
//07-10 need check
	if (dae_mode != DAE_FRC_MODE)
		dcntr_en = dctgrd_en;
#ifdef _HIS_CODE_
	if (dae_frc_loop != 0 || dae_sw_en)
		dcntr_en = dctgrd_en;
#endif
	dbg_h2("%s:dcntr_en = %d\n", __func__, dcntr_en);
	w_reg_bit(FRC_ME_EN, frc_me_en, 31, 1);//reg_me_en,rtl nouse
	w_reg_bit(FRC_MELOGO_EN, melogo_en, 7, 1); //reg_melogo_en
	w_reg_bit(FRC_VP_TOP1, vp_en, 0, 1); //reg_vp_en
	w_reg_bit(VPU_DAE_WRAP_CTRL, iplogo_en, 30, 1); //reg_iplogo_en
	w_reg_bit(VPU_DAE_WRAP_CTRL, bbd_en, 29, 1); //reg_bbd_en
	w_reg_bit(FRC_BBD_MOTION_SETTING, bbd_en, 10, 1); //reg_bb_bbd_temporal_enable
	w_reg_bit(VPU_DAE_WRAP_CTRL, dcntr_en, 1, 1);

#ifdef ME_GLB_CLR_BYPASS_MC
	if (src_is_frc) {
		w_reg_bit(FRC_ME_GCV_EN, 1, 0, 1);  //reg_me_glb_clr_force_en
		w_reg_bit(FRC_ME_GCV_EN, 1, 16, 1); //reg_me_glb_clr_top_en

		w_reg_bit(FRC_ME_GCV_STEP, 0, 24, 8);  //reg_me_glb_clr_st_ofrm_idx
		w_reg_bit(FRC_ME_GCV_STEP, 0, 8, 8);   //reg_me_bypass_mc_hold_time
		w_reg_bit(FRC_ME_GCV_STEP, 1, 20, 1);  //reg_me_glb_clr_dec_en
		w_reg_bit(FRC_ME_GCV_STEP, 0, 16, 1);  //reg_me_glb_clr_dec_stp
		w_reg_bit(FRC_ME_GCV_T_APL, 0, 0, 20); //reg_me_glb_clr_t_sad
	}
#endif
	//==============================================================//
	//
	//==============================================================//
	u32 MAX_ME_MVX  = 511;
	u32 ME_MVX_BIT  = 12;
	u32 MAX_ME_MVY  = 160;
	u32 ME_MVY_BIT  = 11;
	u32 me_max_mvx = (MAX_ME_MVX << mvx_div_mode < (1 << (ME_MVX_BIT - 1)) - 1) ?
				(MAX_ME_MVX << mvx_div_mode) :
				((1 << (ME_MVX_BIT - 1)) - 1);
	u32 me_max_mvy = (MAX_ME_MVY << mvy_div_mode < (1 << (ME_MVY_BIT - 1)) - 1) ?
				(MAX_ME_MVY << mvy_div_mode) :
				((1 << (ME_MVY_BIT - 1)) - 1);

	u32 dpss_fnr_vpp_link = rd(DPSS_NR_VPP_LINK) && 0x1;

	if (dpss_fnr_vpp_link == 1)
		w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 30, 1); //reg_me_rpd_nc_en
	// w_reg_bit(FRC_ME_RPD_MAXSAD_CLIP, 2048, 16, 13); //reg_me_rpd_max_sad_clip

	w_reg_bit(VPU_NRME_TOP_CTRL3, 1, 12, 1); //reg_istop_invert todo

	if (frc_me_en | ini_cfg_dae_in_nr_frc) {
		w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvx_div_mode, 2, 2); //reg_me_mvx_div_mode
		w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvy_div_mode, 0, 2); //reg_me_mvy_div_mode

		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvx, 16, 12); //reg_me_cmv_max_mvx
		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvy, 0, 11); //reg_me_cmv_max_mvy
		w_reg_bit(FRC_ME_CMV_CTRL, mv_seed_update, 31, 1);
		//reg_me_cmv_rand_pulse
		w_reg_bit(FRC_ME_GMV_CTRL, mvx_div_mode + 2, 4, 3); //reg_gmv_mvx_sft
		w_reg_bit(FRC_ME_GMV_CTRL, mvy_div_mode + 2, 0, 3); //reg_gmv_mvy_sft
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, mvx_div_mode + 2, 17, 3);
		//reg_region_rp_gmv_mvx_sft
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, mvy_div_mode + 2, 14, 3);
		//reg_region_rp_gmv_mvy_sft
		w_reg_bit(FRC_ME_EN, 1, 4, 2); //reg_me_mvx_div_mode
		w_reg_bit(FRC_ME_EN, 1, 0, 2); //reg_me_mvy_div_mode
		// w_reg_bit(FRC_ME_EN, !frc_me_en, 30, 1); //reg_me_lpf_en
		w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 31, 1); //reg_me_rpd_cn_en
		w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 30, 1); //reg_me_rpd_cn_en
		w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 31, 1); //reg_me_add_fs_cn_en
		w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 30, 1); //reg_me_add_fs_nc_en
		w_reg_bit(FRC_ME_CAD_PRJ_EN, frc_me_en, 18, 1); //reg_me_add_prj_en
		w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 13, 1); //reg_me_loop_scan0_0
		w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 12, 1); //reg_me_loop_scan1_0
		w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 13, 1); //reg_me_loop_scan0_1
		w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 12, 1); //reg_me_loop_scan1_1
		w_reg_bit(FRC_ME_OBME_MODE_2, frc_me_en, 13, 1); //reg_me_loop_scan0_2
		w_reg_bit(FRC_ME_OBME_MODE_2, 1, 12, 1);         //reg_me_loop_scan1_2
		w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, frc_me_en, 28, 1);
		//reg_acdc_sel_mode_en_0
		w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, frc_me_en, 28, 1);
		//reg_acdc_sel_mode_en_1
		w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, frc_me_en, 28, 1);
		//reg_acdc_sel_mode_en_2
		w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 29, 1); //reg_me_zmv_patch_en
		w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 28, 1); //reg_me_gmv_patch_en
		w_reg_bit(FRC_ME_P1_EN_0, frc_me_en, 25, 1);
		//reg_me_periodic_aplsad_rule_en_0
		w_reg_bit(FRC_ME_P1_EN_1, frc_me_en, 25, 1);
		//reg_me_periodic_aplsad_rule_en_1
		w_reg_bit(FRC_ME_P1_EN_2, frc_me_en, 25, 1);
		//reg_me_periodic_aplsad_rule_en_2
		w_reg_bit(FRC_MEPP_SMOB_EN, frc_me_en, 31, 1); //reg_smob_en
		w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 0, 1);  //reg_me_periodic2_en_0
		w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 1, 1);  //reg_me_periodic2_en_1
		w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 2, 1);  //reg_me_periodic2_en_2
		w_reg_bit(FRC_ME_TOP_CTRL, 0, 29, 1);//reg_me_mode_ctrl

		if (game_mode) {
			w_reg_bit(FRC_ME_RPD_EN, 0, 31, 1); //reg_me_rpd_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 31, 1); //reg_me_add_fs_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 30, 1); //reg_me_add_fs_nc_en
			w_reg_bit(FRC_ME_CAD_PRJ_EN, 0, 18, 1); //reg_me_add_prj_en
			w_reg_bit(FRC_ME_OBME_MODE_0, 0, 13, 1); //reg_me_loop_scan0_0
			w_reg_bit(FRC_ME_OBME_MODE_0, 1, 12, 1); //reg_me_loop_scan1_0
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 13, 1); //reg_me_loop_scan0_1
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 12, 1); //reg_me_loop_scan1_1
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 13, 1); //reg_me_loop_scan0_2
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 12, 1); //reg_me_loop_scan1_2
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, 0, 28, 1);
			//reg_acdc_sel_mode_en_0
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, 0, 28, 1);
			//reg_acdc_sel_mode_en_1
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, 0, 28, 1);
			//reg_acdc_sel_mode_en_2
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 29, 1); //reg_me_zmv_patch_en
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 28, 1); //reg_me_gmv_patch_en
			w_reg_bit(FRC_ME_P1_EN_0, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_0
			w_reg_bit(FRC_ME_P1_EN_1, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_1
			w_reg_bit(FRC_ME_P1_EN_2, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_2
			w_reg_bit(FRC_MEPP_SMOB_EN, 0, 31, 1); //reg_smob_en
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 0, 1);  //reg_me_periodic2_en_0
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 1, 1);  //reg_me_periodic2_en_1
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 2, 1);  //reg_me_periodic2_en_2
			w_reg_bit(FRC_ME_TOP_CTRL, 1, 29, 1);//reg_me_mode_ctrl
		}

		w_reg_bit(FRC_ME_BB_PIX_ED, me_hsize - 1, 16, 12);//reg_me_bb_xyxy_2
		w_reg_bit(FRC_ME_BB_PIX_ED, me_vsize - 1, 0, 12); //reg_me_bb_xyxy_3
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_hsize - 1, 16, 10); //reg_me_bb_blk_xyxy_2
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_vsize - 1, 0, 10); //reg_me_bb_blk_xyxy_3
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 1 / 4, 16, 10);
		//reg_me_stat_region_hend_0
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 2 / 4, 0, 10);
		//reg_me_stat_region_hend_1
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 3 / 4, 16, 10);
		//reg_me_stat_region_hend_2
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 4 / 4, 0, 10);
		//reg_me_stat_region_hend_3
		w_reg_bit(FRC_ME_STAT_12R_V0, me_blk_vsize * 1 / 3, 0, 10);
		//reg_me_stat_region_vend_0
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 2 / 3, 16, 10);
		//reg_me_stat_region_vend_1
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 3 / 3, 0, 10);
		//reg_me_stat_region_vend_2
	}
	//just for reg_match,rtl not use
	//    w_reg_bit(FRC_REG_LOAD_ORG_FRAME_0,1,27,1);//reg_ip_film_end_0
	//    w_reg_bit(FRC_REG_OUT_FID,2,24,8);//reg_otb_cnt
	//    w_reg_bit(FRC_REG_PD_PAT_NUM,1,0,8);//reg_frc_pd_pat_num
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 0, 9, 1);//reg_dos_ds_input_mode
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 1, 8, 1);//reg_me_input_is444

	//me_mdpd.pd
	if (src_is_di && !dpss_me_en) {
		w_reg_bit(VPU_FD_X_WND0, me_hsize - 1, 0, 13); //reg_fd_x_wnd0_1
		w_reg_bit(VPU_FD_X_WND1, me_hsize - 1, 0, 13); //reg_fd_x_wnd1_1
		w_reg_bit(VPU_FD_X_WND2, me_hsize - 1, 0, 13); //reg_fd_x_wnd2_1
		w_reg_bit(VPU_FD_X_WND3, me_hsize - 1, 0, 13); //reg_fd_x_wnd3_1
		w_reg_bit(VPU_FD_Y_WND0, 0, 12, 12);//reg_fd_y_wnd0_0
		w_reg_bit(VPU_FD_Y_WND0, 1 * me_vsize / 5, 0, 12);//reg_fd_y_wnd0_1
		w_reg_bit(VPU_FD_Y_WND1, 1 * me_vsize / 5, 12, 12);//reg_fd_y_wnd1_0
		w_reg_bit(VPU_FD_Y_WND1, 2 * me_vsize / 5, 0, 12);//reg_fd_y_wnd1_1
		w_reg_bit(VPU_FD_Y_WND2, 2 * me_vsize / 5, 12, 12);//reg_fd_y_wnd2_0
		w_reg_bit(VPU_FD_Y_WND2, 3 * me_vsize / 5, 0, 12);//reg_fd_y_wnd2_1
		w_reg_bit(VPU_FD_Y_WND3, 3 * me_vsize / 5, 12, 12);//reg_fd_y_wnd3_0
		w_reg_bit(VPU_FD_Y_WND3, 4 * me_vsize / 5, 0, 12);//reg_fd_y_wnd3_1
	}
#ifdef FUNC_EN_PQ	//ary add mark
	dpss_me_width(me_hsize);
	dpss_me_height(me_vsize);
#endif
//==============================================================//
// sub-com-mif:
// change burst_len of all rd/wr mif
//==============================================================//
	dpss_dae_rdmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
	dpss_dae_wrmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
	dbg_w2("%s frc_fst_frm_in_nr= %d end\n", "dae", frc_fst_frm_in_nr);//tmp for sim
	if (dcntr_en && !src_is_frc) {
		dpss_ini_dcntr_pre(prm_dae->prm_size_ext->me_hsize,
			prm_dae->prm_size_ext->me_vsize,
			dae_grd_num_mode, 1, 0, 0, prm_top->dae_dsx_scale, prm_top->dae_dsy_scale);
	if (!dpss_tnr_en && prm_top->sw_dct_frame_cnt == 0) {
		prm_top->sw_dct_frame_cnt = prm_top->frame_count;
		dbg_h0("sw dct %d\n", prm_top->sw_dct_frame_cnt);
	}
		if (prm_top->frame_count > prm_top->sw_dct_frame_cnt && sw_dct) {
			w_reg_bit(VPU_NR_SCENCE_CHANGE_FLAG, 0x0, 0, 1);//dpss_tnr_en = 1;
			sw_dct = 0;
			prm_top->sw_dct_frame_cnt = 0;
			dbg_h0("sw tnr on\n");
		}
	}
}

// for frc dae 0 interrupt  in frc only case
u32 ini_cfg_frc_dae;
int frc_fst_frm;
bool bbd_init;
void hw_cfg_dpss_dae_frc(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae)
{
	struct PRM_INTF_TYPE *dae_yuv;
	u32 org_hsize;
	u32 org_vsize;
	enum vid_src_fmt src_fmt;
	enum vid_src_fmt src_bit;
	enum vid_src_fmt src_plane;
	bool din_pad_en = 0;
	bool game_mode   = prm_top->dpe_game_mode;
	bool src_is_frc  = (dae_mode == DAE_FRC_MODE) || game_mode;
	bool src_is_byps = dae_mode == DAE_BYPS_MODE;
	bool dae_sw_en   = prm_top->sw_ctrl_en & 0x1;
	bool dae_hpps_en = prm_top->nr_hscale_on & 0x1;
	bool frc_ds_scale_en = prm_top->frc_ds_scale_en;
	bool dae_auto_align = prm_top->auto_alig_en & 0x1;
	bool frc_bbd_en = prm_top->frc_bbd_en;
	bool frc_me_en = prm_top->frc_me_en;
	bool frc_vp_en = prm_top->frc_vp_en;
	bool frc_iplogo_en = prm_top->frc_iplogo_en;
	bool frc_melogo_en = prm_top->frc_melogo_en;
	bool frc_fbuf_only = prm_top->frc_fbuf_only;
	bool mc_skip_v = (prm_top->mc_skip_mode & 0x1) && src_is_frc;
	bool mc_skip_h = ((prm_top->mc_skip_mode >> 1) & 0x1) && src_is_frc;
	bool mc_skip = prm_top->mc_skip_mode && src_is_frc;
	bool me_skip = mc_skip && prm_top->dpe_mc_size.frm_hsize > 960;
	u32 inp_hsize, inp_vsize, me_hsize, me_vsize;
	bool frc_no_alg_ko = prm_top->frc_no_alg_ko;
	int i;

	dbg_h2("%s:dae_mode = %d, prm_dpss_dae:0x%px\n", __func__,
				dae_mode, prm_dae);
	dbg_h2("frc_fbuf_only=%d frc_ds_scale_en=%d\n", frc_fbuf_only, frc_ds_scale_en);
	dbg_h2("me_skip=%d\n", me_skip);
	dbg_h2("mc_skip_h =%d mc_skip_v=%d\n", mc_skip_h, mc_skip_v);

	w_reg_bit(VPU_DAE_WRAP_CTRL, src_is_byps, 31, 1);//reg_dae_byps

	if (src_is_byps) {
		dbg_h2("%s, dae bypass\n", __func__);
		return;
	}
	if (!src_is_frc)  {
		dbg_h2("%s, no frc dae\n", __func__);
		return;
	}

	src_fmt = frc_fbuf_only ?
		prm_top->src_fs_fmt.src_fmt : prm_top->src_ds_fmt.src_fmt;
	src_bit = frc_fbuf_only ?
		prm_top->src_fs_fmt.src_bit : prm_top->src_ds_fmt.src_bit;
	src_plane = frc_fbuf_only ?
		prm_top->src_fs_fmt.src_plan : prm_top->src_ds_fmt.src_plan;

	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize    = prm_top->org_hsize;
		org_vsize    = prm_top->org_vsize;
	} else {
		org_hsize    = (frc_ds_scale_en | mc_skip) ?
				prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
		org_vsize    = (frc_ds_scale_en | mc_skip) ?
				prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;
	}

	u32 me_dsx        = mc_skip ? 1 : prm_top->dae_dsx_scale;
	u32 me_dsy        = mc_skip ? 1 : prm_top->dae_dsy_scale;
	u32 me_hvds_en    = frc_ds_scale_en | frc_fbuf_only;

	inp_hsize     =  me_hvds_en ?  org_hsize :
				(org_hsize + (1 << me_dsx) - 1) >> me_dsx;
	inp_vsize     =  me_hvds_en ?  org_vsize :
				(org_vsize + (1 << me_dsy) - 1) >> me_dsy;

	u32 reg_mix_hds_en = (inp_hsize > 1920) ? 2 :
				(frc_ds_scale_en || frc_fbuf_only || inp_hsize > 960) ? me_dsx : 0;
	u32 reg_mix_vds_en = (inp_vsize > 1080) ? 2 :
				(frc_ds_scale_en || frc_fbuf_only || inp_vsize > 540) ? me_dsy : 0;

	me_hsize =	(dae_hpps_en) ? ((org_hsize == 4096) ? 960  : 480) :
			(me_hvds_en) ? (inp_hsize + (1 << me_dsx) - 1) >> me_dsx : inp_hsize;
	me_vsize =	(me_hvds_en) ? (inp_vsize + (1 << me_dsy) - 1) >> me_dsy : inp_vsize;

	if (dae_auto_align) {
		me_hsize = (me_hsize + 3) / 4 * 4;
		me_vsize = (me_vsize + 3) / 4 * 4;
	}
	dbg_h2("\tinp<%d,%d>, mix_ds_en:<%d,%d>\n", inp_hsize,
		inp_vsize, reg_mix_hds_en, reg_mix_vds_en);
	dbg_h2("\tme_size:<%d,%d>\n", me_hsize, me_vsize);
	u32 me_blk_hsize  = (me_hsize + 3) >> 2;
	u32 me_blk_vsize  = (me_vsize + 3) >> 2;
	//u32 mvx_div_mode  = prm_top->mvx_div_mode;
	//u32 mvy_div_mode  = prm_top->mvy_div_mode;

	struct PRM_DPSS_SIZE *prm_size_ext;

	prm_size_ext = prm_dae->prm_size_ext;
	//ary move to glb: struct PRM_DPSS_SIZE prm_size;
	prm_size_ext->frm_hsize       = inp_hsize;
	prm_size_ext->frm_vsize       = inp_vsize;
	prm_size_ext->me_hsize        = me_hsize;
	prm_size_ext->me_vsize        = me_vsize;
	prm_size_ext->me_blk_hsize    = me_blk_hsize;
	prm_size_ext->me_blk_vsize    = me_blk_vsize;
	prm_dae->prm_size         = *prm_size_ext;
	dbg_h2("%s:prm_dpss_dae: update prm_size\n", __func__);

	//==============================================================//
	// dae_size
	//==============================================================//

	wr(VPU_DAE_WRAP_SW_SIZE0,	(inp_vsize   & 0x1fff) << 16 | //inp_size
					(inp_hsize   & 0x1fff));//inp_size
	wr(VPU_DAE_WRAP_SW_SIZE1,	(me_vsize    & 0x1fff) << 16 | //frm_size
					(me_hsize    & 0x1fff));//frm_size
	wr(VPU_DAE_WRAP_SW_SIZE2,	(me_blk_vsize & 0x1fff) << 16 | //blk_size
					(me_blk_hsize & 0x1fff));//blk_size
	w_reg_bit(VPU_DAE_WRAP_SW_SEL0, 0x3f, 9, 6);//reg_mode
	//==============================================================//
	// dae baddr,only cur
	// 0x82-MC reg
	// 0x83-NR reg
	//==============================================================//

	u32 mc_dae_proc_st = (rd(DPSS_FRC_PROC_STATUS) >> 8) & 0x7;
	u32 nr_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS) >> 16) & 0x7;
	u32 di_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS + (0x86 - 0x83) * 256) >> 16) & 0x7;
	u32 luma_baddr_x16[16];
	u32 chrm_baddr_x16[16];
	u32 dae_frc_loop = mc_dae_proc_st == 3 ? 0 :	//ary:frc me? is 3
				nr_dae_proc_st == 3 ? 1 :	//
				di_dae_proc_st == 3 ? 2 : 0;

	dbg_h2("%s:reg:st:mc=0x%x, nr=0x%x, di = 0x%x\n", __func__,
		mc_dae_proc_st, nr_dae_proc_st, di_dae_proc_st);

	for (i = 0; i < 16; i++) {
		luma_baddr_x16[i] = me_hvds_en ? prm_top->src0_fbuf_yaddr[i] :
				prm_top->src0_dwbuf_yaddr[i];
		chrm_baddr_x16[i] = me_hvds_en ? prm_top->src0_fbuf_caddr[i] :
				prm_top->src0_dwbuf_caddr[i];
	}

	u32 mc_rd_idx  = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	u32 nr_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX) >> 20) & 0xf;
	u32 di_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX + (0x86 - 0x83) * 256) >> 20) & 0xf;
	//ary addr change *4
	u32 cur_rd_idx =	dae_frc_loop == 0 ? mc_rd_idx :
				dae_frc_loop == 1 ? nr_rd_idx :
				dae_frc_loop == 2 ? di_rd_idx : 0;

	dbg_h2("%s:reg:idx:mc=0x%x, nr=0x%x, di = 0x%x, cur=0x%x\n", __func__,
		mc_rd_idx, nr_rd_idx, di_rd_idx, cur_rd_idx);

	//u32 luma_raddr = luma_baddr + luma_step * cur_rd_idx ;
	//u32 chrm_raddr = chrm_baddr + chrm_step * cur_rd_idx ;
	u32 luma_raddr; // = luma_baddr_x16[cur_rd_idx] << 5;
	u32 chrm_raddr; // = chrm_baddr_x16[cur_rd_idx] << 5;

	if (dae_frc_loop == 0x0 && me_hvds_en) {
		luma_raddr = luma_baddr_x16[cur_rd_idx];
		chrm_raddr = chrm_baddr_x16[cur_rd_idx];
	} else {
		luma_raddr = luma_baddr_x16[cur_rd_idx] << 5;
		chrm_raddr = chrm_baddr_x16[cur_rd_idx] << 5;
	}

	if (dae_sw_en == 0 && prm_top->fnr_sw_mode == 0) { //ary ? need set?? for auto mode
		dbg_h2("%s:y_addr = 0x%x, uv_addr = 0x%x\n", __func__, luma_raddr, chrm_raddr);
		dbg_h2("%s: DAE_WRAP_SW_ADDR11 <= 0x%x, next_y_addr?\n", __func__, luma_raddr);
		wr(VPU_DAE_WRAP_SW_ADDR11, luma_raddr);//nxt_luma_addr
		wr(VPU_DAE_WRAP_SW_ADDR12, chrm_raddr);//nxt_chrm_addr
		//        w_reg_bit(VPU_DAE_WRAP_SW_SEL1,3,1,2);//sel
	}

	//Augustine: add for mif mmu mode
	prm_dae->dae_yuv_mif.src_baddr[0] = luma_raddr << 4;
	prm_dae->dae_yuv_mif.src_baddr[1] = chrm_raddr << 4;

	dbg_h2("dae_frc_loop=%d cur_rd_idx=%d\n", dae_frc_loop, cur_rd_idx);
	dbg_h2("luma_raddr=0x%x chrm_raddr=0x%x\n", luma_raddr, chrm_raddr);
	//    dbg_h2("luma_step=0x%x chrm_step=0x%x\n",luma_step,chrm_step);
	// if (dae_mode == DAE_FRC_MODE)
	dae_yuv = prm_dae->ext_yuv_intf;

	dae_yuv->src_hsize = inp_hsize;
	dae_yuv->src_vsize = inp_vsize;
	dae_yuv->src_bit   = src_bit;
	dae_yuv->src_plan  = src_plane;
	dae_yuv->src_fmt   = src_fmt;
	dae_yuv->src_baddr[0] = prm_dae->dae_yuv_mif.src_baddr[0];
	dae_yuv->src_baddr[1] = prm_dae->dae_yuv_mif.src_baddr[1];

	dae_yuv->slc_x_st[0]  = 0;
	dae_yuv->slc_x_ed[0]  = inp_hsize - 1;
	dae_yuv->slc_y_st[0]  = 0;
	//yu.zong 2024-12-06	dae_yuv_intf.slc_y_ed[0]  = inp_vsize -1;
	dae_yuv->slc_y_ed[0] = (prm_top->di_interlace ? inp_vsize * 2 : inp_vsize) - 1;
	//yu.zong 2024-12-06

	dae_yuv->bits_mode  = prm_dae->dae_yuv_mif.bits_mode;
	dae_yuv->pack_mode  = prm_dae->dae_yuv_mif.pack_mode;

	dae_yuv->reverse[0] =  (prm_top->frc_rev_mode & 0x1) |
			(prm_top->comp_setting.vfcd_rev_mode & 0x1);
	dae_yuv->reverse[1] = ((prm_top->frc_rev_mode & 0x2) >> 1) |
			(prm_top->comp_setting.vfcd_rev_mode & 0x2) >> 1;
	//yu.zong 2024-12-06	dae_yuv_intf.skip_line =0 ;
	dae_yuv->skip_line = 0;
	dae_yuv->reg_mode  = 0x0;
	dae_yuv->field_mode = 0;//0:close 1:odd->even 2:even->odd
	//0 ;//0:close 1:odd->even 2:even->odd	//yu.zong 2024-12-06

	dbg_h2("skip_line=%d, reg_mode=%d, field_mode=%d\n",
		dae_yuv->skip_line,
		dae_yuv->reg_mode,
		dae_yuv->field_mode);
	dae_yuv->mmu_mode_en = prm_top->ds_mif_mmu_mode_en;
	dae_yuv->uv_swap = 0;//dpss_dbg_uv_swap & C_BIT0;
	dae_yuv->little_endian = 0;//dpss_dbg_uv_swap & C_BIT1; //ary add default is 1;
	dae_yuv->swap_64bit = 1;//dpss_dbg_uv_swap & C_BIT2;
	dbg_h2("dae uv_swap: %d, little_endian:%d, swap-64bit:%d\n",
		dae_yuv->uv_swap,
		dae_yuv->little_endian,
		dae_yuv->swap_64bit);

	dpss_cfg_viu_pix_rdmif(dae_yuv, 1);

	//w_reg_bit(VPU_DAE_WRAP_CTRL,1, 0,1);// reg_dae_start_sel   0:hw_auto  1:pls

	//==============================================================//
	// dae_wrap
	//==============================================================//
	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, 0, 31, 1); //reg_tfbf_en

	u32 di_in_mode     = src_fmt == YUV444 ? 0 : src_fmt == YUV422 ? 1 : 2;
	u32 di_bit_tight   = (src_bit == BIT_010);//0:10 in 16bit 1:10 in 10bit
	u32 di_bit         = (src_bit == BIT_010) | (src_bit == BIT_P010);

	din_pad_en = ((inp_hsize != (me_hsize * (1 << reg_mix_hds_en))) |
		(inp_vsize != (me_vsize * (1 << reg_mix_vds_en)))) & (dae_hpps_en == 0);

	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, game_mode, 0, 1);//reg_game_mode
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE1, game_mode, 4, 1);//reg_llm_en

	w_reg_bit(VPU_DAE_WRAP_DI, ((di_bit_tight & 0x1) << 4) |
					((di_in_mode & 0x3) << 0), 18, 5);
					//((di_ds_mode & 0x3) << 0), 16, 7);

	w_reg_bit(VPU_DAE_WRAP_DI, di_bit & 0x1, 4, 1);
	if ((prm_top->frame_count < 5 && dpss_frc_bypass) || !dpss_frc_bypass)
		w_reg_bit(VPU_DAE_WRAP_MIX_DS, (reg_mix_vds_en & 0x3) << 2 |
			(reg_mix_hds_en & 0x3), 0, 4);

	w_reg_bit(VPU_DAE_WRAP_DI, src_plane == PLANAR_X1, 24, 1);//reg_yuv444_one_cavs
		w_reg_bit(VPU_DAE_WRAP_DI, 1, 27, 1);//reg_me_bld_inp_mode
		w_reg_bit(VPU_DAE_WRAP_DI, 0xd, 28, 4);//reg_me_bld_y_gain
		w_reg_bit(VPU_DAE_WRAP_DI, 0x44, 8, 8);
		//reg_me_bld_v_gain/reg_me_bld_u_gain

	w_reg_bit(VPU_DAE_WRAP_PAD_CTRL, din_pad_en, 0, 1);

	if (di_in_mode == YUV420)
		w_reg_bit(DAE_VD_CFMT_CTRL, 1, 28, 1); //fmt up repeat
	else
		w_reg_bit(DAE_VD_CFMT_CTRL, 0, 28, 1);
	//fmt up repeat

	// need reflash with set 0 in dae0 irq
	w_reg_bit(FRC_ME_TOP_CTRL2, 0, 16, 1);	//for p source read PD
	dbg_h2("%s:prm_dpss_dae0:%x\n", __func__, rd(FRC_ME_TOP_CTRL2));

	ini_cfg_frc_dae++;
	if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0) {
		dbg_f2("dae0 not ctrl_0 reg cnt:%d\n", ini_cfg_frc_dae);
		return; // test not ok
	} else if (ini_cfg_frc_dae > 0xfffffff0) {
		ini_cfg_frc_dae = 3;
	}
	//==============================================================//
	// frc_me/vp/melogo/iplogo/bbd enable
	//==============================================================//
	if (game_mode)
		frc_vp_en = 0;
	if (ini_cfg_frc_dae > 0 && frc_me_en) {
		w_reg_bit(FRC_ME_EN, frc_me_en, 31, 1);//reg_me_en,rtl nouse
		w_reg_bit(FRC_MELOGO_EN, 1, 7, 1); //frc_melogo_en must 1
		w_reg_bit(FRC_VP_TOP1, frc_vp_en, 0, 1); //reg_vp_en
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_iplogo_en, 30, 1); //reg_iplogo_en
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_bbd_en, 29, 1); //reg_bbd_en
		w_reg_bit(FRC_BBD_MOTION_SETTING, frc_bbd_en, 10, 1); //reg_bb_bbd_temporal_enable
		// w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 1, 1);
	}
	w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 1, 1);

#ifdef ME_GLB_CLR_BYPASS_MC
	if (src_is_frc) {
		w_reg_bit(FRC_ME_GCV_EN, 1, 0, 1);  //reg_me_glb_clr_force_en
		w_reg_bit(FRC_ME_GCV_EN, 1, 16, 1); //reg_me_glb_clr_top_en

		w_reg_bit(FRC_ME_GCV_STEP, 0, 24, 8);  //reg_me_glb_clr_st_ofrm_idx
		w_reg_bit(FRC_ME_GCV_STEP, 0, 8, 8);   //reg_me_bypass_mc_hold_time
		w_reg_bit(FRC_ME_GCV_STEP, 1, 20, 1);  //reg_me_glb_clr_dec_en
		w_reg_bit(FRC_ME_GCV_STEP, 0, 16, 1);  //reg_me_glb_clr_dec_stp
		w_reg_bit(FRC_ME_GCV_T_APL, 0, 0, 20); //reg_me_glb_clr_t_sad
	}
#endif
	//==============================================================//
	//
	//==============================================================//
	u32 MAX_ME_MVX  = 511;
	u32 ME_MVX_BIT  = 12;
	u32 MAX_ME_MVY  = 160;
	u32 ME_MVY_BIT  = 11;
	u32 me_max_mvx = (MAX_ME_MVX << 2 < (1 << (ME_MVX_BIT - 1)) - 1) ?
				(MAX_ME_MVX << 2) :
				((1 << (ME_MVX_BIT - 1)) - 1);
	u32 me_max_mvy = (MAX_ME_MVY << 2 < (1 << (ME_MVY_BIT - 1)) - 1) ?
				(MAX_ME_MVY << 2) :
				((1 << (ME_MVY_BIT - 1)) - 1);

	// w_reg_bit(FRC_ME_RPD_MAXSAD_CLIP, 2048, 16, 13); //reg_me_rpd_max_sad_clip
	w_reg_bit(VPU_NRME_TOP_CTRL3, 1, 12, 1); //reg_istop_invert todo

	if (ini_cfg_frc_dae && frc_me_en) {
		w_reg_bit(FRC_ME_EN, frc_me_en, 31, 1);//reg_me_en,rtl nouse
		if (frc_no_alg_ko == 1) {
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_0, frc_me_en, 0, 1);//reg_me_en,rtl nouse
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_1, frc_me_en, 0, 1);//reg_me_en,rtl nouse
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_2, frc_me_en, 0, 1);//reg_me_en,rtl nouse
		}
		//w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvx_div_mode, 2, 2); //reg_me_mvx_div_mode
		//w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvy_div_mode, 0, 2); //reg_me_mvy_div_mode
		//w_reg_bit(FRC_ME_TOP_CTRL2, 0, 16, 1);  //for p source read PD
		//dbg_h2("%s:prm_dpss_dae0:%x\n", __func__, rd(FRC_ME_TOP_CTRL2));
		//if (ini_cfg_frc_dae > 4 && frc_melogo_en == 0) {
		//	dbg_f2("dae0 not ctrl_1 reg cnt:%d\n", ini_cfg_frc_dae);
		//	return;  // test not ok
		//}
		if (!game_mode) { // no game mode
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 31, 1); //reg_me_rpd_cn_en
				w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 30, 1); //reg_me_rpd_cn_en
			}
			if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0) {
				dbg_f2("dae0 not ctrl reg_2 cnt:%d\n", ini_cfg_frc_dae);
				return;  // test ok
			}
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 31, 1); //reg_me_add_fs_cn_en
				w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 30, 1); //reg_me_add_fs_nc_en
				w_reg_bit(FRC_ME_CAD_PRJ_EN, frc_me_en, 18, 1); //reg_me_add_prj_en
			}
			w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 13, 1); //reg_me_loop_scan0_0
			w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 12, 1); //reg_me_loop_scan1_0
			w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 13, 1); //reg_me_loop_scan0_1
			w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 12, 1); //reg_me_loop_scan1_1
			w_reg_bit(FRC_ME_OBME_MODE_2, frc_me_en, 13, 1); //reg_me_loop_scan0_2
			w_reg_bit(FRC_ME_OBME_MODE_2, 1, 12, 1);         //reg_me_loop_scan1_2
			//if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0) {
			//	dbg_f2("dae0 not ctrl reg_3 cnt:%d\n", ini_cfg_frc_dae);
			//	return;  // test ok
			//}
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_0
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_1
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_2
				w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 29, 1);
				w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 28, 1);
				w_reg_bit(FRC_ME_P1_EN_0, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_0
				w_reg_bit(FRC_ME_P1_EN_1, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_1
				w_reg_bit(FRC_ME_P1_EN_2, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_2
				w_reg_bit(FRC_MEPP_SMOB_EN, frc_me_en, 31, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 0, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 1, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 2, 1);
			}
			w_reg_bit(FRC_ME_TOP_CTRL, 0, 29, 1);//reg_me_mode_ctrl
		} else {  // (game_mode)
			w_reg_bit(FRC_ME_RPD_EN, 0, 31, 1); //reg_me_rpd_cn_en
			w_reg_bit(FRC_ME_RPD_EN, 0, 30, 1); //reg_me_rpd_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 31, 1); //reg_me_add_fs_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 30, 1); //reg_me_add_fs_nc_en
			w_reg_bit(FRC_ME_CAD_PRJ_EN, 0, 18, 1); //reg_me_add_prj_en
			w_reg_bit(FRC_ME_OBME_MODE_0, 0, 13, 1); //reg_me_loop_scan0_0
			w_reg_bit(FRC_ME_OBME_MODE_0, 1, 12, 1); //reg_me_loop_scan1_0
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 13, 1); //reg_me_loop_scan0_1
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 12, 1); //reg_me_loop_scan1_1
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 13, 1); //reg_me_loop_scan0_2
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 12, 1); //reg_me_loop_scan1_2
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, 0, 28, 1);
			//reg_acdc_sel_mode_en_0
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, 0, 28, 1);
			//reg_acdc_sel_mode_en_1
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, 0, 28, 1);
			//reg_acdc_sel_mode_en_2
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 29, 1); //reg_me_zmv_patch_en
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 28, 1); //reg_me_gmv_patch_en
			w_reg_bit(FRC_ME_P1_EN_0, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_0
			w_reg_bit(FRC_ME_P1_EN_1, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_1
			w_reg_bit(FRC_ME_P1_EN_2, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_2
			w_reg_bit(FRC_MEPP_SMOB_EN, 0, 31, 1); //reg_smob_en
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 0, 1);  //reg_me_periodic2_en_0
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 1, 1);  //reg_me_periodic2_en_1
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 2, 1);  //reg_me_periodic2_en_2
			w_reg_bit(FRC_ME_TOP_CTRL, 1, 29, 1);//reg_me_mode_ctrl
		}

		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvx, 16, 12); //reg_me_cmv_max_mvx
		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvy, 0, 11); //reg_me_cmv_max_mvy
		// w_reg_bit(FRC_ME_CMV_CTRL, mv_seed_update, 31, 1);
		//reg_me_cmv_rand_pulse
		w_reg_bit(FRC_ME_GMV_CTRL, 2 + 2, 4, 3); //reg_gmv_mvx_sft
		w_reg_bit(FRC_ME_GMV_CTRL, 2 + 2, 0, 3); //reg_gmv_mvy_sft
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, 2 + 2, 17, 3);
		//reg_region_rp_gmv_mvx_sft
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, 2 + 2, 14, 3);
		//reg_region_rp_gmv_mvy_sft
		w_reg_bit(FRC_ME_EN, 2, 4, 2); //reg_me_mvx_div_mode
		w_reg_bit(FRC_ME_EN, 2, 0, 2); //reg_me_mvy_div_mode
		// w_reg_bit(FRC_ME_EN, !frc_me_en, 30, 1); //reg_me_lpf_en

		//if (ini_cfg_frc_dae > 4 && frc_melogo_en == 0) {
		//	dbg_f2("dae0 not ctrl_4 reg cnt:%d\n", ini_cfg_frc_dae);
		//	return;  // test ok
		//}
		w_reg_bit(FRC_ME_BB_PIX_ED, me_hsize - 1, 16, 12);//reg_me_bb_xyxy_2
		w_reg_bit(FRC_ME_BB_PIX_ED, me_vsize - 1, 0, 12); //reg_me_bb_xyxy_3
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_hsize - 1, 16, 10); //reg_me_bb_blk_xyxy_2
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_vsize - 1, 0, 10); //reg_me_bb_blk_xyxy_3
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 1 / 4, 16, 10);
		//reg_me_stat_region_hend_0
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 2 / 4, 0, 10);
		//reg_me_stat_region_hend_1
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 3 / 4, 16, 10);
		//reg_me_stat_region_hend_2
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 4 / 4, 0, 10);
		//reg_me_stat_region_hend_3
		w_reg_bit(FRC_ME_STAT_12R_V0, me_blk_vsize * 1 / 3, 0, 10);
		//reg_me_stat_region_vend_0
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 2 / 3, 16, 10);
		//reg_me_stat_region_vend_1
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 3 / 3, 0, 10);
		//reg_me_stat_region_vend_2
	}

	if (ini_cfg_frc_dae >= 2) {
		dbg_f2("dae0 frc return:%d\n", ini_cfg_frc_dae);
		return;
	}
	//just for reg_match,rtl not use
	//    w_reg_bit(FRC_REG_LOAD_ORG_FRAME_0,1,27,1);//reg_ip_film_end_0
	//    w_reg_bit(FRC_REG_OUT_FID,2,24,8);//reg_otb_cnt
	//    w_reg_bit(FRC_REG_PD_PAT_NUM,1,0,8);//reg_frc_pd_pat_num
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 0, 9, 1);//reg_dos_ds_input_mode
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 1, 8, 1);//reg_me_input_is444

	//ini_cfg_frc_dae = 1;
	//==============================================================//
	// VP
	//==============================================================//
	if (frc_vp_en && ini_cfg_frc_dae > 0) {
		w_reg_bit(FRC_VP_TOP1, frc_vp_en, 0, 1); //reg_vp_en

		w_reg_bit(FRC_VP_TOP1, 2, 12, 2); //reg_vp_mvx_div_mode
		w_reg_bit(FRC_VP_TOP1, 2, 8, 2); //reg_vp_mvy_div_mode
		w_reg_bit(FRC_VP_BB_2, me_blk_hsize - 1, 0, 16);
		w_reg_bit(FRC_VP_BB_2, me_blk_vsize - 1, 16, 16);
		w_reg_bit(FRC_VP_ME_BB_2, me_blk_hsize - 1, 0, 16);
		w_reg_bit(FRC_VP_ME_BB_2, me_blk_vsize - 1, 16, 16);
		w_reg_bit(FRC_VP_REGION_WINDOW_1, me_blk_hsize * 1 / 4,  8, 12);
		//reg_vp_stat_region_hend_0
		w_reg_bit(FRC_VP_REGION_WINDOW_1, me_blk_hsize * 2 / 4, 20, 12);
		//reg_vp_stat_region_hend_1
		w_reg_bit(FRC_VP_REGION_WINDOW_2, me_blk_hsize * 3 / 4,  0, 12);
		//reg_vp_stat_region_hend_2
		w_reg_bit(FRC_VP_REGION_WINDOW_2, me_blk_hsize * 4 / 4, 12, 12);
		//reg_vp_stat_region_hend_3
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 1 / 3,  0,  8);
		//reg_vp_stat_region_vend_0
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 2 / 3,  8, 12);
		//reg_vp_stat_region_vend_1
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 3 / 3, 20, 12);
		//reg_vp_stat_region_vend_2
	}

	//==============================================================//
	// melogo
	//==============================================================//
	if (frc_melogo_en && ini_cfg_frc_dae > 0) {
		wr(FRC_MELOGO_BB_BLK_ED, (me_blk_hsize - 1) << 16 |
			//reg_melogo_bb_blk_xyxy_2
					(me_blk_vsize - 1));
		//reg_melogo_bb_blk_xyxy_3
		wr(FRC_MELOGO_REGION_HWINDOW_2, (me_blk_hsize * 4 / 4) << 16 |
		//reg_melogo_stat_region_hend_3
						(me_blk_hsize * 3 / 4));
		//reg_melogo_stat_region_hend_2
		wr(FRC_MELOGO_REGION_HWINDOW_1, (me_blk_hsize * 2 / 4) << 16 |
		//reg_melogo_stat_region_hend_1
						(me_blk_hsize * 1 / 4));
		//reg_melogo_stat_region_hend_0
		wr(FRC_MELOGO_REGION_VWINDOW_1, (me_blk_vsize * 3 / 3) << 16 |
		//reg_melogo_stat_region_vend_2
						(me_blk_vsize * 2 / 3));
		//reg_melogo_stat_region_vend_1
		w_reg_bit(FRC_MELOGO_REGION_VWINDOW_0, me_blk_vsize * 1 / 3, 16, 10);
		//reg_melogo_stat_region_vend_0
	}

	//==============================================================//
	// iplogo
	//==============================================================//
	if (frc_iplogo_en && ini_cfg_frc_dae > 0) {
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_iplogo_en, 30, 1); //reg_iplogo_en

		wr(FRC_IPLOGO_BB_PIX_ED, (me_hsize - 1) << 16 |
					(me_vsize - 1)); //reg_iplogo_bb_xyxy_2/3
		wr(FRC_IPLOGO_REGION_HWINDOW_2, (me_hsize * 4 / 4) << 16 |
					(me_hsize * 3 / 4)); //reg_iplogo_stat_region_hend_3/2
		wr(FRC_IPLOGO_REGION_HWINDOW_1, (me_hsize * 2 / 4) << 16 |
					(me_hsize * 1 / 4)); //reg_iplogo_stat_region_hend_1/0
		wr(FRC_IPLOGO_REGION_VWINDOW_1, (me_vsize * 3 / 3) << 16 |
					(me_vsize * 2 / 3)); // reg_iplogo_stat_region_vend_3/2
		w_reg_bit(FRC_IPLOGO_REGION_VWINDOW_0, me_vsize * 1 / 3, 16, 10);
		//reg_iplogo_stat_region_vend_0
	}

	//==============================================================//
	// bbd
	//==============================================================//
	if (frc_bbd_en && bbd_init) {
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_bbd_en, 29, 1); //reg_bbd_en
		w_reg_bit(FRC_BBD_MOTION_SETTING, frc_bbd_en, 10, 1); //reg_bb_bbd_temporal_enable
		cfg_dpss_me_bbd(me_hsize, me_vsize);
		bbd_init = 0;
	}

//==============================================================//
// sub-com-mif:
// change burst_len of all rd/wr mif
//==============================================================//
	dpss_dae_rdmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
	dpss_dae_wrmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
	dbg_w2("%s frc end, cnt:%d\n", "dae0", ini_cfg_frc_dae);//tmp for sim
}

// for frc dae 0 interrupt  in nr_frc case
void hw_cfg_dpss_dae0_frc(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae)
{
	struct PRM_INTF_TYPE *dae_yuv;
	u32 org_hsize;
	u32 org_vsize;
	bool din_pad_en = 0;
	bool game_mode   = prm_top->dpe_game_mode;
	bool src_is_frc  = (dae_mode == DAE_FRC_MODE) || game_mode;
	bool src_is_byps = dae_mode == DAE_BYPS_MODE;
	bool dae_sw_en   = prm_top->sw_ctrl_en & 0x1;
	bool dae_hpps_en = prm_top->nr_hscale_on & 0x1;
	bool frc_ds_scale_en = prm_top->frc_ds_scale_en;
	bool dae_auto_align = prm_top->auto_alig_en & 0x1;
	bool frc_bbd_en = prm_top->frc_bbd_en;
	bool frc_me_en = prm_top->frc_me_en;
	bool frc_vp_en = prm_top->frc_vp_en;
	bool frc_iplogo_en = prm_top->frc_iplogo_en;
	bool frc_melogo_en = prm_top->frc_melogo_en;
	bool dpss_nr_bypass = prm_top->dpss_nr_byp;
	bool frc_no_alg_ko = prm_top->frc_no_alg_ko;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	u32 dpe_pps_en = (pps->pps_en | prm_top->nr_aapps_on) & src_is_frc;
	int i;
	u32 inp_hsize, inp_vsize, me_hsize, me_vsize;

	u32 v_limit = 540;
	static u16 pre_me_hsize, pre_me_vsize;

	w_reg_bit(VPU_DAE_WRAP_CTRL, src_is_byps, 31, 1);//reg_dae_byps
	if (src_is_byps) {
		dbg_f2("dae0 bypass\n");
		return;
	}

	enum vid_src_fmt src_fmt = prm_top->nro_ds_fmt.src_fmt;
	enum vid_src_fmt src_bit = prm_top->nro_ds_fmt.src_bit;
	enum vid_src_fmt src_plane  = prm_top->nro_ds_fmt.src_plan;

	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize    = dpe_pps_en ?
			prm_top->dpe_mc_size.frm_hsize : prm_top->org_hsize;
		org_vsize    = dpe_pps_en ?
			prm_top->dpe_mc_size.frm_vsize : prm_top->org_vsize;
	} else {
		org_hsize    = dpe_pps_en ?
			prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
		org_vsize    = dpe_pps_en ?
			prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;
	}

	u32 me_dsx        = prm_top->dpe_dw_dsx;
	u32 me_dsy        = prm_top->dpe_dw_dsy;
	u32 me_hvds_en    = prm_top->frc_ds_scale_en;

	inp_hsize     = me_hvds_en ?  org_hsize : (org_hsize + (1 << me_dsx) - 1) >> me_dsx;
	inp_vsize     = me_hvds_en ?  org_vsize : (org_vsize + (1 << me_dsy) - 1) >> me_dsy;
	if (dpe_pps_en && prm_top->is_i && org_hsize == 1440 && org_vsize == 1152)
		v_limit = 576;
	u32 reg_mix_hds_en = (inp_hsize > 1920) ? 2 :
				(frc_ds_scale_en || inp_hsize > 960) ? 1 : 0; //need_check
	u32 reg_mix_vds_en = (inp_vsize > 1080) ? 2 :
				(frc_ds_scale_en || inp_vsize > v_limit) ? 1 : 0; //need_check

	me_hsize =	(dae_hpps_en) ? ((org_hsize == 4096) ? 960  : 480) :
			(me_hvds_en) ? (inp_hsize + (1 << me_dsx) - 1) >> me_dsx : inp_hsize;
	me_vsize =	(me_hvds_en) ? (inp_vsize + (1 << me_dsy) - 1) >> me_dsy : inp_vsize;

	if (dae_auto_align) {
		me_hsize = (me_hsize + 3) / 4 * 4;
		me_vsize = (me_vsize + 3) / 4 * 4;
	}

	u32 me_blk_hsize  = (me_hsize + 3) >> 2;
	u32 me_blk_vsize  = (me_vsize + 3) >> 2;
	//u32 mvx_div_mode  = prm_top->mvx_div_mode;
	//u32 mvy_div_mode  = prm_top->mvy_div_mode;

	struct PRM_DPSS_SIZE *prm_size_ext;

	prm_size_ext = prm_dae->prm_size_ext;
	//ary move to glb: struct PRM_DPSS_SIZE prm_size;
	prm_size_ext->frm_hsize       = inp_hsize;
	prm_size_ext->frm_vsize       = inp_vsize;
	prm_size_ext->me_hsize        = me_hsize;
	prm_size_ext->me_vsize        = me_vsize;
	prm_size_ext->me_blk_hsize    = me_blk_hsize;
	prm_size_ext->me_blk_vsize    = me_blk_vsize;
	prm_dae->prm_size         = *prm_size_ext;

	if (pre_me_hsize != me_hsize || pre_me_vsize != me_vsize) {
		ini_cfg_frc_dae = 0;
		pre_me_hsize = me_hsize;
		pre_me_vsize = me_vsize;
		dbg_h2("me size chg\n");
	}

	if (pre_me_hsize != me_hsize || pre_me_vsize != me_vsize) {
		ini_cfg_frc_dae = 0;
		pre_me_hsize = me_hsize;
		pre_me_vsize = me_vsize;
		dbg_h2("me size chg\n");
	}

	//==============================================================//
	// dae_size
	//==============================================================//

	wr(VPU_DAE_WRAP_SW_SIZE0, (inp_vsize & 0x1fff) << 16 | (inp_hsize & 0x1fff));
	wr(VPU_DAE_WRAP_SW_SIZE1, (me_vsize & 0x1fff) << 16 | (me_hsize & 0x1fff));
	wr(VPU_DAE_WRAP_SW_SIZE2, (me_blk_vsize & 0x1fff) << 16 | (me_blk_hsize & 0x1fff));
	w_reg_bit(VPU_DAE_WRAP_SW_SEL0, 0x3f, 9, 6);//reg_mode
	//==============================================================//
	// dae baddr,only cur
	// 0x82-MC reg
	// 0x83-NR reg
	//==============================================================//

	u32 mc_dae_proc_st = (rd(DPSS_FRC_PROC_STATUS) >> 8) & 0x7;
	u32 nr_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS) >> 16) & 0x7;
	u32 di_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS + (0x86 - 0x83) * 256) >> 16) & 0x7;
	u32 src0_i = 0;
	u32 src1_i = 1;
	u32 luma_baddr_x16[16];
	u32 chrm_baddr_x16[16];
	u32 dae_frc_loop = mc_dae_proc_st == 3 ? 0 :
				nr_dae_proc_st == 3 ? 1 :
				di_dae_proc_st == 3 ? 2 : 0;

	u32 nr_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_FRC_NR_MODE ||
				(prm_top->dpss_mode == DPSS_FRC_NRBYPS_MODE &&
				(~prm_top->bbd_only)));
	u32 di_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_NRDI_FRC_SRC0_MODE ||
				prm_top->dpss_mode == DPSS_DI_FRC_SRC0_MODE);
	for (i = 0; i < 16; i++) {
		luma_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_yaddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_yaddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_yaddr[i] :
					dae_frc_loop == 0x1 ?
						(src0_i ? prm_top->src0_fbuf_yaddr[i] :
							prm_top->src0_dwbuf_yaddr[i]) ://src0_p
					(src1_i ? prm_top->src1_fbuf_yaddr[i] :
					prm_top->src1_dwbuf_yaddr[i]);//src1_p
		chrm_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_caddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_caddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_caddr[i] :
					dae_frc_loop == 0x1 ?
						(src0_i ? prm_top->src0_fbuf_caddr[i] :
					prm_top->src0_dwbuf_caddr[i]) ://src0_p
					(src1_i ? prm_top->src1_fbuf_caddr[i] :
					prm_top->src1_dwbuf_caddr[i]);//src1_p
	}

	u32 mc_rd_idx  = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	u32 nr_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX) >> 20) & 0xf;
	u32 di_rd_idx  = (rd(DPSS_FBUF_DAE_LOOP_IDX + (0x86 - 0x83) * 256) >> 20) & 0xf;
	u32 cur_rd_idx = dae_frc_loop == 0 ? mc_rd_idx :
				dae_frc_loop == 1 ? nr_rd_idx :
				dae_frc_loop == 2 ? di_rd_idx : 0;

	u32 luma_raddr = luma_baddr_x16[cur_rd_idx] << 5;
	u32 chrm_raddr = chrm_baddr_x16[cur_rd_idx] << 5;

	if (dae_sw_en == 0 && prm_top->fnr_sw_mode == 0) { //ary ? need set?? for auto mode
		dbg_h2("%s:y_addr = 0x%x, uv_addr = 0x%x\n", __func__, luma_raddr, chrm_raddr);
		dbg_h2("%s: DAE_WRAP_SW_ADDR11 <= 0x%x, next_y_addr?\n", __func__, luma_raddr);
		wr(VPU_DAE_WRAP_SW_ADDR11, luma_raddr);//nxt_luma_addr
		wr(VPU_DAE_WRAP_SW_ADDR12, chrm_raddr);//nxt_chrm_addr
	}

	//Augustine: add for mif mmu mode
	prm_dae->dae_yuv_mif.src_baddr[0] = luma_raddr << 4;
	prm_dae->dae_yuv_mif.src_baddr[1] = chrm_raddr << 4;

	if (dae_mode == DAE_FRC_MODE)
		dae_yuv = prm_dae->ext_yuv_intf;
	else
		dae_yuv = &prm_dae->nr_yuv_intf;

	dae_yuv->src_hsize = inp_hsize;
	dae_yuv->src_vsize = inp_vsize;
	dae_yuv->src_bit   = src_bit;
	dae_yuv->src_plan  = src_plane;
	dae_yuv->src_fmt   = src_fmt;
	dae_yuv->src_baddr[0] = prm_dae->dae_yuv_mif.src_baddr[0];
	dae_yuv->src_baddr[1] = prm_dae->dae_yuv_mif.src_baddr[1];

	dae_yuv->slc_x_st[0] = 0;
	dae_yuv->slc_x_ed[0] = inp_hsize - 1;
	dae_yuv->slc_y_st[0] = 0;
	dae_yuv->slc_y_ed[0] = inp_vsize - 1;

	dae_yuv->bits_mode  = prm_dae->dae_yuv_mif.bits_mode;
	dae_yuv->pack_mode  = prm_dae->dae_yuv_mif.pack_mode;

	dae_yuv->reverse[0] =  (prm_top->frc_rev_mode & 0x1) |
			(prm_top->comp_setting.vfcd_rev_mode & 0x1);
	dae_yuv->reverse[1] = ((prm_top->frc_rev_mode & 0x2) >> 1) |
			(prm_top->comp_setting.vfcd_rev_mode & 0x2) >> 1;
	dae_yuv->skip_line = 0;

	dae_yuv->mmu_mode_en = prm_top->ds_mif_mmu_mode_en;

	dae_yuv->uv_swap = 0;
	dae_yuv->little_endian = prm_top->l_endian;
	dae_yuv->swap_64bit = prm_top->swap_64bit;
	dae_yuv->block_mode = 0;//prm_top->block_mode;

	dpss_cfg_viu_pix_rdmif(dae_yuv, 1);

	//bool ini_cfg_dae = 1;//todo
	// ini_cfg_frc_dae

	//w_reg_bit(VPU_DAE_WRAP_CTRL,1, 0,1);// reg_dae_start_sel   0:hw_auto  1:pls

	//==============================================================//
	// dae_wrap
	//==============================================================//

	bool dctgrd_en  = prm_dae->dctgrd_en;

	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, 0, 31, 1); //reg_tfbf_en

	//Use Aligned Size:
	//if(tfbf_en) cfg_tfbf(me_hsize, me_vsize);

	//YUV422;//todo, dpe_ds420==mix422 ????
	u32 di_in_mode     = src_fmt == YUV444 ? 0 : src_fmt == YUV422 ? 1 : 2;
	u32 di_bit_tight   = (src_bit == BIT_010);//0:10 in 16bit 1:10 in 10bit
	u32 di_bit         = (src_bit == BIT_010) | (src_bit == BIT_P010);

	din_pad_en = ((inp_hsize != (me_hsize * (1 << reg_mix_hds_en))) |
		(inp_vsize != (me_vsize * (1 << reg_mix_vds_en)))) & (dae_hpps_en == 0);

	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, game_mode, 0, 1);//reg_game_mode
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE1, game_mode, 4, 1);//reg_llm_en

	w_reg_bit(VPU_DAE_WRAP_DI, ((di_bit_tight & 0x1) << 4) |
					((di_in_mode & 0x3) << 0), 18, 5);
					//((di_ds_mode & 0x3) << 0), 16, 7);

	w_reg_bit(VPU_DAE_WRAP_DI, di_bit & 0x1, 4, 1);

	if ((prm_top->frame_count < 5 && dpss_frc_bypass) || !dpss_frc_bypass)
		w_reg_bit(VPU_DAE_WRAP_MIX_DS, (reg_mix_vds_en & 0x3) << 2 |
			(reg_mix_hds_en & 0x3), 0, 4);

	w_reg_bit(VPU_DAE_WRAP_DI, src_plane == PLANAR_X1, 24, 1);//reg_yuv444_one_cavs

	w_reg_bit(VPU_DAE_WRAP_DI, 1, 27, 1);//reg_me_bld_inp_mode
	w_reg_bit(VPU_DAE_WRAP_DI, 0xd, 28, 4);//reg_me_bld_y_gain
	w_reg_bit(VPU_DAE_WRAP_DI, 0x44, 8, 8);
	//w_reg_bit(VPU_DAE_WRAP_DI, 0x1, 22, 1);//2025-5-7 lukang check dae0 from zhangyi

	w_reg_bit(VPU_DAE_WRAP_PAD_CTRL, din_pad_en, 0, 1);

	if (din_pad_en || dctgrd_en)
		prm_dae->dae_fmt_up_repeat = 1;

	if (di_in_mode == YUV420)
		w_reg_bit(DAE_VD_CFMT_CTRL, 1, 28, 1); //fmt up repeat
	else
		w_reg_bit(DAE_VD_CFMT_CTRL, prm_dae->dae_fmt_up_repeat, 28, 1);
	//fmt up repeat

	// need reflash with set 0 in dae0 irq
	w_reg_bit(FRC_ME_TOP_CTRL2, 0, 16, 1);	//for p source read PD

	ini_cfg_frc_dae++;
	if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0) {
		dbg_f2("dae0 not ctrl_0 reg cnt:%d\n", ini_cfg_frc_dae);
		return; // test not ok
	} else if (ini_cfg_frc_dae > 0xfffffff0) {
		ini_cfg_frc_dae = 3;
	}
	//==============================================================//
	// frc_me/vp/melogo/iplogo/bbd enable
	//==============================================================//
	// bool mv_seed_update = 0;//for rnd_mv
	//if (dae_frc_loop != 0 || dae_sw_en)
	//	dcntr_en = dctgrd_en;
	if (game_mode)
		frc_vp_en = 0;
	if (ini_cfg_frc_dae > 0 && frc_me_en && !dpss_nr_bypass) {
		w_reg_bit(FRC_ME_EN, frc_me_en, 31, 1);//reg_me_en,rtl nouse
		// w_reg_bit(FRC_MELOGO_EN, 1, 7, 1); //frc_melogo_en must 1
		w_reg_bit(FRC_VP_TOP1, frc_vp_en, 0, 1); //reg_vp_en
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_iplogo_en, 30, 1); //reg_iplogo_en
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_bbd_en, 29, 1); //reg_bbd_en
		w_reg_bit(FRC_BBD_MOTION_SETTING, frc_bbd_en, 10, 1); //reg_bb_bbd_temporal_enable
		// w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 1, 1);
	}
	w_reg_bit(FRC_MELOGO_EN, 1, 7, 1); //frc_melogo_en must 1
	w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 1, 1);

#ifdef ME_GLB_CLR_BYPASS_MC
	if (src_is_frc) {
		w_reg_bit(FRC_ME_GCV_EN, 1, 0, 1);  //reg_me_glb_clr_force_en
		w_reg_bit(FRC_ME_GCV_EN, 1, 16, 1); //reg_me_glb_clr_top_en

		w_reg_bit(FRC_ME_GCV_STEP, 0, 24, 8);  //reg_me_glb_clr_st_ofrm_idx
		w_reg_bit(FRC_ME_GCV_STEP, 0, 8, 8);   //reg_me_bypass_mc_hold_time
		w_reg_bit(FRC_ME_GCV_STEP, 1, 20, 1);  //reg_me_glb_clr_dec_en
		w_reg_bit(FRC_ME_GCV_STEP, 0, 16, 1);  //reg_me_glb_clr_dec_stp
		w_reg_bit(FRC_ME_GCV_T_APL, 0, 0, 20); //reg_me_glb_clr_t_sad
	}
#endif
	//==============================================================//
	//
	//==============================================================//
	u32 MAX_ME_MVX  = 511;
	u32 ME_MVX_BIT  = 12;
	u32 MAX_ME_MVY  = 160;
	u32 ME_MVY_BIT  = 11;
	u32 me_max_mvx = (MAX_ME_MVX << 2 < (1 << (ME_MVX_BIT - 1)) - 1) ?
				(MAX_ME_MVX << 2) :
				((1 << (ME_MVX_BIT - 1)) - 1);
	u32 me_max_mvy = (MAX_ME_MVY << 2 < (1 << (ME_MVY_BIT - 1)) - 1) ?
				(MAX_ME_MVY << 2) :
				((1 << (ME_MVY_BIT - 1)) - 1);

	// w_reg_bit(FRC_ME_RPD_MAXSAD_CLIP, 2048, 16, 13); //reg_me_rpd_max_sad_clip
	w_reg_bit(VPU_NRME_TOP_CTRL3, 1, 12, 1); //reg_istop_invert todo

	if (ini_cfg_frc_dae && frc_me_en) {
		w_reg_bit(FRC_ME_EN, frc_me_en, 31, 1);//reg_me_en,rtl nouse
		if (frc_no_alg_ko == 1) {
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_0, frc_me_en, 0, 1);//reg_me_en,rtl nouse
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_1, frc_me_en, 0, 1);//reg_me_en,rtl nouse
			w_reg_bit(FRC_ME_CAD_ZGMV_EN_2, frc_me_en, 0, 1);//reg_me_en,rtl nouse
		}
		//w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvx_div_mode, 2, 2); //reg_me_mvx_div_mode
		//w_reg_bit(VPU_MCNR_ME_BLOCK_SIZE, mvy_div_mode, 0, 2); //reg_me_mvy_div_mode
		//w_reg_bit(FRC_ME_TOP_CTRL2, 0, 16, 1);  //for p source read PD
		//dbg_h2("%s:prm_dpss_dae0:%x\n", __func__, rd(FRC_ME_TOP_CTRL2));
		//if (ini_cfg_frc_dae > 4 && frc_melogo_en == 0) {
		//	dbg_f2("dae0 not ctrl_1 reg cnt:%d\n", ini_cfg_frc_dae);
		//	return;  // test not ok
		//}
		if (!game_mode) { // no game mode
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 31, 1); //reg_me_rpd_cn_en
				w_reg_bit(FRC_ME_RPD_EN, frc_me_en, 30, 1); //reg_me_rpd_cn_en
			}
			if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0 && dpss_nr_bypass) {
				dbg_f2("dae0 not ctrl reg_2 cnt:%d\n", ini_cfg_frc_dae);
				return;  // test ok
			}
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 31, 1); //reg_me_add_fs_cn_en
				w_reg_bit(FRC_ME_CAD_FS_EN, frc_me_en, 30, 1); //reg_me_add_fs_nc_en
				w_reg_bit(FRC_ME_CAD_PRJ_EN, frc_me_en, 18, 1); //reg_me_add_prj_en
			}
			w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 13, 1); //reg_me_loop_scan0_0
			w_reg_bit(FRC_ME_OBME_MODE_0, frc_me_en, 12, 1); //reg_me_loop_scan1_0
			w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 13, 1); //reg_me_loop_scan0_1
			w_reg_bit(FRC_ME_OBME_MODE_1, frc_me_en, 12, 1); //reg_me_loop_scan1_1
			w_reg_bit(FRC_ME_OBME_MODE_2, frc_me_en, 13, 1); //reg_me_loop_scan0_2
			w_reg_bit(FRC_ME_OBME_MODE_2, 1, 12, 1);         //reg_me_loop_scan1_2
			//if (ini_cfg_frc_dae > 2 && frc_melogo_en == 0) {
			//	dbg_f2("dae0 not ctrl reg_3 cnt:%d\n", ini_cfg_frc_dae);
			//	return;  // test ok
			//}
			if (frc_no_alg_ko == 1) {
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_0
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_1
				w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, frc_me_en, 28, 1);
				//reg_acdc_sel_mode_en_2
				w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 29, 1);
				w_reg_bit(FRC_ME_ZGMV_PATCH_EN, frc_me_en, 28, 1);
				w_reg_bit(FRC_ME_P1_EN_0, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_0
				w_reg_bit(FRC_ME_P1_EN_1, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_1
				w_reg_bit(FRC_ME_P1_EN_2, frc_me_en, 25, 1);
				//reg_me_periodic_aplsad_rule_en_2
				w_reg_bit(FRC_MEPP_SMOB_EN, frc_me_en, 31, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 0, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 1, 1);
				//w_reg_bit(FRC_ME_PERIODIC2_S1, frc_me_en, 2, 1);
			}
			w_reg_bit(FRC_ME_TOP_CTRL, 0, 29, 1);//reg_me_mode_ctrl
		} else {  // (game_mode)
			w_reg_bit(FRC_ME_RPD_EN, 0, 31, 1); //reg_me_rpd_cn_en
			w_reg_bit(FRC_ME_RPD_EN, 0, 30, 1); //reg_me_rpd_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 31, 1); //reg_me_add_fs_cn_en
			w_reg_bit(FRC_ME_CAD_FS_EN, 0, 30, 1); //reg_me_add_fs_nc_en
			w_reg_bit(FRC_ME_CAD_PRJ_EN, 0, 18, 1); //reg_me_add_prj_en
			w_reg_bit(FRC_ME_OBME_MODE_0, 0, 13, 1); //reg_me_loop_scan0_0
			w_reg_bit(FRC_ME_OBME_MODE_0, 1, 12, 1); //reg_me_loop_scan1_0
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 13, 1); //reg_me_loop_scan0_1
			w_reg_bit(FRC_ME_OBME_MODE_1, 0, 12, 1); //reg_me_loop_scan1_1
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 13, 1); //reg_me_loop_scan0_2
			w_reg_bit(FRC_ME_OBME_MODE_2, 0, 12, 1); //reg_me_loop_scan1_2
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_0, 0, 28, 1);
			//reg_acdc_sel_mode_en_0
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_1, 0, 28, 1);
			//reg_acdc_sel_mode_en_1
			w_reg_bit(FRC_ME_SAD_ACDC_REG0_2, 0, 28, 1);
			//reg_acdc_sel_mode_en_2
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 29, 1); //reg_me_zmv_patch_en
			w_reg_bit(FRC_ME_ZGMV_PATCH_EN, 0, 28, 1); //reg_me_gmv_patch_en
			w_reg_bit(FRC_ME_P1_EN_0, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_0
			w_reg_bit(FRC_ME_P1_EN_1, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_1
			w_reg_bit(FRC_ME_P1_EN_2, 0, 25, 1);
			//reg_me_periodic_aplsad_rule_en_2
			w_reg_bit(FRC_MEPP_SMOB_EN, 0, 31, 1); //reg_smob_en
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 0, 1);  //reg_me_periodic2_en_0
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 1, 1);  //reg_me_periodic2_en_1
			w_reg_bit(FRC_ME_PERIODIC2_S1, 0, 2, 1);  //reg_me_periodic2_en_2
			w_reg_bit(FRC_ME_TOP_CTRL, 1, 29, 1);//reg_me_mode_ctrl
		}

		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvx, 16, 12); //reg_me_cmv_max_mvx
		w_reg_bit(FRC_ME_CMV_MAX_MV, me_max_mvy, 0, 11); //reg_me_cmv_max_mvy
		// w_reg_bit(FRC_ME_CMV_CTRL, mv_seed_update, 31, 1);
		// reg_me_cmv_rand_pulse
		w_reg_bit(FRC_ME_GMV_CTRL, 2 + 2, 4, 3); //reg_gmv_mvx_sft
		w_reg_bit(FRC_ME_GMV_CTRL, 2 + 2, 0, 3); //reg_gmv_mvy_sft
		// reg_region_rp_gmv_mvx_sft / mvy_sft
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, 2 + 2, 17, 3);
		w_reg_bit(FRC_ME_REGION_RP_GMV_2, 2 + 2, 14, 3);
		w_reg_bit(FRC_ME_EN, 2, 4, 2); //reg_me_mvx_div_mode
		w_reg_bit(FRC_ME_EN, 2, 0, 2); //reg_me_mvy_div_mode
		// w_reg_bit(FRC_ME_EN, !frc_me_en, 30, 1); //reg_me_lpf_en

		//if (ini_cfg_frc_dae > 4 && frc_melogo_en == 0) {
		//	dbg_f2("dae0 not ctrl_4 reg cnt:%d\n", ini_cfg_frc_dae);
		//	return;  // test ok
		//}
		w_reg_bit(FRC_ME_BB_PIX_ED, me_hsize - 1, 16, 12);//reg_me_bb_xyxy_2
		w_reg_bit(FRC_ME_BB_PIX_ED, me_vsize - 1, 0, 12); //reg_me_bb_xyxy_3
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_hsize - 1, 16, 10); //reg_me_bb_blk_xyxy_2
		w_reg_bit(FRC_ME_BB_BLK_ED, me_blk_vsize - 1, 0, 10); //reg_me_bb_blk_xyxy_3
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 1 / 4, 16, 10);
		//reg_me_stat_region_hend_0
		w_reg_bit(FRC_ME_STAT_12R_H01, me_blk_hsize * 2 / 4, 0, 10);
		//reg_me_stat_region_hend_1
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 3 / 4, 16, 10);
		//reg_me_stat_region_hend_2
		w_reg_bit(FRC_ME_STAT_12R_H23, me_blk_hsize * 4 / 4, 0, 10);
		//reg_me_stat_region_hend_3
		w_reg_bit(FRC_ME_STAT_12R_V0, me_blk_vsize * 1 / 3, 0, 10);
		//reg_me_stat_region_vend_0
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 2 / 3, 16, 10);
		//reg_me_stat_region_vend_1
		w_reg_bit(FRC_ME_STAT_12R_V1, me_blk_vsize * 3 / 3, 0, 10);
		//reg_me_stat_region_vend_2
	}

	if (ini_cfg_frc_dae >= 2) {
		dbg_f2("dae0 frc return:%d\n", ini_cfg_frc_dae);
		return;
	}
	//just for reg_match,rtl not use
	//    w_reg_bit(FRC_REG_LOAD_ORG_FRAME_0,1,27,1);//reg_ip_film_end_0
	//    w_reg_bit(FRC_REG_OUT_FID,2,24,8);//reg_otb_cnt
	//    w_reg_bit(FRC_REG_PD_PAT_NUM,1,0,8);//reg_frc_pd_pat_num
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 0, 9, 1);//reg_dos_ds_input_mode
	w_reg_bit(VPU_MCNR_ME_BLD_UV_GAIN, 1, 8, 1);//reg_me_input_is444

	//ini_cfg_frc_dae = 1;
	//==============================================================//
	// VP
	//==============================================================//
	if (frc_vp_en && ini_cfg_frc_dae > 0) {
		w_reg_bit(FRC_VP_TOP1, frc_vp_en, 0, 1); //reg_vp_en

		w_reg_bit(FRC_VP_TOP1, 2, 12, 2); //reg_vp_mvx_div_mode
		w_reg_bit(FRC_VP_TOP1, 2, 8, 2); //reg_vp_mvy_div_mode
		w_reg_bit(FRC_VP_BB_2, me_blk_hsize - 1, 0, 16);
		w_reg_bit(FRC_VP_BB_2, me_blk_vsize - 1, 16, 16);
		w_reg_bit(FRC_VP_ME_BB_2, me_blk_hsize - 1, 0, 16);
		w_reg_bit(FRC_VP_ME_BB_2, me_blk_vsize - 1, 16, 16);
		w_reg_bit(FRC_VP_REGION_WINDOW_1, me_blk_hsize * 1 / 4,  8, 12);
		//reg_vp_stat_region_hend_0
		w_reg_bit(FRC_VP_REGION_WINDOW_1, me_blk_hsize * 2 / 4, 20, 12);
		//reg_vp_stat_region_hend_1
		w_reg_bit(FRC_VP_REGION_WINDOW_2, me_blk_hsize * 3 / 4,  0, 12);
		//reg_vp_stat_region_hend_2
		w_reg_bit(FRC_VP_REGION_WINDOW_2, me_blk_hsize * 4 / 4, 12, 12);
		//reg_vp_stat_region_hend_3
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 1 / 3,  0,  8);
		//reg_vp_stat_region_vend_0
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 2 / 3,  8, 12);
		//reg_vp_stat_region_vend_1
		w_reg_bit(FRC_VP_REGION_WINDOW_3, me_blk_vsize * 3 / 3, 20, 12);
		//reg_vp_stat_region_vend_2
	}

	//==============================================================//
	// melogo
	//==============================================================//
	if (frc_melogo_en && ini_cfg_frc_dae > 0) {
		wr(FRC_MELOGO_BB_BLK_ED, (me_blk_hsize - 1) << 16 |
			//reg_melogo_bb_blk_xyxy_2
					(me_blk_vsize - 1));
		//reg_melogo_bb_blk_xyxy_3
		wr(FRC_MELOGO_REGION_HWINDOW_2, (me_blk_hsize * 4 / 4) << 16 |
		//reg_melogo_stat_region_hend_3
						(me_blk_hsize * 3 / 4));
		//reg_melogo_stat_region_hend_2
		wr(FRC_MELOGO_REGION_HWINDOW_1, (me_blk_hsize * 2 / 4) << 16 |
		//reg_melogo_stat_region_hend_1
						(me_blk_hsize * 1 / 4));
		//reg_melogo_stat_region_hend_0
		wr(FRC_MELOGO_REGION_VWINDOW_1, (me_blk_vsize * 3 / 3) << 16 |
		//reg_melogo_stat_region_vend_2
						(me_blk_vsize * 2 / 3));
		//reg_melogo_stat_region_vend_1
		w_reg_bit(FRC_MELOGO_REGION_VWINDOW_0, me_blk_vsize * 1 / 3, 16, 10);
		//reg_melogo_stat_region_vend_0
	}

	//==============================================================//
	// iplogo
	//==============================================================//
	if (frc_iplogo_en && ini_cfg_frc_dae > 0) {
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_iplogo_en, 30, 1); //reg_iplogo_en

		wr(FRC_IPLOGO_BB_PIX_ED, (me_hsize - 1) << 16 |
					(me_vsize - 1)); //reg_iplogo_bb_xyxy_2/3
		wr(FRC_IPLOGO_REGION_HWINDOW_2, (me_hsize * 4 / 4) << 16 |
					(me_hsize * 3 / 4)); //reg_iplogo_stat_region_hend_3/2
		wr(FRC_IPLOGO_REGION_HWINDOW_1, (me_hsize * 2 / 4) << 16 |
					(me_hsize * 1 / 4)); //reg_iplogo_stat_region_hend_1/0
		wr(FRC_IPLOGO_REGION_VWINDOW_1, (me_vsize * 3 / 3) << 16 |
					(me_vsize * 2 / 3)); // reg_iplogo_stat_region_vend_3/2
		w_reg_bit(FRC_IPLOGO_REGION_VWINDOW_0, me_vsize * 1 / 3, 16, 10);
		//reg_iplogo_stat_region_vend_0
	}

	//==============================================================//
	// bbd
	//==============================================================//

	if (frc_bbd_en/* && bbd_init*/) {
		w_reg_bit(VPU_DAE_WRAP_CTRL, frc_bbd_en, 29, 1); //reg_bbd_en
		w_reg_bit(FRC_BBD_MOTION_SETTING, frc_bbd_en, 10, 1); //reg_bb_bbd_temporal_enable
		cfg_dpss_me_bbd(me_hsize, me_vsize);
		bbd_init = 0;
	}

//==============================================================//
// sub-com-mif:
// change burst_len of all rd/wr mif
//==============================================================//
	dpss_dae_rdmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
	dpss_dae_wrmif_cfg_brst_len(2); //(u32 burst_len): 2 : brst4, 1: brst2
}

//ME_LBUF
//w_reg_bit(FRC_ME_PRE_LBUF_OFST,  43, 16, 15);
//w_reg_bit(FRC_ME_PRE_LBUF_OFST, -48, 0 , 15);
//w_reg_bit(FRC_ME_CUR_LBUF_OFST,  43, 16, 15);
//w_reg_bit(FRC_ME_CUR_LBUF_OFST, -48, 0 , 15);
//w_reg_bit(FRC_ME_NEX_LBUF_OFST,  43, 16, 15);
//w_reg_bit(FRC_ME_NEX_LBUF_OFST, -48, 0 , 15);
//w_reg_bit(FRC_ME_LBUF_NUM, 92,  0, 8);
//w_reg_bit(FRC_ME_LBUF_NUM, 92,  8, 8);
//w_reg_bit(FRC_ME_LBUF_NUM, 92, 16, 8);

//only for bitmatch no need for ucode driver
//should be ignored for real case
//cfg_dpss_dae
void cfg_dae_me_is_top(u32 frm_cnt)
{
#ifdef SW_ME_IS_TOP
	bool dae_is_top;

	dae_is_top = frm_cnt & 0x1;
	w_reg_bit(VPU_NRME_TOP_CTRL1, 1 << 3 | dae_is_top << 2 | 1 << 1 |
		dae_is_top, 0, 4);
#endif
}

#ifdef FUNC_EN_DAE_HPPS
void dpss_dae_hpps(u32 ihsize, u32 ohsize)
{
	int i;
	int hsc_en = (ihsize == ohsize) ? 0 : 1;
	float h_size_div = ((float)ihsize) / ((float)ohsize);
	float step_h_fraction_f = h_size_div * (1 << 24);
	int step_h_fraction = (int)step_h_fraction_f;

	DBG_INF("%s:\n", __func__);
	wr(VPU_DAE_SCALE_EN, (ohsize << 16) | //;
		(11 << 8) | //; //reg_flt_sft
		(8 << 4) | //; //reg_hsc_tap_num
		(hsc_en));

int dae_pps_lut_tap8[33][8] = {
	{0,   0,   0, 512,   0,   0,   0,   0},
	{-1,    3,   -7,  512,    7,   -3,    1,    0},
	{-2,    5,  -14,  511,   15,   -5,    2,    0},
	{-2,    7,  -20,  510,   23,   -8,    3,   -1},
	{-3,   10,  -27,  509,   31,  -11,    3,    0},
	{-4,   12,  -32,  507,   39,  -14,    4,    0},
	{-4,   14,  -38,  504,   48,  -17,    5,    0},
	{-5,   16,  -43,  501,   57,  -19,    6,   -1},
	{-5,   18,  -49,  498,   66,  -22,    7,   -1},
	{-6,   19,  -53,  495,   75,  -26,    8,    0},
	{-6,   21,  -58,  490,   85,  -29,   10,   -1},
	{-6,   22,  -62,  486,   94,  -32,   11,   -1},
	{-7,   24,  -66,  481,  104,  -35,   12,   -1},
	{-7,   25,  -69,  476,  114,  -38,   13,   -2},
	{-7,   26,  -72,  470,  124,  -41,   14,   -2},
	{-8,   27,  -75,  464,  134,  -44,   15,   -1},
	{-8,   28,  -78,  458,  145,  -47,   16,   -2},
	{-8,   29,  -80,  451,  155,  -50,   17,   -2},
	{-8,   30,  -83,  444,  166,  -53,   18,   -2},
	{-8,   31,  -84,  437,  177,  -56,   19,   -4},
	{-8,   31,  -86,  429,  188,  -59,   20,   -3},
	{-9,   32,  -87,  421,  199,  -62,   22,   -4},
	{-8,   32,  -88,  413,  209,  -64,   23,   -5},
	{-8,   32,  -89,  405,  220,  -67,   24,   -5},
	{-9,   33,  -89,  396,  231,  -69,   25,   -6},
	{-8,   33,  -90,  387,  242,  -72,   25,   -5},
	{-8,   33,  -90,  377,  253,  -74,   26,   -5},
	{-8,   32,  -89,  368,  264,  -76,   27,   -6},
	{-8,   32,  -89,  358,  275,  -78,   28,   -6},
	{-8,   32,  -88,  348,  286,  -80,   29,   -7},
	{-7,   32,  -88,  338,  297,  -82,   29,   -7},
	{-7,   31,  -86,  328,  307,  -84,   30,   -7},
	{-8,   31,  -85,  318,  318,  -85,   31,   -8}
	};

	wr(VPU_DAE_SCALE_STEP, step_h_fraction);
	w_reg_bit(DPSS_DAE_SCALE_COEF_IDX, 0x800, 0, 13);

	for (i = 0; i < 33; i++) {
		wr(DPSS_DAE_SCALE_COEF, (((dae_pps_lut_tap8[i][0] >> 8) &
				0xff) << 24) |
			((dae_pps_lut_tap8[i][0] & 0xff) << 16) |
			(((dae_pps_lut_tap8[i][1] >> 8) & 0xff) << 8) |
			((dae_pps_lut_tap8[i][1] & 0xff) << 0));
		wr(DPSS_DAE_SCALE_COEF, (((dae_pps_lut_tap8[i][2] >> 8) &
			0xff) << 24) |
			((dae_pps_lut_tap8[i][2] & 0xff) << 16) |
			(((dae_pps_lut_tap8[i][3] >> 8) & 0xff) << 8) |
			((dae_pps_lut_tap8[i][3] & 0xff) << 0));
	}
	w_reg_bit(DPSS_DAE_SCALE_COEF_IDX, 0xC00, 0, 13);
	for (i = 0; i < 33; i++) {
		wr(DPSS_DAE_SCALE_COEF, (((dae_pps_lut_tap8[i][4] >> 8) &
			0xff) << 24) |
		((dae_pps_lut_tap8[i][4] & 0xff) << 16) |
		(((dae_pps_lut_tap8[i][5] >> 8) & 0xff) << 8) |
		((dae_pps_lut_tap8[i][5] & 0xff) << 0));

		wr(DPSS_DAE_SCALE_COEF, (((dae_pps_lut_tap8[i][6] >> 8) &
			0xff) << 24) |
		((dae_pps_lut_tap8[i][6] & 0xff) << 16) |
		(((dae_pps_lut_tap8[i][7] >> 8) & 0xff) << 8) |
		((dae_pps_lut_tap8[i][7] & 0xff) << 0));
	}
}
#else //FUNC_EN_DAE_HPPS
void dpss_dae_hpps(u32 ihsize, u32 ohsize)
{
}

#endif //FUNC_EN_DAE_HPPS

void cfg_dpss_dae_me_rand_seed(u32 frm_cnt_inp, u32 frm_cnt_src0,
u32 frm_cnt_src1, u32 frm_cnt_src2)
{
	if (dpss_slt_mode)
		return;

	static u32 ro_rand_seed_mvx[4][3];//4 for mult_di?
	static u32 ro_rand_seed_mvy[4][3];
	static u32 ro_rand_seed_mvq[4][3];

	u32 default_seed_mvx = 0xabcd;
	u32 default_seed_mvy = 0x9988;
	u32 default_seed_mvq = 0x3838;

	int i, j;

	if (frm_cnt_inp == 0) { //ary default setting
		dbg_h2("set default seed for all mvx mvy mvq\n");
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 3; j++) {
				ro_rand_seed_mvx[i][j] = default_seed_mvx;
				ro_rand_seed_mvy[i][j] = default_seed_mvy;
				ro_rand_seed_mvq[i][j] = default_seed_mvq;
			}
		}
		return;
	}
	if (frm_cnt_src0 == 0) {
		dbg_h2("set default seed for frc mvx mvy mvq\n");
		for (j = 0; j < 3; j++) {
			ro_rand_seed_mvx[0][j] = default_seed_mvx;
			ro_rand_seed_mvy[0][j] = default_seed_mvy;
			ro_rand_seed_mvq[0][j] = default_seed_mvq;
		}
		//return;
	}
	if (frm_cnt_src1 == 0) {
		dbg_h2("set default seed for nr mvx mvy mvq\n");
		for (j = 0; j < 3; j++) {
			ro_rand_seed_mvx[1][j] = default_seed_mvx;
			ro_rand_seed_mvy[1][j] = default_seed_mvy;
			ro_rand_seed_mvq[1][j] = default_seed_mvq;
		}

		//return;
	}
	if (frm_cnt_src2 == 0) {
		dbg_h2("set default seed for di mvx mvy mvq\n");
		for (j = 0; j < 3; j++) {
			ro_rand_seed_mvx[2][j] = default_seed_mvx;
			ro_rand_seed_mvy[2][j] = default_seed_mvy;
			ro_rand_seed_mvq[2][j] = default_seed_mvq;
		}
		//return;
	}

	u32 cur_dae_src;
	u32 pre_dae_src;
	u32 data32;

	data32 = rd(DPSS_SRC_INDEX);
	pre_dae_src = (data32 >> 24) & 0xf;
	cur_dae_src = (data32 >> 16) & 0xf;

	dbg_h2("pre_src=%d  cur_src=%4d\n", pre_dae_src, cur_dae_src);

	if (pre_dae_src == 1 && frm_cnt_src0 > 0) {//pre_frm is frc
		ro_rand_seed_mvx[0][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[0][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[0][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[0][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[0][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[0][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[0][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[0][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[0][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store frc rand_seed_mv[%0d]: %0x", 0, ro_rand_seed_mvx[0][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[0][0], ro_rand_seed_mvq[0][0]);
		dbg_h2("%0x %0x", ro_rand_seed_mvx[0][1], ro_rand_seed_mvy[0][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[0][1], ro_rand_seed_mvx[0][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[0][2], ro_rand_seed_mvq[0][2]);
	} else if (pre_dae_src == 7 && frm_cnt_src1 > 0) {//pre_frm is nr
		ro_rand_seed_mvx[1][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[1][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[1][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[1][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[1][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[1][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[1][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store nr rand_seed_mv[%0d]: %0x ", 1, ro_rand_seed_mvx[1][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[1][0], ro_rand_seed_mvq[1][0]);
		dbg_h2("%0x %0x ", ro_rand_seed_mvx[1][1], ro_rand_seed_mvy[1][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[1][1], ro_rand_seed_mvx[1][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[1][2], ro_rand_seed_mvq[1][2]);
	} else if (pre_dae_src == 9 && frm_cnt_src1 > 2) {//pre_frm is nrdi src0
		ro_rand_seed_mvx[1][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[1][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[1][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[1][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[1][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[1][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[1][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("nr di src0 rand_seed_mv[%0d]: %0x ", 1, ro_rand_seed_mvx[1][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[1][0], ro_rand_seed_mvq[1][0]);
		dbg_h2("%0x %0x ", ro_rand_seed_mvx[1][1], ro_rand_seed_mvy[1][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[1][1], ro_rand_seed_mvx[1][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[1][2], ro_rand_seed_mvq[1][2]);
	} else if (pre_dae_src == 8 && frm_cnt_src2 > 2) {
		//pre_frm is di //todo frm_cnt_src2 > 2?
		ro_rand_seed_mvx[2][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[2][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[2][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[2][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[2][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[2][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[2][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[2][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[2][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store nrdi src1 rand_seed_mv[%0d]: %0x ", 2, ro_rand_seed_mvx[2][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[2][0], ro_rand_seed_mvq[2][0]);
		dbg_h2("%0x %0x ", ro_rand_seed_mvx[2][1], ro_rand_seed_mvy[2][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[2][1], ro_rand_seed_mvx[2][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[2][2], ro_rand_seed_mvq[2][2]);
	}

	int cur_dae_idx = cur_dae_src == 1 ? 0 : (cur_dae_src == 7 || cur_dae_src == 9) ? 1 : 2;

	wr(FRC_ME_SEED0_0, (ro_rand_seed_mvx[cur_dae_idx][0] & 0xffff));
	wr(FRC_ME_SEED1_0, ((ro_rand_seed_mvy[cur_dae_idx][0] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][0] & 0xffff));
	wr(FRC_ME_SEED0_1, (ro_rand_seed_mvx[cur_dae_idx][1] & 0xffff));
	wr(FRC_ME_SEED1_1, ((ro_rand_seed_mvy[cur_dae_idx][1] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][1] & 0xffff));
	wr(FRC_ME_SEED0_2, (ro_rand_seed_mvx[cur_dae_idx][2] & 0xffff));
	wr(FRC_ME_SEED1_2, ((ro_rand_seed_mvy[cur_dae_idx][2] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][2] & 0xffff));
	dbg_h2("sw rand_seed_mv[%0d]: %0x ", cur_dae_idx, ro_rand_seed_mvx[cur_dae_idx][0]);
	dbg_h2("%0x %0x|", ro_rand_seed_mvy[cur_dae_idx][0], ro_rand_seed_mvq[cur_dae_idx][0]);
	dbg_h2("%0x %0x ", ro_rand_seed_mvx[cur_dae_idx][1], ro_rand_seed_mvy[cur_dae_idx][1]);
	dbg_h2("%0x|%0x", ro_rand_seed_mvq[cur_dae_idx][1], ro_rand_seed_mvx[cur_dae_idx][2]);
	dbg_h2("%0x%0x\n", ro_rand_seed_mvy[cur_dae_idx][2], ro_rand_seed_mvq[cur_dae_idx][2]);
	w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);//reg_me_cmv_rand_pulse
}

//pre_dae_src pre write done resault idx
//cur_dae_src cur will deal  resault idx
//dae done trigger
void cfg_dpss_dae_di_sw_seed(u32 frm_di_cnt, u32 pre_dae_src, u32 cur_dae_src)
{
	static u32 ro_rand_seed_mvx[4][3];//4 for mult_di?
	static u32 ro_rand_seed_mvy[4][3];
	static u32 ro_rand_seed_mvq[4][3];

	u32 default_seed_mvx = 0xabcd;
	u32 default_seed_mvy = 0x9988;
	u32 default_seed_mvq = 0x3838;

	int i, j;

	if (frm_di_cnt == 0) {
		dbg_h2("set default seed for mvx mvy mvq\n");
		for (i = 0; i < 4; i++) {
			for (j = 0; j < 3; j++) {
				ro_rand_seed_mvx[i][j] = default_seed_mvx;
				ro_rand_seed_mvy[i][j] = default_seed_mvy;
				ro_rand_seed_mvq[i][j] = default_seed_mvq;
			}
		}
		return;
	}

	dbg_h2("pre_src=%d  cur_src=%4d\n", pre_dae_src, cur_dae_src);
	if (pre_dae_src == 8 && frm_di_cnt > 1) {
		//pre_frm is di done //todo frm_cnt_src2 > 2? //need_check
		ro_rand_seed_mvx[0][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[0][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[0][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[0][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[0][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[0][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[0][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[0][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[0][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store rand_seed_mv[%0d]: %0x ", 0, ro_rand_seed_mvx[0][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[0][0], ro_rand_seed_mvq[0][0]);
		dbg_h2("%0x %0x ", ro_rand_seed_mvx[0][1], ro_rand_seed_mvy[0][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[0][1], ro_rand_seed_mvx[0][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[0][2], ro_rand_seed_mvq[0][2]);
	} else if (pre_dae_src == 9 && frm_di_cnt > 1) {
	//pre_frm is di //todo frm_cnt_src2 > 2? //need_check
		ro_rand_seed_mvx[1][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[1][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[1][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[1][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[1][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[1][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[1][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[1][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store rand_seed_mv[%0d]: %0x ", 1, ro_rand_seed_mvx[1][0]);
		dbg_h2("%0x %0x |", ro_rand_seed_mvy[1][0], ro_rand_seed_mvq[1][0]);
		dbg_h2("%0x %0x ", ro_rand_seed_mvx[1][1], ro_rand_seed_mvy[1][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[1][1], ro_rand_seed_mvx[1][2]);
		dbg_h2(" %0x %0x\n", ro_rand_seed_mvy[1][2], ro_rand_seed_mvq[1][2]);
	} else if (pre_dae_src == 10 && frm_di_cnt > 1) {
	//pre_frm is di //todo frm_cnt_src2 > 2? //need_check
		ro_rand_seed_mvx[2][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[2][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[2][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[2][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[2][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[2][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[2][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[2][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[2][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store rand_seed_mv[%0d]: %0x", 2, ro_rand_seed_mvx[2][0]);
		dbg_h2("%0x %0x|", ro_rand_seed_mvy[2][0], ro_rand_seed_mvq[2][0]);
		dbg_h2("%0x %0x", ro_rand_seed_mvx[2][1], ro_rand_seed_mvy[2][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[2][1], ro_rand_seed_mvx[2][2]);
		dbg_h2("%0x %0x\n", ro_rand_seed_mvy[2][2], ro_rand_seed_mvq[2][2]);
	} else if (pre_dae_src == 11 && frm_di_cnt > 1) {
		//pre_frm is di //todo frm_cnt_src2 > 2? //need_check
		ro_rand_seed_mvx[3][0] = (rd(FRC_ME_RO_SEED0_0)) & 0xffff;
		ro_rand_seed_mvx[3][1] = (rd(FRC_ME_RO_SEED0_1)) & 0xffff;
		ro_rand_seed_mvx[3][2] = (rd(FRC_ME_RO_SEED0_2)) & 0xffff;
		ro_rand_seed_mvy[3][0] = (rd(FRC_ME_RO_SEED1_0) >> 16) & 0xffff;
		ro_rand_seed_mvy[3][1] = (rd(FRC_ME_RO_SEED1_1) >> 16) & 0xffff;
		ro_rand_seed_mvy[3][2] = (rd(FRC_ME_RO_SEED1_2) >> 16) & 0xffff;
		ro_rand_seed_mvq[3][0] = (rd(FRC_ME_RO_SEED1_0)) & 0xffff;
		ro_rand_seed_mvq[3][1] = (rd(FRC_ME_RO_SEED1_1)) & 0xffff;
		ro_rand_seed_mvq[3][2] = (rd(FRC_ME_RO_SEED1_2)) & 0xffff;
		dbg_h2("store rand_seed_mv[%0d]: %0x", 3, ro_rand_seed_mvx[2][0]);
		dbg_h2("%0x %0x|", ro_rand_seed_mvy[3][0], ro_rand_seed_mvq[3][0]);
		dbg_h2("%0x %0x", ro_rand_seed_mvx[3][1], ro_rand_seed_mvy[3][1]);
		dbg_h2("%0x | %0x", ro_rand_seed_mvq[3][1], ro_rand_seed_mvx[3][2]);
		dbg_h2("%0x %0x\n", ro_rand_seed_mvy[3][2], ro_rand_seed_mvq[3][2]);
	}

	int cur_dae_idx = cur_dae_src == 8  ? 0 :
			cur_dae_src == 9  ? 1 :
			cur_dae_src == 10 ? 2 : 3;

	wr(FRC_ME_SEED0_0,  (ro_rand_seed_mvx[cur_dae_idx][0] & 0xffff));
	wr(FRC_ME_SEED1_0, ((ro_rand_seed_mvy[cur_dae_idx][0] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][0] & 0xffff));
	wr(FRC_ME_SEED0_1,  (ro_rand_seed_mvx[cur_dae_idx][1] & 0xffff));
	wr(FRC_ME_SEED1_1, ((ro_rand_seed_mvy[cur_dae_idx][1] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][1] & 0xffff));
	wr(FRC_ME_SEED0_2,  (ro_rand_seed_mvx[cur_dae_idx][2] & 0xffff));
	wr(FRC_ME_SEED1_2, ((ro_rand_seed_mvy[cur_dae_idx][2] & 0xffff) << 16) |
		(ro_rand_seed_mvq[cur_dae_idx][2] & 0xffff));
	dbg_h2("switch rand_seed_mv[%0d]: %0x ", cur_dae_idx,
			ro_rand_seed_mvx[cur_dae_idx][0]);
	dbg_h2("%0x %0x|", ro_rand_seed_mvy[cur_dae_idx][0],
		ro_rand_seed_mvq[cur_dae_idx][0]);
	dbg_h2("%0x %0x", ro_rand_seed_mvx[cur_dae_idx][1],
		ro_rand_seed_mvy[cur_dae_idx][1]);
	dbg_h2("%0x | %0x", ro_rand_seed_mvq[cur_dae_idx][1],
		ro_rand_seed_mvx[cur_dae_idx][2]);
	dbg_h2("%0x %0x\n", ro_rand_seed_mvy[cur_dae_idx][2],
		ro_rand_seed_mvq[cur_dae_idx][2]);
	w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);//reg_me_cmv_rand_pulse
}

void cfg_dpss_me_bbd(u32 me_hsize, u32 me_vsize)
{
//u32 me_hsize = (prm_top.frm_hsize+(1<<prm_top.dae_dsx_scale)-1) >>
	//prm_top.dae_dsx_scale;
//u32 me_vsize = (prm_top.frm_vsize+(1<<prm_top.dae_dsy_scale)-1) >>
	//prm_top.dae_dsy_scale;
	u32 me_hsize_m1 = me_hsize - 1;
	u32 me_vsize_m1 = me_vsize - 1;

//  u32 rd_reg_data0 = rd(FRC_BBD_MOTION_NUM);
	u32 rd_reg_data1 = 0x1000100;//rd(FRC_BBD_DETECT_MOTION_LINE_NUM);
	u32 rd_reg_data2 = 0x90011;//rd(FRC_BBD_ROW_MOTION_TH);
	u32 rd_reg_data3 = 0x90011;//rd(FRC_BBD_COL_MOTION_TH);

	u32 reg_bb_finer_col_num = 64; //FRC_BBD_MOTION_NUM[6:0] default value
	// u32 reg_bb_finer_col_num = rd_reg_data0 & 0x7f;
	u32 reg_bb_det_line_num_y = ((rd_reg_data1 >> 16) & 0xffff) * me_vsize / 1080;
	u32 reg_bb_det_line_num_x = (rd_reg_data1 & 0xffff) * me_hsize / 1920;
	u32 reg_bb_row_motion_th1 = ((rd_reg_data2 >> 16) & 0xffff) * me_hsize / 1920;
	u32 reg_bb_row_motion_th2 = (rd_reg_data2 & 0xffff) * me_hsize / 1920;
	u32 reg_bb_col_motion_th1 = ((rd_reg_data3 >> 16) & 0xffff) * me_vsize / 1080;
	u32 reg_bb_col_motion_th2 = (rd_reg_data3 & 0xffff) * me_vsize / 1080;
	u32 reg_bb_adv_motion_pos, reg_bb_fin_col_num_p2;

	if ((reg_bb_det_line_num_x / reg_bb_finer_col_num) < 4) {
		reg_bb_finer_col_num  = reg_bb_det_line_num_x >> 2;
		reg_bb_det_line_num_x = reg_bb_finer_col_num  << 2;
	} else {
		reg_bb_fin_col_num_p2 = reg_bb_finer_col_num << 1;
		reg_bb_det_line_num_x = reg_bb_det_line_num_x /
			reg_bb_fin_col_num_p2 * reg_bb_fin_col_num_p2;
	}
	reg_bb_adv_motion_pos = me_hsize - reg_bb_finer_col_num;

	w_reg_bit(FRC_BBD_DETECT_MOTION_REGION_LFT2RIT, me_hsize_m1, 0, 16);
	//reg_bb_det_motion_rit
	w_reg_bit(FRC_BBD_DETECT_MOTION_REGION_TOP2BOT, me_vsize_m1, 0, 16);
	dbg_h2("%s bbd size(%d, %d)\n",
		__func__, me_hsize_m1, me_vsize_m1);
	//reg_bb_det_motion_bot
	w_reg_bit(FRC_BBD_MOTION_NUM, reg_bb_finer_col_num, 0, 7);
	//reg_bb_motion_finer_col_num
	wr(FRC_BBD_MOTION_DETEC_REGION_RIT_BOT_DS, me_hsize_m1 << 16 | me_vsize_m1);
	//reg_bb_motion_xyxy_ds_2
	//reg_bb_motion_xyxy_ds_3
	wr(FRC_BBD_ADV_ROUGH_RIT_MOTION_POSI, reg_bb_adv_motion_pos << 16 |
		//reg_bb_adv_rough_rit_motion_posi1
						reg_bb_adv_motion_pos);
		//reg_bb_adv_rough_rit_motion_posi2
	wr(FRC_BBD_DETECT_MOTION_LINE_NUM, reg_bb_det_line_num_y << 16 |
	//reg_bb_top_bot_det_motion_line_num
						reg_bb_det_line_num_x);
	//reg_bb_lft_rit_det_motion_line_num

	wr(FRC_BBD_ROW_MOTION_TH, reg_bb_row_motion_th1 << 16 | //reg_bb_row_motion_th1
		reg_bb_row_motion_th2); //reg_bb_row_motion_th2

	wr(FRC_BBD_COL_MOTION_TH, reg_bb_col_motion_th1 << 16 | //reg_bb_col_motion_th1
	reg_bb_col_motion_th2); //reg_bb_col_motion_th2
}

void cfg_tfbf(u32 hsize, u32 vsize)
{
	u32 h_step, v_step, h_ofst, v_ofst;
	u32 dsbuf_ocol, dsbuf_orow;
	u32 dsbuf_rowmax = 96;
	u32 dsbuf_colmax = 128;

	h_step = (hsize + dsbuf_colmax - 1) / dsbuf_colmax;
	v_step = (vsize + dsbuf_rowmax - 1) / dsbuf_rowmax;

	h_ofst = (h_step >> 1);
	v_ofst = (v_step >> 1);

	dsbuf_ocol =  hsize / h_step - 1;
	dsbuf_orow =  vsize / v_step - 1;

	if (dsbuf_ocol > dsbuf_colmax)
		dsbuf_ocol =  dsbuf_colmax;
	if (dsbuf_orow > dsbuf_rowmax)
		dsbuf_orow =  dsbuf_rowmax;

	//w_reg_bit(VPU_DAE_WRAP_MIX_DS, 0, 6, 1); //reg_tfbf_pre_ds
	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, 1, 31, 1); //reg_tfbf_en
	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, dsbuf_ocol, 0, 13); //reg_tfbf_hsize
	w_reg_bit(VPU_DAE_WRAP_TFTB_EN, dsbuf_orow, 16, 13); //reg_tfbf_vsize
	w_reg_bit(VPU_MCNR_H_V_STEP, h_step, 16, 6); //reg_h_step
	w_reg_bit(VPU_MCNR_H_V_STEP, v_step, 0, 6); //reg_v_step
	w_reg_bit(VPU_MCNR_H_V_OFST, h_ofst, 16, 10); //reg_h_ofst
	w_reg_bit(VPU_MCNR_H_V_OFST, v_ofst, 0, 10); //reg_v_ofst
	w_reg_bit(VPU_MCNR_DSBUF_O, dsbuf_ocol, 8, 8); //dsbuf_ocol
	w_reg_bit(VPU_MCNR_DSBUF_O, dsbuf_orow, 0, 8); //dsbuf_orow
	w_reg_bit(VPU_MCNR_HAA_VAA, 0, 3, 1); //reg_is_top
}

void cfg_dae_triggle(void)
{
	w_reg_bit(VPU_DAE_WRAP_CTRL, 1, 20, 1);// pls_dae_frm_start
}

void cfg_dae_game_mode_sw(s32 *base_addr, s32 *base_oft,
s32 buf_depth, s32 in_addr_en, struct PRM_DPSS_TOP *prm_top, s32 c_match)
{
	static u32 dae_send_num;
	u32 dae_frm_num = dae_send_num / 2;
	u32 dae_out_buf = 4; //todo regs
	u32 dae_src_buf = 16; //todo regs
	u32 dae_ds_num = 1; //1/2 todo add sel
	struct PRM_DPSS_TOP dpss_top = *prm_top;
//ary no use enum DPSS_WORK_MODE dae_mode = dpss_top.dpss_mode;
	u32 me_dsx = dpss_top.dae_dsx_scale;
	u32 me_dsy = dpss_top.dae_dsy_scale;
	u32 dae_inp_hsize = dpss_top.frm_hsize & 0xffff;
	u32 dae_inp_vsize = dpss_top.frm_vsize & 0xffff;
	u32 dae_me_hsize = (dae_inp_hsize + (1 << me_dsx) - 1) >> me_dsx;
	u32 dae_me_vsize = (dae_inp_vsize + (1 << me_dsy) - 1) >> me_dsy;
	u32 dae_blk_hsize = (dae_me_hsize + 3) >> 2;
	u32 dae_blk_vsize = (dae_me_vsize + 3) >> 2;

	u32 dae_frc_idx_num = dae_frm_num % buf_depth;
	u32 dae_src_idx_num = dae_frm_num % dae_src_buf;
	u32 dae_frc_idx_num_m2 = dae_frm_num % 2;
	//ary no use u32  dae_frc_out_num = dae_frm_num % dae_out_buf;
	u32 dae_frc_out_pre_num = (dae_frm_num - 1) % dae_out_buf;
	u32 dae_frc_pre1_num_m2 = (dae_frm_num - 1) % 2;
	u32 dae_frc_pre1_num = (dae_frm_num - 1) % buf_depth;
	//ary no use u32  dae_frc_pre2_num = (dae_frm_num - 2) % buf_depth;
	u32 addr_outside = in_addr_en & 0x1;
	u32 nxt_luma_base_baddr = (addr_outside) ? base_addr[0] : 0x4000000;
	u32 nxt_chrm_base_baddr = (addr_outside) ? base_addr[1] : 0x4100000;
	u32 mix_base_baddr = (addr_outside) ? base_addr[2] : 0xa220000;
	u32 logo_flt_base_baddr = (addr_outside) ? base_addr[3] : 0x4358000;
	u32 logo_scc_base_baddr = (addr_outside) ? base_addr[4] : 0x4368000;
	u32 pcphs_base_baddr = (addr_outside) ? base_addr[5] : 0x4350000;
	u32 ip_logo_base_baddr = (addr_outside) ? base_addr[6] : 0x43b8000;
	u32 mc_logo_base_baddr = (addr_outside) ? base_addr[7] : 0x4438000;
	u32 mcmv_base_baddr = (addr_outside) ? base_addr[8] : 0x43f8000;
	u32 luma_ofst = (addr_outside) ? base_oft[0] : 0x20000;
	u32 chrm_ofst = (addr_outside) ? base_oft[1] : 0x10000;
	u32 mix_ofst = (addr_outside) ? base_oft[2] : 0x8000;
	u32 logo_flt_ofst = (addr_outside) ? base_oft[3] : 0x8000;
	u32 logo_scc_ofst = (addr_outside) ? base_oft[4] : 0x8000;
	//ary no use u32  pc_phs_ofst = (addr_outside)? base_oft[5] : 0x4000;
	u32 ip_logo_ofst = (addr_outside) ? base_oft[6] : 0x8000;
	u32 mc_logo_ofst = (addr_outside) ? base_oft[7] : 0x8000;
	u32 mcmv_ofst = (addr_outside) ? base_oft[8] : 0x8000;
	u32  nxt_luma_wbaddr;
	u32  nxt_chrm_wbaddr;

	nxt_luma_wbaddr = nxt_luma_base_baddr + dae_src_idx_num * luma_ofst;
	nxt_chrm_wbaddr = nxt_chrm_base_baddr + dae_src_idx_num * chrm_ofst;

	u32 mix_out_wbaddr = mix_base_baddr + dae_frc_idx_num * mix_ofst;
	u32 logo_flt_wbaddr = logo_flt_base_baddr +
		dae_frc_idx_num_m2 * logo_flt_ofst;
	u32 logo_scc_wbaddr = logo_scc_base_baddr +
		dae_frc_idx_num_m2 * logo_scc_ofst;
	//u32  pcphs_out_wbaddr= pcphs_base_baddr +
	//dae_frc_idx_num_m2 * pc_phs_ofst;
	u32 pcphs_out_wbaddr = pcphs_base_baddr;
	u32 ip_logo_wbaddr = ip_logo_base_baddr +
		dae_frc_out_pre_num * ip_logo_ofst;
	u32 mc_logo_wbaddr = mc_logo_base_baddr +
		dae_frc_out_pre_num * mc_logo_ofst;
	u32 mc_mv_wbaddr = mcmv_base_baddr +
		dae_frc_out_pre_num * mcmv_ofst;
	u32  mix_pre1_rbaddr = mix_base_baddr +
		dae_frc_pre1_num * mix_ofst;
	//u32 mix_pre2_rbaddr = mix_base_baddr + dae_frc_pre1_num * mix_ofst;
	u32 logo_flt_pre_rbaddr = logo_flt_base_baddr +
		dae_frc_pre1_num_m2 * logo_flt_ofst;
	u32 logo_scc_pre_rbaddr = logo_scc_base_baddr +
		dae_frc_pre1_num_m2 * logo_scc_ofst;
	//u32 pcphs_pre_rbaddr = pcphs_base_baddr + dae_frc_pre1_num_m2 * pc_phs_ofst;
	u32 pcphs_pre_rbaddr = pcphs_base_baddr;
	u32 mc_logo_pre_rbaddr = mc_logo_base_baddr +
		dae_frc_out_pre_num * mc_logo_ofst;
	u32 frm_swth = (dae_send_num % 2) == 0;
	u32 ip_logo_init_frm = (dae_send_num == (2 * 2 - 1));//ip logo deal fst frm

	DBG_INF("%s:c_match = %d.\n", __func__, c_match);
	if (dae_send_num == 0) {
		wr(VPU_DAE_WRAP_SW_SEL0, 0xffffffff);
		wr(VPU_DAE_WRAP_SW_SEL1, 0xffffffff);
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 0x3f, 2, 6);//irq sel dae done
		w_reg_bit(VPU_DAE_WRAP_SW_INFO, dae_ds_num << 2 | dae_ds_num, 0, 4);
		//reg_sw_mvy_div_mode,reg_sw_mvx_div_mode
	}

	w_reg_bit(VPU_DAE_WRAP_CTRL, (dae_send_num == 0), 31, 1);//reg_dae_byps
	w_reg_bit(VPU_DAE_WRAP_SW_EN, 1 << 9 |
					0 << 9 |
					1 << 4 |
			(dae_send_num > 1), 12, 10);
	//reg_dae_frc_en,reg_dae_src_mode,reg_dae_src_idx,reg_dae_frm_phs
	w_reg_bit(VPU_DAE_WRAP_SW_EN, 1, 0, 1);//pls_syn_frm_rst_me
	w_reg_bit(VPU_DAE_WRAP_SW_EN, 0x40 << 3 |
		frm_swth << 1 | ip_logo_init_frm, 1, 10);
	//reg_frm_phase,reg_frm_swth,reg_ip_logo_init_frm
	if (frm_swth == 1) {
		wr(VPU_DAE_WRAP_SW_SIZE0, (dae_inp_vsize / 2) << 16 |
					(dae_inp_hsize / 2));
		wr(VPU_DAE_WRAP_SW_SIZE1, dae_me_vsize << 16 | dae_me_hsize);
		wr(VPU_DAE_WRAP_SW_SIZE2, dae_blk_vsize << 16 | dae_blk_hsize);

		wr(VPU_DAE_WRAP_SW_ADDR2, pcphs_out_wbaddr);
		//reg_me_pc_phs_mv_wr_addr
		wr(VPU_DAE_WRAP_SW_ADDR5, pcphs_pre_rbaddr);
		//reg_me_pc_phs_mv_rd_addr
		//        wr(VPU_DAE_WRAP_SW_ADDR9, mix_pre1_rbaddr);
		//reg_me_cur_rdmif_baddr
		wr(VPU_DAE_WRAP_SW_ADDR10, mix_pre1_rbaddr);
		//reg_me_pre_rdmif_baddr
		wr(VPU_DAE_WRAP_SW_ADDR11, nxt_luma_wbaddr);
		//reg_me_nxt_luma_baddr
		wr(VPU_DAE_WRAP_SW_ADDR12, nxt_chrm_wbaddr);
		//reg_me_nxt_chrm_baddr
		wr(VPU_DAE_WRAP_SW_ADDR13, mix_out_wbaddr);
		//reg_me_mix_wrmif_baddr
		wr(VPU_DAE_WRAP_SW_ADDR14, mc_mv_wbaddr);
		//reg_mevp_mc_mv_wr_baddr
		wr(VPU_DAE_WRAP_SW_ADDR15, mc_logo_wbaddr);
		//reg_mevp_mc_logo_wr_baddr
		wr(VPU_DAE_WRAP_SW_ADDR16, mc_logo_pre_rbaddr);
		//reg_mevp_mc_logo_rd_baddr
		wr(VPU_DAE_WRAP_SW_ADDR26, logo_flt_pre_rbaddr);
		//reg_logo_pre_iir_baddr
		wr(VPU_DAE_WRAP_SW_ADDR27, logo_flt_wbaddr);
		//reg_logo_cur_iir_baddr
		wr(VPU_DAE_WRAP_SW_ADDR28, logo_scc_pre_rbaddr);
		//reg_logo_pre_scc_baddr
		wr(VPU_DAE_WRAP_SW_ADDR29, logo_scc_wbaddr);
		//reg_logo_cur_scc_baddr
		wr(VPU_DAE_WRAP_SW_ADDR30, ip_logo_wbaddr);
		//reg_iplogo_wrmif_baddr
	}
	cfg_dae_triggle();
	dae_send_num++;
}

void cfg_dae_nr_sw(s32 *base_addr, s32 *base_oft,
	s32 buf_depth, s32 in_addr_en,
	struct PRM_DPSS_TOP *prm_top, s32 c_match)
{
	static u32 dae_send_num;
	// u32 dae_frm_num = dae_send_num / 2;
	//ary no use u32 dae_out_buf = 4;//todo regs
	u32  dae_ds_num = 1;//1/2 todo add sel
	struct PRM_DPSS_TOP dpss_top = *prm_top;
//ary no use enum DPSS_WORK_MODE dae_mode = dpss_top.dpss_mode;
	u32 me_dsx               = dpss_top.dae_dsx_scale;
	u32 me_dsy               = dpss_top.dae_dsy_scale;
	u32 dae_inp_hsize        = dpss_top.frm_hsize & 0xffff;
	u32 dae_inp_vsize        = dpss_top.frm_vsize & 0xffff;
	u32 dae_me_hsize         = (dae_inp_hsize + (1 << me_dsx) - 1) >> me_dsx;
	u32 dae_me_vsize         = (dae_inp_vsize + (1 << me_dsy) - 1) >> me_dsy;
	u32 dae_blk_hsize        = (dae_me_hsize + 3) >> 2;
	u32 dae_blk_vsize        = (dae_me_vsize + 3) >> 2;

	if (dae_send_num == 0) {
		wr(VPU_DAE_WRAP_SW_SEL0, 0xffffffff);
		wr(VPU_DAE_WRAP_SW_SEL1, 0xffffffff);
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 0x3f, 2, 6);
		//irq sel dae done
		w_reg_bit(VPU_DAE_WRAP_SW_INFO, dae_ds_num << 2 | dae_ds_num, 0, 4);
		//reg_sw_mvy_div_mode,reg_sw_mvx_div_mode
	}

	w_reg_bit(VPU_DAE_WRAP_SW_EN, 1 << 11 |
					0 << 10 |
					0 << 9 |
					0 << 9 |
					7 << 4 |
					(dae_send_num > 1), 12, 10);
	//reg_dae_nr_en,reg_dae_di_en,
	//reg_dae_frc_en,reg_dae_src_mode,reg_dae_src_idx,reg_dae_frm_phs
	w_reg_bit(VPU_DAE_WRAP_SW_EN, 1, 0, 1);//pls_syn_frm_rst_me//
	wr(VPU_DAE_WRAP_SW_SIZE0, (dae_inp_vsize / 2) << 16 | (dae_inp_hsize / 2));
	wr(VPU_DAE_WRAP_SW_SIZE1, dae_me_vsize << 16 | dae_me_hsize);
	wr(VPU_DAE_WRAP_SW_SIZE2, dae_blk_vsize << 16 | dae_blk_hsize);
	cfg_dae_triggle();
	dae_send_num++;
}

static void cfg_dae_secure(unsigned int dae_mode,
		struct PRM_DPSS_TOP  *prm_top,
		struct PRM_DPSS_DAE  *prm_dae)
{
	unsigned int set = 0;

	if (!prm_top->is_secure) {
		if (dae_mode == DAE_NR_MODE ||
		    dae_mode == DAE_VDI_MODE ||
		    dae_mode == DAE_BYPS_MODE ||
		    dae_mode == DAE_NRDI_MODE ||
		    dae_mode == DAE_NR_SRC1_MODE ||
		    dae_mode == DAE_DV_SRC0_MODE) {
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 0, 12, 1);
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 0, 16, 1);
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 0, 24, 1);
			set = 1;
		} else {
			//frc: to-do
			set = 3;
		}
	} else {
		if (dae_mode == DAE_NR_MODE	||
		    dae_mode == DAE_BYPS_MODE	||
		    dae_mode == DAE_NR_SRC1_MODE ||
		    dae_mode == DAE_DV_SRC0_MODE) {
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 1, 16, 1);
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 1, 24, 1);
			set = 11;
		} else if (dae_mode == DAE_VDI_MODE	||
			   dae_mode == DAE_NRDI_MODE) {
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 1, 12, 1);
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 1, 16, 1);
			w_reg_bit(VPU_DAE_WRAP_SW_INFO, 1, 24, 1);
			set = 12;
		} else {
		//frc: to-do
			set = 13;
		}
	}
	dbg_h2("%s:%d\n", __func__, set);
}

//extern volatile Vpu_queue glb_motQ;
