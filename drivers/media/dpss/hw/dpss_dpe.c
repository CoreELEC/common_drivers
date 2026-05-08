// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_dpe.h"
#include "dpss_aa_pps.h"
#include "dpss_vdi.h"
#include "../sys_def.h"
#include <linux/module.h>
#ifdef RUN_ON_ARM
#include <linux/math64.h>
#endif
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

struct AA_PPS_TOP_TYPE nr_pps_cfg; //ary move from local
static void cfg_dpe_secure(unsigned int dpe_mode,
		struct PRM_DPSS_TOP  *prm_top,
		struct PRM_DPSS_DPE  *prm_dpe);

bool frc_direct_connect;
module_param_named(frc_direct_connect, frc_direct_connect, bool, 0664);

unsigned int bbd_mode;
module_param_named(bbd_mode, bbd_mode, uint, 0664);

bool bb_en;
module_param_named(bb_en, bb_en, bool, 0664);

bool last_lcevc;

static const int dpe_pps_lut_tap8[33][8] = {
	{0,   0,   0, 512,   0,   0,   0,   0},
	{-1,    3,   -7,  512,    7,   -3,    1, 0},
	{-2,    5,  -14,  511,   15,   -5,    2, 0},
	{-2,    7,  -20,  510,   23,   -8,    3, -1},
	{-3,   10,  -27,  509,   31,  -11,    3,  0},
	{-4,   12,  -32,  507,   39,  -14,    4,  0},
	{-4,   14,  -38,  504,   48,  -17,    5,  0},
	{-5,   16,  -43,  501,   57,  -19,    6, -1},
	{-5,   18,  -49,  498,   66,  -22,    7, -1},
	{-6,   19,  -53,  495,   75,  -26,    8, 0},
	{-6,   21,  -58,  490,   85,  -29,   10, -1},
	{-6,   22,  -62,  486,   94,  -32,   11, -1},
	{-7,   24,  -66,  481,  104,  -35,   12, -1},
	{-7,   25,  -69,  476,  114,  -38,   13, -2},
	{-7,   26,  -72,  470,  124,  -41,   14, -2},
	{-8,   27,  -75,  464,  134,  -44,   15, -1},
	{-8,   28,  -78,  458,  145,  -47,   16, -2},
	{-8,   29,  -80,  451,  155,  -50,   17, -2},
	{-8,   30,  -83,  444,  166,  -53,   18, -2},
	{-8,   31,  -84,  437,  177,  -56,   19, -4},
	{-8,   31,  -86,  429,  188,  -59,   20, -3},
	{-9,   32,  -87,  421,  199,  -62,   22, -4},
	{-8,   32,  -88,  413,  209,  -64,   23, -5},
	{-8,   32,  -89,  405,  220,  -67,   24, -5},
	{-9,   33,  -89,  396,  231,  -69,   25, -6},
	{-8,   33,  -90,  387,  242,  -72,   25, -5},
	{-8,   33,  -90,  377,  253,  -74,   26, -5},
	{-8,   32,  -89,  368,  264,  -76,   27, -6},
	{-8,   32,  -89,  358,  275,  -78,   28, -6},
	{-8,   32,  -88,  348,  286,  -80,   29, -7},
	{-7,   32,  -88,  338,  297,  -82,   29, -7},
	{-7,   31,  -86,  328,  307,  -84,   30, -7},
	{-8,   31,  -85,  318,  318,  -85,   31, -8}
};

void hw_cfg_dpss_dpe(enum DPSS_WORK_MODE  dpe_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DPE  *prm_dpe,
	struct AA_PPS_TOP_TYPE *nr_pps_cfg)
{
	dbg_w2("%s:%d\n", "cfg_dpe", dpe_mode);

	u32 reg_din_sel_sw_en0 = 0;//0xffe00000; //bit31:21
	u32 reg_din_sel_sw_en1 = 0;//0x007fffff; //bit54:32
	u32 reg_din_sel_sw_en2 = 0;//0x00000018; //bit69:67

	u32 wrmif_en     = 0xf;//{?,dblk_h,dblk_v,dmsq_wr,nrwr,diwr}
	//ary no use   u32 rdmif_en_t   = 0xf;
	u32 rdmif_en     = 0xf;
	u32 di_en        = 0;
	u32 nr_en        = 0;
	u32 ds_mix_en    = 0;
	u32 frm_end_mask = 0;
	u32 mif_reg_mode = 0;//0:hw_auto 1:cpu_reg
	//for nr debug mode, 0:close debug mode 1:open debug mode
	bool nr_debug_mode = ((rd(DOS_DOWN_S_MODE) >> 8) & 0xff) != 0;
	//for di debug mode, 0:close debug mode 1:open debug mode
	//bool di_debug_mode = ((rd(VPU_DI_BLEND_EI_POST_EN_MODE)>>4) & 0xf) !=0;
	bool     nr_cur_only_mode = ((rd(VPU_NR_TOP_MISC) >> 20) & 0x1);
	//    u32 org_hsize;
	//    u32 org_vsize;
	bool     me_mc_link_en = !prm_top->me_mc_link_en;
	bool     dpe_game_mode = !prm_top->dpe_game_mode;
	bool     dpe_sw_ctrl   = prm_top->sw_ctrl_en;
	bool dolby_parallel;
	u32 dcntr_en     = prm_dpe->dcntr_en;

	u32 bbd_en     = rd(DPSS_DPE_BBD_CTRL) == 0x2;
	//reg_aa_pps_mode == 3 -> nr aapps 4k1k
	bool nr_aapps_en;//= ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;
	bool dpss_nr_vpp_link = (rd(DPSS_NR_VPP_LINK) & 0x1);
	bool nr_src1_di_loop = 0;//di loop when src1 only nr

	dbg_h2("pps ch=%d, dcntr_en=%d,=%d\n", prm_top->ch,
			dcntr_en, nr_pps_cfg->pps_en);

	if (nr_pps_cfg->pps_en)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	dbg_h2("\treg:en:bbd=%d, nr_aapps=%d, nr_link=%d\n",
		bbd_en, nr_aapps_en, dpss_nr_vpp_link);
	dolby_parallel = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;
	cfg_dpe_secure(dpe_mode, prm_top, prm_dpe); //secure
	if (dolby_parallel && prm_top->src_mode) {//ary tmp
	} else if (!dpe_sw_ctrl && !dpss_nr_vpp_link) {
		wr(DPSS_DPE_TOP_SW_SEL0, reg_din_sel_sw_en0);
		wr(DPSS_DPE_TOP_SW_SEL1, reg_din_sel_sw_en1);
		wr(DPSS_DPE_TOP_SW_SEL2, reg_din_sel_sw_en2);
	}

	if (dpss_force_nr_debug || dpss_nr_debug == 2)
		nr_debug_mode = 0;

	prm_top->dpe_out2bbd_en = bb_en;
	prm_top->dpe_out2bbd_mode = bbd_mode;
	u32 me_dsx       = prm_top->dae_dsx_scale;
	u32 me_dsy       = prm_top->dae_dsy_scale;
	//u32 is_psrc      = prm_top->src_fs_fmt.interlace == IS_PSRC;//is_progressive
	u32 nr_di2bbd    = prm_top->dpe_out2bbd_en == 1 && (prm_top->dpe_out2bbd_mode <= 1);
	u32 dv2bbd       = prm_top->dpe_out2bbd_en == 1 && (prm_top->dpe_out2bbd_mode == 2);
	u32 di2bbd       = prm_top->dpe_out2bbd_en == 1 && (prm_top->dpe_out2bbd_mode == 1);
	//u32 dpe_out2bbd  = nr_di2bbd || dv2bbd || di2bbd;

	u32 dcntr_pad = dcntr_en ? 8 : 0;

	//u32 bbd_pad = dpe_mode == DPE_BBD_ONLY ? 2 :
	//	dpe_mode == DPE_NR_BYPS && dcntr_en ? 0 :
	//	dpe_mode == DPE_NR_BYPS && bbd_en ? 2 : 0;
	u32 bbd_pad = 8;

	//   u32 vbe_pad = (dpe_mode == DPE_NR_BYPS || dpe_mode == DPE_BBD_ONLY ?  0 : 64);

	//u32 aa_pad   = (prm_top->nr_aapps_up | prm_top->nr_aapps_on) ? 16 :
	//		prm_top->comp_setting.vfce_in_overlap > 0 ? 4 :
	//		dpe_mode == DPE_NR_BYPS && bbd_en ? bbd_pad : 0;
	u32 aa_pad = (nr_pps_cfg->pps_en | prm_top->nr_aapps_on) ? 16 :
			prm_dpe->aa_pad_assign_en == 0 ? 8 : prm_dpe->aa_pad;

	if (dpss_en_lcevc) {
		aa_pad = 0;
		dbg_h2("lcevc chg aa_pad\n");
	}

	u32 vbe_pad = (dpe_mode == DPE_NR_BYPS || dpe_mode == DPE_BBD_ONLY) ? 0 + aa_pad :
			64;

	if (dpss_nr_debug || dpss_force_nr_debug) {
		bbd_pad = 0;
		aa_pad = 0;
		vbe_pad = (dpe_mode == DPE_NR_BYPS || dpe_mode == DPE_BBD_ONLY ? 0 :
				prm_top->comp_setting.vfce_in_overlap > 0 ? 60 : 64) + aa_pad;
	}
	if (dpss_force_nr_debug || dpss_nr_debug == 2) {
		nr_pps_cfg->aa_pad = 0;
		prm_dpe->aa_pad = 0;
		prm_dpe->aa_pad_assign_en = 1;
	}
	if (dpss_force_nr_debug > 0xf)
		prm_dpe->dpe_reg_mode.vfce_reg_mode = 1;
	u32 nrdi_pad = aa_pad >= 8  ? aa_pad : bbd_pad;
	u32 amdv_pad   = 0;

	struct PRM_DPE_PAD dpe_pad;

	dpe_pad.reg_vbe_pad  = vbe_pad;
	dpe_pad.reg_nrdi_pad = nrdi_pad;
	dpe_pad.reg_mc_pad   = 32;
	//dpe_pad.reg_amdv_pad   = 96;
	dpe_pad.reg_aa_pad   = aa_pad;
	dpe_pad.reg_dmsq_pad = 32;
	dpe_pad.reg_dcntr_pad = dcntr_pad;
	dpe_pad.reg_bbd_pad = bbd_pad;

	u32 reg_nr_byps_en   = 0;
	u32 reg_nr_byps_sel  = 0;//1:din0 2:din1
	u32 reg_nr_byps_mode = 0;//dat_sel 0:din0

	dbg_h2("dpemode=%d 31 nr 32 nr_byps dby_mode=%d\n", dpe_mode, prm_top->dolby_mode);
	dbg_h2("aa_pad = %d\n", aa_pad);
	//dbg_h2("[dpss_dpe.c]:org_hsize=%4d  org_vsize=%4d\n", org_hsize, org_vsize);
	//dbg_h2("[dpss_dpe.c]:frm_hsize=%4d  frm_vsize=%4d\n", frm_hsize, frm_vsize);
	//path0=nr path1=di path2=dv

	u32 wrpath_sel = (rd(DPSS_DPE_INTF_AFBCD0) >> 12) & 0x1ffff;

	u32 vfce_on = wrpath_sel & 0x1; //cmp or uncmp
	u32 vfce_sel = (wrpath_sel >> 1) & 0x7;
	u32 w2_sel = (wrpath_sel >> 4) & 0x7;
	u32 w0_sel = (wrpath_sel >> 7) & 0x7;
	u32 w1_sel = (wrpath_sel >> 10) & 0x7;
	u32 ds_sel = (wrpath_sel >> 13) & 0x3;

	dbg_h2("\twrpath_sel = 0x%x, src_cmpr=%d\n", wrpath_sel, prm_top->nro_fs_fmt.src_cmpr);
	if (!prm_top->src_mode ||
	    (prm_top->src_mode && !dolby_parallel)) { //0714 tmp
		if (prm_top->nro_fs_fmt.src_cmpr == CMPR_UN)
			vfce_on = 0x0; //uncmp
		else
			vfce_on = 1;//rd(DPSS_REG_DPE_LOSS_OUT_EN); //cmp
		dbg_h2("%s:set vfce_on = %d\n", __func__, vfce_on);
	}
	u32 submif_msk;

	switch (dpe_mode) {           //todo use dblk_en/dmsq_en to mif_en/mask_en
	case DPE_MC_MODE:
		di_en        = 0b0;
		nr_en        = 0b00;
		wrmif_en     = 0b0000;   //{ds_mix,dblk_h,dblk_v,dmsq_wr}
		rdmif_en     = dcntr_en ? 0b10111111 : 0b00111111;
		ds_mix_en    = 0b0;
		//frm_end_mask = 0b111100; //{ds_mix,dblk_h,dblk_v,dmsq_wr,path1,path0}
		submif_msk   = 0b111;
		break;
	case DPE_BBD_ONLY:
		di_en        = 0b0;
		nr_en        = 0b00;
		wrmif_en     = 0b0000;
		rdmif_en     = 0b00000000;
		ds_mix_en    = 0b0;
		submif_msk   = 0b111;
		reg_nr_byps_en   = 1;
		reg_nr_byps_sel  = 0;  //din0_hs
		reg_nr_byps_mode = 0;  //din0_dat
		vfce_sel     = 7;
		w2_sel       = vfce_sel;
		w1_sel       = 7;
		w0_sel       = 7;
		ds_sel       = 3;
		break;
	case DPE_NR_MODE:
		di_en        = 0b0;
		nr_en        = 0b01;
		wrmif_en     = nr_cur_only_mode ? 0b0110 : 0b0111;
		//{ds_mix,dblk_h,dblk_v,dmsq_wr}
		rdmif_en     = dcntr_en ? 0b11010101 : 0b01010101;
		ds_mix_en    = 0b1;
		//frm_end_mask = 0b100010;//{ds_mix,dblk_h,dblk_v,dmsq_wr,path1,path0}
		submif_msk   = 0b000;
		vfce_sel     = 0;
		w2_sel       = vfce_sel;
		w1_sel       = nr_aapps_en ? 3 : nr_debug_mode ? 1 : 7;//4k1k wr1
		w0_sel       = 7;
		ds_sel       = prm_top->dpe_dw_off ? 3 : 0;
		break;
	case DPE_VDI_MODE:   //di_only
		di_en        = 1;
		nr_en        = 0b00;
		wrmif_en     = 0b0000;
		rdmif_en     = dcntr_en ? 0b10111111 : 0b00111111;//todo
		ds_mix_en    = 0b0;
		//frm_end_mask = 0b111101;
		submif_msk   = 0b111;
		vfce_sel     = 1;
		w2_sel       = vfce_sel;
		w1_sel       = nr_aapps_en ? 3 : 7;
		w0_sel       = 0;
		ds_sel       = 1;
		break;
	case DPE_NR_SRC1_MODE:
		di_en        = 0;
		nr_en        = 0b10;
		wrmif_en     = 0b0111;
		rdmif_en     = dcntr_en ? 0b11010101 : 0b01010101;
		ds_mix_en    = 0b0;
		nr_src1_di_loop = 1;
		//frm_end_mask = 0b100010;
		submif_msk   = 0b000;
		vfce_sel     = 0;
		w2_sel       = vfce_sel;
		w1_sel       = 7;
		w0_sel       = 7;
		ds_sel       = 0;
		break;
	case DPE_NRDI_MODE:  //nr+di
		di_en        = 1;
		nr_en        = 0b10;
		wrmif_en     = 0b0111;
		rdmif_en     = dcntr_en ? 0b11111111 : 0b01111111;
		ds_mix_en    = 0b0;
		//frm_end_mask = 0b111100;
		submif_msk   = 0b000;
		vfce_sel     = 1;
		w2_sel       = vfce_sel;
		w1_sel       = nr_aapps_en ? 3 : 7;
		w0_sel       = 0;
		ds_sel       = 1;
		break;
	case DPE_NR_BYPS:
		di_en        = 0b0;
		nr_en        = 0b00;
		wrmif_en     = 0b0010;    //{ds_mix,dblk_h,dblk_v,dmsq_wr}
		rdmif_en     = dcntr_en ? 0b11010101 : 0b01010101;//todo
		reg_nr_byps_en   = 1;
		reg_nr_byps_sel  = 1;  //din0_hs
		reg_nr_byps_mode = 0;  //din0_dat
		ds_mix_en    = 0b0;
		//frm_end_mask = 0b111110;
		submif_msk   = 0b111;
		vfce_sel     = 0;
		w2_sel       = vfce_sel;
		w1_sel       = nr_aapps_en ? 3 : 7;//4k1k wr1
		w0_sel       = 7;
		ds_sel       = 0;
		break;
	//	case :add NRDI_MODE
	//case DPSS_FRC_DV_PRL_MODE://no-use
		//di_en        = 1;
		//nr_en        = 0b10;
		//wrmif_en     = 0b0111;
		//rdmif_en     = dcntr_en ? 0b11111111 : 0b01111111;//todo
		//ds_mix_en    = 0b0;
		////frm_end_mask = 0b111101;
		//submif_msk   = 0b111;
		//vfce_sel     = 2;
		//w2_sel       = vfce_sel;
		//w1_sel       = 1;
		//w0_sel       = 0;
		//ds_sel       = 2;
		//break;
	default:  //ERROR
		di_en        = 0b1;
		nr_en        = 0b11;
		wrmif_en     = 0b0111;
		rdmif_en     = dcntr_en ? 0b11111111 : 0b01111111;
		ds_mix_en    = 0b0;
		//frm_end_mask = 0b111110;
		submif_msk   = 0b111;
		DBG_ERR("%s: Error: dpe_mode 0x%x\n", __func__, dpe_mode);
		return;
	}
	prm_dpe->dpe_mode = dpe_mode;
	prm_dpe->nr_en    = nr_en;
	prm_dpe->di_en    = di_en;

//	u32 dolby_parallel;

//	dolby_parallel = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;
	vfce_sel = dolby_parallel ? (nr_aapps_en ? 3 : 2) : vfce_sel;
	w1_sel = dolby_parallel ? 1 : w1_sel;
	w0_sel = dolby_parallel ? 0 : w0_sel;
	ds_sel = dolby_parallel ? 2 : ds_sel;
	w2_sel = vfce_sel;
	u32 vfce_mask = dolby_parallel ? 1 : vfce_sel == 7;
	u32 w0_mask = w0_sel == 7;
	u32 w1_mask = w1_sel == 7;
	u32 ds_mask = dolby_parallel ? 1 : ds_sel == 3;

	if (dpss_dbg_dis_dw || prm_top->dw_disable) {
		ds_sel = 3;
		dbg_h2("this force disable dw\n");
	}
	if (dpss_force_nr_debug || dpss_nr_debug == 2) {
		vfce_mask = 1;
		ds_mask = 1;
		ds_sel = 3;
		dbg_h2("this force disable dw\n");
	}
	frm_end_mask = (submif_msk << 4) |
			(ds_mask << 3) |
			(w1_mask << 2) |
			(w0_mask << 1) | (vfce_mask);

	wrpath_sel = (ds_sel << 13) |
			(w1_sel << 10) |
			(w0_sel << 7) |
			(w2_sel << 4) |
			(vfce_sel << 1) |
			(vfce_on);

	w_reg_bit(DPSS_DPE_INTF_AFBCD0, wrpath_sel, 12, 15);
	if (dpss_force_nr_debug || dpss_nr_debug == 2) {
		w_reg_bit(DPSS_DPE_INTF_AFBCD0, 1, 12 + 4, 3);
		w_reg_bit(DPSS_DPE_INTF_AFBCD0, 3, 12 + 13, 2);
		w_reg_bit(DPSS_DPE_INTF_CTRL0, 1, 0, 2);//close ds wrmif0
		w_reg_bit(DPSS_DPE_INTF_CTRL0, 1, 2, 2);//close ds wrmif0
	}

	//VPU_VBE_TOP_HDR_CTRL[1:0] - reg_vbe_hdr_path_sel
	//0:dcntr2hdr; 1:nr; 2:di; 3:before dpe
	u32 vbe_hdr_path_sel = 3;
	u32 dct_status = 0;

	if (!prm_top->src_mode && dcntr_en &&
		prm_top->dct_ahead_dv_mode) {
		vbe_hdr_path_sel = 0;
		dct_status = 1;
	} else {
		if (dpss_en_lcevc) {
			vbe_hdr_path_sel = 1;
			dct_status = 1;
		}
	}

	if (!dpss_en_hdr || prm_top->di_front) {
		vbe_hdr_path_sel = 3;
		dct_status = 0;
	}

	w_reg_bit(VPU_VBE_TOP_HDR_CTRL, vbe_hdr_path_sel, 0, 2);
	if (!prm_top->di_front) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		amvecm_set_ext_status_for_dpss(dct_status);
#endif
	}
//	dbg_h2("vbe_hdr_path_sel=%d, dct_status=%d\n",
//		vbe_hdr_path_sel, dct_status);

	//reg_mode
	bool top_reg_mode  = prm_dpe->dpe_reg_mode.top_reg_mode;
	bool vfcd_reg_mode = prm_dpe->dpe_reg_mode.vfcd_reg_mode;
	bool vfce_reg_mode = prm_dpe->dpe_reg_mode.vfce_reg_mode;
	u32 subrd_reg_mode = prm_dpe->dpe_reg_mode.subrd_reg_mode;
	u32 subwr_reg_mode = prm_dpe->dpe_reg_mode.subwr_reg_mode;

	u32 cur_mif_pad = dcntr_pad + vbe_pad;	//ary no use

	//nr_byp,vbe_pad=0,nrdi_pad=0;
	//nr_byp+bbd,vbe_pad=2,nrdi_pad=2;
	//nr_byp+dcntr,vbe_pad=dcntr_pad,nrdi_pad=0
	//nr_byp+dcntr+bbd,vbe_pad=dcntr_pad,nrdi_pad=0
	if (frc_direct_connect && get_cpu_type() == MESON_CPU_MAJOR_ID_T6X)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 0, 1);//bypass nr,default = 7

	dbg_h2("%s:nr_byps_en=%d, dctre_en = %d\n", __func__, reg_nr_byps_en, dcntr_en);
	if (reg_nr_byps_en & ~dcntr_en) {
		dpss_vbe_proc_byp(0, 0);//bypass nr
		if (dpe_mode == DPE_BBD_ONLY) {
			w_reg_bit(DPSS_DPE_RDMA_INFO1, 1, 0, 8);
			//bbd_only can work when bypass
			w_reg_bit(DPSS_DPE_RDMA_WR_SIZE, 1, 16, 1);
		//BBD_ONLY,bbd_ro_num =83
			w_reg_bit(DPSS_DPE_RDMA_WR_SIZE, 83, 0, 12);
		} else if (rd(DPSS_DPE_BBD_CTRL) == 0x2) {//bbd_on
			w_reg_bit(DPSS_DPE_RDMA_INFO1, 0x1f, 0, 8);
			//ro match except bbd_only
		}
	} else if (reg_nr_byps_en & dcntr_en) {
		wr(VPU_NR_TOP_SYNC_CTRL, 0xffffffff);
		//wr(VPU_NR_ENABLE, 0x0);
		w_reg_bit(VPU_NR_ENABLE, 0x0, 1, 31);
		w_reg_bit(DPSS_DPE_TOP_CTRL0, 0, 4, 4);
		//reg_wrmif_enable,{dblk_h,dblk_v,dmsq_wr}
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 2, 1);//nr_done_mask
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 28, 1); //reg_nr_byps_en
		w_reg_bit(DPSS_BBD_ONLY_CTRL, 1, 1, 1);//nr_cur_only
		w_reg_bit(VPU_NR_OUT_CROP, 0, 0, 16);//reg_nr_out_hcrop_left/right
		//        w_reg_bit(DPSS_DPE_PAD,dpe_pad.reg_vbe_pad,24,8); //vbe_pad
	} else {
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_mode & 0x7, 29, 3);
		//reg_nr_byps_mode
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_en & 0x1, 28, 1); //reg_nr_byps_en
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_sel & 0x7, 8, 3); //reg_nr_byps_sel
		w_reg_bit(DPSS_BBD_ONLY_CTRL, nr_cur_only_mode, 1, 1);//nr_cur_only
		if (dpss_nr_debug || dpss_force_nr_debug) {
			if (prm_top->comp_setting.vfce_in_overlap > 0)
				w_reg_bit(VPU_NR_OUT_CROP, 60 << 8 | 60 << 0, 0, 16);
				//reg_nr_out_hcrop_left/right
			else
				w_reg_bit(VPU_NR_OUT_CROP, 64 << 8 | 64 << 0, 0, 16);
		} else {
			w_reg_bit(VPU_NR_OUT_CROP,
				(vbe_pad - aa_pad) << 8 | (vbe_pad - aa_pad) << 0, 0, 16);
		}
		w_reg_bit(DPSS_DPE_HW_DBG, 0, 2, 2);
		w_reg_bit(DPSS_DPE_HW_DBG, 0, 14, 1); // bbd done mask
		// bit2-nr core done mask, bit3-di_core done mask
	//	w_reg_bit(DPSS_DPE_PAD,dpe_pad.reg_vbe_pad,24,8); //vbe_pad
	}
	w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_vbe_pad, 24, 8);//vbe_pad
	w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_dcntr_pad, 0, 8); //dcntr_pad

	dpe_pad.reg_dv_pad = ((prm_top->dolby_mode == DOLBY_DPSS_MODE) ||
		(dolby_parallel & (dpe_mode == DPE_NR_BYPS))) ? 96 : 0;//todo

	//cfg dpe size for software mode
	hw_cfg_dpe_size(dpe_mode, prm_top, prm_dpe, dpe_pad);
	if (prm_top->dpe_slc_num == 1) // 20260116
		prm_dpe->lst_overlap = 0;
	else
		prm_dpe->lst_overlap = dpe_pad.reg_vbe_pad +
			dpe_pad.reg_dv_pad + dpe_pad.reg_dcntr_pad; //1225
	hw_cfg_dpss_dpe_intf(prm_top, prm_dpe, nr_pps_cfg->pps_en);

	u32 cur_lft_pad[4];
	u32 cur_rgt_pad[4];
	u32 dct2dv_lft_pad[4];
	u32 dct2dv_rgt_pad[4];
	u32 slc_num = prm_top->dpe_slc_num;

	u32 dct2dv_en = ((rd(VPU_VBE_TOP_HDR_CTRL) & 0x3) == 0);
	u32 dct2dv_pad = dct2dv_en ? dpe_pad.reg_dv_pad : 0;

	dct2dv_lft_pad[0] = 0;
	dct2dv_lft_pad[1] = slc_num == 2 ? dct2dv_pad : slc_num == 4 ? dct2dv_pad / 2 : 0;
	dct2dv_lft_pad[2] = dct2dv_pad / 2;
	dct2dv_lft_pad[3] = dct2dv_pad;

	dct2dv_rgt_pad[0] = slc_num != 1 ? dct2dv_pad : 0;
	dct2dv_rgt_pad[1] = slc_num == 4 ? dct2dv_pad / 2 : 0;
	dct2dv_rgt_pad[2] = dct2dv_pad / 2;
	dct2dv_rgt_pad[3] = 0;

	cur_lft_pad[0] = 0;
	cur_lft_pad[1] = dct2dv_lft_pad[1] + dcntr_pad + vbe_pad;
	cur_lft_pad[2] = dct2dv_lft_pad[2] + dcntr_pad + vbe_pad;
	cur_lft_pad[3] = dct2dv_lft_pad[3] + dcntr_pad + vbe_pad;

	cur_rgt_pad[0] = dct2dv_rgt_pad[0] + dcntr_pad + vbe_pad;
	cur_rgt_pad[1] = dct2dv_rgt_pad[1] + dcntr_pad + vbe_pad;
	cur_rgt_pad[2] = dct2dv_rgt_pad[2] + dcntr_pad + vbe_pad;
	cur_rgt_pad[3] = 0;

	if (dcntr_en) {
		//dcntr_pst_slc(&prm_dpe->dcntr_slc, cur_mif_pad);	//12-17
		dcntr_pst_slc(&prm_dpe->dcntr_slc, cur_mif_pad,
			cur_lft_pad, cur_rgt_pad,
			prm_top->frm_hsize_sel,
			prm_top->dpe_slc_num,
			prm_top->dae_dsx_scale,
			prm_top->dae_dsy_scale,
			prm_top->dct_ahead_dv_mode);
	} else {
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 26, 1);//close dcntr
		w_reg_bit(DCT_YCFLT_MIF0_RMIF_CTRL1, 0, 19, 1);//close dct dpe rdmif0
		w_reg_bit(DCT_YCFLT_MIF1_RMIF_CTRL1, 0, 19, 1);//close dct dpe rdmif1
	}

	//aa_pad = dpe_pad.reg_aa_pad;
	amdv_pad   = dpe_pad.reg_dv_pad;

	w_reg_bit(DPSS_DPE_TOP_CTRL0, prm_top->dpe_slc_num, 16, 3);
	//reg_slc_num //todo, need separate 2src slc_num
	w_reg_bit(DPSS_DPE_TOP_CTRL0, rdmif_en, 8, 8); //reg_rdmif_enable
	w_reg_bit(DPSS_DPE_TOP_CTRL0, wrmif_en, 4, 4); //reg_wrmif_enable
	//2024-11-28 discuss with VLSI jin.xue no use
	//w_reg_bit(DPSS_DPE_TOP_CTRL0,di_en   ,0,1); //reg_vbe_nrdi_sel
	w_reg_bit(DPSS_DPE_TOP_CTRL0, nr_src1_di_loop, 23, 1);//reg_src1_nr_di_loop
	w_reg_bit(DPSS_DPE_DS_MIX, ds_mix_en, 26, 1);//reg_ds_mix_en
	w_reg_bit(DPSS_DPE_INTF_DBG, frm_end_mask & 0x7ff, 0, 11);
	//reg_frm_end_mask
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, aa_pad, 8, 8);
	//reg_aa_pad reg_nrdi_pad //todo temporary
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, amdv_pad, 24, 8);
	//reg_dv_pad
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, nrdi_pad, 16, 8);
	//reg_aa_pad reg_nrdi_pad //todo temporary
	w_reg_bit(DPSS_DPE_HW_DBG, ~dcntr_en, 12, 1); // dcntr_slc_end mask

	if (prm_top->src_mode == 0) {
		w_reg_bit(DPSS_DPE_PAD, 96, 8, 8); //todo,use mcpad as src1 dvpad
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (prm_dpe->nr_copy_mode && !prm_top->is_amdv_frame) {
			w_reg_bit(DPSS_DPE_PAD, prm_top->amdv_pad, 8, 8);
			dbg_h0("nr dv cp mode DPSS_DPE_PAD 0\n");
	}
#endif
	}
	if (dpss_force_nr_debug > 0xf)
		vfcd_reg_mode = 1;

	mif_reg_mode = subwr_reg_mode << 15 | subrd_reg_mode << 3 |
		vfce_reg_mode << 2 | vfcd_reg_mode << 1 | top_reg_mode;

	w_reg_bit(DPSS_DPE_MIF_CTRL0, me_dsy & 0x3, 22, 2);//reg_vbe_me_ysft
	w_reg_bit(DPSS_DPE_MIF_CTRL0, me_dsx & 0x3, 20, 2);//reg_vbe_me_xsft
	w_reg_bit(DPSS_DPE_MIF_CTRL0, mif_reg_mode & 0xfffff, 0, 20);//reg_mif_mode
	//wr(DPSS_DPE_MIF_CTRL0, ((me_dsy & 0x3) << 22) |//reg_vbe_me_ysft
	//	((me_dsx & 0x3) << 20)|//reg_vbe_me_xsft
	//	((mif_reg_mode & 0xfffff) << 0));//reg_mif_mode
	//wr(DPSS_DPE_DMSQ_SIZE,(1 << 29) | //reg_dmsq_xsft
	//	((dmsq_hsize & 0x1fff) << 16)|//reg_dmsq_hsize
	//	((dmsq_vsize & 0x1fff) << 0));//reg_dmsq_vsize

	w_reg_bit(FRC_MC_MVRD_CTRL, dpe_game_mode << 1 | me_mc_link_en, 0, 2);
	//dpe_game_mode,me_mc_link_en
	//w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0,0,0,2); //nrdi vfcd out fmt =444
	//w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0,1,2,2); //mc   vfcd out fmt =422 //todo,.420??

	//==================================================
	//BBD_ONLY
	//==================================================
	if (prm_dpe->dpe_mode == DPE_BBD_ONLY) {
		w_reg_bit(DPSS_DPE_PAD, 2, 24, 8); //bbd need +- 2 padding
		w_reg_bit(DPSS_BBD_ONLY_CTRL, 1, 0, 1);
	}

	//==================================================
	//NR_CUR hscale 4096->3840
	//==================================================
	u32 hsc_hsize_in = prm_dpe->dpe_nr_size.rmif_hsize[0];
	u32 hsc_hsize_out = prm_dpe->dpe_nr_size.frm_hsize;
	u32 hsc_out_lft_pad = dpe_pad.reg_vbe_pad + dpe_pad.reg_dcntr_pad;
	u32 hsc_out_rgt_pad = dpe_pad.reg_vbe_pad + dpe_pad.reg_dcntr_pad;

	if (prm_top->nr_hscale_on)
		cfg_dpe_hscale(1, hsc_hsize_in, hsc_hsize_out, hsc_out_lft_pad, hsc_out_rgt_pad);

	//==================================================
	//NR_OUT pps scale up, 1280*720->1920*1080; 2560*1440->3840*2160
	//example:640*120->960*180,slc=4
	//TODO:need support slc=2,slc=1
	//==================================================
	int slc_out_size;
	int slc_out_size1;
