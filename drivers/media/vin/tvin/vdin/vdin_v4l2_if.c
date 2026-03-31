// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <asm/div64.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_irq.h>
#include <linux/cma.h>
#include <linux/dma-buf.h>
#include <linux/scatterlist.h>
#include <linux/mm_types.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/iomap.h>
#include <linux/fdtable.h>
#include <linux/dma-buf.h>

/* v4l2 core */
#include <linux/videodev2.h>
#include <linux/v4l2-dv-timings.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-memops.h>

/* Amlogic Headers */
/*#include <linux/amlogic/media/vpu/vpu.h>*/
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/frame_sync/timestamp.h>
#include <linux/amlogic/media/frame_sync/tsync.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/video_sink/video.h>

/* Local Headers */
/*#include "../tvin_global.h"*/
#include "../tvin_format_table.h"
#include "../hdmirx/hdmi_rx_drv_ext.h"
/*#include "../tvin_frontend.h"*/
/*#include "../tvin_global.h"*/
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_sm.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_v4l2_if.h"

/*give a default page size*/
#define VDIN_IMG_SIZE		(1024 * 8)

int vdin_v4l_debug;

#define dprintk(level, fmt, arg...)				\
	do {							\
		if (vdin_v4l_debug >= (level))			\
			pr_info("vdin-v4l: " fmt, ## arg);	\
	} while (0)

static struct v4l2_frmsize_discrete vdin_v4l2_frmsize_dis[] = {
	{320, 240},		{640, 480},		{960, 540},
	{1280, 720},	{1920, 1080},	{3840, 2160}
};

static struct v4l2_fract fract_discrete[] = {
	{.numerator = 1, .denominator = 24,},
	{.numerator = 1, .denominator = 25,},
	{.numerator = 1, .denominator = 30,},
	{.numerator = 1, .denominator = 50,},
	{.numerator = 1, .denominator = 60,},
};

static struct vdin_v4l2_pix_fmt pix_formats[] = {
	{.fourcc = V4L2_PIX_FMT_NV12,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV21,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV12M,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_NV21M,
	 .depth  = 12, },

	{.fourcc = V4L2_PIX_FMT_UYVY,
	 .depth  = 16, },
};

static struct v4l2_capability g_vdin_v4l2_cap[VDIN_MAX_DEVS] = {
	{.driver = VDIN_V4L_DRV_NAME,	 .card = VDIN_V4L_CARD_NAME,
	 .bus_info = VDIN0_V4L_BUS_INFO, .version = VDIN_DEV_VER,
	 .capabilities = VDIN_DEVICE_CAPS | V4L2_CAP_DEVICE_CAPS,
	 .device_caps = VDIN_DEVICE_CAPS},

	{.driver = VDIN_V4L_DRV_NAME,	 .card = VDIN_V4L_CARD_NAME,
	 .bus_info = VDIN1_V4L_BUS_INFO, .version = VDIN_DEV_VER,
	 .capabilities = VDIN_DEVICE_CAPS | V4L2_CAP_DEVICE_CAPS,
	 .device_caps = VDIN_DEVICE_CAPS},

	{.driver = VDIN_V4L_DRV_NAME,	 .card = VDIN_V4L_CARD_NAME,
	 .bus_info = VDIN2_V4L_BUS_INFO, .version = VDIN_DEV_VER,
	 .capabilities = VDIN_DEVICE_CAPS | V4L2_CAP_DEVICE_CAPS,
	 .device_caps = VDIN_DEVICE_CAPS},
};

int vdin_v4l2_if_isr(struct vdin_dev_s *devp, struct vframe_s *vfp)
{
	struct vb2_queue *vb_que;
	struct vb2_buffer *vb2buf;
	struct vb2_v4l2_buffer *vb = NULL;
	int err = 0;

	if (!devp->vb_queue.streaming) {
		err = -1;
		goto error;
	}

	if (devp->dbg_v4l_pause) {
		err = -2;
		goto error;
	}

	/* do framerate control */
	if (devp->vdin_v4l2.divide > 1 && (devp->frame_cnt % devp->vdin_v4l2.divide) != 0) {
		devp->vdin_v4l2.stats.drop_divide++;
		err = -3;
		goto error;
	}
	spin_lock(&devp->list_head_lock);

	if (list_empty(&devp->buf_list)) {
		spin_unlock(&devp->list_head_lock);
		err = -4;
		goto error;
	}

	vb_que = &devp->vb_queue;
	vb2buf = vb_que->bufs[vfp->index];

	vb = to_vb2_v4l2_buffer(vb2buf);
	devp->cur_buff = to_vdin_vb_buf(vb);

	dprintk(3, "v4l2 isr vf index = %d\n", vfp->index);
	devp->cur_buff->v4l2_vframe_s = vfp;
	list_del(&devp->cur_buff->list);
	spin_unlock(&devp->list_head_lock);
	devp->vdin_v4l2.stats.done_cnt++;
	vb2_buffer_done(vb2buf, VB2_BUF_STATE_DONE);

	return 0;
error:
	dprintk(2, "v4l2 isr error:%d\n", err);
	return err;
}

/**
 * enum vb2_buffer_state - current video buffer state
 * @VB2_BUF_STATE_DEQUEUED:	buffer under userspace control
 * @VB2_BUF_STATE_PREPARING:	buffer is being prepared in videobuf
 * @VB2_BUF_STATE_PREPARED:	buffer prepared in videobuf and by the driver
 * @VB2_BUF_STATE_QUEUED:	buffer queued in videobuf, but not in driver
 * @VB2_BUF_STATE_REQUEUEING:	re-queue a buffer to the driver
 * @VB2_BUF_STATE_ACTIVE:	buffer queued in driver and possibly used
 *				in a hardware operation
 * @VB2_BUF_STATE_DONE:		buffer returned from driver to videobuf, but
 *				not yet dequeued to userspace
 * @VB2_BUF_STATE_ERROR:	same as above, but the operation on the buffer
 *				has ended with an error, which will be reported
 *				to the userspace when it is dequeued
 */
char *vb2_buf_sts_to_str(uint32_t state)
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

char *vb2_memory_sts_to_str(uint32_t memory)
{
	switch (memory) {
	case VB2_MEMORY_MMAP:
		return "VB2_MEMORY_MMAP";
	case VB2_MEMORY_USERPTR:
		return "VB2_MEMORY_USERPTR";
	case VB2_MEMORY_DMABUF:
		return "VB2_MEMORY_DMABUF";
	default:
		return "VB2_MEMORY_UNKNOWN";
	}
}

void vdin_fill_pix_format(struct vdin_dev_s *devp)
{
	struct v4l2_format *v4l2_fmt = NULL;
	//unsigned int scan_mod = TVIN_SCAN_MODE_PROGRESSIVE;

	if (IS_ERR_OR_NULL(devp))
		return;

	v4l2_fmt = &devp->v4l2_fmt;

	if (v4l2_fmt->fmt.pix_mp.num_planes == 1) {
		if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_UYVY) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height * 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				(v4l2_fmt->fmt.pix_mp.width * 16) >> 3;
		} else if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
				   v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height * 3 / 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				(v4l2_fmt->fmt.pix_mp.width * 12) >> 3;
		}
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage = 0;
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline = 0;
	} else if (v4l2_fmt->fmt.pix_mp.num_planes == 2) {
		if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV16M ||
			v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV61M) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
		} else if (v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M ||
				   v4l2_fmt->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M) {
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				v4l2_fmt->fmt.pix_mp.width * v4l2_fmt->fmt.pix_mp.height / 2;
			v4l2_fmt->fmt.pix_mp.plane_fmt[0].bytesperline =
				v4l2_fmt->fmt.pix_mp.width;
			v4l2_fmt->fmt.pix_mp.plane_fmt[1].bytesperline =
				v4l2_fmt->fmt.pix_mp.width / 2;
		}
	} else {
		dprintk(0, "vdin%d num_planes=%d error!!!\n",
			devp->index, v4l2_fmt->fmt.pix_mp.num_planes);
		return;
	}
	if (devp->mem_ta_access) {
		v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			roundup(v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage, 64 * 1024);
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
			roundup(v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage, 64 * 1024);
		devp->secure_mem_size = v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage;
	} else {
		v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			PAGE_ALIGN(v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage);
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
			PAGE_ALIGN(v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage);
	}

	dprintk(1, "vdin%d fill fmt:%#x num=%d, p[0].size=%x; p[1].size=%x\n",
		devp->index, v4l2_fmt->fmt.pix_mp.pixelformat, v4l2_fmt->fmt.pix_mp.num_planes,
		v4l2_fmt->fmt.pix_mp.plane_fmt[0].sizeimage,
		v4l2_fmt->fmt.pix_mp.plane_fmt[1].sizeimage);
}

static int vdin_v4l2_get_phy_addr(struct vdin_dev_s *devp,
	struct v4l2_buffer *p, unsigned int plane_no)
{
	int idx;
	int fd;
	struct page *page;
	struct vb2_queue *vb_que = NULL;
	struct vb2_buffer *vb2buf = NULL;
	int err = 0;

	if (p->m.planes[plane_no].m.fd < 0 || p->index >= devp->v4l2_req_buf_num) {
		err = -1;
		goto error;
	}

	idx = p->index;

	if (p->memory == V4L2_MEMORY_MMAP) {
		vb_que  = &devp->vb_queue;
		vb2buf = vb_que->bufs[p->index];
		devp->st_vdin_set_canvas_addr[idx][plane_no].index = idx;
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr =
			vb2_dma_contig_plane_dma_addr(vb2buf, plane_no);
		devp->st_vdin_set_canvas_addr[idx][plane_no].size  =
			vb2buf->planes[plane_no].length;
	} else if (p->memory == V4L2_MEMORY_DMABUF) {
		fd = p->m.planes[plane_no].m.fd;
		if (fd <= 0) {
			err = -2;
			goto error;
		}
		devp->st_vdin_set_canvas_addr[idx][plane_no].fd    = fd;
		devp->st_vdin_set_canvas_addr[idx][plane_no].index = idx;
		devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer = dma_buf_get(fd);
		if (!devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer) {
			err = -3;
			goto error;
		}
		devp->st_vdin_set_canvas_addr[idx][plane_no].dmabuf_attach =
			dma_buf_attach(devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer,
				devp->dev);
		devp->st_vdin_set_canvas_addr[idx][plane_no].sg_table =
			dma_buf_map_attachment(devp->st_vdin_set_canvas_addr[idx][plane_no]
				.dmabuf_attach, DMA_BIDIRECTIONAL);
		page = sg_page(devp->st_vdin_set_canvas_addr[idx][plane_no].sg_table->sgl);
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr = PFN_PHYS(page_to_pfn(page));
		devp->st_vdin_set_canvas_addr[idx][plane_no].size  =
			devp->st_vdin_set_canvas_addr[idx][plane_no].dma_buffer->size;
	} else {
		err = -4;
		goto error;
	}

