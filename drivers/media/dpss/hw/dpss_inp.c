// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_inp.h"

static void cfg_inp_secure(struct PRM_DPSS_TOP *prm_top)
{
	if (!prm_top->is_secure) {
		w_reg_bit(FRC_INP_DIN_SW_SEL, 0, 13, 1);
		w_reg_bit(FRC_INP_SW_FRM_INFO, 0, 4, 2);
	} else {
		w_reg_bit(FRC_INP_DIN_SW_SEL, 1, 13, 1);
		w_reg_bit(FRC_INP_SW_FRM_INFO, 1, 4, 2);
	}

	dbg_h2("set inp secure:%d\n", prm_top->is_secure);
}

static void _cfg_dpss_inp(struct PRM_DPSS_TOP *prm_top,
			struct PRM_DPSS_INP prm_inp,
			struct PRM_INTF_TYPE *inp_yuv_rdmif)
{
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	u32 frm_hsize = pps->di2pps_en ? prm_top->dpe_mc_size.frm_hsize : prm_top->frm_hsize;
	u32 frm_vsize = pps->di2pps_en ? prm_top->dpe_mc_size.frm_vsize : prm_top->frm_vsize;
	u32 me_dsx = prm_top->dae_dsx_scale;
	u32 me_dsy = prm_top->dae_dsy_scale;
	bool me_hvds_en;
	bool only_fbuf = prm_top->no_ds || prm_top->frc_fbuf_only;
	enum vid_src_fmt src_fmt;
	enum vid_src_fmt src_plan;
	enum vid_src_fmt src_bit;
	u32 v_limit = 540;

	u32 org_hsize;
	u32 org_vsize;

