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

static unsigned int lvds_vx1_p2p_phy_ch_tl1 = 0x00020002;
static unsigned int p2p_low_common_phy_ch_tl1 = 0x000b000b;

static void lcd_phy_common_update(struct phy_config_s *phy, unsigned int com_data)
{
	/* vcm */
	if ((phy->flag & (1 << 1))) {
		com_data &= ~(0x7ff << 4);
		com_data |= (phy->vcm & 0x7ff) << 4;
	}
	/* ref bias switch */
	if ((phy->flag & (1 << 2))) {
		com_data &= ~(1 << 15);
		com_data |= (phy->ref_bias & 0x1) << 15;
	}
	/* odt */
	if ((phy->flag & (1 << 3))) {
		com_data &= ~(0xff << 24);
		com_data |= (phy->odt & 0xff) << 24;
	}
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL18, com_data);
}

/*
 *    chreg: channel ctrl
 *    bypass: 1=bypass
 *    mode: 1=normal mode, 0=low common mode
 *    ckdi: clk phase for minilvds
 */
static void lcd_phy_cntl_set(struct phy_config_s *phy, int status, int bypass,
				unsigned int mode, unsigned int ckdi)
{
	unsigned int cntl_vinlp_pi = 0, cntl_ckdi = 0;
	unsigned int data = 0, chreg = 0, chctl = 0;

	if (!phy_ctrl_p)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		chreg |= ((phy_ctrl_p->ctrl_bit_on << 16) |
			  (phy_ctrl_p->ctrl_bit_on << 0));
		chctl |= ((0x7 << 19) | (0x7 << 3));
		if (bypass)
			chctl |= ((1 << 18) | (1 << 2));
		if (mode) {
			chreg |= lvds_vx1_p2p_phy_ch_tl1;
			cntl_vinlp_pi = 0x00070000;
		} else {
			chreg |= p2p_low_common_phy_ch_tl1;
			if (phy->weakly_pull_down)
				chreg &= ~((1 << 19) | (1 << 3));
			cntl_vinlp_pi = 0x000e0000;
		}
		cntl_ckdi = ckdi | 0x80000000;
	} else {
		if (phy_ctrl_p->ctrl_bit_on)
			data = 0;
		else
			data = 1;
		chreg |= ((data << 16) | (data << 0));
		cntl_vinlp_pi = 0;
		cntl_ckdi = 0;
		lcd_ana_write(ANACTRL_DIF_PHY_CNTL18, 0);
	}

	lcd_ana_write(ANACTRL_DIF_PHY_CNTL19, cntl_vinlp_pi);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL20, cntl_ckdi);

	data = ((phy->lane[0].preem & 0xff) << 8 | (phy->lane[1].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL1, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL10, chctl);

	data = ((phy->lane[2].preem & 0xff) << 8 | (phy->lane[3].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL2, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL11, chctl);

	data = ((phy->lane[4].preem & 0xff) << 8 | (phy->lane[5].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL3, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL12, chctl);

	data = ((phy->lane[6].preem & 0xff) << 8 | (phy->lane[7].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL4, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL13, chctl);

	data = ((phy->lane[8].preem & 0xff) << 8 | (phy->lane[9].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL6, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL14, chctl);

	data = ((phy->lane[10].preem & 0xff) << 8 | (phy->lane[11].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL7, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL15, chctl);

	data = ((phy->lane[12].preem & 0xff) << 8 | (phy->lane[13].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL8, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL16, chctl);

	data = ((phy->lane[14].preem & 0xff) << 8 | (phy->lane[15].preem & 0xff) << 24);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL9, chreg | data);
	lcd_ana_write(ANACTRL_DIF_PHY_CNTL17, chctl);
}

static void lcd_lvds_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int com_data = 0;

	if (status == LCD_PHY_LOCK_LANE)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("vswing_level=0x%x\n", phy->vswing_level);

		com_data = 0xff2027e0 | phy->vswing;
		lcd_phy_common_update(phy, com_data);
		lcd_phy_cntl_set(phy, status, 1, 1, 0);
	} else {
		lcd_phy_cntl_set(phy, status, 1, 0, 0);
	}
}

static void lcd_vbyone_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy = &pdrv->config.phy_cfg;
	unsigned int com_data = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("%s: %d\n", __func__, status);

	if (status) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("vswing_level=0x%x, ext_pullup=%d\n",
			      phy->vswing_level, phy->ext_pullup);
		}

		if (phy->ext_pullup)
			com_data = 0xff2027e0 | phy->vswing;
		else
			com_data = 0xf02027a0 | phy->vswing;
		lcd_phy_common_update(phy, com_data);
		lcd_phy_cntl_set(phy, status, 1, 1, 0);
	} else {
		lcd_phy_cntl_set(phy, status, 1, 0, 0);
	}
}

static struct lcd_phy_ctrl_s lcd_phy_ctrl_t3x = {
	.ctrl_bit_on = 1,
	.lane_lock = 0,
	.phy_vswing_level_to_val = lcd_phy_vswing_level_to_value_dft,
	.phy_preem_level_to_val = lcd_phy_preem_level_to_value_dft,
	.phy_set_lvds = lcd_lvds_phy_set,
	.phy_set_vx1 = lcd_vbyone_phy_set,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = NULL,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_t3x(struct aml_lcd_drv_s *pdrv)
{
	phy_ctrl_p = &lcd_phy_ctrl_t3x;
	return phy_ctrl_p;
}
