// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/**********************************************************************************
 * Copyright (c) 2024, AMLOGIC Inc.
 * All rights reserved
 **********************************************************************************
 * File       : dpss.c
 * Author     :
 * Created    : 2024-01-01
 * Description:
 **********************************************************************************
 * |->cfg_dpss_top             @xianjun @lusen.chai
 *    |->cfg_dpss_tbc_frc
 *    |->cfg_dpss_size_frc
 *    |->dpss_phs_lut_ini
 *    |->cfg_dpss_buff_addr
 *    |->cfg_dpss_tbc_vdi
 * |->cfg_dpss_inp             @ruanjian
 * |->cfg_dpss_dae             @jinsen.yang @yi.zhang
 * |->cfg_dpss_dpe             @tao.yao
 *       |->cfg_dpss_dpe_intf  @menglong
 *    |->cfg_dpss_mc           @yu.zhong @tao.yao
 *    |->cfg_dpss_vdi          @yu.zhong @lusen.chai
 *       |->cfg_dpss_vdi_slice
 * |->cfg_dpss_intf       @menglong
 *	|->vfcd          @dandan.lv
 *	!->vfce          @xinchang
 *********************************************************************************/

#include "define.h"
#include "dpss.h"
#include "dpss_phslut.h"
#include "dpss_mc.h"
#include "../dpss/dpss_hw.h"
#include "../dpss/dpss_hw_frc.h"
#include "../dpss/sys_def.h"
#include <linux/amlogic/media/registers/cpu_version.h>

unsigned int nr_aepe_buf_num_dbg = 3;
module_param_named(nr_aepe_buf_num_dbg, nr_aepe_buf_num_dbg, uint, 0664);

static void hw_cfg_dpss_size_frc(struct PRM_DPSS_TOP *prm_top);
static void hw_cfg_dpss_tbc_frc(struct PRM_DPSS_TOP *prm_top,
	unsigned int frc_nr_en);
static void hw_cfg_dpss_tbc_vdi(struct PRM_DPSS_TOP *prm_top, u32 vdi_nr_en);
static void hw_cfg_dpss_buff_addr(struct PRM_DPSS_TOP *prm_top,
	enum DPSS_WORK_MODE dpss_mode, u32 src0_type,
	u32 src1_type, u32 path_id);
//static void hw_init_dpss_dpe_obuf(void);
static void hw_init_dpss_dpe_out_tbc(unsigned int src_mode);

struct PRM_SRC_FMT fmt_cfg(enum vid_src_fmt src_fmt,
	enum vid_src_fmt src_plan, enum vid_src_fmt src_bit,
	enum vid_src_fmt src_cmpr, enum vid_src_fmt interlace)
{
	struct PRM_SRC_FMT  prm_fmt;

	prm_fmt.src_fmt = src_fmt;
	prm_fmt.src_plan = src_plan;
	prm_fmt.src_bit = src_bit;
	prm_fmt.src_cmpr = src_cmpr;
	prm_fmt.interlace = interlace;
	return prm_fmt;
}

void set_dpss_path_mode(struct PRM_DPSS_TOP *prm_top)
{
	enum PRM_SRC_IDX src_idx = 0;

	switch (prm_top->dpss_mode) {
	case DPSS_FRC_MODE: {
		src_idx = SRC_IDX_FRC; break;
	}
	case DPSS_FRC_NR_MODE: {
		src_idx = SRC_IDX_FRC; break;
	}
	case DPSS_NR_SRC0_MODE: {
		src_idx = SRC_IDX_NR;  break;
	}
	case DPSS_FRC_NRBYPS_MODE: {
		src_idx = SRC_IDX_FRC; break;
	}
	case DPSS_DI_MODE: {
		src_idx = SRC_IDX_DI0; break;
	}
	case DPSS_NRDI_MODE: {
		src_idx = SRC_IDX_DI0; break;
	}
	case DPSS_NR_SRC1_MODE: {
		src_idx = SRC_IDX_DI0; break;
	}
	default: {
		DBG_ERR("Error: dpss_mode=%d wrong !!!\n", prm_top->dpss_mode);
		break;
		};
	}

	dbg_h2("%s:src_idx=%d\n", __func__, src_idx);
	//0-4: for dpss path mode,  slc_num[27:24], src_idx[31:28]
	//send_event_to_sv(enum DPSS_WORK_MODE_EVENT, (prm_dpss_top.dpss_mode |
	//(prm_dpss_top.dpe_slc_num << 24) | (src_idx << 28)));
	stimulus_wait_event_done(DPSS_WORK_MODE_EVENT);
}

void hw_cfg_dpss_top(struct PRM_DPSS_TOP *prm_top)
{
	//==============================================================//
	//path mode
	//==============================================================//
	u32 frc_nr_en = 0;
	u32 vdi_nr_en = 0;
	u32 src0_nrdi_frc_en = 0; //2 means NRDI use NR tbc

	dbg_h1("%s st\n", __func__);
	enum DPSS_WORK_MODE dpss_mode = prm_top->dpss_mode;

	switch (dpss_mode) {
	case DPSS_FRC_MODE:
		{frc_nr_en = 0x2; vdi_nr_en = 0x0; break; }
	case DPSS_FRC_NR_MODE:
		{frc_nr_en = 0x3; vdi_nr_en = 0x0; break; }
	case DPSS_NR_SRC0_MODE:
		{frc_nr_en = 0x1; vdi_nr_en = 0x0; break; }
	case DPSS_FRC_NRBYPS_MODE:
		{frc_nr_en = 0x3; vdi_nr_en = 0x0; break; }
	case DPSS_DI_MODE:
		{frc_nr_en = 0x0; vdi_nr_en = 0x2; break; }
	case DPSS_NRDI_MODE:
		{frc_nr_en = 0x0; vdi_nr_en = 0x3; break; }
	case DPSS_NR_SRC1_MODE:
		{frc_nr_en = 0x0; vdi_nr_en = 0x1; break; }

	//	case DPSS_DV_SRC0_MODE:
	//{frc_nr_en = 0x4; vdi_nr_en = 0x0; break;}
	//dv_di_parallel mode(no nr)
	//	case DPSS_FRC_DV_PRL_MODE:
	//{frc_nr_en = 0x6; vdi_nr_en = 0x0; break;}//
	case DPSS_NRDI_SRC0_MODE: {
		frc_nr_en = 0x1; vdi_nr_en = 0x0; src0_nrdi_frc_en = 1;
		break;
	}
	case DPSS_NRDI_FRC_SRC0_MODE: {
		frc_nr_en = 0x3; vdi_nr_en = 0x0; src0_nrdi_frc_en = 1;
		break;
	}
	case DPSS_DI_FRC_SRC0_MODE: {
		frc_nr_en = 0x3; vdi_nr_en = 0x0; src0_nrdi_frc_en = 1;
		break;
	}
	default:
		DBG_ERR("%s: dpss_mode=%d wrong !!!\n", __func__,
		dpss_mode);
		break;
	}

	dbg_h1("prm_top->dolby_mode:%d\n", prm_top->dolby_mode);
	dbg_h1("\tdpss_mode=%d\n", dpss_mode);
	dbg_h1("\tvdi_nr_en=%d\n", vdi_nr_en);
	dbg_h1("\tfrc_nr_en=%d src0_nrdi_frc_en = %d\n",
		frc_nr_en, src0_nrdi_frc_en);
	bool src0_dv_mode = (vdi_nr_en == 0) &
		(prm_top->dolby_mode >= DOLBY_DPSS_MODE) ||
	prm_top->bbd_parallel;
	//ary no use bool src1_dv_mode =
	//(frc_nr_en==0)&(prm_top->dolby_mode>=DOLBY_DPSS_MODE);
	bool amdv_prl_en = (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE);

	frc_nr_en = (src0_dv_mode << 2) |
		(amdv_prl_en ? frc_nr_en & 0x2 : frc_nr_en);
	//set_need_check prm_top->dpe_mc_phsx2 = 0;//v3 mc only support 0 //
	bool src0_path_en = frc_nr_en > 0;
	bool src1_path_en = vdi_nr_en > 0;
	bool src0_fnr_mode = ((frc_nr_en & 0x3) == 1); //only nr
	bool src0_frc_mode = ((frc_nr_en & 0x2) == 2); //nr+frc or only nr
	//bool src0_dv_mode = prm_top->dolby_mode == DOLBY_DPE_FRM_MODE;//onlydv
	bool me_mc_link_en = prm_top->me_mc_link_en;
	bool me_game_mode_en = prm_top->dpe_game_mode;
	//==============================================================//
	// dpss_top
	//==============================================================//
//#if 1	//init by xianjun 2024-12-06
	w_reg_bit(FRC_DAE_FRM_DLY_NUM, 2, 0, 3);

	if (dpss_slt_mode)
		w_reg_bit(DPSS_FBUF_DAE_INIT, 2, 8, 5);
	else
		w_reg_bit(DPSS_FBUF_DAE_INIT, 0, 8, 5);
	//w_reg_bit(DPSS_REGOFST_VDICTRL+DPSS_FBUF_DAE_INIT, dpss_di_ignore_num ,8,5);
	w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 1, 0, 1);
