/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DPSS_TOP_H_
#define _DPSS_TOP_H_
#ifdef RUN_ON_PC
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#endif /* RUN_ON_PC */

#include "dpss_intf.h"
//#include "dpss_baddr.h"
#include "fgrain.h"

#define DPSS_REGOFST_VDICTRL  ((0x86 - 0x83) * 256)  //ary addr change *4
#define PHS_FRAC_BIT         7

#define DPSS_INP_EVENT       0  //src0:inp frm rst
#define DPSS_DAE0_EVENT      1  //src0:frc_me, frc_nr_me
#define DPSS_DAE1_EVENT      2  //src0: nr only me
#define DPSS_DAE2_EVENT      3  //src1: di only
#define DPSS_DPE0_EVENT      4  //dpe_only_mc_int
#define DPSS_DPE1_EVENT      5  //dpe_only_nr_int
#define DPSS_DPE2_EVENT      6  //dpe_di_nr_int

#define SRC0_DATA_EVENT      7  //src0 data load
#define SRC1_DATA_EVENT      8  //src1 data load

#define DPSS_WORK_MODE_EVENT 9  //work mode  set

#define DPSS_DAE2_DONE_EVENT 11  //dpe_di_nr_int
#define DPSS_DPE2_DONE_EVENT 12  //dpe_di_nr_int
#define DPSS_DAE_OUT_ADDR    13
#define DPSS_DPE_OUT_ADDR    14
#define DPSS_DONE_EVENT      15
#define DPSS_SEC_EVENT       16 //for tb to set ddr sec
#define DPE_APB_CRASH_EVENT  17
#define DPSS_TOP_RO_EVENT    18
#define DPSS_DPE_ADDR_EVENT  19
//==============================================================//
// ENUM
//==============================================================//
//SRC0 means : frc nr tbc ctrl,  src1: vdi tbc ctrl
enum DPSS_WORK_MODE {
	DPSS_FRC_MODE        = 0, //src0:frc path
	DPSS_FRC_NR_MODE     = 1, //src0:frc+nr path
	DPSS_NR_SRC0_MODE    = 2, //src0:nr only path for frc/nr crtl
	DPSS_FRC_NRBYPS_MODE = 3, //nr core bypass mode

	DPSS_DI_MODE         = 4, //src1:di only
	DPSS_NRDI_MODE       = 5, //src1:di(+nr)
	DPSS_NR_SRC1_MODE    = 6, //src1:nr path

	//src0 support NRDI
	DPSS_NRDI_SRC0_MODE      = 14,
	DPSS_NRDI_FRC_SRC0_MODE  = 15,
	DPSS_DI_FRC_SRC0_MODE    = 16,

	DAE_FRC_MODE         = 20,//frc+link_nr
	DAE_NR_MODE          = 21,//nr_only 1loop
	DAE_VDI_MODE         = 22,//di(+nr)
	DAE_BYPS_MODE        = 23,//dae bypass
	DAE_NRDI_MODE        = 24,//nr_only x6loop
	DAE_NR_SRC1_MODE     = 25,//nr_only x6loop
	DAE_DV_SRC0_MODE     = 26,
	DAE_GAME_MODE        = 27,//scr0_frc game_mode

	DPE_MC_MODE          = 30,
	DPE_NR_MODE          = 31,
	DPE_NR_BYPS          = 32,
	DPE_VDI_MODE         = 33, //src1:di only path
	DPE_NRDI_MODE        = 34, //src1:di(+nr) path
	DPE_NR_SRC1_MODE     = 35,
	DPE_DV_SRC0_MODE     = 36,  //dolby
	DPE_BBD_ONLY         = 37
};

enum PRM_SRC_IDX {
	SRC_IDX_FRC = 1,
	SRC_IDX_NR  = 7,
	SRC_IDX_DI0 = 8,
	SRC_IDX_DI1 = 9,
	SRC_IDX_DI2 = 10,
	SRC_IDX_DI3 = 11,
};

enum DONE_TRIGGER {
	FRC_SRC_DONE  = 0,
	FRC_DST_DONE  = 1,
	FNR_SRC_DONE  = 2,
	FNR_DST_DONE  = 3,
	VDI_SRC_DONE  = 4,
	VDI_DST_DONE  = 5
};

