// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vicp_reg.h"
#include "vicp_log.h"
#include "vicp_hardware.h"

void set_module_enable(u32 is_enable)
{
	return vicp_reg_set_bits(VID_CMPR_CTRL, is_enable, 0, 1);
}

void set_module_start(u32 is_start)
{
	return vicp_reg_set_bits(VID_CMPR_START, is_start, 0, 1);
}

void set_rdmif_enable(u32 is_enable)
{
	return vicp_reg_set_bits(VID_CMPR_RDMIFXN_GEN_REG, is_enable, 0, 1);
}

void set_afbcd_enable(u32 is_enable)
{
	//0:disable; 1:enable
	return vicp_reg_set_bits(VID_CMPR_AFBCDM_ENABLE, is_enable, 8, 1);
}

void set_input_path(enum vicp_input_path_e path)
{
	return vicp_reg_set_bits(VID_CMPR_AFBCDM_VDTOP_CTRL0, path, 13, 1);
}

void set_output_path(enum vicp_output_path_e path)
{
	return vicp_reg_set_bits(VID_CMPR_WR_PATH_CTRL, path, 0, 2);
}

void set_input_size(u32 size_v, u32 size_h)
{
	return  vicp_reg_write(VID_CMPR_IN_SIZE, (size_v << 16) | size_h);
}

void set_output_size(u32 size_v, u32 size_h)
{
	return  vicp_reg_write(VID_CMPR_OUT_SIZE, (size_v << 16) | size_h);
}

void set_afbcd_4k_enable(u32 is_enable)
{
	return vicp_reg_set_bits(VID_CMPR_AFBCDM_VDTOP_CTRL0, is_enable, 14, 1);
}

void set_afbcd_input_size(u32 size_h, u32 size_v)
{
	u32 value = 0;

	value = ((size_h & 0x1fff) << 16) | (size_v & 0x1fff);
	return vicp_reg_write(VID_CMPR_AFBCDM_SIZE_IN, value);
}