//#endif
	//ary add 2025-0314 for rdma
	w_reg_bit(DPSS_TOP_INT_RDMA_MODE_CTRL, 0x3f, 24, 6);
	//04-15 for rdma
	w_reg_bit(DPSS_RDMA_INT_MODEL_SEL, 1, 0, 1);
	w_reg_bit(DPSS_RDMA_DONE_FLAG_SEL, 0, 0, 16);
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6W) {
		wr(VPU_DAE_WRAP_GCLK_CTRL0, 0xa);
		wr(VPU_DAE_WRAP_GCLK_CTRL1, 0xa);//close gate clock from vlsi dae stuck
	}

	//just for get register for check
	w_reg_bit(DPSS_DPE_INTF_CTRL0,
		((prm_top->dpe_dw_dsy & 0x3) << 2) |
		((prm_top->dpe_dw_dsx & 0x3) << 0), 0, 4);
	w_reg_bit(DPSS_DPE_INTF_CTRL1,
		((prm_top->dpe_dw_dsy & 0x3) << 2) |
		((prm_top->dpe_dw_dsx & 0x3) << 0), 0, 4);
	dbg_h2("<dpss.c> src0_path_en=%d src1_path_en = %d\n",
		src0_path_en, src1_path_en);
	if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE &&
	    prm_top->src_mode == 0) {
		dbg_h2("dae bypass_mode\n");
		w_reg_bit(FRC_AUTO_MODE_INP_NR_LINK, 1, 0, 1);	//
		w_reg_bit(DPSS_FBUF_TOP_CTRL, 1, 6, 1);//reg_fbuf_enable
		dbg_h1("nr_by_test DPSS_FBUF_TOP_CTRL 0x83d0 bit 6\n");
	} else if (prm_top->dolby_mode != DOLBY_DPSS_PRL_MODE) {
		w_reg_bit(DPSS_FBUF_TOP_CTRL, 0, 6, 1);
	}
	if (src0_path_en) {
		hw_cfg_dpss_size_frc(prm_top);
		hw_cfg_dpss_tbc_frc(prm_top, frc_nr_en);
		w_reg_bit(DPSS_TOP_FUNC_CTRL, src0_nrdi_frc_en,
			5, 1);//wr reg_frc_nr_en[1], src0 include di
		w_reg_bit(DPSS_TOP_PER_DLY_CTRL,
			src0_nrdi_frc_en, 0,
			1); //wr reg_frc_nr_en[1], src0 include di
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 3, 8,
			2); //dpe frm_int 0 for frc done
		hw_cfg_dpss_buff_addr(prm_top, dpss_mode,
			src0_nrdi_frc_en, 1, 0);

		w_reg_bit(FRC_DPSS_VPP_LINK, 0, 0, 1); //add tmp here
		if (src0_frc_mode) {
			w_reg_bit(FRC_DPSS_VPP_LINK, 1, 0,
				1);//frc_vpp_link
			w_reg_bit(FRC_DPSS_LLM, me_game_mode_en << 1 |
				me_mc_link_en, 0,
			2);//me_game_mode_en,frc_me_mc_link
			w_reg_bit_vc(VPU_AXIRD_LINE_CRTL0,
				me_game_mode_en * 15, 28,
				4); //reg_line_sync_en vfcd
			hw_cfg_dpss_mc_ini(prm_top, (frc_nr_en & 0x1) || amdv_prl_en);

			if (prm_top->dpe_game_mode) {
				wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883180);
				//add inp->me->mc
			} else {
				wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x76543210);
				//add for nr+frc setting
			}

			wr(FRC_INP_GAME_MODE,
				prm_top->dpe_game_mode);//reg_inp_game_mode
			w_reg_bit(FRC_DPSS_LLM, prm_top->dpe_game_mode,
				3, 1); //reg_inp_game_mode

		} else if (src0_fnr_mode || src0_dv_mode) {
			if (prm_top->nr_vpp_link_en == 0) {
				wr(DPSS_NR_VPP_LINK, 0);//no nr_vpp_link
				w_reg_bit(DPSS_DPE_HW_DBG, 1, 13,
					1); //mask nr link done
			} else {
				wr(DPSS_NR_VPP_LINK, 1);//nr_vpp_link
				w_reg_bit(DPSS_DPE_HW_DBG, 0, 13,
					1); //no mask nr link done

				if (src0_nrdi_frc_en == 1) {
					//nr_vpp_link + src0_nrdi_en -> src0 di link
					w_reg_bit(VPU_AXIRD_PATH_CTRL, 4, 0,
						3);
					//reg_di_vpp_link,reg_nr_vpp_link,reg_vd1_vpp_link

				} else {
					w_reg_bit(VPU_AXIRD_PATH_CTRL, 2, 0,
					3);
					//reg_di_vpp_link,reg_nr_vpp_link,reg_vd1_vpp_link
				}

				u32 dpss_nr_vpp_link = rd(DPSS_NR_VPP_LINK);

				cfg_dpss_dpe_nr_link_out(dpss_nr_vpp_link & 0x1);
				wr(DPSS_DPE_NR_FRM_EN_SEL,
					1); //frm_rst/en sel of nrtop
				u32 src_hsize;
				u32 src_vsize;

				if (prm_top->auto_alig_en &&
					prm_top->org_hsize != 0xffff) {
					src_hsize = prm_top->org_hsize;
					src_vsize = prm_top->org_vsize;
				} else {
					src_hsize = prm_top->frm_hsize;
					src_vsize = prm_top->frm_vsize;
				}

				wr(DPSS_DPE_DIN_SEL_SIZE0,
					src_hsize << 16 | //src hsize
					src_vsize);//src vsize
				wr(DPSS_DPE_DIN_SEL_SIZE1,
					prm_top->frm_hsize << 16 | //frm hsize
					prm_top->frm_vsize); //frm vsize
				wr(DPSS_DPE_DIN_SEL_SIZE2,
					prm_top->frm_hsize << 16 | //dv src hsize
					prm_top->frm_vsize);//dv src vsize
				wr(DPSS_DPE_DIN_SEL_SIZE3,
					prm_top->frm_hsize << 16 | //dv frm hsize
					prm_top->frm_vsize);//dv frm vsize
				dbg_h1("\tnr vpp link --- frm_hsize: %d\n",
				prm_top->frm_hsize);
				dbg_h1("\tnr vpp link --- frm_vsize: %d\n",
				prm_top->frm_vsize);

				u32 func_en = (src0_nrdi_frc_en << 1) | 1;
				u32 dpe_src_idx =  src0_nrdi_frc_en ? 9 : 7;

				wr(DPSS_DPE_DIN_SEL_CTRL0,
					0 << 0 | //reg_clr_dpe_flag
					dpe_src_idx << 4 | //dpe_src_idx, when src0_nrdi
					1 << 8 | //me_mvy_div_mode, nouse
					1 << 10 | //me_mvx_div_mode, nouse
					0 << 14 | //mc_gcb_bypass_mc_en, nouse
					1 << 17 | //fnr loop
					0 << 18 | //dv loop, nouse
					0 << 19 | //mc loop
					0 << 20 | //di loop
					func_en << 21 | //func_en
					0 << 28); //mv_blk_mode, nouse
				wr(DPSS_DPE_TOP_SW_SEL0,
					0xfffffffc);
				//din sel0 //w_reg_bit(DPSS_DPE_TOP_SW_SEL0,0x0,9,14);
				wr(DPSS_DPE_TOP_SW_SEL1, 0xff7fffff);//din sel1
				wr(DPSS_DPE_TOP_SW_SEL2, 0xffffffff);//din sel2
			}
			wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88882808);
		} else {
			DBG_ERR("%s:src0_path_en:%d,%d,%d !!!\n",
				__func__,
				src0_frc_mode, src0_fnr_mode, src0_dv_mode);
			return; //ary add this return
		}

		if ((src0_nrdi_frc_en & 0x1) == 1) {
			if (src0_frc_mode) {
				//wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883208);
				//src0 nrdi frc //reg_doutxn_ibuf_sel //no inp
				wr(FRC_DPSS_TBC_MUX_DOUT_SEL,
					0x88883102);
				//src0 nrdi frc //reg_doutxn_ibuf_sel  //have inp
			} else if (src0_fnr_mode) {
				wr(FRC_DPSS_TBC_MUX_DOUT_SEL,
					0x88882808); //src0 nrdi
			} else {
				DBG_WARN("%s:Check src0_nrdi_frc !!!\n",
					__func__);
			}
		}
	} else if (src1_path_en) {
		hw_cfg_dpss_tbc_vdi(prm_top, vdi_nr_en);
		hw_cfg_dpss_buff_addr(prm_top, dpss_mode, 0, 1, 1);
	} else {
		DBG_ERR("%s:Error: tbc_path_en wrong !!!\n", __func__);
		return;
	}
	if (prm_top->nr_vpp_link_en == 0)
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 13, 1); //mask nr link done

	//==============================================================//
	//Share ram config
	//==============================================================//
	u32 dpss_frc_vpp_link = rd(FRC_DPSS_VPP_LINK);

	if (dpss_frc_vpp_link)
		wr(DPSS_DPE_NR_PRE_LINK, 0);//share ram give mc
	else
		wr(DPSS_DPE_NR_PRE_LINK, 1);//share ram give nrdi

	//==============================================================//
	// Close reg_low_latency_mode for bitmatch
	//==============================================================//
#ifdef MOV // ndef DPSS_FPGA_DRV
	w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 0, 0, 1);//reg_low_latency_mode
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_LOW_LATENCY_MODE, 0, 0,
		1); //reg_low_latency_mode
#else	//init by xianjun 2024-12-06
	w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 1, 0, 1);//reg_low_latency_mode
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_LOW_LATENCY_MODE, 1, 0,
		1); //reg_low_latency_mode
