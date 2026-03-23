/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _GE2D_TEE_H_
#define _GE2D_TEE_H_

#include <linux/types.h>
#include <linux/amlogic/media/ge2d/ge2d.h>

struct ge2d_tee_src1_data_s {
	unsigned char     urgent_en;
	unsigned char     ddr_burst_size_y;
	unsigned char     ddr_burst_size_cb;
	unsigned char     ddr_burst_size_cr;
	unsigned int	  canaddr;
	unsigned char     x_yc_ratio;
	unsigned char     y_yc_ratio;
	unsigned char     sep_en;
	unsigned char     format;

	unsigned char     endian;
	unsigned char     color_map;

	unsigned char     mode_8b_sel;
	unsigned char     lut_en;
	unsigned char     deep_color;
	unsigned char     mult_rounding;
	unsigned char     alpha_conv_mode0;
	unsigned char     alpha_conv_mode1;
	unsigned char     color_conv_mode0;
	unsigned char     color_conv_mode1;
	unsigned int      def_color;
	unsigned int      format_all;
	unsigned long long phy_addr[MAX_PLANE];
	unsigned int      stride[MAX_PLANE];
	unsigned int      stride_mode[MAX_PLANE];
};

struct ge2d_tee_src2_dst_data_s {
	unsigned char     urgent_en;
	unsigned char     ddr_burst_size;
	unsigned char     src2_canaddr;
	unsigned char     src2_format;

	unsigned char     src2_endian;
	unsigned char     src2_color_map;

	unsigned char     src2_mode_8b_sel;
	unsigned int      src2_def_color;

	unsigned int     dst_canaddr;
	unsigned char    dst_format;

	unsigned char     dst_endian;
	unsigned char     dst_color_map;

	unsigned char     dst_mode_8b_sel;

	unsigned int      src2_format_all;
	unsigned int      dst_format_all;

	/* only for m6 */
	unsigned char     dst2_pixel_byte_width;
	unsigned char     dst2_color_map;
	unsigned char     dst2_discard_mode;
	unsigned char     dst2_enable;

	unsigned long long src2_phyaddr[MAX_PLANE];
	unsigned int src2_stride[MAX_PLANE];
	unsigned int src2_stride_mode[MAX_PLANE];
	unsigned long long dst_phyaddr[MAX_PLANE];
	unsigned int dst_stride[MAX_PLANE];
	unsigned int dst_stride_mode[MAX_PLANE];
	unsigned char dst_rpt;
	unsigned char dst_swap64bit;
};

struct ge2d_tee_cmd_s {
	int              src1_x_start;
	int              src1_y_start;
	int              src1_x_end;
	int              src1_y_end;
	/* unsigned char    src1_x_start_ex; */
	/* unsigned char    src1_y_start_ex; */
	/* unsigned char    src1_x_end_ex; */
	/* unsigned char    src1_y_end_ex; */

	unsigned char    src1_x_rev;
	unsigned char    src1_y_rev;
	/* unsigned char    src1_x_chr_phase; */
	/* unsigned char    src1_y_chr_phase; */
	unsigned char    src1_fill_color_en;
	unsigned int     src1_fmt;

	int              src2_x_start;
	int              src2_y_start;
	int              src2_x_end;
	int              src2_y_end;
	unsigned char    src2_x_rev;
	unsigned char    src2_y_rev;
	unsigned char    src2_fill_color_en;
	unsigned int     src2_fmt;

	int              dst_x_start;
	int              dst_y_start;
	int              dst_x_end;
	int              dst_y_end;
	unsigned char    dst_xy_swap;
	unsigned char    dst_x_rev;
	unsigned char    dst_y_rev;
	unsigned int     dst_fmt;

	int              sc_prehsc_en;
	int              sc_prevsc_en;
	int              sc_hsc_en;
	int              sc_vsc_en;
	int              vsc_phase_step;
	int              vsc_phase_slope;
	unsigned char    vsc_rpt_l0_num;
	int              vsc_ini_phase;
	int              hsc_phase_step;
	int              hsc_phase_slope;
	unsigned char    hsc_rpt_p0_num;
	int              hsc_ini_phase;
	unsigned char    hsc_div_en;
	unsigned int    hsc_div_length;
	int              hsc_adv_num;
	int              hsc_adv_phase;

	unsigned char    src1_cmult_asel;
	unsigned char    src2_cmult_asel;
#ifdef CONFIG_GE2D_SRC2
	unsigned char    src2_cmult_ad;
#endif
	unsigned char    color_blend_mode;
	unsigned char    color_src_blend_factor;
	unsigned char    color_dst_blend_factor;
	unsigned char    color_logic_op;

	unsigned char    alpha_blend_mode;
	unsigned char    alpha_src_blend_factor;
	unsigned char    alpha_dst_blend_factor;
	unsigned char    alpha_logic_op;

	unsigned char    cmd_op;
};

struct ge2d_tee_config_s {
	struct ge2d_tee_src1_data_s      src1_data;
	struct ge2d_src1_gen_s       src1_gen;
	struct ge2d_tee_src2_dst_data_s  src2_dst_data;
	struct ge2d_src2_dst_gen_s   src2_dst_gen;
	struct ge2d_dp_gen_s         dp_gen;
	unsigned int	v_scale_coef_type;
	unsigned int	h_scale_coef_type;
	unsigned int	update_flag;
	struct ge2d_matrix_s matrix_custom;
};

struct ge2d_tee_queue_item_s {
	struct ge2d_tee_cmd_s cmd;
	struct ge2d_tee_config_s config;
	u32 reserved0;
	u32 reserved1;
	u32 reserved2;
};

#endif

