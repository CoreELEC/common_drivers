// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

//#include "register_t6w.h"
//#include "register_t6w_vc.h"

#include "sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#endif
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_hw.h"
#include "dpss_hw_frc.h"
#include "dpss_func.h"
#include "dpss_input_q.h"
#include "dpss_s_frc.h"
#include "dpss_rdma_frc.h"

#include "hw/dpss.h"
#include "hw/dpss_phslut.h"
#include "hw/dpss_mc.h"
#include "hw/dpss_vdi.h"
#include "hw/dpss_lib.h"
#include "hw/define.h"
#include "drv/d_pq.h"

struct PRM_DPSS_INP prm_dpss_inp;

#ifdef USE_TWO_DATA
struct PRM_DPSS_TOP prm_dpss_top[2];
struct PRM_DPSS_DAE prm_dpss_dae[2];
struct PRM_DPSS_DPE prm_dpss_dpe[2];
#endif

#ifdef USE_ONE_DATA
struct PRM_DPSS_TOP *prm_dpss_top;
struct PRM_DPSS_DAE *prm_dpss_dae;
struct PRM_DPSS_DPE *prm_dpss_dpe;
struct PRM_DPSS_TOP prm_dpss_top_di;
struct PRM_DPSS_DAE prm_dpss_dae_di;
struct PRM_DPSS_DPE prm_dpss_dpe_di;

struct PRM_DPSS_TOP *prm_dpss_top2;
struct PRM_DPSS_DAE *prm_dpss_dae2;
struct PRM_DPSS_DPE *prm_dpss_dpe2;

#endif
//ary add:
struct PRM_DPSS_TOP prm_dpss_top_1;
struct PRM_DPSS_INP prm_dpss_inp_1;
struct PRM_DPSS_DAE prm_dpss_dae_1;
struct PRM_DPSS_DPE prm_dpss_dpe_1;

struct Vpu_queue inp_bufQ;
//move from dpss_proc_irq.h
u32 src0_frm_idx_cnt;
u32 src1_frm_idx_cnt;
u32 dpss_dpe_mc_frm_cnt;
u32 dpss_dpe_mcp_frm_cnt;
u32 dpss_dpe_nr_frm_cnt;
u32 dpss_dpe_di_frm_cnt;
u32 dpss_rls0_frm_cnt;
u32 dpss_rls1_frm_cnt;
u32 dpss_disp0_frm_cnt;
u32 dpss_disp1_frm_cnt;
u32 sec_data;
bool fnr_disp_obuf_rdy;
bool src0_disp_obuf_rdy;
bool src1_disp_obuf_rdy;

u32 frc_obuf_status;
u32 frc_ocnt_status;
u32 mc_need_match;
u32 dpss_disp_timeout_cnt;
//---
u32 dpss_last_dpe_mode; //07-04 for 2ch
u32 dpss_last_dpe_src = 0x1f;	//07-04 for 2ch

u32 dpss_inp_frm_cnt;
u32 dpss_dae_frm_cnt_src0;
u32 dpss_dae_frm_cnt_src1;
u32 dpss_dae_frm_cnt_src2;

bool frc_vpp_link_obuf_rdy;
static bool prev_is_vd1_link;
static bool prev_is_vd1_link_valid;

u32 src0_pre_idx_cnt;
u32 dpe_pre_phs0_flg;
u32 dpe_start_num = 2;
u32 dae_rd_less_one;
u32 vdin_dpss_phs;

u32 mc_repeat_frm;		//1114
//Vpu_queue inp_sec_bufQ;       //1114
//Vpu_queue dae_sec_bufQ;       //1114
//Vpu_queue dpe_sec_bufQ;       //1114

struct DPSS_SEC dpss_sec;		//1114
//below is add by ary:
unsigned int g_dae_frc_loop;
unsigned int g_dae_index;
unsigned int g_dpe_index;
unsigned int g_dae_frc_cnt;
unsigned int g_dae_nr_cnt;

//struct PRM_DPSS_TBC g_dpss_prm_tbc; //use in hw_cfg_dpss_tbc_frc
struct PRM_INTF_TYPE g_inp_yuv_rdmif;
struct PRM_DPSS_SIZE prm_size;	//in hw_cfg_dpss_dae
struct PRM_INTF_TYPE dae_yuv_intf;	//hw_cfg_dpss_dae
struct PRM_DPSS_SIZE prm_size_di;	//in hw_cfg_dpss_dae
struct PRM_INTF_TYPE dae_yuv_intf_di;	//hw_cfg_dpss_dae

struct AA_PPS_TOP_TYPE g_nr_pps_cfg;	//ary move from hw_cfg_dpss_dpe
struct AA_PPS_TOP_TYPE g_nr_pps_cfg2;

//struct PRM_INTF_TYPE g_pix_rmif;		//ary used in hw_cfg_dpss_dpe_intf
struct VFCD_t g_vfcd0;
struct VFCD_t g_vfcd1;
struct PRM_INTF_TYPE g_pix_rmif2;
struct PRM_INTF_TYPE g_pix_rmif3;
struct PRM_INTF_TYPE g_pix_rmif4;
struct PRM_INTF_TYPE g_pix_wmif0;
struct PRM_INTF_TYPE g_pix_wmif1;
struct PRM_INTF_TYPE g_pix_wmif2;
struct PRM_INTF_TYPE g_ds_wmif0;
struct PRM_INTF_TYPE g_ds_wmif1;
struct VFCE_TYPE g_vfce0;

bool dpss_dbg_dis_dw;		// = true;
module_param_named(dpss_dbg_dis_dw, dpss_dbg_dis_dw, bool, 0664);

bool dpss_dbg_match_dis = true;
module_param_named(dpss_dbg_match_dis, dpss_dbg_match_dis, bool, 0664);
unsigned int dpss_dbg_uv_swap; // 0x1f0 //bit 0, 4,5,6,7, 8//0x1ff
module_param_named(dpss_dbg_uv_swap, dpss_dbg_uv_swap, uint, 0664);
unsigned int dpss_dbg_h_buf_size;	// = 1920; //
module_param_named(dpss_dbg_h_buf_size, dpss_dbg_h_buf_size, uint, 0664);
unsigned int dpss_dbg_tbc;
module_param_named(dpss_dbg_tbc, dpss_dbg_tbc, uint, 0664);
//20250102 lukang add for irq
enum DPSS_FILM_MODE det_film_mode = DPSS_VIDEO;
u32 det_film_phs;
u32 det_filmmode_chg;
bool dpss_h_bypass;		// = true; //bit 0 for dae
module_param_named(dpss_h_bypass, dpss_h_bypass, bool, 0664);

bool dpss_dpe1_link0;		// = true; //bit 0 for dae
module_param_named(dpss_dpe1_link0, dpss_dpe1_link0, bool, 0664);

//cfg_at_inp_site
unsigned int dpss_dbg_cfg_at_inp_site = 1;// = 1920; //
module_param_named(dpss_dbg_cfg_at_inp_site, dpss_dbg_cfg_at_inp_site, uint, 0664);

unsigned int dpss_dbg_delay_cnt = 2;

bool dpss_get_h_bypass(void)
{
	return dpss_h_bypass;
}

void dbg_u64_print(void)//dbg only
{
	u64 tmp_t64 = 0x123456789abcdef0, tmp2_t64;

	//DBG_INF("size u64t=%d, 0x%llx, 0x%llx\n",
	//(unsigned int)sizeof(tmp_t64), tmp_t64, tmp_t64 << 2);
	DBG_INF("high:0x%x\n", (unsigned int)(tmp_t64 >> 32));
	tmp2_t64 = tmp_t64 << 2;
	DBG_INF("high:0x%x, low:0x%x\n", (unsigned int)(tmp2_t64 >> 32),
		(unsigned int)tmp2_t64);
	DBG_INF("llx:0x%llx, 0x%llx\n", tmp2_t64, tmp_t64);
}

void hw_val_init(unsigned int src)
{
	int i;

	dbg_h0("%s:reset to 0\n", __func__);
	//memset(&prm_dpss_top, 0, sizeof(prm_dpss_top));
	//memset(&prm_dpss_dae, 0, sizeof(prm_dpss_dae));
	//memset(&prm_dpss_dpe, 0, sizeof(prm_dpss_dpe));	//ary add 2025-02-28
	memset(&prm_size, 0, sizeof(prm_size));	//for hw_cfg_dpss_dae
	memset(&dae_yuv_intf, 0, sizeof(dae_yuv_intf));	//hw_cfg_dpss_dae
	//prm_dpss_dae.ext_yuv_intf = &dae_yuv_intf;
	//prm_dpss_dae.prm_size_ext = &prm_size;
	if (!src) {
		memset(&prm_dpss_dae_di, 0, sizeof(prm_dpss_dae_di));
		memset(&prm_size_di, 0, sizeof(prm_size_di));	//for hw_cfg_dpss_dae
		memset(&dae_yuv_intf_di, 0, sizeof(dae_yuv_intf_di));	//hw_cfg_dpss_dae
		prm_dpss_dae_di.ext_yuv_intf = &dae_yuv_intf_di;
		prm_dpss_dae_di.prm_size_ext = &prm_size_di;
		memset(&g_inp_yuv_rdmif, 0, sizeof(g_inp_yuv_rdmif));	//for _cfg_dpss_inp
		memset(&g_nr_pps_cfg, 0, sizeof(g_nr_pps_cfg));	//hw_cfg_dpss_dpe out
	} else {
		memset(&g_nr_pps_cfg2, 0, sizeof(g_nr_pps_cfg2));	//hw_cfg_dpss_dpe out
	}

//	memset(&g_pix_rmif, 0, sizeof(g_pix_rmif));	//hw_cfg_dpss_dpe_intf
#ifdef _HIS_CODE_	//will be copy when cfg_dpe
	memset(&g_pix_rmif2, 0, sizeof(g_pix_rmif2));	//hw_cfg_dpss_dpe_intf
	memset(&g_pix_rmif3, 0, sizeof(g_pix_rmif3));	//hw_cfg_dpss_dpe_intf
	memset(&g_pix_rmif4, 0, sizeof(g_pix_rmif4));	//hw_cfg_dpss_dpe_intf
#endif
	memset(&g_vfcd0, 0, sizeof(g_vfcd0));	//hw_cfg_dpss_dpe_intf
	memset(&g_vfcd1, 0, sizeof(g_vfcd1));	//hw_cfg_dpss_dpe_intf

	memset(&g_pix_wmif0, 0, sizeof(g_pix_wmif0));	//hw_cfg_dpss_dpe_intf
	memset(&g_pix_wmif1, 0, sizeof(g_pix_wmif1));	//hw_cfg_dpss_dpe_intf
	memset(&g_pix_wmif2, 0, sizeof(g_pix_wmif2));	//hw_cfg_dpss_dpe_intf
	memset(&g_ds_wmif0, 0, sizeof(g_ds_wmif0));	//hw_cfg_dpss_dpe_intf
	memset(&g_ds_wmif1, 0, sizeof(g_ds_wmif1));	//hw_cfg_dpss_dpe_intf
	memset(&g_vfce0, 0, sizeof(g_vfce0));	//hw_cfg_dpss_dpe_intf
	if (!src) {
		dpss_disp0_frm_cnt = 0;
		dpss_dae_frm_cnt_src0 = 0;
		dpss_dae_frm_cnt_src1 = 0;
		dpss_dpe_nr_frm_cnt = 0;
		dae_rd_less_one = 0;
		dpss_inp_frm_cnt = 0;
		dpe_pre_phs0_flg = 0;
		frc_vpp_link_obuf_rdy = 0;
		frc_ocnt_status = 0;
		mc_need_match = 0;
		mc_repeat_frm = 0;
		g_mc_phase = 0;
		src0_frm_idx_cnt = 0;
		//tbc
		inp_din_idx = 0;
		src_frm_idx = 0;	//input src 0

		inp_ibuf_level = 0;
		dae0_frm_rst_cnt = 0;
		dae1_frm_rst_cnt = 0;
		cfg_dae1_level = 0;
		fnr_dae_frm_idx = 0;	//dae done
		fnr_dpe_ibuf_level = 0;	//dae done
		fnr_dpe_din_idx = 0;
		fnr_dae_obuf_level = 6;	//dae done
		inp_obuf_level = 16;	//dae done
		fnr_dpe_frm_idx = 0;	//dpe done
		fnr_dae_ibuf_level = 0;
		fnr_dpe_obuf_level = 6;
		fnr_dpe_idx_rdpt = 0;
		//for wait mode
		for (i = 0; i < 10; i++)
			dpss_dae_done[i] = 0;
		dpss_wait_cnt1 = 0;
		//clear:
		for (i = 0; i < 16; i++)
			inp_fifo[i] = 0;	// 0
		for (i = 0; i < 8; i++)
			frc_dae_fifo[i] = 0;	// 1
		for (i = 0; i < 8; i++)
			fnr_dae_fifo[i] = 0;	//2
		for (i = 0; i < 8; i++)
			frc_dpe_fifo[i] = 0;	//3
		for (i = 0; i < 8; i++)
			fnr_dpe_fifo[i] = 0;	//4
		for (i = 0; i < 8; i++)
			fnr_dpe_idx_fifo[i] = 0;	//5
	} else {
		dpss_dae_frm_cnt_src2 = 0;
		dpss_dpe_di_frm_cnt = 0;
		src1_frm_idx_cnt = 0;

		dae2_frm_rst_cnt = 0;	//tbc:
	}
}

unsigned int dpss_dbg_fifo;
module_param_named(dpss_dbg_fifo, dpss_dbg_fifo, uint, 0664);

void dpss_dbg_fifo_pt(unsigned int idx)
{
	int i;

	if (idx == 0 && (dpss_dbg_fifo & C_BIT0)) {
		DBG_INF("inp_fifo\n");
		for (i = 0; i < 16; i++)
			DBG_INF("\t%d:0x%llx\n", i, inp_fifo[i]);
	} else if (idx == 1 && (dpss_dbg_fifo & C_BIT1)) {
		DBG_INF("frc_dae_fifo\n");
		for (i = 0; i < 8; i++)
			DBG_INF("\t%d:0x%llx\n", i, frc_dae_fifo[i]);
	} else if (idx == 2 && (dpss_dbg_fifo & C_BIT2)) {
		DBG_INF("fnr_dae_fifo\n");
		for (i = 0; i < 8; i++)
			DBG_INF("\t%d:0x%llx\n", i, fnr_dae_fifo[i]);
	} else if (idx == 3 && (dpss_dbg_fifo & C_BIT3)) {
		DBG_INF("frc_dpe_fifo\n");
		for (i = 0; i < 8; i++)
			DBG_INF("\t%d:0x%llx\n", i, frc_dpe_fifo[i]);
	} else if (idx == 4 && (dpss_dbg_fifo & C_BIT4)) {
		DBG_INF("fnr_dpe_fifo\n");
		for (i = 0; i < 8; i++)
			DBG_INF("\t%d:0x%llx\n", i, fnr_dpe_fifo[i]);
	} else if (idx == 5 && (dpss_dbg_fifo & C_BIT5)) {
		DBG_INF("fnr_dpe_idx_fifo\n");
		for (i = 0; i < 8; i++)
			DBG_INF("\t%d:0x%llx\n", i, fnr_dpe_idx_fifo[i]);
	}
}

//set after prm_top value have set
void hw_init_small_addr(struct PRM_DPSS_TOP *prm_top, unsigned int src)
{
	if (src > 1) {
		DBG_WARN("%s:src overflow %d\n", __func__, src);
		src = 0;
	}
	dbg_h1("%s:src[%d]\n", __func__, src);
	if (src == 0) {
		//prm_top->src0_me_alp_addr = prm_top->src0_mix_addr[1]; //tmp
		//prm_top->src0_me_alp_step = prm_top->src0_mix_step[1];
		//DBG_WARN("tmp set me_alp as src0_mix_addr\n");
		wr(DPSS_SRC0_MIX_CBUF_ADDR, prm_top->src0_mix_addr);	//

		wr(DPSS_SRC0_ME_INFO_ADDR, prm_top->src0_me_info_addr);
		wr(DPSS_SRC0_ME_MTN_ADDR, prm_top->src0_mtn_addr);
		wr(DPSS_SRC0_ME_ALP_ADDR, prm_top->src0_me_alp_addr);
		wr(DPSS_SRC0_ME_TFBF_ADDR, prm_top->src0_tfbf_addr);
		wr(DPSS_SRC0_NRDI_MV_ADDR, prm_top->src0_mv_addr);
		wr(DPSS_SRC0_DCG_HIST_ADDR, prm_top->src0_dct_his_addr);
		wr(DPSS_SRC0_DCG_YDS_ADDR, prm_top->src0_dct_y_addr);
		wr(DPSS_SRC0_DCG_CDS_ADDR, prm_top->src0_dct_c_addr);
		wr(DPSS_SRC0_NR_HDBLK_ADDR, prm_top->src0_grad_h_addr);
		wr(DPSS_SRC0_NR_VDBLK_ADDR, prm_top->src0_grad_v_addr);
		wr(DPSS_SRC0_NRODW_MBUF_ADDR, prm_top->src0_nro_dw_addr);
		wr(DPSS_SRC0_NR_DMSQ_ADDR, prm_top->src0_dmsq_addr);
		wr(DPSS_SRC0_NR_MERO_ADDR, prm_top->src0_ro1_addr);
		wr(DPSS_SRC0_DPE_RDMA_ADDR, prm_top->src0_ro2_addr);
		wr(DPSS_SRC0_NROUT_YHEAD_ADDR, prm_top->src0_afbce_h_yaddr);	//tmp ?
		wr(DPSS_SRC0_NROUT_CHEAD_ADDR, prm_top->src0_afbce_h_caddr);

		//wr(DPSS_SRC0_MIX_PBUF_STEP,    prm_top->src0_mix_step[0]);
		wr(DPSS_SRC0_MIX_CBUF_STEP, prm_top->src0_mix_step[0]);
		wr(DPSS_SRC0_ME_INFO_STEP, prm_top->src0_me_info_step);
		wr(DPSS_SRC0_ME_MTN_STEP, prm_top->src0_mtn_step);
		wr(DPSS_SRC0_ME_ALP_STEP, prm_top->src0_me_alp_step);
		wr(DPSS_SRC0_ME_TFBF_STEP, prm_top->src0_tfbf_step);
		wr(DPSS_SRC0_NRDI_MV_STEP, prm_top->src0_mv_step);
		wr(DPSS_SRC0_DCG_HIST_STEP, prm_top->src0_dct_his_step);
		wr(DPSS_SRC0_DCG_YDS_STEP, prm_top->src0_dct_y_step);
		wr(DPSS_SRC0_DCG_CDS_STEP, prm_top->src0_dct_c_step);
		wr(DPSS_SRC0_NR_HDBLK_STEP, prm_top->src0_grad_h_step);
		wr(DPSS_SRC0_NR_VDBLK_STEP, prm_top->src0_grad_v_step);
		wr(DPSS_SRC0_NRODW_MBUF_STEP, prm_top->src0_nro_dw_step);
		wr(DPSS_SRC0_NR_DMSQ_STEP, prm_top->src0_dmsq_step);
		wr(DPSS_SRC0_MERO_STEP, prm_top->src_ro_step);
		wr(DPSS_SRC0_DPE_RDMA_STEP, prm_top->src_ro_step);

		wr(DPSS_SRC0_NROUT_YHEAD_STEP, prm_top->src0_afbce_h_y_step);
		wr(DPSS_SRC0_NROUT_CHEAD_STEP, prm_top->src0_afbce_h_c_step);

		wr(DPSS_INP_MBUF_ADDR0, prm_top->inp_mbuf_addr[0]);
		wr(DPSS_INP_MBUF_ADDR1, prm_top->inp_mbuf_addr[1]);
		wr(DPSS_FRC_ME_MBUF_ADDR, prm_top->frc_me_mbuf_addr);
		wr(DPSS_SRC0_ME_NC_UNI_MV_ADDR,
		   prm_top->src0_me_nc_uni_mv_addr);
		wr(DPSS_SRC0_ME_CN_UNI_MV_ADDR,
		   prm_top->src0_me_cn_uni_mv_addr);
		wr(DPSS_SRC0_ME_PC_PHS_MV_ADDR,
		   prm_top->src0_me_pc_phs_mv_addr);
		wr(DPSS_SRC0_LOGO_IIR_BUF_ADDR, prm_top->src0_logo_iir_addr);
		wr(DPSS_SRC0_LOGO_SCC_BUF_ADDR, prm_top->src0_logo_scc_addr);
		wr(DPSS_SRC0_BLK_LOGO_ADDR, prm_top->src0_blk_logo_addr);
		wr(DPSS_SRC0_PIX_LOGO_ADDR, prm_top->src0_pix_logo_addr);
		wr(DPSS_SRC0_VP_MC_MV_ADDR, prm_top->src0_vp_mc_mv_addr);
		wr(DPSS_SRC0_VP_MC_LOGO_ADDR, prm_top->src0_vp_mc_logo_addr);
		wr(DPSS_SRC0_FRC_MERO_ADDR, prm_top->src0_frc_mero_addr);

		// wr(DPSS_INP_MBUF_STEP, prm_top->inp_mbuf_step);
		wr(DPSS_FRC_ME_MBUF_STEP, prm_top->frc_me_mbuf_step);
		wr(DPSS_SRC0_ME_NC_UNI_MV_STEP,
		   prm_top->src0_me_nc_uni_mv_step);
		wr(DPSS_SRC0_ME_CN_UNI_MV_STEP,
		   prm_top->src0_me_cn_uni_mv_step);
		wr(DPSS_SRC0_ME_PC_PHS_MV_STEP,
		   prm_top->src0_me_pc_phs_mv_step);
		wr(DPSS_SRC0_LOGO_IIR_BUF_STEP, prm_top->src0_logo_iir_step);
		wr(DPSS_SRC0_LOGO_SCC_BUF_STEP, prm_top->src0_logo_scc_step);
		wr(DPSS_SRC0_BLK_LOGO_STEP, prm_top->src0_blk_logo_step);
		wr(DPSS_SRC0_PIX_LOGO_STEP, prm_top->src0_pix_logo_step);
		wr(DPSS_SRC0_VP_MC_MV_STEP, prm_top->src0_vp_mc_mv_step);
		wr(DPSS_SRC0_VP_MC_LOGO_STEP, prm_top->src0_vp_mc_logo_step);
		wr(DPSS_FRC_MERO_STEP, prm_top->src0_frc_mero_step);
		dbg_h1("    wr reg:DPSS_FRC_ME_MBUF_ADDR: = 0x%x\n",
		       rd(DPSS_FRC_ME_MBUF_ADDR));
		dbg_h1("    wr reg:DPSS_SRC0_ME_NC_UNI_MV_ADDR = 0x%x\n",
		       rd(DPSS_SRC0_ME_NC_UNI_MV_ADDR));
	} else {		// (src == 1)
		//wr(DPSS_SRC1_MIX_PBUF_ADDR,    (unsigned int)(prm_top->src1_mix_addr[0]));
		//wr(DPSS_SRC1_MIX_CBUF_ADDR,    (unsigned int)(prm_top->src1_mix_addr[1]));
		//DPSS_SRC1_NRODW_MBUF_ADDR
		wr(DPSS_SRC1_NRODW_MBUF_ADDR, prm_top->src1_mix_addr);
		wr(DPSS_SRC1_NRODW_MBUF_STEP, prm_top->src1_mix_step[0]);

		wr(DPSS_SRC1_ME_INFO_ADDR, prm_top->src1_me_info_addr);
		wr(DPSS_SRC1_ME_MTN_ADDR, prm_top->src1_mtn_addr);
		wr(DPSS_SRC1_ME_ALP_ADDR, prm_top->src1_me_alp_addr);
		wr(DPSS_SRC1_ME_TFBF_ADDR, prm_top->src1_tfbf_addr);
		wr(DPSS_SRC1_NRDI_MV_ADDR, prm_top->src1_mv_addr);
		wr(DPSS_SRC1_DCG_HIST_ADDR, prm_top->src1_dct_his_addr);
		wr(DPSS_SRC1_DCG_YDS_ADDR, prm_top->src1_dct_y_addr);
		wr(DPSS_SRC1_DCG_CDS_ADDR, prm_top->src1_dct_c_addr);
		wr(DPSS_SRC1_NR_HDBLK_ADDR, prm_top->src1_grad_h_addr);
		wr(DPSS_SRC1_NR_VDBLK_ADDR, prm_top->src1_grad_v_addr);

		wr(DPSS_SRC1_NR_DMSQ_ADDR, prm_top->src1_dmsq_addr);
		wr(DPSS_SRC1_DI_MERO_ADDR, prm_top->src1_ro1_addr);
		wr(DPSS_SRC1_DPE_RDMA_ADDR, prm_top->src1_ro2_addr);

		//wr(DPSS_SRC1_MIX_PBUF_STEP,    prm_top->src1_mix_step[0]);
		//wr(DPSS_SRC1_MIX_CBUF_STEP,    prm_top->src1_mix_step[1]);
		wr(DPSS_SRC1_ME_INFO_STEP, prm_top->src1_me_info_step);
		wr(DPSS_SRC1_ME_MTN_STEP, prm_top->src1_mtn_step);
		wr(DPSS_SRC1_ME_ALP_STEP, prm_top->src1_me_alp_step);
		wr(DPSS_SRC1_ME_TFBF_STEP, prm_top->src1_tfbf_step);
		wr(DPSS_SRC1_NRDI_MV_STEP, prm_top->src1_mv_step);
		wr(DPSS_SRC1_MERO_STEP, prm_top->src_ro_step);
		wr(DPSS_SRC1_DPE_RDMA_STEP, prm_top->src_ro_step);
		wr(DPSS_SRC1_DCG_HIST_STEP, prm_top->src1_dct_his_step);
		wr(DPSS_SRC1_DCG_YDS_STEP, prm_top->src1_dct_y_step);
		wr(DPSS_SRC1_DCG_CDS_STEP, prm_top->src1_dct_c_step);
		wr(DPSS_SRC1_NR_HDBLK_STEP, prm_top->src1_grad_h_step);
		wr(DPSS_SRC1_NR_VDBLK_STEP, prm_top->src1_grad_v_step);
		//wr(DPSS_SRC1_NRODW_MBUF_STEP, prm_top->src1_mix_step[0]);     //mix ? have y only?
		wr(DPSS_SRC1_NR_DMSQ_STEP, prm_top->src1_dmsq_step);
	}
}

/*****************************************/

/******************************************/
//-----------------------------------------------------------------------------