#endif
	//==============================================================//
	// dpss_top:inp/dae/dpe triggle mode auto/manual
	//==============================================================//
	if (!prm_top->src_mode) { //07-10 add for 2ch nr+frc +i
	//trigger frm_en by config_done_signal
		w_reg_bit(FRC_REG_INP_FRM_CTRL, 1, 4, 1);
		dbg_h2("patch 0523\n");
	}
	//reg_inp_start_sel  0:hold_line 1:pls
	w_reg_bit(VPU_DAE_WRAP_CTRL, 1, 0, 1);
	//reg_dae_start_sel  0:hold_line 1:pls
	w_reg_bit(DPSS_DPE_START_CTRL, 1, 0, 1);
	if (!prm_top->src_mode) { //07-10 add for 2ch nr+frc +i
	//reg_dpe_start_sel  0:hold_line 1:pls
		w_reg_bit(DPSS_TOP_SYNC_RST, 1, 5, 1);
		dbg_h2("patch 0524\n");
	}
	//pls_ctrl_syn_rst  //for some regs latch

	//w_reg_bit(DPSS_TOP_SYNC_RST,1,0,1);//pls_top_syn_rst
	//for some reg_latch
	if (src0_path_en) {
		w_reg_bit(DPSS_TOP_SYNC_RST, 1, 7, 1);
		//pls_src0_syn_rst  //for some regs latch
	} else if (src1_path_en) {
		w_reg_bit(DPSS_TOP_SYNC_RST, 1, 8, 1);
		//pls_src1_syn_rst  //for some regs latch
	}

	//==============================================================//
	// GAME_MODE
	//==============================================================//
	if (me_game_mode_en) {
		//12-05 w_reg_bit(VPU_DAE_WRAP_GCLK_CTRL0,0x2,2,2);
		//dae_idel_clk_en
		//12-05 w_reg_bit(FRC_REG_INP_GCLK_CTRL,0x2,30,2);
		//inp_idel_clk_en
		cfg_dpss_gmd_phs_lut(prm_top->frc_ratio, 1);
	}

	if (prm_top->sw_ctrl_en) {
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 0x3fff, 0,
		14);//irq sel {dpe,dae,inp} done
		wr(FRC_INP_DIN_SW_SEL,
			0xffffffff);//reg_dpe_inp_en
		wr(VPU_DAE_WRAP_SW_SEL0,
			0xffffffff);//reg_dae_sw_en
		wr(VPU_DAE_WRAP_SW_SEL1, 0xffffffff);
		wr(DPSS_DPE_TOP_SW_SEL0,
			0xffffffff);//reg_dpe_sw_en0
		wr(DPSS_DPE_TOP_SW_SEL1, 0xffffffff);
		wr(DPSS_DPE_TOP_SW_SEL2, 0xffffffff);
	}

	//==============================================================//
	// FNR sw mode
	//==============================================================//
	//12-05 if(prm_top->fnr_sw_mode == 1 || prm_top->vdi_sw_mode == 1){
	if (prm_top->sw_tbc_mode == 1) { //12-05
		//use frm done int instead of frm_en int
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 0x1555, 0, 16);
		//irq sel {dpe,dae,inp} done
		//
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, 2, 0, 2); //fnr_sw_drv mode
		w_reg_bit(DPSS_VDI_SW_DRV_CTRL1, 2, 0,
			2); //vdi_sw_drv mode
		w_reg_bit(DPSS_NR_SW_DRV_CTRL0, 2, 0, 2);
		//nr_dae_dpe_sw_drv mode

		w_reg_bit(FRC_INP_SW_CTRL0, 2, 30, 2);
		w_reg_bit(FRC_DAE_SW_CTRL0, 2, 30, 2);
		w_reg_bit(FRC_OBUF_SW_CTRL0, 2, 30, 2);
		//bit 30 change to 0 , from 2 to 0
		w_reg_bit(FRC_REG_INP_FRM_CTRL, 0, 4, 1);
		//inp start sel change to auto mode
		w_reg_bit(FRC_AUTO_MODE_BUFF_NUM, 8, 0, 5);
		//inp ibuf num
		//hw_init_dpss_dpe_obuf();
		hw_init_dpss_dpe_out_tbc(prm_top->src_mode);
	}
	dbg_h2("%s:tbc:%d\n", __func__, prm_top->sw_tbc_mode);

	w_reg_bit(FRC_AUTO_MODE_PHS_INIT, 1, 0, 1); //pls_dae_loop_init_en

	//==============================================================//
	// DV-NRDI parallel
	//==============================================================//
	if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE) {
		//DV use wmif2/vfce and ds wrmif
		w_reg_bit(DPSS_DPE_TOP_CTRL0, 0xc, 24, 4);
		w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 1, 0, 1);
		//reg_low_latency_mode
	} else {
		w_reg_bit(DPSS_DPE_TOP_CTRL0, 0x0, 24, 4);
	}

	if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE	&&
	    prm_top->src_mode == 0) {
		wr(DPSS_FBUF_FSIZE, prm_top->amdv_frm_vsize << 16 |
					prm_top->amdv_frm_hsize);
		wr(DPSS_FBUF_FSIZE_CFG, prm_top->amdv_org_vsize << 16 |
					prm_top->amdv_org_hsize);
		w_reg_bit(DPSS_DPE_TOP_CTRL0, prm_top->amdv_slc_num, 20, 3);
		w_reg_bit_vc(VPU_DOLBY_WRAP_CTRL, 1, 31, 1);
		wr(DPSS_DAE_BYP_SIZE, 1920 << 16 | 1080);
		dbg_h1("nr_by_test fbuf_size: 0x%x\n", rd(DPSS_FBUF_FSIZE));
		dbg_h1("nr_by_test fbuf_size_cfg: 0x%x\n", rd(DPSS_FBUF_FSIZE_CFG));
		dbg_h1("nr_by_test dv slice bit22:20: 0x%x\n", rd(DPSS_DPE_TOP_CTRL0));
		dbg_h1("nr_by_test dv pad: 0x%x\n", rd(DPSS_DPE_PAD));
	}
	w_reg_bit(DPSS_DPE_INTF_AFBCD0, 7, 27, 3);
	//dv,di,nr wr back on
	//==============================================================//
	// dpss_top:end
	//==============================================================//
	dbg_h1("%s: sw_tbc_mode %d end\n", __func__, prm_top->sw_tbc_mode);

#ifdef MOV //ary 2025-03-23
	//--------------------------------------------------------------
	//0xExxx_xxxx address invalid in SYS TB
	//--------------------------------------------------------------
	wr(DPSS_SRC1_INP_YHEAD_ADDR,
		PIC_DI_HEADY_BADDR >> 4); //12-05: ary need check
	wr(DPSS_SRC1_INP_CHEAD_ADDR,
		PIC_DI_HEADC_BADDR >> 4);
	wr(DPSS_SRC1_NROUT_YHEAD_ADDR,
		DI_NR_HEADY_BADDR  >> 4);
	wr(DPSS_SRC1_NROUT_CHEAD_ADDR,
		DI_NR_HEADC_BADDR  >> 4);
	wr(DPSS_SRC1_DIOUT_YHEAD_ADDR,
		DI_OUTC_YHAED      >> 4);
	wr(DPSS_SRC1_DIOUT_CHEAD_ADDR,
		DI_OUTC_CHAED      >> 4);
	wr(DPSS_SRC1_DI_MERO_ADDR,
		DI_MERO_BADDR      >> 4);
	wr(DPSS_SRC1_DPE_RDMA_ADDR,
		SRC1_DPE_RDMA_BADDR >> 4);
	//--------------------------------------------------------------
#endif
}

//cfg_dpss_tbc_frc
void hw_cfg_dpss_tbc_frc(struct PRM_DPSS_TOP *prm_top,
	unsigned int frc_nr_en)
{
	enum DPSS_FILM_MODE film_mode = prm_top->film_mode;
	enum DPSS_FRC_RATIO frc_ratio = prm_top->frc_ratio;//DPSS_FRC_RATIO_2_5

	w_reg_bit(FRC_REG_MODE_OPT, 1, 0, 1); //reg_cfg_syn_mode //12-05
	dbg_h1("%s frc_nr_en=%d\n", __func__, frc_nr_en);

	if (film_mode == DPSS_FILM_PD)
		film_mode = DPSS_VIDEO;

	u32 reg_phsx2x_mode_en = prm_top->dpe_mc_phsx2;  // 1bit
	bool reg_inp_ibuf_cfg_en = 1;
	//(prm_top.film_mode != DPSS_VIDEO);
	//video mode set 0, video2filme or film set 1
	//DPSS_FILM_PD/VIDEO mode  = 0, direct film 1
	bool reg_film_phs_test = ((prm_top->film_mode != DPSS_VIDEO) &&
	(prm_top->film_mode != DPSS_FILM_PD));
	//1bit  0 use detect film phase, 1 use calculate film phase
	bool inp_ofrm_idx_sel = ((prm_top->film_mode != DPSS_VIDEO) &&
	(prm_top->film_mode != DPSS_FILM_PD));
	//1bit, 0 use detect film phase, 1 use calculate film phase
	u32 tbc_auto_mode = 1;//1bit, 0: phase tab ctrl, 1: automatic ctrl
	//ary no use	u32 inp_nr_link_mode = 1; //1bit frcnr_me_share_mode
	u32 inp_ibuf_num;
	//16; //(prm_top.film_mode == DPSS_FILM_PD) ? 16 : 4;
	u32 inp_obuf_num;//6
	u32 nr_dae_ibuf_num;//6;
	u32  nr_aepe_buf_num;//?
	u32  nr_dpe_obuf_num;//6;
	u32  frc_dae_ibuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6
	u32  frc_dae_obuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6
	u32  frc_dpe_obuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6
	u32 num_dblk, num_wrbk, num_nrdi_wrpt, num_nr_me_ro, num_dpe_ro;
	u32  reg_frc_src_idx = 1; // 4bit //todo ?
	enum DPSS_WORK_MODE dpss_mode = prm_top->dpss_mode;
	bool frc_tbc_en = (frc_nr_en >> 1) & 0x1; //todo
	bool nr_tbc_en = ((frc_nr_en & 0x4) && (prm_top->bbd_parallel == 0)) |
		(frc_nr_en & 0x1); //|(inp_nr_link_mode&0x1);
	//[12]vdi_byp_mode, [8]frc_byp_mode, [4]fnr_byp_mode, [0]inp_byp_mode,
	bool frc_byp_mode = (dpss_mode == DPSS_NR_SRC0_MODE);
	bool fnr_byp_mode = (dpss_mode == DPSS_FRC_MODE) && (!nr_tbc_en);
	bool inp_byp_mode = frc_byp_mode;

	if (dpss_mode == DPSS_FRC_MODE) {
		inp_ibuf_num = DPSS_SML_FRC_NUB; //16;//10-2025-4-17
		inp_obuf_num = DPSS_SML_FRC_NUB; //16;
		nr_dae_ibuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6;
		nr_aepe_buf_num = nr_aepe_buf_num_dbg;//?
		nr_dpe_obuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6;
		num_dblk = nr_dpe_obuf_num;
		num_wrbk = nr_dpe_obuf_num;
		frc_dae_ibuf_num = DPSS_SML_FRC_NUB; //16;
		frc_dae_obuf_num = DPSS_SML_FRC_NUB; //6;
		frc_dpe_obuf_num = DPSS_SML_FRC_NUB; //16;
	} else {
		inp_ibuf_num = prm_top->num_in;//DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		inp_obuf_num = prm_top->num_in;//DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		nr_dae_ibuf_num = prm_top->num_in;//DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6;
		nr_aepe_buf_num = prm_top->num_aepe;//nr_aepe_buf_num_dbg;//
		nr_dpe_obuf_num = prm_top->num_dpe_o; //DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6;
		frc_dae_ibuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		frc_dae_obuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		frc_dpe_obuf_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		num_dblk = prm_top->num_dblk;
		if (prm_top->is_i)
			num_wrbk = prm_top->num_lc;
		else
			num_wrbk = nr_dpe_obuf_num;
	}
	num_nrdi_wrpt = prm_top->num_nr_wrpt;
	num_nr_me_ro = prm_top->num_nr_me_ro;
	num_dpe_ro = prm_top->num_nr_me_ro;

	w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, frc_byp_mode, 8, 1);
	//reg_frc_byp_mode
	w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, fnr_byp_mode, 4, 1);
	//reg_fnr_byp_mode
	w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, inp_byp_mode, 0, 1);
	//reg_inp_byp_mode
	w_reg_bit(FRC_TOTAL_NUM_MODE, frc_byp_mode, 0, 1);
	//DPSS_TOP_FUNC_BYP_CTRL,reg_frc_byp_mode
	w_reg_bit(FRC_AUTO_MODE_INP_NR_LINK, fnr_byp_mode, 0, 1);
	if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE)
		w_reg_bit(FRC_AUTO_MODE_INP_NR_LINK, 1, 0, 1);

	//DPSS_TOP_FUNC_BYP_CTRL,reg_fnr_byp_mode
	w_reg_bit(FRC_REG_TOP_CTRL29, inp_byp_mode, 0, 1);
	//DPSS_TOP_FUNC_BYP_CTRL,reg_inp_byp_mode

	if (prm_top->bbd_only == 1) {
		w_reg_bit(DPSS_FBUF_TOP_CTRL, 1, 6, 1);//reg_dpe_bbd_only
		w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, 1, 4, 1);//reg_fnr_byp_mode
	}

	w_reg_bit(FRC_REG_INPUT_SIZE, frc_tbc_en, 30, 1);
	w_reg_bit(DPSS_FBUF_TOP_CTRL, 1, 2, 1);//reg_fbuf_enable
	w_reg_bit(DPSS_FBUF_TOP_CTRL, prm_top->auto_alig_en, 1, 1);
	wr(FRC_INPUT_SIZE_ALIGN, (prm_top->alig_hmode & 0x1) << 1 |
		(prm_top->alig_vmode & 0x1) << 0);
	//NR tbc
	w_reg_bit(DPSS_FBUF_TOP_CTRL, prm_top->alig_hmode, 4, 1);
	w_reg_bit(DPSS_FBUF_TOP_CTRL, prm_top->alig_vmode, 5, 1);
	w_reg_bit(FRC_AUTO_MODE_ENABLE, tbc_auto_mode, 0, 1);
	//reg_tbc_auto_mode
	w_reg_bit(FRC_AUTO_MODE_ENABLE, inp_ofrm_idx_sel, 4, 1);
	w_reg_bit(FRC_AUTO_MODE_ENABLE, 0, 12, 1); //reg_inp_ofrm_wren_sel
	w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 0, 0, 16); //inp use frm_reset int
	w_reg_bit(DPSS_FRC_FRM_DONE_SEL, 0, 0, 1); //inp vld flag by hw
