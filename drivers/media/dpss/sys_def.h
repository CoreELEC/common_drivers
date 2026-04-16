/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SYS_DEF_H__

#define __SYS_DEF_H__

/**/
//#define RUN_ON_PC	(1)
#define RUN_ON_ARM	(1)
//need_check
//set_need_check
//ary addr change *4
#ifdef RUN_ON_PC
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> //for Sleep
#include <stdbool.h>
#endif
/*************************************/

#define FUNC_EN_DCT	(1)
#define FUNC_EN_FGRAIN	(1)

//#define FUNC_EN_DAE_HPPS	(1) //have float

#define FUNC_DI_FRONT		(1)
//#define FUNC_EN_CHECK_RO (1)
/*************************************/

//close some function:
//#define EN_DISPLAY_FUNC	(1)
//#define ARY_SIM		(1)

#define ARY_IRQ_EN	(1)

/*************************************/

//----ary add test code
#define TABLE_FLG_END	(0xfffffffe)
#define DIMTABLE_LEN_MAX (6000)
//----------------------
#ifdef RUN_ON_ARM
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>

#endif
#ifdef RUN_ON_PC
typedef unsigned int u32;
typedef unsigned long u64;
#endif

/*************************/
#ifdef RUN_ON_PC
#define base_pr		printf
#else
//#include "../dpss_base.h"
#define base_pr		pr_debug
#endif

#define PRC_DBG(fmt, args ...)		base_pr("sw:" fmt, ##args)

#define PRC_REG		base_pr

//#define di_print			base_pr
#define dbg_tmp				base_pr

#define st_printf		base_pr
#define sprint			base_pr //ary use this for debug infor

//#define fpga_mc_printf  base_pr
#define fpga_fifo_printf base_pr

#ifdef RUN_ON_PC
#else
//#include "hw/register_t6w.h"
#include <linux/amlogic/media/di/register_t6w.h>
#include "hw/register_t6w_vc.h"
#include <linux/amlogic/media/registers/register_map.h>
#include "dpss_base.h"

#include "hw/dpss_param.h"
#include "hw/decontour.h"
#include "hw/dpss_lib.h"
#include "hw/dpss_inp.h"
#include "hw/dpss_intf.h"

#include "hw/dpss_dae.h"
#include "hw/dpss_dpe.h"
#include "hw/dpss_mc.h"
#include "hw/dpss.h"
#include "hw/dpss_sw_cfg.h"
#include "dpss_hw.h"
#include <linux/amlogic/media/di/di_pq.h>
#endif /* RUN_ON_PC */

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#ifdef RUN_ON_ARM
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#endif
#else
struct rdma_op_s {
	void (*irq_cb)(void *arg);
	void *arg;
};

#endif

/************************************/
/*register rd wr switch             */
/************************************/

void wr(unsigned int addr, unsigned int val);
unsigned int rd(unsigned int adr);
unsigned int w_reg_bit(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len);
void wr_vc(unsigned int addr, unsigned int val);
unsigned int rd_vc(unsigned int adr);
unsigned int w_reg_bit_vc(unsigned int adr, unsigned int val,
			      unsigned int start, unsigned int len);
void send_event_to_sv(unsigned int event, unsigned int val);
void stimulus_wait_event_done(unsigned int event);
void set_int_type(unsigned int irq_vec, unsigned int type);
void set_int_enable(unsigned int irq_vec);
void stimulus_finish_pass(void);
void stimulus_finish_fail(unsigned int ty);

int aml_read_dpss(unsigned int reg);
void aml_write_dpss(unsigned int reg, unsigned int val);

void aml_dpss_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val);
int aml_read_vcbus(unsigned int reg);
void aml_write_vcbus(unsigned int reg, unsigned int val);
void aml_vcbus_update_bits(unsigned int reg,
			   unsigned int mask,
			   unsigned int val);

#ifdef ARY_IRQ_2SRC_EN
#define USE_TWO_DATA	(1)
#else
#ifdef ARY_IRQ_CASE_115
#define USE_TWO_DATA	(1)
#else
#define USE_ONE_DATA	(1)
#endif
#endif
extern unsigned int dpss_tnr_en;
extern const struct reg_acc t6w_dpss_reg_acc;
extern unsigned int dpss_nr_debug;
extern unsigned int dpss_tnr_en;
extern unsigned int dpss_snr_en;
extern unsigned int dpss_dm_en;
extern unsigned int dpss_dblk_en;
extern unsigned int dpss_dct_en;
extern unsigned int dpss_cfr_en;
extern unsigned int dpss_cue_en;
extern unsigned int dpss_pq_en;
extern unsigned int dpss_frc_bypass;
extern unsigned int dpss_me_en;
extern unsigned int dpss_xlr_en;
extern struct PRM_DPSS_INP prm_dpss_inp;
extern unsigned int dpss_xlr_side_en;
extern unsigned int dpss_afrc_head;
extern unsigned int dpss_afrc_y;
extern unsigned int dpss_afrc_c;
extern unsigned int afrc_dict_mode_y;
extern unsigned int afrc_dict_mode_c;
extern unsigned int dpss_en_lcevc;
extern unsigned int dpss_di_debug;
extern unsigned int dpss_force_nr_debug;
extern unsigned int pq_update;
extern unsigned int nr_aepe_buf_num_dbg;
extern unsigned int dpss_force_pq;
#ifdef USE_ONE_DATA
extern struct PRM_DPSS_TOP *prm_dpss_top;
extern struct PRM_DPSS_DAE *prm_dpss_dae;
extern struct PRM_DPSS_DPE *prm_dpss_dpe;
extern struct PRM_DPSS_TOP prm_dpss_top_di;
extern struct PRM_DPSS_DAE prm_dpss_dae_di;
extern struct PRM_DPSS_DPE prm_dpss_dpe_di;

