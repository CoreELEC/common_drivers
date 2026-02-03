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
#include <linux/compat.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_data.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include "../../lcd_reg.h"
#include "../../lcd_common.h"
#include "lcd_tcon.h"
#include "lcd_tcon_pdf.h"
#include "../lcd_connector.h"

//1: unlocked, 0: locked, negative: locked, possible waiters
struct mutex lcd_tcon_dbg_mutex;

/*tcon adb port use */
#define LCD_ADB_TCON_REG_RW_MODE_NULL              0
#define LCD_ADB_TCON_REG_RW_MODE_RN                1
#define LCD_ADB_TCON_REG_RW_MODE_WM                2
#define LCD_ADB_TCON_REG_RW_MODE_WN                3
#define LCD_ADB_TCON_REG_RW_MODE_WS                4
#define LCD_ADB_TCON_REG_RW_MODE_ERR               5

#define ADB_TCON_REG_8_bit                         0
#define ADB_TCON_REG_32_bit                        1

struct lcd_tcon_adb_reg_s {
	unsigned int rw_mode;
	unsigned int bit_width;
	unsigned int addr;
	unsigned int len;
};

/*for tconless reg adb use*/
static struct lcd_tcon_adb_reg_s adb_reg = {
	.rw_mode = LCD_ADB_TCON_REG_RW_MODE_NULL,
	.bit_width = ADB_TCON_REG_8_bit,
	.addr = 0,
	.len = 0,
};

#define TCON_DEMURA_MODE_NULL  0
#define TCON_DEMURA_MODE_READ  1
#define TCON_DEMURA_MODE_WRITE 2
#define TCON_DEMURA_MODE_ERROR 0xff
struct lcd_tcon_demura_reg_s {
	unsigned int rw_mode;
	unsigned int bit_width;

	//read/write offset parameter
	unsigned int offset;

	//read/write length parameter, not distinguish with 8/32 bit
	unsigned int len;

	//read option may not completely print all data
	//because command line limit, so need to store read offset
	unsigned int rd_ofs;  //store read offset
	unsigned int rd_eofs; //store read end offset

	unsigned int mem_size;  //mem bytes
	unsigned char *mem;

	struct mutex lock;  //demura operation lock
};

static struct lcd_tcon_demura_reg_s demura_reg = {
	.rw_mode = TCON_DEMURA_MODE_NULL,
	.bit_width = 8,
	.offset = 0,
	.len = 0,
	.rd_ofs = 0,
	.rd_eofs = 0,
	.mem_size = 0,
	.mem = NULL,
};

static char *get_tcon_pmu_data_path_ini(struct aml_lcd_drv_s *pdrv)
{
	void *inip, *psec;
	const char *str;
	int len, i, k, cnt = 0;
	char  *mem = NULL, *p, tag_name[64];
#define MAX_DATA_PATH_CNT 32

	inip = get_lcd_ini_parse_mem(pdrv->index);
	if (!inip) {
		LCDERR("[%d]: %s: parse_mem not ready\n", pdrv->index, __func__);
		return NULL;
	}

	psec = lcd_ini_get_section(inip, "tcon_Path");
	if (!psec) {
		LCDERR("[%d]: %s: not find lcd_Attr\n", pdrv->index, __func__);
		return NULL;
	}

	mem = kzalloc(256 * MAX_DATA_PATH_CNT + 4, GFP_KERNEL);
	if (!mem)
		return NULL;

	p = mem + 4;
	for (i = 0; i < 4; i++) {
		snprintf(tag_name, 64, "TCON_EXT_B%d_BIN_PATH", i);
		str = lcd_ini_get_str(inip, psec, tag_name, NULL);
		if (str) {
			len = snprintf(p, 255, "%s:%s", tag_name, str);
			p += 256;
			cnt++;
			if (cnt >= MAX_DATA_PATH_CNT)
				goto get_tcon_pmu_data_path_ini_done;
		} else {
			for (k = 0; k < 10; k++) {
				snprintf(tag_name, 64, "TCON_EXT_B%d_%d_BIN_PATH", i, k);
				str = lcd_ini_get_str(inip, psec, tag_name, NULL);
				if (!str)
					continue;
				len = snprintf(p, 255, "%s:%s", tag_name, str);
				p += 256;
				cnt++;
				if (cnt >= MAX_DATA_PATH_CNT)
					goto get_tcon_pmu_data_path_ini_done;
			}
		}

		snprintf(tag_name, 64, "TCON_EXT_B%d_SPI_BIN_PATH", i);
		str = lcd_ini_get_str(inip, psec, tag_name, NULL);
		if (str) {
			len = snprintf(p, 155, "%s:%s", tag_name, str);
			p += 256;
			cnt++;
			if (cnt >= MAX_DATA_PATH_CNT)
				goto get_tcon_pmu_data_path_ini_done;
		} else {
			for (k = 0; k < 10; k++) {
				snprintf(tag_name, 64, "TCON_EXT_B%d_%d_SPI_BIN_PATH", i, k);
				str = lcd_ini_get_str(inip, psec, tag_name, NULL);
				if (!str)
					continue;
				len = snprintf(p, 255, "%s:%s", tag_name, str);
				p += 256;
				cnt++;
				if (cnt >= MAX_DATA_PATH_CNT)
					goto get_tcon_pmu_data_path_ini_done;
			}
		}
	}

get_tcon_pmu_data_path_ini_done:

	*(unsigned int *)mem = cnt;
	return mem;
}

static char *get_tcon_pmu_data_path_json(struct aml_lcd_drv_s *pdrv)
{
	struct json_parse_s *jsp;
	struct json_s *parent, *node;
	char  *mem, *p;
	const char *base_path = NULL, *val = NULL, *key;
	char path[256];
	int i = 0, cnt = 0, len = 0;

	jsp = get_panel_jsp(pdrv->index);
	if (!json_parse_ok(jsp)) {
		LCDERR("panel%d json not ready\n", pdrv->index);
		return NULL;
	}

	parent = json_path_to_node(jsp, jsp->root, "tcon");
	if (!parent) {
		LCDERR("find /interface\n");
		return NULL;
	}
	base_path = json_get_obj_str(jsp, parent, "panel_dir_kernel", NULL);

	parent = json_path_to_node(jsp, jsp->root, "tcon/pmu_data");
	if (!parent) {
		LCDERR("find /tcon/pmu_data\n");
		return NULL;
	}

	cnt = json_get_object_size(jsp, parent);
	if (cnt <= 0)
		return NULL;
	if (cnt > 32)
		cnt = 32;
	mem = kzalloc(256 * 32 + 4, GFP_KERNEL);
	if (!mem)
		return NULL;

	p = mem + 4;
	for (i = 0; i < cnt; i++, p += 256) {
		node = json_get_object_child_by_id(jsp, parent, i);

		key = json_get_key(jsp, node);
		val = json_get_str(jsp, node);
		path_name_compose(base_path, val, path);

		len = snprintf(p, 255, "%s:%s", key, path);
	}
	*(unsigned int *)mem = cnt;

	return mem;
}

static char *get_tcon_pmu_data_path(struct aml_lcd_drv_s *pdrv)
{
	unsigned char file_type = PANEL_FILE_INVALID;
	char *mem = NULL;

	if (!pdrv)
		return NULL;
	if (pdrv->config_load == LCD_CONFIG_FILE) {
		file_type = get_lcd_panel_file_type(pdrv->index);
		if (file_type == PANEL_FILE_JSON)
			mem = get_tcon_pmu_data_path_json(pdrv);
		else if (file_type == PANEL_FILE_INI)
			mem = get_tcon_pmu_data_path_ini(pdrv);
	}
	return mem;
}
static void lcd_tcon_multi_lut_print(void)
{
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct tcon_data_multi_s *data_multi;
	struct tcon_data_list_s *data_list;
	int i, j;

	if (!mm_table || !mm_table->data_multi || mm_table->data_multi_cnt == 0) {
		LCDERR("tcon data_multi is null\n");
		return;
	}

	LCDPR("tcon multi_update: %d\n", mm_table->multi_lut_update);
	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];
		LCDPR("data_multi[%d]:\n"
			"  type:        0x%x\n"
			"  list_cnt:    %d\n"
			"  bypass_flag: %d\n",
			i, data_multi->block_type, data_multi->list_cnt,
			data_multi->bypass_flag);
		if (data_multi->list_cur) {
			data_list = data_multi->list_cur;
			pr_info("data_multi[%d] current:\n"
				"  sel_id:        %d\n"
				"  sel_name:      %s\n"
				"  range:         %d, %d\n"
				"  ctrl_data_cnt: %d\n",
				i, data_list->id,
				data_list->block_name,
				data_list->multi.range.min,
				data_list->multi.range.max,
				data_list->ctrl_data_cnt);
			if (data_list->ctrl_data_cnt) {
				if (data_list->ctrl_data) {
					pr_info("  ctrl_data:\n");
					for (j = 0; j < data_list->ctrl_data_cnt; j++) {
						pr_info("    [%d]: %d\n",
							j, data_list->ctrl_data[j]);
					}
				} else {
					pr_info("  ctrl_data is NULL\n");
				}
			}
		} else {
			pr_info("data_multi[%d] current: NULL\n", i);
		}
		pr_info("data_multi[%d] list:\n", i);
		data_list = data_multi->list_header;
		while (data_list) {
			pr_info("  block[%d]: %s, range: %d,%d, ctrl_data_cnt:%d, vaddr=0x%px\n",
				data_list->id,
				data_list->block_name,
				data_list->multi.range.min,
				data_list->multi.range.max,
				data_list->ctrl_data_cnt,
				data_list->block_vaddr);
			data_list = data_list->next;
		}
		pr_info("\n");
	}
}

int lcd_tcon_axi_mem_print(struct tcon_rmem_s *tcon_rmem, char *buf, int offset)
{
	int len = 0, n, i;

	if (!tcon_rmem || !tcon_rmem->axi_rmem)
		return 0;

	if (tcon_rmem->secure_axi_rmem.sec_protect) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "axi secure mem paddr: 0x%lx, size:0x%x\n",
			(unsigned long)tcon_rmem->secure_axi_rmem.mem_paddr,
			tcon_rmem->secure_axi_rmem.mem_size);
		}
	for (i = 0; i < tcon_rmem->axi_bank; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"axi_mem[%d] paddr: 0x%lx\n"
			"axi_mem[%d] size:  0x%x\n",
			i, (unsigned long)tcon_rmem->axi_rmem[i].mem_paddr,
			i, tcon_rmem->axi_rmem[i].mem_size);
	}

	return len;
}

