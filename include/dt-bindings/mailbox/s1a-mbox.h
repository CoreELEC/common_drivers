/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __S1A_MBOX_H__
#define __S1A_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define S1A_AO2REE0       0
#define S1A_AO2REE1       1
#define S1A_AO2REE2       2
#define S1A_AO2REE3       3
#define S1A_AO2REE4       4
#define S1A_AO2REE5       5
#define S1A_AO2REE6       6
#define S1A_AO2REE7       7
#define S1A_AO2REE8       8
#define S1A_AO2REE9       9
#define S1A_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define S1A_REE2AO0       64
#define S1A_REE2AO1       65
#define S1A_REE2AO2       66
#define S1A_REE2AO3       67
#define S1A_REE2AO4       68
#define S1A_REE2AO5       69
#define S1A_REE2AO6       70
#define S1A_REE2AO7       71
#define S1A_REE2AO8       72
#define S1A_REE2AO9       73
#define S1A_REE2AO10      74

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define S1A_AO2REE_DEV       S1A_AO2REE0
#define S1A_AO2REE_VRTC      S1A_AO2REE1
#define S1A_AO2REE_RTC       S1A_AO2REE2
#define S1A_AO2REE_KEYPAD    S1A_AO2REE3
#define S1A_AO2REE_AOCEC     S1A_AO2REE4
#define S1A_AO2REE_LED       S1A_AO2REE5
#define S1A_AO2REE_ETH       S1A_AO2REE6
#define S1A_AO2REE_SPINLOCK  S1A_AO2REE7
#define S1A_AO2REE_FD650     S1A_AO2REE8

/* ARMREE to AOCPU client ID */
#define S1A_REE2AO_DRV       S1A_REE2AO0
#define S1A_REE2AO_DEV       S1A_REE2AO1
#define S1A_REE2AO_VRTC      S1A_REE2AO2
#define S1A_REE2AO_RTC       S1A_REE2AO3
#define S1A_REE2AO_KEYPAD    S1A_REE2AO4
#define S1A_REE2AO_AOCEC     S1A_REE2AO5
#define S1A_REE2AO_LED       S1A_REE2AO6
#define S1A_REE2AO_ETH       S1A_REE2AO7
#define S1A_REE2AO_SPINLOCK  S1A_REE2AO8
#define S1A_REE2AO_FD650     S1A_REE2AO9

/* MBOX CHANNEL ID */
#define S1A_MBOX_AO2REE    2
#define S1A_MBOX_REE2AO    3
#define S1A_MBOX_NUMS      2

/* MBOX SEC CHANNEL ID */
#define S1A_MBOX_REE2SCPU  0
#define S1A_MBOX_SCPU2REE  1

/* ARMREE to SECPU client ID */
#define S1A_REE2SCPU_DEV   S1A_MBOX_REE2SCPU

#endif /* __S1A_MBOX_H__ */
