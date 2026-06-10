// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include "lcd_common.h"
#include "./connectors/lcd_connector.h"

#define UFR_TMG_PARSE        "cus_ctrl ufr_tmg"
#define DFR_TMG_PARSE        "cus_ctrl dfr_tmg"
#define EXT_TMG_PARSE        "cus_ctrl ext_tmg"
#define TUNING_ATTR_PARSE    "cus_ctrl tuning_attr"
#define CLK_ADV_PARSE        "cus_ctrl clk_adv"

static int lcd_cus_ctrl_parse_clk_adv_dts(struct aml_lcd_drv_s *pdrv,
		struct aml_lcd_device_s *dev_p, struct lcd_cus_ctrl_attr_config_s *attr_conf,
		struct device_node *child)
{
	unsigned int para[2];

	if (of_property_read_u32_array(child, "clk_advanced", &para[0], 2)) {
		LCD_DEV_ERR(pdrv, dev_p->idx, "failed to get clk_advanced");
		return -1;
	}
	dev_p->dev_cfg.timing.ss_freq = para[0];
	dev_p->dev_cfg.timing.ss_mode = para[1];

	LCD_DEV_DBG(pdrv, dev_p->idx, "%s: freq=%hu, mode=%hu", CLK_ADV_PARSE, para[0], para[1]);

	return 0;
}

#define LCD_EXT_TIMING_MAX 8
static int lcd_cus_ctrl_parse_extend_tmg_dts(struct aml_lcd_drv_s *pdrv,
		struct aml_lcd_device_s *dev_p, struct lcd_cus_ctrl_attr_config_s *attr_conf,
		struct device_node *child)
{
	struct lcd_detail_timing_s *ptiming;
	struct lcd_timing_s *tims = &dev_p->dev_cfg.timing;
	char snode[32];
	int ret;
	unsigned char etmg_idx, c_bit;
	unsigned int para[14];

	if (!tims->timings[0])
		return -1;

	for (etmg_idx = 0; etmg_idx < LCD_EXT_TIMING_MAX; etmg_idx++) {
		memset(snode, 0, 32 * sizeof(char));
		sprintf(snode, "extend_tmg_%hu", etmg_idx);

		ret = of_property_read_u32_array(child, snode, &para[0], 14);
		if (ret) {
			LCD_DEV_ERR(pdrv, dev_p->idx, "failed to get %s", snode);
			break;
		}

		ptiming = lcd_timing_alloc(pdrv, dev_p);
		if (!ptiming)
			break;
		memcpy(ptiming, tims->timings[0], sizeof(*ptiming));
		ptiming->ss_force = 0;

		ptiming->h_period    = para[0];
		ptiming->h_active    = para[1];
		ptiming->hsync_width = para[2];
		ptiming->hsync_bp    = para[3];
		ptiming->hsync_pol   = para[4];
		ptiming->hsync_fp    = ptiming->h_period - ptiming->h_active -
					ptiming->hsync_width - ptiming->hsync_bp;
		ptiming->v_period    = para[5];
		ptiming->v_active    = para[6];
		ptiming->vsync_width = para[7];
		ptiming->vsync_bp    = para[8];
		ptiming->vsync_pol   = para[9];
		ptiming->vsync_fp    = ptiming->v_period - ptiming->v_active -
					ptiming->vsync_width - ptiming->vsync_bp;

		c_bit = para[10];
		ptiming->pixel_clk = para[13];

		memset(snode, 0, 32 * sizeof(char));
		sprintf(snode, "extend_tmg_%hu_frame_rate", etmg_idx);
		ret = of_property_read_u32_array(child, snode, &para[0], 2);
		ptiming->frame_rate_min = ret ? 0 : para[0];
		ptiming->frame_rate_max = ret ? 0 : para[1];

		memset(snode, 0, 32 * sizeof(char));
		sprintf(snode, "extend_tmg_%hu_range_setting", etmg_idx);
		ret = of_property_read_u32_array(child, snode, &para[0], 6);
		ptiming->h_period_min = ret ? ptiming->h_period  : para[0];
		ptiming->h_period_max = ret ? ptiming->h_period  : para[1];
		ptiming->v_period_min = ret ? ptiming->v_period  : para[2];
		ptiming->v_period_max = ret ? ptiming->v_period  : para[3];
		ptiming->pclk_min     = ret ? ptiming->pixel_clk : para[4];
		ptiming->pclk_max     = ret ? ptiming->pixel_clk : para[5];
		ptiming->fr_adjust_type = ret ? 0xff : 3;
		ptiming->switch_type = attr_conf->attr_flag;

		lcd_clk_frame_rate_init(ptiming);
		LCD_DEV_DBG(pdrv, dev_p->idx, "%s: timing[%d]: %dx%dp%dhz", EXT_TMG_PARSE,
			etmg_idx, ptiming->h_active, ptiming->v_active, ptiming->frame_rate);

		lcd_config_timing_check(pdrv, dev_p, ptiming);
	}

