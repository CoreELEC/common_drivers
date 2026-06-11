// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/utils/am_com.h>
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
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/dma-buf.h>
#include <linux/amlogic/ion.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/resource_mgr/resourcemanage.h>

#include "v2d.h"

#define V2D_INSTANCE_NUM    2
#define BUFFER_4K_WIDTH    3840
#define BUFFER_4K_HEIGHT    2160

#define BUFFER_2K_WIDTH    2560
#define BUFFER_2K_HEIGHT    1440

#define BUFFER_720_WIDTH    1280
#define BUFFER_720_HEIGHT    720

#define MIN_DEWARP_WIDTH   960
#define MIN_DEWARP_HEIGHT    540
#define ADDR_VALUE_8G    (0x200000000ULL)

static u32 v2d_instance_num;
static enum composer_dev v2d_dev_choice; /*0:default 1:ge2d 2:dewarp 3:vicp*/
static int use_full_axis_scaling = 1;
static int display_yuv444;
static int vicp_output_mode = 2; /*1 mif 2. fbc 3. mif + fbc*/
static u32 vicp_shrink_mode = 1; /*0 2x, 1 4x, 2 8x*/
static u32 lossy_compress_rate;//0: 100% compress; 1: 67% compress; 2: 83% compress
static u32 manual_buf_width;
static u32 manual_buf_height;
static u32 total_get_count;
static u32 total_put_count;
static u32 print_flag;
static u32 v2d_bypass;
static bool buf_config_444;
static bool enable_v2d_dump;
static int transform = -1;
static DEFINE_SPINLOCK(v2d_spin_lock);

int ge2d_com_debug;
int dewarp_com_dump;
int dewarp_print;

static const char v2d_group_name0[] = "v2d0_debug";
static const char v2d_group_name1[] = "v2d1_debug";
static const char v2d_group_name2[] = "v2d2_debug";

static DEFINE_MUTEX(v2d_mutex);

static struct v2d_port_s ports[] = {
	{
		.name = "v2d.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "v2d.1",
		.index = 1,
		.open_count = 0,
	},
	{
		.name = "v2d.2",
		.index = 2,
		.open_count = 0,
	},
};

int v2d_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "v2d:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static void frames_put_file(struct v2d_dev *dev,
			    struct received_frames_t *current_frames)
{
	struct file *file_vf;
	int current_count;
	int i;

	current_count = current_frames->frames_info.frame_count;
	for (i = 0; i < current_count; i++) {
		file_vf = current_frames->input_file[i];
		fput(file_vf);
		total_put_count++;
		dev->fput_count++;
	}
}

static void file_q_init(struct v2d_dev *dev)
{
	int i;
	struct dma_buf *dmabuf;

	INIT_KFIFO(dev->file_free_q);
	kfifo_reset(&dev->file_free_q);

	for (i = 0; i < V2D_POOL_SIZE; i++) {
		dmabuf = uvm_alloc_dmabuf(SZ_4K, 0, 0);
		if (!dmabuf_is_uvm(dmabuf))
			v2d_print(dev->index, PRINT_ERROR, "dmabuf is not uvm\n");
		v2d_print(dev->index, PRINT_OTHER, "dmabuf is uvm:dmabuf=%px\n", dmabuf);

		dev->out_dmabuf[i] = dmabuf;
		if (!kfifo_put(&dev->file_free_q, dev->out_dmabuf[i]))
			v2d_print(dev->index, PRINT_ERROR,
				"file_free_q put fail\n");
	}
}

static void file_q_uninit(struct v2d_dev *dev)
{
	int i;

	for (i = 0; i < V2D_POOL_SIZE; i++) {
		if (dev->out_dmabuf[i])
			dma_buf_put(dev->out_dmabuf[i]);
		else
			v2d_print(dev->index, PRINT_ERROR, "unreg: dmabuf is NULL\n");
		dev->out_dmabuf[i] = NULL;
	}
}

static void receive_q_init(struct v2d_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->receive_q);
	kfifo_reset(&dev->receive_q);

	for (i = 0; i < V2D_POOL_SIZE; i++)
		atomic_set(&dev->received_frames[i].on_use, false);
}

static void receive_q_uninit(struct v2d_dev *dev)
{
	int i = 0;
	struct received_frames_t *received_frames = NULL;

	v2d_print(dev->index, PRINT_OTHER, "unit receive_q len=%d\n",
		 kfifo_len(&dev->receive_q));
	while (kfifo_len(&dev->receive_q) > 0) {
		if (kfifo_get(&dev->receive_q, &received_frames))
			frames_put_file(dev, received_frames);
	}

	for (i = 0; i < V2D_POOL_SIZE; i++)
		atomic_set(&dev->received_frames[i].on_use, false);
}

static void display_q_init(struct v2d_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->display_q);
	kfifo_reset(&dev->display_q);

	for (i = 0; i < V2D_POOL_SIZE; i++)
		atomic_set(&dev->display_data[i].on_use, false);
}

static void display_q_uninit(struct v2d_dev *dev)
{
	struct display_data_t *display_data = NULL;
	struct vframe_s *vf;
	int keep_id = 0, keep_table_id = 0;

	/*mak sure all fence signaled by videodisplay before release buffer*/
	v2d_print(dev->index, PRINT_OTHER,
		"display_q_uninit: buffer release count:%d signal_count:%d display_q:%d\n",
		dev->buffer_release_count,
		dev->fence_signal_count,
		kfifo_len(&dev->display_q));

	spin_lock(&v2d_spin_lock);
	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &display_data)) {
			vf = &display_data->private_data->vf;

			keep_id = codec_mm_keeper_mask_keep_mem(vf->mem_handle,
				MEM_TYPE_CODEC_MM);
			if (keep_id > 0) {
				display_data->private_data->keep_id = keep_id;
				display_data->private_data->is_keep = true;
			} else {
				v2d_print(dev->index, PRINT_ERROR,
					"keep fail id=%d\n", keep_id);
			}

			if (vf->type & VIDTYPE_COMPRESS && dev->dev_choice == COMPOSER_WITH_VICP) {
				keep_table_id = codec_mm_keeper_mask_keep_mem
					(vf->mem_handle_1, MEM_TYPE_CODEC_MM);
				if (keep_table_id > 0) {
					display_data->private_data->keep_id_1 =
						keep_table_id;
					display_data->private_data->is_keep = true;
				} else {
					v2d_print(dev->index, PRINT_ERROR,
						"keep fail id=%d\n", keep_table_id);
				}
			} else if (vf->type & VIDTYPE_COMPRESS &&
					dev->dev_choice != COMPOSER_WITH_VICP) {
				v2d_print(dev->index, PRINT_ERROR,
					"v2d uninit: unknown afbc input\n");
			}
		}
	}
	spin_unlock(&v2d_spin_lock);
}

static struct input_axis_s adjust_output_axis(struct v2d_dev *dev,
	struct frame_info_t *vframe_info)
{
	int src_width = 0, src_height = 0;
	int scaled_width = 0, scaled_height = 0;
	int dst_width, dst_height;
	struct input_axis_s output_axis;
	int tmp_size;

	memset(&output_axis, 0, sizeof(struct input_axis_s));
	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(vframe_info)) {
		pr_info("%s: invalid param.\n", __func__);
		return output_axis;
	}

	src_width = vframe_info->crop_w;
	src_height = vframe_info->crop_h;
	dst_width = vframe_info->dst_w;
	dst_height = vframe_info->dst_h;

	if (vframe_info->transform == V2D_TRANSFORM_ROT_90 ||
		vframe_info->transform == V2D_TRANSFORM_ROT_270 ||
		vframe_info->transform == V2D_TRANSFORM_FLIP_H_ROT_90 ||
		vframe_info->transform == V2D_TRANSFORM_FLIP_V_ROT_90) {
		tmp_size = src_height;
		src_height = src_width;
		src_width = tmp_size;
	}
	if (!use_full_axis_scaling && src_width > 0 && src_height > 0) {
		scaled_width  = dst_width;
		scaled_height  = dst_width * src_height  / src_width;
		if (scaled_height  > dst_height) {
			scaled_height = dst_height;
			scaled_width = dst_height * src_width / src_height;
		}
	} else {
		scaled_width = dst_width;
		scaled_height = dst_height;
	}
	output_axis.left = vframe_info->dst_x + (dst_width - scaled_width) / 2;
	output_axis.top = vframe_info->dst_y + (dst_height - scaled_height) / 2;
	output_axis.width = scaled_width;
	output_axis.height = scaled_height;

	v2d_print(dev->index, PRINT_AXIS,
		 "frame out data axis left top width height: %d %d %d %d\n",
		 output_axis.left, output_axis.top, output_axis.width, output_axis.height);
	return output_axis;
}

static void *v2d_timeline_create(struct v2d_dev *dev)
{
	const char *tl_name = "v2d_timeline_0";

	if (dev->index == 0)
		tl_name = "v2d_timeline_0";
	else if (dev->index == 1)
		tl_name = "v2d_timeline_1";

	if (IS_ERR_OR_NULL(dev->v2d_timeline)) {
		dev->cur_streamline_val = 0;
		dev->v2d_timeline = aml_sync_create_timeline(tl_name);
		v2d_print(dev->index, PRINT_FENCE,
			 "timeline create tlName =%s, video_timeline=%p\n",
			 tl_name, dev->v2d_timeline);
	}

	return dev->v2d_timeline;
}

static int v2d_timeline_create_fence(struct v2d_dev *dev)
{
	int out_fence_fd = -1;
	u32 pt_val = 0;

	pt_val = dev->cur_streamline_val + 1;
	v2d_print(dev->index, PRINT_FENCE, "pt_val:%d", pt_val);

	out_fence_fd = aml_sync_create_fence(dev->v2d_timeline, pt_val);
	if (out_fence_fd >= 0) {
		dev->cur_streamline_val++;
		dev->fence_creat_count++;
	} else {
		v2d_print(dev->index, PRINT_ERROR,
			"create fence returned %d", out_fence_fd);
	}
	return out_fence_fd;
}

static void v2d_timeline_increase(struct v2d_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(dev->v2d_timeline, value);
	dev->fence_signal_count += value;
	v2d_print(dev->index, PRINT_FENCE,
		"receive_cnt=%d,new_cnt=%d,fen_creat_cnt=%d,fen_release_cnt=%d\n",
		dev->received_count,
		dev->received_new_count,
		dev->fence_creat_count,
		dev->fence_signal_count);
}

static void v2d_free_fd_private(void *arg)
{
	struct vframe_s *vf;
	struct file_private_data *data = (struct file_private_data *)arg;

	if (!data) {
		v2d_print(-1, PRINT_ERROR, "free: arg is NULL\n");
	} else {
		vf = &data->vf;
		if (data->is_keep) {
			v2d_print(-1, PRINT_OTHER, "free: keep frame_index:%d\n", vf->frame_index);
			if (data->keep_id > 0) {
				codec_mm_keeper_unmask_keeper(data->keep_id, 0);
				data->keep_id = -1;
			}
			if (data->keep_id_1 > 0) {
				codec_mm_keeper_unmask_keeper
					(data->keep_id_1, 0);
				data->keep_id_1 = -1;
			}
		}
		kfree(vf->composer_info);
	}
}

static void v2d_dump_output_buffer(struct vframe_s *vf)
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

	snprintf(name_buf, sizeof(name_buf), "/data/dst_vframe.yuv");
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

