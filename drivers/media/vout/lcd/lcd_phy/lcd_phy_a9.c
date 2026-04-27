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
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/efuse.h>
#include "../lcd_reg.h"
#include "lcd_phy_config.h"

#define PHY_DEF_ODT  0x0
#define PHY_DEF_BIAS 0x10
static unsigned int cali_bias, cali_odt;
static u32 chctrl_reg[] = {
	ANACTRL_DIF_PHY_CNTL1_A9, ANACTRL_DIF_PHY_CNTL2_A9,
	ANACTRL_DIF_PHY_CNTL3_A9, ANACTRL_DIF_PHY_CNTL4_A9,
	ANACTRL_DIF_PHY_CNTL5_A9, ANACTRL_DIF_PHY_CNTL1_A9,
	ANACTRL_DIF_PHY_CNTL2_A9, ANACTRL_DIF_PHY_CNTL3_A9,
	ANACTRL_DIF_PHY_CNTL4_A9, ANACTRL_DIF_PHY_CNTL5_A9,
};

static unsigned int lcd_phy_get_def_odt(void)
{
	int efuse_odt = 0;

	efuse_odt = efuse_amlogic_cali_item_read(EFUSE_CALI_SUBITEM_P2P_VINLP);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("efuse odt_cm=%#x\n", efuse_odt);
	if (efuse_odt <= 0) {
		efuse_odt = PHY_DEF_ODT;
		LCDERR("odt_cm uncalibrated, use odt=%#x\n", efuse_odt);
	}
	return (unsigned int)efuse_odt;
}

static unsigned int lcd_phy_get_def_bias(void)
{
	int efuse_bias = 0;

	efuse_bias = efuse_amlogic_cali_item_read(EFUSE_CALI_SUBITEM_P2P_COMMON);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("efuse bias=%#x\n", efuse_bias);
	if (efuse_bias <= 0) {
		efuse_bias = PHY_DEF_BIAS;
		LCDERR("bias uncalibrated, use bias=%#x\n", efuse_bias);
	}
	return (unsigned int)efuse_bias;
}

static int lcd_phy_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s reg_table[] = {
		{ANACTRL_DIF_PHY_CNTL1_A9,  "PHY_CNTL1"},
		{ANACTRL_DIF_PHY_CNTL2_A9,  "PHY_CNTL2"},
		{ANACTRL_DIF_PHY_CNTL3_A9,  "PHY_CNTL3"},
		{ANACTRL_DIF_PHY_CNTL4_A9,  "PHY_CNTL4"},
		{ANACTRL_DIF_PHY_CNTL5_A9,  "PHY_CNTL5"},
		{ANACTRL_DIF_PHY_CNTL6_A9,  "PHY_CNTL6"},
		{ANACTRL_DIF_PHY_CNTL7_A9,  "PHY_CNTL7"},
		{ANACTRL_DIF_PHY_CNTL8_A9,  "PHY_CNTL8"},
		{ANACTRL_DIF_PHY_CNTL9_A9,  "PHY_CNTL9"},
	};

	len += str_add_reg_sets(pdrv, buf, offset, LCD_REG_DBG_VX1_LVDS_CTRL_BUS, 0,
				reg_table, ARRAY_SIZE(reg_table));
	return len;
}

/*
 * update odt based on efuse trim value
 *   write_odt = cali_odt + (custom_odt - DEF_ODT)
 */
static unsigned int lcd_phy_get_write_odt(struct phy_attr_s *phy)
{
	int odt = cali_odt + phy->odt - PHY_DEF_ODT;

	odt = odt < 0 ? 0 : odt;
	odt = odt > 0xf ? 0xf : odt;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("cali odt=0x%x, write odt=0x%x\n", cali_odt, odt);
	return odt;
}

static int lcd_phy_param_get_from_reg(struct aml_lcd_drv_s *pdrv,
				      struct phy_config_s *phy_cfg, struct phy_attr_s *phy)
{
	return 0;
}

#define CH_NUM_EACH_GROUP 5

static void lcd_phy_common_update(struct aml_lcd_drv_s *pdrv, unsigned int common0,
					unsigned int common1, unsigned int common2)
{
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;
	struct phy_attr_s *phy = pdrv->curr_dev->dev_cfg.phy_cfg.act_phy;

	if (phy_cfg->lane_num > CH_NUM_EACH_GROUP) {
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL6_A9, common0, 0, 16);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL6_A9, common0, 16, 16);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL7_A9,
					common1 | lcd_phy_get_write_odt(phy), 0, 16);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL7_A9,
					common1 | lcd_phy_get_write_odt(phy), 16, 16);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL8_A9, common2, 0, 16);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL8_A9, common2, 16, 16);
	} else {
		if (pdrv->index) {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL6_A9, common0, 16, 16);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL7_A9,
						common1 | phy->odt, 16, 16);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL8_A9, common2, 16, 16);
		} else {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL6_A9, common0, 0, 16);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL7_A9,
						common1 | phy->odt, 0, 16);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL8_A9, common2, 0, 16);
		}
	}
}

