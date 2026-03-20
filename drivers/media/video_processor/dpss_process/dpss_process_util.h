/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef DPSS_PROCESS_UTIL_H
#define DPSS_PROCESS_UTIL_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vfm/vframe.h>

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_FENCE		0X0002
#define PRINT_ROTATE	0X0004
#define PRINT_MORE		0X0080

#define DPSS_INSTANCE_COUNT 2
enum videocom_source_type {
	DECODER_8BIT_NORMAL = 0,
	DECODER_8BIT_BOTTOM,
	DECODER_8BIT_TOP,
	DECODER_10BIT_NORMAL,
	DECODER_10BIT_BOTTOM,
	DECODER_10BIT_TOP,
	VDIN_8BIT_NORMAL,
	VDIN_10BIT_NORMAL,
};

enum rotate_buffer_used {
	USED_UNINITIAL = 0,
	USED_BY_GE2D,
	USED_BY_DEWARP,
};

enum buffer_format_t {
	NV21 = 0,
	YUV444 = 1,
	YUV444_10BIT = 2,
	NV12_VC = 3,
};

enum rotate_buffer_status {
	UNINITIAL = 0,
	INIT_SUCCESS,
	INIT_ERROR,
};

struct rotate_buf_t {
	int index;
	struct vframe_s frame;
	enum rotate_buffer_used buf_used;
	bool dirty;
	struct file *src_file;
	ulong phy_addr;
	u32 buf_w;
	u32 buf_h;
	u32 buf_size;
	bool is_tvp;
	u32 dw_size;
	ulong afbc_head_addr;
	u32 afbc_head_size;
	ulong afbc_body_addr;
	u32 afbc_body_size;
	ulong afbc_table_addr;
	ulong afbc_table_handle;
	u32 afbc_table_size;
};

int set_dp_print_flag(int index, u32 value);
u32 get_dp_print_flag(int index);
int dp_print(int index, int debug_flag, const char *fmt, ...);
#endif
