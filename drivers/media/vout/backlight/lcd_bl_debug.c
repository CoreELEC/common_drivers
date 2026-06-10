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
#include <linux/amlogic/media/vout/backlight/backlight.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/backlight/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#endif
#include "lcd_bl.h"
// #include "../lcd_common.h"

static const char *bl_debug_usage_str = {
"Usage:\n"
"    cat status ; dump backlight config\n"
"\n"
"  echo freq <index> <pwm_freq> > pwm ; set pwm freq(Hz for pwm, vfreq multiple for pwm_vs)\n"
"  echo duty <index> <pwm_duty> > pwm ; set pwm duty cycle\n"
"  echo pol <index> <pwm_pol> > pwm ; set pwm polarity\n"
"  echo max <index> <duty_max> > pwm ; set pwm duty_max\n"
"  echo min <index> <duty_min> > pwm ; set pwm duty_min\n"
"  cat pwm_info ; dump pwm state\n"
"  echo free <0|1> > pwm ; set pwm_duty_free enable or disable\n"
"\n"
"  echo <0|1> > power ; backlight power ctrl\n"
"  cat power ; print backlight power state\n"
};

static ssize_t bl_debug_help(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", bl_debug_usage_str);
}

static ssize_t bl_status_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_config_s *bconf;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
	ssize_t len = 0;

	if (!bdrv) {
		BLPR(PR_COMMON, "bdrv is null\n");
		return len;
	}

	bconf = &bdrv->bconf;
	len = sprintf(buf, "read backlight status:\n"
		"bl on connector:    0x%02x\n"
		"key_valid:          %d\n"
		"config_load:        %d\n"
		"index:              %d\n"
		"name:               %s\n"
		"state:              0x%x\n"
		"level_in_scale:     %d\n"
		"level_range:        %d~%d\n"
		"level:              %d\n"
		"level_gd:           %d\n"
		"level_brightness:   %d(in:%d)\n"
		"step_on_flag        %d\n"
		"debug_force:        %d\n\n"
		"method:             %s\n"
		"en_gpio:            %s(%d)\n"
		"en_gpio_on:         %d\n"
		"en_gpio_off:        %d\n"
		"power_on_delay:     %d\n"
		"power_off_delay:    %d\n"
		"en_sequence_rev:    %d\n"
		"bl_hold_on:         %d\n\n",
		bdrv->bl_to_connector,
		bdrv->key_valid, bdrv->config_load,
		bdrv->index, bconf->name, bdrv->state,
		bconf->level_in_scale,
		bconf->level_min, bconf->level_max,
		bdrv->level, bdrv->level_gd, bdrv->level_brightness,
		bl_bri_level_output_scale(bdrv, bdrv->level_brightness),
		bdrv->step_on_flag, bdrv->debug_force,
		bl_method_type_to_str(bconf->method),
		(bconf->en_gpio >= BL_GPIO_NUM_MAX) ? "null" :
			bconf->bl_gpio[bconf->en_gpio].name,
		bconf->en_gpio, bconf->en_gpio_on, bconf->en_gpio_off,
		bconf->power_on_delay, bconf->power_off_delay,
		bconf->en_sequence_reverse, bconf->bl_hold_on);

	switch (bconf->method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		len += sprintf(buf + len,
			"pwm_on_off_delay:   %d,%d\n"
			"pwm_duty_in_scale:  %d\n"
			"pwm_mapping_method: %d\n",
			bconf->pwm_on_delay, bconf->pwm_off_delay,
			bconf->pwm_duty_in_scale, bconf->pwm_mapping_method);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLPR(PR_COMMON, "[%d]: ldim is null\n", bdrv->index);
			break;
		}
		if (!ldim_drv->config_print) {
			BLPR(PR_COMMON, "[%d]: ldim_drv->config_print is null\n", bdrv->index);
			break;
		}
		ldim_drv->config_print();
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLPR(PR_COMMON, "[%d]: bext is null\n", bdrv->index);
			break;
		}
		if (!bext->config_print) {
			BLPR(PR_COMMON, "[%d]: bext->config_print is null\n", bdrv->index);
			break;
		}
		bext->config_print(bext);
		break;
#endif
	default:
		len += sprintf(buf + len, "wrong backlight control method\n");
		break;
	}
	return len;
}

