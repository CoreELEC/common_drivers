// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include "dsc_dec_drv.h"
#include "dsc_dec_hw.h"
#include "dsc_dec_debug.h"

//config 4k120hz 444 8bpc 12bpp
void init_pps_data(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
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

	dsc_dec_drv->recon_jump_depth = 0x29;
	dsc_dec_drv->in_swap = 0x435102;
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

	dsc_dec_drv->s0_de_dly = 0xa;
	dsc_dec_drv->s1_de_dly = 0x21;
	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1099; //compress htotal from HDMI RX (hc_active+hc_blank)/2 - 1
	dsc_dec_drv->pix_out_swap0 = 0x68435102;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xa967;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	dsc_dec_config_register(dsc_dec_drv);
}

//config 4k60hz 444 8bpc 12bpp
void init_pps_data_v1(struct aml_dsc_dec_drv_s *dsc_dec_drv)
{
	int i = 0;
	struct dsc_pps_data_s *pps_data = &dsc_dec_drv->pps_data;

	/* config pps register begin */
	pps_data->native_422 = 0;
	pps_data->native_420 = 0;
	pps_data->convert_rgb = 1;
	pps_data->block_pred_enable = 1;
	pps_data->vbr_enable = 0;
	dsc_dec_drv->full_ich_err_precision = 1;
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

	dsc_dec_drv->recon_jump_depth = 0x29;
	dsc_dec_drv->in_swap = 0x435102;
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

	dsc_dec_drv->s0_de_dly = 0xa;
	dsc_dec_drv->s1_de_dly = 0x21;
	dsc_dec_drv->hc_htotal_offs_oddline = 0;
	dsc_dec_drv->hc_htotal_offs_evenline = 0;
	dsc_dec_drv->hc_htotal_m1 = 1099; //compress htotal from HDMI RX (hc_active+hc_blank)/2 - 1
	dsc_dec_drv->pix_out_swap0 = 0x68435102;
	dsc_dec_drv->intr_maskn = 0;
	dsc_dec_drv->pix_out_swap1 = 0xa967;
	dsc_dec_drv->clr_bitstream_fetch = 0;
	dsc_dec_drv->dbg_vcnt = 10;
	dsc_dec_drv->dbg_hcnt = 0;

	dsc_dec_config_register(dsc_dec_drv);
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
	// need todo
	DSC_DEC_PR("need todo calculate dsc clk\n");
}
