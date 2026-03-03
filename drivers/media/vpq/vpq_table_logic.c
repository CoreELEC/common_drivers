// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include <linux/amlogic/media/vpp/vpp_drv.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include "vpq_table_logic.h"
#include "vpq_printk.h"
#include "vpq_common.h"
#include "vpq_vfm.h"
#include "modules/vpq_module_cm.h"
#include "modules/vpq_module_timing.h"
#include "modules/vpq_module_dump.h"
#include "../vpp/vpp_common.h"
#include "../vpp/vpp_pq_mgr.h"
#include "../dpss/drv/d_pq_mgr.h"

// whole static variables
static enum vpq_chip_class_e chip_type;
static enum vpq_chip_e chip_id;
static int gma_point;

static struct vpq_rgb_ogo_s rgb_ogo = {0};
static int base_curve_sat_by_y[CURVE_LEN_SAT_BY_Y * CURVE_NUM_SAT_BY_Y] = {0};
static int base_curve_luma_by_hue[CURVE_LEN_LUMA_BY_HUE * CURVE_NUM_LUMA_BY_HUE] = {0};
static int base_curve_sat_by_hs[CURVE_LEN_SAT_BY_HS * CURVE_NUM_SAT_BY_HS] = {0};
static int base_curve_hue_by_hue[CURVE_LEN_HUE_BY_HUE * CURVE_NUM_HUE_BY_HUE] = {0};
static int base_curve_hue_by_hy[CURVE_LEN_HUE_BY_HY * CURVE_NUM_HUE_BY_HY] = {0};
static int base_curve_hue_by_hs[CURVE_LEN_HUE_BY_HS * CURVE_NUM_HUE_BY_HS] = {0};
static int base_curve_sat_by_hy[CURVE_LEN_SAT_BY_HY * CURVE_NUM_SAT_BY_HY] = {0};
static int curve_sat_by_hs[CURVE_LEN_SAT_BY_HS * CURVE_NUM_SAT_BY_HS] = {0};
static int curve_hue_by_hue[CURVE_LEN_HUE_BY_HUE * CURVE_NUM_HUE_BY_HUE] = {0};
static int curve_luma_by_hue[CURVE_LEN_LUMA_BY_HUE * CURVE_NUM_LUMA_BY_HUE] = {0};
static struct vpq_cms_s cms_param = {0};
static struct vpq_light_sensor_s pre_light_sensor = {0};

static struct pq_cm_base_param_s *pcm_base_data;

// whole variables
unsigned int pq_setting_val[PQ_SETTING_MAX] = {
	[0 ... PQ_SETTING_MAX - 1] = 255
};

unsigned char pq_table_idx[PQ_INDEX_MAX] = {
	[0 ... PQ_INDEX_MAX - 1] = 0xFF
};

static void _buffer_init(void)
{
	// TODO
}

static void _buffer_free(void)
{
	pcm_base_data =  NULL;
}

void vpq_table_init(struct vpq_dev_s *pdev)
{
	if (!pdev)
		return;

	chip_type = pdev->pm_data->chip_type;
	chip_id = pdev->pm_data->chip_id;
	gma_point = vpq_get_gamma_table_point();

	_buffer_init();

	vpq_pr_info("chip_type:%d, chip_id:%d, gma_point:%d\n", chip_type, chip_id, gma_point);
}

void vpq_table_deinit(void)
{
	_buffer_free();
}

/*
 * update vpq pq table version info by client
 */
int vpq_set_pq_table_version(struct vpq_table_ver_info_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	if (memcpy(&pq_table_ver, (struct TABLE_VER_PQ *)pdata,
			sizeof(struct TABLE_VER_PQ)) == NULL) {
		vpq_pr_err("fail to update driver memory\n");
		return RET_NULL_POINT;
	}

	return 0;
}

/*
 * update vpq default pq table by client
 */
int vpq_set_default_pq_table(struct vpq_table_bin_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	if (memcpy(&pq_table_param, (struct PQ_TABLE_PARAM *)pdata->ptr,
			sizeof(struct PQ_TABLE_PARAM)) == NULL) {
		vpq_pr_err("fail to update driver memory\n");
		return RET_NULL_POINT;
	}

	vpq_pr_info("len:0x%x, size:0x%zx\n", pdata->len, sizeof(struct PQ_TABLE_PARAM));
	return 0;
}

/*
 * update customer non-standard timing config information by client
 */
int vpq_set_nonstandard_timing_map(unsigned char value,
	struct vpq_nonstandard_timing_map_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpq_module_timing_set_nonstandard_map(value,
			(struct nonstandard_timing_map_s *)pdata);
}

/*
 * update pq module config by client
 */