//#ifndef DPSS_FPGA_DRV
//    w_reg_bit(FRC_REG_TOP_CTRL7,0,20,2);//reg_dae_step_sel
//#else
//    w_reg_bit(FRC_REG_TOP_CTRL7,1,20,2);//reg_dae_step_sel
//#endif
	w_reg_bit(FRC_REG_TOP_CTRL7, 2, 20, 2);//reg_dae_step_sel fixed switch halt 20241230
	//==============================================================//
	//TBC initial
	//==============================================================//
	//struct PRM_DPSS_TBC prm_tbc;
	dpss_phs_lut_ini(frc_ratio, film_mode);
	//==============================================================//
	// FILM_MODE
	//==============================================================//
	config_inp_loop_tab(film_mode, 0, 0);
	//==============================================================//
	// FRC_RATIO
	//==============================================================//
	config_dae_loop_tab(frc_ratio, film_mode, prm_top->dae_step_sel == 1);
	w_reg_bit(FRC_REG_INP_IBUF_CFG, reg_inp_ibuf_cfg_en, 0, 1);
	//reg_inp_ibuf_cfg_en
	w_reg_bit(FRC_REG_PHS_TABLE, film_mode, 8, 8); //reg_frc_film_mode,
	w_reg_bit(FRC_REG_PHS_TABLE, film_mode, 0, 8); //reg_frc_film_mode
	w_reg_bit(FRC_REG_TOP_CTRL17, reg_film_phs_test, 9, 1);//reg_film_phs_test
	//12-17 wr(FRC_AUTO_MODE_INP_NR_LINK,inp_nr_link_mode);
	wr(FRC_AUTO_MODE_BUFF_NUM, ((frc_dae_ibuf_num & 0x1f) << 16) |
		//reg_dae_ibuf_num
		((inp_obuf_num    & 0x1f) << 8) |
		//reg_inp_obuf_num
		((inp_ibuf_num & 0x1f) << 0));//reg_inp_ibuf_num
	wr(DPSS_FBUF_BUF_CTRL,
		((nr_dae_ibuf_num & 0x1f) << 16)
		| //reg_dae_ibuf_num
		((nr_aepe_buf_num & 0x1f) << 8)
		| //reg_aepe_buf_num
		((nr_dpe_obuf_num & 0x1f) <<
		0)); //reg_dpe_obuf_num
	wr(DPSS_FBUF_DPE_NUM_CTRL,
		((num_dblk & 0x1f) << 8)
		| //reg_dblk_buf_num
		((num_wrbk & 0x1f) <<
		0)); //reg_wrbk_obuf_num
	wr(FRC_DPE_OUT_BUFF_NUM,
		((frc_dae_obuf_num & 0x1f) << 16)
		| //reg_dae_obuf_num
		((frc_dpe_obuf_num & 0x1f) <<
		0));//reg_dpe_obuf_num
	wr(FRC_FRAME_BUFFER_NUM,
		((2 & 0x1f) << 16)
		| //reg_mero_fb_num//nr_frc_0113
		((8 & 0x1f) << 8) | //reg_logo_fb_num
		((8 & 0x1f) << 0));//reg_frc_fb_num

	w_reg_bit(DPSS_REG_NRDI_WRPT_FULL_NUM,
		(num_nrdi_wrpt & 0xf), 0, 4); //reg_nrdi_wrpt_full_num
	//move
	w_reg_bit(DPSS_FBUF_DPE_RDMA_INFO, num_dpe_ro, 0, 4);	//reg_dpe_rdma_num
	w_reg_bit(DPSS_FBUF_DAE_INIT, num_nr_me_ro, 24, 4);	//reg_dae_mero_num
	//move w_reg_bit(DPSS_FBUF_DAE_INIT + 0x300, DPSS_HW_LOOP_IN_OUT_BUF_NUB, 24, 4);

	w_reg_bit(FRC_REG_LOAD_ORG_FRAME_0, 1, 27, 1);
	//reg_ip_film_end_0, TODO for FILM MODE
	w_reg_bit(DPSS_TOP_FUNC_CTRL, reg_phsx2x_mode_en, 0, 1);
	//reg_phsx2x_mode_en
	w_reg_bit(DPSS_TOP_FUNC_CTRL, frc_nr_en & 0x5, 4, 3); //bit4/5/6
	w_reg_bit(DPSS_TOP_FUNC_CTRL, (frc_nr_en >> 0) & 0x1, 4, 1);
	w_reg_bit(DPSS_TOP_PER_DLY_CTRL, (frc_nr_en >> 1) & 0x0, 0, 1);
	w_reg_bit(DPSS_TOP_INTR_CTRL, (frc_nr_en >> 2) & 0x1, 0, 1);
	w_reg_bit(DPSS_TOP_FUNC_CTRL, reg_frc_src_idx, 16, 4);
	w_reg_bit(FRC_REG_TOP_CTRL13, 0, 6, 1); //reg_force_film_end
	wr(FRC_REG_TOP_CTRL25,
		0 << 31 | //reg_inp_padding_en
		0x40080200 << 0);//reg_padding_value
	w_reg_bit(FRC_REG_PAT_POINTER, 3, 24, 8); //reg_init_load_num
	dbg_h1("%s:reg:FRC_INPUT_SIZE: = 0x%x\n",
		__func__, rd(FRC_INPUT_SIZE));
}

