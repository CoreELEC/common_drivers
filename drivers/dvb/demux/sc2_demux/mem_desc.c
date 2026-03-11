// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/tee.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/cpu_version.h>

#include <linux/highmem.h>

//#define CHECK_PACKET_ALIGNM
#ifdef CHECK_PACKET_ALIGNM
#include <linux/highmem.h>
#endif

#include "mem_desc.h"
#include "sc2_control.h"
#include "../aml_dvb.h"
#include "../dmx_log.h"

#define MEM_DESC_SLOT_USED		1
#define MEM_DESC_SLOT_FREE		0

#define MEM_DESC_EOC_USED		1
#define MEM_DESC_EOC_FREE		0

#define R_MAX_MEM_CHAN_NUM			32
#define W_MAX_MEM_CHAN_NUM			128

static struct chan_id chan_id_table_w[W_MAX_MEM_CHAN_NUM];
static struct chan_id chan_id_table_r[R_MAX_MEM_CHAN_NUM];

#define dprint_i(fmt, args...)   \
	dprintk(LOG_ERROR, debug_mem_desc, fmt, ## args)
#define dprint(fmt, args...)     \
	dprintk(LOG_ERROR, debug_mem_desc, "mem_desc:" fmt, ## args)
#define pr_dbg(fmt, args...)     \
	dprintk(LOG_DBG, debug_mem_desc, "mem_desc:" fmt, ## args)
#define pr_es_dbg(fmt, args...)     \
	dprintk(LOG_DBG, debug_mem_desc, "mem_desc:" fmt, ## args)
#define pr_dbg_mem(fmt, args...)     \
	do {\
		if (debug_mem_desc == 2)\
			dprintk(LOG_DBG, 1, "mem_desc:" fmt, ## args);\
	} while (0)

#define DEFAULT_RCH_SYNC_NUM	8
#define BEN_LEVEL_SIZE			(512 * 1024)

static int rch_sync_num_last = -1;
static int rch_sync_num = DEFAULT_RCH_SYNC_NUM;
static int dmc_keep_alive;
static int write_timeout_ms = 1000;
static int debug_mem_desc;
static int loop_desc = 0x1;
static int pvr_memory_reserved;

struct mem_cache {
	unsigned long start_virt;
	unsigned long start_phys;
	unsigned int elem_count;
	unsigned int elem_size;
	unsigned int used_count;
	// TODO: the array's size should be larger than 2 caches' elem count
	char flag_arry[64];
};

#define FIRST_CACHE_ELEM_COUNT     64
#define SECOND_CACHE_ELEM_COUNT    48
#define SECOND_CACHE_ELEM_SIZE     (128 * 1024)

static int cache0_count_max = FIRST_CACHE_ELEM_COUNT;
static int cache1_count_max = SECOND_CACHE_ELEM_COUNT;

static struct mem_cache *first_cache;
static struct mem_cache *second_cache;

struct mem_region {
	u8 status;
	unsigned long start_phy;
	unsigned long start_virt;
	unsigned int len;
	struct mem_region *pnext;
};

struct dmc_range {
	u8 used;
	u8 level;
	unsigned int handle;
	unsigned int size;
	unsigned long buf_start_phy;
	unsigned int ref;
	unsigned long free_start_phy;
	unsigned int free_len;
	struct mem_region *region_list;
};

struct dmc_mem {
	int init;
	u8 level;
	unsigned int total_size;
	u8 range_num;
	struct dmc_range *range;
};

#define DMC_MEM_DEFAULT_SIZE		(20 * 1024 * 1024)
#define DMC_MEM_NON_VIDEO_SIZE		(5 * 1024 * 1024)
#define INIT_DMC_RAGNE_NUM	2
#define DESC_LAST_SIZE		0x100
#define DESC_NUM		4
#define UPDATE_DESC_TIMEOUT	30

static DEFINE_SPINLOCK(add_buf_lock);
static struct mutex dmc_mutex;
static struct dmc_mem dmc_mem_level[7];
static dmx_dump_cb dump_input_cb;
static int dump_input_cb_ref;
LIST_HEAD(dump_input_head);

static int cache_init(int cache_level)
{
	int total_size = 0;
	int buf_page_num = 0;
	unsigned int gfp_flags = 0;
	int err_val = 0;

	if (cache_level == 0 && !first_cache) {
		first_cache = vmalloc(sizeof(*first_cache));
		if (!first_cache)
			return -1;

		memset(first_cache, 0, sizeof(struct mem_cache));
		first_cache->elem_size = sizeof(union mem_desc) * MAX_ES_BUF_NUM * DESC_NUM;
		first_cache->elem_count = cache0_count_max;
		total_size = first_cache->elem_size * first_cache->elem_count;
		if ((get_dmx_version() >= 6) && (sizeof(unsigned long) == 8)) {
			gfp_flags = GFP_KERNEL;
			if (dma_set_mask_and_coherent(aml_get_device(), DMA_BIT_MASK(64)))
				dprint("dma_set_mask fail\n");
		} else {
			gfp_flags = GFP_KERNEL | GFP_DMA32;
		}
		first_cache->start_virt = (unsigned long)dma_alloc_coherent(aml_get_device(),
			total_size, (dma_addr_t *)&first_cache->start_phys, gfp_flags);
		if (!first_cache->start_virt) {
			vfree(first_cache);
			first_cache = NULL;
			err_val = -1;
			goto error_handle;
		}
		pr_dbg("first_cache start_phys:0x%lx, start_virt:0x%lx\n",
			first_cache->start_phys, first_cache->start_virt);
	} else if (cache_level == 1 && !second_cache) {
		second_cache = vmalloc(sizeof(*second_cache));
		if (!second_cache)
			return -1;

		memset(second_cache, 0, sizeof(struct mem_cache));
		second_cache->elem_size = SECOND_CACHE_ELEM_SIZE;
		second_cache->elem_count = cache1_count_max;
		total_size = cache1_count_max * SECOND_CACHE_ELEM_SIZE;
		buf_page_num = PAGE_ALIGN(total_size) / PAGE_SIZE;

		pr_dbg_mem("%s size:%d\n", __func__, total_size);
		second_cache->start_phys =
		    codec_mm_alloc_for_dma("dmx_cache", buf_page_num,
					4 + PAGE_SHIFT, CODEC_MM_FLAGS_DMA_CPU);
		if (!second_cache->start_phys) {
			vfree(second_cache);
			second_cache = NULL;
			err_val = -2;
			goto error_handle;
		}
		second_cache->start_virt =
		(unsigned long)codec_mm_phys_to_virt(second_cache->start_phys);
		if (IS_ERR_OR_NULL((const void *)second_cache->start_virt)) {
			codec_mm_free_for_dma("dmx", second_cache->start_phys);
			vfree(second_cache);
			second_cache = NULL;
			err_val = -3;
			goto error_handle;
		}
		pr_dbg("second_cache start_phys:0x%lx, start_virt:0x%lx\n",
			second_cache->start_phys, second_cache->start_virt);
	}
	return 0;

error_handle:
	dprint("cache init failed! err_val:%d\n", err_val);
	return -1;
}

static int cache_destroy(int cache_level)
{
	int ret = -1;

	if (cache_level == 0 && first_cache) {
		dma_free_coherent(aml_get_device(),
			first_cache->elem_size * first_cache->elem_count,
			(void *)first_cache->start_virt,
			first_cache->start_phys);
		vfree(first_cache);
		first_cache = NULL;
		dprint_i("clear first cache done\n");
		ret = 0;
	} else if (cache_level == 1 && second_cache) {
		codec_mm_free_for_dma("dmx_cache", second_cache->start_phys);
		vfree(second_cache);
		second_cache = NULL;
		dprint_i("clear second cache done\n");
		ret = 0;
	}
	return ret;
}

static int cache_get_block(struct mem_cache *cache,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int i = 0;

	for (i = 0; i < cache->elem_count; i++) {
		if (cache->flag_arry[i] == 0)
			break;
	}

	if (i == cache->elem_count) {
		dprint_i("error!!! dmx cache full, elem_count:%d\n", cache->elem_count);
		return -1;
	}
	if (p_virt)
		*p_virt = cache->start_virt + i * cache->elem_size;
	if (p_phys)
		*p_phys = cache->start_phys + i * cache->elem_size;

	cache->flag_arry[i] = 1;
	cache->used_count++;
	return 0;
}

static int cache_free_block(struct mem_cache *cache, unsigned long phys_mem)
{
	int i = 0;

	if (phys_mem >= cache->start_phys &&
		phys_mem <= cache->start_phys +
			(cache->elem_count - 1) * cache->elem_size) {
		for (i = 0; i < cache->elem_count; i++) {
			if (phys_mem ==
					cache->start_phys +
					i * cache->elem_size) {
				cache->flag_arry[i] = 0;
				cache->used_count--;
				return 0;
			}
		}
	}

	dprint("%s error!!! phys_mem:0x%lx\n", __func__, phys_mem);
	for (i = 0; i < cache->elem_count; i++)
		dprint("%s flag_arry[%d]:%d addr:0x%lx\n", __func__, i,
			cache->flag_arry[i], cache->start_phys + i * cache->elem_size);
	return -1;
}

static int cache_malloc(int len, unsigned long *p_virt, unsigned long *p_phys)
{
//	dprint("%s, len:%d\n", __func__, len);
	if (!first_cache) {
		if (cache_init(0) != 0)
			return -1;
	}
	if (!second_cache) {
		if (cache_init(1) != 0)
			return -1;
	}

	if (len <= first_cache->elem_size)
		return cache_get_block(first_cache, p_virt, p_phys);
	else if (len <= second_cache->elem_size)
		return cache_get_block(second_cache, p_virt, p_phys);

	return -1;
}

static int cache_free(int len, unsigned long phys_mem)
{
	int iret  = -1;

	if (first_cache && len <= first_cache->elem_size)
		iret = cache_free_block(first_cache, phys_mem);
	else if (second_cache && len <= second_cache->elem_size)
		iret = cache_free_block(second_cache, phys_mem);

	return iret;
}

int cache_clear(void)
{
	int ret0 = -1;
	int ret1 = -1;

	if (first_cache && first_cache->used_count == 0)
		ret0 = cache_destroy(0);
	if (second_cache && second_cache->used_count == 0)
		ret1 = cache_destroy(1);
	if (!ret0 && !ret1)
		return 0;
	else
		return -1;
}

int cache_adjust(int cache0_count, int cache1_count)
{
	if (cache0_count > 64 || cache1_count > 64) {
		dprint_i("cache count can't bigger than 64\n");
		return -1;
	}
	dprint_i("cache0 count:%d, cache1 count:%d\n",
		cache0_count, cache1_count);
	cache0_count_max = cache0_count;
	cache1_count_max = cache1_count;

	if (first_cache && first_cache->used_count == 0)
		cache_destroy(0);
	if (second_cache && second_cache->used_count == 0)
		cache_destroy(1);

	cache_init(0);
	cache_init(1);
	return 0;
}

int cache_status_info(char *buf)
{
	int r, total = 0;

	if (first_cache) {
		r = sprintf(buf, "first cache:\n");
		buf += r;
		total += r;

		r = sprintf(buf, "total size:%d, block count:%d, ",
		    first_cache->elem_count * first_cache->elem_size,
			first_cache->elem_count);
		buf += r;
		total += r;

		r = sprintf(buf, "block size:%d, used count:%d\n",
			first_cache->elem_size,
			first_cache->used_count);
		buf += r;
		total += r;
	} else {
		r = sprintf(buf, "first cache:no\n");
		buf += r;
		total += r;
	}

	if (second_cache) {
		r = sprintf(buf, "second cache:\n");
		buf += r;
		total += r;

		r = sprintf(buf, "total size:%d, block count:%d, ",
		    second_cache->elem_count * second_cache->elem_size,
			second_cache->elem_count);
		buf += r;
		total += r;

		r = sprintf(buf, "block size:%d, used count:%d\n",
			second_cache->elem_size,
			second_cache->used_count);
		buf += r;
		total += r;
	} else {
		r = sprintf(buf, "second cache:no\n");
		buf += r;
		total += r;
	}
	return total;
}

static int dmc_range_init(struct dmc_range *range, int sec_level)
{
	int flags = 0;
	int buf_page_num = 0;
	unsigned long buf_start = 0;
	unsigned long buf_start_virt = 0;
	u32 ret = -1;
	u32 len = range->size;
	int err_val;

	flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER;

	buf_page_num = PAGE_ALIGN(len) / PAGE_SIZE;

	pr_dbg_mem("%s size:%d\n", __func__, len);
	buf_start =
	    codec_mm_alloc_for_dma("dmx", buf_page_num, 4 + PAGE_SHIFT, flags);
	if (!buf_start) {
		err_val = -1;
		goto error_handle;
	}
	buf_start_virt = (unsigned long)codec_mm_phys_to_virt(buf_start);
	if (IS_ERR_OR_NULL((const void *)buf_start_virt)) {
		codec_mm_free_for_dma("dmx", buf_start);
		err_val = -2;
		goto error_handle;
	}
	pr_dbg_mem("dmc mem init phy:0x%lx, virt:0x%lx, len:%d\n",
		buf_start, buf_start_virt, len);
	if (debug_mem_desc == 2) {
		memset((char *)buf_start_virt, 0, len);
		codec_mm_dma_flush((void *)buf_start_virt, len, DMA_TO_DEVICE);
	}
	range->level = sec_level;
	if (sec_level) {
		sec_level = sec_level == 1 ? 0 : sec_level;
		ret = tee_protect_mem(TEE_MEM_TYPE_DEMUX,
			sec_level, buf_start, len, &range->handle);
		dprint("tee_protect_mem ret:0x%x\n", ret);
	}
	range->buf_start_phy = buf_start;
	range->free_start_phy = buf_start;
	range->free_len = len;
	range->ref = 0;
	range->region_list = NULL;
	range->used = 1;
	return 0;

error_handle:
	dprint("dmc range init failed! err_val:%d\n", err_val);
	return -1;
}

static int dmc_range_destroy(struct dmc_range *range)
{
	struct mem_region *header = NULL;
	struct mem_region *tmp = NULL;

	pr_es_dbg("%s buf_start_phy:0x%lx\n", __func__, range->buf_start_phy);
	if (!range->used) {
		dprint("%s error!!! not used\n", __func__);
		return 0;
	}

	tee_unprotect_mem(range->handle);
	codec_mm_free_for_dma("dmx", range->buf_start_phy);
	range->handle = 0;
	range->buf_start_phy = 0;

	header = range->region_list;
	while (header) {
		tmp = header->pnext;
		if (header->status == 1)
			dprint("%s used mem have free error\n", __func__);
		vfree(header);
		header = tmp;
	}

	range->region_list = NULL;
//	mem->size = 0;
	range->free_len = 0;
	range->free_start_phy = 0;
	range->used = 0;
	return 0;
}

static int dmc_range_get_block(struct dmc_range *range, unsigned int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	struct mem_region *header = range->region_list;
	struct mem_region *temp = NULL;
	int keep_4k_align = 0;

	while (header) {
		if (header->len == len &&
			header->status == 0) {
			header->status = 1;
			*p_virt = header->start_virt;
			*p_phys = header->start_phy;
			range->ref++;
			return 0;
		}
		header = header->pnext;
	}

	if (range->free_len < len)
		return -1;

	temp = vmalloc(sizeof(*temp));
	if (!temp)
		return -1;

	temp->len = len;
	temp->start_phy = range->free_start_phy;
	temp->start_virt = (unsigned long)codec_mm_phys_to_virt(temp->start_phy);
	if (!temp->start_virt) {
		vfree(temp);
		dprint("phys_to_virt fail\n");
		return -1;
	}
	temp->status = 1;
	temp->pnext = range->region_list;

	range->region_list = temp;
	range->free_len -= len;
	range->free_start_phy += len;
	keep_4k_align = range->free_start_phy % (4 * 1024);
	if (keep_4k_align) {
		keep_4k_align = 4 * 1024 - keep_4k_align;
		if (range->free_len > keep_4k_align) {
			range->free_len -= keep_4k_align;
			range->free_start_phy += keep_4k_align;
		}
	}
	range->ref++;

	*p_virt = temp->start_virt;
	*p_phys = temp->start_phy;
	pr_es_dbg("%s start_phy:0x%lx len:0x%x\n", __func__, temp->start_phy, len);
	return 0;
}

static int dmc_range_free_block(struct dmc_range *range, unsigned long phys_mem)
{
	struct mem_region *header = range->region_list;

	while (header) {
		if (header->start_phy == phys_mem &&
			header->status == 1) {
			header->status = 0;
			range->ref--;
			pr_es_dbg("%s phys_mem:0x%lx\n", __func__, phys_mem);
			return 0;
		}
		header = header->pnext;
	}
	dprint("%s no find block\n", __func__);
	return -1;
}

static int dmc_mem_init(struct dmc_mem *mem, int sec_level)
{
	struct dmc_range *temp = NULL;
	int i = 0;
	int num = 0;

	mem->range_num = 2;
	temp = vmalloc(sizeof(*temp) * mem->range_num);
	if (!temp)
		return -1;
	memset(temp, 0, sizeof(*temp) * mem->range_num);

	num = mem->total_size / DMC_MEM_DEFAULT_SIZE;
	if (num >= mem->range_num || num >= 3) {
		if (mem->total_size == 66 * 1024 * 1024) {
			temp[0].size = 44 * 1024 * 1024;
			temp[1].size = 22 * 1024 * 1024;
		} else {
			for (i = 0; i < mem->range_num; i++)
				temp[i].size = mem->total_size / mem->range_num;
		}
	} else {
		if (num == 1 || num == 0) {
			temp[0].size = mem->total_size;
		} else {
			temp[0].size = mem->total_size - DMC_MEM_DEFAULT_SIZE;
			temp[1].size = DMC_MEM_DEFAULT_SIZE;
		}
	}
	mem->range = temp;
	mem->level = sec_level;
	mem->init = 1;
	return 0;
}

static int dmc_mem_init_multi_buf(struct dmc_mem *mem, int sec_level)
{
	struct dmc_range *temp = NULL;
	int num = 0;

	mem->range_num = INIT_DMC_RAGNE_NUM;
	temp = vmalloc(sizeof(*temp) * (INIT_DMC_RAGNE_NUM + 3 * (MAX_ES_BUF_NUM - 1)));
	if (!temp) {
		dprint("%s err vmalloc\n", __func__);
		return -1;
	}
	memset(temp, 0, sizeof(*temp) * (INIT_DMC_RAGNE_NUM + 3 * (MAX_ES_BUF_NUM - 1)));

	// TODO: need to review this
	num = mem->total_size / (DMC_MEM_NON_VIDEO_SIZE + get_multi_es_buf0_size());
	if (num >= mem->range_num) {
		temp[0].size = DMC_MEM_NON_VIDEO_SIZE + get_multi_es_buf0_size() + 2 * 1024 * 1024;
		temp[1].size = mem->total_size - temp[0].size;
	} else {
		temp[0].size = mem->total_size;
		mem->range_num = 1;
	}

	mem->range = temp;
	mem->level = sec_level;
	mem->init = 1;
	dprint("%s total_size:0x%x num:%d range_num:%d\n", __func__, mem->total_size, num,
		mem->range_num);
	return 0;
}

static int dmc_mem_add_range(struct dmc_mem *mem, int range_size)
{
	int i = 0;

	for (i = 0; i < (INIT_DMC_RAGNE_NUM + 3 * (MAX_ES_BUF_NUM - 1)); i++) {
		if (mem->range[i].used == 0)
			break;
	}

	if (i >= (INIT_DMC_RAGNE_NUM + 3 * (MAX_ES_BUF_NUM - 1))) {
		dprint("%s error!!! i:%d\n", __func__, i);
		return -1;
	}

	mem->range[i].size = range_size;
	//mem->total_size += range_size;
	mem->range_num += 1;
	dprint("%s range[%d].size:0x%x range_num:%d\n", __func__, i, mem->range[i].size,
		mem->range_num);
	return 0;
}

static int dmc_mem_destroy(struct dmc_mem *mem)
{
	int i = 0;
	int not_free = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (mem->range[i].used) {
			if (mem->range[i].ref == 0)
				dmc_range_destroy(&mem->range[i]);
			else
				not_free = 1;
		}
	}

	pr_es_dbg("%s not_free:%d\n", __func__, not_free);
	if (not_free)
		return 0;
	vfree(mem->range);
	mem->range = NULL;
	mem->init = 0;
	return 0;
}

static int dmc_mem_get_block(struct dmc_mem *mem, unsigned int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int i = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (!mem->range[i].used && mem->range[i].size != 0)
			if (dmc_range_init(&mem->range[i], mem->level) != 0)
				return -1;
		if (dmc_range_get_block(&mem->range[i], len, p_virt, p_phys) == 0)
			return 0;
	}
	return -1;
}

static void dmc_mem_print(int mem_num);
static void dmc_mem_print_info(void);
static int dmc_mem_free_block(struct dmc_mem *mem, unsigned long phys_mem)
{
	int i = 0;

	for (i = 0; i < mem->range_num; i++) {
		if (mem->range[i].used && mem->range[i].size != 0) {
			if (dmc_range_free_block(&mem->range[i], phys_mem) == 0) {
				if (!mem->range[i].ref && !dmc_keep_alive)
					dmc_mem_destroy(mem);
				return 0;
			}
		}
	}

	dprint("%s error!!! no find block\n", __func__);
	dmc_mem_print_info();
	return -1;
}

static int dmc_mem_malloc(int sec_level, int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int dmc_index = sec_level - 1;
	int ret = 0;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}

	if (!dmc_mem_level[dmc_index].init) {
		ret = dmc_mem_init(&dmc_mem_level[dmc_index], sec_level);
		if (ret != 0)
			return -1;
	}
	return dmc_mem_get_block(&dmc_mem_level[dmc_index], len, p_virt, p_phys);
}

static int dmc_mem_malloc_multi_buf(int sec_level, int len,
	unsigned long *p_virt, unsigned long *p_phys)
{
	int dmc_index = sec_level - 1;
	int ret = 0;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}

	if (!dmc_mem_level[dmc_index].init) {
		ret = dmc_mem_init_multi_buf(&dmc_mem_level[dmc_index], sec_level);
		if (ret != 0)
			return -1;
	}

	ret = dmc_mem_get_block(&dmc_mem_level[dmc_index], len, p_virt, p_phys);
	if (ret != 0) {
		dprint("%s len:0x%x\n", __func__, len);
		ret = dmc_mem_add_range(&dmc_mem_level[dmc_index], len);
		if (ret != 0)
			return -1;
		ret = dmc_mem_get_block(&dmc_mem_level[dmc_index], len, p_virt, p_phys);
	}

	// TODO: added for debug, will remove it
	if (len > 3 * 1024 * 1024)
		dmc_mem_print(1);

	return ret;
}

static int dmc_mem_free(unsigned long buf, unsigned int len, int sec_level)
{
	int dmc_index = sec_level - 1;
	int ret = 0;
	int err_val;

	if (dmc_index < 0 || dmc_index >= 7) {
		err_val = -1;
		goto error_handle;
	}
	if (!dmc_mem_level[dmc_index].init) {
		err_val = -2;
		goto error_handle;
	}
	ret = dmc_mem_free_block(&dmc_mem_level[dmc_index], buf);
	if (ret != 0) {
		err_val = -3;
		goto error_handle;
	}
	return ret;

error_handle:
	dprint("dmc mem free failed! err_val:%d level:%d buf:0x%lx\n", err_val, sec_level, buf);
	return -1;
}

int dmc_mem_set_size(int sec_level, unsigned int mem_size)
{
	int dmc_index = sec_level - 1;

	// TODO: added for fcc test, will remove this
	//if (get_multi_es_buf_switch() && mem_size == 66 * 1024 * 1024)
		//mem_size = 36 * 1024 * 1024;

	if (dmc_index < 0 || dmc_index >= 7) {
		dprint("%s err level:%d\n", __func__, sec_level);
		return -1;
	}
	if (!dmc_mem_level[dmc_index].init)
		dmc_mem_level[dmc_index].total_size = mem_size / (64 * 1024) * (64 * 1024);
	else
		dprint("%s should set size before app\n", __func__);

	pr_es_dbg("%s mem_size:0x%x total_size:0x%x\n", __func__, mem_size,
		dmc_mem_level[dmc_index].total_size);
	return 0;
}

static void dmc_mem_print(int mem_num)
{
	struct dmc_mem *mem = &dmc_mem_level[mem_num];
	int n = 0;

	if (mem->init) {
		pr_es_dbg("%d init:1 level:%d total size:%d range_num:%d\n",
			mem_num, mem->level, mem->total_size, mem->range_num);

		for (n = 0; n < mem->range_num; n++) {
			struct mem_region *header = mem->range[n].region_list;

			pr_es_dbg("\t%d used:%d addr:0x%lx size:0x%0x ref:%d free:0x%0x\n",
				n, mem->range[n].used, mem->range[n].buf_start_phy,
				mem->range[n].size, mem->range[n].ref, mem->range[n].free_len);
			while (header) {
				if (header->len >= 3 * 1024 * 1024)
					dprint("\t\tstatus:%d addr:0x%lx len 0x%0x\n",
						header->status, header->start_phy, header->len);
				header = header->pnext;
			}
		}
	} else {
		pr_es_dbg("%d init:0 level:%d total size:%d\n",
			mem_num, mem->level, mem->total_size);
	}
}

static void dmc_mem_print_info(void)
{
	int i = 0;

	pr_es_dbg("********dmc mem********\n");
	for (i = 0; i < 7; i++)
		dmc_mem_print(i);
}

static int dmc_mem_dump(char *buf, int mem_num)
{
	struct dmc_mem *mem = &dmc_mem_level[mem_num];
	int r, total = 0;
	int n = 0;

	if (mem->init) {
		r = sprintf(buf, "%d status: using level:%d total size:%d range_num:%d\n",
			mem_num,
			mem->level,
			mem->total_size,
			mem->range_num);
		buf += r;
		total += r;
		if (!mem->range)
			return total;

		for (n = 0; n < mem->range_num; n++) {
			struct mem_region *header = mem->range[n].region_list;

			r = sprintf(buf, "\t%d used:%d addr:0x%lx size:0x%0x ref:%d free:0x%0x\n",
				n,
				mem->range[n].used,
				mem->range[n].buf_start_phy,
				mem->range[n].size,
				mem->range[n].ref,
				mem->range[n].free_len);
			buf += r;
			total += r;

			while (header) {
				r = sprintf(buf, "\t\tstatus:%d addr:0x%lx len 0x%0x\n",
					header->status, header->start_phy, header->len);
				buf += r;
				total += r;
				header = header->pnext;
			}
		}
	} else {
		r = sprintf(buf, "%d status: no use level:%d total size:%d\n",
			mem_num,
			mem->level,
			mem->total_size);
		buf += r;
		total += r;
	}

	return total;
}

int dmc_mem_dump_info(char *buf)
{
	int i = 0;
	int r, total = 0;

	r = sprintf(buf, "********dmc mem********\n");
	buf += r;
	total += r;

	for (i = 0; i < 7; i++) {
		r = dmc_mem_dump(buf, i);
		buf += r;
		total += r;
	}
	return total;
}

/**
 * dma_alloc_from_contiguous_dmx() - allocate pages from contiguous area
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
struct page *dma_alloc_from_contiguous_dmx(struct device *dev, size_t count,
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
 * dma_release_from_contiguous_dmx() - release allocated pages
 * @dev:   Pointer to device for which the pages were allocated.
 * @pages: Allocated pages.
 * @count: Number of allocated pages.
 *
 * This function releases memory allocated by dma_alloc_from_contiguous().
 * It returns false when provided pages do not belong to contiguous area and
 * true otherwise.
 */
bool dma_release_from_contiguous_dmx(struct device *dev, struct page *pages,
				 int count)
{
	return cma_release(dev_get_cma_area(dev), pages, count);
}

int _alloc_buff_cma(unsigned int len,	unsigned long *vir_mem, unsigned long *phy_mem)
{
	struct page *pages;
	int count;
	unsigned long virt_tmp;
	unsigned long phy_tmp;

	count = PAGE_ALIGN(len) / PAGE_SIZE;
	pages = dma_alloc_from_contiguous_dmx(aml_get_device(), count, 4 + PAGE_SHIFT, 0);
	if (pages) {
		virt_tmp = (unsigned long)page_address(pages);
		phy_tmp = page_to_phys(pages);
		if (virt_tmp == 0 || phy_tmp == 0) {
			dprint_i("%s transfer fail\n", __func__);
			return -1;
		}
		*vir_mem = virt_tmp;
		*phy_mem = phy_tmp;
		return 0;
	}
	return -1;
}

void _free_buff_cma(unsigned long phy_buf, unsigned int len)
{
	struct page *pages;
	bool ret;
	int count;

	pages = phys_to_page(phy_buf);
	count = PAGE_ALIGN(len) / PAGE_SIZE;
	ret = dma_release_from_contiguous_dmx(aml_get_device(), pages, count);
	if (!ret)
		dprint_i("dma_release_from_contiguous fail\n");
}

int _alloc_buff(unsigned int len, int sec_level,
		unsigned long *vir_mem, unsigned long *phy_mem, int format)
{
	int flags = 0;
	int buf_page_num = 0;
	unsigned long buf_start = 0;
	unsigned long buf_start_virt = 0;
	int iret = 0;
	int err_val = 0;
	int alloc_pos = 0;

	pr_dbg_mem("%s size:%d\n", __func__, len);
	if (sec_level) {
		if (get_multi_es_buf_switch()) {
			mutex_lock(&dmc_mutex);
			iret = dmc_mem_malloc_multi_buf(sec_level, len, &buf_start_virt,
							&buf_start);
			mutex_unlock(&dmc_mutex);
		} else {
			iret = dmc_mem_malloc(sec_level, len, &buf_start_virt, &buf_start);
		}
		if (iret == 0) {
			alloc_pos = 1;
			goto success_handle;
		}

		dmc_mem_print_info();
		err_val = -1;
		goto error_handle;
	}

	iret = cache_malloc(len, &buf_start_virt, &buf_start);
	if (iret == 0) {
		alloc_pos = 2;
		goto success_handle;
	}

	if (pvr_memory_reserved && format == DVR_FORMAT) {
		iret = _alloc_buff_cma(len, &buf_start_virt, &buf_start);
		if (iret == 0) {
			alloc_pos = 3;
			goto success_handle;
		}

		err_val = -2;
		goto error_handle;
	}

	if (len < BEN_LEVEL_SIZE)
		flags = CODEC_MM_FLAGS_DMA_CPU;
	else
		flags = CODEC_MM_FLAGS_DMA_CPU | CODEC_MM_FLAGS_FOR_VDECODER;
	buf_page_num = PAGE_ALIGN(len) / PAGE_SIZE;
	buf_start = codec_mm_alloc_for_dma("dmx", buf_page_num, 4 + PAGE_SHIFT, flags);
	if (!buf_start) {
		err_val = -3;
		goto error_handle;
	}
	buf_start_virt = (unsigned long)codec_mm_phys_to_virt(buf_start);
	if (IS_ERR_OR_NULL((const void *)buf_start_virt)) {
		codec_mm_free_for_dma("dmx", buf_start);
		err_val = -4;
		goto error_handle;
	}
	alloc_pos = 4;

success_handle:
	pr_dbg_mem("alloc buf, phy:0x%lx, virt:0x%lx, len:%d, alloc_pos:%d\n",
		buf_start, buf_start_virt, len, alloc_pos);
	if (!sec_level && debug_mem_desc == 2)
		memset((char *)buf_start_virt, 0xa5, len);
	*vir_mem = buf_start_virt;
	*phy_mem = buf_start;
	return 0;

error_handle:
	dprint("alloc buf failed! err_val:%d\n", err_val);
	return -1;
}

void _free_buff(unsigned long vir_mem, unsigned long phy_mem,
					unsigned int len, int sec_level, int format)
{
	int iret = 0;

	if (sec_level) {
		if (get_multi_es_buf_switch()) {
			mutex_lock(&dmc_mutex);
			dmc_mem_free(phy_mem, len, sec_level);
			mutex_unlock(&dmc_mutex);
		} else {
			dmc_mem_free(phy_mem, len, sec_level);
		}
		return;
	}

	iret = cache_free(len, phy_mem);
	if (iret == 0)
		return;

	if (pvr_memory_reserved &&
		format == DVR_FORMAT) {
		_free_buff_cma(phy_mem, len);
		return;
	}
	codec_mm_free_for_dma("dmx", phy_mem);
}

static void _bufferid_set_descriptors(struct chan_id *pchan, int buf_num, int eoc)
{
	union mem_desc *desc_ptr = pchan->es_buf_list[buf_num].descs_virt;
	u64 desc_addr = pchan->es_buf_list[buf_num].buf_phy;

	//dprint("%s pchan->id:%d buf_num:%d\n", __func__, pchan->id, buf_num);
	memset(desc_ptr, 0, sizeof(union mem_desc) * DESC_NUM);

	//first descriptor
	if (get_dmx_version() >= 6)
		desc_ptr[0].bits.address_high = (desc_addr >> 32) & 0x3;
	desc_ptr[0].bits.address_low = desc_addr & 0xFFFFFFFF;
	desc_ptr[0].bits.byte_length = pchan->es_buf_list[buf_num].buf_size - DESC_RESERVED_SIZE;

	//second descriptor
	desc_addr += desc_ptr[0].bits.byte_length;
	if (get_dmx_version() >= 6)
		desc_ptr[1].bits.address_high = (desc_addr >> 32) & 0x3;
	desc_ptr[1].bits.address_low = desc_addr & 0xFFFFFFFF;
	desc_ptr[1].bits.byte_length = DESC_RESERVED_SIZE - DESC_LAST_SIZE * 2;

	//third descriptor
	desc_addr += desc_ptr[1].bits.byte_length;
	if (get_dmx_version() >= 6)
		desc_ptr[2].bits.address_high = (desc_addr >> 32) & 0x3;
	desc_ptr[2].bits.address_low = desc_addr & 0xFFFFFFFF;
	desc_ptr[2].bits.byte_length = DESC_LAST_SIZE;

	//forth descriptor
	desc_addr += desc_ptr[2].bits.byte_length;
	if (get_dmx_version() >= 6)
		desc_ptr[3].bits.address_high = (desc_addr >> 32) & 0x3;
	desc_ptr[3].bits.address_low = desc_addr & 0xFFFFFFFF;
	desc_ptr[3].bits.byte_length = DESC_LAST_SIZE;
	desc_ptr[3].bits.loop = eoc & loop_desc;
	desc_ptr[3].bits.eoc = eoc;
}

static int _bufferid_malloc_desc_mem(struct chan_id *pchan,
				     unsigned int mem_size, int sec_level)
{
	unsigned long mem;
	unsigned long mem_phy;
	unsigned long memdescs;
	unsigned long memdescs_phy;
	int ret = 0;

	if ((mem_size >> 27) > 0) {
		dprint("%s fail, need support >=128M bytes\n", __func__);
		return -1;
	}

	// TODO: need to review this
	if (pchan->is_multi_buf) {
		if ((get_multi_es_buf0_size() + get_multi_es_buf1_size() + DESC_RESERVED_SIZE >=
			get_video_buf_size()) || get_multi_es_buf0_size() <= DESC_RESERVED_SIZE ||
			get_multi_es_buf1_size() <= DESC_RESERVED_SIZE) {
			dprint("%s error!!! buf0_size:0x%x buf1_size:0x%x video_buf_size:0x%x\n",
				__func__, get_multi_es_buf0_size(), get_multi_es_buf1_size(),
				get_video_buf_size());
			return -1;
		}
		mem_size = get_multi_es_buf0_size();
		pr_es_dbg("%s multi_es_buf0_size:0x%x\n", __func__, mem_size);
	}

	if (pchan->mode == INPUT_MODE && sec_level != 0) {
		mem = 0;
		mem_size = 0;
		mem_phy = 0;
	} else {
		if (pchan->sec_size) {
			mem_size = pchan->sec_size;
			mem_phy = pchan->sec_mem;
			mem = (unsigned long)phys_to_virt(mem_phy);
			pr_dbg("use sec phy mem:0x%lx, virt:0x%lx size:0x%x\n",
					mem_phy, mem, mem_size);
		} else {
			ret = _alloc_buff(mem_size, sec_level, &mem, &mem_phy, pchan->format);
			if (ret != 0) {
				dprint("%s malloc fail\n", __func__);
				return -1;
			}
			pr_dbg("%s id:%d, mem:0x%lx, mem_phy:0x%lx, mem_size:0x%x, sec:%d\n",
				__func__, pchan->id, mem, mem_phy, mem_size, sec_level);
		}
	}

	pchan->descs_size = pchan->is_multi_buf ?
		(sizeof(union mem_desc) * MAX_ES_BUF_NUM * DESC_NUM) : sizeof(union mem_desc);
	ret = _alloc_buff(pchan->descs_size, 0, &memdescs, &memdescs_phy, pchan->format);
	if (ret != 0) {
		_free_buff(mem, mem_phy, mem_size, sec_level, pchan->format);
		dprint("%s malloc 2 fail\n", __func__);
		return -1;
	}
	pchan->mem = mem;
	pchan->mem_phy = mem_phy;
	pchan->mem_size = mem_size;
	pchan->sec_level = sec_level;
	pchan->memdescs = (union mem_desc *)memdescs;
	pchan->memdescs_phy = memdescs_phy;

	if (get_dmx_version() >= 6)
		pchan->memdescs->bits.address_high = (sizeof(unsigned long) == 8) ?
							((mem_phy >> 32) & 0x3) : 0;
	pchan->memdescs->bits.address_low = mem_phy & 0xFFFFFFFF;
	pchan->memdescs->bits.byte_length = mem_size;
	pchan->memdescs->bits.loop = loop_desc;
	pchan->memdescs->bits.eoc = 1;

	if (pchan->is_multi_buf) {
		pchan->es_buf_list = vmalloc(sizeof(*pchan->es_buf_list) * MAX_ES_BUF_NUM);
		if (!pchan->es_buf_list) {
			_free_buff(memdescs, memdescs_phy, pchan->descs_size, 0, pchan->format);
			_free_buff(mem, mem_phy, mem_size, sec_level, pchan->format);
			dprint("%s vmalloc error!!!\n", __func__);
			return -1;
		}
		memset(pchan->es_buf_list, 0, sizeof(*pchan->es_buf_list) * MAX_ES_BUF_NUM);

		pchan->buf_num = 1;
		pchan->rptr_phy = mem_phy;
		pchan->reduce_counter = 0;
		pchan->max_data_size = 0;

		pchan->es_buf_list[0].allocated = 1;
		pchan->es_buf_list[0].used = 1;
		pchan->es_buf_list[0].buf_size = mem_size;
		pchan->es_buf_list[0].buf_virt = mem;
		pchan->es_buf_list[0].buf_phy = mem_phy;
		pchan->es_buf_list[0].descs_virt = pchan->memdescs;
		pchan->es_buf_list[0].descs_phy = memdescs_phy;
		_bufferid_set_descriptors(pchan, 0, 1);
	}

	dma_sync_single_for_device(aml_get_device(),
		pchan->memdescs_phy, pchan->descs_size, DMA_TO_DEVICE);
	pr_dbg("flush mem descs to ddr\n");
	pr_dbg("%s mem_desc phy addr 0x%lx, memdescs:0x%lx\n", __func__,
	       pchan->memdescs_phy, (unsigned long)pchan->memdescs);

	/*mem from secure os, don't operate this memory*/
	if (pchan->sec_size == 0)
		dma_sync_single_for_device(aml_get_device(),
			mem_phy, mem_size, DMA_TO_DEVICE);

	pchan->r_offset = 0;
	pchan->last_w_addr = 0;
	return 0;
}

int SC2_bufferid_add_buf(struct chan_id *pchan)
{
	unsigned long mem;
	unsigned long mem_phy;
	unsigned int add_buf_size;
	s64 start_time;
	s64 spent_time;
	unsigned long flags;
	u64 now_w = SC2_bufferid_get_wptr(pchan);
	int wptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_w);
	int ret = 0;
	int i;

	// TODO: need to review this
	add_buf_size = (pchan->buf_num == 1) ? get_multi_es_buf1_size() :
			(get_video_buf_size() - pchan->mem_size);
	pr_es_dbg("%s buf_num:%d add_buf_size:0x%x\n", __func__, pchan->buf_num, add_buf_size);

	for (i = 0; i < MAX_ES_BUF_NUM; i++) {
		if (pchan->es_buf_list[i].allocated == 0)
			break;
	}

	if (i >= MAX_ES_BUF_NUM || i <= 0) {
		dprint("%s no more buffer i:%d\n", __func__, i);
		return -3;
	}

	ret = _alloc_buff(add_buf_size, pchan->sec_level, &mem, &mem_phy, pchan->format);
	if (ret != 0) {
		dprint("%s error!!! alloc memory failed\n", __func__);
		return -4;
	}
	dma_sync_single_for_device(aml_get_device(), mem_phy, add_buf_size, DMA_TO_DEVICE);

	// disable irq to avoid interruption
	spin_lock_irqsave(&add_buf_lock, flags);
	start_time = ktime_to_us(ktime_get());
	if ((wptr_buf_num == i - 1) && (now_w > pchan->es_buf_list[wptr_buf_num].buf_phy +
		pchan->es_buf_list[wptr_buf_num].buf_size - DESC_RESERVED_SIZE)) {
		spin_unlock_irqrestore(&add_buf_lock, flags);
		dprint("%s caution!!! wp not available now, now_w:0x%llx\n", __func__, now_w);
		_free_buff(mem, mem_phy, add_buf_size, pchan->sec_level, pchan->format);
		return -5;
	}

	pchan->es_buf_list[i].allocated = 1;
	pchan->es_buf_list[i].used = 0;
	pchan->es_buf_list[i].buf_size = add_buf_size;
	pchan->es_buf_list[i].buf_virt = mem;
	pchan->es_buf_list[i].buf_phy = mem_phy;
	pchan->es_buf_list[i].descs_virt = pchan->es_buf_list[i - 1].descs_virt + DESC_NUM;
	pchan->es_buf_list[i].descs_phy = pchan->es_buf_list[i - 1].descs_phy +
					sizeof(union mem_desc) * DESC_NUM;
	_bufferid_set_descriptors(pchan, i, 1);

	pchan->es_buf_list[i - 1].descs_virt[DESC_NUM - 1].bits.loop = 0;
	pchan->es_buf_list[i - 1].descs_virt[DESC_NUM - 1].bits.eoc = 0;
	pchan->buf_num += 1;
	pchan->mem_size += add_buf_size;
	pchan->reduce_counter = 0;

	dma_sync_single_for_device(aml_get_device(), pchan->memdescs_phy,
		sizeof(union mem_desc) * MAX_ES_BUF_NUM * DESC_NUM, DMA_TO_DEVICE);
	wdma_set_wch_len(pchan->id, pchan->mem_size);
	spent_time = ktime_to_us(ktime_get()) - start_time;
	spin_unlock_irqrestore(&add_buf_lock, flags);

	pr_es_dbg("%s id:%d, mem:0x%lx, mem_phy:0x%lx, add_buf_size:0x%x, sec_level:%d\n",
		__func__, pchan->id, mem, mem_phy, add_buf_size, pchan->sec_level);
	pr_es_dbg("%s spent time: %lld us\n", __func__, spent_time);
	if (spent_time > UPDATE_DESC_TIMEOUT)
		dprint("%s error!!! spent time: %lld us\n", __func__, spent_time);

	return 0;
}

