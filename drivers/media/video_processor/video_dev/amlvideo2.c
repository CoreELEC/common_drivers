// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/random.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ctrls.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <uapi/amlogic/v4l2-ext/videodev2-ext.h>
#include <uapi/amlogic/v4l2-ext/v4l2-controls-ext.h>
#include <linux/types.h>
#include <linux/timex.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "../utils/vfp-queue.h"
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/kernel.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include "amlvideo2.h"

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
/* #include <mach/mod_gate.h> */
/* #endif */

#include <linux/of.h>
#include <linux/of_fdt.h>

#include <linux/semaphore.h>
#include <linux/sched/rt.h>

#define AVMLVIDEO2_MODULE_NAME "amlvideo2"

#define MULTI_NODE
/* #define USE_SEMA_QBUF */
/* #define USE_VDIN_PTS */

#define MULTI_NODE

#ifdef MULTI_NODE
#define MAX_SUB_DEV_NODE 2
#else
#define MAX_SUB_DEV_NODE 1
#endif

#define norm_maxw() 2000
#define norm_maxh() 2000
#define norm_maxw_4k() 3840
#define norm_maxh_4k() 3840

#define VDIN1_SCALE_W 480
#define VDIN1_SCALE_H 270

/* Wake up at about 30 fps */
#define WAKE_NUMERATOR 30
#define WAKE_DENOMINATOR 1001

/*For VIDIOC_DQEVENT ioctl*/
#define V4L2_EVENT_PRIVATE_EXT_BASE (V4L2_EVENT_PRIVATE_START + 0X2000)
#define V4L2_EVENT_PRIVATE_EXT_VDIN_PATTERN (V4L2_EVENT_PRIVATE_EXT_BASE + 1)

#define AMLVIDEO2_MAJOR_VERSION 0
#define AMLVIDEO2_MINOR_VERSION 7
#define AMLVIDEO2_RELEASE 1
#define AMLVIDEO2_VERSION \
KERNEL_VERSION(\
	       AMLVIDEO2_MAJOR_VERSION, AMLVIDEO2_MINOR_VERSION,\
	       AMLVIDEO2_RELEASE)

#define MAGIC_SG_MEM 0x17890714
#define MAGIC_DC_MEM 0x0733ac61
#define MAGIC_VMAL_MEM 0x18221223
#define MAGIC_RE_MEM 0x123039dc

#ifdef MULTI_NODE
#define DEVICE_NAME0 "amlvideo2.0"
#define DEVICE_NAME1 "amlvideo2.1"
#else
#define RECEIVER_NAME "amlvideo2.0"
#define DEVICE_NAME   "amlvideo2.0"
#endif
static const char amlvideo2_group_name[] = "amlvideo2";

#define AMLVIDEO2_MAX_INSTANCE 1
#define BUFFER_COUNT 4

#define DUR2PTS_RM(x) ((x) & 0xf)

#define CMA_ALLOC_SIZE 24
#define CMA_ALLOC_SIZE_720P_NV21 6
#define CMA_ALLOC_SIZE_1080P_NV21 12
#define CMA_ALLOC_SIZE_4K_NV21 48
#define CMA_ALLOC_SIZE_1080P_RGBA 32

#define CANVAS_WIDTH_ALIGN 32
#define SECURE_BUFFER_SIZE_1M 0x100000

#define VIDEO_Q_BUFF_NUM 4
//MODULE_AUTHOR("amlogic-sh");
//MODULE_LICENSE("GPL");
static unsigned int video_nr = 11;
static bool is_black_frame[2];

#define TEST_LATENCY
#define DEF_FRAMERATE 30
static unsigned int debug;
static unsigned int mirror_value;
static int amlvideo2_total_get_count;
module_param(amlvideo2_total_get_count, uint, 0664);
MODULE_PARM_DESC(amlvideo2_total_get_count, "amlvideo2_total_get_count");

static int amlvideo2_total_put_count;
module_param(amlvideo2_total_put_count, uint, 0664);
MODULE_PARM_DESC(amlvideo2_total_put_count, "amlvideo2_total_put_count");

static unsigned int vid_limit = 50;
module_param_named(video2_vid_limit, vid_limit, uint, 0644);

static unsigned int amlvideo2_dbg_en;
module_param(amlvideo2_dbg_en, uint, 0664);
MODULE_PARM_DESC(amlvideo2_dbg_en, "enable/disable amlvideo2 debug information");

static unsigned int amlvideo2_scaledown1 = 2;
module_param(amlvideo2_scaledown1, uint, 0664);
MODULE_PARM_DESC(amlvideo2_scaledown1, "amlvideo2_scaledown1");
static unsigned int amlvideo2_scaledown2 = 2;
module_param(amlvideo2_scaledown2, uint, 0664);
MODULE_PARM_DESC(amlvideo2_scaledown2, "amlvideo2_scaledown2");

static int amlvideo2_angle;
module_param(amlvideo2_angle, uint, 0664);
MODULE_PARM_DESC(amlvideo2_angle, "amlvideo2_angle");

static int amlvideo2_dump;
module_param(amlvideo2_dump, uint, 0664);
MODULE_PARM_DESC(amlvideo2_dump, "amlvideo2_dump");

static int amlvideo2_dest_w;
module_param(amlvideo2_dest_w, uint, 0664);
MODULE_PARM_DESC(amlvideo2_dest_w, "amlvideo2_dest_w");

static int amlvideo2_dest_h;
module_param(amlvideo2_dest_h, uint, 0664);
MODULE_PARM_DESC(amlvideo2_dest_h, "amlvideo2_dest_w");

static int dump_num_increase;
static int amlvideo2_continue_dump;
module_param(amlvideo2_continue_dump, uint, 0664);
MODULE_PARM_DESC(amlvideo2_continue_dump, "amlvideo2_continue_dump");

static int amlvideo2_force_report;
module_param(amlvideo2_force_report, uint, 0664);
MODULE_PARM_DESC(amlvideo2_force_report, "amlvideo2_force_report");

static int amlvideo2_set_outfmt_dump;
module_param(amlvideo2_set_outfmt_dump, uint, 0664);
MODULE_PARM_DESC(amlvideo2_set_outfmt_dump, "amlvideo2_set_outfmt_dump");

static unsigned long amlvideo2_secure_paddr;
module_param(amlvideo2_secure_paddr, ulong, 0664);
MODULE_PARM_DESC(amlvideo2_secure_paddr, "amlvideo2_secure_paddr");

static int amlvideo2_secure_aipq_flag;
module_param(amlvideo2_secure_aipq_flag, uint, 0664);
MODULE_PARM_DESC(amlvideo2_secure_aipq_flag, "amlvideo2_secure_aipq_flag");

static struct v4l2_fract amlvideo2_frmintervals_active = {
	.numerator = 1, .denominator = DEF_FRAMERATE, };

/* supported controls */
static struct v4l2_queryctrl amlvideo2_node_qctrl[] = {{
	.id = V4L2_CID_ROTATE, .type = V4L2_CTRL_TYPE_INTEGER, .name = "Rotate",
	.minimum = 0, .maximum = 270, .step = 90, .default_value = 0, .flags =
		V4L2_CTRL_FLAG_SLIDER, } };

static struct v4l2_frmivalenum amlvideo2_frmivalenum[] = {
	{
	.index = 0, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1920, .height =
		1080, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 30, } }
	},
	{
	.index = 0, .pixel_format = V4L2_PIX_FMT_NV21, .width = 3840, .height =
		2160, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 30, } }
	},
	{
	.index = 1, .pixel_format = V4L2_PIX_FMT_NV21, .width = 1600, .height =
		1200, .type = V4L2_FRMIVAL_TYPE_DISCRETE, {.discrete = {
		.numerator = 1, .denominator = 15, } }
	}
};

/* ------------------------------------------------------------------
 * Basic structures
 * ------------------------------------------------------------------
 */

enum aml_provider_type_e {
	AML_PROVIDE_NONE = 0,
	AML_PROVIDE_VDIN0 = 1,
	AML_PROVIDE_VDIN1 = 2,
	AML_PROVIDE_DECODE = 3,
	AML_PROVIDE_PPMGR = 4,
	AML_PROVIDE_MAX = 5
};

enum aml_receiver_type_e {
	AML_RECEIVER_NONE = 0,
	AML_RECEIVER_PPMGR,
	AML_RECEIVER_DI,
	AML_RECEIVER_MAX
};

enum aml_screen_mode_e {
	AML_SCREEN_MODE_RATIO = 0,
	AML_SCREEN_MODE_FULL,
	AML_SCREEN_MODE_ADAPTIVE,
	AML_SCREEN_MODE_MAX
};

enum amlvideo2_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

enum aml_fill_frame_type_e {
	AML_NORMAL = 0,
	AML_HDCP_BLACK,
	AML_MAX,
};

struct amlvideo2_fmt {
	char *name;
	u32 fourcc; /* v4l2 format id */
	int depth;
};

static struct amlvideo2_fmt formats[] = {
	{.name = "RGB565 (BE)",
	.fourcc = V4L2_PIX_FMT_RGB565X, /* rrrrrggg gggbbbbb */
	.depth = 16, },

	{.name = "RGB888 (24)",
	.fourcc = V4L2_PIX_FMT_RGB24, /* 24  RGB-8-8-8 */
	.depth = 24, },

	{.name = "BGR888 (24)",
	.fourcc = V4L2_PIX_FMT_BGR24, /* 24  BGR-8-8-8 */
	.depth = 24, },

	{.name = "RGBA888 (32)",
	.fourcc = V4L2_PIX_FMT_RGB32, /* 32  RGBA-8-8-8 */
	.depth = 32, },

	{.name = "ARGB888 (32)",
	.fourcc = V4L2_PIX_FMT_ARGB32, /* 32  ARGB-8-8-8 */
	.depth = 32, },

	{.name = "BGRA888 (32)",
	.fourcc = V4L2_PIX_FMT_BGR32, /* 32  BGRA-8-8-8 */
	.depth = 32, },

	{.name = "12  Y/CbCr 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV12,
	.depth = 12, },

	{.name = "12  Y/CbCr 4:2:0",
	.fourcc = V4L2_PIX_FMT_NV21,
	.depth = 12, },

	{.name = "YUV420P",
	.fourcc = V4L2_PIX_FMT_YUV420,
	.depth = 12, },

	{.name = "YVU420P",
	.fourcc = V4L2_PIX_FMT_YVU420,
	.depth = 12, }
};

static char *vb2_buf_sts_to_str(uint32_t state)
{
	switch (state) {
	case VB2_BUF_STATE_DEQUEUED:
		return "VB2_BUF_STATE_DEQUEUED(0)";
	case VB2_BUF_STATE_IN_REQUEST:
		return "VB2_BUF_STATE_PREPARED(1)";
	case VB2_BUF_STATE_PREPARING:
		return "VB2_BUF_STATE_PREPARING(2)";
	case VB2_BUF_STATE_QUEUED:
		return "VB2_BUF_STATE_QUEUED(3)";
	case VB2_BUF_STATE_ACTIVE:
		return "VB2_BUF_STATE_ACTIVE(4)";
	case VB2_BUF_STATE_DONE:
		return "VB2_BUF_STATE_DONE(5)";
	case VB2_BUF_STATE_ERROR:
		return "VB2_BUF_STATE_ERROR(6)";
	default:
		return "VB2_BUF_STATE_UNKNOWN";
	}
}

static struct amlvideo2_fmt *get_format(struct v4l2_format *f)
{
	struct amlvideo2_fmt *fmt;
	unsigned int k;

	for (k = 0; k < ARRAY_SIZE(formats); k++) {
		fmt = &formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			break;
	}

	if (k == ARRAY_SIZE(formats))
		return NULL;

	return &formats[k];
}

/* buffer for one video frame */
struct amlvideo2_frame_info {
	int x;
	int y;
	int w;
	int h;
};

struct amlvideo2_latency_info {
	int total_latency;
	int total_latency_out;
	long long cur_time;
	long long cur_time_out;
	int frame_num;
	int frame_num_out;
	struct timeval test_time;
};

struct amlvideo2_node_buffer {
	struct vb2_v4l2_buffer vb2;
	void *vaddr[AML_PLANE_MAX];
	dma_addr_t addr[AML_PLANE_MAX];
	struct amlvideo2_fmt *fmt;
	int canvas_id;
	struct amlvideo2_frame_info axis;
	struct list_head list;
};

struct amlvideo2_node_dmaqueue {
	struct list_head head;
	// spinlock for list head.
	spinlock_t buff_list_lock;
	/* thread for generating video stream*/
	struct task_struct *kthread;
	wait_queue_head_t wq;
	unsigned char task_running;
#ifdef USE_SEMA_QBUF
	struct completion qbuf_comp;
#endif
};

struct amlvideo2_device {
	// mutex for vdev_lock
	struct mutex lock;
	// mutex for vb2_q->lock
	struct mutex q_lock;

	struct v4l2_device v4l2_dev;
	struct platform_device *pdev;
	struct amlvideo2_node *node[MAX_SUB_DEV_NODE];
	int node_num;
	resource_size_t buffer_start;
	unsigned int buffer_size;
	int cma_mode;
	int node_id;
	bool use_reserve;   //0:codec_mm  1:reserved
	int support_4k_capture;
	u32 framebuffer_total_size;
	int codec_mm_alloc;
	int vpu_dma_mask;
	bool is_one_buffer;
};

struct crop_info_s {
	int source_top_crop;
	int source_left_crop;
	int source_width_crop;
	int source_height_crop;
	int capture_crop_enable;
};

struct dump_info_s {
	u32 width;
	u32 height;
	u32 sizeimage;
	u64 phy_addr;
	void *vir_addr;
	wait_queue_head_t dump_wq;
	bool is_dump;
	bool config_dump_done;
	struct mutex mutex;/*for dump finish then user layer to dq*/
};

struct screen_display_info_s {
	struct vdisplay_info_s display_info;
	enum aml_screen_mode_e mode;
};

struct amlvideo2_fh;
struct amlvideo2_node {
	struct mutex mutex;/* */
	int vid;
	int users;

	struct amlvideo2_device *aml2_dev;

	/* various device info */
	struct video_device *vdev;

	struct amlvideo2_node_dmaqueue vidq;

	/* Control 'registers' */
	int qctl_regs[ARRAY_SIZE(amlvideo2_node_qctrl)];

	struct vframe_receiver_s recv;
	struct vframe_receiver_s *sub_recv;
	struct vframe_provider_s *provider;
	enum aml_provider_type_e p_type;
	enum aml_receiver_type_e r_type;
	int provide_ready;

	struct amlvideo2_fh *fh;
	unsigned int input; /* 0:mirrocast; 1:hdmiin */
	enum tvin_port_e porttype;
	unsigned int start_vdin_flag;
	struct ge2d_context_s *context;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	struct vdin_v4l2_ops_s vops;
#endif
	int vdin_device_num;
	struct vdisplay_info_s display_info;
	struct crop_info_s crop_info;
	enum aml_screen_mode_e mode;
	bool has_amvideo_node;

	struct vframe_provider_s amlvideo2_vf_prov;

	int amlvideo2_pool_size;
	struct vfq_s q_ready;
	struct vframe_s *amlvideo2_pool_ready;

	bool video_blocking;
	struct timespec64 thread_ts1;
	struct timespec64 thread_ts2;
	int frameinv_adjust;
	int frameinv;
	struct vframe_s *tmp_vf;
	int frame_inittime;
	struct amlvideo2_latency_info latency_info;
	bool pflag;
	struct completion plug_sema;
	bool field_flag;
	bool field_condition_flag;
	bool ge2d_multi_process_flag;
	int aml2_canvas[3];
#ifdef CONFIG_PM
	atomic_t is_suspend;
	struct completion thread_sema;
	bool could_suspend;
	struct completion suspend_sema;
#endif
	int vdin_port_ext;
	struct device *mem_form_dev;
	unsigned int frame_rate_black;
	bool vdin1_is_start;
	struct dump_info_s dump_info_src;
	struct dump_info_s dump_info_dst;
	bool is_reg;
	int screencap_type;  //0:only video, 1:osd+video, 2:only osd
};

struct amlvideo2_fh {
	struct amlvideo2_node *node;

	/* video capture */
	struct amlvideo2_fmt *fmt;
	unsigned int width, height;
	unsigned int buf_width, buf_height;
	unsigned int bytesperline;
	unsigned int sizeimage;
	unsigned int src_width, src_height;
	bool set_format_flag;
	struct vb2_queue vb2_q;
	enum v4l2_field field;
	struct mutex mutex;/*for judge if screencap doing */
	unsigned int is_streamed_on;
	suseconds_t frm_save_time_us; /* us */
	unsigned int f_flags;
	enum v4l2_buf_type type;
	struct v4l2_fh fh;
	enum aml_fill_frame_type_e fill_frame_type;
	struct v4l2_ext_amlvideo2_output_data ext_output_data;
	struct v4l2_ext_amlvideo2_get_video_data video_data;
	struct v4l2_ext_amlvideo2_set_secure_data secure_data;
	struct v4l2_ext_amlvideo2_get_vinfo_data vinfo_data;
	enum aml_quantization_range quantization_range;
};

struct amlvideo2_output {
	int canvas_id;
	void *vbuf;
	int width;
	int height;
	int buf_width;
	int buf_height;
	u32 v4l2_format;
	int angle;
	struct screen_display_info_s info;
	struct amlvideo2_frame_info *frame;
	int bsize;
};

static struct v4l2_frmsize_discrete amlvideo2_prev_resolution[] = {
	{160, 120},
	{320, 240},
	{640, 480},
	{1280, 720},
	{1920, 1080},
	{3840, 2160}
};

static struct v4l2_frmsize_discrete amlvideo2_pic_resolution[] = {
	{1280, 960}
};

static int amlvideo2_canvas[2][2 * BUFFER_COUNT] = {
	{-1, -1, -1, -1, -1, -1, -1, -1},
	{-1, -1, -1, -1, -1, -1, -1, -1}
};

static int yuv420_canvas[2][BUFFER_COUNT] = {
	{-1, -1, -1, -1},
	{-1, -1, -1, -1}
};

struct vinfo_s *amlvideo2_get_vinfo(struct amlvideo2_node *node)
{
	struct vinfo_s *vinfo = NULL;

	if (!node)
		return NULL;

	if (node->vid == 0) {
		vinfo = get_current_vinfo();
	} else {
		if (node->vdin_port_ext == PORT_VPP0_OSD_VIDEO ||
			node->vdin_port_ext == PORT_VPP0_OSD_VIDEO_NO_PQ ||
			node->vdin_port_ext == PORT_VPP0_VIDEO_ONLY ||
			node->vdin_port_ext == PORT_VPP0_VIDEO_ONLY_NO_PQ ||
			node->vdin_port_ext == PORT_VPP0_OSD1_ONLY ||
			node->vdin_port_ext == PORT_VPP0_OSD2_ONLY) {
			vinfo = get_current_vinfo();
		} else if (node->vdin_port_ext == PORT_VPP1_VIDEO_ONLY ||
			node->vdin_port_ext == PORT_VPP1_OSD_VIDEO ||
			node->vdin_port_ext == PORT_VPP1_OSD1_ONLY) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
			vinfo = get_current_vinfo2();
#else
			vinfo = NULL;
			pr_err("amlvideo2: not support vinfo2");

#endif
		} else {
			vinfo = get_current_vinfo();
			pr_err("amlvideo2: unsupport vdin_port %d\n", node->vdin_port_ext);
		}
	}

	return vinfo;
}