	return 0;
}

int lcd_cus_ctrl_load_from_dts(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
				struct device_node *child)
{
	char cus_node[25];
	unsigned int cut_attr_para[4];

	unsigned char cus_idx, cus_type;
	struct lcd_cus_ctrl_attr_config_s cus_cfg;
	int ret = 0;

	for (cus_idx = 0; cus_idx < LCD_CUS_CTRL_ATTR_CNT_MAX; cus_idx++) {
		sprintf(cus_node, "cus_ctrl_%hu_attr", cus_idx);
		ret = of_property_read_variable_u32_array(child, cus_node, &cut_attr_para[0], 1, 4);
		if (ret <= 0)
			break;

		cus_cfg.attr_index = cus_idx;
		cus_cfg.param_flag = 0;
		cus_cfg.param_size = 1;
		cus_cfg.priv_sel = 0;

		cus_type = cut_attr_para[0];
		cus_cfg.attr_type = cus_type;
		switch (cus_type) {
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			cus_cfg.attr_flag = cut_attr_para[1];
			ret = lcd_cus_ctrl_parse_extend_tmg_dts(pdrv, dev_p, &cus_cfg, child);
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			ret = lcd_cus_ctrl_parse_clk_adv_dts(pdrv, dev_p, &cus_cfg, child);
			break;
		default:
			LCD_DEV_ERR(pdrv, dev_p->idx, "%s: invalid cus_type: %d",
				__func__, cus_type);
			break;
		}
	}

	return ret;
}

static int lcd_cus_ctrl_parse_ufr_ini(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
		void *inip, void *psec, struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	struct lcd_detail_timing_s *ptiming;
	struct lcd_timing_s *tims = &dev_p->dev_cfg.timing;
	unsigned int temp;

	if (!tims->timings[0])
		return -1;

	ptiming = lcd_timing_alloc(pdrv, dev_p);
	if (!ptiming)
		return -1;

	memcpy((void *)ptiming, (void *)tims->timings[0], sizeof(*ptiming));
	ptiming->ss_force = 0;
	ptiming->ufr_mode = 1;

	temp = lcd_ini_get_val(inip, psec, "ufr_vtotal_min", 0);
	if (temp == 0)
		temp = lcd_ini_get_val(inip, psec, "ctrl_attr_0_parm0", 0);
	if (temp)
		ptiming->v_period_min = temp;

	temp = lcd_ini_get_val(inip, psec, "ufr_vtotal_max", 0);
	if (temp == 0)
		temp = lcd_ini_get_val(inip, psec, "ctrl_attr_0_parm1", 0);
	if (temp)
		ptiming->v_period_max = temp;

	ptiming->frame_rate_min = lcd_ini_get_val(inip, psec, "ufr_frame_rate_min", 0);
	ptiming->frame_rate_max = lcd_ini_get_val(inip, psec, "ufr_frame_rate_max", 0);

	temp = lcd_ini_get_val(inip, psec, "ufr_vpw", 0);
	if (temp)
		ptiming->vsync_width = temp;
	temp = lcd_ini_get_val(inip, psec, "ufr_vbp", 0);
	if (temp)
		ptiming->vsync_bp = temp;

	ptiming->v_active /= 2;
	ptiming->v_period /= 2;
	ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
	ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
	ptiming->switch_type = attr_conf->attr_flag;
	lcd_clk_frame_rate_init(ptiming);
	LCD_DEV_DBG(pdrv, dev_p->idx, "%s: %dx%dp%dhz", UFR_TMG_PARSE,
			ptiming->h_active, ptiming->v_active, ptiming->frame_rate);

	lcd_config_timing_check(pdrv, dev_p, ptiming);

	return 0;
}

