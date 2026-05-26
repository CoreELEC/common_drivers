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
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>

#include <linux/compat.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_BL_EXTERN
#include <linux/amlogic/media/vout/lcd/aml_bl_extern.h>
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#endif
#include "lcd_bl.h"
#include "../lcd_reg.h"
#include "../lcd_common.h"

#include <linux/amlogic/gki_module.h>

#define BL_CDEV_NAME  "aml_bl"
struct bl_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct bl_cdev_s *bl_cdev;
/* for driver global resource init:
 *  0: none
 *  n: initialized cnt
 */
static unsigned char bl_global_init_flag;
static unsigned int bl_drv_init_state;
static struct aml_bl_drv_s *bl_drv[LCD_MAX_DRV];
static int bl_index_lut[LCD_MAX_DRV] = {0xff, 0xff, 0xff};
static int bl_level_bootup[LCD_MAX_DRV] = {-1, -1, -1};

static DEFINE_MUTEX(bl_status_mutex);
static DEFINE_MUTEX(bl_power_mutex);
static DEFINE_MUTEX(bl_level_mutex);

static int aml_bl_check_driver(struct aml_bl_drv_s *bdrv)
{
	int ret = 0, i;

	if (!bdrv) {
		/*BLERR("no bl driver\n");*/
		return -1;
	}
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (!bdrv->bconf.bl_pwm) {
			ret = -1;
			BLERR("no bl_pwm struct\n");
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (!bdrv->bconf.bl_pwm_combo0) {
			ret = -1;
			BLERR("no bl_pwm_combo_0 struct\n");
		}
		if (!bdrv->bconf.bl_pwm_combo1) {
			ret = -1;
			BLERR("no bl_pwm_combo_1 struct\n");
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			if (!bdrv->bconf.bl_pwm_array[i]) {
				ret = -1;
				BLERR("no bl_pwm_array[%d] struct\n", i);
			}
		}
		break;
	case BL_CTRL_MAX:
		ret = -1;
		break;
	default:
		break;
	}

	return ret;
}

struct aml_bl_drv_s *aml_bl_get_driver(int index)
{
	if (index >= LCD_MAX_DRV) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLERR("%s: invalid index: %d\n", __func__, index);
		return NULL;
	}
	if (!bl_drv[index]) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLERR("no bl driver");
		return NULL;
	}

	return bl_drv[index];
}

void bl_gpio_probe(struct aml_bl_drv_s *bdrv, int index)
{
	struct bl_gpio_s *bl_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
			BLPR("gpio %s[%d] is already registered\n",
			     bl_gpio->name, index);
		}
		return;
	}

	/* init gpio flag */
	bl_gpio->probe_flag = 1;
	bl_gpio->register_flag = 0;
}

static int bl_gpio_register(struct aml_bl_drv_s *bdrv, int index, int init_value)
{
	struct bl_gpio_s *bl_gpio;
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (bl_gpio->register_flag) {
		BLPR("%s: gpio %s[%d] is already registered\n",
		     __func__, bl_gpio->name, index);
		return 0;
	}

	switch (init_value) {
	case BL_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case BL_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case BL_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	bl_gpio->gpio = devm_gpiod_get_index(bdrv->dev, "bl", index, value);
	if (IS_ERR(bl_gpio->gpio)) {
		BLERR("register gpio %s[%d]: %p, err: %d\n",
		      bl_gpio->name, index, bl_gpio->gpio,
		      IS_ERR(bl_gpio->gpio));
		return -1;
	}

	bl_gpio->register_flag = 1;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("register gpio %s[%d]: %p, init value: %d\n",
		     bl_gpio->name, index, bl_gpio->gpio, init_value);
	}

	return 0;
}

static void bl_gpio_set(struct aml_bl_drv_s *bdrv, int index, int value)
{
	struct bl_gpio_s *bl_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		BLERR("gpio index %d, exit\n", index);
		return;
	}
	bl_gpio = &bdrv->bconf.bl_gpio[index];
	if (bl_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (bl_gpio->register_flag == 0) {
		bl_gpio_register(bdrv, index, value);
		return;
	}

	if (IS_ERR_OR_NULL(bl_gpio->gpio)) {
		BLERR("gpio %s[%d]: %p, err: %ld\n",
		      bl_gpio->name, index, bl_gpio->gpio,
		      PTR_ERR(bl_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(bl_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(bl_gpio->gpio);
		break;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("set gpio %s[%d] value: %d\n",
		     bl_gpio->name, index, value);
	}
}

/* ****************************************************** */
static char *bl_pinmux_str[] = {
	"none",                 /* 0 */
	"pwm_on",               /* 1 */
	"pwm_vs_on",            /* 2 */
	"pwm_combo_0_1_on",     /* 3 */
	"pwm_combo_0_vs_1_on",  /* 4 */
	"pwm_combo_0_1_vs_on",  /* 5 */
	"pwm_off",              /* 6 */
	"pwm_combo_off",        /* 7 */
	"pwm_array_on",         /* 8 */
	"pwm_array_off"         /* 9 */
};

static void bl_pwm_custom_pinmux_ctrl(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	int index = 0;
	char pinmux_str[BL_NAME_MAX + 5];

	if (status) {
		index = 1;
		sprintf(pinmux_str, "%s", bconf->pinmux_name);
	} else {
		index = 2;
		sprintf(pinmux_str, "%s_off", bconf->pinmux_name);
	}

	if (bdrv->pinmux_flag == index) {
		BLPR("[%d]: pinmux %s(%d) is already selected\n", bdrv->index, pinmux_str, index);
		return;
	}

	/* request pwm pinmux */
	bdrv->pin = devm_pinctrl_get_select(bdrv->dev, pinmux_str);
	if (IS_ERR(bdrv->pin)) {
		BLERR("[%d]: custom_pinmux: %s(%d) error\n", bdrv->index, pinmux_str, index);
		return;
	}

	bdrv->pinmux_flag = index;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: custom_pinmux: %s(%d): %p\n",
			bdrv->index, pinmux_str, index, bdrv->pin);
	}
}

static void bl_pwm_pinmux_set(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	int index = 0;

	if (strcmp(bconf->pinmux_name, "invalid")) {
		bl_pwm_custom_pinmux_ctrl(bdrv, status);
		return;
	}

	switch (bconf->method) {
	case BL_CTRL_PWM:
		if (status) {
			if (bconf->bl_pwm->pwm_port == BL_PWM_VS)
				index = 2;
			else
				index = 1;
		} else {
			index = 6;
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (status) {
			if (bconf->bl_pwm_combo0->pwm_port == BL_PWM_VS) {
				index = 4;
			} else {
				if (bconf->bl_pwm_combo1->pwm_port == BL_PWM_VS)
					index = 5;
				else
					index = 3;
			}
		} else {
			index = 7;
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		index = status ? 8 : 9;
		break;
	default:
		BLERR("[%d]: pinmux_set: wrong ctrl_mothod=%d\n", bdrv->index, bconf->method);
		break;
	}

	if (index == 0) {
		BLERR("[%d]: pinmux_set: pinmux index %d is invalid\n", bdrv->index, index);
		return;
	}

	if (bdrv->pinmux_flag == index) {
		BLPR("[%d]: pinmux_set: %s(%d) is already selected\n",
		     bdrv->index, bl_pinmux_str[index], index);
		return;
	}

	/* request pwm pinmux */
	bdrv->pin = devm_pinctrl_get_select(bdrv->dev, bl_pinmux_str[index]);
	if (IS_ERR(bdrv->pin)) {
		BLERR("[%d]: pinmux_set: %s(%d) error\n",
		      bdrv->index, bl_pinmux_str[index], index);
		return;
	}

	bdrv->pinmux_flag = index;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: pinmux_set: %s(%d): %p\n",
			bdrv->index, bl_pinmux_str[index], index, bdrv->pin);
	}
}

/* ****************************************************** */
static void bl_power_en_ctrl(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;

	if (status) {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_on);
	} else {
		if (bconf->en_gpio < BL_GPIO_NUM_MAX)
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_off);
	}
}

