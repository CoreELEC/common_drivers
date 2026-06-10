/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_LCD_MODEL_H
#define _AML_LCD_MODEL_H
#include <linux/amlogic/media/vout/panel_utils/json_parse.h>

static inline unsigned int cal_CRC32(unsigned int crc, const unsigned char *ptr, int buf_len)
{
	static const unsigned int s_crc32[16] = {
		0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
	};

	unsigned int crcu32 = crc;
	unsigned char b;

	if (buf_len <= 0)
		return 0;

	if (!ptr)
		return 0;

	crcu32 = ~crcu32;
	while (buf_len--) {
		b = *ptr++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b & 0xf)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b >> 4)];
	}

	return ~crcu32;
}

/*lcd_model*/
const char *get_lcd_config_load(unsigned char type);
int lcd_get_str_array_cnt(const char *data_str);
int lcd_trans_str_array(const char *data_str, unsigned int *data_buf, int cnt_max);
unsigned int lcd_get_str_array_index(const char *data_str, unsigned int index,
			unsigned int def_val);
void lcd_dbg_mem_dump(void *addr, size_t size);
int is_ukey_in_param_mem(void);
unsigned char *panel_param_mem_get(const char *name, u32 *len);
int path_name_compose(const char *path, const char *name, char *path_name);
unsigned char get_lcd_panel_file_type(int index);
void set_lcd_panel_file_type(int index, unsigned char type);
int set_panel_file_parse_mem(int index, void *parse_mem, int size, unsigned char type);
void *get_panel_file_parse_mem(int index);
void rm_panel_file_parse_mem(int index);
void dump_panel_file_parse_mem(int index);
int lcd_panel_file_pre_proc(void);

/*lcd_json*/
struct json_parse_s *get_panel_jsp(int index);
int lcd_panel_json_init_try_mem(int index, void *mem);
int lcd_panel_json_init_try_file(int index, void *mem, unsigned int fsize);
void panel_json_mem_free(void *mem);
void panel_json_mem_dump(void *mem);

/*lcd_ini*/
void *lcd_ini_get_section(void *inip, const char *section);
const char *lcd_ini_get_str(void *inip, void *psec, const char *key, const char *def_val);
unsigned int lcd_ini_get_val(void *inip, void *psec, const char *key, unsigned int def_val);
int lcd_ini_get_array_cnt(void *inip, void *psec, const char *key);
int lcd_ini_get_array(void *inip, void *psec, const char *key, unsigned int *buf, unsigned int cnt);
unsigned int lcd_ini_get_array_index(void *inip, void *psec, const char *key,
				     unsigned int index, unsigned int def_val);
int lcd_ini_set_exist_single_key(void *inip, void *psec, const char *key, const char *str);
int lcd_ini_set_exist_keys(void *inip, void *psec,
			   const char *new_line, const char *new_value, int line_cnt);
void *lcd_ini_file_parse(int index, unsigned char *file_buf);
int lcd_ini_preload_mem(int index, void *parse_mem);
void lcd_ini_mem_free(void *parse_mem);
void lcd_ini_mem_dump(void *mem);
void *get_lcd_ini_parse_mem(int index);
void lcd_ini_list_key_value(int index);

#define LCD_CONFIG_NONE 0
#define LCD_CONFIG_DTS  1
#define LCD_CONFIG_FILE 2
#define LCD_CONFIG_BSP  3
#define LCD_CONFIG_ERR  0xff

#define PANEL_FILE_INVALID  0
#define PANEL_FILE_INI      1
#define PANEL_FILE_JSON     2

unsigned char panel_config_load_detect(int index, const char *func_name);

#endif

