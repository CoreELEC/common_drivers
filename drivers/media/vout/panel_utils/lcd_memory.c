// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>

#include <linux/compat.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/pm.h>
#include <linux/of_reserved_mem.h>
#include <linux/amlogic/aml_free_reserved.h>

#include <linux/page-flags.h>
#include <linux/mm.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include "panel_utils.h"
/*
 * LCD_ALLOC_FRONT is seek free memory  from low to high
 * LCD_ALLOC_TAIL is seek free memory  from high to low
 * for these options:
 * for lcd probe, may use reserved memory and free while probe done
 * and for this kind of memorys we wish they alloced at high,
 * because we can think of them as unused memorys, and release them later in delay work
 */
#define LCD_ALLOC_FRONT 0 //form low to high
#define LCD_ALLOC_TAIL 1 //from high to low

#define RSVD_MEM_TAIL_SIZE (PAGE_SIZE * 1) ////4k for save uboot used information and args
#define RSVD_MEM_UBOOT_INFO_SIZE (RSVD_MEM_TAIL_SIZE / 2)// for save uboot used information
#define RSVD_MEM_BOOTARGS_SIZE   (RSVD_MEM_TAIL_SIZE - RSVD_MEM_UBOOT_INFO_SIZE)// for args

#define LCD_ATTR_SEC     BIT(1) //secure memory
#define LCD_ATTR_TAIL    BIT(31)

#define LCD_RSVD_MEM_MAGIC       "amlogic_lcd_reserved"
#define LCD_BOOTARGS_IDENTIFIER  "lcd_bootargs"

#define LCD_MEM_MAGIC_LEN 32
#define LCD_MEM_INFO_SIZE 64

#define RSVD_FREE_UNUSED_DLY (10 * 1000)

struct lcd_mem_info_s {
	u32 offset;
	u32 size;
	u32 attr;
	u32 sec_handle;
	char name[LCD_MEM_INFO_SIZE - 16];
}; //64 byte

struct lcd_mem_list_s {
	struct list_head list;
	struct lcd_mem_info_s info;
};

struct lcd_bootargs_head_s {
	unsigned int crc32;
	unsigned int total_size;
	unsigned int args_num;
	unsigned int cur_pos;
	char identifier[16];
};

struct lcd_bootargs_item_s {
	unsigned int data_size;
	unsigned int attr;
	char name[36];
};

struct lcd_rsvd_mem_s {
	char magic[LCD_MEM_MAGIC_LEN];  //magic words fixed 'amlogic_lcd_reserved'
	u32 size;              //total memory size
	u32 allocated_num;     // how many memorys are alloced
	u64 pstart;            //whole reserved memory physic start
	u32 no_map;
	u32 args_num;
	u32 tcon_size;
	u32 free_unused;
	unsigned char *bootargs;
	struct list_head mem_list;
	spinlock_t lock;//
};

struct lcd_rsvd_mem_s *lcd_reserved_memory;
struct delayed_work lcd_rsvd_delay_work;

struct lrm_resource_device_list_s {
	char name[32];
	struct list_head list;
};

DEFINE_SPINLOCK(lrm_res_dev_lock);
LIST_HEAD(lrm_resource_device_list);

/* Use lrm_resource_device_prepare() to record device which maybe use reserved memory
 * and use lrm_resource_device_finish() to tell lrm device has allocated all reserved memorys
 * once all device has finished alloc reserved memory, lrm will release unused memorys to kernel
 */
void lrm_resource_device_prepare(char *name)
{
	struct lrm_resource_device_list_s *res = NULL;

	if (!name)
		return;

	res = kzalloc(sizeof(*res), GFP_KERNEL);
	if (!res)
		return;

	strscpy(res->name, name, 32);
	spin_lock(&lrm_res_dev_lock);
	list_add_tail(&res->list, &lrm_resource_device_list);
	spin_unlock(&lrm_res_dev_lock);

	if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
		LRMPR("lrm device %s add ok", name);
}

