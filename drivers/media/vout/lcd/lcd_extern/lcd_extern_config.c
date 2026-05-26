// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/amlogic/media/vout/lcd/lcd_extern.h>

#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "lcd_extern.h"

#ifdef CONFIG_OF
struct device_node *aml_lcd_extern_get_dts_child(struct lcd_extern_driver_s *edrv, int index)
{
	char propname[15];
	struct device_node *child;

	sprintf(propname, "extern_%d", index);
	child = of_get_child_by_name(edrv->sub_dev->of_node, propname);
	return child;
}

static int lcd_extern_init_table_handle_dts(struct lcd_extern_driver_s *edrv,
					    struct lcd_extern_dev_s *edev,
					    struct device_node *np)
{
	struct lcd_extern_config_s *extconf = &edev->config;
	int len_on, len_off, init_max, data_cnt;
	unsigned int *init_buf;
	unsigned char *table;
	int ret;

	len_on = of_property_count_u32_elems(np, "init_on");
	if (len_on < 0)
		len_on = 0;
	len_off = of_property_count_u32_elems(np, "init_off");
	if (len_off < 0)
		len_off = 0;
	init_max = len_on >= len_off ? len_on : len_off;
	if (init_max <= 0)
		return 0;

	init_buf = kcalloc(init_max, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;

	//init_on
	ret = of_property_read_u32_array(np, "init_on", init_buf, len_on);
	if (ret) {
		EXTERR("%s: get init_on failed\n", extconf->name);
		goto lcd_extern_init_table_handle_dts_err;
	}
	table = lcd_init_table_load_array("ext_init_on", extconf->cmd_size,
				init_buf, len_on, LCD_EXTERN_INIT_ON_MAX, &data_cnt);
	if (!table)
		goto lcd_extern_init_table_handle_dts_err;
	extconf->table_init_on = table;
	extconf->table_init_on_cnt = data_cnt;

	//init_off
	ret = of_property_read_u32_array(np, "init_off", init_buf, len_off);
	if (ret) {
		EXTERR("%s: get init_off failed\n", extconf->name);
		goto lcd_extern_init_table_handle_dts_err;
	}
	table = lcd_init_table_load_array("ext_ini_off", extconf->cmd_size,
				init_buf, len_off, LCD_EXTERN_INIT_OFF_MAX, &data_cnt);
	if (!table)
		goto lcd_extern_init_table_handle_dts_err;
	extconf->table_init_off = table;
	extconf->table_init_off_cnt = data_cnt;

	extconf->table_init_loaded = 1;

	kfree(init_buf);
	return 0;

lcd_extern_init_table_handle_dts_err:
	kfree(init_buf);
	return -1;
}

static int lcd_extern_get_config_dts(struct device_node *np,
				     struct lcd_extern_driver_s *edrv,
				     struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_config_s *econf;
	char snode[15];
	struct device_node *child;
	const char *str;
	unsigned int val;
	int ret = 0;

	sprintf(snode, "extern_%d", edev->dev_index);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		EXTPR("[%d]: %s: %s\n", edrv->index, __func__, snode);

	child = of_get_child_by_name(np, snode);
	if (!child) {
		EXTERR("[%d]: failed to get %s\n", edrv->index, snode);
		return -1;
	}

	econf = &edev->config;

	ret = of_property_read_u32(child, "index", &val);
	if (ret) {
		EXTERR("get index failed, exit\n");
		return -1;
	}
	econf->index = (unsigned char)val;

	ret = of_property_read_string(child, "status", &str);
	if (ret) {
		EXTERR("[%d]: get index %d status failed\n", edrv->index, econf->index);
		return -1;
	}
	if (strncmp(str, "okay", 2) == 0)
		econf->status = 1;

	ret = of_property_read_string(child, "extern_name", &str);
	if (ret) {
		EXTERR("[%d]: get extern_name failed\n", edrv->index);
		strscpy(econf->name, "none", LCD_EXTERN_NAME_LEN_MAX);
	} else {
		strscpy(econf->name, str, LCD_EXTERN_NAME_LEN_MAX);
	}

	ret = of_property_read_u32(child, "type", &econf->type);
	if (ret) {
		econf->type = LCD_EXTERN_MAX;
		EXTERR("[%d]: %s: get type failed, exit\n", edrv->index, econf->name);
		return -1;
	}

	if (econf->status == 0 || econf->index == LCD_EXTERN_INDEX_INVALID) {
		EXTERR("[%d]: %s[%d] status %d is disabled, or index %d is invalid\n",
		       edrv->index, econf->name, edev->dev_index, econf->status, econf->index);
		return -1;
	}
	EXTPR("[%d]: config from dts: %s[%d], index: %d, type: %d\n",
		edrv->index, econf->name, edev->dev_index, econf->index, econf->type);

	switch (econf->type) {
	case LCD_EXTERN_I2C:
		ret = of_property_read_u32(child, "i2c_address", &val);
		if (ret) {
			EXTERR("[%d]: %s: get i2c_address failed, exit\n",
			       edrv->index, econf->name);
			econf->i2c_addr = LCD_EXT_I2C_ADDR_INVALID;
			return -1;
		}
		econf->i2c_addr = (unsigned char)val;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr);
		}
		ret = of_property_read_u32(child, "i2c_address2", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address2 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr2 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr2 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address2 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr2);
		}
		ret = of_property_read_u32(child, "i2c_address3", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address3 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr3 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr3 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address3 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr3);
		}
		ret = of_property_read_u32(child, "i2c_address4", &val);
		if (ret) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				EXTPR("[%d]: %s: no i2c_address4 exist\n",
				      edrv->index, econf->name);
			}
			econf->i2c_addr4 = LCD_EXT_I2C_ADDR_INVALID;
		} else {
			econf->i2c_addr4 = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: i2c_address4 = 0x%02x\n",
			      edrv->index, econf->name, econf->i2c_addr4);
		}

		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size = %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size == 0)
			break;
		ret = lcd_extern_init_table_handle_dts(edrv, edev, child);
		break;
	case LCD_EXTERN_SPI:
		ret = of_property_read_u32(child, "gpio_spi_cs", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_cs failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_cs = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_cs = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_cs: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_cs);
		}
		ret = of_property_read_u32(child, "gpio_spi_clk", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_clk failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_clk = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_clk = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_clk: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_clk);
		}
		ret = of_property_read_u32(child, "gpio_spi_data", &val);
		if (ret) {
			EXTERR("[%d]: %s: get gpio_spi_data failed, exit\n",
			       edrv->index, econf->name);
			econf->spi_gpio_data = LCD_EXT_GPIO_INVALID;
			return -1;
		}
		econf->spi_gpio_data = val;
		lcd_extern_gpio_probe(edrv, val);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_gpio_data: %d\n",
			      edrv->index, econf->name, econf->spi_gpio_data);
		}
		ret = of_property_read_u32(child, "spi_clk_freq", &val);
		if (ret) {
			EXTERR("[%d]: %s: get spi_clk_freq failed, default to %dKHz\n",
			       edrv->index, econf->name, LCD_EXT_SPI_CLK_FREQ_DFT);
			econf->spi_clk_freq = LCD_EXT_SPI_CLK_FREQ_DFT;
		} else {
			econf->spi_clk_freq = val;
		}
		ret = of_property_read_u32(child, "spi_clk_pol", &val);
		if (ret) {
			EXTERR("[%d]: %s: get spi_clk_pol failed, default to 1\n",
			       edrv->index, econf->name);
			econf->spi_clk_pol = 1;
		} else {
			econf->spi_clk_pol = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: spi_clk_freq: %dKHz, spi_clk_pol: %d\n",
			      edrv->index, econf->name, econf->spi_clk_freq,
			      econf->spi_clk_pol);
		}
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size: %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size == 0)
			break;
		ret = lcd_extern_init_table_handle_dts(edrv, edev, child);
		break;
	case LCD_EXTERN_SIMPLE:
		ret = of_property_read_u32(child, "cmd_size", &val);
		if (ret) {
			EXTPR("[%d]: %s: no cmd_size\n", edrv->index, econf->name);
			econf->cmd_size = 0;
		} else {
			econf->cmd_size = (unsigned char)val;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: %s: cmd_size: %d\n",
			      edrv->index, econf->name, econf->cmd_size);
		}
		if (econf->cmd_size == 0)
			break;
		ret = lcd_extern_init_table_handle_dts(edrv, edev, child);
		break;
	default:
		break;
	}

	return ret;
}
#endif

