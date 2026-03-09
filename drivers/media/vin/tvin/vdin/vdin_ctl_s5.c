// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/sched/clock.h>

#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_hdr.h"
#include "vdin_hw.h"

#define VDIN_V_SHRINK_H_LIMIT 1280
#define VDIN_MAX_H_ACTIVE 4096	/*the max h active of vdin*/

//static unsigned int vdin0_afbce_debug_force;
static unsigned int vdin_luma_max;

//static unsigned int vpu_reg_27af = 0x3;

/***************************Local defines**********************************/

#define VDIN_MUX_NULL                   0

#define VDIN_MAP_Y_G                    0
#define VDIN_MAP_BPB                    1
#define VDIN_MAP_RCR                    2

#define MEAS_MUX_NULL                   0
#define MEAS_MUX_656                    1
#define MEAS_MUX_TVFE                   2
#define MEAS_MUX_CVD2                   3
#define MEAS_MUX_HDMI                   4
#define MEAS_MUX_DVIN                   5
#define MEAS_MUX_DTV                    6
#define MEAS_MUX_ISP                    8
#define MEAS_MUX_656_B                  9
#define MEAS_MUX_VIU1                   6
#define MEAS_MUX_VIU2                   8

#define HDMI_DE_REPEAT_DONE_FLAG	0xF0
#define DECIMATION_REAL_RANGE		0x0F

