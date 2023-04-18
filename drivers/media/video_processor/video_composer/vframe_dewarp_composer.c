// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include "vframe_ge2d_composer.h"
#include "vfq.h"
#include <linux/amlogic/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include "vc_util.h"
#include "vframe_dewarp_composer.h"
#include <api/gdc_api.h>
#include "video_composer.h"

#define GDC_FIRMWARE_PATH    "/vendor/lib/firmware/gdc/"

static unsigned int dewarp_com_dump;
MODULE_PARM_DESC(dewarp_com_dump, "\n dewarp_com_dump\n");
module_param(dewarp_com_dump, uint, 0664);
static unsigned int dewarp_print;
MODULE_PARM_DESC(dewarp_print, "\n dewarp_print\n");
module_param(dewarp_print, uint, 0664);

int get_dewarp_format(int vc_index, struct vframe_s *vf)
{
	int format = NV12;

	if (IS_ERR_OR_NULL(vf)) {
		pr_info("vc:[%d] %s: vf is NULL.\n", vc_index, __func__);
		return -1;
	}

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

static int get_dewarp_rotation_value(int vc_transform)
{
	int rotate_value = 0;

	if (vc_transform == 4 || vc_transform == 5 || vc_transform == 6)
		rotate_value = 270;
	else if (vc_transform == 3)
		rotate_value = 180;
	else if (vc_transform == 7)
		rotate_value = 90;
	else
		rotate_value = 0;

	return rotate_value;
}

static int dump_dewarp_vframe(char *path, int width, int height, u32 phy_adr_y, u32 phy_adr_uv)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	int size = 0;
	struct file *fp = NULL;
	loff_t position = 0;
	u8 *data;

	if (IS_ERR_OR_NULL(path)) {
		pr_info("%s: path is NULL.\n", __func__);
		return -1;
	}

	/* open file to write */
	fp = filp_open(path, O_WRONLY | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		pr_info("%s: open file error\n", __func__);
		return -1;
	}

	/* Write buf to file */
	size = width * height;
	data = codec_mm_vmap(phy_adr_y, size);
	if (!data) {
		pr_info("%s: vmap failed\n", __func__);
		return -1;
	}
	/* change to KERNEL_DS address limit */
	kernel_write(fp, data, size, &position);
	codec_mm_unmap_phyaddr(data);

	size = width * height / 2;
	data = codec_mm_vmap(phy_adr_uv, size);
	if (!data) {
		pr_info("%s: vmap failed\n", __func__);
		return -1;
	}
	/* change to KERNEL_DS address limit */
	kernel_write(fp, data, size, &position);
	codec_mm_unmap_phyaddr(data);

	filp_close(fp, NULL);
#else
	return -1;
#endif
}

int load_dewarp_firmware(struct dewarp_composer_para *param)
{
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	int ret = 0;
#endif
	char file_name[64];
	struct firmware_rotate_s fw_param;
	bool is_need_load = false;
	int frame_rotation = 0;

	if (IS_ERR_OR_NULL(param)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
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
		pr_info("vc:[%d] need load firmware: %d.\n", param->vc_index, is_need_load);
		pr_info("vc:[%d] src_vf, w:%d, h:%d, fromat:%d, rotation:%d.\n",
			param->vc_index,
			param->vf_para->src_vf_width,
			param->vf_para->src_vf_height,
			param->vf_para->src_vf_format,
			param->vf_para->src_vf_angle);
		pr_info("vc:[%d] src_buf, w0:%d, w1:%d.\n", param->vc_index,
			param->vf_para->src_buf_stride0, param->vf_para->src_buf_stride1);
		pr_info("vc:[%d] dst_vf, w:%d, h:%d.\n", param->vc_index,
			param->vf_para->dst_vf_width, param->vf_para->dst_vf_height);
		pr_info("vc:[%d] dst_buf, w:%d.\n", param->vc_index,
			param->vf_para->dst_buf_stride);
	}

	if (param->fw_load.phys_addr == 0 || is_need_load) {
		unload_dewarp_firmware(param);
		pr_info("vc:[%d] start load firmware.\n", param->vc_index);
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
				pr_info("vc:[%d] %s: load firmware failed.\n", param->vc_index,
					__func__);
				return -1;
			}
#else
			return -1;
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
				pr_info("vc:[%d] %s: calc and load firmware failed.\n",
					param->vc_index, __func__);
				return -1;
			}
