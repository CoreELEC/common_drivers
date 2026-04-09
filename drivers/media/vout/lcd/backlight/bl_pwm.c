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
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_bl.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

static struct bl_pwm_init_config_s pwm_init_cfg = {
	.pwm_vs_reg = NULL,
	.pwm_vs_reg_cnt = 0,
	.pwm_vs_flag = 0,
};

static char *bl_pwm_name[] = {
	"PWM_A",
	"PWM_B",
	"PWM_C",
	"PWM_D",
	"PWM_E",
	"PWM_F",
	"PWM_G",
	"PWM_H",
	"PWM_I",
	"PWM_J",
	"PWM_K",
	"PWM_L",
	"PWM_M",
	"PWM_N"
};

static char *bl_pwm_ao_name[] = {
	"PWM_AO_A",
	"PWM_AO_B",
	"PWM_AO_C",
	"PWM_AO_D",
	"PWM_AO_E",
	"PWM_AO_F",
	"PWM_AO_G",
	"PWM_AO_H"
};

static char *bl_pwm_vs_name[] = {"PWM_VS"};
static char *bl_pwm_invalid_name[] = {"invalid"};

enum bl_pwm_port_e bl_pwm_str_to_num(const char *str)
{
	enum bl_pwm_port_e pwm_port = BL_PWM_MAX;
	int i, cnt;

	cnt = ARRAY_SIZE(bl_pwm_name);
	for (i = 0; i < cnt; i++) {
		if (strcmp(str, bl_pwm_name[i]) == 0) {
			pwm_port = i + BL_PWM_A;
			return pwm_port;
		}
	}

	cnt = ARRAY_SIZE(bl_pwm_ao_name);
	for (i = 0; i < cnt; i++) {
		if (strcmp(str, bl_pwm_ao_name[i]) == 0) {
			pwm_port = i + BL_PWM_AO_A;
			return pwm_port;
		}
	}

	if (strcmp(str, bl_pwm_vs_name[0]) == 0) {
		pwm_port = BL_PWM_VS;
		return pwm_port;
	}

	return BL_PWM_MAX;
}

char *bl_pwm_num_to_str(unsigned int num)
{
	unsigned int temp, cnt;

	if (num < BL_PWM_AO_A) {
		temp = num - BL_PWM_A;
		cnt = ARRAY_SIZE(bl_pwm_name);
		if (temp >= cnt)
			return bl_pwm_invalid_name[0];
		return bl_pwm_name[temp];
	} else if (num < BL_PWM_VS) {
		temp = num - BL_PWM_AO_A;
		cnt = ARRAY_SIZE(bl_pwm_ao_name);
		if (temp >= cnt)
			return bl_pwm_invalid_name[0];
		return bl_pwm_ao_name[temp];
	} else if (num == BL_PWM_VS) {
		return bl_pwm_vs_name[0];
	}

	return bl_pwm_invalid_name[0];
}

int bl_str_to_pwm_method(const char *str, int def_val)
{
	if (strcmp(str, "BL_PWM_NEGATIVE") == 0)
		return BL_PWM_NEGATIVE;
	else if (strcmp(str, "BL_PWM_POSITIVE") == 0)
		return BL_PWM_POSITIVE;
	else
		return def_val;
}

unsigned int bl_pwm_duty_input_scale(struct aml_bl_drv_s *bdrv, unsigned int duty)
{
	unsigned int out_duty, half;
	unsigned long long temp = BL_PWM_DUTY_FULL_SCALE;

	if (!bdrv || bdrv->bconf.pwm_duty_in_scale == 0)
		return 0;

	half = (bdrv->bconf.pwm_duty_in_scale + 1) >> 1;
	out_duty = lcd_do_div(duty * temp + half, bdrv->bconf.pwm_duty_in_scale);
	return out_duty;
}

