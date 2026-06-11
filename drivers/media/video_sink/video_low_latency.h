/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_LOW_LATENCTY_HH
#define VIDEO_LOW_LATENCTY_HH
#include <linux/platform_device.h>
#include "vpp_pq.h"
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include "video_reg.h"

extern u32 lowlatency_overrun_recovery_cnt;
extern u32 blackout[MAX_VD_LAYER];
extern char old_vmode[32];
extern char new_vmode[32];
extern struct vframe_s *cur_dispbuf2;
extern int vframe_walk_delay;
extern struct vpp_frame_par_s *cur_frame_par[MAX_VD_LAYER];
extern u32 new_frame_count;
extern u32 vsync_proc_drop;
extern ulong lowlatency_vsync_count;
extern bool overrun_flag;
extern u32 vsync_pts_inc_scale;
extern u32 vsync_pts_inc_scale_base;
extern atomic_t axis_changed;
extern const char *src_fmt_str[13];
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
extern int ai_pq_disable;
extern int ai_pq_value;
extern int ai_pq_debug;
extern int ai_pq_policy;
extern struct ai_scenes_pq vpp_scenes[AI_SCENES_MAX];
extern struct nn_value_t nn_scenes_value[AI_PQ_TOP];
#endif
extern atomic_t video_prop_change;
extern atomic_t video_prop_wakeup;
extern wait_queue_head_t amvideo_prop_change_wait;
extern u32 vpp_crc_en;
extern int vpp_crc_result;
extern atomic_t video_proc_lock;
extern bool over_field;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
extern bool dvel_status;
#endif

extern atomic_t video_llm_wake;
extern atomic_t video_llm_done;

ssize_t lowlatency_states_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count);
ssize_t lowlatency_states_show(const struct class *cla,
	const struct class_attribute *attr,
	char *buf);
bool tvin_vf_is_keeped(struct vframe_s *vf);
void _set_video_mirror(struct disp_info_s *layer, int mirror);
void primary_swap_frame(struct video_layer_s *layer, struct vframe_s *vf1, int line);
int dvel_swap_frame(struct vframe_s *vf);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void vf_pq_process(struct vframe_s *vf,
		   struct ai_scenes_pq *vpp_scenes,
		   int *pq_debug,
		   unsigned int vpp_new_frame);
void get_low_latency_info(u32 *enable, u32 *version);
void set_low_latency_info(u32 enable, u32 version);
int get_low_latency_version(void);
void get_low_latency_params(u32 *p0, u32 *p1, u32 *p2, u32 *p3, u32 *p4, u32 *p5);
void set_low_latency_params(u32 p0, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5);
void video_lowlatency_init(struct platform_device *pdev);
void video_lowlatency_exit(void);
void video_lowlatency_line_n_num(void);
int video_lowlatency_process(void);
#endif
#endif
/*VIDEO_LOW_LATENCTY_HH*/
