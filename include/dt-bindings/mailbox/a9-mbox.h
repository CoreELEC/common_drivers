/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __A9_MBOX_H__
#define __A9_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define A9_AO2REE0       0
#define A9_AO2REE1       1
#define A9_AO2REE2       2
#define A9_AO2REE3       3
#define A9_AO2REE4       4
#define A9_AO2REE5       5
#define A9_AO2REE6       6
#define A9_AO2REE7       7
#define A9_AO2REE8       8
#define A9_AO2REE9       9
#define A9_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define A9_REE2AO0       64
#define A9_REE2AO1       65
#define A9_REE2AO2       66
#define A9_REE2AO3       67
#define A9_REE2AO4       68
#define A9_REE2AO5       69
#define A9_REE2AO6       70
#define A9_REE2AO7       71
#define A9_REE2AO8       72
#define A9_REE2AO9       73
#define A9_REE2AO10      74

/* DSPA to ARMREE driver ID */
#define A9_DSPA2REE0     129
#define A9_DSPA2REE1     130
#define A9_DSPA2REE2     131
#define A9_DSPA2REE3     132
#define A9_DSPA2REE4     133

/* ARMREE to DSPA driver ID */
#define A9_REE2DSPA0     161
#define A9_REE2DSPA1     162
#define A9_REE2DSPA2     163
#define A9_REE2DSPA3     164
#define A9_REE2DSPA4     165

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define A9_AO2REE_DEV       A9_AO2REE0
#define A9_AO2REE_VRTC      A9_AO2REE1
#define A9_AO2REE_RTC       A9_AO2REE2
#define A9_AO2REE_KEYPAD    A9_AO2REE3
#define A9_AO2REE_AOCEC     A9_AO2REE4
#define A9_AO2REE_LED       A9_AO2REE5
#define A9_AO2REE_ETH       A9_AO2REE6
#define A9_AO2REE_SPINLOCK  A9_AO2REE7

/* ARMREE to AOCPU client ID */
#define A9_REE2AO_DRV       A9_REE2AO0
#define A9_REE2AO_DEV       A9_REE2AO1
#define A9_REE2AO_VRTC      A9_REE2AO2
#define A9_REE2AO_RTC       A9_REE2AO3
#define A9_REE2AO_KEYPAD    A9_REE2AO4
#define A9_REE2AO_AOCEC     A9_REE2AO5
#define A9_REE2AO_LED       A9_REE2AO6
#define A9_REE2AO_ETH       A9_REE2AO7
#define A9_REE2AO_SPINLOCK  A9_REE2AO8
#define A9_REE2AO_IR        A9_REE2AO9

/* ARMREE to DSPA client ID */
#define A9_REE2DSPA_DEV     A9_REE2DSPA0
#define A9_REE2DSPA_HOST    A9_REE2DSPA1

/* DSPA to ARMREE client ID */
#define A9_DSPA2REE_DEV     A9_DSPA2REE0
#define A9_DSPA2REE_HOST    A9_DSPA2REE1

/* MBOX CHANNEL ID */
#define A9_MBOX_DSPA2REE  0
#define A9_MBOX_REE2DSPA  1
#define A9_MBOX_AO2REE    2
#define A9_MBOX_REE2AO    3
#define A9_MBOX_NUMS      4

/* AOCPU STATUS MBOX CHANNEL ID */
#define A9_AOCPU_STS_CHAN 4

#endif /* __A9_MBOX_H__ */
