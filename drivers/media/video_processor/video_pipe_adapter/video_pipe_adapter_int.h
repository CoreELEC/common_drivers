/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_PIPE_ADAPTER_INT_H
#define VIDEO_PIPE_ADAPTER_INT_H

#define MAX_KEEP_FRAME 64

struct keep_mem_info {
	void *handle;
	int keep_id;
	int count;
};

struct keeper_mgr {
	/*lock*/
	spinlock_t lock;
	struct keep_mem_info keep_list[MAX_KEEP_FRAME];
};

#endif
