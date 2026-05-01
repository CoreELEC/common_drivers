// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#include "register_t6w.h"
//#include "register_t6w_vc.h"

#include "sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/amlogic/media/vpu/vpu.h>
/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#endif				//RUN_ON_ARM
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_hw.h"
#include "dpss_func.h"
#include "dpss_s_frc.h"
#include "dpss_rdma.h"

#include "hw/dpss.h"
#include "hw/dpss_phslut.h"
#include "hw/dpss_mc.h"
#include "hw/fgrain.h"
#include "drv/d_dd.h"
#include "drv/d_hdr.h"
#include "drv/d_pq.h"
#include <linux/amlogic/media/di/di_pq.h>
#include <linux/amlogic/media/vout/vout_notify.h>

//unsigned int g_dpss_tst_case;
unsigned int dpss_dbg_tst_case;	//force g_dpss_tst_case
module_param_named(dpss_dbg_tst_case, dpss_dbg_tst_case, uint, 0664);

unsigned int dpss_force_bypss;	//force g_dpss_tst_case
module_param_named(dpss_force_bypss, dpss_force_bypss, uint, 0664);

unsigned int dpss_dbg_in_fmt;
module_param_named(dpss_dbg_in_fmt, dpss_dbg_in_fmt, uint, 0664);

unsigned int dpss_dbg_top_cfg0;
module_param_named(dpss_dbg_top_cfg0, dpss_dbg_top_cfg0, uint, 0664);
unsigned int dpss_dbg_top_cfg_di;
module_param_named(dpss_dbg_top_cfg_di, dpss_dbg_top_cfg_di, uint, 0664);

unsigned int dpss_dbg_o_fmt;
module_param_named(dpss_dbg_o_fmt, dpss_dbg_o_fmt, uint, 0664);

unsigned int dpss_dbg_dae_dpe_cfg = 4;	//
module_param_named(dpss_dbg_dae_dpe_cfg, dpss_dbg_dae_dpe_cfg, uint, 0664);

bool dpss_en_hdr = 1;		//bit 0 for dae
module_param_named(dpss_en_hdr, dpss_en_hdr, bool, 0664);

bool dpss_p_as_i;		//
module_param_named(dpss_p_as_i, dpss_p_as_i, bool, 0664);

bool dpss_slt_mode;
module_param_named(dpss_slt_mode, dpss_slt_mode, bool, 0664);

bool fg_enable = 1;
module_param_named(fg_enable, fg_enable, bool, 0664);

uint fg_path;
module_param_named(fg_path, fg_path, uint, 0664);

unsigned int dpss_t6x_direct;
module_param_named(dpss_t6x_direct, dpss_t6x_direct, uint, 0664);

bool dpss_dbg_dis_i_srcreg;	//
module_param_named(dpss_dbg_dis_i_srcreg, dpss_dbg_dis_i_srcreg, bool, 0664);
bool dpss_dbg_en_logo;	//
module_param_named(dpss_dbg_en_logo, dpss_dbg_en_logo, bool, 0664);

unsigned int dpss_dv_en;	//bit 0 for dae
module_param_named(dpss_dv_en, dpss_dv_en, uint, 0664);

unsigned int dpss_en_dct = 1;
module_param_named(dpss_en_dct, dpss_en_dct, uint, 0664);
unsigned int dpss_en_afbc;
module_param_named(dpss_en_afbc, dpss_en_afbc, uint, 0664);
unsigned int dpss_en_afbc_force;
//bit 0 ~ 7 for src0// bit 8 ~ 15 for src 1
//bit 0: 1: force afbce;
//bit 1: 1: force afbrc;
//bit 2: 1: force mif;

module_param_named(dpss_en_afbc_force, dpss_en_afbc_force, uint, 0664);

unsigned int dpss_en_rdma;
module_param_named(dpss_en_rdma, dpss_en_rdma, uint, 0664);

bool dpss_en_filed;		//bit 0 for dae
module_param_named(dpss_en_filed, dpss_en_filed, bool, 0664);
bool dpss_force_b;
module_param_named(dpss_force_b, dpss_force_b, bool, 0664);

unsigned int dpss_force_in_h;
module_param_named(dpss_force_in_h, dpss_force_in_h, uint, 0664);

unsigned int dpss_force_in_v;
module_param_named(dpss_force_in_v, dpss_force_in_v, uint, 0664);

unsigned int dpss_dbg_game_mode;
module_param_named(dpss_dbg_game_mode, dpss_dbg_game_mode, uint, 0664);

unsigned int dpss_dbg_sw_mode;
module_param_named(dpss_dbg_sw_mode, dpss_dbg_sw_mode, uint, 0664);

bool dpss_dbg_dos_mode;		// = true; //bit 0 for dae
module_param_named(dpss_dbg_dos_mode, dpss_dbg_dos_mode, bool, 0664);

unsigned int dpss_en_lcevc;
module_param_named(dpss_en_lcevc, dpss_en_lcevc, uint, 0664);

unsigned int dpss_o_42210_2p;
module_param_named(dpss_o_42210_2p, dpss_o_42210_2p, uint, 0664);

unsigned int nr_bypass_test;
module_param_named(nr_bypass_test, nr_bypass_test, uint, 0664);

unsigned int auto_alig_en = 1;// = true; //bit 0 for dae
module_param_named(auto_alig_en, auto_alig_en, uint, 0664);

unsigned int auto_alig_en_i = 1;
module_param_named(auto_alig_en_i, auto_alig_en_i, uint, 0664);

unsigned int dpss_o_mode = 3;
module_param_named(dpss_o_mode, dpss_o_mode, uint, 0664);

bool nr_4k1k_en;
module_param_named(nr_4k1k_en, nr_4k1k_en, bool, 0664);

unsigned int nr_dae_xy;
module_param_named(nr_dae_xy, nr_dae_xy, uint, 0664);

unsigned int dpss_dbg_step;
module_param_named(dpss_dbg_step, dpss_dbg_step, uint, 0664);

unsigned int dpss_dbg_ds;
module_param_named(dpss_dbg_ds, dpss_dbg_ds, uint, 0664);

unsigned int dpss_pps_check;
module_param_named(dpss_pps_check, dpss_pps_check, uint, 0664);

unsigned int test_slice;
module_param_named(test_slice, test_slice, uint, 0664);

unsigned int pps_out_w;//1920;
module_param_named(pps_out_w, pps_out_w, uint, 0664);

unsigned int pps_out_h;//1080;
module_param_named(pps_out_h, pps_out_h, uint, 0664);

unsigned int afrc_dict_mode_y = 1;
module_param_named(afrc_dict_mode_y, afrc_dict_mode_y, uint, 0664);

unsigned int afrc_dict_mode_c = 1;
module_param_named(afrc_dict_mode_c, afrc_dict_mode_c, uint, 0664);

unsigned int dpss_dct_force = 0xff;
module_param_named(dpss_dct_force, dpss_dct_force, uint, 0664);

//unsigned int g_dpss_in_cnt;
//unsigned int g_dpss_out_cnt;
//unsigned int g_dpss_dbg_vfm_in;

struct dpss_nr_i_s *dpss_nr1;
unsigned char *dpss_idx;

unsigned int dpss_frclink_en = 1;
unsigned int dpss_frc_bypass;

//#include "drv/d_dd.c"
//#include "drv/d_hdr.c"
//#include "drv/d_pq.c"

static void dpss_h_val_init(struct dpss_ch_s *pch)
{
	if (!pch) {
		DBG_WARN("%s:no input:\n", __func__);
		return;
	}
	memset(&pch->c.prm_top, 0, sizeof(pch->c.prm_top));
	memset(&pch->c.prm_dae, 0, sizeof(pch->c.prm_dae));
	memset(&pch->c.prm_dpe, 0, sizeof(pch->c.prm_dpe));

	memset(&pch->c.dae_yuv_intf, 0, sizeof(pch->c.dae_yuv_intf));
	memset(&pch->c.prm_size, 0, sizeof(pch->c.prm_size));

	pch->c.prm_dae.ext_yuv_intf = &pch->c.dae_yuv_intf;
	pch->c.prm_dae.prm_size_ext = &pch->c.prm_size;
}

void dpss_hw_disable_one_play(struct dpss_ch_s *pch)
{
	dbg_i0("%s:ch[%d]:start:\n", "disable one play", pch->c.ch);
	if (pch->c.ch == 0) {
		if (!(dpss_get_hw()->src_act & C_BIT0))
			return;
		dpss_dd_disable();
		w_reg_bit(VPU_VBE_TOP_HDR_CTRL, 3, 0, 2);
		dpss_hdr_disable();
		dbg_i0("%s:ch[%d]\n", __func__, pch->c.ch);
		dpss_get_hw()->src_act &= (~C_BIT0); //clear
	} else { // (pch->c.ch == 1)
		if (!(dpss_get_hw()->src_act & C_BIT1))
			return;

		dbg_i0("%s:ch[%d]\n", __func__, pch->c.ch);
		dpss_get_hw()->src_act &= (~C_BIT1);	//clear
	}
	dbg_i2("%s:ch[%d]:end\n", "disable one play", pch->c.ch);
}

bool check_need_enable_4k1k(struct vframe_s *vf)
{
	bool ret = false;
	bool input_4k2k = false, output_4k1k = false;
	struct vinfo_s *vinfo = NULL;
	int flag = 2560 * 1440;

	if (!vf) {
		dbg_h1("%s: NULL param!!\n", __func__);
		return false;
	}

	vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(vinfo)) {
		dbg_h1("%s: get display vinfo err!!\n", __func__);
		return false;
	}

	if (vf->compWidth * vf->compHeight > flag || vf->width * vf->height > flag) {
		dbg_h1("%s: 4k2k input.\n", __func__);
		input_4k2k = true;
	}

	if ((vf->compWidth == 3840 && vf->compHeight == 1080) ||
		(vf->width == 3840 && vf->height == 1080)) {
		dbg_h1("%s: 4k1k input not set\n", __func__);
		input_4k2k = false;
	}

	if (vinfo->width >= 3840 && vinfo->height <= 1080) {
		dbg_h1("%s: 4k1k output.\n", __func__);
		output_4k1k = true;
	}

	if ((input_4k2k && output_4k1k) || nr_4k1k_en  ||
		(vf->dpss_flg & DPSS_FLG_NEED_SKIP &&
		!VFM_IS_I_SRC(vf->type)))
		ret = true;
	else
		ret = false;

	return ret;
}

void dpss_crc_init(void)
{
	/*set crc ctrl reg*/

	wr(FRC_ME_CAD_RAD_EN_2, 0);
	wr(FRC_ME_CAD_RAD_EN_1, 0);
	wr(FRC_ME_CAD_RAD_EN_0, 0);
	//wr(VPSS_VBE_CRC_CTRL, 0);
	dbg_h1("init 0 0x%x\n", rd(VPSS_VBE_CRC_CTRL));
	w_reg_bit(FRC_ME_DBG, 0x1, 5, 1);

	w_reg_bit(FRC_ME_PG_ST_MV_0, 0x1, 0, 12);
	w_reg_bit(FRC_ME_PG_ST_MV_1, 0x1, 0, 12);
	w_reg_bit(FRC_ME_PG_ST_MV_2, 0x1, 0, 12);

	w_reg_bit(FRC_ME_PG_ST_MV_0, 0x1, 16, 12);
	w_reg_bit(FRC_ME_PG_ST_MV_1, 0x1, 16, 12);
	w_reg_bit(FRC_ME_PG_ST_MV_2, 0x1, 16, 12);

	w_reg_bit(VPSS_VBE_CRC_CTRL, 0x7, 0, 3);
	w_reg_bit(VPSS_VBE_CRC_CTRL, 0xa90, 4, 12);
	w_reg_bit(VPSS_VBE_CRC_CTRL, 0xa, 21, 4);
	w_reg_bit(VPSS_VBE_CRC_SIZE, 1920, 0, 13);

	w_reg_bit(VPSS_VBE_CRC_CTRL, 7, 18, 3);
	w_reg_bit(DCTR_RAND_EN, 0, 10, 3);
	w_reg_bit(DCTR_RAND_EN, 0, 4, 3);
	w_reg_bit(DCTR_RAND_EN, 0x3, 0, 2);

	dbg_h1("DPSS CRC init ok  crc_ctrl: 0x%x\n", rd(VPSS_VBE_CRC_CTRL));
	dbg_h0("DPSS SLT TEST\n");
}

void dbg_dpss_reset1(unsigned int para)
{
	w_reg_bit(DPSS_TOP_SOFT_RST, 0x047e, 0, 13); //2024-12-05 from 0x1fff to 0xf7e
	w_reg_bit(DPSS_TOP_SOFT_RST, 0x0000, 0, 13);
	dbg_h1("dpss reset\n");
}

//0:nr_frc path 1:nr_di path //dpss_prm_init
static void _prm_top_init(struct dpss_ch_s *pch, struct PRM_DPSS_TOP *prm_top,
				int path_id, bool is_ex_di,
				struct vframe_s *vf)
{
	dbg_h1("%s:path_id =%d\n", __func__, path_id);
	//move to init memset(prm_top, 0, sizeof(*prm_top));
//	unsigned int fmt = YUV420, plan = PLANAR_X2, bit = BIT_008, uncm =
//	    CMPR_UN;
//	unsigned int i_p, cmpr;

	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;		// add abnormal case
	}

	if (!vf)
		return;
	//
	if (pch->d->di_front)
		prm_top->di_front = true;
	else
		prm_top->di_front = false;
	prm_top->dae_dpe_cfg_en = dpss_dbg_dae_dpe_cfg;
	dbg_h1("dae_dpe_cfg_en = %d\n", prm_top->dae_dpe_cfg_en);
	prm_top->case_id = pch->c.case_id;
	//---------
	if (path_id == 0) {
		prm_top->reverse[0] = 0;
		prm_top->reverse[1] = 0;
#ifdef _HIS_CODE_
		prm_top->dae_dsx_scale = 1;
		prm_top->dae_dsy_scale = 1;
#endif
		prm_top->mvx_div_mode = 1;
		prm_top->mvy_div_mode = 1;
		prm_top->dpe_slc_num = 1;

		prm_top->amdv_slc_num = 1;
		prm_top->amdv_pad = 0;

		prm_top->dpe_dw_dsx = 1;
		prm_top->dpe_dw_dsy = 1;
		prm_top->dpe_mc_phsx2 = 0; //set_need_check //ary set 1. ? if 0:vdin; 1:tbc?
		// DPSS_TOP_FUNC_CTRL bit0
		if (pch->d->is_i)
			prm_top->auto_alig_en = auto_alig_en_i;
		else
			prm_top->auto_alig_en = auto_alig_en;
		prm_top->alig_hmode = frc_top->align_hmode;
		prm_top->alig_vmode = frc_top->align_hmode;
		prm_top->org_hsize = 0xffff;
		//DPSS_VIDEO
		prm_top->film_mode = (enum DPSS_FILM_MODE)frc_top->film_mode;
		//DPSS_FRC_RATIO_1_2
		prm_top->frc_ratio = (enum DPSS_FRC_RATIO)frc_top->in_out_ratio;
		prm_top->dpss_mode = frc_top->dpss_mode;	//DPSS_FRC_MODE;
	} else if (path_id == 1) {
		prm_top->reverse[0] = 0;
		prm_top->reverse[1] = 0;
#ifdef _HIS_CODE_
		prm_top->dae_dsx_scale = 0;
		prm_top->dae_dsy_scale = 0;
#endif
		prm_top->mvx_div_mode = 1;
		prm_top->mvy_div_mode = 1;
		prm_top->dpe_slc_num = 1;

		prm_top->amdv_slc_num = 1;
		prm_top->amdv_pad = 0;

		prm_top->dpe_dw_dsx = 0;
		prm_top->dpe_dw_dsy = 0;
		prm_top->dpe_mc_phsx2 = 0;	//set_need_check
		prm_top->auto_alig_en = 0;
		prm_top->alig_hmode = 0;
		prm_top->alig_vmode = 0;
		prm_top->org_hsize = 0xffff;
		prm_top->dpss_mode = DPSS_DI_MODE;
		prm_top->film_mode = DPSS_VIDEO;
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_2;
	}

	prm_top->num_in		= pch->c.in_b_nub;
	prm_top->num_dpe_o    = pch->c.o_b_nub;
	prm_top->num_lc       = pch->c.num_lc;   //
	prm_top->num_aepe     = pch->c.num_aepe;
	prm_top->num_nr_wrpt  = pch->c.num_nr_wrpt;
	prm_top->num_dblk     = pch->c.num_pq_buf;
	prm_top->num_dpe_ro   = pch->c.num_pq_buf;
	prm_top->num_nr_me_ro = pch->c.num_pq_buf;
	if (!pch->c.en_dw)
		prm_top->dw_disable = true;
	else
		prm_top->dw_disable = false;
}

static void _prm_top_init_light(struct dpss_ch_s *pch, struct PRM_DPSS_TOP *prm_top,
				int path_id, bool is_ex_di,
				struct vframe_s *vf)
{
	unsigned int fmt = YUV420, plan = PLANAR_X2, bit = BIT_008, uncm =
	    CMPR_UN;
	unsigned int i_p, cmpr;

	if (dpss_p_as_i)
		pch->d->proc_as_i = true;
	else
		pch->d->proc_as_i = false;

	fmt = pch->d->fmt;
	plan = pch->d->plane;
	bit = BIT_008;
	i_p = IS_PSRC;
	cmpr = CMPR_UN;
	if (pch->d->is_10bit)
		bit = BIT_010;
	if (pch->d->is_afbcd)
		cmpr = CMPR_AFBC;
	if (pch->d->is_i || pch->d->proc_as_i) {
		if (pch->d->en_di_src) {
			if (is_ex_di)
				i_p = IS_ISRC;
		} else {
			i_p = IS_ISRC;
		}
	}
	if (dpss_dbg_in_fmt) {
		i_p = (dpss_dbg_in_fmt >> 24) & 0xff;
		fmt = (dpss_dbg_in_fmt >> 16) & 0xff;
		plan = (dpss_dbg_in_fmt >> 8) & 0xff;
		bit = (dpss_dbg_in_fmt) & 0xff;
		dbg_h1("in:fmt:%d,%d,%d", fmt, plan, bit);
	}
	dbg_h2("is_ex_di = %d, i_p = %d, plan=%d, bit=%d\n", is_ex_di, i_p, plan, bit);
	prm_top->src_fs_fmt	= fmt_cfg(fmt, plan, bit, cmpr, i_p);
	//ds fmt:
	if (vf && pch->d->is_afbcd) {
		if (vf->bitdepth_dw & BITDEPTH_Y10) {
			if ((vf->type & VIDTYPE_VIU_444) ||
				(vf->type & VIDTYPE_RGB_444) ||
				((vf->type & VIDTYPE_VIU_422) &&
				vf->bitdepth_dw & FULL_PACK_422_MODE) ||
				(((vf->type & VIDTYPE_VIU_NV12) ||
				(vf->type & VIDTYPE_VIU_NV21)) &&
				(vf->bitdepth_dw & FULL_PACK_420_MODE)))
				//10 bit full packet
				bit = BIT_010;
			else
				//10 bit in 12 bit
				bit = BIT_10IN12;
		} else {
			if (vf->canvas0_config[0].bit_depth & P010_MODE)
				bit = BIT_P010;
			else
				bit = BIT_008;
		}
	}
	cmpr = CMPR_UN;
	i_p = IS_PSRC;
	prm_top->src_ds_fmt	= fmt_cfg(fmt, plan, bit, cmpr, i_p);
#ifdef FUNC_EN_DBG_TREE
	if (dpss_dbg_level_get() > 1)	//dbg only
		print_s_PRM_SRC_FMT_t(&prm_top->src_fs_fmt, "src_fs");
#endif
	if (pch->d->di_front) {
		prm_top->nro_fs_fmt =
		    fmt_cfg(YUV420, PLANAR_X2, BIT_008, CMPR_UN, IS_PSRC);
	} else if ((dpss_en_afbc & C_BIT0 || (pch->c.o_afbc & C_BIT0))) {
		prm_top->nro_fs_fmt =
		    fmt_cfg(YUV420, PLANAR_X1, BIT_008, CMPR_AFBC, IS_PSRC);
	} else if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1)) {
		prm_top->nro_fs_fmt	= fmt_cfg(YUV422, PLANAR_X2, BIT_010, CMPR_AFRC, IS_PSRC);

	} else {
		if (prm_top->out_mode == OUT_YUV422_1_PLANE)
			prm_top->nro_fs_fmt = fmt_cfg(YUV422, PLANAR_X1,
				BIT_010, CMPR_UN, IS_PSRC);
		else if (prm_top->out_mode == OUT_YUV422_2_PLANE)
			prm_top->nro_fs_fmt = fmt_cfg(YUV422, PLANAR_X2,
				BIT_010, CMPR_UN, IS_PSRC);
		else
			prm_top->nro_fs_fmt = fmt_cfg(YUV420, PLANAR_X2,
				BIT_008, CMPR_UN, IS_PSRC);
	}

	if (prm_top->out_mode == OUT_YUV422_1_PLANE)
		prm_top->nro_ds_fmt = fmt_cfg(YUV422, PLANAR_X1,
				BIT_010, CMPR_UN, IS_PSRC);
	else if (prm_top->out_mode == OUT_YUV422_2_PLANE)
		prm_top->nro_ds_fmt = fmt_cfg(YUV422, PLANAR_X2,
				BIT_010, CMPR_UN, IS_PSRC);
	else
		prm_top->nro_ds_fmt = fmt_cfg(YUV420, PLANAR_X2,
			BIT_008, CMPR_UN, IS_PSRC);

	if (dpss_dbg_o_fmt) {
		uncm = (dpss_dbg_o_fmt >> 24) & 0xff;
		fmt = (dpss_dbg_o_fmt >> 16) & 0xff;
		plan = (dpss_dbg_o_fmt >> 8) & 0xff;
		bit = (dpss_dbg_o_fmt) & 0xff;

		prm_top->nro_fs_fmt	= fmt_cfg(fmt, plan, bit, uncm, IS_PSRC);
		if (prm_top->nro_fs_fmt.src_fmt == YUV422 &&
				prm_top->nro_fs_fmt.src_plan == PLANAR_X2)
			prm_top->out_mode = OUT_YUV422_2_PLANE;
		else
			prm_top->out_mode = prm_top->nro_fs_fmt.src_fmt;
		dbg_h1("o:fmt:%d,%d,%d,%d", fmt, plan, bit, prm_top->out_mode);
	}
	prm_top->dpe_game_mode = 0;//dpe_game_mode
	prm_top->dae_step_sel = 2;
	prm_top->dolby_mode = DOLBY_WRAP_BYPS;
	if (pch->d->nr_cp_mode)
		prm_top->dolby_mode = DOLBY_DPSS_PRL_MODE;
	prm_top->pvsync_intr_en = 1;

	prm_top->mc_setting.luma_srng_mode = SRNG_NORMAL;
	prm_top->mc_setting.chrm_srng_mode = SRNG_NORMAL;

	prm_top->frm_hsize_sel = 3;	//frame hsize align mode
	prm_top->mif_mmu_mode_en = 0;
	prm_top->ds_mif_mmu_mode_en = 0;

	prm_top->comp_setting.vdin_mmu_mode_en = 0;	//0:disable 1:enable
	//prm_top->comp_setting.mmu_mode_en         = 0;//0:disable 1:enable
	prm_top->comp_setting.mmu_mode_en = 1;	//0:disable 1:enable
	prm_top->comp_setting.mmu_page_mode = 0;	//0:4k page 1:8k page
	prm_top->comp_setting.reg_444to422_mode = 0;	//filter mode
	prm_top->comp_setting.vfce_in_overlap = 0;	//vfce input overlap
	prm_top->comp_setting.src_comp_switch = 0;	//afrc afbc switch
	prm_top->comp_setting.src_switch_num = 3;	//switch num
	prm_top->comp_setting.nro_comp_switch = 0;	//afrc afbc switch
	prm_top->comp_setting.nro_switch_num = 3;	//switch num
	prm_top->comp_setting.afrc_head_mode = dpss_afrc_head;	//0:wo header 1: wi header
	prm_top->comp_setting.afrc_dict_mode_y = afrc_dict_mode_y;	//
	prm_top->comp_setting.afrc_dict_mode_c = afrc_dict_mode_c;	//
	prm_top->comp_setting.afrc_target_byte_y = dpss_afrc_y;
	prm_top->comp_setting.afrc_target_byte_c = dpss_afrc_c;
	prm_top->comp_setting.vfcd_rev_mode = 0;	//rev mode
	prm_top->comp_setting.vfcd_h_skip = 0;	//skip
	prm_top->comp_setting.vfcd_h_skip = 0;	//skip
	prm_top->comp_setting.fmt444_comb = pch->d->fmt444_comb;	//comb 8bit 444
	prm_top->comp_setting.dpe_vfce_num = 0;	//vfce start num
	prm_top->comp_setting.dpe_vfcd_num = 1;	//vfcd start num

	if (pch->c.parm.di_parm.rotate_flag & VFRAME_FLAG_MIRROR_H)
		prm_top->dp_set_rev_mode |= C_BIT0;
	if (pch->c.parm.di_parm.rotate_flag & VFRAME_FLAG_MIRROR_V)
		prm_top->dp_set_rev_mode |= C_BIT1;

