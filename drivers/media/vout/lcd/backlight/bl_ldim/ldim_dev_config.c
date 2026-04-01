// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>

#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include <linux/amlogic/aml_spi.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"
#include "../lcd_bl.h"
#include "ldim_reg.h"

static int ldim_dev_init_table_handle_dts(struct ldim_dev_driver_s *dev_drv,
					  struct device_node *child)
{
	int len_on, len_off, init_len;
	unsigned int *init_buf;
	unsigned char *table;
	int ret;

	len_on = of_property_count_u32_elems(child, "init_on");
	if (len_on < 0)
		len_on = 0;
	len_off = of_property_count_u32_elems(child, "init_off");
	if (len_off < 0)
		len_off = 0;

	init_len = len_on >= len_off ? len_on : len_off;
	if (init_len == 0)
		return 0;

	init_buf = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;

	//init_on
	ret = of_property_read_u32_array(child, "init_on", init_buf, len_on);
	if (ret) {
		LDIMERR("%s: get init_on failed\n", dev_drv->name);
		goto ldim_dev_init_table_handle_dts_err;
	}
	table = lcd_init_table_load_array("ldim_dev_init_on", LCD_EXT_CMD_SIZE_DYNAMIC,
				init_buf, len_on, LDIM_INIT_ON_MAX, &init_len);
	if (!table)
		goto ldim_dev_init_table_handle_dts_err;
	dev_drv->init_on = table;
	dev_drv->init_on_cnt = init_len;

	//init_off
	ret = of_property_read_u32_array(child, "init_off", init_buf, len_off);
	if (ret) {
		LDIMERR("%s: get init_off failed\n", dev_drv->name);
		goto ldim_dev_init_table_handle_dts_err;
	}
	table = lcd_init_table_load_array("ldim_dev_init_off", LCD_EXT_CMD_SIZE_DYNAMIC,
				init_buf, len_off, LDIM_INIT_OFF_MAX, &init_len);
	if (!table)
		goto ldim_dev_init_table_handle_dts_err;
	dev_drv->init_off = table;
	dev_drv->init_off_cnt = init_len;

	dev_drv->init_loaded = 1;

	kfree(init_buf);
	return 0;

ldim_dev_init_table_handle_dts_err:
	kfree(init_buf);
	return -1;
}