int SC2_bufferid_reduce_buf(struct chan_id *pchan, u64 rp)
{
	s64 start_time;
	s64 spent_time;
	unsigned long flags;
	u64 now_w = SC2_bufferid_get_wptr(pchan);
	u64 now_r = rp ? rp : pchan->rptr_phy;
	int wptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_w);
	int rptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_r);
	int i = pchan->buf_num - 1;

	// disable irq to avoid interruption
	spin_lock_irqsave(&add_buf_lock, flags);
	start_time = ktime_to_us(ktime_get());
	if (wptr_buf_num < rptr_buf_num || wptr_buf_num == i ||
		(wptr_buf_num == rptr_buf_num && now_w < now_r) ||
		((wptr_buf_num == i - 1) && (now_w > pchan->es_buf_list[wptr_buf_num].buf_phy +
		pchan->es_buf_list[wptr_buf_num].buf_size - DESC_RESERVED_SIZE))) {
		spin_unlock_irqrestore(&add_buf_lock, flags);
		pr_dbg("%s unavailable now_w:0x%llx now_r:0x%llx\n", __func__, now_w, now_r);
		return -1;
	}

	pchan->es_buf_list[i].allocated = 0;
	pchan->es_buf_list[i].used = 0;
	pchan->es_buf_list[i - 1].descs_virt[DESC_NUM - 1].bits.loop = loop_desc;
	pchan->es_buf_list[i - 1].descs_virt[DESC_NUM - 1].bits.eoc = 1;
	pchan->buf_num -= 1;
	pchan->mem_size -= pchan->es_buf_list[i].buf_size;
	pchan->reduce_counter = 0;

	dma_sync_single_for_device(aml_get_device(), pchan->memdescs_phy,
		sizeof(union mem_desc) * MAX_ES_BUF_NUM * DESC_NUM, DMA_TO_DEVICE);
	wdma_set_wch_len(pchan->id, pchan->mem_size);
	spent_time = ktime_to_us(ktime_get()) - start_time;
	spin_unlock_irqrestore(&add_buf_lock, flags);

	_free_buff((unsigned long)pchan->es_buf_list[i].buf_virt,
		(unsigned long)pchan->es_buf_list[i].buf_phy, pchan->es_buf_list[i].buf_size,
		pchan->sec_level, pchan->format);
	pr_es_dbg("%s id:%d, mem:0x%lx, mem_phy:0x%lx, buf_size:0x%x, sec_level:%d\n",
		__func__, pchan->id, (unsigned long)pchan->es_buf_list[i].buf_virt,
		(unsigned long)pchan->es_buf_list[i].buf_phy, pchan->es_buf_list[i].buf_size,
		pchan->sec_level);
	pr_es_dbg("%s spent time: %lld us\n", __func__, spent_time);
	if (spent_time > UPDATE_DESC_TIMEOUT)
		dprint("%s error!!! spent time: %lld us\n", __func__, spent_time);

	return 0;
}