enum DPSS_FILM_MODE {
	DPSS_VIDEO     = 0,
	DPSS_FILM22    = 1,
	DPSS_FILM32    = 2,
	DPSS_FILM3223  = 3,
	DPSS_FILM2224  = 4,
	DPSS_FILM32322 = 5,
	DPSS_FILM44    = 6,
	DPSS_FILM21111 = 7,
	DPSS_FILM23322 = 8,
	DPSS_FILM2111  = 9,
	DPSS_FILM22224 = 10,
	DPSS_FILM33    = 11,
	DPSS_FILM334   = 12,
	DPSS_FILM55    = 13,
	DPSS_FILM64    = 14,
	DPSS_FILM66    = 15,
	DPSS_FILM87    = 16,
	DPSS_FILM212   = 17,
	DPSS_FILM_PD   = 20//film_mode use pd auto
};

enum DPSS_FRC_RATIO {
	DPSS_FRC_RATIO_1_1  = 0,
	DPSS_FRC_RATIO_1_2  = 1,
	DPSS_FRC_RATIO_2_3  = 2,
	DPSS_FRC_RATIO_2_5  = 3,
	DPSS_FRC_RATIO_5_6  = 4,
	DPSS_FRC_RATIO_5_12 = 5,
	DPSS_FRC_RATIO_2_9  = 6,
	DPSS_FRC_RATIO_2_15 = 7,
	DPSS_FRC_RATIO_1_4  = 8,
	DPSS_FRC_RATIO_1_5  = 9,
	DPSS_FRC_RATIO_1_6  = 10,
	DPSS_FRC_RATIO_1_3  = 11,
	DPSS_FRC_RATIO_3_5  = 12,
	DPSS_FRC_RATIO_3_10 = 13,
	DPSS_FRC_RATIO_4_5  = 14,
	DPSS_FRC_RATIO_1_8  = 15,
	DPSS_FRC_RATIO_1_10  = 16,
	DPSS_FRC_RATIO_1_12  = 17,
	DPSS_FRC_RATIO_1_20  = 18
};

struct DPSS_SEC {
	u32 inp_sec[2];       //[15:0]inp pix mif, //[31:16]input pre/cur
	u32 dae_sec[4];       //dae pix mif, dae/pre/cur/nxt  , nxt dae wrmif_sec
	u32 nrdi_rd_sec[4];   //pix mif 0/1/2/3
	u32 mc_rd_sec[2];     //pix mif 4/5
	u32 nrdi_wr_sec[4];   //wrmif0/vfce,  wrmif1, ds wrmif, nr_sub_sec
};

enum MC_SRNG_MODE_e {
	SRNG_NORMAL            = 0,  //normal mode,       no h/2 v/2
	SRNG_SINGLE_DOWN       = 1,  //single mode(down), no h/2 v/2
	SRNG_SINGLE_UP         = 2,  //single mode(up),   no h/2 v/2
	//ASYMMETRIC           = 3,  //not supported
	SRNG_NORMAL_V2         = 4,  //normal mode ,      v/2
	SRNG_SINGLE_DOWN_V2    = 5,  //single mode(down), v/2
	SRNG_SINGLE_UP_V2      = 6,  //single mode(up),   v/2
	SRNG_NORMAL_H2V2       = 7,  //normal mode ,      v/2 + h/2
	SRNG_SINGLE_DOWN_H2V2  = 8,  //single mode(down), v/2 + h/2
	SRNG_SINGLE_UP_H2V2    = 9  //single mode(up),   v/2 + h/2
	//SRNG_GREEDY            = 10, //single mode(up),  v/2 + h/2
	//SRNG_GREEDY1           = 11, //single mode(up),  v/2 + h/2
	//SRNG_GREEDY2           = 12  //single mode(up),  v/2 + h/2
};

enum DOLBY_WORK_MODE {
	DOLBY_WRAP_BYPS         = 0,
	DOLBY_CORE_BYPS         = 1,
	DOLBY_VD1_MODE          = 2,
	DOLBY_VDIN_MODE         = 3,
	DOLBY_DPSS_MODE         = 4,//serial mode
	DOLBY_DPSS_PRL_MODE     = 5,//parallel mode
	DOLBY_DPSS_DI_MODE      = 6 //serial mode
};