static int ldim_dev_get_config_from_dts(struct ldim_dev_driver_s *dev_drv,
					struct device_node *np, int index, phandle pwm_phandle)
{
	char propname[20];
	struct device_node *child;
	const char *str;
	unsigned int *temp, val, size;
	struct bl_pwm_config_s *bl_pwm;
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();
	char dbg_str[256];
	int i, dbg_str_len = 0, ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_LDIM)
		LDIMPR("load ldim_dev config from dts\n");

	size = (dev_drv->zone_num >= 10) ? dev_drv->zone_num : 10;
	temp = kcalloc(size, sizeof(unsigned int), GFP_KERNEL);
	if (!temp)
		return -1;

	/* get device config */
	sprintf(propname, "ldim_dev_%d", index);
	LDIMPR("load: %s\n", propname);
	child = of_get_child_by_name(np, propname);
	if (!child) {
		LDIMERR("failed to get %s\n", propname);
		goto ldim_get_config_err;
	}

	ret = of_property_read_string(child, "ldim_dev_name", &str);
	if (ret) {
		LDIMERR("failed to get ldim_dev_name\n");
		str = "ldim_dev";
	}
	strscpy(dev_drv->name, str, LDIM_DEV_NAME_MAX);

	ret = of_property_read_u32(child, "type", &val);
	if (ret) {
		LDIMERR("failed to get type\n");
		dev_drv->type = LDIM_DEV_TYPE_NORMAL;
	} else {
		dev_drv->type = val;
	}
	if (dev_drv->type >= LDIM_DEV_TYPE_MAX) {
		LDIMERR("type num is out of support\n");
		goto ldim_get_config_err;
	}

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		/* get spi config */
		ret = of_property_read_u32(child, "spi_bus_num", &val);
		if (ret) {
			LDIMERR("failed to get spi_bus_num\n");
		} else {
			dev_drv->spi_conf.spi_info[0].bus_num = val & 0xf;
			dev_drv->spi_conf.spi_info[1].bus_num = (val >> 4) & 0xf;
			if (dev_drv->spi_conf.spi_info[1].bus_num)
				dev_drv->spi_conf.spi_dev_num = 2;
			if (ldim_debug_print) {
				LDIMPR("spi bus_num: %d : %d, spi_dev_num:%d\n",
					dev_drv->spi_conf.spi_info[0].bus_num,
					dev_drv->spi_conf.spi_info[1].bus_num,
					dev_drv->spi_conf.spi_dev_num);
			}
		}

		ret = of_property_read_u32(child, "spi_chip_select", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			dev_drv->spi_conf.spi_info[0].chip_select = val & 0xf;
			dev_drv->spi_conf.spi_info[1].chip_select = (val >> 4) & 0xf;
			if (ldim_debug_print) {
				LDIMPR("spi chip_select: %d : %d\n",
				dev_drv->spi_conf.spi_info[0].chip_select,
				dev_drv->spi_conf.spi_info[1].chip_select);
			}
		}

		ret = of_property_read_u32(child, "spi_max_frequency", &val);
		if (ret) {
			LDIMERR("failed to get spi_chip_select\n");
		} else {
			dev_drv->spi_conf.spi_info[0].max_speed_hz = val;
			dev_drv->spi_conf.spi_info[1].max_speed_hz = val;
			if (ldim_debug_print) {
				LDIMPR("spi max_speed_hz: %d\n",
				       dev_drv->spi_conf.spi_info[0].max_speed_hz);
			}
		}

		ret = of_property_read_u32(child, "spi_mode", &val);
		if (ret) {
			LDIMERR("failed to get spi_mode\n");
		} else {
			dev_drv->spi_conf.spi_info[0].mode = val;
			dev_drv->spi_conf.spi_info[1].mode = val;
			if (ldim_debug_print)
				LDIMPR("spi mode: %d\n", dev_drv->spi_conf.spi_info[0].mode);
		}

		ret = of_property_read_u32_array(child, "spi_cs_delay", &temp[0], 2);
		if (ret) {
			dev_drv->spi_conf.cs2clk_delay = 0;
			dev_drv->spi_conf.clk2cs_delay = 0;
		} else {
			dev_drv->spi_conf.cs2clk_delay = temp[0];
			dev_drv->spi_conf.clk2cs_delay = temp[1];
		}
		break;
	default:
		break;
	}

	/* ldim pwm config */
	bl_pwm = &dev_drv->ldim_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	ret = of_property_read_string(child, "ldim_pwm_port", &str);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_port\n");
	} else {
		bl_pwm->pwm_port = bl_pwm_str_to_num(str);
		LDIMPR("ldim_pwm_port: %s(0x%x)\n", str, bl_pwm->pwm_port);
	}
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		ret = of_property_read_u32_array(child, "ldim_pwm_attr",
						 temp, 3);
		if (ret) {
			LDIMERR("failed to get ldim_pwm_attr\n");
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = 1;
			else
				bl_pwm->pwm_freq = 60;
			bl_pwm->pwm_duty = 50;
			bl_pwm->pwm_phase = 0;
		} else {
			bl_pwm->pwm_method = temp[0];
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				bl_pwm->pwm_freq = temp[1] & 0xff;
				bl_pwm->pwm_phase = (temp[1] >> 8) & 0xffffff;
			} else {
				bl_pwm->pwm_freq = temp[1];
				bl_pwm->pwm_phase = 0;
			}
			bl_pwm->pwm_duty = temp[2];
		}
		LDIMPR("get ldim_pwm pol = %d, freq = %d, phase = %d, dft duty = %d%%\n",
		       bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_duty);

		bl_pwm_config_init(bl_pwm);

		if (bl_pwm->pwm_port < BL_PWM_VS) {
			bl_pwm_channel_register(dev_drv->dev,
						pwm_phandle, bl_pwm);
		}
	}

	/* analog pwm config */
	bl_pwm = &dev_drv->analog_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	ret = of_property_read_string(child, "analog_pwm_port", &str);
	if (ret)
		bl_pwm->pwm_port = BL_PWM_MAX;
	else
		bl_pwm->pwm_port = bl_pwm_str_to_num(str);
	if (bl_pwm->pwm_port < BL_PWM_VS) {
		LDIMPR("find analog_pwm_port: %s(%u)\n", str, bl_pwm->pwm_port);
		ret = of_property_read_u32_array(child, "analog_pwm_attr",
						 temp, 5);
		if (ret) {
			LDIMERR("failed to get analog_pwm_attr\n");
		} else {
			bl_pwm->pwm_method = temp[0];
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				bl_pwm->pwm_freq = temp[1] & 0xff;
				bl_pwm->pwm_phase = (temp[1] >> 8) & 0xffffff;
			} else {
				bl_pwm->pwm_freq = temp[1];
				bl_pwm->pwm_phase = 0;
			}
			bl_pwm->pwm_duty_max = temp[2];
			bl_pwm->pwm_duty_min = temp[3];
			bl_pwm->pwm_duty = temp[4];
		}
		LDIMPR("get analog_pwm pol = %d, freq = %d, phase = %d\n",
			bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_phase);
		LDIMPR("duty max = %d, min = %d, default = %d\n",
			bl_pwm->pwm_duty_max,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty);

		bl_pwm_config_init(bl_pwm);
		bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
	}

	ret = of_property_read_string(child, "ldim_pwm_pinmux_sel", &str);
	if (ret)
		strscpy(dev_drv->pinmux_name, "invalid", LDIM_DEV_NAME_MAX);
	else
		strscpy(dev_drv->pinmux_name, str, LDIM_DEV_NAME_MAX);

	ret = of_property_read_u32_array(child, "en_gpio_on_off", temp, 3);
	if (ret) {
		LDIMERR("failed to get en_gpio_on_off\n");
		dev_drv->en_gpio = BL_GPIO_MAX;
		dev_drv->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		dev_drv->en_gpio_off = BL_GPIO_OUTPUT_LOW;
	} else {
		if (temp[0] >= BL_GPIO_NUM_MAX) {
			dev_drv->en_gpio = BL_GPIO_MAX;
		} else {
			dev_drv->en_gpio = temp[0];
			ldim_gpio_probe(dev_drv, dev_drv->en_gpio);
		}
		dev_drv->en_gpio_on = temp[1];
		dev_drv->en_gpio_off = temp[2];
	}

	ret = of_property_read_u32(child, "lamp_err_gpio", &val);
	if (ret) {
		dev_drv->lamp_err_gpio = BL_GPIO_MAX;
		dev_drv->fault_check = 0;
	} else {
		if (val >= BL_GPIO_NUM_MAX) {
			dev_drv->lamp_err_gpio = BL_GPIO_MAX;
			dev_drv->fault_check = 0;
		} else {
			dev_drv->lamp_err_gpio = val;
			dev_drv->fault_check = 1;
			ldim_gpio_probe(dev_drv, dev_drv->lamp_err_gpio);
			ldim_gpio_set(dev_drv, dev_drv->lamp_err_gpio,
				      BL_GPIO_INPUT);
		}
	}

	ret = of_property_read_u32(child, "spi_write_check", &val);
	if (ret)
		dev_drv->write_check = 0;
	else
		dev_drv->write_check = (unsigned char)val;

	ret = of_property_read_u32_array(child, "dim_max_min", &temp[0], 2);
	if (ret) {
		LDIMERR("failed to get dim_max_min\n");
		dev_drv->dim_max = 0xfff;
		dev_drv->dim_min = 0x7f;
	} else {
		dev_drv->dim_max = temp[0];
		dev_drv->dim_min = temp[1];
	}

	ret = of_property_read_u32(child, "chip_count", &val);
	if (ret)
		dev_drv->chip_cnt = 1;
	else
		dev_drv->chip_cnt = val;

	ret = of_property_read_u32(child, "vs2cs_delay", &val);
	if (ret) {
		dev_drv->spi_conf.trig_mode = SPI_ASYNC;
		dev_drv->spi_conf.vs2cs_delay = 0;
		dev_drv->spi_conf.use_ctrl_cs = 0;
	} else {
		dev_drv->spi_conf.trig_mode = SPI_TRIG_VSYNC;
		dev_drv->spi_conf.vs2cs_delay = (unsigned int)val;
		dev_drv->spi_conf.use_ctrl_cs = 1;
	}

	dbg_str_len = sprintf(dbg_str + dbg_str_len, "trig_mode:%d, vs2cs_delay: %d,",
		dev_drv->spi_conf.trig_mode, dev_drv->spi_conf.vs2cs_delay);
	sprintf(dbg_str + dbg_str_len, "chip_cnt:%d, cus pwm_pinmux_sel:%s",
		dev_drv->chip_cnt, dev_drv->pinmux_name);
	LDIMPR("load dts config: %s: type:%d, %s\n",
		dev_drv->name, dev_drv->type, dbg_str);

	/*boost_conf*/
	ret = of_property_read_u32_array(child, "boost_conf", temp, 7);
	if (ret) {
		LDIMERR("failed to get boost_conf\n");
		dev_drv->boost_conf.en = 0;
		dev_drv->boost_conf.mode = 0;
		dev_drv->boost_conf.i_l100 = 0;
		dev_drv->boost_conf.i_l32 = 0;
		dev_drv->boost_conf.i_l100_val = 0;
		dev_drv->boost_conf.i_l32_val = 0;
		dev_drv->boost_conf.kp_l100 = 0;
		dev_drv->boost_conf.kp_l32 = 0;
	} else {
		dev_drv->boost_conf.en = (unsigned char)temp[0];
		dev_drv->boost_conf.mode = (unsigned char)temp[1];
		dev_drv->boost_conf.i_l100 = (unsigned short)temp[3];
		dev_drv->boost_conf.i_l32 = (unsigned short)temp[4];
		dev_drv->boost_conf.i_l100_val = (unsigned short)temp[5];
		dev_drv->boost_conf.i_l32_val = (unsigned short)temp[6];
		dev_drv->boost_conf.kp_l100 = (unsigned char)temp[7];
		dev_drv->boost_conf.kp_l32 = (unsigned char)temp[8];
	}

	dbg_str_len = sprintf(dbg_str, "i_l100:%d, i_l32:%d, i_l100_val:%d, i_l32_val:%d, ",
		dev_drv->boost_conf.i_l100, dev_drv->boost_conf.i_l32,
		dev_drv->boost_conf.i_l100_val, dev_drv->boost_conf.i_l32_val);
	sprintf(dbg_str + dbg_str_len, "kp_l100:%d, kp_l32:%d",
		dev_drv->boost_conf.kp_l100, dev_drv->boost_conf.kp_l32);
	LDIMPR("boost_conf: en:%d, mode:%d, %s\n",
		dev_drv->boost_conf.en, dev_drv->boost_conf.mode, dbg_str);

	ret = of_property_read_string(child, "ldim_zone_mapping_path", &str);
	if (ret == 0) {
		LDIMPR("find custom ldim_zone_mapping\n");
		strscpy(dev_drv->bl_mapping_path, str, sizeof(dev_drv->bl_mapping_path));
		goto ldim_dev_get_config_from_dts_profile;
	}
	ret = of_property_read_u32_array(child, "ldim_zone_mapping",
					 &temp[0], dev_drv->zone_num);
	if (ret) {
		ret = of_property_read_u32_array(child, "ldim_region_mapping",
						 &temp[0], dev_drv->zone_num);
		if (ret) {
			for (i = 0; i < dev_drv->zone_num; i++)
				dev_drv->bl_mapping[i] = (unsigned short)i;
			goto ldim_dev_get_config_from_dts_profile;
		}
	}
	LDIMPR("find custom ldim_zone_mapping\n");
	for (i = 0; i < dev_drv->zone_num; i++)
		dev_drv->bl_mapping[i] = (unsigned short)temp[i];

