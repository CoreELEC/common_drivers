// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "vfcd.h"
#include <linux/amlogic/media/di/dpss_interface.h>
#include "../dpss_base.h"
#include "../dpss_s.h"
#include "../dpss_sys.h"

#include <linux/amlogic/media/dpss/dpss_frc.h>

void init_dpss_afbcd(int mode, struct AFBCD *prm)
{
	prm->hsize = 320;	//prm_afbcd->hsize;
	prm->vsize = 240;	//prm_afbcd->vsize;
	prm->head_baddr = 0;	//prm_afbcd->head_baddr;
	prm->compbits = 2;	//prm_afbcd->compbits-8;
	prm->fmt_mode = 0;	//prm_afbcd->fmt_mode;
	prm->ddr_sz_mode = 1;
	prm->fmt444_comb = 0;
	prm->dos_uncomp = 0;
	prm->rot_en = 0;
	prm->rot_hbgn = 0;
	prm->rot_vbgn = 0;
	prm->y_h_skip_en = 0;	//prm_afbcd->h_skip_en;
	prm->y_v_skip_en = 0;	//prm_afbcd->v_skip_en;
	prm->c_h_skip_en = 0;	//prm_afbcd->h_skip_en;
	prm->c_v_skip_en = 0;	//prm_afbcd->v_skip_en;
	//prm->hfmt_en             = 0;//prm_afbcd->hfmt_en;
	//prm->vfmt_en             = 0;//prm_afbcd->vfmt_en;
	prm->rev_mode = 0;	//prm_afbcd->rev_mode;
	prm->lossy_en = 0;
	prm->def_color_y = 0x00;
	prm->def_color_u = 0x80;
	prm->def_color_v = 0x80;
	prm->win_bgn_h = 0;	//prm_afbcd->win_bgn_h;
	prm->win_end_h = 319;	//prm_afbcd->win_end_h;
	prm->win_bgn_v = 0;	//prm_afbcd->win_bgn_v;
	prm->win_end_v = 239;	//prm_afbcd->win_end_v;
	prm->pip_src_mode = 0;	//prm_afbcd->out_reg_pip_mode;
	prm->rot_vshrk = 0;
	prm->rot_hshrk = 0;
	prm->rot_drop_mode = 0;
	prm->rot_ofmt_mode = 0;
	prm->rot_ocompbit = 2;
	prm->fix_cr_en = 0;	//prm_afbcd->out_lossy_en==3 ? 0 : prm_afbcd->out_lossy_en;
	prm->brst_len_add_en = 0;	//prm_afbcd->out_brst_len_add_value==0 ? 0 : 1;
	prm->brst_len_add_value = 0;	//prm_afbcd->out_brst_len_add_value;
	prm->ofset_brst4_en = 0;	//prm_afbcd->out_ofset_brst4_en;
}

void init_dpss_afrcd(int mode, struct AFRCD_TYPE *prm)
{
	prm->input_bayer_mode = 1;
	prm->input_fmt_mode = 1;
	prm->raw_8x16_en = 0;

	prm->luma_comp_target = dpss_afrc_y;
	prm->luma_header_en = dpss_afrc_head;
	prm->luma_dict_en = 0;

	prm->chrm_comp_target = dpss_afrc_c;
	prm->chrm_header_en = dpss_afrc_head;
	prm->chrm_dict_en = 0;
}

void init_dpss_vfcd(int mode, struct VFCD_t *prm)
{
	prm->cmpr_sel = 0;
	prm->frm_en_sel = 0;
	prm->fmt444_out = 0;
	prm->src_hsize = 320;
	prm->src_vsize = 240;
	prm->win_bgn_h[0] = 0;
	prm->win_end_h[0] = 319;
	prm->win_bgn_v[0] = 0;
	prm->win_end_v[0] = 239;
	prm->win_bgn_h[1] = 0;
	prm->win_end_h[1] = 319;
	prm->win_bgn_v[1] = 0;
	prm->win_end_v[1] = 239;
	prm->win_bgn_h[2] = 0;
	prm->win_end_h[2] = 319;
	prm->win_bgn_v[2] = 0;
	prm->win_end_v[2] = 239;
	prm->win_bgn_h[3] = 0;
	prm->win_end_h[3] = 319;
	prm->win_bgn_v[3] = 0;
	prm->win_end_v[3] = 239;
	prm->luma_head_baddr = 0;
	prm->luma_body_baddr = 0;
	prm->chrm_head_baddr = 0;
	prm->chrm_body_baddr = 0;
	prm->src_fmt = 0;
	prm->rev_mode = 0;
	prm->hfmt_en = 0;
	prm->vfmt_en = 0;
	prm->compbits_y = 2;
	prm->compbits_c = 2;
	prm->y_h_skip_en = 0;
	prm->y_v_skip_en = 0;
	prm->c_h_skip_en = 0;
	prm->c_v_skip_en = 0;
	prm->film_grain_en = 0;
	prm->pad_en = 0;
	prm->pad_hmode = 0;	//0;alg8 1:alg16
	prm->pad_vmode = 0;	//0;alg8 1:alg16
	prm->slc_num = 1;
	prm->mmu_mode_en = 0;
	prm->mmu_page_mode = 0;
	prm->mmu_baddr0 = 0;
	prm->mmu_baddr1 = 0;
	prm->mif_burst_len = 3;	//mif_burst_len
	prm->yc_rst_sep = 0;
	prm->cut_win_en = 0;
	init_dpss_afbcd(0, &prm->afbcd);
	init_dpss_afrcd(0, &prm->afrcd);
}

void cfg_dpss_afbcd(u32 index,
		    u32 hfmt_en, u32 vfmt_en, struct AFBCD *dpss_afbcd)
{
	u32 hsize_in = dpss_afbcd->hsize;	//=dpss_afbcd->hsize;
	u32 vsize_in = dpss_afbcd->vsize;	//=dpss_afbcd->vsize;
	u32 compbits = dpss_afbcd->compbits;	//=dpss_afbcd->compbits-8;
	u32 mif_info_baddr = dpss_afbcd->head_baddr;	//=dpss_afbcd->head_baddr;
	u32 mif_data_baddr = dpss_afbcd->body_baddr;	//=dpss_afbcd->body_baddr;
	u32 fmt_mode = dpss_afbcd->fmt_mode;	//=dpss_afbcd->fmt_mode;
	u32 ddr_sz_mode = dpss_afbcd->ddr_sz_mode;	//=1;
	u32 reg_fmt444_comb = dpss_afbcd->fmt444_comb;
	u32 reg_dos_uncomp_mode = dpss_afbcd->dos_uncomp;	//=0;
	u32 rot_en = dpss_afbcd->rot_en;	//=0;
	u32 rot_hbgn = dpss_afbcd->rot_hbgn;	//=0;
	u32 rot_vbgn = dpss_afbcd->rot_vbgn;	//=0;
	u32 y_h_skip_en = dpss_afbcd->y_h_skip_en;	//=dpss_afbcd->h_skip_en;
	u32 y_v_skip_en = dpss_afbcd->y_v_skip_en;	//=dpss_afbcd->h_skip_en;
	u32 c_h_skip_en = dpss_afbcd->c_h_skip_en;	//=dpss_afbcd->v_skip_en;
	u32 c_v_skip_en = dpss_afbcd->c_v_skip_en;	//=dpss_afbcd->v_skip_en;
	u32 rev_mode = dpss_afbcd->rev_mode;	//=dpss_afbcd->rev_mode;
	u32 lossy_en = dpss_afbcd->lossy_en;	//=dpss_afbcd->lossy_en ;
	u32 def_color_y = dpss_afbcd->def_color_y;	//=0x00<<(dpss_afbcd->compbits-8);
	u32 def_color_u = dpss_afbcd->def_color_u;	//=0x80<<(dpss_afbcd->compbits-8);
	u32 def_color_v = dpss_afbcd->def_color_v;	//=0x80<<(dpss_afbcd->compbits-8);
	u32 win_bgn_h = dpss_afbcd->win_bgn_h;	//=dpss_afbcd->win_bgn_h;
	u32 win_end_h = dpss_afbcd->win_end_h;	//=dpss_afbcd->win_end_h;
	u32 win_bgn_v = dpss_afbcd->win_bgn_v;	//=dpss_afbcd->win_bgn_v;
	u32 win_end_v = dpss_afbcd->win_end_v;	//=dpss_afbcd->win_end_v;
	u32 pip_src_mode = dpss_afbcd->pip_src_mode;	//=dpss_afbcd->out_reg_pip_mode;
	u32 rot_vshrk = dpss_afbcd->rot_vshrk;	//=0;
	u32 rot_hshrk = dpss_afbcd->rot_hshrk;	//=0;
	u32 rot_drop_mode = dpss_afbcd->rot_drop_mode;	//=2;
	u32 rot_ofmt_mode = dpss_afbcd->rot_ofmt_mode;
	u32 rot_ocompbit = dpss_afbcd->rot_ocompbit;
	u32 fix_cr_en = dpss_afbcd->fix_cr_en;	// 0;//dpss_afbcd->out_ofset_brst4_en;
	u32 brst_len_add_en = dpss_afbcd->brst_len_add_en;	// 1 bits
	u32 brst_len_add_value = dpss_afbcd->brst_len_add_value;	// 3 bits
	u32 ofset_brst4_en = dpss_afbcd->ofset_brst4_en;	// 1 bits

	cfg_dpss_afbcd_mult(index,	// baddr_ofst
			2,	// hold_line_num   , // 7 bits
			0,	// blk_mem_mode    ,
			compbits,	// compbits_y      , // 2 bits 0-8bits 1-9bits 2-10bits
			compbits,	// compbits_u      , // 2 bits 0-8bits 1-9bits 2-10bits
			compbits,	// compbits_v      , // 2 bits 0-8bits 1-9bits 2-10bits
			hsize_in,	// hsize_in        , //13 bits dec source pic hsize
			vsize_in,	// vsize_in        , //13 bits dec source pic vsize
			mif_info_baddr,	// mif_info_baddr  , //32 bits
			mif_data_baddr,	// mif_data_baddr  , //32 bits
			win_bgn_h,	// out_horz_bgn    , //13 bits
			win_bgn_v,	// out_vert_bgn    , //13 bits
			win_end_h,	// out_horz_end    , //13 bits
			win_end_v,	// out_vert_end    , //13 bits
			0,	// hz_ini_phase    , // 4 bits
			0,	// vt_ini_phase    , // 4 bits
			0,	// hz_rpt_fst0_en  , // 1 bits
			0,	// vt_rpt_fst0_en  , // 1 bits
			rev_mode,	// rev_mode        ,
			y_v_skip_en,	// vert_skip_y     ,
			y_h_skip_en,	// horz_skip_y     ,
			c_v_skip_en,	// vert_skip_uv    ,
			c_h_skip_en,	// horz_skip_uv    ,
			hfmt_en, vfmt_en, def_color_y,	// def_color_y     10 bits
			def_color_u,	// def_color_u     10 bits
			def_color_v,	// def_color_v     10 bits
			lossy_en,	// reg_lossy_en        2 bit
			ddr_sz_mode,	// ddr_sz_mode         1 bit  1:mmu mode
			fmt_mode,	// fmt_mode            2 bit  0:444 1:422 2:420
			reg_fmt444_comb,	// reg_fmt444_comb
			reg_dos_uncomp_mode,	// reg_dos_uncomp_mode 1 bit
			pip_src_mode,	// pip_src_mode  1
			rot_ofmt_mode,	// rot_ofmt_mode 2 bit  0:444 1:422 2:420
			rot_ocompbit,	// rot_ocompbit
			rot_drop_mode,	// rot_drop_mode
			rot_vshrk,	// rot_vshrk
			rot_hshrk,	// rot_hshrk
			rot_hbgn,	// rot_hbgn          5 bits
			rot_vbgn,	// rot_vbgn          2 bits
			rot_en,	// rot_enable        1 bit
			fix_cr_en,	// fix_cr_en          1 bits
			brst_len_add_en,	// brst_len_add_en    1 bits
			brst_len_add_value,	// brst_len_add_value 3 bits
			ofset_brst4_en	// ofset_brst4_en     1 bits
		);
}

