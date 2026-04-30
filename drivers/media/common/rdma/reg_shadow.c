// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include <linux/amlogic/media/rdma/reg_shadow.h>

#include "rdma.h"

#define VIU_OSD1_PATH_CTRL                         0x1a76
#define VIU_OSD2_PATH_CTRL                         0x1a77
#define VIU_OSD3_PATH_CTRL                         0x1a78

struct meson_reg_entry {
	u32 offset;

	u32 curr;
	u32 next;
	u32 dirty;
	u32 valid;
};

struct meson_reg_shadow {
	/* reg write protect*/
	spinlock_t lock;

	struct meson_reg_entry *regs;
	int num_regs;
};

static struct meson_reg_entry meson_regs[] = {
	{ .offset = VIU_OSD1_PATH_CTRL },
	{ .offset = VIU_OSD2_PATH_CTRL },
	{ .offset = VIU_OSD3_PATH_CTRL },
};

static struct meson_reg_entry *
meson_find_reg(struct meson_reg_shadow *ctx, u32 offset)
{
	int i;

	for (i = 0; i < ctx->num_regs; i++) {
		if (ctx->regs[i].offset == offset)
			return &ctx->regs[i];
	}

	return NULL;
}

/*
 * Update bits in software shadow and return the merged register value.
 *
 * This helper is used for shared registers which may be updated by
 * different modules in the same commit path. Each module may still read
 * the hardware register before calling this helper, but the hardware value
 * is only used to initialize the software shadow once.
 *
 * If this is the first update for the register, @hw_val is used as the
 * base value of the shadow register. If another module has already updated
 * this register, the existing shadow value is kept and @hw_val will not
 * overwrite previous updates.
 *
 * The software shadow keeps the latest merged target register value even
 * before the value is finally written to hardware through RDMA/shadow path.
 * This allows later modules to see updates already requested by previous
 * modules in the same commit sequence.
 *
 * IMPORTANT:
 *
 * This helper only merges register bits. It does NOT understand register
 * semantics or resolve ownership conflicts between modules.
 *
 * If different modules update different bit fields of the same register,
 * their updates can be merged safely.
 *
 * However, if multiple modules update the same bit field, the helper cannot
 * distinguish the update intent. In that case, the later update will
 * overwrite the previous value for the overlapping bits.
 *
 * Callers must guarantee bit ownership or update ordering for overlapping
 * bit fields.
 *
 * Return:
 * The merged shadow value after applying @mask/@val. The caller can write
 * this value to the following RDMA/shadow path.
 */
u32 meson_reg_shadow_update_bits(struct meson_reg_shadow *ctx,
				      u32 offset,
				      u32 hw_val,
				      u32 mask,
				      u32 val)
{
	struct meson_reg_entry *entry;
	unsigned long flags;
	u32 ret = hw_val;

	spin_lock_irqsave(&ctx->lock, flags);

	entry = meson_find_reg(ctx, offset);
	if (!entry)
		goto out;

	/*
	 * Lazy init:
	 *
	 * The first module that updates this register initializes the software
	 * shadow from the current hardware value.
	 *
	 * Later modules must not reinitialize entry->next from their own
	 * meson_drm_read_reg() result, because that may overwrite the bits
	 * already updated by previous modules.
	 */
	if (!entry->valid) {
		entry->curr = hw_val;
		entry->next = hw_val;
		entry->valid = 1;
	}

	/*
	 * Merge this module's bit update into the software shadow.
	 *
	 * Only bits covered by @mask are changed. Other bits keep the previous
	 * shadow value, including updates made by other modules.
	 */
	entry->next &= ~mask;
	entry->next |= val & mask;

	entry->dirty = 1;
	ret = entry->next;

out:
	spin_unlock_irqrestore(&ctx->lock, flags);

	return ret;
}
EXPORT_SYMBOL(meson_reg_shadow_update_bits);

u32 meson_reg_shadow_read(struct meson_reg_shadow *ctx, u32 offset)
{
	struct meson_reg_entry *entry;
	unsigned long flags;
	u32 val = 0;

	if (WARN_ON(!ctx))
		return 0;

	spin_lock_irqsave(&ctx->lock, flags);

	entry = meson_find_reg(ctx, offset);
	if (entry)
		val = entry->next;
	else
		WARN_ON(1);

	spin_unlock_irqrestore(&ctx->lock, flags);

	return val;
}
EXPORT_SYMBOL(meson_reg_shadow_read);

static struct meson_reg_shadow *g_ctx;

struct meson_reg_shadow *meson_reg_shadow_get(void)
{
	return g_ctx;
}
EXPORT_SYMBOL(meson_reg_shadow_get);

int meson_reg_shadow_init(struct rdma_device_data_s *rdma_dev)
{
	struct meson_reg_shadow *ctx;
	int i;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	spin_lock_init(&ctx->lock);

	ctx->num_regs = ARRAY_SIZE(meson_regs);
	ctx->regs = kcalloc(ctx->num_regs, sizeof(*ctx->regs),
				 GFP_KERNEL);
	if (!ctx->regs)
		return -ENOMEM;

	memcpy(ctx->regs, meson_regs, sizeof(meson_regs));

	for (i = 0; i < ctx->num_regs; i++) {
		ctx->regs[i].curr = 0;
		ctx->regs[i].next = 0;
		ctx->regs[i].dirty = 0;
	}

	g_ctx = ctx;

	return 0;
}
