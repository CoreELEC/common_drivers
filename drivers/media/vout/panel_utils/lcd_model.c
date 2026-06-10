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
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>

#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/aml_free_reserved.h>

#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/panel_utils/json_parse.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include "panel_utils.h"
struct panel_parse_mem_s {
	unsigned char type;
	int size;
	void *mem;
};

static struct panel_parse_mem_s glcd_file_parse_mem[LCD_MAX_DRV] = {
	{PANEL_FILE_INVALID, 0, NULL},
	{PANEL_FILE_INVALID, 0, NULL},
	{PANEL_FILE_INVALID, 0, NULL}
};

#define PANEL_PARAM_KEY_SIZE       64
#define PANEL_PARAM_HEAD_SIZE      PANEL_PARAM_KEY_SIZE
#define PANEL_PARAM_KEY_NAME_SIZE  (PANEL_PARAM_KEY_SIZE - 8)

struct panel_param_key_s {
	unsigned int size;
	unsigned int mem_pos;
	char name[PANEL_PARAM_KEY_NAME_SIZE];
};

struct panel_param_head_s {
	unsigned int _crc32;
	unsigned int size;
	unsigned short key_cnt;
	unsigned short ukey_exist;
	unsigned char rsvd[PANEL_PARAM_HEAD_SIZE - 12];
};

struct panel_param_mem_s {
	struct panel_param_head_s *head;
	unsigned char *mem;
	unsigned char *key_mem;
	struct panel_param_key_s *keys;
};

static struct panel_param_mem_s panel_param_mem = {NULL, NULL, NULL, NULL};

static char *panel_config_load_strs[] = {
	[LCD_CONFIG_NONE] = "none",
	[LCD_CONFIG_DTS] = "dts",
	[LCD_CONFIG_FILE] = "file"
};

const char *get_lcd_config_load(unsigned char type)
{
	if (type > LCD_CONFIG_FILE)
		return panel_config_load_strs[LCD_CONFIG_NONE];

	return panel_config_load_strs[type];
}

int lcd_get_str_array_cnt(const char *data_str)
{
	const char *p;
	int cnt = 0;

	for (p = data_str; *p != '\0'; p++) {
		if (*p == ',')
			cnt++;
	}
	if (*(p - 1) != ',')
		cnt++;

	return cnt;
}

int lcd_trans_str_array(const char *data_str, unsigned int *data_buf, int cnt_max)
{
	int str_len, i = 0;
	char *token = NULL, *end;
	char *tmp_buf = NULL;
	int ret;

	if (!data_str || !data_buf)
		return -1;

	str_len = strlen(data_str) + 1;
	tmp_buf = kzalloc(str_len, GFP_KERNEL);
	if (!tmp_buf)
		return -1;

	strscpy(tmp_buf, data_str, str_len);
	token = tmp_buf;
	while (*token != '\0') {
		if (i >= cnt_max)
			break;
		end = strchr(token, ',');
		if (end)
			*end = '\0';
		while (*token <= ' ' && *token != '\0') // Eliminate Spaces for this case: " 0x12"
			token++;
		if (*token == '\0') // for this case: "0x12,  "
			break;
		ret = kstrtouint(token, 0, &data_buf[i]);
		if (ret) {
			MODEL_PR("%s: index %d failed", __func__, i);
			kfree(tmp_buf);
			return -1;
		}
		i++;
		if (!end)
			break;
		token = end + 1;
	}

	kfree(tmp_buf);

	return i;
}

unsigned int lcd_get_str_array_index(const char *data_str, unsigned int index, unsigned int def_val)
{
	int str_len, i = 0;
	char *token = NULL, *end;
	char *tmp_buf = NULL;
	unsigned int val = def_val;
	int ret;

	if (!data_str)
		return def_val;

	str_len = strlen(data_str) + 1;
	tmp_buf = kzalloc(str_len, GFP_KERNEL);
	if (!tmp_buf)
		return def_val;

	strscpy(tmp_buf, data_str, str_len);
	token = tmp_buf;
	while (*token != '\0') {
		end = strchr(token, ',');
		if (end)
			*end = '\0';
		while (*token <= ' ' && *token != '\0') // Eliminate Spaces for this case: " 0x12"
			token++;
		if (*token == '\0') // for this case: "0x12,  "
			break;
		if (i == index) {
			ret = kstrtouint(token, 0, &val);
			if (ret)
				MODEL_PR("%s: index %d failed", __func__, index);
			break;
		}
		i++;
		if (!end)
			break;
		token = end + 1;
	}

	kfree(tmp_buf);

	return val;
}