ldim_dev_get_config_from_dts_profile:
	ret = of_property_read_string(child, "ldim_bl_profile_path", &str);
	if (ret) {
		LDIMERR("failed to get ldim_bl_profile_path\n");
		strcpy(dev_drv->profile_path, "null");
	} else {
		strscpy(dev_drv->profile_path, str, sizeof(dev_drv->profile_path));
	}

	/* get cus_param , maximum = 32 x 4 bytes */
	if (fw_cus && fw_cus->fw_param) {
		fw_cus->fw_param->param_len = 32;
		fw_cus->fw_param->param = kcalloc(32, sizeof(int), GFP_KERNEL);
		if (fw_cus->fw_param->param) {
			for (i = 0; i < 32; i++) {
				ret = of_property_read_u32_index(child,
					"ldim_cus_param", i, &val);
				if (ret) {
					LDIMPR("get ldim_cus_param[%d] failed, size =%d\n",
						i, i);
					i = 32;
				} else {
					fw_cus->fw_param->param[i] = val;
					LDIMPR("param[%d] = %d\n",
						i, fw_cus->fw_param->param[i]);
				}
			}
		}
	}

	/* get init_cmd */
	ret = of_property_read_u32(child, "cmd_size", &val);
	if (ret) {
		LDIMPR("no cmd_size\n");
		dev_drv->cmd_size = 0;
	} else {
		dev_drv->cmd_size = (unsigned char)val;
	}
	if (ldim_debug_print)
		LDIMPR("%s: cmd_size = %d\n", dev_drv->name, dev_drv->cmd_size);
	if (dev_drv->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
		goto ldim_dev_get_config_from_dts_end;

	ret = ldim_dev_init_table_handle_dts(dev_drv, child);
	if (ret)
		goto ldim_get_config_err;

ldim_dev_get_config_from_dts_end:
	kfree(temp);
	return 0;

ldim_get_config_err:
	kfree(temp);
	return -1;
}

/* json ==========================================================================================*/
struct num_str_s ldim_dev_type_match[] = {
	{LDIM_DEV_TYPE_NORMAL, "NORMAL"},
	{LDIM_DEV_TYPE_SPI, "SPI"},
	{LDIM_DEV_TYPE_I2C, "I2C"},
	{LDIM_DEV_TYPE_ABCON, "ABCON"},
	{LDIM_DEV_TYPE_MAX, "MAX"}
};

static int ldim_gpio_name_to_index(struct ldim_dev_driver_s *drv, const char *name)
{
	int i = 0;

	if (!drv || !name)
		return BL_GPIO_MAX;

	for (i = 0; i < BL_GPIO_NUM_MAX; i++)
		if (!strcmp(ldim_gpio[i].name, name))
			return i;
	return BL_GPIO_MAX;
}

static int ldim_dev_get_config_from_json(struct ldim_dev_driver_s *dev_drv, phandle pwm_phandle)
{
	struct json_parse_s *jsp = get_panel_jsp(0);
	struct json_s *parent, *child, *child2, *child3;
	int cnt = 0, i = 0, j = 0, cnt_max, data_cnt;
	const char *str = NULL;
	//struct spi_board_info *spi_info;
	struct bl_pwm_config_s *bl_pwm, *pwms[3];
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();
	unsigned int *nums = NULL;
	unsigned char *table;
	struct abcon_conf_s *abcon_conf;
	struct spi_conf_s *spi_conf;
	struct data_pkt_s *data_pkt;

	if (!json_parse_ok(jsp)) {
		LDIMERR("panel 0 json not ready\n");
		return -1;
	}

	parent = json_path_to_node(jsp, jsp->root, "backlight/ldim_dev");
	if (!parent) {
		LDIMERR("failed find /backlight/ldim_dev\n");
		return -1;
	}

//basic_info
	child = json_get_object_child(jsp, parent, "basic_info");
	if (!child) {
		LDIMERR("fail to get basic_info\n");
		return -1;
	}

	str = json_get_obj_str(jsp, child, "name", "null");
	strscpy(dev_drv->name, str, LDIM_DEV_NAME_MAX);
	dev_drv->index    = 0;
	dev_drv->chip_cnt = json_get_obj_u32(jsp, child, "chip_count", 1);
	dev_drv->dim_min  = json_get_obj_u32(jsp, child, "dim_min", 0);
	dev_drv->dim_max  = json_get_obj_u32(jsp, child, "dim_max", 4095);
	dev_drv->brightness_mode = json_get_obj_u32(jsp, child, "brightness_mode", 0x0);

//interface
	child = json_get_object_child(jsp, parent, "interface");
	if (!child) {
		LDIMERR("fail to get interface\n");
		return -1;
	}

	str = json_get_obj_str(jsp, child, "type", NULL);
	dev_drv->type = strnum_get_num(str, ldim_dev_type_match,
				       ARRAY_SIZE(ldim_dev_type_match), LDIM_DEV_TYPE_MAX);
	if (dev_drv->type == LDIM_DEV_TYPE_MAX) {
		LDIMERR("invalid type:%d\n", dev_drv->type);
		return -1;
	}

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		spi_conf = &dev_drv->spi_conf;
		spi_conf->spi_dev_num = json_get_obj_u32(jsp, child, "spi_dev_num", 1);
		child2 = json_get_object_child(jsp, child, "bus_number");
		if (child2) {
			for (i = 0; i < spi_conf->spi_dev_num; i++)
				spi_conf->spi_info[i].bus_num = json_get_arr_u32(jsp, child2, i, 0);
		}
		child2 = json_get_object_child(jsp, child, "chip_select");
		if (child2) {
			for (i = 0; i < spi_conf->spi_dev_num; i++)
				spi_conf->spi_info[i].chip_select =
					json_get_arr_u32(jsp, child2, i, 0);
		}
		spi_conf->spi_info[0].max_speed_hz =
			json_get_obj_u32(jsp, child, "max_freq", 3000000);
		spi_conf->spi_info[1].max_speed_hz = spi_conf->spi_info[0].max_speed_hz;
		spi_conf->spi_info[2].max_speed_hz = spi_conf->spi_info[0].max_speed_hz;
		spi_conf->spi_info[0].mode = json_get_obj_u32(jsp, child, "spi_mode", 0);
		spi_conf->spi_info[1].mode = spi_conf->spi_info[0].mode;
		spi_conf->spi_info[2].mode = spi_conf->spi_info[0].mode;
		spi_conf->cs2clk_delay = json_get_obj_u32(jsp, child, "cs2clk_delay", 0);
		spi_conf->clk2cs_delay = json_get_obj_u32(jsp, child, "clk2cs_delay", 0);
		spi_conf->vs2cs_delay = json_get_obj_u32(jsp, child, "vs2cs_delay", 0);
		spi_conf->trig_mode = json_get_obj_u32(jsp, child, "trig_mode", 0);
		spi_conf->rx_en = json_get_obj_u32(jsp, child, "rx_en", 0);

		if (spi_conf->trig_mode == SPI_ASYNC)
			spi_conf->use_ctrl_cs = 0;
		else
			spi_conf->use_ctrl_cs = 1;

		if (lcd_debug_print_flag & LCD_DBG_PR_BL_LDIM) {
			LDIMPR("spi dev_num:%d, bus:%d:%d:%d, cs:%d:%d:%d, freq:%dhz, mode:%d,\n"
				"cs2clk:%d, clk2cs:%d,vs2cs:%d, trig_mode:%d, rx_en:%d\n",
				spi_conf->spi_dev_num,
				spi_conf->spi_info[0].bus_num,
				spi_conf->spi_info[1].bus_num,
				spi_conf->spi_info[2].bus_num,
				spi_conf->spi_info[0].chip_select,
				spi_conf->spi_info[1].chip_select,
				spi_conf->spi_info[2].chip_select,
				spi_conf->spi_info[0].max_speed_hz,
				spi_conf->spi_info[0].mode,
				spi_conf->cs2clk_delay,
				spi_conf->clk2cs_delay,
				spi_conf->vs2cs_delay,
				spi_conf->trig_mode,
				spi_conf->rx_en);
		}
		break;

	case LDIM_DEV_TYPE_ABCON:
		abcon_conf = &dev_drv->abcon_conf;
		abcon_conf->tx_clk = json_get_obj_u32(jsp, child, "tx_clk", 1000);
		abcon_conf->rx_clk = json_get_obj_u32(jsp, child, "rx_clk", 1000);
		child2 = json_get_object_child(jsp, child, "gpio_o");
		if (child2) {
			for (i = 0; i < 3; i++)
				abcon_conf->gpio_o[i] = json_get_arr_u32(jsp, child2, i, 0);
		}
		child2 = json_get_object_child(jsp, child, "gpio_i");
		if (child2) {
			for (i = 0; i < 3; i++)
				abcon_conf->gpio_i[i] = json_get_arr_u32(jsp, child2, i, 0);
		}
		abcon_conf->dev_type = json_get_obj_u32(jsp, child, "dev_type", 0);
		child2 = json_get_object_child(jsp, child, "chip_num");
		if (child2) {
			for (i = 0; i < 12; i++)
				abcon_conf->chip_num[i] = json_get_arr_u32(jsp, child2, i, 0);
		}
		abcon_conf->idle_level = json_get_obj_u32(jsp, child, "idle_level", 0);
		abcon_conf->ch_num = json_get_obj_u32(jsp, child, "ch_num", 4);
		abcon_conf->dimming_mode = json_get_obj_u32(jsp, child, "dimming_mode", 4);
		abcon_conf->fb_en = json_get_obj_u32(jsp, child, "fb_en", 0);
		abcon_conf->fb_det_int = json_get_obj_u32(jsp, child, "fb_det_int", 300);
		abcon_conf->fb_adj_th = json_get_obj_u32(jsp, child, "fb_adj_th", 5);
		abcon_conf->fb_pwm_dir = json_get_obj_u32(jsp, child, "fb_pwm_dir", 1);
		abcon_conf->fb_pwm_step = json_get_obj_u32(jsp, child, "fb_pwm_step", 1);
		abcon_conf->ctrl = json_get_obj_u32(jsp, child, "ctrl", 0x2);

		if (lcd_debug_print_flag & LCD_DBG_PR_BL_LDIM) {
			LDIMPR("clk:%d:%d, gpio_o: 0x%x:0x%x:0x%x, gpio_i:0x%x:0x%x:0x%x,\n"
			"dev_type:%d, chip_num:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d,\n"
			"idle_level:%d, ch_num:%d, dimming_mode:%d, fb_en:%d\n"
			"fb_det_int:%d, fb_adj_th:%d, fb_pwm_dir:%d, fb_pwm_step:%d, ctrl:0x%x\n",
			abcon_conf->tx_clk, abcon_conf->rx_clk,
			abcon_conf->gpio_o[0], abcon_conf->gpio_o[1], abcon_conf->gpio_o[2],
			abcon_conf->gpio_i[0], abcon_conf->gpio_i[1], abcon_conf->gpio_i[2],
			abcon_conf->dev_type,
			abcon_conf->chip_num[0], abcon_conf->chip_num[1], abcon_conf->chip_num[2],
			abcon_conf->chip_num[3], abcon_conf->chip_num[4], abcon_conf->chip_num[5],
			abcon_conf->chip_num[6], abcon_conf->chip_num[7], abcon_conf->chip_num[8],
			abcon_conf->chip_num[9], abcon_conf->chip_num[10], abcon_conf->chip_num[11],
			abcon_conf->idle_level, abcon_conf->ch_num, abcon_conf->dimming_mode,
			abcon_conf->fb_en, abcon_conf->fb_det_int, abcon_conf->fb_adj_th,
			abcon_conf->fb_pwm_dir, abcon_conf->fb_pwm_step, abcon_conf->ctrl);
		}
		break;
	default:
		break;
	}

