// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "dpss_process_util.h"
#include "ge2d_process.h"

#define WIDTH_8K 7680
#define HEIGHT_8K 7680
#define IS_DI_PSTLINK(di_flag) ((di_flag) & DI_FLAG_DI_PSTVPPLINK)

static u32 dump_flag;

#ifndef CONFIG_AMLOGIC_MEDIA_GE2D
void stretchblt_noalpha(struct ge2d_context_s *wq,
			int src_x, int src_y, int src_w, int src_h,
			int dst_x, int dst_y, int dst_w, int dst_h)
{
}

int  destroy_ge2d_work_queue(struct ge2d_context_s *ge2d_work_queue)
{
	return 0;
}

void fillrect(struct ge2d_context_s *wq, int x, int y, int w, int h, unsigned int color)
{
}
#endif

#ifndef CONFIG_AMLOGIC_UVM_CORE
struct vframe_s *dmabuf_get_vframe(struct dma_buf *dmabuf)
{
	return NULL;
}
#endif

static int get_source_type(struct src_data_para *src_data)
{
	enum videocom_source_type ret;
	int interlace_mode;

	interlace_mode = src_data->type & VIDTYPE_TYPEMASK;
	if (src_data->source_type == VFRAME_SOURCE_TYPE_HDMI ||
	    src_data->source_type == VFRAME_SOURCE_TYPE_CVBS ||
	    src_data->source_type == VFRAME_SOURCE_TYPE_TUNER) {
		if ((src_data->bitdepth & BITDEPTH_Y10)  &&
		    (!(src_data->type & VIDTYPE_COMPRESS)) &&
		    (get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL))
			ret = VDIN_10BIT_NORMAL;
		else
			ret = VDIN_8BIT_NORMAL;
	} else {
		if ((src_data->bitdepth & BITDEPTH_Y10)  &&
			(!(src_data->type & VIDTYPE_COMPRESS)) &&
			(get_cpu_type() >= MESON_CPU_MAJOR_ID_TXL)) {
			if (src_data->type & VIDTYPE_VIU_FIELD)
				ret = DECODER_10BIT_NORMAL;
			else if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_10BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_10BIT_BOTTOM;
			else
				ret = DECODER_10BIT_NORMAL;
		} else {
			if (src_data->type & VIDTYPE_VIU_FIELD)
				ret = DECODER_8BIT_NORMAL;
			else if (interlace_mode == VIDTYPE_INTERLACE_TOP)
				ret = DECODER_8BIT_TOP;
			else if (interlace_mode == VIDTYPE_INTERLACE_BOTTOM)
				ret = DECODER_8BIT_BOTTOM;
			else
				ret = DECODER_8BIT_NORMAL;
		}
	}
	return ret;
}

static int get_input_format(struct src_data_para *src_data)
{
	int format = GE2D_FORMAT_M24_YUV420;
	enum videocom_source_type soure_type;

	soure_type = get_source_type(src_data);
	switch (soure_type) {
	case DECODER_8BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422;
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21;
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12;
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444;
		else
			format = GE2D_FORMAT_M24_YUV420;
		break;
	case DECODER_8BIT_BOTTOM:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12B & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444B & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420B & (3 << 3));
		break;
	case DECODER_8BIT_TOP:
		if (src_data->type & VIDTYPE_VIU_422)
			format = GE2D_FORMAT_S16_YUV422
				| (GE2D_FORMAT_S16_YUV422T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV21)
			format = GE2D_FORMAT_M24_NV21
				| (GE2D_FORMAT_M24_NV21T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_NV12)
			format = GE2D_FORMAT_M24_NV12
				| (GE2D_FORMAT_M24_NV12T & (3 << 3));
		else if (src_data->type & VIDTYPE_VIU_444)
			format = GE2D_FORMAT_S24_YUV444
				| (GE2D_FORMAT_S24_YUV444T & (3 << 3));
		else
			format = GE2D_FORMAT_M24_YUV420
				| (GE2D_FMT_M24_YUV420T & (3 << 3));
		break;
	case DECODER_10BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		}
		break;
	case DECODER_10BIT_BOTTOM:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
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
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
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
		if (src_data->type & VIDTYPE_VIU_422) {
			format = GE2D_FORMAT_S16_YUV422;
		} else if (src_data->type & VIDTYPE_VIU_NV21) {
			format = GE2D_FORMAT_M24_NV21;
		} else if (src_data->type & VIDTYPE_VIU_NV12) {
			format = GE2D_FORMAT_M24_NV12;
		} else if (src_data->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_YUV444;
		} else if (src_data->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_RGB;
			if (src_data->is_vframe)
				format &= (~GE2D_LITTLE_ENDIAN);
		} else {
			format = GE2D_FORMAT_M24_YUV420;
		}
		break;
	case VDIN_10BIT_NORMAL:
		if (src_data->type & VIDTYPE_VIU_422) {
			if (src_data->bitdepth & FULL_PACK_422_MODE)
				format = GE2D_FORMAT_S16_10BIT_YUV422;
			else
				format = GE2D_FORMAT_S16_12BIT_YUV422;
		} else if (src_data->type & VIDTYPE_VIU_444) {
			format = GE2D_FORMAT_S24_10BIT_YUV444;
		} else if (src_data->type & VIDTYPE_RGB_444) {
			format = GE2D_FORMAT_S24_10BIT_RGB888;
			if (!src_data->is_vframe)
				format |= GE2D_LITTLE_ENDIAN;
		}
		break;
	default:
		format = GE2D_FORMAT_M24_YUV420;
	}
	return format;
}