static int lcd_tcon_mm_table_print(struct tcon_mem_map_table_s *mm_table,
		struct tcon_rmem_s *tcon_rmem, char *buf, int offset)
{
	unsigned char *mem_vaddr;
	struct lcd_tcon_data_block_header_s *bin_header;
	struct tcon_data_list_s *cur_list;
	int len = 0, n, i;

	if (mm_table->data_mem_vaddr && mm_table->data_size) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"data_mem block_cnt: %d\n"
			"data_mem list:\n",
			mm_table->block_cnt);
		for (i = 0; i < mm_table->block_cnt; i++) {
			mem_vaddr = mm_table->data_mem_vaddr[i];
			n = lcd_debug_info_len(len + offset);
			if (mem_vaddr) {
				bin_header = (struct lcd_tcon_data_block_header_s *)mem_vaddr;
				len += snprintf((buf + len), n,
					"  [%d]: vaddr: 0x%px, size: 0x%x, %s(0x%x)\n",
					i, mem_vaddr,
					mm_table->data_size[i],
					bin_header->name, bin_header->block_type);
			} else {
				len += snprintf((buf + len), n,
					"  [%d]: vaddr: NULL, size:  0\n", i);
			}
		}
	}

	if (mm_table->data_multi_cnt > 0 && mm_table->data_multi) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"tcon multi_update: %d\n",
			mm_table->multi_lut_update);
		for (i = 0; i < mm_table->data_multi_cnt; i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"data_multi[%d] current (bypass:%d):\n",
				i, mm_table->data_multi[i].bypass_flag);
			if (!mm_table->data_multi[i].list_cur) {
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n, "  NULL\n");
			} else {
				cur_list = mm_table->data_multi[i].list_cur;
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"  type:      0x%x\n"
					"  list_cnt:  %d\n"
					"  sel_id:    %d\n"
					"  sel_name:  %s\n"
					"  sel_range: %d,%d\n",
					mm_table->data_multi[i].block_type,
					mm_table->data_multi[i].list_cnt,
					cur_list->id, cur_list->block_name,
					cur_list->multi.range.min,
					cur_list->multi.range.max);
			}
		}
	}

	return len;
}

static int lcd_tcon_lut_valid_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	int len = 0, n;

	if (!mm_table)
		return len;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lut_valid:\n"
		"acc:    %d\n"
		"od:     %d\n"
		"lod:    %d\n"
		"vac:    %d\n"
		"demura: %d\n"
		"dither: %d\n",
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_ACC),
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_OD),
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_LOD),
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_VAC),
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_DEMURA),
		!!(mm_table->lut_valid_flag & LCD_TCON_DATA_VALID_DITHER));

	return len;
}

int lcd_tcon_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	struct lcd_tcon_bin_path_header_s *bin_path_header;
	unsigned int size, file_size, cnt, m, sec_protect, sec_handle, *data;
	unsigned char *mem_vaddr;
	char *str;
	int len = 0, n, i, ret = 0;

	ret = lcd_tcon_valid_check();
	if (ret)
		return len;
	if (!mm_table || !tcon_rmem || !local_cfg || !tcon_conf)
		return len;

	lcd_tcon_init_setting_check(pdrv, &pdrv->curr_dev->dev_cfg.timing.act_timing,
			&local_cfg->cur_core_info);

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
			"\ntcon info:\n"
			"fw_ready:       %d\n"
			"core_reg_width: %d\n"
			"reg_table_len:  %d\n"
			"tcon_bin_ver:   %s\n"
			"tcon_rmem_flag: %d\n"
			"rsv_mem paddr:  0x%lx, size: 0x%x\n",
			tcon_fw ? tcon_fw->fw_ready : 0,
			tcon_conf->core_reg_width,
			tcon_conf->reg_table_len,
			local_cfg->cur_core_info.bin_ver,
			tcon_rmem->flag,
			(unsigned long)tcon_rmem->rsv_mem_paddr,
			tcon_rmem->rsv_mem_size);
	if (mm_table->bin_path_valid) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"bin_path_mem\n"
			"  paddr: 0x%lx, size: 0x%x\n"
			"  vaddr: 0x%px\n",
			(unsigned long)tcon_rmem->bin_path_rmem.mem_paddr,
			tcon_rmem->bin_path_rmem.mem_size,
			tcon_rmem->bin_path_rmem.mem_vaddr);
	}
	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,	"\n");

	if (tcon_rmem->flag) {
		len += lcd_tcon_axi_mem_print(tcon_rmem, (buf + len), (len + offset));
		if (mm_table->version < 0xff) {
			len += lcd_tcon_mm_table_print(mm_table, tcon_rmem,
					(buf + len), (len + offset));
		}

		if (tcon_rmem->bin_path_rmem.mem_vaddr) {
			mem_vaddr = tcon_rmem->bin_path_rmem.mem_vaddr;
			bin_path_header = (struct lcd_tcon_bin_path_header_s *)mem_vaddr;
			size = bin_path_header->data_size;
			cnt = bin_path_header->block_cnt;
			if (size < (32 + 256 * cnt))
				return len;
			if (cnt > 32)
				return len;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"\nbin_path rsv_mem\n"
				"bin cnt:        %d\n"
				"data_complete:  %d\n"
				"bin_path_valid: %d\n",
				cnt, mm_table->data_complete, mm_table->bin_path_valid);
			for (i = 0; i < cnt; i++) {
				m = 32 + 256 * i;
				file_size = *(unsigned int *)&mem_vaddr[m];
				str = (char *)&mem_vaddr[m + 4];
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"bin[%d]: size: 0x%x, %s\n", i, file_size, str);
			}
			mem_vaddr = get_tcon_pmu_data_path(pdrv);
			if (mem_vaddr) {
				cnt = *(unsigned int *)mem_vaddr;//path numbers
				if (cnt) {
					n = lcd_debug_info_len(len + offset);
					len += snprintf((buf + len), n, "pmu_data cnt=%d :\n", cnt);
					for (i = 0; i < cnt; i++) {
						n = lcd_debug_info_len(len + offset);
						len += snprintf((buf + len), n, "%s\n",
							mem_vaddr + 4 + 256 * i);
					}
				}
				kfree(mem_vaddr);
				mem_vaddr = NULL;
			}
		}

		if (tcon_rmem->secure_cfg_rmem.mem_vaddr) {
			data = (unsigned int *)tcon_rmem->secure_cfg_rmem.mem_vaddr;
			cnt = tcon_rmem->axi_bank;
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n,
				"\nsecure_cfg rsv_mem:\n"
				"  paddr: 0x%lx, size: 0x%x\n"
				"  vaddr: 0x%px\n",
				(unsigned long)tcon_rmem->secure_cfg_rmem.mem_paddr,
				tcon_rmem->secure_cfg_rmem.mem_size,
				tcon_rmem->secure_cfg_rmem.mem_vaddr);
			for (i = 0; i < cnt; i++) {
				m = 2 * i;
				sec_protect = *(data + m);
				sec_handle = *(data + m + 1);
				n = lcd_debug_info_len(len + offset);
				len += snprintf((buf + len), n,
					"  [%d]: protect: %d, handle: %d\n",
					i,  sec_protect, sec_handle);
			}
		}
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\n");
	len += lcd_tcon_lut_valid_print(pdrv, buf + len, len + offset);

	return len;
}

static void lcd_tcon_lut_ctrl_status_print(struct lcd_tcon_ctrl_s *tcon_ctrl, char *lut_str)
{
	pr_info("%s:\n"
		"acc:    %d\n"
		"od:     %d\n"
		"lod:    %d\n"
		"vac:    %d\n"
		"demura: %d\n"
		"dither: %d\n\n",
		lut_str,
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_ACC),
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_OD),
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_LOD),
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_VAC),
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_DEMURA),
		!!(tcon_ctrl->ctrl_val & TCON_FW_LUT_DITHER));
}

ssize_t lcd_tcon_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);

	return lcd_tcon_info_print(pdrv, buf, 0);
}

