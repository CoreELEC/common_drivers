// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <uapi/linux/dma-heap.h>
#include <linux/pagemap.h>
#include <linux/iosys-map.h>

#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>
#include <linux/amlogic/meson_uvm_allocator.h>
#include <linux/amlogic/meson_uvm_core.h>
static int uvm_debug_level = UVM_ERROR;
module_param(uvm_debug_level, int, 0644);
/* bit1: for skip realloc interface;bit0: for skip map */
static int force_skip_realloc;
module_param(force_skip_realloc, int, 0644);

#define UVM_PRINTK(level, fmt, arg...) \
	do {	\
		if (uvm_debug_level & (level))	\
			pr_info("UVM: " fmt, ## arg); \
	} while (0)

static void uvm_hook_mod_release(struct kref *kref);
static struct uvm_hook_mod *uvm_find_hook_mod(struct uvm_handle *handle,
					      int type);

static void uvm_handle_destroy(struct kref *kref)
{
	struct uvm_handle *handle;

	handle = container_of(kref, struct uvm_handle, ref);
	if (!handle)
		return;

	handle->flags &= ~BIT(UVM_DETACH_FLAG);
	if (handle->ua) {
		if (handle->ua->sgt[0]) {
			sg_free_table(handle->ua->sgt[0]);
			kfree(handle->ua->sgt[0]);
		}
		if (handle->ua->sgt[1]) {
			sg_free_table(handle->ua->sgt[1]);
			kfree(handle->ua->sgt[1]);
		}
	}
	UVM_PRINTK(UVM_INFO, "destroy handle:%px\n", handle);

	kfree(handle->ua);
	kfree(handle);
}

static unsigned int get_sgt_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

static int validate_sgt(struct sg_table *sgt)
{
	struct scatterlist *sg;
	int i;

	if (!sgt || !sgt->sgl || sgt->nents <= 0)
		return -EINVAL;

	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		if (!sg_page(sg))
			return -EINVAL;
		if (sg->length <= 0)
			return -EINVAL;
	}

	return 0;
}

static struct sg_table
	*meson_uvm_map_dma_buf(struct dma_buf_attachment *attachment,
			       enum dma_data_direction direction)
{
	struct dma_buf *dmabuf;
	struct uvm_handle *handle;
	struct uvm_alloc *ua;
	struct sg_table *sgt;
	bool gpu_access = false;
	bool skip_realloc = false;

	if (!attachment || !attachment->dev)
		goto err_out;
	if (strstr(dev_name(attachment->dev), "bifrost") ||
		strstr(dev_name(attachment->dev), "valhall")) {
		gpu_access = true;
	}
	dmabuf = attachment->dmabuf;
	if (!dmabuf)
		goto err_out;
	handle = dmabuf->priv;
	if (!handle)
		goto err_out;
	ua = handle->ua;
	if (!ua || !ua->flags)
		goto err_out;

	UVM_PRINTK(UVM_INFO, "uvm map: %s called. name=%s gpu_access:%d flag:0x%llx\n",
		current->comm, dev_name(attachment->dev), gpu_access, ua->flags);
	UVM_PRINTK(UVM_DBG, "handle:%px dmabuf:%px handle->flags:0x%lx\n",
		handle, dmabuf, handle->flags);
	sgt = ua->sgt[0];
	if ((ua->flags & BIT(UVM_SKIP_REALLOC)) ||
		(ua->flags & BIT(UVM_SECURE_ALLOC)) ||
		(force_skip_realloc & BIT(UVM_DEBUG_SKIP_REALLOC_IN_MAP)))
		skip_realloc = true;

	if (ua->flags & BIT(UVM_DELAY_ALLOC) && gpu_access) {
		if (ua->delay_alloc(dmabuf, ua->obj))
			UVM_PRINTK(UVM_INFO, "gpu delay alloc fail\n");
		else
			sgt = ua->sgt[1];
	}

	if (ua->flags & BIT(UVM_IMM_ALLOC) && gpu_access && !skip_realloc) {
		UVM_PRINTK(UVM_INFO, "begin ua->gpu_realloc. size:%zu scalar:%d\n",
			ua->size, ua->scalar);
		if (ua->gpu_realloc(dmabuf, ua->obj, ua->scalar)) {
			UVM_PRINTK(UVM_INFO, "gpu_realloc fail\n");
			goto err_out;
		} else {
			sgt = ua->sgt[1];
		}
	}

	if (ua->flags & BIT(UVM_FBC_DEC) && ua->sgt[1]) {
		UVM_PRINTK(UVM_DBG, "surfacetexture 1:1 temporary path\n");
		sgt = ua->sgt[1];
	}