void cfg_dpss_afbcd_mult(u32 module_sel, u32 hold_line_num,
	u32 blk_mem_mode, u32 compbits_y, u32 compbits_u, u32 compbits_v, u32 hsize_in,
	u32 vsize_in, u32 mif_info_baddr, u32 mif_data_baddr,
	u32 out_horz_bgn, u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 hz_ini_phase, u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 rev_mode, u32 vert_skip_y, u32 horz_skip_y, u32 vert_skip_uv,
	u32 horz_skip_uv, u32 hfmt_en, u32 vfmt_en, u32 def_color_y,
	u32 def_color_u, u32 def_color_v, u32 reg_lossy_en, u32 ddr_sz_mode,
	u32 fmt_mode, u32 reg_fmt444_comb, u32 reg_dos_uncomp_mode,
	u32 pip_src_mode, u32 rot_ofmt_mode, u32 rot_ocompbit,
	u32 rot_drop_mode, u32 rot_vshrk, u32 rot_hshrk,
	u32 rot_hbgn, u32 rot_vbgn, u32 rot_en, u32 fix_cr_en, u32 brst_len_add_en,
	u32 brst_len_add_value, u32 ofset_brst4_en)
{
	u32 compbits_yuv;
	u32 conv_lbuf_len;	//12 bits

	s32 compbits_eq8;
	s32 use_4kram = 0;
	//ary no use    s32 real_hsize_mt2k;
	//ary no use    s32 comp_mt_20bit ;
	s32 regs_ofst;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	regs_ofst = module_sel * 256;	//ary addr change *4
	w_reg_bit_vc(regs_ofst + VFCD_MMU_CTRL, 0, 0, 1);	//vfcd_mmu_mode_en ,bc not support

	compbits_yuv = ((compbits_v & 0x3) << 4) |
		((compbits_u & 0x3) << 2) | ((compbits_y & 0x3) << 0);

	compbits_eq8 = (compbits_y == 0 && compbits_u == 0 && compbits_v == 0);

	u32 tfmt_ram_ofst0;	//todo
	u32 tfmt_ram_ofst1;	//todo
	u32 tfmt_ram_ofst2;	//todo
	u32 mif_lbuf_depth;	//todo
	u32 head_ram_ofst;	//todo
	u32 body_ram_ofst;	//todo

	if (module_sel <= 3) {
		mif_lbuf_depth = 256;
		head_ram_ofst = 48;
		body_ram_ofst = 128;
	} else if (module_sel > 5) {
		mif_lbuf_depth = 256;
		head_ram_ofst = 128;
		body_ram_ofst = 64;
	} else {
		if (pchip_st && pchip_st->chip == ID_T6X)
			mif_lbuf_depth = 128;
		else
			mif_lbuf_depth = 256;
		head_ram_ofst = 128;
		body_ram_ofst = 128;
	}			//4096

	w_reg_bit_vc(regs_ofst + VFCD_AFRC1_PARAM, head_ram_ofst & 0x7ff, 21,
		     11);
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, body_ram_ofst & 0x3ff, 10, 10);
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, 1, 31, 1);	//reg_vfcd_enable

	if (module_sel <= 3) {
		tfmt_ram_ofst0 = 168;
		tfmt_ram_ofst1 = 336;
		if (compbits_yuv == 0x2a && fmt_mode == 0)
			tfmt_ram_ofst0 = 112;
	} else if (module_sel > 5) {
		tfmt_ram_ofst0 = 256;
		tfmt_ram_ofst1 = 512;
	} else {
		tfmt_ram_ofst0 = 512;
		tfmt_ram_ofst1 = 1024;
	}

	dbg_h2("%s:\n", __func__);
	if (fmt_mode == 2 && vfmt_en) {	//bc_420_fmtup need 1line_lbuf
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
		tfmt_ram_ofst1 = tfmt_ram_ofst1 * 3 / 4;
	} else {
		//ary same ? tfmt_ram_ofst1 = tfmt_ram_ofst1 ;
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
	}

	wr_vc((regs_ofst + VFCD_CONV_BUF_LENS), ((tfmt_ram_ofst1 & 0xfff) << 16) |
		((tfmt_ram_ofst0 & 0xfff) << 0)	// ybuf_depth
		);
	wr_vc((regs_ofst + VFCD_AFBC_LBUF_DEPTH), ((tfmt_ram_ofst2 & 0xfff) << 16) |
		((mif_lbuf_depth & 0xfff) << 0)	// mif_lbuf_depth
		);

	if ((hsize_in > 1024 && fmt_mode == 0 && compbits_eq8 == 0) ||
		hsize_in > 2048 || use_4kram)
		conv_lbuf_len = 256;
	else
		conv_lbuf_len = 128;

	w_reg_bit_vc((regs_ofst + VFCD_AFBC_IQUANT_ENABLE), reg_lossy_en & 1, 0, 1);
	w_reg_bit_vc((regs_ofst + VFCD_AFBC_IQUANT_ENABLE), (reg_lossy_en >> 1) & 1, 4, 1);

	w_reg_bit_vc((regs_ofst + VFCD_LUMA_PIC_XPOS), out_horz_bgn, 16, 13);
	w_reg_bit_vc((regs_ofst + VFCD_LUMA_PIC_XPOS), out_horz_end, 0, 13);
	w_reg_bit_vc((regs_ofst + VFCD_LUMA_PIC_YPOS), out_vert_bgn, 16, 13);
	w_reg_bit_vc((regs_ofst + VFCD_LUMA_PIC_YPOS), out_vert_end, 0, 13);
	w_reg_bit_vc((regs_ofst + VFCD_FRM_PIC_SIZE), hsize_in, 16, 13);
	w_reg_bit_vc((regs_ofst + VFCD_FRM_PIC_SIZE), vsize_in, 0, 13);

	wr_vc((regs_ofst + VFCD_AFBC_BURST_CTRL), ((ofset_brst4_en & 0x1) << 4) |
		((brst_len_add_en & 0x1) << 3) |	//brst_len_add_en
		((brst_len_add_value & 0x7) << 0)	//brst_len_add_value
		);

	hold_line_num = hold_line_num > 4 ? hold_line_num - 4 : 0;
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, compbits_yuv, 4, 6);
	wr_vc((regs_ofst + VFCD_AFBC_MODE), ((1) << 30) |	// afbc_emit_mode 0:16DW ,1:64DW
		((ddr_sz_mode & 0x1) << 29) |	// ddr_sz_mode
		((blk_mem_mode & 0x1) << 28) |	// blk_mem_mode
		((rev_mode & 0x3) << 26) |	// rev_mode
		((3) << 24) |	// mif_urgent
		((hold_line_num & 0x7f) << 16) |	// hold_line_num
		((3) << 14) |	// burst_len  //burst8
		//((compbits_yuv & 0x3f)<< 8 ) |  // compbits_yuv
		((vert_skip_y & 0x3) << 6) |	// vert_skip_y
		((horz_skip_y & 0x3) << 4) |	// horz_skip_y
		((vert_skip_uv & 0x3) << 2) |	// vert_skip_uv
		((horz_skip_uv & 0x3) << 0)	// horz_skip_uv
		);

	//wr((regs_ofst+VFCD_AFBC_SIZE_IN),
	//    ((hsize_in & 0x1fff )<< 16) |  // hsize_in
	//    ((vsize_in & 0x1fff )<< 0 )    // vsize_in
	//    );
	wr_vc((regs_ofst + VFCD_AFBC_DEC_DEF_COLOR), ((def_color_u & 0xfff) << 16) |
		((def_color_v & 0xfff) << 0)	// def_color_v
		);

	wr_vc((regs_ofst + VFCD_AFBC_CONV_CTRL), ((def_color_y & 0xfff) << 16) |
		((fmt_mode & 0x3) << 12) |	// fmt_mode
		((conv_lbuf_len & 0xfff) << 0)	// conv_lbuf_len
		);

	wr_vc((regs_ofst + VFCD_AFBC_HEAD_BADDR), mif_info_baddr);
	wr_vc((regs_ofst + VFCD_AFBC_BODY_BADDR), mif_data_baddr);

	wr_vc((regs_ofst + VFCD_AFBC_ENABLE), ((2 & 0x3) << 29) |
		((3 & 0x3) << 21) |
		((reg_fmt444_comb & 0x1) << 20) |	//reg_fmt444_comb
		((reg_dos_uncomp_mode & 0x1) << 19) |	//reg_dos_uncomp_mode
		((0x01701 & 0x7ffff) << 0)	//ddr_blk_size ,cmd_blk_size , lathc_mux
		);
}				//cfg_dpss_afbcd_mult