static void _bufferid_free_desc_mem(struct chan_id *pchan)
{
	// TODO: watch out the size param
	if (pchan->is_multi_buf) {
		int i;

		pr_es_dbg("%s max_data_size:0x%x\n", __func__, pchan->max_data_size);
		for (i = 0; i < pchan->buf_num; i++) {
			if (pchan->es_buf_list[i].allocated) {
				pr_es_dbg("%s i:%d buf_phy:0x%lx\n", __func__, i,
					pchan->es_buf_list[i].buf_phy);
				_free_buff(pchan->es_buf_list[i].buf_virt,
					pchan->es_buf_list[i].buf_phy,
					pchan->es_buf_list[i].buf_size,
					pchan->sec_level, pchan->format);
				memset(pchan->es_buf_list + i, 0, sizeof(pchan->es_buf_list[i]));
			}
		}

		vfree(pchan->es_buf_list);
		pchan->es_buf_list = NULL;
		pchan->is_multi_buf = 0;
		pchan->buf_num = 0;
	} else {
		if (pchan->mem && pchan->sec_size == 0)
			_free_buff((unsigned long)pchan->mem, (unsigned long)pchan->mem_phy,
				pchan->mem_size, pchan->sec_level, pchan->format);
	}

	if (pchan->memdescs)
		_free_buff((unsigned long)pchan->memdescs, (unsigned long)pchan->memdescs_phy,
			   pchan->descs_size, 0, pchan->format);

	pchan->mem = 0;
	pchan->mem_phy = 0;
	pchan->mem_size = 0;
	pchan->sec_level = 0;
	pchan->memdescs = NULL;
	pchan->memdescs_phy = 0;
	pchan->memdescs_map = 0;
	pchan->sec_size = 0;
}