	if (!sgt)
		goto err_out;
	if (ua->flags & BIT(UVM_SECURE_ALLOC)) {
		if (validate_sgt(sgt) != 0)
			goto err_out;
		if (dma_map_sgtable(attachment->dev, sgt, direction, DMA_ATTR_SKIP_CPU_SYNC))
			return ERR_PTR(-ENOMEM);
	} else {
		if (ua->flags & BIT(UVM_FAKE_ALLOC)) {
			dma_map_page(attachment->dev, sg_page(sgt->sgl), 0,
			     UVM_FAKE_SIZE, direction);
		} else {
			if (validate_sgt(sgt) != 0)
				goto err_out;
			UVM_PRINTK(UVM_DBG, "uvm1 size[dmabuf:%zu,handle:%zu,sgt:%d]\n",
				dmabuf->size, handle->size, get_sgt_size(sgt));
			if (ua->flags & BIT(UVM_IMM_ALLOC) && !gpu_access &&
			    handle->size != get_sgt_size(sgt)) {
				/* omx2 and v4l2 will check the dma->size with sgt size
				 * we sync the fake size to sgt after map and sync done,
				 * so we need to double check the sgt size before map
				 */
				sg_set_page(sgt->sgl, sg_page(sgt->sgl), handle->size, 0);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
				sgt->sgl->dma_length = handle->size;
#endif
			}
			if (!dma_map_sg(attachment->dev, sgt->sgl, sgt->nents, direction))
				return ERR_PTR(-ENOMEM);
		}
		dma_sync_sg_for_device(attachment->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
	}
	if (ua->flags & BIT(UVM_IMM_ALLOC) && !gpu_access &&
	    dmabuf->size != get_sgt_size(sgt)) {
		/* omx2 and v4l2 will check the dma->size with sgt size
		 * so we sync the fake size to sgt after map and sync done
		 */
		UVM_PRINTK(UVM_DBG, "sync fake size[%zu,%zu] for omx2 and v4l2\n",
			dmabuf->size, handle->size);
		sg_set_page(sgt->sgl, sg_page(sgt->sgl), dmabuf->size, 0);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		sgt->sgl->dma_length = dmabuf->size;
#endif
		UVM_PRINTK(UVM_DBG, "uvm2 get_sgt_size[%d][%d][%d]\n",
			sgt->sgl->length, sg_dma_len(sgt->sgl), get_sgt_size(sgt));
	}
	return sgt;

err_out:
	UVM_PRINTK(UVM_DBG, "uvm map dma_buf: null pointer\n");
	return ERR_PTR(-ENOMEM);
}

static void meson_uvm_unmap_dma_buf(struct dma_buf_attachment *attachment,
				    struct sg_table *sgt,
				    enum dma_data_direction direction)
{
	struct dma_buf *dmabuf;
	struct uvm_handle *handle;
	struct uvm_alloc *ua;
	bool gpu_access = false;

	if (strstr(dev_name(attachment->dev), "bifrost") ||
		strstr(dev_name(attachment->dev), "valhall")) {
		gpu_access = true;
	}

	dmabuf = attachment->dmabuf;
	if (!dmabuf)
		goto err_out;
	handle = dmabuf->priv;
	if (!handle)
		goto err_out;
	ua = handle->ua;
	if (!ua)
		goto err_out;