void lcd_dbg_mem_dump(void *addr, size_t size)
{
	int i = 0, j = 0, k = 0, len = 0;
	unsigned char buf[128], *p = (unsigned char *)addr;

	if (!addr)
		return;
	MODEL_PR("memory dump addr:%px, size:0x%x\n", addr, (unsigned int)size);
	for (j = 0; j < size / 16; j++) {
		len = 0;
		for (i = 0; i < 16; i++, k++)
			len += sprintf(buf, "%02x ", p[k]);
		pr_info("0x%04x:%s\n", j * 0x10, buf);
	}
	for (i = 0, len = 0; i < size % 16; i++, k++)
		len += sprintf(buf, "%02x ", p[k]);
	MODEL_PR("0x%04x:%s\n", j * 0x10, buf);
}

int is_ukey_in_param_mem(void)
{
	return (panel_param_mem.head && panel_param_mem.head->ukey_exist) ? 1 : 0;
}

unsigned char *panel_param_mem_get(const char *name, u32 *len)
{
	unsigned int i = 0;
	struct panel_param_key_s *key;

	if (!panel_param_mem.mem || !panel_param_mem.head->key_cnt)
		return NULL;

	for (i = 0; i < panel_param_mem.head->key_cnt; i++) {
		key = &panel_param_mem.keys[i];
		if (strncmp(key->name, name, sizeof(key->name)) == 0) {
			*len = key->size;
			return panel_param_mem.key_mem + key->mem_pos;
		}
	}

	return NULL;
}

int path_name_compose(const char *path, const char *name, char *path_name)
{
	char *p1;
	const char *p2;
	int len1, len2, len, back = 0, k;

	if (!path || !name || !path_name)
		return -1;

	p2 = name;
	len2 = strlen(name);
	if (name[0] == '/') {//absolute path, ignore path
		strcpy(path_name, name);
		path_name[len2 + 1] = '\0';
		return 0;
	} else if (name[0] == '.' && name[1] == '/') {
		back = 0;
		p2 += 2;
	} else if (p2[0] == '.' && p2[1] == '.' && p2[2] == '/') {
		while (len2 > 0 && p2[0] == '.' && p2[1] == '.' && p2[2] == '/') {
			p2 += 3;
			len2 -= 3;
			back++;
		}
	}

	if (len2 <= 0) {
		path_name[0] = '\0';
		return -1;
	}

	p1 = path_name;
	len1 = strlen(path);
	len = len1;
	memcpy(path_name, path, len);
	path_name[len] = '\0';
	if (path_name[len - 1] != '/') {
		path_name[len] = '/';
		len += 1;
		path_name[len] = '\0';
	}
	back += 1;

	for (k = len - 1; k > 0; k--) {
		if (p1[k] == '/')
			back--;
		if (back == 0) {
			memcpy(p1 + k + 1, p2, len2);
			len = k + len2 + 1;
			p1[len] = '\0';
			return 0;
		}
	}
	return -1;
}

unsigned char get_lcd_panel_file_type(int index)
{
	return index < LCD_MAX_DRV ? glcd_file_parse_mem[index].type : PANEL_FILE_INVALID;
}

void set_lcd_panel_file_type(int index, unsigned char type)
{
	if (index >= LCD_MAX_DRV) {
		LRMERR("%s: invalid index %d\n", __func__, index);
		return;
	}

	glcd_file_parse_mem[index].type = type;
}

int set_panel_file_parse_mem(int index, void *parse_mem, int size, unsigned char type)
{
	if (index >= LCD_MAX_DRV)
		return -1;
	if (glcd_file_parse_mem[index].type == PANEL_FILE_INVALID) {
		glcd_file_parse_mem[index].type = type;
	} else {
		if (type != glcd_file_parse_mem[index].type) {
			LRMERR("%s: panel%d file type not match\n", __func__, index);
			return -1;
		}
	}

	glcd_file_parse_mem[index].size = size;
	glcd_file_parse_mem[index].mem = parse_mem;
	return 0;
}

void *get_panel_file_parse_mem(int index)
{
	if (index >= LCD_MAX_DRV)
		return NULL;

	return glcd_file_parse_mem[index].mem;
}

