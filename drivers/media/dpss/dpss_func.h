/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_FUNC_H__
#define __DPSS_FUNC_H__

/* interface for linux: probe ... */
void dpss_cfg_top_dts(struct dpss_dev_s *dpss_pdev);
bool dpss_s_prob(struct dpss_dev_s *dpss_pdev);
void dpss_pch_init(void);
void dpss_check_st(struct dpss_ch_s *pch, unsigned int pos);
void dpss_mem_prob(struct dpss_dev_s *dpss_pdev);
void dpss_s_remove(struct dpss_dev_s *dpss_pdev);
void dpss_mem_remove(struct dpss_dev_s *dpss_pdev);
void dpss_exit(void);
void dpss_s_shutdown(struct dpss_dev_s *dpss_pdev);
void dpss_s_suspend(struct dpss_dev_s *dpss_pdev);
void dpss_s_resume(struct dpss_dev_s *dpss_pdev);
void dpss_s_freeze(struct dpss_dev_s *dpss_pdev);
void dpss_s_restore(struct dpss_dev_s *dpss_pdev);
void dpss_s_thaw(struct dpss_dev_s *dpss_pdev);
void dpss_irq_set_affinity_hint(struct dpss_dev_s *dpss_pdev);
//pre-alloc
void dpss_pre_buf_prob(void);

/* interface for interrupt */
#ifdef C_TST_ON_T6D
irqreturn_t dpss_pre_irq(int irq, void *dev_instance);
irqreturn_t dpss_post_irq(int irq, void *dev_instance);
irqreturn_t dpss_aisr_irq(int irq, void *dev_instance);
#endif /* C_TST_ON_T6D */

#ifdef FUNC_EN_IRQ
irqreturn_t dpss_irq_pre_vs(int irq, void *dev_instance);
irqreturn_t dpss_irq_buf_rls1(int irq, void *dev_instance);
irqreturn_t dpss_irq_buf_rls0(int irq, void *dev_instance);
irqreturn_t dpss_irq_inp(int irq, void *dev_instance);
irqreturn_t dpss_irq_dae2(int irq, void *dev_instance);
irqreturn_t dpss_irq_dae1(int irq, void *dev_instance);
irqreturn_t dpss_irq_dae0(int irq, void *dev_instance);
irqreturn_t dpss_irq_dpe2(int irq, void *dev_instance);
irqreturn_t dpss_irq_dpe1(int irq, void *dev_instance);
irqreturn_t dpss_irq_dpe0(int irq, void *dev_instance);
irqreturn_t dpss_irq_rdma(int irq, void *dev_instance);
irqreturn_t dpss_irq_dbg(int irq, void *dev_instance);
#endif

extern unsigned int dpss_en_rdma;

/* interface for task */
void dpss_tsk_s_polling(void);
void dpss_tsk_m_polling(void);

void dpss_tsk_m_delay(unsigned int val);
void dpss_tsk_m_wake(unsigned int id);
void dpss_tsk_s_wake(void);

/*alloc:*/
bool dpss_mm_alloc_api2(struct dpss_mem_a_s *in_para, struct dpss_mm_s *o);
bool dpss_mm_release_api2(struct dpss_mem_r_s *in_para);

/* interface for ch reg/unreg and called by other thread */
/* step 1: init data */
bool dpss_api_init_data(struct dpss_ch_s *pch);
/* step 2: trig reg in thread */
bool dpss_api_reg(struct dpss_ch_s *pch);

void itf_polling_in_o(struct dpss_ch_s *pch);
//when put ready buffer, call this function.
void itf_vfm_path_ready_notify(struct dpss_ch_s *pch);
void itf_vfm_path_init(void);
void itf_vfm_path_exit(void);

void itf_clear_caller_data(struct dpss_ch_s *pch, struct vframe_s *vfm);

/* vframe control */
bool dpss_vfm_2_subvf(struct dpss_sub_vf_s *vfms, struct vframe_s *vfm);
bool vfm_from_subvf(struct vframe_s *vfm, struct dpss_sub_vf_s *vfms);

unsigned int dpss_sub_vf_check(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfms);
void dpss_vfm_cp(struct vframe_s *vfm_t, struct vframe_s *vfm_f);

u8 *dpss_vmap(ulong addr, u32 size, bool *bflg);
void dpss_unmap_phyaddr(u8 *vaddr);
void dpss_frc_secure_proc(bool type, unsigned int addr, unsigned int size); //nr_frc_0113