static int lcd_cus_ctrl_parse_dfr_ini(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
		void *inip, void *psec, struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	struct lcd_detail_timing_s *ptiming, tmg_tmp;
	struct lcd_timing_s *tims = &dev_p->dev_cfg.timing;
	char str[30];
	unsigned int temp, tmg_index = 0;
	int i;

	if (!tims->timings[0])
		return -1;

	for (i = 0; i < 15; i++) {
		memcpy((void *)&tmg_tmp, (void *)tims->timings[0], sizeof(tmg_tmp));

		sprintf(str, "dfr_fr_%d_tmg_index", i);
		tmg_index = lcd_ini_get_val(inip, psec, str, 0xffff);
		if (tmg_index == 0xffff)
			break;
		if (tmg_index > 15) {
			LCD_DEV_ERR(pdrv, dev_p->idx, "%s: %s error: %d",
				DFR_TMG_PARSE, str, tmg_index);
			return -1;
		}

		sprintf(str, "dfr_fr_%d_frame_rate", i);
		tmg_tmp.frame_rate = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_tmp.frame_rate == 0)
			break;

		sprintf(str, "dfr_fr_%d_frame_rate_min", i);
		tmg_tmp.frame_rate_min = lcd_ini_get_val(inip, psec, str, 0);

		sprintf(str, "dfr_fr_%d_frame_rate_max", i);
		tmg_tmp.frame_rate_max = lcd_ini_get_val(inip, psec, str, 0);

		if (tmg_index) {
			sprintf(str, "dfr_tmg_%d_vtotal", tmg_index);
			tmg_tmp.v_period = lcd_ini_get_val(inip, psec, str, 0);
			if (tmg_tmp.v_period == 0)
				break;

			sprintf(str, "dfr_tmg_%d_vtotal_min", i);
			tmg_tmp.v_period_min = lcd_ini_get_val(inip, psec, str, 0);
			if (tmg_tmp.v_period_min == 0)
				break;

			sprintf(str, "dfr_tmg_%d_vtotal_max", i);
			tmg_tmp.v_period_max = lcd_ini_get_val(inip, psec, str, 0);
			if (tmg_tmp.v_period_max == 0)
				break;

			sprintf(str, "dfr_tmg_%d_vpw", i);
			temp = lcd_ini_get_val(inip, psec, str, 0);
			if (temp)
				tmg_tmp.vsync_width = temp;

			sprintf(str, "dfr_tmg_%d_vbp", i);
			temp = lcd_ini_get_val(inip, psec, str, 0);
			if (temp == 0)
				tmg_tmp.vsync_bp = temp;
		}

		ptiming = lcd_timing_alloc(pdrv, dev_p);
		if (!ptiming)
			return -1;

		memcpy((void *)ptiming, (void *)&tmg_tmp, sizeof(*ptiming));
		ptiming->ss_force = 0;

		ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
		ptiming->pixel_clk = ptiming->frame_rate *
			ptiming->h_period * ptiming->v_period;
		ptiming->switch_type = attr_conf->attr_flag;
		lcd_clk_frame_rate_init(ptiming);
		LCD_DEV_DBG(pdrv, dev_p->idx, "%s: dfr[%d]: %dx%dp%dhz", DFR_TMG_PARSE,
			i, ptiming->h_active, ptiming->v_active, ptiming->frame_rate);

		lcd_config_timing_check(pdrv, dev_p, ptiming);
	}

	return 0;
}

