/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_MBOX_HEALTH_H__
#define __MESON_MBOX_HEALTH_H__

#include "meson_mbox_comm.h"

int mbox_aocpu_health_monitor_start(struct aocpu_health_info *health_info);
int mbox_aocpu_health_monitor_stop(struct aocpu_health_info *health_info);

#endif