void hw_dpss_patch_for_bitmatch(u32 inp_frm_cnt,
				u32 dae_frm_cnt_src0,
				u32 dae_frm_cnt_src1,
				u32 dae_frm_cnt_src2,
				u32 dpe_nr_frm_cnt,
				u32 dpe_di_frm_cnt,
				u32 *di_ignore_num_out,
				u32 *nr_ignore_num_out,
				u32 *inp_frm_cnt_adj_out,
				u32 *dae_frm_cnt_src0_adj_out,
				u32 *dae_frm_cnt_src1_adj_out,
				u32 *dae_frm_cnt_src2_adj_out,
				u32 *dpe_nr_frm_cnt_adj_out,
				u32 *dpe_di_frm_cnt_adj_out)
{
	if (dpss_dbg_match_dis) {
		*di_ignore_num_out = 2;
		*nr_ignore_num_out = 1;
		*inp_frm_cnt_adj_out = dpss_inp_frm_cnt;
		*dae_frm_cnt_src0_adj_out = dpss_dae_frm_cnt_src0;
		*dae_frm_cnt_src1_adj_out = dpss_dae_frm_cnt_src1;
		*dae_frm_cnt_src2_adj_out = dpss_dae_frm_cnt_src2;
		*dpe_nr_frm_cnt_adj_out = dpss_dpe_nr_frm_cnt;
		*dpe_di_frm_cnt_adj_out = dpss_dpe_di_frm_cnt;
		return;
	}
	u32 dpss_dae_frm_cnt_src1_adj;

	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;
	u32 bbd_only = (rd(DPSS_FBUF_TOP_CTRL) >> 6) & 0x1;

	u32 dpss_di_ignore_num = 2;
	u32 dpss_inp_ignore_num = 0;	//(m_di_frc_link ? dpss_di_ignore_num:0);
	//(m_di_frc_link && dpss_di_film_mode == 2) ? 1 + 1: dpss_inp_ignore_num + 1;
	u32 dpss_nr_ignore_num = src0_nrdi_frc_en ? dpss_di_ignore_num : 1;
#ifdef DPSS_GAME_MODE_DLY1
	u32 dpss_frc_ignore_num = 1;
#else
	u32 dpss_frc_ignore_num = 2;
#endif

	u32 dpss_inp_frm_cnt_adj = inp_frm_cnt <= dpss_inp_ignore_num ? 0 :
	    (inp_frm_cnt - dpss_inp_ignore_num);

	u32 dpss_dae_frm_cnt_src0_adj = dae_frm_cnt_src0 <= dpss_frc_ignore_num ? 0 :
					(dae_frm_cnt_src0 - dpss_frc_ignore_num);
	if (src0_nrdi_frc_en == 0) {
		dpss_dae_frm_cnt_src1_adj = dae_frm_cnt_src1 <= dpss_nr_ignore_num  ? 0 :
					(dae_frm_cnt_src1 - dpss_nr_ignore_num);
	} else {
		dpss_dae_frm_cnt_src1_adj = dae_frm_cnt_src1;
	}
	u32 dpss_dae_frm_cnt_src2_adj = dae_frm_cnt_src2;

	u32 dpss_dpe_nr_frm_cnt_adj = dpe_nr_frm_cnt <= dpss_nr_ignore_num ? 0 :
	    (dpe_nr_frm_cnt - dpss_nr_ignore_num);
	u32 dpss_dpe_di_frm_cnt_adj = dpe_di_frm_cnt <= dpss_di_ignore_num ? 0 :
	    (dpe_di_frm_cnt - dpss_di_ignore_num);

	*di_ignore_num_out = dpss_di_ignore_num;
	*nr_ignore_num_out = dpss_nr_ignore_num;
	*inp_frm_cnt_adj_out = dpss_inp_frm_cnt_adj;
	*dae_frm_cnt_src0_adj_out = dpss_dae_frm_cnt_src0_adj;
	*dae_frm_cnt_src1_adj_out = dpss_dae_frm_cnt_src1_adj;
	*dae_frm_cnt_src2_adj_out = dpss_dae_frm_cnt_src2_adj;
	*dpe_nr_frm_cnt_adj_out = dpss_dpe_nr_frm_cnt_adj;
	*dpe_di_frm_cnt_adj_out = dpss_dpe_di_frm_cnt_adj;

	//just print 1st time
	if (inp_frm_cnt == 0 && dae_frm_cnt_src2 == 0) {
		wr(FRC_DAE_FRM_DLY_NUM, dpss_frc_ignore_num);
		w_reg_bit(DPSS_FBUF_DAE_INIT, bbd_only ? 0 : dpss_nr_ignore_num, 8, 5);
		w_reg_bit(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DAE_INIT, dpss_di_ignore_num, 8, 5);
	}
	dbg_h2("in:inp_frm_cnt =%d, dae:<%d, %d, %d>, dpe:<%d, %d>\n",
	       inp_frm_cnt, dae_frm_cnt_src0, dae_frm_cnt_src1,
	       dae_frm_cnt_src2, dpe_nr_frm_cnt, dpe_di_frm_cnt);
	dbg_h2("out:ignore:<%d, %d>, adj:in<%d>,dae<%d,%d,%d>,dpe<%d,%d>\n",
	       *di_ignore_num_out, *nr_ignore_num_out, *inp_frm_cnt_adj_out,
	       *dae_frm_cnt_src0_adj_out, *dae_frm_cnt_src1_adj_out,
	       *dae_frm_cnt_src2_adj_out, *dpe_nr_frm_cnt_adj_out,
	       *dpe_di_frm_cnt_adj_out);
}

//ary #define FUNC_CHECK_RO_EN  (1)
void hw_check_nr_ro(int frm_idx, enum PRM_SRC_IDX dump_src_idx)
{
	u32 dpe_pre_src_idx;
	enum PRM_SRC_IDX src_idx;
	u32 bbd_sel;

	dpe_pre_src_idx = ((rd(DPSS_SRC_INDEX) >> 8) & 0xf);
	src_idx = (enum PRM_SRC_IDX)dpe_pre_src_idx;
	bbd_sel = (rd(DPSS_DPE_BBD_CTRL)) & 0x3;

	if (src_idx != dump_src_idx)
		return;

#ifdef NR_BBD_TEST
	if (dump_src_idx == SRC_IDX_NR) {
		if (bbd_sel == 0x2)
			check_mc_bbd_ro(frm_idx, 0);
	}
#endif
#ifdef FUNC_CHECK_RO_EN		//ary add
	check_ne_ro(frm_idx, src_idx);
	check_hist_ro(frm_idx, src_idx);
#endif
}

void hw_cfg_vfcd_rdmif_2ch_update_phase(enum dpss_mif_e mif_index, u32 tb_flag)
{
	s32 regs_ofst;

	regs_ofst = get_vfcd_regs_ofst(mif_index);
	if (tb_flag)//bottom
		w_reg_bit_vc(regs_ofst + VFCD_AFBC_VD_CFMT_CTRL, 0xa, 8, 4);
	else
		w_reg_bit_vc(regs_ofst + VFCD_AFBC_VD_CFMT_CTRL, 0xe, 8, 4);
}

void hw_cfg_dae_rdmif_update_phase(u32 tb_flag)
{
	if (tb_flag) {//bottom
		w_reg_bit(INP_VD_CFMT_CTRL, 0xa, 8, 4);
		w_reg_bit(DCT_YCFLT_VD_CFMT_CTRL, 0xa, 8, 4);
		w_reg_bit(DAE_VD_CFMT_CTRL, 0xa, 8, 4);
	} else {
		w_reg_bit(INP_VD_CFMT_CTRL, 0xe, 8, 4);
		w_reg_bit(DCT_YCFLT_VD_CFMT_CTRL, 0xe, 8, 4);
		w_reg_bit(DAE_VD_CFMT_CTRL, 0xe, 8, 4);
	}
}

//yu.zong 2024-12-06 ary add:
void hw_cfg_vfcd_rdmif_2ch_update_y_st_only(enum dpss_mif_e mif_index,
					    struct PRM_INTF_TYPE *prm_mif)
{
	s32 regs_ofst;

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	u32 slc0_y_st_luma = prm_mif->slc_y_st[0];
	u32 slc1_y_st_luma = prm_mif->slc_y_st[1];
	u32 slc2_y_st_luma = prm_mif->slc_y_st[2];
	u32 slc3_y_st_luma = prm_mif->slc_y_st[3];

	u32 slc0_y_st_chrm =
	    prm_mif->src_fmt == YUV420 ? slc0_y_st_luma >> 1 : slc0_y_st_luma;
	u32 slc1_y_st_chrm =
	    prm_mif->src_fmt == YUV420 ? slc1_y_st_luma >> 1 : slc1_y_st_luma;
	u32 slc2_y_st_chrm =
	    prm_mif->src_fmt == YUV420 ? slc2_y_st_luma >> 1 : slc2_y_st_luma;
	u32 slc3_y_st_chrm =
	    prm_mif->src_fmt == YUV420 ? slc3_y_st_luma >> 1 : slc3_y_st_luma;

	w_reg_bit_vc(regs_ofst + LUMA_RMIF_SCOPE_Y, slc0_y_st_luma, 0, 13);	//ch0 slc0 scope_y
	w_reg_bit_vc(regs_ofst + LUMA_SLC1_YSIZE, slc1_y_st_luma, 0, 13);	//ch0 slc1 scope_y
	w_reg_bit_vc(regs_ofst + LUMA_SLC2_YSIZE, slc2_y_st_luma, 0, 13);	//ch0 slc2 scope_y
	w_reg_bit_vc(regs_ofst + LUMA_SLC3_YSIZE, slc3_y_st_luma, 0, 13);	//ch0 slc2 scope_y

	w_reg_bit_vc(regs_ofst + CHRM_RMIF_SCOPE_Y, slc0_y_st_chrm, 0, 13);	//ch1 slc0 scope_y
	w_reg_bit_vc(regs_ofst + CHRM_SLC1_YSIZE, slc1_y_st_chrm, 0, 13);      //ch1 slc1 scope_y
	w_reg_bit_vc(regs_ofst + CHRM_SLC2_YSIZE, slc2_y_st_chrm, 0, 13);      //ch1 slc2 scope_y
	w_reg_bit_vc(regs_ofst + CHRM_SLC3_YSIZE, slc3_y_st_chrm, 0, 13);      //ch1 slc2 scope_y

};

void hw_dae_update_interlace_cfg(struct PRM_DPSS_TOP *prm_top,
			     unsigned int cnt)
{
	u32 di_cur_ybgn;

	if (prm_top->is_i) {
		di_cur_ybgn = (cnt + prm_top->f_top + 1) & 0x1;
		dbg_h2("%s:%d\n", __func__, di_cur_ybgn);

		w_reg_bit(VPU_NRME_TOP_CTRL1, 1, 3, 1);
		w_reg_bit(VPU_NRME_TOP_CTRL1, 1, 1, 1);
		if (di_cur_ybgn) {
			w_reg_bit(VPU_NRME_TOP_CTRL1, 0, 2, 1);
			w_reg_bit(VPU_NRME_TOP_CTRL1, 0, 0, 1);
		} else {
			w_reg_bit(VPU_NRME_TOP_CTRL1, 1, 2, 1);
			w_reg_bit(VPU_NRME_TOP_CTRL1, 1, 0, 1);
		}
	} else {
		w_reg_bit(VPU_NRME_TOP_CTRL1, 0, 3, 1);
		w_reg_bit(VPU_NRME_TOP_CTRL1, 0, 1, 1);
	}
}

//ary add
void hw_dae_update_interlace(struct PRM_DPSS_TOP *prm_top,
			     struct PRM_DPSS_DAE *prm_dae,
			     unsigned int cnt)
{
//	int i;
	u32 di_cur_ybgn;
	struct PRM_INTF_TYPE *dae_yuv = &prm_dae->nr_yuv_intf;
	unsigned int y, u;

	if (!prm_top->di_interlace)
		return;
	di_cur_ybgn = (cnt + prm_top->f_top + 1) & 0x1;
	dbg_h2("%s:%d\n", __func__, di_cur_ybgn);

	y = dae_yuv->back_2_y | di_cur_ybgn;
	u = dae_yuv->back_2_u | di_cur_ybgn;
	hw_cfg_dae_rdmif_update_phase(di_cur_ybgn);
	dpss_cfg_viu_pix_rdmif_di_update(y, u);
}

void hw_dpe_update_interlace(struct PRM_DPSS_TOP *prm_top,
			     struct PRM_DPSS_DPE *prm_dpe)
{
	int i;
	u32 di_cur_ybgn;

	if (!prm_top->di_interlace)
		return;
	di_cur_ybgn = (prm_dpe->dpss_dpe_di_frm_cnt + prm_top->f_top + 1) & 0x1;
	dbg_h2("%s:%d\n", __func__, di_cur_ybgn);

	for (i = 0; i < 4; i++) {
		prm_dpe->pix_rmif1.slc_y_st[i] =
		    prm_top->di_interlace ? di_cur_ybgn : 0;
		//
	}
	hw_cfg_vfcd_rdmif_2ch_update_phase(DPSS_RMIF_DPE1, di_cur_ybgn);
	hw_cfg_vfcd_rdmif_2ch_update_y_st_only(DPSS_RMIF_DPE1, &prm_dpe->pix_rmif1);	//di current
}

void hw_cfg_vfcd_rdmif_2ch_addr_only(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif,
				     u32 fmt444_out)
{
	u32 regs_ofst;

	dbg_h2("%s\n", __func__);

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	u32 y_baddr = prm_mif->src_baddr[0];
	u32 u_baddr = prm_mif->src_baddr[1];

	dbg_h2("%s:addr:in:mif_index:%d,yaddr:0x%x\n", __func__, mif_index, y_baddr);

	wr_vc(regs_ofst + MIF0_RMIF_CTRL4, y_baddr >> 4);	//ch0 baddr
	wr_vc(regs_ofst + MIF1_RMIF_CTRL4, u_baddr >> 4);	//ch1 baddr
};

void hw_cfg_vfcd_afrcd_addr_only(u32 src_sel, struct AFRCD_TYPE *afrcd)
{
	u32 reg_ofst = src_sel;

	dbg_h2("%s:\n", __func__);
	w_reg_bit_vc(reg_ofst + VFCD_MMU_BADDR0, afrcd->mmu_baddr0, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_MMU_BADDR1, afrcd->mmu_baddr1, 0, 32);

	w_reg_bit_vc(reg_ofst + VFCD_AFBC_HEAD_BADDR, afrcd->luma_head_addr, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_AFBC_BODY_BADDR, afrcd->luma_body_addr, 0, 32);

	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CHRM_HEAD_BADDR, afrcd->chrm_head_addr, 0, 32);
	w_reg_bit_vc(reg_ofst + VFCD_AFRC_CHRM_BODY_BADDR, afrcd->chrm_body_addr, 0, 32);
}

void hw_cfg_dpss_afbcd_mult_addr_only(u32 module_sel,
	u32 mif_info_baddr, u32 mif_data_baddr)
{
	u32 regs_ofst;

	regs_ofst = module_sel * 256;	//ary addr change *4

	wr_vc((regs_ofst + VFCD_AFBC_HEAD_BADDR), mif_info_baddr);
	wr_vc((regs_ofst + VFCD_AFBC_BODY_BADDR), mif_data_baddr);

}				//cfg_dpss_afbcd_mult

void hw_cfg_vfcd_dec_addr_only(u32 index, struct VFCD_t *vfcd)
{
	dbg_h2("%s:index:0x%x\n", __func__, index);

	u32 cmpr_sel = vfcd->cmpr_sel;
	u32 regs_ofst = index * 256;	//ary addr change *4

	vfcd->afbcd.head_baddr = vfcd->luma_head_baddr;
	vfcd->afbcd.body_baddr = vfcd->luma_body_baddr;

	vfcd->afrcd.luma_head_addr = vfcd->luma_head_baddr;
	vfcd->afrcd.luma_body_addr = vfcd->luma_body_baddr;

	vfcd->afrcd.chrm_head_addr = vfcd->chrm_head_baddr;
	vfcd->afrcd.chrm_body_addr = vfcd->chrm_body_baddr;

	vfcd->afrcd.mmu_baddr0 = vfcd->mmu_baddr0;
	vfcd->afrcd.mmu_baddr1 = vfcd->mmu_baddr1;

	if (cmpr_sel == 1) {	//afbc
		//ary cfg_dpss_afbcd(index, hfmt_en, vfmt_en, &vfcd->afbcd); -> cfg_dpss_afbcd_mult
		hw_cfg_dpss_afbcd_mult_addr_only(index, vfcd->afbcd.head_baddr,
						 vfcd->afbcd.body_baddr);
		dbg_h2("vfcd_afbc_cfg_done\n");
	} else if (cmpr_sel == 2) {	//afrc
		hw_cfg_vfcd_afrcd_addr_only(regs_ofst, &vfcd->afrcd);
		dbg_h2("vfcd_afrc_cfg_done\n");
	}
}

void hw_cfg_dpe_pix_wrmif_addr_only(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif)
{
	u32 regs_ofst = mif_index == DPSS_WMIF_DPE0 ? 0x0	//wr0   //ary addr change *4
	    : mif_index == DPSS_WMIF_DPE1 ? 0x20	//wr0
	    : mif_index == DPSS_WMIF_DPE2 ? 0x40	//wr0
	    : mif_index == DPSS_WMIF_DPE3 ? 0x60	//ds0
	    : mif_index == DPSS_WMIF_DPE4 ? 0x80 : 0xa0;

	u32 luma_baddr;
	u32 chrm_baddr;

	dbg_h2("%s\n", __func__);

	dbg_h2("%s:mif_index=%d,regs_ofst=0x%x\n", __func__, mif_index,
	       regs_ofst);

	luma_baddr = prm_mif->src_baddr[0];
	chrm_baddr = prm_mif->src_baddr[1];

	dbg_h2("[dpss_dpe.c]:wrmif luma_baddr=%x  wrmif chrm_baddr=%x\n",
	       luma_baddr, chrm_baddr);

	wr(regs_ofst + VID_WMIF_LUMA_BADDR, luma_baddr);	//reg_luma_baddr
	wr(regs_ofst + VID_WMIF_CHRM_BADDR, chrm_baddr);	//reg_chrm_baddr

};

void hw_cfg_vfce_addr_only(int index, int channel, struct VFCE_TYPE *prm)
{
	int reg_offset;
//	VFCE_CALC_t cal;

	reg_offset = get_vfce_offset(index) * 0x100 + channel * 0x80;	//ary addr change *4

	//config channel FE
	hw_cfg_vfce_chnl_addr_only(reg_offset, prm);

	//config channel BE
	hw_cfg_vfce_chnl_addr_only(reg_offset + 32, prm);	//ary addr /4

}				/* cfg_vfce */

void hw_cfg_dpss_dpe_intf_addr_only(struct PRM_DPSS_TOP   *prm_top,
	struct PRM_DPSS_DPE  *prm_dpe)
{
	enum DPSS_WORK_MODE dpe_mode = prm_dpe->dpe_mode;

	struct PRM_INTF_TYPE *pix_rmif;	//ary change
	//memset((void *)(&pix_rmif), 0, sizeof(struct PRM_INTF_TYPE));
//	pix_rmif = &g_pix_rmif;
	pix_rmif = &prm_dpe->pix_rmif_tmp;

	u32 dpe_src0_idx = rd(DPSS_FBUF_DPE_LOOP_IDX);
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
	dbg_h2("%s:ary_sim:dpe_src0_idx:%d->%d\n", __func__, g_dpe_index,
	       dpe_src0_idx);

#endif

	u32 dpe_src1_idx = rd(DPSS_REGOFST_VDICTRL + DPSS_FBUF_DPE_LOOP_IDX);
	u32 dpe_src0_cur0_rd_idx = (dpe_src0_idx >> 24) & 0xf;
	u32 dpe_src0_pre1_rd_idx = (dpe_src0_idx >> 20) & 0xf;
	u32 dpe_src0_pre2_rd_idx = (dpe_src0_idx >> 16) & 0xf;
	u32 dpe_src0_nr_wr_idx = (dpe_src0_idx >> 4) & 0xf;
	u32 dpe_src1_cur0_rd_idx = (dpe_src1_idx >> 24) & 0xf;
	u32 dpe_src1_pre1_rd_idx = (dpe_src1_idx >> 20) & 0xf;
	u32 dpe_src1_pre2_rd_idx = (dpe_src1_idx >> 16) & 0xf;
	u32 dpe_src1_nr_wr_idx = (dpe_src1_idx >> 4) & 0xf;
	u32 dpe_src1_di_wr_idx = (dpe_src1_idx >> 0) & 0xf;

	dbg_h2("%s:idx:wr:%d:%d,%d,%d\n", __func__,
	       dpe_src0_nr_wr_idx,
	       dpe_src0_cur0_rd_idx,
	       dpe_src0_pre1_rd_idx, dpe_src0_pre2_rd_idx);

	u32 fmt444_out = (dpe_mode != DPE_MC_MODE);

	struct PRM_INTF_TYPE *pix_rmif2 = &g_pix_rmif2;
	struct PRM_INTF_TYPE *pix_rmif3 = &g_pix_rmif3;
	struct PRM_INTF_TYPE *pix_rmif4 = &g_pix_rmif4;

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
		dbg_h2("%s:addr:nro: nr_mode:wr pix_rmif2 :%d:yaddr:0x%x\n",
		       __func__, dpe_src0_pre1_rd_idx, pix_rmif2->src_baddr[0]);
		break;
	case DPE_VDI_MODE:	//di_only
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
	case DPE_NRDI_MODE:	//nr+di
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
		pix_rmif->src_baddr[0] = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 9;
		pix_rmif->src_baddr[1] = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 9;
		break;
	default:
		dbg_h2("%s:5 %d not in case\n", __func__, dpe_mode);
		break;
	}

	hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE0, pix_rmif, fmt444_out);	//pix_cur
	hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE1, pix_rmif2, fmt444_out);	//pix_pre1
	hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE2, pix_rmif3, fmt444_out);	//pix_pre2
	hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE3, pix_rmif4, fmt444_out);	//?

	bool dpss_nr_vpp_link = (rd(DPSS_NR_VPP_LINK) & 0x1);
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	if (dpss_nr_vpp_link) {
		if (src0_nrdi_frc_en) {	//src0 di link
			dbg_h2("------di vpp link config rdmif------\n");
			hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_MC0,
				pix_rmif2, fmt444_out);	//cur
			hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE0,
				pix_rmif3, fmt444_out);	//pre
			hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_DPE1,
				pix_rmif4, fmt444_out);	//pre2
		} else {	//src0 nr link
			dbg_h2("------nr vpp link config rdmif------\n");
			hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_MC0, pix_rmif,
							fmt444_out);
			hw_cfg_vfcd_rdmif_2ch_addr_only(DPSS_RMIF_MC1,
							pix_rmif2, fmt444_out);
		}
	}
	struct VFCD_t *vfcd0 = &g_vfcd0;
	struct VFCD_t *vfcd1 = &g_vfcd1;

	switch (dpe_mode) {
	case DPE_NR_MODE:
		vfcd0->luma_head_baddr = (prm_top->di_frc_link ?
					prm_top->src1_dio_head_yaddr[dpe_src0_cur0_rd_idx] :
					prm_top->src0_head_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->luma_body_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_head_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_head_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_caddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_body_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd1->luma_head_baddr = prm_top->src0_nro_head_yaddr[dpe_src0_pre1_rd_idx] << 5;
		vfcd1->luma_body_baddr = prm_top->src0_nro_fbuf_yaddr[dpe_src0_pre1_rd_idx] << 5;
		dbg_h2("%s:addr:nro: nr_mode:wr vfcd1 :%d:yaddr:0x%x\n", __func__,
						dpe_src0_pre1_rd_idx, vfcd1->luma_body_baddr);
		vfcd1->chrm_head_baddr = prm_top->src0_nro_head_caddr[dpe_src0_pre1_rd_idx] << 5;
		vfcd1->chrm_body_baddr = prm_top->src0_nro_fbuf_caddr[dpe_src0_pre1_rd_idx] << 5;
		break;
	case DPE_VDI_MODE:	//di_only
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
	case DPE_NRDI_MODE:	//nr+di
		vfcd1->luma_head_baddr =
		    prm_top->src1_head_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->luma_body_baddr =
		    prm_top->src1_fbuf_yaddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->chrm_head_baddr =
		    prm_top->src1_head_caddr[dpe_src1_cur0_rd_idx] << 5;
		vfcd1->chrm_body_baddr =
		    prm_top->src1_fbuf_caddr[dpe_src1_cur0_rd_idx] << 5;
		break;
	case DPE_NR_BYPS:
		vfcd0->luma_head_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_head_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->luma_body_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_yaddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_yaddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_head_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_head_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_head_caddr[dpe_src0_cur0_rd_idx]) << 5;
		vfcd0->chrm_body_baddr = (prm_top->di_frc_link ?
			prm_top->src1_dio_fbuf_caddr[dpe_src0_cur0_rd_idx] :
			prm_top->src0_fbuf_caddr[dpe_src0_cur0_rd_idx]) << 5;
		break;
	default:
		dbg_h2("%s:4 %d not in case\n", __func__, dpe_mode);
		break;
	}

	hw_cfg_vfcd_dec_addr_only(0, vfcd0);	//nr din cur
	hw_cfg_vfcd_dec_addr_only(1, vfcd1);	//nr din pre

	u32 wrpath_sel = (rd(DPSS_DPE_INTF_AFBCD0) >> 12) & 0x1ffff;
	u32 w0_sel = (wrpath_sel >> 7) & 0x3;
	u32 w1_sel = (wrpath_sel >> 10) & 0x3;
	u32 vfce_on = (wrpath_sel) & 0x1;

	dbg_h2("%s:wrpath_sel = 0x%x, <%d,%d> vfce_on[%d]\n", __func__,
	       wrpath_sel, w0_sel, w1_sel, vfce_on);
	struct PRM_INTF_TYPE *pix_wmif0 = &g_pix_wmif0;
	struct PRM_INTF_TYPE *pix_wmif1 = &g_pix_wmif1;
	struct PRM_INTF_TYPE *pix_wmif2 = &g_pix_wmif2;

	switch (dpe_mode) {
	case DPE_NR_MODE:
		pix_wmif2->src_baddr[0] =
		    prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		pix_wmif2->src_baddr[1] =
		    prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		pix_wmif1->src_baddr[0] =
		    prm_top->src0_dio_fbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		pix_wmif1->src_baddr[1] =
		    prm_top->src0_dio_fbuf_caddr[dpe_src0_nr_wr_idx] << 5;

		dbg_h2("%s:addr:nro: nr_mode:wr pix_wmif2 :%d:yaddr:0x%x\n",
		       __func__, dpe_src0_nr_wr_idx, pix_wmif2->src_baddr[0]);
		break;
	case DPE_VDI_MODE:	//di_only
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
	case DPE_NRDI_MODE:	//nr+di
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
		dbg_h2("%s:addr:nro: DPE_NR_BYPS:wr pix_wmif2 :%d:yaddr:0x%x\n",
		       __func__, dpe_src0_nr_wr_idx, pix_wmif2->src_baddr[0]);
		break;
	default:
		dbg_h2("%s:3 %d not in case\n", __func__, dpe_mode);
		break;
	}

	struct PRM_INTF_TYPE *ds_wmif0 = &g_ds_wmif0;
	//struct PRM_INTF_TYPE *ds_wmif1 = &g_ds_wmif1;

	switch (dpe_mode) {
	case DPE_NR_MODE:
		ds_wmif0->src_baddr[0] =
		    prm_top->src0_nro_dwbuf_yaddr[dpe_src0_nr_wr_idx] << 5;
		ds_wmif0->src_baddr[1] =
		    prm_top->src0_nro_dwbuf_caddr[dpe_src0_nr_wr_idx] << 5;
		break;
	case DPE_VDI_MODE:	//di_only
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
	case DPE_NRDI_MODE:	//nr+di
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
		DBG_WARN("%s:2 %d not in case\n", __func__, dpe_mode);
		break;
	}

	if (w0_sel != 7)
		hw_cfg_dpe_pix_wrmif_addr_only(DPSS_WMIF_DPE0, pix_wmif0);	//nr_wrmif
	if (w1_sel != 7)
		hw_cfg_dpe_pix_wrmif_addr_only(DPSS_WMIF_DPE1, pix_wmif1);	//di_wrmif

	if (vfce_on) {
		//channel 0
		struct VFCE_TYPE *vfce0 = &g_vfce0;

		switch (dpe_mode) {
		case DPE_NR_MODE:
			vfce0->head_baddr_0 = prm_top->src0_nro_head_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->head_baddr_1 = prm_top->src0_nro_head_caddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_0 = prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_1 = prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 9;
			dbg_h2("%s:addr:nro: nr:wr vfce0 :0x%x:yaddr:0x%llx\n", __func__,
			dpe_src0_nr_wr_idx, vfce0->body_baddr_0);

			break;
		case DPE_VDI_MODE:   //di_only
			vfce0->head_baddr_0 = prm_top->src1_dio_head_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->head_baddr_1 = prm_top->src1_dio_head_caddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_0 = prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_1 = prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 9;
			break;
		case DPE_NR_SRC1_MODE:
			vfce0->head_baddr_0 = prm_top->src1_nro_head_yaddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->head_baddr_1 = prm_top->src1_nro_head_caddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->body_baddr_0 = prm_top->src1_nro_fbuf_yaddr[dpe_src1_nr_wr_idx] << 9;
			vfce0->body_baddr_1 = prm_top->src1_nro_fbuf_caddr[dpe_src1_nr_wr_idx] << 9;
			break;
		case DPE_NRDI_MODE:  //nr+di
			vfce0->head_baddr_0 = prm_top->src1_dio_head_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->head_baddr_1 = prm_top->src1_dio_head_caddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_0 = prm_top->src1_dio_fbuf_yaddr[dpe_src1_di_wr_idx] << 9;
			vfce0->body_baddr_1 = prm_top->src1_dio_fbuf_caddr[dpe_src1_di_wr_idx] << 9;
			break;
		case DPE_NR_BYPS:
			vfce0->head_baddr_0 = prm_top->src0_nro_head_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->head_baddr_1 = prm_top->src0_nro_head_caddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_0 = prm_top->src0_nro_fbuf_yaddr[dpe_src0_nr_wr_idx] << 9;
			vfce0->body_baddr_1 = prm_top->src0_nro_fbuf_caddr[dpe_src0_nr_wr_idx] << 9;
			dbg_h2("%s:addr:nro: DPE_NR_BYPS:wr vfce0 :0x%x:yaddr:0x%llx\n", __func__,
				dpe_src0_nr_wr_idx, vfce0->body_baddr_0);
			break;
		default:
			DBG_WARN("%s:%d not in case\n", __func__, dpe_mode);
			break;
		}

		//cfg_vfce -> cfg_vfce_chnl cfg_vfce_chnl
		hw_cfg_vfce_addr_only(1, 0, vfce0);

	} else {
		hw_cfg_dpe_pix_wrmif_addr_only(DPSS_WMIF_DPE2, pix_wmif2); //wrmif2, todo!!!
	}

	hw_cfg_dpe_pix_wrmif_addr_only(DPSS_WMIF_DPE3, ds_wmif0);	//nr_ds
}