void cfg_vfcd_fmtup(u32 cmpr_sel, u32 regs_ofst, u32 fmt_mode, u32 hfmt_en,
	u32 vfmt_en, u32 hfmt_mode, u32 vfmt_mode,
	u32 horz_skip_y, u32 vert_skip_y, u32 horz_skip_uv,
	u32 vert_skip_uv, u32 rev_mode_h, u32 rev_mode_v, u32 out_horz_bgn,
	u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 pad_en, u32 pad_hmode, u32 pad_vmode, u32 hz_ini_phase,
	u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 hsize_y_last_slc)
{
	u32 cu_hdw = cmpr_sel == 2 ? 4 : cmpr_sel == 1 ? 5 : 0;
	u32 cu_vdw = 2;
	u32 vt_yc_ratio;
	u32 hz_yc_ratio;
	u32 cvfmt_h;
	u32 chfmt_w;
	u32 vfmt_w;

	u32 win_yout_hsize, win_uvout_hsize; //@0801
	u32 mif_algblk_bgn_h = (out_horz_bgn >> cu_hdw) << cu_hdw;
	u32 mif_algblk_bgn_v = (out_vert_bgn >> cu_vdw) << cu_vdw;
	u32 mif_algblk_end_h = ((out_horz_end >> cu_hdw) << cu_hdw) + ((1 << cu_hdw) - 1);
	u32 mif_algblk_end_v = ((out_vert_end >> cu_vdw) << cu_vdw) + ((1 << cu_vdw) - 1);

	u32 out_end_dif_h = out_horz_end - out_horz_bgn;
	u32 out_end_dif_v = out_vert_end - out_vert_bgn;

	u32 dec_pixel_bgn_h = (out_horz_bgn & ((1 << cu_hdw) - 1));
	u32 dec_pixel_bgn_v = (out_vert_bgn & ((1 << cu_vdw) - 1));
	u32 dec_pixel_end_h = (dec_pixel_bgn_h + out_end_dif_h);
	u32 dec_pixel_end_v = (dec_pixel_bgn_v + out_end_dif_v);

	dec_pixel_bgn_h =
		rev_mode_h ? mif_algblk_end_h - dec_pixel_end_h : dec_pixel_bgn_h;
	dec_pixel_bgn_v =
		rev_mode_v ? mif_algblk_end_v - dec_pixel_end_v : dec_pixel_bgn_v;
	dec_pixel_end_h =
		rev_mode_h ? mif_algblk_end_h - dec_pixel_bgn_h : dec_pixel_end_h;
	dec_pixel_end_v =
		rev_mode_v ? mif_algblk_end_v - dec_pixel_bgn_v : dec_pixel_end_v;

	u32 fmt_size_h = mif_algblk_end_h - mif_algblk_bgn_h + 1;
	u32 fmt_size_v = mif_algblk_end_v - mif_algblk_bgn_v + 1;

	fmt_size_h = horz_skip_y != 0 ? (fmt_size_h >> 1) : fmt_size_h;
	fmt_size_v = vert_skip_y != 0 ? (fmt_size_v >> 1) : fmt_size_v;

	//dbg_h2("===fmt_size_h = %0d\n",fmt_size_h );
	//dbg_h2("===fmt_size_v = %0d\n",fmt_size_v );

	if (fmt_mode == 2) {	//420
		if (vert_skip_uv == 0 && vert_skip_y == 0)
			vt_yc_ratio = 2;
		else if (vert_skip_uv == 0 && vert_skip_y != 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y != 0)
			vt_yc_ratio = 2;
		else
			vt_yc_ratio = 0;	//cfg_error
	} else {		//422 or 444
		if (vert_skip_uv == 0 && vert_skip_y == 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y != 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y == 0)
			vt_yc_ratio = 2;
		else
			vt_yc_ratio = 0;	//cfg_error
	}

	if (fmt_mode != 0) {	//420 or 422
		if (horz_skip_uv == 0 && horz_skip_y == 0)
			hz_yc_ratio = 2;
		else if (horz_skip_uv == 0 && horz_skip_y != 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y != 0)
			hz_yc_ratio = 2;
		else
			hz_yc_ratio = 0;	//cfg_error

	} else {		//444
		if (horz_skip_uv == 0 && horz_skip_y == 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y != 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y == 0)
			hz_yc_ratio = 2;
		else
			hz_yc_ratio = 0;	//cfg_error
	}

	//FMT_IN
	cvfmt_h = vt_yc_ratio == 0 ? 0 : (fmt_size_v >> (vt_yc_ratio - 1));
	vfmt_w = hz_yc_ratio == 0 ? 0 : (fmt_size_h >> (hz_yc_ratio - 1));
	//FMT_OUT
	chfmt_w = hfmt_en ? vfmt_w * 2 : vfmt_w;	//fmt_out

	//dbg_h2("===cvfmt_h = %0d\n",cvfmt_h );
	//dbg_h2("===cvfmt_w = %0d\n",vfmt_w  );
	//dbg_h2("===chfmt_w = %0d\n",chfmt_w );

	//u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vt_phase_step = 8;

	wr_vc((regs_ofst + VFCD_AFBC_VD_CFMT_CTRL), (hfmt_mode << 28) |
		(hz_ini_phase << 24) |	//hz ini phase
		(hz_rpt_fst0_en << 23) |	//repeat p0 enable
		((hz_yc_ratio - 1) << 21) |	//hz yc ratio
		(hfmt_en << 20) |	//hz enable
		(vfmt_mode << 19) |	//cvfmt_always_en
		(0 << 18) |	//cvfmt_rpt_lst_line_disable
		(1 << 17) |	//nrpt_phase0 enable
		(vt_rpt_fst0_en << 16) |	//repeat l0 enable
		(0 << 12) |	//skip line num
		(vt_ini_phase << 8) |	//vt ini phase
		(vt_phase_step << 1) |	//vt phase step (3.4)
		(vfmt_en << 0)	//vt enable
		);

	wr_vc((regs_ofst + VFCD_AFBC_VD_CFMT_W), (chfmt_w << 16) |	//hz format width
		(vfmt_w << 0)	//vt format width
		);

	wr_vc((regs_ofst + VFCD_POST_LUMA_SIZE), (fmt_size_v & 0x1fff) << 16 |
		(fmt_size_h & 0x1fff));
	w_reg_bit_vc((regs_ofst + VFCD_AFBC_VD_CFMT_H), cvfmt_h & 0x1fff, 0,
			13);

	vt_yc_ratio = vfmt_en ? vt_yc_ratio / 2 : vt_yc_ratio;
	hz_yc_ratio = hfmt_en ? hz_yc_ratio / 2 : hz_yc_ratio;
	//dbg_h2("===vt_yc_ratio= %0x\n",vt_yc_ratio );
	//dbg_h2("===hz_yc_ratio= %0x\n",hz_yc_ratio );
	//dbg_h2("===hfmt_en    = %0x\n",hfmt_en   );
	//dbg_h2("===vfmt_en    = %0x\n",vfmt_en   );
	//dbg_h2("===horz_skip_y = %0x\n",horz_skip_y );
	//dbg_h2("===vert_skip_y = %0x\n",vert_skip_y );
	//dbg_h2("===horz_skip_uv= %0x\n",horz_skip_uv);
	//dbg_h2("===vert_skip_uv= %0x\n",vert_skip_uv);

	dec_pixel_bgn_h = horz_skip_y ? dec_pixel_bgn_h >> 1 : dec_pixel_bgn_h;
	dec_pixel_bgn_v = vert_skip_y ? dec_pixel_bgn_v >> 1 : dec_pixel_bgn_v;
	dec_pixel_end_h = horz_skip_y ? dec_pixel_end_h >> 1 : dec_pixel_end_h;
	dec_pixel_end_v = vert_skip_y ? dec_pixel_end_v >> 1 : dec_pixel_end_v;
	win_yout_hsize = horz_skip_y ? hsize_y_last_slc >> 1 : hsize_y_last_slc; //@0801

	u32 dec_pixel_uv_bgn_h =
		hz_yc_ratio == 2 ? dec_pixel_bgn_h >> 1 : dec_pixel_bgn_h;
	u32 dec_pixel_uv_bgn_v =
		vt_yc_ratio == 2 ? dec_pixel_bgn_v >> 1 : dec_pixel_bgn_v;
	u32 dec_pixel_uv_end_h =
		hz_yc_ratio == 2 ? dec_pixel_end_h >> 1 : dec_pixel_end_h;
	u32 dec_pixel_uv_end_v =
		vt_yc_ratio == 2 ? dec_pixel_end_v >> 1 : dec_pixel_end_v;

	win_uvout_hsize =
		vt_yc_ratio == 2 ? win_yout_hsize >> 1 : win_yout_hsize; //@0801

	wr_vc((regs_ofst + VFCD_POST_LUMA_WIN_H),
		dec_pixel_bgn_h << 16 | dec_pixel_end_h);
	wr_vc((regs_ofst + VFCD_POST_CHRM_WIN_H),
		dec_pixel_uv_bgn_h << 16 | dec_pixel_uv_end_h);
	wr_vc((regs_ofst + VFCD_POST_LUMA_WIN_V),
		dec_pixel_bgn_v << 16 | dec_pixel_end_v);
	wr_vc((regs_ofst + VFCD_POST_CHRM_WIN_V),
		dec_pixel_uv_bgn_v << 16 | dec_pixel_uv_end_v);

//@0801	u32 win_yout_hsize = dec_pixel_end_h - dec_pixel_bgn_h + 1;
	u32 win_yout_vsize = dec_pixel_end_v - dec_pixel_bgn_v + 1;
//@0801	u32 win_uvout_hsize = dec_pixel_uv_end_h - dec_pixel_uv_bgn_h + 1;
	u32 win_uvout_vsize = dec_pixel_uv_end_v - dec_pixel_uv_bgn_v + 1;

	//u32 pad_en          = 0;
	u32 pad_ydout_hofst = 0;
	u32 pad_ydout_vofst = 0;
#ifdef _HIS_CODE_
	u32 pad_ydout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_yout_hsize + 7) / 8 * 8 : (win_yout_hsize +
			15) / 16 * 16) : win_yout_hsize;
#else	//@0801
	u32 pad_ydout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_yout_hsize + 7) / 8 * 8 : (win_yout_hsize +
			15) / 16 * 16) : win_yout_hsize;
#endif
	u32 pad_ydout_vsize =
		pad_en ? (pad_vmode ==
			0 ? (win_yout_vsize + 7) / 8 * 8 :
			(win_yout_vsize + 15) / 16 * 16) : win_yout_vsize;
	u32 pad_cdout_hofst = 0;
	u32 pad_cdout_vofst = 0;
#ifdef _HIS_CODE_
	u32 pad_cdout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_uvout_hsize + 7) / 8 * 8 :
			(win_uvout_hsize + 5) / 16 * 16) : win_uvout_hsize;
#else	//@0801
	u32 pad_cdout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_uvout_hsize + 7) / 8 * 8 :
			(win_uvout_hsize + 15) / 16 * 16) : win_uvout_hsize;
#endif
	u32 pad_cdout_vsize =
		pad_en ? (pad_vmode ==
			0 ? (win_uvout_vsize + 7) / 8 * 8 :
			(win_uvout_vsize + 15) / 16 * 16) : win_uvout_vsize;

	pad_ydout_vsize = win_yout_vsize;//ary debug
	pad_cdout_vsize = win_uvout_vsize;//ary debug
	dbg_h2("pad_en=%d, pad_mode=%d pad_ydout_vsize =%d c-%d\n",
		pad_en, pad_hmode, pad_ydout_vsize, pad_cdout_vsize);
	dbg_h2("h pad: %d,%d\n", pad_ydout_hsize, pad_cdout_hsize);
	dbg_h2("h size slc: %d,%d\n", hsize_y_last_slc, out_end_dif_h + 1);
	dbg_h2("h win: %d,%d\n", win_yout_hsize, win_uvout_hsize);
	u32 index = regs_ofst / (256 * 4);

	if (dpss_dbg_0709 & C_BIT0) {
		dbg_h2("vfcd index is%0x, pad_ydout_vsize=0x%x\n", index, pad_ydout_vsize);
		wr_vc((regs_ofst + VFCD_LUMA_PAD_SIZE),
			pad_en << 29 | ((pad_ydout_vsize & 0x7ff) << 16) |
			(pad_ydout_hsize & 0x7ff));
		wr_vc((regs_ofst + VFCD_LUMA_PAD_OFST),
			pad_ydout_vofst << 16 | pad_ydout_hofst);

		wr_vc((regs_ofst + VFCD_CHRM_PAD_SIZE),
			pad_cdout_vsize << 16 | pad_cdout_hsize);
		wr_vc((regs_ofst + VFCD_CHRM_PAD_OFST),
			pad_cdout_vofst << 16 | pad_cdout_hofst);
	} else {
		dbg_h2("disable vfcd_wr\n");
	}
	switch (index) {
	case 0:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12bit  {luma_data,chrm_data}
	case 1:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 2:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 3:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 4:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	case 5:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 6:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	default:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;
	}
}

void cfg_vfcd_afrcd(u32 src_sel, struct AFRCD_TYPE *afrcd)
{
	u32 reg_ofst = src_sel;

//declare
	u32 luma_pixel_format = 0;
//ary set no use        u32 luma_raw_last_th   = 0;
	u32 luma_max_ac_depth = 0;
	u32 luma_last_extend_en = 0;

	u32 chrm_pixel_format = 0;
//ary set no use        u32 chrm_raw_last_th   = 0;
	u32 chrm_max_ac_depth = 0;
	u32 chrm_last_extend_en = 0;
	u32 pixel_is_diff_chn = 0;

	u32 afrc_raw_en = afrcd->input_fmt_mode == 2;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (afrcd->chrm_comp_target < 16 || afrcd->chrm_comp_target > 48)
		dbg_h2("rtl not support this target\n");

	afrcd->chrm_cu_bits = (afrcd->chrm_comp_target - 16) / 4;
	afrcd->luma_cu_bits = (afrcd->luma_comp_target - 16) / 4;
	afrcd->chrm_pixel_bits = afrcd->compbits_chrm;
	afrcd->luma_pixel_bits = afrcd->compbits_luma;

////luma
	if (afrcd->input_fmt_mode == 0) {	//rgb
		luma_pixel_format = 2;
		chrm_pixel_format = 2;
		pixel_is_diff_chn = 1;
	} else if (afrcd->input_fmt_mode == 1) {	//yuv
		luma_pixel_format = 5;
		chrm_pixel_format = 5;
		pixel_is_diff_chn = 0;
	} else if (afrcd->raw_8x16_en) {	//raw_8x16_for_npu
		luma_pixel_format = 8;
		chrm_pixel_format = 8;
		pixel_is_diff_chn = 0;
	} else if ((afrcd->input_fmt_mode == 2) && (afrcd->input_bayer_mode == 0)) {
		luma_pixel_format = 6;
		chrm_pixel_format = 6;
		pixel_is_diff_chn = 0;
	} else if ((afrcd->input_fmt_mode == 2) && (afrcd->input_bayer_mode == 1)) {
		luma_pixel_format = 7;
		chrm_pixel_format = 7;
		pixel_is_diff_chn = 1;
	} else {
		dbg_h2("rtl not support reg_input_fmt_mode\n");
	}

	if (afrcd->compbits_luma <= 10) {
		//ary set no use   luma_raw_last_th           = 9;
		luma_max_ac_depth = 9;
		luma_last_extend_en = 0;
	} else if (afrcd->compbits_luma <= 12) {
//ary set no use    luma_raw_last_th           = 12;
		luma_max_ac_depth = 12;
		luma_last_extend_en = 1;
	} else {
		dbg_h2("rtl not support reg_compbits_y\n");
	}

//chrm
	if (afrcd->compbits_chrm <= 10) {
//ary set no use    chrm_raw_last_th           = 9;
		chrm_max_ac_depth = 9;
		chrm_last_extend_en = 0;
	} else if (afrcd->compbits_chrm <= 12) {
//ary set no use    chrm_raw_last_th           = 12;
		chrm_max_ac_depth = 12;
		chrm_last_extend_en = 1;
	} else {
		dbg_h2("rtl not support reg_compbits_c\n");
	}
	u32 mmu_mode_en = afrcd->luma_header_en ? 0 : afrcd->mmu_mode_en;

	w_reg_bit_vc(reg_ofst + VFCD_MIF1_URGENT, afrc_raw_en, 25, 1);
	w_reg_bit_vc(reg_ofst + VFCD_MMU_BADDR0, afrcd->mmu_baddr0, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_MMU_BADDR1, afrcd->mmu_baddr1, 0, 32);
	wr_vc(reg_ofst + VFCD_MMU_CTRL, ((4 & 0x1f) << 2 |
					 (afrcd->mmu_page_mode & 0x1) << 1 |
					 (mmu_mode_en & 0x1)));
//for luma
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, luma_pixel_format, 0, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, afrcd->luma_dict_en, 4, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, 1, 12, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, luma_last_extend_en, 16, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, luma_max_ac_depth, 20, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, pixel_is_diff_chn, 24, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL0, afrcd->input_bayer_mode, 25, 2);
	w_reg_bit_vc(reg_ofst + VFCD_AFBC_HEAD_BADDR, afrcd->luma_head_addr, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_AFBC_BODY_BADDR, afrcd->luma_body_addr, 0, 32);

	w_reg_bit_vc(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_header_en, 4, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_cu_bits, 8, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_pixel_bits, 20, 4);
	if (pchip_st && pchip_st->chip == ID_T6X)
		w_reg_bit_vc(reg_ofst + VFCD_AFRC0_CTRL, 1, 30, 1);

