// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/mmzone.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/seq_file.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/kasan.h>
#include <linux/slab.h>
#include <linux/vmstat.h>
#include <linux/kasan.h>
#include <linux/kmsan.h>
#include <linux/page-flags.h>
#ifdef CONFIG_AMLOGIC_MEMORY_SLAB_LARGE
#include <../../../mm/internal.h>
#endif
#include <linux/kmemleak.h>
#if IS_ENABLED(CONFIG_AMLOGIC_PAGE_TRACE)
#include <linux/amlogic/page_trace.h>
#endif

/* smarthome 32bit not support vendor hook */

#if IS_MODULE(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE)
#define PF_ANY(page, enforce)   PF_POISONED_CHECK(page)

PAGEFLAG(OwnerPriv1, owner_priv_1, PF_ANY) TESTCLEARFLAG(OwnerPriv1, owner_priv_1, PF_ANY)
#endif

static void aml_slub_free_large(struct folio *folio, const void *obj)
{
	unsigned int nr_pages, i;
	struct page *page = folio_page(folio, 0);

	if (page) {
		ClearPageOwnerPriv1(page);
		nr_pages = page->index;
		pr_debug("%s, page:%p, pages:%d, obj:%p\n",
			__func__, page_address(page), nr_pages, obj);

		if (!nr_pages) {
			pr_err("%s index num error\n", __func__);
			dump_stack();
		}

		lruvec_stat_mod_folio(folio, NR_SLAB_UNRECLAIMABLE_B,
			-(nr_pages * PAGE_SIZE));

		for (i = 0; i < nr_pages; i++)  {
			__free_pages(page, 0);
			page++;
		}
	}
}

void adjust_redzone_end(const void *ptr, size_t size, unsigned long *p_end)
{
	if (PageOwnerPriv1(virt_to_page(ptr)))
		*p_end = (unsigned long)ptr + PAGE_ALIGN(size);
}

void *aml_slub_alloc_large(int node, size_t size, gfp_t flags, int order)
{
	struct page *page, *p;

	flags &= ~__GFP_COMP;
	page = alloc_pages_node(node, flags, order);
	if (page) {
		unsigned long used_pages = PAGE_ALIGN(size) / PAGE_SIZE;
		unsigned long total_pages = 1 << order;
		unsigned long saved = 0;
		unsigned long fun = 0;
		int i;

		/* record how many pages in first page*/
		SetPageOwnerPriv1(page); /* special flag */

	#if (IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE) && \
		IS_MODULE(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE)) || \
		(IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE) && \
		 IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE))
		fun = get_page_trace(page);
	#endif

		for (i = 1; i < used_pages; i++) {
			p = page + i;
		#if (IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE) && \
			IS_MODULE(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE)) || \
			(IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE) && \
			 IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE))
			set_page_trace(page, 0, flags, (void *)fun);
		#endif
		}
		page->index = used_pages;
		split_page(page, order);
		p = page + used_pages;
		while (used_pages < total_pages) {
			__free_pages(p, 0);
			used_pages++;
			p++;
			saved++;
		}
		pr_debug("%s, page:%p, all:%5ld, size:%5ld, save:%5ld, f:%ps\n",
			__func__, page_address(page), total_pages * PAGE_SIZE,
			(long)size, saved * PAGE_SIZE, (void *)fun);
		return page;
	}
	return NULL;
}

int aml_free_nonslab_page(struct folio *folio, void *object)
{
	unsigned int order = folio_order(folio);

	/*
	 *if (WARN_ON_ONCE(order == 0))
	 *	pr_warn_once("object pointer: 0x%p\n", object);
	 */

	kmemleak_free(object);
#ifdef CONFIG_AMLOGIC_MEMORY_SLAB_LARGE
	kasan_kfree_large(object);
	kmsan_kfree_large(object);
#endif

	if (unlikely(PageOwnerPriv1(folio_page(folio, 0)))) {
		aml_slub_free_large(folio, object);

		return 1;
	}

	lruvec_stat_mod_folio(folio, NR_SLAB_UNRECLAIMABLE_B,
			      -(PAGE_SIZE << order));

	return 0;
}

#if IS_MODULE(CONFIG_AMLOGIC_MEMORY_SLAB_LARGE)
static void __maybe_unused adjust_redzone_hook(void *data,
		const void *ptr, size_t size, unsigned long *end)
{
	if (PageOwnerPriv1(virt_to_page(ptr)))
		*end = (unsigned long)ptr + PAGE_ALIGN(size);
}

