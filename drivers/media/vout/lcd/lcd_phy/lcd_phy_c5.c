// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_reg.h"
#include "lcd_phy_config.h"

static int lcd_phy_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s reg_table[] = {
		{ANACTRL_MIPIDSI_CTRL0_C5, "ANACTRL_MIPIDSI_CTRL0"},
		{ANACTRL_MIPIDSI_CTRL1_C5, "ANACTRL_MIPIDSI_CTRL1"},
	};

	len += str_add_reg_sets(pdrv, buf, offset, LCD_REG_DBG_ANA_BUS, 0,
				reg_table, ARRAY_SIZE(reg_table));
	return len;
}

static void lcd_mipi_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	if (status) {
		lcd_ana_write(ANACTRL_MIPIDSI_CTRL0_C5, 0xa4870008);
		lcd_ana_write(ANACTRL_MIPIDSI_CTRL1_C5, 0x0001002e);
	} else {
		lcd_ana_write(ANACTRL_MIPIDSI_CTRL0_C5, 0x04070000);
		lcd_ana_write(ANACTRL_MIPIDSI_CTRL1_C5, 0x2e);
	}
}

static struct lcd_phy_ctrl_s lcd_phy_ctrl_c5 = {
	.lane_num = 5,
	.lane_lock_total = 0,

	.phy_vswing_level_to_val = NULL,
	.phy_amp_dft_val = NULL,
	.phy_preem_level_to_val = NULL,
	.phy_param_get = NULL,
	.phy_reg_dump = lcd_phy_reg_dump,
	.phy_reset = NULL,

	.phy_set_lvds = NULL,
	.phy_set_vx1 = NULL,
	.phy_set_mlvds = NULL,
	.phy_set_p2p = NULL,
	.phy_set_mipi = lcd_mipi_phy_set,
	.phy_set_edp = NULL,
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_c5(struct lcd_data_s *pdata)
{
	return &lcd_phy_ctrl_c5;
}