void lrm_resource_device_finish(char *name)
{
	struct lrm_resource_device_list_s *res, *tmp;

	if (!name)
		return;

	spin_lock(&lrm_res_dev_lock);
	list_for_each_entry_safe(res, tmp, &lrm_resource_device_list, list) {
		if (strncmp(res->name, name, 32) == 0) {
			list_del(&res->list);
			kfree(res);
			if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
				LRMPR("lrm device %s finish", name);
		}
	}
	spin_unlock(&lrm_res_dev_lock);
}

void lrm_resource_device_show(void)
{
	struct lrm_resource_device_list_s *res = NULL;

	list_for_each_entry(res, &lrm_resource_device_list, list)
		pr_info("lrm registered device: %s", res->name);
}

static inline bool lrm_resource_device_empty(void)
{
	return list_empty(&lrm_resource_device_list);
}

static inline bool lrm_all_device_prepare_done(void)
{
	return lrm_resource_device_empty();
}

/*==============================================================================================*/
static inline struct lcd_rsvd_mem_s *lrm_get(void)
{
	return lcd_reserved_memory;
}

static struct lcd_mem_info_s *lrm_info_get_by_name(char *name)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	struct lcd_mem_list_s *pos;
	struct list_head *head;

	if (!lrm || list_empty(&lrm->mem_list))
		return NULL;

	head = &lrm->mem_list;
	list_for_each_entry(pos, head, list)
		if (strcmp(pos->info.name, name) == 0)
			return &pos->info;

	return NULL;
}

int lrm_get_by_name(char *name, u64 *pa, u32 *size)
{
	struct lcd_mem_info_s *mem_info;
	struct lcd_rsvd_mem_s *lrm = lrm_get();

	mem_info = lrm_info_get_by_name(name);
	if (lrm  && mem_info) {
		if (pa)
			*pa = lrm->pstart + mem_info->offset;
		if (size)
			*size = mem_info->size;

		return 0;
	}

	return -1;
}

/*==============================================================================================*/
int lrm_tee_protect(struct lcd_mem_info_s *mem_info, u32 type, s32 sec)
{
#ifdef CONFIG_AMLOGIC_TEE
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	phys_addr_t paddr;
	u32 size;
	int ret = 0;

	if (!lrm || !mem_info)
		return -1;

	paddr = lrm->pstart + mem_info->offset;
	size = mem_info->size;
	if (sec && !(mem_info->attr & LCD_ATTR_SEC)) {
		/* user flush manually if needed
		 * flush_dcache_range(paddr, size);
		 */
		ret = tee_protect_mem_by_type(type, paddr, size, &mem_info->sec_handle);

		if (ret) {
			LRMERR("%s: protect failed! start:0x%llx, size:0x%x, ret:%d\n",
			       mem_info->name, (u64)paddr, size, ret);
			return -1;
		}
		mem_info->attr |= LCD_ATTR_SEC;
	} else if (!sec && (mem_info->attr & LCD_ATTR_SEC)) {
		tee_unprotect_mem(mem_info->sec_handle);
		mem_info->attr &= ~LCD_ATTR_SEC;
	}
#endif
	return 0;
}

int lrm_tee_protect_by_name(char *name, u32 type, s32 sec)
{
	return lrm_tee_protect(lrm_info_get_by_name(name), type, sec);
}

int lrm_paddr_to_offset(phys_addr_t paddr)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();

	return (lrm && paddr >= lrm->pstart) ? (paddr - lrm->pstart) : -1;
}

/*==============================================================================================*/
void *lrm_phys_to_virt(phys_addr_t paddr, u32 size)
{
	int nr_page, i;
	struct page **pages;
	void *vaddr = NULL;
	struct lcd_rsvd_mem_s *lrm = lrm_get();

	if (!lrm || !paddr)
		return NULL;

	if (lrm->no_map) {
		vaddr = memremap(paddr, size, MEMREMAP_WC);
		if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
			LRMPR("%s memremap pa:0x%llx", __func__, (u64)paddr);
	} else if (PageHighMem(phys_to_page(paddr))) {
		nr_page = DIV_ROUND_UP(size, PAGE_SIZE);
		pages = kmalloc_array(nr_page, sizeof(struct page *), GFP_KERNEL);
		for (i = 0; i < nr_page; i++)
			pages[i] = phys_to_page(paddr + i * PAGE_SIZE);

		vaddr = vmap(pages, nr_page, VM_MAP, PAGE_KERNEL);
		if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
			LRMPR("%s vmap pa;0x%llx %d pages", __func__, (u64)paddr, nr_page);
	} else {
		vaddr = phys_to_virt(paddr);
	}

	return vaddr;
}

