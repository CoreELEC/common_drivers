/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef V2D_H
#define V2D_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/of.h>

#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>

#include <linux/kfifo.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/video_sink/video.h>
#include "v2d_ge2d_composer.h"
#include "v2d_dewarp_composer.h"
#include "v2d_vicp_composer.h"

#define MAX_LAYER_COUNT		9
#define V2D_POOL_SIZE		32
#define BUFFER_LEN		4
#define V2D_SET_ENABLE_MODE	1
#define V2D_SET_DISABLE_MODE	0

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X01
#define PRINT_FENCE		0X02
#define PRINT_PERFORMANCE	0X04
#define PRINT_AXIS		0X08
#define PRINT_INDEX_DISP	0X10
#define PRINT_PATTERN	        0X20
#define PRINT_OTHER		0X040
#define PRINT_DEWARP		0X0100
#define PRINT_VICP		0x0200

#define WAIT_READY_Q_TIMEOUT	100
#define WAIT_DISPLAY_Q_TIMEOUT	100
#define MAX_RECEIVE_WAIT_TIME	15  /*15ms*/

#define V2D_AXIS_LEFT_INIT_MAX	7680
#define V2D_AXIS_TOP_INIT_MAX	5120

#define V2D_IOC_MAGIC  'R'
#define V2D_IOCTL_SET_FRAMES	_IOW(V2D_IOC_MAGIC, 0x00, struct frames_info_t)
#define V2D_IOCTL_SET_ENABLE	_IOW(V2D_IOC_MAGIC, 0x01, int)
#define V2D_IOCTL_SET_FENCE	_IOW(V2D_IOC_MAGIC, 0x02, struct release_info_t)

struct frame_info_t {
	s32 fd;
	s32 fence_fd;
	s32 buffer_w;
	s32 buffer_h;
	s32 align_w;
	s32 align_h;
	s32 dst_x;
	s32 dst_y;
	s32 dst_w;
	s32 dst_h;
	s32 crop_x;
	s32 crop_y;
	s32 crop_w;
	s32 crop_h;
	s32 zorder;
	s32 buffer_format;
	s32 transform;
	s32 alpha;
	s32 reserved[10];
};

struct frames_info_t {
	s32 frame_count;
	struct frame_info_t input_buffer[MAX_LAYER_COUNT];
	struct frame_info_t out_buffer;
	s32 reserved[4];
};

struct release_info_t {
	s32 release_fd;
	s32 release_fence_fd;
};

enum v2d_buffer_status {
	UNINITIAL = 0,
	INIT_SIZE_SUCCESS,
	INIT_BUFFER_SUCCESS,
	INIT_BUFFER_ERROR,
	STATUS_SIZE_CHANGED,
};

struct input_crop_s {
	u32 left;
	u32 top;
	u32 width;
	u32 height;
};

struct input_axis_s {
	int left;
	int top;
	int width;
	int height;
};

struct output_axis {
	int min_left;
	int min_top;
	int max_right;
	int max_bottom;
};

enum v2d_transform_t {
	/* flip source image horizontally */
	V2D_TRANSFORM_FLIP_H = 1,
	/* flip source image vertically */
	V2D_TRANSFORM_FLIP_V = 2,
	/* rotate source image 90 degrees clock-wise */
	V2D_TRANSFORM_ROT_90 = 4,
	/* rotate source image 180 degrees */
	V2D_TRANSFORM_ROT_180 = 3,
	/* rotate source image 270 degrees clock-wise */
	V2D_TRANSFORM_ROT_270 = 7,
	/* flip source image horizontally, the rotate 90 degrees clock-wise */
	V2D_TRANSFORM_FLIP_H_ROT_90 = V2D_TRANSFORM_FLIP_H | V2D_TRANSFORM_ROT_90,
	/* flip source image vertically, the rotate 90 degrees clock-wise */
	V2D_TRANSFORM_FLIP_V_ROT_90 = V2D_TRANSFORM_FLIP_V | V2D_TRANSFORM_ROT_90,
};

enum output_buf_mode_t {
	BUFFER_MODE_UNINIT = 0,
	BUFFER_MODE_GE2D,
	BUFFER_MODE_DEWARP,
	BUFFER_MODE_VICP,
};

enum output_buf_status_t {
	BUFFER_STATUS_NORMAL = 0,
	BUFFER_STATUS_CHANGE,
};

struct received_frames_t {
	int index;
	atomic_t on_use;
	struct input_crop_s crop_info[MAX_LAYER_COUNT];
	struct input_axis_s axis_info[MAX_LAYER_COUNT];
	struct output_axis output_axis;
	struct frames_info_t frames_info;
	struct file *input_file[MAX_LAYER_COUNT];
	struct file *input_fence[MAX_LAYER_COUNT];
	unsigned long phy_addr[MAX_LAYER_COUNT];
	bool is_tvp;
	size_t usage;
};

