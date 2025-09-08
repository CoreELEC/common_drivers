// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include "lnb_controller.h"
#include "gpio_lnbc.h"

static int gpio_lnbc_init(struct lnbc *lnbc);
static int gpio_lnbc_set_config(struct lnbc *lnbc,
		enum LNBC_CONFIG_ID config_id, int value);
static int gpio_lnbc_set_voltage(struct lnbc *lnbc, enum LNBC_VOLTAGE voltage);
static int gpio_lnbc_set_tone(struct lnbc *lnbc, enum LNBC_TONE tone);
static int gpio_lnbc_set_transmit_mode(struct lnbc *lnbc,
		enum LNBC_TRANSMIT_MODE transmit_mode);
static int gpio_lnbc_sleep(struct lnbc *lnbc);
static int gpio_lnbc_wake_up(struct lnbc *lnbc);
static int gpio_lnbc_get_diag_status(struct lnbc *lnbc, unsigned int *status,
		unsigned int *status_supported);

int gpio_lnbc_create(struct lnbc *lnbc, struct gpio_desc *gpio_lnb_en,
		struct gpio_desc *gpio_lnb_sel)
{
	if (!lnbc || !gpio_lnb_en || !gpio_lnb_sel)
		return -EFAULT;

	lnbc->init = gpio_lnbc_init;
	lnbc->set_config = gpio_lnbc_set_config;
	lnbc->set_voltage = gpio_lnbc_set_voltage;
	lnbc->set_tone = gpio_lnbc_set_tone;
	lnbc->set_transmit_mode = gpio_lnbc_set_transmit_mode;
	lnbc->sleep = gpio_lnbc_sleep;
	lnbc->wake_up = gpio_lnbc_wake_up;
	lnbc->get_diag_status = gpio_lnbc_get_diag_status;
	lnbc->is_internal_tone = 0;

	lnbc->gpio_lnb_en = gpio_lnb_en;
	lnbc->gpio_lnb_sel = gpio_lnb_sel;

	lnbc->state = LNBC_STATE_UNKNOWN;
	lnbc->voltage = LNBC_VOLTAGE_LOW;
	lnbc->tone = LNBC_TONE_AUTO;
	lnbc->transmit_mode = LNBC_TRANSMIT_MODE_TX;

	return 0;
}

static int gpio_lnbc_init(struct lnbc *lnbc)
{
	gpiod_set_value(lnbc->gpio_lnb_en, 0);
	gpiod_set_value(lnbc->gpio_lnb_sel, 0);

	lnbc->voltage = LNBC_VOLTAGE_OFF;
	lnbc->state = LNBC_STATE_SLEEP;

	return 0;
}

static int gpio_lnbc_set_config(struct lnbc *lnbc,
		enum LNBC_CONFIG_ID config_id, int value)
{
	return 0;
}

static int gpio_lnbc_set_voltage(struct lnbc *lnbc,
		enum LNBC_VOLTAGE voltage)
{
	if (lnbc->state != LNBC_STATE_ACTIVE && lnbc->state != LNBC_STATE_SLEEP)
		return -EINVAL;

	switch (voltage) {
	case LNBC_VOLTAGE_OFF:
		gpiod_set_value(lnbc->gpio_lnb_en, 0);
		gpiod_set_value(lnbc->gpio_lnb_sel, 0);
		break;

	case LNBC_VOLTAGE_LOW:
		gpiod_set_value(lnbc->gpio_lnb_en, 1);
		gpiod_set_value(lnbc->gpio_lnb_sel, 0);
		break;

	case LNBC_VOLTAGE_HIGH:
		gpiod_set_value(lnbc->gpio_lnb_en, 1);
		gpiod_set_value(lnbc->gpio_lnb_sel, 1);
		break;

	default:
		return -EINVAL;
	}

	if (voltage != LNBC_VOLTAGE_OFF) {
		// switch between 13V and 18V
		if (lnbc->voltage != LNBC_VOLTAGE_OFF) {
			if (!strncasecmp(lnbc->name, "TMI8037", strlen("TMI8037")) ||
				!strncasecmp(lnbc->name, "STI8036", strlen("STI8036")))
				usleep_range(1000, 1500);
			else if (!strncasecmp(lnbc->name, "sgm_41286", strlen("sgm_41286")) ||
				!strncasecmp(lnbc->name, "sgm41286", strlen("sgm41286")))
				usleep_range(6000, 7000);
			else if (!strncasecmp(lnbc->name, "LP6220A", strlen("LP6220A")))
				usleep_range(10000, 11000);
			else
				usleep_range(2000, 2500);
		} else {
			// OFF to 13/18V
			if (!strncasecmp(lnbc->name, "TMI8037", strlen("TMI8037")) ||
				!strncasecmp(lnbc->name, "STI8036", strlen("STI8036")))
				usleep_range(1500, 2000);
			else if (!strncasecmp(lnbc->name, "sgm_41286", strlen("sgm_41286")) ||
				!strncasecmp(lnbc->name, "sgm41286", strlen("sgm41286")))
				usleep_range(20000, 21000);
			else if (!strncasecmp(lnbc->name, "LP6220A", strlen("LP6220A")))
				usleep_range(10000, 11000);
			else
				usleep_range(2000, 2500);
		}
	} else {
		// 13/18V to OFF
		if (lnbc->voltage != LNBC_VOLTAGE_OFF) {
			if (!strncasecmp(lnbc->name, "TMI8037", strlen("TMI8037")) ||
				!strncasecmp(lnbc->name, "STI8036", strlen("STI8036")))
				usleep_range(1000, 1500);
			else if (!strncasecmp(lnbc->name, "sgm_41286", strlen("sgm_41286")) ||
				!strncasecmp(lnbc->name, "sgm41286", strlen("sgm41286")))
				usleep_range(20000, 21000);
			else if (!strncasecmp(lnbc->name, "LP6220A", strlen("LP6220A")))
				usleep_range(10000, 11000);
			else
				usleep_range(2000, 2500);
		}
	}

	lnbc->voltage = voltage;
	lnbc->state = LNBC_STATE_ACTIVE;

	return 0;
}

static int gpio_lnbc_set_tone(struct lnbc *lnbc, enum LNBC_TONE tone)
{
	return 0;
}

static int gpio_lnbc_set_transmit_mode(struct lnbc *lnbc,
		enum LNBC_TRANSMIT_MODE transmit_mode)
{
	return 0;
}

static int gpio_lnbc_sleep(struct lnbc *lnbc)
{
	if (!lnbc)
		return -EFAULT;

	gpiod_set_value(lnbc->gpio_lnb_en, 0);
	gpiod_set_value(lnbc->gpio_lnb_sel, 0);

	lnbc->voltage = LNBC_VOLTAGE_OFF;
	lnbc->state = LNBC_STATE_SLEEP;

	return 0;
}

static int gpio_lnbc_wake_up(struct lnbc *lnbc)
{
	if (!lnbc)
		return -EFAULT;

	lnbc->state = LNBC_STATE_ACTIVE;

	return 0;
}

static int gpio_lnbc_get_diag_status(struct lnbc *lnbc, unsigned int *status,
		unsigned int *status_supported)
{
	return 0;
}
