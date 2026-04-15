// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2026 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/dma-fence.h>
#include <linux/sync_file.h>

#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_print.h>
#include <uapi/amlogic/drm/meson_drm.h>
#include "meson_user_fence.h"

static const char *meson_user_fence_get_driver_name(struct dma_fence *f)
{
	return "meson";
}

static const char *meson_user_fence_get_timeline_name(struct dma_fence *f)
{
	return "meson_user";
}

static void meson_user_fence_release(struct dma_fence *f)
{
	struct meson_user_fence *uf = to_meson_user_fence(f);

	kfree(uf);
}

static const struct dma_fence_ops meson_user_fence_ops = {
	.get_driver_name = meson_user_fence_get_driver_name,
	.get_timeline_name = meson_user_fence_get_timeline_name,
	.release = meson_user_fence_release,
};

int meson_create_fence_ioctl(struct drm_device *dev,
				    void *data, struct drm_file *file_priv)
{
	struct meson_drm_create_fence *arg = data;
	struct meson_user_fence *user_fence;
	struct sync_file *sync_file;
	int fd, ret = 0;

	if (!arg || !arg->context_id || !arg->seqno)
		return -EINVAL;

	user_fence = kzalloc(sizeof(*user_fence), GFP_KERNEL);
	if (!user_fence)
		return -ENOMEM;

	spin_lock_init(&user_fence->lock);

	dma_fence_init(&user_fence->base,
		       &meson_user_fence_ops,
		       &user_fence->lock,
		       arg->context_id,
		       arg->seqno);

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto err_free_fence;
	}

	sync_file = sync_file_create(&user_fence->base);
	if (!sync_file) {
		ret = -ENOMEM;
		goto err_put_fd;
	}

	fd_install(fd, sync_file->file);
	arg->fence_fd = fd;

	return 0;

err_put_fd:
	put_unused_fd(fd);
err_free_fence:
	kfree(user_fence);
	return ret;
}

int meson_signal_fence_ioctl(struct drm_device *dev,
				    void *data, struct drm_file *file_priv)
{
	struct meson_drm_signal_fence *arg = data;
	struct dma_fence *fence;
	struct sync_file *sync_file;
	struct file *file;
	int ret = 0;

	if (!arg || arg->fence_fd < 0)
		return -EINVAL;

	file = fget(arg->fence_fd);
	if (!file)
		return -EBADF;

	sync_file = file->private_data;
	if (!sync_file || !sync_file->fence) {
		ret = -EINVAL;
		goto out_fput;
	}

	fence = sync_file->fence;
	if (fence->ops != &meson_user_fence_ops) {
		ret = -EINVAL;
		goto out_fput;
	}

	if (!dma_fence_is_signaled(fence))
		ret = dma_fence_signal(fence);

out_fput:
	fput(file);
	return ret;
}
