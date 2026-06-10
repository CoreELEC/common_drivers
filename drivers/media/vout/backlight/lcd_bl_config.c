// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/backlight.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/amlogic/pwm-meson.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
#include <linux/amlogic/media/vout/backlight/aml_bl_notify.h>

#include <linux/compat.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/backlight/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#endif
#include "lcd_bl.h"
// #include "../lcd_reg.h"
// #include "../lcd_common.h"

#define PWM_STR_MAX 32
char pwm_str[PWM_STR_MAX];

struct bl_method_match_s {
	char *name;
	enum bl_ctrl_method_e type;
};

static struct bl_method_match_s bl_method_match_table[] = {
	{"gpio",          BL_CTRL_GPIO},
	{"pwm",           BL_CTRL_PWM},
	{"pwm_combo",     BL_CTRL_PWM_COMBO},
	{"pwm_array",     BL_CTRL_PWM_ARRAY},
	{"local_dimming", BL_CTRL_LOCAL_DIMMING},
	{"extern",        BL_CTRL_EXTERN},
	{"invalid",       BL_CTRL_MAX},
};

char *bl_method_type_to_str(int type)
{
	int i;
	char *str = bl_method_match_table[BL_CTRL_MAX].name;

	for (i = 0; i < BL_CTRL_MAX; i++) {
		if (type == bl_method_match_table[i].type) {
			str = bl_method_match_table[i].name;
			break;
		}
	}
	return str;
}

static void bl_pwm_config_print(struct aml_bl_drv_s *bdrv,
				struct bl_pwm_config_s *bl_pwm, const char *tag)
{
	if (!bl_pwm)
		return;

	BLPR(bdrv->index, "%s_index:       %d", tag, bl_pwm->index);
	BLPR(bdrv->index, "%s_port:        %s(0x%x)",
	     tag, bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port);
	BLPR(bdrv->index, "%s_method:      %d", tag, bl_pwm->pwm_method);
	BLPR(bdrv->index, "%s_freq:        %d", tag, bl_pwm->pwm_freq);
	BLPR(bdrv->index, "%s_level_range: %d~%d(in:%d~%d)",
	     tag, bl_pwm->level_min, bl_pwm->level_max,
	     bl_bri_level_output_scale(bdrv, bl_pwm->level_min),
	     bl_bri_level_output_scale(bdrv, bl_pwm->level_max));
	BLPR(bdrv->index, "%s_duty_range:  %d~%d(in:%d~%d)",
	     tag, bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max,
	     bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
	     bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max));
	BLPR(bdrv->index, "%s_duty_dft:    %d(in:%d)",
	     tag, bl_pwm->pwm_duty_dft, bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_dft));
	BLPR(bdrv->index, "%s_duty:        %d(in:%d)",
	     tag, bl_pwm->pwm_duty, bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty));

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		BLPR(bdrv->index, "%s_phase:      %d", tag, bl_pwm->pwm_phase);
		break;
	default:
		break;
	}
}

static void bl_config_print(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	char str[128];
	char tag_str[8];
	int len = 0, i;

	if (bconf->method == BL_CTRL_MAX) {
		BLPR(bdrv->index, "no backlight exist");
		return;
	}

	if ((bl_debug_print_flag & BL_DBG_PR_NORMAL) == 0) {
		len += sprintf(str + len, "step_on_flag:%d, en_seq_rev:%d",
			bdrv->step_on_flag, bconf->en_sequence_reverse);
		if (bconf->ldim_flag)
			sprintf(str + len, ", ldim_flag:%d", bconf->ldim_flag);
		BLPR(bdrv->index, "config from %s: %s, method: %s(%d), %s",
			get_lcd_config_load(bdrv->config_load),
			bconf->name, bl_method_type_to_str(bconf->method), bconf->method,
			str);
		return;
	}

	BLPR(bdrv->index, "level_in_scale   = %d", bconf->level_in_scale);
	BLPR(bdrv->index, "en_gpio          = %d", bconf->en_gpio);
	BLPR(bdrv->index, "en_gpio_on_off   = %d,%d", bconf->en_gpio_on, bconf->en_gpio_off);
	BLPR(bdrv->index, "power_on_off_dly = %d,%dms", bconf->power_on_delay,
							bconf->power_off_delay);
	BLPR(bdrv->index, "en_sequence_reverse = %d", bconf->en_sequence_reverse);
	BLPR(bdrv->index, "bl_hold_on    = %dms", bconf->bl_hold_on);
	BLPR(bdrv->index, "bl_pinmux_sel = %s", bconf->pinmux_name);
	switch (bconf->method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		BLPR(bdrv->index, "pwm_on_off_delay   = %d,%dms", bconf->pwm_on_delay,
								bconf->pwm_off_delay);
		BLPR(bdrv->index, "pwm_duty_in_scale  = %d", bconf->pwm_duty_in_scale);
		BLPR(bdrv->index, "pwm_mapping_method = %d", bconf->pwm_mapping_method);
		break;
	default:
		break;
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bl_pwm_config_print(bdrv, bconf->bl_pwm, "pwm");
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm_config_print(bdrv, bconf->bl_pwm_combo0, "pwm_0");
		bl_pwm_config_print(bdrv, bconf->bl_pwm_combo1, "pwm_1");
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			snprintf(tag_str, sizeof(tag_str), "pwm[%d]", i);
			bl_pwm_config_print(bdrv, bconf->bl_pwm_array[i], tag_str);
		}
		break;
	default:
		break;
	}
}

