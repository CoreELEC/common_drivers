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
static unsigned int bl_level_bootup[LCD_MAX_DRV];

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

static void bl_pwm_ctrl_status_set(struct aml_bl_drv_s *bdrv, int status)
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

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("backlight power on pwm first\n");
}

static void bl_power_on(struct aml_bl_drv_s *bdrv)
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

	if (bdrv->brightness_bypass == 0) {
		if (bdrv->level == 0 || (bdrv->state & BL_STATE_BL_ON))
			goto exit_power_on_bl;
	}

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

static void bl_power_off(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
	int ret, i;

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
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm */
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm, 0);
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
			bl_pwm_ctrl(bconf->bl_pwm, 0);
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm_combo */
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off pwm_combo */
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bdrv, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo0, 0);
			bl_pwm_ctrl(bconf->bl_pwm_combo1, 0);
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		if (bconf->en_sequence_reverse == 1) {
			/* step 1: power off pwm_array */
			bl_pwm_pinmux_set(bdrv, 0);
			for (i = 0; i < 4; i++)
				bl_pwm_ctrl(bconf->bl_pwm_array[i], 0);
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			/* step 2: power off enable */
			bl_power_en_ctrl(bdrv, 0);
		} else {
			/* step 1: power off enable */
			bl_power_en_ctrl(bdrv, 0);
			/* step 2: power off pwm_array */
			if (bconf->pwm_off_delay > 0)
				lcd_delay_ms(bconf->pwm_off_delay);
			bl_pwm_pinmux_set(bdrv, 0);
			for (i = 0; i < 4; i++)
				bl_pwm_ctrl(bconf->bl_pwm_array[i], 0);
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

static void aml_bl_set_level(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	struct bl_pwm_config_s *pwm0, *pwm1;
	int i;

	if (aml_bl_check_driver(bdrv))
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("bl_set_level=%u, last_level=%u, state=0x%x\n",
		     level, bdrv->level, bdrv->state);
	}

	/* level range check */
	if (level > bdrv->bconf.level_max)
		level = bdrv->bconf.level_max;
	if (level < bdrv->bconf.level_min) {
		if (level < BL_LEVEL_OFF)
			level = 0;
		else
			level = bdrv->bconf.level_min;
	}
	bdrv->level = level;

	if (level == 0)
		return;

	switch (bdrv->bconf.method) {
	case BL_CTRL_GPIO:
		break;
	case BL_CTRL_PWM:
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm, level);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm0 = bdrv->bconf.bl_pwm_combo0;
		pwm1 = bdrv->bconf.bl_pwm_combo1;

		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("pwm0 region, level=%u\n", level);
		bl_pwm_set_level(bdrv, pwm0, level);

		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
			BLPR("pwm1 region, level=%u\n", level);
		bl_pwm_set_level(bdrv, pwm1, level);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			if (!bdrv->bconf.bl_pwm_array[i]) {
				if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
					BLERR("pwm_array[%d] is null\n", i);
				continue;
			}
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV)
				BLPR("pwm_array[%d] region, level=%u\n", i, level);
			bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm_array[i], level);
		}
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		bl_set_level_ldim(level);
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bl_set_level_extern(bdrv, level);
		break;
#endif
	default:
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
			BLPR("invalid backlight control method\n");
		break;
	}
}

static inline unsigned int bl_brightness_level_map(struct aml_bl_drv_s *bdrv,
						   unsigned int brightness)
{
	unsigned int level;

	if (brightness > bdrv->bconf.level_max)
		level = bdrv->bconf.level_max;
	else if (brightness < bdrv->bconf.level_min)
		level = bdrv->bconf.level_min;
	else
		level = brightness;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("%s: input brightness=%d, output level=%d\n",
			__func__, brightness, level);
	}

	return level;
}

static inline unsigned int bl_gd_level_map(struct aml_bl_drv_s *bdrv, unsigned int gd_level)
{
	unsigned int max, min, val;

	min = bdrv->bconf.level_min;
	max = bdrv->bconf.level_max;

	if (bdrv->level_brightness <= min)
		return min;

	val = (gd_level * (bdrv->level_brightness - min)) / 4095 + min;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
		BLPR("%s: input gd_level=%d, output val=%d, min=%d, max=%d\n",
			__func__, gd_level, val, min, max);
	}

	return val;
}

static unsigned int aml_bl_init_level(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	unsigned int bl_level = level;
	struct aml_lcd_drv_s *pdrv;

	pdrv = aml_lcd_get_driver(bdrv->index);
	if (!pdrv)
		return 0;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0)) {
		if (pdrv->boot_ctrl->init_level == LCD_INIT_LEVEL_PREBOOT)
			return 0;
		bl_level = 0;
	}

	if (bl_level == 0) {
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		aml_bl_set_level(bdrv, bl_level);
		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: %s: %u, final level: %u, state: 0x%x\n",
		     bdrv->index, __func__, level, bl_level, bdrv->state);
	}

	return 0;
}

int aml_bl_set_level_brightness(struct aml_bl_drv_s *bdrv, unsigned int brightness)
{
	unsigned int level;

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
			BLPR("[%d]: %s: backlight is off, state: 0x%x\n",
					bdrv->index, __func__, bdrv->state);
		return 0;
	}

	mutex_lock(&bl_status_mutex);
	bdrv->level_brightness = bl_brightness_level_map(bdrv, brightness);

	if (bdrv->level_brightness == 0) {
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		if ((bdrv->state & BL_STATE_GD_EN) == 0) {
			aml_bl_set_level(bdrv, bdrv->level_brightness);
		} else {
			level = bl_gd_level_map(bdrv, bdrv->level_gd);
			aml_bl_set_level(bdrv, level);
		}

		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
	}
	mutex_unlock(&bl_status_mutex);

	return 0;
}