int get_amlvideo2_canvas_index(struct amlvideo2_output *output,
			       int inst, int buffer_id)
{
	int canvas = amlvideo2_canvas[inst][2 * buffer_id];
	int v4l2_format = output->v4l2_format;
	void *buf = (void *)output->vbuf;
	int width = output->buf_width;
	int height = output->buf_height;
	int canvas_height = height;
	const char *canvas_owner0 = "amlvideo2.0";
	const char *canvas_owner1 = "amlvideo2.1";

	if (canvas < 0)
		return -1;
	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas_config(canvas, (unsigned long)buf, width * 2,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
		canvas_config(canvas, (unsigned long)buf, width * 3,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_BGR32:
		canvas_config(canvas, (unsigned long)buf, width * 4,
			      canvas_height, CANVAS_ADDR_NOWRAP,
			      CANVAS_BLKMODE_LINEAR);
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		if (amlvideo2_canvas[inst][2 * buffer_id + 1] < 0)
			return -1;
		canvas_config(amlvideo2_canvas[inst][2 * buffer_id],
			      (unsigned long)buf, width, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config(amlvideo2_canvas[inst][2 * buffer_id + 1],
			      (unsigned long)(buf + width * height),
			      width, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas = amlvideo2_canvas[inst][2 * buffer_id]
			| (amlvideo2_canvas[inst][2 * buffer_id + 1] << 8);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		if (amlvideo2_canvas[inst][2 * buffer_id + 1] < 0)
			return -1;

		if (yuv420_canvas[inst][buffer_id] < 0)
			yuv420_canvas[inst][buffer_id] =
				canvas_pool_map_alloc_canvas((inst == 0) ?
							     canvas_owner0 :
							     canvas_owner1);
		if (yuv420_canvas[inst][buffer_id] < 0)
			return -1;
		canvas_config(amlvideo2_canvas[inst][2 * buffer_id],
			      (unsigned long)buf, width, canvas_height,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config(amlvideo2_canvas[inst][2 * buffer_id + 1],
			      (unsigned long)(buf + width * height),
			      width / 2, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		canvas_config(yuv420_canvas[inst][buffer_id],
			      (unsigned long)(buf + width * height * 5 / 4),
			      width / 2, canvas_height / 2,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		if (v4l2_format == V4L2_PIX_FMT_YUV420) {
			canvas = amlvideo2_canvas[inst][2 * buffer_id]
				| (amlvideo2_canvas[inst][2 * buffer_id + 1]
				   << 8) | (yuv420_canvas[inst][buffer_id]
					    << 16);
		} else {
			canvas = amlvideo2_canvas[inst][2 * buffer_id]
				| (yuv420_canvas[inst][buffer_id] << 8)
				| (amlvideo2_canvas[inst][2 * buffer_id + 1]
				   << 16);
		}
		break;
	default:
		break;
	}
	return canvas;
}

/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
int convert_canvas_index(struct amlvideo2_output *output,
			       int inst, int buffer_id)
{
	int canvas = amlvideo2_canvas[inst][2 * buffer_id];
	int v4l2_format = output->v4l2_format;
	const char *canvas_owner0 = "amlvideo2.0";
	const char *canvas_owner1 = "amlvideo2.1";

	if (canvas < 0)
		return -1;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_VYUY:
		canvas = amlvideo2_canvas[inst][2 * buffer_id];
		break;
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_BGR32:
		canvas = amlvideo2_canvas[inst][2 * buffer_id];
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		if (amlvideo2_canvas[inst][2 * buffer_id + 1] < 0)
			return -1;
		canvas = amlvideo2_canvas[inst][2 * buffer_id]
			| (amlvideo2_canvas[inst][2 * buffer_id + 1] << 8);
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
		if (amlvideo2_canvas[inst][2 * buffer_id + 1] < 0)
			return -1;
		if (yuv420_canvas[inst][buffer_id] < 0)
			yuv420_canvas[inst][buffer_id] =
				canvas_pool_map_alloc_canvas((inst == 0) ?
							     canvas_owner0 :
							     canvas_owner1);
		if (yuv420_canvas[inst][buffer_id] < 0)
			return -1;

		if (v4l2_format == V4L2_PIX_FMT_YUV420)
			canvas = amlvideo2_canvas[inst][2 * buffer_id]
			| (amlvideo2_canvas[inst][2 * buffer_id + 1] << 8)
			| (yuv420_canvas[inst][buffer_id] << 16);
		else
			canvas = amlvideo2_canvas[inst][2 * buffer_id]
			| (yuv420_canvas[inst][buffer_id] << 8)
			| (amlvideo2_canvas[inst][2 * buffer_id + 1] << 16);
		break;
	default:
		break;
	}
	return canvas;
}

/* #endif */

static int get_source_type(struct vframe_s *vf)
{
	enum amlvideo2_source_type ret;
	int interlace_mode;

	interlace_mode = vf->type & VIDTYPE_TYPEMASK;
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
	    vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
		if ((vf->bitdepth & BITDEPTH_Y10)  &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((vf->bitdepth & BITDEPTH_Y10)  &&
		    (!(vf->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum amlvideo2_source_type source_type;

	source_type = get_source_type(vf);
	switch (source_type) {
	case DECODER_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12;
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12B & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12T & (3 << 3));
		else if (vf->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422B
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422B
					& (3 << 3));
		}
		break;
	case DECODER_10BIT_TOP:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422
					| (GE2D_FORMAT_S16_10BIT_YUV422T
					& (3 << 3));
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422
					| (GE2D_FORMAT_S16_12BIT_YUV422T
					& (3 << 3));
		}
		break;
	case VDIN_8BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			format = GE2D_FORMAT_S16_YUV422;
		} else if (vf->type & VIDTYPE_VIU_NV21) {
			format = GE2D_FORMAT_M24_NV21;
		} else if (vf->type & VIDTYPE_VIU_NV12) {
			format = GE2D_FORMAT_M24_NV12;
		} else if (vf->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_YUV444;
		} else if (vf->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_RGB;
			if (!(vf->flag & VFRAME_FLAG_VIDEO_LINEAR))
				format &= (~GE2D_LITTLE_ENDIAN);
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
		break;
	case VDIN_10BIT_NORMAL:
		if (vf->type & VIDTYPE_VIU_422) {
			if (vf->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		} else if (vf->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_10BIT_YUV444;
		} else if (vf->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_10BIT_RGB888;
			if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
				format |= GE2D_LITTLE_ENDIAN;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

static int get_input_format_no_interlace(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422;
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21;
	else
		format = GE2D_FORMAT_M24_YUV420;

	return format;
}

static int get_interlace_input_format(struct vframe_s *vf,
				      struct amlvideo2_output *output)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422) {
		format = GE2D_FORMAT_S16_YUV422;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		}
	} else if (vf->type & VIDTYPE_VIU_NV21) {
		format = GE2D_FORMAT_M24_NV21;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		}
	} else {
		format = GE2D_FORMAT_M24_YUV420;
		if (vf->height >= output->height << 2) {
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		}
	}
	return format;
}

static inline int get_top_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422T & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21T & (3 << 3));
	else
		format = GE2D_FORMAT_M24_YUV420 |
			(GE2D_FORMAT_M24_YUV420T & (3 << 3));
	return format;
}

static inline int get_bottom_input_format(struct vframe_s *vf)
{
	int format = GE2D_FORMAT_M24_NV21;

	if (vf->type & VIDTYPE_VIU_422)
		format = GE2D_FORMAT_S16_YUV422 |
			(GE2D_FORMAT_S16_YUV422B & (3 << 3));
	else if (vf->type & VIDTYPE_VIU_NV21)
		format = GE2D_FORMAT_M24_NV21 |
			(GE2D_FORMAT_M24_NV21B & (3 << 3));
	else
		format =
			GE2D_FORMAT_M24_YUV420 |
			(GE2D_FMT_M24_YUV420B & (3 << 3));
	return format;
}

static int get_output_format(int v4l2_format)
{
	int format = GE2D_FORMAT_S24_YUV444;

	switch (v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
		format = GE2D_FORMAT_S16_RGB_565;
		break;
	case V4L2_PIX_FMT_YUV444:
		format = GE2D_FORMAT_S24_YUV444;
		break;
	case V4L2_PIX_FMT_VYUY:
		format = GE2D_FORMAT_S16_YUV422;
		break;
	case V4L2_PIX_FMT_BGR24:
		format = GE2D_FORMAT_S24_RGB;
		break;
	case V4L2_PIX_FMT_RGB24:
		format = GE2D_FORMAT_S24_BGR;
		break;
	case V4L2_PIX_FMT_RGB32:
		format = GE2D_FORMAT_S32_ABGR;
		break;
	case V4L2_PIX_FMT_ARGB32:
		format = GE2D_FORMAT_S32_BGRA;
		break;
	case V4L2_PIX_FMT_BGR32:
		format = GE2D_FORMAT_S32_ARGB;
		break;
	case V4L2_PIX_FMT_NV12:
		format = GE2D_FORMAT_M24_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = GE2D_FORMAT_M24_NV21;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		format = GE2D_FORMAT_S8_Y;
		break;
	default:
		break;
	}
	return format;
}

static void src_axis_adjust(int  *src_top,  int  *src_left,
			    int  *src_w,  int  *src_h,
			    struct amlvideo2_output *output)
{
	*src_top = output->info.display_info.display_vsc_startp;
	*src_left = output->info.display_info.display_hsc_startp;
	*src_w = output->info.display_info.display_hsc_endp -
	output->info.display_info.display_hsc_startp + 1;
	*src_h = output->info.display_info.display_vsc_endp -
	output->info.display_info.display_vsc_startp + 1;
}

static void output_axis_adjust(int src_w, int src_h, int *dst_w, int *dst_h,
			       int angle, struct amlvideo2_output *output)
{
	int w = 0, h = 0, disp_w = 0, disp_h = 0;
	int post_blend_w = 0, post_blend_h = 0, window_w = 0, window_h = 0;

	disp_w = *dst_w;
	disp_h = *dst_h;
	if (src_w == 0 || src_h == 0) {
		pr_err("src width or height is zero");
		return;
	}

	if (amlvideo2_dbg_en & 4) {
		pr_info("%s:src_w = %d, src_h = %d, angle = %d.\n",
			__func__, src_w, src_h, angle);
	}

	if (output) {
		if (amlvideo2_dbg_en & 4) {
			pr_info("output->info.mode = %d.\n", output->info.mode);
		}
		if (output->info.mode == AML_SCREEN_MODE_RATIO) {
			if (angle % 180 == 0) {
				if (src_w < disp_w && src_h < disp_h) {
					w = disp_w;
					h = disp_h;
				} else if ((src_w * disp_h) >
					(disp_w * src_h)) {
					w = disp_w;
					h = disp_w * src_h / src_w;
				} else {
					h = disp_h;
					w = disp_h * src_w / src_h;
				}
				*dst_w = w;
				*dst_h = h;
			}
		} else  if (output->info.mode == AML_SCREEN_MODE_FULL) {
			h = disp_h;
			w = disp_w;
			*dst_w = w;
			*dst_h = h;
		} else if (output->info.mode == AML_SCREEN_MODE_ADAPTIVE)  {
			post_blend_w =
			output->info.display_info.screen_vd_h_end_ -
			output->info.display_info.screen_vd_h_start_ + 1;
			post_blend_h =
			output->info.display_info.screen_vd_v_end_ -
			output->info.display_info.screen_vd_v_start_ + 1;
			window_w = output->info.display_info.display_hsc_endp -
			output->info.display_info.display_hsc_startp + 1;
			window_h = output->info.display_info.display_vsc_endp -
			output->info.display_info.display_vsc_startp + 1;
			*dst_w = (window_w * disp_w) / post_blend_w;
			*dst_h = (window_h * disp_h) / post_blend_h;
		}
	} else {
		if (angle % 180 == 0) {
			if (src_w < disp_w && src_h < disp_h) {
				w = disp_w;
				h = disp_h;
			} else if ((src_w * disp_h) >
				(disp_w * src_h)) {
				w = disp_w;
				h = disp_w * src_h / src_w;
			} else {
				h = disp_h;
				w = disp_h * src_w / src_h;
			}
			*dst_w = w;
			*dst_h = h;
		}
	}

	if (amlvideo2_dbg_en & 4)
		pr_info("%s: dst_w = %d, dst_h = %d.\n", __func__, *dst_w, *dst_h);
}

int amlvideo2_ge2d_interlace_two_canvasaddr_process(struct vframe_s *vf,
						    struct ge2d_context_s *
						    context,
						    struct config_para_ex_s *
						    ge2d_config,
						    struct amlvideo2_output *
						    output,
						    struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	bool is_tvp = false;
	int secure_size = 0;

	/* ============================ */
	/* top field */
	/* ============================ */
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		is_tvp = true;
		amlvideo2_secure_aipq_flag = 1;
	}

	if (node->crop_info.capture_crop_enable == 1) {
		if (node->crop_info.source_top_crop > 0 &&
		    node->crop_info.source_top_crop < vf->height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
		    node->crop_info.source_left_crop < vf->width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
		    node->crop_info.source_width_crop < (vf->width - src_left))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
		    node->crop_info.source_height_crop < (vf->height - src_top))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
					&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if ((src_width + src_left) > vf->width ||
		    (src_height + src_top) > vf->height ||
		    src_top < 0 || src_left < 0 ||
		    src_width <= 0 || src_height <= 0) {
			if (amlvideo2_dbg_en & 4)
				pr_info("%s: parameters is not match.\n", __func__);
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;
	if (node->porttype == TVIN_PORT_VIU1) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	//node->crop_info.source_top_crop = dst_top;
	//node->crop_info.source_left_crop = dst_left;
	//node->crop_info.source_width_crop = dst_width;
	//node->crop_info.source_height_crop = dst_height;

	if (amlvideo2_secure_aipq_flag) {
		if (amlvideo2_dbg_en & 4)
			pr_info("amlvideo2_secure_aipq_flag = %d , amlvideo2_secure_paddr = %lx\n",
				amlvideo2_secure_aipq_flag, amlvideo2_secure_paddr);
		if (!amlvideo2_secure_paddr) {
			pr_info("err:secure aipq but no secure phyaddr\n");
			return -1;
		}
		if (output->v4l2_format == V4L2_PIX_FMT_RGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_ARGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_BGR32) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB32 or ARGB32 or BGR32\n");
			secure_size = output->buf_width * output->buf_height * 4;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 4, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_RGB24 ||
				output->v4l2_format == V4L2_PIX_FMT_BGR24) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB24 or BGR24\n");
			secure_size = output->buf_width * output->buf_height * 3;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 3, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_NV12 ||
				output->v4l2_format == V4L2_PIX_FMT_NV21) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is NV12 or NV21\n");
			secure_size = output->buf_width * output->buf_height * 3 / 2;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			canvas_config((output_canvas >> 8) & 0xff,
					(unsigned long)amlvideo2_secure_paddr +
					output->buf_width * output->buf_height,
					output->buf_width, output->buf_height / 2,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else {
			pr_info("amlvideo2:do secure aipq, format err!!!\n");
		}
		if (secure_size > SECURE_BUFFER_SIZE_1M) {
			pr_info("error:amlvideo2 do secure aipq, output image size out 1M\n");
			return -1;
		}
		if (amlvideo2_dbg_en & 4)
			pr_info("secure aipq is doing\n");
	}

	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;
		ge2d_config->mem_sec = is_tvp;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->buf_width, output->buf_height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->mem_sec = is_tvp;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		if (vf->canvas0Addr != (u32)-1) {
			pr_info("src0_addr = %lx, w = %d, h = %d\n",
				cs0.addr, cs0.width, cs0.height);
			pr_info("src1_addr = %lx, w = %d, h = %d\n",
				cs1.addr, cs1.width, cs1.height);
			pr_info("src2_addr = %lx, w = %d, h = %d\n",
				cs2.addr, cs2.width, cs2.height);
		}
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top / 2,
			   src_width, src_height / 2,
			   output->frame->x, output->frame->y / 2,
			   output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 4, output->frame->w / 2,
				   output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420T & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 4, output->frame->w / 2,
				   output->frame->h / 4);
	}

	/* ============================ */
	/* bottom field */
	/* ============================ */
	output_canvas = vf->canvas1Addr;
	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;

	if (node->crop_info.capture_crop_enable == 1) {
		if (node->crop_info.source_top_crop > 0 &&
		    node->crop_info.source_top_crop < vf->height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
		    node->crop_info.source_left_crop < vf->width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
		    node->crop_info.source_width_crop < (vf->width - src_left))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
		    node->crop_info.source_height_crop < (vf->height - src_top))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
					&src_width, &src_height, output);
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;

	if (src_width < src_height)
		cur_angle = (cur_angle + 90) % 360;
	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	if (vf->canvas1Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas1_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas1_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas1_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas1_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas1_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas1_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas1_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas1_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas1_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas1_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas1_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas1_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas1Addr & 0xff, &cs0);
		canvas_read((vf->canvas1Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas1Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas1Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 *  vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = (get_output_format(output->v4l2_format)
		| GE2D_LITTLE_ENDIAN)
					| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height / 2;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, src_left, src_top / 2,
			   src_width, src_height / 2,
			   output->frame->x, output->frame->y / 2,
			   output->frame->w, output->frame->h / 2);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 4, output->frame->w / 2,
				   output->frame->h / 4);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			(GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN)
			| (GE2D_FORMAT_M24_YUV420B & (3 << 3));
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 4;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 4, output->frame->w / 2,
				   output->frame->h / 4);
	}

	return output_canvas;
}

int amlvideo2_ge2d_interlace_vdindata_process(struct vframe_s *vf,
					      struct ge2d_context_s *
					      context,
					      struct config_para_ex_s *
					      ge2d_config,
					      struct amlvideo2_output *
					      output,
					      struct amlvideo2_node *
					      node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (node->crop_info.capture_crop_enable == 1) {
		if (node->crop_info.source_top_crop > 0 &&
		    node->crop_info.source_top_crop < vf->height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
		    node->crop_info.source_left_crop < vf->width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
		    node->crop_info.source_width_crop < (vf->width - src_left))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = vf->width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
		    node->crop_info.source_height_crop < (vf->height - src_top))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = vf->height - src_top;
	} else {
		if (node->has_amvideo_node) {
			src_axis_adjust(&src_top, &src_left,
					&src_width, &src_height, output);
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("src_width = %d, src_left = %d\n",
				src_width, src_left);
			pr_info("src_height = %d, src_top = %d\n",
				src_height, src_top);
		}
		if ((src_width + src_left) > vf->width ||
		    (src_height + src_top) > vf->height ||
		    src_top < 0 || src_left < 0 ||
		    src_width <= 0 || src_height <= 0) {
			if (amlvideo2_dbg_en & 4)
				pr_info("%s: parameters is not match.\n", __func__);
			src_width = vf->width;
			src_height = vf->height;
			src_top = 0;
			src_left = 0;
		}
		src_top = src_top & 0xfffffffe;
		src_left = src_left & 0xfffffffe;
		src_width = src_width & 0xfffffffe;
		src_height = src_height & 0xfffffffe;
	}

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;
	if (node->porttype == TVIN_PORT_VIU1) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	//node->crop_info.source_top_crop = dst_top;
	//node->crop_info.source_left_crop = dst_left;
	//node->crop_info.source_width_crop = dst_width;
	//node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->buf_width, output->buf_height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	src_height = src_height / 2;
	src_top = src_top / 2;
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_interlace_input_format(vf, output);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(output->v4l2_format) |
						GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		if (vf->canvas0Addr != (u32)-1) {
			pr_info("src0_addr = %lx, w = %d, h = %d\n",
				cs0.addr, cs0.width, cs0.height);
			pr_info("src1_addr = %lx, w = %d, h = %d\n",
				cs1.addr, cs1.width, cs1.height);
			pr_info("src2_addr = %lx, w = %d, h = %d\n",
				cs2.addr, cs2.width, cs2.height);
		}
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top, src_width, src_height,
			   output->frame->x, output->frame->y, output->frame->w,
			   output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height,
				   output->frame->x / 2, output->frame->y / 2,
				   output->frame->w / 2, output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height,
				   output->frame->x / 2, output->frame->y / 2,
				   output->frame->w / 2, output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_interlace_one_canvasaddr_process(struct vframe_s *vf,
						    struct ge2d_context_s *
						    context,
						    struct config_para_ex_s *
						    ge2d_config,
						    struct amlvideo2_output *
						    output,
						    struct amlvideo2_node *
						    node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	struct amlvideo2_fh *fh = node->fh;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	bool is_tvp = false;
	int secure_size = 0;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}
	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		is_tvp = true;
		amlvideo2_secure_aipq_flag = 1;
	}
	if (node->crop_info.capture_crop_enable == 1 && node->vid == 0) {
		if (node->crop_info.source_top_crop > 0 &&
			node->crop_info.source_top_crop < fh->vinfo_data.height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
			node->crop_info.source_left_crop < fh->vinfo_data.width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
			(node->crop_info.source_width_crop <
			 (fh->vinfo_data.width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = fh->vinfo_data.width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			 (fh->vinfo_data.height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = fh->vinfo_data.height - src_top;
	}

	//amlvideo2.0 screencap, ioctl crop is relative to vinfo
	if (node->vid == 0) {
		src_top = src_top * vf->height / fh->vinfo_data.height;
		src_left = src_left * vf->width / fh->vinfo_data.width;
		src_width = src_width * vf->width / fh->vinfo_data.width;
		src_height = src_height * vf->height / fh->vinfo_data.height;
	} else if (node->vid == 1) {
		src_top = 0;
		src_left = 0;
		src_width = vf->width;
		src_height = vf->height;
	}

	if (amlvideo2_dbg_en & 4)
		pr_info("src_top=%d, src_left=%d, src_width=%d, src_height=%d, crop_enable=%d\n",
			src_top, src_left, src_width, src_height,
			node->crop_info.capture_crop_enable);

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;
	if (node->porttype == TVIN_PORT_VIU1) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	//node->crop_info.source_top_crop = dst_top;
	//node->crop_info.source_left_crop = dst_left;
	//node->crop_info.source_width_crop = dst_width;
	//node->crop_info.source_height_crop = dst_height;

	if (amlvideo2_secure_aipq_flag) {
		if (amlvideo2_dbg_en & 4)
			pr_info("amlvideo2_secure_aipq_flag = %d , amlvideo2_secure_paddr = %lx\n",
				amlvideo2_secure_aipq_flag, amlvideo2_secure_paddr);
		if (!amlvideo2_secure_paddr) {
			pr_info("err:secure aipq but no secure phyaddr\n");
			return -1;
		}
		if (output->v4l2_format == V4L2_PIX_FMT_RGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_ARGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_BGR32) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB32 or ARGB32 or BGR32\n");
			secure_size = output->buf_width * output->buf_height * 4;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 4, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_RGB24 ||
				output->v4l2_format == V4L2_PIX_FMT_BGR24) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB24 or BGR24\n");
			secure_size = output->buf_width * output->buf_height * 3;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 3, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_NV12 ||
				output->v4l2_format == V4L2_PIX_FMT_NV21) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is NV12 or NV21\n");
			secure_size = output->buf_width * output->buf_height * 3 / 2;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			canvas_config((output_canvas >> 8) & 0xff,
					(unsigned long)amlvideo2_secure_paddr +
					output->buf_width * output->buf_height,
					output->buf_width, output->buf_height / 2,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else {
			pr_info("amlvideo2:do secure aipq, format err!!!\n");
		}
		if (secure_size > SECURE_BUFFER_SIZE_1M) {
			pr_info("error:amlvideo2 do secure aipq, output image size out 1M\n");
			return -1;
		}
		if (amlvideo2_dbg_en & 4)
			pr_info("secure aipq is doing\n");
	}

	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;
		ge2d_config->mem_sec = is_tvp;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->buf_width, output->buf_height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->mem_sec = is_tvp;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format_no_interlace(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height / 2;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(output->v4l2_format) |
						GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		if (vf->canvas0Addr != (u32)-1) {
			pr_info("src0_addr = %lx, w = %d, h = %d\n",
				cs0.addr, cs0.width, cs0.height);
			pr_info("src1_addr = %lx, w = %d, h = %d\n",
				cs1.addr, cs1.width, cs1.height);
			pr_info("src2_addr = %lx, w = %d, h = %d\n",
				cs2.addr, cs2.width, cs2.height);
		}
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top / 2,
			   src_width, src_height / 2,
			   output->frame->x, output->frame->y,
			   output->frame->w, output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 2, output->frame->w / 2,
				   output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 2, output->frame->w / 2,
				   output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_interlace_dtv_process(struct vframe_s *vf,
					 struct ge2d_context_s *context,
					 struct config_para_ex_s *ge2d_config,
					 struct amlvideo2_output *output,
					 struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	struct amlvideo2_fh *fh = node->fh;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	bool is_tvp = false;
	int secure_size = 0;

	src_top = 0;
	src_left = 0;
	src_width = vf->width;
	/* src_height = vf->height/2; */
	src_height = vf->height;
	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d\n",
			node->crop_info.capture_crop_enable);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d\n\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
	}

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		is_tvp = true;
		amlvideo2_secure_aipq_flag = 1;
	}

	if (node->crop_info.capture_crop_enable == 1 && node->vid == 0) {
		if (node->crop_info.source_top_crop > 0 &&
			node->crop_info.source_top_crop < fh->vinfo_data.height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
			node->crop_info.source_left_crop < fh->vinfo_data.width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
			(node->crop_info.source_width_crop <
			 (fh->vinfo_data.width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = fh->vinfo_data.width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			 (fh->vinfo_data.height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = fh->vinfo_data.height - src_top;
	}

	//amlvideo2.0 screencap, ioctl crop is relative to vinfo
	if (node->vid == 0) {
		src_top = src_top * vf->height / fh->vinfo_data.height;
		src_left = src_left * vf->width / fh->vinfo_data.width;
		src_width = src_width * vf->width / fh->vinfo_data.width;
		src_height = src_height * vf->height / fh->vinfo_data.height;
	} else if (node->vid == 1) {
		src_top = 0;
		src_left = 0;
		src_width = vf->width;
		src_height = vf->height;
	}

	if (amlvideo2_dbg_en & 4)
		pr_info("src_top=%d, src_left=%d, src_width=%d, src_height=%d, crop_enable=%d\n",
			src_top, src_left, src_width, src_height,
			node->crop_info.capture_crop_enable);

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;
	if (node->porttype == TVIN_PORT_VIU1) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	//node->crop_info.source_top_crop = dst_top;
	//node->crop_info.source_left_crop = dst_left;
	//node->crop_info.source_width_crop = dst_width;
	//node->crop_info.source_height_crop = dst_height;

	if (amlvideo2_secure_aipq_flag) {
		if (amlvideo2_dbg_en & 4)
			pr_info("amlvideo2_secure_aipq_flag = %d , amlvideo2_secure_paddr = %lx\n",
				amlvideo2_secure_aipq_flag, amlvideo2_secure_paddr);
		if (!amlvideo2_secure_paddr) {
			pr_info("err:secure aipq but no secure phyaddr\n");
			return -1;
		}
		if (output->v4l2_format == V4L2_PIX_FMT_RGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_ARGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_BGR32) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB32 or ARGB32 or BGR32\n");
			secure_size = output->buf_width * output->buf_height * 4;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 4, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_RGB24 ||
				output->v4l2_format == V4L2_PIX_FMT_BGR24) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB24 or BGR24\n");
			secure_size = output->buf_width * output->buf_height * 3;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 3, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_NV12 ||
				output->v4l2_format == V4L2_PIX_FMT_NV21) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is NV12 or NV21\n");
			secure_size = output->buf_width * output->buf_height * 3 / 2;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			canvas_config((output_canvas >> 8) & 0xff,
					(unsigned long)amlvideo2_secure_paddr +
					output->buf_width * output->buf_height,
					output->buf_width, output->buf_height / 2,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else {
			pr_info("amlvideo2:do secure aipq, format err!!!\n");
		}
		if (secure_size > SECURE_BUFFER_SIZE_1M) {
			pr_info("error:amlvideo2 do secure aipq, output image size out 1M\n");
			return -1;
		}
		if (amlvideo2_dbg_en & 4)
			pr_info("secure aipq is doing\n");
	}

	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;
		ge2d_config->mem_sec = is_tvp;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
				(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->buf_width, output->buf_height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->mem_sec = is_tvp;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_interlace_input_format(vf, output);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(output->v4l2_format) |
						GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	if (amlvideo2_dbg_en & 4) {
		if (vf->canvas0Addr != (u32)-1) {
			pr_info("src0_addr = %lx, w = %d, h = %d\n",
				cs0.addr, cs0.width, cs0.height);
			pr_info("src1_addr = %lx, w = %d, h = %d\n",
				cs1.addr, cs1.width, cs1.height);
			pr_info("src2_addr = %lx, w = %d, h = %d\n",
				cs2.addr, cs2.width, cs2.height);
		}
		pr_info("output: w = %d, h = %d, output_canvas = %x\n\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top, src_width,  src_height,
			   output->frame->x, output->frame->y, output->frame->w,
			   output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 2, output->frame->w / 2,
				   output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top / 2, src_width,
				   src_height / 2, output->frame->x / 2,
				   output->frame->y / 2, output->frame->w / 2,
				   output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_multi_pre_process(struct vframe_s *vf,
				     struct ge2d_context_s *context,
				     struct config_para_ex_s *ge2d_config,
				     struct amlvideo2_output *output,
				     struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	int temp_canvas;
	unsigned long temp_start = node->aml2_dev->buffer_start;
	int temp_w = vf->width / 4;
	int temp_h = vf->height / 4;
	u32 h_scale_coef_type =
		context->config.h_scale_coef_type;
	u32 v_scale_coef_type =
		context->config.v_scale_coef_type;

	temp_w = (temp_w + 31) & (~31);
	temp_h = (temp_h + 1) & (~1);

	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
	}

	if (yuv420_canvas[node->vid][0] < 0)
		yuv420_canvas[node->vid][0] =
			canvas_pool_map_alloc_canvas("amlvideo2.1");
	if (yuv420_canvas[node->vid][0] < 0) {
		pr_info("amlvideo2 has no canvas to use %d\n", __LINE__);
		return -1;
	}

	temp_canvas = yuv420_canvas[node->vid][0];

	if (amlvideo2_scaledown1 == 0) {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	} else if (amlvideo2_scaledown1 == 1) {
		context->config.h_scale_coef_type = FILTER_TYPE_BILINEAR;
		context->config.v_scale_coef_type = FILTER_TYPE_BILINEAR;
	} else if (amlvideo2_scaledown1 == 2) {
		context->config.h_scale_coef_type = FILTER_TYPE_TRIANGLE;
		context->config.v_scale_coef_type = FILTER_TYPE_TRIANGLE;
	} else {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	}
	canvas_config(temp_canvas,
		      (unsigned long)temp_start,
		      temp_w, temp_h,
		      CANVAS_ADDR_NOWRAP,
		      CANVAS_BLKMODE_LINEAR);

	src_top = 4;
	src_left = 4;
	src_width = vf->width;
	src_height = vf->height;
	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = 0;

	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;
	//node->crop_info.source_top_crop = dst_top;
	//node->crop_info.source_left_crop = dst_left;
	//node->crop_info.source_width_crop = dst_width;
	//node->crop_info.source_height_crop = dst_height;
	/* data operating. */

	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = temp_canvas & 0xff;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = temp_w;
	ge2d_config->dst_para.height = temp_h;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, src_left, src_top, src_width - src_left,
			   src_height - src_top, 0, 0, temp_w, temp_h);

	if (amlvideo2_scaledown2 == 0) {
		context->config.h_scale_coef_type = FILTER_TYPE_BICUBIC;
		context->config.v_scale_coef_type = FILTER_TYPE_BICUBIC;
	} else if (amlvideo2_scaledown2 == 1) {
		context->config.h_scale_coef_type = FILTER_TYPE_BILINEAR;
		context->config.v_scale_coef_type = FILTER_TYPE_BILINEAR;
	} else if (amlvideo2_scaledown2 == 2) {
		context->config.h_scale_coef_type = FILTER_TYPE_TRIANGLE;
		context->config.v_scale_coef_type = FILTER_TYPE_TRIANGLE;
	}
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(temp_canvas & 0xff, &cs0);
	ge2d_config->src_planes[0].addr = cs0.addr;
	ge2d_config->src_planes[0].w = cs0.width;
	ge2d_config->src_planes[0].h = cs0.height;
	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.canvas_index = temp_canvas;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = temp_w;
	ge2d_config->src_para.height = temp_h;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = GE2D_FORMAT_S8_Y | GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}
	stretchblt_noalpha(context, 0, 0, temp_w, temp_h,
			   0, 0, output->width, output->height);

	context->config.h_scale_coef_type =
		h_scale_coef_type;
	context->config.v_scale_coef_type =
		v_scale_coef_type;
	return output_canvas;
}

int amlvideo2_ge2d_pre_process(struct vframe_s *vf,
			       struct ge2d_context_s *context,
			       struct config_para_ex_s *ge2d_config,
			       struct amlvideo2_output *output,
			       struct amlvideo2_node *node)
{
	int src_top, src_left, src_width, src_height;
	int dst_top, dst_left, dst_width, dst_height;
	struct canvas_s cs0, cs1, cs2, cd;
	struct amlvideo2_fh *fh = node->fh;
	int current_mirror;
	int cur_angle = 0;
	int output_canvas = output->canvas_id;
	bool is_tvp = false;
	int secure_size = 0;

	if (amlvideo2_dbg_en & 4) {
		pr_info("vf->width = %d, vf->height = %d\n",
			vf->width, vf->height);
		pr_info("vf->type = %x, vf->src_canvas = %x\n",
			vf->type, vf->canvas0Addr);
		pr_info("crop_enable = %d, node->has_amvideo_node = %d.\n",
			node->crop_info.capture_crop_enable, node->has_amvideo_node);
		pr_info("crop_top = %d, crop_left = %d\n",
			node->crop_info.source_top_crop,
			node->crop_info.source_left_crop);
		pr_info("crop_width = %d, crop_height = %d.\n",
			node->crop_info.source_width_crop,
			node->crop_info.source_height_crop);
		pr_info("output->angle =%d\n", output->angle);
		pr_info("vf->original_width = %d, vf->original_height = %d\n",
			vf->original_width, vf->original_height);
	}

	fh->ext_output_data.input_w = vf->original_width;
	fh->ext_output_data.input_h = vf->original_height;
	fh->ext_output_data.output_w = vf->width;
	fh->ext_output_data.output_h = vf->height;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		is_tvp = true;
		amlvideo2_secure_aipq_flag = 1;
	}

	if (node->crop_info.capture_crop_enable == 1 && node->vid == 0) {
		if (node->crop_info.source_top_crop > 0 &&
			node->crop_info.source_top_crop < fh->vinfo_data.height)
			src_top = node->crop_info.source_top_crop;
		else
			src_top = 0;

		if (node->crop_info.source_left_crop > 0 &&
			node->crop_info.source_left_crop < fh->vinfo_data.width)
			src_left = node->crop_info.source_left_crop;
		else
			src_left = 0;

		if (node->crop_info.source_width_crop > 0 &&
			(node->crop_info.source_width_crop <
			 (fh->vinfo_data.width - src_left)))
			src_width = node->crop_info.source_width_crop;
		else
			src_width = fh->vinfo_data.width - src_left;

		if (node->crop_info.source_height_crop > 0 &&
			(node->crop_info.source_height_crop <
			 (fh->vinfo_data.height - src_top)))
			src_height = node->crop_info.source_height_crop;
		else
			src_height = fh->vinfo_data.height - src_top;
	}

	//amlvideo2.0 screencap, ioctl crop is relative to vinfo
	if (node->vid == 0) {
		src_top = src_top * vf->height / fh->vinfo_data.height;
		src_left = src_left * vf->width / fh->vinfo_data.width;
		src_width = src_width * vf->width / fh->vinfo_data.width;
		src_height = src_height * vf->height / fh->vinfo_data.height;
	} else if (node->vid == 1) {
		src_top = 0;
		src_left = 0;
		src_width = vf->width;
		src_height = vf->height;
	} else {
		src_top = 0;
		src_left = 0;
		src_width = 0;
		src_height = 0;
	}

	if (amlvideo2_dbg_en & 4)
		pr_info("src_top=%d, src_left=%d, src_width=%d, src_height=%d, crop_enable=%d\n",
			src_top, src_left, src_width, src_height,
			node->crop_info.capture_crop_enable);

	dst_top = 0;
	dst_left = 0;
	dst_width = output->width;
	dst_height = output->height;

	current_mirror = mirror_value;
	cur_angle = output->angle;
	if (current_mirror == 1)
		cur_angle = (360 - cur_angle % 360);
	else
		cur_angle = cur_angle % 360;
	if (node->porttype == TVIN_PORT_VIU1) {
		if (src_width < src_height)
			cur_angle = (cur_angle + 90) % 360;
	}

	if (node->crop_info.capture_crop_enable == 0 &&
	    (node->porttype != TVIN_PORT_VIU1_VIDEO &&
	     node->porttype != TVIN_PORT_VIU1_WB0_VD1 &&
	     node->porttype != TVIN_PORT_VIU2_VD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD1 &&
	     node->porttype != TVIN_PORT_VIU1_WB0_OSD2 &&
	     node->porttype != TVIN_PORT_VIU2_OSD1 &&
	     node->porttype != TVIN_PORT_VIU3_OSD1)) {
		output_axis_adjust(src_width, src_height,
				   &dst_width, &dst_height,
				   cur_angle, output);
	}
	dst_width = dst_width & 0xfffffffe;
	dst_height = dst_height & 0xfffffffe;
	dst_top = (output->height - dst_height) / 2;
	dst_left = (output->width - dst_width) / 2;
	dst_top = dst_top & 0xfffffffe;
	dst_left = dst_left & 0xfffffffe;

	if (amlvideo2_secure_aipq_flag) {
		if (amlvideo2_dbg_en & 4)
			pr_info("amlvideo2_secure_aipq_flag = %d , amlvideo2_secure_paddr = %lx\n",
				amlvideo2_secure_aipq_flag, amlvideo2_secure_paddr);
		if (!amlvideo2_secure_paddr) {
			pr_info("err:secure aipq but no secure phyaddr\n");
			return -1;
		}
		if (output->v4l2_format == V4L2_PIX_FMT_RGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_ARGB32 ||
			output->v4l2_format == V4L2_PIX_FMT_BGR32) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB32 or ARGB32 or BGR32\n");
			secure_size = output->buf_width * output->buf_height * 4;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 4, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_RGB24 ||
				output->v4l2_format == V4L2_PIX_FMT_BGR24) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is RGB24 or BGR24\n");
			secure_size = output->buf_width * output->buf_height * 3;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width * 3, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else if (output->v4l2_format == V4L2_PIX_FMT_NV12 ||
				output->v4l2_format == V4L2_PIX_FMT_NV21) {
			if (amlvideo2_dbg_en & 4)
				pr_info("amlvideo2:do secure aipq, fmt is NV12 or NV21\n");
			secure_size = output->buf_width * output->buf_height * 3 / 2;
			canvas_config(output_canvas & 0xff,
					(unsigned long)amlvideo2_secure_paddr,
					output->buf_width, output->buf_height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			canvas_config((output_canvas >> 8) & 0xff,
					(unsigned long)amlvideo2_secure_paddr +
					output->buf_width * output->buf_height,
					output->buf_width, output->buf_height / 2,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		} else {
			pr_info("amlvideo2:do secure aipq, format err!!!\n");
		}
		if (secure_size > SECURE_BUFFER_SIZE_1M) {
			pr_info("error:amlvideo2 do secure aipq, output image size out 1M\n");
			return -1;
		}
		if (amlvideo2_dbg_en & 4)
			pr_info("secure aipq is doing\n");
	}

	/* data operating. */
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	if (dst_left != output->frame->x || dst_top != output->frame->y ||
	    dst_width != output->frame->w || dst_height != output->frame->h) {
		ge2d_config->alu_const_color = 0;
		ge2d_config->bitmask_en = 0;
		ge2d_config->src1_gb_alpha = 0;/* 0xff; */
		ge2d_config->dst_xy_swap = 0;
		ge2d_config->mem_sec = is_tvp;

		canvas_read(output_canvas & 0xff, &cd);
		ge2d_config->src_planes[0].addr = cd.addr;
		ge2d_config->src_planes[0].w = cd.width;
		ge2d_config->src_planes[0].h = cd.height;
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;

		ge2d_config->src_key.key_enable = 0;
		ge2d_config->src_key.key_mask = 0;
		ge2d_config->src_key.key_mode = 0;

		ge2d_config->src_para.canvas_index = output_canvas;
		ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->src_para.format = get_output_format
						(output->v4l2_format) |
						GE2D_LITTLE_ENDIAN;
		ge2d_config->src_para.fill_color_en = 0;
		ge2d_config->src_para.fill_mode = 0;
		ge2d_config->src_para.x_rev = 0;
		ge2d_config->src_para.y_rev = 0;
		ge2d_config->src_para.color = 0;
		ge2d_config->src_para.top = 0;
		ge2d_config->src_para.left = 0;
		ge2d_config->src_para.width = output->buf_width;
		ge2d_config->src_para.height = output->buf_height;

		ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

		ge2d_config->dst_para.canvas_index = output_canvas;
		ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
		ge2d_config->dst_para.format = get_output_format
						(output->v4l2_format) |
						GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.fill_color_en = 0;
		ge2d_config->dst_para.fill_mode = 0;
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
		ge2d_config->dst_para.color = 0;
		ge2d_config->dst_para.top = 0;
		ge2d_config->dst_para.left = 0;
		ge2d_config->dst_para.width = output->buf_width;
		ge2d_config->dst_para.height = output->buf_height;

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -2;
		}
		if (amlvideo2_dbg_en & 4) {
			pr_info("output_width = %d , output_height = %d\n",
				output->buf_width, output->buf_height);
			pr_info("dst_format = %x\n",
				ge2d_config->dst_para.format);
		}
		fillrect(context,
			 0,
			 0,
			 output->buf_width,
			 output->buf_height,
			 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
			 0x008080ff : 0);
		output->frame->x = dst_left;
		output->frame->y = dst_top;
		output->frame->w = dst_width;
		output->frame->h = dst_height;
		memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	}
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;
	ge2d_config->mem_sec = is_tvp;

	if (vf->canvas0Addr == (u32)-1) {
		canvas_config_config(node->aml2_canvas[0],
				     &vf->canvas0_config[0]);
		if (vf->plane_num == 2) {
			canvas_config_config(node->aml2_canvas[1],
					     &vf->canvas0_config[1]);
		} else if (vf->plane_num == 3) {
			canvas_config_config(node->aml2_canvas[2],
					     &vf->canvas0_config[2]);
		}
		ge2d_config->src_para.canvas_index =
			node->aml2_canvas[0]
			| (node->aml2_canvas[1] << 8)
			| (node->aml2_canvas[2] << 16);
		ge2d_config->src_planes[0].addr =
				vf->canvas0_config[0].phy_addr;
		ge2d_config->src_planes[0].w =
				vf->canvas0_config[0].width;
		ge2d_config->src_planes[0].h =
				vf->canvas0_config[0].height;
		ge2d_config->src_planes[1].addr =
				vf->canvas0_config[1].phy_addr;
		ge2d_config->src_planes[1].w =
				vf->canvas0_config[1].width;
		ge2d_config->src_planes[1].h =
				vf->canvas0_config[1].height >> 1;
		if (vf->plane_num == 3) {
			ge2d_config->src_planes[2].addr =
				vf->canvas0_config[2].phy_addr;
			ge2d_config->src_planes[2].w =
				vf->canvas0_config[2].width;
			ge2d_config->src_planes[2].h =
				vf->canvas0_config[2].height >> 1;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);
		ge2d_config->src_planes[0].addr = cs0.addr;
		ge2d_config->src_planes[0].w = cs0.width;
		ge2d_config->src_planes[0].h = cs0.height;
		ge2d_config->src_planes[1].addr = cs1.addr;
		ge2d_config->src_planes[1].w = cs1.width;
		ge2d_config->src_planes[1].h = cs1.height;
		ge2d_config->src_planes[2].addr = cs2.addr;
		ge2d_config->src_planes[2].w = cs2.width;
		ge2d_config->src_planes[2].h = cs2.height;
		ge2d_config->src_para.canvas_index = vf->canvas0Addr;
	}

	canvas_read(output_canvas & 0xff, &cd);
	if (amlvideo2_dbg_en & 4)
		pr_info("amlvideo2: add =%lx, %d*%d\n",
			cd.addr, cd.width, cd.height);

	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;
	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format = get_input_format(vf);
	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
		ge2d_config->src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0xffffffff;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = vf->width;
	ge2d_config->src_para.height = vf->height;
	/* pr_err("vf_width is %d ,
	 * vf_height is %d\n",vf->width ,vf->height);
	 */
	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.canvas_index = output_canvas & 0xff;

	if (output->v4l2_format != V4L2_PIX_FMT_YUV420 &&
	    output->v4l2_format != V4L2_PIX_FMT_YVU420)
		ge2d_config->dst_para.canvas_index = output_canvas;

	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format = get_output_format(output->v4l2_format) |
							GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->width;
	ge2d_config->dst_para.height = output->height;

	if (current_mirror == 1) {
		ge2d_config->dst_para.x_rev = 1;
		ge2d_config->dst_para.y_rev = 0;
	} else if (current_mirror == 2) {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 1;
	} else {
		ge2d_config->dst_para.x_rev = 0;
		ge2d_config->dst_para.y_rev = 0;
	}

	if (cur_angle == 90) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.x_rev ^= 1;
	} else if (cur_angle == 180) {
		ge2d_config->dst_para.x_rev ^= 1;
		ge2d_config->dst_para.y_rev ^= 1;
	} else if (cur_angle == 270) {
		ge2d_config->dst_xy_swap = 1;
		ge2d_config->dst_para.y_rev ^= 1;
	}

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -1;
	}

	if (amlvideo2_dbg_en & 4) {
		if (vf->canvas0Addr != (u32)-1) {
			pr_info("src0_addr = %lx, w = %d, h = %d\n",
				cs0.addr, cs0.width, cs0.height);
			pr_info("src1_addr = %lx, w = %d, h = %d\n",
				cs1.addr, cs1.width, cs1.height);
			pr_info("src2_addr = %lx, w = %d, h = %d\n",
				cs2.addr, cs2.width, cs2.height);
		}
		pr_info("output: w = %d, h = %d, output_canvas = %x\n",
			output->width, output->height, output_canvas);

		pr_info("%s:node->id = %d ,src_left = %d ,src_top = %d\n",
			__func__, node->vid, src_left, src_top);
		pr_info("src_width=%d ,src_height=%d ,output->frame->x=%d\n",
			src_width, src_height, output->frame->x);
		pr_info("output->frame->y=%d ,frame->w=%d ,frame->h=%d\n",
			output->frame->y, output->frame->w, output->frame->h);
	}
	stretchblt_noalpha(context, src_left, src_top, src_width, src_height,
			   output->frame->x, output->frame->y,
			   output->frame->w, output->frame->h);

	/* for cr of  yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 ||
	    output->v4l2_format == V4L2_PIX_FMT_YVU420) {
		/* for cb. */
		canvas_read((output_canvas >> 8) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 8) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CB | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height,
				   output->frame->x / 2,
				   output->frame->y / 2,
				   output->frame->w / 2,
				   output->frame->h / 2);
	}
	/* for cb of yuv420p or yuv420sp. */
	if (output->v4l2_format == V4L2_PIX_FMT_YUV420 || output->v4l2_format
		== V4L2_PIX_FMT_YVU420) {
		canvas_read((output_canvas >> 16) & 0xff, &cd);
		ge2d_config->dst_planes[0].addr = cd.addr;
		ge2d_config->dst_planes[0].w = cd.width;
		ge2d_config->dst_planes[0].h = cd.height;
		ge2d_config->dst_para.canvas_index =
			(output_canvas >> 16) & 0xff;
		ge2d_config->dst_para.format =
			GE2D_FORMAT_S8_CR | GE2D_LITTLE_ENDIAN;
		ge2d_config->dst_para.width = output->width / 2;
		ge2d_config->dst_para.height = output->height / 2;
		ge2d_config->dst_xy_swap = 0;

		if (current_mirror == 1) {
			ge2d_config->dst_para.x_rev = 1;
			ge2d_config->dst_para.y_rev = 0;
		} else if (current_mirror == 2) {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 1;
		} else {
			ge2d_config->dst_para.x_rev = 0;
			ge2d_config->dst_para.y_rev = 0;
		}

		if (cur_angle == 90) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.x_rev ^= 1;
		} else if (cur_angle == 180) {
			ge2d_config->dst_para.x_rev ^= 1;
			ge2d_config->dst_para.y_rev ^= 1;
		} else if (cur_angle == 270) {
			ge2d_config->dst_xy_swap = 1;
			ge2d_config->dst_para.y_rev ^= 1;
		}

		if (ge2d_context_config_ex(context, ge2d_config) < 0) {
			pr_err("++ge2d configing error.\n");
			return -1;
		}
		stretchblt_noalpha(context, src_left, src_top,
				   src_width, src_height,
				   output->frame->x / 2,
				   output->frame->y / 2,
				   output->frame->w / 2,
				   output->frame->h / 2);
	}
	return output_canvas;
}