/* config from json =============================================================================*/
static struct num_str_s ext_type_name[] = {
	{LCD_EXTERN_I2C,    "LCD_EXTERN_I2C"},
	{LCD_EXTERN_SPI,    "LCD_EXTERN_SPI"},
	{LCD_EXTERN_MAX, "LCD_EXTERN_MAX"},
};

static int lcd_extern_init_table_check(unsigned char *table, int len)
{
	int i = 0, type = 0, size = 0;

	for (i = 0; i < len; i += size) {
		type = table[i];
		size = table[i + 1] + 2;//type + size
		if (i + size > len)
			return -1;
		if (type == LCD_EXT_CMD_TYPE_END)
			return 0;
	}
	return -1;
}

static int lcd_extern_get_init_data_from_panel_param(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev)
{
	struct lcd_extern_config_s *cfg;
	int on_cnt = 2, off_cnt = 2;
	int size;
	unsigned char *p = NULL;
	char name[32];

	cfg = &edev->config;
	sprintf(name, "panel%d_ext%d_init_table", edrv->index, edev->dev_index);
	p = panel_param_mem_get(name, &size);
	if (!p)
		return -1;

	/* on_cnt|off_cnt|on_data|off_data */
	on_cnt = *(u32 *)(p + 0);
	off_cnt = *(u32 *)(p + 4);
	if (on_cnt + off_cnt + 8 > size) {
		EXTPR("%s init size fail\n", __func__);
		return -1;
	}

	cfg->table_init_on = kzalloc(on_cnt, GFP_KERNEL);
	cfg->table_init_off = kzalloc(off_cnt, GFP_KERNEL);
	if (cfg->table_init_on) {
		memcpy(cfg->table_init_on, p + 8, on_cnt);
		cfg->table_init_on_cnt = on_cnt;
		if (lcd_extern_init_table_check(cfg->table_init_on, on_cnt)) {
			kfree(cfg->table_init_on);
			cfg->table_init_on = NULL;
			cfg->table_init_on_cnt = 0;
			EXTPR("%s init_on check fail\n", __func__);
		}
	}
	if (cfg->table_init_off) {
		memcpy(cfg->table_init_off, p + 8 + on_cnt, off_cnt);
		cfg->table_init_off_cnt = off_cnt;
		if (lcd_extern_init_table_check(cfg->table_init_off, off_cnt)) {
			kfree(cfg->table_init_off);
			cfg->table_init_off = NULL;
			cfg->table_init_off_cnt = 0;
			EXTPR("%s init_off check fail\n", __func__);
		}
	}

	return 0;
}

