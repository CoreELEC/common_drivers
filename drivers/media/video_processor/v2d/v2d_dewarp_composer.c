// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <api/gdc_api.h>
#include "v2d_dewarp_composer.h"
#include "v2d_util.h"

#define GDC_FIRMWARE_PATH    "/vendor/lib/firmware/gdc/"

static unsigned int dewarp_com_dump_last;
static int v2d_index;

int v2d_get_dewarp_format(int index, struct vframe_s *vf)
{
	int format = NV12;

	if (IS_ERR_OR_NULL(vf))
		return -1;

	if ((vf->type & VIDTYPE_VIU_NV21) ||
	    (vf->type & VIDTYPE_VIU_NV12))
		format = NV12;
	else if (vf->type & VIDTYPE_VIU_422)
		format = 0;
	else if (vf->type & VIDTYPE_VIU_444)
		format = YUV444_P;
	else if (vf->type & VIDTYPE_RGB_444)
		format = RGB444_P;

	return format;
}

static int get_dewarp_rotation_value(int transform)
{
	int rotate_value = 0;

	if (transform == 1)
		rotate_value = 1 << 10;
	else if (transform == 2)
		rotate_value = 2 << 10;
	else if (transform == 4 || transform == 5 || transform == 6)
		rotate_value = 270;
	else if (transform == 3)
		rotate_value = 180;
	else if (transform == 7)
		rotate_value = 90;
	else
		rotate_value = 0;

	return rotate_value;
}

static int dump_dewarp_vframe(char *path, int width, int height, u32 phy_adr_y, u32 phy_adr_uv)
{
	int err = -1;

#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	int size = 0;
	struct file *fp = NULL;
	loff_t position = 0;
	u8 *data;

	if (IS_ERR_OR_NULL(path)) {
		err = 1;
		goto exit;
	}

	/* open file to write */
	fp = filp_open(path, O_WRONLY | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		err = 2;
		goto exit;
	}

	/* Write buf to file */
	size = width * height;
	data = codec_mm_vmap(phy_adr_y, size);
	if (!data) {
		err = 3;
		goto exit;
	}
	/* change to KERNEL_DS address limit */
	kernel_write(fp, data, size, &position);
	codec_mm_unmap_phyaddr(data);

	size = width * height / 2;
	data = codec_mm_vmap(phy_adr_uv, size);
	if (!data) {
		err = 4;
		goto exit;
	}
	/* change to KERNEL_DS address limit */
	kernel_write(fp, data, size, &position);
	codec_mm_unmap_phyaddr(data);

	filp_close(fp, NULL);
	return 0;
#else
	err = 5;
	goto exit;
#endif

exit:
	pr_info("dump failed, errno =  %d.\n", err);
	return -1;
}

