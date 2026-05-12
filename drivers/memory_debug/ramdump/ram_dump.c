// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include <linux/version.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/reboot.h>
#include <linux/memblock.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/ramdump.h>
#include <linux/amlogic/reboot.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/capability.h>
#include <asm/cacheflush.h>
#include <linux/amlogic/gki_module.h>
#include <trace/hooks/debug.h>
#include <linux/panic_notifier.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
#include "../../crypto/aml-crypto-dma.h"
#endif
#include <linux/string.h>
#include <linux/crypto.h>
#include <crypto/hash.h>

#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
struct ramdump_dma_info {
	u32 rsv_base;               // Reserved base address for memory dump
	u32 rsv_size;               // Reserved size for memory dump
	void __iomem *reg_base;     // DMA register base address (from dts)
	void __iomem *reg_virt_t0;  // DMA register for TX ready
	void __iomem *reg_virt_sts0;// DMA register for TX status
	struct dma_dsc *dsc;        // DMA descriptor buffer pointer
	dma_addr_t paddr;           // Physical address of descriptor buffer
	struct device *dev;         // Pointer to the device structure
};

struct ramdump_dma_info dma_t;
#define DMA_STS0_READY          0xf
#define DMA_EOC_FLAG            0x2
#define REG_OFFSET_T0           (TXLX_DMA_T0)
#define REG_OFFSET_STS0         (TXLX_DMA_STS0)
#endif

static unsigned long ramdump_base;
static unsigned long ramdump_size;
static bool ramdump_disable			=	1;
void __iomem *cfg_sticky_reg;   // REG_MDUMP_CPUBOOT_STATUS

#define WAIT_TIMEOUT		(40ULL * 1000 * 1000 * 1000)
#define SAVE_DATA_BY_INIT_RC_SHELL			1

struct ramdump {
	unsigned long       mem_size;
	unsigned long       mem_base;
#ifdef SAVE_DATA_BY_INIT_RC_SHELL
	struct mutex        read_lock;  // Protects ramdump read operations
	struct kobject      *kobj;
	loff_t              last_logged_off;
	phys_addr_t         window_paddr;
	void                *window_vaddr;
	size_t              window_size;
#endif
	struct delayed_work work;
	int                 disable;
};

static struct ramdump *ram;
char ramdump_info[256];

static void ramdump_parse_info(void)
{
#if IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_DEBUG)
	pr_info("ramdump .text : 0x%px - 0x%px, pa(.text): 0x%lx\n",
		(unsigned long *)_text,
		(unsigned long *)_etext, (unsigned long)__pa_symbol(_text));
#endif

#ifdef CONFIG_ARM64
	pr_info("%s, -m kimage_voffset=0x%llx\n", __func__, kimage_voffset);
	pr_info("%s, -m vabits_actual=%d\n", __func__, (unsigned int)vabits_actual);
#if IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_DEBUG)
	pr_info("%s, --kaslr 0x%lx\n", __func__, kaslr_offset());
#endif
	snprintf(ramdump_info, sizeof(ramdump_info),
			"aml_ramdump_kimage_voffset=0x%llx\n"
			"aml_ramdump_vabits_actual=%d\n"
#if IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_DEBUG)
			"aml_ramdump_kaslr=0x%lx\n"
#endif
			"aml_ramdump_build_time=%s\n",
			kimage_voffset, (unsigned int)vabits_actual,
#if IS_BUILTIN(CONFIG_AMLOGIC_MEMORY_DEBUG)
			kaslr_offset(),
#endif
			BUILD_TIME);
#endif
}

static int early_ramdump_para(char *buf)
{
	int ret;

	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "disabled") == 0) {
		ramdump_disable = 1;
	} else {
		ret = sscanf(buf, "%lx,%lx", &ramdump_base, &ramdump_size);
		if (ret != 2) {
			ramdump_disable = 1;
		}
		ramdump_disable = 0;
		ramdump_parse_info();
	}
	return 0;
}

early_param("ramdump", early_ramdump_para);

