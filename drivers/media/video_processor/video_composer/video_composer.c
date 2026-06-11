// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/aml_sync_api.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <../../video_sink/video_priv.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include <linux/dma-buf.h>
#include <linux/amlogic/ion.h>
#include "video_composer.h"
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/resource_mgr/resourcemanage.h>
#include "../../gdc/inc/api/gdc_api.h"
#include "../common/video_pp_common.h"
#include "../../common/uvm_process/meson_uvm_lcevc_processor.h"
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>
#endif
#include <linux/amlogic/media/di/dpss_interface.h>
#include "videodisplay.h"

#ifdef CONFIG_AMLOGIC_MEDIA_VRR
#include <linux/amlogic/media/vrr/vrr.h>
#endif
#define VIDEO_COMPOSER_VERSION "1.0"

#define VIDEO_COMPOSER_NAME_SIZE 32

#define VIDEO_COMPOSER_DEVICE_NAME   "video_composer-dev"

#define WAIT_THREAD_STOPPED_TIMEOUT 20

#define WAIT_READY_Q_TIMEOUT 100

#define ADDR_VALUE_8G    (0x200000000ULL)

#define INORM   50000

u32 use_low_latency;
MODULE_PARM_DESC(use_low_latency, "\n use_low_latency\n");
module_param(use_low_latency, uint, 0664);

static u32 video_composer_instance_num;
static unsigned int force_composer;
static unsigned int force_composer_pip;
static int transform = -1;
static unsigned int vidc_debug;
static unsigned int vidc_pattern_debug;
static int last_index[MAX_VD_LAYERS][MAX_COMPOSER_COUNT];
static int last_frame_index;
static u32 print_flag;
static u32 full_axis = 1;
static u32 print_close;
static u32 receive_wait = 15;
static u32 margin_time = 2000;
static u32 max_width = 2560;
static u32 max_height = 1440;
static u32 pic_mode_max_width = 3840;
static u32 pic_mode_max_height = 2160;
static u32 rotate_width = 1280;
static u32 rotate_height = 720;
static u32 dewarp_rotate_width = 3840;
static u32 dewarp_rotate_height = 2160;
static u32 vicp_max_width = 3840;
static u32 vicp_max_height = 2160;
static u32 close_black;
static u32 debug_axis_pip;
static u32 debug_crop_pip;
static u32 composer_use_444;
static u32 reset_drop;
static u32 drop_cnt;
static u32 drop_cnt_pip;
static u32 receive_count;
static u32 receive_count_pip;
static u32 receive_new_count;
static u32 receive_new_count_pip;
static u32 total_get_count;
static u32 total_put_count;
static u64 nn_need_time = 15000;
static u64 nn_margin_time = 9000;
static u32 nn_bypass;
static u32 tv_fence_creat_count;
static u32 dump_vframe;
static u32 vicp_output_dev = 2; /*1 mif, 2 fbc, 3 fbc+mif*/
static u32 vicp_shrink_mode = 1; /*0 2x, 1 4x, 2 8x*/
static u32 force_comp_w;
static u32 force_comp_h;
static u32 lossy_compress_rate;//0: 100% copress; 1: 67% compress; 2: 83% compress
static u32 enable_frc_pattern;
static enum vc_fence_status last_buffer_status;
static struct vframe_s *last_normal_vf;
static struct composer_dev *dev_array[MAX_VD_LAYERS];

u32 vd_pulldown_level = 2;
u32 vd_max_hold_count = 300;
u32 vd_set_frame_delay[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
u32 vd_dump_vframe;
u32 vpp_drop_count;
static u32 composer_dev_choice; /*1 ge2d, 2 dewarp, 3 vicp*/
struct vframe_s *current_display_vf;
u32 vd_test_fps[MAX_VD_LAYERS];
u64 vd_test_fps_val[MAX_VD_LAYERS] = {1, 1, 1};
u64 vd_test_vsync_val[MAX_VD_LAYERS] = {1, 1, 1};
u32 dewarp_load_flag; /*0 dynamic load, 1 load bin file*/
u32 new_afr_pulldown;
u32 next_vsync_wakeup_vpp_to_get;
u32 allow_vpp_to_get;
u64 next_isr_spec_time;

#define to_dst_buf(vf)	\
	container_of(vf, struct dst_buf_t, frame)

#define IS_DI_PRELINK_BYPASS(di_flag) ((di_flag) & DI_FLAG_DI_PVPPLINK_BYPASS)

static void vd_dump_afbc_vf(u8 *data_y, u8 *data_uv, struct vframe_s *vf, int flag)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	loff_t pos;

	if (!vf)
		return;

	/*use flag to distinguish src and dst vframe*/
	if (flag == 0)
		snprintf(name_buf, sizeof(name_buf), "/sdcard/src_afbc_vframe.yuv");
	else
		snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_afbc_vframe.yuv");

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->compWidth * vf->compHeight;
	data_size_uv = vf->compWidth * vf->compHeight / 2;
	pr_info("dump: data_size_y =%d, data_size_uv=%d\n", data_size_y, data_size_uv);

	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	filp_close(fp, NULL);
#endif
}

static void vd_dump_vf(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	loff_t pos;

	if (!vf)
		return;

	snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_vframe.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height;
	data_size_uv = vf->canvas0_config[1].width *
			vf->canvas0_config[1].height;
	data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

int vd_vframe_afbc_soft_decode(struct vframe_s *vf, int flag)
{
	int ret, i, j, y_size, free_cnt;
	short *planes[4];
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	int bit_10;
	struct timeval start, end;
	unsigned long time_use = 0;
	struct fbc_decoder_param param;

	if ((vf->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	u32 p_data_size = vf->compWidth * vf->compHeight  * 3 / 2;
	u8 *p = vmalloc(p_data_size);

	if (!p)
		return -1;

	y_size = vf->compWidth * vf->compHeight * sizeof(short);
	pr_info("width: %d, height: %d, compWidth: %u, compHeight: %u.\n",
		 vf->width, vf->height, vf->compWidth, vf->compHeight);
	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		if (!planes[i]) {
			free_cnt = i;
			pr_err("vmalloc fail in %s\n", __func__);
			vfree(p);
			goto free;
		}
		pr_info("plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}
	free_cnt = 4;

	do_gettimeofday(&start);
	param.compHeadAddr = vf->compHeadAddr;
	param.compWidth = vf->compWidth;
	param.compHeight = vf->compHeight;
	param.bitdepth = vf->bitdepth;
#ifdef CONFIG_AMLOGIC_UVM_CORE
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, &param, 0, 0);
#else
	ret = -1;
#endif
	if (ret < 0) {
		pr_err("amlogic_fbc_lib.ko error %d", ret);
		vfree(p);
		goto free;
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;

	do_gettimeofday(&start);
	for (i = 0; i < vf->compHeight; i++) {
		for (j = 0; j < vf->compWidth; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst + j) = *s2c >> 2;
			else
				*(y_dst + j) = tmp[0];
		}

			y_dst += vf->compWidth;
			y_src += vf->compWidth;
	}

	for (i = 0; i < (vf->compHeight / 2); i++) {
		for (j = 0; j < vf->compWidth; j += 2) {
			s2c = v_src + j / 2;
			s2c1 = u_src + j / 2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst + j) = *s2c >> 2;
				*(vu_dst + j + 1) = *s2c1 >> 2;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
		}
		vu_dst += vf->compWidth;
		u_src += (vf->compWidth / 2);
		v_src += (vf->compWidth / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("bitblk time: %ldms\n", time_use);

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;
	vd_dump_afbc_vf(y_dst, vu_dst, vf, flag);
	vfree(p);
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return 0;

free:
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return -1;
}

void ext_controls(void)
{
	if (current_display_vf->type & VIDTYPE_COMPRESS) {
		vd_vframe_afbc_soft_decode(current_display_vf, 1);
	} else {
		vd_dump_vf(current_display_vf);
	}
}

int vc_print(int index, int debug_flag, const char *fmt, ...)
{
	if (index + 1 == print_close)
		return 0;

	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "vc:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static DEFINE_MUTEX(video_composer_mutex);

struct video_composer_port_s ports[] = {
	{
		.name = "video_composer.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "video_composer.1",
		.index = 1,
		.open_count = 0,
	},
	{
		.name = "video_composer.2",
		.index = 2,
		.open_count = 0,
	},
};

struct video_composer_port_s *video_composer_get_port(u32 index)
{
	int i = 0;

	if (index >= video_composer_instance_num) {
		vc_print(index, PRINT_ERROR,
			"%s: invalid index.\n",
			__func__);
		return NULL;
	}

	for (i = 0; i < video_composer_instance_num; i++) {
		if (index == ports[i].index)
			break;
	}

	if (i == video_composer_instance_num) {
		vc_print(index, PRINT_ERROR,
			"%s: don't find port[%d].\n",
			__func__, index);
		return NULL;
	} else {
		return &ports[i];
	}
}

#ifndef CONFIG_AMLOGIC_UVM_CORE
int dmabuf_put_vframe(struct dma_buf *dmabuf)
{
	return 0;
}

bool is_valid_mod_type(struct dma_buf *dmabuf,
		       enum uvm_hook_mod_type type)
{
	return false;
}

int uvm_put_hook_mod(struct dma_buf *dmabuf, int type)
{
	return 0;
}

struct uvm_hook_mod *uvm_get_hook_mod(struct dma_buf *dmabuf,
				      int type)
{
	return NULL;
}
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_GE2D
struct ge2d_context_s *create_ge2d_work_queue(void)
{
	return NULL;
}

int ge2d_context_config_ex(struct ge2d_context_s *context,
			   struct config_para_ex_s *ge2d_config)
{
	return -1;
}
#endif

void debug_vc_print_flag(const char *module, int debug_flags)
{
	print_flag = debug_flags;
}
EXPORT_SYMBOL(debug_vc_print_flag);

void debug_vc_transform(const char *module, int debug_flags)
{
	transform = debug_flags;
}
EXPORT_SYMBOL(debug_vc_transform);

void debug_vc_force_composer(const char *module, int debug_flags)
{
	force_composer = debug_flags;
}
EXPORT_SYMBOL(debug_vc_force_composer);

void debug_vc_get_count(const char *module, int debug_flags)
{
	if (debug_flags)
		pr_info("total_get_count: %d\n", total_get_count);
}
EXPORT_SYMBOL(debug_vc_get_count);

static void detect_vf_type(struct frame_info_t *frame_info, struct file *file_vf,
	bool *is_dec_vf_ptr, bool *is_v4l_vf_ptr)
{
	if (!is_dec_vf_ptr || !is_v4l_vf_ptr) {
		pr_info("is_dec_vf or is_v4l_vf err.\n");
		return;
	}
	if (frame_info->source_type == SOURCE_HWC_CREAT_ION) {
		*is_dec_vf_ptr = false;
		*is_v4l_vf_ptr = false;
		return;
	}
	*is_dec_vf_ptr = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	*is_v4l_vf_ptr = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);
}

static void *video_timeline_create(struct composer_dev *dev)
{
	const char *tl_name = "videocomposer_timeline_0";

	if (dev->index == 0)
		tl_name = "videocomposer_timeline_0";
	else if (dev->index == 1)
		tl_name = "videocomposer_timeline_1";
	else if (dev->index == 2)
		tl_name = "videocomposer_timeline_2";

	if (IS_ERR_OR_NULL(dev->video_timeline)) {
		dev->cur_streamline_val = 0;
		dev->video_timeline = aml_sync_create_timeline(tl_name);
		vc_print(dev->index, PRINT_FENCE,
			 "timeline create tlName =%s, video_timeline=%p\n",
			 tl_name, dev->video_timeline);
	}

	return dev->video_timeline;
}

static int video_timeline_create_fence(struct composer_dev *dev)
{
	int out_fence_fd = -1;
	u32 pt_val = 0;

	pt_val = dev->cur_streamline_val + 1;
	vc_print(dev->index, PRINT_FENCE, "pt_val %d", pt_val);

	out_fence_fd = aml_sync_create_fence(dev->video_timeline, pt_val);
	if (out_fence_fd >= 0) {
		dev->cur_streamline_val++;
		dev->fence_creat_count++;
	} else {
		vc_print(dev->index, PRINT_ERROR,
			 "create fence returned %d", out_fence_fd);
	}
	return out_fence_fd;
}

static void video_timeline_increase(struct composer_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(dev->video_timeline, value);
	dev->fence_release_count += value;
	vc_print(dev->index, PRINT_FENCE,
		"receive_cnt=%lld,new_cnt=%lld,fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		dev->received_count,
		dev->received_new_count,
		dev->fence_creat_count,
		dev->fence_release_count);
}

static void video_timeline_update(struct composer_dev *dev, struct vframe_s *vf)
{
	int normal_frame_count = 0;
	struct vframe_s *new_display_vf;
	enum vc_fence_status buffer_status;
	bool rendered;
	int repeat_count;

	if (!vf)
		return;

	rendered = vf->rendered;
	repeat_count = vf->repeat_count;

	switch (vf->dec_fence_status) {
	case DEC_FENCE_SUCCESS:
		vc_print(dev->index, PRINT_FENCE,
			"%s: dec fence success, ready to put dec fence:%px\n",
			__func__, vf->fence);
		dma_fence_put(vf->fence);
		if (last_buffer_status == VC_FENCE_DEC_ERR)
			buffer_status = VC_FENCE_RELEASED;
		else if (rendered)
			buffer_status = VC_FENCE_NORMAL;
		else
			buffer_status = VC_FENCE_WAIT;
		break;
	case DEC_FENCE_INVALID:
		if (rendered)
			buffer_status = VC_FENCE_NORMAL;
		else
			buffer_status = VC_FENCE_WAIT;
		break;
	case DEC_FENCE_ERR:
		buffer_status = VC_FENCE_DEC_ERR;
		break;
	default:
		buffer_status = VC_FENCE_INVALID;
		break;
	}

	switch (buffer_status) {
	case VC_FENCE_DEC_ERR:
		//release the normal vf when the first error vf appears
		if (last_normal_vf == current_display_vf) {
			new_display_vf = NULL;
		} else {
			last_normal_vf = current_display_vf;
			new_display_vf = last_normal_vf;
		}
		if (new_display_vf && new_display_vf->dec_fence_status == DEC_FENCE_SUCCESS) {
			normal_frame_count = 1 + new_display_vf->repeat_count;
			vc_print(dev->index, PRINT_OTHER,
				"err vf, need drop frame_index:%d, count:%d",
				new_display_vf->frame_index, normal_frame_count);
		}
		vc_print(dev->index, PRINT_OTHER,
			"put: frame_index:%d error, fence released\n",
			vf->frame_index);
		video_timeline_increase(dev, repeat_count + normal_frame_count +
			1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
		break;
	case VC_FENCE_RELEASED:
		vc_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: frame_index: %d, err frame already put fence\n", vf->frame_index);
		break;
	case VC_FENCE_NORMAL:
		video_timeline_increase(dev, repeat_count
					+ 1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
		break;
	case VC_FENCE_WAIT:
		dev->drop_frame_count += repeat_count + 1;
		vc_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: drop repeat_count=%d\n", repeat_count);
		break;
	default:
		vc_print(dev->index, PRINT_ERROR, "error, fence status unknown\n");
		break;
	}
	last_buffer_status = buffer_status;
}

static int vc_init_ge2d_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;

	buf_width = (dev->vinfo_w + 0x1f) & ~0x1f;
	buf_height = dev->vinfo_h;

	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, dev->output_duration);
	if (dev->output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %ld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = rotate_width;
			buf_height = rotate_height;
		}

		if (buf_width > max_width)
			buf_width = max_width;
		if (buf_height > max_height)
			buf_height = max_height;
	}

	if (composer_use_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			dev->dst_buf[i].phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR,
			 "%s: cma memory is %lx , size is  %x\n",
			 ports[dev->index].name,
			 dev->dst_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->dst_buf[i].phy_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}
		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;
		dev->dst_buf[i].buf_used = USED_BY_GE2D;

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
		}
	return 0;
}

static int vc_init_dewarp_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;

	buf_width = (dev->vinfo_w + 0x1f) & ~0x1f;
	buf_height = dev->vinfo_h;

	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, dev->output_duration);
	if (dev->output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %ld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = dewarp_rotate_width;
			buf_height = dewarp_rotate_height;
		}
	}

	if (composer_use_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			dev->dst_buf[i].phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR,
			 "%s: cma memory is %lx , size is  %x\n",
			 ports[dev->index].name,
			 (unsigned long)dev->dst_buf[i].phy_addr,
			 (unsigned int)buf_size);

		if (dev->dst_buf[i].phy_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}
		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;
		dev->dst_buf[i].buf_used = USED_BY_DEWARP;

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}
	return 0;
}
static int vc_init_vicp_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int i, j, flags;
	u32 buf_addr = 0;
	u32 buf_width, buf_height, buf_size;
	int dw_size = 0, afbc_body_size = 0, afbc_head_size = 0, afbc_table_size = 0;
	u32 *virt_addr = NULL, *temp_addr = NULL;
	u32 temp_body_addr;
	ulong buf_handle, buf_phy_addr;

	buf_width = (dev->vinfo_w + 0x1f) & ~0x1f;
	buf_height = dev->vinfo_h;

	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, dev->output_duration);
	if (dev->output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %ld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->need_rotate) {
			buf_width = rotate_width;
			buf_height = rotate_height;
		}

		if (buf_width > vicp_max_width)
			buf_width = vicp_max_width;
		if (buf_height > vicp_max_height)
			buf_height = vicp_max_height;
	}

	if (vicp_output_dev == 1) {
		buf_size = buf_width * buf_height * 3;
		if (composer_use_444 == 0)
			buf_size = buf_size * 3 / 2;
	} else {
		if (vicp_output_dev == 3)//mif
			dw_size = roundup(buf_width >> 2, 32) * roundup(buf_height >> 2, 2);

		afbc_body_size = buf_width * buf_height + (1024 * 1658);
		if (composer_use_444 == 0) {
			dw_size = dw_size * 3 / 2;
			afbc_body_size = afbc_body_size * 3 / 2;
		}

		dw_size = PAGE_ALIGN(dw_size);
		afbc_body_size = roundup(PAGE_ALIGN(afbc_body_size), PAGE_SIZE);
		afbc_head_size = (roundup(buf_width, 64) * roundup(buf_height, 64)) / 32;
		afbc_head_size = PAGE_ALIGN(afbc_head_size);
		afbc_table_size = PAGE_ALIGN((afbc_body_size * 4) / PAGE_SIZE);
		buf_size = dw_size + afbc_body_size + afbc_head_size;
	}

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0)
			buf_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);

		if (buf_addr == 0) {
			dev->buffer_status = INIT_ERROR;
			vc_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}

		dev->dst_buf[i].phy_addr = buf_addr;
		vc_print(dev->index, PRINT_ERROR,
			"%s: cma memory is 0x%lx , size is 0x%x.\n",
			ports[dev->index].name, dev->dst_buf[i].phy_addr, buf_size);

		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;

		if (vicp_output_dev != 1) {
			dev->dst_buf[i].dw_size = dw_size;
			dev->dst_buf[i].afbc_body_addr = dev->dst_buf[i].phy_addr + dw_size;
			dev->dst_buf[i].afbc_body_size = afbc_body_size;
			dev->dst_buf[i].afbc_head_addr = dev->dst_buf[i].afbc_body_addr +
				afbc_body_size;
			dev->dst_buf[i].afbc_head_size = afbc_head_size;

			virt_addr = (u32 *)codec_mm_dma_alloc_coherent(&buf_handle, &buf_phy_addr,
				afbc_table_size, dev->port->name);
			dev->dst_buf[i].afbc_table_handle = buf_handle;
			dev->dst_buf[i].afbc_table_addr = buf_phy_addr;
			dev->dst_buf[i].afbc_table_size = afbc_table_size;
			if (dev->dst_buf[i].afbc_table_addr == 0) {
				dev->buffer_status = INIT_ERROR;
				vc_print(dev->index, PRINT_ERROR, "alloc table buf fail.\n");
				return -1;
			}
			temp_body_addr = dev->dst_buf[i].afbc_body_addr & 0xffffffff;
			memset(virt_addr, 0, afbc_table_size);
			temp_addr = virt_addr;
			for (j = 0; j < afbc_body_size; j += 4096) {
				*virt_addr = ((j + temp_body_addr) >> 12) & 0x000fffff;
				virt_addr++;
			}

			vc_print(dev->index, PRINT_VICP, "dw_size = %d.\n", dw_size);
			vc_print(dev->index, PRINT_VICP, "HeadAddr = 0x%lx, headsize = %d.\n",
				dev->dst_buf[i].afbc_head_addr, afbc_head_size);
			vc_print(dev->index, PRINT_VICP, "BodyAddr = 0x%lx, bodysize = %d.\n",
				dev->dst_buf[i].afbc_body_addr, afbc_body_size);
			vc_print(dev->index, PRINT_VICP, "tableAddr = 0x%lx, tablesize = %d.\n",
				dev->dst_buf[i].afbc_table_addr, afbc_table_size);
		}

		if (!kfifo_put(&dev->free_q, &dev->dst_buf[i].frame))
			vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}

	return 0;
}

static int video_composer_init_buffer(struct composer_dev *dev, bool is_tvp, size_t usage)
{
	int ret = 0;

	switch (dev->buffer_status) {
	case UNINITIAL:/*not config*/
		break;
	case INIT_SUCCESS:/*config before , return ok*/
		return 0;
	case INIT_ERROR:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}

	if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		ret = vc_init_dewarp_buffer(dev, is_tvp, usage);
		if (IS_ERR_OR_NULL(dev->dewarp_para.context))
			ret |= init_dewarp_composer(&dev->dewarp_para);
	} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
		ret = vc_init_vicp_buffer(dev, is_tvp, usage);
	} else if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		ret = vc_init_ge2d_buffer(dev, is_tvp, usage);
		if (IS_ERR_OR_NULL(dev->ge2d_para.context))
			ret |= init_ge2d_composer(&dev->ge2d_para);
	} else {
		vc_print(dev->index, PRINT_ERROR, "composer device choice error!\n");
		ret = -1;
	}

	if (ret < 0) {
		vc_print(dev->index, PRINT_ERROR, "config vc buf failed!\n");
		return -1;
	} else {
		dev->buffer_status = INIT_SUCCESS;
		return 0;
	}
}

static void video_composer_uninit_buffer(struct composer_dev *dev)
{
	int i;
	int ret = 0;

	if (dev->buffer_status == UNINITIAL) {
		vc_print(dev->index, PRINT_OTHER,
			 "%s buffer have uninit already finished!\n", __func__);
		return;
	}

	if (!IS_ERR_OR_NULL(dev->dewarp_para.context)) {
		ret = uninit_dewarp_composer(&dev->dewarp_para);
		if (ret < 0)
			vc_print(dev->index, PRINT_ERROR, "uninit dewarp composer fail!\n");
		dev->dewarp_para.context = NULL;
	}

	if (!IS_ERR_OR_NULL(dev->ge2d_para.context)) {
		ret = uninit_ge2d_composer(&dev->ge2d_para);
		if (ret < 0)
			vc_print(dev->index, PRINT_ERROR, "uninit ge2d composer failed!\n");
		dev->ge2d_para.context = NULL;
	}

	dev->buffer_status = UNINITIAL;
	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr != 0) {
			pr_info("%s: cma free addr is %lx\n",
				ports[dev->index].name,
				(unsigned long)dev->dst_buf[i].phy_addr);
			codec_mm_free_for_dma(ports[dev->index].name,
					      dev->dst_buf[i].phy_addr);
			dev->dst_buf[i].phy_addr = 0;
			if (vicp_output_dev != 1 && dev->dev_choice == COMPOSER_WITH_VICP) {
				codec_mm_dma_free_coherent(dev->dst_buf[i].afbc_table_handle);
				dev->dst_buf[i].afbc_table_addr = 0;
			}
		}
	}

	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->last_dst_vf = NULL;

	INIT_KFIFO(dev->free_q);
	kfifo_reset(&dev->free_q);
}

static struct file_private_data *vc_get_file_private(struct composer_dev *dev,
						      struct file *file_vf)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
	bool is_v4lvideo_fd = false;
#endif

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "get_file_private_data fail\n");
		return NULL;
	}
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
	if (is_v4lvideo_buf_file(file_vf))
		is_v4lvideo_fd = true;

	if (is_v4lvideo_fd) {
		file_private_data =
			(struct file_private_data *)(file_vf->private_data);
		return file_private_data;
	}
#endif

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
	if (!uhmod) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}
	file_private_data = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 VF_PROCESS_V4LVIDEO);

	return file_private_data;
}

static struct vf_nn_sr_t *vc_get_hfout_data(struct composer_dev *dev,
						     struct file *file_vf)
{
	struct vf_nn_sr_t *srout_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get hfout data fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_NN);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "dma file file_private_data is NULL 1\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL 2\n");
		return NULL;
	}
	srout_data = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_NN);

	return srout_data;
}

static struct vf_aiface_t *vc_get_aiface_info(struct composer_dev *dev,
						struct file *file_vf, int frame_index)
{
	struct vf_aiface_t *aiface_info;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "NULL param: frame_index=%d.\n", frame_index);

		return NULL;
	}
	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_AIFACE);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "NULL uhmod: frame_index=%d\n", frame_index);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "invalid uhmod param:frame_index=%d\n",
			frame_index);
		return NULL;
	}
	aiface_info = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_AIFACE);

	return aiface_info;
}