static int bl_pwm_switch_init(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	if (bconf->bl_pwm_switch_port < BL_PWM_MAX && bconf->bl_pwm_switch_freq > 0) {
		bconf->bl_pwm_switch = kzalloc(sizeof(*bconf->bl_pwm_switch), GFP_KERNEL);
		if (!bconf->bl_pwm_switch)
			return -1;

		switch (bconf->method) {
		case BL_CTRL_PWM:
			bconf->bl_pwm_switch = memcpy(bconf->bl_pwm_switch,
				bconf->bl_pwm, sizeof(*bconf->bl_pwm));
			bconf->bl_pwm_default = bconf->bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			bconf->bl_pwm_switch = memcpy(bconf->bl_pwm_switch,
				bconf->bl_pwm_combo0, sizeof(*bconf->bl_pwm_combo0));
			bconf->bl_pwm_default = bconf->bl_pwm_combo0;
			break;
		default:
			break;
		}

		bconf->bl_pwm_switch->pwm_port = bconf->bl_pwm_switch_port;
		bconf->bl_pwm_switch->pwm_freq = bconf->bl_pwm_switch_freq;
		bl_pwm_config_init(bconf->bl_pwm_switch);

		BLPR(bdrv->index, "pwm_switch port_freq: dft: %s(0x%x),%d, switch: %s(0x%x),%d",
			bl_pwm_num_to_str(bconf->bl_pwm_default->pwm_port),
			bconf->bl_pwm_default->pwm_port, bconf->bl_pwm_default->pwm_freq,
			bl_pwm_num_to_str(bconf->bl_pwm_switch_port),
			bconf->bl_pwm_switch_port, bconf->bl_pwm_switch_freq);
	} else {
		bconf->bl_pwm_switch = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static int bl_config_load_from_dts(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	const char *str;
	unsigned int para[10];
	char bl_propname[20];
	struct device_node *child;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	int val, ret = 0;

	/* select backlight by index */
	bconf->index = aml_bl_index_get(bdrv->index);
	if (bconf->index == 0xff) {
		bconf->method = BL_CTRL_MAX;
		return -1;
	}
	snprintf(bl_propname, 20, "backlight_%d", bconf->index);
	child = of_get_child_by_name(bdrv->dev->of_node, bl_propname);
	if (!child) {
		BLERR(bdrv->index, "failed to get %s", bl_propname);
		return -1;
	}

	ret = of_property_read_string(child, "bl_name", &str);
	if (ret) {
		BLERR(bdrv->index, "failed to get bl_name");
		str = "backlight";
	}
	strscpy(bconf->name, str, BL_NAME_MAX);

	ret = of_property_read_string(child, "bl_pinmux_sel", &str);
	if (!ret) {
		BLERR(bdrv->index, "find custom bl_pinmux_sel:%s", str);
		strscpy(bconf->pinmux_name, str, BL_NAME_MAX);
	} else {
		strscpy(bconf->pinmux_name, "invalid", BL_NAME_MAX);
	}

	ret = of_property_read_u32(child, "bl_step_on_flag", &val);
	if (ret)
		bdrv->step_on_flag = 0;
	else
		bdrv->step_on_flag = val;

	ret = of_property_read_u32(child, "bl_level_in_scale", &val);
	if (ret)
		bconf->level_in_scale = 255; //BL_LEVEL_FULL_SCALE;
	else
		bconf->level_in_scale = val;

	ret = of_property_read_u32(child, "bl_ctrl_method", &val);
	if (ret) {
		BLERR(bdrv->index, "failed to get bl_ctrl_method");
		bconf->method = BL_CTRL_MAX;
	} else {
		bconf->method = (val >= BL_CTRL_MAX) ? BL_CTRL_MAX : val;
	}

	ret = of_property_read_u32_array(child, "bl_power_attr", &para[0], 5);
	if (ret) {
		BLERR(bdrv->index, "failed to get bl_power_attr");
		bconf->en_gpio = BL_GPIO_MAX;
		bconf->en_gpio_on = BL_GPIO_OUTPUT_HIGH;
		bconf->en_gpio_off = BL_GPIO_OUTPUT_LOW;
		bconf->power_on_delay = 100;
		bconf->power_off_delay = 30;
	} else {
		if (para[0] >= BL_GPIO_NUM_MAX) {
			bconf->en_gpio = BL_GPIO_MAX;
		} else {
			bconf->en_gpio = para[0];
			bl_gpio_probe(bdrv, bconf->en_gpio);
		}
		bconf->en_gpio_on = para[1];
		bconf->en_gpio_off = para[2];
		bconf->power_on_delay = para[3];
		bconf->power_off_delay = para[4];
	}
	ret = of_property_read_u32(child, "en_sequence_reverse", &val);
	if (ret)
		bconf->en_sequence_reverse = 0;
	else
		bconf->en_sequence_reverse = val;

	ret = of_property_read_u32(child, "pwm_duty_in_scale", &val);
	if (ret)
		bconf->pwm_duty_in_scale = 100;//BL_PWM_DUTY_FULL_SCALE;
	else
		bconf->pwm_duty_in_scale = val;
	ret = of_property_read_u32(child, "pwm_mapping_method", &val);
	if (ret)
		bconf->pwm_mapping_method = PWM_MAP_RESCALING;
	else
		bconf->pwm_mapping_method = val;

	ret = of_property_read_u32_array(child, "bl_ldim_region_row_col", &para[0], 2);
	if (ret) {
		ret = of_property_read_u32_array(child, "bl_ldim_zone_row_col", &para[0], 2);
		if (ret == 0) {
			if (para[0] && para[1])
				bconf->ldim_flag = 1;
		}
	} else {
		if (para[0] && para[1])
			bconf->ldim_flag = 1;
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm)
			return -1;

		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;
		bl_pwm->drv_index = bdrv->index;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		ret = of_property_read_string(child, "bl_pwm_port", &str);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_port");
			bl_pwm->pwm_port = BL_PWM_MAX;
		} else {
			bl_pwm->pwm_port = bl_pwm_str_to_num(str);
			BLPR(bdrv->index, "bl pwm_port: %s(0x%x)", str, bl_pwm->pwm_port);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_level_range", &para[0], 2);
		if (ret) {
			bl_pwm->level_max = bconf->level_max;
			bl_pwm->level_min = bconf->level_min;
		} else {
			bl_pwm->level_max = bl_bri_level_input_scale(bdrv, para[0]);
			bl_pwm->level_min = bl_bri_level_input_scale(bdrv, para[1]);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_attr", &para[0], 4);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_attr");
			bl_pwm->pwm_method = BL_PWM_POSITIVE;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				bl_pwm->pwm_freq = BL_FREQ_DEFAULT;
			bl_pwm->pwm_duty_max = 0;
			bl_pwm->pwm_duty_min = 0;
			bl_pwm->pwm_phase = 0;
		} else {
			bl_pwm->pwm_method = para[0];
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				bl_pwm->pwm_freq = para[1] & 0xff;
				bl_pwm->pwm_phase = (para[1] >> 8) & 0xffffff;
			} else {
				bl_pwm->pwm_freq = para[1];
				bl_pwm->pwm_phase = 0;
			}
			bl_pwm->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, para[2]);
			bl_pwm->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, para[3]);
		}
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			if (bl_pwm->pwm_freq > 8) {
				BLERR(bdrv->index, "bl_pwm_vs wrong freq %d", bl_pwm->pwm_freq);
				bl_pwm->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
				bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
		}
		ret = of_property_read_u32(child, "bl_pwm_duty_dft", &val);
		if (ret)
			bl_pwm->pwm_duty_dft = bl_pwm->pwm_duty_min;
		else
			bl_pwm->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);
		ret = of_property_read_u32_array(child, "bl_pwm_power", &para[0], 4);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_power");
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[2];
			bconf->pwm_off_delay = para[3];
		}

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
		bl_pwm->pwm_duty_save = bl_pwm->pwm_duty_dft;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0)
			return -1;
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1)
			return -1;

		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;

		pwm_combo0->index = 0;
		pwm_combo1->index = 1;
		pwm_combo0->drv_index = bdrv->index;
		pwm_combo1->drv_index = bdrv->index;

		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 0, &str);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_combo_port");
			pwm_combo0->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo0->pwm_port = bl_pwm_str_to_num(str);
		}
		ret = of_property_read_string_index(child, "bl_pwm_combo_port", 1, &str);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_combo_port");
			pwm_combo1->pwm_port = BL_PWM_MAX;
		} else {
			pwm_combo1->pwm_port = bl_pwm_str_to_num(str);
		}
		BLPR(bdrv->index, "pwm_combo_port: %s(0x%x), %s(0x%x)",
		     bl_pwm_num_to_str(pwm_combo0->pwm_port),
		     pwm_combo0->pwm_port,
		     bl_pwm_num_to_str(pwm_combo1->pwm_port),
		     pwm_combo1->pwm_port);
		ret = of_property_read_u32_array(child, "bl_pwm_combo_level_range",
						 &para[0], 4);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_combo_level_range");
			pwm_combo0->level_max = bconf->level_max;
			pwm_combo0->level_min = bconf->level_min;
			pwm_combo1->level_max = bconf->level_max;
			pwm_combo1->level_min = bconf->level_min;
		} else {
			pwm_combo0->level_max = bl_bri_level_input_scale(bdrv, para[0]);
			pwm_combo0->level_min = bl_bri_level_input_scale(bdrv, para[1]);
			pwm_combo1->level_max = bl_bri_level_input_scale(bdrv, para[2]);
			pwm_combo1->level_min = bl_bri_level_input_scale(bdrv, para[3]);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_attr", &para[0], 8);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_pwm_combo_attr");
			pwm_combo0->pwm_method = BL_PWM_POSITIVE;
			if (pwm_combo0->pwm_port == BL_PWM_VS)
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo0->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo0->pwm_duty_max = 0;
			pwm_combo0->pwm_duty_min = 0;
			pwm_combo1->pwm_method = BL_PWM_NEGATIVE;
			if (pwm_combo1->pwm_port == BL_PWM_VS)
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			else
				pwm_combo1->pwm_freq = BL_FREQ_DEFAULT;
			pwm_combo1->pwm_duty_max = 0;
			pwm_combo1->pwm_duty_min = 0;
			pwm_combo0->pwm_phase = 0;
			pwm_combo1->pwm_phase = 0;
		} else {
			pwm_combo0->pwm_method = para[0];
			if (pwm_combo0->pwm_port == BL_PWM_VS) {
				pwm_combo0->pwm_freq = para[1] & 0xff;
				pwm_combo0->pwm_phase = (para[1] >> 8) & 0xffffff;
			} else {
				pwm_combo0->pwm_freq = para[1];
				pwm_combo0->pwm_phase = 0;
			}
			pwm_combo0->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, para[2]);
			pwm_combo0->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, para[3]);
			pwm_combo1->pwm_method = para[4];
			if (pwm_combo1->pwm_port == BL_PWM_VS) {
				pwm_combo1->pwm_freq = para[5] & 0xff;
				pwm_combo1->pwm_phase = (para[5] >> 8) & 0xffffff;
			} else {
				pwm_combo1->pwm_freq = para[5];
				pwm_combo1->pwm_phase = 0;
			}
			pwm_combo1->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, para[6]);
			pwm_combo1->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, para[7]);
		}
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			if (pwm_combo0->pwm_freq > 4) {
				BLERR(bdrv->index, "bl_pwm_0_vs wrong freq %d",
					pwm_combo0->pwm_freq);
				pwm_combo0->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo0->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo0->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo0->pwm_freq < 50)
				pwm_combo0->pwm_freq = 50;
		}
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			if (pwm_combo1->pwm_freq > 4) {
				BLERR(bdrv->index, "bl_pwm_1_vs wrong freq %d",
				      pwm_combo1->pwm_freq);
				pwm_combo1->pwm_freq = BL_FREQ_VS_DEFAULT;
			}
		} else {
			if (pwm_combo1->pwm_freq > XTAL_HALF_FREQ_HZ)
				pwm_combo1->pwm_freq = XTAL_HALF_FREQ_HZ;
			if (pwm_combo1->pwm_freq < 50)
				pwm_combo1->pwm_freq = 50;
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_duty_dft", &para[0], 2);
		if (ret) {
			pwm_combo0->pwm_duty_dft = pwm_combo0->pwm_duty_min;
			pwm_combo1->pwm_duty_dft = pwm_combo1->pwm_duty_min;
		} else {
			pwm_combo0->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, para[0]);
			pwm_combo1->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, para[1]);
		}
		ret = of_property_read_u32_array(child, "bl_pwm_combo_power", &para[0], 6);
		if (ret) {
			BLERR(0, "[%d]: failed to get bl_pwm_combo_power\n", bdrv->index);
			bconf->pwm_on_delay = 0;
			bconf->pwm_off_delay = 0;
		} else {
			bconf->pwm_on_delay = para[4];
			bconf->pwm_off_delay = para[5];
		}

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_dft;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_dft;
		pwm_combo0->pwm_duty_save = pwm_combo0->pwm_duty_dft;
		pwm_combo1->pwm_duty_save = pwm_combo1->pwm_duty_dft;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		/* get bl_extern_index from dts */
		ret = of_property_read_u32(child, "bl_extern_index", &para[0]);
		if (ret) {
			BLERR(bdrv->index, "failed to get bl_extern_index");
		} else {
			bconf->bl_extern_index = para[0];
			bl_extern_dev_index_add(bdrv->index, para[0]);
			BLPR(bdrv->index, "get bl_extern_index = %d",  bconf->bl_extern_index);
		}
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag) {
		ret = aml_ldim_get_config_dts(child);
		if (ret < 0)
			return -1;
	}