//	if (frc_top->frc_rev_mode)
//		prm_top->frc_rev_mode = frc_top->frc_rev_mode;	// dpss_frc_rev_mode;
	prm_top->fnr_sw_mode = 0;
	prm_top->vdi_sw_mode = 0;
	prm_top->sw_tbc_mode = 0;
	prm_top->sw_ctrl_en = 0;
	prm_top->src_is_1080p_nods = 1;	//ary 2024-12-31 for no small picture in
	prm_top->nr_hscale_on = 0;
	prm_top->frc_ds_scale_en = 0;
//      prm_top->frc_ds_scale_en                  = 1; //for input have no small pic
	prm_top->nr_double_rst_mode = 1;
	prm_top->bbd_only = 0;
	prm_top->bbd_parallel = 0;
	prm_top->cut_win_en = 0;	// dpss_frc_cut_win_en;
	prm_top->dpe_ro_wdma_en = 1;
	prm_top->lcevc_en = 0;
	prm_top->dpe_dw_off = 0;
	prm_top->nr_aapps_up = 0;
	prm_top->nr_aapps_ds = 0;
	prm_top->nr_aapps_on = 0;
	prm_top->nr_aapps_mode = 0;
	prm_top->di_interlace = 0;	//yu.zong 2024-12-06
	prm_top->sw_dct_frame_cnt = 0;
	if ((pch->d->is_i || pch->d->proc_as_i) &&
	    !pch->d->is_field && !dpss_en_filed)
		prm_top->di_interlace = 1;	//yu.zong 2024-12-06
	if (pch->d->is_top_first)
		prm_top->f_top = 1;
	if (dpss_force_b)
		prm_top->f_top = (prm_top->f_top + 1) & 0x1; //rev
	if (pch->d->is_secure)
		prm_top->is_secure = true;
	else
		prm_top->is_secure = false;

	if (vf) {
		dbg_h0("%s vf->dpss_flg:0x%x\n", __func__, vf->dpss_flg);
		if ((check_need_enable_4k1k(vf) && !pch->c.ch) ||
				(vf->dpss_flg & DPSS_FLG_NEED_SKIP &&
				!VFM_IS_I_SRC(vf->type)))
			prm_top->vds_4k1k_en = 1;
		else
			prm_top->vds_4k1k_en = 0;
	}

	dbg_i1("%s:di_interlace:%d,%d\n", __func__, prm_top->di_interlace, prm_top->f_top);
	dbg_i1("%s:prm_top->comp_setting.afrc_head_mode:%d,%d %d\n", __func__,
		prm_top->comp_setting.afrc_head_mode,
		prm_top->comp_setting.afrc_target_byte_y,
		prm_top->comp_setting.afrc_target_byte_c);

	dbg_i1(" dpss_en_filed=%d, %d\n", dpss_en_filed, dpss_force_b);
	dbg_h0("prm_top->dolby_mode:%d\n", prm_top->dolby_mode);
	dbg_h0("prm_top->vds_4k1k_en:%d\n", prm_top->vds_4k1k_en);
//	dbg_h0("%s: frc_rev_mode:%d, node:%d.\n", __func__, prm_top->frc_rev_mode,
//		frc_top->frc_rev_mode);
	if (dpss_dbg_dos_mode)
		prm_top->force_dos_mode = 1;
	dbg_h2("%s %d\n", __func__, dpss_dbg_dos_mode);
	prm_top->s_cnt = 0;
	prm_top->in_q = &pch->q;
}

// src : 0 or 1
void _prm_top_init_buffer(struct PRM_DPSS_TOP *prm_top,
			  struct dpss_ch_s *pch,
			  unsigned int src)
{
	int i;
	//unsigned long addr_offset = 0; //to-do
	unsigned long laddr_y, laddr_uv;
	unsigned int *addr_t1, *addr_t2, *addr_t3, *addr_t4;
	unsigned int *addr_t5, *addr_t6;
	unsigned int *addr_t11, *addr_t12, *addr_t21, *addr_t22; //10-02
	unsigned int *addr_t31, *addr_t32;

	struct dpss_buf_sml_s *sml_info;
	unsigned char b_idx;
	unsigned int buf_nub;

	if (!prm_top || !pch || !pch->c.sml_info) {
		DBG_ERR("%s:src: %d\n", __func__, src);
		return;
	}
	dbg_h1("%s:src=%d\n", __func__, src);
	sml_info = pch->c.sml_info;
	dbg_i0("%s:is_pps ch=%d,=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);

	buf_nub = pch->c.o_b_nub;
	if (src > 1) {
		DBG_WARN("%s:src fix to 0:%d\n", __func__, src);
		src = 0;
	}
	if (buf_nub > DPSS_HW_LOOP_IN_OUT_BUF_NUB) {
		DBG_WARN("%s:buf_nub overflow:%d\n", __func__, buf_nub);
		buf_nub = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
	}
	if (src == 0) {
		//post buffer:
		addr_t1 = &prm_top->src0_nro_fbuf_yaddr[0];
		addr_t2 = &prm_top->src0_nro_fbuf_caddr[0];
		addr_t3 = &prm_top->src0_dio_fbuf_yaddr[0];
		addr_t4 = &prm_top->src0_dio_fbuf_caddr[0];
		addr_t5 = &prm_top->src0_di2pps_buf_yaddr[0];
		addr_t6 = &prm_top->src0_di2pps_buf_caddr[0];

		if (!pch->d->proc_as_i && !pch->d->is_i) {
			addr_t11 = &prm_top->src0_nro_fbuf_af_y[0];
			addr_t12 = &prm_top->src0_nro_fbuf_af_c[0];
			addr_t21 = &prm_top->src0_nro_fbuf_m_y[0];
			addr_t22 = &prm_top->src0_nro_fbuf_m_c[0];
			addr_t31 = &prm_top->src0_diopps_fbuf_yaddr[0];
			addr_t32 = &prm_top->src0_diopps_fbuf_caddr[0];
			for (i = 0; i < buf_nub; i++) {	//out
				b_idx = (i + pch->d->idx_start) % buf_nub;
				laddr_y = pch->c.addr_afbc_tab[b_idx];
				laddr_uv = pch->c.addr_afbc_tab_c[b_idx];
				addr_t11[i] = (unsigned int)(laddr_y >> 9);
				addr_t12[i] = (unsigned int)(laddr_uv >> 9);

				laddr_y = pch->c.addr_nr[b_idx];
				laddr_uv = pch->c.addr_nr_uv[b_idx];
				addr_t21[i] = (unsigned int)(laddr_y >> 9);
				addr_t22[i] = (unsigned int)(laddr_uv >> 9);

				laddr_y = pch->c.addr_lc[b_idx];
				laddr_uv = pch->c.addr_lc_uv[b_idx];
				addr_t31[i] = (unsigned int)(laddr_y >> 9);
				addr_t32[i] = (unsigned int)(laddr_uv >> 9);

			}
		}
		for (i = 0; i < buf_nub; i++) {
			b_idx = (i + pch->d->idx_start) % buf_nub;
			if (pch->d->is_i || pch->d->proc_as_i) {	//----------is i
				if (!dpss_en_afbc && !pch->c.o_afbc) {
					laddr_y = pch->c.addr_nr[b_idx];
					laddr_uv = pch->c.addr_nr_uv[b_idx];
					addr_t3[i] =
					    (unsigned int)(laddr_y >> 9);
					addr_t4[i] =
					    (unsigned int)(laddr_uv >> 9);
				} else {
					laddr_y = pch->c.addr_afbc_tab[b_idx];
					laddr_uv = pch->c.addr_afbc_tab_c[b_idx];
					addr_t3[i] =
					    (unsigned int)(laddr_y >> 9);
					addr_t4[i] =
					    (unsigned int)(laddr_uv >> 9);
				}
				laddr_y = pch->c.addr_lc[i];
				laddr_uv = pch->c.addr_lc_uv[i];
				addr_t1[i] = (unsigned int)(laddr_y >> 9);
				addr_t2[i] = (unsigned int)(laddr_uv >> 9);

				if (pch->c.prm_top.is_di2pps) {
					laddr_y = pch->c.addr_di2pps[b_idx];
					laddr_uv = pch->c.addr_di2pps_uv[b_idx];
					addr_t5[i] = (unsigned int)(laddr_y >> 9);
					addr_t6[i] = (unsigned int)(laddr_uv >> 9);
				} else {
					laddr_y = 0;
					laddr_uv = 0;
					addr_t5[i] = (unsigned int)(laddr_y >> 9);
					addr_t6[i] = (unsigned int)(laddr_uv >> 9);
				}
			} else {	//----------is p
				if (!dpss_en_afbc && !pch->c.o_afbc) {
					if (dpss_nr_debug) {
						//for nr debug pattern,todo temp
						laddr_y = pch->c.addr_nr[b_idx];
						laddr_uv = pch->c.addr_nr_uv[b_idx];
						addr_t3[i] = (unsigned int)(laddr_y >> 9);
						addr_t4[i] = (unsigned int)(laddr_uv >> 9);

						laddr_y = pch->c.addr_lc[b_idx];
						laddr_uv = pch->c.addr_lc_uv[b_idx];
						addr_t1[i] = (unsigned int)(laddr_y >> 9);
						addr_t2[i] = (unsigned int)(laddr_uv >> 9);
					} else {
						laddr_y = pch->c.addr_nr[b_idx];
						laddr_uv = pch->c.addr_nr_uv[b_idx];
						addr_t1[i] = (unsigned int)(laddr_y >> 9);  //test1
						addr_t2[i] = (unsigned int)(laddr_uv >> 9);
						laddr_y = pch->c.addr_lc[b_idx];
						laddr_uv = pch->c.addr_lc_uv[b_idx];
						addr_t3[i] = (unsigned int)(laddr_y >> 9);//test1
						addr_t4[i] = (unsigned int)(laddr_uv >> 9);
					}
				} else {
					laddr_y = pch->c.addr_afbc_tab[b_idx];
					laddr_uv = pch->c.addr_afbc_tab_c[b_idx];
					addr_t1[i] =
					    (unsigned int)(laddr_y >> 9);
					addr_t2[i] =
					    (unsigned int)(laddr_uv >> 9);
					addr_t3[i] = addr_t1[i];
					addr_t4[i] = addr_t2[i];
				}
				prm_top->fbuf_is_pps[i] = 0;
			}
		}
		//set from buf_nub to 15:
		for (i = buf_nub; i < 15; i++) {
			addr_t1[i] = addr_t1[i % buf_nub];
			addr_t2[i] = addr_t2[i % buf_nub];
			addr_t3[i] = addr_t3[i % buf_nub];
			addr_t4[i] = addr_t4[i % buf_nub];
		}
		//dw buffer:
		addr_t1 = &prm_top->src0_nro_dwbuf_yaddr[0];
		addr_t2 = &prm_top->src0_nro_dwbuf_caddr[0];
		addr_t3 = &prm_top->src0_dio_dwbuf_yaddr[0];
		addr_t4 = &prm_top->src0_dio_dwbuf_caddr[0];
		for (i = 0; i < buf_nub; i++) {
			b_idx = (i + pch->d->idx_start) % buf_nub;
			laddr_y = pch->c.addr_dw[b_idx];
			laddr_uv = pch->c.addr_dwuv[b_idx];
			addr_t1[i] = (unsigned int)(laddr_y >> 9);
			addr_t2[i] = (unsigned int)(laddr_uv >> 9);

			addr_t3[i] = addr_t1[i];
			addr_t4[i] = addr_t2[i];
		}

		//set from buf_nub to 15:
		for (i = buf_nub; i < 15; i++) {
			addr_t1[i] = addr_t1[i % buf_nub];
			addr_t2[i] = addr_t2[i % buf_nub];
			addr_t3[i] = addr_t3[i % buf_nub];
			addr_t4[i] = addr_t4[i % buf_nub];
		}
		//set small buffer addr:
		prm_top->src0_mix_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MIX] >> 4);
		prm_top->src0_me_info_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_BLK_INFO] >> 4);
		prm_top->src0_mtn_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MTN] >> 4);
		prm_top->src0_tfbf_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_TFBC] >> 4);
		prm_top->src0_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MV] >> 4);
		prm_top->src0_me_alp_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MEALP] >> 4);
		prm_top->src0_ro1_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_RO1] >> 4);
		prm_top->src0_ro2_addr =
			(unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_RO2] >> 4);
		prm_top->src0_dct_his_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_GRAID] >> 4);
		prm_top->src0_dct_y_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_Y] >> 4);
		prm_top->src0_dct_c_addr =
			(unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_C] >> 4);
		prm_top->src0_grad_h_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_GRAD_H] >> 4);
		prm_top->src0_grad_v_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_GRAD_V] >> 4);
		prm_top->src0_dmsq_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DMSQ] >> 4);
		prm_top->src0_nro_dw_addr =
		    (unsigned int)(pch->c.addr_dw[0] >> 4);

		if (pch->d->idx_hd)
			b_idx = buf_nub;
		else
			b_idx = 0;
		prm_top->src0_afbce_h_yaddr =
		    (unsigned int)(pch->c.addr_afbc_info[b_idx] >> 4);
		prm_top->src0_afbce_h_caddr =
			(unsigned int)(pch->c.addr_afbc_info_c[b_idx] >> 4); //tmp
		//step:

		prm_top->src0_mix_step[0] = sml_info->size_mix >> 4;
		prm_top->src0_mix_step[1] = sml_info->size_mix >> 4;
		prm_top->src0_me_info_step = sml_info->size_blk_info >> 4;
		prm_top->src0_mtn_step = sml_info->size_mtn >> 4;
//		prm_top->src0_me_alp_step = sml_info->size_mix;//to-do
		prm_top->src0_tfbf_step = sml_info->size_tfbc >> 4;
		prm_top->src0_mv_step = sml_info->size_mv >> 4;
		prm_top->src0_me_alp_step = sml_info->size_alp >> 4; //same as mv;
		prm_top->src0_dct_his_step = sml_info->size_dct_grid >> 4;
		prm_top->src0_dct_y_step = sml_info->size_dct_y >> 4;
		prm_top->src0_dct_c_step = sml_info->size_dct_c >> 4;
		prm_top->src0_grad_h_step = sml_info->size_grad_h >> 4;
		prm_top->src0_grad_v_step = sml_info->size_grad_v >> 4;
		prm_top->src0_dmsq_step = sml_info->size_dmsq >> 4;
		prm_top->src0_nro_dw_step = sml_info->size_dw >> 4;
		if ((dpss_en_afbc & C_BIT0) || (pch->c.o_afbc & C_BIT0))
			prm_top->src0_afbce_h_y_step = sml_info->size_afbc >> 4;
		else
			prm_top->src0_afbce_h_y_step = sml_info->size_afrc_y >> 4;
		prm_top->src0_afbce_h_c_step = sml_info->size_afrc_c >> 4; //tmp

		//frc small buffer addr
		prm_top->inp_mbuf_addr[0] =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_INP_MBUF0] >> 4);
		prm_top->inp_mbuf_addr[1] =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_INP_MBUF1] >> 4);
		prm_top->frc_me_mbuf_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_ME_MBUF] >> 4);
		prm_top->src0_me_nc_uni_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_ME_NC_UNI_MV] >> 4);
		prm_top->src0_me_cn_uni_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_ME_CN_UNI_MV] >> 4);
		prm_top->src0_me_pc_phs_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_ME_PC_PHS_MV] >> 4);
		prm_top->src0_logo_iir_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_LOGO_IIR_BUF] >> 4);
		prm_top->src0_logo_scc_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_LOGO_SSC_BUF] >> 4);
		prm_top->src0_blk_logo_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_BLK_LOGO] >> 4);
		prm_top->src0_pix_logo_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_PIX_LOGO] >> 4);
		prm_top->src0_vp_mc_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_VP_MC_MV] >> 4);
		prm_top->src0_vp_mc_logo_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_VP_MC_LOGO] >> 4);
		prm_top->src0_frc_mero_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPSS_SML_BUF_IDX_FRC_MERO] >> 4);
		// frc step
		prm_top->inp_mbuf_step = sml_info->size_frc_inp_mbuf0;
		prm_top->frc_me_mbuf_step = sml_info->size_frc_me_mbuf >> 4;
		prm_top->src0_me_nc_uni_mv_step =
		    sml_info->size_frc_me_nc_uni_mv >> 4;
		prm_top->src0_me_cn_uni_mv_step =
		    sml_info->size_frc_me_cn_uni_mv >> 4;
		prm_top->src0_me_pc_phs_mv_step =
		    sml_info->size_frc_me_pc_phs_mv >> 4;
		prm_top->src0_logo_iir_step = sml_info->size_frc_logo_iir >> 4;
		prm_top->src0_logo_scc_step = sml_info->size_frc_logo_ssc >> 4;
		prm_top->src0_blk_logo_step = sml_info->size_frc_blk_logo >> 4;
		prm_top->src0_pix_logo_step = sml_info->size_frc_pix_logo >> 4;
		prm_top->src0_vp_mc_mv_step = sml_info->size_frc_vp_mc_mv >> 4;
		prm_top->src0_vp_mc_logo_step = sml_info->size_frc_vp_mc_logo >> 4;
		prm_top->src0_frc_mero_step = sml_info->size_frc_mer0 >> 4;
	} else if (src == 1) {
		//post buffer:
		addr_t1 = &prm_top->src1_nro_fbuf_yaddr[0];
		addr_t2 = &prm_top->src1_nro_fbuf_caddr[0];
		addr_t3 = &prm_top->src1_dio_fbuf_yaddr[0];
		addr_t4 = &prm_top->src1_dio_fbuf_caddr[0];
		for (i = 0; i < buf_nub; i++) {
			b_idx = (i + pch->d->idx_start) % buf_nub;
			if (pch->d->is_i || pch->d->proc_as_i) {
				laddr_y = pch->c.addr_nr[b_idx];
				laddr_uv = pch->c.addr_nr_uv[b_idx];
				addr_t3[i] = (unsigned int)(laddr_y >> 9);
				addr_t4[i] = (unsigned int)(laddr_uv >> 9);
				laddr_y = pch->c.addr_lc[i];
				laddr_uv = pch->c.addr_lc_uv[i];
				addr_t1[i] = (unsigned int)(laddr_y >> 9);
				addr_t2[i] = (unsigned int)(laddr_uv >> 9);
			} else {
				laddr_y = pch->c.addr_nr[b_idx];
				laddr_uv = pch->c.addr_nr_uv[b_idx];
				addr_t1[i] = (unsigned int)(laddr_y >> 9);
				addr_t2[i] = (unsigned int)(laddr_uv >> 9);
				addr_t3[i] = addr_t1[i];
				addr_t4[i] = addr_t2[i];
			}
		}
		//set from buf_nub to 15:
		for (i = buf_nub; i < 15; i++) {
			addr_t1[i] = addr_t1[i % buf_nub];
			addr_t2[i] = addr_t2[i % buf_nub];
			addr_t3[i] = addr_t3[i % buf_nub];
			addr_t4[i] = addr_t4[i % buf_nub];
		}
		//debug: src1:
		dbg_h2("%s:check src1 addr o:\n", __func__);
		for (i = 0; i < 15; i++) {
			dbg_h2("%d:0x%x 0x%x 0x%x 0x%x\n", i,
			       prm_top->src1_nro_fbuf_yaddr[i],
			       prm_top->src1_nro_fbuf_caddr[i],
			       prm_top->src1_dio_fbuf_yaddr[i],
			       prm_top->src1_dio_fbuf_caddr[i]);
		}
		//dw buffer:
		addr_t1 = &prm_top->src1_nro_dwbuf_yaddr[0];
		addr_t2 = &prm_top->src1_nro_dwbuf_caddr[0];
		addr_t3 = &prm_top->src1_dio_dwbuf_yaddr[0];
		addr_t4 = &prm_top->src1_dio_dwbuf_caddr[0];
		for (i = 0; i < buf_nub; i++) {
			b_idx = (i + pch->d->idx_start) % buf_nub;
			laddr_y = pch->c.addr_dw[b_idx];
			laddr_uv = pch->c.addr_dwuv[b_idx];
			addr_t1[i] = (unsigned int)(laddr_y >> 9);
			addr_t2[i] = (unsigned int)(laddr_uv >> 9);
			addr_t3[i] = addr_t1[i];
			addr_t4[i] = addr_t2[i];
		}
		//set from buf_nub to 15:
		for (i = buf_nub; i < 15; i++) {
			addr_t1[i] = addr_t1[i % buf_nub];
			addr_t2[i] = addr_t2[i % buf_nub];
			addr_t3[i] = addr_t3[i % buf_nub];
			addr_t4[i] = addr_t4[i % buf_nub];
		}

		//small buffer addr:
		prm_top->src1_mix_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MIX] >> 4);
		prm_top->src1_me_info_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_BLK_INFO] >> 4);
		prm_top->src1_mtn_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MTN] >> 4);
		prm_top->src1_tfbf_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_TFBC] >> 4);
		prm_top->src1_mv_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MV] >> 4);
		prm_top->src1_me_alp_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_MEALP] >> 4);
		prm_top->src1_ro1_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_RO1] >> 4);
		prm_top->src1_ro2_addr =
			(unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_RO2] >> 4);
		prm_top->src1_dct_his_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_GRAID] >> 4);
		prm_top->src1_dct_y_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_Y] >> 4);
		prm_top->src1_dct_c_addr =
			(unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DCT_C] >> 4);
		prm_top->src1_grad_h_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_GRAD_H] >> 4);
		prm_top->src1_grad_v_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_GRAD_V] >> 4);
		prm_top->src1_dmsq_addr =
		    (unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DMSQ] >> 4);
		prm_top->src1_dmsq_addr =
			(unsigned int)(pch->c.addr_sml[0][DPPS_SML_BUF_IDX_DMSQ] >> 4);
		prm_top->src1_nro_dw_addr = (unsigned int)(pch->c.addr_dw[0] >> 4);
		//step:
		prm_top->src1_mix_step[0] = sml_info->size_mix >> 4;
		prm_top->src1_mix_step[1] = sml_info->size_mix >> 4;
		prm_top->src1_me_info_step = sml_info->size_blk_info >> 4;
		prm_top->src1_mtn_step = sml_info->size_mtn >> 4;
//		prm_top->src1_me_alp_step = sml_info->size_mix;//to-do
		prm_top->src1_tfbf_step = sml_info->size_tfbc >> 4;
		prm_top->src1_mv_step = sml_info->size_mv >> 4;
		prm_top->src1_me_alp_step = sml_info->size_alp >> 4; //same as mv;

		prm_top->src1_dct_his_step = sml_info->size_dct_grid >> 4;
		prm_top->src1_dct_y_step = sml_info->size_dct_y >> 4;
		prm_top->src1_dct_c_step = sml_info->size_dct_c >> 4;
		prm_top->src1_grad_h_step = sml_info->size_grad_h >> 4;
		prm_top->src1_grad_v_step = sml_info->size_grad_v >> 4;
		prm_top->src1_dmsq_step = sml_info->size_dmsq >> 4;
		prm_top->src1_nro_dw_step = sml_info->size_dw >> 4;
	}
	prm_top->src_ro_step = sml_info->size_ro1 >> 4;
	dbg_h2("%s:src_ro_step = %d\n", __func__, prm_top->src_ro_step);
	dbg_h0("%s:src:%d, buf nub:%d\n", __func__, src, buf_nub);
}

bool dpss_dbg_in_sel;		//tmp //= true;
module_param_named(dpss_dbg_in_sel, dpss_dbg_in_sel, bool, 0664);
int dpss_dbg_ratio = -1;
module_param_named(dpss_dbg_ratio, dpss_dbg_ratio, int, 0664);

unsigned int dpss_w_mode;
module_param_named(dpss_w_mode, dpss_w_mode, uint, 0664);