int v2d_load_dewarp_firmware(struct dewarp_composer_para *param)
{
	int err = -1;

#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	int ret = 0;
#endif
	char file_name[64];
	struct firmware_rotate_s fw_param;
	bool is_need_load = false;
	int frame_rotation = 0;

	if (IS_ERR_OR_NULL(param)) {
		err = 1;
		goto exit;
	}

	frame_rotation = get_dewarp_rotation_value(param->vf_para->src_vf_angle);
	if (param->last_fw_param.in_width != param->vf_para->src_vf_width ||
		param->last_fw_param.in_height != param->vf_para->src_vf_height ||
		param->last_fw_param.out_width != param->vf_para->dst_vf_width ||
		param->last_fw_param.out_height != param->vf_para->dst_vf_height ||
		param->last_fw_param.degree != frame_rotation) {
		param->last_fw_param.format = NV12;
		param->last_fw_param.in_width = param->vf_para->src_vf_width;
		param->last_fw_param.in_height = param->vf_para->src_vf_height;
		param->last_fw_param.out_width = param->vf_para->dst_vf_width;
		param->last_fw_param.out_height = param->vf_para->dst_vf_height;
		param->last_fw_param.degree = frame_rotation;
		is_need_load = true;
	}

	if (dewarp_print) {
		pr_info("v2d:[%d] need load firmware: %d.\n", param->index, is_need_load);
		pr_info("v2d:[%d] src_vf, w:%d, h:%d, fromat:%d, rotation:%d.\n",
			param->index,
			param->vf_para->src_vf_width,
			param->vf_para->src_vf_height,
			param->vf_para->src_vf_format,
			param->vf_para->src_vf_angle);
		pr_info("v2d:[%d] src_buf, w0:%d, w1:%d.\n", param->index,
			param->vf_para->src_buf_stride0, param->vf_para->src_buf_stride1);
		pr_info("v2d:[%d] dst_vf, w:%d, h:%d.\n", param->index,
			param->vf_para->dst_vf_width, param->vf_para->dst_vf_height);
		pr_info("v2d:[%d] dst_buf, w:%d.\n", param->index,
			param->vf_para->dst_buf_stride);
	}

	if (param->fw_load.phys_addr == 0 || is_need_load) {
		v2d_unload_dewarp_firmware(param);
		pr_info("v2d:[%d] start load firmware.\n", param->index);
		if (dewarp_load_flag) {
			memset(file_name, 0, 64);
			sprintf(file_name, "%dx%d-%dx%d-%d_nv12.bin",
				param->vf_para->src_vf_width,
				param->vf_para->src_vf_height,
				param->vf_para->dst_vf_width,
				param->vf_para->dst_vf_height,
				frame_rotation);
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
			ret = load_firmware_by_name(file_name, &param->fw_load);
			if (ret <= 0) {
				err = 2;
				goto exit;
			}
#else
			err = 3;
			goto exit;
#endif
		} else {
			fw_param.format = NV12;
			fw_param.in_width = param->vf_para->src_vf_width;
			fw_param.in_height = param->vf_para->src_vf_height;
			fw_param.out_width = param->vf_para->dst_vf_width;
			fw_param.out_height = param->vf_para->dst_vf_height;
			fw_param.degree = frame_rotation;
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
			ret = rotation_calc_and_load_firmware(&fw_param, &param->fw_load);
			if (ret <= 0) {
				err = 4;
				goto exit;
			}
#else
			err = 5;
			goto exit;
#endif
		}
	}
	return 0;

exit:
	pr_info("load firmware failed, errno =  %d.\n", err);
	return -1;
}

int v2d_unload_dewarp_firmware(struct dewarp_composer_para *param)
{
	if (IS_ERR_OR_NULL(param))
		return -1;

	if (param->fw_load.phys_addr != 0) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		release_config_firmware(&param->fw_load);
#endif
		param->fw_load.phys_addr = 0;
	}

	return 0;
}

bool check_dewarp_status(struct vframe_s *input_vf, int count, int work_mode, int index)
{
	int src_formate;

	v2d_index = index;
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	if (!is_aml_gdc_supported())
		return false;
#else
	return false;
#endif

	if (count > 1)
		return false;

	if (work_mode != 1)
		return false;

	if (!input_vf) {
		src_formate = NV12;
	} else {
		if (work_mode == 1 && (input_vf->type & VIDTYPE_COMPRESS))
			return false;

		src_formate = v2d_get_dewarp_format(index, input_vf);
	}

	if (src_formate != NV12)
		return false;

	return true;
}

int v2d_init_dewarp_composer(struct dewarp_composer_para *param)
{
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	int dev_type = AML_GDC;
#endif

	if (IS_ERR_OR_NULL(param))
		return -1;

	if (IS_ERR_OR_NULL(param->context)) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		dev_type = get_gdc_version();
		param->context = create_gdc_work_queue(dev_type);
#else
		param->context = NULL;
#endif
		if (IS_ERR_OR_NULL(param->context)) {
			pr_info("v2d[%d]: create dewrap work_queue failed.\n", param->index);
			return -1;
		}
	}

	return 0;
}

int v2d_uninit_dewarp_composer(struct dewarp_composer_para *param)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(param))
		return -1;

	v2d_unload_dewarp_firmware(param);

	if (!IS_ERR_OR_NULL(param->context)) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		ret = destroy_gdc_work_queue(param->context);
#else
		ret = -1;
#endif
		if (ret != 0) {
			pr_info("v2d[%d]: destroy dewarp work queue failed.\n", param->index);
			return -1;
		}
	}

	param->context = NULL;
	return 0;
}