int vpq_set_pq_module_cfg(struct vpq_pq_module_cfg_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	memcpy(&pq_module_cfg, pdata, sizeof(struct TABLE_PQ_MODULE_CFG));

	struct vpp_pq_state_s state = {
		.pq_en = pq_module_cfg.pq_en,
		.pq_cfg = {
			.vadj1_en = pq_module_cfg.vadj1_en,
			.vd1_ctrst_en = pq_module_cfg.vd1_ctrst_en,
			.vadj2_en = pq_module_cfg.vadj2_en,
			.post_ctrst_en = pq_module_cfg.post_ctrst_en,
			.pregamma_en = pq_module_cfg.pregamma_en,
			.gamma_en = pq_module_cfg.gamma_en,
			.wb_en = pq_module_cfg.wb_en,
			.dnlp_en = pq_module_cfg.dnlp_en,
			.lc_en = pq_module_cfg.lc_en,
			.black_ext_en = pq_module_cfg.black_ext_en,
			.blue_stretch_en = pq_module_cfg.blue_stretch_en,
			.chroma_cor_en = pq_module_cfg.chroma_cor_en,
			.sharpness0_en = pq_module_cfg.sharpness0_en,
			.sharpness1_en = pq_module_cfg.sharpness1_en,
			.cm_en = pq_module_cfg.cm_en,
			.lut3d_en = pq_module_cfg.lut3d_en,
			.dejaggy_sr0_en = pq_module_cfg.dejaggy_sr0_en,
			.dejaggy_sr1_en = pq_module_cfg.dejaggy_sr1_en,
			.dering_sr0_en = pq_module_cfg.dering_sr0_en,
			.dering_sr1_en = pq_module_cfg.dering_sr1_en
		}
	};

	return vpp_pq_mgr_set_status(&state);
}

/*
 * get current pq module state from dev/vpp
 */
void vpq_get_pq_module_status(struct vpq_pq_state_s *pdata)
{
	struct vpp_pq_state_s state = {0};

	vpp_pq_mgr_get_status(&state);

	pdata->pq_en                  = state.pq_en;
	pdata->pq_cfg.vadj1_en        = state.pq_cfg.vadj1_en;
	pdata->pq_cfg.vd1_ctrst_en    = state.pq_cfg.vd1_ctrst_en;
	pdata->pq_cfg.vadj2_en        = state.pq_cfg.vadj2_en;
	pdata->pq_cfg.post_ctrst_en   = state.pq_cfg.post_ctrst_en;
	pdata->pq_cfg.pregamma_en     = state.pq_cfg.pregamma_en;
	pdata->pq_cfg.gamma_en        = state.pq_cfg.gamma_en;
	pdata->pq_cfg.wb_en           = state.pq_cfg.wb_en;
	pdata->pq_cfg.dnlp_en         = state.pq_cfg.dnlp_en;
	pdata->pq_cfg.lc_en           = state.pq_cfg.lc_en;
	pdata->pq_cfg.black_ext_en    = state.pq_cfg.black_ext_en;
	pdata->pq_cfg.blue_stretch_en = state.pq_cfg.blue_stretch_en;
	pdata->pq_cfg.chroma_cor_en   = state.pq_cfg.chroma_cor_en;
	pdata->pq_cfg.sharpness0_en   = state.pq_cfg.sharpness0_en;
	pdata->pq_cfg.sharpness1_en   = state.pq_cfg.sharpness1_en;
	pdata->pq_cfg.cm_en           = state.pq_cfg.cm_en;
	pdata->pq_cfg.lut3d_en        = state.pq_cfg.lut3d_en;
	pdata->pq_cfg.dejaggy_sr0_en  = state.pq_cfg.dejaggy_sr0_en;
	pdata->pq_cfg.dejaggy_sr1_en  = state.pq_cfg.dejaggy_sr1_en;
	pdata->pq_cfg.dering_sr0_en   = state.pq_cfg.dering_sr0_en;
	pdata->pq_cfg.dering_sr1_en   = state.pq_cfg.dering_sr1_en;
}

/*
 * set pq module status on/off
 */
int vpq_set_pq_module_status(enum vpq_module_e module, int status)
{
	return vpp_pq_mgr_set_module_status((enum vpp_module_e)module, (bool)status);
}

/*
 * get current chip class
 */
enum vpq_chip_class_e vpq_get_chip_type(void)
{
	return chip_type;
}

/*
 * get current chip id
 */
enum vpq_chip_e vpq_get_chip_id(void)
{
	return chip_id;
}

/*
 * VPP pq function
 */
int vpq_set_brightness(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj1_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_brightness(value);
	if (ret == 0)
		pq_setting_val[PQ_BRIG] = value;

	return ret;
}

int vpq_set_contrast(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj1_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_contrast(value);
	if (ret == 0)
		pq_setting_val[PQ_CONT] = value;

	return ret;
}

int vpq_set_saturation(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj1_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_saturation(value);
	if (ret == 0)
		pq_setting_val[PQ_SAT] = value;

	return ret;
}

int vpq_set_hue(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj1_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_hue(value);
	if (ret == 0)
		pq_setting_val[PQ_HUE] = value;

	return ret;
}

