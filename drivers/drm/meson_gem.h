/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AM_MESON_GEM_H
#define __AM_MESON_GEM_H
#include <drm/drm_gem.h>
#include <uapi/amlogic/drm/meson_drm.h>
#include <linux/amlogic/meson_uvm_core.h>
#include "meson_drv.h"

struct am_meson_gem_object {
	struct drm_gem_object base;
	unsigned int flags;

	/*for buffer create from ion heap */
	struct ion_buffer *ionbuffer;
	struct dma_buf *dmabuf;
	bool bscatter;

	/* for buffer import form other driver */
	phys_addr_t addr;
	struct sg_table *sg;

	/* for uvm related field */
	bool is_uvm;
	bool is_afbc;
	bool is_secure;
	struct uvm_handle *dma_handle;
	struct uvm_buf_obj ubo;
};

/* GEM MANAGER CREATE*/
int am_meson_gem_create(struct meson_drm  *drmdrv);

void am_meson_gem_cleanup(struct meson_drm  *drmdrv);

int am_meson_gem_mmap(struct file *filp, struct vm_area_struct *vma);

/* GEM DUMB OPERATIONS */
int am_meson_gem_dumb_create(struct drm_file *file_priv, struct drm_device *dev,
			     struct drm_mode_create_dumb *args);

int am_meson_gem_dumb_destroy(struct drm_file *file,
			      struct drm_device *dev, uint32_t handle);

int am_meson_gem_create_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv);

int am_meson_gem_dumb_map_offset(struct drm_file *file_priv,
				 struct drm_device *dev,
				 u32 handle, uint64_t *offset);

/* GEM OBJECT OPERATIONS */
struct am_meson_gem_object *
am_meson_gem_object_create(struct drm_device *dev, unsigned int flags,
			   unsigned long size);

phys_addr_t am_meson_gem_object_get_phyaddr(struct meson_drm *drm,
	struct am_meson_gem_object *meson_gem,
	size_t *len);

/* GEM PRIME OPERATIONS */
struct drm_gem_object *
am_meson_gem_prime_import_sg_table(struct drm_device *dev,
				   struct dma_buf_attachment *attach,
				   struct sg_table *sgt);

struct drm_gem_object *am_meson_drm_gem_prime_import(struct drm_device *dev,
						     struct dma_buf *dmabuf);

#endif /* __AM_MESON_GEM_H */
