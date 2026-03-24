/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __FRC_COMMON_X_H__
#define __FRC_COMMON_X_H__
#include <linux/amlogic/media/frc/frc_common.h>

#define DPSS_FRC_ALG_VER_SIZE     64

enum frc_log_level_t {
	FRC_LOG_LEVEL_INFO  = 0,  // normal
	FRC_LOG_LEVEL_DEBUG = 1,  // debug
	FRC_LOG_LEVEL_WARN,       // warning
	FRC_LOG_LEVEL_ERROR,      // err
	FRC_LOG_LEVEL_NONE        // close info
};

enum frc_module_t {
	DPSS_FRC_TOP		 = 0x00,
	DPSS_FRC_INP		 = 0x01,
	DPSS_FRC_DAE		 = 0x02,
	DPSS_FRC_MC			 = 0x03,
	DPSS_FRC_DPE		 = 0x04,
	DPSS_FRC_GAME		 = 0x05,
	DPSS_FRC_OTHER		 = 0x06,
	DPSS_DRV_FIFO		 = 0x07,
	DPSS_CORE_DISP		 = 0x08,
	DPSS_VDIN_INF		 = 0x09,

	FRC_ALG_BBD			 = 100,
	FRC_ALG_FILM		 = 200,
	FRC_ALG_LOGO		 = 300,
	FRC_ALG_ME			 = 400,
	FRC_ALG_MC			 = 500,
	FRC_ALG_SCENE		 = 600,
	FRC_ALG_VP			 = 700,
	FRC_ALG_GLB			 = 800,
	DPSS_MODULE_MAX		 = 999, // max
};

