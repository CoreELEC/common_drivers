/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __SC2_MBOX_H__
#define __SC2_MBOX_H__

#include "amlogic,mbox.h"

/* MAILBOX DRIVER ID */
/* AOCPU to ARMREE driver ID */
#define SC2_AO2REE0       0
#define SC2_AO2REE1       1
#define SC2_AO2REE2       2
#define SC2_AO2REE3       3
#define SC2_AO2REE4       4
#define SC2_AO2REE5       5
#define SC2_AO2REE6       6
#define SC2_AO2REE7       7
#define SC2_AO2REE8       8
#define SC2_AO2REE9       9
#define SC2_AO2REE10      10

/* ARMREE to AOCPU driver ID */
#define SC2_REE2AO0       64
#define SC2_REE2AO1       65
#define SC2_REE2AO2       66
#define SC2_REE2AO3       67
#define SC2_REE2AO4       68
#define SC2_REE2AO5       69
#define SC2_REE2AO6       70
#define SC2_REE2AO7       71
#define SC2_REE2AO8       72
#define SC2_REE2AO9       73
#define SC2_REE2AO10      74

/* DSPA to ARMREE driver ID */
#define SC2_DSPA2REE0     129
#define SC2_DSPA2REE1     130
#define SC2_DSPA2REE2     131
#define SC2_DSPA2REE3     132
#define SC2_DSPA2REE4     133

/* ARMREE to DSPA driver ID */
#define SC2_REE2DSPA0     161
#define SC2_REE2DSPA1     162
#define SC2_REE2DSPA2     163
#define SC2_REE2DSPA3     164
#define SC2_REE2DSPA4     165

/* MBOX CLIENT ID */
/* AOCPU to ARMREE client ID */
#define SC2_AO2REE_DEV       SC2_AO2REE0
#define SC2_AO2REE_VRTC      SC2_AO2REE1
#define SC2_AO2REE_RTC       SC2_AO2REE2
#define SC2_AO2REE_KEYPAD    SC2_AO2REE3
#define SC2_AO2REE_AOCEC     SC2_AO2REE4
#define SC2_AO2REE_LED       SC2_AO2REE5
#define SC2_AO2REE_ETH       SC2_AO2REE6
#define SC2_AO2REE_SPINLOCK  SC2_AO2REE7

/* ARMREE to AOCPU client ID */
#define SC2_REE2AO_DRV       SC2_REE2AO0
#define SC2_REE2AO_DEV       SC2_REE2AO1
#define SC2_REE2AO_VRTC      SC2_REE2AO2
#define SC2_REE2AO_RTC       SC2_REE2AO3
#define SC2_REE2AO_KEYPAD    SC2_REE2AO4
#define SC2_REE2AO_AOCEC     SC2_REE2AO5
#define SC2_REE2AO_LED       SC2_REE2AO6
#define SC2_REE2AO_ETH       SC2_REE2AO7
#define SC2_REE2AO_SPINLOCK  SC2_REE2AO8
#define SC2_REE2AO_IR        SC2_REE2AO9

/* ARMREE to DSPA client ID */
#define SC2_REE2DSPA_DEV     SC2_REE2DSPA0
#define SC2_REE2DSPA_HOST    SC2_REE2DSPA1

/* DSPA to ARMREE client ID */
#define SC2_DSPA2REE_DEV     SC2_DSPA2REE0
#define SC2_DSPA2REE_HOST    SC2_DSPA2REE1

/* MBOX CHANNEL ID */
#define SC2_MBOX_DSPA2REE  0
#define SC2_MBOX_REE2DSPA  1
#define SC2_MBOX_AO2REE    2
#define SC2_MBOX_REE2AO    3
#define SC2_MBOX_NUMS      4

/* MBOX SEC CHANNEL ID */
#define SC2_MBOX_REE2SCPU  0
#define SC2_MBOX_SCPU2REE  1

/* ARMREE to SECPU client ID */
#define SC2_REE2SCPU_DEV  SC2_MBOX_REE2SCPU
#define SC2_SCPU2REE_DEV  SC2_MBOX_SCPU2REE

#endif /* __SC2_MBOX_H__ */