static ssize_t bl_pwm_info_dump(struct aml_bl_drv_s *bdrv,
				char *buf, ssize_t len, size_t size,
				struct bl_pwm_config_s *bl_pwm, const char *tag)
{
	struct pwm_state pstate;

	if (!bl_pwm)
		return len;

	len += scnprintf(buf + len, size - len,
		"%s_index:       %d\n"
		"%s_port:        %s(0x%x)\n"
		"%s_method:      %d\n"
		"%s_freq:        %d\n"
		"%s_level_range: %d~%d(in:%d~%d)\n"
		"%s_duty_range:  %d~%d(in:%d~%d)\n"
		"%s_duty_dft:    %d(in:%d)\n"
		"%s_duty:        %d(in:%d)\n"
		"%s_cnt:         %d\n"
		"%s_value:       %d\n"
		"%s_curve:       %d_%d_%d_%d_%d %d_%d_%d\n"
		"%s_switch_port: %s(0x%x)\n"
		"%s_switch_freq: %d\n",
		tag, bl_pwm->index,
		tag, bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port,
		tag, bl_pwm->pwm_method,
		tag, bl_pwm->pwm_freq,
		tag, bl_pwm->level_min, bl_pwm->level_max,
			bl_bri_level_output_scale(bdrv, bl_pwm->level_min),
			bl_bri_level_output_scale(bdrv, bl_pwm->level_max),
		tag, bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max,
			bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
			bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max),
		tag, bl_pwm->pwm_duty_dft, bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_dft),
		tag, bl_pwm->pwm_duty, bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty),
		tag, bl_pwm->pwm_cnt,
		tag, bl_pwm->pwm_value,
		tag, bl_pwm->pwm_curve.curve_duty[0],
			bl_pwm->pwm_curve.curve_duty[1],
			bl_pwm->pwm_curve.curve_duty[2],
			bl_pwm->pwm_curve.curve_duty[3],
			bl_pwm->pwm_curve.curve_duty[4],
			bl_pwm->pwm_curve.turn_point_duty,
			bl_pwm->pwm_curve.turn_point_level,
			bl_pwm->pwm_curve.en,
		tag, bl_pwm_num_to_str(bdrv->bconf.bl_pwm_switch_port),
			bdrv->bconf.bl_pwm_switch_port,
		tag, bdrv->bconf.bl_pwm_switch_freq);

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		len += scnprintf(buf + len, size - len,
			"%s_phase:      %d\n", tag, bl_pwm->pwm_phase);
		len = bl_pwm_vs_reg_dump(buf, len, size);
		break;
	default:
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			len += scnprintf(buf + len, size - len, "pwm_ch invalid\n");
			break;
		}

		pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
		len += scnprintf(buf + len, size - len,
			"%s state:\n"
			"  period:      %lld\n"
			"  duty_cycle:  %lld\n"
			"  polarity:    %d\n"
			"  enabled:     %d\n",
			tag, pstate.period, pstate.duty_cycle,
			pstate.polarity, pstate.enabled);
		break;
	}

	return len;
}

static ssize_t bl_debug_pwm_info_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	ssize_t len = 0;
	char tag_str[8];
	int i;

	len += scnprintf(buf + len, PAGE_SIZE - len, "pwm info:\n");

	len += scnprintf(buf + len, PAGE_SIZE - len,
		"pwm_bypass:      %d\n"
		"pwm_duty_free:   %d\n",
		bdrv->pwm_bypass,
		bdrv->pwm_duty_free);

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		len = bl_pwm_info_dump(bdrv, buf, len, PAGE_SIZE, bdrv->bconf.bl_pwm, "pwm");
		break;
	case BL_CTRL_PWM_COMBO:
		len = bl_pwm_info_dump(bdrv, buf, len, PAGE_SIZE,
					bdrv->bconf.bl_pwm_combo0, "pwm_0");
		len = bl_pwm_info_dump(bdrv, buf, len, PAGE_SIZE,
					bdrv->bconf.bl_pwm_combo1, "pwm_1");
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			snprintf(tag_str, sizeof(tag_str), "pwm[%d]", i);
			len = bl_pwm_info_dump(bdrv, buf, len, PAGE_SIZE,
				bdrv->bconf.bl_pwm_array[i], tag_str);
		}
		break;
	default:
		break;
	}

	return len;
}

