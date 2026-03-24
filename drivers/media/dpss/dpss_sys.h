/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_SYS_H__
#define __DPSS_SYS_H__
#ifdef RUN_ON_ARM

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/semaphore.h>

#endif				/* RUN_ON_ARM */
#define DEVICE_NAME		"dpss"
#define CLASS_NAME		"vpu_dpss"

#define DPSS_COUNT   1
#define DI_MAP_FLAG	0x1
#define DI_SUSPEND_FLAG 0x2
#define DI_LOAD_REG_FLAG 0x4
#define DI_VPU_CLKB_SET 0x8
#define DAE_BUF_NUM_LEFT 5

/***********************************************/
//task
struct task_dpss_m_s {		//di_task
	bool flg_init;
	struct semaphore sem;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned int status;

	unsigned int wakeup;
	unsigned int delay;
	bool exit;
	/*not use cmd */
	/*local event */
	struct kfifo fifo_cmd;
	spinlock_t lock_cmd;	/*spinlock */
	bool flg_cmd;
	struct kfifo fifo_cmd2[DPSS_CHANNEL_NUB];
	spinlock_t lock_cmd2[DPSS_CHANNEL_NUB];	/*spinlock */
	bool flg_cmd2[DPSS_CHANNEL_NUB];
	unsigned int err_cmd_cnt;
	bool shut_down_flag;
	struct completion task_done;
};

struct dpss_fcmd_s {		//dim_fcmd_s
	struct kfifo fifo;
	spinlock_t lock_w;	/*spinlock */
	spinlock_t lock_r;	/*spinlock */
	unsigned int flg_lock;
	bool flg;		/* flg for kfifo */

	bool alloc_cmd;
	bool release_cmd;
	unsigned int reg_nub;
	unsigned int reg_page;	/*size >> page_shift */
	atomic_t doing;		/* inc in send_cmd, and set 0 when thread done */
	int sum_alloc;		/* alloc ++, releas -- */
	int sum_hf_alloc;	/* alloc ++, releas -- */
	unsigned int sum_hf_psize;
	struct completion alloc_done;
};

struct dpss_mtask {		//di_mtask
	bool flg_init;
	struct semaphore sem;
	wait_queue_head_t wait_queue;
	struct task_struct *thread;
	unsigned int status;

	unsigned int wakeup;
	unsigned int delay;
	bool exit;

	struct dpss_fcmd_s fcmd[DPSS_CHANNEL_NUB];
	unsigned int err_res;	/* 0: no err; other have error */
	unsigned int err_cmd_cnt;
};

struct dpss_v_rdma_s {
	int handle;
	bool have_driver;

	bool flg_register;
	unsigned int err_cnt;	//
	unsigned int irq_src;
	unsigned int size;
	struct rdma_op_s rdma_op;
	//debug only:
	unsigned int irq_cnt;
	unsigned int undone_cnt;
	char *name;
};

struct dpss_mng_s {		//di_mng_s
	/*workqueue */
	//1016 no need struct dim_wq_s          wq;

	/*task: */
	struct task_dpss_m_s tsk;
	struct dpss_mtask mtsk;

	unsigned int reg_flg_ch;	/*for x ch reg/unreg flg */
	unsigned int reg_setting_ch;	/*ary 2020-10-10 */

	bool hw_reg_flg;	/*for di_reg_setting/di_unreg_setting */
	bool act_flg;		/*active_flag */

	bool flg_hw_int;	/*only once */

	/*2024-10-17 */
	struct dpss_ch_s ch[DPSS_CHANNEL_NUB];
	struct dpss_hw_s hw;	//to-do

	/*buffer */
	struct dpss_dd_s dd;

	struct dpss_v_rdma_s v_rdma[2];	// 0: for d's dae, 1:for d's dpe
};

struct dpss_user_para_s {
	unsigned char dpss_mode;
	unsigned char dae_nr_mode;
	unsigned char dpe_nr_mode;
	unsigned char dpe_di_mode;
	unsigned char src_mode;
	unsigned char cfg_slc;	/* ary:small /2k(1920) / 4k / 1 slc, 2 slc, and 4 slc */
};