/***************************Local Structures**********************************/
static struct vdin_matrix_lup_s vdin_matrix_lup[] = {
	{0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x0400200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV601 */
	/* 0     0.257  0.504  0.098     16 */
	/* 0    -0.148 -0.291  0.439    128 */
	/* 0     0.439 -0.368 -0.071    128 */
	{0x00000000, 0x00000000, 0x01070204, 0x00641f68, 0x1ed601c2, 0x01c21e87,
		0x00001fb7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_GBR_YUV601 */
	/* 0	    0.504  0.098  0.257     16 */
	/* 0      -0.291  0.439 -0.148    128 */
	/* 0	   -0.368 -0.071  0.439    128 */
	{0x00000000, 0x00000000, 0x02040064, 0x01071ed6, 0x01c21f68, 0x1e871fb7,
		0x000001c2, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_BRG_YUV601 */
	/* 0	    0.098  0.257  0.504     16 */
	/* 0       0.439 -0.148 -0.291    128 */
	/* 0      -0.071  0.439 -0.368    128 */
	{0x00000000, 0x00000000, 0x00640107, 0x020401c2, 0x1f681ed6, 0x1fb701c2,
		0x00001e87, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGB */
	/* -16     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x066204a8, 0x1e701cbf, 0x04a80812,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_GBR */
	/* -16     1.164 -0.391 -0.813      0 */
	/* -128     1.164  2.018  0	     0 */
	/* -128     1.164  0	  1.596      0 */
	{0x07c00600, 0x00000600, 0x04a81e70, 0x1cbf04a8, 0x08120000, 0x04a80000,
		0x00000662, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV601_BRG */
	/* -16     1.164  2.018  0          0 */
	/* -128     1.164  0      1.596      0 */
	/* -128     1.164 -0.391 -0.813      0 */
	{0x07c00600, 0x00000600, 0x04a80812, 0x000004a8, 0x00000662, 0x04a81e70,
		0x00001cbf, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV601F */
	/* 0     0.299  0.587  0.114      0 */
	/* 0    -0.169 -0.331  0.5      128 */
	/* 0     0.5   -0.419 -0.081    128 */
	{0x00000000, 0x00000000, 0x01320259, 0x00751f53, 0x1ead0200, 0x02001e53,
		0x00001fad, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGB */
	/* 0     1      0      1.402      0 */
	/* -128     1     -0.344 -0.714      0 */
	/* -128     1      1.772  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x059c0400, 0x1ea01d25, 0x04000717,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV601 */
	/* -16     0.299  0.587  0.114     16 */
	/* -16    -0.173 -0.339  0.511    128 */
	/* -16     0.511 -0.429 -0.083    128 */
	{0x07c007c0, 0x000007c0, 0x01320259, 0x00751f4f, 0x1ea5020b, 0x020b1e49,
		0x00001fab, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_RGBS */
	/* -16     1      0      1.371     16 */
	/* -128     1     -0.336 -0.698     16 */
	/* -128     1      1.733  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x057c0400, 0x1ea81d35, 0x040006ef,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV601F */
	/* -16     0.348  0.683  0.133      0 */
	/* -16    -0.197 -0.385  0.582    128 */
	/* -16     0.582 -0.488 -0.094    128 */
	{0x07c007c0, 0x000007c0, 0x016402bb, 0x00881f36, 0x1e760254, 0x02541e0c,
		0x00001fa0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_RGBS */
	/* 0     0.859  0      1.204     16 */
	/* -128     0.859 -0.295 -0.613     16 */
	/* -128     0.859  1.522  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x04d10370, 0x1ed21d8c, 0x03700617,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV601F_YUV601 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV601F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGB_YUV709 */
	/* 0     0.183  0.614  0.062     16 */
	/* 0    -0.101 -0.338  0.439    128 */
	/* 0     0.439 -0.399 -0.04     128 */
	{0x00000000, 0x00000000, 0x00bb0275, 0x003f1f99, 0x1ea601c2, 0x01c21e67,
		0x00001fd7, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGB */
	/* -16     1.164  0      1.793      0 */
	/* -128     1.164 -0.213 -0.534      0 */
	/* -128     1.164  2.115  0          0 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x072c04a8, 0x1f261ddd, 0x04a80876,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_GBR */
	/* -16	1.164 -0.213 -0.534	 0 */
	/* -128	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	{0x07c00600, 0x00000600, 0x04a81f26, 0x1ddd04a8, 0x08760000, 0x04a80000,
		0x0000072c, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_YUV709_BRG */
	/* -16	1.164  2.115  0	 0 */
	/* -128	1.164  0      1.793	 0 */
	/* -128	1.164 -0.213 -0.534	 0 */
	{0x07c00600, 0x00000600, 0x04a80876, 0x000004a8, 0x0000072c, 0x04a81f26,
		0x00001ddd, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_YUV709F */
	/* 0     0.213  0.715  0.072      0 */
	/* 0    -0.115 -0.385  0.5      128 */
	/* 0     0.5   -0.454 -0.046    128 */
	{0x00000000, 0x00000000, 0x00da02dc, 0x004a1f8a, 0x1e760200, 0x02001e2f,
		0x00001fd1, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGB */
	/* 0     1      0      1.575      0 */
	/* -128     1     -0.187 -0.468      0 */
	/* -128     1      1.856  0          0 */
	{0x00000600, 0x00000600, 0x04000000, 0x064d0400, 0x1f411e21, 0x0400076d,
		0x00000000, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGBS_YUV709 */
	/* -16     0.213  0.715  0.072     16 */
	/* -16    -0.118 -0.394  0.511    128 */
	/* -16     0.511 -0.464 -0.047    128 */
	{0x07c007c0, 0x000007c0, 0x00da02dc, 0x004a1f87, 0x1e6d020b, 0x020b1e25,
		0x00001fd0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_RGBS */
	/* -16     1      0      1.54      16 */
	/* -128     1     -0.183 -0.459     16 */
	/* -128     1      1.816  0         16 */
	{0x07c00600, 0x00000600, 0x04000000, 0x06290400, 0x1f451e2a, 0x04000744,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_RGBS_YUV709F */
	/* -16     0.248  0.833  0.084      0 */
	/* -16    -0.134 -0.448  0.582    128 */
	/* -16     0.582 -0.529 -0.054    128 */
	{0x07c007c0, 0x000007c0, 0x00fe0355, 0x00561f77, 0x1e350254, 0x02541de2,
		0x00001fc9, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_RGBS */
	/* 0     0.859  0      1.353     16 */
	/* -128     0.859 -0.161 -0.402     16 */
	/* -128     0.859  1.594  0         16 */
	{0x00000600, 0x00000600, 0x03700000, 0x05690370, 0x1f5b1e64, 0x03700660,
		0x00000000, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_YUV709F_YUV709 */
	/* 0     0.859  0      0         16 */
	/* -128     0      0.878  0        128 */
	/* -128     0      0      0.878    128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV709F */
	/* -16     1.164  0      0          0 */
	/* -128     0      1.138  0        128 */
	/* -128     0      0      1.138    128 */
	{0x07c00600, 0x00000600, 0x04a80000, 0x00000000, 0x048d0000, 0x00000000,
		0x0000048d, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709 */
	/* -16     1     -0.115 -0.207     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x07c00600, 0x00000600, 0x04001f8a, 0x1f2c0000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601 */
	/* -16     1      0.100  0.192     16 */
	/* -128     0      0.990 -0.110    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x07c00600, 0x00000600, 0x04000066, 0x00c50000, 0x03f61f8f, 0x00001fb6,
		0x000003f0, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601_YUV709F */
	/* -16     1.164 -0.134 -0.241      0 */
	/* -128     0      1.160  0.129    128 */
	/* -128     0      0.085  1.167    128 */
	{0x07c00600, 0x00000600, 0x04a81f77, 0x1f090000, 0x04a40084, 0x00000057,
		0x000004ab, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601 */
	/* 0     0.859  0.088  0.169     16 */
	/* -128     0      0.869 -0.097    128 */
	/* -128     0     -0.063  0.864    128 */
	{0x00000600, 0x00000600, 0x0370005a, 0x00ad0000, 0x037a1f9d, 0x00001fbf,
		0x00000375, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709 */
	/* 0     0.859 -0.101 -0.182     16 */
	/* -128     0      0.894  0.100    128 */
	/* -128     0      0.066  0.900    128 */
	{0x00000600, 0x00000600, 0x03701f99, 0x1f460000, 0x03930066, 0x00000044,
		0x0000039a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709_YUV601F */
	/* -16     1.164  0.116  0.223      0 */
	/* -128     0      1.128 -0.126    128 */
	/* -128     0     -0.082  1.120    128 */
	{0x07c00600, 0x00000600, 0x04a80077, 0x00e40000, 0x04831f7f, 0x00001fac,
		0x0000047b, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_YUV601F_YUV709F */
	/* 0     1     -0.118 -0.212     16 */
	/* -128     0      1.018  0.114    128 */
	/* -128     0      0.075  1.025    128 */
	{0x00000600, 0x00000600, 0x04001f87, 0x1f270000, 0x04120075, 0x0000004d,
		0x0000041a, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV709F_YUV601F */
	/* 0     1      0.102  0.196      0 */
	/* -128     0      0.990 -0.111    128 */
	/* -128     0     -0.072  0.984    128 */
	{0x00000600, 0x00000600, 0x04000068, 0x00c90000, 0x03f61f8e, 0x00001fb6,
		0x000003f0, 0x00000200, 0x00000200,},
	/* VDIN_MATRIX_RGBS_RGB */
	/* -16     1.164  0      0          0 */
	/* -16     0      1.164  0          0 */
	/* -16     0      0      1.164      0 */
	{0x07c007c0, 0x000007c0, 0x04a80000, 0x00000000, 0x04a80000, 0x00000000,
		0x000004a8, 0x00000000, 0x00000000,},
	/* VDIN_MATRIX_RGB_RGBS */
	/* 0     0.859  0      0         16 */
	/* 0     0      0.859  0         16 */
	/* 0     0      0      0.859     16 */
	{0x00000000, 0x00000000, 0x03700000, 0x00000000, 0x03700000, 0x00000000,
		0x00000370, 0x00400040, 0x00000040,},
	/* VDIN_MATRIX_2020RGB_YUV2020 */
	/* 0	 0.224732	0.580008  0.050729	 16 */
	/* 0	-0.122176 -0.315324  0.437500	128 */
	/* 0	 0.437500 -0.402312 -0.035188	128 */
	{0x00000000, 0x00000000, 0x00e60252, 0x00341f84, 0x1ebe01c0, 0x01c01e65,
		0x00001fdd, 0x00400200, 0x00000200,},
	/* VDIN_MATRIX_YUV2020F_YUV2020 */
	/* 0 0.859 0 0 16 */
	/* -128 0 0.878 0 128 */
	/* -128 0 0 0.878 128 */
	{0x00000600, 0x00000600, 0x03700000, 0x00000000, 0x03830000, 0x00000000,
		0x00000383, 0x00400200, 0x00000200,},
};

/***************************Local function**********************************/

/*get prob of r/g/b
 *	r 9:0
 *	g 19:10
 *	b 29:20
 */
void vdin_prob_get_rgb_s5(unsigned int offset,
		       unsigned int *r, unsigned int *g, unsigned int *b)
{
	*b = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT2_PROBE_COLOR_BIT,
				  COMPONENT0_PROBE_COLOR_WID);
	*g = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT1_PROBE_COLOR_BIT,
				  COMPONENT1_PROBE_COLOR_WID);
	*r = rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
				  COMPONENT0_PROBE_COLOR_BIT,
				  COMPONENT0_PROBE_COLOR_WID);
}

void vdin_prob_get_yuv_s5(unsigned int offset,
			unsigned int *y,
			unsigned int *u,
			unsigned int *v)
{
	*v = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT2_PROBE_COLOR_BIT,
			      COMPONENT2_PROBE_COLOR_WID) << 8) >> 10);
	*u = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT1_PROBE_COLOR_BIT,
			      COMPONENT1_PROBE_COLOR_WID) << 8) >> 10);
	*y = ((rd_bits(offset, VDIN_MAT0_PROBE_COLOR,
			      COMPONENT0_PROBE_COLOR_BIT,
			      COMPONENT0_PROBE_COLOR_WID) << 8) >> 10);
}

void vdin_prob_matrix_sel_s5(unsigned int offset,
			  unsigned int sel, struct vdin_dev_s *devp)
{
	unsigned int x;

	x = sel & 0x03;
	/* 1:select matrix 1 */
	wr_bits(offset, VDIN_MAT0_CTRL, x, 1, 1);
}

/* this function set flowing parameters:
 *a.devp->debug.rgb_info_x	b.devp->debug.rgb_info_y
 *debug usage:
 *echo rgb_xy x y > /sys/class/vdin/vdinx/attr
 */
void vdin_prob_set_xy_s5(unsigned int offset,
		      unsigned int x, unsigned int y, struct vdin_dev_s *devp)
{
	/* set position */
	devp->debug.rgb_info_x = x;
	if (devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		devp->debug.rgb_info_y = y / 2;
	else
		devp->debug.rgb_info_y = y;

	/* #if defined(VDIN_V1) */
	wr_bits(offset, VDIN_MAT0_PROBE_POS, devp->debug.rgb_info_y,
		PROBE_POX_Y_BIT, PROBE_POX_Y_WID);
	wr_bits(offset, VDIN_MAT0_PROBE_POS, devp->debug.rgb_info_x,
		PROBE_POS_X_BIT, PROBE_POS_X_WID);
}

/*function:
 *	1.set meas mux based on port_:
 *		0x01: /mpeg/			0x10: /CVBS/
 *		0x02: /bt656/			0x20: /SVIDEO/
 *		0x04: /VGA/				0x40: /hdmi/
 *		0x08: /COMPONENT/		0x80: /dvin/
 *		0xc0:/viu/				0x100:/dtv mipi/
 *		0x200:/isp/
 *	2.set VDIN_MEAS in accumulation mode
 *	3.set VPP_VDO_MEAS in accumulation mode
 *	4.set VPP_MEAS in latch-on-falling-edge mode
 *	5.set VDIN_MEAS mux
 *	6.manual reset VDIN_MEAS & VPP_VDO_MEAS at the same time
 */
static void vdin_set_meas_mux_s5(unsigned int offset, enum tvin_port_e port_,
			      enum bt_path_e bt_path)
{
	/* unsigned int offset = devp->addr_offset; */
	unsigned int meas_mux = MEAS_MUX_NULL;

	if (is_meson_s5_cpu()) {
		return;
	}
	switch ((port_) >> 8) {
	case 0x01: /* mpeg */
		meas_mux = MEAS_MUX_NULL;
		break;
	case 0x02: /* bt656 , txl and txlx do not support bt656 */
		if ((is_meson_gxbb_cpu() || is_meson_gxtvbb_cpu()) &&
		    bt_path == BT_PATH_GPIO_B) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			meas_mux = MEAS_MUX_656_B;
#endif
		} else if ((is_meson_gxl_cpu() || is_meson_gxm_cpu() ||
			cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) &&
			(bt_path == BT_PATH_GPIO)) {
			meas_mux = MEAS_MUX_656;
		} else {
			pr_info("cpu not define or do not support  bt656");
		}
		break;
	case 0x04: /* VGA */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x08: /* COMPONENT */
		meas_mux = MEAS_MUX_TVFE;
		break;
	case 0x10: /* CVBS */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x20: /* SVIDEO */
		meas_mux = MEAS_MUX_CVD2;
		break;
	case 0x40: /* hdmi */
		meas_mux = MEAS_MUX_HDMI;
		break;
	case 0x80: /* dvin */
		meas_mux = MEAS_MUX_DVIN;
		break;
	case 0xa0:/* viu */
		meas_mux = MEAS_MUX_VIU1;
		break;
	case 0xc0:/* viu */
		meas_mux = MEAS_MUX_VIU2;
		break;
	case 0x100:/* dtv mipi */
		meas_mux = MEAS_MUX_DTV;
		break;
	case 0x200:/* isp */
		meas_mux = MEAS_MUX_ISP;
		break;
	default:
		meas_mux = MEAS_MUX_NULL;
		break;
	}
	/* set VDIN_MEAS in accumulation mode */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1,
		MEAS_VS_TOTAL_CNT_EN_BIT, MEAS_VS_TOTAL_CNT_EN_WID);
	/* set VDIN_MEAS mux */
	wr_bits(offset, VDIN_MEAS_CTRL0, meas_mux,
		MEAS_HS_VS_SEL_BIT, MEAS_HS_VS_SEL_WID);
	/* manual reset VDIN_MEAS,
	 * rst = 1 & 0
	 */
	wr_bits(offset, VDIN_MEAS_CTRL0, 1, MEAS_RST_BIT, MEAS_RST_WID);
	wr_bits(offset, VDIN_MEAS_CTRL0, 0, MEAS_RST_BIT, MEAS_RST_WID);
}

/*function:set VDIN_COM_CTRL0
 *Bit 3:0 vdin selection,
 *	1: mpeg_in from dram, 2: bt656 input,3: component input
 *	4: tv decoder input, 5: hdmi rx input,6: digital video input,
 *	7: loopback from Viu1, 8: MIPI.
 */
/* Bit 7:6, component0 output switch,
 *	00: select component0 in,01: select component1 in,
 *	10: select component2 in
 */
/* Bit 9:8, component1 output switch,
 *	00: select component0 in,
 *	01: select component1 in, 10: select component2 in
 */
/* Bit 11:10, component2 output switch,
 *	00: select component0 in, 01: select component1 in,
 *	10: select component2 in
 */

/* attention:new add for bt656
 * 0x02: /bt656/
	a.BT_PATH_GPIO:	gxl & gxm & g12a
	b.BT_PATH_GPIO_B:gxtvbb & gxbb
	c.txl and txlx don't support bt656
 */
void vdin_set_top_s5(struct vdin_dev_s *devp, enum tvin_port_e port,
		  enum tvin_color_fmt_e input_cfmt, enum bt_path_e bt_path)
{
	unsigned int offset = devp->addr_offset;
	unsigned int vdin_mux = VDIN_MUX_NULL;
	unsigned int vdin_data_bus_0 = VDIN_MAP_Y_G;
	unsigned int vdin_data_bus_1 = VDIN_MAP_BPB;
	unsigned int vdin_data_bus_2 = VDIN_MAP_RCR;

	pr_info("%s %d:port:%d,input_cfmt:%d\n",
		__func__, __LINE__, port, input_cfmt);

	wr(offset, VDIN_TOP_SECURE1_MAX_SIZE, (3840 << 16) | (2160 << 0));
	wr(offset, VDIN_IF_TOP_SIZE, (devp->h_active_org << 16) | (devp->v_active_org << 0));
	switch (port >> 8) {
	case 0x01: /* mpeg */
		vdin_mux = VDIN_VDI0_MPEG;
		wr(offset, VDIN_IF_TOP_MPEG_CTRL, 0xe4);
		break;
	case 0x02: /* first bt656 */
		vdin_mux = VDIN_VDI1_BT656;
		wr(offset, VDIN_IF_TOP_VDI1_CTRL, 0xe4);
		break;
	case 0x04: /* reserved */
		vdin_mux = VDIN_VDI2_RESERVED;
		wr(offset, VDIN_IF_TOP_VDI2_CTRL, 0xe4);
		break;
	case 0x08: /* tv decode */
		vdin_mux = VDIN_VDI3_TV_DECODE_IN;
		wr(offset, VDIN_IF_TOP_VDI3_CTRL, 0xe4);
		break;
	case 0x10:
		break;
	case 0x20: /* digital video */
		vdin_mux = VDIN_VDI5_DVI;
		wr(offset, VDIN_IF_TOP_VDI5_CTRL, 0xe4);
		break;
	case 0x40: /* hdmi rx *//* first internal loopback */
		vdin_mux = VDIN_VDI4_HDMIRX;
		wr(offset, VDIN_IF_TOP_VDI4_CTRL, 0xe4);
		break;
	case 0x80: /* mipi csi2 */
		vdin_mux = VDIN_VDI7_MIPI_CSI2;
		wr(offset, VDIN_IF_TOP_VDI7_CTRL, 0xe4);
		break;
	case 0xa0:/* viu1 */
		if (port >= TVIN_PORT_VIU1_WB0_VD1 && port <= TVIN_PORT_VIU1_WB0_POST_BLEND)
			vdin_mux = VDIN_VDI6_LOOPBACK_1;
		else if ((port >= TVIN_PORT_VIU1_WB1_VD1) &&
			 (port <= TVIN_PORT_VIU1_WB1_POST_BLEND))
			vdin_mux = VDIN_VDI8_LOOPBACK_2;
		wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0xe4);
		//wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0xe4, 0, 8);
		break;
	case 0xc0: /* viu2 */
		//vdin_mux = VDIN_MUX_VIU1_WB1;
		//wr(offset, VDIN_IF_TOP_VDI9_CTRL, 0xe4);
		break;
	case 0xe0: /* venc0 */
		vdin_mux = VDIN_VDI6_LOOPBACK_1;
		wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0xf4);
		break;
	default:
		vdin_mux = VDIN_MUX_NULL;
		break;
	}

	switch (input_cfmt) {
	case TVIN_YVYU422:
	//case TVIN_YUV444:
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_BPB;
		break;
	case TVIN_UYVY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	case TVIN_VYUY422:
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_RCR;
		vdin_data_bus_2 = VDIN_MAP_Y_G;
		break;
	case TVIN_RGB444:
		/*RGB mapping*/
		if (devp->input_swap ^ devp->debug.input_swap) {
			vdin_data_bus_0 = VDIN_MAP_RCR;
			vdin_data_bus_1 = VDIN_MAP_BPB;
			vdin_data_bus_2 = VDIN_MAP_Y_G;
		}
		break;
	default:
		break;
	}
	if (vdin_dv_is_need_tunnel(devp)) {
		vdin_data_bus_0 = VDIN_MAP_BPB;
		vdin_data_bus_1 = VDIN_MAP_Y_G;
		vdin_data_bus_2 = VDIN_MAP_RCR;
	}
	pr_info("%s %d:vdin_mux:%d\n", __func__, __LINE__, vdin_mux);

	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_0,
		PP_COMP0_SWAP_BIT, PP_COMP0_SWAP_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_1,
		PP_COMP1_SWAP_BIT, PP_COMP1_SWAP_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, vdin_data_bus_2,
		PP_COMP2_SWAP_BIT, PP_COMP2_SWAP_WID);

	wr(offset, VDIN_PP_TOP_IN_SIZE, devp->h_active_org << 16 | devp->v_active_org);
	wr(offset, VDIN_PP_TOP_OUT_SIZE, devp->h_active << 16 | devp->v_active);

	wr_bits(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, vdin_mux,
		IF_VDIN1_SEL_BIT, IF_VDIN1_SEL_WID);
	//wr_bits(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, 1, IF_VDIN1_EN_BIT, IF_VDIN1_EN_WID);
	wr_bits(offset, VDIN_IF_TOP_VDIN1_CTRL, 1, 0, 1); /* reg_vdin1_enable */
}

/*this function will set the bellow parameters of devp:
 *1.h_active
 *2.v_active
 */
void vdin_set_decimation_s5(struct vdin_dev_s *devp)
{
	//unsigned int offset = devp->addr_offset;
	unsigned int new_clk = 0;
	bool decimation_in_frontend = false;

	if (devp->prop.decimation_ratio & HDMI_DE_REPEAT_DONE_FLAG) {
		decimation_in_frontend = true;
		if (devp->debug.vdin_ctl_dbg)
			pr_info("decimation_in_frontend\n");
	}
	devp->prop.decimation_ratio = devp->prop.decimation_ratio &
			DECIMATION_REAL_RANGE;

	new_clk = devp->fmt_info_p->pixel_clk /
			(devp->prop.decimation_ratio + 1);

	if (devp->debug.vdin_ctl_dbg)
		pr_info("%s decimation_ratio=%u,new_clk=%u.h:%d,v:%d\n",
			__func__, devp->prop.decimation_ratio, new_clk,
			devp->h_active, devp->v_active);
	devp->h_active = devp->fmt_info_p->h_active /
			(devp->prop.decimation_ratio + 1);
	devp->v_active = devp->fmt_info_p->v_active;

	devp->h_active_org = devp->h_active;
	devp->v_active_org = devp->v_active;

	/* output_width_m1 */
	//wr_bits(offset, VDIN_INTF_WIDTHM1, (devp->h_active - 1),
	//	VDIN_INTF_WIDTHM1_BIT, VDIN_INTF_WIDTHM1_WID);
	if (devp->debug.vdin_ctl_dbg)
		pr_info("%s: h_active=%u, v_active=%u\n",
			__func__, devp->h_active, devp->v_active);
}

void vdin_cfg_cutwin_regs_s5(struct vdin_dev_s *devp,
	unsigned int rdma_enable, struct tvin_cutwin_s *cutwin_s)
{
	bool cutwin_en;
	unsigned int offset = devp->addr_offset;

	cutwin_en = (cutwin_s->hs || cutwin_s->he || cutwin_s->vs || cutwin_s->ve);
	/* Enable vdin1 cut window always for write done checking */
	if (devp->hw_core == VDIN_HW_CORE_LITE)
		cutwin_en = true;
	//update cut window
	wr(offset, VDIN_PP_TOP_H_WIN,
		(cutwin_s->hs << INPUT_WIN_H_START_BIT) |
		(cutwin_s->he << INPUT_WIN_H_END_BIT));
	wr(offset, VDIN_PP_TOP_V_WIN,
		(cutwin_s->vs << INPUT_WIN_V_START_BIT) |
		(cutwin_s->ve << INPUT_WIN_V_END_BIT));
	wr_bits(offset, VDIN_PP_TOP_CTRL, cutwin_en,
		PP_WIN_EN_BIT, PP_WIN_EN_WID);
	//update wrmif
	wr_bits(offset, VDIN_WRMIF_H_START_END,
		(devp->h_active - 1), WR_HEND_BIT, WR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_WRMIF_V_START_END,
		(devp->v_active - 1), WR_VEND_BIT, WR_VEND_WID);
}

void vdin_change_matrix0_s5(u32 offset, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_MAT0_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_MAT0_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_MAT0_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_MAT0_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_MAT0_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_MAT0_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_MAT0_COEF22,    matrix_tbl->coef22);
		wr(offset, VDIN_MAT0_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_MAT0_OFFSET2,   matrix_tbl->post_offset2);
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr_bits(offset, VDIN_PP_TOP_CTRL, 1,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
	}
}

/* hdr2 used as only matrix,not hdr matrix
 * should use hdr matrix related registers
 * VDIN_MAT1_XX have no effect
 */
void vdin_change_matrix1_s5(u32 offset, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT1_EN_BIT, PP_MAT1_EN_WID);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_HDR2_MATRIXI_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_HDR2_MATRIXI_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_HDR2_MATRIXI_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_HDR2_MATRIXI_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_HDR2_MATRIXI_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_HDR2_MATRIXI_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_HDR2_MATRIXI_COEF22, matrix_tbl->coef22);
		wr(offset, VDIN_HDR2_MATRIXI_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_HDR2_MATRIXI_OFFSET2, matrix_tbl->post_offset2);
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
		wr(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 1);/* rgb2yuv or yuv2rgb */
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 16, 1);// hdr2 used as matrix
		wr_bits(offset, VDIN_PP_TOP_CTRL, 1,
			PP_MAT1_EN_BIT, PP_MAT1_EN_WID);
	}
}

/*
 * after, equal g12a have this matrix
 */
void vdin_change_matrix_hdr_s5(struct vdin_dev_s *devp, u32 matrix_csc)
{
	struct vdin_matrix_lup_s *matrix_tbl;
	u32 offset = devp->addr_offset;

	if (matrix_csc == VDIN_MATRIX_NULL)	{
		wr_bits(offset, VDIN_PP_TOP_CTRL, 0,
			PP_MAT0_EN_BIT, PP_MAT0_EN_WID);
		wr_bits(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 0, 0, 8);
	} else {
		matrix_tbl = &vdin_matrix_lup[matrix_csc - 1];

		/*coefficient index select matrix0*/
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX_COEF_INDEX_BIT, VDIN_MATRIX_COEF_INDEX_WID);

		wr(offset, VDIN_PP_HDR2_MATRIXI_PRE_OFFSET0_1, matrix_tbl->pre_offset0_1);
		wr(offset, VDIN_PP_HDR2_MATRIXI_PRE_OFFSET2, matrix_tbl->pre_offset2);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF00_01, matrix_tbl->coef00_01);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF02_10, matrix_tbl->coef02_10);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF11_12, matrix_tbl->coef11_12);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF20_21, matrix_tbl->coef20_21);
		wr(offset, VDIN_PP_HDR2_MATRIXI_COEF22,    matrix_tbl->coef22);
		wr(offset, VDIN_PP_HDR2_MATRIXI_OFFSET0_1, matrix_tbl->post_offset0_1);
		wr(offset, VDIN_PP_HDR2_MATRIXI_OFFSET2,   matrix_tbl->post_offset2);
		wr_bits(offset, VDIN_PP_HDR2_MATRIXI_EN_CTRL, 1, 0, 8);
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 13, 1);//VDIN0_HDR2_top_en
		wr_bits(offset, VDIN_PP_HDR2_CTRL, 1,
			PP_MATRIXI_TOP_EN_BIT, PP_MATRIXI_TOP_EN_WID);
		if (vdin_is_convert_to_rgb(devp->format_convert)) {
			wr_bits(offset, VDIN_PP_HDR2_MATRIXO_EN_CTRL, 0, 0, 1);
			wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 17, 1);
		} else if (vdin_is_convert_to_yuv(devp->format_convert)) {
			wr_bits(offset, VDIN_PP_HDR2_MATRIXO_EN_CTRL, 1, 0, 1);
			wr_bits(offset, VDIN_PP_HDR2_CTRL, 0, 17, 1);
		}
		if (vdin_is_rgb_input(devp->prop.color_format))
			wr_bits(offset, VDIN_PP_HDR2_CTRL, 0, 4, 1);
		else
			wr_bits(offset, VDIN_PP_HDR2_CTRL, 1, 4, 1);
		//wr_bits(offset, VDIN_MATRIX_CTRL, 0,
		//	VDIN_MATRIX0_BYPASS_BIT, VDIN_MATRIX0_BYPASS_WID);
	}
}