#ifdef SAVE_DATA_BY_INIT_RC_SHELL
#define DUMP_LOG_STEP      (200 * 1024 * 1024ULL)
#define MAP_WINDOW_SIZE    (4 * 1024 * 1024)
static ssize_t ramdump_bin_read(struct file *filp, struct kobject *kobj,
				struct bin_attribute *attr,
				char *buf, loff_t off, size_t count)
{
	void *p = NULL;

	if (!ram->mem_base || off >= ram->mem_size) {
		pr_info("crash sysfsnode data err.\n");
		return 0;
	}

	if (off + count > ram->mem_size)
		count = ram->mem_size - off;

#ifndef CONFIG_ARM64
	if (!ram->window_vaddr ||
		(ramdump_base + off) < ram->window_paddr ||
		(ramdump_base + off + count - 1) >= (ram->window_paddr + ram->window_size)) {
		if (ram->window_vaddr) {
			memunmap(ram->window_vaddr);
			ram->window_vaddr = NULL;
		}

		ram->window_paddr = ramdump_base + ALIGN_DOWN(off, MAP_WINDOW_SIZE);
		ram->window_size  = MAP_WINDOW_SIZE;
		ram->window_vaddr  = memremap(ram->window_paddr, ram->window_size, MEMREMAP_WB);
		if (!ram->window_vaddr)
			return -ENOMEM;
	}

	p = (void *)(ram->window_vaddr + (ramdump_base + off - ram->window_paddr));
#else
	p = (void *)(ram->mem_base + off);
#endif

	mutex_lock(&ram->read_lock);
	memcpy(buf, p, count);
	mutex_unlock(&ram->read_lock);

	/* Log when offset moves forward by 200MB */
	if (off >= ram->last_logged_off + DUMP_LOG_STEP) {
		pr_info("read offset: 0x%llx (%llu MB) count: %zu\n",
			(unsigned long long)off,
			(unsigned long long)(off >> 20), count);
		ram->last_logged_off = off;
	}

	/* Log once at the end of the dump */
	if (off + count >= ram->mem_size)
		pr_info("read finished. Total: 0x%llx (%llu MB)\n",
			(unsigned long long)ram->mem_size,
			(unsigned long long)(ram->mem_size >> 20));

	return count;
}

static void meson_set_reboot_reason(int reboot_reason)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(SET_REBOOT_REASON,
		      reboot_reason, 0, 0, 0, 0, 0, 0, &smccc);
}

static ssize_t ramdump_bin_write(struct file *filp,
				 struct kobject *kobj,
				 struct bin_attribute *bin_attr,
				 char *buf, loff_t off, size_t count)
{
	if (ram->mem_base && !strncmp("reboot", buf, 6))
		kernel_restart("RAM-DUMP finished\n");

	if (!strncmp("disable", buf, 7)) {
		ram->disable = 1;
		meson_set_reboot_reason(MESON_NORMAL_BOOT);
	}
	if (!strncmp("enable", buf, 6)) {
		ram->disable = 0;
		meson_set_reboot_reason(MESON_KERNEL_PANIC);
	}

	return count;
}

static struct bin_attribute ramdump_attr = {
	.attr = {
		.name = "compmsg",
		.mode = 0664,
	},
	.read  = ramdump_bin_read,
	.write = ramdump_bin_write,
};

#endif /* end SAVE_DATA_BY_INIT_RC_SHELL */

int ramdump_disabled(void)
{
	if (ram)
		return ram->disable;
	return 0;
}
EXPORT_SYMBOL(ramdump_disabled);

#ifdef CONFIG_ARM64
/* arm64: kill flush_cache_all() 68234df4ea79
 * Flush the whole D-cache.
 * Corrupted registers: x0-x7, x9-x12, x16
 */