bool dpss_f_mode_get(unsigned int mode, unsigned int *val)
{
	unsigned int tmp;

	*val = 0;
	if (mode == 0) {
		tmp = dpss_w_mode & 0x7f;
		if (tmp) {
			*val = tmp;
			return true;
		}
	}

	if (mode == 1) {	//dae mode
		tmp = (dpss_w_mode >> 8) & 0x7f;
		if (tmp) {
			*val = tmp;
			return true;
		}
	}

	if (mode == 2) {
		tmp = (dpss_w_mode >> 16) & 0x7f;
		if (tmp) {
			*val = tmp;
			return true;
		}
	}

	if (mode == 3) {	//dpe mode:
		tmp = (dpss_w_mode >> 24) & 0x7f;
		if (tmp) {
			*val = tmp;
			return true;
		}
	}

	if (mode == 4) {
		tmp = dpss_w_mode & C_BIT7;
		if (tmp) {
			if (dpss_w_mode & C_BIT15)
				*val = 1;
			else
				*val = 0;
			return true;
		}
	}
	if (mode == 5) {
		tmp = dpss_w_mode & C_BIT23;
		if (tmp) {
			if (dpss_w_mode & C_BIT31)
				*val = 1;
			else
				*val = 0;
			return true;
		}
	}
	return false;
}

void dpss_val_2_top(struct PRM_DPSS_TOP *prm_top,
		    struct dpss_user_para_s *prm_user)
{
	prm_top->dpss_mode = prm_user->dpss_mode;
	prm_top->dae_nr_mode = prm_user->dae_nr_mode;
	prm_top->dpe_nr_mode = prm_user->dpe_nr_mode;
	prm_top->dpe_di_mode = prm_user->dpe_di_mode;
	prm_top->src_mode = prm_user->src_mode;
	//move out prm_top->cfg_slc = prm_user->cfg_slc;
}

bool dpss_dis_frc;		//bit 0 for dae
module_param_named(dpss_dis_frc, dpss_dis_frc, bool, 0664);

void dpss_val_user(struct dpss_ch_s *pch,
		struct dpss_user_para_s *prm_user,
		struct dpss_sub_vf_s *vfs)
{
	unsigned int val;
	unsigned int case_id;
	unsigned int src;

	if (dpss_en_lcevc & C_BIT0) {
		prm_user->dpss_mode	   = DPSS_FRC_NR_MODE;

		prm_user->dae_nr_mode		=  DAE_BYPS_MODE;
		prm_user->dpe_nr_mode		=  DPE_NR_BYPS;
		prm_user->dpe_di_mode		=  DPE_NR_BYPS;
		prm_user->src_mode		= 0;
		prm_user->cfg_slc = 1;

		pch->c.case_id = TST_CASE_IDX_0102;

		dbg_i0("%s: lcevc case_id=%d\n", __func__, pch->c.case_id);
		return;
	}
	if (dpss_frc_bypass) {
		pch->c.parm.dps_work_mode &= ~DPSS_WORK_MODE_FRC;
		dbg_i0("%s:dpss_frc_bypass\n", __func__);
	}
	if (dpss_dbg_tst_case) {
		pch->c.case_id = dpss_dbg_tst_case;
	} else {
		if (pch->c.parm.dps_work_mode & DPSS_WORK_MODE_MAIN ||
			pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC)
			src = 0;
		else
			src = 1;
		dbg_i0("%s:src=%d\n", __func__, src);
		if (pch->d->di_front) {
			if (src) {
				if (VFM_IS_I_SRC(vfs->type))
					pch->c.case_id = TST_CASE_IDX_SRC1_NRDI;
				else
					pch->c.case_id = TST_CASE_IDX_SRC1_NR;
			} else {
				if (VFM_IS_I_SRC(vfs->type))
					pch->c.case_id = TST_CASE_IDX_0107;
				else
					pch->c.case_id = TST_CASE_IDX_0000;
			}
		} else if (src) {
			//src 1:
			if (VFM_IS_I_SRC(vfs->type))
				pch->c.case_id = TST_CASE_IDX_SRC1_NRDI;
			else
				pch->c.case_id = TST_CASE_IDX_SRC1_NR;
		} else {
			if ((pch->c.parm.dps_work_mode & DPSS_WORK_MODE_FRC) &&
			    !dpss_dis_frc) {
				//en frc:
				if (VFM_IS_I_SRC(vfs->type))
					pch->c.case_id = TST_CASE_IDX_0157;
				else
					pch->c.case_id = TST_CASE_IDX_0102;
			} else {
				if (VFM_IS_I_SRC(vfs->type))
					pch->c.case_id = TST_CASE_IDX_0107;
				else
					pch->c.case_id = TST_CASE_IDX_0000;
			}
		}
	}
	dbg_i0("%s:tst case:%d\n", __func__, pch->c.case_id);

	case_id = pch->c.case_id;
	//dpss_en_dct = 1;  // nr+frc
	//dpss_en_hdr = 1;  // nr+frc
	//dpss_bypass_ko = 0;  // nr+frc

	if (case_id == TST_CASE_IDX_0000) {
		prm_user->dpss_mode = DPSS_NR_SRC0_MODE;
		prm_user->dae_nr_mode = DAE_NR_MODE;
		prm_user->dpe_nr_mode = DPE_NR_MODE;
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 0;
		//prm_user->cfg_slc     = 0;
		dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;
	} else if (case_id == TST_CASE_IDX_0001) {
		prm_user->dpss_mode = DPSS_DI_MODE;
		prm_user->dae_nr_mode = DAE_VDI_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_VDI_MODE; //DPE_NR_BYPS;
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 1;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_1_SRC1;
	} else if (case_id == TST_CASE_IDX_SRC1_NR) {
		prm_user->dpss_mode = DPSS_NR_SRC1_MODE;
		prm_user->dae_nr_mode = DAE_NR_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_NR_MODE;
		prm_user->dpe_di_mode = DPE_NR_MODE;
		prm_user->src_mode = 1;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_1_SRC1;
	} else if (case_id == TST_CASE_IDX_SRC1_NRDI) {
		prm_user->dpss_mode = DPSS_NRDI_MODE;
		prm_user->dae_nr_mode = DAE_VDI_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_NRDI_MODE;
		prm_user->dpe_di_mode = DPE_NRDI_MODE;
		prm_user->src_mode = 1;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_1_SRC1;
	} else if (case_id == TST_CASE_IDX_0002) {	//frc only
		prm_user->dpss_mode = DPSS_FRC_MODE;
		prm_user->dae_nr_mode = DAE_FRC_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_MC_MODE;
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 0;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_2;
	} else if (case_id == TST_CASE_IDX_0102) {	// SRC0 NR_FRC
		prm_user->dpss_mode = DPSS_FRC_NR_MODE;
		prm_user->dae_nr_mode = DAE_NR_MODE;
		prm_user->dpe_nr_mode = DPE_NR_MODE;	// ary 0113 DPE_NR_BYPS; // DPE_MC_MODE
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 0;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;

	} else if (case_id == TST_CASE_IDX_0107) {	//src0 nr + di
		prm_user->dpss_mode = DPSS_NRDI_SRC0_MODE;
		prm_user->dae_nr_mode = DAE_VDI_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_NRDI_MODE;
		prm_user->dpe_di_mode = DPE_NRDI_MODE;
		prm_user->src_mode = 0;
		///prm_user->cfg_slc    = 0;//1;
		dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;
	} else if (case_id == TST_CASE_IDX_0157) {	// SRC0 NR DI FRC
		prm_user->dpss_mode = DPSS_NRDI_FRC_SRC0_MODE;
		prm_user->dae_nr_mode = DAE_VDI_MODE;
		prm_user->dpe_nr_mode = DPE_NRDI_MODE;
		prm_user->dpe_di_mode = DPE_NRDI_MODE;
		prm_user->src_mode = 0;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;
	} else if (case_id == TST_CASE_IDX_1000) {
		prm_user->dpss_mode = DPSS_NR_SRC0_MODE;
		prm_user->dae_nr_mode = DAE_NR_MODE;	//no use;
		prm_user->dpe_nr_mode = DPE_NR_MODE;	//no use;
		prm_user->dpe_di_mode = DPE_VDI_MODE;	//no use;
		prm_user->src_mode = 0;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_TBC_1000;	//check
		dbg_i1("%s:case _1000\n", __func__);
	} else if (case_id == TST_CASE_IDX_1001) {
		prm_user->dpss_mode = DPSS_DI_MODE;
		prm_user->dae_nr_mode = DAE_VDI_MODE;	//no use;
		prm_user->dpe_nr_mode = DPE_VDI_MODE;	//no use;
		prm_user->dpe_di_mode = DPE_VDI_MODE;	//no use;
		prm_user->src_mode = 1;
		//prm_user->cfg_slc     = 0;//1;
		dpss_int = IRQ_MODE_CASE_TBC_1000;	//check
		dbg_i1("%s:case _1001:src1:di only\n", __func__);
	}

	if (!pch->c.ch &&
	    ((vfs->duration_overflow &&
	    !vfs->is_i) || (vfs->dpss_flag & DPSS_FLG_HIGH_BANDWIDTH) ||
			dpss_force_bypss)) {
		prm_user->dae_nr_mode = DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_NR_BYPS;
		pch->d->state_nr_bypass = true;
		DBG_INF("bypass nr\n");
	} else {
		pch->d->state_nr_bypass = false;
	}
#ifdef _HIS_CODE_	//1002 move out:
	if (vfs->width > DPSS_SLC_THD2_M)
		prm_user->cfg_slc = 1;
	else
		prm_user->cfg_slc = 0;
#endif
	//force:
	if (dpss_f_mode_get(0, &val))
		prm_user->dpss_mode = val;
	if (dpss_f_mode_get(1, &val))
		prm_user->dae_nr_mode = val;
	if (dpss_f_mode_get(2, &val))
		prm_user->dpe_nr_mode = val;
	if (dpss_f_mode_get(3, &val))
		prm_user->dpe_di_mode = val;
	if (dpss_f_mode_get(4, &val))
		prm_user->src_mode = val;
#ifdef _HIS_CODE_	//1002 move out
	if (dpss_f_mode_get(5, &val))
		prm_user->cfg_slc = val;
#endif
	dbg_i1("%s:%d,%d,%d,%d,%d\n",
	       __func__, prm_user->dpss_mode,
	       prm_user->dae_nr_mode,
	       prm_user->dpe_nr_mode,
	       prm_user->dpe_di_mode, prm_user->src_mode);
}

void fpga_ucode_1217(struct PRM_DPSS_TOP *prm_top, unsigned int cfg0)
{
	unsigned int tmp;

	if (cfg0 & C_BIT0)
		prm_top->afrc_cmp_en = 1;
	prm_top->pvsync_intr_en = 1;

	if (cfg0 & C_BIT1)
		prm_top->dae_step_sel = 2;
	prm_top->cut_win_en = 0;	//todo add disp blend option zeji
	if (cfg0 & C_BIT2)
		prm_top->fw_en = 1;
	// prm_top->badedit_en = 0;
	if (cfg0 & C_BIT3)
		prm_top->inp_done_int = 1;
	if (cfg0 & C_BIT4)
		prm_top->disp_pat_en = 1;

	if (prm_top->dpss_mode == DPSS_FRC_MODE)
		prm_top->force_film_mode = 0;//0;
	else
		prm_top->force_film_mode = 1;//0;

	prm_top->nr_vpp_link_en = 0;
	prm_top->nr_inp_en = 0;
	if (cfg0 & C_BIT5)
		prm_top->sw_tbc_ctrl_en = 1;
	if (cfg0 & C_BIT6)
		prm_top->sw_buf_rls_en = 1;
	if (cfg0 & C_BIT7)
		prm_top->src_is_1080p_nods = 0;	//ary add for 1080p no dw input;
	//---------------------------------------
	//12-17 cp from fpga: dpss_hw_init
	if (prm_top->afrc_cmp_en == 1)
		prm_top->nro_fs_fmt.src_cmpr = CMPR_AFRC;
#ifdef MOV			//by ary.sui 2025-03-05
	else
		prm_top->nro_fs_fmt.src_cmpr = CMPR_UN;
#endif
	tmp = ((cfg0 & (C_BIT11 | C_BIT10)) >> 10) & 3;

	if (tmp) {
		if (tmp == 3) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsy_scale = 1;
		} else if (tmp == 1) {
			prm_top->dae_dsx_scale = 0;
			prm_top->dae_dsy_scale = 0;
		} else if (tmp == 2) {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 2;
		}
		dbg_i1("force dsx scale:%d\n", prm_top->dae_dsx_scale);
	}
	tmp = cfg0 & (C_BIT13 | C_BIT14 | C_BIT15);
	tmp = tmp >> 13;
	if (tmp)
		prm_top->frm_hsize_sel = tmp - 1;
#ifdef _HIS_CODE_
	prm_top->dpe_dw_dsx = prm_top->dae_dsx_scale;
	prm_top->dpe_dw_dsy = prm_top->dae_dsy_scale;
#endif
	if (cfg0 & C_BIT8)
		prm_top->film_hwfw_sel = 1;
	if (cfg0 & C_BIT16)
		prm_top->frc_ds_scale_en = 1;
	//---------------------------------------
	dbg_ins1("%s:%d\n", __func__, tmp);
	dbg_ins1("\t:cmp[%d], dae_step[%d], fw_en[%d]\n",
		 prm_top->afrc_cmp_en, prm_top->dae_step_sel, prm_top->fw_en);
	dbg_ins1("\t:inp_done[%d], pat_en[%d], nr_inp[%d]\n",
		 prm_top->inp_done_int, prm_top->disp_pat_en,
		 prm_top->nr_inp_en);
	dbg_ins1("\t:sw_tbc_ctrl_en[%d], sw_buf_rls_en[%d]\n",
		 prm_top->sw_tbc_ctrl_en, prm_top->sw_buf_rls_en);
	dbg_ins1("\t:src_is_1080p_nods[%d]\n",
		prm_top->src_is_1080p_nods);
	dbg_ins1("\t:frc_ds_scale_en[%d]\n",
		prm_top->frc_ds_scale_en);
	dbg_ins1("\t:frm_hsize_sel[%d]\n",
		prm_top->frm_hsize_sel);
	dbg_ins1("\t:ds_s<%d,%d>, dw_s<%d,%d>\n",
		prm_top->dae_dsx_scale, prm_top->dae_dsy_scale,
		prm_top->dpe_dw_dsx, prm_top->dpe_dw_dsy);
	dbg_ins1("\t:frc_fbuf_only[%d]\n",
		prm_top->frc_fbuf_only);
	dbg_ins1("\t:frc_vfcd_vfmt[%d]\n",
		prm_top->frc_vfcd_vfmt);
	dbg_ins1("\t:auto_alig_en[%d]\n",
		prm_top->auto_alig_en);
}

void nr_dpe_dw_cfg(struct PRM_DPSS_TOP *prm_top)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	u32 vout_fr = 0;

	if (!prm_top)
		return;

	if (prm_top->frm_hsize <= 1920)
		prm_top->dpe_dw_dsx    = 1;
	else
		prm_top->dpe_dw_dsx    = 2;
	if (prm_top->frm_vsize <= 1080)
		prm_top->dpe_dw_dsy    = 1;
	else
		prm_top->dpe_dw_dsy    = 2;

	if (vinfo)
		vout_fr = vinfo->sync_duration_num / vinfo->sync_duration_den;

	//if (vout_fr > 199 && prm_top->frm_vsize >= 1080) {
	//	prm_top->dpe_dw_dsx    = 2;
	//	prm_top->dpe_dw_dsy    = 2;
	//}

	if (prm_top->dpe_dw_dsy >= 2) //  v > h
		prm_top->dpe_dw_dsx = 2;
	if (prm_top->frc_dae_div4 == 1 && vout_fr > 199 && prm_top->frm_vsize >= 1080) {
		prm_top->dpe_dw_dsx = 2;
		prm_top->dpe_dw_dsy = 2;
		prm_top->dlg_stats = 1;
		//prm_top->frm_hsize = (prm_top->org_hsize + 15) & (~0xf);
		//prm_top->alig_hmode = 1;
		//if (prm_top->org_hsize != prm_top->frm_hsize)
		//	prm_top->nr_src_align = true;
		dbg_h2("adj_dlg:org_hsize=%d, frm_hsize=%d, align=%d\n",
			prm_top->org_hsize, prm_top->frm_hsize,
			prm_top->alig_hmode);
	}
	dbg_h2("%s:dpe_dw_dsx = %d,%d,%d, div4:%d\n", __func__, prm_top->dpe_dw_dsx,
		prm_top->dpe_dw_dsy, prm_top->frm_vsize, prm_top->frc_dae_div4);
}

void nr_dpe_pps_para(struct dpss_ch_s *pch,
	unsigned int width, unsigned int height, bool use_dw)
{
	struct AA_PPS_TOP_TYPE *pps;
	struct frc_chip_st *pchip_st = NULL;
	struct vinfo_s *vinfo = get_current_vinfo();

	pchip_st = dpss_get_frc_st();

	if (pch->c.ch == 0)
		pps = &g_nr_pps_cfg;
	else
		pps = &g_nr_pps_cfg2;
	pch->c.prm_top.ch = pch->c.ch;

	if (!pch->c.ch &&  pchip_st &&
	    pchip_st->chip == ID_T6X) {
		if (width <= 960 || height <= 540) {
			if (width <= 960)
				pch->c.prm_top.pps_dsx = 1;
			else
				pch->c.prm_top.pps_dsx = 0;
			if (height <= 540)
				pch->c.prm_top.pps_dsy = 1;
			else
				pch->c.prm_top.pps_dsy = 0;
			pch->c.prm_top.is_pps = 1;
			if ((width == 720  && height == 576) &&
				pch->d->is_i) {
				pch->c.prm_top.pps_dsx = 1;
				pch->c.prm_top.pps_dsy = 1;
			}
		} else {
			pch->c.prm_top.pps_dsx = 0;
			pch->c.prm_top.pps_dsy = 0;
			pch->c.prm_top.is_pps = 0;
		}
	} else {
		pch->c.prm_top.pps_dsx = 0;
		pch->c.prm_top.pps_dsy = 0;
		pch->c.prm_top.is_pps = 0;
	}
	if (pch->d->is_i && pch->c.prm_top.is_pps)
		pch->c.prm_top.is_di2pps = 1;
	else
		pch->c.prm_top.is_di2pps = 0;

	if (dpss_nr_debug || dpss_force_nr_debug) {
		pch->c.prm_top.is_pps = 0;
		pch->c.prm_top.is_di2pps = 0;
	}
	if (dpss_pps_check & C_BIT0) {
		pch->c.prm_top.is_pps = 1;
		pch->c.prm_top.is_di2pps = 1;
	} else if (dpss_pps_check & C_BIT1) {
		pch->c.prm_top.is_pps = 0;
		pch->c.prm_top.is_di2pps = 0;
		pch->c.prm_top.pps_dsx = 0;
		pch->c.prm_top.pps_dsy = 0;
	}
	if ((vinfo && vinfo->height == 1080 &&
		vinfo->width == 3840) || width >= 1920 || height >= 1080 ||
		(width % 2) || (height % 2) || use_dw) {
		pch->c.prm_top.pps_dsx = 0;
		pch->c.prm_top.pps_dsy = 0;
		pch->c.prm_top.is_pps = 0;
		pch->c.prm_top.is_di2pps = 0;
	}

	pps->pps_en = pch->c.prm_top.is_pps;
	pps->di2pps_en = pch->c.prm_top.is_di2pps;

	dbg_h2("%s:0is_pps dsx ch= %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		pch->c.prm_top.ch, pch->c.prm_top.pps_dsx,
		pch->c.prm_top.pps_dsy, width, height,
		pch->d->is_i, pch->c.prm_top.is_pps,
		pch->c.prm_top.is_di2pps, pps->pps_en, vinfo->height, use_dw);
}

void nr_dpe_pps_cfg(struct PRM_DPSS_TOP *prm_top, struct dpss_ch_s *pch,
		struct dpss_sub_vf_s *vfms)
{
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;
	bool use_dw = false;

	if (!prm_top)
		return;
	if (VFM_IS_I_SRC(vfms->type))
		pch->d->is_i = 1;
	else
		pch->d->is_i = 0;

	if (vfms->dpss_flag & DPSS_FLG_USE_DW || vfms->dpss_flag & DPSS_FLG_PPS_SKIP)
		use_dw = true;
	nr_dpe_pps_para(pch, vfms->width, vfms->height, use_dw);

	if (pch->c.prm_top.is_pps) {
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2); //open pps path
		if (pps_out_w) {
			prm_dpe->dpe_nr_size.pps_hsize = pps_out_w;
			prm_dpe->dpe_nr_size.pps_vsize = pps_out_h;
		} else {
			prm_dpe->dpe_nr_size.pps_hsize =
				vfms->width << pch->c.prm_top.pps_dsx;
			prm_dpe->dpe_nr_size.pps_vsize =
				vfms->height << pch->c.prm_top.pps_dsy;
		}
	} else {
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);
		prm_dpe->dpe_nr_size.pps_hsize = 0;
		prm_dpe->dpe_nr_size.pps_vsize = 0;
		pch->c.prm_top.pps_dsx = 0;
		pch->c.prm_top.pps_dsy = 0;
	}
	dbg_h2("%s:pps_dsx = %d,%d,%d,%d,%d,%d,%d\n", __func__,
		pch->c.prm_top.pps_dsx,
		pch->c.prm_top.pps_dsy, vfms->height, prm_top->frm_vsize,
		pch->c.prm_top.is_pps, pch->d->is_i, pch->c.prm_top.is_di2pps);
	dbg_i0("%s:is_pps ch=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps);
}

extern unsigned int fnr_dpe_obuf_rls_ini;
void _prm_top_init_vfm(struct dpss_ch_s *pch,
	struct PRM_DPSS_TOP *prm_top, struct dpss_sub_vf_s *vfms, bool is_ex_di)
{
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;

	if (!prm_top || !vfms) {
		DBG_ERR("%s is_ex_di:%d\n", __func__, is_ex_di);
		return;
	}

	prm_top->frm_hsize = vfms->width;
	prm_top->frm_vsize = vfms->height;
	prm_top->org_hsize = vfms->width;
	prm_top->org_vsize = vfms->height;

	prm_top->amdv_frm_hsize = vfms->width;
	prm_top->amdv_frm_vsize = vfms->height;
	prm_top->amdv_org_hsize = vfms->width;
	prm_top->amdv_org_vsize = vfms->height;

	if (VFM_IS_I_SRC(vfms->type)) {
		if (vfms->height % 4)
			prm_top->org_vsize = roundup(vfms->height, 4);
	} else {
		if (vfms->height % 2)
			prm_top->org_vsize = roundup(vfms->height, 2);
	}

	//if (!pch->d->is_afbcd)
	{
		prm_top->nr_src_canvas_hsize = vfms->canvas0_config[0].width;
		dbg_h2("1org_hsize=%d, vfms->height=%d_1\n", prm_top->org_hsize, vfms->height);

		if (prm_top->org_hsize > 1920) {
			prm_top->frm_hsize = (prm_top->org_hsize + 15) & (~0xf);
			prm_top->alig_hmode = 1;
			if (dpss_dbg_ds & 0x100)
				prm_top->alig_hmode = 0;
		} else if (prm_top->org_vsize > 1080) {
			prm_top->frm_hsize = (prm_top->org_hsize + 15) & (~0xf);
			prm_top->alig_hmode = 1;
			if (dpss_dbg_ds & 0x100)
				prm_top->alig_hmode = 0;
		} else {
			prm_top->frm_hsize = (prm_top->org_hsize + 7) & (~0x7);
			prm_top->alig_hmode = 0;
		}

		prm_top->frm_vsize = prm_top->org_vsize;
		dbg_h2("org_hsize=%d, org_vsize=%d_2\n", prm_top->org_hsize, prm_top->org_vsize);
		dbg_h2("frm_hsize=%d, frm_vsize=%d_3\n", prm_top->frm_hsize, prm_top->frm_vsize);
		if (prm_top->org_hsize != prm_top->frm_hsize)
			prm_top->nr_src_align = true;

		dbg_i0("align %d\n", prm_top->alig_hmode);
	}
#ifdef _HIS_CODE_ //move to above:
	if (prm_top->org_hsize > 1920)
		prm_top->frm_hsize = (prm_top->org_hsize + 15) & (~0xf);
	else
		prm_top->frm_hsize = (prm_top->org_hsize + 7) & (~0x7);
#endif
	if (prm_top->vds_4k1k_en) {
		prm_top->frm_vsize = vfms->height >> 1;
		prm_top->org_vsize   = vfms->height >> 1;
	}

	dbg_h1("%s: src_frm:w*h:%d*%d,4k1k_en=%d\n", __func__, prm_top->frm_hsize,
		prm_top->frm_vsize, prm_top->vds_4k1k_en);
	prm_top->src_h_buf_size = vfms->cvs_h;	//tmp for nv21/12
	dbg_h2("src_h_buf_size=%d\n", prm_top->src_h_buf_size);