/***********************************************/
enum DPSS_IRQ_ID {
	DPSS_IRQ_ID_PRE_VS,
	DPSS_IRQ_ID_BUF_RLS1,
	DPSS_IRQ_ID_BUF_RLS0,
	DPSS_IRQ_ID_INP,
	DPSS_IRQ_ID_DAE2,
	DPSS_IRQ_ID_DAE1,
	DPSS_IRQ_ID_DAE0,
	DPSS_IRQ_ID_DPE2,
	DPSS_IRQ_ID_DPE1,
	DPSS_IRQ_ID_DPE0,
	DPSS_IRQ_ID_RDMA,
	DPSS_IRQ_ID_DBG,
	DPSS_IRQ_ID_NUB,
};

/***********************************************/
#define FRC_ALG_VER_SIZE     64
#define RD_REG_MAX   50
#define RD_MOTION_BY_MEMC_KO   0
#define RD_MOTION_BY_VPU_ISR   1
#define RD_MOTION_BY_INP_ISR   2
#define RD_MOTION_BY_RDMA_IN   3

#define MONITOR_REG_MAX 6

enum frc_state_e {
	FRC_STATE_DISABLE = 0,
	FRC_STATE_ENABLE,
	FRC_STATE_BYPASS,
	FRC_STATE_NULL,
};

enum chip_id {
	ID_NULL = 0,
	ID_T3,
	ID_T5M,
	ID_T3X,
	ID_T6W,
	ID_T6X,
	ID_TMAX,
};

enum chip_rev {
	ID_REVA = 0,
	ID_REVB,
	ID_REVC,
	ID_REVF = 0xF,
};

enum compress_fmt_s {
	DDR_MIF,
	DDR_AFBC,
	DDR_AFRC,
	NONE,
};

enum frc_src_s {
	FROM_DEC,
	FROM_NR,
	FROM_DI,
	FROM_VDIN,
	FROM_NONE,
};

struct frc_regs_s {
	u32 addr;
	u32 value;
};

struct tool_debug_s {
	u32 reg_read;
	u32 reg_read_val;
};

struct dpss_frc_game_res_t {
	u32 width;
	u32 height;
};

struct dpss_game_n2m_t {
	u32 input_framerate;
	u32 output_framerate;
};

struct frc_monitor_s {
	u32 dbg_reg_monitor_inp;
	u32 dbg_inp_reg[MONITOR_REG_MAX];
	u32 dbg_reg_monitor_dae0;
	u32 dbg_dae0_reg[MONITOR_REG_MAX];
	u32 dbg_reg_monitor_pre_vs;
	u32 dbg_pre_vs_reg[MONITOR_REG_MAX];
	u32 dbg_reg_monitor_vs;
	u32 dbg_vs_reg[MONITOR_REG_MAX];
	u32 dbg_buf_len;
	u32 dbg_vf_monitor;
};

struct frc_ctrl_dbg_s {
	unsigned reg_tab_pr_en:2;
	unsigned align_dbg_en:1;
	unsigned clr_vbuf_ctl:1;
	unsigned mute_cfg_ctl:1;
	unsigned dbg_freq_disable:1;
	unsigned dbg_mute_disable:1;
	unsigned dbg_dur0_disable:1;
	unsigned mc_rdmif_err_reset_en:1;
	unsigned disable_io_ctrl:1;
	unsigned ui_frc_state_sel:1;
	unsigned frc_on_dly_cnt:8;	// t3x revB no used
	unsigned dbg_mvrd_mode:16;
	unsigned out_line:32;  /*ctl mc out line for user*/
	unsigned check_reg_status:16;
	unsigned frc_dae_div4:1;
};

enum frc_mc_mtx_csc_e {
	CSC_OFF = 0,
	RGB_YUV709L,
	RGB_YUV709F,
	YUV709L_RGB,
	YUV709F_RGB,
	PAT_RD,
	PAT_GR,
	PAT_BU,
	PAT_WT,
	PAT_BK,
	DEFAULT,
};