int amlvideo2_ge2d_black_process(struct ge2d_context_s *context,
			       struct config_para_ex_s *ge2d_config,
			       struct amlvideo2_output *output)
{
	struct canvas_s cd;
	int output_canvas = output->canvas_id;

	/* data operating. */
	memset(ge2d_config, 0, sizeof(struct config_para_ex_s));
	ge2d_config->alu_const_color = 0;
	ge2d_config->bitmask_en = 0;
	ge2d_config->src1_gb_alpha = 0;/* 0xff; */
	ge2d_config->dst_xy_swap = 0;

	canvas_read(output_canvas & 0xff, &cd);
	ge2d_config->src_planes[0].addr = cd.addr;
	ge2d_config->src_planes[0].w = cd.width;
	ge2d_config->src_planes[0].h = cd.height;
	ge2d_config->dst_planes[0].addr = cd.addr;
	ge2d_config->dst_planes[0].w = cd.width;
	ge2d_config->dst_planes[0].h = cd.height;

	ge2d_config->src_key.key_enable = 0;
	ge2d_config->src_key.key_mask = 0;
	ge2d_config->src_key.key_mode = 0;

	ge2d_config->src_para.canvas_index = output_canvas;
	ge2d_config->src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->src_para.format =
		get_output_format(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
	if (output->v4l2_format == V4L2_PIX_FMT_RGB565X)
		ge2d_config->src_para.format &= ~GE2D_LITTLE_ENDIAN;
	ge2d_config->src_para.fill_color_en = 0;
	ge2d_config->src_para.fill_mode = 0;
	ge2d_config->src_para.x_rev = 0;
	ge2d_config->src_para.y_rev = 0;
	ge2d_config->src_para.color = 0;
	ge2d_config->src_para.top = 0;
	ge2d_config->src_para.left = 0;
	ge2d_config->src_para.width = output->buf_width;
	ge2d_config->src_para.height = output->buf_height;

	ge2d_config->src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config->dst_para.canvas_index = output_canvas;
	ge2d_config->dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config->dst_para.format =
		get_output_format(output->v4l2_format) | GE2D_LITTLE_ENDIAN;
	if (output->v4l2_format == V4L2_PIX_FMT_RGB565X)
		ge2d_config->src_para.format &= ~GE2D_LITTLE_ENDIAN;
	ge2d_config->dst_para.fill_color_en = 0;
	ge2d_config->dst_para.fill_mode = 0;
	ge2d_config->dst_para.x_rev = 0;
	ge2d_config->dst_para.y_rev = 0;
	ge2d_config->dst_para.color = 0;
	ge2d_config->dst_para.top = 0;
	ge2d_config->dst_para.left = 0;
	ge2d_config->dst_para.width = output->buf_width;
	ge2d_config->dst_para.height = output->buf_height;

	if (ge2d_context_config_ex(context, ge2d_config) < 0) {
		pr_err("++ge2d configing error.\n");
		return -2;
	}
	if (amlvideo2_dbg_en & 4) {
		pr_info("output_width = %d , output_height = %d\n",
			output->buf_width, output->buf_height);
		pr_info("dst_format = %x\n",
			ge2d_config->dst_para.format);
	}
	fillrect(context,
		 0,
		 0,
		 output->buf_width,
		 output->buf_height,
		 (ge2d_config->dst_para.format & GE2D_FORMAT_YUV) ?
		 0x008080ff : 0);
	return output_canvas;
}

static void config_dump_src_data(struct vframe_s *vf, struct amlvideo2_node *node)
{
	struct canvas_s cs0;

	if (!vf || !node)
		return;

	if (vf->canvas0Addr == (u32)-1) {
		node->dump_info_src.width = vf->canvas0_config[0].width;
		node->dump_info_src.height = vf->canvas0_config[0].height;
		node->dump_info_src.sizeimage = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height * 3 / 2;
		node->dump_info_src.phy_addr = vf->canvas0_config[0].phy_addr;
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		node->dump_info_src.width = cs0.width;
		node->dump_info_src.height = cs0.height;
		node->dump_info_src.sizeimage = cs0.width * cs0.height * 3 / 2;
		node->dump_info_src.phy_addr = cs0.addr;
	}
}

static void config_dump_dst_data(struct amlvideo2_output *output, struct amlvideo2_node *node)
{
	int output_canvas;
	struct canvas_s cd;

	if (!output || !node)
		return;

	output_canvas = output->canvas_id;
	canvas_read(output_canvas & 0xff, &cd);

	node->dump_info_dst.width = cd.width;
	node->dump_info_dst.height = cd.height;
	node->dump_info_dst.phy_addr = cd.addr;

	if (output->v4l2_format == V4L2_PIX_FMT_RGB24 ||
		output->v4l2_format == V4L2_PIX_FMT_BGR24 ||
		output->v4l2_format == V4L2_PIX_FMT_RGB32 ||
		output->v4l2_format == V4L2_PIX_FMT_ARGB32 ||
		output->v4l2_format == V4L2_PIX_FMT_BGR32)
		node->dump_info_dst.sizeimage = cd.width * cd.height;
	else
		node->dump_info_dst.sizeimage = cd.width * cd.height * 3 / 2;
}

static void dump_vf(struct vframe_s *vf, int type)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	loff_t pos;
	struct canvas_s cs0;

	if (!vf)
		return;

	if (type == 1)
		snprintf(name_buf, sizeof(name_buf), "/data/vdin.yuv");
	else
		snprintf(name_buf, sizeof(name_buf), "/data/vdin_%d.yuv", dump_num_increase);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;

	if (vf->canvas0Addr == (u32)-1) {
		write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height
			* 3 / 2;
		data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
		pr_info("amlvideo2:dump in %d %d, phy_addr0=%ld, type=%d\n",
			vf->canvas0_config[0].width,
			vf->canvas0_config[0].height,
			vf->canvas0_config[0].phy_addr,
			type);
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		write_size = cs0.width * cs0.height
			* 3 / 2;
		data = codec_mm_vmap(cs0.addr, write_size);
		pr_info("amlvideo2: dump in %d %d, phy_addr=%ld, type=%d\n",
			cs0.width,
			cs0.height,
			cs0.addr,
			type);
	}

	if (!data)
		return;
	pos = 0;
	kernel_write(fp, data, write_size, &pos);
	pr_info("amlvideo2: dump in %u size to addr=%p\n", write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
#endif
}

static void dump_output(struct amlvideo2_output *output, int type)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data;
	loff_t pos;
	int output_canvas;
	struct canvas_s cd;

	if (!output)
		return;

	output_canvas = output->canvas_id;
	canvas_read(output_canvas & 0xff, &cd);

	if (type == 1)
		snprintf(name_buf, sizeof(name_buf), "/data/amlvideo2.yuv");
	else
		snprintf(name_buf, sizeof(name_buf), "/data/amlvideo2_%d.yuv", dump_num_increase);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	if (amlvideo2_set_outfmt_dump == 1) {
		write_size = cd.width * cd.height;
		pr_info("amlvideo2:dump output %d %d, phy_addr=%ld\n",
			cd.width,
			cd.height,
			cd.addr);
	} else {
		write_size = cd.width * cd.height * 3 / 2;
		pr_info("amlvideo2: dump output %d %d, phy_addr=%ld\n",
			cd.width,
			cd.height,
			cd.addr);
	}

	data = codec_mm_vmap(cd.addr, write_size);
	if (!data)
		return;
	pos = 0;
	kernel_write(fp, data, write_size, &pos);
	pr_info("amlvideo2: dump output %u size to addr%p\n",
		write_size, data);
	codec_mm_unmap_phyaddr(data);
	filp_close(fp, NULL);
#endif
}

void amlvideo2_dispatch_event(struct amlvideo2_fh *aml_fh, u32 changes)
{
	struct v4l2_event event = {0};
	int dst1 = 1;

	switch (changes) {
	case V4L2_EVENT_PRIVATE_EXT_VDIN_PATTERN:
		event.type = V4L2_EVENT_PRIVATE_EXT_VDIN_PATTERN;
		event.id = 0;
		memcpy(&event.u.data[0], &dst1, sizeof(int));
		break;
	default:
		pr_info("amlvideo2: unsupport event\n");
		return;
	}

	v4l2_event_queue_fh(&aml_fh->fh, &event);
}

static int amlvideo2_fill_black(struct amlvideo2_fh *fh,
			      struct amlvideo2_node_buffer *buf)
{
	struct config_para_ex_s ge2d_config;
	struct amlvideo2_output output;
	struct amlvideo2_node *node = fh->node;
	void *vbuf = NULL;
	int ge2d_proc = 0;

	vbuf = (void *)buf->addr[0];

	if (!vbuf)
		return -1;

	/* memset(&ge2d_config,0,sizeof(struct config_para_ex_s)); */
	memset(&output, 0, sizeof(struct amlvideo2_output));

	output.v4l2_format = fh->fmt->fourcc;
	output.vbuf = vbuf;
	output.buf_width = fh->buf_width;
	output.buf_height = fh->buf_height;
	output.canvas_id = buf->canvas_id;
	output.angle = node->qctl_regs[0];
	output.frame = &buf->axis;
	output.info.mode = node->mode;
	output.bsize = fh->sizeimage;
	memcpy(&output.info.display_info, &node->display_info,
	       sizeof(struct vdisplay_info_s));

	output.canvas_id =
		get_amlvideo2_canvas_index(&output, node->vid, buf->vb2.vb2_buf.index);
	if (output.canvas_id < 0) {
		pr_info("amlvideo2: has no canvas value, %d\n", __LINE__);
		return -1;
	}

	switch (output.v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ge2d_proc = 1;
		break;
	default:
		break;
	}

	dma_sync_single_for_device(node->mem_form_dev,
		(dma_addr_t)output.vbuf, output.bsize, DMA_TO_DEVICE);

	if (ge2d_proc) {
#ifdef CONFIG_PM
		node->could_suspend = false;
#endif
	amlvideo2_ge2d_black_process(node->context,
		&ge2d_config, &output);
#ifdef CONFIG_PM
		node->could_suspend = true;
		if (atomic_read(&node->is_suspend))
			complete(&node->suspend_sema);
#endif
	}

	return 0;
}

static int amlvideo2_fillbuff(struct amlvideo2_fh *fh,
			      struct amlvideo2_node_buffer *buf,
			      struct vframe_s *vf)
{
	struct config_para_ex_s ge2d_config;
	struct amlvideo2_output output;
	struct amlvideo2_node *node = fh->node;
	void *vbuf = NULL;
	int src_canvas = -1;
	int ge2d_proc = 0;
	bool output_black;

	vbuf = (void *)buf->addr[0];

	if (!vbuf || !vf)
		return -1;

	if ((vf->type_ext & VIDTYPE_EXT_VDIN_HDCP) ||
		amlvideo2_force_report) {
		amlvideo2_dispatch_event(fh,
			V4L2_EVENT_PRIVATE_EXT_VDIN_PATTERN);
		pr_info("secure capture, report.\n");
	}

	if ((fh->secure_data.secure_capture_type ==
		V4L2_EXT_AMLVIDEO2_SECURE_TEE_ONE_BUFFER) || amlvideo2_secure_paddr)
		output_black = false;
	else if ((vf->type_ext & VIDTYPE_EXT_VDIN_HDCP) ||
		(vf->flag & VFRAME_FLAG_VIDEO_SECURE))
		output_black = true;
	else
		output_black = false;

	if (amlvideo2_dbg_en & 2)
		pr_info("%s: amlvideo2_force_report=%d.\n", __func__,
			amlvideo2_force_report);

	/* memset(&ge2d_config,0,sizeof(struct config_para_ex_s)); */
	memset(&output, 0, sizeof(struct amlvideo2_output));

	output.v4l2_format = fh->fmt->fourcc;
	output.vbuf = vbuf;
	output.width = fh->width;
	output.height = fh->height;
	output.buf_width = fh->buf_width;
	output.buf_height = fh->buf_height;
	output.canvas_id = buf->canvas_id;
	output.angle = node->qctl_regs[0];
	output.frame = &buf->axis;
	output.info.mode = node->mode;
	output.bsize = fh->sizeimage;
	memcpy(&output.info.display_info, &node->display_info,
	       sizeof(struct vdisplay_info_s));

	output.canvas_id =
		get_amlvideo2_canvas_index(&output, node->vid, buf->vb2.vb2_buf.index);
	if (output.canvas_id < 0) {
		pr_info("amlvideo2: has no canvas value, %d\n", __LINE__);
		return -1;
	}

	switch (output.v4l2_format) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ge2d_proc = 1;
		break;
	default:
		break;
	}
	src_canvas = vf->canvas0Addr;

	dma_sync_single_for_device(node->mem_form_dev,
		(dma_addr_t)output.vbuf, output.bsize, DMA_TO_DEVICE);

	if (ge2d_proc) {
#ifdef CONFIG_PM
		node->could_suspend = false;
#endif
		if (output_black) {
			amlvideo2_ge2d_black_process(node->context,
				&ge2d_config, &output);
		} else if ((vf->type & VIDTYPE_INTERLACE_BOTTOM) ||
			(vf->type & VIDTYPE_INTERLACE_TOP)) {
			if (vf->type & VIDTYPE_VIU_FIELD) {
				amlvideo2_ge2d_interlace_one_canvasaddr_process
					(vf, node->context, &ge2d_config, &output, node);
			} else {
				amlvideo2_ge2d_interlace_dtv_process
					(vf, node->context, &ge2d_config, &output, node);
			}
		} else {
			if (node->ge2d_multi_process_flag)
				src_canvas = amlvideo2_ge2d_multi_pre_process
					(vf, node->context, &ge2d_config, &output, node);
			else
				src_canvas = amlvideo2_ge2d_pre_process
					(vf, node->context, &ge2d_config, &output, node);
		}
		if ((vf->type_ext & VIDTYPE_EXT_VDIN_HDCP) &&
			(fh->secure_data.secure_capture_type !=
			V4L2_EXT_AMLVIDEO2_SECURE_TEE_ONE_BUFFER))
			fh->fill_frame_type = AML_HDCP_BLACK;
		else
			fh->fill_frame_type = AML_NORMAL;
#ifdef CONFIG_PM
		node->could_suspend = true;
		if (atomic_read(&node->is_suspend))
			complete(&node->suspend_sema);
#endif
	}

	if (node->dump_info_src.is_dump) {
		config_dump_src_data(vf, node);
		node->dump_info_src.config_dump_done = 1;
		wake_up_interruptible(&node->dump_info_src.dump_wq);
		mutex_lock(&node->dump_info_src.mutex);
		mutex_unlock(&node->dump_info_src.mutex);
	}
	if (node->dump_info_dst.is_dump) {
		config_dump_dst_data(&output, node);
		node->dump_info_dst.config_dump_done = 1;
		wake_up_interruptible(&node->dump_info_dst.dump_wq);
		mutex_lock(&node->dump_info_dst.mutex);
		mutex_unlock(&node->dump_info_dst.mutex);
	}
	if (amlvideo2_dump) {
		dump_vf(vf, 1);
		dump_output(&output, 1);
		amlvideo2_dump = 0;
	}
	if (amlvideo2_continue_dump) {
		dump_num_increase++;
		dump_vf(vf, 2);
		dump_output(&output, 2);
		amlvideo2_continue_dump--;
		if (!amlvideo2_continue_dump)
			dump_num_increase = 0;
	}

	return 0;
}