#endif

	return 0;
}
#endif

/* config from json =============================================================================*/

static struct num_str_s bl_ctrl_method[] = {
	{BL_CTRL_GPIO,          "BL_CTRL_GPIO"},
	{BL_CTRL_PWM,           "BL_CTRL_PWM"},
	{BL_CTRL_PWM_COMBO,     "BL_CTRL_PWM_COMBO"},
	{BL_CTRL_PWM_ARRAY,       "BL_CTRL_PWM_ARRAY"},
	{BL_CTRL_LOCAL_DIMMING, "BL_CTRL_LOCAL_DIMMING"},
	{BL_CTRL_EXTERN,        "BL_CTRL_EXTERN"},
	{BL_CTRL_MAX,           "BL_CTRL_MAX"},
};

static inline int bl_ctrl_method_str2num(const char *str)
{
	return bl_strnum_get_num(str, bl_ctrl_method, ARRAY_SIZE(bl_ctrl_method), BL_CTRL_MAX);
}

static int bl_gpio_name_to_index(struct aml_bl_drv_s *bdrv, const char *name)
{
	int i = 0;

	if (!bdrv || !name)
		return BL_GPIO_MAX;

	for (i = 0; i < BL_GPIO_NUM_MAX; i++)
		if (!strcmp(bdrv->bconf.bl_gpio[i].name, name))
			return i;
	return BL_GPIO_MAX;
}