noinline void ramdump_sync_data(void)
{
	asm volatile
		("sub sp, sp, #0x70\n"			//save corrupted registers: x0-x7, x9-x12, x16
		"str x0, [sp]\n"
		"str x1, [sp,#8]\n"
		"str x2, [sp,#16]\n"
		"str x3, [sp,#24]\n"
		"str x4, [sp,#32]\n"
		"str x5, [sp,#40]\n"
		"str x6, [sp,#48]\n"
		"str x7, [sp,#56]\n"
		"str x9, [sp,#64]\n"
		"str x10, [sp,#72]\n"
		"str x11, [sp,#80]\n"
		"str x12, [sp,#88]\n"
		"str x16, [sp,#96]\n"

		"mov	x1, #0\n"			//only flush cache
		"dsb	sy\n"
		"mrs	x10, clidr_el1\n"		/* read clidr_el1 */
		"ubfx	x11, x10, #24, #3\n"		/* x11 <- loc */
		"cbz	x11, finished\n"		/* if loc is 0, exit */
		"mrs	x16, s3_0_c0_c7_2\n"		/* read value of id_aa64mmfr2_el1*/
		"ubfx	x16, x16, #20, #4\n"		/* save FEAT_CCIDX identifier in x16 */
		"mov	x0, #0\n"			/* start flush at cache level 0 */
		/* x0  <- cache level */
		/* x10 <- clidr_el1 */
		/* x11 <- loc */
		/* x15 <- return address */

	"loop_level:\n"
		"add	x12, x0, x0, lsl #1\n"		/* x12 <- tripled cache level */
		"lsr	x12, x10, x12\n"
		"and	x12, x12, #7\n"			/* x12 <- cache type */
		"cmp	x12, #2\n"
		"b.lt	skip\n"				/* skip if no cache or icache */

		"lsl   x12, x0, #1\n"
		"msr    csselr_el1, x12\n"              /* select cache level */
		"isb\n"                                 /* sync change of cssidr_el1 */
		"mrs    x6, ccsidr_el1\n"               /* read the new cssidr_el1 */
		"ubfx   x2, x6,  #0,  #3\n"             /* x2 <- log2(cache line size)-4 */
		"cbz    x16, 3f\n"                      /* check for FEAT_CCIDX */
		"ubfx   x3, x6,  #3, #21\n"             /* x3 <- number of cache ways - 1 */
		"ubfx   x4, x6, #32, #24\n"             /* x4 <- number of cache sets - 1 */
		"b      4f\n"
	"3:\n"
		"ubfx   x3, x6,  #3, #10\n"             /* x3 <- number of cache ways - 1 */
		"ubfx   x4, x6, #13, #15\n"             /* x4 <- number of cache sets - 1 */
	"4:\n"
		"add    x2, x2, #4\n"                   /* x2 <- log2(cache line size) */
		"clz    w5, w3\n"                       /* bit position of #ways */
		/* x12 <- cache level << 1 */
		/* x2 <- line length offset */
		/* x3 <- number of cache ways - 1 */
		/* x4 <- number of cache sets - 1 */
		/* x5 <- bit position of #ways */
	"loop_set:\n"
		"mov    x6, x3\n"                       /* x6 <- working copy of #ways */
	"loop_way:\n"
		"lsl    x7, x6, x5\n"
		"orr    x9, x12, x7\n"                  /* map way and level to cisw value */
		"lsl    x7, x4, x2\n"
		"orr    x9, x9, x7\n"                   /* map set number to cisw value */
		"tbz    w1, #0, 1f\n"
		"dc     isw, x9\n"
		"b      2f\n"
	"1:\n"
		"dc     cisw, x9\n"                     /* clean & invalidate by set/way */
	"2:\n"
		"subs   x6, x6, #1\n"                   /* decrement the way */
		"b.ge   loop_way\n"
		"subs   x4, x4, #1\n"                   /* decrement the set */
		"b.ge   loop_set\n"

	"skip:\n"
		"add	x0, x0, #1\n"			/* increment cache level */
		"cmp	x11, x0\n"
		"b.gt	loop_level\n"

		"mov	x0, #0\n"
		"msr	csselr_el1, x0\n"		/* restore csselr_el1 */
		"dsb	sy\n"
		"isb\n"

	"finished:\n"
		"ldr x0, [sp]\n"			//restore corrupted registers: x0-x7, x9-x12
		"ldr x1, [sp,#8]\n"
		"ldr x2, [sp,#16]\n"
		"ldr x3, [sp,#24]\n"
		"ldr x4, [sp,#32]\n"
		"ldr x5, [sp,#40]\n"
		"ldr x6, [sp,#48]\n"
		"ldr x7, [sp,#56]\n"
		"ldr x9, [sp,#64]\n"
		"ldr x10, [sp,#72]\n"
		"ldr x11, [sp,#80]\n"
		"ldr x12, [sp,#88]\n"
		"ldr x16, [sp,#96]\n"
		"add sp, sp, #0x70\n"
	);
}
#else
noinline void ramdump_sync_data(void)
{
	flush_cache_all();
}
#endif