static int lcd_extern_get_config_json(struct lcd_extern_driver_s *edrv,
				struct lcd_extern_dev_s *edev)
{
	struct json_s *parent, *child;
	const char *str = NULL;
	struct lcd_extern_config_s *cfg;
	struct json_parse_s *jsp = get_panel_jsp(edrv->index);

	if (!json_parse_ok(jsp)) {
		EXTERR("panel%d jsp not ok\n", edrv->index);
		return -1;
	}

	parent = json_path_to_node(jsp, jsp->root, "/lcd_ext_dev");
	if (!parent) {
		EXTERR("find /lcd_extern\n");
		return -1;
	}
	parent = json_get_array_child(jsp, parent, edev->dev_index);
	if (!parent) {
		EXTERR("find /lcd_ext_dev[%d]\n", edev->dev_index);
		return -1;
	}

	cfg = &edev->config;
	cfg->index = edev->dev_index;
	str = json_get_obj_str(jsp, parent, "name", "ext_default");
	strscpy(cfg->name, str ? str : "ext_default", LCD_EXTERN_NAME_LEN_MAX);
	str = json_get_obj_str(jsp, parent, "type", NULL);
	cfg->type = strnum_get_num(str, ext_type_name, ARRAY_SIZE(ext_type_name), LCD_EXTERN_MAX);
	cfg->status = json_get_obj_u32(jsp, parent, "status", 0);