static struct vf_aicolor_t *vc_get_aicolor_info(struct composer_dev *dev,
		struct file *file_vf)
{
	struct vf_aicolor_t *aicolor_info;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get aicolor_info fail.\n");
		return NULL;
	}
	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_AICOLOR);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "dma file file_private_data is NULL 1\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL 2\n");
		return NULL;
	}
	aicolor_info = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_AICOLOR);

	return aicolor_info;
}

static void frames_put_file(struct composer_dev *dev,
			    struct received_frames_t *current_frames)
{
	struct file *file_vf;
	int current_count;
	int i;

	current_count = current_frames->frames_info.frame_count;
	for (i = 0; i < current_count; i++) {
		file_vf = current_frames->file_vf[i];
		fput(file_vf);
		total_put_count++;
		dev->fput_count++;
	}
}

void vc_private_q_init(struct composer_dev *dev)
{
	int i;

	INIT_KFIFO(dev->vc_private_q);
	kfifo_reset(&dev->vc_private_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++) {
		dev->vc_private[i].index = i;
		dev->vc_private[i].flag = 0;
		dev->vc_private[i].srout_data = NULL;
		dev->vc_private[i].src_vf = NULL;
		dev->vc_private[i].vsync_index = 0;
		dev->vc_private[i].aicolor_info = NULL;
		if (!kfifo_put(&dev->vc_private_q, &dev->vc_private[i]))
			vc_print(dev->index, PRINT_ERROR,
				"q_init: vc_private_q is full!\n");
	}
}

void vc_private_q_recycle(struct composer_dev *dev,
	struct video_composer_private *vc_private)
{
	if (!vc_private)
		return;

	memset(vc_private, 0, sizeof(struct video_composer_private));

	if (!kfifo_put(&dev->vc_private_q, vc_private))
		vc_print(dev->index, PRINT_ERROR,
			"vc_private_q is full!\n");
}

struct video_composer_private *vc_private_q_pop(struct composer_dev *dev)
{
	struct video_composer_private *vc_private = NULL;

	if (!kfifo_get(&dev->vc_private_q, &vc_private)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: get vc_private_q failed\n");
		vc_private = NULL;
	} else {
		vc_private->flag = 0;
		vc_private->srout_data = NULL;
		vc_private->src_vf = NULL;
		vc_private->aicolor_info = NULL;
	}

	return vc_private;
}

static void display_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	int repeat_count;
	int i;
	bool is_mosaic_22 = false;
	struct file *file_vf;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit display_q len=%d\n",
		 kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			is_mosaic_22 = dis_vf->type_ext & VIDTYPE_EXT_MOSAIC_22;
			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS && !is_mosaic_22) {
				repeat_count = dis_vf->repeat_count;
				vc_print(dev->index, PRINT_FENCE,
					 "vc: unit repeat_count=%d, frame_index=%d\n",
					 repeat_count,
					 dis_vf->frame_index);
				if (dis_vf->fence &&
					dis_vf->dec_fence_status == DEC_FENCE_SUCCESS) {
					vc_print(dev->index, PRINT_FENCE,
						 "vc: unit put fence=%px\n",
						 dis_vf->fence);
					dma_fence_put(dis_vf->fence);
				}
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			} else if (is_mosaic_22) {
				for (i = 0; i < 4; i++) {
					if (!dis_vf->vc_private ||
						!dis_vf->vc_private->mosaic_vf[i])
						continue;
					file_vf = dis_vf->vc_private->mosaic_vf[i]->file_vf;
					if (file_vf) {
						fput(file_vf);
						total_put_count++;
						dev->fput_count++;
					} else {
						vc_print(dev->index, PRINT_ERROR,
							"%s error!!!: i=%d fput fail\n",
							__func__, i);
					}
				}
			} else if (!(dis_vf->flag
				     & VFRAME_FLAG_VIDEO_COMPOSER)) {
				vc_print(dev->index, PRINT_ERROR,
					 "vc: unit display_q flag is null, frame_index=%d\n",
					 dis_vf->frame_index);
			}
		}
	}
}

static void receive_q_uninit(struct composer_dev *dev)
{
	int i = 0;
	struct received_frames_t *received_frames = NULL;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit receive_q len=%d\n",
		 kfifo_len(&dev->receive_q));
	while (kfifo_len(&dev->receive_q) > 0) {
		if (kfifo_get(&dev->receive_q, &received_frames))
			frames_put_file(dev, received_frames);
	}

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++) {
		atomic_set(&dev->received_frames[i].on_use,
			   false);
	}
}

static void ready_q_uninit(struct composer_dev *dev)
{
	struct vframe_s *dis_vf = NULL;
	int repeat_count;
	int i;

	vc_print(dev->index, PRINT_QUEUE_STATUS, "vc: unit ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &dis_vf)) {
			if (!dis_vf) {
				vc_print(dev->index, PRINT_ERROR, "%s: dis_vf is NULL.\n",
					__func__);
				break;
			}

			if (dis_vf->vc_private)
				if (dis_vf->vc_private->srout_data) {
					if (dis_vf->vc_private->srout_data->nn_status == NN_DONE)
						dis_vf->vc_private->srout_data->nn_status =
							NN_DISPLAYED;
				}

			if (dis_vf->flag
			    & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				repeat_count = dis_vf->repeat_count;
				for (i = 0; i <= repeat_count; i++) {
					fput(dis_vf->file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			}
		}
	}
}

static void videocom_vf_put(struct vframe_s *vf, struct composer_dev *dev)
{
	struct dst_buf_t *dst_buf;

	if (IS_ERR_OR_NULL(vf)) {
		vc_print(dev->index, PRINT_ERROR, "vf is NULL\n");
		return;
	}

	dst_buf = to_dst_buf(vf);
	if (IS_ERR_OR_NULL(dst_buf)) {
		vc_print(dev->index, PRINT_ERROR, "dst_buf is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "dev is NULL\n");
		return;
	}

	if (!kfifo_put(&dev->free_q, vf))
		vc_print(dev->index, PRINT_ERROR, "put free_q is full\n");
	vc_print(dev->index, PRINT_OTHER,
		 "%s free buffer count: %d %d\n",
		 __func__, kfifo_len(&dev->free_q), __LINE__);

	if (kfifo_is_full(&dev->free_q)) {
		dev->need_free_buffer = true;
		vc_print(dev->index, PRINT_ERROR,
			 "free_q is full, could uninit buffer!\n");
	}
	vc_print(dev->index, PRINT_PATTERN, "put: vf=%p\n", vf);
	wake_up_interruptible(&dev->wq);
}

struct vframe_s *videocomposer_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval now_time;
	struct timeval nn_start_time;
	u64 nn_used_time;
	bool canbe_peek = true;
	u32 nn_status;
	u32 nn_mode;
	bool bypass_nn = false;

	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (!vf->vc_private) {
			vc_print(dev->index, PRINT_OTHER, "peek: vf->vc_private is NULL\n");
			return vf;
		}

		if (vf->vc_private->flag & VC_FLAG_AI_SR) {
			nn_status = vf->vc_private->srout_data->nn_status;
			nn_mode = vf->vc_private->srout_data->nn_mode;

			vc_print(dev->index, PRINT_NN,
				"peek:nn_status=%d, nn_index=%d, nn_mode=%d, PHY=%llx, nn out:%d*%d, hf:%d*%d,hf_align:%d*%d\n",
				vf->vc_private->srout_data->nn_status,
				vf->vc_private->srout_data->nn_index,
				vf->vc_private->srout_data->nn_mode,
				vf->vc_private->srout_data->nn_out_phy_addr,
				vf->vc_private->srout_data->nn_out_width,
				vf->vc_private->srout_data->nn_out_height,
				vf->vc_private->srout_data->hf_width,
				vf->vc_private->srout_data->hf_height,
				vf->vc_private->srout_data->hf_align_w,
				vf->vc_private->srout_data->hf_align_h);
			if (nn_status != NN_DONE) {
				if (nn_status == NN_INVALID) {
					vf->vc_private->flag &= ~VC_FLAG_AI_SR;
					vc_print(dev->index, PRINT_NN | PRINT_OTHER,
						"nn status is invalid, need bypass");
					return vf;
				} else if (nn_status == NN_WAIT_DOING) {
					vc_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: nn wait doing, nn_index =%d, frame_index=%d, nn_status=%d,srout_data=%px\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status,
						vf->vc_private->srout_data);
					return NULL;
				} else if (nn_status == NN_DISPLAYED) {
					vc_print(dev->index, PRINT_ERROR,
						"peek: nn_status err, nn_index =%d, frame_index=%d, nn_status=%d\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status);
					return vf;
				}

				if (!(vf->type_original & VIDTYPE_INTERLACE)) {
					if (!(dev->nn_mode_flag & 0x1) && nn_mode == 1) {
						dev->nn_mode_flag |= 0x1;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x2) && nn_mode == 2) {
						dev->nn_mode_flag |= 0x2;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x4) && nn_mode == 3) {
						dev->nn_mode_flag |= 0x4;
						bypass_nn = true;
					}
				} else {
					if (!(dev->nn_mode_flag & 0x100) && nn_mode == 1) {
						dev->nn_mode_flag |= 0x100;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x200) && nn_mode == 2) {
						dev->nn_mode_flag |= 0x200;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x400) && nn_mode == 3) {
						dev->nn_mode_flag |= 0x400;
						bypass_nn = true;
					}
				}

				if (bypass_nn) {
					vf->vc_private->flag &= ~VC_FLAG_AI_SR;
					vc_print(dev->index, PRINT_NN,
						"nn mode change, bypass first frame\n");
					return vf;
				}

				do_gettimeofday(&now_time);
				nn_start_time = vf->vc_private->srout_data->start_time;
				nn_used_time = (u64)1000000 *
					(now_time.tv_sec - nn_start_time.tv_sec)
					+ now_time.tv_usec - nn_start_time.tv_usec;

				if (nn_used_time < (nn_need_time - nn_margin_time))
					canbe_peek = false;
				vc_print(dev->index, PRINT_FENCE | PRINT_NN,
					"peek: nn not done, nn_index = %d, frame_index = %d, nn_status = %d, nn_used_time = %lld, canbe_peek = %d.\n",
					vf->vc_private->srout_data->nn_index,
					vf->frame_index,
					vf->vc_private->srout_data->nn_status,
					nn_used_time,
					canbe_peek);
				if (!canbe_peek) {
					vc_print(dev->index, PRINT_FENCE | PRINT_NN,
					"peek:fail: nn not done, nn_index =%d, frame_index=%d, nn_status=%d, nn_used_time=%lld canbe_peek=%d\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status,
						nn_used_time,
						canbe_peek);
					return NULL;
				}
			}
		}
		if (vf->vc_private->flag & VC_FLAG_AI_COLOR) {
			vc_print(dev->index, PRINT_NN,
				"peek: aicolor is enable\n");
			nn_status = vf->vc_private->aicolor_info->nn_status;
			vc_print(dev->index, PRINT_NN, "peek: aicolor_status=%d", nn_status);
			if (nn_status != NN_DONE) {
				if (nn_status == NN_INVALID) {
					vf->vc_private->flag &= ~VC_FLAG_AI_COLOR;
					vc_print(dev->index, PRINT_NN | PRINT_OTHER,
						"aicolor status is invalid, need bypass");
					return vf;
				} else if (nn_status == NN_WAIT_DOING) {
					vc_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: aicolor wait doing, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return NULL;
				} else if (nn_status == NN_START_DOING) {
					vc_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: aicolor start doing, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return NULL;
				} else if (nn_status == NN_DISPLAYED) {
					vc_print(dev->index, PRINT_ERROR,
						"peek: aicolor_status err, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return vf;
				}
			}
		}
		return vf;
	} else {
		return NULL;
	}
}

void videocomposer_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	int repeat_count;
	int frame_index;
	int index_disp;
	bool is_composer;
	bool is_mosaic_22;
	int i;
	struct file *file_vf[4] = {NULL};
	int src_frame_index[4] = {0};
	int dst_frame_index[4] = {0};
	struct vd_prepare_s *vd_prepare;

	if (!vf)
		return;

	repeat_count = vf->repeat_count;
	frame_index = vf->frame_index;
	index_disp = vf->index_disp;
	is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
	is_mosaic_22 = vf->type_ext & VIDTYPE_EXT_MOSAIC_22;

	if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
		vc_print(dev->index, PRINT_OTHER, "put: fake frame\n");
		return;
	}

	if (vf->vc_private && vf->vc_private->srout_data) {
		if (vf->vc_private->srout_data->nn_status == NN_DONE)
			vf->vc_private->srout_data->nn_status = NN_DISPLAYED;
	}
	if (vf->vc_private && vf->vc_private->aicolor_info) {
		if (vf->vc_private->aicolor_info->nn_status == NN_DONE)
			vf->vc_private->aicolor_info->nn_status = NN_DISPLAYED;
	}
	vc_print(dev->index, PRINT_FENCE,
		 "put: repeat_count =%d, frame_index=%d, index_disp=%x\n",
		 repeat_count, frame_index, index_disp);
	if (!is_composer && !is_mosaic_22) {
		if (vf->vc_private) {
			vc_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}

		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: prepare is NULL.\n",
				__func__);
			return;
		}

		if (IS_ERR_OR_NULL(vd_prepare->src_frame)) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: vd_prepare->src_frame is NULL.\n",
				__func__);
			return;
		}

		file_vf[0] = vd_prepare->src_frame->file_vf;
		src_frame_index[0] = vd_prepare->src_frame->frame_index;
		dst_frame_index[0] = vd_prepare->dst_frame.frame_index;
		vd_prepare_data_q_put(dev, vd_prepare);
		if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
			vc_print(dev->index, PRINT_OTHER, "put enhance vf:%px\n", vf->enhance_vf);
			if (!kfifo_put(&dev->free_q, vf->enhance_vf))
				vc_print(dev->index, PRINT_ERROR, "put free_q is full\n");
		}
	} else if (is_mosaic_22) {
		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		for (i = 0; i < 4; i++) {
			if (IS_ERR_OR_NULL(vf->vc_private)) {
				vc_print(dev->index, PRINT_ERROR, "put mosaic no private!!!\n");
				break;
			}

			if (IS_ERR_OR_NULL(vf->vc_private->mosaic_vf[i])) {
				vc_print(dev->index, PRINT_ERROR, "mosaic_vf is NULL.\n");
				break;
			}

			file_vf[i] = vf->vc_private->mosaic_vf[i]->file_vf;
			src_frame_index[i] = vf->vc_private->mosaic_src_vf[i]->frame_index,
			src_frame_index[i] = vf->vc_private->mosaic_dst_vf[i].frame_index;
		}

		if (vf->vc_private) {
			vc_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}

		vd_prepare_data_q_put(dev, vd_prepare);
	} else {
		if (vf->vc_private) {
			vc_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}
	}

	//all the vframe param used must beforce fence release
	video_timeline_update(dev, vf);

	if (!is_composer && !is_mosaic_22) {
		for (i = 0; i <= repeat_count; i++) {
			if (file_vf[0]) {
				fput(file_vf[0]);
				total_put_count++;
				dev->fput_count++;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					"%s error:src_index=%d,dst_index=%d.\n",
					__func__,
					src_frame_index[0],
					dst_frame_index[0]);
			}
		}

	} else if (is_mosaic_22) {
		for (i = 0; i < 4; i++) {
			if (file_vf[i]) {
				fput(file_vf[i]);
				total_put_count++;
				dev->fput_count++;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					"%s error: i=%d,src_index=%d,dst_index=%d.\n",
					__func__,
					i,
					src_frame_index[i],
					dst_frame_index[i]);
			}
		}
	} else {
		videocom_vf_put(vf, dev);
	}
}

static unsigned long get_dma_phy_addr(int fd, int index)
{
	unsigned long phy_addr = 0;
	struct dma_buf *dbuf = NULL;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct dma_buf_attachment *attach = NULL;

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf))
		vc_print(index, PRINT_ERROR, "dbuf got from fd error!!!\n");
	attach = dma_buf_attach(dbuf, ports[index].pdev);
	if (IS_ERR(attach))
		return 0;

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
	dma_buf_put(dbuf);
	return phy_addr;
}

static struct vframe_s *get_dst_vframe_buffer(struct composer_dev *dev)
{
	struct vframe_s *dst_vf;

	if (!kfifo_get(&dev->free_q, &dst_vf)) {
		vc_print(dev->index, PRINT_QUEUE_STATUS, "free q is empty\n");
		return NULL;
	}
	return dst_vf;
}

static u32 need_switch_buffer(struct dst_buf_t *buf, bool is_tvp, struct composer_dev *dev)
{
	int flags, ret;
	bool vicp_fbc_out_en = false;
	u32 *virt_addr = NULL;
	u32 temp_body_addr;
	u32 offset_body_addr;

	if (IS_ERR_OR_NULL(buf) || IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: is about %s buffer\n", __func__,
		is_tvp ? "none tvp" : "tvp");
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	if (vicp_output_dev != 1 && dev->dev_choice == COMPOSER_WITH_VICP)
		vicp_fbc_out_en = true;

	if (buf->phy_addr > 0) {
		vc_print(dev->index, PRINT_OTHER, "free buffer 0x%lx\n", buf->phy_addr);
		codec_mm_free_for_dma(ports[dev->index].name, buf->phy_addr);
	}

	buf->phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
					buf->buf_size / PAGE_SIZE, 0, flags);
	vc_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%lx\n", __func__, buf->phy_addr);

	if (vicp_fbc_out_en) {
		buf->afbc_body_addr = buf->phy_addr + buf->dw_size;
		buf->afbc_head_addr = buf->afbc_body_addr + buf->afbc_body_size;

		if (buf->afbc_table_addr > 0) {
			vc_print(dev->index, PRINT_OTHER, "%s: free table buffer 0x%lx\n", __func__,
				buf->afbc_table_addr);
			codec_mm_dma_free_coherent(buf->afbc_table_handle);
		}
		virt_addr = (u32 *)codec_mm_dma_alloc_coherent(&buf->afbc_table_handle,
			&buf->afbc_table_addr, buf->afbc_table_size, dev->port->name);
		temp_body_addr = buf->afbc_body_addr & 0xffffffff;
		memset(virt_addr, 0, buf->afbc_table_size);
		for (offset_body_addr = 0; offset_body_addr < buf->afbc_body_size;
			offset_body_addr += 4096) {
			*virt_addr = ((offset_body_addr + temp_body_addr) >> 12) & 0x000fffff;
			virt_addr++;
		}

		vc_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%lx\n", __func__,
			buf->afbc_table_addr);
	}

	buf->is_tvp = is_tvp;

	if (buf->phy_addr == 0 || (vicp_fbc_out_en && buf->afbc_table_addr == 0))
		ret = 0;
	else
		ret = 1;

	return ret;
}

static void check_window_change(struct composer_dev *dev,
				struct frames_info_t *cur_frame_info)
{
	int last_width, last_height, current_width, current_height;
	int cur_pos_x, cur_pos_y, cur_pos_w, cur_pos_h;
	int last_pos_x, last_pos_y, last_pos_w, last_pos_h;
	struct frames_info_t *last_frame_info;
	int last_zorder, cur_zorder;
	bool window_changed = false;
	int i;

	last_frame_info = &dev->last_frames.frames_info;
	if (cur_frame_info->frame_count != last_frame_info->frame_count) {
		window_changed = true;
		vc_print(dev->index, PRINT_ERROR,
			 "last count=%d, current count=%d\n",
			 last_frame_info->frame_count,
			 cur_frame_info->frame_count);
	} else {
		for (i = 0; i < cur_frame_info->frame_count; i++) {
			current_width = cur_frame_info->frame_info[i].crop_w;
			current_height = cur_frame_info->frame_info[i].crop_h;
			last_width = last_frame_info->frame_info[i].crop_w;
			last_height = last_frame_info->frame_info[i].crop_h;

			if (current_width * last_height !=
				current_height * last_width) {
				vc_print(dev->index, PRINT_ERROR,
					 "frame width or height changed!");
				window_changed = true;
				break;
			}

			cur_pos_x = cur_frame_info->frame_info[i].dst_x;
			cur_pos_y = cur_frame_info->frame_info[i].dst_y;
			cur_pos_w = cur_frame_info->frame_info[i].dst_w;
			cur_pos_h = cur_frame_info->frame_info[i].dst_h;
			last_pos_x = last_frame_info->frame_info[i].dst_x;
			last_pos_y = last_frame_info->frame_info[i].dst_y;
			last_pos_w = last_frame_info->frame_info[i].dst_w;
			last_pos_h = last_frame_info->frame_info[i].dst_h;

			if (cur_pos_x != last_pos_x ||
			    cur_pos_y != last_pos_y ||
			    cur_pos_w != last_pos_w ||
			    cur_pos_h != last_pos_h) {
				vc_print(dev->index, PRINT_OTHER,
					 "frame axis changed!");
				window_changed = true;
				break;
			}

			cur_zorder = cur_frame_info->frame_info[i].zorder;
			last_zorder = last_frame_info->frame_info[i].zorder;
			if (cur_zorder != last_zorder) {
				vc_print(dev->index, PRINT_OTHER,
					 "frame zorder changed!");
				window_changed = true;
				break;
			}
		}
	}

	if (!window_changed)
		return;

	for (i = 0; i < BUFFER_LEN; i++)
		dev->dst_buf[i].dirty = true;
}

static struct output_axis output_axis_adjust(struct composer_dev *dev,
	struct frame_info_t *vframe_info)
{
	int picture_width = 0, picture_height = 0;
	int render_w = 0, render_h = 0;
	int disp_w, disp_h;
	struct output_axis axis;
	int tmp;

	memset(&axis, 0, sizeof(struct output_axis));
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(vframe_info)) {
		pr_info("%s: invalid param.\n", __func__);
		return axis;
	}

	picture_width = vframe_info->crop_w;
	picture_height = vframe_info->crop_h;
	disp_w = vframe_info->dst_w;
	disp_h = vframe_info->dst_h;

	if (vframe_info->transform == VC_TRANSFORM_ROT_90 ||
		vframe_info->transform == VC_TRANSFORM_ROT_270) {
		tmp = picture_height;
		picture_height = picture_width;
		picture_width = tmp;
	}
	if (!full_axis) {
		render_w = disp_w;
		render_h = disp_w * picture_height / picture_width;
		if (render_h > disp_h) {
			render_h = disp_h;
			render_w = disp_h * picture_width / picture_height;
		}
	} else {
		render_w = disp_w;
		render_h = disp_h;
	}
	axis.left = vframe_info->dst_x + (disp_w - render_w) / 2;
	axis.top = vframe_info->dst_y + (disp_h - render_h) / 2;
	axis.width = render_w;
	axis.height = render_h;

	vc_print(dev->index, PRINT_AXIS,
		 "frame out data axis left top width height: %d %d %d %d\n",
		 axis.left, axis.top, axis.width, axis.height);
	return axis;
}

bool vf_is_pre_link(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (vf && pvpp_check_vf(vf) > 0 && vf->vf_ext)
		return true;
#endif
	return false;
}

//#define IS_DI_PSTLINK(di_flag) ((di_flag) & DI_FLAG_DI_PSTVPPLINK)

bool vf_is_post_link(struct vframe_s *vf)
{
	if (vf && vf->di_flag && IS_DI_PSTLINK(vf->di_flag))
		return true;
	return false;
}

static struct vframe_s *get_vf_from_file(struct composer_dev *dev,
					 struct file *file_vf, bool need_dw)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	bool is_dec_vf = false;
	struct file_private_data *file_private_data = NULL;
	bool enable_prelink = false;
	bool dec_is_i = false;
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct vframe_s *dma_di_vf = NULL;
	bool dma_has_di_vf = false;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);

	if (is_dec_vf) {
		vc_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf =
		dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			vc_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}

		di_vf = vf->vf_ext;
		vc_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);
		dec_is_i = vf->type & VIDTYPE_INTERLACE;

		dmabuf = (struct dma_buf *)(file_vf->private_data);
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_DI);
		if (!IS_ERR_OR_NULL(uhmod)) {
			dma_has_di_vf = true;
			dma_di_vf = (struct vframe_s *)uhmod->arg;
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vc_print(dev->index, PRINT_OTHER,
				"dma_has_di_vf=%d, dma_di_vf=%px\n",
				dma_has_di_vf, dma_di_vf);
			if (!(dma_has_di_vf && di_vf == dma_di_vf)) {
				vc_print(dev->index, PRINT_ERROR,
					"di vf err: file_vf=%px, dmabuf=%px, uhmod=%px, vf=%px\n",
					file_vf, dmabuf, uhmod, vf);
				vc_print(dev->index, PRINT_ERROR,
					"di_vf=%px, dma_di_vf=%px, frame_index=%d\n",
					di_vf, dma_di_vf, vf->frame_index);
				di_vf = NULL;
			}
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
			enable_prelink = dim_get_pre_link();
#endif
			vc_print(dev->index, PRINT_OTHER,
				"di_vf->type = 0x%x, di_vf->org = 0x%x, enable_prelink = %d\n",
				di_vf->type,
				di_vf->type_original,
				enable_prelink);
			if (!need_dw ||
			    (need_dw && di_vf->width != 0 &&
				di_vf->canvas0_config[0].phy_addr != 0 &&
				((!dec_is_i && !enable_prelink) || dec_is_i))) {
				vc_print(dev->index, PRINT_OTHER,
					"use di vf\n");
				/* link uvm vf into di_vf->vf_ext */
				if (!di_vf->vf_ext)
					di_vf->vf_ext = vf;
				/* link uvm vf into vf->uvm_vf */
				di_vf->uvm_vf = vf;
				vf = di_vf;
			}
		}
		if (vf->frame_index == 0 && vf->index_disp != 0)
			vf->frame_index = vf->index_disp;

		if (dma_has_di_vf)
			uvm_put_hook_mod(dmabuf, VF_PROCESS_DI);
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));

	} else {
		vc_print(dev->index, PRINT_OTHER, "vf is from v4lvideo\n");
		file_private_data = vc_get_file_private(dev, file_vf);
		if (!file_private_data) {
			vc_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		} else {
			vf = &file_private_data->vf;
			if (need_dw && (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) && vf->vf_ext) {
				if (vf->width == 0 ||
					vf->canvas0_config[0].phy_addr == 0 ||
					vf_is_pre_link(vf)) {
					vf = vf->vf_ext;
					vc_print(dev->index, PRINT_OTHER, "use dec vf.\n");
				}
			}
		}
	}
	return vf;
}