static int _bufferid_alloc_chan_w_for_es(struct bufferid_attr *attr, struct chan_id **pchan,
					 struct chan_id **pchan1)
{
	int i = 0;

#define ES_BUFF_ID_END  63
	for (i = ES_BUFF_ID_END; i >= 0; i--) {
		struct chan_id *pchan_tmp = &chan_id_table_w[i];
		struct chan_id *pchan1_tmp = &chan_id_table_w[i + 64];

		if (pchan_tmp->used == 0 && pchan1_tmp->used == 0) {
			pchan_tmp->used = 1;
			pchan1_tmp->used = 1;
			pchan_tmp->is_es = 1;
			pchan1_tmp->is_es = 1;
			pchan_tmp->format = attr->format;
			pchan1_tmp->format = attr->format;
			pchan_tmp->is_multi_buf = attr->is_multi_buf;
			*pchan = pchan_tmp;
			*pchan1 = pchan1_tmp;
			break;
		}
	}
	if (i >= 0) {
		dprint("find bufferid %d & %d for es\n", i, i + 64);
		return 0;
	}
	dprint("can't find bufferid for es\n");
	return -1;
}

static int _bufferid_alloc_chan_w_for_ts(struct bufferid_attr *attr, struct chan_id **pchan)
{
	int i = 0;
	struct chan_id *pchan_tmp;

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		pchan_tmp = &chan_id_table_w[i];
		if (pchan_tmp->used == 0) {
			pchan_tmp->used = 1;
			pchan_tmp->format = attr->format;
			break;
		}
	}
	if (i == W_MAX_MEM_CHAN_NUM)
		return -1;
	*pchan = pchan_tmp;
	return 0;
}

