/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_S_H__
#define __DPSS_S_H__

#include <linux/kfifo.h>
#include <linux/amlogic/media/di/dpss_interface.h>

/**************************************/
#define FIFO_256_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    256,		\
	    GFP_KERNEL)

#define FIFO_128_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    128,		\
	    GFP_KERNEL)

#define FIFO_64_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    64,			\
	    GFP_KERNEL)

#define FIFO_32_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    32,			\
	    GFP_KERNEL)

#define FIFO_16_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    16,			\
	    GFP_KERNEL)

#define FIFO_8_ALLOC(fifo)	\
kfifo_alloc(fifo,		\
	    8,			\
	    GFP_KERNEL)

enum EDPSS_Q_CH_ID {
	/* frame interface */
	EDPSS_Q_CH_IDLE_NR,	// in, idle nr -> in
	EDPSS_Q_CH_IDLE_FRC,	//no use
	EDPSS_Q_CH_IN,		// parser: in -> nr doing
	EDPSS_Q_CH_RD,		//ready to display
	EDPSS_Q_CH_IN_RECYCLE,	//ready to back to pre
	EDPSS_Q_CH_BACK,
	EDPSS_Q_CH_V_IDLE,
	EDPSS_Q_CH_V_RECYCLE,
	EDPSS_Q_CH_NR_DOING,	//
	EDPSS_Q_CH_FRC_DOING,	//
	EDPSS_Q_CH_FRONT_IDLE,	// for di_front out idle
	EDPSS_Q_CH_FRONT_O,	// for di_front out buffer
	EDPSS_Q_CH_NUB
};

/*make sure below's gap*/
#define EDPSS_HD_ID_NR_BEGIN (0)
#define EDPSS_HD_ID_BYPASS_BEGIN (0x40)
#define EDPSS_HD_ID_FRC_BEGIN (0x80)

enum EDPSS_Q_HW_ID {
	/* hw proce interface */
	EDPSS_Q_HW_INP_IN,
	EDPSS_Q_HW_INP_RD,
	EDPSS_Q_HW_NR_IN,
	EDPSS_Q_HW_NR_RD,
	EDPSS_Q_HW_FRC_IN,
	EDPSS_Q_HW_FRC_RD,
	EDPSS_Q_HW_NUB
};

#ifdef FUNC_EN_CFGX
/* ************************************** */
/* *************** cfg x *************** */
/* ************************************** */
/*also see dpss_cfgx_ctr */
enum EDPSS_CFGX_IDX {
	/* cfg channel x */
	EDPSS_CFGX_BEGIN,
	EDPSS_CFGX_BYPASS_ALL,	/*bypass_all */
	EDPSS_CFGX_END,

	/* debug cfg x */
	EDPSS_DBG_CFGX_BEGIN,
	EDPSS_DBG_CFGX_IDX_VFM_IN,
	EDPSS_DBG_CFGX_IDX_VFM_OT,
	EDPSS_DBG_CFGX_END,
};

#define DPSS_CFGX_NUB	(EDPSS_DBG_CFGX_END - EDPSS_CFGX_BEGIN + 1)
#endif				/* FUNC_EN_CFGX */

#ifdef FUNC_DBG_REGISTER_CAT
/*************************
 *debug register:
 *************************/
#define DPSS_SIZE_REG_LOG	(1024)
#define DPSS_LAB_MOD		(0xf001)
/*also see: dbg_mode_name*/
enum EDPSS_DBG_MOD {
	EPSS_DBG_MOD_REGB,	/* 0 */
	EPSS_DBG_MOD_REGE,	/* 1 */

	EDPSS_DBG_MOD_END,
};

