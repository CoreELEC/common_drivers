// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/vicp/vicp.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/mm.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include "v2d_vicp_composer.h"
#include "v2d_util.h"

static int dev_index;

bool check_vicp_status(struct vframe_s *input_vf, int work_mode, int index)
{
	int cpu_type = 0;
	bool is_hardware_support = false;
	int ret = true;

	dev_index = index;

	cpu_type = get_cpu_type();
	v2d_print(dev_index, PRINT_OTHER, "%s: cpu_type is %d.\n", __func__, cpu_type);
	if (cpu_type == MESON_CPU_MAJOR_ID_S5 ||
		cpu_type == MESON_CPU_MAJOR_ID_T3X ||
		cpu_type == MESON_CPU_MAJOR_ID_S6 ||
		cpu_type == MESON_CPU_MAJOR_ID_A9)
		is_hardware_support = true;

	if (!is_hardware_support || work_mode == 3)
		return false;

	if (work_mode == 1) {
		if (!input_vf) {
			ret = false;
		} else {
			if (!(input_vf->type & VIDTYPE_COMPRESS)) {
				v2d_print(dev_index, PRINT_OTHER, "vicp not support mif rotate.\n");
				ret = false;
			}
		}
	}

	return ret;
}

enum vicp_rotation_mode_e map_rotationmode_from_v2d_to_vicp(int v2d_rotation)
{
	enum vicp_rotation_mode_e rotation = VICP_ROTATION_MAX;

	if (v2d_rotation == 1)
		rotation = VICP_ROTATION_MIRROR_H;
	else if (v2d_rotation == 2)
		rotation = VICP_ROTATION_MIRROR_V;
	else if (v2d_rotation == 3)
		rotation = VICP_ROTATION_180;
	else if (v2d_rotation == 4)
		rotation = VICP_ROTATION_90;
	else if (v2d_rotation == 7)
		rotation = VICP_ROTATION_270;
	else
		rotation = VICP_ROTATION_0;

	return rotation;
}

int v2d_config_vicp_input_data(struct vframe_s *vf, ulong addr, int width, int height,
	int stride_w, int stride_y, int endian, int color_fmt, int color_depth,
	struct input_data_param_s *input_data)
{
	if (IS_ERR_OR_NULL(input_data)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	memset(input_data, 0, sizeof(struct input_data_param_s));
	if (vf) {
		input_data->is_vframe = true;
		input_data->data_vf = vf;
	} else {
		input_data->is_vframe = false;
		if (stride_w > width)
			input_data->data_dma.buf_stride_w = stride_w;
		else
			input_data->data_dma.buf_stride_w = width;

		if (stride_y > height)
			input_data->data_dma.buf_stride_h = stride_y;
		else
			input_data->data_dma.buf_stride_h = height;

		input_data->data_dma.buf_addr = addr;
		input_data->data_dma.data_width = width;
		input_data->data_dma.data_height = height;
		input_data->data_dma.color_format = color_fmt;
		input_data->data_dma.color_depth = color_depth;
		input_data->data_dma.plane_count = 2;
		input_data->data_dma.endian = endian;
		input_data->data_dma.need_swap_cbcr = 0;
	}

	return 0;
}

int v2d_config_vicp_output_data(int fbc_out_en, int mif_out_en, ulong *phy_addr, int stride,
	int width, int height, int endian, enum vicp_color_format_e cfmt_mif, int cdep_mif,
	enum vicp_color_format_e cfmt_fbc, int cdep_fbc, int init_ctrl, int pip_mode,
	int out_sig_fmt, struct output_data_param_s *output_data)
{
	if (IS_ERR_OR_NULL(output_data)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}
	memset(output_data, 0, sizeof(struct output_data_param_s));
	output_data->width = width;
	output_data->height = height;
	output_data->phy_addr[0] = phy_addr[0];
	output_data->stride[0] = stride;
	output_data->mif_out_en = mif_out_en;
	output_data->endian = endian;
	output_data->need_swap_cbcr = 0;
	output_data->out_sig_fmt = out_sig_fmt;
	if (mif_out_en) {
		output_data->mif_color_fmt = cfmt_mif;
		output_data->mif_color_dep = cdep_mif;
	}

	output_data->fbc_out_en = fbc_out_en;
	if (fbc_out_en) {
		output_data->fbc_color_fmt = cfmt_fbc;
		output_data->fbc_color_dep = cdep_fbc;
		output_data->phy_addr[1] = phy_addr[1];
		output_data->phy_addr[2] = phy_addr[2];
		output_data->fbc_init_ctrl = init_ctrl;
		output_data->fbc_pip_mode = pip_mode;
	}

	return 0;
}

int config_vicp_axis_crop(struct vicp_data_config_s *data_config, int vicp_rotate_mode,
	int i, int count, int rotate_part, int vicp_transform, struct output_axis_s *display_axis,
	struct crop_info_s *crop_info, struct vframe_s *src_vf)
{
	u32 src_w = 0, src_h = 0;

