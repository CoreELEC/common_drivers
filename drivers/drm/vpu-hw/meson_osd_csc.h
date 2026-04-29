/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_OSD_CSC_H_
#define _MESON_OSD_CSC_H_

#define HW_OSD_MATRIX_NUM	3
#define VPP0	0
#define VPP1	1

struct csc_reg_s {
	u32 vpp_wrap_osd_matrix_coef00_01;
	u32 vpp_wrap_osd_matrix_coef02_10;
	u32 vpp_wrap_osd_matrix_coef11_12;
	u32 vpp_wrap_osd_matrix_coef20_21;
	u32 vpp_wrap_osd_matrix_coef22;
	u32 vpp_wrap_osd_matrix_offset0_1;
	u32 vpp_wrap_osd_matrix_offset2;
	u32 vpp_wrap_osd_matrix_en_ctrl;
};

#endif
