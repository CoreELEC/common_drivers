/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __S7_MBOX_H__
#define __S7_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define S7_AO2REE0       0
#define S7_AO2REE1       1
#define S7_AO2REE2       2
#define S7_AO2REE3       3
#define S7_AO2REE4       4
#define S7_AO2REE5       5
#define S7_AO2REE6       6
#define S7_AO2REE7       7
#define S7_AO2REE8       8
#define S7_AO2REE9       9
#define S7_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define S7_REE2AO0       64
#define S7_REE2AO1       65
#define S7_REE2AO2       66
#define S7_REE2AO3       67
#define S7_REE2AO4       68
#define S7_REE2AO5       69
#define S7_REE2AO6       70
#define S7_REE2AO7       71
#define S7_REE2AO8       72
#define S7_REE2AO9       73
#define S7_REE2AO10      74

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define S7_AO2REE_DEV       S7_AO2REE0
#define S7_AO2REE_VRTC      S7_AO2REE1
#define S7_AO2REE_RTC       S7_AO2REE2
#define S7_AO2REE_KEYPAD    S7_AO2REE3
#define S7_AO2REE_AOCEC     S7_AO2REE4
#define S7_AO2REE_LED       S7_AO2REE5
#define S7_AO2REE_ETH       S7_AO2REE6
#define S7_AO2REE_SPINLOCK  S7_AO2REE7

/* ARMREE to AOCPU client ID */
#define S7_REE2AO_DRV       S7_REE2AO0
#define S7_REE2AO_DEV       S7_REE2AO1
#define S7_REE2AO_VRTC      S7_REE2AO2
#define S7_REE2AO_RTC       S7_REE2AO3
#define S7_REE2AO_KEYPAD    S7_REE2AO4
#define S7_REE2AO_AOCEC     S7_REE2AO5
#define S7_REE2AO_LED       S7_REE2AO6
#define S7_REE2AO_ETH       S7_REE2AO7
#define S7_REE2AO_SPINLOCK  S7_REE2AO8
#define S7_REE2AO_IR        S7_REE2AO9

/* MBOX CHANNEL ID */
#define S7_MBOX_AO2REE    2
#define S7_MBOX_REE2AO    3
#define S7_MBOX_NUMS      2

/* AOCPU STATUS MBOX CHANNEL ID */
#define S7_MBOX_AO2TEE    4

#endif /* __S7_MBOX_H__ */