//pwms
	child = json_get_object_child(jsp, parent, "pwms");
	if (child) {
		cnt = json_get_array_size(jsp, child);
		cnt = lcd_s32_constraint(cnt, 0, 2);
		pwms[0] = &dev_drv->ldim_pwm_config;
		pwms[1] = &dev_drv->analog_pwm_config;
		for (i = 0; i < cnt; i++) {
			child2 = json_get_array_child(jsp, child, i);
			if (!child2) {
				BLPR("fail find pwm[%d]\n", i);
				break;
			}

			bl_pwm = pwms[i];
			bl_pwm->drv_index = 0;
			str = json_get_obj_str(jsp, child2, "port", NULL);
			bl_pwm->pwm_port = bl_pwm_str_to_num(str ? str : "Invalid");
			if (bl_pwm->pwm_port >= BL_PWM_MAX ||
			    (i == 1 && bl_pwm->pwm_port >= BL_PWM_VS))
				continue;

			bl_pwm->pwm_method = json_get_obj_u32(jsp, child2, "polarity", 1);
			bl_pwm->pwm_phase  = json_get_obj_u32(jsp, child2, "phase", 0);
			bl_pwm->pwm_freq   = json_get_obj_u32(jsp, child2, "freq", 300);
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;

			child3 = json_get_object_child(jsp, child2, "duty_range");
			if (child3) {
				bl_pwm->pwm_duty_min = json_get_arr_u32(jsp, child3, 0, 0);
				bl_pwm->pwm_duty_max = json_get_arr_u32(jsp, child3, 1, 4095);
			} else {
				bl_pwm->pwm_duty_min = 0;
				bl_pwm->pwm_duty_max = 4095;
			}
			bl_pwm->pwm_duty = json_get_obj_u32(jsp, child2, "duty",
							    bl_pwm->pwm_duty_min);
			child3 = json_get_object_child(jsp, child2, "pwm_range");
			if (child3) {
				bl_pwm->pwm_range_l = json_get_arr_u32(jsp, child3,
					0, bl_pwm->pwm_duty_min);
				bl_pwm->pwm_range_h = json_get_arr_u32(jsp, child3,
					1, bl_pwm->pwm_duty_max);
			} else {
				bl_pwm->pwm_range_l = bl_pwm->pwm_duty_min;
				bl_pwm->pwm_range_h = bl_pwm->pwm_duty_max;
			}

			bl_pwm_config_init(bl_pwm);
			if (bl_pwm->pwm_port < BL_PWM_VS)
				bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
			LDIMPR("get pwm[%d] pol = %d, freq = %d, phase = %d\n"
				"duty:%d(%d ~ %d), pwm_range:[%d: %d]\n",
				i, bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_phase,
				bl_pwm->pwm_duty, bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max,
				bl_pwm->pwm_range_l, bl_pwm->pwm_range_h);
		}
	}

//ctrl
	child = json_get_object_child(jsp, parent, "ctrl");
	if (child) {
		str = json_get_obj_str(jsp, child, "pinmux_name", NULL);
		strscpy(dev_drv->pinmux_name, str ? str : "invalid", LDIM_DEV_NAME_MAX);

		str = json_get_obj_str(jsp, child, "err_gpio", NULL);
		dev_drv->lamp_err_gpio = ldim_gpio_name_to_index(dev_drv, str);
		str = json_get_obj_str(jsp, child, "en_gpio", NULL);
		dev_drv->en_gpio = ldim_gpio_name_to_index(dev_drv, str);
		dev_drv->en_gpio_on = json_get_obj_u32(jsp, child, "en_gpio_on", 1);
		dev_drv->en_gpio_off = json_get_obj_u32(jsp, child, "en_gpio_off", 0);

		if (dev_drv->en_gpio < BL_GPIO_NUM_MAX)
			ldim_gpio_probe(dev_drv, dev_drv->en_gpio);
		if (dev_drv->lamp_err_gpio < BL_GPIO_NUM_MAX) {
			dev_drv->fault_check = 1;
			ldim_gpio_probe(dev_drv, dev_drv->lamp_err_gpio);
			ldim_gpio_set(dev_drv, dev_drv->lamp_err_gpio, BL_GPIO_INPUT);
		}

		dev_drv->hw_on_delay = json_get_obj_u32(jsp, child, "hw_on_delay_ms", 0);
		dev_drv->hw_off_delay = json_get_obj_u32(jsp, child, "hw_off_delay_ms", 0);
		dev_drv->write_check = json_get_obj_u32(jsp, child, "write_check", 0);
	}