static void hw_cfg_dpss_size_frc(struct PRM_DPSS_TOP *prm_top)
{
	u32 org_hsize;
	u32 org_vsize;

	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize = prm_top->org_hsize;
		org_vsize = prm_top->org_vsize;
	} else {
		org_hsize = prm_top->frm_hsize;
		org_vsize = prm_top->frm_vsize;
	}

	u32 frm_hsize = prm_top->frm_hsize;
	u32 frm_vsize = prm_top->frm_vsize;
	u32 me_dsx = prm_top->dpe_dw_dsx;
	u32 me_dsy = prm_top->dpe_dw_dsy;
	u32 mvx_div_mode = prm_top->mvx_div_mode;
	u32 mvy_div_mode = prm_top->mvy_div_mode;
	//ary no use u32 dpe_slc_num = prm_top->dpe_slc_num;
	//ary no use u32 dpe_dw_dsx = prm_top->dpe_dw_dsx;
	//ary no use u32 dpe_dw_dsy = prm_top->dpe_dw_dsy;
	bool nods_1080p = prm_top->src_is_1080p_nods & 0x1;
	u32 inp_dsx = nods_1080p ? 0 : me_dsx;
	u32 inp_dsy = nods_1080p ? 0 : me_dsy;

	dbg_h2("\torg_hsize=%4d  org_vsize=%4d\n", org_hsize, org_vsize);
	dbg_h2("\tfrm_hsize=%4d  frm_vsize=%4d\n", frm_hsize, frm_vsize);
	//cfg dpss_top size
	wr(FRC_INPUT_SIZE, (org_vsize & 0x3fff) << 16 | //reg_frm_vsize_in
		(org_hsize & 0x3fff) << 0);//reg_frm_hsize_in
	w_reg_bit(FRC_REG_INPUT_SIZE, frm_hsize,
		13, 13);//reg_frc_input_xsize
	w_reg_bit(FRC_REG_INPUT_SIZE, frm_vsize, 0,
		13);//reg_frc_input_ysize
	wr(FRC_REG_ME_SCALE,
		(me_dsx & 0x3) << 6 | //reg_me_dsx_scale
		(me_dsy & 0x3) << 4);//reg_me_dsy_scale
	w_reg_bit(FRC_REG_BLK_SCALE, me_dsy, 2, 2); //reg_logo_mc_dsy_ratio
	w_reg_bit(FRC_REG_BLK_SCALE, me_dsx, 4, 2); //reg_logo_mc_dsx_ratio
	//cfg INP size
	wr(INP_SRC_SIZE, (org_vsize & 0x3fff) << 16 |
		(org_hsize & 0x3fff) << 0);
	wr(INP_FRM_SIZE, (frm_vsize & 0x3fff) << 16 |
		(frm_hsize & 0x3fff) << 0);
	wr(INP_SIZE_CTRL, (me_dsx & 0x3) << 4 | (me_dsy & 0x3) << 0);

	//for MC blksize_xscale, TODO
	if (me_dsx == 2)//mc lbuf 16x16 or 16x8
		w_reg_bit(FRC_REG_BLK_SCALE, 4, 6, 3);//reg_mc_blksize_xscale
	else
		w_reg_bit(FRC_REG_BLK_SCALE, 3, 6, 3); //reg_mc_blksize_xscale

	if (me_dsy == 2)//mc lbuf 16x16 or 16x8
		w_reg_bit(FRC_REG_BLK_SCALE, 4, 9, 3); //reg_mc_blksize_yscale
	else
		w_reg_bit(FRC_REG_BLK_SCALE, 3, 9, 3); //reg_mc_blksize_yscale

	wr(FRC_REG_BLK_SIZE_XY,
		(0x0 & 0x1) << 25
		| //reg_me_logo_dsx_ratio
		(0x0 & 0x1) << 24
		| //reg_me_logo_dsy_ratio
		(0x2 & 0x7) << 19 | //reg_me_blksize_x
		(0x2 & 0x7) << 16 | //reg_me_blksize_y
		(mvx_div_mode & 0x3) << 14 | //reg_me_mvx_div_mode
		(mvy_div_mode & 0x3) << 12);//reg_me_mvy_div_mode
	wr(FRC_BBD_DS_WIN_SETTING_RIT_BOT,
		((org_hsize - 1) & 0xffff) << 16 |
		((org_vsize - 1) & 0xffff) << 0);
	//frc_nr_ctrl
	wr(DPSS_FBUF_FSIZE,
		(org_vsize & 0x1fff) << 16
		| //reg_frm_vsize_in //TODO need config org size
		(org_hsize & 0x1fff) <<
		0); //reg_frm_hsize_in //TODO need config org size
	wr(DPSS_FBUF_FSIZE_CFG,
		(frm_vsize & 0x1fff) << 16 | //reg_vsize_cfg
		(frm_hsize & 0x1fff) << 0);//reg_hsize_cfg
	wr(DPSS_FBUF_ME_SCALE,
		(me_dsy   & 0x3) << 4 | //reg_me_dsy_scale
		(me_dsx   & 0x3) << 0); //reg_me_dsx_scale
	wr(DPSS_FBUF_INP_SCALE,
		(inp_dsy  & 0x3) << 4 | //reg_inp_dsy_scale
		(inp_dsx  & 0x3) << 0); //reg_inp_dsx_scale
	wr(DPSS_FBUF_MV_DIV_MODE,
		(mvx_div_mode & 0x3) << 14
		| //reg_me_mvx_div_mode
		(mvy_div_mode & 0x3) << 12);//reg_me_mvy_div_mode
	//FRC/BB
	wr(FRC_LOGO_BB_RIT_BOT,
		((org_hsize - 1) & 0xffff) << 16
		| //reg_logo_bb_xyxy_2
		((org_vsize - 1) & 0xffff) << 0);//reg_logo_bb_xyxy_3
	//wr(FRC_REG_BLACKBAR_XYXY_ED,
	//	((org_hsize - 1) & 0xffff) << 16
	//	| //reg_blackbar_xyxy_2
	//	((org_vsize - 1) & 0xffff) << 0);//reg_blackbar_xyxy_3
	//--reg for new xyxy addr
	wr(DPSS_DPE_MC_BLKBAR_X,
		0 | (((org_hsize - 1) & 0xffff) << 16));
	wr(DPSS_DPE_MC_BLKBAR_Y,
		0 | (((org_vsize - 1) & 0xffff) << 16));
	wr(FRC_REG_BLACKBAR_XYXY_ME_ED,
		((org_hsize - 1) & 0xffff) << 16
		| //reg_blackbar_xyxy_me_2
		((org_vsize - 1) & 0xffff) << 0);//reg_blackbar_xyxy_me_3
	//debug path
	//reg_debug_bot_motion_posi2
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI2, frm_vsize - 1, 0, 16);
	//reg_debug_rit_motion_posi2
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI2, frm_hsize - 1, 0, 16);
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_MOTION_POSI1, frm_vsize - 1, 0, 16);
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_MOTION_POSI1, frm_hsize - 1, 0, 16);
	//reg_debug_rit_motion_posi1
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI2, frm_vsize - 1, 0, 16);
	//reg_debug_bot_edge_posi2
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI2, frm_hsize - 1, 0, 16);
	//reg_debug_rit_edge_posi2
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_EDGE_POSI1, frm_vsize - 1, 0, 16);
	//reg_debug_bot_edge_posi1
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_EDGE_POSI1, frm_hsize - 1, 0, 16);
	//reg_debug_rit_edge_posi1
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_LB_POSI2, org_vsize - 1, 0, 16);
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_LB_POSI2, org_hsize - 1, 0, 16);
	w_reg_bit(FRC_REG_DEBUG_PATH_TOP_BOT_LB_POSI1, org_vsize - 1, 0, 16);
	w_reg_bit(FRC_REG_DEBUG_PATH_LFT_RIT_LB_POSI1, org_hsize - 1, 0, 16);
	dbg_h1("%s %d\n", __func__, org_vsize);
	dbg_h2("%s:reg:FRC_INPUT_SIZE: = 0x%x\n", __func__, rd(FRC_INPUT_SIZE));
}

void cfg_dpss_size_nr(struct PRM_DPSS_TOP *prm_top)
{
	dbg_h2("cfg_dpss_size nr ctrl start\n");
	u32 org_hsize;
	u32 org_vsize;

	if (prm_top->auto_alig_en &&
		prm_top->org_hsize != 0xffff) {
		org_hsize = prm_top->org_hsize;
		org_vsize = prm_top->org_vsize;
	} else {
		org_hsize = prm_top->frm_hsize;
		org_vsize = prm_top->frm_vsize;
	}

	u32 frm_hsize    = prm_top->frm_hsize;
	u32 frm_vsize    = prm_top->frm_vsize;
	u32 me_dsx       = prm_top->dae_dsx_scale;
	u32 me_dsy       = prm_top->dae_dsy_scale;
	u32 mvx_div_mode = prm_top->mvx_div_mode;
	u32 mvy_div_mode = prm_top->mvy_div_mode;

	dbg_h2("NR tbc org_hsize=%4d  org_vsize=%4d\n", org_hsize, org_vsize);
	dbg_h2("NR tbc frm_hsize=%4d  frm_vsize=%4d\n", frm_hsize, frm_vsize);
	//frc_nr_ctrl
	wr(DPSS_FBUF_FSIZE,
		(org_vsize & 0x1fff) << 16
		| //reg_frm_vsize_in//TODO need config org size
		(org_hsize & 0x1fff) <<
		0);//reg_frm_hsize_in  //TODO need config org size
	wr(DPSS_FBUF_FSIZE_CFG,
		(frm_vsize & 0x1fff) << 16 | //reg_vsize_cfg
		(frm_hsize & 0x1fff) << 0);//reg_hsize_cfg
	wr(DPSS_FBUF_ME_SCALE,
		(me_dsy & 0x3) << 4 | //reg_me_dsy_scale
		(me_dsx & 0x3) << 0);//reg_me_dsx_scale
	wr(DPSS_FBUF_INP_SCALE,
		(me_dsy   & 0x3) << 4 | //reg_inp_dsy_scale
		(me_dsx   & 0x3) << 0);//reg_inp_dsx_scale
	wr(DPSS_FBUF_MV_DIV_MODE,
		(mvx_div_mode & 0x3) << 14
		| //reg_me_mvx_div_mode
		(mvy_div_mode & 0x3) << 12); //reg_me_mvy_div_mode
}

//cfg_dpss_tbc_vdi
static void hw_cfg_dpss_tbc_vdi(struct PRM_DPSS_TOP *prm_top, u32 vdi_nr_en)
{
	u32 org_hsize;
	u32 org_vsize;
//#if 1	//ary add for loop num
	u32 nr_dae_ibuf_num = prm_top->num_in;
	u32 nr_aepe_buf_num = prm_top->num_aepe;//nr_aepe_buf_num_dbg;//?
	u32 nr_dpe_obuf_num = prm_top->num_dpe_o;//DPSS_HW_LOOP_IN_OUT_BUF_NUB;//6;
//#endif
	u32 num_dblk, num_wrbk, num_nrdi_wrpt, num_nr_me_ro, num_dpe_ro;

	if (prm_top->auto_alig_en && prm_top->org_hsize != 0xffff) {
		org_hsize    = prm_top->org_hsize;
		org_vsize    = prm_top->org_vsize;
	} else {
		org_hsize    = prm_top->frm_hsize;
		org_vsize    = prm_top->frm_vsize;
	}

	u32 frm_hsize    = prm_top->frm_hsize;
	u32 frm_vsize    = prm_top->frm_vsize;
	u32 me_dsx       = prm_top->dae_dsx_scale;
	u32 me_dsy       = prm_top->dae_dsy_scale;
	u32 mvx_div_mode = 1;
	u32 mvy_div_mode = 1;
	//ary no use u32 dpe_slc_num  = prm_top->dpe_slc_num;
	//ary no use u32 dpe_dw_dsx   = prm_top->dpe_dw_dsx;
	//ary no use u32 dpe_dw_dsy   = prm_top->dpe_dw_dsy;
	u32 di_frc_link  = prm_top->di_frc_link;
	bool di_tbc_en = ((vdi_nr_en & 0x3) != 0);

	dbg_h2("\torg_hsize=%4d  org_vsize=%4d\n", org_hsize, org_vsize);
	dbg_h2("\tfrm_hsize=%4d  frm_vsize=%4d\n", frm_hsize, frm_vsize);

	num_nrdi_wrpt = prm_top->num_nr_wrpt;
	num_nr_me_ro = prm_top->num_nr_me_ro;
	num_dpe_ro = prm_top->num_nr_me_ro;
	num_dblk = prm_top->num_dblk;
	if (prm_top->is_i)
		num_wrbk = prm_top->num_lc;
	else
		num_wrbk = nr_dpe_obuf_num;

	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_TOP_CTRL, di_tbc_en, 2,
		1);//reg_fbuf_enable
	w_reg_bit(DPSS_TOP_FUNC_CTRL1, vdi_nr_en, 4,
		3);//reg_vdi_nr_en
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_TOP_CTRL, prm_top->auto_alig_en, 1,
		1);//reg_auto_align_en
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_TOP_CTRL, ((prm_top->alig_hmode) |
		(prm_top->alig_vmode << 1)), 4, 2);
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DAE_INIT,
		8 << 24 | //reg_dae_mero_num
		0 << 16 | //reg_dae_mask_num
		2 << 8 | //reg_dae_mixo_num
		1 << 0);//reg_dae_init_num