static int lcd_cus_ctrl_parse_extend_tmg_ini(struct aml_lcd_drv_s *pdrv,
		struct aml_lcd_device_s *dev_p, void *inip, void *psec,
		struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	struct lcd_detail_timing_s *ptiming, tmg_temp;
	struct lcd_timing_s *tims = &dev_p->dev_cfg.timing;
	unsigned int val;
	char str[30];
	int i;

	if (!tims->timings[0]) {
		LCD_DEV_ERR(pdrv, dev_p->idx, "%s: no dft_timing, exit", EXT_TMG_PARSE);
		return -1;
	}

	for (i = 0;  i < 15; i++) {
		memcpy((void *)&tmg_temp, (void *)tims->timings[0], sizeof(tmg_temp));

		sprintf(str, "extend_tmg_%d_clk_mode", i);
		tmg_temp.clk_mode = lcd_ini_get_val(inip, psec, str, tmg_temp.clk_mode);

		sprintf(str, "extend_tmg_%d_asf_mode", i);
		tmg_temp.asf_mode = lcd_ini_get_val(inip, psec, str, tmg_temp.asf_mode);

		sprintf(str, "extend_tmg_%d_ufr_mode", i);
		tmg_temp.ufr_mode = lcd_ini_get_val(inip, psec, str, tmg_temp.ufr_mode);

		sprintf(str, "extend_tmg_%d_hactive", i);
		tmg_temp.h_active = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.h_active == 0)
			break;
		sprintf(str, "extend_tmg_%d_vactive", i);
		tmg_temp.v_active = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.v_active == 0)
			break;
		sprintf(str, "extend_tmg_%d_htotal", i);
		tmg_temp.h_period = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.h_period == 0)
			break;
		sprintf(str, "extend_tmg_%d_vtotal", i);
		tmg_temp.v_period = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.v_period == 0)
			break;
		sprintf(str, "extend_tmg_%d_hpw", i);
		tmg_temp.hsync_width = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.hsync_width == 0)
			break;
		sprintf(str, "extend_tmg_%d_hbp", i);
		tmg_temp.hsync_bp = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.hsync_bp == 0)
			break;
		sprintf(str, "extend_tmg_%d_hs_pol", i);
		val = lcd_ini_get_val(inip, psec, str, 0xffff);
		if (val == 0xffff)
			break;
		tmg_temp.hsync_pol = val;
		sprintf(str, "extend_tmg_%d_vpw", i);
		tmg_temp.vsync_width = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.vsync_width == 0)
			break;
		sprintf(str, "extend_tmg_%d_vbp", i);
		tmg_temp.vsync_bp = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.vsync_bp == 0)
			break;
		sprintf(str, "extend_tmg_%d_vs_pol", i);
		val = lcd_ini_get_val(inip, psec, str, 0xffff);
		if (val == 0xffff)
			break;
		tmg_temp.vsync_pol = val;
		sprintf(str, "extend_tmg_%d_fr_adj_type", i);
		val = lcd_ini_get_val(inip, psec, str, 0xffff);
		if (val == 0xffff)
			break;
		tmg_temp.fr_adjust_type = val;
		sprintf(str, "extend_tmg_%d_pixel_clk", i);
		val = lcd_ini_get_val(inip, psec, str, 0xffffffff);
		if (val == 0xffffffff)
			break;
		tmg_temp.pixel_clk = val;

		sprintf(str, "extend_tmg_%d_htotal_min", i);
		tmg_temp.h_period_min = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.h_period_min == 0)
			break;
		sprintf(str, "extend_tmg_%d_htotal_max", i);
		tmg_temp.h_period_max = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.h_period_max == 0)
			break;
		sprintf(str, "extend_tmg_%d_vtotal_min", i);
		tmg_temp.v_period_min = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.v_period_min == 0)
			break;
		sprintf(str, "extend_tmg_%d_vtotal_max", i);
		tmg_temp.v_period_max = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.v_period_max == 0)
			break;
		sprintf(str, "extend_tmg_%d_frame_rate_min", i);
		tmg_temp.frame_rate_min = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.frame_rate_min == 0)
			break;
		sprintf(str, "extend_tmg_%d_frame_rate_max", i);
		tmg_temp.frame_rate_max = lcd_ini_get_val(inip, psec, str, 0);
		if (tmg_temp.frame_rate_max == 0)
			break;
		sprintf(str, "extend_tmg_%d_pclk_min", i);
		tmg_temp.pclk_min = lcd_ini_get_val(inip, psec, str, 0);
		sprintf(str, "extend_tmg_%d_pclk_max", i);
		tmg_temp.pclk_max = lcd_ini_get_val(inip, psec, str, 0);

		ptiming = lcd_timing_alloc(pdrv, dev_p);
		if (!ptiming) {
			LCD_DEV_ERR(pdrv, dev_p->idx,
				"%s: ptiming alloc failed, exit", EXT_TMG_PARSE);
			return -1;
		}
		memcpy((void *)ptiming, (void *)&tmg_temp, sizeof(*ptiming));
		ptiming->ss_force = 0;

		ptiming->hsync_fp = ptiming->h_period - ptiming->h_active -
			ptiming->hsync_width - ptiming->hsync_bp;
		ptiming->vsync_fp = ptiming->v_period - ptiming->v_active -
			ptiming->vsync_width - ptiming->vsync_bp;
		ptiming->switch_type = attr_conf->attr_flag;

		lcd_clk_frame_rate_init(ptiming);
		LCD_DEV_DBG(pdrv, dev_p->idx, "%s: extend_tmg[%d]: %dx%dp%dhz", EXT_TMG_PARSE,
			i, ptiming->h_active, ptiming->v_active, ptiming->frame_rate);

		lcd_config_timing_check(pdrv, dev_p, ptiming);
	}

	return 0;
}

