/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_AUDIO_H
#define __MESON_TX_AUDIO_H

struct meson_tx_audio {
	struct meson_tx_audio_ops *ops;
	void *inst_data;
	void (*hpd_callback)(bool hpd, void *hpd_data);
	void *hpd_data;
	struct list_head conn_list;
	/* defined in <dt-bindings/display/meson-conn-ids.h> */
	int id;
};

/* for DPTX/HDMITx driver register */
int meson_tx_audio_register(struct meson_tx_audio *tx_aud, void *inst_data, int id);

#endif
