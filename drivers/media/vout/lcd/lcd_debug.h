/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_DEBUG_H__
#define __AML_LCD_DEBUG_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_reg.h"

#define LCD_DEBUG_REG_CNT_MAX    30
#define LCD_DEBUG_REG_END        0xffffffff

struct lcd_debug_info_s {
	unsigned int *reg_pinmux_table;

#ifdef CONFIG_AMLOGIC_LCD_LVDS
	int (*reg_dump_lvds)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	int (*reg_dump_vbyone)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	int (*reg_dump_mipi)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	int (*reg_dump_mlvds)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*reg_dump_p2p)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
#endif

	int (*interface_print)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*reg_dump_interface)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
};

static unsigned int lcd_reg_dump_pinmux_tl1[] = {
	PERIPHS_PIN_MUX_7_TL1,
	PERIPHS_PIN_MUX_8_TL1,
	PERIPHS_PIN_MUX_9_TL1,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_t5[] = {
	PERIPHS_PIN_MUX_5_TL1,
	PERIPHS_PIN_MUX_6_TL1,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_t7[] = {
	PADCTRL_PIN_MUX_REGK,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_t3[] = {
	PADCTRL_PIN_MUX_REG7,
	PADCTRL_PIN_MUX_REG8,
	LCD_DEBUG_REG_END
};

static unsigned int lcd_reg_dump_pinmux_c3[] = {
	PADCTRL_PIN_MUX_REG0,
	PADCTRL_PIN_MUX_REG1,
	PADCTRL_PIN_MUX_REG3,
	PADCTRL_PIN_MUX_REG4,
	PADCTRL_PIN_MUX_REGB,
	PADCTRL_PIN_MUX_REGJ,
	PADCTRL_PIN_MUX_REGK,
	LCD_DEBUG_REG_END
};

#endif