//packet_info
	child = json_get_object_child(jsp, parent, "packet_info");
	if (child) {
		data_pkt = &dev_drv->spi_conf.data_pkt;
		data_pkt->pkt_type = json_get_obj_u32(jsp, child, "pkt_type", 0x0);
		data_pkt->header_cnt = json_get_obj_u32(jsp, child, "headercnt", 0x0);
		child2 = json_get_object_child(jsp, child, "headerdata");
		if (child2) {
			data_pkt->header_data =
			kcalloc(data_pkt->header_cnt, sizeof(unsigned int), GFP_KERNEL);
			if (data_pkt->header_data) {
				for (i = 0; i < data_pkt->header_cnt; i++)
					data_pkt->header_data[i] =
					(unsigned char)json_get_arr_u32(jsp, child2, i, 0);
			} else {
				LDIMERR("kcalloc header_data failed!\n");
			}
		} else {
			LDIMERR("get headerdata failed!\n");
		}

		data_pkt->extend_cnt = json_get_obj_u32(jsp, child, "extendcnt", 0x0);
		child2 = json_get_object_child(jsp, child, "extenddata");
		if (child2) {
			data_pkt->extend_data =
			kcalloc(data_pkt->extend_cnt, sizeof(unsigned int), GFP_KERNEL);
			if (data_pkt->extend_data) {
				for (i = 0; i < data_pkt->extend_cnt; i++)
					data_pkt->extend_data[i] =
					(unsigned char)json_get_arr_u32(jsp, child2, i, 0);
			} else {
				LDIMERR("kcalloc extenddata failed!\n");
			}
		} else {
			LDIMERR("get extenddata failed!\n");
		}

		data_pkt->adim_width = json_get_obj_u32(jsp, child, "adim_width", 0x0);
		data_pkt->adim = json_get_obj_u32(jsp, child, "adim_data", 0x0);
		data_pkt->pdim_width = json_get_obj_u32(jsp, child, "pdim_width", 0x0);
		data_pkt->pdim = json_get_obj_u32(jsp, child, "pdim_data", 0x0);
		data_pkt->datamapping_en = json_get_obj_u32(jsp, child, "datamapping_en", 0x1);
		LDIMPR("header_cnt:%d, extend_cnt:%d, adim_width:%d,pdim_width:%d\n"
			"datamapping_en:%d, pkt_type:%d, adim:%d, pdim:%d\n",
			data_pkt->header_cnt, data_pkt->extend_cnt,
			data_pkt->adim_width, data_pkt->pdim_width,
			data_pkt->datamapping_en, data_pkt->pkt_type,
			data_pkt->adim, data_pkt->pdim);

		if (data_pkt->header_data) {
			for (i = 0; i < data_pkt->header_cnt; i++)
				LDIMPR("header_data[%d]:0x%x\n", i, data_pkt->header_data[i]);
		}

		if (data_pkt->extend_data) {
			for (i = 0; i < data_pkt->extend_cnt; i++)
				LDIMPR("extend_data[%d]:0x%x\n", i, data_pkt->extend_data[i]);
		}
	}

//boost
	child = json_get_object_child(jsp, parent, "boost");
	if (child) {
		dev_drv->boost_conf.en = json_get_obj_u32(jsp, child, "en", 0);
		dev_drv->boost_conf.mode = json_get_obj_u32(jsp, child, "mode", 0);
		dev_drv->boost_conf.kp_l32 = json_get_obj_u32(jsp, child, "kp_l32", 0);
		dev_drv->boost_conf.kp_l100 = json_get_obj_u32(jsp, child, "kp_l100", 0x0);
		dev_drv->boost_conf.i_l32 = json_get_obj_u32(jsp, child, "i_l32", 0x0);
		dev_drv->boost_conf.i_l100 = json_get_obj_u32(jsp, child, "i_l100", 0x0);
		dev_drv->boost_conf.i_l32_val = json_get_obj_u32(jsp, child, "i_l32_val", 0x0);
		dev_drv->boost_conf.i_l100_val = json_get_obj_u32(jsp, child, "i_l100_val", 0x0);
	}

//profile & zone map
	str = json_get_obj_str(jsp, parent, "zone_mapping_path_k", NULL);
	if (!str) {
		if (dev_drv->type == LDIM_DEV_TYPE_ABCON) {
			for (i = 0; i < dev_drv->zone_num; i = i + 4) {
				dev_drv->bl_mapping[j] =
				(i & 0xfff) | (((i + 1) & 0xf) << 12);
				dev_drv->bl_mapping[j + 1] =
				(((i + 1) >> 4) & 0xff) | (((i + 2) & 0xff) << 8);
				dev_drv->bl_mapping[j + 2] =
				(((i + 2) >> 8) & 0xf) | (((i + 3) & 0xfff) << 4);
				j = j + 3;
			}
		} else {
			for (i = 0; i < dev_drv->zone_num; i++)
				dev_drv->bl_mapping[i] = (unsigned short)i;
		}
	} else {
		LDIMPR("find custom zone_mapping: %s\n", str);
		strscpy(dev_drv->bl_mapping_path, str, sizeof(dev_drv->bl_mapping_path));
	}

	str = json_get_obj_str(jsp, parent, "profile_path", NULL);
	if (str) {
		strscpy(dev_drv->profile_path, str, sizeof(dev_drv->profile_path));
	} else {
		LDIMERR("failed to get ldim_bl_profile_path\n");
		strcpy(dev_drv->profile_path, "null");
	}

	str = json_get_obj_str(jsp, child, "boundary_x", NULL);
	if (str) {
		cnt_max = lcd_get_str_array_cnt(str);
		if (cnt_max > 0) {
			dev_drv->boundary_x = kcalloc(cnt_max, sizeof(unsigned int), GFP_KERNEL);
			if (dev_drv->boundary_x)
				cnt = lcd_trans_str_array(str, dev_drv->boundary_x, cnt_max);
		}
	}
	str = json_get_obj_str(jsp, child, "boundary_y", NULL);
	if (str) {
		cnt_max = lcd_get_str_array_cnt(str);
		if (cnt_max > 0) {
			dev_drv->boundary_y = kcalloc(cnt_max, sizeof(unsigned int), GFP_KERNEL);
			if (dev_drv->boundary_y)
				cnt = lcd_trans_str_array(str, dev_drv->boundary_y, cnt_max);
		}
	}

//custom_params
	child = json_get_object_child(jsp, parent, "custom_params");
	if (child && fw_cus && fw_cus->fw_param) {
		cnt = json_get_array_size(jsp, child);
		cnt = lcd_s32_constraint(cnt, 0, 256);
		LDIMPR("get custom_params cnt = %d\n", cnt);
		if (cnt) {//!=0
			fw_cus->fw_param->param_len = cnt;
			fw_cus->fw_param->param = kcalloc(cnt, sizeof(int), GFP_KERNEL);
			if (!fw_cus->fw_param->param)
				goto parse_ldim_init_on;
			for (i = 0; i < cnt; i++)
				fw_cus->fw_param->param[i] = json_get_arr_u32(jsp, child, i, 0);
		}
	}

//commands
parse_ldim_init_on:
	child = json_get_object_child(jsp, parent, "commands");
	if (child) {
		dev_drv->cmd_size = LCD_EXT_CMD_SIZE_DYNAMIC;

		str = json_get_obj_str(jsp, child, "init_on", NULL);
		if (str) {
			cnt_max = lcd_get_str_array_cnt(str);
			nums = kcalloc(cnt_max, sizeof(unsigned int), GFP_KERNEL);
			if (!nums)
				goto parse_ldim_init_off;

			cnt = lcd_trans_str_array(str, nums, cnt_max);
			table = lcd_init_table_load_array("ldim_dev_init_on",
							  LCD_EXT_CMD_SIZE_DYNAMIC,
							  nums, cnt_max, LDIM_INIT_ON_MAX,
							  &data_cnt);
			if (!table) {
				kfree(nums);
				goto ldim_dev_get_config_from_json_end;
			}
			dev_drv->init_on = table;
			dev_drv->init_on_cnt = data_cnt;
			kfree(nums);
		}
parse_ldim_init_off:
		str = json_get_obj_str(jsp, child, "init_off", NULL);
		if (str) {
			cnt_max = lcd_get_str_array_cnt(str);
			nums = kcalloc(cnt_max, sizeof(unsigned int), GFP_KERNEL);
			if (!nums)
				goto ldim_dev_get_config_from_json_end;

			cnt = lcd_trans_str_array(str, nums, cnt_max);
			table = lcd_init_table_load_array("ldim_dev_init_off",
							  LCD_EXT_CMD_SIZE_DYNAMIC,
							  nums, cnt_max, LDIM_INIT_OFF_MAX,
							  &data_cnt);
			if (!table) {
				kfree(nums);
				goto ldim_dev_get_config_from_json_end;
			}
			dev_drv->init_off = table;
			dev_drv->init_off_cnt = data_cnt;
			kfree(nums);
		}
		dev_drv->init_loaded = 1;
	}