static int _bufferid_alloc_chan_r_for_ts(struct bufferid_attr *attr, struct chan_id **pchan)
{
	struct chan_id *pchan_tmp;

	if (attr->req_id >= R_MAX_MEM_CHAN_NUM)
		return -1;
	pchan_tmp = &chan_id_table_r[attr->req_id];
	if (pchan_tmp->used == 1)
		return -1;

	pchan_tmp->used = 1;
	pchan_tmp->format = attr->format;
	*pchan = pchan_tmp;
	return 0;
}

#ifdef CHECK_PACKET_ALIGNM
static void check_packet_alignm(unsigned long start, unsigned long end, int pack_len)
{
	int n = 0;
	char *p = NULL;
	unsigned long reg;
	unsigned int detect_len = end - start;

	if (detect_len % pack_len != 0) {
		dprint_i("len:%d not alignm\n", detect_len);
		return;
	}
	reg = round_down(start, 0x3);
	if (reg != start)
		dprint_i("mem addr not alignm 4\n");

	if (!pfn_valid(__phys_to_pfn(reg)))
		p = (void *)ioremap(reg, 0x4);
	else
		p = (void *)kmap(pfn_to_page(__phys_to_pfn(reg)));
	if (!p) {
		dprint_i("phy transfer to virt fail\n");
		return;
	}
	//detect packet alignm
	for (n = 0; n < detect_len / pack_len; n++) {
		if (p[n * pack_len] != 0x47) {
			dprint_i("packet not alignm at %d,header:0x%0x\n",
				n * pack_len, p[n * pack_len]);
			break;
		}
	}
	if (!pfn_valid(__phys_to_pfn(reg)))
		kunmap(pfn_to_page(__phys_to_pfn(reg)));
}
#endif

/**
 * chan init
 * \retval 0: success
 * \retval -1:fail.
 */