enum EDPSS_LOG_TYPE {
	EDPSS_LOG_TYPE_ALL = 1,
	EDPSS_LOG_TYPE_REG,
	EDPSS_LOG_TYPE_MOD,
	EDPSS_LOG_TYPE_REG_M,
	EDPSS_LOG_TYPE_REG_M_B = 0x10,	//begin
	EDPSS_LOG_TYPE_REG_M_1,
	EDPSS_LOG_TYPE_REG_M_2,
	EDPSS_LOG_TYPE_REG_M_E	//end 0x13
};

struct dpss_dbg_reg {
	unsigned char type;
	unsigned int addr;
	unsigned int val;
	unsigned int st_bit:8, b_w:8, r_w:1, vc:1, res:14;
};

struct dpss_dbg_mod {
	unsigned int label;	/*0xf001: mean dbg mode */
	unsigned int ch:8, mod:8, res:16;
	unsigned int cnt;	/*frame cnt */
	unsigned int diff_us;
};

union udbg_data {
	struct dpss_dbg_reg reg;
	struct dpss_dbg_mod mod;
};

struct dpss_dbg_reg_log {
	bool en;
	bool en_reg;
	bool en_reg_m;		//monitor special register
	bool en_mod;
	bool en_all;
	bool en_notoverwrite;
	unsigned int reg_m[4];	//monitor special register

	union udbg_data log[DPSS_SIZE_REG_LOG];
	unsigned int pos;
	unsigned int wsize;
	bool overflow;
};

struct dpss_dbg_data {
	unsigned int vframe_type;	/*use for type info */
	unsigned int cur_channel;
	struct dpss_dbg_reg_log reg_log;
};

#endif				/* FUNC_DBG_REGISTER_CAT */
/**************************************/
/* */
struct dpss_dev_vfram_t {	//dev_vfram_t
	const char *name;
	/*receiver: */
	struct vframe_receiver_s di_vf_recv;
	/*provider: */
	struct vframe_provider_s di_vf_prov;

	unsigned int index;
	/*status: */
	bool bypass_complete;
	bool reg;		/*use this for vframe reg/unreg */
	/*	unsigned int data[32]; *//*null */

};

struct dpss_fifo_s {
	struct kfifo f;
	bool flg_alloc;
	unsigned char id;
	unsigned char size;	//dbg only
	const char *name;
};

struct dpss_h_nr_mng_s {
	struct vframe_s *vf;
	bool used;
	bool active;
	unsigned char r_b_idx;
	unsigned char vf_idx;	//maybe this is better than *vfm
};

struct dpss_nr_i_s {
	void *lk_parent;
	void *lk_grd_parent;
	unsigned char idx;
	unsigned char nr_buf_idx; //add playback fallback check for frc
	struct vframe_s *in_vfm;
	struct vframe_s *front_vfm; //for di_front
	struct dpss_sub_vf_s sub_vf_in;

	struct dpss_in_vf_info info_in;	//
	struct dpss_out_vf_info info_out;	//
	unsigned int work_mode_v;
	unsigned int chg;	//for parser
};

struct dpss_frc_i_s {
	void *lk_parent;
	void *lk_grd_parent;
	unsigned char idx;
	struct vframe_s *in_vfm;
	struct dpss_sub_vf_s sub_vf_in;

	struct dpss_in_vf_info info_in;	//
	struct dpss_out_vf_info info_out;	//
	unsigned int work_mode_v;
	unsigned int chg;	//for parser
	unsigned char inp_idx;
};

struct dpss_lk1_s {
	void *lk_up;
	void *lk_dn;		//maybe all vfm link to same
	void *lk_info;		//maybe one link for one vframe;
	unsigned char idx_q;
	unsigned char idx_t;	//lk type? for internal or out
	unsigned int code_up;
	unsigned int code_dn;
	unsigned int code_if;
};