ldim_dev_get_config_from_json_end:

	return 0;
}

static inline int ldim_dev_type_str2num(const char *str)
{
	const char *start;

	start = strchr(str, 'V');
	if (start)
		start += 2;
	else
		start = str;

	return strnum_get_num(start, ldim_dev_type_match, ARRAY_SIZE(ldim_dev_type_match),
			      LDIM_DEV_TYPE_MAX);
}

static int ldim_dev_pwm_port_str2num(const char *str)
{
	char *start;

	start = strchr(str, 'P');
	if (!start)
		return BL_PWM_MAX;

	return bl_pwm_str_to_num(start);
}

static int ldim_dev_get_config_from_ini(struct ldim_dev_driver_s *dev_drv, phandle pwm_phandle)
{
	void *inip, *psec;
	const char *str = NULL;
	struct bl_pwm_config_s *bl_pwm;
	struct ldim_fw_custom_s *fw_cus = aml_ldim_get_fw_cus();
	unsigned int val, *init_buf;
	unsigned char *table;
	int init_len, data_cnt, tmp_cnt;
	struct spi_conf_s *spi_conf;
	struct data_pkt_s *data_pkt;
	int i;

	inip = get_lcd_ini_parse_mem(0);
	if (!inip) {
		LDIMERR("%s: parse_mem not ready\n", __func__);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "Ldim_dev_Attr");
	if (!psec) {
		LDIMERR("%s: not find Ldim_dev_Attr\n", __func__);
		return -1;
	}

	dev_drv->index = 0;
	str = lcd_ini_get_str(inip, psec, "dev_name", "null");
	strscpy(dev_drv->name, str, LDIM_DEV_NAME_MAX);

	str = lcd_ini_get_str(inip, psec, "if_type", "null");
	dev_drv->type = ldim_dev_type_str2num(str);
	if (dev_drv->type == LDIM_DEV_TYPE_MAX)
		return -1;

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		spi_conf = &dev_drv->spi_conf;
		spi_conf->spi_info[0].max_speed_hz = lcd_ini_get_val(inip, psec, "if_freq", 0);
		spi_conf->spi_info[1].max_speed_hz = spi_conf->spi_info[0].max_speed_hz;
		spi_conf->spi_info[2].max_speed_hz = spi_conf->spi_info[0].max_speed_hz;

		val = lcd_ini_get_val(inip, psec, "if_attr_0", 0);
		spi_conf->spi_info[0].bus_num = val & 0xf;
		spi_conf->spi_info[1].bus_num = (val >> 4) & 0xf;
		spi_conf->spi_info[2].bus_num = (val >> 8) & 0xf;
		i = (val >> 12) & 0xf;
		if (i)
			spi_conf->spi_dev_num = i;

		val = lcd_ini_get_val(inip, psec, "if_attr_1", 0);
		spi_conf->spi_info[0].chip_select = val & 0xf;
		spi_conf->spi_info[1].chip_select = (val >> 4) & 0xf;
		spi_conf->spi_info[2].chip_select = (val >> 4) & 0xf;

		spi_conf->spi_info[0].mode = lcd_ini_get_val(inip, psec, "if_attr_2", 0);
		spi_conf->spi_info[1].mode = spi_conf->spi_info[0].mode;
		spi_conf->spi_info[2].mode = spi_conf->spi_info[0].mode;

		spi_conf->cs2clk_delay = lcd_ini_get_val(inip, psec, "if_attr_4", 0);
		spi_conf->clk2cs_delay = lcd_ini_get_val(inip, psec, "if_attr_5", 0);
		spi_conf->vs2cs_delay = lcd_ini_get_val(inip, psec, "custome_attr_2", 0);
		spi_conf->trig_mode = lcd_ini_get_val(inip, psec, "if_attr_9", 1);
		spi_conf->rx_en = lcd_ini_get_val(inip, psec, "if_attr_8", 0);
		if (spi_conf->trig_mode == SPI_ASYNC)
			spi_conf->use_ctrl_cs = 0;
		else
			spi_conf->use_ctrl_cs = 1;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_LDIM) {
			LDIMPR("spi dev_num:%d, bus:%d:%d:%d, cs:%d:%d:%d, freq:%dhz, mode:%d\n"
				"cs2clk:%d, clk2cs:%d,vs2cs:%d, trig_mode:%d, rx_en:%d\n",
				spi_conf->spi_dev_num,
				spi_conf->spi_info[0].bus_num,
				spi_conf->spi_info[1].bus_num,
				spi_conf->spi_info[2].bus_num,
				spi_conf->spi_info[0].chip_select,
				spi_conf->spi_info[1].chip_select,
				spi_conf->spi_info[2].chip_select,
				spi_conf->spi_info[0].max_speed_hz,
				spi_conf->spi_info[0].mode,
				spi_conf->cs2clk_delay,
				spi_conf->clk2cs_delay,
				spi_conf->vs2cs_delay,
				spi_conf->trig_mode, spi_conf->rx_en);
		}
		break;
	default:
		break;
	}

	bl_pwm = &dev_drv->ldim_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	str = lcd_ini_get_str(inip, psec, "pwm_vs_port", "null");
	bl_pwm->pwm_port = ldim_dev_pwm_port_str2num(str);
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		str = lcd_ini_get_str(inip, psec, "pwm_vs_pol", "null");
		bl_pwm->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			val = lcd_ini_get_val(inip, psec, "pwm_vs_freq", 0);
			bl_pwm->pwm_freq = (val & 0xff);
			bl_pwm->pwm_phase = (val >> 8) & 0xffffff;

		} else {
			bl_pwm->pwm_freq = lcd_ini_get_val(inip, psec, "pwm_vs_freq", 0);
			bl_pwm->pwm_phase = 0;
		}
		bl_pwm->pwm_duty = lcd_ini_get_val(inip, psec, "pwm_vs_duty", 0);
		bl_pwm->pwm_duty_max = lcd_ini_get_val(inip, psec, "pwm_vs_attr_0", 4095);
		bl_pwm->pwm_duty_min = lcd_ini_get_val(inip, psec, "pwm_vs_attr_1", 0);
		bl_pwm->pwm_range_h = lcd_ini_get_val(inip, psec, "pwm_vs_range_h",
				bl_pwm->pwm_duty_max);
		bl_pwm->pwm_range_l = lcd_ini_get_val(inip, psec, "pwm_vs_range_l",
				bl_pwm->pwm_duty_min);

		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 4) {
				LDIMERR("pwm_vs wrong freq %d\n", bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
		}
		LDIMPR("get ldim_pwm pol: %d, freq: %d, dft duty: %d%%, phase: %d\n",
		       bl_pwm->pwm_method, bl_pwm->pwm_freq, bl_pwm->pwm_duty, bl_pwm->pwm_phase);
		bl_pwm_config_init(bl_pwm);
		if (bl_pwm->pwm_port < BL_PWM_VS)
			bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
	}

	bl_pwm = &dev_drv->analog_pwm_config;
	bl_pwm->drv_index = 0; /* only venc0 support ldim */
	str = lcd_ini_get_str(inip, psec, "pwm_adj_port", "null");
	bl_pwm->pwm_port = ldim_dev_pwm_port_str2num(str);
	if (bl_pwm->pwm_port < BL_PWM_VS) {
		str = lcd_ini_get_str(inip, psec, "pwm_adj_pol", "BL_PWM_POSITIVE");
		bl_pwm->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			val = lcd_ini_get_val(inip, psec, "pwm_adj_freq", 0);
			bl_pwm->pwm_freq = (val & 0xff);
			bl_pwm->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			bl_pwm->pwm_freq = lcd_ini_get_val(inip, psec, "pwm_adj_freq", 0);
			bl_pwm->pwm_phase = 0;
		}
		bl_pwm->pwm_duty = lcd_ini_get_val(inip, psec, "pwm_adj_duty", 0);
		bl_pwm->pwm_duty_max = lcd_ini_get_val(inip, psec, "pwm_adj_attr_0", 0);
		bl_pwm->pwm_duty_min = lcd_ini_get_val(inip, psec, "pwm_adj_attr_1", 0);
		bl_pwm->pwm_range_h = lcd_ini_get_val(inip, psec, "pwm_range_h",
				bl_pwm->pwm_duty_max);
		bl_pwm->pwm_range_l = lcd_ini_get_val(inip, psec, "pwm_range_l",
				bl_pwm->pwm_duty_min);

		if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
			bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
		LDIMPR("get analog_pwm pol = %d, freq = %d\n",
			bl_pwm->pwm_method, bl_pwm->pwm_freq);
		LDIMPR("duty max = %d%%, min = %d%%, default = %d%%, phase=%d\n",
			bl_pwm->pwm_duty_max,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty, bl_pwm->pwm_phase);
		bl_pwm_config_init(bl_pwm);
		bl_pwm_channel_register(dev_drv->dev, pwm_phandle, bl_pwm);
	}

	str = lcd_ini_get_str(inip, psec, "pinmux_sel", "invalid");
	strscpy(dev_drv->pinmux_name, str, LDIM_DEV_NAME_MAX);

	/* ctrl (271Byte) */
	dev_drv->en_gpio = lcd_ini_get_val(inip, psec, "en_gpio", LCD_GPIO_MAX);
	dev_drv->en_gpio_on = lcd_ini_get_val(inip, psec, "en_gpio_on", 0);
	dev_drv->en_gpio_off = lcd_ini_get_val(inip, psec, "en_gpio_off", 0);
	if (dev_drv->en_gpio < BL_GPIO_NUM_MAX)
		ldim_gpio_probe(dev_drv, dev_drv->en_gpio);

	dev_drv->lamp_err_gpio = lcd_ini_get_val(inip, psec, "err_gpio", LCD_GPIO_MAX);
	if (dev_drv->lamp_err_gpio >= BL_GPIO_NUM_MAX) {
		dev_drv->fault_check = 0;
	} else {
		dev_drv->fault_check = 1;
		ldim_gpio_probe(dev_drv, dev_drv->lamp_err_gpio);
		ldim_gpio_set(dev_drv, dev_drv->lamp_err_gpio, BL_GPIO_INPUT);
	}

	dev_drv->hw_on_delay = lcd_ini_get_val(inip, psec, "hw_on_delay", 500);
	dev_drv->hw_off_delay = lcd_ini_get_val(inip, psec, "hw_off_delay", 0);
	dev_drv->write_check = lcd_ini_get_val(inip, psec, "write_check", 0);

	dev_drv->dim_max = lcd_ini_get_val(inip, psec, "dim_max", 0);
	dev_drv->dim_min = lcd_ini_get_val(inip, psec, "dim_min", 0);

	dev_drv->chip_cnt = lcd_ini_get_val(inip, psec, "chip_count", 0);

	//dev_drv->mcu_header = lcd_ini_get_val(inip, psec, "custome_attr_0", 0);
	//dev_drv->mcu_dim = lcd_ini_get_val(inip, psec, "custome_attr_1", 0);
	data_pkt = &dev_drv->spi_conf.data_pkt;
	val = lcd_ini_get_val(inip, psec, "custome_attr_0", 0);
	data_pkt->header_cnt = val & 0xff;
	data_pkt->extend_cnt = (val >> 8) & 0xff;
	data_pkt->adim_width = (val >> 16) & 0x1f;
	data_pkt->pdim_width = (val >> 21) & 0x1f;
	data_pkt->datamapping_en = (val >> 26) & 0x1;
	data_pkt->pkt_type = (val >> 27) & 0xf;
	dev_drv->brightness_mode = (val >> 31) & 0x1;

	val = lcd_ini_get_val(inip, psec, "custome_attr_1", 0);
	data_pkt->adim = val & 0xffff;
	data_pkt->pdim = (val >> 16) & 0xffff;
	//ini fix max header_cnt is 8
	data_pkt->header_data = kcalloc(8, sizeof(unsigned char), GFP_KERNEL);
	if (data_pkt->header_data) {
		val = lcd_ini_get_val(inip, psec, "custome_attr_6", 0);
		data_pkt->header_data[0] = val & 0xff;
		data_pkt->header_data[1] = (val >> 8) & 0xff;
		data_pkt->header_data[2] = (val >> 16) & 0xff;
		data_pkt->header_data[3] = (val >> 24) & 0xff;
		val = lcd_ini_get_val(inip, psec, "custome_attr_7", 0);
		data_pkt->header_data[4] = val & 0xff;
		data_pkt->header_data[5] = (val >> 8) & 0xff;
		data_pkt->header_data[6] = (val >> 16) & 0xff;
		data_pkt->header_data[7] = (val >> 24) & 0xff;
	} else {
		LDIMERR("kcalloc header_data failed!\n");
	}

	data_pkt->extend_data = kcalloc(8, sizeof(unsigned char), GFP_KERNEL);
	if (data_pkt->extend_data) {
		val = lcd_ini_get_val(inip, psec, "custome_attr_8", 0);
		data_pkt->extend_data[0] = val & 0xff;
		data_pkt->extend_data[1] = (val >> 8) & 0xff;
		data_pkt->extend_data[2] = (val >> 16) & 0xff;
		data_pkt->extend_data[3] = (val >> 24) & 0xff;
		val = lcd_ini_get_val(inip, psec, "custome_attr_9", 0);
		data_pkt->extend_data[4] = val & 0xff;
		data_pkt->extend_data[5] = (val >> 8) & 0xff;
		data_pkt->extend_data[6] = (val >> 16) & 0xff;
		data_pkt->extend_data[7] = (val >> 24) & 0xff;
	} else {
		LDIMERR("kcalloc extend_data failed!\n");
	}
	LDIMPR("header_cnt:%d, extend_cnt:%d, adim_width:%d,pdim_width:%d\n"
	       "datamapping_en:%d, pkt_type:%d, brightness_mode:%d,adim:%d, pdim:%d\n",
		data_pkt->header_cnt, data_pkt->extend_cnt,
		data_pkt->adim_width, data_pkt->pdim_width,
		data_pkt->datamapping_en, data_pkt->pkt_type,
		dev_drv->brightness_mode, data_pkt->adim, data_pkt->pdim);

	if (data_pkt->header_data)
		LDIMPR("header_data:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
			data_pkt->header_data[0], data_pkt->header_data[1],
			data_pkt->header_data[2], data_pkt->header_data[3],
			data_pkt->header_data[4], data_pkt->header_data[5],
			data_pkt->header_data[6], data_pkt->header_data[7]);

	if (data_pkt->extend_data)
		LDIMPR("extend_data:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
			data_pkt->extend_data[0], data_pkt->extend_data[1],
			data_pkt->extend_data[2], data_pkt->extend_data[3],
			data_pkt->extend_data[4], data_pkt->extend_data[5],
			data_pkt->extend_data[6], data_pkt->extend_data[7]);

	val = lcd_ini_get_val(inip, psec, "custome_attr_2", 0);
	if (val == 0) {
		dev_drv->spi_conf.trig_mode = SPI_ASYNC;
		dev_drv->spi_conf.vs2cs_delay = 0;
		dev_drv->spi_conf.use_ctrl_cs = 0;
	} else {
		dev_drv->spi_conf.trig_mode = SPI_TRIG_VSYNC;
		dev_drv->spi_conf.vs2cs_delay = val;
		dev_drv->spi_conf.use_ctrl_cs = 1;
	}

	val = lcd_ini_get_val(inip, psec, "custome_attr_3", 0);
	dev_drv->boost_conf.en = (val >> 31) & 0x1;
	dev_drv->boost_conf.mode = (val >> 24) & 0x7f;
	dev_drv->boost_conf.kp_l32 = (val >> 8) & 0xff;
	dev_drv->boost_conf.kp_l100 = val & 0xff;
	val = lcd_ini_get_val(inip, psec, "custome_attr_4", 0);
	dev_drv->boost_conf.i_l100 = val & 0xffff;
	dev_drv->boost_conf.i_l32 = (val >> 16) & 0xffff;
	val = lcd_ini_get_val(inip, psec, "custome_attr_5", 0);
	dev_drv->boost_conf.i_l100_val = val & 0xffff;
	dev_drv->boost_conf.i_l32_val = (val >> 16) & 0xffff;

	str = lcd_ini_get_str(inip, psec, "zone_mapping_path_k", NULL);
	if (!str) {
		for (i = 0; i < dev_drv->zone_num; i++)
			dev_drv->bl_mapping[i] = (unsigned short)i;
	} else {
		LDIMPR("find custom zone_mapping: %s\n", str);
		strscpy(dev_drv->bl_mapping_path, str, sizeof(dev_drv->bl_mapping_path));
	}

	str = lcd_ini_get_str(inip, psec, "profile_path", "null");
	if (str) {
		strscpy(dev_drv->profile_path, str, sizeof(dev_drv->profile_path));
	} else {
		LDIMERR("failed to get ldim_bl_profile_path\n");
		strcpy(dev_drv->profile_path, "null");
	}

	init_len = lcd_ini_get_array_cnt(inip, psec, "boundary_x");
	if (init_len > 0) {
		dev_drv->boundary_x = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
		if (dev_drv->boundary_x)
			lcd_ini_get_array(inip, psec, "boundary_x",
				dev_drv->boundary_x, init_len);
	}

	init_len = lcd_ini_get_array_cnt(inip, psec, "boundary_y");
	if (init_len > 0) {
		dev_drv->boundary_y = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
		if (dev_drv->boundary_y)
			lcd_ini_get_array(inip, psec, "boundary_y",
				dev_drv->boundary_y, init_len);
	}

	if (fw_cus && fw_cus->fw_param) {
		tmp_cnt = lcd_ini_get_array(inip, psec, "param_data", fw_cus->fw_param->param, 32);
		LDIMPR("custom param size = %d\n",  tmp_cnt);
		if (tmp_cnt > 0) {
			fw_cus->fw_param->param_len = tmp_cnt;
			fw_cus->fw_param->param = kcalloc(tmp_cnt, sizeof(int), GFP_KERNEL);
			if (fw_cus->fw_param->param) {
				for (i = 0; i < tmp_cnt; i++) {
					LDIMPR("fw_cus->fw_param->param[%d] = %d = 0x%x\n",
					i, fw_cus->fw_param->param[i], fw_cus->fw_param->param[i]);
				}
			}
		}
	}

	dev_drv->cmd_size = lcd_ini_get_val(inip, psec, "cmd_size", 0);
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_LDIM)
		LDIMPR("%s: cmd_size = %d\n", dev_drv->name, dev_drv->cmd_size);
	if (dev_drv->cmd_size != LCD_EXT_CMD_SIZE_DYNAMIC)
		return 0;

	init_len = lcd_ini_get_array_cnt(inip, psec, "init_on");
	if (init_len < 0) {
		LDIMPR("%s: not find init_on\n", dev_drv->name);
		return 0;
	}
	init_buf = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;
	data_cnt = lcd_ini_get_array(inip, psec, "init_on", init_buf, init_len);
	table = lcd_init_table_load_array("ldim_dev_init_on", LCD_EXT_CMD_SIZE_DYNAMIC,
					  init_buf, data_cnt, LDIM_INIT_ON_MAX, &init_len);
	if (!table)
		goto ldim_dev_get_config_from_ini_err;
	dev_drv->init_on = table;
	dev_drv->init_on_cnt = init_len;
	kfree(init_buf);

	init_len = lcd_ini_get_array_cnt(inip, psec, "init_off");
	if (init_len < 0) {
		LDIMPR("%s: not find init_off\n", dev_drv->name);
		return 0;
	}
	init_buf = kcalloc(init_len, sizeof(unsigned int), GFP_KERNEL);
	if (!init_buf)
		return -1;
	data_cnt = lcd_ini_get_array(inip, psec, "init_off", init_buf, init_len);
	table = lcd_init_table_load_array("ldim_dev_init_off", LCD_EXT_CMD_SIZE_DYNAMIC,
					  init_buf, data_cnt, LDIM_INIT_OFF_MAX, &init_len);
	if (!table)
		goto ldim_dev_get_config_from_ini_err;
	dev_drv->init_off = table;
	dev_drv->init_off_cnt = init_len;
	kfree(init_buf);

	dev_drv->init_loaded = 1;

	return 0;

