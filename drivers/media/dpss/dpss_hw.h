/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_HW_H__
#define __DPSS_HW_H__

//ary add:
void hw_val_init(unsigned int src);
void hw_init_small_addr(struct PRM_DPSS_TOP *prm_top, unsigned int src);

//ori:
extern struct AA_PPS_TOP_TYPE g_nr_pps_cfg;
extern struct AA_PPS_TOP_TYPE g_nr_pps_cfg2;
extern u32 dpss_disp0_frm_cnt;

void hw_cfg_dpss_top(struct PRM_DPSS_TOP *prm_top);
void hw_cfg_dpss_buff_addr_src0_nro(struct PRM_DPSS_TOP *prm_top,
					bool sel);

void hw_cfg_dpss_inp(struct PRM_DPSS_TOP *prm_top,
	struct PRM_DPSS_INP  prm_inp);
void hw_cfg_dpss_dae(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae);
void hw_cfg_dpss_dpe(enum DPSS_WORK_MODE  dpe_mode,
		struct PRM_DPSS_TOP  *prm_top,
		struct PRM_DPSS_DPE  *prm_dpe,
		struct AA_PPS_TOP_TYPE *nr_pps_cfg);
void hw_cfg_dae_small(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae);
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
	u32 *dpe_di_frm_cnt_adj_out
);

void hw_cfg_dpss_dae_frc(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae);
void hw_cfg_dpss_dae0_frc(enum DPSS_WORK_MODE  dae_mode,
	struct PRM_DPSS_TOP  *prm_top,
	struct PRM_DPSS_DAE  *prm_dae);

void hw_irq_src0_nr_only_dae_irq(void);
void hw_check_nr_ro(int frm_idx, enum PRM_SRC_IDX dump_src_idx);
void hw_dpe_out_addr_rd(enum PRM_SRC_IDX src_idx);
int get_vfce_offset(int index);
void hw_cfg_dpss_dpe_small(enum DPSS_WORK_MODE dpe_mode,
	struct PRM_DPSS_TOP *prm_top,
	struct PRM_DPSS_DPE *prm_dpe);
void hw_irq_src0_nr_only_dpe_irq(void);
void hw_rls0_irq(void);
void hw_vdi_rls_int(void);

void check_dae_ro(int frm_idx, enum PRM_SRC_IDX dump_src_idx);
void check_di_ro(int frm_idx, enum PRM_SRC_IDX src_idx);

extern u32 dpe_pre_phs0_flg;

void hw_process_dae1_frm_rst(int cfg_seed,
				int frm_cnt0_adj,
				int frm_cnt1_adj,
				int frm_cnt2_adj,
				enum DPSS_WORK_MODE dae_nr_mode,
				bool src0_nrdi_frc_en,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DAE *prm_dae);
void hw_process_dae1_nrdi_frm_rst(int cfg_seed,
				int frm_cnt0_adj,
				int frm_cnt1_adj,
				int frm_cnt2_adj,
			bool src0_nrdi_frc_en,
			struct PRM_DPSS_TOP *prm_top,
			struct PRM_DPSS_DAE *prm_dae);

void hw_process_dpe1_frm_rst(int cfg_slc,
				int slc_num,
				int nr_frm_cnt_adj,
				int di_frm_cnt_adj,
				int dpss_nr_ignore_num,
				enum DPSS_WORK_MODE dpe_nr_mode,
				bool src0_nrdi_frc_en,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DPE *prm_dpe);
void hw_process_dpe1_nrdi_frm_rst(int cfg_slc,
				int slc_num,
				int nr_frm_cnt_adj,
				int di_frm_cnt_adj,
				int dpss_di_ignore_num,
				enum DPSS_WORK_MODE dpe_di_mode,
				bool src0_nrdi_frc_en,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DPE *prm_dpe);

void hw_process_dae1_nr_only(int cfg_seed,
				int frm_cnt0_adj,
				int frm_cnt1_adj,
				int frm_cnt2_adj,
				enum DPSS_WORK_MODE dae_nr_mode,
				bool src0_nrdi_frc_en,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DAE *prm_dae);
void hw_process_dae2_frm_rst(int cfg_seed,
			int frm_cnt0_adj,
			int frm_cnt1_adj,
			int frm_cnt2_adj,
			bool src0_nrdi_frc_en,
			struct PRM_DPSS_TOP *prm_top,
			struct PRM_DPSS_DAE *prm_dae);
void hw_process_dpe2_frm_rst(int cfg_slc,
			int slc_num,
			int nr_frm_cnt_adj,
			int di_frm_cnt_adj,
			int dpss_nr_ignore_num,
			int dpss_di_ignore_num,
			enum DPSS_WORK_MODE dpe_di_mode,
			bool src0_nrdi_frc_en,
			struct PRM_DPSS_TOP *prm_top,
			struct PRM_DPSS_DPE *prm_dpe);

void hw_process_dae0_frm_rst(int cfg_seed,
				int frm_cnt0_adj,
				int frm_cnt1_adj,
				int frm_cnt2_adj,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DAE *prm_dae);

void hw_process_inp_frm_rst(int frm_cnt_adj);
void hw_process_pre_vsync_int(void);