/*
 * clear memory to avoid large amount of memory not used.
 * for random data, it's hard to compress
 */
static void lazy_clear_work(struct work_struct *work)
{
	struct page *page;
	struct list_head head, *pos, *next;
	void *virt;
	int order;
	gfp_t flags = __GFP_NORETRY		|
					__GFP_NOWARN	|
					__GFP_MOVABLE;
	unsigned long clear = 0, size = 0, free = 0, tick;
	unsigned long free_pages;
	unsigned long target_size;

	free_pages = global_zone_page_state(NR_FREE_PAGES);
	target_size = (free_pages * 90) / 100 * PAGE_SIZE;

	INIT_LIST_HEAD(&head);
	order = 7; /* alloc size = 128 * 4KB = 512KB*/
	tick = sched_clock();
	do {
		page = alloc_pages(flags, order);
		if (page) {
			list_add(&page->lru, &head);
			virt = page_address(page);
			size = (1 << order) * PAGE_SIZE;
			memset(virt, 0, size);
			clear += size;
		}
		if (clear > target_size)
			break;
	} while (page);
	tick = sched_clock() - tick;

	list_for_each_safe(pos, next, &head) {
		page = list_entry(pos, struct page, lru);
		list_del(&page->lru);
		__free_pages(page, order);
		free += size;
	}
	pr_info("ramdump, available: %lu MB, clear:%lu MB, free:%lu MB, tick:%ld ms\n",
			free_pages * PAGE_SIZE / 1024 / 1024, clear / 1024 / 1024,
			free / 1024 / 1024, tick / 1000000);
}

#ifdef CONFIG_ANDROID_VENDOR_HOOKS
static void flush_all_cache_hook(void *data, struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	pr_info("ramdump: ONLINE CPU-%d flush cache ...\n", cpu);
	ramdump_sync_data();
	pr_info("ramdump: ONLINE CPU-%d flush cache finish.\n", cpu);
}
#endif