static void dump_vf(int vc_index, struct vframe_s *vf, int flag)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	loff_t pos;

	/*use flag to distinguish src and dst vframe*/
	if (!vf)
		return;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		vc_print(vc_index, PRINT_ERROR, "%s: security vf.\n", __func__);
		return;
	}

	if (flag == 0)
		snprintf(name_buf, sizeof(name_buf),
			"/data/src_vframe_%d.yuv", dump_vframe);
	else
		snprintf(name_buf, sizeof(name_buf),
			"/data/dst_vframe_%d.yuv", dump_vframe);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height;
	data_size_uv = vf->canvas0_config[1].width *
			vf->canvas0_config[1].height;
	data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		vc_print(vc_index, PRINT_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

static void dump_dma(int vc_index, struct frame_info_t *vframe_info_cur,
		unsigned long addr)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	loff_t pos;
	unsigned long addr_uv;

	snprintf(name_buf, sizeof(name_buf), "/data/src_vframe_%d.yuv", dump_vframe);
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vframe_info_cur->buffer_w *
			vframe_info_cur->buffer_h;
	data_size_uv = vframe_info_cur->buffer_w *
			vframe_info_cur->buffer_h / 2;
	addr_uv = addr + data_size_y;
	data_y = codec_mm_vmap(addr, data_size_y);
	data_uv = codec_mm_vmap(addr_uv, data_size_uv);
	if (!data_y || !data_uv) {
		vc_print(vc_index, PRINT_ERROR, "%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	vc_print(vc_index, PRINT_ERROR, "%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

static bool check_dewarp_support_status(struct composer_dev *dev,
	struct received_frames_t *received_frames)
{
	struct frame_info_t frame_info;
	struct composer_vf_para vframe_para;
	struct vframe_s *src_vf = NULL;
	struct file *file_vf = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return false;
	}

	if (received_frames->frames_info.frame_count > 1) {
		vc_print(dev->index, PRINT_OTHER, "%s: dewarp not support composer.\n", __func__);
		return false;
	}

	memset(&vframe_para, 0, sizeof(struct composer_vf_para));
	memset(&frame_info, 0, sizeof(struct frame_info_t));
	frame_info = received_frames->frames_info.frame_info[0];
	vframe_para.src_vf_width = frame_info.crop_w;
	vframe_para.src_vf_height = frame_info.crop_h;
	vframe_para.dst_vf_width = dewarp_rotate_width;
	vframe_para.dst_vf_height = dewarp_rotate_height;
	vframe_para.src_vf_angle = frame_info.transform;
	file_vf = received_frames->file_vf[0];
	detect_vf_type(&frame_info, file_vf, &is_dec_vf, &is_v4l_vf);

	if (is_dec_vf || is_v4l_vf) {
		src_vf = get_vf_from_file(dev, file_vf, true);
		if (!src_vf) {
			vc_print(dev->index, PRINT_ERROR, "get vf NULL\n");
			vframe_para.src_vf_format = NV12;
		} else {
			vframe_para.src_vf_format = get_dewarp_format(dev->index, src_vf);
		}
	} else {
		vframe_para.src_vf_format = NV12;
	}
	dev->dewarp_para.vf_para = &vframe_para;
	if (dev->need_rotate && is_dewarp_supported(dev->index, dev->dewarp_para.vf_para))
		return true;
	else
		return false;
}

static void choose_composer_device(struct composer_dev *dev,
	struct received_frames_t *received_frames)
{
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		dev->dev_choice = COMPOSER_WITH_UNINITIAL;
		return;
	}

	if (composer_dev_choice == 0) {
		if (check_dewarp_support_status(dev, received_frames))
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (is_vicp_supported() && !dev->need_rotate)
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	} else {
		if (check_dewarp_support_status(dev, received_frames) && composer_dev_choice == 2)
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (is_vicp_supported() && composer_dev_choice == 3 && !dev->need_rotate)
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	}
}

static void check_vicp_ship_mode(struct composer_dev *dev, struct frames_info_t *frames_info,
	enum vicp_skip_mode_e *buf)
{
	int i = 0, input_4k_count = 0;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(frames_info) || IS_ERR_OR_NULL(buf)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	for (i = 0; i < frames_info->frame_count; i++) {
		if (frames_info->frame_info[i].buffer_w > 1920) {
			input_4k_count++;
			buf[i] = VICP_SKIP_MODE_ALL;
		} else {
			buf[i] = VICP_SKIP_MODE_OFF;
		}
	}

	if (input_4k_count <= 2) {
		vc_print(dev->index, PRINT_VICP, "%s: no need skip.\n", __func__);
		memset(buf, VICP_SKIP_MODE_OFF, frames_info->frame_count);
	}
}

static void video_wait_aiface_ready(struct composer_dev *dev,
				struct vf_aiface_t *aiface_info, int frame_index)
{
	int wait_count = 0;

	while (1) {
		if (aiface_info->nn_status != NN_DONE &&
			(aiface_info->nn_status == NN_WAIT_DOING ||
			aiface_info->nn_status == NN_START_DOING)) {
			usleep_range(2000, 2100);
			wait_count++;
			vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
				"aiface wait count %d, frame_index=%d, nn_status=%d\n",
				wait_count, frame_index, aiface_info->nn_status);
		} else {
			break;
		}
	}
	vc_print(dev->index, PRINT_PATTERN | PRINT_AIFACE,
		 "aiface wait %dms, frame_index=%d, nn_status=%d\n",
		 2 * wait_count, frame_index, aiface_info->nn_status);
}

static struct vf_aiface_t *aiface_info_adjust(struct composer_dev *dev, struct file *file_vf,
	int frame_index, int display_x, int display_y, int display_w, int display_h)
{
	struct vf_aiface_t *aiface_info = NULL;
	struct face_value_t face_value_tmp;
	int nn_width, nn_height, count = 0;
	int i = 0;

	aiface_info = vc_get_aiface_info(dev, file_vf, frame_index);
	if (aiface_info) {
		nn_width = aiface_info->nn_frame_width;
		nn_height = aiface_info->nn_frame_height;
		video_wait_aiface_ready(dev, aiface_info, frame_index);
		if (aiface_info->nn_status == NN_DONE) {
			memset(&face_value_tmp, 0, sizeof(struct face_value_t));
			count = aiface_info->aiface_value_count;
			for (i = 0; i < count; i++) {
				vc_print(dev->index, PRINT_AIFACE,
					"src: frame_index=%d, x=%d, y=%d, w=%d, h=%d, score=%d.\n",
					frame_index,
					aiface_info->face_value[i].x,
					aiface_info->face_value[i].y,
					aiface_info->face_value[i].w,
					aiface_info->face_value[i].h,
					aiface_info->face_value[i].score);
				face_value_tmp.x = display_x +
					display_w * aiface_info->face_value[i].x / nn_width;
				face_value_tmp.y = display_y +
					display_h * aiface_info->face_value[i].y / nn_height;
				face_value_tmp.w = display_w *
					aiface_info->face_value[i].w / nn_width;
				face_value_tmp.h = display_h *
					aiface_info->face_value[i].h / nn_height;
				face_value_tmp.score = aiface_info->face_value[i].score;

				aiface_info->face_value[i] = face_value_tmp;
				vc_print(dev->index, PRINT_AIFACE,
					"dst: frame_index=%d, x=%d, y=%d, w=%d, h=%d, score=%d.\n",
					frame_index,
					aiface_info->face_value[i].x,
					aiface_info->face_value[i].y,
					aiface_info->face_value[i].w,
					aiface_info->face_value[i].h,
					aiface_info->face_value[i].score);
			}

			aiface_info->nn_status = NN_DISPLAYED;
		} else {
			vc_print(dev->index, PRINT_AIFACE,
				"%s: frame_index=%d, aiface not ready.\n",
				__func__, frame_index);
		}
	} else {
		//vc_print(dev->index, PRINT_ERROR, "%s: get aiface info failed.\n", __func__);
		aiface_info = NULL;
	}

	return aiface_info;
}

static int video_wait_file_fence(struct composer_dev *dev,
				   struct file *fence_file)
{
	struct sync_file *sync_file = NULL;
	struct dma_fence *fence_obj = NULL;
	int ret = 1;
	u64 timestamp;
	u64 time_cost;

	if (!IS_ERR_OR_NULL(fence_file)) {
		sync_file = (struct sync_file *)fence_file->private_data;
	} else {
		vc_print(dev->index, PRINT_FENCE, "wait: fence_file is NULL\n");
		return 1;
	}

	if (!IS_ERR_OR_NULL(sync_file)) {
		fence_obj = sync_file->fence;
	} else {
		vc_print(dev->index, PRINT_FENCE, "sync_file is NULL\n");
		return 1;
	}

	if (fence_obj) {
		vc_print(dev->index, PRINT_FENCE, "sync_file=%px, seqno=%lld\n",
			sync_file, fence_obj->seqno);
		timestamp = local_clock();
		ret = dma_fence_wait_timeout(fence_obj,
					     false, msecs_to_jiffies(3000));
		if (ret == 0) {
			vc_print(dev->index, PRINT_ERROR, "fence wait timeout\n");
			return 0;
		}

		time_cost = local_clock() - timestamp;
		dev->fence_wait_time_total += time_cost;
		dev->fence_wait_count++;
		if (dev->fence_wait_count == 100) {
			vc_print(dev->index, PRINT_FENCE,
				"wait fence avg=%lldns\n",
				div64_u64(dev->fence_wait_time_total, dev->fence_wait_count));
				dev->fence_wait_count = 0;
				dev->fence_wait_time_total = 0;
		}

		vc_print(dev->index, PRINT_FENCE,
			 "wait fence, state: %d, wait cost time:%lldms\n",
			 ret,
			 div64_u64(time_cost, 1000000));
	}

	fput(fence_file);
	return 1;
}

static void vframe_do_mosaic_22(struct composer_dev *dev)
{
	struct received_frames_t *received_frames = NULL;
	struct vd_prepare_s *vd_prepare = NULL;
	struct vframe_s *vf = NULL;
	struct vframe_s *scr_vf = NULL;
	struct vframe_s *mosaic_vf = NULL;
	struct vframe_s *vf_ext = NULL;
	struct frames_info_t *frames_info = NULL;
	struct file *file_vf = NULL;
	int i;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct video_composer_private *vc_private;
	struct frame_info_t *frame_info = NULL;
	u32 pic_w;
	u32 pic_h;

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	if (!kfifo_peek(&dev->receive_q, &received_frames))
		return;

	vd_prepare = vd_prepare_data_q_get(dev);
	if (!vd_prepare) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get prepare_data failed.\n",
			 __func__);
		return;
	}

	vf = &vd_prepare->dst_frame;
	memset(vf, 0, sizeof(struct vframe_s));

	if (!kfifo_get(&dev->receive_q, &received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "com: get failed\n");
		return;
	}

	vc_private = vc_private_q_pop(dev);
	if (!vc_private) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get vc_private failed.\n",
			 __func__);
		return;
	}

	vc_private->flag |= VC_FLAG_MOSAIC_22;
	vf->vc_private = vc_private;

	frames_info = &received_frames->frames_info;

	for (i = 0; i < 4; i++) {
		frame_info = &frames_info->frame_info[i];
		scr_vf = NULL;
		file_vf = received_frames->file_vf[i];
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		if (is_dec_vf || is_v4l_vf) {
			vc_print(dev->index, PRINT_OTHER,
				 "%s dma buffer is vf\n", __func__);
			scr_vf = get_vf_from_file(dev, file_vf, false);
			if (!scr_vf) {
				vc_print(dev->index,
					 PRINT_ERROR, "get vf NULL\n");
				continue;
			}
		} else {
			vc_print(dev->index, PRINT_ERROR, "%s dma buffer not vf\n", __func__);
		}
		if (!scr_vf) {
			vc_print(dev->index, PRINT_ERROR, "%s:no vf\n", __func__);
			return;
		}
		vc_private->mosaic_src_vf[i] = scr_vf;
		vc_private->mosaic_dst_vf[i] = *scr_vf;
		vc_private->mosaic_vf[i] = &vc_private->mosaic_dst_vf[i];
		mosaic_vf = vc_private->mosaic_vf[i];

		vc_print(dev->index, PRINT_AXIS,
			"mosaic %d frame_info crop: x y w h %d %d %d %d\n", i,
			frame_info->crop_x,
			frame_info->crop_y,
			frame_info->crop_w,
			frame_info->crop_h);
		if (is_src_crop_valid(mosaic_vf->src_crop))
			vc_print(dev->index, PRINT_AXIS,
				"mosaic %d src_crop: %d %d %d %d\n", i,
				mosaic_vf->src_crop.top,
				mosaic_vf->src_crop.left,
				mosaic_vf->src_crop.bottom,
				mosaic_vf->src_crop.right);

		mosaic_vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		mosaic_vf->axis[0] = frame_info->dst_x;
		mosaic_vf->axis[1] = frame_info->dst_y;
		mosaic_vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		mosaic_vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
		mosaic_vf->crop[0] = frame_info->crop_y;
		mosaic_vf->crop[1] = frame_info->crop_x;
		if ((mosaic_vf->type & VIDTYPE_COMPRESS) != 0) {
			pic_w = mosaic_vf->compWidth;
			pic_h = mosaic_vf->compHeight;
		} else {
			pic_w = mosaic_vf->width;
			pic_h = mosaic_vf->height;
		}
		if (frame_info->source_type == SOURCE_DTV_FIX_TUNNEL) {
			mosaic_vf->flag |= VFRAME_FLAG_FIX_TUNNEL;
			mosaic_vf->crop[2] = frame_info->crop_y +
				frame_info->crop_h;
			mosaic_vf->crop[3] = frame_info->crop_x +
				frame_info->crop_w;
			vc_print(dev->index, PRINT_AXIS,
				"mosaic %d tunnel set vf crop:%d %d %d %d\n", i,
				mosaic_vf->crop[0],
				mosaic_vf->crop[1],
				mosaic_vf->crop[2],
				mosaic_vf->crop[3]);
		} else {
			mosaic_vf->crop[2] = pic_h - frame_info->crop_h - frame_info->crop_y;
			mosaic_vf->crop[3] = pic_w - frame_info->crop_w - frame_info->crop_x;
			vc_print(dev->index, PRINT_AXIS,
				"mosaic %d non-tunnel set vf org crop:%d %d %d %d\n", i,
				mosaic_vf->crop[0],
				mosaic_vf->crop[1],
				mosaic_vf->crop[2],
				mosaic_vf->crop[3]);
			if (is_src_crop_valid(mosaic_vf->src_crop)) {
				if (mosaic_vf->type & VIDTYPE_COMPRESS) {
					mosaic_vf->crop[2] -= mosaic_vf->src_crop.bottom;
					mosaic_vf->crop[3] -= mosaic_vf->src_crop.right;
					if ((int)mosaic_vf->crop[2] < 0)
						mosaic_vf->crop[2] = 0;
					if ((int)mosaic_vf->crop[3] < 0)
						mosaic_vf->crop[3] = 0;
				}
				vc_print(dev->index, PRINT_AXIS,
					"mosaic %d none-tunnel set final vf crop:%d %d %d %d\n", i,
					mosaic_vf->crop[0],
					mosaic_vf->crop[1],
					mosaic_vf->crop[2],
					mosaic_vf->crop[3]);
			}
		}

		mosaic_vf->zorder = frame_info->zorder;
		mosaic_vf->file_vf = file_vf;

		if (mosaic_vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			vf_ext = mosaic_vf->vf_ext;
			if (vf_ext) {
				vf_ext->axis[0] = mosaic_vf->axis[0];
				vf_ext->axis[1] = mosaic_vf->axis[1];
				vf_ext->axis[2] = mosaic_vf->axis[2];
				vf_ext->axis[3] = mosaic_vf->axis[3];
				vf_ext->crop[0] = mosaic_vf->crop[0];
				vf_ext->crop[1] = mosaic_vf->crop[1];
				vf_ext->crop[2] = mosaic_vf->crop[2];
				vf_ext->crop[3] = mosaic_vf->crop[3];
				vf_ext->zorder = mosaic_vf->zorder;
				vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER
					| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
			} else {
				vc_print(dev->index, PRINT_ERROR,
					 "vf_ext is null\n");
			}
		}
	}

	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;

	vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	vf->type_ext |= VIDTYPE_EXT_MOSAIC_22;

	vf->axis[0] = 0;
	vf->axis[1] = 0;
	vf->axis[2] = dev->vinfo_w - 1;
	vf->axis[3] = dev->vinfo_h - 1;

	vf->crop[0] = 0;
	vf->crop[1] = 0;
	vf->crop[2] = 0;
	vf->crop[3] = 0;

	vf->zorder = frames_info->disp_zorder;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;

	vf->width = dev->vinfo_w;
	vf->height = dev->vinfo_h;

	vc_print(dev->index, PRINT_DEWARP,
			 "composer:vf_w: %d, vf_h: %d\n", vf->width, vf->height);

	vf->canvas0_config[0].phy_addr = 0;
	vf->canvas0_config[0].width = dev->vinfo_w;
	vf->canvas0_config[0].height = dev->vinfo_h;
	vf->canvas0_config[0].block_mode = 0;

	vf->canvas0_config[1].phy_addr = 0;
	vf->canvas0_config[1].width = dev->vinfo_w;
	vf->canvas0_config[1].height = dev->vinfo_h >> 1;
	vf->canvas0_config[1].block_mode = 0;
	vf->plane_num = 2;

	vf->repeat_count = 0;

	dev->vd_prepare_last = vd_prepare;

	dev->fake_vf = *vf;

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf))
		vc_print(dev->index, PRINT_ERROR, "ready_q is full\n");

	atomic_set(&received_frames->on_use, false);
}

static void check_composer_buffer_status(struct composer_dev *dev, bool is_tvp,
		size_t usage, struct dst_buf_t *dst_buf)
{
	int ret;
	int flags;
	u32 buf_width, buf_height, buf_size;

	buf_width = (dev->vinfo_w + 0x1f) & ~0x1f;
	buf_height = dev->vinfo_h;

	vc_print(dev->index, PRINT_OTHER,
		"%s: output_duration is %lld.\n", __func__, dev->output_duration);
	if (dev->output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	vc_print(dev->index, PRINT_OTHER, "%s: usage: %ld\n", __func__, usage);
	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > pic_mode_max_width)
			buf_width = pic_mode_max_width;
		if (buf_height > pic_mode_max_height)
			buf_height = pic_mode_max_height;
	} else {
		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			buf_width = dewarp_rotate_width;
			buf_height = dewarp_rotate_height;
			dst_buf->buf_used = USED_BY_DEWARP;
		} else if (dev->dev_choice == COMPOSER_WITH_GE2D) {
			buf_width = rotate_width;
			buf_height = rotate_height;
			dst_buf->buf_used = USED_BY_GE2D;
		}
	}

	if (composer_use_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	dev->composer_buf_w = buf_width;
	dev->composer_buf_h = buf_height;
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	if (dst_buf->buf_size < buf_size) {
		ret = codec_mm_free_for_dma(ports[dev->index].name, dst_buf->phy_addr);
		if (ret == 0)
			vc_print(dev->index, PRINT_ERROR, "free small buffer success.\n");
		dst_buf->phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
						buf_size / PAGE_SIZE, 0, flags);
		vc_print(dev->index, PRINT_ERROR,
			"%s:alloc big buffer success,addr:0x%lx size:%d\n",
			__func__, dst_buf->phy_addr, buf_size);
	}

	if (dev->dev_choice == COMPOSER_WITH_GE2D &&
		IS_ERR_OR_NULL(dev->ge2d_para.context)) {
		ret = init_ge2d_composer(&dev->ge2d_para);
		if (ret < 0) {
			vc_print(dev->index, PRINT_ERROR, "%s: init ge2d composer err!\n",
				__func__);
			return;
		}
	}
	if (dev->dev_choice == COMPOSER_WITH_DEWARP &&
		IS_ERR_OR_NULL(dev->dewarp_para.context)) {
		ret = init_dewarp_composer(&dev->dewarp_para);
		if (ret < 0) {
			vc_print(dev->index, PRINT_ERROR, "%s: init dewarp composer err!\n",
				__func__);
			return;
		}
	}

	dst_buf->buf_w = buf_width;
	dst_buf->buf_h = buf_height;
	dst_buf->buf_size = buf_size;
	dst_buf->is_tvp = is_tvp;
}

static int get_output_duration(struct composer_dev *dev)
{
	int duration = 0;

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return 1600;
	}

	if (dev->output_duration > 0 && dev->output_duration < 24)
		duration = 4004;
	else if (dev->output_duration == 24)
		duration = 4000;
	else if (dev->output_duration == 25)
		duration = 3840;
	else if (dev->output_duration > 25 && dev->output_duration < 30)
		duration = 3203;
	else if (dev->output_duration == 30)
		duration = 3200;
	else if (dev->output_duration == 50)
		duration = 1920;
	else if (dev->output_duration > 50 && dev->output_duration < 60)
		duration = 1601;
	else if (dev->output_duration == 60)
		duration = 1600;
	else if (dev->output_duration == 100)
		duration = 960;
	else if (dev->output_duration > 100 && dev->output_duration < 120)
		duration = 801;
	else if (dev->output_duration == 120)
		duration = 800;
	else if (dev->output_duration == 240)
		duration = 400;
	else
		duration = 1600;

	return duration;
}