	cfg_inp_secure(prm_top);//inp secure
	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize = pps->di2pps_en ? prm_top->dpe_mc_size.frm_hsize : prm_top->org_hsize;
		org_vsize = pps->di2pps_en ? prm_top->dpe_mc_size.frm_vsize : prm_top->org_vsize;
	} else {
		org_hsize = prm_top->frm_hsize;
		org_vsize = prm_top->frm_vsize;
	}

	if (prm_top->is_i) {
		me_dsx = prm_top->dpe_dw_dsx;
		me_dsy = prm_top->dpe_dw_dsy;
		only_fbuf = false;
		if (pps->di2pps_en && frm_hsize == 1440 && frm_vsize == 1152)
			v_limit = 576;
	}

	if (prm_top->is_i) {
		src_fmt = prm_top->nro_ds_fmt.src_fmt;
		src_plan = prm_top->nro_ds_fmt.src_plan;
		src_bit = prm_top->nro_ds_fmt.src_bit;
		me_hvds_en = prm_top->frc_ds_scale_en || prm_top->frc_fbuf_only;
	} else {
		src_fmt = only_fbuf ? prm_top->src_fs_fmt.src_fmt : prm_top->src_ds_fmt.src_fmt;
		src_plan = only_fbuf ? prm_top->src_fs_fmt.src_plan : prm_top->src_ds_fmt.src_plan;
		src_bit = only_fbuf ? prm_top->src_fs_fmt.src_bit : prm_top->src_ds_fmt.src_bit;
		me_hvds_en = prm_top->frc_ds_scale_en || prm_top->frc_fbuf_only || prm_top->no_ds ||
					prm_top->size_as_in;
		if (prm_top->frm_vsize <= 540 &&
			prm_top->frm_hsize <= 960) {
			if ((prm_top->use_inp_big >> 4) > 0) {
				me_hvds_en = 0;
			} else {
				me_hvds_en = 1;
				me_dsx = 1;
				me_dsy = 1;
			}
		}
	}

	u32 org_me_hsize =
	    me_hvds_en ? org_hsize : (org_hsize + (1 << me_dsx) - 1) >> me_dsx;
	u32 org_me_vsize =
	    me_hvds_en ? org_vsize : (org_vsize + (1 << me_dsy) - 1) >> me_dsy;
	u32 me_hsize =
	    me_hvds_en ? frm_hsize : (frm_hsize + (1 << me_dsx) - 1) >> me_dsx;
	u32 me_vsize =
	    me_hvds_en ? frm_vsize : (frm_vsize + (1 << me_dsy) - 1) >> me_dsy;
	u32 det_hsize = (frm_hsize + (1 << me_dsx) - 1) >> me_dsx;
	u32 det_vsize = (frm_vsize + (1 << me_dsy) - 1) >> me_dsy;
	//  u32 me_blk_hsize = (me_hsize+3)>>2;
	//  u32 me_blk_vsize = (me_vsize+3)>>2;
	u32 hsize_m1 = me_hsize - 1;
	u32 vsize_m1 = me_vsize - 1;
	u32 x_fmt = src_fmt == YUV444 ? 1 : 0;
	u32 y_fmt = src_fmt == YUV420 ? 0 : 1;

	if ((prm_top->use_inp_big & 0x0f) == 1) {
		if (prm_top->dpss_mode != DPSS_FRC_MODE && prm_top->is_i) {
			if (prm_top->frm_vsize <= v_limit && prm_top->frm_hsize <= 960) {
				src_fmt = prm_top->nro_fs_fmt.src_fmt;
				src_plan = prm_top->nro_fs_fmt.src_plan;
				src_bit = prm_top->nro_fs_fmt.src_bit;
				org_me_hsize = frm_hsize;
				org_me_vsize = frm_vsize;
				me_hsize = frm_hsize;
				me_vsize = frm_vsize;
				det_hsize = frm_hsize;
				det_vsize = frm_vsize;
				hsize_m1 = me_hsize - 1;
				vsize_m1 = me_vsize - 1;
				w_reg_bit(DPSS_SRC_RADDR_SEL, 2, 4, 2);
				dbg_h2("%s use_inp_big frm_hsize=%d vsize=%d\n",
					__func__, frm_hsize, frm_vsize);
			}
		}
	}
	dbg_h2("%s hsize=%d vsize=%d v_limit:%d\n", __func__,
			me_hsize, me_vsize, v_limit);

	//====================================================//
	// inp interface
	//====================================================//
	//ary move to glb struct PRM_INTF_TYPE inp_yuv_rdmif; //ary:need cfg input format and addr
	//ary move to glb memset((void*)(&inp_yuv_rdmif), 0, sizeof(struct PRM_INTF_TYPE));

	u32 nr_4k1k_vds_en = prm_top->vds_4k1k_en;

	inp_yuv_rdmif->src_hsize = me_hsize;
	inp_yuv_rdmif->src_vsize = me_vsize;
	inp_yuv_rdmif->src_bit = src_bit;
	inp_yuv_rdmif->src_plan = src_plan;
	inp_yuv_rdmif->src_fmt = src_fmt;
	//   inp_yuv_rdmif.src_baddr[0] =;
	//   inp_yuv_rdmif.src_baddr[1] =;
	inp_yuv_rdmif->skip_line = nr_4k1k_vds_en ? 1 : 0;
	inp_yuv_rdmif->slc_x_st[0] = 0;
	inp_yuv_rdmif->slc_x_ed[0] = hsize_m1;
	inp_yuv_rdmif->slc_y_st[0] = 0;
	inp_yuv_rdmif->slc_y_ed[0] = nr_4k1k_vds_en ? (me_vsize * 2 - 1) : vsize_m1;
	inp_yuv_rdmif->reverse[0] = 0;
	inp_yuv_rdmif->reverse[1] = 0;
	inp_yuv_rdmif->reg_mode = nr_4k1k_vds_en ? 0x1 : 0x0;
	inp_yuv_rdmif->mmu_mode_en = prm_top->ds_mif_mmu_mode_en;
	inp_yuv_rdmif->uv_swap = prm_top->uv_swap;
	inp_yuv_rdmif->little_endian = prm_top->l_endian;
	inp_yuv_rdmif->swap_64bit = prm_top->swap_64bit;
	inp_yuv_rdmif->block_mode = 0;
	if (!prm_top->is_i)
		inp_yuv_rdmif->block_mode = prm_top->block_mode;

	dbg_h2("%s: x:%d %d, y:%d %d.\n",
		__func__,
		inp_yuv_rdmif->slc_x_st[0],
		inp_yuv_rdmif->slc_x_ed[0],
		inp_yuv_rdmif->slc_y_st[0],
		inp_yuv_rdmif->slc_y_ed[0]);

	//   cfg_dpss_rdmif_3ch(DPSS_RMIF_INP,&inp_yuv_rdmif);
	dpss_cfg_viu_pix_rdmif(inp_yuv_rdmif, 0);
	//====================================================//
	// inp top
	//====================================================//

	wr(FRC_INP_MIF_SCOPE_CTRL, 0 << 4 | // mif reg_mode
	   x_fmt << 1 |		// chrm_x_size
	   y_fmt);		// chrm_y_size
	//  w_reg_bit(FRC_REG_INP_FRM_CTRL, 1, 16, 1);// pls_inp_frm_rst

	//====================================================//
	// inp size
	//====================================================//
	if (me_hvds_en) {
		w_reg_bit(FRC_REG_INP_MODULE_EN, 0x3, 3, 2);
		//reg_me_vds_en & reg_me_hds_en set 1
		w_reg_bit(FRC_INP_ME_DS_SCALE, me_dsx, 16, 2);
	//reg_me_hds_scale  //0 :non ;1 :1v2 2: 1v4
		w_reg_bit(FRC_INP_ME_DS_SCALE, me_dsy, 0, 2);
	//reg_me_vds_scale  //0 :non ;1 :1v2 2: 1v4
	} else {
		w_reg_bit(FRC_REG_INP_MODULE_EN, 0x0, 3, 2);
	}
	wr(FRC_INP_SW_FRM_ORIGINAL_SIZE, org_me_hsize << 16 |	//original_size
		org_me_vsize);	//original_size
	wr(FRC_INP_SW_FRM_SKIP_SIZE, org_me_hsize << 16 |	//skip_size
		org_me_vsize);	//skip_size
	wr(FRC_INP_SW_FRM_ALIGNED_SIZE, me_hsize << 16 |	//aligned_size
		me_vsize);		//aligned_size
	wr(FRC_INP_SW_DETECT_SIZE, det_hsize << 16 |	//detect_size
		det_vsize);		//detect_size
	w_reg_bit(FRC_INP_DIN_SW_SEL, 0xff, 4, 8);	//sw cfg above size
	dbg_h2("%s finish\n", __func__);
}

//cfg_dpss_inp
void hw_cfg_dpss_inp(struct PRM_DPSS_TOP *prm_top, struct PRM_DPSS_INP prm_inp)
{
	_cfg_dpss_inp(prm_top, prm_inp, &g_inp_yuv_rdmif);
}

void inp_chg2done_triggle(void)
{
	w_reg_bit(FRC_REG_INP_FRM_CTRL, 0, 4, 1); //auto frm_start
	w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 3, 0, 2); //inp use frm_done int
	w_reg_bit(DPSS_FRC_FRM_DONE_SEL, 1, 0, 1); //write inp frmae valid flag by fw
	w_reg_bit(FRC_AUTO_MODE_ENABLE, 1, 12, 1); //reg_inp_ofrm_wren_se
}

void cfg_inp_triggle(bool triggle_sel)
{
	if (triggle_sel)
		w_reg_bit(FRC_REG_INP_FRM_CTRL, 1, 0, 1);	//inp frm start
	else
		w_reg_bit(DPSS_FRC_INP_FW_DONE, 1, 0, 1);	//frm_done_triggle
}