//#if 1 //move to dpss_s.h
struct dpss_dd_s {
	struct dpss_buf_sml_s hd_sml_info;
	struct dpss_buf_sml_s uhd_sml_info;	//?
	struct dpss_buf_rdma_s rdma_info;
	struct dpss_mm_p_cnt_out_s fd_nr_info;	//4k
	struct dpss_mm_p_cnt_out_s hd_nr_info;
	struct dpss_mm_p_cnt_out_s hd_lc_info;	//local
	struct afbcd_buf_inf_s hd_afbc_info;
	struct afbcd_buf_inf_s fd_afbc_info;	//4k
	struct afrc_buf_inf_s hd_afrc_info;
	struct afrc_buf_inf_s fd_afrc_info; //4k
	unsigned char afrc_bs_y;
	unsigned char afrc_bs_c;
};

//...buffer end ....
struct dpss_vf_nr_s2 {
	struct dpss_vf_s1 *parent_dpss;
	struct vframe_s *grandpa_vfm;

	unsigned char ch;
	unsigned int dpss_id;
	unsigned int flg_process;	//BIT 0: NR, BIT1:DI, BIT2,CP, BIT3,dv;BIT4,dct....
	/*flg_process is 0 : bypass */
	unsigned int flg_link_buf;	//bit0 have dw buffer,
	//bit 1: have full-solution buffer ;
	//bit 2: have full-solution scart buffer;
#ifdef MOV	//no used tmp
	struct dpss_vf_s_buf_s *buf_dw;
	struct dpss_vf_s_buf_s *buf_full;
	struct dpss_vf_s_buf_s *buf_sct;
#endif
};

struct dpss_vf_frc_s2 {
	struct dpss_vf_s1 *parent_dpss;
	struct vframe_s *grandpa_vfm;
	struct vframe_s *in_vfm_crr;
	struct vframe_s *in_vfm_pre1;
	struct vframe_s *in_vfm_pre2;
	struct vframe_s *in_vfm_pre3;
	unsigned char ch;
	unsigned int dpss_id_crr;
	unsigned int dpss_id_pre1;
	unsigned int dpss_id_pre2;
	unsigned int dpss_id_pre3;
};

struct dpss_vf_s1 {
	unsigned int owner_infor;	//DPSS_VF_OWNER_NR ...
	struct vframe_s *parent_vfm;	//reciprocal link vfm->dpss_data
	struct dpss_hd_s *hd;	//link to hd for que

	unsigned char ch;
	struct vframe_s *in_vfm;
	struct dpss_sub_vf_s sub_vf_in;

	struct dpss_in_vf_info info_in;	//
	struct dpss_out_vf_info info_out;	//
	unsigned int work_mode_v;
	unsigned int chg;	//for parser
	struct dpss_vf_nr_s2 *vf_nr;	//when owner_infor is DPSS_VF_OWNER_NR
	struct dpss_vf_frc_s2 *vf_frc;	//when owener_info is DPSS_VF_OWNER_FRC
};

//#endif

struct dpss_ch_d_s1 {
	/*use dpss_lk_s replace dpss_vf_s1 */
	struct dpss_lk1_s lk_bp[DPSS_VFM_IN_NUB];
	struct dpss_lk1_s lk_nr[DPSS_VFM_IN_NUB];
	struct dpss_nr_i_s nr_i[DPSS_VFM_IN_NUB];
	//----------------------------------------
	struct dpss_lk1_s lk_frc[DPSS_VFM_IN_NUB];
	struct dpss_frc_i_s frc_i[DPSS_VFM_IN_NUB];

	//----------------------------------------
	/* other vfm and hd */
//      struct dpss_vf_s1 vf_bp[DPSS_VFM_IN_NUB];
	struct vframe_s *vfm_bp[DPSS_VFM_IN_NUB];

	/* input and nr */
//      struct dpss_hd_s hd_nr[DPSS_VFM_IN_NUB];
///     struct dpss_vf_s1 vf_s1_nr[DPSS_VFM_IN_NUB];
	struct dpss_vf_nr_s2 vf_s2_nr[DPSS_VFM_IN_NUB];
	struct vframe_s vfm_nr[DPSS_VFM_IN_NUB];
	bool vfm_in_act[DPSS_VFM_IN_NUB];	//dbg only
	/* frc */
	struct dpss_hd_s hd_frc[DPSS_VFM_FRC_NUB];
	/* hd_frc: id is from EDPSS_HD_ID_FRC_BEGIN to share q with input */
	struct dpss_vf_s1 vf_s1_frc[DPSS_VFM_FRC_NUB];