//ary no use    u32 reg_hds_sel;
//ary no use    u32 reg_vds_sel;
//ary no use    u32 reg_hvds_sel;
	u32 skip_dsx;
	u32 skip_dsy;
	u32 slc_act_out_bgn1[4];
	u32 slc_act_out_end1[4];
	u32 bbd_slc_in_lft_ovlp[4];
	u32 bbd_slc_in_rgt_ovlp[4];

	dbg_w2("%s:aapps_up=%d, pps_hsize=%d, pps_vsize=%d.\n", __func__,
			nr_pps_cfg->pps_en,
			prm_dpe->dpe_nr_size.pps_hsize,
			prm_dpe->dpe_nr_size.pps_vsize);

	slc_out_size  = prm_dpe->dpe_nr_size.pps_hsize / prm_top->dpe_slc_num;
	slc_out_size  = (slc_out_size & 0x1) + slc_out_size;

	slc_out_size1  = prm_top->frm_hsize / prm_top->dpe_slc_num;
	slc_out_size1  = (slc_out_size1 & 0x1) + slc_out_size1;

	bool di2pps = (ds_sel == 1); //di_out -->pps, wrmif0 for pre.wrmif1 for pps,wrmif2 for di
	u32 pps_out2bbd_en = (nr_pps_cfg->pps_en == 1 || prm_top->nr_aapps_on == 1) &&
		(prm_top->dpe_out2bbd_en == 1 && (prm_top->dpe_out2bbd_mode == 3));

	if (!nr_aapps_en && !prm_top->comp_setting.vfce_in_overlap) {
		dbg_h2("pps reset\n");
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 8, 2);
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 12, 2);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 0, 11, 1);
		wr(FRC_AA_PPS_COMP, 0xa1dc44);
		w_reg_bit(FRC_AA_PPS_CTRL, 3, 0, 2);
		wr(FRC_AA_PPS_HSC_START_PHASE_STEP, 0x1000000);
		wr(FRC_AA_PPS_VSC_START_PHASE_STEP, 0x21000000);
		wr(FRC_AA_PPS_COEF, 0x80);
	}

	//ary move to globe    AA_PPS_TOP_t nr_pps_cfg;
	if (nr_pps_cfg->pps_en) {
		nr_pps_cfg->frm_hsize_in  = prm_top->org_hsize;
		nr_pps_cfg->frm_vsize_in  = di2pps ? prm_top->frm_vsize * 2 : prm_top->frm_vsize;
		nr_pps_cfg->frm_hsize_out = prm_dpe->dpe_nr_size.pps_hsize;
		nr_pps_cfg->frm_vsize_out = prm_dpe->dpe_nr_size.pps_vsize;
		//TODO  need check prm_top.dpss_mode;//1-slice mode;0-frame mode
		nr_pps_cfg->slc_mode      = 1;
		nr_pps_cfg->inst_sel      = 0; //reg cfg---0:aa_pps 1:lcevc
		nr_pps_cfg->apply_mode    = 0;
		//0:zoom up 2path out 1:zoom down 2path out 2:zoom up/down 1path out
		nr_pps_cfg->ds_mode       = nr_pps_cfg->pps_en;
		nr_pps_cfg->slc_num       = prm_top->dpe_slc_num;

		//manual config pps addr buffer per frame
		if (di2pps)
			w_reg_bit(DPSS_DPE_TOP_SW_SEL0, 1, 18, 1);
		else
			w_reg_bit(DPSS_DPE_TOP_SW_SEL0, 0, 18, 1);

		dbg_w2("%s:in:%d * %d,out:%d * %d.\n", __func__,
			nr_pps_cfg->frm_hsize_in,
			nr_pps_cfg->frm_vsize_in,
			nr_pps_cfg->frm_hsize_out,
			nr_pps_cfg->frm_vsize_out);
		u32 frm_hsize_d4;
		u32 frm_hsize_d4_d16;
		u32 frm_hsize_d4_d32;
		u32 frm_hsize_d4_d64;
		u32 frm_hsize_d4_d128;
		u32 frm_hsize_d4_rnd;
		u32 frm_hsize_sel;
		u32 frm_hsize_t0;
		u32 frm_hsize_t1;
		u32 frm_hsize_t2;
		u32 frm_hsize_m1;
		u32 slc_num;
		//int up_rate;
		//int up_rate_num = 3;
		//int up_rate_den = 2;
		u32 uprate = nr_pps_cfg->frm_hsize_out * 64 / nr_pps_cfg->frm_hsize_in;
		//u32 ext_pad;
		u32 hds_pad = (nr_pps_cfg->frm_hsize_in == nr_pps_cfg->frm_hsize_out) ? 4 : 0;
		u32 ext_pad_lf[4];
		u32 ext_pad_rt[4];

		slc_num = prm_top->slc_num;
		ext_pad_lf[0] = 0;
		ext_pad_lf[1] =
			(slc_num == 4) ? hds_pad : (slc_num == 2) ? hds_pad : 0;
		ext_pad_lf[2] = (slc_num == 4) ? hds_pad : 0;
		ext_pad_lf[3] = (slc_num == 4) ? hds_pad : 0;
		ext_pad_rt[0] =
			(slc_num == 4) ? hds_pad : (slc_num == 2) ? hds_pad : 0;
		ext_pad_rt[1] = (slc_num == 4) ? hds_pad : 0;
		ext_pad_rt[2] = (slc_num == 4) ? hds_pad : 0;
		ext_pad_rt[3] = 0;
		//ary no use	u32 frm_hsize_t3;

		frm_hsize_d4 = (prm_top->frm_hsize + 3) >> 2;
		frm_hsize_d4_d16 = (frm_hsize_d4 + 15) >> 4;
		frm_hsize_d4_d32 = (frm_hsize_d4 + 31) >> 5;
		frm_hsize_d4_d64 = (frm_hsize_d4 + 63) >> 6;
		frm_hsize_d4_d128 = (frm_hsize_d4 + 127) >> 7;
		frm_hsize_sel = prm_top->frm_hsize_sel;
		frm_hsize_d4_rnd = (frm_hsize_sel == 4) ? frm_hsize_d4_d128 << 7 :
				(frm_hsize_sel == 3) ? frm_hsize_d4_d64 << 6 :
				(frm_hsize_sel == 2) ? frm_hsize_d4_d32 << 5 :
				(frm_hsize_sel == 1) ? frm_hsize_d4_d16 << 4 :
				frm_hsize_d4;
		frm_hsize_t0 = frm_hsize_d4_rnd;
		frm_hsize_t1 = frm_hsize_d4_rnd << 1;
		frm_hsize_t2 = frm_hsize_t0 + frm_hsize_t1;
		frm_hsize_m1 = nr_pps_cfg->frm_hsize_out;
		//up_rate = nr_pps_cfg->frm_hsize_out / nr_pps_cfg->frm_hsize_in;

		if (pps_out2bbd_en) {
			nr_pps_cfg->slc_act_out_bgn[0] = 0;
			nr_pps_cfg->slc_act_out_end[0] =
				(slc_num == 4 ? frm_hsize_t0 * uprate / 64 :
				slc_num == 2 ? frm_hsize_t1 *
					uprate / 64 : frm_hsize_m1) + bbd_pad;

			nr_pps_cfg->slc_act_out_bgn[1] =
				(slc_num == 4 ? frm_hsize_t0 : frm_hsize_t1) *
				uprate / 64 - bbd_pad;
			nr_pps_cfg->slc_act_out_end[1] =
				slc_num == 4 ? frm_hsize_t1 * uprate + bbd_pad :
				frm_hsize_m1;
			nr_pps_cfg->slc_act_out_bgn[2] =
				frm_hsize_t1 * uprate / 64 - bbd_pad;
			nr_pps_cfg->slc_act_out_end[2] =
				frm_hsize_t2 * uprate / 64 + bbd_pad;
			nr_pps_cfg->slc_act_out_bgn[3] =
				frm_hsize_t2 * uprate / 64 - bbd_pad;
			nr_pps_cfg->slc_act_out_end[3] = frm_hsize_m1;
		} else {
			nr_pps_cfg->slc_act_out_bgn[0] = 0;
			nr_pps_cfg->slc_act_out_end[0] =
				slc_num == 4 ? frm_hsize_t0 * uprate / 64 :
					slc_num == 2 ? frm_hsize_t1 *
						uprate / 64 : frm_hsize_m1;

			nr_pps_cfg->slc_act_out_bgn[1] =
				slc_num == 4 ? frm_hsize_t0 * uprate / 64 :
				frm_hsize_t1 * uprate / 64;
			nr_pps_cfg->slc_act_out_end[1] =
				slc_num == 4 ? frm_hsize_t1 * uprate / 64 :
				frm_hsize_m1;
			nr_pps_cfg->slc_act_out_bgn[2] = frm_hsize_t1 * uprate / 64;
			nr_pps_cfg->slc_act_out_end[2] = frm_hsize_t2 * uprate / 64;
			nr_pps_cfg->slc_act_out_bgn[3] = frm_hsize_t2 * uprate / 64;
			nr_pps_cfg->slc_act_out_end[3] = frm_hsize_m1;
		}

		/*
		 *nr_pps_cfg->slc_act_out_bgn[0] = 0 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_bgn[1] = 1 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_bgn[2] = 2 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_bgn[3] = 3 * slc_out_size;

		 *nr_pps_cfg->slc_act_out_end[0] = 1 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_end[1] = 2 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_end[2] = 3 * slc_out_size;
		 *nr_pps_cfg->slc_act_out_end[3] = nr_pps_cfg->frm_hsize_out;
		 */

		//reg_hds_sel = 3;
		//reg_vds_sel = 1;
		//reg_hvds_sel = 1;
		dbg_h2("%s:pps0:%d%d,pps1:%d%d,pps2:%d%d,pps3:%d%d.\n",
			__func__,
			nr_pps_cfg->slc_act_out_bgn[0],
			nr_pps_cfg->slc_act_out_end[0],
			nr_pps_cfg->slc_act_out_bgn[1],
			nr_pps_cfg->slc_act_out_end[1],
			nr_pps_cfg->slc_act_out_bgn[2],
			nr_pps_cfg->slc_act_out_end[2],
			nr_pps_cfg->slc_act_out_bgn[3],
			nr_pps_cfg->slc_act_out_end[3]);

		skip_dsx = 1;
		skip_dsy = 1;
		aa_pps_top(nr_pps_cfg);
		//ds wrmif path
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC0, nr_pps_cfg->slc_in_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC0, nr_pps_cfg->slc_in_end[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC1, nr_pps_cfg->slc_in_bgn[1], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC1, nr_pps_cfg->slc_in_end[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC2, nr_pps_cfg->slc_in_bgn[2], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC2, nr_pps_cfg->slc_in_end[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC3, nr_pps_cfg->slc_in_bgn[3], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC3, nr_pps_cfg->slc_in_end[3] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 11, 1);
		//aa_pps output slice size,with overlap
		//ext_pad = (nr_pps_cfg->frm_hsize_in == nr_pps_cfg->frm_hsize_out) ? 4 : 0;
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC0,
			nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC0,
			nr_pps_cfg->slc_act_out_end[0] + ext_pad_rt[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC1,
			nr_pps_cfg->slc_act_out_bgn[1] - ext_pad_lf[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC1,
			nr_pps_cfg->slc_act_out_end[1] + ext_pad_rt[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC2,
			nr_pps_cfg->slc_act_out_bgn[2] - ext_pad_lf[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC2,
			nr_pps_cfg->slc_act_out_end[2] + ext_pad_rt[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC3,
			nr_pps_cfg->slc_act_out_bgn[3] - ext_pad_lf[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC3,
			nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC0,
			nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC0,
			nr_pps_cfg->slc_act_out_end[0] + ext_pad_rt[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC1,
			nr_pps_cfg->slc_act_out_bgn[1] - ext_pad_lf[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC1,
			nr_pps_cfg->slc_act_out_end[1] + ext_pad_rt[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC2,
			nr_pps_cfg->slc_act_out_bgn[2] - ext_pad_lf[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC2,
			nr_pps_cfg->slc_act_out_end[2] + ext_pad_rt[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC3,
			nr_pps_cfg->slc_act_out_bgn[3] - ext_pad_lf[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC3,
			nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
		//aa_pps output slice size,without overlap,to wrmif
		if (pps_out2bbd_en) {
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
			nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
				nr_pps_cfg->slc_act_out_end[0] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
				nr_pps_cfg->slc_act_out_bgn[1] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
				nr_pps_cfg->slc_act_out_end[1] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
				nr_pps_cfg->slc_act_out_bgn[2] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
				nr_pps_cfg->slc_act_out_end[2] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
				nr_pps_cfg->slc_act_out_bgn[3] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
				nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
				nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
				nr_pps_cfg->slc_act_out_end[0] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
				nr_pps_cfg->slc_act_out_bgn[1] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
				nr_pps_cfg->slc_act_out_end[1] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
				nr_pps_cfg->slc_act_out_bgn[2] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
				nr_pps_cfg->slc_act_out_end[2] - 1 - bbd_pad, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
				nr_pps_cfg->slc_act_out_bgn[3] + bbd_pad, 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
				nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
		} else {
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
			nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
				nr_pps_cfg->slc_act_out_end[0] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
				nr_pps_cfg->slc_act_out_bgn[1], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
				nr_pps_cfg->slc_act_out_end[1] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
				nr_pps_cfg->slc_act_out_bgn[2], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
				nr_pps_cfg->slc_act_out_end[2] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
				nr_pps_cfg->slc_act_out_bgn[3], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
				nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
				nr_pps_cfg->slc_act_out_bgn[0], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
				nr_pps_cfg->slc_act_out_end[0] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
				nr_pps_cfg->slc_act_out_bgn[1], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
				nr_pps_cfg->slc_act_out_end[1] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
				nr_pps_cfg->slc_act_out_bgn[2], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
				nr_pps_cfg->slc_act_out_end[2] - 1, 16, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
				nr_pps_cfg->slc_act_out_bgn[3], 0, 13);
			w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
				nr_pps_cfg->slc_act_out_end[3] - 1, 16, 13);
		}
		//aa_pps output frame size,without overlap,to wrmif
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT_SIZE,
			nr_pps_cfg->frm_vsize_out, 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT_SIZE,
			nr_pps_cfg->frm_hsize_out, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_SIZE,
			nr_pps_cfg->frm_vsize_out, 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_SIZE,
			nr_pps_cfg->frm_hsize_out, 16, 13);
	//	w_reg_bit(FRC_AA_PPS_COMP,reg_hvds_sel,20,1);
	//	w_reg_bit(FRC_AA_PPS_COMP,reg_vds_sel,21,2);
	//	w_reg_bit(FRC_AA_PPS_COMP,reg_hds_sel,23,2);
		w_reg_bit(DPSS_DPE_INTF_CTRL2, skip_dsx, 8, 2);
		w_reg_bit(DPSS_DPE_INTF_CTRL2, skip_dsy, 12, 2);

		if (pps_out2bbd_en) {
			wr(DPSS_DPE_BBD_IN_CFG, prm_top->dpe_out2bbd_mode << 4 | //bbd_in_dpath_sel
				7); //bbd_in_sw_cfg,0-bgn/end,1-ovlp,2-frm size
			//wr(DPSS_DPE_BBD_IN_X_SLC0, (1*slc_out_size - 1)<<16 | 0*slc_out_size);
			//wr(DPSS_DPE_BBD_IN_X_SLC1, (2*slc_out_size - 1)<<16 | 1*slc_out_size);
			//wr(DPSS_DPE_BBD_IN_X_SLC2, (3*slc_out_size - 1)<<16 | 2*slc_out_size);
			//wr(DPSS_DPE_BBD_IN_X_SLC3, (4*slc_out_size - 1)<<16 | 3*slc_out_size);
			wr(DPSS_DPE_BBD_IN_X_SLC0,
				(nr_pps_cfg->slc_act_out_end[0] - 1 - bbd_pad) << 16 |
					nr_pps_cfg->slc_act_out_bgn[0]);
			wr(DPSS_DPE_BBD_IN_X_SLC1,
				(nr_pps_cfg->slc_act_out_end[1] - 1 - bbd_pad) << 16 |
				(nr_pps_cfg->slc_act_out_bgn[1] + bbd_pad));
			wr(DPSS_DPE_BBD_IN_X_SLC2,
				(nr_pps_cfg->slc_act_out_end[2] - 1 - bbd_pad) << 16 |
				(nr_pps_cfg->slc_act_out_bgn[2] + bbd_pad));
			wr(DPSS_DPE_BBD_IN_X_SLC3,
				(nr_pps_cfg->slc_act_out_end[3] - 1) << 16 |
				(nr_pps_cfg->slc_act_out_bgn[3] + bbd_pad));

			bbd_slc_in_lft_ovlp[0] = 0;
			bbd_slc_in_lft_ovlp[1] = bbd_pad;//2;
			bbd_slc_in_lft_ovlp[2] = bbd_pad;//2;
			bbd_slc_in_lft_ovlp[3] = bbd_pad;//2;
			bbd_slc_in_rgt_ovlp[0] = bbd_pad;//2;
			bbd_slc_in_rgt_ovlp[1] = bbd_pad;//2;
			bbd_slc_in_rgt_ovlp[2] = bbd_pad;//2;
			bbd_slc_in_rgt_ovlp[3] = 0;

			wr(DPSS_DPE_BBD_IN_SLC_OVLP01, bbd_slc_in_rgt_ovlp[1] << 24 |
				bbd_slc_in_lft_ovlp[1] << 16 |
				bbd_slc_in_rgt_ovlp[0] << 8 |
				bbd_slc_in_lft_ovlp[0]);
			wr(DPSS_DPE_BBD_IN_SLC_OVLP23, bbd_slc_in_rgt_ovlp[3] << 24 |
				bbd_slc_in_lft_ovlp[3] << 16 |
				bbd_slc_in_rgt_ovlp[2] << 8 |
				bbd_slc_in_lft_ovlp[2]);
			wr(DPSS_DPE_BBD_IN_FRM_SIZE, nr_pps_cfg->frm_hsize_out << 16 |
				nr_pps_cfg->frm_vsize_out);
			cfg_dpss_mc_bbd(nr_pps_cfg->frm_hsize_out, nr_pps_cfg->frm_vsize_out,
				prm_top->dpe_out2bbd_en,//bbd_en
				2, //bbd_sel,0-mc rdmif0,1-mc rdmif1,2-nr rdmif
				nr_pps_cfg->frm_hsize_out, nr_pps_cfg->frm_vsize_out);
		}
	}

//==================================================
//NR_OUT pps scale down
//==================================================

	if (prm_top->nr_aapps_ds) {
		struct AA_PPS_TOP_TYPE nr_pps_cfg;

		nr_pps_cfg.frm_hsize_in  = prm_top->frm_hsize;
		nr_pps_cfg.frm_vsize_in  = prm_top->frm_vsize;
		nr_pps_cfg.frm_hsize_out = prm_dpe->dpe_nr_size.pps_hsize;
		nr_pps_cfg.frm_vsize_out = prm_dpe->dpe_nr_size.pps_vsize;

		nr_pps_cfg.slc_mode      = 0;
		nr_pps_cfg.inst_sel      = 0; //reg cfg---0:aa_pps 1:lcevc
		nr_pps_cfg.apply_mode    = 2;
		//0:zoom up 2path out 1:zoom down 2path out 2:zoom up/down 1path out
		nr_pps_cfg.ds_mode       = prm_top->nr_aapps_ds;
		nr_pps_cfg.slc_num       = 1;

		nr_pps_cfg.slc_act_out_bgn[0] = 0;
		nr_pps_cfg.slc_act_out_bgn[1] = 0;
		nr_pps_cfg.slc_act_out_bgn[2] = 0;
		nr_pps_cfg.slc_act_out_bgn[3] = 0;

		nr_pps_cfg.slc_act_out_end[0] = nr_pps_cfg.frm_hsize_out;
		nr_pps_cfg.slc_act_out_end[1] = nr_pps_cfg.frm_hsize_out;
		nr_pps_cfg.slc_act_out_end[2] = nr_pps_cfg.frm_hsize_out;
		nr_pps_cfg.slc_act_out_end[3] = nr_pps_cfg.frm_hsize_out;

		aa_pps_top(&nr_pps_cfg);
	}

	if (prm_top->nr_aapps_on) {
		struct AA_PPS_TOP_TYPE nr_pps_cfg;

		nr_pps_cfg.frm_hsize_in  = prm_top->frm_hsize;
		nr_pps_cfg.frm_vsize_in  = prm_top->frm_vsize;
		nr_pps_cfg.frm_hsize_out = prm_dpe->dpe_nr_size.pps_hsize;
		nr_pps_cfg.frm_vsize_out = prm_dpe->dpe_nr_size.pps_vsize;

		nr_pps_cfg.slc_mode      = (prm_top->dpe_slc_num == 1) ? 0 : 1;
		nr_pps_cfg.inst_sel      = 0; //reg cfg---0:aa_pps 1:lcevc
		nr_pps_cfg.apply_mode    = prm_top->nr_aapps_mode;
		//0:zoom up 2path out 1:zoom down 2path out 2:zoom up/down 1path out
		nr_pps_cfg.ds_mode       = prm_top->nr_aapps_on;
		nr_pps_cfg.slc_num       = prm_top->dpe_slc_num;

		nr_pps_cfg.slc_act_out_bgn[0] = 0 * slc_out_size;
		nr_pps_cfg.slc_act_out_bgn[1] = 1 * slc_out_size;
		nr_pps_cfg.slc_act_out_bgn[2] = 2 * slc_out_size;
		nr_pps_cfg.slc_act_out_bgn[3] = 3 * slc_out_size;

		nr_pps_cfg.slc_act_out_end[0] = 1 * slc_out_size;
		nr_pps_cfg.slc_act_out_end[1] = 2 * slc_out_size;
		nr_pps_cfg.slc_act_out_end[2] = 3 * slc_out_size;
		nr_pps_cfg.slc_act_out_end[3] = nr_pps_cfg.frm_hsize_out;

		slc_act_out_bgn1[0] = 0 * slc_out_size1;
		slc_act_out_bgn1[1] = 1 * slc_out_size1;
		slc_act_out_bgn1[2] = 2 * slc_out_size1;
		slc_act_out_bgn1[3] = 3 * slc_out_size1;

		slc_act_out_end1[0] = 1 * slc_out_size1;
		slc_act_out_end1[1] = 2 * slc_out_size1;
		slc_act_out_end1[2] = 3 * slc_out_size1;
		slc_act_out_end1[3] = prm_top->frm_hsize;

		aa_pps_top(&nr_pps_cfg);

		skip_dsx = prm_top->nr_aapps_mode == 0 ? 1 : 0;
		//	prm_top->nr_aapps_mode == 1 ? 0 : 0;
		skip_dsy = prm_top->nr_aapps_mode == 0 ? 1 :
			prm_top->nr_aapps_mode == 1 ? 1 : 0;
		w_reg_bit(DPSS_DPE_INTF_CTRL2, skip_dsx, 8, 2);
		w_reg_bit(DPSS_DPE_INTF_CTRL2, skip_dsy, 12, 2);

		//ds wrmif path
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC0, nr_pps_cfg.slc_in_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC0, nr_pps_cfg.slc_in_end[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC1, nr_pps_cfg.slc_in_bgn[1], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC1, nr_pps_cfg.slc_in_end[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC2, nr_pps_cfg.slc_in_bgn[2], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC2, nr_pps_cfg.slc_in_end[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC3, nr_pps_cfg.slc_in_bgn[3], 0, 13);
		w_reg_bit(DPSS_DPE_DS_IN_X_SLC3, nr_pps_cfg.slc_in_end[3] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 11, 1);

		//ds path 4k2k->vds-hds 960*540
		//cut in
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC0,
			nr_pps_cfg.slc_act_out_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC0,
			nr_pps_cfg.slc_act_out_end[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC1,
			nr_pps_cfg.slc_act_out_bgn[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC1,
			nr_pps_cfg.slc_act_out_end[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC2,
			nr_pps_cfg.slc_act_out_bgn[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC2,
			nr_pps_cfg.slc_act_out_end[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC3,
			nr_pps_cfg.slc_act_out_bgn[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT0_X_SLC3,
			nr_pps_cfg.slc_act_out_end[3] - 1, 16, 13);

		//cut out
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
			nr_pps_cfg.slc_act_out_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC0,
			nr_pps_cfg.slc_act_out_end[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
			nr_pps_cfg.slc_act_out_bgn[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC1,
			nr_pps_cfg.slc_act_out_end[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
			nr_pps_cfg.slc_act_out_bgn[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC2,
			nr_pps_cfg.slc_act_out_end[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
			nr_pps_cfg.slc_act_out_bgn[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT1_X_SLC3,
			nr_pps_cfg.slc_act_out_end[3] - 1, 16, 13);

		w_reg_bit(DPSS_DPE_PATH_PPS_OUT_SIZE, nr_pps_cfg.frm_vsize_out, 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT_SIZE, nr_pps_cfg.frm_hsize_out, 16, 13);

		//pps path, 4k2k->vds 4k1k
		//cut in
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC0,
			nr_pps_cfg.slc_in_bgn[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC0,
			nr_pps_cfg.slc_in_end[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC1,
			nr_pps_cfg.slc_in_bgn[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC1,
			nr_pps_cfg.slc_in_end[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC2,
			nr_pps_cfg.slc_in_bgn[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC2,
			nr_pps_cfg.slc_in_end[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC3,
			nr_pps_cfg.slc_in_bgn[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_X_SLC3,
			nr_pps_cfg.slc_in_end[3] - 1, 16, 13);

		//cut out
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
			slc_act_out_bgn1[0], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC0,
			slc_act_out_end1[0] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
			slc_act_out_bgn1[1], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC1,
			slc_act_out_end1[1] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
			slc_act_out_bgn1[2], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC2,
			slc_act_out_end1[2] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
			slc_act_out_bgn1[3], 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT3_X_SLC3,
			slc_act_out_end1[3] - 1, 16, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_SIZE,
			nr_pps_cfg.frm_vsize_out, 0, 13);
		w_reg_bit(DPSS_DPE_PATH_PPS_OUT2_SIZE,
			prm_top->frm_vsize >> 1, 16, 13);

		if (pps_out2bbd_en) {
			u32 bbd_frm_hsize = prm_top->frm_hsize;
			u32 bbd_frm_vsize = prm_top->frm_vsize / 2;

			wr(DPSS_DPE_BBD_IN_CFG, prm_top->dpe_out2bbd_mode << 4 | //bbd_in_dpath_sel
				7); //bbd_in_sw_cfg,0-bgn/end,1-ovlp,2-frm size
			wr(DPSS_DPE_BBD_IN_X_SLC0,
				(1 * slc_out_size1 - 1) << 16 | 0 * slc_out_size1);
			wr(DPSS_DPE_BBD_IN_X_SLC1,
				(2 * slc_out_size1 - 1) << 16 | 1 *  slc_out_size1);
			wr(DPSS_DPE_BBD_IN_X_SLC2,
				(3 * slc_out_size1 - 1) << 16 | 2 * slc_out_size1);
			wr(DPSS_DPE_BBD_IN_X_SLC3,
				(4 * slc_out_size1 - 1) << 16 | 3 * slc_out_size1);

			bbd_slc_in_lft_ovlp[0] = 0;
			bbd_slc_in_lft_ovlp[1] = 1 * slc_out_size1 - nr_pps_cfg.slc_in_bgn[1];
			bbd_slc_in_lft_ovlp[2] = 2 * slc_out_size1 - nr_pps_cfg.slc_in_bgn[2];
			bbd_slc_in_lft_ovlp[3] = 3 * slc_out_size1 - nr_pps_cfg.slc_in_bgn[3];

			bbd_slc_in_rgt_ovlp[0] = nr_pps_cfg.slc_in_end[0] - 1 * slc_out_size1;
			bbd_slc_in_rgt_ovlp[1] = nr_pps_cfg.slc_in_end[1] - 2 * slc_out_size1;
			bbd_slc_in_rgt_ovlp[2] = nr_pps_cfg.slc_in_end[2] - 3 * slc_out_size1;
			bbd_slc_in_rgt_ovlp[3] = 0;

			wr(DPSS_DPE_BBD_IN_SLC_OVLP01, bbd_slc_in_rgt_ovlp[1] << 24 |
				bbd_slc_in_lft_ovlp[1] << 16 |
				bbd_slc_in_rgt_ovlp[0] << 8 |
				bbd_slc_in_lft_ovlp[0]);
			wr(DPSS_DPE_BBD_IN_SLC_OVLP23, bbd_slc_in_rgt_ovlp[3] << 24 |
				bbd_slc_in_lft_ovlp[3] << 16 |
				bbd_slc_in_rgt_ovlp[2] << 8 |
				bbd_slc_in_lft_ovlp[2]);
			wr(DPSS_DPE_BBD_IN_FRM_SIZE, bbd_frm_hsize << 16 |
				bbd_frm_vsize);
			cfg_dpss_mc_bbd(bbd_frm_hsize, bbd_frm_vsize,
				prm_top->dpe_out2bbd_en,//bbd_en
				2, //bbd_sel,0-mc rdmif0,1-mc rdmif1,2-nr rdmif
				bbd_frm_hsize, bbd_frm_vsize);
		}
	}
	w_reg_bit(DPSS_DPE_INTF_CTRL2, 7, 0, 3); //default
	if ((prm_top->mode_drct2 || prm_top->mode_drct) &&
	    !prm_top->src_mode &&
	    dpe_mode == DPE_NR_BYPS)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 6, 0, 3);
	dbg_h2("direct:0x%x\n", rd(DPSS_DPE_INTF_CTRL2));

//==================================================
// CFG DPSS_VPU_VBE_TOP
//==================================================
	nr_pps_cfg->aa_pad = aa_pad;
	prm_dpe->aa_pad = aa_pad;
	if  (dpss_nr_debug || dpss_force_nr_debug) {
		nr_pps_cfg->aa_pad = 0;
		prm_dpe->aa_pad = 0;
		prm_dpe->aa_pad_assign_en = 1;
	}

	if (dpss_force_nr_debug > 0xf)
		prm_dpe->dpe_reg_mode.vfce_reg_mode = 1;
	cfg_dpss_vdi(prm_top, prm_dpe, nr_pps_cfg); //nr+di
//==================================================
// CFG DPSS_DPE_RO_CHK
//==================================================

	if (prm_top->bbd_parallel) {
		w_reg_bit(DPSS_DPE_RDMA_INFO1, 0x1e, 0, 8); // close  bbd ro2ddr
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 14, 1);//di+bbd_done_mask
	} else if (prm_top->dpe_ro_wdma_en && !reg_nr_byps_en) {
		w_reg_bit(DPSS_DPE_RDMA_INFO1, 0x1f, 0, 8);
	}
//==================================================
// CFG LCEVC
//==================================================
	if (prm_top->lcevc_en) {
		dbg_h2("cfg_lcevc_top() start");
		last_lcevc = 1;
		cfg_lcevc_top(prm_top->lcevc_en, //lcevc_en,
			prm_top->lcevc.src1_frm_hsize, //1920 //src1_frm_hsize,
			//normal_afbcd_hsize, input
			prm_top->lcevc.src1_frm_vsize, //1080 //src1_frm_vsize,
			prm_top->lcevc.src1_head_ybaddr,//0x6200000,
			//src1_head_baddr,//word addr
			prm_top->lcevc.src1_body_ybaddr,//0x7200000,
			//src1_body_baddr,
			prm_top->lcevc.src1_head_cbaddr,//0x6200000,
			//src1_head_baddr,//word addr
			prm_top->lcevc.src1_body_cbaddr,//0x7200000,
			//src1_body_baddr,
			prm_top->lcevc.src1_is_cmpr,
			prm_top->lcevc.src1_bit,
			prm_top->lcevc.src2_frm_hsize,
			//3840,//src2_frm_hsize, //luma_only_hsize, input
			prm_top->lcevc.src2_frm_vsize,//2160,//src2_frm_vsize,
			prm_top->lcevc.src2_ybaddr,//0x700000,//src2_ybaddr,//word baddr
			prm_top->lcevc.src2_cbaddr,//0x700000,//src2_cbaddr,

			prm_top->lcevc.frm_hsize_out,//3840//frm_hsize_out,
			prm_top->lcevc.frm_vsize_out,//2160//frm_vsize_out,
			1,//inst_sel,
			2,//apply_mode,
			1,//ds_mode, //0:aa mode 1:pps mode
			1,//slc_mode, //0:frame 1:slice
			4,//slc_num, //2/4 slice
			0,//bld_din0_premult_en,
			0,//bld_din1_premult_en
			0x100,//bld_din0_alpha,
			0x50,//bld_din1_alpha ,
			1,//bld_src0_sel,
			//1:lay1 chose src0  2:lay1 chose src1  else :close blend lay1
			2,
			prm_top->lcevc.dbg_cfg);//bld_src1_sel,
			//1:lay2 chose src0  2:lay2 chose src1  else :close blend lay2
	} else if (last_lcevc) {
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 0, 16, 1);
		//bit16 = 0: auto vfcd0/1/2   bit 16= 1：mannal switch DPSS_DPE_PATH_CTRL
		w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 0, 8, 1);
		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 0, 31, 1);
		w_reg_bit_vc(MIF0_RMIF_CTRL1 + (0x67 - 0x65) * 256, 0, 27, 2);
		last_lcevc = 0;
		dbg_h2("not lcevc, do reset\n");
	}

	if (prm_top->dpe_out2bbd_en == 1 && prm_top->dpe_out2bbd_mode <= 1) {
		u32 bbd_slc_in_lft_ovlp[4];
		u32 bbd_slc_in_rgt_ovlp[4];

		dbg_h0("mode slc<%d %d>\n", prm_top->dpe_out2bbd_mode, prm_top->dpe_slc_num);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 11, 1);
		wr(DPSS_DPE_BBD_IN_CFG, prm_top->dpe_out2bbd_mode << 4 | //bbd_in_dpath_sel
		6); //bbd_in_sw_cfg,0-bgn/end,1-ovlp,2-frm size
		bbd_slc_in_lft_ovlp[0] = 0;
		bbd_slc_in_lft_ovlp[1] = aa_pad;//2;
		bbd_slc_in_lft_ovlp[2] = aa_pad;//2;
		bbd_slc_in_lft_ovlp[3] = aa_pad;//2;

		bbd_slc_in_rgt_ovlp[0] = prm_top->dpe_slc_num == 1 ? 0 : aa_pad;//2;
		bbd_slc_in_rgt_ovlp[1] = prm_top->dpe_slc_num == 2 ? 0 : aa_pad;//2;
		bbd_slc_in_rgt_ovlp[2] = aa_pad;//2;
		bbd_slc_in_rgt_ovlp[3] = 0;

		wr(DPSS_DPE_BBD_IN_SLC_OVLP01, bbd_slc_in_rgt_ovlp[1] << 24 |
			bbd_slc_in_lft_ovlp[1] << 16 |
			bbd_slc_in_rgt_ovlp[0] << 8 |
			bbd_slc_in_lft_ovlp[0]);
		wr(DPSS_DPE_BBD_IN_SLC_OVLP23, bbd_slc_in_rgt_ovlp[3] << 24 |
			bbd_slc_in_lft_ovlp[3] << 16 |
			bbd_slc_in_rgt_ovlp[2] << 8 |
			bbd_slc_in_lft_ovlp[2]);
		if (prm_top->dpe_out2bbd_mode == 1) {
			wr(DPSS_DPE_BBD_IN_FRM_SIZE, prm_top->frm_hsize << 16 |
			prm_top->frm_vsize * 2);
		} else {
			wr(DPSS_DPE_BBD_IN_FRM_SIZE, prm_top->frm_hsize << 16 |
			prm_top->frm_vsize);
		}
	}
//dv2bbd
	if (prm_top->dpe_out2bbd_en == 1 && prm_top->dpe_out2bbd_mode == 2) {
		u32 bbd_slc_in_lft_ovlp[4];
		u32 bbd_slc_in_rgt_ovlp[4];

		wr(DPSS_DPE_BBD_IN_CFG, prm_top->dpe_out2bbd_mode << 4 | //bbd_in_dpath_sel
			2); //bbd_in_sw_cfg,0-bgn/end,1-ovlp,2-frm size
		bbd_slc_in_lft_ovlp[0] = 0;
		bbd_slc_in_lft_ovlp[1] = aa_pad;//2;
		bbd_slc_in_lft_ovlp[2] = aa_pad;//2;
		bbd_slc_in_lft_ovlp[3] = aa_pad;//2;

		bbd_slc_in_rgt_ovlp[0] = prm_top->dpe_slc_num == 1 ? 0 :  aa_pad;//2;
		bbd_slc_in_rgt_ovlp[1] = prm_top->dpe_slc_num == 2 ? 0 :  aa_pad;//2;
		bbd_slc_in_rgt_ovlp[2] = aa_pad;//2;
		bbd_slc_in_rgt_ovlp[3] = 0;

		wr(DPSS_DPE_BBD_IN_SLC_OVLP01, bbd_slc_in_rgt_ovlp[1] << 24 |
			bbd_slc_in_lft_ovlp[1] << 16 |
			bbd_slc_in_rgt_ovlp[0] << 8 |
			bbd_slc_in_lft_ovlp[0]);
		wr(DPSS_DPE_BBD_IN_SLC_OVLP23, bbd_slc_in_rgt_ovlp[3] << 24 |
			bbd_slc_in_lft_ovlp[3] << 16 |
			bbd_slc_in_rgt_ovlp[2] << 8 |
			bbd_slc_in_lft_ovlp[2]);
	}
	if (nr_di2bbd | dv2bbd) {
		if (dv2bbd) {
			w_reg_bit(DPSS_DPE_RDMA_INFO1, 0x1e, 0, 8); // close  bbd ro2ddr
			w_reg_bit(DPSS_DPE_HW_DBG, 1, 14, 1);//dv+bbd_done_mask
		}
		if (di2bbd) {
			cfg_dpss_mc_bbd(prm_top->frm_hsize, prm_top->frm_vsize * 2,
					prm_top->dpe_out2bbd_en,//bbd_en
					2, //bbd_sel,0-mc rdmif0,1-mc rdmif1,2-nr rdmif
					prm_top->frm_hsize, prm_top->frm_vsize * 2);
			//not cfg bbd cause src0&src1 conflict
			w_reg_bit(DPSS_DPE_BBD_CTRL, 2, 0, 2);
			//bbd_sel,0-mc rdmif0,1-mc rdmif1,2-nr rdmif

			u32 reg_vdi_src_idx = (rd(DPSS_TOP_FUNC_CTRL) >> 24) & 0xF;

			w_reg_bit(DPSS_BBD_ONLY_CTRL, reg_vdi_src_idx, 4, 4);//bbd_src_idx
			w_reg_bit(DPSS_DPE_RDMA_WR_SIZE, 1, 16, 1);
			w_reg_bit(DPSS_DPE_RDMA_WR_SIZE, 88, 0, 12);
		} else {
			cfg_dpss_mc_bbd(prm_top->frm_hsize, prm_top->frm_vsize,
				prm_top->dpe_out2bbd_en,//bbd_en
				2, //bbd_sel,0-mc rdmif0,1-mc rdmif1,2-nr rdmif
				prm_top->frm_hsize, prm_top->frm_vsize);
		}
	}
	dbg_h2("dpe end\n");//tmp for sim
}

struct PRM_INTF_TYPE pix_rmif;
struct VFCD_t vfcd0;
struct VFCD_t vfcd1;

//cfg_dpss_dpe_intf
void hw_cfg_dpss_dpe_intf(struct PRM_DPSS_TOP *prm_top,
		struct PRM_DPSS_DPE *prm_dpe, bool pps_en)
{
	int i;
	u32 dw_dsx = prm_top->dpe_dw_dsx;//double_wr
	u32 dw_dsy = prm_top->dpe_dw_dsy;
	u32 reg_mc_pix_fmt = prm_top->src_fs_fmt.src_fmt;//rdmif fmt 0:444 1:422 2:420
	u32 reg_vbe_pix_fmt = prm_top->src_fs_fmt.src_fmt == YUV422;
	//u32 frm_end_mask = 0b111110;//{ds_mix,dblk_h,dblk_v,dmsq_wr,nrwr,diwr}
	u32 big_endian = 0, rev_x = 0, temp_uv;
	unsigned int crr_idx; //07-10
	bool dis_s_afce = false;
	enum DPSS_WORK_MODE dpe_mode = prm_dpe->dpe_mode;
	//w_reg_bit(DPSS_DPE_INTF_DBG, frm_end_mask&0xffff, 0, 16); //reg_frm_end_mask
	//==============================================================//
	//pix_rdmif
	//==============================================================//
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, reg_mc_pix_fmt, 2, 2);//reg_mc_pix_fmt
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, reg_vbe_pix_fmt, 0, 2);//reg_vbe_pix_fmt
	int p010_mode = prm_top->src_fs_fmt.src_bit == BIT_P010;
	//int bit8_mode = prm_top.src_fs_fmt.src_bit == BIT_008;
	w_reg_bit(DPSS_DPE_VFCD_FMT, prm_top->src_fs_fmt.src_fmt << 6 |
					prm_top->src_fs_fmt.src_fmt << 4 |
					prm_top->src_fs_fmt.src_fmt << 2 |
					prm_top->src_fs_fmt.src_fmt << 0, 0, 8);
	struct PRM_INTF_TYPE *pix_rmif; //ary change
	//memset((void *)(&pix_rmif), 0, sizeof(struct PRM_INTF_TYPE));
	//pix_rmif = &g_pix_rmif;
	pix_rmif  = &prm_dpe->pix_rmif_tmp;
	pix_rmif->src_fmt = prm_top->src_fs_fmt.src_fmt;//YUV444/YUV422/YUV420/RGB
	pix_rmif->src_plan = prm_top->src_fs_fmt.src_plan;//PLANAR_X1/X2
	pix_rmif->src_bit = prm_top->src_fs_fmt.src_bit;//BIT_8/10
	pix_rmif->src_cmpr = CMPR_UN;//un/afbc/afrc
	pix_rmif->interlace = IS_PSRC;//IS_PSRC/IS_ISRC
	pix_rmif->swap_64bit = prm_top->swap_64bit;
	pix_rmif->little_endian = prm_top->l_endian;
	pix_rmif->block_mode = prm_top->block_mode;
	//pix_rmif->uv_swap = prm_top->uv_swap;
	pix_rmif->slc_num     =  prm_top->dpe_slc_num;
	pix_rmif->src_hsize = prm_top->frm_hsize;
	pix_rmif->src_vsize = prm_top->frm_vsize;
	pix_rmif->reverse[0] = 0; //[0]:x [1]:y
	pix_rmif->reverse[1] = 0; //[0]:x [1]:y
	//pix_rmif.skip = {0,0}; //[0]:x [1]:y
	//pix_rmif.bits_mode;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	//   pix_rmif.pack_mode;    //0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	pix_rmif->mmu_mode_en     =  prm_top->mif_mmu_mode_en;
	pix_rmif->pad_en          =  prm_top->auto_alig_en;
	pix_rmif->pad_hmode       =  prm_top->alig_hmode;
	pix_rmif->pad_vmode       =  prm_top->alig_vmode;

	if (dpss_dbg_uv_swap & C_BIT4)
		pix_rmif->uv_swap = true;

	dbg_h2("dpe rd1 uv sw:%d\n", pix_rmif->uv_swap);
	//pix_rmif->little_endian = 1; //ary default is 1
	u32 dpe_src0_idx = rd(DPSS_FBUF_DPE_LOOP_IDX);
	u32 dpe_src1_idx = rd(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DPE_LOOP_IDX);
#ifdef ARY_SIM
	if (g_dpe_index == 0)
		dpe_src0_idx = 0x000000;
	else if (g_dpe_index == 1)
		dpe_src0_idx = 0x01000010;
	else if (g_dpe_index == 2)
		dpe_src0_idx = 0x02000020;
	else if (g_dpe_index == 3)
		dpe_src0_idx = 0x03100030;
	else if (g_dpe_index == 4)
		dpe_src0_idx = 0x04210040;
	else if (g_dpe_index == 5)
		dpe_src0_idx = 0x05320050;
	//dpe_src0_idx = g_dpe_index; //tmp
	dbg_h2("%s:ary_sim:dpe_src0_idx:%d->%d\n", __func__, g_dpe_index, dpe_src0_idx);
#endif

	u32 dpe_src0_cur0_rd_idx = (dpe_src0_idx >> 24) & 0xf;
	u32 dpe_src0_pre1_rd_idx = (dpe_src0_idx >> 20) & 0xf;
	u32 dpe_src0_pre2_rd_idx = (dpe_src0_idx >> 16) & 0xf;
	u32 dpe_src0_nr_wr_idx   = (dpe_src0_idx >> 4) & 0xf;
	u32 dpe_src0_di_wr_idx	 = (dpe_src0_idx >> 0) & 0xf;
	u32 dpe_src1_cur0_rd_idx = (dpe_src1_idx >> 24) & 0xf;
	u32 dpe_src1_pre1_rd_idx = (dpe_src1_idx >> 20) & 0xf;
	u32 dpe_src1_pre2_rd_idx = (dpe_src1_idx >> 16) & 0xf;
	u32 dpe_src1_nr_wr_idx   = (dpe_src1_idx >> 4) & 0xf;
	u32 dpe_src1_di_wr_idx   = (dpe_src1_idx >> 0) & 0xf;

	dbg_h2("%s:idx:wr:%d:%d,%d,%d,%d\n", __func__,
		dpe_src0_nr_wr_idx,
		dpe_src0_di_wr_idx,
		dpe_src0_cur0_rd_idx,
		dpe_src0_pre1_rd_idx,
		dpe_src0_pre2_rd_idx);

	u32 fmt444_out = 1;//(dpe_mode!=DPE_MC_MODE);
	struct PRM_INTF_TYPE *pix_rmif2    = &g_pix_rmif2;
	struct PRM_INTF_TYPE *pix_rmif3    = &g_pix_rmif3;
	struct PRM_INTF_TYPE *pix_rmif4    = &g_pix_rmif4;

	memcpy(pix_rmif2, pix_rmif, sizeof(*pix_rmif2));
	memcpy(pix_rmif3, pix_rmif, sizeof(*pix_rmif3));

	pix_rmif3->src_fmt = prm_top->nro_fs_fmt.src_fmt;//YUV444/YUV422/YUV420/RGB
	pix_rmif3->src_plan = prm_top->nro_fs_fmt.src_plan;//PLANAR_X1/X2
	pix_rmif3->src_bit = prm_top->nro_fs_fmt.src_bit;//BIT_8/10
	memcpy(pix_rmif4, pix_rmif3, sizeof(*pix_rmif4));

	//pix_rmif2->uv_swap = false;
	if (prm_top->is_i) {
		pix_rmif2->uv_swap = prm_top->uv_swap;
		pix_rmif->uv_swap = false;

	} else {
		memcpy(pix_rmif2, pix_rmif3, sizeof(*pix_rmif2));
		pix_rmif->uv_swap = prm_top->uv_swap;
		pix_rmif2->uv_swap = false;
		pix_rmif2->block_mode = 0;
	}
	pix_rmif3->block_mode = 0;
	pix_rmif4->block_mode = 0;
	pix_rmif3->uv_swap = false;
	pix_rmif4->uv_swap = false;

	if (!prm_top->is_i) {
		//must not write to pix_rmif2 / pix_rmif3
		pix_rmif->lst_overlap	= prm_dpe->lst_overlap;
		pix_rmif2->lst_overlap = prm_dpe->lst_overlap2;
	} else {
		//must not write to pix_rmif2 / pix_rmif3
		pix_rmif2->lst_overlap	= prm_dpe->lst_overlap;
		pix_rmif3->lst_overlap = prm_dpe->lst_overlap2;
		pix_rmif4->lst_overlap = prm_dpe->lst_overlap2;
	}
	dbg_h2("2: lst_overlap =%d,%d\n", pix_rmif->lst_overlap, pix_rmif2->lst_overlap);

	if (dpss_dbg_uv_swap & C_BIT5)
		pix_rmif2->uv_swap = true;

	if (dpss_dbg_uv_swap & C_BIT6)
		pix_rmif3->uv_swap = true;

	if (dpss_dbg_uv_swap & C_BIT7)
		pix_rmif4->uv_swap = true;

	dbg_h2("dpe rd uv sw:%d %d %d\n",
		pix_rmif2->uv_swap,
		pix_rmif3->uv_swap,
		pix_rmif4->uv_swap);
	pix_rmif->h_buf_size = prm_top->src_h_buf_size;
	if (dpss_dbg_h_buf_size) {
		dbg_h1("%s:input:h_buf_size:%d\n", __func__, dpss_dbg_h_buf_size);
		pix_rmif->h_buf_size = dpss_dbg_h_buf_size;
	}

	pix_rmif->reverse[0] = prm_top->reverse[0];
	pix_rmif->reverse[1] = prm_top->reverse[1];

	pix_rmif->src_hsize = prm_top->nr_hscale_on |
		prm_top->comp_setting.vfcd_h_skip ? prm_dpe->dpe_nr_size.rmif_hsize[0] :
		prm_top->frm_hsize;
	pix_rmif->src_vsize = prm_top->nr_hscale_on |
		prm_top->comp_setting.vfcd_v_skip ? prm_dpe->dpe_nr_size.rmif_vsize[0] :
		prm_top->frm_vsize;
	dbg_h2("%s:addr var set\n", __func__);

	switch (dpe_mode) {
	case DPE_NR_MODE:
		pix_rmif->src_baddr[0] = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
				prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 9;
		pix_rmif->src_baddr[1] = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
				prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 9;
		dbg_h2("%s:addr:in: nr_mode: pix_rmif :%d:yaddr:0x%x\n", __func__,
			dpe_src0_cur0_rd_idx, pix_rmif->src_baddr[0]);
		pix_rmif2->src_baddr[0] =
			prm_top->src0_nro_fbuf_yaddr[dpe_src0_pre1_rd_idx] << 9;
		pix_rmif2->src_baddr[1] =
			prm_top->src0_nro_fbuf_caddr[dpe_src0_pre1_rd_idx] << 9;
		dbg_h2("%s:addr:nro: nr_mode:wr pix_rmif2 :%d:yaddr:0x%x\n", __func__,
			dpe_src0_pre1_rd_idx, pix_rmif2->src_baddr[0]);
	break;
	case DPE_VDI_MODE:   //di_only
		pix_rmif2->src_baddr[0] =
			prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif2->src_baddr[1] =
			prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif3->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre1_rd_idx] << 9;
		pix_rmif3->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre1_rd_idx] << 9;
		pix_rmif4->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre2_rd_idx] << 9;
		pix_rmif4->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre2_rd_idx] << 9;
		break;
	case DPE_NR_SRC1_MODE:
		pix_rmif->src_baddr[0] =
			prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif->src_baddr[1] =
			prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif2->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre1_rd_idx] << 9;
		pix_rmif2->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre1_rd_idx] << 9;
		break;
	case DPE_NRDI_MODE:  //nr+di
		pix_rmif2->src_baddr[0] =
			prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif2->src_baddr[1] =
			prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 9;
		pix_rmif3->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre1_rd_idx] << 9;
		pix_rmif3->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre1_rd_idx] << 9;
		pix_rmif4->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre2_rd_idx] << 9;
		pix_rmif4->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre2_rd_idx] << 9;
		break;
	case DPE_NR_BYPS:
		pix_rmif->src_baddr[0] =
			(prm_top->di_frc_link ?
				prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
				prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 9;
		pix_rmif->src_baddr[1] =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
				prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 9;
		break;
	default:
		DBG_WARN("%s:5 %d not in case\n", __func__, dpe_mode);
		break;
	}

	for (i = 0; i < 4; i++) {
		pix_rmif->slc_x_st[i] = prm_dpe->pix_rmif0.slc_x_st[i];
		pix_rmif->slc_x_ed[i] = prm_dpe->pix_rmif0.slc_x_ed[i];
		pix_rmif->slc_y_st[i] = prm_dpe->pix_rmif0.slc_y_st[i];
		pix_rmif->slc_y_ed[i] = prm_dpe->pix_rmif0.slc_y_ed[i];

		pix_rmif2->slc_x_st[i] = prm_dpe->pix_rmif1.slc_x_st[i];
		pix_rmif2->slc_x_ed[i] = prm_dpe->pix_rmif1.slc_x_ed[i];
		pix_rmif2->slc_y_st[i] = prm_dpe->pix_rmif1.slc_y_st[i];
		pix_rmif2->slc_y_ed[i] = prm_dpe->pix_rmif1.slc_y_ed[i];

		pix_rmif3->slc_x_st[i] = prm_dpe->pix_rmif2.slc_x_st[i];
		pix_rmif3->slc_x_ed[i] = prm_dpe->pix_rmif2.slc_x_ed[i];
		pix_rmif3->slc_y_st[i] = prm_dpe->pix_rmif2.slc_y_st[i];
		pix_rmif3->slc_y_ed[i] = prm_dpe->pix_rmif2.slc_y_ed[i];

		pix_rmif4->slc_x_st[i] = prm_dpe->pix_rmif3.slc_x_st[i];
		pix_rmif4->slc_x_ed[i] = prm_dpe->pix_rmif3.slc_x_ed[i];
		pix_rmif4->slc_y_st[i] = prm_dpe->pix_rmif3.slc_y_st[i];
		pix_rmif4->slc_y_ed[i] = prm_dpe->pix_rmif3.slc_y_ed[i];
	}
	//==================================================
	//DI mode, interlace rd
	//==================================================
	if (prm_top->di_interlace)//yu.zong 2024-12-06
		pix_rmif2->skip_line = 1;

	bool dolby_parallel = (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE);

	pix_rmif->canvas_hsize = 0;
	pix_rmif2->canvas_hsize = 0;
	pix_rmif3->canvas_hsize = 0;
	pix_rmif4->canvas_hsize = 0;
	if (prm_top->is_i)
		pix_rmif2->canvas_hsize = prm_top->nr_src_canvas_hsize;
	else
		pix_rmif->canvas_hsize = prm_top->nr_src_canvas_hsize;

#ifdef LCEVC_MIF_SRC   //VFCD SRC
	if (!prm_top.lcevc_en) {
#endif
		//ary note: for di: vfcd1-cur, vfcd2 - pre1, vfcd3 - pre2
		if ((dolby_parallel && prm_dpe->dpe_mode == DPE_NR_BYPS) ||
		    !dolby_parallel)
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE0, pix_rmif, fmt444_out); //pix_cur
		cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE1, pix_rmif2, fmt444_out); //pix_pre1
		cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE2, pix_rmif3, fmt444_out); //pix_pre2
		cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE3, pix_rmif4, fmt444_out); //?
#ifdef LCEVC_MIF_SRC   //VFCD SRC
	}
#endif
	bool nr_4k1k_vds_en = prm_top->vds_4k1k_en;

	if (nr_4k1k_vds_en) {
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 1, 25, 1);      //reg_mif_win_y_mode
		w_reg_bit_vc(RMIF_F0_LUMA_RPT_PAT, 1, 3, 1);  //reg_f0_luma_rpt_pat
		w_reg_bit_vc(RMIF_F0_CHRM_RPT_PAT, 1, 3, 1);  //reg_f0_chrm_rpt_pat
		//reg_VFCD_MIF0_VSIZE
		w_reg_bit(DPSS_DPE_VFCD_MIF0_VSIZE, prm_top->frm_vsize * 2, 0, 13);
		//NO USE APB MODE
		w_reg_bit_vc(MIF0_RMIF_CTRL1, 0, 27, 1);
	} else {
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 0, 25, 1);      //reg_mif_win_y_mode
	}

	bool dpss_nr_vpp_link = (rd(DPSS_NR_VPP_LINK) & 0x1);
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	if (dpss_nr_vpp_link) {
		if (src0_nrdi_frc_en) {//src0 di link
			dbg_h2("------di vpp link config rdmif------\n");
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif2, fmt444_out);
			//cur
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE0, pix_rmif3, fmt444_out);
			//pre
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE1, pix_rmif4, fmt444_out);
			//pre2
		} else {//src0 nr link
			dbg_h2("------nr vpp link config rdmif------\n");
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC0, pix_rmif, fmt444_out);
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_MC1, pix_rmif2, fmt444_out);
		}
	}
	struct VFCD_t *vfcd0 = &g_vfcd0;
	struct VFCD_t *vfcd1 = &g_vfcd1;
	//ary move to globe struct VFCD_t vfcd0;
	//ary move to globe struct VFCD_t vfcd1;
	init_dpss_vfcd(0, vfcd0);
	init_dpss_vfcd(0, vfcd1);
	struct PRM_SRC_FMT vfcd0_fs_fmt;
	struct PRM_SRC_FMT vfcd1_fs_fmt;

	if (dpe_mode == DPE_VDI_MODE ||
		dpe_mode == DPE_NRDI_MODE) {
		vfcd0_fs_fmt = prm_top->src_fs_fmt;
		vfcd1_fs_fmt = prm_top->src_fs_fmt;
	} else {
		vfcd0_fs_fmt = prm_top->src_fs_fmt;
		vfcd1_fs_fmt = prm_top->nro_fs_fmt;
	}

	switch (dpe_mode) {
	case DPE_NR_MODE:
		crr_idx = dpe_src0_cur0_rd_idx; //ori setting
		if (!prm_top->src_mode)
			crr_idx = dpe_src0_cur0_rd_idx;
		else
			crr_idx = dpe_src1_cur0_rd_idx;
		vfcd0->luma_head_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_head_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_yaddr[crr_idx]) << 5;
		vfcd0->luma_body_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_head_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_head_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_caddr[crr_idx]) << 5;
		vfcd0->chrm_body_baddr =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd1->luma_head_baddr =
			prm_top->src0_nro_head_yaddr[dpe_src0_pre1_rd_idx] << 5;
		vfcd1->luma_body_baddr =
			prm_top->src0_nro_fbuf_yaddr[dpe_src0_pre1_rd_idx] << 5;
		dbg_h2("%s:addr:nro: nr_mode:wr vfcd1 :%d:yaddr:0x%x\n", __func__,
		dpe_src0_pre1_rd_idx, vfcd1->luma_body_baddr);
		vfcd1->chrm_head_baddr =
			prm_top->src0_nro_head_caddr[dpe_src0_pre1_rd_idx] << 5;
		vfcd1->chrm_body_baddr =
			prm_top->src0_nro_fbuf_caddr[dpe_src0_pre1_rd_idx] << 5;
		break;
	case DPE_VDI_MODE://di_only
		vfcd1->luma_head_baddr =
			prm_top->src1_head_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->luma_body_baddr =
			prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->chrm_head_baddr =
			prm_top->src1_head_caddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->chrm_body_baddr =
			prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 5;
		break;
	case DPE_NR_SRC1_MODE:
		vfcd0->luma_head_baddr =
			prm_top->src1_head_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd0->luma_body_baddr =
			prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd0->chrm_head_baddr =
			prm_top->src1_head_caddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd0->chrm_body_baddr =
			prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->luma_head_baddr =
			prm_top->src1_nro_head_yaddr[dpe_src1_pre1_rd_idx] << 5;
		vfcd1->luma_body_baddr =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_pre1_rd_idx] << 5;
		vfcd1->chrm_head_baddr =
			prm_top->src1_nro_head_caddr[dpe_src1_pre1_rd_idx] << 5;
		vfcd1->chrm_body_baddr =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_pre1_rd_idx] << 5;
		break;
	case DPE_NRDI_MODE:  //nr+di
		if (!prm_top->src_mode)
			crr_idx = dpe_src0_cur0_rd_idx;
		else
			crr_idx = dpe_src1_cur0_rd_idx;
		vfcd1->luma_head_baddr =
			prm_top->src0_head_yaddr[crr_idx] << 5;
		vfcd1->luma_body_baddr =
			prm_top->src0_fbuf_yaddr[crr_idx] << 5;
		vfcd1->chrm_head_baddr =
			prm_top->src0_head_caddr[crr_idx] << 5;
		vfcd1->chrm_body_baddr =
			prm_top->src0_fbuf_caddr[crr_idx] << 5;
		break;
	case DPE_NR_BYPS:
		crr_idx = dpe_src0_cur0_rd_idx; //ori setting
		if (!prm_top->src_mode)
			crr_idx = dpe_src0_cur0_rd_idx;
		else
			crr_idx = dpe_src1_cur0_rd_idx;

		vfcd0->luma_head_baddr =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_head_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_yaddr[crr_idx]) << 5;
		vfcd0->luma_body_baddr =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
		prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_head_baddr =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_head_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_caddr[crr_idx]) << 5;
		vfcd0->chrm_body_baddr =
			(prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
		prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 5;
		break;
	default:
		dbg_h2("%s:4 %d not in case\n", __func__, dpe_mode);
		break;
	}

	vfcd0->fmt444_out             = fmt444_out;
	vfcd0->slc_num                = prm_top->dpe_slc_num;
	vfcd0->film_grain_en          = 0;
	vfcd0->pad_en                 = prm_top->auto_alig_en;
	vfcd0->pad_hmode              = prm_top->alig_hmode;
	vfcd0->pad_vmode              = prm_top->alig_vmode;
	vfcd0->mmu_page_mode          = prm_top->comp_setting.mmu_page_mode;

	vfcd0->afrcd.luma_comp_target = prm_top->comp_setting.afrc_target_byte_y;
	vfcd0->afrcd.luma_header_en   = prm_top->comp_setting.afrc_head_mode;
	vfcd0->afrcd.luma_dict_en     = prm_top->comp_setting.afrc_dict_mode_y;

	vfcd0->afrcd.chrm_comp_target = prm_top->comp_setting.afrc_target_byte_c;
	vfcd0->afrcd.chrm_header_en   = prm_top->comp_setting.afrc_head_mode;
	vfcd0->afrcd.chrm_dict_en     = prm_top->comp_setting.afrc_dict_mode_c;

	//ary test 0723//prm_top->frm_hsize;
	vfcd0->src_hsize              = prm_top->org_hsize;
	vfcd0->src_vsize              = prm_top->org_vsize << nr_4k1k_vds_en;
	for (i = 0; i < 4; i++) {
		vfcd0->win_bgn_h[i]       = prm_dpe->pix_rmif0.slc_x_st[i];
		vfcd0->win_end_h[i]       = prm_dpe->pix_rmif0.slc_x_ed[i];
		vfcd0->win_bgn_v[i]       = prm_dpe->pix_rmif0.slc_y_st[i];
		vfcd0->win_end_v[i]       = prm_dpe->pix_rmif0.slc_y_ed[i] << nr_4k1k_vds_en;
	}
	vfcd0->mmu_mode_en = prm_top->comp_setting.afrc_head_mode == 0 ?
			prm_top->comp_setting.vdin_mmu_mode_en : 0;
	vfcd0->afbcd.fmt444_comb      = prm_top->comp_setting.fmt444_comb;
	vfcd0->y_h_skip_en            = prm_top->comp_setting.vfcd_h_skip;
	vfcd0->y_v_skip_en            = prm_top->comp_setting.vfcd_v_skip | nr_4k1k_vds_en;
	vfcd0->c_h_skip_en            = prm_top->comp_setting.vfcd_h_skip;
	vfcd0->c_v_skip_en            = prm_top->comp_setting.vfcd_v_skip | nr_4k1k_vds_en;
	vfcd0->rev_mode               = prm_top->comp_setting.vfcd_rev_mode;
	vfcd0->cmpr_sel               = vfcd0_fs_fmt.src_cmpr == CMPR_AFBC ? 1 :
					vfcd0_fs_fmt.src_cmpr == CMPR_AFRC ? 2 :
					0;
	vfcd0->hfmt_en = fmt444_out ? vfcd0_fs_fmt.src_fmt != YUV444 : 0;
	vfcd0->vfmt_en = fmt444_out ? vfcd0_fs_fmt.src_fmt == YUV420 : 0;
	vfcd0->src_fmt = vfcd0_fs_fmt.src_fmt == YUV444 ? 0 :
					vfcd0_fs_fmt.src_fmt == YUV422 ? 1 : 2;
	vfcd0->compbits_y = vfcd0_fs_fmt.src_cmpr == CMPR_UN ?
			10 : vfcd0_fs_fmt.src_bit == BIT_008  ? 8
			: vfcd0_fs_fmt.src_bit == BIT_012 ? 12 : 10;
	vfcd0->compbits_c = vfcd0_fs_fmt.src_cmpr == CMPR_UN ?
			10 : vfcd0_fs_fmt.src_bit == BIT_008  ? 8
					: vfcd0_fs_fmt.src_bit == BIT_012 ? 12 : 10;
	//1114 cfg_vfcd_dec(0, vfcd0);//nr din cur

	vfcd1->fmt444_out = fmt444_out;
	vfcd1->slc_num = prm_top->dpe_slc_num;
	vfcd1->film_grain_en = 0;
	vfcd1->pad_en = prm_top->auto_alig_en;
	vfcd1->pad_hmode = prm_top->alig_hmode;
	vfcd1->pad_vmode = prm_top->alig_vmode;
	vfcd1->mmu_page_mode = prm_top->comp_setting.mmu_page_mode;

	vfcd1->afrcd.luma_comp_target = prm_top->comp_setting.afrc_target_byte_y;
	vfcd1->afrcd.luma_header_en = prm_top->comp_setting.afrc_head_mode;
	vfcd1->afrcd.luma_dict_en = prm_top->comp_setting.afrc_dict_mode_y;

	vfcd1->afrcd.chrm_comp_target = prm_top->comp_setting.afrc_target_byte_c;
	vfcd1->afrcd.chrm_header_en = prm_top->comp_setting.afrc_head_mode;
	vfcd1->afrcd.chrm_dict_en = prm_top->comp_setting.afrc_dict_mode_c;

	vfcd1->src_hsize = prm_top->comp_setting.vfcd_h_skip ?
		prm_dpe->dpe_nr_size.frm_hsize : prm_top->frm_hsize;
	vfcd1->src_vsize = prm_top->comp_setting.vfcd_v_skip ?
		prm_dpe->dpe_nr_size.frm_vsize : prm_top->frm_vsize;
	for (i = 0; i < 4; i++) {
		vfcd1->win_bgn_h[i] = prm_dpe->pix_rmif1.slc_x_st[i];
		vfcd1->win_end_h[i] = prm_dpe->pix_rmif1.slc_x_ed[i];
		vfcd1->win_bgn_v[i] = prm_dpe->pix_rmif1.slc_y_st[i];
		vfcd1->win_end_v[i] = prm_dpe->pix_rmif1.slc_y_ed[i];
	}
	//vfcd1.win_bgn_h[1] = 0; //need real_size
	//vfcd1.win_end_h[1] = vfcd1.src_hsize -1;
	//vfcd1.win_bgn_v[1] = 0;
	//vfcd1.win_end_v[1] = vfcd1.src_vsize -1;
	vfcd1->mmu_mode_en = prm_top->comp_setting.afrc_head_mode == 0 ?
			prm_top->comp_setting.mmu_mode_en : 0;
	vfcd1->afbcd.fmt444_comb      = 0;
	vfcd1->y_h_skip_en            = 0;
	vfcd1->y_v_skip_en            = 0;
	vfcd1->c_h_skip_en            = 0;
	vfcd1->c_v_skip_en            = 0;
	vfcd1->rev_mode               = 0;
	vfcd1->cmpr_sel               = vfcd1_fs_fmt.src_cmpr == CMPR_AFBC ? 1 :
					vfcd1_fs_fmt.src_cmpr == CMPR_AFRC ? 2 :
					0;
	vfcd1->hfmt_en = fmt444_out ? vfcd1_fs_fmt.src_fmt != YUV444 : 0;
	vfcd1->vfmt_en = fmt444_out ? vfcd1_fs_fmt.src_fmt == YUV420 : 0;
	vfcd1->src_fmt = vfcd1_fs_fmt.src_fmt == YUV444 ? 0 :
					vfcd1_fs_fmt.src_fmt == YUV422 ? 1 : 2;
	vfcd1->compbits_y = vfcd1_fs_fmt.src_cmpr == CMPR_UN ?
		10 : vfcd1_fs_fmt.src_bit == BIT_008  ? 8
			: vfcd1_fs_fmt.src_bit == BIT_012 ? 12 : 10;
	vfcd1->compbits_c = vfcd1_fs_fmt.src_cmpr == CMPR_UN ?
		10 : vfcd1_fs_fmt.src_bit == BIT_008  ? 8
			: vfcd1_fs_fmt.src_bit == BIT_012 ? 12 : 10;

	if (nr_4k1k_vds_en) {
		dbg_h1("DLG mode set Y UV vertical skip mode : avg\n");
		vfcd0->y_v_skip_en = 0x3;
		vfcd0->c_v_skip_en = 0x3;
	}

	u32 vfcd_idx0;
	u32 vfcd_idx1;

	if (dpss_nr_vpp_link) {
		vfcd_idx0 = 4;
		vfcd_idx1 = 5;
	} else {
		vfcd_idx0 = 0;
		vfcd_idx1 = 1;
	}
	if (vfcd_idx1 == 1 && prm_top->is_i)
		vfcd1->src_is_i = true;
	else
		vfcd1->src_is_i = false;

	if (!prm_top->is_i) {
		if (prm_top->force_dos_mode) //ary add for test
			vfcd0->afbcd.dos_uncomp = 1;
		else
			vfcd0->afbcd.dos_uncomp = 0;
	} else {
		if (prm_top->force_dos_mode)
			vfcd1->afbcd.dos_uncomp = 1;
		else
			vfcd1->afbcd.dos_uncomp = 0;
	}
	if (prm_top->fmt_444_10) {
		vfcd0->adj_offset = true;
		vfcd1->adj_offset = true;
	} else {
		vfcd0->adj_offset = false;
		vfcd1->adj_offset = false;
	}
	if (prm_top->is_amdv_frame) {
		vfcd0->hfmt_mode = 1;
		vfcd1->hfmt_mode = 1;
	} else {
		vfcd0->hfmt_mode = 0;
		vfcd1->hfmt_mode = 0;
	}
	if ((dolby_parallel && prm_dpe->dpe_mode == DPE_NR_BYPS) ||
	    !dolby_parallel)
		cfg_vfcd_dec(vfcd_idx0, vfcd0);//nr din cur
	cfg_vfcd_dec(vfcd_idx1, vfcd1);//nr din pre

	//nr cur din vfcd mode
	if ((dolby_parallel && prm_dpe->dpe_mode == DPE_NR_BYPS) ||
	    !dolby_parallel) {
		dbg_h2("%s:src_cmpr = %d\n", __func__, vfcd0_fs_fmt.src_cmpr);
		if (vfcd0_fs_fmt.src_cmpr == CMPR_AFBC)
			cfg_vfcd_cmpr_enable(vfcd_idx0, 1);//vfcd_chose afbc
		else if (vfcd0_fs_fmt.src_cmpr == CMPR_AFRC)
			cfg_vfcd_cmpr_enable(vfcd_idx0, 2);//vfcd_chose afrc
		else
			cfg_vfcd_cmpr_enable(vfcd_idx0, 0);//vfcd_chose mif
	}
	//nr pre din vfcd mode
	if (vfcd1_fs_fmt.src_cmpr == CMPR_AFBC)
		cfg_vfcd_cmpr_enable(vfcd_idx1, 1);//vfcd_chose afbc
	else if (vfcd1_fs_fmt.src_cmpr == CMPR_AFRC)
		cfg_vfcd_cmpr_enable(vfcd_idx1, 2);//vfcd_chose afrc
	else
		cfg_vfcd_cmpr_enable(vfcd_idx1, 0);//vfcd_chose mif

	//==============================================================//
	// pix_wrmif
	//==============================================================//
	enum vid_src_fmt fs_fmt  = prm_top->nro_fs_fmt.src_fmt;
	//src_is_nr ? prm_top.nro_fs_fmt.src_fmt  : prm_top.src_fs_fmt.src_fmt;
	enum vid_src_fmt fs_bit  = prm_top->nro_fs_fmt.src_bit;
	//src_is_nr ? prm_top.nro_fs_fmt.src_bit  : prm_top.src_fs_fmt.src_bit;
	enum vid_src_fmt fs_plan = prm_top->nro_fs_fmt.src_plan;
	//src_is_nr ? prm_top.nro_fs_fmt.src_plan : prm_top.src_fs_fmt.src_plan;

	//enum vid_src_fmt fs_fmt0  = prm_top->src_fs_fmt.src_fmt;
	//src_is_nr ? prm_top.nro_fs_fmt.src_fmt  : prm_top.src_fs_fmt.src_fmt;
	//enum vid_src_fmt fs_bit0  = prm_top->src_fs_fmt.src_bit;
	//src_is_nr ? prm_top.nro_fs_fmt.src_bit  : prm_top.src_fs_fmt.src_bit;
	//enum vid_src_fmt fs_plan0 = prm_top->src_fs_fmt.src_plan;
	//src_is_nr ? prm_top.nro_fs_fmt.src_plan : prm_top.src_fs_fmt.src_plan;

	enum vid_src_fmt ds_fmt  = prm_top->nro_ds_fmt.src_fmt;
	//src_is_nr ? prm_top.nro_ds_fmt.src_fmt  : prm_top.src_ds_fmt.src_fmt;
	enum vid_src_fmt ds_bit  = prm_top->nro_ds_fmt.src_bit;
	//src_is_nr ? prm_top.nro_ds_fmt.src_bit  : prm_top.src_ds_fmt.src_bit;
	enum vid_src_fmt ds_plan = prm_top->nro_ds_fmt.src_plan;
	//src_is_nr ? prm_top.nro_ds_fmt.src_plan : prm_top.src_ds_fmt.src_plan;

	bool nr_aapps_en;// = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;
	//reg_aa_pps_mode == 3 -> nr aapps 4k1k

	u32 reg_wr_wr_sep = fs_plan == PLANAR_X1 ? 0 : 1;//1:2plane
	u32 reg_wr_wr_bit = fs_bit == BIT_008  ? 0 : fs_bit == BIT_010 ?
			1 : fs_bit == BIT_012 ? 2 : 3;//0:8bit 1:10bit 2:12bit 3:16bit
	u32 reg_wr_wr_fmt = fs_fmt == YUV444 ? 0 : fs_fmt == YUV422 ?
			1 : 2; //0:444 1:422 2:420
	u32 reg_wr_wr_sep0 = fs_plan == PLANAR_X1 ? 0 : 1;//1:2plane
	u32 reg_wr_wr_bit0 = fs_bit == BIT_008  ? 0 :
						fs_bit == BIT_010 ? 1 : fs_bit == BIT_012 ? 2 : 3;
	//0:8bit 1:10bit 2:12bit 3:16bit
	u32 reg_wr_wr_fmt0 = fs_fmt == YUV444 ? 0 : fs_fmt == YUV422 ? 1 : 2; //0:444 1:422 2:420
	u32 reg_wr_ds_sep = ds_plan == PLANAR_X1 ? 0 : 1;//1:2plane
	u32 reg_wr_ds_bit = ds_bit == BIT_008  ?
		0 : ds_bit == BIT_010 ? 1 : ds_bit == BIT_012 ? 2 : 3;
	//0:8bit 1:10bit 2:12bit 3:16bit
	u32 reg_wr_ds_fmt = ds_fmt == YUV444 ?
		0 : ds_fmt == YUV422 ? 1 : 2;
	//0:444 1:422 2:420
	u32 reg_wr0_dsy    = dw_dsy;
	//(src_is_nr && src_is_di) ? 0 :dw_dsy;
	//I source NR DI , NR double write close
	u32 reg_wr0_dsx;// = nr_aapps_en &&
			//(prm_dpe->dpe_nr_size.pps_hsize != prm_top->frm_hsize) ? 0 : dw_dsx;
	//(src_is_nr && src_is_di) ? 0 :dw_dsx;
	//I source NR DI , NR double write close
	u32 reg_wr1_dsy    = dw_dsy;//wr1 nouse, wr0 use, only one dsmif
	u32 reg_wr1_dsx    = dw_dsx;//wr1 nouse, wr0 use, only one dsmif
	u32 reg_wr_ds_sep0 = ds_plan == PLANAR_X1 ? 0 : 1;//1:2plane
	u32 reg_wr_ds_bit0 = ds_bit == BIT_008  ?
		0 : ds_bit == BIT_010 ? 1 : ds_bit == BIT_012 ? 2 : 3;
	//0:8bit 1:10bit 2:12bit 3:16bit
	u32 reg_wr_ds_fmt0 = ds_fmt == YUV444 ? 0 : ds_fmt == YUV422 ? 1 : 2; //0:444 1:422 2:420

	u32 ds0_src_hsize = pps_en ?
		prm_dpe->dpe_nr_size.pps_hsize : prm_top->nr_hscale_on ?
		prm_dpe->dpe_nr_size.frm_hsize : prm_top->frm_hsize;
	u32 ds0_src_vsize = pps_en ?
		prm_dpe->dpe_nr_size.pps_vsize : prm_top->nr_hscale_on ?
		prm_dpe->dpe_nr_size.frm_vsize : prm_top->frm_vsize;

	if (pps_en)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	reg_wr0_dsx = nr_aapps_en &&
		(prm_dpe->dpe_nr_size.pps_hsize != prm_top->frm_hsize) ? 0 : dw_dsx;
	dbg_h2("%s pps ch=%d,pps_en=%x,%d,%d\n", __func__,
			prm_top->ch, nr_aapps_en,
			ds0_src_hsize, pps_en);

	//wrmif0 and ds wrmif0: nr(ds) dout/mc frm0 //ref
	wr(DPSS_DPE_INTF_CTRL0, ((reg_wr_ds_sep0 & 0x1) << 13)
				| ((reg_wr_wr_sep0 & 0x1) << 12)
				| ((reg_wr_ds_bit0 & 0x3) << 10)
				| ((reg_wr_ds_fmt0 & 0x3) << 8)
				| ((reg_wr_wr_bit0 & 0x3) << 6)
				| ((reg_wr_wr_fmt0 & 0x3) << 4)
				| ((reg_wr0_dsy & 0x3) << 2)
				| ((reg_wr0_dsx & 0x3) << 0));

	//wrmif1 and ds wrmif1: di(ds) dout/mc frm1
	wr(DPSS_DPE_INTF_CTRL1, ((reg_wr_ds_sep & 0x1) << 13)
				| ((reg_wr_wr_sep & 0x1) << 12)
				| ((reg_wr_ds_bit & 0x3) << 10)
				| ((reg_wr_ds_fmt & 0x3) << 8)
				| ((reg_wr_wr_bit & 0x3) << 6)
				| ((reg_wr_wr_fmt & 0x3) << 4)
				| ((reg_wr1_dsy & 0x3) << 2)
				| ((reg_wr1_dsx & 0x3) << 0));

	//wrmif2 and ds wrmif2: di(ds) dout/mc frm1
	wr(DPSS_DPE_INTF_CTRL3, ((reg_wr_ds_sep & 0x1) << 13)
	//todo, add another reg??
				| ((reg_wr_wr_sep & 0x1) << 12)
				| ((reg_wr_ds_bit & 0x3) << 10)
				| ((reg_wr_ds_fmt & 0x3) << 8)
				| ((reg_wr_wr_bit & 0x3) << 6)
				| ((reg_wr_wr_fmt & 0x3) << 4)
				| ((reg_wr1_dsy   & 0x3) << 2)
				| ((reg_wr1_dsx   & 0x3) << 0));
	dbg_h2("%s:DPSS_DPE_INTF_CTRL3 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL3));
	dbg_h2("%s:DPSS_DPE_INTF_CTRL1 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL1));
	dbg_h2("%s:DPSS_DPE_INTF_CTRL0 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL0));
	//dbg_h2("%s:DPSS_DPE_MIF_CTRL1 0x%x\n", __func__,Rd(DPSS_DPE_MIF_CTRL1));

	dbg_h2("%s:DPSS_DPE_INTF_CTRL3 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL3));
	dbg_h2("%s:DPSS_DPE_INTF_CTRL1 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL1));
	dbg_h2("%s:DPSS_DPE_INTF_CTRL0 0x%x\n", __func__, rd(DPSS_DPE_INTF_CTRL0));
	//dbg_h2("%s:DPSS_DPE_MIF_CTRL1 0x%x\n", __func__,rd(DPSS_DPE_MIF_CTRL1));

	//u32 aa0_baddr = FRC_AA_PPS_CTRL;//VID_WMIF_CTRL0+(0xC0+0x10*0)*4; //todo
	//u32 aa1_baddr = FRC_AA_PPS_CTRL;//VID_WMIF_CTRL0+(0xC0+0x10*1)*4; //todo
	//wr(aa0_baddr + (1), 0x7f7f);//reg_aa_ds_coef_0 dsx/dsy //coef sum = 127 mean 1
	//ary addr change *4
	//wr(aa0_baddr + (2), 0x0);//reg_aa_ds_coef_1 dsx/dsy	//ary addr change *4
	//wr(aa0_baddr + (3), 0x0);//reg_aa_ds_coef_2 dsx/dsy	//ary addr change *4
	//wr(aa0_baddr + (4), 0x0);//reg_aa_ds_coef_3 dsx/dsy	//ary addr change *4

	u32 wrpath_sel = (rd(DPSS_DPE_INTF_AFBCD0) >> 12) & 0x1ffff;
	u32 w0_sel = (wrpath_sel >> 7) & 0x3;
	u32 w1_sel = (wrpath_sel >> 10) & 0x3;
	u32 vfce_on = (wrpath_sel) & 0x1;

	struct PRM_INTF_TYPE *pix_wmif0 = &g_pix_wmif0;
	struct PRM_INTF_TYPE *pix_wmif1 = &g_pix_wmif1;
	struct PRM_INTF_TYPE *pix_wmif2 = &g_pix_wmif2;

	memset((void *)(pix_wmif0), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(pix_wmif1), 0, sizeof(struct PRM_INTF_TYPE));
	memset((void *)(pix_wmif2), 0, sizeof(struct PRM_INTF_TYPE));

	#ifdef HISTOR_VER
	if (prm_top->nro_fs_fmt.src_fmt == OUT_YUV420_NV12)
		pix_wmif2->uv_swap = true;
	else if (prm_top->nro_fs_fmt.src_fmt == OUT_YUV422_1_PLANE ||
		prm_top->nro_fs_fmt.src_fmt == OUT_YUV422_2_PLANE)
		pix_wmif2->uv_swap = false;
	else
		pix_wmif2->uv_swap = false;
	#endif
	pix_wmif0->swap_64bit = prm_top->swap_64bit;
	pix_wmif0->little_endian = prm_top->l_endian;
	//pix_wmif0->uv_swap = 0;

	pix_wmif1->swap_64bit = prm_top->swap_64bit;
	pix_wmif1->little_endian = prm_top->l_endian;
	//pix_wmif1->uv_swap = 0;

	big_endian = prm_top->l_endian ? 0 : 1;
	rev_x = pix_wmif2->reverse[0];
	temp_uv = big_endian ^ rev_x;

	if (prm_top->out_mode == OUT_YUV420_NV12) {
	/* NV12 and big_endian = 0 and rev_x = 0 default uvuv */
		if (temp_uv == 0)
			pix_wmif2->uv_swap = 1;
		else
			pix_wmif2->uv_swap = 0;
	} else if (prm_top->out_mode == OUT_YUV420_NV21) {
		if (temp_uv == 0)
			pix_wmif2->uv_swap = 0;
		else
			pix_wmif2->uv_swap = 1;
	} else if (prm_top->out_mode == OUT_YUV422_1_PLANE ||
		prm_top->out_mode == OUT_YUV422_2_PLANE) {
		pix_wmif2->uv_swap = 0;
	}
	pix_wmif2->swap_64bit = prm_top->swap_64bit;
	pix_wmif2->little_endian = prm_top->l_endian;
	pix_wmif1->uv_swap = pix_wmif2->uv_swap;
	if (prm_top->is_i)
		pix_wmif0->uv_swap = false;
	else
		pix_wmif0->uv_swap = pix_wmif2->uv_swap;

	dbg_h2("dpe wr uv sw:%d,fs_plan %d\n", pix_wmif0->uv_swap,
				fs_plan);
	if (dpss_dbg_uv_swap & C_BIT8) {
		pix_wmif0->uv_swap = true;
		pix_wmif1->uv_swap = true;
		pix_wmif2->uv_swap = true;
	}

	pix_wmif0->src_fmt       = fs_fmt;
	//prm_top.nro_fs_fmt.src_fmt    //YUV444/YUV422/YUV420/RGB
	pix_wmif0->src_plan      = fs_plan;
	//prm_top.nro_fs_fmt.src_plan   //PLANAR_X1/X2/X3
	pix_wmif0->src_bit       = p010_mode ? BIT_P010 :  fs_bit;
	//prm_top.nro_fs_fmt.src_bit    //BIT_8/10/12
	pix_wmif0->src_cmpr      = CMPR_UN;
	//prm_top.nro_fs_fmt.src_cmpr   //un/afbc/afrc
	pix_wmif0->interlace     = IS_PSRC;
	//prm_top.nro_fs_fmt.interlace  //IS_PSRC/IS_ISRC
	pix_wmif0->src_hsize     = prm_top->frm_hsize;
	pix_wmif0->src_vsize     = prm_top->frm_vsize;
	pix_wmif0->bits_mode     = 7;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	pix_wmif0->pack_mode     = 1;
	//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	pix_wmif1->src_fmt       = fs_fmt;
	//prm_top.nro_fs_fmt.src_fmt    //YUV444/YUV422/YUV420/RGB
	pix_wmif1->src_plan      = fs_plan;
	//prm_top.nro_fs_fmt.src_plan   //PLANAR_X1/X2/X3
	pix_wmif1->src_bit       = p010_mode ? BIT_P010 :  fs_bit;
	//prm_top.nro_fs_fmt.src_bit    //BIT_8/10/12
	pix_wmif1->src_cmpr      = CMPR_UN;
	//prm_top.nro_fs_fmt.src_cmpr   //un/afbc/afrc
	pix_wmif1->interlace     = IS_PSRC;
	//prm_top.nro_fs_fmt.interlace  //IS_PSRC/IS_ISRC
	pix_wmif1->src_hsize     = pps_en ?
		prm_dpe->dpe_nr_size.pps_hsize : prm_top->frm_hsize;
	pix_wmif1->src_vsize     = pps_en ?
		prm_dpe->dpe_nr_size.pps_vsize : prm_top->frm_vsize;
	pix_wmif1->bits_mode     = 7;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	pix_wmif1->pack_mode     = 1;
	//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	pix_wmif2->src_fmt       = fs_fmt;
	//prm_top.nro_fs_fmt.src_fmt    //YUV444/YUV422/YUV420/RGB
	pix_wmif2->src_plan      = fs_plan;
	//prm_top.nro_fs_fmt.src_plan   //PLANAR_X1/X2/X3
	pix_wmif2->src_bit       = p010_mode ? BIT_P010 :  fs_bit;
	//prm_top.nro_fs_fmt.src_bit    //BIT_8/10/12
	pix_wmif2->src_cmpr      = CMPR_UN;
	//prm_top.nro_fs_fmt.src_cmpr   //un/afbc/afrc
	pix_wmif2->interlace     = IS_PSRC;
	//prm_top.nro_fs_fmt.interlace  //IS_PSRC/IS_ISRC
	pix_wmif2->src_hsize     = prm_top->nr_hscale_on ?
		prm_dpe->dpe_nr_size.frm_hsize : prm_top->frm_hsize;
	pix_wmif2->src_vsize     = prm_top->nr_hscale_on ?
		prm_dpe->dpe_nr_size.frm_vsize : prm_top->frm_vsize;
	pix_wmif2->bits_mode     = 7;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	pix_wmif2->pack_mode     = 1;//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	pix_wmif0->mmu_mode_en   =  prm_top->mif_mmu_mode_en;
	pix_wmif1->mmu_mode_en   =  prm_top->mif_mmu_mode_en;
	pix_wmif2->mmu_mode_en   =  prm_top->mif_mmu_mode_en;

	switch (dpe_mode) {
	case DPE_NR_MODE:
		pix_wmif1->src_baddr[0]     = prm_top->src0_dio_fbuf_yaddr[dpe_src0_di_wr_idx] << 5;
		pix_wmif1->src_baddr[1]     = prm_top->src0_dio_fbuf_caddr[dpe_src0_di_wr_idx] << 5;
		pix_wmif2->src_baddr[0]     = prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[1]     = prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		dbg_h2("%s:addr:nro: nr_mode:wr pix_wmif2 :%d:yaddr:0x%x\n", __func__,
			dpe_src0_nr_wr_idx, pix_wmif2->src_baddr[0]);
		break;
	case DPE_VDI_MODE: //di_only
		pix_wmif0->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_nr_wr_idx] << 5;
		pix_wmif0->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[0] =
			prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 5;
		pix_wmif2->src_baddr[1] =
			prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 5;
		break;
	case DPE_NR_SRC1_MODE:
		pix_wmif2->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_nr_wr_idx] << 5;
		break;
	case DPE_NRDI_MODE:  //nr+di
		pix_wmif0->src_baddr[0] =
			prm_top->src1_nro_fbuf_yaddr[dpe_src1_nr_wr_idx] << 5;
		pix_wmif0->src_baddr[1] =
			prm_top->src1_nro_fbuf_caddr[dpe_src1_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[0] =
			prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 5;
		pix_wmif2->src_baddr[1] =
			prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 5;
		break;
	case DPE_NR_BYPS:
		pix_wmif2->src_baddr[0] =
			prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[1] =
			prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		dbg_h2("%s:addr:nro: DPE_NR_BYPS:wr pix_wmif2 :%d:yaddr:0x%x\n", __func__,
		dpe_src0_nr_wr_idx, pix_wmif2->src_baddr[0]);
		break;
	default:
		dbg_h2("%s:3 %d not in case\n", __func__, dpe_mode);
		break;
	}
	dbg_h2("%s:addr:var wmif set\n", __func__);
	for (i = 0; i < 4; i++) {
		pix_wmif0->luma_wr_x_st[i] = prm_dpe->prm_wmif0_intf.luma_wr_x_st[i];
		pix_wmif0->luma_wr_x_ed[i] = prm_dpe->prm_wmif0_intf.luma_wr_x_ed[i];
		pix_wmif0->luma_wr_y_st[i] = prm_dpe->prm_wmif0_intf.luma_wr_y_st[i];
		pix_wmif0->luma_wr_y_ed[i] = prm_dpe->prm_wmif0_intf.luma_wr_y_ed[i];
		pix_wmif0->chrm_wr_x_st[i] = prm_dpe->prm_wmif0_intf.chrm_wr_x_st[i];
		pix_wmif0->chrm_wr_x_ed[i] = prm_dpe->prm_wmif0_intf.chrm_wr_x_ed[i];
		pix_wmif0->chrm_wr_y_st[i] = prm_dpe->prm_wmif0_intf.chrm_wr_y_st[i];
		pix_wmif0->chrm_wr_y_ed[i] = prm_dpe->prm_wmif0_intf.chrm_wr_y_ed[i];

		pix_wmif1->luma_wr_x_st[i] = prm_dpe->prm_wmif1_intf.luma_wr_x_st[i];
		pix_wmif1->luma_wr_x_ed[i] = prm_dpe->prm_wmif1_intf.luma_wr_x_ed[i];
		pix_wmif1->luma_wr_y_st[i] = prm_dpe->prm_wmif1_intf.luma_wr_y_st[i];
		pix_wmif1->luma_wr_y_ed[i] = prm_dpe->prm_wmif1_intf.luma_wr_y_ed[i];
		pix_wmif1->chrm_wr_x_st[i] = prm_dpe->prm_wmif1_intf.chrm_wr_x_st[i];
		pix_wmif1->chrm_wr_x_ed[i] = prm_dpe->prm_wmif1_intf.chrm_wr_x_ed[i];
		pix_wmif1->chrm_wr_y_st[i] = prm_dpe->prm_wmif1_intf.chrm_wr_y_st[i];
		pix_wmif1->chrm_wr_y_ed[i] = prm_dpe->prm_wmif1_intf.chrm_wr_y_ed[i];

		pix_wmif2->luma_wr_x_st[i] = prm_dpe->prm_wmif2_intf.luma_wr_x_st[i];
		pix_wmif2->luma_wr_x_ed[i] = prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i];
		pix_wmif2->luma_wr_y_st[i] = prm_dpe->prm_wmif2_intf.luma_wr_y_st[i];
		pix_wmif2->luma_wr_y_ed[i] = prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i];
		pix_wmif2->chrm_wr_x_st[i] = prm_dpe->prm_wmif2_intf.chrm_wr_x_st[i];
		pix_wmif2->chrm_wr_x_ed[i] = prm_dpe->prm_wmif2_intf.chrm_wr_x_ed[i];
		pix_wmif2->chrm_wr_y_st[i] = prm_dpe->prm_wmif2_intf.chrm_wr_y_st[i];
		pix_wmif2->chrm_wr_y_ed[i] = prm_dpe->prm_wmif2_intf.chrm_wr_y_ed[i];
	}

	struct PRM_INTF_TYPE *ds_wmif0 = &g_ds_wmif0;
	struct PRM_INTF_TYPE *ds_wmif1 = &g_ds_wmif1;

	memset(ds_wmif0, 0, sizeof(struct PRM_INTF_TYPE));
	memset(ds_wmif1, 0, sizeof(struct PRM_INTF_TYPE));

	ds_wmif0->src_fmt       = ds_fmt;//YUV444/YUV422/YUV420/RGB
	ds_wmif0->src_plan      = ds_plan;//PLANAR_X1/X2/X3
	ds_wmif0->src_bit       = ds_bit;//BIT_8/10/12
	ds_wmif0->src_cmpr      = CMPR_UN;//un/afbc/afrc
	ds_wmif0->interlace     = IS_PSRC;//IS_PSRC/IS_ISRC
	ds_wmif0->src_hsize     = prm_top->nr_hscale_on ?
		(((prm_dpe->dpe_nr_size.frm_hsize + ((1 << dw_dsx) - 1))) >> dw_dsx) :
		(((prm_top->frm_hsize + ((1 << dw_dsx) - 1))) >> dw_dsx);
	ds_wmif0->src_vsize     = prm_top->nr_hscale_on ?
		(((prm_dpe->dpe_nr_size.frm_vsize + ((1 << dw_dsy) - 1))) >> dw_dsy) :
		(((prm_top->frm_vsize + ((1 << dw_dsy) - 1))) >> dw_dsy);

	if (pps_en) {
		ds_wmif0->src_hsize = (ds0_src_hsize + ((1 << dw_dsx) - 1)) >> dw_dsx;
		ds_wmif0->src_vsize = (ds0_src_vsize + ((1 << dw_dsy) - 1)) >> dw_dsy;
	}
	// below not use
	ds_wmif0->bits_mode = 7;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	ds_wmif0->pack_mode = 1;
	//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	ds_wmif1->src_fmt = ds_fmt;//YUV444/YUV422/YUV420/RGB
	ds_wmif1->src_plan = ds_plan;//PLANAR_X1/X2/X3
	ds_wmif1->src_bit = ds_bit;//BIT_8/10/12
	ds_wmif1->src_cmpr = CMPR_UN;//un/afbc/afrc
	ds_wmif1->interlace = IS_PSRC;//IS_PSRC/IS_ISRC
	ds_wmif1->src_hsize = (((prm_top->frm_hsize + ((1 << dw_dsx) - 1))) >> dw_dsx);
	ds_wmif1->src_vsize = (((prm_top->frm_vsize + ((1 << dw_dsy) - 1))) >> dw_dsy);
	// below not use
	ds_wmif1->bits_mode = 7;
	//0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b 9:24b 10:20b
	ds_wmif1->pack_mode = 1;
	//0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x

	switch (dpe_mode) {
	case DPE_NR_MODE:
		ds_wmif0->src_baddr[0] =
			prm_top->src0_nro_dwbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
			prm_top->src0_nro_dwbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		break;
	case DPE_VDI_MODE:   //di_only
		ds_wmif0->src_baddr[0] =
			prm_top->src1_dio_dwbuf_yaddr[dpe_src1_di_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
			prm_top->src1_dio_dwbuf_caddr[dpe_src1_di_wr_idx] << 5;
		break;
	case DPE_NR_SRC1_MODE:
		ds_wmif0->src_baddr[0] =
			prm_top->src1_nro_dwbuf_yaddr[dpe_src1_nr_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
			prm_top->src1_nro_dwbuf_caddr[dpe_src1_nr_wr_idx] << 5;
		break;
	case DPE_NRDI_MODE:  //nr+di
		ds_wmif0->src_baddr[0] =
			prm_top->src1_dio_dwbuf_yaddr[dpe_src1_di_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
			prm_top->src1_dio_dwbuf_caddr[dpe_src1_di_wr_idx] << 5;
		break;
	case DPE_NR_BYPS:
		ds_wmif0->src_baddr[0] =
			prm_top->src0_nro_dwbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
			prm_top->src0_nro_dwbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		break;
	default:
		dbg_h2("%s:2 %d not in case\n", __func__, dpe_mode);
		break;
	}
	dbg_h2("%s:this force addr:var ds_wmif0 set 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n", __func__,
			rd(DPSS_DPE_DIN_WR_BADDR0), rd(DPSS_DPE_DIN_WR_BADDR1),
			rd(DPSS_DPE_DIN_WR_BADDR4), rd(DPSS_DPE_DIN_WR_BADDR5),
			rd(DPSS_DPE_DIN_DWR_BADDR2), rd(DPSS_DPE_DIN_DWR_BADDR3));
	if (dpss_force_nr_debug > 0xf) {
		pix_wmif0->src_baddr[0] = rd(DPSS_DPE_DIN_WR_BADDR0);
		pix_wmif0->src_baddr[1] = rd(DPSS_DPE_DIN_WR_BADDR1);
		pix_wmif2->src_baddr[0] = rd(DPSS_DPE_DIN_WR_BADDR4);
		pix_wmif2->src_baddr[1] = rd(DPSS_DPE_DIN_WR_BADDR5);
		ds_wmif0->src_baddr[0] = rd(DPSS_DPE_DIN_DWR_BADDR2);
		ds_wmif0->src_baddr[1] = rd(DPSS_DPE_DIN_DWR_BADDR3);
	}

	for (i = 0; i < 4; i++) {
		ds_wmif0->luma_wr_x_st[i] = prm_dpe->prm_ds_wmif0_intf.luma_wr_x_st[i];
		ds_wmif0->luma_wr_x_ed[i] = prm_dpe->prm_ds_wmif0_intf.luma_wr_x_ed[i];
		ds_wmif0->luma_wr_y_st[i] = prm_dpe->prm_ds_wmif0_intf.luma_wr_y_st[i];
		ds_wmif0->luma_wr_y_ed[i] = prm_dpe->prm_ds_wmif0_intf.luma_wr_y_ed[i];
		ds_wmif0->chrm_wr_x_st[i] = prm_dpe->prm_ds_wmif0_intf.chrm_wr_x_st[i];
		ds_wmif0->chrm_wr_x_ed[i] = prm_dpe->prm_ds_wmif0_intf.chrm_wr_x_ed[i];
		ds_wmif0->chrm_wr_y_st[i] = prm_dpe->prm_ds_wmif0_intf.chrm_wr_y_st[i];
		ds_wmif0->chrm_wr_y_ed[i] = prm_dpe->prm_ds_wmif0_intf.chrm_wr_y_ed[i];

		ds_wmif1->luma_wr_x_st[i] = prm_dpe->prm_ds_wmif1_intf.luma_wr_x_st[i];
		ds_wmif1->luma_wr_x_ed[i] = prm_dpe->prm_ds_wmif1_intf.luma_wr_x_ed[i];
		ds_wmif1->luma_wr_y_st[i] = prm_dpe->prm_ds_wmif1_intf.luma_wr_y_st[i];
		ds_wmif1->luma_wr_y_ed[i] = prm_dpe->prm_ds_wmif1_intf.luma_wr_y_ed[i];
		ds_wmif1->chrm_wr_x_st[i] = prm_dpe->prm_ds_wmif1_intf.chrm_wr_x_st[i];
		ds_wmif1->chrm_wr_x_ed[i] = prm_dpe->prm_ds_wmif1_intf.chrm_wr_x_ed[i];
		ds_wmif1->chrm_wr_y_st[i] = prm_dpe->prm_ds_wmif1_intf.chrm_wr_y_st[i];
		ds_wmif1->chrm_wr_y_ed[i] = prm_dpe->prm_ds_wmif1_intf.chrm_wr_y_ed[i];
	}
	ds_wmif0->swap_64bit = prm_top->swap_64bit;
	ds_wmif0->little_endian = prm_top->l_endian;
	ds_wmif0->uv_swap = pix_wmif2->uv_swap;//test

	ds_wmif0->mmu_mode_en =  prm_top->ds_mif_mmu_mode_en;
	ds_wmif1->mmu_mode_en =  prm_top->ds_mif_mmu_mode_en;

	if (w0_sel != 7)
		cfg_dpe_pix_wrmif(DPSS_WMIF_DPE0, pix_wmif0);	//nr_wrmif
	if (w1_sel != 7)
		cfg_dpe_pix_wrmif(DPSS_WMIF_DPE1, pix_wmif1);	//di_wrmif

	if (vfce_on) { //for src0 dv & src1 i
		if (prm_top->nro_fs_fmt.src_cmpr == CMPR_UN) {
			dis_s_afce = true;
			dbg_h2("%s:%d:dis afce\n", __func__, prm_top->src_mode);
		}
	}

	if (vfce_on && !dis_s_afce) {
		//vfce
		u32 vfce_size = prm_top->frm_hsize / prm_top->dpe_slc_num;
		u32 vfce_fmt_map = prm_top->nro_fs_fmt.src_bit == BIT_008 ? 8  :
				prm_top->nro_fs_fmt.src_bit == BIT_010 ? 10 :
				prm_top->nro_fs_fmt.src_bit == BIT_012 ? 12 :
				prm_top->nro_fs_fmt.src_bit == BIT_P010 ? 10 :
				16;
		//channel 0
		struct VFCE_TYPE *vfce0 = &g_vfce0;

		init_vfce(0, vfce0);

		switch (dpe_mode) {
		case DPE_NR_MODE:
			vfce0->head_baddr_0 =
				prm_top->src0_nro_head_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->head_baddr_1 =
				prm_top->src0_nro_head_caddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_0 =
				prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_1 =
				prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 9;
			dbg_h2("%s:addr:nro: nr:wr vfce0 :%d:yaddr:0x%llx\n", __func__,
			dpe_src0_nr_wr_idx, vfce0->body_baddr_0);
			break;
		case DPE_VDI_MODE: //di_only
			vfce0->head_baddr_0 =
				prm_top->src1_dio_head_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->head_baddr_1 =
				prm_top->src1_dio_head_caddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_0 =
				prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_1 =
				prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 9;
			break;
		case DPE_NR_SRC1_MODE:
			vfce0->head_baddr_0 =
				prm_top->src1_nro_head_yaddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->head_baddr_1 =
				prm_top->src1_nro_head_caddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->body_baddr_0 =
				prm_top->src1_nro_fbuf_yaddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->body_baddr_1 =
				prm_top->src1_nro_fbuf_caddr[dpe_src1_nr_wr_idx] << 9;
			break;
		case DPE_NRDI_MODE:  //nr+d
			vfce0->head_baddr_0 =
				prm_top->src1_dio_head_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->head_baddr_1 =
				prm_top->src1_dio_head_caddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_0 =
				prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_1 =
				prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 9;
			break;
		case DPE_NR_BYPS:
			vfce0->head_baddr_0 =
				prm_top->src0_nro_head_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->head_baddr_1 =
				prm_top->src0_nro_head_caddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_0 =
				prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_1 =
				prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 9;
		dbg_h2("%s:addr:nro: DPE_NR_BYPS:wr vfce0 :%d:yaddr:0x%llx\n", __func__,
			dpe_src0_nr_wr_idx, vfce0->body_baddr_0);
			break;
		default:
			DBG_WARN("%s:%d not in case\n", __func__, dpe_mode);
			break;
		}

		vfce0->hsize_bgnd = prm_top->nr_hscale_on ?
			prm_dpe->dpe_nr_size.frm_hsize : prm_top->frm_hsize;
		vfce0->vsize_bgnd = prm_top->nr_hscale_on ?
			prm_dpe->dpe_nr_size.frm_vsize : prm_top->frm_vsize;

		for (i = 0; i < 4; i++) {
			vfce0->enc_slc_bgn_h[i] =
				prm_dpe->prm_wmif2_intf.luma_wr_x_st[i];
			vfce0->enc_slc_end_h[i] =
				prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i];
			vfce0->enc_slc_bgn_v[i] =
				prm_dpe->prm_wmif2_intf.luma_wr_y_st[i];
			vfce0->enc_slc_end_v[i] =
				prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i];
		}
		vfce0->reg_core_sel =
			prm_top->nro_fs_fmt.src_cmpr == CMPR_AFBC ? 0 : 1;
		//0:afbc 1:afrc
		vfce0->mmu_mode_en = prm_top->comp_setting.mmu_mode_en;
		vfce0->mmu_page_mode = prm_top->comp_setting.mmu_page_mode;
		vfce0->reg_444to422_mode = prm_top->comp_setting.reg_444to422_mode;
		vfce0->reg_inp_format = 0;//always YUV444
		vfce0->reg_enc_format = prm_top->nro_fs_fmt.src_fmt;
		vfce0->reg_compbits_y = vfce_fmt_map;
		vfce0->reg_compbits_c = vfce_fmt_map;
		vfce0->reg_ram_offset_mode = vfce_size > 1024 ? 1 : 0;
		vfce0->soft_slc_mode_en = prm_dpe->dpe_reg_mode.vfce_reg_mode;
		vfce0->soft_slc_num_h = prm_top->dpe_slc_num;
		vfce0->soft_slc_num_v = 1;
		vfce0->afrce.reg_afrc_head_mode =
			prm_top->comp_setting.afrc_head_mode;
		vfce0->afrce.reg_input_format_mode = 1;//always yuv
		vfce0->afrce.reg_comp_target_byte_0 =
			prm_top->comp_setting.afrc_target_byte_y;
		vfce0->afrce.reg_comp_target_byte_1 =
			prm_top->comp_setting.afrc_target_byte_c;
		cfg_vfce(1, 0, vfce0);
	} else {
		if ((dolby_parallel && prm_dpe->dpe_mode == DPE_NR_BYPS) ||
		    !dolby_parallel)
			cfg_dpe_pix_wrmif(DPSS_WMIF_DPE2, pix_wmif2); //wrmif2, todo!!!
	}
	if ((dolby_parallel && prm_dpe->dpe_mode == DPE_NR_BYPS) ||
	    !dolby_parallel)
		cfg_dpe_pix_wrmif(DPSS_WMIF_DPE3, ds_wmif0);	//nr_ds
}

void dpss_vbe_proc_byp(u32 path_id, bool pps_en)
{
	if (dpss_slt_mode)
		w_reg_bit(VPSS_VBE_CRC_CTRL, 7, 18, 3);
	dbg_h2("%s:%d\n", __func__, path_id);
	//close NR/DI en sync ctrl function, need confirm TODO
	wr(VPU_NR_TOP_SYNC_CTRL, 0xffffffff);
	//close wrmif enable
	w_reg_bit(DPSS_DPE_TOP_CTRL0, 0, 4, 4);
	//reg_wrmif_enable,{ds_mix,dblk_h,dblk_v,dmsq_wr}

	if (path_id == 1 || path_id == 2) {//di_path
		bool di_en = 0;

		//only enable di for di out wrmif
		//wr(VPU_NR_ENABLE, 0x0);
		w_reg_bit(VPU_NR_ENABLE, 0x0, 1, 31);
		w_reg_bit(VPU_NR_ENABLE, di_en, 9, 1);
		//dblk en close, data bypass, dblk 444to422 need close
		w_reg_bit(VPU_DBLK_MISC_CTRL, 0, 0, 1);
		//reg_dblk_inp_c44to42_en if dblk en close, close 444to422
		w_reg_bit(VPU_NR_CORE_MISC_CTRL, ((1 & 0x1) << 2) |
		//reg_nr_pre2_inp_c44to42_en
		((1 & 0x1) << 1) | //reg_nr_pre1_inp_c44to42_en
		((di_en & 0x1) << 0), 0, 3);//reg_nr_cur_inp_c44to42_en

		w_reg_bit(DPSS_DPE_HW_DBG, (!di_en), 3, 1);//di_done_mask
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 2, 1);//nr_done_mask
		//w_reg_bit(DPSS_DPE_INTF_DBG, 0b111110, 0,16); //reg_frm_end_mask
		//w_reg_bit(DPSS_DPE_INTF_AFBCD0,0,12+1,2);//nr data to vfce
		//w_reg_bit(DPSS_DPE_INTF_AFBCD0,0,12+3,2);//nr data to wmif2
		//w_reg_bit(DPSS_DPE_INTF_AFBCD0,0,12+9,2);//nr data to dsmif

		if (path_id == 1) { //src0 di
			w_reg_bit(DPSS_DPE_INTF_AFBCD0, 7, 12 + 4, 3);
			//di data switch off
			w_reg_bit(DPSS_DPE_INTF_AFBCD0, 7, 12 + 13, 3);
		//di ds data switch off
			//wmif1 frm_end_mask //for di2pps
			if (pps_en)
				w_reg_bit(DPSS_DPE_INTF_DBG, 1, 2, 1);
			else
				w_reg_bit(DPSS_DPE_INTF_DBG, 0, 2, 1);
			w_reg_bit(DPSS_DPE_INTF_DBG, 1, 0, 1);//wmif2  frm_end_mask
			w_reg_bit(DPSS_DPE_INTF_DBG, 1, 3, 1);//dswmif frm_end_mask
			w_reg_bit(DPSS_DPE_INTF_CTRL0, 0, 0, 2);//close ds wrmif0
			w_reg_bit(DPSS_DPE_INTF_CTRL0, 0, 2, 2);//close ds wrmif0
			w_reg_bit(DPSS_DPE_INTF_DBG, 0x7, 4, 3); //{dblkh,dblkv,dmsq}
			w_reg_bit(DPSS_DPE_HW_DBG, 0x1, 14, 1);//bbd mask for di
		} else { //src0 di+src1 dv
			w_reg_bit(DPSS_DPE_INTF_AFBCD0, 7, 12 + 10, 3);
			//di data switch off
			w_reg_bit(DPSS_DPE_INTF_DBG, 1, 2, 1);//wmif1  frm_end_mask
			w_reg_bit(DPSS_DPE_INTF_DBG, 0x7, 4, 3); //{dblkh,dblkv,dmsq}
		}
		if (dpss_force_nr_debug || dpss_nr_debug == 2) {
			w_reg_bit(DPSS_DPE_INTF_AFBCD0, 1, 12 + 4, 3);
			w_reg_bit(DPSS_DPE_INTF_AFBCD0, 3, 12 + 13, 2);
			w_reg_bit(DPSS_DPE_INTF_CTRL0, 1, 0, 2);//close ds wrmif0
			w_reg_bit(DPSS_DPE_INTF_CTRL0, 1, 2, 2);//close ds wrmif0
		}
		//w_reg_bit(DPSS_DPE_INTF_DBG, 0b1110110, 0,16);
		dbg_h2(" this is di bypass: %x %x =\n",
			rd(DPSS_DPE_INTF_AFBCD0), rd(DPSS_DPE_INTF_DBG));
	} else {//nr_path
		//close all process
		//wr(VPU_NR_ENABLE, 0x0);
		w_reg_bit(VPU_NR_ENABLE, 0x0, 1, 31);
		//close wrmif enable
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 2, 1);//nr_done_mask
		w_reg_bit(VPU_VBE_TOP_CTR0, 1, 28, 1); //reg_nr_byps_en
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 29, 3); //reg_nr_byps_mode
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 8, 3); //reg_nr_byps_sel
		w_reg_bit(DPSS_BBD_ONLY_CTRL, 1, 1, 1);//nr_cur_only
	}
}