//for chrm
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, chrm_pixel_format, 0, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, afrcd->chrm_dict_en, 4, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, 1, 12, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, chrm_last_extend_en, 16, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, chrm_max_ac_depth, 20, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, pixel_is_diff_chn, 24, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CTRL1, afrcd->input_bayer_mode, 25, 2);

	w_reg_bit_vc(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_header_en, 4, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_cu_bits, 8, 4);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_pixel_bits, 20, 4);
	if (pchip_st && pchip_st->chip == ID_T6X)
		w_reg_bit_vc(reg_ofst + VFCD_AFRC1_CTRL, 1, 30, 1);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CHRM_HEAD_BADDR, afrcd->chrm_head_addr, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CHRM_BODY_BADDR, afrcd->chrm_body_addr, 0, 32);
}

void cfg_vfcd_dec(u32 index, struct VFCD_t *vfcd)
{
	u32 hfmt_mode = vfcd->hfmt_mode;
	u32 cmpr_sel = vfcd->cmpr_sel;
	u32 fmt444_out = vfcd->fmt444_out;
	unsigned int last_h_size_y, idx_last;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	dbg_h2("%s:[%d] fmt444_out = %d\n", __func__, index, fmt444_out);
	u32 regs_ofst = index * 256;	//ary addr change *4
	// switch(index){
	//     case 0 : regs_ofst = index*256*4;break;
	//     case 1 : regs_ofst = index*256*4;break;
	//     case 2 : regs_ofst = index*256*4;break;
	//     case 3 : regs_ofst = index*256*4;break;
	//     case 4 : regs_ofst = index*256*4;break;
	//     case 5 : regs_ofst = index*256*4;break;
	//     default: regs_ofst = index*256*4;break;
	// }
	u32 hold_line_num = 4;

	if (pchip_st && (pchip_st->chip == ID_T6X ||
		pchip_st->chip == ID_T6W) &&
		(regs_ofst == 0x400 || regs_ofst == 0x500)) {
		hold_line_num = 6; //8
		if (pchip_st->dbg_vfcd_holdline)
			hold_line_num = pchip_st->dbg_vfcd_holdline;
	}
	//ary no use    u32 flim_grain_src_idx = 0;
	w_reg_bit_vc((regs_ofst + VFCD_TOP_CTRL2), 1, 31, 1);	//reg_vfcd_enable
	w_reg_bit_vc((regs_ofst + VFCD_TOP_CTRL2), vfcd->yc_rst_sep, 30, 1);
	w_reg_bit_vc((regs_ofst + VFCD_TOP_CTRL0), vfcd->slc_num, 24, 3);
	w_reg_bit_vc((regs_ofst + VFCD_TOP_CTRL0), hold_line_num, 4, 12);
	w_reg_bit_vc((regs_ofst + VFCD_TOP_CTRL0), vfcd->frm_en_sel, 2, 2);
	if ((vfcd->cmpr_sel && index == 0) ||
	   (vfcd->src_is_i && vfcd->cmpr_sel && index == 1))
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 1, 27, 2);	//patch-0404

	if (vfcd->cut_win_en == 1)
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 2, 27, 2);
	else if (regs_ofst == 0x400)
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 0, 27, 2);

	u32 fmt_mode = (cmpr_sel == 2 && vfcd->afrcd.input_fmt_mode == 2) ? 1 : vfcd->src_fmt;
	u32 horz_skip_y = vfcd->y_h_skip_en;
	u32 vert_skip_y = vfcd->y_v_skip_en;
	u32 horz_skip_uv = vfcd->c_h_skip_en;
	u32 vert_skip_uv = vfcd->c_v_skip_en;
	u32 rev_mode = vfcd->rev_mode;
	u32 hfmt_en	  = (fmt444_out == 1) ? (vfcd->src_fmt != 0) : 0;
	u32 vfmt_en	  = (fmt444_out != 0) ? (vfcd->src_fmt == 2) : 0;
	u32 cmpb_y_map = vfcd->compbits_y == 12 ? 3 : vfcd->compbits_y - 8;
	u32 cmpb_c_map = vfcd->compbits_c == 12 ? 3 : vfcd->compbits_c - 8;

	u32 compbits_v = cmpb_c_map;
	u32 compbits_u = cmpb_c_map;
	u32 compbits_y = cmpb_y_map;

	u32 chrm_win_bgn_h_0 =
		fmt_mode != 0 ? vfcd->win_bgn_h[0] >> 1 : vfcd->win_bgn_h[0];
	u32 chrm_win_end_h_0 =
		fmt_mode != 0 ? vfcd->win_end_h[0] >> 1 : vfcd->win_end_h[0];
	u32 chrm_win_bgn_v_0 =
		fmt_mode == 2 ? vfcd->win_bgn_v[0] >> 1 : vfcd->win_bgn_v[0];
	u32 chrm_win_end_v_0 =
		fmt_mode == 2 ? vfcd->win_end_v[0] >> 1 : vfcd->win_end_v[0];

	u32 chrm_win_bgn_h_1 =
		fmt_mode != 0 ? vfcd->win_bgn_h[1] >> 1 : vfcd->win_bgn_h[1];
	u32 chrm_win_end_h_1 =
		fmt_mode != 0 ? vfcd->win_end_h[1] >> 1 : vfcd->win_end_h[1];
	u32 chrm_win_bgn_h_2 =
		fmt_mode != 0 ? vfcd->win_bgn_h[2] >> 1 : vfcd->win_bgn_h[2];
	u32 chrm_win_end_h_2 =
		fmt_mode != 0 ? vfcd->win_end_h[2] >> 1 : vfcd->win_end_h[2];
	u32 chrm_win_bgn_h_3 =
		fmt_mode != 0 ? vfcd->win_bgn_h[3] >> 1 : vfcd->win_bgn_h[3];
	u32 chrm_win_end_h_3 =
		fmt_mode != 0 ? vfcd->win_end_h[3] >> 1 : vfcd->win_end_h[3];

	vfcd->afbcd.hsize = vfcd->src_hsize;
	vfcd->afbcd.vsize = vfcd->src_vsize;
	vfcd->afbcd.compbits = compbits_y;
	vfcd->afbcd.head_baddr = vfcd->luma_head_baddr;
	vfcd->afbcd.body_baddr = vfcd->luma_body_baddr;
	vfcd->afbcd.fmt_mode = fmt_mode;

	if (vfcd->compbits_y == 12)
		vfcd->afbcd.def_color_y = 0xfff;
	else if (vfcd->compbits_y == 10)
		vfcd->afbcd.def_color_y = 0x3ff;
	else
		vfcd->afbcd.def_color_y = 0x0ff;
	vfcd->afbcd.def_color_u = 0x80 << (vfcd->compbits_c - 8);
	vfcd->afbcd.def_color_v = 0x80 << (vfcd->compbits_c - 8);
	vfcd->afbcd.win_bgn_h = vfcd->win_bgn_h[0];
	vfcd->afbcd.win_end_h = vfcd->win_end_h[0];
	vfcd->afbcd.win_bgn_v = vfcd->win_bgn_v[0];
	vfcd->afbcd.win_end_v = vfcd->win_end_v[0];
	vfcd->afbcd.y_h_skip_en = vfcd->y_h_skip_en;
	vfcd->afbcd.y_v_skip_en = vfcd->y_v_skip_en;
	vfcd->afbcd.c_h_skip_en = vfcd->c_h_skip_en;
	vfcd->afbcd.c_v_skip_en = vfcd->c_v_skip_en;
	vfcd->afbcd.rev_mode = vfcd->rev_mode;

	vfcd->afrcd.input_fmt_mode = 1;	//1:yuv
	vfcd->afrcd.input_bayer_mode = 0;
	vfcd->afrcd.luma_head_addr = vfcd->luma_head_baddr;
	vfcd->afrcd.luma_body_addr = vfcd->luma_body_baddr;
	//vfcd->afrcd.luma_dict_en = 0;
	vfcd->afrcd.compbits_luma = vfcd->compbits_c;

	vfcd->afrcd.chrm_head_addr = vfcd->chrm_head_baddr;
	vfcd->afrcd.chrm_body_addr = vfcd->chrm_body_baddr;
	//vfcd->afrcd.chrm_dict_en = 0;
	vfcd->afrcd.mmu_baddr0 = vfcd->mmu_baddr0;
	vfcd->afrcd.mmu_baddr1 = vfcd->mmu_baddr1;

	u32 compbits_yuv = ((compbits_v & 0x3) << 4) |
		((compbits_u & 0x3) << 2) | ((compbits_y & 0x3) << 0);
	vfcd->afrcd.compbits_chrm = vfcd->compbits_y;
	vfcd->afrcd.mmu_mode_en = vfcd->mmu_mode_en;
	vfcd->afrcd.mmu_page_mode = vfcd->mmu_page_mode;

	u32 mif_lbuf_depth;
	u32 head_ram_ofst;
	u32 body_ram_ofst;
	u32 tfmt_ram_ofst0;
	u32 tfmt_ram_ofst1;
	u32 tfmt_ram_ofst2;

	if (index <= 3) {
		mif_lbuf_depth = 128;
		head_ram_ofst = 48;
		body_ram_ofst = 128;
	} else if (index > 5) {
		mif_lbuf_depth = 128;
		head_ram_ofst = 64;
		body_ram_ofst = 128;
	} else {
		if (pchip_st && pchip_st->chip == ID_T6X) {
			mif_lbuf_depth = 64;
			head_ram_ofst = 64;
			body_ram_ofst = 64;
		} else {
			mif_lbuf_depth = 128;
			head_ram_ofst = 128;
			body_ram_ofst = 128;
		}
	}			//4096

	if (cmpr_sel == 1)
		mif_lbuf_depth = mif_lbuf_depth * 2;

	if (index <= 3) {
		tfmt_ram_ofst0 = 168;
		tfmt_ram_ofst1 = 336;
		if (vfcd->adj_offset)
			tfmt_ram_ofst0 = 112;
	} else if (index > 5) {
		tfmt_ram_ofst0 = 256;
		tfmt_ram_ofst1 = 512;
	} else {
		tfmt_ram_ofst0 = 512;
		tfmt_ram_ofst1 = 1024;
	}			//4096

	if (cmpr_sel == 1 && fmt_mode == 2 && vfmt_en) {
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
		tfmt_ram_ofst1 = tfmt_ram_ofst1 * 3 / 4;
	} else {
		//ary same?        tfmt_ram_ofst1 = tfmt_ram_ofst1 ;
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
	}
	if (cmpr_sel == 1 || cmpr_sel == 2) {
		w_reg_bit_vc(regs_ofst + VFCD_AFRC1_PARAM,
			head_ram_ofst & 0x7ff, 21, 11);
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, body_ram_ofst & 0x3ff,
			10, 10);

		wr_vc((regs_ofst + VFCD_CONV_BUF_LENS), ((tfmt_ram_ofst1 & 0xfff) << 16) |
			((tfmt_ram_ofst0 & 0xfff) << 0));

		wr_vc((regs_ofst + VFCD_AFBC_LBUF_DEPTH), ((tfmt_ram_ofst2 & 0xfff) << 16) |
			((mif_lbuf_depth & 0xfff) << 0));

		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, compbits_yuv, 4, 6);
		wr_vc((regs_ofst + VFCD_FRM_PIC_SIZE),
			(vfcd->src_hsize << 16) | (vfcd->src_vsize & 0xffff));
		wr_vc((regs_ofst + VFCD_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[0] << 16) | (vfcd->win_end_h[0] & 0x1fff));
		wr_vc((regs_ofst + VFCD_LUMA_PIC_YPOS),
			(vfcd->win_bgn_v[0] << 16) | (vfcd->win_end_v[0] & 0x1fff));
		wr_vc((regs_ofst + VFCD_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_0 << 16) | (chrm_win_end_h_0 & 0x1fff));
		wr_vc((regs_ofst + VFCD_CHRM_PIC_YPOS),
			(chrm_win_bgn_v_0 << 16) | (chrm_win_end_v_0 & 0x1fff));

		wr_vc((regs_ofst + VFCD_SLC1_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[1] << 16) | (vfcd->win_end_h[1] & 0x1fff));
		wr_vc((regs_ofst + VFCD_SLC1_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_1 << 16) | (chrm_win_end_h_1 & 0x1fff));
		wr_vc((regs_ofst + VFCD_SLC2_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[2] << 16) | (vfcd->win_end_h[2] & 0x1fff));
		wr_vc((regs_ofst + VFCD_SLC2_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_2 << 16) | (chrm_win_end_h_2 & 0x1fff));
		wr_vc((regs_ofst + VFCD_SLC3_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[3] << 16) | (vfcd->win_end_h[3] & 0x1fff));
		wr_vc((regs_ofst + VFCD_SLC3_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_3 << 16) | (chrm_win_end_h_3 & 0x1fff));
		wr_vc((regs_ofst + VFCD_AFBC_MODE), ((1) << 30) |
			((1 & 0x1) << 29) |	// ddr_sz_mode
			((0 & 0x1) << 28) |	// blk_mem_mode
			((rev_mode & 0x3) << 26) |	// rev_mode
			((3) << 24) |	// mif_urgent
			((4 & 0x7f) << 16) |	// hold_line_num
			((vfcd->mif_burst_len) << 14) |
			((vert_skip_y & 0x3) << 6) |	// vert_skip_y
			((horz_skip_y & 0x3) << 4) |	// horz_skip_y
			((vert_skip_uv & 0x3) << 2) |	// vert_skip_uv
			((horz_skip_uv & 0x3) << 0)	// horz_skip_uv
			);
		wr_vc((regs_ofst + VFCD_AFBC_CONV_CTRL), ((fmt_mode & 0x3) << 12));
		//w_reg_bit_vc(regs_ofst+VFCD_AFBC_MODE,vfcd->mif_burst_len,14,2);
	}

	idx_last = vfcd->slc_num - 1;
	last_h_size_y = vfcd->win_end_h[idx_last] - vfcd->win_bgn_h[idx_last] + 1;

	if (cmpr_sel == 1) {	//afbc
		cfg_dpss_afbcd(index, hfmt_en, vfmt_en, &vfcd->afbcd);
		cfg_vfcd_fmtup(2, regs_ofst, fmt_mode, hfmt_en, vfmt_en, hfmt_mode, 0,
			horz_skip_y, vert_skip_y, horz_skip_uv,
			vert_skip_uv, rev_mode & 0x1, rev_mode & 0x2,
			vfcd->win_bgn_h[0], vfcd->win_bgn_v[0],
			vfcd->win_end_h[0], vfcd->win_end_v[0],
			//0, 0,
			vfcd->pad_en, vfcd->pad_hmode,
			0,
			0, 0, 0, 0,
			last_h_size_y);
		dbg_h2("vfcd_afbc_cfg_done\n");
	} else if (cmpr_sel == 2) {	//afrc
		cfg_vfcd_afrcd(regs_ofst, &vfcd->afrcd);
		cfg_vfcd_fmtup(2, regs_ofst, fmt_mode, hfmt_en, vfmt_en, hfmt_mode, 0,
			horz_skip_y, vert_skip_y, horz_skip_uv,
			vert_skip_uv, rev_mode & 0x1, rev_mode & 0x2,
			vfcd->win_bgn_h[0], vfcd->win_bgn_v[0],
			vfcd->win_end_h[0], vfcd->win_end_v[0],
			0, 0,
			0,
			0, 0, 0, 0,
			0);
		dbg_h2("vfcd_afrc_cfg_done\n");
	} else {
	}
}