	struct dpss_vf_frc_s2 vf_s2_frc[DPSS_VFM_FRC_NUB];
	struct vframe_s vfm_frc[DPSS_VFM_FRC_NUB];
	struct vframe_s *vfm_in[DPSS_VFM_IN_NUB];	//dbg only
	struct vframe_s *vfm_dis;	//dbg only
	struct dpss_h_nr_mng_s h_nr_mng[DPSS_VFM_IN_NUB];	//use doing

	/*di front*/
	struct vframe_s *cfg_vfm[DPSS_CFG_NUM];
	unsigned int cfg_cnt;

	bool vfm_dis_en;	//dbg only
	/* q_ch */
	struct dpss_fifo_s q_ch[EDPSS_Q_CH_NUB];

	struct dpss_vinfo_s vf_st_lst;
	struct dpss_vinfo_s vf_st_crr;
	unsigned int cnt_pre_get;
	unsigned int cnt_in;
	unsigned int cnt_pre_put;
	unsigned int cnt_pst_get;
	unsigned int cnt_pst_put;
	unsigned int cnt_parser_in;
	unsigned int cnt_h_in;
	bool state_bypass;
	bool state_nr_bypass; //@0808
	bool is_i;
	bool is_field; //for interlace
	bool is_top_first; //for interlace
	bool is_afbcd; //for input afbc
	bool is_afrcd; //for input afrc
	bool nr_cp_mode;
	bool mode_drct; // from t6x
	// unsigned int fmt;
	bool frc_ds_scale_en;
	bool need_dis_light_chg;

	bool is_10bit; //for input
	bool proc_as_i;
	bool frc_link;
	bool init;
	bool is_dd;
	bool en_di_src; //only for di + frc tmp
	bool is_secure;
	bool fmt444_comb;
	unsigned int plane;
	unsigned int video_mode; /* 0:420 1:422 2:444 */
	unsigned int fmt;
	unsigned int cnt_wait_nr_free;
	unsigned int cnt_wait_frc_free;
	unsigned int cnt_wait_input;
	unsigned char h_dae_vf_idx[DPSS_VFM_IN_NUB];
	unsigned char mc_skip_mode;
	unsigned int out_mode;
	unsigned char afrc_bs_y;
	unsigned char afrc_bs_c;
	bool status_bypass;
	unsigned int bypass_rs;
	unsigned int new_w_mode;
	unsigned int w_mode_2; // DPSS_DIRECT_MODE
	unsigned char idx_done;	//
	unsigned char idx_start;
	unsigned char idx_hd; //
	unsigned char mem_err;
	bool nr_reset; //add for light chg
	bool di_front;
};