static int bl_config_load_from_json(struct aml_bl_drv_s *bdrv)
{
	int cnt = 0, val, i = 0;
	struct json_parse_s *jsp;
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm, *pwms[4] = {NULL, NULL, NULL, NULL};
	const char *str = NULL;
	struct json_s *parent, *child, *child2, *child3;

	jsp = get_panel_jsp(bdrv->index);
	if (!json_parse_ok(jsp)) {
		BLERR(0, "panel%d json not ready\n", bdrv->index);
		return -1;
	}

	parent = json_get_object_child(jsp, jsp->root, "backlight");
	if (!parent) {
		BLERR(0, "failed find /backlight\n");
		return -1;
	}

//basic
	child = json_get_object_child(jsp, parent, "basic_info");
	if (!child) {
		BLERR(0, "failed find basic_info\n");
		return -1;
	}

	str = json_get_obj_str(jsp, child, "name", NULL);
	if (str)
		strscpy(bconf->name, str, BL_NAME_MAX);

	bdrv->step_on_flag = json_get_obj_u32(jsp, child, "step_on_flag", 0);

//level setup
	child = json_get_object_child(jsp, parent, "level_setup");
	if (!child) {
		BLERR(0, "failed find level_setup\n");
		return -1;
	}

	bconf->level_in_scale = json_get_obj_u32(jsp, child, "in_scale", 255);
	bconf->level_min   = 0;
	bconf->level_max   = BL_LEVEL_FULL_SCALE;

//control method
	child = json_get_object_child(jsp, parent, "control_method");
	if (!child) {
		BLERR(0, "failed find control_method\n");
		return -1;
	}
	bconf->method  = bl_ctrl_method_str2num(json_get_obj_str(jsp, child, "method", NULL));
	bconf->en_gpio = bl_gpio_name_to_index(bdrv, json_get_obj_str(jsp, child, "en_gpio", NULL));
	bconf->en_gpio_on          = json_get_obj_u32(jsp, child, "en_gpio_on", 1);
	bconf->en_gpio_off         = json_get_obj_u32(jsp, child, "en_gpio_off", 0);
	//bconf->power_on_delay    = json_get_obj_u32(jsp, child, "bl_on_delay_ms", 0);
	//bconf->power_off_delay   = json_get_obj_u32(jsp, child, "bl_off_delay_ms", 0);
	bconf->pwm_on_delay        = json_get_obj_u32(jsp, child, "pwm_on_delay_ms", 0);
	bconf->pwm_off_delay       = json_get_obj_u32(jsp, child, "pwm_off_delay_ms", 0);
	bconf->en_sequence_reverse = json_get_obj_u32(jsp, child, "en_sequence_reverse", 0);
	bconf->bl_pwm_switch_port = json_get_obj_u32(jsp, child, "pwm_switch_port", BL_PWM_MAX);
	bconf->bl_pwm_switch_freq = json_get_obj_u32(jsp, child, "pwm_switch_freq", 0);
	bconf->bl_hold_on = json_get_obj_u32(jsp, child, "bl_hold_on", 0);

	bconf->pwm_duty_in_scale  =
		json_get_obj_u32(jsp, child, "pwm_duty_in_scale", 100);
	bconf->pwm_mapping_method =
		json_get_obj_u32(jsp, child, "pwm_mapping_method", PWM_MAP_RESCALING);

	str = json_get_obj_str(jsp, child, "bl_pinmux_sel", "invalid");
	if (str)
		strscpy(bconf->pinmux_name, str, BL_NAME_MAX);
	bl_gpio_probe(bdrv, bconf->en_gpio);

	if (bconf->method == BL_CTRL_LOCAL_DIMMING) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->index == 0) {
			bconf->ldim_flag = 1;
			return aml_ldim_get_config_json(bdrv->index);
		} else {
			return -1;
		}