unsigned int aml_bl_get_level_brightness(struct aml_bl_drv_s *bdrv)
{
	if (aml_bl_check_driver(bdrv))
		return 0;

	return bdrv->level_brightness;
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

/* lcd notify */
static void bl_on_function(struct aml_bl_drv_s *bdrv)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	int i;

	mutex_lock(&bl_level_mutex);

	/* lcd power on backlight flag */
	bdrv->state |= BL_STATE_LCD_ON;
	BLPR("%s: bl_step_on_flag=%d, bl_level=%u, state=0x%x\n",
	     __func__, bdrv->step_on_flag, bdrv->level, bdrv->state);

	bdrv->state |= BL_STATE_BL_INIT_ON;
	switch (bdrv->step_on_flag) {
	case 1:
		BLPR("bl_step_on level: %d\n", bconf->level_default);
		aml_bl_init_level(bdrv, bconf->level_default);
		lcd_delay_ms(120);
		if (bdrv->brightness_bypass) {
			switch (bconf->method) {
			case BL_CTRL_PWM:
				bconf->bl_pwm->pwm_duty =
					bconf->bl_pwm->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm);
				break;
			case BL_CTRL_PWM_COMBO:
				bconf->bl_pwm_combo0->pwm_duty =
					bconf->bl_pwm_combo0->pwm_duty_save;
				bconf->bl_pwm_combo1->pwm_duty =
					bconf->bl_pwm_combo1->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo0);
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo1);
				break;
			case BL_CTRL_PWM_ARRAY:
				for (i = 0; i < 4; i++) {
					if (bconf->bl_pwm_array[i]) {
						bconf->bl_pwm_array[i]->pwm_duty =
							bconf->bl_pwm_array[i]->pwm_duty_save;
						bl_pwm_set_duty(bdrv, bconf->bl_pwm_array[i]);
					}
				}
				break;
			default:
				break;
			}
		} else {
			BLPR("bl_on level: %d\n", bdrv->level_brightness);
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
		break;
	case 2:
		BLPR("bl_step_on level: %d\n", bconf->level_uboot);
		aml_bl_init_level(bdrv, bconf->level_uboot);
		lcd_delay_ms(120);
		if (bdrv->brightness_bypass) {
			switch (bconf->method) {
			case BL_CTRL_PWM:
				bconf->bl_pwm->pwm_duty =
					bconf->bl_pwm->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm);
				break;
			case BL_CTRL_PWM_COMBO:
				bconf->bl_pwm_combo0->pwm_duty =
					bconf->bl_pwm_combo0->pwm_duty_save;
				bconf->bl_pwm_combo1->pwm_duty =
					bconf->bl_pwm_combo1->pwm_duty_save;
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo0);
				bl_pwm_set_duty(bdrv, bconf->bl_pwm_combo1);
				break;
			case BL_CTRL_PWM_ARRAY:
				for (i = 0; i < 4; i++) {
					if (bconf->bl_pwm_array[i]) {
						bconf->bl_pwm_array[i]->pwm_duty =
							bconf->bl_pwm_array[i]->pwm_duty_save;
						bl_pwm_set_duty(bdrv, bconf->bl_pwm_array[i]);
					}
				}
				break;
			default:
				break;
			}
		} else {
			BLPR("bl_on level: %d\n", bdrv->level_brightness);
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
		break;
	default:
		if (bdrv->brightness_bypass) {
			if ((bdrv->state & BL_STATE_BL_ON) == 0)
				bl_power_on(bdrv);
		} else {
			aml_bl_init_level(bdrv, bdrv->level_brightness);
		}
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

	if (pdrv->status & LCD_STATE_BL_PRE_ON) {
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

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: %d, state: 0x%x\n", bdrv->index, __func__, temp, bdrv->state);

	if (temp == 0) {
		bdrv->state &= ~BL_STATE_BL_POWER_ON;
		if (bdrv->state & BL_STATE_BL_ON)
			bl_power_off(bdrv);
	} else {
		bdrv->state |= BL_STATE_BL_POWER_ON;
		if ((bdrv->state & BL_STATE_BL_ON) == 0)
			bl_power_on(bdrv);
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

	if (bdrv->brightness_bypass)
		bl_pwm_set_duty(bdrv, bl_pwm);
	else
		aml_bl_set_level(bdrv, bdrv->bldev->props.brightness);
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

static int bl_lcd_test_notifier(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct aml_bl_drv_s *bdrv;
	int flag;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	/* If we aren't interested in this event, skip it immediately */
	if (event != LCD_EVENT_TEST_PATTERN)
		return NOTIFY_DONE;

	if (!pdrv)
		return NOTIFY_DONE;
	bdrv = aml_bl_get_driver(pdrv->index);
	if (aml_bl_check_driver(bdrv))
		return NOTIFY_DONE;
	if (bdrv->probe_done == 0)
		return NOTIFY_DONE;

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s for lcd test_pattern\n", bdrv->index, __func__);

	flag = (pdrv->test_state > 0) ? 1 : 0;
	switch (bdrv->bconf.method) {
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		if (ldim_drv->test_ctrl)
			ldim_drv->test_ctrl(flag);
		break;
#endif
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block bl_lcd_test_nb = {
	.notifier_call = bl_lcd_test_notifier,
};

static int bl_gd_diming_func(struct aml_bl_drv_s *bdrv, unsigned int level)
{
	unsigned int level_new;

	if (bdrv->brightness_bypass)
		return 0;
	if ((bdrv->state & BL_STATE_GD_EN) == 0)
		return 0;

	if (((bdrv->state & BL_STATE_LCD_ON) == 0) ||
	    (bdrv->state & BL_STATE_BL_INIT_ON) ||
	    ((bdrv->state & BL_STATE_BL_POWER_ON) == 0) ||
	    ((bdrv->state & BL_STATE_BL_ON) == 0) ||
	    (bdrv->level_brightness == 0))
		return 0;

	/* atomic notifier, can't schedule or sleep */
	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL)
		BLPR("[%d]: %s: level_gd: %d\n", bdrv->index, __func__, level);

	bdrv->level_gd = (level < 10) ? 10 : ((level > 4095) ? 4095 : level);
	level_new = bl_gd_level_map(bdrv, bdrv->level_gd);
	aml_bl_set_level(bdrv, level_new);

	if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
		BLPR("[%d]: %s: %u, final level: %u, state: 0x%x\n",
		     bdrv->index, __func__, level,
		     level_new, bdrv->state);
	}

	return 0;
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

static int bl_gd_sel_func(struct aml_bl_drv_s *bdrv, unsigned int sel)
{
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
#endif

	if (bdrv->brightness_bypass)
		return 0;

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
	unsigned int level;

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

	if (*(unsigned int *)data == 0)
		return NOTIFY_OK;

	/* atomic notifier, can't schedule or sleep */
	bdrv->bldev->props.brightness = *(unsigned int *)data;
	bdrv->level_brightness = bl_brightness_level_map(bdrv,
					bdrv->bldev->props.brightness);

	if ((bdrv->state & BL_STATE_GD_EN) == 0) {
		aml_bl_set_level(bdrv, bdrv->level_brightness);
	} else {
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
		aml_bl_set_level(bdrv, level);
	}

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

	if (bdrv->brightness_bypass)
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
			bconf->bl_pwm_switch->bl_level = bconf->bl_pwm->bl_level;
		if (bconf->bl_pwm_default)
			bconf->bl_pwm_default->bl_level = bconf->bl_pwm->bl_level;
		break;
	case BL_CTRL_PWM_COMBO:
		if (bconf->bl_pwm_switch)
			bconf->bl_pwm_switch->bl_level = bconf->bl_pwm_combo0->bl_level;
		if (bconf->bl_pwm_default)
			bconf->bl_pwm_default->bl_level = bconf->bl_pwm_combo0->bl_level;
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
	ret = aml_lcd_notifier_register(&bl_lcd_test_nb);
	if (ret)
		BLERR("register bl_lcd_test_nb failed\n");
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
	aml_lcd_notifier_unregister(&bl_lcd_test_nb);
	aml_lcd_notifier_unregister(&bl_lcd_update_nb);
	aml_lcd_notifier_unregister(&bl_power_ctrl_nb);
}

static void bl_pwm_port_update(struct aml_bl_drv_s *bdrv)
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
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm,
							bdrv->bconf.bl_pwm->bl_level);
		bl_pwm_pinmux_set(bdrv, 1);
		break;
	case BL_CTRL_PWM_COMBO:
		bl_pwm_set_level(bdrv, bdrv->bconf.bl_pwm_combo0,
							bdrv->bconf.bl_pwm_combo0->bl_level);
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
	unsigned int level = 0;
	static int backlight_toggled;

	if ((bdrv->state & BL_STATE_BL_ON) == 0)
		return;
	if (bdrv->brightness_bypass)
		return;
	if (bdrv->level_brightness == 0)
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
			bl_pwm_port_update(bdrv);
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

	if ((bdrv->state & BL_STATE_GD_EN) == 0) {
		if (bdrv->level_brightness == bdrv->level)
			return;
		aml_bl_set_level(bdrv, bdrv->level_brightness);
	} else {
		level = bl_gd_level_map(bdrv, bdrv->level_gd);
		if (level == bdrv->level)
			return;
		aml_bl_set_level(bdrv, level);
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

/***************************************************************/
static const char *bl_debug_usage_str = {
"Usage:\n"
"    cat status ; dump backlight config\n"
"\n"
"    echo freq <index> <pwm_freq> > pwm ; set pwm frequency(unit in Hz for pwm, vfreq multiple for pwm_vs)\n"
"    echo duty <index> <pwm_duty> > pwm ; set pwm duty cycle(unit: %)\n"
"    echo pol <index> <pwm_pol> > pwm ; set pwm polarity(unit: %)\n"
"	 echo max <index> <duty_max> > pwm ; set pwm duty_max(unit: %)\n"
"	 echo min <index> <duty_min> > pwm ; set pwm duty_min(unit: %)\n"
"    cat pwm ; dump pwm state\n"
"	 echo free <0|1> > pwm ; set pwm_duty_free enable or disable\n"
"\n"
"    echo <0|1> > power ; backlight power ctrl\n"
"    cat power ; print backlight power state\n"
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
	struct bl_pwm_config_s *bl_pwm;
	struct bl_pwm_config_s *pwm_combo0, *pwm_combo1;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	struct aml_ldim_driver_s *ldim_drv;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	struct bl_extern_driver_s *bext;
#endif
	ssize_t len = 0;
	int i;

	if (!bdrv) {
		BLPR("bdrv is null\n");
		return len;
	}

	bconf = &bdrv->bconf;
	len = sprintf(buf, "read backlight status:\n"
		      "key_valid:          %d\n"
		      "config_load:        %d\n"
		      "index:              %d\n"
		      "name:               %s\n"
		      "state:              0x%x\n"
		      "level:              %d\n"
		      "level_brightness:   %d\n"
		      "level_gd:           %d\n"
		      "level_uboot:        %d\n"
		      "level_default:      %d\n"
		      "step_on_flag        %d\n"
		      "brightness_bypass:  %d\n\n"
		      "debug_force:        %d\n\n"
		      "level_max:          %d\n"
		      "level_min:          %d\n"
		      "level_mid:          %d\n"
		      "level_mid_mapping:  %d\n\n"
		      "method:             %s\n"
		      "en_gpio:            %s(%d)\n"
		      "en_gpio_on:         %d\n"
		      "en_gpio_off:        %d\n"
		      "power_on_delay:     %d\n"
		      "power_off_delay:    %d\n"
		      "bl_hold_on:         %d\n\n",
		      bdrv->key_valid, bdrv->config_load,
		      bdrv->index, bconf->name, bdrv->state,
		      bdrv->level, bdrv->level_brightness, bdrv->level_gd,
		      bconf->level_uboot, bconf->level_default,
		      bdrv->step_on_flag, bdrv->brightness_bypass,
		      bdrv->debug_force,
		      bconf->level_max, bconf->level_min,
		      bconf->level_mid, bconf->level_mid_mapping,
		      bl_method_type_to_str(bconf->method),
		      (bconf->en_gpio >= BL_GPIO_NUM_MAX) ? "null" :
		      bconf->bl_gpio[bconf->en_gpio].name,
		      bconf->en_gpio, bconf->en_gpio_on, bconf->en_gpio_off,
		      bconf->power_on_delay, bconf->power_off_delay,
		      bconf->bl_hold_on);

	switch (bconf->method) {
	case BL_CTRL_GPIO:
		len += sprintf(buf + len, "to do\n");
		break;
	case BL_CTRL_PWM:
		bl_pwm = bconf->bl_pwm;
		len += sprintf(buf + len,
			       "pwm_method:         %d\n"
			       "pwm_port:           %s(0x%x)\n"
			       "pwm_freq:           %d\n"
			       "pwm_phase:           %d\n"
			       "pwm_duty_max:       %d\n"
			       "pwm_duty_min:       %d\n"
			       "pwm_on_delay:       %d\n"
			       "pwm_off_delay:      %d\n"
			       "en_sequence_reverse: %d\n\n",
			       bl_pwm->pwm_method,
			       bl_pwm_num_to_str(bl_pwm->pwm_port),
			       bl_pwm->pwm_port,
			       bl_pwm->pwm_freq,
			       bl_pwm->pwm_phase,
			       bl_pwm->pwm_duty_max, bl_pwm->pwm_duty_min,
			       bconf->pwm_on_delay, bconf->pwm_off_delay,
			       bconf->en_sequence_reverse);
		break;
	case BL_CTRL_PWM_COMBO:
		pwm_combo0 = bconf->bl_pwm_combo0;
		pwm_combo1 = bconf->bl_pwm_combo1;
		len += sprintf(buf + len,
			       "pwm_0_level_max:    %d\n"
			       "pwm_0_level_min:    %d\n"
			       "pwm_0_method:       %d\n"
			       "pwm_0_port:         %s(0x%x)\n"
			       "pwm_0_freq:         %d\n"
			       "pwm_0_phase:         %d\n"
			       "pwm_0_duty_max:     %d\n"
			       "pwm_0_duty_min:     %d\n"
			       "pwm_1_level_max:    %d\n"
			       "pwm_1_level_min:    %d\n"
			       "pwm_1_method:       %d\n"
			       "pwm_1_port:         %s(0x%x)\n"
			       "pwm_1_freq:         %d\n"
			       "pwm_1_phase:         %d\n"
			       "pwm_1_duty_max:     %d\n"
			       "pwm_1_duty_min:     %d\n"
			       "pwm_on_delay:       %d\n"
			       "pwm_off_delay:      %d\n"
			       "en_sequence_reverse: %d\n\n",
			       pwm_combo0->level_max, pwm_combo0->level_min,
			       pwm_combo0->pwm_method,
			       bl_pwm_num_to_str(pwm_combo0->pwm_port),
			       pwm_combo0->pwm_port,
			       pwm_combo0->pwm_freq,
			       pwm_combo0->pwm_phase,
			       pwm_combo0->pwm_duty_max,
			       pwm_combo0->pwm_duty_min,
			       pwm_combo1->level_max, pwm_combo1->level_min,
			       pwm_combo1->pwm_method,
			       bl_pwm_num_to_str(pwm_combo1->pwm_port),
			       pwm_combo1->pwm_port,
			       pwm_combo1->pwm_freq,
			       pwm_combo1->pwm_phase,
			       pwm_combo1->pwm_duty_max,
			       pwm_combo1->pwm_duty_min,
			       bconf->pwm_on_delay, bconf->pwm_off_delay,
			       bconf->en_sequence_reverse);
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			bl_pwm = bconf->bl_pwm_array[i];
			if (!bl_pwm) {
				BLPR("[%d]: bl_pwm_array[%d] is null\n",
				     bdrv->index, i);
				continue;
			}

			len += sprintf(buf + len,
				       "pwm_%d_level_max:    %d\n"
				       "pwm_%d_level_min:    %d\n"
				       "pwm_%d_method:       %d\n"
				       "pwm_%d_port:         %s(0x%x)\n"
				       "pwm_%d_freq:         %d\n"
				       "pwm_%d_phase:         %d\n"
				       "pwm_%d_duty_max:     %d\n"
				       "pwm_%d_duty_min:     %d\n",
				       i, bl_pwm->level_max,
					   i, bl_pwm->level_min,
				       i, bl_pwm->pwm_method,
				       i, bl_pwm_num_to_str(bl_pwm->pwm_port),
				       bl_pwm->pwm_port,
				       i, bl_pwm->pwm_freq,
				       i, bl_pwm->pwm_phase,
				       i, bl_pwm->pwm_duty_max,
				       i, bl_pwm->pwm_duty_min);
		}
		len += sprintf(buf + len,
			       "pwm_on_delay:       %d\n"
			       "pwm_off_delay:      %d\n"
			       "en_sequence_reverse: %d\n\n",
			bconf->pwm_on_delay, bconf->pwm_off_delay,
			bconf->en_sequence_reverse);
		break;
#ifdef CONFIG_AMLOGIC_BL_LDIM
	case BL_CTRL_LOCAL_DIMMING:
		ldim_drv = aml_ldim_get_driver();
		if (!ldim_drv) {
			BLPR("[%d]: ldim is null\n", bdrv->index);
			break;
		}
		if (!ldim_drv->config_print) {
			BLPR("[%d]: ldim_drv->config_print is null\n", bdrv->index);
			break;
		}
		ldim_drv->config_print();
		break;
#endif
#ifdef CONFIG_AMLOGIC_BL_EXTERN
	case BL_CTRL_EXTERN:
		bext = bl_extern_get_driver(bdrv->index);
		if (!bext) {
			BLPR("[%d]: bext is null\n", bdrv->index);
			break;
		}
		if (!bext->config_print) {
			BLPR("[%d]: bext->config_print is null\n", bdrv->index);
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

int print_pwm_vs_registers(char *buf, int len,
				struct bl_pwm_init_config_s *pwm_cfg)
{
	len += sprintf(buf + len,
				   "pwm_vs_reg0:         0x%08x\n"
				   "pwm_vs_reg1:         0x%08x\n"
				   "pwm_vs_reg2:         0x%08x\n"
				   "pwm_vs_reg3:         0x%08x\n",
				   lcd_vcbus_read(pwm_cfg->pwm_vs_reg[0]),
				   lcd_vcbus_read(pwm_cfg->pwm_vs_reg[1]),
				   lcd_vcbus_read(pwm_cfg->pwm_vs_reg[2]),
				   lcd_vcbus_read(pwm_cfg->pwm_vs_reg[3]));
	return len;
}

static inline bool bl_pwm_port_is_hw(int port)
{
	return ((port >= BL_PWM_A && port <= BL_PWM_N) ||
		(port >= BL_PWM_AO_A && port <= BL_PWM_AO_H));
}

static ssize_t bl_debug_pwm_hw_state_show(char *buf, ssize_t len, size_t size,
					  struct bl_pwm_config_s *bl_pwm,
					  struct bl_pwm_init_config_s *pwm_cfg,
					  bool allow_vs)
{
	struct pwm_state pstate;

	if (bl_pwm_port_is_hw(bl_pwm->pwm_port)) {
		if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm))
			return len + scnprintf(buf + len, size - len,
						"pwm invalid\n");

		pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
		return len + scnprintf(buf + len, size - len,
			"pwm state:\n"
			"  period:           %lld\n"
			"  duty_cycle:       %lld\n"
			"  polarity:         %d\n"
			"  enabled:          %d\n",
			pstate.period,
			pstate.duty_cycle,
			pstate.polarity,
			pstate.enabled);
	}

	if (bl_pwm->pwm_port == BL_PWM_VS) {
		if (!allow_vs)
			return len + scnprintf(buf + len, size - len,
						"pwm_vs not support\n");

		return print_pwm_vs_registers(buf, len, pwm_cfg);
	}

	return len + scnprintf(buf + len, size - len,
				"invalid pwm_port: 0x%x\n",
				bl_pwm->pwm_port);
}

static ssize_t bl_debug_pwm_cfg_show(char *buf, ssize_t len, size_t size,
				     struct bl_pwm_config_s *bl_pwm,
				     const char *tag,
				     struct bl_pwm_init_config_s *pwm_cfg,
				     bool allow_vs)
{
	int duty_percent;

	if (!bl_pwm)
		return len;

	len += scnprintf(buf + len, size - len,
		"%s_index:        %d\n"
		"%s_port:         %s(0x%x)\n"
		"%s_method:       %d\n"
		"%s_freq:         %d\n"
		"%s_phase:        %d\n"
		"%s_duty_max:     %d\n"
		"%s_duty_min:     %d\n"
		"%s_level_max:    %d\n"
		"%s_level_min:    %d\n"
		"%s_cnt:          %d\n"
		"%s_max:          %d\n"
		"%s_min:          %d\n"
		"%s_level:        %d\n"
		"%s_mapping:      %d_%d_%d_%d_%d %d_%d_%d\n",
		tag, bl_pwm->index,
		tag, bl_pwm_num_to_str(bl_pwm->pwm_port), bl_pwm->pwm_port,
		tag, bl_pwm->pwm_method,
		tag, bl_pwm->pwm_freq,
		tag, bl_pwm->pwm_phase,
		tag, bl_pwm->pwm_duty_max,
		tag, bl_pwm->pwm_duty_min,
		tag, bl_pwm->level_max,
		tag, bl_pwm->level_min,
		tag, bl_pwm->pwm_cnt,
		tag, bl_pwm->pwm_max,
		tag, bl_pwm->pwm_min,
		tag, bl_pwm->pwm_level,
		tag,
		bl_pwm->pwm_mapping[0],
		bl_pwm->pwm_mapping[1],
		bl_pwm->pwm_mapping[2],
		bl_pwm->pwm_mapping[3],
		bl_pwm->pwm_mapping[4],
		bl_pwm->pwm_mapping[5],
		bl_pwm->pwm_mapping[6],
		bl_pwm->pwm_mapping[7]);

	if (bl_pwm->pwm_duty_max > 100) {
		duty_percent = bl_pwm->pwm_duty * 100 /
			       bl_pwm->pwm_duty_max;
		len += scnprintf(buf + len, size - len,
				 "%s_duty:         %d(%d%%)\n",
				 tag, bl_pwm->pwm_duty, duty_percent);
	} else {
		len += scnprintf(buf + len, size - len,
				 "%s_duty:         %d%%\n",
				 tag, bl_pwm->pwm_duty);
	}

	return bl_debug_pwm_hw_state_show(buf, len, size,
					  bl_pwm, pwm_cfg, allow_vs);
}

static ssize_t bl_debug_pwm_info_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	struct bl_pwm_init_config_s *pwm_cfg = get_pwm_init_cfg();
	ssize_t len = 0;
	int i;

	len += scnprintf(buf + len, PAGE_SIZE - len,
			 "read backlight pwm info:\n");

	switch (bdrv->bconf.method) {
	case BL_CTRL_PWM:
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "pwm_bypass:      %d\n"
				 "pwm_duty_free:   %d\n",
				 bdrv->pwm_bypass,
				 bdrv->pwm_duty_free);

		len = bl_debug_pwm_cfg_show(buf, len, PAGE_SIZE,
					    bdrv->bconf.bl_pwm,
					    "pwm",
					    pwm_cfg, true);
		break;

	case BL_CTRL_PWM_COMBO:
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "pwm_bypass:      %d\n"
				 "pwm_duty_free:   %d\n",
				 bdrv->pwm_bypass,
				 bdrv->pwm_duty_free);

		len = bl_debug_pwm_cfg_show(buf, len, PAGE_SIZE,
					    bdrv->bconf.bl_pwm_combo0,
					    "pwm_0",
					    pwm_cfg, true);

		len = bl_debug_pwm_cfg_show(buf, len, PAGE_SIZE,
					    bdrv->bconf.bl_pwm_combo1,
					    "pwm_1",
					    pwm_cfg, true);
		break;

	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			char tag[8];

			if (!bdrv->bconf.bl_pwm_array[i])
				continue;

			snprintf(tag, sizeof(tag), "pwm_%d", i);
			len = bl_debug_pwm_cfg_show(buf, len, PAGE_SIZE,
				bdrv->bconf.bl_pwm_array[i],
				tag, pwm_cfg, false);
		}
		break;

	default:
		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "not pwm control method\n");
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
				       "pwm: freq=%d, phase=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					"duty_value=%d(%d%%)\n",
					bl_pwm->pwm_duty,
					bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}
		break;
	case BL_CTRL_PWM_COMBO:
		if (bdrv->bconf.bl_pwm_combo0) {
			bl_pwm = bdrv->bconf.bl_pwm_combo0;
			len += sprintf(buf + len,
				       "pwm_0: freq=%d, phase=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					"duty_value=%d(%d%%)\n",
					bl_pwm->pwm_duty,
					bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}

		if (bdrv->bconf.bl_pwm_combo1) {
			bl_pwm = bdrv->bconf.bl_pwm_combo1;
			len += sprintf(buf + len,
				       "pwm_1: freq=%d, phase=%d, pol=%d, duty_max=%d, duty_min=%d, ",
				       bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method,
				       bl_pwm->pwm_duty_max,
				       bl_pwm->pwm_duty_min);
			if (bl_pwm->pwm_duty_max > 100) {
				len += sprintf(buf + len,
					"duty_value=%d(%d%%)\n",
					bl_pwm->pwm_duty,
					bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max);
			} else {
				len += sprintf(buf + len,
					       "duty_value=%d%%\n",
					       bl_pwm->pwm_duty);
			}
		}
		break;
	case BL_CTRL_PWM_ARRAY:
		for (i = 0; i < 4; i++) {
			if (bdrv->bconf.bl_pwm_array[i]) {
				bl_pwm = bdrv->bconf.bl_pwm_array[i];
				len += sprintf(buf + len,
					       "pwm_%d: freq=%d, phase=%d, pol=%d, duty_max=%d, duty_min=%d, ",
					       i, bl_pwm->pwm_freq, bl_pwm->pwm_phase,
					       bl_pwm->pwm_method, bl_pwm->pwm_duty_max,
					       bl_pwm->pwm_duty_min);
				if (bl_pwm->pwm_duty_max > 100) {
					len += sprintf(buf + len,
					"duty_value=%d(%d%%)\n",
					bl_pwm->pwm_duty,
					bl_pwm->pwm_duty * 100 / bl_pwm->pwm_duty_max);
				} else {
					len += sprintf(buf + len,
						       "duty_value=%d%%\n",
						       bl_pwm->pwm_duty);
				}
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
#define BL_DEBUG_PWM_PHASE        5
static void bl_debug_pwm_set(struct aml_bl_drv_s *bdrv, unsigned int index,
			     unsigned int value, int state)
{
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned long long temp;
	unsigned int pwm_range, temp_duty;

	if (aml_bl_check_driver(bdrv))
		return;

	mutex_lock(&bl_level_mutex);

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
			BLERR("pwm index(%d) out of range(0-3)\n", index);
			mutex_unlock(&bl_level_mutex);
			return;
		}
		break;
	default:
		BLERR("not pwm control method\n");
		break;
	}
	if (bl_pwm) {
		switch (state) {
		case BL_DEBUG_PWM_FREQ:
			bl_pwm->pwm_freq = value;
			if (bl_pwm->pwm_freq < 50)
				bl_pwm->pwm_freq = 50;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: set index(%d) pwm_port(0x%x) freq: %dHz\n",
				     bdrv->index, index,
				     bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
			break;
		case BL_DEBUG_PWM_DUTY:
			bl_pwm->pwm_duty = value;
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty);
				}
			}
			break;
		case BL_DEBUG_PWM_POL:
			bl_pwm->pwm_method = value;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: set index(%d) pwm_port(0x%x) method: %d\n",
				     bdrv->index, index, bl_pwm->pwm_port,
				     bl_pwm->pwm_method);
			}
			break;
		case BL_DEBUG_PWM_DUTY_MAX:
			bl_pwm->pwm_duty_max = value;
			if (bl_pwm->pwm_duty_max > 255)
				pwm_range = 40950;
			else if (bl_pwm->pwm_duty_max > 100)
				pwm_range = 2550;
			else
				pwm_range = 1000;
			temp = bl_pwm->pwm_level;
			temp_duty = bl_do_div((temp * pwm_range),
					      bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty = (temp_duty + 5) / 10;
			temp = bl_pwm->pwm_min;
			temp_duty = bl_do_div((temp * pwm_range),
					      bl_pwm->pwm_cnt);
			bl_pwm->pwm_duty_min = (temp_duty + 5) / 10;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_max: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_max);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_max: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_max);
				}
			}
			break;
		case BL_DEBUG_PWM_DUTY_MIN:
			bl_pwm->pwm_duty_min = value;
			bl_pwm_config_init(bl_pwm);
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				if (bl_pwm->pwm_duty_max > 100) {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_min: %d\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_min);
				} else {
					BLPR("[%d]: set index(%d) pwm_port(0x%x) duty_min: %d%%\n",
					     bdrv->index, index,
					     bl_pwm->pwm_port,
					     bl_pwm->pwm_duty_min);
				}
			}
			break;
		case BL_DEBUG_PWM_PHASE:
			bl_pwm->pwm_phase = value;
			bl_pwm_set_duty(bdrv, bl_pwm);
			if (lcd_debug_print_flag & LCD_DBG_PR_BL_NORMAL) {
				BLPR("[%d]: set index(%d) pwm_port(0x%x) phase: %d\n",
				     bdrv->index, index, bl_pwm->pwm_port,
				     bl_pwm->pwm_phase);
			}
			break;
		default:
			break;
		}
	}
	mutex_unlock(&bl_level_mutex);
}