#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
static int ramdump_dma_init(struct device *dev)
{
	struct device_node *node;
	struct reserved_mem *rmem;
	struct resource res;

	/* find dts node: /reserved-memory/ramdump_bl33z */
	node = of_find_node_by_name(NULL, "ramdump_bl33z");
	if (!node) {
		return -EINVAL;
	}

	if (!of_device_is_available(node))
		return -ENODEV;

	rmem = of_reserved_mem_lookup(node);
	if (rmem) {
		dma_t.rsv_base = rmem->base;
		dma_t.rsv_size = rmem->size;
	} else {
		pr_err("%s, Failed to get resource from node ramdump_bl33z.\n", __func__);
		return -EINVAL;
	}

	/* find dts node: /aml_dma */
	node = of_find_node_by_name(NULL, "aml_dma");
	if (!node) {
		return -EINVAL;
	}

	if (!of_device_is_available(node))
		return -ENODEV;

	if (of_address_to_resource(node, 0, &res)) {
		of_node_put(node);
		return -EINVAL;
	}

	dma_t.reg_base = devm_ioremap(dev, res.start, resource_size(&res));
	if (!dma_t.reg_base) {
		of_node_put(node);
		return -ENOMEM;
	}

	pr_info("DMA register base address: %p, REG_OFFSET_STS0=0x%x\n",
			dma_t.reg_base, REG_OFFSET_STS0);
	dma_t.reg_virt_t0 = dma_t.reg_base + REG_OFFSET_T0 * 4;
	dma_t.reg_virt_sts0 = dma_t.reg_base + REG_OFFSET_STS0 * 4;

	dma_t.dsc = dma_alloc_coherent(dev, sizeof(struct dma_dsc), &dma_t.paddr, GFP_KERNEL);
	if (!dma_t.dsc)
		return -ENOMEM;

	dma_t.dev = dev;
	memset(dma_t.dsc, 0, sizeof(struct dma_dsc));

	/* dma dsc configuration, ready to copy */
	dma_t.dsc->src_addr = 0x0;
	dma_t.dsc->tgt_addr = dma_t.rsv_base;
	dma_t.dsc->dsc_cfg.d32 = 0;      // clean descriptor chain
	dma_t.dsc->dsc_cfg.b.length = (dma_t.rsv_size >> 9);  // Each block is 512 bytes;
	dma_t.dsc->dsc_cfg.b.block = 1;  // block mode
	dma_t.dsc->dsc_cfg.b.mode = 0;   // DMA mode
	dma_t.dsc->dsc_cfg.b.eoc = 1;    // End of chain
	dma_t.dsc->dsc_cfg.b.owner = 1;  // Owned by hardware

	pr_info("dma_dsc size %d Bytes. Vaddr:%08x, Paddr:%08x\n",
			sizeof(struct dma_dsc),
			(unsigned int)dma_t.dsc, (unsigned int)dma_t.paddr);

	return 0;
}

static void ramdump_dma_free(struct device *dev)
{
	if (dma_t.dsc) {
		dma_free_coherent(dev, sizeof(struct dma_dsc), dma_t.dsc, dma_t.paddr);
		dma_t.dsc = NULL;
		pr_info("DMA descriptor freed.\n");
	}
}

static int ramdump_dma_copy(void)
{
	unsigned int dump_set;
	int cnt = 0;
	int ret = 0;

	if (!dma_t.reg_base) {
		pr_err("need dma init before.\n");
		return -1;
	}

	if (!cfg_sticky_reg) {
		pr_err("need ramdump init before.\n");
		return -1;
	}

	writel(DMA_STS0_READY, dma_t.reg_virt_sts0);
	writel((uintptr_t)dma_t.paddr | DMA_EOC_FLAG, dma_t.reg_virt_t0);

	pr_info("DMA tx start ...\n");
	while (cnt <= RAMDUMP_DMA_MAX_RETRIES) {
		if (readl(dma_t.reg_virt_sts0) != 0)
			break;
		udelay(1000); /* 1ms */
		cnt++;
	}

	if (cnt > RAMDUMP_DMA_MAX_RETRIES) {
		pr_info("ERROR! DMA tx timeout(%d ms). Drop dump.bin!\n",
				RAMDUMP_DMA_MAX_RETRIES);
		dump_set = readl(cfg_sticky_reg) & ~RAMDUMP_STICKY_DMA_MASK;
		dump_set &= ~AMLOGIC_KERNEL_BOOTED;
		writel(dump_set, cfg_sticky_reg);
		return -1;
	}

	pr_info("ramdump_dma_copy OK. DMA tx %d MB in %d ms.\n",
			dma_t.rsv_size / (1 << 20), cnt);
	dump_set = readl(cfg_sticky_reg) & ~RAMDUMP_STICKY_DMA_MASK;
	dump_set |= dma_t.rsv_base / RAMDUMP_DMA_ALIGNED_4MB;
	writel(dump_set, cfg_sticky_reg);
	pr_info("ramdump_dma_copy done. addr=0x%08x, sticky=0x%04x\n",
			dma_t.rsv_base, readl(cfg_sticky_reg));

	return ret;
}
#endif

