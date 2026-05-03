/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_DBG_H__
#define __DI_DBG_H__

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#define CRC_COUNT_NUB		(20)
#define CRC_NUB		(3)

extern unsigned int dim_cfg;
extern int dim_trig_fg;
extern int ON_420_10;
extern int nv_21_10bit;
extern int invert_top_bot;
extern int frame_count;
extern unsigned int dbg_dct;
extern unsigned int tst_pre_vpp;
extern unsigned int tst_plink_vpp;
extern u32 afbc_cfg;
extern unsigned int dim_afbc_skip_en;
extern unsigned int afbc_skip_pps_w, afbc_skip_pps_h;
extern unsigned int s4dw_buf_h;

extern int dim_post_num;
extern int dim_slt_mode;
extern unsigned int dimmcen_mode;
extern unsigned int dimmcpre_en;
extern unsigned int dimpulldown_enable;
extern unsigned int di_dbg;
extern unsigned int dim_pre_tm_thd;
extern unsigned int combing_fix_en;
extern int cur_lev;
extern unsigned int di_force_bit_mode;
extern unsigned int force_prog;
extern unsigned int dnr_pr;
extern unsigned int dnr_dm_en;
extern unsigned int dnr_en;
extern unsigned int nr2_en;
extern unsigned int dynamic_dm_chk;
extern unsigned int autonr_en;
extern unsigned int nr4ne_en;
extern unsigned int nr_ctrl_reg;
extern int dim_trig_delay;
extern unsigned int insert_line;
extern unsigned int cfg_vf;
//extern unsigned int di_stop_reg_addr;
extern unsigned int overturn;
extern int mpeg2vdin_flag;
extern int mpeg2vdin_en;
extern unsigned int di_pre_rdma_enable;
extern unsigned int pldn_dly;
extern unsigned int pldn_dly1;
extern int di_reg_unreg_cnt;
extern unsigned int by_pass_pre;
extern unsigned int pd_source_en;
extern u32 afbc_cfg_vf;
extern u32 afbc_cfg_bt;
extern u32 sc2_dbg_cnt_pre;
extern u32 sc2_dbg_cnt_pst;
extern u32 sc2_dbg;
extern u32 sc2_reg_mask;
extern unsigned int dim_hf_dbg;
extern unsigned int dim_nr_h;
extern unsigned int dim_nr_v;
extern unsigned int dim_bitmode;
extern unsigned int dim_dbg_mode;
extern unsigned int new_int;
extern u32 dbg_sct_cfg;
extern unsigned int vert_scaler_filter;
extern unsigned int vert_chroma_scaler_filter;
extern unsigned int horz_scaler_filter;
extern unsigned int pre_scaler_en;
extern unsigned int di_forc_pq_load_later;
extern unsigned int dim_dbg_dec21;
extern unsigned int dim_dbg_post;
extern unsigned int sleep_cnt;
extern unsigned int dpvpp_dbg_force_dech;
extern unsigned int dpvpp_dbg_force_decv;
extern unsigned int cue_en;
extern unsigned int cue_en_force_disable;
extern unsigned int glb_fieldck_en;
extern unsigned int invert_cue_phase;
extern unsigned int cue_pr_cnt;
extern unsigned int cue_glb_mot_check_en;
extern unsigned int cue_en_last;
extern unsigned int flm22_sure_num;
extern unsigned int flm22_ratio;
extern unsigned int pldn_cmb0;
extern int flag_di_weave;
extern unsigned int pldn_cmb1;
extern unsigned int flmxx_sure_num;
extern unsigned int flm22_glbpxlnum_rat;
extern unsigned int flm22_glbpxl_maxrow;
extern unsigned int flm22_glbpxl_minrow;
extern unsigned int cmb_3point_rnum;
extern unsigned int cmb_3point_rrat;
extern unsigned int dim_dbg_o_vtype;
extern unsigned int dim_dbg_o_b;
extern unsigned int dbg_trig_eos;
extern unsigned int flmxx_maybe_num;

