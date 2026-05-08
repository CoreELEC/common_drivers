// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/cma.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/libfdt_env.h>
#include <linux/of_reserved_mem.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/genalloc.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/dma-buf.h>
#include <linux/notifier.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_scatter.h>
#include <linux/amlogic/media/codec_mm/codec_mm_state.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm_prealloc.h>
#include <linux/page-isolation.h>
#include <asm/pgtable.h>
#ifdef CONFIG_ARM64
#include <asm/mte.h>
#endif
#include <linux/swap.h>
#include <linux/swapops.h>
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_CODEC_MM
#include <trace/events/meson_atrace.h>
#include <linux/amlogic/aml_cma.h>

#if IS_ENABLED(CONFIG_AMLOGIC_CPU_INFO)
#include <linux/amlogic/media/registers/cpu_version.h>
#else
unsigned char get_meson_cpu_version(int level)
{
	return 0;
}
#endif

#define CONFIG_CODEC_MM_EXT_POOL

#include "codec_mm_priv.h"
#include "codec_mm_scatter_priv.h"
#include "codec_mm_keeper_priv.h"
#include "codec_mm_track_priv.h"
#include "codec_mm_compact_priv.h"
#include "codec_mm_sys_priv.h"
#include "codec_mm_prealloc_priv.h"
#include <linux/highmem.h>
#include <linux/page-flags.h>
#include <linux/vmalloc.h>

#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
#include <linux/amlogic/tee.h>
#else
#define TEE_MEM_TYPE_STREAM_INPUT	0x4
u32 tee_protect_tvp_mem(phys_addr_t start, size_t size, u32 *handle)
{
	return 0xFFFFFFFF;
}

void tee_unprotect_tvp_mem(u32 handle)
{
}

u32 tee_register_mem(u32 type, phys_addr_t pa, size_t size)
{
	pr_info("no tee config\n");
	return (-1);
}

u32 tee_sectbl_secmem_set(phys_addr_t start, size_t size, u32 secure)
{
	return -1;
}

u32 tee_sectbl_mem_map(phys_addr_t tbl0_sta, size_t tbl0_size, u32 tbl0_blk_size,
	phys_addr_t tbl1_sta, size_t tbl1_size, u32 tbl1_blk_size)
{
	pr_info("no tee sectbl\n");
	return -1;
}
#endif

#include <linux/amlogic/cpu_version.h>

static bool prealloc_more_tvp;
module_param(prealloc_more_tvp, bool, 0644);

static bool secure_mem_ctrl;
static u32 secure_mem_align2n;

#define TVP_POOL_NAME "TVP_POOL"
#define CMA_RES_POOL_NAME "CMA_RES"

#define CONFIG_PATH "media.codec_mm"
#define CONFIG_PREFIX "media"

/*
 * level 0: nice = -17, default value
 * level 1: nice = -10
 * level 2: nice = 0
 */
#define CMA_TASK_NICE_LEVEL_0 0
#define CMA_TASK_NICE_LEVEL_1 1
#define CMA_TASK_NICE_LEVEL_2 2

#define MM_ALIGN_DOWN_2N(addr, alg2n)  ((addr) & (~((1 << (alg2n)) - 1)))

#define RES_IS_MAPED
#define DEFAULT_TVP_SIZE_FOR_4K (236 * SZ_1M)
#define DEFAULT_TVP_SIZE_FOR_NO4K (160 * SZ_1M)
#define DEFAULT_TVP_SEGMENT_MIN_SIZE (8 * SZ_1M)
#define AV1_4K_FG_MEM_SIZE 342 // 3 segments 240M/70M/32M

#define ALLOC_MAX_RETRY 1
#define MAX_LOCAL_ID 0x3FF
#define CODEC_MM_FOR_DMA_ONLY(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA)

#define CODEC_MM_FOR_CPU_ONLY(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_CPU)

#define CODEC_MM_FOR_DMACPU(flags) \
	(((flags) & CODEC_MM_FLAGS_FROM_MASK) == CODEC_MM_FLAGS_DMA_CPU)

#define RESERVE_MM_ALIGNED_2N	16

#define RES_MEM_FLAGS_HAVE_MAPED 0x4

#define MAP_RANGE (SZ_1M)
#ifdef CONFIG_PRINTK_CALLER
#define PREFIX_MAX		48
#else
#define PREFIX_MAX		32
#endif

/* the maximum size of a formatted record (i.e. with prefix added per line) */
#define CONSOLE_LOG_MAX		1024

/* the maximum size allowed to be reserved for a record */
#define LOG_LINE_MAX		(CONSOLE_LOG_MAX - PREFIX_MAX)

static int dump_mem_infos(struct seq_file *m, char *buf);

static int dump_free_mem_infos(void *buf, int size);
static int secure_vdec_res_setup(struct reserved_mem *rmem);
static int codec_mm_res_setup(struct reserved_mem *rmem);
#ifdef CONFIG_CODEC_MM_EXT_POOL
static int codec_mm_res_ext_setup(struct reserved_mem *rmem);
#endif

static inline u32 codec_mm_align_up2n(u32 addr, u32 alg2n)
{
	return ((addr + (1 << alg2n) - 1) & (~((1 << alg2n) - 1)));
}

static inline u64 codec_mm_get_current_us(void)
{
	return div64_u64(local_clock(), 1000);
}

static inline u64 codec_mm_get_current_ms(void)
{
	return div64_u64(local_clock(), 1000000);
}

/* dma_alloc_from_contiguous and dma_release_from_contiguous
 * not EXPORT_SYMBOL in kernel. To be consistent with 5.4,
 * copy them in here when media build as ko.
 */
#if IS_MODULE(CONFIG_AMLOGIC_MEDIA_MODULE)
/**
 * dma_alloc_from_contiguous() - allocate pages from contiguous area
 * @dev:   Pointer to device for which the allocation is performed.
 * @count: Requested number of pages.
 * @align: Requested alignment of pages (in PAGE_SIZE order).
 * @no_warn: Avoid printing message about failed allocation.
 *
 * This function allocates memory buffer for specified device. It uses
 * device specific contiguous memory area if available or the default
 * global one. Requires architecture specific dev_get_cma_area() helper
 * function.
 */
struct page *dma_alloc_from_contiguous(struct device *dev, size_t count,
				       unsigned int align, bool no_warn)
{
	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
	if (no_warn)
		return cma_alloc(dev_get_cma_area(dev), count, align, GFP_KERNEL | __GFP_NOWARN);
	else
		return cma_alloc(dev_get_cma_area(dev), count, align, GFP_KERNEL);
#else
	return cma_alloc(dev_get_cma_area(dev), count, align, no_warn);
#endif
}

/**
 * dma_release_from_contiguous() - release allocated pages
 * @dev:   Pointer to device for which the pages were allocated.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by dma_alloc_from_contiguous().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool dma_release_from_contiguous(struct device *dev, struct page *pages,
				 int count)
{
	return cma_release(dev_get_cma_area(dev), pages, count);
}

int is_vmalloc_or_module_addr(const void *x)
{
	/*
	 * ARM, x86-64 and sparc64 put modules in a special place,
	 * and fall back on vmalloc() if that fails. Others
	 * just put it in the vmalloc space.
	 */
#if defined(CONFIG_MODULES) && defined(MODULES_VADDR)
	unsigned long addr = (unsigned long)kasan_reset_tag(x);

	if (addr >= MODULES_VADDR && addr < MODULES_END)
		return 1;
#endif
	return is_vmalloc_addr(x);
}

struct cma {
	unsigned long   base_pfn;
	unsigned long   count;
	unsigned long   *bitmap;
	unsigned int order_per_bit; /* Order of pages represented by one bit */
	spinlock_t	lock;
};

phys_addr_t cma_get_base(const struct cma *cma)
{
	return PFN_PHYS(cma->base_pfn);
}

unsigned long cma_get_size(const struct cma *cma)
{
	return cma->count << PAGE_SHIFT;
}
#endif

/*
 * debug_mode:
 * 0x1:	disable reserved
 * 0x2:	disable cma
 * 0x4:	disable sys memory
 * 0x8:	disable half memory
 *			only used half memory,for debug.
 *			return nomem,if alloced > total/2;
 * 0x10	dump memory info on failed
 * 0x20	trace memory alloc/free info
 * 0x40	dump info for clear
 * 0x80	scatter watermark info
 * 0x100	dmabuf ref trace
 * 0x200	dump stack when allocation/free happens
 */
u32 debug_mode;
u32 codec_mm_get_debug_mode(void)
{
	return debug_mode;
}

static u32 debug_sc_mode;
u32 codec_mm_get_sc_debug_mode(void)
{
	return debug_sc_mode;
}
EXPORT_SYMBOL(codec_mm_get_sc_debug_mode);

static u32 debug_keep_mode;
u32 codec_mm_get_keep_debug_mode(void)
{
	return debug_keep_mode;
}
EXPORT_SYMBOL(codec_mm_get_keep_debug_mode);

bool codec_mm_get_secure_mem_ctrl(void)
{
	return secure_mem_ctrl;
}

static u32 default_tvp_size;
static u32 default_tvp_4k_size;
static u32 default_cma_res_size;
static u32 default_tvp_pool_segment_size[4];

static u32 default_tvp_pool_size_0;
static u32 tvp_pool_segment_maxsize_0;
static u32 default_tvp_pool_size_1;
static u32 default_tvp_pool_size_2;
static u32 tvp_dynamic_increase_disable;
static u32 tvp_dynamic_alloc_max_size;
static u32 tvp_pool_early_release_switch;
static u64 sec_vdec_addr;
static u64 sec_vdec_size;

#define TVP_POOL_SEGMENT_MAX_USED 4
#define TVP_MAX_SLOT 8
/*
 *tvp_mode == 0 means protect secure memory in secmem ta
 *tvp_mode == 1 means use protect secure memory in codec_mm.
 */
static u32 tvp_mode;

struct extpool_mgt_s {
	struct gen_pool *gen_pool[TVP_MAX_SLOT];
	struct codec_mm_s *mm[TVP_MAX_SLOT];
	int slot_num;
	int alloced_size;
	int total_size;
	/* mutex lock */
	struct mutex pool_lock;
	struct task_struct *tvp_kthread;
};

struct codec_mm_mgt_s {
	struct cma *cma;
	struct device *dev;
	struct list_head mem_list;
	struct gen_pool *res_pool;
	struct extpool_mgt_s tvp_pool;
	struct extpool_mgt_s cma_res_pool;
	struct reserved_mem rmem;
#ifdef CONFIG_CODEC_MM_EXT_POOL
	struct reserved_mem rmem_ext;
	struct gen_pool *res_ext_pool;
	int total_reserved_ext_size;
	int alloced_res_ext_size;
#endif
	int support_res_ext;
	int total_codec_mem_size;
	int total_alloced_size;
	int total_cma_size;
	int total_reserved_size;
	int max_used_mem_size;

	int alloced_res_size;
	int alloced_cma_size;
	int alloced_sys_size;
	int alloced_for_sc_size;
	int alloced_for_sc_cnt;
	int alloced_from_coherent;
	int phys_vmaped_page_cnt;

	int alloc_from_sys_pages_max;
	int res_mem_flags;
	int global_memid;
	/*1:for 1080p,2:for 4k*/
	int tvp_enable;
	/*1:for 1080p,2:for 4k */
	int fastplay_enable;
	int codec_mm_for_linear;
	int codec_mm_scatter_watermark;
	int codec_mm_scatter_free_size;
	/* spin lock for protect scatter_watermark */
	spinlock_t scatter_watermark_lock;
	/* spin lock */
	spinlock_t lock;
	atomic_t tvp_user_count;
	/* for tvp operator used */
	struct mutex tvp_protect_lock;
	struct codec_state_node cs;
	void *trk_h;
	struct work_struct	tvp_alloc_wrk;
	/* alloc time states */
	int tee_set_max_us;
	u64 tee_set_total_us;
	int tee_set_cnt;
	int tee_set_10us_less_cnt;
	int tee_set_10_50us_cnt;
	int tee_set_50_100us_cnt;
	int tee_set_100_1000us_cnt;
	int tee_set_1_10ms_cnt;
	int tee_set_10_100ms_cnt;
	int tee_set_100ms_up_cnt;
	/* free time states */
	int tee_clear_max_us;
	u64 tee_clear_total_us;
	int tee_clear_cnt;
	int tee_clear_10us_less_cnt;
	int tee_clear_10_50us_cnt;
	int tee_clear_50_100us_cnt;
	int tee_clear_100_1000us_cnt;
	int tee_clear_1_10ms_cnt;
	int tee_clear_10_100ms_cnt;
	int tee_clear_100ms_up_cnt;
	u64 global_local_id;
};

#define PHY_OFF() offsetof(struct codec_mm_s, phy_addr)
#define HANDLE_OFF() offsetof(struct codec_mm_s, mem_handle)
#define VADDR_OFF() offsetof(struct codec_mm_s, vbuffer)
#define VAL_OFF_VAL(mem, off) (*(unsigned long *)((unsigned long)(mem) + (off)))

static int codec_mm_extpool_pool_release(struct extpool_mgt_s *tvp_pool);
static int codec_mm_tvp_pool_alloc_by_slot(struct extpool_mgt_s *tvp_pool, int memflags, int flags);
static int codec_mm_tvp_pool_unprotect_and_release(struct extpool_mgt_s *tvp_pool);

static struct codec_mm_mgt_s *get_mem_mgt(void)
{
	static struct codec_mm_mgt_s mgt;
	static int inited;

	if (!inited) {
		memset(&mgt, 0, sizeof(struct codec_mm_mgt_s));
		tvp_dynamic_increase_disable = 0;
		tvp_mode = 1;
		mutex_init(&mgt.tvp_protect_lock);
		inited++;
	}
	return &mgt;
}

int codec_get_mem_list_count(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags = 0;
	int list_count = 0;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			list_count++;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
	return list_count;
}

static void *codec_mm_extpool_alloc(struct extpool_mgt_s *tvp_pool,
				    void **from_pool, int size, int align_2n)
{
	int i = 0;
	void *handle = NULL;
	int align_size = (1 << align_2n);

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (!tvp_pool->gen_pool[i]) {
			mutex_unlock(&tvp_pool->pool_lock);
			return NULL;
		}
		handle = (void *)gen_pool_alloc_algo(tvp_pool->gen_pool[i], size,
			gen_pool_first_fit_align, (void *)&align_size);
		if (handle) {
			*from_pool = tvp_pool->gen_pool[i];
			mutex_unlock(&tvp_pool->pool_lock);
			return handle;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return NULL;
}

static void *codec_mm_extpool_free(struct gen_pool *from_pool, void *handle,
				   int size)
{
	gen_pool_free(from_pool, (unsigned long)handle, size);
	return 0;
}

static int codec_mm_valid_mm_locked(struct codec_mm_s *mmhandle)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int have_found = 0;

	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem == mmhandle) {
				have_found =  1;
				break;
			}
		}
	}
	return have_found;
}

void codec_mm_mgt_lock(unsigned long *flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	spin_lock_irqsave(&mgt->lock, *flags);
}

void codec_mm_mgt_unlock(unsigned long *flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	spin_unlock_irqrestore(&mgt->lock, *flags);
}

struct list_head *codec_mm_get_list_head(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return &mgt->mem_list;
}

static int codec_mm_alloc_tvp_pre_check_in(struct codec_mm_mgt_s *mgt, int need_size)
{
	if (!mgt)
		return 0;

	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (gen_pool_avail(tvp_pool->gen_pool[i]) >= need_size) {
			mutex_unlock(&tvp_pool->pool_lock);
			return 1;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return 0;
}

/*
 *have_space:
 *1:	can alloced from reserved
 *2:	can alloced from cma
 *4:	can alloced from sys.
 *8:	can alloced from tvp.
 */
static int codec_mm_alloc_pre_check_in(struct codec_mm_mgt_s *mgt, int need_size, bool tvp_flag)
{
	int have_space = 0;
	int aligned_size = PAGE_ALIGN(need_size);

	if (!tvp_flag) {
		if (aligned_size <= mgt->total_reserved_size - mgt->alloced_res_size)
			have_space |= 1;
		if (aligned_size <= mgt->cma_res_pool.total_size -
			mgt->cma_res_pool.alloced_size)
			have_space |= 1;

		if (aligned_size <= mgt->total_cma_size - mgt->alloced_cma_size)
			have_space |= 2;

		if (aligned_size / PAGE_SIZE <= mgt->alloc_from_sys_pages_max)
			have_space |= 4;
	} else {
		if (tvp_dynamic_increase_disable) {
			if (aligned_size <= mgt->tvp_pool.total_size -
				mgt->tvp_pool.alloced_size)
				have_space |= 8;
		} else {
			if (codec_mm_alloc_tvp_pre_check_in(mgt, aligned_size)) {
				have_space |= 8;
			} else {
				do {
					if (codec_mm_tvp_pool_alloc_by_slot(&mgt->tvp_pool,
						0, mgt->tvp_enable) == 0) {
						pr_err("no more memory %d", mgt->tvp_enable);
						break;
					}
					if (codec_mm_alloc_tvp_pre_check_in(mgt, aligned_size)) {
						have_space |= 8;
						break;
					}
				} while (mgt->tvp_pool.slot_num <= TVP_POOL_SEGMENT_MAX_USED);
			}
		}
	}

	if (codec_dbg_level(CODEC_DBG_DISABLE_MODE)) {
		have_space = have_space & (~codec_dbg_level(CODEC_DBG_DISABLE_RESV));
		have_space = have_space & (~codec_dbg_level(CODEC_DBG_DISABLE_CMA));
		have_space = have_space & (~codec_dbg_level(CODEC_DBG_DISABLE_SYS));
		if (codec_dbg_level(CODEC_DBG_DISABLE_HALF_MEM)) {
			if (mgt->total_alloced_size >
				mgt->total_codec_mem_size / 2) {
				pr_info("codec mm memory is limited by %d, (bit8 enable)\n",
					debug_mode);
				have_space = 0;
			}
		}
	}

	return have_space;
}

static ulong codec_mm_search_phy_addr(char *vaddr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	ulong phy_addr = 0;
	unsigned long flags;

	spin_lock_irqsave(&mgt->lock, flags);

	list_for_each_entry(mem, &mgt->mem_list, list) {
		/*
		 * If work on the fast play mode that is allocate a lot of
		 * memory from CMA, It will be a reserve memory and add to the
		 * codec_mm list, but this area can't be used for search vaddr
		 * thus the node of codec_mm list must be ignored.
		 */
		if (mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)
			continue;

		if (vaddr - mem->vbuffer >= 0 &&
		    vaddr - mem->vbuffer < mem->buffer_size) {
			if (mem->phy_addr)
				phy_addr = mem->phy_addr +
					(vaddr - mem->vbuffer);
			break;
		}
	}

	spin_unlock_irqrestore(&mgt->lock, flags);

	return phy_addr;
}

static void *codec_mm_search_vaddr(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	void *vaddr = NULL;
	unsigned long flags;

	if (!PageHighMem(phys_to_page(phy_addr)))
		return phys_to_virt(phy_addr);

	spin_lock_irqsave(&mgt->lock, flags);

	list_for_each_entry(mem, &mgt->mem_list, list) {
		/*
		 * If work on the fast play mode that is allocate a lot of
		 * memory from CMA, It will be a reserve memory and add to the
		 * codec_mm list, but this area can't be used for search vaddr
		 * thus the node of codec_mm list must be ignored.
		 */
		if (mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)
			continue;

		if (phy_addr >= mem->phy_addr &&
		    phy_addr < mem->phy_addr + mem->buffer_size) {
			if (mem->vbuffer)
				vaddr = mem->vbuffer +
					(phy_addr - mem->phy_addr);
			break;
		}
	}

	spin_unlock_irqrestore(&mgt->lock, flags);

	return vaddr;
}

void *codec_mm_search_mem_by_phy(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mgt->lock, flags);
	list_for_each_entry(mem, &mgt->mem_list, list) {
		/*
		 * If work on the fast play mode that is allocate a lot of
		 * memory from CMA, It will be a reserve memory and add to the
		 * codec_mm list, but this area can't be used for search vaddr
		 * thus the node of codec_mm list must be ignored.
		 */
		if (mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)
			continue;

		if (phy_addr >= mem->phy_addr &&
			phy_addr < mem->phy_addr + mem->buffer_size) {
			spin_unlock_irqrestore(&mgt->lock, flags);
			return mem;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	return NULL;
}

static u8 *__codec_mm_vmap(ulong addr, u32 size, bool is_cache)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	pgprot_t pgprot = PAGE_KERNEL;
	ulong vm_flags = VM_MAP;

	if (!PageHighMem(phys_to_page(addr)) && is_cache)
		return phys_to_virt(addr);

	if (!is_cache) { /* remap for noncached. */
		pgprot = pgprot_dmacoherent(PAGE_KERNEL);
		vm_flags = VM_DMA_COHERENT;
	}
	offset = offset_in_page(addr);
	page_start = addr - offset;
	npages = DIV_ROUND_UP(size + offset, PAGE_SIZE);

	pages = kmalloc_array(npages, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < npages; i++) {
		phys = page_start + i * PAGE_SIZE;
		pages[i] = pfn_to_page(phys >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, npages, vm_flags, pgprot);
	if (!vaddr) {
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}

	kfree(pages);

	codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO,
		"[VMAP] %s, pa(%lx) to va(%lx), size: %d, cache:%d\n",
		__func__, page_start, (ulong)vaddr,
		npages << PAGE_SHIFT, is_cache);

	return vaddr + offset;
}

u8 *codec_mm_vmap(ulong addr, u32 size)
{
	return __codec_mm_vmap(addr, size, true);
}
EXPORT_SYMBOL(codec_mm_vmap);

u8 *codec_mm_vmap_noncache(ulong addr, u32 size)
{
	return __codec_mm_vmap(addr, size, false);
}
EXPORT_SYMBOL(codec_mm_vmap_noncache);

void codec_mm_memset(ulong phys, u32 val, u32 size)
{
	void *ptr = NULL;
	struct page *page = phys_to_page(phys);
	int i, len;

	/*any data lurking in the kernel direct-mapped region is invalidated.*/
	if (!PageHighMem(page)) {
		ptr = page_address(page);
		memset(ptr, val, size);
		codec_mm_dma_flush(ptr, size, DMA_FROM_DEVICE);
		return;
	}

	/* memset highmem area */
	for (i = 0; i < size; i += MAP_RANGE) {
		len = ((size - i) > MAP_RANGE) ? MAP_RANGE : size - i;
		ptr = codec_mm_vmap(phys + i, len);
		if (!ptr) {
			pr_err("%s,vmap the page failed.\n", __func__);
			return;
		}
		memset(ptr, val, len);
		codec_mm_dma_flush(ptr, len, DMA_TO_DEVICE);
		codec_mm_unmap_phyaddr(ptr);
	}
}
EXPORT_SYMBOL(codec_mm_memset);

void codec_mm_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr))
		vunmap(addr);
}
EXPORT_SYMBOL(codec_mm_unmap_phyaddr);