//#if 1	//ary add for loop num
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_BUF_CTRL,
		((nr_dae_ibuf_num & 0x1f) << 16)
		| //reg_dae_ibuf_num
		((nr_aepe_buf_num & 0x1f) << 8)
		| //reg_aepe_buf_num
		((nr_dpe_obuf_num & 0x1f) <<
		0));//reg_dpe_obuf_num
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DPE_NUM_CTRL,
		((num_dblk & 0x1f) << 8)
		| //reg_dblk_buf_num
		((num_wrbk & 0x1f) <<
		0));//reg_wrbk_obuf_num
//add-------
	w_reg_bit(DPSS_REGOFST_VDICTRL + DPSS_REG_NRDI_WRPT_FULL_NUM,
			(num_nrdi_wrpt & 0xf), 0, 4); //reg_nrdi_wrpt_full_num
	//move
	w_reg_bit(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DPE_RDMA_INFO,
			num_dpe_ro, 0, 4);	//reg_dpe_rdma_num
	w_reg_bit(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DAE_INIT,
		num_nr_me_ro, 24, 4);	//reg_dae_mero_num
//-----------
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_OUT_RLS_MODE, di_frc_link, 0, 1);
	//for frc align
	w_reg_bit(DPSS_FBUF_TOP_CTRL,
		prm_top->auto_alig_en, 1, 1);
	wr(FRC_INPUT_SIZE_ALIGN,
		(prm_top->alig_hmode & 0x1) << 1 |
		(prm_top->alig_vmode & 0x1) << 0);
	//NR tbc
	w_reg_bit(DPSS_FBUF_TOP_CTRL,
		prm_top->alig_hmode, 4, 1);
	w_reg_bit(DPSS_FBUF_TOP_CTRL,
		prm_top->alig_vmode, 5, 1);
	//for di_ctrl
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_FSIZE,
		(org_vsize & 0x1fff) << 16 | //reg_frm_vsize_in
		(org_hsize & 0x1fff) << 0); //reg_frm_hsize_in
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_FSIZE_CFG,
		(frm_vsize & 0x1fff) << 16 | //reg_vsize_cfg
		(frm_hsize & 0x1fff) << 0);//reg_hsize_cfg
	//di tbc
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_TOP_CTRL, prm_top->alig_hmode, 4, 1);
	w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_TOP_CTRL, prm_top->alig_vmode, 5, 1);
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_ME_SCALE,
		(me_dsy & 0x3) << 4 | //reg_me_dsy_scale
		(me_dsx & 0x3) << 0);//reg_me_dsx_scale
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_INP_SCALE,
		(0 & 0x3) << 4 | //reg_inp_dsy_scale
		(0 & 0x3) << 0);//reg_inp_dsx_scale
	wr(DPSS_REGOFST_VDICTRL + DPSS_FBUF_MV_DIV_MODE,
		(mvx_div_mode & 0x3) << 14 | //reg_me_mvx_div_mode
		(mvy_div_mode & 0x3) << 12);//reg_me_mvy_div_mode
	dbg_h1("%s %d\n", __func__, org_hsize);
}

void cfg_dpss_done_trigger(enum DONE_TRIGGER done_mode)
{
	switch (done_mode) {
	case FRC_SRC_DONE:
		wr(DPSS_SRC_FRM_DONE, 1);
		break; //pls_src_frm_done
	case FRC_DST_DONE:
		wr(DPSS_DST_FRM_DONE, 1);
		break; //pls_dst_frm_done
	case FNR_SRC_DONE:
		wr(DPSS_SRC_FRM_DONE, 1);
		break; //pls_dst_frm_done
	case FNR_DST_DONE:
		wr(DPSS_DST_FRM_DONE, 1);
		break; //pls_dst_frm_done
	case VDI_SRC_DONE:
		wr(DPSS_DI_SRC_FRM_DONE, 1);
		break; //pls_vdi_src_frm_done
	case VDI_DST_DONE:
		wr(DPSS_DI_DST_FRM_DONE, 1);
		break; //pls_vdi_dst_frm_done
	default:
		dbg_h2("Error: trigger_mode=%d wrong !!!\n",
			done_mode); break;
	}
}

/////////////////////////load src/////////////////////////
int src0_data_trigger(void)
{
	u32 data32;
	u32 inp_ibuf_rdy;//, dpe_ibuf_cnt;
	//check inp ibuf full or not

	data32 = rd(DPSS_FRC_SRC_BUFF_STATUS);
	inp_ibuf_rdy = (data32 >> 4) & 0x1 ;//ro_fbuf_ibuf_wrdy
	dbg_h2("%s: return %0d\n", __func__, inp_ibuf_rdy);

	if (inp_ibuf_rdy)
		return 1;
	else
		return 0;
}

int src1_data_trigger(void)
{
	u32 data32;
	u32 ro_fbuf_ibuf_wrdy;
	//check inp ibuf full or not
	data32 = rd(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_IBUF_WRDY);
	ro_fbuf_ibuf_wrdy = data32 & 0x1;
	dbg_h2("src1_trigger: %8x\n", ro_fbuf_ibuf_wrdy);

	if (ro_fbuf_ibuf_wrdy)
		return 1;
	else
		return 0;
}

/*
 *void dst1_data_trigger(u32 dst1_done)
 *{
 *	u32 dpe_obuf_cnt;
 *	//check inp ibuf full or not
 *	dpe_obuf_cnt = (rd(DPSS_REGOFST_VDICTRL +
 *		DPSS_FBUF_BUF_CNT)) & 0x1f;//ro_dpe_obuf_level
 *	dst1_done = (dpe_obuf_cnt > 0);
 *}
 */
unsigned int disp_obuf_trigger(enum DONE_TRIGGER done_mode)
{
	u32 disp_obuf_cnt = 0;
	bool rd_flag = 0;
	unsigned int status = 0;

	//check disp obuf
	switch (done_mode) {
	case FNR_DST_DONE:
		status = rd(DPSS_FRC_DST_BUFF_STATUS);
		rd_flag = ((status >> 4) & 0x01) ? true : false;
		disp_obuf_cnt = status & 0x0f;
		break;//ro_dst_obuf_rdrdy

	case FRC_DST_DONE:
		disp_obuf_cnt = ((rd(DPSS_FRC_DST_BUFF_STATUS) >>
		4) & 0x1);
		break;//ro_dst_obuf_rdrdy

	//case FRC_DST_DONE : { disp_obuf_cnt =
	//(( rd(DPSS_FRC_MC_IUFF_STATUS )>>4 )& 0x1); break;}
	//ro_dst_obuf_rdrdy
	case VDI_DST_DONE:
		disp_obuf_cnt = (((rd(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_BUF_CNT)) & 0x1f));

		if (disp_obuf_cnt) //ary add
			rd_flag = 1;
		break;//ro_dpe_obuf_level

	default: {
		DBG_WARN("Error: done_mode=%d wrong !!!\n", done_mode);
		break;
		};
	}

	if (rd_flag)
		dbg_h1("%s: state = 0x%x, rd = %d, cnt %d\n",
			__func__, status, rd_flag, disp_obuf_cnt);

	return disp_obuf_cnt;
}

//cfg_dpss_buff_addr
static void hw_cfg_dpss_buff_addr(struct PRM_DPSS_TOP *prm_top,
	enum DPSS_WORK_MODE dpss_mode,
	u32 src0_type, //ary note: 0: frc disable; 1: frc enable
	u32 src1_type, u32 path_id)
{
	u32 data32;
	u32 loop_num    = 16;
	u32 di_frc_link = prm_top->di_frc_link;
	//u32 reg_nrdi_src_sel;

	dbg_h1("%s:src_plan:%d, loop_num:%d\n",
		__func__, prm_top->src_ds_fmt.src_plan, loop_num);

	if (di_frc_link) {
		w_reg_bit(DPSS_TOP_FUNC_CTRL1, 1, 0, 1);
		//reg_di_frc_link_en
		w_reg_bit(FRC_AUTO_MODE_BUFF_NUM, loop_num, 0, 5);
		//frc,reg_inp_ibuf_num
		w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_BUF_CTRL, loop_num, 0, 5); //di ,reg_dpe_obuf_num
		w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_DPE_NUM_CTRL, loop_num, 0, 5); //di ,reg_dpe_obuf_num
		w_reg_bit(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_DPE_NUM_CTRL, loop_num, 8, 5); //di ,reg_dpe_obuf_num
		data32 = rd(DPSS_SRC1_DIOUT_YHEAD_ADDR);
		wr(DPSS_SRC0_INP_YHEAD_ADDR, data32);
		data32 = rd(DPSS_SRC1_DIOUT_CHEAD_ADDR);
		wr(DPSS_SRC0_INP_CHEAD_ADDR, data32);
		data32 = rd(DPSS_SRC1_DIOUT_YHEAD_STEP);
		wr(DPSS_SRC0_INP_YHEAD_STEP, data32);
		data32 = rd(DPSS_SRC1_DIOUT_CHEAD_STEP);
		wr(DPSS_SRC0_INP_CHEAD_STEP, data32);
	}

	//nrdi addr
	u32 reg_frc_en = (rd(FRC_REG_INPUT_SIZE) >> 30) & 0x1;
	u32 reg_frc_nr_en = (rd(DPSS_TOP_FUNC_CTRL) >> 4) & 0x7;
#ifdef ARY_SIM
	if (prm_top->case_id == TST_CASE_IDX_0000) {
		if (reg_frc_en != 0)
			dbg_k0("sim: case0: reg_frc_en %d-> 0\n", reg_frc_en);

		if (reg_frc_nr_en != 1)
			dbg_h2("sim:case0:reg_frc_nr_en %d->1\n", reg_frc_nr_en);
	}