	UVM_PRINTK(UVM_INFO, "uvm unmap: %s called. handle:%px dmabuf:%px\n",
		current->comm, handle, dmabuf);
	if (sgt != ua->sgt[0] && sgt != ua->sgt[1]) {
		UVM_PRINTK(UVM_ERROR, "uvm unmap skip, sgt have free\n");
		return;
	}
	UVM_PRINTK(UVM_DBG, "size[dmabuf:%zu,handle:%zu,sgt:%d] gpu_access:%d\n",
		dmabuf->size, handle->size, get_sgt_size(sgt), gpu_access);
	if (dmabuf->size != handle->size &&
	    handle->size != get_sgt_size(sgt) &&
	    ua->flags & BIT(UVM_IMM_ALLOC) && !gpu_access) {
		/* omx2 and v4l2 will check the dma->size with sgt size
		 * we sync the fake size to sgt after map,
		 * so we need to check and correct the sgt size before unmap
		 */
		sg_set_page(sgt->sgl, sg_page(sgt->sgl), handle->size, 0);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		sgt->sgl->dma_length = handle->size;
#endif
	}
	if (ua->flags & BIT(UVM_SECURE_ALLOC)) {
		dma_unmap_sgtable(attachment->dev, sgt, direction, DMA_ATTR_SKIP_CPU_SYNC);
	} else {
		if (ua->flags & BIT(UVM_FAKE_ALLOC))
			dma_unmap_page(attachment->dev, sgt->sgl->dma_address,
				       UVM_FAKE_SIZE, direction);
		else
			dma_unmap_sg(attachment->dev, sgt->sgl, sgt->nents, direction);
	}

err_out:
	UVM_PRINTK(UVM_DBG, "uvm unmap dma_buf: null pointer\n");
	return;
}

static void meson_uvm_release(struct dma_buf *dmabuf)
{
	struct uvm_hook_mod *uhmod, *uhtmp;
	struct uvm_handle *handle = dmabuf->priv;
	struct uvm_alloc *ua = handle->ua;

	if (ua->free)
		ua->free(ua->obj);

	list_for_each_entry_safe(uhmod, uhtmp, &handle->mod_attached, list) {
		UVM_PRINTK(UVM_INFO, "uhmod:%px uhmod_ref:%u n_attached_mod:%zu\n",
			uhmod, kref_read(&uhmod->ref), handle->n_mod_attached);
		kref_put(&uhmod->ref, uvm_hook_mod_release);
		handle->n_mod_attached--;
	}

	UVM_PRINTK(UVM_INFO,
		"uvm release: %s called. dmabuf:%px, handle:%px, handle->ref=%u\n",
		current->comm, dmabuf, handle, kref_read(&handle->ref));
	kref_put(&handle->ref, uvm_handle_destroy);
}

static int meson_uvm_begin_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction dir)
{
	return 0;
}

static int meson_uvm_end_cpu_access(struct dma_buf *dmabuf,
				    enum dma_data_direction dir)
{
	return 0;
}

static int meson_uvm_attach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attach)
{
	return 0;
}

static void meson_uvm_detach(struct dma_buf *dmabuf,
			     struct dma_buf_attachment *attach)
{
	/* TODO */
}

static int meson_uvm_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct uvm_handle *handle;
	struct sg_table *sgt;
	struct scatterlist *sg;
	struct page **pages;
	struct page **tmp;
	int npages, i, j;
	pgprot_t pgprot;
	void *vaddr;

	handle = dmabuf->priv;
	if (handle->ua && ((handle->ua->flags & BIT(UVM_IMM_ALLOC)) ||
	    !handle->ua->sgt[1]) &&
	    !(handle->ua->flags & BIT(UVM_FBC_DEC)))
		sgt = handle->ua->sgt[0];
	else
		sgt = handle->ua->sgt[1];
	if (!sgt)
		return -ENOMEM;

	npages = PAGE_ALIGN(handle->size) / PAGE_SIZE;
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return -ENOMEM;

	if (handle->ua->flags & BIT(UVM_IMM_ALLOC) &&
	    handle->size != get_sgt_size(sgt)) {
		/* omx2 and v4l2 will check the dma->size with sgt size
		 * we sync the fake size to sgt after map,
		 * so we need to check and correct the sgt size before vmap
		 */
		sg_set_page(sgt->sgl, sg_page(sgt->sgl), handle->size, 0);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		sgt->sgl->dma_length = handle->size;
#endif
	}

	tmp = pages;
	for_each_sg(sgt->sgl, sg, sgt->nents, i) {
		int npages_this_entry = PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		WARN_ON(i >= npages);
		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	vfree(pages);

	if (!vaddr)
		return -ENOMEM;

	handle->ua->vaddr = vaddr;
	UVM_PRINTK(UVM_INFO, "uvm vmap: dmabuf:%px, handle:%px\n", dmabuf, handle);
	iosys_map_set_vaddr(map, vaddr);

	return 0;
}

static void meson_uvm_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct uvm_handle *handle = dmabuf->priv;

	UVM_PRINTK(UVM_INFO, "uvm vunmap: dmabuf:%px, handle:%px\n", dmabuf, handle);
	vunmap(handle->ua->vaddr);
}