struct frc_mc_csc_set_s {
	u32 enable;
	u32 coef00_01;
	u32 coef02_10;
	u32 coef11_12;
	u32 coef20_21;
	u32 coef22;
	u32 pre_offset01;
	u32 pre_offset2;
	u32 pst_offset01;
	u32 pst_offset2;
};

struct frc_pat_dbg_s {
	u8 pat_en;
	u8 pat_type;
	u8 pat_color;
	u8 pat_reserved;
};

struct frc_debug_s {
	struct tool_debug_s tool_dbg;
//      struct frc_ud_s ud_dbg;
	struct frc_pat_dbg_s pat_dbg;
//      struct frc_timer_dbg timer_dbg;
//      struct frc_force_dbg_s force_dbg;
	struct frc_monitor_s monitor_dbg;
	struct frc_ctrl_dbg_s ctrl_dbg;
//      struct frc_probe_dbg probe_dbg;
	u8 debug_log_en_cfg[1000];
};

struct buf_s {
	u32 inp_in_order_idx;	// FRC_REG_INPUT_FUL_IDX
	u32 inp_out_order_idx;	// FRC_REG_OUT_FRAME_IDX
	u32 inp_out_w_order_idx;	// DPSS_FRC_INP_OBUF_WDATA 808E
	u32 dae_in_order_idx;	// DPSS_FBUF_DAE_FRM_IDX
	u32 dae_out_order_idx;	// FRC_DAE_FRM_IDX
	u32 dpe_order_idx;	// DPSS_FBUF_DPE_FRM_IDX

	// buf idx
	u8 inp_rotation_idx;	// FRC_REG_PAT_POINTER b7:4
	u8 dae_rotation_idx;	// FRC_DAE_IN_FRM_IDX b3:0
	u8 dpe_rotation_idx;

	u8 inp_in_to_free_cnt;	// DPSS_FRC_INP_BUF_STATUS
	u8 inp_out_to_wr_cnt;
	u8 inp_in_wr_to_cnt;
	u8 dae_wr_to_cnt;	// DPSS_FRC_DAE_BUF_STATUS
	u8 dpe_out_to_wr_cnt;	// DPSS_FRC_DPE_BUF_STATUS
	u8 dpe_in_wr_to_cnt;

	u8 buf_err;		// DPSS_FRC_BUF_ERR_FLAG b28
	u8 inp_obuf_free_err;	// b24
	u8 frc_inp_ibuf_err;	// b16
	u8 frc_inp_obuf_err;	// b12
	u8 frc_dae_ibuf_err;	// b8
	u8 frc_dpe_ibuf_err;	// b4
	u8 frc_dpe_obuf_err;	// b0
	u8 dpe_free_err;	// DPSS_FBUF_BUF_ERR_FLAG b8
	u8 dae_ibuf_err;	// b4

};

struct proc_s {
	u8 frc_inp_proc_st;	// DPSS_FRC_PROC_STATUS
	u8 frc_dae_proc_st;
	u8 frc_dpe_proc_st;
	u8 dae_proc_st;		// DPSS_FBUF_PROC_STATUS
	u8 dpe_proc_st;
	u8 dpss_ctl_idle_stats;	// FRC_DPSS_CTRL_IDLE
};

struct undone_s {
	u8 frc_inp;
	u8 frc_mevp;
	u8 frc_vp;
	u8 frc_mc;
	u16 frc_mc_undone_vcnt;
	u8 frc_aa;
	u8 dpss_inp;
	u32 dpss_inp_undo_info;
	u8 dpss_dae;
	u8 dpss_dpe;
};

struct frc_cut_win_s {
	u16 cut_win_en;
	u16 cut_win_chg;
	u32 frm_hsize;
	u32 frm_vsize;
	u32 win_hsize;
	u32 win_vsize;
	u32 win_hbgn;
	u32 win_hend;
	u32 win_vbgn;
	u32 win_vend;
};