static ssize_t bl_debug_pwm_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int ret;
	unsigned int index = 0, val = 0;
	unsigned int val1 = 0, val2 = 0, val3 = 0, val4 = 0, val5 = 0, val6 = 0, val7 = 0;
	struct bl_config_s *bconf = &bdrv->bconf;
	struct bl_pwm_config_s *bl_pwm = NULL;

	switch (buf[0]) {
	case 'f':
		if (buf[3] == 'q') { /* frequency */
			ret = sscanf(buf, "freq %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_FREQ);
			} else {
				BLERR("invalid parameters\n");
			}
		} else if (buf[3] == 'e') { /* duty free */
			ret = sscanf(buf, "free %d", &val);
			if (ret == 1) {
				bdrv->pwm_duty_free = (unsigned char)val;
				/*BLPR("[d]: set pwm_duty_free: %d\n",
				 *   bdrv->index, bdrv->pwm_duty_free);
				 */
			} else {
				BLERR("invalid parameters\n");
			}
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d %d", &index, &val);
		if (ret == 2)
			bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_DUTY);
		else
			BLERR("invalid parameters\n");
		break;
	case 'p':
		if (buf[1] == 'o') {/* polarity */
			ret = sscanf(buf, "pol %d %d", &index, &val);
			if (ret == 2)
				bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_POL);
			else
				BLERR("invalid parameters\n");
		} else if (buf[1] == 'h') {/* phase */
			ret = sscanf(buf, "pol %d %d", &index, &val);
			if (ret == 2)
				bl_debug_pwm_set(bdrv, index, val, BL_DEBUG_PWM_PHASE);
			else
				BLERR("invalid parameters\n");
		}
		break;
	case 'b': /* bypass */
		ret = sscanf(buf, "bypass %d", &val);
		if (ret == 1) {
			bdrv->pwm_bypass = (unsigned char)val;
			BLPR("[%d]: set pwm_bypass: %d\n",
			     bdrv->index, bdrv->pwm_bypass);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'm':
		if (buf[1] == 'a') { /* max */
			ret = sscanf(buf, "max %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MAX);
			} else {
				BLERR("invalid parameters\n");
			}
		} else if (buf[1] == 'i') { /* min */
			ret = sscanf(buf, "min %d %d", &index, &val);
			if (ret == 2) {
				bl_debug_pwm_set(bdrv, index, val,
						 BL_DEBUG_PWM_DUTY_MIN);
			} else {
				BLERR("invalid parameters\n");
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
					BLERR("pwm index(%d) out of range(0-3)\n",
					      index);
					return -EINVAL;
				}
				break;
			default:
				BLERR("not pwm control method\n");
				break;
			}
			if (bl_pwm) {
				bl_pwm->pwm_mapping[0] = val;
				bl_pwm->pwm_mapping[1] = val1;
				bl_pwm->pwm_mapping[2] = val2;
				bl_pwm->pwm_mapping[3] = val3;
				bl_pwm->pwm_mapping[4] = val4;
				bl_pwm->pwm_mapping[5] = val5;
				bl_pwm->pwm_mapping[6] = val6;
				bl_pwm->pwm_mapping[7] = val7;
			}
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	default:
		BLERR("wrong command\n");
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
		BLERR("invalid data\n");
		return -EINVAL;
	}

	BLPR("[%d]: power control: %u\n", bdrv->index, temp);
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
		BLERR("invalid data\n");
		return -EINVAL;
	}

	bdrv->step_on_flag = (unsigned char)temp;
	BLPR("[%d]: step_on: %u\n", bdrv->index, bdrv->step_on_flag);

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
		pr_info("[%d]: set bl power_delay: on=%dms, off=%dms\n",
			bdrv->index, val[0], val[1]);
	} else {
		pr_info("invalid data\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t bl_debug_brightness_bypass_show(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", bdrv->brightness_bypass);
}