	if (plane_no == VDIN_PLANES_IDX_Y)
		devp->vf_mem_start[idx] =
			devp->st_vdin_set_canvas_addr[idx][plane_no].paddr;
	else
		devp->vf_mem_c_start[idx] =
			devp->st_vdin_set_canvas_addr[idx][plane_no].paddr;

	devp->vf_mem_start[idx] =
		roundup(devp->vf_mem_start[idx], devp->canvas_align);
	devp->vf_mem_c_start[idx] =
		roundup(devp->vf_mem_c_start[idx], devp->canvas_align);

	dprintk(1, "vdin%d mem:%d, paddr[%d][%d]=0x%lx, size=0x%x\n",
		devp->index, p->memory, idx, plane_no,
		devp->st_vdin_set_canvas_addr[idx][plane_no].paddr,
		devp->st_vdin_set_canvas_addr[idx][plane_no].size);

	return 0;
error:
	dprintk(0, "vdin v4l2 get phy_addr error:%d\n", err);
	return err;
}

/*
 * Query device capability
 * cmd ID:VIDIOC_QUERYCAP
 */
static int vdin_vidioc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp) || !cap)
		return -EFAULT;

	cap->version	  = g_vdin_v4l2_cap[devp->index].version;
	cap->device_caps  = g_vdin_v4l2_cap[devp->index].device_caps;
	cap->capabilities = g_vdin_v4l2_cap[devp->index].capabilities;

	strncpy(cap->driver, g_vdin_v4l2_cap[devp->index].driver, sizeof(cap->driver));
	strncpy(cap->bus_info, g_vdin_v4l2_cap[devp->index].bus_info, sizeof(cap->bus_info));
	strncpy(cap->card, g_vdin_v4l2_cap[devp->index].card, sizeof(cap->card));

	return 0;
}

/*
 * query video standard
 * cmd ID: VIDIOC_QUERYSTD
 */
static int vdin_vidioc_querystd(struct file *file, void *priv,
				v4l2_std_id *std)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	return 0;
}

/*
 * enum current input
 * cmd ID: VIDIOC_ENUMINPUT
 */
static int vdin_vidioc_enum_input(struct file *file,
				  void *fh, struct v4l2_input *inp)
{
	const char *str = NULL;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (inp->index >= devp->v4l2_port_num) {
		dprintk(0, "vdin%d enum input %d error!!!\n",
			inp->index, devp->v4l2_port_num);
		return -EINVAL;
	}

	inp->std = V4L2_STD_ALL;
	str = tvin_port_str(devp->v4l2_port[inp->index]);

	if (str)
		strscpy(inp->name, str, sizeof(inp->name));

	dprintk(1, "vdin%d enum input port[%d]:%s\n",
		devp->index, inp->index, inp->name);
	return 0;
}

/*
 * allocate the video frame buffer
 * cmd ID:VIDIOC_REQBUFS
 */
static int vdin_vidioc_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;
	unsigned int i = 0;
	struct vb2_v4l2_buffer *vb_buf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;
	int err;

	if (IS_ERR_OR_NULL(devp)) {
		err = -EPERM;
		goto error;
	}
	if (reqbufs->memory != V4L2_MEMORY_DMABUF &&
		reqbufs->memory != V4L2_MEMORY_MMAP) {
		err = -EINVAL;
		goto error;
	}

	if (reqbufs->count == 0) {
		//return 0;
	}
	if (reqbufs->count && (reqbufs->count < devp->vb_queue.min_queued_buffers ||
		reqbufs->count > VDIN_CANVAS_MAX_CNT)) {
		err = -EINVAL;
		goto error;
	}

	/*need config by input source type*/
	devp->source_bitdepth = VDIN_COLOR_DEEPS_8BIT;
	devp->vb_queue.type = reqbufs->type;

	//vdin_buffer_calculate(devp, req_buffs_num);
	//vdin_fill_pix_format(devp);

	ret = vb2_ioctl_reqbufs(file, priv, reqbufs);

	devp->v4l2_req_buf_num = reqbufs->count;

	vb_buf = to_vb2_v4l2_buffer(devp->vb_queue.bufs[i]);
	vdin_buf = to_vdin_vb_buf(vb_buf);

	dprintk(1, "vdin%d v4l2 req buf mem:%d, type:%d cnt:%d buf_num:%d\n",
		devp->index, reqbufs->memory, reqbufs->type, reqbufs->count,
		devp->vb_queue.max_num_buffers);
	return ret;
error:
	dprintk(1, "vdin v4l2 req buf error:%d\n", err);
	return err;
}

/*
 *
 * cmd ID:VIDIOC_CREATE_BUFS
 */
int vdin_vidioc_create_bufs(struct file *file, void *priv,
			    struct v4l2_create_buffers *p)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	unsigned int ret = 0;

	dprintk(2, "vdin create buf\n");
	ret = vb2_ioctl_create_bufs(file, priv, p);
	return ret;
}

/*
 * Got every buffer info, and mmp to userspace
 * cmd ID:VIDIOC_QUERYBUF
 */
static int vdin_vidioc_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *v4lbuf)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vb2_queue *vb_que = NULL;
	unsigned int ret = 0;
	struct vb2_buffer *vb2buf = NULL;

	if (IS_ERR_OR_NULL(devp) || IS_ERR_OR_NULL(v4lbuf))
		return -EFAULT;

	dprintk(1, "vdin query buf id:%d\n", v4lbuf->index);

	vb_que = &devp->vb_queue;
	if (v4lbuf->index >= vb_que->max_num_buffers) {
		dprintk(0, "vdin query buf id %d out of range (max %d)\n",
			v4lbuf->index, vb_que->max_num_buffers - 1);
		return -EINVAL;
	}
	vb2buf = vb_que->bufs[v4lbuf->index];

	ret = vb2_ioctl_querybuf(file, priv, v4lbuf);

	return ret;
}

/*
 * user put a empty vframe to driver empty video buffer
 * cmd ID:VIDIOC_QBUF
 */
static int vdin_vidioc_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *p)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;
	int i;
	unsigned int num_planes;
	int err = 0;

	if (IS_ERR_OR_NULL(devp) || IS_ERR_OR_NULL(p)) {
		err  = -EFAULT;
		goto error;
	}

	if (p->index >= devp->vb_queue.max_num_buffers) {
		err = -EINVAL;
		goto error;
	}

	vb = to_vb2_v4l2_buffer(devp->vb_queue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);

	num_planes = vb->vb2_buf.num_planes;
	if (num_planes > 3 || num_planes == 0) {
		err = -EINVAL;
		goto error;
	}
	dprintk(3, "vdin qbuf id:%d planes:%d, streaming:%d\n",
		p->index, num_planes, devp->vb_queue.streaming);

	ret = vb2_ioctl_qbuf(file, priv, p);
	if (ret < 0)
		dprintk(0, "vdin%d qbuf err\n", devp->index);

	/* recycle buffer */
	if (devp->vb_queue.streaming) {
		devp->vdin_v4l2.stats.que_cnt++;
		if (vdin_buf->v4l2_vframe_s && !IS_ERR(vdin_buf->v4l2_vframe_s)) {
			if (!devp->vfp) {
				err = -EINVAL;
				goto error;
			}
			receiver_vf_put(vdin_buf->v4l2_vframe_s, devp->vfp);
			dprintk(3, "vdin qbuf vf idx:%d (0x%p) put back to pool,fd=%d\n",
				vdin_buf->v4l2_vframe_s->index, devp->vfp, p->m.fd);

			vdin_buf->v4l2_vframe_s = NULL;
		} else {
			dprintk(0, "err vf null\n");
		}
	} else {
		for (i = 0; i < num_planes; i++)
			vdin_v4l2_get_phy_addr(devp, p, i);
	}

	return ret;
error:
	dprintk(0, "vdin qbuf error:%d\n", err);
	return err;
}

/*
 * user get a vframe from driver filled video buffer
 * cmd ID:VIDIOC_DQBUF
 */
static int vdin_vidioc_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *p)
{
	unsigned int ret = 0, i;
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vb2_v4l2_buffer *vb = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	ret = vb2_ioctl_dqbuf(file, priv, p);
	if (ret) {
		if (devp->parm.info.status == TVIN_SIG_STATUS_NOSIG)
			ret = -ENODEV;
		goto error;
	}
	if (p->index >= devp->vb_queue.max_num_buffers) {
		ret = -EINVAL;
		goto error;
	}

	for (i = 0; i < devp->v4l2_fmt.fmt.pix_mp.num_planes; i++)
		p->m.planes[i].bytesused = p->m.planes[i].length;

	/*frame_cnt++;*/
	vb = to_vb2_v4l2_buffer(devp->vb_queue.bufs[p->index]);
	vdin_buf = to_vdin_vb_buf(vb);
	devp->vdin_v4l2.stats.dque_cnt++;
	dprintk(3, "vdin v4l2 dq id=%d, fd=%d, vf_id:%d\n",
		p->index, p->m.fd, vdin_buf->v4l2_vframe_s->index);

	return ret;
error:
	dprintk(0, "vdin v4l2 dq error:%d\n", ret);
	return ret;
}

/*
 *
 * cmd ID:VIDIOC_EXPBUF
 */
static int vdin_vidioc_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *p)
{
	struct vdin_dev_s *devp = video_drvdata(file);
//	struct dma_buf *dmabuf;
	int ret;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(1, "vdin v4l2 exp buf:%d\n", p->index);

	ret = vb2_ioctl_expbuf(file, priv, p);

//	dmabuf = dma_buf_get(p->fd);
//	if (IS_ERR_OR_NULL(dmabuf))
//		dprintk(0, "get dma buf err\n");

	return ret;
}

static int vdin_vidioc_streamon(struct file *file, void *priv,
				enum v4l2_buf_type i)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;
	if (devp->hw_core == VDIN_HW_CORE_NORMAL &&
		devp->parm.info.status != TVIN_SIG_STATUS_STABLE) {
		ret = -EBUSY;
		goto error;
	}

	if ((!get_video_enabled(0) &&
		(devp->v4l2_port_cur == TVIN_PORT_VIU1_VIDEO ||
		devp->v4l2_port_cur == TVIN_PORT_VIU1_WB0_VD1)) ||
		(!get_video_enabled(1) &&
		devp->v4l2_port_cur == TVIN_PORT_VIU2_VD1)) {
		dprintk(0, "%s capture video but no video\n", __func__);
		if (devp->mem_ta_access) {
			ret = -EBUSY;
			goto error;
		}
	}

	ret = vb2_ioctl_streamon(file, priv, i);
	if (ret < 0) {
		goto error;
	}
	dprintk(2, "vdin%d v4l2 streamon ok\n", devp->index);

	vdin_v4l2_start_tvin(devp);
	memset(&devp->vdin_v4l2.stats, 0, sizeof(devp->vdin_v4l2.stats));
	return ret;
