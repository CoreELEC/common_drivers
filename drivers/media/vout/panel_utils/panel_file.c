// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include <linux/amlogic/media/vout/panel_utils/panel_common.h>
#include "panel_utils.h"

#define PANEL_MAX_CNT 4

u8 panel_debug_print_flag = 0xf;
u8 panel_boot_init_level[PANEL_MAX_CNT];

static int panel_file_get_dbg_source(void)
{
	//return lcd_debug_ctrl_config.debug_para_source;
	return LCD_CONFIG_NONE;
}

unsigned char panel_config_load_detect(int index, const char *func_name)
{
	unsigned char load = LCD_CONFIG_NONE;
	unsigned char file_type = PANEL_FILE_INVALID;

	file_type = get_lcd_panel_file_type(index);
	load = panel_file_get_dbg_source();

	if (load != LCD_CONFIG_NONE) {
		switch (load) {
		case LCD_CONFIG_DTS:
			break;
		case LCD_CONFIG_FILE:
			if (file_type != PANEL_FILE_JSON && file_type != PANEL_FILE_INI)
				load = LCD_CONFIG_ERR;
			break;
		default:
			load = LCD_CONFIG_NONE;
		}
		goto panel_config_load_detect_done;
	}

	if (file_type == PANEL_FILE_INI || file_type == PANEL_FILE_JSON)
		load = LCD_CONFIG_FILE;
	else
		load = LCD_CONFIG_DTS;

panel_config_load_detect_done:
	if (load == LCD_CONFIG_ERR)
		PANEL_PR("%d %s: ERROR", index, func_name);
	else if (load == LCD_CONFIG_NONE)
		PANEL_PR("%d %s: NONE", index, func_name);
	if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL)
		PANEL_PR("%d %s: %d", index, func_name, load);

	return load;
}

#define LCD_EXT_CMD_TYPE_END 0xff

unsigned char *panel_init_table_dynamic_load_array(char *name, u32 *buf, int buf_len,
						 int tbl_max, int *tbl_cnt)
{
	unsigned char *table, type, size;
	int data_cnt, step = 0, i = 0;

	if (!buf || buf_len == 0 || tbl_max == 0 || !tbl_cnt)
		return NULL;

	PANEL_PR("%s: %s: buf_len=%d, tbl_max=%d", __func__, name, buf_len, tbl_max);

	while (1) {
		if ((i + 2) > buf_len) {
			PANEL_ERR("%s: %s: step[%d]: no ending error", __func__, name, step);
			return NULL;
		}
		if ((i + 2) > tbl_max) {
			PANEL_ERR("%s: %s: step[%d]: size out of support (max %d)",
			       __func__, name, step, tbl_max);
			return NULL;
		}
		type = buf[i];
		size = buf[i + 1];
		PANEL_PR("%s: %s: step[%d]: type=0x%x, size=%d, i=%d\n",
			__func__, name, step, type, size, i);
		i += 2;

		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (size == 0)
			goto init_dynamic_array_next;
		if ((i + size) > buf_len) {
			PANEL_ERR("%s: %s: step[%d]: size out of support (buf_len %d)",
			       __func__, name, step, buf_len);
			return NULL;
		}
		if ((i + size) > tbl_max) {
			PANEL_ERR("%s: %s: step[%d]: size out of support (max %d)",
				__func__, name, step, tbl_max);
			return NULL;
		}

		i += size;

init_dynamic_array_next:
		step++;
	}
	data_cnt = i;

	table = kzalloc(data_cnt, GFP_KERNEL);
	if (!table)
		return NULL;
	for (i = 0; i < data_cnt; i++)
		table[i] = buf[i];
	*tbl_cnt = data_cnt;

	return table;
}

unsigned char *panel_init_table_fixed_load_array(char *name, u8 cmd_size,
					       u32 *buf, int buf_len,  int tbl_max, int *tbl_cnt)
{
	unsigned char *table, type;
	int data_cnt, step = 0, i = 0;

	if (cmd_size == 0 || !buf || buf_len == 0 || tbl_max == 0 || !tbl_cnt)
		return NULL;

	PANEL_PR("%s: %s: buf_len=%d, tbl_max=%d", __func__, name, buf_len, tbl_max);

	while (1) {
		if ((i + cmd_size) > buf_len) {
			PANEL_ERR("%s: %s: step[%d]: no ending error", __func__, name, step);
			return NULL;
		}
		if ((i + cmd_size) > tbl_max) {
			PANEL_ERR("%s: %s: step[%d]: size out of support (max %d)",
			       __func__, name, step, tbl_max);
			return NULL;
		}
		type = buf[i];
		i += cmd_size;

		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		step++;
	}
	data_cnt = i;

	table = kzalloc(data_cnt, GFP_KERNEL);
	if (!table)
		return NULL;
	for (i = 0; i < data_cnt; i++)
		table[i] = buf[i];
	*tbl_cnt = data_cnt;

	return table;
}

unsigned char *panel_init_table_load_array(char *name, u8 cmd_size,
					 u32 *buf, int buf_len, int tbl_max, int *tbl_cnt)
{
	// if (cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
	if (cmd_size == 0xff)
		return panel_init_table_dynamic_load_array(name, buf, buf_len, tbl_max, tbl_cnt);

	return panel_init_table_fixed_load_array(name, cmd_size, buf, buf_len, tbl_max, tbl_cnt);
}

void set_panel_init_level(u8 index, u8 init_level)
{
	if (index >= PANEL_MAX_CNT)
		return;
	panel_boot_init_level[index] = init_level;
}

u8 get_panel_init_level(u8 index)
{
	if (index >= PANEL_MAX_CNT)
		return 0;
	return panel_boot_init_level[index];
}
