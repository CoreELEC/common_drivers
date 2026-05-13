/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_VENC_H__
#define __DPTX_VENC_H__
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/types.h>

#define DPTX_DUAL_PORT_L_R        0
#define DPTX_DUAL_PORT_R_L        1
#define DPTX_DUAL_PORT_O_E        2
#define DPTX_DUAL_PORT_E_O        3

#define DPTX_WAIT_VSYNC_TIMEOUT    50000

struct edptx_venc_op_s {
	void (*wait_vsync)(struct edptx_drv_s *edp);
	u32  (*get_max_lcnt)(struct edptx_drv_s *edp);
	int  (*venc_debug_test)(struct edptx_drv_s *edp, u8 num);
	void (*venc_set_timing)(struct edptx_drv_s *edp);
	void (*venc_set)(struct edptx_drv_s *edp);
	void (*venc_switch)(struct edptx_drv_s *edp, u8 flag);
	void (*mute_set)(struct edptx_drv_s *edp, u8 flag);
	u32  (*get_encl_line_cnt)(struct edptx_drv_s *edp);
};

void edptx_venc_op_init_t7(struct edptx_venc_op_s *venc_op);
void edptx_venc_op_init_a9(struct edptx_venc_op_s *venc_op);

#endif
