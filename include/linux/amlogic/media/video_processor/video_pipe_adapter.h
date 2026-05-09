/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_PIPE_ADAPTER_EXT_H
#define VIDEO_PIPE_ADAPTER_EXT_H

struct video_delay_arg_t {
	u32 input_fps;
	u32 output_fps;
	u32 video_delay;
};

#define VIDEO_PIPE_ADAPTER_IOC_MAGIC  'V'
#define VIDEO_PIPE_ADAPTER_GET_VIDEO_DELAY	\
		_IOWR(VIDEO_PIPE_ADAPTER_IOC_MAGIC, 0x01, struct video_delay_arg_t)

void video_keeper_keep_mem(void *mem_handle, int type, int *id);
void video_keeper_free_mem(int keep_id, int delayms);
#endif
