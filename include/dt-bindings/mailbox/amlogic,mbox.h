/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AMLOGIC_MBOX_H__
#define __AMLOGIC_MBOX_H__

#define MAILBOX_AOCPU  1
#define MAILBOX_DSP    2
#define MAILBOX_SECPU  3
#define MAILBOX_MF     4
#define MAILBOX_ARM    5

/* Mailbox Driver ID Range Description
 * Direction    ID Range    ID Numbers    Total ID Range    Total ID Numbers
 * AO2ARMREE    0 ~ 63      64            0 ~ 242           243
 * ARMREE2AO    64 ~ 127    64
 * ARMTEE2AO    128         1
 * DSPA2ARMREE  129 ~ 144   16
 * DSPB2ARMREE  145 ~ 160   16
 * ARMREE2DSPA  161 ~ 176   16
 * ARMREE2DSPB  177 ~ 192   16
 * M4A2ARMREE   193 ~ 200   8
 * M4B2ARMREE   201 ~ 208   8
 * ARMREE2M4A   209 ~ 216   8
 * ARMREE2M4B   217 ~ 224   8
 * DSPA2AO      225 ~ 228   4
 * DSPB2AO      229 ~ 232   4
 * AO2DSPA      233 ~ 236   4
 * AO2DSPB      237 ~ 240   4
 * ARMSEC2DSPA  241         1
 * ARMSEC2DSPB  242         1
 */
#define MBOX_DRVID_AO2ARMREE_BEGIN    0
#define MBOX_DRVID_AO2ARMREE_END      63
#define MBOX_DRVID_DSP2ARMREE_BEGIN   129
#define MBOX_DRVID_DSP2ARMREE_END     160
#define MBOX_DRVID_M42ARMREE_BEGIN    193
#define MBOX_DRVID_M42ARMREE_END      208
#define MBOX_ID_MAX  242

#endif /* __AMLOGIC_MBOX_H__ */