//typedef struct DPSS_NRDI_ENS{
//	u32   vdi_dblk_en_h     ;
//	u32   vdi_dblk_en_v     ;
//	u32   vdi_deblk_info_en ;
//	u32   vdi_xlr_en        ;
//	u32   vdi_xlr_side_en   ;
//	u32   vdi_nr_tnr_enable ;
//	u32   vdi_nr_snr_enable ;
//	u32   vdi_dms_enable    ;
//	u32   vdi_ne_en         ;
//	u32   vdi_cfr_en        ;
//	u32   vdi_cue_en        ;
//	u32   vdi_cue_enable_l  ;
//	u32   vdi_cue_enable_r  ;
//} DPSS_NRDI_ENS_t;

//==============================================================//
// STRUCT
//==============================================================//
struct PRM_SRC_FMT {
	enum vid_src_fmt    src_fmt;
	enum vid_src_fmt    src_plan;
	enum vid_src_fmt    src_bit;
	enum vid_src_fmt    src_cmpr;
	enum vid_src_fmt    interlace;
};

struct DPSS_CUT_WIN {
	u32  frm_hsize;
	u32  frm_vsize;
	u32  win_hsize;
	u32  win_vsize;
	u32  win_hsize_align;
	u32  win_vsize_align;
	u32  win_hbgn;
	u32  win_hend;
	u32  win_vbgn;
	u32  win_vend;
	u32  win_hbgn_align;
	u32  win_hend_align;
	u32  win_vbgn_align;
	u32  win_vend_align;
};

struct PRM_MC_SET {
	enum MC_SRNG_MODE_e     luma_srng_mode;
	enum MC_SRNG_MODE_e     chrm_srng_mode;
};

struct PRM_COMP_SET {
	u32        vdin_mmu_mode_en    ;//0:disable 1:enable
	u32        mmu_mode_en         ;//0:disable 1:enable
	u32        mmu_page_mode       ;//0:4k page 1:8k page
	u32        reg_444to422_mode   ;//filter mode
	u32        vfce_in_overlap     ;//vfce input overlap
	u32        src_comp_switch     ;//afrc afbc switch
	u32        src_switch_num      ;//switch num
	u32        nro_comp_switch     ;//afrc afbc switch
	u32        nro_switch_num      ;//switch num
	u32        afrc_head_mode      ;//0:wo header 1: wi header
	u32        afrc_dict_mode_y      ;//
	u32        afrc_dict_mode_c      ;//
	u32        afrc_target_byte_y  ;//target byte
	u32        afrc_target_byte_c  ;//target byte
	u32        vfcd_rev_mode       ;//rev mode
	u32        vfcd_h_skip         ;//skip
	u32        vfcd_v_skip         ;//skip
	u32        fmt444_comb         ;//comb 8bit 444
	u32        dpe_vfce_num        ;//vfce start num
	u32        dpe_vfcd_num        ;//vfcd start num
};

struct PRM_DPE_MC_SIZE {
	u32 src_hsize;
	u32 src_vsize;
	u32 frm_hsize;
	u32 frm_vsize;
	u32 rmif_hsize[2];
	u32 rmif_vsize[2];
};

struct PRM_DPSS_LCEVC {
	u32 src1_frm_hsize;
	u32 src1_frm_vsize;
	u32 src1_head_ybaddr;
	u32 src1_body_ybaddr;
	u32 src1_head_cbaddr;
	u32 src1_body_cbaddr;
	u32 src1_is_cmpr;
	u32 src1_bit;
	u32 src2_frm_hsize;
	u32 src2_frm_vsize;
	u32 src2_ybaddr;
	u32 src2_cbaddr;
	u32 frm_hsize_out;
	u32 frm_vsize_out;
	unsigned int dbg_cfg; //ary add
};

struct PRM_DPSS_TOP {
	u32	case_id; //07-10 2ch
	u32        org_hsize;
	u32        org_vsize;
	u32        frm_hsize;	//ary input size
	u32        frm_vsize;
	u32        nr_src_canvas_hsize; //jt input canvas size

