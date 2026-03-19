/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __S6_MBOX_H__
#define __S6_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define S6_AO2REE0       0
#define S6_AO2REE1       1
#define S6_AO2REE2       2
#define S6_AO2REE3       3
#define S6_AO2REE4       4
#define S6_AO2REE5       5
#define S6_AO2REE6       6
#define S6_AO2REE7       7
#define S6_AO2REE8       8
#define S6_AO2REE9       9
#define S6_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define S6_REE2AO0       64
#define S6_REE2AO1       65
#define S6_REE2AO2       66
#define S6_REE2AO3       67
#define S6_REE2AO4       68
#define S6_REE2AO5       69
#define S6_REE2AO6       70
#define S6_REE2AO7       71
#define S6_REE2AO8       72
#define S6_REE2AO9       73
#define S6_REE2AO10      74

/* DSPA to ARMREE driver ID */
#define S6_DSPA2REE0     129
#define S6_DSPA2REE1     130
#define S6_DSPA2REE2     131
#define S6_DSPA2REE3     132
#define S6_DSPA2REE4     133

/* ARMREE to DSPA driver ID */
#define S6_REE2DSPA0     161
#define S6_REE2DSPA1     162
#define S6_REE2DSPA2     163
#define S6_REE2DSPA3     164
#define S6_REE2DSPA4     165

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define S6_AO2REE_DEV       S6_AO2REE0
#define S6_AO2REE_VRTC      S6_AO2REE1
#define S6_AO2REE_RTC       S6_AO2REE2
#define S6_AO2REE_KEYPAD    S6_AO2REE3
#define S6_AO2REE_AOCEC     S6_AO2REE4
#define S6_AO2REE_LED       S6_AO2REE5
#define S6_AO2REE_ETH       S6_AO2REE6
#define S6_AO2REE_SPINLOCK  S6_AO2REE7

/* ARMREE to AOCPU client ID */
#define S6_REE2AO_DRV       S6_REE2AO0
#define S6_REE2AO_DEV       S6_REE2AO1
#define S6_REE2AO_VRTC      S6_REE2AO2
#define S6_REE2AO_RTC       S6_REE2AO3
#define S6_REE2AO_KEYPAD    S6_REE2AO4
#define S6_REE2AO_AOCEC     S6_REE2AO5
#define S6_REE2AO_LED       S6_REE2AO6
#define S6_REE2AO_ETH       S6_REE2AO7
#define S6_REE2AO_SPINLOCK  S6_REE2AO8
#define S6_REE2AO_IR        S6_REE2AO9

/* ARMREE to DSPA client ID */
#define S6_REE2DSPA_DEV     S6_REE2DSPA0
#define S6_REE2DSPA_HOST    S6_REE2DSPA1

/* DSPA to ARMREE client ID */
#define S6_DSPA2REE_DEV     S6_DSPA2REE0
#define S6_DSPA2REE_HOST    S6_DSPA2REE1

/* MBOX CHANNEL ID */
#define S6_MBOX_DSPA2REE  0
#define S6_MBOX_REE2DSPA  1
#define S6_MBOX_AO2REE    2
#define S6_MBOX_REE2AO    3
#define S6_MBOX_NUMS      4

/* AOCPU STATUS MBOX CHANNEL ID */
#define S6_MBOX_AO2TEE    4

#endif /* __S6_MBOX_H__ */
