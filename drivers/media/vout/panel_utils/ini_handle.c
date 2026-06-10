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
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include "ini_parse.h"
#include "panel_utils.h"

int handle_ini_mem_check(void *inip)
{
	unsigned char *p;
	char *identifier;

	//check identifier
	p = (unsigned char *)inip;
	identifier = (char *)(p + sizeof(struct ini_s));
	if (strcmp(identifier, INI_IDENTIFIER_STR)) {
		//not ini mem
		return -1;
	}

	return 0;
}

//maybe other file type mem
void handle_ini_parser_uninit(void *inip)
{
	struct ini_s *ini_buf;
	int ret;

	if (!inip)
		return;

	ret = handle_ini_mem_check(inip);
	if (ret) {
		//not sure mem_size, only free
		kfree(inip);
		return;
	}

	ini_buf = (struct ini_s *)inip;
	ini_handler_mem_free(ini_buf);
	kfree(inip);
}

static unsigned char *handle_ini_parser_init(int mem_size)
{
	unsigned char *local_ini_mem;
	unsigned char *identifier;
	struct ini_s *ini_buf;
	int total_size;

	if (mem_size)
		total_size = mem_size;
	else
		total_size = INI_PARSER_MEM_SIZE_DFT;

	local_ini_mem = kzalloc(total_size, GFP_KERNEL);
	if (!local_ini_mem)
		return NULL;

	ini_buf = (struct ini_s *)local_ini_mem;
	ini_buf->total_size = total_size;
	ini_buf->mem_size = total_size - sizeof(struct ini_s);
	ini_buf->mem_start_pos = sizeof(struct ini_s);

	//init identifier:
	identifier = local_ini_mem + ini_buf->mem_start_pos;
	strscpy(identifier, INI_IDENTIFIER_STR, INI_MEM_DATA_OFFSET);

	//init parser data offset
	ini_buf->mem_cur_pos = INI_MEM_DATA_OFFSET;

	return local_ini_mem;
}

void *handle_ini_file_parse(const char *file_buf, int mem_size)
{
	unsigned char *local_ini_mem = NULL;
	struct ini_s *ini_buf;
	int ret = 0;

	if (!file_buf)
		return NULL;

	if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL)
		INIPR("%s: mem_size:%d\n", __func__, mem_size);

	local_ini_mem = handle_ini_parser_init(mem_size);
	if (!local_ini_mem)
		return NULL;

	ini_buf = (struct ini_s *)local_ini_mem;
	ret = ini_parse_mem(ini_buf, file_buf);
	if (ret) {
		handle_ini_parser_uninit(local_ini_mem);
		return NULL;
	}
	ini_buf->crc32 = cal_CRC32(0, local_ini_mem + ini_buf->mem_start_pos, ini_buf->mem_cur_pos);
	return (void *)local_ini_mem;
}

int handle_ini_parser_mem_get_size(void *inip)
{
	struct ini_s *ini_buf;
	int size, ret;

	if (!inip) {
		INIERR("%s: error\n", __func__);
		return -1;
	}

	ret = handle_ini_mem_check(inip);
	if (ret)
		return -1;

	ini_buf = (struct ini_s *)inip;

	size = ini_buf->mem_start_pos + ini_buf->mem_cur_pos; //real mem size

	return size;
}

void *handle_ini_parser_dupmem(void *inip, int *buf_size)
{
	void *local_ini_mem;
	struct ini_s *ini_buf;
	unsigned char *p;
	int size;

	if (!inip) {
		INIERR("%s: error\n", __func__);
		return NULL;
	}
	ini_buf = (struct ini_s *)inip;

	size = ini_buf->mem_start_pos + ini_buf->mem_cur_pos; //real mem size
	local_ini_mem = kzalloc(size, GFP_KERNEL);
	if (!local_ini_mem)
		return NULL;

	memcpy(local_ini_mem, inip, size);

	//update dest_buf value
	ini_buf = (struct ini_s *)local_ini_mem;
	ini_buf->total_size = size;
	ini_buf->mem_size = ini_buf->mem_cur_pos;
	p = (unsigned char *)local_ini_mem;
	ini_buf->crc32 = cal_CRC32(0, p + ini_buf->mem_start_pos, ini_buf->mem_cur_pos);
	*buf_size = size;

	return local_ini_mem;
}