static void vframe_composer(struct composer_dev *dev)
{
	struct received_frames_t *received_frames = NULL;
	struct received_frames_t *received_frames_tmp = NULL;
	struct frames_info_t *frames_info = NULL;
	struct vframe_s *src_vf = NULL;
	struct file *file_vf = NULL;
	int vf_dev[MAX_COMPOSER_COUNT];
	struct frame_info_t *vframe_info[MAX_COMPOSER_COUNT];
	size_t usage = 0;
	int i, j, tmp;
	u32 zd1, zd2;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;
	int ret = 0;
	struct vframe_s *dst_vf = NULL;
	int count;
	struct dst_buf_t *dst_buf = NULL;
	u32 drop_count = 0;
	unsigned long addr = 0;
	struct output_axis dst_axis;
	struct output_axis display_axis;
	int min_left = 0, min_top = 0;
	int max_right = 0, max_bottom = 0;
	struct composer_info_t *composer_info;
	bool is_dec_vf = false, is_v4l_vf = false;
	bool is_tvp = false;
	bool is_fixtunnel = false;
	struct src_data_para *src_data = NULL;
	struct composer_vf_para *vframe_para;
	struct vicp_data_config_s *data_config;
	struct crop_info_s crop_info;
	ulong buf_addr[3];
	int fbc_init_ctrl, fbc_pip_mode;
	int mifout_en = 1, fbcout_en = 1;
	enum vicp_skip_mode_e skip_mode[MAX_COMPOSER_COUNT] = {VICP_SKIP_MODE_OFF};
	struct file *temp_file = NULL;
	struct frame_info_t *vframe_info_cur = NULL;
	struct composer_common_para common_para;
	struct vf_aiface_t *aiface_info_temp = NULL;
	u32 num = 0, last_num = 0, size = 0;
	struct vframe_s *input_vf[MAX_COMPOSER_COUNT] = {NULL};
	struct output_axis out_axis[MAX_COMPOSER_COUNT] = {0};
	struct file *fence_file;
	bool has_fence;
	u32 transform_tmp;
	u32 dewarp_crop_top = 0, dewarp_crop_left = 0;
	u32 dewarp_crop_bottom = 0, dewarp_crop_right = 0;
	u32 dewarp_src_w, dewarp_src_h;
	u32 dewarp_dst_w, dewarp_dst_h;

	if (IS_ERR_OR_NULL(dev)) {
		vc_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	do_gettimeofday(&begin_time);

	if (!kfifo_peek(&dev->receive_q, &received_frames_tmp))
		return;

	choose_composer_device(dev, received_frames_tmp);
	is_tvp = received_frames_tmp->is_tvp;
	temp_file = received_frames_tmp->file_vf[0];
#ifdef CONFIG_AMLOGIC_UVM_CORE
	if (meson_uvm_get_usage(received_frames_tmp->file_vf[0]->private_data, &usage) < 0)
		vc_print(dev->index, PRINT_ERROR,
			"%s:meson_uvm_get_usage fail.\n", __func__);
#endif
	ret = video_composer_init_buffer(dev, is_tvp, usage);
	if (ret != 0) {
		vc_print(dev->index, PRINT_ERROR, "vc: init buffer failed!\n");
		video_composer_uninit_buffer(dev);
		return;
	} else {
		dst_vf = get_dst_vframe_buffer(dev);
		if (!dst_vf)
			return;
	}

	if (IS_ERR_OR_NULL(dst_vf)) {
		vc_print(dev->index, PRINT_PATTERN, "dst vf is NULL\n");
		return;
	}

	memset(dst_vf, 0, sizeof(struct vframe_s));
	dst_buf = to_dst_buf(dst_vf);
	composer_info = &dst_buf->componser_info;
	memset(composer_info, 0, sizeof(struct composer_info_t));
	memset(&common_para, 0, sizeof(struct composer_common_para));
	while (1) {
		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR, "com: get failed\n");
			return;
		}

		has_fence = false;
		frames_info = &received_frames->frames_info;
		count = frames_info->frame_count;
		for (i = 0; i < count - 1; i++) {
			fence_file = received_frames->fence_file[i];
			if (fence_file) {
				has_fence = true;
				break;
			}
		}
		if (has_fence) {
			vc_print(dev->index, PRINT_OTHER, "com: has fence, cannot drop\n");
			break;
		}

		if (!kfifo_peek(&dev->receive_q, &received_frames_tmp))
			break;
		drop_count++;
		frames_put_file(dev, received_frames);
		vc_print(dev->index, PRINT_OTHER, "com: drop frame\n");
		atomic_set(&received_frames->on_use, false);
	}

	if (temp_file != received_frames->file_vf[0])
		choose_composer_device(dev, received_frames);

	if ((u8)dst_buf->buf_used != (u8)dev->dev_choice && dev->need_rotate)
		check_composer_buffer_status(dev, is_tvp, usage, dst_buf);

	frames_info = &received_frames->frames_info;

	if (force_comp_w != 0) {
		frames_info->frame_info[0].dst_x = 0;
		frames_info->frame_info[0].dst_y = 0;
		frames_info->frame_info[0].dst_w = force_comp_w;
		frames_info->frame_info[0].dst_h = force_comp_h;
	}

	count = frames_info->frame_count;
	check_window_change(dev, &received_frames->frames_info);
	is_tvp = received_frames->is_tvp;
	if (is_tvp != dst_buf->is_tvp) {
		ret = need_switch_buffer(dst_buf, is_tvp, dev);
		if (ret == 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "switch buffer from %s to %s failed\n",
				 dst_buf->is_tvp ? "tvp" : "non tvp",
				 is_tvp ? "tvp" : "non tvp");
			return;
		}
	}

	if (composer_use_444) {
		dev->ge2d_para.format = GE2D_FORMAT_S24_YUV444;
		dev->ge2d_para.plane_num = 1;
	} else {
		dev->ge2d_para.format = GE2D_FORMAT_M24_NV21;
		dev->ge2d_para.plane_num = 2;
	}
	dev->ge2d_para.is_tvp = is_tvp;
	dev->ge2d_para.phy_addr[0] = dst_buf->phy_addr;
	dev->ge2d_para.buffer_w = dst_buf->buf_w;
	dev->ge2d_para.buffer_h = dst_buf->buf_h;
	dev->ge2d_para.canvas0_addr = -1;

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		if (dst_buf->dirty && !close_black) {
			ret = fill_vframe_black(&dev->ge2d_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "ge2d fill black failed\n");
			else
				vc_print(dev->index, PRINT_OTHER, "fill black\n");
			dst_buf->dirty = false;
		}
	}

	for (i = 0; i < count; i++) {
		vf_dev[i] = i;
		vframe_info[i] = &frames_info->frame_info[i];
	}

	for (i = 0; i < count - 1; i++) {
		for (j = 0; j < count - 1 - i; j++) {
			zd1 = vframe_info[vf_dev[j]]->zorder;
			zd2 = vframe_info[vf_dev[j + 1]]->zorder;
			if (zd1 > zd2) {
				tmp = vf_dev[j];
				vf_dev[j] = vf_dev[j + 1];
				vf_dev[j + 1] = tmp;
			}
		}
	}
	min_left = vframe_info[0]->dst_x;
	min_top = vframe_info[0]->dst_y;
	check_vicp_ship_mode(dev, frames_info, skip_mode);

	for (i = 0; i < count; i++) {
		file_vf = received_frames->file_vf[vf_dev[i]];
		if (i != 0) {
			fence_file = received_frames->fence_file[vf_dev[i]];
			if (video_wait_file_fence(dev, fence_file) == 0)
				continue;
		}
		vframe_info_cur = vframe_info[vf_dev[i]];
		if (!vframe_info_cur) {
			vc_print(dev->index, PRINT_ERROR, "vframe_info_cur NULL\n");
			return;
		}

		vc_print(dev->index, PRINT_AXIS,
			 "=========frame info:==========\n");
		vc_print(dev->index, PRINT_AXIS,
			 "frame axis x,y,w,h: %d %d %d %d\n",
			 vframe_info_cur->dst_x, vframe_info_cur->dst_y,
			 vframe_info_cur->dst_w, vframe_info_cur->dst_h);
		vc_print(dev->index, PRINT_AXIS,
			 "frame crop t,l,w,h: %d %d %d %d\n",
			 vframe_info_cur->crop_y, vframe_info_cur->crop_x,
			 vframe_info_cur->crop_w, vframe_info_cur->crop_h);
		vc_print(dev->index, PRINT_AXIS,
			 "frame buffer Width X Height: %d X %d\n",
			 vframe_info_cur->buffer_w, vframe_info_cur->buffer_h);
		vc_print(dev->index, PRINT_AXIS,
			 "frame buffer stride Width X Height: %d X %d\n",
			 vframe_info_cur->reserved[0], vframe_info_cur->reserved[1]);
		vc_print(dev->index, PRINT_AXIS,
			 "===============================\n");

		detect_vf_type(vframe_info_cur, file_vf, &is_dec_vf, &is_v4l_vf);
		if (vframe_info_cur->source_type == SOURCE_DTV_FIX_TUNNEL)
			is_fixtunnel = true;

		if (is_dec_vf || is_v4l_vf) {
			vc_print(dev->index, PRINT_OTHER, "%s dma buffer is vf\n", __func__);
			src_vf = get_vf_from_file(dev, file_vf, true);
			if (!src_vf) {
				vc_print(dev->index, PRINT_ERROR, "get vf NULL\n");
				continue;
			}
			if (src_vf->type & VIDTYPE_V4L_EOS) {
				vc_print(dev->index, PRINT_ERROR, "eos vf\n");
				continue;
			}
			common_para.input_para.vframe = src_vf;
		} else {
			addr = received_frames->phy_addr[vf_dev[i]];
			vc_print(dev->index, PRINT_OTHER,
				"%s dma buffer not vf, i=%d fd=%d phy_addr=0x%lx\n", __func__,
				vf_dev[i], vframe_info_cur->fd, addr);
		}

		if (src_vf && is_src_crop_valid(src_vf->src_crop)) {
			crop_info.left = MAX(vframe_info_cur->crop_x, src_vf->src_crop.left);
			crop_info.top = MAX(vframe_info_cur->crop_y, src_vf->src_crop.top);
			if (!(src_vf->type_original & VIDTYPE_COMPRESS)) {
				crop_info.width = MIN(vframe_info_cur->crop_w, src_vf->width -
					src_vf->src_crop.left - src_vf->src_crop.right);
				crop_info.height = MIN(vframe_info_cur->crop_h, src_vf->height -
					src_vf->src_crop.top - src_vf->src_crop.bottom);
			} else {
				crop_info.width = MIN(vframe_info_cur->crop_w, src_vf->compWidth -
					src_vf->src_crop.left - src_vf->src_crop.right);
				crop_info.height = MIN(vframe_info_cur->crop_h, src_vf->compHeight -
					src_vf->src_crop.top - src_vf->src_crop.bottom);
			}
		} else {
			crop_info.left = vframe_info_cur->crop_x;
			crop_info.top = vframe_info_cur->crop_y;
			crop_info.width = vframe_info_cur->crop_w;
			crop_info.height = vframe_info_cur->crop_h;
		}

		vc_print(dev->index, PRINT_AXIS, "crop_info: left %d, top %d, width %d height %d\n",
			crop_info.left, crop_info.top, crop_info.width, crop_info.height);

		dst_axis = output_axis_adjust(dev, vframe_info_cur);
		display_axis.left = dst_axis.left * dst_buf->buf_w / dev->vinfo_w;
		display_axis.top = dst_axis.top * dst_buf->buf_h / dev->vinfo_h;
		display_axis.width = dst_axis.width * dst_buf->buf_w / dev->vinfo_w;
		display_axis.height = dst_axis.height * dst_buf->buf_h / dev->vinfo_h;
		vc_print(dev->index, PRINT_AXIS,
			"display_axis: left top width height: %d %d %d %d\n",
			display_axis.left, display_axis.top,
			display_axis.width, display_axis.height);
		if (min_left > dst_axis.left)
			min_left = dst_axis.left;
		if (min_top > dst_axis.top)
			min_top = dst_axis.top;
		if (max_right < (dst_axis.left + dst_axis.width))
			max_right = dst_axis.left + dst_axis.width;
		if (max_bottom < (dst_axis.top + dst_axis.height))
			max_bottom = dst_axis.top + dst_axis.height;

		input_vf[i] = src_vf;
		out_axis[i] = display_axis;

		common_para.input_para.call_index = dev->index;
		common_para.input_para.transform = vframe_info_cur->transform;
		common_para.input_para.pic_info.format = vframe_info_cur->buffer_format;
		common_para.input_para.pic_info.width = vframe_info_cur->buffer_w;
		common_para.input_para.pic_info.height = vframe_info_cur->buffer_h;
		common_para.input_para.pic_info.addr[0] = addr;
		common_para.input_para.pic_info.align_w = vframe_info_cur->reserved[0];
		common_para.input_para.pic_info.align_h = vframe_info_cur->reserved[1];

		common_para.output_para.pic_info.align_w =
			(vframe_info_cur->dst_w * dst_buf->buf_w / dev->vinfo_w + 0xf) & ~0xf;
		common_para.output_para.pic_info.align_h =
			(vframe_info_cur->dst_h * dst_buf->buf_h / dev->vinfo_h + 0xf) & ~0xf;
		common_para.output_para.pic_info.addr[0] = dst_buf->phy_addr;
		common_para.input_para.pic_info.is_tvp = is_tvp;

		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			vc_print(dev->index, PRINT_OTHER, "use dewarp composer.\n");
			vframe_para = &dev->dewarp_vframe_para;
			memset(vframe_para, 0, sizeof(*vframe_para));
			ret = config_dewarp_vframe(vframe_para, &common_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "dewarp config err.\n");
			dev->dewarp_para.vf_para = vframe_para;
			ret = load_dewarp_firmware(&dev->dewarp_para);
			if (ret != 0) {
				vc_print(dev->index, PRINT_ERROR, "load firmware failed.\n");
				break;
			}
			ret = dewarp_data_composer(&dev->dewarp_para, is_tvp);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "dewarp data composer failed.\n");
		} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
			vc_print(dev->index, PRINT_OTHER, "use vicp composer.\n");
			data_config = &dev->vicp_data_config;
			memset(data_config, 0, sizeof(struct vicp_data_config_s));
			config_vicp_input_data(src_vf,
					addr,
					vframe_info_cur->buffer_w,
					vframe_info_cur->buffer_h,
					vframe_info_cur->reserved[0],
					vframe_info_cur->reserved[1],
					1,
					VICP_COLOR_FORMAT_YUV420,
					8,
					&data_config->input_data);

			if (vicp_output_dev == 1) {
				mifout_en = 1;
				fbcout_en = 0;
			} else if (vicp_output_dev == 2) {
				mifout_en = 0;
				fbcout_en = 1;
			} else {
				mifout_en = 1;
				fbcout_en = 1;
			}

			buf_addr[0] = (ulong)dst_buf->phy_addr;
			if (fbcout_en) {
				buf_addr[1] = dst_buf->afbc_head_addr;
				buf_addr[2] = dst_buf->afbc_table_addr;
			}

			if (count == 1) {
				fbc_init_ctrl = 1;
				fbc_pip_mode = 1;
			} else {
				if (i == 0) {
					fbc_init_ctrl = 1;
					fbc_pip_mode = 1;
				} else {
					fbc_init_ctrl = 0;
					fbc_pip_mode = 1;
				}
			}
			config_vicp_output_data(fbcout_en,
				mifout_en,
				buf_addr,
				dst_buf->buf_w,
				dst_buf->buf_w,
				dst_buf->buf_h,
				1,
				VICP_COLOR_FORMAT_YUV420,
				8,
				VICP_COLOR_FORMAT_YUV420,
				8,
				fbc_init_ctrl,
				fbc_pip_mode,
				VFRAME_SIGNAL_FMT_SDR,
				&data_config->output_data);
			data_config->data_option.rotation_mode =
				map_rotationmode_from_vc_to_vicp(vframe_info_cur->transform);
			data_config->data_option.crop_info.left = crop_info.left;
			data_config->data_option.crop_info.top = crop_info.top;
			data_config->data_option.crop_info.width = crop_info.width;
			data_config->data_option.crop_info.height = crop_info.height;
			data_config->data_option.output_axis.left = display_axis.left;
			data_config->data_option.output_axis.top = display_axis.top;
			data_config->data_option.output_axis.width = display_axis.width;
			data_config->data_option.output_axis.height = display_axis.height;

			data_config->data_option.shrink_mode =
				(enum vicp_shrink_mode_e)vicp_shrink_mode;
			if (count > 1)
				data_config->data_option.rdma_enable = true;
			else
				data_config->data_option.rdma_enable = false;
			data_config->data_option.input_source_count = count;
			data_config->data_option.input_source_number = i;
			data_config->data_option.security_enable = is_tvp;
			data_config->data_option.skip_mode = skip_mode[vf_dev[i]];
			data_config->data_option.compress_rate = lossy_compress_rate;

			ret = vicp_data_composer(data_config);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "vicp composer failed\n");
		} else {
			vc_print(dev->index, PRINT_OTHER, "use ge2d composer.\n");
			src_data = &dev->ge2d_src_data;
			src_data->buf_format = vframe_info_cur->buffer_format;
			ret = config_ge2d_data(src_vf,
				addr,
				vframe_info_cur->buffer_w,
				vframe_info_cur->buffer_h,
				vframe_info_cur->reserved[0],
				vframe_info_cur->reserved[1],
				crop_info.left,
				crop_info.top,
				crop_info.width,
				crop_info.height,
				src_data);
			if (ret < 0)
				continue;
			transform_tmp = vframe_info_cur->transform;
			if (src_vf && src_vf->flag & VFRAME_FLAG_MIRROR_H) {
				if (transform_tmp & VC_TRANSFORM_FLIP_H)
					transform_tmp &= ~VC_TRANSFORM_FLIP_H;
				else
					transform_tmp |= VC_TRANSFORM_FLIP_H;
			}
			if (src_vf && src_vf->flag & VFRAME_FLAG_MIRROR_V) {
				if (transform_tmp & VC_TRANSFORM_FLIP_V)
					transform_tmp &= ~VC_TRANSFORM_FLIP_V;
				else
					transform_tmp |= VC_TRANSFORM_FLIP_V;
			}
			dev->ge2d_para.angle = transform_tmp;
			dev->ge2d_para.position_left = display_axis.left;
			dev->ge2d_para.position_top = display_axis.top;
			dev->ge2d_para.position_width = display_axis.width;
			dev->ge2d_para.position_height = display_axis.height;

			ret = ge2d_data_composer(src_data, &dev->ge2d_para);
			if (ret < 0)
				vc_print(dev->index, PRINT_ERROR, "ge2d composer failed\n");
		}
		src_vf = NULL;
	}

	src_vf = common_para.input_para.vframe;
	if (src_vf)
		dst_vf->ready_jiffies64 = src_vf->ready_jiffies64;

	for (i = 0; i < count; i++) {
		if (!input_vf[i] || out_axis[i].width == 0 || out_axis[i].height == 0) {
			vc_print(dev->index, PRINT_AIFACE, "invalid aiface param.\n");
			break;
		}

		aiface_info_temp = aiface_info_adjust(dev,
						received_frames->file_vf[vf_dev[i]],
						input_vf[i]->frame_index,
						out_axis[i].left,
						out_axis[i].top,
						out_axis[i].width,
						out_axis[i].height);
		if (aiface_info_temp) {
			vc_print(dev->index, PRINT_AIFACE,
				"frame_index = %d, aiface_value_count = %d.\n",
				input_vf[i]->frame_index, aiface_info_temp->aiface_value_count);
			if (!dev->aiface_buf) {
				size = sizeof(struct vf_aiface_t) * MAX_FACE_COUNT_PER_FRAME;
				dev->aiface_buf = vmalloc(size);
				if (!dev->aiface_buf) {
					vc_print(dev->index, PRINT_ERROR, "vmalloc failed.\n");
					break;
				}
			}

			for (num = 0; num < aiface_info_temp->aiface_value_count; num++)
				dev->aiface_buf->face_value[last_num + num] =
					aiface_info_temp->face_value[num];

			last_num += num;
			if (last_num >= MAX_FACE_COUNT_PER_FRAME) {
				vc_print(dev->index, PRINT_ERROR, "over max face count.\n");
				break;
			}
		} else {
			vc_print(dev->index, PRINT_AIFACE, "get aiface_info failed.\n");
		}
	}

	if (dev->aiface_buf) {
		dev->aiface_buf->aiface_value_count = last_num;
		dst_vf->vc_private = vc_private_q_pop(dev);
		dst_vf->vc_private->aiface_info = dev->aiface_buf;
		dst_vf->vc_private->flag |= VC_FLAG_AI_FACE;
	} else {
		vc_print(dev->index, PRINT_AIFACE, "aiface_info is NULL.\n");
	}

	frames_put_file(dev, received_frames);

	do_gettimeofday(&end_time);
	cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
		+ (end_time.tv_usec - begin_time.tv_usec));
	vc_print(dev->index, PRINT_PERFORMANCE, "vframe composer cost: %d us\n", cost_time);

	dst_vf->flag |= (VFRAME_FLAG_VIDEO_COMPOSER | VFRAME_FLAG_COMPOSER_DONE);

	dst_vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	if (!composer_use_444) {
		dst_vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		dst_vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	} else {
		dst_vf->type = (VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD);
	}
	if (usage == UVM_USAGE_IMAGE_PLAY)
		dst_vf->type |= VIDTYPE_PIC;

	if (is_tvp)
		dst_vf->flag |= VFRAME_FLAG_VIDEO_SECURE;

	if (is_fixtunnel)
		dst_vf->flag |= VFRAME_FLAG_FIX_TUNNEL;

	if (!vframe_info_cur) {
		vc_print(dev->index, PRINT_ERROR, "vframe_info_cur NULL\n");
		return;
	}
	if (vframe_info_cur->transform == VC_TRANSFORM_FLIP_H_ROT_90)
		dst_vf->flag |= VFRAME_FLAG_MIRROR_V;
	if (vframe_info_cur->transform == VC_TRANSFORM_FLIP_V_ROT_90)
		dst_vf->flag |= VFRAME_FLAG_MIRROR_H;

	if (debug_axis_pip) {
		dst_vf->axis[0] = 0;
		dst_vf->axis[1] = 0;
		dst_vf->axis[2] = 0;
		dst_vf->axis[3] = 0;
	} else {
		dst_vf->axis[0] = min_left;
		dst_vf->axis[1] = min_top;
		dst_vf->axis[2] = max_right - 1;
		dst_vf->axis[3] = max_bottom - 1;
	}
	composer_info->count = count;
	for (i = 0; i < count; i++) {
		composer_info->axis[i][0] = vframe_info[vf_dev[i]]->dst_x
			- dst_vf->axis[0];
		composer_info->axis[i][1] = vframe_info[vf_dev[i]]->dst_y
			- dst_vf->axis[1];
		composer_info->axis[i][2] = vframe_info[vf_dev[i]]->dst_w
			+ composer_info->axis[i][0] - 1;
		composer_info->axis[i][3] = vframe_info[vf_dev[i]]->dst_h
			+ composer_info->axis[i][1] - 1;
		vc_print(dev->index, PRINT_AXIS,
			 "alpha index=%d %d %d %d %d\n",
			 i,
			 composer_info->axis[i][0],
			 composer_info->axis[i][1],
			 composer_info->axis[i][2],
			 composer_info->axis[i][3]);
	}
	if (debug_crop_pip) {
		dst_vf->crop[0] = 0;
		dst_vf->crop[1] = 0;
		dst_vf->crop[2] = 0;
		dst_vf->crop[3] = 0;
	} else {
		if (dev->dev_choice != COMPOSER_WITH_DEWARP) {
			dst_vf->crop[0] = min_top * dst_buf->buf_h / dev->vinfo_h;
			dst_vf->crop[1] = min_left * dst_buf->buf_w / dev->vinfo_w;
			dst_vf->crop[2] = dst_buf->buf_h -
				max_bottom * dst_buf->buf_h / dev->vinfo_h;
			dst_vf->crop[3] = dst_buf->buf_w -
				max_right * dst_buf->buf_w / dev->vinfo_w;
		} else {
			vc_print(dev->index, PRINT_DEWARP, "dewarp no need crop.\n");
		}
	}
	vc_print(dev->index, PRINT_AXIS,
		 "min_top,min_left,max_bottom,max_right: %d %d %d %d\n",
		 min_top, min_left, max_bottom, max_right);

	if (src_vf && count == 1 && dev->dev_choice != COMPOSER_WITH_VICP) {
		vc_print(dev->index, PRINT_OTHER,
			 "%s: copy hdr info.\n", __func__);
		dst_vf->src_fmt = src_vf->src_fmt;
		dst_vf->signal_type = src_vf->signal_type;
		dst_vf->source_type = src_vf->source_type;
	}

	dst_vf->zorder = frames_info->disp_zorder;
	dst_vf->canvas0Addr = -1;
	dst_vf->canvas1Addr = -1;
	if (dev->dev_choice == COMPOSER_WITH_VICP) {
		if (fbcout_en) {
			dst_vf->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
			dst_vf->compWidth = dst_buf->buf_w;
			dst_vf->compHeight = dst_buf->buf_h;
			dst_vf->compHeadAddr = dst_buf->afbc_head_addr;
			dst_vf->compBodyAddr = dst_buf->afbc_body_addr;
			if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3X) {
				dst_vf->fgs_valid = false;
				dst_vf->fgs_table_adr = 0;
				if (lossy_compress_rate) {
					dst_vf->type |= VIDTYPE_COMPRESS_LOSS;
					dst_vf->vf_lossycomp_param.lossy_mode = 1;
					dst_vf->vf_lossycomp_param.burst_length_add_en = 0;
					dst_vf->vf_lossycomp_param.burst_length_add_value = 2;
					dst_vf->vf_lossycomp_param.quant_diff_root_leave = 2;
				} else {
					dst_vf->vf_lossycomp_param.lossy_mode = 0;
					dst_vf->vf_lossycomp_param.burst_length_add_en = 0;
					dst_vf->vf_lossycomp_param.burst_length_add_value = 2;
					dst_vf->vf_lossycomp_param.quant_diff_root_leave = 2;
				}
				if (dst_vf->compHeadAddr >= (unsigned long long)ADDR_VALUE_8G ||
					dst_vf->compBodyAddr >= (unsigned long long)ADDR_VALUE_8G)
					dst_vf->vf_lossycomp_param.ofset_burst4_en = 1;
				else
					dst_vf->vf_lossycomp_param.ofset_burst4_en = 0;
			}
		}

		if (vicp_shrink_mode >= VICP_SHRINK_MODE_MAX) {
			dst_vf->width = dst_buf->buf_w;
			dst_vf->height = dst_buf->buf_h;
		} else {
			dst_vf->width = dst_buf->buf_w >> (1 + vicp_shrink_mode);
			dst_vf->height = dst_buf->buf_h >> (1 + vicp_shrink_mode);
		}
	} else if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		dst_vf->width = common_para.output_para.pic_info.align_w;
		dst_vf->height = common_para.output_para.pic_info.align_h;
	} else {
		dst_vf->width = dst_buf->buf_w;
		dst_vf->height = dst_buf->buf_h;
	}
	vc_print(dev->index, PRINT_DEWARP,
			 "composer:vf_w: %d, vf_h: %d\n", dst_vf->width, dst_vf->height);
	if (composer_use_444) {
		dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
		dst_vf->canvas0_config[0].width = dst_vf->width * 3;
		dst_vf->canvas0_config[0].height = dst_vf->height;
		dst_vf->canvas0_config[0].block_mode = 0;
		dst_vf->plane_num = 1;

		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			dst_vf->canvas0_config[0].width = common_para.output_para.pic_info.align_w;
			dst_vf->canvas0_config[0].height = common_para.output_para.pic_info.align_h;
		}
	} else {
		dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
		dst_vf->canvas0_config[0].width = dst_vf->width;
		dst_vf->canvas0_config[0].height = dst_vf->height;
		dst_vf->canvas0_config[0].block_mode = 0;

		dst_vf->canvas0_config[1].phy_addr = dst_buf->phy_addr
			+ dst_vf->width * dst_vf->height;
		dst_vf->canvas0_config[1].width = dst_vf->width;
		dst_vf->canvas0_config[1].height = dst_vf->height >> 1;
		dst_vf->canvas0_config[1].block_mode = 0;
		dst_vf->plane_num = 2;

		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			dst_vf->canvas0_config[0].width = common_para.output_para.pic_info.align_w;
			dst_vf->canvas0_config[0].height = common_para.output_para.pic_info.align_h;
			dst_vf->canvas0_config[1].phy_addr = dst_buf->phy_addr +
				dst_vf->canvas0_config[0].width * dst_vf->canvas0_config[0].height;
			dst_vf->canvas0_config[1].width = dst_vf->canvas0_config[0].width;
			dst_vf->canvas0_config[1].height = dst_vf->canvas0_config[0].height >> 1;
		}
	}

	if (dev->dev_choice == COMPOSER_WITH_DEWARP &&
		(vframe_info_cur->crop_w > 0 || vframe_info_cur->crop_h > 0)) {
		if (src_vf) {
			//uvm
			if (src_vf->type & VIDTYPE_COMPRESS) {
				dewarp_src_w = src_vf->compWidth;
				dewarp_src_h = src_vf->compHeight;
			} else {
				dewarp_src_w = src_vf->width;
				dewarp_src_h = src_vf->height;
			}
			vc_print(dev->index, PRINT_DEWARP,
				"src_vf: compWidth:%d compHeight:%d w:%d h:%d.\n",
				src_vf->compWidth,
				src_vf->compHeight,
				src_vf->width,
				src_vf->height);
		} else {
			//dma
			dewarp_src_w = vframe_info_cur->buffer_w;
			dewarp_src_h = vframe_info_cur->buffer_h;
		}

		if (src_vf && is_src_crop_valid(src_vf->src_crop)) {
			dewarp_crop_top =
				MAX(vframe_info_cur->crop_y, src_vf->src_crop.top);
			dewarp_crop_left =
				MAX(vframe_info_cur->crop_x, src_vf->src_crop.left);
			dewarp_crop_bottom = MAX(dewarp_src_h - vframe_info_cur->crop_y
				- vframe_info_cur->crop_h, src_vf->src_crop.bottom);
			dewarp_crop_right = MAX(dewarp_src_w - vframe_info_cur->crop_x
				- vframe_info_cur->crop_w, src_vf->src_crop.right);
		} else {
			dewarp_crop_top = vframe_info_cur->crop_y;
			dewarp_crop_left = vframe_info_cur->crop_x;
			dewarp_crop_bottom = dewarp_src_h - vframe_info_cur->crop_y
				- vframe_info_cur->crop_h;
			dewarp_crop_right = dewarp_src_w - vframe_info_cur->crop_x
				- vframe_info_cur->crop_w;
		}
		dewarp_dst_w = common_para.output_para.pic_info.align_w;
		dewarp_dst_h = common_para.output_para.pic_info.align_h;

		if (vframe_info_cur->transform == VC_TRANSFORM_ROT_270) {
			dst_vf->crop[0] = dewarp_crop_right * dewarp_dst_h / dewarp_src_w;
			dst_vf->crop[1] = dewarp_crop_top * dewarp_dst_w / dewarp_src_h;
			dst_vf->crop[2] = dewarp_crop_left * dewarp_dst_h / dewarp_src_w;
			dst_vf->crop[3] = dewarp_crop_bottom * dewarp_dst_w / dewarp_src_h;
		} else if (vframe_info_cur->transform == VC_TRANSFORM_ROT_180) {
			dst_vf->crop[0] = dewarp_crop_bottom * dewarp_dst_h / dewarp_src_h;
			dst_vf->crop[1] = dewarp_crop_right * dewarp_dst_w / dewarp_src_w;
			dst_vf->crop[2] = dewarp_crop_top * dewarp_dst_h / dewarp_src_h;
			dst_vf->crop[3] = dewarp_crop_left * dewarp_dst_w / dewarp_src_w;
		} else if (vframe_info_cur->transform == VC_TRANSFORM_ROT_90 ||
					vframe_info_cur->transform == VC_TRANSFORM_FLIP_H_ROT_90 ||
					vframe_info_cur->transform == VC_TRANSFORM_FLIP_V_ROT_90) {
			dst_vf->crop[0] = dewarp_crop_left * dewarp_dst_h / dewarp_src_w;
			dst_vf->crop[1] = dewarp_crop_bottom * dewarp_dst_w / dewarp_src_h;
			dst_vf->crop[2] = dewarp_crop_right * dewarp_dst_h / dewarp_src_w;
			dst_vf->crop[3] = dewarp_crop_top * dewarp_dst_w / dewarp_src_h;
		}
		vc_print(dev->index, PRINT_DEWARP,
			"dst_vf->crop: top:%d left:%d bottom:%d right:%d.\n",
			dst_vf->crop[0],
			dst_vf->crop[1],
			dst_vf->crop[2],
			dst_vf->crop[3]);
	}

	vc_print(dev->index, PRINT_DEWARP,
		"canvas0_addr: 0x%lx, canvas0_w: %d, canvas0_h: %d.\n",
		dst_vf->canvas0_config[0].phy_addr,
		dst_vf->canvas0_config[0].width,
		dst_vf->canvas0_config[0].height);
	vc_print(dev->index, PRINT_DEWARP,
		"canvas1_addr:  0x%lx, canvas1_w: %d, canvas1_h: %d.\n",
		dst_vf->canvas0_config[1].phy_addr,
		dst_vf->canvas0_config[1].width,
		dst_vf->canvas0_config[1].height);
	dst_vf->repeat_count = 0;
	dst_vf->composer_info = composer_info;
	if (count == 1 && src_vf)
		dst_vf->duration = src_vf->duration;
	else
		dst_vf->duration = get_output_duration(dev);

	if (dev->last_dst_vf)
		dev->last_dst_vf->repeat_count += drop_count;
	else
		dst_vf->repeat_count += drop_count;
	dev->last_dst_vf = dst_vf;
	dev->last_frames = *received_frames;
	dev->fake_vf = *dev->last_dst_vf;

	if (dump_vframe != dev->vframe_dump_flag) {
		if (is_dec_vf || is_v4l_vf) {
			if (src_vf && src_vf->type & VIDTYPE_COMPRESS)
				vd_vframe_afbc_soft_decode(src_vf, 0);
			else
				dump_vf(dev->index, src_vf, 0);
		} else {
			dump_dma(dev->index, vframe_info_cur, addr);
		}

		if (dst_vf->type & VIDTYPE_COMPRESS)
			vd_vframe_afbc_soft_decode(dst_vf, 1);
		else
			dump_vf(dev->index, dst_vf, 1);

		dev->vframe_dump_flag = dump_vframe;
	}

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)dst_vf))
		vc_print(dev->index, PRINT_ERROR, "ready_q is full\n");
	vf_notify_receiver(dev->vf_provider_name,
		VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
	vc_print(dev->index, PRINT_PERFORMANCE,
		 "ready len=%d\n", kfifo_len(&dev->ready_q));

	atomic_set(&received_frames->on_use, false);
}