phys_addr_t __lrm_phys_alloc(u32 size, u32 align, u32 dir, char *desc)
{
	struct lcd_rsvd_mem_s *lrm;
	struct lcd_mem_list_s *mem, *l = NULL, *r = NULL, s, e;
	struct list_head *head;
	u32 start = 0, temp, tail_rsvd_size = 0;
	u64 pstart, paddr = 0;
	unsigned long flags = 0;

	lrm = lrm_get();
	if (!lrm || !size)
		return 0;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return 0;

	mem->info.size = PAGE_ALIGN(size);
	mem->info.attr = 0;
	if (desc)
		strscpy(mem->info.name, desc, sizeof(mem->info.name));
	align = ALIGN(align, PAGE_SIZE);
	spin_lock_irqsave(&lrm->lock, flags);

	if (lrm->size - tail_rsvd_size < size) {
		kfree(mem);
		spin_unlock_irqrestore(&lrm->lock, flags);
		return 0;
	}

	s.info.offset = 0;
	s.info.size = 0;
	s.info.attr = 0;
	e.info.offset = lrm->size - tail_rsvd_size;
	e.info.size = 0;
	e.info.attr = LCD_ATTR_TAIL;

	head = &lrm->mem_list;
	list_add(&s.list, head);
	list_add_tail(&e.list, head);
	pstart = lrm->pstart;
	if (dir == LCD_ALLOC_FRONT) {
		list_for_each_entry_safe(l, r, head, list) {
			temp = l->info.attr & LCD_ATTR_TAIL;
			if (list_is_last(&l->list, head) || temp)
				goto lrm_alloc_exit;

			start = ALIGN(l->info.offset + l->info.size, align);
			if (start + size <= r->info.offset) {
				mem->info.offset = start;
				paddr = pstart + start;
				lrm->allocated_num++;
				list_add(&mem->list, &l->list);
				goto lrm_alloc_exit;
			}
		}
	} else {
		list_for_each_entry_safe_reverse(r, l, head, list) {
			temp = r->info.attr & LCD_ATTR_TAIL;
			if (list_is_first(&r->list, head) || !temp)
				goto lrm_alloc_exit;

			start = ALIGN_DOWN(r->info.offset - size, align);
			if (start >= l->info.offset + l->info.size) {
				mem->info.offset = start;
				mem->info.attr |= LCD_ATTR_TAIL;
				paddr = pstart + start;
				lrm->allocated_num++;
				list_add(&mem->list, &l->list);
				goto lrm_alloc_exit;
			}
		}
	}

lrm_alloc_exit:
	list_del(head->next);
	list_del(head->prev);
	spin_unlock_irqrestore(&lrm->lock, flags);

	if (!paddr) {
		LRMERR("%s %s size:0x%x, align:%u fail\n", __func__, desc, size, align);
		kfree(mem);
	}

	return (phys_addr_t)paddr;
}

//alloc paddr PAGE_ALIGNED
phys_addr_t lrm_phys_alloc(u32 size, char *desc)
{
	return __lrm_phys_alloc(size, PAGE_SIZE, LCD_ALLOC_FRONT, desc);
}

//alloc paddr PAGE_ALIGNED
phys_addr_t lrm_phys_alloc_tail(u32 size, char *desc)
{
	return __lrm_phys_alloc(size, PAGE_SIZE, LCD_ALLOC_TAIL, desc);
}

//align must be PAGE_ALIGNED
phys_addr_t lrm_phys_alloc_align(u32 size, u32 align, char *desc)
{
	return __lrm_phys_alloc(size, align, LCD_ALLOC_FRONT, desc);
}