void hw_cfg_dae_small(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae)
{
	bool src_is_byps = dae_mode == DAE_BYPS_MODE;	//dae_src_idx == SRC_IDX_DI;
	bool dae_sw_en = prm_top->sw_ctrl_en & 0x1;
	int i;

	dbg_h2("%s start\n", __func__);

	dbg_h2("\tdae_mode = %d,prm_dpss_dae:%p\n", dae_mode, prm_dae);

//      bool dae_nosrc_en;
	w_reg_bit(VPU_DAE_WRAP_CTRL, src_is_byps, 31, 1);	//reg_dae_byps

	if (src_is_byps) {
		dbg_h2("[dpss_dae.c], dae bypass\n");
		return;
	}

	u32 mc_dae_proc_st = (rd(DPSS_FRC_PROC_STATUS) >> 8) & 0x7;
	u32 nr_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS) >> 16) & 0x7;
	u32 di_dae_proc_st = (rd(DPSS_FBUF_PROC_STATUS + (0x86 - 0x83) * 256) >> 16) & 0x7;
	//u32 src0_i = 0;
	//ary no use    u32 src0_p = 0;
	//u32 src1_i = 1;
	//ary no use    u32 src1_p = 0;
	u32 luma_baddr_x16[16];
	u32 chrm_baddr_x16[16];
	u32 dae_frc_loop = mc_dae_proc_st == 3 ? 0 :	//ary:frc me? is 3
				nr_dae_proc_st == 3 ? 1 :	//
				di_dae_proc_st == 3 ? 2 : 0;

	dbg_h2("%s:reg:st:mc=0x%x, nr=0x%x, di = 0x%x\n", __func__,
	       mc_dae_proc_st, nr_dae_proc_st, di_dae_proc_st);
#ifdef ARY_SIM
	dae_frc_loop = g_dae_frc_loop;
#endif
	u32 nr_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_FRC_NR_MODE ||
				prm_top->dpss_mode == DPSS_FRC_NRBYPS_MODE);
	u32 di_frc_link = (dae_frc_loop == 0x0) &&
				(prm_top->dpss_mode == DPSS_NRDI_FRC_SRC0_MODE ||
				prm_top->dpss_mode == DPSS_DI_FRC_SRC0_MODE);
	for (i = 0; i < 16; i++) {
		//marked for coverity
		//luma_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_yaddr[i] :
		//			di_frc_link ? prm_top->src0_dio_dwbuf_yaddr[i] :
		//			dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_yaddr[i] :
		//			dae_frc_loop == 0x1 ?
		//				(src0_i ? prm_top->src0_fbuf_yaddr[i] :
		//					prm_top->src0_dwbuf_yaddr[i]) :
		//			(src1_i ? prm_top->src1_fbuf_yaddr[i] :
		//			prm_top->src1_dwbuf_yaddr[i]);//src1_p
		//chrm_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_caddr[i] :
		//			di_frc_link ? prm_top->src0_dio_dwbuf_caddr[i] :
		//			dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_caddr[i] :
		//			dae_frc_loop == 0x1 ? (src0_i ?
		//			prm_top->src0_fbuf_caddr[i] :
		//			prm_top->src0_dwbuf_caddr[i]) ://src0_p
		//			(src1_i ? prm_top->src1_fbuf_caddr[i] :
		//			prm_top->src1_dwbuf_caddr[i]);//src1_p

		luma_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_yaddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_yaddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_yaddr[i] :
					dae_frc_loop == 0x1 ? prm_top->src0_dwbuf_yaddr[i] :
					prm_top->src1_fbuf_yaddr[i];//src1_p
		chrm_baddr_x16[i] = nr_frc_link ? prm_top->src0_nro_dwbuf_caddr[i] :
					di_frc_link ? prm_top->src0_dio_dwbuf_caddr[i] :
					dae_frc_loop == 0x0 ? prm_top->src0_dwbuf_caddr[i] :
					dae_frc_loop == 0x1 ? prm_top->src0_dwbuf_caddr[i] :
					prm_top->src1_fbuf_caddr[i];//src1_p
	}
	u32 mc_rd_idx = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	u32 nr_rd_idx = (rd(DPSS_FBUF_DAE_LOOP_IDX) >> 20) & 0xf;
	u32 di_rd_idx = (rd(DPSS_FBUF_DAE_LOOP_IDX + (0x86 - 0x83) * 256) >> 20) & 0xf;
	u32 cur_rd_idx = dae_frc_loop == 0 ? mc_rd_idx :
			dae_frc_loop == 1 ? nr_rd_idx :
			dae_frc_loop == 2 ? di_rd_idx : 0;

	dbg_h2("%s:reg:idx:mc=0x%x, nr=0x%x, di = 0x%x, cur=0x%x\n", __func__,
		mc_rd_idx, nr_rd_idx, di_rd_idx, cur_rd_idx);
#ifdef ARY_SIM
	cur_rd_idx = g_dae_index;
#endif

	//u32 luma_raddr = luma_baddr + luma_step * cur_rd_idx;
	//u32 chrm_raddr = chrm_baddr + chrm_step * cur_rd_idx;
	u32 luma_raddr = luma_baddr_x16[cur_rd_idx] << 5;
	u32 chrm_raddr = chrm_baddr_x16[cur_rd_idx] << 5;

	if (dae_sw_en == 0 && prm_top->fnr_sw_mode == 0) {
		dbg_h2("%s:luma_raddr = 0x%x, chrm_raddr = 0x%x\n",
			__func__, luma_raddr, chrm_raddr);
		dbg_h2("%s: wr VPU_DAE_WRAP_SW_ADDR11 <= 0x%x, next_luma_addr?\n",
			__func__, luma_raddr);
		wr(VPU_DAE_WRAP_SW_ADDR11, luma_raddr);//nxt_luma_addr
		wr(VPU_DAE_WRAP_SW_ADDR12, chrm_raddr);//nxt_chrm_addr
		//        w_reg_bit(VPU_DAE_WRAP_SW_SEL1, 3, 1, 2);//sel
	}

	//Augustine: add for mif mmu mode
	prm_dae->dae_yuv_mif.src_baddr[0] = luma_raddr << 4;
	prm_dae->dae_yuv_mif.src_baddr[1] = chrm_raddr << 4;

	dbg_h2("dae_frc_loop=%d cur_rd_idx=%d\n\n", dae_frc_loop, cur_rd_idx);
	dbg_h2("luma_raddr=0x%x chrm_raddr=0x%x\n", luma_raddr, chrm_raddr);
	//    dbg_h2("luma_step=0x%x chrm_step=0x%x\n", luma_step, chrm_step);

	dae_yuv_intf.src_baddr[0] = prm_dae->dae_yuv_mif.src_baddr[0];
	dae_yuv_intf.src_baddr[1] = prm_dae->dae_yuv_mif.src_baddr[1];

	dpss_cfg_viu_pix_rdmif(&dae_yuv_intf, 1);
}

void hw_cfg_dpss_dpe_small(enum DPSS_WORK_MODE  dpe_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DPE  *prm_dpe)
{
	dbg_h2("%s:start\n", __func__);

	//cfg dpe size for software mode
	//hw_cfg_dpe_size(dpe_mode, prm_top, prm_dpe, dpe_pad);
	hw_cfg_dpss_dpe_intf_addr_only(prm_top, prm_dpe);
}

void hw_irq_src0_nr_only_dae_irq(void)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;

	//follow process_dae1_frm_rst

	hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
				   dpss_dae_frm_cnt_src0,
				   dpss_dae_frm_cnt_src1,
				   dpss_dae_frm_cnt_src2,
				   dpss_dpe_nr_frm_cnt,
				   dpss_dpe_di_frm_cnt,
				   &dpss_di_ignore_num,
				   &dpss_nr_ignore_num,
				   &dpss_inp_frm_cnt_adj,
				   &dpss_dae_frm_cnt_src0_adj,
				   &dpss_dae_frm_cnt_src1_adj,
				   &dpss_dae_frm_cnt_src2_adj,
				   &dpss_dpe_nr_frm_cnt_adj,
				   &dpss_dpe_di_frm_cnt_adj);

	hw_cfg_dae_small(DAE_NR_MODE, prm_top, prm_dae);
	cfg_dae_triggle();
	dpss_dae_frm_cnt_src1++;
}

unsigned int last_dpe_mode;
void hw_irq_src0_nr_only_dpe_irq(void)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;
	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;
	unsigned int dpe_nr_mode = DPE_NR_MODE;
	//follow process_dae1_frm_rst
	int amdv_prl_mode = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;	//src0:dv_on,nrdi_off

	dbg_h2("%s:\n", __func__);
	hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
				   dpss_dae_frm_cnt_src0,
				   dpss_dae_frm_cnt_src1,
				   dpss_dae_frm_cnt_src2,
				   dpss_dpe_nr_frm_cnt,
				   dpss_dpe_di_frm_cnt,
				   &dpss_di_ignore_num,
				   &dpss_nr_ignore_num,
				   &dpss_inp_frm_cnt_adj,
				   &dpss_dae_frm_cnt_src0_adj,
				   &dpss_dae_frm_cnt_src1_adj,
				   &dpss_dae_frm_cnt_src2_adj,
				   &dpss_dpe_nr_frm_cnt_adj,
				   &dpss_dpe_di_frm_cnt_adj);

	//ref to process_dpe1_frm_rst
	if (dpss_dpe_nr_frm_cnt_adj > 0) {	//For bitmatch start
		hw_check_nr_ro(dpss_dpe_nr_frm_cnt - 1, SRC_IDX_NR);
	}

	if (prm_top->bbd_parallel == 1)
		dbg_h2("in bbd parallel mode,do nothing\n");
		//in bbd parallel mode, cannot change din_dmsq_init config, otherwise src1 affected
	else if (dpss_dpe_nr_frm_cnt_adj == 0)
		wr(DPSS_DPE_DIN_DMSQ_INIT, 1);	//set dms init for first frame
	else
		wr(DPSS_DPE_DIN_DMSQ_INIT, 0);

	if (dpss_dpe_nr_frm_cnt_adj > 0) {	//dpss_dpe_nr_frm_cnt_adj //di_frm_cnt_adj
		check_di_ro(dpss_dpe_di_frm_cnt - 1, SRC_IDX_DI0);
		hw_check_nr_ro(dpss_dpe_di_frm_cnt - 1, SRC_IDX_DI0);
	}

	if (amdv_prl_mode == 0) {
		if (dpss_dpe_nr_frm_cnt < dpss_nr_ignore_num && dpe_nr_mode != DPE_BBD_ONLY)
			dpe_nr_mode = DPE_NR_BYPS;
		else
			dpe_nr_mode = DPE_NR_MODE;

		if (dpe_nr_mode != last_dpe_mode) {
			hw_cfg_dpss_dpe(DPE_NR_MODE, prm_top, prm_dpe, &g_nr_pps_cfg);
			last_dpe_mode = dpe_nr_mode;
		} else {
			hw_cfg_dpss_dpe_small(dpe_nr_mode, prm_top, prm_dpe);
		}

		hw_dpe_out_addr_rd(SRC_IDX_NR);
		cfg_dpe_triggle(prm_top);
		dpss_dpe_nr_frm_cnt++;
	}
}

//only for bypass switch
void hw_cfg_dpss_dpe_no_buf_update(enum DPSS_WORK_MODE dpe_mode,
				   struct PRM_DPSS_TOP *prm_top,
				   struct PRM_DPSS_DPE *prm_dpe)
{
	dbg_h2("%s:start\n", __func__);

	u32 reg_din_sel_sw_en0 = 0;	//0xffe00000; //bit31:21
	u32 reg_din_sel_sw_en1 = 0;	//0x007fffff; //bit54:32
	u32 reg_din_sel_sw_en2 = 0;	//0x00000018; //bit69:67
	u32 wrmif_en = 0xf;	//{?,dblk_h,dblk_v,dmsq_wr,nrwr,diwr}
	u32 rdmif_en = 0xf;
	u32 di_en = 0;
	u32 nr_en = 0;
	u32 ds_mix_en = 0;
	u32 frm_end_mask = 0;
//	u32 mif_reg_mode = 0;//0:hw_auto 1:cpu_reg
	bool nr_debug_mode = ((rd(DOS_DOWN_S_MODE) >> 8) & 0xff) != 0;
	bool nr_cur_only_mode = ((rd(VPU_NR_TOP_MISC) >> 20) & 0x1);
	//    u32 org_hsize;
	//    u32 org_vsize;
//	bool me_mc_link_en = !prm_top->me_mc_link_en;
//	bool dpe_game_mode = !prm_top->dpe_game_mode;
	bool dpe_sw_ctrl = prm_top->sw_ctrl_en;

	u32 dcntr_en = prm_dpe->dcntr_en;

	u32 bbd_en = rd(DPSS_DPE_BBD_CTRL) == 0x2;
	//reg_aa_pps_mode == 3 -> nr aapps 4k1k
	bool nr_aapps_en;// = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;
	bool dpss_nr_vpp_link = (rd(DPSS_NR_VPP_LINK) & 0x1);
	bool nr_src1_di_loop = 0;	//di loop when src1 only nr

	if (prm_top->is_pps)
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 3, 4, 2);
	else
		w_reg_bit(DPSS_DPE_INTF_CTRL2, 0, 4, 2);

	nr_aapps_en = ((rd(DPSS_DPE_INTF_CTRL2) >> 4) & 0x3) == 3;

	dbg_h2("dcntr_en=%d\n", dcntr_en);
	dbg_h2("%s pps nr_aapps_en=%x\n", __func__, nr_aapps_en);

	dbg_h2("\treg:en:bbd=%d, nr_aapps=%d, nr_link=%d\n",
	       bbd_en, nr_aapps_en, dpss_nr_vpp_link);
	if (!dpe_sw_ctrl && !dpss_nr_vpp_link) {
		wr(DPSS_DPE_TOP_SW_SEL0, reg_din_sel_sw_en0);
		wr(DPSS_DPE_TOP_SW_SEL1, reg_din_sel_sw_en1);
		wr(DPSS_DPE_TOP_SW_SEL2, reg_din_sel_sw_en2);
	}

//	u32 me_dsx       = prm_top->dae_dsx_scale;
//	u32 me_dsy       = prm_top->dae_dsy_scale;

	u32 dcntr_pad = dcntr_en ? 8 : 0;

	u32 bbd_pad = dpe_mode == DPE_BBD_ONLY ? 2 :
	    dpe_mode == DPE_NR_BYPS && dcntr_en ? 0 :
	    dpe_mode == DPE_NR_BYPS && bbd_en ? 2 : 0;

	//   u32 vbe_pad = (dpe_mode == DPE_NR_BYPS || dpe_mode == DPE_BBD_ONLY ?  0 : 64);

	u32 aa_pad = prm_top->nr_aapps_up ? 16 :
	    dpe_mode == DPE_NR_BYPS && bbd_en ? bbd_pad : 0;

	u32 vbe_pad = (dpe_mode == DPE_NR_BYPS || dpe_mode == DPE_BBD_ONLY ? 0 : 64) + aa_pad;
	u32 nrdi_pad = aa_pad >= 2 ? aa_pad : bbd_pad;
	u32 amdv_pad = 0;

	struct PRM_DPE_PAD dpe_pad;

	dpe_pad.reg_vbe_pad = vbe_pad;
	dpe_pad.reg_nrdi_pad = nrdi_pad;
	dpe_pad.reg_mc_pad = 32;
	//dpe_pad.reg_dv_pad   = 96;
	dpe_pad.reg_aa_pad = aa_pad;
	dpe_pad.reg_dmsq_pad = 32;
	dpe_pad.reg_dcntr_pad = dcntr_pad;
	dpe_pad.reg_bbd_pad = bbd_pad;

	u32 reg_nr_byps_en = 0;
	u32 reg_nr_byps_sel = 0;	//1:din0 2:din1
	u32 reg_nr_byps_mode = 0;	//dat_sel 0:din0

	dbg_h2("dpe_mode=%d ary: 31 is nr; 32 is nr_byps dolby_mode=%d\n",
	       dpe_mode, prm_top->dolby_mode);
	//    dbg_h2("[dpss_dpe.c]:org_hsize=%4d  org_vsize=%4d\n", org_hsize, org_vsize);
	//    dbg_h2("[dpss_dpe.c]:frm_hsize=%4d  frm_vsize=%4d\n", frm_hsize, frm_vsize);

	//path0=nr path1=di path2=dv

	u32 wrpath_sel = (rd(DPSS_DPE_INTF_AFBCD0) >> 12) & 0x1ffff;

	u32 vfce_on = wrpath_sel & 0x1;	//cmp or uncmp
	u32 vfce_sel = (wrpath_sel >> 1) & 0x7;
	u32 w2_sel = (wrpath_sel >> 4) & 0x7;
	u32 w0_sel = (wrpath_sel >> 7) & 0x7;
	u32 w1_sel = (wrpath_sel >> 10) & 0x7;
	u32 ds_sel = (wrpath_sel >> 13) & 0x3;

	dbg_h2("\twrpath_sel = 0x%x\n", wrpath_sel);
	if (prm_top->nro_fs_fmt.src_cmpr == CMPR_UN)
		vfce_on = 0x0;	//uncmp
	else
		vfce_on = 1;	//rd(DPSS_REG_DPE_LOSS_OUT_EN); //cmp

	u32 submif_msk;

	switch (dpe_mode) {	//todo use dblk_en/dmsq_en to mif_en/mask_en
	case DPE_MC_MODE:
		di_en = 0b0;
		nr_en = 0b00;
		wrmif_en = 0b0000;	//{ds_mix, dblk_h, dblk_v, dmsq_wr}
		rdmif_en = dcntr_en ? 0b10111111 : 0b00111111;
		ds_mix_en = 0b0;
		//frm_end_mask = 0b111100; //{ds_mix, dblk_h, dblk_v, dmsq_wr, path1, path0}
		submif_msk = 0b111;
		break;
	case DPE_BBD_ONLY:
		di_en = 0b0;
		nr_en = 0b00;
		wrmif_en = 0b0000;
		rdmif_en = 0b00000000;
		ds_mix_en = 0b0;
		submif_msk = 0b111;
		reg_nr_byps_en = 1;
		reg_nr_byps_sel = 0;	//din0_hs
		reg_nr_byps_mode = 0;	//din0_dat
		vfce_sel = 7;
		w2_sel = vfce_sel;
		w1_sel = 7;
		w0_sel = 7;
		ds_sel = 3;
		break;
	case DPE_NR_MODE:
		di_en = 0b0;
		nr_en = 0b01;
		wrmif_en = nr_cur_only_mode ? 0b0110 : 0b0111;//{ds_mix, dblk_h, dblk_v, dmsq_wr}
		rdmif_en = dcntr_en ? 0b11010101 : 0b01010101;
		ds_mix_en = 0b1;
		//frm_end_mask = 0b100010;//{ds_mix, dblk_h, dblk_v, dmsq_wr, path1, path0}
		submif_msk = 0b000;
		vfce_sel = 0;
		w2_sel = vfce_sel;
		w1_sel = nr_aapps_en ? 3 : nr_debug_mode ? 1 : 7;	//4k1k wr1
		w0_sel = 7;
		ds_sel = prm_top->dpe_dw_off ? 3 : 0;
		break;
	case DPE_VDI_MODE:	//di_only
		di_en = 1;
		nr_en = 0b00;
		wrmif_en = 0b0000;
		rdmif_en = dcntr_en ? 0b10111111 : 0b00111111;	//todo
		ds_mix_en = 0b0;
		//frm_end_mask = 0b111101;
		submif_msk = 0b111;
		vfce_sel = 1;
		w2_sel = vfce_sel;
		w1_sel = 7;
		w0_sel = 0;
		ds_sel = 1;
		break;
	case DPE_NR_SRC1_MODE:
		di_en = 0;
		nr_en = 0b10;
		wrmif_en = 0b0111;
		rdmif_en = dcntr_en ? 0b11010101 : 0b01010101;
		ds_mix_en = 0b0;
		nr_src1_di_loop = 1;
		//frm_end_mask = 0b100010;
		submif_msk = 0b000;
		vfce_sel = 0;
		w2_sel = vfce_sel;
		w1_sel = 7;
		w0_sel = 7;
		ds_sel = 0;
		break;
	case DPE_NRDI_MODE:	//nr+di
		di_en = 1;
		nr_en = 0b10;
		wrmif_en = 0b0111;
		rdmif_en = dcntr_en ? 0b11111111 : 0b01111111;
		ds_mix_en = 0b0;
		//frm_end_mask = 0b111100;
		submif_msk = 0b000;
		vfce_sel = 1;
		w2_sel = vfce_sel;
		w1_sel = 7;
		w0_sel = 0;
		ds_sel = 1;
		break;
	case DPE_NR_BYPS:
		di_en = 0b0;
		nr_en = 0b00;
		wrmif_en = 0b0010;	//{ds_mix, dblk_h, dblk_v, dmsq_wr}
		rdmif_en = dcntr_en ? 0b11010101 : 0b01010101;	//todo
		reg_nr_byps_en = 1;
		reg_nr_byps_sel = 1;	//din0_hs
		reg_nr_byps_mode = 0;	//din0_dat
		ds_mix_en = 0b0;
		//frm_end_mask = 0b111110;
		submif_msk = 0b111;
		vfce_sel = 0;
		w2_sel = vfce_sel;
		w1_sel = nr_aapps_en ? 3 : 7;	//4k1k wr1
		w0_sel = 7;
		ds_sel = 0;
		break;
		//    case :add NRDI_MODE
		//case DPSS_FRC_DV_PRL_MODE://no-use
		//    di_en        = 1;
		//    nr_en        = 0b10;
		//    wrmif_en     = 0b0111;
		//    rdmif_en     = dcntr_en ? 0b11111111 : 0b01111111;//todo
		//    ds_mix_en    = 0b0;
		//    //frm_end_mask = 0b111101;
		//    submif_msk   = 0b111;
		//    vfce_sel     = 2;
		//    w2_sel       = vfce_sel;
		//    w1_sel       = 1;
		//    w0_sel       = 0;
		//    ds_sel       = 2;
		//    break;
	default:		//ERROR
		di_en = 0b1;
		nr_en = 0b11;
		wrmif_en = 0b0111;
		rdmif_en = dcntr_en ? 0b11111111 : 0b01111111;
		ds_mix_en = 0b0;
		//frm_end_mask = 0b111110;
		submif_msk   = 0b111;
		DBG_ERR("%s: Error: dpe_mode 0x%x\n", __func__, dpe_mode);
#ifdef RUN_ON_PC
		exit(1);
#else
		return;
#endif
	}

	prm_dpe->dpe_mode = dpe_mode;
	prm_dpe->nr_en = nr_en;
	prm_dpe->di_en = di_en;

	u32 dolby_parallel;

	dolby_parallel = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;
	vfce_sel = dolby_parallel ? 2 : vfce_sel;
	w1_sel = dolby_parallel ? 1 : w1_sel;
	w0_sel = dolby_parallel ? 0 : w0_sel;
	ds_sel = dolby_parallel ? 2 : ds_sel;
	w2_sel = vfce_sel;
	u32 vfce_mask = dolby_parallel ? 1 : vfce_sel == 7;
	u32 w0_mask = w0_sel == 7;
	u32 w1_mask = w1_sel == 7;
	u32 ds_mask = dolby_parallel ? 1 : ds_sel == 3;

	frm_end_mask = (submif_msk << 4) |
			(ds_mask << 3) |
			(w1_mask << 2) |
			(w0_mask << 1) |
			(vfce_mask);

	wrpath_sel = (ds_sel  << 13) |
			(w1_sel  << 10) |
			(w0_sel  << 7) |
			(w2_sel  << 4) |
			(vfce_sel << 1) |
			(vfce_on);

	w_reg_bit(DPSS_DPE_INTF_AFBCD0, wrpath_sel, 12, 17);

	//reg_mode
//	bool top_reg_mode  = prm_dpe->dpe_reg_mode.top_reg_mode;
//	bool vfcd_reg_mode = prm_dpe->dpe_reg_mode.vfcd_reg_mode;
//	bool vfce_reg_mode = prm_dpe->dpe_reg_mode.vfce_reg_mode;
//	u32 subrd_reg_mode = prm_dpe->dpe_reg_mode.subrd_reg_mode;
//	u32 subwr_reg_mode = prm_dpe->dpe_reg_mode.subwr_reg_mode;

