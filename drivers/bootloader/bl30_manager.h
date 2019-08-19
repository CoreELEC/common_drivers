/*
 * Driver for manage data between config.ini and bootloader blob bl30
 *
 * Copyright (C) 2022 Team CoreELEC
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef _BL30_MANAGER_H_
#define _BL30_MANAGER_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_mbox.h>

#define DRIVER_NAME	"bl30_manager"

#define MBOX_HEAD_SIZE	0x1C

enum aml_mbox_client_id {
	AML_MBOX_CL_NONE,
	AML_MBOX_CL_CLOCKS,
	AML_MBOX_CL_DVFS,
	AML_MBOX_CL_POWER,
	AML_MBOX_CL_THERMAL,
	AML_MBOX_CL_REMOTE,
	AML_MBOX_CL_LED_TIMER,
	AML_MBOX_CL_SET_CEC_DATA,
	AML_MBOX_CL_WOL = 100,
	AML_MBOX_CL_IRPROTO,
	AML_MBOX_CL_REMOTE_MASK,
	AML_MBOX_CL_5V_SYSTEM_POWER,
	AML_MBOX_MAX = 0xff,
};

enum aml_mbox_error_codes {
	AML_MBOX_SUCCESS        = 0, /* Success */
	AML_MBOX_ERR_PARAM      = 1, /* Invalid parameter(s) */
	AML_MBOX_ERR_ALIGN      = 2, /* Invalid alignment */
	AML_MBOX_ERR_SIZE       = 3, /* Invalid size */
	AML_MBOX_ERR_HANDLER    = 4, /* Invalid handler/callback */
	AML_MBOX_ERR_ACCESS     = 5, /* Invalid access/permission denied */
	AML_MBOX_ERR_RANGE      = 6, /* Value out of range */
	AML_MBOX_ERR_TIMEOUT    = 7, /* Timeout has occurred */
	AML_MBOX_ERR_NOMEM      = 8, /* Invalid memory area or pointer */
	AML_MBOX_ERR_PWRSTATE   = 9, /* Invalid power state */
	AML_MBOX_ERR_SUPPORT    = 10, /* Not supported or disabled */
	AML_MBOX_ERR_DEVICE     = 11, /* Device error */
	AML_MBOX_ERR_MAX
};

#endif /* _BL30_MANAGER_H_ */
