/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __T7_MBOX_H__
#define __T7_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define T7_AO2REE0       0
#define T7_AO2REE1       1
#define T7_AO2REE2       2
#define T7_AO2REE3       3
#define T7_AO2REE4       4
#define T7_AO2REE5       5
#define T7_AO2REE6       6
#define T7_AO2REE7       7
#define T7_AO2REE8       8
#define T7_AO2REE9       9
#define T7_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define T7_REE2AO0       64
#define T7_REE2AO1       65
#define T7_REE2AO2       66
#define T7_REE2AO3       67
#define T7_REE2AO4       68
#define T7_REE2AO5       69
#define T7_REE2AO6       70
#define T7_REE2AO7       71
#define T7_REE2AO8       72
#define T7_REE2AO9       73
#define T7_REE2AO10      74

/* DSPA to ARMREE driver ID */
#define T7_DSPA2REE0     129
#define T7_DSPA2REE1     130
#define T7_DSPA2REE2     131
#define T7_DSPA2REE3     132
#define T7_DSPA2REE4     133

/* ARMREE to DSPA driver ID */
#define T7_REE2DSPA0     161
#define T7_REE2DSPA1     162
#define T7_REE2DSPA2     163
#define T7_REE2DSPA3     164
#define T7_REE2DSPA4     165

/* DSPB to ARMREE driver ID */
#define T7_DSPB2REE0     145
#define T7_DSPB2REE1     146
#define T7_DSPB2REE2     147
#define T7_DSPB2REE3     148
#define T7_DSPB2REE4     149

/* ARMREE to DSPB driver ID */
#define T7_REE2DSPB0     177
#define T7_REE2DSPB1     178
#define T7_REE2DSPB2     179
#define T7_REE2DSPB3     180
#define T7_REE2DSPB4     181

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define T7_AO2REE_DEV       T7_AO2REE0
#define T7_AO2REE_VRTC      T7_AO2REE1
#define T7_AO2REE_RTC       T7_AO2REE2
#define T7_AO2REE_KEYPAD    T7_AO2REE3
#define T7_AO2REE_AOCEC     T7_AO2REE4
#define T7_AO2REE_LED       T7_AO2REE5
#define T7_AO2REE_ETH       T7_AO2REE6
#define T7_AO2REE_SPINLOCK  T7_AO2REE7

/* ARMREE to AOCPU client ID */
#define T7_REE2AO_DRV       T7_REE2AO0
#define T7_REE2AO_DEV       T7_REE2AO1
#define T7_REE2AO_VRTC      T7_REE2AO2
#define T7_REE2AO_RTC       T7_REE2AO3
#define T7_REE2AO_KEYPAD    T7_REE2AO4
#define T7_REE2AO_AOCEC     T7_REE2AO5
#define T7_REE2AO_LED       T7_REE2AO6
#define T7_REE2AO_ETH       T7_REE2AO7
#define T7_REE2AO_SPINLOCK  T7_REE2AO8
#define T7_REE2AO_IR        T7_REE2AO9

/* ARMREE to DSPA client ID */
#define T7_REE2DSPA_DEV     T7_REE2DSPA0
#define T7_REE2DSPA_DSP     T7_REE2DSPA1

/* DSPA to ARMREE client ID */
#define T7_DSPA2REE_DEV     T7_DSPA2REE0

/* ARMREE to DSPB client ID */
#define T7_REE2DSPB_DEV     T7_REE2DSPB0
#define T7_REE2DSPB_DSP     T7_REE2DSPB1

/* DSPB to ARMREE client ID */
#define T7_DSPB2REE_DEV     T7_DSPB2REE0

/* MBOX CHANNEL ID */
#define T7_MBOX_DSPA2REE  0
#define T7_MBOX_REE2DSPA  1
#define T7_MBOX_AO2REE    2
#define T7_MBOX_REE2AO    3
#define T7_MBOX_DSPB2REE  6
#define T7_MBOX_REE2DSPB  7
#define T7_MBOX_NUMS      6

#endif /* __T7_MBOX_H__ */
