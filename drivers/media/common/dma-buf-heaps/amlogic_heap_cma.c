// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cma.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-map-ops.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>
#include <linux/amlogic/tee.h>

/* t6d secure block_size choose 1M */
#define ALIGN_SIZE 1048576

struct meson_cma_heap {
	struct dma_heap *heap;
	struct cma *cma;
	struct mutex lock;/* protect size account */
	struct device *device;
	u64 num_of_buffers;
	u64 num_of_alloc_bytes;
	u64 num_of_free_bytes;
};

struct meson_cma_heap_buffer {
	struct meson_cma_heap *heap;
	struct list_head attachments;
	struct mutex lock;//protect list operation
	unsigned long len;
	struct sg_table sg_table;
	struct page *cma_pages;
	struct page **pages;
	pgoff_t pagecount;
	int vmap_cnt;
	void *vaddr;
	bool uncached;
};

struct meson_dma_heap_attachment {
	struct device *dev;
	struct sg_table table;
	struct list_head list;
	bool mapped;
	bool uncached;
};

static struct dma_heap_device *dma_heap_dev;

static int meson_cma_heap_attach(struct dma_buf *dmabuf,
			   struct dma_buf_attachment *attachment)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;
	struct meson_dma_heap_attachment *a;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = sg_alloc_table_from_pages(&a->table, buffer->pages,
					buffer->pagecount, 0,
					buffer->pagecount << PAGE_SHIFT,
					GFP_KERNEL);
	if (ret) {
		kfree(a);
		return ret;
	}

	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;
	a->uncached = buffer->uncached;
	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void meson_cma_heap_detach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attachment)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;
	struct meson_dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(&a->table);
	kfree(a);
}

static struct sg_table *meson_cma_heap_map_dma_buf(struct dma_buf_attachment *attachment,
					     enum dma_data_direction direction)
{
	struct meson_dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = &a->table;
	int attr = 0;
	int ret;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;
	ret = dma_map_sgtable(attachment->dev, table, direction, attr);
	if (ret)
		return ERR_PTR(-ENOMEM);
	a->mapped = true;
	return table;
}

static void meson_cma_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	struct meson_dma_heap_attachment *a = attachment->priv;
	int attr = 0;

	if (a->uncached)
		attr = DMA_ATTR_SKIP_CPU_SYNC;
	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attr);
}

static int meson_cma_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					     enum dma_data_direction direction)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	mutex_unlock(&buffer->lock);

	return 0;
}

static int meson_cma_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	mutex_unlock(&buffer->lock);

	return 0;
}

static vm_fault_t meson_cma_heap_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct meson_cma_heap_buffer *buffer = vma->vm_private_data;

	if (vmf->pgoff > buffer->pagecount)
		return VM_FAULT_SIGBUS;

	vmf->page = buffer->pages[vmf->pgoff];
	get_page(vmf->page);

	return 0;
}

static const struct vm_operations_struct dma_heap_vm_ops = {
	.fault = meson_cma_heap_vm_fault,
};

static int meson_cma_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;

	if ((vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) == 0)
		return -EINVAL;

	if (buffer->uncached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &dma_heap_vm_ops;
	vma->vm_private_data = buffer;

	return 0;
}

static void *meson_cma_heap_do_vmap(struct meson_cma_heap_buffer *buffer)
{
	void *vaddr;
	pgprot_t pgprot = PAGE_KERNEL;

	if (buffer->uncached)
		pgprot = pgprot_writecombine(PAGE_KERNEL);

	vaddr = vmap(buffer->pages, buffer->pagecount, VM_MAP, pgprot);
	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static int meson_cma_heap_vmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;
	void *vaddr;
	int ret = 0;

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		iosys_map_set_vaddr(map, buffer->vaddr);
		goto out;
	}

	vaddr = meson_cma_heap_do_vmap(buffer);
	if (IS_ERR(vaddr)) {
		ret = PTR_ERR(vaddr);
		goto out;
	}
	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
	iosys_map_set_vaddr(map, buffer->vaddr);
out:
	mutex_unlock(&buffer->lock);

	return ret;
}

static void meson_cma_heap_vunmap(struct dma_buf *dmabuf, struct iosys_map *map)
{
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
	iosys_map_clear(map);
}

static int meson_cma_heap_zero_buffer(struct meson_cma_heap_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *p;
	void *vaddr;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		p = sg_page_iter_page(&piter);
		vaddr = kmap_atomic(p);
		memset(vaddr, 0, PAGE_SIZE);
		kunmap_atomic(vaddr);
	}

	return ret;
}

