// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include "vpq_module_dump.h"
#include "../vpq_printk.h"
#include "../vpq_common.h"

static void dump_dnlp_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump dnlp value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_DNLP_CURVE_LEN; i++) {
		vpq_pr_dbg(lev_mod, "scurv_low/mid1/mid2/hgh1/hgh2[%d]:%d %d %d %d %d\n", i,
			pq_table_param.dnlp_table[index][value].param.dnlp_scurv_low[i],
			pq_table_param.dnlp_table[index][value].param.dnlp_scurv_mid1[i],
			pq_table_param.dnlp_table[index][value].param.dnlp_scurv_mid2[i],
			pq_table_param.dnlp_table[index][value].param.dnlp_scurv_hgh1[i],
			pq_table_param.dnlp_table[index][value].param.dnlp_scurv_hgh2[i]);
	}
	for (i = 0; i < VPQ_DNLP_GAIN_VAR_LUT_LEN; i++)
		vpq_pr_dbg(lev_mod, "gain_var_lut49[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.gain_var_lut49[i]);
	for (i = 0; i < VPQ_DNLP_WEXT_GAIN_LEN; i++)
		vpq_pr_dbg(lev_mod, "wext_gain[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.wext_gain[i]);
	for (i = 0; i < VPQ_DNLP_ADP_THRD_LEN; i++)
		vpq_pr_dbg(lev_mod, "adp_thrd[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.adp_thrd[i]);
	for (i = 0; i < VPQ_DNLP_REG_BLK_BOOST_LEN; i++)
		vpq_pr_dbg(lev_mod, "reg_blk_boost_12[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.reg_blk_boost_12[i]);
	for (i = 0; i < VPQ_DNLP_REG_ADP_OFSET_LEN; i++)
		vpq_pr_dbg(lev_mod, "reg_adp_ofset_20[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.reg_adp_ofset_20[i]);
	for (i = 0; i < VPQ_DNLP_REG_MONO_PROT_LEN; i++)
		vpq_pr_dbg(lev_mod, "reg_mono_protect[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.reg_mono_protect[i]);
	for (i = 0; i < VPQ_DNLP_TREND_WHT_EXP_LUT_LEN; i++)
		vpq_pr_dbg(lev_mod, "reg_trend_wht_expand_lut8[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.reg_trend_wht_expand_lut8[i]);
	for (i = 0; i < VPQ_DNLP_HIST_GAIN_LEN; i++) {
		vpq_pr_dbg(lev_mod, "c_hist_gain/s_hist_gain[%d]:%d %d\n", i,
			pq_table_param.dnlp_table[index][value].param.c_hist_gain[i],
			pq_table_param.dnlp_table[index][value].param.s_hist_gain[i]);
	}
	for (i = 0; i < VPQ_DNLP_PARAM_MAX; i++)
		vpq_pr_dbg(lev_mod, "reg_trend_wht_expand_lut8[%d]:%d\n", i,
			pq_table_param.dnlp_table[index][value].param.param[i]);
}

static void dump_hdrtmo_table(int value, int index)
{
	vpq_pr_dbg(lev_mod, "dump hdr tmo value:%d index:%d start\n", value, index);
	if (index == 255)
		return;
}

static void dump_aipq_table(int index)
{
	int i = 0;
	int j = 0;

	vpq_pr_dbg(lev_mod, "dump aipq index:%d start\n", index);
	if (index == 255)
		return;

	vpq_pr_dbg(lev_mod, "height/width:%d %d\n",
		pq_table_param.aipq_table[index].size.height,
		pq_table_param.aipq_table[index].size.width);
	for (i = 0; i < AIPQ_ROWS_MAX; i++) {
		for (j = 0; j < AIPQ_COLS_MAX; j++) {
			vpq_pr_dbg(lev_mod, "aipq_table[%d][%d]:%d\n", i, j,
				pq_table_param.aipq_table[index].aipq_table[i][j]);
		}
	}
}

static void dump_lc_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump lc value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < 63; i++)
		vpq_pr_dbg(lev_mod, "lc_saturation[%d]:%d\n", i,
			pq_table_param.lc_table[index][value].curve.lc_saturation[i]);
	for (i = 0; i < 16; i++) {
		vpq_pr_dbg(lev_mod, "lc_yminval/ypkbv_ymaxval/ymaxval/ypkbv_lmt[%d]:%d %d %d %d\n",
			i,
			pq_table_param.lc_table[index][value].curve.lc_yminval_lmt[i],
			pq_table_param.lc_table[index][value].curve.lc_ypkbv_ymaxval_lmt[i],
			pq_table_param.lc_table[index][value].curve.lc_ymaxval_lmt[i],
			pq_table_param.lc_table[index][value].curve.lc_ypkbv_lmt[i]);
	}
	for (i = 0; i < 4; i++)
		vpq_pr_dbg(lev_mod, "lc_ypkbv_ratio[%d]:%d\n", i,
			pq_table_param.lc_table[index][value].curve.lc_ypkbv_ratio[i]);
	for (i = 0; i < 18; i++)
		vpq_pr_dbg(lev_mod, "param.param[%d]:%d\n", i,
			pq_table_param.lc_table[index][value].param.param[i]);
}

static void dump_ccoring_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump chroma coring value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_CCORING_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.ccoring_table[index][value].param.param[i]);
}

static void dump_blackext_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump blackext value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_BLKEXT_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.blkext_table[index][value].param.param[i]);
}

