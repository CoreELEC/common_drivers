/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef DPSS_PROCESS_EXT_H
#define DPSS_PROCESS_EXT_H

#include <linux/amlogic/media/vfm/vframe.h>

int set_display_speed(int sync_ins_id, int speed);
/**
 * @brief  set_display_speed  set video play speed
 *
 * @param[in]  sync_ins_id    decoder instance id
 * @param[in]  speed          play speed(*1000), 0.5x	500
 * @return     return set result 0:success, -1:fail
 */
#endif /* DPSS_PROCESS_EXT_H */