	u32     amdv_frm_hsize;
	u32     amdv_frm_vsize;
	u32     amdv_org_hsize;
	u32     amdv_org_vsize;
	u32     amdv_slc_num; /*DI DV parallel DV_slice_num*/
	u32     amdv_pad; /*DI DV parallel DV overlap DV on:96 DV off :?*/

	u32        dae_dsx_scale;
	u32        dae_dsy_scale;
	u32        mvx_div_mode;  //nr di must be 1:1.frc must be 2:2
	u32        mvy_div_mode;
	u32        dpe_slc_num;
	u32        dpe_dw_dsx;	//ary:double write x ratio?
	u32        dpe_dw_dsy;
	u32        dpe_mc_phsx2;
	u32        di_frc_link;
	bool            auto_alig_en;
	u32        alig_hmode;
	u32        alig_vmode;
	bool            dpe_game_mode;
	bool            no_ds;   //jintao: no dw case
	bool       dpe_out2bbd_en;
	u32        dpe_out2bbd_mode;
	enum DPSS_WORK_MODE  dpss_mode;
	enum DPSS_FILM_MODE  film_mode;
	enum DPSS_FRC_RATIO  frc_ratio;
	struct PRM_SRC_FMT   src_fs_fmt;//full-resolution
	struct PRM_SRC_FMT   src_ds_fmt;//down-scaler src
	struct PRM_SRC_FMT   nro_fs_fmt;//nr/di/mc out full resolution
	struct PRM_SRC_FMT   nro_ds_fmt;//dpe down-scaler out

	u32        badedit_en;
	enum DOLBY_WORK_MODE dolby_mode;
	bool            nr_double_rst_mode;
	bool            nr_vpp_link_en;	//ary
	bool            me_mc_link_en;
	bool            src_is_1080p_nods; //?
	bool		size_as_in; //ary
	bool            mc_lp_mode;
	bool            is_amdv_frame;
	struct PRM_MC_SET    mc_setting;  //better move to DPE STRUCT
	bool            fw_en;
	bool            sw_ctrl_en;
	u32        dae_step_sel;//0:tab 1:inp site config step 2:dae site config step
	bool            pvsync_intr_en;
	bool            film_hwfw_sel;
	bool            force_film_mode;
	bool            dpss_nr_byp;
	u32        frm_hsize_sel;
	struct PRM_COMP_SET  comp_setting;
	u32        frc_rev_mode;
	u32        nr_hscale_on;
	u32        nr_aapps_up;
	u32        nr_aapps_ds;
	u32        nr_aapps_on;
	u32        nr_aapps_mode;
	u32        mif_mmu_mode_en; //rdmif/wrmif use mmu mode
	u32        ds_mif_mmu_mode_en; //ds  pix rdmif/wrmif use mmu mode
	struct PRM_DPE_MC_SIZE dpe_mc_size;
	bool            afrc_cmp_en;
	u32        fnr_sw_mode;
	u32        vdi_sw_mode;
	u32        sw_tbc_mode;
	u32        frc_ds_scale_en;
	u32        nr_src_align;
	u32        frc_fbuf_only;
	u32        mc_skip_mode;
	bool   frc_me_en:1;
	bool   frc_vp_en:1;
	bool   frc_melogo_en:1;
	bool   frc_iplogo_en:1;
	bool   frc_bbd_en:1;
	bool   frc_no_alg_ko:1;
	bool frc_dae_div4:1;

	u8   use_inp_big;
	int            amdv_2_frc_frm;
	unsigned int        src0_fbuf_yaddr[16];
	unsigned int        src0_fbuf_caddr[16];
	unsigned int        src0_dwbuf_yaddr[16];
	unsigned int        src0_dwbuf_caddr[16];
	unsigned int        src0_nro_fbuf_yaddr[16];
	unsigned int        src0_nro_fbuf_caddr[16];

	unsigned int        src0_nro_fbuf_af_y[16];//1002 ary add for light update
	unsigned int        src0_nro_fbuf_af_c[16];
	unsigned int        src0_nro_fbuf_m_y[16];
	unsigned int        src0_nro_fbuf_m_c[16];

