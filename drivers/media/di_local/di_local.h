/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DI_LOCAL_H__
#define __DI_LOCAL_H__
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>

int get_current_vscale_skip_count(struct vframe_s *vf);

void dil_set_cpuver_flag(unsigned int para);

unsigned int dil_get_cpuver_flag(void);

struct di_ext_ops {
	unsigned int (*di_post_reg_rd)(unsigned int addr);
	int (*di_post_wr_reg_bits)(u32 adr, u32 val, u32 start, u32 len);
	void (*post_update_mc)(void);
	void (*post_keep_cmd_release2)(struct vframe_s *vframe);
	void (*polic_cfg)(unsigned int cmd, bool on);
	/* new interface */
	int (*new_create_instance)(struct di_init_parm parm);
	int (*new_destroy_instance)(int index);
	enum DI_ERRORTYPE (*new_empty_input_buffer)(int index, struct di_buffer *buffer);
	enum DI_ERRORTYPE (*new_fill_output_buffer)(int index, struct di_buffer *buffer);
	int (*new_get_state)(int index, struct di_status *status);
	int (*new_write)(struct di_buffer *buffer, struct composer_dst *dst);
	int (*new_release_keep_buf)(struct di_buffer *buffer);
	int (*new_get_output_buffer_num)(int index);
	int (*new_get_input_buffer_num)(int index);
	int (*pvpp_link_display)(struct vframe_s *vfm,
				    struct pvpp_dis_para_in_s *in_para, void *out_para);
	int (*pvpp_link_check_vf)(struct vframe_s *vfm);
	int (*pvpp_link_check_act)(bool interlace);
	int (*pvpp_link_sw)(bool on, bool interlace);
	u32 (*pvpp_get_ins_id)(void);
	bool (*config_crc_ic)(void);
	unsigned int (*dim_ic_sub)(void);
	unsigned int (*dim_is_link)(void);
	int (*s_bypass_ch)(int index, bool on);
	unsigned int (*get_vpu_clkb_ext)(void);
	void (*set_vpu_clkb_ext)(unsigned long clkrate);
	bool (*is_pre_link)(void);
	bool (*is_post_link)(void);
	int (*set_buffer_num)(unsigned int post, unsigned int pre);
	bool (*get_vfm_info)(struct afbcd_info *vfm_info);
	bool (*is_dct_on)(void);
};

#endif	/*__DI_LOCAL_H__*/
