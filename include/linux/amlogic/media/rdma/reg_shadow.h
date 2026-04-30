/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _REG_SHADOW_H_
#define _REG_SHADOW_H_

#include <linux/types.h>

struct meson_reg_shadow;

/* API */
u32 meson_reg_shadow_update_bits(struct meson_reg_shadow *ctx,
					u32 offset, u32 hw_val, u32 mask, u32 val);

u32 meson_reg_shadow_read(struct meson_reg_shadow *ctx, u32 offset);
struct meson_reg_shadow *meson_reg_shadow_get(void);

#endif