static int meson_uvm_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	int i;
	int ret;
	struct scatterlist *sg;
	struct uvm_handle *handle = dmabuf->priv;
	struct uvm_alloc *ua = handle->ua;
	struct sg_table *table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;

	UVM_PRINTK(UVM_INFO, "uvm mmap: dmabuf:%px handle:%px\n", dmabuf, handle);

	if (handle->ua && ((handle->ua->flags & BIT(UVM_IMM_ALLOC)) ||
	    !handle->ua->sgt[1]) && !(handle->ua->flags & BIT(UVM_FBC_DEC)))
		table = handle->ua->sgt[0];
	else
		table = handle->ua->sgt[1];
	if (!table)
		return -EINVAL;

	if ((vma->vm_flags & VM_WRITE) && (handle->flags & BIT(UVM_READONLY_ALLOC)))
		return -EPERM;

	if (!(handle->flags & BIT(UVM_USAGE_CACHED)))
		vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
	UVM_PRINTK(UVM_DBG, "size[dmabuf:%zu,handle:%zu,sgt:%d],flag:0x%llx\n",
		dmabuf->size, handle->size, get_sgt_size(table), ua->flags);
	if (ua->flags & BIT(UVM_IMM_ALLOC) &&
	    handle->size != get_sgt_size(table)) {
		/* omx2 and v4l2 will check the dma->size with sgt size
		 * we sync the fake size to sgt after map,
		 * so we need to check and correct the sgt size before mmap
		 */
		sg_set_page(table->sgl, sg_page(table->sgl), handle->size, 0);
#ifdef CONFIG_NEED_SG_DMA_LENGTH
		table->sgl->dma_length = handle->size;
#endif
	}
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg->length;

		if (offset >= sg->length) {
			offset -= sg->length;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg->length - offset;
			offset = 0;
		}
		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;

		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static struct dma_buf_ops meson_uvm_dma_ops = {
	.attach = meson_uvm_attach,
	.detach = meson_uvm_detach,
	.map_dma_buf = meson_uvm_map_dma_buf,
	.unmap_dma_buf = meson_uvm_unmap_dma_buf,
	.release = meson_uvm_release,
	.begin_cpu_access = meson_uvm_begin_cpu_access,
	.end_cpu_access = meson_uvm_end_cpu_access,
	.vmap = meson_uvm_vmap,
	.vunmap = meson_uvm_vunmap,
	.mmap = meson_uvm_mmap,
};

static struct uvm_handle *uvm_handle_alloc(size_t len, size_t align,
					   ulong flags)
{
	struct uvm_handle *handle;
	struct uvm_alloc *ua;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return ERR_PTR(-ENOMEM);

	handle->commit_tunnel_id = -1;
	kref_init(&handle->ref);
	mutex_init(&handle->lock);
	handle->size = len;
	handle->align = align;
	handle->flags = flags;
	handle->flags &= ~BIT(UVM_DETACH_FLAG);
	INIT_LIST_HEAD(&handle->mod_attached);

	ua = kzalloc(sizeof(*ua), GFP_KERNEL);
	if (!ua) {
		kfree(handle);
		return ERR_PTR(-ENOMEM);
	}
	handle->ua = ua;

	return handle;
}

static struct dma_buf *uvm_dmabuf_export(struct uvm_handle *handle)
{
	struct dma_buf *dmabuf;
	int flag = O_RDWR;

	if (handle->flags & BIT(UVM_READONLY_ALLOC))
		flag = O_RDONLY;

	struct dma_buf_export_info exp_info = {
		.exp_name = "uvm",
		.owner = THIS_MODULE,
		.ops = &meson_uvm_dma_ops,
		.size = handle->size,
		.flags = flag,
		.priv = handle,
	};

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		UVM_PRINTK(UVM_ERROR, "dma_buf_export fail\n");
		return ERR_PTR(-ENOMEM);
	}

	handle->dmabuf = dmabuf;

	return dmabuf;
}

struct dma_buf *uvm_alloc_dmabuf(size_t len, size_t align, ulong flags)
{
	struct uvm_handle *handle;
	struct dma_buf *dmabuf;

	handle = uvm_handle_alloc(len, align, flags);
	if (IS_ERR_OR_NULL(handle))
		return ERR_PTR(-ENOMEM);

	dmabuf = uvm_dmabuf_export(handle);
	if (IS_ERR_OR_NULL(dmabuf)) {
		kfree(handle);
		return ERR_PTR(-EINVAL);
	}
	UVM_PRINTK(UVM_DBG, "uvm alloc: len=%zu dmabuf:%px handle:%px\n",
		len, dmabuf, handle);

	return dmabuf;
}
EXPORT_SYMBOL(uvm_alloc_dmabuf);

bool dmabuf_is_uvm(struct dma_buf *dmabuf)
{
	return dmabuf->ops == &meson_uvm_dma_ops;
}
EXPORT_SYMBOL(dmabuf_is_uvm);