ssize_t lcd_tcon_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 520
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char **parm = NULL;
	unsigned int temp = 0, val, back_val, i, n, size = 0;
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	struct lcd_tcon_ctrl_s tcon_ctrl;
	unsigned char data;
	unsigned char *table = NULL;
	int ret = -1;
	struct lcd_tcon_vrr_data_s *vrr_data;
	unsigned int fr_levels[20];
	unsigned long flags = 0;
	unsigned int slave_addr, speed;
	struct lcd_tcon_ip27_s *ip27;

	if (!pdrv || !buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);

	if (strcmp(parm[0], "reg") == 0) {
		if (!parm[1]) {
			lcd_tcon_reg_readback_print(pdrv);
			goto lcd_tcon_debug_store_end;
		}
		if (strcmp(parm[1], "dump") == 0) {
			lcd_tcon_reg_readback_print(pdrv);
			goto lcd_tcon_debug_store_end;
		} else if (strcmp(parm[1], "rb") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("read tcon byte [0x%04x] = 0x%02x\n",
				temp, lcd_tcon_read_byte(pdrv, temp));
		} else if (strcmp(parm[1], "wb") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			data = (unsigned char)val;
			lcd_tcon_write_byte(pdrv, temp, data);
			pr_info("write tcon byte [0x%04x] = 0x%02x\n",
				temp, data);
		} else if (strcmp(parm[1], "wlb") == 0) { /*long write byte*/
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;

			if (!parm[3 + size]) {
				pr_info("size and data is not match\n");
				goto lcd_tcon_debug_store_err;
			}

			for (i = 0; i < size; i++) {
				ret = kstrtouint(parm[4 + i], 16, &val);
				if (ret)
					goto lcd_tcon_debug_store_err;
				data = (unsigned char)val;
				lcd_tcon_write_byte(pdrv, (temp + i), data);
				pr_info("write tcon byte [0x%04x] = 0x%02x\n",
					(temp + i), data);
			}
		} else if (strcmp(parm[1], "db") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 10, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon byte:\n");
			for (i = 0; i < size; i++) {
				pr_info("  [0x%04x] = 0x%02x\n",
					(temp + i),
					lcd_tcon_read_byte(pdrv, temp + i));
			}
		} else if (strcmp(parm[1], "r") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("read tcon [0x%04x] = 0x%08x\n",
				temp, lcd_tcon_read(pdrv, temp));
		} else if (strcmp(parm[1], "w") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			lcd_tcon_write(pdrv, temp, val);
			pr_info("write tcon [0x%04x] = 0x%08x\n",
				temp, val);
		} else if (strcmp(parm[1], "wl") == 0) { /*long write*/
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;

			if (!parm[3 + size]) {
				pr_info("size and data is not match\n");
				goto lcd_tcon_debug_store_err;
			}

			for (i = 0; i < size; i++) {
				ret = kstrtouint(parm[4 + i], 16, &val);
				if (ret)
					goto lcd_tcon_debug_store_err;
				lcd_tcon_write(pdrv, temp + i, val);
				pr_info("write tcon [0x%04x] = 0x%08x\n",
					(temp + i), val);
			}
		} else if (strcmp(parm[1], "d") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 10, &size);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon:\n");
			for (i = 0; i < size; i++) {
				pr_info("  [0x%04x] = 0x%08x\n",
					(temp + i),
					lcd_tcon_read(pdrv, temp + i));
			}
		}
	} else if (strcmp(parm[0], "table") == 0) {
		if (!local_cfg)
			goto lcd_tcon_debug_store_end;
		table = local_cfg->cur_core_info.table;
		size = local_cfg->cur_core_info.table_size;
		if (!table)
			goto lcd_tcon_debug_store_end;
		if (size == 0)
			goto lcd_tcon_debug_store_end;
		if (!parm[1]) {
			lcd_tcon_reg_table_print(&local_cfg->cur_core_info);
			goto lcd_tcon_debug_store_end;
		}
		if (strcmp(parm[1], "dump") == 0) {
			lcd_tcon_reg_table_print(&local_cfg->cur_core_info);
			goto lcd_tcon_debug_store_end;
		} else if (strcmp(parm[1], "r") == 0) {
			if (!parm[2])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			val = lcd_tcon_table_read(temp);
			pr_info("read table 0x%x = 0x%x\n", temp, val);
		} else if (strcmp(parm[1], "w") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &val);
			if (ret)
				goto lcd_tcon_debug_store_err;
			back_val = lcd_tcon_table_write(temp, val);
			pr_info("write table 0x%x = 0x%x, readback 0x%x\n",
				temp, val, back_val);
		} else if (strcmp(parm[1], "d") == 0) {
			if (!parm[3])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 16, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[3], 16, &n);
			if (ret)
				goto lcd_tcon_debug_store_err;
			pr_info("dump tcon table:\n");
			for (i = temp; i < (temp + n); i++) {
				if (i > size)
					break;
				data = table[i];
				pr_info("  [0x%04x]=0x%02x\n", i, data);
			}
		} else if (strcmp(parm[1], "update") == 0) {
			lcd_tcon_core_update(pdrv);
		} else if (strcmp(parm[1], "info") == 0) {
			lcd_tcon_core_info_print(pdrv, &local_cfg->cur_core_info);
		} else {
			goto lcd_tcon_debug_store_err;
		}
	} else if (strcmp(parm[0], "lut") == 0) {
		if (!tcon_fw || !tcon_fw->cmd_handler)
			goto lcd_tcon_debug_store_err;
		if (!parm[1]) {
			tcon_ctrl.ctrl_val = 0;
			tcon_ctrl.ctrl_mask = TCON_FW_LUT_MASK;
			tcon_ctrl.ctrl_type = TCON_FW_CTRL_LUT_VALID_GET;
			ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
			if (ret)
				goto lcd_tcon_debug_store_err;
			lcd_tcon_lut_ctrl_status_print(&tcon_ctrl, "lut_valid");

			tcon_ctrl.ctrl_val = 0;
			tcon_ctrl.ctrl_type = TCON_FW_CTRL_LUT_MULTI_GET;
			ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
			if (ret)
				goto lcd_tcon_debug_store_err;
			lcd_tcon_lut_ctrl_status_print(&tcon_ctrl, "lut_multi");
			goto lcd_tcon_debug_store_end;
		}
		if (strcmp(parm[1], "demo") == 0) {
			if (!parm[2]) {
				tcon_ctrl.ctrl_val = 0;
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_MASK;
				tcon_ctrl.ctrl_type = TCON_FW_CTRL_LUT_DEMO_GET;
				ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
				if (ret)
					goto lcd_tcon_debug_store_err;
				lcd_tcon_lut_ctrl_status_print(&tcon_ctrl, "lut_demo");
				goto lcd_tcon_debug_store_end;
			}
			if (strcmp(parm[2], "acc") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_ACC;
			else if (strcmp(parm[2], "od") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_OD;
			else if (strcmp(parm[2], "vac") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_VAC;
			else if (strcmp(parm[2], "demura") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_DEMURA;
			else if (strcmp(parm[2], "lod") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_LOD;
			else if (strcmp(parm[2], "dither") == 0)
				tcon_ctrl.ctrl_mask = TCON_FW_LUT_DITHER;
			else
				goto lcd_tcon_debug_store_err;
			if (!parm[3]) {
				tcon_ctrl.ctrl_type = TCON_FW_CTRL_LUT_DEMO_GET;
				ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
				if (ret)
					goto lcd_tcon_debug_store_err;
				pr_info("%s demo: %d\n", parm[2],
					!!(tcon_ctrl.ctrl_val & tcon_ctrl.ctrl_mask));
				goto lcd_tcon_debug_store_end;
			}
			ret = kstrtouint(parm[3], 10, &temp);
			if (ret)
				goto lcd_tcon_debug_store_err;
			tcon_ctrl.ctrl_type = TCON_FW_CTRL_LUT_DEMO_SET;
			tcon_ctrl.ctrl_val = temp ? tcon_ctrl.ctrl_mask : 0;
			ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
			if (ret)
				goto lcd_tcon_debug_store_err;
		} else {
			goto lcd_tcon_debug_store_err;
		}
	} else if (strcmp(parm[0], "check") == 0) {
		if (!tcon_fw || !tcon_fw->cmd_handler)
			goto lcd_tcon_debug_store_err;
		tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CHK_RATIONALITY, NULL);
	} else if (strcmp(parm[0], "comp") == 0) {
		lcd_tcon_complete_data_load(pdrv);
	} else if (strcmp(parm[0], "multi_lut") == 0) {
		lcd_tcon_multi_lut_print();
	} else if (strcmp(parm[0], "multi_update") == 0) {
		if (!mm_table)
			goto lcd_tcon_debug_store_end;
		if (!parm[1])
			goto lcd_tcon_debug_store_err;
		ret = kstrtouint(parm[1], 10, &temp);
		if (ret)
			goto lcd_tcon_debug_store_err;
		mm_table->multi_lut_update = temp;
		LCDPR("tcon multi_update: %d\n", temp);
	} else if (strcmp(parm[0], "multi_bypass") == 0) {
		if (!mm_table)
			goto lcd_tcon_debug_store_end;
		if (!parm[2])
			goto lcd_tcon_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp);
		if (ret)
			goto lcd_tcon_debug_store_err;
		if (strcmp(parm[1], "set") == 0)
			lcd_tcon_data_multi_bypass_set(mm_table, temp, 1);
		else //clr
			lcd_tcon_data_multi_bypass_set(mm_table, temp, 0);
	} else if (strcmp(parm[0], "tee") == 0) {
		if (!parm[1])
			goto lcd_tcon_debug_store_err;
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
		if (strcmp(parm[1], "status") == 0)
			lcd_tcon_mem_tee_get_status();
		else if (strcmp(parm[1], "off") == 0)
			ret = lcd_tcon_mem_tee_protect(0);
		else if (strcmp(parm[1], "on") == 0)
			ret = lcd_tcon_mem_tee_protect(1);
		else
			goto lcd_tcon_debug_store_err;
#endif
	} else if (strcmp(parm[0], "isr") == 0) {
		if (parm[1]) {
			if (strcmp(parm[1], "bypass") == 0) {
				if (parm[2]) {
					ret = kstrtouint(parm[2], 10, &temp);
					if (ret)
						goto lcd_tcon_debug_store_err;
					pdrv->tcon_isr_bypass = temp ? 1 : 0;
				}
				pr_info("tcon isr bypass: %d\n", pdrv->tcon_isr_bypass);
				goto lcd_tcon_debug_store_end;
			}
#ifdef TCON_DBG_TIME
			if (strcmp(parm[1], "dbg") == 0) {
				if (parm[2]) {
					ret = kstrtouint(parm[2], 16, &temp);
					if (ret)
						goto lcd_tcon_debug_store_err;
					if (temp)
						lcd_debug_print_flag |= LCD_DBG_PR_TEST;
					else
						lcd_debug_print_flag &= ~LCD_DBG_PR_TEST;
					pr_err("tcon isr dbg_log_en: %d\n", temp);
					goto lcd_tcon_debug_store_end;
				}
			} else if (strcmp(parm[1], "clr") == 0) {
				pdrv->tcon_isr_bypass = 1;
				lcd_delay_ms(100);
				lcd_tcon_dbg_trace_clear();
				pdrv->tcon_isr_bypass = 0;
				goto lcd_tcon_debug_store_end;
			} else if (strcmp(parm[1], "log") == 0) {
				pdrv->tcon_isr_bypass = 1;
				lcd_delay_ms(100);
				lcd_tcon_dbg_trace_print();
				pdrv->tcon_isr_bypass = 0;
				goto lcd_tcon_debug_store_end;
			}
#endif
		}
#ifdef TCON_DBG_TIME
		pr_err("tcon dbg_log_en: %d\n",
			(lcd_debug_print_flag & LCD_DBG_PR_TEST) ? 1 : 0);
		if (lcd_debug_print_flag & LCD_DBG_PR_TEST)
			lcd_tcon_dbg_trace_print();
#endif
	} else if (strcmp(parm[0], "ip27") == 0) {
		if (!local_cfg)
			goto lcd_tcon_debug_store_err;

		ip27 = &local_cfg->ip27;

		if (strcmp(parm[1], "init") == 0) {
			if (!parm[2] || !parm[3] || !parm[4] || !parm[5])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &temp);// trig mode
			ret |= kstrtouint(parm[3], 10, &val);//trig line
			ret |= kstrtouint(parm[4], 10, &size);//data_mode
			ret |= kstrtouint(parm[5], 10, &back_val);//vcom_order
			if (ret)
				goto lcd_tcon_debug_store_err;
			ip27->trig_mode = temp;
			ip27->trig_line_dbg = val;
			ip27->trig_line = val;
			ip27->data_mode = size;
			ip27->vcom_order = back_val;
			ip27->valid = 1;
			ip27->debug = 1;
			ip27->i2c_bytes = ip27->data_mode == 0 ? 24 + 1 : 3 + 1;
			tcon_ip27_init(pdrv, ip27);
			tcon_i2c_init(pdrv, ip27->i2c_addr, ip27->i2c_bytes, ip27->i2c_speed);
		} else if (strcmp(parm[1], "i2c") == 0) {
			if (!parm[2] || !parm[3] || !parm[4])
				goto lcd_tcon_debug_store_err;
			ret = kstrtouint(parm[2], 10, &speed);//
			ret |= kstrtouint(parm[3], 16, &slave_addr);
			ret |= kstrtouint(parm[4], 16, &temp);//reg offset
			if (ret)
				goto lcd_tcon_debug_store_err;

			LCDPR("speed:%d, addr:0x%x, reg_offset:0x%x\n", speed, slave_addr, temp);
			ip27->i2c_addr = slave_addr;
			ip27->i2c_speed = speed;
			ip27->i2c_reg_offset = temp;
			tcon_i2c_init(pdrv, ip27->i2c_addr, ip27->i2c_bytes, ip27->i2c_speed);
		} else if (strcmp(parm[1], "enable") == 0) {
			if (!ip27->valid)
				goto lcd_tcon_debug_store_err;

			spin_lock_irqsave(&pdrv->isr_lock, flags);
			if (ip27->en) {
				spin_unlock_irqrestore(&pdrv->isr_lock, flags);
				goto lcd_tcon_debug_store_end;
			}
			if (ip27->init) {
				//tcon_ip27_en_match(pdrv, ip27);
				if (!ip27->i2c_ready)
					tcon_i2c_init(pdrv, ip27->i2c_addr, ip27->i2c_bytes,
							ip27->i2c_speed);
				ip27->i2c_ready = 1;
				ip27->step = 11;
			}
			ip27->en = 1;
			spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		} else if (strcmp(parm[1], "disable") == 0) {
			if (!ip27->valid)
				goto lcd_tcon_debug_store_err;
			spin_lock_irqsave(&pdrv->isr_lock, flags);
			ip27->en = 0;
			ip27->step = 10; // will be disable in vsync isr
			spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		} else if (strcmp(parm[1], "status") == 0) {
			LCDPR("ip27:valid:%d, en:%d, init:%d, mode:%d, line:%d, step:%d\n"
				"i2c:addr:0x%x, speed:%d, bytes:%d, reg_offset:0x%x\n",
				ip27->valid, ip27->en, ip27->init,
				ip27->trig_mode, ip27->trig_line, ip27->step,
				ip27->i2c_addr, ip27->i2c_speed, ip27->i2c_bytes,
				ip27->i2c_reg_offset);
		}
	} else if (strcmp(parm[0], "fr_det") == 0) {
		if (!parm[1])
			goto lcd_tcon_debug_store_err;
		if (strcmp(parm[1], "enable") == 0) {
			tcon_fr_detect_enable(pdrv, 1);
		} else if (strcmp(parm[1], "disable") == 0) {
			tcon_fr_detect_enable(pdrv, 0);
		} else if (strcmp(parm[1], "init") == 0) {
			for (i = 0; i < 20; i++)
				fr_levels[i] = 0x1fff;
			ret = 0;
			for (i = 0; i < 20; i++) {
				if (parm[2 + i])
					ret = kstrtouint(parm[2 + i], 10, &val);
				else
					break;
				if (ret)
					break;

				fr_levels[i] = val;
			}
			LCDPR("fr:\n");
			for (i = 0; i < 20; i++)
				LCDPR("%d\n", fr_levels[i]);

			tcon_fr_detect_config(pdrv, 0, fr_levels, 20, 1);
		}
	} else if (strcmp(parm[0], "vrr_data") == 0) {
		if (!local_cfg)
			goto lcd_tcon_debug_store_err;
		vrr_data = &local_cfg->vrr_data;
		LCDPR("vrr_vdf:en:%d, base_mdoe:%dx%dp[%d~%d]hz\n",
			vrr_data->en, vrr_data->disp_h_active, vrr_data->disp_v_active,
			vrr_data->disp_frame_rate_min, vrr_data->disp_frame_rate_max);
		LCDPR("size:0x%x, part_size:0x%x, part:%d\n",
			vrr_data->size, vrr_data->part_size, vrr_data->part);
		LCDPR("fr count:%d\n", vrr_data->fr_count);
		for (i = 0; i < vrr_data->fr_count; i++)
			LCDPR("fr[%d]:%d\n", i, vrr_data->fr_level[i]);
	} else if (strcmp(parm[0], "reset") == 0) {
		lcd_tcon_global_reset(pdrv);
	} else {
		goto lcd_tcon_debug_store_err;
	}

lcd_tcon_debug_store_end:
	kfree(parm);
	kfree(buf_orig);
	return count;

lcd_tcon_debug_store_err:
	pr_info("invalid parameters\n");
	kfree(parm);
	kfree(buf_orig);
	return count;
#undef __MAX_PARAM
}

ssize_t lcd_tcon_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int len = 0;

	if (!pdrv->curr_dev)
		return sprintf(buf, "[%d]: no curr_dev\n", pdrv->index);

	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_P2P)
		len += sprintf(buf + len, "p2p_lock: %d\n", lcd_tcon_lock_status_get(pdrv));
	len += lcd_tcon_lut_valid_print(pdrv, buf + len, len);

	return len;
}