#else
			return -1;
#endif
		}
	}
	return 0;
}

int unload_dewarp_firmware(struct dewarp_composer_para *param)
{
	if (IS_ERR_OR_NULL(param)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	if (param->fw_load.phys_addr != 0) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		release_config_firmware(&param->fw_load);
#endif
		param->fw_load.phys_addr = 0;
	}

	return 0;
}

bool is_dewarp_supported(int vc_index, struct composer_vf_para *vf_param)
{
#ifdef CONFIG_AMLOGIC_ENABLE_MEDIA_FILE
	int ret;
	struct kstat stat;
#endif
	char file_name[64];
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
	if (!is_aml_gdc_supported())
		return false;
#endif
	if (IS_ERR_OR_NULL(vf_param)) {
		pr_info("vc:[%d] %s: NULL param, please check.\n",
			vc_index,
			__func__);
		return false;
	}
	if (dewarp_print) {
		pr_info("vc:[%d] %s: src:w:%d h:%d format:%d, dst:w:%d h:%d, angle:%d.\n",
			vc_index,
			__func__,
			vf_param->src_vf_width,
			vf_param->src_vf_height,
			vf_param->src_vf_format,
			vf_param->dst_vf_width,
			vf_param->dst_vf_height,
			vf_param->src_vf_angle);
	}

	if (vf_param->src_vf_format != NV12)
		return false;

	if (dewarp_load_flag == 0) {
		if (get_dewarp_rotation_value(vf_param->src_vf_angle) == 0)
			return false;
		else
			return true;
	} else {
		memset(file_name, 0, 64);
		sprintf(file_name, "%s%dx%d-%dx%d-%d_nv12.bin",
			GDC_FIRMWARE_PATH,
			vf_param->src_vf_width,
			vf_param->src_vf_height,
			vf_param->dst_vf_width,
			vf_param->dst_vf_height,
			get_dewarp_rotation_value(vf_param->src_vf_angle));
#ifdef CONFIG_AMLOGIC_ENABLE_MEDIA_FILE
		ret = vfs_stat(file_name, &stat);
		if (ret < 0) {
			pr_info("vc:[%d] %s: %s don't exist.\n",
				vc_index,
				__func__,
				file_name);
			return false;
		}
		return true;
#else
		return false;
#endif
	}
}

int init_dewarp_composer(struct dewarp_composer_para *param)
{
	if (IS_ERR_OR_NULL(param)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(param->context)) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		param->context = create_gdc_work_queue(AML_GDC);
#else
		param->context = NULL;
#endif
		if (IS_ERR_OR_NULL(param->context)) {
			pr_info("vc:[%d] %s: create dewrap work_queue failed.\n",
				param->vc_index,
				__func__);
			return -1;
		} else {
			pr_info("vc:[%d] %s: create dewrap work_queue success.\n",
				param->vc_index,
				__func__);
		}
	} else {
		pr_info("vc:[%d] %s: dewrap work queue exist.\n",
			param->vc_index,
			__func__);
	}

	return 0;
}

int uninit_dewarp_composer(struct dewarp_composer_para *param)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(param)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	unload_dewarp_firmware(param);

	if (!IS_ERR_OR_NULL(param->context)) {
#ifdef CONFIG_AMLOGIC_MEDIA_GDC
		ret = destroy_gdc_work_queue(param->context);
#else
		ret = -1;
#endif
		if (ret != 0) {
			pr_info("vc:[%d] %s: destroy dewarp work queue failed.\n",
				param->vc_index,
				__func__);
			return -1;
		} else {
			pr_info("vc:[%d] %s: destroy dewarp work queue success.\n",
				param->vc_index,
				__func__);
		}
	} else {
		pr_info("vc:[%d] %s: dewarp work queue not create.\n",
			param->vc_index,
			__func__);
	}

	param->context = NULL;
	return 0;
}

