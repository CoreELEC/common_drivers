/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DPSS_LIB_H_
#define _DPSS_LIB_H_
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include "dpss_param.h"

#define DPSS_QUEEN_NUM 10

struct display_buffer_info_s {
	u8 dae_mix;
	u8 p;
	u8 c;
	u8 n;
	u8 logo_pre;
	u8 logo_cur;
	u8 mv_buf;
	u8 mc_phase;
	u32 frame_index;
};

struct display_queue {
	struct display_buffer_info_s data[DPSS_QUEEN_NUM];
	u8 drop_idx;
	u8 disp_idx;
	u8 mc_idx;
	u8 inp_idx;
};

void vpu_initqueue(struct Vpu_queue *queue);
bool vpu_is_queue_empty(struct Vpu_queue *queue);
bool vpu_enqueue(struct Vpu_queue *queue, int value);
bool vpu_dequeue(struct Vpu_queue *queue, int *value, int *empty);

void display_init_queue(struct display_queue *queue);
bool display_queue_is_empty(struct display_queue *queue);
bool display_queue_put(struct display_queue *queue);
u8 get_dst_buf_cnt(struct display_queue *queue);
void display_queue_use_last(struct display_queue *queue, u8 sel);

extern struct Vpu_queue inp_bufQ;
extern unsigned int g_SIM_FRM_NUM;
extern unsigned int g_SIM_FRM_NUM_SRC1;

#endif