static int lcd_cus_ctrl_parse_clk_adv_ini(struct aml_lcd_drv_s *pdrv,
					struct aml_lcd_device_s *dev_p, void *inip, void *psec,
					struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	unsigned int temp;

	if (attr_conf->attr_flag & (1 << 0)) {
		temp = lcd_ini_get_val(inip, psec, "ss_freq", 0xffff);
		if (temp != 0xffff)
			dev_p->dev_cfg.timing.ss_freq = temp;
		temp = lcd_ini_get_val(inip, psec, "ss_mode", 0xffff);
		if (temp != 0xffff)
			dev_p->dev_cfg.timing.ss_mode = temp;
	}

	return 0;
}

static int lcd_cus_ctrl_parse_tuning_attr_ini(struct aml_lcd_drv_s *pdrv,
					struct aml_lcd_device_s *dev_p, void *inip, void *psec,
					struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	struct phy_config_s *phy_cfg;
	struct phy_attr_s *phy, tmp_phy;
	void *tuning_sec;
	char sec_str[16], key_str[16];
	unsigned short lane_cnt;
	unsigned int temp;
	int i, j;

	if (!dev_p->dev_cfg.phy_cfg.phys[0])
		return -1;

	tuning_sec = lcd_ini_get_section(inip, "lane_sel_Attr");
	if (!tuning_sec) {
		LCD_DEV_ERR(pdrv, dev_p->idx, "%s: not find lane_sel_Attr", TUNING_ATTR_PARSE);
		return -1;
	}

	lane_cnt = lcd_ini_get_val(inip, tuning_sec, "lane_count", 0);
	if (lane_cnt == 0)
		return -1;
	if (lane_cnt > CH_LANE_MAX) {
		LCD_DEV_ERR(pdrv, dev_p->idx, "%s: lane_cnt %d error", TUNING_ATTR_PARSE, lane_cnt);
		return -1;
	}

	phy_cfg = &dev_p->dev_cfg.phy_cfg;
	phy_cfg->lane_num = lane_cnt;
	for (i = 0; i < lane_cnt; i++) {
		sprintf(key_str, "ch%u_sel", i);
		temp = lcd_ini_get_val(inip, tuning_sec, key_str, 0xffff);
		if (temp == 0xffff)
			goto lcd_cus_parse_phy_err;
		phy_cfg->ch_ctrl[i].sel = temp;
		phy_cfg->ch_ctrl[i].sel_dft = temp;

		sprintf(key_str, "ch%u_phase", i);
		phy_cfg->ch_ctrl[i].phase_sel = lcd_ini_get_val(inip, tuning_sec, key_str, 0xff);

		sprintf(key_str, "ch%u_pn_swap", i);
		phy_cfg->ch_ctrl[i].pn_swap = lcd_ini_get_val(inip,
			tuning_sec, key_str, phy_cfg->ch_ctrl[i].pn_swap);

		LCD_DEV_DBG(pdrv, dev_p->idx, "%s: lane[%d]: sel=0x%x, pn_swap=%d, phase_sel=0x%x",
			TUNING_ATTR_PARSE, i, phy_cfg->ch_ctrl[i].sel,
			phy_cfg->ch_ctrl[i].pn_swap, phy_cfg->ch_ctrl[i].phase_sel);
	}

	for (i = 0; i < MAX_NUM_PHY_CFGS; i++) {
		memcpy((void *)&tmp_phy, (void *)phy_cfg->phys[0], sizeof(tmp_phy));

		if (i == 0)
			sprintf(sec_str, "tuning_Attr");
		else
			sprintf(sec_str, "tuning_Attr%d", i);
		tuning_sec = lcd_ini_get_section(inip, sec_str);
		if (!tuning_sec) {
			if (i == 0) {
				LCD_DEV_ERR(pdrv, dev_p->idx, "%s: not find %s",
						TUNING_ATTR_PARSE, sec_str);
			}
			break;
		}

		// phy_clk_min and phy_clk_max
		tmp_phy.phy_clk_min = lcd_ini_get_val(inip, tuning_sec, "phy_clk_min", 0);
		tmp_phy.phy_clk_max = lcd_ini_get_val(inip, tuning_sec, "phy_clk_max", 0);
		// phy_clk_min and phy_clk_max

		// phy_clk_min and phy_clk_max error, should read phy_clk:
		// 1.phy_clk_min or phy_clk_max equal to 0
		// 2.phy_clk_min > phy_clk_max
		if ((!tmp_phy.phy_clk_min && !tmp_phy.phy_clk_max) ||
			tmp_phy.phy_clk_min > tmp_phy.phy_clk_max) {
			tmp_phy.phy_clk = lcd_ini_get_val(inip, tuning_sec, "phy_clk", 0);
			if (tmp_phy.phy_clk) {
				tmp_phy.phy_clk_min = tmp_phy.phy_clk - 20;
				tmp_phy.phy_clk_max = tmp_phy.phy_clk + 20;
			} else {
				tmp_phy.phy_clk_min = 0;
				tmp_phy.phy_clk_max = 0;
			}
		}

		tmp_phy.ss.level = lcd_ini_get_val(inip, tuning_sec, "ss_level", tmp_phy.ss.level);
		tmp_phy.ss.freq =
			lcd_ini_get_val(inip, tuning_sec, "ss_frequency", tmp_phy.ss.freq);
		tmp_phy.ss.mode = lcd_ini_get_val(inip, tuning_sec, "ss_mode", tmp_phy.ss.mode);

		if (dev_p->dev_cfg.basic.lcd_type == LCD_MLVDS) {
			tmp_phy.clk_phase = lcd_ini_get_val(inip, tuning_sec,
						"mlvds_clk_phase", tmp_phy.clk_phase);
		}

		tmp_phy.vswing = lcd_ini_get_val(inip, tuning_sec, "vswing", tmp_phy.vswing);
		tmp_phy.vcm = lcd_ini_get_val(inip, tuning_sec, "vcm", tmp_phy.vcm);
		tmp_phy.ref_bias = lcd_ini_get_val(inip, tuning_sec, "ref_bias", tmp_phy.ref_bias);
		tmp_phy.odt = lcd_ini_get_val(inip, tuning_sec, "odt", tmp_phy.odt);
		tmp_phy.cv_mode = lcd_ini_get_val(inip, tuning_sec, "cv_mode", tmp_phy.cv_mode);
		LCD_DEV_DBG(pdrv, dev_p->idx,
			"%s: vswing=0x%x, vcm=0x%x, bias=0x%x, odt=0x%x, cv_mode=%d",
			TUNING_ATTR_PARSE, tmp_phy.vswing, tmp_phy.vcm,
			tmp_phy.ref_bias, tmp_phy.odt, tmp_phy.cv_mode);

		for (j = 0; j < lane_cnt; j++) {
			sprintf(key_str, "ch%u_rterm", j);
			tmp_phy.lane[j].rterm =
				lcd_ini_get_val(inip, tuning_sec, key_str, tmp_phy.lane[j].rterm);
			sprintf(key_str, "ch%u_amp", j);
			tmp_phy.lane[j].amp =
				lcd_ini_get_val(inip, tuning_sec, key_str, tmp_phy.lane[j].amp);
			sprintf(key_str, "ch%u_preem", j);
			tmp_phy.lane[j].preem =
				lcd_ini_get_val(inip, tuning_sec, key_str, tmp_phy.lane[j].preem);
			LCD_DEV_DBG(pdrv, dev_p->idx,
				"%s: lane[%d]: preem=0x%x, amp=0x%x, rterm=0x%x",
				TUNING_ATTR_PARSE, j, tmp_phy.lane[j].preem, tmp_phy.lane[j].amp,
				tmp_phy.lane[j].rterm);
		}

		phy = lcd_phy_alloc(pdrv, dev_p);
		if (!phy)
			return -1;
		memcpy((void *)phy, (void *)&tmp_phy, sizeof(*phy));

		LCD_DEV_DBG(pdrv, dev_p->idx, "%s: tuning_param[%d]: phy_clk:%d, lane_cnt:%d",
			TUNING_ATTR_PARSE, i, tmp_phy.phy_clk, lane_cnt);
		if (tmp_phy.phy_clk_min || tmp_phy.phy_clk_max)
			LCD_DEV_DBG(pdrv, dev_p->idx,
				"%s: tuning_param[%d]: phy_clk_match_range:%d~%d\n",
				TUNING_ATTR_PARSE, i, tmp_phy.phy_clk_min, tmp_phy.phy_clk_max);

	}

	if (phy_cfg->phys[0] && phy_cfg->phys[1] &&
	    phy_cfg->phys[0]->phy_clk == phy_cfg->phys[1]->phy_clk) {
		kfree(phy_cfg->phys[0]);
		for (i = 0; i < phy_cfg->group_num - 1; i++)
			phy_cfg->phys[i] = phy_cfg->phys[i + 1];
		phy_cfg->phys[phy_cfg->group_num - 1] = NULL;
		phy_cfg->group_num--;
		phy_cfg->act_phy = phy_cfg->phys[0];
	}

	return 0;

lcd_cus_parse_phy_err:
	LCD_DEV_ERR(pdrv, dev_p->idx, "%s: %s error", TUNING_ATTR_PARSE, key_str);
	return -1;
}