enum DPPS_SML_BUF_IDX {
	DPPS_SML_BUF_IDX_MIX,
	DPPS_SML_BUF_IDX_MV,
	DPPS_SML_BUF_IDX_MEALP,	//
	DPPS_SML_BUF_IDX_MTN,
	DPPS_SML_BUF_IDX_BLK_INFO,
	DPPS_SML_BUF_IDX_DMSQ,
	DPPS_SML_BUF_IDX_DCT_GRAID,
	DPPS_SML_BUF_IDX_DCT_Y,	//tmp
	DPPS_SML_BUF_IDX_DCT_C,	//tmp
	DPPS_SML_BUF_IDX_TFBC,
	DPPS_SML_BUF_IDX_GRAD_H,
	DPPS_SML_BUF_IDX_GRAD_V,
	DPPS_SML_BUF_IDX_RO1,
	DPPS_SML_BUF_IDX_RO2,
//	DPSS_SML_AFBC,
//	DPSS_SML_AFRC_C, //add for afrc
	// FRC
	DPPS_SML_BUF_IDX_INP_MBUF0,
	DPPS_SML_BUF_IDX_INP_MBUF1,
	DPPS_SML_BUF_IDX_ME_MBUF,
	DPSS_SML_BUF_IDX_ME_NC_UNI_MV,
	DPSS_SML_BUF_IDX_ME_CN_UNI_MV,
	DPSS_SML_BUF_IDX_ME_PC_PHS_MV,
	DPSS_SML_BUF_IDX_LOGO_IIR_BUF,
	DPSS_SML_BUF_IDX_LOGO_SSC_BUF,
	DPSS_SML_BUF_IDX_BLK_LOGO,
	DPSS_SML_BUF_IDX_PIX_LOGO,
	DPSS_SML_BUF_IDX_VP_MC_MV,
	DPSS_SML_BUF_IDX_VP_MC_LOGO,
	DPSS_SML_BUF_IDX_FRC_MERO,
	DPPS_SML_BUF_IDX_NUB,
};

enum DPSS_B_ST {
	DPSS_B_ST_IDEL,
	DPSS_B_ST_RD_DPSS,
	DPSS_B_ST_DPSS_DONE,
	DPSS_B_ST_S_GET,
};

/********************************
 * must ref to :
 *	module_support_tab
 *
 ********************************/
enum EDPSS_W_MODE_B {
	EDPSS_W_MODE_B_BP, //bypass mode //this is special //only di/nr can return this flg;
	EDPSS_W_MODE_B_NR,
	EDPSS_W_MODE_B_DI,
	EDPSS_W_MODE_B_DCT,
	EDPSS_W_MODE_B_HDR,
	EDPSS_W_MODE_B_DDD,
	EDPSS_W_MODE_B_FRC,
	EDPSS_W_MODE_B_LEVC,	//end, if change need check
};

#define D_W_B(x) (1 << (EDPSS_W_MODE_B_##x))

#ifdef MOV
#define DPSS_W_MODE_T_FULL	(DPSS_W_MODE_NR	|	\
				DPSS_W_MODE_DI	|	\
				DPSS_W_MODE_DCT	|	\
				DPSS_W_MODE_FRC |	\
				DPSS_W_MODE_HDR |	\
				DPSS_W_MODE_DDD)
#endif
#define DPSS_W_MODE_T_FULL	(D_W_B(NR)	|	\
				D_W_B(DI)	|	\
				D_W_B(DCT)	|	\
				D_W_B(HDR)	|	\
				D_W_B(DDD)	|	\
				D_W_B(FRC)	|	\
				D_W_B(LEVC))
struct dpss_ch_c_s1 {
	unsigned int check_st1;	//
	struct mutex ch_lock;	//block issue such as create...
	bool reg_s1;		//set in create /destroy
	bool reg_s2;		//set in thread
	bool unreg_s1;		//have set unreg step 1
	bool etype;		//0 is vfm path, 1 is ins path
	unsigned int ch;
	//vfm path only:
	struct dpss_dev_vfram_t dvfm;	/* for vfm prob fix */