struct v2d_dump_info_s {
	u32 plane_num;
	u32 bit_10;
	u32 width;
	u32 height;
	u32 sizeimage;
	u64 phy_addr[3];
	void *vir_addr[3];
	wait_queue_head_t dump_wq;
	bool is_dump;
	bool config_dump_done;
	bool allow_dump;
	struct mutex mutex;/*for dump finish then user layer to dq*/
};

struct dst_buf_t {
	int index;
	enum output_buf_mode_t buf_used;
	enum output_buf_mode_t last_buf_used;
	enum output_buf_status_t buf_status;
	bool dirty;
	ulong phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 width;
	u32 height;
	u32 buf_size;
	bool is_tvp;
	u32 dw_size;
	ulong mif_handle;
	ulong afbc_head_addr;
	u32 afbc_head_size;
	ulong afbc_body_addr;
	u32 afbc_body_size;
	ulong afbc_table_addr;
	ulong afbc_table_handle;
	u32 afbc_table_size;
};

struct v2d_fence_cb_t {
	struct dma_fence_cb base_cb;
	struct v2d_dev *dev;
	struct file *buffer_file;
	struct file *fence_file;
};

struct display_data_t {
	struct dma_buf *output_dmabuf;
	struct dst_buf_t *output_buffer;
	struct file_private_data *private_data;
	atomic_t on_use;
};

enum composer_dev {
	COMPOSER_WITH_DEFAULT = 0,
	COMPOSER_WITH_GE2D,
	COMPOSER_WITH_DEWARP,
	COMPOSER_WITH_VICP,
	COMPOSER_WITH_UNINITIAL,
};

enum v2d_work_mode {
	V2D_MODE_UNINITIAL = 0,
	V2D_MODE_ROTATE,
	V2D_MODE_COMPOSER,
};

union composer_device_config {
	struct ge2d_src_para_s ge2d_data;
	struct dewarp_vf_para_s dewarp_data;
	struct vicp_data_config_s vicp_data;
};

struct v2d_dev_config {
	union composer_device_config composer_device_param;
	unsigned long addr;
	struct vframe_s *input_vf;
	struct dst_buf_t *dst_buf;
	struct input_axis_s frame_axis;
	struct input_crop_s frame_crop;
	int count;
	int index;
	int zorder;
	bool is_tvp;
};

struct v2d_dev {
	u32 index;
	struct v2d_port_s *port;
	enum composer_dev dev_choice;
	enum composer_dev last_dev_choice;
	enum v2d_work_mode work_mode;
	bool status_enabled;
	DECLARE_KFIFO(receive_q, struct received_frames_t *, V2D_POOL_SIZE);
	DECLARE_KFIFO(free_q, struct dst_buf_t *, BUFFER_LEN);
	DECLARE_KFIFO(file_free_q, struct dma_buf *, V2D_POOL_SIZE);
	DECLARE_KFIFO(file_wait_q, struct dma_buf *, V2D_POOL_SIZE);
	DECLARE_KFIFO(display_q, struct display_data_t *, BUFFER_LEN);
	DECLARE_KFIFO(fence_wait_q, struct file *, V2D_POOL_SIZE);
	DECLARE_KFIFO(fence_cb_q, struct v2d_fence_cb_t *, V2D_POOL_SIZE);
	struct received_frames_t received_frames[V2D_POOL_SIZE];
	struct dst_buf_t dst_buf[BUFFER_LEN];
	struct v2d_fence_cb_t v2d_fence_cb[V2D_POOL_SIZE];
	struct display_data_t display_data[V2D_POOL_SIZE];
	struct task_struct *file_thread;
	wait_queue_head_t file_wq;
	struct completion file_task_done;
	struct dma_buf *out_dmabuf[V2D_POOL_SIZE];
	void *v2d_timeline;
	u32 cur_streamline_val;
	u32 buf_width;
	u32 buf_height;
	int (*config_param_func)(struct v2d_dev *dev,
		struct frame_info_t *vframe_info_cur, struct v2d_dev_config *v2d_composer_param);
	int (*process_data_func)(struct v2d_dev *dev, struct v2d_dev_config *v2d_composer_param);
	enum v2d_buffer_status buffer_status;
	struct ge2d_composer_para ge2d_para;
	struct dewarp_composer_para dewarp_para;
	struct received_frames_t last_frames;
	u32 vinfo_w;
	u32 vinfo_h;
	u32 output_duration;
	u32 file_thread_stopped;
	bool thread_need_stop;
	bool need_do_file;
	u32 fget_count;
	u32 fput_count;
	u32 received_count;
	u32 received_new_count;
	u32 fence_creat_count;
	u32 fence_signal_count;
	u32 buffer_release_count;
	u32 fence_wait_count;
	u32 fence_wait_time_total;
	u32 set_frame_count;
	u32 set_fence_count;
	struct v2d_dump_info_s info_output_yuv;
	struct v2d_dump_info_s info_dewarp_src;
	struct kref refcount;
};

struct v2d_port_s {
	const char *name;
	u32 index;
	u32 open_count;
	struct device *class_dev;
	struct device *pdev;
	struct v2d_dev *dev;
	int vpu_dma_mask;
};

#endif
