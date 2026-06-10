// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "videodisplay_util.h"

u32 vd_print_flag;

static u32 print_flag[MAX_VIDEODISPLAY_INSTANCE_NUM];
int set_print_flag(int index, u32 value)
{
	if (index >= MAX_VIDEODISPLAY_INSTANCE_NUM) {
		pr_info("%s: %d out of buf.\n", __func__, index);
		return -1;
	}

	pr_info("%s: set vd[%d] print_flag to %d.\n", __func__, index, value);
	print_flag[index] = value;
	vd_print_flag = value;
	return 0;
}

u32 get_print_flag(int index)
{
	if (index >= MAX_VIDEODISPLAY_INSTANCE_NUM) {
		pr_info("%s: %d out of buf.\n", __func__, index);
		return -1;
	}

	return print_flag[index];
}

int vd_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag[index] & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "vd:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}