struct file_private_data *v2d_get_file_private_data(struct file *file_vf,
							 bool alloc_if_null)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;
	struct uvm_hook_mod_info info;
	struct dma_buf *dmabuf;
	int ret;

	if (!file_vf) {
		v2d_print(0, PRINT_ERROR, "get_file_private_data fail\n");
		return NULL;
	}

	dmabuf = (struct dma_buf *)file_vf->private_data;
	if (IS_ERR_OR_NULL(dmabuf)) {
		v2d_print(0, PRINT_ERROR, "%s: private_data is null\n", __func__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		v2d_print(0, PRINT_ERROR, "%s: dmabuf is not uvm\n", __func__);
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
	if (uhmod && uhmod->arg) {
		file_private_data = uhmod->arg;
		uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
		return file_private_data;
	} else if (!alloc_if_null) {
		return NULL;
	}

	file_private_data = vmalloc(sizeof(*file_private_data));
	if (!file_private_data)
		return NULL;
	memset(file_private_data, 0, sizeof(struct file_private_data));

	memset(&info, 0, sizeof(struct uvm_hook_mod_info));
	info.type = VF_PROCESS_V4LVIDEO;
	info.arg = file_private_data;
	info.free = v2d_free_fd_private;
	info.acquire_fence = NULL;
	ret = uvm_attach_hook_mod((struct dma_buf *)(file_vf->private_data),
				   &info);
	return file_private_data;
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
		v2d_print(index, PRINT_ERROR, "dbuf got from fd error!!!\n");
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

static int v2d_get_received_frame_free_index(struct v2d_dev *dev)
{
	int i = 0;
	int j = 0;

	while (1) {
		for (i = 0; i < V2D_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frames[i].on_use))
				break;
		}
		if (i == V2D_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				v2d_print(dev->index, PRINT_ERROR,
					"receive_q is full, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * MAX_RECEIVE_WAIT_TIME,
				     1000 * (MAX_RECEIVE_WAIT_TIME + 1));
			v2d_print(dev->index, PRINT_ERROR,
				"receive_q is full!!! need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}
	return i;
}

static int v2d_get_display_data_free_index(struct v2d_dev *dev)
{
	int i = 0;
	int j = 0;

	while (1) {
		for (i = 0; i < V2D_POOL_SIZE; i++) {
			if (!atomic_read(&dev->display_data[i].on_use))
				break;
		}
		if (i == V2D_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				v2d_print(dev->index, PRINT_ERROR,
					"display_data is empty, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * MAX_RECEIVE_WAIT_TIME,
				     1000 * (MAX_RECEIVE_WAIT_TIME + 1));
			v2d_print(dev->index, PRINT_ERROR,
				"display_data is empty!!! need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}
	return i;
}

static void dev_get_vinfo(struct v2d_dev *dev)
{
	struct vinfo_s *display_vinfo;
	struct vinfo_s default_vinfo = {.width = 1280, .height = 720, };
	u64 output_duration;

	display_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(display_vinfo)) {
		v2d_print(dev->index, PRINT_ERROR, "get display vinfo err!!\n");
		display_vinfo = &default_vinfo;
	}
	output_duration = div64_u64(display_vinfo->sync_duration_num,
		display_vinfo->sync_duration_den);

	dev->vinfo_w = display_vinfo->width;
	dev->vinfo_h = display_vinfo->height;
	dev->output_duration = output_duration;
}

static u32 v2d_switch_buffer(struct dst_buf_t *buf, bool is_tvp, struct v2d_dev *dev)
{
	int flags, ret;
	bool vicp_fbc_out_en = false;
	u32 *virt_addr = NULL;
	u32 temp_body_addr;
	u32 offset_body_addr;
	struct codec_mm_s *mm;

	if (IS_ERR_OR_NULL(buf) || IS_ERR_OR_NULL(dev)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return 0;
	}

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	if (vicp_output_mode != 1 && dev->dev_choice == COMPOSER_WITH_VICP)
		vicp_fbc_out_en = true;

	if (buf->phy_addr > 0) {
		v2d_print(dev->index, PRINT_OTHER, "free buffer 0x%lx\n", buf->phy_addr);
		codec_mm_release((struct codec_mm_s *)buf->mif_handle, ports[dev->index].name);
	}

	mm = codec_mm_alloc(ports[dev->index].name, buf->buf_size, 0, flags, -1);
	if (!mm) {
		v2d_print(dev->index, PRINT_ERROR,
			"v2d_switch_buffer: get_mif_handle failed\n");
		return 0;
	}
	buf->phy_addr = mm->phy_addr;
	buf->mif_handle = (ulong)mm;

	v2d_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%lx\n", __func__, buf->phy_addr);

	if (vicp_fbc_out_en) {
		buf->afbc_body_addr = buf->phy_addr + buf->dw_size;
		buf->afbc_head_addr = buf->afbc_body_addr + buf->afbc_body_size;

		if (buf->afbc_table_addr > 0) {
			v2d_print(dev->index, PRINT_OTHER, "%s: free table buffer 0x%lx\n",
				__func__, buf->afbc_table_addr);
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

		v2d_print(dev->index, PRINT_ERROR, "%s: alloc buffer 0x%lx\n", __func__,
			buf->afbc_table_addr);
	}

	buf->is_tvp = is_tvp;

	if (buf->phy_addr == 0 || (vicp_fbc_out_en && buf->afbc_table_addr == 0))
		ret = 0;
	else
		ret = 1;

	return ret;
}

static void choose_v2d_device(struct v2d_dev *dev,
	struct received_frames_t *received_frames, struct vframe_s *input_vf)
{
	u32 frame_transform = 0;
	int count = 0;
	int i = 0;
	bool video_need_rotate = false;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		dev->dev_choice = COMPOSER_WITH_UNINITIAL;
		return;
	}

	frame_transform = received_frames->frames_info.input_buffer[0].transform;
	count = received_frames->frames_info.frame_count;

	if (count == 1) {
		if (frame_transform == V2D_TRANSFORM_ROT_90 ||
			frame_transform == V2D_TRANSFORM_ROT_180 ||
			frame_transform == V2D_TRANSFORM_ROT_270 ||
			frame_transform == V2D_TRANSFORM_FLIP_H_ROT_90 ||
			frame_transform == V2D_TRANSFORM_FLIP_V_ROT_90 ||
			frame_transform == V2D_TRANSFORM_FLIP_H ||
			frame_transform == V2D_TRANSFORM_FLIP_V)
			dev->work_mode = V2D_MODE_ROTATE;
		else
			dev->work_mode = V2D_MODE_COMPOSER;
	} else {
		for (i = 0; i < count; i++) {
			frame_transform = received_frames->frames_info.input_buffer[i].transform;
			if (frame_transform == V2D_TRANSFORM_ROT_90 ||
				frame_transform == V2D_TRANSFORM_ROT_180 ||
				frame_transform == V2D_TRANSFORM_ROT_270 ||
				frame_transform == V2D_TRANSFORM_FLIP_H_ROT_90 ||
				frame_transform == V2D_TRANSFORM_FLIP_V_ROT_90 ||
				frame_transform == V2D_TRANSFORM_FLIP_H ||
				frame_transform == V2D_TRANSFORM_FLIP_V) {
				video_need_rotate = true;
				break;
			}
		}

		if (video_need_rotate)
			dev->work_mode = V2D_MODE_COMPOSER_ROTATE;
		else
			dev->work_mode = V2D_MODE_COMPOSER;
	}

	if (v2d_dev_choice == COMPOSER_WITH_DEFAULT) {
		if (check_dewarp_status(input_vf, count, dev->work_mode, dev->index))
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (check_vicp_status(input_vf, dev->work_mode, dev->index))
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	} else {
		if (v2d_dev_choice == COMPOSER_WITH_DEWARP &&
			check_dewarp_status(input_vf, count, dev->work_mode, dev->index))
			dev->dev_choice = COMPOSER_WITH_DEWARP;
		else if (v2d_dev_choice == COMPOSER_WITH_VICP &&
			check_vicp_status(input_vf, dev->work_mode, dev->index))
			dev->dev_choice = COMPOSER_WITH_VICP;
		else
			dev->dev_choice = COMPOSER_WITH_GE2D;
	}

	if (dev->dev_choice != dev->last_dev_choice && dev->buffer_status != UNINITIAL)
		dev->buffer_status = STATUS_SIZE_CHANGED;
	dev->last_dev_choice = dev->dev_choice;

	v2d_print(dev->index, PRINT_OTHER, "%s: dev_choice=%d, work_mode=%d.\n",
		__func__, dev->dev_choice, dev->work_mode);
}

static void init_output_buffer_size(struct v2d_dev *dev,
	struct received_frames_t *received_frames)
{
	u32 buf_width, buf_height;
	size_t usage = 0;

	switch (dev->buffer_status) {
	case UNINITIAL:
	case STATUS_SIZE_CHANGED:
		break;
	case INIT_SIZE_SUCCESS:
	case INIT_BUFFER_SUCCESS:
	case INIT_BUFFER_ERROR:
	default:
		return;
	}

#ifdef CONFIG_AMLOGIC_UVM_CORE
	if (meson_uvm_get_usage(received_frames->input_file[0]->private_data, &usage) < 0)
		v2d_print(dev->index, PRINT_ERROR,
			"%s:meson_uvm_get_usage fail.\n", __func__);
#endif
	received_frames->usage = usage;
	buf_width = dev->vinfo_w;
	buf_height = dev->vinfo_h;

	if (dev->output_duration > 60) {
		buf_width = 1920;
		buf_height = 1080;
	}

	if (usage == UVM_USAGE_IMAGE_PLAY) {
		if (buf_width > BUFFER_4K_WIDTH)
			buf_width = BUFFER_4K_WIDTH;
		if (buf_height > BUFFER_4K_HEIGHT)
			buf_height = BUFFER_4K_HEIGHT;
	} else {
		if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
			buf_width = BUFFER_4K_WIDTH;
			buf_height = BUFFER_4K_HEIGHT;
		} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
			if (dev->work_mode == V2D_MODE_ROTATE) {
				buf_width = BUFFER_4K_HEIGHT;
				buf_height = BUFFER_4K_WIDTH;
			}
		} else {
			if (dev->work_mode == V2D_MODE_ROTATE) {
				buf_width = BUFFER_720_WIDTH;
				buf_height = BUFFER_720_HEIGHT;
			} else {
				buf_width = BUFFER_2K_WIDTH;
				buf_height = BUFFER_2K_HEIGHT;
			}
		}
	}

	if (manual_buf_width != 0 && manual_buf_height != 0) {
		buf_width = manual_buf_width;
		buf_height = manual_buf_height;
	}

	buf_width = ALIGN(buf_width, 32);

	dev->buf_width = buf_width;
	dev->buf_height = buf_height;
	dev->buffer_status = INIT_SIZE_SUCCESS;
	v2d_print(dev->index, PRINT_OTHER,
		"%s: init buffer size:w:%d h:%d.\n",
		__func__, dev->buf_width, dev->buf_height);
}

static bool is_window_changed(struct v2d_dev *dev,
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
		v2d_print(dev->index, PRINT_OTHER,
			 "last count=%d, current count=%d\n",
			 last_frame_info->frame_count,
			 cur_frame_info->frame_count);
	} else {
		for (i = 0; i < cur_frame_info->frame_count; i++) {
			current_width = cur_frame_info->input_buffer[i].crop_w;
			current_height = cur_frame_info->input_buffer[i].crop_h;
			last_width = last_frame_info->input_buffer[i].crop_w;
			last_height = last_frame_info->input_buffer[i].crop_h;

			if (current_width * last_height !=
				current_height * last_width) {
				v2d_print(dev->index, PRINT_OTHER,
					 "frame width or height changed!");
				window_changed = true;
				break;
			}

			cur_pos_x = cur_frame_info->input_buffer[i].dst_x;
			cur_pos_y = cur_frame_info->input_buffer[i].dst_y;
			cur_pos_w = cur_frame_info->input_buffer[i].dst_w;
			cur_pos_h = cur_frame_info->input_buffer[i].dst_h;
			last_pos_x = last_frame_info->input_buffer[i].dst_x;
			last_pos_y = last_frame_info->input_buffer[i].dst_y;
			last_pos_w = last_frame_info->input_buffer[i].dst_w;
			last_pos_h = last_frame_info->input_buffer[i].dst_h;

			if (cur_pos_x != last_pos_x ||
			    cur_pos_y != last_pos_y ||
			    cur_pos_w != last_pos_w ||
			    cur_pos_h != last_pos_h) {
				v2d_print(dev->index, PRINT_OTHER,
					 "frame axis changed!");
				window_changed = true;
				break;
			}

			cur_zorder = cur_frame_info->input_buffer[i].zorder;
			last_zorder = last_frame_info->input_buffer[i].zorder;
			if (cur_zorder != last_zorder) {
				v2d_print(dev->index, PRINT_OTHER,
					 "frame zorder changed!");
				window_changed = true;
				break;
			}
		}
	}

	return window_changed;
}

static void get_output_axis_crop(struct v2d_dev *dev, struct vframe_s *src_vf,
		struct received_frames_t *received_frame,
		struct frame_info_t *vframe_info_cur, int j)
{
	struct input_crop_s crop_info;
	struct input_axis_s dst_axis;
	struct input_axis_s display_axis;
	struct output_axis area_bound;

	if (!dev || !received_frame || !vframe_info_cur) {
		pr_info("%s input error\n", __func__);
		return;
	}
	if (j == 0) {
		received_frame->output_axis.min_left = V2D_AXIS_LEFT_INIT_MAX;
		received_frame->output_axis.min_top = V2D_AXIS_TOP_INIT_MAX;
	}
	memcpy(&area_bound, &received_frame->output_axis, sizeof(struct output_axis));

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

	dst_axis = adjust_output_axis(dev, vframe_info_cur);
	display_axis.left = dst_axis.left * (int)dev->buf_width / (int)dev->vinfo_w;
	display_axis.top = dst_axis.top * (int)dev->buf_height / (int)dev->vinfo_h;
	display_axis.width = dst_axis.width * (int)dev->buf_width / (int)dev->vinfo_w;
	display_axis.height = dst_axis.height * (int)dev->buf_height / (int)dev->vinfo_h;

	memcpy(&received_frame->crop_info[j], &crop_info, sizeof(struct input_crop_s));
	memcpy(&received_frame->axis_info[j], &display_axis, sizeof(struct input_axis_s));

	if (area_bound.min_left > dst_axis.left)
		area_bound.min_left = dst_axis.left;
	if (area_bound.min_top > dst_axis.top)
		area_bound.min_top = dst_axis.top;
	if (area_bound.max_right < (dst_axis.left + dst_axis.width))
		area_bound.max_right = dst_axis.left + dst_axis.width;
	if (area_bound.max_bottom < (dst_axis.top + dst_axis.height))
		area_bound.max_bottom = dst_axis.top + dst_axis.height;

	if (area_bound.min_top < 0)
		area_bound.min_top = 0;
	if (area_bound.min_left < 0)
		area_bound.min_left = 0;
	if (area_bound.max_bottom > dev->vinfo_h)
		area_bound.max_bottom = dev->vinfo_h;
	if (area_bound.max_right > dev->vinfo_w)
		area_bound.max_right = dev->vinfo_w;

	memcpy(&received_frame->output_axis, &area_bound, sizeof(struct output_axis));
}

static void set_output_buffer_area(struct v2d_dev *dev, struct frames_info_t *frames_info,
	struct received_frames_t *received_frame, struct vframe_s *src_vf)
{
	struct frame_info_t *vframe_info_cur = NULL;
	struct output_axis area_bound;
	u32 out_crop[4] = {0};
	u32 dewarp_crop_top = 0, dewarp_crop_left = 0;
	u32 dewarp_crop_bottom = 0, dewarp_crop_right = 0;
	u32 src_w = 0, src_h = 0;
	u32 dewarp_dst_w = 0, dewarp_dst_h = 0;

	memcpy(&area_bound, &received_frame->output_axis, sizeof(struct output_axis));
	v2d_print(dev->index, PRINT_OTHER,
		"%s: min_top:%d min_left:%d, max_bottom:%d, max_right:%d.\n",
		__func__,
		area_bound.min_top,
		area_bound.min_left,
		area_bound.max_bottom,
		area_bound.max_right);

	vframe_info_cur = &frames_info->input_buffer[0];
	if (!vframe_info_cur)
		return;

	if (src_vf) {
		if (src_vf->type & VIDTYPE_COMPRESS) {
			src_w = src_vf->compWidth;
			src_h = src_vf->compHeight;
		} else {
			src_w = src_vf->width;
			src_h = src_vf->height;
		}
	} else {
		src_w = vframe_info_cur->buffer_w;
		src_h = vframe_info_cur->buffer_h;
	}

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		out_crop[0] = area_bound.min_top * dev->buf_width / dev->vinfo_w;
		out_crop[1] = area_bound.min_left * dev->buf_height / dev->vinfo_h;
		out_crop[2] = dev->buf_height - area_bound.max_bottom *
			dev->buf_height / dev->vinfo_h;
		out_crop[3] = dev->buf_width - area_bound.max_right *
			dev->buf_height / dev->vinfo_h;

		frames_info->out_buffer.crop_y = out_crop[0];
		frames_info->out_buffer.crop_x = out_crop[1];
		frames_info->out_buffer.crop_h = dev->buf_height - out_crop[2] - out_crop[0];
		frames_info->out_buffer.crop_w = dev->buf_width - out_crop[3] - out_crop[1];
		frames_info->out_buffer.dst_x = received_frame->output_axis.min_left;
		frames_info->out_buffer.dst_y = received_frame->output_axis.min_top;
		frames_info->out_buffer.dst_w = received_frame->output_axis.max_right -
			received_frame->output_axis.min_left + 1;
		frames_info->out_buffer.dst_h = received_frame->output_axis.max_bottom -
			received_frame->output_axis.min_top + 1;
	} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
		if (dev->work_mode == V2D_MODE_ROTATE) {
			frames_info->out_buffer.crop_y = 0;
			frames_info->out_buffer.crop_x = 0;
			frames_info->out_buffer.crop_h = src_w;
			frames_info->out_buffer.crop_w = src_h;
		} else {
			out_crop[0] = area_bound.min_top * dev->buf_width / dev->vinfo_w;
			out_crop[1] = area_bound.min_left * dev->buf_height / dev->vinfo_h;
			out_crop[2] = dev->buf_height - area_bound.max_bottom *
				dev->buf_height / dev->vinfo_h;
			out_crop[3] = dev->buf_width - area_bound.max_right *
				dev->buf_height / dev->vinfo_h;
			frames_info->out_buffer.crop_y = out_crop[0];
			frames_info->out_buffer.crop_x = out_crop[1];
			frames_info->out_buffer.crop_h = dev->buf_height - out_crop[2] -
				out_crop[0];
			frames_info->out_buffer.crop_w = dev->buf_width - out_crop[3] -
				out_crop[1];
		}

		frames_info->out_buffer.dst_x = received_frame->output_axis.min_left;
		frames_info->out_buffer.dst_y = received_frame->output_axis.min_top;
		frames_info->out_buffer.dst_w = received_frame->output_axis.max_right -
			received_frame->output_axis.min_left - 1;
		frames_info->out_buffer.dst_h = received_frame->output_axis.max_bottom -
			received_frame->output_axis.min_top - 1;
	} else {
		v2d_print(dev->index, PRINT_DEWARP, "dewarp process out_buffer crop.\n");
		if (vframe_info_cur->crop_w > 0 || vframe_info_cur->crop_h  > 0) {
			if (src_vf && is_src_crop_valid(src_vf->src_crop)) {
				dewarp_crop_top =
					MAX(vframe_info_cur->crop_y, src_vf->src_crop.top);
				dewarp_crop_left =
					MAX(vframe_info_cur->crop_x, src_vf->src_crop.left);
				dewarp_crop_bottom = MAX(src_h - vframe_info_cur->crop_y
					- vframe_info_cur->crop_h, src_vf->src_crop.bottom);
				dewarp_crop_right = MAX(src_w - vframe_info_cur->crop_x
					- vframe_info_cur->crop_w, src_vf->src_crop.right);
			} else {
				dewarp_crop_top = vframe_info_cur->crop_y;
				dewarp_crop_left = vframe_info_cur->crop_x;
				dewarp_crop_bottom = src_h - vframe_info_cur->crop_y
					- vframe_info_cur->crop_h;
				dewarp_crop_right = src_w - vframe_info_cur->crop_x
					- vframe_info_cur->crop_w;
			}

			dewarp_dst_w = (vframe_info_cur->dst_w * dev->buf_width /
				dev->vinfo_w + 0xf) & ~0xf;
			dewarp_dst_h = (vframe_info_cur->dst_h * dev->buf_height /
				dev->vinfo_h + 0xf) & ~0xf;
			if (src_w <= 0 || src_h <= 0) {
				v2d_print(dev->index, PRINT_ERROR,
					"%s: dewarp_src_w:%d dewarp_src_h:%d.\n",
					__func__, src_w, src_h);
				src_w = BUFFER_4K_WIDTH;
				src_h = BUFFER_4K_HEIGHT;
			}
			if (vframe_info_cur->transform == V2D_TRANSFORM_ROT_270) {
				out_crop[0] = dewarp_crop_right * dewarp_dst_h / src_w;
				out_crop[1] = dewarp_crop_top * dewarp_dst_w / src_h;
				out_crop[2] = dewarp_crop_left * dewarp_dst_h / src_w;
				out_crop[3] = dewarp_crop_bottom * dewarp_dst_w / src_h;
			} else if (vframe_info_cur->transform == V2D_TRANSFORM_ROT_180) {
				out_crop[0] = dewarp_crop_bottom * dewarp_dst_h / src_h;
				out_crop[1] = dewarp_crop_right * dewarp_dst_w / src_w;
				out_crop[2] = dewarp_crop_top * dewarp_dst_h / src_h;
				out_crop[3] = dewarp_crop_left * dewarp_dst_w / src_w;
			} else if (vframe_info_cur->transform == V2D_TRANSFORM_ROT_90 ||
				vframe_info_cur->transform == V2D_TRANSFORM_FLIP_H_ROT_90 ||
				vframe_info_cur->transform == V2D_TRANSFORM_FLIP_V_ROT_90) {
				out_crop[0] = dewarp_crop_left * dewarp_dst_h / src_w;
				out_crop[1] = dewarp_crop_bottom * dewarp_dst_w / src_h;
				out_crop[2] = dewarp_crop_right * dewarp_dst_h / src_w;
				out_crop[3] = dewarp_crop_top * dewarp_dst_w / src_h;
			}
		}

		frames_info->out_buffer.crop_y = out_crop[0];
		frames_info->out_buffer.crop_x = out_crop[1];
		frames_info->out_buffer.crop_h = dewarp_dst_h - out_crop[2] - out_crop[0];
		frames_info->out_buffer.crop_w = dewarp_dst_w - out_crop[3] - out_crop[1];
		frames_info->out_buffer.dst_x = vframe_info_cur->dst_x;
		frames_info->out_buffer.dst_y = vframe_info_cur->dst_y;
		frames_info->out_buffer.dst_w = vframe_info_cur->dst_w;
		frames_info->out_buffer.dst_h = vframe_info_cur->dst_h;
	}
}