#endif
	//ary no use u32 reg_vdi_nr_en = (rd(DPSS_TOP_FUNC_CTRL1) >> 4) & 0x7;
	u32 src0_nr_en = ((reg_frc_nr_en & 0x1) == 0x1) |
		(prm_top->dolby_mode >= DOLBY_DPSS_MODE);
	u32 src0_di_en = (reg_frc_nr_en & 0x2) == 0x2;
	//ary no use    u32 frc_only = dpss_mode == DPSS_FRC_MODE;
	u32 src0_nr_frc = reg_frc_en & src0_nr_en;
	u32 src0_di_frc = reg_frc_en & src0_di_en;
	//dpss_mode == DPSS_NRDI_FRC_SRC0_MODE || dpss_mode == DPSS_DI_FRC_SRC0_MODE;
	//ary no use    u32 src1_di_frc = di_frc_link & frc_only;
	dbg_h2("\t:reg_frc_en = %d, reg_frc_nr_en = %d\n",
		reg_frc_en, reg_frc_nr_en);
	u32 dpss_src0_fbuf_yaddr;
	u32 dpss_src0_fbuf_caddr;
	u32 dpss_src0_dwbuf_yaddr;
	u32 dpss_src0_dwbuf_caddr;
	u32 dpss_src0_nro_fbuf_yaddr;
	u32 dpss_src0_nro_fbuf_caddr;
	u32 dpss_src0_dio_fbuf_yaddr;
	u32 dpss_src0_dio_fbuf_caddr;
	u32 dpss_src0_dpeo_dwbuf_yaddr;
	u32 dpss_src0_dpeo_dwbuf_caddr;
	u32 dpss_src1_fbuf_yaddr;
	u32 dpss_src1_fbuf_caddr;
	u32 dpss_src1_dwbuf_yaddr;
	u32 dpss_src1_dwbuf_caddr;
	u32 dpss_src1_nro_fbuf_yaddr;
	u32 dpss_src1_nro_fbuf_caddr;
	u32 dpss_src1_dio_fbuf_yaddr;
	u32 dpss_src1_dio_fbuf_caddr;
	u32 dpss_src1_dpeo_dwbuf_yaddr;
	u32 dpss_src1_dpeo_dwbuf_caddr;
	int i;

	for (i = 0; i < 16;  i++) {
		dpss_src0_nro_fbuf_yaddr = prm_top->src0_nro_fbuf_yaddr[i];
		dpss_src0_nro_fbuf_caddr = prm_top->src0_nro_fbuf_caddr[i];
		dpss_src0_dio_fbuf_yaddr = prm_top->src0_dio_fbuf_yaddr[i];
		dpss_src0_dio_fbuf_caddr = prm_top->src0_dio_fbuf_caddr[i];
		dpss_src0_dpeo_dwbuf_yaddr = src0_di_frc ?
			prm_top->src0_dio_dwbuf_yaddr[i] :
			prm_top->src0_nro_dwbuf_yaddr[i];
		dpss_src0_dpeo_dwbuf_caddr = src0_di_frc ?
			prm_top->src0_dio_dwbuf_caddr[i] :
			prm_top->src0_nro_dwbuf_caddr[i];
		dpss_src0_fbuf_yaddr =
			prm_top->frc_fbuf_only ? prm_top->src0_fbuf_yaddr[i] :
			prm_top->frc_ds_scale_en ? prm_top->src0_dwbuf_yaddr[i] :
			di_frc_link ? prm_top->src1_dio_fbuf_yaddr[i] :
			prm_top->src0_fbuf_yaddr[i];
		dpss_src0_fbuf_caddr =
			prm_top->frc_fbuf_only ? prm_top->src0_fbuf_caddr[i] :
			prm_top->frc_ds_scale_en ? prm_top->src0_dwbuf_caddr[i] :
			di_frc_link ? prm_top->src1_dio_fbuf_caddr[i] :
			prm_top->src0_fbuf_caddr[i];
		dpss_src0_dwbuf_yaddr = di_frc_link ?
			prm_top->src1_dio_dwbuf_yaddr[i] :
			src0_type ? prm_top->src0_fbuf_yaddr[i] :
			prm_top->src0_dwbuf_yaddr[i];
		dpss_src0_dwbuf_caddr = di_frc_link ?
			prm_top->src1_dio_dwbuf_caddr[i] :
			src0_type ? prm_top->src0_fbuf_caddr[i] :
			prm_top->src0_dwbuf_caddr[i];
		dpss_src1_fbuf_yaddr =
			prm_top->src1_fbuf_yaddr[i];
		dpss_src1_fbuf_caddr =
			prm_top->src1_fbuf_caddr[i];
		dpss_src1_dwbuf_yaddr = src1_type ?
			prm_top->src1_fbuf_yaddr[i] :
			prm_top->src1_dwbuf_yaddr[i];
		dpss_src1_dwbuf_caddr = src1_type ?
			prm_top->src1_fbuf_caddr[i] :
			prm_top->src1_dwbuf_caddr[i];
		dpss_src1_nro_fbuf_yaddr  =
			prm_top->src1_nro_fbuf_yaddr[i];
		dpss_src1_nro_fbuf_caddr  =
			prm_top->src1_nro_fbuf_caddr[i];
		dpss_src1_dio_fbuf_yaddr  =
			prm_top->src1_dio_fbuf_yaddr[i];
		//dbg_h2("%s:src1:nro:0x%x, dio:0x%x\n",
		//	__func__,
		//	dpss_src1_nro_fbuf_yaddr,
		//	dpss_src1_dio_fbuf_yaddr);
		dpss_src1_dio_fbuf_caddr =
			prm_top->src1_dio_fbuf_caddr[i];
		dpss_src1_dpeo_dwbuf_yaddr = src1_type ?
			prm_top->src1_dio_dwbuf_yaddr[i] :
			prm_top->src1_nro_dwbuf_yaddr[i];
		dpss_src1_dpeo_dwbuf_caddr = src1_type ?
			prm_top->src1_dio_dwbuf_caddr[i] :
			prm_top->src1_nro_dwbuf_caddr[i];

		if (path_id == 0) {	//ary addr change *4
			//ary:input ?
			wr((DPSS_SRC0_FBUF_YADDR0 + i),
				dpss_src0_fbuf_yaddr);
			wr((DPSS_SRC0_FBUF_CADDR0 + i),
				dpss_src0_fbuf_caddr);
			dbg_h2("%s:addr:in:path_id 0,yaddr:[%d]:0x%x\n",
				__func__, i, dpss_src0_fbuf_yaddr);
			wr((DPSS_SRC0_DWBUF_YADDR0 + i),
				dpss_src0_dwbuf_yaddr);
			wr((DPSS_SRC0_DWBUF_CADDR0 + i),
				dpss_src0_dwbuf_caddr);
			//ary: out
			wr((DPSS_SRC0_NRO_FBUF_YADDR0 + i),
				dpss_src0_nro_fbuf_yaddr);
			dbg_h2("%s:addr:nro:path_id 0,yaddr:[%d]:0x%x\n",
				__func__, i, dpss_src0_nro_fbuf_yaddr);
			wr((DPSS_SRC0_NRO_FBUF_CADDR0 + i),
				dpss_src0_nro_fbuf_caddr);
			wr((DPSS_SRC0_DIO_FBUF_YADDR0 + i),
				dpss_src0_dio_fbuf_yaddr);
			dbg_pps0("%s:addr:nro:path_id 0,yaddr:[%d]:0x%x\n",
				__func__, i, dpss_src0_dio_fbuf_yaddr);

			wr((DPSS_SRC0_DIO_FBUF_CADDR0 + i),
				dpss_src0_dio_fbuf_caddr);
			wr((DPSS_SRC0_DPEO_DWBUF_YADDR0 + i),
				dpss_src0_dpeo_dwbuf_yaddr);
			wr((DPSS_SRC0_DPEO_DWBUF_CADDR0 + i),
				dpss_src0_dpeo_dwbuf_caddr);
			//===============================================================
			// 0-src full buff
			// 1-src dw buff
			// 2-dpe out full buff
			// 3-dpe out dw buff
			//===============================================================
			u32 reg_inp_src_sel = src0_di_frc ? 3 : 1;
			u32 reg_frc_me_src_sel = prm_top->bbd_only ? 1 :
				src0_di_frc | src0_nr_frc ? 3 : 1;
			u32 reg_frc_mc_src_sel = prm_top->bbd_only ? 0 :
				prm_top->frc_ds_scale_en ? 1 : src0_di_frc |
				src0_nr_frc ? 2 : 0;
			u32 reg_dcntr_src_sel =
				prm_top->src_is_1080p_nods ? 0 : 1;
			u32 reg_nrme_src_sel  =
				prm_top->src_is_1080p_nods ? 0 : 1;
			//u32 reg_nrdi_src_sel;

			wr(DPSS_SRC_RADDR_SEL, reg_inp_src_sel << 4 |
				reg_frc_me_src_sel << 2 |
				reg_frc_mc_src_sel << 0);
			w_reg_bit(DPSS_FBUF_RADDR_CTRL,
				(reg_dcntr_src_sel << 1) | reg_nrme_src_sel, 1,
				2);
			//wr(DPSS_FBUF_RADDR_CTRL,reg_dcntr_src_sel << 2 |
			//	reg_nrme_src_sel << 1 |
			//	reg_nrdi_src_sel << 0 );
		} else if (path_id == 1) {
			wr((DPSS_SRC1_FBUF_YADDR0 + i),
				dpss_src1_fbuf_yaddr);
			wr((DPSS_SRC1_FBUF_CADDR0 + i),
				dpss_src1_fbuf_caddr);
			wr((DPSS_SRC1_DWBUF_YADDR0 + i),
				dpss_src1_dwbuf_yaddr);
			wr((DPSS_SRC1_DWBUF_CADDR0 + i),
				dpss_src1_dwbuf_caddr);
			wr((DPSS_SRC1_NRO_FBUF_YADDR0 + i),
				dpss_src1_nro_fbuf_yaddr);
			wr((DPSS_SRC1_NRO_FBUF_CADDR0 + i),
				dpss_src1_nro_fbuf_caddr);
			wr((DPSS_SRC1_DIO_FBUF_YADDR0 + i),
				dpss_src1_dio_fbuf_yaddr);
			wr((DPSS_SRC1_DIO_FBUF_CADDR0 + i),
				dpss_src1_dio_fbuf_caddr);
			wr((DPSS_SRC1_DPEO_DWBUF_YADDR0 + i),
				dpss_src1_dpeo_dwbuf_yaddr);
			wr((DPSS_SRC1_DPEO_DWBUF_CADDR0 + i),
				dpss_src1_dpeo_dwbuf_caddr);
		} else {
			DBG_ERR("%s cfg path wrong !!!:%d\n", __func__,
				path_id);
		}
	}
	//ary add below: mc addr
}

