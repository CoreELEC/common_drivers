// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "dsc_dec_drv.h"
#include "dsc_dec_hw.h"
#include "dsc_dec_debug.h"
#include "dsc_dec_reg.h"
#include "../tvin_global.h"

#define MHz	1000000
#define PIX_BAND0 (333 * MHz)
#define PIX_BAND1 (358 * MHz)
#define PIX_BAND2 (390 * MHz)
#define PIX_BAND3 (409 * MHz)

#define VPU_CLK_DIV_2 1
#define FPLL_DIV3 2
#define DSC_PIX_PLL 3
#define V_FILTER_MAX 3
#define CHUNK_BUF_NUM 8
#define HALF_CHUNK_BUF_DEPTH 175
#define BYTE_PER_FLOOR 12

int v_print_en;

unsigned int calculate_tmg_havon_begin(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int havon_tmp = 0;
	int slice_num = dsc_dec_drv->slice_num_m1 + 1;
	int slice_width;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		slice_width = dsc_dec_drv->pps_data.slice_width >> 1;
	else
		slice_width = dsc_dec_drv->pps_data.slice_width;

	switch (slice_num) {
	case 1:
		havon_tmp = slice_width >> 1;
		break;
	case 2:
		havon_tmp = slice_width * 5 / 8;
		break;
	case 4:
		havon_tmp = slice_width * 7 / 8;
		break;
	case 8:
		havon_tmp = slice_width + dsc_dec_drv->s1_de_dly;
		break;
	default:
		DSC_DEC_PR("slice_num not match value\n");
		break;
	}

/*	if (dsc_dec_drv->pps_data.pic_width == 7680)
 *		return havon_tmp + dsc_dec_drv->s0_de_dly + 50; // minus htotal need todo
 *	else
 */
	return havon_tmp + dsc_dec_drv->s0_de_dly + 50;
}

