// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/statfs.h>
#include <linux/buffer_head.h>
#include <linux/backing-dev.h>
#include <linux/kthread.h>
#include <linux/parser.h>
#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/exportfs.h>
#include <linux/blkdev.h>
#include <linux/quotaops.h>
#include <linux/sysfs.h>
#include <linux/quota.h>
#include <linux/unicode.h>
#include <linux/part_stat.h>
#include <linux/cleancache.h>
#include <linux/sched/clock.h>
#include <linux/async.h>
#include <linux/amlogic/amfc.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/symbols.h>
#include <linux/xattr.h>
#include <linux/cred.h>
#include <linux/xxhash.h>
#include <linux/psi.h>

#include <linux/proc_fs.h>

/*--------- erofs symbols ----------*/
FUN_TYPE1(xxh32);
FUN_TYPE1(filemap_add_folio);
FUN_TYPE1(psi_memstall_leave);
FUN_TYPE1(psi_memstall_enter);
FUN_TYPE1(migrate_disable);
FUN_TYPE1(migrate_enable);
FUN_TYPE1(alloc_pages_bulk_noprof);
FUN_TYPE1(lockref_get_not_zero);
FUN_TYPE1(lockref_put_or_lock);
FUN_TYPE1(lockref_mark_dead);
FUN_TYPE1(bio_init);
FUN_TYPE1(vfs_iocb_iter_read);
FUN_TYPE1(__bio_advance);
FUN_TYPE1(zero_fill_bio_iter);
FUN_TYPE1(bio_uninit);
FUN_TYPE1(bio_add_folio);
FUN_TYPE1(kmem_cache_alloc_lru_noprof);
FUN_TYPE1(get_tree_bdev_flags);
FUN_TYPE1(filp_open);
FUN_TYPE1(get_tree_nodev);
FUN_TYPE1(super_setup_bdi);
FUN_TYPE1(bdi_dev_name);
FUN_TYPE1(read_cache_folio);
FUN_TYPE1(folio_end_read);
FUN_TYPE1(iomap_read_folio);
FUN_TYPE1(flush_dcache_folio);
FUN_TYPE1(iomap_invalidate_folio);
FUN_TYPE1(iomap_release_folio);
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
FUN_TYPE1(thp_get_unmapped_area);
#endif
FUN_TYPE1(__seq_puts);
FUN_TYPE1(file_bdev);
FUN_TYPE1(generic_encode_ino32_fh);
FUN_TYPE1(bdev_file_open_by_path);
FUN_TYPE1(__folio_swap_cache_index);
FUN_TYPE1(_copy_to_iter);
FUN_TYPE1(copy_highpage);
FUN_TYPE1(iov_iter_bvec);
FUN_TYPE1(kmemdup_nul);
FUN_TYPE1(memchr_inv);
FUN_TYPE1(folio_unlock);
FUN_TYPE1(__filemap_get_folio);
FUN_TYPE1(filemap_splice_read);
FUN_TYPE1(wait_for_completion_io);

struct ksymbol {
	const char *name;
	void *data;
	unsigned int name_len;
};

static int __symbol_fixed;

OBJ_TYPE(struct qstr, dotdot_name);
OBJ_TYPE(struct xattr_handler, nop_posix_acl_access);
OBJ_TYPE(struct xattr_handler, nop_posix_acl_default);

#ifdef CONFIG_ARM64
#ifndef CONFIG_KALLSYMS_ALL
static struct qstr _dotdot_name = QSTR_INIT("..", 2);

static bool
f_posix_acl_xattr_list(struct dentry *dentry)
{
	return IS_POSIXACL(d_backing_inode(dentry));
}

/*
 * nop_posix_acl_access - legacy xattr handler for access POSIX ACLs
 *
 * This is the legacy POSIX ACL access xattr handler. It is used by some
 * filesystems to implement their ->listxattr() inode operation. New code
 * should never use them.
 */
static struct xattr_handler _nop_posix_acl_access = {
	.name = XATTR_NAME_POSIX_ACL_ACCESS,
	.list = f_posix_acl_xattr_list,
};