extern unsigned int pr_pd;
extern int flm32_mim_frms;
extern int flm22_dif01a_flag;
extern int flm22_mim_frms;
extern int flm22_mim_smfrms;
extern int flm32_f2fdif_min0;
extern int flm32_f2fdif_min1;
extern int flm32_chk1_rtn;
extern int flm32_ck13_rtn;
extern int flm32_chk2_rtn;
extern int flm32_chk3_rtn;
extern int flm32_dif02_ratio;
extern int flm22_chk20_sml;
extern int flm22_chk21_sml;
extern int flm22_chk21_sm2;
extern int flm22_lavg_sft;
extern int flm22_lavg_lg;
/* dif02 < (size >> sft) => static */
extern int flm22_stl_sft; /*10*/
extern int flm22_chk5_avg;
extern int flm22_chk6_max;
extern int flm22_chk6_ma;
extern int flm22_anti_chk1;
extern int flm22_anti_chk3;
extern int flm22_anti_chk4;
extern int flm22_anti_ck140;
extern int flm22_anti_ck141;
extern int flm22_frmdif_max;
extern int flm22_flddif_max;
extern int flm22_minus_cntmax;
extern int flagdif01chk;
extern int dif01_ratio;
extern int nflagch4_ratio;
extern int nflagch5_ratio;
extern int nflagch4_th;
extern int nflagch5_th;
extern int dif02_flag;
extern int dif02_ratio;
extern int flm2224_stl_sft; /*10*/
extern int cmb32_blw_wnd; /*192 */
extern int cmb32_wnd_ext;/*suggest from vlsi-yanling*/
extern int cmb32_wnd_tol;
extern int cmb32_frm_nocmb;
extern int cmb32_min02_sft;
extern int cmb32_cmb_tol;
extern int cmb32_avg_dff; /* if avg dif32 > dff>>4 */
extern int cmb32_smfrm_num;
extern int cmb32_nocmb_num;
extern int cmb22_gcmb_rnum;
extern int flmxx_cal_lcmb;

void didbg_fs_init(void);
void didbg_fs_exit(void);

void di_cfgx_init_val(void);

void didbg_vframe_in_copy(unsigned int ch, struct vframe_s *pvfm);
void didbg_vframe_out_save(unsigned int ch, struct vframe_s *pvfm, unsigned int id);

/********************************
 *debug register:
 *******************************/
void ddbg_reg_save(unsigned int addr, unsigned int val,
		   unsigned int st, unsigned int bw);
void dim_ddbg_mod_save(unsigned int mod,
		       unsigned int ch,
		       unsigned int cnt);
void ddbg_sw(enum EDI_LOG_TYPE mode, bool on);

/********************************
 *time:
 *******************************/
u64 cur_to_msecs(void);
u64 cur_to_usecs(void);	/*2019*/

/********************************
 *trace:
 *******************************/
struct dim_tr_ops_s {
	void (*pre)(unsigned int index, unsigned long ctime);
	void (*post)(unsigned int index, unsigned long ctime);
	void (*pre_get)(unsigned int index);
	void (*pre_set)(unsigned int index);
	void (*pre_ready)(unsigned int index);
	void (*post_ready)(unsigned int index);
	void (*post_get)(unsigned int index);
	void (*post_get2)(unsigned int index);
	void (*post_set)(unsigned int index);
	void (*post_ir)(unsigned int index);
	void (*post_do)(unsigned int index);
	void (*post_peek)(unsigned int index);
	void (*sct_alloc)(unsigned int index, u64 timer_begin);
	void (*sct_tail)(unsigned int index, unsigned int used_cnt);
	void (*sct_sts)(unsigned int index);
	void (*self_trig)(unsigned int index);
	void (*irq_aisr)(unsigned int index);
	void (*irq_dct)(unsigned int index);
	void (*dct_set)(unsigned int index);
};

extern const struct dim_tr_ops_s dim_tr_ops;
void dbg_timer(unsigned int ch, enum EDBG_TIMER item);
void dbg_timer_clear(unsigned int ch);
void dim_dump_mif_state(struct DI_MIF_S *mif, char *name);
void dump_mif_state_seq(struct DI_MIF_S *mif,
			struct seq_file *seq);

int seq_file_dvfm(struct seq_file *seq, void *v, struct dvfm_s *pvfm);
void print_dvfm(struct dvfm_s *pvfm, char *name);
void print_mif(struct DI_MIF_S *mif, char *name);
void print_mif_simple(struct DI_SIM_MIF_S *simp_mif, char *name);
void dbg_slt_crc(struct di_buf_s *di_buf);
int print_vframe(struct vframe_s *pvfm);
int seq_file_vframe(struct seq_file *seq, void *v, struct vframe_s *pvfm);
int dpvpp_itf_show(struct seq_file *s, void *what);
void dbg_slt_crc_count(struct di_ch_s *pch, unsigned int postcrc,
		       unsigned int nrcrc, unsigned int mtncrc);

int seq_file_dvfm(struct seq_file *seq, void *v, struct dvfm_s *pvfm);
void di_attr_create(struct class *di_class);
ssize_t di_dump_yuv_show(struct file *filp, struct kobject *kobj,
	struct bin_attribute *attr, char *buf, loff_t off, size_t count);
void dump_yuv_time(struct vframe_s *vf, struct di_buf_s *di_buf);
#endif	/*__DI_DBG_H__*/
