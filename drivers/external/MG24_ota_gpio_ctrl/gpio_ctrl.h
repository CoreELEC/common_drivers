/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __SEI_COMMON_CONTROL_H__
#define __SEI_COMMON_CONTROL_H__

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
//struct gpio_ctrl_data {
//    //char name[24];            //reserve
//    //unsigned int state;       //reserve
//    //unsigned int pin;         //reserve
//    struct gpio_desc *gpio_ctrl;
//    unsigned int mode;
//    bool isValid;
//};

struct gpio_ctrl_dev {
    struct gpio_desc *gpio_ctrl;
    unsigned int mode;
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
    struct early_suspend gpio_ctrl_device_early_suspend_handler;
#endif
};

//struct gpio_ctrl_dev *ctl_dev = NULL;

#endif
