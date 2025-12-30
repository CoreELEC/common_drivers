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
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_reg.h"
#include "lcd_phy_config.h"

static struct lcd_phy_ctrl_s *phy_ctrl_p;

const unsigned int t7_lane_cntl_reg[] = {
	ANACTRL_DIF_PHY_CNTL1,  ANACTRL_DIF_PHY_CNTL3,
	ANACTRL_DIF_PHY_CNTL4,  ANACTRL_DIF_PHY_CNTL5,
	ANACTRL_DIF_PHY_CNTL6,  ANACTRL_DIF_PHY_CNTL7,
	ANACTRL_DIF_PHY_CNTL8,  ANACTRL_DIF_PHY_CNTL9,
	ANACTRL_DIF_PHY_CNTL10, ANACTRL_DIF_PHY_CNTL12,
	ANACTRL_DIF_PHY_CNTL13, ANACTRL_DIF_PHY_CNTL14,
	ANACTRL_DIF_PHY_CNTL15, ANACTRL_DIF_PHY_CNTL16,
	ANACTRL_DIF_PHY_CNTL17, ANACTRL_DIF_PHY_CNTL18,
};

static int lcd_phy_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s reg_table[] = {
		{ANACTRL_DIF_PHY_CNTL1,  "PHY_CNTL1"},
		{ANACTRL_DIF_PHY_CNTL2,  "PHY_CNTL2"},
		{ANACTRL_DIF_PHY_CNTL3,  "PHY_CNTL3"},
		{ANACTRL_DIF_PHY_CNTL4,  "PHY_CNTL4"},
		{ANACTRL_DIF_PHY_CNTL5,  "PHY_CNTL5"},
		{ANACTRL_DIF_PHY_CNTL6,  "PHY_CNTL6"},
		{ANACTRL_DIF_PHY_CNTL7,  "PHY_CNTL7"},
		{ANACTRL_DIF_PHY_CNTL8,  "PHY_CNTL8"},
		{ANACTRL_DIF_PHY_CNTL9,  "PHY_CNTL9"},
		{ANACTRL_DIF_PHY_CNTL10, "PHY_CNTL10"},
		{ANACTRL_DIF_PHY_CNTL11, "PHY_CNTL11"},
		{ANACTRL_DIF_PHY_CNTL12, "PHY_CNTL12"},
		{ANACTRL_DIF_PHY_CNTL13, "PHY_CNTL13"},
		{ANACTRL_DIF_PHY_CNTL14, "PHY_CNTL14"},
		{ANACTRL_DIF_PHY_CNTL15, "PHY_CNTL15"},
		{ANACTRL_DIF_PHY_CNTL16, "PHY_CNTL16"},
		{ANACTRL_DIF_PHY_CNTL17, "PHY_CNTL17"},
		{ANACTRL_DIF_PHY_CNTL18, "PHY_CNTL18"},
		{ANACTRL_DIF_PHY_CNTL19, "PHY_CNTL19"},
		{ANACTRL_DIF_PHY_CNTL20, "PHY_CNTL20"},
		{ANACTRL_DIF_PHY_CNTL21, "PHY_CNTL21"}
	};

	len += str_add_reg_sets(pdrv, buf, offset, LCD_REG_DBG_ANA_BUS, 0,
				reg_table, ARRAY_SIZE(reg_table));
	return len;
}

static void lcd_phy_cntl_set_lane_t7(unsigned int flag, unsigned int base_val,
		unsigned int preem_val)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(t7_lane_cntl_reg); i++) {
		if (flag & (1 << i))
			lcd_ana_write(t7_lane_cntl_reg[i], base_val | preem_val << 28);
	}
}

