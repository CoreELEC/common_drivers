// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "./lcd_connector.h"

#undef VPU_VOUT_BT_CTRL
#define VPU_VOUT_BT_CTRL                           0x0130

void lcd_rgb_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	//todo
}

void lcd_bt_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int field_type, mode_422, yc_swap, cbcr_swap;

	field_type = pconf->control.bt_cfg.field_type;
	mode_422 = pconf->control.bt_cfg.mode_422;
	yc_swap = pconf->control.bt_cfg.yc_swap;
	cbcr_swap = pconf->control.bt_cfg.cbcr_swap;

	if (on_off) {
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, field_type, 12, 1);
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, yc_swap, 0, 1);
		//0:cb first   1:cr first
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, cbcr_swap, 1, 1);
		//0:left, 1:right, 2:average
		lcd_vcbus_setb(VPU_VOUT_BT_CTRL, mode_422, 2, 2);
	}
}