static enum vdin_matrix_csc_e
vdin_set_color_matrix_s5(struct vdin_dev_s *devp, enum vdin_matrix_sel_e matrix_sel,
				enum vdin_format_convert_e format_convert)
{
	enum vdin_matrix_csc_e    matrix_csc = VDIN_MATRIX_NULL;
	/*struct vdin_matrix_lup_s *matrix_tbl;*/
	struct tvin_format_s *fmt_info = devp->fmt_info_p;
	unsigned int vdin_hdr_flag = devp->prop.vdin_hdr_flag | devp->dv.dv_flag;

	if (!fmt_info) {
		pr_err("%s fmt_info NULL\n", __func__);
		return VDIN_MATRIX_NULL;
	}

	switch (format_convert)	{
	case VDIN_MATRIX_XXX_YUV_BLACK:
		matrix_csc = VDIN_MATRIX_XXX_YUV601_BLACK;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		if (IS_HDMI_SRC(devp->parm.port)) {
			if (devp->color_range_mode == 1) {
				if (devp->prop.color_fmt_range == TVIN_RGB_FULL) {
					matrix_csc = VDIN_MATRIX_RGB_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc =
						VDIN_MATRIX_RGBS_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGBS_YUV709;
				}
			} else {
				if (devp->prop.color_fmt_range == TVIN_RGB_FULL) {
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB2020_YUV2020;
					else
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
				}
			}
		} else {
			if (devp->color_range_mode == 1)
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_RGB_YUV709;
		}
		break;
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
		matrix_csc = VDIN_MATRIX_GBR_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		matrix_csc = VDIN_MATRIX_BRG_YUV601;
		break;
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		if (IS_HDMI_SRC(devp->parm.port)) {
			if (devp->color_range_mode == 1) {
				if (devp->prop.color_fmt_range == TVIN_RGB_FULL) {
					matrix_csc = VDIN_MATRIX_RGB_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709F;
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGBS_YUV709;
				}
			} else {
				if (devp->prop.color_fmt_range == TVIN_RGB_FULL) {
					if (vdin_hdr_flag == 1)
						matrix_csc =
						VDIN_MATRIX_RGB2020_YUV2020;
					else
						matrix_csc =
						VDIN_MATRIX_RGB_YUV709F;
				} else {
					matrix_csc = VDIN_MATRIX_RGBS_YUV709;
				}
			}
		} else {
			if (devp->color_range_mode == 1)
				matrix_csc = VDIN_MATRIX_RGB_YUV709F;
			else
				matrix_csc = VDIN_MATRIX_RGB_YUV709;
		}
		break;
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))  /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_RGB;
		else
			matrix_csc = VDIN_MATRIX_YUV601_RGB;
		break;
	case VDIN_FORMAT_CONVERT_YUV_GBR:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))    /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_GBR;
		else
			matrix_csc = VDIN_MATRIX_YUV601_GBR;
		break;
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540))    /* 1080i & above */
			matrix_csc = VDIN_MATRIX_YUV709_BRG;
		else
			matrix_csc = VDIN_MATRIX_YUV601_BRG;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		if ((fmt_info->scan_mode == TVIN_SCAN_MODE_PROGRESSIVE &&
		     fmt_info->v_active >= 720) || /* 720p & above */
		    (fmt_info->scan_mode == TVIN_SCAN_MODE_INTERLACED &&
		     fmt_info->v_active >= 540)) { /* 1080i & above */

			if (devp->color_range_mode == 1 &&
			    devp->prop.color_fmt_range != TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709_YUV709F;
			else if (devp->color_range_mode == 0 &&
				devp->prop.color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV709F_YUV709;
		} else {
			if (devp->color_range_mode == 1) {
				if (devp->prop.color_fmt_range == TVIN_YUV_FULL)
					matrix_csc =
						VDIN_MATRIX_YUV601F_YUV709F;
				else
					matrix_csc = VDIN_MATRIX_YUV601_YUV709F;
			} else {
				if (devp->prop.color_fmt_range == TVIN_YUV_FULL)
					matrix_csc = VDIN_MATRIX_YUV601F_YUV709;
				else
					matrix_csc = VDIN_MATRIX_YUV601_YUV709;
			}
		}
		if (vdin_hdr_flag == 1) {
			if (devp->prop.color_fmt_range == TVIN_YUV_FULL)
				matrix_csc = VDIN_MATRIX_YUV2020F_YUV2020;
			else
				matrix_csc = VDIN_MATRIX_NULL;
		}
		break;
	default:
		matrix_csc = VDIN_MATRIX_NULL;
		break;
	}

	vdin_manual_matrix_csc(devp, &matrix_csc);

	if (vdin_select_hdr_matrix(devp))
		matrix_sel = VDIN_SEL_MATRIX_HDR;

	if (devp->debug.matrix_sel < VDIN_SEL_MATRIX_MAX)
		matrix_sel = devp->debug.matrix_sel;

	if (matrix_sel == VDIN_SEL_MATRIX0)
		vdin_change_matrix0_s5(devp->addr_offset, matrix_csc);
	else if (matrix_sel == VDIN_SEL_MATRIX1)
		vdin_change_matrix1_s5(devp->addr_offset, matrix_csc);
	else if (matrix_sel == VDIN_SEL_MATRIX_HDR)
		vdin_change_matrix_hdr_s5(devp, matrix_csc);
	else
		matrix_csc = VDIN_MATRIX_NULL;

	if (devp->debug.vdin_ctl_dbg)
		pr_info("vdin%d set matrix:%d, csc:%d, fmt:%dx%d, m:%d, c:%d, r=%d %d, hdr:%d\n",
			devp->index, matrix_sel, matrix_csc,
			fmt_info->h_active, fmt_info->v_active, fmt_info->scan_mode,
			devp->format_convert,
			devp->color_range_mode, devp->prop.color_fmt_range,
			vdin_hdr_flag);

	return matrix_csc;
}

void vdin_set_hdr_s5(struct vdin_dev_s *devp)
{
	enum vd_format_e video_format;

	if (devp->index == 0 || !devp->dtdata->vdin1_set_hdr ||
		devp->debug.vdin1_set_hdr_bypass)
		return;

	video_format = vdin_get_tx_format(devp);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification()) {
		/*not modify data*/
		return;
	}
#endif

#ifndef VDIN_BRINGUP_NO_AMLVECM
	pr_info("%s fmt:%d\n", __func__, video_format);
	switch (video_format) {
	case SIGNAL_HDR10:
	case SIGNAL_HDR10PLUS:
		/* parameters:
		 * 1st, module sel: 6=VDIN0_HDR, 7=VDIN1_HDR
		 * 2nd, process sel: bit1=HDR_SDR
		 * bit11=HDR10P_SDR
		 */
		hdr_set(VDIN1_HDR, HDR_SDR, VPP_TOP0);
		break;

	case SIGNAL_SDR:
		/* HDR_BYPASS(bit0) | HLG_BYPASS(bit3) */
		hdr_set(VDIN1_HDR, HDR_BYPASS | HLG_BYPASS, VPP_TOP0);
		break;

	case SIGNAL_HLG:
		/* HLG_SDR(bit4) */
		hdr_set(VDIN1_HDR, HLG_SDR, VPP_TOP0);
		break;

	/* VDIN DON'T support dv loopback currently */
	case SIGNAL_DOVI:
		pr_err("err:  don't support dv signal loopback");
		break;

	default:
		break;
	}