static ssize_t bl_debug_pwm_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;
	int i;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm) {
			bl_pwm = bdrv->bconf.bl_pwm;
			len += sprintf(buf + len,
				"pwm: freq=%d, phase=%d, pol=%d, ",
				bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method);
			len += sprintf(buf + len,
				"duty_max=%d(in:%d), duty_min=%d(in:%d), duty=%d(in:%d)\n",
				bl_pwm->pwm_duty_max,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max),
				bl_pwm->pwm_duty_min,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
				bl_pwm->pwm_duty,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty));
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bdrv->bconf.bl_pwm_combo0) {
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
			len += sprintf(buf + len,
				"pwm_0: freq=%d, phase=%d, pol=%d, ",
				bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method);
			len += sprintf(buf + len,
				"duty_max=%d(in:%d), duty_min=%d(in:%d), duty=%d(in:%d)\n",
				bl_pwm->pwm_duty_max,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max),
				bl_pwm->pwm_duty_min,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
				bl_pwm->pwm_duty,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty));
		}

		if (bdrv->bconf.bl_pwm_combo1) {
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
			len += sprintf(buf + len,
				"pwm_1: freq=%d, phase=%d, pol=%d, ",
				bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method);
			len += sprintf(buf + len,
				"duty_max=%d(in:%d), duty_min=%d(in:%d), duty=%d(in:%d)\n",
				bl_pwm->pwm_duty_max,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max),
				bl_pwm->pwm_duty_min,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
				bl_pwm->pwm_duty,
				bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty));
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			if (bdrv->bconf.bl_pwm_array[i]) {
				bl_pwm = bdrv->bconf.bl_pwm_array[i];
				len += sprintf(buf + len,
					"pwm_%d: freq=%d, phase=%d, pol=%d ",
					i, bl_pwm->pwm_freq, bl_pwm->pwm_phase,
					bl_pwm->pwm_method);
				len += sprintf(buf + len,
					"duty_max=%d(in:%d), duty_min=%d(in:%d), duty=%d(in:%d)\n",
					bl_pwm->pwm_duty_max,
					bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_max),
					bl_pwm->pwm_duty_min,
					bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty_min),
					bl_pwm->pwm_duty,
					bl_pwm_duty_output_scale(bdrv, bl_pwm->pwm_duty));
			}
		}
		break;
	default:
		break;
	}
	return len;
}

#define BL_DEBUG_PWM_FREQ       0
#define BL_DEBUG_PWM_DUTY       1
#define BL_DEBUG_PWM_POL        2
#define BL_DEBUG_PWM_DUTY_MAX   3
#define BL_DEBUG_PWM_DUTY_MIN   4
#define BL_DEBUG_PWM_PHASE      5
static void bl_debug_pwm_set(struct aml_bl_drv_s *bdrv, unsigned int index,
			     unsigned int value, int state)
{
	struct bl_config_s *bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned long flags = 0;

	if (!bdrv)
		return;
	bconf = &bdrv->bconf;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		break;
	case BL_CTRL_PWM_COMBO:
		if (index == 0)
			bl_pwm = bconf->bl_pwm_combo0;
		else
			bl_pwm = bconf->bl_pwm_combo1;
		break;
	case BL_CTRL_PWM_ARRAY:
		if (index < 4) {
			bl_pwm = bconf->bl_pwm_array[index];
		} else {
			BLERR(0, "pwm index(%d) out of range(0-3)\n", index);
			return;
		}
		break;
	default:
		break;
	}
	if (bl_pwm) {
		switch (state) {
		case BL_DEBUG_PWM_FREQ:
			bl_pwm->pwm_freq = value;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) freq: %dHz",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
			break;
		case BL_DEBUG_PWM_DUTY:
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm->pwm_duty = bl_pwm_duty_input_scale(bdrv, value);
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) duty: %d(in:%d)",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_duty, value);
			}
			break;
		case BL_DEBUG_PWM_POL:
			bl_pwm->pwm_method = value;
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) method: %d",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_method);
			}
			break;
		case BL_DEBUG_PWM_DUTY_MAX:
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm->pwm_duty_max = bl_pwm_duty_input_scale(bdrv, value);
			if (bl_pwm->pwm_duty > bl_pwm->pwm_duty_max)
				bl_pwm->pwm_duty = bl_pwm->pwm_duty_max;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) duty_max:%d(in:%d)",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_duty_max, value);
			}
			break;
		case BL_DEBUG_PWM_DUTY_MIN:
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm->pwm_duty_min = bl_pwm_duty_input_scale(bdrv, value);
			if (bl_pwm->pwm_duty < bl_pwm->pwm_duty_min)
				bl_pwm->pwm_duty = bl_pwm->pwm_duty_min;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) duty_min:%d(in:%d)",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_duty_min, value);
			}
			break;
		case BL_DEBUG_PWM_PHASE:
			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bl_pwm->pwm_phase = value;
			bl_pwm_set_duty(bdrv, bl_pwm);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
				BLPR(bdrv->index, "set index(%d) pwm_port(0x%x) phase: %d",
				     index, bl_pwm->pwm_port, bl_pwm->pwm_phase);
			}
			break;
		default:
			break;
		}
	}
}