ldim_dev_get_config_from_ini_err:
	kfree(init_buf);
	return -1;
}

static int ldim_check_config_load(struct ldim_dev_driver_s *drv)
{
	drv->config_load = lcd_panel_config_load_detect(0, __func__);
	if (drv->config_load == LCD_CONFIG_NONE || drv->config_load == LCD_CONFIG_ERR)
		return -1;

	return 0;
}

int ldim_dev_get_config(struct ldim_dev_driver_s *dev_drv, struct device_node *np, int index)
{
	unsigned int val;
	phandle pwm_phandle;
	int ret = 0, cnt = 0, i;
	const char *ldim_gpio_str[BL_GPIO_NUM_MAX];
	unsigned char file_type = PANEL_FILE_INVALID;
	char str[160];
	int len = 0;

	ret = of_property_read_u32(np, "key_valid", &val);
	if (ret) {
		LDIMERR("failed to get key_valid\n");
		val = 0;
	}
	dev_drv->key_valid = val;

	ret = of_property_read_u32(np, "ldim_pwm_config", &pwm_phandle);
	if (ret) {
		LDIMERR("failed to get ldim_pwm_config node\n");
		return -1;
	}

	cnt = of_property_read_string_array(dev_drv->dev->of_node, "ldim_dev_gpio_names",
					    ldim_gpio_str, BL_GPIO_NUM_MAX);
	LDIMPR("ldim_dev_gpio_names cnt: %d\n", cnt);
	for (i = 0; i < cnt; i++)
		strscpy(ldim_gpio[i].name, ldim_gpio_str[i], LCD_CPU_GPIO_NAME_MAX);

	if (ldim_check_config_load(dev_drv))
		return -1;

	switch (dev_drv->config_load) {
	case LCD_CONFIG_DTS:
		ret = ldim_dev_get_config_from_dts(dev_drv, np, index, pwm_phandle);
		break;
	case LCD_CONFIG_FILE:
		file_type = get_lcd_panel_file_type(0);
		if (file_type == PANEL_FILE_JSON)
			ret = ldim_dev_get_config_from_json(dev_drv, pwm_phandle);
		else if (file_type == PANEL_FILE_INI)
			ret = ldim_dev_get_config_from_ini(dev_drv, pwm_phandle);
		break;
	default:
		ret = -1;
		break;
	}
	len += sprintf(str + len, "trig_mode:%d, vs2cs_delay: %d, ",
		dev_drv->spi_conf.trig_mode, dev_drv->spi_conf.vs2cs_delay);
	sprintf(str + len,
		"chip_cnt:%d, hw_on_dly: %dms, hw_off_dly: %dms, cus pwm_pinmux_sel:%s",
		dev_drv->chip_cnt, dev_drv->hw_on_delay, dev_drv->hw_off_delay,
		dev_drv->pinmux_name);

	LDIMPR("load %s config: %s: type:%d, %s\n",
		dev_drv->name, get_lcd_config_load(dev_drv->config_load),
		dev_drv->type, str);

	len = sprintf(str, "i_l100:%d, i_l32:%d, i_l100_val:%d, i_l32_val:%d, ",
		dev_drv->boost_conf.i_l100, dev_drv->boost_conf.i_l32,
		dev_drv->boost_conf.i_l100_val, dev_drv->boost_conf.i_l32_val);
	sprintf(str + len, "kp_l100:%d, kp_l32:%d",
		dev_drv->boost_conf.kp_l100, dev_drv->boost_conf.kp_l32);
	LDIMPR("boost_conf: en:%d, mode:%d, %s\n",
		dev_drv->boost_conf.en, dev_drv->boost_conf.mode, str);

	return ret;
}