	if (dpss_force_in_h) {
		DBG_INF("force h: %d -> %d\n", prm_top->frm_hsize,
			dpss_force_in_h);
		prm_top->frm_hsize = dpss_force_in_h;
	}
	if (dpss_force_in_v) {
		DBG_INF("force v: %d -> %d\n", prm_top->frm_vsize,
			dpss_force_in_v);
		prm_top->frm_vsize = dpss_force_in_v;
	}
	if (dpss_dbg_in_sel) {	//tmp
		prm_top->frm_hsize = vfms->canvas0_config[0].width;
		prm_top->frm_vsize = vfms->canvas0_config[0].height;
		dbg_h1("cfg cvs size:<%d,%d>\n", prm_top->frm_hsize,
		       prm_top->frm_vsize);
	}

	if (pch->d->is_i || pch->d->proc_as_i) {
		if (pch->d->en_di_src) {
			if (is_ex_di) {
				prm_top->frm_vsize = prm_top->frm_vsize / 2;
				prm_top->org_vsize = prm_top->org_vsize / 2;
			}
		} else {
			prm_top->frm_vsize = prm_top->frm_vsize / 2;
			prm_top->org_vsize = prm_top->org_vsize / 2;
		}
	}
	dbg_i2("is_ex_di = %d, frm_vsize:%d\n", is_ex_di, prm_top->frm_vsize);
	/*1080p dcntr must 2 slice*/
	if (prm_top->cfg_slc == 1) {
		if (prm_top->frm_hsize > 1920) {
			prm_top->slc_num = 4;
			prm_top->dpe_slc_num = 4;
		} else {
			prm_top->slc_num = 2;
			prm_top->dpe_slc_num = 2;
		}
	} else {
		prm_top->slc_num = 1;
		prm_top->dpe_slc_num = 1;
	}

	if (prm_top->fmt_444_10) {
		prm_top->slc_num = 4;
		prm_top->dpe_slc_num = 4;
	}

	if (pch->c.prm_top.is_pps) {
		/*ramsize max=768, if slice num =2 && 32align, hsize max=1408*/
		/* (H+3) / 4 + 31 < 384, for frm_hsize_t1 < 768*/
		if (prm_top->frm_hsize > 1408) {
			prm_top->slc_num = 4;
			prm_top->dpe_slc_num = 4;
		} else if (prm_top->frm_hsize > 768) {
			prm_top->slc_num = 2;
			prm_top->dpe_slc_num = 2;
		} else {
			prm_top->slc_num = 1;
			prm_top->dpe_slc_num = 1;
		}
	}

	if (dpss_dbg_top_cfg0 & C_BIT9)
		prm_top->dpe_slc_num = 2;

	if (test_slice) {
		prm_top->slc_num = test_slice;
		prm_top->dpe_slc_num = test_slice;
	}

	if (pch->c.prm_top.is_pps)
		prm_top->nr_aapps_up = 1;
	else
		prm_top->nr_aapps_up = 0;

	if (pch->d->nr_cp_mode)
		prm_top->amdv_slc_num = prm_top->dpe_slc_num;
	dbg_i0("%s:slc_num = %d\n", __func__, prm_top->dpe_slc_num);
	dbg_i0("%s:is_pps ch=%d,=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);

	if (prm_top->dpe_slc_num == 1)
		prm_top->frm_hsize_sel = 0;
	else if (prm_top->dpe_slc_num == 2)
		prm_top->frm_hsize_sel = 2;
	else if (prm_top->dpe_slc_num == 4)
		prm_top->frm_hsize_sel = 3;

	if (pch->c.case_id == TST_CASE_IDX_0000) {
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 0;
		//      prm_top->dpe_nr_mode
	} else if (pch->c.case_id == TST_CASE_IDX_0001 ||
		   pch->c.case_id == TST_CASE_IDX_SRC1_NR ||
		   pch->c.case_id == TST_CASE_IDX_SRC1_NRDI) {
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 0;
	} else if (pch->c.case_id == TST_CASE_IDX_0002) {
		// prm_top->frc_ratio	   = DPSS_FRC_RATIO_1_2;
		if (prm_top->frm_hsize <= 1920 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 1920 * 1080)) {
			prm_top->dae_dsx_scale  = 1;
			prm_top->dae_dsy_scale  = 1;
			prm_top->dpe_dw_dsx		= 1;
			prm_top->dpe_dw_dsy		= 1;
			prm_top->mvx_div_mode	= 1;
			prm_top->mvy_div_mode	= 1;
			prm_top->dpe_slc_num	= 2;
			prm_top->alig_hmode     = 0;
			prm_top->alig_vmode     = 0;
			dbg_i2("input is FHD\n");
		} else if (prm_top->frm_hsize <= 3840 &&
				(prm_top->frm_hsize * prm_top->frm_vsize <= 3840 * 1080)) {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 1;
			prm_top->dpe_dw_dsx    = 2;
			prm_top->dpe_dw_dsy    = 1;
			prm_top->mvx_div_mode  = 2;
			prm_top->mvy_div_mode  = 1;
			prm_top->dpe_slc_num   = 4;
			prm_top->alig_hmode    = 1;
			prm_top->alig_vmode    = 0;
			dbg_i2("input is 4K1k\n");
		} else {
			prm_top->dae_dsx_scale  = 2;
			prm_top->dae_dsy_scale  = 2;
			prm_top->dpe_dw_dsx		= 2;
			prm_top->dpe_dw_dsy		= 2;
			prm_top->mvx_div_mode	= 2;
			prm_top->mvy_div_mode	= 2;
			prm_top->dpe_slc_num	= 4;
			prm_top->alig_hmode     = 1;
			prm_top->alig_vmode     = 1;
			dbg_i2("input is UHD\n");
		}
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 1;
		prm_top->frc_en = true;
		if (prm_top->frc_ds_scale_en == 1) {
			prm_top->dpe_mc_size.frm_hsize =
				prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.frm_vsize =
				prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dpe_mc_size.src_hsize =
				prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.src_vsize =
				prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dae_dsx_scale  = 1;
			prm_top->dae_dsy_scale  = 1;
			prm_top->auto_alig_en	= 1;
			prm_top->dpe_dw_dsx		= 1;
			prm_top->dpe_dw_dsy		= 1;
			prm_top->mvx_div_mode	= 1;
			prm_top->mvy_div_mode	= 1;
			prm_top->dpe_slc_num	= 2;
			prm_top->alig_hmode     = 0;
			prm_top->alig_vmode     = 0;
			dbg_i2("dpe_mc_size frm(%d,%d),src(%d,%d)\n",
				prm_top->dpe_mc_size.frm_hsize,
				prm_top->dpe_mc_size.frm_vsize,
				prm_top->dpe_mc_size.src_hsize,
				prm_top->dpe_mc_size.src_vsize);
			// prm_top->frc_no_ds = 1;
		}
		//w_reg_bit(VPU_AXIRD_PATH_CTRL,0,0,3);

		// test game mode
		prm_top->game_mode_n2m = dpss_dbg_game_mode;
		// test soft contrl case
		prm_top->sw_buf_rls_en = 0;
		prm_top->sw_tbc_ctrl_en = dpss_dbg_sw_mode;
		prm_top->inp_done_int = 1;
		prm_top->me_mc_link_en = dpss_dbg_game_mode;

		if (prm_top->game_mode_n2m | prm_top->game_mode_film)
			prm_top->dpe_game_mode = 1;
		else
			prm_top->dpe_game_mode = 0;
		fnr_dpe_obuf_rls_ini = 0;
		if (prm_top->frc_fbuf_only == 1) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsx_scale = 1;
		}
	} else if (pch->c.case_id == TST_CASE_IDX_0102) {
		// cfg_dpss_dae_me_rand_seed(0, 0, 0, 0);	// ME SEED
		// prm_top->frc_ratio = DPSS_FRC_RATIO_1_1; //DPSS_FRC_RATIO_1_2;
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 0;
		prm_top->frc_en = true;
		if (prm_top->frc_ds_scale_en == 1) {
			prm_top->dpe_mc_size.frm_hsize =
			    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.frm_vsize =
			    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dpe_mc_size.src_hsize =
			    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.src_vsize =
			    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
		}
		if (pch->c.prm_top.is_pps) {
			if (pps_out_w) {
				prm_top->dpe_mc_size.frm_hsize = pps_out_w;
				prm_top->dpe_mc_size.frm_vsize = pps_out_h;
				prm_top->dpe_mc_size.src_hsize = pps_out_w;
				prm_top->dpe_mc_size.src_vsize = pps_out_h;
			} else {
				prm_top->dpe_mc_size.frm_hsize = prm_dpe->dpe_nr_size.pps_hsize;
				prm_top->dpe_mc_size.frm_vsize = prm_dpe->dpe_nr_size.pps_vsize;
				prm_top->dpe_mc_size.src_hsize = prm_dpe->dpe_nr_size.pps_hsize;
				prm_top->dpe_mc_size.src_vsize = prm_dpe->dpe_nr_size.pps_vsize;
			}
			dbg_i2("frc_pps frm(%d,%d),src(%d,%d)\n",
				prm_top->dpe_mc_size.frm_hsize,
				prm_top->dpe_mc_size.frm_vsize,
				prm_top->dpe_mc_size.src_hsize,
				prm_top->dpe_mc_size.src_vsize);
		}

		//w_reg_bit(VPU_AXIRD_PATH_CTRL,0,0,3);

		// test game mode
		prm_top->game_mode_n2m = dpss_dbg_game_mode;
		// test soft contrl case
		prm_top->sw_buf_rls_en = 0;
		prm_top->sw_tbc_ctrl_en = dpss_dbg_sw_mode;
		prm_top->inp_done_int = 1;
		prm_top->me_mc_link_en = dpss_dbg_game_mode;

		if (prm_top->game_mode_n2m | prm_top->game_mode_film)
			prm_top->dpe_game_mode = 1;
		else
			prm_top->dpe_game_mode = 0;
		fnr_dpe_obuf_rls_ini = 0;
		prm_top->mvx_div_mode   = prm_top->dae_dsx_scale;
		prm_top->mvy_div_mode   = prm_top->dae_dsy_scale;
#ifdef USE_FRC_LINK_CODE
		//prm_top->mvx_div_mode   = prm_top->dae_dsx_scale;
		//prm_top->mvy_div_mode   = prm_top->dae_dsy_scale;
		if (prm_top->frm_hsize <= 1920 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 1920 * 1080)) {
			dbg_i2("input is FHD\n");
		} else if (prm_top->frm_hsize <= 3840 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 3840 * 1080)) {
			dbg_i2("input is 4K1k\n");
		} else {
			dbg_i2("input is UHD\n");
		}
#endif
	} else if (pch->c.case_id == TST_CASE_IDX_0157) {	//ary need check
		cfg_dpss_dae_me_rand_seed(0, 0, 0, 0);	// ME SEED
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 1;
		prm_top->frc_en = true;
		prm_top->frc_ds_scale_en = 0;	//1;
		prm_top->inp_done_int = 1;
		if (pch->c.prm_top.is_di2pps) {
			if (pps_out_w) {
				prm_top->dpe_mc_size.frm_hsize = pps_out_w;
				prm_top->dpe_mc_size.frm_vsize = pps_out_h;
				prm_top->dpe_mc_size.src_hsize = pps_out_w;
				prm_top->dpe_mc_size.src_vsize = pps_out_h;
			} else {
				prm_top->dpe_mc_size.frm_hsize = prm_dpe->dpe_nr_size.pps_hsize;
				prm_top->dpe_mc_size.frm_vsize = prm_dpe->dpe_nr_size.pps_vsize;
				prm_top->dpe_mc_size.src_hsize = prm_dpe->dpe_nr_size.pps_hsize;
				prm_top->dpe_mc_size.src_vsize = prm_dpe->dpe_nr_size.pps_vsize;
			}
		}
		dbg_i2("frc_pps[%d] frm(%d,%d),src(%d,%d)\n",
				pch->c.case_id,
				prm_top->dpe_mc_size.frm_hsize,
				prm_top->dpe_mc_size.frm_vsize,
				prm_top->dpe_mc_size.src_hsize,
				prm_top->dpe_mc_size.src_vsize);
#ifdef MOV
		prm_top->dpe_mc_size.frm_hsize =
		    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
		prm_top->dpe_mc_size.frm_vsize =
		    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
		prm_top->dpe_mc_size.src_hsize =
		    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
		prm_top->dpe_mc_size.src_vsize =
		    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
#endif
	} else if (pch->c.case_id == TST_CASE_IDX_1000) {
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
		prm_top->sw_tbc_mode = 1;
		if (dpss_dv_en & C_BIT16) {	//tmp
			prm_top->wait_en = true;
			dbg_i0("wait_en:%d", prm_top->wait_en);
		}
	} else if (pch->c.case_id == TST_CASE_IDX_1001) {
		prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
		prm_top->sw_tbc_mode = 1;
	}
	if (pch->c.ch == 0) {
		dpss_nr1 = &pch->d->nr_i[0];
		dpss_idx = &pch->d->h_dae_vf_idx[0];
	}
	if (dpss_dv_en & C_BIT0 || pch->d->is_dd) {
		prm_top->dolby_mode = DOLBY_DPSS_MODE;
		if (pch->d->nr_cp_mode)
			prm_top->dolby_mode = DOLBY_DPSS_PRL_MODE;
		DBG_INF("dd:%s:dolby_mode = %d\n", __func__, prm_top->dolby_mode);
	}

	if (dpss_dbg_ratio != -1) {
		DBG_INF("%s:ratio force %d -> %d", __func__,
			prm_top->frc_ratio, dpss_dbg_ratio);
		prm_top->frc_ratio = (unsigned int)dpss_dbg_ratio;
	}
	if (pch->c.case_id == TST_CASE_IDX_0002 ||
	    pch->c.case_id == TST_CASE_IDX_0102 ||
	    pch->c.case_id == TST_CASE_IDX_0157) {
		pch->d->frc_link = true;
	}
	nr_dpe_dw_cfg(prm_top);
	dbg_i2("frc link = %d\n", pch->d->frc_link);
	dbg_h0("dv slic :%d dv pad :%d\n", prm_top->amdv_slc_num, prm_top->amdv_pad);
	dbg_h0("dv_frm_size <%d %d> dv org size <%d %d>\n", prm_top->amdv_frm_hsize,
		prm_top->amdv_frm_vsize, prm_top->amdv_org_hsize, prm_top->amdv_org_vsize);
}

//use this for auto mode only:
void _prm_top_input_buffer_set(struct PRM_DPSS_TOP *prm_top,
					unsigned int src,
					unsigned int idx,
					struct dpss_sub_vf_s *vfms)
{
	unsigned long laddr1, laddr2;
	unsigned int *addr_t1_src, *addr_t2_src, *addr_t3_dw, *addr_t4_dw;
	unsigned int reg_t1_src, reg_t2_src, reg_t3_dw, reg_t4_dw;

	if (src > 1) {
		DBG_WARN("%s:src fix to 0:%d\n", __func__, src);
		src = 0;
	}
	if (idx >= 16)
		idx = 0;
//for frc:
	if (prm_top->frc_en) {
		dae_rd_less_one = (rd(FRC_DPSS_LLM) >> 2) & 0x1;
		dbg_h2("%s:frc:dae_rd_less_one = %d\n", __func__,
			dae_rd_less_one);
	}

	if (src == 0) {
		addr_t1_src = &prm_top->src0_fbuf_yaddr[idx];
		addr_t2_src = &prm_top->src0_fbuf_caddr[idx];
		addr_t3_dw = &prm_top->src0_dwbuf_yaddr[idx];
		addr_t4_dw = &prm_top->src0_dwbuf_caddr[idx];
		reg_t1_src = DPSS_SRC0_FBUF_YADDR0 + idx;
		reg_t2_src  = DPSS_SRC0_FBUF_CADDR0 + idx;
		reg_t3_dw = DPSS_SRC0_DWBUF_YADDR0 + idx;
		reg_t4_dw = DPSS_SRC0_DWBUF_CADDR0 + idx;
	} else {
		addr_t1_src = &prm_top->src1_fbuf_yaddr[idx];
		addr_t2_src = &prm_top->src1_fbuf_caddr[idx];
		addr_t3_dw = &prm_top->src1_dwbuf_yaddr[idx];
		addr_t4_dw = &prm_top->src1_dwbuf_caddr[idx];
		reg_t1_src = DPSS_SRC1_FBUF_YADDR0 + idx;
		reg_t2_src  = DPSS_SRC1_FBUF_CADDR0 + idx;
		reg_t3_dw = DPSS_SRC1_DWBUF_YADDR0 + idx;
		reg_t4_dw = DPSS_SRC1_DWBUF_CADDR0 + idx;
	}
	laddr1 = vfms->canvas0_config[0].phy_addr;
	laddr2 = vfms->canvas0_config[1].phy_addr;

	*addr_t1_src = (unsigned int)(laddr1 >> 9);
	*addr_t2_src = (unsigned int)(laddr2 >> 9);
	*addr_t3_dw = *addr_t1_src;
	*addr_t4_dw = *addr_t2_src;
	wr(reg_t1_src, *addr_t1_src);
	wr(reg_t2_src, *addr_t2_src);
	wr(reg_t3_dw, *addr_t3_dw);
	wr(reg_t4_dw, *addr_t4_dw);
	dbg_h1("%s:idx:%d\n", __func__, idx);
	dbg_h1("\treg:addr_src:0x%x = 0x%x\n", reg_t1_src, rd(reg_t1_src));
	dbg_h1("\treg:addr_src:0x%x = 0x%x\n", reg_t2_src, rd(reg_t2_src));
	dbg_h1("\treg:addr_dw:0x%x = 0x%x\n", reg_t3_dw, rd(reg_t3_dw));
	dbg_h1("\treg:addr_dw:0x%x = 0x%x\n", reg_t4_dw, rd(reg_t4_dw));
}

void _prm_top_input_buffer_ds_set(struct PRM_DPSS_TOP *prm_top,
					unsigned int src,
					unsigned int idx,
					struct dpss_sub_vf_s *vfms)
{
	unsigned long laddr1, laddr2;
	unsigned int *addr_t3, *addr_t4;
	unsigned int reg_t3, reg_t4;

	if (src > 1) {
		DBG_WARN("%s:src fix to 0:%d\n", __func__, src);
		src = 0;
	}
	if (idx >= 16)
		idx = 0;

	if (src == 0) {
		addr_t3 = &prm_top->src0_dwbuf_yaddr[idx];
		addr_t4 = &prm_top->src0_dwbuf_caddr[idx];
		reg_t3 = DPSS_SRC0_DWBUF_YADDR0 + idx;
		reg_t4 = DPSS_SRC0_DWBUF_CADDR0 + idx;
	} else {
		addr_t3 = &prm_top->src1_dwbuf_yaddr[idx];
		addr_t4 = &prm_top->src1_dwbuf_caddr[idx];
		reg_t3 = DPSS_SRC1_DWBUF_YADDR0 + idx;
		reg_t4 = DPSS_SRC1_DWBUF_CADDR0 + idx;
	}
	laddr1 = vfms->canvas0_config[0].phy_addr;
	laddr2 = vfms->canvas0_config[1].phy_addr;

	*addr_t3 = (unsigned int)(laddr1 >> 9);
	*addr_t4 = (unsigned int)(laddr2 >> 9);

	wr(reg_t3, *addr_t3);
	wr(reg_t4, *addr_t4);
	dbg_h1("%s:idx:%d\n", __func__, idx);

	dbg_h2("\treg:addr:0x%x = 0x%x\n", reg_t3, rd(reg_t3));
	dbg_h2("\treg:addr:0x%x = 0x%x\n", reg_t4, rd(reg_t4));
}

void _prm_top_input_afbcd(struct PRM_DPSS_TOP *prm_top,
			  unsigned int src,
			  unsigned int idx,
			  u32 src_sel, struct dpss_sub_vf_s *vfms)
{
//	unsigned int reg_ofst = src_sel * 256;
	struct PRM_DPSS_TOP *prm_top_di = &prm_dpss_top_di;

	prm_top->src0_head_yaddr[idx] = vfms->compHeadAddr >> 9;
	prm_top->src0_head_caddr[idx] = prm_top->src0_head_yaddr[idx];
	prm_top->src0_fbuf_yaddr[idx] = vfms->compBodyAddr >> 9;
	dbg_h2("src[%d], afbcd: addr <0x%lx, 0x%x> <0x%lx, 0x%x>\n",
	       src,
	       vfms->compHeadAddr, prm_top->src0_head_yaddr[idx],
	       vfms->compBodyAddr, prm_top->src0_fbuf_yaddr[idx]);
	if (!prm_top->src_mode) {
		prm_top_di->src0_head_yaddr[idx] = prm_top->src0_head_yaddr[idx];
		prm_top_di->src0_head_caddr[idx] = prm_top->src0_head_caddr[idx];
		prm_top_di->src0_fbuf_yaddr[idx] = prm_top->src0_fbuf_yaddr[idx];
	}
#ifdef _HIS_CODE_
	//ary to-do:if multi frame input, err
	wr_vc(reg_ofst + VFCD_AFBC_HEAD_BADDR, prm_top->src0_head_yaddr[idx] << 5);
	wr_vc(reg_ofst + VFCD_AFBC_BODY_BADDR, prm_top->src0_fbuf_yaddr[idx] << 5);
#endif
	_prm_top_input_buffer_ds_set(prm_top, src, idx, vfms);
}

//called after buffer is alloced and have input vfm
//debug only
void hw_init_prm(struct dpss_ch_s *pch, struct PRM_DPSS_TOP *prm_top,
	unsigned int src, struct dpss_sub_vf_s *vfms)
{
	struct dpss_user_para_s *prm_user, user_d;
	unsigned int src_in;
	unsigned int val;

	dpss_h_val_init(pch);
	hw_val_init(pch->c.ch);
	//count
	prm_user = &user_d;
	memset(prm_user, 0, sizeof(*prm_user));
	dpss_val_user(pch, prm_user, vfms);

	//1002 move from val_user:
	if (vfms->width > DPSS_SLC_THD2_M)
		prm_user->cfg_slc = 1;
	else
		prm_user->cfg_slc = 0;
	if (dpss_f_mode_get(5, &val))
		prm_user->cfg_slc = val;
	dbg_i1("cfg_slc=%d\n", prm_user->cfg_slc);

	if (VFM_IS_I_SRC(vfms->type)) {
		pch->d->is_i = true;
		if (vfms->type & VIDTYPE_VIU_FIELD)
			pch->d->is_field = true;
		else
			pch->d->is_field = false;	// need rd skip line
		if (D_COM_ME(vfms->type, VIDTYPE_INTERLACE_BOTTOM))
			pch->d->is_top_first = false;
		else
			pch->d->is_top_first = true;

	} else {
		pch->d->is_i = false;
	}
	DBG_INF("%s:is_i:%d %d %d\n", __func__,
		pch->d->is_i, pch->d->is_field, pch->d->is_top_first);
	src_in = prm_user->src_mode;

	//------
	_prm_top_init(pch, prm_top, src_in, false, NULL); //path id 0 is src 0
	_prm_top_init_light(pch, prm_top, src_in, false, NULL);
	_prm_top_init_buffer(prm_top, pch, src_in);
	hw_init_small_addr(prm_top, src_in);
	_prm_top_init_vfm(pch, prm_top, vfms, false);
}

void nr_unreg_hw(struct dpss_ch_s *pch)
{
	dpss_hw_disable_one_play(pch);
	if (!dpss_get_hw()->src_act &&
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W) {
		wr(VPU_DAE_WRAP_GCLK_CTRL0, 0x0);
		wr(VPU_DAE_WRAP_GCLK_CTRL1, 0x0);
	}
}

void nr_unreg_val(struct dpss_ch_s *pch)
{
//	dpss_inp_frm_cnt = 0;
	if (pch->c.ch == 0) {
		dpss_inp_frm_cnt = 0; //2026-02-05
		dpss_get_hw()->ch_act &= ~C_BIT0;
		dpss_get_hw()->st_idle |= C_BIT0;
		dpss_get_hw()->src_act &= ~C_BIT0;
	} else {//if (pch->c.ch == 1) {
		dpss_get_hw()->ch_act &= ~C_BIT1;
		dpss_get_hw()->st_idle |= C_BIT1;
		dpss_get_hw()->src_act &= ~C_BIT1;
	}
	pch->c.case_id = 0;
	dbg_i2("%s:ch[%d]:act=0x%x,idle=0x%x, hold=0x%x\n", __func__,
		pch->c.ch,
		dpss_get_hw()->ch_act,
		dpss_get_hw()->st_idle,
		dpss_get_hw()->need_hold);
}

void nr_lcevc_update_addr(struct dpss_ch_s *pch, struct vframe_s *vf,
			unsigned int idx,
			struct PRM_DPSS_TOP *prm_top)
{
	struct vframe_s *vf_m, *vf_s;
//	struct PRM_DPSS_LCEVC *lv;
//	unsigned int src = 0;