#endif
	/* hdr set uses rdma, will delay 1 frame to take effect */
	devp->frame_drop_num = 1;
}

/*set matrix based on rgb_info_enable:
 * 0:set matrix0, disable matrix1
 * 1:set matrix1, set matrix0
 * after equal g12a: matrix1, matrix_hdr2
 */
void vdin_set_matrix_s5(struct vdin_dev_s *devp)
{
//	enum vdin_format_convert_e format_convert_matrix0;
//	enum vdin_format_convert_e format_convert_matrix1;
//	unsigned int offset = devp->addr_offset;
	enum vdin_matrix_sel_e matrix_sel;

	if (devp->debug.rgb_info_enable == 0) {
		//TODO:Kernel panic
		vdin_set_hdr_s5(devp);
		/* matrix1 disable */
//	wr_bits(offset, VDIN_MATRIX_CTRL, 0,
//		VDIN_MATRIX1_EN_BIT, VDIN_MATRIX1_EN_WID);
//		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
//			matrix_sel = VDIN_SEL_MATRIX1;/*VDIN_SEL_MATRIX_HDR*/
//		} else {
//			matrix_sel = VDIN_SEL_MATRIX0;
//		}

		if (devp->debug.dbg_sel_mat & 0x10)
			matrix_sel = devp->debug.dbg_sel_mat & 0x10;
		else
			matrix_sel = VDIN_SEL_MATRIX0;/*VDIN_SEL_MATRIX_HDR*/

		if (!devp->dv.dv_flag) {
			devp->csc_idx = vdin_set_color_matrix_s5(devp,
						matrix_sel, devp->format_convert);
		} else {
			devp->csc_idx = VDIN_MATRIX_NULL;
		}
//		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
//		if (vdin_is_dolby_signal_in(devp) ||
//		    devp->parm.info.fmt == TVIN_SIG_FMT_CVBS_SECAM)
//			wr_bits(offset, VDIN_MATRIX_CTRL, 0,
//				VDIN_MATRIX_EN_BIT, VDIN_MATRIX_EN_WID);
//		#endif
//		wr_bits(offset, VDIN_MATRIX_CTRL, 3,
//			VDIN_PROBE_SEL_BIT, VDIN_PROBE_SEL_WID);
	}

	if (devp->matrix_pattern_mode)
		vdin_set_matrix_color_s5(devp);
}

/*et histogram window
 * pow\h_start\h_end\v_start\v_end
 */
static inline void vdin_set_histogram_s5(unsigned int offset, unsigned int hs,
				      unsigned int he, unsigned int vs,
				      unsigned int ve)
{
	//unsigned int pixel_sum = 0, record_len = 0, hist_pow = 0;
	//pr_info("%s %d:hs=%d,he=%d,vs=%d,ve=%d\n",
	//	__func__, __LINE__, hs, he, vs, ve);
	if (hs < he && vs < ve) {
//		pixel_sum = (he - hs + 1) * (ve - vs + 1);
//		record_len = 0xffff << 3;
//		while (pixel_sum > record_len && hist_pow < 3) {
//			hist_pow++;
//			record_len <<= 1;
//		}
//		/* #ifdef CONFIG_MESON2_CHIP */
//		/* pow */
//		wr_bits(offset, VDIN_HIST_CTRL, hist_pow,
//			HIST_POW_BIT, HIST_POW_WID);
		/* win_hs */
		wr_bits(offset, VDIN_LUMA_HIST_H_START_END, hs,
			HIST_HSTART_BIT, HIST_HSTART_WID);
		/* win_he */
		wr_bits(offset, VDIN_LUMA_HIST_H_START_END, he,
			HIST_HEND_BIT, HIST_HEND_WID);
		/* win_vs */
		wr_bits(offset, VDIN_LUMA_HIST_V_START_END, vs,
			HIST_VSTART_BIT, HIST_VSTART_WID);
		/* win_ve */
		wr_bits(offset, VDIN_LUMA_HIST_V_START_END, ve,
			HIST_VEND_BIT, HIST_VEND_WID);
	}
}

/* set hist mux
 * hist_spl_sel, 0: win; 1: mat0; 2: vsc; 3: hdr2_mat1
 */
static inline void vdin_set_hist_mux_s5(struct vdin_dev_s *devp)
{
	enum tvin_port_e port = TVIN_PORT_NULL;

	port = devp->parm.port;
	/*if ((port < TVIN_PORT_HDMI0) || (port > TVIN_PORT_HDMI7))*/
	/*	return;*/
	/* For AV input no correct data, from vlsi fei.jun
	 * AV, HDMI all set 3
	 */
	/* use 11: form matrix1 din */
//	wr_bits(devp->addr_offset, VDIN_HIST_CTRL, 3,
//		HIST_HIST_DIN_SEL_BIT, HIST_HIST_DIN_SEL_WID);
	wr_bits(devp->addr_offset, VDIN_LUMA_HIST_CTRL, 0x7f, 24, 8);//reg_luma_hist_pix_white_th
	wr_bits(devp->addr_offset, VDIN_LUMA_HIST_CTRL, 0x7f, 16, 8);//reg_luma_hist_pix_black_th
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 0, 12, 2);//reg_hist_spl_sel
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 1, 7, 1);//reg_hist_spl_en

	/*for project get vdin1 hist*/
	//if (devp->index == 1)
	// wr_bits(devp->addr_offset, VDIN_WR_CTRL2, 1, 8, 1);
}

/* urgent ctr config */
/* if vdin fifo over up_th,will trigger increase
 *  urgent responds to vdin write,
 * if vdin fifo lower dn_th,will trigger decrease
 *  urgent responds to vdin write
 */
static void vdin_urgent_patch_s5(unsigned int offset, unsigned int v,
			      unsigned int h)
{
	if (h >= 1920 && v >= 1080) {
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 1,
			VDIN_LFIFO_URG_CTRL_EN_BIT, VDIN_LFIFO_URG_CTRL_EN_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 1,
			VDIN_LFIFO_URG_WR_EN_BIT, VDIN_LFIFO_URG_WR_EN_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 20,
			VDIN_LFIFO_URG_UP_TH_BIT, VDIN_LFIFO_URG_UP_TH_WID);
		wr_bits(offset, VDIN_LFIFO_URG_CTRL, 8,
			VDIN_LFIFO_URG_DN_TH_BIT, VDIN_LFIFO_URG_DN_TH_WID);
		/*vlsi guys suggest setting:*/
		W_VCBUS_BIT(VPU_ARB_URG_CTRL, 1,
			    VDIN_LFF_URG_CTRL_BIT, VDIN_LFF_URG_CTRL_WID);
		W_VCBUS_BIT(VPU_ARB_URG_CTRL, 1,
			    VPP_OFF_URG_CTRL_BIT, VPP_OFF_URG_CTRL_WID);
	} else {
		wr(offset, VDIN_LFIFO_URG_CTRL, 0);
		aml_write_vcbus(VPU_ARB_URG_CTRL, 0);
	}
}

static unsigned int vdin_is_support_10bit_for_dw_s5(struct vdin_dev_s *devp)
{
	if (devp->double_wr) {
		if (devp->double_wr_10bit_sup)
			return 1;
		else
			return 0;
	} else {
		return 1;
	}
}

/*static unsigned int vdin_wr_mode = 0xff;*/
/*module_param(vdin_wr_mode, uint, 0644);*/
/*MODULE_PARM_DESC(vdin_wr_mode, "vdin_wr_mode");*/

/* set write ctrl regs:
 * VDIN_WR_H_START_END
 * VDIN_WR_V_START_END
 * VDIN_WR_CTRL
 * VDIN_LFIFO_URG_CTRL
 */
static inline void vdin_set_wr_ctrl_s5(struct vdin_dev_s *devp,
				    unsigned int offset, unsigned int v,
				    unsigned int h,
				    enum vdin_format_convert_e format_convert,
				    unsigned int full_pack,
				    unsigned int source_bitdepth)
{
	enum vdin_mif_fmt write_fmt = MIF_FMT_YUV422;
	unsigned int swap_cbcr = 0;
	unsigned int hconv_mode = 2;

	if (devp->vf_mem_size_small) {
		h = devp->h_shrink_out;
		v = devp->v_shrink_out;
	}

	pr_info("%s,%d.h=%d,v=%d\n", __func__, __LINE__, h, v);
	switch (format_convert)	{
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		write_fmt = MIF_FMT_YUV422;
		break;

	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 1;
		break;

	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		write_fmt = MIF_FMT_NV12_21;
		swap_cbcr = 0;
		break;

	default:
		write_fmt = MIF_FMT_YUV444;
		break;
	}

	/* yuv422 full pack mode for 10bit
	 * only support 8bit at vpp side when double write
	 */
	if ((format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422) &&
	    full_pack == VDIN_422_FULL_PK_EN &&
	    source_bitdepth > VDIN_COLOR_DEEPS_8BIT &&
	    vdin_is_support_10bit_for_dw_s5(devp)) {
		write_fmt = MIF_FMT_YUV422_FULL_PACK;

		/* IC bug, fixed at tm2 revB */
		if (devp->dtdata->hw_ver == VDIN_HW_ORG)
			hconv_mode = 0;
	}

	/* win_he */
	if ((h % 2) && devp->source_bitdepth > VDIN_COLOR_DEEPS_8BIT &&
	    devp->full_pack == VDIN_422_FULL_PK_EN &&
	    (devp->format_convert == VDIN_FORMAT_CONVERT_YUV_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_RGB_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_GBR_YUV422 ||
	     devp->format_convert == VDIN_FORMAT_CONVERT_BRG_YUV422))
		h += 1;

	wr(offset, VDIN_DW_TOP_IN_SIZE,
		devp->s5_data.h_scale_out << 16 | devp->s5_data.v_scale_out << 0);
	wr(offset, VDIN_DW_TOP_OUT_SIZE, devp->h_shrink_out << 16 | devp->v_shrink_out << 0);
	wr_bits(offset, VDIN_WRMIF_H_START_END, (devp->h_shrink_out - 1), WR_HEND_BIT, WR_HEND_WID);
	/* win_ve */
	wr_bits(offset, VDIN_WRMIF_V_START_END, (devp->v_shrink_out - 1), WR_VEND_BIT, WR_VEND_WID);
	/* hconv_mode */
	wr_bits(offset, VDIN_WRMIF_CTRL, hconv_mode, 26, 2);
	/* vconv_mode */
	/* output even lines's cbcr */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, 28, 2);

	if (write_fmt == MIF_FMT_NV12_21) {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, swap_cbcr, 30, 1);
	} else if (write_fmt == MIF_FMT_YUV444) {
		/* output all cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, 3, 28, 2);
	} else {
		/* swap_cbcr */
		wr_bits(offset, VDIN_WRMIF_CTRL, 0, 30, 1);
		/* chroma canvas */
		/* wr_bits(offset, VDIN_WR_CTRL2, 0,
		 *  WRITE_CHROMA_CANVAS_ADDR_BIT,
		 */
		/* WRITE_CHROMA_CANVAS_ADDR_WID); */
	}

	/* format444 */
	wr_bits(offset, VDIN_WRMIF_CTRL, write_fmt, WR_FMT_BIT, WR_FMT_WID);
	/* req_urgent */
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, WR_REQ_URGENT_BIT, WR_REQ_URGENT_WID);
	/* req_en */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);

	/*only for vdin0*/
	if (devp->dts_config.urgent_en && devp->index == 0)
		vdin_urgent_patch_s5(offset, v, h);
	/* dis ctrl reg w pulse */
	/*if (is_meson_g9tv_cpu() || is_meson_m8_cpu() ||
	 *	is_meson_m8m2_cpu() || is_meson_gxbb_cpu() ||
	 *	is_meson_m8b_cpu())
	 */
	//wr_bits(offset, VDIN_WR_CTRL, 1,
	//	VDIN_WR_CTRL_REG_PAUSE_BIT, VDIN_WR_CTRL_REG_PAUSE_WID);
	/*  swap the 2 64bits word in 128 words */
	/*if (is_meson_gxbb_cpu())*/
	if ((devp->wr_endian || devp->cfg_dma_buf ||
		devp->work_mode == VDIN_WORK_MD_V4L) ^ devp->dbg_no_swap_en) {
		/*not swap 2 64bits words in 128 words */
		wr_bits(offset, VDIN_WRMIF_CTRL, 0, 31, 1);
		/*little endian*/
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 25, 1);
	} else {
		wr_bits(offset, VDIN_WRMIF_CTRL, 1, 31, 1);
	}
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, 19, 1);
}

