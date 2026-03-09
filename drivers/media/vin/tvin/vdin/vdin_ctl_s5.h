/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __TVIN_VDIN_CTL_S5_H
#define __TVIN_VDIN_CTL_S5_H
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include "vdin_drv.h"

/* ************************************************************************ */
/* ******** GLOBAL FUNCTION CLAIM ******** */
/* ************************************************************************ */
void vdin_set_vframe_prop_info_s5(struct vframe_s *vf,
			       struct vdin_dev_s *devp);
void vdin_prob_get_rgb_s5(unsigned int offset, unsigned int *r,
		       unsigned int *g, unsigned int *b);
void vdin_set_all_regs_s5(struct vdin_dev_s *devp);
void vdin_set_default_regmap_s5(struct vdin_dev_s *devp);
void vdin_hw_enable_s5(struct vdin_dev_s *devp);
void vdin_hw_disable_s5(struct vdin_dev_s *devp);
void vdin_cfg_cutwin_regs_s5(struct vdin_dev_s *devp,
	unsigned int rdma_enable, struct tvin_cutwin_s *cutwin_s);
void vdin_set_decimation_s5(struct vdin_dev_s *devp);
unsigned int vdin_get_active_h(struct vdin_dev_s *devp);
unsigned int vdin_get_active_v(struct vdin_dev_s *devp);
unsigned int vdin_get_total_v(struct vdin_dev_s *devp);
void vdin_enable_module_s5(struct vdin_dev_s *devp, bool enable);
void vdin_set_matrix_s5(struct vdin_dev_s *devp);
void vdin_select_matrix_s5(struct vdin_dev_s *devp, unsigned char no,
		      enum vdin_format_convert_e csc);
bool vdin_write_done_check_s5(struct vdin_dev_s *devp);
void vdin_wr_reverse_s5(unsigned int offset, bool h_reverse,
		     bool v_reverse);
void vdin_set_hv_scale_s5(struct vdin_dev_s *devp);
void vdin_set_bitdepth_s5(struct vdin_dev_s *devp, unsigned int rdma_enable);
void vdin_set_cm2_s5(unsigned int offset, unsigned int w,
		  unsigned int h, unsigned int *data, bool cm_enable);
void vdin_hdmiin_patch(struct vdin_dev_s *devp);
void vdin_set_top_s5(struct vdin_dev_s *devp, enum tvin_port_e port,
		  enum tvin_color_fmt_e input_cfmt, enum bt_path_e bt_path);

void vdin_clk_on_off_s5(struct vdin_dev_s *devp, bool on_off);

//void vdin_vlock_input_sel(unsigned int type,
//			  enum vframe_source_type_e source_type);
void vdin_set_dv_tunnel_s5(struct vdin_dev_s *devp);
void vdin_dolby_mdata_write_en_s5(unsigned int offset, unsigned int en);
void vdin_prob_set_xy_s5(unsigned int offset,
		      unsigned int x, unsigned int y, struct vdin_dev_s *devp);
void vdin_prob_get_yuv_s5(unsigned int offset,
		       unsigned int *rgb_yuv0,	unsigned int *rgb_yuv1,
		       unsigned int *rgb_yuv2);
void vdin_prob_matrix_sel_s5(unsigned int offset,
			  unsigned int sel, struct vdin_dev_s *devp);
void vdin_change_matrix(unsigned int offset,
			unsigned int matrix_csc);
void vdin_dolby_desc_sc_enable(struct vdin_dev_s *devp,
			       unsigned int  on_off);
bool vdin_is_dolby_tunnel_444_input(struct vdin_dev_s *devp);
void vdin_dolby_de_tunnel_to_12bit(struct vdin_dev_s *devp,
				   unsigned int on_off);
void vdin_set_mif_on_off_s5(struct vdin_dev_s *devp, unsigned int rdma_enable);
unsigned int vdin_get_active_h_s5(unsigned int offset);
unsigned int vdin_get_active_v_s5(unsigned int offset);
unsigned int vdin_get_total_v_s5(unsigned int offset);
void vdin_set_matrix_color_s5(struct vdin_dev_s *devp);

void vdin_change_matrix0_s5(u32 offset, u32 matrix_csc);
void vdin_change_matrix1_s5(u32 offset, u32 matrix_csc);
void vdin_change_matrix_hdr_s5(struct vdin_dev_s *devp, u32 matrix_csc);

void vdin_set_frame_mif_write_addr_s5(struct vdin_dev_s *devp,
			unsigned int rdma_enable, struct vf_entry *vfe);
void vdin_sw_reset_s5(struct vdin_dev_s *devp);
void vdin_bist_s5(struct vdin_dev_s *devp, unsigned int mode);
void vdin_dlg_update_hist_hv_s5(unsigned int temp_hist_width, unsigned int temp_hist_height);
void vdin_get_hist_val_s5(struct vdin_dev_s *devp, struct vdin_hist_s *vdin1_hist_temp);
void vdin_hist_init_s5(struct vdin_dev_s *devp);
#endif