#else
		BLERR(0, "%s not support ldim\n", __func__);
		return -1;
#endif
	}

//pwms
	if (bconf->method != BL_CTRL_PWM &&
	    bconf->method != BL_CTRL_PWM_COMBO &&
	    bconf->method != BL_CTRL_PWM_ARRAY)
		return 0;

	child = json_get_object_child(jsp, child, "pwms");
	if (!child) {
		BLERR(0, "failed find pwms\n");
		return -1;
	}
	cnt = json_get_array_size(jsp, child);
	cnt = bl_s32_constraint(cnt, 0, 4);
	for (i = 0; i < cnt; i++) {
		child2 = json_get_array_child(jsp, child, i);
		if (!child2) {
			BLPR(PR_COMMON, "fail find pwm[%d]\n", i);
			for (i--; i >= 0; i--) {
				kfree(pwms[i]);
				pwms[i] = NULL;
			}
			return -1;
		}
		if (bl_debug_print_flag & BL_DBG_PR_NORMAL)
			BLPR(bdrv->index, "load pwm[%d]", i);

		pwms[i] = kzalloc(sizeof(*bl_pwm), GFP_KERNEL);
		if (!pwms[i]) {
			BLPR(bdrv->index, "error malloc bl_pwm");
			for (i--; i >= 0; i--) {
				kfree(pwms[i]);
				pwms[i] = NULL;
			}
			return -1;
		}
		if (bl_debug_print_flag & BL_DBG_PR_NORMAL)
			BLPR(bdrv->index, "malloc bl_pwm[%d] ok", i);
		memset(pwms[i], 0, sizeof(*bl_pwm));

		bl_pwm = pwms[i];
		bl_pwm->drv_index = bdrv->index;
		bl_pwm->index = i;

		str = json_get_obj_str(jsp, child2, "port", NULL);
		bl_pwm->pwm_port      = bl_pwm_str_to_num(str ? str : "invalid");
		bl_pwm->pwm_method    = json_get_obj_u32(jsp, child2, "polarity", 1);
		bl_pwm->pwm_phase     = json_get_obj_u32(jsp, child2, "phase", 0);
		bl_pwm->pwm_freq      = json_get_obj_u32(jsp, child2, "freq", 180);

		if (bl_pwm->pwm_freq > XTAL_HALF_FREQ_HZ)
			bl_pwm->pwm_freq = XTAL_HALF_FREQ_HZ;

		child3 = json_get_object_child(jsp, child2, "level_range");
		if (!child3)
			BLPR(bdrv->index, "failed find pwms[%d]/level_range", i);
		val = json_get_arr_u32(jsp, child3, 0, 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->level_min = bconf->level_min;
		else
			bl_pwm->level_min = bl_bri_level_input_scale(bdrv, val);
		val = json_get_arr_u32(jsp, child3, 1, 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->level_max = bconf->level_max;
		else
			bl_pwm->level_max = bl_bri_level_input_scale(bdrv, val);

		child3 = json_get_object_child(jsp, child2, "duty_range");
		if (!child3)
			BLPR(PR_COMMON, "failed find pwms[%d]/level_range\n", i);
		val = json_get_arr_u32(jsp, child3, 0, 0);
		bl_pwm->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, val);
		val = json_get_arr_u32(jsp, child3, 1, bconf->pwm_duty_in_scale);
		bl_pwm->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, val);
		val = json_get_obj_u32(jsp, child2, "duty", 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->pwm_duty_dft = bl_pwm->pwm_duty_min;
		else
			bl_pwm->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);
		bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
		bl_pwm->pwm_duty_save = bl_pwm->pwm_duty_dft;
		bl_pwm_config_init(bl_pwm);
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = pwms[0];
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = pwms[0];
		bconf->bl_pwm_combo1 = pwms[1];
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++)
			bconf->bl_pwm_array[i] = pwms[i];
		break;
	default:
		break;
	}

	bl_pwm_switch_init(bdrv);

	return 0;
}