static void *codec_mm_map_phyaddr(struct codec_mm_s *mem)
{
	void *vaddr = NULL;
	unsigned int phys = mem->phy_addr;
	unsigned int size = mem->buffer_size;

	if (!PageHighMem(phys_to_page(phys)))
		return phys_to_virt(phys);

	vaddr = codec_mm_vmap(phys, size);
	/*vaddr = ioremap_nocache(phy_addr, size);*/

	if (vaddr)
		mem->flags |= CODEC_MM_FLAGS_FOR_PHYS_VMAPED;

	return vaddr;
}

int codec_mm_add_release_callback(struct codec_mm_s *mem, struct codec_mm_cb_s *cb)
{
	unsigned long flags;

	if (!mem || !cb)
		return -EINVAL;

	spin_lock_irqsave(&mem->lock, flags);

	if (cb->func) {
		list_add_tail(&cb->node, &mem->release_cb_list);
		mem->release_cb_cnt += 1;
	} else {
		INIT_LIST_HEAD(&cb->node);
	}
	spin_unlock_irqrestore(&mem->lock, flags);

	return 0;
}
EXPORT_SYMBOL(codec_mm_add_release_callback);

void codec_mm_dev_set_dma_mask(u64 bits)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	u64 cma_base, res_base;

	cma_base = cma_get_base(dev_get_cma_area(mgt->dev));
	res_base = mgt->rmem.base;

	if ((cma_base & 0xFFFFFFFF00000000UL) || (res_base & 0xFFFFFFFF00000000UL))
		dma_coerce_mask_and_coherent(mgt->dev, DMA_BIT_MASK(bits));
}
EXPORT_SYMBOL(codec_mm_dev_set_dma_mask);

int is_reserved_ext_support(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->support_res_ext;
}
EXPORT_SYMBOL(is_reserved_ext_support);

#ifdef CONFIG_CODEC_MM_EXT_POOL
static int codec_mm_alloc_first(struct codec_mm_mgt_s *mgt,
							struct codec_mm_s *mem, int align_2n)
{
	int align_size = (1 << align_2n);

	if (!(mem->flags & CODEC_MM_FLAGS_RESERVED_EXT))
		return 0;

	if (mgt->total_reserved_ext_size > mem->buffer_size) {
		mem->mem_handle = (void *)gen_pool_alloc_algo(mgt->res_ext_pool,
			mem->buffer_size, gen_pool_first_fit_align, (void *)&align_size);
		mem->from_flags =
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED_EXT;
		if (mem->mem_handle) {
			/*default is no map */
			mem->vbuffer = NULL;
			mem->phy_addr = (unsigned long)mem->mem_handle;
			if (debug_mode & 0x80)
				pr_info("alloc flags:%d,align=%d,pages:%d,s:%d\n",
					mem->flags,
					mem->align2n,
					mem->page_count,
					mem->buffer_size);
			return 0;
		} else {
			return -ENOMEM;
		}
	}
	return -ENOMEM;
}
#endif

bool alloc_from_system(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem,
	int align_2n, u32 *alloc_trace_mask)
{
	if (align_2n > PAGE_SHIFT)
		return false;

	*alloc_trace_mask |= 1 << 0;
	mem->mem_handle = (void *)__get_free_pages(GFP_KERNEL | GFP_DMA32,
		get_order(mem->buffer_size));
	mem->from_flags =
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES;
	if (mem->mem_handle) {
		mem->vbuffer = mem->mem_handle;
		mem->phy_addr = virt_to_phys(mem->mem_handle);
		dma_sync_single_for_cpu(mgt->dev, mem->phy_addr,
			PAGE_ALIGN(mem->buffer_size), DMA_FROM_DEVICE);
		return true;
	}

	return false;
}

bool alloc_from_cma(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem,
	int align_2n, u32 *alloc_trace_mask)
{
	if (mem->flags & CODEC_MM_FLAGS_CMA_FIRST)
		*alloc_trace_mask |= 1 << 1;
	else
		*alloc_trace_mask |= 1 << 4;

	codec_mm_compact_lock();
	range_conflict_check_wait(mgt->cma, mem->page_count, align_2n - PAGE_SHIFT);

	mem->mem_handle = dma_alloc_from_contiguous(mgt->dev,
		mem->page_count, align_2n - PAGE_SHIFT, false);
	codec_mm_compact_unlock();
	mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA;
	if (mem->mem_handle) {
		mem->phy_addr = page_to_phys((struct page *)mem->mem_handle);
		dma_sync_single_for_cpu(mgt->dev, mem->phy_addr,
				mem->page_count << PAGE_SHIFT, DMA_FROM_DEVICE);
		mem->vbuffer = (mem->flags & CODEC_MM_FLAGS_CPU) ?
			codec_mm_map_phyaddr(mem) : NULL;

		return true;
	}

	return false;
}

bool alloc_from_res(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem,
	int align_2n, u32 *alloc_trace_mask)
{
	struct extpool_mgt_s *ptr_pool = &mgt->cma_res_pool;

	if (mgt->res_pool) {
		int align_size = (1 << align_2n);

		*alloc_trace_mask |= 1 << 2;
		mem->mem_handle = (void *)gen_pool_alloc_algo(mgt->res_pool, mem->buffer_size,
			gen_pool_first_fit_align, (void *)&align_size);
		mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED;
		if (mem->mem_handle) {
			/*default is no mapped */
			mem->vbuffer = NULL;
			mem->phy_addr = (unsigned long)mem->mem_handle;
			return true;
		}
	}

	if (ptr_pool->total_size > 0 &&
		(ptr_pool->alloced_size + mem->buffer_size) <= ptr_pool->total_size) {
		*alloc_trace_mask |= 1 << 3;
		mem->mem_handle = (void *)codec_mm_extpool_alloc(ptr_pool,
			&mem->from_ext, mem->buffer_size, align_2n);
		mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES;
		if (mem->mem_handle) {
			mem->phy_addr = (unsigned long)mem->mem_handle;
			mem->vbuffer = (mem->flags & CODEC_MM_FLAGS_CPU) ?
				codec_mm_map_phyaddr(mem) : NULL;
			return true;
		}
	}

	return false;
}

static void codec_mm_clear_alloc_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem,
	int align_2n, u32 *alloc_trace_mask)
{
	int try_cma_first = mem->flags & CODEC_MM_FLAGS_CMA_FIRST;
	int max_retry = ALLOC_MAX_RETRY;
	int have_space;
	bool alloc_ret;

	int can_from_res = ((mgt->res_pool ||
		(mgt->cma_res_pool.total_size > 0)) &&	/*have res */
		!(mem->flags & CODEC_MM_FLAGS_CMA)) ||	/*must not CMA */
		((mem->flags & CODEC_MM_FLAGS_RESERVED) ||/*need RESERVED */
		CODEC_MM_FOR_DMA_ONLY(mem->flags) ||	/*NO CPU */
		((mem->flags & CODEC_MM_FLAGS_CPU) &&
			(mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)));
	 /*CPU*/
	int can_from_cma = ((mgt->total_cma_size > 0) &&/*have cma */
		!(mem->flags & CODEC_MM_FLAGS_RESERVED)) ||
		(mem->flags & CODEC_MM_FLAGS_CMA);	/*can from CMA */
	/*not always reserved. */

	int can_from_sys = (mem->flags & CODEC_MM_FLAGS_DMA_CPU) &&
		!(mem->flags & CODEC_MM_FLAGS_TVP) &&
		(mem->page_count <= mgt->alloc_from_sys_pages_max);

#ifdef CONFIG_CODEC_MM_EXT_POOL
	if (mem->flags & CODEC_MM_FLAGS_RESERVED_EXT) {
		codec_mm_alloc_first(mgt, mem, align_2n);
		return;
	}
#endif

	have_space = codec_mm_alloc_pre_check_in(mgt, mem->buffer_size, false);
	if (!have_space) {
		codec_pr_dbg(CODEC_DBG_ERR_INFO, "error, codec mm have no space\n");
		return;
	}

	can_from_res = can_from_res && (have_space & 1);
	can_from_cma = can_from_cma && (have_space & 2);
	can_from_sys = can_from_sys && (have_space & 4);

	if (!can_from_res && !can_from_cma && !can_from_sys) {
		codec_pr_dbg(CODEC_DBG_ERR_INFO,
			"error, codec mm have space:%x\n", have_space);
	}

	do {
		if (can_from_sys) {
			alloc_ret = alloc_from_system(mgt, mem, align_2n, alloc_trace_mask);
			if (alloc_ret)
				break;
		}
		/*cma first. */
		if (try_cma_first && can_from_cma) {
			alloc_ret = alloc_from_cma(mgt, mem, align_2n, alloc_trace_mask);
			if (alloc_ret)
				break;
		}

		if (can_from_res) {
			alloc_ret = alloc_from_res(mgt, mem, align_2n, alloc_trace_mask);
			if (alloc_ret)
				break;
		}

		if (can_from_cma) {
			alloc_ret = alloc_from_cma(mgt, mem, align_2n, alloc_trace_mask);
			if (alloc_ret)
				break;
		}
	} while (--max_retry > 0);

	codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO, "codec mm have space:%x can alloc from: %d,%d,%d\n",
		have_space, can_from_sys, can_from_res, can_from_cma);
}

static void codec_mm_secure_alloc_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem,
	int align_2n, u32 *alloc_trace_mask)
{
	(*alloc_trace_mask) |= 1 << 5;
	if (tvp_dynamic_increase_disable) {
		if (mem->buffer_size > mgt->tvp_pool.total_size - mgt->tvp_pool.alloced_size)
			return;

		mem->mem_handle = (void *)codec_mm_extpool_alloc(&mgt->tvp_pool,
			&mem->from_ext, mem->buffer_size, align_2n);
		mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP;
		if (mem->mem_handle) {
			/*no vaddr for TVP MEMORY */
			mem->vbuffer = NULL;
			mem->phy_addr = (unsigned long)mem->mem_handle;
		}
	} else {
		do {
			mem->mem_handle = (void *)codec_mm_extpool_alloc(&mgt->tvp_pool,
				&mem->from_ext, mem->buffer_size, align_2n);
			if (mem->mem_handle) {
				/*no vaddr for TVP MEMORY */
				mem->from_flags = AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP;
				mem->vbuffer = NULL;
				mem->phy_addr = (unsigned long)mem->mem_handle;
				break;
			}
			if (codec_mm_tvp_pool_alloc_by_slot(&mgt->tvp_pool,
				0, mgt->tvp_enable) == 0) {
				pr_err("no more memory can be alloc %d", mgt->tvp_enable);
				break;
			}
		} while (mgt->tvp_pool.slot_num <= TVP_POOL_SEGMENT_MAX_USED);
	}
}

static inline void codec_mm_update_tee_alloc_time(struct codec_mm_mgt_s *mgt,
				       u64 startus)
{
	int spend_time_us;

	spend_time_us = (int)(codec_mm_get_current_us() - startus);
	if (spend_time_us > 0 && spend_time_us < 100000000) {
		/*	>0 && less than 100s*/
			/*else think time base changed.*/
		mgt->tee_set_cnt++;
		if (spend_time_us < 10)
			mgt->tee_set_10us_less_cnt++;
		else if (spend_time_us < 50)
			mgt->tee_set_10_50us_cnt++;
		else if (spend_time_us < 100)
			mgt->tee_set_50_100us_cnt++;
		else if (spend_time_us < 1000)
			mgt->tee_set_100_1000us_cnt++;
		else if (spend_time_us < 10000)
			mgt->tee_set_1_10ms_cnt++;
		else if (spend_time_us < 100000)
			mgt->tee_set_10_100ms_cnt++;
		else
			mgt->tee_set_100ms_up_cnt++;

		mgt->tee_set_total_us += spend_time_us;
		if (spend_time_us > mgt->tee_set_max_us) {
			/*..*/
			mgt->tee_set_max_us  = spend_time_us;
		}
	}
}

static void codec_mm_update_tee_free_time(struct codec_mm_mgt_s *mgt,
				      u64 startus)
{
	int spend_time_us;

	spend_time_us = (int)(codec_mm_get_current_us() - startus);
	if (spend_time_us > 0 && spend_time_us < 100000000) {
		/*	>0 && less than 100s*/
			/*else think time base changed.*/
		mgt->tee_clear_cnt++;
		if (spend_time_us < 10)
			mgt->tee_clear_10us_less_cnt++;
		else if (spend_time_us < 50)
			mgt->tee_clear_10_50us_cnt++;
		else if (spend_time_us < 100)
			mgt->tee_clear_50_100us_cnt++;
		else if (spend_time_us < 1000)
			mgt->tee_clear_100_1000us_cnt++;
		else if (spend_time_us < 10000)
			mgt->tee_clear_1_10ms_cnt++;
		else if (spend_time_us < 100000)
			mgt->tee_clear_10_100ms_cnt++;
		else
			mgt->tee_clear_100ms_up_cnt++;

		mgt->tee_clear_total_us += spend_time_us;
		if (spend_time_us > mgt->tee_clear_max_us) {
			/*..*/
			mgt->tee_clear_max_us	= spend_time_us;
		}
	}
}

static int codec_mm_alloc_in(struct codec_mm_mgt_s *mgt, struct codec_mm_s *mem)
{
	int align_2n = mem->align2n < PAGE_SHIFT ? PAGE_SHIFT : mem->align2n;

	u32 alloc_trace_mask = 0;

	if (mem->flags & CODEC_MM_FLAGS_TVP) {
		if (secure_mem_ctrl) {
			codec_mm_clear_alloc_in(mgt, mem, align_2n, &alloc_trace_mask);
			if (mem->mem_handle) {
				mem->tee_set_start_time = codec_mm_get_current_us();
				tee_sectbl_secmem_set(mem->phy_addr, mem->buffer_size, true);
				codec_mm_update_tee_alloc_time(mgt, mem->tee_set_start_time);
				mem->tee_set_end_time = codec_mm_get_current_us();
			}
		} else {
			codec_mm_secure_alloc_in(mgt, mem, align_2n, &alloc_trace_mask);
		}
	} else {
		codec_mm_clear_alloc_in(mgt, mem, align_2n, &alloc_trace_mask);
	}

	if (mem->mem_handle)
		return 0;

	if (debug_mode & 0x10)
		codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO, "alloc flags:%d,align=%d,%d,pages:%d,s:%d try alloc mask:%x\n",
			mem->flags, mem->align2n, align_2n,
			mem->page_count, mem->buffer_size, alloc_trace_mask);

	return -10003;
}

static void codec_mm_free_in(struct codec_mm_mgt_s *mgt,
			     struct codec_mm_s *mem)
{
	unsigned long flags;

	if (secure_mem_ctrl && (mem->flags & CODEC_MM_FLAGS_TVP)) {
		u64 start_time = codec_mm_get_current_us();

		tee_sectbl_secmem_set(mem->phy_addr, mem->buffer_size, false);
		codec_mm_update_tee_free_time(mgt, start_time);
	}
	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
			codec_mm_unmap_phyaddr(mem->vbuffer);

		dma_release_from_contiguous(mgt->dev, mem->mem_handle,
					    mem->page_count);
	} else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		gen_pool_free(mgt->res_pool,
			      (unsigned long)mem->mem_handle,
			      mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
				      mem->mem_handle,
				      mem->buffer_size);
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		free_pages((unsigned long)mem->mem_handle,
			   get_order(mem->buffer_size));
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES) {
		if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
			codec_mm_unmap_phyaddr(mem->vbuffer);
		codec_mm_extpool_free((struct gen_pool *)mem->from_ext,
				      mem->mem_handle,
				      mem->buffer_size);
		if (!mgt->fastplay_enable && codec_mm_extpool_pool_release(&mgt->cma_res_pool) == 0)
			pr_info("disable cma_res mem\n");
	}
#ifdef CONFIG_CODEC_MM_EXT_POOL
	else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED_EXT) {
		gen_pool_free(mgt->res_ext_pool,
				  (unsigned long)mem->mem_handle,
				  mem->buffer_size);
	}
#endif
	spin_lock_irqsave(&mgt->lock, flags);
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR))
		mgt->total_alloced_size -= mem->buffer_size;
	if (mem->flags & CODEC_MM_FLAGS_FOR_SCATTER) {
		mgt->alloced_for_sc_size -= mem->buffer_size;
		mgt->alloced_for_sc_cnt--;
	}

	if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
		mgt->phys_vmaped_page_cnt -= mem->page_count;

	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA) {
		mgt->alloced_cma_size -= mem->buffer_size;
		ATRACE_COUNTER("codec_mm_cma_size", mgt->alloced_cma_size >> PAGE_SHIFT);
	} else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED) {
		mgt->alloced_res_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP) {
		mgt->tvp_pool.alloced_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES) {
		mgt->alloced_sys_size -= mem->buffer_size;
	} else if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES) {
		mgt->cma_res_pool.alloced_size -= mem->buffer_size;
	}
#ifdef CONFIG_CODEC_MM_EXT_POOL
	else if (mem->from_flags ==
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED_EXT) {
		mgt->alloced_res_ext_size -= mem->buffer_size;
	}