#if defined(CONFIG_ARM64)
static int ramdump_verify_md5_sum(const void *addr, size_t size)
{
	struct crypto_shash *tfm;
	struct shash_desc *desc;
	u8 result[RAMDUMP_MD5_DIGEST_SIZE];
	char result_str[RAMDUMP_MD5_STRING_LEN + 1] = {0};
	struct device_node *chosen_node;
	const char *cmdline_md5;
	int i, ret = -EINVAL;
	int desc_size;

	pr_debug("%s, arm64 start.\n", __func__);
	tfm = crypto_alloc_shash("md5", 0, 0);
	if (IS_ERR(tfm))
		return PTR_ERR(tfm);

	desc_size = sizeof(*desc) + crypto_shash_descsize(tfm);
	desc = kzalloc(desc_size, GFP_KERNEL);
	if (!desc) {
		ret = -ENOMEM;
		goto out_free_tfm;
	}

	desc->tfm = tfm;

	ret = crypto_shash_digest(desc, addr, size, result);
	if (ret)
		goto out_free_desc;

	for (i = 0; i < RAMDUMP_MD5_DIGEST_SIZE; i++)
		snprintf(result_str + i * 2, 3, "%02x", result[i]);

	pr_info("ramdump calculated md5: %s\n", result_str);

	chosen_node = of_find_node_by_path("/chosen");
	if (!chosen_node) {
		ret = -ENOENT;
		goto out_free_desc;
	}

	ret = of_property_read_string(chosen_node, "bootargs", &cmdline_md5);
	if (ret) {
		ret = -ENOENT;
		goto out_free_desc;
	}

	cmdline_md5 = strstr(cmdline_md5, "androidboot.ramdumpmd5=");
	if (!cmdline_md5) {
		ret = -ENOENT;
		goto out_free_desc;
	}

	cmdline_md5 += strlen("androidboot.ramdumpmd5=");
	if (strncmp(cmdline_md5, result_str, RAMDUMP_MD5_STRING_LEN) == 0) {
		pr_debug("%s, MD5 match OK\n", __func__);
		ret = 0;
	} else {
		pr_err("MD5 mismatch!\n");
		ret = -EFAULT;
	}

out_free_desc:
	kfree(desc);
out_free_tfm:
	crypto_free_shash(tfm);
	return ret;
}
#else
static int ramdump_verify_md5_sum(const void *addr, size_t size)
{
	pr_info("arm32 mem no-map, exit.\n");
	return 0;
}
#endif

static int panic_notify(struct notifier_block *self,
			unsigned long cmd, void *ptr)
{
	int cpu = smp_processor_id();

	pr_info("ramdump: PANIC CPU-%d flush cache ...\n", cpu);
	ramdump_sync_data();
	pr_info("ramdump: PANIC CPU-%d flush cache finish.\n", cpu);
#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
	ramdump_dma_copy();
#endif
	return NOTIFY_DONE;
}

static struct notifier_block panic_notifier = {
	.notifier_call	= panic_notify,
};