void cfg_vfcd_cmpr_enable(u32 index, u32 cmp_sel)
{
	u32 regs_ofst = index * 256;	//ary addr change *4

	if (cmp_sel == 0) {
		dbg_h2("[VFCD] index: %d, mode: MIF\n", index);
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 0x2, 16, 2);	//vfcd_chose mif
	} else if (cmp_sel == 1) {
		dbg_h2("[VFCD] index: %d, mode: AFBC\n", index);
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 0x1, 16, 2);	//vfcd_chose afbc
	} else if (cmp_sel == 2) {
		dbg_h2("[VFCD] index: %d, mode: AFRC\n", index);
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 0x3, 16, 2);	//vfcd_chose afrc
	}
}

void afbcd_update_addr(struct PRM_DPSS_TOP *prm_top, unsigned int cnt)
{
	unsigned int reg_ofst;
	unsigned int src_sel;
	unsigned int idx;
	unsigned int addr;

	if (!prm_top->afbcd_update) {
		dbg_h2("afbcd nothing\n");
		return;
	}
	if (prm_top->afbcd_update == 1)
		src_sel = 0;
	else
		src_sel = 1;

	reg_ofst = src_sel * 256;

	w_reg_bit_vc(reg_ofst + VFCD_TOP_CTRL0, 1, 27, 2);

	idx = cnt % prm_top->num_in;
	addr = prm_top->src0_head_yaddr[idx] << 5;

	wr_vc(reg_ofst + VFCD_AFBC_HEAD_BADDR,  addr);//for luma_addr ,
	wr_vc(reg_ofst + VFCD_AFBC_BODY_BADDR, prm_top->src0_fbuf_yaddr[idx] << 5);
	dbg_h2("afbcd_addr:update:src[%d]:idx=%d:0x%08x\n",
		prm_top->src_mode, idx, addr);
}

void frc_cfg_dpss_afbcd(u32 index,
		    u32 hfmt_en, u32 vfmt_en, struct AFBCD *dpss_afbcd)
{
	u32 hsize_in = dpss_afbcd->hsize;	//=dpss_afbcd->hsize;
	u32 vsize_in = dpss_afbcd->vsize;	//=dpss_afbcd->vsize;
	u32 compbits = dpss_afbcd->compbits;	//=dpss_afbcd->compbits-8;
	u32 mif_info_baddr = dpss_afbcd->head_baddr;	//=dpss_afbcd->head_baddr;
	u32 mif_data_baddr = dpss_afbcd->body_baddr;	//=dpss_afbcd->body_baddr;
	u32 fmt_mode = dpss_afbcd->fmt_mode;	//=dpss_afbcd->fmt_mode;
	u32 ddr_sz_mode = dpss_afbcd->ddr_sz_mode;	//=1;
	u32 reg_fmt444_comb = dpss_afbcd->fmt444_comb;
	u32 reg_dos_uncomp_mode = dpss_afbcd->dos_uncomp;	//=0;
	u32 rot_en = dpss_afbcd->rot_en;	//=0;
	u32 rot_hbgn = dpss_afbcd->rot_hbgn;	//=0;
	u32 rot_vbgn = dpss_afbcd->rot_vbgn;	//=0;
	u32 y_h_skip_en = dpss_afbcd->y_h_skip_en;	//=dpss_afbcd->h_skip_en;
	u32 y_v_skip_en = dpss_afbcd->y_v_skip_en;	//=dpss_afbcd->h_skip_en;
	u32 c_h_skip_en = dpss_afbcd->c_h_skip_en;	//=dpss_afbcd->v_skip_en;
	u32 c_v_skip_en = dpss_afbcd->c_v_skip_en;	//=dpss_afbcd->v_skip_en;
	u32 rev_mode = dpss_afbcd->rev_mode;	//=dpss_afbcd->rev_mode;
	u32 lossy_en = dpss_afbcd->lossy_en;	//=dpss_afbcd->lossy_en ;
	u32 def_color_y = dpss_afbcd->def_color_y;	//=0x00<<(dpss_afbcd->compbits-8);
	u32 def_color_u = dpss_afbcd->def_color_u;	//=0x80<<(dpss_afbcd->compbits-8);
	u32 def_color_v = dpss_afbcd->def_color_v;	//=0x80<<(dpss_afbcd->compbits-8);
	u32 win_bgn_h = dpss_afbcd->win_bgn_h;	//=dpss_afbcd->win_bgn_h;
	u32 win_end_h = dpss_afbcd->win_end_h;	//=dpss_afbcd->win_end_h;
	u32 win_bgn_v = dpss_afbcd->win_bgn_v;	//=dpss_afbcd->win_bgn_v;
	u32 win_end_v = dpss_afbcd->win_end_v;	//=dpss_afbcd->win_end_v;
	u32 pip_src_mode = dpss_afbcd->pip_src_mode;	//=dpss_afbcd->out_reg_pip_mode;
	u32 rot_vshrk = dpss_afbcd->rot_vshrk;	//=0;
	u32 rot_hshrk = dpss_afbcd->rot_hshrk;	//=0;
	u32 rot_drop_mode = dpss_afbcd->rot_drop_mode;	//=2;
	u32 rot_ofmt_mode = dpss_afbcd->rot_ofmt_mode;
	u32 rot_ocompbit = dpss_afbcd->rot_ocompbit;
	u32 fix_cr_en = dpss_afbcd->fix_cr_en;	// 0;//dpss_afbcd->out_ofset_brst4_en;
	u32 brst_len_add_en = dpss_afbcd->brst_len_add_en;	// 1 bits
	u32 brst_len_add_value = dpss_afbcd->brst_len_add_value;	// 3 bits
	u32 ofset_brst4_en = dpss_afbcd->ofset_brst4_en;	// 1 bits

	frc_cfg_dpss_afbcd_mult(index,	// baddr_ofst
			2,	// hold_line_num   , // 7 bits
			0,	// blk_mem_mode    ,
			compbits,	// compbits_y      , // 2 bits 0-8bits 1-9bits 2-10bits
			compbits,	// compbits_u      , // 2 bits 0-8bits 1-9bits 2-10bits
			compbits,	// compbits_v      , // 2 bits 0-8bits 1-9bits 2-10bits
			hsize_in,	// hsize_in        , //13 bits dec source pic hsize
			vsize_in,	// vsize_in        , //13 bits dec source pic vsize
			mif_info_baddr,	// mif_info_baddr  , //32 bits
			mif_data_baddr,	// mif_data_baddr  , //32 bits
			win_bgn_h,	// out_horz_bgn    , //13 bits
			win_bgn_v,	// out_vert_bgn    , //13 bits
			win_end_h,	// out_horz_end    , //13 bits
			win_end_v,	// out_vert_end    , //13 bits
			0,	// hz_ini_phase    , // 4 bits
			0,	// vt_ini_phase    , // 4 bits
			0,	// hz_rpt_fst0_en  , // 1 bits
			0,	// vt_rpt_fst0_en  , // 1 bits
			rev_mode,	// rev_mode        ,
			y_v_skip_en,	// vert_skip_y     ,
			y_h_skip_en,	// horz_skip_y     ,
			c_v_skip_en,	// vert_skip_uv    ,
			c_h_skip_en,	// horz_skip_uv    ,
			hfmt_en, vfmt_en, def_color_y,	// def_color_y     10 bits
			def_color_u,	// def_color_u     10 bits
			def_color_v,	// def_color_v     10 bits
			lossy_en,	// reg_lossy_en        2 bit
			ddr_sz_mode,	// ddr_sz_mode         1 bit  1:mmu mode
			fmt_mode,	// fmt_mode            2 bit  0:444 1:422 2:420
			reg_fmt444_comb,	// reg_fmt444_comb
			reg_dos_uncomp_mode,	// reg_dos_uncomp_mode 1 bit
			pip_src_mode,	// pip_src_mode  1
			rot_ofmt_mode,	// rot_ofmt_mode 2 bit  0:444 1:422 2:420
			rot_ocompbit,	// rot_ocompbit
			rot_drop_mode,	// rot_drop_mode
			rot_vshrk,	// rot_vshrk
			rot_hshrk,	// rot_hshrk
			rot_hbgn,	// rot_hbgn          5 bits
			rot_vbgn,	// rot_vbgn          2 bits
			rot_en,	// rot_enable        1 bit
			fix_cr_en,	// fix_cr_en          1 bits
			brst_len_add_en,	// brst_len_add_en    1 bits
			brst_len_add_value,	// brst_len_add_value 3 bits
			ofset_brst4_en	// ofset_brst4_en     1 bits
		);
}