void vdin_set_mif_on_off_s5(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
	unsigned int offset = devp->addr_offset;

	if (devp->vframe_wr_en_pre == devp->vframe_wr_en)
		return;

	#if CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle, VDIN_PP_TOP_CTRL + offset,
			devp->vframe_wr_en ? 0 : 1, 21, 1);
		rdma_write_reg_bits(devp->rdma_handle, VDIN_WRMIF_CTRL + offset,
				    devp->vframe_wr_en, WR_REQ_EN_BIT,
				    WR_REQ_EN_WID);
	}
	#endif
	devp->vframe_wr_en_pre = devp->vframe_wr_en;
}

/***************************global function**********************************/

unsigned int vdin_get_active_h_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_ACTIVE_MAX_PIX_CNT_STATUS,
		       ACTIVE_MAX_PIX_CNT_SDW_BIT, ACTIVE_MAX_PIX_CNT_SDW_WID);
}

unsigned int vdin_get_active_v_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_LINE_CNT_SHADOW_STATUS,
		       ACTIVE_LN_CNT_SDW_BIT, ACTIVE_LN_CNT_SDW_WID);
}

unsigned int vdin_get_total_v_s5(unsigned int offset)
{
	return rd_bits(offset, VDIN_IF_TOP_VDIN1_LINE_CNT_SHADOW_STATUS,
		       GO_LN_CNT_SDW_BIT, GO_LN_CNT_SDW_WID);
}

void vdin_set_frame_mif_write_addr_s5(struct vdin_dev_s *devp,
			unsigned int rdma_enable,
			struct vf_entry *vfe)
{
	u32 stride_luma, stride_chroma = 0;
	u32 hsize;
	u32 phy_addr_luma = 0, phy_addr_chroma = 0;

	if (devp->vf_mem_size_small)
		hsize = devp->h_shrink_out;
	else
		hsize = devp->h_active;

	stride_luma = devp->canvas_w >> 4;
	if (vfe->vf.plane_num == 2)
		stride_chroma = devp->canvas_w >> 4;

	/* one region mode only have one buffer RGB,YUV */
	phy_addr_luma = vfe->vf.canvas0_config[0].phy_addr;
	/* two region mode have Y and uv two buffer(NV12, NV21) */
	if (vfe->vf.plane_num == 2)
		phy_addr_chroma = vfe->vf.canvas0_config[1].phy_addr;

	if (devp->debug.vdin_isr_monitor & VDIN_ISR_MONITOR_BUFFER) {
		pr_info("vdin%d,phy addr luma:0x%x chroma:0x%x\n", devp->index,
			phy_addr_luma, phy_addr_chroma);
		pr_info("vf[%d],stride luma:0x%x, chroma:0x%x\n", vfe->vf.index,
			stride_luma, stride_chroma);
	}
	/*if (vdin_ctl_dbg) {*/
	/*	pr_info("mif fmt:0x%x (0:422,1:444,2:NV21) bit:0x%x h:%d\n",*/
	/*		devp->mif_fmt, devp->source_bitdepth, hsize);*/
	/*	pr_info("phy addr luma:0x%x chroma:0x%x\n",*/
	/*		phy_addr_luma, phy_addr_chroma);*/
	/*	pr_info("stride luma:0x%x, chroma:0x%x\n",*/
	/*		stride_luma, stride_chroma);*/
	/*}*/

	if (rdma_enable) {
		rdma_write_reg(devp->rdma_handle,
			       VDIN_WRMIF_BADDR_LUMA + devp->addr_offset,
			       phy_addr_luma >> 4);
		rdma_write_reg(devp->rdma_handle,
			       VDIN_WRMIF_STRIDE_LUMA + devp->addr_offset,
			       stride_luma);

		if (vfe->vf.plane_num == 2) {
			rdma_write_reg(devp->rdma_handle,
				       VDIN_WRMIF_BADDR_CHROMA + devp->addr_offset,
				       phy_addr_chroma >> 4);
			rdma_write_reg(devp->rdma_handle,
				       VDIN_WRMIF_STRIDE_CHROMA + devp->addr_offset,
				       stride_chroma);
		}
		if (devp->pause_dec)
			rdma_write_reg_bits(devp->rdma_handle,
				VDIN_WRMIF_CTRL + devp->addr_offset,
				0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			rdma_write_reg_bits(devp->rdma_handle,
				VDIN_WRMIF_CTRL + devp->addr_offset,
				1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	} else {
		wr(devp->addr_offset, VDIN_WRMIF_BADDR_LUMA, phy_addr_luma >> 4);
		wr(devp->addr_offset, VDIN_WRMIF_STRIDE_LUMA, stride_luma);

		if (vfe->vf.plane_num == 2) {
			wr(devp->addr_offset, VDIN_WRMIF_BADDR_CHROMA,
			   phy_addr_chroma >> 4);
			wr(devp->addr_offset, VDIN_WRMIF_STRIDE_CHROMA,
			   stride_chroma);
		}
		if (devp->pause_dec)
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL,
				0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
		else
			wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL,
				1, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	}
}

//static unsigned int vdin_luma_max;

void vdin_set_vframe_prop_info_s5(struct vframe_s *vf,
			       struct vdin_dev_s *devp)
{
	int i;
	unsigned int offset = devp->addr_offset;
	u64 divid;
	struct vframe_bbar_s bbar = {0};
//
//#ifdef CONFIG_AML_LOCAL_DIMMING
//	/*int i;*/
//#endif
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,
	 * HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   );
	 */
//	vf->prop.hist.hist_pow   = rd_bits(offset, VDIN_HIST_CTRL,
//					   HIST_POW_BIT, HIST_POW_WID);
	vf->prop.hist.luma_sum   = rd(offset, VDIN_LUMA_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,
	 * HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID );
	 */
	vf->prop.hist.chroma_sum = rd(offset, VDIN_LUMA_HIST_CHROMA_SUM);
	vf->prop.hist.pixel_sum  = rd(offset, VDIN_LUMA_HIST_SPL_CNT) & 0xffffff;
	vf->prop.hist.height     =
		rd_bits(offset, VDIN_LUMA_HIST_V_START_END, HIST_VEND_BIT, HIST_VEND_WID) -
		rd_bits(offset, VDIN_LUMA_HIST_V_START_END, HIST_VSTART_BIT, HIST_VSTART_WID) + 1;
	vf->prop.hist.width      =
		rd_bits(offset, VDIN_LUMA_HIST_H_START_END, HIST_HEND_BIT, HIST_HEND_WID) -
		rd_bits(offset, VDIN_LUMA_HIST_H_START_END, HIST_HSTART_BIT, HIST_HSTART_WID) + 1;
	vf->prop.hist.luma_max   = rd_bits(offset, VDIN_LUMA_HIST_MAX_MIN,
								HIST_MAX_BIT, HIST_MAX_WID);
	vf->prop.hist.luma_min   = rd_bits(offset, VDIN_LUMA_HIST_MAX_MIN,
								HIST_MIN_BIT, HIST_MIN_WID);
	for (i = 0; i < 64; i++) {
		wr(offset, VDIN_LUMA_HIST_DNLP_IDX, i);
		vf->prop.hist.gamma[i]	 = rd(offset, VDIN_LUMA_HIST_DNLP_GRP);
	}

	/* fetch bbar info */
	bbar.top = rd_bits(offset, VDIN_PP_BLKBAR_STATUS0,
			   BLKBAR_TOP_POS_BIT, BLKBAR_TOP_POS_WID);
	bbar.bottom = rd_bits(offset, VDIN_PP_BLKBAR_STATUS0,
			      BLKBAR_BTM_POS_BIT,   BLKBAR_BTM_POS_WID);
	bbar.left = rd_bits(offset, VDIN_PP_BLKBAR_STATUS1,
			    BLKBAR_LEFT_POS_BIT, BLKBAR_LEFT_POS_WID);
	bbar.right = rd_bits(offset, VDIN_PP_BLKBAR_STATUS1,
			     BLKBAR_RIGHT_POS_BIT, BLKBAR_RIGHT_POS_WID);
	if (bbar.top > bbar.bottom) {
		bbar.top = 0;
		bbar.bottom = vf->height - 1;
	}
	if (bbar.left > bbar.right) {
		bbar.left = 0;
		bbar.right = vf->width - 1;
	}

	/* Update Histgram windown with detected BlackBar window */
	if (devp->hist_bar_enable)
		vdin_set_histogram_s5(offset, 0, vf->width - 1, 0, vf->height - 1);
	else
		vdin_set_histogram_s5(offset, bbar.left, vf->width - 1 - bbar.right,
				   bbar.top, vf->height - 1 - bbar.bottom);

	if (devp->black_bar_enable) {
		vf->prop.bbar.top        = bbar.top;
		vf->prop.bbar.bottom     = bbar.bottom;
		vf->prop.bbar.left       = bbar.left;
		vf->prop.bbar.right      = bbar.right;
	} else {
		memset(&vf->prop.bbar, 0, sizeof(struct vframe_bbar_s));
	}

	/* fetch meas info - For M2 or further chips only, not for M1 chip */
	vf->prop.meas.vs_stamp = devp->stamp;
	vf->prop.meas.vs_cycle = devp->cycle;
	if ((devp->debug.vdin_ctl_dbg & CTL_DEBUG_LUMA_MAX) &&
	    vdin_luma_max != vf->prop.hist.luma_max) {
		vf->ready_clock_hist[0] = sched_clock();
		divid = vf->ready_clock_hist[0];
		do_div(divid, 1000);
		pr_info("vdin write done %lld us. lum_max(0x%x-->0x%x)\n",
			divid, vdin_luma_max, vf->prop.hist.luma_max);
		vdin_luma_max = vf->prop.hist.luma_max;
	}
}

void vdin_dlg_update_hist_hv_s5(unsigned int temp_hist_width, unsigned int temp_hist_height)
{
	struct vdin_dev_s *devp_vdin1 = vdin_get_dev(1);
	unsigned int hist_offset;

	/* s5 no vdin0 */
	hist_offset = devp_vdin1->addr_offset;

	/*  win_h */
	wr_bits(hist_offset, VDIN_LUMA_HIST_H_START_END, temp_hist_width - 1,
		HIST_HEND_BIT, HIST_HEND_WID);
	/* win_v */
	wr_bits(hist_offset, VDIN_LUMA_HIST_V_START_END, temp_hist_height - 1,
		HIST_VEND_BIT, HIST_VEND_WID);
}

void vdin_get_hist_val_s5(struct vdin_dev_s *devp, struct vdin_hist_s *vdin1_hist_temp)
{
	unsigned int offset = devp->addr_offset;

	vdin1_hist_temp->width = (rd(offset, VDIN_LUMA_HIST_H_START_END) & 0x1fff) + 1;
	vdin1_hist_temp->height = (rd(offset, VDIN_LUMA_HIST_V_START_END) & 0x1fff) + 1;
	vdin1_hist_temp->sum =  rd(offset, VDIN_LUMA_HIST_SPL_VAL);
}

void vdin_hist_init_s5(struct vdin_dev_s *devp)
{
	//todo
}

void vdin_set_all_regs_s5(struct vdin_dev_s *devp)
{
	/* matrix sub-module */
	vdin_set_matrix_s5(devp);

	/* bbar sub-module */
	//vdin_set_bbar_s5(devp->addr_offset, devp->v_active, devp->h_active);

	/* hist sub-module */
	//vdin_set_histogram_s5(devp->addr_offset, 0,
	//		   devp->h_active - 1, 0, devp->v_active - 1);
	/* hist mux select */
	vdin_set_hist_mux_s5(devp);
	/* write sub-module */
	//vdin_set_wr_ctrl_s5(devp, devp->addr_offset, devp->v_active,
	//		 devp->h_active, devp->format_convert,
	//		 devp->full_pack, devp->source_bitdepth);
	vdin_set_wr_ctrl_s5(devp, devp->addr_offset, devp->v_active,
			devp->h_active, devp->format_convert,
			devp->full_pack, devp->source_bitdepth);
	/* top sub-module */
	vdin_set_top_s5(devp, devp->parm.port,
		     devp->prop.color_format,
		     devp->bt_path);

	vdin_set_meas_mux_s5(devp->addr_offset, devp->parm.port,
			  devp->bt_path);

//	/* for t7 vdin2 write meta data */
//	if (devp->dtdata->hw_ver == VDIN_HW_T7) {
//		vdin_wrmif2_initial(devp);
//		vdin_wrmif2_addr_update(devp);
//		vdin_wrmif2_enable(devp, 0);
//	}
}

static void vdin_delay_line_s5(struct vdin_dev_s *devp, unsigned int num)
{
	if (devp->hw_core == VDIN_HW_CORE_LITE)
		return;

	wr_bits(devp->addr_offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, num,
		DLY_GO_FIELD_LINES_BIT, DLY_GO_FIELD_LINES_WID);
	if (num)
		wr_bits(devp->addr_offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 1,
			DLY_GO_FIELD_EN_BIT, DLY_GO_FIELD_EN_WID);
	else
		wr_bits(devp->addr_offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 0,
			DLY_GO_FIELD_EN_BIT, DLY_GO_FIELD_EN_WID);
}

void vdin_if_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;

