/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _INC_BACKLIGHT_EXPORT_H_
#define _INC_BACKLIGHT_EXPORT_H_

#include <linux/notifier.h>
#include <linux/amlogic/media/vout/backlight/aml_bl.h>

void aml_bl_set_bl_state(u8 bl_idx, u8 state);

struct aml_bl_drv_s *aml_bl_get_driver(u8 index);
int aml_bl_index_add(u8 drv_index, u16 connector_type, u8 conf_index);

int aml_bl_set_level_brightness(u8 bl_idx, int brightness);
int aml_bl_get_level_brightness(u8 bl_idx);

void aml_bl_on_ctrl(u8 bl_idx);
void aml_bl_off_ctrl(u8 bl_idx);

u32 aml_bl_off_time_report(u8 bl_idx);
u32 aml_bl_on_time_report(u8 bl_idx);

/* lcd backlight pwm_vs vfreq change occurred */
#define BACKLIGHT_EVENT_UPDATE     BIT(25)
/* lcd backlight brightness update by global dimming */
#define BACKLIGHT_EVENT_GD_DIM     BIT(26)
/* lcd backlight brightness on/off by global dimming */
#define BACKLIGHT_EVENT_GD_SEL     BIT(27)
/* lcd backlight brightness on/off by local dimming */
#define BACKLIGHT_EVENT_LD_SEL     BIT(28)
/* lcd backlight brightness update by driver api */
#define BACKLIGHT_EVENT_BRIGHTNESS_DIM     BIT(29)
/* lcd backlight pwm choose switch port */
#define BACKLIGHT_EVENT_PWM_SWITCH   BIT(30)

/* blocking notify */
// int aml_bl_notifier_register(struct notifier_block *nb);
// int aml_bl_notifier_unregister(struct notifier_block *nb);
int aml_bl_notifier_call_chain(unsigned long event, void *v);
/* atomic notify */
// int aml_bl_atomic_notifier_register(struct notifier_block *nb);
// int aml_bl_atomic_notifier_unregister(struct notifier_block *nb);
int aml_bl_atomic_notifier_call_chain(unsigned long event, void *v);

void aml_bl_debug_print_set(u8 debug_mask);

#endif /* _INC_LCD_NOTIFY_H_ */