#define TEST_LATENCY

/* ------------------------------------------------------------------
 * queue operations
 * ------------------------------------------------------------------
 */

void vf_inqueue(struct vframe_s *vf, struct amlvideo2_node *node)
{
	struct vframe_receiver_s *vfp;
	const char *name = node->recv.name;

	if (!vf)
		return;

	vfp = vf_get_receiver(name);
	if (!vfp) {
		vf_put(vf, name);
		amlvideo2_total_put_count++;
		return;
	}

	vfq_push(&node->q_ready, vf);

	vf_notify_receiver(name,
			   VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);
}

static int amlvideo2_thread_tick(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node_buffer *buf = NULL;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	unsigned int diff = 0;
	bool no_frame = false;
	struct vframe_s *vf = NULL;
	unsigned long flags = 0;
	int active_duration = 0;
	int i_ret = 0;

	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("Enter amlvideo2.0 thread tick\n");
		else
			pr_info("Enter amlvideo2.1 thread tick\n");
	}

	if (kthread_should_stop()) {
		if (amlvideo2_dbg_en & 2)
			pr_info("amlvideo2 thread stop\n");
		return 0;
	}
	i_ret = wait_event_interruptible_timeout(dma_q->wq,
						 (vf_peek(node->recv.name) &&
						 node->provide_ready) ||
						 node->vidq.task_running == 0,
						 msecs_to_jiffies(5000));

	if (i_ret == 0 || node->vidq.task_running == 0) {
		if (node->pflag)
			complete(&node->plug_sema);
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0) {
				if (!vf_peek(node->recv.name))
					pr_info("amlvideo2.0 peek vf NULL\n");
				if (!node->provide_ready)
					pr_info("amlvideo2.0 no ready\n");
				if (node->vidq.task_running == 0)
					pr_info("amlvideo2.0 no running\n");
			} else {
				if (!vf_peek(node->recv.name))
					pr_info("amlvideo2.1 peek vf NULL\n");
				if (!node->provide_ready)
					pr_info("amlvideo2.1 no ready\n");
				if (node->vidq.task_running == 0)
					pr_info("amlvideo2.1 no running\n");
			}
		}
		return 0;
	}

	if (node->r_type != AML_RECEIVER_NONE &&
	    vfq_full(&node->q_ready)) {
		usleep_range(5000, 6000);
		if (amlvideo2_dbg_en & 2)
			pr_info("q_ready full ,receiver is none\n");
		return -1;
	}

	if (node->video_blocking && node->amlvideo2_pool_ready) {
		vfq_init(&node->q_ready,
			 node->amlvideo2_pool_size,
			 (struct vframe_s **)&node->amlvideo2_pool_ready[0]);
		node->video_blocking = false;
		node->tmp_vf = NULL;
		pr_err("video blocking need to reset@!!!!!\n");
		return 0;
	}

	if (!fh->is_streamed_on) {
		if (amlvideo2_dbg_en & 2)
			pr_info("dev doesn't stream on\n");
		if (node->r_type != AML_RECEIVER_NONE) {
			if (node->tmp_vf) {
				vf_inqueue(node->tmp_vf, node);
				node->tmp_vf = NULL;
			}
		} else {
			node->tmp_vf = NULL;
		}
		while (vf_peek(node->recv.name) &&
		       !vfq_full(&node->q_ready)) {
			if (amlvideo2_dbg_en & 2)
				pr_info("while 1.\n");
			vf = vf_get(node->recv.name);
			if (vf)
				amlvideo2_total_get_count++;
			vf_inqueue(vf, node);
			if (vf) {
				fh->src_width  =  vf->width;
				fh->src_height  =  vf->height;
			}
		}
		if (vfq_full(&node->q_ready) || !vf_peek(node->recv.name))
			usleep_range(5000, 6000);
		return 0;
	}

	if (!node->provide_ready) {
		if (amlvideo2_dbg_en & 2)
			pr_info("provide is not ready .\n");
		if (node->pflag)
			complete(&node->plug_sema);
		return -1;
	}

	if (node->pflag) {
		complete(&node->plug_sema);
		return 0;
	}

	if (list_empty(&dma_q->head)) {
		if (amlvideo2_dbg_en & 2)
			pr_info("No active queue to serve .\n");
		while (vf_peek(node->recv.name) &&
		       !vfq_full(&node->q_ready)) {
			if (amlvideo2_dbg_en & 2)
				pr_info("while 2.\n");
			vf = vf_get(node->recv.name);
			if (vf)
				amlvideo2_total_get_count++;
			vf_inqueue(vf, node);
		}
		goto unlock;
	}

	buf = list_first_entry(&dma_q->head,
			 struct amlvideo2_node_buffer,
			 list);

	if (amlvideo2_dbg_en & 2)
		pr_info("ready videobuf2 to fill data .\n");

	if (!vf_peek(node->recv.name)) {
		no_frame = true;
	} else {
		/* drop the frame to get the last one */
		if (!vfq_full(&node->q_ready)) {
			vf = vf_get(node->recv.name);
			if (vf) {
				amlvideo2_total_get_count++;
				if (vf->type & VIDTYPE_V4L_EOS) {
					vf_inqueue(vf, node);
					goto unlock;
				}
				if (((vf->type & VIDTYPE_TYPEMASK) ==
					   VIDTYPE_INTERLACE_TOP) &&
				    node->field_flag) {
					node->field_flag = false;
					node->field_condition_flag = true;
				}
			} else {
				pr_info("%s:vf is NULL.\n", __func__);
			}
			while (vf_peek(node->recv.name) &&
			       !node->field_condition_flag) {
				if (amlvideo2_dbg_en & 2)
					pr_info("while 3.\n");
				vf_inqueue(vf, node);
				if (!vfq_full(&node->q_ready)) {
					vf = vf_get(node->recv.name);
					if (vf)
						amlvideo2_total_get_count++;
					if (vf &&
					    ((vf->type & VIDTYPE_TYPEMASK) ==
					     VIDTYPE_INTERLACE_TOP) &&
					    node->field_flag) {
						node->field_flag = false;
						break;
					}
				} else {
					break;
				}
			}
			if (vf) {
				if (vf->type & VIDTYPE_V4L_EOS) {
					vf_inqueue(vf, node);
					goto unlock;
				}
				if (((vf->type & VIDTYPE_TYPEMASK) ==
				     VIDTYPE_INTERLACE_BOTTOM) &&
				    vf->canvas0Addr == vf->canvas1Addr) {
					vf_inqueue(vf, node);
					no_frame = true;
					vf = NULL;
					node->field_flag = true;
				}
			}
		}
		node->field_condition_flag = false;
	}

	#ifdef USE_VDIN_PTS
	if (no_frame)
		goto unlock;
	if (node->frame_inittime == 1) {
		if (node->tmp_vf) {
			vf_inqueue(node->tmp_vf, node);
			node->tmp_vf = NULL;
		}
		node->frameinv_adjust = 0;
		node->frameinv = 0;
		node->thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
		node->thread_ts1.tv_nsec = vf->pts * 1000;
		node->frame_inittime = 0;
	} else {
		diff = (vf->pts_us64 & 0xFFFFFFFF) - node->thread_ts1.tv_sec;
		diff = diff * 1000000 + vf->pts - node->thread_ts1.tv_nsec / 1000;
		if (diff < (int)fh->frm_save_time_us) {
			vf_inqueue(vf, node);
			goto unlock;
		}
	}
	#else
	if (node->frame_inittime == 1) {
		if (node->tmp_vf) {
			vf_inqueue(node->tmp_vf, node);
			node->tmp_vf = NULL;
		}
		if (no_frame)
			goto unlock;
		node->frameinv_adjust = 0;
		node->frameinv = 0;
		ktime_get_ts64(&node->thread_ts1);
		node->frame_inittime = 0;
	} else {
		ktime_get_ts64(&node->thread_ts2);
		/* thread_ts2.tv_sec = vf->pts_us64& 0xFFFFFFFF; */
		/* thread_ts2.tv_usec = vf->pts; */
		diff = node->thread_ts2.tv_sec - node->thread_ts1.tv_sec;
		diff = diff * 1000000 + node->thread_ts2.tv_nsec / 1000 -
			node->thread_ts1.tv_nsec / 1000;
		node->frameinv += diff;
		memcpy(&node->thread_ts1, &node->thread_ts2,
		       sizeof(struct timespec64));
		active_duration = node->frameinv - node->frameinv_adjust;
		/* Fill buffer */
		if (active_duration + 5000 > (int)fh->frm_save_time_us) {
			/* ||(active_duration  > (int)fh->frm_save_time_us) */
			if (vf) {
				if (node->tmp_vf)
					vf_inqueue(node->tmp_vf, node);

				node->tmp_vf = NULL;
			} else if (node->tmp_vf) {
				vf = node->tmp_vf;
				node->tmp_vf = NULL;
			} else {
				goto unlock;
			}
		} else {
			if (vf) {
				if (node->tmp_vf)
					vf_inqueue(node->tmp_vf, node);

				node->tmp_vf = vf;
			}
			goto unlock;
		}
	}
	while (active_duration >= (int)fh->frm_save_time_us) {
		if (amlvideo2_dbg_en & 2)
			pr_info("while 4.\n");
		active_duration -= fh->frm_save_time_us;
	}

	if ((active_duration + 5000) > fh->frm_save_time_us)
		node->frameinv_adjust = fh->frm_save_time_us - active_duration;
	else
		node->frameinv_adjust = -active_duration;
	node->frameinv = 0;
	#endif

	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("node0 fillbuff start .\n");
		else
			pr_info("node1 fillbuff start .\n");
	}

	if (amlvideo2_fillbuff(fh, buf, vf) < 0)
		pr_info("amlvideo2 fill buff error!\n");

	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("node0 fillbuff end .\n");
		else
			pr_info("node1 fillbuff end .\n");
	}
	#ifdef USE_VDIN_PTS
	buf->vb2.vb2_buf.timestamp = vf->pts_us64 * 1000000 + vf->pts;
	node->thread_ts1.tv_sec = vf->pts_us64 & 0xFFFFFFFF;
	node->thread_ts1.tv_nsec = vf->pts * 1000;
	#else
	buf->vb2.vb2_buf.timestamp = node->thread_ts2.tv_sec * 1000000
		+ node->thread_ts2.tv_nsec / 1000;
	#endif
	vf_inqueue(vf, node);

	while (vf_peek(node->recv.name) &&
	       !vfq_full(&node->q_ready)) {
		if (amlvideo2_dbg_en & 2)
			pr_info("while 5.\n");
		vf = vf_get(node->recv.name);
		if (vf)
			amlvideo2_total_get_count++;
		vf_inqueue(vf, node);
	}

	if (amlvideo2_dbg_en & 2)
		pr_info("spin_lock_irqsave1.\n");

	spin_lock_irqsave(&dma_q->buff_list_lock, flags);
	buf->vb2.timecode.flags = 0;
	if (buf->vb2.vb2_buf.state == VB2_BUF_STATE_ACTIVE) {
		if (fh->fill_frame_type == AML_HDCP_BLACK) {
			//private:bit1 is 1 means it is hdcp black frame
			buf->vb2.timecode.flags |= (1 << 1);
		}

		vb2_buffer_done(&buf->vb2.vb2_buf, VB2_BUF_STATE_DONE);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&dma_q->buff_list_lock, flags);
	if (amlvideo2_dbg_en & 2)
		pr_info("finish spin_lock_irqsave1.\n");

	if (node->pflag) {
		complete(&node->plug_sema);
		return 0;
	}

	if (amlvideo2_dbg_en & 2)
		pr_info("filled buffer %p\n", buf);

	return 0;

unlock:
	if (node->pflag)
		complete(&node->plug_sema);
	return 0;
}

static int amlvideo2_thread_tick_black(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node_buffer *buf = NULL;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	unsigned int diff = 0;
	unsigned long flags = 0;
	int active_duration = 0;

	if (amlvideo2_dbg_en & 2) {
		if (node->vid == 0)
			pr_info("Enter amlvideo2.0 %s\n", __func__);
		else
			pr_info("Enter amlvideo2.1 %s\n", __func__);
	}

	if (kthread_should_stop()) {
		if (amlvideo2_dbg_en & 2)
			pr_info("amlvideo2 thread stop\n");
		return 0;
	}

	if (amlvideo2_dbg_en)
		pr_info("enter create black frame\n");
	if (node->vidq.task_running == 0) {
		pr_info("amlvideo2 no running\n");
		return 0;
	}

	if (node->frame_rate_black <= 60)
		usleep_range(node->frame_rate_black * 1000,
			(node->frame_rate_black + 1) * 1000);
	else
		usleep_range(60 * 1000, 61 * 1000);

	if (list_empty(&dma_q->head)) {
		if (amlvideo2_dbg_en & 2)
			pr_info("No active queue to serve .\n");
		goto unlock;
	}

	buf = list_first_entry(&dma_q->head,
			 struct amlvideo2_node_buffer,
			 list);
	if (amlvideo2_dbg_en & 2)
		pr_info("ready videobuf2 to fill data .\n");

	if (node->frame_inittime == 1) {
		node->frameinv_adjust = 0;
		node->frameinv = 0;
		ktime_get_ts64(&node->thread_ts1);
		node->frame_inittime = 0;
	} else {
		ktime_get_ts64(&node->thread_ts2);
		diff = node->thread_ts2.tv_sec - node->thread_ts1.tv_sec;
		diff = diff * 1000000 + node->thread_ts2.tv_nsec / 1000 -
			node->thread_ts1.tv_nsec / 1000;
		node->frameinv += diff;
		memcpy(&node->thread_ts1, &node->thread_ts2,
			   sizeof(struct timespec64));
		active_duration = node->frameinv - node->frameinv_adjust;

		if (active_duration + 5000 <= (int)fh->frm_save_time_us)
			goto unlock;
	}
	while (active_duration >= (int)fh->frm_save_time_us) {
		if (amlvideo2_dbg_en & 2)
			pr_info("while 4.\n");
		active_duration -= fh->frm_save_time_us;
	}

	if ((active_duration + 5000) > fh->frm_save_time_us)
		node->frameinv_adjust = fh->frm_save_time_us - active_duration;
	else
		node->frameinv_adjust = -active_duration;
	node->frameinv = 0;

	if (amlvideo2_dbg_en & 2)
		pr_info("amlvideo2 fillbuff start .\n");

	if (amlvideo2_fill_black(fh, buf) < 0)
		pr_info("amlvideo2 fill buff error!\n");

	if (amlvideo2_dbg_en & 2)
		pr_info("amlvideo2 fillbuff end .\n");

	buf->vb2.vb2_buf.timestamp = node->thread_ts2.tv_sec * 1000000
		+ node->thread_ts2.tv_nsec / 1000;

	if (amlvideo2_dbg_en & 2)
		pr_info("spin_lock_irqsave1.\n");

	spin_lock_irqsave(&dma_q->buff_list_lock, flags);
	buf->vb2.timecode.flags = 0;
	if (buf->vb2.vb2_buf.state == VB2_BUF_STATE_ACTIVE) {
		//private:bit0 is 1 means it is amlvideo2 create black frame
		buf->vb2.timecode.flags |= (1 << 0);
		vb2_buffer_done(&buf->vb2.vb2_buf, VB2_BUF_STATE_DONE);
		list_del(&buf->list);
	}
	spin_unlock_irqrestore(&dma_q->buff_list_lock, flags);
	if (amlvideo2_dbg_en & 2)
		pr_info("finish spin_lock_irqsave1.\n");

	if (node->pflag) {
		complete(&node->plug_sema);
		return 0;
	}

	if (amlvideo2_dbg_en & 2)
		pr_info("filled buffer %p\n", buf);

	return 0;

unlock:
	if (node->pflag)
		complete(&node->plug_sema);
	return 0;
}

static void amlvideo2_sleep(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node *node = fh->node;
	/* struct amlvideo2_node_dmaqueue *dma_q = &node->vidq; */

	if (is_black_frame[node->vid] && fh->is_streamed_on && node->screencap_type == 0) {
		if (amlvideo2_thread_tick_black(fh) < 0)
			schedule_timeout_interruptible(1);
	} else {
		if (amlvideo2_thread_tick(fh) < 0)
			schedule_timeout_interruptible(1);
	}

	/* stop_task: */
	/* remove_wait_queue(&dma_q->wq, &wait); */
	/*try_to_freeze();*/
}

static int stop_vdin1_service(struct amlvideo2_node *node)
{
	struct amlvideo2_fh *fh = node->fh;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	struct vdin_v4l2_ops_s *vops = &node->vops;
#endif
	int ret = -1;

	mutex_lock(&fh->mutex);
	if (node->start_vdin_flag ||
		node->r_type == AML_RECEIVER_NONE) {
		if (amlvideo2_dbg_en)
			pr_info("%s: stop tvin service.\n", __func__);
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		if (IS_ERR_OR_NULL(vops)) {
			mutex_unlock(&fh->mutex);
			pr_info("%s amlvideo2 vdin ops is NULL\n", __func__);
			return 0;
		}
		if (IS_ERR_OR_NULL(vops->stop_tvin_service)) {
			mutex_unlock(&fh->mutex);
			pr_info("%s amlvideo2 vdin stop_tvin_service is NULL\n", __func__);
			return 0;
		}

		if (node->aml2_dev->node_id == 1 && node->vdin1_is_start) {
			ret = vops->stop_tvin_service(node->vdin_device_num);
			if (ret < 0) {
				mutex_unlock(&fh->mutex);
				pr_err("%s amlvideo2 vdin stop failed\n", __func__);
				return 0;
			}
			node->vdin1_is_start = false;
		}
#endif
	}

	mutex_unlock(&fh->mutex);
	return ret;
}

static int start_send_normal_frame(struct amlvideo2_fh *fh);
static int amlvideo2_thread(void *data)
{
	struct amlvideo2_fh *fh = data;
	struct amlvideo2_node *node = fh->node;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	bool video_enable;
	int ret = 0;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);

	if (amlvideo2_dbg_en) {
		if (node->vid == 0)
			pr_info("start amlvideo2.0 thread.\n");
		else
			pr_info("start amlvideo2.1 thread.\n");
	}
	if (amlvideo2_dbg_en & 0x10)
		pr_info("thread started\n");

	/*set_freezable();*/

	while (1) {
#ifdef CONFIG_PM
		if (atomic_read(&node->is_suspend) && node->vidq.task_running == 1)
			wait_for_completion(&node->thread_sema);
#endif
		if (kthread_should_stop()) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 kthread stop 1.\n");
				else
					pr_info("node1 kthread stop 1.\n");
			}
			break;
		}

	#ifdef USE_SEMA_QBUF
		ret = wake_up_interruptible(&node->vidq.qbuf_comp);
	#endif
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("node0 task_running = %d\n",
					node->vidq.task_running);
			else
				pr_info("node1 task_running = %d\n",
					node->vidq.task_running);
		}
		if (!node->vidq.task_running) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 here break.\n");
				else
					pr_info("node1 here break.\n");
			}
			break;
		}

		if ((get_video_enabled(0) &&
			(node->porttype == TVIN_PORT_VIU1_VIDEO ||
			node->porttype == TVIN_PORT_VIU1_WB0_VD1)) ||
			(get_video_enabled(1) &&
			node->porttype == TVIN_PORT_VIU2_VD1))
			video_enable = true;

		if (node->vid == 1) {
			if (video_enable && is_black_frame[1] && node->screencap_type == 0) {
				pr_info("amlvideo2.1:video already ok\n");
				is_black_frame[1] = false;
				ret = start_send_normal_frame(fh);
				if (ret < 0)
					pr_err("start normal frame err.\n");
				node->frame_inittime = 1;
			}
			if (!video_enable && !is_black_frame[1] && node->screencap_type == 0) {
				is_black_frame[1] = true;
				stop_vdin1_service(node);
				pr_info("amlvideo2.1:send from normal to black.\n");
				node->frame_inittime = 1;
				ktime_get_ts64(&node->thread_ts1);
#ifdef TEST_LATENCY
				node->latency_info.cur_time = node->thread_ts1.tv_sec;
				node->latency_info.cur_time_out = node->thread_ts1.tv_sec;
				node->latency_info.total_latency = 0;
				node->latency_info.total_latency_out = 0;
#endif
			}
		} else if (node->vid == 0) {
			if (node->is_reg && is_black_frame[0]) {
				pr_info("amlvideo2.0:video already ok\n");
				is_black_frame[0] = false;
				node->frame_inittime = 1;
			}
			if (!node->is_reg && !is_black_frame[0]) {
				is_black_frame[0] = true;
				pr_info("amlvideo2.0:send from normal to black.\n");
				node->frame_inittime = 1;
				ktime_get_ts64(&node->thread_ts1);
#ifdef TEST_LATENCY
				node->latency_info.cur_time = node->thread_ts1.tv_sec;
				node->latency_info.cur_time_out = node->thread_ts1.tv_sec;
				node->latency_info.total_latency = 0;
				node->latency_info.total_latency_out = 0;
#endif
			}
		}

		amlvideo2_sleep(fh);
		if (kthread_should_stop()) {
			if (amlvideo2_dbg_en & 2) {
				if (node->vid == 0)
					pr_info("node0 kthread stop 2.\n");
				else
					pr_info("node1 kthread stop 2.\n");
			}
			break;
		}
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("amlvideo2.0_thread while 1 .\n");
			else
				pr_info("amlvideo2.1_thread while 1 .\n");
		}
	}
	while (!kthread_should_stop()) {
		if (amlvideo2_dbg_en & 2) {
			if (node->vid == 0)
				pr_info("amlvideo2.0_thread while 2 .\n");
			else
				pr_info("amlvideo2.1_thread while 2 .\n");
		}
		usleep_range(9000, 10000);
	}
		/*msleep(10);*/

	if (node->tmp_vf) {
		if (node->recv.name &&
		    (!vf_get_receiver(node->recv.name))) {
			vf_put(node->tmp_vf, node->recv.name);
			amlvideo2_total_put_count++;
		}
		node->tmp_vf = NULL;
	}

	if (amlvideo2_dbg_en) {
		if (node->vid == 0)
			pr_info("amlvideo2.0 thread exit.\n");
		else
			pr_info("amlvideo2.1 thread exit.\n");
	}
	if (amlvideo2_dbg_en & 0x10)
		pr_info("thread: exit\n");
	return ret;
}

static int amlvideo2_start_thread(struct amlvideo2_fh *fh)
{
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#ifdef USE_SEMA_QBUF
	init_completion(&dma_q->qbuf_comp);
	#endif

	if (amlvideo2_dbg_en & 1)
		pr_info("begin %s\n", __func__);
	mutex_lock(&node->mutex);
	if (dma_q->task_running) {
		mutex_unlock(&node->mutex);
		return 0;
	}

	fh->src_width = 0;
	fh->src_height = 0;
	node->tmp_vf = NULL;
	dma_q->task_running = 1;

	node->context = create_ge2d_work_queue();
	if (!node->context) {
		mutex_unlock(&node->mutex);
		pr_info("amlvideo2 create_ge2d_work_queue error!\n");
		return -1;
	}
	#ifdef MULTI_NODE
	dma_q->kthread =
		kthread_run(amlvideo2_thread, fh,
			    (node->vid == 0) ? "amlvideo2.0" : "amlvideo2.1");
	#else
	dma_q->kthread = kthread_run(amlvideo2_thread, fh, "amlvideo2.0");
	#endif

	if (IS_ERR(dma_q->kthread)) {
		v4l2_err(&node->aml2_dev->v4l2_dev, "kernel_thread() failed\n");
		dma_q->task_running = 0;
		dma_q->kthread = NULL;
		mutex_unlock(&node->mutex);
		pr_info("start thread error.....\n");
		return PTR_ERR(dma_q->kthread);
	}
	mutex_unlock(&node->mutex);
	if (amlvideo2_dbg_en & 1)
		pr_info("success create amlvideo2 thread .\n");
	/* Wakes thread */
	/* wake_up_interruptible(&dma_q->wq); */

	if (amlvideo2_dbg_en & 0x10)
		pr_info("returning from %s\n", __func__);
	return 0;
}

static void amlvideo2_stop_thread(struct amlvideo2_node_dmaqueue *dma_q)
{
	int ret = 0;
	struct amlvideo2_node *node;

	if (IS_ERR_OR_NULL(dma_q)) {
		pr_info("dma_q is NULL\n");
		return;
	}
	node = container_of(dma_q, struct amlvideo2_node, vidq);

	if (amlvideo2_dbg_en & 1) {
		if (node->vid == 0)
			pr_info("begin to stop amlvideo2.0 thread\n");
		else
			pr_info("begin to stop amlvideo2.1 thread\n");
	}
	mutex_lock(&node->mutex);
	/* shutdown control thread */
	if (!IS_ERR_OR_NULL(dma_q->kthread)) {
		dma_q->task_running = 0;
		send_sig(SIGTERM, dma_q->kthread, 1);
	#ifdef USE_SEMA_QBUF
		complete(&dma_q->qbuf_comp);
	#endif
		complete(&node->plug_sema);
		wake_up_interruptible(&dma_q->wq);
		if (amlvideo2_dbg_en & 1) {
			if (node->vid == 0)
				pr_info("ready to stop amlvideo2.0 thread\n");
			else
				pr_info("ready to stop amlvideo2.1 thread\n");
		}
		complete(&node->thread_sema);
		ret = kthread_stop(dma_q->kthread);
		if (ret < 0)
			pr_info("%s, ret = %d .\n", __func__, ret);

		dma_q->kthread = NULL;
	}
	if (node->context) {
		destroy_ge2d_work_queue(node->context);
		node->context = NULL;
	}
	mutex_unlock(&node->mutex);
	if (amlvideo2_dbg_en & 1) {
		if (node->vid == 0)
			pr_info("finish stop amlvideo2.0 thread\n");
		else
			pr_info("finish stop amlvideo2.1 thread\n");
	}
}

enum aml_provider_type_e get_provider_type(const char *name)
{
	enum aml_provider_type_e type = AML_PROVIDE_NONE;

	if (!name)
		return type;
	if (strncasecmp(name, "vdin0", 5) == 0)
		type = AML_PROVIDE_VDIN0;
	else if (strncasecmp(name, "vdin1", 5) == 0)
		type = AML_PROVIDE_VDIN1;
	else if (strncasecmp(name, "decoder", 7) == 0)
		type = AML_PROVIDE_DECODE;
	else if (strncasecmp(name, "ppmgr", 5) == 0)
		type = AML_PROVIDE_PPMGR;
	else
		type = AML_PROVIDE_MAX;
	return type;
}

enum aml_receiver_type_e get_sub_receiver_type(const char *name)
{
	enum aml_receiver_type_e type = AML_RECEIVER_NONE;

	if (!name)
		return type;
	if (strncasecmp(name, "ppmgr", 5) == 0) {
		type = AML_RECEIVER_PPMGR;
		if (amlvideo2_dbg_en)
			pr_info("type is ppmgr");
	} else if (strncasecmp(name, "deinterlace", 11) == 0) {
		type = AML_RECEIVER_DI;
		if (amlvideo2_dbg_en)
			pr_info("type is deinterlace");
	} else {
		type = AML_RECEIVER_MAX;
		if (amlvideo2_dbg_en)
			pr_info("type is not certain\n");
	}
	return type;
}

/* ------------------------------------------------------------------
 *           provider operations
 * ------------------------------------------------------------------
 */
static struct vframe_s *amlvideo2_vf_peek(void *op_arg)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	if (node->video_blocking)
		return NULL;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf peek .\n");

	if (node->vid == 0 &&
		strcmp(node->sub_recv->name, "video_render.0") == 0) {
		node->provide_ready = 1;
		if (amlvideo2_dbg_en & 8)
			pr_info("provider : node->recv.name = %s\n",
				node->recv.name);
		if (vf_peek(node->recv.name))
			wake_up_interruptible(&node->vidq.wq);
	}

	return vfq_peek(&node->q_ready);
}

static struct vframe_s *amlvideo2_vf_get(void *op_arg)
{
	struct vframe_s *vf = NULL;
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	if (node->video_blocking)
		return NULL;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf get .\n");