	offset = devp->addr_offset;

	wr(offset, VDIN_IF_TOP_MPEG_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI1_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI2_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI3_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI4_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI5_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI6_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI7_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI8_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_VDI9_CTRL, 0x0);

	wr(offset, VDIN_IF_TOP_SIZE, 0x0);
	wr(offset, VDIN_IF_TOP_DECIMATE_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_BUF_CTRL, 0x0);

	wr(offset, VDIN_IF_TOP_MEAS_CTRL, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_FIRST_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_SECOND_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_THIRD_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_MEAS_FOURTH_RANGE, 0x0);
	wr(offset, VDIN_IF_TOP_PTGEN_CTRL, 0x0);

	if (devp->index == 0) {
		wr(offset, VDIN_IF_TOP_VDIN0_SECURE_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN0_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN0_SYNC_CTRL0, 0x1);
		wr(offset, VDIN_IF_TOP_VDIN0_SYNC_CTRL1, 0x4040);
		wr(offset, VDIN_IF_TOP_VDIN0_LINE_INT_CTRL, 0x0);
	} else {
		//wr(offset, VDIN_IF_TOP_VDIN1_SECURE_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN1_CTRL, 0x0);
		wr(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL0, 0x1);
		wr(offset, VDIN_IF_TOP_VDIN1_SYNC_CTRL1, 0x4040);
		wr(offset, VDIN_IF_TOP_VDIN1_LINE_INT_CTRL, 0x0);
	}
}

void vdin_pp_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;
	unsigned int lfifo_buf_size;

	offset = devp->addr_offset;
	if (devp->index)
		lfifo_buf_size = devp->dtdata->vdin1_line_buff_size;
	else
		lfifo_buf_size = devp->dtdata->vdin0_line_buff_size;
	//wr(offset, VDIN_PP_TOP_GCLK_CTRL,	0x0);
	wr(offset, VDIN_PP_TOP_CTRL,		0x90000000);
	wr(offset, VDIN_PP_TOP_IN_SIZE,		0x0);
	wr(offset, VDIN_PP_TOP_OUT_SIZE,	0x0);
	wr(offset, VDIN_PP_TOP_H_WIN,		0x1fff);
	wr(offset, VDIN_PP_TOP_V_WIN,		0x1fff);
	wr(offset, VDIN_PP_LFIFO_CTRL,
		(0 << PP_LFIFO_GCLK_CTRL_BIT) |
		(1 << PP_LFIFO_SOFT_RST_BIT) |
		(lfifo_buf_size << PP_LFIFO_BUF_SIZE_BIT) |
		(0 << PP_LFIFO_URG_CTRL_BIT));
	//udelay(5);
	//wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0, PP_LFIFO_SOFT_RST_BIT, 1);
	//wrmif
	//wr_bits(offset, VDIN_WRMIF_STRIDE_LUMA, 1, 31, 1);
}

void vdin_dw_default_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;

	offset = devp->addr_offset;
	//wr(offset, VDIN_DW_TOP_GCLK_CTRL,	0x0);
	wr(offset, VDIN_DW_TOP_CTRL,		0x0);
	wr(offset, VDIN_DW_TOP_IN_SIZE,		0x0);
	wr(offset, VDIN_DW_TOP_OUT_SIZE,	0x0);
}

void vdin_set_default_regmap_s5(struct vdin_dev_s *devp)
{
	vdin_if_default_s5(devp);
	vdin_pp_default_s5(devp);
	vdin_dw_default_s5(devp);

	vdin_delay_line_s5(devp, devp->delay_line_num);
}

void vdin_hw_enable_s5(struct vdin_dev_s *devp)
{
	//unsigned int offset = devp->addr_offset;

	/* enable video data input */
	/* [    4]  top.datapath_en  = 1 */
	//wr_bits(offset, VDIN_COM_CTRL0, 1,
	//	VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);

	vdin_clk_on_off_s5(devp, true);
	/* wr(offset, VDIN_COM_GCLK_CTRL, 0x0); */
}

/*
 * 1) disable cm2
 * 2) disable video input and set mux input to null
 * 3) set delay line number
 * 4) disable vdin clk
 */
void vdin_hw_disable_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	//unsigned int def_canvas;
	//int temp;

	pr_info("%s: vdin[%d] VDIN_PP_TOP_CTRL:%x, offset:%d\n",
		__func__, devp->index, VDIN_PP_TOP_CTRL, offset);
	//rdma write may working now! clear req_en will not take effect
	/* req_en */
	wr_bits(offset, VDIN_WRMIF_CTRL, 0, WR_REQ_EN_BIT, WR_REQ_EN_WID);
	/* req_en */
	wr_bits(offset, VDIN_IF_TOP_VDIN1_CTRL, 0, 0, 1); /* reg_vdin1_enable */
	//def_canvas = offset ? vdin_canvas_ids[1][0] : vdin_canvas_ids[0][0];
	/* disable cm2 */
	//wr(offset, VDIN_PP_TOP_CTRL, 0);
	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	/* disable video data input */
	/* [    4]  top.datapath_en  = 0 */
	//wr_bits(offset, VDIN_COM_CTRL0, 0,
	//	VDIN_COMMON_INPUT_EN_BIT, VDIN_COMMON_INPUT_EN_WID);

	/* mux null input */
	/* [ 3: 0]  top.mux  = 0/(null, mpeg, 656, tvfe, cvd2, hdmi, dvin) */
	//wr_bits(offset, VDIN_COM_CTRL0, 0, VDIN_SEL_BIT, VDIN_SEL_WID);
	//wr(offset, VDIN_COM_CTRL0, 0x00000910);
	//vdin_delay_line_s5(devp, devp->delay_line_num);
	//if (devp->enable_reset)
	//	wr(offset, VDIN_WR_CTRL, 0x0b401000 | def_canvas);
	//else
	//	wr(offset, VDIN_WR_CTRL, 0x0bc01000 | def_canvas);

	//vdin_wrmif2_enable(devp, 0);

	/* disable clock of blackbar, histogram, histogram, line fifo1, matrix,
	 * hscaler, pre hscaler, clock0
	 */
	/* [15:14]  Disable blackbar clock      = 01/(auto, off, on, on) */
	/* [13:12]  Disable histogram clock     = 01/(auto, off, on, on) */
	/* [11:10]  Disable line fifo1 clock    = 01/(auto, off, on, on) */
	/* [ 9: 8]  Disable matrix clock        = 01/(auto, off, on, on) */
	/* [ 7: 6]  Disable hscaler clock       = 01/(auto, off, on, on) */
	/* [ 5: 4]  Disable pre hscaler clock   = 01/(auto, off, on, on) */
	/* [ 3: 2]  Disable clock0              = 01/(auto, off, on, on) */
	/* [    0]  Enable register clock       = 00/(auto, off!!!!!!!!) */
	/*switch_vpu_clk_gate_vmod(offset == 0 ? VPU_VIU_VDIN0:VPU_VIU_VDIN1,
	 *VPU_CLK_GATE_OFF);
	 */
	vdin_clk_on_off_s5(devp, false);
	/* wr(offset, VDIN_COM_GCLK_CTRL, 0x5554); */

	/*if (devp->dtdata->de_tunnel_tunnel) */{
		//todo vdin_dolby_desc_to_4448bit(devp, 0);
		//todo vdin_dolby_de_tunnel_to_44410bit(devp, 0);
	}
}

void vdin_enable_module_s5(struct vdin_dev_s *devp, bool enable)
{
	//unsigned int offset = devp->addr_offset;

	//vdin_dmc_ctrl(devp, 1);
	if (enable)	{
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000100); */
		/* vdin_hw_enable_s5(devp); */
		/* todo: check them */
	} else {
		/* set VDIN_MEAS_CLK_CNTL, select XTAL clock */
		/* if (is_meson_gxbb_cpu()) */
		/* ; */
		/* else */
		/* aml_write_cbus(HHI_VDIN_MEAS_CLK_CNTL, 0x00000000); */
		vdin_hw_disable_s5(devp);
		//todo vdin_dobly_mdata_write_en(offset, false);
	}
}

bool vdin_write_done_check_s5(struct vdin_dev_s *devp)
{
	unsigned int offset;

	offset = devp->addr_offset;
	if (devp->debug.vdin_isr_monitor & VDIN_ISR_MONITOR_WRITE_DONE) {
		pr_info("vdin%d,[%#x]:%#x,[%#x]:%#x,[%#x]:%#x\n", devp->index,
			VDIN_LFIFO_BUF_COUNT, rd(0, VDIN_LFIFO_BUF_COUNT),
			VDIN_IF_TOP_VDI_INT_STATUS1, rd(0, VDIN_IF_TOP_VDI_INT_STATUS1),
			VDIN_WRMIF_RO_STATUS, rd(0, VDIN_WRMIF_RO_STATUS));
	}
	/*clear int status,reg_field_done_clr_bit */
	wr_bits(offset, VDIN_WRMIF_CTRL, 1, 18, 1);

//	/*clear int status*/
//	wr_bits(offset, VDIN_WR_CTRL, 1,
//			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
//	wr_bits(offset, VDIN_WR_CTRL, 0,
//			DIRECT_DONE_CLR_BIT, DIRECT_DONE_CLR_WID);
//
//	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
//		if (rd_bits(offset, VDIN_RO_WRMIF_STATUS,
//			    WRITE_DONE_BIT, WRITE_DONE_WID))
//			return true;
//
//	} else {
//		if (rd_bits(offset, VDIN_COM_STATUS0,
//			    DIRECT_DONE_STATUS_BIT, DIRECT_DONE_STATUS_WID))
//			return true;
//	}
//	devp->wr_done_abnormal_cnt++;
//	return false;
	return true;
}

/* just for horizontal down scale src_w is origin width,
 * dst_w is width after scale down
 */
static void vdin_set_hscale_s5(struct vdin_dev_s *devp, unsigned int dst_w)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_active;
	//unsigned int tmp;
	int horz_phase_step, i;

	u32 coef_lut[3][33] = {
		{ 0x00800000, 0x007f0100, 0xff7f0200, 0xfe7f0300,
		  0xfd7e0500, 0xfc7e0600, 0xfb7d0800, 0xfb7c0900,
		  0xfa7b0b00, 0xfa7a0dff, 0xf9790fff, 0xf97711ff,
		  0xf87613ff, 0xf87416fe, 0xf87218fe, 0xf8701afe,
		  0xf76f1dfd, 0xf76d1ffd, 0xf76b21fd, 0xf76824fd,
		  0xf76627fc, 0xf76429fc, 0xf7612cfc, 0xf75f2ffb,
		  0xf75d31fb, 0xf75a34fb, 0xf75837fa, 0xf7553afa,
		  0xf8523cfa, 0xf8503ff9, 0xf84d42f9, 0xf84a45f9,
	      0xf84848f8 }, // bicubic
		{ 0x00800000, 0x007e0200, 0x007c0400, 0x007a0600,
		  0x00780800, 0x00760a00, 0x00740c00, 0x00720e00,
		  0x00701000, 0x006e1200, 0x006c1400, 0x006a1600,
		  0x00681800, 0x00661a00, 0x00641c00, 0x00621e00,
		  0x00602000, 0x005e2200, 0x005c2400, 0x005a2600,
		  0x00582800, 0x00562a00, 0x00542c00, 0x00522e00,
		  0x00503000, 0x004e3200, 0x004c3400, 0x004a3600,
		  0x00483800, 0x00463a00, 0x00443c00, 0x00423e00,
		  0x00404000 }, // 2 point bilinear
		{ 0x80000000, 0x7e020000, 0x7c040000, 0x7a060000,
		  0x78080000, 0x760a0000, 0x740c0000, 0x720e0000,
		  0x70100000, 0x6e120000, 0x6c140000, 0x6a160000,
		  0x68180000, 0x661a0000, 0x641c0000, 0x621e0000,
		  0x60200000, 0x5e220000, 0x5c240000, 0x5a260000,
		  0x58280000, 0x562a0000, 0x542c0000, 0x522e0000,
		  0x50300000, 0x4e320000, 0x4c340000, 0x4a360000,
		  0x48380000, 0x463a0000, 0x443c0000, 0x423e0000,
		  0x40400000 }}; // 2 point bilinear, bank_length == 2

	if (!dst_w) {
		pr_err("[vdin..]%s parameter dst_w error.\n", __func__);
		return;
	}
	/* enable reg_hsc_gclk_ctrl */
	wr_bits(offset, VDIN_PP_TOP_GCLK_CTRL, 2, PP_HSC_GCLK_CTRL_BIT, PP_HSC_GCLK_CTRL_WID);
	/* disable hscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_HSC_EN_BIT, PP_HSC_EN_WID);
	/* write horz filter coefs */
	//wr_bits(offset, VDIN_HSC_COEF_IDX, 0x0100, 0, 7);
	wr(offset, VDIN_HSC_COEF_IDX, 0x0100);
	for (i = 0; i < 33; i++)
		wr(offset, VDIN_HSC_COEF, coef_lut[0][i]); /* bicubic */

	if (src_w >= 2048) {/* for src_w >= 4096, avoid data overflow. */
		horz_phase_step = ((src_w << 18) / dst_w) << 2;
		//horz_phase_step = (horz_phase_step << 6);
	} else {
		horz_phase_step = (src_w << 20) / dst_w;
		//horz_phase_step = (horz_phase_step << 4);
	}
	horz_phase_step = (horz_phase_step << 4);

	//wr(offset, VDIN_WIDTHM1I_WIDTHM1O, ((src_w - 1) << WIDTHM1I_BIT) |
	//		(dst_w  - 1));

	wr(offset, VDIN_PP_HSC_PHASE_STEP, horz_phase_step);
	wr(offset, VDIN_HSC_INI_PHASE, 0);
	wr(offset, VDIN_HSC_MISC,
		(1 << HSC_RPT_P0_NUM_BIT)  |
		(4 << HSC_INI_RCV_NUM_BIT));
	/* hsc_p0_num */
	//wr(offset, VDIN_HSC_INI_CTRL, (1 << HSCL_RPT_P0_NUM_BIT) |
	//   (4 << HSCL_INI_RCV_NUM_BIT) | /* hsc_ini_rcv_num */
	//   (0 << HSCL_INI_PHASE_BIT) /* hsc_ini_phase */
	//   );

	wr(offset, VDIN_HSC_CTRL,
	   (2 << PP_HSC_GCLK_CTL_BIT)           | /* gclk_ctrl */
	   (1 << PP_HSC_NEAREST_EN_BIT)         | /* nearest_en */
	   (1 << PP_HSC_SHORT_LINE_O_EN_BIT)    | /* short_line_o_en */
	   (0 << PP_HSC_SP422_MODE_BIT)         | /* sp422_mode */
	   (0 << PP_HSC_PHASE0_ALWASY_EN_BIT)   | /* phase0_always_en */
	   (4 << PP_HSC_BANK_LEN_BIT));           /* hsc_bank_length */
	/* enable hscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_HSC_EN_BIT, PP_HSC_EN_WID);

	devp->h_active = dst_w;
}

/*
 *just for vertical scale src_w is origin height,
 *just dst_h is the height after scale
 */