int vpq_set_sharpness(int value)
{
	int ret = 0;

	if (!pq_module_cfg.sharpness0_en && !pq_module_cfg.sharpness1_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_sharpness(value);
	if (ret == 0)
		pq_setting_val[PQ_SHARPNESS] = value;

	return ret;
}

int vpq_set_brightness_post(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj2_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_brightness_post(value);

	return ret;
}

int vpq_set_contrast_post(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj2_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_contrast_post(value);

	return ret;
}

int vpq_set_saturation_post(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj2_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_saturation_post(value);

	return ret;
}

int vpq_set_hue_post(int value)
{
	int ret = 0;

	if (!pq_module_cfg.vadj2_en)
		return REF_CFG_DISABLED;

	ret = vpp_pq_mgr_set_hue_post(value);

	return ret;
}

int vpq_get_gamma_table_point(void)
{
	switch (chip_id) {
	case VPQ_CHIP_T6W:    return VPQ_GAMMA_TABLE_POINT_257;
	case VPQ_CHIP_T6X:    return VPQ_GAMMA_TABLE_POINT_257;
	default:              return VPQ_GAMMA_TABLE_POINT_256;
	}
}

int vpq_set_gamma_table(struct vpq_gamma_table_s *pdata)
{
	int ret = 0;
	int gma_buf = 0;
	int i = 0;
	struct vpp_gamma_table_s gma_table = {0};

	if (!pq_module_cfg.gamma_en)
		return REF_CFG_DISABLED;

	if (!pdata || !pdata->r_data || !pdata->g_data || !pdata->b_data)
		return RET_NULL_POINT;

	gma_buf = sizeof(unsigned int) * gma_point;
	gma_table.r_data = kmalloc(gma_buf, GFP_KERNEL);
	gma_table.g_data = kmalloc(gma_buf, GFP_KERNEL);
	gma_table.b_data = kmalloc(gma_buf, GFP_KERNEL);
	if (!gma_table.r_data || !gma_table.g_data || !gma_table.b_data) {
		ret = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < gma_point; i++) {
		gma_table.r_data[i] = (unsigned int)pdata->r_data[i];
		gma_table.g_data[i] = (unsigned int)pdata->g_data[i];
		gma_table.b_data[i] = (unsigned int)pdata->b_data[i];
	}
	ret = vpp_pq_mgr_set_gamma_table(&gma_table);

exit:
	kfree(gma_table.r_data);
	kfree(gma_table.g_data);
	kfree(gma_table.b_data);
	return ret;
}

int vpq_set_rgb_ogo(struct vpq_rgb_ogo_s *pdata)
{
	int ret = 0;

	if (!pq_module_cfg.wb_en)
		return REF_CFG_DISABLED;

	if (memcmp(pdata, &rgb_ogo, sizeof(struct vpq_rgb_ogo_s)) == 0) {
		vpq_pr_dbg(lev_tab, "rgb ogo same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	ret = vpp_pq_mgr_set_whitebalance((struct vpp_white_balance_s *)pdata);
	if (ret == 0)
		memcpy(&rgb_ogo, pdata, sizeof(struct vpq_rgb_ogo_s));

	return ret;
}

int vpq_set_matrix_param(struct vpq_mtrx_info_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_matrix_param((struct vpp_mtrx_info_s *)pdata);
}

static int set_color_curves(struct pq_cm_base_param_s *param)
{
	int ret = 0;
	int i = 0;

	// LumaByHue
	for (i = 0; i < CURVE_LEN_LUMA_BY_HUE; i++)
		base_curve_luma_by_hue[i] = param->luma_by_hue_0[i];
	ret = vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, base_curve_luma_by_hue);

	// SatByHS
	for (i = 0; i < CURVE_LEN_SAT_BY_HS; i++) {
		base_curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 0] = param->sat_by_hs_0[i];
		base_curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 1] = param->sat_by_hs_1[i];
		base_curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 2] = param->sat_by_hs_2[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT, base_curve_sat_by_hs);

	// HueByHue
	for (i = 0; i < CURVE_LEN_HUE_BY_HUE; i++)
		base_curve_hue_by_hue[i] = param->hue_by_hue_0[i];
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE, base_curve_hue_by_hue);

	// HueByHS
	for (i = 0; i < CURVE_LEN_HUE_BY_HS; i++) {
		base_curve_hue_by_hs[i + CURVE_LEN_HUE_BY_HS * 0] = param->hue_by_hs_0[i];
		base_curve_hue_by_hs[i + CURVE_LEN_HUE_BY_HS * 1] = param->hue_by_hs_1[i];
		base_curve_hue_by_hs[i + CURVE_LEN_HUE_BY_HS * 2] = param->hue_by_hs_2[i];
		base_curve_hue_by_hs[i + CURVE_LEN_HUE_BY_HS * 3] = param->hue_by_hs_3[i];
		base_curve_hue_by_hs[i + CURVE_LEN_HUE_BY_HS * 4] = param->hue_by_hs_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HS, base_curve_hue_by_hs);

	// SatByY
	for (i = 0; i < CURVE_LEN_SAT_BY_Y; i++)
		base_curve_sat_by_y[i] = param->sat_by_y_0[i];
	/* coverity[overrun-buffer-val] */
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_L, base_curve_sat_by_y);

	// SatByHY
	for (i = 0; i < CURVE_LEN_SAT_BY_HY; i++) {
		base_curve_sat_by_hy[i + CURVE_LEN_SAT_BY_HY * 0] = param->sat_by_hy_0[i];
		base_curve_sat_by_hy[i + CURVE_LEN_SAT_BY_HY * 1] = param->sat_by_hy_1[i];
		base_curve_sat_by_hy[i + CURVE_LEN_SAT_BY_HY * 2] = param->sat_by_hy_2[i];
		base_curve_sat_by_hy[i + CURVE_LEN_SAT_BY_HY * 3] = param->sat_by_hy_3[i];
		base_curve_sat_by_hy[i + CURVE_LEN_SAT_BY_HY * 4] = param->sat_by_hy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_SAT_BY_HL, base_curve_sat_by_hy);

	// HueByHY
	for (i = 0; i < CURVE_LEN_HUE_BY_HY; i++) {
		base_curve_hue_by_hy[i + CURVE_LEN_HUE_BY_HY * 0] = param->hue_by_hy_0[i];
		base_curve_hue_by_hy[i + CURVE_LEN_HUE_BY_HY * 1] = param->hue_by_hy_1[i];
		base_curve_hue_by_hy[i + CURVE_LEN_HUE_BY_HY * 2] = param->hue_by_hy_2[i];
		base_curve_hue_by_hy[i + CURVE_LEN_HUE_BY_HY * 3] = param->hue_by_hy_3[i];
		base_curve_hue_by_hy[i + CURVE_LEN_HUE_BY_HY * 4] = param->hue_by_hy_4[i];
	}
	ret |= vpp_pq_mgr_set_cm_curve(EN_CM_HUE_BY_HL, base_curve_hue_by_hy);

	return ret;
}