/*
 * nop_posix_acl_default - legacy xattr handler for default POSIX ACLs
 *
 * This is the legacy POSIX ACL default xattr handler. It is used by some
 * filesystems to implement their ->listxattr() inode operation. New code
 * should never use them.
 */
static struct xattr_handler _nop_posix_acl_default = {
	.name = XATTR_NAME_POSIX_ACL_DEFAULT,
	.list = f_posix_acl_xattr_list,
};
#endif

static struct ksymbol module_symbols[] = {
	/* MUST keep sorted */
	KSYM_FUN(__bio_advance),
	KSYM_FUN(__filemap_get_folio),
	KSYM_FUN(__folio_swap_cache_index),
	KSYM_FUN(__seq_puts),
	KSYM_FUN(_copy_to_iter),
	KSYM_FUN(alloc_pages_bulk_noprof),
	KSYM_FUN(bdi_dev_name),
	KSYM_FUN(bio_add_folio),
	KSYM_FUN(bio_init),
	KSYM_FUN(bio_uninit),
	KSYM_FUN(bdev_file_open_by_path),
	KSYM_FUN(copy_highpage),
#ifdef CONFIG_KALLSYMS_ALL
	KSYM_OBJ(dotdot_name),
#endif
	KSYM_FUN(file_bdev),
	KSYM_FUN(filemap_add_folio),
	KSYM_FUN(filemap_splice_read),
	KSYM_FUN(filp_open),
	KSYM_FUN(folio_end_read),
	KSYM_FUN(folio_unlock),
	KSYM_FUN(flush_dcache_folio),
	KSYM_FUN(generic_encode_ino32_fh),
	KSYM_FUN(get_tree_bdev_flags),
	KSYM_FUN(get_tree_nodev),
	KSYM_FUN(iomap_invalidate_folio),
	KSYM_FUN(iomap_read_folio),
	KSYM_FUN(iomap_release_folio),
	KSYM_FUN(iov_iter_bvec),
	KSYM_FUN(kmem_cache_alloc_lru_noprof),
	KSYM_FUN(kmemdup_nul),
	KSYM_FUN(lockref_get_not_zero),
	KSYM_FUN(lockref_mark_dead),
	KSYM_FUN(lockref_put_or_lock),
	KSYM_FUN(memchr_inv),
	KSYM_FUN(migrate_disable),
	KSYM_FUN(migrate_enable),
#ifdef CONFIG_KALLSYMS_ALL
	KSYM_OBJ(nop_posix_acl_access),
	KSYM_OBJ(nop_posix_acl_default),
#endif
	KSYM_FUN(psi_memstall_enter),
	KSYM_FUN(psi_memstall_leave),
	KSYM_FUN(read_cache_folio),
	KSYM_FUN(super_setup_bdi),
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	KSYM_FUN(thp_get_unmapped_area),
#endif
	KSYM_FUN(vfs_iocb_iter_read),
	KSYM_FUN(wait_for_completion_io),
	KSYM_FUN(xxh32),
	KSYM_FUN(zero_fill_bio_iter),
	{}
};

/* see struct proc_dir_entry in fs/proc/internal.h */
struct proc_node {
	/*
	 * number of callers into module in progress;
	 * negative -> it's going away RSN
	 */
	atomic_t in_use;
	refcount_t refcnt;
	struct list_head pde_openers;	/* who did ->open, but not ->release */
	/* protects ->pde_openers and all struct pde_opener instances */
	spinlock_t pde_unload_lock;
	struct completion *pde_unload_completion;
	const struct inode_operations *proc_iops;
	union {
		const struct proc_ops *proc_ops;
		const struct file_operations *proc_dir_ops;
	};
	const struct dentry_operations *proc_dops;
	union {
		const struct seq_operations *seq_ops;
		int (*single_show)(struct seq_file *, void *);
	};
	proc_write_t write;
	void *data;
	unsigned int state_size;
	unsigned int low_ino;
	nlink_t nlink;
	kuid_t uid;
	kgid_t gid;
	loff_t size;
	struct proc_node *parent;
	struct rb_root subdir;
	struct rb_node subdir_node;
	char *name;
	umode_t mode;
	u8 flags;
	u8 namelen;
	char inline_name[];
} __randomize_layout;