static void vdin_set_vscale_s5(struct vdin_dev_s *devp)
{
	int ver_phase_step, tmp;
	unsigned int offset = devp->addr_offset;
	unsigned int src_h = devp->v_active;
	unsigned int dst_h = devp->prop.scaling4h;

	if (!dst_h) {
		pr_err("[vdin..]%s parameter dst_h error.\n", __func__);
		return;
	}
	/* enable reg_hsc_gclk_ctrl */
	wr_bits(offset, VDIN_PP_TOP_GCLK_CTRL, 2, PP_VSC_GCLK_CTRL_BIT, PP_VSC_GCLK_CTRL_WID);
	/* disable vscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_VSC_EN_BIT, PP_VSC_EN_WID);

	if (src_h >= 2048)
		ver_phase_step = ((src_h << 18) / dst_h) << 2;
	else
		ver_phase_step = (src_h << 20) / dst_h;

	tmp = ver_phase_step >> 25;
	if (tmp) {
		pr_err("[vdin..]%s error. cannot be divided more than 31.9999.\n",
		       __func__);
		return;
	}
	wr(offset, VDIN_PP_VSC_PHASE_STEP, ver_phase_step);
	if (!(ver_phase_step >> 20)) {/* scale up the bit should be 1 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 1,
			VSC_PHASE0_ALWAYS_EN_BIT, VSC_PHASE0_ALWAYS_EN_WID);
		/* scale phase is 0 */
		wr_bits(offset, VDIN_VSC_INI_CTRL, 0,
			VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	} else {
		wr_bits(offset, VDIN_VSC_CTRL, 0,
			PP_VSC_PHASE0_ALWAS_EN_BIT, PP_VSC_PHASE0_ALWAS_EN_WID);
		wr_bits(offset, VDIN_VSC_CTRL, 1, 0, 1);/* reg_vsc_rpt_last_ln_mode */
		/* scale phase is 0x0 */
		wr_bits(offset, VDIN_VSC_INI_PHASE, 0x0,
			VSCALER_INI_PHASE_BIT, VSCALER_INI_PHASE_WID);
	}
	/* skip 0 line in the beginning */
	wr_bits(offset, VDIN_VSC_CTRL, 0,
		PP_VSC_SKIP_LINE_NUM_BIT, PP_VSC_SKIP_LINE_NUM_WID);

	//wr_bits(offset, VDIN_SCIN_HEIGHTM1, src_h - 1,
	//	SCALER_INPUT_HEIGHT_BIT, SCALER_INPUT_HEIGHT_WID);
	wr(offset, VDIN_VSC_DUMMY_DATA, 0x8080);
	/* enable vscale */
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_VSC_EN_BIT, PP_VSC_EN_WID);
	devp->v_active = dst_h;
}

/* new add pre_hscale module
 * do hscaler down when scaling4w is smaller than half of h_active
 * which is closed by default
 */
static void vdin_set_pre_h_scale_s5(struct vdin_dev_s *devp)
{
	wr_bits(devp->addr_offset, VDIN_PHSC_CTRL, 0, PP_PHSC_MODE_BIT,
		PP_PHSC_MODE_WID);
	wr_bits(devp->addr_offset, VDIN_PP_TOP_CTRL, 1, PP_PHSC_EN_BIT,
		PP_PHSC_EN_WID);
	devp->h_active >>= 1;
	pr_info("set_prehsc done!h_active:%d\n", devp->h_active);
}

/*tm2 new add*/
static void vdin_set_h_shrink_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_active;
	unsigned int dst_w = devp->h_shrink_out;
	unsigned int hshrk_mode = 0;
	unsigned int i = 0;
	unsigned int coef = 0;

	if (!cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		pr_err("vdin.%d don't supports h_shrink before TM2\n",
		       devp->index);
		return;
	}

	hshrk_mode = src_w / dst_w;

	/*check maximum value*/
	if (hshrk_mode < 2 || hshrk_mode > 64) {
		pr_err("vdin.%d h_shrink: size is out of range\n", devp->index);
		return;
	}
	pr_err("%s vdin.%d h:%d,v:%d,src:%d %d;mode:%d\n", __func__,
		   devp->index, devp->h_active, devp->v_active,
		   src_w, dst_w, hshrk_mode);
	/*check legality, valid is 2,4,8,16,32,64*/
	switch (hshrk_mode) {
	case 2:
	case 4:
	case 8:
	case 16:
	case 32:
	case 64:
		break;

	default:
		pr_err("vdin.%d invalid h_shrink mode : %d\n", devp->index,
		       hshrk_mode);
		return;
	}
//int hsk_mode = (int)(ceil((float)(vdin->vdin_dw_top.in_hsize) /
//(float)(vdin->vdin_dw_top.out_hsize)));
	coef = 64 / hshrk_mode;
	coef = (coef << 24) | (coef << 16) | (coef << 8) | coef;

	/*hshrk_mode: 2->1/2, 4->1/4... 64->1/64(max)*/
	wr_bits(offset, VDIN_DW_HSK_CTRL, hshrk_mode, DW_HSK_MODE_BIT, DW_HSK_MODE_WID);
	//wr_bits(offset, VDIN_DW_HSK_CTRL, src_w, HSK_HSIZE_IN_BIT,
	//	HSK_HSIZE_IN_WID);

	/*h shrink coef*/
	for (i = VDIN_HSK_COEF_0; i <= VDIN_HSK_COEF_15; i++)
		wr(offset, i, coef);

	/*h shrink enable*/
	//wr_bits(offset, VDIN_DW_HSK_CTRL, 1, DW_HSK_EN_BIT, DW_HSK_EN_WID);
	wr_bits(offset, VDIN_DW_TOP_CTRL, 1, DW_HSK_EN_BIT, DW_HSK_EN_WID);

	devp->h_active = devp->h_shrink_out;
	pr_info("vdin.%d set_h_shrink done! h shrink mode = %d\n", devp->index,
		hshrk_mode);
}

/* new add v_shrink module
 * do vertical scale down,when scaling4h is smaller than half of v_active
 * which is closed by default
 * vshrk_mode:0-->1:2; 1-->1:4; 2-->1:8
 * chip <= TL1, only vdin1 has this module.
 * chip >= TM2 && != T5, both vdin0 and vdin1 are supported.
 * chip == T5, only vdin0 support
 */
static void vdin_set_v_shrink_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int src_w = devp->h_shrink_out;
	unsigned int src_h = devp->v_active;
	unsigned int vshrk_mode = 0;
	int lpf_mode = 1;

	if (src_w > VDIN_V_SHRINK_H_LIMIT) {
		pr_err("vdin.%d v_shrink: only support input width <= %d\n",
		       devp->index, VDIN_V_SHRINK_H_LIMIT);
		return;
	}

	vshrk_mode = src_h / devp->v_shrink_out;

	pr_err("%s vdin.%d h:%d,v:%d,src:%d %d;mode:%d\n", __func__,
		   devp->index, devp->h_active, devp->v_active,
		   src_h, devp->v_shrink_out, vshrk_mode);

	if (vshrk_mode < 2) {
		pr_info("vdin.%d v shrink: out > in/2,return\n", devp->index);
		return;
	}

	if (vshrk_mode >= 8)
		vshrk_mode = 2;
	else if (vshrk_mode >= 4)
		vshrk_mode = 1;
	else//if (vshrk_mode >= 2)
		vshrk_mode = 0;
	// mode: 0 for 2:1, 1 for 4:1, 2 for 8:1

	pr_info("vdin.%d vshrk mode = %d\n", devp->index, vshrk_mode);

	wr(offset, VDIN_VSK_CTRL, (vshrk_mode << 2) | (lpf_mode << 1));
	wr(offset, VDIN_VSK_PTN_DATA0, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA1, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA2, 0x8080);
	wr(offset, VDIN_VSK_PTN_DATA3, 0x8080);
	wr_bits(offset, VDIN_DW_TOP_CTRL, 1, DW_VSK_EN_BIT, DW_VSK_EN_WID);
	devp->v_active = devp->v_shrink_out;
	pr_info("vdin.%d set_v_shrink done!\n", devp->index);
}

/*function:set horizontal and vertical scale
 *vdin scale down path:
 *	vdin0:pre hsc-->horizontal scaling-->v scaling;
 *	vdin1:pre hsc-->horizontal scaling-->v_shrink-->v scaling
 *for tm2, scaling path, both vdin are same:
 *	prehsc-->h scaling-->v scaling-->optional h/v shrink;
 */
void vdin_set_hv_scale_s5(struct vdin_dev_s *devp)
{
	/*backup current h v size*/
	devp->s5_data.h_scale_out = devp->h_active;
	devp->s5_data.v_scale_out = devp->v_active;

	if (K_FORCE_HV_SHRINK)
		goto set_hv_shrink;

	pr_info("vdin%d %s h_active:%u,v_active:%u.scaling:%dx%d\n", devp->index,
		__func__, devp->h_active, devp->v_active,
		devp->prop.scaling4w, devp->prop.scaling4h);

	if (devp->prop.scaling4w < devp->h_active &&
	    devp->prop.scaling4w > 0) {
		if (devp->pre_h_scale_en && (devp->prop.scaling4w <=
		    (devp->h_active >> 1)))
			vdin_set_pre_h_scale_s5(devp);

		if (devp->prop.scaling4w < devp->h_active)
			vdin_set_hscale_s5(devp, devp->prop.scaling4w);
	} else if (devp->h_active > VDIN_MAX_H_ACTIVE) {
		vdin_set_hscale_s5(devp, VDIN_MAX_H_ACTIVE);
	}

	if (devp->prop.scaling4h < devp->v_active &&
	    devp->prop.scaling4h > 0) {
		if (devp->v_shrink_en && !cpu_after_eq(MESON_CPU_MAJOR_ID_TM2) &&
		    (devp->prop.scaling4h <= (devp->v_active >> 1))) {
			vdin_set_v_shrink_s5(devp);
		}

		if (devp->prop.scaling4h < devp->v_active)
			vdin_set_vscale_s5(devp);
	}

set_hv_shrink:

	if ((devp->double_wr || K_FORCE_HV_SHRINK) &&
	    (devp->h_active > 1920 || devp->v_active > 1080)) {
		devp->h_shrink_times = H_SHRINK_TIMES_4k;
		devp->v_shrink_times = V_SHRINK_TIMES_4k;
	} else if (devp->double_wr && (devp->h_active > 720 ||
		devp->v_active > 576)) {
		devp->h_shrink_times = H_SHRINK_TIMES_1080;
		devp->v_shrink_times = V_SHRINK_TIMES_1080;
	} else {
		devp->h_shrink_times = 1;
		devp->v_shrink_times = 1;
	}
	if (devp->debug.dbg_force_shrink_en) {
		devp->h_shrink_times = H_SHRINK_TIMES_1080;
		devp->v_shrink_times = V_SHRINK_TIMES_1080;
	}
	devp->h_shrink_out = devp->h_active / devp->h_shrink_times;
	devp->v_shrink_out = devp->v_active / devp->v_shrink_times;

	if (devp->double_wr || K_FORCE_HV_SHRINK || devp->debug.dbg_force_shrink_en) {
		if (devp->h_shrink_out < devp->h_active)
			vdin_set_h_shrink_s5(devp);

		if (devp->v_shrink_out < devp->v_active)
			vdin_set_v_shrink_s5(devp);
	}

	if (devp->debug.vdin_dbg_en) {
		pr_info("[vdin.%d] %s h_active:%u,v_active:%u.\n", devp->index,
			__func__, devp->h_active, devp->v_active);
		pr_info("[vdin.%d] %s shrink out h:%d,v:%d\n", devp->index,
			__func__, devp->h_shrink_out, devp->v_shrink_out);
	}
}