	vf = vfq_pop(&node->q_ready);

	return vf;
}

static void amlvideo2_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;
	char *name = (node->vid == 0) ? DEVICE_NAME0 : DEVICE_NAME1;

	if (node->video_blocking)
		return;
	if (amlvideo2_dbg_en & 8)
		pr_info("amlvideo2 vf put .\n");
	vf_put(vf, name);
	amlvideo2_total_put_count++;
}

static int amlvideo2_event_cb(int type, void *data, void *private_data)
{
	if (type & VFRAME_EVENT_RECEIVER_PUT) {
		/* pr_err("video put, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_GET) {
		/* pr_err("video get, avail=%d\n", vfq_level(&q_ready) ); */
	} else if (type & VFRAME_EVENT_RECEIVER_FRAME_WAIT) {
		/* up(&thread_sem); */
		pr_err("receiver is waiting\n");
	}
	return 0;
}

static int amlvideo2_vf_states(struct vframe_states *states, void *op_arg)
{
	/* unsigned long flags; */
	/* spin_lock_irqsave(&lock, flags); */
	struct amlvideo2_node *node = (struct amlvideo2_node *)op_arg;

	states->vf_pool_size = node->amlvideo2_pool_size;
	states->buf_recycle_num = 0;
	states->buf_free_num = node->amlvideo2_pool_size -
		vfq_level(&node->q_ready);
	states->buf_avail_num = vfq_level(&node->q_ready);
	/* spin_unlock_irqrestore(&lock, flags); */
	return 0;
}

static const struct vframe_operations_s amlvideo2_vf_provider = {
.peek = amlvideo2_vf_peek,
.get = amlvideo2_vf_get,
.put = amlvideo2_vf_put,
.event_cb = amlvideo2_event_cb,
.vf_states = amlvideo2_vf_states, };

/* ------------------------------------------------------------------
 * IOCTL vidioc handling
 * ------------------------------------------------------------------
 */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;

	strcpy(cap->driver, "amlvideo2");
	strcpy(cap->card, "amlvideo2");
	strscpy(cap->bus_info,
		node->aml2_dev->v4l2_dev.name,
		sizeof(cap->bus_info));
	cap->version = AMLVIDEO2_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps |
			V4L2_CAP_DEVICE_CAPS;
	return 0;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct amlvideo2_fmt *fmt;

	if (f->index >= ARRAY_SIZE(formats))
		return -EINVAL;

	fmt = &formats[f->index];

	strscpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
}

static inline u32 get_bytesperline(struct amlvideo2_fmt *fmt, u32 width)
{
	switch (fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		return width;
	default:
		return (width * fmt->depth) >> 3;
	}
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);

	if (fh->set_format_flag) {
		f->fmt.pix.width = fh->width;
		f->fmt.pix.height = fh->height;
		f->fmt.pix.field = fh->field;
		f->fmt.pix.pixelformat = fh->fmt->fourcc;
		f->fmt.pix.bytesperline = get_bytesperline(fh->fmt, fh->buf_width);
		f->fmt.pix.sizeimage =
			(fh->buf_height * fh->buf_width * fh->fmt->depth) >> 3;
	} else {
		if (fh->node->start_vdin_flag && fh->node->provide_ready)  {
			const struct vinfo_s *vinfo;
			vinfo = amlvideo2_get_vinfo(fh->node);
			f->fmt.pix.width = vinfo->width;
			f->fmt.pix.height = vinfo->height;
		}  else if (fh->node->provide_ready)  {
			f->fmt.pix.width = fh->src_width;
			f->fmt.pix.height = fh->src_height;
		} else {
			f->fmt.pix.width = 0;
			f->fmt.pix.height = 0;
		}
	}
	return 0;
}

static int vidioc_try_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_fmt *fmt = NULL;
	enum v4l2_field field;
	unsigned int maxw, maxh;
	int support_4k = 0;

	fmt = get_format(f);
	if (!fmt) {
		pr_err("Fourcc format (0x%08x) invalid.\n",
			f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	field = f->fmt.pix.field;

	if (field == V4L2_FIELD_ANY) {
		field = V4L2_FIELD_INTERLACED;
	} else if (field != V4L2_FIELD_INTERLACED) {
		pr_err("Field type invalid.\n");
		return -EINVAL;
	}

	support_4k = node->aml2_dev->support_4k_capture;
	if (amlvideo2_dbg_en)
		pr_info("%s support_4k=%d\n", __func__, support_4k);
	if (support_4k) {
		maxh = norm_maxh_4k();
		maxw = norm_maxw_4k();
	} else {
		maxh = norm_maxh();
		maxw = norm_maxw();
	}

	f->fmt.pix.field = field;
	v4l_bound_align_image(&fh->buf_width, 16,
			      maxw, 2, &fh->buf_height, 16,
			      maxh, 0, 0);
	f->fmt.pix.bytesperline = get_bytesperline(fmt, fh->buf_width);
	f->fmt.pix.sizeimage =
		(fh->buf_height * fh->buf_width * fmt->depth) >> 3;
	if (amlvideo2_dbg_en) {
		pr_info("buf_width:%d buf_height:%d depth:%d\n",
			fh->buf_width, fh->buf_height, fmt->depth);
		pr_info("bytesperline:%d sizeimage:%d\n",
			f->fmt.pix.bytesperline, f->fmt.pix.sizeimage);
	}
	return 0;
}

/*FIXME: This seems to be generic enough to be at videodev2 */
static int vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				struct v4l2_format *f)
{
	int ret = 0;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct vb2_queue *vb2_q = &fh->vb2_q;
	if (amlvideo2_dbg_en)
		pr_info("amlvideo2 s_fmt_1 %d * %d\n",
			f->fmt.pix.width, f->fmt.pix.height);

	fh->buf_width = (f->fmt.pix.width + (CANVAS_WIDTH_ALIGN - 1)) &
				(~(CANVAS_WIDTH_ALIGN - 1));
	fh->buf_height = f->fmt.pix.height;
	if (f->fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420 ||
	    f->fmt.pix.pixelformat == V4L2_PIX_FMT_YUV420) {
		fh->buf_width =
			(f->fmt.pix.width + (CANVAS_WIDTH_ALIGN * 2 - 1)) &
			(~(CANVAS_WIDTH_ALIGN * 2 - 1));
	}

	if (f->fmt.pix.quantization == V4L2_QUANTIZATION_LIM_RANGE)
		fh->quantization_range = QUANTIZATION_LIMITED;
	else if (f->fmt.pix.quantization == V4L2_QUANTIZATION_FULL_RANGE)
		fh->quantization_range = QUANTIZATION_FULL;
	else
		fh->quantization_range = QUANTIZATION_DEFAULT;

	ret = vidioc_try_fmt_vid_cap(file, fh, f);
	if (ret < 0)
		return ret;

	mutex_lock(vb2_q->lock);

	if (vb2_is_busy(&fh->vb2_q)) {
		pr_err("%s queue busy\n", __func__);
		ret = -EBUSY;
		goto out;
	}
	if (amlvideo2_dbg_en)
		pr_info("amlvideo2 s_fmt_2 %d * %d\n",
			fh->buf_width, fh->buf_height);

	fh->fmt = get_format(f);
	fh->width = f->fmt.pix.width;
	fh->height = f->fmt.pix.height;
	fh->field = f->fmt.pix.field;
	fh->type = f->type;
	fh->set_format_flag = true;
	fh->bytesperline = f->fmt.pix.bytesperline;
	fh->sizeimage = f->fmt.pix.sizeimage;
	ret = 0;
out: mutex_unlock(vb2_q->lock);
	return ret;
}

/*
 * Implement G/S_PARM.  There is a "high quality" mode we could try
 * to do someday; for now, we just do the frame rate tweak.
 * V4L2_CAP_TIMEPERFRAME need to be supported furthermore.
 */
static int vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct v4l2_captureparm *cp = &parms->parm.capture;

	pr_err("%s\n", __func__);
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));
	cp->capability = V4L2_CAP_TIMEPERFRAME;

	cp->timeperframe = amlvideo2_frmintervals_active;
	pr_err("g_parm,deno=%d, numerator=%d\n", cp->timeperframe.denominator,
	       cp->timeperframe.numerator);

	return 0;
}

static int vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	suseconds_t def_ival = 1000000 / DEF_FRAMERATE;
	suseconds_t ival; /* us */

	/*save the frame period. */
	if (parms->parm.capture.timeperframe.denominator == 0) {
		fh->frm_save_time_us = def_ival;
		amlvideo2_frmintervals_active.numerator = 1;
		amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
		return -EINVAL;
	}

	ival = parms->parm.capture.timeperframe.numerator * 1000000
		/ parms->parm.capture.timeperframe.denominator;

	fh->frm_save_time_us = ival;
	amlvideo2_frmintervals_active = parms->parm.capture.timeperframe;
	node->frame_rate_black = 1000 * amlvideo2_frmintervals_active.numerator /
		amlvideo2_frmintervals_active.denominator;

	if (amlvideo2_dbg_en & 0x10) {
		pr_info("%s,%d,type=%d\n", __func__, __LINE__, parms->type);
		pr_info("setting framerate:%d/%d(fps),rate=%d\n",
			amlvideo2_frmintervals_active.denominator,
			amlvideo2_frmintervals_active.numerator,
			node->frame_rate_black);
	}
	return 0;
}

static int vidioc_reqbufs(struct file *file, void *priv,
			  struct v4l2_requestbuffers *reqbufs)
{
	int ret = 0;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_device *aml2_dev = node->aml2_dev;
#ifdef USE_SEMA_QBUF
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
#endif

#ifdef USE_SEMA_QBUF
	init_completion(&dma_q->qbuf_comp);
#endif
	if (reqbufs->memory != V4L2_MEMORY_DMABUF &&
		reqbufs->memory != V4L2_MEMORY_MMAP) {
		pr_err("%s err,memory=%d,only support DMABUF and MMAP\n",
			__func__, reqbufs->memory);
		return -EINVAL;
	}
	if (reqbufs->count == 1)
		aml2_dev->is_one_buffer = true;
	else
		aml2_dev->is_one_buffer = false;

	if (fh->sizeimage * reqbufs->count > aml2_dev->framebuffer_total_size * 1024 * 1024) {
		pr_err("%s:no enough mem, amlvideo2 don't allow capture,sizeimage=%u, count=%d\n",
			__func__, fh->sizeimage, reqbufs->count);
		return -EINVAL;
	}
	if (amlvideo2_dbg_en)
		pr_info("%s memory:%d,type:%d buff_num:%d\n", __func__,
			reqbufs->memory, reqbufs->type, reqbufs->count);

	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);
	if (ret < 0)
		pr_err("vb2_ioctl_reqbufs fail\n");
	if (amlvideo2_dbg_en)
		pr_info("%s reqbufs end\n", __func__);
	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	int ret = vb2_ioctl_querybuf(file, priv, p);

	/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON8 */
	if (ret == 0) {
		struct amlvideo2_output output;

		memset(&output, 0, sizeof(struct amlvideo2_output));

		output.v4l2_format = fh->fmt->fourcc;
		output.vbuf = NULL;
		output.width = fh->width;
		output.height = fh->height;
		output.canvas_id = -1;
		p->reserved =
			convert_canvas_index(&output, fh->node->vid, p->index);
		if (p->reserved == (u32)-1)
			pr_info("amlvideo2 canvas alloc failed, %d\n", __LINE__);
	} else {
		p->reserved = 0;
	}
	if (amlvideo2_dbg_en)
		pr_info("%s querybuf\n", __func__);
	return ret;
}

static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	#ifdef USE_SEMA_QBUF
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	#endif

	int ret = vb2_ioctl_qbuf(file, priv, p);
	#ifdef USE_SEMA_QBUF
	complete(&dma_q->qbuf_comp);
	#endif

	if (amlvideo2_dbg_en)
		pr_info("%s: q buf, ret=%d\n", __func__, ret);
	return ret;
}

static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *p)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	int ret;

#ifdef CONFIG_PM
	if (atomic_read(&fh->node->is_suspend))
		return -ENOMEM;
#endif

	ret = vb2_ioctl_dqbuf(file, priv, p);
	if (ret == 0)
		amlvideo2_secure_aipq_flag = 0;

	if (amlvideo2_dbg_en)
		pr_info("%s: dq buf, ret=%d\n", __func__, ret);
	return ret;
}

#ifdef PREVIOUS_VOUT_MODE
static enum tvin_scan_mode_e vmode2scan_mode(enum vmode_e mode)
{
	enum tvin_scan_mode_e scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		//TVIN_SCAN_MODE_NULL;/* 1: progressive 2:interlaced */

	switch (mode) {
	case VMODE_480I:
	case VMODE_480CVBS:
	case VMODE_NTSC_M:
	case VMODE_576I:
	case VMODE_576CVBS:
	case VMODE_PAL_M:
	case VMODE_PAL_N:
	case VMODE_1080I:
	case VMODE_1080I_50HZ:
		scan_mode = TVIN_SCAN_MODE_INTERLACED;
		break;
	case VMODE_480P:
	case VMODE_576P:
	case VMODE_720P:
	case VMODE_1080P:
	case VMODE_720P_50HZ:
	case VMODE_1080P_50HZ:
	case VMODE_1080P_24HZ:
	case VMODE_4K2K_30HZ:
	case VMODE_4K2K_25HZ:
	case VMODE_4K2K_24HZ:
	case VMODE_4K2K_SMPTE:
	case VMODE_VGA:
	case VMODE_SVGA:
	case VMODE_XGA:
	case VMODE_SXGA:
	case VMODE_LCD:
	case VMODE_4K2K_50HZ:
	case VMODE_4K2K_50HZ_Y420:
	case VMODE_4K2K_60HZ:
	case VMODE_4K2K_60HZ_Y420:
		scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		break;
	default:
		pr_err("unknown mode=%d\n", mode);
		break;
	}
	/* pr_err("mode=%d, scan_mode=%d\n", mode, scan_mode); */

	return scan_mode;
}
#endif

/*the counter of AMLVIDEO2*/
#define AMLVIDEO2_MAX_NODE		2
static struct amlvideo2_node  *amlvideo2_node[AMLVIDEO2_MAX_NODE];
static int amlvideo2_stop_tvin_service(struct amlvideo2_node *node)
{
	int ret = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	struct vdin_v4l2_ops_s *vops = &node->vops;
#endif
	if (amlvideo2_dbg_en)
		pr_info("%s , %d\n", __func__, __LINE__);

	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_stop_thread(&node->vidq);

	if ((node->input == 0 || node->input == 0x1000C000) &&
	    node->r_type == AML_RECEIVER_NONE) {
		if (amlvideo2_dbg_en)
			pr_info("stop tvin service .\n");
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		if (!(vops->stop_tvin_service)) {
			pr_info("stop_tvin_service func is NULL.\n");
			return -1;
		}
		vops->stop_tvin_service(node->vdin_device_num);
#endif
	}

	return ret;
}

static int amlvideo2_start_tvin_service(struct amlvideo2_node *node)
{
		int ret;
		struct amlvideo2_fh *fh = node->fh;
		struct amlvideo2_device *aml2_dev = node->aml2_dev;
		struct video_input_info video_input_parms;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		struct vdin_v4l2_ops_s *vops = &node->vops;
		struct vdin_parm_s para;
		const struct vinfo_s *vinfo;
		int axis[4];

		if (node->r_type != AML_RECEIVER_NONE)
			goto start;

		vinfo = amlvideo2_get_vinfo(node);
#endif
		get_video_input_info(&video_input_parms); /*only video and point 7*/
		memset(&node->display_info, 0, sizeof(struct vdisplay_info_s));
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		if (!node->start_vdin_flag ||
			node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
			node->porttype == TVIN_PORT_VIU1_VIDEO ||
			node->porttype == TVIN_PORT_VIU2_VD1 ||
			node->porttype == TVIN_PORT_VIU1_WB0_OSD1 ||
			node->porttype == TVIN_PORT_VIU1_WB0_OSD2 ||
			node->porttype == TVIN_PORT_VIU2_OSD1 ||
			node->porttype == TVIN_PORT_VIU3_OSD1) {
			if (node->porttype == TVIN_PORT_VIU1_WB0_VD1)
				get_vdx_real_axis(0, axis);
			else if (node->porttype == TVIN_PORT_VIU2_VD1)
				get_vdx_real_axis(1, axis);

			if (node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
				node->porttype == TVIN_PORT_VIU2_VD1) {
				node->display_info.display_hsc_startp = axis[0];
				node->display_info.display_vsc_startp = axis[1];
				node->display_info.display_hsc_endp = axis[2];
				node->display_info.display_vsc_endp = axis[3];
			}

			node->has_amvideo_node = true;
			node->mode = AML_SCREEN_MODE_ADAPTIVE;
			/* AML_SCREEN_MODE_ADAPTIVE; */
			if (amlvideo2_dbg_en) {
				pr_info("restart screen:h_start = %d,h_end = %d\n",
					node->display_info.screen_vd_h_start_,
					node->display_info.screen_vd_h_end_);
				pr_info("screen:v_start = %d,v_end = %d\n",
					node->display_info.screen_vd_v_start_,
					node->display_info.screen_vd_v_end_);
				pr_info("display:hsc_startp=%d,hsc_endp=%d\n",
					node->display_info.display_hsc_startp,
					node->display_info.display_hsc_endp);
				pr_info("display:vsc_startp=%d,vsc_endp=%d\n",
					node->display_info.display_vsc_startp,
					node->display_info.display_vsc_endp);
				pr_info("frame:hd_start_lines=%d,hd_end_lines=%d\n",
				 node->display_info.frame_hd_start_lines_,
				 node->display_info.frame_hd_end_lines_);
				pr_info("frame:vd_start_lines=%d,vd_end_lines=%d\n",
				 node->display_info.frame_vd_start_lines_,
				 node->display_info.frame_vd_end_lines_);
			}
		}

		if (amlvideo2_dbg_en) {
			pr_info("node->porttype = %d\n", node->porttype);
			pr_info("crop_enable = %d, node->porttype = 0x%x.\n",
				node->crop_info.capture_crop_enable, node->porttype);
			pr_info("node->r_type=%d, node->p_type=%d\n",
				node->r_type, node->p_type);
			pr_info("vinfo->w=%d, vinfo->h=%d, vinfo->field_height=%d.\n",
				vinfo->width, vinfo->height, vinfo->field_height);
			pr_info("video_w=%d, video_h=%d.\n",
				video_input_parms.width, video_input_parms.height);
		}
		memset(&para, 0, sizeof(para));
		para.port = node->porttype;
		para.fmt = TVIN_SIG_FMT_MAX;
		para.frame_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
		if (para.port == TVIN_PORT_VIU1_WB0_VD1 ||
			para.port == TVIN_PORT_VIU2_VD1) {
			para.h_active = (node->display_info.display_hsc_endp -
			node->display_info.display_hsc_startp + 1);
			para.v_active = (node->display_info.display_vsc_endp -
			node->display_info.display_vsc_startp + 1);
		} else if (para.port == TVIN_PORT_VIU1_VIDEO) {
			/*only video point 7*/
			para.h_active = video_input_parms.width;
			para.v_active = video_input_parms.height;
		} else {
			para.h_active = vinfo->width;
			para.v_active = vinfo->height;
		}

		para.hsync_phase = 0;
		para.vsync_phase = 1;
		para.hs_bp = 0;
		para.vs_bp = 0;
		para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
#ifdef PREVIOUS_VOUT_MODE
		/* TVIN_SCAN_MODE_PROGRESSIVE; */
		para.scan_mode = vmode2scan_mode(vinfo->mode);
#else
		if (vinfo->height == vinfo->field_height)
			para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		else
			para.scan_mode = TVIN_SCAN_MODE_INTERLACED;
#endif
		if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
			para.v_active = para.v_active / 2;

		if (vinfo->width > vinfo->height && (node->qctl_regs[0] % 180)) {
			para.dest_h_active = fh->height;
			para.dest_v_active = fh->width;
		} else {
			para.dest_h_active = fh->width;
			para.dest_v_active = fh->height;
		}

		if (fh->width < VDIN1_SCALE_W || fh->height < VDIN1_SCALE_H) {
			para.dest_h_active = VDIN1_SCALE_W;
			para.dest_v_active = VDIN1_SCALE_H;
			if (amlvideo2_dbg_en)
				pr_info("need vdin scale, dst_w: %d, dst_h: %d\n",
					fh->width, fh->height);
		}

		para.crop[0] = node->crop_info.source_top_crop;
		para.crop[1] = node->crop_info.source_left_crop;
		para.crop[2] = node->crop_info.source_width_crop;
		para.crop[3] = node->crop_info.source_height_crop;
		if (para.crop[0] > para.v_active || para.crop[1] > para.h_active) {
			pr_info("crop err, modify top: %d, left: %d\n", para.crop[0], para.crop[1]);
			para.crop[0] = 0;
			para.crop[1] = 0;
		}
		if (para.crop[2] > para.h_active || para.crop[3] > para.v_active) {
			pr_info("crop err, modify width: %d, height: %d\n",
				para.crop[2], para.crop[3]);
			para.crop[2] = para.h_active;
			para.crop[3] = para.v_active;
		}
		if (para.crop[2] == 0 || para.crop[3] == 0) {
			para.crop[0] = 0;
			para.crop[1] = 0;
			para.crop[2] = para.h_active;
			para.crop[3] = para.v_active;
		}
		para.reserved |= PARAM_STATE_SCREEN_CAP;
		if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
			para.dest_v_active = para.dest_v_active / 2;
		if (para.port == TVIN_PORT_VIU1_VIDEO ||
			para.port == TVIN_PORT_VIU1_WB0_VD1 ||
			para.port == TVIN_PORT_VIU2_VD1 ||
			para.port == TVIN_PORT_VIU1_WB0_OSD1 ||
			para.port == TVIN_PORT_VIU1_WB0_OSD2 ||
			para.port == TVIN_PORT_VIU2_OSD1 ||
			para.port == TVIN_PORT_VIU3_OSD1) {
			if (node->ge2d_multi_process_flag) {
				para.dest_h_active = 384;
				para.dest_v_active = 216;
			} else {
				para.cfmt = 1;
			}
		}
		para.is_one_buffer = aml2_dev->is_one_buffer;
		if (fh->quantization_range == QUANTIZATION_LIMITED)
			para.quantization_range = 2;
		else if (fh->quantization_range == QUANTIZATION_FULL)
			para.quantization_range = 1;
		else
			para.quantization_range = 0;

		if (fh->secure_data.secure_capture_type ==
			V4L2_EXT_AMLVIDEO2_SECURE_TEE_ONE_BUFFER)
			para.support_secure = true;
		else
			para.support_secure = false;

		if (amlvideo2_dbg_en) {
			pr_info("para.h_active: %d, para.v_active: %d,",
				para.h_active, para.v_active);
			pr_info("para.dest_h_active: %d, para.dest_v_active: %d,",
				para.dest_h_active, para.dest_v_active);
			pr_info("fh->width: %d, fh->height: %d,",
				fh->width, fh->height);
			pr_info("vinfo->mode: %d,para.scan_mode: %d\n",
				vinfo->mode, para.scan_mode);
			pr_info("node->vdin_device_num = %d, is_one_buffer=%d.\n",
				node->vdin_device_num, para.is_one_buffer);
			pr_info("crop:top %d left %d width %d height %d\n",
				para.crop[0], para.crop[1], para.crop[2], para.crop[3]);
			pr_info("quantization_range %d, vdin_support_secure: %d,",
				para.quantization_range, para.support_secure);
		}
		if (IS_ERR_OR_NULL(vops)) {
			pr_info("%s amlvideo2 vdin ops is NULL\n", __func__);
			return 0;
		}
		if (node->vdin_port_ext < 0) {
			if (IS_ERR_OR_NULL(vops->start_tvin_service)) {
				pr_info("%s amlvideo2 vdin start_tvin_service is NULL\n",
					__func__);
				return 0;
			}
			ret = vops->start_tvin_service(node->vdin_device_num, &para);
		} else {
			if (IS_ERR_OR_NULL(vops->start_tvin_service_ex)) {
				pr_info("%s amlvideo2 vdin start_tvin_service_ex is NULL\n",
					__func__);
				return 0;
			}
			ret = vops->start_tvin_service_ex(node->vdin_device_num,
							  node->vdin_port_ext, &para);
		}
		if (ret < 0) {
			pr_info("%s amlvideo2 vdin start_tvin_service_ex fail\n",
				__func__);
			return 0;
		}
#endif

start: node->frame_inittime = 1;
	/* frameinv_adjust = 0; */
	/* frameinv = 0; */
	/* tmp_vf = NULL; */
	ktime_get_ts64(&node->thread_ts1);
	return 0;
}

int amlvideo2_notify_callback(struct notifier_block *block, unsigned long cmd,
			      void *para)
{
	struct amlvideo2_node  *node = NULL;
	struct amlvideo2_fh *fh = NULL;
	struct vframe_s *recycle_vf = NULL;
	struct vframe_states states;
	struct vframe_provider_s *vfp = NULL;
	int i;
	int index = 0;
	int ret = 0;
	int i_ret = 0;

	for (i = 0; i < AMLVIDEO2_MAX_NODE; i++) {
		if (amlvideo2_node[i] &&
		    amlvideo2_node[i]->r_type == AML_RECEIVER_NONE)
			index = 1;
	}

	if (amlvideo2_dbg_en)
		pr_info("%s , index = %d\n", __func__, index);

	if (index != 1) {
		if (amlvideo2_dbg_en)
			pr_info("index is not right, return\n");
		return ret;
	}

	node = amlvideo2_node[index];
	if (!node) {
		if (amlvideo2_dbg_en)
			pr_info("node is NULL, return\n");
		return ret;
	}
	if (node->vid != 1) {
		if (amlvideo2_dbg_en)
			pr_info("node is not right, return\n");
		return ret;
	}
	if (!(node->users)) {
		if (amlvideo2_dbg_en)
			pr_info("node user not work, return\n");
		return ret;
	}
	fh = node->fh;

	switch (cmd) {
	case  VOUT_EVENT_MODE_CHANGE:
		pr_info("mode changed in amlvideo2.\n");
		mutex_lock(&fh->mutex);
		if (!node->fh->is_streamed_on) {
			mutex_unlock(&fh->mutex);
			pr_info("driver not need to screencap.\n");
			return ret;
		}
		node->pflag = true;
		i_ret = wait_for_completion_timeout(&node->plug_sema,
						    msecs_to_jiffies(150));
		if (i_ret == 0) {
			mutex_unlock(&fh->mutex);
			return 0;
		}

		if (amlvideo2_dbg_en)
			pr_info("finish wait plug sema .\n");

		/* if local queue have vf , should give back to provider */
		if (vfq_empty(&node->q_ready)) {
			if (amlvideo2_dbg_en & 4)
				pr_info("q_ready is empty .\n");
		} else {
			recycle_vf = vfq_pop(&node->q_ready);
			while (recycle_vf) {
				vf_put(recycle_vf, node->recv.name);
				amlvideo2_total_put_count++;
				recycle_vf = vfq_pop(&node->q_ready);
			}
			if (amlvideo2_dbg_en & 4)
				pr_info("already flush local vf .\n");
		}

		/*debug provider vf state*/
		if (amlvideo2_dbg_en & 4) {
			ret = vf_get_states(vfp, &states);
			if (ret == 0) {
				pr_info("vf_pool_size = %d, buf_free_num = %d.\n",
				states.vf_pool_size, states.buf_free_num);
				pr_info("buf_recycle_num = %d, buf_avail_num = %d.\n",
				states.buf_recycle_num, states.buf_avail_num);
			}
		}

		if (node->vdin1_is_start) {
			//0x2 for judge vdin1 if already start
			if (get_vdin_status(0) & 0x2) {
				ret = amlvideo2_stop_tvin_service(node);
				if (ret < 0) {
					mutex_unlock(&fh->mutex);
					pr_err("stop tvin service failed.\n");
					node->pflag = false;
					return ret;
				}
			}

			if (node->r_type == AML_RECEIVER_NONE)
				amlvideo2_start_thread(node->fh);

			ret = amlvideo2_start_tvin_service(node);
			if (ret < 0) {
				mutex_unlock(&fh->mutex);
				pr_err("start tvin service failed.\n");
				node->pflag = false;
				return ret;
			}
		}
		node->pflag = false;
		mutex_unlock(&fh->mutex);
		break;

	default:
		pr_info("amlvideo2 not support vout cmd\n");
		break;
	}

	if (amlvideo2_dbg_en)
		pr_info("finish %s. ret = %d\n", __func__, ret);

	return ret;
}