#define FRC_MODULE_LOG(module, level, fmt, ...) \
	do { \
		if (level >= frc_module_log_level[module]) { \
			pr_info("[%s]: " fmt, frc_module_name[module], ##__VA_ARGS__); \
		} \
		else if (level >= frc_module_log_level[module] && ((module >> 8) > 0)) { \
			pr_info("[%s]: " fmt, frc_module_name[module], ##__VA_ARGS__); \
		} \
	} while (0)

#define PR_ERR(fmt, args ...)  pr_info("frc_Err: " fmt, ##args)
#define PR_FRC(fmt, args ...)  pr_info("frc: " fmt, ##args)

#define frc_dbg(module, fmt, ...)   FRC_MODULE_LOG(module, FRC_LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define frc_info(module, fmt, ...)  FRC_MODULE_LOG(module, FRC_LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define frc_warn(module, fmt, ...)  FRC_MODULE_LOG(module, FRC_LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define frc_error(module, fmt, ...) FRC_MODULE_LOG(module, FRC_LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

extern enum frc_log_level_t frc_module_log_level[DPSS_MODULE_MAX];
extern const char *frc_module_name[DPSS_MODULE_MAX];

enum dpss_frc_ratio_mode_type {
	FRC_DPSS_RATIO_1_1	= 0,
	FRC_DPSS_RATIO_1_2	= 1,
	FRC_DPSS_RATIO_2_3	= 2,
	FRC_DPSS_RATIO_2_5	= 3,
	FRC_DPSS_RATIO_5_6	= 4,
	FRC_DPSS_RATIO_5_12	= 5,
	FRC_DPSS_RATIO_2_9	= 6,
	FRC_DPSS_RATIO_2_15	= 7,
	FRC_DPSS_RATIO_1_4	= 8,
	FRC_DPSS_RATIO_1_5	= 9,
	FRC_DPSS_RATIO_1_6	= 10,
	FRC_DPSS_RATIO_1_3	= 11,
	FRC_DPSS_RATIO_3_5	= 12,
	FRC_DPSS_RATIO_3_10	= 13,
	FRC_DPSS_RATIO_4_5	= 14,
	FRC_DPSS_RATIO_1_8	= 15,
	FRC_DPSS_RATIO_1_10	= 16,
	FRC_DPSS_RATIO_1_12	= 17,
	FRC_DPSS_RATIO_1_20	= 18
};

struct dpss_frc_top_type_s {
	/*input*/
	u16       hsize;
	u16       vsize;
	u16       mc_hsize;
	u16       mc_vsize;

	u16       crop_top;
	u16       crop_left;
	u16       crop_bottom;
	u16       crop_right;

	u32       frc_fb_num; //buffer num for frc loop
	enum frc_ratio_mode_type frc_ratio_mode;
	enum en_drv_film_mode       film_mode;//film_mode
	u32       film_hwfw_sel;//0:hw 1:fw
	u8        is_me1mc4;//1: me:mc=1/4, 0 : me:mc=1/2, default 0
	u8        panel_res;
	u16       other_set;
	u8        memc_loss_en;// bit0, mcloss, bit1:meloss, bit4:mcdw_loss
	u8        chip;   // 1:T3, 2:T5M
	u16       other_set1;
	u8        rdma_en; //1:rdma 0:cpu interrupt access reg
	u8        motion_ctrl;  // for frc motion ctrl
	u8        need_dpe_mix; //t6w badedit
	u8        dpe_mix_cnt; //t6w badedit
	u32       frc_prot_mode;//0:memc prefetch acorrding mode frame 1:memc prefetch 1 frame
	u32       force_en;    // for debug
	u32       in_out_ratio;
	u8        frc_input_n;
	u8        frc_output_m;

	/*output*/
	u32 out_hsize;
	u32 out_vsize;
	u8  frc_memc_level;
	u8  frc_memc_level_1; // default 0, 1:fullback, 2:24P film
	u8  frc_other_reserved;
	u8  frc_out_frm_rate;  // = frc_other_reserved;
	u8  frc_in_frm_rate;
	u16 video_duration;
	u8  frc_deblur_level;
	u8  frc_reserved;
	u8  src_uv_swap;
	u8  src_swap_64_bit;
	u8  src_little_endian;
	// t6w
	u8        dpss_mode;
	u8        demo_style;
	unsigned  frc_fw_working:1;
	unsigned  me_glb_clr_bypass_mc:1;// ME_GLB_CLR_BYPASS_MC
	unsigned  badedit_en:1;
	unsigned  frc_ds_scale_en:1;
	unsigned  frc_rev_mode:2;
	unsigned  auto_align_en:1;
	unsigned  align_hmode:1;
	unsigned  align_vmode:1;
	unsigned  me_mc_link_en:1;
	unsigned  fw_pause:1;
	unsigned  afrc_cmp_en:1;
	unsigned  mc_auto_en:1;
	unsigned  disp_pat_en:1;
	unsigned  game_mode:2; // 0:n2m, 1:film
	unsigned  mc_cut_en:1;
	unsigned  mc_bypass:2;
	unsigned  src_is_1080p_nods:1;
	unsigned  dpss_nr_byp:1;
	unsigned  frc_fbuf_only:1;
	unsigned  force_mix:1; // TEST_FORCE_MIX
	unsigned  mc_skip_mode:2;
	unsigned  memc_enable:2;
};

struct dpss_frc_fw_alg_ctrl_s {
	u8 frc_algctrl_u8vendor;  // vendor information
	u8 frc_algctrl_u8mcfb;
	u8 frc_algctrl_u8param3;
	u8 frc_algctrl_u8param4;
	u8 frc_algctrl_u8param5;
	u8 frc_algctrl_u8param6;
	u8 frc_algctrl_u8param7;
	u8 frc_algctrl_u8param8;
	s16 frc_algctrl_s16param1; //gmv_x
	s16 frc_algctrl_s16param2; //gmv_y
	u32 frc_algctrl_u32film;
	u32 frc_algctrl_u32param2;
	// t6w
	unsigned frc_me_en:1;
	unsigned vp_en:1;
	unsigned melogo_en:1;
	unsigned iplogo_en:1;
	unsigned bbd_en:1;
	unsigned dcntr_en:1;
	unsigned mv_seed_update:1;  //for rnd_mv
};

struct dpss_frc_fw_data_s {
	/*frc top type config*/
	u8 frc_alg_ver[FRC_ALG_VER_SIZE];
	struct dpss_frc_top_type_s frc_top_type;
	struct frc_holdline_s holdline_parm;
	struct dpss_frc_fw_alg_ctrl_s  frc_fw_alg_ctrl;
	struct frc_reg_val reg_val[RD_REG_MAX];

	void (*frc_input_cfg)(struct dpss_frc_fw_data_s *pfw_data);
	void (*frc_memc_level)(struct dpss_frc_fw_data_s *pfw_data);
	ssize_t (*frc_alg_dbg_show)(struct dpss_frc_fw_data_s *pfw_data,
					enum efrc_memc_dbg_type dbg_type, char *buf);
	ssize_t (*frc_alg_dbg_stor)(struct dpss_frc_fw_data_s *pfw_data,
					enum efrc_memc_dbg_type dbg_type, char *buf, size_t count);
	void (*frc_fw_reinit)(struct dpss_frc_fw_data_s *pfw_data);
	void (*frc_fw_ctrl_if)(struct dpss_frc_fw_data_s *pfw_data);

	// below t6w ...
	u32 (*inp_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
	void (*dae_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
	void (*dpe_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
	void (*pre_vsync_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
	void (*vsync_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
	void (*buf_rls0_irq_handler)(struct dpss_frc_fw_data_s *pfw_data);
};

//extern int frc_dbg_en;
//extern int frc_kerdrv_ver;
#define QUEEN_NUM 60

struct Vpu_queue {
	int data[QUEEN_NUM];
	int front;
	int rear;
};

struct memc_gmv_s {
	s16 gmv_x;
	s16 gmv_y;
};

struct game_frc_s {
	unsigned int game_frc_lock_en;
	unsigned int vdin_stop_dec;
};

void DPSS_WRITE_FRC_REG(unsigned int reg, unsigned int val);
void DPSS_WRITE_FRC_REG_BY_CPU(unsigned int reg, unsigned int val);

void DPSS_WRITE_FRC_BITS(unsigned int reg, unsigned int value,
			unsigned int start, unsigned int len);
unsigned int DPSS_READ_FRC_REG(unsigned int reg);
// #define UPDATE_FRC_REG_BITS(addr, val, mask) FRC_RDMA_VSYNC_REG_UPDATE(addr, val, mask)
void DPSS_UPDATE_FRC_REG_BITS(unsigned int reg,
		unsigned int value, unsigned int mask);
void DPSS_RDMA_WR_PRE_VS(u32 addr, u32 val);
void DPSS_RDMA_WR_VS(u32 addr, u32 val);

inline u32 floor_rs(u32 ix, u32 rs);
inline u32 ceil_rx(u32 ix, u32 rs);
inline s32  negative_convert(s32 data, u32 fbits);
inline void frc_config_reg_value(u32 need_val, u32 mask, u32 *reg_val);
void vdin_game_frc_notify(struct game_frc_s *frc_data);
bool get_dpss_game_frc_locked(void);
void rx_game_frc_notify(void);

#endif
