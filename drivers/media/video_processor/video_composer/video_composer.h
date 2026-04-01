/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_COMPOSER_H
#define VIDEO_COMPOSER_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>

#include <linux/kfifo.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include "vfq.h"
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/video_sink/video.h>
#include "vframe_ge2d_composer.h"
#include "vframe_dewarp_composer.h"
#include "vframe_vicp_composer.h"

#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_VIDEO_COMPOSER
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
#include <trace/events/meson_atrace.h>
#endif

#define COMPOSER_READY_POOL_SIZE 32
#define FRAMES_INFO_POOL_SIZE 32

/* disable video_composer mode */
#define VIDEO_COMPOSER_ENABLE_NONE    0
#define VIDEO_COMPOSER_ENABLE_NORMAL  1
#define BUFFER_LEN 4
#define DMA_BUF_COUNT 4
#define PATTEN_FACTOR_MAX 5

#define VCOM_MAP_NAME_SIZE 90
#define VCOM_MAP_STRUCT_SIZE 120

#define VCOM_PROVIDER_NAME_SIZE 32

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X0001
#define PRINT_FENCE		0X0002
#define PRINT_PERFORMANCE	0X0004
#define PRINT_AXIS		0X0008
#define PRINT_INDEX_DISP	0X0010
#define PRINT_PATTERN	        0X0020
#define PRINT_OTHER		0X0040
#define PRINT_NN		0X0080
#define PRINT_DEWARP		0X0100
#define PRINT_VICP		0X0200
#define PRINT_DALTON		0X0400
#define PRINT_AIFACE		0X0800

#define SOURCE_DTV_FIX_TUNNEL		0x1
#define SOURCE_HWC_CREAT_ION		0x2
#define SOURCE_PIC_MODE		0x4

extern int actual_delay_count[MAX_VD_LAYERS];
extern u32 vd_vsync_pts_inc_scale[MAX_VD_LAYERS];
extern u32 vd_vsync_pts_inc_scale_base[MAX_VD_LAYERS];

#ifndef MAX
#define MAX(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	_a > _b ? _a : _b; \
	})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