error:
	dprintk(0, "vdin v4l2 stream error:%d\n", ret);
	return ret;
}

static int vdin_vidioc_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type i)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	int ret = 0;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	ret = vb2_ioctl_streamoff(file, priv, i);
	if (ret < 0)
		dprintk(0, "vdin v4l2 streamoff error\n");

	ret = vdin_v4l2_stop_tvin(devp);

	return ret;
}

/*
 * for single plane
 * cmd ID:VIDIOC_G_FMT
 */
static int vdin_vidioc_g_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;
	/*for test set a default value*/
	devp->v4l2_fmt.fmt.pix.width = 1280;
	devp->v4l2_fmt.fmt.pix.height = 720;
	devp->v4l2_fmt.fmt.pix.quantization = V4L2_QUANTIZATION_DEFAULT;
	devp->v4l2_fmt.fmt.pix.ycbcr_enc = V4L2_YCBCR_ENC_709;
	devp->v4l2_fmt.fmt.pix.field = V4L2_FIELD_ANY;
	devp->v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	memcpy(&f->fmt.pix, &devp->v4l2_fmt.fmt.pix,
		sizeof(struct v4l2_pix_format));

	dprintk(2, "vdin v4l2 get cap:%dx%d\n",
		devp->v4l2_fmt.fmt.pix.width, devp->v4l2_fmt.fmt.pix.height);
	return 0;
}

/*
 * user get a vframe from driver filled video buffer
 * cmd ID:VIDIOC_S_FMT
 */
static int vdin_vidioc_s_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_format *fmt)
{
	struct vdin_dev_s *devp = video_drvdata(file);
	/*struct v4l2_format *fmt = devp->pixfmt;*/

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	dprintk(1, "vdin v4l2 set cap=%dx%d pix_fmt=0x%x\n",
		fmt->fmt.pix.width, fmt->fmt.pix.height, fmt->fmt.pix.pixelformat);

	return 0;
}

/*
 * for mplane
 * cmd ID:VIDIOC_G_FMT
 */
static int vdin_vidioc_g_fmt_vid_cap_mplane(struct file *file,
					    void *priv,
					    struct v4l2_format *f)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	/* for test set a default value
	 * mult-planes mode
	 */
//	devp->v4l2_fmt.fmt.pix_mp.width = 1920;
//	devp->v4l2_fmt.fmt.pix_mp.height = 1080;
//	devp->v4l2_fmt.fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
//	devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_709;
//	devp->v4l2_fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
//	devp->v4l2_fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV;
//	devp->v4l2_fmt.fmt.pix_mp.num_planes = VDIN_NUM_PLANES;

	f->fmt.pix_mp.width = devp->v4l2_fmt.fmt.pix_mp.width;
	f->fmt.pix_mp.height = devp->v4l2_fmt.fmt.pix_mp.height;
	f->fmt.pix_mp.quantization = devp->v4l2_fmt.fmt.pix_mp.quantization;
	f->fmt.pix_mp.ycbcr_enc = devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc;
	f->fmt.pix_mp.field = devp->v4l2_fmt.fmt.pix_mp.field;
	f->fmt.pix_mp.pixelformat = devp->v4l2_fmt.fmt.pix_mp.pixelformat;
	f->fmt.pix_mp.num_planes = devp->v4l2_fmt.fmt.pix_mp.num_planes;
	dprintk(1, "vdin v4l2 get %d plane %dx%d,quant:%d,enc:%d,field:%d,pix_fmt:0x%x\n",
		f->fmt.pix_mp.num_planes, f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.quantization, f->fmt.pix_mp.ycbcr_enc,
		f->fmt.pix_mp.field, f->fmt.pix_mp.pixelformat);
	return 0;
}

/*
 * for mplane
 * cmd ID:VIDIOC_S_FMT
 */
static int vdin_vidioc_s_fmt_vid_cap_mplane(struct file *file,
					    void *priv,
					    struct v4l2_format *fmt)
{
	int i;
	struct vdin_dev_s *devp = video_drvdata(file);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (devp->v4l2_dbg_ctl.dbg_pix_fmt)
		devp->v4l2_fmt.fmt.pix_mp.pixelformat = devp->v4l2_dbg_ctl.dbg_pix_fmt;

#ifndef VDIN_V4L2_FILL_PIXFMT_MP
	devp->v4l2_fmt.fmt.pix_mp.width	       = fmt->fmt.pix_mp.width;
	devp->v4l2_fmt.fmt.pix_mp.height       = fmt->fmt.pix_mp.height;
	devp->v4l2_fmt.fmt.pix_mp.quantization = fmt->fmt.pix_mp.quantization;
	devp->v4l2_fmt.fmt.pix_mp.ycbcr_enc    = fmt->fmt.pix_mp.ycbcr_enc;
	devp->v4l2_fmt.fmt.pix_mp.field        = fmt->fmt.pix_mp.field;
	devp->v4l2_fmt.fmt.pix_mp.pixelformat  = fmt->fmt.pix_mp.pixelformat;
	devp->v4l2_fmt.fmt.pix_mp.num_planes   = fmt->fmt.pix_mp.num_planes;

	vdin_fill_pix_format(devp);
	for (i = 0; i < fmt->fmt.pix_mp.num_planes; i++) {
		fmt->fmt.pix_mp.plane_fmt[i].bytesperline =
			devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].bytesperline;
		fmt->fmt.pix_mp.plane_fmt[i].sizeimage =
			devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
	}
#else
	v4l2_fill_pixfmt_mp(&devp->v4l2_fmt.fmt.pix_mp,
		fmt->fmt.pix_mp.pixelformat, fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height);
#endif
	dprintk(1, "vdin v4l2 set %d plane %dx%d,quant:%d,enc:%d,field:%d,pix_fmt:0x%x\n",
		fmt->fmt.pix_mp.num_planes, fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
		fmt->fmt.pix_mp.quantization, fmt->fmt.pix_mp.ycbcr_enc,
		fmt->fmt.pix_mp.field, fmt->fmt.pix_mp.pixelformat);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE */
static int vdin_vidioc_s_divide_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0 || ctrl->value > 240) {
		dprintk(0, "vdin v4l2 set divide = %d over range\n",
			ctrl->value);
		return -EINVAL;
	}
	devp->vdin_v4l2.divide = ctrl->value;
	dprintk(2, "vdin v4l2 set divide = %d\n", devp->vdin_v4l2.divide);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_DONE_USER_PROCESSING */
static int vdin_vidioc_s_done_user_process(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0 || ctrl->value >= devp->vfp->size) {
		dprintk(0, "vdin v4l2 done process =%d over range\n",
			ctrl->value);
		return -EINVAL;
	}
	/* TODO */

	return -ENOTTY;
}

/* AML_V4L2_SET_DRM_MODE */
static int vdin_vidioc_s_drm_mode(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	if (ctrl->value < 0) {
		dprintk(0, "vdin v4l2 set drm =%d over range\n",
			ctrl->value);
		return -EINVAL;
	}

	devp->vdin_v4l2.secure_flg = !!ctrl->value;
	dprintk(0, "vdin%d v4l2 set drm:%d\n",
		devp->index, devp->vdin_v4l2.secure_flg);

	return 0;
}

static int vdin_vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int ret = 0;
	struct vdin_dev_s *devp = video_drvdata(file);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
		ret = vdin_vidioc_s_divide_fr(devp, ctrl);
	else if (ctrl->id == V4L2_CID_EXT_CAPTURE_DONE_USER_PROCESSING)
		ret = vdin_vidioc_s_done_user_process(devp, ctrl);
	else if (ctrl->id == AML_V4L2_SET_DRM_MODE)
		ret = vdin_vidioc_s_drm_mode(devp, ctrl);

	return ret;
}

/* V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE */
static int vdin_vidioc_g_divide_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	ctrl->value = devp->vdin_v4l2.divide;
	dprintk(2, "vdin v4l2 get divide = %d\n", ctrl->value);

	return 0;
}

/* V4L2_CID_MIN_BUFFERS_FOR_CAPTURE */
static int vdin_vidioc_g_min_buffers(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	ctrl->value = 2;
	dprintk(2, "vdin v4l2 get min buf = %d\n", ctrl->value);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_OUTPUT_FRAMERATE */
static int vdin_vidioc_g_output_fr(struct vdin_dev_s *devp,
			 struct v4l2_control *ctrl)
{
	ctrl->value = devp->prop.fps;
	dprintk(2, "vdin v4l2 get output fr = %d\n", ctrl->value);

	return 0;
}

static int vdin_vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	int ret = 0;
	struct vdin_dev_s *devp = video_drvdata(file);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (ctrl->id == V4L2_CID_EXT_CAPTURE_DIVIDE_FRAMERATE)
		ret = vdin_vidioc_g_divide_fr(devp, ctrl);
	else if (ctrl->id == V4L2_CID_EXT_CAPTURE_OUTPUT_FRAMERATE)
		ret = vdin_vidioc_g_output_fr(devp, ctrl);
	else if (ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE)
		ret = vdin_vidioc_g_min_buffers(devp, ctrl);

	return ret;
}