static ssize_t bl_debug_pwm_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	int step, ret;
	unsigned int index = 0, val = 0;
	int val1 = 0, val2 = 0, val3 = 0, val4 = 0, val5 = 0, val6 = 0, val7 = 0;
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	struct pwm_curve_s *pwm_curve;

	switch (buf[0]) {
	case 'f':
		if (buf[3] == 'q') { /* frequency */
			ret = sscanf(buf, "freq %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_FREQ);
			} else {
				BLERR(0, "invalid parameters\n");
			}
		} else if (buf[3] == 'e') { /* duty free */
			ret = sscanf(buf, "free %d", &val);
			if (ret == 1) {
				bdrv->pwm_duty_free = (unsigned char)val;
				/*BLPR(PR_COMMON, "[d]: set pwm_duty_free: %d\n",
				 *   bdrv->index, bdrv->pwm_duty_free);
				 */
			} else {
				BLERR(0, "invalid parameters\n");
			}
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_DUTY);
		else
			BLERR(0, "invalid parameters\n");
		break;
	case 'p':
		if (buf[1] == 'o') {/* polarity */
			ret = sscanf(buf, "pol %d %d", &index, &val);
			if (ret == 2)
				bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_POL);
			else
				BLERR(0, "invalid parameters\n");
		} else if (buf[1] == 'h') {/* phase */
			ret = sscanf(buf, "pol %d %d", &index, &val);
			if (ret == 2)
				bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_PHASE);
			else
				BLERR(0, "invalid parameters\n");
		}
		break;
	case 'b': /* bypass */
		ret = sscanf(buf, "bypass %d", &val);
		if (ret == 1) {
			bdrv->pwm_bypass = (unsigned char)val;
			BLPR(PR_COMMON, "[%d]: set pwm_bypass: %d\n",
			     bdrv->index, bdrv->pwm_bypass);
		} else {
			BLERR(0, "invalid parameters\n");
		}
		break;
	case 'm':
		if (buf[2] == 'x') { /* max */
			ret = sscanf(buf, "max %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MAX);
			} else {
				BLERR(0, "invalid parameters\n");
			}
		} else if (buf[2] == 'n') { /* min */
			ret = sscanf(buf, "min %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MIN);
			} else {
				BLERR(0, "invalid parameters\n");
			}
		} else if (buf[2] == 'p') { /* mapping_method */
			ret = sscanf(buf, "mapping %d", &val);
			if (ret == 1) {
				bdrv->bconf.pwm_mapping_method = val;
				BLPR(PR_COMMON, "[%d]: set pwm_mapping_method: %d\n",
					bdrv->index, bdrv->bconf.pwm_mapping_method);
			} else {
				BLERR(0, "invalid parameters\n");
			}
		}
		break;
	case 'c': /* curve */
		ret = sscanf(buf, "curve %d %d %d %d %d %d %d %d %d",
			&index, &val, &val1, &val2, &val3, &val4, &val5, &val6, &val7);
		if (ret == 9) {
			switch (bconf->method) {
			case BL_CTRL_PWM:
				bl_pwm = bconf->bl_pwm;
				break;
			case BL_CTRL_PWM_COMBO:
				if (index == 0)
					bl_pwm = bconf->bl_pwm_combo0;
				else
					bl_pwm = bconf->bl_pwm_combo1;
				break;
			case BL_CTRL_PWM_ARRAY:
				if (index < 4) {
					bl_pwm = bconf->bl_pwm_array[index];
				} else {
					BLERR(0, "pwm index(%d) out of range(0-3)\n",
					      index);
					return -EINVAL;
				}
				break;
			default:
				BLERR(0, "not pwm control method\n");
				break;
			}
			if (bl_pwm) {
				pwm_curve = &bl_pwm->pwm_curve;
				pwm_curve->curve_duty[0] = bl_pwm_duty_input_scale(bdrv, val);
				pwm_curve->curve_duty[1] = bl_pwm_duty_input_scale(bdrv, val1);
				pwm_curve->curve_duty[2] = bl_pwm_duty_input_scale(bdrv, val2);
				pwm_curve->curve_duty[3] = bl_pwm_duty_input_scale(bdrv, val3);
				pwm_curve->curve_duty[4] = bl_pwm_duty_input_scale(bdrv, val4);
				pwm_curve->turn_point_duty = bl_pwm_duty_input_scale(bdrv, val5);
				pwm_curve->turn_point_level = bl_bri_level_input_scale(bdrv, val6);
				pwm_curve->en = val7;
				step = (bl_pwm->level_max - bl_pwm->level_min + 2) >> 2;
				pwm_curve->step_level = step;
				pwm_curve->curve_level[0] = bl_pwm->level_min;
				pwm_curve->curve_level[1] = pwm_curve->curve_level[0] + step;
				pwm_curve->curve_level[2] = pwm_curve->curve_level[1] + step;
				pwm_curve->curve_level[3] = pwm_curve->curve_level[2] + step;
				pwm_curve->curve_level[4] = bl_pwm->level_max;
			}
		} else {
			BLERR(0, "invalid parameters\n");
		}
		break;
	default:
		BLERR(0, "wrong command\n");
		break;
	}

	return count;
}