void bl_pwm_ctrl_status_set(struct aml_bl_drv_s *bdrv, int status)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	int i;

	switch (bconf->method) {
	case BL_CTRL_PWM:
		bl_pwm_ctrl(bconf->bl_pwm, status);
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm_ctrl(bconf->bl_pwm_combo0, status);
		bl_pwm_ctrl(bconf->bl_pwm_combo1, status);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++)
			bl_pwm_ctrl(bconf->bl_pwm_array[i], status);
		break;
	default:
		break;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		if (status)
			BLPR("backlight power on pwm first\n");
	}
}

void bl_power_on(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver(bdrv))
		return;

	mutex_lock(&bl_power_mutex);

	if ((bdrv->state & BL_STATE_LCD_ON) == 0) {
		BLPR("%s exit, for lcd is off\n", __func__);
		goto exit_power_on_bl;
	}
	if ((bdrv->state & BL_STATE_BL_POWER_ON) == 0) {
		BLPR("%s exit, for backlight power off\n", __func__);
		goto exit_power_on_bl;
	}

	if (bdrv->state & BL_STATE_BL_ON)
		goto exit_power_on_bl;

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bdrv, 1);
		break;
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				lcd_delay_ms(bconf->pwm_on_delay);
			/* step 2: power on pwm */
			bl_pwm_pinmux_set(bdrv, 1);
		} else {
			/* step 1: power on pwm */
			bl_pwm_pinmux_set(bdrv, 1);
			if (bconf->pwm_on_delay > 0)
				lcd_delay_ms(bconf->pwm_on_delay);
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLERR("no ldim driver\n");
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			/* step 2: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
		} else {
			/* step 1: power on ldim */
			if (ldim_drv->power_on) {
				ret = ldim_drv->power_on();
				if (ret)
					BLERR("ldim: power on error\n");
			} else {
				BLPR("ldim: power on is null\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLERR("[%d]: no bl_extern driver\n", bdrv->index);
			goto exit_power_on_bl;
		}
		if (bconf->en_sequence_reverse) {
			/* step 1: power on enable */
			bl_power_en_ctrl(bdrv, 1);
			/* step 2: power on bl_extern */
			if (bext->power_on) {
				ret = bext->power_on(bext);
				if (ret)
					BLERR("bl_extern: power on error\n");
			}
		} else {
			/* step 1: power on bl_extern */
			if (bext->power_on) {
				ret = bext->power_on(bext);
				if (ret)
					BLERR("bl_extern: power on error\n");
			}
			/* step 2: power on enable */
			bl_power_en_ctrl(bdrv, 1);
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		goto exit_power_on_bl;
	}

	if (bconf->bl_hold_on > 0)
		lcd_delay_ms(bconf->bl_hold_on);

	bdrv->state |= BL_STATE_BL_ON;
	BLPR("backlight power on\n");

exit_power_on_bl:
	mutex_unlock(&bl_power_mutex);
}

void bl_power_off(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret;

	if (aml_bl_check_driver(bdrv))
		return;
	mutex_lock(&bl_power_mutex);

	if ((bdrv->state & BL_STATE_BL_ON) == 0) {
		goto exit_power_off_bl;
		return;
	}

	ret = 0;
	switch (bconf->method) {
	case BL_CTRL_GPIO:
		bl_power_en_ctrl(bdrv, 0);
		break;
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm */
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl_status_set(bdrv, 0);
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off pwm */
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl_status_set(bdrv, 0);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLERR("no ldim driver\n");
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off ldim */
			if (ldim_drv->power_off) {
				ret = ldim_drv->power_off();
				if (ret)
					BLERR("ldim: power off error\n");
			} else {
				BLERR("ldim: power off is null\n");
			}
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLERR("[%d]: no bl_extern driver\n", bdrv->index);
			goto exit_power_off_bl;
		}
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off bl_extern */
			if (bext->power_off) {
				ret = bext->power_off(bext);
				if (ret)
					BLERR("bl_extern: power off error\n");
			}
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off bl_extern */
			if (bext->power_off) {
				ret = bext->power_off(bext);
				if (ret)
					BLERR("bl_extern: power off error\n");
			}
		}
		break;
#endif
	default:
		BLPR("invalid backlight control method\n");
		goto exit_power_off_bl;
		break;
	}
	if (bconf->power_off_delay > 0 &&
		((bdrv->state & BL_STATE_PWM_SWITCH) == 0))
		lcd_delay_ms(bconf->power_off_delay);
	bdrv->state &= ~BL_STATE_BL_ON;
	BLPR("backlight power off\n");

exit_power_off_bl:
	mutex_unlock(&bl_power_mutex);
}

#ifdef CONFIG_AMLOGIC_BL_EXTERN
static void bl_set_level_extern(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	struct bl_extern_driver_s *bext;
	int ret;

	bext = bl_extern_get_driver(bdrv->index);
	if (!bext) {
		BLERR("[%d]: no bl_extern driver\n", bdrv->index);
		return;
	}

	if (bext->set_level) {
		ret = bext->set_level(bext, level);
		if (ret)
			BLERR("bl_ext: set_level error\n");
	} else {
		BLERR("bl_ext: set_level is null\n");
	}
}
#endif

#ifdef CONFIG_AMLOGIC_BL_LDIM
static void bl_set_level_ldim(unsigned int level)
{
	struct aml_ldim_driver_s *ldim_drv;
	int ret = 0;

	ldim_drv = aml_ldim_get_driver();
	if (!ldim_drv) {
		BLERR("no ldim driver\n");
	} else {
		if (ldim_drv->set_level) {
			ret = ldim_drv->set_level(level);
			if (ret)
				BLERR("ldim: set_level error\n");
		} else {
			BLERR("ldim: set_level is null\n");
		}
	}
}
#endif

void aml_bl_set_level(struct aml_bl_drv_s *bdrv, int level)
{
	int new_level = level, i;

	if (aml_bl_check_driver(bdrv))
		return;
	if (level < 0)
		return;

	/* level range check */
	if (level > bdrv->bconf.level_max)
		new_level = bdrv->bconf.level_max;
	else if (level < bdrv->bconf.level_min)
		new_level = bdrv->bconf.level_min;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("[%d]: bl_set_level: level:%d(<-%d), last_level=%u, state=0x%x\n",
		     bdrv->index, new_level, level, bdrv->level, bdrv->state);
	}
	bdrv->level = new_level;

	switch (bdrv->bconf.method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm, new_level);
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm_combo0, new_level);
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm_combo1, new_level);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			if (!bdrv->bconf.bl_pwm_array[i]) {
				if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
					BLERR("pwm_array[%d] is null\n", i);
				continue;
			}
			bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm_array[i], new_level);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bl_set_level_ldim(new_level);
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_set_level_extern(bdrv, new_level);
		break;
#endif
	default:
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("invalid backlight control method\n");
		break;
	}
}

int bl_bri_level_input_scale(struct aml_bl_drv_s *bdrv, int level)
{
	int out_level, half;
	unsigned long long temp = BL_LEVEL_FULL_SCALE;

	if (!bdrv || bdrv->bconf.level_in_scale == 0)
		return 0;
	if (level < 0)
		return level;

	half = (bdrv->bconf.level_in_scale + 1) >> 1;
	out_level = lcd_do_div(level * temp + half, bdrv->bconf.level_in_scale);
	return out_level;
}