int vpq_set_color_base(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct pq_cm_base_param_s *param;

	if (!pq_module_cfg.cm_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_CM2);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_CM2] &&
		value == pq_setting_val[PQ_CM]) {
		vpq_pr_dbg(lev_tab, "color base same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	param = &pq_table_param.cm_base_table[index][(enum pq_table_level_e)value];
	ret = set_color_curves(param);
	if (ret == 0) {
		pq_setting_val[PQ_CM] = value;
		pq_table_idx[PQ_INDEX_CM2] = index;
		pcm_base_data = param;
	}

	return ret;
}

static int config_adj_param(struct vpq_cms_s *pdata, struct color_adj_param_s *param)
{
	if (!pdata || !param)
		return RET_NULL_POINT;

	param->color_type = (enum color_adj_type_e)pdata->color_type;
	param->value = pdata->value;

	switch (pdata->color_9) {
	case VPQ_COLOR_RED:
		param->mode_9 = EN_CAM9_RED;
		break;
	case VPQ_COLOR_GREEN:
		param->mode_9 = EN_CAM9_GREEN;
		break;
	case VPQ_COLOR_BLUE:
		param->mode_9 = EN_CAM9_BLUE;
		break;
	case VPQ_COLOR_CYAN:
		param->mode_9 = EN_CAM9_CYAN;
		break;
	case VPQ_COLOR_MAGENTA:
		param->mode_9 = EN_CAM9_PURPLE;
		break;
	case VPQ_COLOR_YELLOW:
		param->mode_9 = EN_CAM9_YELLOW;
		break;
	case VPQ_COLOR_SKIN:
		param->mode_9 = EN_CAM9_SKIN;
		break;
	case VPQ_COLOR_YELLOW_GREEN:
		param->mode_9 = EN_CAM9_YELLOW_GREEN;
		break;
	case VPQ_COLOR_BLUE_GREEN:
		param->mode_9 = EN_CAM9_BLUE_GREEN;
		break;
	default:
		return RET_INVALID_INPUT;
	}

	return 0;
}

static int apply_saturation_adjustment(struct color_adj_param_s *param, int *curve)
{
	int i = 0;

	param->curve_type = EN_COLOR_ADJ_CURVE_SAT;
	color_adj_by_mode(*param, curve);

	for (i = 0; i < CURVE_LEN_SAT_BY_HS; i++) {
		curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 0] =
			pcm_base_data->sat_by_hs_0[i] + curve[i];
		curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 1] =
			pcm_base_data->sat_by_hs_1[i] + curve[i];
		curve_sat_by_hs[i + CURVE_LEN_SAT_BY_HS * 2] =
			pcm_base_data->sat_by_hs_2[i] + curve[i];
	}
	return vpp_pq_mgr_set_cm_curve(EN_CM_SAT, curve_sat_by_hs);
}

static int apply_hue_adjustment(struct color_adj_param_s *param, int *curve)
{
	int i = 0;

	param->curve_type = EN_COLOR_ADJ_CURVE_HUE;
	color_adj_by_mode(*param, curve);

	for (i = 0; i < CURVE_LEN_HUE_BY_HUE; i++)
		curve_hue_by_hue[i] = pcm_base_data->hue_by_hue_0[i] + curve[i];
	return vpp_pq_mgr_set_cm_curve(EN_CM_HUE, curve_hue_by_hue);
}

static int apply_luma_adjustment(struct color_adj_param_s *param, int *curve)
{
	int i = 0;

	param->curve_type = EN_COLOR_ADJ_CURVE_LUMA;
	color_adj_by_mode(*param, curve);

	for (i = 0; i < CURVE_LEN_LUMA_BY_HUE; i++)
		curve_luma_by_hue[i] = pcm_base_data->luma_by_hue_0[i] + curve[i];
	return vpp_pq_mgr_set_cm_curve(EN_CM_LUMA, curve_luma_by_hue);
}

int vpq_set_color_customize(struct vpq_cms_s *pdata)
{
	int ret = 0;
	int color_custom_curve[CURVE_LEN_CM] = {0};
	struct color_adj_param_s adj_param = {0};

	if (!pdata || !pcm_base_data)
		return RET_NULL_POINT;

	if (memcmp(pdata, &cms_param, sizeof(struct vpq_cms_s)) == 0) {
		vpq_pr_dbg(lev_tab, "color customize same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	ret = config_adj_param(pdata, &adj_param);

	switch (pdata->cms_type) {
	case VPQ_CMS_SAT:
		ret = apply_saturation_adjustment(&adj_param, color_custom_curve);
		break;
	case VPQ_CMS_HUE:
		ret = apply_hue_adjustment(&adj_param, color_custom_curve);
		break;
	case VPQ_CMS_LUMA:
		ret = apply_luma_adjustment(&adj_param, color_custom_curve);
		break;
	default:
		return RET_INVALID_INPUT;
	}

	if (ret == 0)
		memcpy(&cms_param, pdata, sizeof(struct vpq_cms_s));

	return ret;
}

int vpq_set_dnlp_mode(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct vpp_dnlp_curve_param_s *pdata;

	if (!pq_module_cfg.dnlp_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_DNLP);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_DNLP] &&
		value == pq_setting_val[PQ_DNLP]) {
		vpq_pr_dbg(lev_tab, "dnlp same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.dnlp_table[index][value].param;
	ret = vpp_pq_mgr_set_dnlp_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_DNLP] = value;
		pq_table_idx[PQ_INDEX_DNLP] = index;
	}

	return ret;
}