static void __maybe_unused slub_alloc_large_hook(void *data, size_t size, gfp_t flags,
		int node, void **ptr, bool *bypass)
{
	unsigned int order = get_order(size);
	struct folio *folio;
	struct page *page;
	int reset = 0;

	/* vendor hook preempt disabled */
	if (!preemptible()) {
		reset = 1;
		preempt_enable();
	}

	flags |= __GFP_COMP;

	if (size < (PAGE_SIZE * (1 << order))) {
		page = aml_slub_alloc_large(node, size, flags, order);
		if (page) {
			folio = page_folio(page);
			*ptr = folio_address(folio);
			lruvec_stat_mod_folio(folio, NR_SLAB_UNRECLAIMABLE_B,
					PAGE_ALIGN(size));
		} else {
			folio = NULL;
		}
	} else {
		folio = (struct folio *)alloc_pages_node_noprof(node, flags, order);
		if (folio) {
			*ptr = folio_address(folio);
			lruvec_stat_mod_folio(folio, NR_SLAB_UNRECLAIMABLE_B,
					PAGE_SIZE << order);
		}
	}

	/* *ptr = kasan_kmalloc_large(*ptr, size, flags); */
	/* As ptr might get tagged, call kmemleak hook after KASAN. */
	kmemleak_alloc(*ptr, size, 1, flags);
	/* kmsan_kmalloc_large(*ptr, size, flags); */

	if (reset == 1)
		preempt_disable();

	*bypass = true;
}

static void __maybe_unused aml_free_nonslab_page_hook(void *data,
		struct folio *folio, const void *object, bool *bypass)
{
	unsigned int order = folio_order(folio);

	/*
	 *if (WARN_ON_ONCE(order == 0))
	 *	pr_warn_once("object pointer: 0x%p\n", object);
	 */

	kmemleak_free(object);
	/* kasan_kfree_large((void *)object); */
	/* kmsan_kfree_large(object); */

	if (unlikely(PageOwnerPriv1(folio_page(folio, 0)))) {
		aml_slub_free_large(folio, object);
	} else {
		lruvec_stat_mod_folio(folio, NR_SLAB_UNRECLAIMABLE_B,
				-(PAGE_SIZE << order));
		folio_put(folio);
	}

	*bypass = true;
}

static void __maybe_unused folio_test_owner_hook(void *data,
		struct folio *folio, bool *bypass)
{
	if (unlikely(folio_test_owner_priv_1(folio)))
		*bypass = true;
}

static void __maybe_unused ksize_hook(void *data,
		struct folio *folio, size_t *up_size)
{
	unsigned long page_num;

	if (unlikely(PageOwnerPriv1(folio_page(folio, 0)))) {
		page_num = folio_page(folio, 0)->index;
		pr_debug("%s, page:%px, index:%ld, size:%ld\n",
				__func__, folio_address(folio),
				page_num, PAGE_SIZE * page_num);
		*up_size = PAGE_SIZE * page_num;
	}
}

static int __init slab_large_module_init(void)
{
	int ret;

	ret = register_trace_android_vh_poison_kmalloc_large_redzone(adjust_redzone_hook, NULL);
	if (ret)
		return -EINVAL;
	ret = register_trace_android_vh_kmalloc_large_node_bypass(slub_alloc_large_hook, NULL);
	if (ret)
		return -EINVAL;
	ret = register_trace_android_vh_kfree_bypass(aml_free_nonslab_page_hook, NULL);
	if (ret)
		return -EINVAL;
	ret = register_trace_android_vh_check_heap_object_bypass(folio_test_owner_hook, NULL);
	if (ret)
		return -EINVAL;
	ret = register_trace_android_vh_ksize(ksize_hook, NULL);
	if (ret)
		return -EINVAL;

	return 0;
}

static void __exit slab_large_module_exit(void)
{
	int ret;

	ret = unregister_trace_android_vh_poison_kmalloc_large_redzone(adjust_redzone_hook, NULL);
	if (ret)
		return;
	ret = unregister_trace_android_vh_kmalloc_large_node_bypass(slub_alloc_large_hook, NULL);
	if (ret)
		return;
	ret = unregister_trace_android_vh_kfree_bypass(aml_free_nonslab_page_hook, NULL);
	if (ret)
		return;
	ret = unregister_trace_android_vh_check_heap_object_bypass(folio_test_owner_hook, NULL);
	if (ret)
		return;
	ret = unregister_trace_android_vh_ksize(ksize_hook, NULL);
	if (ret)
		return;
}

module_init(slab_large_module_init);
module_exit(slab_large_module_exit);
MODULE_LICENSE("GPL v2");
#endif