//void cfg_dpe_dv(struct PRM_DPSS_TOP prm_top) {
// u32 hsize = prm_top.frm_hsize;
// u32 vsize = prm_top.frm_vsize;
// u32 slc_num= prm_top.dpe_sl_num;
//
// w_reg_bit(DPSS_DPE_PAD,96,8,8); //use mcpad as src1 dvpad
//}

void cfg_dpe_triggle(struct PRM_DPSS_TOP *prm_top)
{
	if (dpss_slt_mode)
		w_reg_bit(VPSS_VBE_CRC_CTRL, 7, 18, 3);

	w_reg_bit(DPSS_DPE_START_CTRL, 1, 31, 1);//pls_dpe_frm_start
}

void cfg_dpe_dv_triggle(void)
{
//w_reg_bit(DPSS_DPE_TOP_RESET, 1, 9, 1);//pls_dpe_frm_start
	dbg_h2("dv triggle\n");
	w_reg_bit(DPSS_DPE_START_CTRL, 1, 30, 1);//pls_dv_frm_start
}

//cfg_dpe_size
void hw_cfg_dpe_size(enum DPSS_WORK_MODE dpe_mode,
//	u32 slc_num,
//	u32 me_dsx,
//	u32 me_dsy,
//	struct PRM_DPE_SUB_RMIF prm_dpe_sub_rmif,
	struct PRM_DPSS_TOP *prm_top,
	struct PRM_DPSS_DPE *prm_dpe,
	struct PRM_DPE_PAD dpe_pad)
{
	//reg_mode 0~15
	int i;
	u32 src_hsize;
	u32 src_vsize;
	u32 frm_hsize;
	u32 frm_vsize;