//ary no use	u32 cur_mif_pad = dcntr_pad + vbe_pad;

	//nr_byp, vbe_pad=0, nrdi_pad=0;
	//nr_byp+bbd, vbe_pad=2, nrdi_pad=2;
	//nr_byp+dcntr, vbe_pad=dcntr_pad, nrdi_pad=0
	//nr_byp+dcntr+bbd, vbe_pad=dcntr_pad, nrdi_pad=0

	if (reg_nr_byps_en & ~dcntr_en) {
		dpss_vbe_proc_byp(0, 0);	//bypass nr
	} else if (reg_nr_byps_en & dcntr_en) {
		wr(VPU_NR_TOP_SYNC_CTRL, 0xffffffff);
		//wr(VPU_NR_ENABLE, 0x0);
		w_reg_bit(VPU_NR_ENABLE, 0x0, 1, 31);
		w_reg_bit(DPSS_DPE_TOP_CTRL0, 0, 4, 4);
		w_reg_bit(DPSS_DPE_HW_DBG, 1, 2, 1);	//nr_done_mask
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 28, 1);	//reg_nr_byps_en
		w_reg_bit(DPSS_BBD_ONLY_CTRL, 1, 1, 1);	//nr_cur_only
		w_reg_bit(VPU_NR_OUT_CROP, 0, 0, 16);	//reg_nr_out_hcrop_left/right

		//        w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_vbe_pad, 24, 8); //vbe_pad
	} else {
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_mode & 0x7, 29, 3);	//reg_nr_byps_mode
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_en & 0x1, 28, 1);	//reg_nr_byps_en
		w_reg_bit(VPU_VBE_TOP_CTR0, reg_nr_byps_sel & 0x7, 8, 3);	//reg_nr_byps_sel
		w_reg_bit(DPSS_BBD_ONLY_CTRL, nr_cur_only_mode, 1, 1);	//nr_cur_only
		w_reg_bit(VPU_NR_OUT_CROP, 64 << 8 | 64 << 0, 0, 16);
		w_reg_bit(DPSS_DPE_HW_DBG, 0, 2, 2);

		//        w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_vbe_pad, 24, 8); //vbe_pad
	}
	w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_vbe_pad, 24, 8);	//vbe_pad
	w_reg_bit(DPSS_DPE_PAD, dpe_pad.reg_dcntr_pad, 0, 8);	//dcntr_pad

	//cfg dpe size for software mode
	//ary hw_cfg_dpe_size(dpe_mode, prm_top,  prm_dpe,  dpe_pad);
	//ary hw_cfg_dpss_dpe_intf(prm_top,  prm_dpe);

	if (dcntr_en)
		dbg_h2("dcntr_en %d,do nothing\n", dcntr_en);
		//12-17 dcntr_pst_slc(&prm_dpe->dcntr_slc, cur_mif_pad);
	else
		w_reg_bit(VPU_VBE_TOP_CTR0, 0, 26, 1);	//close dcntr

	dpe_pad.reg_dv_pad = ((prm_top->dolby_mode == DOLBY_DPSS_MODE) ||
		(prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE)) ? 96 : 0;	//todo

	//aa_pad   = dpe_pad.reg_aa_pad;
	amdv_pad = dpe_pad.reg_dv_pad;

	w_reg_bit(DPSS_DPE_TOP_CTRL0, prm_top->dpe_slc_num, 16, 3);	//reg_slc_num
	w_reg_bit(DPSS_DPE_TOP_CTRL0, rdmif_en, 8, 8);	//reg_rdmif_enable
	w_reg_bit(DPSS_DPE_TOP_CTRL0, wrmif_en, 4, 4);	//reg_wrmif_enable
	w_reg_bit(DPSS_DPE_TOP_CTRL0, di_en, 0, 1);	//reg_vbe_nrdi_sel
	w_reg_bit(DPSS_DPE_TOP_CTRL0, nr_src1_di_loop, 23, 1);	//reg_src1_nr_di_loop
	w_reg_bit(DPSS_DPE_DS_MIX, ds_mix_en, 26, 1);	//reg_ds_mix_en
	w_reg_bit(DPSS_DPE_INTF_DBG, frm_end_mask & 0x7ff, 0, 11);	//reg_frm_end_mask
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, aa_pad, 8, 8);	//reg_aa_pad reg_nrdi_pad
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, amdv_pad, 24, 8);	//reg_dv_pad
	w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, nrdi_pad, 16, 8);	//reg_aa_pad reg_nrdi_pad
	w_reg_bit(DPSS_DPE_HW_DBG, ~dcntr_en, 12, 1);	// dcntr_slc_end mask
}

unsigned int dpss_int = IRQ_MODE_CASE_0_SRC0_NR_DI;	//bit 0 for dae
module_param_named(dpss_int, dpss_int, uint, 0664);

unsigned int dpss_dbg_dae0_byp_cnt = 20;
module_param_named(dpss_dbg_dae0_byp_cnt, dpss_dbg_dae0_byp_cnt, uint, 0664);

int dpss_dbg_adj_idx = 1;
module_param_named(dpss_dbg_adj_idx, dpss_dbg_adj_idx, uint, 0664);

unsigned int dpss_dbg_irq_ko = 0x1111;
module_param_named(dpss_dbg_irq_ko, dpss_dbg_irq_ko, uint, 0664);

static void mc_put_buffer(struct frc_state_s *state_st)
{
	u8 drop_idx;
	u8 dst_buf_cnt;
	struct display_buffer_info_s *display_buf_info;

	if (!state_st)
		return;

	cfg_dpss_done_trigger(FRC_DST_DONE);
	drop_idx = display_buf_q.drop_idx;
	display_buf_info = &display_buf_q.data[drop_idx];
	state_st->mc_drop_idx = display_buf_info->p;
	display_buf_q.drop_idx = (display_buf_q.drop_idx + 1) % DPSS_QUEEN_NUM;
	dst_buf_cnt = get_dst_buf_cnt(&display_buf_q);
	pr_frc(1, "drop_idx=%d, mc_idx=%d, inp_idx=%d, diff=%d, mc_drop=%d, mc_display=%d\n",
		display_buf_q.drop_idx, display_buf_q.mc_idx, display_buf_q.inp_idx,
		dst_buf_cnt, state_st->mc_drop_idx, state_st->mc_disp_st.disp_pre_idx);
}

static bool mc_need_drop_frame(struct dpss_frc_top_type_s *frc_top, struct frc_state_s *state_st)
{
	u32 frc_dst_buf;
	u32 nr_buf_state;
	u8 nr_dae_ibuf_level;
	u8 nr_dpe_free_buf_level;
	bool ret = false;

	if (!frc_top || !state_st)
		return false;

	frc_dst_buf = rd(DPSS_FRC_DST_BUFF_STATUS) & 0xf;
	nr_buf_state = rd(DPSS_FBUF_BUF_CNT);
	nr_dpe_free_buf_level = nr_buf_state >> 8 & 0x1f;
	nr_dae_ibuf_level = nr_buf_state >> 16 & 0x1f;

	if (frc_dst_buf > 0 && frc_top->need_dpe_mix)
		ret = true;
	else if (!state_st->force_disable_dpe_mix && nr_dpe_free_buf_level < 2 &&
		(frc_dst_buf > state_st->dst_buf_th ||
		(frc_dst_buf > 0 && nr_dae_ibuf_level > 2)))
		ret = true;
	frc_top->need_dpe_mix = 0;
	dbg_f2("nr_dae_ibuf:%d nr_dpe_free_buf:%d frc_dst_buf=%d dpe_mix=%d\n",
		nr_dae_ibuf_level, nr_dpe_free_buf_level, frc_dst_buf, ret);
	return ret;
}

void pre_vs_game(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_game_state_s *game_st;
	u32 ro_disp_info_0;
	bool src0_disp_obuf_rdy = 0;
	unsigned int dpss_frc_vpp_link;
	// unsigned int mc_ibuf_stats = rd(DPSS_FRC_MC_IUFF_STATUS);
	unsigned int pre_idx, cur_idx;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_interrupt_s *frc_int_st;
	u32 dae_frm_phs;
	u32 dpe_intp_phs;
	bool mc_ibuf_vld;
	bool is_vd1_link;
	bool manual_disable_link = false;
	bool phs_swth = false;
	u64 timestamp = sched_clock();
	u32 frc_dst_buf;
	static u32 cur_idx_last;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st || !pchip_st->state_st.frc_en)
		return;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("irq_pre_vsync  dpss_frc fw_data is null\n");
		return;
	}
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->pre_vsync_duration = timestamp - frc_int_st->pre_vsync_timestamp;
	frc_int_st->pre_vsync_timestamp = timestamp;
	frc_int_st->pre_vsync_cnt++;
	// w_reg_bit(VPU_DAE_WRAP_DI, 1, 22, 1);

	// dpss_frc_check_reg_stats();
	dbg_h2("irq_pre_vsync start %d\n", frc_int_st->pre_vsync_cnt);
	dpss_frc_vpp_link = rd(FRC_DPSS_VPP_LINK);
	src0_disp_obuf_rdy = state_st->src0_disp_obuf_rdy;

	if (dpss_frc_vpp_link)
		frc_undone_check();

	if (src0_disp_obuf_rdy && dpss_frc_vpp_link) {
		w_reg_bit(FRC_DAE_SW_CTRL0, 1, 25, 1);
		cfg_dpss_done_trigger(FRC_DST_DONE);	//last frame
	}
	frc_dst_buf = rd(DPSS_FRC_DST_BUFF_STATUS) & 0xf;
	dbg_f2("frc_dst_buf pre = %d\n", frc_dst_buf);

	check_dpss_frc_status();
	dpss_pre_vs_reg_monitor();
	src0_disp_obuf_rdy = disp_obuf_trigger(FRC_DST_DONE);
	dbg_h2("src0_disp_obuf_rdy_cur: %d\n", src0_disp_obuf_rdy);

	dae_frm_phs = (rd(FRC_DPSS_DISP_BUFF_INFO_1) >> 12) & 0x1;

	state_st->src0_disp_obuf_rdy = src0_disp_obuf_rdy;
	frc_ocnt_status = src0_disp_obuf_rdy;
	ro_disp_info_0 = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	pre_idx = (ro_disp_info_0 >> 2) & 0xf;
	cur_idx = (ro_disp_info_0 >> 8) & 0xf;
	if (src0_disp_obuf_rdy) {
		if (cur_idx_last == pre_idx)
			phs_swth = true;
		cur_idx_last = cur_idx;
		dbg_f2("ro_disp_info_0:%#x pre_idx/cur_idx:%x %x, phs_swth=%d, cur_last=%x\n",
			ro_disp_info_0, pre_idx, cur_idx, phs_swth, cur_idx_last);
	}

	is_vd1_link = is_vd1_link_state();
	if (!enable_mc_link && !is_vd1_link)
		manual_disable_link = true;

	hw_dpss_dpe_game_info_cfg(prm_top, src0_disp_obuf_rdy);
	if (src0_disp_obuf_rdy && frc_top->fw_pause == 0 &&
		pfw_data->pre_vsync_irq_handler && (dpss_dbg_irq_ko & C_BIT0))
		pfw_data->pre_vsync_irq_handler(pfw_data);

	if (state_st->mc_bypass || manual_disable_link) {
		wr(DPSS_DPE_MC_PHASE, 0);
		dbg_f2("set mc phase 0 (mcbypass:%d, disable_link:%d)\n",
			state_st->mc_bypass, manual_disable_link);
		state_st->mc_set_phase0 = true;
	}

	mc_ibuf_vld = (rd(DPSS_FRC_MC_IUFF_STATUS) >> 4) & 0x1;
	dpe_intp_phs = mc_ibuf_vld ? (rd(FRC_DPSS_DISP_BUFF_INFO_0) >> 22) & 0xff : 0;

	if (state_st->cut_pre_mc_work == 1) {
		wr(DPSS_DPE_MC_PHASE, 0);
		dbg_f2("only for cut set phase0)\n");
		state_st->cut_pre_mc_work = 0;
	}

	if (state_st->mc_bypass && phs_swth && state_st->have_update_vfcd &&
		!state_st->mc_bypass_always && frc_top->memc_enable) {
		if (state_st->mc_bypass_cnt) {
			dbg_f2("mc_bypass_cnt = %d\n", state_st->mc_bypass_cnt);
			state_st->mc_bypass_cnt--;
		} else {
			frc_set_mc_bypass(0);
			state_st->mc_bypass = false;
			dbg_f2("mc bypass set 0\n");
		}
	}

	dbg_f2("mc_intp_phs=%x, mc_phase=%x, mc_ibuf_vld=%d, mc_bypass:%d\n",
		dpe_intp_phs, rd(DPSS_DPE_MC_PHASE), mc_ibuf_vld, state_st->mc_bypass);

	//don't add  logic after triggle function,add by fxj
	// hw_cfg_dpss_mc_pre_triggle();
	w_reg_bit(DPSS_DPE_MC_START_CTRL, 1, 30, 1); //mc pre trig

	if (game_st->mc_need_byp_en) {
		frc_set_mc_bypass(3);
		dbg_f2("game mode mc bypass set 3\n");
	}

#ifdef USE_FRC_PRE_VS_RDMA
	// dpss_rdma_auto_wr_reg(reg, val + idx);
	DPSS_RDMA_WR_PRE_VS(FRC_REG_TOP_RESERVE0, frc_int_st->pre_vsync_cnt);
	dpss_rdma_auto_wr_tri(1);

#endif
	if (pchip_st->dbg_st.ctrl_dbg.reg_tab_pr_en & 0x1)
		frc_dbg_read_table_value();
}

void irq_pre_vs(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	bool src0_disp_obuf_rdy = 0;
	bool display_ready;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;
	struct frc_interrupt_s *frc_int_st;
	u64 timestamp = sched_clock();
	u32 frc_dst_buf;
	u32 nr_proc_st;
	u32 frc_proc_st;
	u8 mc_drop_idx;
	u8 drop_idx;
	u8 mc_bypass_st;
	u8 mc_phase_st;
	u8 fallback_st;
	u8 vd1_link_st;
	struct display_buffer_info_s *display_buf_info;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st || !pchip_st->state_st.frc_en)
		return;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("irq_pre_vsync  dpss_frc fw_data is null\n");
		return;
	}
	if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		pre_vs_game();
		return;
	}

	state_st = &pchip_st->state_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->pre_vsync_duration = timestamp - frc_int_st->pre_vsync_timestamp;
	frc_int_st->pre_vsync_timestamp = timestamp;
	frc_int_st->pre_vsync_cnt++;
	if (state_st->frc_init) {
		state_st->frc_init = false;
		frc_int_st->irq_chk = 0;
		frc_int_st->irq_chk_flag = 0;
	}
	frc_int_st->irq_chk++;
	if (frc_int_st->irq_chk_flag) {
		if (frc_int_st->irq_chk) {
			frc_int_st->irq_chk_err_cnt++;
			frc_int_st->irq_chk = 0;
		}
	} else {
		if (frc_int_st->irq_chk != 1) {
			frc_int_st->irq_chk_err_cnt++;
			frc_int_st->irq_chk = 1;
		}
	}

	mc_bypass_st = rd(FRC_MC_HW_CTRL0) & 0x3;
	src0_disp_obuf_rdy = state_st->src0_disp_obuf_rdy;
	nr_proc_st = rd(DPSS_FBUF_PROC_STATUS);
	frc_proc_st = rd(DPSS_FRC_PROC_STATUS);

	frc_undone_check();

	if (src0_disp_obuf_rdy && display_queue_put(&display_buf_q))
		mc_put_buffer(state_st);
	frc_dst_buf = rd(DPSS_FRC_DST_BUFF_STATUS) & 0xf;
	while (src0_disp_obuf_rdy && frc_dst_buf > 0 && display_queue_put(&display_buf_q)) {
		drop_idx = (display_buf_q.drop_idx + 1) % DPSS_QUEEN_NUM;
		display_buf_info = &display_buf_q.data[drop_idx];
		mc_drop_idx = display_buf_info->p;
		if (state_st->mc_disp_st.disp_pre_idx == state_st->mc_drop_idx &&
			mc_drop_idx != state_st->mc_drop_idx) {
			pr_frc(2, "now_mc_drop_idx=%d\n", mc_drop_idx);
			break;
		}
		mc_put_buffer(state_st);
		frc_top->dpe_mix_cnt++;
		if (display_buf_info->dae_mix)
			frc_top->dpe_mix_cnt--;
		pr_frc(2, "dpe_mix_cnt=%d\n", frc_top->dpe_mix_cnt);
	}

	if (!state_st->dpe_mix)
		state_st->dpe_mix = mc_need_drop_frame(frc_top, state_st);

	check_dpss_frc_status();
	dpss_pre_vs_reg_monitor();

	src0_disp_obuf_rdy = disp_obuf_trigger(FRC_DST_DONE);
	state_st->src0_disp_obuf_rdy = src0_disp_obuf_rdy;
	frc_ocnt_status = src0_disp_obuf_rdy;
	if ((state_st->need_drop_dd || state_st->is_seek_bar) && state_st->dpe_ready)
		state_st->enable_last_drop = true;
	else
		state_st->enable_last_drop = false;
	if (state_st->force_disable_drop_last || prm_top->trig_byp_mc)
		state_st->enable_last_drop = false;

	display_ready = hw_dpss_dpe_info_cfg(prm_top, src0_disp_obuf_rdy);
	fallback_st = state_st->force_disable_check_fallback ? 0 : state_st->check_fallback;

	if (state_st->enable_mc_cnt != 0) {
		state_st->enable_mc_cnt--;
		if (state_st->enable_mc_cnt == dpss_dbg_dae0_byp_cnt)
			state_st->trig_pos_chg = false;
		if (state_st->enable_mc_cnt == 0)
			state_st->mc_byp_switch_off = true;
	}
	if (state_st->mc_byp_switch_off && !state_st->need_force_mc_pre) {
		frc_set_mc_bypass_rdma(0);
		state_st->bypass_chg = true;
		state_st->mc_byp_switch_off = false;
		dbg_h2("enable mc\n");
	}

	if (display_ready && frc_top->fw_pause == 0 &&
		pfw_data->pre_vsync_irq_handler && (dpss_dbg_irq_ko & C_BIT0))
		pfw_data->pre_vsync_irq_handler(pfw_data);

	if (state_st->mc_bypass && state_st->have_update_vfcd &&
		!state_st->mc_bypass_always && frc_top->memc_enable &&
		state_st->enable_mc_cnt == 0) {
		if (state_st->mc_bypass_cnt) {
			state_st->mc_bypass_cnt--;
		} else if (fallback_st == 0 || fallback_st == 3) {
			frc_set_mc_bypass_rdma(0);
			state_st->mc_bypass = false;
			state_st->bypass_chg = true;
		}
	}

	if (state_st->need_chg_trig_pos)
		state_st->trig_pos_chg = true;

	if (state_st->mc_byp_switch_on) {
		frc_set_mc_bypass_rdma(3);
		state_st->bypass_chg = true;
		state_st->use_phase0_done = false;
		state_st->need_chg_trig_pos = true;
		dbg_h2("trig byp mc cur\n");
	}

	if (state_st->src_chg) {
		if (mc_bypass_st != 3)
			DBG_WARN("mc not bypass\n");
		state_st->mc_byp_switch_on = false;
		state_st->enable_mc_cnt = dpss_dbg_dae0_byp_cnt + 10;
		state_st->need_chg_trig_pos = false;
		if (pps_out_w) {
			frc_top->hsize = prm_top->is_pps ? pps_out_w : prm_top->frm_hsize;
			frc_top->vsize = prm_top->is_pps ? pps_out_h : prm_top->frm_vsize;
		} else {
			frc_top->hsize = prm_top->is_pps ?
				prm_dpe->dpe_nr_size.pps_hsize : prm_top->frm_hsize;
			frc_top->vsize = prm_top->is_pps ?
				prm_dpe->dpe_nr_size.pps_vsize : prm_top->frm_vsize;
		}
		if (pfw_data->frc_input_cfg)
			pfw_data->frc_input_cfg(pfw_data);

		dbg_h2("src_chg\n");
	}

	if (!state_st->dpss_reg || state_st->bypass_chg || state_st->src_chg ||
			!state_st->use_phase0_done || state_st->need_set_phase0 ||
			state_st->mc_bypass || !state_st->is_frc_vpp_link ||
			pchip_st->win_st.cut_win_chg) {
		DPSS_RDMA_WR_PRE_VS(DPSS_DPE_MC_PHASE, 0);
		state_st->mc_set_phase0 = true;
		state_st->bypass_chg = false;
	} else {
		state_st->mc_set_phase0 = false;
	}

	if (state_st->mc_set_phase0 || state_st->dpe_intp_phs == 0)
		state_st->mc_phase0_rdma = true;
	else
		state_st->mc_phase0_rdma = false;

	mc_phase_st = rd(DPSS_DPE_MC_PHASE) & 0xff;
	vd1_link_st = is_vd1_link_state();
	dbg_f2("phase0=(%d,%3d,%d), mc_phase=%3d, obuf_rdy=%d, display_ready=%d, pre_vs_cnt=%d\n",
			state_st->mc_set_phase0, state_st->dpe_intp_phs, state_st->mc_phase0_rdma,
			mc_phase_st, src0_disp_obuf_rdy, display_ready, frc_int_st->pre_vsync_cnt);
	dbg_f2("mc_bypass=%d,bypass_cnt=%d,fallback=%d,bypass_chg=%d,trig_pos_chg=%d,mc_link=%d\n",
		state_st->mc_bypass, state_st->mc_bypass_cnt, fallback_st,
		state_st->bypass_chg, state_st->trig_pos_chg, state_st->is_frc_vpp_link);
	dbg_h2("hw_proc_st:nr_dae=%d,nr_dpe=%d,frc_inp=%d,frc_dae=%d,mc_byp_st=%d,vd1_link=%d\n",
			nr_proc_st >> 16 & 7, nr_proc_st & 7,
			frc_proc_st >> 16 & 7, frc_proc_st >> 8 & 7, mc_bypass_st, vd1_link_st);
	//don't add  logic after triggle function,add by fxj
	if (!state_st->trig_pos_chg)
		hw_cfg_dpss_mc_pre_triggle();

#ifdef USE_FRC_PRE_VS_RDMA
	// dpss_rdma_auto_wr_reg(reg, val + idx);
	DPSS_RDMA_WR_BIT_PRE_VS(FRC_REG_TOP_RESERVE15, frc_int_st->pre_vsync_cnt, 0, 32);
	pre_vsync_signal_to_dpss_rdma();

#endif
	if (pchip_st->dbg_st.ctrl_dbg.reg_tab_pr_en & 0x1)
		frc_dbg_read_table_value();
}

void display_game(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	bool src0_disp_obuf_rdy;
	bool phs_swth;
	u64 timestamp = sched_clock();
	// struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_interrupt_s *frc_int_st;
	u32 ro_disp_info;
	u32 mc_ibuf_rdy = 0;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st || !pchip_st->state_st.frc_en)
		return;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("irq_pre_vsync  dpss_frc fw_data is null\n");
		return;
	}
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->frc_vsync_duration = timestamp - frc_int_st->frc_vsync_timestamp;
	frc_int_st->frc_vsync_timestamp = timestamp;
	frc_int_st->frc_vsync_cnt++;

	dpss_vsync_reg_monitor();
	if (frc_ocnt_status)
		dpss_disp0_frm_cnt++;

	src0_disp_obuf_rdy = disp_obuf_trigger(FRC_DST_DONE);
	ro_disp_info = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	mc_ibuf_rdy = (rd(DPSS_FRC_MC_IUFF_STATUS) >> 4) & 0x1;
	state_st->pre_idx = state_st->cur_idx;
	if (mc_ibuf_rdy == 1)
		state_st->cur_idx = (ro_disp_info >> 2) & 0xf;
	phs_swth = state_st->pre_idx != state_st->cur_idx;

	if (phs_swth && src0_disp_obuf_rdy) {
		dbg_h2("now mc rls pre_idx: %d\n", state_st->pre_idx);
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, state_st->pre_idx, 4, 4);
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 1, 1);
		state_st->phs_swth = true;
		// if (prm_top->dpss_mode == DPSS_FRC_MODE)
		dpss_tsk_m_wake(2); // recycle buf
	}

	if (frc_top->fw_pause == 0 && (dpss_dbg_irq_ko & C_BIT4)) {
		if (pfw_data->vsync_irq_handler)
			pfw_data->vsync_irq_handler(pfw_data);
	}
	if (state_st->is_frc_vpp_link) {
		hw_cfg_dpss_mc_triggle();
		dbg_h2("vsync, trigger mc\n");
	}

	if (game_st->video_mute_en &&
		(dpss_disp0_frm_cnt > game_st->video_mute_cnt ||
		frc_int_st->frc_vsync_cnt > 100)) {
		set_video_mute(FRC_MUTE_SET, false);
		game_st->video_mute_en = false;
		dbg_h2("video unmute cnt %d\n", game_st->video_mute_cnt);
	}

	dbg_h2("vsync disp_done_int_cnt=%d\tvsync_cnt=%d\n",
		dpss_disp0_frm_cnt, frc_int_st->frc_vsync_cnt);

#ifdef USE_FRC_VS_RDMA
	// dpss_rdma_auto_wr_reg(reg, val + idx);
	DPSS_RDMA_WR_VS(FRC_REG_TOP_RESERVE13, dpss_disp0_frm_cnt);
	DPSS_RDMA_WR_BIT_VS(FRC_REG_TOP_RESERVE14, dpss_disp0_frm_cnt, 16, 16);
	DPSS_RDMA_WR_BIT_VS(FRC_REG_TOP_RESERVE14, dpss_disp0_frm_cnt + 1, 0, 16);
	// dpss_rdma_auto_wr_tri(2);
#endif
}

void irq_display(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	u64 timestamp = sched_clock();
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_interrupt_s *frc_int_st;
	bool is_vd1_link;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st || !pchip_st->state_st.frc_en)
		return;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("irq_pre_vsync  dpss_frc fw_data is null\n");
		return;
	}
	if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		display_game();
		return;
	}
	state_st = &pchip_st->state_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->frc_vsync_duration = timestamp - frc_int_st->frc_vsync_timestamp;
	frc_int_st->frc_vsync_timestamp = timestamp;
	frc_int_st->frc_vsync_cnt++;
	if (state_st->frc_init) {
		state_st->frc_init = false;
		frc_int_st->irq_chk = 0;
		frc_int_st->irq_chk_flag = 1;
	}
	frc_int_st->irq_chk--;
	if (frc_int_st->irq_chk_flag) {
		if (frc_int_st->irq_chk != -1) {
			frc_int_st->irq_chk_err_cnt++;
			frc_int_st->irq_chk = -1;
		}
	} else {
		if (frc_int_st->irq_chk != 0) {
			frc_int_st->irq_chk_err_cnt++;
			frc_int_st->irq_chk = 0;
		}
	}

	if (state_st->trig_pos_chg)
		DPSS_RDMA_WR_VS(DPSS_DPE_MC_START_CTRL, 0x40000042);

	dpss_vsync_reg_monitor();
	if (frc_ocnt_status)
		dpss_disp0_frm_cnt++;

	is_vd1_link = is_vd1_link_state();
	if (prev_is_vd1_link_valid && prev_is_vd1_link && !is_vd1_link) {
		prm_top->reset_path = true;
		dbg_h2("%s vd1 link->off reset path\n", __func__);
	}
	prev_is_vd1_link = is_vd1_link;
	prev_is_vd1_link_valid = true;
	if (state_st->src_chg) {
		cfg_dpss_mc_ini(prm_top, 1);
		state_st->src_chg = 0;
		state_st->mc_ini_rdma_done = true;
		state_st->use_phase0_done = true;
//		update_frc_state();
		dbg_h2("mc update pre_idx:%d, cur_idx:%d\n",
			state_st->mc_disp_st.disp_pre_idx,
			state_st->mc_disp_st.wr_cur_idx);
	}

	if (frc_top->fw_pause == 0 && (dpss_dbg_irq_ko & C_BIT4)) {
		if (pfw_data->vsync_irq_handler)
			pfw_data->vsync_irq_handler(pfw_data);
	}
	if (state_st->is_frc_vpp_link) {
		hw_cfg_dpss_mc_triggle();
		dbg_h2("vsync, trigger mc\n");
	}

	dbg_h2("vsync disp_done_int_cnt=%d\tvsync_cnt=%d\n",
		dpss_disp0_frm_cnt, frc_int_st->frc_vsync_cnt);

#ifdef USE_FRC_VS_RDMA
	// dpss_rdma_auto_wr_reg(reg, val + idx);
	DPSS_RDMA_WR_BIT_VS(FRC_REG_TOP_RESERVE14, dpss_disp0_frm_cnt, 16, 16);
	DPSS_RDMA_WR_BIT_VS(FRC_REG_TOP_RESERVE14, dpss_disp0_frm_cnt + 1, 0, 16);
	// dpss_rdma_auto_wr_tri(2);
#endif
}

u32 fnr_dpe_obuf_rls_ini;