//sel: 1 for afrc /afbc
void hw_cfg_dpss_buff_addr_src0_nro(struct PRM_DPSS_TOP *prm_top,
					bool sel)
{
	int i;
	//only for light change:

	if (prm_top->src_mode) {
		DBG_ERR("%s:not src0 %d\n", __func__, prm_top->src_mode);
		return;
	}
	if (sel) {
		for (i = 0; i < 16;  i++) {
			prm_top->src0_nro_fbuf_yaddr[i] =
				prm_top->src0_nro_fbuf_af_y[i];
			prm_top->src0_nro_fbuf_caddr[i] =
				prm_top->src0_nro_fbuf_af_c[i];
		}
	} else {
		for (i = 0; i < 16;  i++) {
			prm_top->src0_nro_fbuf_yaddr[i] =
				prm_top->src0_nro_fbuf_m_y[i];
			prm_top->src0_nro_fbuf_caddr[i] =
				prm_top->src0_nro_fbuf_m_c[i];
			prm_top->src0_dio_fbuf_yaddr[i] =
				prm_top->src0_diopps_fbuf_yaddr[i];
			prm_top->src0_dio_fbuf_caddr[i] =
				prm_top->src0_diopps_fbuf_caddr[i];

		}
	}
	for (i = 0; i < 16;  i++) {
		wr((DPSS_SRC0_NRO_FBUF_YADDR0 + i),
			prm_top->src0_nro_fbuf_yaddr[i]);
		wr((DPSS_SRC0_NRO_FBUF_CADDR0 + i),
			prm_top->src0_nro_fbuf_caddr[i]);
		wr((DPSS_SRC0_DIO_FBUF_YADDR0 + i),
			prm_top->src0_dio_fbuf_yaddr[i]);
		wr((DPSS_SRC0_DIO_FBUF_CADDR0 + i),
			prm_top->src0_dio_fbuf_caddr[i]);

	}
}

void dpss_patch_for_bitmatch(u32 dpss_inp_frm_cnt,
	u32 dpss_dae_frm_cnt_src0,
	u32 dpss_dae_frm_cnt_src1,
	u32 dpss_dae_frm_cnt_src2,
	u32 dpss_dpe_nr_frm_cnt,
	u32 dpss_dpe_di_frm_cnt,
	u32 *dpss_di_ignore_num_out,
	u32 *dpss_nr_ignore_num_out,
	u32 *dpss_inp_frm_cnt_adj_out,
	u32 *dpss_dae_frm_cnt_src0_adj_out,
	u32 *dpss_dae_frm_cnt_src1_adj_out,
	u32 *dpss_dae_frm_cnt_src2_adj_out,
	u32 *dpss_dpe_nr_frm_cnt_adj_out,
	u32 *dpss_dpe_di_frm_cnt_adj_out)
{
	/////////////////////For bitmatch start//////////////////////////////
	u32 dpss_dae_frm_cnt_src1_adj;
	//ary no use u32 m_di_frc_link =  rd(DPSS_TOP_FUNC_CTRL1) &0x1;
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;
	u32 bbd_only = (rd(DPSS_FBUF_TOP_CTRL) >> 6) & 0x1;
#ifdef SRC1_PSRC_NR
	u32 dpss_di_ignore_num = 1;
#else
	u32 dpss_di_ignore_num = 2;
#endif
	u32 dpss_inp_ignore_num = 0;//(m_di_frc_link ? dpss_di_ignore_num:0);
	u32 dpss_nr_ignore_num = src0_nrdi_frc_en ? dpss_di_ignore_num : 1;
	//(m_di_frc_link && dpss_di_film_mode==2) ? 1+1: dpss_inp_ignore_num+1;
#ifdef DPSS_GAME_MODE_DLY1
	u32 dpss_frc_ignore_num = 1;

	dbg_h2("game mode dly 1frame\n");
#else
	u32 dpss_frc_ignore_num = 2;
#endif
	u32 dpss_inp_frm_cnt_adj =
		dpss_inp_frm_cnt <= dpss_inp_ignore_num ? 0 :
		(dpss_inp_frm_cnt - dpss_inp_ignore_num);
	u32 dpss_dae_frm_cnt_src0_adj =
		dpss_dae_frm_cnt_src0 <= dpss_frc_ignore_num ?
		0 : (dpss_dae_frm_cnt_src0 - dpss_frc_ignore_num);

	if (src0_nrdi_frc_en == 0) {
		dpss_dae_frm_cnt_src1_adj = dpss_dae_frm_cnt_src1
			<= dpss_nr_ignore_num  ? 0 :
			(dpss_dae_frm_cnt_src1 - dpss_nr_ignore_num);
	} else {
		dpss_dae_frm_cnt_src1_adj = dpss_dae_frm_cnt_src1;
	}

	u32 dpss_dae_frm_cnt_src2_adj = dpss_dae_frm_cnt_src2;
	//dpss_dae_frm_cnt_src2 <= dpss_di_ignore_num ? 0 :
	//(dpss_dae_frm_cnt_src2 - dpss_di_ignore_num);
	u32 dpss_dpe_nr_frm_cnt_adj =
		dpss_dpe_nr_frm_cnt <= dpss_nr_ignore_num ?
		0 : (dpss_dpe_nr_frm_cnt - dpss_nr_ignore_num);
	u32 dpss_dpe_di_frm_cnt_adj =
		dpss_dpe_di_frm_cnt <= dpss_di_ignore_num ?
		0 : (dpss_dpe_di_frm_cnt - dpss_di_ignore_num);
	*dpss_di_ignore_num_out = dpss_di_ignore_num;
	*dpss_nr_ignore_num_out = dpss_nr_ignore_num;
	*dpss_inp_frm_cnt_adj_out = dpss_inp_frm_cnt_adj;
	*dpss_dae_frm_cnt_src0_adj_out = dpss_dae_frm_cnt_src0_adj;
	*dpss_dae_frm_cnt_src1_adj_out = dpss_dae_frm_cnt_src1_adj;
	*dpss_dae_frm_cnt_src2_adj_out = dpss_dae_frm_cnt_src2_adj;
	*dpss_dpe_nr_frm_cnt_adj_out = dpss_dpe_nr_frm_cnt_adj;
	*dpss_dpe_di_frm_cnt_adj_out = dpss_dpe_di_frm_cnt_adj;

	//just print 1st time
	if (dpss_inp_frm_cnt == 0 &&
	    dpss_dae_frm_cnt_src2 == 0) {
		wr(FRC_DAE_FRM_DLY_NUM, dpss_frc_ignore_num);
		w_reg_bit(DPSS_FBUF_DAE_INIT
			, bbd_only ? 0 : dpss_nr_ignore_num, 8, 5);
		w_reg_bit(DPSS_REGOFST_VDICTRL +
			DPSS_FBUF_DAE_INIT, dpss_di_ignore_num, 8, 5);
		//printf("[Process_Irq]: frc_di_link[%d] :[%d %d %d %d]\n",
		//m_di_frc_link,dpss_inp_ignore_num,dpss_di_ignore_num,
		//dpss_nr_ignore_num,dpss_frc_ignore_num);
		dbg_h2("frc dly num: %d\n", dpss_frc_ignore_num);
	}
}

void cfg_dpss_dpe_nr_link_out(u32 nr_link_en)//1:link out nr output 0:no link out
{
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	if (nr_link_en == 0)//no link
		wr(DPSS_DPE_NR_DATAPATH_OUT_MUX, 0x3);
	else if (src0_nrdi_frc_en == 0)//src0 nr output link
		wr(DPSS_DPE_NR_DATAPATH_OUT_MUX, 0x0);
}

u64 buff_addr_cal(u64 buff_addr, u32 buff_step, u32 buff_idx,
u32 buff_num)
{
	return (buff_addr + buff_step * (buff_idx !=
		buff_num ? buff_idx : 0));
}

//init_dpss_dpe_obuf
//static
void hw_init_dpss_dpe_obuf(void)
{
	u32 fnr_obuf_num;
	u32 vdi_obuf_num;
	int i;

	fnr_obuf_num = rd(DPSS_FBUF_BUF_CTRL) & 0x1f;
	vdi_obuf_num = rd(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_BUF_CTRL) & 0x1f; //ary: 0x86e5?
	dbg_h2("%s:fnr_obuf_num=%d,vdi_obuf_num=%d\n",
		__func__, fnr_obuf_num, vdi_obuf_num);

	//12-05	for (i = 0; i < fnr_obuf_num; i++) {
	//12-05	w_reg_bit(DPSS_FNR_SW_DRV_CTRL1,i,4,4);//dpe_obuf_free_idx
	//12-05	w_reg_bit(DPSS_FNR_SW_DRV_CTRL0,1,1,1);//fnr_dpe_obuf_free,pls
	//12-05	}
	for (i = 0; i < vdi_obuf_num; i++) {
		w_reg_bit(DPSS_VDI_SW_DRV_CTRL1, i, 4, 4);
		w_reg_bit(DPSS_VDI_SW_DRV_CTRL0, 1, 1, 1);
	}
}

static void hw_init_dpss_dpe_out_tbc(unsigned int src_mode)
{
	u32 fnr_obuf_num;
	u32 vdi_obuf_num;
	int i;

	fnr_obuf_num = rd(DPSS_FBUF_BUF_CTRL) & 0x1f;
	vdi_obuf_num = rd(DPSS_REGOFST_VDICTRL +
		DPSS_FBUF_BUF_CTRL) & 0x1f; //ary: 0x86e5?
	dbg_h2("%s:fnr_obuf_num=%d,vdi_obuf_num=%d\n",
		__func__, fnr_obuf_num, vdi_obuf_num);
	if (src_mode == 0) {
		for (i = 0; i < reg_fnr_dpe_obuf_num; i++) {
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, i, 4, 4);
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 1, 1);//initial fnr obuf release idx
			//ary note: ref to dpss_dae_done[dpe_fifo_idx]
		}
	} else {
		for (i = 0; i < vdi_obuf_num; i++) {
			w_reg_bit(DPSS_VDI_SW_DRV_CTRL1, i, 4, 4);
			w_reg_bit(DPSS_VDI_SW_DRV_CTRL0, 1, 1, 1);
		}
	}
}