static struct vframe_s *v2d_get_vf_from_file(struct v2d_dev *dev,
					 struct file *file_vf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct file_private_data *file_private_data = NULL;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		v2d_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		v2d_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf = dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			v2d_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
	} else if (is_v4l_vf) {
		v2d_print(dev->index, PRINT_OTHER, "vf is from v4lvideo\n");
		file_private_data = v2d_get_file_private_data(file_vf, true);
		if (!file_private_data)
			v2d_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		else
			vf = &file_private_data->vf;
	}
	return vf;
}

static int v2d_wait_file_fence(struct v2d_dev *dev,
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
		v2d_print(dev->index, PRINT_FENCE, "wait: fence_file is NULL\n");
		return 1;
	}

	if (!IS_ERR_OR_NULL(sync_file)) {
		fence_obj = sync_file->fence;
	} else {
		v2d_print(dev->index, PRINT_FENCE, "sync_file is NULL\n");
		fput(fence_file);
		return 1;
	}

	if (fence_obj) {
		v2d_print(dev->index, PRINT_FENCE, "sync_file=%px, fence_obj=%px, seqno=%lld\n",
			sync_file, fence_obj, fence_obj->seqno);
		timestamp = local_clock();
		ret = dma_fence_wait_timeout(fence_obj,
					     false, msecs_to_jiffies(3000));
		if (ret == 0) {
			v2d_print(dev->index, PRINT_ERROR, "fence wait timeout\n");
			fput(fence_file);
			return 0;
		}

		time_cost = local_clock() - timestamp;
		dev->fence_wait_time_total += time_cost;
		dev->fence_wait_count++;
		if (dev->fence_wait_count == 100) {
			v2d_print(dev->index, PRINT_FENCE,
				"wait fence avg=%lldns\n",
				div64_u64(dev->fence_wait_time_total, dev->fence_wait_count));
				dev->fence_wait_count = 0;
				dev->fence_wait_time_total = 0;
		}

		v2d_print(dev->index, PRINT_FENCE,
			 "wait fence, state: %d, wait cost time:%lldms\n",
			 ret,
			 div64_u64(time_cost, 1000000));
	}

	fput(fence_file);
	return 1;
}

static int v2d_init_ge2d_buffer(struct v2d_dev *dev, bool is_tvp)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;
	struct codec_mm_s *mm;

	buf_width = dev->buf_width;
	buf_height = dev->buf_height;

	if (buf_config_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);
	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0) {
			mm = codec_mm_alloc(ports[dev->index].name, buf_size, 0, flags, -1);
			if (!mm) {
				v2d_print(dev->index, PRINT_ERROR, "init ge2d buffer failed\n");
				dev->buffer_status = INIT_BUFFER_ERROR;
				return -1;
			}
			dev->dst_buf[i].phy_addr = mm->phy_addr;
			dev->dst_buf[i].mif_handle = (ulong)mm;

			if (!kfifo_put(&dev->free_q, &dev->dst_buf[i]))
				v2d_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
			dev->dst_buf[i].buf_status = BUFFER_STATUS_NORMAL;

			v2d_print(dev->index, PRINT_ERROR,
				 "%s: cma memory is %x , size is  %x\n",
				 ports[dev->index].name,
				 (unsigned int)dev->dst_buf[i].phy_addr,
				 (unsigned int)buf_size);
		} else if (dev->dst_buf[i].buf_used == BUFFER_MODE_VICP) {
			v2d_print(dev->index, PRINT_ERROR,
				 "%s: composer device changed, last: %d, current:%d\n",
				 __func__,
				 dev->dst_buf[i].buf_used,
				 BUFFER_MODE_GE2D);
			dev->dst_buf[i].buf_status = BUFFER_STATUS_CHANGE;
		}

		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;
		dev->dst_buf[i].buf_used = BUFFER_MODE_GE2D;

	}
	return 0;
}

static int v2d_init_dewarp_buffer(struct v2d_dev *dev, bool is_tvp)
{
	int i, flags;
	u32 buf_width, buf_height, buf_size;
	struct codec_mm_s *mm;

	buf_width = dev->buf_width;
	buf_height = dev->buf_height;

	if (buf_config_444)
		buf_size = buf_width * buf_height * 3;
	else
		buf_size = buf_width * buf_height * 3 / 2;

	buf_size = PAGE_ALIGN(buf_size);

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0) {
			mm = codec_mm_alloc(ports[dev->index].name, buf_size, 0, flags, -1);
			if (!mm) {
				v2d_print(dev->index, PRINT_ERROR, "init dewarp buffer failed\n");
				dev->buffer_status = INIT_BUFFER_ERROR;
				return -1;
			}
			dev->dst_buf[i].phy_addr = mm->phy_addr;
			dev->dst_buf[i].mif_handle = (ulong)mm;

			if (!kfifo_put(&dev->free_q, &dev->dst_buf[i]))
				v2d_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
			dev->dst_buf[i].buf_status = BUFFER_STATUS_NORMAL;

			v2d_print(dev->index, PRINT_ERROR,
				 "%s: cma memory is %lx , size is  %x\n",
				 ports[dev->index].name,
				 (unsigned long)dev->dst_buf[i].phy_addr,
				 (unsigned int)buf_size);
		} else {
			v2d_print(dev->index, PRINT_ERROR,
				 "%s: composer device changed, last: %d, current:%d\n",
				 __func__,
				 dev->dst_buf[i].buf_used,
				 BUFFER_MODE_DEWARP);
			dev->dst_buf[i].buf_status = BUFFER_STATUS_CHANGE;
		}

		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;
		dev->dst_buf[i].buf_used = BUFFER_MODE_DEWARP;

	}
	return 0;
}

static int v2d_init_vicp_buffer(struct v2d_dev *dev, bool is_tvp)
{
	int i, j, flags;
	u32 buf_width, buf_height, buf_size;
	int dw_size = 0, afbc_body_size = 0, afbc_head_size = 0, afbc_table_size = 0;
	u32 *virt_addr = NULL, *temp_addr = NULL;
	u32 temp_body_addr;
	ulong buf_handle, buf_phy_addr;
	struct codec_mm_s *mm;

	buf_width = dev->buf_width;
	buf_height = dev->buf_height;

	if (vicp_output_mode == 1) {
		buf_size = buf_width * buf_height * 3;
		if (buf_config_444 == 0)
			buf_size = buf_size * 3 / 2;
	} else {
		if (vicp_output_mode == 3)//mif
			dw_size = roundup(buf_width >> 2, 32) * roundup(buf_height >> 2, 2);

		afbc_body_size = buf_width * buf_height + (1024 * 1658);
		if (buf_config_444 == 0) {
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

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr == 0) {
			mm = codec_mm_alloc(ports[dev->index].name, buf_size, 0, flags, -1);
			if (!mm) {
				v2d_print(dev->index, PRINT_ERROR, "init vicp buffer failed\n");
				dev->buffer_status = INIT_BUFFER_ERROR;
				return -1;
			}
			dev->dst_buf[i].phy_addr = mm->phy_addr;
			dev->dst_buf[i].mif_handle = (ulong)mm;

			if (!kfifo_put(&dev->free_q, &dev->dst_buf[i]))
				v2d_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");

			dev->dst_buf[i].buf_status = BUFFER_STATUS_NORMAL;

			v2d_print(dev->index, PRINT_ERROR,
				"%s: cma memory is 0x%lx , size is 0x%x.\n",
				ports[dev->index].name, dev->dst_buf[i].phy_addr, buf_size);
		} else {
			v2d_print(dev->index, PRINT_ERROR,
				 "%s: composer device changed, last: %d, current:%d\n",
				 __func__,
				 dev->dst_buf[i].buf_used,
				 BUFFER_MODE_VICP);
			dev->dst_buf[i].buf_status = BUFFER_STATUS_CHANGE;
		}

		dev->dst_buf[i].index = i;
		dev->dst_buf[i].dirty = true;
		dev->dst_buf[i].buf_w = buf_width;
		dev->dst_buf[i].buf_h = buf_height;
		dev->dst_buf[i].buf_size = buf_size;
		dev->dst_buf[i].is_tvp = is_tvp;
		dev->dst_buf[i].buf_used = BUFFER_MODE_VICP;

		if (vicp_output_mode != 1) {
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
				dev->buffer_status = INIT_BUFFER_ERROR;
				v2d_print(dev->index, PRINT_ERROR, "alloc table buf fail.\n");
				return -1;
			}
			temp_body_addr = dev->dst_buf[i].afbc_body_addr & 0xffffffff;
			memset(virt_addr, 0, afbc_table_size);
			temp_addr = virt_addr;
			for (j = 0; j < afbc_body_size; j += 4096) {
				*virt_addr = ((j + temp_body_addr) >> 12) & 0x000fffff;
				virt_addr++;
			}
		}
	}

	return 0;
}