static void lcd_phy_bias_common_update(struct aml_lcd_drv_s *pdrv, unsigned int bias_common)
{
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9, bias_common, 0, 16);
}

static void lcd_phy_serializer_common_update(struct aml_lcd_drv_s *pdrv)
{
	unsigned char model_set = 0;
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
		model_set = 0;
		break;
	case LCD_MIPI:
		model_set = 1;
		break;
	case LCD_VBYONE:
		model_set = 2;
		break;
	default:
		break;
	}

	if (phy_cfg->lane_num > CH_NUM_EACH_GROUP) {
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9, 1 | (model_set << 2), 16, 4);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9, 1 | (model_set << 2), 20, 4);
	} else {
		if (pdrv->index)
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9,
						1 | (model_set << 2), 20, 4);
		else
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9,
						1 | (model_set << 2), 16, 4);
	}
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL9_A9, 2, 24, 3);
}

static void lcd_phy_ch_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int chctrl = 0;
	unsigned char i, bit;
	struct phy_config_s *phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;
	struct phy_attr_s *phy = pdrv->curr_dev->dev_cfg.phy_cfg.act_phy;

	for (i = 0; i < phy_cfg->lane_num; i++) {
		if ((phy_cfg->lane_valid & (1 << i)) == 0)
			continue;
		bit = pdrv->index ? 16 : 0;
		if (i >= CH_NUM_EACH_GROUP)
			bit = 16;
		chctrl = 1 << 13;
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type  == LCD_LVDS)
			chctrl |= 1 << 15;
		//if (phy->cv_mode == PHY_VMODE) {
			//chctrl |= (phy->lane[i].rterm & 0x7) << 10;
		chctrl |= 0x4 << 10;
		chctrl |= (phy->vcm & 0x7) << 7;

		chctrl |= (phy->lane[i].preem & 0x7) << 4;
		chctrl |= (phy->lane[i].amp & 0xf);
		lcd_vx1_lvds_ctrl_setb(pdrv, chctrl_reg[i], chctrl, bit, 16);
	}
}

static void lcd_lvds_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int common0 = 0;
	unsigned int common1 = 0x4020;
	unsigned int common2 = 0x4026;
	unsigned int bias_common = 0x2450;

	lcd_phy_common_update(pdrv, common0, common1, common2);
	lcd_phy_bias_common_update(pdrv, bias_common);
	lcd_phy_serializer_common_update(pdrv);

	lcd_phy_ch_set(pdrv);
}

static void lcd_vx1_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int common0 = 0x27;
	unsigned int common1 = 0x20;
	unsigned int common2 = 0x4000;
	unsigned int bias_common = 0xe050;

	lcd_phy_common_update(pdrv, common0, common1, common2);
	lcd_phy_bias_common_update(pdrv, bias_common);
	lcd_phy_serializer_common_update(pdrv);

	lcd_phy_ch_set(pdrv);
}

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int common0 = 0x27;
	unsigned int common1 = 0x6000;
	unsigned int common2 = 0x59;
	unsigned int bias_common = 0xe050;

	lcd_phy_common_update(pdrv, common0, common1, common2);
	lcd_phy_bias_common_update(pdrv, bias_common);
	lcd_phy_serializer_common_update(pdrv);

	lcd_phy_ch_set(pdrv);
}

static unsigned int lcd_phy_preem_level_to_val_a9(struct aml_lcd_drv_s *pdrv,
		struct aml_lcd_device_s *dev_p, unsigned int level)
{
	return 0;
}

static unsigned int lcd_phy_amp_dft_a9(struct aml_lcd_drv_s *pdrv,
		struct aml_lcd_device_s *dev_p)
{
	return 0xf;
}

static void phy_glb_param_dft_a9(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct phy_attr_s *phy = dev_p->dev_cfg.phy_cfg.act_phy;

	phy->ref_bias = 0;
	switch (dev_p->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_MIPI:
	case LCD_VBYONE:
		phy->vcm = 0x4;
		phy->odt = PHY_DEF_ODT;
		phy->vswing = 0xf;
		phy->cv_mode = 0;
		break;
	default:
		break;
	}
}

static struct lcd_phy_ctrl_s lcd_phy_ctrl_a9 = {
	.lane_num = 10,

	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_amp_dft_val = lcd_phy_amp_dft_a9,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_val_a9,
	.phy_lane_phase_sel_def = NULL,
	.phy_glb_param_dft_val = phy_glb_param_dft_a9,
	.phy_param_get = lcd_phy_param_get_from_reg,
	.phy_reg_dump = lcd_phy_reg_dump,

	.phy_reset = NULL,

	.phy_set_lvds = lcd_lvds_phy_set,
	.phy_set_vx1 = lcd_vx1_phy_set,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_a9(struct lcd_data_s *pdata)
{
	cali_odt = lcd_phy_get_def_odt();
	cali_bias = lcd_phy_get_def_bias();

	return &lcd_phy_ctrl_a9;
}
