// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_vma_manager.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/amlogic/ion.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/dev_ion.h>
#include "meson_gem.h"
#include <drm/drm_gem_dma_helper.h>
#include <linux/iosys-map.h>

#include "linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h"
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <uapi/linux/dma-heap.h>

#define to_am_meson_gem_obj(x) container_of(x, struct am_meson_gem_object, base)
#define uvm_to_gem_object(x) container_of(container_of((x), struct mua_buffer, base), \
		struct am_meson_gem_object, buffer)

#define MESON_GEM_NAME "meson_gem"

#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
static void zap_dma_buf_range(struct page *dma_page, unsigned long len)
{
	struct page *page;
	void *vaddr;
	int num_pages = len / PAGE_SIZE;

	if (PageHighMem(dma_page)) {
		page = dma_page;

		while (num_pages > 0) {
			vaddr = kmap_atomic(page);
			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);

			/*
			 *Avoid wasting time zeroing memory if the process
			 *has been killed by SIGKILL
			 */
			if (fatal_signal_pending(current))
				return;

			page++;
			num_pages--;
		}
	} else {
		memset(page_address(dma_page), 0, PAGE_ALIGN(len));
	}
}
#endif

static int am_meson_gem_alloc_buff(struct am_meson_gem_object *
				       meson_gem_obj, int flags)
{
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
	int i;
	struct dma_heap *heap = NULL;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *sg_table = NULL;
	struct page *page;
	bool from_heap_codecmm = false;
	char DMAHEAP[][20] = {"heap-fb", "heap-gfx", "heap-codecmm"};
	int dma_heap_num = 2; /* "heap-fb", "heap-gfx" */
	struct meson_drm *priv = NULL;
#endif
#ifdef CONFIG_AMLOGIC_ION_DEV
	size_t len;
	unsigned int id;
	struct ion_buffer *buffer;
	bool alloc_flag = false;
#endif
	u32 bscatter = 0;
	struct dma_buf *dmabuf = NULL;

	if (!meson_gem_obj) {
		DRM_ERROR("meson_gem_obj is NULL\n");
		return -EINVAL;
	}

	DRM_DEBUG_PRIME("pid[%s %d], meson_gem_obj %p, flags 0x%x, size 0x%x\n",
		current->comm, current->pid,
		meson_gem_obj, flags, (u32)meson_gem_obj->base.size);
	/*
	 *check flags to set different ion heap type.
	 *if flags is set to 0, need to use ion dma buffer.
	 */
	if (flags & MESON_USE_PROTECTED) {
#ifdef CONFIG_AMLOGIC_HEAP_SECURE
		DRM_DEBUG_PRIME("alloc protected buffer in heap-secure\n");
		heap = dma_heap_find("heap-secure");
		if (!IS_ERR_OR_NULL(heap)) {
			DRM_DEBUG_PRIME("find heap-secure success.\n");
			dmabuf = dma_heap_buffer_alloc(heap, meson_gem_obj->base.size,
				O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
			if (!IS_ERR_OR_NULL(dmabuf)) {
				DRM_DEBUG_PRIME("heap-secure alloc success.\n");
				meson_gem_obj->is_dma = true;
			}
		}
#endif
	} else if (((flags & (MESON_USE_SCANOUT | MESON_USE_CURSOR)) != 0) || flags == 0) {
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
		if (!meson_gem_obj->base.dev) {
			DRM_ERROR("dev is NULL\n");
			return -EINVAL;
		}
		priv = meson_gem_obj->base.dev->dev_private;
		if (priv && priv->recovery_mode) {
			DRM_INFO("%s: dma heap of recovery mode add heap-codecmm\n", __func__);
			dma_heap_num = 3; /* "heap-fb", "heap-gfx", "heap-codecmm" */
		}

		for (i = 0; i < dma_heap_num; i++) {
			heap = dma_heap_find(DMAHEAP[i]);
			if (!IS_ERR_OR_NULL(heap)) {
				dmabuf = dma_heap_buffer_alloc(heap, meson_gem_obj->base.size,
					O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
				if (!IS_ERR_OR_NULL(dmabuf)) {
					if (i == 2)
						from_heap_codecmm = true;
					meson_gem_obj->is_dma = true;
					break;
				} else {
					DRM_DEBUG_PRIME("%s alloc fail\n", DMAHEAP[i]);
				}
			} else {
				DRM_DEBUG_PRIME("dma_heap_find %s fail.\n", DMAHEAP[i]);
			}
		}
		if (!meson_gem_obj->is_dma)
			DRM_INFO("%s: fail to alloc memory from heap\n", __func__);
		else
			DRM_DEBUG_PRIME("alloc dmabuf(%px) from %s heap.size=0x%x\n",
				dmabuf, DMAHEAP[i], (u32)meson_gem_obj->base.size);
#endif
#ifdef CONFIG_AMLOGIC_ION_DEV
		if (!meson_gem_obj->is_dma) {
			id = meson_ion_fb_heap_id_get();
			if (id) {
				dmabuf = ion_alloc(meson_gem_obj->base.size, (1 << id),
					ION_FLAG_EXTEND_MESON_HEAP);
				if (IS_ERR_OR_NULL(dmabuf))
					DRM_DEBUG_PRIME("ion_fb alloc fail\n");
				else
					alloc_flag = true;
			} else {
				DRM_DEBUG_PRIME("ion_fb_heap_id_get fail.\n");
			}

			if (!alloc_flag) {
				DRM_DEBUG_PRIME("will try ion-dev next.\n");
				id = meson_ion_cma_heap_id_get();
				if (id) {
					dmabuf = ion_alloc(meson_gem_obj->base.size, (1 << id),
						ION_FLAG_EXTEND_MESON_HEAP);
					if (IS_ERR_OR_NULL(dmabuf)) {
						DRM_ERROR("ion-dev alloc fail.\n");
						return -ENOMEM;
					}
				} else {
					DRM_ERROR("ion_cma_heap_id_get fail.\n");
					return -ENOMEM;
				}
			}
			DRM_DEBUG_PRIME("alloc dmabuf(%px) ion_alloc success.size=0x%x\n",
				dmabuf, (u32)meson_gem_obj->base.size);
		}
#endif
	} else if (flags & MESON_USE_VIDEO_PLANE) {
		meson_gem_obj->is_uvm = true;
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
		heap = dma_heap_find("heap-codecmm");
		if (!IS_ERR_OR_NULL(heap)) {
			DRM_DEBUG_PRIME("heap-codecmm_find success.\n");
			dmabuf = dma_heap_buffer_alloc(heap, meson_gem_obj->base.size, O_RDWR,
				DMA_HEAP_VALID_HEAP_FLAGS);
			meson_gem_obj->is_dma = true;
		}
		if (!meson_gem_obj->is_dma)
			DRM_INFO("%s: fail to alloc memory from heap-codecmm\n", __func__);
		else
			DRM_DEBUG_PRIME("alloc dmabuf(%px) in heap-codecmm.size=0x%x\n",
				dmabuf, (u32)meson_gem_obj->base.size);
#endif
#ifdef CONFIG_AMLOGIC_ION_DEV
		if (!meson_gem_obj->is_dma) {
			id = meson_ion_codecmm_heap_id_get();
			if (id) {
				DRM_DEBUG_PRIME("ion_codecmm_heap_id_get success.\n");
				dmabuf = ion_alloc(meson_gem_obj->base.size, (1 << id),
							   ION_FLAG_EXTEND_MESON_HEAP);
			}
		}
		DRM_DEBUG_PRIME("alloc video dmabuf(%px) in ion_codecmm\n", dmabuf);
#endif
	} else if (flags & MESON_USE_VIDEO_AFBC) {
		meson_gem_obj->is_uvm = true;
		meson_gem_obj->is_afbc = true;
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
		heap = dma_heap_find("system");
		if (!IS_ERR_OR_NULL(heap)) {
			DRM_DEBUG_PRIME("system heap find success.\n");
			dmabuf = dma_heap_buffer_alloc(heap, UVM_FAKE_SIZE, O_RDWR, 0);
			meson_gem_obj->is_dma = true;
		}
		DRM_DEBUG_PRIME("alloc video afbc dmabuf(%px) in system.size=0x%x,is_dma=%d\n",
			dmabuf, (u32)meson_gem_obj->base.size, meson_gem_obj->is_dma);
#endif
#ifdef CONFIG_AMLOGIC_ION
		if (!meson_gem_obj->is_dma)
			dmabuf = ion_alloc(UVM_FAKE_SIZE, ION_HEAP_SYSTEM, 0);
#endif
	} else {
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
		heap = dma_heap_find("system");
		if (!IS_ERR_OR_NULL(heap)) {
			DRM_DEBUG_PRIME("system heap find success.\n");
			dmabuf = dma_heap_buffer_alloc(heap, meson_gem_obj->base.size, O_RDWR, 0);
			meson_gem_obj->is_dma = true;
		}
		DRM_DEBUG_PRIME("alloc other dmabuf(%px) in system.size=0x%x,is_dma=%d\n",
			dmabuf, (u32)meson_gem_obj->base.size, meson_gem_obj->is_dma);
#endif
#ifdef CONFIG_AMLOGIC_ION
		if (!meson_gem_obj->is_dma)
			dmabuf = ion_alloc(meson_gem_obj->base.size,
					   ION_HEAP_SYSTEM, 0);
#endif
		bscatter = 1;
	}

	if (IS_ERR_OR_NULL(dmabuf)) {
		DRM_ERROR("dmabuf IS_ERR_OR_NULL\n");
		return PTR_ERR(dmabuf);
	}
	meson_gem_obj->dmabuf = dmabuf;
	DRM_DEBUG_PRIME("size(0x%x), dmabuf(%px), is_dma(%d)\n",
		(u32)meson_gem_obj->base.size,
		meson_gem_obj->dmabuf, meson_gem_obj->is_dma);

#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
	if (meson_gem_obj->is_dma) {
		attachment = dma_buf_attach(dmabuf, meson_gem_obj->base.dev->dev);
		if (!attachment) {
			DRM_ERROR("dma_buf_attach fail");
			return -ENOMEM;
		}

		sg_table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
		if (!sg_table) {
			DRM_ERROR("Failed to get dma sg");
			dma_buf_detach(dmabuf, attachment);
			return -ENOMEM;
		}
		meson_gem_obj->sg = sg_table;
		meson_gem_obj->attachment = attachment;

		sg_dma_address(sg_table->sgl) = sg_phys(sg_table->sgl);
		page = sg_page(sg_table->sgl);

		if (from_heap_codecmm)
			zap_dma_buf_range(page, meson_gem_obj->base.size);

		dma_sync_sg_for_device(meson_gem_obj->base.dev->dev,
					   sg_table->sgl,
					   sg_table->nents,
					   DMA_TO_DEVICE);
		meson_gem_obj->addr = PFN_PHYS(page_to_pfn(page));
		DRM_DEBUG_PRIME("heap sg_table:nents=%d,dma=0x%x,length=%d,offset=%d\n",
			  sg_table->nents,
			  (u32)sg_table->sgl->dma_address,
			  sg_table->sgl->length,
			  sg_table->sgl->offset);
		DRM_DEBUG_PRIME("heap allocate size (0x%x) addr=0x%x.\n",
			  (u32)meson_gem_obj->base.size, (u32)meson_gem_obj->addr);
	}
#endif
#ifdef CONFIG_AMLOGIC_ION_DEV
	if (!meson_gem_obj->is_dma) {
		if (dmabuf) {
			buffer = (struct ion_buffer *)dmabuf->priv;
		} else {
			DRM_ERROR("dmabuf is NULL");
			return -ENOMEM;
		}
		meson_gem_obj->ionbuffer = buffer;
		sg_dma_address(buffer->sg_table->sgl) = sg_phys(buffer->sg_table->sgl);
		dma_sync_sg_for_device(meson_gem_obj->base.dev->dev,
				       buffer->sg_table->sgl,
				       buffer->sg_table->nents,
				       DMA_TO_DEVICE);
		meson_ion_buffer_to_phys(buffer,
					 (phys_addr_t *)&meson_gem_obj->addr,
					 (size_t *)&len);
		meson_gem_obj->bscatter = bscatter;
		DRM_DEBUG_PRIME("ion sg_table:nents=%d,dma=0x%x,length=%d,offset=%d\n",
			  buffer->sg_table->nents,
			  (u32)buffer->sg_table->sgl->dma_address,
			  buffer->sg_table->sgl->length,
			  buffer->sg_table->sgl->offset);
		DRM_DEBUG_PRIME("ion allocate size (0x%x) addr=0x%x.\n",
			(u32)meson_gem_obj->base.size, (u32)meson_gem_obj->addr);
	}
#endif

	return 0;
}

int am_meson_prime_handle_to_fd(struct drm_device *dev, void *data,
				 struct drm_file *file_priv)
{
	struct drm_prime_handle *args = data;

	if (!dev->driver->prime_handle_to_fd)
		return -EOPNOTSUPP;

	/* check flags are valid */
	if (args->flags & ~(DRM_CLOEXEC | DRM_RDWR | MESON_RDONLY))
		return -EINVAL;

	DRM_INFO("%s flags:%u\n", __func__, args->flags);
	return dev->driver->prime_handle_to_fd(dev, file_priv,
			args->handle, args->flags, &args->fd);
}

static void am_meson_gem_free_buf(struct drm_device *dev,
				      struct am_meson_gem_object *meson_gem_obj)
{
#ifdef CONFIG_AMLOGIC_ION
	if (meson_gem_obj->ionbuffer) {
		DRM_DEBUG_PRIME("free buffer(0x%p),dmabuf(%px)\n",
			meson_gem_obj->ionbuffer, meson_gem_obj->dmabuf);
		meson_gem_obj->ionbuffer = NULL;
		meson_gem_obj->dmabuf = NULL;
	} else if (!meson_gem_obj->ionbuffer && !meson_gem_obj->is_dma)
		DRM_ERROR("meson_gem_obj buffer is null\n");
#endif
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
	if (meson_gem_obj->is_dma) {
		DRM_DEBUG_PRIME("dma_heap_buffer_free (%px).\n",
			meson_gem_obj->dmabuf);
		dma_buf_unmap_attachment(meson_gem_obj->attachment,
					 meson_gem_obj->sg, DMA_BIDIRECTIONAL);
		dma_buf_detach(meson_gem_obj->dmabuf, meson_gem_obj->attachment);
		dma_buf_put(meson_gem_obj->dmabuf);
		meson_gem_obj->dmabuf = NULL;
		DRM_DEBUG_PRIME("meson_gem_obj free dmabuf success\n");
	}
#endif
}

static int am_meson_gem_alloc_video_secure_buff(struct am_meson_gem_object *meson_gem_obj)
{
	unsigned long addr;

	if (!meson_gem_obj) {
		DRM_ERROR("meson_gem_obj is NULL\n");
		return -EINVAL;
	}
	addr = codec_mm_alloc_for_dma(MESON_GEM_NAME,
				      meson_gem_obj->base.size / PAGE_SIZE,
				      0, CODEC_MM_FLAGS_TVP);
	if (!addr) {
		DRM_ERROR("alloc 0x%x secure memory FAILED.\n",
			(unsigned int)meson_gem_obj->base.size);
		return -ENOMEM;
	}
	meson_gem_obj->addr = addr;
	meson_gem_obj->is_secure = true;
	meson_gem_obj->is_uvm = true;
	DRM_DEBUG_PRIME("allocate secure addr(%p) of size(0x%x)\n",
		&meson_gem_obj->addr, (unsigned int)meson_gem_obj->base.size);
	return 0;
}

static void am_meson_gem_free_video_secure_buf(struct am_meson_gem_object *meson_gem_obj)
{
	if (!meson_gem_obj || !meson_gem_obj->addr) {
		DRM_ERROR("meson_gem_obj or addr is null.\n");
		return;
	}
	codec_mm_free_for_dma(MESON_GEM_NAME, meson_gem_obj->addr);
	DRM_DEBUG_PRIME("free secure addr (%p).\n", &meson_gem_obj->addr);
}

void meson_gem_object_free(struct drm_gem_object *obj)
{
	struct am_meson_gem_object *meson_gem_obj = to_am_meson_gem_obj(obj);

	DRM_DEBUG_PRIME("%px handle count = %d, import_attach = %p, flag = 0x%x, is_dma = %d\n",
		meson_gem_obj, obj->handle_count, obj->import_attach,
		meson_gem_obj->flags, meson_gem_obj->is_dma);

	if ((meson_gem_obj->flags & MESON_USE_VIDEO_PLANE) &&
	    (meson_gem_obj->flags & MESON_USE_PROTECTED))
		am_meson_gem_free_video_secure_buf(meson_gem_obj);
	else if (!obj->import_attach)
		am_meson_gem_free_buf(obj->dev, meson_gem_obj);
	else
		drm_prime_gem_destroy(obj, meson_gem_obj->sg);

	drm_gem_free_mmap_offset(obj);

	/* release file pointer to gem object. */
	drm_gem_object_release(obj);

	kfree(meson_gem_obj);
	meson_gem_obj = NULL;
}

static void am_meson_drm_gem_unref_uvm(struct uvm_buf_obj *ubo)
{
	struct am_meson_gem_object *meson_gem_obj;

	meson_gem_obj = uvm_to_gem_object(ubo);

	/* for di post playback, drm need to put the replaced dmabuf from decoder */
	if (meson_gem_obj->buffer.idmabuf[0] &&
		(meson_gem_obj->buffer.ion_flags & BIT(UVM_DMABUF_REPLACE)))
		dma_buf_put(meson_gem_obj->buffer.idmabuf[0]);

	DRM_DEBUG_PRIME("%px dmabuf:%px flag:0x%x\n", meson_gem_obj, meson_gem_obj->dmabuf,
			meson_gem_obj->buffer.ion_flags);
	meson_gem_obj->buffer.idmabuf[0] = NULL;
	meson_gem_obj->buffer.idmabuf[1] = NULL;
	drm_gem_object_put(&meson_gem_obj->base);
}

static struct sg_table *am_meson_gem_create_sg_table(struct drm_gem_object *obj)
{
	struct am_meson_gem_object *meson_gem_obj;
	struct sg_table *dst_table = NULL;
	struct scatterlist *dst_sg = NULL;
	struct sg_table *src_table = NULL;
	struct scatterlist *src_sg = NULL;
	struct page *gem_page = NULL;
	int ret;

	meson_gem_obj = to_am_meson_gem_obj(obj);

	if ((meson_gem_obj->flags & MESON_USE_VIDEO_PLANE) &&
	    (meson_gem_obj->flags & MESON_USE_PROTECTED)) {
		gem_page = phys_to_page(meson_gem_obj->addr);
		dst_table = kmalloc(sizeof(*dst_table), GFP_KERNEL);
		if (!dst_table) {
			ret = -ENOMEM;
			return ERR_PTR(ret);
		}
		ret = sg_alloc_table(dst_table, 1, GFP_KERNEL);
		if (ret) {
			kfree(dst_table);
			return ERR_PTR(ret);
		}
		dst_sg = dst_table->sgl;
		sg_set_page(dst_sg, gem_page, obj->size, 0);
		sg_dma_address(dst_sg) = meson_gem_obj->addr;
		sg_dma_len(dst_sg) = obj->size;

		return dst_table;
	} else if (meson_gem_obj->is_afbc) {
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
		if (meson_gem_obj->is_dma) {
			src_table = meson_gem_obj->sg;
			dst_table = kmalloc(sizeof(*dst_table), GFP_KERNEL);
			if (!dst_table) {
				ret = -ENOMEM;
				return ERR_PTR(ret);
			}

			ret = sg_alloc_table(dst_table, 1, GFP_KERNEL);
			if (ret) {
				kfree(dst_table);
				return ERR_PTR(ret);
			}

			dst_sg = dst_table->sgl;
			src_sg = src_table->sgl;

			sg_set_page(dst_sg, sg_page(src_sg), obj->size, 0);
			sg_dma_address(dst_sg) = sg_phys(src_sg);
			sg_dma_len(dst_sg) = obj->size;

			return dst_table;
		}
#endif
#ifdef CONFIG_AMLOGIC_ION
		if (meson_gem_obj->ionbuffer) {
			src_table = meson_gem_obj->ionbuffer->sg_table;
			dst_table = kmalloc(sizeof(*dst_table), GFP_KERNEL);
			if (!dst_table) {
				ret = -ENOMEM;
				return ERR_PTR(ret);
			}

			ret = sg_alloc_table(dst_table, 1, GFP_KERNEL);
			if (ret) {
				kfree(dst_table);
				return ERR_PTR(ret);
			}

			dst_sg = dst_table->sgl;
			src_sg = src_table->sgl;

			sg_set_page(dst_sg, sg_page(src_sg), obj->size, 0);
			sg_dma_address(dst_sg) = sg_phys(src_sg);
			sg_dma_len(dst_sg) = obj->size;

			return dst_table;
		}
#endif
	}
	DRM_ERROR("Not support import buffer from other driver.\n");
	return NULL;
}

static void am_meson_gem_destroy_sg_table(struct sg_table *sgt)
{
	if (!sgt) {
		DRM_ERROR("sgt is NULL!\n");
		return;
	}

	sg_free_table(sgt);
	kfree(sgt);
}

static struct dma_buf *meson_gem_prime_export(struct drm_gem_object *obj,
					      int flags)
{
	struct dma_buf *dmabuf;
	struct am_meson_gem_object *meson_gem_obj;
	struct uvm_alloc_info info;
	int alloc_flag = flags;

	meson_gem_obj = to_am_meson_gem_obj(obj);
	memset(&info, 0, sizeof(struct uvm_alloc_info));

	if (meson_gem_obj->is_uvm) {
		if (flags == MESON_RDONLY)
			alloc_flag = BIT(UVM_READONLY_ALLOC);

		dmabuf = uvm_alloc_dmabuf(obj->size, 0, alloc_flag);
		if (dmabuf) {
			if (meson_gem_obj->is_afbc || meson_gem_obj->is_secure) {
				info.sgt =
				am_meson_gem_create_sg_table(obj);
			} else {
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
				if (meson_gem_obj->is_dma)
					info.sgt =
					meson_gem_obj->sg;
#endif
#ifdef CONFIG_AMLOGIC_ION
				if (meson_gem_obj->ionbuffer)
					info.sgt =
					meson_gem_obj->ionbuffer->sg_table;
#endif
			}

			if (meson_gem_obj->is_afbc)
				info.flags |= BIT(UVM_FAKE_ALLOC);

			if (meson_gem_obj->is_secure)
				info.flags |= BIT(UVM_SECURE_ALLOC);

			info.flags |= BIT(UVM_SKIP_REALLOC);
			meson_gem_obj->buffer.ion_flags = info.flags;
			meson_gem_obj->buffer.idmabuf[0] = meson_gem_obj->dmabuf;
			meson_gem_obj->buffer.idmabuf[1] = NULL;
			info.obj = &meson_gem_obj->buffer.base;
			info.free = am_meson_drm_gem_unref_uvm;
			dmabuf_bind_uvm_alloc(dmabuf, &info);

			drm_gem_object_get(obj);

			if (meson_gem_obj->is_afbc || meson_gem_obj->is_secure)
				am_meson_gem_destroy_sg_table(info.sgt);
		}

		return dmabuf;
	}

	return drm_gem_prime_export(obj, flags);
}

static struct sg_table *meson_gem_prime_get_sg_table(struct drm_gem_object *obj)
{
	struct am_meson_gem_object *meson_gem_obj;
	struct sg_table *dst_table = NULL;
	struct scatterlist *dst_sg = NULL;
	struct sg_table *src_table = NULL;
	struct scatterlist *src_sg = NULL;
	int ret, i;

	meson_gem_obj = to_am_meson_gem_obj(obj);
	DRM_DEBUG_PRIME("%p.\n", meson_gem_obj);

#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
	if (!meson_gem_obj->base.import_attach && meson_gem_obj->is_dma) {
		src_table = meson_gem_obj->sg;
		dst_table = kmalloc(sizeof(*dst_table), GFP_KERNEL);
		if (!dst_table) {
			ret = -ENOMEM;
			return ERR_PTR(ret);
		}

		ret = sg_alloc_table(dst_table, src_table->nents, GFP_KERNEL);
		if (ret) {
			kfree(dst_table);
			return ERR_PTR(ret);
		}

		dst_sg = dst_table->sgl;
		src_sg = src_table->sgl;
		for (i = 0; i < src_table->nents; i++) {
			sg_set_page(dst_sg, sg_page(src_sg), src_sg->length, 0);
			sg_dma_address(dst_sg) = sg_phys(src_sg);
			sg_dma_len(dst_sg) = sg_dma_len(src_sg);
			dst_sg = sg_next(dst_sg);
			src_sg = sg_next(src_sg);
		}
		return dst_table;
	}
#endif
#ifdef CONFIG_AMLOGIC_ION
	if (!meson_gem_obj->base.import_attach && meson_gem_obj->ionbuffer) {
		src_table = meson_gem_obj->ionbuffer->sg_table;
		dst_table = kmalloc(sizeof(*dst_table), GFP_KERNEL);
		if (!dst_table) {
			ret = -ENOMEM;
			return ERR_PTR(ret);
		}

		ret = sg_alloc_table(dst_table, src_table->nents, GFP_KERNEL);
		if (ret) {
			kfree(dst_table);
			return ERR_PTR(ret);
		}

		dst_sg = dst_table->sgl;
		src_sg = src_table->sgl;
		for (i = 0; i < src_table->nents; i++) {
			sg_set_page(dst_sg, sg_page(src_sg), src_sg->length, 0);
			sg_dma_address(dst_sg) = sg_phys(src_sg);
			sg_dma_len(dst_sg) = sg_dma_len(src_sg);
			dst_sg = sg_next(dst_sg);
			src_sg = sg_next(src_sg);
		}
		return dst_table;
	}
#endif
	DRM_ERROR("Not support import buffer from other driver.\n");
	return NULL;
}

static int meson_gem_prime_vmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	struct am_meson_gem_object *meson_gem_obj = to_am_meson_gem_obj(obj);

	if (meson_gem_obj->is_uvm) {
		DRM_ERROR("UVM cannot call vmap.\n");
		return 0;
	}
	DRM_DEBUG_PRIME("%p.\n", obj);

	return 0;
}

static void meson_gem_prime_vunmap(struct drm_gem_object *obj, struct iosys_map *map)
{
	DRM_DEBUG_PRIME("nothing to do.\n");
}

#ifdef CONFIG_AMLOGIC_ION
int am_meson_gem_object_mmap(struct am_meson_gem_object *obj,
			     struct vm_area_struct *vma)
{
	int ret = 0;
	struct ion_buffer *buffer = obj->ionbuffer;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vm_flags_clear(vma, VM_PFNMAP);
	vma->vm_pgoff = 0;

	if (obj->base.import_attach) {
		DRM_ERROR("Not support import buffer from other driver.\n");
	} else {
		if (!(buffer->flags & ION_FLAG_CACHED))
			vma->vm_page_prot =
				pgprot_writecombine(vma->vm_page_prot);

		/* now map it to userspace */
		if (obj->dmabuf->ops->mmap)
			ret = obj->dmabuf->ops->mmap(obj->dmabuf, vma);
	}

	if (ret) {
		DRM_ERROR("failure mapping buffer to userspace (%d)\n", ret);
		drm_gem_vm_close(vma);
	}

	return ret;
}
#endif

#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
static int am_meson_gem_object_mmap_dma(struct am_meson_gem_object *meson_gem_obj,
	struct vm_area_struct *vma)
{
	struct sg_table *sg_table = meson_gem_obj->sg;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	/*
	 * Clear the VM_PFNMAP flag that was set by drm_gem_mmap(), and set the
	 * vm_pgoff (used as a fake buffer offset by DRM) to 0 as we want to map
	 * the whole buffer.
	 */
	vm_flags_clear(vma, VM_PFNMAP);
	vma->vm_pgoff = 0;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sgtable_page(sg_table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}
#endif

int meson_gem_prime_mmap(struct drm_gem_object *obj,
			    struct vm_area_struct *vma)
{
	struct am_meson_gem_object *meson_gem_obj;

	meson_gem_obj = to_am_meson_gem_obj(obj);
	DRM_DEBUG_PRIME("%p.\n", meson_gem_obj);
#if (defined CONFIG_AMLOGIC_HEAP_CMA) || (defined CONFIG_AMLOGIC_HEAP_CODEC_MM)
	if (meson_gem_obj->is_dma)
		return am_meson_gem_object_mmap_dma(meson_gem_obj, vma);
#endif
#ifdef CONFIG_AMLOGIC_ION
	if (meson_gem_obj->ionbuffer)
		return am_meson_gem_object_mmap(meson_gem_obj, vma);
#endif
	DRM_ERROR("fail.\n");
	return -ENOMEM;
}

static const struct drm_gem_object_funcs meson_gem_object_funcs = {
	.free = meson_gem_object_free,
	.export = meson_gem_prime_export,
	.get_sg_table = meson_gem_prime_get_sg_table,
	.vmap = meson_gem_prime_vmap,
	.vunmap = meson_gem_prime_vunmap,
	.mmap = meson_gem_prime_mmap,
	.vm_ops = &drm_gem_dma_vm_ops,
};

struct am_meson_gem_object *am_meson_gem_object_create(struct drm_device *dev,
	unsigned int flags,
	unsigned long size)
{
	struct am_meson_gem_object *meson_gem_obj = NULL;
	int ret;

	if (!size) {
		DRM_ERROR("invalid size.\n");
		return ERR_PTR(-EINVAL);
	}

	size = roundup(size, PAGE_SIZE);
	meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
	if (!meson_gem_obj) {
		DRM_ERROR("alloc meson_gem_obj failed\n");
		return ERR_PTR(-ENOMEM);
	}

	meson_gem_obj->base.funcs = &meson_gem_object_funcs;

	ret = drm_gem_object_init(dev, &meson_gem_obj->base, size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object, ret = %d\n", ret);
		goto error;
	}

	if ((flags & MESON_USE_VIDEO_PLANE) && (flags & MESON_USE_PROTECTED))
		ret = am_meson_gem_alloc_video_secure_buff(meson_gem_obj);
	else
		ret = am_meson_gem_alloc_buff(meson_gem_obj, flags);
	if (ret < 0) {
		DRM_ERROR("pid[%s %d] alloc buf failed, flags 0x%x, size 0x%x, ret = %d\n",
			current->comm, current->pid, flags,
			(u32)meson_gem_obj->base.size, ret);
		dump_stack();
		drm_gem_object_release(&meson_gem_obj->base);
		goto error;
	}

	if (meson_gem_obj->is_uvm) {
		meson_gem_obj->buffer.base.arg = meson_gem_obj;
		meson_gem_obj->buffer.base.dev = dev->dev;
		meson_gem_obj->buffer.size = size;
	}
	/*for release check*/
	meson_gem_obj->flags = flags;

	DRM_DEBUG_PRIME("meson_gem_obj %p, flags 0x%x, is_uvm %d\n",
		meson_gem_obj, flags, meson_gem_obj->is_uvm);
	return meson_gem_obj;

error:
	kfree(meson_gem_obj);
	return ERR_PTR(ret);
}

phys_addr_t am_meson_gem_object_get_phyaddr(struct meson_drm *drm,
	struct am_meson_gem_object *meson_gem,
	size_t *len)
{
	*len = meson_gem->base.size;
	return meson_gem->addr;
}
EXPORT_SYMBOL(am_meson_gem_object_get_phyaddr);

int am_meson_gem_dumb_create(struct drm_file *file_priv,
			     struct drm_device *dev,
			     struct drm_mode_create_dumb *args)
{
	int ret = 0;
	struct am_meson_gem_object *meson_gem_obj;
	int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	DRM_DEBUG_PRIME("IN,pid[%s %d],width=%d,height=%d,bpp=%d,min_pitch=%d\n",
		current->comm, current->pid,
		args->width, args->height, args->bpp, min_pitch);

	args->pitch = ALIGN(min_pitch, 64);
	if (args->size < args->pitch * args->height)
		args->size = args->pitch * args->height;

	args->size = round_up(args->size, PAGE_SIZE);
	DRM_DEBUG_PRIME("pitch=%d, size=0x%llx, flags=0x%x\n",
		args->pitch, args->size, args->flags);
	meson_gem_obj = am_meson_gem_object_create(dev,
						   args->flags, args->size);
	if (IS_ERR(meson_gem_obj)) {
		DRM_ERROR("meson_gem_obj IS_ERR\n");
		return PTR_ERR(meson_gem_obj);
	}

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv,
				    &meson_gem_obj->base, &args->handle);
	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_put(&meson_gem_obj->base);
	if (ret) {
		DRM_ERROR("create dumb handle failed %d\n", ret);
		return ret;
	}

	DRM_DEBUG_PRIME("OUT: create dumb %p with gem handle(0x%x)\n",
		  meson_gem_obj, args->handle);
	return 0;
}

int am_meson_gem_dumb_destroy(struct drm_file *file,
			      struct drm_device *dev, uint32_t handle)
{
	DRM_DEBUG_PRIME("destroy dumb with handle (0x%x)\n", handle);
	drm_gem_handle_delete(file, handle);
	return 0;
}

int am_meson_gem_dumb_map_offset(struct drm_file *file_priv,
				 struct drm_device *dev,
				 u32 handle, uint64_t *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	mutex_lock(&dev->struct_mutex);

	/*
	 * get offset of memory allocated for drm framebuffer.
	 * - this callback would be called by user application
	 *	with DRM_IOCTL_MODE_MAP_DUMB command.
	 */
	obj = drm_gem_object_lookup(file_priv, handle);
	if (!obj) {
		DRM_ERROR("failed to lookup gem object.\n");
		ret = -EINVAL;
		goto unlock;
	}

	ret = drm_gem_create_mmap_offset(obj);
	if (ret)
		goto out;

	*offset = drm_vma_node_offset_addr(&obj->vma_node);
	DRM_DEBUG_PRIME("offset = 0x%lx\n", (unsigned long)*offset);

out:
	drm_gem_object_put(obj);
unlock:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

int am_meson_gem_create_ioctl(struct drm_device *dev, void *data,
			      struct drm_file *file_priv)
{
	struct am_meson_gem_object *meson_gem_obj;
	struct drm_meson_gem_create *args = data;
	int ret = 0;

	DRM_DEBUG_PRIME("IN, pid[%s %d], flags = 0x%x, size = 0x%llx\n",
		current->comm, current->pid, args->flags, args->size);
	meson_gem_obj = am_meson_gem_object_create(dev, args->flags,
						args->size);
	if (IS_ERR(meson_gem_obj)) {
		DRM_ERROR("meson_gem_obj error\n");
		return PTR_ERR(meson_gem_obj);
	}

	/*
	 * allocate a id of idr table where the obj is registered
	 * and handle has the id what user can see.
	 */
	ret = drm_gem_handle_create(file_priv,
				    &meson_gem_obj->base, &args->handle);
	/* drop reference from allocate - handle holds it now. */
	drm_gem_object_put(&meson_gem_obj->base);
	if (ret) {
		DRM_ERROR("create dumb handle failed %d\n", ret);
		return ret;
	}

	DRM_DEBUG_PRIME("OUT, create dumb %p with gem handle(0x%x)\n",
		meson_gem_obj, args->handle);
	return 0;
}

int am_meson_gem_create(struct meson_drm  *drmdrv)
{
	/*TODO??*/
	return 0;
}

void am_meson_gem_cleanup(struct meson_drm  *drmdrv)
{
	/*TODO??s*/
}

struct drm_gem_object *
am_meson_gem_prime_import_sg_table(struct drm_device *dev,
				   struct dma_buf_attachment *attach,
				   struct sg_table *sgt)
{
	struct am_meson_gem_object *meson_gem_obj;
	int ret;

	meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
	if (!meson_gem_obj)
		return ERR_PTR(-ENOMEM);

	meson_gem_obj->base.funcs = &meson_gem_object_funcs;

	ret = drm_gem_object_init(dev,
				  &meson_gem_obj->base,
			attach->dmabuf->size);
	if (ret < 0) {
		DRM_ERROR("failed to initialize gem object\n");
		kfree(meson_gem_obj);
		return  ERR_PTR(-ENOMEM);
	}

	DRM_DEBUG_PRIME("%px, sg_table %px, %d %d\n", meson_gem_obj, sgt,
		   meson_gem_obj->base.handle_count, dmabuf_is_uvm(attach->dmabuf));

	meson_gem_obj->sg = sgt;
	meson_gem_obj->addr = sg_dma_address(sgt->sgl);
	meson_gem_obj->is_uvm = dmabuf_is_uvm(attach->dmabuf);
	return &meson_gem_obj->base;
}



struct drm_gem_object *am_meson_drm_gem_prime_import(struct drm_device *dev,
						     struct dma_buf *dmabuf)
{
	if (dmabuf_is_uvm(dmabuf)) {
		struct uvm_handle *handle;
		struct uvm_buf_obj *ubo;
		struct am_meson_gem_object *meson_gem_obj;
		unsigned long size;
		int ret;

		handle = dmabuf->priv;
		DRM_DEBUG_PRIME("info:%px %px %px\n", dmabuf, handle, handle->ua->obj);
		if (handle->ua && handle->ua->sgt[0] && handle->ua->obj) {
			ubo = handle->ua->obj;

			DRM_DEBUG_PRIME("dev: %px %px %px\n", ubo->dev, dev, dev->dev);
			if (ubo->dev == dev->dev) {
				meson_gem_obj = uvm_to_gem_object(ubo);
				drm_gem_object_get(&meson_gem_obj->base);
				DRM_DEBUG_PRIME("same device-%px %d\n", meson_gem_obj,
					     meson_gem_obj->base.handle_count);
				return &meson_gem_obj->base;
			}

			return drm_gem_prime_import(dev, dmabuf);
		}

		meson_gem_obj = kzalloc(sizeof(*meson_gem_obj), GFP_KERNEL);
		if (!meson_gem_obj)
			return ERR_PTR(-ENOMEM);
		DRM_DEBUG_PRIME("skeleton uvm alloc %px\n", meson_gem_obj);
		meson_gem_obj->base.funcs = &meson_gem_object_funcs;
		meson_gem_obj->is_uvm = true;
		meson_gem_obj->buffer.base.dmabuf = dmabuf;

		size = roundup(dmabuf->size, PAGE_SIZE);
		ret = drm_gem_object_init(dev, &meson_gem_obj->base, size);
		if (ret < 0) {
			DRM_ERROR("failed to initialize gem object\n");
			kfree(meson_gem_obj);
			return NULL;
		}

		return &meson_gem_obj->base;
	}

	return drm_gem_prime_import(dev, dmabuf);
}

u8 *am_meson_drm_vmap(ulong addr, u32 size, bool *bflg)
{
	u8 *vaddr = NULL;
	ulong phys = addr;
	u32 offset = phys & ~PAGE_MASK;
	u32 npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = NULL;
	pgprot_t pgprot;
	int i;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);

	if (offset)
		npages++;

	pages = kcalloc(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}

	pgprot = PAGE_KERNEL;

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmap fail, size: %d\n",
		       addr - offset, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);

	DRM_DEBUG_PRIME("map high mem pa(%lx) to va(%p), size: %d\n",
		  addr, vaddr + offset, npages << PAGE_SHIFT);
	*bflg = true;

	return vaddr + offset;
}