/* V4L2_CID_EXT_CAPTURE_CAPABILITY_INFO */
static int vdin_vidioc_g_cid_cap_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_capability_info)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_cap_info,
				sizeof(struct v4l2_ext_capture_capability_info)))
			return -EFAULT;
	} else {
		dprintk(0, "vdin%d v4l2 get cap info error!!!\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_INFO */
static int vdin_vidioc_g_cid_plane_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_plane_info)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_plane_info,
				sizeof(struct v4l2_ext_capture_plane_info)))
			return -EFAULT;
	} else {
		dprintk(0, "vdin%d v4l2 get plane info error!!!\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO */
static int vdin_vidioc_g_cid_video_win_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	unsigned int input_h_size = 0, input_v_size = 0;
	unsigned int output_h_size, output_v_size;
	struct video_input_info video_input_parms;
	int cur_axis[4];

	memset(&cur_axis, 0, sizeof(cur_axis));
	memset(&video_input_parms, 0, sizeof(struct video_input_info));

	if (devp->hw_core == VDIN_HW_CORE_LITE) {
		if (vdin_get_video_ready_state(devp->v4l2_port_cur)) {
			if (devp->v4l2_port_cur == TVIN_PORT_VIU1_WB0_VD1) {
				get_vdx_real_axis(0, cur_axis);
				input_h_size = cur_axis[2] - cur_axis[0] + 1;
				input_v_size = cur_axis[3] - cur_axis[1] + 1;
			} else if (devp->v4l2_port_cur == TVIN_PORT_VIU2_VD1) {
				get_vdx_real_axis(1, cur_axis);
				input_h_size = cur_axis[2] - cur_axis[0] + 1;
				input_v_size = cur_axis[3] - cur_axis[1] + 1;
			} else if (devp->v4l2_port_cur == TVIN_PORT_VIU1_VIDEO) {
				get_video_input_info(&video_input_parms);
				input_h_size = video_input_parms.width;
				input_v_size = video_input_parms.height;
			}

			devp->ext_cap_video_win_info.in.w = input_h_size;
			devp->ext_cap_video_win_info.in.h = input_v_size;

			output_h_size = devp->v4l2_fmt.fmt.pix_mp.width;
			output_v_size = devp->v4l2_fmt.fmt.pix_mp.height;

			if (output_h_size > input_h_size)
				devp->ext_cap_video_win_info.out.w = input_h_size;
			if (output_v_size > input_v_size)
				devp->ext_cap_video_win_info.out.h = input_v_size;
		}
	}

	if (control->size == sizeof(struct v4l2_ext_capture_video_win_info)) {
		dprintk(3, "vdin%d v4l2 get video win:%dx%d -> %dx%d\n",
			devp->index,
			devp->ext_cap_video_win_info.in.w, devp->ext_cap_video_win_info.in.h,
			devp->ext_cap_video_win_info.out.w, devp->ext_cap_video_win_info.out.h);
		if (copy_to_user(control->ptr, &devp->ext_cap_video_win_info,
				sizeof(struct v4l2_ext_capture_video_win_info)))
			return -EFAULT;
	} else {
		dprintk(0, "vdin%d v4l2 get video win error\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_FREEZE_MODE */
static int vdin_vidioc_g_cid_freeze_mode(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_freeze_mode)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_freezee_mode,
				sizeof(struct v4l2_ext_capture_freeze_mode)))
			return -EFAULT;
	} else {
		dprintk(0, "vdin%d v4l2 get freeze mode error\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_PHYSICAL_MEMORY_INFO */
static int vdin_vidioc_g_cid_phy_mem_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	int ret = 0;
	unsigned int chroma_size = 0;
	struct v4l2_ext_capture_physical_memory_info info;

	memset(&info, 0, sizeof(struct v4l2_ext_capture_physical_memory_info));

	if (control->size == sizeof(struct v4l2_ext_capture_physical_memory_info)) {
		if (copy_from_user(&info, control->ptr,
				sizeof(struct v4l2_ext_capture_physical_memory_info))) {
			ret = -EFAULT;
			goto error;
		}
		if (info.buf_index < 0 || info.buf_index >= VDIN_CANVAS_MAX_CNT) {
			ret = -EINVAL;
			goto error;
		}

		switch (devp->format_convert) {
		case VDIN_FORMAT_CONVERT_YUV_NV12:
		case VDIN_FORMAT_CONVERT_YUV_NV21:
		case VDIN_FORMAT_CONVERT_RGB_NV12:
		case VDIN_FORMAT_CONVERT_RGB_NV21:
			if (!devp->mem_ta_access)
				chroma_size = devp->canvas_w * devp->canvas_h / 2;
			else
				chroma_size = devp->v4l2_fmt.fmt.pix_mp.width *
					devp->v4l2_fmt.fmt.pix_mp.height;
			break;
		default:
			break;
		}

		info.compat_y_data = (unsigned int)devp->vf_mem_start[info.buf_index];
		info.compat_c_data = info.compat_y_data + chroma_size;

		info.buf_location = V4L2_EXT_CAPTURE_INPUT_BUF;

		dprintk(1, "vdin%d v4l2 get phy mem id:%d, y:%#x, c:%#x(%#x), buf_loc:%d\n",
			devp->index, info.buf_index, info.compat_y_data, info.compat_c_data,
			(unsigned int)devp->vf_mem_c_start[info.buf_index], info.buf_location);
		if (copy_to_user(control->ptr, &info,
				sizeof(struct v4l2_ext_capture_physical_memory_info))) {
			ret = -EFAULT;
			goto error;
		}
	} else {
		ret = -EINVAL;
		goto error;
	}

	return 0;
error:
	dprintk(0, "vdin%d v4l2 get phy mem error:%d\n", devp->index, ret);
	return ret;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_PROP */
static int vdin_vidioc_g_cid_plane_prop(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	if (control->size == sizeof(struct v4l2_ext_capture_plane_prop)) {
		if (copy_to_user(control->ptr, &devp->ext_cap_plane_prop,
				sizeof(struct v4l2_ext_capture_plane_prop)))
			return -EFAULT;
	} else {
		dprintk(0, "vdin%d v4l2 get plane prop error\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

static int vdin_vidioc_g_ext_ctrls(struct file *file, void *fh,
			  struct v4l2_ext_controls *a)
{
	int i, ret = 0;
	struct v4l2_ext_control *ctrl = a->controls;
	struct vdin_dev_s *devp = video_drvdata(file);
	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	/* all controls in the control array must belong
	 * to the same control class
	 */

	for (i = 0; i < a->count; ctrl++, i++) {
		/* check control valid */
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_CAPABILITY_INFO:
			ret = vdin_vidioc_g_cid_cap_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PLANE_INFO:
			ret = vdin_vidioc_g_cid_plane_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO:
			ret = vdin_vidioc_g_cid_video_win_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_FREEZE_MODE:
			ret = vdin_vidioc_g_cid_freeze_mode(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PHYSICAL_MEMORY_INFO:
			ret = vdin_vidioc_g_cid_phy_mem_info(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_PLANE_PROP:
			ret = vdin_vidioc_g_cid_plane_prop(devp, ctrl);
			break;
		default:
			break;
		}
		if (ret)
			break;
	}

	return ret;
}

/* V4L2_CID_EXT_CAPTURE_PLANE_PROP */
static int vdin_vidioc_s_cid_plane_prop(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	struct v4l2_ext_capture_plane_prop prop;
	enum tvin_port_e v4l2_port = TVIN_PORT_VIU1_WB0_VPP;

	if (control->size == sizeof(struct v4l2_ext_capture_plane_prop)) {
		if (copy_from_user(&prop, control->ptr,
				sizeof(struct v4l2_ext_capture_plane_prop)))
			return -EFAULT;

		if (prop.l <= V4L2_EXT_CAPTURE_SCALER_INPUT ||
			prop.l > V4L2_EXT_CAPTURE_OSD_OUTPUT)
			return -ENOTTY;
		devp->vdin_v4l2.l = prop.l;

		switch (devp->vdin_v4l2.l) {
		case V4L2_EXT_CAPTURE_SCALER_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_VD1;
			break;
		case V4L2_EXT_CAPTURE_DISPLAY_OUTPUT:
			/* no match loopback point */
			v4l2_port = TVIN_PORT_VIU1_WB0_VD1;
			break;
		case V4L2_EXT_CAPTURE_BLENDED_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_VPP;
			break;
		case V4L2_EXT_CAPTURE_OSD_OUTPUT:
			v4l2_port = TVIN_PORT_VIU1_WB0_OSD1;
			break;
		default:
			break;
		}
		mutex_lock(&devp->fe_lock);
		devp->parm.index	= devp->index;
		devp->parm.port		= v4l2_port;
		devp->v4l2_port_cur = v4l2_port;
		devp->unstable_flag = false;
		mutex_unlock(&devp->fe_lock);

		dprintk(1, "vdin%d set plane prop location:%#x, port:%d(%s)\n",
			devp->index, devp->vdin_v4l2.l,
			devp->v4l2_port_cur, tvin_port_str(devp->v4l2_port_cur));
	} else {
		dprintk(0, "vdin%d v4l2 set plane prop error\n", devp->index);
		return -EINVAL;
	}

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_FREEZE_MODE */
static int vdin_vidioc_s_cid_freeze_mode(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	struct v4l2_ext_capture_freeze_mode mode;

	if (copy_from_user(&mode, control->ptr, sizeof(struct v4l2_ext_capture_freeze_mode)))
		return -EFAULT;
	/* check args valid */
	//freeze one frame or all capture frames ?
	devp->ext_cap_freezee_mode.plane_index = mode.plane_index;
	devp->ext_cap_freezee_mode.val = mode.val;
	//devp->pause_dec = control->value;
	dprintk(1, "vdin%d set freeze mode plane_id:%d, val:%#x\n",
		devp->index, mode.plane_index, mode.val);

	return 0;
}

/* V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO */
static int vdin_vidioc_s_cid_video_win_info(struct vdin_dev_s *devp,
	struct v4l2_ext_control *control)
{
	/* LG chip is only supported */
	return -ENOTTY;
}

static int vdin_vidioc_s_ext_ctrls(struct file *file, void *fh,
			  struct v4l2_ext_controls *a)
{
	int i;
	struct v4l2_ext_control *ctrl = a->controls;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	/* all controls in the control array must belong
	 * to the same control class
	 */

	for (i = 0; i < a->count; ctrl++, i++) {
		switch (ctrl->id) {
		case V4L2_CID_EXT_CAPTURE_PLANE_PROP:
			vdin_vidioc_s_cid_plane_prop(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_FREEZE_MODE:
			vdin_vidioc_s_cid_freeze_mode(devp, ctrl);
			break;
		case V4L2_CID_EXT_CAPTURE_VIDEO_WIN_INFO:
			vdin_vidioc_s_cid_video_win_info(devp, ctrl);
			break;
		default:
			break;
		}
	}

	return 0;
}

static int vdin_vidioc_try_ext_ctrls(struct file *file, void *fh,
				    struct v4l2_ext_controls *a)
{
	/* struct vdin_dev_s *devp = video_drvdata(file); */
	return 0;
}

static int vdin_vidioc_g_input(struct file *file, void *priv, unsigned int *i)
{
	int idx;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	for (idx = 0; idx < devp->v4l2_port_num; idx++) {
		if (devp->v4l2_port_cur == devp->v4l2_port[idx]) {
			*i = idx;
			break;
		}
	}

	return 0;
}

/* set input and open vdin fe */
static int vdin_vidioc_s_input(struct file *file, void *priv, unsigned int i)
{
	int ret;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp)) {
		ret = -EFAULT;
		goto error;
	}

	if (i >= devp->v4l2_port_num) {
		ret = -EINVAL;
		goto error;
	}

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		dprintk(0, "%s warning VDIN_FLAG_DEC_STARTED\n", __func__);
		stop_tvin_service(devp->index);
		vdin_close_fe(devp);
	}

	mutex_lock(&devp->fe_lock);

	devp->parm.index    = devp->index;
	devp->parm.port     = devp->v4l2_port[i];
	devp->v4l2_port_cur = devp->v4l2_port[i];
	devp->unstable_flag = false;

	devp->work_mode = VDIN_WORK_MD_V4L;
	devp->afbce_flag = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	if (IS_HDMI_SRC(devp->v4l2_port_cur))
		rx_update_edid_callback(devp->v4l2_port_cur, RX_EDID_REMOVE_HDR);
#endif
	if (devp->parm.port != TVIN_PORT_MIPI && devp->index == 0 &&
		!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
		ret = vdin_open_fe(devp->parm.port, 0, devp);
		if (ret) {
			mutex_unlock(&devp->fe_lock);
			ret = -EFAULT;
			goto error;
		}
	}

	if (devp->parm.port == TVIN_PORT_MIPI) {
		devp->frontend = tvin_get_frontend(devp->parm.port, devp->parm.index);
		if (!(devp->frontend)) {
			mutex_unlock(&devp->fe_lock);
			ret = -1;
			goto error;
		}
		if (devp->frontend && devp->frontend->sm_ops &&
			devp->frontend->sm_ops->get_fmt)
			devp->parm.info.fmt =
				devp->frontend->sm_ops->get_fmt(devp->frontend, 0);
		else
			devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_1920X1080P_30HZ;
	}

	/* mipi-csi and loopback donot support state_machine */
	if (devp->hw_core == VDIN_HW_CORE_LITE ||
		devp->parm.port == TVIN_PORT_MIPI) {
		devp->parm.info.status = TVIN_SIG_STATUS_STABLE;
	}
	mutex_unlock(&devp->fe_lock);

	dprintk(0, "vdin%d set input port:%#x(%s), status:%d, fmt:%s\n",
		devp->index, devp->v4l2_port_cur, tvin_port_str(devp->v4l2_port_cur),
		devp->parm.info.status, tvin_sig_fmt_str(devp->parm.info.fmt));
	return 0;
error:
	dprintk(0, "vdin v4l2 set input error:%d\n", ret);
	return ret;
}

static int vdin_vidioc_enum_fmt_vid_cap(struct file *file, void *priv,
				   struct v4l2_fmtdesc *f)
{
	struct vdin_v4l2_pix_fmt *fmt;

	if (!f || f->index >= ARRAY_SIZE(pix_formats))
		return -EINVAL;

	fmt = &pix_formats[f->index];
	/* description will be filled by v4l_fill_fmtdesc */
	f->pixelformat = fmt->fourcc;

	dprintk(0, "vdin v4l2 enum fmt cap id:%d pix_fmt:%x\n",
		f->index, f->pixelformat);

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int i = 0;
	int ret = 0;
	int err = 0;
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp)) {
		ret = -EFAULT;
		err = -1;
		goto error;
	}

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = -EINVAL;
		err = -2;
		goto error;
	}
	if (f->fmt.pix_mp.width > 4096 || f->fmt.pix_mp.height > 2160) {
		ret = -EINVAL;
		err = -3;
		goto error;
	}
	if (f->fmt.pix_mp.num_planes > 2) {
		ret = -EINVAL;
		err = -4;
		goto error;
	}
	for (i = 0; i < ARRAY_SIZE(pix_formats); i++) {
		if (f->fmt.pix_mp.pixelformat == pix_formats[i].fourcc)
			break;
	}
	if (i >= ARRAY_SIZE(pix_formats)) {
		ret = -EINVAL;
		err = -5;
		goto error;
	}

	dprintk(1, "vdin%d v4l2 try fmt: %dx%d num_planes=%d\n",
		devp->index, f->fmt.pix_mp.width, f->fmt.pix_mp.height, f->fmt.pix_mp.num_planes);

	return 0;
error:
	dprintk(0, "vdin v4l2 try fmt error:%d\n", err);
	return ret;
}

/* This is an experimental interface */
static int vdin_vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	int ret = 0, i = 0;
	struct vdin_v4l2_pix_fmt *fmt = NULL;
	struct v4l2_frmsize_discrete *frmsize = NULL;

	for (i = 0; i < ARRAY_SIZE(pix_formats); i++) {
		if (pix_formats[i].fourcc == fsize->pixel_format) {
			fmt = &pix_formats[i];
			break;
		}
	}
	if (!fmt)
		return -EINVAL;
	if (fsize->index >= ARRAY_SIZE(vdin_v4l2_frmsize_dis))
		return -EINVAL;

	frmsize = &vdin_v4l2_frmsize_dis[fsize->index];
	/* TODO:vdin can only scale down
	 * width & height should less than real format size.
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width  = frmsize->width;
	fsize->discrete.height = frmsize->height;

	return ret;
}

static int vdin_vidioc_enum_frameintervals(struct file *file, void *priv,
				      struct v4l2_frmivalenum *fival)
{
	if (fival->index >= ARRAY_SIZE(fract_discrete))
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->discrete.numerator   = fract_discrete[fival->index].numerator;
	fival->discrete.denominator = fract_discrete[fival->index].denominator;

	return 0;
}

static int vdin_vidioc_g_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	return 0;
}

static int vdin_vidioc_s_parm(struct file *file, void *priv,
			 struct v4l2_streamparm *parms)
{
	return 0;
}

static const struct v4l2_ioctl_ops vdin_v4l2_ioctl_ops = {
	.vidioc_querystd	= vdin_vidioc_querystd,
	.vidioc_enum_input	= vdin_vidioc_enum_input,
	.vidioc_s_input		= vdin_vidioc_s_input,
	.vidioc_g_input		= vdin_vidioc_g_input,
	.vidioc_enum_fmt_vid_cap	= vdin_vidioc_enum_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt_vid_cap_mplane,
	.vidioc_querycap			= vdin_vidioc_querycap,
	.vidioc_enum_framesizes		= vdin_vidioc_enum_framesizes,
	.vidioc_enum_frameintervals = vdin_vidioc_enum_frameintervals,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm = vdin_vidioc_g_parm,
	.vidioc_s_parm = vdin_vidioc_s_parm,

	/*queue io control*/
	.vidioc_reqbufs = vdin_vidioc_reqbufs,
	.vidioc_create_bufs = vdin_vidioc_create_bufs,
	.vidioc_querybuf = vdin_vidioc_querybuf,
	.vidioc_qbuf = vdin_vidioc_qbuf,
	.vidioc_dqbuf = vdin_vidioc_dqbuf,
	.vidioc_expbuf = vdin_vidioc_expbuf,
	.vidioc_streamon = vdin_vidioc_streamon,
	.vidioc_streamoff = vdin_vidioc_streamoff,

	.vidioc_g_ctrl = vdin_vidioc_g_ctrl,
	.vidioc_s_ctrl = vdin_vidioc_s_ctrl,

	.vidioc_g_ext_ctrls		= vdin_vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls		= vdin_vidioc_s_ext_ctrls,
	.vidioc_try_ext_ctrls	= vdin_vidioc_try_ext_ctrls,

	.vidioc_s_fmt_vid_cap_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap			= vdin_vidioc_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out_mplane	= vdin_vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out			= vdin_vidioc_s_fmt_vid_cap,

	.vidioc_g_fmt_vid_cap_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_cap			= vdin_vidioc_g_fmt_vid_cap,
	.vidioc_g_fmt_vid_out_mplane	= vdin_vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out			= vdin_vidioc_g_fmt_vid_cap,
};

static void vdin_vdev_release(struct video_device *vdev)
{
	dprintk(2, "%s\n", __func__);
}

static int vdin_v4l2_open(struct file *file)
{
	struct vdin_dev_s *devp = video_drvdata(file);

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	mutex_lock(&devp->fe_lock);

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		dprintk(0, "vdin%d v4l2 already opened!\n", devp->index);
		mutex_unlock(&devp->fe_lock);
		return -EBUSY;
	}

	dprintk(0, "vdin%d v4l2 open ok\n", devp->index);
	/*dump_stack();*/
	devp->afbce_flag_backup = devp->afbce_flag;

	v4l2_fh_open(file);

	INIT_LIST_HEAD(&devp->buf_list);
	mutex_unlock(&devp->fe_lock);
	return 0;
}

static int vdin_v4l2_release(struct file *file)
{
	int ret = 0, i = 0;
	int plane_idx = 0;
	struct vdin_dev_s *devp = video_drvdata(file);
	struct vdin_set_canvas_addr_s *p_addr = NULL;

	if (IS_ERR_OR_NULL(devp))
		return -EFAULT;

	if (devp->flags & VDIN_FLAG_DEC_STARTED) {
		ret |= vdin_v4l2_stop_tvin(devp);
	}

	/*release que*/
	ret |= vb2_fop_release(file);

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			for (plane_idx = 0;
				plane_idx < devp->v4l2_fmt.fmt.pix_mp.num_planes;
				plane_idx++) {
				p_addr = &devp->st_vdin_set_canvas_addr[i][plane_idx];
				if (p_addr->dma_buffer == 0)
					break;
				dma_buf_unmap_attachment(p_addr->dmabuf_attach,
					p_addr->sg_table, DMA_BIDIRECTIONAL);
				dma_buf_detach(p_addr->dma_buffer, p_addr->dmabuf_attach);
				dma_buf_put(p_addr->dma_buffer);
			}
		}
		memset(devp->st_vdin_set_canvas_addr, 0, sizeof(devp->st_vdin_set_canvas_addr));

		devp->afbce_flag = devp->afbce_flag_backup;
	}
	devp->work_mode = VDIN_WORK_MD_NORMAL;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_HDMI
	if (IS_HDMI_SRC(devp->v4l2_port_cur))
		rx_update_edid_callback(devp->v4l2_port_cur, RX_EDID_DEFAULT);