	unsigned int        src0_nro_dwbuf_yaddr[16];
	unsigned int        src0_nro_dwbuf_caddr[16];
	unsigned int        src0_dio_fbuf_yaddr[16];
	unsigned int        src0_dio_fbuf_caddr[16];
	unsigned int	    src0_diopps_fbuf_yaddr[16];
	unsigned int	    src0_diopps_fbuf_caddr[16];
	unsigned int        src0_di2pps_buf_yaddr[16];
	unsigned int        src0_di2pps_buf_caddr[16];
	unsigned int        src0_dio_dwbuf_yaddr[16];
	unsigned int        src0_dio_dwbuf_caddr[16];
	unsigned int        src1_fbuf_yaddr[16];
	unsigned int        src1_fbuf_caddr[16];
	unsigned int        src1_dwbuf_yaddr[16];
	unsigned int        src1_dwbuf_caddr[16];
	unsigned int        src1_nro_fbuf_yaddr[16];
	unsigned int        src1_nro_fbuf_caddr[16];
	unsigned int        src1_nro_dwbuf_yaddr[16];
	unsigned int        src1_nro_dwbuf_caddr[16];
	unsigned int        src1_dio_fbuf_yaddr[16];
	unsigned int        src1_dio_fbuf_caddr[16];
	unsigned int        src1_dio_dwbuf_yaddr[16];
	unsigned int        src1_dio_dwbuf_caddr[16];
	unsigned int        src0_head_yaddr[16];
	unsigned int        src0_head_caddr[16];
	unsigned int        src0_nro_head_yaddr[16];
	unsigned int        src0_nro_head_caddr[16];
	unsigned int        src0_dio_head_yaddr[16];
	unsigned int        src0_dio_head_caddr[16];
	unsigned int        src1_head_yaddr[16];
	unsigned int        src1_head_caddr[16];
	unsigned int        src1_nro_head_yaddr[16];
	unsigned int        src1_nro_head_caddr[16];
	unsigned int        src1_dio_head_yaddr[16];
	unsigned int        src1_dio_head_caddr[16];

	u32        bbd_only;
	u32        bbd_parallel;
	bool            cut_win_en;
	bool            cut_win_position;
	u32        dpe_ro_wdma_en;
	struct DPSS_CUT_WIN prm_cut_win;
	u32        reverse[2];//[0]:x      [1]:y
	u32        lcevc_en;
	struct PRM_DPSS_LCEVC lcevc;
	u32        dpe_dw_off;
	bool			dlg_stats;
	bool			game_mode_n2m;
	bool			game_mode_film;
	//frc
	unsigned int  inp_mbuf_addr[2]; //
	unsigned int  frc_me_mbuf_addr; //
	unsigned int  src0_me_nc_uni_mv_addr; //
	unsigned int  src0_me_cn_uni_mv_addr; //
	unsigned int  src0_me_pc_phs_mv_addr; //
	unsigned int  src0_logo_iir_addr; //
	unsigned int  src0_logo_scc_addr; //
	unsigned int  src0_blk_logo_addr; //
	unsigned int  src0_pix_logo_addr; //
	unsigned int  src0_vp_mc_mv_addr; //
	unsigned int  src0_vp_mc_logo_addr; //
	unsigned int  src0_frc_mero_addr; //