void irq_inp(void)
{
	unsigned int inp_cnt;
	unsigned int inp_film_mode_fw;
	unsigned int inp_film_phse_fw;
	unsigned int frc_reg_film_phs_1;
	unsigned int inp_film_mode_hw;
	unsigned int inp_film_phs_hw;

	u32 inp_ofrm_vld = 0;
	unsigned int dpss_frc_inp_buf_fid = 0;
	unsigned int inp_org_in = 0;
	unsigned int inp_in = 0;
	unsigned int inp_out = 0;
	unsigned int ro_fd_glb_mot_all = 0;
	unsigned int ro_inp_ibuf = 0;
	unsigned int ro_dae_ibuf = 0;
	unsigned int ro_fd_glb_mot_all_film = 0;
	u32 inp_obuf_info;	// = rd(DPSS_FRC_INP_OBUF_WDATA);
	u32 inp_ibuf_idx;	// = (inp_obuf_info>>4)&0xf;
	u32 nr_dpe_obuf_num = 0;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct frc_interrupt_s *frc_int_st;
	struct dpss_game_state_s *game_st;
	u8 frc_fw_working = 0;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	u64 timestamp = sched_clock();

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;
	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		w_reg_bit(FRC_REG_INP_FRM_RST_CTRL, 1, 4, 1); //rst auto W1T
		dbg_h2("auto irq inp: yline_sync: %#X, cline_sync:%#X\r\n",
			rd(FRC_INP_RD_YLINE_CNT), rd(FRC_INP_RD_CLINE_CNT));
	}

	frc_top = &pfw_data->frc_top_type;
	frc_fw_working = frc_top->frc_fw_working;
	frc_int_st = &pchip_st->state_st.frc_int_st;
	game_st = &pchip_st->game_st;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->inp_duration = timestamp - frc_int_st->inp_timestamp;
	if (frc_int_st->inp_min_duration == 0 ||
		frc_int_st->inp_min_duration > frc_int_st->inp_duration)
		frc_int_st->inp_min_duration = frc_int_st->inp_duration;
	if (frc_int_st->inp_max_duration == 0 ||
		frc_int_st->inp_max_duration < frc_int_st->inp_duration)
		frc_int_st->inp_max_duration = frc_int_st->inp_duration;
	frc_int_st->inp_timestamp = timestamp;
	frc_int_st->inp_int_cnt++;

	inp_film_mode_fw = rd(FRC_REG_PHS_TABLE) & 0xff;
	inp_film_phse_fw = (rd(FRC_REG_FILM_PHS_2) >> 16) & 0xff;
	frc_reg_film_phs_1 = rd(FRC_REG_FILM_PHS_1);
	inp_film_mode_hw = (frc_reg_film_phs_1 >> 8) & 0xff;
	inp_film_phs_hw = (frc_reg_film_phs_1) & 0xff;

	dpss_frc_inp_buf_fid = rd(DPSS_FRC_INP_BUF_FID);
	inp_org_in = (dpss_frc_inp_buf_fid >> 16) & 0xf;
	inp_in = (dpss_frc_inp_buf_fid >> 8) & 0xf;
	inp_out = (dpss_frc_inp_buf_fid) & 0xf;
	inp_cnt = rd(FRC_REG_INPUT_FUL_IDX);

	ro_fd_glb_mot_all = rd(FRC_FD_DIF_GL);
	ro_fd_glb_mot_all_film = rd(FRC_FD_DIF_GL_FILM);
	ro_inp_ibuf = rd(DPSS_FRC_INP_BUF_STATUS) & 0xff;
	ro_dae_ibuf = rd(DPSS_FRC_DAE_BUF_STATUS) & 0xff;

	unsigned int badedit_en = prm_top->badedit_en;
	unsigned int inp_film_phs;

	enum DPSS_FILM_MODE inp_film_mode;
	unsigned int force_film_mode = rd(FRC_REG_TOP_RESERVE1);

	int i = 0;

	if (force_film_mode) {
		inp_film_mode = (enum DPSS_FILM_MODE)(inp_film_mode_fw);
		inp_film_phs = inp_film_phse_fw;
	} else {
		inp_film_mode = (enum DPSS_FILM_MODE)(inp_film_mode_hw);
		inp_film_phs = inp_film_phs_hw;
	}

	if (det_film_mode != inp_film_mode) {
		det_film_phs = inp_film_phs;
		det_filmmode_chg = 1;
		dbg_h2("det_film_phs = %d det_filmmode_chg = %d\n",
		       det_film_phs, det_filmmode_chg);
	}
	nr_dpe_obuf_num = rd(DPSS_FBUF_BUF_CTRL) & 0x1f;

	if (fnr_dpe_obuf_rls_ini == 0 && prm_top->sw_tbc_ctrl_en == 1) {
		dbg_h2("initial nr_dpe_obuf_fifo when INP_CNT: %d\n", inp_cnt);
		fnr_dpe_obuf_rls_ini = 1;
		for (i = 0; i < nr_dpe_obuf_num; i++) {
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, i & 0xf, 4, 4);	//fnr dpe obuf wrpt
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 1, 1);	//pls fnr obuf rdy
		}
	}

	//badedit
	if (badedit_en) {	////write inp frmae valid flag by fw
		if (((rd(FRC_REG_INP_FRM_CTRL) >> 4) & 0x1)) {	//1st frame
			inp_ofrm_vld = config_inp_loop_tab(inp_film_mode,
					inp_film_phs, badedit_en);
			config_dae_loop_tab(prm_top->frc_ratio,
						inp_film_mode, 1);
			dbg_h2("irq_inp_1st frame\n");
		} else {
			if (det_filmmode_chg == 1 && inp_film_phs == 0)
				det_filmmode_chg = 0;
			if (prm_top->frc_me_en == 0) {
				inp_film_mode = DPSS_VIDEO;
				inp_film_phs = 0;
			}
			inp_ofrm_vld = config_inp_loop_tab(inp_film_mode,
					inp_film_phs, badedit_en);
			config_dae_loop_tab(prm_top->frc_ratio,
						inp_film_mode, 1);
			if (frc_top->fw_pause == 0 && pfw_data->inp_irq_handler &&
				(dpss_dbg_irq_ko & C_BIT8)) {
				inp_ofrm_vld = pfw_data->inp_irq_handler(pfw_data);
			}
			if (prm_top->inp_done_int == 1 &&
				(prm_top->sw_tbc_ctrl_en == 1 ||
				prm_top->sw_buf_rls_en == 1))
				cfg_inp_triggle(0);	//frm_end
		}
	} else {
		if (((rd(FRC_REG_INP_FRM_CTRL) >> 4) &  0x1) && prm_top->inp_done_int == 1) {
			dbg_h2("irq_inp_1st frame\n");
		} else {
			if (det_filmmode_chg == 1) {
				if (inp_film_phs == 0) {
					det_film_mode = inp_film_mode;
					det_filmmode_chg = 0;

					inp_ofrm_vld = config_inp_loop_tab(inp_film_mode, 0, 0);
					config_dae_loop_tab(prm_top->frc_ratio,
						inp_film_mode, 1);
					dbg_h2("irq_inp_new film step: %d\n",
						(rd(FRC_REG_TOP_CTRL7) & 0xfffff));
				} else {
					if (det_film_mode == DPSS_VIDEO)
						det_film_phs = 0;
					inp_ofrm_vld = config_inp_loop_tab(det_film_mode,
						det_film_phs, 0);
					config_dae_loop_tab(prm_top->frc_ratio,
						det_film_mode, 1);
					if (det_film_mode == DPSS_VIDEO)
						det_film_phs = 0;
					else
						det_film_phs = det_film_phs + 1;
				}
			} else {
				if (prm_top->frc_me_en == 0) {
					inp_film_mode = DPSS_VIDEO;
					inp_film_phs = 0;
				}
				inp_ofrm_vld = config_inp_loop_tab(inp_film_mode, inp_film_phs, 0);
				config_dae_loop_tab(prm_top->frc_ratio, inp_film_mode, 1);
			}
			if (frc_top->fw_pause == 0 && pfw_data->inp_irq_handler &&
				(dpss_dbg_irq_ko & C_BIT8)) {
				inp_ofrm_vld = pfw_data->inp_irq_handler(pfw_data);
			}
			if (prm_top->inp_done_int == 1 &&
				(prm_top->sw_tbc_ctrl_en == 1 ||
				prm_top->sw_buf_rls_en == 1))
				cfg_inp_triggle(0);	//frm_end
		}
	}
#ifdef FUNC_EN_PQ
	if (prm_top->dpss_mode != DPSS_FRC_MODE)
		dpss_input_ro_data_update(inp_ofrm_vld);
#endif
	inp_obuf_info = rd(DPSS_FRC_INP_OBUF_WDATA);
	inp_ibuf_idx = (inp_obuf_info >> 4) & 0xf;

	if (prm_top->sw_buf_rls_en == 1 && inp_ofrm_vld == 0) {
		vpu_enqueue(&inp_bufQ, inp_ibuf_idx);
		dbg_h2("recyling buf_idx: %#x\n", inp_ibuf_idx);
	}

	if (prm_top->sw_tbc_ctrl_en == 1 && inp_ofrm_vld == 1) {
		if (prm_top->dpss_mode == DPSS_FRC_MODE) {
			wr(FRC_DAE_SW_CTRL1, inp_obuf_info);	//dae src ibuf info_0
			w_reg_bit(FRC_DAE_SW_CTRL0, 1, 27, 1);	//dae src ibuf_rdy
		} else {
			wr(DPSS_FNR_SW_DRV_CTRL2, inp_obuf_info);	//dae src ibuf info_0
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 0, 1);	//dae src ibuf_rdy
		}
	}

	dpss_inp_reg_monitor();
	dbg_h2("inp_ibuf:%d, dae_ibuf:%d, sw_int_cnt:%d\n",
		ro_inp_ibuf, ro_dae_ibuf, dpss_inp_frm_cnt);
	dbg_h2("glb_mot:%8d, glb_mot_film:%8d, film_mode:%2d, inp_ofrm_vld:%d\n",
		ro_fd_glb_mot_all, ro_fd_glb_mot_all_film, inp_film_mode, inp_ofrm_vld);

	dpss_inp_frm_cnt++;

	if (prm_top->dpss_mode != DPSS_FRC_MODE) {
		if (prm_top->is_i && dpss_inp_frm_cnt == 2) {
			inp_ofrm_vld = 0;
			w_reg_bit(FRC_AUTO_MODE_INP_OFRM_WREN, inp_ofrm_vld, 0, 1);
		}
		if (((rd(FRC_REG_INP_FRM_CTRL) >> 4) & 0x1) != 1)
			dpss_input_q_update_status(prm_top->in_q, inp_ofrm_vld, prm_top->is_i);
	} else if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		if (game_st->inp_ctrl)
			w_reg_bit(FRC_INP_HOLD_CTRL, 1, 31, 1);
	}

	//don't add  logic after triggle function,add by fxj
	if (badedit_en) {	////write inp frmae valid flag by fw
		if (((rd(FRC_REG_INP_FRM_CTRL) >> 4) & 0x1) == 1) {	//1st frame
			cfg_inp_triggle(1);	//frm_start
			inp_chg2done_triggle();	//config after triggle
		} else {
			if (prm_top->sw_tbc_ctrl_en != 1 &&
				prm_top->sw_buf_rls_en != 1)
				cfg_inp_triggle(0);	//frm_end
		}
	} else {
		if (prm_top->inp_done_int) {
			if (((rd(FRC_REG_INP_FRM_CTRL) >> 4) & 0x1) == 1) {//1st frame
				cfg_inp_triggle(1);//frm_start
				inp_chg2done_triggle();//config after triggle
			} else {
				if (prm_top->sw_tbc_ctrl_en != 1 &&
					prm_top->sw_buf_rls_en != 1)
					cfg_inp_triggle(0);	//frm_end
			}
		} else {
			if (prm_top->sw_buf_rls_en == 1)
				w_reg_bit(FRC_INP_HOLD_CTRL, 1, 31, 1);	//inp_tbc_ctrl frm_start
			cfg_inp_triggle(1);	//frm_start
		}
	}
}

void dae0_game(void)
{
	// unsigned int frc_reg_top_reserve0 = rd(FRC_REG_TOP_RESERVE0);
	//unsigned int badedit_en = frc_reg_top_reserve0&0x1;
	//unsigned int memc_link_en = (frc_reg_top_reserve0>>1)&0x1;
	unsigned int start_mode;
	unsigned int inp_cnt = rd(FRC_REG_INPUT_FUL_IDX);
	u32 dae_out_fid = rd(FRC_REG_OUT_FID);
	u32 pre_fid;
	u32 cur_fid;
	u32 nxt_fid;
	u32 phs_swth;
	// u32 tmpseg;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct frc_interrupt_s *frc_int_st;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	u64 timestamp = sched_clock();

	//unsigned int pvsync_intr_en = rd(FRC_REG_TOP_RESERVE2);

	enum DPSS_FILM_MODE dae_film_mode =
		(enum DPSS_FILM_MODE)(rd(FRC_FRC_DAE_STATUS) & 0xff);

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;

	game_st = &pchip_st->game_st;
	state_st = &pchip_st->state_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->dae0_duration = timestamp - frc_int_st->dae0_timestamp;
	frc_int_st->dae0_timestamp = timestamp;

	// frc_int_st->dae0_int_cnt++;

	// u32 frc_stats = rd(DPSS_FRC_PROC_STATUS);
	dbg_h2("yline_sync: %#X, cline_sync:%#X\r\n",
		rd(VPU_DAE_WRAP_LLM_RO), rd(VPU_DAE_WRAP_LLM_R1));

	pre_fid = (dae_out_fid >> 16) & 0xf;
	cur_fid = (dae_out_fid >> 12) & 0xf;
	nxt_fid = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	phs_swth = (dae_out_fid >> 20) & 0x1;
	if (dpss_slt_mode)
		w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);
	dpss_dae0_reg_monitor();
	dbg_h2("phs_swth=%d,pre_fid = %d,cur_fid = %d,nxt_fid= %d\r\n",
	       phs_swth, pre_fid, cur_fid, nxt_fid);

	dbg_h2("inp_cnt = %d frc_dae_cnt = %d dpss_dae_frm_cnt_src0 = %d\n",
			inp_cnt, frc_int_st->dae0_int_cnt, dpss_dae_frm_cnt_src0);

	dbg_h2("DAE (dae_in_idx(80a3):%d,dae_idx(80a4):%d,fbuf_dae_idx(83e8):%d,sw_dae_cnt:%d)\n",
		rd(FRC_DAE_IN_FRM_IDX), rd(FRC_DAE_FRM_IDX),
		rd(DPSS_FBUF_DAE_FRM_IDX), frc_int_st->dae0_int_cnt);
	dbg_h2("DAE (stats:%d, film:%d)\n",
		rd(DPSS_FRC_DAE_BUF_STATUS), dae_film_mode);
	dpss_dae_frm_cnt_src0++;
	prm_top->frame_count = dpss_dae_frm_cnt_src0;

	if (pfw_data->dae_irq_handler)
		prm_top->frc_no_alg_ko = 0;
	else
		prm_top->frc_no_alg_ko = 1;

	if (unlikely(game_st->game_dae_byp_en))
		hw_cfg_dpss_dae_frc(DAE_BYPS_MODE, prm_top, prm_dae);
	else
		hw_cfg_dpss_dae_frc(DAE_FRC_MODE, prm_top, prm_dae);

	if (!pfw_data->dae_irq_handler || frc_top->fw_pause == 1)
		prm_top->fw_en = 0;
	else
		prm_top->fw_en = 1;
	hw_config_dae_loop_tab(prm_top->frc_ratio, dae_film_mode, 0);
	//tmpseg = 0xA0A00000;
	//tmpseg |= ((rd(FRC_REG_FILM_PHS_1) >> 8) & 0xff) << 16;
	//wr(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, tmpseg);

	if (frc_top->fw_pause == 0 && pfw_data->dae_irq_handler &&
		(dpss_dbg_irq_ko & C_BIT12))
		pfw_data->dae_irq_handler(pfw_data);
	// hw_config_dae_loop_tab(prm_dpss_top.frc_ratio, dae_film_mode, 0);

	//don't add  logic after triggle function,add by fxj
	start_mode = rd(VPU_DAE_WRAP_CTRL) & 0x1;
	dbg_h2("dae0 start_mode = %d\n", start_mode);
	if (start_mode == 1)
		cfg_dae_triggle();
}

void irq_dae0(void)
{
	// unsigned int frc_reg_top_reserve0 = rd(FRC_REG_TOP_RESERVE0);
	//unsigned int badedit_en = frc_reg_top_reserve0&0x1;
	//unsigned int memc_link_en = (frc_reg_top_reserve0>>1)&0x1;
	u32 dae_out_fid = rd(FRC_REG_OUT_FID);
	u32 pre_fid;
	u32 cur_fid;
	u32 nxt_fid;
	u32 phs_swth;
	u32 start_mode;
	u8 display_inp_idx;
	static u16 pre_org_hsize;
	static u16 pre_org_vsize;
	static u16 dae0_byp_cnt;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct frc_interrupt_s *frc_int_st;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	u64 timestamp = sched_clock();

	enum DPSS_FILM_MODE dae_film_mode = (enum DPSS_FILM_MODE)(rd(FRC_FRC_DAE_STATUS) & 0xff);

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;
	if (!(dpss_get_hw()->ch_act & C_BIT0)) {
		DBG_WARN("%s:ch0 not work\n", "dae0");
		return;
	}
	state_st = &pchip_st->state_st;
	frc_int_st = &state_st->frc_int_st;
	frc_top = &pfw_data->frc_top_type;

	/*update inp_irq time*/
	timestamp = div64_u64(timestamp, 1000);
	frc_int_st->dae0_duration = timestamp - frc_int_st->dae0_timestamp;
	frc_int_st->dae0_timestamp = timestamp;

	frc_int_st->dae0_int_cnt++;

	if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		dae0_game();
		return;
	}

	if (!state_st->dae_ready && frc_int_st->dae0_int_cnt >= 3)
		state_st->dae_ready = true;

	if (state_st->mc_bypass_always == 1 && state_st->dae0_bypass_mode == 1) {
		if (frc_int_st->dae0_int_cnt <= 30 && dae_film_mode != DPSS_VIDEO) {
			state_st->dae0_bypass_mode = 0;
			state_st->mc_bypass_always = 0;
		}
		if (frc_int_st->dae0_int_cnt > 30 && dae_film_mode == DPSS_VIDEO)
			state_st->dae0_bypass_mode = 2;
	} else if (state_st->mc_bypass_always == 2) {
		state_st->dae0_bypass_mode = 2;
	}

	pre_fid = (dae_out_fid >> 16) & 0xf;
	cur_fid = (dae_out_fid >> 12) & 0xf;
	nxt_fid = rd(FRC_DAE_IN_FRM_IDX) & 0xf;
	phs_swth = (dae_out_fid >> 20) & 0x1;
	if (dpss_slt_mode)
		w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);
	dpss_dae0_reg_monitor();
	if (phs_swth) {
		if (state_st->me_pcn_st.use == 0) {
			state_st->me_pcn_st.p = nxt_fid;
			state_st->me_pcn_st.use++;
		} else if (state_st->me_pcn_st.use == 1) {
			state_st->me_pcn_st.c = nxt_fid;
			state_st->me_pcn_st.use++;
		} else if (state_st->me_pcn_st.use == 2) {
			state_st->me_pcn_st.n = nxt_fid;
			state_st->me_pcn_st.use++;
		} else {
			state_st->me_pcn_st.p = state_st->me_pcn_st.c;
			state_st->me_pcn_st.c = state_st->me_pcn_st.n;
			state_st->me_pcn_st.n = nxt_fid;
		}

		if (!state_st->mc_link_available) {
			state_st->dae0_available_cnt++;
			if (state_st->dae0_available_cnt == DAE0_FREEZE_CNT)
				state_st->mc_link_available = true;
		}
	}
	dbg_h2("phs_swth=%d,pre_fid = %d,cur_fid = %d,nxt_fid= %d\n",
	       phs_swth, pre_fid, cur_fid, nxt_fid);
	dbg_h2("dae_film_mode:%d,bypass_mode:%d,sw_dae_cnt:%d\n",
		dae_film_mode, state_st->dae0_bypass_mode, frc_int_st->dae0_int_cnt);
	dpss_dae_frm_cnt_src0++;
	prm_top->frame_count = dpss_dae_frm_cnt_src0;

	if (prm_top->trig_byp_dae0 && !prm_top->reset_path) {
		prm_top->frc_me_en = 0;
		pre_org_hsize = prm_top->org_hsize;
		pre_org_vsize = prm_top->org_vsize;
		prm_top->trig_byp_dae0 = false;
		dae0_byp_cnt = dpss_dbg_dae0_byp_cnt;

		//frc_top->fw_pause = 1;
		//w_reg_bit(VPU_DAE_WRAP_MIF_MASK, 1, 15, 1); // dae bbd 11.11 from yaotao
		//w_reg_bit(VPU_DAE_WRAP_MIF_MASK, 1, 9, 1);  // dae ro 11.11 from yaotao
		wr(VPU_DAE_WRAP_GCLK_CTRL0, 0xaaaaaaaa); // dae clk 11.13 from yaotao
		wr(VPU_DAE_WRAP_GCLK_CTRL1, 0xaaaaaaaa); // dae clk 11.13 from yaotao
		dbg_h2("trig byp dae0,top_org_size(%d,%d)\n",
			prm_top->org_hsize, prm_top->org_vsize);
	} else if (dae0_byp_cnt && !prm_top->reset_path) {
		dae0_byp_cnt--;
		dbg_h2("trig byp dae0 cnt:%d\n", dae0_byp_cnt);
		if (!dae0_byp_cnt) {
			prm_top->frc_me_en = state_st->cur_frc_me_en;
			//frc_top->fw_pause = state_st->cur_fw_pause;
			dbg_h2("frc_me_en=%d fw_pause=%d\n",
				prm_top->frc_me_en, frc_top->fw_pause);
			// wr(VPU_DAE_WRAP_GCLK_CTRL0, 0x0);
			// wr(VPU_DAE_WRAP_GCLK_CTRL1, 0x0);
		}
	}

#ifdef FUNC_EN_PQ
	if (!(is_vd1_link_state()) && prm_dpss_top->dae_nr_mode != DAE_BYPS_MODE)
		dpss_me_ro_data_update();
#endif
	if (pfw_data->dae_irq_handler)
		prm_top->frc_no_alg_ko = 0;
	else
		prm_top->frc_no_alg_ko = 1;

	if (prm_top->dpss_mode != DPSS_FRC_MODE)
		if (prm_top->frc_me_en == 1 && state_st->dae0_bypass_mode != 2)
			hw_cfg_dpss_dae0_frc(DAE_FRC_MODE, prm_top, prm_dae);
		else
			hw_cfg_dpss_dae0_frc(DAE_BYPS_MODE, prm_top, prm_dae);
	else // FRC_ONLY_CASE
		hw_cfg_dpss_dae_frc(DAE_FRC_MODE, prm_top, prm_dae);

	if (!pfw_data->dae_irq_handler || frc_top->fw_pause == 1)
		prm_top->fw_en = 0;
	else
		prm_top->fw_en = 1;
	hw_config_dae_loop_tab(prm_top->frc_ratio, dae_film_mode, 0);
	//tmpseg = 0xA0A00000;
	//tmpseg |= ((rd(FRC_REG_FILM_PHS_1) >> 8) & 0xff) << 16;
	//wr(FRC_MC_SEVEN_FLAG_NUM13_NUM14_NUM15_NUM16, tmpseg);

	if (frc_top->fw_pause == 0 && pfw_data->dae_irq_handler && (dpss_dbg_irq_ko & C_BIT12)) {
		pfw_data->dae_irq_handler(pfw_data);
	}
	// hw_config_dae_loop_tab(prm_dpss_top.frc_ratio, dae_film_mode, 0);

	if (state_st->dae_ready) {
		display_inp_idx = display_buf_q.inp_idx;
		hw_update_display_info(&display_buf_q.data[display_inp_idx]);
		display_buf_q.inp_idx = (display_buf_q.inp_idx + 1) % DPSS_QUEEN_NUM;
	}

	//don't add  logic after triggle function,add by fxj
	start_mode = rd(VPU_DAE_WRAP_CTRL) & 0x1;
	if (start_mode == 1)
		cfg_dae_triggle();
}

void irq_dae1(void)
{
	unsigned int dae_nr_mode, dpe_nr_mode, dpe_di_mode, src_mode, cfg_seed, slc_num;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;

#ifdef FUNC_EN_PQ
	const struct reg_acc *op = &t6w_dpss_reg_acc;
	u32 ro_index, temp_index;
	struct di_parm_s *de_devp = dpss_pq_data(DPSS_MAIN_CHANNEL);
#endif
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;
	struct vframe_s *vfm;
	unsigned int idx_in = 0;

	dbg_h2("%s:m=0x%x\n", __func__, src0_nrdi_frc_en);
	if (!(dpss_get_hw()->src_act & C_BIT0)) {
		DBG_ERR("not act 0\n");
		return;
	}
#ifdef FUNC_EN_PQ
	if (dpss_dae_frm_cnt_src1 < 2) {
		ro_index = 0;
		temp_index = 0;
	} else {
		temp_index = dpss_dae_frm_cnt_src1 - 1;
		ro_index = temp_index % prm_top->num_nr_me_ro; //DPSS_DPS_NUB
	}
	dbg_h2("%s: ro_index=%d, ro_index=%d\n", __func__,
		dpss_dae_frm_cnt_src1, ro_index);

	if (is_vd1_link_state() && prm_dpss_top->dae_nr_mode != DAE_BYPS_MODE)
		dpss_me_ro_data_update();
	if (dpss_dae_frm_cnt_src1 >= 2 && prm_dpss_top->dae_nr_mode != DAE_BYPS_MODE) {
		pq_update_ro_dae(ro_index, DPSS_MAIN_CHANNEL);
		pq_update_ro_me(ro_index, DPSS_MAIN_CHANNEL);
		pq_update_ro_input(ro_index, DPSS_MAIN_CHANNEL);
		if (prm_top->mode_drct2 == 0) {
			dae_fwalg_read(de_devp, op);
			dae_fwalg(de_devp, op);
			dae_fwalg_write(de_devp, op);
		}
	}
#endif
	if (dpss_slt_mode)
		w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);
	dae_nr_mode = prm_top->dae_nr_mode;	//dae_nr_mode
	if (prm_top->mode_drct2)
		dae_nr_mode = DAE_BYPS_MODE;
	dpe_nr_mode = prm_top->dpe_nr_mode;	//dpe_nr_mode
	dpe_di_mode = prm_top->dpe_di_mode;	//dpe_di_mode
	src_mode = prm_top->src_mode;	//src_mode
	cfg_seed = prm_top->cfg_seed;	//cfg_seed
//ary no use    cfg_slc = prm_top->cfg_slc;//cfg_slc
	slc_num = prm_top->slc_num;

	if (prm_top->case_id == TST_CASE_IDX_0157) {
		dbg_h2("irq, di\n");
		prm_top = &prm_dpss_top_di;
		prm_dae = &prm_dpss_dae_di;
		prm_dpe = &prm_dpss_dpe_di;
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);
		hw_process_dae1_nrdi_frm_rst(cfg_seed,
					     dpss_dae_frm_cnt_src0_adj,
					     dpss_dae_frm_cnt_src1_adj,
					     dpss_dae_frm_cnt_src2_adj,
					     src0_nrdi_frc_en,
					     prm_top, prm_dae);
		return;
	}