static enum ge2d_angle_type config_ge2d_rotation(int dev_index, int vf_transform)
{
	int ge2d_angle = 0;

	if (vf_transform == 1)
		ge2d_angle = GE2D_ANGLE_TYPE_FLIP_H;
	else if (vf_transform == 2)
		ge2d_angle = GE2D_ANGLE_TYPE_FLIP_V;
	else if (vf_transform == 4 || vf_transform == 5 || vf_transform == 6)
		ge2d_angle = GE2D_ANGLE_TYPE_ROT_90;
	else if (vf_transform == 3)
		ge2d_angle = GE2D_ANGLE_TYPE_ROT_180;
	else if (vf_transform == 7)
		ge2d_angle = GE2D_ANGLE_TYPE_ROT_270;
	else if (vf_transform != 0)
		dp_print(dev_index, PRINT_ERROR, "%s: not support transform=%d\n",
			__func__, vf_transform);

	return ge2d_angle;
}

static int alloc_src_canvas(int dev_index, struct ge2d_composer_para *ge2d_comp_para)
{
	const char *keep_owner = "dpss_rotate";

	if (ge2d_comp_para->canvas_scr[0] < 0)
		ge2d_comp_para->canvas_scr[0] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (ge2d_comp_para->canvas_scr[1] < 0)
		ge2d_comp_para->canvas_scr[1] =
			canvas_pool_map_alloc_canvas(keep_owner);
	if (ge2d_comp_para->canvas_scr[2] < 0)
		ge2d_comp_para->canvas_scr[2] =
			canvas_pool_map_alloc_canvas(keep_owner);

	if (ge2d_comp_para->canvas_scr[0] < 0 ||
	    ge2d_comp_para->canvas_scr[1] < 0 ||
	    ge2d_comp_para->canvas_scr[2] < 0) {
		dp_print(dev_index, PRINT_ERROR, "%s failed\n", __func__);
		return -1;
	}
	return 0;
}

static int free_src_canvas(int dev_index, struct ge2d_composer_para *ge2d_comp_para)
{
	if (ge2d_comp_para->canvas_scr[0] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[0]);
		ge2d_comp_para->canvas_scr[0] = -1;
	}

	if (ge2d_comp_para->canvas_scr[1] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[1]);
		ge2d_comp_para->canvas_scr[1] = -1;
	}

	if (ge2d_comp_para->canvas_scr[2] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_scr[2]);
		ge2d_comp_para->canvas_scr[2] = -1;
	}
	if (ge2d_comp_para->canvas_scr[0] >= 0 ||
	    ge2d_comp_para->canvas_scr[1] >= 0 ||
	    ge2d_comp_para->canvas_scr[2] >= 0) {
		dp_print(dev_index, PRINT_ERROR, "%s failed!\n", __func__);
		return -1;
	}
	return 0;
}