int bl_bri_level_output_scale(struct aml_bl_drv_s *bdrv, int level)
{
	int in_level, half = (BL_LEVEL_FULL_SCALE + 1) >> 1;
	unsigned long long temp;

	if (!bdrv || bdrv->bconf.level_in_scale == 0)
		return 0;
	if (level < 0)
		return level;

	temp = bdrv->bconf.level_in_scale;
	in_level = lcd_do_div(level * temp + half, BL_LEVEL_FULL_SCALE);
	return in_level;
}

int bl_gd_level_map(struct aml_bl_drv_s *bdrv, int gd_level)
{
	int max, min, val, half = (BL_LEVEL_FULL_SCALE + 1) >> 1;

	min = bdrv->bconf.level_min;
	max = bdrv->bconf.level_max;

	if (bdrv->level_brightness < 0)
		return gd_level;
	if (bdrv->level_brightness <= min)
		return min;
	val = (gd_level * (bdrv->level_brightness - min) + half) / BL_LEVEL_FULL_SCALE + min;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		if (val != bdrv->level) {
			BLPR("%s: input gd_level=%d, output level=%d [%d~%d]\n",
				__func__, gd_level, val, min, max);
		}
	}

	return val;
}

int aml_bl_set_level_brightness(struct aml_bl_drv_s *bdrv, int brightness)
{
	int level;
	unsigned long flags = 0;

	if (aml_bl_check_driver(bdrv))
		return -1;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return 0;
	}

	if ((bdrv->state & BL_STATE_LCD_ON) == 0 ||
	    (bdrv->state & BL_STATE_BL_POWER_ON) == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: set_level_brightness: backlight is off, state: 0x%x\n",
				bdrv->index, bdrv->state);
		return 0;
	}

	mutex_lock(&bl_status_mutex);

	spin_lock_irqsave(&bdrv->isr_lock, flags);
	bdrv->level_brightness = bl_bri_level_input_scale(bdrv, brightness);
	if ((bdrv->state & BL_STATE_GD_EN) == 0)
		level = bdrv->level_brightness;
	else
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
	aml_bl_set_level(bdrv, level);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);

	if ((bdrv->state & BL_STATE_BL_ON) == 0) {
		bl_pwm_ctrl_status_set(bdrv, 1);
		bl_power_on(bdrv);
	}

	mutex_unlock(&bl_status_mutex);

	return 0;
}

int aml_bl_get_level_brightness(struct aml_bl_drv_s *bdrv)
{
	if (aml_bl_check_driver(bdrv))
		return 0;

	return bl_bri_level_output_scale(bdrv, bdrv->level_brightness);
}

static int aml_bl_update_status(struct backlight_device *bd)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)bl_get_data(bd);

	return aml_bl_set_level_brightness(bdrv, bdrv->bldev->props.brightness);
}

static int aml_bl_get_brightness(struct backlight_device *bd)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)bl_get_data(bd);

	return aml_bl_get_level_brightness(bdrv);
}

static const struct backlight_ops aml_bl_ops = {
	.get_brightness = aml_bl_get_brightness,
	.update_status  = aml_bl_update_status,
};

static void aml_bl_power_init_on(struct aml_bl_drv_s *bdrv)
{
	struct aml_lcd_drv_s *pdrv;
	struct bl_pwm_config_s *bl_pwm, *bl_pwm2;
	int i;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (!pdrv)
		return;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0)) {
		if (pdrv->boot_ctrl->init_level == LCD_INIT_LEVEL_PREBOOT)
			return;
	}

	switch (bdrv->bconf.method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_pwm = bdrv->bconf.bl_pwm;
		if (!bl_pwm)
			return;
		switch (bdrv->step_on_flag) {
		case 1:
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
			break;
		default:
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
			break;
		}
		bl_pwm_set_duty(bdrv, bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm = bdrv->bconf.bl_pwm_combo0;
		bl_pwm2 = bdrv->bconf.bl_pwm_combo1;
		if (!bl_pwm || !bl_pwm2)
			return;
		switch (bdrv->step_on_flag) {
		case 1:
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
			bl_pwm2->pwm_duty = bl_pwm2->pwm_duty_dft;
			break;
		default:
			bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
			bl_pwm2->pwm_duty = bl_pwm2->pwm_duty_save;
			break;
		}
		bl_pwm_set_duty(bdrv, bl_pwm);
		bl_pwm_set_duty(bdrv, bl_pwm2);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			bl_pwm = bdrv->bconf.bl_pwm_array[i];
			if (!bl_pwm) {
				if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
					BLERR("pwm_array[%d] is null\n", i);
				continue;
			}
			switch (bdrv->step_on_flag) {
			case 1:
				bl_pwm->pwm_duty = bl_pwm->pwm_duty_dft;
				break;
			default:
				bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
				break;
			}
			bl_pwm_set_duty(bdrv, bl_pwm);
		}
		break;
	default:
		break;
	}
	if ((bdrv->state & BL_STATE_BL_ON) == 0)
		bl_power_on(bdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: power_init_on, state: 0x%x\n", bdrv->index, bdrv->state);
}

static void bl_on_function(struct aml_bl_drv_s *bdrv)
{
	unsigned long flags = 0;

	mutex_lock(&bl_level_mutex);

	/* lcd power on backlight flag */
	bdrv->state |= BL_STATE_LCD_ON;
	BLPR("[%d]: bl_on_func: bl_step_on_flag=%d, level_brightness=%d(in:%d), state=0x%x\n",
		bdrv->index, bdrv->step_on_flag, bdrv->level_brightness,
		bl_bri_level_output_scale(bdrv, bdrv->level_brightness), bdrv->state);

	bdrv->state |= BL_STATE_BL_INIT_ON;
	switch (bdrv->step_on_flag) {
	case 1:
		BLPR("[%d]: bl_step_on with default level\n", bdrv->index);
		aml_bl_power_init_on(bdrv);

		lcd_delay_ms(120);
		BLPR("[%d]: bl_on level: %d\n", bdrv->index, bdrv->level_brightness);
		spin_lock_irqsave(&bdrv->isr_lock, flags);
		aml_bl_set_level(bdrv, bdrv->level_brightness);
		spin_unlock_irqrestore(&bdrv->isr_lock, flags);
		break;
	case 2:
		BLPR("[%d]: bl_step_on with saved level\n", bdrv->index);
		aml_bl_power_init_on(bdrv);

		lcd_delay_ms(120);
		BLPR("[%d]: bl_on level: %d\n", bdrv->index, bdrv->level_brightness);
		spin_lock_irqsave(&bdrv->isr_lock, flags);
		aml_bl_set_level(bdrv, bdrv->level_brightness);
		spin_unlock_irqrestore(&bdrv->isr_lock, flags);
		break;
	default:
		aml_bl_power_init_on(bdrv);
		break;
	}
	bdrv->state &= ~(BL_STATE_BL_INIT_ON);

	mutex_unlock(&bl_level_mutex);
}

static void bl_delayed_on(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_bl_drv_s *bdrv;

	d_work = container_of(p_work, struct delayed_work, work);
	bdrv = container_of(d_work, struct aml_bl_drv_s, delayed_on_work);

	if (bdrv->probe_done == 0)
		return;
	if (bdrv->on_request == 0)
		return;

	bl_on_function(bdrv);
}

