/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __T5M_MBOX_H__
#define __T5M_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define T5M_AO2REE0       0
#define T5M_AO2REE1       1
#define T5M_AO2REE2       2
#define T5M_AO2REE3       3
#define T5M_AO2REE4       4
#define T5M_AO2REE5       5
#define T5M_AO2REE6       6
#define T5M_AO2REE7       7
#define T5M_AO2REE8       8
#define T5M_AO2REE9       9
#define T5M_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define T5M_REE2AO0       64
#define T5M_REE2AO1       65
#define T5M_REE2AO2       66
#define T5M_REE2AO3       67
#define T5M_REE2AO4       68
#define T5M_REE2AO5       69
#define T5M_REE2AO6       70
#define T5M_REE2AO7       71
#define T5M_REE2AO8       72
#define T5M_REE2AO9       73
#define T5M_REE2AO10      74

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define T5M_AO2REE_DEV       T5M_AO2REE0
#define T5M_AO2REE_VRTC      T5M_AO2REE1
#define T5M_AO2REE_RTC       T5M_AO2REE2
#define T5M_AO2REE_KEYPAD    T5M_AO2REE3
#define T5M_AO2REE_AOCEC     T5M_AO2REE4
#define T5M_AO2REE_LED       T5M_AO2REE5
#define T5M_AO2REE_ETH       T5M_AO2REE6
#define T5M_AO2REE_SPINLOCK  T5M_AO2REE7

/* ARMREE to AOCPU client ID */
#define T5M_REE2AO_DRV       T5M_REE2AO0
#define T5M_REE2AO_DEV       T5M_REE2AO1
#define T5M_REE2AO_VRTC      T5M_REE2AO2
#define T5M_REE2AO_RTC       T5M_REE2AO3
#define T5M_REE2AO_KEYPAD    T5M_REE2AO4
#define T5M_REE2AO_AOCEC     T5M_REE2AO5
#define T5M_REE2AO_LED       T5M_REE2AO6
#define T5M_REE2AO_ETH       T5M_REE2AO7
#define T5M_REE2AO_SPINLOCK  T5M_REE2AO8
#define T5M_REE2AO_IR        T5M_REE2AO9
#define T5M_REE2AO_WATCHKEY  T5M_REE2AO10

/* MBOX CHANNEL ID */
#define T5M_MBOX_AO2REE    2
#define T5M_MBOX_REE2AO    3
#define T5M_MBOX_NUMS      2

// AOCPU STATUS MBOX CHANNEL ID
#define T5M_MBOX_AO2TEE    4

#endif /* __T5M_MBOX_H__ */