static void meson_cma_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct dma_heap_device *hdev = dma_heap_dev;
	struct meson_cma_heap_buffer *buffer = dmabuf->priv;
	struct meson_cma_heap *meson_cma_heap = buffer->heap;
	const char *heap_name = dma_heap_get_name(meson_cma_heap->heap);
	phys_addr_t paddr_secure = PFN_PHYS(page_to_pfn(buffer->cma_pages));
	int ret = 0;

	if (buffer->vmap_cnt > 0) {
		WARN(1, "%s: buffer still mapped in the kernel\n", __func__);
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}

	mutex_lock(&meson_cma_heap->lock);
	meson_cma_heap->num_of_buffers--;
	meson_cma_heap->num_of_alloc_bytes -= buffer->len;
	if (strstr(heap_name, "heap-gfx")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_gfx_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_gfx_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_gfx_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_gfx_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	if (strstr(heap_name, "heap-fb")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_fb_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_fb_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_fb_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_fb_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	if (strstr(heap_name, "heap-secure")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_secure_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_secure_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_secure_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_secure_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	pr_debug("dmaheap: release name %s num %llu alloc %llu free %llu\n",
		heap_name, meson_cma_heap->num_of_buffers,
		meson_cma_heap->num_of_alloc_bytes, meson_cma_heap->num_of_free_bytes);
	mutex_unlock(&meson_cma_heap->lock);

	/* for tee secure protect */
	if (strstr(heap_name, "heap-secure")) {
		ret = tee_sectbl_secmem_set((u32)paddr_secure, (u32)buffer->len, 0);
		if (ret)
			pr_err("%s: remove tee protect mem fail!\n", __func__);
		else
			pr_debug("remove tee protect mem success.\n");
		pr_debug("%s: success remove %s, size=%lu, paddr=%pa\n",
			 __func__, heap_name, buffer->len, &paddr_secure);
	}

	meson_cma_heap_zero_buffer(buffer);
	sg_free_table(&buffer->sg_table);
	/* free page list */
	kvfree(buffer->pages);
	/* release memory */
	cma_release(meson_cma_heap->cma, buffer->cma_pages, buffer->pagecount);
	kfree(buffer);
}

static const struct dma_buf_ops meson_cma_heap_buf_ops = {
	.attach = meson_cma_heap_attach,
	.detach = meson_cma_heap_detach,
	.map_dma_buf = meson_cma_heap_map_dma_buf,
	.unmap_dma_buf = meson_cma_heap_unmap_dma_buf,
	.begin_cpu_access = meson_cma_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = meson_cma_heap_dma_buf_end_cpu_access,
	.mmap = meson_cma_heap_mmap,
	.vmap = meson_cma_heap_vmap,
	.vunmap = meson_cma_heap_vunmap,
	.release = meson_cma_heap_dma_buf_release,
};

static struct dma_buf *meson_cma_heap_allocate(struct dma_heap *heap,
					 unsigned long len,
					 u32 fd_flags,
					 u64 heap_flags)
{
	struct meson_cma_heap *meson_cma_heap = dma_heap_get_drvdata(heap);
	struct meson_cma_heap_buffer *buffer;
	struct dma_heap_device *hdev = dma_heap_dev;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	size_t size;

	if (strstr(dma_heap_get_name(heap), "heap-secure"))
		size = ALIGN(len, ALIGN_SIZE);
	else
		size = PAGE_ALIGN(len);

	pgoff_t pagecount = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);
	struct page *cma_pages;
	struct dma_buf *dmabuf;
	struct sg_table *table;
	int ret = -ENOMEM;
	pgoff_t pg;
	phys_addr_t paddr_secure;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->len = size;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
	if (strstr(dma_heap_get_name(heap), "heap-gfx"))
		cma_pages = cma_alloc(meson_cma_heap->cma, pagecount, align,
				      GFP_KERNEL | __GFP_NOWARN);
	else
		cma_pages = cma_alloc(meson_cma_heap->cma, pagecount, align, GFP_KERNEL);
#else
	if (strstr(dma_heap_get_name(heap), "heap-gfx"))
		cma_pages = cma_alloc(meson_cma_heap->cma, pagecount, align, true);
	else
		cma_pages = cma_alloc(meson_cma_heap->cma, pagecount, align, false);
#endif
	if (!cma_pages)
		goto free_buffer;

	/* Clear the cma pages */
	if (PageHighMem(cma_pages)) {
		unsigned long nr_clear_pages = pagecount;
		struct page *page = cma_pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			/*
			 * Avoid wasting time zeroing memory if the process
			 * has been killed by SIGKILL
			 */
			if (fatal_signal_pending(current))
				goto free_cma;
			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(cma_pages), 0, size);
	}

	buffer->pages = __vmalloc_array_noprof(pagecount, sizeof(*buffer->pages), GFP_KERNEL);
	if (!buffer->pages) {
		ret = -ENOMEM;
		goto free_cma;
	}

	for (pg = 0; pg < pagecount; pg++)
		buffer->pages[pg] = &cma_pages[pg];

	buffer->cma_pages = cma_pages;
	buffer->heap = meson_cma_heap;
	buffer->pagecount = pagecount;
	buffer->uncached = true;

	table = &buffer->sg_table;
	ret = sg_alloc_table_from_pages(table, buffer->pages,
					buffer->pagecount, 0,
					buffer->pagecount << PAGE_SHIFT,
					GFP_KERNEL);
	if (ret) {
		ret = -ENOMEM;
		goto free_cma;
	}
	/* create the dmabuf */
	exp_info.exp_name = dma_heap_get_name(heap);
	exp_info.ops = &meson_cma_heap_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}
	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	if (buffer->uncached) {
		ret = dma_map_sgtable(dma_heap_get_dev(heap), table,
						DMA_BIDIRECTIONAL, 0);
		if (ret) {
			ret = -ENOMEM;
			goto free_pages;
		}
		dma_unmap_sgtable(dma_heap_get_dev(heap), table,
						DMA_BIDIRECTIONAL, 0);
	}

	/* for tee secure protect */
	paddr_secure = PFN_PHYS(page_to_pfn(buffer->cma_pages));
	if (strstr(exp_info.exp_name, "heap-secure")) {
		ret = tee_sectbl_secmem_set((u32)paddr_secure, (u32)buffer->len, 1);
		if (ret)
			pr_err("%s: tee protect mem fail!\n", __func__);
		else
			pr_err("tee protect mem success.\n");
		pr_err("%s: success add %s, size=%lu, paddr=%pa\n",
			 __func__, exp_info.exp_name, buffer->len, &paddr_secure);
	}

	mutex_lock(&meson_cma_heap->lock);
	meson_cma_heap->num_of_buffers++;
	meson_cma_heap->num_of_alloc_bytes += buffer->len;
	if (strstr(exp_info.exp_name, "heap-gfx")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_gfx_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_gfx_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_gfx_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_gfx_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	if (strstr(exp_info.exp_name, "heap-fb")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_fb_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_fb_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_fb_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_fb_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	if (strstr(exp_info.exp_name, "heap-secure")) {
		meson_cma_heap->num_of_free_bytes =
			hdev->total_heap_secure_size - meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_secure_num_of_buffers = meson_cma_heap->num_of_buffers;
		hdev->heap_secure_num_of_alloc_bytes = meson_cma_heap->num_of_alloc_bytes;
		hdev->heap_secure_num_of_free_bytes = meson_cma_heap->num_of_free_bytes;
	}
	pr_debug("dmaheap: allocate name %s num %llu alloc %llu free %llu\n",
		exp_info.exp_name, meson_cma_heap->num_of_buffers,
		meson_cma_heap->num_of_alloc_bytes, meson_cma_heap->num_of_free_bytes);
	mutex_unlock(&meson_cma_heap->lock);

	return dmabuf;

free_pages:
	sg_free_table(table);
free_cma:
	kvfree(buffer->pages);
	cma_release(meson_cma_heap->cma, cma_pages, pagecount);
free_buffer:
	kfree(buffer);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops meson_cma_heap_ops = {
	.allocate = meson_cma_heap_allocate,
};

static int meson_cma_heap_get_info(struct meson_cma_heap_info *info_data)
{
	struct dma_heap_device *hdev = dma_heap_dev;
	int ret = -EINVAL;

	if (strstr(info_data->heap_name, "heap-gfx")) {
		info_data->num_of_buffers = hdev->heap_gfx_num_of_buffers;
		info_data->num_of_alloc_bytes = hdev->heap_gfx_num_of_alloc_bytes;
		info_data->num_of_free_bytes = hdev->heap_gfx_num_of_free_bytes;
		ret = 0;
	} else if (strstr(info_data->heap_name, "heap-fb")) {
		info_data->num_of_buffers = hdev->heap_fb_num_of_buffers;
		info_data->num_of_alloc_bytes = hdev->heap_fb_num_of_alloc_bytes;
		info_data->num_of_free_bytes = hdev->heap_fb_num_of_free_bytes;
		ret = 0;
	} else {
		info_data->num_of_buffers = 0;
		info_data->num_of_alloc_bytes = 0;
		info_data->num_of_free_bytes = 0;
	}

	return ret;
}

static long meson_cma_heap_ioctl(struct file *file,
				unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	union meson_cma_heap_ioctl_arg data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case MESON_CMA_HEAP_IOC_GET_INFO:
		ret = meson_cma_heap_get_info(&data.info_data);
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static const struct file_operations meson_cma_heap_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = meson_cma_heap_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = meson_cma_heap_ioctl,
#endif
};

#if DMAHEAP_CLASS
static ssize_t num_of_buffers_show(struct device *dev,
			struct device_attribute *attr, char * const buf)
{
	struct dma_heap_device *hdev = dma_heap_dev;

	if (strstr(kobject_name(&dev->kobj), "heap-gfx"))
		return sprintf(buf, "%zu\n", hdev->heap_gfx_num_of_buffers);
	else if (strstr(kobject_name(&dev->kobj), "heap-fb"))
		return sprintf(buf, "%zu\n", hdev->heap_fb_num_of_buffers);
	else
		return 0;
}

static ssize_t num_of_alloc_bytes_show(struct device *dev,
			struct device_attribute *attr, char * const buf)
{
	struct dma_heap_device *hdev = dma_heap_dev;

	if (strstr(kobject_name(&dev->kobj), "heap-gfx"))
		return sprintf(buf, "%zu\n", hdev->heap_gfx_num_of_alloc_bytes);
	else if (strstr(kobject_name(&dev->kobj), "heap-fb"))
		return sprintf(buf, "%zu\n", hdev->heap_fb_num_of_alloc_bytes);
	else
		return 0;
}

static ssize_t num_of_free_bytes_show(struct device *dev,
			struct device_attribute *attr, char * const buf)
{
	struct dma_heap_device *hdev = dma_heap_dev;

	if (strstr(kobject_name(&dev->kobj), "heap-gfx"))
		return sprintf(buf, "%zu\n", hdev->heap_gfx_num_of_free_bytes);
	else if (strstr(kobject_name(&dev->kobj), "heap-fb"))
		return sprintf(buf, "%zu\n", hdev->heap_fb_num_of_free_bytes);
	else
		return 0;
}

static DEVICE_ATTR_RO(num_of_buffers);
static DEVICE_ATTR_RO(num_of_alloc_bytes);
static DEVICE_ATTR_RO(num_of_free_bytes);

static struct attribute *heap_gfx_attrs[] = {
	&dev_attr_num_of_buffers.attr,
	&dev_attr_num_of_alloc_bytes.attr,
	&dev_attr_num_of_free_bytes.attr,
	NULL
};

static struct attribute *heap_fb_attrs[] = {
	&dev_attr_num_of_buffers.attr,
	&dev_attr_num_of_alloc_bytes.attr,
	&dev_attr_num_of_free_bytes.attr,
	NULL
};

static const struct attribute_group heap_gfx_attr_group = {
	.attrs = heap_gfx_attrs,
};

static const struct attribute_group heap_fb_attr_group = {
	.attrs = heap_fb_attrs,
};
#endif

static int __add_meson_cma_heap(struct cma *cma, void *data)
{
	struct meson_cma_heap *meson_cma_heap;
	struct dma_heap_export_info exp_info;
	struct dma_heap_device *dev = dma_heap_dev;
	struct dentry *heap_root;
	phys_addr_t paddr_secure = cma_get_base(cma);
	unsigned long secure_size = cma_get_size(cma);
	int ret;

	meson_cma_heap = kzalloc(sizeof(*meson_cma_heap), GFP_KERNEL);
	if (!meson_cma_heap)
		return -ENOMEM;
	meson_cma_heap->cma = cma;

	exp_info.name = cma_get_name(cma);
	exp_info.ops = &meson_cma_heap_ops;
	exp_info.priv = meson_cma_heap;

	meson_cma_heap->heap = dma_heap_add(&exp_info);
	if (IS_ERR(meson_cma_heap->heap)) {
		ret = PTR_ERR(meson_cma_heap->heap);
		kfree(meson_cma_heap);
		return ret;
	}

	if (strstr(exp_info.name, "heap-secure")) {
		ret = tee_sectbl_mem_map(0, 0, 0,
				(u32)paddr_secure, (u32)secure_size, ALIGN_SIZE);
		if (ret)
			pr_err("%s: Initialize cma heap-secure fail!\n", __func__);
		else
			pr_info("Initialize cma heap-secure success.\n");
		pr_info("%s: success add %s, size=%lu, paddr=%pa\n",
			 __func__, exp_info.name, secure_size, &paddr_secure);
	}

	mutex_init(&meson_cma_heap->lock);
	meson_cma_heap->device = dma_heap_get_dev(meson_cma_heap->heap);
	/* Create debug device */
	meson_cma_heap->num_of_buffers = 0;
	meson_cma_heap->num_of_alloc_bytes = 0;
	meson_cma_heap->num_of_free_bytes = 0;

	heap_root = debugfs_create_dir(exp_info.name, dev->debug_root);
	debugfs_create_u64("num_of_buffers",
			   0444, heap_root,
			   &meson_cma_heap->num_of_buffers);
	debugfs_create_u64("num_of_alloc_bytes",
			   0444, heap_root,
			   &meson_cma_heap->num_of_alloc_bytes);
	debugfs_create_u64("num_of_free_bytes",
			   0444, heap_root,
			   &meson_cma_heap->num_of_free_bytes);

#if DMAHEAP_CLASS
	if (strstr(exp_info.name, "heap-gfx")) {
		ret = sysfs_create_group(&meson_cma_heap->device->kobj, &heap_gfx_attr_group);
		if (ret) {
			pr_err("dmaheap: failed to create heap-gfx group.\n");
			sysfs_remove_group(&meson_cma_heap->device->kobj, &heap_gfx_attr_group);
		}
	}
	if (strstr(exp_info.name, "heap-fb")) {
		ret = sysfs_create_group(&meson_cma_heap->device->kobj, &heap_fb_attr_group);
		if (ret) {
			pr_err("dmaheap: failed to create heap-fb group.\n");
			sysfs_remove_group(&meson_cma_heap->device->kobj, &heap_fb_attr_group);
		}
	}
#endif

	dma_coerce_mask_and_coherent(dma_heap_get_dev(meson_cma_heap->heap),
		DMA_BIT_MASK(64));
	mb(); /* make sure we only set allocate after dma_mask is set */

	return 0;
}

static int meson_cma_scan(struct cma *cma, void *data)
{
	struct dma_heap_device *hdev = dma_heap_dev;
	const char *cma_name = cma_get_name(cma);
	unsigned long cma_size = cma_get_size(cma);

	if (strstr(cma_name, "heap-gfx") ||
		strstr(cma_name, "heap-fb") ||
		strstr(cma_name, "heap-secure")) {
		__add_meson_cma_heap(cma, NULL);
		pr_info("dmaheap: find %s size %lu\n", cma_name, cma_size);
	}

	if (strstr(cma_name, "heap-gfx")) {
		hdev->total_heap_gfx_size = cma_size;
		hdev->heap_gfx_num_of_free_bytes = hdev->total_heap_gfx_size;
	}
	if (strstr(cma_name, "heap-fb")) {
		hdev->total_heap_fb_size = cma_size;
		hdev->heap_fb_num_of_free_bytes = hdev->total_heap_fb_size;
	}
	if (strstr(cma_name, "heap-secure")) {
		hdev->total_heap_secure_size = cma_size;
		hdev->heap_secure_num_of_free_bytes = hdev->total_heap_secure_size;
	}

	return 0;
}

int add_meson_cma_heap(void)
{
	struct dma_heap_device *dma_dev;
	int ret;

	dma_dev = kzalloc(sizeof(*dma_dev), GFP_KERNEL);
	if (!dma_dev)
		return -ENOMEM;

	dma_dev->dev.minor = MISC_DYNAMIC_MINOR;
	dma_dev->dev.name = "dmaheap";
	dma_dev->dev.fops = &meson_cma_heap_fops;
	dma_dev->dev.parent = NULL;
	ret = misc_register(&dma_dev->dev);
	if (ret) {
		pr_err("dmaheap: failed to register misc device.\n");
		kfree(dma_dev);
		return ret;
	}

	dma_dev->debug_root = debugfs_create_dir("dma_heap", NULL);
	dma_heap_dev = dma_dev;
	cma_for_each_area(meson_cma_scan, NULL);
	return 0;
}

MODULE_LICENSE("GPL v2");