unsigned int bl_pwm_duty_output_scale(struct aml_bl_drv_s *bdrv, unsigned int duty)
{
	unsigned int in_duty, half = (BL_PWM_DUTY_FULL_SCALE + 1) >> 1;
	unsigned long long temp;

	if (!bdrv || bdrv->bconf.pwm_duty_in_scale == 0)
		return 0;

	temp = bdrv->bconf.pwm_duty_in_scale;
	in_duty = lcd_do_div(duty * temp + half, BL_PWM_DUTY_FULL_SCALE);
	return in_duty;
}

static int bl_level_to_pwm_duty(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm,
				int level)
{
	int lmin, lmax, dmin, dmax;
	int half, duty;
	unsigned long long temp;

	lmin = bl_pwm->level_min;
	lmax = bl_pwm->level_max;
	if (level < lmin || level > lmax || lmin >= lmax)
		return -1; //invalid

	if (bdrv->bconf.pwm_mapping_method == PWM_MAP_NORMALIZATION) {
		dmin = 0;
		dmax = BL_PWM_DUTY_FULL_SCALE;
	} else { //PWM_MAP_RESCALING
		dmin = bl_pwm->pwm_duty_min;
		dmax = bl_pwm->pwm_duty_max;
	}
	if (dmin > dmax)
		return -1;
	half = (lmax - lmin + 1) >> 1;
	temp = dmax - dmin;
	duty = lcd_do_div(temp * (level - lmin) + half, lmax - lmin) + dmin;
	return duty;
}

void bl_pwm_duty_to_pwm_value(struct bl_pwm_config_s *bl_pwm)
{
	unsigned int half = (BL_PWM_DUTY_FULL_SCALE + 1) >> 1;
	unsigned long long temp;

	temp = bl_pwm->pwm_cnt;
	bl_pwm->pwm_value = lcd_do_div((temp * bl_pwm->pwm_duty + half), BL_PWM_DUTY_FULL_SCALE);
}

/* ****************************************************** */
ssize_t bl_pwm_vs_reg_dump(char *buf, ssize_t len, size_t size)
{
	len += scnprintf(buf + len, size - len,
			"pwm_vs_reg0: 0x%08x\n"
			"pwm_vs_reg1: 0x%08x\n"
			"pwm_vs_reg2: 0x%08x\n"
			"pwm_vs_reg3: 0x%08x\n",
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[0]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[1]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[2]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[3]));
	if (pwm_init_cfg.pwm_vs_flag) {
		lcd_vcbus_setb(pwm_init_cfg.pwm_vs_reg[4], 1, 31, 1);
		len += scnprintf(buf + len, size - len,
			"pwm_vs_reg4: 0x%08x\n"
			"pwm_vs_reg5: 0x%08x\n"
			"pwm_vs_reg6: 0x%08x\n"
			"pwm_vs_reg7: 0x%08x\n",
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[0]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[1]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[2]),
			lcd_vcbus_read(pwm_init_cfg.pwm_vs_reg[3]));
		lcd_vcbus_setb(pwm_init_cfg.pwm_vs_reg[4], 0, 31, 1);
	}
	return len;
}

static int bl_pwm_out_level_check(struct bl_pwm_config_s *bl_pwm)
{
	int out_level = 0xff;

	switch (bl_pwm->pwm_duty) {
	case 0:
		out_level = (bl_pwm->pwm_method == BL_PWM_NEGATIVE) ? 1 : 0;
		break;
	case BL_PWM_DUTY_FULL_SCALE:
		out_level = (bl_pwm->pwm_method == BL_PWM_NEGATIVE) ? 0 : 1;
		break;
	default:
		break;
	}

	return out_level;
}

