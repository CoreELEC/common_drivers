/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef __AM_LC_H
#define __AM_LC_H

#include <linux/amlogic/media/vfm/vframe.h>

/*V1.0: Local_contrast Basic function, iir algorithm, debug interface for tool*/
/*V1.1: add ioctrl load interface supprt*/
/*v2.0: add lc tune curve node patch by vlsi-guopan*/
#define LC_VER	"v2025.02.26.1(minimum ko version: v2025.02.26.1)"

enum lc_mtx_sel_e {
	INP_MTX = 0x1,
	OUTP_MTX = 0x2,
	STAT_MTX = 0x4,
	MAX_MTX
};

enum lc_mtx_csc_e {
	LC_MTX_NULL = 0,
	LC_MTX_YUV709L_RGB = 0x1,
	LC_MTX_RGB_YUV709L = 0x2,
	LC_MTX_YUV601L_RGB = 0x3,
	LC_MTX_RGB_YUV601L = 0x4,
	LC_MTX_YUV709_RGB  = 0x5,
	LC_MTX_RGB_YUV709  = 0x6,
	LC_MTX_MAX
};

enum lc_reg_lut_e {
	SATUR_LUT = 0x1,
	YMINVAL_LMT = 0x2,
	YPKBV_YMAXVAL_LMT = 0x4,
	YPKBV_RAT = 0x8,
	YMAXVAL_LMT = 0x10,
	YPKBV_LMT = 0x20,
	YPKBV_SLP_LMT = 0x40,
	CNTST_LMT = 0x80,
	MAX_REG_LUT
};

struct lc_alg_param_s {
	unsigned int dbg_parm0;
	unsigned int dbg_parm1;
	unsigned int dbg_parm2;
	unsigned int dbg_parm3;
	unsigned int dbg_parm4;
};

extern int lc_en;
extern struct ve_lc_curve_parm_s lc_curve_parm_load;
extern int lc_curve_isr_defined;
extern int use_lc_curve_isr;
extern int lc_change_curve_nodes_en;
extern int lc_demo_mode;
/*for debug*/
extern int lc_dbg_flag;
extern enum lc_reg_lut_e reg_sel;
extern int lc_temp;
extern char lc_dbg_curve[100];
extern int amlc_debug;
extern int lc_skip_iir;
extern int lc_force_ctrl;
extern int alpha1;
extern int lc_pattern_detect_log1;
extern int lc_read_curve_nodes_changed_en;
extern int lc_change_curve_nodes_en;
extern int pre_input_422_flag;

void lc_init(int bitdepth);
void lc_process(struct vframe_s *vf,
		struct vpq_size_s *pvpq_size,
		int vpp_index,
		struct vpp_hist_param_s *vp);
void lc_free(void);
void lc_read_region(int blk_vnum, int blk_hnum, int slice);
void lc_disable(int rdma_mode, int vpp_index);
bool lc_curve_ctrl_reg_set_flag(unsigned int addr);
void lc_prt_curve(void);
int get_lc_pattern_gain(void);
void lc_change_pattern_curve_nodes(int lc_gain,
	int blk_hnum, int blk_vnum);
void lc_load_curve(struct ve_lc_curve_parm_s *p);
void lc_rd_reg(enum lc_reg_lut_e reg_sel, int data_type, char *buf);
void lc_slt_en(int enable);
int lc_debug_store(char **parm);
#endif
#endif
