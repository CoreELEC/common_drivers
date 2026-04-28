/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef V2D_UTIL_H
#define V2D_UTIL_H

#define PRINT_ERROR		0X0
#define PRINT_QUEUE_STATUS	0X01
#define PRINT_FENCE		0X02
#define PRINT_PERFORMANCE	0X04
#define PRINT_AXIS		0X08
#define PRINT_INDEX_DISP	0X10
#define PRINT_PATTERN	        0X20
#define PRINT_OTHER		0X040
#define PRINT_DEWARP		0X0100
#define PRINT_VICP		0x0200

/*src format from hwc*/
enum v2d_src_buffer_format_t {
	V2D_SRC_NV21 = 0,
	V2D_SRC_YUV444 = 1,
	V2D_SRC_YUV444_10BIT = 2,
	V2D_SRC_NV12 = 3,
};

int v2d_print(int index, int debug_flag, const char *fmt, ...);
#endif