static void empty_ready_queue(struct composer_dev *dev)
{
	int repeat_count;
	int frame_index;
	bool is_composer;
	int i;
	struct file *file_vf;
	struct vframe_s *vf = NULL;

	vc_print(dev->index, PRINT_OTHER, "vc: empty ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &vf)) {
			if (!vf)
				break;
			repeat_count = vf->repeat_count;
			frame_index = vf->frame_index;
			is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
			file_vf = vf->file_vf;
			vc_print(dev->index, PRINT_OTHER,
				 "empty: repeat_count =%d, frame_index=%d\n",
				 repeat_count, frame_index);
			video_timeline_increase(dev, repeat_count + 1);
			if (!is_composer) {
				for (i = 0; i <= repeat_count; i++) {
					fput(file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			} else {
				videocom_vf_put(vf, dev);
			}
		}
	}
}

static void video_wait_decode_fence(struct composer_dev *dev,
				    struct vframe_s *vf)
{
	struct dma_fence *fence_tmp;

	if (!vf) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s get vf is NULL\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_COUNTER("video_composer_wait_Dec_fence_frame_index", vf->frame_index);
	ATRACE_COUNTER("video_composer_wait_Dec_fence_frame_index", 0);
	ATRACE_COUNTER("video_composer_wait_Dec_fence_timestamp",
		div_u64(vf->timestamp, 1000000000));
	ATRACE_COUNTER("video_composer_wait_Dec_fence_timestamp", 0);
#endif

	vc_print(dev->index, PRINT_FENCE, "%s: frame_index=%d, timestamp:%lld.\n",
		__func__, vf->frame_index, div_u64(vf->timestamp, 1000000000));

	fence_tmp = vf->fence;
	if (fence_tmp) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(fence_tmp, false, 2000);

		vc_print(dev->index, PRINT_FENCE,
			 "%s, fence %lx, state: %d, wait cost time: %lld ns\n",
			 __func__, (ulong)fence_tmp, ret,
			 local_clock() - timestamp);
		ret = dma_fence_get_status(fence_tmp);
		if (ret > 0)
			vf->dec_fence_status = DEC_FENCE_SUCCESS;
		else if (ret < 0)
			vf->dec_fence_status = DEC_FENCE_ERR;
		else
			vc_print(dev->index, PRINT_ERROR, "dec error, fence timeout\n");
	} else {
		vc_print(dev->index, PRINT_FENCE,
			 "decoder fence is NULL\n");
	}
}

static void video_wait_sr_fence(struct composer_dev *dev,
				    struct dma_fence *fence)
{
	if (fence) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(fence, false, 2000);

		vc_print(dev->index, PRINT_FENCE,
			 "%s, sr fence %lx, state: %d, wait cost time:%lldns\n",
			 __func__, (ulong)fence, ret,
			 local_clock() - timestamp);
	} else {
		vc_print(dev->index, PRINT_FENCE,
			 "sr fence is NULL\n");
	}
}

static bool check_vf_has_afbc(struct composer_dev *dev, struct file *file_vf)
{
	struct vframe_s *vf = NULL;

	vf = get_vf_from_file(dev, file_vf, false);
	if (!vf)
		return false;

	if (vf->type & VIDTYPE_COMPRESS)
		return true;

	return false;
}

bool get_lowlatency_mode(void)
{
	return use_low_latency;
}
EXPORT_SYMBOL(get_lowlatency_mode);

static unsigned int get_vf_ds_ratio(struct composer_dev *dev, struct vframe_s *vf)
{
	unsigned int ds_ratio = 0;
	unsigned int hdctds_ratio = 0;
	unsigned int src_fmt = 2;
	unsigned int skip = 0;
	bool need_ds = false;

	if ((vf->type & VIDTYPE_VIU_422) && !(vf->type & 0x10000000)) {
		src_fmt = 0;
		need_ds = true;
		/*422 is one plane, post not support, need pre out nv21*/
	} else if ((vf->type & VIDTYPE_VIU_NV21) || (vf->type & 0x10000000)) {
		/*hdmi in dw is nv21 VIDTYPE_DW_NV21*/
		src_fmt = 2;
	}

	if (vf->type & VIDTYPE_INTERLACE) {
		if (src_fmt == 2) {
			skip = 1;
		} else if (src_fmt == 0) {
			need_ds = true;
		/*hdmiin output, In the first half of the line*/
			if (vf->width > 960 || (vf->height >> 1) > 540)
				hdctds_ratio = 1;
		}
	} else {
		if (vf->width > 1920 || vf->height > 1080) {
			hdctds_ratio = 1;
			skip = 1;
		} else if (vf->width > 960 || vf->height > 540) {
			if (src_fmt == 0) {
				/*hdmi in always use ds*/
				hdctds_ratio = 1;
			} else {
				/*decoder use mif skip for save ddr*/
				hdctds_ratio = 0;
				skip = 1;
				vc_print(dev->index, PRINT_OTHER, "1080p use mif skip\n");
			}
		}
	}

	if (hdctds_ratio || skip || need_ds) {
		ds_ratio = hdctds_ratio;
		if (skip)
			ds_ratio = ds_ratio + 1;

		if (need_ds && (vf->type & VIDTYPE_COMPRESS))
			ds_ratio = (vf->compWidth / vf->width) >> 1;
	} else {
		if (vf->type & VIDTYPE_COMPRESS) {
			if (vf->width == vf->compWidth)
				ds_ratio = 0;
			else if (vf->width >= (vf->compWidth >> 1))
				ds_ratio = 1;
			else if (vf->width >= (vf->compWidth >> 2))
				ds_ratio = 2;
			else
				ds_ratio = 3;
		}
	}
	vc_print(dev->index, PRINT_OTHER, "skip=%d, need_ds=%d, src_fmt=%d.\n",
		skip, need_ds, src_fmt);

	return ds_ratio;
}