static void bl_set_pwm_vs(struct bl_pwm_config_s *bl_pwm,
			  unsigned int pol, unsigned int out_level)
{
	unsigned int pwm_hi, n, sw;
	unsigned int reg[5], vs[8], ve[8];
	int i;

	if (out_level == 0) {
		for (i = 0; i < 8; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0;
		}
	} else if (out_level == 1) {
		for (i = 0; i < 8; i++) {
			vs[i] = 0;
			ve[i] = 0x1fff;
		}
	} else {
		pwm_hi = bl_pwm->pwm_value;
		n = bl_pwm->pwm_freq;
		sw = (bl_pwm->pwm_cnt * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi * 10 / n + 5) / 10;
		pwm_hi = (pwm_hi > 1) ? pwm_hi : 1;
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("pwm_vs: n=%d, sw=%d, pwm_high=%d, pwm_phase=%d\n",
			     n, sw, pwm_hi, bl_pwm->pwm_phase);
		}
		for (i = 0; i < n; i++) {
			vs[i] = 1 + (sw * i) + bl_pwm->pwm_phase;
			ve[i] = vs[i] + pwm_hi - 1;
		}
		for (i = n; i < 8; i++) {
			vs[i] = 0x1fff;
			ve[i] = 0x1fff;
		}
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		for (i = 0; i < 8; i++) {
			BLPR("pwm_vs: vs[%d]=%d, ve[%d]=%d\n",
			     i, vs[i], i, ve[i]);
		}
	}

	if (pwm_init_cfg.pwm_vs_reg_cnt < 5) {
		BLERR("%s: pwm_vs reg error\n", __func__);
		return;
	}
	for (i = 0; i < pwm_init_cfg.pwm_vs_reg_cnt; i++)
		reg[i] = pwm_init_cfg.pwm_vs_reg[i];

	lcd_vcbus_write(reg[0], (pol << 31) |
		       (2 << 14) | /* vsync latch */
		       (ve[0] << 16) | (vs[0]));
	lcd_vcbus_write(reg[1], (ve[1] << 16) | (vs[1]));
	lcd_vcbus_write(reg[2], (ve[2] << 16) | (vs[2]));
	lcd_vcbus_write(reg[3], (ve[3] << 16) | (vs[3]));
	if (pwm_init_cfg.pwm_vs_flag) {
		lcd_vcbus_setb(reg[4], 1, 31, 1);
		lcd_vcbus_setb(reg[0], vs[4], 0, 13);
		lcd_vcbus_setb(reg[0], ve[4], 16, 13);
		lcd_vcbus_write(reg[1], (ve[5] << 16) | (vs[5]));
		lcd_vcbus_write(reg[2], (ve[6] << 16) | (vs[6]));
		lcd_vcbus_write(reg[3], (ve[7] << 16) | (vs[7]));
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("VPU_VPU_PWM_V4=0x%08x\n", lcd_vcbus_read(reg[0]));
			BLPR("VPU_VPU_PWM_V5=0x%08x\n", lcd_vcbus_read(reg[1]));
			BLPR("VPU_VPU_PWM_V6=0x%08x\n", lcd_vcbus_read(reg[2]));
			BLPR("VPU_VPU_PWM_V7=0x%08x\n", lcd_vcbus_read(reg[3]));
		}
		lcd_vcbus_setb(reg[4], 0, 31, 1);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("VPU_VPU_PWM_V0=0x%08x\n", lcd_vcbus_read(reg[0]));
		BLPR("VPU_VPU_PWM_V1=0x%08x\n", lcd_vcbus_read(reg[1]));
		BLPR("VPU_VPU_PWM_V2=0x%08x\n", lcd_vcbus_read(reg[2]));
		BLPR("VPU_VPU_PWM_V3=0x%08x\n", lcd_vcbus_read(reg[3]));
	}
}

static inline void bl_pwm_normal_state_print(struct pwm_state *state)
{
	BLPR("pwm state: polarity=%d, duty_cycle=%lld, period=%lld, enabled=%d\n",
	     state->polarity, state->duty_cycle,
	     state->period, state->enabled);
}