void bl_lcd_on_ctrl(struct aml_lcd_drv_s *pdrv)
{
	struct aml_bl_drv_s *bdrv;
	unsigned long long local_time[2];
	unsigned int delay_ms;

	if (!pdrv)
		return;

	local_time[0] = sched_clock();

	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return;
	if (bdrv->probe_done == 0)
		return;

	if (pdrv->status & LCD_STATUS_BL_PRE_ON) {
		bdrv->pre_on_time = local_time[0];
		bdrv->state |= BL_STATE_PRE_ON;
		BLPR("[%d]: %s: pre_on\n", bdrv->index, __func__);
		return;
	}

	if (pdrv->boot_ctrl->init_level == LCD_INIT_LEVEL_PREBOOT) {
		bdrv->state = BL_STATE_BL_POWER_ON;
		pdrv->boot_ctrl->init_level = LCD_INIT_LEVEL_NORMAL;
	}

	if (bdrv->state & BL_STATE_BL_ON) {
		bdrv->pre_on_time = 0;
		BLPR("[%d]: %s already on\n", bdrv->index, __func__);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s\n", bdrv->index, __func__);

	bdrv->on_request = 1;
	/* lcd power on sequence control */
	bl_pwm_ctrl_status_set(bdrv, 1);
	if (bdrv->pre_on_time) {
		local_time[1] = sched_clock();
		delay_ms = lcd_do_div((local_time[1] - bdrv->pre_on_time), 1000000);
		bdrv->pre_on_time = 0;
		if (delay_ms < bdrv->bconf.power_on_delay)
			delay_ms = bdrv->bconf.power_on_delay - delay_ms;
		else
			delay_ms = 0;
	} else {
		delay_ms = bdrv->bconf.power_on_delay;
	}

	if (bdrv->bconf.method < BL_CTRL_MAX) {
		if (bdrv->state & BL_STATE_PRE_ON) {
			bdrv->state &= ~BL_STATE_PRE_ON;
			lcd_queue_delayed_work(&bdrv->delayed_on_work, delay_ms);
		} else {
			if (delay_ms)
				lcd_delay_ms(delay_ms);
			bl_on_function(bdrv);
		}
	} else {
		BLERR("[%d]: wrong backlight control method\n", bdrv->index);
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.bl_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

void bl_lcd_off_ctrl(struct aml_lcd_drv_s *pdrv)
{
	struct aml_bl_drv_s *bdrv;
	unsigned long long local_time[2];

	if (!pdrv)
		return;
	local_time[0] = sched_clock();

	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return;
	if (bdrv->probe_done == 0)
		return;

	bdrv->pre_on_time = 0;
	bdrv->on_request = 0;
	if (!(bdrv->state & BL_STATE_BL_ON)) {
		BLPR("[%d]: %s already off\n", bdrv->index, __func__);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s\n", bdrv->index, __func__);

	bdrv->state &= ~BL_STATE_LCD_ON;
	mutex_lock(&bl_level_mutex);
	bdrv->state |= BL_STATE_BL_INIT_ON;
	if (bdrv->state & BL_STATE_BL_ON)
		bl_power_off(bdrv);
	bdrv->state &= ~BL_STATE_BL_INIT_ON;
	mutex_unlock(&bl_level_mutex);

	local_time[1] = sched_clock();
	pdrv->proc_time.bl_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

static int bl_power_ctrl_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)data;
	unsigned int temp = 0;

	if (event & LCD_EVENT_BL_POWER_ON)
		temp = 1;
	else if (event & LCD_EVENT_BL_POWER_OFF)
		temp = 0;
	else
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	BLPR("[%d]: power_notifier: %d, state: 0x%x\n", bdrv->index, temp, bdrv->state);

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

	return NOTIFY_OK;
}

static struct notifier_block bl_power_ctrl_nb = {
	.notifier_call = bl_power_ctrl_notifier,
};

static inline int bl_pwm_vs_lcd_update(struct aml_bl_drv_s *bdrv,
				       struct bl_pwm_config_s *bl_pwm)
{
	unsigned int cnt;
	unsigned long flags = 0;

	if (!bl_pwm) {
		BLERR("[%d]: %s: bl_pwm is null\n", bdrv->index, __func__);
		return 0;
	}

	cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
	if (cnt == bl_pwm->pwm_cnt)
		return 0;

	mutex_lock(&bl_level_mutex);
	bl_pwm_config_init(bl_pwm);

	if (bdrv->state & BL_STATE_GD_EN) {
		mutex_unlock(&bl_level_mutex);
		return 0;
	}

	spin_lock_irqsave(&bdrv->isr_lock, flags);
	bdrv->level_brightness = bl_bri_level_input_scale(bdrv, bdrv->bldev->props.brightness);
	aml_bl_set_level(bdrv, bdrv->level_brightness);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);

	mutex_unlock(&bl_level_mutex);

	return 0;
}

static int bl_lcd_update_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned int frame_rate;
	unsigned short hactive, vactive;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_UPDATE)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	frame_rate = pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate;
	hactive = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	vactive = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;

#ifdef BL_BRIGHTNESS_METER
	bdrv->bl_metrics_conf.frame_rate = frame_rate;
#endif
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s for pwm_vs\n", bdrv->index, __func__);
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bdrv->bconf.bl_pwm->pwm_port == BL_PWM_VS) {
			bl_pwm = bdrv->bconf.bl_pwm;
			if (bl_pwm)
				bl_pwm_vs_lcd_update(bdrv, bl_pwm);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bdrv->bconf.bl_pwm_combo0->pwm_port == BL_PWM_VS)
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
		else if (bdrv->bconf.bl_pwm_combo1->pwm_port == BL_PWM_VS)
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
		if (bl_pwm)
			bl_pwm_vs_lcd_update(bdrv, bl_pwm);
		break;
	case BL_CTRL_PWM_ARRAY:
		BLERR("pwm_array not support pwm_vs!!!");
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("[%d]: %s fr = %d, ha=%d, va=%d\n",
			bdrv->index, __func__,
			frame_rate, hactive, vactive);
		ldim_drv->vsync_change_flag = (unsigned char)(frame_rate);
		if (hactive != ldim_drv->conf->hsize ||
			vactive != ldim_drv->conf->vsize) {
			ldim_drv->conf->hsize = hactive;
			ldim_drv->conf->vsize = vactive;
			ldim_drv->resolution_update = 1;
		}

		ldim_drv->trig_param.line_time_ns =
			pdrv->curr_dev->dev_cfg.timing.act_timing.line_time_ns;
		ldim_drv->trig_param.line_total_cnt =
			pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;
		ldim_trig_line_update(ldim_drv);

		if (ldim_drv->pwm_vs_update)
			ldim_drv->pwm_vs_update();
		break;
#endif

	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bl_lcd_update_nb = {
	.notifier_call = bl_lcd_update_notifier,
};

void bl_gd_diming_func(struct aml_bl_drv_s *bdrv, int level)
{
	int level_new;
	unsigned long flags = 0;

	if ((bdrv->state & BL_STATE_GD_EN) == 0)
		return;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    (bdrv->state & BL_STATE_BL_INIT_ON) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_ON) == 0))
		return;
	if (level < 0)
		return;

	/* atomic notifier, can't schedule or sleep */
	spin_lock_irqsave(&bdrv->isr_lock, flags);
	bdrv->level_gd = (level > BL_LEVEL_FULL_SCALE) ? BL_LEVEL_FULL_SCALE : level;
	level_new = bl_gd_level_map(bdrv, bdrv->level_gd);
	aml_bl_set_level(bdrv, level_new);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: gd_dimming: level_gd: %d(in:%d), final level: %d, state: 0x%x\n",
		     bdrv->index, bdrv->level_gd, level, level_new, bdrv->state);
	}
}