static bool check_mosaic_22(struct composer_dev *dev, struct received_frames_t *received_frames)
{
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	int a[4];
	int i = 0;
	struct frames_info_t *f = &received_frames->frames_info;
	struct frame_info_t *frame_info;
	int half_w;
	int half_h;

	if (!dev->support_mosaic)
		return false;

	if (received_frames->frames_info.frame_count != 4)
		return false;

	if (dev->vinfo_w == 0) {
		video_composer_vinfo = get_current_vinfo();
		if (IS_ERR_OR_NULL(video_composer_vinfo))
			video_composer_vinfo = &vinfo;

		dev->vinfo_w = video_composer_vinfo->width;
		dev->vinfo_h = video_composer_vinfo->height;
	}

	if (dev->vinfo_w == 0 || dev->vinfo_h == 0)
		return false;

	half_w = dev->vinfo_w >> 1;
	half_h = dev->vinfo_h >> 1;

	for (i = 0; i < 4; i++) {
		frame_info = &f->frame_info[i];
		vc_print(dev->index, PRINT_AXIS,
			"check mosaic: i=%d: %d %d %d %d\n",
			i,
			frame_info->dst_x,
			frame_info->dst_y,
			frame_info->dst_w,
			frame_info->dst_h);
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6X && f->frame_count == 4 && dev->index == 0) {
		vc_print(dev->index, PRINT_AXIS, "check mosaic ok.\n");
		return true;
	}

	/*check all w h <= 1/2 vinfo*/
	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_w > half_w || f->frame_info[i].dst_h > half_h)
			return false;
		if (!check_vf_has_afbc(dev, received_frames->file_vf[i])) {
			vc_print(dev->index, PRINT_AXIS, "vf has no afbc\n");
			return false;
		}
	}

	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y < half_h)
			a[0] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y < half_h)
			a[1] = i;
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y >= half_h)
			a[2] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y >= half_h)
			a[3] = i;
	}

	/*check quadrant 1*/
	frame_info = &f->frame_info[a[0]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 2*/
	frame_info = &f->frame_info[a[1]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	/*check quadrant 3*/
	frame_info = &f->frame_info[a[2]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 4*/
	frame_info = &f->frame_info[a[3]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	vc_print(dev->index, PRINT_AXIS, "check mosaic ok\n");

	return true;
}

static struct vframe_s *get_enhance_vf_pointer(struct composer_dev *dev,
	struct vframe_s *vf)
{
	int i;
	struct vframe_s *enhance_vf;

	if (dev->kfifo_need_initialize) {
		vc_print(dev->index, PRINT_QUEUE_STATUS, "init buffer free_q for lcevc\n");
		dev->kfifo_need_initialize = false;
		for (i = 0; i < DMA_BUF_COUNT; i++) {
			if (!kfifo_put(&dev->free_q, &dev->enhance_vf[i]))
				vc_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
		}
	}

	if (!kfifo_get(&dev->free_q, &enhance_vf)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: free_q is empty, can not provide enhance_vf\n");
		enhance_vf = NULL;
	} else {
		vc_print(dev->index, PRINT_OTHER,
			 "task: get_enhance_vf:%px\n", enhance_vf);
		memcpy(enhance_vf, vf, sizeof(struct vframe_s));
	}

	return enhance_vf;
}

static struct  uvm_lcevc_frame_info *vc_get_lcevc_data(struct composer_dev *dev,
						     struct file *file_vf)
{
	struct uvm_lcevc_hook_data *hook_data;
	struct uvm_lcevc_frame_info *lcevc_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "vc get lcevc data fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_LCEVC);
	if (!uhmod) {
		vc_print(dev->index, PRINT_OTHER, "%s:dma file file_private_data is NULL 1\n",
			__func__);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vc_print(dev->index, PRINT_ERROR, "%s: dma file file_private_data is NULL 2\n",
			__func__);
		return NULL;
	}
	hook_data = uhmod->arg;
	lcevc_data = &hook_data->lcevc_vframe;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_LCEVC);

	return lcevc_data;
}

static bool set_vf_lcevc_data(struct composer_dev *dev,
	struct vframe_s *enhance_vf, struct uvm_lcevc_frame_info *lcevc_data)
{
	int len, i;

	if (!enhance_vf || !lcevc_data)
		return false;
	enhance_vf->width = lcevc_data->width;
	enhance_vf->height = lcevc_data->height;
	enhance_vf->canvas0Addr = -1;
	enhance_vf->canvas0_config[0].phy_addr = lcevc_data->y_physical_addr;
	enhance_vf->canvas1Addr = -1;
	enhance_vf->canvas0_config[1].phy_addr = lcevc_data->uv_physical_addr;

	enhance_vf->canvas0_config[0].width = lcevc_data->stride;
	enhance_vf->canvas0_config[0].height = enhance_vf->height;
	enhance_vf->canvas0_config[1].width = lcevc_data->stride;
	enhance_vf->canvas0_config[1].height = enhance_vf->canvas0_config[0].height;

	enhance_vf->plane_num = 1;
	enhance_vf->type = VIDTYPE_VIU_FIELD
		| VIDTYPE_VIU_422
		| VIDTYPE_VIU_SINGLE_PLANE;
	enhance_vf->flag = VFRAME_FLAG_VIDEO_LINEAR
		| VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
	enhance_vf->type_ext = VIDTYPE_EXT_LCEVC;
	enhance_vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10 | FULL_PACK_422_MODE;
	memcpy(&enhance_vf->scaler_coeff, &lcevc_data->upsample_kernel, sizeof(struct vf_lcevc_t));
	len = enhance_vf->scaler_coeff.len;
	vc_print(dev->index, PRINT_INDEX_DISP, "task: coeff len:%d", len);
	for (i = 0; i < len; i++) {
		vc_print(dev->index, PRINT_INDEX_DISP,
				"task: coeff[%d] %d %d\n",
				i,
				enhance_vf->scaler_coeff.k[0][i],
				enhance_vf->scaler_coeff.k[1][i]);
	}
	vc_print(dev->index, PRINT_INDEX_DISP,
		"task: enhance_vf width:%d, height:%d, enhance_vf yuv addr:%px\n",
		enhance_vf->width,
		enhance_vf->height,
		(void *)enhance_vf->canvas0_config[0].phy_addr);

	return true;
}

static const u32 bt2020_primaries[3][2] = {
	{0.17 * INORM + 0.5, 0.797 * INORM + 0.5},      /* G */
	{0.131 * INORM + 0.5, 0.046 * INORM + 0.5},     /* B */
	{0.708 * INORM + 0.5, 0.292 * INORM + 0.5},     /* R */
};

static const u32 bt2020_white_point[2] = {
	0.3127 * INORM + 0.5, 0.3290 * INORM + 0.5
};

static void set_vf_hdr_info(struct composer_dev *dev, struct vframe_s *vf)
{
	if (!vf) {
		vc_print(dev->index, PRINT_ERROR, "%s:vf is NULL\n", __func__);
		return;
	}

	vf->signal_type = 0x20091009;
	vf->prop.master_display_colour.present_flag = 1;

	memcpy(vf->prop.master_display_colour.primaries, bt2020_primaries, sizeof(u32) * 6);
	memcpy(vf->prop.master_display_colour.white_point, bt2020_white_point, sizeof(u32) * 2);

	vf->prop.master_display_colour.luminance[0] = 1000;
	vf->prop.master_display_colour.luminance[1] = 50;
	vf->prop.master_display_colour.content_light_level.max_content = 0;
	vf->prop.master_display_colour.content_light_level.max_pic_average = 0;
}

static bool detect_composer_usage(struct composer_dev *dev,
	struct received_frames_t *received_frames, bool *need_composer_ptr, bool *mosaic_22_ptr)
{
	struct vframe_s *vf = NULL;
	int count;
	u32 frame_transform = 0;
	struct file *file_vf = NULL;
	struct frames_info_t *frames_info = NULL;
	struct frame_info_t *frame_info = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;

	count = received_frames->frames_info.frame_count;
	if (count == 1) {
		if ((dev->index == 0 && force_composer) ||
		    (dev->index == 1 && force_composer_pip))
			*need_composer_ptr = true;
		frame_transform =
			received_frames->frames_info.frame_info[0].transform;
		if (frame_transform == VC_TRANSFORM_ROT_90 ||
			frame_transform == VC_TRANSFORM_ROT_180 ||
			frame_transform == VC_TRANSFORM_ROT_270 ||
			frame_transform == VC_TRANSFORM_FLIP_H_ROT_90 ||
			frame_transform == VC_TRANSFORM_FLIP_V_ROT_90) {
			*need_composer_ptr = true;
			dev->need_rotate = true;
		} else {
			dev->need_rotate = false;
		}
	} else {
		dev->need_rotate = false;
		if (check_mosaic_22(dev, received_frames)) {
			*need_composer_ptr = false;
			*mosaic_22_ptr = true;
		} else {
			*need_composer_ptr = true;
		}
	}

	frames_info = &received_frames->frames_info;
	frame_info = frames_info->frame_info;
	file_vf = received_frames->file_vf[0];
	if (!file_vf) {
		vc_print(dev->index, PRINT_ERROR, "file_vf is NULL\n");
		return false;
	}
	detect_vf_type(frame_info, file_vf, &is_dec_vf, &is_v4l_vf);

	if (is_dec_vf || is_v4l_vf) {
		vf = get_vf_from_file(dev, file_vf, false);
		if (!vf) {
			vc_print(dev->index, PRINT_ERROR, "get NULL vf!!\n");
			return false;
		}
		if (dev->last_vf_from_dpss && !(vf->dpss_flg & 0x40))
			vf->dpss_flg |= 0x08;
		else if (!(vf->dpss_flg & 0x08))
			vf->dpss_flg |= 0x10;
	}
	if (vf && (vf->type & VIDTYPE_DI_PW || vf->di_flag & DI_FLAG_DI_PVPPLINK)) {
		vc_print(dev->index, PRINT_OTHER, "di_vf=%px type_ext=%x.\n", vf, vf->type_ext);
		if (vf->type_ext & VIDTYPE_EXT_DI_DO_ROTATE && count == 1) {
			*need_composer_ptr = false;
			dev->need_rotate = false;
			vc_print(dev->index, PRINT_OTHER, "di already do rotate, vc needn't do.\n");
		}
	}

	if (dev->output_duration >= 240 && dev->vinfo_w > 1920 && !*need_composer_ptr) {
		if (vf && (vf->flag & VFRAME_FLAG_GAME_MODE)) {
			vc_print(dev->index, PRINT_OTHER, "game mode no need force composer.\n");
			return true;
		}
		vc_print(dev->index, PRINT_AXIS, "fps > 240, need composer.\n");
		if (is_dec_vf || is_v4l_vf) {
			vc_print(dev->index, PRINT_OTHER,
					 "%s vf_height:%d vf_com_height:%d\n",
					 __func__, vf->height, vf->compHeight);
			if (vf->height > 1088 || vf->compHeight > 1088)
				*need_composer_ptr = true;
		} else {
			vc_print(dev->index, PRINT_OTHER,
				"%s: frame_info->height:%d\n",
				 __func__, frame_info->buffer_h);
			if (frame_info->buffer_h > 1088)
				*need_composer_ptr = true;
		}
	}
	return true;
}

static int config_crop_param(struct composer_dev *dev,
	struct received_frames_t *received_frames, struct vframe_s *vf)
{
	u32 pic_w;
	u32 pic_h;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct frames_info_t *frames_info = NULL;
	struct frame_info_t *frame_info = NULL;
	struct file *file_vf = NULL;

	if (!dev || !received_frames || !vf) {
		pr_info("vc: %s: NULL param.\n", __func__);
		return -1;
	}

	frames_info = &received_frames->frames_info;
	frame_info = frames_info->frame_info;
	file_vf = received_frames->file_vf[0];
	if (!file_vf)
		return -1;

	vc_print(dev->index, PRINT_AXIS,
		"frame_info crop: x y w h %d %d %d %d, pps:x y %d %d.\n",
		frame_info->crop_x,
		frame_info->crop_y,
		frame_info->crop_w,
		frame_info->crop_h,
		vf->dpss_pps_dsx,
		vf->dpss_pps_dsy);

	if (vf->dpss_flg & DPSS_FLG_USE_DW) {
		frame_info->crop_w <<= 2;
		frame_info->crop_h <<= 2;
	}

	if (is_src_crop_valid(vf->src_crop))
		vc_print(dev->index, PRINT_AXIS,
			"src_crop: %d %d %d %d\n",
			vf->src_crop.top,
			vf->src_crop.left,
			vf->src_crop.bottom,
			vf->src_crop.right);

	detect_vf_type(frame_info, file_vf, &is_dec_vf, &is_v4l_vf);
	if (is_dec_vf || is_v4l_vf) {
		if ((vf->type_original & VIDTYPE_COMPRESS) != 0) {
			pic_w = vf->compWidth;
			pic_h = vf->compHeight;
		} else {
			pic_w = vf->width;
			pic_h = vf->height;
		}
		vc_print(dev->index, PRINT_AXIS, "pic_w: %d, pic_h: %d.\n", pic_w, pic_h);
		if (frame_info->source_type == SOURCE_DTV_FIX_TUNNEL) {
			vf->flag |= VFRAME_FLAG_FIX_TUNNEL;
			vf->crop[0] = frame_info->crop_y;
			vf->crop[1] = frame_info->crop_x;
			vf->crop[2] = frame_info->crop_y +
				frame_info->crop_h;
			vf->crop[3] = frame_info->crop_x +
				frame_info->crop_w;
			vc_print(dev->index, PRINT_AXIS,
				"tunnel set vf crop:%d %d %d %d\n",
				vf->crop[0],
				vf->crop[1],
				vf->crop[2],
				vf->crop[3]);
		} else if ((pic_w > MAX(frame_info->reserved[0], frame_info->buffer_w)) ||
			(pic_h > MAX(frame_info->reserved[1], frame_info->buffer_h))) {
			/*omx receive w*h is small than actual w*h;such as 8k*/
			vf->crop[0] = 0;
			vf->crop[1] = 0;
			vf->crop[2] = 0;
			vf->crop[3] = 0;
			vc_print(dev->index, PRINT_AXIS,
				"crop info is error!\n");
		} else {
			vf->crop[0] = frame_info->crop_y;
			vf->crop[1] = frame_info->crop_x;
			vf->crop[2] = (pic_h >> vf->dpss_pps_dsy)
				- frame_info->crop_h
				- frame_info->crop_y;
			vf->crop[3] = (pic_w >> vf->dpss_pps_dsx)
				- frame_info->crop_w
				- frame_info->crop_x;
			vc_print(dev->index, PRINT_AXIS,
				"none-tunnel set vf crop:%d %d %d %d\n",
				vf->crop[0],
				vf->crop[1],
				vf->crop[2],
				vf->crop[3]);
		}
	} else {
		if (frame_info->type == 1) {
			vf->crop[0] = frame_info->crop_y;
			vf->crop[1] = frame_info->crop_x;
			vf->crop[2] = frame_info->buffer_h
				- frame_info->crop_h
				- frame_info->crop_y;
			vf->crop[3] = frame_info->buffer_w
				- frame_info->crop_w
				- frame_info->crop_x;
		} else {
			vf->crop[0] = 0;
			vf->crop[1] = 0;
			vf->crop[2] = 0;
			vf->crop[3] = 0;
		}
	}

	return 0;
}

static int config_ai_param(struct composer_dev *dev,
	struct received_frames_t *received_frames, struct vframe_s *vf)
{
	struct frames_info_t *frames_info = NULL;
	struct frame_info_t *frame_info = NULL;
	struct file *file_vf = NULL;
	struct vf_nn_sr_t *srout_data = NULL;
	u32 nn_status;
	struct vf_aiface_t *aiface_info = NULL;
	struct vf_aicolor_t *aicolor_info = NULL;

	if (!dev || !received_frames || !vf || !vf->vc_private) {
		pr_info("vc: %s: NULL param.\n", __func__);
		return -1;
	}

	frames_info = &received_frames->frames_info;
	frame_info = frames_info->frame_info;
	file_vf = received_frames->file_vf[0];
	if (!file_vf)
		return -1;

	if (vf->hf_info && !nn_bypass)
		srout_data = vc_get_hfout_data(dev, file_vf);
	if (srout_data) {
		video_wait_sr_fence(dev, srout_data->fence);
		nn_status = srout_data->nn_status;
		if (vf->hf_info->phy_addr != 0 &&
			vf->hf_info->width != 0 &&
			vf->hf_info->height != 0 &&
			(nn_status == NN_WAIT_DOING ||
			nn_status == NN_START_DOING ||
			nn_status == NN_DONE)) {
			vf->vc_private->srout_data = srout_data;
			vf->vc_private->flag |= VC_FLAG_AI_SR;
		}
	}

	aiface_info = aiface_info_adjust(dev,
					file_vf,
					vf->frame_index,
					frame_info->dst_x,
					frame_info->dst_y,
					frame_info->dst_w,
					frame_info->dst_h);
	if (aiface_info) {
		vf->vc_private->aiface_info = aiface_info;
		vf->vc_private->flag |= VC_FLAG_AI_FACE;
		vc_print(dev->index, PRINT_AIFACE,
			"frame_index = %d, aiface_value_count = %d.\n",
			vf->frame_index, aiface_info->aiface_value_count);
	}

	aicolor_info = vc_get_aicolor_info(dev, file_vf);
	if (aicolor_info) {
		nn_status = aicolor_info->nn_status;
		if (nn_status == NN_WAIT_DOING ||
			nn_status == NN_START_DOING ||
			nn_status == NN_DONE) {
			vf->vc_private->aicolor_info = aicolor_info;
			vf->vc_private->flag |= VC_FLAG_AI_COLOR;
		}
	}

	return 0;
}

static void set_frc_pattern(struct composer_dev *dev, struct vframe_s *vf)
{
	int duration;

	if (!vf->vc_private)
		return;
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER) {
		vf->vc_private->frc_operation_mode = VC_FRC_FLAG_1_1;
		new_afr_pulldown = 0;
		vc_print(dev->index, PRINT_OTHER, "%s:HDMI mode, FRC full function", __func__);
		return;
	}

	duration = find_nearest_duration(dev, vf->duration);
	switch (duration) {
	case 3200:
	case 3203:
	case 3840:
		if (check_frc_n2m_status())
			new_afr_pulldown = 1;
		vf->vc_private->frc_operation_mode = VC_FRC_FLAG_1_2;
		break;
	case 4000:
	case 4004:
		if (check_frc_n2m_status())
			new_afr_pulldown = 1;
		vf->vc_private->frc_operation_mode = VC_FRC_FLAG_2_5;
		break;
	default:
		vf->vc_private->frc_operation_mode = VC_FRC_FLAG_BYPASS;
		break;
	}
	vc_print(dev->index, PRINT_OTHER, "%s: set frc mode:%d\n",
		__func__, vf->vc_private->frc_operation_mode);
}

static int is_pulldown_1_1(struct composer_dev *dev, int duration)
{
	int ret = 0;
	int vsync_pts = 0;

	if (vd_vsync_pts_inc_scale[dev->index])
		vsync_pts = vd_vsync_pts_inc_scale_base[dev->index] /
			vd_vsync_pts_inc_scale[dev->index];

	vc_print(dev->index, PRINT_OTHER, "vsync_pts=%d, duration=%d.\n",
		vsync_pts, duration);

	switch (duration) {
	case 800:
	case 801:
		if (vsync_pts == 120)
			ret = 1;
		break;
	case 960:
		if (vsync_pts == 100)
			ret = 1;
		break;
	case 1600:
	case 1601:
		if (vsync_pts == 60)
			ret = 1;
		break;
	case 1920:
		if (vsync_pts == 50)
			ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void calculate_low_latency_case(struct composer_dev *dev, struct vframe_s *vf)
{
	bool enable_prelink = false;
	int vsync_pts = 0;
	u64 elapsed_ns = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	enable_prelink = dim_get_pre_link();
#endif

	if (vd_vsync_pts_inc_scale[dev->index])
		vsync_pts = vd_vsync_pts_inc_scale_base[dev->index] /
			vd_vsync_pts_inc_scale[dev->index];

	if (dev->is_tv_path) {
		if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI && vsync_pts >= 100) {
			dev->low_latency_case = 2;
			vc_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get, dv tv path case.\n");
		} else if (is_pulldown_1_1(dev, vf->duration)) {
			vpp_lowlatency_wakeup();
			dev->low_latency_case = 1;
			vc_print(dev->index, PRINT_OTHER,
				"case1:tv path 1:1 mode use low latency2 mode.\n");
		} else if ((vf->flag & VFRAME_FLAG_GAME_MODE)) {
			dev->low_latency_case = 2;
			vc_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get, game mode case.\n");
		} else {
			dev->low_latency_case = 0;
			vc_print(dev->index, PRINT_OTHER,
				"case3:tv path normal to get.\n");
		}
	} else {
		if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI && vsync_pts >= 100) {
			dev->low_latency_case = 2;
			vc_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get,dv local play case.\n");
		} else if ((enable_prelink &&
			!(vf->type_original & VIDTYPE_TYPEMASK)) ||
			(!is_panel_output() && !(vf->type_original & VIDTYPE_TYPEMASK) &&
			get_cpu_type() != MESON_CPU_MAJOR_ID_S5)) {
			vpp_lowlatency_wakeup();
			dev->low_latency_case = 1;
			vc_print(dev->index, PRINT_OTHER,
				"case1:P and prelink vc use low latency2 mode.\n");
		}
	}

	if (dev->low_latency_case == 2)
		next_vsync_wakeup_vpp_to_get = 1;
	if (vd_vsync_pts_inc_scale_base[dev->index])
		elapsed_ns = div_u64(1000000000LL *
			vd_vsync_pts_inc_scale[dev->index],
			vd_vsync_pts_inc_scale_base[dev->index]);
	next_isr_spec_time = isr_spec_time.tv_sec * 1000000000LL +
		isr_spec_time.tv_nsec + elapsed_ns;
	vc_print(dev->index, PRINT_OTHER,
		"elapsed_ns=%llu, next_isr_spec_time=%llu\n",
		elapsed_ns, next_isr_spec_time);
}

static void video_composer_task(struct composer_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *enhance_vf = NULL;
	struct file *file_vf = NULL;
	struct file *fence_file = NULL;
	struct frame_info_t *frame_info = NULL;
	struct received_frames_t *received_frames = NULL;
	struct frames_info_t *frames_info = NULL;
	int count;
	u32 frame_transform = 0;
	bool need_composer = false;
	int ready_count = 0;
	unsigned long phy_addr;
	u64 time_us64;
	struct vframe_s *vf_ext = NULL;
	bool is_dec_vf = false, is_v4l_vf = false, is_repeat_vf = false;
	u64 delay_time1;
	u64 delay_time2;
	u64 now_time;
	struct vd_prepare_s *vd_prepare = NULL;
	size_t usage = 0;
	bool do_mosaic_22 = false;
	struct uvm_lcevc_frame_info *lcevc_data;
	bool enable_prelink = false;
	bool enable_dct = false;
	unsigned int ds_ratio = 0;
	int ret = 0;

	if (!kfifo_peek(&dev->receive_q, &received_frames)) {
		vc_print(dev->index, PRINT_ERROR, "task: peek failed\n");
		return;
	}

	if (IS_ERR_OR_NULL(received_frames)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: get received_frames is NULL\n");
		return;
	}

	count = received_frames->frames_info.frame_count;
	time_us64 = received_frames->time_us64;

	if (count == 1)
		frame_transform = received_frames->frames_info.frame_info[0].transform;

	fence_file = received_frames->fence_file[0];
	if (video_wait_file_fence(dev, fence_file) == 0)
		return;

	if (!detect_composer_usage(dev, received_frames, &need_composer, &do_mosaic_22)) {
		vc_print(dev->index, PRINT_ERROR,
			 "task: fail to get need_composer status.\n");
		return;
	}

	if (!need_composer && !do_mosaic_22) {
		frames_info = &received_frames->frames_info;
		frame_info = frames_info->frame_info;
		phy_addr = received_frames->phy_addr[0];
		vc_print(dev->index, PRINT_OTHER,
			 "task:frame_cnt=%d,z=%d,index=%d,receive_q len=%d\n",
			 frames_info->frame_count,
			 frames_info->disp_zorder,
			 dev->index,
			 kfifo_len(&dev->receive_q));
		file_vf = received_frames->file_vf[0];
		if (!file_vf) {
			vc_print(dev->index, PRINT_ERROR, "file_vf is NULL\n");
			return;
		}

		detect_vf_type(frame_info, file_vf, &is_dec_vf, &is_v4l_vf);
		/*check repeat vframe*/
		if (dev->last_file == file_vf && (is_dec_vf || is_v4l_vf))
			is_repeat_vf = true;

		if (is_dec_vf || is_v4l_vf) {
			vf = get_vf_from_file(dev, file_vf, false);
			if (!vf) {
				vc_print(dev->index, PRINT_ERROR, "%s get vf is NULL\n", __func__);
				return;
			}
			if (!is_repeat_vf)
				video_wait_decode_fence(dev, vf);
		} else {
			vc_print(dev->index, PRINT_OTHER, "%s dma buffer not vf\n", __func__);
		}

		if (!kfifo_get(&dev->receive_q, &received_frames)) {
			vc_print(dev->index, PRINT_ERROR, "task: get failed\n");
			return;
		}

		if (is_repeat_vf) {
			vd_prepare = dev->vd_prepare_last;
		} else {
			vd_prepare = vd_prepare_data_q_get(dev);
			if (!vd_prepare) {
				vc_print(dev->index, PRINT_ERROR,
					 "%s: get prepare_data failed.\n",
					 __func__);
				return;
			}

			if (is_dec_vf || is_v4l_vf) {
				if (!vf) {
					vc_print(dev->index, PRINT_ERROR, "vf is NULL\n");
					return;
				}
				vd_prepare->src_frame = vf;
				vd_prepare->src_frame->file_vf = file_vf;
				vd_prepare->dst_frame = *vf;
			} else {/*dma buf*/
				vd_prepare->src_frame = &vd_prepare->dst_frame;
				vd_prepare->src_frame->file_vf = file_vf;
			}
		}
		vf = &vd_prepare->dst_frame;
		vf->axis[0] = frame_info->dst_x;
		vf->axis[1] = frame_info->dst_y;
		vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
		vf->composer_info = NULL;

		ret = config_crop_param(dev, received_frames, vf);
		if (ret)
			vc_print(dev->index, PRINT_ERROR, "config crop param failed.\n");

		vf->zorder = frames_info->disp_zorder;
		vf->file_vf = file_vf;
		//vf->zorder = 1;
		vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		//mirror frame
		if (frame_transform == VC_TRANSFORM_FLIP_H) {
			if (vf->flag & VFRAME_FLAG_MIRROR_H)
				vf->flag &= ~VFRAME_FLAG_MIRROR_H;
			else
				vf->flag |= VFRAME_FLAG_MIRROR_H;
		} else if (frame_transform == VC_TRANSFORM_FLIP_V) {
			if (vf->flag & VFRAME_FLAG_MIRROR_V)
				vf->flag &= ~VFRAME_FLAG_MIRROR_V;
			else
				vf->flag |= VFRAME_FLAG_MIRROR_V;
		}
		vf->pts_us64 = time_us64;
		vf->disp_pts = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		enable_prelink = dim_get_pre_link();
		enable_dct = dim_is_dct_on();
#endif
		if (enable_prelink &&
			!IS_DI_PRELINK(vf->di_flag) &&
			!IS_DI_PSTLINK(vf->di_flag) &&
			!IS_DI_PLINK_BYPASS(vf->di_flag) &&
			!(vf->type & VIDTYPE_INTERLACE)) {
			vc_print(dev->index, PRINT_OTHER, "need set ds_ratio.\n");
			ds_ratio = get_vf_ds_ratio(dev, vf);
			if (!enable_dct) {
				ds_ratio = DI_FLAG_DCT_DS_RATIO_MAX;
				vc_print(dev->index, PRINT_OTHER, "no dct reset ds_ratio.\n");
			}
			ds_ratio = ds_ratio << DI_FLAG_DCT_DS_RATIO_BIT;
			ds_ratio &= DI_FLAG_DCT_DS_RATIO_MASK;
			vf->di_flag |= DI_FLAG_DI_PVPPLINK_BYPASS | DI_FLAG_DI_BYPASS;
			vf->di_flag &= ~DI_FLAG_DCT_DS_RATIO_MASK;
			vf->di_flag |= ds_ratio;
		}

		if (frame_info->type == 1 && !(is_dec_vf || is_v4l_vf)) {
			if (frame_info->source_type == SOURCE_HWC_CREAT_ION)
				vf->source_type = VFRAME_SOURCE_TYPE_HWC;
			vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER_DMA;
			vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
			if (!(vf->dpss_flg & 0x08))
				vf->dpss_flg |= 0x10;
			vf->canvas0Addr = -1;
			vf->canvas0_config[0].phy_addr = phy_addr;

			vc_print(dev->index, PRINT_PATTERN,
				"buffer: format = %d, w*h = %d*%d, deal: w*h = %d*%d.\n",
				frame_info->buffer_format,
				frame_info->buffer_w,
				frame_info->buffer_h,
				frame_info->reserved[0],
				frame_info->reserved[1]);
			if (frame_info->buffer_w > frame_info->reserved[0])
				vf->canvas0_config[0].width = frame_info->buffer_w;
			else
				vf->canvas0_config[0].width = frame_info->reserved[0];
			if (frame_info->buffer_format == YUV444) {
				vc_print(dev->index, PRINT_OTHER, "buffer_format_t YUV444\n");
				vf->canvas0_config[0].width *= 3;
			} else if (frame_info->buffer_format == YUV444_10BIT) {
				vc_print(dev->index, PRINT_OTHER,
					"buffer_format_t HDR10 YUV444_10bit\n");
				vf->canvas0_config[0].width *= 4;
				set_vf_hdr_info(dev, vf);
			}
			if (frame_info->buffer_h > frame_info->reserved[1])
				vf->canvas0_config[0].height = frame_info->buffer_h;
			else
				vf->canvas0_config[0].height = frame_info->reserved[1];

			vf->canvas1Addr = -1;
			vf->canvas0_config[1].phy_addr = phy_addr
				+ vf->canvas0_config[0].width * vf->canvas0_config[0].height;
			vf->canvas0_config[1].width = vf->canvas0_config[0].width;
			vf->canvas0_config[1].height = vf->canvas0_config[0].height;
			vf->width = frame_info->buffer_w;
			vf->height = frame_info->buffer_h;
#ifdef CONFIG_AMLOGIC_UVM_CORE
			if (dmabuf_is_uvm(file_vf->private_data) &&
				meson_uvm_get_usage(file_vf->private_data, &usage) < 0)
				vc_print(dev->index, PRINT_ERROR,
					"%s:meson_uvm_get_usage fail.\n", __func__);
#endif
			if (frame_info->buffer_format == YUV444) {
				vf->plane_num = 1;
				vf->type = VIDTYPE_VIU_SINGLE_PLANE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_444;
				vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			} else if (frame_info->buffer_format == YUV444_10BIT) {
				vf->plane_num = 1;
				vf->type = VIDTYPE_VIU_SINGLE_PLANE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_444;
				vf->bitdepth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			} else if (frame_info->buffer_format == NV12_VC) {
				vf->plane_num = 2;
				vf->type = VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_NV12;
				vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			} else {
				vf->plane_num = 2;
				vf->type = VIDTYPE_PROGRESSIVE
					| VIDTYPE_VIU_FIELD
					| VIDTYPE_VIU_NV21;
				vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			}
			if (usage == UVM_USAGE_IMAGE_PLAY)
				vf->type |= VIDTYPE_PIC;
		}
		vc_print(dev->index, PRINT_AXIS,
			 "axis: %d %d %d %d, crop: %d %d %d %d\n",
			 vf->axis[0], vf->axis[1], vf->axis[2], vf->axis[3],
			 vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3]);
		vc_print(dev->index, PRINT_AXIS,
			 "vf_width: %d, vf_height: %d\n",
			 vf->width, vf->height);
		vc_print(dev->index, PRINT_AXIS,
			 "=========frame info:==========\n");
		vc_print(dev->index, PRINT_AXIS,
			 "frame axis x,y,w,h: %d %d %d %d\n",
			 frame_info->dst_x, frame_info->dst_y,
			 frame_info->dst_w, frame_info->dst_h);
		vc_print(dev->index, PRINT_AXIS,
			 "frame crop t,l,b,r: %d %d %d %d\n",
			 frame_info->crop_y, frame_info->crop_x,
			 frame_info->crop_h, frame_info->crop_w);
		vc_print(dev->index, PRINT_AXIS,
			 "frame buffer Width X Height: %d X %d\n",
			 vf->canvas0_config[0].width,
			 vf->canvas0_config[0].height);
		vc_print(dev->index, PRINT_AXIS,
			 "===============================\n");

#ifdef CONFIG_AMLOGIC_MEDIA_VRR
		vrr_crop_update_delay_line(frame_info->crop_y, frame_info->dst_y,
			frame_info->dst_w, frame_info->dst_h, VRR_AXIS);
#endif

		if (is_dec_vf) {
			/* copy to uvm vf */
			vf_ext = vf->uvm_vf;
		} else if (is_v4l_vf && vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			vf_ext = vf->vf_ext;
		}

		if (vf_ext) {
			vf_ext->axis[0] = vf->axis[0];
			vf_ext->axis[1] = vf->axis[1];
			vf_ext->axis[2] = vf->axis[2];
			vf_ext->axis[3] = vf->axis[3];
			vf_ext->crop[0] = vf->crop[0];
			vf_ext->crop[1] = vf->crop[1];
			vf_ext->crop[2] = vf->crop[2];
			vf_ext->crop[3] = vf->crop[3];
			vf_ext->zorder = vf->zorder;
			vf_ext->dec_set_screen_mode = vf->dec_set_screen_mode;
			vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER
				| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		} else {
			vc_print(dev->index, PRINT_OTHER, "no vf_ext.\n");
		}

		if (is_repeat_vf) {
			vf->repeat_count++;
			vc_print(dev->index, PRINT_FENCE,
				 "repeat =%d, frame_index=%d\n",
				 vf->repeat_count,
				 vf->frame_index);
		} else {
			if (is_dec_vf || is_v4l_vf) {
				vf->vc_private = vc_private_q_pop(dev);
				if (vf->vc_private)
					vf->vc_private->src_vf = vd_prepare->src_frame;

				/*only vd0 support ai feature*/
				if (dev->video_render_index == 0) {
					ret = config_ai_param(dev, received_frames, vf);
					if (ret)
						vc_print(dev->index, PRINT_ERROR,
							"config ai param failed");
					if (enable_frc_pattern)
						set_frc_pattern(dev, vf);
				}
				if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
					lcevc_data = vc_get_lcevc_data(dev, file_vf);
					if (!lcevc_data)
						vc_print(dev->index, PRINT_ERROR,
							"task: lcevc data is NULL!\n");
					else
						enhance_vf = get_enhance_vf_pointer(dev, vf);
					if (set_vf_lcevc_data(dev, enhance_vf, lcevc_data)) {
						vf->enhance_vf = enhance_vf;
						enhance_vf = vf;
						vf = vf->enhance_vf;
						vf->enhance_vf = enhance_vf;
					} else {
						vc_print(dev->index, PRINT_ERROR,
							"task: set lcevc data failed\n");
						vf->type_ext &= ~VIDTYPE_EXT_LCEVC;
					}
				}
			}
			dev->last_file = file_vf;
			vf->repeat_count = 0;
			dev->vd_prepare_last = vd_prepare;
			if (vf->flag & VFRAME_FLAG_GAME_MODE) {
				now_time = ktime_to_us(ktime_get());
				delay_time1 = now_time - vf->disp_pts_us64;
				delay_time2 = now_time - vf->timestamp;
				vc_print(dev->index, PRINT_PATTERN,
						 "total: time1=%lld,  time2=%lld\n",
						 delay_time1, delay_time2);
				if (delay_time1 > 1000)
					vc_print(dev->index, PRINT_PATTERN, "delay too long.\n");
			}

			if (!(vf->type & VIDTYPE_VIU_FIELD) &&
				(vf->type & VIDTYPE_INTERLACE_BOTTOM) == 0x3) {
				vf->type &= (~VIDTYPE_INTERLACE_BOTTOM);
				vf->type |= VIDTYPE_INTERLACE_TOP;
				vc_print(dev->index, PRINT_OTHER,
					"vc put bottom to top, vf->frame_index=%d\n",
					vf->frame_index);
			}

			vc_print(dev->index, PRINT_FENCE,
				"task: push to ready list: frame_index=%d\n", vf->frame_index);
			video_display_push_ready(dev, vf);
			if (vf->dpss_flg & 0x08)
				dev->last_vf_from_dpss = true;
			if (!kfifo_put(&dev->ready_q,
				       (const struct vframe_s *)vf))
				vc_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
			vf_notify_receiver(dev->vf_provider_name,
				VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
			if (is_video_process_in_thread()) {
				calculate_low_latency_case(dev, vf);
			} else {
				dev->low_latency_case = 0;
				vc_print(dev->index, PRINT_OTHER,
					"latency2 mode and normal logic.\n");
			}
			ready_count = kfifo_len(&dev->ready_q);

			/* dev->video_render_index == 5 means T7 dual screen mode */
			if (ready_count > 3 && dev->video_render_index == 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			else if (ready_count > 2 && dev->video_render_index != 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			else if (ready_count > 1 && dev->video_render_index != 5)
				vc_print(dev->index, PRINT_OTHER,
					 "ready len=%d\n", ready_count);
			vc_print(dev->index, PRINT_QUEUE_STATUS,
				 "ready len=%d\n", kfifo_len(&dev->ready_q));
		}
		dev->fake_vf = *vf;

		if (dump_vframe != dev->vframe_dump_flag) {
			if (vf->type & VIDTYPE_COMPRESS)
				vd_vframe_afbc_soft_decode(vf, 0);
			else
				dump_vf(dev->index, vf, 0);
			dev->vframe_dump_flag = dump_vframe;
		}
		atomic_set(&received_frames->on_use, false);
		if ((use_low_latency || (is_dec_vf && vf->flag & VFRAME_FLAG_GAME_MODE)) &&
			dev->index == 0)
			proc_lowlatency_frame(0);
	} else if (do_mosaic_22) {
		vframe_do_mosaic_22(dev);
		dev->last_file = NULL;
	} else {
		vframe_composer(dev);
		dev->last_file = NULL;
		dev->vd_prepare_last = NULL;
	}
}

static void video_composer_wait_event(struct composer_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 (kfifo_len(&dev->receive_q) > 0 &&
					  dev->composer_enabled) ||
					 dev->need_free_buffer ||
					 dev->need_unint_receive_q ||
					 dev->need_empty_ready ||
					 dev->thread_need_stop,
					 msecs_to_jiffies(5000));
}

void vpp_vsync_in(void)
{
	vc_print(0, PRINT_OTHER, "enter %s, %d\n",
		__func__, next_vsync_wakeup_vpp_to_get);
	if (is_video_process_in_thread() && next_vsync_wakeup_vpp_to_get) {
		allow_vpp_to_get = 1;
		vc_print(0, PRINT_OTHER,
			"vc start wake vpp to get,allow_vpp_to_get=%d\n", allow_vpp_to_get);
		vpp_lowlatency_wakeup();
		next_vsync_wakeup_vpp_to_get = 0;
	}
}

static int video_composer_thread(void *data)
{
	struct composer_dev *dev = data;

	vc_print(dev->index, PRINT_OTHER, "thread: started\n");
	dev->thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) == 0)
			video_composer_wait_event(dev);

		if (dev->need_empty_ready) {
			vc_print(dev->index, PRINT_OTHER,
				 "empty_ready_queue\n");
			dev->need_empty_ready = false;
			empty_ready_queue(dev);
			dev->last_file = NULL;
			dev->fake_vf.flag |= VFRAME_FLAG_FAKE_FRAME;
			dev->fake_vf.vf_ext = NULL;
			dev->fake_vf.uvm_vf = NULL;
			dev->fake_back_vf = dev->fake_vf;
			if (!kfifo_put(&dev->ready_q,
				       &dev->fake_back_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
		}

		if (dev->need_free_buffer) {
			dev->need_free_buffer = false;
			video_composer_uninit_buffer(dev);
			vc_print(dev->index, PRINT_OTHER,
				 "%s video composer release!\n", __func__);
			continue;
		}
		if (kthread_should_stop())
			break;

		if (!dev->enable_composer && dev->need_unint_receive_q) {
			receive_q_uninit(dev);
			dev->need_unint_receive_q = false;
			ready_q_uninit(dev);
			complete(&dev->task_done);
			continue;
		}
		if (kfifo_len(&dev->receive_q) > 0 && dev->enable_composer)
			video_composer_task(dev);
	}
	dev->thread_stopped = 1;
	vc_print(dev->index, PRINT_OTHER, "thread: exit\n");
	return 0;
}