static ssize_t bl_debug_power_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	int pwr_state, real_state;

	if (bdrv->state & BL_STATE_BL_POWER_ON)
		pwr_state = 1;
	else
		pwr_state = 0;
	if (bdrv->state & BL_STATE_BL_ON)
		real_state = 1;
	else
		real_state = 0;
	return sprintf(buf, "backlight power state: %d, real state: %d\n",
		       pwr_state, real_state);
}

static ssize_t bl_debug_power_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR(bdrv->index, "invalid data");
		return -EINVAL;
	}

	BLPR(bdrv->index, "power control: %u", temp);
	if (temp == 0) {
		bdrv->state &= ~BL_STATE_BL_POWER_ON;
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		bdrv->state |= BL_STATE_BL_POWER_ON;
		if ((bdrv->state & BL_STATE_BL_ON) == 0) {
			bl_pwm_ctrl_status_set(bdrv, 1);
			bl_power_on(bdrv);
		}
	}

	return count;
}

static ssize_t bl_debug_step_on_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "backlight step_on: %d\n", bdrv->step_on_flag);
}

static ssize_t bl_debug_step_on_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int temp = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR(0, "invalid data\n");
		return -EINVAL;
	}

	bdrv->step_on_flag = (unsigned char)temp;
	BLPR(bdrv->index, "step_on: %u", bdrv->step_on_flag);

	return count;
}

static ssize_t bl_debug_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "bl power delay: on=%dms, off=%dms\n",
		       bdrv->bconf.power_on_delay,
		       bdrv->bconf.power_off_delay);
}