extern struct PRM_DPSS_TOP *prm_dpss_top2;
extern struct PRM_DPSS_DAE *prm_dpss_dae2;
extern struct PRM_DPSS_DPE *prm_dpss_dpe2;
extern u32 dpss_last_dpe_mode; //ary 07-04
extern u32 dpss_last_dpe_src;	//ary 07-04

#endif
extern struct PRM_INTF_TYPE g_inp_yuv_rdmif;

#ifdef USE_TWO_DATA
extern struct PRM_DPSS_TOP prm_dpss_top[2];
extern struct PRM_DPSS_DAE prm_dpss_dae[2];
extern struct PRM_DPSS_DPE prm_dpss_dpe[2];

#endif
extern struct PRM_DPSS_TOP prm_dpss_top_1;
extern struct PRM_DPSS_INP prm_dpss_inp_1;
extern struct PRM_DPSS_DAE prm_dpss_dae_1;
extern struct PRM_DPSS_DPE prm_dpss_dpe_1;
//extern unsigned int g_dpss_tst_case;

extern struct PRM_DPSS_SIZE prm_size; //in hw_cfg_dpss_dae
extern struct PRM_INTF_TYPE dae_yuv_intf;	//hw_cfg_dpss_dae
extern struct PRM_DPSS_SIZE prm_size_di; //in hw_cfg_dpss_dae
extern struct PRM_INTF_TYPE dae_yuv_intf_di;	//hw_cfg_dpss_da

//extern struct PRM_INTF_TYPE g_pix_rmif; //ary used in hw_cfg_dpss_dpe_intf
extern struct VFCD_t g_vfcd0;
extern struct VFCD_t g_vfcd1;
extern struct PRM_INTF_TYPE g_pix_rmif2;
extern struct PRM_INTF_TYPE g_pix_rmif3;
extern struct PRM_INTF_TYPE g_pix_rmif4;
extern struct PRM_INTF_TYPE g_pix_wmif0;
extern struct PRM_INTF_TYPE g_pix_wmif1;
extern struct PRM_INTF_TYPE g_pix_wmif2;
extern struct PRM_INTF_TYPE g_ds_wmif0;
extern struct AA_PPS_TOP_TYPE g_nr_pps_cfg;
extern unsigned int nr_bypass_test;

bool tst_dbg_reg(void);

extern bool dpss_slt_mode;
extern bool dpss_en_hdr;

//exnern void tst_case_00(void);		//SRC0->INP->NR
//tbc:
extern u32 fnr_dpe_obuf_wrpt;
extern u32 fnr_src_ibuf_num;

extern u32 src1_dpe_obuf_wrpt;

extern u32 fnr_dae_obuf_wrpt;
extern u32 fnr_dae_ibuf_wrpt;
extern u32 fnr_dae_ibuf_rdpt;
extern u32 fnr_dae_ibuf[16][2];
extern u32 fnr_dpe_obuf_num;

extern u32 fnr_dpe_obuf_free_idx;

extern   u32 reg_inp_ibuf_num;
//(src0_nrdi_frc_en == 1) ? ((rd(DPSS_FBUF_BUF_CTRL)>>16)&0x1f) :
// (rd(FRC_AUTO_MODE_BUFF_NUM) &0x1f);
extern   u32 reg_inp_obuf_num;
extern   u32 reg_fnr_dae_ibuf_num;
extern   u32 reg_fnr_dae_obuf_num; //todo
extern   u32 reg_fnr_dpe_ibuf_num; //todo
extern   u32 reg_fnr_dpe_obuf_num;
extern   u32 reg_frc_dae_ibuf_num;
extern   u32 reg_frc_dae_obuf_num;
extern   u32 reg_frc_dpe_ibuf_num;
extern   u32 reg_frc_dpe_obuf_num;