ssize_t lcd_tcon_reg_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	int len = 0;
	unsigned int i, addr;

	mutex_lock(&lcd_tcon_dbg_mutex);

	len += sprintf(buf + len, "for_tool:");
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0) {
		len += sprintf(buf + len, "ERROR\n");
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return len;
	}
	switch (adb_reg.rw_mode) {
	case LCD_ADB_TCON_REG_RW_MODE_NULL:
		len += sprintf(buf + len, "NULL");
		break;
	case LCD_ADB_TCON_REG_RW_MODE_RN:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WM:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			addr = adb_reg.addr;
			len += sprintf(buf + len, "%04x=%08x ",
				       addr, lcd_tcon_read(pdrv, addr));
		} else {
			addr = adb_reg.addr;
			len += sprintf(buf + len, "%04x=%02x ",
				       addr, lcd_tcon_read_byte(pdrv, addr));
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WN:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			for (i = 0; i < adb_reg.len; i++) {
				addr = adb_reg.addr + i;
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_WS:
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			addr = adb_reg.addr;
			for (i = 0; i < adb_reg.len; i++) {
				len += sprintf(buf + len, "%04x=%08x ",
					       addr, lcd_tcon_read(pdrv, addr));
			}
		} else {
			addr = adb_reg.addr;
			for (i = 0; i < adb_reg.len; i++) {
				len += sprintf(buf + len, "%04x=%02x ",
					       addr, lcd_tcon_read_byte(pdrv, addr));
			}
		}
		break;
	case LCD_ADB_TCON_REG_RW_MODE_ERR:
		len += sprintf(buf + len, "ERROR");
		break;
	default:
		len += sprintf(buf + len, "ERROR");
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_NULL;
		adb_reg.addr = 0;
		adb_reg.len = 0;
		break;
	}
	len += sprintf(buf + len, "\n");
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return len;
}