#endif

	spin_unlock_irqrestore(&mgt->lock, flags);
	if (mem->from_flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP &&
	    tvp_mode >= 1) {
		mutex_lock(&mgt->tvp_protect_lock);
		if (atomic_read(&mgt->tvp_user_count) == 0) {
			if (codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool) == 0) {
				mgt->tvp_enable = 0;
				pr_info("disable tvp\n");
			}
		}
		mutex_unlock(&mgt->tvp_protect_lock);
	}

	/*return;*/
}

static void dump_tvp_pool_info(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i, ret, size = 0;
	char *buf, *pbuf;

	buf = kzalloc(120, GFP_KERNEL);
	if (!buf)
		return;
	pbuf = buf;

	for (i = 0; i < TVP_POOL_SEGMENT_MAX_USED; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];

		if (gpool) {
			ret = snprintf(pbuf, 120 - size,
					"TVP_POOL[%d]%lx/%lx\n", i,
					(ulong)(gen_pool_size(gpool) - gen_pool_avail(gpool)),
					(ulong)gen_pool_size(gpool));
			pbuf += ret;
			size += ret;
		}
	}
	pr_info("%s", buf);
	kfree(buf);
}

struct codec_mm_s *codec_mm_alloc(const char *owner, int size,
				  int align2n, int memflags, int inst_id)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int count;
	int ret;
	unsigned long flags;
	u32 local_id = 0;

	if (codec_dbg_level(CODEC_DBG_DUMP_INFO))
		dump_stack();

	if (secure_mem_ctrl && (memflags & CODEC_MM_FLAGS_FOR_TRY_PREALLOC)) {
		bool no_check_inst_id = false;

		if (strcmp(owner, DMA_BUF_CODEC_MM) == 0 || strcmp(owner, SCATTER_MEM) == 0)
			no_check_inst_id = true;
		mem = get_mms_from_hashtable(size, align2n, inst_id, memflags, no_check_inst_id);
		if (mem) {
			if (debug_mode & 0x20)
				pr_err("mem_id [%d] %s [%s] alloc size %d used %d at 0x%lx from %d,2n:%d,flags:%d tvp_flag %d alloc time %llu us tee alloc %llu us\n",
					mem->mem_id, owner, mem->owner[0], mem->buffer_size, size,
					mem->phy_addr, mem->from_flags, align2n, memflags,
					!!(memflags & CODEC_MM_FLAGS_TVP),
					mem->end_alloc_time - mem->start_alloc_time,
					mem->tee_set_end_time - mem->tee_set_start_time);
			mem->owner[0] = owner;
			mem->flags = memflags;
			return mem;
		}
	}

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem) {
		pr_err("not enough mem for struct codec_mm_s\n");
		return NULL;
	}

	if (mgt->tvp_enable & 3) {
		/*if tvp & video decoder used tvp memory.*/
		   /*Audio don't protect for default now.*/
		if (memflags & CODEC_MM_FLAGS_TVP) {
			/*clear other flags, when tvp mode.*/
			memflags = memflags & (~CODEC_MM_FLAGS_FROM_MASK);
			memflags |= CODEC_MM_FLAGS_TVP;
		}
	} else { /*tvp not enabled*/
		if (memflags & CODEC_MM_FLAGS_TVP) {
			if (secure_mem_ctrl) {
				if (debug_mode & 0x80)
					pr_err("%s original size is %d align is %d memflags %d\n",
						owner, size, align2n, memflags);
				size = ALIGN(size, 1 << secure_mem_align2n);
				align2n = secure_mem_align2n;
			} else if (tvp_dynamic_increase_disable) {
				pr_err("TVP not enabled, when alloc from tvp %s need %d\n",
					owner, size);
			}
		}
	}

	if ((memflags & CODEC_MM_FLAGS_FROM_MASK) == 0)
		memflags |= CODEC_MM_FLAGS_DMA;

	mem->start_alloc_time = codec_mm_get_current_us();
	mem->buffer_size = PAGE_ALIGN(size);
	count = mem->buffer_size / PAGE_SIZE;
	mem->page_count = count;
	mem->align2n = align2n;
	mem->flags = memflags;
	INIT_LIST_HEAD(&mem->release_cb_list);
	mem->release_cb_cnt = 0;
	local_id = (mgt->global_local_id++) & MAX_LOCAL_ID;
	mem->local_id = local_id * 1000000 + count;
	ATRACE_ASYNC_BEGIN("codec_mm_alloc", mem->local_id);
	ret = codec_mm_alloc_in(mgt, mem);
	if (ret < 0) {
		if (fatal_signal_pending(current)) {
			pr_info("The allocation was canceled\n");
			ATRACE_ASYNC_END("codec_mm_alloc", mem->local_id);
			kfree(mem);
			return NULL;
		}
		if (mgt->alloced_for_sc_cnt > 0 && /*have used for scatter.*/
			!(memflags & CODEC_MM_FLAGS_FOR_SCATTER)) {
			/*if not scatter, free scatter caches. */
			pr_err(" No mem ret=%d, clear scatter cache!!\n", ret);
			dump_free_mem_infos(NULL, 0);
			if (memflags & CODEC_MM_FLAGS_TVP)
				codec_mm_scatter_free_all_ignorecache(2);
			else
				codec_mm_scatter_free_all_ignorecache(1);
		}
		release_all_prealloc_job_direct();
		if (atomic_read(&mgt->tvp_user_count) != 0 &&
			!(memflags & CODEC_MM_FLAGS_TVP) &&
			strncmp((void *)mem->owner, TVP_POOL_NAME, strlen(TVP_POOL_NAME)) &&
			tvp_pool_early_release_switch) {
			codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool);
			dump_tvp_pool_info();
		}
		ret = codec_mm_alloc_in(mgt, mem);
		if (ret < 0) {
			if (fatal_signal_pending(current)) {
				pr_info("The allocation was canceled\n");
			} else {
				if (mgt->total_codec_mem_size - mgt->total_alloced_size <= size)
					pr_err("not enough mem for %s size %d, ret=%d\n",
						owner, size, ret);
				else
					pr_err("not enough mem for %s size %d, ret=%d (fragmentation)\n",
						owner, size, ret);
				pr_err("mem flags %d %d, %d\n",
					memflags, mem->flags, align2n);
				codec_mm_dbuf_walk(NULL);
				dump_mem_infos(NULL, NULL);
				if (mgt->tvp_enable)
					dump_tvp_pool_info();
			}
			ATRACE_ASYNC_END("codec_mm_alloc", mem->local_id);
			kfree(mem);
			return NULL;
		}
	}
	atomic_set(&mem->use_cnt, 1);
	mem->owner[0] = owner;
	spin_lock_init(&mem->lock);
	spin_lock_irqsave(&mgt->lock, flags);
	mem->mem_id = mgt->global_memid++;
	ATRACE_ASYNC_END("codec_mm_alloc", mem->local_id);
	ATRACE_COUNTER("codec_mm_alloc_id", mem->mem_id);
	ATRACE_COUNTER("codec_mm_alloc_id", 0);
	list_add_tail(&mem->list, &mgt->mem_list);
	switch (mem->from_flags) {
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_PAGES:
		mgt->alloced_sys_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA:
		mgt->alloced_cma_size += mem->buffer_size;
		ATRACE_COUNTER("codec_mm_cma_size", mgt->alloced_cma_size >> PAGE_SHIFT);
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP:
		mgt->tvp_pool.alloced_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED:
		mgt->alloced_res_size += mem->buffer_size;
		break;
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES:
		mgt->cma_res_pool.alloced_size += mem->buffer_size;
		break;
#ifdef CONFIG_CODEC_MM_EXT_POOL
	case AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED_EXT:
		mgt->alloced_res_ext_size += mem->buffer_size;
		break;
#endif
	default:
		pr_err("error alloc flags %d\n", mem->from_flags);
	}
	if (!(mem->flags & CODEC_MM_FLAGS_FOR_LOCAL_MGR)) {
		mgt->total_alloced_size += mem->buffer_size;
		if (mgt->total_alloced_size > mgt->max_used_mem_size)
			mgt->max_used_mem_size = mgt->total_alloced_size;
	}
	if ((mem->flags & CODEC_MM_FLAGS_FOR_SCATTER)) {
		mgt->alloced_for_sc_size += mem->buffer_size;
		mgt->alloced_for_sc_cnt++;
	}

	if (mem->flags & CODEC_MM_FLAGS_FOR_PHYS_VMAPED)
		mgt->phys_vmaped_page_cnt += mem->page_count;

	spin_unlock_irqrestore(&mgt->lock, flags);
	mem->alloced_jiffies = get_jiffies_64();

	mem->end_alloc_time = codec_mm_get_current_us();
	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE, "mem_id [%d] %s alloc size %d at 0x%lx from %d,2n:%d,flags:%d tvp_flag %d alloc time %llu us tee alloc %llu us\n",
		mem->mem_id, owner, size, mem->phy_addr,
		mem->from_flags,
		align2n,
		memflags,
		!!(memflags & CODEC_MM_FLAGS_TVP),
		mem->end_alloc_time - mem->start_alloc_time,
		mem->tee_set_end_time - mem->tee_set_start_time);

	return mem;
}
EXPORT_SYMBOL(codec_mm_alloc);

void codec_mm_release(struct codec_mm_s *mem, const char *owner)
{
	int index;
	unsigned long flags;
	int i, j;
	const char *max_owner;
	struct codec_mm_cb_s **release_cb;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (!mem)
		return;

	release_cb = (struct codec_mm_cb_s **)
		kzalloc(sizeof(struct codec_mm_cb_s *) * (mem->release_cb_cnt + 1), GFP_KERNEL);

	if (codec_dbg_level(CODEC_DBG_DUMP_INFO))
		dump_stack();

	spin_lock_irqsave(&mgt->lock, flags);
	if (!codec_mm_valid_mm_locked(mem)) {
		pr_err("codec mm not valid!\n");
		spin_unlock_irqrestore(&mgt->lock, flags);
		kfree(release_cb);
		return;
	}
	index = atomic_dec_return(&mem->use_cnt);
	max_owner = mem->owner[index];
	for (i = 0; i < index; i++) {
		if (mem->owner[i] && strcmp(owner, mem->owner[i]) == 0)
			mem->owner[i] = max_owner;
	}

	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE, "mem_id [%d] %s free mem size %d at %lx from %d,index =%d comm :%s pid:%d tgid:%d\n",
		mem->mem_id, owner, mem->buffer_size, mem->phy_addr,
		mem->from_flags, index, current->comm, current->pid, current->tgid);

	mem->owner[index] = NULL;
	if (index == 0) {
		struct codec_mm_cb_s *cur, *tmp;
		list_del(&mem->list);
		i = 0;
		list_for_each_entry_safe(cur,
			tmp, &mem->release_cb_list, node) {
			list_del_init(&cur->node);
			if (release_cb)
				release_cb[i] = cur;
			i++;
			if (i > mem->release_cb_cnt) {
				pr_err("%s, error, release cnt %d less than loop %d\n",
					__func__, mem->release_cb_cnt, i);
				break;
			}
		}
		spin_unlock_irqrestore(&mgt->lock, flags);

		if (release_cb) {
			for (j = 0; j < i; j++) {
				cur = release_cb[j];
				if (cur)
					cur->func(mem, cur);
				mem->release_cb_cnt--;
			}
		}
		if (mem->release_cb_cnt != 0) {
			pr_err("%s, error, i %d, release %d\n",
					__func__, i, mem->release_cb_cnt);
		}

		ATRACE_ASYNC_BEGIN("codec_mm_free", mem->local_id);
		codec_mm_free_in(mgt, mem);
		ATRACE_ASYNC_END("codec_mm_free", mem->local_id);
		ATRACE_COUNTER("codec_mm_free_id", mem->mem_id);
		ATRACE_COUNTER("codec_mm_free_id", 0);
		kfree(mem);
		kfree(release_cb);
		return;
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	kfree(release_cb);
}
EXPORT_SYMBOL(codec_mm_release);

void *codec_mm_dma_alloc_coherent(ulong *handle,
				  ulong *phy_out,
				  int size,
				  const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	void *vaddr;
	dma_addr_t dma_handle = 0;
	int buf_size = PAGE_ALIGN(size);
	ulong flags;
	u32 local_id = 0;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		goto err;

	mem->start_alloc_time = codec_mm_get_current_us();
	local_id = (mgt->global_local_id++) & MAX_LOCAL_ID;
	mem->local_id = local_id * 1000000 + (buf_size >> PAGE_SHIFT);
	ATRACE_ASYNC_BEGIN("codec_mm_alloc", mem->local_id);
	codec_mm_compact_lock();
	range_conflict_check_wait(mgt->cma, mem->page_count, 0);

	vaddr = dma_alloc_coherent(mgt->dev, buf_size, &dma_handle, GFP_KERNEL);
	codec_mm_compact_unlock();
	mem->end_alloc_time = codec_mm_get_current_us();
	ATRACE_ASYNC_END("codec_mm_alloc", mem->local_id);

	if (!vaddr)
		goto err;

	mem->owner[0]	= owner;
	mem->vbuffer	= vaddr;
	mem->phy_addr	= dma_handle;
	mem->buffer_size = buf_size;
	mem->from_flags	= AMPORTS_MEM_FLAGS_FROM_GET_FROM_COHERENT;
	mem->alloced_jiffies = get_jiffies_64();

	spin_lock_irqsave(&mgt->lock, flags);

	mem->mem_id = mgt->global_memid++;
	ATRACE_COUNTER("codec_mm_alloc_id", mem->mem_id);
	ATRACE_COUNTER("codec_mm_alloc_id", 0);
	mgt->alloced_cma_size	+= buf_size;
	mgt->alloced_from_coherent	+= buf_size;
	mgt->total_alloced_size		+= buf_size;
	ATRACE_COUNTER("codec_mm_cma_size", mgt->alloced_cma_size >> PAGE_SHIFT);
	if (mgt->total_alloced_size > mgt->max_used_mem_size)
		mgt->max_used_mem_size = mgt->total_alloced_size;
	*handle				= (ulong)mem;
	*phy_out			= mem->phy_addr;
	list_add_tail(&mem->list, &mgt->mem_list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
			   "mem_id [%d] [%s] alloc coherent mem (phy %lx, vddr %px) size (%d) alloc_time %llu us.\n",
			   mem->mem_id, owner, mem->phy_addr, vaddr, buf_size,
			   mem->end_alloc_time - mem->start_alloc_time);

	return vaddr;
err:
	kfree(mem);

	return NULL;
}
EXPORT_SYMBOL(codec_mm_dma_alloc_coherent);

void codec_mm_dma_free_coherent(ulong handle)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = (struct codec_mm_s *)handle;
	ulong flags;

	if (!handle)
		return;

	ATRACE_ASYNC_BEGIN("codec_mm_free", mem->local_id);

	dma_free_coherent(mgt->dev, mem->buffer_size,
		mem->vbuffer, mem->phy_addr);

	ATRACE_ASYNC_END("codec_mm_free", mem->local_id);
	ATRACE_COUNTER("codec_mm_free_id", mem->mem_id);
	ATRACE_COUNTER("codec_mm_free_id", 0);

	spin_lock_irqsave(&mgt->lock, flags);

	mgt->alloced_cma_size	-= mem->buffer_size;
	mgt->alloced_from_coherent	-= mem->buffer_size;
	mgt->total_alloced_size		-= mem->buffer_size;
	list_del(&mem->list);
	ATRACE_COUNTER("codec_mm_cma_size", mgt->alloced_cma_size >> PAGE_SHIFT);
	spin_unlock_irqrestore(&mgt->lock, flags);

	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
		   "mem_id [%d] [%s] free coherent mem (phy %lx, vddr %px) size (%d)\n",
		   mem->mem_id, mem->owner[0], mem->phy_addr, mem->vbuffer,
		   mem->buffer_size);

	kfree(mem);
}
EXPORT_SYMBOL(codec_mm_dma_free_coherent);

void codec_mm_release_with_check(struct codec_mm_s *mem, const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&mgt->lock, flags);
	ret = codec_mm_valid_mm_locked(mem);
	spin_unlock_irqrestore(&mgt->lock, flags);
	if (ret) {
		/*for check,*/
		return codec_mm_release(mem, owner);
	}
}
EXPORT_SYMBOL(codec_mm_release_with_check);

void codec_mm_dma_flush(void *vaddr,
			int size,
			enum dma_data_direction dir)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	dma_addr_t dma_addr;
	ulong phy_addr;

	if (is_vmalloc_or_module_addr(vaddr)) {
		phy_addr = codec_mm_search_phy_addr(vaddr);
		if (!phy_addr)
			phy_addr = page_to_phys(vmalloc_to_page(vaddr))
				+ offset_in_page(vaddr);
		if (phy_addr && PageHighMem(phys_to_page(phy_addr)))
			dma_sync_single_for_device(mgt->dev,
						   phy_addr, size, dir);
		return;
	}

	/* only apply to the lowmem. */
	dma_addr = dma_map_single(mgt->dev, vaddr, size, dir);
	if (dma_mapping_error(mgt->dev, dma_addr)) {
		pr_err("dma map %d bytes error\n", size);
		return;
	}

	dma_sync_single_for_device(mgt->dev, dma_addr, size, dir);
	dma_unmap_single(mgt->dev, dma_addr, size, dir);
}
EXPORT_SYMBOL(codec_mm_dma_flush);

void codec_mm_inval_cache(dma_addr_t addr, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	dma_sync_single_for_cpu(mgt->dev, addr, size, DMA_FROM_DEVICE);
}
EXPORT_SYMBOL(codec_mm_inval_cache);

int codec_mm_has_owner(struct codec_mm_s *mem, const char *owner)
{
	int index;
	int i;
	unsigned long flags;
	int is_owner = 0;

	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (mem) {
		spin_lock_irqsave(&mgt->lock, flags);
		if (!codec_mm_valid_mm_locked(mem)) {
			spin_unlock_irqrestore(&mgt->lock, flags);
			pr_err("codec mm %p not valid!\n", mem);
			return 0;
		}

		index = atomic_read(&mem->use_cnt);

		for (i = 0; i < index; i++) {
			if (mem->owner[i] &&
			    strcmp(owner, mem->owner[i]) == 0) {
				is_owner = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&mgt->lock, flags);
	}

	return is_owner;
}

int codec_mm_request_shared_mem(struct codec_mm_s *mem, const char *owner)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;
	int ret = -1;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!codec_mm_valid_mm_locked(mem)) {
		ret = -2;
		goto out;
	}
	if (atomic_read(&mem->use_cnt) > 7) {
		ret = -3;
		goto out;
	}
	ret = 0;
	mem->owner[atomic_inc_return(&mem->use_cnt) - 1] = owner;

out:
	spin_unlock_irqrestore(&mgt->lock, flags);

	return ret;
}
EXPORT_SYMBOL(codec_mm_request_shared_mem);

static struct codec_mm_s *codec_mm_get_by_val_off(unsigned long val, int off)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem, *want_mem;
	unsigned long flags;

	want_mem = NULL;
	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem && VAL_OFF_VAL(mem, off) == val)
				want_mem = mem;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
	return want_mem;
}

unsigned long codec_mm_alloc_for_dma(const char *owner, int page_cnt,
				     int align2n, int memflags)
{
	struct codec_mm_s *mem;

	mem = codec_mm_alloc(owner, page_cnt << PAGE_SHIFT, align2n, memflags, -1);
	if (!mem)
		return 0;
	return mem->phy_addr;
}
EXPORT_SYMBOL(codec_mm_alloc_for_dma);

unsigned long codec_mm_alloc_for_dma_ex(const char *owner,
					int page_cnt,
					int align2n,
					int memflags,
					int ins_id,
					int buffer_id)
{
	struct codec_mm_s *mem;

	mem = codec_mm_alloc(owner, page_cnt << PAGE_SHIFT, align2n, memflags, -1);
	if (!mem)
		return 0;
	mem->ins_id = ins_id;
	mem->ins_buffer_id = buffer_id;
	codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO,
			   "%s, for ins %d, buffer id:%d\n",
			    mem->owner[0] ? mem->owner[0] : "no",
			    mem->ins_id,
			    buffer_id);

	return mem->phy_addr;
}
EXPORT_SYMBOL(codec_mm_alloc_for_dma_ex);