//align must be PAGE_ALIGNED
phys_addr_t lrm_phys_alloc_tail_align(u32 size, u32 align, char *desc)
{
	return __lrm_phys_alloc(size, align, LCD_ALLOC_TAIL, desc);
}

/* remove memory node for other to use */
void lrm_phys_free(phys_addr_t paddr)
{
	struct lcd_rsvd_mem_s *lrm;
	struct lcd_mem_list_s *temp, *pos;
	struct list_head *head;
	unsigned long flags = 0;
	unsigned int offset = 0xffffffff;

	lrm = lrm_get();
	if (!lrm ||  list_empty(&lrm->mem_list))
		return;

	offset = lrm_paddr_to_offset(paddr);

	head = &lrm->mem_list;
	spin_lock_irqsave(&lrm->lock, flags);
	list_for_each_entry_safe(pos, temp, head, list) {
		if (pos->info.offset == offset) {
			list_del(&pos->list);
			lrm->allocated_num--;
			if (pos->info.attr & LCD_ATTR_SEC) {
				LRMPR("unprotect before release");
				lrm_tee_protect(&pos->info, 0, 0);
			}
			kfree(pos);
			break;
		}
	}

	spin_unlock_irqrestore(&lrm->lock, flags);
}

/*unmap vaddr is vaddr is mapped by vmap */
void lrm_virt_free(void *vaddr)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (!lrm)
		return;

	if (lrm->no_map)
		memunmap(vaddr);
	else if (is_vmalloc_or_module_addr(vaddr))
		vunmap(addr);
}

/*
 * unmap vaddr if va valid
 * free paddr if pa valid
 */
void lrm_free(void *va, phys_addr_t pa)
{
	if (va)
		lrm_virt_free(va);
	if (pa)
		lrm_phys_free(pa);
}

void *__lrm_alloc(u32 size, phys_addr_t *paddr, u32 align, u32 dir, char *desc)
{
	phys_addr_t pa;
	void *va;

	*paddr = 0;
	pa = __lrm_phys_alloc(size, align, dir, desc);
	va = lrm_phys_to_virt(pa, size);
	if (!va)
		lrm_phys_free(pa);
	*paddr = pa;

	return va;
}

void *lrm_alloc(u32 size, phys_addr_t *paddr, char *desc)
{
	return __lrm_alloc(size, paddr, PAGE_SIZE, LCD_ALLOC_FRONT, desc);
}

void *lrm_alloc_tail(u32 size, phys_addr_t *paddr, char *desc)
{
	return __lrm_alloc(size, paddr, PAGE_SIZE, LCD_ALLOC_TAIL, desc);
}

void *lrm_alloc_align(u32 size, phys_addr_t *paddr, u32 align, char *desc)
{
	return __lrm_alloc(size, paddr, align, LCD_ALLOC_FRONT, desc);
}

void *lrm_alloc_tail_align(u32 size, phys_addr_t *paddr, u32 align, char *desc)
{
	return __lrm_alloc(size, paddr, align, LCD_ALLOC_TAIL, desc);
}

/*==============================================================================================*/
void lrm_show(void)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	struct lcd_mem_list_s *pos;
	struct list_head *head;
	u64 pa;
	u32 offset, size, total = 0, i;
	char *attr[4] = {"no-map", "high-map", "cross-high-map", "linear"};

	if (!lrm)
		return;

	pr_info("mem alloc_num: %u\n", lrm->allocated_num);
	head = &lrm->mem_list;
	list_for_each_entry(pos, head, list) {
		pa = lrm->pstart + pos->info.offset;
		offset = pos->info.offset;
		size = pos->info.size;
		total += size;
		pr_info("pa:0x%llx offset:0x%08x, size:0x%08x attr:0x%08x, %s\n",
			pa, offset, size, pos->info.attr, pos->info.name);
	}

	i = lrm->no_map ? 0 :
		PageHighMem(phys_to_page(lrm->pstart)) ? 1 :
		PageHighMem(phys_to_page(lrm->pstart + lrm->size)) ? 2 :
		3;
	pr_info("\n%s %s pa:0x%llx, memory used: 0x%x/0x%x\n\n",
		lrm->magic, attr[i], lrm->pstart, total, lrm->size);

	lrm_resource_device_show();

	lrm_bootargs_dump();
}