struct disp_screen_vinfo_s {
	u32 vinfo_chg;
	u32 vtotal;
	u32 htotal;
	u32 width;
	u32 height;
	u32 frequency;
	u32 x_d_st;
	u32 y_d_st;
	u32 x_d_end;
	u32 y_d_end;
	u32 x_d_size;
	u32 y_d_size;
};

struct framerate_s {
	u16 integer;
	u16 fraction;
};

struct n2m_s {
	enum DPSS_FRC_RATIO frc_ratio;
	enum DPSS_FRC_RATIO force_frc_ratio;
	u8 input_framerate;
	u8 output_framerate;
	struct framerate_s in_fr;
	struct framerate_s out_fr;
	u16 duration;
};

struct n2m_lut_s {
	u16 input_framerate;
	u16 output_framerate;
	enum DPSS_FRC_RATIO frc_ratio;
};

struct frc_interrupt_s {
	u32 inp_int_cnt;
	u32 inp_duration;
	u64 inp_timestamp;
	u32 inp_min_duration;
	u32 inp_max_duration;

	u32 dae0_int_cnt;
	u32 dae0_duration;
	u64 dae0_timestamp;

	u32 pre_vsync_cnt;
	u32 pre_vsync_duration;
	u64 pre_vsync_timestamp;

	u32 frc_vsync_cnt;
	u32 frc_vsync_duration;
	u64 frc_vsync_timestamp;

	s16 irq_chk;
	u16 irq_chk_flag;
	u32 irq_chk_err_cnt;
};

struct me_pcn_s {
	unsigned p:4;
	unsigned c:4;
	unsigned n:4;
	unsigned use:4;
};

struct mc_disp_s {
	u8 wr_pre_idx;
	u8 wr_cur_idx;
	u8 disp_pre_idx;
	u8 disp_cur_idx;
	u8 step;
};

struct frc_work_s {
	// struct work_struct clk_work;
	struct work_struct print_work;
	// struct work_struct secure_work;
};

struct dpss_frc_vf_s {
	bool is_on;
	u32 nr_buf_id;
	struct vframe_s vfm;
	u32 sequence;
};

struct frc_vf_ctrl_s {
	u32 wr_idx;
	u32 rd_idx;
	u32 wr_seq;
	u32 rd_seq;
	u32 last_idx;
	u32 last_seq;
	bool wr_initialized;
	bool rd_initialized;
	bool last_valid;
};

struct dpss_game_dly_s {
	/* data */
	u32 vtotal;
	u32 mc_dly;
	u32 vsp_dly;
	u32 pre_dly;
	u32 vmin_num_adj;
	u32 vmax_num_adj;
};

struct dpss_game_state_s {
	bool game_n2m;
	bool game_film;
	bool frc_only_path;
	bool vdin_game_flag_en;
	bool mc_link_en;
	bool game_vrr_en;
	bool video_mute_en;
	bool do_game_frc;
	bool off_game_mode;
	bool inp_ctrl;
	bool delay_enable_game_frc;
	bool mc_need_byp_en;
	u32 vrr_drop_cnt;
	u32 video_mute_cnt;
	u32 vdin_buf_num;
	u32 game_deblur;
	u32 delay_enable_cnt;

	struct dpss_game_dly_s dly_st;
};