static int config_ge2d_param(struct v2d_dev *dev,
		struct frame_info_t *vframe_info_cur, struct v2d_dev_config *v2d_composer_param)
{
	int ret = 0;
	u32 cur_transform;
	struct vframe_s *input_vf = NULL;
	struct ge2d_src_para_s *ge2d_data = NULL;

	if (!dev || !vframe_info_cur || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	input_vf = v2d_composer_param->input_vf;
	ge2d_data = &v2d_composer_param->composer_device_param.ge2d_data;

	v2d_print(dev->index, PRINT_OTHER, "use ge2d composer.\n");

	ge2d_data->buf_format = vframe_info_cur->buffer_format;
	ret = v2d_config_ge2d_data(v2d_composer_param->input_vf,
		v2d_composer_param->addr,
		vframe_info_cur->buffer_w,
		vframe_info_cur->buffer_h,
		vframe_info_cur->align_w,
		vframe_info_cur->align_h,
		v2d_composer_param->frame_crop.left,
		v2d_composer_param->frame_crop.top,
		v2d_composer_param->frame_crop.width,
		v2d_composer_param->frame_crop.height,
		ge2d_data);
	if (ret < 0) {
		v2d_print(dev->index, PRINT_ERROR, "config ge2d data error\n");
		return -1;
	}
	cur_transform = vframe_info_cur->transform;
	if (input_vf && input_vf->flag & VFRAME_FLAG_MIRROR_H) {
		if (cur_transform & V2D_TRANSFORM_FLIP_H)
			cur_transform &= ~V2D_TRANSFORM_FLIP_H;
		else
			cur_transform |= V2D_TRANSFORM_FLIP_H;
	}
	if (input_vf && input_vf->flag & VFRAME_FLAG_MIRROR_V) {
		if (cur_transform & V2D_TRANSFORM_FLIP_V)
			cur_transform &= ~V2D_TRANSFORM_FLIP_V;
		else
			cur_transform |= V2D_TRANSFORM_FLIP_V;
	}
	if (cur_transform == V2D_TRANSFORM_FLIP_H_ROT_90 ||
		cur_transform == V2D_TRANSFORM_FLIP_V_ROT_90)
		cur_transform = V2D_TRANSFORM_ROT_90;

	v2d_print(dev->index, PRINT_AXIS,
		"display_axis: left top width height: %d %d %d %d\n",
		v2d_composer_param->frame_axis.left, v2d_composer_param->frame_axis.top,
		v2d_composer_param->frame_axis.width, v2d_composer_param->frame_axis.height);

	dev->ge2d_para.angle = cur_transform;
	dev->ge2d_para.position_left = v2d_composer_param->frame_axis.left;
	dev->ge2d_para.position_top = v2d_composer_param->frame_axis.top;
	dev->ge2d_para.position_width = v2d_composer_param->frame_axis.width;
	dev->ge2d_para.position_height = v2d_composer_param->frame_axis.height;

	return 0;
}

static int process_ge2d_data(struct v2d_dev *dev, struct v2d_dev_config *v2d_composer_param)
{
	struct ge2d_src_para_s *ge2d_data = NULL;
	int ret = 0;

	if (!dev || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	ge2d_data = &v2d_composer_param->composer_device_param.ge2d_data;
	ret = v2d_ge2d_data_composer(ge2d_data, &dev->ge2d_para);
	return ret;
}

static int config_dewarp_param(struct v2d_dev *dev,
		struct frame_info_t *vframe_info_cur, struct v2d_dev_config *v2d_composer_param)
{
	int ret = 0;
	struct dewarp_vf_para_s *dewarp_data = NULL;
	struct dst_buf_t *dst_buf = NULL;
	struct dewarp_common_para common_para;
	int out_align_width, out_align_height;

	if (!dev || !vframe_info_cur || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	v2d_print(dev->index, PRINT_OTHER, "use dewarp composer.\n");

	dewarp_data = &v2d_composer_param->composer_device_param.dewarp_data;
	dst_buf = v2d_composer_param->dst_buf;

	//common_para ensures compatibility with older API functions.
	common_para.input_para.vframe = v2d_composer_param->input_vf;
	common_para.input_para.call_index = dev->index;
	common_para.input_para.transform = vframe_info_cur->transform;
	common_para.input_para.pic_info.format = vframe_info_cur->buffer_format;
	common_para.input_para.pic_info.width = vframe_info_cur->buffer_w;
	common_para.input_para.pic_info.height = vframe_info_cur->buffer_h;
	common_para.input_para.pic_info.addr[0] = v2d_composer_param->addr;
	common_para.input_para.pic_info.align_w = vframe_info_cur->align_w;
	common_para.input_para.pic_info.align_h = vframe_info_cur->align_h;

	out_align_width = (vframe_info_cur->dst_w * dst_buf->buf_w / dev->vinfo_w + 0xf) & ~0xf;
	out_align_height = (vframe_info_cur->dst_h * dst_buf->buf_h / dev->vinfo_h + 0xf) & ~0xf;

	common_para.output_para.pic_info.align_w = out_align_width > MIN_DEWARP_WIDTH ?
		out_align_width : MIN_DEWARP_WIDTH;
	common_para.output_para.pic_info.align_h = out_align_height > MIN_DEWARP_HEIGHT ?
		out_align_height : MIN_DEWARP_HEIGHT;
	common_para.output_para.pic_info.addr[0] = dst_buf->phy_addr;
	common_para.input_para.pic_info.is_tvp = v2d_composer_param->is_tvp;

	dst_buf->width = common_para.output_para.pic_info.align_w;
	dst_buf->height = common_para.output_para.pic_info.align_h;
	ret = v2d_config_dewarp_vframe(dewarp_data, &common_para);
	if (ret < 0)
		v2d_print(dev->index, PRINT_ERROR, "dewarp config err.\n");
	dev->dewarp_para.vf_para = dewarp_data;
	ret = v2d_load_dewarp_firmware(&dev->dewarp_para);
	if (ret != 0) {
		v2d_print(dev->index, PRINT_ERROR, "load firmware failed.\n");
		return -1;
	}

	return 0;
}

static int process_dewarp_data(struct v2d_dev *dev, struct v2d_dev_config *v2d_composer_param)
{
	int ret = 0;

	if (!dev || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	//the second param is not used, is_tvp is in dev->dewarp_para
	ret = v2d_dewarp_data_composer(&dev->dewarp_para, false);

	return ret;
}

static int config_vicp_param(struct v2d_dev *dev,
		struct frame_info_t *vframe_info_cur, struct v2d_dev_config *v2d_composer_param)
{
	struct vframe_s *input_vf = NULL;
	struct vicp_data_config_s *vicp_data = NULL;
	struct dst_buf_t *dst_buf = NULL;
	int mifout_en = 1, fbcout_en = 1;
	int fbc_init_ctrl, fbc_pip_mode;
	ulong buf_addr[3];
	struct output_axis_s display_axis;
	struct crop_info_s crop_info;
	u32 vicp_rotate_mode = 0;

	if (!dev || !vframe_info_cur || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	input_vf = v2d_composer_param->input_vf;
	vicp_data = &v2d_composer_param->composer_device_param.vicp_data;
	dst_buf = v2d_composer_param->dst_buf;

	v2d_config_vicp_input_data(input_vf,
			v2d_composer_param->addr,
			vframe_info_cur->buffer_w,
			vframe_info_cur->buffer_h,
			vframe_info_cur->align_w,
			vframe_info_cur->align_h,
			1,
			VICP_COLOR_FORMAT_YUV420,
			8,
			&vicp_data->input_data);

	mifout_en = (vicp_output_mode != 2);
	fbcout_en = (vicp_output_mode != 1);

	buf_addr[0] = dst_buf->phy_addr;
	if (fbcout_en) {
		buf_addr[1] = dst_buf->afbc_head_addr;
		buf_addr[2] = dst_buf->afbc_table_addr;
	}

	if (v2d_composer_param->count == 1 || v2d_composer_param->index == 0) {
		fbc_init_ctrl = 1;
		fbc_pip_mode = 1;
	} else {
		fbc_init_ctrl = 0;
		fbc_pip_mode = 1;
	}

	v2d_config_vicp_output_data(fbcout_en,
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
		&vicp_data->output_data);

	vicp_data->data_option.rotation_mode =
			map_rotationmode_from_v2d_to_vicp(vframe_info_cur->transform);

	vicp_data->data_option.shrink_mode = (enum vicp_shrink_mode_e)vicp_shrink_mode;
	if (v2d_composer_param->count > 1)
		vicp_data->data_option.rdma_enable = true;
	else
		vicp_data->data_option.rdma_enable = false;
	vicp_data->data_option.security_enable = v2d_composer_param->is_tvp;
	vicp_data->data_option.skip_mode = VICP_SKIP_MODE_OFF;
	vicp_data->data_option.compress_rate = lossy_compress_rate;

	crop_info.left = vframe_info_cur->crop_x;
	crop_info.top = vframe_info_cur->crop_y;
	crop_info.width = vframe_info_cur->crop_w;
	crop_info.height = vframe_info_cur->crop_h;
	display_axis.left =  vframe_info_cur->dst_x;
	display_axis.top = vframe_info_cur->dst_y;
	display_axis.width = vframe_info_cur->dst_w;
	display_axis.height = vframe_info_cur->dst_h;
	if (dev->work_mode == V2D_MODE_ROTATE)
		vicp_rotate_mode = 1;
	config_vicp_axis_crop(vicp_data,
		vicp_rotate_mode,
		v2d_composer_param->index,
		v2d_composer_param->count,
		0,
		vframe_info_cur->transform,
		&display_axis,
		&crop_info,
		input_vf);

	return 0;
}

static int process_vicp_data(struct v2d_dev *dev, struct v2d_dev_config *v2d_composer_param)
{
	struct vicp_data_config_s *vicp_data = NULL;
	int ret = 0;

	if (!dev || !v2d_composer_param) {
		pr_info("%s:input param err\n", __func__);
		return -1;
	}

	vicp_data = &v2d_composer_param->composer_device_param.vicp_data;
	ret = v2d_vicp_data_composer(vicp_data);

	return ret;
}

static void v2d_device_data_free(struct kref *ref)
{
	struct v2d_dev *dev = container_of(ref, struct v2d_dev, refcount);

	vfree(dev);
}

static void v2d_fence_signal_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct v2d_fence_cb_t *v2d_cb = container_of(cb, struct v2d_fence_cb_t, base_cb);
	struct v2d_dev *dev = v2d_cb->dev;
	struct file *need_recycle_file = v2d_cb->buffer_file;
	struct file *fence_file = v2d_cb->fence_file;
	struct display_data_t *display_data = NULL;
	int len, i;

	spin_lock(&v2d_spin_lock);
	len = kfifo_len(&dev->display_q);
	for (i = 0; i < len; i++) {
		if (!kfifo_get(&dev->display_q, &display_data)) {
			v2d_print(dev->index, PRINT_ERROR,
				"%s: error, display_q underflow!\n", __func__);
			spin_unlock(&v2d_spin_lock);
			goto out_clean;
		}
		if (display_data->output_dmabuf->file == need_recycle_file)
			break;

		if (!kfifo_put(&dev->display_q, display_data))
			v2d_print(dev->index, PRINT_ERROR, "error, display_q overflow!\n");
	}
	spin_unlock(&v2d_spin_lock);

	if (i == len) {
		v2d_print(dev->index, PRINT_OTHER,
			"recycle failed, may be at uninit process?\n");
		dev->buffer_release_count++;
		goto out_clean;
	}

	dev->buffer_release_count++;
	display_data->private_data->is_keep = false;
	atomic_set(&display_data->on_use, false);
	if (!kfifo_put(&dev->fence_cb_q, v2d_cb))
		v2d_print(dev->index, PRINT_ERROR,
			"error, fence_cb_q is full\n");

	if (!kfifo_put(&dev->file_free_q, display_data->output_dmabuf))
		v2d_print(dev->index, PRINT_ERROR,
			"error, file_free_q is full\n");

	if (!kfifo_put(&dev->free_q, display_data->output_buffer))
		v2d_print(dev->index, PRINT_ERROR, "error, free_q is empty\n");

	v2d_print(dev->index, PRINT_FENCE,
		"wait dma_fence:%px done, display_q:%d free_q:%d\n",
		fence, kfifo_len(&dev->display_q), kfifo_len(&dev->free_q));
out_clean:
	fput(need_recycle_file);
	fput(fence_file);

	kref_put(&dev->refcount, v2d_device_data_free);

}

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
static void set_v2d_mediaproxy_info(struct v2d_dev *dev, struct vframe_s *src_vf,
	struct composer_info_t *composer_info, int index)
{
	if (!dev || !src_vf || !composer_info) {
		pr_info("%s: input param error\n", __func__);
		return;
	}

	composer_info->input_info[index].timestamp = src_vf->timestamp;
	composer_info->input_info[index].decoder_instid = src_vf->decoder_instid;
	composer_info->input_info[index].frame_index = src_vf->frame_index;
}
#endif

static void config_output_vf_param(struct v2d_dev *dev, struct vframe_s *output_vf,
		bool is_tvp, struct dst_buf_t *output_buffer, struct frame_info_t *vframe_info)
{
	enum meson_cpuid_type_e cpuid_type = MESON_CPU_MAJOR_ID_UNKNOWN;

	if (!dev || !output_vf) {
		pr_info("%s:input param err\n", __func__);
		return;
	}

	output_vf->flag |= VFRAME_FLAG_COMPOSER_DONE;
	output_vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	if (!display_yuv444) {
		output_vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		output_vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	} else {
		output_vf->type = (VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE | VIDTYPE_VIU_FIELD);
	}
	if (vframe_info->transform == V2D_TRANSFORM_FLIP_H_ROT_90)
		output_vf->flag |= VFRAME_FLAG_MIRROR_V;
	if (vframe_info->transform == V2D_TRANSFORM_FLIP_V_ROT_90)
		output_vf->flag |= VFRAME_FLAG_MIRROR_H;
	if (is_tvp)
		output_vf->flag |= VFRAME_FLAG_VIDEO_SECURE;
	output_vf->canvas0Addr = -1;
	output_vf->canvas1Addr = -1;
	if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		output_vf->width = output_buffer->width;
		output_vf->height = output_buffer->height;
	} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
		if (vicp_output_mode != 1) {//fbc_out en
			output_vf->type |= (VIDTYPE_COMPRESS | VIDTYPE_SCATTER);
			output_vf->compWidth = output_buffer->buf_w;
			output_vf->compHeight = output_buffer->buf_h;
			output_vf->compHeadAddr = output_buffer->afbc_head_addr;
			output_vf->compBodyAddr = output_buffer->afbc_body_addr;
			cpuid_type = get_cpu_type();
			if (cpuid_type == MESON_CPU_MAJOR_ID_T3X ||
				cpuid_type == MESON_CPU_MAJOR_ID_S6 ||
				cpuid_type == MESON_CPU_MAJOR_ID_A9) {
				output_vf->fgs_valid = false;
				output_vf->fgs_table_adr = 0;
				if (lossy_compress_rate) {
					output_vf->type |= VIDTYPE_COMPRESS_LOSS;
					output_vf->vf_lossycomp_param.lossy_mode = 1;
					output_vf->vf_lossycomp_param.burst_length_add_en = 0;
					output_vf->vf_lossycomp_param.burst_length_add_value = 2;
					output_vf->vf_lossycomp_param.quant_diff_root_leave = 2;
				} else {
					output_vf->vf_lossycomp_param.lossy_mode = 0;
					output_vf->vf_lossycomp_param.burst_length_add_en = 0;
					output_vf->vf_lossycomp_param.burst_length_add_value = 2;
					output_vf->vf_lossycomp_param.quant_diff_root_leave = 2;
				}
				output_vf->vf_lossycomp_param.ofset_burst4_en = 0;
			}
		}

		if (vicp_shrink_mode >= VICP_SHRINK_MODE_MAX) {
			output_vf->width = output_buffer->buf_w;
			output_vf->height = output_buffer->buf_h;
		} else {
			output_vf->width = output_buffer->buf_w >> (1 + vicp_shrink_mode);
			output_vf->height = output_buffer->buf_h >> (1 + vicp_shrink_mode);
		}

	} else {
		output_vf->width = output_buffer->buf_w;
		output_vf->height = output_buffer->buf_h;
	}
	v2d_print(dev->index, PRINT_AXIS,
			 "composer:vf_w: %d, vf_h: %d\n", output_vf->width, output_vf->height);
	if (display_yuv444) {
		output_vf->canvas0_config[0].phy_addr = output_buffer->phy_addr;
		output_vf->canvas0_config[0].width = output_buffer->buf_w * 3;
		output_vf->canvas0_config[0].height = output_buffer->buf_h;
		output_vf->canvas0_config[0].block_mode = 0;
		output_vf->plane_num = 1;
	} else {
		output_vf->canvas0_config[0].phy_addr = output_buffer->phy_addr;
		output_vf->canvas0_config[0].width = output_vf->width;
		output_vf->canvas0_config[0].height = output_vf->height;
		output_vf->canvas0_config[0].block_mode = 0;

		output_vf->canvas0_config[1].phy_addr = output_buffer->phy_addr
			+ output_vf->width * output_vf->height;
		output_vf->canvas0_config[1].width = output_vf->width;
		output_vf->canvas0_config[1].height = output_vf->height >> 1;
		output_vf->canvas0_config[1].block_mode = 0;
		output_vf->plane_num = 2;
	}
	output_vf->mem_handle = (void *)output_buffer->mif_handle;
	if (dev->dev_choice == COMPOSER_WITH_VICP && vicp_output_mode != 1)
		output_vf->mem_handle_1 = (void *)output_buffer->afbc_table_handle;
	output_vf->repeat_count = 0;
}

static int v2d_init_buffer(struct v2d_dev *dev, bool is_tvp)
{
	int ret = 0;

	switch (dev->buffer_status) {
	case UNINITIAL:/*not config size, return failure*/
		return -1;
	case INIT_SIZE_SUCCESS:
		break;
	case INIT_BUFFER_SUCCESS:/*config before , return ok*/
		return 0;
	case INIT_BUFFER_ERROR:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		ret = v2d_init_ge2d_buffer(dev, is_tvp);
		if (IS_ERR_OR_NULL(dev->ge2d_para.context))
			ret |= v2d_init_ge2d_composer(&dev->ge2d_para);
		dev->config_param_func = config_ge2d_param;
		dev->process_data_func = process_ge2d_data;
	} else if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		v2d_init_dewarp_buffer(dev, is_tvp);
		if (IS_ERR_OR_NULL(dev->dewarp_para.context))
			ret |= v2d_init_dewarp_composer(&dev->dewarp_para);
		dev->config_param_func = config_dewarp_param;
		dev->process_data_func = process_dewarp_data;
		v2d_print(dev->index, PRINT_ERROR, "only ge2d is config by now\n");
	} else {
		ret = v2d_init_vicp_buffer(dev, is_tvp);
		dev->config_param_func = config_vicp_param;
		dev->process_data_func = process_vicp_data;
		v2d_print(dev->index, PRINT_OTHER, "VICP buffer init success\n");
	}
	if (ret < 0)
		return -1;
	dev->buffer_status = INIT_BUFFER_SUCCESS;
	return 0;
}

static void v2d_uninit_buffer(struct v2d_dev *dev)
{
	int i;
	int ret = 0;

	if (dev->buffer_status == UNINITIAL) {
		v2d_print(dev->index, PRINT_OTHER,
			 "%s buffer have uninit already finished!\n", __func__);
		return;
	}

	if (!IS_ERR_OR_NULL(dev->dewarp_para.context)) {
		ret = v2d_uninit_dewarp_composer(&dev->dewarp_para);
		if (ret < 0)
			v2d_print(dev->index, PRINT_ERROR, "uninit dewarp composer fail!\n");
		dev->dewarp_para.context = NULL;
	}

	if (!IS_ERR_OR_NULL(dev->ge2d_para.context)) {
		ret = v2d_uninit_ge2d_composer(&dev->ge2d_para);
		if (ret < 0)
			v2d_print(dev->index, PRINT_ERROR, "uninit ge2d composer failed!\n");
		dev->ge2d_para.context = NULL;
	}

	dev->buffer_status = UNINITIAL;
	for (i = 0; i < BUFFER_LEN; i++) {
		if (dev->dst_buf[i].phy_addr != 0) {
			pr_info("%s: cma free addr is %x\n",
				ports[dev->index].name,
				(unsigned int)dev->dst_buf[i].phy_addr);
			codec_mm_release((struct codec_mm_s *)dev->dst_buf[i].mif_handle,
				ports[dev->index].name);
			dev->dst_buf[i].phy_addr = 0;
			if (vicp_output_mode != 1 && dev->dev_choice == COMPOSER_WITH_VICP) {
				codec_mm_dma_free_coherent(dev->dst_buf[i].afbc_table_handle);
				dev->dst_buf[i].afbc_table_addr = 0;
			}
		}
	}

	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->last_dev_choice = COMPOSER_WITH_UNINITIAL;

	INIT_KFIFO(dev->free_q);
	kfifo_reset(&dev->free_q);
}

static void v2d_fence_cb_q_init(struct v2d_dev *dev)
{
	int i;

	INIT_KFIFO(dev->fence_cb_q);
	kfifo_reset(&dev->fence_cb_q);

	memset(dev->v2d_fence_cb, 0, sizeof(dev->v2d_fence_cb));
	for (i = 0; i < V2D_POOL_SIZE; i++) {
		if (!kfifo_put(&dev->fence_cb_q, &dev->v2d_fence_cb[i]))
			v2d_print(dev->index, PRINT_ERROR, "%s failed", __func__);
	}
}

static void v2d_config_init(struct v2d_dev *dev)
{
	if (!dev)
		return;

	INIT_KFIFO(dev->free_q);
	INIT_KFIFO(dev->file_wait_q);
	INIT_KFIFO(dev->fence_wait_q);

	kfifo_reset(&dev->free_q);
	kfifo_reset(&dev->file_wait_q);
	kfifo_reset(&dev->fence_wait_q);

	file_q_init(dev);
	display_q_init(dev);
	receive_q_init(dev);
	v2d_fence_cb_q_init(dev);

	dev->received_new_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_signal_count = 0;
	dev->buffer_release_count = 0;
	dev->set_frame_count = 0;
	dev->set_fence_count = 0;
	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->last_dev_choice = COMPOSER_WITH_UNINITIAL;
	init_completion(&dev->file_task_done);
	dev_get_vinfo(dev);
}

static int v2d_config_uninit(struct v2d_dev *dev)
{
	int time_left = 0;

	dev->need_do_file = true;
	wake_up_interruptible(&dev->file_wq);
	time_left = wait_for_completion_timeout(&dev->file_task_done,
						msecs_to_jiffies(500));
	if (!time_left)
		v2d_print(dev->index, PRINT_ERROR, "unreg:wait file timeout\n");
	else if (time_left < 100)
		v2d_print(dev->index, PRINT_ERROR,
			 "unreg:do file wait time left:%d\n", time_left);
	if (dev->fence_creat_count != dev->fence_signal_count) {
		v2d_print(dev->index, PRINT_ERROR,
			 "uninit: fence_r=%u, fence_c=%u\n",
			 dev->fence_signal_count,
			 dev->fence_creat_count);
		v2d_timeline_increase(dev, dev->fence_creat_count - dev->fence_signal_count);
	}
	display_q_uninit(dev);
	v2d_uninit_buffer(dev);
	receive_q_uninit(dev);
	file_q_uninit(dev);

	return 0;
}

static int v2d_set_enable(struct v2d_dev *dev, u32 val)
{
	if (val > V2D_SET_ENABLE_MODE)
		return -EINVAL;

	v2d_print(dev->index, PRINT_ERROR, "set enable index=%d, val=%d\n", dev->index, val);

	if (dev->status_enabled == val) {
		v2d_print(dev->index, PRINT_ERROR,
			"set_enable repeat, dev index =%d\n", dev->index);
		return 0;
	}
	dev->status_enabled = val;

	if (val == V2D_SET_ENABLE_MODE)
		v2d_config_init(dev);
	else if (val == V2D_SET_DISABLE_MODE)
		v2d_config_uninit(dev);

	return 0;
}

static int v2d_set_frames(struct v2d_dev *dev,
				struct frames_info_t *frames_info)
{
	struct file *file_vf = NULL;
	struct file *fence_file = NULL;
	struct file *output_fence_file = NULL;
	struct vframe_s *src_vf = NULL;
	struct dma_buf *dmabuf;
	struct frame_info_t *vframe_info_cur = NULL;
	int out_fd;
	int out_fence_fd;
	int i = 0, j = 0;
	bool is_dec_vf = false, is_v4l_vf = false;
	bool is_tvp = 0;