	if (!dpss_en_lcevc)
		return;

	if (!(vf->type_ext & VIDTYPE_EXT_LCEVC) ||
	    !vf->enhance_vf) {
		DBG_WARN("not lcevc?0x%x:0x%x\n", vf->type_ext, vf->ext_signal_type);
		return;
	}

	vf_s = vf;  //lum
	vf_m = vf->enhance_vf; //afbcd

	unsigned int reg_ofst = 0; //0src_sel * 256;

	prm_top->src0_head_yaddr[idx] = vf_m->compHeadAddr >> 9;
	prm_top->src0_head_caddr[idx] = prm_top->src0_head_yaddr[idx];
	prm_top->src0_fbuf_yaddr[idx] = vf_m->compBodyAddr >> 9;
	dbg_h2("src[0], afbcd: addr <0x%lx, 0x%x> <0x%lx, 0x%x> %p\n",
		vf_m->compHeadAddr, prm_top->src0_head_yaddr[idx],
		vf_m->compBodyAddr, prm_top->src0_fbuf_yaddr[idx],
		vf);
	prm_top->lcevc.src1_head_ybaddr = prm_top->src0_head_yaddr[idx] << 5;
	//ary to-do:if multi frame input, err
	wr_vc(reg_ofst + VFCD_AFBC_HEAD_BADDR,  prm_top->src0_head_yaddr[idx] << 5);//for luma_addr
	wr_vc(reg_ofst + VFCD_AFBC_BODY_BADDR, prm_top->src0_fbuf_yaddr[idx] << 5);//for chrm_addr

	// sub:-----------------------------------
	unsigned long laddr1, laddr2;
	unsigned int *addr_t1, *addr_t2, *addr_t3, *addr_t4;
	unsigned int reg_t1, reg_t2, reg_t3, reg_t4;

//	if (src >= 1) {
//		DBG_WARN("%s:src fix to 0:%d\n", __func__, src);
//		src = 0;
//	}
	if (idx >= 16) {
		DBG_WARN("%s:%d\n", __func__, idx);
		idx = 0;
	}

	addr_t1 = &prm_top->src0_fbuf_yaddr[idx];
	addr_t2 = &prm_top->src0_fbuf_caddr[idx];
	addr_t3 = &prm_top->src0_dwbuf_yaddr[idx];
	addr_t4 = &prm_top->src0_dwbuf_caddr[idx];
	reg_t1 = DPSS_SRC0_FBUF_YADDR0 + idx;
	reg_t2	= DPSS_SRC0_FBUF_CADDR0 + idx;
	reg_t3 = DPSS_SRC0_DWBUF_YADDR0 + idx;
	reg_t4 = DPSS_SRC0_DWBUF_CADDR0 + idx;

	laddr1 = vf_s->canvas0_config[0].phy_addr;
	laddr2 = vf_s->canvas0_config[1].phy_addr;

	*addr_t1 = (unsigned int)(laddr1 >> 9);
	*addr_t2 = (unsigned int)(laddr2 >> 9);
	*addr_t3 = *addr_t1;
	*addr_t4 = *addr_t2;
	wr(reg_t1, *addr_t1);
	wr(reg_t2, *addr_t2);
	wr(reg_t3, *addr_t3);
	wr(reg_t4, *addr_t4);

	lcevc_update_addr((unsigned int)(laddr1 >> 4), (unsigned int)(laddr2 >> 4));

	dbg_h1("%s:idx:%d\n", __func__, idx);
	dbg_h1("\treg:addr:0x%x = 0x%x\n", reg_t1, rd(reg_t1));
	dbg_h1("\treg:addr:0x%x = 0x%x\n", reg_t2, rd(reg_t2));
	dbg_h1("\treg:addr:0x%x = 0x%x\n", reg_t3, rd(reg_t3));
	dbg_h1("\treg:addr:0x%x = 0x%x\n", reg_t4, rd(reg_t4));
}

unsigned int dpss_dbg_lcevc_cfg;
module_param_named(dpss_dbg_lcevc_cfg, dpss_dbg_lcevc_cfg, uint, 0664);

void nr_lcevc_vf_parser(struct dpss_ch_s *pch, struct vframe_s *vf,
			struct PRM_DPSS_TOP *prm_top)
{
	struct vframe_s *vf_m, *vf_s;
	struct PRM_DPSS_LCEVC *lv;

	if (!(dpss_en_lcevc & C_BIT1))
		return;

	if (!(vf->type_ext & VIDTYPE_EXT_LCEVC) ||
	    !vf->enhance_vf) {
		DBG_WARN("not lcevc?0x%x\n", vf->type_ext);
		return;
	}
	vf_s = vf;  //lum
	vf_m = vf->enhance_vf; //afbcd
	//
	prm_top->frm_hsize = vf_s->width;
	prm_top->frm_vsize = vf_s->height;
	prm_top->dpe_slc_num = 4;
	if (dpss_en_lcevc & C_BIT3) {
		prm_top->lcevc_en = 1;
		dbg_h2("%s:lcevc_en=%d\n", __func__, prm_top->lcevc_en);
	}

	lv = &prm_top->lcevc;
	lv->src1_frm_hsize = vf_m->compWidth;
	lv->src1_frm_vsize = vf_m->compHeight;
	lv->src1_is_cmpr = 1;
	lv->src1_bit = vf_m->bitdepth;
	lv->dbg_cfg = dpss_dbg_lcevc_cfg;
	lv->src2_frm_hsize = prm_top->frm_hsize;
	lv->src2_frm_vsize = prm_top->frm_vsize;
	lv->frm_hsize_out = prm_top->frm_hsize;
	lv->frm_vsize_out = lv->src2_frm_vsize;

//-----------------------------------
	//setting:
	//need check:
	//Wr_reg_bits(DPSS_DAE_TOP_ARB_MODE, 1, 0, 1);//arbiter mode
	//Wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88882808);//tbc_mux
//-----------------------------------
	dbg_i0("\ts:<%d,%d>\n", prm_top->frm_hsize, prm_top->frm_vsize);
	dbg_i0("\tm:<%d,%d>\n", lv->src1_frm_hsize, lv->src1_frm_vsize);
	dbg_i0("\tdbg=0x%x\n", lv->dbg_cfg);
}

unsigned int dpss_dbg_0709; //disable reset
module_param_named(dpss_dbg_0709, dpss_dbg_0709, uint, 0664);

unsigned int dpss_dbg_nr_sleep; //disable reset
module_param_named(dpss_dbg_nr_sleep, dpss_dbg_nr_sleep, uint, 0664);

void nr_only_int(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs,
		 struct vframe_s *vf)
{
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = &pch->c.prm_dae;//&prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;//&prm_dpss_dpe;

	struct PRM_DPSS_TOP *prm_top_di = &prm_dpss_top_di;
	struct PRM_DPSS_DAE *prm_dae_di = &prm_dpss_dae_di;
	struct PRM_DPSS_DPE *prm_dpe_di = &prm_dpss_dpe_di;
	struct dpss_user_para_s *prm_user, user_d;
	struct AA_PPS_TOP_TYPE *pps;
	unsigned char last_idx_done;
	bool light_chg = false;
	unsigned int val;
	unsigned int me_size_tmp;
	struct PRM_INTF_TYPE *dae_yuv;
	struct frc_chip_st *pchip_st = NULL;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct dpss_info_s dpss_info;
#endif /**/

#ifdef FUNC_EN_PQ
	const struct reg_acc *op = &t6w_dpss_reg_acc;
#endif /* FUNC_EN_PQ */
	unsigned int tmp;
	unsigned int src = 0;
	unsigned int src_w, src_h;
	u32 big_endian = 0, rev_x = 0, temp_uv;
	int i;
	bool is_4k, is_fhd_high_fps;
	//release: to-do;

	if (pch->d->nr_reset &&
	    !VFM_IS_I_SRC(vf->type) &&
	    (dpss_light_chg & C_BIT0) &&
		!VFM_IS_HDMI_SRC(vf->source_type) &&
		!pch->d->need_dis_light_chg) {
		//not support i / p
		//not support small window memc
		light_chg = true;
	}

	dbg_i1("%s: start init.light = %d source_type:%d vf_dpss_flg:0x%x\n",
		__func__, light_chg, vf->source_type, vf->dpss_flg);

	if (vf->dpss_flg & DPSS_FLG_HIGH_BANDWIDTH)
		vfs->dpss_flag |= DPSS_FLG_HIGH_BANDWIDTH;

	if (light_chg) {
		prm_top->trig_byp_dae0 = true;
		prm_top->trig_byp_mc = true;
		dbg_h2("%s trig byp dae0 and mc\n", __func__);
		if (dpss_dbg_nr_sleep) {
			usleep_range(dpss_dbg_nr_sleep, dpss_dbg_nr_sleep + 1000);
			dbg_h2("%s sleep done\n", __func__);
		}
	} else {
		prm_top->reset_path = true;
		dbg_h2("%s do reset path\n", __func__);
	}

	if (VFM_IS_HDMI_SRC(vf->source_type))
		prm_top->is_hdmi_src = true;
	else
		prm_top->is_hdmi_src = false;

	if (!light_chg)
		dpss_h_val_init(pch);
	if (dpss_slt_mode) {
		dbg_dpss_reset1(0);
		dpss_crc_init();
	}

	if (dpss_is_h_first_ch(pch) && !light_chg) {
		dbg_dpss_reset(dpss_reset_ctrl);
		hw_init_part1(vf);
		if (VFM_IS_I_SRC(vfs->type))
			pch->d->nr_reset = false;
		else
			pch->d->nr_reset = true;
	}
	if (pch->c.ch == 0) {
		dpss_get_hw()->src_act |= C_BIT0;
		pps = &g_nr_pps_cfg;
	} else {
		dpss_get_hw()->src_act |= C_BIT1;
		pps = &g_nr_pps_cfg2;
	}
	if (!light_chg) {
		pch->c.in_cnt = 0;
		pch->c.out_cnt = 0;
		pch->c.disp_cnt = 0;
	}

	src_w = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;
	src_h = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;

	is_4k = (src_w > 1920 || src_h > 1088);

	is_fhd_high_fps = (src_w == 1920 &&
		vf->duration <= 960 &&
		vf->duration > 0 &&
		!(vf->type & VIDTYPE_INTERLACE));

	if (is_4k || is_fhd_high_fps || (dpss_en_afbc & C_BIT1))
		pch->c.o_afbc = 2;
	else
		pch->c.o_afbc = 0;

	if ((dpss_en_afbc_force & 0xff) && pch->c.ch == 0) {
		if (dpss_en_afbc_force & C_BIT0)
			pch->c.o_afbc = 1;
		else if (dpss_en_afbc_force & C_BIT1)
			pch->c.o_afbc = 2;
		else
			pch->c.o_afbc = 0;
	} else if ((dpss_en_afbc_force & 0xff00) && pch->c.ch == 1) {
		if (dpss_en_afbc_force & C_BIT8)
			pch->c.o_afbc = 1;
		else if (dpss_en_afbc_force & C_BIT9)
			pch->c.o_afbc = 2;
		else if (dpss_en_afbc_force & C_BIT10)
			pch->c.o_afbc = 0;
	}

	if (dpss_nr_debug == 1)
		pch->c.o_afbc = 0;

	dbg_i1("%s:is_4k: %d, o_afrc: %d, afrc_bs : %d.%d\n",
		__func__,
		is_4k,
		pch->c.o_afbc,
		pch->d->afrc_bs_y,
		pch->d->afrc_bs_c);

	if (!light_chg) {
		last_idx_done = pch->d->idx_done;
		pch->d->idx_start = pch->d->idx_start + pch->d->idx_done + 1;
		pch->d->idx_done = 0;
		if (pch->d->idx_hd)
			pch->d->idx_hd = 0;
		else
			pch->d->idx_hd = 1;
		dbg_pps0("ch[%d] 2 idx_start = %d done = %d\n",
		pch->c.ch, pch->d->idx_start,
		last_idx_done);
		//tmp here:
		for (i = 0; i < DPSS_VFM_IN_NUB; i++)
			pch->d->h_dae_vf_idx[i] = 0xff;
		dbg_h2("%s:dae_vf:clean\n", __func__);
	} else {
		prm_top->idx_done = pch->d->idx_done;
		dbg_h2("%s size chg buf idx:%d\n", __func__, pch->d->idx_done);
	}

	//..
	if (pch->c.ch)
		src = 1;
	else
		src = 0;
	if (!src) {
		if (!light_chg) {
			dpss_inp_frm_cnt = 0;
			dpss_dae_frm_cnt_src0 = 0;
			dpss_dae_frm_cnt_src1 = 0;
			dpss_dpe_nr_frm_cnt = 0;
			src0_frm_idx_cnt = 0;
			// ini_cfg_frc_dae = 0;
			frc_fst_frm = 0;
			ini_cfg_dae_in_nr_frc = 0;
			frc_fst_frm_in_nr = 1;
		}

		ini_cfg_frc_dae = 0; // initial bbd size
	} else {
		dpss_dae_frm_cnt_src2 = 0;
		dpss_dpe_di_frm_cnt = 0;
	}
	if (!light_chg) {//1005
		prm_top->rls0_irq_count = 0;
		prm_top->is_dpss_init_done = 0;
		pch->c.cnt_in = 0;
		pch->c.cnt_proce = 0;

		hw_val_init(pch->c.ch);
	}
	//count
	prm_user = &user_d;
	memset(prm_user, 0, sizeof(*prm_user));

	if (!light_chg)
		dpss_val_user(pch, prm_user, vfs);
	//1002 move from val_user:
	if (vfs->width > DPSS_SLC_THD2_M)
		prm_user->cfg_slc = 1;
	else
		prm_user->cfg_slc = 0;
	if (dpss_f_mode_get(5, &val))
		prm_user->cfg_slc = val;
	dbg_i1("cfg_slc=%d\n", prm_user->cfg_slc);

	if (VFM_IS_I_SRC(vfs->type)) {
		pch->d->is_i = true;
		prm_top->is_i = true;
		if (vfs->type & VIDTYPE_VIU_FIELD)
			pch->d->is_field = true;
		else
			pch->d->is_field = false;	// need rd skip line
		if (D_COM_ME(vfs->type, VIDTYPE_INTERLACE_BOTTOM))
			pch->d->is_top_first = false;
		else
			pch->d->is_top_first = true;
		if (dpss_nr_debug) {
			dpss_nr_debug = 2;
			//dpss_force_nr_debug = 1;
		}
	} else {
		pch->d->is_i = false;
		prm_top->is_i = false;
		if (dpss_nr_debug) {
			//dpss_force_nr_debug = 0;
			dpss_nr_debug = 1;
		}
	}

	dbg_i1("%s:is_i:%d %d top:%d:\n", __func__,
		pch->d->is_i,
		pch->d->is_field,
		pch->d->is_top_first);
	if (VFM_IS_COMP_MODE(vfs->type)) {
		pch->d->is_afbcd = true;
		prm_top->is_afbcd = true;
		prm_top->afbcd_update = 1;
		if (pch->d->is_i)
			prm_top->afbcd_update = 2;
		prm_top->no_ds = false;
	} else {
		pch->d->is_afbcd = false;
		prm_top->is_afbcd = false;
		prm_top->afbcd_update = 0;
		prm_top->no_ds = true;
	}

	pch->d->is_10bit = false;
	if (vfs->bitdepth & BITDEPTH_Y10)
		pch->d->is_10bit = true;
	dbg_h2("%s: vfs->bitdepth %d, is_10bit=%d.\n", __func__, vfs->bitdepth, pch->d->is_10bit);

	if (_IS_VDIN_SRC(vfs->source_type))
		prm_top->force_dos_mode = 0;
	else
		prm_top->force_dos_mode = 1;

	if (vfs->plane_num == 1)
		pch->d->plane = PLANAR_X1;
	else if (vfs->plane_num == 2)
		pch->d->plane = PLANAR_X2;
	else
		pch->d->plane = PLANAR_X3;
	dbg_h2("vfm plane:%d vfs->type 0x%x,vf->flag 0x%x.\n", vfs->plane_num,
			vfs->type, vf->flag);

	dae_yuv = &prm_dae->nr_yuv_intf;

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR) {
		prm_top->l_endian = 1;
		prm_top->swap_64bit = 0;
		dae_yuv->swap_64bit = 0;
		dae_yuv->little_endian = 1;
	} else {
		prm_top->l_endian = 0;
		prm_top->swap_64bit = 1;
		dae_yuv->swap_64bit = 1;
		dae_yuv->little_endian = 0;
	}
	prm_top->block_mode = vf->canvas0_config[0].block_mode;
	dbg_h2("vfm flag:%d vfs->endian 0x%x\n", vf->flag, vf->canvas0_config[0].endian);
	dbg_h2("vfm block mode %d,%d\n", vf->canvas0_config[0].block_mode, dae_yuv->little_endian);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_frame(vf) && is_amdv_enable()) {
		prm_top->is_amdv_frame = true;
		prm_top->amdv_2_frc_frm = is_amdv_frame(vf);
	} else {
		prm_top->is_amdv_frame = false;
	}
	if (prm_top->is_amdv_frame && (vf->type & VIDTYPE_VIU_444) &&
		_IS_VDIN_SRC(vfs->source_type))
		dpss_en_dct = 0;
	else
		dpss_en_dct = 1;
#endif
	if (vfs->type & VIDTYPE_COMB_MODE)
		pch->d->fmt444_comb = 1;
	else
		pch->d->fmt444_comb = 0;
	if (vfs->type & VIDTYPE_VIU_422) {
		if (dpss_o_42210_2p == 2)
			prm_top->out_mode = OUT_YUV422_2_PLANE;
		else
			prm_top->out_mode = OUT_YUV422_1_PLANE;
	} else if (vfs->type & VIDTYPE_VIU_NV21 ||
		vfs->type & VIDTYPE_VIU_NV12) {
		prm_top->out_mode = OUT_YUV420_NV21;
	} else {
		prm_top->out_mode = OUT_YUV422_1_PLANE;
	}
	if (dpss_o_mode)
		prm_top->out_mode = dpss_o_mode;
	if (dpss_o_dw == 1)
		prm_top->out_mode = OUT_YUV422_1_PLANE;
	dbg_h2("%s:is_afbcd:%d %d,vfs->type=0x%x\n", __func__,
			pch->d->is_afbcd,
			pch->d->is_10bit, vfs->type);
//------
	pch->d->nr_cp_mode = false;
	if (nr_bypass_test && !pch->c.ch) { //dv_pl
		if (!pch->d->is_i) {
			pch->d->nr_cp_mode = true;
		} else {
			pch->d->nr_cp_mode = false;
			DBG_WARN("input is i, can't enable dd cp mode\n");
		}
	} else if (nr_bypass_test && pch->c.ch) {
		if (pch->d->is_i) {
			pch->d->nr_cp_mode = true;
		} else {
			pch->d->nr_cp_mode = false;
			DBG_WARN("can't support dv cp + p\n");//to-do
		}
	}
//------
	//direct for t6x:
	pch->d->mode_drct = false;
	if ((dpss_t6x_direct & C_BIT0) && !pch->c.ch)
		pch->d->mode_drct = true;
	else
		pch->d->mode_drct = false;
//------
	prm_dpe->nr_copy_mode = pch->d->nr_cp_mode;	//dv_pl
	big_endian = prm_top->l_endian ? 0 : 1;
	rev_x = prm_top->reverse[0];
	temp_uv = big_endian ^ rev_x;

	if (vf->fgs_valid && fg_enable) {
		film_grain_init(&prm_top->fgrain);
		prm_top->fgrain.path_sel = fg_path;
		prm_top->fg_en = true;
	} else {
		prm_top->fg_en = false;
	}
	if (vf->type & VIDTYPE_VIU_NV12) {
		/* NV12 and big_endian = 0 and rev_x = 0 default uvuv */
		if (temp_uv == 0)
			prm_top->uv_swap = 1;
		else
			prm_top->uv_swap = 0;
		pch->d->fmt = YUV420;
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		if (temp_uv == 0)
			prm_top->uv_swap = 0;
		else
			prm_top->uv_swap = 1;
		pch->d->fmt = YUV420;
	} else if (vf->type & VIDTYPE_VIU_422) {
		if (temp_uv == 0)
			prm_top->uv_swap = 1;
		else
			prm_top->uv_swap = 0;
		pch->d->fmt = YUV422;
	} else {
		pch->d->fmt = YUV444;
		DBG_WARN("vfm_in 444 fmt\n");
	}
	if (pch->d->fmt == YUV444 && pch->d->is_10bit && pch->d->is_afbcd)
		prm_top->fmt_444_10 = true;
	else
		prm_top->fmt_444_10 = false;
	//prm_top->src_dw_uv_swap = prm_top->uv_swap;
	if (pch->d->is_afbcd)
		prm_top->uv_swap = 0;

	dbg_h2("%s:big_endian:%d %d,vfs->type=0x%x,temp_uv=0x%x\n", __func__,
			big_endian,
			rev_x, vf->type, temp_uv);
	if (src != prm_user->src_mode) {
		DBG_ERR("%s:ch[%d]:src not map:%d,%d\n",
			__func__, pch->c.ch, src, prm_user->src_mode);
	}
	//add for di/nr pq
	pq_create_instance(0);
	pq_int(pch, vfs);
#ifdef FUNC_EN_PQ
	dinr_pq_data[pch->c.ch].ch = pch->c.ch;
	dae_hw_init(&dinr_pq_data[pch->c.ch], op);