static ssize_t bl_debug_brightness_bypass_store(struct device *dev,
						struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int temp;
	int ret;

	ret = kstrtouint(buf, 10, &temp);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}
	bdrv->brightness_bypass = (unsigned char)temp;
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
		if (lcd_debug_print_flag & LCD_DBG_PR_BL_ADV) {
			BLPR("cnt: %d, %d: brightness_buf: %d, level_buf: %d\n",
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
		BLERR("invalid data\n");
		return -EINVAL;
	}

	if (!meter_conf->level_buf) {
		BLERR("get no brightness value\n");
		return  -EINVAL;
	}

	if (temp > (BL_LEVEL_CNT_MAX / 60)) {
		BLPR("max support 60min\n");
		meter_conf->times = BL_LEVEL_CNT_MAX;
	} else {
		meter_conf->times = temp * 60;
	}

	bl_brightness_metrics_calc(bdrv);

	BLPR("time: %d, brightness_metrics: %d, level_metrics: %d\n",
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

	return sprintf(buf, "%d\n", bdrv->level);
}

static ssize_t bl_debug_level_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int level = 0;
	unsigned int ret;

	ret = kstrtouint(buf, 10, &level);
	if (ret != 0) {
		BLERR("invalid data\n");
		return -EINVAL;
	}

	if (!bdrv->debug_force)
		bdrv->debug_force = 1;

	mutex_lock(&bl_level_mutex);

	aml_bl_set_level(bdrv, level);
	BLPR("[%d]: %s: %u, state: 0x%x\n",
	     bdrv->index, __func__, level, bdrv->state);

	mutex_unlock(&bl_level_mutex);

	return count;
}