static void lcd_phy_cntl_set_dsi(struct phy_config_s *phy_cfg, uint8_t status)
{
	unsigned int temp, flag = phy_cfg->lane_valid;
	unsigned int data_lane     = status ? 0x022a0028 : 0;
	unsigned int data_lane_aux = status ? 0x0000ffcf : 0;
	unsigned int  clk_lane     = status ? 0x822a0028 : 0;

	if (flag & (1 << 0)) {
		// only lcd0_port0
		temp = data_lane;
		if (!(phy_cfg->ch_ctrl[0].pn_swap == 0 || phy_cfg->ch_ctrl[0].pn_swap == 0xff))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, temp);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, data_lane_aux);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, status ? (0xffff << 16) : 0);
	}
	if (flag & (1 << 1)) {
		temp = data_lane;
		if (!(phy_cfg->ch_ctrl[1].pn_swap == 0 || phy_cfg->ch_ctrl[1].pn_swap == 0xff))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, temp);
	}
	if (flag & (1 << 2)) {
		temp = clk_lane;
		if (!(phy_cfg->ch_ctrl[2].pn_swap == 0 || phy_cfg->ch_ctrl[2].pn_swap == 0xff))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, temp);
	}
	if (flag & (1 << 3)) {
		temp = data_lane;
		if (!(phy_cfg->ch_ctrl[3].pn_swap == 0 || phy_cfg->ch_ctrl[3].pn_swap == 0xff))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL5, temp);
	}
	if (flag & (1 << 4)) {
		temp = data_lane;
		if (!(phy_cfg->ch_ctrl[4].pn_swap == 0 || phy_cfg->ch_ctrl[4].pn_swap == 0xff))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, temp);
	}

	if (flag & (1 << 8)) {
		temp = data_lane;
		if (phy_cfg->lane_offset == 8 && // lcd1
		    (!(phy_cfg->ch_ctrl[0].pn_swap == 0 || phy_cfg->ch_ctrl[0].pn_swap == 0xff)))
			temp |= 0x4;
		if (phy_cfg->lane_offset == 0 && // lcd0_port1
		    (!(phy_cfg->ch_ctrl[5].pn_swap == 0 || phy_cfg->ch_ctrl[5].pn_swap == 0xff)))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, temp);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, data_lane_aux);
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL21, status ? 0xffff : 0);
	}
	if (flag & (1 << 9)) {
		temp = data_lane;
		if (phy_cfg->lane_offset == 8 && // lcd1
		    (!(phy_cfg->ch_ctrl[1].pn_swap == 0 || phy_cfg->ch_ctrl[1].pn_swap == 0xff)))
			temp |= 0x4;
		if (phy_cfg->lane_offset == 0 && // lcd0_port1
		    (!(phy_cfg->ch_ctrl[6].pn_swap == 0 || phy_cfg->ch_ctrl[6].pn_swap == 0xff)))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, temp);
	}
	if (flag & (1 << 10)) {
		temp = clk_lane;
		if (phy_cfg->lane_offset == 8 && // lcd1
		    (!(phy_cfg->ch_ctrl[2].pn_swap == 0 || phy_cfg->ch_ctrl[2].pn_swap == 0xff)))
			temp |= 0x4;
		if (phy_cfg->lane_offset == 0 && // lcd0_port1
		    (!(phy_cfg->ch_ctrl[7].pn_swap == 0 || phy_cfg->ch_ctrl[7].pn_swap == 0xff)))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, temp);
	}
	if (flag & (1 << 11)) {
		temp = data_lane;
		if (phy_cfg->lane_offset == 8 && // lcd1
		    (!(phy_cfg->ch_ctrl[3].pn_swap == 0 || phy_cfg->ch_ctrl[3].pn_swap == 0xff)))
			temp |= 0x4;
		if (phy_cfg->lane_offset == 0 && // lcd0_port1
		    (!(phy_cfg->ch_ctrl[8].pn_swap == 0 || phy_cfg->ch_ctrl[8].pn_swap == 0xff)))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, temp);
	}
	if (flag & (1 << 12)) {
		temp = data_lane;
		if (phy_cfg->lane_offset == 8 && // lcd1
		    (!(phy_cfg->ch_ctrl[4].pn_swap == 0 || phy_cfg->ch_ctrl[4].pn_swap == 0xff)))
			temp |= 0x4;
		if (phy_cfg->lane_offset == 0 && // lcd0_port1
		    (!(phy_cfg->ch_ctrl[9].pn_swap == 0 || phy_cfg->ch_ctrl[9].pn_swap == 0xff)))
			temp |= 0x4;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, temp);
	}
}

