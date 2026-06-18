/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_HW_HH
#define VIDEO_HW_HH

#define FGRAIN_TBL_SIZE  (498 * 16)

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
void vpp_secure_cb(u32 arg);
#endif
void int_vpu_delay_work(void);
void vd1_set_go_field(void);
void check_video_mute(void);
bool get_video_mute_val(u32 owner);
void vppx_vdx_mux_set(void);
int get_video_reg_table(u32 *check_item);
#ifdef CONFIG_AMLOGIC_ZAPPER_CUT
void get_video_mute_info(void);
int get_video_reg_table_cut(u32 *check_item);
int set_video_mute_info(u32 owner, bool on, bool use_vcbus);
#else
int set_video_mute_info(u32 owner, u32 video_layer,
		bool on, bool use_vcbus);
void get_video_mute_info(u32 video_layer);

#endif
void clear_vpu_venc_error(void);
u32 get_vpu_venc_error_status(void);

void set_probe_ctrl_a4(u8 probe_id);
u32 get_probe_pos_a4(u8 probe_id);
void set_probe_pos_a4(u32 val_x, u32 val_y);
u32 get_probe_data_a4(void);

#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
void vd_vpp_toggle_cur_vf(struct vframe_s *vf, int layer_index);
#endif

#endif