static void bl_set_pwm_normal(struct bl_pwm_config_s *bl_pwm,
			      unsigned int pol, unsigned int out_level)
{
	if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
		BLERR("%s: invalid bl_pwm_ch\n", __func__);
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("pwm: pwm=0x%p, port=%d, meson_index=%d\n",
		     bl_pwm->pwm_data.pwm, bl_pwm->pwm_port,
		     bl_pwm->pwm_data.meson_index);
	}

	bl_pwm->pwm_data.state.polarity = pol;
	bl_pwm->pwm_data.state.duty_cycle = bl_pwm->pwm_value;
	bl_pwm->pwm_data.state.period =  bl_pwm->pwm_cnt;
	bl_pwm->pwm_data.state.enabled = true;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
		bl_pwm_normal_state_print(&bl_pwm->pwm_data.state);

	pwm_apply_atomic(bl_pwm->pwm_data.pwm, &bl_pwm->pwm_data.state);
}

void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status)
{
	struct pwm_state pstate;
	unsigned int pol = 0, out_level = 0xff;

	if (!bl_pwm)
		return;
	if (bl_pwm->pwm_cnt == 0) {
		BLERR("pwm_ctrl: pwm port %d: pwm_cnt is 0\n", bl_pwm->pwm_port);
		return;
	}
	bl_pwm_duty_to_pwm_value(bl_pwm);

	if (bl_pwm->pwm_method == BL_PWM_NEGATIVE)
		pol = 1;

	if (status) {
		/* enable pwm */
		out_level = bl_pwm_out_level_check(bl_pwm);
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("pwm_ctrl: port %d on: pwm_value=%d, pwm_cnt=%d, out:%d,%s\n",
			     bl_pwm->pwm_port, bl_pwm->pwm_value, bl_pwm->pwm_cnt,
			     out_level, (pol ? "negative" : "positive"));
		}

		switch (bl_pwm->pwm_port) {
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, out_level);
			break;
		default:
			bl_set_pwm_normal(bl_pwm, pol, out_level);
			break;
		}
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("pwm_ctrl: port %d off\n", bl_pwm->pwm_port);

		/* disable pwm */
		switch (bl_pwm->pwm_port) {
		case BL_PWM_VS:
			bl_set_pwm_vs(bl_pwm, pol, 0);
			break;
		default:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				BLERR("%s: invalid bl_pwm_ch\n", __func__);
				return;
			}

			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pstate.duty_cycle = 0;
			pstate.enabled = 0;
			pstate.period = bl_pwm->pwm_data.state.period;
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
				bl_pwm_normal_state_print(&pstate);
			pwm_apply_atomic(bl_pwm->pwm_data.pwm, &(pstate));
			break;
		}
	}
}

static void bl_set_pwm(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm)
{
	bl_pwm->pwm_duty_save = bl_pwm->pwm_duty;

	if (bdrv->state & BL_STATE_BL_ON ||
	    bdrv->state & BL_STATE_BL_INIT_ON || bdrv->state & BL_STATE_PWM_SWITCH) {
		bl_pwm_ctrl(bl_pwm, 1);
	} else {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLERR("%s: bl_drv state is off\n", __func__);
	}
}