struct frc_state_s {
	bool is_frc_vpp_link;
	bool need_disable_mc_link;
	bool mc_link_available;
	bool have_update_vfcd;
	bool dpss_reg;
	bool frc_en;//dpss mode include frc
	bool frc_init;
	bool trig_pos_chg;
	bool check_frc_status_en;
	bool unformat_bypass;
	bool special_format;
	bool inp_alg_worked;
	bool force_n2m;
	bool ui_set_n2m;
	bool dbg_set_n2m;
	bool fast_pat_set_n2m;
	bool n2m_property_change;
	bool is_first_frame;
	bool src0_disp_obuf_rdy;
	bool mc_bypass;
	bool force_mc_phase0;
	bool need_set_phase0;
	bool mc_set_phase0;
	bool mc_phase0_rdma;//rdma wr phase0
	bool demo_win_en;
	bool mc_cut_position;
	bool mc_lp_mode;
	u8 cut_pre_mc_work;
	u8 mc_bypass_always;
	u8 use_inp_big;
	bool detect_speed;
	bool src_chg;
	bool mc_byp_switch_on;
	bool mc_byp_switch_off;
	bool mc_ini_rdma_done;
	bool use_phase0_done;
	bool first_put_vfm;
	bool force_disable_dpe_mix;
	bool force_disable_check_fallback;
	bool is_dos;
	bool dae_ready;
	bool dpe_ready;
	bool mc_last_ready;//release mc buf
	bool bypass_chg;
	u8 dpe_mix;
	u8 mv_buf_idx;
	bool win_size_zero_flag;
	bool used_seq_vfm;
	bool phs_swth;
	u8 check_fallback;
	u8 detect_threshold;
	u8 dae0_bypass_mode;
	u8 dst_buf_th;
	u8 enable_frclink_cnt;
	u8 nr_buf_idx;
	u8 force_mc_cur_idx;
	u8 mc_drop_idx;
	u8 mc_switch_adj_idx;
	u32 frc_only_rst;
	u32 mc_bypass_cnt;
	u32 force_mc_byp_cnt;
	u32 pre_vsync_offset;
	u32 enable_mc_cnt;
	u32 cur_fw_pause;
	u32 cur_frc_me_en;
	u32 dpe_intp_phs;
	unsigned int put_frame_cnt;
	enum compress_fmt_s compr_sel;
	unsigned int pre_idx;
	unsigned int cur_idx;
	unsigned int big_fmt;
	unsigned int big_bits;
	unsigned int big_plane;
	unsigned int mc_used_cnt;
	enum frc_src_s frc_src;/*2025312 not use*/
	unsigned int phs_swth_cnt;

	bool enable_last_drop; //drop last frame
	bool need_drop_dd;
	bool is_seek_bar;
	bool force_disable_drop_last;
	bool is_pause_state_last_frmae;
	u8 mc_last_idx;

	struct buf_s buf_stats;
	struct proc_s proc_stats;
	struct undone_s undone_stats;
	struct n2m_s n2m_status;
	struct frc_interrupt_s frc_int_st;
	struct me_pcn_s me_pcn_st;
	struct mc_disp_s mc_disp_st;
	struct dpss_frc_i_s *src_frc_inp[16];
	bool pps_frc;
};

struct frc_chip_st {
	enum chip_id chip;
	u8 frc_en;
	u32 encl_frc_ctrl;
	u32 dbg_vfcd_holdline;
	u32 venc_vrr_ctrl;
	u32 venc_vrr_adj_lmt;
	u32 venc_vrr_ctrl_1;
	u32 venc_ro_vrr;
//      struct frc_attribute_s *attr_st;
//      struct frc_buf_s *buf_st;
//      struct frc_clock_s *clk_st;
//      struct frc_method_s *method_st;
//      struct frc_event_s *event_st;
	struct frc_state_s state_st;
	struct frc_work_s work_st;
//      struct frc_irq_s irq_st[IRQ_MAX];
	struct frc_debug_s dbg_st;
//      struct frc_other_s other_st;
	struct VFCD_t mc_vfcd;
	struct PRM_INTF_TYPE mc_pix_rmif;
	struct PRM_INTF_TYPE pre_logo_rmif;
	struct PRM_INTF_TYPE cur_logo_rmif;
	struct PRM_INTF_TYPE mv_rmif;
	struct PRM_INTF_TYPE me_logo_rmif;
	struct DPSS_MC0_TYPE prm_mc;
	struct frc_cut_win_s win_st;
	struct disp_screen_vinfo_s vinfo_st;
	struct frc_mc_csc_set_s init_csc;
	struct dpss_game_state_s game_st;
	int fmt444_out;
};

/***********************************************/
struct dpss_dev_s {		//di_dev_s      di_dev_t
	dev_t devt;
	struct cdev cdev;	/* The cdev structure */
	struct device *dev;
	struct platform_device *pdev;
	dev_t devno;
	struct class *pclss;

	bool sema_flg;		/*di_sema_init_flag */