int init_ge2d(int dev_index, struct ge2d_composer_para *ge2d_comp_para)
{
	const char *keep_owner = "dpss_rotate";

	ge2d_comp_para->context = create_ge2d_work_queue();
	if (IS_ERR_OR_NULL(ge2d_comp_para->context)) {
		dp_print(dev_index, PRINT_ERROR, "creat ge2d work failed\n");
		goto exit;
	}

	if (ge2d_comp_para->plane_num > 0) {
		if (ge2d_comp_para->canvas_dst[0] < 0)
			ge2d_comp_para->canvas_dst[0] = canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[0] < 0)
			goto exit;
	}

	if (ge2d_comp_para->plane_num > 1) {
		if (ge2d_comp_para->canvas_dst[1] < 0)
			ge2d_comp_para->canvas_dst[1] = canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[1] < 0)
			goto exit;
	}

	if (ge2d_comp_para->plane_num > 2) {
		if (ge2d_comp_para->canvas_dst[2] < 0)
			ge2d_comp_para->canvas_dst[2] = canvas_pool_map_alloc_canvas(keep_owner);
		if (ge2d_comp_para->canvas_dst[2] < 0)
			goto exit;
	}

	return 0;

exit:
	dp_print(dev_index, PRINT_ERROR, "%s: dst canvas failed!\n", __func__);
	return -1;
}

int uninit_ge2d(int dev_index, struct ge2d_composer_para *ge2d_comp_para)
{
	destroy_ge2d_work_queue(ge2d_comp_para->context);
	ge2d_comp_para->context = NULL;

	if (free_src_canvas(dev_index, ge2d_comp_para) < 0) {
		dp_print(dev_index, PRINT_ERROR, "free src canvas failed!\n");
		return -1;
	}

	if (ge2d_comp_para->canvas_dst[0] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[0]);
		ge2d_comp_para->canvas_dst[0] = -1;
	}

	if (ge2d_comp_para->canvas_dst[1] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[1]);
		ge2d_comp_para->canvas_dst[1] = -1;
	}

	if (ge2d_comp_para->canvas_dst[2] >= 0) {
		canvas_pool_map_free_canvas(ge2d_comp_para->canvas_dst[2]);
		ge2d_comp_para->canvas_dst[2] = -1;
	}

	if (ge2d_comp_para->canvas_dst[0] >= 0 ||
	    ge2d_comp_para->canvas_dst[1] >= 0 ||
	    ge2d_comp_para->canvas_dst[2] >= 0) {
		dp_print(dev_index, PRINT_ERROR, "free dst canvas failed!\n");
		return -1;
	}
	return 0;
}