//config 4k120hz rgb 8bpc 12bpp
void init_pps_data_4k_120hz(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;
	int i;

	dsc_dec_config_fix_pll_clk(DSC_CLK_BAND0);

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 1;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
	pps_data->dsc_version_major = 1;
	pps_data->dsc_version_minor = 2;

	pps_data->pic_width = 3840;
	pps_data->pic_height = 2160;
	pps_data->slice_width = 960;
	pps_data->slice_height = 2160;

	pps_data->line_buf_depth = 13;
	pps_data->bits_per_component = 8;
	pps_data->bits_per_pixel = 192;
	dsc_dec_drv->rcb_bits = 15036;
	pps_data->rc_parameter_set.rc_model_size = 8192;
	pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
	pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
	pps_data->rc_parameter_set.rc_edge_factor = 6;
	pps_data->rc_parameter_set.rc_quant_incr_limit1 = 11;
	pps_data->rc_parameter_set.rc_quant_incr_limit0 = 11;
	pps_data->initial_xmit_delay = 341;
	pps_data->initial_dec_delay = 912;
	pps_data->initial_scale_value = 10;
	pps_data->initial_offset = 2048;
	pps_data->second_line_offset_adj = 0;
	pps_data->first_line_bpg_offset = 15;
	pps_data->second_line_bpg_offset = 0;
	pps_data->nfl_bpg_offset = 15;
	pps_data->nsl_bpg_offset = 0;

	u8 rc_buf_thresh[RC_BUF_THRESH_NUM] = {14, 28, 42, 56, 70, 84, 98, 105,
					112, 119, 121, 123, 125, 126};
	signed char range_bpg_offset[RC_RANGE_PARAMETERS_NUM] = {2, 0, 0, -2, -4, -6, -8, -8, -8,
							-10, -10, -10, -12, -12, -12};

	signed char range_max_qp[RC_RANGE_PARAMETERS_NUM] = {2, 3, 4, 5, 6, 6, 7, 8, 8, 9, 9,
							9, 9, 10, 11};
	signed char range_min_qp[RC_RANGE_PARAMETERS_NUM] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
							5, 5, 7, 10};
	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = rc_buf_thresh[i];

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		pps_data->rc_parameter_set.rc_range_parameters[i].range_bpg_offset =
		range_bpg_offset[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_max_qp =
		range_max_qp[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_min_qp =
		range_min_qp[i];
	}
	pps_data->flatness_min_qp = 3;
	pps_data->flatness_max_qp = 12;
	pps_data->scale_decrement_interval = 160;
	pps_data->scale_increment_interval = 32677;
	pps_data->slice_bpg_offset = 19;
	pps_data->final_offset = 4340;
	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	dsc_dec_drv->mux_word_size = 48;
	pps_data->chunk_size = 1440;
	dsc_dec_drv->very_flat_qp = 1;
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 7;
	/* config pps register end */
	int slice_num;

	slice_num = pps_data->pic_width / pps_data->slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->pix_per_clk = 1;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;

	if (pps_data->convert_rgb ||
	    (!pps_data->native_422 && pps_data->native_420))
		dsc_dec_drv->slice_group_number =
			(pps_data->slice_width + 2) / 3 *
			pps_data->slice_height;
	else if (pps_data->native_422 || pps_data->native_420)
		dsc_dec_drv->slice_group_number =
			((pps_data->slice_width >> 1) + 2) / 3 *
			pps_data->slice_height;

	dsc_dec_drv->partial_group_pix_num =
		(pps_data->slice_width % 3) ? 0 : 3;

	if (pps_data->native_422 || pps_data->native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1099;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    (slice_num == 4 || slice_num == 8)) {
		if (pps_data->native_422 || pps_data->native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s1_de_dly = 0x21;

	/* config timing register */
	dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 1260;
	dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 1068;
	dsc_dec_drv->tmg_ctrl.tmg_hso_end = 1112;
	dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 3;
	dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 13;
	dsc_dec_drv->tmg_cb_von_bline = 85;
	dsc_dec_drv->tmg_cb_von_eline = 2245;
	if (dsc_dec_drv->dsc_dec_en) {
		dsc_dec_config_register(dsc_dec_drv);
		dsc_dec_config_vpu_mux(dsc_dec_drv);
		set_dsc_dec_en(1);
	}
}

//config 4k60hz rgb 8bpc 12bpp
void init_pps_data_4k_60hz(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	dsc_dec_config_fix_pll_clk(DSC_CLK_BAND0);

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
	pps_data->dsc_version_major = 1;
	pps_data->dsc_version_minor = 2;

	pps_data->pic_width = 3840;
	pps_data->pic_height = 2160;
	pps_data->slice_width = 1920;
	pps_data->slice_height = 2160;

	pps_data->line_buf_depth = 13;
	pps_data->bits_per_component = 8;
	pps_data->bits_per_pixel = 192;
	dsc_dec_drv->rcb_bits = 19836;
	pps_data->rc_parameter_set.rc_model_size = 8192;
	pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
	pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
	pps_data->rc_parameter_set.rc_edge_factor = 6;
	pps_data->rc_parameter_set.rc_quant_incr_limit1 = 11;
	pps_data->rc_parameter_set.rc_quant_incr_limit0 = 11;
	pps_data->initial_xmit_delay = 341;
	pps_data->initial_dec_delay = 1312;
	pps_data->initial_scale_value = 10;
	pps_data->initial_offset = 2048;
	pps_data->second_line_offset_adj = 0;
	pps_data->first_line_bpg_offset = 15;
	pps_data->second_line_bpg_offset = 0;
	pps_data->nfl_bpg_offset = 15;
	pps_data->nsl_bpg_offset = 0;

	u8 rc_buf_thresh[RC_BUF_THRESH_NUM] = {14, 28, 42, 56, 70, 84, 98, 105,
						112, 119, 121, 123, 125, 126};
	signed char range_bpg_offset[RC_RANGE_PARAMETERS_NUM] = {2, 0, 0, -2, -4, -6, -8, -8, -8,
								-10, -10, -10, -12, -12, -12,};

	signed char range_max_qp[RC_RANGE_PARAMETERS_NUM] = {2, 3, 4, 5, 6, 6, 7, 8, 8, 9, 9,
								9, 9, 10, 11};
	signed char range_min_qp[RC_RANGE_PARAMETERS_NUM] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
								5, 5, 7, 10};
	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = rc_buf_thresh[i];

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		pps_data->rc_parameter_set.rc_range_parameters[i].range_bpg_offset =
			range_bpg_offset[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_max_qp =
			range_max_qp[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_min_qp =
			range_min_qp[i];
	}
	pps_data->flatness_min_qp = 3;
	pps_data->flatness_max_qp = 12;
	pps_data->scale_decrement_interval = 320;
	pps_data->scale_increment_interval = 44441;
	pps_data->slice_bpg_offset = 10;
	pps_data->final_offset = 4340;
	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	dsc_dec_drv->mux_word_size = 48;
	pps_data->chunk_size = 2880;
	dsc_dec_drv->very_flat_qp = 1;
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 7;
	/* config pps register end */
	int slice_num;

	slice_num = dsc_dec_drv->pps_data.pic_width / dsc_dec_drv->pps_data.slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->pix_per_clk = 1;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;

	if (dsc_dec_drv->pps_data.convert_rgb ||
	    (!dsc_dec_drv->pps_data.native_422 && !dsc_dec_drv->pps_data.native_420))
		dsc_dec_drv->slice_group_number =
			(dsc_dec_drv->pps_data.slice_width + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;
	else if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		dsc_dec_drv->slice_group_number =
			((dsc_dec_drv->pps_data.slice_width >> 1) + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;

	dsc_dec_drv->partial_group_pix_num =
		(pps_data->slice_width % 3) ? 0 : 3;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1099;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    (slice_num == 4 || slice_num == 8)) {
		if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s1_de_dly = 0x21;

	/* config timing register */
	dsc_dec_drv->tmg_ctrl.tmg_havon_begin = calculate_tmg_havon_begin(dsc_dec_drv);
	dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 1068;
	dsc_dec_drv->tmg_ctrl.tmg_hso_end = 1112;
	dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 3;
	dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 13;
	dsc_dec_drv->tmg_cb_von_bline = 85;
	dsc_dec_drv->tmg_cb_von_eline = 2245;
	if (dsc_dec_drv->dsc_dec_en) {
		dsc_dec_config_register(dsc_dec_drv);
		dsc_dec_config_vpu_mux(dsc_dec_drv);
		set_dsc_dec_en(1);
	}
}

//config 8k30hz rgb 8bpc 12bpp
void init_pps_data_8k_30hz(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	dsc_dec_config_fix_pll_clk(DSC_CLK_BAND1);

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
	pps_data->dsc_version_major = 1;
	pps_data->dsc_version_minor = 2;

	pps_data->pic_width = 7680;
	pps_data->pic_height = 4320;
	pps_data->slice_width = 1920;
	pps_data->slice_height = 2160;

	pps_data->line_buf_depth = 13;
	pps_data->bits_per_component = 8;
	pps_data->bits_per_pixel = 192;
	dsc_dec_drv->rcb_bits = 19836;
	pps_data->rc_parameter_set.rc_model_size = 8192;
	pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
	pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
	pps_data->rc_parameter_set.rc_edge_factor = 6;
	pps_data->rc_parameter_set.rc_quant_incr_limit1 = 11;
	pps_data->rc_parameter_set.rc_quant_incr_limit0 = 11;
	pps_data->initial_xmit_delay = 341;
	pps_data->initial_dec_delay = 1312;
	pps_data->initial_scale_value = 10;
	pps_data->initial_offset = 2048;
	pps_data->second_line_offset_adj = 0;
	pps_data->first_line_bpg_offset = 15;
	pps_data->second_line_bpg_offset = 0;
	pps_data->nfl_bpg_offset = 15;
	pps_data->nsl_bpg_offset = 0;

	u8 rc_buf_thresh[RC_BUF_THRESH_NUM] = {14, 28, 42, 56, 70, 84, 98, 105,
						112, 119, 121, 123, 125, 126};
	signed char range_bpg_offset[RC_RANGE_PARAMETERS_NUM] = {2, 0, 0, -2, -4, -6, -8, -8, -8,
								-10, -10, -10, -12, -12, -12,};

	signed char range_max_qp[RC_RANGE_PARAMETERS_NUM] = {2, 3, 4, 5, 6, 6, 7, 8, 8, 9, 9,
								9, 9, 10, 11};
	signed char range_min_qp[RC_RANGE_PARAMETERS_NUM] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
								5, 5, 7, 10};
	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = rc_buf_thresh[i];

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		pps_data->rc_parameter_set.rc_range_parameters[i].range_bpg_offset =
			range_bpg_offset[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_max_qp =
			range_max_qp[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_min_qp =
			range_min_qp[i];
	}
	pps_data->flatness_min_qp = 3;
	pps_data->flatness_max_qp = 12;
	pps_data->scale_decrement_interval = 320;
	pps_data->scale_increment_interval = 44441;
	pps_data->slice_bpg_offset = 10;
	pps_data->final_offset = 4340;
	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	dsc_dec_drv->mux_word_size = 48;
	pps_data->chunk_size = 2880;
	dsc_dec_drv->very_flat_qp = 1;
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 7;
	/* config pps register end */
	int slice_num;

	slice_num = dsc_dec_drv->pps_data.pic_width / dsc_dec_drv->pps_data.slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->pix_per_clk = 1;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;

	if (dsc_dec_drv->pps_data.convert_rgb ||
	    (!dsc_dec_drv->pps_data.native_422 && !dsc_dec_drv->pps_data.native_420))
		dsc_dec_drv->slice_group_number =
			(dsc_dec_drv->pps_data.slice_width + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;
	else if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		dsc_dec_drv->slice_group_number =
			((dsc_dec_drv->pps_data.slice_width >> 1) + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;

	dsc_dec_drv->partial_group_pix_num =
		(pps_data->slice_width % 3) ? 0 : 3;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 2249;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    (slice_num == 4 || slice_num == 8)) {
		if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s1_de_dly = 0x21;

	/* config timing register */
	dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 95;
	dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 2153;
	dsc_dec_drv->tmg_ctrl.tmg_hso_end = 2197;
	dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 2;
	dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 22;
	dsc_dec_drv->tmg_cb_von_bline = 65;
	dsc_dec_drv->tmg_cb_von_eline = 4385;
	if (dsc_dec_drv->dsc_dec_en) {
		dsc_dec_config_register(dsc_dec_drv);
		dsc_dec_config_vpu_mux(dsc_dec_drv);
		set_dsc_dec_en(1);
	}
}

//config 8k60hz rgb 8bpc 9.9375bpp
void init_pps_data_8k_60hz_8bpc(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	dsc_dec_config_fix_pll_clk(DSC_CLK_BAND1);

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
	pps_data->dsc_version_major = 1;
	pps_data->dsc_version_minor = 2;

	pps_data->pic_width = 7680;
	pps_data->pic_height = 4320;
	pps_data->slice_width = 960;
	pps_data->slice_height = 2160;

	pps_data->line_buf_depth = 13;
	pps_data->bits_per_component = 8;
	pps_data->bits_per_pixel = 159;
	dsc_dec_drv->rcb_bits = 11448;
	pps_data->rc_parameter_set.rc_model_size = 8192;
	pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
	pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
	pps_data->rc_parameter_set.rc_edge_factor = 6;
	pps_data->rc_parameter_set.rc_quant_incr_limit1 = 11;
	pps_data->rc_parameter_set.rc_quant_incr_limit0 = 11;
	pps_data->initial_xmit_delay = 412;
	pps_data->initial_dec_delay = 740;
	pps_data->initial_scale_value = 25;
	pps_data->initial_offset = 5648;
	pps_data->second_line_offset_adj = 0;
	pps_data->first_line_bpg_offset = 15;
	pps_data->second_line_bpg_offset = 0;
	pps_data->nfl_bpg_offset = 15;
	pps_data->nsl_bpg_offset = 0;

	u8 rc_buf_thresh[RC_BUF_THRESH_NUM] = {14, 28, 42, 56, 70, 84, 98, 105,
						112, 119, 121, 123, 125, 126};
	signed char range_bpg_offset[RC_RANGE_PARAMETERS_NUM] = {2, 0, 0, -2, -4, -6, -8, -8, -8,
								-10, -10, -10, -12, -12, -12,};

	signed char range_max_qp[RC_RANGE_PARAMETERS_NUM] = {3, 4, 5, 6, 7, 7, 7, 8, 9, 9, 10, 10,
								11, 11, 13};
	signed char range_min_qp[RC_RANGE_PARAMETERS_NUM] = {0, 0, 1, 1, 3, 3, 3, 3, 3, 3, 5,
								5, 5, 7, 12};
	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = rc_buf_thresh[i];

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		pps_data->rc_parameter_set.rc_range_parameters[i].range_bpg_offset =
			range_bpg_offset[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_max_qp =
			range_max_qp[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_min_qp =
			range_min_qp[i];
	}
	pps_data->flatness_min_qp = 3;
	pps_data->flatness_max_qp = 12;
	pps_data->scale_decrement_interval = 18;
	pps_data->scale_increment_interval = 46272;
	pps_data->slice_bpg_offset = 9;
	pps_data->final_offset = 4338;
	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	dsc_dec_drv->mux_word_size = 48;
	pps_data->chunk_size = 1193;
	dsc_dec_drv->very_flat_qp = 1;
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 7;
	/* config pps register end */
	int slice_num;

	slice_num = dsc_dec_drv->pps_data.pic_width / dsc_dec_drv->pps_data.slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->pix_per_clk = 2;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;

	if (dsc_dec_drv->pps_data.convert_rgb ||
	    (!dsc_dec_drv->pps_data.native_422 && !dsc_dec_drv->pps_data.native_420))
		dsc_dec_drv->slice_group_number =
			(dsc_dec_drv->pps_data.slice_width + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;
	else if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		dsc_dec_drv->slice_group_number =
			((dsc_dec_drv->pps_data.slice_width >> 1) + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;

	dsc_dec_drv->partial_group_pix_num =
		(pps_data->slice_width % 3) ? 0 : 3;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1660;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    (slice_num == 4 || slice_num == 8)) {
		if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s0_de_dly = 1593;
	dsc_dec_drv->s1_de_dly = 0x21;

	/* config timing register */
	dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 386;
	dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 194;
	dsc_dec_drv->tmg_ctrl.tmg_hso_end = 238;
	dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 2;
	dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 22;
	dsc_dec_drv->tmg_cb_von_bline = 65;
	dsc_dec_drv->tmg_cb_von_eline = 4385;
	if (dsc_dec_drv->dsc_dec_en) {
		dsc_dec_config_register(dsc_dec_drv);
		dsc_dec_config_vpu_mux(dsc_dec_drv);
		set_dsc_dec_en(1);
	}
}

//config 8k60hz YUV444 10bpc 9.9375bpp
void init_pps_data_8k_60hz_10bpc(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	dsc_dec_config_fix_pll_clk(DSC_CLK_BAND1);

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 0;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
	pps_data->dsc_version_major = 1;
	pps_data->dsc_version_minor = 2;

	pps_data->pic_width = 7680;
	pps_data->pic_height = 4320;
	pps_data->slice_width = 960;
	pps_data->slice_height = 2160;

	pps_data->line_buf_depth = 13;
	pps_data->bits_per_component = 10;
	pps_data->bits_per_pixel = 159;
	dsc_dec_drv->rcb_bits = 11448;
	pps_data->rc_parameter_set.rc_model_size = 8192;
	pps_data->rc_parameter_set.rc_tgt_offset_hi = 3;
	pps_data->rc_parameter_set.rc_tgt_offset_lo = 3;
	pps_data->rc_parameter_set.rc_edge_factor = 6;
	pps_data->rc_parameter_set.rc_quant_incr_limit1 = 15;
	pps_data->rc_parameter_set.rc_quant_incr_limit0 = 15;
	pps_data->initial_xmit_delay = 412;
	pps_data->initial_dec_delay = 740;
	pps_data->initial_scale_value = 25;
	pps_data->initial_offset = 5648;
	pps_data->second_line_offset_adj = 0;
	pps_data->first_line_bpg_offset = 15;
	pps_data->second_line_bpg_offset = 0;
	pps_data->nfl_bpg_offset = 15;
	pps_data->nsl_bpg_offset = 0;

	u8 rc_buf_thresh[RC_BUF_THRESH_NUM] = {14, 28, 42, 56, 70, 84, 98, 105,
						112, 119, 121, 123, 125, 126};
	signed char range_bpg_offset[RC_RANGE_PARAMETERS_NUM] = {2, 0, 0, -2, -4, -6, -8, -8, -8,
								-10, -10, -10, -12, -12, -12,};

	signed char range_max_qp[RC_RANGE_PARAMETERS_NUM] = {7, 8, 9, 10, 11, 11, 11, 12, 13, 13,
								14, 14, 15, 15, 17};
	signed char range_min_qp[RC_RANGE_PARAMETERS_NUM] = {0, 4, 5, 5, 7, 7, 7, 7, 7, 7, 9,
								9, 9, 12, 16};
	for (i = 0; i < RC_BUF_THRESH_NUM; i++)
		pps_data->rc_parameter_set.rc_buf_thresh[i] = rc_buf_thresh[i];

	for (i = 0; i < RC_RANGE_PARAMETERS_NUM; i++) {
		pps_data->rc_parameter_set.rc_range_parameters[i].range_bpg_offset =
			range_bpg_offset[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_max_qp =
			range_max_qp[i];
		pps_data->rc_parameter_set.rc_range_parameters[i].range_min_qp =
			range_min_qp[i];
	}
	pps_data->flatness_min_qp = 7;
	pps_data->flatness_max_qp = 16;
	pps_data->scale_decrement_interval = 18;
	pps_data->scale_increment_interval = 46272;
	pps_data->slice_bpg_offset = 9;
	pps_data->final_offset = 4338;
	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	dsc_dec_drv->mux_word_size = 48;
	pps_data->chunk_size = 1193;
	dsc_dec_drv->very_flat_qp = 5;
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 11;
	/* config pps register end */
	int slice_num;

	slice_num = dsc_dec_drv->pps_data.pic_width / dsc_dec_drv->pps_data.slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->pix_per_clk = 2;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;

	if (dsc_dec_drv->pps_data.convert_rgb ||
	    (!dsc_dec_drv->pps_data.native_422 && !dsc_dec_drv->pps_data.native_420))
		dsc_dec_drv->slice_group_number =
			(dsc_dec_drv->pps_data.slice_width + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;
	else if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		dsc_dec_drv->slice_group_number =
			((dsc_dec_drv->pps_data.slice_width >> 1) + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;

	dsc_dec_drv->partial_group_pix_num =
		(pps_data->slice_width % 3) ? 0 : 3;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1660;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 &&
	    (slice_num == 4 || slice_num == 8)) {
		if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s0_de_dly = 1593;
	dsc_dec_drv->s1_de_dly = 0x21;

	/* config timing register */
	dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 386;
	dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 194;
	dsc_dec_drv->tmg_ctrl.tmg_hso_end = 238;
	dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
	dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 2;
	dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 22;
	dsc_dec_drv->tmg_cb_von_bline = 65;
	dsc_dec_drv->tmg_cb_von_eline = 4385;
	if (dsc_dec_drv->dsc_dec_en) {
		dsc_dec_config_register(dsc_dec_drv);
		dsc_dec_config_vpu_mux(dsc_dec_drv);
		set_dsc_dec_en(1);
	}
}

/* integer: clk integer (M)
 * frac: clk fraction (max four place decimal)
 * config dsc dec clk
 * 1. 3000M < Target_frequency *(1<<OD)<6000M;
 * 2. DPLL_N = 1;
 * 3. DPLL_M = (Target_frequency*(1<<OD))/24
 * 4. DIV_FRAC = (Target_frequency*(1<<OD)/24-DPLL_M)*2^17
 * 5. Target_frequency = 24M*((DPLL_M+DIV_FRAC/2^17)/DPLL_N)*(1/(1<<OD))
 */
void dsc_dec_clk_calculate(unsigned int integer, unsigned int frac)
{
	int i;
	unsigned int od = 0;
	unsigned int dpll_m;
	unsigned int dpll_n = 1;
	unsigned int div_frac;
	unsigned int target_mult_value;
	unsigned int remain = 1;

	if (integer < 3000) {
		for (i = 1; i < 7; i++) { //7 is 24*(1<<7)>3000M
			if ((integer << i) > 5000) {
				od = i - 1;
				break;
			} else if ((integer << i) > 3000) {
				od = i;
				break;
			}
		}
	} else if (integer > 6000) {
		DSC_DEC_PR("config clk is too big(%d.%d)\n", integer, frac);
		return;
	}

	//calculate how many remainders
	for (i = 0; i < 4; i++) { //max four place decimal
		remain = remain * 10;
		if (!(frac / remain))
			break;
	}

	target_mult_value = integer * CLK_FRACTION + frac * CLK_FRACTION / remain;
	dpll_m = target_mult_value * (1 << od) / 24 / CLK_FRACTION;
	div_frac = (unsigned int)((target_mult_value *
			(1 << od) - dpll_m * CLK_FRACTION * 24) * (1 << 17) / 24 / CLK_FRACTION);

	DSC_DEC_PR("config clk:%d.%d(%d) od:%d m:%d n:%d div_frac:%d remain:%d config:%#x\n",
			integer, frac, target_mult_value, od, dpll_m, dpll_n, div_frac, remain,
			((dpll_n << 16) | (od << 10) | dpll_m));

	dsc_dec_config_pll_clk(od, dpll_m, dpll_n, div_frac);
}

void __weak set_dsc_clk_cntl(int clk_select, int clk)
{
}

void dsc_clk_config(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;
	u32 tmp;
	u32 clk;

	if (pps_data->native_420 == 1 || pps_data->native_422 == 1)
		tmp = 2;
	else
		tmp = 1;

	clk = pps_data->pixel_clk / (pps_data->bits_per_pixel / tmp) * 192;
	DSC_DEC_PR("pixel clk = %d, bits_per_pixel = %d\n", clk, pps_data->bits_per_pixel);
	if (dsc_dec_drv->data->chip_type == DSC_DEC_CHIP_T3X) {
		dsc_dec_config_fix_pll_clk(DSC_CLK_BAND0);
		if ((pps_data->pic_width == 3840 && pps_data->pic_height == 2160) ||
			(pps_data->pic_width == 4096 && pps_data->pic_height == 2160)) {
			if (pps_data->pixel_clk <= PIX_BAND0) {
				dsc_dec_drv->pix_per_clk = 1;
				if (pps_data->hw_vic == 97)
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND0);
				else
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND3);
			} else {
				dsc_dec_drv->pix_per_clk = 2;
				if (pps_data->pixel_clk <= PIX_BAND2)
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND2);
				else
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND4);
			}
		} else if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320) {
			dsc_dec_config_fix_pll_clk(DSC_CLK_BAND3);
			if (pps_data->pixel_clk <= PIX_BAND0) {
				//yuv420 ppc setting is different from others.
				if (dsc_dec_drv->pps_data.native_420)
					dsc_dec_drv->pix_per_clk = 2;
				else
					dsc_dec_drv->pix_per_clk = 1;
			} else {
				dsc_dec_drv->pix_per_clk = 2;
			}
		}
	} else {
		if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320) {
			set_dsc_clk_cntl(FPLL_DIV3, clk);
			if (pps_data->pixel_clk <= PIX_BAND0) {
				//yuv420 ppc setting is different from others.
				if (dsc_dec_drv->pps_data.native_420)
					dsc_dec_drv->pix_per_clk = 2;
				else
					dsc_dec_drv->pix_per_clk = 1;
			} else {
				dsc_dec_drv->pix_per_clk = 2;
			}
		} else {
			if (clk > PIX_BAND0 ||
				pps_data->dsc_force_4ppc) {
				dsc_dec_drv->pix_per_clk = 2;
				if (clk <= PIX_BAND2) {
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND2);
				} else if (clk <= PIX_BAND3) {
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND4);
				} else {
					//now only 1080p700 use dsc clk 431M
					if (pps_data->pic_width == 1920 &&
						pps_data->pic_height == 1080)
						dsc_dec_config_fix_pll_clk(DSC_CLK_BAND5);
					else
						dsc_dec_config_fix_pll_clk(DSC_CLK_BAND4);
				}
				//need vpu clk enhance
				set_dsc_clk_cntl(DSC_PIX_PLL, clk);
			} else {
				dsc_dec_drv->pix_per_clk = 1;
				set_dsc_clk_cntl(FPLL_DIV3, clk);
			}
		}
	}
}

void dsc_dec_config_init(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	struct hdmi_dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;
	int idx;
	//int input_htotal = R_DSC_DEC_BIT(DSC_ASIC_CTRL16,
		//INPUT_HTOTAL, INPUT_HTOTAL_WID);

	dsc_dec_drv->pix_per_clk = 1;
	dsc_clk_config(dsc_dec_drv);

	/* config pps register begin */
	dsc_dec_drv->full_ich_err_precision = 1;

	dsc_dec_drv->rcb_bits = 19836;

	dsc_dec_drv->flatness_det_thresh = 2 << (pps_data->bits_per_component - 8);
	if (pps_data->bits_per_component == 12)
		dsc_dec_drv->mux_word_size = 64;
	else
		dsc_dec_drv->mux_word_size = 48;
	dsc_dec_drv->very_flat_qp = 1 + 2 * (pps_data->bits_per_component - 8);
	dsc_dec_drv->somewhat_flat_qp_delta = 4;
	dsc_dec_drv->somewhat_flat_qp_thresh = 7 + 2 * (pps_data->bits_per_component - 8);
	/* config pps register end */
	int slice_num;

	slice_num = dsc_dec_drv->pps_data.pic_width / dsc_dec_drv->pps_data.slice_width;
	dsc_dec_drv->slice_num_m1 = slice_num - 1;
	dsc_dec_drv->dsc_dec_frm_latch_en = 1;
	dsc_dec_drv->c3_clk_en = 1;
	dsc_dec_drv->c2_clk_en = 1;
	dsc_dec_drv->c1_clk_en = 1;
	dsc_dec_drv->c0_clk_en = 1;
	dsc_dec_drv->aff_clr = 0;
	if (slice_num == 8)
		dsc_dec_drv->slices_in_core = 1;
	else
		dsc_dec_drv->slices_in_core = 0;

	if (dsc_dec_drv->pps_data.convert_rgb ||
	    (!dsc_dec_drv->pps_data.native_422 && !dsc_dec_drv->pps_data.native_420))
		dsc_dec_drv->slice_group_number =
			(dsc_dec_drv->pps_data.slice_width + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;
	else if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		dsc_dec_drv->slice_group_number =
			((dsc_dec_drv->pps_data.slice_width >> 1) + 2) / 3 *
			dsc_dec_drv->pps_data.slice_height;

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
		idx = 2;
	else
		idx = 1;
	//slice_width / 3 == 0 ?
	//4096 may have problem, so use calculation below.
	dsc_dec_drv->partial_group_pix_num =
		((pps_data->slice_width / idx) % 3) == 0 ? 3 : ((pps_data->slice_width / idx) % 3);

	if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420) {
		dsc_dec_drv->recon_jump_depth =
			((pps_data->slice_width >> 1) / (12 * (8 / slice_num))) + 1;
	} else {
		dsc_dec_drv->recon_jump_depth =
			(pps_data->slice_width / (12 * (8 / slice_num))) + 1;
	}
	if (dsc_dec_drv->recon_jump_depth > 80)
		dsc_dec_drv->recon_jump_depth = 80;

	dsc_dec_drv->in_swap = 0x543210;
	dsc_dec_drv->gclk_ctrl = 0;
	/* config slice overflow threshold value */
	dsc_dec_drv->c0s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c0s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c1s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c2s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s1_cb_ovfl_th		= 8 / slice_num * 350 - 1;
	dsc_dec_drv->c3s0_cb_ovfl_th		= 8 / slice_num * 350 - 1;

	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	//if (pps_data->htotal == 4000 && pps_data->hactive == 1852)
		//dsc_dec_drv->hc_htotal_m1 = 961;
	//else if (pps_data->htotal == 3920 && pps_data->hactive == 1852)
		//dsc_dec_drv->hc_htotal_m1 = 956;
	//else
	dsc_dec_drv->hc_htotal_m1 = pps_data->htotal / 2 - 1;
	dsc_dec_drv->pix_out_swap0 = 0x76543210;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xba98;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	//remove slice = 4;
	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320 && slice_num == 8) {
		if (dsc_dec_drv->pps_data.native_422 || dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width / 2 + 200;
		else
			dsc_dec_drv->s0_de_dly =
				pps_data->initial_xmit_delay / 4 + (dsc_dec_drv->hc_htotal_m1 + 1) -
				pps_data->slice_width + 200;
	} else {
		dsc_dec_drv->s0_de_dly = 0xa;
	}
	dsc_dec_drv->s1_de_dly = 0x21;

	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320) {
		dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 1260;
		if (dsc_dec_drv->pps_data.native_420)
			dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 1068;
		else
			dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 2153;
		dsc_dec_drv->tmg_ctrl.tmg_hso_end = 2197;
		dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
		dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
		dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 2;
		dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 22;
		if (pps_data->pixel_clk >= PIX_BAND0 &&
			!dsc_dec_drv->pps_data.native_420 &&
			!dsc_dec_drv->pps_data.native_422) {
			dsc_dec_drv->tmg_ctrl.tmg_havon_begin = 386;
			dsc_dec_drv->tmg_ctrl.tmg_hso_begin = 194;
			dsc_dec_drv->tmg_ctrl.tmg_hso_end = 238;
			dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
			dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
			dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 2;
			dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 22;
		}
		//now 8k has two settings, if signal unstable,vbegin and vend may
		//get wrong,so set a range, if not in range, use value from rx.
		if (abs(dsc_dec_drv->pps_data.vbegin - 164) < 10) {
			dsc_dec_drv->tmg_cb_von_bline = 165;
			dsc_dec_drv->tmg_cb_von_eline = 4485;
		} else if (abs(dsc_dec_drv->pps_data.vbegin - 64) < 10) {
			dsc_dec_drv->tmg_cb_von_bline = 65;
			dsc_dec_drv->tmg_cb_von_eline = 4385;
		} else {
			dsc_dec_drv->tmg_cb_von_bline = dsc_dec_drv->pps_data.vbegin + 1;
			dsc_dec_drv->tmg_cb_von_eline = dsc_dec_drv->pps_data.vend + 1;
		}
	} else {
		dsc_dec_drv->tmg_ctrl.tmg_havon_begin = calculate_tmg_havon_begin(dsc_dec_drv);
		dsc_dec_drv->tmg_ctrl.tmg_hso_begin = dsc_dec_drv->tmg_ctrl.tmg_havon_begin - 10;
		dsc_dec_drv->tmg_ctrl.tmg_hso_end = dsc_dec_drv->tmg_ctrl.tmg_havon_begin - 20;
		dsc_dec_drv->tmg_ctrl.tmg_vso_begin = 0;
		dsc_dec_drv->tmg_ctrl.tmg_vso_end = 0;
		dsc_dec_drv->tmg_ctrl.tmg_vso_bline = 3;
		dsc_dec_drv->tmg_ctrl.tmg_vso_eline = 13;
		dsc_dec_drv->tmg_cb_von_bline = dsc_dec_drv->pps_data.vbegin + 1;
		dsc_dec_drv->tmg_cb_von_eline = dsc_dec_drv->pps_data.vend + 1;
	}
	//now some chip flash,hc_total_m1 + 1 to work around.
	if (pps_data->pic_width == 7680 && pps_data->pic_height == 4320)
		;
	else
		dsc_dec_drv->hc_htotal_m1 += 1;
	dsc_dec_config_register(dsc_dec_drv);
	dsc_dec_config_vpu_mux(dsc_dec_drv);
	set_dsc_dec_en(1);
}

void dsc_update_info(int vstart, int vend, int odd)
{
	int vcnt;
	struct aml_dsc_dec_drv_s *dsc_dec_drv;
	struct hdmi_dsc_pps_data_s *pps_data;
	u32 tmp;
	u32 clk;
	u32 v;
	static u32 pre_v;
	static int v_stable_cnt;

	dsc_dec_drv = dsc_dec_drv_get();
	if (!dsc_dec_drv)
		return;
	pps_data = &dsc_dec_drv->pps_data;
	if (dsc_dec_drv->data->chip_type != DSC_DEC_CHIP_T6X)
		return;
	vcnt = R_DSC_DEC_BIT(DSC_ASIC_CTRL15,
		DBG_DE_BEGIN_VCNT, DBG_DE_BEGIN_VCNT_WID);
	//(175*8/(m_hslice_num+1) * 12+ 0.5* m_chunk_size) / m_chunk_size
	v = vcnt + (HALF_CHUNK_BUF_DEPTH * CHUNK_BUF_NUM /
	(dsc_dec_drv->slice_num_m1 + 1) * BYTE_PER_FLOOR +
	dsc_dec_drv->pps_data.chunk_size / 2) /
	dsc_dec_drv->pps_data.chunk_size;
	if (v_print_en)
		DSC_DEC_PR("v = %d\n", v);
	if (v == pre_v) {
		v_stable_cnt++;
	} else {
		v_stable_cnt = 0;
		pre_v = v;
	}

	if (v_stable_cnt == V_FILTER_MAX) {
		W_DSC_DEC_BIT(DSC_ASIC_CTRL10, v,
			TMG_CB_VON_BLINE, TMG_CB_VON_BLINE_WID);
		W_DSC_DEC_BIT(DSC_ASIC_CTRL10, v + pps_data->pic_height,
			TMG_CB_VON_ELINE, TMG_CB_VON_ELINE_WID);
	}

	if (pps_data->native_420 == 1 || pps_data->native_422 == 1)
		tmp = 2;
	else
		tmp = 1;
	clk = pps_data->pixel_clk / (pps_data->bits_per_pixel / tmp) * 192;
	if (clk > PIX_BAND0 || pps_data->dsc_force_4ppc) {
		dsc_dec_drv->pix_per_clk = 2;
		if (dsc_dec_drv->pix_per_clk !=
			R_DSC_DEC_BIT(DSC_ASIC_CTRL0, PIX_PER_CLK, PIX_PER_CLK_WID)) {
			W_DSC_DEC_BIT(DSC_ASIC_CTRL0, dsc_dec_drv->pix_per_clk,
				PIX_PER_CLK, PIX_PER_CLK_WID);
			if (clk <= PIX_BAND2) {
				dsc_dec_config_fix_pll_clk(DSC_CLK_BAND2);
			} else if (clk <= PIX_BAND3) {
				dsc_dec_config_fix_pll_clk(DSC_CLK_BAND4);
			} else {
				//now only 1080p700 use dsc clk 431M
				if (pps_data->pic_width == 1920 &&
					pps_data->pic_height == 1080)
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND5);
				else
					dsc_dec_config_fix_pll_clk(DSC_CLK_BAND4);
			}
			//need vpu clk enhance
			set_dsc_clk_cntl(DSC_PIX_PLL, clk);
			wr_bits(0, VPU_VDIN_HDMI0_CTRL1_T6X, 1, 30, 1);
			wr_bits(0, VPU_VDIN_HDMI0_CTRL1_T6X, 1, 29, 1);
		}
	} else {
		dsc_dec_drv->pix_per_clk = 1;
		if (dsc_dec_drv->pix_per_clk !=
			R_DSC_DEC_BIT(DSC_ASIC_CTRL0, PIX_PER_CLK, PIX_PER_CLK_WID)) {
			W_DSC_DEC_BIT(DSC_ASIC_CTRL0, dsc_dec_drv->pix_per_clk,
			PIX_PER_CLK, PIX_PER_CLK_WID);
			set_dsc_clk_cntl(FPLL_DIV3, clk);
		}
	}
}