	struct task_struct *task;
	struct clk *vpu_clkb;
	struct clk *vpu_clk_mux;
	struct clk *vpu_clk_dpe;
	struct clk *vpu_clk_dae;
	unsigned long clkb_max_rate;
	unsigned long clkb_min_rate;
	unsigned int clk_status;
	unsigned char di_event;	//?
#ifdef C_TST_ON_T6D
	unsigned int pre_irq;
	unsigned int post_irq;
	unsigned int aisr_irq;
#endif				/* C_TST_ON_T6D */
#ifdef FUNC_EN_IRQ
#ifdef MOV
	unsigned int irq_pre_vs;
	unsigned int irq_buf_rls1;
	unsigned int irq_buf_rls0;
	unsigned int irq_inp;
	unsigned int irq_dae2;
	unsigned int irq_dae1;
	unsigned int irq_dae0;
	unsigned int irq_dpe2;
	unsigned int irq_dpe1;
	unsigned int irq_dpe0;
	unsigned int irq_rdma;
	unsigned int irq_dbg;
#endif
	unsigned int irq_q[DPSS_IRQ_ID_NUB];	/* enum DPSS_IRQ_ID */
#endif				/* FUNC_EN_IRQ */
	unsigned int flags;

	bool mem_flg;		/*ary add for make sure mem is ok */
//info:
	unsigned char info_pos;
	unsigned char info[32];
	int plane[DPSS_CHANNEL_NUB];	/*use for debugfs */
	struct dentry *dbg_root_top;	/* dbg_fs */
#ifdef FUNC_DBG_REGISTER_CAT
	struct dpss_dbg_data dbg_data;
#endif
	/***************************/
	struct dpss_mng_s mng;
	/***************************/
	struct dpss_data_s *data_l;

	struct vpu_dev_s *dim_vpu_clk_gate_dev;
	struct vpu_dev_s *dim_vpu_pd_dec;
	struct vpu_dev_s *dim_vpu_pd_dec1;
	struct vpu_dev_s *dim_vpu_pd_vd1;
	struct vpu_dev_s *dim_vpu_pd_post;

	// memc
	struct frc_chip_st *pchip_st;
	void *fw_data;
};

struct frc_latency_st {
	int in_fps;
	int out_fps;
	int delta_ms; /* offset between actual and theoretical values */
};

struct dpss_dev_s *dpss_get_devp(void);

static inline struct task_dpss_m_s *dpss_get_m_task(void)
{
	return &dpss_get_devp()->mng.tsk;
}

static inline struct dpss_mtask *dpss_get_s_task(void)
{
	return &dpss_get_devp()->mng.mtsk;
}

static inline struct dpss_data_s *dpss_get_datal(void)
{
	return (struct dpss_data_s *)dpss_get_devp()->data_l;
}

static inline struct dpss_ch_s *dpss_get_ch(unsigned int ch)
{
	if (ch >= DPSS_CHANNEL_NUB) {
		DBG_ERR("%s ch: %d\n", __func__, ch);
		return NULL;
	}
	return &dpss_get_devp()->mng.ch[ch];
}

static inline struct dpss_dd_s *dpss_get_dd(void)
{
	return &dpss_get_devp()->mng.dd;
}

static inline struct dpss_buf_sml_s *dpss_get_buf_sml(void)
{
	return &dpss_get_devp()->mng.dd.hd_sml_info;
}

static inline struct dpss_v_rdma_s *dpss_get_v_rdma_d_0(void)
{
	return &dpss_get_devp()->mng.v_rdma[0];
}

static inline struct dpss_v_rdma_s *dpss_get_v_rdma_d_1(void)
{
	return &dpss_get_devp()->mng.v_rdma[1];
}

static inline struct dpss_hw_s *dpss_get_hw(void)
{
	return &dpss_get_devp()->mng.hw;
}

static inline struct frc_chip_st *dpss_get_frc_st(void)
{
	return dpss_get_devp() ? dpss_get_devp()->pchip_st : NULL;
}

void *dpss_get_fw_data(void);

/*-------------------------*/
#endif	/*__DPSS_SYS_H__*/