	switch (cfg->type) {
	case LCD_EXTERN_I2C:
		child = json_get_object_child(jsp, parent, "i2c_addr");
		cfg->i2c_addr = json_get_arr_u32(jsp, child, 0, LCD_EXT_I2C_ADDR_INVALID);
		cfg->i2c_addr2 = json_get_arr_u32(jsp, child, 1, LCD_EXT_I2C_ADDR_INVALID);
		cfg->i2c_addr3 = json_get_arr_u32(jsp, child, 2, LCD_EXT_I2C_ADDR_INVALID);
		cfg->i2c_addr4 = json_get_arr_u32(jsp, child, 3, LCD_EXT_I2C_ADDR_INVALID);
		cfg->cmd_size = LCD_EXT_CMD_SIZE_DYNAMIC;
		if (lcd_debug_print_flag)
			EXTPR("i2c_addr=[%x, %x, %x, %x]\n", cfg->i2c_addr, cfg->i2c_addr2,
			      cfg->i2c_addr3, cfg->i2c_addr4);
		break;
	case LCD_EXTERN_SPI:
		cfg->spi_gpio_cs    = json_get_obj_u32(jsp, parent, "gpio_cs_id", 0);
		lcd_extern_gpio_probe(edrv, cfg->spi_gpio_cs);
		cfg->spi_gpio_clk   = json_get_obj_u32(jsp, parent, "gpio_clk_id", 0);
		lcd_extern_gpio_probe(edrv, cfg->spi_gpio_clk);
		cfg->spi_gpio_data  = json_get_obj_u32(jsp, parent, "gpio_data_id", 0);
		lcd_extern_gpio_probe(edrv, cfg->spi_gpio_data);
		cfg->spi_clk_pol    = json_get_obj_u32(jsp, parent, "clk_pol", 0);
		cfg->spi_clk_freq   = json_get_obj_u32(jsp, parent, "clk_freq", 0);
		//cfg->spi_delay_us   = json_get_obj_u32(jsp, parent, "interval", 10);
		if (lcd_debug_print_flag)
			EXTPR("spi cs=%d, clk=%d data=%d, pol=%d, freq=%d\n",
			      cfg->spi_gpio_cs, cfg->spi_gpio_clk, cfg->spi_gpio_data,
			      cfg->spi_clk_pol, cfg->spi_clk_freq);
		break;
	default:
		EXTERR("invalid type\n");
		return -1;
	}

/* init data*/
	lcd_extern_get_init_data_from_panel_param(edrv, edev);
	cfg->cmd_size = LCD_EXT_CMD_SIZE_DYNAMIC;
	cfg->table_init_loaded = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		EXTPR("init_on:(cnt=%d)\n", cfg->table_init_on_cnt);
		lcd_dbg_mem_dump(cfg->table_init_on, cfg->table_init_on_cnt);

		EXTPR("init off:(cnt=%d)\n", cfg->table_init_off_cnt);
		lcd_dbg_mem_dump(cfg->table_init_off, cfg->table_init_off_cnt);
	}

	return 0;
}

static int lcd_extern_get_config_ini(struct lcd_extern_driver_s *edrv,
				     struct lcd_extern_dev_s *edev)
{
	void *inip, *psec;
	const char *str;
	unsigned int val, *init_buf = NULL;
	unsigned char *init_data;
	int init_len, data_cnt;
	int init_cmd_valid = 1;
	char str_info[64] = {'\0'};
	int str_info_len = 0;

	inip = get_lcd_ini_parse_mem(edrv->index);
	if (!inip) {
		EXTERR("[%d]: %s: parse_mem not ready\n", edrv->index, __func__);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "lcd_ext_Attr");
	if (!psec) {
		EXTERR("[%d]: %s: not find lcd_ext_Attr\n", edrv->index, __func__);
		return -1;
	}

	str = lcd_ini_get_str(inip, psec, "ext_name", "null");
	strscpy(edev->config.name, str, LCD_EXTERN_NAME_LEN_MAX);

	edev->config.index = lcd_ini_get_val(inip, psec, "ext_index", 0xff);

	str = lcd_ini_get_str(inip, psec, "ext_type", "null");
	edev->config.type = strnum_get_num(str, ext_type_name, ARRAY_SIZE(ext_type_name),
					   LCD_EXTERN_MAX);