	if (!dev || !frames_info) {
		pr_err("%s: param is invalid.\n", __func__);
		return -EINVAL;
	}

	if (!dev->status_enabled) {
		v2d_print(dev->index, PRINT_ERROR,
			"%s: set_frame but not enabled\n", __func__);
		return -EINVAL;
	}

	if (frames_info->frame_count > MAX_LAYER_COUNT ||
		frames_info->frame_count < 1) {
		v2d_print(dev->index, PRINT_ERROR,
			"frame_count:%d, return\n", frames_info->frame_count);
		return -EINVAL;
	}

	i = v2d_get_received_frame_free_index(dev);
	if (!kfifo_get(&dev->file_free_q, &dmabuf)) {
		v2d_print(dev->index, PRINT_ERROR, "peek free dma_buf fail!!!\n");
		return -EINVAL;
	}

	dev->set_frame_count++;

	v2d_get_file_private_data(dmabuf->file, true);

	if (frames_info->frame_count == 1 && transform != -1) {
		v2d_print(dev->index, PRINT_OTHER, "set_frame: change transform from %d to %d.\n",
			frames_info->input_buffer[0].transform,
			transform);
		frames_info->input_buffer[0].transform = transform;
	}

	for (j = 0; j < frames_info->frame_count; j++) {
		vframe_info_cur = &frames_info->input_buffer[j];
		file_vf = fget(vframe_info_cur->fd);
		dev->fget_count++;
		total_get_count++;
		if (!file_vf) {
			v2d_print(dev->index, PRINT_ERROR,
				 "%s: fd is invalid!!!\n", __func__);
			return -EINVAL;
		}
		if (vframe_info_cur->fence_fd > 0) {
			fence_file = fget(vframe_info_cur->fence_fd);
			if (!fence_file) {
				v2d_print(dev->index, PRINT_ERROR,
					 "%s: fence_fd is invalid!!!\n", __func__);
				return -EINVAL;
			}
			dev->received_frames[i].input_fence[j] = fence_file;
		} else {
			dev->received_frames[i].input_fence[j] = NULL;
		}
		dev->received_frames[i].input_file[j] = file_vf;

		v2d_print(dev->index, PRINT_OTHER,
			"%s:num:%d trans:%d, in_fd:%d(0x%px), in_fence:%d(0x%px), fget=%d, total_get:%d.\n",
			__func__,
			j,
			vframe_info_cur->transform,
			vframe_info_cur->fd,
			file_vf,
			vframe_info_cur->fence_fd,
			dev->received_frames[i].input_fence[j],
			dev->fget_count,
			total_get_count);
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		if (is_dec_vf || is_v4l_vf) {
			src_vf = v2d_get_vf_from_file(dev, file_vf);
			if (!src_vf) {
				v2d_print(dev->index, PRINT_ERROR, "get vf NULL\n");
				continue;
			}
			if (!is_tvp) {
				if (src_vf->flag & VFRAME_FLAG_VIDEO_SECURE)
					is_tvp = true;
			}
			v2d_print(dev->index, PRINT_OTHER,
				"%s:frame_index:%d, vframe_type = 0x%x, vframe_flag = 0x%x.\n",
				__func__,
				src_vf->frame_index,
				src_vf->type,
				src_vf->flag);
		} else {
			dev->received_frames[i].phy_addr[j] =
				get_dma_phy_addr(frames_info->input_buffer[j].fd, dev->index);
			v2d_print(dev->index, PRINT_OTHER, "%s dma buffer not vf\n", __func__);
		}

		if (j == 0) {
			dev->received_frames[i].frames_info.frame_count = frames_info->frame_count;
			dev->received_frames[i].frames_info.input_buffer[0].transform =
					frames_info->input_buffer[0].transform;
			choose_v2d_device(dev, &dev->received_frames[i], src_vf);
			init_output_buffer_size(dev, &dev->received_frames[i]);
		}

		v2d_print(dev->index, PRINT_AXIS,
			"input_axis x,y,w,h: %d %d %d %d.\n",
			vframe_info_cur->dst_x,
			vframe_info_cur->dst_y,
			vframe_info_cur->dst_w,
			vframe_info_cur->dst_h);
		v2d_print(dev->index, PRINT_AXIS,
			"input crop t,l,w,h: %d %d %d %d.\n",
			vframe_info_cur->crop_y,
			vframe_info_cur->crop_x,
			vframe_info_cur->crop_w,
			vframe_info_cur->crop_h);
		v2d_print(dev->index, PRINT_AXIS,
			"input buffer w*h=%d*%d, stride w*h=%d*%d.\n",
			vframe_info_cur->buffer_w,
			vframe_info_cur->buffer_h,
			vframe_info_cur->align_w,
			vframe_info_cur->align_h);

		get_output_axis_crop(dev, src_vf, &dev->received_frames[i], vframe_info_cur, j);
	}