/*==============================================================================================*/
void lrm_release_unused(void)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	struct lcd_mem_list_s *pos;
	struct list_head *head;
	phys_addr_t pstart, pend, ptemp;
	u32 offset = 0;
	unsigned long flags = 0;

	if (!lrm || lrm->no_map)
		return;

	if (!lrm->free_unused) //for debug info
		return;

	spin_lock_irqsave(&lrm->lock, flags);
	head = &lrm->mem_list;
	if (!list_empty(head)) {
		pos = list_last_entry(head, struct lcd_mem_list_s, list);
		offset = PAGE_ALIGN(pos->info.offset + pos->info.size);
	}

	if (offset >= lrm->size || lrm->size - offset < PAGE_SIZE) {
		LRMPR("lcd reserved memory not enough 1 page");
		goto  lrm_release_unused_exit;
	}

	pstart = lrm->pstart + offset;
	pend = lrm->pstart + lrm->size - 1;

	ptemp = ALIGN_DOWN(pend, PAGE_SIZE);
	if (PageHighMem(phys_to_page(ptemp))) {
		while (PageHighMem(phys_to_page(ptemp)) && ptemp >= pstart) {
			free_highmem_page(phys_to_page(ptemp));
			ptemp -= PAGE_SIZE;
		}
		ptemp += PAGE_SIZE;
		LRMPR("free high-map mem addr:0x%llx, size:0x%x",
			(u64)ptemp, (u32)(pend + 1 - ptemp));
		pend = ptemp - 1;
	}
	if (pstart < ptemp) {
		aml_free_reserved_area(__va(pstart), __va(pend + 1), 0,
			"lcd_reserved_memory unused");
		LRMPR("free linear mem addr:0x%llx, size:0x%x",
			(u64)pstart, (u32)(pend + 1 - pstart));
	}

	lrm->size = offset;
	if (lrm->size == 0) {
		kfree(lrm->bootargs);
		spin_unlock_irqrestore(&lrm->lock, flags);
		kfree(lrm);
		lcd_reserved_memory = NULL;
		LRMPR("%s reserved memory no used, deinit", __func__);
		return;
	}

lrm_release_unused_exit:
	spin_unlock_irqrestore(&lrm->lock, flags);
}

static void lcd_rsvd_delayed_work(struct work_struct *p_work)
{
	if (!lrm_all_device_prepare_done()) {
		schedule_delayed_work(&lcd_rsvd_delay_work, msecs_to_jiffies(10 * 1000));
		return;
	}

	lrm_release_unused();
	if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
		lrm_show();
}

/*==============================================================================================*/
static int lrm_bootargs_check(unsigned char *bootargs_mem)
{
	struct lcd_bootargs_head_s *head;
	unsigned int crc;

	head = (struct lcd_bootargs_head_s *)bootargs_mem;
	if (strcmp(head->identifier, LCD_BOOTARGS_IDENTIFIER)) {
		LRMERR("%s: identifier error\n", __func__);
		return -1;
	}
	if (head->args_num) {
		crc = cal_CRC32(0, bootargs_mem + 4, head->total_size - 4);
		if (crc != head->crc32) {
			LRMERR("%s: crc error\n", __func__);
			return -1;
		}
	}

	return 0;
}

static struct lcd_bootargs_item_s *lrm_bootargs_get_item(unsigned char *bootargs_mem,
			const char *name)
{
	struct lcd_bootargs_head_s *head;
	struct lcd_bootargs_item_s *item = NULL;
	unsigned char *p;
	int mem_size = 0, item_size, i;

	if (!bootargs_mem)
		return NULL;

	head = (struct lcd_bootargs_head_s *)bootargs_mem;
	p = bootargs_mem + sizeof(struct lcd_bootargs_head_s);
	for (i = 0; i < head->args_num; i++) {
		item = (struct lcd_bootargs_item_s *)p;
		item_size = (sizeof(struct lcd_bootargs_item_s) + item->data_size);
		mem_size += item_size;
		if (mem_size > head->cur_pos)
			break;

		if (strcmp(item->name, name) == 0)
			return item;

		p += item_size;
	}

	return NULL;
}

