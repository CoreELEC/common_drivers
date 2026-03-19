/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __A4_MBOX_H__
#define __A4_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define A4_AO2REE0       0
#define A4_AO2REE1       1
#define A4_AO2REE2       2
#define A4_AO2REE3       3
#define A4_AO2REE4       4
#define A4_AO2REE5       5
#define A4_AO2REE6       6
#define A4_AO2REE7       7
#define A4_AO2REE8       8
#define A4_AO2REE9       9
#define A4_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define A4_REE2AO0       64
#define A4_REE2AO1       65
#define A4_REE2AO2       66
#define A4_REE2AO3       67
#define A4_REE2AO4       68
#define A4_REE2AO5       69
#define A4_REE2AO6       70
#define A4_REE2AO7       71
#define A4_REE2AO8       72
#define A4_REE2AO9       73
#define A4_REE2AO10      74

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define A4_AO2REE_DEV        A4_AO2REE0
#define A4_AO2REE_VRTC       A4_AO2REE1
#define A4_AO2REE_RTC        A4_AO2REE2
#define A4_AO2REE_KEYPAD     A4_AO2REE3
#define A4_AO2REE_AOCEC      A4_AO2REE4
#define A4_AO2REE_LED        A4_AO2REE5
#define A4_AO2REE_ETH        A4_AO2REE6
#define A4_AO2REE_SPINLOCK   A4_AO2REE7
#define A4_AO2REE_BTCLK_SYNC A4_AO2REE8

/* ARMREE to AOCPU client ID */
#define A4_REE2AO_DRV       A4_REE2AO0
#define A4_REE2AO_DEV       A4_REE2AO1
#define A4_REE2AO_VRTC      A4_REE2AO2
#define A4_REE2AO_RTC       A4_REE2AO3
#define A4_REE2AO_KEYPAD    A4_REE2AO4
#define A4_REE2AO_AOCEC     A4_REE2AO5
#define A4_REE2AO_LED       A4_REE2AO6
#define A4_REE2AO_ETH       A4_REE2AO7
#define A4_REE2AO_SPINLOCK  A4_REE2AO8
#define A4_REE2AO_IR        A4_REE2AO9

/* MBOX CHANNEL ID */
#define A4_MBOX_AO2REE    2
#define A4_MBOX_REE2AO    3
#define A4_MBOX_NUMS      2

#endif /* __A4_MBOX_H__ */