static int bl_pwm_duty_check(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm)
{
	if (bdrv->pwm_duty_free) {
		if (bl_pwm->pwm_duty > BL_PWM_DUTY_FULL_SCALE) {
			BLERR("[%d]: pwm_duty_check: pwm port %d: duty %d out of full_scale %d\n",
				bdrv->index, bl_pwm->pwm_port, bl_pwm->pwm_duty,
				BL_PWM_DUTY_FULL_SCALE);
			return -1;
		}
	} else {
		if (bl_pwm->pwm_duty < bl_pwm->pwm_duty_min ||
		    bl_pwm->pwm_duty > bl_pwm->pwm_duty_max) {
			BLERR("[%d]: pwm_duty_check: pwm_port: %d, duty %d out of range [%d~%d]\n",
			      bdrv->index, bl_pwm->pwm_port, bl_pwm->pwm_duty,
			      bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
			return -1;
		}
	}

	return 0;
}

void bl_pwm_set_duty(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm)
{
	if (!bdrv || !bl_pwm)
		return;
	if (bdrv->pwm_bypass)
		return;
	if (bl_pwm_duty_check(bdrv, bl_pwm)) {
		bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("[%d]: pwm_set_duty: pwm port %d: duty=%d [%d~%d]\n",
		     bdrv->index, bl_pwm->pwm_port, bl_pwm->pwm_duty,
		     bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
	}
	bl_set_pwm(bdrv, bl_pwm);
}

static void init_bl_pwm_curve(struct bl_pwm_config_s *bl_pwm)
{
	struct pwm_curve_s *pwm_curve;
	unsigned int step_duty;
	int step_level;

	if (!bl_pwm)
		return;

	pwm_curve = &bl_pwm->pwm_curve;
	step_level = (bl_pwm->level_max - bl_pwm->level_min + 2) >> 2;
	step_duty = (bl_pwm->pwm_duty_max - bl_pwm->pwm_duty_min + 2) >> 2;

	pwm_curve->curve_duty[0] = bl_pwm->pwm_duty_min;
	pwm_curve->curve_duty[1] = pwm_curve->curve_duty[0] + step_duty;
	pwm_curve->curve_duty[2] = pwm_curve->curve_duty[1] + step_duty;
	pwm_curve->curve_duty[3] = pwm_curve->curve_duty[2] + step_duty;
	pwm_curve->curve_duty[4] = bl_pwm->level_max;

	pwm_curve->curve_level[0] = bl_pwm->level_min;
	pwm_curve->curve_level[1] = pwm_curve->curve_level[0] + step_level;
	pwm_curve->curve_level[2] = pwm_curve->curve_level[1] + step_level;
	pwm_curve->curve_level[3] = pwm_curve->curve_level[2] + step_level;
	pwm_curve->curve_level[4] = bl_pwm->level_max;

	pwm_curve->turn_point_duty = bl_pwm->level_max;
	pwm_curve->turn_point_level = bl_pwm->level_max;
	pwm_curve->step_level = step_level;
	pwm_curve->en = 0;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("pwm curve: %d  %d  %d  %d  %d, tp:%d %d en:%d, step:%d %d\n",
			pwm_curve->curve_duty[0], pwm_curve->curve_duty[1],
			pwm_curve->curve_duty[2], pwm_curve->curve_duty[3],
			pwm_curve->curve_duty[4], pwm_curve->turn_point_duty,
			pwm_curve->turn_point_level, pwm_curve->en,
			step_duty, step_level);
}

void bl_pwm_curve_init(struct aml_bl_drv_s *bdrv)
{
	int i;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		init_bl_pwm_curve(bdrv->bconf.bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		init_bl_pwm_curve(bdrv->bconf.bl_pwm_combo0);
		init_bl_pwm_curve(bdrv->bconf.bl_pwm_combo1);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++)
			init_bl_pwm_curve(bdrv->bconf.bl_pwm_array[i]);
		break;
	default:
		break;
	}

	if (bdrv->bconf.bl_pwm_switch) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("pwm switch port %d mapping\n", bdrv->bconf.bl_pwm_switch_port);
		init_bl_pwm_curve(bdrv->bconf.bl_pwm_switch);
	}
}