void irq_dae0(void);
void irq_dae1(void);
void irq_dpe1(void);
void irq_dae2(void);
void irq_dpe2(void);
void irq_inp(void);
void irq_pre_vs(void);
void irq_display(void);
void hw_dpe_update_interlace(struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DPE *prm_dpe);
void hw_dae_update_interlace(struct PRM_DPSS_TOP *prm_top,
			     struct PRM_DPSS_DAE  *prm_dae,
			     unsigned int cnt);
void afbcd_update_addr(struct PRM_DPSS_TOP *prm_top, unsigned int idx);
void hw_dae_update_interlace_cfg(struct PRM_DPSS_TOP *prm_top,
				unsigned int cnt);

void dbg_dpss_reset(unsigned int para);
void hw_cfg_dpss_dpe_no_buf_update(enum DPSS_WORK_MODE  dpe_mode,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DPE *prm_dpe);

void dpss_tsk_m_wake(unsigned int id);
void nr_only_release_input(unsigned int index);

//extern unsigned int g_dpss_in_cnt;//tmp
//extern unsigned int g_dpss_out_cnt;
//extern unsigned int g_dpss_dbg_vfm_in;
extern unsigned int fnr_dpe_obuf_rls_ini;

extern unsigned int frc_ocnt_status;
extern unsigned int dpss_dbg_game_mode;
//extern unsigned int g_dpss_tst_case;
extern u32 dpss_mc_rls_cnt;
extern u32 det_filmmode_chg;
extern struct TRANS trans;
bool dpss_get_h_bypass(void);
void hw_config_top_post(struct PRM_DPSS_TOP *prm_top);
void venc_vrr_vdin(void);
void dpss_hdr_int(unsigned int x, unsigned int y,
	unsigned int path_mode);
void dpss_dv_init(void);

//tbc:
void cfg_fnr_dae_ibuf_rdy(u64 info);
void cfg_dpss_trigger(enum DPSS_TRIGGER_MODE mode);
void cfg_fnr_dpe_ibuf_rdy(u64 info0, u64 info1);
void cfg_dpss_inp_free(u32 src_idx);
void cfg_dpss_dae0_fifo(u32 wren, u32 rden);
void cfg_dpss_dae1_fifo(u32 wren, u32 rden);
void hw_dpe_out_addr_rd_tbc(enum PRM_SRC_IDX src_idx);
void cfg_dpss_dpe_free(u32 idx);
void cfg_dpss_dpe_src1_free(void);

void hw_tbc_src0_input(struct PRM_DPSS_TOP *prm_top, unsigned int inp_src_idx);
void hw_tbc_process_dae1_frm_done(struct PRM_DPSS_TOP *prm_top,
					struct PRM_DPSS_DPE *prm_dpe);
void hw_tbc_process_dpe1_frm_done(struct PRM_DPSS_TOP *prm_top);
void hw_tbc_process_dae1_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top,
					struct PRM_DPSS_DAE *prm_dae, u32 cfg_seed,
					unsigned int dae_frm_cnt_src0_adj,
					unsigned int dae_frm_cnt_src1_adj,
					unsigned int dae_frm_cnt_src2_adj);
void hw_tbc_process_dpe1_frm_rst_tbc(struct PRM_DPSS_TOP *prm_top,
					struct PRM_DPSS_DPE *prm_dpe,
					unsigned int nr_ignore_num);
void hw_tbc_src1_input(struct PRM_DPSS_TOP *prm_top, unsigned int inp_src_idx);
void fpga_ucode_1217(struct PRM_DPSS_TOP *prm_top, unsigned int cfg0);
void dbg_tbc_trig(unsigned int step);
#ifdef FUNC_EN_DBG_TREE
void print_s_PRM_SRC_FMT_t(struct PRM_SRC_FMT *p, char *name);
#endif
void f_dpss_hw_init(struct PRM_DPSS_TOP *prm_top);

void cfg_dpss_vdi_istop(u32 di_frm_cnt, bool top);

//tmp
extern unsigned int dpss_dbg_uv_swap;
extern unsigned int dpss_dbg_h_buf_size;
extern struct PRM_INTF_TYPE g_ds_wmif1;
extern struct PRM_INTF_TYPE g_inp_yuv_rdmif;
extern bool dpss_dbg_dis_dw;
extern unsigned int dpss_o_dw;
extern unsigned int dpss_dbg_irq_ko;

void hw_cfg_dpe_size(enum DPSS_WORK_MODE dpe_mode,
				struct PRM_DPSS_TOP *prm_top,
				struct PRM_DPSS_DPE *prm_dpe,
				struct PRM_DPE_PAD dpe_pad);
void hw_cfg_dpss_dpe_intf(struct PRM_DPSS_TOP *prm_top,
		struct PRM_DPSS_DPE *prm_dpe, bool pps_en);

//called by vs:
void hw_process_disp_done_int(char ch);

//ary add
void lcevc_update_addr(unsigned int y_addr, unsigned int u_addr);

struct vframe_s *dpss_irq_get_vfm(unsigned int idx, unsigned int lab);
void dpss_dd_dae_update(struct vframe_s *vfm);	//called in irq
void dpss_dd_dpe_update(struct vframe_s *vfm);	//called in irq
void dpss_dd_update_info_by_cnt(unsigned int cnt, struct PRM_DPSS_TOP *prm_top,
	struct PRM_DPSS_DPE *prm_dpe);

#endif	/*__DPSS_HW_H__*/