	out_fence_fd = v2d_timeline_create_fence(dev);
	output_fence_file = fget(out_fence_fd);
	out_fd = get_unused_fd_flags(O_CLOEXEC);
	if (out_fd < 0) {
		v2d_print(dev->index, PRINT_ERROR, "not fd!!!\n");
		return -ENOMEM;
	}

	/*out_fd is released by HWC*/
	fd_install(out_fd, dmabuf->file);
	dma_buf_get(out_fd);

	if (!kfifo_put(&dev->file_wait_q, dmabuf))
		v2d_print(dev->index, PRINT_ERROR, "put file_wait fail\n");
	v2d_print(dev->index, PRINT_OTHER,
		"set_frames: file_wait_q count: %d\n",
		kfifo_len(&dev->file_wait_q));

	frames_info->out_buffer.fd = out_fd;
	frames_info->out_buffer.fence_fd = out_fence_fd;
	frames_info->out_buffer.zorder = 0;
	frames_info->out_buffer.buffer_w = dev->buf_width;
	frames_info->out_buffer.buffer_h = dev->buf_height;
	frames_info->out_buffer.align_w = ALIGN(dev->buf_width, 32);
	frames_info->out_buffer.align_h = ALIGN(dev->buf_height, 32);
	set_output_buffer_area(dev, frames_info, &dev->received_frames[i], src_vf);

	v2d_print(dev->index, PRINT_PATTERN,
		"%s done, in_file:%px\n",
		__func__, dev->received_frames[i].input_file[0]);

	fput(output_fence_file);
	v2d_print(dev->index, PRINT_AXIS,
		"output_axis x,y,w,h: %d %d %d %d\n",
		frames_info->out_buffer.dst_x,
		frames_info->out_buffer.dst_y,
		frames_info->out_buffer.dst_w,
		frames_info->out_buffer.dst_h);
	v2d_print(dev->index, PRINT_AXIS,
		"output_crop t,l,w,h: %d %d %d %d.\n",
		frames_info->out_buffer.crop_x,
		frames_info->out_buffer.crop_y,
		frames_info->out_buffer.crop_w,
		frames_info->out_buffer.crop_h);
	v2d_print(dev->index, PRINT_AXIS,
		"output buffer w*h=%d*%d, stride w*h=%d*%d.\n",
		frames_info->out_buffer.buffer_w,
		frames_info->out_buffer.buffer_h,
		frames_info->out_buffer.align_w,
		frames_info->out_buffer.align_h);
	dev->received_frames[i].frames_info = *frames_info;
	dev->received_frames[i].is_tvp = is_tvp;
	atomic_set(&dev->received_frames[i].on_use, true);
	dev->received_count++;
	dev->received_new_count++;

	if (!kfifo_put(&dev->receive_q, &dev->received_frames[i]))
		v2d_print(dev->index, PRINT_ERROR, "put ready fail\n");
	wake_up_interruptible(&dev->file_wq);
	return 0;
}

static int v2d_set_fence(struct v2d_dev *dev, struct release_info_t *release_info)
{
	struct file *buffer_file;
	struct file *fence_file;
	int fence_fd, fd;
	struct sync_file *sync_file = NULL;
	struct dma_fence *fence_obj = NULL;
	struct v2d_fence_cb_t *cur_fence_cb;
	int ret = 0;

	if (!dev) {
		pr_err("%s: param is invalid.\n", __func__);
		return -EINVAL;
	}

	fd = release_info->release_fd;
	fence_fd = release_info->release_fence_fd;

	v2d_print(dev->index, PRINT_FENCE, "%s: fd:%d fence:fd:%d\n",
		__func__, fd, fence_fd);
	buffer_file = fget(fd);
	if (IS_ERR_OR_NULL(buffer_file)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: buffer_file is NULL\n", __func__);
		return -EBADF;
	}

	fence_file = fget(fence_fd);
	if (IS_ERR_OR_NULL(fence_file)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: fence_file is NULL\n", __func__);
		return -EBADF;
	}
	sync_file = (struct sync_file *)fence_file->private_data;
	if (IS_ERR_OR_NULL(sync_file)) {
		v2d_print(dev->index, PRINT_ERROR, "sync_file is NULL\n");
		fput(buffer_file);
		fput(fence_file);
		return -ENOBUFS;
	}

	fence_obj = sync_file->fence;
	if (IS_ERR_OR_NULL(fence_obj)) {
		v2d_print(dev->index, PRINT_ERROR, "fence_obj is NULL\n");
		fput(buffer_file);
		fput(fence_file);
		return -ENOBUFS;
	}

	v2d_print(dev->index, PRINT_FENCE, "sync_file=%px, dma_fence=%px, seqno=%lld\n",
		sync_file, fence_obj, fence_obj->seqno);

	if (!kfifo_get(&dev->fence_cb_q, &cur_fence_cb)) {
		v2d_print(dev->index, PRINT_ERROR,
			"%s: fence_cb_q is empty\n", __func__);
		fput(fence_file);
		return -ENOBUFS;
	}
	cur_fence_cb->dev = dev;
	cur_fence_cb->buffer_file = buffer_file;
	cur_fence_cb->fence_file = fence_file;
	kref_get(&dev->refcount);
	ret = dma_fence_add_callback(fence_obj, &cur_fence_cb->base_cb, v2d_fence_signal_cb);
	if (ret == -ENOENT) {
		v2d_print(dev->index, PRINT_ERROR, "fence has already signaled\n");
		v2d_fence_signal_cb(fence_obj, &cur_fence_cb->base_cb);
		return 0;
	}
	if (ret)
		v2d_print(dev->index, PRINT_ERROR, "dma_fence_add_callback failed: %d\n", ret);
	dev->set_fence_count++;
	if (dev->set_fence_count != dev->set_frame_count)
		v2d_print(dev->index, PRINT_FENCE,
			"%s: warning, fence may leak, set_frame:%d, set_fence:%d!\n",
			__func__, dev->set_frame_count, dev->set_fence_count);
	return ret;
}

ssize_t v2d_dump_dewarp_src_show(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off,
		size_t count)
{
	int ret = 0;
	struct v2d_port_s *port = (struct v2d_port_s *)attr->private;
	struct v2d_dev *v2d_dev = port->dev;
	struct v2d_dump_info_s *info = &v2d_dev->info_dewarp_src;
	u32 sizeimage, y_size = 0, uv_size = 0;
	size_t y_remain = 0;
	size_t y_copy = 0;
	size_t uv_remain = 0;
	size_t uv_copy = 0;
	size_t to_copy;
	loff_t cur_off = off;
	size_t copied = 0;
	loff_t uv_off = 0;

	if (!info->vir_addr[0] && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			pr_info("dump yuv: interrupted while waiting for mutex\n");
			return ret;
		}
		info->is_dump = 1;
		ret = wait_event_interruptible_timeout(info->dump_wq,
							info->config_dump_done,
							msecs_to_jiffies(500000));
		if (ret <= 0) {
			pr_info("dump yuv: wait interrupted by signal or timeout, ret=%d\n", ret);
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return ret;
		}

		pr_info("yuv src phy_addr0:%llx phy_addr1:%llx width:%d height:%d sizeimage:%d plane:%d\n",
			info->phy_addr[0], info->phy_addr[1], info->width, info->height,
			info->sizeimage, info->plane_num);

		y_size = info->width * info->height;
		info->vir_addr[0] = codec_mm_vmap(info->phy_addr[0], y_size);
		if (!info->vir_addr[0]) {
			pr_info("vmap failed. vir_addr0 is null\n");
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return -EINVAL;
		}
		if (info->plane_num == 2) {
			uv_size = info->sizeimage - y_size;
			info->vir_addr[1] = codec_mm_vmap(info->phy_addr[1], uv_size);
			if (!info->vir_addr[1]) {
				pr_info("vmap failed. vir_addr1 is null\n");
				codec_mm_unmap_phyaddr(info->vir_addr[0]);
				info->is_dump = 0;
				mutex_unlock(&info->mutex);
				return -EINVAL;
			}
		}
	}

	sizeimage = info->sizeimage;
	y_size = info->width * info->height;
	uv_size = sizeimage - y_size;

	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	to_copy = min_t(size_t, count, sizeimage - off);

	while (to_copy > 0) {
		if (cur_off < y_size) {
			y_remain = y_size - cur_off;
			y_copy = min(to_copy, y_remain);
			memcpy(buf + copied, info->vir_addr[0] + cur_off, y_copy);
			copied += y_copy;
			cur_off += y_copy;
			to_copy -= y_copy;
		} else {
			uv_off = cur_off - y_size;
			uv_remain = uv_size - uv_off;
			uv_copy = min(to_copy, uv_remain);
			memcpy(buf + copied, info->vir_addr[1] + uv_off, uv_copy);
			copied += uv_copy;
			cur_off += uv_copy;
			to_copy -= uv_copy;
		}
	}

	if ((off + copied) >= sizeimage) {
		codec_mm_unmap_phyaddr(info->vir_addr[0]);
		info->vir_addr[0] = NULL;
		if (info->plane_num == 2) {
			codec_mm_unmap_phyaddr(info->vir_addr[1]);
			info->vir_addr[1] = NULL;
		}
		info->is_dump = 0;
		info->config_dump_done = 0;
		mutex_unlock(&info->mutex);
	}

	return copied;
}

static void config_dewarp_src_data(struct v2d_dev *dev)
{
	struct dewarp_composer_para *dewarp_param = NULL;
	struct v2d_dump_info_s *info = NULL;

	if (!dev)
		return;
	dewarp_param = &dev->dewarp_para;
	info = &dev->info_dewarp_src;

	if (!dewarp_param->vf_para) {
		pr_info("%s:dewarp_param->vf_para is null\n", __func__);
		return;
	}

	info->phy_addr[0] = dewarp_param->vf_para->src_buf_addr0;
	info->phy_addr[1] = dewarp_param->vf_para->src_buf_addr1;
	info->width = dewarp_param->vf_para->src_vf_width;
	info->height = dewarp_param->vf_para->src_vf_height;
	info->plane_num = 2;
	info->sizeimage = info->width * info->height * 3 / 2;

	v2d_print(dev->index, PRINT_OTHER, "%s:config done\n", __func__);
}

static void config_dump_output_yuv_data(struct vframe_s *vf, struct v2d_dev *dev)
{
	struct canvas_s cs0;
	struct canvas_s cs1;
	struct canvas_s cs2;

	if (!vf || !dev)
		return;
	if (vf->canvas0Addr == (u32)-1) {
		dev->info_output_yuv.width = vf->canvas0_config[0].width;
		dev->info_output_yuv.height = vf->canvas0_config[0].height;
		dev->info_output_yuv.phy_addr[0] = vf->canvas0_config[0].phy_addr;
		if (vf->plane_num == 1) {
			dev->info_output_yuv.sizeimage = vf->canvas0_config[0].width *
				vf->canvas0_config[0].height;
		} else if (vf->plane_num == 2) {
			dev->info_output_yuv.sizeimage = vf->canvas0_config[0].width *
				vf->canvas0_config[0].height * 3 / 2;
			dev->info_output_yuv.phy_addr[1] = vf->canvas0_config[1].phy_addr;
		} else if (vf->plane_num == 3) {
			dev->info_output_yuv.sizeimage = vf->canvas0_config[0].width *
				vf->canvas0_config[0].height * 3 / 2;
			dev->info_output_yuv.phy_addr[1] = vf->canvas0_config[1].phy_addr;
			dev->info_output_yuv.phy_addr[2] = vf->canvas0_config[2].phy_addr;
		}
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);
		canvas_read((vf->canvas0Addr >> 16) & 0xff, &cs2);

		dev->info_output_yuv.width = cs0.width;
		dev->info_output_yuv.height = cs0.height;
		dev->info_output_yuv.phy_addr[0] = cs0.addr;
		if (vf->plane_num == 1) {
			dev->info_output_yuv.sizeimage = cs0.width * cs0.height;
		} else if (vf->plane_num == 2) {
			dev->info_output_yuv.sizeimage = cs0.width * cs0.height * 3 / 2;
			dev->info_output_yuv.phy_addr[1] = cs1.addr;
		} else if (vf->plane_num == 3) {
			dev->info_output_yuv.sizeimage = cs0.width * cs0.height * 3 / 2;
			dev->info_output_yuv.phy_addr[1] = cs1.addr;
			dev->info_output_yuv.phy_addr[2] = cs2.addr;
		}
	}
	dev->info_output_yuv.plane_num = vf->plane_num;
	v2d_print(dev->index, PRINT_OTHER, "%s:config done\n", __func__);
}

