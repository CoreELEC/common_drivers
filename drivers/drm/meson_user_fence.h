/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2026 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_USER_FENCE_H
#define __MESON_USER_FENCE_H

#define to_meson_user_fence(x) \
	container_of((x), struct meson_user_fence, base)

struct meson_user_fence {
	struct dma_fence base;
	/* protects dma_fence state and signaling */
	spinlock_t lock;
};

int meson_create_fence_ioctl(struct drm_device *dev,
				    void *data, struct drm_file *file_priv);
int meson_signal_fence_ioctl(struct drm_device *dev,
				    void *data, struct drm_file *file_priv);

#endif
