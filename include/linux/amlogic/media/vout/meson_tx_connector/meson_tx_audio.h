/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_AUDIO_H_
#define __MESON_TX_AUDIO_H_
#include <dt-bindings/display/meson-conn-ids.h>

struct meson_tx_audio;

struct meson_tx_audio_ops {
	int (*set_audio_mode)(int id, void *param);
	void (*mute_audio)(int id, unsigned int mute);
	int (*earc_hpd_process)(int id);
};

/* for eARC driver register */
int meson_tx_register_earc_hpd_cb(int id, void (*hpd_callback)(bool hpd, void *hpd_data),
	void *hpd_data);
/* for audio driver to get the ops API */
struct meson_tx_audio_ops *meson_tx_get_audio_ops(int id);

#endif
