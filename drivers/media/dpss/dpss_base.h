/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_BASE_H__
#define __DPSS_BASE_H__

#ifdef RUN_ON_ARM
#include <linux/amlogic/media/vfm/vframe.h>

#endif				/* RUN_ON_ARM */

#include "_base_c.h"
#include "_base_vfm.h"

/*****************************************/
#define IC_ID_T6D		(0x01)
#define IC_ID_T6W		(0x10)
#define IC_ID_T6X		(0x13)

/*****************************************/
//#define C_TST_ON_T6D  (1)

//#define FUNC_EN_COLLING (1)
#define FUNC_EN_IRQ (1)
#define FUNC_EN_CFGX	(1)
#define FUNC_EN_HDR	(1)
#define FUNC_EN_DD	(1)
#define FUNC_EN_PQ	(1)  // frc only set 0
//#define FUNC_EN_VPP_RDMA_M    (1)
#define DPSS_CHECK_ST1	(0x1234567)
#define USE_FRC_LINK_CODE   (1)
// #define USE_FRC_ONLY_CASE   (0)
#define USE_FRC_PRE_VS_RDMA (1)
#define USE_FRC_VS_RDMA (1)
/*****************************************/

/*****************************************/
#define DPSS_CHANNEL_NUB	4
#define DPSS_CHANNEL_MAX	4
#define DPSS_PQ_CHANNEL_NUM	2
#define DPSS_RO_ME_SIZE		160
#define DPSS_RO_INPUT_SIZE	10
#define DPSS_MAIN_CHANNEL	0
#define DPSS_PIP_CHANNEL	1
//#define DI_PLINK_CN_NUB       4
/*****************************************/
#define DPSS_DPS_NUB	(8)	//tmp
#define DPSS_HW_LOOP_IN_OUT_BUF_NUB (8)	//0 ~ 15
#define DPSS_SML_NUB  (8)  // frc only need set to 8
#define DPSS_DMS_NUB  (2)  // dms only need set to 2
#define DPSS_SML_FRC_NUB  (8)   // frc_only =8
#define DPSS_RDMA_RAM_NUB	(10)
#define DPSS_RDMA_RAM_ONE_SIZE	(0x100)
#define DPSS_RDMA_BUF_NUB_MAX	(16)
#define DPSS_CFG_NUM		(10) //di front out
/*****************************************/
//#ifdef USE_FRC_ONLY_CASE
//#define DPSS_VFM_IN_NUB		16
//#define DPSS_VFM_NR_NUB		16
//#define DPSS_VFM_FRC_NUB	16
//#else
#define DPSS_VFM_IN_NUB		20
#define DPSS_VFM_NR_NUB		20
#define DPSS_VFM_FRC_NUB	30
//#endif
/*****************************************/
#define DPSS_QUE_LOCK_RD	(C_BIT0)
#define DPSS_QUE_LOCK_WR	(C_BIT1)
#define DPSS_QUE_LOCK_RD_WR (C_BIT0 | C_BIT1)

/*****************************************/
#define _DPSS_SIZE_QC_4	(4)
#define _DPSS_SIZE_QC_8	(8)
#define _DPSS_SIZE_QC_16	(16)
#define _DPSS_SIZE_QC_32	(32)
#define _DPSS_SIZE_QC_64	(64)
#define _DPSS_SIZE_QC_128	(128)
#define _DPSS_SIZE_QC_256	(256)

#define DPSS_SIZE_NR_FREE	(_DPSS_SIZE_QC_32)	//> DPSS_VFM_IN_NUB
#define DPSS_SIZE_FRC_FREE	(_DPSS_SIZE_QC_32)	//> DPSS_VFM_IN_NUB

#define DPSS_CODE_DATA_MASK	(0x12120000)
#define DPSS_CODE_NR	(DPSS_CODE_DATA_MASK | 0x01)
#define DPSS_CODE_FRC	(DPSS_CODE_DATA_MASK | 0x01)

/*****************************************/
#define DPSS_SLC_THD_L	(300)
#define DPSS_SLC_THD_M	(1920)
#define DPSS_SLC_THD2_M	(960)