	unsigned int  inp_mbuf_step; //
	unsigned int  frc_me_mbuf_step; //
	unsigned int  src0_me_nc_uni_mv_step; //
	unsigned int  src0_me_cn_uni_mv_step; //
	unsigned int  src0_me_pc_phs_mv_step; //
	unsigned int  src0_logo_iir_step; //
	unsigned int  src0_logo_scc_step; //
	unsigned int  src0_blk_logo_step; //
	unsigned int  src0_pix_logo_step; //
	unsigned int  src0_vp_mc_mv_step; //
	unsigned int  src0_vp_mc_logo_step; //
	unsigned int  src0_frc_mero_step; //
	//ary add:
	unsigned int  src0_mix_addr; //y/c
	unsigned int  src0_me_info_addr; //y/c
	unsigned int  src0_mtn_addr; //
	unsigned int  src0_me_alp_addr; //
	unsigned int  src0_tfbf_addr; //
	unsigned int  src0_mv_addr; //
	unsigned int  src0_ro1_addr;
	unsigned int  src0_ro2_addr;
	unsigned int  src0_dct_his_addr; //
	unsigned int  src0_dct_y_addr; //
	unsigned int  src0_dct_c_addr; //
	unsigned int  src0_grad_h_addr; //
	unsigned int  src0_grad_v_addr; //
	unsigned int  src0_nro_dw_addr; //
	unsigned int  src0_dmsq_addr; //
	unsigned int  src0_logo_addr;
	unsigned int  src0_melogo_addr;
	unsigned int  src0_afbce_h_yaddr;	//2025-03-04
	unsigned int  src0_afbce_h_caddr;
	unsigned int  src0_mix_step[2]; //y/c
	unsigned int  src0_me_info_step; //y/c
	unsigned int  src0_mtn_step; //
	unsigned int  src0_me_alp_step; //
	unsigned int  src0_tfbf_step; //
	unsigned int  src0_mv_step; //
	unsigned int  src0_dct_his_step; //
	unsigned int  src0_dct_y_step; //
	unsigned int  src0_dct_c_step; //
	unsigned int  src0_grad_h_step; //
	unsigned int  src0_grad_v_step; //
	unsigned int  src0_nro_dw_step; //
	unsigned int  src0_dmsq_step; //
	unsigned int  src0_logo_step;
	unsigned int  src0_melogo_step;
	unsigned int	src0_afbce_h_y_step;	//2025-03-04
	unsigned int	src0_afbce_h_c_step;
	unsigned int  src1_mix_addr; //y/c
	unsigned int  src1_me_info_addr; //y/c
	unsigned int  src1_mtn_addr; //
	unsigned int  src1_me_alp_addr; //
	unsigned int  src1_tfbf_addr; //
	unsigned int  src1_mv_addr; //
	unsigned int  src1_ro1_addr;
	unsigned int  src1_ro2_addr;
	unsigned int  src1_dct_his_addr; //
	unsigned int  src1_dct_y_addr; //
	unsigned int  src1_dct_c_addr; //
	unsigned int  src1_grad_h_addr; //
	unsigned int  src1_grad_v_addr; //
	unsigned int  src1_nro_dw_addr; //
	unsigned int  src1_dmsq_addr; //
	unsigned int  src1_afbce_h_yaddr;	//2025-03-04
	unsigned int  src1_afbce_h_caddr;

	unsigned int  src1_mix_step[2]; //y/c
	unsigned int  src1_me_info_step; //y/c
	unsigned int  src1_mtn_step; //
	unsigned int  src1_me_alp_step; //
	unsigned int  src1_tfbf_step; //
	unsigned int  src1_mv_step; //
	unsigned int  src_ro_step;//
	unsigned int  src1_dct_his_step; //
	unsigned int  src1_dct_y_step; //
	unsigned int  src1_dct_c_step; //
	unsigned int  src1_grad_h_step; //
	unsigned int  src1_grad_v_step; //
	unsigned int  src1_nro_dw_step; //
	unsigned int  src1_dmsq_step; //
	unsigned int	src1_afbce_h_y_step;	//2025-03-04
	unsigned int	src1_afbce_h_c_step;
//ary add:
	unsigned int dae_dpe_cfg_en;
	bool dpe_chg;
	unsigned int dpe_last_mode;
	unsigned int h_o_rd_cnt; //read in dpe irq each time;
	unsigned int cnt_dpe_irq; //from 0
	unsigned int cnt_dpe_rd; //
	enum DPSS_WORK_MODE  dae_nr_mode;
	enum DPSS_WORK_MODE  dpe_nr_mode;
	enum DPSS_WORK_MODE  dpe_di_mode;
	unsigned char src_mode;
	unsigned char cfg_seed;
	unsigned char cfg_slc;
	unsigned char slc_num;
	bool frc_en; //ary add
	unsigned char        di_interlace;
	unsigned char	f_top;//ary add
	bool			mc_auto_en;
	bool			disp_pat_en;
	bool			inp_done_int;
	bool			nr_inp_en;
	bool			sw_tbc_ctrl_en;
	bool			sw_buf_rls_en;
	bool	wait_en; //ary add 2025-0314 for tbc dv small buffer mode
	unsigned char mc_bypass;
	bool di_line_sel;
	bool  force_dos_mode;
	bool is_secure;
	unsigned char afbcd_update; //07-10 0: not update, 1. update vfcd0, 2. update vfcd1
	unsigned int src_h_buf_size; //ary
	unsigned int frc_vfcd_vfmt;
	bool	l_endian;
	bool	uv_swap;
	bool	src_dw_uv_swap;//no-afbc src data
	bool	swap_64bit;
	bool	little_endian;
	u32 block_mode;
	unsigned int out_mode;
	bool is_afbcd;
	bool is_i;
	bool is_dpss_init_done;
	bool mode_drct;
	bool mode_drct2;
	bool mode_drct_lst; //for dd
	bool dw_disable; //
	struct dpss_input_q *in_q;
	u32 vds_4k1k_en;
	bool is_current;
	u32 frame_count;
	u32 rls0_irq_count;
	struct FGRAIN_t fgrain;
	bool fg_en;
	bool fmt_444_10;
	u32 sw_dct_frame_cnt;
	bool trig_bypass; //1006
	bool trig_byp_dae0;
	bool trig_byp_mc;
	bool is_hdmi_src;
	bool reset_path;
	u32  idx_done;
	bool di_front; //1125
	u32 s_cnt; //for dv cnt
	bool dct_ahead_dv_mode;
	unsigned char num_in;
	unsigned char num_dpe_o;
	unsigned char num_lc; //
	unsigned char num_aepe;
	unsigned char num_nr_wrpt;
	unsigned char num_dblk;
	unsigned char num_dpe_ro;
	unsigned char num_nr_me_ro;
	u32 pps_dsx;
	u32 pps_dsy;
	bool is_pps;
	bool is_di2pps;
	u32 ch;
	unsigned int fbuf_is_pps[16];
	bool en_update_hdr; //
	void *update_hdr_data;
};

