/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_eDPTX_NOTIFY_H
#define _AMLOGIC_eDPTX_NOTIFY_H

#include <linux/notifier.h>

#define DPTX_PRIORITY_HPD_IGNORE       31

#define DPTX_PRIORITY_BACKLIGHT_OFF    9
#define DPTX_PRIORITY_SCREEN_BLACK     8
#define DPTX_PRIORITY_LINK_OFF         7
#define DPTX_PRIORITY_TX_CLOSE         6

#define DPTX_PRIORITY_TX_READY         5
#define DPTX_PRIORITY_LINK_ON          4
#define DPTX_PRIORITY_SCREEN_RESTORE   3
#define DPTX_PRIORITY_BACKLIGHT_ON     2

#define DPTX_PRIORITY_HPD_CHECK        1
#define DPTX_PRIORITY_HPD_RESTORE      0

/* original event */
#define eDPTX_EVENT_BACKLIGHT_OFF       BIT(0)
#define eDPTX_EVENT_SCREEN_BLACK        BIT(1)
#define eDPTX_EVENT_LINK_OFF            BIT(2)
#define eDPTX_EVENT_TX_CLOSE            BIT(3)

#define eDPTX_EVENT_TX_READY            BIT(4) // IP PWR
#define eDPTX_EVENT_HPD_CHECK           BIT(5)
#define eDPTX_EVENT_LINK_ON             BIT(6)
#define eDPTX_EVENT_SCREEN_RESTORE      BIT(7)

#define eDPTX_EVENT_BACKLIGHT_ON        BIT(8)
#define eDPTX_EVENT_HPD_IGNORE          BIT(9)
#define eDPTX_EVENT_HPD_RESTORE         BIT(10)

#define eDPTX_EVENT_DISP_OFF          \
	(eDPTX_EVENT_HPD_IGNORE |     \
	 eDPTX_EVENT_BACKLIGHT_OFF | eDPTX_EVENT_SCREEN_BLACK | eDPTX_EVENT_LINK_OFF)
#define eDPTX_EVENT_DISP_ON        (eDPTX_EVENT_HPD_CHECK)

#define eDPTX_EVENT_PLUG_OUT       (eDPTX_EVENT_DISP_OFF | eDPTX_EVENT_HPD_RESTORE)
#define eDPTX_EVENT_PLUG_IN           \
	(eDPTX_EVENT_HPD_IGNORE |     \
	 eDPTX_EVENT_LINK_ON | eDPTX_EVENT_SCREEN_RESTORE | eDPTX_EVENT_BACKLIGHT_ON | \
	 eDPTX_EVENT_HPD_RESTORE)

#define eDPTX_EVENT_UNPREPARE  (eDPTX_EVENT_DISP_OFF | eDPTX_EVENT_TX_CLOSE)
#define eDPTX_EVENT_PREPARE (eDPTX_EVENT_TX_READY | eDPTX_EVENT_HPD_CHECK | eDPTX_EVENT_HPD_RESTORE)

#define eDPTX_EVENT_ENABLE         (eDPTX_EVENT_PREPARE)
#define eDPTX_EVENT_DISABLE        (eDPTX_EVENT_UNPREPARE)

int edptx_notifier_register(struct notifier_block *nb);
int edptx_notifier_unregister(struct notifier_block *nb);
int edptx_notifier_call_chain(unsigned long event, void *v);

#endif /* _INC_LCD_NOTIFY_H_ */