/* interface for q */
bool dpss_q_in_idx(struct dpss_fifo_s *q, unsigned char val);
struct vframe_s *dpss_q_out_vfm(struct dpss_ch_s *pch, unsigned int q_id);
struct vframe_s *dpss_q_peek_vfm(struct dpss_ch_s *pch, unsigned int q_id);
bool dpss_q_in_vfm(struct dpss_fifo_s *q, struct vframe_s *vfm);
void dpss_in_rck_in(struct dpss_ch_s *pch, struct vframe_s *vfm);

unsigned char lk_idx_get(struct vframe_s *vfm);
struct dpss_nr_i_s *lk_get_nr_i(struct vframe_s *vfm);
struct dpss_frc_i_s *lk_get_frc_i(struct vframe_s *vfm);
unsigned char lk_owner_get(struct vframe_s *vfm);

/* working interface */
//work when reg_s2
void dpss_s2_work(struct dpss_ch_s *pch);
void dpss_s2_parser_input_new(struct dpss_ch_s *pch);
void dpss_s2_parser_input_frc(struct dpss_ch_s *pch);
void dpss_h_parser_nr(struct dpss_ch_s *pch);
void dpss_hw_disable_one_play(struct dpss_ch_s *pch);
void nr_unreg_hw(struct dpss_ch_s *pch);
void dpss_h_parser_frc(struct dpss_ch_s *pch);
//-----
void dpss_h_b_int(struct dpss_ch_s *pch);
//-----
void dpss_s2_recycle_back(struct dpss_ch_s *pch);
void dpss_s2_recycle_back_frc(struct dpss_ch_s *pch);
void dpss_s2_parser_rd_new(struct dpss_ch_s *pch);
void dpss_s2_parser_rd_nr_only(struct dpss_ch_s *pch);
void dpss_s2_parser_rd_frc(struct dpss_ch_s *pch);

void dpss_s_reg(struct dpss_ch_s *pch);
/* unreg_step 1: trig internal unreg flow. */
void dpss_s_unreg_step1(struct dpss_ch_s *pch);
bool dpss_s_unreg_wait_hw_stop(struct dpss_ch_s *pch);
void dpss_s_unreg_step2(struct dpss_ch_s *pch);

/* rdma */
void dpss_rdma_isr(void);
void dpss_ram_tab_init(struct dpss_ch_s *pch);
void dbg_rdma_process(u32 *para, u32 cnt);
void tst_0_check_rmda_ch(void);

void dpss_rdma_probe(void);
int dpss_rdma_register(struct rdma_op_s *rdma_op, void *op_ara,
			unsigned int mbuf_num,
			unsigned int buf_step,
			unsigned int rdma_mode);

void dpss_rdma_unregister(int handle);
void dpss_rdma_para_chg_irq_src(int handle, unsigned int irq_src_bit);
void dpss_rdma_para_chg_used_nub(int handle, unsigned int used_nub);
void dpss_rdma_irq_src_clear(int handle);
void dpss_pre_tab_add_w(struct dpss_rdma_pre_s *pre_tab, u32 addr, u32 val);
void dpss_pre_tab_add_w_bits(struct dpss_rdma_pre_s *pre_tab, u32 addr,
		u32 val, u32 start, u32 len);
void dpss_pre_tab_fill_null(struct dpss_rdma_pre_s *pre_tab, u32 nub);
int dpss_rdma_mbuf_fill_buf(int handle, int mbuf_index,
			struct dpss_rdma_pre_s *pre_tab);
int dpss_rdma_mbuf_config(int handle);
void dpss_rdma_count_shift_val_ot(unsigned int inv, u8 *shift, u8 *val, u8 *other);

void dpss_pre_tab_add_w_dpss(u32 addr, u32 val);
void dpss_pre_tab_add_w_vc(u32 addr, u32 val);
void dpss_pre_tab_add_wb_dpss(u32 addr, u32 val, u32 start, u32 len);
void dpss_pre_tab_add_wb_vc(u32 addr, u32 val, u32 start, u32 len);