	if (prm_top->nr_hscale_on) {
		src_hsize = prm_dpe->dpe_nr_size.src_hsize;
		src_vsize = prm_dpe->dpe_nr_size.src_vsize;
		frm_hsize = prm_dpe->dpe_nr_size.frm_hsize;
		frm_vsize = prm_dpe->dpe_nr_size.frm_vsize;
	} else {
		src_hsize = prm_top->auto_alig_en && (prm_top->org_hsize != 0xffff) ?
			prm_top->org_hsize : prm_top->frm_hsize;
		src_vsize = prm_top->auto_alig_en && (prm_top->org_hsize != 0xffff) ?
			prm_top->org_vsize : prm_top->frm_vsize;
		frm_hsize = prm_top->frm_hsize;
		frm_vsize = prm_top->frm_vsize;
	}

	if (prm_top->comp_setting.vfcd_h_skip) {
		src_hsize = prm_dpe->dpe_nr_size.src_hsize;
		frm_hsize = prm_dpe->dpe_nr_size.frm_hsize;
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 1, 24, 1);
		//reg_mif_win_mode,1-select reg config;0-use port config
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, 0, 0, 13);//xbgn
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, prm_top->frm_hsize - 1, 16, 13);
		//xend
	}

	if (prm_top->comp_setting.vfcd_v_skip) {
		src_vsize = prm_dpe->dpe_nr_size.src_vsize;
		frm_vsize = prm_dpe->dpe_nr_size.frm_vsize;
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 1, 25, 1);
		//reg_mif_win_mode,1-select reg config;0-use port config
		w_reg_bit(DPSS_DPE_VFCD_MIF0_VSIZE, prm_top->frm_vsize, 0, 13);
		//xbgn
	}

	u32 me_dsx = prm_top->dae_dsx_scale;
	u32 me_dsy = prm_top->dae_dsy_scale;
	u32 slc_num = prm_top->dpe_slc_num;
	u32 frm_hsize_m1 = frm_hsize - 1;
	u32 src_hsize_m1 = src_hsize - 1;
	u32 src_vsize_m1 = src_vsize - 1;
	u32 dpe_hsize[4];
	u32 dpe_vsize[4];
	u32 dmsq_hsize = (frm_hsize + 1) >> 1;//dms_ds_rate
	u32 dmsq_vsize = (frm_vsize + 1) >> 1;

	u32 dpe_hsize_slc = slc_num == 1 ? frm_hsize :
		(frm_hsize + slc_num - 1) / slc_num + 128;//todo ovlp ?
	u32 dpe_vsize_slc = slc_num == 1 ? frm_vsize :
		(frm_vsize + slc_num - 1) / slc_num + 128;

	for (i = 0; i < 4; i++) {
		dpe_hsize[i] = dpe_hsize_slc;
		dpe_vsize[i] = dpe_vsize_slc;
	}
	dbg_h2("%s:src:w*h:%d*%d, frm:w*h:%d*%d.\n",
		__func__, src_hsize, src_vsize, frm_hsize, frm_vsize);
	dbg_h2("%s:vsize:%d\n", __func__, dpe_vsize[0]); //for no use
	bool        mcnr_sel;
	u32    pc_pad_sel;
	u32    amdv_di = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;

	mcnr_sel = 1;

	//pc_pad_sel = mcnr_sel ? dpe_pad.reg_vbe_pad : dpe_pad.reg_pc_pad;
	//ary no use    u32    nrdi_pad = dpe_pad.reg_nrdi_pad;
	//ary no use    u32    aa_pad   = dpe_pad.reg_aa_pad;
	u32 mc_pad = dpe_pad.reg_mc_pad;
	u32 amdv_pad = dpe_pad.reg_dv_pad;
	u32 vbe_pad = dpe_pad.reg_vbe_pad;
	u32 vbe_dmsq_pad = dpe_pad.reg_dmsq_pad;
	u32 dcntr_pad = dpe_pad.reg_dcntr_pad;
	u32 amdv_lft_pad[4];
	u32 amdv_rgt_pad[4];
	u32 cur_lft_pad[4];
	u32 cur_rgt_pad[4];
	u32 pre_pad;
	u32 src_type;//0-p src,1-i src

	src_type = prm_top->src_fs_fmt.interlace == IS_ISRC;
	pc_pad_sel = vbe_pad;
	amdv_lft_pad[0] = 0;
	amdv_lft_pad[1] = slc_num == 2 ?
		amdv_pad : slc_num == 4 ? amdv_pad / 2 : 0;
	amdv_lft_pad[2] = amdv_pad / 2;
	amdv_lft_pad[3] = amdv_pad;// 2026-01-28 amdv_pad / 2;
	amdv_rgt_pad[0] = slc_num != 1 ? amdv_pad : 0;
	amdv_rgt_pad[1] = slc_num == 4 ? amdv_pad / 2 : 0;
	amdv_rgt_pad[2] = amdv_pad / 2;
	amdv_rgt_pad[3] = 0;

	cur_lft_pad[0] = 0;
	cur_lft_pad[1] = amdv_lft_pad[1] + dcntr_pad + vbe_pad;
	cur_lft_pad[2] = amdv_lft_pad[2] + dcntr_pad + vbe_pad;
	cur_lft_pad[3] = amdv_lft_pad[3] + dcntr_pad + vbe_pad;

	cur_rgt_pad[0] = amdv_rgt_pad[0] + dcntr_pad + vbe_pad;
	cur_rgt_pad[1] = amdv_rgt_pad[1] + dcntr_pad + vbe_pad;
	cur_rgt_pad[2] = amdv_rgt_pad[2] + dcntr_pad + vbe_pad;
	cur_rgt_pad[3] = 0;

	pre_pad = vbe_pad;
	prm_dpe->lst_overlap2 = pre_pad;

	u32 dmsq_xsft = prm_dpe->prm_dpe_sub_rmif.dmsq_xsft;

	dbg_h2("%s dpe_mode/mcnr_sel=%d %d\n", __func__, dpe_mode, mcnr_sel);

	wr(DPSS_DPE_DMSQ_SIZE, (1 << 29) | //reg_dmsq_xsft
				((dmsq_hsize & 0x1fff) << 16) | //reg_dmsq_hsize
				((dmsq_vsize & 0x1fff) << 0));//reg_dmsq_vsize

	if (prm_top->nr_src_align) {
		wr(DPSS_DPE_DIN_SEL_SIZE0, ((frm_hsize & 0x1fff) << 16) | //src_hsize   1920
					((frm_vsize & 0x1fff) << 0));//src_vsize
	} else {
		wr(DPSS_DPE_DIN_SEL_SIZE0, ((src_hsize & 0x1fff) << 16) | //src_hsize   1918
					((src_vsize & 0x1fff) << 0));//src_vsize
	}
	wr(DPSS_DPE_DIN_SEL_SIZE1, ((frm_hsize & 0x1fff) << 16) | //frm_hsize
				((frm_vsize & 0x1fff) << 0));//frm_vsize
	w_reg_bit(DPSS_DPE_TOP_SW_SEL0, 0xf, 6, 4);//
	//=================================================================================
	//dpe top size
	//=================================================================================
	u32    reg_slc0_hsize;
	u32    reg_slc1_hsize;
	u32    reg_slc2_hsize;
	u32    reg_slc3_hsize;
	u32    reg_luma_slc0_xbgn;
	u32    reg_luma_slc0_xend;
	u32    reg_luma_slc1_xbgn;
	u32    reg_luma_slc1_xend;
	u32    reg_luma_slc2_xbgn;
	u32    reg_luma_slc2_xend;
	u32    reg_luma_slc3_xbgn;
	u32    reg_luma_slc3_xend;
	u32    reg_chroma_slc0_xbgn;
	u32    reg_chroma_slc0_xend;
	u32    reg_chroma_slc1_xbgn;
	u32    reg_chroma_slc1_xend;
	u32    reg_chroma_slc2_xbgn;
	u32    reg_chroma_slc2_xend;
	u32    reg_chroma_slc3_xbgn;
	u32    reg_chroma_slc3_xend;
	u32    reg_out_slc0_xbgn;
	u32    reg_out_slc0_xend;
	u32    reg_out_slc1_xbgn;
	u32    reg_out_slc1_xend;
	u32    reg_out_slc2_xbgn;
	u32    reg_out_slc2_xend;
	u32    reg_out_slc3_xbgn;
	u32    reg_out_slc3_xend;
	u32    reg_dv_luma_slc0_xbgn = 0;
	u32    reg_dv_luma_slc0_xend = dpe_hsize[0] - 1;
	u32    reg_dv_luma_slc1_xbgn = 0;
	u32    reg_dv_luma_slc1_xend = dpe_hsize[1] - 1;
	u32    reg_dv_luma_slc2_xbgn = 0;
	u32    reg_dv_luma_slc2_xend = dpe_hsize[2] - 1;
	u32    reg_dv_luma_slc3_xbgn = 0;
	u32    reg_dv_luma_slc3_xend = dpe_hsize[3] - 1;
	u32    reg_dv_chroma_slc0_xbgn = 0;
	u32    reg_dv_chroma_slc0_xend = dpe_hsize[0] - 1;
	u32    reg_dv_chroma_slc1_xbgn = 0;
	u32    reg_dv_chroma_slc1_xend = dpe_hsize[1] - 1;
	u32    reg_dv_chroma_slc2_xbgn = 0;
	u32    reg_dv_chroma_slc2_xend = dpe_hsize[2] - 1;
	u32    reg_dv_chroma_slc3_xbgn = 0;
	u32    reg_dv_chroma_slc3_xend = dpe_hsize[3] - 1;
	u32    reg_dv_out_slc0_xbgn = 0;
	u32    reg_dv_out_slc0_xend = dpe_hsize[0] - 1;
	u32    reg_dv_out_slc1_xbgn = 0;
	u32    reg_dv_out_slc1_xend = dpe_hsize[1] - 1;
	u32    reg_dv_out_slc2_xbgn = 0;
	u32    reg_dv_out_slc2_xend = dpe_hsize[2] - 1;
	u32    reg_dv_out_slc3_xbgn = 0;
	u32    reg_dv_out_slc3_xend = dpe_hsize[3] - 1;

	u32 mc_slc0_hbgn;
	u32 mc_slc0_hend;
	u32 mc_slc1_hbgn;
	u32 mc_slc1_hend;
	u32 mc_slc2_hbgn;
	u32 mc_slc2_hend;
	u32 mc_slc3_hbgn;
	u32 mc_slc3_hend;

	//luma, chroma xbgn, xend
	u32 frm_hsize_d4;
	u32 frm_hsize_d4_d16;
	u32 frm_hsize_d4_d32;
	u32 frm_hsize_d4_d64;
	u32 frm_hsize_d4_d128;
	u32 frm_hsize_d4_rnd;
	u32 frm_hsize_sel = prm_top->frm_hsize_sel;//TODO

	w_reg_bit(DPSS_DPE_INTF_CTRL, frm_hsize_sel, 17, 3);

	u32 frm_hsize_t0;
	u32 frm_hsize_t1;
	u32 frm_hsize_t2;
	//ary no use    u32 frm_hsize_t3;

	frm_hsize_d4 = (frm_hsize + 3) >> 2;
	frm_hsize_d4_d16 = (frm_hsize_d4 + 15) >> 4;
	frm_hsize_d4_d32 = (frm_hsize_d4 + 31) >> 5;
	frm_hsize_d4_d64 = (frm_hsize_d4 + 63) >> 6;
	frm_hsize_d4_d128 = (frm_hsize_d4 + 127) >> 7;
	frm_hsize_d4_rnd = (frm_hsize_sel == 4) ? frm_hsize_d4_d128 << 7 :
			(frm_hsize_sel == 3) ? frm_hsize_d4_d64 << 6 :
			(frm_hsize_sel == 2) ? frm_hsize_d4_d32 << 5 :
			(frm_hsize_sel == 1) ? frm_hsize_d4_d16 << 4 :
			frm_hsize_d4;
	frm_hsize_t0 = frm_hsize_d4_rnd;
	frm_hsize_t1 = frm_hsize_d4_rnd << 1;
	frm_hsize_t2 = frm_hsize_t0 + frm_hsize_t1;
	reg_slc0_hsize = frm_hsize_t0;
	reg_slc1_hsize = frm_hsize_t0;
	reg_slc2_hsize = frm_hsize_t0;
	reg_slc3_hsize = frm_hsize - frm_hsize_t0 * 3;
	reg_out_slc0_xbgn = 0;
	reg_out_slc0_xend = slc_num == 4 ? frm_hsize_t0 - 1 :
				slc_num == 2 ? frm_hsize_t1 - 1 : frm_hsize_m1;
	reg_out_slc1_xbgn = slc_num == 4 ? frm_hsize_t0 : frm_hsize_t1;
	reg_out_slc1_xend = slc_num == 4 ? frm_hsize_t1 - 1 : frm_hsize_m1;
	reg_out_slc2_xbgn = frm_hsize_t1;
	reg_out_slc2_xend = frm_hsize_t2 - 1;
	reg_out_slc3_xbgn = frm_hsize_t2;
	reg_out_slc3_xend = frm_hsize_m1;

	reg_luma_slc0_xbgn = 0;
	reg_luma_slc0_xend = slc_num == 4 ?
		frm_hsize_t0 + pc_pad_sel - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t0 + pc_pad_sel - 1 :
		slc_num == 2 ? frm_hsize_t1 + pc_pad_sel - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t1 + pc_pad_sel - 1 : frm_hsize_m1;
	reg_luma_slc1_xbgn = slc_num == 4 ? frm_hsize_t0 < pc_pad_sel ?
		0 : frm_hsize_t0 - pc_pad_sel :
		frm_hsize_t1 < pc_pad_sel ?
		0 : frm_hsize_t1 - pc_pad_sel;
	reg_luma_slc1_xend = slc_num == 4 ?
		frm_hsize_t1 + pc_pad_sel - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t1 + pc_pad_sel - 1 : frm_hsize_m1;
	reg_luma_slc2_xbgn = frm_hsize_t1 < pc_pad_sel ?
			0 : frm_hsize_t1 - pc_pad_sel;
	reg_luma_slc2_xend = frm_hsize_t2 + pc_pad_sel - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t2 + pc_pad_sel - 1;
	reg_luma_slc3_xbgn = frm_hsize_t2 < pc_pad_sel ? 0 : frm_hsize_t2 - pc_pad_sel;
	reg_luma_slc3_xend = frm_hsize_m1;

	reg_chroma_slc0_xbgn = mcnr_sel ? reg_luma_slc0_xbgn : reg_luma_slc0_xbgn >> 1;
	reg_chroma_slc0_xend = mcnr_sel ? reg_luma_slc0_xend : reg_luma_slc0_xend >> 1;
	reg_chroma_slc1_xbgn = mcnr_sel ? reg_luma_slc1_xbgn : reg_luma_slc1_xbgn >> 1;
	reg_chroma_slc1_xend = mcnr_sel ? reg_luma_slc1_xend : reg_luma_slc1_xend >> 1;
	reg_chroma_slc2_xbgn = mcnr_sel ? reg_luma_slc2_xbgn : reg_luma_slc2_xbgn >> 1;
	reg_chroma_slc2_xend = mcnr_sel ? reg_luma_slc2_xend : reg_luma_slc2_xend >> 1;
	reg_chroma_slc3_xbgn = mcnr_sel ? reg_luma_slc3_xbgn : reg_luma_slc3_xbgn >> 1;
	reg_chroma_slc3_xend = mcnr_sel ? reg_luma_slc3_xend : reg_luma_slc3_xend >> 1;

	dbg_h2("[dpss_dpe.c]:luma_slc0_xend=%4d  chrm_slc0_xend=%4d\n",
			reg_luma_slc0_xend, reg_chroma_slc0_xend);
	dbg_h2("ary:%s:%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		reg_chroma_slc0_xbgn, reg_chroma_slc0_xend,
		reg_chroma_slc1_xbgn, reg_chroma_slc1_xend,
		reg_chroma_slc2_xbgn, reg_chroma_slc2_xend,
		reg_chroma_slc3_xbgn, reg_chroma_slc3_xend);
	u32 amdv_mif_luma_slc_xbgn[4] = {0, 0, 0, 0};//TODO
	u32 amdv_mif_luma_slc_xend[4] = {0, 0, 0, 0};
	u32 cur_mif_luma_slc_xbgn[4];
	u32 cur_mif_luma_slc_xend[4];
	u32 pre_mif_luma_slc_xbgn[4];
	u32 pre_mif_luma_slc_xend[4];

	cur_mif_luma_slc_xbgn[0] = 0;
	cur_mif_luma_slc_xend[0] = slc_num == 4 ?
		frm_hsize_t0 + cur_rgt_pad[0] - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t0 + cur_rgt_pad[0] - 1 : slc_num == 2 ?
		frm_hsize_t1 + cur_rgt_pad[0] - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t1 + cur_rgt_pad[0] - 1 : src_hsize_m1;
	cur_mif_luma_slc_xbgn[1] = slc_num == 4 ?
		frm_hsize_t0 < cur_lft_pad[1] ? 0 : frm_hsize_t0 - cur_lft_pad[1] :
		frm_hsize_t1 < cur_lft_pad[1] ? 0 : frm_hsize_t1 - cur_lft_pad[1];
	cur_mif_luma_slc_xend[1] = slc_num == 4 ?
		frm_hsize_t1 + cur_rgt_pad[1] - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t1 + cur_rgt_pad[1] - 1 : src_hsize_m1;
	cur_mif_luma_slc_xbgn[2] = frm_hsize_t1 < cur_lft_pad[2] ?
		0 : frm_hsize_t1 - cur_lft_pad[2];
	cur_mif_luma_slc_xend[2] = frm_hsize_t2 + cur_rgt_pad[2] - 1 > src_hsize_m1 ?
				src_hsize_m1 : frm_hsize_t2 + cur_rgt_pad[2] - 1;
	cur_mif_luma_slc_xbgn[3] = frm_hsize_t2 < cur_lft_pad[3] ?
		0 : frm_hsize_t2 - cur_lft_pad[3];
	cur_mif_luma_slc_xend[3] = src_hsize_m1;

	pre_mif_luma_slc_xbgn[0] = 0;
	pre_mif_luma_slc_xend[0] = slc_num == 4 ?
		frm_hsize_t0 + pre_pad - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t0 + pre_pad - 1 :
		slc_num == 2 ? frm_hsize_t1 + pre_pad - 1 > src_hsize_m1 ?
			src_hsize_m1 : frm_hsize_t1 + pre_pad - 1 : src_hsize_m1;
	pre_mif_luma_slc_xbgn[1] = slc_num == 4 ?
		frm_hsize_t0 < pre_pad ? 0 : frm_hsize_t0 - pre_pad :
					frm_hsize_t1 < pre_pad ?
						0 : frm_hsize_t1 - pre_pad;
	pre_mif_luma_slc_xend[1] = slc_num == 4 ?
		frm_hsize_t1 + pre_pad - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t1 + pre_pad - 1 : src_hsize_m1;
	pre_mif_luma_slc_xbgn[2] = frm_hsize_t1 < pre_pad ? 0 : frm_hsize_t1 - pre_pad;
	pre_mif_luma_slc_xend[2] = frm_hsize_t2 + pre_pad - 1 > src_hsize_m1 ?
		src_hsize_m1 : frm_hsize_t2 + pre_pad - 1;
	pre_mif_luma_slc_xbgn[3] = frm_hsize_t2 < pre_pad ? 0 : frm_hsize_t2 - pre_pad;
	pre_mif_luma_slc_xend[3] = src_hsize_m1;

	//ary set no use    u32    rmif_luma_slc_xbgn[4];
	//ary set no use    u32    rmif_luma_slc_xend[4];
	u32 wmif_luma_slc_xbgn[4];
	u32 wmif_luma_slc_xend[4];

	wmif_luma_slc_xbgn[0] = reg_out_slc0_xbgn;
	wmif_luma_slc_xend[0] = reg_out_slc0_xend;
	wmif_luma_slc_xbgn[1] = reg_out_slc1_xbgn;
	wmif_luma_slc_xend[1] = reg_out_slc1_xend;
	wmif_luma_slc_xbgn[2] = reg_out_slc2_xbgn;
	wmif_luma_slc_xend[2] = reg_out_slc2_xend;
	wmif_luma_slc_xbgn[3] = reg_out_slc3_xbgn;
	wmif_luma_slc_xend[3] = reg_out_slc3_xend;

	if (prm_top->nr_src_align) {
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 1, 24, 1);
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, cur_mif_luma_slc_xbgn[0], 0, 13);//xbgn
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, cur_mif_luma_slc_xend[0], 16, 13);//xend
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC1, cur_mif_luma_slc_xbgn[1], 0, 13);//xbgn
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC1, cur_mif_luma_slc_xend[1], 16, 13);//xend
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC2, cur_mif_luma_slc_xbgn[2], 0, 13);//xbgn
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC2, cur_mif_luma_slc_xend[2], 16, 13);//xend
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC3, cur_mif_luma_slc_xbgn[3], 0, 13);//xbgn
		w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC3, cur_mif_luma_slc_xend[3], 16, 13);//xend
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 27, 1);
	} else {
		w_reg_bit(DPSS_DPE_MIF_CTRL0, 0, 24, 1);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 0, 27, 1);
	}
	enum vid_src_fmt src_fmt = prm_top->nro_fs_fmt.src_fmt;
	u32 di_en = dpe_mode == DPE_VDI_MODE || dpe_mode == DPE_NRDI_MODE;

	u32 di_cur_ybgn = prm_dpe->dpss_dpe_di_frm_cnt & 0x1;	//yu.zong 2024-12-06
	u32 di_cur_yend = (src_vsize <<  1) - 1;	//yu.zong 2024-12-06

	for (i = 0; i < 4; i++) {
		prm_dpe->pix_rmif0.slc_x_st[i] = amdv_di ?
			amdv_mif_luma_slc_xbgn[i] : cur_mif_luma_slc_xbgn[i];
		prm_dpe->pix_rmif0.slc_x_ed[i] = amdv_di ?
			amdv_mif_luma_slc_xend[i] : cur_mif_luma_slc_xend[i];
		prm_dpe->pix_rmif0.slc_y_st[i] = 0;
		prm_dpe->pix_rmif0.slc_y_ed[i] = src_vsize_m1;
		prm_dpe->pix_rmif1.slc_x_st[i] = src_type ?
			cur_mif_luma_slc_xbgn[i] : pre_mif_luma_slc_xbgn[i];
		prm_dpe->pix_rmif1.slc_x_ed[i] = src_type ?
			cur_mif_luma_slc_xend[i] : pre_mif_luma_slc_xend[i];
#ifdef MOV		//yu.zong 2024-12-06
		prm_dpe->pix_rmif1.slc_y_st[i] = 0;
		prm_dpe->pix_rmif1.slc_y_ed[i] = src_vsize_m1;
#else
		prm_dpe->pix_rmif1.slc_y_st[i] = prm_top->di_interlace ?
			di_cur_ybgn : 0;
		prm_dpe->pix_rmif1.slc_y_ed[i] = prm_top->di_interlace ?
			di_cur_yend : src_vsize_m1;
#endif
		prm_dpe->pix_rmif2.slc_x_st[i] = pre_mif_luma_slc_xbgn[i];
		prm_dpe->pix_rmif2.slc_x_ed[i] = pre_mif_luma_slc_xend[i];
		prm_dpe->pix_rmif2.slc_y_st[i] = 0;
		prm_dpe->pix_rmif2.slc_y_ed[i] = src_vsize_m1;

		prm_dpe->pix_rmif3.slc_x_st[i] = pre_mif_luma_slc_xbgn[i];
		prm_dpe->pix_rmif3.slc_x_ed[i] = pre_mif_luma_slc_xend[i];
		prm_dpe->pix_rmif3.slc_y_st[i] = 0;
		prm_dpe->pix_rmif3.slc_y_ed[i] = src_vsize_m1;

		prm_dpe->prm_wmif0_intf.luma_wr_x_st[i] = wmif_luma_slc_xbgn[i];
		prm_dpe->prm_wmif0_intf.luma_wr_x_ed[i] = wmif_luma_slc_xend[i];
		prm_dpe->prm_wmif0_intf.luma_wr_y_st[i] = 0;
		prm_dpe->prm_wmif0_intf.luma_wr_y_ed[i] = frm_vsize - 1;

		prm_dpe->prm_wmif1_intf.luma_wr_x_st[i] = wmif_luma_slc_xbgn[i];
		prm_dpe->prm_wmif1_intf.luma_wr_x_ed[i] = wmif_luma_slc_xend[i];
		prm_dpe->prm_wmif1_intf.luma_wr_y_st[i] = 0;
		prm_dpe->prm_wmif1_intf.luma_wr_y_ed[i] = frm_vsize - 1;

		prm_dpe->prm_wmif2_intf.luma_wr_x_st[i] = wmif_luma_slc_xbgn[i];
		prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i] = wmif_luma_slc_xend[i];
		prm_dpe->prm_wmif2_intf.luma_wr_y_st[i] = 0;
		prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i] =
			(frm_vsize << (di_en & 0x1)) - 1;
		if (dpss_force_nr_debug > 0xf)
			prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i] =
				frm_vsize - 1;
		dbg_h2(" this force v: =%x\n", frm_vsize);

		prm_dpe->prm_wmif0_intf.chrm_wr_x_st[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif0_intf.luma_wr_x_st[i] :
				prm_dpe->prm_wmif0_intf.luma_wr_x_st[i] >> 1;
		prm_dpe->prm_wmif0_intf.chrm_wr_x_ed[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif0_intf.luma_wr_x_ed[i] :
				prm_dpe->prm_wmif0_intf.luma_wr_x_ed[i] >> 1;
		prm_dpe->prm_wmif0_intf.chrm_wr_y_st[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif0_intf.luma_wr_y_st[i] >> 1 :
				prm_dpe->prm_wmif0_intf.luma_wr_y_st[i];
		prm_dpe->prm_wmif0_intf.chrm_wr_y_ed[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif0_intf.luma_wr_y_ed[i] >> 1 :
				prm_dpe->prm_wmif0_intf.luma_wr_y_ed[i];
		prm_dpe->prm_wmif1_intf.chrm_wr_x_st[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif1_intf.luma_wr_x_st[i] :
				prm_dpe->prm_wmif1_intf.luma_wr_x_st[i] >> 1;
		prm_dpe->prm_wmif1_intf.chrm_wr_x_ed[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif1_intf.luma_wr_x_ed[i] :
				prm_dpe->prm_wmif1_intf.luma_wr_x_ed[i] >> 1;
		prm_dpe->prm_wmif1_intf.chrm_wr_y_st[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif1_intf.luma_wr_y_st[i] >> 1 :
				prm_dpe->prm_wmif1_intf.luma_wr_y_st[i];
		prm_dpe->prm_wmif1_intf.chrm_wr_y_ed[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif1_intf.luma_wr_y_ed[i] >> 1 :
				prm_dpe->prm_wmif1_intf.luma_wr_y_ed[i];
		prm_dpe->prm_wmif2_intf.chrm_wr_x_st[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif2_intf.luma_wr_x_st[i] :
				prm_dpe->prm_wmif2_intf.luma_wr_x_st[i] >> 1;
		prm_dpe->prm_wmif2_intf.chrm_wr_x_ed[i] = src_fmt == YUV444 ?
			prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i] :
				prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i] >> 1;
		prm_dpe->prm_wmif2_intf.chrm_wr_y_st[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif2_intf.luma_wr_y_st[i] >> 1 :
				prm_dpe->prm_wmif2_intf.luma_wr_y_st[i];
		prm_dpe->prm_wmif2_intf.chrm_wr_y_ed[i] = src_fmt == YUV420 ?
			prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i] >> 1 :
				prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i];

		prm_dpe->prm_ds_wmif0_intf.luma_wr_x_st[i] =
			prm_dpe->prm_wmif2_intf.luma_wr_x_st[i] >> prm_top->dpe_dw_dsx;
		prm_dpe->prm_ds_wmif0_intf.luma_wr_x_ed[i] =
			prm_dpe->prm_wmif2_intf.luma_wr_x_ed[i] >> prm_top->dpe_dw_dsx;
		prm_dpe->prm_ds_wmif0_intf.luma_wr_y_st[i] =
			prm_dpe->prm_wmif2_intf.luma_wr_y_st[i] >> prm_top->dpe_dw_dsy;
		prm_dpe->prm_ds_wmif0_intf.luma_wr_y_ed[i] =
			prm_dpe->prm_wmif2_intf.luma_wr_y_ed[i] >> prm_top->dpe_dw_dsy;

		prm_dpe->prm_ds_wmif0_intf.chrm_wr_x_st[i] =
			prm_dpe->prm_wmif2_intf.chrm_wr_x_st[i] >> prm_top->dpe_dw_dsx;
		prm_dpe->prm_ds_wmif0_intf.chrm_wr_x_ed[i] =
			prm_dpe->prm_wmif2_intf.chrm_wr_x_ed[i] >> prm_top->dpe_dw_dsx;
		prm_dpe->prm_ds_wmif0_intf.chrm_wr_y_st[i] =
			prm_dpe->prm_wmif2_intf.chrm_wr_y_st[i] >> prm_top->dpe_dw_dsy;
		prm_dpe->prm_ds_wmif0_intf.chrm_wr_y_ed[i] =
			prm_dpe->prm_wmif2_intf.chrm_wr_y_ed[i] >> prm_top->dpe_dw_dsy;
	}
#ifdef FUNC_EN_HDR	//07-10 need check
	//cfg hdr size with overlap
	u32 vbe_hdr_sel = rd(VPU_VBE_TOP_HDR_CTRL) & 0x3;//0:dcnt;1:nr;2:di;3:before dpe
	u32 hdr_slc_hsize[4];
	u32 remove_pad[4];
	u32 hdr_reg_offset = 0;
	u32 cur_alig_mif_luma_slc_xbgn[4];
	u32 cur_alig_mif_luma_slc_xend[4];

	cur_alig_mif_luma_slc_xbgn[0] = 0;
	cur_alig_mif_luma_slc_xend[0] = slc_num == 4 ?
		frm_hsize_t0 + cur_rgt_pad[0] - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t0 + cur_rgt_pad[0] - 1 : slc_num == 2 ?
		frm_hsize_t1 + cur_rgt_pad[0] - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t1 + cur_rgt_pad[0] - 1 : frm_hsize_m1;
	cur_alig_mif_luma_slc_xbgn[1] = slc_num == 4 ?
		frm_hsize_t0 < cur_lft_pad[1] ? 0 : frm_hsize_t0 - cur_lft_pad[1] :
		frm_hsize_t1 < cur_lft_pad[1] ? 0 : frm_hsize_t1 - cur_lft_pad[1];
	cur_alig_mif_luma_slc_xend[1] = slc_num == 4 ?
		frm_hsize_t1 + cur_rgt_pad[1] - 1 > frm_hsize_m1 ?
		frm_hsize_m1 : frm_hsize_t1 + cur_rgt_pad[1] - 1 : frm_hsize_m1;
	cur_alig_mif_luma_slc_xbgn[2] = frm_hsize_t1 < cur_lft_pad[2] ?
		0 : frm_hsize_t1 - cur_lft_pad[2];
	cur_alig_mif_luma_slc_xend[2] = frm_hsize_t2 + cur_rgt_pad[2] - 1 > frm_hsize_m1 ?
				frm_hsize_m1 : frm_hsize_t2 + cur_rgt_pad[2] - 1;
	cur_alig_mif_luma_slc_xbgn[3] = frm_hsize_t2 < cur_lft_pad[3] ?
		0 : frm_hsize_t2 - cur_lft_pad[3];
	cur_alig_mif_luma_slc_xend[3] = frm_hsize_m1;

	dbg_h2("[dpe.c]:vbe_hdr_sel=%d\n", vbe_hdr_sel);

	if (vbe_hdr_sel == 3) {
		remove_pad[0] = 0;
		remove_pad[1] = 0;
		remove_pad[2] = 0;
		remove_pad[3] = 0;
	} else if (vbe_hdr_sel == 0) {//dcntr to hdr
		remove_pad[0] = slc_num == 1 ? 0 : amdv_rgt_pad[0] + dcntr_pad;
		remove_pad[1] = amdv_rgt_pad[0] + amdv_lft_pad[1] +
			dcntr_pad * (slc_num / 2);
		remove_pad[2] = amdv_rgt_pad[0] + amdv_lft_pad[2] +
			dcntr_pad * 2;
		remove_pad[3] = amdv_lft_pad[3] + dcntr_pad;
	} else {//nr,di to hdr
		remove_pad[0] = slc_num == 1 ? 0 : amdv_rgt_pad[0] +
			dcntr_pad + vbe_pad;
		remove_pad[1] = amdv_rgt_pad[0] + amdv_lft_pad[1] +
			(dcntr_pad + vbe_pad) * (slc_num / 2);
		remove_pad[2] = amdv_rgt_pad[0] + amdv_lft_pad[2] +
			(dcntr_pad + vbe_pad) * 2;
		remove_pad[3] = amdv_lft_pad[3] + dcntr_pad + vbe_pad;
	}

	for (i = 0; i < 4; i++) {
		hdr_slc_hsize[i] = cur_alig_mif_luma_slc_xend[i] -
			cur_alig_mif_luma_slc_xbgn[i] + 1 - remove_pad[i];
		dbg_h2("[dpe.c]:hdr_slc_hsize/remove_pad[%d]=%d/%d\n",
			i, hdr_slc_hsize[i], remove_pad[i]);
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6X)
		hdr_reg_offset = 0x1;

	w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_0 + hdr_reg_offset,
		hdr_slc_hsize[0], 0, 13);
	w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_0 + hdr_reg_offset,
		hdr_slc_hsize[1], 16, 13);
	w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_1 + hdr_reg_offset,
		hdr_slc_hsize[2], 0, 13);
	w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_1 + hdr_reg_offset,
		hdr_slc_hsize[3], 16, 13);
	wr_vc(VPU_HDR2_SIZE_IN + hdr_reg_offset,
		(frm_vsize << 16) | frm_hsize);

	//cfg hdr hist size
	u32 hdr_hist_slc_xbgn[4];
	u32 hdr_hist_slc_xend[4];

	if (vbe_hdr_sel == 3) {
		hdr_hist_slc_xbgn[0] = 0;
		hdr_hist_slc_xbgn[1] = cur_lft_pad[1];
		hdr_hist_slc_xbgn[2] = cur_lft_pad[2];
		hdr_hist_slc_xbgn[3] = cur_lft_pad[3];
	} else if (vbe_hdr_sel == 0) {//dcntr to hdr
		hdr_hist_slc_xbgn[0] = 0;
		hdr_hist_slc_xbgn[1] = cur_lft_pad[1] - dcntr_pad;
		hdr_hist_slc_xbgn[2] = cur_lft_pad[2] - dcntr_pad;
		hdr_hist_slc_xbgn[3] = cur_lft_pad[3] - dcntr_pad;
	} else {
		hdr_hist_slc_xbgn[0] = 0;
		hdr_hist_slc_xbgn[1] = 0;
		hdr_hist_slc_xbgn[2] = 0;
		hdr_hist_slc_xbgn[3] = 0;
	}

	hdr_hist_slc_xend[0] =
		slc_num == 4 ? hdr_hist_slc_xbgn[0] + frm_hsize_t0 - 1 :
		slc_num == 2 ? hdr_hist_slc_xbgn[0] + frm_hsize_t1 - 1 :
		frm_hsize_m1;
	hdr_hist_slc_xend[1] =
		slc_num == 4 ? hdr_hist_slc_xbgn[1] + frm_hsize_t0 - 1 :
		hdr_hist_slc_xbgn[1] + frm_hsize_t1 - 1;
	hdr_hist_slc_xend[2] = hdr_hist_slc_xbgn[2] + frm_hsize_t0 - 1;
	hdr_hist_slc_xend[3] = hdr_hist_slc_xbgn[3] + frm_hsize_t0 - 1;

	wr_vc(DOLBY_HDR2_HIST_H_START_END,
		hdr_hist_slc_xend[0] | (hdr_hist_slc_xbgn[0] << 16));
	wr_vc(DOLBY_HDR2_HIST_V_START_END, frm_vsize - 1);
	wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_0,
		hdr_hist_slc_xend[0] | (hdr_hist_slc_xbgn[0] << 16) | (slc_num << 29));
	wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_1,
		hdr_hist_slc_xend[1] | (hdr_hist_slc_xbgn[1] << 16));
	wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_2,
		hdr_hist_slc_xend[2] | (hdr_hist_slc_xbgn[2] << 16));
	wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_3,
		hdr_hist_slc_xend[3] | (hdr_hist_slc_xbgn[3] << 16));
