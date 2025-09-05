/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_FRC_DBG_H__
#define __DPSS_FRC_DBG_H__

extern const char *const frc_state_ary[];
extern u32 g_input_hsize;
extern u32 g_input_vsize;
extern int frc_dbg_en;
extern int frc_dbg_ctrl;
extern int frc_enable_cnt;
extern int frc_disable_cnt;
extern int frc_re_cfg_cnt;
extern unsigned int dpss_dbg_dae_dpe_cfg;	//bit 0 for dae  set 4 by console
extern int frc_delay_dbg; // for avsync debug

#define FRC_CHK_REG_ARRAY_LEN     300

ssize_t dpss_frc_reg_show(const struct class *class, const struct class_attribute *attr,
			  char *buf);
ssize_t dpss_frc_reg_store(const struct class *class, const struct class_attribute *attr,
			   const char *buf, size_t count);
ssize_t dpss_frc_tool_debug_show(const struct class *class,
				 const struct class_attribute *attr, char *buf);
ssize_t dpss_frc_tool_debug_store(const struct class *class,
				 const struct class_attribute *attr, const char *buf,
				  size_t count);
ssize_t dpss_frc_debug_show(const struct class *class, const struct class_attribute *attr,
			    char *buf);
ssize_t dpss_frc_debug_store(const struct class *class, const struct class_attribute *attr,
			     const char *buf, size_t count);
ssize_t dpss_frc_buf_show(const struct class *class, const struct class_attribute *attr,
			  char *buf);
ssize_t dpss_frc_buf_store(const struct class *class, const struct class_attribute *attr,
			   const char *buf, size_t count);
ssize_t dpss_frc_param_show(const struct class *class, const struct class_attribute *attr,
			    char *buf);
ssize_t dpss_frc_param_store(const struct class *class, const struct class_attribute *attr,
			     const char *buf, size_t count);
ssize_t dpss_frc_other_show(const struct class *class, const struct class_attribute *attr,
			    char *buf);
ssize_t dpss_frc_other_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count);
ssize_t dpss_frc_probe_dbg_show(const struct class *class,
				const struct class_attribute *attr, char *buf);
ssize_t dpss_frc_probe_dbg_store(const struct class *class,
				const struct class_attribute *attr, const char *buf,
				size_t count);
ssize_t dpss_frc_dump_reg_table_show(const struct class *class,
			const struct class_attribute *attr, char *buf);
ssize_t dpss_frc_dump_reg_table_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count);
ssize_t dpss_frc_rdma_show(const struct class *class,
			const struct class_attribute *attr, char *buf);
ssize_t dpss_frc_rdma_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count);
ssize_t dpss_frc_game_show(const struct class *class,
			const struct class_attribute *attr, char *buf);
ssize_t dpss_frc_game_store(const struct class *class,
			const struct class_attribute *attr, const char *buf, size_t count);

void dump_vfcd_reg_t6w(void);
void dump_param_frc_init_apb(void);
void dump_src0_buf_regs(void);
void dump_frc_pd_regs_apb(void);
void dump_iplogo_regs_apb(void);
void dump_me_logo_regs_apb(void);
void dump_param_vp_init_apb(void);
void dump_param_mc_init_apb(void);
void dump_me_regs_apb(void);
void dump_vfcd_page(void);

#endif