ssize_t v2d_dump_yuv_output_show(struct file *filp, struct kobject *kobj,
		struct bin_attribute *attr, char *buf, loff_t off,
		size_t count)
{
	int ret = 0;
	struct v2d_port_s *port = (struct v2d_port_s *)attr->private;
	struct v2d_dev *v2d_dev = port->dev;
	struct v2d_dump_info_s *info = &v2d_dev->info_output_yuv;
	u32 sizeimage, y_size = 0, uv_size = 0;
	size_t y_remain = 0;
	size_t y_copy = 0;
	size_t uv_remain = 0;
	size_t uv_copy = 0;
	size_t to_copy;
	loff_t cur_off = off;
	size_t copied = 0;
	loff_t uv_off = 0;

	if (!info->vir_addr[0] && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			pr_info("dump yuv: interrupted while waiting for mutex\n");
			return ret;
		}
		info->is_dump = 1;
		ret = wait_event_interruptible_timeout(info->dump_wq,
							info->config_dump_done,
							msecs_to_jiffies(500000));
		if (ret <= 0) {
			pr_info("dump yuv: wait interrupted by signal or timeout, ret=%d\n", ret);
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return ret;
		}

		pr_info("yuv dst phy_addr0:%llx phy_addr1:%llx width:%d height:%d sizeimage:%d plane:%d\n",
			info->phy_addr[0], info->phy_addr[1], info->width, info->height,
			info->sizeimage, info->plane_num);

		y_size = info->width * info->height;
		info->vir_addr[0] = codec_mm_vmap(info->phy_addr[0], y_size);
		if (!info->vir_addr[0]) {
			pr_info("vmap failed. vir_addr0 is null\n");
			info->is_dump = 0;
			mutex_unlock(&info->mutex);
			return -EINVAL;
		}
		if (info->plane_num == 2) {
			uv_size = info->sizeimage - y_size;
			info->vir_addr[1] = codec_mm_vmap(info->phy_addr[1], uv_size);
			if (!info->vir_addr[1]) {
				pr_info("vmap failed. vir_addr1 is null\n");
				codec_mm_unmap_phyaddr(info->vir_addr[0]);
				info->is_dump = 0;
				mutex_unlock(&info->mutex);
				return -EINVAL;
			}
		}
	}

	sizeimage = info->sizeimage;
	y_size = info->width * info->height;
	uv_size = sizeimage - y_size;

	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	to_copy = min_t(size_t, count, sizeimage - off);

	while (to_copy > 0) {
		if (cur_off < y_size) {
			y_remain = y_size - cur_off;
			y_copy = min(to_copy, y_remain);
			memcpy(buf + copied, info->vir_addr[0] + cur_off, y_copy);
			copied += y_copy;
			cur_off += y_copy;
			to_copy -= y_copy;
		} else {
			uv_off = cur_off - y_size;
			uv_remain = uv_size - uv_off;
			uv_copy = min(to_copy, uv_remain);
			memcpy(buf + copied, info->vir_addr[1] + uv_off, uv_copy);
			copied += uv_copy;
			cur_off += uv_copy;
			to_copy -= uv_copy;
		}
	}

	if ((off + copied) >= sizeimage) {
		codec_mm_unmap_phyaddr(info->vir_addr[0]);
		info->vir_addr[0] = NULL;
		if (info->plane_num == 2) {
			codec_mm_unmap_phyaddr(info->vir_addr[1]);
			info->vir_addr[1] = NULL;
		}
		info->is_dump = 0;
		info->config_dump_done = 0;
		mutex_unlock(&info->mutex);
	}

	return copied;
}

static void v2d_do_file_task(struct v2d_dev *dev)
{
	struct received_frames_t *received_frames = NULL;
	struct frame_info_t *vframe_info[MAX_LAYER_COUNT];
	struct frame_info_t *vframe_info_cur = NULL;
	struct dma_buf *output_dmabuf;
	struct dst_buf_t *output_buffer = NULL;
	struct vframe_s *output_vf = NULL;
	struct vframe_s *input_vf = NULL;
	struct file_private_data *private_data = NULL;
	struct file *file_vf = NULL;
	struct file *fence_file = NULL;
	struct output_axis output_axis;
	struct composer_info_t *composer_info;
	struct display_data_t *display_data;
	struct v2d_dev_config v2d_composer_param;
	s64 start_time, end_time, cost_time;
	int vf_dev[MAX_LAYER_COUNT];
	unsigned long input_addr = 0;
	bool is_tvp = false;
	bool is_dec_vf = false, is_v4l_vf = false;
	int i, j, tmp;
	int count;
	int ret = 0;
	int data_index;
	u32 zd1, zd2;

	if (!dev) {
		pr_info("%s: invalid param.\n", __func__);
		return;
	}
	v2d_print(dev->index, PRINT_OTHER,
		 "%s file_wait_q count: %d\n",
		 __func__, kfifo_len(&dev->file_wait_q));

	if (!kfifo_get(&dev->receive_q, &received_frames)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: receive_q is empty.\n", __func__);
		return;
	}

	if (!kfifo_get(&dev->file_wait_q, &output_dmabuf)) {
		v2d_print(dev->index, PRINT_ERROR, "%s: no output buf, return.\n", __func__);
		return;
	}
	private_data = v2d_get_file_private_data(output_dmabuf->file, true);
	if (!private_data) {
		v2d_print(dev->index, PRINT_ERROR, "%s:private_data is null\n", __func__);
		return;
	}

	if (v2d_bypass)
		v2d_print(dev->index, PRINT_OTHER, "%s:bypass consider later\n", __func__);
	start_time = ktime_to_ns(ktime_get());
	output_vf = &private_data->vf;
	memset(output_vf, 0, sizeof(struct vframe_s));

	if (!output_vf->composer_info) {
		output_vf->composer_info = kmalloc(sizeof(*output_vf->composer_info), GFP_KERNEL);
		v2d_print(dev->index, PRINT_OTHER, "%s:alloc composer_info for new vf\n", __func__);
	}
	composer_info = output_vf->composer_info;
	memset(composer_info, 0, sizeof(struct composer_info_t));

	is_tvp = received_frames->is_tvp;
	count = received_frames->frames_info.frame_count;
	memcpy(&output_axis, &received_frames->output_axis, sizeof(struct output_axis));

	ret = v2d_init_buffer(dev, is_tvp);
	if (ret != 0) {
		v2d_print(dev->index, PRINT_ERROR, "v2d: init buffer failed!\n");
		v2d_uninit_buffer(dev);
		return;
	}
	while (!kfifo_get(&dev->free_q, &output_buffer)) {
		v2d_print(dev->index, PRINT_QUEUE_STATUS, "free q is empty!\n");
		usleep_range(2000, 4000);
	}

	if (is_window_changed(dev, &received_frames->frames_info)) {
		for (i = 0; i < BUFFER_LEN; i++)
			dev->dst_buf[i].dirty = true;
	}

	if (is_tvp != output_buffer->is_tvp || output_buffer->buf_status == BUFFER_STATUS_CHANGE) {
		ret = v2d_switch_buffer(output_buffer, is_tvp, dev);
		output_buffer->buf_status = BUFFER_STATUS_NORMAL;
		if (ret == 0) {
			v2d_print(dev->index, PRINT_ERROR,
				 "switch buffer from %s to %s failed\n",
				 output_buffer->is_tvp ? "tvp" : "non tvp",
				 is_tvp ? "tvp" : "non tvp");
			return;
		}
	}

	if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		if (display_yuv444) {
			dev->ge2d_para.format = GE2D_FORMAT_S24_YUV444;
			dev->ge2d_para.plane_num = 1;
		} else {
			dev->ge2d_para.format = GE2D_FORMAT_M24_NV21;
			dev->ge2d_para.plane_num = 2;
		}
		dev->ge2d_para.is_tvp = is_tvp;
		dev->ge2d_para.phy_addr[0] = output_buffer->phy_addr;
		dev->ge2d_para.buffer_w = output_buffer->buf_w;
		dev->ge2d_para.buffer_h = output_buffer->buf_h;
		dev->ge2d_para.canvas0_addr = -1;

		if (output_buffer->dirty) {
			ret = v2d_fill_vframe_black(&dev->ge2d_para);
			if (ret < 0)
				v2d_print(dev->index, PRINT_ERROR, "ge2d fill black failed\n");
			else
				v2d_print(dev->index, PRINT_OTHER, "fill black\n");
			output_buffer->dirty = false;
		}
	}

	for (i = 0; i < count; i++) {
		vf_dev[i] = i;
		vframe_info[i] = &received_frames->frames_info.input_buffer[i];
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
	for (i = 0; i < count; i++) {
		input_vf = NULL;
		file_vf = received_frames->input_file[vf_dev[i]];
		fence_file = received_frames->input_fence[vf_dev[i]];
		v2d_print(dev->index, PRINT_OTHER, "%s: file_vf:%px fence_file:%px.\n",
			__func__, file_vf, fence_file);
		if (v2d_wait_file_fence(dev, fence_file) == 0)
			continue;
		memcpy(&v2d_composer_param.frame_crop, &received_frames->crop_info[vf_dev[i]],
			sizeof(struct input_crop_s));
		memcpy(&v2d_composer_param.frame_axis, &received_frames->axis_info[vf_dev[i]],
			sizeof(struct input_axis_s));
		vframe_info_cur = vframe_info[vf_dev[i]];
		if (!vframe_info_cur) {
			v2d_print(dev->index, PRINT_ERROR, "vframe_info_cur NULL\n");
			return;
		}
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		if (is_dec_vf || is_v4l_vf) {
			v2d_print(dev->index, PRINT_OTHER, "%s dmabuf is vf\n", __func__);
			input_vf = v2d_get_vf_from_file(dev, file_vf);
			if (!input_vf) {
				v2d_print(dev->index, PRINT_ERROR, "get vf NULL\n");
				continue;
			}
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
			set_v2d_mediaproxy_info(dev, input_vf, composer_info, i);
#endif
			v2d_print(dev->index, PRINT_OTHER, "frame_index:%d\n",
				input_vf->frame_index);
		} else {
			input_addr = received_frames->phy_addr[vf_dev[i]];
			v2d_print(dev->index, PRINT_OTHER,
				"%s dmabuf not vf, i=%d fd=%d phy_addr=0x%lx\n",
				__func__, vf_dev[i], vframe_info_cur->fd, input_addr);
		}
		v2d_print(dev->index, PRINT_AXIS, "===============================\n");

		v2d_composer_param.input_vf = input_vf;
		v2d_composer_param.dst_buf = output_buffer;
		v2d_composer_param.addr = input_addr;
		v2d_composer_param.is_tvp = is_tvp;
		v2d_composer_param.count = count;
		v2d_composer_param.index = i;
		v2d_composer_param.zorder = vf_dev[i];
		dev->config_param_func(dev, vframe_info_cur, &v2d_composer_param);

		dev->process_data_func(dev, &v2d_composer_param);
	}

	if (dev->dev_choice == COMPOSER_WITH_DEWARP && dev->info_dewarp_src.is_dump) {
		v2d_print(dev->index, PRINT_OTHER, "%s:src start dump\n", __func__);
		config_dewarp_src_data(dev);
		dev->info_dewarp_src.config_dump_done = 1;
		wake_up_interruptible(&dev->info_dewarp_src.dump_wq);
		mutex_lock(&dev->info_dewarp_src.mutex);
		mutex_unlock(&dev->info_dewarp_src.mutex);
		v2d_print(dev->index, PRINT_OTHER, "%s:src dump end\n", __func__);
	}

	dev->last_frames = *received_frames;
	frames_put_file(dev, received_frames);
	end_time = ktime_to_ns(ktime_get());
	cost_time = end_time - start_time;
	v2d_print(dev->index, PRINT_PERFORMANCE,
		"vframe composer cost: %lld us\n",
		div64_u64(cost_time, 1000));

	composer_info->count = count;
	for (i = 0; i < count; i++) {
		composer_info->axis[i][0] = vframe_info[vf_dev[i]]->dst_x
			- output_axis.min_left;
		composer_info->axis[i][1] = vframe_info[vf_dev[i]]->dst_y
			- output_axis.min_top;
		composer_info->axis[i][2] = vframe_info[vf_dev[i]]->dst_w
			+ composer_info->axis[i][0] - 1;
		composer_info->axis[i][3] = vframe_info[vf_dev[i]]->dst_h
			+ composer_info->axis[i][1] - 1;
		v2d_print(dev->index, PRINT_AXIS,
			 "alpha index=%d %d %d %d %d\n",
			 i,
			 composer_info->axis[i][0],
			 composer_info->axis[i][1],
			 composer_info->axis[i][2],
			 composer_info->axis[i][3]);
	}

	config_output_vf_param(dev, output_vf, is_tvp, output_buffer, vframe_info_cur);

	if (count == 1 && input_vf) {
		v2d_print(dev->index, PRINT_OTHER, "composer done, copy hdr info.\n");
		output_vf->duration = input_vf->duration;
		output_vf->ready_jiffies64 = input_vf->ready_jiffies64;
		output_vf->duration = input_vf->duration;
		output_vf->src_fmt = input_vf->src_fmt;
		output_vf->signal_type = input_vf->signal_type;
		output_vf->source_type = input_vf->source_type;

		if (input_vf->flag & VFRAME_FLAG_PC_MODE)
			output_vf->flag |= VFRAME_FLAG_PC_MODE;
		if (input_vf->flag & VFRAME_FLAG_GAME_MODE)
			output_vf->flag |= VFRAME_FLAG_GAME_MODE;
	}

	v2d_print(dev->index, PRINT_OTHER, "v2d_task: signal_type = 0x%x.\n",
		output_vf->signal_type);
	if (enable_v2d_dump) {
		enable_v2d_dump = 0;
		v2d_dump_output_buffer(output_vf);
	}

	if (dev->info_output_yuv.is_dump) {
		v2d_print(dev->index, PRINT_ERROR, "v2d_dump:vf->type=%x, vf->flag=%x.\n",
			output_vf->type, output_vf->flag);
		v2d_print(dev->index, PRINT_OTHER, "%s:dst start dump\n", __func__);
		config_dump_output_yuv_data(output_vf, dev);
		dev->info_output_yuv.config_dump_done = 1;
		wake_up_interruptible(&dev->info_output_yuv.dump_wq);
		mutex_lock(&dev->info_output_yuv.mutex);
		mutex_unlock(&dev->info_output_yuv.mutex);
		v2d_print(dev->index, PRINT_OTHER, "%s:dst dump end\n", __func__);
	}
	data_index = v2d_get_display_data_free_index(dev);
	private_data->is_keep = true;
	atomic_set(&dev->display_data[data_index].on_use, true);

	display_data = &dev->display_data[data_index];
	display_data->output_buffer = output_buffer;
	display_data->output_dmabuf = output_dmabuf;
	display_data->private_data = private_data;
	if (!kfifo_put(&dev->display_q, display_data))
		v2d_print(dev->index, PRINT_ERROR, "display_q is full\n");

	v2d_timeline_increase(dev, 1);
	v2d_print(dev->index, PRINT_PERFORMANCE,
		 "composer done, dmabuf=%px dst_addr:%lx\n",
		 output_dmabuf, output_buffer->phy_addr);
	v2d_print(dev->index, PRINT_PERFORMANCE,
		 "display_q len=%d\n", kfifo_len(&dev->display_q));

	atomic_set(&received_frames->on_use, false);
}