void rm_panel_file_parse_mem(int index)
{
	if (index >= LCD_MAX_DRV)
		return;

	if (!glcd_file_parse_mem[index].mem)
		return;

	switch (glcd_file_parse_mem[index].type) {
	case PANEL_FILE_JSON:
		panel_json_mem_free(glcd_file_parse_mem[index].mem);
		break;
	case PANEL_FILE_INI:
		lcd_ini_mem_free(glcd_file_parse_mem[index].mem);
		break;
	default:
		kfree(glcd_file_parse_mem[index].mem);
		break;
	}
	glcd_file_parse_mem[index].mem = NULL;
	glcd_file_parse_mem[index].size = 0;
	glcd_file_parse_mem[index].type = PANEL_FILE_INVALID;
}

void dump_panel_file_parse_mem(int index)
{
	if (index >= LCD_MAX_DRV)
		return;

	if (!glcd_file_parse_mem[index].mem)
		return;

	switch (glcd_file_parse_mem[index].type) {
	case PANEL_FILE_JSON:
		panel_json_mem_dump(glcd_file_parse_mem[index].mem);
		break;
	case PANEL_FILE_INI:
		lcd_ini_mem_dump(glcd_file_parse_mem[index].mem);
		break;
	default:
		break;
	}
}

int lcd_panel_file_pre_proc(void)
{
	char name[32];
	unsigned char *mem;
	unsigned int size, _crc32;
	struct panel_param_head_s *head;
	struct panel_param_key_s *key;
	int ret = -1, i = 0;
	void *ini_mem = NULL;

	mem = lcd_transmit_mem_get("panel_config", &size);
	if (!mem) {
		LRMPR("panel_config get fail");
		return -1;
	}

	head = (struct panel_param_head_s *)mem;
	_crc32 = cal_CRC32(0, mem + 4, head->size - 4);
	if (_crc32 != head->_crc32) {
		LRMPR("panel_config crc check fail:cal:0x%x-ori:0x%x", _crc32, head->_crc32);
		return -1;
	}

	panel_param_mem.mem = mem;
	panel_param_mem.head = (struct panel_param_head_s *)mem;
	panel_param_mem.keys = (struct panel_param_key_s *)(panel_param_mem.mem +
				PANEL_PARAM_HEAD_SIZE);
	head = panel_param_mem.head;
	size = head->key_cnt * PANEL_PARAM_KEY_SIZE + PANEL_PARAM_HEAD_SIZE;
	panel_param_mem.key_mem = panel_param_mem.mem + size;

	if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL) {
		LRMPR("%s panel_param_mem: size:0x%x, key_cnt:%d, ukey_exist:%d",
			__func__, head->size, head->key_cnt, head->ukey_exist);
		for (i = 0; i < head->key_cnt; i++) {
			key = &panel_param_mem.keys[i];
			LRMPR("[%d]: size;0x%x, mem_ofst:0x%x, name:%s",
				i, key->size, key->mem_pos, key->name);
		}
	}

	for (i = 0; i < LCD_MAX_DRV; i++) {
		snprintf(name, 31, "panel%d_jsp", i);
		mem = panel_param_mem_get(name, &size);
		if (mem) {
			ret = lcd_panel_json_init_try_mem(i, mem);
			goto lcd_panel_file_pre_loop_next;
		}

		snprintf(name, 31, "panel%d_json", i);
		mem = panel_param_mem_get(name, &size);
		if (mem) {
			ret = lcd_panel_json_init_try_file(i, mem, size);
			goto lcd_panel_file_pre_loop_next;
		}

		snprintf(name, 31, "panel%d_ini", i);
		mem = panel_param_mem_get(name, &size);
		if (mem) {
			ret = lcd_ini_preload_mem(i, mem);
			goto lcd_panel_file_pre_loop_next;
		}

		snprintf(name, 31, "panel%d_ini_file", i);
		mem = panel_param_mem_get(name, &size);
		if (mem) {
			ini_mem = lcd_ini_file_parse(i, mem);
			if (ini_mem)
				ret = 0;
			else
				ret = -1;
			goto lcd_panel_file_pre_loop_next;
		}

		continue;

lcd_panel_file_pre_loop_next:
		if (ret)
			LRMPR("%s: %s init %s", __func__, name, "fail");
	}

	return 0;
}