static void dump_data(int dev_index, struct dump_param *para, enum buffer_data type)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	loff_t pos;

	if (!para) {
		dp_print(dev_index, PRINT_ERROR, "%s: null param.\n", __func__);
		return;
	}

	if (type == BLACK_BUFFER)
		sprintf(name_buf, "/data/black_vframe.yuv");
	else if (type == SCR_BUFFER)
		sprintf(name_buf, "/data/scr_vframe.yuv");
	else if (type == DST_BUFFER_DATA)
		sprintf(name_buf, "/data/dst_vframe.yuv");
	else
		return;

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = para->canvas0_config[0].width * para->canvas0_config[0].height;
	data_size_uv = para->canvas0_config[1].width * para->canvas0_config[1].height;
	data_y = codec_mm_vmap(para->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(para->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		dp_print(dev_index, PRINT_ERROR, "dump: vmap failed.\n");
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	dp_print(dev_index, PRINT_ERROR, "dump: write %u size to addr%p.\n", data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	dp_print(dev_index, PRINT_ERROR, "dump: write %u size to addr%p\n", data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

int ge2d_fill_black(int dev_index, struct ge2d_composer_para *ge2d_comp_para)
{
	struct canvas_config_s dst_canvas0_config[3];
	struct config_para_ex_s ge2d_config;
	u32 dst_plane_num;
	struct dump_param para;
	int temp = 0;
	int canvas_index = 0;

	memset(dst_canvas0_config, 0, sizeof(dst_canvas0_config));
	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));

	if (ge2d_comp_para->format == GE2D_FORMAT_S24_YUV444) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w * 3;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
	} else if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
		dst_canvas0_config[1].phy_addr = ge2d_comp_para->phy_addr[0]
			+ ge2d_comp_para->buffer_w * ge2d_comp_para->buffer_h;
		dst_canvas0_config[1].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[1].height = ge2d_comp_para->buffer_h >> 1;
		dst_canvas0_config[1].block_mode = 0;
		dst_canvas0_config[1].endian = 0;
	}
	dst_plane_num = ge2d_comp_para->plane_num;

	if (ge2d_comp_para->canvas0_addr == (u32)-1) {
		canvas_config_config(ge2d_comp_para->canvas_dst[0], &dst_canvas0_config[0]);
		if (dst_plane_num == 2)
			canvas_config_config(ge2d_comp_para->canvas_dst[1], &dst_canvas0_config[1]);
		else if (dst_plane_num == 3)
			canvas_config_config(ge2d_comp_para->canvas_dst[2], &dst_canvas0_config[2]);

		canvas_index = ge2d_comp_para->canvas_dst[0]
				| (ge2d_comp_para->canvas_dst[1] << 8)
				| (ge2d_comp_para->canvas_dst[2] << 16);

	} else {
		canvas_index = ge2d_comp_para->canvas0_addr;
	}

	ge2d_config.src_para.canvas_index = canvas_index;
	ge2d_config.alu_const_color = 0;/*0x000000ff;*/
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;/*0xff;*/
	ge2d_config.dst_xy_swap = 0;

	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = ge2d_comp_para->format;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0;
	ge2d_config.src_para.top = 0;
	ge2d_config.src_para.left = 0;
	ge2d_config.src_para.width = ge2d_comp_para->buffer_w;
	ge2d_config.src_para.height = ge2d_comp_para->buffer_h;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;

	ge2d_config.dst_para.canvas_index = canvas_index;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.format = ge2d_comp_para->format;
	if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21)
		ge2d_config.dst_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config.dst_para.width = ge2d_comp_para->buffer_w;
	ge2d_config.dst_para.height = ge2d_comp_para->buffer_h;
	ge2d_config.mem_sec = ge2d_comp_para->is_tvp;

	temp = ge2d_context_config_ex(ge2d_comp_para->context, &ge2d_config);
	if (temp < 0) {
		dp_print(dev_index, PRINT_ERROR, "%s: ge2d configing error.\n", __func__);
		return -1;
	}

	fillrect(ge2d_comp_para->context, 0, 0,
		 ge2d_comp_para->buffer_w,
		 ge2d_comp_para->buffer_h,
		 0x008080ff);

	if (dump_flag) {
		para.canvas0_config[0] = dst_canvas0_config[0];
		para.canvas0_config[1] = dst_canvas0_config[1];
		para.canvas0_config[2] = dst_canvas0_config[2];
		para.plane_num = dst_plane_num;
		dump_data(dev_index, &para, BLACK_BUFFER);
	}

	return 0;
}

int config_ge2d(int dev_index, struct vframe_s *src_vf, unsigned long addr, int buf_w, int buf_h,
	int data_w, int data_h, int crop_x, int crop_y, int crop_w, int crop_h,
	struct src_data_para *data)
{
	struct vframe_s *vf = NULL;
	int vf_height;