bool dmabuf_uvm_realloc(struct dma_buf *dmabuf)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return false;

	handle = dmabuf->priv;

	if ((handle->ua->flags & BIT(UVM_SKIP_REALLOC)) ||
		(handle->ua->flags & BIT(UVM_SECURE_ALLOC)) ||
		(force_skip_realloc & BIT(UVM_DEBUG_SKIP_REALLOC_API)))
		return false;
	else
		return true;
}
EXPORT_SYMBOL(dmabuf_uvm_realloc);

struct uvm_buf_obj *dmabuf_get_uvm_buf_obj(struct dma_buf *dmabuf)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return ERR_PTR(-EINVAL);

	handle = dmabuf->priv;

	return handle->ua->obj;
}
EXPORT_SYMBOL(dmabuf_get_uvm_buf_obj);

static struct sg_table *uvm_copy_sgt(struct sg_table *src_table)
{
	struct sg_table *dst_table = NULL;
	struct scatterlist *dst_sgl = NULL;
	struct scatterlist *src_sgl = NULL;
	int i, ret;

	dst_table = kzalloc(sizeof(*dst_table), GFP_KERNEL);
	if (!dst_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(dst_table, src_table->nents, GFP_KERNEL);
	if (ret) {
		kfree(dst_table);
		return ERR_PTR(-ENOMEM);
	}

	dst_sgl = dst_table->sgl;
	src_sgl = src_table->sgl;
	for (i = 0; i < src_table->nents; i++) {
		sg_set_page(dst_sgl, sg_page(src_sgl), src_sgl->length, 0);
		sg_dma_address(dst_sgl) = sg_phys(src_sgl);
		sg_dma_len(dst_sgl) = sg_dma_len(src_sgl);
		dst_sgl = sg_next(dst_sgl);
		src_sgl = sg_next(src_sgl);
	}

	return dst_table;
}

int dmabuf_bind_uvm_alloc(struct dma_buf *dmabuf, struct uvm_alloc_info *info)
{
	struct uvm_handle *handle;
	struct uvm_alloc *ua;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	ua = handle->ua;

	ua->size = info->size;
	ua->flags = info->flags;
	ua->free = info->free;
	ua->scalar = info->scalar;
	ua->delay_alloc = info->delay_alloc;
	ua->gpu_realloc = info->gpu_realloc;

	if (info->obj) {
		ua->obj = info->obj;
		ua->obj->dmabuf = dmabuf;
	}

	if (info->sgt)
		ua->sgt[0] = uvm_copy_sgt(info->sgt);
	handle->size = ua->size;
	UVM_PRINTK(UVM_DBG, "ua:%px dmabuf:%px flags:ua=0x%llx, handle=0x%lx\n",
		ua, dmabuf, ua->flags, handle->flags);

	return 0;
}
EXPORT_SYMBOL(dmabuf_bind_uvm_alloc);

int dmabuf_bind_uvm_delay_alloc(struct dma_buf *dmabuf,
				struct uvm_alloc_info *info)
{
	struct uvm_handle *handle;
	struct uvm_alloc *ua;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	ua = handle->ua;

	if (info->sgt) {
		if (ua->sgt[1]) {
			sg_free_table(ua->sgt[1]);
			kfree(ua->sgt[1]);
		}
		ua->sgt[1] = uvm_copy_sgt(info->sgt);
	}

	return 0;
}
EXPORT_SYMBOL(dmabuf_bind_uvm_delay_alloc);

static int do_fbc_decoder(struct dma_buf *dmabuf,
				   struct uvm_buf_obj *obj, struct uvm_handle *handle)
{
	int i, j, num_pages;
	struct dma_buf *idmabuf = NULL;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page *page;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;
	struct dma_heap *heap;
	struct dma_buf_attachment *attachment = NULL;
	int ret = 0;

	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return -EINVAL;
	memset(&info, 0, sizeof(info));
	dmabuf->size = mua_calc_real_dmabuf_size(buffer->align,
			buffer->byte_stride, buffer->height);
	UVM_PRINTK(UVM_INFO,
		"do fbc decoder. WxH: %dx%d buffer(%px)->size:%zu realloc dmabuf->size=%zu\n",
		buffer->width, buffer->height, buffer, buffer->size, dmabuf->size);

	heap = dma_heap_find(CODECMM_HEAP_NAME);
	if (!heap)
		return -ENOMEM;