int codec_mm_free_for_dma(const char *owner, unsigned long phy_addr)
{
	struct codec_mm_s *mem;

	mem = codec_mm_get_by_val_off(phy_addr, PHY_OFF());

	if (mem)
		codec_mm_release(mem, owner);
	else
		return -1;
	return 0;
}
EXPORT_SYMBOL(codec_mm_free_for_dma);

static int codec_mm_init_tvp_pool(struct extpool_mgt_s *tvp_pool,
				  struct codec_mm_s *mm)
{
	struct gen_pool *pool;
	int ret;

	pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!pool)
		return -ENOMEM;
	ret = gen_pool_add(pool, mm->phy_addr, mm->buffer_size, -1);
	if (ret < 0) {
		gen_pool_destroy(pool);
		return -1;
	}
	tvp_pool->gen_pool[tvp_pool->slot_num] = pool;
	mm->tvp_handle = -1;
	tvp_pool->mm[tvp_pool->slot_num] = mm;
	return 0;
}

static int codec_mm_tvp_pool_protect(struct extpool_mgt_s *tvp_pool)
{
	int ret = 0;
	int i = 0;

	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (tvp_pool->mm[i]->tvp_handle == -1) {
			if (secure_mem_ctrl) {
				ret = tee_sectbl_secmem_set(tvp_pool->mm[i]->phy_addr,
					tvp_pool->mm[i]->buffer_size, 1);

				tvp_pool->mm[i]->tvp_handle = i;
			} else {
				ret = tee_protect_tvp_mem
					(tvp_pool->mm[i]->phy_addr,
					tvp_pool->mm[i]->buffer_size,
					&tvp_pool->mm[i]->tvp_handle);
			}
			pr_info("protect tvp %d %d ret %d %lx %x\n", i, tvp_pool->mm[i]->tvp_handle,
				ret, tvp_pool->mm[i]->phy_addr, tvp_pool->mm[i]->buffer_size);

			if (ret)
				break;
		} else {
			pr_info("protect tvp %d %d ret %d\n",
				i, tvp_pool->mm[i]->tvp_handle, ret);
		}
	}
	return ret;
}

static bool codec_mm_from_reserved(int flags)
{
	if (flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES ||
		flags == AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED)
		return true;

	return false;
}

static int codec_mm_extpool_pool_release_inner(int slot_num_start,
					       struct extpool_mgt_s *tvp_pool)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int i;

	for (i = slot_num_start; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (tvp_pool->mm[i] && tvp_pool->mm[i]->tvp_handle >= 0) {
				pr_info("unprotect tvp %d handle is %d\n", i,
					tvp_pool->mm[i]->tvp_handle);

				if (secure_mem_ctrl)
					tee_sectbl_secmem_set(tvp_pool->mm[i]->phy_addr,
						tvp_pool->mm[i]->buffer_size, 0);
				else
					tee_unprotect_tvp_mem(tvp_pool->mm[i]->tvp_handle);

				tvp_pool->mm[i]->tvp_handle = -1;
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				if (codec_mm_from_reserved(tvp_pool->mm[i]->from_flags))
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm,
					   tvp_pool->mm[i]->page_count,
					   1);
				codec_mm_release(tvp_pool->mm[i],
						 TVP_POOL_NAME);
			}
		}
		mgt->tvp_pool.total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	tvp_pool->slot_num = slot_num_start;
	return slot_num_start;
}

static int codec_mm_tvp_pool_alloc_by_type(struct extpool_mgt_s *tvp_pool,
	int size, int memflags, int type)
{
	struct codec_mm_s *mem;
	int ret;

	if (type == CODEC_MM_FLAGS_CMA) {
		mem = codec_mm_alloc(TVP_POOL_NAME,
					size,
					RESERVE_MM_ALIGNED_2N,
					CODEC_MM_FLAGS_FOR_LOCAL_MGR |
					CODEC_MM_FLAGS_CMA,
					-1);
		if (mem) {
			struct page *mm = mem->mem_handle;

			if (codec_mm_from_reserved(mem->from_flags))
				mm = phys_to_page((unsigned long)mm);
			cma_mmu_op(mm, mem->page_count, 0);
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				cma_mmu_op(mm, mem->page_count, 1);
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				tvp_pool->total_size += size;
				tvp_pool->slot_num++;
				if (tvp_mode >= 1) {
					if (codec_mm_tvp_pool_protect(tvp_pool)) {
						codec_mm_extpool_pool_release_inner
							(tvp_pool->slot_num - 1, tvp_pool);
						return 1;
					}
				}
				return 0;
			}
		}
	} else if (type == CODEC_MM_FLAGS_RESERVED) {
		mem = codec_mm_alloc(TVP_POOL_NAME,
				size,
				RESERVE_MM_ALIGNED_2N,
				CODEC_MM_FLAGS_FOR_LOCAL_MGR |
				CODEC_MM_FLAGS_RESERVED,
				-1);

		if (mem) {
			struct page *mm = mem->mem_handle;

			if (codec_mm_from_reserved(mem->from_flags))
				mm = phys_to_page((unsigned long)mm);
			cma_mmu_op(mm, mem->page_count, 0);
			ret = codec_mm_init_tvp_pool(tvp_pool, mem);
			if (ret < 0) {
				codec_mm_release(mem, TVP_POOL_NAME);
			} else {
				tvp_pool->total_size += size;
				tvp_pool->slot_num++;
				if (tvp_mode >= 1) {
					if (codec_mm_tvp_pool_protect(tvp_pool)) {
						codec_mm_extpool_pool_release_inner
							(tvp_pool->slot_num - 1, tvp_pool);
						return 1;
					}
				}
				return 0;
			}
		}
	}
	return 1;
}

static int codec_mm_try_alloc_more_tvp(void *param)
{
	int alloc_size = 0;
	struct extpool_mgt_s *tvp_pool = (struct extpool_mgt_s *)param;

	if (!tvp_pool)
		return 0;

	if (!kthread_should_stop()) {
		mutex_lock(&tvp_pool->pool_lock);
		if (tvp_pool->slot_num == 1) {
			mutex_unlock(&tvp_pool->pool_lock);
			set_cma_task_priority_level(CMA_TASK_NICE_LEVEL_2);
			alloc_size = codec_mm_tvp_pool_alloc_by_slot(tvp_pool, 0, 2);
			set_cma_task_priority_level(CMA_TASK_NICE_LEVEL_0);
			if (alloc_size <= 0)
				pr_err("Async alloc more tvp failed\n");
		} else {
			mutex_unlock(&tvp_pool->pool_lock);
		}
	}

	tvp_pool->tvp_kthread = NULL;
	return 0;
}

static int codec_mm_tvp_pool_alloc_by_slot(struct extpool_mgt_s *tvp_pool,
	int memflags, int flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int size = 0;
	int try_alloced_size = 0;
	int max_reserved_free_size = 0;
	int max_cma_reserved_free_size = 0;
	int max_cma_free_size = 0;
	int use_cma_pool_first = 0;
	int retry_cnt = 0;
	int alloc_more = 0;
	u64 start_ms = codec_mm_get_current_ms();
	u64 end_ms = 0;
	u64 spend_ms = 0;

	mutex_lock(&tvp_pool->pool_lock);
	max_cma_reserved_free_size = mgt->cma_res_pool.total_size - mgt->cma_res_pool.alloced_size;
	max_reserved_free_size = mgt->total_reserved_size - mgt->alloced_res_size;
	if (max_cma_reserved_free_size > max_reserved_free_size)
		max_reserved_free_size = max_cma_reserved_free_size;
	max_cma_free_size = mgt->total_cma_size - mgt->alloced_cma_size;
	use_cma_pool_first = max_cma_free_size > max_reserved_free_size ? 1 : 0;

	if (tvp_pool->slot_num >= TVP_POOL_SEGMENT_MAX_USED) {
		try_alloced_size = 0;
		pr_info("slot_num  exceed the limit %d\n", tvp_pool->slot_num);
		goto alloced_finished;
	}

	size = default_tvp_pool_segment_size[tvp_pool->slot_num];
	if (size <= 0) {
		try_alloced_size = 0;
		goto alloced_finished;
	}

	if (!tvp_dynamic_increase_disable && tvp_pool->slot_num == 0 &&
		tvp_pool_segment_maxsize_0 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		if (size - tvp_pool_segment_maxsize_0 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
			default_tvp_pool_segment_size[0] = tvp_pool_segment_maxsize_0;
			default_tvp_pool_segment_size[1] = size - tvp_pool_segment_maxsize_0;
			size = tvp_pool_segment_maxsize_0;
			alloc_more = 1;
		}
	}

	if (use_cma_pool_first) {
		try_alloced_size = size > max_cma_free_size ? max_cma_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_CMA)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	} else {
		try_alloced_size = size > max_reserved_free_size ? max_reserved_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_RESERVED)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	}

	if (use_cma_pool_first) {
		try_alloced_size = size > max_reserved_free_size ? max_reserved_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_RESERVED)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	} else {
		try_alloced_size = size > max_cma_free_size ? max_cma_free_size : size;
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
		retry_cnt = try_alloced_size / (4 * SZ_1M);
		if (try_alloced_size > 0) {
			int retry = 0;

			do {
				if (codec_mm_tvp_pool_alloc_by_type(tvp_pool,
					try_alloced_size, memflags, CODEC_MM_FLAGS_CMA)) {
					try_alloced_size = try_alloced_size - 4 * SZ_1M;
					if (try_alloced_size < 16 * SZ_1M)
						break;
					try_alloced_size = codec_mm_align_up2n(try_alloced_size,
							RESERVE_MM_ALIGNED_2N);
				} else {
					goto alloced_finished;
				}
			} while (retry++ < retry_cnt);
		}
	}
	try_alloced_size = 0;
alloced_finished:
	if (try_alloced_size && !tvp_dynamic_increase_disable && !flags) {
		pr_info("Force enable tvp, please enable it by resource manager or secmem");
		mgt->tvp_enable = 2;
	}

	if (prealloc_more_tvp && tvp_pool->slot_num == 1 && !tvp_pool->tvp_kthread) {
		tvp_pool->tvp_kthread = kthread_run(codec_mm_try_alloc_more_tvp,
			(void *)tvp_pool, "tvp_alloc_more");
		if (!tvp_pool->tvp_kthread)
			pr_info("Create thread enable second segment error");
	}

	if (alloc_more && try_alloced_size > 0 && !tvp_pool->tvp_kthread) {
		tvp_pool->tvp_kthread = kthread_run(codec_mm_try_alloc_more_tvp,
			(void *)tvp_pool, "tvp_alloc_more");
		if (!tvp_pool->tvp_kthread)
			pr_info("Create thread enable second segment error");
	}

	mutex_unlock(&tvp_pool->pool_lock);

	end_ms = codec_mm_get_current_ms();
	spend_ms = end_ms - start_ms;

	pr_info("Enable tvp %d size %d MB spend %d ms\n", tvp_pool->slot_num,
		try_alloced_size / 1024 / 1024, (u32)spend_ms);

	return try_alloced_size;
}

static void codec_mm_tvp_alloc_monitor(struct work_struct *work)
{
	int ret = 0;
	struct codec_mm_mgt_s *mgt = container_of(work,
		struct codec_mm_mgt_s, tvp_alloc_wrk);

	while (mgt->tvp_pool.total_size < (AV1_4K_FG_MEM_SIZE * 1024 * 1024)) {
		ret = codec_mm_tvp_pool_alloc_by_slot(&mgt->tvp_pool, 0, mgt->tvp_enable);
		if (ret == 0) {
			pr_err("prealloc tvp_pool fail\n");
			return;
		}
	}
}

void codec_mm_prealloc_tvp_pool(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (mgt->tvp_enable < 2 || tvp_dynamic_increase_disable) // not 4k secure video,do nothing
		return;

	schedule_work(&mgt->tvp_alloc_wrk);
}
EXPORT_SYMBOL(codec_mm_prealloc_tvp_pool);

int codec_mm_extpool_pool_alloc(struct extpool_mgt_s *tvp_pool,
	int size, int memflags, int for_tvp)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	int alloced_size = tvp_pool->total_size;
	int try_alloced_size = 0;
	int ret;
	int retry_cnt = size / (4 * SZ_1M);
	int slot_num = tvp_pool->slot_num;
	int max_cma_reserved_free_size = 0;
	u64 start_ms = codec_mm_get_current_ms();
	u64 end_ms = 0;
	u64 spend_ms = 0;

	/*alloced from reserved*/
	mutex_lock(&tvp_pool->pool_lock);
	if (alloced_size >= size)
		goto alloced_finished1;
	max_cma_reserved_free_size = mgt->cma_res_pool.total_size - mgt->cma_res_pool.alloced_size;
	try_alloced_size = mgt->total_reserved_size - mgt->alloced_res_size;
	if (max_cma_reserved_free_size > try_alloced_size)
		try_alloced_size = max_cma_reserved_free_size;
	if (try_alloced_size > 0 && for_tvp) {
		int retry = 0;
		try_alloced_size = min_t(int,
			size - alloced_size, try_alloced_size);
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
			RESERVE_MM_ALIGNED_2N);
		do {
			mem = codec_mm_alloc(TVP_POOL_NAME,
						try_alloced_size,
						RESERVE_MM_ALIGNED_2N,
						CODEC_MM_FLAGS_FOR_LOCAL_MGR |
						CODEC_MM_FLAGS_RESERVED,
						-1);

			if (mem) {
				struct page *mm = mem->mem_handle;

				if (codec_mm_from_reserved(mem->from_flags))
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm, mem->page_count, 0);
				ret = codec_mm_init_tvp_pool(tvp_pool, mem);
				if (ret < 0) {
					codec_mm_release(mem, TVP_POOL_NAME);
				} else {
					alloced_size += try_alloced_size;
					tvp_pool->slot_num++;
				}
				try_alloced_size = size - alloced_size;
			} else {
				try_alloced_size = try_alloced_size - 4 * SZ_1M;
				if (try_alloced_size < 16 * SZ_1M)
					break;
			}
			if (tvp_pool->slot_num < TVP_POOL_SEGMENT_MAX_USED &&
			    alloced_size < size) {
				try_alloced_size = codec_mm_align_up2n
					(try_alloced_size, RESERVE_MM_ALIGNED_2N);
			} else {
				break;
			}
		} while (retry++ < retry_cnt);
	}
	if (alloced_size >= size || tvp_pool->slot_num >= TVP_POOL_SEGMENT_MAX_USED) {
		/*alloc finished. */
		goto alloced_finished;
	}

/*alloced from cma:*/
	try_alloced_size = mgt->total_cma_size - mgt->alloced_cma_size;
	if (try_alloced_size > 0) {
		int retry = 0;
		int memflags = CODEC_MM_FLAGS_FOR_LOCAL_MGR | CODEC_MM_FLAGS_CMA;

		if (for_tvp)
			memflags |= CODEC_MM_FLAGS_RESERVED;

		try_alloced_size = min_t(int,
			size - alloced_size, try_alloced_size);
		try_alloced_size = MM_ALIGN_DOWN_2N(try_alloced_size,
			RESERVE_MM_ALIGNED_2N);
		do {
			mem = codec_mm_alloc(for_tvp ? TVP_POOL_NAME :
						CMA_RES_POOL_NAME,
					try_alloced_size, RESERVE_MM_ALIGNED_2N,
					memflags, -1);
			if (mem) {
				struct page *mm = mem->mem_handle;

				if (codec_mm_from_reserved(mem->from_flags))
					mm = phys_to_page((unsigned long)mm);
				if (for_tvp) {
					cma_mmu_op(mm,
						mem->page_count,
						0);
				}
				ret = codec_mm_init_tvp_pool(tvp_pool, mem);
				if (ret < 0) {
					if (for_tvp) {
						cma_mmu_op(mm,
							mem->page_count,
							1);
					}
					codec_mm_release(mem, TVP_POOL_NAME);
				} else {
					alloced_size += try_alloced_size;
					tvp_pool->slot_num++;
				}
				try_alloced_size = size - alloced_size;
			} else {
				try_alloced_size = try_alloced_size - 4 * SZ_1M;
				if (try_alloced_size < 16 * SZ_1M)
					break;
			}
			if (tvp_pool->slot_num < TVP_POOL_SEGMENT_MAX_USED &&
			    alloced_size < size) {
				try_alloced_size = codec_mm_align_up2n(try_alloced_size,
					RESERVE_MM_ALIGNED_2N);
			} else {
				break;
			}
		} while (retry++ < retry_cnt);
	}

alloced_finished:
	if (alloced_size > 0)
		tvp_pool->total_size = alloced_size;
	if (for_tvp) {
		if (alloced_size >= size) {
			if (tvp_mode >= 1)
				codec_mm_tvp_pool_protect(tvp_pool);
		} else {
			codec_mm_extpool_pool_release_inner(slot_num, tvp_pool);
			alloced_size = 0;
		}
	}
alloced_finished1:
	mutex_unlock(&tvp_pool->pool_lock);

	end_ms = codec_mm_get_current_ms();
	spend_ms = end_ms - start_ms;

	pr_info("Enable segment for tvp %d %d size %d MB spend %d ms\n", for_tvp,
		tvp_pool->slot_num, try_alloced_size / 1024 / 1024, (u32)spend_ms);

	return alloced_size;
}
EXPORT_SYMBOL(codec_mm_extpool_pool_alloc);

/*
 *call this before free all
 *alloced TVP memory;
 *it not free,some memory may free ignore.
 *return :
 */
static int codec_mm_extpool_pool_release(struct extpool_mgt_s *tvp_pool)
{
	int i;
	int ignored = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (gen_pool_avail(gpool) != gen_pool_size(gpool)) {
				pr_err("ext pool is not free.\n");
				ignored++;
				continue;	/*ignore this free now, */
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				if (codec_mm_from_reserved(tvp_pool->mm[i]->from_flags))
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm,
					   tvp_pool->mm[i]->page_count,
					   1);
				codec_mm_release(tvp_pool->mm[i],
						 TVP_POOL_NAME);
			}
		}
		tvp_pool->total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	if (ignored > 0) {
		int before_free_slot = tvp_pool->slot_num + 1;

		for (i = 0; i < tvp_pool->slot_num; i++) {
			if (tvp_pool->gen_pool[i] && before_free_slot < i) {
				tvp_pool->gen_pool[before_free_slot] =
					tvp_pool->gen_pool[i];
				tvp_pool->mm[before_free_slot] =
					tvp_pool->mm[i];
				swap(default_tvp_pool_segment_size[i],
						default_tvp_pool_segment_size[before_free_slot]);
				tvp_pool->gen_pool[i] = NULL;
				tvp_pool->mm[i] = NULL;
				before_free_slot++;
			}
			if (!tvp_pool->gen_pool[i] && before_free_slot > i) {
				before_free_slot = i;
				/**/
			}
		}
	}
	tvp_pool->slot_num = ignored;
	mutex_unlock(&tvp_pool->pool_lock);
	return ignored;
}