int vpq_set_csc_type(int value)
{
	return vpp_pq_mgr_set_csc_type(value);
}

int vpq_get_csc_type(void)
{
	return vpp_pq_mgr_get_csc_type();
}

int vpq_set_3dlut_data(struct vpq_lut3d_table_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_3dlut_data((struct vpp_lut3d_table_s *)pdata);
}

int vpq_get_hist_avg(struct vpq_hist_ave_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	vpp_pq_mgr_get_histogram_ave((struct vpp_hist_ave_s *)pdata);

	return 0;
}

int vpq_get_histogram(struct vpq_hist_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	vpp_pq_mgr_get_histogram((struct vpp_hist_bin_s *)pdata);

	return 0;
}

int vpq_get_hdr_histogram(struct vpq_hdr_hist_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	vpp_pq_mgr_get_hdr_histogram((struct vpp_hdr_hist_param_s *)pdata);

	return 0;
}

int vpq_set_lc_mode(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct vpp_lc_curve_s *plc_curve;
	struct vpp_lc_param_s *plc_param;

	if (!pq_module_cfg.lc_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_LC);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_LC] &&
		value == pq_setting_val[PQ_LC]) {
		vpq_pr_dbg(lev_tab, "lc same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	plc_curve = &pq_table_param.lc_table[index][value].curve;
	ret = vpp_pq_mgr_set_lc_curve(plc_curve);

	plc_param = &pq_table_param.lc_table[index][value].param;
	ret |= vpp_pq_mgr_set_lc_param(plc_param);
	if (ret == 0) {
		pq_setting_val[PQ_LC] = value;
		pq_table_idx[PQ_INDEX_LC] = index;
	}

	return ret;
}

int vpq_set_hdr_tmo_mode(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct vpp_tmo_param_s *pdata;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_HDR_TMO);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_HDR_TMO] &&
		value == pq_setting_val[PQ_HDR_TMO]) {
		vpq_pr_dbg(lev_tab, "hdr tmo same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.tmo_table[index][value].param;
	ret = vpp_pq_mgr_set_hdr_tmo_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_HDR_TMO] = value;
		pq_table_idx[PQ_INDEX_HDR_TMO] = index;
	}

	return ret;
}

int vpq_set_hdr_tmo(struct vpq_hdr_lut_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_hdr_tmo_curve((struct vpp_hdr_lut_s *)pdata);
}

int vpq_set_hdr_oetf(struct vpq_hdr_lut_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_hdr_oetf_curve((struct vpp_hdr_lut_s *)pdata);
}

int vpq_set_hdr_eotf(struct vpq_hdr_lut_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_hdr_eotf_curve((struct vpp_hdr_lut_s *)pdata);
}

int vpq_set_hdr_cgain(struct vpq_hdr_lut_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_hdr_cgain_curve((struct vpp_hdr_lut_s *)pdata);
}