#endif /* FUNC_EN_HDR */
	mc_slc0_hbgn = 0;
	mc_slc0_hend = slc_num == 4 ? frm_hsize_t0 + mc_pad - 1 :
		slc_num == 2 ? frm_hsize_t1 + mc_pad - 1 : frm_hsize_m1;
	mc_slc1_hbgn = slc_num == 4 ? frm_hsize_t0 - mc_pad : frm_hsize_t1 - mc_pad;
	mc_slc1_hend = slc_num == 4 ? frm_hsize_t1 + mc_pad - 1 : frm_hsize_m1;
	mc_slc2_hbgn = frm_hsize_t1 - mc_pad;
	mc_slc2_hend = frm_hsize_t2 + mc_pad - 1;
	mc_slc3_hbgn = frm_hsize_t2 - mc_pad;
	mc_slc3_hend = frm_hsize_m1;

	wr(DPSS_DPE_SLC_SIZE0, reg_slc1_hsize << 16 | reg_slc0_hsize);
	wr(DPSS_DPE_SLC_SIZE1, reg_slc3_hsize << 16 | reg_slc2_hsize);
	if (prm_top->comp_setting.vfce_in_overlap > 0) { //12-05
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 1, 1);
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 11, 1);
		wr(DPSS_DPE_DS_IN_X_SLC0, reg_out_slc0_xend << 16 | reg_out_slc0_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC1, reg_out_slc1_xend << 16 | reg_out_slc1_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC2, reg_out_slc2_xend << 16 | reg_out_slc2_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC3, reg_out_slc3_xend << 16 | reg_out_slc3_xbgn);
		reg_out_slc0_xend =
			reg_out_slc0_xend + prm_top->comp_setting.vfce_in_overlap;
		reg_out_slc1_xend =
			reg_out_slc1_xend + prm_top->comp_setting.vfce_in_overlap;
		reg_out_slc2_xend =
			reg_out_slc2_xend + prm_top->comp_setting.vfce_in_overlap;
	}

	wr(DPSS_DPE_PATH_OUT_X_SLC0, reg_out_slc0_xend << 16 | reg_out_slc0_xbgn);
	wr(DPSS_DPE_PATH_OUT_X_SLC1, reg_out_slc1_xend << 16 | reg_out_slc1_xbgn);
	wr(DPSS_DPE_PATH_OUT_X_SLC2, reg_out_slc2_xend << 16 | reg_out_slc2_xbgn);
	wr(DPSS_DPE_PATH_OUT_X_SLC3, reg_out_slc3_xend << 16 | reg_out_slc3_xbgn);
	wr(DPSS_DPE_PATH_DV_LUMA_X_SLC0,
		reg_dv_luma_slc0_xend << 16 | reg_dv_luma_slc0_xbgn);
	wr(DPSS_DPE_PATH_DV_LUMA_X_SLC1,
		reg_dv_luma_slc1_xend << 16 | reg_dv_luma_slc1_xbgn);
	wr(DPSS_DPE_PATH_DV_LUMA_X_SLC2,
		reg_dv_luma_slc2_xend << 16 | reg_dv_luma_slc2_xbgn);
	wr(DPSS_DPE_PATH_DV_LUMA_X_SLC3,
		reg_dv_luma_slc3_xend << 16 | reg_dv_luma_slc3_xbgn);
	wr(DPSS_DPE_PATH_DV_CHRM_X_SLC0,
		reg_dv_chroma_slc0_xend << 16 | reg_dv_chroma_slc0_xbgn);
	wr(DPSS_DPE_PATH_DV_CHRM_X_SLC1,
		reg_dv_chroma_slc1_xend << 16 | reg_dv_chroma_slc1_xbgn);
	wr(DPSS_DPE_PATH_DV_CHRM_X_SLC2,
		reg_dv_chroma_slc2_xend << 16 | reg_dv_chroma_slc2_xbgn);
	wr(DPSS_DPE_PATH_DV_CHRM_X_SLC3,
		reg_dv_chroma_slc3_xend << 16 | reg_dv_chroma_slc3_xbgn);
	wr(DPSS_DPE_PATH_DV_OUT_X_SLC0,
		reg_dv_out_slc0_xend << 16 | reg_dv_out_slc0_xbgn);
	wr(DPSS_DPE_PATH_DV_OUT_X_SLC1,
		reg_dv_out_slc1_xend << 16 | reg_dv_out_slc1_xbgn);
	wr(DPSS_DPE_PATH_DV_OUT_X_SLC2,
		reg_dv_out_slc2_xend << 16 | reg_dv_out_slc2_xbgn);
	wr(DPSS_DPE_PATH_DV_OUT_X_SLC3,
		reg_dv_out_slc3_xend << 16 | reg_dv_out_slc3_xbgn);
	if (prm_top->dpe_out2bbd_en == 1 && prm_top->dpe_out2bbd_mode <= 1) {
		//crop before aa_pps
		wr(DPSS_DPE_DS_IN_X_SLC0, reg_out_slc0_xend << 16 | reg_out_slc0_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC1, reg_out_slc1_xend << 16 | reg_out_slc1_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC2, reg_out_slc2_xend << 16 | reg_out_slc2_xbgn);
		wr(DPSS_DPE_DS_IN_X_SLC3, reg_out_slc3_xend << 16 | reg_out_slc3_xbgn);
	}
	//=================================================================================
	//sub rdmif size
	//=================================================================================
	u32 rdmif0_slc_hbgn[4];
	u32 rdmif0_slc_hend[4];
	u32 rdmif0_slc_vbgn[4];
	u32 rdmif0_slc_vend[4];
	u32 rdmif1_slc_hbgn[4];
	u32 rdmif1_slc_hend[4];
	u32 rdmif1_slc_vbgn[4];
	u32 rdmif1_slc_vend[4];
	u32 rdmif2_slc_hbgn[4];
	u32 rdmif2_slc_hend[4];
	u32 rdmif2_slc_vbgn[4];
	u32 rdmif2_slc_vend[4];
	u32 rdmif3_slc_hbgn[4];
	u32 rdmif3_slc_hend[4];
	u32 rdmif3_slc_vbgn[4];
	u32 rdmif3_slc_vend[4];
	u32 rdmif4_slc_hbgn[4];
	u32 rdmif4_slc_hend[4];
	u32 rdmif4_slc_vbgn[4];
	u32 rdmif4_slc_vend[4];
	u32 rdmif5_slc_hbgn[4];
	u32 rdmif5_slc_hend[4];
	u32 rdmif5_slc_vbgn[4];
	u32 rdmif5_slc_vend[4];
	u32 rdmif6_slc_hbgn[4];
	u32 rdmif6_slc_hend[4];
	u32 rdmif6_slc_vbgn[4];
	u32 rdmif6_slc_vend[4];

	u32 rdmif0_hsize;
	//ary no use u32 rdmif0_vsize;
	u32 rdmif1_hsize;
	//ary no use u32 rdmif1_vsize;
	u32 rdmif2_hsize;
	//ary no use u32 rdmif2_vsize;
	u32 rdmif3_hsize;
	//ary no use u32 rdmif3_vsize;
	u32 rdmif4_hsize;
	//ary no use u32 rdmif4_vsize;
	u32 rdmif5_hsize;
	//ary no use u32 rdmif5_vsize;
	u32 rdmif6_hsize;
	u32 vbe_mv_hbgnx4[4];
	u32 vbe_mv_hendx4[4];
	u32 vbe_mv_vbgnx4[4];
	u32 vbe_mv_vendx4[4];
	u32 mc_logo_hbgnx4[4];
	u32 mc_logo_hendx4[4];
	u32 mc_logo_vbgnx4[4];
	u32 mc_logo_vendx4[4];
	u32 vbe_me_hbgnx4[4];
	u32 vbe_me_hendx4[4];
	u32 vbe_me_vbgnx4[4];
	u32 vbe_me_vendx4[4];
	u32 mc_mv_hbgnx4[4];
	u32 mc_mv_hendx4[4];
	u32 mc_mv_vbgnx4[4];
	u32 mc_mv_vendx4[4];
	u32 mc_melg_hbgnx4[4];
	u32 mc_melg_hendx4[4];
	u32 mc_melg_vbgnx4[4];
	u32 mc_melg_vendx4[4];
	u32 mc_lbuf_mode = 0;//TODO
	u32 vbe_me_pad;

	//u32 me_dsx = prm_top.dae_dsx_scale;
	//u32 me_dsy = prm_top.dae_dsy_scale;
	vbe_me_pad = me_dsx == 0 ? vbe_pad : me_dsx == 1 ?
		((vbe_pad + 1) >> 1) : ((vbe_pad + 2) >> 2);

	u32 vbe_me_hsize_t0;
	u32 vbe_me_hsize_t1;
	u32 vbe_me_hsize_t2;
	u32 vbe_me_hsize;
	u32 vbe_me_vsize;
	u32 vbe_mv_hsize;
	u32 frm_mc_logo_hsize = 0;//TODO
	u32 frm_mc_mv_hsize = 0;
	u32 vbe_dmsq_hsize;//TODO, regs
	u32 grd_xnum_use;

	vbe_me_hsize_t0 = frm_hsize_t0 >> me_dsx;
	vbe_me_hsize_t1 = frm_hsize_t1 >> me_dsx;
	vbe_me_hsize_t2 = frm_hsize_t2 >> me_dsx;
	vbe_me_hsize = frm_hsize >> me_dsx;
	vbe_me_vsize = frm_vsize >> me_dsy;

	dbg_h2("[dpe.c]:me_dsx=%4d  me_dsy=%4d\n", me_dsx, me_dsy);
	dbg_h2("[dpe.c]:f_hsize=%4d vbe_me_hsize=%4d\n", frm_hsize, vbe_me_hsize);

	vbe_mv_hsize    = (vbe_me_hsize + 2) >> 2;

	vbe_dmsq_hsize  = dmsq_hsize;
	grd_xnum_use    = prm_dpe->prm_dpe_sub_rmif.grd_xnum_use;

	u32 vbe_me_slc0_hbgn;
	u32 vbe_me_slc0_hend;
	u32 vbe_me_slc1_hbgn;
	u32 vbe_me_slc1_hend;
	u32 vbe_me_slc2_hbgn;
	u32 vbe_me_slc2_hend;
	u32 vbe_me_slc3_hbgn;
	u32 vbe_me_slc3_hend;
	u32 vbe_me_slc0_vbgn;
	u32 vbe_me_slc0_vend;
	u32 vbe_me_slc1_vbgn;
	u32 vbe_me_slc1_vend;
	u32 vbe_me_slc2_vbgn;
	u32 vbe_me_slc2_vend;
	u32 vbe_me_slc3_vbgn;
	u32 vbe_me_slc3_vend;

	vbe_me_slc0_hbgn = 0;
	vbe_me_slc0_hend = slc_num == 4 ? vbe_me_hsize_t0 + vbe_me_pad - 1 :
	slc_num == 2 ? vbe_me_hsize_t1 + vbe_me_pad - 1 : vbe_me_hsize - 1;
	vbe_me_slc1_hbgn = slc_num == 4 ?
		vbe_me_hsize_t0 - vbe_me_pad : vbe_me_hsize_t1 - vbe_me_pad;
	vbe_me_slc1_hend = slc_num == 4 ?
		vbe_me_hsize_t1 + vbe_me_pad - 1 : vbe_me_hsize - 1;
	vbe_me_slc2_hbgn = vbe_me_hsize_t1 - vbe_me_pad;
	vbe_me_slc2_hend = vbe_me_hsize_t2 + vbe_me_pad - 1;
	vbe_me_slc3_hbgn = vbe_me_hsize_t2 - vbe_me_pad;
	vbe_me_slc3_hend = vbe_me_hsize - 1;
	vbe_me_slc0_vbgn = 0;
	vbe_me_slc0_vend = vbe_me_vsize - 1;
	vbe_me_slc1_vbgn = 0;
	vbe_me_slc1_vend = vbe_me_vsize - 1;
	vbe_me_slc2_vbgn = 0;
	vbe_me_slc2_vend = vbe_me_vsize - 1;
	vbe_me_slc3_vbgn = 0;
	vbe_me_slc3_vend = vbe_me_vsize - 1;

	dbg_h2("[dpss_dpe.c]:vbe_me_hsize_t0=%4d  vbe_me_pad=%4d\n",
		vbe_me_hsize_t0, vbe_me_pad);
	dbg_h2("[dpss_dpe.c]:vbe_me_hsize_t1=%4d  vbe_me_hsize_t2=%4d\n",
		vbe_me_hsize_t1, vbe_me_hsize_t2);

	vbe_mv_hbgnx4[0] = vbe_me_slc0_hbgn >> 2;
	vbe_mv_hbgnx4[1] = vbe_me_slc1_hbgn >> 2;
	vbe_mv_hbgnx4[2] = vbe_me_slc2_hbgn >> 2;
	vbe_mv_hbgnx4[3] = vbe_me_slc3_hbgn >> 2;
	vbe_mv_hendx4[0] = vbe_me_slc0_hend >> 2;
	vbe_mv_hendx4[1] = vbe_me_slc1_hend >> 2;
	vbe_mv_hendx4[2] = vbe_me_slc2_hend >> 2;
	vbe_mv_hendx4[3] = vbe_me_slc3_hend >> 2;
	vbe_mv_vbgnx4[0] = vbe_me_slc0_vbgn >> 2;
	vbe_mv_vbgnx4[1] = vbe_me_slc1_vbgn >> 2;
	vbe_mv_vbgnx4[2] = vbe_me_slc2_vbgn >> 2;
	vbe_mv_vbgnx4[3] = vbe_me_slc3_vbgn >> 2;
	vbe_mv_vendx4[0] = vbe_me_slc0_vend >> 2;
	vbe_mv_vendx4[1] = vbe_me_slc1_vend >> 2;
	vbe_mv_vendx4[2] = vbe_me_slc2_vend >> 2;
	vbe_mv_vendx4[3] = vbe_me_slc3_vend >> 2;

	vbe_me_hbgnx4[0] = vbe_me_slc0_hbgn;
	vbe_me_hbgnx4[1] = vbe_me_slc1_hbgn;
	vbe_me_hbgnx4[2] = vbe_me_slc2_hbgn;
	vbe_me_hbgnx4[3] = vbe_me_slc3_hbgn;
	vbe_me_hendx4[0] = vbe_me_slc0_hend;
	vbe_me_hendx4[1] = vbe_me_slc1_hend;
	vbe_me_hendx4[2] = vbe_me_slc2_hend;
	vbe_me_hendx4[3] = vbe_me_slc3_hend;
	vbe_me_vbgnx4[0] = vbe_me_slc0_vbgn;
	vbe_me_vbgnx4[1] = vbe_me_slc1_vbgn;
	vbe_me_vbgnx4[2] = vbe_me_slc2_vbgn;
	vbe_me_vbgnx4[3] = vbe_me_slc3_vbgn;
	vbe_me_vendx4[0] = vbe_me_slc0_vend;
	vbe_me_vendx4[1] = vbe_me_slc1_vend;
	vbe_me_vendx4[2] = vbe_me_slc2_vend;
	vbe_me_vendx4[3] = vbe_me_slc3_vend;
	dbg_h2("[dpss_dpe.c]:vbe_me_slc0_xend=%4d  vbe_me_slc0_yend=%4d\n",
		vbe_me_hendx4[0], vbe_me_vendx4[0]);

	mc_logo_hbgnx4[0] = 0;
	mc_logo_hendx4[0] = mc_lbuf_mode >= 1 ?
		reg_luma_slc0_xend >> 2 : reg_luma_slc0_xend >> 1;
	mc_logo_hbgnx4[1] = mc_lbuf_mode >= 1 ?
		reg_luma_slc1_xbgn >> 2 : reg_luma_slc1_xbgn >> 1;
	mc_logo_hendx4[1] = mc_lbuf_mode >= 1 ?
		reg_luma_slc1_xend >> 2 : reg_luma_slc1_xend >> 1;
	mc_logo_hbgnx4[2] = mc_lbuf_mode >= 1 ?
		reg_luma_slc2_xbgn >> 2 : reg_luma_slc2_xbgn >> 1;
	mc_logo_hendx4[2] = mc_lbuf_mode >= 1 ?
		reg_luma_slc2_xend >> 2 : reg_luma_slc2_xend >> 1;
	mc_logo_hbgnx4[3] = mc_lbuf_mode >= 1 ?
		reg_luma_slc3_xbgn >> 2 : reg_luma_slc3_xbgn >> 1;
	mc_logo_hendx4[3] = mc_lbuf_mode >= 1 ?
		reg_luma_slc3_xend >> 2 : reg_luma_slc3_xend >> 1;
	mc_logo_vbgnx4[0] = 0;
	mc_logo_vendx4[0] = mc_lbuf_mode  == 1 ?
		(frm_vsize - 1) >> 2 : (frm_vsize - 1) >> 1;
	mc_logo_vbgnx4[1] = 0;
	mc_logo_vendx4[1] = mc_lbuf_mode  == 1 ?
		(frm_vsize - 1) >> 2 : (frm_vsize - 1) >> 1;
	mc_logo_vbgnx4[2] = 0;
	mc_logo_vendx4[2] = mc_lbuf_mode  == 1 ?
		(frm_vsize - 1) >> 2 : (frm_vsize - 1) >> 1;
	mc_logo_vbgnx4[3] = 0;
	mc_logo_vendx4[3] = mc_lbuf_mode  == 1 ?
		(frm_vsize - 1) >> 2 : (frm_vsize - 1) >> 1;

	mc_mv_hbgnx4[0] = mc_lbuf_mode >= 1 ? mc_slc0_hbgn >> 4 : mc_slc0_hbgn >> 3;
	mc_mv_hendx4[0] = mc_lbuf_mode >= 1 ? mc_slc0_hend >> 4 : mc_slc0_hend >> 3;
	mc_mv_hbgnx4[1] = mc_lbuf_mode >= 1 ? mc_slc1_hbgn >> 4 : mc_slc1_hbgn >> 3;
	mc_mv_hendx4[1] = mc_lbuf_mode >= 1 ? mc_slc1_hend >> 4 : mc_slc1_hend >> 3;
	mc_mv_hbgnx4[2] = mc_lbuf_mode >= 1 ? mc_slc2_hbgn >> 4 : mc_slc2_hbgn >> 3;
	mc_mv_hendx4[2] = mc_lbuf_mode >= 1 ? mc_slc2_hend >> 4 : mc_slc2_hend >> 3;
	mc_mv_hbgnx4[3] = mc_lbuf_mode >= 1 ? mc_slc3_hbgn >> 4 : mc_slc3_hbgn >> 3;
	mc_mv_hendx4[3] = mc_lbuf_mode >= 1 ? mc_slc3_hend >> 4 : mc_slc3_hend >> 3;
	mc_mv_vbgnx4[0] = 0;
	mc_mv_vendx4[0] = mc_lbuf_mode == 1 ?
		(frm_vsize - 1) >> 4 : (frm_vsize - 1) >> 3;
	mc_mv_vbgnx4[1] = 0;
	mc_mv_vendx4[1] = mc_lbuf_mode == 1 ?
		(frm_vsize - 1) >> 4 : (frm_vsize - 1) >> 3;
	mc_mv_vbgnx4[2] = 0;
	mc_mv_vendx4[2] = mc_lbuf_mode == 1 ?
		(frm_vsize - 1) >> 4 : (frm_vsize - 1) >> 3;
	mc_mv_vbgnx4[3] = 0;
	mc_mv_vendx4[3] = mc_lbuf_mode == 1 ?
		(frm_vsize - 1) >> 4 : (frm_vsize - 1) >> 3;

	mc_melg_hbgnx4[0] = mc_logo_hbgnx4[0] >> 2;
	mc_melg_hbgnx4[1] = (mc_logo_hbgnx4[1] >> 2) - 1;
	mc_melg_hbgnx4[2] = (mc_logo_hbgnx4[2] >> 2) - 1;
	mc_melg_hbgnx4[3] = (mc_logo_hbgnx4[3] >> 2) - 1;
	mc_melg_hendx4[0] = slc_num == 1 ?
		mc_logo_hendx4[0] >> 2 : (mc_logo_hendx4[0] >> 2) + 1;
	mc_melg_hendx4[1] = slc_num == 2 ?
		mc_logo_hendx4[1] >> 2 : (mc_logo_hendx4[1] >> 2) + 1;
	mc_melg_hendx4[2] = (mc_logo_hendx4[1] >> 2) + 1;
	//mc_logo_hendx4[1] >> 2+1; //need_check
	mc_melg_hendx4[3] = mc_logo_hendx4[1] >> 2;
	mc_melg_vbgnx4[0] = 0;
	mc_melg_vendx4[0] = mc_logo_vendx4[0] >> 2;
	mc_melg_vbgnx4[1] = 0;
	mc_melg_vendx4[1] = mc_logo_vendx4[1] >> 2;
	mc_melg_vbgnx4[2] = 0;
	mc_melg_vendx4[2] = mc_logo_vendx4[2] >> 2;
	mc_melg_vbgnx4[3] = 0;
	mc_melg_vendx4[3] = mc_logo_vendx4[3] >> 2;

	u32  vbe_dmsq_hsize_t0;
	u32  vbe_dmsq_hsize_t1;
	u32  vbe_dmsq_hsize_t2;
	//u32  vbe_dmsq_hsize;
	u32  vbe_dmsq_vsize;

	vbe_dmsq_hsize_t0 = frm_hsize_t0 >> dmsq_xsft;
	vbe_dmsq_hsize_t1 = frm_hsize_t1 >> dmsq_xsft;
	vbe_dmsq_hsize_t2 = frm_hsize_t2 >> dmsq_xsft;
	//vbe_dmsq_hsize = frm_vsize >> dmsq_xsft;
	vbe_dmsq_vsize = frm_vsize >> 1;

	u32 vbe_dmsq_slc_hbgn[4];
	u32 vbe_dmsq_slc_hend[4];
	u32 vbe_dmsq_slc_vbgn[4];
	u32 vbe_dmsq_slc_vend[4];

	vbe_dmsq_slc_hbgn[0] = 0;
	vbe_dmsq_slc_hend[0] = slc_num == 4 ?
		vbe_dmsq_hsize_t0 + vbe_dmsq_pad - 1 : slc_num == 2 ?
			vbe_dmsq_hsize_t1 + vbe_dmsq_pad - 1 : vbe_dmsq_hsize - 1;
	vbe_dmsq_slc_hbgn[1] = slc_num == 4 ?
		vbe_dmsq_hsize_t0 - vbe_dmsq_pad : vbe_dmsq_hsize_t1 - vbe_dmsq_pad;
	vbe_dmsq_slc_hend[1] = slc_num == 4 ?
		vbe_dmsq_hsize_t1 + vbe_dmsq_pad - 1 : vbe_dmsq_hsize - 1;
	vbe_dmsq_slc_hbgn[2] = vbe_dmsq_hsize_t1 - vbe_dmsq_pad;
	vbe_dmsq_slc_hend[2] = vbe_dmsq_hsize_t2 + vbe_dmsq_pad - 1;
	vbe_dmsq_slc_hbgn[3] = vbe_dmsq_hsize_t2 - vbe_dmsq_pad;
	vbe_dmsq_slc_hend[3] = vbe_dmsq_hsize - 1;
	vbe_dmsq_slc_vbgn[0] = 0;
	vbe_dmsq_slc_vend[0] = vbe_dmsq_vsize - 1;
	vbe_dmsq_slc_vbgn[1] = 0;
	vbe_dmsq_slc_vend[1] = vbe_dmsq_vsize - 1;
	vbe_dmsq_slc_vbgn[2] = 0;
	vbe_dmsq_slc_vend[2] = vbe_dmsq_vsize - 1;
	vbe_dmsq_slc_vbgn[3] = 0;
	vbe_dmsq_slc_vend[3] = vbe_dmsq_vsize - 1;

	dbg_h2("[dpss_dpe.c]:vbe_mv_slc0_xend=%4d  vbe_mv_slc0_yend=%4d\n",
		vbe_mv_hendx4[0], vbe_mv_vendx4[0]);

	for (i = 0; i < 4; i++) {
		rdmif0_slc_hbgn[i] = mcnr_sel ? vbe_mv_hbgnx4[i] : mc_logo_hbgnx4[i];
		rdmif0_slc_hend[i] = mcnr_sel ? vbe_mv_hendx4[i] : mc_logo_hendx4[i];
		rdmif0_slc_vbgn[i] = mcnr_sel ? vbe_mv_vbgnx4[i] : mc_logo_vbgnx4[i];
		rdmif0_slc_vend[i] = mcnr_sel ? vbe_mv_vendx4[i] : mc_logo_vendx4[i];

		rdmif1_slc_hbgn[i] = mcnr_sel ? vbe_mv_hbgnx4[i] : mc_logo_hbgnx4[i];
		rdmif1_slc_hend[i] = mcnr_sel ? vbe_mv_hendx4[i] : mc_logo_hendx4[i];
		rdmif1_slc_vbgn[i] = mcnr_sel ? vbe_mv_vbgnx4[i] : mc_logo_vbgnx4[i];
		rdmif1_slc_vend[i] = mcnr_sel ? vbe_mv_vendx4[i] : mc_logo_vendx4[i];

		rdmif2_slc_hbgn[i] = mcnr_sel ? vbe_mv_hbgnx4[i] : mc_mv_hbgnx4[i];
		rdmif2_slc_hend[i] = mcnr_sel ? vbe_mv_hendx4[i] : mc_mv_hendx4[i];
		rdmif2_slc_vbgn[i] = mcnr_sel ? vbe_mv_vbgnx4[i] : mc_mv_vbgnx4[i];
		rdmif2_slc_vend[i] = mcnr_sel ? vbe_mv_vendx4[i] : mc_mv_vendx4[i];

		rdmif3_slc_hbgn[i] = mcnr_sel ? vbe_mv_hbgnx4[i] : mc_mv_hbgnx4[i];
		rdmif3_slc_hend[i] = mcnr_sel ? vbe_mv_hendx4[i] : mc_mv_hendx4[i];
		rdmif3_slc_vbgn[i] = mcnr_sel ? vbe_mv_vbgnx4[i] : mc_mv_vbgnx4[i];
		rdmif3_slc_vend[i] = mcnr_sel ? vbe_mv_vendx4[i] : mc_mv_vendx4[i];

		rdmif4_slc_hbgn[i] = mcnr_sel ? vbe_me_hbgnx4[i] : mc_melg_hbgnx4[i];
		rdmif4_slc_hend[i] = mcnr_sel ? vbe_me_hendx4[i] : mc_melg_hendx4[i];
		rdmif4_slc_vbgn[i] = mcnr_sel ? vbe_me_vbgnx4[i] : mc_melg_vbgnx4[i];
		rdmif4_slc_vend[i] = mcnr_sel ? vbe_me_vendx4[i] : mc_melg_vendx4[i];

		rdmif5_slc_hbgn[i] = mcnr_sel ? vbe_me_hbgnx4[i] : mc_melg_hbgnx4[i];
		rdmif5_slc_hend[i] = mcnr_sel ? vbe_me_hendx4[i] : mc_melg_hendx4[i];
		rdmif5_slc_vbgn[i] = mcnr_sel ? vbe_me_vbgnx4[i] : mc_melg_vbgnx4[i];
		rdmif5_slc_vend[i] = mcnr_sel ? vbe_me_vendx4[i] : mc_melg_vendx4[i];

		rdmif6_slc_hbgn[i] = vbe_dmsq_slc_hbgn[i];
		rdmif6_slc_hend[i] = vbe_dmsq_slc_hend[i];
		rdmif6_slc_vbgn[i] = vbe_dmsq_slc_vbgn[i];
		rdmif6_slc_vend[i] = vbe_dmsq_slc_vend[i];
	}

	rdmif0_hsize = mcnr_sel ? vbe_mv_hsize : frm_mc_logo_hsize;
	rdmif1_hsize = mcnr_sel ? vbe_mv_hsize : frm_mc_logo_hsize;
	rdmif2_hsize = mcnr_sel ? vbe_mv_hsize : frm_mc_mv_hsize;
	rdmif3_hsize = mcnr_sel ? vbe_mv_hsize : frm_mc_mv_hsize;
	rdmif4_hsize = mcnr_sel ? vbe_me_hsize : frm_mc_mv_hsize;
	rdmif5_hsize = mcnr_sel ? vbe_me_hsize : frm_mc_mv_hsize;
	rdmif6_hsize = vbe_dmsq_hsize;
	//ary set no use rdmif7_hsize = vbe_me_hsize;
	u32 rdmif_baddr[11];
	u32 wrmif_baddr[11];

	for (i = 0; i < 11; i++)
		rdmif_baddr[i] = prm_dpe->prm_dpe_sub_rmif.sub_rdmif_baddr[i];
	for (i = 0; i < 4; i++)
		wrmif_baddr[i] = prm_dpe->prm_dpe_sub_rmif.sub_wrmif_baddr[i];

	//void dpss_dpe_rdmif_cfg(u32 hsize, u32 baddr, u32 mode, u32 idx, u32 reg_grd,
	//u32 xbgn_slc0, u32 xend_slc0, u32 ybgn_slc0, u32 yend_slc0,
	//u32 xbgn_slc1, u32 xend_slc1, u32 ybgn_slc1, u32 yend_slc1,
	//u32 xbgn_slc2, u32 xend_slc2, u32 ybgn_slc2, u32 yend_slc2,
	//u32 xbgn_slc3, u32 xend_slc3, u32 ybgn_slc3, u32 yend_slc3)

	dpss_dpe_rdmif_cfg(rdmif0_hsize, rdmif_baddr[0], mcnr_sel, 0, grd_xnum_use,
	rdmif0_slc_hbgn[0], rdmif0_slc_hend[0], rdmif0_slc_vbgn[0], rdmif0_slc_vend[0],
	rdmif0_slc_hbgn[1], rdmif0_slc_hend[1], rdmif0_slc_vbgn[1], rdmif0_slc_vend[1],
	rdmif0_slc_hbgn[2], rdmif0_slc_hend[2], rdmif0_slc_vbgn[2], rdmif0_slc_vend[2],
	rdmif0_slc_hbgn[3], rdmif0_slc_hend[3], rdmif0_slc_vbgn[3], rdmif0_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif1_hsize, rdmif_baddr[1], mcnr_sel, 1, grd_xnum_use,
	rdmif1_slc_hbgn[0], rdmif1_slc_hend[0], rdmif1_slc_vbgn[0], rdmif1_slc_vend[0],
	rdmif1_slc_hbgn[1], rdmif1_slc_hend[1], rdmif1_slc_vbgn[1], rdmif1_slc_vend[1],
	rdmif1_slc_hbgn[2], rdmif1_slc_hend[2], rdmif1_slc_vbgn[2], rdmif1_slc_vend[2],
	rdmif1_slc_hbgn[3], rdmif1_slc_hend[3], rdmif1_slc_vbgn[3], rdmif1_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif2_hsize, rdmif_baddr[2], mcnr_sel, 2, grd_xnum_use,
	rdmif2_slc_hbgn[0], rdmif2_slc_hend[0], rdmif2_slc_vbgn[0], rdmif2_slc_vend[0],
	rdmif2_slc_hbgn[1], rdmif2_slc_hend[1], rdmif2_slc_vbgn[1], rdmif2_slc_vend[1],
	rdmif2_slc_hbgn[2], rdmif2_slc_hend[2], rdmif2_slc_vbgn[2], rdmif2_slc_vend[2],
	rdmif2_slc_hbgn[3], rdmif2_slc_hend[3], rdmif2_slc_vbgn[3], rdmif2_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif3_hsize, rdmif_baddr[3], mcnr_sel, 3, grd_xnum_use,
	rdmif3_slc_hbgn[0], rdmif3_slc_hend[0], rdmif3_slc_vbgn[0], rdmif3_slc_vend[0],
	rdmif3_slc_hbgn[1], rdmif3_slc_hend[1], rdmif3_slc_vbgn[1], rdmif3_slc_vend[1],
	rdmif3_slc_hbgn[2], rdmif3_slc_hend[2], rdmif3_slc_vbgn[2], rdmif3_slc_vend[2],
	rdmif3_slc_hbgn[3], rdmif3_slc_hend[3], rdmif3_slc_vbgn[3], rdmif3_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif4_hsize, rdmif_baddr[4], mcnr_sel, 4, grd_xnum_use,
	rdmif4_slc_hbgn[0], rdmif4_slc_hend[0], rdmif4_slc_vbgn[0], rdmif4_slc_vend[0],
	rdmif4_slc_hbgn[1], rdmif4_slc_hend[1], rdmif4_slc_vbgn[1], rdmif4_slc_vend[1],
	rdmif4_slc_hbgn[2], rdmif4_slc_hend[2], rdmif4_slc_vbgn[2], rdmif4_slc_vend[2],
	rdmif4_slc_hbgn[3], rdmif4_slc_hend[3], rdmif4_slc_vbgn[3], rdmif4_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif5_hsize, rdmif_baddr[5], mcnr_sel, 5, grd_xnum_use,
	rdmif5_slc_hbgn[0], rdmif5_slc_hend[0], rdmif5_slc_vbgn[0], rdmif5_slc_vend[0],
	rdmif5_slc_hbgn[1], rdmif5_slc_hend[1], rdmif5_slc_vbgn[1], rdmif5_slc_vend[1],
	rdmif5_slc_hbgn[2], rdmif5_slc_hend[2], rdmif5_slc_vbgn[2], rdmif5_slc_vend[2],
	rdmif5_slc_hbgn[3], rdmif5_slc_hend[3], rdmif5_slc_vbgn[3], rdmif5_slc_vend[3]);
	dpss_dpe_rdmif_cfg(rdmif6_hsize, rdmif_baddr[6], mcnr_sel, 6, grd_xnum_use,
	rdmif6_slc_hbgn[0], rdmif6_slc_hend[0], rdmif6_slc_vbgn[0], rdmif6_slc_vend[0],
	rdmif6_slc_hbgn[1], rdmif6_slc_hend[1], rdmif6_slc_vbgn[1], rdmif6_slc_vend[1],
	rdmif6_slc_hbgn[2], rdmif6_slc_hend[2], rdmif6_slc_vbgn[2], rdmif6_slc_vend[2],
	rdmif6_slc_hbgn[3], rdmif6_slc_hend[3], rdmif6_slc_vbgn[3], rdmif6_slc_vend[3]);

	u32 wrmif0_slc_hbgn[4];
	u32 wrmif0_slc_hend[4];
	u32 wrmif0_slc_vbgn[4];
	u32 wrmif0_slc_vend[4];
	u32 wrmif1_slc_hbgn[4] = {0, 1, 2, 3};
	u32 wrmif1_slc_hend[4] = {0, 1, 2, 3};
	u32 wrmif1_slc_vbgn[4] = {0, 0, 0, 0};
	u32 wrmif1_slc_vend[4] = {
		frm_vsize - 1, frm_vsize - 1, frm_vsize - 1, frm_vsize - 1};
	u32 wrmif2_slc_hbgn[4];
	u32 wrmif2_slc_hend[4];
	u32 wrmif2_slc_vbgn[4];
	u32 wrmif2_slc_vend[4];

	u32 vbe_dmsq_tslc_hbgn[4];
	u32 vbe_dmsq_tslc_hend[4];
	u32 vbe_dmsq_tslc_vbgn[4];
	u32 vbe_dmsq_tslc_vend[4];

	u32 dout_slc_hbgn[4];
	u32 dout_slc_hend[4];
	u32 dout_slc_vbgn[4];
	u32 dout_slc_vend[4];

	vbe_dmsq_tslc_hbgn[0] = 0;
	vbe_dmsq_tslc_hend[0] = slc_num == 4 ? vbe_dmsq_hsize_t0 - 1 :
				slc_num == 2 ? vbe_dmsq_hsize_t1 - 1 : vbe_dmsq_hsize - 1;
	vbe_dmsq_tslc_hbgn[1] = slc_num == 4 ? vbe_dmsq_hsize_t0 : vbe_dmsq_hsize_t1;
	vbe_dmsq_tslc_hend[1] = slc_num == 4 ? vbe_dmsq_hsize_t1 - 1 : vbe_dmsq_hsize - 1;
	vbe_dmsq_tslc_hbgn[2] = vbe_dmsq_hsize_t1;
	vbe_dmsq_tslc_hend[2] = vbe_dmsq_hsize_t2 - 1;
	vbe_dmsq_tslc_hbgn[3] = vbe_dmsq_hsize_t2;
	vbe_dmsq_tslc_hend[3] = vbe_dmsq_hsize - 1;
	vbe_dmsq_tslc_vbgn[0] = 0;
	vbe_dmsq_tslc_vend[0] = vbe_dmsq_vsize - 1;
	vbe_dmsq_tslc_vbgn[1] = 0;
	vbe_dmsq_tslc_vend[1] = vbe_dmsq_vsize - 1;
	vbe_dmsq_tslc_vbgn[2] = 0;
	vbe_dmsq_tslc_vend[2] = vbe_dmsq_vsize - 1;
	vbe_dmsq_tslc_vbgn[3] = 0;
	vbe_dmsq_tslc_vend[3] = vbe_dmsq_vsize - 1;

	dout_slc_hbgn[0] = 0;
	dout_slc_hend[0] = slc_num == 4 ? frm_hsize_t0 - 1 :
			slc_num == 2 ? frm_hsize_t1 - 1 : frm_hsize - 1;
	dout_slc_hbgn[1] = slc_num == 4 ? frm_hsize_t0 : frm_hsize_t1;
	dout_slc_hend[1] = slc_num == 4 ? frm_hsize_t1 - 1 : frm_hsize - 1;
	dout_slc_hbgn[2] = frm_hsize_t1;
	dout_slc_hend[2] = frm_hsize_t2 - 1;
	dout_slc_hbgn[3] = frm_hsize_t2;
	dout_slc_hend[3] = frm_hsize - 1;
	dout_slc_vbgn[0] = 0;
	dout_slc_vend[0] = frm_vsize - 1;
	dout_slc_vbgn[1] = 0;
	dout_slc_vend[1] = frm_vsize - 1;
	dout_slc_vbgn[2] = 0;
	dout_slc_vend[2] = frm_vsize - 1;
	dout_slc_vbgn[3] = 0;
	dout_slc_vend[3] = frm_vsize - 1;

	dbg_h2("[dpss_dpe.c]:dout_slc_xend=%4d  dout_frm_hsize=%4d\n", dout_slc_hend[0], frm_hsize);

	for (i = 0; i < 4; i++) {
		wrmif0_slc_hbgn[i] = vbe_dmsq_tslc_hbgn[i];
		wrmif0_slc_hend[i] = vbe_dmsq_tslc_hend[i];
		wrmif0_slc_vbgn[i] = vbe_dmsq_tslc_vbgn[i];
		wrmif0_slc_vend[i] = vbe_dmsq_tslc_vend[i];

		wrmif2_slc_hbgn[i] = dout_slc_hbgn[i];
		wrmif2_slc_hend[i] = dout_slc_hend[i];
//		wrmif2_slc_vbgn[i] = dout_slc_vbgn[i];
//		wrmif2_slc_vend[i] = dout_slc_vend[i];
		wrmif2_slc_vbgn[i] = 0;
		wrmif2_slc_vend[i] = 0;
	}

	dbg_h2("[dpss_dpe.c]:vbe_dmsq_tslc0_hend=%4d  vbe_dmsq_hsize_t0=%4d\n",
		vbe_dmsq_tslc_hend[0], vbe_dmsq_hsize_t0);

	u32 wrmif0_hsize = vbe_dmsq_hsize;
	u32 wrmif1_hsize = 2;
	u32 wrmif2_hsize = frm_hsize;

	//void dpss_dpe_wrmif_cfg(u32 hsize, u32 baddr,u32 idx,
	// u32 xbgn_slc0, u32 xend_slc0, u32 ybgn_slc0, u32 yend_slc0,
	// u32 xbgn_slc1, u32 xend_slc1, u32 ybgn_slc1, u32 yend_slc1,
	// u32 xbgn_slc2, u32 xend_slc2, u32 ybgn_slc2, u32 yend_slc2,
	// u32 xbgn_slc3, u32 xend_slc3, u32 ybgn_slc3, u32 yend_slc3)
	dpss_dpe_wrmif_cfg(wrmif0_hsize, wrmif_baddr[0], 0,
		wrmif0_slc_hbgn[0], wrmif0_slc_hend[0],
		wrmif0_slc_vbgn[0], wrmif0_slc_vend[0],
		wrmif0_slc_hbgn[1], wrmif0_slc_hend[1],
		wrmif0_slc_vbgn[1], wrmif0_slc_vend[1],
		wrmif0_slc_hbgn[2], wrmif0_slc_hend[2],
		wrmif0_slc_vbgn[2], wrmif0_slc_vend[2],
		wrmif0_slc_hbgn[3], wrmif0_slc_hend[3],
		wrmif0_slc_vbgn[3], wrmif0_slc_vend[3]);
	dpss_dpe_wrmif_cfg(wrmif1_hsize, wrmif_baddr[1], 1,
		wrmif1_slc_hbgn[0], wrmif1_slc_hend[0],
		wrmif1_slc_vbgn[0], wrmif1_slc_vend[0],
		wrmif1_slc_hbgn[1], wrmif1_slc_hend[1],
		wrmif1_slc_vbgn[1], wrmif1_slc_vend[1],
		wrmif1_slc_hbgn[2], wrmif1_slc_hend[2],
		wrmif1_slc_vbgn[2], wrmif1_slc_vend[2],
		wrmif1_slc_hbgn[3], wrmif1_slc_hend[3],
		wrmif1_slc_vbgn[3], wrmif1_slc_vend[3]);
	dpss_dpe_wrmif_cfg(wrmif2_hsize, wrmif_baddr[2], 2,
		wrmif2_slc_hbgn[0], wrmif2_slc_hend[0],
		wrmif2_slc_vbgn[0], wrmif2_slc_vend[0],
		wrmif2_slc_hbgn[1], wrmif2_slc_hend[1],
		wrmif2_slc_vbgn[1], wrmif2_slc_vend[1],
		wrmif2_slc_hbgn[2], wrmif2_slc_hend[2],
		wrmif2_slc_vbgn[2], wrmif2_slc_vend[2],
		wrmif2_slc_hbgn[3], wrmif2_slc_hend[3],
		wrmif2_slc_vbgn[3], wrmif2_slc_vend[3]);
}

