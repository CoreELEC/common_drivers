/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef GE2D_PROCESS_H
#define GE2D_PROCESS_H
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include "dpss_process_util.h"

enum ge2d_angle_type {
	GE2D_ANGLE_TYPE_ROT_90 = 1,
	GE2D_ANGLE_TYPE_ROT_180,
	GE2D_ANGLE_TYPE_ROT_270,
	GE2D_ANGLE_TYPE_FLIP_H,
	GE2D_ANGLE_TYPE_FLIP_V,
	GE2D_ANGLE_TYPE_MAX,
};

struct ge2d_composer_para {
	int count;
	int format;
	int position_left;
	int position_top;
	int position_width;
	int position_height;
	int buffer_w;
	int buffer_h;
	int plane_num;
	int canvas_scr[3];
	int canvas_dst[3];
	ulong phy_addr[3];
	u32 canvas0_addr;
	struct ge2d_context_s *context;
	int angle;
	bool is_tvp;
};

struct src_data_para {
	u32 canvas0Addr;
	u32 canvas1Addr;
	u32 plane_num;
	u32 type;
	u32 position_x;
	u32 position_y;
	u32 width;
	u32 height;
	u32 bitdepth;
	struct canvas_config_s canvas0_config[3];
	struct canvas_config_s canvas1_config[3];
	enum vframe_source_type_e source_type;
	bool is_vframe;
	enum buffer_format_t buf_format;
};

struct dump_param {
	u32 plane_num;
	struct canvas_config_s canvas0_config[3];
};

enum buffer_data {
	BLACK_BUFFER = 0,
	SCR_BUFFER,
	DST_EMPTY_BUFFER,
	DST_BUFFER_DATA,
	OTHER_BUFFER,
};

int init_ge2d(int dev_index, struct ge2d_composer_para *ge2d_comp_para);

int uninit_ge2d(int dev_index, struct ge2d_composer_para *ge2d_comp_para);

int ge2d_fill_black(int dev_index, struct ge2d_composer_para *ge2d_comp_para);

int config_ge2d(int dev_index, struct vframe_s *src_vf, unsigned long addr, int buf_w, int buf_h,
	int data_w, int data_h, int crop_x, int crop_y, int crop_w, int crop_h,
	struct src_data_para *data);

int ge2d_data_process(int dev_index, struct src_data_para *src_para,
	struct ge2d_composer_para *ge2d_comp_para);

int set_dump_flag(int flag);
#endif