static struct notifier_block amlvideo2_notifier_nb = {
	.notifier_call	= amlvideo2_notify_callback,
};

//V4L2_CID_EXT_CAPTURE_AMLVIDEO2_SET_SECURE_INFO
static int amlvideo2_vidioc_s_cid_secure_info(struct amlvideo2_fh *fh,
	struct v4l2_ext_control *ctrl)
{
	if (ctrl->size == sizeof(struct v4l2_ext_amlvideo2_set_secure_data)) {
		if (copy_from_user(&fh->secure_data, ctrl->ptr,
			sizeof(struct v4l2_ext_amlvideo2_set_secure_data)))
			return -EFAULT;
	} else {
		return -EINVAL;
	}

	return 0;
}

//V4L2_CID_EXT_CAPTURE_AMLVIDEO2_OUTPUT_INFO
static int amlvideo2_vidioc_g_cid_output_info(struct amlvideo2_fh *fh,
	struct v4l2_ext_control *ctrl)
{
	if (ctrl->size == sizeof(struct v4l2_ext_amlvideo2_output_data)) {
		if (copy_to_user(ctrl->ptr, &fh->ext_output_data,
			sizeof(struct v4l2_ext_amlvideo2_output_data)))
			return -EFAULT;
	} else {
		return -EINVAL;
	}

	return 0;
}

//V4L2_CID_EXT_CAPTURE_AMLVIDEO2_GET_VIDEO_INFO
static int amlvideo2_vidioc_g_cid_video_info(struct amlvideo2_fh *fh,
	struct v4l2_ext_control *ctrl)
{
	struct video_input_info video_input_parms;

	get_video_input_info(&video_input_parms); /*only video and point 7*/
	fh->video_data.input_w = video_input_parms.width;
	fh->video_data.input_h = video_input_parms.height;
	if (ctrl->size == sizeof(struct v4l2_ext_amlvideo2_get_video_data)) {
		if (copy_to_user(ctrl->ptr, &fh->video_data,
			sizeof(struct v4l2_ext_amlvideo2_get_video_data)))
			return -EFAULT;
	} else {
		return -EINVAL;
	}

	return 0;
}

//V4L2_CID_EXT_CAPTURE_AMLVIDEO2_GET_DISPLAY_INFO
static int amlvideo2_vidioc_g_cid_display_info(struct amlvideo2_fh *fh,
	struct v4l2_ext_control *ctrl)
{
	if (ctrl->size == sizeof(struct v4l2_ext_amlvideo2_get_vinfo_data)) {
		if (copy_to_user(ctrl->ptr, &fh->vinfo_data,
			sizeof(struct v4l2_ext_amlvideo2_get_vinfo_data)))
			return -EFAULT;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int vidioc_streamon(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	struct amlvideo2_device *aml2_dev = node->aml2_dev;
	struct video_input_info video_input_parms;
	const struct vinfo_s *vinfo;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		struct vdin_v4l2_ops_s *vops = &node->vops;
		struct vdin_parm_s para;
		int angle;
		int axis[4];

		if (node->qctl_regs[0] == 0)
			node->qctl_regs[0] = amlvideo2_angle;

		angle = node->qctl_regs[0];
#endif

	vinfo = amlvideo2_get_vinfo(node);
	fh->vinfo_data.width = vinfo->width;
	fh->vinfo_data.height = vinfo->height;

	if (!amlvideo2_secure_paddr)
		amlvideo2_secure_paddr = fh->secure_data.secure_phy_addr;
	if (!fh->secure_data.secure_buffer_size)
		fh->secure_data.secure_buffer_size = SECURE_BUFFER_SIZE_1M;
	if (aml2_dev->node_id == 0)
		node->mode = AML_SCREEN_MODE_FULL;
	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != fh->type)
		return -EINVAL;
	ret = vb2_ioctl_streamon(file, priv, i);
	if (ret < 0) {
		pr_err("%s, vb2_ioctl_streamon ret: %d\n", __func__, ret);
		return ret;
	}

	if ((!get_video_enabled(0) &&
		(node->porttype == TVIN_PORT_VIU1_VIDEO ||
		node->porttype == TVIN_PORT_VIU1_WB0_VD1)) ||
		(!get_video_enabled(1) &&
		node->porttype == TVIN_PORT_VIU2_VD1)) {
		pr_info("start screencap but no video\n");
		is_black_frame[node->vid] = true;
	} else {
		is_black_frame[node->vid] = false;
	}

	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_start_thread(fh);

	if (is_black_frame[node->vid])
		goto start;

	get_video_input_info(&video_input_parms); /*only video and point 7*/
	memset(&node->display_info, 0, sizeof(struct vdisplay_info_s));
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	if (!node->start_vdin_flag ||
	    node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
	    node->porttype == TVIN_PORT_VIU1_VIDEO ||
	    node->porttype == TVIN_PORT_VIU2_VD1 ||
	    node->porttype == TVIN_PORT_VIU1_WB0_OSD1 ||
	    node->porttype == TVIN_PORT_VIU1_WB0_OSD2 ||
	    node->porttype == TVIN_PORT_VIU2_OSD1 ||
	    node->porttype == TVIN_PORT_VIU3_OSD1) {
		if (node->porttype == TVIN_PORT_VIU1_WB0_VD1)
			get_vdx_real_axis(0, axis);
		else if (node->porttype == TVIN_PORT_VIU2_VD1)
			get_vdx_real_axis(1, axis);

		if (node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
			node->porttype == TVIN_PORT_VIU2_VD1) {
			node->display_info.display_hsc_startp = axis[0];
			node->display_info.display_vsc_startp = axis[1];
			node->display_info.display_hsc_endp = axis[2];
			node->display_info.display_vsc_endp = axis[3];
		}

		node->has_amvideo_node = true;
		node->mode = AML_SCREEN_MODE_ADAPTIVE;
		/* AML_SCREEN_MODE_ADAPTIVE; */
		if (amlvideo2_dbg_en) {
			pr_info("screen:h_start = %d,h_end = %d\n",
				node->display_info.screen_vd_h_start_,
				node->display_info.screen_vd_h_end_);
			pr_info("screen:v_start = %d,v_end = %d\n",
				node->display_info.screen_vd_v_start_,
				node->display_info.screen_vd_v_end_);
			pr_info("display:hsc_startp=%d,hsc_endp=%d\n",
				node->display_info.display_hsc_startp,
				node->display_info.display_hsc_endp);
			pr_info("display:vsc_startp=%d,vsc_endp=%d\n",
				node->display_info.display_vsc_startp,
				node->display_info.display_vsc_endp);
			pr_info("frame:hd_start_lines=%d,hd_end_lines=%d\n",
			 node->display_info.frame_hd_start_lines_,
			 node->display_info.frame_hd_end_lines_);
			pr_info("frame:vd_start_lines=%d,vd_end_lines=%d\n",
			 node->display_info.frame_vd_start_lines_,
			 node->display_info.frame_vd_end_lines_);
		}
	}

	if (amlvideo2_dbg_en) {
		pr_info("node->porttype = %d\n", node->porttype);
		pr_info("crop_enable = %d, node->porttype = 0x%x.\n",
			node->crop_info.capture_crop_enable, node->porttype);
		pr_info("node->r_type=%d, node->p_type=%d\n",
			node->r_type, node->p_type);
		pr_info("vinfo->w=%d, vinfo->h=%d, vinfo->field_height=%d.\n",
			vinfo->width, vinfo->height, vinfo->field_height);
		pr_info("video_w=%d, video_h=%d.\n",
			video_input_parms.width, video_input_parms.height);
	}
#endif

	if (aml2_dev->node_id == 0)
		node->mode = AML_SCREEN_MODE_FULL;

	if (!node->start_vdin_flag || node->r_type != AML_RECEIVER_NONE)
		goto start;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	memset(&para, 0, sizeof(para));
	para.port = node->porttype;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
	if (para.port == TVIN_PORT_VIU1_WB0_VD1 ||
	    para.port == TVIN_PORT_VIU2_VD1) {
		para.h_active = (node->display_info.display_hsc_endp -
		node->display_info.display_hsc_startp + 1);
		para.v_active = (node->display_info.display_vsc_endp -
		node->display_info.display_vsc_startp + 1);
	} else if (para.port == TVIN_PORT_VIU1_VIDEO) {
		/*only video point 7*/
		para.h_active = video_input_parms.width;
		para.v_active = video_input_parms.height;
	} else {
		para.h_active = vinfo->width;
		para.v_active = vinfo->height;
	}

	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 0;
	para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
#ifdef PREVIOUS_VOUT_MODE
	/* TVIN_SCAN_MODE_PROGRESSIVE; */
	para.scan_mode = vmode2scan_mode(vinfo->mode);
#else
	if (vinfo->height == vinfo->field_height)
		para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
	else
		para.scan_mode = TVIN_SCAN_MODE_INTERLACED;
#endif
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.v_active = para.v_active / 2;

	if (vinfo->width > vinfo->height && (node->qctl_regs[0] % 180)) {
		para.dest_h_active = fh->height;
		para.dest_v_active = fh->width;
	} else {
		para.dest_h_active = fh->width;
		para.dest_v_active = fh->height;
	}

	if (fh->width < VDIN1_SCALE_W || fh->height < VDIN1_SCALE_H) {
		para.dest_h_active = VDIN1_SCALE_W;
		para.dest_v_active = VDIN1_SCALE_H;
		if (amlvideo2_dbg_en)
			pr_info("need vdin scale, dst_w: %d, dst_h: %d,", fh->width, fh->height);
	}

	if (amlvideo2_dest_w != 0)
		para.dest_h_active = amlvideo2_dest_w;

	if (amlvideo2_dest_h != 0)
		para.dest_v_active = amlvideo2_dest_h;
	para.crop[0] = node->crop_info.source_top_crop;
	para.crop[1] = node->crop_info.source_left_crop;
	para.crop[2] = node->crop_info.source_width_crop;
	para.crop[3] = node->crop_info.source_height_crop;
	if (para.crop[0] > para.v_active || para.crop[1] > para.h_active) {
		pr_info("crop err, modify top: %d, left: %d\n", para.crop[0], para.crop[1]);
		para.crop[0] = 0;
		para.crop[1] = 0;
	}
	if (para.crop[2] > para.h_active || para.crop[3] > para.v_active) {
		pr_info("crop err, modify width: %d, height: %d\n", para.crop[2], para.crop[3]);
		para.crop[2] = para.h_active;
		para.crop[3] = para.v_active;
	}
	if (para.crop[2] == 0 || para.crop[3] == 0) {
		para.crop[0] = 0;
		para.crop[1] = 0;
		para.crop[2] = para.h_active;
		para.crop[3] = para.v_active;
	}
	para.reserved |= PARAM_STATE_SCREEN_CAP;
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.dest_v_active = para.dest_v_active / 2;
	if (para.port == TVIN_PORT_VIU1_VIDEO ||
	    para.port == TVIN_PORT_VIU1_WB0_VD1 ||
	    para.port == TVIN_PORT_VIU2_VD1 ||
	    para.port == TVIN_PORT_VIU1_WB0_OSD1 ||
	    para.port == TVIN_PORT_VIU1_WB0_OSD2 ||
	    para.port == TVIN_PORT_VIU2_OSD1 ||
	    para.port == TVIN_PORT_VIU3_OSD1) {
		if (node->ge2d_multi_process_flag) {
			para.dest_h_active = 384;
			para.dest_v_active = 216;
		} else {
			para.cfmt = 1;
		}
	}
	para.is_one_buffer = aml2_dev->is_one_buffer;
	if (fh->quantization_range == QUANTIZATION_LIMITED)
		para.quantization_range = 2;
	else if (fh->quantization_range == QUANTIZATION_FULL)
		para.quantization_range = 1;
	else
		para.quantization_range = 0;

	if (fh->secure_data.secure_capture_type ==
		V4L2_EXT_AMLVIDEO2_SECURE_TEE_ONE_BUFFER)
		para.support_secure = true;
	else
		para.support_secure = false;
	if (amlvideo2_dbg_en) {
		pr_info("para.h_active: %d, para.v_active: %d,",
			para.h_active, para.v_active);
		pr_info("para.dest_h_active: %d, para.dest_v_active: %d,",
			para.dest_h_active, para.dest_v_active);
		pr_info("fh->width: %d, fh->height: %d,",
			fh->width, fh->height);
		pr_info("vinfo->mode: %d,para.scan_mode: %d, node->mode=%d\n",
			vinfo->mode, para.scan_mode, node->mode);
		pr_info("node->vdin_device_num = %d, is_one_buffer=%d.\n",
			node->vdin_device_num, para.is_one_buffer);
		pr_info("crop:top %d left %d width %d height %d\n",
			para.crop[0], para.crop[1], para.crop[2], para.crop[3]);
		pr_info("quantization_range %d, vdin_support_secure: %d,",
			para.quantization_range, para.support_secure);
	}
	if (IS_ERR_OR_NULL(vops)) {
		if (node->r_type == AML_RECEIVER_NONE)
			amlvideo2_stop_thread(dma_q);
		pr_info("%s amlvideo2 vdin ops is NULL\n", __func__);
		return -EBUSY;
	}
	if (node->vdin_port_ext < 0) {
		if (IS_ERR_OR_NULL(vops->start_tvin_service)) {
			if (node->r_type == AML_RECEIVER_NONE)
				amlvideo2_stop_thread(dma_q);
			pr_info("%s amlvideo2 vdin start_tvin_service is NULL\n",
				__func__);
			return -EBUSY;
		}
		ret = vops->start_tvin_service(node->vdin_device_num, &para);
	} else {
		if (IS_ERR_OR_NULL(vops->start_tvin_service_ex)) {
			if (node->r_type == AML_RECEIVER_NONE)
				amlvideo2_stop_thread(dma_q);
			pr_info("%s amlvideo2 vdin start_tvin_service_ex is NULL\n",
				__func__);
			return -EBUSY;
		}
		ret = vops->start_tvin_service_ex(node->vdin_device_num,
						  node->vdin_port_ext, &para);
	}
	if (ret < 0) {
		if (node->r_type == AML_RECEIVER_NONE)
			amlvideo2_stop_thread(dma_q);
		pr_info("%s amlvideo2 start vdin fail\n",
			__func__);
		return -EBUSY;
	}
	node->vdin1_is_start = true;
#endif
start: node->frame_inittime = 1;
	fh->is_streamed_on = 1;
	/* frameinv_adjust = 0; */
	/* frameinv = 0; */
	/* tmp_vf = NULL; */
	ktime_get_ts64(&node->thread_ts1);
#ifdef TEST_LATENCY
	node->latency_info.cur_time = node->thread_ts1.tv_sec;
	node->latency_info.cur_time_out = node->thread_ts1.tv_sec;
	node->latency_info.total_latency = 0;
	node->latency_info.total_latency_out = 0;
#endif
	return 0;
}

static int start_send_normal_frame(struct amlvideo2_fh *fh)
{
	int ret;
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_device *aml2_dev = node->aml2_dev;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	struct video_input_info video_input_parms;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	struct vdin_v4l2_ops_s *vops = &node->vops;
	struct vdin_parm_s para;
	const struct vinfo_s *vinfo;
	int axis[4];

	if (node->qctl_regs[0] == 0)
		node->qctl_regs[0] = amlvideo2_angle;

	vinfo = amlvideo2_get_vinfo(node);
#endif

	get_video_input_info(&video_input_parms); /*only video and point 7*/
	memset(&node->display_info, 0, sizeof(struct vdisplay_info_s));
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	if (!node->start_vdin_flag ||
	    node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
	    node->porttype == TVIN_PORT_VIU1_VIDEO ||
	    node->porttype == TVIN_PORT_VIU2_VD1 ||
	    node->porttype == TVIN_PORT_VIU1_WB0_OSD1 ||
	    node->porttype == TVIN_PORT_VIU1_WB0_OSD2 ||
	    node->porttype == TVIN_PORT_VIU2_OSD1 ||
	    node->porttype == TVIN_PORT_VIU3_OSD1) {
		if (node->porttype == TVIN_PORT_VIU1_WB0_VD1)
			get_vdx_real_axis(0, axis);
		else if (node->porttype == TVIN_PORT_VIU2_VD1)
			get_vdx_real_axis(1, axis);

		if (node->porttype == TVIN_PORT_VIU1_WB0_VD1 ||
			node->porttype == TVIN_PORT_VIU2_VD1) {
			node->display_info.display_hsc_startp = axis[0];
			node->display_info.display_vsc_startp = axis[1];
			node->display_info.display_hsc_endp = axis[2];
			node->display_info.display_vsc_endp = axis[3];
		}

		node->has_amvideo_node = true;
		node->mode = AML_SCREEN_MODE_ADAPTIVE;
		/* AML_SCREEN_MODE_ADAPTIVE; */
		if (amlvideo2_dbg_en) {
			pr_info("screen:h_start = %d,h_end = %d\n",
				node->display_info.screen_vd_h_start_,
				node->display_info.screen_vd_h_end_);
			pr_info("screen:v_start = %d,v_end = %d\n",
				node->display_info.screen_vd_v_start_,
				node->display_info.screen_vd_v_end_);
			pr_info("display:hsc_startp=%d,hsc_endp=%d\n",
				node->display_info.display_hsc_startp,
				node->display_info.display_hsc_endp);
			pr_info("display:vsc_startp=%d,vsc_endp=%d\n",
				node->display_info.display_vsc_startp,
				node->display_info.display_vsc_endp);
			pr_info("frame:hd_start_lines=%d,hd_end_lines=%d\n",
			 node->display_info.frame_hd_start_lines_,
			 node->display_info.frame_hd_end_lines_);
			pr_info("frame:vd_start_lines=%d,vd_end_lines=%d\n",
			 node->display_info.frame_vd_start_lines_,
			 node->display_info.frame_vd_end_lines_);
		}
	}

	if (amlvideo2_dbg_en) {
		pr_info("node->porttype = %d\n", node->porttype);
		pr_info("crop_enable = %d, node->porttype = 0x%x.\n",
			node->crop_info.capture_crop_enable, node->porttype);
		pr_info("node->r_type=%d, node->p_type=%d\n",
			node->r_type, node->p_type);
		pr_info("vinfo->w=%d, vinfo->h=%d, vinfo->field_height=%d.\n",
			vinfo->width, vinfo->height, vinfo->field_height);
		pr_info("video_w=%d, video_h=%d.\n",
			video_input_parms.width, video_input_parms.height);
	}
#endif

	if (!node->start_vdin_flag || node->r_type != AML_RECEIVER_NONE)
		goto start;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	memset(&para, 0, sizeof(para));
	para.port = node->porttype;
	para.fmt = TVIN_SIG_FMT_MAX;
	para.frame_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
	if (para.port == TVIN_PORT_VIU1_WB0_VD1 ||
	    para.port == TVIN_PORT_VIU2_VD1) {
		para.h_active = (node->display_info.display_hsc_endp -
		node->display_info.display_hsc_startp + 1);
		para.v_active = (node->display_info.display_vsc_endp -
		node->display_info.display_vsc_startp + 1);
	} else if (para.port == TVIN_PORT_VIU1_VIDEO) {
		/*only video point 7*/
		para.h_active = video_input_parms.width;
		para.v_active = video_input_parms.height;
	} else {
		para.h_active = vinfo->width;
		para.v_active = vinfo->height;
	}

	para.hsync_phase = 0;
	para.vsync_phase = 1;
	para.hs_bp = 0;
	para.vs_bp = 0;
	para.dfmt = TVIN_NV21;/* TVIN_YUV422; */
#ifdef PREVIOUS_VOUT_MODE
	/* TVIN_SCAN_MODE_PROGRESSIVE; */
	para.scan_mode = vmode2scan_mode(vinfo->mode);
#else
	if (vinfo->height == vinfo->field_height)
		para.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
	else
		para.scan_mode = TVIN_SCAN_MODE_INTERLACED;
#endif
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.v_active = para.v_active / 2;

	if (vinfo->width > vinfo->height && (node->qctl_regs[0] % 180)) {
		para.dest_h_active = fh->height;
		para.dest_v_active = fh->width;
	} else {
		para.dest_h_active = fh->width;
		para.dest_v_active = fh->height;
	}

	if (fh->width < VDIN1_SCALE_W || fh->height < VDIN1_SCALE_H) {
		para.dest_h_active = VDIN1_SCALE_W;
		para.dest_v_active = VDIN1_SCALE_H;
		if (amlvideo2_dbg_en)
			pr_info("need vdin scale, dst_w: %d, dst_h: %d,", fh->width, fh->height);
	}
	if (amlvideo2_dest_w != 0)
		para.dest_h_active = amlvideo2_dest_w;
	if (amlvideo2_dest_h != 0)
		para.dest_v_active = amlvideo2_dest_h;
	para.crop[0] = node->crop_info.source_top_crop;
	para.crop[1] = node->crop_info.source_left_crop;
	para.crop[2] = node->crop_info.source_width_crop;
	para.crop[3] = node->crop_info.source_height_crop;
	if (para.crop[0] > para.v_active || para.crop[1] > para.h_active) {
		pr_info("crop err, modify top: %d, left: %d\n", para.crop[0], para.crop[1]);
		para.crop[0] = 0;
		para.crop[1] = 0;
	}
	if (para.crop[2] > para.h_active || para.crop[3] > para.v_active) {
		pr_info("crop err, modify width: %d, height: %d\n", para.crop[2], para.crop[3]);
		para.crop[2] = para.h_active;
		para.crop[3] = para.v_active;
	}
	if (para.crop[2] == 0 || para.crop[3] == 0) {
		para.crop[0] = 0;
		para.crop[1] = 0;
		para.crop[2] = para.h_active;
		para.crop[3] = para.v_active;
	}
	para.reserved |= PARAM_STATE_SCREEN_CAP;
	if (para.scan_mode == TVIN_SCAN_MODE_INTERLACED)
		para.dest_v_active = para.dest_v_active / 2;
	if (para.port == TVIN_PORT_VIU1_VIDEO ||
	    para.port == TVIN_PORT_VIU1_WB0_VD1 ||
	    para.port == TVIN_PORT_VIU2_VD1 ||
	    para.port == TVIN_PORT_VIU1_WB0_OSD1 ||
	    para.port == TVIN_PORT_VIU1_WB0_OSD2 ||
	    para.port == TVIN_PORT_VIU2_OSD1 ||
	    para.port == TVIN_PORT_VIU3_OSD1) {
		if (node->ge2d_multi_process_flag) {
			para.dest_h_active = 384;
			para.dest_v_active = 216;
		} else {
			para.cfmt = 1;
		}
	}
	para.is_one_buffer = aml2_dev->is_one_buffer;
	if (fh->quantization_range == QUANTIZATION_LIMITED)
		para.quantization_range = 2;
	else if (fh->quantization_range == QUANTIZATION_FULL)
		para.quantization_range = 1;
	else
		fh->quantization_range = 0;

	if (fh->secure_data.secure_capture_type ==
		V4L2_EXT_AMLVIDEO2_SECURE_TEE_ONE_BUFFER)
		para.support_secure = true;
	else
		para.support_secure = false;

	if (amlvideo2_dbg_en) {
		pr_info("para.h_active: %d, para.v_active: %d,",
			para.h_active, para.v_active);
		pr_info("para.dest_h_active: %d, para.dest_v_active: %d,",
			para.dest_h_active, para.dest_v_active);
		pr_info("fh->width: %d, fh->height: %d,",
			fh->width, fh->height);
		pr_info("vinfo->mode: %d,para.scan_mode: %d\n",
			vinfo->mode, para.scan_mode);
		pr_info("node->vdin_device_num = %d, is_one_buffer=%d.\n",
			node->vdin_device_num, para.is_one_buffer);
		pr_info("crop:top %d left %d width %d height %d\n",
			para.crop[0], para.crop[1], para.crop[2], para.crop[3]);
		pr_info("quantization_range %d, vdin_support_secure: %d,",
			para.quantization_range, para.support_secure);
	}
	if (IS_ERR_OR_NULL(vops)) {
		if (node->r_type == AML_RECEIVER_NONE)
			amlvideo2_stop_thread(dma_q);
		pr_info("%s amlvideo2 vdin ops is NULL\n", __func__);
		return -EBUSY;
	}
	if (node->vdin_port_ext < 0) {
		if (IS_ERR_OR_NULL(vops->start_tvin_service)) {
			if (node->r_type == AML_RECEIVER_NONE)
				amlvideo2_stop_thread(dma_q);
			pr_info("%s amlvideo2 vdin start_tvin_service is NULL\n",
				__func__);
			return -EBUSY;
		}
		ret = vops->start_tvin_service(node->vdin_device_num, &para);
	} else {
		if (IS_ERR_OR_NULL(vops->start_tvin_service_ex)) {
			if (node->r_type == AML_RECEIVER_NONE)
				amlvideo2_stop_thread(dma_q);
			pr_info("%s amlvideo2 vdin start_tvin_service_ex is NULL\n",
				__func__);
			return -EBUSY;
		}
		ret = vops->start_tvin_service_ex(node->vdin_device_num,
						  node->vdin_port_ext, &para);
	}
	if (ret < 0) {
		if (node->r_type == AML_RECEIVER_NONE)
			amlvideo2_stop_thread(dma_q);
		pr_info("%s amlvideo2 start vdin fail\n",
			__func__);
		return -EBUSY;
	}
	node->vdin1_is_start = true;
#endif
start: node->frame_inittime = 1;
	/* frameinv_adjust = 0; */
	/* frameinv = 0; */
	/* tmp_vf = NULL; */
	ktime_get_ts64(&node->thread_ts1);
#ifdef TEST_LATENCY
	node->latency_info.cur_time = node->thread_ts1.tv_sec;
	node->latency_info.cur_time_out = node->thread_ts1.tv_sec;
	node->latency_info.total_latency = 0;
	node->latency_info.total_latency_out = 0;
#endif
	return 0;
}

static int vidioc_streamoff(struct file *file, void *priv, enum v4l2_buf_type i)
{
	int ret;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	struct vdin_v4l2_ops_s *vops = &node->vops;
#endif

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE || i != fh->type)
		return -EINVAL;
	ret = vb2_ioctl_streamoff(file, priv, i);
	if (ret < 0) {
		pr_err("videobuf2 stream off failed\n");
		return 0;
	}

	is_black_frame[node->vid] = false;

	if (amlvideo2_dbg_en) {
		pr_info("%s , %d\n", __func__, __LINE__);
		pr_info("start_vdin_flag = %d\n", node->start_vdin_flag);
		pr_info("node->r_type = %d, node->vid = %d\n",
			node->r_type, node->vid);
		pr_info("vdin_device_num = %d, vdin1_is_start = %d\n",
			node->vdin_device_num, node->vdin1_is_start);
	}

	mutex_lock(&fh->mutex);
	if (node->start_vdin_flag ||
	    node->r_type == AML_RECEIVER_NONE) {
		if (amlvideo2_dbg_en)
			pr_info("stop tvin service .\n");
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
		if (IS_ERR_OR_NULL(vops)) {
			mutex_unlock(&fh->mutex);
			pr_info("%s amlvideo2 vdin ops is NULL\n", __func__);
			return 0;
		}
		if (IS_ERR_OR_NULL(vops->stop_tvin_service)) {
			mutex_unlock(&fh->mutex);
			pr_info("%s amlvideo2 vdin stop_tvin_service is NULL\n", __func__);
			return 0;
		}

		if (node->aml2_dev->node_id == 1 && node->vdin1_is_start) {
			ret = vops->stop_tvin_service(node->vdin_device_num);
			if (ret < 0) {
				mutex_unlock(&fh->mutex);
				pr_err("%s amlvideo2 vdin stop failed\n", __func__);
				return 0;
			}
			node->vdin1_is_start = false;
		}
#endif
	}
	node->start_vdin_flag = 0;
	mutex_unlock(&fh->mutex);