void cfg_dpe_hscale(u32 hsc_en, u32 hsize_in, u32 hsize_out,
		u32 hsc_out_lft_pad, u32 hsc_out_rgt_pad)
{
	int init_integer[4];
	int init_frac[4];
	int reg_init_inte[4], reg_init_phase[4], init_phase[4];
	int hsc_in_xbgn[4];
	int hsc_in_xend[4];
	int hsc_out_xbgn[4];
	int hsc_out_xend[4];
	int hsc_step;
//ary no use    int hsc_slc_in = hsize_in / 4;
	int hsc_slc_out = hsize_out / 4;
	int hsc_step_int = hsize_in / hsize_out;
#ifdef RUN_ON_PC
	int hsc_step_frc = ((u64)hsize_in << 24) / hsize_out - (hsc_step_int << 24);
#else
	int hsc_step_frc = div_u64((u64)hsize_in << 24, hsize_out) -
				(hsc_step_int << 24);
#endif
	hsc_step = (hsc_step_int << 24) + hsc_step_frc;
	//u32 hsc_in_pad = hsc_out_pad * hsc_step >> 24;

#ifndef DPSS_PRINT_CLOSE
	dbg_h2("hsize_in=%d\n", hsize_in);
	dbg_h2("hsize_out=%d\n", hsize_out);
	dbg_h2("hsc_out_lft_pad=%d\n", hsc_out_lft_pad);
	dbg_h2("hsc_out_rgt_pad=%d\n", hsc_out_rgt_pad);
#endif

	//hsc_in_xbgn[0] = 0;
	//hsc_in_xbgn[1] = 1*hsc_slc_in - hsc_in_pad - 4;
	//hsc_in_xbgn[2] = 2*hsc_slc_in - hsc_in_pad - 4;
	//hsc_in_xbgn[3] = 3*hsc_slc_in - hsc_in_pad - 4;

	//hsc_in_xend[0] = 1*hsc_slc_in - 1 + hsc_in_pad + 4;
	//hsc_in_xend[1] = 2*hsc_slc_in - 1 + hsc_in_pad + 4;
	//hsc_in_xend[2] = 3*hsc_slc_in - 1 + hsc_in_pad + 4;
	//hsc_in_xend[3] = hsize_in - 1;

	//hsc_out_xbgn[0] = 0;
	//hsc_out_xbgn[1] = (((u64)hsc_in_xbgn[1] << 24) / hsc_step) + 1;
	//hsc_out_xbgn[2] = (((u64)hsc_in_xbgn[2] << 24) / hsc_step) + 1;
	//hsc_out_xbgn[3] = (((u64)hsc_in_xbgn[3] << 24) / hsc_step) + 1;
	//hsc_out_xend[0] = ((((u64)hsc_in_xend[0]+1) << 24) / hsc_step);
	//hsc_out_xend[1] = ((((u64)hsc_in_xend[1]+1) << 24) / hsc_step);
	//hsc_out_xend[2] = ((((u64)hsc_in_xend[2]+1) << 24) / hsc_step);
	//hsc_out_xend[3] = hsize_out - 1;

	hsc_out_xbgn[0] = 0;
	hsc_out_xend[0] = 1 * hsc_slc_out + hsc_out_rgt_pad;

	hsc_out_xbgn[1] = 1 * hsc_slc_out - hsc_out_lft_pad;
	hsc_out_xend[1] = 2 * hsc_slc_out + hsc_out_rgt_pad;

	hsc_out_xbgn[2] = 2 * hsc_slc_out - hsc_out_lft_pad;
	hsc_out_xend[2] = 3 * hsc_slc_out + hsc_out_rgt_pad;

	hsc_out_xbgn[3] = 3 * hsc_slc_out - hsc_out_lft_pad;
	hsc_out_xend[3] = hsize_out;

	int slc_num = 4;
	int i, bord_bgn, bord_end;

	for (i = 0; i < slc_num; i++) {//TODO need change parameter
		bord_bgn = (i == 0) ? 1 : 0;
		bord_end = (i == slc_num - 1) ? 1 : 0;
#ifndef DPSS_PRINT_CLOSE
	dbg_h2("-------slc %0d--------\n", i);
#endif
		set_aa_pps_hor_slice(&hsc_in_xbgn[i], &hsc_in_xend[i],
			&reg_init_inte[i], &reg_init_phase[i],
			bord_bgn, bord_end, hsc_out_xbgn[i],
			hsc_out_xend[i], hsize_in, 0, 0, hsc_step);
	}
#ifndef DPSS_PRINT_CLOSE
	dbg_h2("%s end %d\n", __func__, slc_num);
#endif
	init_integer[0] = reg_init_inte[0];
	init_integer[1] = reg_init_inte[1];
	init_integer[2] = reg_init_inte[2];
	init_integer[3] = reg_init_inte[3];

	init_frac[0] = reg_init_phase[0];
	init_frac[1] = reg_init_phase[1];
	init_frac[2] = reg_init_phase[2];
	init_frac[3] = reg_init_phase[3];

	init_phase[0] = init_integer[0] << 24 | init_frac[0];
	init_phase[1] = init_integer[1] << 24 | init_frac[1];
	init_phase[2] = init_integer[2] << 24 | init_frac[2];
	init_phase[3] = init_integer[3] << 24 | init_frac[3];

	//1024->960, 4 slice,
	w_reg_bit(DPSS_DPE_MIF_CTRL0, 1, 24, 1);
	//reg_mif_win_mode,1-select reg config;0-use port config
	//frame , ini_integer=0, ini_phase=0, slc=0
	w_reg_bit(DPSS_DPE_HSC_INI_PHS0, init_phase[0], 0, 29);//interge 4, phase 24
	w_reg_bit(DPSS_DPE_HSC_INI_PHS1, init_phase[1], 0, 29);
	w_reg_bit(DPSS_DPE_HSC_INI_PHS2, init_phase[2], 0, 29);
	w_reg_bit(DPSS_DPE_HSC_INI_PHS3, init_phase[3], 0, 29);
	//rdmif hslice size
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, hsc_in_xbgn[0], 0, 13);//xbgn
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC0, hsc_in_xend[0] - 1, 16, 13);//xend
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC1, hsc_in_xbgn[1], 0, 13);//xbgn
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC1, hsc_in_xend[1] - 1, 16, 13);//xend
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC2, hsc_in_xbgn[2], 0, 13);//xbgn
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC2, hsc_in_xend[2] - 1, 16, 13);//xend
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC3, hsc_in_xbgn[3], 0, 13);//xbgn
	w_reg_bit(DPSS_DPE_VFCD_MIF0_X_SLC3, hsc_in_xend[3] - 1, 16, 13);//xend

	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC0, hsc_out_xbgn[0], 0, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC0, hsc_out_xend[0] - 1, 16, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC1, hsc_out_xbgn[1], 0, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC1, hsc_out_xend[1] - 1, 16, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC2, hsc_out_xbgn[2], 0, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC2, hsc_out_xend[2] - 1, 16, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC3, hsc_out_xbgn[3], 0, 13);
	w_reg_bit(DPSS_DPE_HSC_OUT_XSLC3, hsc_out_xend[3] - 1, 16, 13);

	w_reg_bit(DPSS_DPE_HSC_CTRL, 8, 4, 4);//hsc_tap_num
	w_reg_bit(DPSS_DPE_HSC_CTRL, 9, 8, 4);//hsc_flt_sft
	w_reg_bit(DPSS_DPE_HSC_STEP, hsc_step & 0xfffffff, 0, 28);
	//hsc_phase_step, cal
	w_reg_bit(DPSS_DPE_SCALE_COEF_IDX, 0xa00, 0, 13);

	for (i = 0; i < 33; i++) {
		wr(DPSS_DPE_SCALE_COEF,
			(((dpe_pps_lut_tap8[i][0] >> 8) & 0xff) << 24) |
			((dpe_pps_lut_tap8[i][0] & 0xff) << 16) |
			(((dpe_pps_lut_tap8[i][1] >> 8) & 0xff) << 8) |
			((dpe_pps_lut_tap8[i][1] & 0xff) << 0));

		wr(DPSS_DPE_SCALE_COEF,
			(((dpe_pps_lut_tap8[i][2] >> 8) & 0xff) << 24) |
			((dpe_pps_lut_tap8[i][2] & 0xff) << 16) |
			(((dpe_pps_lut_tap8[i][3] >> 8) & 0xff) << 8) |
			((dpe_pps_lut_tap8[i][3] & 0xff) << 0));
	}
	w_reg_bit(DPSS_DPE_SCALE_COEF_IDX, 0xe00, 0, 13);

	for (i = 0; i < 33; i++) {
		wr(DPSS_DPE_SCALE_COEF,
			(((dpe_pps_lut_tap8[i][4] >> 8) & 0xff) << 24) |
			((dpe_pps_lut_tap8[i][4] & 0xff) << 16) |
			(((dpe_pps_lut_tap8[i][5] >> 8) & 0xff) << 8) |
			((dpe_pps_lut_tap8[i][5] & 0xff) << 0));
		wr(DPSS_DPE_SCALE_COEF,
			(((dpe_pps_lut_tap8[i][6] >> 8) & 0xff) << 24) |
			((dpe_pps_lut_tap8[i][6] & 0xff) << 16) |
			(((dpe_pps_lut_tap8[i][7] >> 8) & 0xff) << 8) |
			((dpe_pps_lut_tap8[i][7] & 0xff) << 0));
	}
	w_reg_bit(DPSS_DPE_HSC_CTRL, hsc_en, 0, 1);//hsc_en
}