ssize_t lcd_tcon_reg_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 1500
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	char *buf_orig;
	char **parm = NULL;
	unsigned int  temp32 = 0, temp_reg = 0;
	unsigned int  temp_len = 0, temp_mask = 0, temp_val = 0;
	unsigned char temp8 = 0;
	int ret = -1, i;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return count;

	if (!buf)
		return count;

	mutex_lock(&lcd_tcon_dbg_mutex);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return count;
	}

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		mutex_unlock(&lcd_tcon_dbg_mutex);
		return count;
	}

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);

	if (strcmp(parm[0], "wn") == 0) {
		if (!parm[3])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 10, &temp_len);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (temp_len <= 0)
			goto lcd_tcon_adb_debug_store_err;
		if (!parm[4 + temp_len - 1])
			goto lcd_tcon_adb_debug_store_err;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			/*(4k - 9)/(8+1) ~=454*/
			if (temp_len > 454)
				goto lcd_tcon_adb_debug_store_err;
		} else {
			/*(4k - 9)/(2+1) ~=1362*/
			if (temp_len > 1362)
				goto lcd_tcon_adb_debug_store_err;
		}
		adb_reg.len = temp_len; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WN;
		for (i = 0; i < temp_len; i++) {
			ret = kstrtouint(parm[i + 4], 16, &temp_val);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit)
				lcd_tcon_write(pdrv, temp_reg, temp_val);
			else
				lcd_tcon_write_byte(pdrv, temp_reg, temp_val);
			temp_reg++;
		}
	} else if (strcmp(parm[0], "wm") == 0) {
		if (!parm[4])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 16, &temp_mask);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[4], 16, &temp_val);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		adb_reg.len = 1; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WM;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			temp32 = lcd_tcon_read(pdrv, temp_reg);
			temp32 &= ~temp_mask;
			temp32 |= temp_val & temp_mask;
			lcd_tcon_write(pdrv, temp_reg, temp32);
		} else {
			temp8 = lcd_tcon_read_byte(pdrv, temp_reg);
			temp8 &= ~temp_mask;
			temp8 |= temp_val & temp_mask;
			lcd_tcon_write_byte(pdrv, temp_reg, temp8);
		}
	} else if (strcmp(parm[0], "ws") == 0) {
		if (!parm[3])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[3], 10, &temp_len);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (temp_len <= 0)
			goto lcd_tcon_adb_debug_store_err;
		if (!parm[4 + temp_len - 1])
			goto lcd_tcon_adb_debug_store_err;
		if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
			/*(4k - 9)/(8+1) ~=454*/
			if (temp_len > 454)
				goto lcd_tcon_adb_debug_store_err;
		} else {
			/*(4k - 9)/(2+1) ~=1362*/
			if (temp_len > 1362)
				goto lcd_tcon_adb_debug_store_err;
		}
		adb_reg.len = temp_len; /* for cat use */
		adb_reg.addr = temp_reg;
		adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_WS;
		for (i = 0; i < temp_len; i++) {
			ret = kstrtouint(parm[i + 4], 16, &temp_val);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit)
				lcd_tcon_write(pdrv, temp_reg, temp_val);
			else
				lcd_tcon_write_byte(pdrv, temp_reg, temp_val);
		}
	} else if (strcmp(parm[0], "rn") == 0) {
		if (!parm[2])
			goto lcd_tcon_adb_debug_store_err;
		if (strcmp(parm[1], "8") == 0)
			adb_reg.bit_width = ADB_TCON_REG_8_bit;
		else if (strcmp(parm[1], "32") == 0)
			adb_reg.bit_width = ADB_TCON_REG_32_bit;
		else
			goto lcd_tcon_adb_debug_store_err;
		ret = kstrtouint(parm[2], 16, &temp_reg);
		if (ret)
			goto lcd_tcon_adb_debug_store_err;
		if (parm[3]) {
			ret = kstrtouint(parm[3], 10, &temp_len);
			if (ret)
				goto lcd_tcon_adb_debug_store_err;
			if (adb_reg.bit_width == ADB_TCON_REG_32_bit) {
				/*(4k - 9)/(8+1) ~=454*/
				if (temp_len > 454)
					goto lcd_tcon_adb_debug_store_err;
			} else {
				/*(4k - 9)/(2+1) ~=1362*/
				if (temp_len > 1362)
					goto lcd_tcon_adb_debug_store_err;
			}
			adb_reg.len = temp_len; /* for cat use */
			adb_reg.addr = temp_reg;
			adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_RN;
		} else {
			adb_reg.len = 1; /* for cat use */
			adb_reg.addr = temp_reg;
			adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_RN;
		}
	} else {
		goto lcd_tcon_adb_debug_store_err;
	}

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;

lcd_tcon_adb_debug_store_err:
	adb_reg.rw_mode = LCD_ADB_TCON_REG_RW_MODE_ERR;

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;
#undef __MAX_PARAM
}

ssize_t lcd_tcon_fw_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	ssize_t ret = 0;

	if (!tcon_fw)
		return ret;

	if (tcon_fw->debug_show)
		ret = tcon_fw->debug_show(tcon_fw, buf);
	else
		LCDERR("%s: tcon_fw is not installed\n", __func__);

	return ret;
}

ssize_t lcd_tcon_fw_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();

	if (!buf)
		return count;
	if (!tcon_fw)
		return count;

	mutex_lock(&lcd_tcon_dbg_mutex);

	if (tcon_fw->debug_store)
		tcon_fw->debug_store(tcon_fw, buf, count);
	else
		LCDERR("%s: tcon_fw is not installed\n", __func__);

	mutex_unlock(&lcd_tcon_dbg_mutex);
	return count;
}

static const char *lcd_debug_tcon_pdf_usage_str = {
	"Usage:\n"
	"  echo help > tcon_pdf\n"
	"  echo ctrl <grp_num> <enable> > tcon_pdf\n"
	"     pdf function(group=0xff) or group enable\n"
	"  echo add group <grp_num> > tcon_pdf\n"
	"     add pdf action group\n"
	"  echo add src <grp_num> <src_mode> <reg> <mask> <val> > tcon_pdf\n"
	"     add src register for group\n"
	"  echo add dst <grp_num> <dst_mode> <index> <mask> <val> > tcon_pdf\n"
	"     add dst action for group\n"
	"  echo del group <grp_num>\n"
	"     delete action group\n"
	"  cat tcon_pdf\n"
	"     show pdf current status\n"
};

ssize_t lcd_tcon_pdf_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcon_pdf_s *tcon_pdf = lcd_tcon_get_pdf();

	if (tcon_pdf->show_status)
		return tcon_pdf->show_status(tcon_pdf, buf);

	return 0;
}

ssize_t lcd_tcon_pdf_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 1500
	char *buf_orig;
	char **parm = NULL;
	int ret = -1;
	unsigned int val_arr[10] = { 0 };
	struct tcon_pdf_reg_s src;
	struct tcon_pdf_dst_s dst;
	struct tcon_pdf_s *tcon_pdf = lcd_tcon_get_pdf();

	if (!buf || !tcon_pdf)
		return count;

	mutex_lock(&lcd_tcon_dbg_mutex);

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		goto __lcd_tcon_pdf_dbg_store_exit;

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto __lcd_tcon_pdf_dbg_store_exit;

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);
	if (!strcmp(parm[0], "help")) {
		LCDPR("%s", lcd_debug_tcon_pdf_usage_str);
	} else if (!strcmp(parm[0], "ctrl")) {
		ret = kstrtouint(parm[1], 0, &val_arr[0]);
		if (ret)
			goto __lcd_tcon_pdf_dbg_store_exit;
		ret = kstrtouint(parm[2], 0, &val_arr[1]);
		if (ret)
			goto __lcd_tcon_pdf_dbg_store_exit;
		if (tcon_pdf->group_enable) {
			ret = tcon_pdf->group_enable(tcon_pdf,
				val_arr[0], val_arr[1]);
			if (ret < 0) {
				LCDERR("Ctrl group %d fail\n", val_arr[0]);
			} else {
				LCDPR("Group %d %s\n", val_arr[0],
					val_arr[1] ? "enable" : "disable");
			}
		}
	} else if (!strcmp(parm[0], "add")) {
		if (!strcmp(parm[1], "group")) {
			if (tcon_pdf->new_group) {
				ret = kstrtouint(parm[2], 0, &val_arr[0]);
				if (ret)
					goto __lcd_tcon_pdf_dbg_store_exit;
				ret = tcon_pdf->new_group(tcon_pdf, val_arr[0]);
				if (ret < 0)
					LCDERR("Add group %d fail\n", val_arr[0]);
				else
					LCDPR("Add group %d\n", ret);
			}
		} else if (!strcmp(parm[1], "src")) {
			ret = kstrtouint(parm[2], 0, &val_arr[0]);  //group
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[3], 0, &val_arr[1]);  //mode
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[4], 0, &val_arr[2]);  //reg
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[5], 0, &val_arr[3]);  //mask
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[6], 0, &val_arr[4]);  //val
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			src.mode = val_arr[1];
			src.reg  = val_arr[2];
			src.mask = val_arr[3];
			src.val  = val_arr[4];
			LCDPR("src mode=%d, reg=%#x, mask=%#x, val=%#x\n",
				src.mode, src.reg, src.mask, src.val);
			if (tcon_pdf->group_add_src) {
				ret = tcon_pdf->group_add_src(tcon_pdf,
					val_arr[0], &src);
				if (ret < 0) {
					LCDERR("Add group src fail\n");
				} else {
					LCDPR("Group %d add src success\n",
						val_arr[0]);
				}
			}
		} else if (!strcmp(parm[1], "dst")) {
			ret = kstrtouint(parm[2], 0, &val_arr[0]);  //group
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[3], 0, &val_arr[1]);  //mode
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[4], 0, &val_arr[2]);  //index
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[5], 0, &val_arr[3]);  //mask
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			ret = kstrtouint(parm[6], 0, &val_arr[4]);  //val
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			dst.mode  = val_arr[1];
			dst.index = val_arr[2];
			dst.mask  = val_arr[3];
			dst.val   = val_arr[4];
			LCDPR("dst mode=%d, index=%#x, mask=%#x, val=%#x\n",
				dst.mode, dst.index, dst.mask, dst.val);
			if (tcon_pdf->group_add_dst) {
				ret = tcon_pdf->group_add_dst(tcon_pdf,
					val_arr[0], &dst);
				if (ret < 0) {
					LCDERR("Add group dst fail\n");
				} else {
					LCDPR("Group %d add dst success\n",
						val_arr[0]);
				}
			}
		}
	} else if (!strcmp(parm[0], "del")) {
		if (!strcmp(parm[1], "group")) {
			ret = kstrtouint(parm[2], 0, &val_arr[0]);  //group
			if (ret)
				goto __lcd_tcon_pdf_dbg_store_exit;
			if (tcon_pdf->del_group) {
				ret = tcon_pdf->del_group(tcon_pdf, val_arr[0]);
				if (ret < 0) {
					LCDERR("Delete group %d fail\n",
						val_arr[0]);
				} else {
					LCDPR("Delete group %d success\n",
						val_arr[0]);
				}
			}
		}
	}

__lcd_tcon_pdf_dbg_store_exit:
	mutex_unlock(&lcd_tcon_dbg_mutex);
	kfree(parm);
	kfree(buf_orig);

	return count;
#undef __MAX_PARAM
}