static ssize_t bl_debug_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int val[2];

	ret = sscanf(buf, "%d %d", &val[0], &val[1]);
	if (ret == 2) {
		bdrv->bconf.power_on_delay = val[0];
		bdrv->bconf.power_off_delay = val[1];
		BLPR(bdrv->index, "set bl power_delay: on=%dms, off=%dms", val[0], val[1]);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

#ifdef BL_BRIGHTNESS_METER
static void bl_brightness_metrics_calc(struct aml_bl_drv_s *bdrv)
{
	struct bl_metrics_config_s *meter_conf = &bdrv->bl_metrics_conf;
	unsigned int j = BL_LEVEL_CNT_MAX;
	unsigned int i = 0;
	unsigned int level_sum = 0;
	unsigned int brightness_sum = 0;
	unsigned int cnt = 0;
	unsigned int temp;

	cnt = meter_conf->cnt;
	temp = meter_conf->times;
	memcpy(&meter_conf->level_buf[j], meter_conf->level_buf,
	       (sizeof(unsigned int) * BL_LEVEL_CNT_MAX));
	memcpy(&meter_conf->brightness_buf[j], meter_conf->brightness_buf,
	       (sizeof(unsigned int) * BL_LEVEL_CNT_MAX));

	for (i = cnt + j; i > (cnt + j - temp); i--) {
		if (bl_debug_print_flag & BL_DBG_PR_ADV) {
			BLPR(PR_COMMON, "cnt: %d, %d: brightness_buf: %d, level_buf: %d\n",
			     cnt, i,
			     meter_conf->brightness_buf[i],
			     meter_conf->level_buf[i]);
		}
		level_sum +=  meter_conf->level_buf[i];
		brightness_sum +=  meter_conf->brightness_buf[i];
	}

	meter_conf->level_metrics = level_sum / temp;
	meter_conf->brightness_metrics = brightness_sum / temp;
}

static ssize_t bl_brightness_metrics_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_metrics_config_s *meter_conf = &bdrv->bl_metrics_conf;

	if (!meter_conf->level_buf)
		return sprintf(buf, "bl_metrics_conf have no level_buf\n");

	bl_brightness_metrics_calc(bdrv);

	return sprintf(buf, "brightness_metrics: %d, level_metrics: %d\n",
		       meter_conf->brightness_metrics,
		       meter_conf->level_metrics);
}

static ssize_t bl_brightness_metrics_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_metrics_config_s *meter_conf = &bdrv->bl_metrics_conf;
	unsigned int temp;
	int ret;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR(0, "invalid data\n");
		return -EINVAL;
	}

	if (!meter_conf->level_buf) {
		BLERR(0, "get no brightness value\n");
		return  -EINVAL;
	}

	if (temp > (BL_LEVEL_CNT_MAX / 60)) {
		BLPR(PR_COMMON, "max support 60min\n");
		meter_conf->times = BL_LEVEL_CNT_MAX;
	} else {
		meter_conf->times = temp * 60;
	}

	bl_brightness_metrics_calc(bdrv);

	BLPR(PR_COMMON, "time: %d, brightness_metrics: %d, level_metrics: %d\n",
	     meter_conf->times,
	     meter_conf->brightness_metrics,
	     meter_conf->level_metrics);

	return count;
}
#endif

static ssize_t bl_debug_level_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "%d(in:%d)\n",
			bdrv->level, bl_bri_level_output_scale(bdrv, bdrv->level));
}

static ssize_t bl_debug_level_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	int level = 0, in_level = 0;
	unsigned long flags = 0;
	int ret;

	ret = kstrtoint(buf, 10, &in_level);
	if (ret != 0) {
		BLERR(0, "invalid data\n");
		return -EINVAL;
	}

	if (!bdrv->debug_force)
		bdrv->debug_force = 1;

	spin_lock_irqsave(&bdrv->isr_lock, flags);
	level = bl_bri_level_input_scale(bdrv, in_level);
	aml_bl_set_level(bdrv, level);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);
	BLPR(bdrv->index, "debug_level: %d(in:%d), state: 0x%x", level, in_level, bdrv->state);

	return count;
}

static ssize_t bl_debug_print_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "bl debug level = 0x%x\n", bl_debug_print_flag);
}

static ssize_t bl_debug_print_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	u32 level = 0;
	int ret;

	ret = kstrtouint(buf, 10, &level);
	if (ret != 0) {
		BLERR(0, "invalid data\n");
		return -EINVAL;
	}

	bl_debug_print_flag = level;

	BLPR(bdrv->index, "debug_print = 0x%x", bl_debug_print_flag);

	return count;
}