/*****************************************/

enum EDPSS_DP_MODE {		//copy from EDPST_MODE
	EDPSS_DP_MODE_NV21_8BIT,	/*NV21 NV12 */
	EDPSS_DP_MODE_422_10BIT_PACK,
	EDPSS_DP_MODE_422_10BIT,
	EDPSS_DP_MODE_422_8BIT,
	EDPSS_DP_MODE_420_10BIT,	//first level
	EDPSS_DP_MODE_422_10BIT_2PACK,
	EDPSS_DP_MODE_422_12BIT_1PACK,
	EDPSS_DP_MODE_422_12BIT_2PACK,
};

enum DPSS_MEM_FROM {
	DPSS_MEM_FROM_CODEC,	//cma mode 4
	DPSS_MEM_FROM_CMA_DPSS,	//1
	DPSS_MEM_FROM_CMA_C,	/* CMA COMMON */
	DPSS_MEM_FROM_DMA,	//
	DPSS_MEM_FROM_CODEC_HD, //09-15
};

/*****************************************/
#define DPSS_VFMT_MASK_ALL		(VIDTYPE_TYPEMASK	|	\
					 VIDTYPE_VIU_NV12	|	\
					 VIDTYPE_VIU_422	|	\
					 VIDTYPE_VIU_444	|	\
					 VIDTYPE_VIU_NV21	|	\
					 VIDTYPE_MVC		|	\
					 VIDTYPE_VIU_SINGLE_PLANE |	\
					 VIDTYPE_VIU_FIELD	|	\
					 VIDTYPE_PIC		|	\
					 VIDTYPE_RGB_444	|	\
					 VIDTYPE_SCATTER	|	\
					 VIDTYPE_COMB_MODE	|	\
					 VIDTYPE_COMPRESS)

#define DPSS_VFM_T_MASK_CHANGE		\
	(VIDTYPE_VIU_422		\
	| VIDTYPE_VIU_SINGLE_PLANE	\
	| VIDTYPE_VIU_444		\
	| VIDTYPE_INTERLACE		\
	| VIDTYPE_COMPRESS		\
	| VIDTYPE_VIU_NV12		\
	| VIDTYPE_VIU_NV21		\
	| VIDTYPE_MVC)

/* di use this to clear di out vfm type */
#define DPSS_VFM_T_MASK_DI_CLEAR		\
	(VIDTYPE_TYPEMASK		\
	| VIDTYPE_VIU_NV12		\
	| VIDTYPE_VIU_422		\
	| VIDTYPE_VIU_SINGLE_PLANE	\
	| VIDTYPE_VIU_FIELD		\
	| VIDTYPE_VIU_NV21		\
	| VIDTYPE_VIU_444		\
	| VIDTYPE_COMPRESS		\
	| VIDTYPE_RGB_444		\
	| VIDTYPE_V4L_EOS)

#define VFM_IS_I_SRC(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM)
#define VFM_IS_FIELD_I_SRC(vftype) ((vftype) & VIDTYPE_INTERLACE_BOTTOM && \
				(vftype) & VIDTYPE_VIU_FIELD)

#define VFM_IS_COMP_MODE(vftype) ((vftype) & VIDTYPE_COMPRESS)
#define VFM_IS_COMP_AFRC(type_ext) ((type_ext) & VIDTYPE_EXT_AFRC_COMPRESS)
#define VFM_IS_FMT_420(type)  (type & VIDTYPE_VIU_NV12 || type & VIDTYPE_VIU_NV21)
#define VFM_IS_FMT_422(type)  (type & VIDTYPE_VIU_422)
#define VFM_IS_FMT_444(type)  (type & VIDTYPE_VIU_444 || type & VIDTYPE_RGB_444)

#define VFM_IS_VDIN_SRC(src) (				\
	((src) == VFRAME_SOURCE_TYPE_TUNER)	||	\
	((src) == VFRAME_SOURCE_TYPE_CVBS)	||	\
	((src) == VFRAME_SOURCE_TYPE_COMP)	||	\
	((src) == VFRAME_SOURCE_TYPE_HDMI))