static int __init ramdump_probe(struct platform_device *pdev)
{
	unsigned long total_mem;
	struct resource *res;
	unsigned int dump_set;
	void *vaddr = NULL;
	int ret = 0;

	total_mem = get_num_physpages() << PAGE_SHIFT;
	/* MEM is aligned upwards to 64MB. And CFG bit0-5 use for dma copy addr */
	total_mem = (total_mem + RAMDUMP_DDR_ALIGNED_64MB - 1) & ~(RAMDUMP_DDR_ALIGNED_64MB - 1);
	pr_info("%ld MB, args base:%lx, size:%lx\n",
			total_mem / 1024 / 1024, ramdump_base, ramdump_size);
	if (!ramdump_disable) {
#ifdef CONFIG_ANDROID_VENDOR_HOOKS
		/* flush cache for online cpu */
		ret = register_trace_android_vh_ipi_stop(flush_all_cache_hook, NULL);
#endif
		/* flush cache for panic cpu, and DMA copy for 32bit */
		ret = atomic_notifier_chain_register(&panic_notifier_list, &panic_notifier);
		if (ret)
			pr_err("ramdump register panic_notifier err(%d).\n", ret);
	}

	ram = kzalloc(sizeof(*ram), GFP_KERNEL);
	if (!ram)
		return -ENOMEM;

	if (ramdump_disable)
		ram->disable = 1;

	ram->mem_base = 0;
	ram->mem_size = ramdump_size;
	if (ramdump_base && ramdump_size) {
		pr_info("ramdump memremap start, paddr area: 0x%08lx - 0x%08lx\n",
			ramdump_base, ramdump_base + PAGE_ALIGN(ramdump_size));
#if defined(CONFIG_ARM64)
		vaddr = memremap(ramdump_base, PAGE_ALIGN(ramdump_size), MEMREMAP_WB);
#else
		vaddr = phys_to_virt(ramdump_base); //do nothing, memremap 4KB on each bin_read
#endif
		if (vaddr)
			ram->mem_base = (unsigned long)vaddr;
	}

	if (!ram->disable) {
		if (!ram->mem_base) {	/* No compressed data */
			INIT_DELAYED_WORK(&ram->work, lazy_clear_work);
			schedule_delayed_work(&ram->work, msecs_to_jiffies(120 * 1000));
		} else {		/* with compressed data */
#ifdef	SAVE_DATA_BY_INIT_RC_SHELL
			pr_info("SAVE_DATA_BY_INIT_RC_SHELL\n");
			ram->kobj = kobject_create_and_add("mdump", kernel_kobj);
			if (!ram->kobj) {
				goto err;
			}

			ramdump_attr.size = ram->mem_size;
			if (sysfs_create_bin_file(ram->kobj, &ramdump_attr)) {
				pr_err("ramdump create sysfs compmsg failed\n");
				goto err1;
			}

			ramdump_verify_md5_sum((void *)ram->mem_base, ram->mem_size);
#endif	/* end SAVE_DATA_BY_INIT_RC_SHELL */
		}

		/* if ramdump is disabled in env, no need to set sticky reg */
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "SYSCTRL_STICKY_REG6");
		if (res) {
			cfg_sticky_reg = devm_ioremap(&pdev->dev, res->start,
					    res->end - res->start);
			if (!cfg_sticky_reg) {
				pr_err("%s, map reg failed\n", __func__);
				goto err;
			}
			dump_set = readl(cfg_sticky_reg);
			dump_set &= ~RAMDUMP_STICKY_DATA_MASK;
			dump_set |= ((total_mem >> 20) | AMLOGIC_KERNEL_BOOTED);
			writel(dump_set, cfg_sticky_reg);
			pr_info("ramdump set sticky(0x%08x) to 0x%x\n",
					(unsigned int)res->start, dump_set);

#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
			ramdump_dma_init(&pdev->dev);
#endif
		}
	}
	return ret;
#ifdef	SAVE_DATA_BY_INIT_RC_SHELL
err1:
	kobject_put(ram->kobj);
#endif

err:
	kfree(ram);

	return -EINVAL;
}

static void ramdump_remove(struct platform_device *pdev)
{
#ifdef SAVE_DATA_BY_INIT_RC_SHELL
	sysfs_remove_bin_file(ram->kobj, &ramdump_attr);
	iounmap((void *)ram->mem_base);
	kobject_put(ram->kobj);
#endif
#ifdef CONFIG_AMLOGIC_RAMDUMP_DMA_COPY
	ramdump_dma_free(&pdev->dev);
#endif
	kfree(ram);
}

#ifdef CONFIG_OF
static const struct of_device_id ramdump_dt_match[] = {
	{
		.compatible = "amlogic, ram_dump",
	},
	{},
};
#endif

static struct platform_driver ramdump_driver = {
	.driver = {
		.name  = "mdump",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = ramdump_dt_match,
	#endif
	},
	.remove = ramdump_remove,
};

int __init ramdump_init(void)
{
	int ret;

	ret = platform_driver_probe(&ramdump_driver, ramdump_probe);

	return ret;
}

void __exit ramdump_uninit(void)
{
	platform_driver_unregister(&ramdump_driver);
}