static int v2d_file_thread(void *data)
{
	struct v2d_dev *dev = data;

	v2d_print(dev->index, PRINT_OTHER, "%s: started\n", __func__);
	dev->file_thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;
		if (dev->thread_need_stop) {
			usleep_range(1000, 2000);
			continue;
		}

		if (kfifo_len(&dev->receive_q) == 0)
			wait_event_interruptible_timeout(dev->file_wq,
							 (kfifo_len(&dev->receive_q) > 0 &&
							  dev->status_enabled) ||
							  dev->need_do_file ||
							 dev->thread_need_stop,
							 msecs_to_jiffies(5000));

		if (kfifo_len(&dev->receive_q) > 0 && dev->status_enabled)
			v2d_do_file_task(dev);
		if (dev->need_do_file) {
			v2d_print(dev->index, PRINT_OTHER,
				 "ready to complete file task\n");
			dev->need_do_file = false;
			complete(&dev->file_task_done);
		}
	}
	dev->file_thread_stopped = 1;
	v2d_print(dev->index, PRINT_OTHER, "%s: exit\n", __func__);
	return 0;
}

static int v2d_open(struct inode *inode, struct file *file)
{
	struct v2d_dev *dev;
	struct v2d_port_s *port = NULL;
	struct sched_param param = {.sched_priority = 2};

	pr_info("%s iminor(inode) =%d\n", __func__, iminor(inode));
	if (iminor(inode) >= v2d_instance_num)
		return -ENODEV;

	// coverity[illegal_address] I'm ensure it is ok.
	port = &ports[iminor(inode)];
	mutex_lock(&v2d_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&v2d_mutex);
		pr_err("v2d: instance %d is already opened",
		       port->index);
		return -EBUSY;
	}
	dev = vmalloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	kref_init(&dev->refcount);
	port->dev = dev;
	dev->ge2d_para.count = 0;
	dev->ge2d_para.canvas_dst[0] = -1;
	dev->ge2d_para.canvas_dst[1] = -1;
	dev->ge2d_para.canvas_dst[2] = -1;
	dev->ge2d_para.canvas_scr[0] = -1;
	dev->ge2d_para.canvas_scr[1] = -1;
	dev->ge2d_para.canvas_scr[2] = -1;
	dev->ge2d_para.plane_num = 2;

	dev->dewarp_para.index = dev->index;
	dev->dewarp_para.context = NULL;
	dev->dewarp_para.fw_load.size_32bit = 0;
	dev->dewarp_para.fw_load.phys_addr = 0;
	dev->dewarp_para.fw_load.virt_addr = NULL;
	dev->dewarp_para.vf_para = NULL;

	dev->buffer_status = UNINITIAL;

	mutex_init(&dev->info_output_yuv.mutex);
	mutex_init(&dev->info_dewarp_src.mutex);

	dev->thread_need_stop = false;
	dev->port = port;
	dev->index = port->index;
	file->private_data = dev;
	port->open_count++;
	mutex_unlock(&v2d_mutex);

	dev->file_thread = kthread_create(v2d_file_thread,
				      dev, dev->port->name);
	if (IS_ERR(dev->file_thread)) {
		pr_err("v2d_composer_thread creat failed\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->file_wq);
	init_waitqueue_head(&dev->info_output_yuv.dump_wq);
	init_waitqueue_head(&dev->info_dewarp_src.dump_wq);

	if (sched_setscheduler_nocheck(dev->file_thread, SCHED_FIFO, &param))
		pr_err("v2d_composer_thread :set realtime priority failed.\n");
	wake_up_process(dev->file_thread);
	v2d_timeline_create(dev);

	return 0;
}

static int v2d_release(struct inode *inode, struct file *file)
{
	struct v2d_dev *dev = file->private_data;
	struct v2d_port_s *port = dev->port;
	int ret = 0;

	pr_info("release: enable=%d ref_count:%u\n",
		dev->status_enabled, kref_read(&dev->refcount));

	if (iminor(inode) >= v2d_instance_num)
		return -ENODEV;
	if (dev->status_enabled) {
		ret = v2d_set_enable(dev, 0);
		if (ret != 0)
			pr_err("%s: disable v2d failed\n", __func__);
	}
	dev->thread_need_stop = true;
	if (dev->file_thread) {
		kthread_stop(dev->file_thread);
		wake_up_interruptible(&dev->file_wq);
	}

	dev->file_thread = NULL;
	dev->thread_need_stop = false;

	mutex_lock(&v2d_mutex);
	port->open_count--;
	mutex_unlock(&v2d_mutex);
	kref_put(&dev->refcount, v2d_device_data_free);

	return 0;
}

static long v2d_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	u32 val;
	struct v2d_dev *dev = (struct v2d_dev *)file->private_data;
	struct frames_info_t *frames_info;
	struct release_info_t release_info;

	switch (cmd) {
	case V2D_IOCTL_SET_ENABLE:
		if (copy_from_user(&val, argp, sizeof(u32)) == 0)
			ret = v2d_set_enable(dev, val);
		else
			ret = -EFAULT;
		break;
	case V2D_IOCTL_SET_FRAMES:
		if (!dev)
			return -ENOMEM;

		frames_info = &dev->frames_info;
		memset(frames_info, 0, sizeof(*frames_info));
		if (copy_from_user(frames_info, argp, sizeof(struct frames_info_t)) != 0)
			return -EFAULT;

		ret = v2d_set_frames(dev, frames_info);
		if (copy_to_user(argp, frames_info, sizeof(struct frames_info_t)) != 0)
			return -EFAULT;
		break;
	case V2D_IOCTL_SET_FENCE:
		if (copy_from_user(&release_info, argp, sizeof(release_info)) == 0)
			ret = v2d_set_fence(dev, &release_info);
		else
			ret = -EFAULT;
		break;
	default:
		pr_info("set cmd err!");
		ret = -EFAULT;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long v2d_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = v2d_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations v2d_fops = {
	.owner = THIS_MODULE,
	.open = v2d_open,
	.release = v2d_release,
	.unlocked_ioctl = v2d_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = v2d_compat_ioctl,
#endif
	.poll = NULL,
};

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

static ssize_t buffer_width_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current manual_buf_width is %d\n",
			manual_buf_width);
}

static ssize_t buffer_width_store(const struct class *cla,
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
	manual_buf_width = tmp;
	return count;
}

static ssize_t buffer_height_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current manual_buf_height is %d\n",
			manual_buf_height);
}

static ssize_t buffer_height_store(const struct class *cla,
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
	manual_buf_height = tmp;
	return count;
}

static ssize_t bypass_v2d_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current v2d_bypass is %d\n",
			v2d_bypass);
}

static ssize_t bypass_v2d_store(const struct class *cla,
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
	v2d_bypass = tmp;
	return count;
}

static ssize_t enable_v2d_dump_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current enable_v2d_dump is %d\n",
			enable_v2d_dump);
}

static ssize_t enable_v2d_dump_store(const struct class *cla,
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
	enable_v2d_dump = tmp;
	return count;
}

static ssize_t ge2d_com_debug_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current ge2d_com_debug is %d\n",
			ge2d_com_debug);
}

static ssize_t ge2d_com_debug_store(const struct class *cla,
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
	ge2d_com_debug = tmp;
	return count;
}

static ssize_t dewarp_com_dump_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_com_dump is %d\n",
			dewarp_com_dump);
}

static ssize_t dewarp_com_dump_store(const struct class *cla,
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
	dewarp_com_dump = tmp;
	return count;
}

static ssize_t dewarp_print_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_print is %d\n",
			dewarp_print);
}

static ssize_t dewarp_print_store(const struct class *cla,
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
	dewarp_print = tmp;
	return count;
}

static ssize_t v2d_dev_choice_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
		"1 ge2d, 2 dewarp, 3 vicp. current choice is %d.\n",
		v2d_dev_choice);
}

static ssize_t v2d_dev_choice_store(const struct class *cla,
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
	v2d_dev_choice = tmp;
	return count;
}

static ssize_t transform_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80, "current transform is %d.\n", transform);
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

static ssize_t vicp_output_mode_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80, "1 mif, 2 fbc, 3. mif+fbc, current vicp_output_mode is %d.\n",
		vicp_output_mode);
}

static ssize_t vicp_output_mode_store(const struct class *cla,
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
	vicp_output_mode = tmp;
	return count;
}

static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(buffer_height);
static CLASS_ATTR_RW(buffer_width);
static CLASS_ATTR_RW(bypass_v2d);
static CLASS_ATTR_RW(enable_v2d_dump);
static CLASS_ATTR_RW(ge2d_com_debug);
static CLASS_ATTR_RW(dewarp_com_dump);
static CLASS_ATTR_RW(dewarp_print);
static CLASS_ATTR_RW(v2d_dev_choice);
static CLASS_ATTR_RW(transform);
static CLASS_ATTR_RW(vicp_output_mode);


static struct attribute *v2d_class_attrs[] = {
	&class_attr_print_flag.attr,
	&class_attr_buffer_height.attr,
	&class_attr_buffer_width.attr,
	&class_attr_bypass_v2d.attr,
	&class_attr_enable_v2d_dump.attr,
	&class_attr_ge2d_com_debug.attr,
	&class_attr_dewarp_com_dump.attr,
	&class_attr_dewarp_print.attr,
	&class_attr_v2d_dev_choice.attr,
	&class_attr_transform.attr,
	&class_attr_vicp_output_mode.attr,
	NULL
};

ATTRIBUTE_GROUPS(v2d_class);

static struct class v2d_class = {
	.name = "v2d",
	.class_groups = v2d_class_groups,
};

static const struct of_device_id amlogic_v2d_dt_match[] = {
	{.compatible = "amlogic, v2d",
	},
	{},
};

static struct bin_attribute v2d0_attr[] = {
	{
		.attr.name = "dump_dewarp_src",
		.attr.mode = 0664,
		.private = &ports[0],
		.read = v2d_dump_dewarp_src_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_yuv_output",
		.attr.mode = 0664,
		.private = &ports[0],
		.read = v2d_dump_yuv_output_show,
		.write = NULL,
	},
};

static struct bin_attribute v2d1_attr[] = {
	{
		.attr.name = "dump_dewarp_src",
		.attr.mode = 0664,
		.private = &ports[1],
		.read = v2d_dump_dewarp_src_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_yuv_output",
		.attr.mode = 0664,
		.private = &ports[1],
		.read = v2d_dump_yuv_output_show,
		.write = NULL,
	},
};

static struct bin_attribute v2d2_attr[] = {
	{
		.attr.name = "dump_dewarp_src",
		.attr.mode = 0664,
		.private = &ports[2],
		.read = v2d_dump_dewarp_src_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_yuv_output",
		.attr.mode = 0664,
		.private = &ports[2],
		.read = v2d_dump_yuv_output_show,
		.write = NULL,
	},
};

static struct bin_attribute *v2d_bin_attrs0[] = {
	&v2d0_attr[0],
	&v2d0_attr[1],
	NULL,
};

static struct bin_attribute *v2d_bin_attrs1[] = {
	&v2d1_attr[0],
	&v2d1_attr[1],
	NULL,
};

static struct bin_attribute *v2d_bin_attrs2[] = {
	&v2d2_attr[0],
	&v2d2_attr[1],
	NULL,
};

static const struct attribute_group v2d_attr_group[] = {
	{
		.name = v2d_group_name0,
		.bin_attrs = v2d_bin_attrs0,
	},
	{
		.name = v2d_group_name1,
		.bin_attrs = v2d_bin_attrs1,
	},
	{
		.name = v2d_group_name2,
		.bin_attrs = v2d_bin_attrs2,
	},
};

static int v2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0, j;
	u32 layer_cap = 0;
	struct v2d_port_s *st;

	layer_cap = video_get_layer_capability();
	v2d_instance_num = 0;
	if (layer_cap & LAYER0_SCALER)
		v2d_instance_num++;
	if (layer_cap & LAYER1_SCALER)
		v2d_instance_num++;
	if (layer_cap & LAYER2_SCALER)
		v2d_instance_num++;

	ret = class_register(&v2d_class);
	if (ret < 0)
		return ret;

	ret = register_chrdev(V2D_MAJOR, "v2d", &v2d_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for v2d device\n");
		goto error_class;
	}

	for (st = &ports[0], i = 0; i < v2d_instance_num; i++, st++) {
		pr_debug("%s:ports[i].name=%s, i=%d\n", __func__,
		       ports[i].name, i);

		st->pdev = &pdev->dev;
		st->class_dev = device_create(&v2d_class, NULL,
					      MKDEV(V2D_MAJOR, i),
					      NULL, ports[i].name);
		if (IS_ERR(st->class_dev)) {
			ret = PTR_ERR(st->class_dev);
			pr_err("device_create failed, ret=%d, i=%d\n", ret, i);
			goto error_device;
		}

		ret = of_property_read_u32(pdev->dev.of_node,
					   "vpu_dma_mask", &st->vpu_dma_mask);
		if (ret) {
			pr_debug("video_composer don't find vpu_dma_mask\n");
			st->vpu_dma_mask = 0;
		}
		if (st->vpu_dma_mask) {
			ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_coherent_mask fail\n");
				goto error_device;
			}
			ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_mask fail\n");
				goto error_device;
			}
		}

		ret = sysfs_create_group(&pdev->dev.kobj, &v2d_attr_group[i]);
		if (ret) {
			pr_err("v2d create dump node fail, ret=%d, i=%d.\n", ret, i);
			goto error_sysfs;
		}
	}

	return 0;

error_sysfs:
	for (j = i - 1; j >= 0; j--) {
		sysfs_remove_group(&pdev->dev.kobj, &v2d_attr_group[j]);
		device_destroy(&v2d_class, MKDEV(V2D_MAJOR, j));
	}

error_device:
	unregister_chrdev(V2D_MAJOR, "v2d");

error_class:
	class_unregister(&v2d_class);
	return ret;
}

static void v2d_remove(struct platform_device *pdev)
{
	int i;
	struct v2d_port_s *st;

	for (st = &ports[0], i = 0; i < v2d_instance_num; i++, st++) {
		sysfs_remove_group(&pdev->dev.kobj, &v2d_attr_group[i]);
		device_destroy(&v2d_class, MKDEV(V2D_MAJOR, i));
	}

	unregister_chrdev(V2D_MAJOR, "v2d");
	class_destroy(&v2d_class);
}

static struct platform_driver v2d_driver = {
	.probe = v2d_probe,
	.remove = v2d_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "v2d",
		.of_match_table = amlogic_v2d_dt_match,
	}
};

int __init v2d_module_init(void)
{
	pr_debug("v2d module init.\n");

	return platform_driver_register(&v2d_driver);
}

void __exit v2d_module_exit(void)
{
	platform_driver_unregister(&v2d_driver);
}