static struct proc_node *find_subnode(struct proc_node *parent, char *name)
{
	struct proc_node *pd = NULL;

	pd = rb_entry_safe(rb_first(&parent->subdir), struct proc_node, subdir_node);
	while (1) {
		if (!pd)
			break;
		if (!strcmp(pd->name, name))
			break;
		pd = rb_entry_safe(rb_next(&pd->subdir_node), struct proc_node, subdir_node);
	}
	return pd;
}

static int namecmp(const char *name1, int len1, const char *name2, int len2)
{
	int cmp;

	cmp = memcmp(name1, name2, min(len1, len2));
	if (cmp == 0)
		cmp = len1 - len2;
	return cmp;
}

static int proc_handle(const struct ctl_table *ctl, int write, void *buffer,
		size_t *lenp, loff_t *ppos)
{
	return 0;
}

static unsigned short ascii_off[128];
static unsigned short ascii_cnt[128];

static void update_ascii_off(struct ksymbol *ksyms)
{
	int i = 0;
	int c;

	memset(ascii_off, 0xff, sizeof(ascii_off));
	while (ksyms->name) {
		if (!ksyms->name_len)
			ksyms->name_len = strlen(ksyms->name);
		c = ksyms->name[0];
		if (ascii_off[c] == 0xffff)
			ascii_off[c] = i;
		ascii_cnt[c]++;
		i++;
		ksyms++;
	}
}

static struct ctl_table *disable_kstr_ptr(int *old)
{
	struct ctl_table tmp[] = {
		{
			.procname = "amfc",
			.proc_handler = proc_handle,
			.mode = 0666,
			.data = NULL,
		}
	};
	struct ctl_table_header *hdr, *head;
	struct rb_node *node;
	struct ctl_table *entry = NULL;
	struct ctl_dir *dir;

	hdr = register_sysctl("kernel", tmp);
	if (!hdr) {
		pr_err("%s, register failed\n", __func__);
		return NULL;
	}

	dir = hdr->parent;
	node = dir->root.rb_node;
	while (node) {
		struct ctl_node *ctl_node;
		const char *procname;
		int cmp;

		ctl_node = rb_entry(node, struct ctl_node, node);
		head = ctl_node->header;
		entry = &head->ctl_table[ctl_node - head->node];
		procname = entry->procname;

		cmp = namecmp("perf_event_paranoid", 13, procname, strlen(procname));
		if (cmp < 0)
			node = node->rb_left;
		else if (cmp > 0)
			node = node->rb_right;
		else {
			break;
		}
	}
	unregister_sysctl_table(hdr);
	if (entry) {
		*old = *(int *)entry->data;
		pr_debug("get entry:%px, %s, value:%d\n",
			entry, entry->procname, *old);
		*(int *)entry->data = 0;
		return entry;
	}
	return NULL;
}

static int fill_symbol(char *line, struct ksymbol *sym)
{
	unsigned long value;
	int ret;
	int off, start, end, i;
	int c;

#ifdef CONFIG_ARM64
	off = 19;
#else
	off = 11;
#endif

	c = line[off];
	if (ascii_off[c] == 0xffff) {	// do not have this acsii
		return 0;
	}

	start = ascii_off[c];
	end = ascii_cnt[c] + start;
	pr_debug("ascii start:%d-%d, sym:%s\n", start, end, &line[off]);

	i = 1;
	while (start < end) {
		c = line[i + off];
		if (!c)	// end of input
			return 0;
		if (*(unsigned long *)sym[start].data) { // already fixed
			start++;
			i = 1;
			continue;
		}
		if (sym[start].name[i] == c) { // keep match in this symbol
			if (i == (sym[start].name_len - 1)) { // sym end
				pr_debug("c:%c-%x, len:%d, start:%d, i:%d, sym:%s, line:%s",
					c, line[i + off + 1], sym[start].name_len,
					start, i, sym[start].name, &line[off]);
				if (line[i + off + 1] != '\n') {	// line not end
					start++;
					i = 1;
					continue;
				}
				line[off - 3] = '\0';
				ret = kstrtoul(line, 16, &value);
				if (ret)
					return ret;
				pr_debug("find sym:%lx %s:%s\n",
					value, sym[start].name, &line[off]);
				*(unsigned long *)sym[start].data = value;
				return 1;
			}
			i++;
			continue;
		}
		/* switch to next symbole */
		start++;
		i = 1;
	}
	pr_debug("not match sym:%s\n", &line[off]);
	return 0;
}