static int video_composer_open(struct inode *inode, struct file *file)
{
	// coverity[illegal_address] I'm ensure it is ok.
	struct composer_dev *dev;
	struct video_composer_port_s *port = &ports[iminor(inode)];
	int i;
	struct sched_param param = {.sched_priority = 2};
	u32 layer_cap = 0;

	pr_info("%s iminor(inode) =%d\n", __func__, iminor(inode));
	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	mutex_lock(&video_composer_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&video_composer_mutex);
		pr_err("video_composer: instance %d is aleady opened",
		       port->index);
		return -EBUSY;
	}

	dev = vmalloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	if (!dev) {
		mutex_unlock(&video_composer_mutex);
		pr_err("video_composer: instance %d alloc dev failed",
		       port->index);
		return -ENOMEM;
	}
	dev->ge2d_para.context = NULL;

	dev->ge2d_para.count = 0;
	dev->ge2d_para.canvas_dst[0] = -1;
	dev->ge2d_para.canvas_dst[1] = -1;
	dev->ge2d_para.canvas_dst[2] = -1;
	dev->ge2d_para.canvas_scr[0] = -1;
	dev->ge2d_para.canvas_scr[1] = -1;
	dev->ge2d_para.canvas_scr[2] = -1;
	dev->ge2d_para.plane_num = 2;

	dev->dewarp_para.vc_index = dev->index;
	dev->dewarp_para.context = NULL;
	dev->dewarp_para.fw_load.size_32bit = 0;
	dev->dewarp_para.fw_load.phys_addr = 0;
	dev->dewarp_para.fw_load.virt_addr = NULL;
	dev->dewarp_para.vf_para = NULL;

	dev->buffer_status = UNINITIAL;

	dev->port = port;
	file->private_data = dev;
	dev->index = port->index;
	dev->need_free_buffer = false;
	dev->last_frames.frames_info.frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->thread_need_stop = false;
	dev->vframe_dump_flag = 0;

	memcpy(dev->vf_provider_name, port->name,
	       strlen(port->name) + 1);
	dev->video_render_index = vd_render_index_get(dev);
	port->video_render_index = dev->video_render_index;
	port->open_count++;
	do_gettimeofday(&dev->start_time);

	mutex_unlock(&video_composer_mutex);
	dev->kthread = kthread_create(video_composer_thread,
				      dev, dev->port->name);
	if (IS_ERR(dev->kthread)) {
		pr_err("video_composer_thread creat failed\n");
		return -ENOMEM;
	}
	init_waitqueue_head(&dev->wq);
	if (sched_setscheduler_nocheck(dev->kthread, SCHED_FIFO, &param))
		pr_err("vc:Could not set realtime priority.\n");

	wake_up_process(dev->kthread);
	//mutex_init(&dev->mutex_input);

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++)
		dev->received_frames[i].index = i;

	video_timeline_create(dev);

	if (dev->index == 0) {
		layer_cap = video_get_layer_capability();
		if (layer_cap & MOSAIC_MODE)
			dev->support_mosaic = true;
	}

	return 0;
}

static int video_composer_release(struct inode *inode, struct file *file)
{
	struct composer_dev *dev = file->private_data;
	struct video_composer_port_s *port = dev->port;
	int i = 0;
	int ret = 0;

	pr_info("%s enable=%d\n", __func__, dev->enable_composer);

	if (iminor(inode) >= video_composer_instance_num)
		return -ENODEV;

	if (dev->enable_composer) {
		ret = video_composer_set_enable(dev, 0);
		if (ret != 0)
			pr_err("%s, disable fail\n", __func__);
	}

	if (dev->kthread) {
		dev->thread_need_stop = true;
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
		dev->thread_need_stop = false;
	}

	mutex_lock(&video_composer_mutex);

	port->open_count--;

	mutex_unlock(&video_composer_mutex);
	while (1) {
		i++;
		if (dev->thread_stopped)
			break;
		usleep_range(9000, 10000);
		if (i > WAIT_THREAD_STOPPED_TIMEOUT) {
			pr_err("wait thread timeout\n");
			break;
		}
	}
	vfree(dev);
	dev = NULL;
	return 0;
}

static void disable_video_layer(struct composer_dev *dev, int val)
{
	pr_debug("dev->index =%d, val=%d", dev->index, val);
	if (dev->index == 0)
		_video_set_disable(val);
	else
		_videopip_set_disable(dev->index, val);
}

static void set_frames_info(struct composer_dev *dev,
			    struct frames_info_t *frames_info)
{
	u32 fence_fd;
	int i = 0;
	int j = 0;
	int type = -1;
	struct file *file_vf = NULL;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_us64;
	int axis[4];
	int ready_len = 0;
	bool current_is_sideband = false;
	bool is_dec_vf = false, is_v4l_vf = false;
	s32 sideband_type = -1;
	bool is_tvp = false;
	bool need_dw = false;
	char render_layer[16] = "";
	struct file *fence_file = NULL;
	struct sync_file *sync_file = NULL;
	struct dma_fence *fence_obj = NULL;

	if (!frames_info ||
	    frames_info->frame_count <= 0 ||
	    frames_info->frame_count > MAX_COMPOSER_COUNT) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: param is invalid.\n",
			 __func__);
		return;
	}

	if (!dev->composer_enabled) {
		for (j = 0; j < frames_info->frame_count; j++)
			frames_info->frame_info[j].composer_fen_fd = -1;
		vc_print(dev->index, PRINT_ERROR,
			 "set frame but not enable\n");
		return;
	}

	for (j = 0; j < frames_info->frame_count; j++) {
		if (frames_info->frame_info[j].type == 2) {
			ready_len = kfifo_len(&dev->ready_q);
			vc_print(dev->index, PRINT_OTHER,
				"sideband: zorder=%d, ready_len =%d\n",
				frames_info->disp_zorder, ready_len);
			frames_info->frame_info[j].composer_fen_fd = -1;
			sideband_type = frames_info->frame_info[j].sideband_type;
			axis[0] = frames_info->frame_info[j].dst_x;
			axis[1] = frames_info->frame_info[j].dst_y;
			axis[2] = frames_info->frame_info[j].dst_w
				+ axis[0] - 1;
			axis[3] = frames_info->frame_info[j].dst_h
				+ axis[1] - 1;
			set_video_window_ext(dev->index, axis);
			set_video_zorder_ext(dev->index,
						frames_info->disp_zorder);
			if (!dev->is_sideband && dev->received_count > 0) {
				vc_print(dev->index, PRINT_OTHER,
					 "non change to sideband:wake_up\n");
				dev->need_empty_ready = true;
				wake_up_interruptible(&dev->wq);
			}
			if (!dev->is_sideband) {
				set_vdx_blackout_policy(dev->index, 0);
				dev->select_path_done = false;
			}
			dev->is_sideband = true;
			current_is_sideband = true;
		}
	}
	if (!dev->select_path_done) {
		if (current_is_sideband) {
			if (dev->index == 0) {
				set_video_path_select("auto", 0);
				set_sideband_type(sideband_type, 0);
			}
		}
		vc_print(dev->index, PRINT_OTHER, "sideband_type =%d\n", sideband_type);
		dev->select_path_done = true;
	}
	if (current_is_sideband) {
		if (frames_info->frame_count > 1)
			vc_print(dev->index, PRINT_ERROR, "sideband count not 1\n");
		return;
	}

	if ((dev->is_sideband && !current_is_sideband) ||
	    dev->received_count == 0) {
		if (dev->is_sideband && !current_is_sideband) {
			set_vdx_blackout_policy(dev->index, 1);
			vc_print(dev->index, PRINT_OTHER, "sideband to none sideband\n");
		}
		dev->is_sideband = false;
		disable_video_layer(dev, 0);
		sprintf(render_layer, "video_render.%d", dev->video_render_index);
		set_video_path_select(render_layer, dev->index);
	}
	dev->is_sideband = false;

	time1 = dev->start_time;
	do_gettimeofday(&time2);
	time_us64 = (u64)1000000 * (time2.tv_sec - time1.tv_sec)
			+ time2.tv_usec - time1.tv_usec;

	/*time_vsync = (u64)1000000 * (time2.tv_sec - vsync_time.tv_sec)*/
	/*+ time2.tv_usec - vsync_time.tv_usec;*/

	if (frames_info->frame_count > MAX_COMPOSER_COUNT ||
	    frames_info->frame_count < 1) {
		vc_print(dev->index, PRINT_ERROR,
			 "vc: layer count %d\n", frames_info->frame_count);
		return;
	}

	j = 0;
	while (1) {
		for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frames[i].on_use))
				break;
		}
		if (i == FRAMES_INFO_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				vc_print(dev->index, PRINT_ERROR,
					"receive_q is full, wait timeout!\n");
				return;
			}
			usleep_range(1000 * receive_wait,
				     1000 * (receive_wait + 1));
			vc_print(dev->index, PRINT_ERROR, "receive_q is full, need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}

	fence_fd = video_timeline_create_fence(dev);
	if (fence_fd >= 0) {
		fence_file = fget(fence_fd);
		vc_print(dev->index, PRINT_FENCE,
				"create fence_fd=%d, fence_file=%px\n",
				fence_fd, fence_file);

		if (!IS_ERR_OR_NULL(fence_file))
			sync_file = (struct sync_file *)fence_file->private_data;

		if (!IS_ERR_OR_NULL(sync_file))
			fence_obj = sync_file->fence;

		if (fence_obj)
			vc_print(dev->index, PRINT_FENCE, "creat sync_file=%px, seqno=%lld\n",
					sync_file, fence_obj->seqno);
		else
			vc_print(dev->index, PRINT_FENCE, "fence file is NULL\n");

		if (!IS_ERR_OR_NULL(fence_file))
			fput(fence_file);
	}

	if (transform != -1) {
		for (j = 0; j < frames_info->frame_count; j++)
			frames_info->frame_info[j].transform = transform;
	}

	dev->received_frames[i].frames_num = dev->received_count;
	dev->received_frames[i].time_us64 = time_us64;

	vc_print(dev->index, PRINT_PERFORMANCE,
		 "len =%d, frame_count=%d, time_us64=%lld, fd=%d, transform=%d\n",
		 kfifo_len(&dev->receive_q),
		 frames_info->frame_count,
		 time_us64,
		 fence_fd,
		 frames_info->frame_info[0].transform);

	for (j = 0; j < frames_info->frame_count; j++) {
		frames_info->frame_info[j].composer_fen_fd = fence_fd;
		file_vf = fget(frames_info->frame_info[j].fd);
		if (!file_vf) {
			vc_print(dev->index, PRINT_ERROR, "fget fd fail\n");
			return;
		}
		fence_file = NULL;
		if (frames_info->frame_info[j].disp_fen_fd >= 0) {
			fence_file = fget(frames_info->frame_info[j].disp_fen_fd);
			if (!fence_file)
				vc_print(dev->index, PRINT_OTHER, "fget disp_fen_fd fail\n");
		}
		total_get_count++;
		dev->received_frames[i].file_vf[j] = file_vf;
		dev->received_frames[i].fence_file[j] = fence_file;

		type = frames_info->frame_info[j].type;
		detect_vf_type(&frames_info->frame_info[j], file_vf, &is_dec_vf, &is_v4l_vf);

		vc_print(dev->index, PRINT_FENCE,
			"receive:file=%px, dma=%px, file_fd=%d, file_count=%ld\n",
			 file_vf,
			 file_vf->private_data,
			 frames_info->frame_info[j].fd,
			 file_count(file_vf));

		vc_print(dev->index, PRINT_FENCE,
			 "disp_fen_fd=%d, disp_fence_file=%px\n",
			 frames_info->frame_info[j].disp_fen_fd,
			 fence_file);

		if (frames_info->frame_info[j].transform != 0 || frames_info->frame_count != 1)
			need_dw = true;

		vc_print(dev->index, PRINT_OTHER, "%s: type is %d.\n", __func__, type);
		if (type == 0 || type == 1) {
			if (!(is_dec_vf || is_v4l_vf)) {
				if (type == 0) {
					vc_print(dev->index, PRINT_ERROR, "%s: not vf\n", __func__);
					return;
				}
				dev->received_frames[i].phy_addr[j] =
				get_dma_phy_addr(frames_info->frame_info[j].fd, dev->index);
				vc_print(dev->index, PRINT_OTHER,
					 "%s dma buffer not vf\n", __func__);
				continue;
			}
			vf = get_vf_from_file(dev, file_vf, need_dw);
			if (!vf) {
				vc_print(dev->index, PRINT_ERROR, "received NULL vf!!\n");
				return;
			}

			vc_print(dev->index, PRINT_FENCE, "%s: vf:%px, vf_ext:%px,timestamp:%lld\n",
				__func__, vf, vf->vf_ext, div_u64(vf->timestamp, 1000000000));

			memcpy(frames_info->frame_info[j].reserved1, &vf, sizeof(vf));

			if (((reset_drop >> dev->index) & 1) ||
			    last_index[dev->index][j] > vf->frame_index) {
				dev->received_new_count = vf->frame_index;
				dev->received_count = vf->frame_index;
				vpp_drop_count = 0;
				reset_drop ^= 1 << dev->index;
				vc_print(dev->index, PRINT_PATTERN, "drop cnt reset!!\n");
			}

			if (last_index[dev->index][j] != vf->frame_index) {
				dev->received_new_count++;
				last_index[dev->index][j] = vf->frame_index;
			}

			if (dev->index == 0) {
				drop_cnt = vf->frame_index + 1
					    - dev->received_new_count;
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
				if (drop_cnt == 0)
					ATRACE_COUNTER("video_composer_drop_cnt", 0);
				if (drop_cnt != dev->last_drop_cnt) {
					dev->last_drop_cnt = drop_cnt;
					ATRACE_COUNTER("video_composer_drop_cnt", drop_cnt);
					ATRACE_COUNTER("video_composer_drop_cnt", 0);
				}
#endif
				receive_new_count = dev->received_new_count;
				receive_count = dev->received_count + 1;
				last_frame_index = vf->frame_index;
			} else if (dev->index == 1) {
				drop_cnt_pip = vf->frame_index + 1
						- dev->received_new_count;
				receive_new_count_pip = dev->received_new_count;
				receive_count_pip = dev->received_count + 1;
				last_frame_index = vf->frame_index;
			}

			if (!is_tvp) {
				if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
					is_tvp = true;
			}
			if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
				vf->source_type == VFRAME_SOURCE_TYPE_CVBS) {
				dev->is_tv_path = true;
				tv_fence_creat_count++;
			} else {
				dev->is_tv_path = false;
			}
			vc_print(dev->index, PRINT_FENCE | PRINT_PATTERN,
				 "received_cnt=%lld,new_cnt=%lld,i=%d,z=%d,frame_index=%d, fence_fd=%d, fc_no=%d, index_disp=%d,pts=%lld,vf=%px\n",
				 dev->received_count + 1,
				 dev->received_new_count,
				 i,
				 frames_info->frame_info[j].zorder,
				 vf->frame_index,
				 fence_fd,
				 dev->cur_streamline_val,
				 vf->index_disp,
				 vf->pts_us64,
				 vf);
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
			ATRACE_COUNTER("video_composer_sf_frame_index", vf->frame_index);
			ATRACE_COUNTER("video_composer_sf_frame_index", 0);
			ATRACE_COUNTER("video_composer_sf_timestamp",
				div_u64(vf->timestamp, 1000000000));
			ATRACE_COUNTER("video_composer_sf_timestamp", 0);
#endif
		} else {
			vc_print(dev->index, PRINT_ERROR, "unsupport type.\n");
		}
	}
	dev->received_frames[i].frames_info = *frames_info;
	dev->received_frames[i].is_tvp = is_tvp;
	atomic_set(&dev->received_frames[i].on_use, true);
	dev->received_count++;

	if (!kfifo_put(&dev->receive_q, &dev->received_frames[i]))
		vc_print(dev->index, PRINT_ERROR, "put ready fail\n");
	wake_up_interruptible(&dev->wq);

	//vc_print(dev->index, PRINT_PERFORMANCE, "set_frames_info_out\n");
}

static void dev_get_vinfo(struct composer_dev *dev)
{
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	u64 output_duration;

	video_composer_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(video_composer_vinfo)) {
		vc_print(dev->index, PRINT_ERROR, "get display vinfo err!!\n");
		video_composer_vinfo = &vinfo;
	}
	output_duration = div64_u64(video_composer_vinfo->sync_duration_num,
		video_composer_vinfo->sync_duration_den);

	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	dev->output_duration = output_duration;
}

static int video_composer_init(struct composer_dev *dev)
{
	int ret;
	int i, j;
	char render_layer[16] = "";

	if (!dev)
		return -1;

	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->receive_q);
	INIT_KFIFO(dev->free_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->vc_prepare_data_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->receive_q);
	kfifo_reset(&dev->free_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->vc_prepare_data_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++)
		vd_prepare_data_q_put(dev, &dev->vd_prepare[i]);

	vc_private_q_init(dev);

	dev->received_count = 0;
	dev->received_new_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;
	dev->fput_count = 0;
	dev->last_dst_vf = NULL;
	dev->drop_frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->last_file = NULL;
	dev->select_path_done = false;
	dev->vd_prepare_last = NULL;
	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->kfifo_need_initialize = true;
	dev->fence_wait_time_total = 0;
	dev->fence_wait_count = 0;
	dev->last_vf_from_dpss = false;
	dev_array[dev->index] = dev;
	init_completion(&dev->task_done);
	for (i = 0; i < MAX_VD_LAYERS; i++) {
		for (j = 0; j < MAX_COMPOSER_COUNT; j++)
			last_index[i][j] = -1;
	}
	last_frame_index = -1;
	disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);

	ret = video_display_create_path(dev);
	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	set_video_path_select(render_layer, dev->index);
	dev_get_vinfo(dev);
#ifdef CONFIG_AMLOGIC_MEDIA_RESMANAGE
	resman_register_debug_callback("Display_VC", set_vc_config);
#endif
	return ret;
}

static int video_composer_uninit(struct composer_dev *dev)
{
	int ret;
	int time_left = 0;

	if (dev->is_sideband) {
		if (dev->index == 0) {
			set_video_path_select("auto", 0);
		}
		set_vdx_blackout_policy(dev->index, 1);
	} else {
		if (dev->index == 0) {
			set_video_path_select("default", 0);
		}
		set_vdx_blackout_policy(dev->index, 1);
	}

	disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_display_release_path(dev);

	dev->need_unint_receive_q = true;

	/* free buffer */
	dev->need_free_buffer = true;
	wake_up_interruptible(&dev->wq);

	time_left = wait_for_completion_timeout(&dev->task_done,
						msecs_to_jiffies(500));
	if (!time_left)
		vc_print(dev->index, PRINT_ERROR, "unreg:wait timeout\n");
	else if (time_left < 100)
		vc_print(dev->index, PRINT_ERROR,
			 "unreg:wait time %d\n", time_left);

	display_q_uninit(dev);

	if (dev->fence_creat_count != dev->fput_count) {
		vc_print(dev->index, PRINT_ERROR,
			 "uninit: fence_r=%lld, fence_c=%lld\n",
			 dev->fence_release_count,
			 dev->fence_creat_count);
		vc_print(dev->index, PRINT_ERROR,
			 "uninit: received=%lld, new_cnt=%lld, fput=%lld, drop=%d\n",
			 dev->received_count,
			 dev->received_new_count,
			 dev->fput_count,
			 dev->drop_frame_count);
	}
	video_timeline_increase(dev,
				dev->fence_creat_count
				- dev->fence_release_count);
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev_array[dev->index] = NULL;
	video_display_para_reset(dev->index);

	if (dev->aiface_buf) {
		vfree(dev->aiface_buf);
		dev->aiface_buf = NULL;
	}
	return ret;
}

int video_composer_set_enable(struct composer_dev *dev, u32 val)
{
	int ret = 0;

	if (val > VIDEO_COMPOSER_ENABLE_NORMAL)
		return -EINVAL;

	if (val == 0)
		dev->composer_enabled = false;

	vc_print(dev->index, PRINT_ERROR, "set enable index=%d, val=%d\n", dev->index, val);

	if (dev->enable_composer == val) {
		vc_print(dev->index, PRINT_ERROR, "set_enable repeat, dev index =%d\n", dev->index);
		return ret;
	}
	dev->enable_composer = val;

	if (val == VIDEO_COMPOSER_ENABLE_NORMAL) {
		ret = video_composer_init(dev);
	} else if (val == VIDEO_COMPOSER_ENABLE_NONE) {
		wake_up_interruptible(&dev->wq);
		ret = video_composer_uninit(dev);
	}

	if (ret != 0)
		vc_print(dev->index, PRINT_ERROR, "%s failed\n", __func__);
	else
		if (val)
			dev->composer_enabled = true;
	return ret;
}

static long video_composer_ioctl(struct file *file,
				 unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	u32 val;
	struct composer_dev *dev = (struct composer_dev *)file->private_data;
	struct frames_info_t *frames_info;
	struct capability_info_t capability_info;
	u32 w;
	u32 h;

	switch (cmd) {
	case VIDEO_COMPOSER_IOCTL_SET_FRAMES:
		if (!dev)
			return -ENOMEM;
		frames_info = &dev->frames_info;
		memset(frames_info, 0, sizeof(*frames_info));
		if (copy_from_user(frames_info, argp, sizeof(struct frames_info_t)) == 0) {
			set_frames_info(dev, frames_info);
			ret = copy_to_user(argp, frames_info, sizeof(struct frames_info_t));
		} else {
			ret = -EFAULT;
		}
		break;
	case VIDEO_COMPOSER_IOCTL_SET_ENABLE:
		if (copy_from_user(&val, argp, sizeof(u32)) == 0)
			ret = video_composer_set_enable(dev, val);
		else
			ret = -EFAULT;
		break;
	case VIDEO_COMPOSER_IOCTL_SET_DISABLE:
		break;
	case VIDEO_COMPOSER_IOCTL_GET_PANEL_CAPABILITY:
		val = video_get_layer_capability();
		ret = copy_to_user(argp, &val, sizeof(u32));
		break;
	case VIDEO_COMPOSER_IOCTL_GET_LAYER_CAPABILITY:
		memset(&capability_info, 0, sizeof(struct capability_info_t));
		capability_info.capability = video_get_layer_capability();
		get_video_src_min_buffer(dev->index, &w, &h);
		capability_info.min_w = w;
		capability_info.min_h = h;
		get_video_src_max_buffer(dev->index, &w, &h);
		capability_info.max_w = w;
		capability_info.max_h = h;
		vc_print(dev->index, PRINT_ERROR,
			"get capability: min %d %d; max %d %d\n",
			 capability_info.min_w, capability_info.min_h, w, h);
		ret = copy_to_user(argp, &capability_info, sizeof(struct capability_info_t));
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long video_composer_compat_ioctl(struct file *file, unsigned int cmd,
					ulong arg)
{
	long ret = 0;

	ret = video_composer_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations video_composer_fops = {
	.owner = THIS_MODULE,
	.open = video_composer_open,
	.release = video_composer_release,
	.unlocked_ioctl = video_composer_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = video_composer_compat_ioctl,
#endif
	.poll = NULL,
};

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token &&
			       (isspace(*token) ||
				!isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static ssize_t debug_crop_pip_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_crop_pip is %d\n",
			debug_crop_pip);
}

static ssize_t debug_crop_pip_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	debug_crop_pip = tmp;
	return count;
}

static ssize_t debug_axis_pip_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_axis_pip is %d\n",
			debug_axis_pip);
}

