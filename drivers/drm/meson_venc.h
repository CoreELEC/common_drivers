/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_VENC_H
#define __MESON_VENC_H

struct meson_tx_venc;
struct regmap;

enum venc_type {
	VENC_ENCP,
	VENC_ENCL,
};

int meson_venc_mode_set(struct meson_tx_venc *venc, u32 enc_index, u32 enc_type,
	enum venc_bist_type bist_type, void *para);
int meson_venc_bist_mode_set(struct meson_tx_venc *venc, u32 enc_index, u32 enc_type,
	enum venc_bist_type bist_type, void *para);
int meson_venc_mode_check(struct meson_tx_venc *venc, u32 enc_index, void *para);
int meson_venc_mode_disable(struct meson_tx_venc *venc, u32 enc_index, u32 enc_type);

#endif