	if (node->r_type == AML_RECEIVER_NONE)
		amlvideo2_stop_thread(&node->vidq);

	fh->is_streamed_on = 0;
	return ret;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct amlvideo2_node *node = video_drvdata(file);
	struct amlvideo2_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;

	for (i = 0; i < ARRAY_SIZE(formats); i++) {
		if (formats[i].fourcc == fsize->pixel_format) {
			fmt = &formats[i];
			break;
		}
	}
	if (!fmt)
		return -EINVAL;
	if (fmt->fourcc == V4L2_PIX_FMT_NV21 ||
	    fmt->fourcc == V4L2_PIX_FMT_NV12 ||
	    fmt->fourcc == V4L2_PIX_FMT_YUV420 ||
	    fmt->fourcc == V4L2_PIX_FMT_YVU420 ||
	    fmt->fourcc == V4L2_PIX_FMT_BGR24 ||
	    fmt->fourcc == V4L2_PIX_FMT_RGB565X ||
	    fmt->fourcc == V4L2_PIX_FMT_RGB24) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_prev_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_prev_resolution[fsize->index];
		if (!node->aml2_dev->support_4k_capture &&
		    (frmsize->width > norm_maxw() || frmsize->height > norm_maxh()))
			return -EINVAL;
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	} else if (fmt->fourcc == V4L2_PIX_FMT_RGB32 ||
		fmt->fourcc == V4L2_PIX_FMT_ARGB32 ||
		fmt->fourcc == V4L2_PIX_FMT_BGR32) {
		if (fsize->index >= ARRAY_SIZE(amlvideo2_pic_resolution))
			return -EINVAL;
		frmsize = &amlvideo2_pic_resolution[fsize->index];
		if (!node->aml2_dev->support_4k_capture &&
		    (frmsize->width > norm_maxw() || frmsize->height > norm_maxh()))
			return -EINVAL;
		fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
		fsize->discrete.width = frmsize->width;
		fsize->discrete.height = frmsize->height;
	}
	return ret;
}

static int vidioc_enum_frameintervals(struct file *file, void *priv,
				      struct v4l2_frmivalenum *fival)
{
	unsigned int k;

	if (fival->index > ARRAY_SIZE(amlvideo2_frmivalenum))
		return -EINVAL;
	for (k = 0; k < ARRAY_SIZE(amlvideo2_frmivalenum); k++) {
		if (fival->index == amlvideo2_frmivalenum[k].index &&
		    fival->pixel_format ==
		    amlvideo2_frmivalenum[k].pixel_format &&
		    fival->width == amlvideo2_frmivalenum[k].width &&
		    fival->height == amlvideo2_frmivalenum[k].height) {
			memcpy(fival, &amlvideo2_frmivalenum[k],
			       sizeof(struct v4l2_frmivalenum));
			return 0;
		}
	}
	return -EINVAL;
}

static int vidioc_s_std(struct file *file, void *priv, v4l2_std_id i)
{
	return 0;
}

/* --- controls ---------------------------------------------- */

static int amlvideo2_setting(struct amlvideo2_node *node, int PROP_ID,
			     int value, int index)
{
	int ret = 0;

	switch (PROP_ID) {
	case V4L2_CID_ROTATE:
		if (node->qctl_regs[index] != value)
			node->qctl_regs[index] = value;

		if (node->qctl_regs[index] == 0)
			node->qctl_regs[index] = amlvideo2_angle;
		pr_info("amlvideo2: set rotate=%d\n", node->qctl_regs[index]);
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (qc->id && qc->id == amlvideo2_node_qctrl[i].id) {
			memcpy(qc, &amlvideo2_node_qctrl[i], sizeof(*qc));
			return 0;
		}
	return -EINVAL;
}

static int vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	*i = node->input;
	return 0;
}

static int vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	/*bit 28 : start tvin service flag, 1 : enable,  0 : disable*/
	node->start_vdin_flag = (i >> 28);
	/*bit 24 : vdin device num : 0 or 1 */
	node->vdin_device_num = (i >> 24) & 1;
	if (node->vdin_device_num == 1 && is_meson_t3x_cpu())
		node->vdin_device_num = 2;
	node->ge2d_multi_process_flag = (i >> 16) & 1;

	/* 0: vpp0 video only; 1: vpp0(osd+video); 2: vpp1 video only; 3: vpp1(osd+video)
	 * 4: vpp0 osd1 only; 5: vpp0 osd2 only; 6: vpp1 osd1 only; 7: vpp2 osd1 only
	 * 8: vpp0 video only but no pq; 9: vpp0(osd+video no pq)
	 */
	if ((i & 0xffff) == 1) {
		node->vdin_port_ext = PORT_VPP0_OSD_VIDEO;
		node->porttype = TVIN_PORT_VIU1_WB0_VPP;
		node->screencap_type = 1;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0(osd+video)\n");
	} else if ((i & 0xffff) == 0) {
		node->vdin_port_ext = PORT_VPP0_VIDEO_ONLY;
		node->porttype = TVIN_PORT_VIU1_WB0_VD1;
		node->screencap_type = 0;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0 video only\n");
	} else if ((i & 0xffff) == 2) {
		node->vdin_port_ext = PORT_VPP1_VIDEO_ONLY;
		node->porttype = TVIN_PORT_VIU2_VD1;
		node->screencap_type = 0;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp1 video only\n");
	} else if ((i & 0xffff) == 3) {
		node->vdin_port_ext = PORT_VPP1_OSD_VIDEO;
		node->porttype = TVIN_PORT_VIU2_VPP;
		node->screencap_type = 1;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp1(osd+video)\n");
	} else if ((i & 0xffff) == 4) {
		node->vdin_port_ext = PORT_VPP0_OSD1_ONLY;
		node->porttype = TVIN_PORT_VIU1_WB0_OSD1;
		node->screencap_type = 2;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0 osd1 only\n");
	} else if ((i & 0xffff) == 5) {
		node->vdin_port_ext = PORT_VPP0_OSD2_ONLY;
		node->porttype = TVIN_PORT_VIU1_WB0_OSD2;
		node->screencap_type = 2;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0 osd2 only)\n");
	} else if ((i & 0xffff) == 6) {
		node->vdin_port_ext = PORT_VPP1_OSD1_ONLY;
		node->porttype = TVIN_PORT_VIU2_OSD1;
		node->screencap_type = 2;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp1 osd1 only)\n");
	} else if ((i & 0xffff) == 7) {
		node->vdin_port_ext = PORT_VPP2_OSD1_ONLY;
		node->porttype = TVIN_PORT_VIU3_OSD1;
		node->screencap_type = 2;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp2 osd1 only)\n");
	} else if ((i & 0xffff) == 8) {
		node->vdin_port_ext = PORT_VPP0_VIDEO_ONLY_NO_PQ;
		node->porttype = TVIN_PORT_VIU1_VIDEO;
		node->screencap_type = 0;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0 video only no pq)\n");
	} else if ((i & 0xffff) == 9) {
		node->vdin_port_ext = PORT_VPP0_OSD_VIDEO_NO_PQ;
		node->porttype = TVIN_PORT_VIU1_WB0_POST_BLEND;
		node->screencap_type = 1;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: vpp0(osd+video) no pq\n");
	} else {
		node->vdin_port_ext = -1;
		node->porttype = i & 0xffff;
		node->screencap_type = -1;
		if (amlvideo2_dbg_en)
			pr_info("amlvideo2: porttype err)\n");
	}
	if (amlvideo2_dbg_en) {
		if (node->vdin_port_ext < 0)
			pr_info("porttype:%x ,start_vdin_flag = %d.\n",
				node->porttype, node->start_vdin_flag);
		else
			pr_info("data: vdin_port_ext=%d, start_vdin_flag=%d.\n",
				node->vdin_port_ext, node->start_vdin_flag);
		pr_info("%s, vdin_device_num = %d\n",
			__func__, node->vdin_device_num);
		pr_info("%s,ge2d_multi_process_flag = %d\n",
			__func__, node->ge2d_multi_process_flag);
	}
	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int i;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			ctrl->value = node->qctl_regs[i];
			return 0;
		}
	return -EINVAL;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int i;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;

	for (i = 0; i < ARRAY_SIZE(amlvideo2_node_qctrl); i++)
		if (ctrl->id == amlvideo2_node_qctrl[i].id) {
			if (ctrl->value < amlvideo2_node_qctrl[i].minimum ||
			    ctrl->value > amlvideo2_node_qctrl[i].maximum ||
				amlvideo2_setting(node, ctrl->id,
						  ctrl->value, i) < 0) {
				return -ERANGE;
			}
			/* node->qctl_regs[i] = ctrl->value; */
			return 0;
		}
	return -EINVAL;
}

static int vidioc_g_ext_ctrls(struct file *file, void *fh1,
			  struct v4l2_ext_controls *ctrls)
{
	int i, ret = 0;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct v4l2_ext_control *ctrl = ctrls->controls;

	for (i = 0; i < ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_AMLVIDEO2_OUTPUT_INFO:
			ret = amlvideo2_vidioc_g_cid_output_info(fh, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_AMLVIDEO2_GET_VIDEO_INFO:
			ret = amlvideo2_vidioc_g_cid_video_info(fh, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_AMLVIDEO2_GET_DISPLAY_INFO:
			ret = amlvideo2_vidioc_g_cid_display_info(fh, ctrl);
			break;
		default:
			break;
		}
		if (ret)
			break;
	}

	if (amlvideo2_dbg_en) {
		pr_info("amlvideo2:g_ext src %d %d dst:%d %d\n",
		fh->ext_output_data.input_w,
		fh->ext_output_data.input_h,
		fh->ext_output_data.output_w,
		fh->ext_output_data.output_h);

		pr_info("amlvideo2:g_ext video input %d %d\n",
		fh->video_data.input_w,
		fh->video_data.input_h);

		pr_info("amlvideo2:g_ext display info %d %d\n",
		fh->vinfo_data.width,
		fh->vinfo_data.height);
	}

	return ret;
}

static int vidioc_s_ext_ctrls(struct file *file, void *fh1,
			  struct v4l2_ext_controls *ctrls)
{
	int i, ret = 0;
	struct v4l2_ext_control *ctrl = ctrls->controls;
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);

	for (i = 0; i < ctrls->count; i++) {
		ctrl = &ctrls->controls[i];
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_AMLVIDEO2_SET_SECURE_INFO:
			ret = amlvideo2_vidioc_s_cid_secure_info(fh, ctrl);
			break;
		default:
			break;
		}
		if (ret)
			break;
	}

	if (amlvideo2_dbg_en)
		pr_info("amlvideo2:s_ext phy_addr=%llx, size=%x, secure_type=%d\n",
			fh->secure_data.secure_phy_addr,
			fh->secure_data.secure_buffer_size,
			fh->secure_data.secure_capture_type);

	return ret;
}

static int vidioc_g_crop(struct file *file, void *fh,
			 struct v4l2_rect *rect)
{
	struct amlvideo2_node *node = video_drvdata(file);

	rect->top = node->crop_info.source_top_crop;
	rect->left = node->crop_info.source_left_crop;
	rect->width = node->crop_info.source_width_crop;
	rect->height = node->crop_info.source_height_crop;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *fh,
			 const struct v4l2_rect *rect)
{
	struct amlvideo2_node *node = video_drvdata(file);

	if (rect->width == 0 || rect->height == 0) {
		pr_info("disable capture crop\n");
		node->crop_info.capture_crop_enable = 0;
		node->crop_info.source_top_crop = 0;
		node->crop_info.source_left_crop = 0;
		node->crop_info.source_width_crop = 0;
		node->crop_info.source_height_crop = 0;
	} else {
		node->crop_info.source_top_crop = rect->top;
		node->crop_info.source_left_crop = rect->left;
		node->crop_info.source_width_crop = rect->width;
		node->crop_info.source_height_crop = rect->height;
		node->crop_info.capture_crop_enable = 1;
	}

	return 0;
}

static int vidioc_g_selection(struct file *file, void *fh,
			struct v4l2_selection *sel)
{
	vidioc_g_crop(file, fh, &sel->r);
	return 0;
}

static int vidioc_s_selection(struct file *file, void *fh,
			struct v4l2_selection *sel)
{
	vidioc_s_crop(file, fh, &sel->r);
	return 0;
}

static int vidioc_s_output(struct file *file, void *fh,
			   unsigned int mode)
{
	struct amlvideo2_node *node = video_drvdata(file);

	if (mode > AML_SCREEN_MODE_MAX)
		return -1;
	node->mode = (enum aml_screen_mode_e)mode;
	pr_info("amlvideo2 set aml_screen_mode_e =%d\n", mode);
	return 0;
}

static int vidioc_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	if (amlvideo2_dbg_en & 1)
		pr_info("%s: sub=%d\n", __func__, sub->type);

	switch (sub->type) {
	case V4L2_EVENT_PRIVATE_EXT_VDIN_PATTERN:
		return v4l2_event_subscribe(fh, sub, 5, NULL);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	if (amlvideo2_dbg_en & 1)
		pr_info("%s: sub=%d\n", __func__, sub->type);

	return v4l2_event_unsubscribe(fh, sub);
}

static int alloc_canvas(struct amlvideo2_node *node)
{
	const char *keep_owner = "amlvideo2";

	if (node->aml2_canvas[0] < 0)
		node->aml2_canvas[0] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (node->aml2_canvas[1] < 0)
		node->aml2_canvas[1] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (node->aml2_canvas[2] < 0)
		node->aml2_canvas[2] =
			canvas_pool_map_alloc_canvas(keep_owner);

	if (node->aml2_canvas[0] < 0 ||
	    node->aml2_canvas[1] < 0 ||
	    node->aml2_canvas[2] < 0) {
		pr_err("amlvideo2 canvas alloc failed\n");
		return -1;
	}
	return 0;
}

static int free_canvas(struct amlvideo2_node *node)
{
	if (node->aml2_canvas[0] >= 0) {
		canvas_pool_map_free_canvas(node->aml2_canvas[0]);
		node->aml2_canvas[0] = -1;
	}

	if (node->aml2_canvas[1] >= 0) {
		canvas_pool_map_free_canvas(node->aml2_canvas[1]);
		node->aml2_canvas[1] = -1;
	}

	if (node->aml2_canvas[2] >= 0) {
		canvas_pool_map_free_canvas(node->aml2_canvas[2]);
		node->aml2_canvas[2] = -1;
	}

	if (node->aml2_canvas[0] >= 0 ||
	    node->aml2_canvas[1] >= 0 ||
	    node->aml2_canvas[2] >= 0) {
		pr_err("amlvideo2 canvas free failed!\n");
		return -1;
	}
	return 0;
}

static void video_vb2_discard_done(struct vb2_queue *q)
{
	struct vb2_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&q->done_lock, flags);
	list_for_each_entry(vb, &q->done_list, done_entry)
		vb->state = VB2_BUF_STATE_ERROR;
	spin_unlock_irqrestore(&q->done_lock, flags);
}

static int amlvideo2_vb2ops_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vq);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_fh *fh = node->fh;

	*num_planes = 1;
	sizes[0] = fh->sizeimage;
	if (*num_buffers > VIDEO_Q_BUFF_NUM)
		*num_buffers = VIDEO_Q_BUFF_NUM;
	while (sizes[0] * *num_buffers > vid_limit * 1024 * 1024)
		(*num_buffers)--;

	if (aml2_dev->use_reserve != 1) {
		alloc_devs[0] = v4l_get_dev_from_codec_mm();
		node->mem_form_dev = alloc_devs[0];
		if (amlvideo2_dbg_en & 0x10)
			pr_info("%s, alloc mem from codec_mm\n", __func__);
	} else {
		alloc_devs[0] = &aml2_dev->pdev->dev;
		node->mem_form_dev = alloc_devs[0];
		if (amlvideo2_dbg_en & 0x10)
			pr_info("%s, alloc mem from CMA mem\n", __func__);
	}
	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s, count=%d, size=%d\n", __func__, *num_buffers, sizes[0]);

	return 0;
}

static int amlvideo2_vb2ops_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vb->vb2_queue);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_fh *fh = node->fh;
	struct vb2_queue *vb2_q = &fh->vb2_q;
	u32 size = fh->sizeimage;

	if (vb2_plane_size(vb, 0) < size) {
		pr_err("Error user buffer too small (%ld < %u)\n",
			vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}

	vb2_set_plane_payload(vb, 0, size);
	if (amlvideo2_dbg_en & 0x10)
		pr_info("buf prepare idx:%d max_bufs:%d planes:%d queue_cnt:%d, buf sts:%s\n",
			vb->index, vb2_q->max_num_buffers,
			vb->num_planes, vb2_q->queued_count,
			vb2_buf_sts_to_str(vb->state));

	vbuf->field = V4L2_FIELD_INTERLACED;

	return 0;
}

static void amlvideo2_vb2ops_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct amlvideo2_node_buffer *buff = container_of(vbuf, struct amlvideo2_node_buffer, vb2);
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vb->vb2_queue);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_node_dmaqueue *vidq = &node->vidq;
	unsigned long flags = 0;

	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s buf:%d, state:%s\n", __func__, vb->index,
			vb2_buf_sts_to_str(vbuf->vb2_buf.state));

	spin_lock_irqsave(&vidq->buff_list_lock, flags);
	list_add_tail(&buff->list, &vidq->head);
	spin_unlock_irqrestore(&vidq->buff_list_lock, flags);
}

static int amlvideo2_vb2ops_buf_init(struct vb2_buffer *vb)
{
	u32 p_size = 0;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct amlvideo2_node_buffer *buf = container_of(vbuf, struct amlvideo2_node_buffer, vb2);
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vb->vb2_queue);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_fh *fh = node->fh;

	buf->addr[AML_PLANE_A] = *((u32 *)vb2_plane_cookie(vb, 0));
	buf->vaddr[AML_PLANE_A] = vb2_plane_vaddr(vb, 0);

	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s: addr 0x%x, vaddr 0x%px\n",
			__func__,
			(u32)buf->addr[AML_PLANE_A],
			buf->vaddr[AML_PLANE_A]);

	if (fh->fmt->fourcc == V4L2_PIX_FMT_NV12 ||
		fh->fmt->fourcc == V4L2_PIX_FMT_NV21) {
		p_size = fh->bytesperline * fh->height;
		buf->addr[AML_PLANE_B] = buf->addr[AML_PLANE_A] + p_size;
	}
	return 0;
}

static int amlvideo2_vb2ops_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vq);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_node_dmaqueue *vidq = &node->vidq;
	struct list_head *head = &vidq->head;

	if (list_empty(head)) {
		pr_err("%s: list is empty\n", __func__);
		return -EINVAL;
	}
	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s\n", __func__);

	return 0;
}

static void amlvideo2_vb2ops_stop_streaming(struct vb2_queue *vq)
{
	struct amlvideo2_device *aml2_dev = vb2_get_drv_priv(vq);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct amlvideo2_node_dmaqueue *vidq = &node->vidq;
	struct amlvideo2_node_buffer *buff;
	unsigned long flags = 0;

	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s start\n", __func__);

	spin_lock_irqsave(&vidq->buff_list_lock, flags);
	list_for_each_entry(buff, &vidq->head, list) {
		if (amlvideo2_dbg_en & 0x10)
			pr_info("%s, index=%d\n", __func__, buff->vb2.vb2_buf.index);
		vb2_buffer_done(&buff->vb2.vb2_buf, VB2_BUF_STATE_ERROR);
	}
	INIT_LIST_HEAD(&vidq->head);
	video_vb2_discard_done(vq);
	spin_unlock_irqrestore(&vidq->buff_list_lock, flags);
	if (amlvideo2_dbg_en & 0x10)
		pr_info("%s\n", __func__);
}

static struct vb2_ops amlvideo2_vb2ops = {
	.queue_setup		= amlvideo2_vb2ops_queue_setup,
	.buf_prepare		= amlvideo2_vb2ops_buffer_prepare,
	.buf_queue			= amlvideo2_vb2ops_buffer_queue,
	.buf_init			= amlvideo2_vb2ops_buf_init,
	.start_streaming	= amlvideo2_vb2ops_start_streaming,
	.stop_streaming		= amlvideo2_vb2ops_stop_streaming,
	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static int amlvideo2_v4l2_queue_init(struct amlvideo2_device *aml2_dev, struct vb2_queue *vb2_q)
{
	int ret = 0;

	vb2_q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vb2_q->io_modes = VB2_MMAP | VB2_DMABUF | VB2_READ;

	vb2_q->dev = &aml2_dev->pdev->dev;
	vb2_q->drv_priv = aml2_dev;
	vb2_q->buf_struct_size = sizeof(struct amlvideo2_node_buffer);
	vb2_q->ops = &amlvideo2_vb2ops;
	vb2_q->mem_ops = &vb2_dma_contig_memops;
	vb2_q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	vb2_q->min_queued_buffers = 1;
	vb2_q->lock = &aml2_dev->q_lock;

	ret = vb2_queue_init(vb2_q);
	if (ret < 0)
		pr_err("%s: vb2_queue_init fail\n", __func__);
	return ret;
}

int amlvideo2_cma_buf_init(struct amlvideo2_device *aml2_dev,  int node_id)
{
	struct amlvideo2_node *node = aml2_dev->node[node_id];
	int flags;
	const char *canvas_owner = NULL;
	int page_count;
	int ret;

	ret = alloc_canvas(aml2_dev->node[node_id]);
	if (ret < 0) {
		pr_err("amlvideo2 alloc cma failed!\n");
		return -1;
	}

	if (node->ge2d_multi_process_flag == 1) {
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

		if (node_id == 0)
			canvas_owner = "amlvideo2.0";
		else
			canvas_owner = "amlvideo2.1";

		page_count = 4 * SZ_1M / PAGE_SIZE;
		aml2_dev->buffer_size = 4 * SZ_1M;

		aml2_dev->buffer_start =
			codec_mm_alloc_for_dma(canvas_owner,
						page_count,
						0,
						flags);

		if (!(aml2_dev->buffer_start)) {
			pr_err("amlvideo2 alloc cma buffer failed\n");
			ret = free_canvas(aml2_dev->node[node_id]);
			if (ret < 0)
				pr_err("%s free canvas fail! line: %d",
					   __func__, __LINE__);
			return -1;
		}

		pr_info("%s cma memory is %x , size is	%x\n",
			canvas_owner,
			(unsigned int)aml2_dev->buffer_start,
			(unsigned int)aml2_dev->buffer_size);
	}

	return 0;
}

int amlvideo2_cma_buf_uninit(struct amlvideo2_device *aml2_dev, int node_id)
{
	int ret;

	ret = free_canvas(aml2_dev->node[node_id]);
	if (ret < 0)
		pr_err("%s free canvas fail! line: %d",
			   __func__, __LINE__);
	if (aml2_dev->buffer_start != 0) {
		if (node_id == 0) {
			codec_mm_free_for_dma
				("amlvideo2.0",
				 aml2_dev->buffer_start);
		} else {
			codec_mm_free_for_dma
				("amlvideo2.1",
				 aml2_dev->buffer_start);
		}
		aml2_dev->buffer_start = 0;
		aml2_dev->buffer_size = 0;
		if (node_id == 0)
			pr_info("amlvideo2.0 cma memory release succeed\n");
		else
			pr_info("amlvideo2.1 cma memory release succeed\n");
	}

	return 0;
}

/* ------------------------------------------------------------------
 * File operations for the device
 * ------------------------------------------------------------------
 */
static int amlvideo2_open(struct file *file)
{
	struct amlvideo2_node *node = video_drvdata(file);
	struct amlvideo2_fh *fh = NULL;
	int temp_canvas;
	const char *canvas_owner0 = "amlvideo2.0";
	const char *canvas_owner1 = "amlvideo2.1";
	int i;
	int ret;

#ifdef CONFIG_PM
	if (atomic_read(&node->is_suspend))
		return -ENOMEM;
#endif
	mutex_lock(&node->mutex);
	node->users++;
	if (node->users > 1) {
		node->users--;
		mutex_unlock(&node->mutex);
		pr_err("amlvideo2 has already open, can't open again.\n");
		return -EBUSY;
	}

	ret = amlvideo2_cma_buf_init(node->aml2_dev, node->vid);
	if (ret < 0) {
		if (node->vid == 0)
			pr_err("alloc amlvideo2.0 cma buffer failed.\n");
		else
			pr_err("alloc amlvideo2.1 cma buffer failed.\n");
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}

	if (node->vid == 0) {
		for (i = 0; i < 2 * BUFFER_COUNT; i++) {
			amlvideo2_canvas[0][i] =
				canvas_pool_map_alloc_canvas(canvas_owner0);
		}
	} else {
		for (i = 0; i < 2 * BUFFER_COUNT; i++) {
			amlvideo2_canvas[1][i] =
				canvas_pool_map_alloc_canvas(canvas_owner1);
		}
	}

	for (i = 0; i < 2 * BUFFER_COUNT; i++)
		if (amlvideo2_canvas[node->vid][i] < 0)
			break;
	if (i != 2 * BUFFER_COUNT && amlvideo2_canvas[node->vid][i] < 0) {
		for (i = 0; i < 2 * BUFFER_COUNT; i++) {
			if (amlvideo2_canvas[node->vid][i] >= 0) {
				temp_canvas = amlvideo2_canvas[node->vid][i];
				canvas_pool_map_free_canvas(temp_canvas);
				amlvideo2_canvas[node->vid][i] = -1;
			}
		}
		pr_info("%s: %s alloc canvas failed, %d\n",
			(node->vid == 0) ? canvas_owner0 : canvas_owner1,
			__func__, __LINE__);
		amlvideo2_cma_buf_uninit(node->aml2_dev, node->vid);
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}
	fh = node->fh;
	if (!fh) {
		amlvideo2_cma_buf_uninit(node->aml2_dev, node->vid);
		node->users--;
		mutex_unlock(&node->mutex);
		return -ENOMEM;
	}

	v4l2_fh_init(&fh->fh, video_devdata(file));
	file->private_data = &fh->fh;
	v4l2_fh_add(&fh->fh);
	mutex_unlock(&node->mutex);
	node->mode = AML_SCREEN_MODE_RATIO;
	fh->node = node;

	fh->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fh->fmt = &formats[0];
	fh->width = 1920;
	fh->height = 1080;

	fh->set_format_flag = false;
	fh->f_flags = file->f_flags;
	memset(&node->crop_info, 0, sizeof(struct crop_info_s));

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	v4l2_vdin_ops_init(&node->vops);
#endif
	fh->frm_save_time_us = 1000000 / DEF_FRAMERATE;
	return 0;
}

static unsigned int amlvideo2_poll(struct file *file,
				   struct poll_table_struct *wait)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);

	if (fh->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return POLLERR;

	return vb2_fop_poll(file, wait);
}

static void amlvideo2_vb2_queue_release(struct file *file,
	struct video_device *vdev)
{
	struct mutex *lock = NULL;