static int codec_mm_tvp_pool_unprotect_and_release(struct extpool_mgt_s *tvp_pool)
{
	int i;
	int ignored = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		struct gen_pool *gpool = tvp_pool->gen_pool[i];
		int slot_mem_size = 0;

		if (gpool) {
			if (gen_pool_avail(gpool) != gen_pool_size(gpool)) {
				codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
					"Warn: TVP pool will release later.\n");
				ignored++;
				continue;	/*ignore this free now, */
			}
			slot_mem_size = gen_pool_size(gpool);
			gen_pool_destroy(tvp_pool->gen_pool[i]);
			if (tvp_pool->mm[i]) {
				struct page *mm = tvp_pool->mm[i]->mem_handle;

				pr_info("unprotect tvp %d handle is %d\n",
					i, tvp_pool->mm[i]->tvp_handle);

				if (tvp_pool->mm[i]->tvp_handle >= 0) {
					if (secure_mem_ctrl)
						tee_sectbl_secmem_set(tvp_pool->mm[i]->phy_addr,
							tvp_pool->mm[i]->buffer_size, 0);
					else
						tee_unprotect_tvp_mem(tvp_pool->mm[i]->tvp_handle);

					tvp_pool->mm[i]->tvp_handle = -1;
				}

				if (codec_mm_from_reserved(tvp_pool->mm[i]->from_flags))
					mm = phys_to_page((unsigned long)mm);
				cma_mmu_op(mm, tvp_pool->mm[i]->page_count, 1);
				codec_mm_release(tvp_pool->mm[i], TVP_POOL_NAME);
			}
		}
		tvp_pool->total_size -= slot_mem_size;
		tvp_pool->gen_pool[i] = NULL;
		tvp_pool->mm[i] = NULL;
	}
	if (ignored > 0) {
		int before_free_slot = tvp_pool->slot_num + 1;

		for (i = 0; i < tvp_pool->slot_num; i++) {
			if (tvp_pool->gen_pool[i] && before_free_slot < i) {
				tvp_pool->gen_pool[before_free_slot] =
					tvp_pool->gen_pool[i];
				tvp_pool->mm[before_free_slot] =
					tvp_pool->mm[i];
				tvp_pool->mm[before_free_slot]->tvp_handle =
					tvp_pool->mm[i]->tvp_handle;
				swap(default_tvp_pool_segment_size[i],
						default_tvp_pool_segment_size[before_free_slot]);
				tvp_pool->gen_pool[i] = NULL;
				tvp_pool->mm[i] = NULL;
				before_free_slot++;
			}
			if (!tvp_pool->gen_pool[i] && before_free_slot > i) {
				before_free_slot = i;
				/**/
			}
		}
	}
	tvp_pool->slot_num = ignored;
	mutex_unlock(&tvp_pool->pool_lock);
	return ignored;
}

/*
 *victor_size
 *=sizeof(res)/sizeof(ulong)
 *res[0]=addr1_start;
 *res[1]=addr1_end;
 *res[2]=addr2_start;
 *res[3]=addr2_end;
 *..
 *res[2*n]=addrx_start;
 *res[2*n+1]=addrx_end;
 *
 *return n;
 */
static int codec_mm_tvp_get_mem_resource(ulong *res, int victor_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;
	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num && i < victor_size / 2; i++) {
		if (tvp_pool->mm[i]) {
			res[2 * i] = tvp_pool->mm[i]->phy_addr;
			res[2 * i + 1] = tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return i;
}

static int codec_mm_is_in_tvp_region(ulong phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct extpool_mgt_s *tvp_pool = &mgt->tvp_pool;
	int i;
	int in = 0, in2 = 0;

	mutex_lock(&tvp_pool->pool_lock);
	for (i = 0; i < tvp_pool->slot_num; i++) {
		if (tvp_pool->mm[i]) {
			in = tvp_pool->mm[i]->phy_addr <= phy_addr;
			in2 = (tvp_pool->mm[i]->phy_addr +
				tvp_pool->mm[i]->buffer_size - 1) >= phy_addr;
			in = in && in2;
			if (in)
				break;
			in = 0;
		}
	}
	mutex_unlock(&tvp_pool->pool_lock);
	return in;
}

void *codec_mm_phys_to_virt(unsigned long phy_addr)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (codec_mm_is_in_tvp_region(phy_addr))
		return NULL;	/* no virt for tvp memory; */

	if (phy_addr >= mgt->rmem.base &&
	    phy_addr < mgt->rmem.base + mgt->rmem.size) {
		if (mgt->res_mem_flags & RES_MEM_FLAGS_HAVE_MAPED)
			return codec_mm_search_vaddr(phy_addr);
		return NULL;	/* no virt for reserved memory; */
	}

	return codec_mm_search_vaddr(phy_addr);
}
EXPORT_SYMBOL(codec_mm_phys_to_virt);

unsigned long codec_mm_virt_to_phys(void *vaddr)
{
	return page_to_phys((struct page *)vaddr);
}
EXPORT_SYMBOL(codec_mm_virt_to_phys);

unsigned long dma_get_cma_size_int_byte(struct device *dev)
{
	unsigned long size = 0;
	struct cma *cma = NULL;

	if (!dev) {
		pr_err("CMA: NULL DEV\n");
		return 0;
	}

	cma = dev_get_cma_area(dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		return 0;
	}
	size = cma_get_size(cma);
	return size;
}
EXPORT_SYMBOL(dma_get_cma_size_int_byte);

static int codec_mm_get_cma_size_int_byte(struct device *dev)
{
	static int static_size = -1;
	struct cma *cma = NULL;

	if (static_size >= 0)
		return static_size;
	if (!dev) {
		pr_err("CMA: NULL DEV\n");
		return 0;
	}

	cma = dev_get_cma_area(dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		return 0;
	}
	if (cma == dev_get_cma_area(NULL))
		static_size = 0;/*ignore default cma pool*/
	else
		static_size = cma_get_size(cma);

	return static_size;
}

static int codec_mm_cal_dump_buf_size(void)
{
	int size = PAGE_SIZE;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			size += 128 + PREFIX_MAX;
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	return codec_mm_align_up2n(size, PAGE_SHIFT);
}

int get_string_offset(int *buf_len, int *pos, int len)
{
	int log_line_step = len + PREFIX_MAX;
	int remain = LOG_LINE_MAX -
		 ((*pos + log_line_step) % LOG_LINE_MAX);

	*pos = (remain > 256) ?
		(*pos + log_line_step) :
		((((*pos + log_line_step) / LOG_LINE_MAX) + 1) * LOG_LINE_MAX);
	len = (remain > 256) ?
		(len) : (*pos - *buf_len);
	*buf_len += len;

	return len;
}

int get_string_segment(int size)
{
	return (size % LOG_LINE_MAX == 0) ?
		(size / LOG_LINE_MAX) :
		(size / LOG_LINE_MAX + 1);
}

void dump_mem_infos_external(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags = 0;

	pr_info("codec mem info:\n");

	pr_info("\ttotal codec mem size:%d MB\n",
			mgt->total_codec_mem_size / SZ_1M);

	pr_info("\talloced size= %d MB\n",
			mgt->total_alloced_size / SZ_1M);

	pr_info("\tmax alloced: %d MB\n",
			mgt->max_used_mem_size / SZ_1M);

	pr_info("\tCMA:%d,RES:%d,TVP:%d,SYS:%d,COHER:%d,VMAPED:%d MB\n",
		mgt->alloced_cma_size / SZ_1M,
		mgt->alloced_res_size / SZ_1M,
		mgt->tvp_pool.alloced_size / SZ_1M,
		mgt->alloced_sys_size / SZ_1M,
		mgt->alloced_from_coherent / SZ_1M,
		(mgt->phys_vmaped_page_cnt << PAGE_SHIFT) / SZ_1M);

#ifdef CONFIG_CODEC_MM_EXT_POOL
	pr_info("\tRES_EXT:%d MB\n",
		mgt->alloced_res_ext_size / SZ_1M);
#endif

	if (mgt->res_pool) {
		pr_info("\t[%d]RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED,
				(int)(gen_pool_size(mgt->res_pool) / SZ_1M),
				(int)(mgt->alloced_res_size / SZ_1M),
				(int)(gen_pool_avail(mgt->res_pool) / SZ_1M));
	}

	pr_info("\t[%d]CMA size:%d MB:alloced: %d MB,free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA,
			(int)(mgt->total_cma_size / SZ_1M),
			(int)(mgt->alloced_cma_size / SZ_1M),
			(int)((mgt->total_cma_size -
			mgt->alloced_cma_size) / SZ_1M));

	if (mgt->tvp_pool.slot_num > 0) {
		pr_info("\t[%d]TVP size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP,
				(int)(mgt->tvp_pool.total_size / SZ_1M),
				(int)(mgt->tvp_pool.alloced_size / SZ_1M),
				(int)((mgt->tvp_pool.total_size -
					mgt->tvp_pool.alloced_size) / SZ_1M));
	}

	if (mgt->cma_res_pool.slot_num > 0) {
		pr_info("\t[%d]CMA_RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES,
				(int)(mgt->cma_res_pool.total_size / SZ_1M),
				(int)(mgt->cma_res_pool.alloced_size / SZ_1M),
				(int)((mgt->cma_res_pool.total_size -
				mgt->cma_res_pool.alloced_size) / SZ_1M));
	}

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			pr_info("\t[%d].%d:%s.%d,addr=0x%lx,size=%d,from=%d,cnt=%d,flags=%d,used:%u ms",
				mem->mem_id,
				mem->ins_id,
				mem->owner[0] ? mem->owner[0] : "no",
				mem->ins_buffer_id,
				mem->phy_addr,
				mem->buffer_size,
				mem->from_flags,
				atomic_read(&mem->use_cnt),
				mem->flags, jiffies_to_msecs(get_jiffies_64() -
				mem->alloced_jiffies)
				);
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
}

void codec_mm_dbuf_walk_external(void)
{
	codec_mm_dbuf_walk(NULL);
}
EXPORT_SYMBOL(codec_mm_dbuf_walk_external);

static int dump_mem_infos(struct seq_file *m, char *buf)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags = 0;
	char *alloc_buf = NULL;
	int buf_size = 0;
	char *pbuf = NULL;
	int tsize = 0;
	int s = 0;
	int buf_len = 0;
	int segment_count = 0;
	int i = 0;

	buf_size = codec_mm_cal_dump_buf_size();
	alloc_buf = vzalloc(buf_size);
	pbuf = alloc_buf;

	s = snprintf(pbuf, buf_size - tsize,
			"codec mem info:\n");
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\ttotal codec mem size:%d MB\n",
			mgt->total_codec_mem_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\talloced size= %d MB\n",
			mgt->total_alloced_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
			"\tmax alloced: %d MB\n",
			mgt->max_used_mem_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

	s = snprintf(pbuf, buf_size - tsize,
		"\tCMA:%d,RES:%d,TVP:%d,SYS:%d,COHER:%d,VMAPED:%d MB\n",
		mgt->alloced_cma_size / SZ_1M,
		mgt->alloced_res_size / SZ_1M,
		mgt->tvp_pool.alloced_size / SZ_1M,
		mgt->alloced_sys_size / SZ_1M,
		mgt->alloced_from_coherent / SZ_1M,
		(mgt->phys_vmaped_page_cnt << PAGE_SHIFT) / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);

#ifdef CONFIG_CODEC_MM_EXT_POOL
	s = snprintf(pbuf, buf_size - tsize,
		"\tRES_EXT:%d MB\n",
		mgt->alloced_res_ext_size / SZ_1M);
	pbuf += get_string_offset(&buf_len, &tsize, s);
#endif

	if (mgt->res_pool) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_REVERSED,
				(int)(gen_pool_size(mgt->res_pool) / SZ_1M),
				(int)(mgt->alloced_res_size / SZ_1M),
				(int)(gen_pool_avail(mgt->res_pool) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	s = snprintf(pbuf, buf_size - tsize,
			"\t[%d]CMA size:%d MB:alloced: %d MB,free:%d MB\n",
			AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA,
			(int)(mgt->total_cma_size / SZ_1M),
			(int)(mgt->alloced_cma_size / SZ_1M),
			(int)((mgt->total_cma_size -
			mgt->alloced_cma_size) / SZ_1M));
	pbuf += get_string_offset(&buf_len, &tsize, s);

	if (mgt->tvp_pool.slot_num > 0) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]TVP size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_TVP,
				(int)(mgt->tvp_pool.total_size / SZ_1M),
				(int)(mgt->tvp_pool.alloced_size / SZ_1M),
				(int)((mgt->tvp_pool.total_size -
					mgt->tvp_pool.alloced_size) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	if (mgt->cma_res_pool.slot_num > 0) {
		s = snprintf(pbuf, buf_size - tsize,
				"\t[%d]CMA_RES size:%d MB,alloced:%d MB free:%d MB\n",
				AMPORTS_MEM_FLAGS_FROM_GET_FROM_CMA_RES,
				(int)(mgt->cma_res_pool.total_size / SZ_1M),
				(int)(mgt->cma_res_pool.alloced_size / SZ_1M),
				(int)((mgt->cma_res_pool.total_size -
				mgt->cma_res_pool.alloced_size) / SZ_1M));
		pbuf += get_string_offset(&buf_len, &tsize, s);
	}

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			s = snprintf(pbuf, buf_size - tsize,
				"\t[%d].%d:%s.%d,addr=0x%lx,size=%d,from=%d,cnt=%d,",
				mem->mem_id,
				mem->ins_id,
				mem->owner[0] ? mem->owner[0] : "no",
				mem->ins_buffer_id,
				mem->phy_addr,
				mem->buffer_size,
				mem->from_flags,
				atomic_read(&mem->use_cnt)
				);
			s += snprintf(pbuf + s, buf_size - tsize,
				"tvp_flag %d, alloc time %llu us,",
				!!(mem->flags & CODEC_MM_FLAGS_TVP),
				mem->end_alloc_time - mem->start_alloc_time);
			if (secure_mem_ctrl)
				s += snprintf(pbuf + s, buf_size - tsize,
					"tee alloc %llu us,",
					mem->tee_set_end_time - mem->tee_set_start_time);
			s += snprintf(pbuf + s, buf_size - tsize,
				"flags=%d,used:%u ms",
				mem->flags, jiffies_to_msecs(get_jiffies_64() -
				mem->alloced_jiffies));
			if (mem->dma_buf)
				s += snprintf(pbuf + s, buf_size - tsize,
					",ino:%ld,file_ref:%ld\n",
					file_inode(mem->dma_buf->file)->i_ino,
					file_count(mem->dma_buf->file));
			else
				s += snprintf(pbuf + s, buf_size - tsize,
					"\n");
			pbuf += get_string_offset(&buf_len, &tsize, s);

			if (tsize > buf_size - 256) {
				s = snprintf(pbuf, buf_size - tsize,
					"\n\t\t**NOT END**\n");
				tsize += s;
				break;/*no memory for dump now.*/
			}
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);

	segment_count = get_string_segment(tsize);
	for (i = 0; i < segment_count; i++)
		cs_printf(m, "%s", alloc_buf + i * LOG_LINE_MAX);

	if (buf) {
		if (buf_len > PAGE_SIZE - 1)
			buf_len = PAGE_SIZE - 1;

		strncpy(buf, alloc_buf, buf_len);
		buf[buf_len] = '\0';
	} else {
		buf_len = 0;
	}

	vfree(alloc_buf);
	return buf_len;
}

int codec_mm_alloc_cma_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->total_alloced_size / SZ_1M;
}
EXPORT_SYMBOL(codec_mm_alloc_cma_size);

static int dump_free_mem_infos(void *buf, int size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem;
	unsigned long flags;
	int i = 0, j, k;
	unsigned long minphy;
	u32 cma_base_phy, cma_end_phy;
	struct dump_buf_s {
		unsigned long phy_addr;
		int buffer_size;
		int align2n;
	} *usedb, *freeb;
	int total_free_size = 0;

	usedb = kzalloc(sizeof(*usedb) * 256, GFP_KERNEL);
	if (!usedb)
		return 0;
	freeb = usedb + 128;

	spin_lock_irqsave(&mgt->lock, flags);
	list_for_each_entry(mem, &mgt->mem_list, list) {
		usedb[i].phy_addr = mem->phy_addr;
		usedb[i].buffer_size = mem->buffer_size;
		usedb[i].align2n = mem->align2n;
		if (++i >= 128) {
			pr_info("too many memlist in codec_mm, break;\n");
			break;
		}
	};
	cma_base_phy = cma_get_base(dev_get_cma_area(mgt->dev));
	cma_end_phy = cma_base_phy + dma_get_cma_size_int_byte(mgt->dev);
	spin_unlock_irqrestore(&mgt->lock, flags);
	pr_info("free cma idea[%x-%x] from codec_mm items %d\n", cma_base_phy,
		cma_end_phy, i);
	for (j = 0; j < i; j++) { /* free */
		freeb[j].phy_addr = usedb[j].phy_addr +
			codec_mm_align_up2n(usedb[j].buffer_size, usedb[j].align2n);
		minphy = cma_end_phy;
		for (k = 0; k < i; k++) { /* used */
			if (usedb[k].phy_addr >= freeb[j].phy_addr &&
			    usedb[k].phy_addr < minphy)
				minphy = usedb[k].phy_addr;
		}
		freeb[j].buffer_size = minphy - freeb[j].phy_addr;
		total_free_size += freeb[j].buffer_size;
		if (freeb[j].buffer_size > 0)
			pr_info("\t[%d] free buf phyaddr=%p, used [%p,%x], size=%x\n",
				j, (void *)freeb[j].phy_addr,
				(void *)usedb[j].phy_addr,
				usedb[j].buffer_size, freeb[j].buffer_size);
	}
	pr_info("total_free_size %x\n", total_free_size);
	kfree(usedb);
	return 0;
}

void set_secure_controller_mode(struct codec_mm_mgt_s *mgt)
{
	u32 temp[2] = { 0 };
	int ret = of_property_read_u32_array(mgt->dev->of_node, "secure_ctrl_para",
		temp, ARRAY_SIZE(temp));

	if (ret)
		return;

	secure_mem_ctrl = temp[0];
	if (secure_mem_ctrl) {
		phys_addr_t cma_base = cma_get_base(dev_get_cma_area(mgt->dev));
		ulong cma_size = cma_get_size(dev_get_cma_area(mgt->dev));

		secure_mem_align2n = temp[1];
		tee_sectbl_mem_map(cma_base, cma_size, 1 << secure_mem_align2n, 0, 0, 0);

		pr_info("codec_mm secure_mem_align2n is %d\n", secure_mem_align2n);
	}
}

static int dump_tee_time_infos(void *buf, int size)
{
	char *pbuf = buf;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	char sbuf[512];
	int tsize = 0;
	int s;

	if (!secure_mem_ctrl)
		return 0;

	if (!pbuf)
		pbuf = sbuf;

#define BUFPRINT(args...) \
		do {\
			s = sprintf(pbuf, args);\
			tsize += s;\
			pbuf += s; \
		} while (0)

	BUFPRINT("\t tee interface cost time info:\n");
	BUFPRINT("\tcurrent time:%lld\n",
		 get_jiffies_64());
	BUFPRINT("\talloc time max us:%d\n",
		 mgt->tee_set_max_us);
	BUFPRINT("\talloc cnt:%d step:%d:%d:%d:%d:%d:%d:%d\n",
		 mgt->tee_set_cnt,
		 mgt->tee_set_10us_less_cnt,
		 mgt->tee_set_10_50us_cnt,
		 mgt->tee_set_50_100us_cnt,
		 mgt->tee_set_100_1000us_cnt,
		 mgt->tee_set_1_10ms_cnt,
		 mgt->tee_set_10_100ms_cnt,
		 mgt->tee_set_100ms_up_cnt);
	BUFPRINT("\tfree time max us:%d\n",
		 mgt->tee_clear_max_us);
	BUFPRINT("\tfree cnt:%d step:%d:%d:%d:%d:%d:%d:%d\n",
		 mgt->tee_clear_cnt,
		 mgt->tee_clear_10us_less_cnt,
		 mgt->tee_clear_10_50us_cnt,
		 mgt->tee_clear_50_100us_cnt,
		 mgt->tee_clear_100_1000us_cnt,
		 mgt->tee_clear_1_10ms_cnt,
		 mgt->tee_clear_10_100ms_cnt,
		 mgt->tee_clear_100ms_up_cnt);
	{
		int average_timeus;
		u64 divider = mgt->tee_set_total_us;

		do_div(divider, mgt->tee_set_cnt);
		average_timeus = (mgt->tee_set_cnt == 0 ?
			0 : (int)divider);
		BUFPRINT("\tset time average us:%d\n", average_timeus);
		divider = mgt->tee_clear_total_us;

		do_div(divider, mgt->tee_clear_cnt);
		average_timeus = (mgt->tee_clear_cnt == 0 ?
			0 : (int)divider);
		BUFPRINT("\tclear time average us:%d\n",	average_timeus);
	}

	if (!buf)
		pr_info("%s", sbuf);

	return tsize;
}

