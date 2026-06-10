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
#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/ctype.h>
// #include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include "ini_parse.h"
#include "panel_utils.h"

#define LCD_INI_PARSER_MEM_SIZE         204800 //200k

#define AML_START     "amlogic_start"
#define AML_END       "amlogic_end"

void *lcd_ini_get_section(void *inip, const char *section)
{
	return handle_ini_get_section(inip, section);
}

const char *lcd_ini_get_str(void *inip, void *psec, const char *key, const char *def_val)
{
	return handle_ini_get_str(inip, psec, key, def_val);
}

unsigned int lcd_ini_get_val(void *inip, void *psec, const char *key, unsigned int def_val)
{
	return handle_ini_get_val(inip, psec, key, def_val);
}

int lcd_ini_get_array_cnt(void *inip, void *psec, const char *key)
{
	const char *str;

	str = handle_ini_get_str(inip, psec, key, NULL);
	if (!str)
		return -1;

	return lcd_get_str_array_cnt(str);
}

int lcd_ini_get_array(void *inip, void *psec, const char *key, unsigned int *buf, unsigned int cnt)
{
	const char *str;

	if (!buf)
		return 0;

	str = handle_ini_get_str(inip, psec, key, NULL);
	if (!str)
		return -1;

	return lcd_trans_str_array(str, buf, cnt);
}

unsigned int lcd_ini_get_array_index(void *inip, void *psec, const char *key,
				     unsigned int index, unsigned int def_val)
{
	const char *str;

	str = handle_ini_get_str(inip, psec, key, NULL);
	if (!str)
		return def_val;

	return lcd_get_str_array_index(str, index, def_val);
}

int lcd_ini_set_exist_single_key(void *inip, void *psec, const char *key, const char *str)
{
	return handle_ini_set_exist_single_key(inip, psec, key, str);
}

int lcd_ini_set_exist_keys(void *inip, void *psec,
			   const char *new_line, const char *new_value, int line_cnt)
{
	return handle_ini_set_exist_keys(inip, psec, new_line, new_value, line_cnt);
}

static int lcd_ini_integrity_flag(void *inip)
{
	void *psec;
	const char *str = NULL;

	if (!inip)
		return -1;

	psec = lcd_ini_get_section(inip, "start");
	str = lcd_ini_get_str(inip, psec, "start_tag", "null");
	if (strcasecmp(str, AML_START)) {
		MODEL_PR("%s: start_tag (%s) is error!", __func__, str);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "end");
	str = lcd_ini_get_str(inip, psec, "end_tag", "null");
	if (strcasecmp(str, AML_END)) {
		MODEL_PR("%s: end_tag (%s) is error!", __func__, str);
		return -1;
	}

	return 0;
}

void *lcd_ini_file_parse(int index, unsigned char *file_buf)
{
	void *local_ini_mem;
	int ret = 0;

	rm_panel_file_parse_mem(index);
	local_ini_mem = handle_ini_file_parse(file_buf, LCD_INI_PARSER_MEM_SIZE);
	if (!local_ini_mem)
		return NULL;

	ret = lcd_ini_integrity_flag(local_ini_mem);
	if (ret) {
		handle_ini_parser_uninit(local_ini_mem);
		return NULL;
	}

	set_panel_file_parse_mem(index, local_ini_mem, LCD_INI_PARSER_MEM_SIZE, PANEL_FILE_INI);

	return local_ini_mem;
}

int lcd_ini_preload_mem(int index, void *parse_mem)
{
	void *local_ini_mem;
	int mem_size = 0, ret;

	ret = handle_ini_mem_check(parse_mem);
	if (ret)
		return -1;

	local_ini_mem = handle_ini_parser_dupmem(parse_mem, &mem_size);
	if (!local_ini_mem)
		return -1;

	set_lcd_panel_file_type(index, PANEL_FILE_INI);
	set_panel_file_parse_mem(index, local_ini_mem, mem_size, PANEL_FILE_INI);
	return 0;
}

void lcd_ini_mem_free(void *parse_mem)
{
	if (parse_mem)
		handle_ini_parser_uninit(parse_mem);
}

void lcd_ini_mem_dump(void *parse_mem)
{
	if (parse_mem)
		handle_ini_list_key_value(parse_mem);
}

void *get_lcd_ini_parse_mem(int index)
{
	if (get_lcd_panel_file_type(index) != PANEL_FILE_INI)
		return NULL;

	return get_panel_file_parse_mem(index);
}

void lcd_ini_list_key_value(int index)
{
	void *parse_mem = get_lcd_ini_parse_mem(index);

	if (parse_mem)
		handle_ini_list_key_value(parse_mem);
}