ssize_t lcd_tcon_info_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct tcon_core_reg_info_s *core_info = NULL;
	struct lcd_tcon_reg_block_info_s *reg_blk_info = NULL;
	char user_version[TCON_BIN_VER_LEN];
	ssize_t len = 0;
	int i = 0;

	if (!local_cfg)
		return 0;

	core_info = &local_cfg->cur_core_info;
	if (!core_info->header)
		return sprintf((buf + len), "invalid header\n");

	memset(user_version, 0, sizeof(user_version));
	memcpy(user_version, core_info->header->version,
		sizeof(core_info->header->version));
	len += sprintf((buf + len),
		"current bin info:\n"
		"basic:\n"
		"userVersion=%s\n"
		"userBinName=%s\n"
		"h_active=%d\n"
		"v_active=%d\n"
		"block_ctrl=0x%x\n",
		user_version,
		core_info->header->name,
		core_info->header->h_active,
		core_info->header->v_active,
		core_info->header->block_ctrl);

	if (core_info->header->ext_header_size) {
		len += sprintf((buf + len),
			"framerate_min=%d\n"
			"framerate_max=%d\n"
			"reg_block_num=%d\n",
			core_info->ext_header.framerate_min,
			core_info->ext_header.framerate_max,
			core_info->ext_header.reg_blk_num);
		for (i = 0; i < core_info->ext_header.reg_blk_num; i++) {
			reg_blk_info = &core_info->ext_header.reg_blk_info[i];
			len += sprintf((buf + len),
				"  reg_block[%d]: start=%#x, len=%d(%#x)  (%#x~%#x)\n",
					i, reg_blk_info->start, reg_blk_info->len,
					reg_blk_info->len, reg_blk_info->start,
					(reg_blk_info->start + reg_blk_info->len - 1));
		}
	}
	if (core_info->user_info && strlen(core_info->user_info) > 0)
		len += sprintf((buf + len), "\nuser:\n%s\n", core_info->user_info);

	return len;
}

ssize_t lcd_tcon_demura_dbg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	unsigned char *mem;
	unsigned long *mem32;
	ssize_t size = 0, max_size = 4096 - 32, mem_len = 0;
	int i, j = 0, ret = -1, lmax_size = 0;
	int line_size = 0, blk_max_size = 0;

	if (!pdrv || (pdrv->status & LCD_STATUS_IF_ON) == 0)
		return 0;

	if (!demura_reg.mem)
		return 0;

	mutex_lock(&demura_reg.lock);

	mem = demura_reg.mem;
	mem32 = (unsigned long *)demura_reg.mem;
	mem_len = (demura_reg.bit_width == 32) ?
		lcd_do_div(demura_reg.mem_size, 4) : demura_reg.mem_size;

	line_size = 10 + 16 * ((demura_reg.bit_width == 32) ? 9 : 3);
	blk_max_size = lcd_do_div(max_size, line_size) * line_size + 2;
	lmax_size = blk_max_size;
	switch (demura_reg.rw_mode) {
	case TCON_DEMURA_MODE_READ:
	case TCON_DEMURA_MODE_WRITE:
		if (demura_reg.rw_mode == TCON_DEMURA_MODE_READ &&
				demura_reg.rd_ofs >= demura_reg.rd_eofs) {
			size += snprintf(buf + size, lmax_size, "Read done\n");
			break;
		}
		for (i = demura_reg.rd_ofs, j = 0;
				i < demura_reg.rd_eofs && i < mem_len && lmax_size > 0;
				i++, j++) {
			if ((j % 16) == 0) {
				if (lmax_size < line_size)
					break;
				ret = snprintf(buf + size, lmax_size,
					"%s%08x:", j ? "\n" : "", i);
				if (ret < 0) {
					LCDERR("Invalid return=%d\n", ret);
					goto __tcon_demura_show_exit;
				}
				size += ret;
				lmax_size -= ret;
			}
			if (demura_reg.bit_width == 32)
				ret = snprintf(buf + size, lmax_size, " %08lx", mem32[i]);
			else
				ret = snprintf(buf + size, lmax_size, " %02x", mem[i]);
			if (ret < 0) {
				LCDERR("Invalid return=%d\n", ret);
				goto __tcon_demura_show_exit;
			}
			size += ret;
			lmax_size -= ret;
			demura_reg.rd_ofs++;
		}
		ret = snprintf(buf + size, lmax_size, "\n");
		if (ret < 0) {
			LCDERR("Invalid return=%d\n", ret);
			goto __tcon_demura_show_exit;
		}
		size += ret;
		lmax_size -= ret;
		break;
	case TCON_DEMURA_MODE_ERROR:
		size += snprintf(buf + size, lmax_size,
			"Error Operation\n");
		break;
	default:
		size += snprintf(buf + size, lmax_size,
			"Unknown Operation\n");
		break;
	}

__tcon_demura_show_exit:
	mutex_unlock(&demura_reg.lock);
	return size;
}

ssize_t lcd_tcon_demura_dbg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
#define __MAX_PARAM 520
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct lcd_tcon_demura_reg_s demura_val;
	char **parm = NULL, *buf_orig = NULL;
	unsigned char *mem;
	unsigned long *mem32;
	unsigned int val = 0, real_len = 0, real_offset = 0;
	int ret = -1, i, mem_size = 0;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return count;

	mutex_lock(&demura_reg.lock);
	if (!demura_reg.mem) {
		mem = lcd_tcon_demura_mem_get(pdrv, &mem_size);
		if (!mem) {
			LCDERR("%s: get demura mem fail\n", __func__);
			goto __tcon_demura_store_err;
		}
		demura_reg.mem = mem;
		demura_reg.mem_size = mem_size;
	}
	mem = demura_reg.mem;
	mem32 = (unsigned long *)demura_reg.mem;
	mem_size = demura_reg.mem_size;

	memset(&demura_val, 0, sizeof(demura_val));
	demura_val.mem = mem;
	demura_val.mem_size = mem_size;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		goto __tcon_demura_store_err;

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto __tcon_demura_store_err;

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);
	if (!strcasecmp(parm[0], "read") ||
			!strcasecmp(parm[0], "rd") ||
			!strcasecmp(parm[0], "dump")) {
		demura_val.rw_mode = TCON_DEMURA_MODE_READ;
		ret = kstrtouint(parm[1], 0, &val);  //bit_width
		if (ret)
			goto __tcon_demura_store_err;
		demura_val.bit_width = val;
		ret = kstrtouint(parm[2], 0, &val);  //offset
		if (ret)
			goto __tcon_demura_store_err;
		demura_val.offset = val;
		ret = kstrtouint(parm[3], 0, &val);  //len
		if (ret)
			goto __tcon_demura_store_err;
		demura_val.len = val;

		//check param valid
		if (demura_val.bit_width != 32 &&
			demura_val.bit_width != 8) {
			LCDERR("Invalid bit width=%u\n", demura_val.bit_width);
			goto __tcon_demura_store_err;
		}
		real_len = demura_val.len;
		real_offset = demura_val.offset;
		if (demura_val.bit_width == 32) {
			real_len = real_len * 4;
			real_offset = real_offset * 4;
		}
		if ((real_offset + real_len) > mem_size) {
			LCDERR("offset(%u) or len(%u) is out of mem size(%d)\n",
				real_offset, real_len, mem_size);
			goto __tcon_demura_store_err;
		}
		val = lcd_do_div(demura_val.bit_width, 8) * demura_val.len;
		if (val > mem_size) {
			LCDERR("Read len(%u) is out of mem size(%d)\n",
				demura_val.len, mem_size);
			goto __tcon_demura_store_err;
		}
		demura_val.rd_ofs = demura_val.offset;
		demura_val.rd_eofs = demura_val.offset + demura_val.len;

		memcpy(&demura_reg, &demura_val, sizeof(demura_val));
	} else if (!strcasecmp(parm[0], "write") ||
			!strcasecmp(parm[0], "wr")) {
		demura_val.rw_mode = TCON_DEMURA_MODE_WRITE;
		ret = kstrtouint(parm[1], 0, &val);  //bit_width
		if (ret) {
			LCDERR("Invalid bit width\n");
			goto __tcon_demura_store_err;
		}
		demura_val.bit_width = val;
		ret = kstrtouint(parm[2], 0, &val);  //offset
		if (ret)
			goto __tcon_demura_store_err;
		demura_val.offset = val;
		ret = kstrtouint(parm[3], 0, &val);  //len
		if (ret)
			goto __tcon_demura_store_err;
		demura_val.len = val;

		//check param valid
		if (demura_val.bit_width != 32 &&
			demura_val.bit_width != 8) {
			LCDERR("Invalid bit width=%u\n", demura_val.bit_width);
			goto __tcon_demura_store_err;
		}
		real_len = demura_val.len;
		real_offset = demura_val.offset;
		if (demura_val.bit_width == 32) {
			real_len = real_len * 4;
			real_offset = real_offset * 4;
		}
		if ((real_offset + real_len) > mem_size) {
			LCDERR("offset(%u) or len(%u) is out of mem size(%d)\n",
				real_offset, real_len, mem_size);
			goto __tcon_demura_store_err;
		}
		demura_val.rd_ofs = demura_val.offset;
		demura_val.rd_eofs = demura_val.offset + demura_val.len;
		memcpy(&demura_reg, &demura_val, sizeof(demura_val));
		for (i = 0; i < demura_val.len; i++) {
			ret = kstrtouint(parm[i + 4], 0, &val);  //val
			if (ret) {
				LCDERR("Invalid data[%d] value\n", i);
				goto __tcon_demura_store_err;
			}
			if (demura_val.bit_width == 32)
				mem32[demura_val.offset + i] = val;
			else
				mem[demura_val.offset + i] = (unsigned char)val;
		}
	} else if (!strcasecmp(parm[0], "info")) {
		LCDPR("Demura info:\n");
		LCDPR("  mem addr =0x%p\n", demura_reg.mem);
		LCDPR("  mem size =%u (%#x)\n",
			demura_reg.mem_size, demura_reg.mem_size);
		switch (demura_reg.rw_mode) {
		case TCON_DEMURA_MODE_NULL:
			LCDPR("  mode     =NULL\n");
			break;
		case TCON_DEMURA_MODE_READ:
			LCDPR("  mode     =READ\n");
			break;
		case TCON_DEMURA_MODE_WRITE:
			LCDPR("  mode     =WRITE\n");
			break;
		case TCON_DEMURA_MODE_ERROR:
			LCDPR("  mode     =ERROR\n");
			break;
		default:
			LCDPR("  mode     =UNKNOWN\n");
			break;
		}
		LCDPR("  bit_width=%d\n", demura_reg.bit_width);
		LCDPR("  offset   =%d (%#x)\n", demura_reg.offset,
			demura_reg.offset);
		LCDPR("  len      =%d (%#x)\n", demura_reg.len,
			demura_reg.len);
		LCDPR("  rd offset     =%d (%#x)\n", demura_reg.rd_ofs,
			demura_reg.rd_ofs);
		LCDPR("  rd end offset =%d (%#x)\n", demura_reg.rd_eofs,
			demura_reg.rd_eofs);
	}

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&demura_reg.lock);
	return count;