	lock = vdev->queue->lock ? vdev->queue->lock : vdev->lock;
	if (lock)
		mutex_lock(lock);
	if (file->private_data == vdev->queue->owner) {
		vb2_queue_release(vdev->queue);
		vdev->queue->owner = NULL;
	}
	if (lock)
		mutex_unlock(lock);
}
static int amlvideo2_close(struct file *file)
{
	struct v4l2_fh *vfh = file->private_data;
	struct amlvideo2_fh *fh = container_of(vfh, struct amlvideo2_fh, fh);
	struct amlvideo2_node *node = fh->node;
	struct amlvideo2_node_dmaqueue *dma_q = &node->vidq;
	struct video_device *vdev = video_devdata(file);
	int temp_canvas;
	int i;
	int ret = 0;

	amlvideo2_vb2_queue_release(file, vdev);
	amlvideo2_cma_buf_uninit(node->aml2_dev, node->vid);
	v4l2_fh_del(&fh->fh);
	v4l2_fh_exit(&fh->fh);
	file->private_data = NULL;
	if (dma_q->kthread)
		vidioc_streamoff(file, NULL, 0);

	for (i = 0; i < 2 * BUFFER_COUNT; i++) {
		if (amlvideo2_canvas[node->vid][i] >= 0) {
			temp_canvas = amlvideo2_canvas[node->vid][i];
			canvas_pool_map_free_canvas(temp_canvas);
			amlvideo2_canvas[node->vid][i] = -1;
		}
	}

	for (i = 0; i < BUFFER_COUNT; i++) {
		if (yuv420_canvas[node->vid][i] >= 0) {
			temp_canvas = yuv420_canvas[node->vid][i];
			canvas_pool_map_free_canvas(temp_canvas);
			yuv420_canvas[node->vid][i] = -1;
		}
	}
	mutex_lock(&node->mutex);
	if (node->r_type == AML_RECEIVER_NONE) {
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
	}
	node->users--;
	amlvideo2_frmintervals_active.numerator = 1;
	amlvideo2_frmintervals_active.denominator = DEF_FRAMERATE;
	node->has_amvideo_node = false;
	node->crop_info.source_top_crop = 0;
	node->crop_info.source_left_crop = 0;
	node->crop_info.source_width_crop = 0;
	node->crop_info.source_height_crop = 0;
	node->crop_info.capture_crop_enable = 0;
	/* node->provider = NULL; */
	node->pflag = false;
	mutex_unlock(&node->mutex);

	return ret;
}

static int amlvideo2_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;

	if (amlvideo2_dbg_en & 0x10)
		pr_info("mmap called, vma=0x%08lx\n", (unsigned long)vma);
	ret = vb2_fop_mmap(file, vma);

	if (amlvideo2_dbg_en & 0x10)
		pr_info("vma start=0x%08lx, size=%ld, ret=%d\n",
			(unsigned long)vma->vm_start,
			(unsigned long)vma->vm_end - (unsigned long)vma->vm_start, ret);

	return ret;
}

static const struct v4l2_file_operations amlvideo2_fops = {
.owner = THIS_MODULE,
.open = amlvideo2_open,
.release = amlvideo2_close,
.read = vb2_fop_read,
.poll = amlvideo2_poll,
.unlocked_ioctl = video_ioctl2, /* V4L2 ioctl handler */
.mmap = amlvideo2_mmap, };

static const struct v4l2_ioctl_ops amlvideo2_ioctl_ops = {
.vidioc_querycap = vidioc_querycap,
.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
.vidioc_g_fmt_vid_cap = vidioc_g_fmt_vid_cap,
.vidioc_try_fmt_vid_cap = vidioc_try_fmt_vid_cap,
.vidioc_s_fmt_vid_cap = vidioc_s_fmt_vid_cap,
.vidioc_g_parm = vidioc_g_parm,
.vidioc_s_parm = vidioc_s_parm,
.vidioc_reqbufs = vidioc_reqbufs,
.vidioc_querybuf = vidioc_querybuf,
.vidioc_qbuf = vidioc_qbuf,
.vidioc_expbuf = vb2_ioctl_expbuf,
.vidioc_dqbuf = vidioc_dqbuf,
.vidioc_s_std = vidioc_s_std,
.vidioc_queryctrl = vidioc_queryctrl,
.vidioc_g_ctrl = vidioc_g_ctrl,
.vidioc_s_ctrl = vidioc_s_ctrl,
.vidioc_g_ext_ctrls = vidioc_g_ext_ctrls,
.vidioc_s_ext_ctrls = vidioc_s_ext_ctrls,
.vidioc_streamon = vidioc_streamon,
.vidioc_streamoff = vidioc_streamoff,
.vidioc_enum_framesizes = vidioc_enum_framesizes,
.vidioc_enum_frameintervals = vidioc_enum_frameintervals,
.vidioc_s_input = vidioc_s_input,
.vidioc_g_input = vidioc_g_input,
.vidioc_g_selection = vidioc_g_selection,
.vidioc_s_selection = vidioc_s_selection,
.vidioc_s_output = vidioc_s_output,
.vidioc_subscribe_event = vidioc_subscribe_event,
.vidioc_unsubscribe_event = vidioc_unsubscribe_event,
};

static struct video_device amlvideo2_template = {
.name = "amlvideo2",
.fops =  &amlvideo2_fops,
.ioctl_ops = &amlvideo2_ioctl_ops,
.release = video_device_release,
.tvnorms = V4L2_STD_525_60,
/* .current_norm = V4L2_STD_NTSC_M, */
};

static int amlvideo2_receiver_event_fun(int type, void *data,
					void *private_data)
{
	struct amlvideo2_node *node = (struct amlvideo2_node *)private_data;
	/* (struct amlvideo2_fh *)container_of(
	 * node, struct amlvideo2_fh, node);
	 */
	struct amlvideo2_fh *fh = node->fh;
	struct vframe_states states;
	const char *name = (node->vid == 0) ? DEVICE_NAME0 : DEVICE_NAME1;

	memset(&states, 0, sizeof(struct vframe_states));

	switch (type) {
	case VFRAME_EVENT_PROVIDER_VFRAME_READY:
		node->provide_ready = 1;
		if (amlvideo2_dbg_en & 8)
			pr_info("provider : node->recv.name = %s\n",
				node->recv.name);
		if (vf_peek(node->recv.name))
			wake_up_interruptible(&node->vidq.wq);
		break;
	case VFRAME_EVENT_PROVIDER_QUREY_STATE:
		amlvideo2_vf_states(&states, node);
		if (states.buf_avail_num > 0)
			return RECEIVER_ACTIVE;
		if (vf_notify_receiver(name,
				       VFRAME_EVENT_PROVIDER_QUREY_STATE,
				       NULL) == RECEIVER_ACTIVE) {
			return RECEIVER_ACTIVE;
		}
		return RECEIVER_INACTIVE;
	case VFRAME_EVENT_PROVIDER_START:
		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 1); */
		/* #endif */
		node->sub_recv = vf_get_receiver(name);
		if (amlvideo2_dbg_en) {
			pr_info("provider start : name = %s\n", name);
			pr_info("provider start : sub_recv = %p\n",
				node->sub_recv);
		}
		if (node->sub_recv) {
			node->r_type =
				get_sub_receiver_type(node->sub_recv->name);
			if (amlvideo2_dbg_en) {
				pr_info("%s , node->sub_recv->name=%s\n",
					__func__, node->sub_recv->name);
				pr_info("r_type=%d\n", node->r_type);
			}
			node->provider = vf_get_provider(name);
			if (node->provider) {
				node->p_type =
					get_provider_type(node->provider->name);
				if (amlvideo2_dbg_en) {
					pr_info("provider=%s\n",
						node->provider->name);
					pr_info("node->p_type = %d .\n",
						node->p_type);
				}
			}

			fh->node = node;

			vf_reg_provider(&node->amlvideo2_vf_prov);
			vf_notify_receiver(name,
					   VFRAME_EVENT_PROVIDER_START,
					   NULL);
			amlvideo2_start_thread(fh);
		}

		break;
	case VFRAME_EVENT_PROVIDER_REG:
		node->video_blocking = false;
		vfq_init(&node->q_ready,
			 node->amlvideo2_pool_size,
			 (struct vframe_s **)&node->amlvideo2_pool_ready[0]);
		amlvideo2_total_get_count = 0;
		amlvideo2_total_put_count = 0;
		if (node->vid == 0) {
			is_black_frame[0] = false;
			node->is_reg = true;
		}
		break;
	case VFRAME_EVENT_PROVIDER_UNREG:
		node->provide_ready = 0;
		if (node->r_type != AML_RECEIVER_NONE) {
			amlvideo2_stop_thread(&node->vidq);
			if (amlvideo2_dbg_en)
				pr_info("%s,%d stop thread\n", __func__, __LINE__);
		}
		if (amlvideo2_dbg_en)
			pr_info("pflag = %d\n", node->pflag);

		if (!node->pflag) {
			if (amlvideo2_dbg_en)
				pr_info("unreg amlvideo2 provider\n");
			vf_unreg_provider(&node->amlvideo2_vf_prov);
		}
		if (node->vid == 0) {
			is_black_frame[0] = true;
			node->is_reg = false;
		}

		/* #if MESON_CPU_TYPE >= MESON_CPU_TYPE_MESON6 */
		/* switch_mod_gate_by_name("ge2d", 0); */
		/* #endif */
		break;
	case VFRAME_EVENT_PROVIDER_LIGHT_UNREG:
		if (amlvideo2_dbg_en)
			pr_info("provider light unreg\n");
		break;
	case VFRAME_EVENT_PROVIDER_RESET:
		if (amlvideo2_dbg_en)
			pr_info("provider reset\n");
		vf_notify_receiver(name,
				   VFRAME_EVENT_PROVIDER_RESET,
				   NULL);
		if (node->vidq.task_running) {
			node->video_blocking = true;
			wake_up_interruptible(&node->vidq.wq);
		}
		break;
	case VFRAME_EVENT_PROVIDER_FR_HINT:
	case VFRAME_EVENT_PROVIDER_FR_END_HINT:
	case VFRAME_EVENT_PROVIDER_FCC:
		vf_notify_receiver(name, type, data);
		break;
	default:
		break;
	}
	return 0;
}

static const struct vframe_receiver_op_s video_vf_receiver = {
.event_cb = amlvideo2_receiver_event_fun};
/* -----------------------------------------------------------------
 * Initialization and module stuff
 * -----------------------------------------------------------------
 */
#ifdef CONFIG_PM
static int amlvideo2_drv_suspend(struct device *dev)
{
	int i;
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct amlvideo2_device *vid_dev =
		container_of(v4l2_dev, struct amlvideo2_device, v4l2_dev);
	struct amlvideo2_node_dmaqueue *dma_q;
	struct amlvideo2_node *node = NULL;

	for (i = 0; i < vid_dev->node_num; i++) {
		node = vid_dev->node[i];
		if (node) {
			atomic_set(&node->is_suspend, 1);
			dma_q = &node->vidq;
			if (dma_q->kthread && !node->could_suspend)
				wait_for_completion(&node->suspend_sema);
			pr_info("amlvideo2.%d suspend ok\n", i);
		}
	}
	return 0;
}

static int amlvideo2_drv_resume(struct device *dev)
{
	int i;
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct amlvideo2_device *vid_dev =
		container_of(v4l2_dev, struct amlvideo2_device, v4l2_dev);
	struct amlvideo2_node_dmaqueue *dma_q;

	for (i = 0; i < vid_dev->node_num; i++) {
		if (vid_dev->node[i]) {
			dma_q = &vid_dev->node[i]->vidq;
			atomic_set(&vid_dev->node[i]->is_suspend, 0);
			if (dma_q->kthread)
				complete(&vid_dev->node[i]->thread_sema);
			pr_info("amlvideo2.%d resume ok\n", i);
		}
	}
	return 0;
}

static const struct dev_pm_ops meson_amlvideo2_pm_ops = {
	.suspend = amlvideo2_drv_suspend,
	.resume = amlvideo2_drv_resume,
	.freeze = amlvideo2_drv_suspend,
	.thaw = amlvideo2_drv_resume,
	.restore = amlvideo2_drv_resume,
};
#endif

static int amlvideo2_release_node(struct amlvideo2_device *vid_dev)
{
	int i = 0;
	struct video_device *vdev = NULL;
	int ret;

	for (i = 0; i < vid_dev->node_num; i++) {
		if (vid_dev->node[i]) {
			vdev = vid_dev->node[i]->vdev;
			video_device_release(vdev);
			vf_unreg_receiver(&vid_dev->node[i]->recv);
			kfree(vid_dev->node[i]->fh);
			kfree(vid_dev->node[i]->amlvideo2_pool_ready);
			vid_dev->node[i]->fh = NULL;
			ret = free_canvas(vid_dev->node[i]);
			if (ret < 0)
				pr_err("%s free canvas failed! line: %d\n",
				       __func__, __LINE__);
			kfree(vid_dev->node[i]);
			vid_dev->node[i] = NULL;
		}
		amlvideo2_node[i] = NULL;
	}

	return 0;
}

/* static struct resource memobj; */
static int amlvideo2_create_node(struct platform_device *pdev, int node_id)
{
	int ret = 0, j = 0;
	struct video_device *vdev = NULL;
	struct amlvideo2_node *vid_node = NULL;
	struct amlvideo2_fh *fh = NULL;
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *aml2_dev = container_of(v4l2_dev,
		struct amlvideo2_device,
		v4l2_dev);
	aml2_dev->node_num = pdev->num_resources;
	if (aml2_dev->node_num > MAX_SUB_DEV_NODE)
		aml2_dev->node_num = MAX_SUB_DEV_NODE;

	aml2_dev->node[node_id] = NULL;
	ret = -ENOMEM;

	vid_node = kzalloc(sizeof(*vid_node), GFP_KERNEL);
	if (!vid_node)
		return ret;
	fh = kzalloc(sizeof(*fh), GFP_KERNEL);
	if (!fh) {
		kfree(vid_node);
		return ret;
	}

	vid_node->amlvideo2_pool_ready = NULL;
	vid_node->amlvideo2_pool_size = 12;
	vid_node->amlvideo2_pool_ready =
		kmalloc((sizeof(struct vframe_s) *
		(vid_node->amlvideo2_pool_size)),
		GFP_KERNEL);
	if (!vid_node->amlvideo2_pool_ready) {
		pr_err("amlvideo2_pool_ready malloc failed\n");
		kfree(fh);
		kfree(vid_node);
		return ret;
	}
	if (vid_node->amlvideo2_pool_ready) {
		vfq_init(&vid_node->q_ready,
			 vid_node->amlvideo2_pool_size,
			 (struct vframe_s **)
			 &vid_node->amlvideo2_pool_ready[0]);
	}

	ret = amlvideo2_v4l2_queue_init(aml2_dev, &fh->vb2_q);
	if (ret < 0) {
		kfree(vid_node->amlvideo2_pool_ready);
		kfree(fh);
		kfree(vid_node);
		return ret;
	}
	/* init video dma queues */
	INIT_LIST_HEAD(&vid_node->vidq.head);

	/* initialize spin locks */
	spin_lock_init(&vid_node->vidq.buff_list_lock);
	init_waitqueue_head(&vid_node->vidq.wq);
	init_waitqueue_head(&vid_node->dump_info_src.dump_wq);
	init_waitqueue_head(&vid_node->dump_info_dst.dump_wq);

	mutex_init(&vid_node->mutex);
	mutex_init(&fh->mutex);
	mutex_init(&vid_node->dump_info_src.mutex);
	mutex_init(&vid_node->dump_info_dst.mutex);
	init_completion(&vid_node->plug_sema);
#ifdef CONFIG_PM
	init_completion(&vid_node->suspend_sema);
	init_completion(&vid_node->thread_sema);
#endif
	vdev = video_device_alloc();
	if (!vdev) {
		kfree(vid_node->amlvideo2_pool_ready);
		kfree(fh);
		kfree(vid_node);
		return ret;
	}
	*vdev = amlvideo2_template;
	vdev->dev_debug = debug;
	vdev->v4l2_dev = v4l2_dev;
	vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING
			| V4L2_CAP_READWRITE;
	vdev->vfl_dir = VFL_DIR_RX;
	vdev->queue = &fh->vb2_q;
	vdev->lock = &aml2_dev->lock;
	ret = video_register_device(vdev, VFL_TYPE_VIDEO, video_nr);
	if (ret < 0) {
		ret = -ENODEV;
		video_device_release(vdev);
		kfree(vid_node->amlvideo2_pool_ready);
		kfree(fh);
		kfree(vid_node);
		return ret;
	}

	vid_node->fh = fh;
	video_set_drvdata(vdev, vid_node);
	/* Set all controls to their default value. */
	for (j = 0; j < ARRAY_SIZE(amlvideo2_node_qctrl); j++) {
		vid_node->qctl_regs[j] =
		amlvideo2_node_qctrl[j].default_value;
	}

	vid_node->vdev = vdev;
	vid_node->vid = node_id;
	vid_node->users = 0;
	vid_node->aml2_dev = aml2_dev;
	vid_node->mem_form_dev = &pdev->dev;
	video_nr++;
#ifdef MULTI_NODE
	vf_receiver_init(&vid_node->recv,
			 (node_id == 0) ? DEVICE_NAME0 : DEVICE_NAME1,
			 &video_vf_receiver, (void *)vid_node);
#else
	vf_receiver_init(&vid_node->recv,
			 RECEIVER_NAME,
			 &video_vf_receiver, (void *)vid_node);
#endif
	vf_reg_receiver(&vid_node->recv);
	vf_provider_init(&vid_node->amlvideo2_vf_prov,
			 (node_id == 0) ? DEVICE_NAME0 : DEVICE_NAME1,
			 &amlvideo2_vf_provider,
			 (void *)vid_node);
	vid_node->pflag = false;
	vid_node->field_flag = false;
	vid_node->field_condition_flag = false;
	vid_node->ge2d_multi_process_flag = false;
#ifdef CONFIG_PM
	atomic_set(&vid_node->is_suspend, 0);
	vid_node->could_suspend = true;
#endif
	vid_node->r_type = AML_RECEIVER_NONE;
	vid_node->aml2_canvas[0] = -1;
	vid_node->aml2_canvas[1] = -1;
	vid_node->aml2_canvas[2] = -1;
	aml2_dev->node[node_id] = vid_node;
	amlvideo2_node[node_id] = vid_node;

	if (ret)
		amlvideo2_release_node(aml2_dev);

	return ret;
}

static ssize_t dump_src_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct amlvideo2_device *aml2_dev = container_of(v4l2_dev,
			struct amlvideo2_device, v4l2_dev);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct dump_info_s *info = &node->dump_info_src;
	u32 sizeimage;
	u8 *vir_addr;
	size_t to_copy;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			pr_info("dump src: interrupted while waiting for mutex\n");
			return ret;
		}
		info->is_dump = 1;
		ret = wait_event_interruptible_timeout(info->dump_wq,
							info->config_dump_done,
							 msecs_to_jiffies(500000));
		if (ret == 0) {
			pr_info("dump src timeout\n");
		} else if (ret < 0) {
			pr_info("dump src: wait interrupted by signal, ret=%d\n", ret);
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return ret;
		}

		pr_info("src phy_addr:%llx width:%d height:%d sizeimage:%d\n",
			info->phy_addr, info->width, info->height, info->sizeimage);

		info->vir_addr = codec_mm_vmap(info->phy_addr, info->sizeimage);
		if (!info->vir_addr) {
			pr_info("vmap failed. vir_addr is null\n");
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return -EINVAL;
		}
	}

	sizeimage = info->sizeimage;
	vir_addr = info->vir_addr;
	if (!vir_addr || off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}
	to_copy = min_t(size_t, count, sizeimage - off);
	memcpy(buf, vir_addr + off, to_copy);
	if ((off + to_copy) >= sizeimage) {
		codec_mm_unmap_phyaddr(info->vir_addr);
		info->vir_addr = NULL;
		info->is_dump = 0;
		info->config_dump_done = 0;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
}

static ssize_t dump_dst_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct v4l2_device *v4l2_dev = dev_get_drvdata(dev);
	struct amlvideo2_device *aml2_dev = container_of(v4l2_dev,
			struct amlvideo2_device, v4l2_dev);
	struct amlvideo2_node *node = aml2_dev->node[aml2_dev->node_id];
	struct dump_info_s *info = &node->dump_info_dst;
	u32 sizeimage;
	u8 *vir_addr;
	size_t to_copy;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			pr_info("dump dst: interrupted while waiting for mutex\n");
			return ret;
		}
		info->is_dump = 1;
		ret = wait_event_interruptible_timeout(info->dump_wq,
							info->config_dump_done,
							 msecs_to_jiffies(500000));
		if (ret == 0) {
			pr_info("dump dst timeout\n");
		} else if (ret < 0) {
			pr_info("dump dst: wait interrupted by signal, ret=%d\n", ret);
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return ret;
		}

		pr_info("dst phy_addr:%llx width:%d height:%d sizeimage:%d\n",
			info->phy_addr, info->width, info->height, info->sizeimage);

		info->vir_addr = codec_mm_vmap(info->phy_addr, info->sizeimage);
		if (!info->vir_addr) {
			pr_info("vmap failed. vir_addr is null\n");
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return -EINVAL;
		}
	}

	sizeimage = info->sizeimage;
	vir_addr = info->vir_addr;
	if (!vir_addr || off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}
	to_copy = min_t(size_t, count, sizeimage - off);
	memcpy(buf, vir_addr + off, to_copy);
	if ((off + to_copy) >= sizeimage) {
		codec_mm_unmap_phyaddr(info->vir_addr);
		info->vir_addr = NULL;
		info->is_dump = 0;
		info->config_dump_done = 0;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
}

static struct bin_attribute amlvideo2_attr[] = {
	{
		.attr.name = "dump_src",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_src_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_dst",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_dst_show,
		.write = NULL,
	},
};

static struct bin_attribute *amlvideo2_bin_attrs[] = {
	&amlvideo2_attr[0],
	&amlvideo2_attr[1],
	NULL,
};

static const struct attribute_group amlvideo2_attr_group[AMLVIDEO2_MAX_INSTANCE] = {
	{
		.name = amlvideo2_group_name,
		.bin_attrs = amlvideo2_bin_attrs,
	},
};

static int amlvideo2_driver_probe(struct platform_device *pdev)
{
	s32 ret, i;
	struct amlvideo2_device *aml2_dev = NULL;

	aml2_dev = kzalloc(sizeof(*aml2_dev), GFP_KERNEL);

	if (!aml2_dev)
		return -ENOMEM;

	memset(aml2_dev, 0, sizeof(struct amlvideo2_device));
	snprintf(aml2_dev->v4l2_dev.name, sizeof(aml2_dev->v4l2_dev.name), "%s",
		 AVMLVIDEO2_MODULE_NAME);

	pdev->num_resources = MAX_SUB_DEV_NODE;
	platform_set_drvdata(pdev, &aml2_dev->v4l2_dev);
	ret = of_reserved_mem_device_init(&pdev->dev);

	if (ret == 0)
		pr_info("amlvideo2_probe done\n");

	ret = of_property_read_u32(pdev->dev.of_node,
				   "cma_mode", &aml2_dev->cma_mode);
	if (ret) {
		pr_err("don't find  match cma_mode\n");
		aml2_dev->cma_mode = 1;
	}
	if (!aml2_dev->cma_mode)
		aml2_dev->use_reserve = 1;
	ret = of_property_read_u32(pdev->dev.of_node,
				   "amlvideo2_id", &aml2_dev->node_id);
	if (ret)
		pr_err("don't find amlvideo2 node id.\n");

	ret = of_property_read_u32(pdev->dev.of_node,
				   "support_4k", &aml2_dev->support_4k_capture);
	if (ret) {
		pr_debug("no match support_4k for amlvideo2.%d, disable as default\n",
			aml2_dev->node_id);
		aml2_dev->support_4k_capture = 0;
	} else {
		pr_debug("support_4k %d for amlvideo2.%d\n",
			aml2_dev->support_4k_capture, aml2_dev->node_id);
	}
	ret = of_property_read_u32(pdev->dev.of_node,
				   "vpu_dma_mask", &aml2_dev->vpu_dma_mask);
	if (ret) {
		pr_debug("amlvideo2 don't find vpu_dma_mask\n");
		aml2_dev->vpu_dma_mask = 0;
	}
	if (aml2_dev->vpu_dma_mask) {
		ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(36));
		if (ret < 0)
			pr_err("dma_set_coherent_mask fail\n");
		ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
		if (ret < 0)
			pr_err("dma_set_mask fail\n");
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				   "codec_mm_alloc", &aml2_dev->codec_mm_alloc);
	if (ret)
		pr_err("don't find codec_mm_alloc, use default parm.\n");

	for (i = 0; i < AMLVIDEO2_MAX_INSTANCE; i++) {
		ret = sysfs_create_group(&pdev->dev.kobj, &amlvideo2_attr_group[i]);
		if (ret)
			pr_err("amlvideo2 creat dump node fail.\n");
	}

	if (aml2_dev->codec_mm_alloc == 1) {
		aml2_dev->framebuffer_total_size = CMA_ALLOC_SIZE_720P_NV21;
	} else if (aml2_dev->codec_mm_alloc == 2) {
		aml2_dev->framebuffer_total_size = CMA_ALLOC_SIZE_1080P_NV21;
	} else if (aml2_dev->codec_mm_alloc == 3 || aml2_dev->support_4k_capture) {
		aml2_dev->framebuffer_total_size = CMA_ALLOC_SIZE_4K_NV21;
		aml2_dev->support_4k_capture = 1;
	} else if (aml2_dev->codec_mm_alloc == 4) {
		aml2_dev->framebuffer_total_size = CMA_ALLOC_SIZE_1080P_RGBA;
	} else {
		aml2_dev->framebuffer_total_size = CMA_ALLOC_SIZE;
	}

	aml2_dev->pdev = pdev;

	if (v4l2_device_register(&pdev->dev, &aml2_dev->v4l2_dev) < 0) {
		dev_err(&pdev->dev, "v4l2_device_register failed\n");
		ret = -ENODEV;
		goto probe_err0;
	}

	mutex_init(&aml2_dev->q_lock);
	mutex_init(&aml2_dev->lock);

	video_nr = 11;

	ret = amlvideo2_create_node(pdev, aml2_dev->node_id);
	if (ret)
		goto probe_err1;

	/* register vout client */
	if (aml2_dev->node_id == 1)
		vout_register_client(&amlvideo2_notifier_nb);
	return 0;

probe_err1: v4l2_device_unregister(&aml2_dev->v4l2_dev);

probe_err0: kfree(aml2_dev);
	return ret;
}

static int amlvideo2_mem_device_init(struct reserved_mem *rmem,
				     struct device *dev)
{
	struct platform_device *pdev = container_of(dev,
			struct platform_device, dev);
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vdevp = container_of(v4l2_dev,
			struct amlvideo2_device, v4l2_dev);

	vdevp->use_reserve = 1;
	return 0;
}

static const struct reserved_mem_ops rmem_amlvideo2_ops = {
.device_init = amlvideo2_mem_device_init, };

static int __init amlvideo2_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_amlvideo2_ops;
	pr_info("amlvideo2 share mem setup\n");

	return 0;
}

static void amlvideo2_drv_remove(struct platform_device *pdev)
{
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct amlvideo2_device *vid_dev = container_of(v4l2_dev,
							struct amlvideo2_device,
							v4l2_dev);
	/* unregister vout client */
	video_nr = 11;
	if (vid_dev->node_id == 1)
		vout_unregister_client(&amlvideo2_notifier_nb);
	amlvideo2_release_node(vid_dev);
	v4l2_device_unregister(v4l2_dev);
	platform_set_drvdata(pdev, NULL);
	kfree(vid_dev);
}

static const struct of_device_id amlvideo2_dt_match[] = {
	{.compatible = "amlogic, amlvideo2",},
	{},
};

/* general interface for a linux driver .*/
static struct platform_driver amlvideo2_drv = {
	.probe = amlvideo2_driver_probe,
	.remove = amlvideo2_drv_remove,
	.driver = {
		.name = "amlvideo2",
		.owner = THIS_MODULE,
		.of_match_table = amlvideo2_dt_match,
#ifdef CONFIG_PM
		.pm = &meson_amlvideo2_pm_ops,
#endif
	}
};

int __init amlvideo2_init(void)
{
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2_init\n"); */
	if (platform_driver_register(&amlvideo2_drv)) {
		pr_err("Failed to register amlvideo2 driver\n");
		return -ENODEV;
	}

	return 0;
}

void __exit amlvideo2_exit(void)
{
	platform_driver_unregister(&amlvideo2_drv);
	/* amlog_level(LOG_LEVEL_HIGH,"amlvideo2 module removed.\n"); */
}

RESERVEDMEM_OF_DECLARE(amlvideo2, "amlogic, amlvideo2_memory",
		       amlvideo2_mem_setup);