static ssize_t debug_axis_pip_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	debug_axis_pip = tmp;
	return count;
}

static ssize_t dump_vframe_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"dump_vframe: %d.\n",
			dump_vframe);
}

static ssize_t dump_vframe_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	dump_vframe = tmp;
	return count;
}

static ssize_t force_composer_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer is %d\n",
			force_composer);
}

static ssize_t force_composer_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_composer = tmp;
	return count;
}

static ssize_t force_composer_pip_show(const struct class *cla,
				       const struct class_attribute *attr,
				       char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer_pip is %d\n",
			force_composer_pip);
}

static ssize_t force_composer_pip_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_composer_pip = tmp;
	return count;
}

static ssize_t transform_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current transform is %d\n",
			transform);
}

static ssize_t transform_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	transform = tmp;
	return count;
}

static ssize_t vidc_debug_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current vidc_debug is %d\n",
			vidc_debug);
}

static ssize_t vidc_debug_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vidc_debug = tmp;
	return count;
}

static ssize_t vidc_pattern_debug_show(const struct class *cla,
				       const struct class_attribute *attr,
				       char *buf)
{
	return snprintf(buf, 80,
			"current vidc_pattern_debug is %d\n",
			vidc_pattern_debug);
}

static ssize_t vidc_pattern_debug_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vidc_pattern_debug = tmp;
	return count;
}

static ssize_t print_flag_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current print_flag is %d\n",
			print_flag);
}

static ssize_t print_flag_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	print_flag = tmp;
	return count;
}

static ssize_t full_axis_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current full_axis is %d\n",
			full_axis);
}

static ssize_t full_axis_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	full_axis = tmp;
	return count;
}

static ssize_t print_close_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			print_close);
}

static ssize_t print_close_store(const struct class *cla,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	print_close = tmp;
	return count;
}

static ssize_t receive_wait_show(const struct class *cla,
				 const struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current receive_wait is %d\n",
			receive_wait);
}

static ssize_t receive_wait_store(const struct class *cla,
				  const struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	receive_wait = tmp;
	return count;
}

static ssize_t margin_time_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current margin_time is %d\n",
			margin_time);
}

static ssize_t margin_time_store(const struct class *cla,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	margin_time = tmp;
	return count;
}

static ssize_t max_width_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80,
			"current max_width is %d\n",
			max_width);
}

static ssize_t max_width_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	max_width = tmp;
	return count;
}

static ssize_t max_height_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current max_height is %d\n",
			max_height);
}

static ssize_t max_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	max_height = tmp;
	return count;
}

static ssize_t rotate_width_show(const struct class *cla,
				 const struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current rotate_width is %d\n",
			rotate_width);
}

static ssize_t rotate_width_store(const struct class *cla,
				  const struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	rotate_width = tmp;
	return count;
}

static ssize_t rotate_height_show(const struct class *cla,
				  const struct class_attribute *attr,
				  char *buf)
{
	return snprintf(buf, 80,
			"current rotate_height is %d\n",
			rotate_height);
}

static ssize_t rotate_height_store(const struct class *cla,
				   const struct class_attribute *attr,
				   const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	rotate_height = tmp;
	return count;
}

static ssize_t dewarp_rotate_width_show(const struct class *cla,
				 const struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_width is %d\n",
			dewarp_rotate_width);
}

static ssize_t dewarp_rotate_width_store(const struct class *cla,
				  const struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	dewarp_rotate_width = tmp;
	return count;
}

static ssize_t dewarp_rotate_height_show(const struct class *cla,
				 const struct class_attribute *attr,
				 char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_height is %d\n",
			dewarp_rotate_height);
}

static ssize_t dewarp_rotate_height_store(const struct class *cla,
				  const struct class_attribute *attr,
				  const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	dewarp_rotate_height = tmp;
	return count;
}

static ssize_t close_black_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current close_black is %d\n",
			close_black);
}

static ssize_t close_black_store(const struct class *cla,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	close_black = tmp;
	return count;
}

static ssize_t composer_use_444_show(const struct class *class,
				     const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", composer_use_444);
}

static ssize_t composer_use_444_store(const struct class *class,
				      const struct class_attribute *attr,
				      const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	composer_use_444 = val;
	pr_info("set composer_use_444:%d\n", composer_use_444);
	return count;
}

static ssize_t reset_drop_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "reset_drop: %d\n", reset_drop);
}

static ssize_t reset_drop_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	reset_drop = val;
	return count;
}

static ssize_t drop_cnt_show(const struct class *class,
			     const struct class_attribute *attr, char *buf)
{
	return sprintf(buf,
		"rec_cnt: %d, frame_index: %d, valid_cnt: %d, player_drop_cnt: %d, vpp_drop_cnt: %d, total_drop_cnt: %d\n",
		receive_count,
		last_frame_index,
		receive_new_count,
		drop_cnt,
		vpp_drop_count,
		drop_cnt + vpp_drop_count);
}

static ssize_t drop_cnt_pip_show(const struct class *class,
				 const struct class_attribute *attr,
				 char *buf)
{
	return sprintf(buf,
		"pip_cnt:%d,frame_index:%d,valid_cnt_pip:%d,drop_cnt_pip:%d\n",
		receive_count_pip,
		last_frame_index,
		receive_new_count_pip,
		drop_cnt_pip);
}

static ssize_t receive_count_show(const struct class *class,
				  const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", receive_count);
}

static ssize_t receive_count_pip_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", receive_count_pip);
}

static ssize_t receive_new_count_show(const struct class *class,
				      const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", receive_new_count);
}

static ssize_t receive_new_count_pip_show(const struct class *class,
					  const struct class_attribute *attr,
					  char *buf)
{
	return sprintf(buf, "%d\n", receive_new_count_pip);
}

static ssize_t total_get_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_get_count);
}

static ssize_t total_put_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_put_count);
}

static ssize_t nn_need_time_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_need_time: %lld\n", nn_need_time);
}

static ssize_t nn_need_time_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_need_time = val;
	return count;
}

static ssize_t nn_margin_time_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_margin_time: %lld\n", nn_margin_time);
}

static ssize_t nn_margin_time_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_margin_time = val;
	return count;
}

static ssize_t nn_bypass_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "nn_bypass: %d\n", nn_bypass);
}

static ssize_t nn_bypass_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	nn_bypass = val;
	return count;
}

static ssize_t tv_fence_creat_count_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "tv_fence_creat_count: %d\n", tv_fence_creat_count);
}

static ssize_t vd_pulldown_level_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_pulldown_level: %d\n", vd_pulldown_level);
}

static ssize_t vd_pulldown_level_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_pulldown_level = val;
	return count;
}

static ssize_t vd_max_hold_count_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_max_hold_count: %d\n", vd_max_hold_count);
}

static ssize_t vd_max_hold_count_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_max_hold_count = val;
	return count;
}

static ssize_t vd_set_frame_delay_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80, "vd_set_frame_delay: %d,%d,%d\n",
		vd_set_frame_delay[0], vd_set_frame_delay[1],
		vd_set_frame_delay[2]);
}

static ssize_t vd_set_frame_delay_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	if (likely(parse_para(buf, MAX_VIDEO_COMPOSER_INSTANCE_NUM,
		vd_set_frame_delay) == MAX_VIDEO_COMPOSER_INSTANCE_NUM))
		return strnlen(buf, count);

	return -EINVAL;
}

static ssize_t vd_dump_vframe_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vd_dump_vframe: %d\n", vd_dump_vframe);
}

static ssize_t vd_dump_vframe_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	vd_dump_vframe = val;
	if (vd_dump_vframe == 1 && current_display_vf) {
		ext_controls();
		vd_dump_vframe = 0;
	}
	return count;
}

static ssize_t actual_delay_count_show(const struct class *class,
			       const struct class_attribute *attr, char *buf)
{
	int val = (actual_delay_count[0] | (actual_delay_count[1] << 4)
		| (actual_delay_count[2] << 8));
	return sprintf(buf, "%d\n", val);
}

static ssize_t vicp_output_dev_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"1 mif, 2 fbc, 3 mif+fbc. current choice is %d.\n", vicp_output_dev);
}

static ssize_t vicp_output_dev_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_output_dev = tmp;
	return count;
}

static ssize_t vicp_shrink_mode_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"0 2x, 1 4x, 2 8x. current choice is %d.\n", vicp_shrink_mode);
}

static ssize_t vicp_shrink_mode_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_shrink_mode = tmp;
	return count;
}

static ssize_t vicp_max_width_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current vicp_max_width is %d.\n", vicp_max_width);
}

static ssize_t vicp_max_width_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_max_width = tmp;
	return count;
}

static ssize_t vicp_max_height_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current vicp_max_height is %d.\n", vicp_max_height);
}

static ssize_t vicp_max_height_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	vicp_max_height = tmp;
	return count;
}

static ssize_t composer_dev_choice_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"1 ge2d, 2 dewarp, 3 vicp. current choice is %d.\n", composer_dev_choice);
}

static ssize_t composer_dev_choice_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	composer_dev_choice = tmp;
	return count;
}

static ssize_t force_comp_w_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"force_comp_w %d.\n", force_comp_w);
}

static ssize_t force_comp_w_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_comp_w = tmp;
	return count;
}

static ssize_t force_comp_h_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80,
		"force_comp_h %d.\n", force_comp_h);
}

static ssize_t force_comp_h_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_comp_h = tmp;
	return count;
}

static ssize_t vd_test_fps_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i;
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	for (i = 0; i < MAX_VD_LAYERS; i++)
		vd_test_fps[i] = tmp;

	return count;
}

static ssize_t vd_test_fps_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	int i = 0;
	u64 fps_h, vsync_h = 0;
	u32 fps_l, vsync_l = 0;
	ssize_t count = 0;

	for (i = 0; i < MAX_VD_LAYERS; i++) {
		fps_h = div_s64_rem(vd_test_fps_val[i], 100000, &fps_l);
		vsync_h = div_s64_rem(vd_test_vsync_val[i], 100000, &vsync_l);

		count += sprintf(buf + count, "vc[%d]: fps=%llu.%u, vsyn=%llu.%u\n",
			i, fps_h, fps_l, vsync_h, vsync_l);
	}
	count += sprintf(buf + count, "\n");
	return count;
}

static ssize_t dewarp_load_flag_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current dewarp_load_flag is %d.\n", dewarp_load_flag);
}

static ssize_t dewarp_load_flag_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	pr_info("set dewarp_load_flag to %ld.\n", tmp);
	dewarp_load_flag = tmp;
	return count;
}

static ssize_t lossy_compress_rate_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "current lossy_compress_rate is %d.\n", lossy_compress_rate);
}

static ssize_t lossy_compress_rate_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	lossy_compress_rate = tmp;
	return count;
}

static ssize_t enable_frc_pattern_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			enable_frc_pattern);
}

static ssize_t enable_frc_pattern_store(const struct class *cla,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	enable_frc_pattern = tmp;
	return count;
}

static ssize_t buffer_status_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	int i, j;
	int len = 0;
	int ret = 0;
	ssize_t count = 0;
	int buffer_count = 0;
	struct received_frames_t *frames[8];
	struct vframe_s *buffer[8];

	for (i = 0; i < MAX_VD_LAYERS; i++) {
		if (!dev_array[i])
			continue;
		count += sprintf(buf + count, "layer[%d]:  vinfo: %d * %d\n", i,
			dev_array[i]->vinfo_w, dev_array[i]->vinfo_h);
		count += sprintf(buf + count, "----------------------------\n");
		len = kfifo_len(&dev_array[i]->receive_q);
		ret = kfifo_out_peek(&dev_array[i]->receive_q, frames, len);
		if (ret != len) {
			pr_err("Failed to peek data from kfifo\n");
			return count;
		}
		for (j = 0; j < len; j++) {
			memcpy(&buffer[j], frames[j]->frames_info.frame_info[0].reserved1,
				sizeof(buffer[0]));
			if (buffer[j] && (buffer[j]->type & VIDTYPE_DI_PW ||
				buffer[j]->di_flag & DI_FLAG_DI_PVPPLINK))
				buffer_count++;
		}
		count += sprintf(buf + count, "di hold buffer count:%d\n", buffer_count);
		buffer_count = 0;
		for (j = 0; j < len; j++) {
			if (buffer[j] && (buffer[j]->type & VIDTYPE_DI_PW ||
				buffer[j]->di_flag & DI_FLAG_DI_PVPPLINK)) {
				if (buffer[j]->vf_ext)
					buffer[j] = buffer[j]->vf_ext;
				count += sprintf(buf + count,
					"\t(receive_q)frame_index: %d\n",
					buffer[j]->frame_index);
				count += sprintf(buf + count, "\t\ttimestamp:%llu\n",
					(unsigned long long)buffer[j]->timestamp);
				count += sprintf(buf + count,
					"\t\ty_addr: 0x%lx  uv_addr:0x%lx  width: %d height:%d\n",
					buffer[j]->canvas0_config[0].phy_addr,
					buffer[j]->canvas0_config[1].phy_addr,
					buffer[j]->width,
					buffer[j]->height);
				buffer[j] = NULL;
			} else {
				buffer_count++;
			}
		}
		count += sprintf(buf + count, "\nvc hold buffer count: %d\n",
			buffer_count + kfifo_len(&dev_array[i]->ready_q));
		buffer_count = 0;
		for (j = 0; j < len; j++) {
			if (buffer[j]) {
				count += sprintf(buf + count,
					"\t%d:(receive_q)frame_index: %d\n",
					buffer_count, buffer[j]->frame_index);
				count += sprintf(buf + count, "\t\ttimestamp:%llu\n",
					(unsigned long long)buffer[j]->timestamp);
				count += sprintf(buf + count,
					"\t\ty_addr: 0x%lx  uv_addr:0x%lx  width: %d height:%d\n",
					buffer[j]->canvas0_config[0].phy_addr,
					buffer[j]->canvas0_config[1].phy_addr,
					buffer[j]->width,
					buffer[j]->height);
				buffer_count++;
			}
		}

		len = kfifo_len(&dev_array[i]->ready_q);
		ret = kfifo_out_peek(&dev_array[i]->ready_q, buffer, len);
		if (ret != len) {
			pr_err("Failed to peek data from kfifo\n");
			return count;
		}
		for (j = 0; j < len; j++) {
			if (buffer[j] && buffer[j]->vf_ext && (buffer[j]->type & VIDTYPE_DI_PW ||
				buffer[j]->di_flag & DI_FLAG_DI_PVPPLINK))
				buffer[j] = buffer[j]->vf_ext;
			count += sprintf(buf + count,
				"\t%d:(ready_q)frame_index: %d\n",
				buffer_count + j, buffer[j]->frame_index);
			count += sprintf(buf + count, "\t\ttimestamp:%llu\n",
				(unsigned long long)buffer[j]->timestamp);
			count += sprintf(buf + count,
				"\t\ty_addr: 0x%lx  uv_addr:0x%lx  width: %d height:%d\n",
				buffer[j]->canvas0_config[0].phy_addr,
				buffer[j]->canvas0_config[1].phy_addr,
				buffer[j]->width,
				buffer[j]->height);
		}
		len = kfifo_len(&dev_array[i]->display_q);
		ret = kfifo_out_peek(&dev_array[i]->display_q, buffer, len);
		if (ret != len) {
			pr_err("Failed to peek data from kfifo\n");
			return count;
		}
		count += sprintf(buf + count, "\nvpp hold buffer count:%d\n", len);
		for (j = 0; j < len; j++) {
			if (buffer[j] && buffer[j]->vf_ext && (buffer[j]->type & VIDTYPE_DI_PW ||
				buffer[j]->di_flag & DI_FLAG_DI_PVPPLINK))
				buffer[j] = buffer[j]->vf_ext;

			count += sprintf(buf + count,
				"\t%d:(display_q)frame_index: %d\n",
				j, buffer[j]->frame_index);
			count += sprintf(buf + count, "\t\ttimestamp:%llu\n",
				(unsigned long long)buffer[j]->timestamp);
			count += sprintf(buf + count,
				"\t\ty_addr: 0x%lx  uv_addr:0x%lx  width: %d height:%d\n",
				buffer[j]->canvas0_config[0].phy_addr,
				buffer[j]->canvas0_config[1].phy_addr,
				buffer[j]->width,
				buffer[j]->height);
		}
		count += sprintf(buf + count, "\nfree_q(internal buffer):%d\n",
			kfifo_len(&dev_array[i]->free_q));
	}
	count += sprintf(buf + count, "\n");
	return count;
}

static CLASS_ATTR_RW(debug_axis_pip);
static CLASS_ATTR_RW(debug_crop_pip);
static CLASS_ATTR_RW(force_composer);
static CLASS_ATTR_RW(force_composer_pip);
static CLASS_ATTR_RW(transform);
static CLASS_ATTR_RW(vidc_debug);
static CLASS_ATTR_RW(vidc_pattern_debug);
static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(full_axis);
static CLASS_ATTR_RW(print_close);
static CLASS_ATTR_RW(receive_wait);
static CLASS_ATTR_RW(margin_time);
static CLASS_ATTR_RW(max_width);
static CLASS_ATTR_RW(max_height);
static CLASS_ATTR_RW(rotate_width);
static CLASS_ATTR_RW(rotate_height);
static CLASS_ATTR_RW(dewarp_rotate_width);
static CLASS_ATTR_RW(dewarp_rotate_height);
static CLASS_ATTR_RW(close_black);
static CLASS_ATTR_RW(composer_use_444);
static CLASS_ATTR_RW(reset_drop);
static CLASS_ATTR_RO(drop_cnt);
static CLASS_ATTR_RO(drop_cnt_pip);
static CLASS_ATTR_RO(receive_count);
static CLASS_ATTR_RO(receive_count_pip);
static CLASS_ATTR_RO(receive_new_count);
static CLASS_ATTR_RO(receive_new_count_pip);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RW(nn_need_time);
static CLASS_ATTR_RW(nn_margin_time);
static CLASS_ATTR_RW(nn_bypass);
static CLASS_ATTR_RO(tv_fence_creat_count);
static CLASS_ATTR_RW(dump_vframe);
static CLASS_ATTR_RW(vd_pulldown_level);
static CLASS_ATTR_RW(vd_max_hold_count);
static CLASS_ATTR_RW(vd_set_frame_delay);
static CLASS_ATTR_RW(vd_dump_vframe);
static CLASS_ATTR_RO(actual_delay_count);
static CLASS_ATTR_RW(vicp_output_dev);
static CLASS_ATTR_RW(vicp_shrink_mode);
static CLASS_ATTR_RW(vicp_max_width);
static CLASS_ATTR_RW(vicp_max_height);
static CLASS_ATTR_RW(composer_dev_choice);
static CLASS_ATTR_RW(force_comp_w);
static CLASS_ATTR_RW(force_comp_h);
static CLASS_ATTR_RW(vd_test_fps);
static CLASS_ATTR_RW(dewarp_load_flag);
static CLASS_ATTR_RW(lossy_compress_rate);
static CLASS_ATTR_RW(enable_frc_pattern);
static CLASS_ATTR_RO(buffer_status);

static struct attribute *video_composer_class_attrs[] = {
	&class_attr_debug_crop_pip.attr,
	&class_attr_debug_axis_pip.attr,
	&class_attr_force_composer.attr,
	&class_attr_force_composer_pip.attr,
	&class_attr_transform.attr,
	&class_attr_vidc_debug.attr,
	&class_attr_vidc_pattern_debug.attr,
	&class_attr_print_flag.attr,
	&class_attr_full_axis.attr,
	&class_attr_print_close.attr,
	&class_attr_receive_wait.attr,
	&class_attr_margin_time.attr,
	&class_attr_max_width.attr,
	&class_attr_max_height.attr,
	&class_attr_rotate_width.attr,
	&class_attr_rotate_height.attr,
	&class_attr_dewarp_rotate_width.attr,
	&class_attr_dewarp_rotate_height.attr,
	&class_attr_close_black.attr,
	&class_attr_composer_use_444.attr,
	&class_attr_reset_drop.attr,
	&class_attr_drop_cnt.attr,
	&class_attr_drop_cnt_pip.attr,
	&class_attr_receive_count.attr,
	&class_attr_receive_count_pip.attr,
	&class_attr_receive_new_count.attr,
	&class_attr_receive_new_count_pip.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_nn_need_time.attr,
	&class_attr_nn_margin_time.attr,
	&class_attr_nn_bypass.attr,
	&class_attr_tv_fence_creat_count.attr,
	&class_attr_dump_vframe.attr,
	&class_attr_vd_pulldown_level.attr,
	&class_attr_vd_max_hold_count.attr,
	&class_attr_vd_set_frame_delay.attr,
	&class_attr_vd_dump_vframe.attr,
	&class_attr_actual_delay_count.attr,
	&class_attr_vicp_output_dev.attr,
	&class_attr_vicp_shrink_mode.attr,
	&class_attr_vicp_max_width.attr,
	&class_attr_vicp_max_height.attr,
	&class_attr_composer_dev_choice.attr,
	&class_attr_force_comp_w.attr,
	&class_attr_force_comp_h.attr,
	&class_attr_vd_test_fps.attr,
	&class_attr_dewarp_load_flag.attr,
	&class_attr_lossy_compress_rate.attr,
	&class_attr_enable_frc_pattern.attr,
	&class_attr_buffer_status.attr,
	NULL
};

ATTRIBUTE_GROUPS(video_composer_class);

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
struct mediaproxy_info_t mediaproxy_display_info[] = {
	{
		.k_producer_name = "video_composer0"
	},
	{
		.k_producer_name = "video_composer1"
	},
	{
		.k_producer_name = "video_composer2"
	},
};
#endif

static struct class video_composer_class = {
	.name = "video_composer",
	.class_groups = video_composer_class_groups,
};

static const struct of_device_id amlogic_video_composer_dt_match[] = {
	{.compatible = "amlogic, video_composer",
	},
	{},
};

static int video_composer_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	u32 layer_cap = 0;
	struct video_composer_port_s *st;

	layer_cap = video_get_layer_capability();
	video_composer_instance_num = 0;
	if (layer_cap & LAYER0_SCALER)
		video_composer_instance_num++;
	if (layer_cap & LAYER1_SCALER)
		video_composer_instance_num++;
	if (layer_cap & LAYER2_SCALER)
		video_composer_instance_num++;
	if (is_meson_c3_cpu())
		video_composer_instance_num = 1;
	ret = class_register(&video_composer_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(VIDEO_COMPOSER_MAJOR,
			      "video_composer", &video_composer_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for video_composer device\n");
		goto error1;
	}

	for (st = &ports[0], i = 0;
	     i < video_composer_instance_num; i++, st++) {
		pr_debug("%s:ports[i].name=%s, i=%d\n", __func__,
		       ports[i].name, i);
		st->pdev = &pdev->dev;
		st->class_dev = device_create(&video_composer_class, NULL,
					      MKDEV(VIDEO_COMPOSER_MAJOR, i),
					      NULL, ports[i].name);
		ret = of_property_read_u32(pdev->dev.of_node, "vpu_dma_mask", &st->vpu_dma_mask);
		if (ret) {
			pr_debug("video_composer don't find vpu_dma_mask\n");
			st->vpu_dma_mask = 0;
		}
		if (st->vpu_dma_mask) {
			ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_coherent_mask fail\n");
				goto error1;
			}
			ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_mask fail\n");
				goto error1;
			}
		}

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
		if (!mediaproxy_display_info[i].k_producer_session) {
			media_proxy_produce_init(&mediaproxy_display_info[i].k_producer_session,
				mediaproxy_display_info[i].k_producer_name,
				MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO |
				MEDIA_VIDEO_METRICS_FRAME_SIGNAFENCE_INFO);
		}
#endif
	}
	pr_debug("%s num=%d\n", __func__, video_composer_instance_num);
	return 0;

error1:
	pr_err("%s error\n", __func__);
	unregister_chrdev(VIDEO_COMPOSER_MAJOR, "video_composer");
	class_unregister(&video_composer_class);
	return ret;
}

static void video_composer_remove(struct platform_device *pdev)
{
	int i;
	struct video_composer_port_s *st;

	for (st = &ports[0], i = 0;
	     i < video_composer_instance_num; i++, st++)
		device_destroy(&video_composer_class,
			       MKDEV(VIDEO_COMPOSER_MAJOR, i));

	unregister_chrdev(VIDEO_COMPOSER_MAJOR, VIDEO_COMPOSER_DEVICE_NAME);
	class_destroy(&video_composer_class);
};

static struct platform_driver video_composer_driver = {
	.probe = video_composer_probe,
	.remove = video_composer_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "video_composer",
		.of_match_table = amlogic_video_composer_dt_match,
	}
};

int __init video_composer_module_init(void)
{
	pr_debug("video composer module init.\n");
	return platform_driver_register(&video_composer_driver);
}

void __exit video_composer_module_exit(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	int i;

	for (i = 0; i < video_composer_instance_num; i++) {
		if (mediaproxy_display_info[i].k_producer_session)
			media_proxy_produce_deinit(mediaproxy_display_info[i].k_producer_session);
	}
#endif

	platform_driver_unregister(&video_composer_driver);
}

//MODULE_DESCRIPTION("Video Technology Magazine video composer Capture Board");
//MODULE_AUTHOR("Amlogic, Jintao Xu<jintao.xu@amlogic.com>");
//MODULE_LICENSE("GPL");
//MODULE_VERSION(VIDEO_COMPOSER_VERSION);