int SC2_bufferid_init(void)
{
	unsigned int dmc_mem_size;
	int i = 0;

	pr_dbg("%s enter\n", __func__);

	for (i = 0; i < W_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_w[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = OUTPUT_MODE;
	}

	for (i = 0; i < R_MAX_MEM_CHAN_NUM; i++) {
		struct chan_id *pchan = &chan_id_table_r[i];

		memset(pchan, 0, sizeof(struct chan_id));
		pchan->id = i;
		pchan->mode = INPUT_MODE;
	}

	// TODO: need to review this
	dmc_mem_size = get_multi_es_buf_switch() ?
		(DMC_MEM_NON_VIDEO_SIZE + get_multi_es_buf0_size()) :
		DMC_MEM_DEFAULT_SIZE;
	pr_es_dbg("%s dmc_mem_size:0x%x\n", __func__, dmc_mem_size);
	for (i = 1; i <= 7; i++)
		dmc_mem_set_size(i, dmc_mem_size);

	mutex_init(&dmc_mutex);

	return 0;
}

/**
 * alloc chan
 * \param attr
 * \param pchan,if ts/es, return this channel
 * \param pchan1, if es, return this channel for pcr
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_alloc(struct bufferid_attr *attr,
		       struct chan_id **pchan, struct chan_id **pchan1)
{
	pr_dbg("%s enter\n", __func__);

	if (attr->mode == INPUT_MODE)
		return _bufferid_alloc_chan_r_for_ts(attr, pchan);

	if (attr->is_es)
		return _bufferid_alloc_chan_w_for_es(attr, pchan, pchan1);
	else
		return _bufferid_alloc_chan_w_for_ts(attr, pchan);
}

/**
 * dealloc chan & free mem
 * \param pchan:struct chan_id handle
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_dealloc(struct chan_id *pchan)
{
	pr_dbg("%s enter\n", __func__);
	_bufferid_free_desc_mem(pchan);
	pchan->is_es = 0;
	pchan->format = 0;
	pchan->used = 0;
	return 0;
}

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param mem_size:used memory
 * \param sec_level: memory security level
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_mem(struct chan_id *pchan,
			 unsigned int mem_size, int sec_level)
{
	pr_dbg("%s mem_size:%d,sec_level:%d\n", __func__, mem_size, sec_level);
	return _bufferid_malloc_desc_mem(pchan, mem_size, sec_level);
}

/**
 * chan mem
 * \param pchan:struct chan_id handle
 * \param sec_mem:direct memory
 * \param sec_size: memory size
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_sec_mem(struct chan_id *pchan,
			 u64 sec_mem, unsigned int sec_size)
{
	pr_dbg("%s sec_mem:0x%llx,sec_size:0x%0x\n", __func__, sec_mem, sec_size);
	pchan->sec_mem = sec_mem;
	pchan->sec_size = sec_size;
	return 0;
}

/**
 * set enable
 * \param pchan:struct chan_id handle
 * \param enable: 1/0
 * \retval success: 0
 * \retval -1:fail.
 */
int SC2_bufferid_set_enable(struct chan_id *pchan, int enable, int sid, int pid)
{
	u64 tmp = 0;

	if (pchan->enable == enable)
		return 0;

	pr_dbg("%s id:%d enable:%d\n", __func__, pchan->id, enable);

	pchan->enable = enable;

	if (get_dmx_version() >= 6)
		tmp = pchan->memdescs->bits.address_high;
	tmp = (tmp << 32) + pchan->memdescs->bits.address_low;
	pr_dbg("WCH_ADDR, buffer id:%d, desc_phy:0x%lx, addr:0x%llx, length:%d\n",
			pchan->id, pchan->memdescs_phy, tmp,
			pchan->memdescs->bits.byte_length);
	wdma_config_enable(pchan, enable, pchan->memdescs_phy, pchan->mem_size,
		sid, pid, pchan->sec_level);

	pr_dbg("err:0x%0x, active:%d\n", wdma_get_err(pchan->id),
	       wdma_get_active(pchan->id));
	pr_dbg("wptr:0x%0llx\n", wdma_get_wptr(pchan->id));
	pr_dbg("wr_len:0x%0x\n", wdma_get_wr_len(pchan->id, NULL));
	return 0;
}

unsigned int SC2_bufferid_get_wp_offset(struct chan_id *pchan)
{
	return (wdma_get_wr_len(pchan->id, NULL) % pchan->mem_size);
}

unsigned int SC2_bufferid_get_data_len(struct chan_id *pchan)
{
	unsigned int w_offset = 0;

	w_offset = wdma_get_wr_len(pchan->id, NULL) % pchan->mem_size;
	if (w_offset >= pchan->r_offset)
		return w_offset - pchan->r_offset;
	else
		return pchan->mem_size - pchan->r_offset + w_offset;
}

/**
 * get which buffer the ptr is in
 * \param pchan:struct chan_id handle
 * \param ptr:ptr
 * \retval >=0:buffer number
 * \retval -1:fail.
 */
int SC2_bufferid_get_ptr_buf(struct chan_id *pchan, u64 ptr)
{
	int i;

	for (i = 0; i < pchan->buf_num; i++) {
		struct multi_es_buf_info *pbuf_info = pchan->es_buf_list + i;

		if (ptr >= pbuf_info->buf_phy && (ptr < pbuf_info->buf_phy + pbuf_info->buf_size))
			return i;
	}

	pr_es_dbg("%s caution!!! pchan->id:%d ptr:0x%llx\n", __func__, pchan->id, ptr);
	for (i = 0; i < pchan->buf_num; i++)
		pr_es_dbg("%s es_buf_list[%d].buf_phy:0x%lx\n", __func__, i,
			pchan->es_buf_list[i].buf_phy);

	return -1;
}

/**
 * get wptr and update it
 * \param pchan:struct chan_id handle
 * \retval:wptr
 */
u64 SC2_bufferid_get_wptr(struct chan_id *pchan)
{
	u64 now_w = wdma_get_wptr(pchan->id);
	int i;

	if (pchan->is_multi_buf) {
		if (now_w == 0)
			now_w = pchan->es_buf_list[0].buf_phy;

		for (i = 0; i < pchan->buf_num; i++) {
			if (now_w == pchan->es_buf_list[i].buf_phy +
				pchan->es_buf_list[i].buf_size) {
				now_w = pchan->es_buf_list[(i + 1) % pchan->buf_num].buf_phy;
				dprint("%s id:%d now_w:0x%llx\n", __func__, pchan->id, now_w);
				break;
			}
		}
	} else {
		if ((now_w == pchan->mem_phy + pchan->mem_size) || now_w == 0) {
			dprint("%s pchan->id:%d now_w:0x%llx mem_phy:0x%lx mem_size:0x%x\n",
				__func__, pchan->id, now_w, pchan->mem_phy, pchan->mem_size);
			now_w = pchan->mem_phy;
		}
	}

	return now_w;
}

/**
 * update rptr
 * \param pchan:struct chan_id handle
 * \param rptr_buf_num:rptr buffer number
 * \retval:rptr
 */
u64 SC2_bufferid_update_rptr(struct chan_id *pchan, int rptr_buf_num, u64 rp)
{
	struct multi_es_buf_info *pbuf_info = pchan->es_buf_list + rptr_buf_num;
	u64 now_w = SC2_bufferid_get_wptr(pchan);
	int wptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_w);
	u64 now_r = rp ? rp : pchan->rptr_phy;

	// TODO: no need to check this, will remove it
	if (now_r > (pbuf_info->buf_phy + pbuf_info->buf_size))
		dprint("%s error!!! now_r:0x%llx buf_phy:0x%lx buf_size:0x%x\n",
			__func__, now_r, pbuf_info->buf_phy, pbuf_info->buf_size);

	if (pchan->es_buf_list[wptr_buf_num].used == 0) {
		pchan->es_buf_list[wptr_buf_num].used = 1;
		pr_es_dbg("%s set used wptr_buf_num:%d\n", __func__, wptr_buf_num);
	}

	if (now_r >= (pbuf_info->buf_phy + pbuf_info->buf_size)) {
		unsigned int rptr_offset = now_r - (pbuf_info->buf_phy + pbuf_info->buf_size);
		int next_buf_num = (rptr_buf_num + 1) % pchan->buf_num;

		// next buffer is added but not used yet
		if (pchan->es_buf_list[next_buf_num].used == 0) {
			pr_es_dbg("%s caution!!! unused id:%d next_buf_num:%d\n",
				__func__, pchan->id, next_buf_num);
			next_buf_num = 0;
		}

		now_r = pchan->es_buf_list[next_buf_num].buf_phy + rptr_offset;
		pr_es_dbg("%s pchan->id:%d next_buf_num:%d now_r:0x%llx now_w:0x%llx\n",
			__func__, pchan->id, next_buf_num, now_r, now_w);
	}

	return now_r;
}

unsigned int SC2_bufferid_get_data_len_multi_buf(struct chan_id *pchan, u64 rp)
{
	u64 now_w = SC2_bufferid_get_wptr(pchan);
	u64 now_r = rp ? rp : pchan->rptr_phy;
	int wptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_w);
	int rptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_r);
	unsigned int data_len;

	if (wptr_buf_num < 0 || rptr_buf_num < 0) {
		dprint("%s error!!! now_w:0x%llx rp:0x%llx rptr_phy:0x%llx\n", __func__,
			now_w, rp, pchan->rptr_phy);
		return 0;
	}

	if (pchan->es_buf_list[wptr_buf_num].used == 0) {
		pchan->es_buf_list[wptr_buf_num].used = 1;
		pr_es_dbg("%s set used wptr_buf_num:%d\n", __func__, wptr_buf_num);
	}

	if (now_w >= now_r && wptr_buf_num == rptr_buf_num) {
		data_len = now_w - now_r;
	} else {
		int next_buf_num = rptr_buf_num;
		int i;

		data_len = pchan->es_buf_list[next_buf_num].buf_phy +
			pchan->es_buf_list[next_buf_num].buf_size - now_r;

		for (i = 0; i < pchan->buf_num; i++) {
			next_buf_num = (next_buf_num + 1) % pchan->buf_num;

			// next buffer is added but not used yet
			if (pchan->es_buf_list[next_buf_num].used == 0) {
				pr_dbg("%s unused id:%d next_buf_num:%d\n",
					__func__, pchan->id, next_buf_num);
				next_buf_num = 0;
			}

			if (next_buf_num == wptr_buf_num) {
				data_len += (now_w - pchan->es_buf_list[next_buf_num].buf_phy);
				break;
			}

			data_len += pchan->es_buf_list[next_buf_num].buf_size;
			pr_dbg("%s id:%d next_buf_num:%d\n", __func__, pchan->id, next_buf_num);
		}
	}

	return data_len;
}

int SC2_bufferid_read_header_again(struct chan_id *pchan, char **pread)
{
	int offset = pchan->r_offset;

	usleep_range(40, 50);
	offset = pchan->r_offset ? (pchan->r_offset - 0x10) : (pchan->mem_size - 0x10);
	dma_sync_single_for_cpu(aml_get_device(),
		(dma_addr_t)((pchan->mem_phy + offset) / 64 * 64), 64, DMA_FROM_DEVICE);

	*pread = (char *)(pchan->mem + offset);
	return 0;
}

/*0:no pts; -1: has pts, not newest; 16: newest pts*/
int SC2_bufferid_read_newest_pts(struct chan_id *pchan, char **pread)
{
	unsigned int w_offset = 0;
	unsigned int w_offset_org = 0;
	unsigned int buf_len = 16;
	int overflow = 0;
	int pts_mem_offset = 0;

	if (!pchan || !pchan->mem_phy) {
		dprint("pchan is null or pchan->mem_phy is 0\n");
		return 0;
	}

	w_offset_org = wdma_get_wr_len(pchan->id, &overflow);
	w_offset = w_offset_org % pchan->mem_size;
	w_offset = w_offset / buf_len * buf_len;

	if (w_offset != pchan->pts_newest_r_offset) {
//		dprint("%s w:0x%0x, r:0x%0x, wr_len:0x%0x\n", __func__,
//		       (u32)w_offset, (u32)(pchan->pts_newest_r_offset),
//		       (u32)w_offset_org);
		if (w_offset == 0)
			pts_mem_offset = pchan->mem_size - buf_len;
		else
			pts_mem_offset = w_offset - buf_len;
		/*get the second to last es header, not the last es header*/
		if (pts_mem_offset == 0)
			pts_mem_offset = pchan->mem_size - buf_len;
		else
			pts_mem_offset = pts_mem_offset - buf_len;

		dma_sync_single_for_cpu(aml_get_device(),
				(dma_addr_t)(pchan->mem_phy + pts_mem_offset),
				buf_len, DMA_FROM_DEVICE);
		*pread = (char *)pchan->mem + pts_mem_offset;
		pchan->pts_newest_r_offset = w_offset;
		return buf_len;
	} else if (w_offset != pchan->r_offset) {
		return -1;
	}
	return 0;
}