//	if (dpss_int == IRQ_MODE_CASE_0_SRC0_NR_DI) {	//case 0
	if (prm_top->case_id == TST_CASE_IDX_0000 ||
	    prm_top->case_id == TST_CASE_IDX_0102 ||
	    prm_top->case_id == TST_CASE_IDX_0107 ||
	    prm_top->case_id == TST_CASE_IDX_0157) {
		//follow process_dae1_frm_rst
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);
//dd tmp here:
		if (prm_top->dolby_mode >= DOLBY_DPSS_MODE &&
		   !(dpss_dv_en & C_BIT0)) {
			if (dpss_inp_frm_cnt == 0)
				idx_in = dpss_dae_frm_cnt_src1 % prm_top->num_in;
			else
				idx_in = (dpss_inp_frm_cnt - 2) % prm_top->num_in;

			vfm = dpss_irq_get_vfm(idx_in, 1);
			dpss_dd_dae_update(vfm);
		}

		hw_process_dae1_frm_rst(cfg_seed,
					dpss_dae_frm_cnt_src0_adj,
					dpss_dae_frm_cnt_src1_adj,
					dpss_dae_frm_cnt_src2_adj,
					dae_nr_mode,
					src0_nrdi_frc_en, prm_top, prm_dae);
		hw_process_dae1_nrdi_frm_rst(cfg_seed,
					     dpss_dae_frm_cnt_src0_adj,
					     dpss_dae_frm_cnt_src1_adj,
					     dpss_dae_frm_cnt_src2_adj,
					     src0_nrdi_frc_en,
					     prm_top, prm_dae);

		return;
	}
//	if (dpss_int == IRQ_MODE_CASE_TBC_1000) {	//
	if (prm_top->case_id == TST_CASE_IDX_1000 ||
	    prm_top->case_id == TST_CASE_IDX_1001) {
		hw_dpss_patch_for_bitmatch(inp_frm_idx,
					   dae0_frm_rst_cnt,
					   dae1_frm_rst_cnt,
					   0,
					   fnr_dpe_frm_idx + 1,
					   0,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);
		hw_tbc_process_dae1_frm_done(prm_top, prm_dpe);
		if ((dpss_dbg_tbc & C_BIT1))
			hw_tbc_process_dpe1_frm_rst_tbc(prm_top, prm_dpe,
							dpss_nr_ignore_num);
#ifdef NO_USED
		hw_tbc_process_dae1_frm_rst_tbc(prm_top, prm_dae, cfg_seed,
						dpss_dae_frm_cnt_src0_adj,
						dpss_dae_frm_cnt_src1_adj,
						dpss_dae_frm_cnt_src2_adj);
#endif
		return;
	}
	if (dpss_int == IRQ_MODE_CASE_MY)	//ary case
		hw_irq_src0_nr_only_dae_irq();

	if (pchip_st && (pchip_st->dbg_st.ctrl_dbg.reg_tab_pr_en & 0x2))
		frc_dbg_read_table_value();
}

void dpss_hdr_update_dpe(bool en, void *data, unsigned int pos)
{
	struct vframe_s *vfm = NULL;

	if (!en || !data)
		return;

	dbg_ins2("dpss hdr sw %d\n", pos);
	vfm = (struct vframe_s *)data;

	dpss_hdr_sw(true, vfm);
	dpss_hdr_proc(vfm);
}

void irq_dpe1(void)
{
	unsigned int dae_nr_mode, dpe_nr_mode, dpe_di_mode, src_mode, cfg_seed,
	    cfg_slc, slc_num;
	struct vframe_s *vfm;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;
//	u32 dpss_fnr_vpp_link = rd(DPSS_NR_VPP_LINK); //nr_frc_0113
	bool en_update_hdr = false;
	void *update_hdr_data = NULL;
#ifdef FUNC_EN_PQ
	const struct reg_acc *op = &t6w_dpss_reg_acc;
	u32 ro_index, temp_index, pd_index;
	struct di_parm_s *de_devp = dpss_pq_data(DPSS_MAIN_CHANNEL);
#endif
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	dbg_h2("%s:m=0x%x, en = %d\n", __func__, dpss_int, src0_nrdi_frc_en);

	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;
	u32 dpss_inp_drop_count;
	u32 dpss_total_count;
	unsigned long nr_yaddr;
	//bool ret_rd;

	if (!(dpss_get_hw()->src_act & C_BIT0)) {
		DBG_ERR("not act 1\n");
		return;
	}
#ifdef FUNC_EN_PQ
	if (dpss_dpe_nr_frm_cnt < 2) {
		ro_index = 0;
		temp_index = 0;
		pd_index = 0;
	} else {
		temp_index = dpss_dpe_nr_frm_cnt - 1;
		ro_index = temp_index % prm_top->num_nr_me_ro; //DPSS_DPS_NUB
		pd_index = dpss_dpe_nr_frm_cnt % prm_top->num_nr_me_ro;
	}
	dbg_h2("%s: ro_index=%d, ro_index=%d, index=%d\n", __func__,
		dpss_dpe_nr_frm_cnt, ro_index,
		pd_index);
	if (dpss_dpe_nr_frm_cnt >= 2 && prm_dpss_top->dpe_nr_mode != DPE_NR_BYPS) {
		pq_update_ro_dpe(ro_index, DPSS_MAIN_CHANNEL);
		pq_update_ro_dae_pd(pd_index, DPSS_MAIN_CHANNEL);
		pq_update_ro_dblk_h(ro_index, DPSS_MAIN_CHANNEL);
		pq_update_ro_dblk_v(ro_index, DPSS_MAIN_CHANNEL);
		if (prm_top->mode_drct2 == 0) {
			dpe_fwalg_read(de_devp, op);
			dpe_fwalg(de_devp, op);
			dpe_fwalg_write(de_devp, op);
		}
	}
#endif
	dae_nr_mode = prm_top->dae_nr_mode;	//dae_nr_mode
	dpe_nr_mode = prm_top->dpe_nr_mode;	//dpe_nr_mode
	if (prm_top->mode_drct2) {
		dpe_nr_mode = DPE_NR_BYPS;
		prm_dpe->dcntr_en = 0;
	} else {
		prm_dpe->dcntr_en = prm_dpe->dcntr_en_bk;
	}
	if (prm_top->trig_bypass) {
		dpe_nr_mode = DPE_NR_BYPS;
		prm_top->trig_bypass = 0;
		dbg_h2("trig bypass\n");
	}
	dpe_di_mode = prm_top->dpe_di_mode;	//dpe_di_mode
	src_mode = prm_top->src_mode;	//src_mode
	cfg_seed = prm_top->cfg_seed;	//cfg_seed
	cfg_slc = prm_top->cfg_slc;	//cfg_slc
	slc_num = prm_top->slc_num;

	if (prm_top->case_id == TST_CASE_IDX_0157) {
		dbg_h2("irq,di\n");
		prm_top = &prm_dpss_top_di;
		prm_dpe = &prm_dpss_dpe_di;
		dae_nr_mode = prm_top->dae_nr_mode;	//dae_nr_mode
		dpe_nr_mode = prm_top->dpe_nr_mode;	//dpe_nr_mode
		dpe_di_mode = prm_top->dpe_di_mode;	//dpe_di_mode
		src_mode = prm_top->src_mode;	//src_mode
		cfg_seed = prm_top->cfg_seed;	//cfg_seed
		cfg_slc = prm_top->cfg_slc;	//cfg_slc
		slc_num = prm_top->slc_num;
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);

		if (prm_top->is_i && !prm_top->di_front) {
			nr_yaddr = rd(DPSS_DPE_DIN_WR_BADDR4) >> 5;
			dpss_input_q_update_dpe_status(prm_top, nr_yaddr);
		}

		if (dpss_h_bypass)
			dpe_nr_mode = DPE_NR_BYPS;	//dpe_nr_mode
		if (dpss_en_hdr) {
			vfm = dpss_irq_get_vfm((dpss_dpe_nr_frm_cnt % prm_top->num_in), 5);
			update_hdr_data = (void *)vfm;
			en_update_hdr = true;

			#ifdef _HIS_CODE_
			dbg_ins2("dpss hdr sw di\n");
			dpss_hdr_sw(true, vfm);
			dpss_hdr_proc(vfm);
			#endif
		}
		prm_top->en_update_hdr = en_update_hdr;
		prm_top->update_hdr_data = update_hdr_data;

		afbcd_update_addr(prm_top, dpss_dpe_nr_frm_cnt); //new
		hw_process_dpe1_nrdi_frm_rst(cfg_slc,
					     slc_num,
					     dpss_dpe_nr_frm_cnt_adj,
					     dpss_dpe_di_frm_cnt_adj,
					     dpss_di_ignore_num,
					     dpe_di_mode,
					     src0_nrdi_frc_en,
					     prm_top, prm_dpe);
		//dpss_tsk_m_wake(2);
		prm_top->s_cnt++;
		return;
	}

//	if (dpss_int == IRQ_MODE_CASE_0_SRC0_NR_DI) {	//case 0
	if (prm_top->case_id == TST_CASE_IDX_0000 ||
	    prm_top->case_id == TST_CASE_IDX_0102 ||
	    prm_top->case_id == TST_CASE_IDX_0107 ||
	    prm_top->case_id == TST_CASE_IDX_0157) {
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);

		if (dpss_h_bypass)
			dpe_nr_mode = DPE_NR_BYPS;	//dpe_nr_mode
		if (!prm_top->di_front) {
			dpss_inp_drop_count = dpss_input_get_drop_count(prm_top->in_q);
			dpss_total_count = dpss_dpe_nr_frm_cnt + dpss_inp_drop_count;
		} else {
			dpss_total_count = dpss_dpe_nr_frm_cnt;
		}
		afbcd_update_addr(prm_top, dpss_total_count); //new
		dbg_ct0("dpe: %d + %d, total=%d\n", dpss_dpe_nr_frm_cnt, dpss_inp_drop_count,
			dpss_total_count);
		//check rd:
		//ret_rd = dpss_check_rd(prm_top);
		//follow process_dae1_frm_rst
		if (prm_top->dolby_mode >= DOLBY_DPSS_MODE &&
		   !(dpss_dv_en & C_BIT0)) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			dpss_dd_update_info_by_cnt(dpss_total_count, prm_top, prm_dpe);
#endif
			vfm = dpss_irq_get_vfm((dpss_total_count % prm_top->num_in), 2);
			dpss_dd_dpe_update(vfm);
		} else if (dpss_en_hdr && !prm_top->di_front) {
			vfm = dpss_irq_get_vfm((dpss_total_count % prm_top->num_in), 3);
			en_update_hdr = true;
			update_hdr_data = (void *)vfm;
			#ifdef _HIS_CODE
			dbg_ins2("dpss hdr sw\n");
			dpss_hdr_sw(true, vfm);
			dpss_hdr_proc(vfm);
			#endif
		}
		prm_top->en_update_hdr = en_update_hdr;
		prm_top->update_hdr_data = update_hdr_data;
		hw_process_dpe1_frm_rst(cfg_slc,
					slc_num,
					dpss_dpe_nr_frm_cnt_adj,
					dpss_dpe_di_frm_cnt_adj,
					dpss_nr_ignore_num,
					dpe_nr_mode,
					src0_nrdi_frc_en, prm_top, prm_dpe);
		hw_process_dpe1_nrdi_frm_rst(cfg_slc,
					     slc_num,
					     dpss_dpe_nr_frm_cnt_adj,
					     dpss_dpe_di_frm_cnt_adj,
					     dpss_di_ignore_num,
					     dpe_di_mode,
					     src0_nrdi_frc_en,
					     prm_top, prm_dpe);
//              dpss_tsk_m_wake(2);
		//dpe_fwalg_write(de_devp, op);
		prm_top->s_cnt++;
		return;
	}

//	if (dpss_int == IRQ_MODE_CASE_TBC_1000) {
	if (prm_top->case_id == TST_CASE_IDX_1000 ||
	    prm_top->case_id == TST_CASE_IDX_1001) {
		hw_dpss_patch_for_bitmatch(inp_frm_idx,
					   dae0_frm_rst_cnt,
					   dae1_frm_rst_cnt,
					   0,
					   fnr_dpe_frm_idx + 1,
					   0,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);
		hw_tbc_process_dpe1_frm_done(prm_top);
		prm_top->s_cnt++;
		return;
	}

	if (dpss_int == IRQ_MODE_CASE_MY)	//ary case
		hw_irq_src0_nr_only_dpe_irq();
}

void irq_dae2(void)
{
	unsigned int dae_nr_mode, dpe_nr_mode, dpe_di_mode, src_mode, cfg_seed,
	    cfg_slc, slc_num;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top2;
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae2;
#ifdef FUNC_EN_PQ
	const struct reg_acc *op = &t6w_dpss_reg_acc;
	u32 ro_index, temp_index;
	struct di_parm_s *de_devp = dpss_pq_data(DPSS_PIP_CHANNEL);
#endif
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	dbg_h2("%s:m=0x%x\n", __func__, src0_nrdi_frc_en);

	dae_nr_mode = prm_top->dae_nr_mode;	//dae_nr_mode
	dpe_nr_mode = prm_top->dpe_nr_mode;	//dpe_nr_mode
	dpe_di_mode = prm_top->dpe_di_mode;	//dpe_di_mode
	src_mode = prm_top->src_mode;	//src_mode
	cfg_seed = prm_top->cfg_seed;	//cfg_seed
	cfg_slc = prm_top->cfg_slc;	//cfg_slc
	slc_num = prm_top->slc_num;

	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;

	if (!(dpss_get_hw()->src_act & C_BIT1)) {
		DBG_ERR("not act 2\n");
		return;
	}
#ifdef FUNC_EN_PQ
	if (dpss_dae_frm_cnt_src2 < 2) {
		ro_index = 0;
		temp_index = 0;
	} else {
		temp_index = dpss_dae_frm_cnt_src2 - 1;
		ro_index = temp_index % prm_top->num_nr_me_ro;
	}

	if (dpss_dae_frm_cnt_src2 >= 2 && dae_nr_mode != DAE_BYPS_MODE) {
		pq_update_ro_dae(ro_index, DPSS_PIP_CHANNEL);
		dae_fwalg_read(de_devp, op);
		dae_fwalg(de_devp, op);
		dae_fwalg_write(de_devp, op);
	}
	dbg_h2("%s:ro_index=%d, ro_index=%d\n", __func__,
		dpss_dae_frm_cnt_src2, ro_index);
#endif
	if (dpss_slt_mode)
		w_reg_bit(FRC_ME_CMV_CTRL, 1, 31, 1);
	//follow process_dae1_frm_rst
//	if (dpss_int == IRQ_MODE_CASE_1_SRC1) {
	if (prm_top->case_id == TST_CASE_IDX_0001 ||
	    prm_top->case_id == TST_CASE_IDX_SRC1_NR ||
	    prm_top->case_id == TST_CASE_IDX_SRC1_NRDI) {
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);

		hw_process_dae2_frm_rst(cfg_seed,
					dpss_dae_frm_cnt_src0_adj,
					dpss_dae_frm_cnt_src1_adj,
					dpss_dae_frm_cnt_src2_adj,
					src0_nrdi_frc_en, prm_top, prm_dae);
//              dpss_tsk_m_wake(2);
		return;
	}
	//if (dpss_int == IRQ_MODE_CASE_TBC_1000) {
	if (prm_top->case_id == TST_CASE_IDX_1000 ||
	    prm_top->case_id == TST_CASE_IDX_1001) {
		if ((dpss_dbg_tbc & C_BIT1))
			dbg_tbc_trig(4);	//trig dpe;
	}
}

void irq_dpe2(void)
{
	unsigned int dae_nr_mode, dpe_nr_mode, dpe_di_mode, src_mode, cfg_seed,
	    cfg_slc, slc_num;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top2;
	//struct PRM_DPSS_DAE *prm_dae = &prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe2;
	//bool ret_rd;

#ifdef FUNC_EN_PQ
	const struct reg_acc *op = &t6w_dpss_reg_acc;
	u32 ro_index, temp_index, pd_index = 0;
	struct di_parm_s *de_devp = dpss_pq_data(DPSS_PIP_CHANNEL);
#endif
	bool src0_nrdi_frc_en = (rd(DPSS_TOP_FUNC_CTRL) >> 5) & 0x1;

	dbg_h2("%s:m=0x%x\n", __func__, src0_nrdi_frc_en);

	dae_nr_mode = prm_top->dae_nr_mode;	//dae_nr_mode
	dpe_nr_mode = prm_top->dpe_nr_mode;	//dpe_nr_mode
	dpe_di_mode = prm_top->dpe_di_mode;	//dpe_di_mode
	src_mode = prm_top->src_mode;	//src_mode
	cfg_seed = prm_top->cfg_seed;	//cfg_seed
	cfg_slc = prm_top->cfg_slc;	//cfg_slc
	slc_num = prm_top->slc_num;

	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;

	if (!(dpss_get_hw()->src_act & C_BIT1)) {
		DBG_ERR("not act 3\n");
		return;
	}

#ifdef FUNC_EN_PQ
	if (dpss_dpe_di_frm_cnt < 2) {
		ro_index = 0;
		temp_index = 0;
	} else {
		temp_index = dpss_dpe_di_frm_cnt - 1;
		ro_index = temp_index % prm_top->num_nr_me_ro;
		pd_index = dpss_dpe_di_frm_cnt % prm_top->num_nr_me_ro;
	}
	dbg_h2("%s: ro_index=%d, ro_index=%d, index=%d\n", __func__,
		dpss_dpe_di_frm_cnt, ro_index,
		pd_index);
	if (dpss_dpe_di_frm_cnt >= 2 && dpe_nr_mode != DPE_NR_BYPS) {
		pq_update_ro_dpe(ro_index, DPSS_PIP_CHANNEL);
		pq_update_ro_dae_pd(pd_index, DPSS_PIP_CHANNEL);
		pq_update_ro_dblk_h(ro_index, DPSS_PIP_CHANNEL);
		pq_update_ro_dblk_v(ro_index, DPSS_PIP_CHANNEL);
		dpe_fwalg_read(de_devp, op);
		dpe_fwalg(de_devp, op);
		dpe_fwalg_write(de_devp, op);
	}
#endif
	//check rd:
	//ret_rd = dpss_check_rd(prm_top);
	//follow process_dae1_frm_rst
//	if (dpss_int == IRQ_MODE_CASE_1_SRC1) {
	if (prm_top->case_id == TST_CASE_IDX_0001 ||
	    prm_top->case_id == TST_CASE_IDX_SRC1_NR ||
	    prm_top->case_id == TST_CASE_IDX_SRC1_NRDI) {
		hw_dpss_patch_for_bitmatch(dpss_inp_frm_cnt,
					   dpss_dae_frm_cnt_src0,
					   dpss_dae_frm_cnt_src1,
					   dpss_dae_frm_cnt_src2,
					   dpss_dpe_nr_frm_cnt,
					   dpss_dpe_di_frm_cnt,
					   &dpss_di_ignore_num,
					   &dpss_nr_ignore_num,
					   &dpss_inp_frm_cnt_adj,
					   &dpss_dae_frm_cnt_src0_adj,
					   &dpss_dae_frm_cnt_src1_adj,
					   &dpss_dae_frm_cnt_src2_adj,
					   &dpss_dpe_nr_frm_cnt_adj,
					   &dpss_dpe_di_frm_cnt_adj);

		hw_process_dpe2_frm_rst(cfg_slc,
					slc_num,
					dpss_dpe_nr_frm_cnt_adj,
					dpss_dpe_di_frm_cnt_adj,
					dpss_nr_ignore_num,
					dpss_di_ignore_num,
					dpe_di_mode,
					src0_nrdi_frc_en, prm_top, prm_dpe);

		//dpss_tsk_m_wake(2);
		prm_top->s_cnt++;
		return;
	}
}

u32 dpss_mc_rls_cnt;
void hw_rls0_irq(void)
{
	u32 ibuf_rd_idx;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (prm_top->dpss_mode != DPSS_FRC_MODE) {
		ibuf_rd_idx = (rd(DPSS_FRC_BUFF_RLS_IDX)) & 0xff;
		//no need nr_only_release_input(ibuf_rd_idx); //tmp here
		dbg_h2("%s rls0_idx =%02d rls0_irq_count = %d\n",
			__func__, ibuf_rd_idx, prm_top->rls0_irq_count);
		prm_top->rls0_irq_count++;
		w_reg_bit(DPSS_FRC_BUFF_RLS_IDX, 1, 31, 1);	//pls_frc_ibuf_ridx_rrdy
		dpss_tsk_m_wake(2);
	} else {
		if ((rd(DPSS_FRC_BUFF_RLS_IDX) >> 8) & 0xff) {
			ibuf_rd_idx = (rd(DPSS_FRC_BUFF_RLS_IDX)) & 0xff;
			vpu_enqueue(&inp_bufQ, ibuf_rd_idx);//ibuf_rd_idx
			dbg_h2("rls_buf_cnt: %d,  rls_buf ibuf_rd_idx:%#x\n",
				(rd(DPSS_FRC_BUFF_RLS_IDX) >> 8) & 0xff, ibuf_rd_idx);
			w_reg_bit(DPSS_FRC_BUFF_RLS_IDX, 1, 31, 1);
		}
		dpss_mc_rls_cnt++;
	}
}

//process_vdi_rls_int
void hw_vdi_rls_int(void)
{
	u32 vdi_src_idx = 0;

	//vpu_enqueue(&inp_bufQ, ibuf_rd_idx);//ibuf_rd_idx
	dbg_h2(" rls1_idx=%02d  ary which?\n", vdi_src_idx);
	dpss_rls1_frm_cnt++;
	dpss_tsk_m_wake(12);
}

#ifdef NO_USED
void hw_dpe_rd(void)
{
	bool rdy;

	rdy = disp_obuf_trigger(FNR_DST_DONE);
	if (rdy) {
		cfg_dpss_done_trigger(FNR_DST_DONE);	//last frame
		dpss_disp0_frm_cnt++;
	}
}
#endif

unsigned int hw_check_rd_nub(unsigned int src_mode)	//tmp
{
//      struct PRM_DPSS_TOP *prm_top = &prm_dpss_top;
	unsigned int mode;

	if (src_mode == 0)
		mode = FNR_DST_DONE;
	else
		mode = VDI_DST_DONE;

	return disp_obuf_trigger(mode);
}

unsigned int hw_check_rd_nub_src1(void)
{
	return disp_obuf_trigger(VDI_DST_DONE);

#ifdef NO_USED
	if (src1_disp_obuf_rdy) {	//for nrdi slice
		cfg_dpss_done_trigger(VDI_DST_DONE);	//last frame
		dpss_disp1_frm_cnt++;
	}
#endif
}

void hw_release_buf(unsigned int src_mode, unsigned int idx, bool tbc_mode)
{
	//struct PRM_DPSS_TOP *prm_top = &prm_dpss_top;
	unsigned int mode;

	dbg_h2("%s:src_mode=%d, idx=%d, tbc_mode=%d\n", __func__, src_mode, idx, tbc_mode);
	//tbc mode:
	if (tbc_mode) {
		if (src_mode == 0) {
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, idx & 0xf, 4, 4);
			cfg_dpss_trigger(FNR_DPE_OBUF_RDY);
		} else {
			w_reg_bit(DPSS_VDI_SW_DRV_CTRL1, idx & 0xf, 4, 4);
			w_reg_bit(DPSS_VDI_SW_DRV_CTRL0, 1, 1, 1);
		}
		dbg_h2("%s:tbc_mode:%d\n", __func__, idx);
	}

	if (src_mode == 0)
		mode = FNR_DST_DONE;
	else
		mode = VDI_DST_DONE;

	cfg_dpss_done_trigger(mode);
	disp_obuf_trigger(mode);
}

void dpss_hdr_int(unsigned int x, unsigned int y,
	unsigned int path_mode)
{
	unsigned int dpe_path_cfg;
	unsigned int axi_path_mode;
	unsigned int wrap_mux_cfg_in;
	unsigned int wrap_mux_cfg_out;
	unsigned int size_h;
	unsigned int size_v;
	unsigned int size_slice_h[4];

	/*0: dct->nr->hdr, 1: hdr->dct->nr*/
	if (!path_mode) {
		dpe_path_cfg = 0x1;
		axi_path_mode = 0x1;
		wrap_mux_cfg_in = 0xff4f;
		wrap_mux_cfg_out = 0xf1ffff;
		size_h = 960;
		size_v = y;
		size_slice_h[0] = 960;
		size_slice_h[1] = 960;
		size_slice_h[2] = 960;
		size_slice_h[3] = 960;
	} else {
		dpe_path_cfg = 0x3;
		axi_path_mode = 0x101;
		wrap_mux_cfg_in = 0xff1f;
		wrap_mux_cfg_out = 0xff1fff;
		size_h = 960;
		size_v = y;
		size_slice_h[0] = 960 + 64 + 8;
		size_slice_h[1] = 960 + 128 + 16;
		size_slice_h[2] = 960 + 128 + 16;
		size_slice_h[3] = 960 + 64 + 8;
	}

	//dpe inner path cfg
	//=0 dcntr2hdr, =1 nr2hdr, =2 di2hdr, =3 no hdr
	w_reg_bit(VPU_VBE_TOP_HDR_CTRL, dpe_path_cfg, 0, 2);
	//axird_intf path cfg
	//Bit 9:8 reg_dv_path_mode
	//unsigned, RW, default = 0, 0:off, 1:nr_dv, 2:vd1_dv
	//w_reg_bit_vc(VPU_AXIRD_PATH_CTRL, 1, 8, 2);
	wr_vc(VPU_AXIRD_PATH_CTRL, axi_path_mode);

	//dv wrap mux cfg
	wr_vc(VPU_DOLBY_IPATH_SEL, wrap_mux_cfg_in);
	wr_vc(VPU_DOLBY_OPATH_SEL, wrap_mux_cfg_out);

	//w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_0,
	//	size_slice_h[0], 0, 13);
	//w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_0,
	//	size_slice_h[1], 16, 13);
	//w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_1,
	//	size_slice_h[2], 0, 13);
	//w_reg_bit_vc(VPU_HDR2_SLC_HSIZE_1,
	//	size_slice_h[3], 16, 13);

	//Make vd1_rmem_if1 high priority in terms of DDR request
	w_reg_bit_vc(VD1_IF0_GEN_REG, 0x3, 27, 2);

	w_reg_bit_vc(VPU_HDR2_SIZE_IN, size_h, 0, 13);
	w_reg_bit_vc(VPU_HDR2_SIZE_IN, size_v, 16, 13);
	dbg_h2("%s:width/height=%d/%d\n", __func__, x, y);
	dbg_h2("%s:size_h/size_v=%d/%d\n", __func__, size_h, size_v);

	//wr_vc(DOLBY_HDR2_HIST_CTRL, 0x35);
	//w_reg_bit_vc(DOLBY_HDR2_HIST_CTRL, 1, 16, 1);//HIST ENABLE
	//wr_vc(DOLBY_HDR2_HIST_H_START_END, x - 1);
	//wr_vc(DOLBY_HDR2_HIST_V_START_END, y - 1);

	//Bit 31:29 reg_hist_slc_num
	//wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_0,
	//	((x - 1) << 0) | (0 << 16) | (1 << 29));
	//wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_1, (0 << 0) | (0 << 16));
	//wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_2, (0 << 0) | (0 << 16));
	//wr_vc(DOLBY_HDR2_HIST_SLC_X_ST_ED_3, (0 << 0) | (0 << 16));
}

void dpss_dv_init(void)
{
	u32 vfcd_dv_path = 1;
	u32 dv_sel_in = 1;
	u32 dv_sel_out = 0x0;

	dbg_h1("%s\n", __func__);
	//reg_vfcd_dv_path, 0:disable 1:dv_nr, 2:vd1_dv, 3:di_dv
	w_reg_bit_vc(VPU_AXIRD_PATH_CTRL, vfcd_dv_path, 8, 2);
	w_reg_bit_vc(VPU_DOLBY_IPATH_SEL, dv_sel_in, 0, 4);
	w_reg_bit_vc(VPU_DOLBY_OPATH_SEL, dv_sel_out, 12, 4);
}

