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
#include <linux/amlogic/media/vout/lcd/aml_lcd.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_common.h"

void lcd_cpu_gpio_probe(struct aml_lcd_drv_s *pdrv, unsigned int index)
{
	struct lcd_cpu_gpio_s *cpu_gpio;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCD_ERR(pdrv, "gpio index %d, exit", index);
		return;
	}
	cpu_gpio = &pdrv->drv_res.cpu_gpio[index];
	if (cpu_gpio->probe_flag) {
		LCD_DBG(pdrv, "gpio %s[%d] is already probed", cpu_gpio->name, index);
		return;
	}

	/* init gpio flag */
	cpu_gpio->probe_flag = 1;
	cpu_gpio->register_flag = 0;
}

static int lcd_cpu_gpio_register(struct aml_lcd_drv_s *pdrv, unsigned int index, int init_value)
{
	struct lcd_cpu_gpio_s *cpu_gpio;
	int value;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCD_ERR(pdrv, "%s: gpio index %d, exit", __func__, index);
		return -1;
	}
	cpu_gpio = &pdrv->drv_res.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCD_ERR(pdrv, "%s: gpio [%d] is not probed, exit", __func__, index);
		return -1;
	}
	if (cpu_gpio->register_flag) {
		LCD_PR(pdrv, "%s: gpio %s[%d] is already registered",
			__func__, cpu_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case LCD_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case LCD_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case LCD_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	cpu_gpio->gpio = devm_gpiod_get_index(pdrv->dev, "lcd_cpu", index, value);
	if (IS_ERR_OR_NULL(cpu_gpio->gpio)) {
		LCD_ERR(pdrv, "register gpio %s[%d]: %p, err: %d",
		       cpu_gpio->name, index, cpu_gpio->gpio, IS_ERR(cpu_gpio->gpio));
		return -1;
	}
	cpu_gpio->register_flag = 1;
	LCD_DBG(pdrv, "register gpio %s[%d]: %p, init value: %d",
		cpu_gpio->name, index, cpu_gpio->gpio, init_value);

	return 0;
}

void lcd_cpu_gpio_set(struct aml_lcd_drv_s *pdrv, unsigned int index, int value)
{
	struct lcd_cpu_gpio_s *cpu_gpio;

	if (index >= LCD_CPU_GPIO_NUM_MAX) {
		LCD_ERR(pdrv, "gpio index %d, exit", index);
		return;
	}
	cpu_gpio = &pdrv->drv_res.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCD_ERR(pdrv, "%s: gpio [%d] is not probed, exit", __func__, index);
		return;
	}
	if (cpu_gpio->register_flag == 0) {
		lcd_cpu_gpio_register(pdrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(cpu_gpio->gpio)) {
		LCD_ERR(pdrv, "gpio %s[%d]: %p, err: %ld",
		       cpu_gpio->name, index, cpu_gpio->gpio, PTR_ERR(cpu_gpio->gpio));
		return;
	}

	switch (value) {
	case LCD_GPIO_OUTPUT_LOW:
	case LCD_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(cpu_gpio->gpio, value);
		break;
	case LCD_GPIO_INPUT:
	default:
		gpiod_direction_input(cpu_gpio->gpio);
		break;
	}
	LCD_DBG(pdrv, "set gpio %s[%d] value: %d", cpu_gpio->name, index, value);
}

unsigned int lcd_cpu_gpio_get(struct aml_lcd_drv_s *pdrv, unsigned int index)
{
	struct lcd_cpu_gpio_s *cpu_gpio;

	cpu_gpio = &pdrv->drv_res.cpu_gpio[index];
	if (cpu_gpio->probe_flag == 0) {
		LCD_ERR(pdrv, "%s: gpio [%d] is not probed", __func__, index);
		return -1;
	}
	if (cpu_gpio->register_flag == 0) {
		LCD_ERR(pdrv, "%s: gpio %s[%d] is not registered", __func__, cpu_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(cpu_gpio->gpio)) {
		LCD_ERR(pdrv, "gpio[%d]: %p, err: %ld",
			index, cpu_gpio->gpio, PTR_ERR(cpu_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(cpu_gpio->gpio);
}

static void lcd_custom_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_config_s *pconf;
	unsigned int index;
	char pinmux_str[CUS_PINMUX_NAME_MAX + 4];

	pconf = &pdrv->curr_dev->dev_cfg;

	memset(pinmux_str, 0, sizeof(pinmux_str));
	if (status) {
		index = 1;
		sprintf(pinmux_str, "%s", pconf->cus_pinmux_name);
	} else {
		index = 2;
		sprintf(pinmux_str, "%s_off", pconf->cus_pinmux_name);
	}

	if (pconf->pinmux_flag == index) {
		LCD_PR(pdrv, "custom_pinmux: %s is already selected", pinmux_str);
		return;
	}

	pconf->pin = devm_pinctrl_get_select(pdrv->dev, pinmux_str);
	if (IS_ERR_OR_NULL(pconf->pin)) {
		LCD_ERR(pdrv, "custom_pinmux: %s(%d) error", pinmux_str, index);
		return;
	}

	pconf->pinmux_flag = index;
	LCD_DBG(pdrv, "custom_pinmux: %s(%d): 0x%px\n", pinmux_str, index, pconf->pin);
}

#ifdef CONFIG_AMLOGIC_LCD_DPI
static char *lcd_rgb_pinmux_str[] = {
	"none",          /* 0 */
	"rgb_sync_on",   /* 1 */
	"rgb_de_on",     /* 2 */
	"rgb_sync_de_on",/* 3 */
	"rgb_off"        /* 4 */
};

static char *lcd_bt_pinmux_str[] = {
	"none",      /* 0 */
	"bt656_on",  /* 1 */
	"bt656_off", /* 2 */
	"bt1120_on", /* 3 */
	"bt1120_off" /* 4 */
};
#endif

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
static char *lcd_vbyone_pinmux_str[] = {
	"none",      /* 0 */
	"vbyone",    /* 1 */
	"vbyone_off" /* 2 */
};
#endif

#ifdef CONFIG_AMLOGIC_LCD_TCON
static char *lcd_tcon_pinmux_str[] = {
	"none",          /* 0 */
	"tcon_p2p",      /* 1 */
	"tcon_p2p_usit", /* 2 */
	"tcon_p2p_off",  /* 3 */
	"tcon_mlvds",    /* 4 */
	"tcon_mlvds_off" /* 5 */
};
#endif

#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
static char *lcd_mipi_pinmux_str[] = {
	"none",   /* 0 */
	"dsi_on", /* 1 */
	"dsi_off" /* 2 */
};
#endif

static char *lcd_pinmux_index_get(struct aml_lcd_drv_s *pdrv, int status, unsigned int *index)
{
	struct lcd_config_s *pconf;
	unsigned int pinmux_idx = 0;
#ifdef CONFIG_AMLOGIC_LCD_TCON
	unsigned int p2p_type;
#endif
	char *pinmux_str = NULL;

	pconf = &pdrv->curr_dev->dev_cfg;
	switch (pconf->basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_DPI
	case LCD_RGB:
		if (status) {
			if (pconf->control.rgb_cfg.sync_valid &&
			    pconf->control.rgb_cfg.de_valid) {
				pinmux_idx = 3;
			} else if (pconf->control.rgb_cfg.de_valid) {
				pinmux_idx = 2;
			} else if (pconf->control.rgb_cfg.sync_valid) {
				pinmux_idx = 1;
			} else {
				return NULL;
			}
		} else {
			pinmux_idx = 4;
		}
		pinmux_str = lcd_rgb_pinmux_str[pinmux_idx];
		break;
	case LCD_BT656:
	case LCD_BT1120:
		if (pconf->basic.lcd_type == LCD_BT656)
			pinmux_idx = status ? 1 : 2;
		else if (pconf->basic.lcd_type == LCD_BT1120)
			pinmux_idx = status ? 3 : 4;
		else
			return NULL;
		pinmux_str = lcd_bt_pinmux_str[pinmux_idx];
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		pinmux_idx = status ? 1 : 2;
		pinmux_str = lcd_vbyone_pinmux_str[pinmux_idx];
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		pinmux_idx = status ? 4 : 5;
		pinmux_str = lcd_tcon_pinmux_str[pinmux_idx];
		break;
	case LCD_P2P:
		p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
		if (p2p_type == P2P_USIT)
			pinmux_idx = status ? 2 : 3;
		else
			pinmux_idx = status ? 1 : 3;
		pinmux_str = lcd_tcon_pinmux_str[pinmux_idx];
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		pinmux_idx = status ? 1 : 2;
		pinmux_str = lcd_mipi_pinmux_str[pinmux_idx];
		break;
#endif
	default:
		pinmux_idx = 0;
		pinmux_str = NULL;
		break;
	}

	*index = pinmux_idx;
	return pinmux_str;
}

void lcd_pinmux_set(struct aml_lcd_drv_s *pdrv, int status)
{
	unsigned int index;
	char *pinmux_str = NULL;

	if (!pdrv || !pdrv->curr_dev)
		return;

	if (strcmp(pdrv->curr_dev->dev_cfg.cus_pinmux_name, "null")) {
		lcd_custom_pinmux_set(pdrv, status);
		return;
	}

	pinmux_str = lcd_pinmux_index_get(pdrv, status, &index);
	if (!pinmux_str) {
		LCD_ERR(pdrv, "pinmux_set: %s invalid",
			lcd_type_type_to_str(pdrv->curr_dev->dev_cfg.basic.lcd_type));
		return;
	}
	if (index == 0)
		return;
	if (pdrv->curr_dev->dev_cfg.pinmux_flag == index) {
		LCD_PR(pdrv, "pinmux_set: %s is already selected", pinmux_str);
		return;
	}

	/* request pinmux */
	pdrv->curr_dev->dev_cfg.pin = devm_pinctrl_get_select(pdrv->dev, pinmux_str);
	if (IS_ERR_OR_NULL(pdrv->curr_dev->dev_cfg.pin)) {
		LCD_ERR(pdrv, "pinmux_set: %s(%d) error", pinmux_str, index);
		return;
	}

	pdrv->curr_dev->dev_cfg.pinmux_flag = index;
	LCD_DBG(pdrv, "pinmux_set: %s(%d): 0x%px", pinmux_str, index, pdrv->curr_dev->dev_cfg.pin);
}

