/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_eDPTX_TIMING_H
#define _AMLOGIC_eDPTX_TIMING_H

#define DPTX_CFMT_RGB_6bit             0
#define DPTX_CFMT_RGB_8bit             1
#define DPTX_CFMT_RGB_10bit            2
#define DPTX_CFMT_RGB_12bit            3
#define DPTX_CFMT_YCbCr422_8bit        4
#define DPTX_CFMT_YCbCr422_10bit       5
#define DPTX_CFMT_YCbCr422_12bit       6
#define DPTX_CFMT_YCbCr444_8bit        7
#define DPTX_CFMT_YCbCr444_10bit       8
#define DPTX_CFMT_YCbCr444_12bit       9
#define DPTX_CFMT_YCbCr420_8bit        10
#define DPTX_CFMT_YCbCr420_10bit       11
#define DPTX_CFMT_YCbCr420_12bit       12
#define DPTX_CFMT_Y_only_8bit          13
#define DPTX_CFMT_Y_only_10bit         14
#define DPTX_CFMT_Y_only_12bit         15
#define DPTX_CFMT_invalid              0xff

struct dptx_color_format_s {
	unsigned char cfmt_id;
	unsigned char bpp;
	char *name;
};

#define DPTX_COLOR_TYPE_MAX  17
extern struct dptx_color_format_s edp_cfmt_tb[DPTX_COLOR_TYPE_MAX];

#define CTRL_HSYNC_POS          BIT(0)
#define CTRL_VSYNC_POS          BIT(1)
#define CTRL_PREFERRED_TIMING   BIT(2)
#define CTRL_DUPLICATED_TIMING  BIT(7)

struct edptx_detail_timing_s {
	char name[32];

	u16 h_size;
	u16 v_size;

	u16 h_period;
	u16 h_act;
	u16 h_blank;
	u16 h_bp;
	u16 h_pw;
	u16 h_fp;

	u16 v_period;
	u16 v_act;
	u16 v_blank;
	u16 v_bp;
	u16 v_pw;
	u16 v_fp;

	// u16 h_border;
	// u16 v_border;

	u16 ctrl;

	u32 pclk;
	u32 fr1000;  //driver calculate for internal usage

	u8 cfmt;

	// unsigned short h_period_range[2];
	unsigned short v_period_range[2];
	unsigned short pclk_range[2];

	unsigned short frame_rate_range[2]; //for vmode management

	// unsigned short vfreq_vrr_range[2]; //driver calculate for vrr range

	unsigned int sync_duration_num;  //driver calculate for internal usage
	unsigned int sync_duration_den;  //driver calculate for internal usage
	unsigned short vmode_add_fr[8];
};

#endif /* AMLOGIC_DISPLAYPORT_TX_TIMING_H */