#endif
	return ret;
}

static unsigned int vdin_v4l2_poll(struct file *file,
				   struct poll_table_struct *wait)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	int ret;

	ret = vb2_fop_poll(file, wait);

	return ret;
}

static int vdin_v4l2_mmap(struct file *file, struct vm_area_struct *va)
{
	return vb2_fop_mmap(file, va);
}

static long vdin_v4l2_ioctl(struct file *file,
			    unsigned int cmd, unsigned long arg)
{
	/*struct vdin_dev_s *devp = video_drvdata(file);*/
	long ret = 0;

	ret = video_ioctl2(file, cmd, arg);

	return ret;
}

static ssize_t vdin_v4l2_read(struct file *fd, char __user *a,
			      size_t b, loff_t *c)
{
	/*vb2_fop_read(fd, a, b, c);*/
	return 0;
}

static ssize_t vdin_v4l2_write(struct file *fd, const char __user *a,
			       size_t b, loff_t *c)
{
	/*vb2_fop_write(fd, a, b, c);*/
	return 0;
}

static void vdin_return_all_buffers(struct vdin_dev_s *devp,
				    enum vb2_buffer_state state)
{
	struct vdin_vb_buff *buf, *node;
	unsigned long flags = 0;

	spin_lock_irqsave(&devp->list_head_lock, flags);
	list_for_each_entry_safe(buf, node, &devp->buf_list, list) {
		dprintk(2, "vdin return all buf id:%d\n", buf->vb.vb2_buf.index);
		vb2_buffer_done(&buf->vb.vb2_buf, state);
		list_del(&buf->list);
	}
	INIT_LIST_HEAD(&devp->buf_list);
	spin_unlock_irqrestore(&devp->list_head_lock, flags);
}