void lrm_bootargs_dump(void)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	struct lcd_bootargs_head_s *head;
	struct lcd_bootargs_item_s *item = NULL;
	unsigned char *p;
	int mem_size = 0, item_size, i;

	if (!lrm || !lrm->bootargs)
		return;

	pr_info("\nbootargs num:%u\n", lrm->args_num);
	if (lrm_bootargs_check(lrm->bootargs))
		return;

	head = (struct lcd_bootargs_head_s *)lrm->bootargs;
	p = lrm->bootargs + sizeof(struct lcd_bootargs_head_s);
	for (i = 0; i < head->args_num; i++) {
		item = (struct lcd_bootargs_item_s *)p;
		item_size = (sizeof(struct lcd_bootargs_item_s) + item->data_size);
		mem_size += item_size;
		if (mem_size > head->cur_pos)
			break;

		pr_info("offset:0x%x, data_size:%d, attr:0x%x, %s\n",
			(unsigned int)(p - lrm->bootargs),
			item->data_size, item->attr, item->name);

		p += item_size;
	}

	pr_info("\n%s: mem used:0x%x/0x%x\n", head->identifier, head->cur_pos, head->total_size);
}

unsigned char *lrm_bootargs_get_data(const char *name, u32 *len)
{
	struct lcd_rsvd_mem_s *lrm = lrm_get();
	struct lcd_bootargs_item_s *item = NULL;
	unsigned char *data = NULL;

	if (!lrm || !lrm->bootargs || !name || !len)
		return NULL;

	if (lrm_bootargs_check(lrm->bootargs))
		return NULL;

	item = lrm_bootargs_get_item(lrm->bootargs, name);
	if (item) {
		data = ((unsigned char *)item) + sizeof(struct lcd_bootargs_item_s);
		*len = item->data_size;
	}

	return data;
}

struct lrm_transmit_mem_s {
	char *name;
	unsigned char *mem;
	unsigned int size;
};

/* transmit_mems: for uboot transmit datas to kernel
 * "panel_config": panel parameter sets, include ukey data, panel_json file, tcon_bin ...
 *	you can put max 63 data sets to "panel_config" use panel_param_mem_put() in uboot
 *	and use panel_param_mem_get() in kernel to get datas
 * you can add new transmit data declared as {.name = "your_data", .mem = NULL, .size = 0}
 * and driver will parse mem addr and size, and other drivers (like lcd_driver)
 * can get this memory by func: vaddr = lcd_transmit_mem_get("your_data", &size);
 * and lcd_transmit_mem_release("your_data"); to release this memory
 * if no longer needed to give it back to system
 */
static struct lrm_transmit_mem_s lrm_transmit_mems[] = {
	{.name = "panel_config", .mem = NULL, .size = 0},
};

static void lrm_transmit_mem_match_load(struct lcd_rsvd_mem_s *lrm, struct lcd_mem_info_s *info)
{
	unsigned char *vaddr = NULL;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lrm_transmit_mems); i++) {
		if (strcmp(info->name, lrm_transmit_mems[i].name) == 0) {
			vaddr = lrm_phys_to_virt(lrm->pstart + info->offset, info->size);
			if (vaddr) {
				lrm_transmit_mems[i].mem = kmalloc(info->size, GFP_KERNEL);
				if (lrm_transmit_mems[i].mem) {
					memcpy(lrm_transmit_mems[i].mem, vaddr, info->size);
					lrm_transmit_mems[i].size = info->size;
				}
				lrm_virt_free(vaddr);
			}
		}
	}
}

static void lrm_transmit_mem_prefetch(struct lcd_rsvd_mem_s *lrm)
{
	struct lcd_mem_list_s *pos;

	if (!lrm || list_empty(&lrm->mem_list))
		return;

	list_for_each_entry_reverse(pos, &lrm->mem_list, list) {
		if (pos->info.attr & LCD_ATTR_TAIL)
			lrm_transmit_mem_match_load(lrm, &pos->info);
	}
}