extern   u32 g_dpss_di_ignore_num;
extern   u32 g_dpss_nr_ignore_num;
extern   u32 g_dpss_inp_frm_cnt_adj;
extern   u32 g_dpss_dae_frm_cnt_src0_adj;
extern   u32 g_dpss_dae_frm_cnt_src1_adj;
extern   u32 g_dpss_dae_frm_cnt_src2_adj;
extern   u32 g_dpss_dpe_nr_frm_cnt_adj;
extern   u32 g_dpss_dpe_di_frm_cnt_adj;
extern   u32 dpss_fnr_vpp_link;

extern  u32 inp_ibuf_level;
extern  u32 inp_obuf_level;
extern  u32 frc_dae_ibuf_level;
extern  u32 frc_dae_obuf_level;

extern  u32 fnr_dae_ibuf_level;
extern  u32 fnr_dae_obuf_level;
extern  u32 fnr_dpe_ibuf_level;
extern  u32 fnr_dpe_obuf_level;

extern  u32 viu_frm_idx;
extern  u32 src_frm_idx;
extern  u32 inp_frm_idx;
extern  u32 frc_dae_frm_idx;
extern  u32 fnr_dae_frm_idx;
extern  u32 fnr_dpe_frm_idx;
extern  u32 dae0_frm_rst_cnt;
extern  u32 dae1_frm_rst_cnt;
extern  u32 dae2_frm_rst_cnt;	//version:0312 add

extern  u32 dae_cur_phase;
extern  u32 dae_pre_phase;
extern  u32 dpe_cur_phase;
extern  u32 dpe_pre_phase;
extern  u32 dpe_pre_idx;
extern  u32 dpe_cur_idx;
extern  u32 mc_proc_phs;
extern  enum DPSS_FILM_MODE pre_film_mode;

extern  u32 inp_din_idx;
extern  u32 inp_proc_idx;
extern  u64 inp_fifo[16];

extern  u32 frc_dae_din_idx;
extern  u32 frc_dae_proc_idx;
extern  u64 frc_dae_fifo[8];

extern  u32 fnr_dae_din_idx;
extern  u64 fnr_dae_fifo[8];

extern  u32 frc_dpe_din_idx;
extern  u32 frc_dpe_proc_idx;
extern  u64 frc_dpe_fifo[8];

extern  u32 fnr_dpe_din_idx;
extern  u32 fnr_dpe_proc_idx;
extern  u64 fnr_dpe_fifo[8];
extern  u64 fnr_dpe_idx_fifo[8];
extern  u32 fnr_dpe_idx_rdpt;
extern  u32 fnr_dpe_idx_wrpt;
extern  u32 cfg_dae0_level;
extern  u32 cfg_dae1_level;
extern bool dpss_dae_done[10];
extern unsigned int dpss_wait_cnt1;

//add by ary:
extern unsigned int g_dae_frc_loop;
extern unsigned int g_dae_index;
extern unsigned int g_dpe_index;
extern unsigned int g_dae_frc_cnt;
extern unsigned int g_dae_nr_cnt;

extern u32 dpss_dae_frm_cnt_src1;
extern u32 src0_frm_idx_cnt;
extern u32 dpss_inp_frm_cnt;
extern u32 dpss_dae_frm_cnt_src0;
extern u32 dpss_dae_frm_cnt_src1;
extern u32 dpss_dae_frm_cnt_src2;
extern u32 dpss_dpe_nr_frm_cnt;
extern u32 dpss_dpe_di_frm_cnt;
extern u32 dpss_rls1_frm_cnt;
extern u32 src0_frm_idx_cnt;
extern u32 src1_frm_idx_cnt;
extern u32 dae_rd_less_one;
extern bool     frc_vpp_link_obuf_rdy;
extern u32 frc_ocnt_status;
extern u32 mc_need_match;
extern u32 mc_repeat_frm; //1114
extern u32 g_mc_phase;
extern unsigned int dpss_dv_en;
extern unsigned int dpss_en_afbc;
extern unsigned int dpss_en_afbc_force;
extern unsigned int dpss_pps_check;
extern unsigned int pps_out_w;
extern unsigned int pps_out_h;
extern bool dpss_en_hdr;

void dbg_dpss_prm_top_setting(struct PRM_DPSS_TOP *prm_top);
extern unsigned int dpss_w_mode;
extern bool bbd_init;
extern unsigned int dpss_reset_ctrl;

extern struct VFCE_TYPE g_vfce0;
extern bool nr_4k1k_en;

//frc lukang add 2025-5-20
extern enum DPSS_FILM_MODE det_film_mode;
extern u32 det_film_phs;
extern u32 det_filmmode_chg;
extern u32 mc_undone_cnt;
extern unsigned int dpss_dbg_step;//tmp
extern int enable_mc_link;
extern struct display_queue display_buf_q;

extern unsigned int dpss_t6x_direct;
extern unsigned int dpss_en_dct;
extern bool dpss_dbg_en_logo;
extern unsigned int dpss_light_chg;
extern unsigned int dpss_ei_sel;
void dpss_hdr_update_dpe(bool en, void *data, unsigned int pos);

#endif //__SYS_DEF_H__