void frc_cfg_dpss_afbcd_mult(u32 module_sel, u32 hold_line_num,
	u32 blk_mem_mode, u32 compbits_y, u32 compbits_u, u32 compbits_v, u32 hsize_in,
	u32 vsize_in, u32 mif_info_baddr, u32 mif_data_baddr,
	u32 out_horz_bgn, u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 hz_ini_phase, u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en,
	u32 rev_mode, u32 vert_skip_y, u32 horz_skip_y, u32 vert_skip_uv,
	u32 horz_skip_uv, u32 hfmt_en, u32 vfmt_en, u32 def_color_y,
	u32 def_color_u, u32 def_color_v, u32 reg_lossy_en, u32 ddr_sz_mode,
	u32 fmt_mode, u32 reg_fmt444_comb, u32 reg_dos_uncomp_mode,
	u32 pip_src_mode, u32 rot_ofmt_mode, u32 rot_ocompbit,
	u32 rot_drop_mode, u32 rot_vshrk, u32 rot_hshrk,
	u32 rot_hbgn, u32 rot_vbgn, u32 rot_en, u32 fix_cr_en, u32 brst_len_add_en,
	u32 brst_len_add_value, u32 ofset_brst4_en)
{
	u32 compbits_yuv;
	u32 conv_lbuf_len;	//12 bits

	s32 compbits_eq8;
	s32 use_4kram = 0;
	//ary no use    s32 real_hsize_mt2k;
	//ary no use    s32 comp_mt_20bit ;
	s32 regs_ofst;

	regs_ofst = module_sel * 256;	//ary addr change *4
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_MMU_CTRL, 0, 0, 1);

	compbits_yuv = ((compbits_v & 0x3) << 4) |
		((compbits_u & 0x3) << 2) | ((compbits_y & 0x3) << 0);

	compbits_eq8 = (compbits_y == 0 && compbits_u == 0 && compbits_v == 0);

	u32 tfmt_ram_ofst0;	//todo
	u32 tfmt_ram_ofst1;	//todo
	u32 tfmt_ram_ofst2;	//todo
	u32 mif_lbuf_depth;	//todo
	u32 head_ram_ofst;	//todo
	u32 body_ram_ofst;	//todo

	if (module_sel <= 3) {
		mif_lbuf_depth = 256;
		head_ram_ofst = 48;
		body_ram_ofst = 128;
	} else if (module_sel > 5) {
		mif_lbuf_depth = 256;
		head_ram_ofst = 128;
		body_ram_ofst = 64;
	} else {
		mif_lbuf_depth = 256;
		head_ram_ofst = 128;
		body_ram_ofst = 128;
	}			//4096

	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFRC1_PARAM, head_ram_ofst & 0x7ff, 21,
		     11);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, body_ram_ofst & 0x3ff, 10, 10);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, 1, 31, 1);	//reg_vfcd_enable

	if (module_sel <= 3) {
		tfmt_ram_ofst0 = 168;
		tfmt_ram_ofst1 = 336;
	} else if (module_sel > 5) {
		tfmt_ram_ofst0 = 256;
		tfmt_ram_ofst1 = 512;
	} else {
		tfmt_ram_ofst0 = 512;
		tfmt_ram_ofst1 = 1024;
	}

	if (fmt_mode == 2 && vfmt_en) {	//bc_420_fmtup need 1line_lbuf
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
		tfmt_ram_ofst1 = tfmt_ram_ofst1 * 3 / 4;
	} else {
		//ary same ? tfmt_ram_ofst1 = tfmt_ram_ofst1 ;
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
	}

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CONV_BUF_LENS),
		((tfmt_ram_ofst1 & 0xfff) << 16) | ((tfmt_ram_ofst0 & 0xfff) << 0));
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_LBUF_DEPTH),
		((tfmt_ram_ofst2 & 0xfff) << 16) | ((mif_lbuf_depth & 0xfff) << 0));

	if ((hsize_in > 1024 && fmt_mode == 0 && compbits_eq8 == 0) ||
		hsize_in > 2048 || use_4kram)
		conv_lbuf_len = 256;
	else
		conv_lbuf_len = 128;

	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_AFBC_IQUANT_ENABLE),
		reg_lossy_en & 1, 0, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_AFBC_IQUANT_ENABLE),
		(reg_lossy_en >> 1) & 1, 4, 1);

	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_LUMA_PIC_XPOS), out_horz_bgn, 16, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_LUMA_PIC_XPOS), out_horz_end, 0, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_LUMA_PIC_YPOS), out_vert_bgn, 16, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_LUMA_PIC_YPOS), out_vert_end, 0, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_FRM_PIC_SIZE), hsize_in, 16, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_FRM_PIC_SIZE), vsize_in, 0, 13);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_BURST_CTRL),
		((ofset_brst4_en & 0x1) << 4) |
		((brst_len_add_en & 0x1) << 3) |
		((brst_len_add_value & 0x7) << 0));

	hold_line_num = hold_line_num > 4 ? hold_line_num - 4 : 0;
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, compbits_yuv, 4, 6);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_MODE),
		((1) << 30) |	// afbc_emit_mode 0:16DW ,1:64DW
		((ddr_sz_mode & 0x1) << 29) |	// ddr_sz_mode
		((blk_mem_mode & 0x1) << 28) |	// blk_mem_mode
		((rev_mode & 0x3) << 26) |	// rev_mode
		((3) << 24) |	// mif_urgent
		((hold_line_num & 0x7f) << 16) |	// hold_line_num
		((3) << 14) |	// burst_len  //burst8
		//((compbits_yuv & 0x3f)<< 8 ) |  // compbits_yuv
		((vert_skip_y & 0x3) << 6) |	// vert_skip_y
		((horz_skip_y & 0x3) << 4) |	// horz_skip_y
		((vert_skip_uv & 0x3) << 2) |	// vert_skip_uv
		((horz_skip_uv & 0x3) << 0)	// horz_skip_uv
		);

	//wr((regs_ofst+VFCD_AFBC_SIZE_IN),
	//    ((hsize_in & 0x1fff )<< 16) |  // hsize_in
	//    ((vsize_in & 0x1fff )<< 0 )    // vsize_in
	//    );
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_DEC_DEF_COLOR),
		((def_color_u & 0xfff) << 16) | ((def_color_v & 0xfff) << 0));

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_CONV_CTRL),
		((def_color_y & 0xfff) << 16) |
		((fmt_mode & 0x3) << 12) |
		((conv_lbuf_len & 0xfff) << 0));

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_HEAD_BADDR), mif_info_baddr);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_BODY_BADDR), mif_data_baddr);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_ENABLE), ((2 & 0x3) << 29) |
		((3 & 0x3) << 21) |
		((reg_fmt444_comb & 0x1) << 20) |	//reg_fmt444_comb
		((reg_dos_uncomp_mode & 0x1) << 19) |	//reg_dos_uncomp_mode
		((0x01701 & 0x7ffff) << 0)	//ddr_blk_size ,cmd_blk_size , lathc_mux
		);
}				//cfg_dpss_afbcd_mult

void frc_cfg_vfcd_fmtup(u32 cmpr_sel, u32 regs_ofst, u32 fmt_mode, u32 hfmt_en,
	u32 vfmt_en, u32 hfmt_mode, u32 vfmt_mode,
	u32 horz_skip_y, u32 vert_skip_y, u32 horz_skip_uv,
	u32 vert_skip_uv, u32 rev_mode_h, u32 rev_mode_v, u32 out_horz_bgn,
	u32 out_vert_bgn, u32 out_horz_end, u32 out_vert_end,
	u32 pad_en, u32 pad_hmode, u32 pad_vmode, u32 hz_ini_phase,
	u32 vt_ini_phase, u32 hz_rpt_fst0_en, u32 vt_rpt_fst0_en)
{
	u32 cu_hdw = cmpr_sel == 2 ? 4 : cmpr_sel == 1 ? 5 : 0;
	u32 cu_vdw = 2;
	u32 vt_yc_ratio;
	u32 hz_yc_ratio;
	u32 cvfmt_h;
	u32 chfmt_w;
	u32 vfmt_w;

	u32 mif_algblk_bgn_h = (out_horz_bgn >> cu_hdw) << cu_hdw;
	u32 mif_algblk_bgn_v = (out_vert_bgn >> cu_vdw) << cu_vdw;
	u32 mif_algblk_end_h = ((out_horz_end >> cu_hdw) << cu_hdw) + ((1 << cu_hdw) - 1);
	u32 mif_algblk_end_v = ((out_vert_end >> cu_vdw) << cu_vdw) + ((1 << cu_vdw) - 1);

	u32 out_end_dif_h = out_horz_end - out_horz_bgn;
	u32 out_end_dif_v = out_vert_end - out_vert_bgn;

	u32 dec_pixel_bgn_h = (out_horz_bgn & ((1 << cu_hdw) - 1));
	u32 dec_pixel_bgn_v = (out_vert_bgn & ((1 << cu_vdw) - 1));
	u32 dec_pixel_end_h = (dec_pixel_bgn_h + out_end_dif_h);
	u32 dec_pixel_end_v = (dec_pixel_bgn_v + out_end_dif_v);

	dec_pixel_bgn_h =
		rev_mode_h ? mif_algblk_end_h - dec_pixel_end_h : dec_pixel_bgn_h;
	dec_pixel_bgn_v =
		rev_mode_v ? mif_algblk_end_v - dec_pixel_end_v : dec_pixel_bgn_v;
	dec_pixel_end_h =
		rev_mode_h ? mif_algblk_end_h - dec_pixel_bgn_h : dec_pixel_end_h;
	dec_pixel_end_v =
		rev_mode_v ? mif_algblk_end_v - dec_pixel_bgn_v : dec_pixel_end_v;

	u32 fmt_size_h = mif_algblk_end_h - mif_algblk_bgn_h + 1;
	u32 fmt_size_v = mif_algblk_end_v - mif_algblk_bgn_v + 1;

	fmt_size_h = horz_skip_y != 0 ? (fmt_size_h >> 1) : fmt_size_h;
	fmt_size_v = vert_skip_y != 0 ? (fmt_size_v >> 1) : fmt_size_v;

	//dbg_h2("===fmt_size_h = %0d\n",fmt_size_h );
	//dbg_h2("===fmt_size_v = %0d\n",fmt_size_v );

	if (fmt_mode == 2) {	//420
		if (vert_skip_uv == 0 && vert_skip_y == 0)
			vt_yc_ratio = 2;
		else if (vert_skip_uv == 0 && vert_skip_y != 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y != 0)
			vt_yc_ratio = 2;
		else
			vt_yc_ratio = 0;	//cfg_error
	} else {		//422 or 444
		if (vert_skip_uv == 0 && vert_skip_y == 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y != 0)
			vt_yc_ratio = 1;
		else if (vert_skip_uv != 0 && vert_skip_y == 0)
			vt_yc_ratio = 2;
		else
			vt_yc_ratio = 0;	//cfg_error
	}

	if (fmt_mode != 0) {	//420 or 422
		if (horz_skip_uv == 0 && horz_skip_y == 0)
			hz_yc_ratio = 2;
		else if (horz_skip_uv == 0 && horz_skip_y != 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y != 0)
			hz_yc_ratio = 2;
		else
			hz_yc_ratio = 0;	//cfg_error

	} else {		//444
		if (horz_skip_uv == 0 && horz_skip_y == 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y != 0)
			hz_yc_ratio = 1;
		else if (horz_skip_uv != 0 && horz_skip_y == 0)
			hz_yc_ratio = 2;
		else
			hz_yc_ratio = 0;	//cfg_error
	}

	//FMT_IN
	cvfmt_h = vt_yc_ratio == 0 ? 0 : (fmt_size_v >> (vt_yc_ratio - 1));
	vfmt_w = hz_yc_ratio == 0 ? 0 : (fmt_size_h >> (hz_yc_ratio - 1));
	//FMT_OUT
	chfmt_w = hfmt_en ? vfmt_w * 2 : vfmt_w;	//fmt_out

	//dbg_h2("===cvfmt_h = %0d\n",cvfmt_h );
	//dbg_h2("===cvfmt_w = %0d\n",vfmt_w  );
	//dbg_h2("===chfmt_w = %0d\n",chfmt_w );

	//u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vt_phase_step = 8;

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_VD_CFMT_CTRL), (hfmt_mode << 28) |
		(hz_ini_phase << 24) |	//hz ini phase
		(hz_rpt_fst0_en << 23) |	//repeat p0 enable
		((hz_yc_ratio - 1) << 21) |	//hz yc ratio
		(hfmt_en << 20) |	//hz enable
		(vfmt_mode << 19) |	//cvfmt_always_en
		(0 << 18) |	//cvfmt_rpt_lst_line_disable
		(1 << 17) |	//nrpt_phase0 enable
		(vt_rpt_fst0_en << 16) |	//repeat l0 enable
		(0 << 12) |	//skip line num
		(vt_ini_phase << 8) |	//vt ini phase
		(vt_phase_step << 1) |	//vt phase step (3.4)
		(vfmt_en << 0)	//vt enable
		);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_VD_CFMT_W), (chfmt_w << 16) |
		(vfmt_w << 0)	//vt format width
		);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_LUMA_SIZE), (fmt_size_v & 0x1fff) << 16 |
		(fmt_size_h & 0x1fff));
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_AFBC_VD_CFMT_H), cvfmt_h & 0x1fff, 0,
			13);

	vt_yc_ratio = vfmt_en ? vt_yc_ratio / 2 : vt_yc_ratio;
	hz_yc_ratio = hfmt_en ? hz_yc_ratio / 2 : hz_yc_ratio;
	//dbg_h2("===vt_yc_ratio= %0x\n",vt_yc_ratio );
	//dbg_h2("===hz_yc_ratio= %0x\n",hz_yc_ratio );
	//dbg_h2("===hfmt_en    = %0x\n",hfmt_en   );
	//dbg_h2("===vfmt_en    = %0x\n",vfmt_en   );
	//dbg_h2("===horz_skip_y = %0x\n",horz_skip_y );
	//dbg_h2("===vert_skip_y = %0x\n",vert_skip_y );
	//dbg_h2("===horz_skip_uv= %0x\n",horz_skip_uv);
	//dbg_h2("===vert_skip_uv= %0x\n",vert_skip_uv);

	dec_pixel_bgn_h = horz_skip_y ? dec_pixel_bgn_h >> 1 : dec_pixel_bgn_h;
	dec_pixel_bgn_v = vert_skip_y ? dec_pixel_bgn_v >> 1 : dec_pixel_bgn_v;
	dec_pixel_end_h = horz_skip_y ? dec_pixel_end_h >> 1 : dec_pixel_end_h;
	dec_pixel_end_v = vert_skip_y ? dec_pixel_end_v >> 1 : dec_pixel_end_v;

	u32 dec_pixel_uv_bgn_h =
		hz_yc_ratio == 2 ? dec_pixel_bgn_h >> 1 : dec_pixel_bgn_h;
	u32 dec_pixel_uv_bgn_v =
		vt_yc_ratio == 2 ? dec_pixel_bgn_v >> 1 : dec_pixel_bgn_v;
	u32 dec_pixel_uv_end_h =
		hz_yc_ratio == 2 ? dec_pixel_end_h >> 1 : dec_pixel_end_h;
	u32 dec_pixel_uv_end_v =
		vt_yc_ratio == 2 ? dec_pixel_end_v >> 1 : dec_pixel_end_v;

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_LUMA_WIN_H),
		dec_pixel_bgn_h << 16 | dec_pixel_end_h);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_CHRM_WIN_H),
		dec_pixel_uv_bgn_h << 16 | dec_pixel_uv_end_h);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_LUMA_WIN_V),
		dec_pixel_bgn_v << 16 | dec_pixel_end_v);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_CHRM_WIN_V),
		dec_pixel_uv_bgn_v << 16 | dec_pixel_uv_end_v);

	u32 win_yout_hsize = dec_pixel_end_h - dec_pixel_bgn_h + 1;
	u32 win_yout_vsize = dec_pixel_end_v - dec_pixel_bgn_v + 1;
	u32 win_uvout_hsize = dec_pixel_uv_end_h - dec_pixel_uv_bgn_h + 1;
	u32 win_uvout_vsize = dec_pixel_uv_end_v - dec_pixel_uv_bgn_v + 1;

	//u32 pad_en          = 0;
	u32 pad_ydout_hofst = 0;
	u32 pad_ydout_vofst = 0;
	u32 pad_ydout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_yout_hsize + 7) / 8 * 8 : (win_yout_hsize +
			15) / 16 * 16) : win_yout_hsize;
	u32 pad_ydout_vsize =
		pad_en ? (pad_vmode ==
			0 ? (win_yout_vsize + 7) / 8 * 8 :
			(win_yout_vsize + 15) / 16 * 16) : win_yout_vsize;
	u32 pad_cdout_hofst = 0;
	u32 pad_cdout_vofst = 0;
	u32 pad_cdout_hsize =
		pad_en ? (pad_hmode ==
			0 ? (win_uvout_hsize + 7) / 8 * 8 :
			(win_uvout_hsize + 5) / 16 * 16) : win_uvout_hsize;
	u32 pad_cdout_vsize =
		pad_en ? (pad_vmode ==
			0 ? (win_uvout_vsize + 7) / 8 * 8 :
			(win_uvout_vsize + 15) / 16 * 16) : win_uvout_vsize;
	u32 index = regs_ofst / (256 * 4);

	if (dpss_dbg_0709 & C_BIT1) {
		dbg_h2("vfcd index is%0x, pad_ydout_vsize=0x%x\n", index, pad_ydout_vsize);
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PAD_SIZE),
			pad_en << 29 | ((pad_ydout_vsize & 0x7ff) << 16) |
			(pad_ydout_hsize & 0x7ff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PAD_OFST),
			pad_ydout_vofst << 16 | pad_ydout_hofst);

		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PAD_SIZE),
			pad_cdout_vsize << 16 | pad_cdout_hsize);
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PAD_OFST),
			pad_cdout_vofst << 16 | pad_cdout_hofst);
	} else {
		dbg_h2("disable vfcd_mc_wr\n");
	}

	switch (index) {
	case 0:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12bit  {luma_data,chrm_data}
	case 1:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 2:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 3:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 4:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	case 5:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 6:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	default:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;
	}
}