static int bl_str_to_pwm_port(const char *str)
{
	char *start;

	start = strchr(str, 'P');
	if (!start)
		return BL_PWM_MAX;

	return bl_pwm_str_to_num(start);
}

static int bl_config_load_from_ini(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
	void *inip, *psec;
	const char *str;
	unsigned int val, version, i;

	inip = get_lcd_ini_parse_mem(bdrv->index);
	if (!inip) {
		BLERR(bdrv->index, "%s: parse_mem not ready", __func__);
		return -1;
	}

	psec = lcd_ini_get_section(inip, "Backlight_Attr");
	if (!psec) {
		BLERR(bdrv->index, "%s: not find Backlight_Attr", __func__);
		return -1;
	}
	version = lcd_ini_get_val(inip, psec, "version", 0);

	str = lcd_ini_get_str(inip, psec, "bl_name", "null");
	strscpy(bconf->name, str, sizeof(bconf->name));

	str = lcd_ini_get_str(inip, psec, "bl_pinmux_sel", "invalid");
	strscpy(bconf->pinmux_name, str, BL_NAME_MAX);

	bdrv->step_on_flag = lcd_ini_get_val(inip, psec, "bl_step_on_flag", 0);

	bconf->level_in_scale = lcd_ini_get_val(inip, psec, "bl_level_in_scale", 255);

	str = lcd_ini_get_str(inip, psec, "bl_method", "null");
	bconf->method = bl_ctrl_method_str2num(str);

	bconf->en_gpio = lcd_ini_get_val(inip, psec, "bl_en_gpio", 0xff);
	bconf->en_gpio_on = lcd_ini_get_val(inip, psec, "bl_en_gpio_on", 0);
	bconf->en_gpio_off = lcd_ini_get_val(inip, psec, "bl_en_gpio_off", 0);
	bconf->power_on_delay = lcd_ini_get_val(inip, psec, "bl_on_delay", 0);
	bconf->power_off_delay = lcd_ini_get_val(inip, psec, "bl_off_delay", 0);
	bl_gpio_probe(bdrv, bconf->en_gpio);

	bconf->en_sequence_reverse = lcd_ini_get_val(inip, psec, "bl_custome_val_0", 0);
	bconf->bl_pwm_switch_port = lcd_ini_get_val(inip, psec, "bl_custome_val_1", 0);
	bconf->bl_pwm_switch_freq = lcd_ini_get_val(inip, psec, "bl_custome_val_2", 0);
	bconf->bl_hold_on = lcd_ini_get_val(inip, psec, "bl_custome_val_3", 0);
	//val = lcd_ini_get_val(inip, psec, "bl_custome_val_4", 0);

	bconf->pwm_duty_in_scale =
		lcd_ini_get_val(inip, psec, "pwm_duty_in_scale", 100);
	bconf->pwm_mapping_method =
		lcd_ini_get_val(inip, psec, "pwm_mapping_method", PWM_MAP_RESCALING);

	if (lcd_ini_get_val(inip, psec, "bl_ldim_row", 0) > 0 &&
	    lcd_ini_get_val(inip, psec, "bl_ldim_col", 0) > 0)
		bconf->ldim_flag = 1;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bconf->bl_pwm = kzalloc(sizeof(*bconf->bl_pwm), GFP_KERNEL);
		if (!bconf->bl_pwm)
			return -1;
		bl_pwm = bconf->bl_pwm;
		bl_pwm->index = 0;
		bl_pwm->drv_index = bdrv->index;

		bl_pwm->level_max = bconf->level_max;
		bl_pwm->level_min = bconf->level_min;

		str = lcd_ini_get_str(inip, psec, "pwm_method", "null");
		bl_pwm->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm_port", "null");
		bl_pwm->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm_freq", 0);
		if (bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm->pwm_freq = val & 0xff;
			bl_pwm->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			bl_pwm->pwm_freq = val;
			bl_pwm->pwm_phase = 0;
		}

		val = lcd_ini_get_val(inip, psec, "pwm_level_max", 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->level_max = bconf->level_max;
		else
			bl_pwm->level_max = bl_bri_level_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_level_min", 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->level_min = bconf->level_min;
		else
			bl_pwm->level_min = bl_bri_level_input_scale(bdrv, val);

		val = lcd_ini_get_val(inip, psec, "pwm_duty_max", 0);
		bl_pwm->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_duty_min", 0);
		bl_pwm->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_duty_dft", 0xffffffff);
		if (val == 0xffffffff)
			bl_pwm->pwm_duty_dft = bl_pwm->pwm_duty_min;
		else
			bl_pwm->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);

		bconf->pwm_on_delay = lcd_ini_get_val(inip, psec, "pwm_on_delay", 0);
		bconf->pwm_off_delay = lcd_ini_get_val(inip, psec, "pwm_off_delay", 0);

		bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
		bl_pwm_config_init(bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bconf->bl_pwm_combo0 = kzalloc(sizeof(*bconf->bl_pwm_combo0), GFP_KERNEL);
		if (!bconf->bl_pwm_combo0)
			return -1;
		bconf->bl_pwm_combo1 = kzalloc(sizeof(*bconf->bl_pwm_combo1), GFP_KERNEL);
		if (!bconf->bl_pwm_combo1) {
			kfree(bconf->bl_pwm_combo0);
			return -1;
		}
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		pwm_combo0->index = 0;
		pwm_combo1->index = 1;
		pwm_combo0->drv_index = bdrv->index;
		pwm_combo1->drv_index = bdrv->index;

		str = lcd_ini_get_str(inip, psec, "pwm_method", "null");
		pwm_combo0->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm_port", "null");
		pwm_combo0->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm_freq", 0);
		if (pwm_combo0->pwm_port == BL_PWM_VS) {
			pwm_combo0->pwm_freq = val & 0xff;
			pwm_combo0->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			pwm_combo0->pwm_freq = val;
			pwm_combo0->pwm_phase = 0;
		}

		val = lcd_ini_get_val(inip, psec, "pwm_level_max", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo0->level_max = bconf->level_max;
		else
			pwm_combo0->level_max = bl_bri_level_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_level_min", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo0->level_min = bconf->level_min;
		else
			pwm_combo0->level_min = bl_bri_level_input_scale(bdrv, val);

		val = lcd_ini_get_val(inip, psec, "pwm_duty_max", 0);
		pwm_combo0->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_duty_min", 0);
		pwm_combo0->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm_duty_dft", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo0->pwm_duty_dft = pwm_combo0->pwm_duty_min;
		else
			pwm_combo0->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);

		str = lcd_ini_get_str(inip, psec, "pwm2_method", "null");
		pwm_combo1->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

		str = lcd_ini_get_str(inip, psec, "pwm2_port", "null");
		pwm_combo1->pwm_port = bl_str_to_pwm_port(str);

		val = lcd_ini_get_val(inip, psec, "pwm2_freq", 0);
		if (pwm_combo1->pwm_port == BL_PWM_VS) {
			pwm_combo1->pwm_freq = val & 0xff;
			pwm_combo1->pwm_phase = (val >> 8) & 0xffffff;
		} else {
			pwm_combo1->pwm_freq = val;
			pwm_combo1->pwm_phase = 0;
		}

		val = lcd_ini_get_val(inip, psec, "pwm2_level_max", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo1->level_max = bconf->level_max;
		else
			pwm_combo1->level_max = bl_bri_level_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm2_level_min", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo1->level_min = bconf->level_min;
		else
			pwm_combo1->level_min = bl_bri_level_input_scale(bdrv, val);

		val = lcd_ini_get_val(inip, psec, "pwm2_duty_max", 0);
		pwm_combo1->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm2_duty_min", 0);
		pwm_combo1->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, val);
		val = lcd_ini_get_val(inip, psec, "pwm2_duty_dft", 0xffffffff);
		if (val == 0xffffffff)
			pwm_combo1->pwm_duty_dft = pwm_combo1->pwm_duty_min;
		else
			pwm_combo1->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);

		bconf->pwm_on_delay = lcd_ini_get_val(inip, psec, "pwm_on_delay", 0);
		bconf->pwm_off_delay = lcd_ini_get_val(inip, psec, "pwm_off_delay", 0);

		pwm_combo0->pwm_duty = pwm_combo0->pwm_duty_dft;
		pwm_combo1->pwm_duty = pwm_combo1->pwm_duty_dft;
		pwm_combo0->pwm_duty_save = pwm_combo0->pwm_duty_dft;
		pwm_combo1->pwm_duty_save = pwm_combo1->pwm_duty_dft;
		bl_pwm_config_init(pwm_combo0);
		bl_pwm_config_init(pwm_combo1);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			bconf->bl_pwm_array[i] = kzalloc(sizeof(*bconf->bl_pwm_array[i]),
				GFP_KERNEL);
			if (!bconf->bl_pwm_array[i]) {
				for (i--; i >= 0; i--)
					kfree(bconf->bl_pwm_array[i]);
				return -1;
			}

			if (bl_debug_print_flag & BL_DBG_PR_NORMAL)
				BLPR(bdrv->index, "malloc bl_pwm[%d] ok", i);

			bconf->bl_pwm_array[i]->index = i;
			bconf->bl_pwm_array[i]->drv_index = bdrv->index;
		}
		bconf->pwm_on_delay = lcd_ini_get_val(inip, psec, "pwm_on_delay", 0);
		bconf->pwm_off_delay = lcd_ini_get_val(inip, psec, "pwm_off_delay", 0);

		for (i = 0; i < 4; i++) {
			bl_pwm = bconf->bl_pwm_array[i];
			bl_pwm->level_max = bconf->level_max;
			bl_pwm->level_min = bconf->level_min;

			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_method", i);
			str = lcd_ini_get_str(inip, psec, pwm_str, "null");
			bl_pwm->pwm_method = bl_str_to_pwm_method(str, BL_PWM_POSITIVE);

			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_port", i);
			str = lcd_ini_get_str(inip, psec, pwm_str, "null");
			bl_pwm->pwm_port = bl_str_to_pwm_port(str);

			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_freq", i);
			val = lcd_ini_get_val(inip, psec, pwm_str, 0);
			if (bl_pwm->pwm_port == BL_PWM_VS) {
				bl_pwm->pwm_freq = val & 0xff;
				bl_pwm->pwm_phase = (val >> 8) & 0xffffff;
			} else {
				bl_pwm->pwm_freq = val;
				bl_pwm->pwm_phase = 0;
			}

			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_level_max", i);
			val = lcd_ini_get_val(inip, psec, pwm_str, 0xffffffff);
			if (val == 0xffffffff)
				bl_pwm->level_max = bconf->level_max;
			else
				bl_pwm->level_max = bl_bri_level_input_scale(bdrv, val);
			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_level_min", i);
			val = lcd_ini_get_val(inip, psec, pwm_str, 0xffffffff);
			if (val == 0xffffffff)
				bl_pwm->level_min = bconf->level_min;
			else
				bl_pwm->level_min = bl_bri_level_input_scale(bdrv, val);

			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_duty_max", i);
			val = lcd_ini_get_val(inip, psec, pwm_str, 0);
			bl_pwm->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, val);
			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_duty_min", i);
			val = lcd_ini_get_val(inip, psec, pwm_str, 0);
			bl_pwm->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, val);
			snprintf(pwm_str, PWM_STR_MAX, "pwm_array%d_duty_dft", i);
			val = lcd_ini_get_val(inip, psec, "pwm_duty_dft", 0xffffffff);
			if (val == 0xffffffff)
				bl_pwm->pwm_duty_dft = bl_pwm->pwm_duty_min;
			else
				bl_pwm->pwm_duty_dft = bl_pwm_duty_input_scale(bdrv, val);

			bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
			bl_pwm->pwm_duty_save = bl_pwm->pwm_duty_dft;
			bl_pwm_config_init(bl_pwm);
		}
		break;