__tcon_demura_store_err:
	demura_reg.rw_mode = TCON_DEMURA_MODE_ERROR;

	kfree(parm);
	kfree(buf_orig);
	mutex_unlock(&demura_reg.lock);
	return count;
#undef __MAX_PARAM
}

static struct device_attribute lcd_tcon_debug_attrs[] = {
	__ATTR(debug,     0644, lcd_tcon_debug_show, lcd_tcon_debug_store),
	__ATTR(status,    0444, lcd_tcon_status_show, NULL),
	__ATTR(reg,       0644, lcd_tcon_reg_debug_show, lcd_tcon_reg_debug_store),
	__ATTR(tcon_fw,   0644, lcd_tcon_fw_dbg_show, lcd_tcon_fw_dbg_store),
	__ATTR(tcon_pdf,  0644, lcd_tcon_pdf_dbg_show, lcd_tcon_pdf_dbg_store),
	__ATTR(tcon_rdma, 0644, lcd_tcon_rdma_dbg_show, lcd_tcon_rdma_dbg_store),
	__ATTR(tcon_demura, 0644, lcd_tcon_demura_dbg_show, lcd_tcon_demura_dbg_store),
	__ATTR(tcon_info, 0444, lcd_tcon_info_dbg_show, NULL),
};

/* **********************************
 * tcon IOCTL
 * **********************************
 */
static unsigned int lcd_tcon_bin_path_index;
static struct aml_lcd_tcon_ctrl_s tcon_ctrl;