int v2d_config_dewarp_vframe(struct dewarp_vf_para_s *vframe_para,
			struct dewarp_common_para *common_para)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *src_vf = common_para->input_para.vframe;
	int call_index = common_para->input_para.call_index;
	struct pic_s *pic_info_in = &common_para->input_para.pic_info;
	struct pic_s *pic_info_out = &common_para->output_para.pic_info;

	if (IS_ERR_OR_NULL(vframe_para))
		return -1;

	if (src_vf) {
		if (src_vf->canvas0_config[0].phy_addr == 0) {
			if ((src_vf->flag &  VFRAME_FLAG_DOUBLE_FRAM) &&
				src_vf->vf_ext) {
				vf = src_vf->vf_ext;
			} else {
				pr_info("v2d:[%d] config: vf no yuv data.\n", call_index);
				return -1;
			}
		} else {
			vf = src_vf;
		}

		vframe_para->src_vf_width = vf->width;
		vframe_para->src_vf_height = vf->height;
		vframe_para->src_vf_format = v2d_get_dewarp_format(call_index, vf);
		vframe_para->src_vf_plane_count = 2;
		vframe_para->src_buf_addr0 = vf->canvas0_config[0].phy_addr;
		vframe_para->src_buf_stride0 = vf->canvas0_config[0].width;
		vframe_para->src_buf_addr1 = vf->canvas0_config[1].phy_addr;
		vframe_para->src_buf_stride1 = vf->canvas0_config[1].width;
		vframe_para->src_vf_angle = common_para->input_para.transform;
		vframe_para->src_endian = vf->canvas0_config[0].endian;
		if (vf->type & VIDTYPE_VIU_NV12)
			vframe_para->uvswap_enable = 1;

		vframe_para->dst_vf_width = pic_info_out->align_w;
		vframe_para->dst_vf_height = pic_info_out->align_h;
		vframe_para->dst_vf_plane_count = 2;
		vframe_para->dst_buf_addr = pic_info_out->addr[0];
		vframe_para->dst_buf_stride = pic_info_out->align_w;
		vframe_para->dst_endian = 0;
		vframe_para->is_tvp = pic_info_in->is_tvp;
	} else {
		if (pic_info_in->format == V2D_SRC_YUV444) {
			vframe_para->src_vf_format = YUV444_P;
			vframe_para->src_buf_stride0 = pic_info_in->align_w * 3;
		} else if (pic_info_in->format == V2D_SRC_NV12) {
			vframe_para->uvswap_enable = 1;
			vframe_para->src_vf_format = NV12;
			vframe_para->src_buf_stride0 = pic_info_in->align_w;
		} else {
			vframe_para->src_vf_format = NV12;
			vframe_para->src_buf_stride0 = pic_info_in->align_w;
		}

		vframe_para->src_vf_width = pic_info_in->width;
		vframe_para->src_vf_height = pic_info_in->height;
		vframe_para->src_vf_plane_count = 2;
		vframe_para->src_buf_addr0 = pic_info_in->addr[0];
		vframe_para->src_buf_addr1 = pic_info_in->addr[0]
			+ vframe_para->src_buf_stride0 * pic_info_in->align_h;
		vframe_para->src_buf_stride1 = vframe_para->src_buf_stride0;
		vframe_para->src_vf_angle = common_para->input_para.transform;
		vframe_para->src_endian = 0;

		vframe_para->dst_vf_width = pic_info_out->align_w;
		vframe_para->dst_vf_height = pic_info_out->align_h;
		vframe_para->dst_vf_plane_count = 2;
		vframe_para->dst_buf_addr = pic_info_out->addr[0];
		vframe_para->dst_buf_stride = pic_info_out->align_w;
		vframe_para->dst_endian = 0;
		vframe_para->is_tvp = pic_info_in->is_tvp;
	}
	if (dewarp_print) {
		pr_info("v2d:[%d]---------------------------------.\n", call_index);
		pr_info("v2d:[%d] src:addr0:%lx addr1:%lx width:%d height:%d\n", call_index,
			vframe_para->src_buf_addr0, vframe_para->src_buf_addr1,
			vframe_para->src_vf_width, vframe_para->src_vf_height);
		pr_info("v2d:[%d] src:stride0:%d stride1:%d format:%d swap:%d\n", call_index,
			vframe_para->src_buf_stride0, vframe_para->src_buf_stride1,
			vframe_para->src_vf_format, vframe_para->uvswap_enable);
		pr_info("v2d:[%d] dst:addr0:%lx width:%d height:%d stride:%d\n", call_index,
			vframe_para->dst_buf_addr, vframe_para->dst_vf_width,
			vframe_para->dst_vf_height, vframe_para->dst_buf_stride);

		pr_info("v2d:[%d]---------------------------------.\n", call_index);
	}
	return 0;
}

