/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_COMPOSER_EXT_H
#define VIDEO_COMPOSER_EXT_H

#include <linux/amlogic/media/vfm/vframe.h>

enum mediasync_cb_parameter_type_e {
	MEDIASYNC_CB_DISPLAY_DELAY = 0,
	MEDIASYNC_CB_TYPE_MAX,
};

struct mediasync_cb {
	int (*mediasync)(s32 sync_ins_id, int mediasync_cb_type, void *arg);
	int (*reserved0)(void);
	int (*reserved1)(void);
	int (*reserved2)(void);
};

int mediasync_set_callback(struct mediasync_cb *func);

#endif /* VIDEO_COMPOSER_EXT_H */