#ifdef CONFIG_AMLOGIC_LCD_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bconf->ldim_flag = 1;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bconf->ldim_flag)
		return aml_ldim_get_config_ini(inip, psec);
#endif

	bl_pwm_switch_init(bdrv);

	return 0;
}

int lcd_bl_check_config_load(struct aml_bl_drv_s *drv)
{
	drv->config_load = panel_config_load_detect(drv->index, __func__);
	if (drv->config_load == LCD_CONFIG_NONE || drv->config_load == LCD_CONFIG_ERR)
		return -1;

	return 0;
}

int bl_config_load(struct aml_bl_drv_s *bdrv, struct platform_device *pdev)
{
	char ukey_name[15];
	phandle pwm_phandle;
	int ret = 0, i;
	unsigned char file_type = PANEL_FILE_INVALID;

	if (!bdrv || !pdev) {
		BLERR(PR_COMMON, "config_load: invalid bdrv or pdev");
		return -1;
	}

	if (bdrv->index == 0)
		sprintf(ukey_name, "backlight");
	else
		sprintf(ukey_name, "backlight%d", bdrv->index);

	switch (bdrv->config_load) {
#ifdef CONFIG_OF
	case LCD_CONFIG_DTS:
		ret = bl_config_load_from_dts(bdrv);
		break;
#endif
	case LCD_CONFIG_FILE:
		file_type = get_lcd_panel_file_type(bdrv->index);
		if (file_type == PANEL_FILE_JSON)
			ret = bl_config_load_from_json(bdrv);
		else if (file_type == PANEL_FILE_INI)
			ret = bl_config_load_from_ini(bdrv);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret)
		return -1;

	bl_config_print(bdrv);

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bdrv->bconf.ldim_flag)
		aml_ldim_probe(pdev);