static void codec_mm_tvp_segment_init(void)
{
	u32 size = 0;
	u32 segment_size = 0;
	u32 rest_size = 0;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (secure_mem_ctrl)
		return;

	/*2M for audio not protect.*/
	if (tvp_dynamic_increase_disable) {
		if (default_tvp_4k_size <= 0 ||
			(tvp_dynamic_alloc_max_size > 0 &&
				default_tvp_4k_size >= tvp_dynamic_alloc_max_size)) {
			default_tvp_4k_size = mgt->total_codec_mem_size - SZ_1M * 2;
			if (default_tvp_4k_size > DEFAULT_TVP_SIZE_FOR_4K)
				default_tvp_4k_size = DEFAULT_TVP_SIZE_FOR_4K;
		}
		/*97MB -> 160MB, may not enough for h265*/
		default_tvp_size = (mgt->total_codec_mem_size - (SZ_1M * 2)) >
				DEFAULT_TVP_SIZE_FOR_NO4K ?
					DEFAULT_TVP_SIZE_FOR_NO4K :
					default_tvp_4k_size;
		return;
	}
	default_tvp_4k_size = mgt->total_codec_mem_size - SZ_1M * 2;
	default_tvp_size = default_tvp_4k_size;

	if (default_tvp_pool_size_0 > default_tvp_size) {
		default_tvp_pool_size_0 = default_tvp_size;
		default_tvp_pool_size_1 = 0;
		default_tvp_pool_size_2 = 0;
	}

	size = default_tvp_pool_size_0 +
		default_tvp_pool_size_1 + default_tvp_pool_size_2;
	if (size > default_tvp_size) {
		default_tvp_pool_size_0 = 0;
		default_tvp_pool_size_1 = 0;
		default_tvp_pool_size_2 = 0;
	}

	if (default_tvp_pool_size_0 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[0] =
			default_tvp_pool_size_0;
	} else {
		segment_size = default_tvp_size / 3;
		default_tvp_pool_segment_size[0] = segment_size;
	}
	size = default_tvp_pool_segment_size[0];
	if (default_tvp_pool_size_1 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[1] =
			default_tvp_pool_size_1;
	} else {
		segment_size = default_tvp_size / 4;
		rest_size = default_tvp_size - size;
		default_tvp_pool_segment_size[1] =
			(rest_size > segment_size) ? segment_size : rest_size;
	}
	size += default_tvp_pool_segment_size[1];
	if (default_tvp_pool_size_2 >= DEFAULT_TVP_SEGMENT_MIN_SIZE) {
		default_tvp_pool_segment_size[2] =
			default_tvp_pool_size_2;
	} else {
		segment_size = default_tvp_size / 4;
		rest_size = default_tvp_size - size;
		default_tvp_pool_segment_size[2] =
			(rest_size > segment_size) ? segment_size : rest_size;
	}
	size += default_tvp_pool_segment_size[2];
	rest_size = default_tvp_size - size;
	if (rest_size >= DEFAULT_TVP_SEGMENT_MIN_SIZE)
		default_tvp_pool_segment_size[3] = rest_size;
	else
		default_tvp_pool_segment_size[3] = 0;
	size += default_tvp_pool_segment_size[3];
	default_tvp_4k_size = size;
	default_tvp_size = size;
	tvp_dynamic_alloc_max_size = size;
}

int codec_mm_enable_tvp(int size, int flags)
{
	int ret;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (secure_mem_ctrl)
		return 0;

	mutex_lock(&mgt->tvp_protect_lock);
	if (mgt->tvp_pool.slot_num <= 0)
		codec_mm_tvp_segment_init();
	if (size == 0) {
		if (flags == 1)
			size = default_tvp_size;
		else
			size = default_tvp_4k_size;
	}
	if (tvp_dynamic_increase_disable) {
		ret = codec_mm_extpool_pool_alloc(&mgt->tvp_pool,
			size, 0, 1);
		if (ret) {
			ret = 0;
			mgt->tvp_enable = flags;
			if (tvp_mode > 0)
				atomic_add_return(1, &mgt->tvp_user_count);
			pr_info("enable tvp for %d\n", flags);
		} else {
			pr_info("tvp enable failed size %d\n", size);
			mutex_unlock(&mgt->tvp_protect_lock);
			return -1;
		}
	} else {
		if (mgt->tvp_pool.slot_num <= 0) {
			ret = codec_mm_tvp_pool_alloc_by_slot
				(&mgt->tvp_pool, 0, flags);
			if (ret) {
				ret = 0;
				if (mgt->tvp_enable != 2)
					mgt->tvp_enable = flags;
				if (tvp_mode > 0)
					atomic_add_return(1,
						&mgt->tvp_user_count);
				pr_info("enable tvp for %d\n", flags);
			} else {
				pr_info("tvp enable failed size %d\n",
					size);
				mutex_unlock(&mgt->tvp_protect_lock);
				return -1;
			}
		} else {
			ret = 0;
			if (mgt->tvp_enable != 2)
				mgt->tvp_enable = flags;
			if (tvp_mode > 0)
				atomic_add_return(1, &mgt->tvp_user_count);
			pr_info("enable tvp for %d\n", flags);
		}
	}
	if (tvp_mode > 0)
		pr_info("tvp_user_count is %d\n",
			atomic_read(&mgt->tvp_user_count));
	mutex_unlock(&mgt->tvp_protect_lock);
	return ret;
}
EXPORT_SYMBOL(codec_mm_enable_tvp);

int codec_mm_disable_tvp(void)
{
	int ret = 0;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (secure_mem_ctrl)
		return 0;

	mutex_lock(&mgt->tvp_protect_lock);
	if (tvp_mode == 0) {
		ret = codec_mm_extpool_pool_release(&mgt->tvp_pool);
		mgt->tvp_enable = 0;
		pr_info("disable tvp\n");
		mutex_unlock(&mgt->tvp_protect_lock);
		return ret;
	}

	if (atomic_dec_and_test(&mgt->tvp_user_count)) {
		if (codec_mm_tvp_pool_unprotect_and_release(&mgt->tvp_pool) == 0) {
			mgt->tvp_enable = 0;
			pr_info("disable tvp\n");
			mutex_unlock(&mgt->tvp_protect_lock);
			return ret;
		}
	}
	if (atomic_read(&mgt->tvp_user_count) < 0)
		atomic_set(&mgt->tvp_user_count, 0);
	pr_info("tvp_user_count is %d\n",
		atomic_read(&mgt->tvp_user_count));
	mutex_unlock(&mgt->tvp_protect_lock);
	return ret;
}
EXPORT_SYMBOL(codec_mm_disable_tvp);

int codec_mm_video_tvp_enabled(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->tvp_enable;
}
EXPORT_SYMBOL(codec_mm_video_tvp_enabled);

int codec_mm_get_total_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int total_size = mgt->total_codec_mem_size;

	if (!codec_dbg_level(CODEC_DBG_DISABLE_MODE)) {	/*no debug memory mode. */
		return total_size;
	}
	/*
	 *disable reserved:1
	 *disable cma:2
	 *disable sys memory:4
	 *disable half memory:8,
	 */
	if (codec_dbg_level(CODEC_DBG_DISABLE_HALF_MEM))
		total_size -= mgt->total_codec_mem_size / 2;
	if (codec_dbg_level(CODEC_DBG_DISABLE_RESV)) {
		total_size -= mgt->total_reserved_size;
		total_size -= mgt->cma_res_pool.total_size;
	}
	if (codec_dbg_level(CODEC_DBG_DISABLE_CMA))
		total_size -= mgt->total_cma_size;
	if (total_size < 0)
		total_size = 0;
	return total_size;
}
EXPORT_SYMBOL(codec_mm_get_total_size);

/*count remain size for no tvp*/
int codec_mm_get_free_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return codec_mm_get_total_size() -
		mgt->tvp_pool.total_size  + mgt->tvp_pool.alloced_size -
		mgt->total_alloced_size;
}
EXPORT_SYMBOL(codec_mm_get_free_size);

int codec_mm_get_tvp_free_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->tvp_pool.total_size -
		mgt->tvp_pool.alloced_size;
}

int codec_mm_get_reserved_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->total_reserved_size;
}
EXPORT_SYMBOL(codec_mm_get_reserved_size);

struct device *v4l_get_dev_from_codec_mm(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->dev;
}
EXPORT_SYMBOL(v4l_get_dev_from_codec_mm);

struct codec_mm_s *v4l_reqbufs_from_codec_mm(const char *owner,
						 unsigned int addr,
						 unsigned int size,
						 unsigned int index)
{
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	int buf_size = PAGE_ALIGN(size);

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (IS_ERR_OR_NULL(mem))
		goto out;

	mem->owner[0]	= owner;
	mem->mem_handle  = NULL;
	mem->buffer_size = buf_size;
	mem->page_count  = buf_size / PAGE_SIZE;
	mem->phy_addr	= addr;
	mem->ins_buffer_id = index;
	mem->alloced_jiffies = get_jiffies_64();
	mem->from_flags =
		AMPORTS_MEM_FLAGS_FROM_GET_FROM_COHERENT;

	spin_lock_irqsave(&mgt->lock, flags);

	mem->mem_id = mgt->global_memid++;
	mgt->alloced_from_coherent += buf_size;
	mgt->total_alloced_size += buf_size;
	mgt->alloced_cma_size += buf_size;
	list_add_tail(&mem->list, &mgt->mem_list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
			   "mem_id [%d] %s alloc coherent size %d at %lx from %d.\n",
			   mem->mem_id, owner, buf_size, mem->phy_addr, mem->from_flags);
out:
	return mem;
}
EXPORT_SYMBOL(v4l_reqbufs_from_codec_mm);

void v4l_freebufs_back_to_codec_mm(const char *owner, struct codec_mm_s *mem)
{
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (IS_ERR_OR_NULL(mem))
		return;

	if (!mem->owner[0] || strcmp(owner, mem->owner[0]) ||
		!mem->buffer_size)
		goto out;

	spin_lock_irqsave(&mgt->lock, flags);

	mgt->alloced_from_coherent -= mem->buffer_size;
	mgt->total_alloced_size -= mem->buffer_size;
	mgt->alloced_cma_size -= mem->buffer_size;
	list_del(&mem->list);

	spin_unlock_irqrestore(&mgt->lock, flags);

	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
			   "mem_id [%d] %s free mem size %d at %lx from %d\n", mem->mem_id,
			   mem->owner[0], mem->buffer_size, mem->phy_addr, mem->from_flags);
out:
	kfree(mem);
}
EXPORT_SYMBOL(v4l_freebufs_back_to_codec_mm);

/*
 *with_wait:
 *1: if no mem, do wait and free some cache.
 *0: do not wait.
 */
int codec_mm_enough_for_size(int size, int with_wait, int mem_flags)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int have_mem;
	bool tvp_flag = secure_mem_ctrl ? 0 : mem_flags & CODEC_MM_FLAGS_TVP;

	have_mem = codec_mm_alloc_pre_check_in(mgt, size, tvp_flag);
	if (!have_mem && with_wait && mgt->alloced_for_sc_cnt > 0) {
		pr_err(" No mem, clear scatter cache!!\n");
		//codec_mm_scatter_free_all_ignorecache(1);
		have_mem = codec_mm_alloc_pre_check_in(mgt, size, tvp_flag);
		if (have_mem)
			return 1;
		if (codec_dbg_level(CODEC_DBG_DUMP_INFO))
			dump_mem_infos(NULL, NULL);
		msleep(50);
		return 0;
	}
	return 1;
}
EXPORT_SYMBOL(codec_mm_enough_for_size);

int codec_mm_mgt_init(struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	int ret;

	INIT_LIST_HEAD(&mgt->mem_list);
	mgt->dev = dev;
	mgt->alloc_from_sys_pages_max = 0;
	if (mgt->rmem.size > 0) {
		unsigned long aligned_addr;
		int aligned_size;
		/*mem aligned, */
		mgt->res_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!mgt->res_pool)
			return -ENOMEM;
		aligned_addr = (unsigned long)mgt->rmem.base;
		aligned_size = mgt->rmem.size;
		ret = gen_pool_add(mgt->res_pool,
				 aligned_addr, aligned_size, -1);
		if (ret < 0) {
			gen_pool_destroy(mgt->res_pool);
			return -1;
		}
		pr_debug("add reserve memory %p(aligned %p) size=%x(aligned %x)\n",
			 (void *)mgt->rmem.base, (void *)aligned_addr,
			 (int)mgt->rmem.size, (int)aligned_size);
		mgt->total_reserved_size = aligned_size;
		mgt->total_codec_mem_size = aligned_size;
#ifdef RES_IS_MAPED
		mgt->res_mem_flags |= RES_MEM_FLAGS_HAVE_MAPED;
#endif
	}

#ifdef CONFIG_CODEC_MM_EXT_POOL
	if (mgt->rmem_ext.size > 0) {
		unsigned long aligned_addr;
		int aligned_size;
		/*mem aligned, */
		mgt->res_ext_pool = gen_pool_create(PAGE_SHIFT, -1);
		if (!mgt->res_ext_pool)
			return -ENOMEM;
		aligned_addr = (unsigned long)mgt->rmem_ext.base;
		aligned_size = mgt->rmem_ext.size -
			(int)(aligned_addr - (unsigned long)mgt->rmem_ext.base);
		ret = gen_pool_add(mgt->res_ext_pool,
				 aligned_addr, aligned_size, -1);
		if (ret < 0) {
			gen_pool_destroy(mgt->res_ext_pool);
			return -1;
		}
		pr_debug("add reserve ext memory %p(aligned %p) size=%x(aligned %x)\n",
			 (void *)mgt->rmem_ext.base, (void *)aligned_addr,
			 (int)mgt->rmem_ext.size, (int)aligned_size);
		mgt->total_reserved_ext_size = aligned_size;
		mgt->support_res_ext = true;
	}
#endif

	mgt->total_cma_size = codec_mm_get_cma_size_int_byte(mgt->dev);
	mgt->total_codec_mem_size += mgt->total_cma_size;
	if (get_meson_cpu_version(MESON_CPU_VERSION_LVL_MAJOR) <
		MESON_CPU_MAJOR_ID_G12A)
		tvp_dynamic_increase_disable = 1;
	default_tvp_4k_size = 0;
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
	set_secure_controller_mode(mgt);
#endif
	codec_mm_tvp_segment_init();
	default_cma_res_size = mgt->total_cma_size;
	mgt->global_memid = 0;
	mgt->global_local_id = 0;
	mutex_init(&mgt->tvp_pool.pool_lock);
	mutex_init(&mgt->cma_res_pool.pool_lock);
	spin_lock_init(&mgt->lock);
	INIT_WORK(&mgt->tvp_alloc_wrk, codec_mm_tvp_alloc_monitor);

	mgt->cma = dev_get_cma_area(mgt->dev);
	codec_mm_compaction_init((void *)mgt->cma);

	return 0;
}
EXPORT_SYMBOL(codec_mm_mgt_init);

void codec_mm_get_default_tvp_size(int *tvp_fhd, int *tvp_uhd)
{
	if (tvp_dynamic_increase_disable) {
		if (tvp_fhd)
			*tvp_fhd = default_tvp_size;
		if (tvp_uhd)
			*tvp_uhd = default_tvp_4k_size;
	} else {
		if (tvp_fhd) {
			*tvp_fhd = default_tvp_size <
				DEFAULT_TVP_SIZE_FOR_NO4K ?
				default_tvp_size : DEFAULT_TVP_SIZE_FOR_NO4K;
		}
		if (tvp_uhd) {
			*tvp_uhd = default_tvp_4k_size <
				DEFAULT_TVP_SIZE_FOR_4K ?
				default_tvp_4k_size : DEFAULT_TVP_SIZE_FOR_4K;
		}
	}
}
EXPORT_SYMBOL(codec_mm_get_default_tvp_size);

static int __init amstream_test_init(void)
{
	return 0;
}

static ssize_t codec_mm_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return (ssize_t)dump_mem_infos(NULL, buf);
}

static ssize_t codec_mm_scatter_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	size_t ret;

	ret = codec_mm_scatter_info_dump(buf, PAGE_SIZE);
	return ret;
}

static ssize_t codec_mm_keeper_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	size_t ret;

	ret = codec_mm_keeper_dump_info(buf, PAGE_SIZE);
	return ret;
}

static ssize_t tvp_enable_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;

	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	mutex_lock(&mgt->tvp_protect_lock);
	size += sprintf(buf, "tvp_flag=%d\n",
			(tvp_mode << 4) + mgt->tvp_enable);
	size += sprintf(buf + size, "tvp ref count=%d\n",
			atomic_read(&mgt->tvp_user_count));
	size += sprintf(buf + size, "tvp enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable tvp(tvp size to 0)\n");
	size += sprintf(buf + size,
		"1: enable tvp for 1080p playing(use default size)\n");
	size += sprintf(buf + size,
		"2: enable tvp for 4k playing(use default 4k size)\n");
	mutex_unlock(&mgt->tvp_protect_lock);
	return size;
}

static ssize_t tvp_enable_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	/*
	 * always free all scatter cache and unused keeper for
	 * tvp changes when tvp mode is 0.
	 */
	if (tvp_mode < 1) {
		mutex_lock(&mgt->tvp_protect_lock);
		codec_mm_keeper_free_all_keep(2);
		codec_mm_scatter_free_all_ignorecache(3);
		mutex_unlock(&mgt->tvp_protect_lock);
	}
	switch (val) {
	case 0:
		codec_mm_disable_tvp();
		break;
	case 1:
		ret = codec_mm_enable_tvp(default_tvp_size, val);
		if (ret) {
			pr_info("tvp enable failed tvp mode %d %d %d\n",
				tvp_mode, val, default_tvp_size);
			return -1;
		}
		break;
	case 2:
		ret = codec_mm_enable_tvp(default_tvp_4k_size, val);
		if (ret) {
			pr_info("tvp enable failed tvp mode %d %d %d\n",
				tvp_mode, val, default_tvp_4k_size);
			return -1;
		}
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
		break;
	}
	return size;
}

static ssize_t fastplay_enable_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "fastplay enable help:\n");
	size += sprintf(buf + size, "echo n > tvp_enable\n");
	size += sprintf(buf + size, "0: disable fastplay\n");
	size += sprintf(buf + size,
		"1: enable fastplay for 1080p playing(use default size)\n");
	return size;
}

static ssize_t fastplay_enable_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned int val;
	ssize_t ret;

	val = -1;
	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;
	switch (val) {
	case 0:
		ret = codec_mm_extpool_pool_release(&mgt->cma_res_pool);
		mgt->fastplay_enable = 0;
		pr_err("disable fastplay ret 0x%zx\n", ret);
		break;
	case 1:
		codec_mm_extpool_pool_alloc(&mgt->cma_res_pool,
						default_cma_res_size, 0, 0);
		mgt->fastplay_enable = 1;
		pr_err("enable fastplay\n");
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;
}

static ssize_t config_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t ret;

	ret = configs_list_path_nodes(CONFIG_PATH, buf, PAGE_SIZE,
					  LIST_MODE_NODE_CMDVAL_ALL);
	return ret;
}

static ssize_t config_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	int ret;

	ret = configs_set_prefix_path_valonpath(CONFIG_PREFIX, buf);
	if (ret < 0)
		pr_err("set config failed %s\n", buf);
	return size;
}

static ssize_t tvp_region_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	size_t ret;
	ulong res_victor[16];
	int i;
	int off = 0;

	ret = codec_mm_tvp_get_mem_resource(res_victor, 8);
	for (i = 0; i < ret; i++) {
		off += sprintf(buf + off,
			"segment%d:0x%lx - 0x%lx (size:0x%x)\n",
			i,
			res_victor[2 * i],
			res_victor[2 * i + 1],
			(int)(res_victor[2 * i + 1] - res_victor[2 * i] + 1));
	}
	return off;
}

