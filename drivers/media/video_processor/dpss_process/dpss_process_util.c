// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "dpss_process_util.h"

static u32 print_flag[DPSS_INSTANCE_COUNT];
int set_dp_print_flag(int index, u32 value)
{
	if (index >= DPSS_INSTANCE_COUNT) {
		pr_info("%s: %d out of buf.\n", __func__, index);
		return -1;
	}

	pr_info("%s: set vd[%d] print_flag to %d.\n", __func__, index, value);
	print_flag[index] = value;
	return 0;
}

u32 get_dp_print_flag(int index)
{
	if (index >= DPSS_INSTANCE_COUNT) {
		pr_info("%s: %d out of buf.\n", __func__, index);
		return -1;
	}

	return print_flag[index];
}

int dp_print(int index, int debug_flag, const char *fmt, ...)
{
	if ((print_flag[index] & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "dp:[%d]", index);
		vsnprintf(buf + len, 256 - len, fmt, args);
		if (debug_flag == PRINT_ERROR)
			pr_err("%s", buf);
		else
			pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}