static int bl_gd_dimming_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	unsigned int level;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_GD_DIM)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	/* atomic notifier, can't schedule or sleep */
	level = *(unsigned int *)data;
	bl_gd_diming_func(bdrv, level);

	return NOTIFY_OK;
}

static struct notifier_block bl_gd_dimming_nb = {
	.notifier_call = bl_gd_dimming_notifier,
};

int bl_gd_sel_func(struct aml_bl_drv_s *bdrv, unsigned int sel)
{
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: gd_sel: %d\n", bdrv->index, __func__, sel);

	if (sel) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->bconf.method == BL_CTRL_LOCAL_DIMMING) {
			if (ldim_drv->ld_sel_ctrl)
				ldim_drv->ld_sel_ctrl(0);
		}
#endif
		bdrv->state |= BL_STATE_GD_EN;
	} else {
		bdrv->state &= ~BL_STATE_GD_EN;
#ifdef CONFIG_AMLOGIC_BL_LDIM
		if (bdrv->bconf.method == BL_CTRL_LOCAL_DIMMING) {
			if (ldim_drv->ld_sel_ctrl)
				ldim_drv->ld_sel_ctrl(1);
		}
#endif
	}

	return 0;
}

static int bl_gd_sel_notifier(struct notifier_block *nb,
			      unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);

	unsigned int sel;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_GD_SEL)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	sel = *(unsigned int *)data;
	bl_gd_sel_func(bdrv, sel);

	return NOTIFY_OK;
}

static struct notifier_block bl_gd_sel_nb = {
	.notifier_call = bl_gd_sel_notifier,
};

static int bl_brightness_dimming_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int level;
	unsigned long flags = 0;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_BRIGHTNESS_DIM)
		return NOTIFY_DONE;

	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
		(bdrv->state & BL_STATE_BL_INIT_ON) ||
		((bdrv->state & BL_STATE_BL_POWER_ON) == 0) ||
		((bdrv->state & BL_STATE_BL_ON) == 0))
		return NOTIFY_DONE;

	if (bdrv->debug_force) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("[%d]: %s: bypass for debug force\n", bdrv->index, __func__);
		return NOTIFY_DONE;
	}
	if (!data)
		return NOTIFY_DONE;

	level = *(int *)data;
	if (level < 0)
		return NOTIFY_OK;

	/* atomic notifier, can't schedule or sleep */
	spin_lock_irqsave(&bdrv->isr_lock, flags);
	bdrv->bldev->props.brightness = level;
	bdrv->level_brightness = bl_bri_level_input_scale(bdrv, level);
	if ((bdrv->state & BL_STATE_GD_EN) == 0)
		level = bdrv->level_brightness;
	else
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
	aml_bl_set_level(bdrv, level);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);

	return NOTIFY_OK;
}

static struct notifier_block bl_bri_dimming_nb = {
	.notifier_call = bl_brightness_dimming_notifier,
};

/* bl_pwm_port_switch
 * state 1: switch pwm port
 * state 0: default pwm port
 */
static int bl_pwm_switch_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct bl_config_s *bconf = &bdrv->bconf;
	unsigned int state = *(unsigned int *)data;

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_BACKLIGHT_PWM_SWITCH)
		return NOTIFY_DONE;

	if (state > 1) {
		state = 1;
		BLWARN("state value is neither 1 nor 0\n");
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s: %d\n", __func__, state);

	if (bconf->bl_pwm_switch_flag == state) {
		BLWARN("pwm port is already in state %d\n", state);
		return NOTIFY_DONE;
	}

	if (state) {
		if (!bconf->bl_pwm_switch) {
			BLWARN("no switch pwm port\n");
			return NOTIFY_DONE;
		}
	}

	bconf->bl_pwm_switch_flag = state;
	/* choose default pwm or switch pwm*/
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		if (bconf->bl_pwm_switch)
			bconf->bl_pwm_switch->pwm_duty = bconf->bl_pwm->pwm_duty;
		if (bconf->bl_pwm_default)
			bconf->bl_pwm_default->pwm_duty = bconf->bl_pwm->pwm_duty;
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->bl_pwm_switch)
			bconf->bl_pwm_switch->pwm_duty = bconf->bl_pwm_combo0->pwm_duty;
		if (bconf->bl_pwm_default)
			bconf->bl_pwm_default->pwm_duty = bconf->bl_pwm_combo0->pwm_duty;
		break;
	case BL_CTRL_PWM_ARRAY:
		BLERR("pwm_array not support pwm port switch!!!\n");
		break;
	default:
		BLWARN("wrong bl control method\n");
		break;
	}

	if (bconf->bl_pwm_switch_flag) {
		if (!bconf->bl_pwm_switch)
			return NOTIFY_DONE;
		bconf->bl_pwm = bconf->bl_pwm_switch;
		bconf->bl_pwm_combo0 = bconf->bl_pwm_switch;
	} else {
		if (!bconf->bl_pwm_default)
			return NOTIFY_DONE;
		bconf->bl_pwm = bconf->bl_pwm_default;
		bconf->bl_pwm_combo0 = bconf->bl_pwm_default;
	}

	bdrv->state |= BL_STATE_PWM_SWITCH;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s, pwm port switch to state %d\n", __func__, bconf->bl_pwm_switch_flag);

	return NOTIFY_OK;
}

static struct notifier_block bl_pwm_switch_nb = {
	.notifier_call = bl_pwm_switch_notifier,
};