unsigned char *lcd_transmit_mem_get(char *name, u32 *size)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lrm_transmit_mems); i++) {
		if (strcmp(name, lrm_transmit_mems[i].name) == 0) {
			if (lrm_transmit_mems[i].mem) {
				*size = lrm_transmit_mems[i].size;
				return lrm_transmit_mems[i].mem;
			}
		}
	}

	return NULL;
}

void lcd_transmit_mem_release(char *name)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(lrm_transmit_mems); i++) {
		if (strcmp(name, lrm_transmit_mems[i].name) == 0) {
			kfree(lrm_transmit_mems[i].mem);
			lrm_transmit_mems[i].mem = NULL;
			lrm_transmit_mems[i].size = 0;
		}
	}
}

/*==============================================================================================*/
/*
 *|used1****|used2*****|free--------------------------------|--4k for pass param end--|
 *last 4k for passing parameters from bootloader to kernel, freed once parameters are acquired
 * used memory may be in continuous used in bootloader and kernel
 */
int lrm_update_bootloader(struct lcd_rsvd_mem_s *lrm)
{
	char *p = NULL, *va = NULL;
	int i = 0;
	unsigned int t_size = 0, used_num = 0, pstart = 0, args_num = 0;
	struct lcd_mem_list_s *mem = NULL, *temp;

	if (!lrm)
		return -1;

	va = lrm_phys_to_virt(lrm->pstart + lrm->size - RSVD_MEM_TAIL_SIZE, RSVD_MEM_TAIL_SIZE);
	if (!va)
		return -1;

	if (strncmp(va, lrm->magic, strlen(lrm->magic))) {
		LRMERR("%s rsvd magic not matched [%s-%s]\n", __func__, va, LCD_RSVD_MEM_MAGIC);
		return -1;
	}

	p = va + LCD_MEM_MAGIC_LEN;//32 byte for magic
	t_size = *(u32 *)(p + 0);
	used_num = *(u32 *)(p + 4);
	pstart = *(u64 *)(p + 8);
	args_num = *(u32 *)(p + 16);

	if (pstart != lrm->pstart || t_size != lrm->size) {
		LRMERR("%s uboot and kernel not match", __func__);
		return -1;
	}

	p = va + 64; //first 64 byte for total information
	for (i = 0; i < used_num && p - va < RSVD_MEM_UBOOT_INFO_SIZE; i++) {
		mem = kzalloc(sizeof(*mem), GFP_KERNEL);
		if (!mem) {
			p += LCD_MEM_INFO_SIZE;
			continue;
		}
		memcpy(&mem->info, p, sizeof(mem->info));
		list_add_tail(&mem->list, &lrm->mem_list);
		if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_MEM)
			LRMPR("lcd rsvd mem used by bootloader: pa:0x%llx, size:0x%x, name:%s",
				 lrm->pstart + mem->info.offset, mem->info.size, mem->info.name);
		lrm->allocated_num++;
		p += LCD_MEM_INFO_SIZE;
	}

	if (args_num) {
		p = va + RSVD_MEM_UBOOT_INFO_SIZE;
		if (lrm_bootargs_check(p) == 0) {
			lrm->bootargs = kzalloc(RSVD_MEM_BOOTARGS_SIZE, GFP_KERNEL);
			if (lrm->bootargs) {
				lrm->args_num = args_num;
				memcpy(lrm->bootargs, p, RSVD_MEM_BOOTARGS_SIZE);
			}
		}
	}
	memset(va, 0, RSVD_MEM_TAIL_SIZE);//merge tail memory to free memory
	lrm_virt_free(va);

	/* here copy uboot alloc tail memorys, and will free these kind of memorys
	 * tcon_bin_path/panel_parameter/tcon_data_bin used in uboot....
	 */
	lrm_transmit_mem_prefetch(lrm);
	lcd_panel_file_pre_proc();

	/* uboot alloc tail memory free (transmit mems)*/
	list_for_each_entry_safe_reverse(mem, temp, &lrm->mem_list, list) {
		if (mem->info.attr & LCD_ATTR_TAIL) {
			list_del(&mem->list);
			kfree(mem);
			lrm->allocated_num--;
		}
	}

	return 0;
}

