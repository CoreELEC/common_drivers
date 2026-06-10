/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_BACKLIGHT_NOTIFY_H_
#define _INC_BACKLIGHT_NOTIFY_H_

#include <linux/notifier.h>

// /* lcd backlight pwm_vs vfreq change occurred */
// #define BACKLIGHT_EVENT_UPDATE     BIT(25)
// /* lcd backlight brightness update by global dimming */
// #define BACKLIGHT_EVENT_GD_DIM     BIT(26)
// /* lcd backlight brightness on/off by global dimming */
// #define BACKLIGHT_EVENT_GD_SEL     BIT(27)
// /* lcd backlight brightness on/off by local dimming */
// #define BACKLIGHT_EVENT_LD_SEL     BIT(28)
// /* lcd backlight brightness update by driver api */
// #define BACKLIGHT_EVENT_BRIGHTNESS_DIM     BIT(29)
// /* lcd backlight pwm choose switch port */
// #define BACKLIGHT_EVENT_PWM_SWITCH   BIT(30)
#define LCD_EVENT_BL_POWER_ON          BIT(14)
#define LCD_EVENT_BL_POWER_OFF         BIT(15)

/* blocking notify */
int aml_bl_notifier_register(struct notifier_block *nb);
int aml_bl_notifier_unregister(struct notifier_block *nb);
// int aml_bl_notifier_call_chain(unsigned long event, void *v);
/* atomic notify */
int aml_bl_atomic_notifier_register(struct notifier_block *nb);
int aml_bl_atomic_notifier_unregister(struct notifier_block *nb);
// int aml_bl_atomic_notifier_call_chain(unsigned long event, void *v);

#endif /* _INC_LCD_NOTIFY_H_ */