static ssize_t debug_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "mm_scatter help:\n");
	size += sprintf(buf + size, "echo n > mm_scatter_debug\n");
	size += sprintf(buf + size, "n==0: clear all debugs)\n");
	size += sprintf(buf + size,
	"n=1: dump all alloced scatters\n");
	size += sprintf(buf + size,
	"n=2: dump all slots\n");

	size += sprintf(buf + size,
	"n=3: dump all free slots\n");

	size += sprintf(buf + size,
	"n=4: dump all sid hash table\n");

	size += sprintf(buf + size,
	"n=5: free all free slot now!\n");

	size += sprintf(buf + size,
	"n=6: clear all time infos!\n");

	size += sprintf(buf + size,
	"n=10: force free all keeper\n");

	size += sprintf(buf + size,
	"n=20: dump memory,# 20 #addr(hex) #len\n");

	size += sprintf(buf + size,
	"n==100: cmd mode p1 p ##mode:0,dump, 1,alloc 2,more,3,free some,4,free all\n");

	size += sprintf(buf + size,
	"n=200: walk dmabuf infos.\n");

	return size;
}

static int codec_mm_mem_dump(unsigned long addr, int isphy, int len)
{
	void *vaddr;
	int is_map = 0;

	pr_info("start dump addr: %p %d\n", (void *)addr, len);
	if (!isphy) {
		vaddr = (void *)addr;
	} else {
		vaddr = ioremap(addr, len);
		if (!vaddr) {
			pr_info("map addr: %p len: %d, failed\n",
				(void *)addr, len);
			vaddr = codec_mm_phys_to_virt(addr);
		} else {
			is_map = 1;
		}
	}
	if (vaddr) {
		unsigned int *p, *vint;
		int i;

		vint = (unsigned int *)vaddr;
		for (i = 0; i <= len - 32; i += sizeof(int) * 8) {
			p = (int *)&vint[i];
			pr_info("%p: %08x %08x %08x %08x %08x %08x %08x %08x\n",
				p,
				p[0], p[1], p[2], p[3],
				p[4], p[5], p[6], p[7]);
		}
	}
	if (vaddr && is_map) {
		/*maped free...*/
		iounmap(vaddr);
	}
	return 0;
}

#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
static void dump_tee_sectbl_info(void)
{
	u32 ret;
	struct tee_sectbl_info tbl_info = { 0 };

	if (!secure_mem_ctrl)
		return;

	ret = tee_sectbl_get_info(&tbl_info);
	if (ret) {
		pr_info("get tee_sectbl info fail ret %u\n", ret);
		return;
	}

	pr_info("codec_mm tee_info addr 0x%llx size 0x%llx blk_size 0x%x secure_bits %u sectbl_crc 0x%x\n",
		tbl_info.tbl0_sta, tbl_info.tbl0_size, tbl_info.tbl0_blk_size,
		tbl_info.secure_bits, tbl_info.sectbl_crc);
}
#endif

static ssize_t debug_store(const struct class *class,
		const struct class_attribute *attr,
		const char *buf, size_t size)
{
	unsigned int val, tmp;
	ssize_t ret;

	val = -1;
	ret = sscanf(buf, "%d %d", &val, &tmp);
	if (ret == 0)
		return -EINVAL;
	switch (val) {
	case 0:
		pr_info("clear debug!\n");
		break;
	case 1:
		codec_mm_dump_all_scatters();
		break;
	case 2:
		codec_mm_dump_all_slots();
		break;
	case 3:
		codec_mm_dump_free_slots();
		break;
	case 4:
		codec_mm_dump_all_hash_table();
		break;
	case 5:
		codec_mm_free_all_free_slots();
		break;
	case 6:
		codec_mm_clear_alloc_infos();
		break;
	case 7:
		codec_mm_scatter_owner_dump();
		break;
	case 10:
		codec_mm_keeper_free_all_keep(1);
		break;
	case 11:
		dump_mem_infos(NULL, NULL);
		break;
	case 12:
		dump_free_mem_infos(NULL, 0);
		break;
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
	case 13:
		dump_tee_sectbl_info();
		break;
#endif
	case 14:
		dump_tee_time_infos(NULL, 0);
		break;
	case 20: {
		int cmd = 0, len = 0;
		unsigned int addr;

		addr = 0;
		ret = sscanf(buf, "%d %x %d", &cmd, &addr, &len);
		if (addr > 0 && len > 0)
			codec_mm_mem_dump(addr, 1, len);
		}
		break;
	case 100:{
			int cmd, mode = 0, p1 = 0, p2 = 0;

			ret = sscanf(buf, "%d %d %d %d", &cmd, &mode, &p1, &p2);
			codec_mm_scatter_test(mode, p1, p2);
		}
		break;
	case 200:
		codec_mm_dbuf_walk(NULL);
		break;
	case 201:
		codec_mm_show_dma_buf();
		break;
	case 900: {
		uint cmd, buffer_page_num, buffer_num, interval_ms, times;
		uint wait_size, tvp_mode;

		ret = sscanf(buf, "%d %d %d %d %d %d %d",
				&cmd, &times, &buffer_page_num, &buffer_num,
				&interval_ms, &wait_size, &tvp_mode);
		pr_info("buf num:%d, page num:%d, interval_ms:%d, times:%d, wait size:%d, tvp:%d\n",
				buffer_num, buffer_page_num, interval_ms,
				times, wait_size, tvp_mode);
		codec_mm_scatter_simu_thread_test(times, buffer_page_num, buffer_num,
				interval_ms, wait_size, tvp_mode);
		}
		break;
	default:
		pr_err("unknown cmd! %d\n", val);
	}
	return size;

}

static ssize_t debug_keep_mode_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	ssize_t size = 0;

	size = sprintf(buf, "%u\n", debug_keep_mode);

	return size;
}

static ssize_t debug_keep_mode_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	unsigned int val;
	ssize_t ret;

	val = -1;
	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	debug_keep_mode = val;

	return size;
}

bool codec_mm_scatter_available_check(int alloc)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	/* if codec_mm_scatter_watermark has been configed, use scatter
	 * free size to check, else use reserved size to check
	 */
	if (mgt->codec_mm_scatter_watermark) {
		if (mgt->codec_mm_scatter_free_size > alloc)
			return true;
	} else {
		if (codec_mm_get_free_size() >
			codec_mm_scatter_get_reserved_size())
			return true;
	}
	codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO,
			"[%s]%s not enough. (watermark:%d/%d)\n", __func__,
			mgt->codec_mm_for_linear ? "scatter watermark" : "reserved",
			mgt->codec_mm_scatter_free_size,
			mgt->codec_mm_scatter_watermark);
	return false;
}

void codec_mm_scatter_level_decrease(int alloc_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;

	if (mgt->codec_mm_for_linear) {
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_free_size -= alloc_size;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
	}
}

void codec_mm_scatter_level_increase(int free_size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	unsigned long flags;

	if (mgt->codec_mm_for_linear) {
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_free_size += free_size;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
	}
}

void codec_mm_set_min_linear_size(int min_mem_val)
{
	int val = min_mem_val;
	unsigned long flags;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	if (val <= 0)
		return;

	if (val > codec_mm_get_total_size()) {
		pr_err("[%s]set value %d is too large, set as %d\n",
				__func__, val, codec_mm_get_total_size());
		val = codec_mm_get_total_size();
	}

	if (!mgt->codec_mm_for_linear) { // first time set
		spin_lock_init(&mgt->scatter_watermark_lock);
		mgt->codec_mm_for_linear = val;
		mgt->codec_mm_scatter_watermark =
			codec_mm_get_total_size() - mgt->codec_mm_for_linear;
		mgt->codec_mm_scatter_free_size =
			mgt->codec_mm_scatter_watermark;
	} else { // has been set, just need to make adjustment
		spin_lock_irqsave(&mgt->scatter_watermark_lock, flags);
		mgt->codec_mm_scatter_watermark += (mgt->codec_mm_for_linear - val);
		mgt->codec_mm_scatter_free_size += (mgt->codec_mm_for_linear - val);
		mgt->codec_mm_for_linear = val;
		spin_unlock_irqrestore(&mgt->scatter_watermark_lock, flags);
		if (mgt->codec_mm_scatter_free_size < 0)
			pr_warn_once("[%s]adjust watermark, scatter need free %d cma\n",
				__func__, mgt->codec_mm_scatter_free_size);
	}

	codec_pr_dbg(CODEC_DBG_MEM_EXT_INFO,
				"[%s]linear:%d, scatter:%d/%d\n",
				__func__, mgt->codec_mm_for_linear,
				mgt->codec_mm_scatter_free_size,
				mgt->codec_mm_scatter_watermark);
}

int codec_mm_get_min_linear_size(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->codec_mm_for_linear;
}

int codec_mm_get_scatter_watermark(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	return mgt->codec_mm_scatter_watermark;
}

static ssize_t dbuf_trace_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	char *pbuf = buf;

	pbuf += sprintf(pbuf, "Dmabuf tracking [%s]\n",
		mgt->trk_h ? "enable" : "disable");

	return pbuf - buf;
}

static ssize_t dbuf_trace_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	bool val = false;
	ssize_t ret;

	ret = kstrtobool(buf, &val);
	if (ret != 0)
		return -EINVAL;

	if (val) {
		//TODO.
	} else {
		mgt->trk_h = NULL;
	}

	return size;
}

static ssize_t dbuf_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	codec_mm_dbuf_walk(NULL);

	return 0;
}

static ssize_t dbuf_dump_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t size)
{
	u32 val = UINT_MAX;
	char *pval = NULL;
	ssize_t ret;

	ret = kstrtouint(buf, 0, &val);
	if (ret) {
		pval = strchr(buf, ' ');
		if (pval)
			ret = kstrtouint(strim(pval), 0, &val);
	}

	switch (buf[0]) {
	case 'a':
	{
		codec_mm_dbuf_dump_config(val);
		break;
	}
	case 'd':
	{
		if (val)
			codec_mm_sampling_open();
		else
			codec_mm_sampling_close();
		break;
	}
	case 'h':
		codec_mm_dbuf_dump_help();
		break;
	default:
		codec_mm_dbuf_dump_config(val);
	}

	return size;
}

static ssize_t codec_mm_compact_show(const struct class *class,
	const struct class_attribute *attr,
	char *buf)
{
	ssize_t size = 0;

	size += sprintf(buf, "n: n times compact\n");
	size += sprintf(buf + size, "-1: persistent compact\n");
	size += sprintf(buf + size, "0: disable compact\n");

	return size;
}

static ssize_t codec_mm_compact_store(const struct class *class,
	const struct class_attribute *attr,
	const char *buf, size_t size)
{
	int val;
	ssize_t ret;

	val = 0;

	ret = kstrtoint(buf, 0, &val);
	if (ret != 0)
		return -EINVAL;

	if (!val)
		codec_mm_compaction_stop();
	else
		codec_mm_compaction_start(val);

	return size;
}

static CLASS_ATTR_RO(codec_mm_dump);
static CLASS_ATTR_RO(codec_mm_scatter_dump);
static CLASS_ATTR_RO(codec_mm_keeper_dump);
static CLASS_ATTR_RO(tvp_region);
static CLASS_ATTR_RW(tvp_enable);
static CLASS_ATTR_RW(fastplay_enable);
static CLASS_ATTR_RW(config);
static CLASS_ATTR_RW(debug);
//static CLASS_ATTR_RW(debug_mode);
static CLASS_ATTR_RW(debug_keep_mode);
static CLASS_ATTR_RW(dbuf_trace);
static CLASS_ATTR_RW(dbuf_dump);
static CLASS_ATTR_RW(codec_mm_compact);

static struct attribute *codec_mm_class_attrs[] = {
	&class_attr_codec_mm_dump.attr,
	&class_attr_codec_mm_scatter_dump.attr,
	&class_attr_codec_mm_keeper_dump.attr,
	&class_attr_tvp_region.attr,
	&class_attr_tvp_enable.attr,
	&class_attr_fastplay_enable.attr,
	&class_attr_config.attr,
	&class_attr_debug.attr,
	//&class_attr_debug_mode.attr,
	&class_attr_debug_keep_mode.attr,
	&class_attr_dbuf_trace.attr,
	&class_attr_dbuf_dump.attr,
	&class_attr_codec_mm_compact.attr,
	NULL
};

ATTRIBUTE_GROUPS(codec_mm_class);

static struct class codec_mm_class = {
	.name = "codec_mm",
	.class_groups = codec_mm_class_groups,
};

static struct mconfig codec_mm_configs[] = {
	MC_PI32("default_tvp_size", &default_tvp_size),
	MC_PI32("default_tvp_4k_size", &default_tvp_4k_size),
	MC_PI32("default_cma_res_size", &default_cma_res_size),
	MC_PI32("default_tvp_pool_size_0", &default_tvp_pool_size_0),
	MC_PI32("default_tvp_pool_size_1", &default_tvp_pool_size_1),
	MC_PI32("default_tvp_pool_size_2", &default_tvp_pool_size_2),
	MC_PI32("tvp_pool_segment_maxsize_0", &tvp_pool_segment_maxsize_0),
	MC_PI32("tvp_mode", &tvp_mode),
	MC_PI32("tvp_dynamic_increase_disable",
			&tvp_dynamic_increase_disable),
	MC_PI32("tvp_pool_early_release_switch",
			&tvp_pool_early_release_switch)
};

static struct mconfig_node codec_mm_trigger_node;

int codec_mm_trigger_fun(const char *trigger, int id, const char *buf, int size)
{
	int ret = size;

	switch (trigger[0]) {
	case 't':
		tvp_enable_store(NULL, NULL, buf, size);
		break;
	case 'f':
		fastplay_enable_store(NULL, NULL, buf, size);
		break;
	case 'd':
		debug_store(NULL, NULL, buf, size);
		break;
	default:
		ret = -1;
	}
	return size;
}

int codec_mm_trigger_help_fun(const char *trigger, int id, char *sbuf, int size)
{
	int ret = -1;
	void *buf, *getbuf = NULL;

	if (size < PAGE_SIZE) {
		getbuf = (void *)__get_free_page(GFP_KERNEL);

		if (!getbuf)
			return -ENOMEM;
		buf = getbuf;
	} else {
		buf = sbuf;
	}
	switch (trigger[0]) {
	case 't':
		ret = tvp_enable_show(NULL, NULL, buf);
		break;
	case 'f':
		ret = fastplay_enable_show(NULL, NULL, buf);
		break;
	case 'd':
		ret = debug_show(NULL, NULL, buf);
		break;
	default:
		pr_err("unknown trigger:[%s]\n", trigger);
		ret = -1;
	}
	if (ret > 0 && getbuf) {
		ret = min_t(int, ret, size);

		strncpy(sbuf, buf, ret);
	}
	if (getbuf)
		free_page((unsigned long)getbuf);
	return ret;
}

static struct mconfig codec_mm_trigger[] = {
	MC_FUN("tvp_enable", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
	MC_FUN("fastplay", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
	MC_FUN("debug", codec_mm_trigger_help_fun, codec_mm_trigger_fun),
};

int codec_mm_cs_show(struct seq_file *m, struct codec_state_node *cs)
{
	char *buf = (void *)__get_free_page(GFP_KERNEL);
	int r = 0;

	seq_printf(m, "\n #### Show %s status ####\n", cs->ops->name);

	seq_puts(m, "\n **** Dump linear buffer alloc status ****\n");
	dump_mem_infos(m, NULL);

	if (buf) {
		seq_puts(m, "\n **** Dump scatter buffer alloc status ****\n");
		memset(buf, 0, PAGE_SIZE);
		codec_mm_scatter_info_dump(buf, PAGE_SIZE);
		seq_printf(m, "%s", buf);

		seq_puts(m, "\n **** Dump codec mm config list ****\n");
		memset(buf, 0, PAGE_SIZE);
		r = configs_list_path_nodes(CONFIG_PATH,
			buf,
			PAGE_SIZE,
			LIST_MODE_NODE_CMDVAL_ALL);
		seq_printf(m, "%s", buf);

		free_page((ulong)buf);
	}

	return 0;
}

CODEC_STATE_RO(codec_mm);

/* aml_media is ko can't use cma_mmu_op() func */
#if IS_MODULE(CONFIG_AMLOGIC_MEDIA_MODULE) && \
	!IS_ENABLED(CONFIG_DEBUG_SPINLOCK)
#ifdef CONFIG_ARM64
#include <linux/proc_fs.h>

struct mm_struct codec_init_mm = {
	.mm_mt		= MTREE_INIT_EXT(mm_mt, MM_MT_FLAGS, codec_init_mm.mmap_lock),
	.mm_users	= ATOMIC_INIT(2),
	.mm_count	= ATOMIC_INIT(1),
	.write_protect_seq = SEQCNT_ZERO(codec_init_mm.write_protect_seq),
	MMAP_LOCK_INITIALIZER(codec_init_mm)
	.page_table_lock =  __SPIN_LOCK_UNLOCKED(codec_init_mm.page_table_lock),
	.arg_lock	=  __SPIN_LOCK_UNLOCKED(codec_init_mm.arg_lock),
	.mmlist		= LIST_HEAD_INIT(codec_init_mm.mmlist),
#ifdef CONFIG_PER_VMA_LOCK
	.vma_writer_wait = __RCUWAIT_INITIALIZER(codec_init_mm.vma_writer_wait),
	.mm_lock_seq	= SEQCNT_ZERO(codec_init_mm.mm_lock_seq),
#endif
	.cpu_bitmap	= CPU_BITS_NONE,
};

static struct mm_struct *aml_init_mm;
static pgd_t *faked_pgd;

void (*aml_mte_sync_tags)(pte_t pte, unsigned int nr_pages);
pte_t * (*aml__pte_offset_map)(pmd_t *pmd, unsigned long addr, pmd_t *pmdvalp);

static void *get_symbol_addr(const char *symbol_name)
{
	struct kprobe kp = {
		.symbol_name = symbol_name,
	};
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe:%s failed, returned %d\n", symbol_name, ret);
		return NULL;
	}
	pr_debug("symbol_name:%s addr=%px\n", symbol_name, kp.addr);
	unregister_kprobe(&kp);

	return kp.addr;
}

static int get_faked_init_mm(void)
{
	unsigned long long ttbr1_el1;
	unsigned long long swapper_pg_dir_pa;

	ttbr1_el1 = read_sysreg(ttbr1_el1);
	swapper_pg_dir_pa = ttbr1_el1 & 0xfffffffff000ULL;
	faked_pgd = phys_to_virt(swapper_pg_dir_pa);

	codec_init_mm.pgd = faked_pgd;
	aml_init_mm = &codec_init_mm;

	pr_info("ttbr1=%llx faked_init_mm=%px\n", ttbr1_el1, aml_init_mm);

	return 0;
}

static int tvp_clear_cma_pagemap(unsigned long pfn, unsigned long count)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	unsigned long addr, end;

	addr = (unsigned long)pfn_to_kaddr(pfn);
	end  = addr + count * PAGE_SIZE;
	for (; addr < end; addr += PMD_SIZE) {
		pgd = pgd_offset_pgd(faked_pgd, addr);
		if (pgd_none(*pgd) || pgd_bad(*pgd))
			break;

		p4d = p4d_offset(pgd, addr);
		if (p4d_none(*p4d) || p4d_bad(*p4d))
			break;

		pud = pud_offset(p4d, addr);
		if (pud_none(*pud) || pud_bad(*pud))
			break;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd))
			break;

		pr_debug("%s, addr:%lx, pgd:%p %llx, pmd:%p %llx\n",
			 __func__, addr, pgd,
			 pgd_val(*pgd), pmd, pmd_val(*pmd));
		pmd_clear(pmd);
	}

	return 0;
}

static void aml_set_pte_at(struct mm_struct *mm, unsigned long addr,
			      pte_t *ptep, pte_t pte)
{
	/*
	 * if (pte_present(pte) && pte_user_exec(pte) && !pte_special(pte))
	 *	__sync_icache_dcache(pte);
	 */

	/*
	 * If the PTE would provide user space access to the tags associated
	 * with it then ensure that the MTE tags are synchronised.  Although
	 * pte_access_permitted() returns false for exec only mappings, they
	 * don't expose tags (instruction fetches don't check tags).
	 */
	if (system_supports_mte() && pte_access_permitted(pte, false) &&
	    !pte_special(pte)) {
		pte_t old_pte = READ_ONCE(*ptep);
		/*
		 * We only need to synchronise if the new PTE has tags enabled
		 * or if swapping in (in which case another mapping may have
		 * set tags in the past even if this PTE isn't tagged).
		 * (!pte_none() && !pte_present()) is an open coded version of
		 * is_swap_pte()
		 */
		if (pte_tagged(pte) || (!pte_none(old_pte) && !pte_present(old_pte)))
			aml_mte_sync_tags(old_pte, 1);
	}

	__check_safe_pte_update(mm, ptep, pte);

	set_pte(ptep, pte);
}