static void dump_blue_stretch_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump blue stretch value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_BLUE_STRETCH_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.bls_table[index][value].param.param[i]);
}

static void dump_cm_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump cm value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < 9; i++)
		vpq_pr_dbg(lev_mod, "sat_by_y_0[%d]:%d\n", i,
			pq_table_param.cm_base_table[index][value].sat_by_y_0[i]);
	for (i = 0; i < 32; i++)
		vpq_pr_dbg(lev_mod, "luma_by_hue_0[%d]:%d\n", i,
			pq_table_param.cm_base_table[index][value].luma_by_hue_0[i]);
	for (i = 0; i < 32; i++) {
		vpq_pr_dbg(lev_mod, "sat_by_hs_0/1/2[%d]:%d %d %d\n", i,
			pq_table_param.cm_base_table[index][value].sat_by_hs_0[i],
			pq_table_param.cm_base_table[index][value].sat_by_hs_1[i],
			pq_table_param.cm_base_table[index][value].sat_by_hs_2[i]);
	}
	for (i = 0; i < 32; i++)
		vpq_pr_dbg(lev_mod, "hue_by_hue_0[%d]:%d\n", i,
			pq_table_param.cm_base_table[index][value].hue_by_hue_0[i]);
	for (i = 0; i < 32; i++) {
		vpq_pr_dbg(lev_mod, "hue_by_hy_0/1/2/3/4[%d]:%d %d %d %d %d\n", i,
			pq_table_param.cm_base_table[index][value].hue_by_hy_0[i],
			pq_table_param.cm_base_table[index][value].hue_by_hy_1[i],
			pq_table_param.cm_base_table[index][value].hue_by_hy_2[i],
			pq_table_param.cm_base_table[index][value].hue_by_hy_3[i],
			pq_table_param.cm_base_table[index][value].hue_by_hy_4[i]);
	}
	for (i = 0; i < 32; i++) {
		vpq_pr_dbg(lev_mod, "hue_by_hs_0/1/2/3/4[%d]:%d %d %d %d %d\n", i,
			pq_table_param.cm_base_table[index][value].hue_by_hs_0[i],
			pq_table_param.cm_base_table[index][value].hue_by_hs_1[i],
			pq_table_param.cm_base_table[index][value].hue_by_hs_2[i],
			pq_table_param.cm_base_table[index][value].hue_by_hs_3[i],
			pq_table_param.cm_base_table[index][value].hue_by_hs_4[i]);
	}
	for (i = 0; i < 32; i++) {
		vpq_pr_dbg(lev_mod, "sat_by_hy_0/1/2/3/4[%d]:%d %d %d %d %d\n", i,
			pq_table_param.cm_base_table[index][value].sat_by_hy_0[i],
			pq_table_param.cm_base_table[index][value].sat_by_hy_1[i],
			pq_table_param.cm_base_table[index][value].sat_by_hy_2[i],
			pq_table_param.cm_base_table[index][value].sat_by_hy_3[i],
			pq_table_param.cm_base_table[index][value].sat_by_hy_4[i]);
	}
}