	if (!data_config || !display_axis || !crop_info) {
		pr_info("%s: src_vf is NULL, please check.\n", __func__);
		return -1;
	}

	if (src_vf) {
		if (src_vf->type & VIDTYPE_COMPRESS) {
			src_w = src_vf->compWidth;
			src_h = src_vf->compHeight;
		} else {
			src_w = src_vf->width;
			src_h = src_vf->height;
		}
	} else {
		src_w = crop_info->width;
		src_h = crop_info->height;
	}

	if (vicp_rotate_mode == 0 || vicp_rotate_mode == 1) {
		data_config->data_option.input_source_count = count;
		data_config->data_option.input_source_number = i;
		data_config->data_option.crop_info.left = crop_info->left;
		data_config->data_option.crop_info.top = crop_info->top;
		data_config->data_option.crop_info.width = crop_info->width;
		data_config->data_option.crop_info.height = crop_info->height;
		if (vicp_rotate_mode == 0) {
			data_config->data_option.output_axis.left = display_axis->left;
			data_config->data_option.output_axis.top = display_axis->top;
			data_config->data_option.output_axis.width = display_axis->width;
			data_config->data_option.output_axis.height = display_axis->height;
		} else {
			data_config->data_option.output_axis.left = 0;
			data_config->data_option.output_axis.top = 0;
			data_config->data_option.output_axis.width = src_h;
			data_config->data_option.output_axis.height = src_w;
		}
	} else {
		data_config->data_option.input_source_count = 2;
		data_config->data_option.input_source_number = rotate_part;

		if (rotate_part == 0) {
			data_config->data_option.crop_info.left = 0;
			data_config->data_option.crop_info.top = 0;
			data_config->data_option.crop_info.width = src_w;
			data_config->data_option.crop_info.height = src_h / 2;
			data_config->data_option.output_axis.width = src_h / 2;
			data_config->data_option.output_axis.height = src_w;

			if (vicp_transform == 4 ||
				vicp_transform == 5 ||
				vicp_transform == 6) {
				data_config->data_option.output_axis.left = src_h / 2;
				data_config->data_option.output_axis.top = 0;
			} else {
				data_config->data_option.output_axis.left = 0;
				data_config->data_option.output_axis.top = 0;
			}
		} else {
			data_config->output_data.fbc_init_ctrl = 0;

			data_config->data_option.crop_info.left = 0;
			data_config->data_option.crop_info.top = src_h / 2;
			data_config->data_option.crop_info.width = src_w;
			data_config->data_option.crop_info.height = src_h / 2;
			data_config->data_option.output_axis.width = src_h / 2;
			data_config->data_option.output_axis.height = src_w;

			if (vicp_transform == 4 ||
				vicp_transform == 5 ||
				vicp_transform == 6) {
				data_config->data_option.output_axis.left = 0;
				data_config->data_option.output_axis.top = 0;
			} else {
				data_config->data_option.output_axis.left = src_h / 2;
				data_config->data_option.output_axis.top = 0;
			}
		}
	}

	return 0;
}

int v2d_vicp_data_composer(struct vicp_data_config_s *data_config)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(data_config)) {
		pr_info("%s: NULL param, please check.\n", __func__);
		return -1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_VICP
	ret = vicp_process(data_config);
#endif
	if (ret < 0)
		pr_info("%s: vicp_process failed.\n", __func__);

	return ret;
}
