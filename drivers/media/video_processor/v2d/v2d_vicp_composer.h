/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef V2D_VICP_COMPOSER_H
#define V2D_VICP_COMPOSER_H
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vicp/vicp.h>

bool check_vicp_status(struct vframe_s *input_vf, int work_mode, int index);
enum vicp_rotation_mode_e map_rotationmode_from_v2d_to_vicp(int v2d_rotation);
int v2d_config_vicp_input_data(struct vframe_s *vf, ulong addr, int width, int height,
	int stride_w, int stride_y, int endian, int color_fmt, int color_depth,
	struct input_data_param_s *input_data);
int v2d_config_vicp_output_data(int fbc_out_en, int mif_out_en, ulong *phy_addr, int stride,
	int width, int height, int endian, enum vicp_color_format_e cfmt_mif, int cdep_mif,
	enum vicp_color_format_e cfmt_fbc, int cdep_fbc, int init_ctrl, int pip_mode,
	int out_sig_fmt, struct output_data_param_s *output_data);
int config_vicp_axis_crop(struct vicp_data_config_s *data_config, int vicp_rotate_mode,
	int i, int count, int rotate_part, int vicp_transform, struct output_axis_s *display_axis,
	struct crop_info_s *crop_info, struct vframe_s *src_vf);

int v2d_vicp_data_composer(struct vicp_data_config_s *data_config);
#ifdef CONFIG_AMLOGIC_MEDIA_VICP
int vicp_process(struct vicp_data_config_s *data_config);
#endif
#endif