int vpq_set_aipq_mode(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct vpp_aipq_table_s table = {0};

	index = vpq_module_timing_table_index(PQ_INDEX_AIPQ);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_AIPQ]) {
		vpq_pr_dbg(lev_tab, "aipq same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	table.height = pq_table_param.aipq_table[index].size.height;
	table.width = pq_table_param.aipq_table[index].size.width;
	table.table_ptr = (void *)pq_table_param.aipq_table[index].aipq_table;
	if (!table.table_ptr)
		return RET_NULL_POINT;

	ret = vpp_pq_mgr_set_aipq_data(&table);
	if (ret == 0) {
		pq_setting_val[PQ_AIPQ] = value;
		pq_table_idx[PQ_INDEX_AIPQ] = index;
	}

	return ret;
}

int vpq_set_aisr_mode(unsigned char value)
{
	// TODO
	return 0;
}

enum vpq_color_primary_e vpq_get_color_primary(void)
{
	enum vpp_color_primary_e color_pri = EN_COLOR_PRI_NULL;

	color_pri = vpp_pq_mgr_get_color_primary();

	return (enum vpq_color_primary_e)color_pri;
}

int vpq_get_hdr_metadata(struct vpq_hdr_metadata_s *pdata)
{
	vpp_pq_mgr_get_hdr_metadata((struct vpp_hdr_metadata_s *)pdata);

	return 0;
}

int vpq_set_black_stretch(unsigned char value)
{
	unsigned char index = 0;
	struct vpp_blkext_param_s *pdata;

	if (!pq_module_cfg.black_ext_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_BLK);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_BLK] &&
		value == pq_setting_val[PQ_BLK]) {
		vpq_pr_dbg(lev_tab, "black stretch same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.blkext_table[index][value].param;
	vpp_pq_mgr_set_blkext_params(pdata);

	pq_setting_val[PQ_BLK] = value;
	pq_table_idx[PQ_INDEX_BLK] = index;

	return 0;
}

int vpq_set_blue_stretch(unsigned char value)
{
	unsigned char index = 0;
	struct vpp_bs_param_s *pdata;

	if (!pq_module_cfg.blue_stretch_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_BLS);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_BLS] &&
		value == pq_setting_val[PQ_BLS]) {
		vpq_pr_dbg(lev_tab, "blue stretch same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.bls_table[index][value].param;
	vpp_pq_mgr_set_blue_stretch_params(pdata);

	pq_setting_val[PQ_BLS] = value;
	pq_table_idx[PQ_INDEX_BLS] = index;

	return 0;
}

int vpq_set_chroma_coring(unsigned char value)
{
	unsigned char index = 0;
	struct vpp_cc_param_s *pdata;

	if (!pq_module_cfg.chroma_cor_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_CCORING);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_CCORING] &&
		value == pq_setting_val[PQ_CCORING]) {
		vpq_pr_dbg(lev_tab, "chroma coring same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.ccoring_table[index][value].param;
	vpp_pq_mgr_set_chroma_coring_params(pdata);

	pq_setting_val[PQ_CCORING] = value;
	pq_table_idx[PQ_INDEX_CCORING] = index;

	return 0;
}

int vpq_set_eys_protect(struct vpq_eye_protect_s *pdata)
{
	return vpp_pq_mgr_set_eye_protect((struct vpp_eye_protect_s *)pdata);
}

int vpq_set_cabc(void)
{
	struct vpp_cabc_param_s param = {0};

	return vpp_pq_mgr_set_cabc_param(&param);
}

int vpq_set_add(void)
{
	struct vpp_aad_param_s param = {0};

	return vpp_pq_mgr_set_aad_param(&param);
}

int vpq_set_overscan_data(unsigned int length, struct vpq_overscan_data_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return vpp_pq_mgr_set_overscan_table(length, (struct vpp_overscan_table_s *)pdata);
}

int vpq_set_pc_mode(int value)
{
	return vpp_pq_mgr_set_pc_mode(value);
}

int vpq_get_pc_mode(void)
{
	return vpp_pq_mgr_get_pc_mode();
}

int vpq_set_color_primary_status(int value)
{
	return vpp_pq_mgr_set_color_primary_status(value);
}

int vpq_set_color_primary(struct vpq_color_primary_s *pdata)
{
	return vpp_pq_mgr_set_color_primary((struct vpp_color_primary_s *)pdata);
}

/*
 * DI&NR function
 */
int vpq_set_nr(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct di_dnr_param_s *pdata;

	if (!pq_module_cfg.nr_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_5)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_NR);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_NR] &&
		value == pq_setting_val[PQ_NR]) {
		vpq_pr_dbg(lev_tab, "nr same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.nr_table[index][value].param;
	ret = di_pq_mgr_dnr_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_NR] = value;
		pq_table_idx[PQ_INDEX_NR] = index;
	}

	return ret;
}

int vpq_set_deblock(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct di_dblk_param_s *pdata;

	if (!pq_module_cfg.deblock_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_DEBLOCK);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_DEBLOCK] &&
		value == pq_setting_val[PQ_DEBLOCK]) {
		vpq_pr_dbg(lev_tab, "deblock same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.dblk_table[index][value].param;
	ret = di_pq_mgr_dblk_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_DEBLOCK] = value;
		pq_table_idx[PQ_INDEX_DEBLOCK] = index;
	}

	return ret;
}

int vpq_set_demosquito(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct di_demosquito_param_s *pdata;

	if (!pq_module_cfg.demosquito_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_DEMOSQUITO);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_DEMOSQUITO] &&
		value == pq_setting_val[PQ_DEMOSQUITO]) {
		vpq_pr_dbg(lev_tab, "demosquito same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.demos_table[index][value].param;
	ret = di_pq_mgr_demosquito_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_DEMOSQUITO] = value;
		pq_table_idx[PQ_INDEX_DEMOSQUITO] = index;
	}

	return ret;
}

int vpq_set_smoothplus_mode(unsigned char value)
{
	int ret = 0;
	unsigned char index = 0;
	struct di_dct_param_s *pdata;

	if (!pq_module_cfg.smoothplus_en)
		return REF_CFG_DISABLED;

	if (value > PQ_TABLE_LV_4)
		return RET_INVALID_INPUT;

	index = vpq_module_timing_table_index(PQ_INDEX_SMOOTHPLUS);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_SMOOTHPLUS] &&
		value == pq_setting_val[PQ_SMOOTHPLUS]) {
		vpq_pr_dbg(lev_tab, "smoothplus same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.dct_table[index][value].param;
	ret = di_pq_mgr_dct_param(pdata);
	if (ret == 0) {
		pq_setting_val[PQ_SMOOTHPLUS] = value;
		pq_table_idx[PQ_INDEX_SMOOTHPLUS] = index;
	}

	return ret;
}

int vpq_set_di_param(void)
{
	int ret = 0;
	unsigned char index = 0;
	struct di_xlr_param_s *pdata;

	if (!pq_module_cfg.di_en)
		return REF_CFG_DISABLED;

	index = vpq_module_timing_table_index(PQ_INDEX_DI);
	if (index == PQ_TABLE_INVALID)
		return RET_TAB_ERROR;

	if (index == pq_table_idx[PQ_INDEX_DI]) {
		vpq_pr_dbg(lev_tab, "di same setting, skip update\n");
		return RET_SAME_SETTING;
	}

	pdata = &pq_table_param.di_table[index].param;
	ret = di_pq_mgr_xlr_param(pdata);
	if (ret == 0)
		pq_table_idx[PQ_INDEX_DI] = index;

	return ret;
}

/* DPSS Start */
int vpq_set_nr_dpss(struct vpq_dnr_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return di_pq_mgr_dnr_param((struct di_dnr_param_s *)pdata);
}

int vpq_set_deblock_dpss(struct vpq_dblk_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return di_pq_mgr_dblk_param((struct di_dblk_param_s *)pdata);
}

int vpq_set_demosquito_dpss(struct vpq_demosquito_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return di_pq_mgr_demosquito_param((struct di_demosquito_param_s *)pdata);
}

int vpq_set_smoothplus_dpss(struct vpq_dct_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return di_pq_mgr_dct_param((struct di_dct_param_s *)pdata);
}

int vpq_set_xlr_dpss(struct vpq_xlr_param_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

	return di_pq_mgr_xlr_param((struct di_xlr_param_s *)pdata);
}

/* DPSS End */

/*
 * DV function
 */
int vpq_set_amdv_pic_mode_id(unsigned char value)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_set_pic_mode_id(value);
#endif

	pq_setting_val[PQ_AMDV_PIC_MODE] = value;

	return 0;
}