void frc_cfg_vfcd_afrcd(u32 src_sel, struct AFRCD_TYPE *afrcd)
{
	u32 reg_ofst = src_sel;

//declare
	u32 luma_pixel_format = 0;
//ary set no use        u32 luma_raw_last_th   = 0;
	u32 luma_max_ac_depth = 0;
	u32 luma_last_extend_en = 0;

	u32 chrm_pixel_format = 0;
//ary set no use        u32 chrm_raw_last_th   = 0;
	u32 chrm_max_ac_depth = 0;
	u32 chrm_last_extend_en = 0;
	u32 pixel_is_diff_chn = 0;

	u32 afrc_raw_en = afrcd->input_fmt_mode == 2;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (afrcd->chrm_comp_target < 16 || afrcd->chrm_comp_target > 48)
		dbg_h2("rtl not support this target\n");

	afrcd->chrm_cu_bits = (afrcd->chrm_comp_target - 16) / 4;
	afrcd->luma_cu_bits = (afrcd->luma_comp_target - 16) / 4;
	afrcd->chrm_pixel_bits = afrcd->compbits_chrm;
	afrcd->luma_pixel_bits = afrcd->compbits_luma;

////luma
	if (afrcd->input_fmt_mode == 0) {	//rgb
		luma_pixel_format = 2;
		chrm_pixel_format = 2;
		pixel_is_diff_chn = 1;
	} else if (afrcd->input_fmt_mode == 1) {	//yuv
		luma_pixel_format = 5;
		chrm_pixel_format = 5;
		pixel_is_diff_chn = 0;
	} else if (afrcd->raw_8x16_en) {	//raw_8x16_for_npu
		luma_pixel_format = 8;
		chrm_pixel_format = 8;
		pixel_is_diff_chn = 0;
	} else if ((afrcd->input_fmt_mode == 2) && (afrcd->input_bayer_mode == 0)) {
		luma_pixel_format = 6;
		chrm_pixel_format = 6;
		pixel_is_diff_chn = 0;
	} else if ((afrcd->input_fmt_mode == 2) && (afrcd->input_bayer_mode == 1)) {
		luma_pixel_format = 7;
		chrm_pixel_format = 7;
		pixel_is_diff_chn = 1;
	} else {
		dbg_h2("rtl not support reg_input_fmt_mode\n");
	}

	if (afrcd->compbits_luma <= 10) {
		//ary set no use   luma_raw_last_th           = 9;
		luma_max_ac_depth = 9;
		luma_last_extend_en = 0;
	} else if (afrcd->compbits_luma <= 12) {
//ary set no use    luma_raw_last_th           = 12;
		luma_max_ac_depth = 12;
		luma_last_extend_en = 1;
	} else {
		dbg_h2("rtl not support reg_compbits_y\n");
	}

//chrm
	if (afrcd->compbits_chrm <= 10) {
//ary set no use    chrm_raw_last_th           = 9;
		chrm_max_ac_depth = 9;
		chrm_last_extend_en = 0;
	} else if (afrcd->compbits_chrm <= 12) {
//ary set no use    chrm_raw_last_th           = 12;
		chrm_max_ac_depth = 12;
		chrm_last_extend_en = 1;
	} else {
		dbg_h2("rtl not support reg_compbits_c\n");
	}
	u32 mmu_mode_en = afrcd->luma_header_en ? 0 : afrcd->mmu_mode_en;

	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_MIF1_URGENT, afrc_raw_en, 25, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_MMU_BADDR0, afrcd->mmu_baddr0, 0, 32);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_MMU_BADDR1, afrcd->mmu_baddr1, 0, 32);
	VSYNC_WR_VIDEO_TABLE_REG(reg_ofst + VFCD_MMU_CTRL, ((4 & 0x1f) << 2 |
					 (afrcd->mmu_page_mode & 0x1) << 1 |
					 (mmu_mode_en & 0x1)));
//for luma
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, luma_pixel_format, 0, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, afrcd->luma_dict_en, 4, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, 1, 12, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, luma_last_extend_en, 16, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, luma_max_ac_depth, 20, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, pixel_is_diff_chn, 24, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL0, afrcd->input_bayer_mode, 25, 2);
	// VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFBC_HEAD_BADDR,
	// afrcd->luma_head_addr, 0, 32);
	// VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFBC_BODY_BADDR,
	// afrcd->luma_body_addr, 0, 32);

	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_header_en, 4, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_cu_bits, 8, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC0_CTRL, afrcd->luma_pixel_bits, 20, 4);
	if (pchip_st && pchip_st->chip == ID_T6X)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC0_CTRL, 1, 30, 1);

//for chrm
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, chrm_pixel_format, 0, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, afrcd->chrm_dict_en, 4, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, 1, 12, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, chrm_last_extend_en, 16, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, chrm_max_ac_depth, 20, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, pixel_is_diff_chn, 24, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CTRL1, afrcd->input_bayer_mode, 25, 2);

	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_header_en, 4, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_cu_bits, 8, 4);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC1_CTRL, afrcd->chrm_pixel_bits, 20, 4);
	if (pchip_st && pchip_st->chip == ID_T6X)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC1_CTRL, 1, 30, 1);
	// VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CHRM_HEAD_BADDR,
	// afrcd->chrm_head_addr, 0, 32);
	// VSYNC_WR_VIDEO_TABLE_REG_BITS(reg_ofst + VFCD_AFRC_CHRM_BODY_BADDR,
	// afrcd->chrm_body_addr, 0, 32);
}