//----------------------------------------------------------------
//tbc: cp from process_irq_load_src0_sw
void hw_tbc_src0_input(struct PRM_DPSS_TOP *prm_top, unsigned int inp_src_idx)
{
	enum DPSS_WORK_MODE dpss_mode = prm_top->dpss_mode;
	u32 frc_only = dpss_mode == DPSS_FRC_MODE;
	u32 nrdi_frc = dpss_mode == DPSS_FRC_NR_MODE;
	u32 nrdi_only = frc_only != 1 && nrdi_frc != 1;

//ary no use    u32 src0_nrdi_frc_en = 0; //todo
//ary no use    u32 sec_data = 0;//security

//	int32_t inp_src_idx;
	u32 vdin_buf_idx = 0;
//no use        int i; //ary
//ary no use    static u32 vdin_idx, vdin_dpss_phs;
#ifdef LLM_1_2_BIT_MATCH
	u32 game_mode_en = 0;
#else
	u32 game_mode_en = (rd(FRC_DPSS_LLM) >> 1) & 0x1;
#endif

	dbg_h2("%s cnt: %d:%d\n", __func__, src_frm_idx, inp_src_idx);

	dbg_h2("inp_ibuf_level: %d, inp_ibuf_num: %d\n", inp_ibuf_level,
	       prm_top->num_in);

	if (inp_ibuf_level < prm_top->num_in) {	//ary:tmp here, need change

		dbg_h2("== inp_src_idx: %d ==\n", inp_src_idx);

		u32 reg_src_frm_mif_opt = rd(FRC_SRC_FRM_MIF_OPT) & 0x3;//[1:0]
		u64 inp_info = 0x800LL << 20 |
			((u64)inp_src_idx & 0xf) << 8 |
			(inp_src_idx & 0xf) << 4 |
			(reg_src_frm_mif_opt << 2) | 0;

		inp_fifo[inp_din_idx % prm_top->num_in] = inp_info;
		dbg_h2("== inp_din_idx: 0x%x inp_info: 0x%llx ==\n", inp_din_idx, inp_info);

		//rdma: wr(FRC_REG_FILM_PHS_1, inp_src_idx<<24);//ary :rdma wr ?

		if (nrdi_only) {	//trigger fnr dae ibuf
			cfg_fnr_dae_ibuf_rdy(inp_info);
			fnr_dae_fifo[inp_din_idx % reg_fnr_dae_ibuf_num] = inp_info;
			dpss_dbg_fifo_pt(2);
			dpss_dbg_fifo_pt(4);
		} else {	//trigger inp ibuf
			if (game_mode_en)
				w_reg_bit(FRC_INP_SW_CTRL0, vdin_buf_idx & 0xf, 4, 4);
			else
				w_reg_bit(FRC_INP_SW_CTRL0, inp_src_idx & 0xf, 4, 4);
			cfg_dpss_trigger(FRC_INP_IBUF_RDY);
		}

		if (inp_din_idx == 0)
			pre_film_mode = DPSS_VIDEO;

		//inp_src_idx++;
		inp_din_idx++;
		inp_ibuf_level++;
		src_frm_idx++;

	} else {
		dbg_h2("error!! inp buffer not ready!\n");
	}
	dbg_h2("src0 end:%d,%d,%d\n", inp_din_idx, inp_ibuf_level, src_frm_idx);

	if ((dpss_dbg_tbc & C_BIT0))
		dbg_tbc_trig(1);	//trig dae;
}

//==============================================================//
// Load src1 cp: process_irq_load_src1_sw
//==============================================================//
void hw_tbc_src1_input(struct PRM_DPSS_TOP *prm_top, unsigned int inp_src_idx)
{
	int reg_vdi_src_idx = 8;
//ary no use    u32  sec_data           = 0;
	u32 reg_dae_ibuf_num;
	u32 di_ibuf_widx = 0;
	u32 reg_dae_ibuf_info = 0;
	u32 reg_dae_ibuf_din_h = 0;
	u32 reg_dae_ibuf_din_l = 0;

	if (!src1_data_trigger()) {
		DBG_WARN("%s:no ibuf\n", __func__);
		return;
	}

	reg_vdi_src_idx = (rd(DPSS_TOP_FUNC_CTRL1) >> 16) & 0xf;

	dbg_h2("%s frame%02d\n", __func__, src1_frm_idx_cnt);

	cfg_dpss_done_trigger(VDI_SRC_DONE);

	src1_frm_idx_cnt++;

	reg_dae_ibuf_num = (rd(DPSS_FBUF_BUF_CTRL) >> 16);
	dbg_h2("\tdae ibuf num %02d\n", reg_dae_ibuf_num);
	di_ibuf_widx = ((src1_frm_idx_cnt - 1) % reg_dae_ibuf_num);
	dbg_h2("\tibuf widx %02d\n", di_ibuf_widx);

	reg_dae_ibuf_din_l = (((di_ibuf_widx & 0x1f)     << 9)  |
				((di_ibuf_widx & 0x1f)     << 4)  |
				((reg_dae_ibuf_info & 0x3) << 2)  |
				3);

	w_reg_bit(DPSS_VDI_SW_DRV_CTRL1, reg_dae_ibuf_din_h, 16, 16);
	w_reg_bit(DPSS_VDI_SW_DRV_CTRL2, reg_dae_ibuf_din_l, 0, 32);
	w_reg_bit(DPSS_VDI_SW_DRV_CTRL0, 1, 0, 1);

	if ((dpss_dbg_tbc & C_BIT0))
		dbg_tbc_trig(3);	//trig dae;
}

void cfg_dst_ibuf_rdy(u32 info0, u32 info1)
{
	dbg_h2("wr to dst buffer:0x%x, 0x%x\n", info0, info1);
	wr(FRC_OBUF_SW_CTRL1, info0 & 0xffffffff);
	w_reg_bit(FRC_OBUF_SW_CTRL2, info1 & 0xffff, 0, 16);
	w_reg_bit(FRC_OBUF_SW_CTRL0, 1, 0, 1);
}

//copy: process_dae1_frm_done SPI_DPSS_DAE1_INT
void hw_tbc_process_dae1_frm_done(struct PRM_DPSS_TOP *prm_top, struct PRM_DPSS_DPE *prm_dpe)
{
	u32 fnr_byp = (rd(DPSS_TOP_FUNC_BYP_CTRL) >> 4) & 0x1;
//	int i;
	unsigned int dae_fifo_idx = 0;
	unsigned int dpe_fifo_idx = 0;	//ary add

	dbg_h2("%s cnt: %d %d\n", "dae1:done", fnr_dae_frm_idx, fnr_byp);

#ifdef _HIS_CODE_
	if (fnr_dae_frm_idx == 0) {	//ary: this should put in init?
		dbg_h2("\t push:dpe o buffer?\n");
		for (i = 0; i < reg_fnr_dpe_obuf_num; i++) {
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, i, 4, 4);
			w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 1, 1);
			//ary note: ref to dpss_dae_done[dpe_fifo_idx]
		}
	}
#endif
	if (fnr_dae_frm_idx > 0 && fnr_byp != 1) {
		//check_dae_ro(fnr_dae_frm_idx-1, SRC_IDX_NR);
		//send_event_to_sv(DPSS_DAE1_EVENT, fnr_dae_frm_idx);
	}

	dbg_h2("== fnr_dpe_ibuf: %d fnr_dpe_obuf: %d==\n", fnr_dpe_ibuf_level,
	       fnr_dpe_obuf_level);
	if (fnr_dpe_ibuf_level >= reg_fnr_dpe_ibuf_num) {
		DBG_ERR("error! FNR DPE ibuf is FULL\n");
	} else if ((reg_fnr_dpe_ibuf_num - fnr_dpe_ibuf_level) > 0) {
		u32 dae_src_idx = (rd(DPSS_TOP_FUNC_CTRL) >> 24) & 0xf;	//reg_fnr_src_idx
		u32 dae_byp_mode = fnr_byp;
		u32 dae_frm_phs = fnr_dae_frm_idx >= 1;	//mix frm num
		u32 dae_obuf_wrpt = fnr_dae_frm_idx % reg_fnr_dae_obuf_num;
		//u32 dae_obuf_din_h = rd(DPSS_DAE_OBUF_DIN1)&0xffff;
		//u32 dae_obuf_din_l = rd(DPSS_DAE_OBUF_DIN0);

		dae_fifo_idx = fnr_dpe_din_idx % reg_fnr_dae_obuf_num;
		u64 dae_info0 = fnr_dae_fifo[dae_fifo_idx];
		u64 dae_info1 = ((u64)dae_src_idx << (42 - 32)) |
			((u64)dae_byp_mode << (41 - 32)) |
			((u64)dae_frm_phs << (40 - 32)) |
			((u64)dae_obuf_wrpt << (36 - 32)) |
			((u64)dae_obuf_wrpt << (32 - 32));
		dpss_dbg_fifo_pt(2);

		u64 dae_info = (dae_info1 << 32) | (dae_info0 & 0xffffffff);

		dbg_h2("dpe_din:%d, %d, %d\n", fnr_dpe_din_idx,
		       reg_fnr_dae_obuf_num, dae_fifo_idx);
		dbg_h2("src_idx:0x%x, 0x%x,0x%x,0x%x\n", dae_src_idx,
		       dae_byp_mode, dae_frm_phs, dae_obuf_wrpt);
		dbg_h2("fnr dae_info0: 0x%llx\n", dae_info0);
		dbg_h2("fnr dae_info1: 0x%llx\n", dae_info1);
		dbg_h2("fnr dae_info : 0x%llx\n", dae_info);

		dpe_fifo_idx = fnr_dpe_din_idx % reg_fnr_dpe_ibuf_num;
		fnr_dpe_fifo[dpe_fifo_idx] = dae_info;
		dpss_dbg_fifo_pt(4);

		fnr_dpe_din_idx++;
		if (prm_top->wait_en) {
			dpss_dae_done[dpe_fifo_idx] = true;
			dpss_tsk_m_wake(3);
		} else {
			cfg_fnr_dpe_ibuf_rdy(dae_info0, dae_info1);
		}

		fnr_dpe_ibuf_level++;
	}
	fnr_dae_frm_idx++;
	fnr_dae_ibuf_level--;
	fnr_dae_obuf_level--;
	inp_obuf_level++;
	if (!prm_top->wait_en)
		cfg_dpss_trigger(FRC_INP_OBUF_RDY);

	dbg_h2("%s:%d,%d,%d,%d\n", "dae1 end:",
	       fnr_dae_frm_idx, fnr_dae_ibuf_level, fnr_dae_obuf_level,
	       inp_obuf_level);
}

//copy: process_dpe1_frm_done
void hw_tbc_process_dpe1_frm_done(struct PRM_DPSS_TOP *prm_top)
{
	enum DPSS_WORK_MODE dpss_mode = prm_top->dpss_mode;
	u32 nrdi_frc = dpss_mode == DPSS_FRC_NR_MODE;
	int i;
	u32 fnr_byp = (rd(DPSS_TOP_FUNC_BYP_CTRL) >> 4) & 0x1;
	u32 dst_info0, dst_info1;

	dbg_h2("%s cnt: %d nrdi_frc=%d, fnr_byp = %d\n", "dpe1 done",
	       fnr_dpe_frm_idx, nrdi_frc, fnr_byp);

	if (fnr_dpe_frm_idx == 0) {	//ary tmp
		dbg_h2("dpe1:done, fifo intio\n");
		for (i = 0; i < 8; i++)
			fnr_dpe_idx_fifo[i] = (u32)(i & 0xf);
		fnr_dpe_idx_wrpt = 0;
	}

	u64 dpe_info = fnr_dpe_fifo[fnr_dpe_frm_idx % reg_fnr_dpe_obuf_num];

	dbg_h2("dpe info: 0x%llx\n", dpe_info);

	if (fnr_byp != 1) {
		//free dae ibuf
		// w_reg_bit(DPSS_FNR_SW_DRV_CTRL0, 1, 2, 1);//fnr_dpe_obuf_free,pls
		// w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, fnr_dpe_obuf_free_idx, 8, 8);
		//  fnr_dpe_obuf_free_idx = ACC(fnr_dpe_obuf_free_idx, 15);

		//free inp ibuf
		u32 pre_src_idx = (dpe_info >> 4) & 0xf;

		cfg_dpss_inp_free(pre_src_idx);
		dbg_h2("pre_src_idx:%d\n", pre_src_idx);
		inp_ibuf_level--;
	}

	if (fnr_dpe_frm_idx >= 1 && fnr_byp != 1) {
		//check_nr_ro(fnr_dpe_frm_idx-1, SRC_IDX_NR);
		//send_event_to_sv(DPSS_DPE1_EVENT, fnr_dpe_frm_idx);
	}

	if (nrdi_frc == 1) {	//todo, drop first nr frame for bitmatch
		if (fnr_dpe_frm_idx > 0) {
			dbg_h2("== fnr data to frc dae ==\n");
			dbg_h2("== frc_dae_ibuf: %d frc_dae_obuf: %d==\n",
			       frc_dae_ibuf_level, frc_dae_obuf_level);

			if (frc_dae_ibuf_level >= reg_frc_dae_ibuf_num) {
				dbg_h2("error! FRC DAE ibuf is FULL\n");

			//} else if (frc_dae_obuf_level==0) {
			//    dbg_h2("error! FRC DAE obuf is FULL\n");
			//}
			} else {
				if (fnr_dpe_idx_wrpt == fnr_dpe_idx_rdpt && fnr_dpe_frm_idx > 1)
					dbg_h2("fnr dpe output fifo error! rdpt overwrite wrpt!\n");

				u32 dpe_byp_mode = fnr_byp;

				dbg_h2("dpe info: 0x%llx\n", dpe_info);
				dpe_info = dpe_byp_mode ? dpe_info :
					(((dpe_info >> 12) & 0xfffff) << 12) |
					//((fnr_dpe_frm_idx % reg_fnr_dpe_obuf_num) << 8) |
					//((fnr_dpe_frm_idx % reg_fnr_dpe_obuf_num) << 4) |
					((fnr_dpe_idx_fifo[fnr_dpe_frm_idx %
						reg_fnr_dpe_obuf_num]) << 8) |
					((fnr_dpe_idx_fifo[fnr_dpe_frm_idx %
						reg_fnr_dpe_obuf_num]) << 4) |
					(0 << 2) | 1;
				dbg_h2("dpe info: 0x%llx\n", dpe_info);

				fnr_dpe_idx_rdpt = (fnr_dpe_idx_rdpt + 1) % reg_fnr_dpe_obuf_num;
				dbg_h2("fnr_dpe_idx_wrpt: %d fnr_dpe_idx_rdpt: %d==\n",
						fnr_dpe_idx_wrpt, fnr_dpe_idx_rdpt);

				frc_dae_fifo[frc_dae_din_idx % reg_frc_dae_ibuf_num] = dpe_info;
				frc_dae_din_idx++;
				//cfg_frc_dae_ibuf_rdy(dae_info);

				//put into rdma, use dae frm_rst auto trigger
				//hw_cfg_dpss_dae(DAE_FRC_MODE, prm_top, prm_dae);
				cfg_dpss_dae0_fifo(1, 0);

				wr(FRC_DAE_SW_CTRL1, dpe_info & 0xffffffff);
				w_reg_bit(FRC_DAE_SW_CTRL2,
					  (dpe_info >> 32) & 0xffff, 0, 16);
				cfg_dpss_trigger(FRC_DAE_IBUF_RDY);

				frc_dae_ibuf_level++;
			}
		}
	} else {		//nrdi_only
		if (fnr_dpe_frm_idx > 0) {
			dst_info0 = (dpe_info & 0xffffffff) | 0x01;
			dst_info1 = 0x1;
			cfg_dst_ibuf_rdy(dst_info0, dst_info1);
		}
		fnr_dpe_idx_rdpt = (fnr_dpe_idx_rdpt + 1) % reg_fnr_dpe_obuf_num;
	}
	w_reg_bit(DPSS_NR_SW_DRV_CTRL1, 1, 1, 1);	//dae_obuf_rd_en
	fnr_dpe_frm_idx++;
	fnr_dae_obuf_level++;
	fnr_dpe_ibuf_level--;
	fnr_dpe_obuf_level--;

	dbg_h2("%s:%d,%d,%d,%d\n", "dpe1 end:",
	       fnr_dpe_frm_idx, fnr_dae_obuf_level, fnr_dpe_ibuf_level,
	       fnr_dpe_obuf_level);
}

//SPI_DPSS_DAE1_FRM_RST rdma for dae?
void hw_tbc_process_dae1_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top,
					struct PRM_DPSS_DAE *prm_dae, u32 cfg_seed,
					unsigned int dae_frm_cnt_src0_adj,
					unsigned int dae_frm_cnt_src1_adj,
					unsigned int dae_frm_cnt_src2_adj)
{
	dbg_h2("%s cnt=%d\n", __func__, dae1_frm_rst_cnt);

	if (prm_top->dae_dpe_cfg_en == 1)
		hw_cfg_dpss_dae(DAE_NR_MODE, prm_top, prm_dae);
	if (prm_top->di_interlace) //0501
		hw_dae_update_interlace(prm_top, prm_dae, dae1_frm_rst_cnt);
	hw_dae_update_interlace_cfg(prm_top, dae1_frm_rst_cnt);
	cfg_dpss_dae1_fifo(0, 1);
	if (cfg_seed == 1)
		cfg_dpss_dae_me_rand_seed(1, dae_frm_cnt_src0_adj,
						dae_frm_cnt_src1_adj,
						dae_frm_cnt_src2_adj);
	dae1_frm_rst_cnt++;
	cfg_dae_triggle();
}

//SPI_DPSS_DPE1_FRM_RST
void hw_tbc_process_dpe1_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top, struct PRM_DPSS_DPE *prm_dpe,
					unsigned int nr_ignore_num)
{
	//enum DPSS_WORK_MODE dpe_nr_mode = prm_dpe.dpe_mode;

	//dpe nr frm_rst
	int amdv_prl_mode = prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE;//src0:dv_on,nrdi_off

	dbg_h2("%s:dolby_prl_mode: %d\n", __func__, amdv_prl_mode);

	if (dpss_dpe_nr_frm_cnt <= 1)
		wr(DPSS_DPE_DIN_DMSQ_INIT, 1);//set dms init for first frame
	else
		wr(DPSS_DPE_DIN_DMSQ_INIT, 0);

	if (dpss_dpe_nr_frm_cnt < nr_ignore_num || amdv_prl_mode == 1) {
		if (prm_top->dae_dpe_cfg_en == 1)
			hw_cfg_dpss_dpe(DPE_NR_BYPS, prm_top, prm_dpe, &g_nr_pps_cfg);
	} else {
		if (prm_top->dae_dpe_cfg_en == 1)
			hw_cfg_dpss_dpe(DPE_NR_MODE, prm_top, prm_dpe, &g_nr_pps_cfg);
	}
	hw_dpe_out_addr_rd_tbc(SRC_IDX_NR);
	cfg_dpe_triggle(prm_top);
	if (prm_top->dolby_mode >= DOLBY_DPSS_MODE)
		cfg_dpe_dv_triggle();
	dpss_dpe_nr_frm_cnt++;
}

//cp from
void hw_tbc_process_disp_done_int_tbc(struct PRM_DPSS_TOP *prm_top)
{
	enum DPSS_WORK_MODE dpss_mode = prm_top->dpss_mode;

	u32 frc_only = dpss_mode == DPSS_FRC_MODE;
	u32 nrdi_frc = dpss_mode == DPSS_FRC_NR_MODE;
	u32 nrdi_only = frc_only != 1 && nrdi_frc != 1;

	u64 dpe_info;

	dbg_h2("%s cnt: %d\n", __func__, viu_frm_idx);
	viu_frm_idx++;

	if (nrdi_only == 0) {
		if (mc_proc_phs && frc_dpe_proc_idx > 0) {//after the first mc done
			dbg_h2("== frc_dpe_proc_idx: %x ==\n", frc_dpe_proc_idx);

			dpe_info = frc_dpe_fifo[(frc_dpe_proc_idx - 1) % reg_frc_dpe_ibuf_num];
			dbg_h2("== dpe_info: %llx ==\n", dpe_info);

			dpe_pre_phase = dpe_cur_phase;
			dpe_cur_phase = (dpe_info >> 22) & 0xff;	//todo
			u32 dpe_swth = dpe_cur_phase < dpe_pre_phase;

			dbg_h2("== dpe_cur_phase: %x dpe_pre_phase: %x  ==\n",
			       dpe_cur_phase, dpe_pre_phase);

			//rdma test only wr(DPSS_DPE_MC_IFRM_IDX, frc_dpe_proc_idx<<16);

			cfg_dpss_done_trigger(FRC_DST_DONE); //release previous frame
			if (dpe_swth == 1) {
				if (frc_only == 1) {
					dbg_h2("frc dpe new frame switch, free 1 src ibuf src\n");
					inp_ibuf_level--;
					cfg_dpss_inp_free(dpe_pre_idx);//release previous frame
				} else { //nr frc
					dbg_h2("frc dpe new frame switch, free 1 nr dpe obuf\n");
					//                     fnr_dpe_obuf_level++;
					cfg_dpss_dpe_free(dpe_pre_idx);
				}
			}

			dpe_pre_idx = dpe_cur_idx;
			dpe_cur_idx = (dpe_info >> 2) & 0xf;
		}
		if (mc_proc_phs) {
			cfg_dpss_mc_triggle();
			frc_dpe_proc_idx++;
		}
	}

	if (nrdi_only == 1) {
		if (disp_obuf_trigger(FNR_DST_DONE)) {
			dpe_pre_idx = fnr_dpe_proc_idx % reg_fnr_dpe_obuf_num;
			cfg_dpss_dpe_free(dpe_pre_idx);
			fnr_dpe_proc_idx++;
		}
		//dpe_info = fnr_dpe_fifo[fnr_dpe_proc_idx%reg_fnr_dpe_ibuf_num];
		//dpe_pre_idx = dpe_cur_idx;
		//cfg_dpss_done_trigger(FNR_DST_DONE);
		//if (fnr_dpe_proc_idx>0)
		//fnr_dpe_obuf_level --;
		//dpe_cur_idx = (dpe_info>>4)&0xf;
	}

	//release src1 obuf
	if (disp_obuf_trigger(VDI_DST_DONE))
		cfg_dpss_dpe_src1_free();
}

//cp process_dae2_frm_rst_tbc //SPI_DPSS_DAE2_FRM_RST
void hw_tbc_process_dae2_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top,
				     struct PRM_DPSS_DAE *prm_dae, u32 cfg_seed,
				     unsigned int frm_cnt_src0_adj,
				     unsigned int frm_cnt_src1_adj,
				     unsigned int frm_cnt_src2_adj)
{
	int reg_vdi_src_idx = 8;
	enum PRM_SRC_IDX src_idx = SRC_IDX_DI0;

	reg_vdi_src_idx = (rd(DPSS_TOP_FUNC_CTRL1) >> 16) & 0xf;	//reg_vdi_src_idx [19:16]
	src_idx = (enum PRM_SRC_IDX)reg_vdi_src_idx;
	dbg_h2("%s frame %02d  for src %d\n", __func__, dpss_dae_frm_cnt_src2, src_idx);

	cfg_dae_me_is_top(dpss_dae_frm_cnt_src2);

	if (prm_top->dae_dpe_cfg_en == 1)
		hw_cfg_dpss_dae(DAE_VDI_MODE, prm_top, prm_dae);
	if (cfg_seed == 1)
		cfg_dpss_dae_me_rand_seed(1, frm_cnt_src0_adj,
					frm_cnt_src1_adj,
					frm_cnt_src2_adj);
	cfg_dae_triggle();

	dpss_dae_frm_cnt_src2++;
	dae2_frm_rst_cnt++;	//version:0312 add
}

//cp process_dpe2_frm_rst_tbc SPI_DPSS_DPE2_FRM_RST
void hw_tbc_process_dpe2_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top,
				     struct PRM_DPSS_DPE *prm_dpe,
				     unsigned int di_ignore_nub)
{
	//ary no use    enum DPSS_WORK_MODE dpe_di_mode = prm_dpe->dpe_mode;
	int slc_num = prm_top->dpe_slc_num;

	int reg_vdi_src_idx;
	enum PRM_SRC_IDX src_idx = SRC_IDX_DI0;

	reg_vdi_src_idx = (rd(DPSS_TOP_FUNC_CTRL1) >> 16) & 0xf;
	src_idx = (enum PRM_SRC_IDX)reg_vdi_src_idx;

	dbg_h2("%s frame %02d  for src%d\n", __func__, dpss_dpe_di_frm_cnt, src_idx);

	int di_frm_cnt_adj = dpss_dpe_di_frm_cnt <= di_ignore_nub ? 0 :
		(dpss_dpe_di_frm_cnt - di_ignore_nub);

	cfg_dpss_vdi_istop(dpss_dpe_di_frm_cnt, prm_top->f_top);
	if (di_frm_cnt_adj == 0) {//(only first di need force 1)
		wr(DPSS_DPE_DIN_DMSQ_INIT, 1);
		//ary is repeat with cfg_dpss_vdi_istop w_reg_bit(VPU_DI_HW_SLC_INFO, 1, 8, 1);
	} else {
		wr(DPSS_DPE_DIN_DMSQ_INIT, 0);
		//ary is repeat with cfg_dpss_vdi_istop  w_reg_bit(VPU_DI_HW_SLC_INFO, 0, 8, 1);
	}

	prm_top->dpe_slc_num = slc_num;

	prm_dpe->dpss_dpe_di_frm_cnt = dpss_dpe_di_frm_cnt;	//yu.zong 2024-12-06
	hw_dpe_update_interlace(prm_top, prm_dpe);

	if (prm_top->dae_dpe_cfg_en == 1 || dpss_dpe_di_frm_cnt == di_ignore_nub)
		hw_cfg_dpss_dpe(DPE_VDI_MODE, prm_top, prm_dpe, &g_nr_pps_cfg2);

	if (dpss_dpe_di_frm_cnt < di_ignore_nub) {
		if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE) {
			dpss_vbe_proc_byp(2, 0);//dv_prl
		} else {
			if (!(dpss_dbg_tbc & C_BIT4))
				dpss_vbe_proc_byp(1, 0);//di
		}
	}

	cfg_dpe_triggle(prm_top);

	dpss_dpe_di_frm_cnt++;
}

void dbg_tbc_trig(unsigned int step)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;	//ary need check
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;	//ary need check
	struct PRM_DPSS_DPE *prm_dpe = prm_dpss_dpe;	//ary need check
	unsigned int cfg_seed = prm_top->cfg_seed;

	u32 dpss_di_ignore_num;
	u32 dpss_nr_ignore_num;
	u32 dpss_inp_frm_cnt_adj;
	u32 dpss_dae_frm_cnt_src0_adj;
	u32 dpss_dae_frm_cnt_src1_adj;
	u32 dpss_dae_frm_cnt_src2_adj;
	u32 dpss_dpe_nr_frm_cnt_adj;
	u32 dpss_dpe_di_frm_cnt_adj;

	hw_dpss_patch_for_bitmatch(inp_frm_idx,
				   dae0_frm_rst_cnt,
				   dae1_frm_rst_cnt,
				   0,
				   fnr_dpe_frm_idx + 1,
				   0,
				   &dpss_di_ignore_num,
				   &dpss_nr_ignore_num,
				   &dpss_inp_frm_cnt_adj,
				   &dpss_dae_frm_cnt_src0_adj,
				   &dpss_dae_frm_cnt_src1_adj,
				   &dpss_dae_frm_cnt_src2_adj,
				   &dpss_dpe_nr_frm_cnt_adj,
				   &dpss_dpe_di_frm_cnt_adj);

	if (step == 1) {
		hw_tbc_process_dae1_frm_rst_tbc(prm_top, prm_dae, cfg_seed,
						dpss_dae_frm_cnt_src0_adj,
						dpss_dae_frm_cnt_src1_adj,
						dpss_dae_frm_cnt_src2_adj);
	} else if (step == 2) {
		hw_tbc_process_dpe1_frm_rst_tbc(prm_top, prm_dpe,
						dpss_nr_ignore_num);
	} else if (step == 3) {
		hw_tbc_process_dae2_frm_rst_tbc(prm_top, prm_dae, cfg_seed,
						dpss_dae_frm_cnt_src0_adj,
						dpss_dae_frm_cnt_src1_adj,
						dpss_dae_frm_cnt_src2_adj);
	} else if (step == 4) {
		hw_tbc_process_dpe2_frm_rst_tbc(prm_top, prm_dpe,
						dpss_di_ignore_num);
	}
}

