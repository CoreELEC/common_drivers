/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_CSC_H_
#define _MESON_OSD_CSC_H_

/* VPP WRAP OSD1 matrix */
#define VPP_WRAP_OSD1_MATRIX_COEF00_01             0x3d60
#define VPP_WRAP_OSD1_MATRIX_COEF02_10             0x3d61
#define VPP_WRAP_OSD1_MATRIX_COEF11_12             0x3d62
#define VPP_WRAP_OSD1_MATRIX_COEF20_21             0x3d63
#define VPP_WRAP_OSD1_MATRIX_COEF22                0x3d64
#define VPP_WRAP_OSD1_MATRIX_COEF13_14             0x3d65
#define VPP_WRAP_OSD1_MATRIX_COEF23_24             0x3d66
#define VPP_WRAP_OSD1_MATRIX_COEF15_25             0x3d67
#define VPP_WRAP_OSD1_MATRIX_CLIP                  0x3d68
#define VPP_WRAP_OSD1_MATRIX_OFFSET0_1             0x3d69
#define VPP_WRAP_OSD1_MATRIX_OFFSET2               0x3d6a
#define VPP_WRAP_OSD1_MATRIX_PRE_OFFSET0_1         0x3d6b
#define VPP_WRAP_OSD1_MATRIX_PRE_OFFSET2           0x3d6c
#define VPP_WRAP_OSD1_MATRIX_EN_CTRL               0x3d6d

struct csc_reg_s {
	u32 vpp_wrap_osd1_matrix_coef00_01;
	u32 vpp_wrap_osd1_matrix_coef02_10;
	u32 vpp_wrap_osd1_matrix_coef11_12;
	u32 vpp_wrap_osd1_matrix_coef20_21;
	u32 vpp_wrap_osd1_matrix_coef22;
	u32 vpp_wrap_osd1_matrix_offset0_1;
	u32 vpp_wrap_osd1_matrix_offset2;
	u32 vpp_wrap_osd1_matrix_en_ctrl;
};

#endif