/*
 * op queue_setup
 * called from VIDIOC_REQBUFS() and VIDIOC_CREATE_BUFS() handlers
 * before memory allocation,
 * need return the num_planes per buffer
 */
static int vdin_vb2ops_queue_setup(struct vb2_queue *vq,
				   unsigned int *num_buffers,
				   unsigned int *num_planes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);
	unsigned int i = 0;

	/*
	 * NV12 every frame need two buffer
	 * one for Y, one for UV
	 * need return the num_planes per buffer
	 */
	*num_planes = devp->v4l2_fmt.fmt.pix_mp.num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
		if (devp->hw_core == VDIN_HW_CORE_LITE)
			alloc_devs[i] = &devp->this_pdev->dev;/* vdin_cma area */
		else
			alloc_devs[i] = v4l_get_dev_from_codec_mm();/* codec_mm_cma area */

		if (devp->debug.v4l2_buff_area == 1)
			alloc_devs[i] = v4l_get_dev_from_codec_mm();/* codec_mm_cma area */
		else if (devp->debug.v4l2_buff_area == 2)
			alloc_devs[i] = &devp->this_pdev->dev;/* vdin0_cma area */
		else if (devp->debug.v4l2_buff_area == 3)
			alloc_devs[i] = devp->dev;/* CMA reserved area */
	}

	dprintk(1, "vdin%d type: %d, plane: %d, buf_cnt: %d, size: [Y: %x, C: %x]\n",
		devp->index, vq->type, *num_planes, *num_buffers, sizes[0], sizes[1]);
	return 0;
}

/*
 * buf_prepare
 *
 */
static int vdin_vb2ops_buffer_prepare(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_queue *queue = NULL;
	/*uint size = 0;*/
	/*unsigned int i;*/

	if (IS_ERR_OR_NULL(devp))
		return -EINVAL;

	queue = &devp->vb_queue;
	dprintk(3, "vdin%d buf prepare idx:%d bufs:%d planes:%d q_cnt:%d, state:%s\n",
		devp->index, vb->index, queue->max_num_buffers,
		vb->num_planes, queue->queued_count,
		vb2_buf_sts_to_str(vb->state));

	//if (vb->num_planes > 1) {
	//	for (i = 0; i < vb->num_planes; i++) {
	//		size = devp->v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage;
	//		if (vb2_plane_size(vb, i) < size) {
	//			dprintk(0, "buffer too small (%lu < %u)\n",
	//				vb2_plane_size(vb, i), size);
	//			return -EINVAL;
	//		}
	//	}
	//}

	/*vb2_set_plane_payload(vb, 0, size);*/
	return 0;
}

/*
 * buf_queue
 * passes buffer vb to the driver; driver may start hardware operation
 * on this buffer; driver should give the buffer back by calling
 * vb2_buffer_done() function; it is always called after calling
 * VIDIOC_STREAMON() ioctl; might be called before
 * start_streaming callback if user pre-queued buffers before calling
 * VIDIOC_STREAMON().
 */
static void vdin_vb2ops_buffer_queue(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4lbuf = to_vb2_v4l2_buffer(vb);
	struct vdin_vb_buff *buf = to_vdin_vb_buf(v4lbuf);
	unsigned long flags = 0;
	u32 pre_state = 0;

	pre_state = buf->vb.vb2_buf.state;

	spin_lock_irqsave(&devp->list_head_lock, flags);
	list_add_tail(&buf->list, &devp->buf_list);

	spin_unlock_irqrestore(&devp->list_head_lock, flags);
	/* TODO: Update any DMA pointers if necessary */
	dprintk(3, "vb_id:%d num_buf:%d, q_cnt:%d, state:%s->%s\n",
		vb->index, devp->vb_queue.max_num_buffers,
		devp->vb_queue.queued_count,
		vb2_buf_sts_to_str(pre_state), vb2_buf_sts_to_str(buf->vb.vb2_buf.state));
}

static int vdin_v4l2_start_streaming(struct vdin_dev_s *devp)
{
	return 0;
}

/*
 * start_streaming
 * Start streaming. First check if the minimum number of buffers have been
 * queued. If not, then return -ENOBUFS and the vb2 framework will call
 * this function again the next time a buffer has been queued until enough
 * buffers are available to actually start the DMA engine.
 */
static int vdin_vb2ops_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);
	struct list_head *buf_head = NULL;
	/*struct vb2_buffer *vb2_buf;*/
	int ret = 0;

	if (IS_ERR_OR_NULL(devp))
		return -EINVAL;

	buf_head = &devp->buf_list;
	if (list_empty(buf_head)) {
		dprintk(0, "buf_list is empty\n");
		return -EINVAL;
	}

	devp->frame_cnt = 0;

	ret = vdin_v4l2_start_streaming(devp);
	if (ret) {
		/*
		 * In case of an error, return all active buffers to the
		 * QUEUED state
		 */
		vdin_return_all_buffers(devp, VB2_BUF_STATE_QUEUED);
	}
	dprintk(2, "start streaming\n");
	return ret;
}

/*
 * stop_streaming
 * Stop the DMA engine. Any remaining buffers in the DMA queue are dequeued
 * and passed on to the vb2 framework marked as STATE_ERROR.
 */
static void vdin_vb2ops_stop_streaming(struct vb2_queue *vq)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vq);

	dprintk(2, "stop streaming\n");
	/* TODO: stop DMA */

	devp->vb_queue.streaming = false;

	/* Release all active buffers */
	vdin_return_all_buffers(devp, VB2_BUF_STATE_ERROR);
}

/*
 * buf_init
 * called once after allocating a buffer (in MMAP case) or after acquiring a
 * new USERPTR buffer; drivers may perform additional buffer-related
 * initialization; initialization failure (return != 0)
 * will prevent queue setup from completing successfully; optional.
 */
static int vdin_vb2ops_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vb2q = vb->vb2_queue;
	struct vb2_v4l2_buffer *vb_buf = NULL;
	struct vdin_vb_buff *vdin_buf = NULL;

	dprintk(1, "vb id:%d, type:0x%x, mem:0x%x, num:%d\n",
		vb->index, vb->type, vb->memory, vb->num_planes);
	dprintk(1, "vb2q type:0x%x num buffer:%d q_cnt:%d\n", vb2q->type,
		vb2q->max_num_buffers, vb2q->queued_count);

	vb_buf = to_vb2_v4l2_buffer(vb);
	vdin_buf = to_vdin_vb_buf(vb_buf);

	/* for test add a tag*/
	vdin_buf->tag = 0xa5a6ff00 + vb->index;
	/* set a tag for test*/
	dprintk(2, "vb_buf:0x%p vdin_buf:0x%p tag:0x%x\n",
		vb_buf, vdin_buf, vdin_buf->tag);

	return 0;
}

/*
 * buf_finish
 * called before every dequeue of the buffer back to userspace; the
 * buffer is synced for the CPU, so drivers can access/modify the
 * buffer contents; drivers may perform any operations required
 * before userspace accesses the buffer; optional. The buffer state
 * can be one of the following: DONE and ERROR occur while
 * streaming is in progress, and the PREPARED state occurs when
 * the queue has been canceled and all pending buffers are being
 * returned to their default DEQUEUED state. Typically you only
 * have to do something if the state is VB2_BUF_STATE_DONE,
 * since in all other cases the buffer contents will be ignored anyway.
 */
static void vdin_vb2ops_buf_finish(struct vb2_buffer *vb)
{
	struct vdin_dev_s *devp = vb2_get_drv_priv(vb->vb2_queue);
	//struct vb2_queue *vb_que = NULL;

	if (IS_ERR_OR_NULL(devp))
		return;
	//vb_que = &devp->vb_queue;

	/*dprintk("%s idx:%d, queue cnt:%d, buf:%s\n", __func__, vb->index,*/
	/*	  vb_que->queued_count, vb2_buf_sts_to_str(vb->state));*/
}

/*
 * The vb2 queue ops. Note that since q->lock is set we can use the standard
 * vb2_ops_wait_prepare/finish helper functions. If q->lock would be NULL,
 * then this driver would have to provide these ops.
 */
static struct vb2_ops vdin_vb2ops = {
	.queue_setup		= vdin_vb2ops_queue_setup,
	.buf_prepare		= vdin_vb2ops_buffer_prepare,
	.buf_queue			= vdin_vb2ops_buffer_queue,

	.buf_init			= vdin_vb2ops_buf_init,
	.buf_finish			= vdin_vb2ops_buf_finish,

	.start_streaming	= vdin_vb2ops_start_streaming,
	.stop_streaming		= vdin_vb2ops_stop_streaming,

	.wait_prepare		= vb2_ops_wait_prepare,
	.wait_finish		= vb2_ops_wait_finish,
};

static const struct v4l2_file_operations vdin_v4l2_fops = {
	.owner = THIS_MODULE,
	.open = vdin_v4l2_open, /*open files */
	.read = vdin_v4l2_read,
	.write = vdin_v4l2_write,
	.release = vdin_v4l2_release, /*release files resource*/
	.poll = vdin_v4l2_poll, /*files poll interface*/
	.mmap = vdin_v4l2_mmap, /*files memory mmap*/
	.unlocked_ioctl = vdin_v4l2_ioctl, /*io control op interface*/
};

static int vdin_v4l2_queue_init(struct vdin_dev_s *devp,
				struct vb2_queue *que)
{
	int ret;

	/*unsigned int ret = 0;*/
	que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	/*que->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;*/
	que->io_modes = VB2_MMAP | VB2_DMABUF;
	/*devp->dev*/
	//que->dev = &devp->vdev.dev;
	que->dev = &devp->this_pdev->dev;
	que->drv_priv = devp;
	que->buf_struct_size = sizeof(struct vdin_vb_buff);
	que->ops = &vdin_vb2ops;