#endif

	bdrv->res_vsync_irq[0] = platform_get_irq_byname(pdev, "vsync");
	if (!bdrv->res_vsync_irq[0]) {
		ret = -ENODEV;
		BLERR(bdrv->index, "bl_vsync_irq[0] resource error");
		return -1;
	}

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm->pwm_port >= BL_PWM_VS)
			break;
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR(bdrv->index, "%s: not match bl_pwm_config node", __func__);
			return -1;
		}
		ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle, bdrv->bconf.bl_pwm);
		if (ret)
			return -1;
		break;
	case BL_CTRL_PWM_COMBO:
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR(bdrv->index, "%s: not match bl_pwm_config node", __func__);
			return -1;
		}
		if (bdrv->bconf.bl_pwm_combo0->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo0);
			if (ret)
				return -1;
		}
		if (bdrv->bconf.bl_pwm_combo1->pwm_port < BL_PWM_VS) {
			ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
						      bdrv->bconf.bl_pwm_combo1);
			if (ret)
				return -1;
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR(bdrv->index, "%s: not match bl_pwm_config node", __func__);
			return -1;
		}
		for (i = 0; i < 4; i++) {
			if (!bdrv->bconf.bl_pwm_array[i]) {
				BLERR(bdrv->index, "%s: bl_pwm_array[%d] is null", __func__, i);
				continue;
			}
			if (bdrv->bconf.bl_pwm_array[i]->pwm_port < BL_PWM_VS) {
				ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
							      bdrv->bconf.bl_pwm_array[i]);
				if (ret)
					return -1;
			}
		}
		break;
	default:
		break;
	}

	/* switch channel register */
	if (bdrv->bconf.bl_pwm_switch && bdrv->bconf.bl_pwm_switch->pwm_port < BL_PWM_MAX) {
		BLPR(bdrv->index, "bl_pwm_switch_port channel register: %d",
				bdrv->bconf.bl_pwm_switch_port);
		ret = of_property_read_u32(bdrv->dev->of_node, "bl_pwm_config", &pwm_phandle);
		if (ret) {
			BLERR(bdrv->index, "%s: not match bl_pwm_config node", __func__);
			return -1;
		}
		ret = bl_pwm_channel_register(bdrv->dev, pwm_phandle,
					      bdrv->bconf.bl_pwm_switch);
		if (ret)
			return -1;

		bdrv->bconf.bl_pwm_switch_flag = 0;
		bdrv->state &= ~BL_STATE_PWM_SWITCH;
	}

	ret = bl_config_load_post(bdrv);
	return ret;
}
