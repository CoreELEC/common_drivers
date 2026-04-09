/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __MESON_PM_H
#define __MESON_PM_H

/*
 * Meson PM is used to unify the standby mode definitions across different stages.
 * The userspace can set the required standby mode through DTS or node configuration.
 * It is a 32-bit value, defined as follows:
 *  +--------+----------------------------+-------------+
 *  | 31:30  |          29:2             |    1:0      |
 *  |Version |         Reserved          |  DDR mode   |
 *  +--------+----------------------------+-------------+
 *
 * - Version   : Bit 31~30, used for versioning (must increment on changes)
 * - Reserved  : Bit 29~2, reserved for future use
 * - DDR mode  : Bit 1~0, specifies the DDR low-power mode
 *
 * Note: All stages share this same definition. Whenever the content is added or removed,
 * it must be updated in all stages accordingly.
 */

/* [0 1 2 3] It must be updated when the content changes. */
#define VERSION_PM_VERSION		2

#define DDR_IDX		0
#define DDR_MASK	0x3
#define SENSOR_IDX	2
#define SENSOR_MASK	0x1
#define EE_PWR_DN_IDX	3
#define EE_PWR_DN_MASK	0x1
#define VERSION_IDX	30
#define VERSION_MASK	0x3

#define DDR_NORMAL	0
#define DDR_SELFREFRESH	1
#define DDR_ASR		2
#define DDR_SHUTDOWN	3

#define EE_PWR_DN_OFF	_OFF_
#define EE_PWR_DN_ON	_ON_

#define SENSOR_OFF	_OFF_
#define SENSOR_ON	_ON_

#define _PM_VERSION	((VERSION_PM_VERSION & VERSION_MASK) << VERSION_IDX)
#define GET_MODULE_VAL(m, v)	(((v) >> (m##_IDX)) & (m##_MASK))
#define _CLR_mask(m, v)		(~((m##_MASK) << (m##_IDX)) & (v))
#define _SET_VAL(m, n_v, v)	((m##_##n_v) << (m##_IDX) | (v))
#define SET_MODULE_VAL(m, n_v, v)	(_SET_VAL(m, n_v, _CLR_mask(m, v)) | _PM_VERSION)
#define DEFINE(m, v)            (_SET_VAL(m, v, 0) | _PM_VERSION)
#define _OFF_     0
#define _ON_      1

#define IS_SAME_PM_VERSION(v)	(GET_MODULE_VAL(VERSION, v) == VERSION_PM_VERSION)

#define SYSTEM_SUSPEND_NORMAL	(DEFINE(DDR, SELFREFRESH) | \
		DEFINE(SENSOR, OFF) | \
		DEFINE(EE_PWR_DN, OFF))
#define SYSTEM_SUSPEND_EE_PWR_DOWN	(DEFINE(DDR, SELFREFRESH) | \
		DEFINE(SENSOR, OFF) | \
		DEFINE(EE_PWR_DN, ON))
#define SYSTEM_SED	(DEFINE(DDR, ASR) | DEFINE(SENSOR, OFF))
#define SYSTEM_COLD_BOOT	(0x00 | _PM_VERSION)

#endif