int check_sym(struct ksymbol *sym)
{
	int find = 0, nfind = 0;

	while (sym->name) {
		if (*(unsigned long *)sym->data) {
			find++;
		} else {
			pr_err("can't find sym:%s\n", sym->name);
			nfind++;
		}
		sym++;
	}
	return nfind;
}

static struct super_block _s = {};

static struct inode _i = {
	.i_sb = &_s,
};

static struct address_space _a = {
	.host = &_i,
};

static struct file f = {
	.f_mapping = &_a,
	.f_inode = &_i,
};

static int fill_module_symbols(struct proc_node *node, struct ksymbol *sym)
{
	struct proc_ops *ops;
	struct seq_file *s;
	char line_buf[256] = {};
	int ret = 0;
	loff_t off = 0;
	void *p;
	int fixed_symbols = 0;

	f.f_cred = current_cred();
	ops = (struct proc_ops *)node->proc_ops;
	ret = ops->proc_open(NULL, &f);
	if (ret) {
		pr_info("open fail\n");
		return -1;
	}
	s = f.private_data;
	p = s->op->start(s, &off);
	s->buf = line_buf;
	while (1) {
		s->count = 0;
		s->size  = sizeof(line_buf);
		//memset(line_buf, 0, sizeof(line_buf));
		ret = s->op->show(s, NULL);
		if (ret < 0) {
			pr_info("read fail:%d\n", ret);
			break;
		}
		s->count = 0;
		pr_debug("line:%s", line_buf);
		ret = fill_symbol(line_buf, sym);
		if (ret > 0) {
			fixed_symbols += ret;
			if (fixed_symbols >= ARRAY_SIZE(module_symbols) - 1)
			break;
		}
		p = s->op->next(s, p, &off);
		if (!p || IS_ERR(p))
			break;
	}
	s->buf = NULL;
	ops->proc_release(NULL, &f);
	if (check_sym(sym))
		return -ENOSYS;
	return 0;
}
#else
/* for arm32 */
static struct ksymbol module_symbols[] = {
	/* these functions are not exported by f2fs */
	{}
};