static void dump_nr_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump nr value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_REG_DNR_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.nr_table[index][value].param.param[i]);
}

static void dump_deblock_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump deblock value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_REG_DBLK_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.dblk_table[index][value].param.param[i]);
}

static void dump_demosquito_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump demosquito value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_REG_DM_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.demos_table[index][value].param.param[i]);
}

static void dump_decontour_table(int value, int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump decontour value:%d index:%d start\n", value, index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_REG_DCT_MAX; i++)
		vpq_pr_dbg(lev_mod, "param[%d]:%d\n", i,
			pq_table_param.dct_table[index][value].param.param[i]);
}

static void dump_di_table(int index)
{
	int i = 0;

	vpq_pr_dbg(lev_mod, "dump di index:%d start\n", index);
	if (index == 255)
		return;

	for (i = 0; i < VPQ_REG_XLR_MAX; i++)
		vpq_pr_dbg(lev_mod, "di xlr param[%d]:%d\n", i,
			pq_table_param.di_table[index].param.param[i]);
}

int vpq_module_dump_pq_table(int module)
{
	switch (module) {
	case 0:
		dump_dnlp_table(pq_setting_val[PQ_DNLP], pq_table_idx[PQ_INDEX_DNLP]);
		break;
	case 1:
		dump_hdrtmo_table(pq_setting_val[PQ_HDR_TMO], pq_table_idx[PQ_INDEX_HDR_TMO]);
		break;
	case 2:
		dump_aipq_table(pq_table_idx[PQ_INDEX_AIPQ]);
		break;
	case 3:
		dump_lc_table(pq_setting_val[PQ_LC], pq_table_idx[PQ_INDEX_LC]);
		break;
	case 4:
		dump_ccoring_table(pq_setting_val[PQ_CCORING], pq_table_idx[PQ_INDEX_CCORING]);
		break;
	case 5:
		dump_blackext_table(pq_setting_val[PQ_BLK], pq_table_idx[PQ_INDEX_BLK]);
		break;
	case 6:
		dump_blue_stretch_table(pq_setting_val[PQ_BLS], pq_table_idx[PQ_INDEX_BLS]);
		break;
	case 7:
		dump_cm_table(pq_setting_val[PQ_CM], pq_table_idx[PQ_INDEX_CM2]);
		break;

	case 20:
		dump_nr_table(pq_setting_val[PQ_NR], pq_table_idx[PQ_INDEX_NR]);
		break;
	case 21:
		dump_deblock_table(pq_setting_val[PQ_DEBLOCK], pq_table_idx[PQ_INDEX_DEBLOCK]);
		break;
	case 22:
		dump_demosquito_table(pq_setting_val[PQ_DEMOSQUITO],
			pq_table_idx[PQ_INDEX_DEMOSQUITO]);
		break;
	case 23:
		dump_decontour_table(pq_setting_val[PQ_SMOOTHPLUS],
			pq_table_idx[PQ_INDEX_SMOOTHPLUS]);
		break;
	case 24:
		dump_di_table(pq_table_idx[PQ_INDEX_DI]);
		break;
	default:
		vpq_pr_dbg(lev_mod, "unknown module:%d\n", module);
		break;
	}

	return 0;
}