enum vc_transform_t {
	/* flip source image horizontally */
	VC_TRANSFORM_FLIP_H = 1,
	/* flip source image vertically */
	VC_TRANSFORM_FLIP_V = 2,
	/* rotate source image 90 degrees clock-wise */
	VC_TRANSFORM_ROT_90 = 4,
	/* rotate source image 180 degrees */
	VC_TRANSFORM_ROT_180 = 3,
	/* rotate source image 270 degrees clock-wise */
	VC_TRANSFORM_ROT_270 = 7,
	/* flip source image horizontally, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_H_ROT_90 = VC_TRANSFORM_FLIP_H | VC_TRANSFORM_ROT_90,
	/* flip source image vertically, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_V_ROT_90 = VC_TRANSFORM_FLIP_V | VC_TRANSFORM_ROT_90,
};

struct frame_info_t {
	u32 fd;
	u32 composer_fen_fd;
	u32 disp_fen_fd;
	u32 buffer_w;
	u32 buffer_h;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	u32 zorder;
	u32 transform;
	u32 type;
	u32 sideband_type;
	u32 reserved[2];
	u32 source_type;
	u32 buffer_format;
	u32 reserved1[5];
};

struct frames_info_t {
	u32 frame_count;
	struct frame_info_t frame_info[MAX_COMPOSER_COUNT];
	u32 layer_index;
	u32 disp_zorder;
	u32 reserved[4];
};

enum com_buffer_status {
	UNINITIAL = 0,
	INIT_SUCCESS,
	INIT_ERROR,
};

enum com_dev_choice {
	COMPOSER_WITH_UNINITIAL = 0,
	COMPOSER_WITH_GE2D,
	COMPOSER_WITH_DEWARP,
	COMPOSER_WITH_VICP,
	COMPOSER_WITH_MAX,
};

enum vc_fence_status {
	VC_FENCE_INVALID = 0,
	VC_FENCE_DEC_ERR,
	VC_FENCE_RELEASED,
	VC_FENCE_NORMAL,
	VC_FENCE_WAIT,
};

struct video_composer_port_s {
	const char *name;
	u32 index;
	u32 open_count;
	struct device *class_dev;
	struct device *pdev;
	u32 video_render_index;
	int vpu_dma_mask;
};

struct videocom_frame_s {
	struct vframe_s frame;
	int index;
};

struct vidc_buf_status {
	int index;
	int dirty;
};

struct output_axis {
	int left;
	int top;
	int width;
	int height;
};

struct received_frames_t {
	int index;
	atomic_t on_use;
	struct frames_info_t frames_info;
	unsigned long long frames_num;
	struct file *file_vf[MAX_COMPOSER_COUNT];
	unsigned long phy_addr[MAX_COMPOSER_COUNT];
	u64 time_us64;
	bool is_tvp;
	struct file *fence_file[MAX_COMPOSER_COUNT];
};

struct vd_prepare_s {
	struct vframe_s *src_frame;
	struct vframe_s dst_frame;
	void *release_fence;
};

struct composer_dev {
	u32 index;
	struct video_composer_port_s *port;
	u32 enable_composer;
	DECLARE_KFIFO(ready_q, struct vframe_s *, COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(receive_q, struct received_frames_t *,
		      FRAMES_INFO_POOL_SIZE);
	DECLARE_KFIFO(free_q, struct vframe_s *, BUFFER_LEN);
	DECLARE_KFIFO(display_q, struct vframe_s *,
			COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(vc_private_q, struct video_composer_private *,
			COMPOSER_READY_POOL_SIZE);
	DECLARE_KFIFO(vc_prepare_data_q, struct vd_prepare_s *,
			COMPOSER_READY_POOL_SIZE);
	char vf_provider_name[VCOM_PROVIDER_NAME_SIZE];
	char vfm_map_id[VCOM_MAP_STRUCT_SIZE];
	char vfm_map_chain[VCOM_MAP_STRUCT_SIZE];
	struct vframe_provider_s vc_vf_prov;
	void *video_timeline;
	u32 cur_streamline_val;
	struct file *last_file;
	enum com_buffer_status buffer_status;
	struct ge2d_composer_para ge2d_para;
	struct dewarp_composer_para dewarp_para;
	struct task_struct *kthread;
	struct received_frames_t received_frames[FRAMES_INFO_POOL_SIZE];
	unsigned long long received_count;
	unsigned long long received_new_count;
	unsigned long long fence_creat_count;
	unsigned long long fence_release_count;
	unsigned long long fput_count;
	unsigned long long fget_count;
	bool need_free_buffer;
	//struct mutex mutex_input;
	wait_queue_head_t wq;
	bool thread_stopped;
	struct vframe_s *last_dst_vf;
	bool need_unint_receive_q;
	struct completion task_done;
	struct dst_buf_t dst_buf[BUFFER_LEN];
	struct vframe_s dma_vf[DMA_BUF_COUNT];
	struct vframe_s enhance_vf[DMA_BUF_COUNT];
	u32 drop_frame_count;
	struct received_frames_t last_frames;
	struct src_data_para ge2d_src_data;
	struct composer_vf_para dewarp_vframe_para;
	struct vicp_data_config_s vicp_data_config;
	struct timeval start_time;
	u32 vinfo_w;
	u32 vinfo_h;
	u32 composer_buf_w;
	u32 composer_buf_h;
	bool kfifo_need_initialize;
	bool need_rotate;
	bool is_sideband;
	bool need_empty_ready;
	struct vframe_s fake_vf;
	struct vframe_s fake_back_vf;
	struct video_composer_private vc_private[COMPOSER_READY_POOL_SIZE];
	struct vd_prepare_s vd_prepare[COMPOSER_READY_POOL_SIZE];
	struct vd_prepare_s *vd_prepare_last;
	bool select_path_done;
	bool composer_enabled;
	bool thread_need_stop;
	bool is_drm_enable;
	enum com_dev_choice dev_choice;
	u32 video_render_index;
	u32 vframe_dump_flag;
	u32 pre_pat_trace;
	u32 pattern[6];
	u32 pattern_enter_cnt;
	u32 pattern_exit_cnt;
	u32 pattern_detected;
	u32 continue_hold_count;
	u32 last_hold_index;
	u32 last_vsync_index;
	u32 last_vf_index;
	bool enable_pulldown;
	bool support_mosaic;
	u32 patten_factor[PATTEN_FACTOR_MAX];
	u32 patten_factor_index;
	u32 next_factor;
	u32 last_drop_cnt;
	u64 output_duration;
	u32 nn_mode_flag;
	struct vf_aiface_t *aiface_buf;
	u64 fence_wait_time_total;
	u32 fence_wait_count;
	bool is_tv_path;
	u32 low_latency_case;
	bool last_vf_from_dpss;
};

struct capability_info_t {
	u32 capability;
	u32 min_w;
	u32 min_h;
	u32 max_w;
	u32 max_h;
	u32 reserved[10];
};

struct mediaproxy_info_t {
	void *k_producer_session;
	char k_producer_name[32];
};

#define VIDEO_COMPOSER_IOC_MAGIC  'V'
#define VIDEO_COMPOSER_IOCTL_SET_FRAMES		\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x00, struct frames_info_t)
#define VIDEO_COMPOSER_IOCTL_SET_ENABLE		\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x01, int)
#define VIDEO_COMPOSER_IOCTL_SET_DISABLE	\
	_IOW(VIDEO_COMPOSER_IOC_MAGIC, 0x02, int)
#define VIDEO_COMPOSER_IOCTL_GET_PANEL_CAPABILITY	\
	_IOR(VIDEO_COMPOSER_IOC_MAGIC, 0x03, int)
#define VIDEO_COMPOSER_IOCTL_GET_LAYER_CAPABILITY	\
	_IOR(VIDEO_COMPOSER_IOC_MAGIC, 0x04, struct capability_info_t)

int video_composer_set_enable(struct composer_dev *dev, u32 val);
struct video_composer_port_s *video_composer_get_port(u32 index);
int vc_print(int index, int debug_flag, const char *fmt, ...);
void videocomposer_vf_put(struct vframe_s *vf, void *op_arg);
struct vframe_s *videocomposer_vf_peek(void *op_arg);
void video_display_push_ready(struct composer_dev *dev, struct vframe_s *vf);
void vc_private_q_init(struct composer_dev *dev);
void vc_private_q_recycle(struct composer_dev *dev,
	struct video_composer_private *vc_private);
struct video_composer_private *vc_private_q_pop(struct composer_dev *dev);
int vd_vframe_afbc_soft_decode(struct vframe_s *vf, int flag);
void debug_vc_print_flag(const char *module, int debug_flags);
void debug_vc_transform(const char *module, int debug_flags);
void debug_vc_force_composer(const char *module, int debug_flags);
void debug_vc_get_count(const char *module, int debug_flags);

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
int media_proxy_produce_deinit(void *handle);
int media_proxy_produce_init(void **handle, char *modulename, u32 msg_type);
int notify_msg_to_mediaproxy(void *handle, int num, void *data);
#endif
#endif /* VIDEO_COMPOSER_H */