static ssize_t bl_debug_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct aml_bl_drv_s *bdrv = dev_get_drvdata(dev);
	unsigned int val, temp, ret;

	switch (buf[0]) {
	case 'g': /* gd */
		ret = sscanf(buf, "gd %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;
			if ((bdrv->state & BL_STATE_GD_EN) == 0)
				bl_gd_sel_func(bdrv, 1);
			mutex_lock(&bl_level_mutex);
			bl_gd_diming_func(bdrv, val);
			mutex_unlock(&bl_level_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'b': /* brightness */
		ret = sscanf(buf, "brightness %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			mutex_lock(&bl_status_mutex);
			bdrv->level_brightness = bl_brightness_level_map(bdrv, val);

			if (bdrv->level_brightness == 0) {
				if (bdrv->state & BL_STATE_BL_ON)
					bl_power_off(bdrv);
			} else {
				if ((bdrv->state & BL_STATE_GD_EN) == 0) {
					aml_bl_set_level(bdrv, bdrv->level_brightness);
					BLPR("[%d]: debug brightness: %u->%u, state: 0x%x\n",
						bdrv->index, val, bdrv->level_brightness,
						bdrv->state);
				} else {
					temp = bl_gd_level_map(bdrv, bdrv->level_gd);
					aml_bl_set_level(bdrv, temp);
					BLPR("[%d]: debug brightness: %u->%u, state: 0x%x\n",
						bdrv->index, val, temp, bdrv->state);
				}

				if ((bdrv->state & BL_STATE_BL_ON) == 0)
					bl_power_on(bdrv);
			}
			mutex_unlock(&bl_status_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'l': /* level */
		ret = sscanf(buf, "level %d", &val);
		if (ret == 1) {
			if (!bdrv->debug_force)
				bdrv->debug_force = 1;
			if ((bdrv->state & BL_STATE_DEBUG_FORCE_EN) == 0)
				bdrv->state |= BL_STATE_DEBUG_FORCE_EN;

			mutex_lock(&bl_level_mutex);

			temp = bl_brightness_level_map(bdrv, val);
			aml_bl_set_level(bdrv, temp);
			BLPR("[%d]: debug level: %u, state: 0x%x\n",
				bdrv->index, temp, bdrv->state);

			mutex_unlock(&bl_level_mutex);
		} else {
			BLERR("invalid parameters\n");
		}
		break;
	case 'o': /* off */
		if (bdrv->state & BL_STATE_GD_EN) {
			temp = 0;
			aml_lcd_atomic_notifier_call_chain(LCD_EVENT_BACKLIGHT_GD_SEL, &temp);
		}
		bdrv->state &= ~BL_STATE_DEBUG_FORCE_EN;
		bdrv->debug_force = 0;
		break;
	default:
		BLERR("wrong command\n");
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
		BLERR("pwm port switch not support for pwm array\n");
		return -EINVAL;
	default:
		break;
	}

	if (!bl_pwm) {
		BLERR("%s, bl_pwm is NULL\n", __func__);
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
		aml_lcd_atomic_notifier_call_chain(LCD_EVENT_BACKLIGHT_PWM_SWITCH, &switch_state);
	else
		BLERR("invalid parameters\n");

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
	__ATTR(brightness_bypass, 0644, bl_debug_brightness_bypass_show,
	       bl_debug_brightness_bypass_store),
#ifdef BL_BRIGHTNESS_METER
	__ATTR(brightness_metrics, 0644, bl_brightness_metrics_show,
	       bl_brightness_metrics_store),
#endif
	__ATTR(debug_level, 0644, bl_debug_level_show, bl_debug_level_store),
	__ATTR(debug, 0644, bl_debug_help, bl_debug_store),
	__ATTR(switch_pwm_port, 0644, bl_pwm_prot_show, bl_pwm_port_switch_store),
};

static int bl_debug_file_creat(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++) {
		if (device_create_file(bdrv->dev, &bl_debug_attrs[i])) {
			BLERR("[%d]: create debug attribute %s fail\n",
			       bdrv->index, bl_debug_attrs[i].attr.name);
		}
	}

	return 0;
}

static int bl_debug_file_remove(struct aml_bl_drv_s *bdrv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bl_debug_attrs); i++)
		device_remove_file(bdrv->dev, &bl_debug_attrs[i]);

	return 0;
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
static struct bl_data_s bl_data_axg = {
	.chip_type = LCD_CHIP_AXG,
	.chip_name = "axg",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_g12a = {
	.chip_type = LCD_CHIP_G12A,
	.chip_name = "g12a",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_g12b = {
	.chip_type = LCD_CHIP_G12B,
	.chip_name = "g12b",
	.pwm_vs_flag = 0,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct bl_data_s bl_data_tl1 = {
	.chip_type = LCD_CHIP_TL1,
	.chip_name = "tl1",
	.pwm_vs_flag = 1,
};
#endif

static struct bl_data_s bl_data_sm1 = {
	.chip_type = LCD_CHIP_SM1,
	.chip_name = "sm1",
	.pwm_vs_flag = 0,
};

static struct bl_data_s bl_data_tm2 = {
	.chip_type = LCD_CHIP_TM2,
	.chip_name = "tm2",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_tm2b = {
	.chip_type = LCD_CHIP_TM2B,
	.chip_name = "tm2b",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5 = {
	.chip_type = LCD_CHIP_T5,
	.chip_name = "t5",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5d = {
	.chip_type = LCD_CHIP_T5D,
	.chip_name = "t5d",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t7 = {
	.chip_type = LCD_CHIP_T7,
	.chip_name = "t7",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t3 = {
	.chip_type = LCD_CHIP_T3,
	.chip_name = "t3",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5w = {
	.chip_type = LCD_CHIP_T5W,
	.chip_name = "t5w",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_c3 = {
	.chip_type = LCD_CHIP_C3,
	.chip_name = "c3",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t5m = {
	.chip_type = LCD_CHIP_T5M,
	.chip_name = "t5m",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t3x = {
	.chip_type = LCD_CHIP_T3X,
	.chip_name = "t3x",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_txhd2 = {
	.chip_type = LCD_CHIP_TXHD2,
	.chip_name = "txhd2",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_s6 = {
	.chip_type = LCD_CHIP_S6,
	.chip_name = "s6",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t6d = {
	.chip_type = LCD_CHIP_T6D,
	.chip_name = "t6d",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t6w = {
	.chip_type = LCD_CHIP_T6W,
	.chip_name = "t6w",
	.pwm_vs_flag = 1,
};

static struct bl_data_s bl_data_t6x = {
	.chip_type = LCD_CHIP_T6X,
	.chip_name = "t6x",
	.pwm_vs_flag = 1,
};

static const struct of_device_id bl_dt_match_table[] = {
	{
		.compatible = "amlogic, backlight-axg",
		.data = &bl_data_axg,
	},
	{
		.compatible = "amlogic, backlight-g12a",
		.data = &bl_data_g12a,
	},
	{
		.compatible = "amlogic, backlight-g12b",
		.data = &bl_data_g12b,
	},
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, backlight-tl1",
		.data = &bl_data_tl1,
	},
#endif
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
		.compatible = "amlogic, backlight-t5",
		.data = &bl_data_t5,
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
	{}
};
#endif

static void bl_init_status_update(struct aml_bl_drv_s *bdrv)
{
	struct aml_lcd_drv_s *pdrv;

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

	bdrv->level_brightness = bl_brightness_level_map(bdrv, bdrv->bldev->props.brightness);
	bdrv->level = bdrv->level_brightness;

	/* default disable lcd & backlight */
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return;

	/* update bl status */
	bdrv->state |= BL_STATE_LCD_ON;
	if (pdrv->boot_ctrl->bl_state)
		bdrv->state |= BL_STATE_BL_ON;
	bdrv->on_request = 1;

	mutex_lock(&bl_level_mutex);
	if (bdrv->brightness_bypass)
		aml_bl_set_level(bdrv, bdrv->level_init_on);
	else
		aml_bl_init_level(bdrv, bdrv->level_brightness);
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

	bl_pwm_mapping_init(bdrv);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = (bdrv->bconf.level_max > 0 ?
				bdrv->bconf.level_max : BL_LEVEL_MAX);
	props.brightness = bdrv->level_init_on;

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

	bdrv->bconf.level_default = 128;
	bdrv->bconf.level_mid = 128;
	bdrv->bconf.level_mid_mapping = 128;
	bdrv->bconf.level_min = 10;
	bdrv->bconf.level_max = 255;
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

	ret = bl_config_load(bdrv, bdrv->pdev, bl_level_bootup[bdrv->index]);
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
		ret = kstrtouint(str, 10, &bl_level_bootup[0]);
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
		ret = kstrtouint(str, 10, &bl_level_bootup[1]);
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
		ret = kstrtouint(str, 10, &bl_level_bootup[2]);
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