#endif
	dbg_h2("%s:proc_as_i:%d\n", __func__,
				pch->d->proc_as_i);
	if (pch->c.case_id == TST_CASE_IDX_0157)
		pch->d->en_di_src = true;

	dpss_dbg_irq_ko = 0x1111;
	if (light_chg) { //1005
		_prm_top_init_light(pch, prm_top, src, false, vf);
//		dpss_val_2_top(prm_top, prm_user);
	} else {
		_prm_top_init(pch, prm_top, src, false, vf); //path id 0 is src 0 //dpss_prm_init
		_prm_top_init_light(pch, prm_top, src, false, vf);
		dpss_val_2_top(prm_top, prm_user);
	}
	prm_top->cfg_slc = prm_user->cfg_slc;
	prm_top->src_is_1080p_nods = 0;

	dbg_h2("%s:is_4k:%d, is_afbcd:%d, compw*h:%d*%d, w*h:%d*%d, bitdepth:%d.\n",
		__func__,
		vfs->is_4k,
		pch->d->is_afbcd,
		vf->compWidth,
		vf->compHeight,
		vf->width,
		vf->height,
		vf->bitdepth);
	if (vfs->is_4k) {
		prm_top->src_is_1080p_nods = 0;
		prm_top->dae_dsx_scale = 2;
		prm_top->dae_dsy_scale = 2;
		prm_top->dpe_dw_dsx = 2;
		prm_top->dpe_dw_dsy = 2;

		if ((vf->compWidth >> 1) == vf->width) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsy_scale = 1;
			if (vf->width > 960 || vf->height > 540) {
				prm_top->dae_dsx_scale = 2;
				prm_top->dae_dsy_scale = 2;
			}
			if (vf->width > 960)
				prm_top->size_as_in = 1;
		} else if (vf->compWidth == vf->width && pch->d->is_afbcd) {
			prm_top->dae_dsx_scale = 0;
			prm_top->dae_dsy_scale = 0;
			if (vf->width > 1920) {
				prm_top->dae_dsx_scale = 2;
				prm_top->size_as_in = 1;
			} else if (vf->width > 960) {
				prm_top->dae_dsx_scale = 1;
				prm_top->size_as_in = 1;
			} else if (vf->height > 1080) {
				prm_top->dae_dsx_scale = 2;
				prm_top->dae_dsy_scale = 2;
				prm_top->size_as_in = 1;
			}
			if ((vf->width >> prm_top->dae_dsx_scale) * vf->height > 960 * 540) {
				prm_top->dae_dsy_scale = 1;
				if ((vf->width >> prm_top->dae_dsx_scale) *
						(vf->height >> 1) > 960 * 540)
					prm_top->dae_dsy_scale = 2;
			}
		} else if ((vf->width + 8) >= vf->compWidth && pch->d->is_afbcd) {
			// for special like afbc 224x1808,dw 222x1808
			prm_top->dae_dsx_scale = 0;
			prm_top->dae_dsy_scale = 0;
			if (vf->width > 1920) {
				prm_top->dae_dsx_scale = 2;
				prm_top->size_as_in = 1;
			} else if (vf->width > 960) {
				prm_top->dae_dsx_scale = 1;
				prm_top->size_as_in = 1;
			}
			if (vf->width >> prm_top->dae_dsx_scale * vf->height > 960 * 540) {
				prm_top->dae_dsy_scale = 1;
				if (vf->width >> prm_top->dae_dsx_scale *
						vf->height >> 1  > 960 * 540)
					prm_top->dae_dsy_scale = 2;
			}
		}
		if (nr_dae_xy) {
			prm_top->dae_dsy_scale = nr_dae_xy >> 16;
			prm_top->dae_dsx_scale = nr_dae_xy & 0xffff;
		}
		dbg_i0("%s:dsx = %d,dsy = %d\n", __func__,
			prm_top->dae_dsx_scale, prm_top->dae_dsy_scale);

	} else if (pch->d->is_afbcd) {
		prm_top->src_is_1080p_nods = 0;
		if ((vf->compWidth >> 1) == vf->width) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsy_scale = 1;
			if (vf->width > (960 << 1))
				prm_top->dae_dsx_scale = 2;
			if (vf->height > (540 << 1))
				prm_top->dae_dsy_scale = 2;
		} else if ((vf->compWidth >> 2) == vf->width) {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 2;
		} else if (vf->compWidth == vf->width) {
			prm_top->dae_dsx_scale = 0;
			prm_top->dae_dsy_scale = 0;
			if (vf->width > 960) {
				prm_top->dae_dsx_scale = 1;
				prm_top->size_as_in = 1;
			}
			if (vf->height > 540)
				prm_top->dae_dsy_scale = 1;
		} else if ((vf->width + 8) >= vf->compWidth) {
			//1:1:
			prm_top->dae_dsx_scale = 0;
			prm_top->dae_dsy_scale = 0;
			if (vf->compWidth > 960)
				prm_top->dae_dsx_scale = 1;
			if (vf->compHeight > 540)
				prm_top->dae_dsy_scale = 1;
		} else if ((vf->width + 8) >= (vf->compWidth >> 1)) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsy_scale = 1;
		} else {
			if (vf->compWidth > 1920)
				prm_top->dae_dsx_scale = 2;
			else if (vf->compWidth > 960)
				prm_top->dae_dsx_scale = 1;
			else
				prm_top->dae_dsx_scale = 0;

			if (vf->compHeight > 540)
				prm_top->dae_dsy_scale = 1;
			else
				prm_top->dae_dsy_scale = 0;

			DBG_WARN("dsx: check in: %d, %d, %d, %d\n",
				vf->compWidth, vf->compHeight,
				vf->width, vf->height);
		}

		if (dpss_dbg_ds & 0x10) {
			prm_top->size_as_in = 1;
			DBG_INF("%s:force size_as_in =%d\n",
				__func__, prm_top->size_as_in);
		}

		prm_top->dpe_dw_dsx = 1;
		prm_top->dpe_dw_dsy = 1;
		if (dpss_dbg_ds & 0x80) {
			prm_top->dpe_dw_dsx = prm_top->dae_dsx_scale;
			prm_top->dpe_dw_dsy = prm_top->dae_dsy_scale;
		}
		dbg_i0("%s:dsx = %d,dsy = %d dw: %d,%d\n", "afbcd",
					prm_top->dae_dsx_scale,
					prm_top->dae_dsy_scale,
					prm_top->dpe_dw_dsx,
					prm_top->dpe_dw_dsy);
		dbg_i0("dsx: check in: %d, %d, %d, %d\n",
				vf->compWidth, vf->compHeight,
				vf->width, vf->height);
	} else {
		//afbcd to-do
		//below only for mif input
		if (vfs->width > 960)
			prm_top->dae_dsx_scale = 1;
		else
			prm_top->dae_dsx_scale = 0;

		if (pch->d->is_i || pch->d->proc_as_i)
			tmp = vfs->height >> 1;
		else
			tmp = vfs->height;
		if (tmp > 540)
			prm_top->dae_dsy_scale = 1;
		else
			prm_top->dae_dsy_scale = 0;

		prm_top->dpe_dw_dsx = 1;
		prm_top->dpe_dw_dsy = 1;
	}

	nr_lcevc_vf_parser(pch, vf, prm_top);
	//cfg_dpss_size_nr(prm_top);//test for me setting todo//test
	fpga_ucode_1217(prm_top, dpss_dbg_top_cfg0);//part of HW_Initialize 12-17
	dbg_h2("%s:aa pps_dsx = %d,\n", __func__, prm_top->frm_vsize);
	if (vf->dpss_flg & DPSS_FLG_USE_DW)
		vfs->dpss_flag |= DPSS_FLG_USE_DW;
	if (vf->dpss_flg & DPSS_FLG_PPS_SKIP)
		vfs->dpss_flag |= DPSS_FLG_PPS_SKIP;
	nr_dpe_pps_cfg(prm_top, pch, vfs);
	if (!light_chg) {
		_prm_top_init_buffer(prm_top, pch, src);
		hw_init_small_addr(prm_top, src);
		pps->trig_pps = 1;
		pps->backup_pps = pps->pps_en;
		dbg_pps0("ch[%d] pps->trig_pps = %d\n",
		pch->c.ch, pps->trig_pps);
	} else {
		if (pps->pps_en && pps->backup_pps)
			pps->trig_pps = 5;//576>>320
		else if (!pps->pps_en && !pps->backup_pps)
			pps->trig_pps = 4;//4k>>1080
		else if (pps->pps_en && !pps->backup_pps)
			pps->trig_pps = 3;//4k>>576
		else if (!pps->pps_en && pps->backup_pps)
			pps->trig_pps = 2;//576>>4k
		pps->backup_pps = pps->pps_en;
	}
	_prm_top_init_vfm(pch, prm_top, vfs, false);

	if (pch->c.ch == 0 && pch->d->frc_link) {
		if (!light_chg)
			init_frc_pre(vfs, pch);
		init_frc_pre_light(vf);
	}

	//hw_init_prm(pch, prm_top, src, vfs);

	if (!light_chg)
		hw_cfg_dpss_top(prm_top);
	else
		hw_cfg_dpss_buff_addr_src0_nro(prm_top,
			pch->c.o_afbc ? true : false);

	if (dpss_is_h_first_ch(pch))
		hw_cfg_dpss_inp(prm_top, prm_dpss_inp);

	if (prm_top->frm_hsize <= 1920 && prm_top->frm_hsize > 960 && !pch->d->is_afbcd)
		prm_top->src_is_1080p_nods = 1;/*only 960->1920 non-afbc set 1*/

	if (prm_top->src_is_1080p_nods)
		prm_top->src_dw_uv_swap = prm_top->uv_swap;
	else
		prm_top->src_dw_uv_swap = 0;
	if (pch->d->nr_cp_mode && !pch->c.ch) {
		prm_top->dae_nr_mode = DAE_BYPS_MODE;
		prm_top->dpe_nr_mode = DPE_NR_BYPS;
		dbg_h0("nr_bypass_test %d\n", prm_top->dae_nr_mode);
	}
	if (pch->d->mode_drct) {
		prm_top->dae_nr_mode = DAE_BYPS_MODE;
		prm_top->dpe_nr_mode = DPE_NR_BYPS;
		prm_top->mode_drct = true;
		dpss_en_dct = 0;
	} else {
		prm_top->mode_drct = false;
	}

	if (pch->d->state_nr_bypass)
		dpss_en_dct = 0;
	else
		dpss_en_dct = 1;
	if (dpss_dct_force & C_BIT31) {
		if (dpss_dct_force & C_BIT8)
			dpss_en_dct = 1;
		else if (dpss_dct_force & C_BIT9)
			dpss_en_dct = 2;
		else
			dpss_en_dct = 0;
	}
	prm_dae->dctgrd_en = dpss_en_dct;
	dbg_i0("init:direct =%d\n", pch->d->mode_drct);
	if ((((dpss_en_dct & C_BIT0) && !pch->c.ch) ||
	    ((dpss_en_dct & C_BIT1) && pch->c.ch)) &&
	    prm_top->dpe_nr_mode != DPE_NR_BYPS) {
		dbg_i1("%s:src_is_1080p_nods =%d, prm_top->dae_dsx_scale=%d. %d, frm_hsize=%d\n",
			__func__, prm_top->src_is_1080p_nods,
			prm_top->dae_dsx_scale,
			prm_top->dae_dsy_scale,
			prm_top->frm_hsize);
		prm_dae->dctgrd_en = 1;
		//dpss_ini_dcntr_pre(prm_top->frm_hsize, prm_top->frm_vsize, 2, 1,
		//		   0, 0, 0);
		prm_dpe->dcntr_en = 1;
		prm_dpe->dcntr_en_bk = 1;
		prm_top->dct_ahead_dv_mode = 1;

		if (dpss_en_lcevc) {
			prm_dae->dctgrd_en = 0;
			prm_dpe->dcntr_en = 0;
			prm_dpe->dcntr_en_bk = 0;
			dbg_h0("enable lcevc,need dct off\n");
		}

		pchip_st = dpss_get_frc_st();
		if (!pchip_st)
			return;
		if ((dpss_dct_force & 0xff) && dpss_en_dct)
			prm_top->dct_ahead_dv_mode = 1;
		else
			prm_top->dct_ahead_dv_mode = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (is_amdv_enable() && is_amdv_frame(vf)) {
			if (!is_amdv_dpss_path() ||
				get_amdv_force_mode() == AMDV_OUTPUT_MODE_BYPASS)
				prm_top->dct_ahead_dv_mode = 0;/*if dv bypass,not config dct->dv*/
		}

		dpss_info.dct_ahead_dv_mode = prm_top->dct_ahead_dv_mode;
#endif

		dbg_i0("dv_ahead:%d\n", prm_top->dct_ahead_dv_mode);

		prm_dpe->dcntr_slc.hsize = prm_top->frm_hsize;
		if (pch->d->en_di_src)
			prm_dpe->dcntr_slc.vsize = prm_top->frm_vsize / 2;
		else
			prm_dpe->dcntr_slc.vsize = prm_top->frm_vsize;
		prm_dpe->dcntr_slc.olap = 0;
		prm_dpe->dcntr_slc.ds_fmt        = YUV420; //0:444 1:422 2:420
		prm_dpe->dcntr_slc.ds_plane      = PLANAR_X2;
		prm_dpe->dcntr_slc.ds_bit        = BIT_008;
		prm_dpe->dcntr_slc.ds_mode       = 0;

		if (prm_top->src_is_1080p_nods)
			prm_dpe->dcntr_slc.block_mode = prm_top->block_mode;
		else
			prm_dpe->dcntr_slc.block_mode = 0;
		prm_dpe->dcntr_slc.canvas_hsize = prm_top->nr_src_canvas_hsize;

		if (pch->d->is_i) {
			if (prm_top->frm_hsize > 960)
				prm_dpe->dcntr_slc.ds_mode = 1;
			else
				prm_dpe->dcntr_slc.ds_mode = 0;
		} else {
			if (pch->d->is_afbcd || prm_top->dae_dsx_scale == 0)
				prm_dpe->dcntr_slc.ds_mode = 0;
			else if (prm_top->dae_dsx_scale == 1) {
				if (prm_top->dae_dsy_scale == 0 &&
					prm_top->src_is_1080p_nods)
					prm_dpe->dcntr_slc.ds_mode = 1;
				else
					prm_dpe->dcntr_slc.ds_mode = 2;
			} else if (prm_top->dae_dsx_scale == 2) {
				prm_dpe->dcntr_slc.ds_mode = 3;
			}
		}

		if (pch->d->is_afbcd) {
			if (vfs->bitdepth_dw & BITDEPTH_Y10)
				prm_dpe->dcntr_slc.ds_bit = BIT_010;
		}

		if (vfs->type & VIDTYPE_VIU_422) {
			prm_dpe->dcntr_slc.ds_fmt = YUV422;
			if (vfs->plane_num == 1)
				prm_dpe->dcntr_slc.ds_plane = PLANAR_X1;
		} else if (vfs->type & VIDTYPE_VIU_444) {
			prm_dpe->dcntr_slc.ds_fmt = YUV444;
			prm_dpe->dcntr_slc.ds_plane = PLANAR_X1;
		} else {
			prm_dpe->dcntr_slc.ds_fmt = YUV420;
		}
		dbg_i1("%s:ds_fmt=%d,ds_plane=%d, ds_bit=%d, ds_mode=%d,dsx_scale=%d,%d,is_i=%d\n",
			__func__,
			prm_dpe->dcntr_slc.ds_fmt,
			prm_dpe->dcntr_slc.ds_plane,
			prm_dpe->dcntr_slc.ds_bit,
			prm_dpe->dcntr_slc.ds_mode,
			prm_top->dae_dsx_scale,
			prm_top->dae_dsy_scale,
			pch->d->is_i);

		if (pch->d->is_afbcd)
			me_size_tmp = vf->compWidth >> prm_top->dae_dsx_scale;
		else
			me_size_tmp = vf->width >> prm_top->dae_dsx_scale;

		if (me_size_tmp < 80)
			prm_dpe->dcntr_slc.grid_num_mode = 0;
		else
			prm_dpe->dcntr_slc.grid_num_mode = 2;
		dbg_i1("me_size_tmp: %d\n", me_size_tmp);

		prm_dpe->dcntr_slc.grid_stride = 648;
		prm_dpe->dcntr_slc.uv_swap = prm_top->src_dw_uv_swap;
		prm_dpe->dcntr_slc.swap_64bit = prm_top->swap_64bit;
		prm_dpe->dcntr_slc.ds_bit = prm_top->src_ds_fmt.src_bit;
	}
	prm_top->frame_count = 0;

	if (light_chg)
		prm_top->trig_bypass = true;

	if (pch->d->en_di_src) {
		dbg_i1("en_di_src\n");
		memcpy(prm_top_di, prm_top, sizeof(*prm_top_di));
		memcpy(prm_dae_di, prm_dae, sizeof(*prm_dae_di));
		memcpy(prm_dpe_di, prm_dpe, sizeof(*prm_dpe_di));
		prm_top_di->src_fs_fmt.interlace = IS_ISRC;
		prm_top_di->frm_vsize	   = prm_top->frm_vsize / 2;
		prm_top_di->org_vsize	   = prm_top->org_vsize / 2;
		prm_top_di->dae_dsy_scale  = 0;

		_prm_top_init_buffer(prm_top_di, pch, src);

		if (!dpss_dbg_dis_i_srcreg)
			w_reg_bit(DPSS_TOP_FUNC_CTRL, SRC_IDX_DI1, 24, 4);

		if ((dpss_dbg_top_cfg0 & C_BIT28) || dpss_is_h_first_ch(pch))
			hw_cfg_dpss_dae(prm_top_di->dae_nr_mode, prm_top_di,
					prm_dae_di);

		if ((dpss_dbg_top_cfg0 & C_BIT29) || dpss_is_h_first_ch(pch)) {
			if (dpss_get_h_bypass()) {
				hw_cfg_dpss_dpe(DPE_NR_BYPS, prm_top_di, prm_dpe_di,
						pps);
				dpss_last_dpe_mode = DPE_NR_BYPS;
				dpss_last_dpe_src = 0;
			} else {
				hw_cfg_dpss_dpe(prm_top_di->dpe_nr_mode, prm_top_di,
						prm_dpe_di, pps);
				dpss_last_dpe_mode = prm_top_di->dpe_nr_mode;
				dpss_last_dpe_src = 0;
			}
			prm_top->is_current = true;
		}
	} else {
		if (!dpss_dbg_dis_i_srcreg)
			w_reg_bit(DPSS_TOP_FUNC_CTRL, SRC_IDX_NR, 24, 4);
		if (((dpss_dbg_top_cfg0 & C_BIT28) || dpss_is_h_first_ch(pch)) && !light_chg)
			hw_cfg_dpss_dae(prm_top->dae_nr_mode, prm_top, prm_dae);
		if ((dpss_dbg_top_cfg0 & C_BIT29) || dpss_is_h_first_ch(pch)) {
			if (pch->c.case_id == TST_CASE_IDX_0002) {
				dbg_i1("do not set dpe\n");
			} else {
				prm_top->is_current = true;
				hw_cfg_dpss_dpe(prm_top->dpe_nr_mode, prm_top,
						prm_dpe, pps);
				dpss_last_dpe_mode = prm_top->dpe_nr_mode;
				dpss_last_dpe_src = src;
			}
		}
	}
	if (pch->c.ch == 0 && pch->d->frc_link && !light_chg)
		init_frc_post(pch, vfs);
	if (dpss_dbg_top_cfg0 & C_BIT30) {
		dbg_h2("bypass dae:0x80ae\n");
		w_reg_bit(FRC_AUTO_MODE_INP_NR_LINK, 1, 0, 1);	//
	}
	//ary add 0405:discuss with VLSI
	w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 1, 0, 2);	//reg_dae_top_arb_mode
	if (dpss_dbg_top_cfg0 & C_BIT31)
		f_dpss_hw_init(prm_top);

	if (dpss_en_hdr && !pch->c.ch && !pch->d->di_front) {
		//dpss_hdr_int(vfs->width, vfs->height, 1);
		dpss_hdr_sw(true, vf);
		dbg_h2("%s dpss_hdr_sw true\n", __func__);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	//      dpss_dv_init();
	if (dpss_dv_en & C_BIT0 || pch->d->is_dd) {
		if (prm_top->dpe_nr_mode == DPE_NR_BYPS) {
			dpss_info.pad_mode = 0;
			if (prm_dpe->dcntr_en && !prm_top->dct_ahead_dv_mode)
				dpss_info.pad_mode += 8;
			if (prm_dpe->aa_pad) {
				if (pps->pps_en)
					dpss_info.pad_mode += prm_dpe->aa_pad;
				else
					dpss_info.pad_mode += 8;
			}
		} else {
			dpss_info.pad_mode = 64;
			if (prm_dpe->dcntr_en && !prm_top->dct_ahead_dv_mode)
				dpss_info.pad_mode += 8;
		}
		if (pch->d->nr_cp_mode) {
			dpss_info.pad_mode = 0;
			dbg_h0("nr_by_test dv pad 0\n");
		}
		dbg_h0("dv_pad: dpe_nr_mode=%d,dcntr_en=%d,aa_pad=%d,nr_cp_mode=%d,pad_mode=%d\n",
			prm_top->dpe_nr_mode,
			prm_dpe->dcntr_en,
			prm_dpe->aa_pad,
			pch->d->nr_cp_mode,
			dpss_info.pad_mode);
		dpss_info.slice_num = prm_top->dpe_slc_num;
		dpss_info.frm_hsize_sel = prm_top->frm_hsize_sel;
		dpss_info.tbc_mode = prm_top->sw_tbc_mode;
		dpss_info.vds_4k1k_en = prm_top->vds_4k1k_en;
		dpss_info.dct_ahead_dv_mode = prm_top->dct_ahead_dv_mode;
		dpss_info.real_h = prm_top->frm_hsize;
		dpss_info.real_v = prm_top->frm_vsize;
		if (dpss_dv_en & C_BIT9) {
			dpss_info.real_h = prm_top->org_hsize;
			dpss_info.real_v = prm_top->org_vsize;
		}
		if (prm_top->vds_4k1k_en)
			dpss_info.real_v = dpss_info.real_v << 1;
		if (prm_top->mode_drct)
			dpss_info.direct_mode = true;
		else
			dpss_info.direct_mode = false;
		dpss_dd_init(vf, &dpss_info);
	}
#endif
	if (dpss_en_rdma & C_BIT0)
		dpss_rdma_pre_set(pch);
	else if (dpss_en_rdma & C_BIT1)
		dpss_rdma_pre_set_all(pch);

	pch->d->init = true;
	prm_top->is_dpss_init_done = true;
	//dbg_f2("dpss_init_done=%d\n", prm_top->is_dpss_init_done);
}

struct vframe_s *dpss_irq_get_vfm(unsigned int idx, unsigned int lab)
{
	struct dpss_nr_i_s *nr_i;
	unsigned char id, v_id;
	struct vframe_s *vfm;

	//id = (idx % DPSS_HW_LOOP_IN_OUT_BUF_NUB);
	id = idx;
	if (idx >= DPSS_HW_LOOP_IN_OUT_BUF_NUB) {
		id = 0;
		DBG_ERR("%s:overflow:%d\n", __func__, idx);
	}

	v_id = dpss_idx[id];
	nr_i = &dpss_nr1[v_id];
	if (!nr_i || !nr_i->in_vfm) {
		DBG_ERR("irq [%d]no nr_i:%p,%d,%d\n",
			lab, nr_i,
			idx, id);
		return NULL;
	}
	vfm = nr_i->in_vfm;

	if (!vfm) {
		DBG_ERR("irq [%d]no vfm\n", lab);
		return NULL;
	}
	return vfm;
}

void nr_h_wait_mode(struct dpss_ch_s *pch)
{				//to-do for ddd
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	u32 dae_info0;
	u32 dae_info1;
	u64 dae_info;
	struct vframe_s *vfm;
	unsigned int idx;
	int i;

	if (pch->c.ch != 0 || !prm_top->wait_en)
		return;

	for (i = 0; i < 2; i++) {
		if (dpss_dae_done[dpss_wait_cnt1]) {
			dpss_dae_done[dpss_wait_cnt1] = 0;
			idx = pch->d->h_dae_vf_idx[dpss_wait_cnt1];
			if (idx == 0xff) {
				DBG_ERR("%s:dae_vf:overflow:\n", __func__);
				return;
			}
			dbg_h2("%s:dae_vf:rd:idx:%d:%d\n", __func__,
			       dpss_wait_cnt1, idx);
			vfm = pch->d->nr_i[idx].in_vfm;
			if (!vfm) {
				DBG_ERR("%s:dae_vf:no vfm?idx=%d\n", __func__,
					idx);
				return;
			}
			//dv set
			dpss_dd_cfg_dpe(vfm);
			dae_info = fnr_dpe_fifo[dpss_wait_cnt1];
			dae_info0 = (u32)(dae_info & 0xffffffff);
			dae_info1 = (u32)(dae_info >> 32);
			cfg_fnr_dpe_ibuf_rdy(dae_info0, dae_info1);
			dpss_wait_cnt1++;
			if (dpss_wait_cnt1 >= reg_fnr_dpe_ibuf_num)
				dpss_wait_cnt1 = 0;
			dbg_h2("%s:%d:info:0x%x,0x%x\n", __func__,
			       dpss_wait_cnt1, dae_info0, dae_info1);
		} else {
			break;
		}
	}
}

void hw_cfg_addr_dio(struct dpss_ch_s *pch,
		     unsigned int in_cnt, struct vframe_s *vfm)
{
	unsigned int idx_in;
	unsigned long addr_y, addr_c;
	unsigned int addr_s_y, addr_s_c;
	struct PRM_DPSS_TOP *prm_top;

	unsigned int src_mode;

	prm_top = &pch->c.prm_top;
	src_mode = prm_top->src_mode;

	idx_in = in_cnt % pch->c.o_b_nub;

	if (!vfm) {
		DBG_ERR("%s:no vfm?in_cnt=%d\n", __func__, in_cnt);
		return;
	}
	addr_y = vfm->canvas0_config[0].phy_addr;
	addr_c = vfm->canvas0_config[1].phy_addr;

	addr_s_y = (unsigned int)(addr_y >> 9);
	addr_s_c = (unsigned int)(addr_c >> 9);

	if (!src_mode) {
		prm_top->src0_dio_fbuf_yaddr[idx_in] = addr_s_y;
		prm_top->src0_dio_fbuf_caddr[idx_in] = addr_s_c;
		wr((DPSS_SRC0_DIO_FBUF_YADDR0 + idx_in), addr_s_y);
		wr((DPSS_SRC0_DIO_FBUF_CADDR0 + idx_in), addr_s_c);
	} else {
		prm_top->src1_dio_fbuf_yaddr[idx_in] = addr_s_y;
		prm_top->src1_dio_fbuf_caddr[idx_in] = addr_s_c;
		wr((DPSS_SRC1_DIO_FBUF_YADDR0 + idx_in), addr_s_y);
		wr((DPSS_SRC1_DIO_FBUF_CADDR0 + idx_in), addr_s_c);
	}
	dbg_h1("%s:vfm_o in_cnt=%d,idx_in=%d, addr y=0x%lx, vfm_o = %px\n",
		"cfg_addr", in_cnt, idx_in, addr_y, vfm);
}