void dpss_rdma_set_v_pre_tab(struct dpss_rdma_pre_s *pre_tab);
void dpss_rdma_set_d_pre_tab(struct dpss_rdma_pre_s *pre_tab);
void dpss_v_rdma_m_cp_tab(struct dpss_rdma_pre_s *pre_tab, unsigned int m_idx, int handle);
void dpss_v_rdma_cp_tab(struct dpss_rdma_pre_s *pre_tab, int handle);
void dpss_pre_tab_chg(struct dpss_rdma_pre_s *pre_tab, u32 addr, u32 val);
void dpss_pre_tab_chg_bits(struct dpss_rdma_pre_s *pre_tab,
			u32 addr, u32 val, u32 start, u32 len);
void dpss_pre_tab_append(struct dpss_rdma_pre_s *pre_tab, struct dpss_rdma_pre_s *sub);

void dpss_rdma_pre_set(struct dpss_ch_s *pch);
void dpss_rdma_pre_set_all(struct dpss_ch_s *pch);
void sprint_rdma_s_data_top(struct seq_file *s, unsigned int para);
void sprint_rdma_info(struct seq_file *s, unsigned int para);

//vpp rdma:
void dpss_v_rdma_remove(void);
void dpss_v_rdma_prob(void);
void dpss_v_rdma_config_a(void);
void dpss_v_rdma_config_b(void);
void dpss_dbg_v_rdma_step(unsigned int para);

//rdma v interface for module:
int DPSS_A_WR_MPEG_REG(u32 adr, u32 val);
int DPSS_A_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 DPSS_A_RD_MPEG_REG(u32 adr);
int DPSS_B_WR_MPEG_REG(u32 adr, u32 val);
int DPSS_B_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 DPSS_B_RD_MPEG_REG(u32 adr);
unsigned int dpss_dbg_level_get(void);

/*dbg */
void dpss_dbg_level_set(unsigned int level);
bool dpss_dbg_is_run(void);
u64 dpss_cur_to_msecs(void);
u64 dpss_cur_to_usecs(void);
void dpss_info_reg(char *name);
void dpss_dbg_v_rdma_step(unsigned int para);

#ifdef DBG_TEST_SAVE_VFM
ssize_t dump_store(struct device *dev, struct device_attribute *attr,
	       const char *buf, size_t len);
#endif /* DBG_TEST_SAVE_VFM*/

#ifdef DBG_TEST_CREATE
bool dbg_crt_r_is_sv_in(void);
void dbg_crt_set_vf(struct vframe_s *vf);
int dbg_crt_do_save_vf(struct vframe_s *vf);
int dbg_crt_do_polling(void);
unsigned long dbt_crt_get_blk_addr(void);
unsigned int dbg_crt_get_buf_size(void);
unsigned int dbg_crt_get_y_size(void);

#endif /* DBG_TEST_CREATE */

bool dpss_cfgx_get(unsigned int ch, enum EDPSS_CFGX_IDX idx);
unsigned char dpss_cfgx_getc(unsigned int ch, enum EDPSS_CFGX_IDX idx);

/* buffer */
void dpss_buf_infor(struct dpss_ch_s *pch);
void dpss_buf_infor_4k(struct dpss_ch_s *pch);
void dpss_buf_reg(struct dpss_ch_s *pch);
void dpss_buf_unreg(struct dpss_ch_s *pch);

void dpss_rdma_pre_tab_reg(struct dpss_ch_s *pch);
void dpss_rdma_pre_tab_unreg(struct dpss_ch_s *pch);

void d_bset(unsigned int *p, unsigned int bitn);
void d_bclr(unsigned int *p, unsigned int bitn);
bool d_bget(unsigned int *p, unsigned int bitn);

//dbg for dpss nr:
void dbg_step1_hw_init(unsigned int ch);
void dbg_step2_hw_top(unsigned int step);
void dbg_step3_int_dae(unsigned int step);
void dbg_step4_int_dpe(unsigned int step);
void dbg_step0_trig_input(unsigned int step);
void dpss_trig_input_src1(unsigned int idx);
void dpss_dbg_peek(unsigned int ch);

void dbg_dpss_reset(unsigned int para);
void hw_init_part1(struct vframe_s *vfrm);
//move to hw: void f_dpss_hw_init(struct PRM_DPSS_TOP *prm_top);