int lcd_reserved_memory_init(struct platform_device *pdev)
{
	phys_addr_t paddr;
	u32 size;
	struct device_node *np;
	struct reserved_mem *rmem = NULL;
	struct lcd_rsvd_mem_s *lrm = NULL;

	if (!pdev || lcd_reserved_memory)
		return 0;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np || !of_device_is_available(np)) {
		LRMPR("%s can't find lcd-reserved node", __func__);
		return -ENODEV;
	}

	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		LRMPR("%s can't find lcd-reserved", __func__);
		return -EINVAL;
	}

	paddr = PAGE_ALIGN(rmem->base);
	size = PAGE_ALIGN(rmem->size);

	lrm = kzalloc(sizeof(*lrm), GFP_KERNEL);
	if (lrm)
		lcd_reserved_memory = lrm;
	else
		return -1;

	lrm->no_map = of_find_property(np, "no-map", NULL) ? 1 : 0;
	lrm->free_unused = of_find_property(pdev->dev.of_node, "no_free_rsvd", NULL) ? 0 : 1;

	if (paddr > PAGE_SIZE && size > PAGE_SIZE) {
		lrm->size = size;
		lrm->pstart = paddr;
		lrm->allocated_num = 0;
		strscpy(lrm->magic, LCD_RSVD_MEM_MAGIC, LCD_MEM_MAGIC_LEN);
		INIT_LIST_HEAD(&lrm->mem_list);
		LRMPR("%s rsvd mem paddr:0x%llx, size:0x%x", __func__, (u64)paddr, lrm->size);
	} else {
		kfree(lrm);
		lcd_reserved_memory = NULL;
		LRMERR("%s map paddr: 0x%llx failed", __func__, (u64)paddr);
		return -1;
	}

	lrm_update_bootloader(lrm);

	INIT_DELAYED_WORK(&lcd_rsvd_delay_work, lcd_rsvd_delayed_work);
	schedule_delayed_work(&lcd_rsvd_delay_work, msecs_to_jiffies(RSVD_FREE_UNUSED_DLY));

	return 0;
}

bool lrm_no_map(void)
{
	return lrm_get() ? lrm_get()->no_map : false;
}

int lrm_exist(void)
{
	return  lrm_get() ? 1 : 0;
}

u8 *lcd_vmap(ulong addr, u32 size)
{
	u8 *vaddr = NULL;
	struct page **pages = NULL;
	u32 i, npages, offset = 0;
	ulong phys, page_start;
	/*pgprot_t pgprot = pgprot_noncached(PAGE_KERNEL);*/
	pgprot_t pgprot = PAGE_KERNEL;

	if (!PageHighMem(phys_to_page(addr)))
		return phys_to_virt(addr);

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

	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		// LCD_ERR(NULL, "the phy(%lx) vmaped fail, size: %d\n",
		//        page_start, npages << PAGE_SHIFT);
		pr_err("the phy(%lx) vmaped fail, size: %d\n",
		       page_start, npages << PAGE_SHIFT);
		kfree(pages);
		return NULL;
	}
	kfree(pages);

	//if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL) {
	//	LCD_PR(NULL, "[lcd HIGH-MEM-MAP] %s, pa(%lx) to va(%p), size: %d\n",
	//	      __func__, page_start, vaddr, npages << PAGE_SHIFT);
	//}
	pr_debug("[lcd HIGH-MEM-MAP] %s, pa(%lx) to va(%p), size: %d\n",
		__func__, page_start, vaddr, npages << PAGE_SHIFT);
	return vaddr + offset;
}

void lcd_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	if (is_vmalloc_or_module_addr(vaddr)) {
		//if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL)
		//	LCD_PR(NULL, "lcd unmap v: %p\n", addr);
		pr_debug("lcd unmap v: %p\n", addr);
		vunmap(addr);
	}
}