	/*que->mem_ops = &vdin_vb2_dma_contig_memops;*/
	que->mem_ops = &vb2_dma_contig_memops;
	/*que->mem_ops = &vb2_vmalloc_memops;*/
	/*que->mem_ops = &vb2_dma_sg_memops;*/

	que->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	/*
	 * Assume that this DMA engine needs to have at least two buffers
	 * available before it can be started. The start_streaming() op
	 * won't be called until at least this many buffers are queued up.
	 */
	que->min_queued_buffers = 3;
	/*
	 * The serialization lock for the streaming ioctls. This is the same
	 * as the main serialization lock, but if some of the non-streaming
	 * ioctls could take a long time to execute, then you might want to
	 * have a different lock here to prevent VIDIOC_DQBUF from being
	 * blocked while waiting for another action to finish. This is
	 * generally not needed for PCI devices, but USB devices usually do
	 * want a separate lock here.
	 */
	que->lock = &devp->lock;
	/*
	 * Since this driver can only do 32-bit DMA we must make sure that
	 * the vb2 core will allocate the buffers in 32-bit DMA memory.
	 */
	que->gfp_flags = GFP_KERNEL;

	ret = vb2_queue_init(que);
	if (ret < 0)
		dprintk(0, "vb2_queue_init fail\n");

	dprintk(1, "vdin%d v4l2 init type:%d, m:%d min_buf:%d multi:%d\n",
		devp->index, que->type, que->io_modes,
		que->min_queued_buffers, que->is_multiplanar);

	return ret;
}

/*
 * vdin v4l2 video device register and device node create
 */
int vdin_v4l2_probe(struct platform_device *pl_dev,
		    struct vdin_dev_s *devp)
{
	struct video_device *video_dev = NULL;
	//struct vb2_queue *queue = NULL;
	int ret = 0;
	int v4l_vd_num;

	if (IS_ERR_OR_NULL(devp)) {
		ret = -EINVAL;
		goto error;
	}

	devp->dts_config.v4l_en = of_property_read_bool(pl_dev->dev.of_node,
						"v4l_support_en");
	if (devp->dts_config.v4l_en == 0) {
		ret = -1;
		goto error;
	}
	/*dprintk(1, "%s vdin[%d] start\n", __func__, devp->index);*/
	snprintf(devp->v4l2_dev.name, sizeof(devp->v4l2_dev.name),
		 "%s", VDIN_V4L_DV_NAME);

	/*video device initial*/
	video_dev = &devp->video_dev;
	video_dev->v4l2_dev = &devp->v4l2_dev;
	/*dprintk(1, "video_device addr:0x%p\n", video_dev);*/
	/* Initialize the top-level structure */
	ret = v4l2_device_register(&pl_dev->dev, &devp->v4l2_dev);
	if (ret) {
		ret = -ENODEV;
		goto error;
	}

	/* Initialize the vb2 queue */
	//queue = &devp->vb_queue;
	ret = vdin_v4l2_queue_init(devp, &devp->vb_queue);
	if (ret) {
		ret = -2;
		goto error;
	}
	INIT_LIST_HEAD(&devp->buf_list);
	mutex_init(&devp->lock);
	spin_lock_init(&devp->list_head_lock);

	strncpy(video_dev->name, VDIN_V4L_DV_NAME, sizeof(video_dev->name));
	video_dev->fops = &vdin_v4l2_fops,
	video_dev->ioctl_ops = &vdin_v4l2_ioctl_ops,
	video_dev->release = vdin_vdev_release;
	video_dev->lock = &devp->lock;
	video_dev->queue = &devp->vb_queue;
	video_dev->v4l2_dev = &devp->v4l2_dev;/*v4l2_device_register*/
	video_dev->device_caps = VDIN_DEVICE_CAPS;

	video_dev->vfl_type = VFL_TYPE_VIDEO;
	video_dev->vfl_dir   = VFL_DIR_RX;
	video_dev->dev_debug = (V4L2_DEV_DEBUG_IOCTL | V4L2_DEV_DEBUG_IOCTL_ARG);

	/*
	 * init the device node name, v4l2 will create
	 * a video device. the name is:videoXX (VDIN_V4L_DV_NAME)
	 * otherwise will be a videoXX, XX is a number
	 */
	/*video_dev->dev.init_name = VDIN_V4L_DV_NAME;*/
	video_set_drvdata(video_dev, devp);

	ret = of_property_read_u32(pl_dev->dev.of_node, "v4l_vd_num",
			&v4l_vd_num);
	if (ret)
		v4l_vd_num = VDIN_VD_NUMBER + (devp->index);

	ret = video_register_device(video_dev, VFL_TYPE_VIDEO,
			v4l_vd_num);
	if (ret) {
		ret = -4;
		goto error;
	}

	pl_dev->dev.dma_mask = &pl_dev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&pl_dev->dev, DMA_BIT_MASK(36)) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&pl_dev->dev, DMA_BIT_MASK(36)) < 0)
		dprintk(0, "set dma mask fail\n");

	video_dev->dev.dma_mask = &video_dev->dev.coherent_dma_mask;
	if (dma_set_coherent_mask(&video_dev->dev, DMA_BIT_MASK(36)) < 0)
		dprintk(0, "dev set_coherent_mask fail\n");

	if (dma_set_mask(&video_dev->dev, DMA_BIT_MASK(36)) < 0)
		dprintk(0, "set dma mask fail\n");

	/*device node need attach device tree vdin dts tree*/
	video_dev->dev.of_node = pl_dev->dev.of_node;
	if (!(devp->cma_config_flag & MEM_ALLOC_FROM_CODEC)) {
		ret = of_reserved_mem_device_init(&video_dev->dev);
		if (ret)
			pr_debug("vdin%d v4l2 reserved_mem not defined\n", devp->index);
	}

	vdin_v4l2_init(devp, pl_dev);

	dprintk(0, "vdin[%d] probe ok, node:%s (/dev/video%d)\n",
		devp->index, video_device_node_name(video_dev), v4l_vd_num);

	return 0;
error:
	dprintk(0, "v4l2 probe error:%d\n", ret);
	return ret;
}

