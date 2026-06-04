/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _di_process_H
#define _di_process_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>

#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#include <linux/amlogic/media/video_processor/di_process_ext.h>

#define DIPR_POOL_SIZE 16

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_FENCE		0X0002
#define PRINT_MORE		0X0080

#define IS_DI_PSTLINK(di_flag) ((di_flag) & DI_FLAG_DI_PSTVPPLINK)

enum di_backend_transform_t {
	DI_BACKEND_TRANSFORM_90 = 4,
	DI_BACKEND_TRANSFORM_270 = 7,
};

struct di_process_port_s {
	const char *name;
	u32 index;
	u32 open_count;
	struct device *class_dev;
	struct device *pdev;
};

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
	u32 reserved[13];
};

struct received_frame_t {
	int index;
	atomic_t on_use;
	struct vframe_s *vf;
	struct file *file_vf;
	bool dummy;
};

struct di_out_buf_t {
	int index;
	atomic_t on_use;
	struct di_buffer *di_buf;
	struct file_private_data *private_data;
};

struct di_process_dev {
	u32 index;
	struct di_process_port_s *port;
	bool inited;
	struct task_struct *kthread;
	bool thread_need_stop;
	bool thread_stopped;
	wait_queue_head_t wq;
	struct di_init_parm di_parm;
	int di_index;
	unsigned long long empty_done_count;
	unsigned long long fill_done_count;
	//unsigned long long received_count;
	unsigned long long fput_count;
	unsigned long long fget_count;
	unsigned long long empty_count;
	unsigned long long fill_count;
	struct received_frame_t received_frame[DIPR_POOL_SIZE];
	DECLARE_KFIFO(receive_q, struct received_frame_t *, DIPR_POOL_SIZE);
	struct di_buffer di_input[DIPR_POOL_SIZE];
	DECLARE_KFIFO(di_input_free_q, struct di_buffer *, DIPR_POOL_SIZE);
	DECLARE_KFIFO(file_free_q, struct dma_buf *, DIPR_POOL_SIZE);
	DECLARE_KFIFO(file_wait_q, struct dma_buf *, DIPR_POOL_SIZE);
	struct di_out_buf_t di_out_buf[DIPR_POOL_SIZE];
	DECLARE_KFIFO(di_out_q, struct di_out_buf_t *, DIPR_POOL_SIZE);
	struct file *last_file;
	struct dp_buf_mgr_t *last_buf_mgr;
	struct dma_buf *last_dmabuf;
	struct dma_buf *out_dmabuf[DIPR_POOL_SIZE];
	unsigned long long fence_creat_count;
	unsigned long long fence_release_count;
	u32 last_dec_type;
	u32 last_instance_id;
	u32 last_buf_mgr_reset_id;
	u32 last_frame_index;
	struct vframe_s dummy_vf;
	struct vframe_s dummy_vf1;
	struct vframe_s last_vf;
	struct mutex mutex_di_out;/*for di_out_q*/
	bool first_out;
	bool q_dummy_frame_done;
	bool last_frame_bypass;
	bool di_is_tvp;
	bool cur_is_i;
	bool di_do_rotate;
	bool di_module_bypass;
};

#define DI_PROCESS_IOC_MAGIC  'I'
#define DI_PROCESS_IOCTL_INIT        _IO(DI_PROCESS_IOC_MAGIC, 0x00)
#define DI_PROCESS_IOCTL_UNINIT      _IO(DI_PROCESS_IOC_MAGIC, 0x01)
#define DI_PROCESS_IOCTL_SET_FRAME   _IOW(DI_PROCESS_IOC_MAGIC, 0x02, struct frame_info_t)
#define DI_PROCESS_IOCTL_Q_OUTPUT    _IOW(DI_PROCESS_IOC_MAGIC, 0x03, int)
#define DI_PROCESS_IOCTL_GET_ENABLE  _IO(DI_PROCESS_IOC_MAGIC, 0x04)

int di_get_ref_vf(struct file *file, struct vframe_s **vf_1, struct vframe_s **vf_2,
	struct file **file_1, struct file **file_2);
struct uvm_di_mgr_t *get_uvm_di_mgr(struct file *file_vf);
int di_processed_checkin(struct file *file);
#endif