int config_dewarp_vframe(struct composer_vf_para *vframe_para,
			struct composer_common_para *common_para)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *src_vf = common_para->input_para.vframe;
	int call_index = common_para->input_para.call_index;
	struct pic_struct_t *pic_info_in = &common_para->input_para.pic_info;
	struct pic_struct_t *pic_info_out = &common_para->output_para.pic_info;

	if (IS_ERR_OR_NULL(vframe_para)) {
		pr_info("vc:[%d] %s: vframe_para is null.\n", call_index, __func__);
		return -1;
	}

	if (src_vf) {
		if (src_vf->canvas0_config[0].phy_addr == 0) {
			if ((src_vf->flag &  VFRAME_FLAG_DOUBLE_FRAM) &&
				src_vf->vf_ext) {
				vf = src_vf->vf_ext;
			} else {
				pr_info("vc:[%d] %s: vf no yuv data.\n", call_index, __func__);
				return -1;
			}
		} else {
			vf = src_vf;
		}

		vframe_para->src_vf_width = vf->width;
		vframe_para->src_vf_height = vf->height;
		vframe_para->src_vf_format = get_dewarp_format(call_index, vf);
		vframe_para->src_vf_plane_count = 2;
		vframe_para->src_buf_addr0 = vf->canvas0_config[0].phy_addr;
		vframe_para->src_buf_stride0 = vf->canvas0_config[0].width;
		vframe_para->src_buf_addr1 = vf->canvas0_config[1].phy_addr;
		vframe_para->src_buf_stride1 = vf->canvas0_config[1].width;
		vframe_para->src_vf_angle = common_para->input_para.transform;
		vframe_para->src_endian = vf->canvas0_config[0].endian;

		vframe_para->dst_vf_width = pic_info_out->align_w;
		vframe_para->dst_vf_height = pic_info_out->align_h;
		vframe_para->dst_vf_plane_count = 2;
		vframe_para->dst_buf_addr = pic_info_out->addr[0];
		vframe_para->dst_buf_stride = pic_info_out->align_w;
		vframe_para->dst_endian = 0;
		vframe_para->is_tvp = pic_info_in->is_tvp;
	} else {
		if (pic_info_in->format == YUV444) {
			vframe_para->src_vf_format = YUV444_P;
			vframe_para->src_buf_stride0 = pic_info_in->align_w * 3;
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
		pr_info("vc:[%d]: src_vf: addr0:0x%x, addr1:0x%x, width:%d, height:%d, format:%d ",
			call_index,
			vframe_para->src_buf_addr0,
			vframe_para->src_buf_addr1,
			vframe_para->src_vf_width,
			vframe_para->src_vf_height,
			vframe_para->src_vf_format);
		pr_info("angle:%d stride_y:%d, stride_uv:%d.\n",
			vframe_para->src_vf_angle,
			vframe_para->src_buf_stride0,
			vframe_para->src_buf_stride1);
		pr_info("vc:[%d]: dst_vf: width:%d, height:%d stride:%d.\n",
			call_index,
			vframe_para->dst_vf_width,
			vframe_para->dst_vf_height,
			vframe_para->dst_buf_stride);
	}
	return 0;
}

int dewarp_data_composer(struct dewarp_composer_para *param, bool is_tvp)
{
	int ret, dump_num = 1;
	struct gdc_phy_setting gdc_config;
	char dump_name[32];

	if (IS_ERR_OR_NULL(param)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

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
		pr_info("vc:[%d] %s: dewrap process failed.\n", param->vc_index, __func__);
	} else {
		if (dewarp_com_dump != dump_num) {
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
			dump_num = dewarp_com_dump;
		}
	}
	return ret;
}