long lcd_tcon_ioctl_handler(struct aml_lcd_drv_s *pdrv, int mcd_nr, unsigned long arg)
{
	void __user *argp;
	struct aml_lcd_tcon_bin_s lcd_tcon_buff;
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	struct lcd_tcon_data_block_header_s block_header, block_header_old;
	struct aml_lcd_dccd_config_s dccd_config;
	struct tcon_fw_core_reg_s *core_info = NULL, *temp_core;
	unsigned int size = 0, old_size, temp = 0, m = 0, header_size = 0;
	struct device *dev;
	phys_addr_t paddr = 0, paddr_old = 0;
	unsigned char *mem_vaddr = NULL, *vaddr = NULL, *vaddr_old = NULL;
	unsigned char *buf = NULL;
	char *str = NULL, name[32];
	int index = 0;
	int ret = 0;
	unsigned long flags;

	if (!pdrv)
		return -EFAULT;
	argp = (void __user *)arg;
	if (IS_ERR_OR_NULL(argp))
		return -EFAULT;

	memset(&dccd_config, 0, sizeof(dccd_config));

	switch (mcd_nr) {
	case LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO:
		if (!mm_table) {
			ret = -EFAULT;
			break;
		}
		if (copy_to_user(argp, &mm_table->block_cnt, sizeof(unsigned int)))
			ret = -EFAULT;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: get bin max_cnt: %d\n", mm_table->block_cnt);
		break;
	case LCD_IOC_SET_TCON_DATA_INDEX_INFO:
		if (copy_from_user(&temp, argp, sizeof(unsigned int)))
			ret = -EFAULT;
		if (temp < TCON_DATA_CNT_MAX)
			lcd_tcon_bin_path_index = temp;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: set bin index: %d\n", lcd_tcon_bin_path_index);
		break;
	case LCD_IOC_GET_TCON_BIN_PATH_INFO:
		if (!mm_table || !tcon_rmem) {
			ret = -EFAULT;
			break;
		}

		mem_vaddr = tcon_rmem->bin_path_rmem.mem_vaddr;
		if (!mem_vaddr) {
			LCDERR("%s: no tcon bin path rmem\n", __func__);
			ret = -EFAULT;
			break;
		}
		if (lcd_tcon_bin_path_index >= mm_table->block_cnt) {
			ret = -EFAULT;
			break;
		}
		m = 32 + 256 * lcd_tcon_bin_path_index;
		str = (char *)&mem_vaddr[m + 4];
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("tcon: get bin_path[%d]: %s\n", lcd_tcon_bin_path_index, str);

		if (copy_to_user(argp, str, sizeof(struct aml_lcd_path_s)))
			ret = -EFAULT;
		break;
	case LCD_IOC_GET_TCON_PMU_PATH:
		mem_vaddr = get_tcon_pmu_data_path(pdrv);
		if (!mem_vaddr) {
			ret = -EFAULT;
			break;
		}
		temp = *(unsigned int *)mem_vaddr;//path numbers
		if (copy_to_user(argp, mem_vaddr, temp * 256 + 4))
			ret = -EFAULT;
		kfree(mem_vaddr);
		mem_vaddr = NULL;
		break;
	case LCD_IOC_SET_TCON_BIN_DATA_INFO:
		if (!mm_table || !mm_table->data_mem_vaddr || !tcon_rmem) {
			ret = -EFAULT;
			break;
		}

		mem_vaddr = tcon_rmem->bin_path_rmem.mem_vaddr;
		if (!mem_vaddr) {
			LCDERR("%s: no tcon bin path rmem\n", __func__);
			ret = -EFAULT;
			break;
		}

		memset(&lcd_tcon_buff, 0, sizeof(struct aml_lcd_tcon_bin_s));
		if (copy_from_user(&lcd_tcon_buff, argp, sizeof(struct aml_lcd_tcon_bin_s))) {
			ret = -EFAULT;
			break;
		}
		if (lcd_tcon_buff.size == 0) {
			LCDERR("%s: invalid data size %d\n", __func__, size);
			ret = -EFAULT;
			break;
		}
		index = lcd_tcon_buff.index;
		if (index >= mm_table->block_cnt) {
			LCDERR("%s: invalid index %d\n", __func__, index);
			ret = -EFAULT;
			break;
		}
		m = 32 + 256 * index;
		str = (char *)&mem_vaddr[m + 4];
		temp = *(unsigned int *)&mem_vaddr[m];

		header_size = sizeof(struct lcd_tcon_data_block_header_s);
		memset(&block_header, 0, header_size);
		memset(&block_header_old, 0, header_size);
		vaddr_old = mm_table->data_mem_vaddr[index];
		paddr_old = mm_table->data_mem_paddr[index];
		if (vaddr_old)
			memcpy(&block_header_old, vaddr_old, header_size);

		argp = (void __user *)lcd_tcon_buff.ptr;
		if (IS_ERR_OR_NULL(argp)) {
			ret = -EFAULT;
			break;
		}

		if (copy_from_user(&block_header, argp, header_size)) {
			ret = -EFAULT;
			break;
		}

		dev = &pdrv->pdev->dev;
		old_size = block_header_old.block_size;
		size = block_header.block_size;
		if (size > lcd_tcon_buff.size || size < header_size) {
			LCDERR("%s: block[%d] size 0x%x error\n", __func__, index, size);
			ret = -EFAULT;
			break;
		}

		if (is_block_ctrl_dma(block_header.block_ctrl)) {
			sprintf(name, "tcon_data_block%d", index);
			if (lrm_exist())
				vaddr = lrm_alloc(size, &paddr, name);

			if (lcd_debug_print_flag)
				LCDPR("alloc for tcon mm_table[%d] form lcd_cma_pool\n"
					"pa:0x%llx, va:%p, size:0x%x\n",
					index, (unsigned long long)paddr, vaddr, size);
		} else {
			vaddr = kcalloc(size, sizeof(unsigned char), GFP_KERNEL);
			paddr = virt_to_phys(vaddr);
		}

		if (unlikely(!vaddr))
			goto set_tcon_bin_error_break;

		if (unlikely(copy_from_user(vaddr, argp, size)))
			goto set_tcon_bin_error_break;

		LCDPR("tcon: load bin_path[%d]: %s, size: 0x%x -> 0x%x\n", index, str, temp, size);

		mm_table->data_mem_vaddr[index] = vaddr;
		mm_table->data_mem_paddr[index] = paddr;
		ret = lcd_tcon_data_load(pdrv, vaddr, index);
		if (unlikely(ret))
			goto set_tcon_bin_error_break;

		break;
set_tcon_bin_error_break:
		mm_table->data_mem_vaddr[index] = vaddr_old;
		mm_table->data_mem_paddr[index] = paddr_old;
		if (is_block_ctrl_dma(block_header.block_ctrl) && lrm_exist())
			lrm_free(vaddr, paddr);
		else
			kfree(vaddr);
		ret = -EFAULT;
		break;
	case LCD_IOC_SET_TCON_BIN_DATA_FINISH:
		lcd_tcon_complete_data_load(pdrv);
		ret = 0;
		break;
	case TCON_IOC_SET_DATA_EFFECT:
	case TCON_IOC_SET_DATA_EFFECT_TYPE:
		ret = -EINVAL;
		if (!mm_table || !mm_table->data_mem_vaddr)
			break;
		if (mm_table->version == 0 || mm_table->version == 0xff)
			break;
		if (copy_from_user(&temp, argp, sizeof(unsigned int)))
			break;

		if ((pdrv->status & LCD_STATUS_TCON_RDY) == 0)
			break;
		if (mcd_nr == TCON_IOC_SET_DATA_EFFECT) {
			if ((temp & 0xff) == 0xff)
				mm_table->effect_id = 0xff;
			else if (temp < mm_table->block_cnt)
				mm_table->effect_id = temp;
			else
				break;
		} else {
			mm_table->effect_type = temp;
		}

		spin_lock_irqsave(&pdrv->isr_lock, flags);
		lcd_tcon_data_set(pdrv, mm_table);
		spin_unlock_irqrestore(&pdrv->isr_lock, flags);
		mm_table->effect_id = 0xff;
		mm_table->effect_type = 0xff;
		ret = 0;
		break;
	case TCON_IOC_SET_DCCD:
	case TCON_IOC_SET_CORE_BASE:
		ret = -EINVAL;
		if (copy_from_user(&dccd_config, argp, sizeof(dccd_config)))
			goto __dccd_set_exit;
		if (dccd_config.size < 4 || dccd_config.size > 0x10000)
			goto __dccd_set_exit;

		argp = (void __user *)dccd_config.data.ptr;
		if (IS_ERR_OR_NULL(argp)) {
			ret = -EFAULT;
			break;
		}
		buf = kzalloc(dccd_config.size, GFP_KERNEL);
		if (unlikely(!buf)) {
			LCDERR("Alloc dccd size=%d fail\n", dccd_config.size);
			goto __dccd_set_exit;
		}

		if (copy_from_user(buf, argp, dccd_config.size)) {
			LCDERR("Copy ptr size=%d fail\n", dccd_config.size);
			goto __dccd_set_exit;
		}
		dccd_config.data.ptr = buf;
		if (mcd_nr == TCON_IOC_SET_DCCD) {
			tcon_fw->buf_table_in = (void *)&dccd_config;
			if (tcon_fw->tcon_alg)
				ret = tcon_fw->tcon_alg(tcon_fw, 0);
		} else if (mcd_nr == TCON_IOC_SET_CORE_BASE) {
			ret = lcd_tcon_fw_add_core_table(tcon_fw, dccd_config.data.ptr);
		}
__dccd_set_exit:
		kfree(buf);
		break;
	case TCON_IOC_SET_QUICK_REG:
		ret = -EINVAL;
		if (!copy_from_user(&temp, argp, sizeof(temp))) {
			if (tcon_fw->cmd_handler) {
				ret = tcon_fw->cmd_handler(tcon_fw,
					FWCMD_AUTOCALC_SET_QK_REG, &temp);
			}
		}
		break;
	case TCON_IOC_GET_DCCD_FLG:
		temp = !!(tcon_fw->flag & TCON_FW_FLAG_DCCD_RUN);
		if (!copy_to_user(argp, &temp, sizeof(temp)))
			ret = 0;
		LCDPR("DCCD get flag(%d) %s!\n", temp, !ret ? "ok" : "error");
		break;
	case TCON_IOC_GET_CALC_STATUS:
		temp = 0;
		ret = -EINVAL;
		if (tcon_fw->cmd_handler) {
			ret = tcon_fw->cmd_handler(tcon_fw,
				FWCMD_AUTOCALC_GET_RDY_FLG, &temp);
		}
		if (!ret && !copy_to_user(argp, &temp, sizeof(temp)))
			ret = 0;
		LCDPR("DCCD get status(%d) %s!\n", temp, !ret ? "ok" : "error");
		break;
	case TCON_IOC_GET_CALC_BUF:
		if (!tcon_fw->config)
			goto __tcon_get_calc_exit;
		if (copy_from_user(&dccd_config, argp, sizeof(dccd_config)))
			goto __tcon_get_calc_exit;

		m = 0;
		temp = dccd_config.size;  //core reg index
		if (list_empty(&tcon_fw->config->core_reg_list))
			goto __tcon_get_calc_exit;

		list_for_each_entry(temp_core, &tcon_fw->config->core_reg_list, list) {
			if (temp == m++) {
				core_info = temp_core;
				break;
			}
		}
		if (!core_info)
			goto __tcon_get_calc_exit;
		dccd_config.size = core_info->init_header->block_size;
		if (copy_to_user(argp, &dccd_config, sizeof(dccd_config)))
			goto __tcon_get_calc_exit;

		argp = (void __user *)dccd_config.data.ptr;
		if (copy_to_user(argp, core_info->full_table, dccd_config.size))
			goto __tcon_get_calc_exit;
		ret = 0;

__tcon_get_calc_exit:
		if (ret) {
			LCDERR("DCCD get core_info[%d] fail\n", temp);
		} else {
			LCDPR("DCCD get tcon core_info[%d] size=%d (%#x), ptr=%p\n",
				temp, dccd_config.size, dccd_config.size, dccd_config.data.ptr);
		}
		break;
	case TCON_IOC_GET_CALC_CNT:
		temp = lcd_tcon_fw_core_table_cnt(tcon_fw);
		if (!copy_to_user(argp, &temp, sizeof(temp)))
			ret = 0;
		LCDPR("DCCD get cnt=%d\n", temp);
		break;
	case TCON_IOC_SET_DCCD_DONE:
		lcd_tcon_fw_update_core(tcon_fw);
		lcd_tcon_fw_remove_core_table(tcon_fw);
		lcd_resource_ready(pdrv->index, LCD_RES_TCON_DCCD, 0);
		ret = 0;
		LCDPR("DCCD set done\n");
		break;
	case TCON_IOC_SET_CTRL:
		if (!tcon_fw->cmd_handler) {
			ret = -EFAULT;
			break;
		}
		if (copy_from_user(&tcon_ctrl, argp, sizeof(struct aml_lcd_tcon_ctrl_s))) {
			ret = -EFAULT;
			break;
		}
		ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
		tcon_ctrl.ctrl_type = TCON_FW_CTRL_NONE; //clear sub_cmd to avoid misoperation
		break;
	case TCON_IOC_GET_CTRL:
		if (!tcon_fw->cmd_handler) {
			ret = -EFAULT;
			break;
		}
		ret = tcon_fw->cmd_handler(tcon_fw, FWCMD_TCON_CTRL, &tcon_ctrl);
		if (ret) {
			ret = -EFAULT;
			break;
		}
		if (copy_to_user(argp, &tcon_ctrl, sizeof(struct aml_lcd_tcon_ctrl_s)))
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int lcd_tcon_open(struct inode *inode, struct file *file)
{
	struct lcd_tcon_local_cfg_s *local_cfg;

	/* Get the per-device structure that contains this cdev */
	local_cfg = container_of(inode->i_cdev, struct lcd_tcon_local_cfg_s, cdev);
	file->private_data = local_cfg;
	return 0;
}

static int lcd_tcon_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long lcd_tcon_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct lcd_tcon_local_cfg_s *local_cfg = (struct lcd_tcon_local_cfg_s *)file->private_data;
	struct aml_lcd_drv_s *pdrv;
	void __user *argp;
	int mcd_nr = -1;
	int ret = 0;

	if (!local_cfg)
		return -EFAULT;
	pdrv = dev_get_drvdata(local_cfg->dev);
	if (!pdrv)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("tcon: %s: cmd_dir = 0x%x, cmd_nr = 0x%x\n",
			__func__, _IOC_DIR(cmd), mcd_nr);
	}

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case LCD_IOC_GET_TCON_BIN_MAX_CNT_INFO:
	case LCD_IOC_SET_TCON_DATA_INDEX_INFO:
	case LCD_IOC_GET_TCON_BIN_PATH_INFO:
	case LCD_IOC_SET_TCON_BIN_DATA_INFO:
	case TCON_IOC_SET_DCCD:
	case TCON_IOC_SET_QUICK_REG:
	case TCON_IOC_SET_CORE_BASE:
	case TCON_IOC_GET_DCCD_FLG:
	case TCON_IOC_GET_CALC_BUF:
	case TCON_IOC_GET_CALC_STATUS:
	case TCON_IOC_GET_CALC_CNT:
	case TCON_IOC_SET_DCCD_DONE:
		ret = lcd_tcon_ioctl_handler(pdrv, mcd_nr, arg);
		break;
	default:
		LCDERR("tcon: don't support ioctl cmd_nr: 0x%x\n", mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long lcd_tcon_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = lcd_tcon_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations lcd_tcon_fops = {
	.owner          = THIS_MODULE,
	.open           = lcd_tcon_open,
	.release        = lcd_tcon_release,
	.unlocked_ioctl = lcd_tcon_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = lcd_tcon_compat_ioctl,
#endif
};

/* **********************************
 * tcon debug api
 * **********************************
 */
void lcd_tcon_debug_file_add(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_local_cfg_s *local_cfg)
{
	int i, ret;

	if (!local_cfg)
		return;

	local_cfg->clsp = class_create(AML_TCON_CLASS_NAME);
	if (IS_ERR(local_cfg->clsp)) {
		LCDERR("tcon: failed to create class\n");
		return;
	}

	ret = alloc_chrdev_region(&local_cfg->devno, 0, 1, AML_TCON_DEVICE_NAME);
	if (ret < 0) {
		LCDERR("tcon: failed to alloc major number\n");
		goto err1;
	}

	local_cfg->dev = device_create(local_cfg->clsp, NULL,
		local_cfg->devno, (void *)pdrv, AML_TCON_DEVICE_NAME);
	if (IS_ERR(local_cfg->dev)) {
		LCDERR("tcon: failed to create device\n");
		goto err2;
	}

	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++) {
		if (device_create_file(local_cfg->dev, &lcd_tcon_debug_attrs[i])) {
			LCDERR("tcon: create attribute %s fail\n",
			       lcd_tcon_debug_attrs[i].attr.name);
			goto err3;
		}
	}

	/* connect the file operations with cdev */
	cdev_init(&local_cfg->cdev, &lcd_tcon_fops);
	local_cfg->cdev.owner = THIS_MODULE;

	/* connect the major/minor number to the cdev */
	ret = cdev_add(&local_cfg->cdev, local_cfg->devno, 1);
	if (ret) {
		LCDERR("tcon: failed to add device\n");
		goto err4;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon: %s: OK\n", __func__);
	return;

err4:
	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++)
		device_remove_file(local_cfg->dev, &lcd_tcon_debug_attrs[i]);
err3:
	device_destroy(local_cfg->clsp, local_cfg->devno);
	local_cfg->dev = NULL;
err2:
	unregister_chrdev_region(local_cfg->devno, 1);
err1:
	class_destroy(local_cfg->clsp);
	local_cfg->clsp = NULL;
	LCDERR("tcon: %s error\n", __func__);
}

void lcd_tcon_debug_file_remove(struct lcd_tcon_local_cfg_s *local_cfg)
{
	int i;

	if (!local_cfg)
		return;

	for (i = 0; i < ARRAY_SIZE(lcd_tcon_debug_attrs); i++)
		device_remove_file(local_cfg->dev, &lcd_tcon_debug_attrs[i]);

	cdev_del(&local_cfg->cdev);
	device_destroy(local_cfg->clsp, local_cfg->devno);
	local_cfg->dev = NULL;
	class_destroy(local_cfg->clsp);
	local_cfg->clsp = NULL;
	unregister_chrdev_region(local_cfg->devno, 1);
}