static void lcd_phy_cntl_set_lane_aux_t7(unsigned int flag, unsigned int aux_reg)
{
	if (flag & 0x01)
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, aux_reg);
	if (flag & 0x0100)
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, aux_reg);
}

static void lcd_lvds_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x06430028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0xfefe, 0x06530028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0100ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static void lcd_vbyone_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x06430028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0xfefe, 0x06530028,
					 phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0100ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00401640 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	if (!phy_ctrl_p)
		return;

	lcd_phy_cntl_set_dsi(&pdrv->curr_dev->dev_cfg.phy_cfg, status);
	if (status) {
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x1e406253);
	} else {
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static void lcd_edp_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;

	if (!phy_ctrl_p)
		return;

	if (status) {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0x06530028, phy_cfg->preem_level);
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid & 0x0101, 0x46770038, 0); //DP aux lane
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0x0000ffff);

		lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0x00406240 | phy_cfg->vswing_level);
	} else {
		lcd_phy_cntl_set_lane_t7(phy_cfg->lane_valid, 0, 0);
		lcd_phy_cntl_set_lane_aux_t7(phy_cfg->lane_valid, 0);
		if (phy_ctrl_p->lane_lock_total == 0)
			lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, 0);
	}
}

static unsigned int lcd_phy_preem_level_to_value_t7(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level)
{
	unsigned int preem_value = 0;

	preem_value = (level >= 0xf) ? 0xf : level;

	return preem_value;
}

static int lcd_phy_param_get_from_reg(struct aml_lcd_drv_s *pdrv,
				      struct phy_config_s *phy_cfg, struct phy_attr_s *phy)
{
	unsigned int data32, i;

	phy->cv_mode = 0;

	data32 = lcd_ana_read(ANACTRL_DIF_PHY_CNTL19);
	phy->ref_bias = 0;
	phy->vswing = data32 & 0xf;

	for (i = 0; i < phy_cfg->lane_num; i++) {
		data32 = lcd_ana_read(t7_lane_cntl_reg[i]);

		phy_cfg->ch_ctrl[i].en = data32 ? 1 : 0;

		phy->lane[i].amp = 0;
		phy->lane[i].rterm = 0; //only for vm
		phy->lane[i].preem = (data32 >> 28) & 0xf;

		phy_cfg->ch_ctrl[i].phase_sel = 0;
		phy_cfg->ch_ctrl[i].pn_swap = (data32 & 0x4) ? 1 : 0;
	}

	return 0;
}

static struct lcd_phy_ctrl_s lcd_phy_ctrl_t7 = {
	.lane_num = 16,
	.lane_lock_total = 0,

	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_t7,
	.phy_amp_dft_val = NULL,
	.phy_glb_param_dft_val = NULL,
	.phy_lane_phase_sel_def = NULL,
	.phy_param_get = lcd_phy_param_get_from_reg,
	.phy_reg_dump = lcd_phy_reg_dump,
	.phy_reset = NULL,

	.phy_set_lvds = lcd_lvds_phy_set,
	.phy_set_vx1 = lcd_vbyone_phy_set,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = lcd_edp_phy_set,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct lcd_data_s *pdata)
{
	char *curr_vout_connector;
	phy_ctrl_p = &lcd_phy_ctrl_t7;

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	curr_vout_connector = get_uboot_connector0_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	curr_vout_connector = get_uboot_connector1_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	curr_vout_connector = get_uboot_connector2_type();
	if (!strncmp("DP-", curr_vout_connector, 3))
		phy_ctrl_p->lane_lock_total |= 1 << 17;
#endif
	return phy_ctrl_p;
}