void frc_cfg_vfcd_dec(u32 index, struct VFCD_t *vfcd)
{
	u32 cmpr_sel = vfcd->cmpr_sel;
	u32 fmt444_out = vfcd->fmt444_out;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	dbg_h2("%s:[%d] fmt444_out = %d\n", __func__, index, fmt444_out);
	u32 regs_ofst = index * 256;	//ary addr change *4
	// switch(index){
	//     case 0 : regs_ofst = index*256*4;break;
	//     case 1 : regs_ofst = index*256*4;break;
	//     case 2 : regs_ofst = index*256*4;break;
	//     case 3 : regs_ofst = index*256*4;break;
	//     case 4 : regs_ofst = index*256*4;break;
	//     case 5 : regs_ofst = index*256*4;break;
	//     default: regs_ofst = index*256*4;break;
	// }
	u32 hold_line_num = 4;

	if (pchip_st && (pchip_st->chip == ID_T6X ||
		pchip_st->chip == ID_T6W) &&
		(regs_ofst == 0x400 || regs_ofst == 0x500)) {
		hold_line_num = 6; //8
		if (pchip_st->dbg_vfcd_holdline)
			hold_line_num = pchip_st->dbg_vfcd_holdline;
	}
	//ary no use    u32 flim_grain_src_idx = 0;
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_TOP_CTRL2), 1, 31, 1);	//reg_vfcd_enable
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_TOP_CTRL2), vfcd->yc_rst_sep, 30, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_TOP_CTRL0), vfcd->slc_num, 24, 3);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_TOP_CTRL0), hold_line_num, 4, 12);
	VSYNC_WR_VIDEO_TABLE_REG_BITS((regs_ofst + VFCD_TOP_CTRL0), vfcd->frm_en_sel, 2, 2);
	if (vfcd->cmpr_sel && regs_ofst == 0)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(VFCD_TOP_CTRL0, 1, 27, 2);	//patch-0404

	if (vfcd->cut_win_en == 1)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 2, 27, 2);
	else if (regs_ofst == 0x400)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 0, 27, 2);

	u32 fmt_mode = (cmpr_sel == 2 && vfcd->afrcd.input_fmt_mode == 2) ? 1 : vfcd->src_fmt;
	u32 horz_skip_y = vfcd->y_h_skip_en;
	u32 vert_skip_y = vfcd->y_v_skip_en;
	u32 horz_skip_uv = vfcd->c_h_skip_en;
	u32 vert_skip_uv = vfcd->c_v_skip_en;
	u32 rev_mode = vfcd->rev_mode;
	u32 hfmt_en	  = (fmt444_out == 1) ? (vfcd->src_fmt != 0) : 0;
	u32 vfmt_en	  = (fmt444_out != 0) ? (vfcd->src_fmt == 2) : 0;
	u32 cmpb_y_map = vfcd->compbits_y == 12 ? 3 : vfcd->compbits_y - 8;
	u32 cmpb_c_map = vfcd->compbits_c == 12 ? 3 : vfcd->compbits_c - 8;

	u32 compbits_v = cmpb_c_map;
	u32 compbits_u = cmpb_c_map;
	u32 compbits_y = cmpb_y_map;

	u32 chrm_win_bgn_h_0 =
		fmt_mode != 0 ? vfcd->win_bgn_h[0] >> 1 : vfcd->win_bgn_h[0];
	u32 chrm_win_end_h_0 =
		fmt_mode != 0 ? vfcd->win_end_h[0] >> 1 : vfcd->win_end_h[0];
	u32 chrm_win_bgn_v_0 =
		fmt_mode == 2 ? vfcd->win_bgn_v[0] >> 1 : vfcd->win_bgn_v[0];
	u32 chrm_win_end_v_0 =
		fmt_mode == 2 ? vfcd->win_end_v[0] >> 1 : vfcd->win_end_v[0];

	u32 chrm_win_bgn_h_1 =
		fmt_mode != 0 ? vfcd->win_bgn_h[1] >> 1 : vfcd->win_bgn_h[1];
	u32 chrm_win_end_h_1 =
		fmt_mode != 0 ? vfcd->win_end_h[1] >> 1 : vfcd->win_end_h[1];
	u32 chrm_win_bgn_h_2 =
		fmt_mode != 0 ? vfcd->win_bgn_h[2] >> 1 : vfcd->win_bgn_h[2];
	u32 chrm_win_end_h_2 =
		fmt_mode != 0 ? vfcd->win_end_h[2] >> 1 : vfcd->win_end_h[2];
	u32 chrm_win_bgn_h_3 =
		fmt_mode != 0 ? vfcd->win_bgn_h[3] >> 1 : vfcd->win_bgn_h[3];
	u32 chrm_win_end_h_3 =
		fmt_mode != 0 ? vfcd->win_end_h[3] >> 1 : vfcd->win_end_h[3];

	vfcd->afbcd.hsize = vfcd->src_hsize;
	vfcd->afbcd.vsize = vfcd->src_vsize;
	vfcd->afbcd.compbits = compbits_y;
	vfcd->afbcd.head_baddr = vfcd->luma_head_baddr;
	vfcd->afbcd.body_baddr = vfcd->luma_body_baddr;
	vfcd->afbcd.fmt_mode = fmt_mode;

	if (vfcd->compbits_y == 12)
		vfcd->afbcd.def_color_y = 0xfff;
	else if (vfcd->compbits_y == 10)
		vfcd->afbcd.def_color_y = 0x3ff;
	else
		vfcd->afbcd.def_color_y = 0x0ff;
	vfcd->afbcd.def_color_u = 0x80 << (vfcd->compbits_c - 8);
	vfcd->afbcd.def_color_v = 0x80 << (vfcd->compbits_c - 8);
	vfcd->afbcd.win_bgn_h = vfcd->win_bgn_h[0];
	vfcd->afbcd.win_end_h = vfcd->win_end_h[0];
	vfcd->afbcd.win_bgn_v = vfcd->win_bgn_v[0];
	vfcd->afbcd.win_end_v = vfcd->win_end_v[0];
	vfcd->afbcd.y_h_skip_en = vfcd->y_h_skip_en;
	vfcd->afbcd.y_v_skip_en = vfcd->y_v_skip_en;
	vfcd->afbcd.c_h_skip_en = vfcd->c_h_skip_en;
	vfcd->afbcd.c_v_skip_en = vfcd->c_v_skip_en;
	vfcd->afbcd.rev_mode = vfcd->rev_mode;

	vfcd->afrcd.input_fmt_mode = 1;	//1:yuv
	vfcd->afrcd.input_bayer_mode = 0;
	vfcd->afrcd.luma_head_addr = vfcd->luma_head_baddr;
	vfcd->afrcd.luma_body_addr = vfcd->luma_body_baddr;
	//vfcd->afrcd.luma_dict_en = 0;
	vfcd->afrcd.compbits_luma = vfcd->compbits_c;

	vfcd->afrcd.chrm_head_addr = vfcd->chrm_head_baddr;
	vfcd->afrcd.chrm_body_addr = vfcd->chrm_body_baddr;

	dbg_h2("vfcd->luma_head_baddr:%x vfcd->luma_body_baddr:%x\n",
		vfcd->luma_head_baddr, vfcd->luma_body_baddr);
	dbg_h2("vfcd->chrm_head_baddr:%x vfcd->chrm_body_baddr:%x\n",
		vfcd->chrm_head_baddr, vfcd->chrm_body_baddr);
	//vfcd->afrcd.chrm_dict_en = 0;
	vfcd->afrcd.mmu_baddr0 = vfcd->mmu_baddr0;
	vfcd->afrcd.mmu_baddr1 = vfcd->mmu_baddr1;

	u32 compbits_yuv = ((compbits_v & 0x3) << 4) |
		((compbits_u & 0x3) << 2) | ((compbits_y & 0x3) << 0);
	vfcd->afrcd.compbits_chrm = vfcd->compbits_y;
	vfcd->afrcd.mmu_mode_en = vfcd->mmu_mode_en;
	vfcd->afrcd.mmu_page_mode = vfcd->mmu_page_mode;

	u32 mif_lbuf_depth;
	u32 head_ram_ofst;
	u32 body_ram_ofst;
	u32 tfmt_ram_ofst0;
	u32 tfmt_ram_ofst1;
	u32 tfmt_ram_ofst2;

	if (index <= 3) {
		mif_lbuf_depth = 128;
		head_ram_ofst = 48;
		body_ram_ofst = 128;
	} else if (index > 5) {
		mif_lbuf_depth = 128;
		head_ram_ofst = 64;
		body_ram_ofst = 128;
	} else {
		if (pchip_st && pchip_st->chip == ID_T6X) {
			mif_lbuf_depth = 64;
			head_ram_ofst = 64;
			body_ram_ofst = 64;
		} else {
			mif_lbuf_depth = 128;
			head_ram_ofst = 128;
			body_ram_ofst = 128;
		}
	}			//4096

	if (cmpr_sel == 1)
		mif_lbuf_depth = mif_lbuf_depth * 2;

	if (index <= 3) {
		tfmt_ram_ofst0 = 168;
		tfmt_ram_ofst1 = 336;
	} else if (index > 5) {
		tfmt_ram_ofst0 = 256;
		tfmt_ram_ofst1 = 512;
	} else {
		tfmt_ram_ofst0 = 512;
		tfmt_ram_ofst1 = 1024;
	}			//4096

	if (cmpr_sel == 1 && fmt_mode == 2 && vfmt_en) {
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
		tfmt_ram_ofst1 = tfmt_ram_ofst1 * 3 / 4;
	} else {
		//ary same?        tfmt_ram_ofst1 = tfmt_ram_ofst1 ;
		tfmt_ram_ofst2 = tfmt_ram_ofst1;
	}
	if (cmpr_sel == 1 || cmpr_sel == 2) {
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFRC1_PARAM,
			head_ram_ofst & 0x7ff, 21, 11);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, body_ram_ofst & 0x3ff,
			10, 10);

		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CONV_BUF_LENS),
			((tfmt_ram_ofst1 & 0xfff) << 16) |
			((tfmt_ram_ofst0 & 0xfff) << 0));

		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_LBUF_DEPTH),
			((tfmt_ram_ofst2 & 0xfff) << 16) |
			((mif_lbuf_depth & 0xfff) << 0));

		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, compbits_yuv, 4, 6);
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_FRM_PIC_SIZE),
			(vfcd->src_hsize << 16) | (vfcd->src_vsize & 0xffff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[0] << 16) | (vfcd->win_end_h[0] & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PIC_YPOS),
			(vfcd->win_bgn_v[0] << 16) | (vfcd->win_end_v[0] & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_0 << 16) | (chrm_win_end_h_0 & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PIC_YPOS),
			(chrm_win_bgn_v_0 << 16) | (chrm_win_end_v_0 & 0x1fff));

		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC1_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[1] << 16) | (vfcd->win_end_h[1] & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC1_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_1 << 16) | (chrm_win_end_h_1 & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC2_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[2] << 16) | (vfcd->win_end_h[2] & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC2_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_2 << 16) | (chrm_win_end_h_2 & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC3_LUMA_PIC_XPOS),
			(vfcd->win_bgn_h[3] << 16) | (vfcd->win_end_h[3] & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_SLC3_CHRM_PIC_XPOS),
			(chrm_win_bgn_h_3 << 16) | (chrm_win_end_h_3 & 0x1fff));
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_MODE), ((1) << 30) |
			((1 & 0x1) << 29) |	// ddr_sz_mode
			((0 & 0x1) << 28) |	// blk_mem_mode
			((rev_mode & 0x3) << 26) |	// rev_mode
			((3) << 24) |	// mif_urgent
			((4 & 0x7f) << 16) |	// hold_line_num
			((vfcd->mif_burst_len) << 14) |
			((vert_skip_y & 0x3) << 6) |	// vert_skip_y
			((horz_skip_y & 0x3) << 4) |	// horz_skip_y
			((vert_skip_uv & 0x3) << 2) |	// vert_skip_uv
			((horz_skip_uv & 0x3) << 0)	// horz_skip_uv
			);
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_CONV_CTRL),
			((fmt_mode & 0x3) << 12));
		//VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst+VFCD_AFBC_MODE,vfcd->mif_burst_len,14,2);
	}

	if (cmpr_sel == 1) {	//afbc
		frc_cfg_dpss_afbcd(index, hfmt_en, vfmt_en, &vfcd->afbcd);
		frc_cfg_vfcd_fmtup(2, regs_ofst, fmt_mode, hfmt_en, vfmt_en, 1, 1,
			horz_skip_y, vert_skip_y, horz_skip_uv,
			vert_skip_uv, rev_mode & 0x1, rev_mode & 0x2,
			vfcd->win_bgn_h[0], vfcd->win_bgn_v[0],
			vfcd->win_end_h[0], vfcd->win_end_v[0], 0, 0, 0,
			0, 0, 0, 0);
		dbg_h2("vfcd_afbc_cfg_done\n");
	} else if (cmpr_sel == 2) {	//afrc
		frc_cfg_vfcd_afrcd(regs_ofst, &vfcd->afrcd);
		frc_cfg_vfcd_fmtup(2, regs_ofst, fmt_mode, hfmt_en, vfmt_en, 1, 1,
			horz_skip_y, vert_skip_y, horz_skip_uv,
			vert_skip_uv, rev_mode & 0x1, rev_mode & 0x2,
			vfcd->win_bgn_h[0], vfcd->win_bgn_v[0],
			vfcd->win_end_h[0], vfcd->win_end_v[0], 0, 0, 0,
			0, 0, 0, 0);
		dbg_h2("vfcd_afrc_cfg_done\n");
	} else {
	}
}

void frc_cfg_vfcd_cmpr_enable(u32 index, u32 cmp_sel)
{
	u32 regs_ofst = index * 256;	//ary addr change *4

	if (cmp_sel == 0) {
		dbg_h2("[VFCD] index: %d, mode: MIF\n", index);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 0x2, 16, 2);
	} else if (cmp_sel == 1) {
		dbg_h2("[VFCD] index: %d, mode: AFBC\n", index);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 0x1, 16, 2);
	} else if (cmp_sel == 2) {
		dbg_h2("[VFCD] index: %d, mode: AFRC\n", index);
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 0x3, 16, 2);
	}
}