void nr_only_int(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs, struct vframe_s *vf);
bool nr_only_input_buf(struct dpss_ch_s *pch, struct dpss_nr_i_s *nr_i);
bool nr_only_rd(struct dpss_ch_s *pch, unsigned int *idx);
void nr_h_wait_mode(struct dpss_ch_s *pch);
unsigned int hw_check_rd_nub(unsigned int src_mode);
void hw_release_buf(unsigned int src_mode, unsigned int idx, bool tbc_mode);
void hw_cfg_addr_dio(struct dpss_ch_s *pch,
	unsigned int in_cnt, struct vframe_s *vfm);
void dpss_di_front_bypass(struct dpss_ch_s *pch, struct dpss_nr_i_s *nr_i);

struct dpss_ch_s *dpss_get_crr_ch(void);
void nr_only_release_input(unsigned int index);
void dpss_dis_idx(struct dpss_ch_s *pch, unsigned int idx);
void dpss_dis_vfm(struct dpss_ch_s *pch, struct vframe_s *vfm);
struct vframe_s *dpss_irq_get_vfm(unsigned int idx, unsigned int lab);
void nr_dpe_dw_cfg(struct PRM_DPSS_TOP *prm_top);
struct vframe_s *dpss_get_vfm(unsigned int ch, unsigned int mode, unsigned int idx);
void dbg_hdr_int(unsigned int ch);
void dbg_tbc_trig(unsigned int step);
void dpss_dbg_reg_itf(unsigned int step);
//void print_s_PRM_SRC_FMT_t(struct PRM_SRC_FMT *p, char *name);
void nr_unreg_val(struct dpss_ch_s *pch);//ary use to clean frc print in vs

//extern const struct afbc_op_s afbc_op;
void dpss_afbc_info_cnt(unsigned int width, unsigned int height, struct afbcd_buf_inf_s *inf);
unsigned int dpss_afbc_int_tab(unsigned long tab_addr, unsigned long buf_addr,
			unsigned int size_tab, unsigned int size_buf);
void afrc_info_cnt(unsigned int w,
		   unsigned int h,
		   unsigned int tgt_b0,// 20 ~ 48, ag: 4
		   unsigned int tgt_b1,
		   unsigned int type_in, //0:yuv444, 1:yuv422, 2: yuv 420
		   struct afrc_buf_inf_s *inf);

unsigned int dpss_uint_up_bits(unsigned int val_ori, unsigned int value,
			    unsigned int start, unsigned int len);
bool dpss_is_h_first_ch(struct dpss_ch_s *pch);
void dpss_val_user(struct dpss_ch_s *pch,
	struct dpss_user_para_s *prm_user, struct dpss_sub_vf_s *vfs);
void dpss_val_user_frc(struct dpss_ch_s *pch,
	struct dpss_user_para_s *prm_user, struct dpss_sub_vf_s *vfs);

void _prm_top_input_afbcd_buffer_set(struct PRM_DPSS_TOP *prm_top,
					unsigned int src,
					unsigned int idx,
					struct dpss_sub_vf_s *vfms);
void _prm_top_input_buffer_set(struct PRM_DPSS_TOP *prm_top,
					unsigned int src,
					unsigned int idx,
					struct dpss_sub_vf_s *vfms);

void dpss_val_2_top(struct PRM_DPSS_TOP *prm_top,
			   struct dpss_user_para_s *prm_user);
void _prm_top_init_buffer(struct PRM_DPSS_TOP *prm_top,
				 struct dpss_ch_s *pch, unsigned int src);
void _prm_top_init_vfm(struct dpss_ch_s *pch, struct PRM_DPSS_TOP *prm_top,
				struct dpss_sub_vf_s *vfms, bool is_ex_di);	//user case
void nr_dpe_pps_para(struct dpss_ch_s *pch,
	unsigned int width, unsigned int height);
void nr_dpe_pps_cfg(struct PRM_DPSS_TOP *prm_top, struct dpss_ch_s *pch,
	struct dpss_sub_vf_s *vfms);
inline u32 update_reg_val(u32 reg_val, u32 val, u8 start, u8 len);

void dpss_hdr_sw(bool sw, struct vframe_s *vfm);
void dpss_hdr_proc(struct vframe_s *vfm);

//frc rdma
void dpss_frc_rdma_reg(void);
void dpss_frc_rdma_unreg(void);
//frc game
int dpss_get_game_lock_st(struct dpss_status *state);
void venc_vrr_vdin(void);

#endif	/*__DPSS_FUNC_H__*/