#ifdef CONFIG_AMLOGIC_LCD_TCON
static int lcd_cus_ctrl_parse_tcon_sw_pol_ini(struct aml_lcd_drv_s *pdrv,
		void *inip, void *psec, struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	return 0;
}

static int lcd_cus_ctrl_parse_tcon_sw_pdf_ini(struct aml_lcd_drv_s *pdrv,
		void *inip, void *psec, struct lcd_cus_ctrl_attr_config_s *attr_conf)
{
	return 0;
}
#endif

int lcd_cus_ctrl_load_from_ini(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
			void *inip, void *psec, unsigned char version)
{
	struct lcd_cus_ctrl_attr_config_s attr_conf;
	unsigned int ctrl_en, ctrl_attr;
	unsigned int i;
	char pr_buf[128], str[32];
	int ret;

	if (version < 2)
		return 0;

	ctrl_en = lcd_ini_get_val(inip, psec, "ctrl_attr_en", 0);
	if (ctrl_en == 0)
		ctrl_en = lcd_ini_get_val(inip, psec, "ctrl_attr_flag", 0);
	if (ctrl_en == 0)
		return 0;

	LCD_DEV_DBG(pdrv, dev_p->idx, "%s: ctrl_en=0x%x", __func__, ctrl_en);

	for (i = 0; i < LCD_CUS_CTRL_ATTR_CNT_MAX; i++) {
		if ((ctrl_en & (1 << i)) == 0)
			continue;

		memset(&attr_conf, 0, sizeof(attr_conf));

		sprintf(str, "ctrl_attr_%d", i);
		ctrl_attr = lcd_ini_get_val(inip, psec, str, 0xffff);
		attr_conf.attr_index = i;
		attr_conf.attr_flag = ctrl_attr & 0xf;
		attr_conf.param_flag = (ctrl_attr >> 4) & 0xf;
		attr_conf.attr_type = (ctrl_attr >> 8) & 0xff;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			sprintf(pr_buf, "attr_type=%x, attr_flag=%d, param_flag=%d",
				attr_conf.attr_type,
				attr_conf.attr_flag,
				attr_conf.param_flag);
			LCD_DEV_DBG(pdrv, dev_p->idx, "%s: attr[%d]: %s", __func__, i, pr_buf);
		}

		switch (attr_conf.attr_type) {
		case LCD_CUS_CTRL_TYPE_UFR:
			ret = lcd_cus_ctrl_parse_ufr_ini(pdrv, dev_p, inip, psec, &attr_conf);
			break;
		case LCD_CUS_CTRL_TYPE_DFR:
			ret = lcd_cus_ctrl_parse_dfr_ini(pdrv, dev_p, inip, psec, &attr_conf);
			break;
		case LCD_CUS_CTRL_TYPE_EXTEND_TMG:
			ret = lcd_cus_ctrl_parse_extend_tmg_ini(pdrv, dev_p, inip, psec,
									&attr_conf);
			break;
		case LCD_CUS_CTRL_TYPE_CLK_ADV:
			ret = lcd_cus_ctrl_parse_clk_adv_ini(pdrv, dev_p, inip, psec,
									&attr_conf);
			break;
		case LCD_CUS_CTRL_TYPE_TUNING_ATTR:
			if (version < 3) {
				ret = -1;
				LCD_DEV_ERR(pdrv, dev_p->idx,
					"%s, invalid tuning_attr with ukey_ver %d",
					__func__, version);
				break;
			}
			ret = lcd_cus_ctrl_parse_tuning_attr_ini(pdrv, dev_p, inip, psec,
									&attr_conf);
			break;
#ifdef CONFIG_AMLOGIC_LCD_TCON
		case LCD_CUS_CTRL_TYPE_TCON_SW_POL:
			ret = lcd_cus_ctrl_parse_tcon_sw_pol_ini(pdrv, inip, psec, &attr_conf);
			break;
		case LCD_CUS_CTRL_TYPE_TCON_SW_PDF:
			ret = lcd_cus_ctrl_parse_tcon_sw_pdf_ini(pdrv, inip, psec, &attr_conf);
			break;
#endif
		default:
			LCD_DEV_ERR(pdrv, dev_p->idx, "%s: invalid attr_type: 0x%x",
				__func__, attr_conf.attr_type);
			goto lcd_cus_ctrl_load_from_ini_err;
		}
		if (ret)
			goto lcd_cus_ctrl_load_from_ini_err;
	}

	return 0;

lcd_cus_ctrl_load_from_ini_err:
	return -1;
}