static void bl_notifier_init(void)
{
	int ret;

	ret = aml_lcd_notifier_register(&bl_power_ctrl_nb);
	if (ret)
		BLERR("register bl_power_ctrl_nb failed\n");
	ret = aml_lcd_notifier_register(&bl_lcd_update_nb);
	if (ret)
		BLERR("register bl_lcd_update_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_pwm_switch_nb);
	if (ret)
		BLERR("register bl_pwm_switch_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_gd_dimming_nb);
	if (ret)
		BLERR("register bl_gd_dimming_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_gd_sel_nb);
	if (ret)
		BLERR("register bl_gd_sel_nb failed\n");
	ret = aml_lcd_atomic_notifier_register(&bl_bri_dimming_nb);
	if (ret)
		BLERR("register bl_bri_dimming_nb failed\n");
}

static void bl_notifier_remove(void)
{
	aml_lcd_atomic_notifier_unregister(&bl_gd_sel_nb);
	aml_lcd_atomic_notifier_unregister(&bl_gd_dimming_nb);
	aml_lcd_atomic_notifier_unregister(&bl_bri_dimming_nb);
	aml_lcd_atomic_notifier_unregister(&bl_pwm_switch_nb);
	aml_lcd_notifier_unregister(&bl_lcd_update_nb);
	aml_lcd_notifier_unregister(&bl_power_ctrl_nb);
}

static void bl_pwm_port_switch_update(struct aml_bl_drv_s *bdrv)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (!pdrv)
		return;

	/* default disable lcd & backlight */
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return;

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		bl_pwm_set_duty(bdrv, bdrv->bconf.bl_pwm);
		bl_pwm_pinmux_set(bdrv, 1);
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm_set_duty(bdrv, bdrv->bconf.bl_pwm_combo0);
		bl_pwm_pinmux_set(bdrv, 1);
		break;
	case BL_CTRL_PWM_ARRAY:
		BLERR("pwm_array not support pwm port update!!!\n");
		break;
	default:
		break;
	}
}

static inline void bl_vsync_handler(struct aml_bl_drv_s *bdrv)
{
	struct bl_pwm_config_s *bl_pwm;
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef BL_BRIGHTNESS_METER
	struct bl_metrics_config_s *meter_conf;
#endif
	int level = 0;
	static int backlight_toggled;
	unsigned long flags = 0;

	if ((bdrv->state & BL_STATE_BL_ON) == 0)
		return;

	if (bdrv->state & BL_STATE_PWM_SWITCH) {
		if (!backlight_toggled) {
			/* backlight off */
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_off);
			/* switch pwm */
			switch (bdrv->bconf.method) {
			case BL_CTRL_PWM:
				bl_pwm = bdrv->bconf.bl_pwm;
				break;
			case BL_CTRL_PWM_COMBO:
				bl_pwm = bdrv->bconf.bl_pwm_combo0;
				break;
			case BL_CTRL_PWM_ARRAY:
				BLERR("pwm_array not support pwm port switch!!!\n");
				return;
			default:
				return;
			}
			bl_pwm_config_init(bl_pwm);
			bl_pwm_port_switch_update(bdrv);
		} else {
			/* backlight on */
			bl_gpio_set(bdrv, bconf->en_gpio, bconf->en_gpio_on);
			bdrv->state &= ~BL_STATE_PWM_SWITCH;
		}
		backlight_toggled = !backlight_toggled;
	}

#ifdef BL_BRIGHTNESS_METER
	meter_conf = &bdrv->bl_metrics_conf;
	if (meter_conf->level_buf) {
		if (meter_conf->sum_cnt < meter_conf->frame_rate) {
			meter_conf->level_count += bdrv->level;
			meter_conf->brightness_count += bdrv->level_brightness;
			meter_conf->sum_cnt++;
		} else {
			meter_conf->level_buf[meter_conf->cnt] =
				meter_conf->level_count / meter_conf->frame_rate;
			meter_conf->brightness_buf[meter_conf->cnt] =
				meter_conf->brightness_count / meter_conf->frame_rate;
			meter_conf->cnt++;
			meter_conf->sum_cnt = 0;
			meter_conf->level_count = 0;
			meter_conf->brightness_count = 0;
		}
		if (meter_conf->cnt == BL_LEVEL_CNT_MAX) {
			meter_conf->sum_cnt = 0;
			meter_conf->cnt = 0;
		}
	}
#endif

	if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0) {
		spin_lock_irqsave(&bdrv->isr_lock, flags);
		if ((bdrv->state & BL_STATE_GD_EN) == 0)
			level = bdrv->level_brightness;
		else
			level = bl_gd_level_map(bdrv, bdrv->level_gd);
		if (level != bdrv->level)
			aml_bl_set_level(bdrv, level);
		spin_unlock_irqrestore(&bdrv->isr_lock, flags);
	}
}

static irqreturn_t bl_vsync_isr(int irq, void *data)
{
	struct aml_bl_drv_s *bdrv = (struct aml_bl_drv_s *)data;

	bl_vsync_handler(bdrv);
	return IRQ_HANDLED;
}

static int bl_vsync_irq_init(struct aml_bl_drv_s *bdrv)
{
	if (bdrv->res_vsync_irq[0]) {
		if (request_irq(bdrv->res_vsync_irq[0],
				bl_vsync_isr, IRQF_SHARED,
				"bl_vsync", (void *)bdrv)) {
			BLPR("[%d]: can't request bl_vsync_irq\n", bdrv->index);
		} else {
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: request bl_vsync_irq successful\n",
				     bdrv->index);
			}
		}
	}

	return 0;
}

static void bl_vsync_irq_remove(struct aml_bl_drv_s *bdrv)
{
	if (bdrv->res_vsync_irq[0])
		free_irq(bdrv->res_vsync_irq[0], (void *)"bl_vsync");
}

/* ************************************************************* */
static int bl_io_open(struct inode *inode, struct file *file)
{
	struct aml_bl_drv_s *bdrv;

	bdrv = container_of(inode->i_cdev, struct aml_bl_drv_s, cdev);
	file->private_data = bdrv;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);

	return 0;
}

static int bl_io_release(struct inode *inode, struct file *file)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("%s\n", __func__);
	file->private_data = NULL;
	return 0;
}

static long bl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long bl_compat_ioctl(struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = bl_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations bl_fops = {
	.owner          = THIS_MODULE,
	.open           = bl_io_open,
	.release        = bl_io_release,
	.unlocked_ioctl = bl_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = bl_compat_ioctl,
#endif
};

static int bl_cdev_add(struct aml_bl_drv_s *bdrv, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!bdrv) {
		BLERR("%s: bdrv is null\n", __func__);
		return -1;
	}
	if (!bl_cdev) {
		ret = 1;
		goto bl_cdev_add_failed;
	}

	devno = MKDEV(MAJOR(bl_cdev->devno), bdrv->index);

	cdev_init(&bdrv->cdev, &bl_fops);
	bdrv->cdev.owner = THIS_MODULE;
	ret = cdev_add(&bdrv->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto bl_cdev_add_failed;
	}

	bdrv->dev = device_create(bl_cdev->class, parent,
				  devno, NULL, "bl%d", bdrv->index);
	if (IS_ERR_OR_NULL(bdrv->dev)) {
		ret = 3;
		goto bl_cdev_add_failed1;
	}

	dev_set_drvdata(bdrv->dev, bdrv);
	bdrv->dev->of_node = parent->of_node;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s OK\n", bdrv->index, __func__);
	return 0;

bl_cdev_add_failed1:
	cdev_del(&bdrv->cdev);
bl_cdev_add_failed:
	BLERR("[%d]: %s: failed: %d\n", bdrv->index, __func__, ret);
	return -1;
}

static void bl_cdev_remove(struct aml_bl_drv_s *bdrv)
{
	dev_t devno;

	if (!bl_cdev || !bdrv)
		return;

	devno = MKDEV(MAJOR(bl_cdev->devno), bdrv->index);
	device_destroy(bl_cdev->class, devno);
	cdev_del(&bdrv->cdev);
}

static int bl_global_init_once(void)
{
	int ret;

	if (bl_global_init_flag) {
		bl_global_init_flag++;
		return 0;
	}
	bl_global_init_flag++;

	bl_notifier_init();

	bl_cdev = kzalloc(sizeof(*bl_cdev), GFP_KERNEL);
	if (!bl_cdev)
		return -1;

	ret = alloc_chrdev_region(&bl_cdev->devno, 0,
				  LCD_MAX_DRV, BL_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto bl_global_init_once_err;
	}

	bl_cdev->class = class_create("aml_bl");
	if (IS_ERR_OR_NULL(bl_cdev->class)) {
		ret = 2;
		goto bl_global_init_once_err_1;
	}

	return 0;

bl_global_init_once_err_1:
	unregister_chrdev_region(bl_cdev->devno, LCD_MAX_DRV);
bl_global_init_once_err:
	kfree(bl_cdev);
	bl_cdev = NULL;
	BLERR("%s: failed: %d\n", __func__, ret);
	return -1;
}

static void bl_global_remove_once(void)
{
	if (bl_global_init_flag > 1) {
		bl_global_init_flag--;
		return;
	}
	bl_global_init_flag--;

	bl_notifier_remove();

	if (!bl_cdev)
		return;

	class_destroy(bl_cdev->class);
	unregister_chrdev_region(bl_cdev->devno, LCD_MAX_DRV);
	kfree(bl_cdev);
	bl_cdev = NULL;
}

/* **************************************** */

#ifdef CONFIG_PM
static int aml_bl_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int aml_bl_resume(struct platform_device *pdev)
{
	return 0;
}
#endif