struct PRM_DPSS_SIZE {
	u32 frm_hsize;
	u32 frm_vsize;
	u32 inp_hsize;
	u32 inp_vsize;
	u32 me_hsize;
	u32 me_vsize;
	u32 me_blk_hsize;
	u32 me_blk_vsize;
	u32 dpe_hsize[4];
	u32 dpe_vsize[4];
	u32 dmsq_hsize;
	u32 dmsq_vsize;
	u32 frm_hsize_sel;
};

struct PRM_DPE_NR_SIZE {
	u32 rmif_hsize[4];//nr rdmif
	u32 rmif_vsize[4];
	u32 src_hsize;//nr_path
	u32 src_vsize;
	u32 frm_hsize;
	u32 frm_vsize;
	u32 pps_hsize;
	u32 pps_vsize;
};

struct PRM_DPSS_TBC {
	u32 input_n;
	u32 output_m;
	u32 phase_table_num;
	u64 phs_lut_table[256];
	u32 inp_ofrm_idx_sel; //1bit, inp out frame idx sel film det phase or calc phase
	u32 frc_fbuf_ctrl_mode; //1bit, 0: phase tab ctrl, 1: automatic ctrl
	u32 inp_nr_link_mode; //1bit, frcnr_me_share_mode
	u32 inp_obuf_num;
	u32 inp_ibuf_num; // todo: 4?
	u32 dae_ibuf_num;
	u32 dae_obuf_num;
	u32 inp_ofrm_loop_num; // 8bit
	u32 inp_ofrm_loop_tab; // 32b
	u32 dae_loop_step_num; // 5b
	u32 mc_phsx2_mode_en; // 1bit
	u32 reg_frc_src_idx; // 4bit //todo ?
};

struct PRM_DPSS_INP {
	u32 todo0;
	u32 todo1;
};

struct PRM_DPE_PAD {
	u32        reg_vbe_pad;
	u32        reg_nrdi_pad;
	u32        reg_mc_pad;
	u32        reg_dv_pad;
	u32        reg_aa_pad;
	u32        reg_dmsq_pad;
	u32        reg_dcntr_pad;
	u32        reg_bbd_pad;
};

struct PRM_DPE_REG_MODE {
	bool            top_reg_mode;
	bool            vfcd_reg_mode;
	bool            vfce_reg_mode;
	u32        subrd_reg_mode;//11bit
	u32        subwr_reg_mode;//4bit
};