bool nr_only_input_buf(struct dpss_ch_s *pch, struct dpss_nr_i_s *nr_i)
{
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	unsigned int src = prm_top->src_mode;
	unsigned int index;
	struct dpss_sub_vf_s *vfs;
	struct vframe_s *vf;
	unsigned int i_cnt = 0;
	bool is_i = false;
	struct frc_chip_st *pchip_st = NULL;

	if (!nr_i || !nr_i->in_vfm) {
		DBG_ERR("%s:no vf?\n", __func__);
		return false;
	}

	vfs = &nr_i->sub_vf_in;
	vf = nr_i->in_vfm;

	//---------
	if (VFM_IS_I_SRC(vf->type)) {
		if (D_COM_ME(vf->type, VIDTYPE_INTERLACE_BOTTOM))
			i_cnt = 1;
		i_cnt += (pch->c.in_cnt & 0x01);
		i_cnt = (i_cnt + 1) & 0x1;
		if (prm_top->f_top != i_cnt) {
			DBG_INF("2 tb chg:%d\n", pch->c.in_cnt);
			prm_top->f_top = i_cnt;
			if (!pch->c.ch)
				prm_dpss_top_di.f_top = i_cnt;
		}
	}
	//---------
	//---------
	pchip_st = dpss_get_frc_st();
	if (!pch->c.ch &&  pchip_st &&
	    pchip_st->chip == ID_T6X) {
		if (((dpss_t6x_direct & C_BIT4) ||
		     (pch->d->w_mode_2 & DPSS_DIRECT_MODE)) &&
		     !is_i && !prm_top->mode_drct2) {
			prm_top->mode_drct2 = 1;
			dbg_i0("enable drct2\n");
		} else if (prm_top->mode_drct2 &&
			!(dpss_t6x_direct & C_BIT4) &&
			!(pch->d->w_mode_2 & DPSS_DIRECT_MODE)) {
			prm_top->mode_drct2 = 0;
			dbg_i0("disable drct2\n");
		}
	}
	//---------
	index = pch->c.in_cnt % pch->c.in_b_nub;

	if (vf->fgs_valid && vf->fgs_table_adr && fg_enable) {
		prm_top->fgrain.dma_ini[0] = 1;
		film_grain_cfg(&prm_top->fgrain, vf, pch->c.in_cnt);
	} else {
		film_grain_uint();
	}
	dbg_h1("%s in ch%d,vfm index:0x%x,type:0x%x,flag:0x%x,in_cnt:0x%x,out:0x%x\n", __func__,
				pch->c.ch, vf->frame_index,
				vf->type, vf->flag, pch->c.in_cnt,
				pch->c.out_cnt);
	pch->c.in_cnt++;
	pch->c.in_total_cnt++;
	if (dpss_en_lcevc & C_BIT2)
		nr_lcevc_update_addr(pch, vf, index, prm_top);
	else if (pch->d->is_afbcd)
		_prm_top_input_afbcd(prm_top, src, index, 0, vfs);
	else
		_prm_top_input_buffer_set(prm_top, src, index, vfs);
	dbg_h2("in:%d\n", index);
	pch->d->h_dae_vf_idx[index] = nr_i->idx;
	dbg_h2("%s:dae_vf:set:%d,%d\n", __func__, index, nr_i->idx);
	if (dpss_dv_en & C_BIT0) {	//test
		//dv set:
		dbg_h2("%s:dd:\n", __func__);
		dpss_dd_cfg_inp(vf);
	}

	if (dpss_en_hdr && !pch->c.ch && pch->c.in_cnt == 1)
		dpss_hdr_proc(vf);
	if (pch->c.case_id == TST_CASE_IDX_1000) {
		hw_tbc_src0_input(prm_top, index);
		return true;
	}
	if (pch->c.case_id == TST_CASE_IDX_1001) {
		hw_tbc_src1_input(prm_top, index);
		return true;
	}

	if (src == 0)
		dbg_step0_trig_input(index);
	else
		dpss_trig_input_src1(index);

	return true;
}

#ifdef MOV

#define DPSS_TST_NR_ONLY_IN_NUB		(5)

void tst_auto_nr_only_set(struct dpss_ch_s *pch)
{
	unsigned int len;
	struct vframe_s *vfm, *vfm_in;
	struct dpss_vf_s1 *vf_s1;
	unsigned int chg;
	int i;
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = &pch->c.prm_dae;//&prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;//&prm_dpss_dpe;

	unsigned int src = 0;

	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN].f);
	if (len < DPSS_TST_NR_ONLY_IN_NUB)
		return;

	//--------------------------
	for (i = 0; i < len; i++) {
		vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_IN);
		if (!vfm || !vfm->dpss_data) {
			DBG_ERR("%s:%d no vfm or data", __func__, i);
			return;
		}
		vf_s1 = (struct dpss_vf_s1 *)vfm->dpss_data;
		if (!vf_s1->vf_nr) {
			DBG_ERR("%s:%d no vf_nr", __func__, i);
			return;
		}

		vfm_in = vf_s1->in_vfm;
		if (!vfm_in) {
			DBG_ERR("%s:no in\n", "tst_auto_nr_only_set");
			continue;
		}
		dpss_vfm_2_subvf(&vf_s1->sub_vf_in, vfm_in);
		chg = dpss_sub_vf_check(pch, &vf_s1->sub_vf_in);
		if (chg)
			vf_s1->sub_vf_in.is_chg = true;

		//-----------------------------------------------
		if (i == 0)
			hw_init_prm(pch, prm_top, src, vfm_in);
		_prm_top_input_buffer_set(prm_top, src, i, vfm_in);

		dpss_vfm_cp(vfm, vfm_in);
		dpss_q_in_idx(&pch->d->q_ch[EDPSS_Q_CH_NR_DOING],
			      vf_s1->hd->idx);
	}
	// Null - checking [vfm_in] suggests that it may be null
	// but it has already been dereferenced at line 3401
	// in function[_prm_top_input_buffer_set].
	//hw setting:
	if (!vfm_in) {
		DBG_ERR("%s:no in\n", __func__);
		return;
	}
	// only for debug
	for (i = len; i < 15; i++)
		_prm_top_input_buffer_set(prm_top, src, i, vfm_in);

	hw_cfg_dpss_top(prm_top);
	hw_cfg_dpss_inp(prm_top, prm_dpss_inp);
	hw_cfg_dpss_dae(DAE_NR_MODE, prm_top, prm_dae);
	hw_cfg_dpss_dpe(DPE_NR_MODE, prm_top, prm_dpe, &g_nr_pps_cfg);
	//DPE_NR_MODE //DPE_NR_BYPS
}
#endif
#ifdef DBG_TEST_CREATE
static struct vframe_s vf_tst1 = {
	.type = 0x9000,
	.width = 192,
	.height = 144,
	.plane_num = 2,
	.canvas0_config[0].width = 192,
	.canvas0_config[0].height = 144,
	.canvas0_config[1].width = 192,
	.canvas0_config[1].height = 144,
};

static struct dpss_sub_vf_s vfs_tst;
#endif				//DBG_TEST_CREATE
void dbg_step1_hw_init(unsigned int ch)
{
#ifdef DBG_TEST_CREATE
	int i;
	struct PRM_DPSS_TOP *prm_top;// = &prm_dpss_top;
//      struct PRM_DPSS_DAE *prm_dae = &prm_dpss_dae;
//      struct PRM_DPSS_DPE *prm_dpe = &prm_dpss_dpe;
	unsigned long baddr, offset, offset_y;

	struct vframe_s *vfm = &vf_tst1;
	struct dpss_sub_vf_s *vfs = &vfs_tst;

	struct dpss_ch_s *pch = dpss_get_ch(ch);

	baddr = dbt_crt_get_blk_addr();
	if (baddr == 0) {
		DBG_ERR("%s addr is 0\n", __func__);
#ifdef RUN_ON_ARM
		return;
#endif
	}
	prm_top = &pch->c.prm_top;
	offset = dbg_crt_get_buf_size();
	offset_y = dbg_crt_get_y_size();

	dpss_vfm_2_subvf(vfs, vfm);

	hw_init_prm(pch, prm_top, 0, vfs);
	//only for debug:
	for (i = 0; i < 15; i++) {
		vfs->canvas0_config[0].phy_addr = baddr + i * offset;
		vfs->canvas0_config[1].phy_addr = baddr + i * offset + offset_y;
		dbg_ins0("input:%d:addr: 0x%lx : 0x%lx\n",
			 i, vfm->canvas0_config[0].phy_addr,
			 vfm->canvas0_config[1].phy_addr);
		_prm_top_input_buffer_set(prm_top, 0, i, vfs);
	}
#endif				//DBG_TEST_CREATE
}

void dbg_step2_hw_top(unsigned int step)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;

	if (step == 1) {
		hw_cfg_dpss_top(prm_top);
	} else if (step == 2) {
		hw_cfg_dpss_inp(prm_top, prm_dpss_inp);
	} else if (step == 3) {
		hw_cfg_dpss_dae(DAE_NR_MODE, prm_top, prm_dae);
	} else if (step == 4) {
		hw_cfg_dpss_dpe(DPE_NR_MODE, prm_top, prm_dpe, &g_nr_pps_cfg);
	} else if (step == 5) {
		DBG_INF("nr bypss\n");
		prm_top->is_current = true;
		hw_cfg_dpss_dpe(DPE_NR_BYPS, prm_top, prm_dpe, &g_nr_pps_cfg);
	} else if (step == 6) {
		DBG_INF("dv\n");
		prm_top->dolby_mode = DOLBY_DPSS_MODE;
	}
}

void dbg_step3_int_dae(unsigned int step)
{
	irq_dae1();
}

void dbg_step4_int_dpe(unsigned int step)
{
	irq_dpe1();
}

void dbg_step0_trig_input(unsigned int index)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;

	if (!pchip_st)
		return;

	game_st = &pchip_st->game_st;

	if (!src0_data_trigger()) {
		DBG_WARN("%s:src0 no rd\n", __func__);
		return;
	}

	if (prm_top->game_mode_n2m == 1 || prm_top->game_mode_film == 1) {
		w_reg_bit_vc(VDIN_TOP_SIZE, game_st->vdin_buf_num - 1, 20, 4);
		// w_reg_bit_vc(VDIN_TOP_SIZE, dpss_frc_vrr_index, 16, 4);
		w_reg_bit_vc(VDIN_TOP_SIZE, index, 16, 4);
		// w_reg_bit_vc(VDIN_TOP_SIZE, 1, 24, 1);
	}
	dbg_h1("%s:index =%d\n", __func__, index);
	w_reg_bit(FRC_REG_INP_IBUF_CFG, index, 4, 4);
	cfg_dpss_done_trigger(FRC_SRC_DONE);

	dbg_h2("%s:index =%d, VDIN_TOP_SIZE=%#X, DPSS_REG_INP_IBUF=%#X\n",
	       __func__, index, rd_vc(VDIN_TOP_SIZE), rd(FRC_REG_INP_IBUF_CFG));
	src0_frm_idx_cnt++;
}

void dpss_trig_input_src1(unsigned int idx)
{
//ary set and no use    int       reg_vdi_src_idx = 8;

	if (src1_data_trigger()) {
//ary set and no use            reg_vdi_src_idx = (rd(DPSS_TOP_FUNC_CTRL1) >> 16)&0xf;
		cfg_dpss_done_trigger(VDI_SRC_DONE);
#ifdef MOV
		if (idx != reg_vdi_src_idx)
			DBG_ERR("%s:not map %d,%d\n", __func__, idx,
				reg_vdi_src_idx);
#endif

		src1_frm_idx_cnt++;
	} else {
		DBG_ERR("%s:src1 no rd\n", __func__);
	}
}

void dbg_trig_input_handy(unsigned int ch_idx)
{
	unsigned int ch;
	unsigned int idx_in, idx_out;
	struct dpss_ch_s *pch;
	struct vframe_s *vfm, *vfm_in;

	struct dpss_nr_i_s *nr_i;

	ch = (ch_idx >> 8) & 3;
	pch = dpss_get_ch(ch);
	idx_in = ch_idx & 0xf;
	idx_out = pch->c.out_cnt % pch->c.o_b_nub;

	DBG_INF("%s:ch[%d], idx_in[%d], o[%d]\n", __func__, ch, idx_in,
		idx_out);
	pch = dpss_get_ch(ch);
	if (!pch->d) {
		DBG_WARN("%s:no d\n", __func__);
		return;
	}
	vfm_in = pch->d->vfm_in[idx_in];
	if (!vfm_in) {
		DBG_ERR("%s:vfm in is null\n", __func__);
		return;
	}
	dbg_h1("%s:type=0x%x, xy <%d, %d>\n", __func__,
	       vfm_in->type, vfm_in->width, vfm_in->height);

	vfm = &pch->d->vfm_nr[idx_out];
	if (!vfm || !vfm->dpss_data)
		return;
#ifdef MOV
	if (!pch->d->vfm_in_act[idx_out]) {
		DBG_INF("not act\n");
		return;
	}
#endif

	nr_i = lk_get_nr_i(vfm);
	if (!nr_i) {
		DBG_WARN("no nr?\n");
		return;
	}
	nr_i->in_vfm = vfm_in;
	dpss_vfm_2_subvf(&nr_i->sub_vf_in, vfm_in);

	//copy
	dpss_vfm_cp(vfm, vfm_in);

	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_NR_DOING], vfm);
	nr_only_input_buf(pch, nr_i);
}

//bit 0. 7 , 12 is dandous, don't touch. bit 9 don't touch
void dbg_dpss_reset(unsigned int para)
{
	if (dpss_dbg_0709 & C_BIT8) {
		dbg_i0("no reset\n");
		return;
	}

	if (para == 0 || para == 0x10) {
		wr(DPSS_DPE_MC_TOP_RESET, BIT_22 + BIT_25);
		dbg_i0("mc reset bit 22+25\n");
		return;
	} else if ((para & 0xF0) == 0x80) {
		wr(DPSS_DPE_MC_TOP_RESET, BIT_22);
		//dbg_f1("mc reset bit 22\n");
	} else if ((para & 0xF0) == 0x20) {
		w_reg_bit(FRC_REG_INPUT_SIZE, 0, 30, 1); //off frc hw
		wr(DPSS_DPE_MC_TOP_RESET, BIT_22 + BIT_25);
		dbg_i0("reset_off_frc with mc_2225\n");
		return;
	}

	if ((para & 0x0F) == 0x01) {
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x0010, 0, 13); //2024-12-05 from 0x47e to 0x10
		//dbg_h1("dpss reset 0x10\n");
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x0000, 0, 13);

	} else if ((para & 0x0F) == 0x02) {
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x047e, 0, 13); //2024-12-05 from 0x1fff to 0xf7e
		//dbg_h1("dpss reset 0x47e\n");
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x0000, 0, 13);
	}
}

void dpss_dis_idx(struct dpss_ch_s *pch, unsigned int idx)
{
	struct vframe_s *vfm;
	struct dpss_nr_i_s *nr_i;

	if (!pch || !pch->d || idx >= DPSS_VFM_IN_NUB) {
		DBG_WARN("%s:no pch? %d\n", __func__, idx);
		return;
	}
	vfm = &pch->d->vfm_nr[idx];

	if (!vfm || !vfm->dpss_data) {
		DBG_ERR("%s:no vfm?\n", __func__);
		return;
	}

	nr_i = lk_get_nr_i(vfm);
	dbg_h1("%s idx:%d, <%d,%d>\n", __func__, idx, vfm->width, vfm->height);
	dbg_h1("\t:type:0x%x, plan:%d", vfm->type, vfm->plane_num);
	dbg_h1("\t:bitdepth = 0x%x\n", vfm->bitdepth);
	dbg_h1("\t:cvs:addr=<0x%lx, 0x%lx>, <%d,%d>",
	       vfm->canvas0_config[0].phy_addr,
	       vfm->canvas0_config[1].phy_addr,
	       vfm->canvas0_config[0].width, vfm->canvas0_config[0].height);

	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_RD], vfm);
	if (pch->c.etype == 0) {	//vframe path
		itf_vfm_path_ready_notify(pch);
	}
}

void dpss_dis_vfm(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
//      struct dpss_nr_i_s *nr_i;
	unsigned int idx;

	if (!pch || !vfm || !pch->d) {
		DBG_ERR("%s: NULL param.\n", __func__);
		return;
	}

	if (!vfm->dpss_data)
		DBG_ERR("%s:ch[%d] no dpss data? %p.\n", __func__, pch->c.ch, vfm);

	idx = lk_idx_get(vfm);
	dbg_h1("%s idx:%d, <%d,%d>\n", __func__, idx, vfm->width, vfm->height);
	dbg_h1("\t:type:0x%x, plan:%d, bitdepth = 0x%x\n",
		vfm->type, vfm->plane_num, vfm->bitdepth);
	dbg_h1("\t:cvs:addr=<0x%lx, 0x%lx>, <%d,%d>,<%d,%d>\n",
		vfm->canvas0_config[0].phy_addr,
		vfm->canvas0_config[1].phy_addr,
		vfm->canvas0_config[0].width,
		vfm->canvas0_config[0].height,
		vfm->canvas0_config[1].width,
		vfm->canvas0_config[1].height);

	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_RD], vfm);
	if (pch->c.etype == 0) {	//vframe path
		itf_vfm_path_ready_notify(pch);
	}
}

void dpss_dbg_dis_o(unsigned int ch_idx)
{
	struct dpss_ch_s *pch;
	unsigned int ch, idx;

	ch = (ch_idx >> 8) & 0x3;
	idx = ch_idx & 0xf;

	pch = dpss_get_ch(ch);
	if (!pch) {
		DBG_ERR("%s:no input:%d\n", __func__, ch);
		return;
	}
	dpss_dis_idx(pch, idx);
}

struct vframe_s *dpss_get_vfm(unsigned int ch, unsigned int mode,
			      unsigned int idx)
{
	struct dpss_ch_s *pch;
	struct vframe_s *vfm = NULL, *vfm_in;

	struct dpss_nr_i_s *nr_i;

	pch = dpss_get_ch(ch);

	if (!pch || !pch->d) {
		DBG_ERR("%s:no input:%d\n", __func__, ch);
		return NULL;
	}
	if (idx >= DPSS_VFM_IN_NUB)
		return NULL;

	if (mode == 1) {	//input
		vfm = pch->d->vfm_in[idx];	//dbg only?
	} else if (mode == 2) {	// nr:
		vfm = &pch->d->vfm_nr[idx];
	} else if (mode == 3) {	//nr->in_in
		vfm_in = &pch->d->vfm_nr[idx];
		if (!vfm_in || !vfm_in->dpss_data)
			return NULL;

		nr_i = lk_get_nr_i(vfm_in);
		if (!nr_i)
			return NULL;
		vfm = nr_i->in_vfm;
	}
	return vfm;
}

void dpss_dbg_dis_in(unsigned int ch_idx)
{
	struct dpss_ch_s *pch;
	unsigned int ch, idx;
	struct vframe_s *vfm;

	ch = (ch_idx >> 8) & 0x3;
	idx = ch_idx & 0xf;

	pch = dpss_get_ch(ch);
	if (!pch || !pch->d) {
		DBG_ERR("%s:no input:%d\n", __func__, ch);
		return;
	}

	vfm = dpss_get_vfm(ch, 1, idx);
	if (vfm) {
		pch->d->vfm_dis = vfm;
		pch->d->vfm_dis_en = true;
	}
	DBG_INF("%s:ch[%d], idx[%d] vfm=0x%px\n", __func__, ch, idx, vfm);
	if (pch->c.etype == 0) {	//vframe path
		itf_vfm_path_ready_notify(pch);
	}
}

void dbg_hdr_int(unsigned int ch)
{
	struct dpss_ch_s *pch;
	struct vframe_s *vfm;
	unsigned int x = 0, y = 0;

	pch = dpss_get_ch(ch);
	if (!pch || !pch->d) {
		DBG_ERR("%s:no input:%d\n", __func__, ch);
		return;
	}

	vfm = dpss_get_vfm(ch, 1, 0);
	if (!vfm) {
		DBG_ERR("%s:vfm is null, do nothing\n", __func__);
		x = 192;
		y = 144;
	} else {
		x = vfm->width;
		y = vfm->height;
	}
	DBG_INF("x = %d, y = %d\n", x, y);
	dpss_hdr_int(x, y, 1);
}

void irq_rdma_dae(void *arg)
{
	struct dpss_rdma_arg_s *para;

	if (!arg)
		return;

	para = (struct dpss_rdma_arg_s *)arg;
	para->cnt++;
	dbg_a2("\t:ch[%d], %s handle[%d], cnt=%d\n", para->ch, __func__, para->handle,
	       para->cnt);
}

void irq_rdma_dpe_a(void *arg)
{
	struct dpss_rdma_arg_s *para;

	if (!arg)
		return;

	para = (struct dpss_rdma_arg_s *)arg;
	para->cnt++;
	dbg_a2("\t:%s ch[%d], handle[%d], cnt=%d\n", __func__, para->ch, para->handle,
	       para->cnt);
}

void irq_rdma_dpe_b(void *arg)
{
	struct dpss_rdma_arg_s *para;

	if (!arg)
		return;

	para = (struct dpss_rdma_arg_s *)arg;
	para->cnt++;
	dbg_a2("\t:ch[%d], %s handle[%d], cnt=%d\n", para->ch, __func__, para->handle,
	       para->cnt);
}

void irq_vpp_rdma1(void *arg)
{
	int i = 0;

	dbg_a2("%s %d\n", __func__, i);
}

void irq_vpp_rdma2(void *arg)
{
	int i = 0;

	dbg_a2("%s %d\n", __func__, i);
}

void dpss_nr_rdma_reg(struct dpss_ch_s *pch)
{
	int rdma_handle = -1;
	struct rdma_op_s *op_irq;
	struct dpss_rdma_arg_s *arg;
	unsigned int buf_nub, step, t_size;
	int i;

	//----------------------------------
	//op first:
	op_irq = &pch->c.op_dae;
	arg = &pch->c.arg_dae;
	arg->ch = pch->c.ch;
	arg->cnt = 0;
	op_irq->arg = arg;
	op_irq->irq_cb = irq_rdma_dae;
	dbg_a2("rdma dae:op_irq:%p,arg:%p\n", op_irq, op_irq->arg);

	for (i = 0; i < 2; i++) {
		op_irq = &pch->c.op_dpe[i];
		arg = &pch->c.arg_dpe[i];
		arg->ch = pch->c.ch;
		arg->cnt = 0;
		op_irq->arg = arg;
		if (i == 0)
			op_irq->irq_cb = irq_rdma_dpe_a;
		else
			op_irq->irq_cb = irq_rdma_dpe_b;
	}
	//----------------------------------

	buf_nub = 4;		//tmp
	step = DPSS_PRE_SIZE_DAE_D;
	t_size = buf_nub * step;

	op_irq = &pch->c.op_dae;
	rdma_handle = dpss_rdma_register(op_irq, NULL, buf_nub, step,
					 DPSS_RDMA_MODE_RD_WR);
	if (rdma_handle < 0) {
		DBG_WARN("%s:can't get 0\n", __func__);
		return;
	}
	pch->c.rdma_handle_dae = rdma_handle;
	arg = &pch->c.arg_dae;
	arg->handle = rdma_handle;

	//
	step = DPSS_PRE_SIZE_DPE_D;
	t_size = buf_nub * step;
	op_irq = &pch->c.op_dpe[0];
	rdma_handle = dpss_rdma_register(op_irq, NULL, buf_nub, step,
					 DPSS_RDMA_MODE_RD_WR);
	if (rdma_handle < 0) {
		DBG_WARN("%s:can't get 1\n", __func__);
		return;
	}
	pch->c.rdma_handle_dpe[0] = rdma_handle;
	arg = &pch->c.arg_dpe[0];
	arg->handle = rdma_handle;
	step = 0x200;
	t_size = buf_nub * step;
	op_irq = &pch->c.op_dpe[1];
	rdma_handle = dpss_rdma_register(op_irq, NULL, buf_nub, step,
					 DPSS_RDMA_MODE_RD_WR);
	if (rdma_handle < 0) {
		DBG_WARN("%s:can't get 2\n", __func__);
		return;
	}
	pch->c.rdma_handle_dpe[1] = rdma_handle;
	arg = &pch->c.arg_dpe[1];
	arg->handle = rdma_handle;

	//------------------------
	//vpp rdma:
	op_irq = &pch->c.vpp_dpe_op[0];
	op_irq->irq_cb = irq_vpp_rdma1;
	op_irq->arg = NULL;
	step = DPSS_PRE_SIZE_DPE_C;

	rdma_handle = rdma_register(op_irq, NULL, step);
	if (rdma_handle < 0) {
		DBG_WARN("%s:can't get vpp rdma?\n", __func__);
		pch->c.vpp_rdma_handle_dpe[0] = -1;
		return;
	}
	pch->c.vpp_rdma_handle_dpe[0] = rdma_handle;

	//------------------------
	pch->c.vpp_rdma_handle_dpe[1] = -1;
#ifdef FUNC_EN_VPP_RDMA_M
	//vpp rdma:
	op_irq = &pch->c.vpp_dpe_op[1];
	op_irq->irq_cb = irq_vpp_rdma2;
	op_irq->arg = NULL;
	buf_nub = 4;
	step = 0x200;

	rdma_handle = rdma_mbuf_register(op_irq, NULL, buf_nub, step);
	if (rdma_handle < 0) {
		DBG_WARN("%s:can't get vpp multi?\n", __func__);
		pch->c.vpp_rdma_handle_dpe[1] = -1;
		return;
	}
	pch->c.vpp_rdma_handle_dpe[1] = rdma_handle;

#endif
}