void dpss_dbg_reg_check_rd(void)
{
	unsigned int y, c;

	y = rd(DPSS_DPE_DIN_LUMA_BADDR0);
	c = rd(DPSS_DPE_DIN_CHRM_BADDR0);

	DBG_INF("rd:pre: 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DIN_LUMA_BADDR1);
	c = rd(DPSS_DPE_DIN_CHRM_BADDR1);
	DBG_INF("rd:curr: 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_FBUF_FSIZE);
	DBG_INF("rd:size: %d, %d\n", (y & 0x1fff), (y >> 16) & 0x1fff);
	y = rd(DPSS_FBUF_FSIZE_CFG);
	DBG_INF("rd:size: %d, %d\n", (y & 0x1fff), (y >> 16) & 0x1fff);
}

void dpss_dbg_reg_check_wr(void)
{
	unsigned int y, c;

	y = rd(DPSS_DPE_DIN_WR_BADDR0);
	c = rd(DPSS_DPE_DIN_WR_BADDR2);
	DBG_INF("wr:nr:body: 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DIN_WR_BADDR1);
	c = rd(DPSS_DPE_DIN_WR_BADDR3);
	DBG_INF("wr:nr:hd 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DIN_DWR_BADDR0);
	c = rd(DPSS_DPE_DIN_DWR_BADDR1);
	DBG_INF("wr:nr dw: 0x%x, 0x%x\n", y, c);
	DBG_INF("======================\n");

	y = rd(DPSS_DPE_DIN_WR_BADDR4);
	c = rd(DPSS_DPE_DIN_WR_BADDR6);
	DBG_INF("wr:dv:body: 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DIN_WR_BADDR5);
	c = rd(DPSS_DPE_DIN_WR_BADDR7);
	DBG_INF("wr:dv:hd 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DIN_DWR_BADDR2);
	c = rd(DPSS_DPE_DIN_DWR_BADDR3);
	DBG_INF("wr:dv dw: 0x%x, 0x%x\n", y, c);
	DBG_INF("======================\n");
	y = rd(DPSS_DPE_DV_WR_BADDR0);
	c = rd(DPSS_DPE_DV_WR_BADDR2);
	DBG_INF("wr:dv:body: 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DV_WR_BADDR1);
	c = rd(DPSS_DPE_DV_WR_BADDR3);
	DBG_INF("wr:dv:hd 0x%x, 0x%x\n", y, c);

	y = rd(DPSS_DPE_DV_DW_BADDR0);
	c = rd(DPSS_DPE_DV_DW_BADDR1);
	DBG_INF("wr:dv dw: 0x%x, 0x%x\n", y, c);
	DBG_INF("======================\n");
}

void dpss_dbg_reg_itf(unsigned int step)
{
	if (step == 0xf) {
		dpss_dbg_reg_check_rd();
		dpss_dbg_reg_check_wr();
		return;
	}
	if (step == 1) {
		dpss_dbg_reg_check_rd();
		return;
	}
	if (step == 2) {
		dpss_dbg_reg_check_wr();
		return;
	}
}

void f_dpss_hw_init(struct PRM_DPSS_TOP *prm_top)	//from fpga ucode: dpss_hw_init
{
	//==============================================================//
	// pre_vsync Timing setting
	//==============================================================//

	//ary wr(0x00020008, 0x187);     //pre_vsync timing gen
//#if 0				//move to hw_init_part1
//	w_reg_bit(DPSS_TOP_PER_DLY, 0x0, 0, 16);	//pre_vsync dae_dly_num
//	w_reg_bit(DPSS_TOP_PER_DLY, 0x100, 16, 16);	//pre_vsync dpe_dly_num
//#endif
	//======================================================================
	// parameter initialize
	//======================================================================
	//ary repeat prm_dpss_top.mvx_div_mode  = 1;
	//ary repeat prm_dpss_top.mvy_div_mode  = 1;
//#if 0
//	if (prm_dpss_top.nr_vpp_link_en)
//		prm_dpss_top.dpe_slc_num = 1;
//	else
//		prm_dpss_top.dpe_slc_num = 4;
//#endif
//#if 0				//ary move to fpga_ucode_1217
//	prm_dpss_top->dpe_dw_dsx = prm_dpss_top->dae_dsx_scale;
//	prm_dpss_top->dpe_dw_dsy = prm_dpss_top->dae_dsy_scale;
//	//ary repeat prm_dpss_top.di_frc_link   = 0;
//	//ary repeat prm_dpss_top.auto_alig_en  = 0;
//	prm_dpss_top->film_hwfw_sel = 1;
//#endif
	//ary repeat prm_dpss_top.alig_hmode    = 0;
	//ary repeat prm_dpss_top.alig_vmode    = 0;
	//ary repeat prm_dpss_top.me_mc_link_en = 0;
	//ary repeat prm_dpss_top.dpe_game_mode = 0;
	//ary repeat prm_dpss_top.frc_rev_mode  = 0;
	//ary repeat prm_dpss_top.mc_auto_en    = 0;

	//prm_dpss_top.src_fs_fmt    = (YUV422, PLANAR_X2, BIT_10, CMPR_UN, IS_PSRC);
	//prm_dpss_top.src_ds_fmt    = (YUV444, PLANAR_X1, BIT_10, CMPR_UN, IS_PSRC);
	//prm_dpss_top.nro_fs_fmt    = (YUV422, PLANAR_X2, BIT_10, CMPR_UN, IS_PSRC);
	//prm_dpss_top.nro_ds_fmt    = (YUV422, PLANAR_X2, BIT_10, CMPR_UN, IS_PSRC);
//#if 0				//ary tmp
//	prm_dpss_top.src_fs_fmt.src_fmt = YUV422;
//	prm_dpss_top.src_fs_fmt.src_plan = PLANAR_X2;
//	prm_dpss_top.src_fs_fmt.src_bit = BIT_P010;	//BIT_10;
//	prm_dpss_top.src_fs_fmt.src_cmpr = CMPR_UN;
//	prm_dpss_top.src_fs_fmt.interlace = IS_PSRC;
//	prm_dpss_top.src_ds_fmt.src_fmt = YUV444;
//	prm_dpss_top.src_ds_fmt.src_plan = PLANAR_X1;
//	prm_dpss_top.src_ds_fmt.src_bit = BIT_10;
//	prm_dpss_top.src_ds_fmt.src_cmpr = CMPR_UN;
//	prm_dpss_top.src_ds_fmt.interlace = IS_PSRC;
//
//	prm_dpss_top.nro_fs_fmt.src_fmt = YUV422;
//	prm_dpss_top.nro_fs_fmt.src_plan = PLANAR_X2;
//	prm_dpss_top.nro_fs_fmt.src_bit = BIT_P010;	//BIT_10;
//	if (prm_dpss_top.afrc_cmp_en == 1)
//		prm_dpss_top.nro_fs_fmt.src_cmpr = CMPR_AFRC;
//	else
//		prm_dpss_top.nro_fs_fmt.src_cmpr = CMPR_UN;
//	prm_dpss_top.nro_fs_fmt.interlace = IS_PSRC;
//	prm_dpss_top.nro_ds_fmt.src_fmt = YUV444;
//	prm_dpss_top.nro_ds_fmt.src_plan = PLANAR_X1;
//	prm_dpss_top.nro_ds_fmt.src_bit = BIT_10;
//	prm_dpss_top.nro_ds_fmt.src_cmpr = CMPR_UN;
//	prm_dpss_top.nro_ds_fmt.interlace = IS_PSRC;
//	prm_dpss_top.prm_cut_win.frm_hsize = 1024;
//	prm_dpss_top.prm_cut_win.frm_vsize = 512;
//	prm_dpss_top.prm_cut_win.win_hbgn = 128;	// 0; // 128;
//	prm_dpss_top.prm_cut_win.win_hend = 128 + 512 - 1;
//	prm_dpss_top.prm_cut_win.win_vbgn = 0;
//	prm_dpss_top.prm_cut_win.win_vend = 512 - 1;
//	prm_dpss_top.prm_cut_win.win_hsize = 512;
//	prm_dpss_top.prm_cut_win.win_vsize = 512;
//#endif
	//======================================================================
	//Top initial&reset
	//======================================================================

	//======================================================================
	//queue initial
	//======================================================================

//      queue_ini();

	//======================================================================
	//nr dae bypass size cfg
	//======================================================================
	u32 dae_byps_hsize = 100;
	u32 dae_byps_vsize = 10;

	wr(VPU_DAE_WRAP_BYPASS, ((dae_byps_hsize << 16) | dae_byps_vsize));

	//======================================================================
	//ds
	//======================================================================
	/*
	 *  w_reg_bit(VPU_NR_OUT_CROP, 56, 0, 8);
	 *  w_reg_bit(VPU_NR_OUT_CROP, 56, 8, 8);
	 *  w_reg_bit(DPSS_DPE_INTF_FMT_CTRL0, 8, 8);
	 *  w_reg_bit(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x76543102, 0, 32);
	 */
	//w_reg_bit(FRC_MC_HW_CTRL0, 1, 3, 1);//lp_mode

	//======================================================================
	//sel dpss src to vd1
	//======================================================================
	if (prm_top->nr_vpp_link_en == 1)
		w_reg_bit(VPU_AXIRD_PATH_CTRL, 2, 0, 3);
	if (prm_top->nr_inp_en == 1)
		wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88882810);
	else
		w_reg_bit(VPU_AXIRD_PATH_CTRL, 0, 0, 3);

	//Film_mode initial
	//#ifndef TEST_FORCE_MIX
	w_reg_bit(FRC_REG_INP_IBUF_CFG, 1, 0, 1);	//SW send buffer id to dpss
	//#endif
	//======================================================================
	//======================================================================
	//inp/dae/dpe manual/auto triggle mode
	w_reg_bit(DPSS_DPE_START_CTRL, 1, 0, 1);	//dpe frm ctrl start,  0:hold line 1:reg
	w_reg_bit(FRC_REG_INP_FRM_CTRL, 1, 4, 1);	//inp frm ctrl start
	w_reg_bit(VPU_DAE_WRAP_CTRL, 1, 0, 1);	// dae frm ctrl start
	if (prm_top->pvsync_intr_en == 0) {
		w_reg_bit(DPSS_DPE_MC_START_CTRL, 0, 1, 1);
		w_reg_bit(DPSS_DPE_MC_START_CTRL, 32, 4, 1);
	}

	//======================================================================
	//======================================================================
	w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, prm_top->dpss_nr_byp, 4, 1);	//fnr byp
	if (prm_top->nr_vpp_link_en == 1)
		w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, 1, 8, 1);	//frc byp
	else
		w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, 0, 8, 1);	//frc byp

	prm_top->film_hwfw_sel = 1;
	prm_top->dae_step_sel = 2;

	w_reg_bit(FRC_REG_FILM_PHS_1, prm_top->film_hwfw_sel, 16, 1);
	if (prm_top->force_film_mode) {
		w_reg_bit(FRC_REG_TOP_CTRL17, 1, 9, 1);	//force film phase
		w_reg_bit(FRC_REG_TOP_CTRL7, 0, 20, 2);
	} else {
		w_reg_bit(FRC_REG_TOP_CTRL17, 0, 9, 1);	//force film phase
		w_reg_bit(FRC_REG_TOP_CTRL7, prm_top->dae_step_sel, 20, 2);
	}
	//=====================================================================
	//write reverse option
	//======================================================================

	wr(FRC_REG_TOP_RESERVE0, prm_top->inp_done_int << 2 |
				prm_top->me_mc_link_en << 1 |
				prm_top->badedit_en); //badedit_en
	wr(FRC_REG_TOP_RESERVE1, prm_top->force_film_mode); //force_film_mode
	wr(FRC_REG_TOP_RESERVE2, prm_top->pvsync_intr_en); //pvsync_intr_en

	dbg_h0("pvsync_intr_en = %d\n", prm_top->pvsync_intr_en);
	//======================================================================
	//======================================================================
	//FORCE MV&LOGO
	//w_reg_bit(FRC_MC_FORCE_BV, 0, 0, 1); //force_bv
	//w_reg_bit(FRC_MC_FORCE_BV, 0, 4, 12); //mvy
	//w_reg_bit(FRC_MC_FORCE_BV, -48, 16, 13);//mvx
	w_reg_bit(FRC_MC_LBUF_LOGO_CTRL, 1, 8, 1);	//reg_mc_force_melg_en
	w_reg_bit(FRC_MC_LBUF_LOGO_CTRL, 1, 5, 1);	//reg_mc_force_iplogo_en

	//======================================================================
	//======================================================================
	//TOP_LATCH&OUT_ORDER_BUFFER
	w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 0, 0, 1);	//release in order=1, 0:disorder
	//w_reg_bit(FRC_MC_HW_CTRL0, 1, 0, 1);//reg_mc_bypass_en
	//w_reg_bit(FRC_MC_MVRD_CTRL, 0, 0, 1);

	//=====================================================================
	//======================================================================
	//frm_dly
	w_reg_bit(FRC_DAE_FRM_DLY_NUM, 2, 0, 5);	//reg_dae_frm_dly_num

	if (dpss_slt_mode)
		w_reg_bit(DPSS_FBUF_DAE_INIT, 2, 8, 5);	//nr reg_dae_mixo_num
	else
		w_reg_bit(DPSS_FBUF_DAE_INIT, 0, 8, 5);	//nr reg_dae_mixo_num
	w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 1, 0, 1);	//all nrout to frc
	//#ifndef TEST_FORCE_MIX
	if (prm_top->sw_tbc_ctrl_en == 1) {
		//split inp->nrdi
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, 2, 0, 2);	//change fnr dae to sw-tbc ctrl mode

		//split nrdi->frc
		w_reg_bit(FRC_DAE_SW_CTRL0, 2, 30, 2);	//change frc dae to sw-tbc ctrl mode
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 3, 2 * 5, 2);	//change nr dpe int to frm done
		w_reg_bit(DPSS_DPE_START_CTRL, 0, 0, 1); //change nr dpe to hold-line start mode
	}
	//#endif
	//======================================================================
	//
	//=====================================================================
	w_reg_bit(FRC_REG_CURSOR, 1, 3, 1);
	w_reg_bit(VPU_DAE_WRAP_ASYNC, 0, 25, 2);

	// prm_dpss_top.badedit_en == 0/1
	// w_reg_bit(DPSS_REG_LOOP_PHASE_ADJ, 3/0, 4, 2);

	if (prm_top->me_mc_link_en == 1)
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 2, 0, 2);	//reg_dae_top_arb_mode
	else
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 1, 0, 2);	//reg_dae_top_arb_mode

	//======================================================================
	//======================================================================
	w_reg_bit(DPSS_TOP_SYNC_RST, 0x1ff, 0, 9);// pls_ctrl_syn_rst  //for some regs latch
//#if 0	//ary
//	//==============================================================//
//	// peripheral  configuration
//	//==============================================================//
//	peripheral_config(prm_top->frm_hsize,
//	prm_top->frm_vsize,
//	prm_top->dae_dsx_scale,
//	prm_top->dae_dsy_scale,
//	output_type);
//#endif
//	//=====================================================================
//#if 0
//	//PAT_GEN
//	wr(0x00003887, 0x000c0000);//pat mvx mvy
//	w_reg_bit(0x00003888, prm_dpss_top.film_mode, 5, 7);//pat mode
//	if (prm_dpss_top.disp_pat_en == 1)
//		wr(0x00003880, 0x00000124);//pat gen open
//	else
//	wr(0x00003880, 0x00000024);//pat gen close
//
//	//======================================================================
//	//=====================================================================
//	// CAP DISP INTF
//	wr(0x00001136, 0x00000003);//open wrmif
//	//======================================================================
//
//
//	dbg_h2("\nhw initialize end!!!!!!!!!!!!!!!!!!!!!!\n");
//#endif
	dbg_h2("%s:end\n", __func__);
	//======================================================================
}

void hw_config_top_post(struct PRM_DPSS_TOP *prm_top)
{
	u32 dae_byps_hsize = 100;
	u32 dae_byps_vsize = 10;

	wr(VPU_DAE_WRAP_BYPASS, ((dae_byps_hsize << 16) | dae_byps_vsize));

	if (prm_top->nr_vpp_link_en == 1) {
		//vfcd_path [2]:reg_di_vpp_link  [1]:reg_nr_vpp_link [0]:reg_vd1_vpp_link
		w_reg_bit(VPU_AXIRD_PATH_CTRL, 2, 0, 3);
		if (prm_top->nr_inp_en == 1)
			wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88882810);	//src0 nrdi
	} else {
		//vfcd_path [2]:reg_di_vpp_link  [1]:reg_nr_vpp_link [0]:reg_vd1_vpp_link
		w_reg_bit(VPU_AXIRD_PATH_CTRL, 0, 0, 3);
	}

	//Film_mode initial
	w_reg_bit(FRC_REG_INP_IBUF_CFG, 1, 0, 1);	//SW send buffer id to dpss

	//inp/dae/dpe manual/auto triggle mode
	w_reg_bit(DPSS_DPE_START_CTRL, 1, 0, 1);	//dpe frm ctrl start, 0:hold line 1:reg
	w_reg_bit(FRC_REG_INP_FRM_CTRL, 1, 4, 1);	//inp frm ctrl start
	w_reg_bit(VPU_DAE_WRAP_CTRL, 1, 0, 1);	// dae frm ctrl start
	if (prm_top->pvsync_intr_en == 0) {
		w_reg_bit(DPSS_DPE_MC_START_CTRL, 0, 1, 1);
		w_reg_bit(DPSS_DPE_MC_START_CTRL, 32, 4, 1);
	}

	w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, prm_top->dpss_nr_byp, 4, 1);	//fnr byp
	if (prm_top->nr_vpp_link_en == 1)
		w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, 1, 8, 1);	//frc byp
	else
		w_reg_bit(DPSS_TOP_FUNC_BYP_CTRL, 0, 8, 1);	//frc byp

	prm_top->film_hwfw_sel = 1;//0:tab 1:inp site config step 2:dae site config step
	prm_top->dae_step_sel = 2;//0:tab 1:inp site config step 2:dae site config step

	w_reg_bit(FRC_REG_FILM_PHS_1, prm_top->film_hwfw_sel, 16, 1);//film phase 1:fw 0:hw
	if (prm_top->force_film_mode) {
		w_reg_bit(FRC_REG_TOP_CTRL17, 1, 9, 1);	//force film phase
		//0:tab 1:inp site config step 2:dae site config step
		w_reg_bit(FRC_REG_TOP_CTRL7, 0, 20, 2);
	} else {
		w_reg_bit(FRC_REG_TOP_CTRL17, 0, 9, 1);	//force film phase
		//0:tab 1:inp site config step 2:dae site config step
		w_reg_bit(FRC_REG_TOP_CTRL7, prm_top->dae_step_sel, 20, 2);
	}
	//======================================================================
	//write reverse option
	//=====================================================================
#ifdef RUN_ON_ARM	//ary add
	vpu_initqueue(&inp_bufQ);
#endif
	wr(FRC_REG_TOP_RESERVE0, prm_top->inp_done_int << 2 |
		prm_top->me_mc_link_en << 1 | prm_top->badedit_en); //badedit_en
	wr(FRC_REG_TOP_RESERVE1, prm_top->force_film_mode); //force_film_mode
	wr(FRC_REG_TOP_RESERVE2, prm_top->pvsync_intr_en); //badedit_en

	//=====================================================================
	//======================================================================
	//FORCE MV&LOGO
	//w_reg_bit(FRC_MC_FORCE_BV, 0, 0, 1); //force_bv
	//w_reg_bit(FRC_MC_FORCE_BV, 0, 4, 12); //mvy
	//w_reg_bit(FRC_MC_FORCE_BV, -48, 16, 13);//mvx
	w_reg_bit(FRC_MC_LBUF_LOGO_CTRL, 1, 8, 1);	//reg_mc_force_melg_en
	w_reg_bit(FRC_MC_LBUF_LOGO_CTRL, 1, 5, 1);	//reg_mc_force_iplogo_en

	//=====================================================================
	//====================================================================
	//TOP_LATCH&OUT_ORDER_BUFFER
	w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 0, 0, 1);	//release in order=1, 0:disorder
	//w_reg_bit(FRC_MC_HW_CTRL0,1,0,1);//mc byp

	//=====================================================================
	//=====================================================================
	//frm_dly
	w_reg_bit(FRC_DAE_FRM_DLY_NUM, 2, 0, 5);	//reg_dae_frm_dly_num
	if (dpss_slt_mode)
		w_reg_bit(DPSS_FBUF_DAE_INIT, 2, 8, 5);	//nr reg_dae_mixo_num
	else
		w_reg_bit(DPSS_FBUF_DAE_INIT, 0, 8, 5);	//nr reg_dae_mixo_num
	w_reg_bit(DPSS_FBUF_LOW_LATENCY_MODE, 1, 0, 1);	//all nrout to frc
//#ifndef TEST_FORCE_MIX
	if (prm_top->sw_tbc_ctrl_en == 1) {
		//split inp->nrdi
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, 2, 0, 2);	//change fnr dae to sw-tbc ctrl mode

		//split nrdi->frc
		w_reg_bit(FRC_DAE_SW_CTRL0, 2, 30, 2);	//change frc dae to sw-tbc ctrl mode
		w_reg_bit(DPSS_TOP_INT_MODE_CTRL, 3, 2 * 5, 2);	//change nr dpe int to frm done
		w_reg_bit(DPSS_DPE_START_CTRL, 0, 0, 1);//change nr dpe to hold-line start mode
	}
	w_reg_bit(FRC_REG_CURSOR, 1, 3, 1);
	w_reg_bit(VPU_DAE_WRAP_ASYNC, 0, 25, 2);

	// prm_dpss_top.badedit_en == 0/1
	// w_reg_bit(DPSS_REG_LOOP_PHASE_ADJ, 3/0, 4, 2);

	if (prm_top->me_mc_link_en == 1)
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 2, 0, 2);	//reg_dae_top_arb_mode
	else
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 1, 0, 2);	//reg_dae_top_arb_mode

	w_reg_bit(DPSS_TOP_SYNC_RST, 0x1ff, 0, 9);	// pls_ctrl_syn_rst  //for some regs latch
}

void venc_vrr_vdin(void)
{
	static u32 enable;
	u32 vtotal, vmin_num, vmax_num, out_frm_rate;
	u32 encl_frc_ctrl, venc_vrr_ctrl, venc_vrr_adj_lmt, venc_vrr_ctrl_1;
	struct dpss_frc_fw_data_s *pfw_data;
	struct vinfo_s *vinfo;
	struct dpss_game_dly_s *dly_st;
	struct dpss_game_state_s *game_st;
	struct dpss_frc_top_type_s *frc_top;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data || !pchip_st)
		return;

	game_st = &pchip_st->game_st;
	dly_st = &game_st->dly_st;
	venc_vrr_ctrl = pchip_st->venc_vrr_ctrl;
	encl_frc_ctrl = pchip_st->encl_frc_ctrl;
	venc_vrr_adj_lmt = pchip_st->venc_vrr_adj_lmt;
	venc_vrr_ctrl_1 = pchip_st->venc_vrr_ctrl_1;
	frc_top = &pfw_data->frc_top_type;
	frc_top->game_mode = (game_st->game_film << 1) | game_st->game_n2m;
	prm_top->game_mode_n2m = frc_top->game_mode & 0x1;
	prm_top->game_mode_film = (frc_top->game_mode >> 1) & 0x1;

	vinfo = get_current_vinfo();
	if (vinfo)
		out_frm_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;

	if (game_st->off_game_mode) {
		prm_top->game_mode_n2m  = 0;
		prm_top->game_mode_film = 0;
		game_st->off_game_mode = false;
	}

	dbg_h2("frc game_mode:%d->%d\n",
		frc_top->game_mode, (prm_top->game_mode_n2m | prm_top->game_mode_film));
	if (prm_top->game_mode_film == 1 || prm_top->game_mode_n2m == 1) {
		if (enable == 1)
			return;
		if (aml_vrr_state()) { // vrr enable, delay enable game frc
			game_st->delay_enable_game_frc = true;
			game_st->delay_enable_cnt = dpss_dbg_delay_cnt;
			return;
		}
		enable = 1;
		vtotal = vinfo->vtotal;
		dly_st->vtotal = vtotal;
		vmin_num = vtotal - dly_st->pre_dly - dly_st->vmin_num_adj;
		vmax_num = vtotal - dly_st->pre_dly + dly_st->vmax_num_adj;

		wr_vc(venc_vrr_ctrl, (1 & 0xf) << 24 |  //cfg_vrr_frm_num
			(dly_st->vsp_dly & 0xffff) << 8 |  //cfg_vsp_dly_num
			(0 & 0xf) << 4 |        //cfg_vrr_frm_ths
			1 << 2 |                //cfg_vrr_vsp_en  vdin en
			(enable & 0x1) << 1 |   //cfg_vrr_mode
			1                       //sel vdin
		);

		wr_vc(encl_frc_ctrl, vtotal << 16); //cfg_vrr_vtotal,cfg_frc_st_ln
		wr_vc(venc_vrr_adj_lmt, vmin_num << 16 | vmax_num);//cfg_vrr_min/max_vnum
		if (prm_top->game_mode_film)
			w_reg_bit_vc(venc_vrr_ctrl_1, 0x0, 4, 8); //film iofrm_num set 0
		else
			w_reg_bit_vc(venc_vrr_ctrl_1, 0x10, 4, 8); //cfg_iofrm_num 1:2

		w_reg_bit_vc(venc_vrr_ctrl_1, dly_st->pre_dly, 12, 16); //cfg_iofrm_num
		game_st->mc_need_byp_en = false;
		dbg_h2("game_frc enable : %d\n", enable);
	} else if (prm_top->game_mode_film == 0 &&
		   prm_top->game_mode_n2m == 0) {
		if (enable == 0)
			return;
		enable = 0;
		wr_vc(venc_vrr_ctrl, (0 & 0xf) << 24 | //cfg_vrr_frm_num
			(0 & 0xffff) << 8 |     //cfg_vsp_dly_num
			(0 & 0xf) << 4 |        //cfg_vrr_frm_ths
			0 << 2 |                //cfg_vrr_vsp_en  vdin en
			(enable & 0x1) << 1 |   //cfg_vrr_mode
			0                       //sel vdin
		);

		w_reg_bit_vc(venc_vrr_ctrl, 1, 30, 1); // clr vrr
		wr_vc(venc_vrr_adj_lmt, 0);
		wr_vc(encl_frc_ctrl, 0); //cfg_vrr_vtotal,cfg_frc_st_ln
		w_reg_bit_vc(venc_vrr_ctrl_1, 0x0, 4, 8);       //cfg_iofrm_num
		w_reg_bit_vc(venc_vrr_ctrl_1, 0x0, 12, 16);     //cfg_iofrm_num
		w_reg_bit_vc(VDIN_TOP_SIZE, 0, 16, 4);
		w_reg_bit_vc(VDIN_TOP_SIZE, 0, 20, 4);
		dbg_h2("game_frc enable : %d\n", enable);
		//frc_top->game_mode = 0;
	}
}