/* Check loopback source format */
int vdin_v4l2_loopback_fmt(struct vdin_dev_s *devp)
{
	int h, v;
	int err_range = 10;
	const struct vinfo_s *vinfo;

#ifdef VDIN_V4L2_GET_LOOPBACK_FMT_BY_VOUT_SERVE
	vinfo = get_current_vinfo();
	h = vinfo->width;
	v = vinfo->height;
#else
	h = vdin_get_active_h(devp);
	v = vdin_get_active_v(devp);
#endif
	if (h >= 4096 - err_range && h <= 4096 + err_range &&
		v >= 2160 - err_range && v <= 2160 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_4096_2160_00HZ;
	else if (h >= 3840 - err_range && h <= 3840 + err_range &&
			 v >= 2160 - err_range && v <= 2160 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_3840_2160_00HZ;
	else if (h >= 1920 - err_range && h <= 1920 + err_range &&
			 v >= 1080 - err_range && v <= 1080 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_1920X1080P_60HZ;
	else if (h >= 1280 - err_range && h <= 1280 + err_range &&
			 v >= 720 - err_range && v <= 720 + err_range)
		devp->parm.info.fmt = TVIN_SIG_FMT_HDMI_1280X720P_60HZ;
	else
		devp->parm.info.fmt = TVIN_SIG_FMT_NULL;

	dprintk(1, "vdin%d, active:%dx%d %dHz, fmt:0x%x, status:%d\n",
		devp->index, h, v, devp->parm.info.fps,
		devp->parm.info.fmt, devp->parm.info.status);

	return 0;
}

/* start vdin dec for v4l2 */
int vdin_v4l2_start_tvin(struct vdin_dev_s *devp)
{
	int ret;
	struct vdin_parm_s vdin_cap_param;
	const struct tvin_format_s *fmt_info_p;
	struct video_input_info video_input_parms;
	int cur_axis[4];
	const struct vinfo_s *vinfo;
	bool from_vinfo = 0;

	memset(&vdin_cap_param, 0, sizeof(struct vdin_parm_s));
	memset(&cur_axis, 0, sizeof(cur_axis));
	memset(&video_input_parms, 0, sizeof(struct video_input_info));

	if (devp->hw_core == VDIN_HW_CORE_LITE) {
		if (vdin_get_video_ready_state(devp->v4l2_port_cur)) {
			if (devp->v4l2_port_cur == TVIN_PORT_VIU1_WB0_VD1) {
				get_vdx_real_axis(0, cur_axis);
				vdin_cap_param.h_active = cur_axis[2] - cur_axis[0] + 1;
				vdin_cap_param.v_active = cur_axis[3] - cur_axis[1] + 1;
			} else if (devp->v4l2_port_cur == TVIN_PORT_VIU2_VD1) {
				get_vdx_real_axis(1, cur_axis);
				vdin_cap_param.h_active = cur_axis[2] - cur_axis[0] + 1;
				vdin_cap_param.v_active = cur_axis[3] - cur_axis[1] + 1;
			} else if (devp->v4l2_port_cur == TVIN_PORT_VIU1_VIDEO) {
				get_video_input_info(&video_input_parms);
				vdin_cap_param.h_active = video_input_parms.width;
				vdin_cap_param.v_active = video_input_parms.height;
			}
			devp->ext_cap_video_win_info.in.w = vdin_cap_param.h_active;
			devp->ext_cap_video_win_info.in.h = vdin_cap_param.v_active;
		} else {
			vinfo = get_current_vinfo();
			vdin_cap_param.h_active = vinfo->width;
			vdin_cap_param.v_active = vinfo->height;
			from_vinfo = true;
		}

		vdin_cap_param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		vdin_cap_param.cfmt = TVIN_YUV422;
		vdin_cap_param.fmt = TVIN_SIG_FMT_MAX;

		vdin_cap_param.hsync_phase = 0;
		vdin_cap_param.vsync_phase = 1;
		vdin_cap_param.hs_bp = 0;
		vdin_cap_param.vs_bp = 0;
	} else {
		fmt_info_p = tvin_get_fmt_info(devp->parm.info.fmt);
		if (!fmt_info_p) {
			dprintk(0, "fmt:%s set hv_active:%dx%d\n",
				tvin_sig_fmt_str(devp->parm.info.fmt),
				devp->v4l2_fmt.fmt.pix_mp.width,
				devp->v4l2_fmt.fmt.pix_mp.height);
			vdin_cap_param.h_active = devp->v4l2_fmt.fmt.pix_mp.width;
			vdin_cap_param.v_active = devp->v4l2_fmt.fmt.pix_mp.height;
			vdin_cap_param.fmt = TVIN_SIG_FMT_MAX;
			vdin_cap_param.hsync_phase = 0;
			vdin_cap_param.vsync_phase = 1;
			vdin_cap_param.hs_bp = 0;
			vdin_cap_param.vs_bp = 0;
			vdin_cap_param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		} else {
			vdin_cap_param.h_active = fmt_info_p->h_active;
			vdin_cap_param.v_active = fmt_info_p->v_active;
			vdin_cap_param.fmt = devp->parm.info.fmt;
			vdin_cap_param.scan_mode = fmt_info_p->scan_mode;
		}
		vdin_cap_param.cfmt  = devp->parm.info.cfmt;
	}

	vdin_cap_param.frame_rate   = devp->parm.info.fps;
	vdin_cap_param.port         = devp->v4l2_port_cur;
	vdin_cap_param.dest_h_active = devp->v4l2_fmt.fmt.pix_mp.width;
	vdin_cap_param.dest_v_active = devp->v4l2_fmt.fmt.pix_mp.height;
	//vdin_cap_param.reserved    |= PARAM_STATE_SCREEN_CAP;

	if (vdin_cap_param.frame_rate == 0)
		vdin_cap_param.frame_rate = 60;

	if (devp->parm.port == TVIN_PORT_MIPI) {
		//vdin_cap_param.frame_rate = 30;
		vdin_cap_param.cfmt = TVIN_YUV444;
		//vdin_cap_param.bt_path = BT_PATH_CSI2;
		vdin_cap_param.hsync_phase = 1;
		vdin_cap_param.vsync_phase = 1;
	}

	/* vdin can not do scale up */
	if (vdin_cap_param.dest_h_active > vdin_cap_param.h_active)
		vdin_cap_param.dest_h_active = vdin_cap_param.h_active;
	if (vdin_cap_param.dest_v_active > vdin_cap_param.v_active)
		vdin_cap_param.dest_v_active = vdin_cap_param.v_active;

	devp->ext_cap_video_win_info.out.w = vdin_cap_param.dest_h_active;
	devp->ext_cap_video_win_info.out.h = vdin_cap_param.dest_v_active;

	if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21 ||
		devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV21M)
		vdin_cap_param.dfmt = TVIN_NV21;
	else if (devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12 ||
		devp->v4l2_fmt.fmt.pix_mp.pixelformat == V4L2_PIX_FMT_NV12M)
		vdin_cap_param.dfmt = TVIN_NV12;
	else
		vdin_cap_param.dfmt = TVIN_YUV422;

	if (vdin_cap_param.h_active && vdin_cap_param.v_active &&
		(vdin_cap_param.h_active > vdin_cap_param.dest_h_active ||
		vdin_cap_param.v_active > vdin_cap_param.dest_v_active)) {
		devp->prop.scaling4w = vdin_cap_param.dest_h_active;
		devp->prop.scaling4h = vdin_cap_param.dest_v_active;
	}

	dprintk(1, "start port:0x%x,fmt:%d %d %d;active:%dx%d->%dx%d %dHz,sm:%d,vinfo:%d\n",
		vdin_cap_param.port,
		vdin_cap_param.cfmt, vdin_cap_param.fmt, vdin_cap_param.dfmt,
		vdin_cap_param.h_active, vdin_cap_param.v_active,
		vdin_cap_param.dest_h_active, vdin_cap_param.dest_v_active,
		vdin_cap_param.frame_rate, vdin_cap_param.scan_mode, from_vinfo);

	ret = start_tvin_service(devp->index, &vdin_cap_param);
	return ret;
}

int vdin_v4l2_stop_tvin(struct vdin_dev_s *devp)
{
	int ret;

	ret = stop_tvin_service(devp->index);

	mutex_lock(&devp->fe_lock);
	vdin_close_fe(devp);
	mutex_unlock(&devp->fe_lock);

	dprintk(0, "vdin v4l2 stop tvin:%d\n", ret);

	return ret;
}

void vdin_v4l2_init(struct vdin_dev_s *devp, struct platform_device *pl_dev)
{
	int fe_ports_num;
	int i, ret;
	u32 tmp_u32 = 0;
	const char *str = NULL;

	memset(devp->v4l2_port, TVIN_PORT_NULL, sizeof(devp->v4l2_port));

	fe_ports_num =
		of_property_count_u32_elems(pl_dev->dev.of_node, "fe_ports");
	if (fe_ports_num < 0) {
		if (devp->index == 0) {
			devp->v4l2_port[0] = TVIN_PORT_HDMI0;
			devp->v4l2_port[1] = TVIN_PORT_HDMI1;
			devp->v4l2_port[2] = TVIN_PORT_HDMI2;
			devp->v4l2_port[3] = TVIN_PORT_CVBS0;
			devp->v4l2_port_num = 4;
		} else {
			devp->v4l2_port[0] = TVIN_PORT_VIU1_WB0_VD1;
			devp->v4l2_port[1] = TVIN_PORT_VIU1_WB0_VD2;
			devp->v4l2_port[2] = TVIN_PORT_VIU1_WB0_OSD1;
			devp->v4l2_port[3] = TVIN_PORT_VIU1_WB0_OSD2;
			devp->v4l2_port[4] = TVIN_PORT_VIU1_WB0_VPP;
			//devp->v4l2_port[5] = TVIN_PORT_VIU1_WB0_VDIN_BIST;
			devp->v4l2_port[5] = TVIN_PORT_VIU1_WB0_POST_BLEND;
			devp->v4l2_port[6] = TVIN_PORT_VIU1_VIDEO;
			devp->v4l2_port_num = 7;
		}
	} else {
		devp->v4l2_port_num = 0;
	}

	for (i = 0; i < fe_ports_num; i++) {
		ret = of_property_read_u32_index(pl_dev->dev.of_node, "fe_ports", i,
				&tmp_u32);
		if (ret || tmp_u32 <= TVIN_PORT_NULL || tmp_u32 >= TVIN_PORT_MAX) {
			continue;
		}
		devp->v4l2_port[devp->v4l2_port_num] = tmp_u32;
		devp->v4l2_port_num++;
	}
	/* default port */
	devp->v4l2_port_cur = TVIN_PORT_MAX;//devp->v4l2_port[0];

	ret = of_property_read_string(pl_dev->dev.of_node, "driver", &str);
	if (ret == 0) {
		strscpy(g_vdin_v4l2_cap[devp->index].driver, str,
			sizeof(g_vdin_v4l2_cap[devp->index].driver));
	}

	ret = of_property_read_string(pl_dev->dev.of_node, "card", &str);
	if (ret == 0) {
		strscpy(g_vdin_v4l2_cap[devp->index].card, str,
			sizeof(g_vdin_v4l2_cap[devp->index].card));
	}

	ret = of_property_read_string(pl_dev->dev.of_node, "bus_info", &str);
	if (ret == 0) {
		strscpy(g_vdin_v4l2_cap[devp->index].bus_info, str,
			sizeof(g_vdin_v4l2_cap[devp->index].bus_info));
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "version", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].version = tmp_u32;
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "capabilities", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].capabilities = tmp_u32;
	}

	ret = of_property_read_u32(pl_dev->dev.of_node, "device_caps", &tmp_u32);
	if (ret == 0) {
		g_vdin_v4l2_cap[devp->index].device_caps = tmp_u32;
	}

	dprintk(4, "vdin%d driver:%s, card:%s, bus:%s, ver:%#x, caps:%#x %#x\n",
		devp->index,
		g_vdin_v4l2_cap[devp->index].driver,
		g_vdin_v4l2_cap[devp->index].card,
		g_vdin_v4l2_cap[devp->index].bus_info,
		g_vdin_v4l2_cap[devp->index].version,
		g_vdin_v4l2_cap[devp->index].capabilities,
		g_vdin_v4l2_cap[devp->index].device_caps);
	/* fill struct v4l2_ext_capture_capability_info with default value */
	devp->ext_cap_cap_info.flags =
		V4L2_EXT_CAPTURE_CAP_SCALE_DOWN |
		V4L2_EXT_CAPTURE_CAP_DIVIDE_FRAMERATE |
		V4L2_EXT_CAPTURE_CAP_INPUT_VIDEO_DEINTERLACE |
		V4L2_EXT_CAPTURE_CAP_DISPLAY_VIDEO_DEINTERLACE;
	devp->ext_cap_cap_info.scale_up_limit_w = 3840;
	devp->ext_cap_cap_info.scale_up_limit_h = 2160;
	devp->ext_cap_cap_info.scale_down_limit_w = 360;
	devp->ext_cap_cap_info.scale_down_limit_h = 240;
	/* LGE chips only */
	devp->ext_cap_cap_info.num_video_frame_buffer = 5;
	devp->ext_cap_cap_info.max_res.x = 0;
	devp->ext_cap_cap_info.max_res.y = 0;
	devp->ext_cap_cap_info.max_res.w = 3840;
	devp->ext_cap_cap_info.max_res.h = 2160;
	/* LGE chips only end */
	devp->ext_cap_cap_info.num_plane =
		V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PLANE_INTERLEAVED;
	devp->ext_cap_cap_info.pixel_format =
		V4L2_EXT_CAPTURE_VIDEO_FRAME_BUFFER_PIXEL_FORMAT_YUV422_INTERLEAVED;
	/* fill struct v4l2_ext_capture_plane_info with default value */
	devp->ext_cap_plane_info.stride = 1920;
	devp->ext_cap_plane_info.plane_region.x = 0;
	devp->ext_cap_plane_info.plane_region.y = 0;
	devp->ext_cap_plane_info.plane_region.w = 1920;
	devp->ext_cap_plane_info.plane_region.h = 1080;
	devp->ext_cap_plane_info.active_region.x = 0;
	devp->ext_cap_plane_info.active_region.y = 0;
	devp->ext_cap_plane_info.active_region.w = 1920;
	devp->ext_cap_plane_info.active_region.h = 1080;

	/* fill struct v4l2_ext_capture_freeze_mode with default value */
	devp->ext_cap_freezee_mode.plane_index = 0;
	devp->ext_cap_freezee_mode.val = 0;

	/* fill struct v4l2_ext_capture_plane_prop with default value */
	devp->ext_cap_plane_prop.l = V4L2_EXT_CAPTURE_BLENDED_OUTPUT;

	devp->vdin_v4l2.divide = 1;

	dprintk(4, "vdin%d v4l2 init num::%d l:%d pix_fmt:0x%x, p:0x%x flag:%x0x\n",
		devp->index, devp->v4l2_port_num,
		devp->ext_cap_plane_prop.l, devp->ext_cap_cap_info.pixel_format,
		devp->ext_cap_cap_info.num_plane, devp->ext_cap_cap_info.flags);
}