void dpss_nr_rdma_unreg(struct dpss_ch_s *pch)
{
	if (pch->c.rdma_handle_dae > 0)
		dpss_rdma_unregister(pch->c.rdma_handle_dae);

	if (pch->c.rdma_handle_dpe[0] > 0)
		dpss_rdma_unregister(pch->c.rdma_handle_dpe[0]);

	if (pch->c.rdma_handle_dpe[1] > 0)
		dpss_rdma_unregister(pch->c.rdma_handle_dpe[1]);

	if (pch->c.vpp_rdma_handle_dpe[0] > 0)
		rdma_unregister(pch->c.vpp_rdma_handle_dpe[0]);

	if (pch->c.vpp_rdma_handle_dpe[1] > 0) {
#ifdef FUNC_EN_VPP_RDMA_M
		rdma_mbuf_unregister(pch->c.vpp_rdma_handle_dpe[1]);
#endif
	}
	pch->c.rdma_handle_dae = -1;
	pch->c.rdma_handle_dpe[0] = -1;
	pch->c.rdma_handle_dpe[1] = -1;
	memset(&pch->c.op_dae, 0, sizeof(pch->c.op_dae));
	memset(&pch->c.op_dpe[0], 0, sizeof(pch->c.op_dpe[0]));
	memset(&pch->c.op_dpe[1], 0, sizeof(pch->c.op_dpe[1]));
	memset(&pch->c.arg_dae, 0, sizeof(pch->c.arg_dae));
	memset(&pch->c.arg_dpe[0], 0, sizeof(pch->c.arg_dpe[0]));
	memset(&pch->c.arg_dpe[1], 0, sizeof(pch->c.arg_dpe[1]));
}

void dpss_rdma_pre_tab_reg(struct dpss_ch_s *pch)
{
	struct dpss_rdma_pre_s *ptab;
	int i;

	if (!(dpss_en_rdma & C_BIT16))
		return;

	ptab = &pch->c.pre_dae_dpss;
	if (!ptab->buf) {
		memset(ptab, 0, sizeof(*ptab));
		ptab->buf = kzalloc(DPSS_PRE_SIZE_DAE_D, GFP_KERNEL);
		ptab->t_size = DPSS_PRE_SIZE_DAE_D;
		if (!ptab->buf)
			ptab->t_size = 0;
	}
	ptab = &pch->c.pre_dae_vc;
	if (!ptab->buf) {
		memset(ptab, 0, sizeof(*ptab));
		ptab->buf = kzalloc(DPSS_PRE_SIZE_DAE_C, GFP_KERNEL);
		ptab->t_size = DPSS_PRE_SIZE_DAE_C;
		if (!ptab->buf)
			ptab->t_size = 0;
	}
	ptab = &pch->c.pre_dpe_dpss;
	if (!ptab->buf) {
		memset(ptab, 0, sizeof(*ptab));
		ptab->buf = kzalloc(DPSS_PRE_SIZE_DPE_D, GFP_KERNEL);
		ptab->t_size = DPSS_PRE_SIZE_DPE_D;
		if (!ptab->buf)
			ptab->t_size = 0;
	}
	ptab = &pch->c.pre_dpe_vc;
	if (!ptab->buf) {
		memset(ptab, 0, sizeof(*ptab));
		ptab->buf = kzalloc(DPSS_PRE_SIZE_DPE_C, GFP_KERNEL);
		ptab->t_size = DPSS_PRE_SIZE_DPE_C;
		if (!ptab->buf)
			ptab->t_size = 0;
	}

	for (i = 0; i < 2; i++) {
		ptab = &pch->c.pre_tmp_a[i];
		if (!ptab->buf) {
			memset(ptab, 0, sizeof(*ptab));
			ptab->buf = kzalloc(DPSS_PRE_SIZE_DPE_D, GFP_KERNEL);
			ptab->t_size = DPSS_PRE_SIZE_DPE_D;
			if (!ptab->buf)
				ptab->t_size = 0;
		}
	}
	for (i = 0; i < 5; i++) {
		ptab = &pch->c.pre_tmp_b[i];
		if (!ptab->buf) {
			memset(ptab, 0, sizeof(*ptab));
			ptab->buf = kzalloc(0x200, GFP_KERNEL);
			ptab->t_size = 0x200;
			if (!ptab->buf)
				ptab->t_size = 0;
		}
	}
	dpss_nr_rdma_reg(pch);
}

void dpss_rdma_pre_tab_unreg(struct dpss_ch_s *pch)
{
	struct dpss_rdma_pre_s *ptab;
	int i;

	if (!(dpss_en_rdma & C_BIT16))
		return;
	ptab = &pch->c.pre_dae_dpss;
	if (!ptab->buf) {
		kfree(ptab->buf);
		ptab->buf = NULL;
		memset(ptab, 0, sizeof(*ptab));
	}

	ptab = &pch->c.pre_dae_vc;
	if (!ptab->buf) {
		kfree(ptab->buf);
		ptab->buf = NULL;
		memset(ptab, 0, sizeof(*ptab));
	}
	ptab = &pch->c.pre_dpe_dpss;
	if (!ptab->buf) {
		kfree(ptab->buf);
		ptab->buf = NULL;
		memset(ptab, 0, sizeof(*ptab));
	}

	ptab = &pch->c.pre_dpe_vc;
	if (!ptab->buf) {
		kfree(ptab->buf);
		ptab->buf = NULL;
		memset(ptab, 0, sizeof(*ptab));
	}
	for (i = 0; i < 2; i++) {
		ptab = &pch->c.pre_tmp_a[i];
		if (!ptab->buf) {
			kfree(ptab->buf);
			ptab->buf = NULL;
			memset(ptab, 0, sizeof(*ptab));
		}
	}
	for (i = 0; i < 5; i++) {
		ptab = &pch->c.pre_tmp_b[i];
		if (!ptab->buf) {
			kfree(ptab->buf);
			ptab->buf = NULL;
			memset(ptab, 0, sizeof(*ptab));
		}
	}
	dpss_nr_rdma_unreg(pch);
}

unsigned int dpss_force_rdma_src;
module_param_named(dpss_force_rdma_src, dpss_force_rdma_src, uint, 0664);

void dpss_rdma_pre_set(struct dpss_ch_s *pch)
{
	struct dpss_rdma_pre_s *ptab;
	unsigned int src_dae, src_dpe;

	if (pch->c.case_id < TST_CASE_IDX_1000)	//tmp
		return;
	if (!(dpss_en_rdma & C_BIT16)) {
		DBG_ERR("%s:not enable dpss_rdma\n", "pre_set");
		return;
	}

	if (pch->c.case_id == TST_CASE_IDX_1000) {
		//dae1 / dpe 1: //src
		src_dae = DPSS_RDMA_IRQ_SRC_DAE_FST_1;
		src_dpe = DPSS_RDMA_IRQ_SRC_DPE_FST_1;
	} else {		//tmp if (g_dpss_tst_case == TST_CASE_IDX_1001) {
		//dae 2 / dpe 2:
		src_dae = DPSS_RDMA_IRQ_SRC_DAE_FST_2;
		src_dpe = DPSS_RDMA_IRQ_SRC_DPE_FST_2;
	}
	if (dpss_force_rdma_src & 0x1f) {
		src_dae = 1 << (dpss_force_rdma_src & 0x1f);
		dbg_a0("%s:force src dae:0x%x\n", __func__, src_dae);
	}
	if (dpss_force_rdma_src & 0x1f00) {
		src_dpe = 1 << ((dpss_force_rdma_src >> 8) & 0x1f);
		dbg_a0("%s:force src dpe:0x%x\n", __func__, src_dae);
	}
	dpss_rdma_para_chg_irq_src(pch->c.rdma_handle_dae, src_dae);
	dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dae, 1);
	dpss_rdma_para_chg_irq_src(pch->c.rdma_handle_dpe[0], src_dpe);
	dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dpe[0], 1);

	if (pch->c.case_id == TST_CASE_IDX_1000 ||
	    pch->c.case_id == TST_CASE_IDX_1001) {
		//set table
		ptab = &pch->c.pre_dae_dpss;
		ptab->cnt = 0;
		//cfg_dae_triggle();
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0, 0x10);
		dpss_pre_tab_add_w_bits(ptab, VPU_DAE_WRAP_CTRL, 1, 20, 1);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 1, 0x20);
		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dae, 0, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dae);

		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 2, 0x30);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 3, 0x40);

		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 0, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0]);
	}
}

void dpss_rdma_pre_set_all(struct dpss_ch_s *pch)
{
	struct dpss_rdma_pre_s *ptab;
	unsigned int src_dae, src_dpe;
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = &pch->c.prm_dae;//&prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;//&prm_dpss_dpe;
	u8 shift, val, other;

	if (pch->c.case_id < TST_CASE_IDX_1000)	//tmp
		return;

	if (!(dpss_en_rdma & C_BIT16)) {
		DBG_ERR("%s:not enable dpss_rdma\n", "set all");
		return;
	}

	//irq src:
	if (pch->c.case_id == TST_CASE_IDX_1000) {
		//dae1 / dpe 1: //src
		src_dae = DPSS_RDMA_IRQ_SRC_DAE_FST_1;
		src_dpe = DPSS_RDMA_IRQ_SRC_DPE_FST_1;
	} else {		//tmp if (g_dpss_tst_case == TST_CASE_IDX_1001) {
		//dae 2 / dpe 2:
		src_dae = DPSS_RDMA_IRQ_SRC_DAE_FST_2;
		src_dpe = DPSS_RDMA_IRQ_SRC_DPE_FST_2;
	}
	if (dpss_force_rdma_src & 0x1f) {
		src_dae = 1 << (dpss_force_rdma_src & 0x1f);
		dbg_a0("%s:force src dae:0x%x\n", __func__, src_dae);
	}
	if (dpss_force_rdma_src & 0x1f00) {
		src_dpe = 1 << ((dpss_force_rdma_src >> 8) & 0x1f);
		dbg_a0("%s:force src dpe:0x%x\n", __func__, src_dae);
	}
	prm_top->frame_count = 0;
	dpss_rdma_para_chg_irq_src(pch->c.rdma_handle_dae, src_dae);
	dpss_rdma_para_chg_irq_src(pch->c.rdma_handle_dpe[0], src_dpe);

	if (pch->c.case_id == TST_CASE_IDX_1000) {
		dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dae, 1);	//tmp use 1 buffer
		dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dpe[0], 1);	//tmp use 1 buffer
		//set table dae:
		ptab = &pch->c.pre_dae_vc;	//this is only test for check if vc bus have data
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);

		ptab = &pch->c.pre_dae_dpss;
		ptab->cnt = 0;

		dpss_rdma_set_d_pre_tab(ptab);
		dpss_wr_sel |= C_BIT16;
		hw_cfg_dpss_dae(prm_top->dae_nr_mode, prm_top, prm_dae);
		if (pch->c.pre_dae_vc.cnt > 0) {
			DBG_WARN("%s:dae have vc bus wr? %d\n", __func__,
				 pch->c.pre_dae_vc.cnt);
		}
		//
		//cfg_dae_triggle();
		dpss_pre_tab_add_w_bits(ptab, VPU_DAE_WRAP_CTRL, 1, 20, 1);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 1, 0x20);
		dpss_wr_sel &= (~C_BIT16);

		dbg_a2("%s:dae:%d\n", __func__, pch->c.pre_dae_dpss.cnt);
		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dae, 0, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dae);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dae, 1, ptab);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dae, 2, ptab);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dae, 3, ptab);
		//dpe:
		ptab = &pch->c.pre_dpe_vc;	//this is only test for check if vc bus have data
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);

		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		dpss_rdma_set_d_pre_tab(ptab);
		dpss_wr_sel |= C_BIT16;
		hw_cfg_dpss_dpe(prm_top->dpe_nr_mode, prm_top, prm_dpe,
				&g_nr_pps_cfg);

		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 2, 0x30);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);
		dpss_wr_sel &= (~C_BIT16);
		dbg_a2("%s:dpe dpss size:0x%x\n", __func__, ptab->cnt);
		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 0, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0]);
		ptab = &pch->c.pre_dpe_vc;
		dbg_a2("%s:dpe vc size:0x%x\n", __func__, ptab->cnt);
		//copy:
		dpss_v_rdma_cp_tab(ptab, pch->c.vpp_rdma_handle_dpe[0]);
		//rdma_mbuf_config(pch->c.vpp_rdma_handle_dpe, 0, C_BIT23);//dpe rdma frm_rst
		rdma_config(pch->c.vpp_rdma_handle_dpe[0], C_BIT23);

		return;
	}

	if (pch->c.case_id == TST_CASE_IDX_1001) {	//for interlace
		dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dae, 1);	//use 1 buffer
		dpss_rdma_para_chg_used_nub(pch->c.rdma_handle_dpe[0], 4);	//use 4 buffer
		//set table
		ptab = &pch->c.pre_dae_vc;	//this is only test for check if vc bus have data
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);

		ptab = &pch->c.pre_dae_dpss;
		ptab->cnt = 0;

		dpss_rdma_set_d_pre_tab(ptab);
		dpss_wr_sel |= C_BIT16;

		hw_cfg_dpss_dae(DAE_VDI_MODE, prm_top, prm_dae);
		if (pch->c.pre_dae_vc.cnt > 0) {
			DBG_WARN("%s:dae have vc bus wr? %d\n", __func__,
				 pch->c.pre_dae_vc.cnt);
		}
		//cfg_dae_triggle();
		dpss_pre_tab_add_w_bits(ptab, VPU_DAE_WRAP_CTRL, 1, 20, 1);
		dpss_pre_tab_add_w(ptab, FRC_REG_TOP_RESERVE0 + 1, 0x20);
		dpss_wr_sel &= (~C_BIT16);

		dbg_a2("%s:dae:%d\n", __func__, pch->c.pre_dae_dpss.cnt);
		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dae, 0, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dae);

		//dpe:===============================================
		//~~~~ cfg tab~~~
		ptab = &pch->c.pre_tmp_a[0];	//vc
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);

		ptab = &pch->c.pre_tmp_a[1];	//dpss
		ptab->cnt = 0;
		dpss_rdma_set_d_pre_tab(ptab);
		dpss_wr_sel |= C_BIT16;
		hw_cfg_dpss_dpe(prm_top->dpe_di_mode, prm_top, prm_dpe,
				&g_nr_pps_cfg);

		//~~~~ bypass tab only dpss~~~
		ptab = &pch->c.pre_tmp_b[0];	//vc dbg only
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);

		ptab = &pch->c.pre_tmp_b[4];	//dpss bypass
		ptab->cnt = 0;
		dpss_rdma_set_d_pre_tab(ptab);
		dpss_vbe_proc_byp(1, 0);	//di
		if (pch->c.pre_tmp_b[0].cnt) {
			DBG_WARN("%s:case10001: bypass vc cnt =%d\n",
				 __func__, pch->c.pre_tmp_b[0].cnt);
		}
		dbg_a2("%s:case10001: bypass dpss cnt =%d\n",
		       __func__, pch->c.pre_tmp_b[4].cnt);
		//~~~~top ~~~
		ptab = &pch->c.pre_tmp_b[0];	//vc top
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);
		ptab = &pch->c.pre_tmp_b[1];	//dpss top
		ptab->cnt = 0;
		dpss_rdma_set_d_pre_tab(ptab);
		cfg_dpss_vdi_istop(2, prm_top->f_top);	//di_frm_cnt is = 2
		prm_dpe->dpss_dpe_di_frm_cnt = 2;	//dpss_dpe_di_frm_cnt; //yu.zong 2024-12-06
		hw_dpe_update_interlace(prm_top, prm_dpe);
		dpss_pre_tab_add_w(&pch->c.pre_tmp_b[1], DPSS_DPE_DIN_DMSQ_INIT,
				   0);
		dbg_a2("%s:case10001: top: dpss cnt =%d, vc =%d\n", __func__,
		       pch->c.pre_tmp_b[1].cnt, pch->c.pre_tmp_b[0].cnt);
		//~~~~~bottom~~~
		ptab = &pch->c.pre_tmp_b[2];	//vc
		ptab->cnt = 0;
		dpss_rdma_set_v_pre_tab(ptab);
		ptab = &pch->c.pre_tmp_b[3];	//dpss bottom
		ptab->cnt = 0;
		dpss_rdma_set_d_pre_tab(ptab);
		cfg_dpss_vdi_istop(3, prm_top->f_top);	//di_frm_cnt is = 2
		prm_dpe->dpss_dpe_di_frm_cnt = 3;	//dpss_dpe_di_frm_cnt; //yu.zong 2024-12-06
		hw_dpe_update_interlace(prm_top, prm_dpe);
		dpss_pre_tab_add_w(&pch->c.pre_tmp_b[3], DPSS_DPE_DIN_DMSQ_INIT,
				   0);
		dpss_wr_sel &= (~C_BIT16);
		dbg_a2("%s:case10001: bottom: dpss cnt =%d, vc =%d\n",
		       __func__,
		       pch->c.pre_tmp_b[3].cnt, pch->c.pre_tmp_b[2].cnt);
		//------------------------------------
		//merge table:
		//~~~frame 0:
		//~dpss~
		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[1]);	//cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[1]);	//top or bottom
		dpss_pre_tab_chg_bits(ptab, VPU_DI_HW_SLC_INFO, 1, 8, 1);	//only for first
		dpss_pre_tab_chg(ptab, DPSS_DPE_DIN_DMSQ_INIT, 1);
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[4]);	//bypass
		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);

		//:check fill null?:
		dpss_rdma_count_shift_val_ot(ptab->cnt, &shift, &val, &other);

		if (other)
			dpss_pre_tab_fill_null(ptab, other);

		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 0, ptab);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0], 0, ptab);
		//~vc~
		ptab = &pch->c.pre_dpe_vc;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[0]);	//cfg
		//      for vpp rdma cfg table:
		dpss_v_rdma_cp_tab(ptab, pch->c.vpp_rdma_handle_dpe[0]);
		rdma_config(pch->c.vpp_rdma_handle_dpe[0], C_BIT23);
#ifdef FUNC_EN_VPP_RDMA_M
		dbg_a2("set vpp m 1\n");
		//      vpp small
		ptab = &pch->c.pre_dpe_vc;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[0]);	//top or bottom
		//copy:
		dpss_v_rdma_m_cp_tab(ptab, 0, pch->c.vpp_rdma_handle_dpe[1]);
		rdma_mbuf_config(pch->c.vpp_rdma_handle_dpe[1], 0, C_BIT23);
#endif
		//~~~frame 1
		//~dpss~
		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[1]);	//cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[3]);	//bottom
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[4]);	//bypass
		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);

		//:check fill null?:
		dpss_rdma_count_shift_val_ot(ptab->cnt, &shift, &val, &other);
		dbg_a2("%s:dpe buf1:shift:%d,%d,%d\n", __func__, shift, val,
		       other);
		if (other)
			dpss_pre_tab_fill_null(ptab, other);

		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 1, ptab);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0], 1, ptab);
#ifdef FUNC_EN_VPP_RDMA_M
		dbg_a2("set vpp m 2\n");
		//~vc~
		ptab = &pch->c.pre_dpe_vc;
		ptab->cnt = 0;
		//dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[0]); //cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[2]);	//bottom
		//copy:
		dpss_v_rdma_m_cp_tab(ptab, 0, pch->c.vpp_rdma_handle_dpe[1]);
		rdma_mbuf_config(pch->c.vpp_rdma_handle_dpe[1], 1, C_BIT23);
#endif
		//~~~frame 2:
		//~dpss~
		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[1]);	//cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[1]);	//top or bottom
		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);
		//:check fill null?:
		dpss_rdma_count_shift_val_ot(ptab->cnt, &shift, &val, &other);

		if (other)
			dpss_pre_tab_fill_null(ptab, other);

		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 2, ptab);
		//dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0], 2, ptab);
#ifdef FUNC_EN_VPP_RDMA_M
		dbg_a2("set vpp m 3\n");
		//~vc~
		ptab = &pch->c.pre_dpe_vc;
		ptab->cnt = 0;
		//dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[0]); //cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[0]);	//top or bottom
		//copy:
		dpss_v_rdma_m_cp_tab(ptab, 0, pch->c.vpp_rdma_handle_dpe[1]);
		rdma_mbuf_config(pch->c.vpp_rdma_handle_dpe[1], 2, C_BIT23);
#endif
		//~~~frame 3:
		//~dpss~
		ptab = &pch->c.pre_dpe_dpss;
		ptab->cnt = 0;
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[1]);	//cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[3]);	//bottom
		//cfg_dpe_triggle(prm_top);
		dpss_pre_tab_add_w_bits(ptab, DPSS_DPE_START_CTRL, 1, 31, 1);
		//:check fill null?:
		dpss_rdma_count_shift_val_ot(ptab->cnt, &shift, &val, &other);

		if (other)
			dpss_pre_tab_fill_null(ptab, other);

		dpss_rdma_mbuf_fill_buf(pch->c.rdma_handle_dpe[0], 3, ptab);
		dpss_rdma_mbuf_config(pch->c.rdma_handle_dpe[0]);

#ifdef FUNC_EN_VPP_RDMA_M
		dbg_a2("set vpp m 4\n");
		//~vc~
		ptab = &pch->c.pre_dpe_vc;
		ptab->cnt = 0;
		//dpss_pre_tab_append(ptab, &pch->c.pre_tmp_a[0]); //cfg
		dpss_pre_tab_append(ptab, &pch->c.pre_tmp_b[2]);	//bottom
		//copy:
		dpss_v_rdma_m_cp_tab(ptab, 0, pch->c.vpp_rdma_handle_dpe[1]);
		rdma_mbuf_config(pch->c.vpp_rdma_handle_dpe[1], 3, C_BIT23);
#endif
	}
}

extern unsigned int dpss_bypass_ko;
void dpss_val_user_frc(struct dpss_ch_s *pch,
		struct dpss_user_para_s *prm_user,
		struct dpss_sub_vf_s *vfs)
{
	unsigned int val;
	unsigned int case_id;

	if (dpss_dbg_tst_case) {
		pch->c.case_id = dpss_dbg_tst_case;
	} else {
		if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC)
			pch->c.case_id = TST_CASE_IDX_0002;
	}
	dbg_i0("%s:tst case:%d\n", __func__, pch->c.case_id);
	//dpss_en_dct = 0;  // only frc set
	//dpss_en_hdr = 0;  // only frc set
	dpss_bypass_ko = 1;  // bypass nr ko
	case_id = pch->c.case_id;
	if (case_id == TST_CASE_IDX_0002) {	//frc only
		prm_user->dpss_mode = DPSS_FRC_MODE;
		prm_user->dae_nr_mode = DAE_FRC_MODE;	// DAE_BYPS_MODE;
		prm_user->dpe_nr_mode = DPE_MC_MODE;
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 0;
		dpss_int = IRQ_MODE_CASE_2;
	} else if (case_id == TST_CASE_IDX_0102) {	// SRC0 NR_FRC
		prm_user->dpss_mode = DPSS_FRC_NR_MODE;
		prm_user->dae_nr_mode = DAE_NR_MODE;
		prm_user->dpe_nr_mode = DPE_NR_MODE;	// ary 0113 DPE_NR_BYPS; // DPE_MC_MODE
		prm_user->dpe_di_mode = DPE_VDI_MODE;
		prm_user->src_mode = 0;
		dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;
	}
	if (vfs->width > DPSS_SLC_THD2_M)
		prm_user->cfg_slc = 1;
	else
		prm_user->cfg_slc = 0;
	//force:
	if (dpss_f_mode_get(0, &val))
		prm_user->dpss_mode = val;
	if (dpss_f_mode_get(1, &val))
		prm_user->dae_nr_mode = val;
	if (dpss_f_mode_get(2, &val))
		prm_user->dpe_nr_mode = val;
	if (dpss_f_mode_get(3, &val))
		prm_user->dpe_di_mode = val;
	if (dpss_f_mode_get(4, &val))
		prm_user->src_mode = val;
	if (dpss_f_mode_get(5, &val))
		prm_user->cfg_slc = val;
	dbg_i1("%s:%d,%d,%d,%d,%d,%d\n",
	       __func__, prm_user->dpss_mode,
	       prm_user->dae_nr_mode,
	       prm_user->dpe_nr_mode,
	       prm_user->dpe_di_mode, prm_user->src_mode, prm_user->cfg_slc);
}