	if (IS_ERR_OR_NULL(data)) {
		dp_print(dev_index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return -1;
	}

	if (src_vf) {
		if (src_vf->canvas0_config[0].phy_addr == 0) {
			if ((src_vf->flag &  VFRAME_FLAG_DOUBLE_FRAM) && src_vf->vf_ext) {
				vf = src_vf->vf_ext;
			} else {
				dp_print(dev_index, PRINT_ERROR,
					"%s: vf no yuv data, composer fail\n", __func__);
				return -1;
			}
		} else {
			vf = src_vf;
		}

		if (vf && vf->di_flag && IS_DI_PSTLINK(vf->di_flag))
			vf_height = vf->height >> 1;
		else
			vf_height = vf->height;

		data->canvas0Addr = vf->canvas0Addr;
		data->canvas1Addr = vf->canvas1Addr;
		data->canvas0_config[0] = vf->canvas0_config[0];
		data->canvas0_config[1] = vf->canvas0_config[1];
		data->canvas0_config[2] = vf->canvas0_config[2];
		data->canvas1_config[0] = vf->canvas1_config[0];
		data->canvas1_config[1] = vf->canvas1_config[1];
		data->canvas1_config[2] = vf->canvas1_config[2];
		data->bitdepth = vf->bitdepth;
		data->source_type = vf->source_type;
		data->type = vf->type;
		data->plane_num = vf->plane_num;
		data->position_x = crop_x;
		data->position_y = crop_y;
		data->width = crop_w;
		data->height = crop_h;

		dp_print(dev_index, PRINT_ROTATE,
			"scr:vf->canvas0_config[0]: paddr=%lu width=%d height=%d\n",
			vf->canvas0_config[0].phy_addr,
			vf->canvas0_config[0].width,
			vf->canvas0_config[0].height);
		dp_print(dev_index, PRINT_ROTATE,
			"scr:vf->canvas0_config[1]: paddr=%lu width=%d height=%d\n",
			vf->canvas0_config[1].phy_addr,
			vf->canvas0_config[1].width,
			vf->canvas0_config[1].height);
		dp_print(dev_index, PRINT_ROTATE,
			"scr:vf->width=%d vf->height=%d vf->ComW=%d vf->ComH=%d\n",
			vf->width,
			vf->height,
			vf->compWidth,
			vf->compHeight);
		dp_print(dev_index, PRINT_ROTATE, "scr:crop %d %d %d %d\n",
			crop_x, crop_y, crop_w, crop_h);
		dp_print(dev_index, PRINT_ROTATE, "scr:data %d %d %d %d\n",
			data->position_x, data->position_y, data->width, data->height);

		if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR)
			data->is_vframe = false;
		else
			data->is_vframe = true;
	} else {
		data->canvas0Addr = -1;
		data->canvas1Addr = -1;
		data->canvas0_config[0].phy_addr = addr;

		dp_print(dev_index, PRINT_ROTATE, "buffer_w(%d), data_w(%d)\n", buf_w, data_w);
		if (buf_w > data_w) {
			if (data->buf_format == YUV444)
				data->canvas0_config[0].width = buf_w * 3;
			else
				data->canvas0_config[0].width = buf_w;
		} else {
			if (data->buf_format == YUV444)
				data->canvas0_config[0].width = data_w * 3;
			else
				data->canvas0_config[0].width = data_w;
		}
		dp_print(dev_index, PRINT_ROTATE, "buffer_h(%d), data_h(%d)\n", buf_h, data_h);

		if (buf_h > data_h)
			data->canvas0_config[0].height = buf_h;
		else
			data->canvas0_config[0].height = data_h;

		data->canvas0_config[0].block_mode = CANVAS_BLKMODE_LINEAR;
		data->canvas0_config[0].endian = 0;
		data->canvas0_config[1].phy_addr =
			(u32)(addr + (data->canvas0_config[0].width)
			      * (data->canvas0_config[0].height));
		data->canvas0_config[1].width =
			data->canvas0_config[0].width;
		data->canvas0_config[1].height =
			data->canvas0_config[0].height / 2;
		data->canvas0_config[1].block_mode =
			CANVAS_BLKMODE_LINEAR;
		data->canvas0_config[1].endian = 0;
		data->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		data->source_type = 0;
		if (data->buf_format == YUV444) {
			data->type = VIDTYPE_VIU_SINGLE_PLANE
				| VIDTYPE_VIU_FIELD
				| VIDTYPE_VIU_444;
			data->plane_num = 1;
		} else if (data->buf_format == NV12_VC) {
			data->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV12;
			data->plane_num = 2;
		} else {
			data->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21;
			data->plane_num = 2;
		}

		dp_print(dev_index, PRINT_ROTATE, "crop %d %d %d %d\n",
			crop_x, crop_y, crop_w, crop_h);
		data->position_x = crop_x;
		data->position_y = crop_y;
		data->is_vframe = false;
		if (crop_w <= 0 || crop_h <= 0) {
			data->width = buf_w;
			data->height = buf_h;
		} else {
			data->width = crop_w;
			data->height = crop_h;
		}
	}

	return 0;
}

int ge2d_data_process(int dev_index, struct src_data_para *scr_data,
	struct ge2d_composer_para *ge2d_comp_para)
{
	int ret;
	struct canvas_config_s dst_canvas0_config[3];
	struct config_para_ex_s ge2d_config;
	u32 dst_plane_num;
	int src_canvas_id, dst_canvas_id;
	int input_x, input_y, input_width, input_height;
	int position_left, position_top;
	int position_width, position_height;
	struct dump_param para;
	enum ge2d_angle_type ge2d_angle;

	dp_print(dev_index, PRINT_ROTATE, "%s: start process!\n", __func__);

	memset(&ge2d_config, 0, sizeof(struct config_para_ex_s));
	memset(dst_canvas0_config, 0, sizeof(dst_canvas0_config));