#define VFM_IS_HDMI_SRC(src) ((src) == VFRAME_SOURCE_TYPE_HDMI)

/*****************************************/
#define DPSS_RUN_FLAG_RUN			0
#define DPSS_RUN_FLAG_PAUSE			1
#define DPSS_RUN_FLAG_STEP			2
#define DPSS_RUN_FLAG_STEP_DONE			3
/*****************************************/
enum TST_CASE_IDX {
	TST_CASE_IDX_0000 = 0,
	TST_CASE_IDX_0001 = 1,	// di src 1
	TST_CASE_IDX_0002 = 2,	//frc only
	TST_CASE_IDX_0107 = 3,	//SRC 0 DI + NR
	TST_CASE_IDX_0102 = 4,	// SRC0 NR+FRC
	TST_CASE_IDX_0157 = 5,	// SRC0 NR+DI+FRC
	//----
	//new:
	TST_CASE_IDX_SRC1_NR = 6,
	TST_CASE_IDX_SRC1_NRDI = 7,
	//----
	TST_CASE_IDX_1000 = 8,	//src0 nr only //ori is 6
	TST_CASE_IDX_1001 = 9,	//src1 di only tbc	//ori is 7
	TST_CASE_IDX_1002 = 10,	////ori is 8
};

enum IRQ_M_ {
	IRQ_MODE_CASE_0_SRC0_NR_DI,
	// case 0 / case 0107 :src is 0, nr only or nr + di
	// irq: dae1, dpe1
	IRQ_MODE_CASE_1_SRC1,
	// case 1:
	// irq: dae2, dpe2
	IRQ_MODE_CASE_2,
	//      TST_CASE_IDX_0002 frc
	// irq:inp, dae0, pre vs ...
	IRQ_MODE_CASE_TBC_1000,
	IRQ_MODE_CASE_MY,
};

/*****************************************/
extern unsigned int dpss_dbg;
#define DPSS_DBG_M_C_ALL		C_BIT3	/* all debug close */
#define DPSS_DBG_LEVEL			(C_BIT0 | C_BIT1)	//is 0~2

//module is begin from bit 4
#define DPSS_M_SYS			C_BIT4 /**/
#define DPSS_M_ITF			C_BIT5 /**/
#define DPSS_M_MEM			C_BIT6 /**/
#define DPSS_M_HW			C_BIT7 /**/
#define DPSS_M_INS			C_BIT8 /**/
#define DPSS_M_RDMA			C_BIT9
#define DPSS_M_CRT			C_BIT10
#define DPSS_M_HW_FRC			C_BIT11
#define DPSS_M_CHECK			C_BIT12
#define DPSS_M_COUNT			C_BIT13
#define DPSS_M_PPS			C_BIT14
#define DPSS_M_IRQ			C_BIT15