static void direct_symbol_assign(void)
{
	/*--------- erofs symbols ----------*/
	FUN_ASSIGN(get_tree_bdev_flags);
	FUN_ASSIGN(kmemdup_nul);
	FUN_ASSIGN(filp_open);
	FUN_ASSIGN(bdev_file_open_by_path);
	FUN_ASSIGN(file_bdev);
	FUN_ASSIGN(bdi_dev_name);
	FUN_ASSIGN(super_setup_bdi);
	FUN_ASSIGN(generic_encode_ino32_fh);
	FUN_ASSIGN(get_tree_nodev);
	FUN_ASSIGN(__folio_swap_cache_index);
	FUN_ASSIGN(read_cache_folio);
	FUN_ASSIGN(folio_end_read);
	FUN_ASSIGN(iomap_read_folio);
	FUN_ASSIGN(memchr_inv);
	FUN_ASSIGN(copy_highpage);
	FUN_ASSIGN(bio_uninit);
	FUN_ASSIGN(iov_iter_bvec);
	FUN_ASSIGN(vfs_iocb_iter_read);
	FUN_ASSIGN(bio_init);
	FUN_ASSIGN(bio_add_folio);
	FUN_ASSIGN(folio_unlock);
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	FUN_ASSIGN(thp_get_unmapped_area);
#endif
	FUN_ASSIGN(filemap_splice_read);
	FUN_ASSIGN(iomap_invalidate_folio);
	FUN_ASSIGN(iomap_release_folio);
	FUN_ASSIGN(__filemap_get_folio);
	FUN_ASSIGN(kmem_cache_alloc_lru_noprof);
	FUN_ASSIGN(flush_dcache_folio);
	FUN_ASSIGN(filemap_add_folio);
	FUN_ASSIGN(wait_for_completion_io);
	FUN_ASSIGN(xxh32);
	FUN_ASSIGN(migrate_disable);
	FUN_ASSIGN(migrate_enable);
	FUN_ASSIGN(alloc_pages_bulk_noprof);
	FUN_ASSIGN(lockref_get_not_zero);
	FUN_ASSIGN(lockref_put_or_lock);
	FUN_ASSIGN(lockref_mark_dead);
	FUN_ASSIGN(__bio_advance);
	FUN_ASSIGN(zero_fill_bio_iter);
	FUN_ASSIGN(__seq_puts);
	FUN_ASSIGN(_copy_to_iter);
	FUN_ASSIGN(psi_memstall_enter),
	FUN_ASSIGN(psi_memstall_leave),
	dotdot_name_t = (struct qstr *)&dotdot_name;
}
#endif

int symbol_fix(void)
{
#ifdef CONFIG_ARM
	direct_symbol_assign();
	__symbol_fixed = 1;
	return 0;
#else
	struct proc_dir_entry *entry;
	struct proc_node *parent, *tmp;
	const struct proc_ops temp = {};
	struct ctl_table *kptr;
	int ret = 0, old = -1;
	unsigned long long te;

	te = sched_clock();
	update_ascii_off(module_symbols);
	entry = proc_create("amfc", 0444, NULL, &temp);
	if (!entry) {
		pr_err("%s, create proc failed\n", __func__);
		return -1;
	}
	parent = ((struct proc_node *)entry)->parent;
	if (!parent) {
		pr_err("%s, NULL pareret\n", __func__);
		return -1;
	}

	kptr = disable_kstr_ptr(&old);
	if (!kptr) {
		pr_err("get kptr failed\n");
		return -1;
	}
	tmp = find_subnode(parent, "kallsyms");
	if (!tmp) {
		pr_err("get kallsyms failed\n");
		return -1;
	}

	ret = fill_module_symbols(tmp, module_symbols);

	*(int *)kptr->data = old;

	proc_remove(entry);

	/* fake for handle */
#ifndef CONFIG_KALLSYMS_ALL
	dotdot_name_t = &_dotdot_name;
	nop_posix_acl_access_t = &_nop_posix_acl_access;
	nop_posix_acl_default_t = &_nop_posix_acl_default;
#endif

	if (!ret)
		__symbol_fixed = 1;
	pr_info("%s, prio:%d, ret:%d, time:%lld\n",
		__func__, current->prio, ret, sched_clock() - te);
	return ret;
#endif
}

int symbol_fixed(void)
{
	return __symbol_fixed;
}
EXPORT_SYMBOL(symbol_fixed);

static void do_symbol_fix(struct work_struct *work)
{
	if (symbol_fix()) {
		pr_emerg("%s, %d, symbol fix failed\n", __func__, __LINE__);
		BUG();
		return;
	}
}

//static struct work_struct symbol_work;

int __init sym_helper_init(void)
{
	//int cpu = num_online_cpus() - 1;

	//INIT_WORK(&symbol_work, do_symbol_fix);
	//queue_work_on(cpu, system_highpri_wq, &symbol_work);
	do_symbol_fix(NULL);
	return 0;
}

void __exit sym_helper_exit(void)
{
	__symbol_fixed = 0;
	memset(module_symbols, 0, sizeof(module_symbols));
}
module_init(sym_helper_init);
module_exit(sym_helper_exit);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_LICENSE("GPL");