int vpq_set_amdv_dark_detail(unsigned char value)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_set_dark_detail(value);
#endif

	pq_setting_val[PQ_AMDV_DARK_DETAIL] = value;

	return 0;
}

int vpq_set_amdv_light_sensor(struct vpq_light_sensor_s *pdata)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct light_sensor_s light_sensor;

	light_sensor.flag = pdata->flag;
	light_sensor.t_frontLux = pdata->t_frontLux;
	light_sensor.t_rearLum = pdata->t_rearLum;

	amdv_set_light_sense(light_sensor);
#endif

	pre_light_sensor = *pdata;

	return 0;
}

int vpq_set_amdv_precision_detail(unsigned char value)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_set_precision_detail_bypass(value);
#endif

	pq_setting_val[PQ_AMDV_PRE_DETAIL] = value;

	return 0;
}

int vpq_set_amdv_cfg_data(struct vpq_dv_config_data_s *pdata)
{
	if (!pdata)
		return RET_NULL_POINT;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_set_config_data((struct dv_config_data_s *)pdata);
#endif

	return 0;
}

struct vpq_dv_cfg_support_s vpq_get_dv_cfg_support(unsigned char value)
{
	struct vpq_dv_cfg_support_s vpq_cfg = {0};
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct dv_cfg_support_s dv_cfg = {0};

	dv_cfg.pic_mode_id = value;

	amdv_get_cfg_support(&dv_cfg);

	vpq_cfg.pic_mode_id = dv_cfg.pic_mode_id;
	vpq_cfg.precision_detail = dv_cfg.precision_detail;
	vpq_cfg.dark_detail = dv_cfg.dark_detail;
	vpq_cfg.light_sense = dv_cfg.light_sense;
#endif

	return vpq_cfg;
}

/*
 * FRC function
 */
int vpq_set_memc_on_off(unsigned char value)
{
	dpss_set_mc_link_state(value);

	return 0;
}

int vpq_set_memc_deblur_level(unsigned char value)
{
	dpss_frc_set_deblur(value);

	return 0;
}

int vpq_set_memc_dejudder_level(unsigned char value)
{
	dpss_frc_set_dejudder(value);

	return 0;
}

int vpq_set_memc_demo_mode(unsigned char value)
{
	dpss_frc_demo_win(value, 0);

	return 0;
}

/*
 * VDIN function
 */
int vpq_set_vdin_cvd(void)
{
	// TODO: link dev/vdin api
	return -1;
}

/*
 * Load PQ function
 */
void vpq_set_pq_effect(void)
{
	int ret = 0;
	unsigned int is_amdv;

	vpq_vfm_get_is_amdv(&is_amdv);
	if (is_amdv != 0) {
		vpq_pr_dbg(lev_tab, "load dv pq\n"
		"amdv_pic_mode:%d, dark_detail:%d, precision_detail:%d\n"
		"light_sensor:%d %d %d\n",
		pq_setting_val[PQ_AMDV_PIC_MODE], pq_setting_val[PQ_AMDV_DARK_DETAIL],
		pq_setting_val[PQ_AMDV_PRE_DETAIL], pre_light_sensor.flag,
		pre_light_sensor.t_frontLux, pre_light_sensor.t_rearLum);

		vpq_set_amdv_pic_mode_id(pq_setting_val[PQ_AMDV_PIC_MODE]);
		vpq_set_amdv_dark_detail(pq_setting_val[PQ_AMDV_DARK_DETAIL]);
		vpq_set_amdv_precision_detail(pq_setting_val[PQ_AMDV_PRE_DETAIL]);
		vpq_set_amdv_light_sensor(&pre_light_sensor);
	}

	vpq_pr_dbg(lev_tab, "load vpp pq\n"
		"sharpness:%d, dnlp:%d,   lc:%d, black_str:%d, blue_str:%d,\n"
		"chroma:%d,    hdrtmo:%d, cm:%d, aipq:%d,      aisr:%d\n",
		pq_setting_val[PQ_SHARPNESS], pq_setting_val[PQ_DNLP],
		pq_setting_val[PQ_LC],        pq_setting_val[PQ_BLK],
	    pq_setting_val[PQ_BLS],  pq_setting_val[PQ_CCORING],
		pq_setting_val[PQ_HDR_TMO],   pq_setting_val[PQ_CM],
		pq_setting_val[PQ_AIPQ],      pq_setting_val[PQ_AISR]);

	ret = vpq_set_sharpness(pq_setting_val[PQ_SHARPNESS]);
	ret |= vpq_set_dnlp_mode(pq_setting_val[PQ_DNLP]);
	ret |= vpq_set_lc_mode(pq_setting_val[PQ_LC]);
	ret |= vpq_set_black_stretch(pq_setting_val[PQ_BLK]);
	ret |= vpq_set_blue_stretch(pq_setting_val[PQ_BLS]);
	ret |= vpq_set_chroma_coring(pq_setting_val[PQ_CCORING]);
	ret |= vpq_set_hdr_tmo_mode(pq_setting_val[PQ_HDR_TMO]);
	ret |= vpq_set_color_base(pq_setting_val[PQ_CM]);
	ret |= vpq_set_aipq_mode(pq_setting_val[PQ_AIPQ]);
	ret |= vpq_set_aisr_mode(pq_setting_val[PQ_AISR]);

	vpq_pr_dbg(lev_tab, "load di pq\n"
		"nr:%d, deblock:%d, demos:%d, dect:%d\n",
		pq_setting_val[PQ_NR],         pq_setting_val[PQ_DEBLOCK],
		pq_setting_val[PQ_DEMOSQUITO], pq_setting_val[PQ_SMOOTHPLUS]);

	ret |= vpq_set_nr(pq_setting_val[PQ_NR]);
	ret |= vpq_set_deblock(pq_setting_val[PQ_DEBLOCK]);
	ret |= vpq_set_demosquito(pq_setting_val[PQ_DEMOSQUITO]);
	ret |= vpq_set_smoothplus_mode(pq_setting_val[PQ_SMOOTHPLUS]);
	ret |= vpq_set_di_param();

	vpq_pr_dbg(lev_tab, "ret:%d\n", ret);
}