/**
 * chan read
 * \param pchan:struct chan_id handle
 * \param pread:data addr
 * \param len:data size
 * \param is_secure:is secure or not
 * \retval >=0:read cnt.
 * \retval -1:fail.
 */
int SC2_bufferid_read(struct chan_id *pchan, char **pread, unsigned int len,
		int is_secure)
{
	unsigned int w_offset = 0;
	unsigned int w_offset_org = 0;
	unsigned int buf_len = len;
	unsigned int data_len = 0;
	int overflow = 0;

	w_offset_org = wdma_get_wr_len(pchan->id, &overflow);
	w_offset = w_offset_org % pchan->mem_size;
//      if (is_buffer_overflow(pchan, w_offset))
//              dprint("chan buffer loop back\n");

	pchan->last_w_addr = w_offset;
	if (w_offset != pchan->r_offset) {
		usleep_range(20, 30);
		pr_dbg("%s w:0x%0x, r:0x%0x, wr_len:0x%0x\n", __func__,
		       (u32)w_offset, (u32)(pchan->r_offset),
		       (u32)w_offset_org);
		if (w_offset > pchan->r_offset) {
			data_len = min((w_offset - pchan->r_offset), buf_len);
			if (!is_secure)
				dma_sync_single_for_cpu(aml_get_device(),
						(dma_addr_t)(pchan->mem_phy +
							      pchan->r_offset),
						data_len, DMA_FROM_DEVICE);
			*pread = (char *)(is_secure ?
					pchan->mem_phy + pchan->r_offset :
					pchan->mem + pchan->r_offset);
			pchan->r_offset += data_len;
			pchan->r_offset = pchan->r_offset % pchan->mem_size;
		} else {
			unsigned int part1_len = 0;

			part1_len = pchan->mem_size - pchan->r_offset;
			data_len = min(part1_len, buf_len);
			*pread = (char *)(is_secure ?
					pchan->mem_phy + pchan->r_offset :
					pchan->mem + pchan->r_offset);
			if (data_len < part1_len) {
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset += data_len;
			} else {
				data_len = part1_len;
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)
							(pchan->mem_phy +
							 pchan->r_offset),
							data_len,
							DMA_FROM_DEVICE);
				pchan->r_offset = 0;
			}
		}
		pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
		return data_len;
	}
	return 0;
}

/**
 * chan read multi buffer
 * \param pchan:struct chan_id handle
 * \param pread:data addr
 * \param len:data size
 * \param is_secure:is secure or not
 * \retval >=0:read cnt.
 * \retval -1:fail.
 */
int SC2_bufferid_read_multi_buf(struct chan_id *pchan, char **pread, unsigned int len,
		int is_secure)
{
	unsigned int buf_len = len;
	unsigned int data_len = 0;
	u64 now_w = SC2_bufferid_get_wptr(pchan);
	int wptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, now_w);
	int rptr_buf_num = SC2_bufferid_get_ptr_buf(pchan, pchan->rptr_phy);
	struct multi_es_buf_info *pbuf_info = pchan->es_buf_list + rptr_buf_num;

	if (now_w != pchan->rptr_phy) {
		usleep_range(20, 30);
		pr_dbg("%s w:0x%llx, r:0x%llx\n", __func__, now_w, pchan->rptr_phy);

		if (now_w > pchan->rptr_phy && wptr_buf_num == rptr_buf_num) {
			data_len = min((now_w - pchan->rptr_phy), buf_len);
			if (!is_secure)
				dma_sync_single_for_cpu(aml_get_device(),
						(dma_addr_t)pchan->rptr_phy,
						data_len, DMA_FROM_DEVICE);
			*pread = (char *)(unsigned long)(is_secure ? pchan->rptr_phy :
				pchan->rptr_phy - pbuf_info->buf_phy + pbuf_info->buf_virt);
			pchan->rptr_phy += data_len;

			// will change buffer soon
			if (now_w + 0x10000 > pbuf_info->buf_phy + pbuf_info->buf_size)
				pr_dbg("%s 1 pchan->id:%d rptr_phy:0x%llx now_w:0x%llx\n",
					__func__, pchan->id, pchan->rptr_phy, now_w);
		} else {
			unsigned int part1_len = 0;

			part1_len = pbuf_info->buf_phy + pbuf_info->buf_size - pchan->rptr_phy;
			data_len = min(part1_len, buf_len);
			*pread = (char *)(unsigned long)(is_secure ? pchan->rptr_phy :
				pchan->rptr_phy - pbuf_info->buf_phy + pbuf_info->buf_virt);

			if (data_len < part1_len) {
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)pchan->rptr_phy,
							data_len, DMA_FROM_DEVICE);
				pchan->rptr_phy += data_len;
				pr_dbg("%s 2 pchan->id:%d rptr_phy:0x%llx now_w:0x%llx\n",
					__func__, pchan->id, pchan->rptr_phy, now_w);
			} else {
				data_len = part1_len;
				if (!is_secure)
					dma_sync_single_for_cpu(aml_get_device(),
							(dma_addr_t)pchan->rptr_phy,
							data_len, DMA_FROM_DEVICE);
				pchan->rptr_phy += data_len;
				pchan->rptr_phy = SC2_bufferid_update_rptr(pchan, rptr_buf_num, 0);
				pr_dbg("%s 3 pchan->id:%d rptr_phy:0x%llx now_w:0x%llx\n\n\n",
					__func__, pchan->id, pchan->rptr_phy, now_w);
			}
		}

		pr_dbg("%s request:%d, ret:%d\n", __func__, len, data_len);
		return data_len;
	}

	return 0;
}

/**
 * SC2_bufferid_move_read_rp
 * \param pchan:struct chan_id handle
 * \param len:length
 * \param flag: 0:rewind; 1:forward
 * \retval 0:success.
 * \retval -1:fail.
 */
int SC2_bufferid_move_read_rp(struct chan_id *pchan, unsigned int len, int flag)
{
	if (flag == 0)
		pchan->r_offset = (pchan->r_offset + pchan->mem_size - len) % pchan->mem_size;
	else
		pchan->r_offset = (pchan->r_offset + len) % pchan->mem_size;
	return 0;
}

void _dump_mem_info(struct chan_id *pchan, int total)
{
	pr_dbg("dump mem info:\n");
	pr_dbg("input data:0x%0x, des len:%d\n", (*(char *)(pchan->mem)), total);
	pr_dbg("desc data:0x%0llx 0x%0llx\n", (*(u64 *)(pchan->memdescs)),
		(*((u64 *)(pchan->memdescs) + 1)));
}

void _dump_rdma_info(struct chan_id *pchan)
{
	pr_dbg("dump rdma info:\n");
	pr_dbg("status:0x%0x\n", rdma_get_status(pchan->id));
	pr_dbg("err:0x%0x, len err:0x%0x, active:%d\n",
		rdma_get_err(), rdma_get_len_err(), rdma_get_active());
	pr_dbg("pkt_sync:0x%0x\n", rdma_get_pkt_sync_status(pchan->id));
	pr_dbg("rptr:0x%0llx\n", rdma_get_rptr(pchan->id));
	pr_dbg("cfg fifo:0x%0x\n", rdma_get_cfg_fifo());
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len)
{
	unsigned int ret;
	u64 tmp;
	unsigned int times = 0;
	struct dmx_sec_ts_data *ts_data;
	struct dmx_sec_ts_data_64bits *ts_data_64bits;
	u64 buf_start;
	u64 buf_end;
	int total = count;
	s64 prev_time_nsec;
	s64 max_timeout_nsec;
	unsigned char *vaddr = 0;
	unsigned long reg = 0;
	unsigned long length = 0;
	struct page *page = NULL;
	int alignment = sizeof(unsigned long);

	pr_dbg("%s start w:%d id:%d, buf_phys:0x%0lx\n", __func__,
			count, pchan->id, (unsigned long)buf_phys);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	if (isphybuf) {
		pchan->enable = 1;
		if (count == sizeof(*ts_data)) {
			ts_data = (struct dmx_sec_ts_data *)buf;
			buf_start = (u64)ts_data->buf_start;
			buf_end = (u64)ts_data->buf_end;
		} else {
			ts_data_64bits = (struct dmx_sec_ts_data_64bits *)buf;
			buf_start = (u64)ts_data_64bits->buf_start;
			buf_end = (u64)ts_data_64bits->buf_end;
		}
#ifdef CHECK_PACKET_ALIGNM
		check_packet_alignm(buf_start, buf_end, pack_len);
#endif
		tmp = (u64)buf_start;
		if (get_dmx_version() >= 6)
			pchan->memdescs->bits.address_high = (tmp >> 32) & 0x3;
		pchan->memdescs->bits.address_low = tmp & 0xFFFFFFFF;
		pchan->memdescs->bits.byte_length = buf_end - buf_start;

		if (dump_input_cb) {
			#ifdef CONFIG_ARM64
			if (pfn_is_map_memory(buf_start >> PAGE_SHIFT)) {
			#else
			if (pfn_valid(buf_start >> PAGE_SHIFT)) {
			#endif
				page = pfn_to_page(buf_start >> PAGE_SHIFT);
				vaddr = kmap(page) + (buf_start & ~PAGE_MASK);
				kasan_disable_current();
				//pr_dbg("%s %d buffers start = %lx phy addr = %lx
				//vir addr = %lx\n",
				//__func__, __LINE__, ts_data.buf_start, tmp, vaddr);
				dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE, vaddr,
				pchan->memdescs->bits.byte_length, &dump_input_head);
				kasan_enable_current();
				kunmap(page);
			} else {
				reg = round_down(buf_start, alignment - 1);
				length = (buf_end - buf_start) + (buf_start - reg);
				vaddr = (void *)ioremap_wc(reg, length);
				if (IS_ERR_OR_NULL(vaddr)) {
					dprint("ioremap_wc fail\n");
				} else {
					//pr_dbg("%s %d buffers start = %lx
					//phy addr = %lx vir addr = %lx\n",
					//__func__, __LINE__, ts_data.buf_start,
					//tmp, vaddr);
					dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE,
						vaddr + (buf_start - reg),
						pchan->memdescs->bits.byte_length,
						&dump_input_head);

					iounmap(vaddr);
				}
			}
		}

		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		total = pchan->memdescs->bits.byte_length;
		rdma_config_enable(pchan, 1, pchan->memdescs_phy, count, total, pack_len);
		pr_dbg("%s isphybuf\n", __func__);
	} else {
		if (dump_input_cb)
			dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE, (char *)buf,
				total, &dump_input_head);

		dma_sync_single_for_device(aml_get_device(),
			(dma_addr_t)buf_phys, total, DMA_TO_DEVICE);

		tmp = (u64)(unsigned long)buf_phys;
		if (get_dmx_version() >= 6)
			pchan->memdescs->bits.address_high = (tmp >> 32) & 0x3;
		pchan->memdescs->bits.address_low = tmp & 0xFFFFFFFF;
		//set desc mem ==len for trigger data transfer.
		pchan->memdescs->bits.byte_length = total;
		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		wmb();	/*Ensure pchan->mem contents visible */

		_dump_mem_info(pchan, total);
		pchan->enable = 1;
		rdma_config_enable(pchan, 1, pchan->memdescs_phy,
				   pchan->mem_size, total, pack_len);
	}
	prev_time_nsec = ktime_to_ns(ktime_get());
	max_timeout_nsec = (s64)write_timeout_ms * 1000 * 1000;
	do {
		usleep_range(100, 200);
	} while (!rdma_get_done(pchan->id) &&
		(ktime_to_ns(ktime_get()) - prev_time_nsec) < max_timeout_nsec);

	ret = rdma_get_rd_len(pchan->id);
	if (ret != total)
		dprint("%s, len not equal,ret:%d,w:%d\n",
		       __func__, ret, total);

	_dump_rdma_info(pchan);

	/*disable */
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);

	rdma_config_ready(pchan->id);
	return count;
}