	if (!buffer->idmabuf[1]) {
		idmabuf = dma_heap_buffer_alloc(heap, dmabuf->size,
			O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
		if (IS_ERR(idmabuf))
			return -ENOMEM;
		UVM_PRINTK(UVM_INFO, "idmabuf(%px) alloc success\n", idmabuf);

		attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
		if (!attachment)
			return -ENOMEM;

		src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
		if (!src_sgt)
			goto detach_dma_buf;

		ret = dma_buf_begin_cpu_access(idmabuf, DMA_BIDIRECTIONAL);
		if (ret)
			goto unmap_attachment;

		UVM_PRINTK(UVM_INFO, "src_sgt(%px). nents=%u, length=%u\n",
			src_sgt, src_sgt->nents, src_sgt->sgl->length);
		page = sg_page(src_sgt->sgl);
		buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
		buffer->idmabuf[1] = idmabuf;
		buffer->sg_table = src_sgt;

		info.sgt = src_sgt;
		dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
	} else {
		idmabuf = buffer->idmabuf[1];
		attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
		if (!attachment)
			return -ENOMEM;

		src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
		if (!src_sgt)
			goto detach_dma_buf;

		ret = dma_buf_begin_cpu_access(idmabuf, DMA_BIDIRECTIONAL);
		if (ret)
			goto unmap_attachment;

		UVM_PRINTK(UVM_INFO, "else src_sgt(%px). nents=%u, length=%u\n",
			src_sgt, src_sgt->nents, src_sgt->sgl->length);
		page = sg_page(src_sgt->sgl);
		buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
		buffer->sg_table = src_sgt;

		info.sgt = src_sgt;
		dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
	}

	//start to do vmap
	if (!buffer->sg_table)
		return -ENODEV;

	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(dmabuf->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	if (!tmp)
		return -ENOMEM;
	page_array = tmp;
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		vfree(page_array);
		if (src_sgt && attachment) {
			dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(idmabuf, attachment);
		}
		return -ENOMEM;
	}

	//start to filldata
	meson_uvm_fill_pattern(buffer, dmabuf, vaddr);

	dma_buf_end_cpu_access(idmabuf, DMA_BIDIRECTIONAL);
	vunmap(vaddr);
	vfree(page_array);
	if (src_sgt && attachment) {
		dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(idmabuf, attachment);
	}
	return 0;

unmap_attachment:
	dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
detach_dma_buf:
	dma_buf_detach(idmabuf, attachment);
	return -ENOMEM;
}

int dmabuf_set_vframe(struct dma_buf *dmabuf, struct vframe_s *vf,
		      enum uvm_hook_mod_type type)
{
	struct uvm_handle *handle;
	struct uvm_alloc *ua;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	ua = handle->ua;
	memcpy(&handle->vf, vf, sizeof(struct vframe_s));
	handle->vfp = vf;
	handle->mod_attached_mask |= (1 << type);
	UVM_PRINTK(UVM_DBG, "set vframe, type=%d\n", type);

	if (type == VF_SRC_DECODER &&
		(ua->flags & BIT(UVM_FBC_DEC)))	{
		if (do_fbc_decoder(dmabuf, ua->obj, handle)) {
			UVM_PRINTK(UVM_ERROR, "fbc decoder fail\n");
			return 0;
		}
	}
	return 0;
}
EXPORT_SYMBOL(dmabuf_set_vframe);

struct vframe_s *dmabuf_get_vframe(struct dma_buf *dmabuf)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return ERR_PTR(-EINVAL);

	handle = dmabuf->priv;
	kref_get(&handle->ref);

	return handle->vfp ? &handle->vf : NULL;
}
EXPORT_SYMBOL(dmabuf_get_vframe);

int dmabuf_put_vframe(struct dma_buf *dmabuf)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	return kref_put(&handle->ref, uvm_handle_destroy);
}
EXPORT_SYMBOL(dmabuf_put_vframe);

bool is_valid_mod_type(struct dma_buf *dmabuf,
		       enum uvm_hook_mod_type type)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return 0;

	handle = dmabuf->priv;

	return (handle->mod_attached_mask & (1 << type)) ? 1 : 0;
}
EXPORT_SYMBOL(is_valid_mod_type);

int uvm_attach_hook_mod(struct dma_buf *dmabuf,
			struct uvm_hook_mod_info *info)
{
	struct uvm_handle *handle;
	struct uvm_hook_mod *uhmod;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	uhmod = kzalloc(sizeof(*uhmod), GFP_KERNEL);
	if (!uhmod)
		return -ENOMEM;

	kref_init(&uhmod->ref);
	uhmod->type = info->type;
	uhmod->arg = info->arg;
	uhmod->getinfo = info->getinfo;
	uhmod->setinfo = info->setinfo;
	uhmod->free = info->free;
	uhmod->acquire_fence = info->acquire_fence;