#define dbg_md(mark, level, fmt, args ...)		\
	do {						\
		if (dpss_dbg & DPSS_DBG_M_C_ALL)	\
			break;				\
		if ((dpss_dbg & (mark)) &&		\
		    (dpss_dbg & DPSS_DBG_LEVEL) > (level)	\
		    ) {					\
			base_pr("dpss:" fmt, ##args);	\
		}					\
	} while (0)
#ifdef RUN_ON_ARM
#define DBG_ERR(fmt, args ...)		pr_err("dpss:err:" fmt, ##args)
#define DBG_WARN(fmt, args ...)		pr_err("dpss:warn:" fmt, ##args)
#define DBG_INF(fmt, args ...)		pr_info("dpss:" fmt, ##args)
#endif
#define dbg_s0(fmt, args ...)		dbg_md(DPSS_M_SYS, 0, fmt, ##args)
#define dbg_s1(fmt, args ...)		dbg_md(DPSS_M_SYS, 1, fmt, ##args)
#define dbg_s2(fmt, args ...)		dbg_md(DPSS_M_SYS, 2, fmt, ##args)
#define dbg_i0(fmt, args ...)		dbg_md(DPSS_M_ITF, 0, fmt, ##args)
#define dbg_i1(fmt, args ...)		dbg_md(DPSS_M_ITF, 1, fmt, ##args)
#define dbg_i2(fmt, args ...)		dbg_md(DPSS_M_ITF, 2, fmt, ##args)
#define dbg_m0(fmt, args ...)		dbg_md(DPSS_M_MEM, 0, fmt, ##args)
#define dbg_m1(fmt, args ...)		dbg_md(DPSS_M_MEM, 1, fmt, ##args)
#define dbg_m2(fmt, args ...)		dbg_md(DPSS_M_MEM, 2, fmt, ##args)
#define dbg_h0(fmt, args ...)		dbg_md(DPSS_M_HW, 0, fmt, ##args)
#define dbg_h1(fmt, args ...)		dbg_md(DPSS_M_HW, 1, fmt, ##args)
#define dbg_h2(fmt, args ...)		dbg_md(DPSS_M_HW, 2, fmt, ##args)
#define dbg_ins0(fmt, args ...)		dbg_md(DPSS_M_INS, 0, fmt, ##args)
#define dbg_ins1(fmt, args ...)		dbg_md(DPSS_M_INS, 1, fmt, ##args)
#define dbg_ins2(fmt, args ...)		dbg_md(DPSS_M_INS, 2, fmt, ##args)
#define dbg_ct0(fmt, args ...)		dbg_md(DPSS_M_COUNT, 0, fmt, ##args)
#define dbg_ct1(fmt, args ...)		dbg_md(DPSS_M_COUNT, 1, fmt, ##args)
#define dbg_ct2(fmt, args ...)		dbg_md(DPSS_M_COUNT, 2, fmt, ##args)
#define dbg_pps0(fmt, args ...)		dbg_md(DPSS_M_PPS, 0, fmt, ##args)
#define dbg_pps1(fmt, args ...)		dbg_md(DPSS_M_PPS, 1, fmt, ##args)
#define dbg_pps2(fmt, args ...)		dbg_md(DPSS_M_PPS, 2, fmt, ##args)
//rdma:
#define dbg_a0(fmt, args ...)		dbg_md(DPSS_M_RDMA, 0, fmt, ##args)
#define dbg_a1(fmt, args ...)		dbg_md(DPSS_M_RDMA, 1, fmt, ##args)
#define dbg_a2(fmt, args ...)		dbg_md(DPSS_M_RDMA, 2, fmt, ##args)
#define dbg_c0(fmt, args ...)		dbg_md(DPSS_M_CRT, 0, fmt, ##args)
#define dbg_c1(fmt, args ...)		dbg_md(DPSS_M_CRT, 1, fmt, ##args)
#define dbg_c2(fmt, args ...)		dbg_md(DPSS_M_CRT, 2, fmt, ##args)
#define dbg_f0(fmt, args ...)		dbg_md(DPSS_M_HW_FRC, 0, fmt, ##args)
#define dbg_f1(fmt, args ...)		dbg_md(DPSS_M_HW_FRC, 1, fmt, ##args)
#define dbg_f2(fmt, args ...)		dbg_md(DPSS_M_HW_FRC, 2, fmt, ##args)
#define dbg_k0(fmt, args ...)		dbg_md(DPSS_M_CHECK, 0, fmt, ##args)
#define dbg_k1(fmt, args ...)		dbg_md(DPSS_M_CHECK, 1, fmt, ##args)
#define dbg_k2(fmt, args ...)		dbg_md(DPSS_M_CHECK, 2, fmt, ##args)

#define dbg_irq0(fmt, args ...)		dbg_md(DPSS_M_IRQ, 0, fmt, ##args)
#define dbg_irq1(fmt, args ...)		dbg_md(DPSS_M_IRQ, 1, fmt, ##args)
#define dbg_irq2(fmt, args ...)		dbg_md(DPSS_M_IRQ, 2, fmt, ##args)

#define dbg_w2(fmt, args ...)		dbg_md(DPSS_M_HW, 2, fmt, ##args)//HW
//pr_err("dpss:" fmt, ##args)
/* struct */
struct dpss_meson_data {
	const char *name;
	unsigned int ic_id;
	unsigned int support;
};

struct dpss_hd_s {		//c_hd_s
	unsigned int code;
	unsigned char hd_tp;	/* HD_TYPE */
	unsigned char idx;
	void *p;		/* real data */
};

struct dpss_name_s {		//c_name_s
	unsigned char ch;
	unsigned char o_type;
	unsigned char o_id;
	unsigned int reg_cnt;
	char *name;
	void *p;
};

struct dpss_sub_vf_s {		//dsub_vf_s
	/*cp from vframe_s */
	unsigned int index_disp;
	unsigned int type;
	unsigned int type_ext;
	unsigned int canvas0Addr;
	unsigned int canvas1Addr;
	unsigned long compHeadAddr;	//afbc_addr
	unsigned long compBodyAddr;
	unsigned int plane_num;
	struct canvas_config_s canvas0_config[2];
	unsigned int width;
	unsigned int height;
	unsigned int bitdepth;
	unsigned int bitdepth_dw;
	unsigned int flag;
	unsigned int di_flag;	//new
	unsigned int dpss_flag;
	void *decontour_pre;
	unsigned int source_type;

	u32 video_angle;
	u32 signal_type;
	enum tvin_sig_fmt_e sig_fmt;
	enum vframe_signal_fmt_e fmt;
	enum tvin_trans_fmt trans_fmt;
	u32 sei_magic_code;
	void *mem_handle;	/* di use this for struct dim_mm_blk_s */
	u32 duration;
	bool fgs_valid;
	ulong fgs_table_adr;
	u32 dpss_flg;
//      u32 dpss_id;
	/*******************************/
	/* */
	bool is_4k;
	bool is_chg;
	bool is_i;
	bool is_not_support;
	bool is_vdin;
	bool is_bypass;
	bool duration_overflow; //0808
	unsigned int cvs_h;	//tmp;
	struct afrc_frame_info_t afrc_info;
	bool is_4k1k;
};

struct dpss_vinfo_s {		//di_vinfo_s
	/*use this for judge type change or not */
	unsigned int ch;
	unsigned int vtype;
	unsigned int src_type;
	unsigned int trans_fmt;
	unsigned int h;
	unsigned int v;
	bool duration_overflow; //0808
	bool fg;
	unsigned int bitdepth;
	u32 dpss_flg;
	struct canvas_config_s canvas0_config[2];
};

#define DPSS_RDMA_MODE_RD_WR	(C_BIT0)	//rd: 0, wr: 1;
#define DPSS_RDMA_MODE_INCL	(C_BIT1)	//none incl:0; incl: 1;
#define DPSS_RDMA_MODE_IRQ_SRC_MAN	(C_BIT2)	//high priority
#define DPSS_RDMA_MODE_IRQ_SRC	(0x1f00)

#define DPSS_RDMA_IRQ_SRC_DAE_FST_0	(14)	//? 14 ~ 16 ?
#define DPSS_RDMA_IRQ_SRC_DAE_FST_1	(15)	//?
#define DPSS_RDMA_IRQ_SRC_DAE_FST_2	(16)	//?
#define DPSS_RDMA_IRQ_SRC_DAE_DONE_0	(17)	//? 18 ~ 20 ?
#define DPSS_RDMA_IRQ_SRC_DAE_DONE_1	(18)	//?
#define DPSS_RDMA_IRQ_SRC_DAE_DONE_2	(19)	//?
#define DPSS_RDMA_IRQ_SRC_DPE_FST_0 (21)	//bit 21
#define DPSS_RDMA_IRQ_SRC_DPE_FST_1 (22)
#define DPSS_RDMA_IRQ_SRC_DPE_FST_2 (23)
#define DPSS_RDMA_IRQ_SRC_DPE_DONE_0 (24)
#define DPSS_RDMA_IRQ_SRC_DPE_DONE_1 (25)
#define DPSS_RDMA_IRQ_SRC_DPE_DONE_2 (26)

#define DPSS_PRE_SIZE_DAE_D	(0x800)
#define DPSS_PRE_SIZE_DAE_C	(0x400)

#define DPSS_PRE_SIZE_DPE_D	(0x1000)
#define DPSS_PRE_SIZE_DPE_C	(0x800)

struct dpss_rdma_pre_s {	//
	u32 *buf;
	unsigned int t_size;
	unsigned int cnt;
	unsigned int mode;
};

struct dpss_rdma_arg_s {
	unsigned int ch;
	int handle;
	unsigned int cnt;
};

//... buffer...
struct dpss_mem_a_s {		//mem_a_s
	struct dpss_cfg_blki_s *inf;
	const char *owner;
	const char *note;
	unsigned int shift_bits;
	unsigned char ower_id;	//use this to debug only
	unsigned int cma_flg; // 10-17
};

struct dpss_mem_r_s {		//mem_r_s
	struct dpss_blk_m_s *blk;
	const char *note;
};

struct dpss_mm_s {		//dim_mm_s
	struct page *ppage;
	unsigned long addr;
	unsigned int flg;	/*bit0 for tvp */
	void *mem_handle; //0915
};

struct dpss_mm_p_cnt_out_s {	//c_mm_p_cnt_out_s
	enum EDPSS_DP_MODE mode;
	unsigned int size_total;
	unsigned int size_page;
	unsigned int cvs_w;
	unsigned int cvs_h;
	unsigned int off_uv;
	unsigned int dbuf_hsize;
};

struct afbcd_buf_inf_s {
	unsigned int blk_total;
	unsigned int size_info;	//pafd_ctr->size_info
	unsigned int body_buffer_size;
	unsigned int size_tab;
};

struct afb_tab_info_s {
	unsigned long tabadd;
	unsigned long bodyadd;
	unsigned int size_buf;
	unsigned int size_tab;
};

struct afrc_buf_inf_s {
	unsigned int w;
	unsigned int h;
	unsigned int target_b0;
	unsigned int target_b1;
	unsigned int type; //yuv444, yuv422, yuv420;
	unsigned int bits; // 8, 10, 12?
	unsigned int size_hearder_y;
	unsigned int size_hearder_c;
	unsigned int size_tab_y;
	unsigned int size_tab_c;

	unsigned int size_body_y;
	unsigned int size_body_c;

	unsigned int size_hd;
	unsigned int size_body;
	unsigned int size_tab;
};

struct dpss_mm_p_cnt_in_s {	//c_mm_p_cnt_in_s
	unsigned int w;
	unsigned int h;
	enum EDPSS_DP_MODE mode;
};

struct dpss_cfg_blki_s {	//c_cfg_blki_s
//tmp   enum EBLK_TYPE blk_t;
	unsigned int mem_size;
	unsigned int page_size;
	bool tvp;
	unsigned char mem_from;
	struct dpss_mm_p_cnt_out_s *cnt_cfg;
};

/* like cma mem */
struct dpss_blk_m_s {		//c_blk_m_s
	struct dpss_cfg_blki_s *inf;
	bool flg_alloc;
	unsigned long mem_start;
	struct page *pages;
	void *mem_handle; //09-15
};

/* scatter mem */
struct dpss_blks_s {		//c_blks_s
	unsigned int blk_typ;
	bool flg_alloc;
	unsigned int sct_keep;	//keep number
	unsigned int length;
	void *pat_buf;
	void *sct;
};

struct dpss_blk_s {		//blk_s
	unsigned int code;	//must be first
	struct dpss_hd_s *hd;
	struct {
		unsigned int blk_typ;
		union {
			struct dpss_blk_m_s blk_m;
			struct dpss_blks_s blk_s;	//?
		} b;
		unsigned char st_id;	//ref to EBLK_ST
		struct dpss_name_s ower;
	} c;
	atomic_t get;
};

struct dpss_buf_sml_s {		//c_cfg_sml_s
	//unsigned int size_nr;
	unsigned int size_dw;
	unsigned int size_mix;
//	unsigned int size_mix_c;	//use this as frc mix buffer
	unsigned int size_mv;
	unsigned int size_alp;
	unsigned int size_logo;
	unsigned int size_melogo;
	unsigned int size_mtn;
	unsigned int size_blk_info;
	unsigned int size_dmsq;
	unsigned int size_dct_grid;
	unsigned int size_dct_y;	//tmp
	unsigned int size_dct_c;	//tmp
	unsigned int size_tfbc;
	unsigned int size_grad_h;
	unsigned int size_grad_v;
	unsigned int size_ro1;
	unsigned int size_ro2;
	unsigned int size_afbc;
	unsigned int size_afrc_y;//y
	unsigned int size_afrc_c; //c
	unsigned int size_frc_inp_mbuf0;
	unsigned int size_frc_inp_mbuf1;
	unsigned int size_frc_me_mbuf;
	unsigned int size_frc_me_nc_uni_mv;
	unsigned int size_frc_me_cn_uni_mv;
	unsigned int size_frc_me_pc_phs_mv;
	unsigned int size_frc_logo_iir;
	unsigned int size_frc_logo_ssc;
	unsigned int size_frc_blk_logo;
	unsigned int size_frc_pix_logo;
	unsigned int size_frc_vp_mc_mv;
	unsigned int size_frc_vp_mc_logo;
	unsigned int size_frc_mer0;

	//unsigned int cvs_w_nr;
	unsigned int cvs_w_mtn;
	unsigned int cvs_w_mv;
	unsigned int cvs_h;
	unsigned int cvs_h_mc;

	unsigned int size_ts_mix;
	unsigned int size_ts_mv;
	unsigned int size_ts_alp;
	unsigned int size_ts_logo;
	unsigned int size_ts_melogo;
	unsigned int size_ts_mtn;
	unsigned int size_ts_blk_info;
	unsigned int size_ts_dmsq;
	unsigned int size_ts_dct_grid;
	unsigned int size_ts_dct_y;
	unsigned int size_ts_dct_c;
	unsigned int size_ts_tfbc;
	unsigned int size_ts_grad_h;
	unsigned int size_ts_grad_v;
	unsigned int size_ts_ro1;
	unsigned int size_ts_ro2;
	unsigned int size_ts_afbc;
	unsigned int size_ts_afrc;	// 06-26 for afrce
	unsigned int size_ts_afbc_tab;
	unsigned int size_ts_afrc_tab_c;
	unsigned int size_ts_dw;	//dw * n

	unsigned int size_ts_frc_inp_mbuf0;
	unsigned int size_ts_frc_inp_mbuf1;
	unsigned int size_ts_frc_me_mbuf;
	unsigned int size_ts_frc_me_nc_uni_mv;
	unsigned int size_ts_frc_me_cn_uni_mv;
	unsigned int size_ts_frc_me_pc_phs_mv;
	unsigned int size_ts_frc_logo_iir;
	unsigned int size_ts_frc_logo_ssc;
	unsigned int size_ts_frc_blk_logo;
	unsigned int size_ts_frc_pix_logo;
	unsigned int size_ts_frc_vp_mc_mv;
	unsigned int size_ts_frc_vp_mc_logo;
	unsigned int size_ts_frc_mero;

	unsigned int size_t_nr_s;	//mix mv mtn blk_info dmsq
	unsigned int size_t_nr_frc_s;
	unsigned int size_t_dct;
	unsigned int size_t_grad;
	unsigned int size_t_frc;

//      unsigned int size_s_nr_only;
	unsigned int size_s_frc_only;
	unsigned int size_s_nr_frc;
#ifdef _HIS_CODE_
	bool en_dct;
	bool en_tfbc;
	bool en_grad;
	bool en_dw;
	bool en_frc;
	bool en_afbc;
#endif
};

struct dpss_buf_rdma_s {
	unsigned int size_total;
	unsigned int size_one;
	unsigned int nub_max;
};

static inline bool is_ic_named(unsigned int crr_id, unsigned int ic_id)
{
	if (crr_id == ic_id)
		return true;
	return false;
}

#define DPSS_IS_IC(cc)	is_ic_named((dpss_get_devp()->mdata->ic_id), \
					IC_ID_##cc)

#ifdef RUN_ON_ARM
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#endif				//RUN_ON_ARM
extern unsigned int dpss_wr_sel;
extern unsigned int dpss_int;

#endif	/*__DPSS_BASE_H__*/