/*set source_bitdepth
 *	base on color_depth_config:
 *		10, 8, 0, other
 */
void vdin_set_bitdepth_s5(struct vdin_dev_s *devp, unsigned int rdma_enable)
{
	unsigned int reg_bit_depth = 0;

	if (devp->source_bitdepth_dw == VDIN_COLOR_DEEPS_8BIT)
		reg_bit_depth = 0;
	else if (devp->source_bitdepth_dw == VDIN_COLOR_DEEPS_10BIT)
		reg_bit_depth = 1;

	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle,
			VDIN_WRMIF_CTRL2 + devp->addr_offset,
			reg_bit_depth, VDIN_WR_10BIT_MODE_BIT,	VDIN_WR_10BIT_MODE_WID);
	} else {
		wr_bits(devp->addr_offset, VDIN_WRMIF_CTRL2, reg_bit_depth,
			VDIN_WR_10BIT_MODE_BIT,	VDIN_WR_10BIT_MODE_WID);

	if (devp->debug.vdin_dbg_en)
		pr_info("%s %d cfg:0x%x prop.dep:%x\n", __func__, devp->source_bitdepth,
			devp->color_depth_config, devp->prop.colordepth);
	}
}

/* do horizontal reverse and vertical reverse
 * h reverse:
 * VDIN_WR_H_START_END
 * Bit29:	1.reverse	0.do not reverse
 *
 * v reverse:
 * VDIN_WR_V_START_END
 * Bit29:	1.reverse	0.do not reverse
 */
void vdin_wr_reverse_s5(unsigned int offset, bool h_reverse, bool v_reverse)
{
	if (h_reverse)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 21, 1);
	else
		wr_bits(offset, VDIN_WRMIF_CTRL2, 0, 21, 1);
	if (v_reverse)
		wr_bits(offset, VDIN_WRMIF_CTRL2, 1, 20, 1);
	else
		wr_bits(offset, VDIN_WRMIF_CTRL2, 0, 20, 1);
}

void vdin_set_cm2_s5(unsigned int offset, unsigned int w,
		  unsigned int h, unsigned int *cm2, bool cm_enable)
{
	unsigned int i = 0, j = 0, start_addr = 0x100;

	if (!cm_enable)
		return;
	pr_info("%s %d,w=%d,h=%d\n", __func__, __LINE__, w, h);

	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 0, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, 0, PP_CM_EN_BIT, PP_CM_EN_WID);
	for (i = 0; i < 160; i++) {
		j = i / 5;
		wr(offset, VDIN_PP_CHROMA_ADDR_PORT, start_addr + (j << 3) + (i % 5));
		wr(offset, VDIN_PP_CHROMA_DATA_PORT, cm2[i]);
	}
	for (i = 0; i < 28; i++) {
		wr(offset, VDIN_PP_CHROMA_ADDR_PORT, 0x200 + i);
		wr(offset, VDIN_PP_CHROMA_DATA_PORT, cm2[160 + i]);
	}
	/*config cm2 frame size*/
	wr(offset, VDIN_PP_CHROMA_ADDR_PORT, 0x205);
	wr(offset, VDIN_PP_CHROMA_DATA_PORT, h << 16 | w);
	//wr_bits(offset, VDIN_CM_BRI_CON_CTRL, 1, CM_TOP_EN_BIT, CM_TOP_EN_WID);
	wr_bits(offset, VDIN_PP_TOP_CTRL, 1, PP_CM_EN_BIT, PP_CM_EN_WID);
}

void vdin_dolby_mdata_write_en_s5(unsigned int offset, unsigned int en)
{
	/*printk("=========>> wr memory %d\n", en);*/
	if (en) {
		/*enable write metadata to memory*/
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 1, 30, 1);
		/*vdin0 dolby meta write enable*/
		/*W_VCBUS_BIT(0x27af, 1, 2, 1);*/
	} else {
		/*disable write metadata to memory*/
		wr_bits(offset, VDIN_DOLBY_AXI_CTRL0, 0, 30, 1);
		/*vdin0 dolby meta write disable*/
		/*W_VCBUS_BIT(0x27af, 0, 2, 1);*/
	}
}

void vdin_clk_on_off_s5(struct vdin_dev_s *devp, bool on_off)
{
	unsigned int offset = devp->addr_offset;

	pr_info("%s %d:on_off:%d\n", __func__, __LINE__, on_off);

	/* gclk_ctrl,00: auto, 01: off, 1x: on */
	if (on_off) { /* on */
		/* 8k top */
		wr(offset, VDIN_TOP_CTRL,
			(0x0 << TOP_GCLK_CTRL_BIT)        | /* 8k top_gclk */
			(0x0 << LOCAL_ARB_GCLK_CTRL_BIT)   | /* local_arb_gclk  */
			(0x0 << TIMING_MEAS_POL_CTRL_BIT) | /* timing_meas_pol */
			(0x0 << TOP_RESET_BIT));             /* reset */
		/* hist */
		wr(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x0); /* luma_hist_gclk */
		/* lfifo */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x0, 30, 2);/* gclk */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 29, 1);/* soft_rst_en */
		/* vsc */
		wr_bits(offset, VDIN_VSC_CTRL, 0x0, 28, 4);/* gclk */
		/* hsc */
		wr_bits(offset, VDIN_HSC_CTRL, 0x0, 30, 2);/* gclk */
		/* phsc */
		wr_bits(offset, VDIN_PHSC_CTRL, 0x0, 30, 2);/* gclk */
		/* mat1 */
		wr_bits(offset, VDIN_MAT1_CTRL, 0x0, 30, 2);/* gclk */
		/* mat0 */
		wr_bits(offset, VDIN_MAT0_CTRL, 0x0, 30, 2);/* gclk */
		/* hist */
		wr_bits(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x0, 30, 2);/* gclk */
		/* pp top */
		wr(offset, VDIN_PP_TOP_GCLK_CTRL, (0x0 << 0)); /* pp top_gclk */
		/* vsk */
		wr_bits(offset, VDIN_VSK_CTRL, 0x0, 28, 4);/* gclk */
		/* hsk */
		wr_bits(offset, VDIN_HSK_CTRL, 0x0, 30, 2);/* gclk */
		/* dw top */
		wr(offset, VDIN_DW_TOP_GCLK_CTRL, (0x0 << 0)); /* dw top_gclk */
		/* hdr2 */
		wr(offset, VDIN_PP_HDR2_CLK_GATE, 0x0);/* hdr2 gclk */
		/* if top buf */
		wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0x0, 30, 2); /* buf_gclk */
		/* local arb */
		wr_bits(offset, VDIN_LOCAL_ARB_CTRL, 0x0, 30, 2);   /* local_arb_gclk */
		wr_bits(offset, VDIN_LOCAL_ARB_WR_MODE, 0x0, 30, 2);/* wr_gate_clk */
	} else { /* off */
		/* local arb */
		wr_bits(offset, VDIN_LOCAL_ARB_CTRL, 0x1, 30, 2);   /* local_arb_gclk */
		wr_bits(offset, VDIN_LOCAL_ARB_WR_MODE, 0x1, 30, 2);/* wr_gate_clk */
		/* hist */
		wr(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x1); /* luma_hist_gclk */
		/* lfifo */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 30, 2);/* gclk */
		wr_bits(offset, VDIN_PP_LFIFO_CTRL, 0x1, 29, 1);/* soft_rst_en */
		/* vsc */
		wr_bits(offset, VDIN_VSC_CTRL, 0x5, 28, 4);/* gclk */
		/* hsc */
		wr_bits(offset, VDIN_HSC_CTRL, 0x1, 30, 2);/* gclk */
		/* phsc */
		wr_bits(offset, VDIN_PHSC_CTRL, 0x1, 30, 2);/* gclk */
		/* mat1 */
		wr_bits(offset, VDIN_MAT1_CTRL, 0x1, 30, 2);/* gclk */
		/* mat0 */
		wr_bits(offset, VDIN_MAT0_CTRL, 0x1, 30, 2);/* gclk */
		/* hist */
		wr_bits(offset, VDIN_LUMA_HIST_GCLK_CTRL, 0x1, 30, 2);/* gclk */
		/* vsk */
		wr_bits(offset, VDIN_VSK_CTRL, 0x5, 28, 4);/* gclk */
		/* hsk */
		wr_bits(offset, VDIN_HSK_CTRL, 0x1, 30, 2);/* gclk */
		/* hdr2 */
		wr(offset, VDIN_PP_HDR2_CLK_GATE, 0x55555555);/* hdr2 gclk */
		/* dw top */
		wr(offset, VDIN_DW_TOP_GCLK_CTRL,
			(0x55555555 << 0)); /* dw top_gclk */
		/* pp top */
		wr(offset, VDIN_PP_TOP_GCLK_CTRL,
			(0x55555555 << 0)); /* pp top_gclk */
		/* if top buf */
		wr_bits(offset, VDIN_IF_TOP_BUF_CTRL, 0x1, 30, 2); /* buf_gclk */
	}
}

void vdin_set_matrix_color_s5(struct vdin_dev_s *devp)
{
	unsigned int offset = devp->addr_offset;
	unsigned int mode = devp->matrix_pattern_mode;

	/*vdin bist mode RGB:black*/
	wr(offset, VDIN_MATRIX_CTRL, 0x4);
	wr(offset, VDIN_MATRIX_COEF00_01, 0x0);
	wr(offset, VDIN_MATRIX_COEF02_10, 0x0);
	wr(offset, VDIN_MATRIX_COEF11_12, 0x0);
	wr(offset, VDIN_MATRIX_COEF20_21, 0x0);
	wr(offset, VDIN_MATRIX_COEF22, 0x0);
	wr(offset, VDIN_MATRIX_PRE_OFFSET0_1, 0x0);
	wr(offset, VDIN_MATRIX_PRE_OFFSET2, 0x0);
	if (mode == 1) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x10f010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x2ff);
	} else if (mode == 2) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x10f010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x1ff);
	} else if (mode == 3) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x00003ff);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x0);
	} else if (mode == 4) {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x1ff01ff);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x1ff);
	} else {
		wr(offset, VDIN_MATRIX_OFFSET0_1, 0x1ff010f);
		wr(offset, VDIN_MATRIX_OFFSET2, 0x2ff);
	}

	if (mode)
		wr(offset, VDIN_MATRIX_CTRL, 0x6);
	else
		wr(offset, VDIN_MATRIX_CTRL, 0x0);
	if (devp->debug.vdin_dbg_en)
		pr_info("%s offset:%d, md:%d\n", __func__, offset, mode);
}

void vdin_sw_reset_s5(struct vdin_dev_s *devp)
{
	//reg_reset[3:0]
	wr_bits(devp->addr_offset, VDIN_TOP_CTRL, 0xf, 0, 4);
	wr_bits(devp->addr_offset, VDIN_TOP_CTRL, 0x0, 0, 4);
}

/*
 *[0]:enable/disable;[1~31]:pattern mode 0/1/2/...
 */
void vdin_bist_s5(struct vdin_dev_s *devp, unsigned int mode)
{
	unsigned int bist_en = 0, test_pat = 0;
	unsigned int hblank = 0, vblank = 0, vde_start = 0;

	if (!(mode & 0x1)) {
		bist_en = 0;
	} else {
		bist_en = 1;
		test_pat = (mode >> 1);
		if (devp->h_active == 320) {
			//320x240
			hblank = 11;
			vblank = 8;
			vde_start = 2;
		} else {
			//1920x1080
			hblank = 128;//280;
			vblank = 128;//45;
			vde_start = 2;
		}
	}
	wr(devp->addr_offset, VDIN_IF_TOP_PTGEN_CTRL,
		(bist_en << 0) | (test_pat << 1) | (hblank << 4) |
		(vblank << 12) | (vde_start << 20));
}