/**
 * write to channel
 * \param pchan:struct chan_id handle
 * \param buf: data addr
 * \param buf_phys: data phys addr
 * \param  count: write size
 * \param  isphybuf: isphybuf
 * \param  pack_len: 188 or 192
 * \retval -1:fail
 * \retval written size
 */
int SC2_bufferid_non_block_write(struct chan_id *pchan, const char *buf, char *buf_phys,
		       unsigned int count, int isphybuf, int pack_len)
{
	u64 tmp;
	unsigned int times = 0;
	struct dmx_sec_ts_data *ts_data;
	struct dmx_sec_ts_data_64bits *ts_data_64bits;
	u64 buf_start;
	u64 buf_end;
	int total = count;
	unsigned char *vaddr = 0;
	unsigned long reg = 0;
	unsigned long length = 0;
	struct page *page = NULL;
	int alignment = sizeof(unsigned long);

	pr_dbg("%s id:%d start w:%d\n", __func__, pchan->id, count);
	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	times = 0;
	if (isphybuf) {
		pchan->enable = 1;
		if (count == sizeof(*ts_data)) {
			ts_data = (struct dmx_sec_ts_data *)buf;
			buf_start = (u64)ts_data->buf_start;
			buf_end = (u64)ts_data->buf_end;
		} else {
			ts_data_64bits = (struct dmx_sec_ts_data_64bits *)buf;
			buf_start = (u64)ts_data_64bits->buf_start;
			buf_end = (u64)ts_data_64bits->buf_end;
		}
#ifdef CHECK_PACKET_ALIGNM
		check_packet_alignm(buf_start, buf_end, pack_len);
#endif
		tmp = (u64)buf_start;
		if (get_dmx_version() >= 6)
			pchan->memdescs->bits.address_high = (tmp >> 32) & 0x3;
		pchan->memdescs->bits.address_low = tmp & 0xFFFFFFFF;
		pchan->memdescs->bits.byte_length = buf_end - buf_start;

		if (dump_input_cb) {
	#ifdef CONFIG_ARM64
			if (pfn_is_map_memory(buf_start >> PAGE_SHIFT)) {
	#else
			if (pfn_valid(buf_start >> PAGE_SHIFT)) {
	#endif
				page = pfn_to_page(buf_start >> PAGE_SHIFT);
				vaddr = kmap(page) + (buf_start & ~PAGE_MASK);
				kasan_disable_current();
				//pr_dbg("%s %d buffers start = %lx phy addr = %lx
				//vir addr = %lx\n",
				//__func__, __LINE__, ts_data.buf_start, tmp, vaddr);
				dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE, vaddr,
				pchan->memdescs->bits.byte_length, &dump_input_head);
				kasan_enable_current();
				kunmap(page);
			} else {
				reg = round_down(buf_start, alignment - 1);
				length = (buf_end - buf_start) + (buf_start - reg);
				vaddr = (void *)ioremap_wc(reg, length);
				if (IS_ERR_OR_NULL(vaddr)) {
					dprint("ioremap_wc fail\n");
				} else {
					//pr_dbg("%s %d buffers start = %lx
					//phy addr = %lx vir addr = %lx\n",
					//__func__, __LINE__, ts_data.buf_start,
					//tmp, vaddr);
					dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE,
						vaddr + (buf_start - reg),
						pchan->memdescs->bits.byte_length,
						&dump_input_head);

					iounmap(vaddr);
				}
			}
		}

		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		total = pchan->memdescs->bits.byte_length;
		rdma_config_enable(pchan, 1, pchan->memdescs_phy, count, total, pack_len);
		pr_dbg("%s isphybuf\n", __func__);
	} else {
		if (dump_input_cb)
			dump_input_cb(pchan->id, -1, DMX_DUMP_INPUT_TYPE, (char *)buf,
				total, &dump_input_head);

		tmp = (u64)(unsigned long)buf_phys;
		if (get_dmx_version() >= 6)
			pchan->memdescs->bits.address_high = (tmp >> 32) & 0x3;
		pchan->memdescs->bits.address_low = tmp & 0xFFFFFFFF;
		//set desc mem ==len for trigger data transfer.
		pchan->memdescs->bits.byte_length = total;
		dma_sync_single_for_device(aml_get_device(),
			pchan->memdescs_phy, sizeof(union mem_desc),
			DMA_TO_DEVICE);

		wmb();	/*Ensure pchan->mem contents visible */

		_dump_mem_info(pchan, total);
		pchan->enable = 1;
		rdma_config_enable(pchan, 1, pchan->memdescs_phy,
				   pchan->mem_size, total, pack_len);
	}
	return count;
}

/**
 * check write done
 * \param pchan:struct chan_id handle
 * \retval 1:done, 0:not done
 */
int SC2_bufferid_non_block_write_status(struct chan_id *pchan)
{
	return rdma_get_done(pchan->id);
}

/**
 * free channel
 * \param pchan:struct chan_id handle
 * \retval 0:success
 */
int SC2_bufferid_non_block_write_free(struct chan_id *pchan)
{
	/*disable */
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);

	pr_dbg("%s end\n", __func__);
	rdma_config_ready(pchan->id);
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int SC2_bufferid_write_empty(struct chan_id *pchan, int pid)
{
	unsigned int len;
	unsigned int ret;
	char *p;
	int i = 0;
	unsigned int times = 0;
	unsigned long virt = 0;
	unsigned long phys = 0;

#define MAX_EMPTY_PACKET_NUM     4

	ret = cache_malloc(MAX_EMPTY_PACKET_NUM * 188, &virt, &phys);
	if (ret == -1) {
		dprint("%s, fail\n", __func__);
		return -1;
	}

	do {
	} while (!rdma_get_ready(pchan->id) && times++ < 20);

	if (rch_sync_num != rch_sync_num_last) {
		rdma_config_sync_num(rch_sync_num);
		rch_sync_num_last = rch_sync_num;
	}

	p = (char *)virt;

	memset(p, 0, 188);
	p[0] = 0x47;
	p[1] = (pid >> 8) & 0x1f;
	p[2] = pid & 0xff;
	p[3] = 0x20;

	for (i = 1; i < MAX_EMPTY_PACKET_NUM; i++)
		memcpy(p + 188 * i, p, 188);

	len = 188 * MAX_EMPTY_PACKET_NUM;

	dma_sync_single_for_device(aml_get_device(),
		phys, len, DMA_TO_DEVICE);

	if (get_dmx_version() >= 6)
		pchan->memdescs->bits.address_high = (sizeof(unsigned long) == 8) ?
							((phys >> 32) & 0x3) : 0;
	pchan->memdescs->bits.address_low = phys & 0xFFFFFFFF;
	//set desc mem ==len for trigger data transfer.
	pchan->memdescs->bits.byte_length = len;
	dma_sync_single_for_device(aml_get_device(),
		pchan->memdescs_phy, sizeof(union mem_desc),
		DMA_TO_DEVICE);

	wmb();	/*Ensure pchan->mem contents visible */

	_dump_mem_info(pchan, len);
	pchan->enable = 1;
	rdma_config_enable(pchan, 1, pchan->memdescs_phy, len, len, 188);

	do {
	} while (!rdma_get_done(pchan->id));

	ret = rdma_get_rd_len(pchan->id);
	if (ret != len)
		dprint("%s, len not equal,ret:%d,w:%d\n",
	       __func__, ret, len);

	_dump_rdma_info(pchan);

	/*disable */
	rdma_config_enable(pchan, 0, 0, 0, 0, 0);
	rdma_clean(pchan->id);

	rdma_config_ready(pchan->id);
	cache_free(len, phys);
	dprint("%s pid:%d end\n", __func__, pid);
	return len;
}
#endif

int SC2_add_dump_cb(struct list_head *node, dmx_dump_cb cb)
{
	if (cb) {
		dump_input_cb_ref++;
		list_add(node, &dump_input_head);
		if (!dump_input_cb)
			dump_input_cb = cb;
	} else {
		dump_input_cb_ref--;
		list_del(node);
		if (dump_input_cb_ref == 0)
			dump_input_cb = NULL;
	}

	return 0;
}

int mem_desc_debug(int direct, char *param_name, int *param_value)
{
	if (direct) {
		if (!strncmp(param_name, "rch_sync_num", strlen("rch_sync_num")))
			rch_sync_num = *param_value;
		else if (!strncmp(param_name, "dmc_keep_alive", strlen("dmc_keep_alive")))
			dmc_keep_alive = *param_value;
		else if (!strncmp(param_name, "write_timeout_ms", strlen("write_timeout_ms")))
			write_timeout_ms = *param_value;
		else if (!strncmp(param_name, "debug_mem_desc", strlen("debug_mem_desc")))
			debug_mem_desc = *param_value;
		else if (!strncmp(param_name, "loop_desc", strlen("loop_desc")))
			loop_desc = *param_value;
		else if (!strncmp(param_name, "pvr_memory_reserved", strlen("pvr_memory_reserved")))
			pvr_memory_reserved = *param_value;
		else
			return -EINVAL;
	} else {
		if (!strncmp(param_name, "rch_sync_num", strlen("rch_sync_num")))
			*param_value = rch_sync_num;
		else if (!strncmp(param_name, "dmc_keep_alive", strlen("dmc_keep_alive")))
			*param_value = dmc_keep_alive;
		else if (!strncmp(param_name, "write_timeout_ms", strlen("write_timeout_ms")))
			*param_value = write_timeout_ms;
		else if (!strncmp(param_name, "debug_mem_desc", strlen("debug_mem_desc")))
			*param_value = debug_mem_desc;
		else if (!strncmp(param_name, "loop_desc", strlen("loop_desc")))
			*param_value = loop_desc;
		else if (!strncmp(param_name, "pvr_memory_reserved", strlen("pvr_memory_reserved")))
			*param_value = pvr_memory_reserved;
		else
			return -EINVAL;
	}

	return 0;
}
