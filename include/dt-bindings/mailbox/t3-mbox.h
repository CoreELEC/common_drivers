/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __T3_MBOX_H__
#define __T3_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define T3_AO2REE0       0
#define T3_AO2REE1       1
#define T3_AO2REE2       2
#define T3_AO2REE3       3
#define T3_AO2REE4       4
#define T3_AO2REE5       5
#define T3_AO2REE6       6
#define T3_AO2REE7       7
#define T3_AO2REE8       8
#define T3_AO2REE9       9
#define T3_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define T3_REE2AO0       64
#define T3_REE2AO1       65
#define T3_REE2AO2       66
#define T3_REE2AO3       67
#define T3_REE2AO4       68
#define T3_REE2AO5       69
#define T3_REE2AO6       70
#define T3_REE2AO7       71
#define T3_REE2AO8       72
#define T3_REE2AO9       73
#define T3_REE2AO10      74

/* DSPA to ARMREE driver ID */
#define T3_DSPA2REE0     129
#define T3_DSPA2REE1     130
#define T3_DSPA2REE2     131
#define T3_DSPA2REE3     132
#define T3_DSPA2REE4     133

/* ARMREE to DSPA driver ID */
#define T3_REE2DSPA0     161
#define T3_REE2DSPA1     162
#define T3_REE2DSPA2     163
#define T3_REE2DSPA3     164
#define T3_REE2DSPA4     165

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define T3_AO2REE_DEV       T3_AO2REE0
#define T3_AO2REE_VRTC      T3_AO2REE1
#define T3_AO2REE_RTC       T3_AO2REE2
#define T3_AO2REE_KEYPAD    T3_AO2REE3
#define T3_AO2REE_AOCEC     T3_AO2REE4
#define T3_AO2REE_LED       T3_AO2REE5
#define T3_AO2REE_ETH       T3_AO2REE6
#define T3_AO2REE_SPINLOCK  T3_AO2REE7

/* ARMREE to AOCPU client ID */
#define T3_REE2AO_DRV       T3_REE2AO0
#define T3_REE2AO_DEV       T3_REE2AO1
#define T3_REE2AO_VRTC      T3_REE2AO2
#define T3_REE2AO_RTC       T3_REE2AO3
#define T3_REE2AO_KEYPAD    T3_REE2AO4
#define T3_REE2AO_AOCEC     T3_REE2AO5
#define T3_REE2AO_LED       T3_REE2AO6
#define T3_REE2AO_ETH       T3_REE2AO7
#define T3_REE2AO_SPINLOCK  T3_REE2AO8
#define T3_REE2AO_IR        T3_REE2AO9

/* ARMREE to DSPA client ID */
#define T3_REE2DSPA_DEV     T3_REE2DSPA0
#define T3_REE2DSPA_DSP     T3_REE2DSPA1

/* DSPA to ARMREE client ID */
#define T3_DSPA2REE_DEV     T3_DSPA2REE0

/* MBOX CHANNEL ID */
#define T3_MBOX_DSPA2REE  0
#define T3_MBOX_REE2DSPA  1
#define T3_MBOX_AO2REE    2
#define T3_MBOX_REE2AO    3
#define T3_MBOX_NUMS      4

#endif /* __T3_MBOX_H__ */