	atomic_t ch_state;
	atomic_t trig_reg;
	atomic_t trig_unreg;
	atomic_t unreg_proc; //0918
	bool init_flg;		/*init_flag no use ? */
	bool trig_unreg_l;	//no use ?
	struct dpss_init_parm parm;
	bool sub_act_flg;
	bool in_flg;
	bool bypass_state;
	unsigned int cnt_in;
	unsigned int cnt_proce;
	unsigned int in_b_nub;
	unsigned int o_b_nub;
	unsigned char in_state[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	//4 bit for one buffer  0: free, 1: rd for process, 2. dpss done
	unsigned char o_state[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned char o_afbc; //1: for afbce, 2: for afrc; 07-07
	//4 bit for one buffer
//      unsigned int o_crr;
	unsigned int cnt_dpe_rd_cp;
	unsigned long mem_start;
	unsigned int mem_size;

	struct dentry *dbg_rootx;	/*dbg_fs */
#ifdef FUNC_EN_CFGX
	unsigned char cfgx_en[DPSS_CFGX_NUB];
#endif
	//buf:
	unsigned char alloc_cnt_blk_nr;
	unsigned char alloc_cnt_blk_lc;
	unsigned char alloc_cnt_blk_di2pps;
	unsigned char alloc_cnt_blk_dw; // 0916
	unsigned long addr_sml_base;
	unsigned long addr_dw_base;
	unsigned long addr_afbc_hd_base;
	unsigned long addr_afbc_hd_b_base; //0916
	unsigned long addr_afbc_tab_base;
	unsigned long addr_rdma_base;
	unsigned int *addr_rdma_v;
	unsigned long addr_sml[DPSS_SML_NUB][DPPS_SML_BUF_IDX_NUB];
	unsigned long addr_nr[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_nr_uv[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_lc[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_lc_uv[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_di2pps[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_di2pps_uv[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_dw[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_dwuv[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_rdma[DPSS_RDMA_RAM_NUB];
	unsigned long addr_afbc_tab[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned long addr_afbc_info[DPSS_HW_LOOP_IN_OUT_BUF_NUB * 2];
	void *vaddr_ro1;
	void *vaddr_ro2;
	void *vaddr_ro3;
	void *vaddr_ro4;
	void *vaddr_ro5;
	unsigned long addr_afbc_tab_c[DPSS_HW_LOOP_IN_OUT_BUF_NUB]; //for afrc
	unsigned long addr_afbc_info_c[DPSS_HW_LOOP_IN_OUT_BUF_NUB * 2]; //for afrc
	unsigned int crc_y[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned int crc_c[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_buf_sml_s *sml_info;
	struct dpss_buf_rdma_s *rdma_info;
	struct dpss_cfg_blki_s blki_sml;	//tmp for alloc
	struct dpss_cfg_blki_s blki_dw;	//09-15
	struct dpss_cfg_blki_s blki_nr;	//tmp for alloc
	struct dpss_cfg_blki_s blki_lc;	//tmp for alloc
	struct dpss_cfg_blki_s blki_di2pps;	//tmp for alloc
	struct dpss_cfg_blki_s blki_rdma;	//tmp for alloc
	struct dpss_cfg_blki_s blki_afbc_hd; //0627
	struct dpss_cfg_blki_s blki_afbc_hd_b; //0916
	struct dpss_cfg_blki_s blki_afbc_tab; //0916

	struct dpss_hd_s hd_nr[DPSS_HW_LOOP_IN_OUT_BUF_NUB];	//tmp for alloc
	struct dpss_blk_s blk_r_sml_nr;	//tmp for alloc
	struct dpss_blk_s blk_r_dw[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s blk_r_nr[DPSS_HW_LOOP_IN_OUT_BUF_NUB];	//tmp for alloc
	struct dpss_blk_s blk_r_lc[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s blk_r_di2pps[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s blk_r_rdma;	//2025-01-17
	struct dpss_blk_s blk_r_afbc_hd;
	struct dpss_blk_s blk_r_afbc_hd_b; //0916
	struct dpss_blk_s blk_r_afbc_tab;

	struct dpss_blk_s *blk_sml_nr;
	struct dpss_blk_s *blk_dw[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s *blk_nr[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s *blk_lc[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s *blk_di2pps[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	struct dpss_blk_s *blk_rdma;
	//struct dpss_buf_nr_s buf_nr[DPSS_HW_LOOP_IN_OUT_BUF_NUB];
	unsigned int work_mode_cfg;	//set when set up play path
	unsigned int work_mode_curr;	//current signal w type
	bool last_chg;		//set in parser

	struct dpss_rdma_pre_s pre_dae_dpss;
	struct dpss_rdma_pre_s pre_dae_vc;
	struct dpss_rdma_pre_s pre_dpe_dpss;
	struct dpss_rdma_pre_s pre_dpe_vc;
	struct dpss_rdma_pre_s pre_tmp_a[2];	//tmp
	struct dpss_rdma_pre_s pre_tmp_b[5];
	int rdma_handle_dae;
	int rdma_handle_dpe[2];
	struct rdma_op_s op_dae;
	struct rdma_op_s op_dpe[2];
	struct dpss_rdma_arg_s arg_dae;
	struct dpss_rdma_arg_s arg_dpe[2];
	unsigned int rdma_err;
	//vpp rdma multi ch:
	int vpp_rdma_handle_dpe[2];	//0 for big table, 1 for small
	struct rdma_op_s vpp_dpe_op[2];
	enum DPSS_BUF_STATE buf_status;
	unsigned int user_case;
	unsigned int case_id; // g_dpss_tst_case
	struct PRM_DPSS_TOP prm_top;
	struct PRM_DPSS_DAE prm_dae;
	struct PRM_DPSS_DPE prm_dpe;
	struct PRM_DPSS_SIZE prm_size; //in hw_cfg_dpss_dae
	struct PRM_INTF_TYPE dae_yuv_intf;	//hw_cfg_dpss_dae
	unsigned int in_cnt;	//g_dpss_in_cnt
	unsigned int out_cnt;	//g_dpss_out_cnt
	unsigned int in_total_cnt; //add 1002
	unsigned int out_total_cnt; //add 1002
	unsigned int disp_cnt;  //g_dpss_disp_cnt
	struct vframe_s *vfm_last;
	bool en_dct;
	bool en_tfbc;
	bool en_dw;
	bool en_frc;
	unsigned char num_lc; //
	unsigned char num_aepe;
	unsigned char num_nr_wrpt;
	unsigned char num_pq_buf;
	//1080p:bit 0: afbc; bit 1: afrc; bit 2: mif
	//4k: bit 4: afbc; bit 5: afrc; bit 6: mif
	unsigned char mem_support;
	//mem support 4k ?
	//mem support i ?
	//other ?
	bool support_4k;
	bool support_i;
	bool support_frc;

};

struct prm_dpss_input {
	unsigned int index;
	unsigned int input_in_index;
	unsigned int input_done_index;
	unsigned int dropped_count;
	bool used;
	bool input_done;
	bool is_key_frame;
	bool dpe_done;
	bool is_i;
	unsigned int buf_index;
};

#define DPSS_INP_LIST_COUNT  32

struct dpss_input_q {
	struct prm_dpss_input input[DPSS_INP_LIST_COUNT];
	DECLARE_KFIFO(input_q, struct prm_dpss_input *, DPSS_INP_LIST_COUNT);
	unsigned int input_frame_count;
	unsigned int dpe_done_count;
	unsigned int output_idx_last;
	unsigned int total_drop_count;
	bool wait_hw_finish;
	bool wait_sw_finish;
	struct mutex input_q_mutex; //mutex lock
	spinlock_t kfifo_lock;//spinlock lock
};

struct dpss_ch_s {
	struct dpss_ch_c_s1 c;
	struct dpss_ch_d_s1 *d;
	struct dpss_input_q q;
};

struct dpss_hw_s {
	/* q_ch */
	struct dpss_fifo_s q_s0[EDPSS_Q_HW_NUB];
	struct dpss_fifo_s q_s1[EDPSS_Q_HW_NUB];
	unsigned int need_hold; //ary 04012
	unsigned int st_idle;
	unsigned int ch_act;
	unsigned char src_act;
};

struct dpss_data_s {
	const struct dpss_meson_data *mdata;
};

#endif	/*__DPSS_S_H__*/