	edev->config.status = lcd_ini_get_val(inip, psec, "ext_status", 0);
	if (edev->config.status == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			EXTPR("[%d]: dev[%d]: %s(%d) is disabled\n",
				edrv->index, edev->dev_index,
				edev->config.name, edev->config.index);
		}
		return -1;
	}

	switch (edev->config.type) {
	case LCD_EXTERN_I2C:
		edev->config.i2c_addr =
			lcd_ini_get_val(inip, psec, "value_0", LCD_EXT_I2C_ADDR_INVALID);
		edev->config.i2c_addr2 =
			lcd_ini_get_val(inip, psec, "value_1", LCD_EXT_I2C_ADDR_INVALID);
		edev->config.i2c_addr3 =
			lcd_ini_get_val(inip, psec, "value_4", LCD_EXT_I2C_ADDR_INVALID);
		edev->config.i2c_addr4 =
			lcd_ini_get_val(inip, psec, "value_5", LCD_EXT_I2C_ADDR_INVALID);
		edev->config.cmd_size = lcd_ini_get_val(inip, psec, "value_3", 0);

		str_info_len += sprintf(str_info + str_info_len, "i2c_addr=[%x, %x, %x, %x]",
					edev->config.i2c_addr, edev->config.i2c_addr2,
					edev->config.i2c_addr3, edev->config.i2c_addr4);
		if (edev->config.cmd_size == 0)
			init_cmd_valid = 0;
		break;
	case LCD_EXTERN_SPI:
		edev->config.spi_gpio_cs    = lcd_ini_get_val(inip, psec, "value_0", 0xff);
		lcd_extern_gpio_probe(edrv, edev->config.spi_gpio_cs);
		edev->config.spi_gpio_clk   = lcd_ini_get_val(inip, psec, "value_1", 0xff);
		lcd_extern_gpio_probe(edrv, edev->config.spi_gpio_clk);
		edev->config.spi_gpio_data  = lcd_ini_get_val(inip, psec, "value_2", 0xff);
		lcd_extern_gpio_probe(edrv, edev->config.spi_gpio_data);
		val = lcd_ini_get_val(inip, psec, "value_4", 0);
		edev->config.spi_clk_freq =
			(val << 8 | (lcd_ini_get_val(inip, psec, "value_3", 0)));
		edev->config.spi_clk_pol    = lcd_ini_get_val(inip, psec, "value_5", 1);
		edev->config.cmd_size = lcd_ini_get_val(inip, psec, "value_6", 0);

		str_info_len += sprintf(str_info + str_info_len, "spi clk_freq=%d, clk_pol=%d",
					edev->config.spi_clk_freq, edev->config.spi_clk_pol);
		if (edev->config.cmd_size == 0)
			init_cmd_valid = 0;
		break;
	case LCD_EXTERN_SIMPLE:
		edev->config.cmd_size = lcd_ini_get_val(inip, psec, "value_9", 0);
		if (edev->config.cmd_size == 0)
			init_cmd_valid = 0;
		break;
	default:
		EXTERR("invalid type\n");
		return -1;
	}

	EXTPR("[%d]: load ini config: dev[%d]: %s(%d), type: %d, cmd_size: %d, %s\n",
		edrv->index, edev->dev_index, edev->config.name, edev->config.index,
		edev->config.type, edev->config.cmd_size, str_info);

	edev->config.table_init_loaded = 0;
	edev->config.table_init_on_cnt = 0;
	edev->config.table_init_on = NULL;
	edev->config.table_init_off_cnt = 0;
	edev->config.table_init_off = NULL;
	if (init_cmd_valid == 0)
		return 0;

	if (lcd_extern_get_init_data_from_panel_param(edrv, edev) == 0) {
		edev->config.table_init_loaded = 1;
		return 0;
	}

	init_len = lcd_ini_get_array_cnt(inip, psec, "init_on");
	if (init_len < 0) {
		EXTPR("[%d]: dev[%d] not find init_on\n", edrv->index, edev->dev_index);
		return 0;
	}
	init_buf = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;
	data_cnt = lcd_ini_get_array(inip, psec, "init_on", init_buf, init_len);
	init_data = lcd_init_table_load_array("ext_init_on", edev->config.cmd_size,
						      init_buf, data_cnt,
						      LCD_EXTERN_INIT_ON_MAX, &init_len);
	if (!init_data) {
		EXTERR("[%d]: dev[%d]: init_on err\n", edrv->index, edev->dev_index);
		goto lcd_extern_get_config_ini_init_err;
	}
	edev->config.table_init_on_cnt = init_len;
	edev->config.table_init_on = init_data;
	kfree(init_buf);

	init_len = lcd_ini_get_array_cnt(inip, psec, "init_off");
	if (init_len < 0) {
		EXTPR("[%d]: dev[%d] not find init_off\n", edrv->index, edev->dev_index);
		return -1;
	}
	init_buf = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;
	data_cnt = lcd_ini_get_array(inip, psec, "init_off", init_buf, init_len);
	init_data = lcd_init_table_load_array("ext_init_off", edev->config.cmd_size,
					      init_buf, data_cnt,
					      LCD_EXTERN_INIT_OFF_MAX, &init_len);
	if (!init_data) {
		EXTERR("[%d]: dev[%d]: init_off err\n", edrv->index, edev->dev_index);
		goto lcd_extern_get_config_ini_init_err;
	}
	edev->config.table_init_off_cnt = init_len;
	edev->config.table_init_off = init_data;
	kfree(init_buf);

	edev->config.table_init_loaded = 1;

	return 0;