int v2d_dewarp_data_composer(struct dewarp_composer_para *param, bool is_tvp)
{
	int ret;
	struct gdc_phy_setting gdc_config;
	char dump_name[32];

	if (IS_ERR_OR_NULL(param))
		return -1;

	memset(&gdc_config, 0, sizeof(struct gdc_phy_setting));
	gdc_config.format = param->vf_para->src_vf_format;
	gdc_config.in_width = param->vf_para->src_vf_width;
	gdc_config.in_height = param->vf_para->src_vf_height;
	/*16-byte alignment*/
	gdc_config.in_y_stride = AXI_WORD_ALIGN(param->vf_para->src_buf_stride0);
	/*16-byte alignment*/
	gdc_config.in_c_stride = AXI_WORD_ALIGN(param->vf_para->src_buf_stride1);
	gdc_config.in_plane_num = param->vf_para->src_vf_plane_count;
	gdc_config.out_width = param->vf_para->dst_vf_width;
	gdc_config.out_height = param->vf_para->dst_vf_height;
	/*16-byte alignment*/
	gdc_config.out_y_stride = AXI_WORD_ALIGN(param->vf_para->dst_buf_stride);
	/*16-byte alignment*/
	gdc_config.out_c_stride = AXI_WORD_ALIGN(param->vf_para->dst_buf_stride);
	gdc_config.out_plane_num = param->vf_para->dst_vf_plane_count;
	gdc_config.in_paddr[0] = param->vf_para->src_buf_addr0;
	gdc_config.in_paddr[1] = param->vf_para->src_buf_addr1;
	gdc_config.out_paddr[0] = param->vf_para->dst_buf_addr;
	gdc_config.out_paddr[1] = param->vf_para->dst_buf_addr
				+ AXI_WORD_ALIGN(gdc_config.out_width)
				* AXI_WORD_ALIGN(gdc_config.out_height);
	gdc_config.config_paddr = param->fw_load.phys_addr;
	gdc_config.config_size = param->fw_load.size_32bit; /* in 32bit */
	gdc_config.uvswap_enable = param->vf_para->uvswap_enable;
	if (param->vf_para->is_tvp)
		gdc_config.use_sec_mem = 1; /* secure mem access */
	else
		gdc_config.use_sec_mem = 0;

	if (param->vf_para->src_endian != 0)
		gdc_config.in_endian = GDC_ENDIAN_BIG_8BYTES;
	else
		gdc_config.in_endian = GDC_ENDIAN_LITTLE;

	if (param->vf_para->dst_endian != 0)
		gdc_config.out_endian = GDC_ENDIAN_BIG_8BYTES;
	else
		gdc_config.out_endian = GDC_ENDIAN_LITTLE;
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	ret = gdc_process_phys(param->context, &gdc_config);
#else
	ret = -1;
#endif
	if (ret < 0) {
		pr_info("v2d[%d]: dewarp composer failed.\n", param->index);
	} else {
		if (dewarp_com_dump != dewarp_com_dump_last) {
			sprintf(dump_name, "/data/src_%d.yuv", dewarp_com_dump);
			dump_dewarp_vframe(dump_name,
				param->vf_para->src_vf_width,
				param->vf_para->src_vf_height,
				param->vf_para->src_buf_addr0,
				param->vf_para->src_buf_addr1);

			sprintf(dump_name, "/data/dst_%d.yuv", dewarp_com_dump);
			dump_dewarp_vframe(dump_name,
				param->vf_para->dst_vf_width,
				param->vf_para->dst_vf_height,
				gdc_config.out_paddr[0],
				gdc_config.out_paddr[1]);
			dewarp_com_dump_last = dewarp_com_dump;
		}
	}
	if (dewarp_print)
		pr_info("v2d[%d]: composer:dst addr0:%lx addr1:%lx stride0:%d stride1:%d\n",
			param->index,
			gdc_config.out_paddr[0], gdc_config.out_paddr[1],
			gdc_config.out_y_stride, gdc_config.out_y_stride);
	return ret;
}