void set_afbcd_default_color(u32 def_color_y, u32 def_color_u, u32 def_color_v)
{
	u32 value = 0;

	value = ((def_color_y & 0x3ff) << 20) |
		((def_color_u & 0x3ff) << 10) |
		((def_color_v & 0x3ff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_DEC_DEF_COLOR, value);
}

void set_afbcd_mode(struct vicp_afbcd_mode_reg_t afbcd_mode)
{
	u32 value = 0;

	value = ((afbcd_mode.ddr_sz_mode & 0x1) << 29) |
		((afbcd_mode.blk_mem_mode & 0x1) << 28) |
		((afbcd_mode.rev_mode & 0x3) << 26) |
		((afbcd_mode.mif_urgent & 0x3) << 24) |
		((afbcd_mode.hold_line_num & 0x7f) << 16) |
		((afbcd_mode.burst_len & 0x3) << 14) |
		((afbcd_mode.compbits_yuv & 0x3f) << 8) |
		((afbcd_mode.vert_skip_y  & 0x3) << 6) |
		((afbcd_mode.horz_skip_y  & 0x3) << 4) |
		((afbcd_mode.vert_skip_uv & 0x3) << 2) |
		((afbcd_mode.horz_skip_uv & 0x3) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_MODE, value);
}

void set_afbcd_conv_control(enum vicp_color_format_e fmt_mode, u32 lbuf_len)
{
	u32 value = 0;

	value = ((fmt_mode & 0x3) << 12) | ((lbuf_len & 0xfff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_CONV_CTRL, value);
}

void set_afbcd_lbuf_depth(u32 dec_lbuf_depth, u32 mif_lbuf_depth)
{
	u32 value = 0;

	value = ((dec_lbuf_depth & 0xfff) << 16) | ((mif_lbuf_depth & 0xfff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_LBUF_DEPTH, value);
}

void set_afbcd_addr(u32 head_body_flag, u64 addr)
{
	//0 head_addr, 1 body_addr
	if (head_body_flag == 0)
		return vicp_reg_write(VID_CMPR_AFBCDM_HEAD_BADDR, addr);
	else
		return vicp_reg_write(VID_CMPR_AFBCDM_BODY_BADDR, addr);
}

void set_afbcd_mif_scope(u32 v_h_flag, u32 scope_begin, u32 scope_end)
{
	u32 scope_value = 0, reg = 0;

	//0 h_scope, 1 v_scope
	if (v_h_flag == 0) {
		reg = VID_CMPR_AFBCDM_MIF_HOR_SCOPE;
		scope_value = ((scope_begin & 0x3ff) << 16) | ((scope_end & 0x3ff) << 0);
	} else {
		reg = VID_CMPR_AFBCDM_MIF_VER_SCOPE;
		scope_value = ((scope_begin & 0xfff) << 16) | ((scope_end & 0xfff) << 0);
	}

	return vicp_reg_write(reg, scope_value);
}

void set_afbcd_pixel_scope(u32 v_h_flag, u32 scope_begin, u32 scope_end)
{
	u32 scope_value = 0, reg = 0;

	//0 h_scope, 1 v_scope
	if (v_h_flag == 0)
		reg = VID_CMPR_AFBCDM_PIXEL_HOR_SCOPE;
	else
		reg = VID_CMPR_AFBCDM_PIXEL_VER_SCOPE;

	scope_value = ((scope_begin & 0x1fff) << 16) | ((scope_end & 0x1fff) << 0);

	return vicp_reg_write(reg, scope_value);
}

void set_afbcd_general(struct vicp_afbcd_general_reg_t reg)
{
	u32 value = 0;

	value = ((reg.gclk_ctrl_core & 0x3f) << 23) |
		((reg.fmt_size_sw_mode & 0x1) << 22) |
		((reg.addr_link_en & 0x1) << 21) |
		((reg.fmt444_comb & 0x1) << 20) |
		((reg.dos_uncomp_mode & 0x1) << 19) |
		((reg.soft_rst & 0x7) << 16) |
		((reg.ddr_blk_size & 0x3) << 12) |
		((reg.cmd_blk_size & 0x7) << 9) |
		((reg.dec_enable & 0x1) << 8) |
		((reg.head_len_sel & 0x1) << 1) |
		((reg.reserved & 0x1) << 0);

	return vicp_reg_write(VID_CMPR_AFBCDM_ENABLE, value);
}

void set_afbcd_colorformat_control(struct vicp_afbcd_cfmt_control_reg_t cfmt_control)
{
	u32 value;

	value = ((cfmt_control.gclk_bit_dis & 0x1) << 31) |
		((cfmt_control.soft_rst_bit & 0x1) << 30) |
		((cfmt_control.cfmt_h_rpt_pix & 0x1) << 28) |
		((cfmt_control.cfmt_h_ini_phase & 0xf) << 24) |
		((cfmt_control.cfmt_h_rpt_p0_en & 0x1) << 23) |
		((cfmt_control.cfmt_h_yc_ratio & 0x3) << 21) |
		((cfmt_control.cfmt_h_en & 0x1) << 20) |
		((cfmt_control.cfmt_v_phase0_en & 0x1) << 19) |
		((cfmt_control.cfmt_v_rpt_last_dis & 0x1) << 18) |
		((cfmt_control.cfmt_v_phase0_nrpt_en & 0x1) << 17) |
		((cfmt_control.cfmt_v_rpt_line0_en & 0x1) << 16) |
		((cfmt_control.cfmt_v_skip_line_num & 0xf) << 12) |
		((cfmt_control.cfmt_v_ini_phase & 0xf) << 8) |
		((cfmt_control.cfmt_v_phase_step & 0x7f) << 1) |
		((cfmt_control.cfmt_v_en & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_VD_CFMT_CTRL, value);
}

void set_afbcd_colorformat_size(u32 w_or_h, u32 fmt_size_h, u32 fmt_size_v)
{
	u32 value = 0, reg = 0;

	if (w_or_h == 1) {//width
		reg = VID_CMPR_AFBCDM_VD_CFMT_W;
		value = (fmt_size_h << 16) | (fmt_size_v << 0);
	} else {//height
		reg = VID_CMPR_AFBCDM_VD_CFMT_H;
		value = fmt_size_h;
	}

	return vicp_reg_write(reg, value);
}

void set_afbcd_quant_control(struct vicp_afbcd_quant_control_reg_t quant_control)
{
	u32 value = 0;

	value = ((quant_control.quant_expand_en_1 & 0x1) << 11) |
		((quant_control.quant_expand_en_0 & 0x1) << 10) |
		((quant_control.bcleav_offsst & 0x3) << 8) |
		((quant_control.lossy_chrm_en & 0x1) << 4) |
		((quant_control.lossy_luma_en & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_IQUANT_ENABLE, value);
}

void set_afbcd_rotate_control(struct vicp_afbcd_rotate_control_reg_t rotate_control)
{
	u32 value = 0;

	value = ((rotate_control.out_ds_mode_h & 0x3) << 30) |
		((rotate_control.out_ds_mode_v & 0x3) << 28) |
		((rotate_control.pip_mode & 0x1) << 27) |
		((rotate_control.uv_shrk_drop_mode_v & 0x7) << 24) |
		((rotate_control.uv_shrk_drop_mode_h & 0x7) << 20) |
		((rotate_control.uv_shrk_ratio_v & 0x3) << 18) |
		((rotate_control.uv_shrk_ratio_h & 0x3) << 16) |
		((rotate_control.y_shrk_drop_mode_v & 0x7) << 12) |
		((rotate_control.y_shrk_drop_mode_h & 0x7) << 8) |
		((rotate_control.y_shrk_ratio_v & 0x3) << 6) |
		((rotate_control.y_shrk_ratio_h & 0x3) << 4) |
		((rotate_control.uv422_drop_mode & 0x3) << 2) |
		((rotate_control.out_fmt_for_uv422 & 0x3) << 1) |
		((rotate_control.enable & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_AFBCDM_ROT_CTRL, value);
}

void set_afbcd_rotate_scope(struct vicp_afbcd_rotate_scope_reg_t rotate_scope)
{
	u32 value = 0;

	value = ((rotate_scope.debug_probe & 0x1f) << 20) |
		((rotate_scope.out_ds_mode & 0x1) << 19) |
		((rotate_scope.out_fmt_down_mode & 0x3) << 17) |
		((rotate_scope.in_fmt_force444 & 0x1) << 16) |
		((rotate_scope.out_fmt_mode & 0x3) << 14) |
		((rotate_scope.out_compbits_y & 0x3) << 12) |
		((rotate_scope.out_compbits_uv & 0x3) << 10) |
		((rotate_scope.win_bgn_v & 0x3) << 8) |
		((rotate_scope.win_bgn_h & 0x1f) << 0);

	return vicp_reg_write(VID_CMPR_AFBCDM_ROT_SCOPE, value);
}

void set_afbce_lossy_luma_enable(u32 enable)
{
	return vicp_reg_set_bits(VID_CMPR_AFBCE_QUANT_ENABLE, (enable & 0x1), 0, 1);
}

void set_afbce_lossy_chrm_enable(u32 enable)
{
	return vicp_reg_set_bits(VID_CMPR_AFBCE_QUANT_ENABLE, (enable & 0x1), 4, 1);
}

void set_afbce_input_size(u32 size_h, u32 size_v)
{
	u32 value = 0;

	value = (((size_h & 0x1fff) << 16) | (size_v & 0x1fff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_SIZE_IN, value);
}

void set_afbce_blk_size(u32 size_h, u32 size_v)
{
	u32 value = 0;

	value = ((size_h & 0x1fff) << 16) | ((size_v & 0x1fff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_BLK_SIZE_IN, value);
}

void set_afbce_head_addr(u64 head_addr)
{
	return vicp_reg_write(VID_CMPR_AFBCE_HEAD_BADDR, head_addr >> 4);
}

void set_rdmif_luma_fifo_size(u32 size)
{
	return vicp_reg_write(VID_CMPR_RDMIFXN_LUMA_FIFO_SIZE, size);
}

void set_rdmif_general_reg(struct vicp_rdmif_general_reg_t reg)
{
	u32 value = 0;

	value = ((reg.enable_free_clk & 0x1) << 31) |
		((reg.reset_on_go_field & 0x1) << 29) |
		((reg.urgent_chroma & 0x1) << 28) |
		((reg.urgent_luma & 0x1) << 27) |
		((reg.chroma_end_at_last_line & 0x1) << 26) |
		((reg.luma_end_at_last_line & 0x1) << 25) |
		((reg.hold_lines & 0x3f) << 19) |
		((reg.last_line_mode & 0x1) << 18) |
		((reg.ro_busy & 0x1) << 17) |
		(((reg.demux_mode & 0x1) << 16) |
		((reg.bytes_per_pixel & 0x3) << 14) |
		((reg.ddr_burst_size_cr & 0x3) << 12) |
		((reg.ddr_burst_size_cb & 0x3) << 10) |
		((reg.ddr_burst_size_y & 0x3) << 8) |
		((reg.chroma_rpt_lastl & 0x1) << 6) |
		((reg.little_endian & 0x1) << 4) |
		((reg.chroma_hz_avg & 0x1) << 3) |
		((reg.luma_hz_avg & 0x1) << 2) |
		((reg.set_separate_en & 0x1) << 1) |
		((reg.enable & 0x1) << 0));

	return vicp_reg_write(VID_CMPR_RDMIFXN_GEN_REG, value);
}

void set_rdmif_general_reg2(struct vicp_rdmif_general_reg2_t reg)
{
	u32 value = 0;

	value = ((reg.chroma_line_read_sel & 0x1) << 29) |
		((reg.luma_line_read_sel & 0x1) << 28) |
		((reg.shift_pat_cr & 0xf) << 24) |
		((reg.shift_pat_cb & 0xf) << 16) |
		((reg.shift_pat_y & 0xf) << 8) |
		((reg.hold_lines & 0x1) << 6) |
		((reg.y_rev1 & 0x1) << 5) |
		((reg.x_rev1 & 0x1) << 4) |
		((reg.y_rev0 & 0x1) << 3) |
		((reg.x_rev0 & 0x1) << 2) |
		((reg.color_map & 0x3) << 0);

	return vicp_reg_write(VID_CMPR_RDMIFXN_GEN_REG2, value);
}

void set_rdmif_general_reg3(struct vicp_rdmif_general_reg3_t reg)
{
	u32 value = 0;

	value = ((reg.f0_stride32aligned2 & 0x1) << 26) |
		((reg.f0_stride32aligned1 & 0x1) << 25) |
		((reg.f0_stride32aligned0 & 0x1) << 24) |
		((reg.f0_cav_blk_mode2 & 0x3) << 22) |
		((reg.f0_cav_blk_mode1 & 0x3) << 20) |
		((reg.f0_cav_blk_mode0 & 0x3) << 18) |
		((reg.abort_mode & 0x3) << 16) |
		((reg.dbg_mode & 0x3) << 10) |
		((reg.bits_mode & 0x3) << 8) |
		((reg.block_len & 0x3) << 4) |
		((reg.burst_len & 0x3) << 1) |
		((reg.bit_swap & 0x1) << 0);

	return vicp_reg_write(VID_CMPR_RDMIFXN_GEN_REG3, value);
}

void set_rdmif_base_addr(enum rdmif_baseaddr_type_e addr_type, u64 base_addr)
{
	u32 value = 0, reg = 0;

	value = base_addr >> 4;
	switch (addr_type) {
	case RDMIF_BASEADDR_TYPE_Y:
		reg = VID_CMPR_RDMIFXN_BADDR_Y;
		break;
	case RDMIF_BASEADDR_TYPE_CB:
		reg = VID_CMPR_RDMIFXN_BADDR_CB;
		break;
	case RDMIF_BASEADDR_TYPE_CR:
		reg = VID_CMPR_RDMIFXN_BADDR_CR;
		break;
	case RDMIF_BASEADDR_TYPE_MAX:
	default:
		vicp_print(VICP_ERROR, "%s: invalid case.", __func__);
		break;
	}

	if (reg != 0)
		vicp_reg_set_bits(reg, value, 0, 32);
}

void set_rdmif_stride(enum rdmif_stride_type_e stride_type, u32 stride_value)
{
	switch (stride_type) {
	case RDMIF_STRIDE_TYPE_Y:
		vicp_reg_set_bits(VID_CMPR_RDMIFXN_STRIDE_0, stride_value, 0, 13);
		break;
	case RDMIF_STRIDE_TYPE_CB:
		vicp_reg_set_bits(VID_CMPR_RDMIFXN_STRIDE_0, stride_value, 16, 13);
		break;
	case RDMIF_STRIDE_TYPE_CR:
		vicp_reg_set_bits(VID_CMPR_RDMIFXN_STRIDE_1, (1 << 16) | stride_value, 0, 32);
		break;
	case RDMIF_STRIDE_TYPE_MAX:
	default:
		vicp_print(VICP_ERROR, "%s: invalid case.", __func__);
		break;
	}
}

void set_rdmif_luma_position(u32 index, u32 is_x, u32 start, u32 end)
{
	u32 value = 0, reg = 0;

	value = (end << 16) | (start << 0);
	if (index == 0) {
		if (is_x)
			reg = VID_CMPR_RDMIFXN_LUMA_X0;
		else
			reg = VID_CMPR_RDMIFXN_LUMA_Y0;
	} else {
		if (is_x)
			reg = VID_CMPR_RDMIFXN_LUMA_X1;
		else
			reg = VID_CMPR_RDMIFXN_LUMA_Y1;
	}

	return vicp_reg_write(reg, value);
}

void set_rdmif_chroma_position(u32 index, u32 is_x, u32 start, u32 end)
{
	u32 value = 0, reg = 0;

	value = (end << 16) | (start << 0);
	if (index == 0) {
		if (is_x)
			reg = VID_CMPR_RDMIFXN_CHROMA_X0;
		else
			reg = VID_CMPR_RDMIFXN_CHROMA_Y0;
	} else {
		if (is_x)
			reg = VID_CMPR_RDMIFXN_CHROMA_X1;
		else
			reg = VID_CMPR_RDMIFXN_CHROMA_Y1;
	}

	return vicp_reg_write(reg, value);
}

void set_rdmif_rpt_loop(struct vicp_rdmif_rpt_loop_t rpt_loop)
{
	u32 value = 0;

	value = ((rpt_loop.rpt_loop1_chroma_start & 0xf) << 28) |
		((rpt_loop.rpt_loop1_chroma_end & 0xf) << 24) |
		((rpt_loop.rpt_loop1_luma_start & 0xf) << 20) |
		((rpt_loop.rpt_loop1_luma_end & 0xf) << 16) |
		((rpt_loop.rpt_loop0_chroma_start & 0xf) << 12) |
		((rpt_loop.rpt_loop0_chroma_end & 0xf) << 8) |
		((rpt_loop.rpt_loop0_luma_start & 0xf) << 4) |
		((rpt_loop.rpt_loop0_luma_end & 0xf) << 0);

	return vicp_reg_write(VID_CMPR_RDMIFXN_RPT_LOOP, value);
}

void set_rdmif_luma_rpt_pat(u32 luma_index, u32 value)
{
	if (luma_index == 0)
		return vicp_reg_write(VID_CMPR_RDMIFXN_LUMA0_RPT_PAT, value);
	else
		return vicp_reg_write(VID_CMPR_RDMIFXN_LUMA1_RPT_PAT, value);
}

void set_rdmif_chroma_rpt_pat(u32 chroma_index, u32 value)
{
	if (chroma_index == 0)
		return vicp_reg_write(VID_CMPR_RDMIFXN_CHROMA0_RPT_PAT, value);
	else
		return vicp_reg_write(VID_CMPR_RDMIFXN_CHROMA1_RPT_PAT, value);
}

void set_rdmif_dummy_pixel(u32 value)
{
	return vicp_reg_write(VID_CMPR_RDMIFXN_DUMMY_PIXEL, value);
}

void set_rdmif_color_format_control(struct vicp_rdmif_color_format_t color_format)
{
	u32 value = 0;

	value  = ((color_format.cfmt_gclk_bit_dis & 0x1) << 31) |
		((color_format.cfmt_soft_rst_bit & 0x1) << 30) |
		((color_format.cfmt_h_rpt_pix & 0x1) << 28) |
		((color_format.cfmt_h_ini_phase & 0xf) << 24) |
		((color_format.cfmt_h_rpt_p0_en & 0x1) << 23) |
		((color_format.cfmt_h_yc_ratio & 0x3) << 21) |
		((color_format.cfmt_h_en & 0x1) << 20) |
		((color_format.cfmt_h_rpt_pix & 0x1) << 19) |
		((color_format.cfmt_h_rpt_pix & 0x1) << 18) |
		((color_format.cfmt_v_phase0_nrpt_en & 0x1) << 17) |
		((color_format.cfmt_v_rpt_line0_en & 0x1) << 16) |
		((color_format.cfmt_v_skip_line_num & 0xf) << 12) |
		((color_format.cfmt_v_ini_phase & 0xf) << 8) |
		((color_format.cfmt_v_phase_step & 0x7f) << 1) |
		((color_format.cfmt_v_en & 0x1) << 0);

	return vicp_reg_write(VID_CMPR_RDMIFXN_CFMT_CTRL, value);
}

void set_rdmif_color_format_width(u32 cfmt_width_h, u32 cfmt_width_v)
{
	u32 value = 0;

	value = (cfmt_width_h << 16) | (cfmt_width_v << 0);
	return vicp_reg_write(VID_CMPR_RDMIFXN_CFMT_W, value);
}

void set_afbce_mif_size(u32 decompress_size)
{
	return vicp_reg_set_bits(VID_CMPR_AFBCE_MIF_SIZE, (decompress_size & 0x1fff), 16, 5);
}

void set_afbce_mix_scope(u32 h_or_v, u32 begin, u32 end)
{
	u32 value = 0, reg = 0;

	if (h_or_v == 1) {
		reg = VID_CMPR_AFBCE_MIF_HOR_SCOPE;
		value = ((end & 0x3ff) << 16) | ((begin & 0x3ff) << 0);
	} else {
		reg = VID_CMPR_AFBCE_MIF_VER_SCOPE;
		value = ((end & 0xfff) << 16) | ((begin & 0xfff) << 0);
	}

	return vicp_reg_write(reg, value);
}

void set_afbce_pixel_in_scope(u32 h_or_v, u32 begin, u32 end)
{
	u32 value = 0, reg = 0;

	if (h_or_v == 1)
		reg = VID_CMPR_AFBCE_PIXEL_IN_HOR_SCOPE;
	else
		reg = VID_CMPR_AFBCE_PIXEL_IN_VER_SCOPE;

	value = ((end & 0x1fff) << 16) | ((begin & 0x1fff) << 0);
	return vicp_reg_write(reg, value);
}

void set_afbce_conv_control(u32 fmt_ybuf_depth, u32 lbuf_depth)
{
	u32 value = 0;

	value = (fmt_ybuf_depth & 0x1fff) << 16 | ((lbuf_depth & 0xfff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_CONV_CTRL, value);
}

void set_afbce_mode(struct vicp_afbce_mode_reg_t afbce_mode_reg)
{
	u32 value = 0;

	value = ((afbce_mode_reg.soft_rst & 0x3) << 29) |
		((afbce_mode_reg.rev_mode & 0x3) << 26) |
		((afbce_mode_reg.mif_urgent & 0x3) << 24) |
		((afbce_mode_reg.hold_line_num & 0x7f) << 16) |
		((afbce_mode_reg.burst_mode & 0x3) << 14) |
		((afbce_mode_reg.fmt444_comb & 0x1) << 0);

	return vicp_reg_write(VID_CMPR_AFBCE_MODE, value);
}

void set_afbce_colorfmt(struct vicp_afbce_color_format_reg_t color_format_reg)
{
	u32 value = 0;

	value = ((color_format_reg.ofset_burst4_en & 0x1) << 11) |
		((color_format_reg.burst_length_add2_en & 0x1) << 10) |
		((color_format_reg.format_mode & 0x3) << 8) |
		((color_format_reg.compbits_c & 0xf) << 4) |
		((color_format_reg.compbits_y & 0xf) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_FORMAT, value);
}

void set_afbce_default_color1(u32 def_color_a, u32 def_color_y)
{
	u32 value = 0;

	value = ((def_color_a & 0xfff) << 12) | ((def_color_y & 0xfff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_DEFCOLOR_1, value);
}

void set_afbce_default_color2(u32 def_color_u, u32 def_color_v)
{
	u32 value = 0;

	value = ((def_color_v & 0xfff) << 12) | ((def_color_u & 0xfff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_DEFCOLOR_2, value);
}

void set_afbce_mmu_rmif_scope(u32 x_or_y, u32 start, u32 end)
{
	u32 value = 0, reg = 0;

	if (x_or_y == 1)
		reg = VID_CMPR_AFBCE_MMU_RMIF_SCOPE_X;
	else
		reg = VID_CMPR_AFBCE_MMU_RMIF_SCOPE_Y;

	value = ((end & 0x1fff) << 16) | ((start & 0x1fff) << 0);
	return vicp_reg_write(reg, value);
}

void set_afbce_mmu_rmif_control1(struct vicp_afbce_mmu_rmif_control1_reg_t rmif_control1)
{
	u32 value = 0;

	value = ((rmif_control1.sync_sel & 0x3) << 24) |
		((rmif_control1.canvas_id & 0xff) << 16) |
		((rmif_control1.cmd_intr_len & 0x7) << 12) |
		((rmif_control1.cmd_req_size & 0x3) << 10) |
		((rmif_control1.burst_len & 0x3) << 8) |
		((rmif_control1.swap_64bit & 0x1) << 7) |
		((rmif_control1.little_endian & 0x1) << 6) |
		((rmif_control1.y_rev & 0x1) << 5) |
		((rmif_control1.x_rev & 0x1) << 4) |
		((rmif_control1.pack_mode & 0x3) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_MMU_RMIF_CTRL1, value);
}

void set_afbce_mmu_rmif_control2(struct vicp_afbce_mmu_rmif_control2_reg_t rmif_control2)
{
	u32 value = 0;

	value = ((rmif_control2.sw_rst & 0x3) << 30) |
		((rmif_control2.int_clr & 0x3) << 20) |
		((rmif_control2.gclk_ctrl & 0x3) << 18) |
		((rmif_control2.urgent_ctrl & 0x1ffff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_MMU_RMIF_CTRL2, value);
}

void set_afbce_mmu_rmif_control3(struct vicp_afbce_mmu_rmif_control3_reg_t rmif_control3)
{
	u32 value = 0;

	value = ((rmif_control3.vstep & 0xf) << 20) |
		((rmif_control3.acc_mode & 0x1) << 16) |
		((rmif_control3.stride & 0x1fff) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_MMU_RMIF_CTRL3, value);
}

void set_afbce_mmu_rmif_control4(u64 baddr)
{
	return vicp_reg_write(VID_CMPR_AFBCE_MMU_RMIF_CTRL4, (baddr >> 4));
}

void set_afbce_pip_control(struct vicp_afbce_pip_control_reg_t pip_control)
{
	u32 value = 0;

	value = ((pip_control.enc_align_en & 0x1) << 2) |
		((pip_control.pip_ini_ctrl & 0x1) << 1) |
		((pip_control.pip_mode & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_PIP_CTRL, value);
}

void set_afbce_rotation_control(u32 rotation_en, u32 step_v)
{
	u32 value = 0;

	value = ((rotation_en & 0x1) << 4) | ((step_v & 0xf) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_ROT_CTRL, value);
}

void set_afbce_enable(struct vicp_afbce_enable_reg_t enable_reg)
{
	u32 value = 0;

	value = ((enable_reg.gclk_ctrl & 0xfff) << 20) |
		((enable_reg.afbce_sync_sel & 0xf) << 16) |
		((enable_reg.enc_rst_mode & 0x1) << 13) |
		((enable_reg.enc_en_mode & 0x1) << 12) |
		((enable_reg.enc_enable & 0x1) << 8) |
		((enable_reg.pls_enc_frm_start & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_AFBCE_ENABLE, value);
}

void set_wrmif_shrk_enable(u32 is_enable)
{
	return vicp_reg_set_bits(VID_CMPR_WRMIF_SHRK_CTRL, is_enable, 0, 1);
}

void set_wrmif_shrk_size(u32 size)
{
	return vicp_reg_set_bits(VID_CMPR_WRMIF_SHRK_SIZE, size, 0, 26);
}

void set_wrmif_shrk_mode_h(u32 mode)
{
	return vicp_reg_set_bits(VID_CMPR_WRMIF_SHRK_CTRL, mode, 6, 2);
}

void set_wrmif_shrk_mode_v(u32 mode)
{
	return vicp_reg_set_bits(VID_CMPR_WRMIF_SHRK_CTRL, mode, 8, 2);
}

void set_wrmif_base_addr(u32 index, u64 addr)
{
	vicp_print(VICP_INFO, "%s: addr = 0x%lx.\n", __func__, addr);

	if (index == 0)
		return vicp_reg_write(VID_CMPR_WRMIF_BADDR0, (addr >> 4));
	else
		return vicp_reg_write(VID_CMPR_WRMIF_BADDR1, (addr >> 4));
}

void set_wrmif_stride(u32 is_y, u32 value)
{
	if (is_y == 1)
		return vicp_reg_write(VID_CMPR_WRMIF_STRIDE0, value);
	else
		return vicp_reg_write(VID_CMPR_WRMIF_STRIDE1, value);
}

void set_wrmif_range(u32 is_x, u32 rev, u32 start, u32 end)
{
	u32 value = 0, reg = 0;

	value = (rev << 30) | (start << 16) | (end << 0);
	if (is_x == 1)
		reg = VID_CMPR_WRMIF_X;
	else
		reg = VID_CMPR_WRMIF_Y;

	return vicp_reg_write(reg, value);
}

void set_wrmif_control(struct vicp_wrmif_control_t wrmif_control)
{
	u32 value = 0;

	value = ((wrmif_control.swap_64bits_en & 0x1) << 30) |
		((wrmif_control.burst_limit & 0xf) << 26) |
		((wrmif_control.canvas_sync_en & 0x1) << 25) |
		((wrmif_control.gate_clock_en & 0x1) << 24) |
		((wrmif_control.rgb_mode & 0x3) << 22) |
		((wrmif_control.h_conv & 0x3) << 20) |
		((wrmif_control.v_conv & 0x3) << 18) |
		((wrmif_control.swap_cbcr & 0x1) < 17) |
		((wrmif_control.urgent & 0x1) << 16) |
		((wrmif_control.word_limit & 0xf) << 4) |
		((wrmif_control.data_ext_ena & 0x1) << 3) |
		((wrmif_control.little_endian & 0x1) << 2) |
		((wrmif_control.bit10_mode & 0x1) << 1) |
		((wrmif_control.wrmif_en & 0x1) << 0);
	return vicp_reg_write(VID_CMPR_WRMIF_CTRL, value);
}

void set_crop_enable(u32 is_enable)
{
	return vicp_reg_set_bits(VID_CMPR_CROP_CTRL, is_enable, 31, 1);
}

void set_crop_holdline(u32 hold_line)
{
	return vicp_reg_set_bits(VID_CMPR_CROP_CTRL, hold_line, 0, 4);
}

void set_crop_dimm(u32 dimm_layer_en, u32 dimm_data)
{
	u32 value = 0;

	value = ((dimm_layer_en & 0x1) << 31) | ((dimm_data & 0x3fffffff) << 0);

	return vicp_reg_write(VID_CMPR_CROP_DIMM_CTRL, value);
}

void set_crop_size_in(u32 size_h, u32 size_v)
{
	u32 value = 0;

	value = ((size_h & 0x1fff) << 16) | ((size_v & 0x1fff) << 0);

	return vicp_reg_write(VID_CMPR_CROP_SIZE_IN, value);
}

void set_crop_scope_h(u32 begain, u32 end)
{
	u32 value = 0;

	value = ((end & 0x1fff) << 16) | ((begain & 0x1fff) << 0);

	return vicp_reg_write(VID_CMPR_CROP_HSCOPE, value);
}

void set_crop_scope_v(u32 begain, u32 end)
{
	u32 value = 0;

	value = ((end & 0x1fff) << 16) | ((begain & 0x1fff) << 0);

	return vicp_reg_write(VID_CMPR_CROP_VSCOPE, value);
}