	if (ge2d_comp_para->format == GE2D_FORMAT_S24_YUV444) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w * 3;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
	} else if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21) {
		dst_canvas0_config[0].phy_addr = ge2d_comp_para->phy_addr[0];
		dst_canvas0_config[0].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[0].height = ge2d_comp_para->buffer_h;
		dst_canvas0_config[0].block_mode = 0;
		dst_canvas0_config[0].endian = 0;
		dst_canvas0_config[1].phy_addr = ge2d_comp_para->phy_addr[0]
			+ ge2d_comp_para->buffer_w * ge2d_comp_para->buffer_h;
		dst_canvas0_config[1].width = ge2d_comp_para->buffer_w;
		dst_canvas0_config[1].height = ge2d_comp_para->buffer_h >> 1;
		dst_canvas0_config[1].block_mode = 0;
		dst_canvas0_config[1].endian = 0;
	}
	dst_plane_num = ge2d_comp_para->plane_num;

	if (scr_data->canvas0Addr == (u32)-1) {
		ret = alloc_src_canvas(dev_index, ge2d_comp_para);
		if (ret < 0) {
			dp_print(dev_index, PRINT_ERROR, "%s: alloc src canvas error!\n", __func__);
			return -1;
		}
		canvas_config_config(ge2d_comp_para->canvas_scr[0], &scr_data->canvas0_config[0]);
		if (scr_data->plane_num == 2) {
			canvas_config_config(ge2d_comp_para->canvas_scr[1],
					     &scr_data->canvas0_config[1]);
		} else if (scr_data->plane_num == 3) {
			canvas_config_config(ge2d_comp_para->canvas_scr[1],
					     &scr_data->canvas0_config[1]);
			canvas_config_config(ge2d_comp_para->canvas_scr[2],
					     &scr_data->canvas0_config[2]);
		}
		src_canvas_id = ge2d_comp_para->canvas_scr[0]
			| (ge2d_comp_para->canvas_scr[1] << 8)
			| (ge2d_comp_para->canvas_scr[2] << 16);
	} else {
		src_canvas_id = scr_data->canvas0Addr;
	}

	if (ge2d_comp_para->canvas0_addr == (u32)-1) {
		canvas_config_config(ge2d_comp_para->canvas_dst[0],
				     &dst_canvas0_config[0]);
		if (dst_plane_num == 2) {
			canvas_config_config(ge2d_comp_para->canvas_dst[1],
					     &dst_canvas0_config[1]);
		} else if (dst_plane_num == 3) {
			canvas_config_config(ge2d_comp_para->canvas_dst[2],
					     &dst_canvas0_config[2]);
		}
		dst_canvas_id = ge2d_comp_para->canvas_dst[0]
			| (ge2d_comp_para->canvas_dst[1] << 8)
			| (ge2d_comp_para->canvas_dst[2] << 16);
	} else {
		dst_canvas_id = ge2d_comp_para->canvas0_addr;
	}
	input_width = scr_data->width;
	input_height = scr_data->height;
	input_x = scr_data->position_x;
	input_y = scr_data->position_y;

	if (scr_data->type & VIDTYPE_INTERLACE)
		input_height = scr_data->height >> 1;
	else
		input_height = scr_data->height;

	ge2d_config.alu_const_color = 0;
	ge2d_config.bitmask_en = 0;
	ge2d_config.src1_gb_alpha = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_config.src_key.key_enable = 0;
	ge2d_config.src_key.key_mask = 0;
	ge2d_config.src_key.key_mode = 0;
	ge2d_config.src_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.src_para.format = get_input_format(scr_data);
	if (!scr_data->is_vframe)
		ge2d_config.src_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config.src_para.fill_color_en = 0;
	ge2d_config.src_para.fill_mode = 0;
	ge2d_config.src_para.x_rev = 0;
	ge2d_config.src_para.y_rev = 0;
	ge2d_config.src_para.color = 0xffffffff;
	ge2d_config.src_para.top = input_y;
	ge2d_config.src_para.left = input_x;
	ge2d_config.src_para.width = input_width;
	ge2d_config.src_para.height = input_height;
	ge2d_config.src_para.canvas_index = src_canvas_id;
	ge2d_config.src2_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.mem_type = CANVAS_TYPE_INVALID;
	ge2d_config.dst_para.fill_color_en = 0;
	ge2d_config.dst_para.fill_mode = 0;
	ge2d_config.dst_para.x_rev = 0;
	ge2d_config.dst_para.y_rev = 0;
	ge2d_config.dst_xy_swap = 0;
	ge2d_angle = config_ge2d_rotation(dev_index, ge2d_comp_para->angle);

	if (ge2d_angle == GE2D_ANGLE_TYPE_ROT_90) {
		ge2d_config.dst_xy_swap = 1;
		ge2d_config.dst_para.x_rev = 1;
	} else if (ge2d_angle == GE2D_ANGLE_TYPE_ROT_180) {
		ge2d_config.dst_para.x_rev = 1;
		ge2d_config.dst_para.y_rev = 1;
	} else if (ge2d_angle == GE2D_ANGLE_TYPE_ROT_270) {
		ge2d_config.dst_xy_swap = 1;
		ge2d_config.dst_para.y_rev = 1;
	} else if (ge2d_angle == GE2D_ANGLE_TYPE_FLIP_H) {
		ge2d_config.dst_xy_swap = 0;
		ge2d_config.dst_para.x_rev = 1;
		ge2d_config.dst_para.y_rev = 0;
	} else if (ge2d_angle == GE2D_ANGLE_TYPE_FLIP_V) {
		ge2d_config.dst_xy_swap = 0;
		ge2d_config.dst_para.x_rev = 0;
		ge2d_config.dst_para.y_rev = 1;
	}

	ge2d_config.dst_para.canvas_index = dst_canvas_id;
	ge2d_config.dst_para.color = 0;
	ge2d_config.dst_para.top = 0;
	ge2d_config.dst_para.left = 0;
	ge2d_config.dst_para.format = ge2d_comp_para->format;
	if (ge2d_comp_para->format == GE2D_FORMAT_M24_NV21)
		ge2d_config.dst_para.format |= GE2D_LITTLE_ENDIAN;
	ge2d_config.dst_para.width = ge2d_comp_para->buffer_w;
	ge2d_config.dst_para.height = ge2d_comp_para->buffer_h;
	ge2d_config.mem_sec = ge2d_comp_para->is_tvp;

	ret = ge2d_context_config_ex(ge2d_comp_para->context, &ge2d_config);
	if (ret < 0) {
		dp_print(dev_index, PRINT_ERROR, "ge2d configing error.\n");
		return -1;
	}

	//dump src buf
	if (dump_flag) {
		para.canvas0_config[0] = scr_data->canvas0_config[0];
		para.canvas0_config[1] = scr_data->canvas0_config[1];
		para.canvas0_config[2] = scr_data->canvas0_config[2];
		dp_print(dev_index, PRINT_ROTATE, "canvas0:%d %d canvas1:%d %d canvas2:%d %d.\n",
			scr_data->canvas0_config[0].width, scr_data->canvas0_config[0].height,
			scr_data->canvas0_config[1].width, scr_data->canvas0_config[1].height,
			scr_data->canvas0_config[2].width, scr_data->canvas0_config[2].height);
		para.plane_num = scr_data->plane_num;
		dump_data(dev_index, &para, SCR_BUFFER);
	}

	position_left = ge2d_comp_para->position_left;
	position_top = ge2d_comp_para->position_top;
	position_width = ge2d_comp_para->position_width;
	position_height = ge2d_comp_para->position_height;

	stretchblt_noalpha(ge2d_comp_para->context,
			   input_x,
			   input_y,
			   input_width,
			   input_height,
			   position_left,
			   position_top,
			   position_width,
			   position_height);

	dp_print(dev_index, PRINT_ROTATE, "format: src %d, dst: %d, position: %d %d %d %d.\n",
		ge2d_config.src_para.format,
		ge2d_config.dst_para.format,
		position_left,
		position_top,
		position_width,
		position_height);
	//dump dst buf
	if (dump_flag) {
		para.canvas0_config[0] = dst_canvas0_config[0];
		para.canvas0_config[1] = dst_canvas0_config[1];
		para.canvas0_config[2] = dst_canvas0_config[2];
		para.plane_num = dst_plane_num;
		dump_data(dev_index, &para, DST_BUFFER_DATA);
	}
	dump_flag = 0;
	dp_print(dev_index, PRINT_ROTATE, "%s: process end.\n", __func__);

	return 0;
}

int set_dump_flag(int flag)
{
	dump_flag = flag;
	return 0;
}