	mutex_lock(&handle->lock);
	UVM_PRINTK(UVM_DBG, "attach hook_mod: type:%d uhmod:%px uhmod->free:%px dmabuf=%px\n",
		info->type, uhmod, uhmod->free, dmabuf);
	list_add_tail(&uhmod->list, &handle->mod_attached);
	if (uhmod->type == VF_PROCESS_DI)
		handle->flags &= ~BIT(UVM_DETACH_FLAG);
	handle->n_mod_attached++;
	handle->mod_attached_mask |= 1 << (uhmod->type);
	UVM_PRINTK(UVM_DBG, "attach: dmabuf=%px type:%d n_attached_mod:%zu flags:%lu\n",
		dmabuf, uhmod->type, handle->n_mod_attached, handle->flags);
	mutex_unlock(&handle->lock);

	return 0;
}
EXPORT_SYMBOL(uvm_attach_hook_mod);

int meson_uvm_get_usage(struct dma_buf *dmabuf, size_t *usage)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	mutex_lock(&handle->lock);
	*usage = handle->usage;
	mutex_unlock(&handle->lock);

	UVM_PRINTK(UVM_DBG, "uvm get usage: usage:%zu dmabuf:%px\n", *usage, dmabuf);
	return 0;
}
EXPORT_SYMBOL(meson_uvm_get_usage);

