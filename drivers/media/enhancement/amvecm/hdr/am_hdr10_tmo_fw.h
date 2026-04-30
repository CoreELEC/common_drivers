/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "../amcsc.h"

#define TMO_PARAM_CNT 163
#define LINE_SIZE 40

extern int pr_tmo_en;

int hdr10_tmo_gen(u32 *oo_gain, u32 *cgain, u32 *oo_gain1, u32 hdr_convert_mode, int hist_flag);
int hdr10_tmo_dbg(char **param);
void hdr10_tmo_parm_show(void);
void hdr10_tmo_reg_set(struct hdr_tmo_sw *pre_tmo_reg);
void hdr10_tmo_reg_get(struct hdr_tmo_sw *pre_tmo_reg_s);
void hdr10_tmo_reg_ext_set(struct hdr_tmo_sw_ext *pre_tmo_reg_ext);
void hdr10_tmo_reg_ext_get(struct hdr_tmo_sw_ext *pre_tmo_reg_ext_s);
int hdr_tmo_adb_show(char *str);
int set_hdr_tmo_reg(void);
extern struct aml_hw_reg_s pq_hw_regs;
int hdr_tmo_alg_dbg(char **parm);
int hdr_tmo_alg_dbg_show(char *str);
void hdr_first_frame_flag_update(void);
#endif