/* Return a pointer to the bitmap storing bits affecting a block of pages */
static inline unsigned long *get_pageblock_bitmap(const struct page *page,
							unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(page)->pageblock_flags;
#endif /* CONFIG_SPARSEMEM */
}

static inline unsigned long pfn_to_bitidx(const struct page *page, unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION - 1);
#else
	pfn = pfn - pageblock_start_pfn(page_zone(page)->zone_start_pfn);
#endif /* CONFIG_SPARSEMEM */
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
}

static unsigned long aml_get_pfnblock_flags_mask(const struct page *page,
					unsigned long pfn, unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG - 1);
	/*
	 * This races, without locks, with set_pfnblock_flags_mask(). Ensure
	 * a consistent read of the memory array, so that results, even though
	 * racy, are not corrupted.
	 */
	word = READ_ONCE(bitmap[word_bitidx]);
	return (word >> bitidx) & mask;
}

static bool is_cma_page(struct page *page)
{
	int migrate_type = 0;

	if (!page)
		return false;
	migrate_type = aml_get_pfnblock_flags_mask(page, page_to_pfn(page), MIGRATETYPE_MASK);
	if (is_migrate_cma(migrate_type) ||
	    is_migrate_isolate(migrate_type)) {
		return true;
	}
	return false;
}

int cma_mmu_op(struct page *page, int count, bool set)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	unsigned long addr, end;
	struct mm_struct *mm;
	struct cma *cma;

	if (!page || PageHighMem(page))
		return -EINVAL;

	/* TODO: owner must make sure this cma pool have called
	 * setup_cma_full_pagemap before call this function
	 */
	if (!is_cma_page(page)) {
		pr_debug("%s, page:%lx is not cma or no clear-map, cma:%px\n",
			 __func__, page_to_pfn(page), cma);
		return -EINVAL;
	}

	if (!aml_init_mm || !aml_mte_sync_tags || !aml__pte_offset_map) {
		pr_err("%s, no cma mmu operation. %px-%px-%px\n", __func__,
				aml_init_mm, aml_mte_sync_tags, aml__pte_offset_map);
		return -EINVAL;
	}

	addr = (unsigned long)page_address(page);
	end  = addr + count * PAGE_SIZE;
	mm = aml_init_mm;
	for (; addr < end; addr += PAGE_SIZE) {
		pgd = pgd_offset(mm, addr);
		if (pgd_none(*pgd) || pgd_bad(*pgd))
			break;

		p4d = p4d_offset(pgd, addr);
		if (p4d_none(*p4d) || p4d_bad(*p4d))
			break;

		pud = pud_offset(p4d, addr);
		if (pud_none(*pud) || pud_bad(*pud))
			break;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd))
			break;

		pte = aml__pte_offset_map(pmd, addr, NULL);
		if (set)
			aml_set_pte_at(mm, addr, pte, mk_pte(page, pgprot_tagged(PAGE_KERNEL)));
		else
			pte_clear(mm, addr, pte);
		pte_unmap(pte);
	#ifdef CONFIG_ARM
		pr_debug("%s, add:%lx, pgd:%p %x, pmd:%p %x, pte:%p %x\n",
			 __func__, addr, pgd, (int)pgd_val(*pgd),
			 pmd, (int)pmd_val(*pmd), pte, (int)pte_val(*pte));
	#elif defined(CONFIG_ARM64)
		pr_debug("%s, add:%lx, pgd:%p %llx, pmd:%p %llx, pte:%p %llx\n",
			 __func__, addr, pgd, pgd_val(*pgd),
			 pmd, pmd_val(*pmd), pte, pte_val(*pte));
	#endif
		page++;
	}
	codec_pr_dbg(CODEC_DBG_TRACE_ALLOC_FREE,
		"[%s]%s pte %d pages\n", __func__,
		set ? "set" : "clear", count);
	return 0;
}

#else
/*
 * highmem no mapping, so limitting codec_mm_cma
 * alloc-range to highmem if cpu can random access
 */
int cma_mmu_op(struct page *page, int count, bool set)
{
	return 0;
}
#endif

static int tvp_setup_cma_full_pagemap(unsigned long pfn, unsigned long count)
{
#ifdef CONFIG_ARM
	/*
	 * arm already create level 3 mmu mapping for lowmem cma.
	 * And if high mem cma, there is no mapping. So nothing to
	 * do for arch arm.
	 */
	return 0;
#elif defined(CONFIG_ARM64)
	if (!aml_init_mm) {
		pr_err("[%s] aml_init_mm NULL!\n", __func__);
		return -EINVAL;
	}

	struct vm_area_struct vma = {};
	unsigned long addr, size;
	int ret;

	tvp_clear_cma_pagemap(pfn, count);
	addr = (unsigned long)pfn_to_kaddr(pfn);
	size = count * PAGE_SIZE;
	vma.vm_mm    = aml_init_mm;
	vma.vm_start = addr;
	vma.vm_end   = addr + size;
	vma.vm_page_prot = pgprot_tagged(PAGE_KERNEL);
	mmap_write_lock(vma.vm_mm);
	ret = remap_pfn_range(&vma, addr, pfn,
			      size, vma.vm_page_prot);
	if (ret < 0)
		pr_info("%s, remap pte failed:%d, cma:%lx\n",
			__func__, ret, pfn);
	mmap_write_unlock(vma.vm_mm);
	return 0;
#else
	#error "NOT supported ARCH"
#endif
}

int __nocfi get_mte_sync_tags_hook_kprobe(void *data)
{
	static DEFINE_MUTEX(lock);
	struct cma *cma = NULL;
	struct page *page = NULL;
	int ret = 0;
#if defined(CONFIG_ARM64)
	get_faked_init_mm();
	aml_mte_sync_tags = (void (*)(pte_t pte, unsigned int nr_pages))
		get_symbol_addr("mte_sync_tags");
	aml__pte_offset_map = (pte_t * (*)(pmd_t *pmd, unsigned long addr,
		pmd_t *pmdvalp))get_symbol_addr("__pte_offset_map");
	pr_debug("aml_init_mm:%px, aml_mte_sync_tags:%px, aml__pte_offset_map:%px\n",
		aml_init_mm, aml_mte_sync_tags, aml__pte_offset_map);
	if (!aml_init_mm || !aml_mte_sync_tags || !aml__pte_offset_map) {
		pr_err("[%s]symbol fix failed, %px %px %px\n",
			__func__, aml_init_mm, aml_mte_sync_tags, aml__pte_offset_map);
		ret = -1;
		goto out;
	}
#endif

	cma = dev_get_cma_area(get_mem_mgt()->dev);
	if (!cma) {
		pr_err("CMA:  NO CMA region\n");
		ret = -1;
		goto out;
	}
	pr_debug("%s, %s, %lx, %lx\n", __func__, cma_get_name(cma), cma->base_pfn, cma->count);
	/* spin_lock_irqsave(&cma->lock, flags); */
	page = cma_alloc(cma, cma->count, 0, 0);
	if (!page) {
		pr_err("cma alloc failed.\n");
		ret = -1;
		goto out;
	}
	mutex_lock(&lock);
	tvp_setup_cma_full_pagemap(cma->base_pfn, cma->count);
	/* spin_unlock_irqrestore(&cma->lock, flags); */
	mutex_unlock(&lock);
	cma_release(cma, page, cma->count);

out:
	codec_mm_pagemap_done_signal();
	return ret;
}
#endif

static inline void codec_mm_parse_reserved_mem(struct platform_device *pdev, char *name,
	int (*vdec_res_setup)(struct reserved_mem *rmem))
{
	int r;
	struct reserved_mem *mem = NULL;
	int region_index = 0;
	struct device_node *search_target = NULL;

	while (1) {
		search_target = of_parse_phandle(pdev->dev.of_node,
						"memory-region",
						region_index);
		if (!search_target)
			break;

		if (!strcmp(search_target->name, name)) {
			mem = of_reserved_mem_lookup(search_target);
			if (mem) {
				r = vdec_res_setup(mem);
				if (r)
					pr_err("%s vdec_res_setup res %x\n", name, r);
				r = of_reserved_mem_device_init_by_idx(&pdev->dev,
					pdev->dev.of_node, region_index);
				if (r)
					pr_err("%s vdec_res_setup device init failed\n", name);
			}
			break;
		}
		region_index++;
	}
}

u32 codec_mm_get_property_from_dts(char *property_name)
{
	struct device_node *pnode = NULL;
	u32 ret = -1;

	pnode = of_find_node_by_name(NULL, "codec_mm");
	if (!pnode) {
		pr_info("not find codec_mm node\n");
		return -EINVAL;
	}

	if (of_property_read_u32(pnode, property_name, &ret)) {
		pr_debug("read format in dts failed, ret = %d\n", ret);
		return -EINVAL;
	}

	return ret;
}

u64 codec_mm_managed_max_addr(void)
{
	u64 addr = 0;
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	u64 rmem_end = 0;
	u64 cma_end = 0;
	struct cma *cma = NULL;

	if (mgt) {
		if (mgt->rmem.size > 0)
			rmem_end = mgt->rmem.base + mgt->rmem.size;

		if (mgt->total_cma_size > 0) {
			cma = dev_get_cma_area(mgt->dev);
			if (cma && cma != dev_get_cma_area(NULL))
				cma_end = cma_get_base(cma) + mgt->total_cma_size;
		}

		addr = rmem_end >= cma_end ? rmem_end : cma_end;
	}

	return addr;
}

u64 codec_mm_secure_vdec_max_addr(void)
{
	return  sec_vdec_addr + sec_vdec_size;
}

static void prepare_full_pagemap(struct device_node *of_node)
{
#if IS_MODULE(CONFIG_AMLOGIC_MEDIA_MODULE) && \
		!IS_ENABLED(CONFIG_DEBUG_SPINLOCK)
	u32 random = 0;
	int ret;

	if (!of_node)
		goto signal;

	ret = of_property_read_u32(of_node, "random_access", &random);
	if (!ret && random) {
		if (kthread_run(get_mte_sync_tags_hook_kprobe, NULL,
						"AML_CODEC_MM") != ERR_PTR(-ENOMEM))
			return; /* kthread started, it will signal when done */
		pr_err("Failed to start kernel thread AML_CODEC_MM\n");
	}

signal:
#endif
	codec_mm_pagemap_done_signal();
}

void codec_mm_attach_dma_buf(struct dma_buf *dmabuf, ulong phy)
{
	struct codec_mm_s *mem = NULL;

	mem = codec_mm_search_mem_by_phy(phy);
	if (mem)
		mem->dma_buf = dmabuf;
	else
		pr_info("no such a codec_mm buf with phy:%lx to attach\n", phy);
}
EXPORT_SYMBOL(codec_mm_attach_dma_buf);

void codec_mm_detach_dma_buf(struct dma_buf *dmabuf, ulong phy)
{
	struct codec_mm_s *mem = NULL;

	if (file_count(dmabuf->file) != 1)
		return;

	mem = codec_mm_search_mem_by_phy(phy);
	if (mem)
		mem->dma_buf = NULL;
	else
		pr_info("no such a codec_mm buf with phy:%lx to detach\n", phy);
}
EXPORT_SYMBOL(codec_mm_detach_dma_buf);

void codec_mm_show_dma_buf(void)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();
	struct codec_mm_s *mem = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&mgt->lock, flags);
	if (!list_empty(&mgt->mem_list)) {
		list_for_each_entry(mem, &mgt->mem_list, list) {
			if (mem->dma_buf)
				pr_info("%s exporter:%s, addr:0x%lx, size:%d, ino:%ld, ref:%ld\n",
					mem->owner[0],
					mem->dma_buf->exp_name,
					mem->phy_addr,
					mem->buffer_size,
					file_inode(mem->dma_buf->file)->i_ino,
					file_count(mem->dma_buf->file));
		}
	}
	spin_unlock_irqrestore(&mgt->lock, flags);
}

static int codec_mm_probe(struct platform_device *pdev)
{
	int r;
	struct codec_mm_mgt_s *mgt = NULL;

	codec_mm_parse_reserved_mem(pdev, "linux,secure_vdec_reserved", secure_vdec_res_setup);
	codec_mm_parse_reserved_mem(pdev, "linux,codec_mm_reserved", codec_mm_res_setup);
#ifdef CONFIG_CODEC_MM_EXT_POOL
	codec_mm_parse_reserved_mem(pdev, "linux,codec_mm_reserved_ext", codec_mm_res_ext_setup);
#endif

	mgt = get_mem_mgt();
	pdev->dev.platform_data = mgt;

	r = class_register(&codec_mm_class);
	if (r) {
		pr_err("vdec class create fail.\n");
		return r;
	}
	r = of_reserved_mem_device_init(&pdev->dev);
	if (r == 0)
		pr_debug("codec_mm cma memory probed done\n");

	pr_debug("%s ok\n", __func__);
	codec_mm_mgt_init(&pdev->dev);
	codec_mm_scatter_mgt_init(&pdev->dev);
	codec_mm_keeper_mgr_init();
	codec_mm_dev_set_dma_mask(DMA_BIT_MASK(64));
	amstream_test_init();
	codec_mm_scatter_mgt_test();
	REG_PATH_CONFIGS(CONFIG_PATH, codec_mm_configs);
	INIT_REG_NODE_CONFIGS(CONFIG_PATH, &codec_mm_trigger_node,
				  "trigger", codec_mm_trigger,
				  CONFIG_FOR_RW | CONFIG_FOR_T);
	codec_state_register(&mgt->cs, &codec_mm_cs_ops);

	init_alloc_page_boost_task();
	prepare_full_pagemap(pdev->dev.of_node);
	init_prealloc_boost_task();

	return 0;
}

#ifdef CONFIG_PM
static BLOCKING_NOTIFIER_HEAD(codec_mm_freeze_chain);

int codec_mm_register_freeze_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&codec_mm_freeze_chain, nb);
}
EXPORT_SYMBOL(codec_mm_register_freeze_notifier);

int codec_mm_unregister_freeze_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&codec_mm_freeze_chain, nb);
}
EXPORT_SYMBOL(codec_mm_unregister_freeze_notifier);

static int codec_mm_restore(struct device *dev)
{
	int ret = 0;

	if (sec_vdec_addr > 0 && sec_vdec_size > 0) {
		ret = tee_register_mem(TEE_MEM_TYPE_STREAM_INPUT,
				sec_vdec_addr, sec_vdec_size);
		if (ret) {
			pr_err("Restore secure vdec memory %llx %llx ret is %x\n",
				(u64)sec_vdec_addr, (u64)sec_vdec_size, ret);
		}
	}

	return ret;
}

static int codec_mm_freeze(struct device *dev)
{
	blocking_notifier_call_chain(&codec_mm_freeze_chain, 0, NULL);
	return 0;
}

static const struct dev_pm_ops codec_mm_pm_ops = {
	.freeze = codec_mm_freeze,
	.restore = codec_mm_restore,
};
#endif

static const struct of_device_id amlogic_mem_dt_match[] = {
	{
			.compatible = "amlogic, codec, mm",
		},
	{},
};

static struct platform_driver codec_mm_driver = {
	.probe = codec_mm_probe,
	.remove = NULL,
	.driver = {
			.owner = THIS_MODULE,
			.name = "codec_mm",
			.of_match_table = amlogic_mem_dt_match,
#ifdef CONFIG_PM
			.pm = &codec_mm_pm_ops,
#endif
		}
};

int __init codec_mm_module_init(void)
{
	pr_debug("now in %s\n", __func__);

	if (platform_driver_register(&codec_mm_driver)) {
		pr_err("failed to register amports mem module\n");
		return -ENODEV;
	}

	if (codec_state_init()) {
		platform_driver_unregister(&codec_mm_driver);
		pr_err("Create codec mm debugfs failed.\n");
		return -EINVAL;
	}

	if (codec_mm_track_init()) {
		codec_state_release();
		platform_driver_unregister(&codec_mm_driver);
		pr_err("Create codec mm debugfs failed.\n");
		return -EINVAL;
	}

	return 0;
}

void __exit codec_mm_module_exit(void)
{
	codec_state_release();

	codec_mm_track_exit();

	platform_driver_unregister(&codec_mm_driver);
}

//MODULE_DESCRIPTION("AMLOGIC amports mem  driver");
//MODULE_LICENSE("GPL");

static int codec_mm_reserved_init(struct reserved_mem *rmem, struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mgt->rmem = *rmem;
	pr_debug("%s %p->%p\n", __func__,
		 (void *)mgt->rmem.base,
		 (void *)mgt->rmem.base + mgt->rmem.size);
	return 0;
}

static const struct reserved_mem_ops codec_mm_rmem_vdec_ops = {
	.device_init = codec_mm_reserved_init,
};

static int codec_mm_res_setup(struct reserved_mem *rmem)
{
	rmem->ops = &codec_mm_rmem_vdec_ops;
	pr_debug("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_reserved, "amlogic, codec-mm-reserved",
			   codec_mm_res_setup);

static int secure_vdec_reserved_init(struct reserved_mem *rmem,
	struct device *dev)
{
	int ret = -1;

	if (!rmem || rmem->size <= 0) {
		pr_err("Invalid reserved secure vdec memory");
		return 0;
	}

	ret = tee_register_mem(TEE_MEM_TYPE_STREAM_INPUT,
				rmem->base,
				rmem->size);
	if (ret) {
		pr_err("protect vdec failed addr %llx %llx ret is %x\n",
			(u64)rmem->base, (u64)rmem->size, ret);
	}

	sec_vdec_addr = rmem->base;
	sec_vdec_size = rmem->size;

	return ret;
}

static const struct reserved_mem_ops secure_vdec_rmem_ops = {
	.device_init = secure_vdec_reserved_init,
};

static int secure_vdec_res_setup(struct reserved_mem *rmem)
{
	rmem->ops = &secure_vdec_rmem_ops;

	return 0;
}

RESERVEDMEM_OF_DECLARE(secure_vdec_reserved, "amlogic, secure-vdec-reserved",
			secure_vdec_res_setup);

#ifdef CONFIG_CODEC_MM_EXT_POOL
static int codec_mm_reserved_ext_init(struct reserved_mem *rmem, struct device *dev)
{
	struct codec_mm_mgt_s *mgt = get_mem_mgt();

	mgt->rmem_ext = *rmem;
	pr_debug("%s %p->%p\n", __func__,
		 (void *)mgt->rmem_ext.base,
		 (void *)mgt->rmem_ext.base + mgt->rmem_ext.size);
	return 0;
}

static const struct reserved_mem_ops codec_mm_rmem_ext_vdec_ops = {
	.device_init = codec_mm_reserved_ext_init,
};

static int codec_mm_res_ext_setup(struct reserved_mem *rmem)
{
	rmem->ops = &codec_mm_rmem_ext_vdec_ops;
	pr_debug("vdec: reserved mem setup\n");

	return 0;
}

RESERVEDMEM_OF_DECLARE(codec_mm_reserved_ext, "amlogic, codec-mm-reserved_ext",
			codec_mm_res_ext_setup);
#endif

module_param(debug_mode, uint, 0664);
MODULE_PARM_DESC(debug_mode, "\n debug module\n");
__module_param(debug_sc_mode, uint, 0664);
MODULE_PARM_DESC(debug_sc_mode, "\n debug scatter module\n");
module_param(debug_keep_mode, uint, 0664);
MODULE_PARM_DESC(debug_keep_mode, "\n debug keep module\n");
__module_param(tvp_mode, uint, 0664);
MODULE_PARM_DESC(tvp_mode, "\n tvp module\n");
__module_param(tvp_dynamic_increase_disable, uint, 0664);
MODULE_PARM_DESC(tvp_dynamic_increase_disable, "\n disable tvp_dynamic_increase\n");
__module_param(tvp_pool_early_release_switch, uint, 0664);
MODULE_PARM_DESC(tvp_pool_early_release_switch,
	"\n forbiden tvp pool release when tvp not disable\n");