void *handle_ini_get_section(void *inip, const char *section)
{
	struct ini_s *ini_buf;
	struct ini_section_s *psec;

	if (!inip || !section)
		return NULL;
	ini_buf = (struct ini_s *)inip;

	psec = ini_get_section(ini_buf, section);
	return (void *)psec;
}

const char *handle_ini_get_str(void *inip, void *psec, const char *key, const char *def_val)
{
	struct ini_s *ini_buf;
	struct ini_section_s *ini_section;
	const char *value;

	if (!inip || !psec || !key)
		return def_val;

	ini_buf = (struct ini_s *)inip;
	ini_section = (struct ini_section_s *)psec;
	value = ini_get_string(ini_buf, ini_section, key, def_val);
	return value;
}

unsigned int handle_ini_get_val(void *inip, void *psec, const char *key, unsigned int def_val)
{
	struct ini_s *ini_buf;
	struct ini_section_s *ini_section;
	const char *str;
	unsigned int val;
	int ret;

	if (!inip || !psec || !key)
		return def_val;
	ini_buf = (struct ini_s *)inip;

	ini_section = (struct ini_section_s *)psec;
	str = ini_get_string(ini_buf, ini_section, key, NULL);
	if (!str)
		return def_val;

	ret = kstrtouint(str, 0, &val);
	if (ret)
		return def_val;
	return val;
}

int handle_ini_key_exist(void *inip, void *psec, const char *key)
{
	struct ini_s *ini_buf;
	struct ini_section_s *ini_section;
	const char *str;

	if (!inip || !psec || !key)
		return 0;
	ini_buf = (struct ini_s *)inip;

	ini_section = (struct ini_section_s *)psec;
	str = ini_get_string(ini_buf, ini_section, key, NULL);
	if (!str)
		return 0;
	return 1;
}

int handle_ini_set_exist_single_key(void *inip, void *psec, const char *key, const char *str)
{
	struct ini_s *ini_buf;
	struct ini_section_s *ini_section;
	struct ini_line_s *ini_line;
	int ret;

	if (!inip || !psec || !key)
		return -1;
	ini_buf = (struct ini_s *)inip;

	ini_section = (struct ini_section_s *)psec;
	ini_line = ini_get_key_line_at_sec(ini_buf, ini_section, key);

	ret = ini_set_line_exist_key_val(ini_buf, ini_section, ini_line, str,
					 INI_SET_KEY_VAL_MODE_OVERWRITE);
	return ret;
}

int handle_ini_set_exist_keys(void *inip, void *psec,
			      const char *new_line, const char *new_value, int line_cnt)
{
	struct ini_s *ini_buf;
	struct ini_section_s *ini_section;
	int ret;

	if (!inip || !psec || !new_line || !new_value)
		return -1;
	ini_buf = (struct ini_s *)inip;

	ini_section = (struct ini_section_s *)psec;

	ret = ini_set_line_exist_keys(ini_buf, ini_section, new_line, new_value, line_cnt);
	return ret;
}

void handle_ini_list_key_value(void *inip)
{
	struct ini_s *ini_buf;

	if (!inip)
		return;
	ini_buf = (struct ini_s *)inip;

	ini_list_key_value(ini_buf);
}

void handle_ini_list_section(void *inip)
{
	struct ini_s *ini_buf;

	if (!inip)
		return;
	ini_buf = (struct ini_s *)inip;

	ini_list_section(ini_buf);
}