static int bl_pwm_curve_to_pwm_duty(struct bl_pwm_config_s *bl_pwm, int level)
{
	struct pwm_curve_s *pwm_curve;
	int lmin, lmax, duty_out = 0;
	int l0, l1, l2, l3, l4, tp_level, step, delta;
	unsigned int d0, d1, d2, d3, d4, tp, en = 0;

	lmin = bl_pwm->level_min;
	lmax = bl_pwm->level_max;
	if (level < lmin || level > lmax || lmin >= lmax)
		return -1; //invalid

	pwm_curve = &bl_pwm->pwm_curve;
	d0 = pwm_curve->curve_duty[0];
	d1 = pwm_curve->curve_duty[1];
	d2 = pwm_curve->curve_duty[2];
	d3 = pwm_curve->curve_duty[3];
	d4 = pwm_curve->curve_duty[4];
	l0 = pwm_curve->curve_level[0];
	l1 = pwm_curve->curve_level[1];
	l2 = pwm_curve->curve_level[2];
	l3 = pwm_curve->curve_level[3];
	l4 = pwm_curve->curve_level[4];
	tp = pwm_curve->turn_point_duty;
	tp_level = pwm_curve->turn_point_level;
	step = pwm_curve->step_level;
	en = pwm_curve->en;//1:adim,2:pdim,3:linear

	switch (en) {
	case 1:
	case 2:
		if (d0 == d4) {
			duty_out = d0;
		} else if (tp == d0) {/*adim*/
			step = BL_PWM_DUTY_FULL_SCALE - tp_level;
			delta = (step + 1) >> 1;
			if (level <= tp_level)
				duty_out = d0;
			else
				duty_out =  ((level - tp_level) * (d4 - d0) + delta) / step + d0;
		} else if (tp == d4) {/*pdim*/
			step = tp_level;
			delta = (step + 1) >> 1;
			if (level >= tp_level)
				duty_out = d4;
			else
				duty_out = (level * (d4 - d0) + delta) / tp_level + d0;
		}
		break;
	default: //normal
		delta = (step + 1) >> 1;

		if (level <= l1)
			duty_out = ((d1 - d0) * (level - l0) + delta) / step + d0;
		else if (level <= l2)
			duty_out = ((d2 - d1) * (level - l1) + delta) / step + d1;
		else if (level < l3)
			duty_out = ((d3 - d2) * (level - l2) + delta) / step + d2;
		else
			duty_out = ((d4 - d3) * (level - l3) + delta) / step + d3;
		break;
	}

	if (duty_out > d4)
		duty_out = d4;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
		BLPR("curve: %d %d %d %d %d, tp=%d_%d, levelin=%d, duty_out=%d, en=%d",
			d0, d1, d2, d3, d4, tp, tp_level, level, duty_out, en);

	return duty_out;
}

void bl_pwm_set_level(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm, int level)
{
	int ret;

	if (bdrv->pwm_bypass)
		return;

	if (bl_pwm->pwm_curve.en)
		ret = bl_pwm_curve_to_pwm_duty(bl_pwm, level);
	else
		ret = bl_level_to_pwm_duty(bdrv, bl_pwm, level);
	if (ret < 0)
		return;
	bl_pwm->pwm_duty = ret;
	if (bl_pwm_duty_check(bdrv, bl_pwm)) {
		bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
		return;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("[%d]: pwm_set_level: pwm port %d: level=%d, duty=%d [%d~%d]\n",
			bdrv->index, bl_pwm->pwm_port, level, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
	}

	bl_set_pwm(bdrv, bl_pwm);
}

void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm)
{
	struct aml_lcd_drv_s *pdrv;

	switch (bl_pwm->pwm_port) {
	case BL_PWM_VS:
		pdrv = aml_lcd_get_driver(bl_pwm->drv_index);
		bl_pwm->pwm_cnt = lcd_get_max_line_cnt(pdrv);
		break;
	default:
		/* for pwm api: pwm_period */
		if (bl_pwm->pwm_freq)
			bl_pwm->pwm_cnt = 1000000000 / bl_pwm->pwm_freq;
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("pwm_config_init: pwm_port: %d, freq=%u, pwm_cnt=%u\n",
		     bl_pwm->pwm_port, bl_pwm->pwm_freq, bl_pwm->pwm_cnt);
	}
}