#ifdef CONFIG_OF
static struct bl_data_s bl_data_g12a = {
	.chip_type = LCD_CHIP_G12A,
	.chip_name = "g12a",
	.pwm_vs_flag = 0,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_g12b = {
	.chip_type = LCD_CHIP_G12B,
	.chip_name = "g12b",
	.pwm_vs_flag = 0,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_sm1 = {
	.chip_type = LCD_CHIP_SM1,
	.chip_name = "sm1",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_tm2 = {
	.chip_type = LCD_CHIP_TM2,
	.chip_name = "tm2",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_tm2b = {
	.chip_type = LCD_CHIP_TM2B,
	.chip_name = "tm2b",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t5d = {
	.chip_type = LCD_CHIP_T5D,
	.chip_name = "t5d",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t7 = {
	.chip_type = LCD_CHIP_T7,
	.chip_name = "t7",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t3 = {
	.chip_type = LCD_CHIP_T3,
	.chip_name = "t3",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t5w = {
	.chip_type = LCD_CHIP_T5W,
	.chip_name = "t5w",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_c3 = {
	.chip_type = LCD_CHIP_C3,
	.chip_name = "c3",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t5m = {
	.chip_type = LCD_CHIP_T5M,
	.chip_name = "t5m",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t3x = {
	.chip_type = LCD_CHIP_T3X,
	.chip_name = "t3x",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_txhd2 = {
	.chip_type = LCD_CHIP_TXHD2,
	.chip_name = "txhd2",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_s6 = {
	.chip_type = LCD_CHIP_S6,
	.chip_name = "s6",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t6d = {
	.chip_type = LCD_CHIP_T6D,
	.chip_name = "t6d",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t6w = {
	.chip_type = LCD_CHIP_T6W,
	.chip_name = "t6w",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static struct bl_data_s bl_data_t6x = {
	.chip_type = LCD_CHIP_T6X,
	.chip_name = "t6x",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 1,
};

static struct bl_data_s bl_data_a9 = {
	.chip_type = LCD_CHIP_A9,
	.chip_name = "a9",
	.pwm_vs_flag = 1,
	.pwm_vs_shift = 0,
};

static const struct of_device_id bl_dt_match_table[] = {
	{
		.compatible = "amlogic, backlight-g12a",
		.data = &bl_data_g12a,
	},
	{
		.compatible = "amlogic, backlight-g12b",
		.data = &bl_data_g12b,
	},
	{
		.compatible = "amlogic, backlight-sm1",
		.data = &bl_data_sm1,
	},
	{
		.compatible = "amlogic, backlight-tm2",
		.data = &bl_data_tm2,
	},
	{
		.compatible = "amlogic, backlight-tm2b",
		.data = &bl_data_tm2b,
	},
	{
		.compatible = "amlogic, backlight-t5d",
		.data = &bl_data_t5d,
	},
	{
		.compatible = "amlogic, backlight-t7",
		.data = &bl_data_t7,
	},
	{
		.compatible = "amlogic, backlight-t3",
		.data = &bl_data_t3,
	},
	{
		.compatible = "amlogic, backlight-t5w",
		.data = &bl_data_t5w,
	},
	{
		.compatible = "amlogic, backlight-c3",
		.data = &bl_data_c3,
	},
	{
		.compatible = "amlogic, backlight-t5m",
		.data = &bl_data_t5m,
	},
	{
		.compatible = "amlogic, backlight-t3x",
		.data = &bl_data_t3x,
	},
	{
		.compatible = "amlogic, backlight-txhd2",
		.data = &bl_data_txhd2,
	},
	{
		.compatible = "amlogic, backlight-s6",
		.data = &bl_data_s6,
	},
	{
		.compatible = "amlogic, backlight-t6d",
		.data = &bl_data_t6d,
	},
	{
		.compatible = "amlogic, backlight-t6w",
		.data = &bl_data_t6w,
	},
	{
		.compatible = "amlogic, backlight-t6x",
		.data = &bl_data_t6x,
	},
	{
		.compatible = "amlogic, backlight-a9",
		.data = &bl_data_a9,
	},
	{}
};
#endif

static void bl_init_status_update(struct aml_bl_drv_s *bdrv)
{
	struct aml_lcd_drv_s *pdrv;
	unsigned long flags = 0;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (!pdrv)
		return;

	/* default power state on */
	if (pdrv->boot_ctrl->init_level != LCD_INIT_LEVEL_PREBOOT)
		bdrv->state = BL_STATE_BL_POWER_ON;
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		lcd_resource_ready(bdrv->index, LCD_RES_BACKLIGHT, bdrv->bconf.index);
		break;
	default:
		break;
	}

	/* default disable lcd & backlight */
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return;

	/* update bl status */
	bdrv->state |= BL_STATE_LCD_ON;
	if (pdrv->boot_ctrl->bl_state)
		bdrv->state |= BL_STATE_BL_ON;
	bdrv->on_request = 1;

	mutex_lock(&bl_level_mutex);
	spin_lock_irqsave(&bdrv->isr_lock, flags);
	aml_bl_set_level(bdrv, bdrv->level_brightness);
	spin_unlock_irqrestore(&bdrv->isr_lock, flags);
	mutex_unlock(&bl_level_mutex);

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
	case BL_CTRL_PWM_COMBO:
	case BL_CTRL_PWM_ARRAY:
		bl_pwm_pinmux_set(bdrv, 1);
		break;
	default:
		break;
	}
}

int bl_config_load_post(struct aml_bl_drv_s *bdrv)
{
	int ret = 0;
#ifdef BL_BRIGHTNESS_METER
	struct bl_metrics_config_s *meter_conf = NULL;
#endif
	struct backlight_properties props;
	struct backlight_device *bldev;
	char bl_name[10];
	int index = bdrv->index;

	bl_pwm_curve_init(bdrv);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = (bdrv->bconf.level_max > 0 ?
				bdrv->bconf.level_max : bdrv->bconf.level_in_scale);
	if (bl_level_bootup[bdrv->index] < 0)
		props.brightness = -1;
	else
		props.brightness = bl_level_bootup[bdrv->index];
	bdrv->level_brightness = bl_bri_level_input_scale(bdrv, props.brightness);

	if (index == 0)
		sprintf(bl_name, "aml-bl");
	else
		sprintf(bl_name, "aml-bl%d", index);
	bldev = backlight_device_register(bl_name, &bdrv->pdev->dev,
					  bdrv, &aml_bl_ops, &props);
	if (IS_ERR(bldev)) {
		BLERR("[%d]: failed to register backlight\n", index);
		ret = PTR_ERR(bldev);
		goto err;
	}
	bdrv->bldev = bldev;

#ifdef BL_BRIGHTNESS_METER
	meter_conf = &bdrv->bl_metrics_conf;
	meter_conf->times = 60;
	meter_conf->cnt = 0;
	meter_conf->sum_cnt = 0;
	meter_conf->level_count = 0;
	meter_conf->brightness_count = 0;
	meter_conf->frame_rate = 60;
	meter_conf->level_buf =
		kcalloc(BL_LEVEL_CNT_MAX * 2, sizeof(unsigned int), GFP_KERNEL);
	if (!meter_conf->level_buf)
		goto err;

	meter_conf->brightness_buf =
		kcalloc(BL_LEVEL_CNT_MAX * 2, sizeof(unsigned int), GFP_KERNEL);
	if (!meter_conf->brightness_buf) {
		kfree(meter_conf->level_buf);
		meter_conf->level_buf = NULL;
		goto err;
	}
#endif

	spin_lock_init(&bdrv->isr_lock);
	bdrv->probe_done = 1;

	/* init workqueue */
	INIT_DELAYED_WORK(&bdrv->delayed_on_work, bl_delayed_on);

	bl_init_status_update(bdrv);

	bl_vsync_irq_init(bdrv);
	bl_debug_file_creat(bdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: ok\n", index, __func__);
	return 0;

err:
	return -1;
}

static void bl_base_config_load(struct aml_bl_drv_s *bdrv)
{
	unsigned int temp;
	int ret, i, cnt = 0;
	const char *bl_gpio[BL_GPIO_NUM_MAX];

	bdrv->level = -1;
	bdrv->level_brightness = -1;
	bdrv->level_gd = -1;
	bdrv->bconf.level_min = 0;
	bdrv->bconf.level_max = BL_LEVEL_FULL_SCALE;
	bdrv->bconf.power_on_delay = 100;
	bdrv->bconf.power_off_delay = 30;
	bdrv->bconf.method = BL_CTRL_MAX;

	if (!bdrv->dev->of_node) {
		BLERR("no backlight[%d] of_node\n", bdrv->index);
		return;
	}

	ret = of_property_read_u32(bdrv->dev->of_node, "key_valid", &temp);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("[%d]: failed to get key_valid\n", bdrv->index);
		temp = 0;
	}
	bdrv->key_valid = temp;

	cnt = of_property_read_string_array(bdrv->dev->of_node, "bl_gpio_names",
					    bl_gpio, BL_GPIO_NUM_MAX);
	for (i = 0; i < cnt; i++)
		strscpy(bdrv->bconf.bl_gpio[i].name, bl_gpio[i], LCD_CPU_GPIO_NAME_MAX);
}

int aml_bl_index_add(int drv_index, int conf_index)
{
	if (drv_index >= LCD_MAX_DRV) {
		BLERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}

	bl_index_lut[drv_index] = conf_index;
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("%s: drv_index %d, config index: %d\n",
			__func__, drv_index, conf_index);
	}
	return 0;
}

int aml_bl_index_get(int drv_index)
{
	int conf_index;

	if (drv_index >= LCD_MAX_DRV) {
		BLERR("%s: invalid drv_index: %d\n", __func__, drv_index);
		return -1;
	}

	conf_index = bl_index_lut[drv_index];
	return conf_index;
}

static int aml_bl_probe(struct platform_device *pdev)
{
	struct aml_bl_drv_s *bdrv = NULL;
	const struct of_device_id *match;
	int index = 0;
	int ret = 0;

	bl_global_init_once();

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("%s: no index exist, default to 0\n", __func__);
		index = 0;
	}
	if (index >= LCD_MAX_DRV) {
		BLERR("%s: invalid index %d\n", __func__, index);
		return -1;
	}
	if (bl_drv_init_state & (1 << index)) {
		BLERR("%s: index %d driver already registered\n",
		       __func__, index);
		return -1;
	}
	bl_drv_init_state |= (1 << index);

	match = of_match_device(bl_dt_match_table, &pdev->dev);
	if (!match) {
		BLERR("%s: no match table\n", __func__);
		goto aml_bl_probe_err;
	}

	bdrv = kzalloc(sizeof(*bdrv), GFP_KERNEL);
	if (!bdrv)
		goto aml_bl_probe_err;
	bdrv->index = index;
	bdrv->data = (struct bl_data_s *)match->data;
	bl_drv[index] = bdrv;
	BLPR("chip type: (%d-%s)\n",
	     bdrv->data->chip_type,
	     bdrv->data->chip_name);

	/* set drvdata */
	platform_set_drvdata(pdev, bdrv);
	bl_cdev_add(bdrv, &pdev->dev);
	bdrv->pdev = pdev;

	bl_pwm_init_config_probe(bdrv->data);
	bl_base_config_load(bdrv);

	if (lcd_bl_check_config_load(bdrv)) {
		BLERR("lcd_bl_check_config_load error\n");
		goto aml_bl_probe_err;
	}

	ret = bl_config_load(bdrv, bdrv->pdev);
	if (ret)
		goto aml_bl_probe_err;

	BLPR("[%d]: probe OK, init_state:0x%x\n", index, bl_drv_init_state);
	return 0;

aml_bl_probe_err:
	if (bdrv) {
		platform_set_drvdata(bdrv->pdev, NULL);
		kfree(bdrv);
		bl_drv[index] = NULL;
	}
	bl_drv_init_state &= ~(1 << index);
	BLPR("[%d]: %s failed\n", index, __func__);
	return -1;
}

static __maybe_unused void aml_bl_remove(struct platform_device *pdev)
{
	struct aml_bl_drv_s *bdrv = platform_get_drvdata(pdev);
	int index, i;

	if (!bdrv)
		return;

	index = bdrv->index;

#ifdef BL_BRIGHTNESS_METER
	kfree(bdrv->bl_metrics_conf.level_buf);
	kfree(bdrv->bl_metrics_conf.brightness_buf);
#endif
	cancel_delayed_work_sync(&bdrv->delayed_on_work);
	cancel_delayed_work(&bdrv->config_probe_dly_work);
	backlight_device_unregister(bdrv->bldev);

	bl_debug_file_remove(bdrv);
	bl_vsync_irq_remove(bdrv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	bl_cdev_remove(bdrv);

#ifdef CONFIG_AMLOGIC_BL_LDIM
	if (bdrv->bconf.ldim_flag)
		aml_ldim_remove();
#endif
	/* platform_set_drvdata(pdev, NULL); */
	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		kfree(bdrv->bconf.bl_pwm);
		break;
	case BL_CTRL_PWM_COMBO:
		kfree(bdrv->bconf.bl_pwm_combo0);
		kfree(bdrv->bconf.bl_pwm_combo1);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++)
			kfree(bdrv->bconf.bl_pwm_array[i]);
		kfree(bdrv->bconf.bl_pwm_array);
		break;
	default:
		break;
	}
	kfree(bdrv);
	bl_drv[index] = NULL;
	bl_drv_init_state &= ~(1 << index);
	bl_global_remove_once();

	return;
}

static struct platform_driver aml_bl_driver = {
	.driver = {
		.name  = AML_BL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(bl_dt_match_table),
#endif
	},
	.probe   = aml_bl_probe,
	.remove  = __exit_p(aml_bl_remove),
#ifdef CONFIG_PM
	.suspend = aml_bl_suspend,
	.resume  = aml_bl_resume,
#endif
};

int __init aml_bl_init(void)
{
	if (platform_driver_register(&aml_bl_driver)) {
		BLPR("failed to register bl driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit aml_bl_exit(void)
{
	platform_driver_unregister(&aml_bl_driver);
}

static int aml_bl_level_setup(char *str)
{
	int ret = 0;

	if (str) {
		ret = kstrtoint(str, 10, &bl_level_bootup[0]);
		if (ret)
			return ret;
		BLPR("bl_level: %d\n", bl_level_bootup[0]);
	}

	return 1;
}

static int aml_bl1_level_setup(char *str)
{
	int ret = 0;

	if (str) {
		ret = kstrtoint(str, 10, &bl_level_bootup[1]);
		if (ret)
			return ret;
		BLPR("bl1_level: %d\n", bl_level_bootup[1]);
	}

	return ret;
}

static int aml_bl2_level_setup(char *str)
{
	int ret = 0;

	if (str) {
		ret = kstrtoint(str, 10, &bl_level_bootup[2]);
		if (ret)
			return ret;
		BLPR("bl2_level: %d\n", bl_level_bootup[2]);
	}

	return ret;
}

__setup("bl_level=", aml_bl_level_setup);
__setup("bl1_level=", aml_bl1_level_setup);
__setup("bl2_level=", aml_bl2_level_setup);

//MODULE_DESCRIPTION("AML Backlight Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