int meson_uvm_getinfo(struct dma_buf *dmabuf,
			int mode_type, char *buf)
{
	struct uvm_handle *handle;
	struct uvm_hook_mod *uhmod = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	mutex_lock(&handle->lock);
	uhmod = uvm_find_hook_mod(handle, mode_type);
	mutex_unlock(&handle->lock);

	if (uhmod) {
		ret = uhmod->getinfo(uhmod->arg, buf);
		if (ret < 0)
			return -EINVAL;
		UVM_PRINTK(UVM_DBG, "uvm getinfo: uhmod:%px, dmabuf:%px, mode_type=%d\n",
			uhmod, dmabuf, mode_type);
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(meson_uvm_getinfo);

static void meson_uvm_core_setinfo(struct uvm_handle *handle,
	char *buf)
{
	int ret = 0;
	int detach_flag = 0;

	UVM_PRINTK(UVM_DBG, "setinfo buf=%s\n", buf);
	ret = sscanf(buf, "detach_flag=%d", &detach_flag);
	if (ret == 1 && detach_flag == 1) {
		mutex_lock(&handle->lock);
		handle->flags |= BIT(UVM_DETACH_FLAG);
		mutex_unlock(&handle->lock);
	}
}

int meson_uvm_set_usage(struct dma_buf *dmabuf, size_t usage)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	mutex_lock(&handle->lock);
	handle->usage = usage;
	mutex_unlock(&handle->lock);

	UVM_PRINTK(UVM_DBG, "uvm get usage: usage:%zu dmabuf:%px\n", handle->usage, dmabuf);
	return 0;
}
EXPORT_SYMBOL(meson_uvm_set_usage);

int meson_uvm_setinfo(struct dma_buf *dmabuf,
			int mode_type, char *buf)
{
	struct uvm_handle *handle;
	struct uvm_hook_mod *uhmod = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	meson_uvm_core_setinfo(handle, buf);
	if (mode_type == PROCESS_HWC)
		return 0;
	mutex_lock(&handle->lock);
	uhmod = uvm_find_hook_mod(handle, mode_type);
	mutex_unlock(&handle->lock);
	if (uhmod) {
		ret = uhmod->setinfo(uhmod->arg, buf);
		if (ret < 0)
			return -EINVAL;
		return 0;
	}

	UVM_PRINTK(UVM_DBG, "uvm setinfo: dmabuf:%px uhmod:%px, mode_type:%d\n",
		dmabuf, uhmod, mode_type);
	return -EINVAL;
}
EXPORT_SYMBOL(meson_uvm_setinfo);

int uvm_detach_hook_mod(struct dma_buf *dmabuf, int type)
{
	return uvm_put_hook_mod(dmabuf, type);
}
EXPORT_SYMBOL(uvm_detach_hook_mod);

static struct uvm_hook_mod *uvm_find_hook_mod(struct uvm_handle *handle,
					      int type)
{
	struct uvm_hook_mod *ret = NULL;
	struct uvm_hook_mod *uhmod = NULL;

	UVM_PRINTK(UVM_DBG, "find hook_mod: type=%d %s called\n", type, current->comm);

	if (!list_empty(&handle->mod_attached)) {
		list_for_each_entry(uhmod, &handle->mod_attached, list) {
			if (uhmod->type == type) {
				ret = uhmod;
				break;
			}
		}
	}

	if (!ret) {
		UVM_PRINTK(UVM_DBG, "find hook_mod fail\n");
		return NULL;
	}
	return ret;
}

struct uvm_hook_mod *uvm_get_hook_mod(struct dma_buf *dmabuf,
				      int type)
{
	struct uvm_handle *handle;
	struct uvm_hook_mod *uhmod = NULL;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return ERR_PTR(-EINVAL);

	handle = dmabuf->priv;

	mutex_lock(&handle->lock);
	uhmod = uvm_find_hook_mod(handle, type);
	if (uhmod) {
		kref_get(&uhmod->ref);
		UVM_PRINTK(UVM_DBG,
			"get hook_mod: uhmod:%px dmabuf:%px ref=%u type=%d. %s called\n",
			uhmod, dmabuf, kref_read(&uhmod->ref), type, current->comm);
	} else {
		UVM_PRINTK(UVM_DBG, "can not find the match uhmod\n");
	}
	mutex_unlock(&handle->lock);

	return uhmod;
}
EXPORT_SYMBOL(uvm_get_hook_mod);

static void uvm_hook_mod_release(struct kref *kref)
{
	struct uvm_hook_mod *uhmod;

	uhmod = container_of(kref, struct uvm_hook_mod, ref);
	if (!uhmod)
		return;

	UVM_PRINTK(UVM_DBG, "release hook_mod: entry=%px, uhmod:%px uhmod->free:%px\n",
		&uhmod->list, uhmod, uhmod->free);
	list_del(&uhmod->list);
	uhmod->free(uhmod->arg);
	kfree(uhmod);
}

int uvm_put_hook_mod(struct dma_buf *dmabuf, int type)
{
	struct uvm_handle *handle;
	struct uvm_hook_mod *uhmod = NULL;
	int ret = 0;
	unsigned int ref = 0;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	mutex_lock(&handle->lock);
	UVM_PRINTK(MUA_INFO, "put hook_mod: type=%d. %s called. dmabuf:%px handle->flags:%lu\n",
		type, current->comm, dmabuf, handle->flags);
	if (!(type & BIT(PROCESS_HWC)) ||
		((type & BIT(PROCESS_HWC)) && (handle->flags & MUA_DETACH))) {
		if (type & BIT(PROCESS_HWC))
			type &= ~BIT(PROCESS_HWC);

		uhmod = uvm_find_hook_mod(handle, type);

		if (uhmod) {
			UVM_PRINTK(UVM_DBG, "before kref_put uhmod:%px, dmabuf:%px ref:%u\n",
				uhmod, dmabuf, kref_read(&uhmod->ref));
			ref = kref_read(&uhmod->ref);
			if (uhmod->type == VF_PROCESS_DI && ref == 1)
				handle->flags &= ~BIT(UVM_DETACH_FLAG);
			ret = kref_put(&uhmod->ref, uvm_hook_mod_release);
			UVM_PRINTK(UVM_DBG, "after kref_put dmabuf:%px ref:%u flags:%lu\n",
				dmabuf, ref, handle->flags);
		} else {
			UVM_PRINTK(UVM_DBG, "can not find the match uhmod\n");
			ret = -EINVAL;
		}
	}

	mutex_unlock(&handle->lock);
	return ret;
}
EXPORT_SYMBOL(uvm_put_hook_mod);

int dmabuf_get_uvm_buf_real_size(struct dma_buf *dmabuf)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;

	return handle->size;
}
EXPORT_SYMBOL(dmabuf_get_uvm_buf_real_size);

void uvm_realloc_dmabuf_put_flag(struct dma_buf *dmabuf)
{
	int i;
	struct uvm_buf_obj *obj = NULL;
	struct mua_buffer *buffer = NULL;
	struct mua_device *mua_dev = NULL;
	struct mua_realloc_buffer_list *entry, *tmp;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return;

	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	buffer = container_of(obj, struct mua_buffer, base);
	mua_dev = meson_uvm_get_mua_dev();

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == buffer->slot_id)
			break;
	}
	if (i == MAX_PIPE_LINE)
		return;

	mutex_lock(&mua_dev->mua_rec_buf_lock[i]);
	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (entry->fake_dmabuf == dmabuf) {
			entry->flag = false;
			entry->timestamp = ktime_get();
			UVM_PRINTK(UVM_INFO, "clear fake_dmabuf(%px) idmabuf(%px) flag(%d)\n",
				dmabuf, entry->dmabuf, entry->flag);
			break;
		}
	}
	mutex_unlock(&mua_dev->mua_rec_buf_lock[i]);
}
EXPORT_SYMBOL(uvm_realloc_dmabuf_put_flag);
