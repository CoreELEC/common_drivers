/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _dpss_process_H
#define _dpss_process_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>

#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#include <linux/amlogic/media/video_processor/dpss_process_ext.h>

#define DPSSPR_POOL_SIZE 16
#define LCEVC_POOL_SIZE  8
#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_FENCE		0X0002
#define PRINT_MORE		0X0080

#define LCEVC_RESIDUAL_Y10BIT_PACKED	1
#define LCEVC_RESIDUAL_UYVY10BIT_PACKED	5
#define DISPLAY_SPEED_DEFAULT    1000
#define DISPLAY_SPEED_MASK       0

struct frame_info_t {
	u32 in_fd;
	u32 out_fd;
	u32 is_repeat;
	u32 out_fence_fd;
	u32 is_i;
	u32 frame_index;
	u32 need_bypass;
	u32 is_tvp;
	u32 transform;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	u32 reserved[9];
};

struct received_frame_t {
	int index;
	atomic_t on_use;
	struct vframe_s *vf;
	struct file *file_vf;
	bool dummy;
	u32 transform;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
};

struct dpss_in_buf_t {
	int index;
	struct vframe_s vf;
	struct pp_info_t pp_info;
};

struct dpss_out_buf_t {
	int index;
	atomic_t on_use;
	struct vframe_s *vf;
	struct file_private_data *private_data;
};

struct lcevc_buf_t {
	int index;
	struct vframe_s *dec_vf;
	struct vframe_s lcevc_vf;
	atomic_t on_use;
};

struct display_speed_t {
	int sync_id;
	int speed;
	atomic_t on_use;
};

enum direct_mode_override {
	DIRECT_MODE_AUTO = 0,
	DIRECT_MODE_FORCED_DISABLE,
	DIRECT_MODE_FORCED_ENABLE,
};

struct dpss_process_dev {
	u32 index;
	bool inited;
	struct task_struct *kthread;
	bool thread_need_stop;
	bool thread_stopped;
	wait_queue_head_t wq;
	struct dpss_init_parm dpss_parm;
	int dpss_index;
	unsigned long long empty_done_count;
	unsigned long long fill_done_count;
	//unsigned long long received_count;
	unsigned long long fput_count;
	unsigned long long fget_count;
	unsigned long long empty_count;
	unsigned long long fill_count;
	struct received_frame_t received_frame[DPSSPR_POOL_SIZE];
	DECLARE_KFIFO(receive_q, struct received_frame_t *, DPSSPR_POOL_SIZE);
	struct dpss_in_buf_t dpss_in_buf[DPSSPR_POOL_SIZE];
	DECLARE_KFIFO(dpss_input_free_q, struct dpss_in_buf_t *, DPSSPR_POOL_SIZE);
	DECLARE_KFIFO(file_free_q, struct dma_buf *, DPSSPR_POOL_SIZE);
	DECLARE_KFIFO(file_wait_q, struct dma_buf *, DPSSPR_POOL_SIZE);
	struct dpss_out_buf_t dpss_out_buf[DPSSPR_POOL_SIZE];
	DECLARE_KFIFO(dpss_out_q, struct dpss_out_buf_t *, DPSSPR_POOL_SIZE);
	DECLARE_KFIFO(dpss_out_free_q, struct vframe_s *, DPSSPR_POOL_SIZE);
	DECLARE_KFIFO(lcevc_out_q, struct lcevc_buf_t *, LCEVC_POOL_SIZE);
	struct file *last_file;
	struct dp_buf_mgr_t *last_buf_mgr;
	struct dma_buf *last_dmabuf;
	struct dma_buf *out_dmabuf[DPSSPR_POOL_SIZE];
	unsigned long long fence_creat_count;
	unsigned long long fence_release_count;
	u32 last_dec_type;
	u32 last_instance_id;
	u32 last_buf_mgr_reset_id;
	u32 last_frame_index;
	struct vframe_s dummy_vf;
	struct vframe_s dummy_vf1;
	struct vframe_s last_vf;
	struct lcevc_buf_t lcevc_buf[LCEVC_POOL_SIZE];
	struct mutex mutex_dpss_out;/*for dpss_out_q*/
	bool first_out;
	bool q_dummy_frame_done;
	bool last_frame_bypass;
	bool dpss_is_tvp;
	bool cur_is_i;
	bool cur_is_frcgame;
	bool dpss_module_bypass;
	bool last_frame_vd1_toggle;
	u32 transform;
	int direct_mode_en;
	int frc_en;
	bool is_start_with_dpss;
	bool need_check_hdr_state;
	int output_duration;
	u32 continue_to_vd1_num;
	bool should_on_vd1;
	bool vd1_switch_dpss_is_done;
	bool is_first_frame;
	bool is_switch_first_frame;
	u32 sync_id;
	int trick_play_en;
};

struct dpss_process_port_s {
	const char *name;
	u32 index;
	u32 open_count;
	struct device *class_dev;
	struct device *pdev;
	struct dpss_process_dev *process_dev;
};

#define DPSS_PROCESS_IOC_MAGIC  'I'
#define DPSS_PROCESS_IOCTL_INIT        _IO(DPSS_PROCESS_IOC_MAGIC, 0x00)
#define DPSS_PROCESS_IOCTL_UNINIT      _IO(DPSS_PROCESS_IOC_MAGIC, 0x01)
#define DPSS_PROCESS_IOCTL_SET_FRAME   _IOW(DPSS_PROCESS_IOC_MAGIC, 0x02, struct frame_info_t)
#define DPSS_PROCESS_IOCTL_Q_OUTPUT    _IOW(DPSS_PROCESS_IOC_MAGIC, 0x03, int)
#define DPSS_PROCESS_IOCTL_GET_ENABLE  _IO(DPSS_PROCESS_IOC_MAGIC, 0x04)

int di_get_ref_vf(struct file *file, struct vframe_s **vf_1, struct vframe_s **vf_2,
	struct file **file_1, struct file **file_2);
struct uvm_di_mgr_t *get_uvm_di_mgr(struct file *file_vf);
int di_processed_checkin(struct file *file);
#endif