static ssize_t bl_debug_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	int val, temp, ret;
	unsigned long flags = 0;

	switch (buf[0]) {
	case 'g': /* gd, full_scale 4095 */
		ret = sscanf(buf, "gd %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;
			if ((bdrv->state & BL_STATE_GD_EN) == 0)
				bl_gd_sel_func(bdrv, 1);
			bl_gd_diming_func(bdrv, val);
		} else {
			BLERR(0, "invalid parameters\n");
		}
		break;
	case 'b': /* brightness */
		ret = sscanf(buf, "brightness %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			spin_lock_irqsave(&bdrv->isr_lock, flags);
			bdrv->level_brightness = bl_bri_level_input_scale(bdrv, val);
			if ((bdrv->state & BL_STATE_GD_EN) == 0)
				temp = bdrv->level_brightness;
			else
				temp = bl_gd_level_map(bdrv, bdrv->level_gd);
			aml_bl_set_level(bdrv, temp);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			BLPR(PR_COMMON, "[%d]: debug brightness: %d(in:%d), state: 0x%x\n",
				bdrv->index, temp, val, bdrv->state);

			if ((bdrv->state & BL_STATE_BL_ON) == 0) {
				bl_pwm_ctrl_status_set(bdrv, 1);
				bl_power_on(bdrv);
			}
		} else {
			BLERR(0, "invalid parameters\n");
		}
		break;
	case 'l': /* level */
		ret = sscanf(buf, "level %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			spin_lock_irqsave(&bdrv->isr_lock, flags);
			temp = bl_bri_level_input_scale(bdrv, val);
			aml_bl_set_level(bdrv, temp);
			spin_unlock_irqrestore(&bdrv->isr_lock, flags);
			BLPR(PR_COMMON, "[%d]: debug level: %d(in:%d), state: 0x%x\n",
				bdrv->index, temp, val, bdrv->state);
		} else {
			BLERR(0, "invalid parameters\n");
		}
		break;
	case 'o': /* off */
		bdrv->debug_force = 0;
		if (bdrv->state & BL_STATE_GD_EN) {
			temp = 0;
			aml_bl_atomic_notifier_call_chain(BACKLIGHT_EVENT_GD_SEL, &temp);
		}
		bdrv->state &= ~BL_STATE_DEBUG_FORCE_EN;
		break;
	default:
		BLERR(0, "wrong command\n");
		break;
	}

	return count;
}

static ssize_t bl_pwm_prot_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_pwm_config_s *bl_pwm = NULL;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		bl_pwm = bdrv->bconf.bl_pwm;
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm = bdrv->bconf.bl_pwm_combo0;
		break;
	case BL_CTRL_PWM_ARRAY:
		BLERR(0, "pwm port switch not support for pwm array\n");
		return -EINVAL;
	default:
		break;
	}

	if (!bl_pwm) {
		BLERR(0, "%s, bl_pwm is NULL\n", __func__);
		return -EINVAL;
	}

	return sprintf(buf, "current pwm port: %d, pwm freq: %d\n",
				bl_pwm->pwm_port, bl_pwm->pwm_freq);
}

static ssize_t bl_pwm_port_switch_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int ret, switch_state;

	ret = sscanf(buf, "switch_state %d", &switch_state);
	if (ret == 1)
		aml_bl_atomic_notifier_call_chain(BACKLIGHT_EVENT_PWM_SWITCH, &switch_state);
	else
		BLERR(0, "invalid parameters\n");

	return count;
}

static struct device_attribute bl_debug_attrs[] = {
	__ATTR(help, 0444, bl_debug_help, NULL),
	__ATTR(status, 0444, bl_status_show, NULL),
	__ATTR(pwm_info, 0444, bl_debug_pwm_info_show, NULL),
	__ATTR(pwm, 0644, bl_debug_pwm_show, bl_debug_pwm_store),
	__ATTR(power_on, 0644, bl_debug_power_show, bl_debug_power_store),
	__ATTR(step_on, 0644, bl_debug_step_on_show, bl_debug_step_on_store),
	__ATTR(delay, 0644, bl_debug_delay_show, bl_debug_delay_store),
#ifdef BL_BRIGHTNESS_METER
	__ATTR(brightness_metrics, 0644, bl_brightness_metrics_show,
	       bl_brightness_metrics_store),
#endif
	__ATTR(debug_level, 0644, bl_debug_level_show, bl_debug_level_store),
	__ATTR(debug_print, 0644, bl_debug_print_show, bl_debug_print_store),
	__ATTR(debug, 0644, bl_debug_help, bl_debug_store),
	__ATTR(switch_pwm_port, 0644, bl_pwm_prot_show, bl_pwm_port_switch_store),
};

int bl_debug_file_creat(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++) {
		if (device_create_file(bdrv->dev, &bl_debug_attrs[i])) {
			BLERR(0, "[%d]: create debug attribute %s fail\n",
			       bdrv->index, bl_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

int bl_debug_file_remove(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++)
		device_remove_file(bdrv->dev, &bl_debug_attrs[i]);

	return 0;
}
