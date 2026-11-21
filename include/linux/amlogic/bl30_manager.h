/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2026 Team CoreELEC
 */

#ifndef __AML_BL30_MANAGER_H__
#define __AML_BL30_MANAGER_H__

#include <linux/kconfig.h>

#if IS_REACHABLE(CONFIG_BL30_MANAGER)
const char *bl30_manager_get_gpiopower(void);
#else
static inline const char *bl30_manager_get_gpiopower(void)
{
	return NULL;
}
#endif

#endif /* __AML_BL30_MANAGER_H__ */