struct PRM_DPE_SUB_RMIF {
	u32        sub_rdmif_baddr[11];
	u32        sub_wrmif_baddr[4];
	u32        vbe_dmsq_hsize;
	u32        grd_xnum_use;
	u32        dmsq_xsft;
};

struct PRM_DPE_2CH_RMIF {
	u32        ch2_rdmif0_baddr[2];
	u32        ch2_rdmif1_baddr[2];
	u32        ch2_rdmif2_baddr[2];
	u32        ch2_rdmif3_baddr[2];
	u32        ch2_rdmif_vd1_baddr[2]; //vd1
	u32        ch2_rdmif_vd2_baddr[2]; //vd2
};

struct PRM_DPSS_DAE {
	enum DPSS_WORK_MODE  dae_mode;
	u32        tfbf_en;
	u32        dctgrd_en;
	struct PRM_INTF_TYPE      dae_yuv_mif;
	bool            dae_fmt_up_repeat;  //0: for avg, 1 for repeat
	struct PRM_DPSS_SIZE prm_size;
	//ary add for globe
	struct PRM_DPSS_SIZE *prm_size_ext; //prm_size
	struct PRM_INTF_TYPE *ext_yuv_intf;//dae_yuv_intf
	struct PRM_INTF_TYPE nr_yuv_intf;	//05-01 by ary
};

struct DPSS_MC0_TYPE { //cfg @pre_go_field
	u32       frm_hsize;
	u32       frm_vsize;
	u32       src_hsize;
	u32       src_vsize;
	u32       mvx_div_mode;
	u32       mvy_div_mode;
	u32       luma0_baddr;
	u32       luma1_baddr;
	u32       chrm0_baddr;
	u32       chrm1_baddr;
	u32       logo0_baddr;
	u32       logo1_baddr;
	u32       mv_baddr;
	u32       melg_baddr;
	u32       phase;
	u32       mc_rev_mode;
	u32       mmu_mode_en; //rdmif/wrmif use mmu mode
	u32       pad_en;
	u32       pad_hmode;//0:8 1:16
	u32       pad_vmode;//0:8 1:16
	enum vid_src_fmt    pix_fmt;
	enum vid_src_fmt    pix_bit;
	enum vid_src_fmt    pix_plan;
	enum vid_src_fmt    src_cmpr;
	struct PRM_COMP_SET comp_setting;
};

struct DPSS_MC1_TYPE {//cfg @go_field
	u32 mc_byp_en;//todo
};

#ifdef NO_USED
struct DPSS_VPU_RDMA {
}; //ary define tmep 1205
#endif
struct VPU_RDMA_TYPE { //ary move from rdma.h
	u64    ahb_start_addr;
	u64    ahb_end_addr;
	u8     ahb_rd_burst_size;
	u8     ahb_wr_burst_size;
	u8     cbus_write;
	u8     cbus_addr_incr;
	u8     rdma_chn_idx;
	u8     cbus_buf_sel_man;// 0: DDR 1: SRAM
	u8     rdma_start_auto;
	// 0: start manually, 1: start by interrupt trigger
	u32    rdma_auto_src_sel[16];
	// channel to select interrupt src num,bit0:interrupt[0],
	//bit1: interrupt[1],....
	// rdma_auto_src_sel[0] not used, its src0 for man
	u64    ahb_auto_start_addr[16];
	// [0] is man addr, [1..15] for auto addr
	u64    ahb_auto_end_addr[16];
	u8     cbus_auto_write[16];
	u8     cbus_auto_addr_incr[16];

	u8     buf_auto_incr_mode[16];
	u32    buf_auto_incr_step[16];
	u8     buf_auto_incr_idx[16];
	u8     buf_auto_incr_length[16][16];
    /* u32    release_sys_spval_after_rdfifo_cnt; */
};

//ary move from tbc
enum DPSS_TRIGGER_MODE {
	FRC_INP_IBUF_RDY = 0,
	FRC_INP_OBUF_RDY = 1,
	FRC_DAE_IBUF_RDY = 2,
	FRC_DAE_OBUF_RDY = 3,
	FNR_DAE_IBUF_RDY = 4,
	FNR_DPE_OBUF_RDY = 5
};

#endif