lcd_extern_get_config_ini_init_err:
	kfree(init_buf);
	return -1;
}

int lcd_ext_check_config_load(struct lcd_extern_driver_s *drv)
{
	drv->config_load = lcd_panel_config_load_detect(drv->index, __func__);
	if (drv->config_load == LCD_CONFIG_NONE || drv->config_load == LCD_CONFIG_ERR)
		return -1;

	return 0;
}

static int lcd_extern_dev_load(struct lcd_extern_driver_s *edrv)
{
	unsigned char file_type = PANEL_FILE_INVALID;
	int dev_index, i;
	int ret = 0;

	for (i = 0; i < lcd_ext_dev_cnt[edrv->index]; i++) {
		dev_index = lcd_ext_index_lut[edrv->index][i];
		edrv->dev[edrv->dev_cnt] = lcd_extern_dev_malloc(dev_index);
		switch (edrv->config_load) {
		case LCD_CONFIG_DTS:
			if (edrv->pdev->dev.of_node) {
				ret = lcd_extern_get_config_dts(edrv->pdev->dev.of_node,
								edrv, edrv->dev[edrv->dev_cnt]);
			} else {
				ret = -1;
			}
			break;
		case LCD_CONFIG_FILE:
			file_type = get_lcd_panel_file_type(edrv->index);
			if (file_type == PANEL_FILE_JSON)
				ret = lcd_extern_get_config_json(edrv, edrv->dev[edrv->dev_cnt]);
			else if (file_type == PANEL_FILE_INI)
				ret = lcd_extern_get_config_ini(edrv, edrv->dev[edrv->dev_cnt]);
			else
				ret = -1;
			break;
		default:
			ret = -1;
			break;
		}
		if (ret) {
			lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
			edrv->dev[edrv->dev_cnt] = NULL;
			lcd_resource_ready(edrv->index, LCD_RES_EXTERN, dev_index);
			continue;
		}
		lcd_extern_config_update(edrv, edrv->dev[edrv->dev_cnt]);
		ret = lcd_extern_add_dev(edrv, edrv->dev[edrv->dev_cnt]);
		if (ret) {
			lcd_extern_dev_free(edrv->dev[edrv->dev_cnt]);
			edrv->dev[edrv->dev_cnt] = NULL;
			continue;
		}

		edrv->dev_cnt++;
		lcd_resource_ready(edrv->index, LCD_RES_EXTERN, dev_index);
	}

	return ret;
}

int lcd_extern_config_load(struct lcd_extern_driver_s *edrv)
{
	struct device_node *np;
	unsigned int para[5];
	const char *str;
	int ret;

	if (!edrv->pdev->dev.of_node) {
		EXTERR("[%d]: %s: dev of_node is null\n", edrv->index, __func__);
		return -1;
	}
	np = edrv->pdev->dev.of_node;

	ret = of_property_read_string(np, "i2c_bus", &str);
	if (ret)
		edrv->i2c_bus = LCD_EXT_I2C_BUS_MAX;
	else
		edrv->i2c_bus = aml_lcd_i2c_bus_get_str(str);

	ret = of_property_read_string(np, "pinctrl-names", &str);
	if (ret)
		edrv->pinmux_valid = 0;
	else
		edrv->pinmux_valid = 1;
	edrv->pinmux_flag = 0xff;

	ret = of_property_read_u32(np, "key_valid", &para[0]);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			EXTPR("failed to get key_valid\n");
		edrv->key_valid = 0;
	} else {
		edrv->key_valid = (unsigned char)para[0];
	}

	if (edrv->index == 0)
		sprintf(edrv->ukey_name, "lcd_extern");
	else
		sprintf(edrv->ukey_name, "lcd%d_extern", edrv->index);

	if (lcd_ext_check_config_load(edrv))
		return -1;

	ret = lcd_extern_dev_load(edrv);
	return ret;

	return 0;
}
