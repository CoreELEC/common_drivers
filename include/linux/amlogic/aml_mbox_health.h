/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_MBOX_HEALTH_H__
#define __AML_MBOX_HEALTH_H__

#include <linux/mailbox_controller.h>

#define DSPA_ID		(0)
#define DSPB_ID		(1)

void mbox_set_dsp_hang_status(bool hang, u32 dsp_id);
bool mbox_check_ip_hang(struct mbox_chan *mbox_chan);

#endif