int vpq_set_frame_status(enum vpq_frame_status_e status)
{
	if (status == VPQ_VFRAME_STOP) {
		//vpq_pr_info("reset local memory\n");
		//int i = 0;

		//for (i = 0; i < PQ_SETTING_MAX; i++)
		//	pq_setting_val[i] = 0xff;

		//for (i = 0; i < PQ_INDEX_MAX; i++)
		//	pq_table_idx[i] = 0xff;
	}

	return 1;
}

int vpq_get_event_info(void)
{
	enum vpq_vfm_event_info_e event_info = vpq_vfm_get_event_info();

	return (int)event_info;
}

void vpq_get_signal_info(struct vpq_signal_info_s *pdata)
{
	vpq_vfm_source_type_e src_type;
	vpq_vfm_port_e src_port;
	vpq_vfm_sig_fmt_e sig_fmt;
	vpq_vfm_trans_fmt_e trans_fmt;
	vpq_vfm_source_mode_e src_mode;
	enum vpq_vfm_hdr_type_e hdr_type;
	enum vpq_vfm_scan_mode_e scan_mode;
	unsigned int is_amdv;
	unsigned int is_game;
	unsigned int is_pc;
	unsigned int height;
	unsigned int width;
	unsigned int fps;

	src_type = vpq_vfm_get_source_type();
	switch (src_type) {
	default:
	case VFRAME_SOURCE_TYPE_OTHERS:
		pdata->src_type = VPQ_SRC_TYPE_MPEG;
		break;
	case VFRAME_SOURCE_TYPE_TUNER:
		pdata->src_type = VPQ_SRC_TYPE_ATV;
		break;
	case VFRAME_SOURCE_TYPE_CVBS:
		pdata->src_type = VPQ_SRC_TYPE_CVBS;
		break;
	case VFRAME_SOURCE_TYPE_HDMI:
		pdata->src_type = VPQ_SRC_TYPE_HDMI;
		break;
	}

	src_port = vpq_vfm_get_source_port();
	switch (src_port) {
	case TVIN_PORT_HDMI0:
		pdata->hdmi_port = VPQ_HDMI_PORT_0;
		break;
	case TVIN_PORT_HDMI1:
		pdata->hdmi_port = VPQ_HDMI_PORT_1;
		break;
	case TVIN_PORT_HDMI2:
		pdata->hdmi_port = VPQ_HDMI_PORT_2;
		break;
	case TVIN_PORT_HDMI3:
		pdata->hdmi_port = VPQ_HDMI_PORT_3;
		break;
	default:
		pdata->hdmi_port = VPQ_HDMI_PORT_NULL;
		break;
	}

	src_mode = vpq_vfm_get_source_mode();
	pdata->sig_mode = (enum vpq_sig_mode_e)src_mode;

	hdr_type = vpq_vfm_get_hdr_type();
	pdata->hdr_type = (enum vpq_hdr_type_e)hdr_type;

	vpq_vfm_get_is_amdv(&is_amdv);
	pdata->is_amdv = is_amdv;

	vpq_vfm_get_is_game(&is_game);
	pdata->is_game = is_game;

	vpq_vfm_get_is_pc(&is_pc);
	pdata->is_pc = is_pc;

	scan_mode = vpq_vfm_get_signal_scan_mode();
	pdata->scan_mode = (enum vpq_scan_mode_e)scan_mode;

	sig_fmt = vpq_vfm_get_signal_format();
	pdata->sig_fmt = (unsigned int)sig_fmt;

	trans_fmt = vpq_vfm_get_trans_format();
	pdata->trans_fmt = (unsigned int)trans_fmt;

	vpq_vfm_get_height_width(&height, &width);
	pdata->height = height;
	pdata->width = width;

	vpq_frm_get_fps(&fps);
	pdata->fps = fps;
}