int bl_pwm_channel_register(struct device *dev, phandle pwm_phandle,
			    struct bl_pwm_config_s *bl_pwm)
{
	struct device_node *pnode = NULL;
	struct device_node *child;
	const char *pwm_str;
	unsigned int pwm_port, pwm_index = BL_PWM_MAX;
	struct fwnode_handle *fwnode;
	int ret = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);

	if (!bl_pwm) {
		BLERR("%s: bl_pwm is null\n", __func__);
		return -1;
	}
	pnode = of_find_node_by_phandle(pwm_phandle);
	if (!pnode) {
		BLERR("%s: can't find bl_pwm_config node\n", __func__);
		return -1;
	}

	/*request for pwm device */
	for_each_child_of_node(pnode, child) {
		ret = of_property_read_string(child, "pwm_port", &pwm_str);
		if (ret) {
			BLERR("%s: failed to get pwm_port\n", __func__);
			return -1;
		}
		pwm_port = bl_pwm_str_to_num(pwm_str);
		if (pwm_port >= BL_PWM_VS)
			continue;

		ret = of_property_read_u32_index(child, "pwms", 1, &pwm_index);
		if (ret) {
			BLERR("failed to get meson_pwm_index\n");
			return ret;
		}

		if (pwm_port != bl_pwm->pwm_port)
			continue;

		bl_pwm->pwm_data.meson_index = pwm_index;
		fwnode = of_node_to_fwnode(child); //Convert device node 2 fwnode handle
		bl_pwm->pwm_data.pwm = devm_fwnode_pwm_get(dev, fwnode, NULL);
		fwnode_handle_put(fwnode);

		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
			ret = PTR_ERR(bl_pwm->pwm_data.pwm);
			BLERR("unable to request %s(%d): 0x%x\n", pwm_str, pwm_port, ret);
			continue;
		}
		bl_pwm->pwm_data.meson = to_meson_pwm(bl_pwm->pwm_data.pwm->chip);
		pwm_init_state(bl_pwm->pwm_data.pwm, &bl_pwm->pwm_data.state);
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLPR("register %s(%d) 0x%px\n",
				pwm_str, bl_pwm->pwm_data.meson_index, bl_pwm->pwm_data.pwm);
		}
	}

	return ret;
}

static unsigned int pwm_vs_reg[] = {
	VPU_VPU_PWM_V0,
	VPU_VPU_PWM_V1,
	VPU_VPU_PWM_V2,
	VPU_VPU_PWM_V3,
	VPU_VPU_PWM_H0
};

static unsigned int pwm_vs_reg_t7[] = {
	VPU_VPU_PWM_V0_T7,
	VPU_VPU_PWM_V1_T7,
	VPU_VPU_PWM_V2_T7,
	VPU_VPU_PWM_V3_T7,
	VPU_VPU_PWM_H0_T7
};

int bl_pwm_init_config_probe(struct bl_data_s *bdata)
{
	switch (bdata->chip_type) {
	case LCD_CHIP_AXG:
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_TL1:
	case LCD_CHIP_SM1:
	case LCD_CHIP_TM2:
	case LCD_CHIP_TM2B:
	case LCD_CHIP_T5:
	case LCD_CHIP_T5D:
	case LCD_CHIP_C3:
	case LCD_CHIP_TXHD2:
	case LCD_CHIP_S6:
		pwm_init_cfg.pwm_vs_reg = pwm_vs_reg;
		pwm_init_cfg.pwm_vs_reg_cnt = ARRAY_SIZE(pwm_vs_reg);
		break;
	default:
		pwm_init_cfg.pwm_vs_reg = pwm_vs_reg_t7;
		pwm_init_cfg.pwm_vs_reg_cnt = ARRAY_SIZE(pwm_vs_reg_t7);
		break;
	}

	pwm_init_cfg.pwm_vs_flag = bdata->pwm_vs_flag;

	return 0;
}

struct bl_pwm_init_config_s *get_pwm_init_cfg(void)
{
	return &pwm_init_cfg;
}