static void cfg_dpe_secure(unsigned int dpe_mode,
		struct PRM_DPSS_TOP  *prm_top,
		struct PRM_DPSS_DPE  *prm_dpe)
{
	unsigned int set = 0;

	if (!prm_top->is_secure) {
		if (dpe_mode == DPE_NR_MODE ||
		    dpe_mode == DPE_NR_BYPS ||
		    dpe_mode == DPE_NR_SRC1_MODE ||
		    dpe_mode == DPE_DV_SRC0_MODE) {
			w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 0, 0, 2);
			if (!prm_top->src_mode)
				w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 0, 4, 2);
			set = 1;
		} else if (dpe_mode == DPE_VDI_MODE ||
		    dpe_mode == DPE_NRDI_MODE) {
			w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 0, 1, 3);
			if (!prm_top->src_mode)
				w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 0, 4, 2);
			set = 2;
		} else {
			//frc: to-do
			set = 3;
		}
	} else {
		if (dpe_mode == DPE_NR_MODE ||
		    dpe_mode == DPE_NR_BYPS ||
		    dpe_mode == DPE_NR_SRC1_MODE ||
		    dpe_mode == DPE_DV_SRC0_MODE) {
			w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 3, 0, 2);
			if (!prm_top->src_mode)
				w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 3, 4, 2);

			set = 11;
		} else if (dpe_mode == DPE_VDI_MODE ||
		    dpe_mode == DPE_NRDI_MODE) {
			w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 7, 1, 3);
			if (!prm_top->src_mode)
				w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 3, 4, 2);
			set = 12;
		} else {
			//frc: to-do
			set = 13;
		}
	}
	dbg_h2("%s:frc %d\n", __func__, set);
}

