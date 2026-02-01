// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/mtd/mtd.h>
#include <linux/slab.h>
#include <linux/mtd/partitions.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_mtd_nand.h>
#include <linux/amlogic/nand_encryption.h>

struct storage_part {
	char name[16];
	unsigned int offset;
	unsigned int size;
	unsigned char flags;
};

struct storage_parts {
#define STORAGE_PART_MAGIC "STO_PART"
	char magic[8];
	unsigned char version;
	unsigned char reserved[3];
	unsigned int count;
#define STORAGE_PART_OFFSET (16)
	struct storage_part *parts;
};

static u32 skip_bad_block;

static bool node_has_compatible(struct device_node *pp)
{
	return of_get_property(pp, "compatible", NULL);
}

void meson_nand_set_skip_bad_block(u32 skip)
{
	if (skip_bad_block)
		pr_warn("WARNING !!! skip_bad_block %d would overwrite to %d\n",
			  skip_bad_block, skip);
	skip_bad_block = skip;
}
EXPORT_SYMBOL_GPL(meson_nand_set_skip_bad_block);

static int parse_meson_fastboot_partitions(struct mtd_info *master,
				  const struct mtd_partition **pparts,
				  struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts;
	struct device_node *mtd_node;
	u32 param_sram_addr, param_sram_size;
	struct storage_parts *part_info;
	struct storage_part *part;
	void __iomem *param_sram;
	int ret = 0;
	u8 nr_parts, boot_parts = 0, i, j;
	u8 *buf;

	/* Pull of_node from the master device node */
	mtd_node = mtd_get_of_node(master);
	if (!mtd_node)
		return 0;

	ret = of_property_read_u32(mtd_node, "param_sram_addr", &param_sram_addr);
	if (ret) {
		pr_err("%s %d, no find para item param_sram_addr in dts!\n",
				__func__, __LINE__);
		return ret;
	}

	ret = of_property_read_u32(mtd_node, "param_sram_size", &param_sram_size);
	if (ret) {
		pr_err("%s %d, no find para item param_sram_size in dts!\n",
				__func__, __LINE__);
		return ret;
	}

	param_sram = ioremap(param_sram_addr, param_sram_size);
	if (!param_sram) {
		pr_err("%s %d, map sram addr failed!\n",
				__func__, __LINE__);
		return ret;
	}

	buf = kmalloc(param_sram_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy((void *)buf, (void *)param_sram, param_sram_size);
	part_info = (struct storage_parts *)buf;

	if (master->type == MTD_NORFLASH)
		boot_parts = 1;

	nr_parts = part_info->count;
	parts = kcalloc(nr_parts + boot_parts, sizeof(struct mtd_partition), GFP_KERNEL);
	if (!parts)
		goto part_fail;

	part = (void *)part_info + STORAGE_PART_OFFSET;
	for (i = boot_parts, j = 0; j < nr_parts; i++, j++) {
		parts[i].offset = part[j].offset;
		if (j == (nr_parts - 1))
			parts[i].size = master->size - parts[i].offset;
		else
			parts[i].size = part[j].size;
		parts[i].of_node = mtd_node;
		parts[i].name = kstrdup(part[j].name, GFP_KERNEL);
	}

	if (master->type == MTD_NORFLASH) {
		parts[0].name = "bl2";
		parts[0].offset = 0;
		parts[0].size = parts[boot_parts].offset;
		parts[0].of_node = mtd_node;
	}

	*pparts = parts;
	kfree(buf);
	return nr_parts + boot_parts;

part_fail:
	kfree(buf);
	return -EINVAL;
}

struct mtd_part_parser meson_fast_parser = {
	.parse_fn = parse_meson_fastboot_partitions,
	.name = "meson-fastboot-partitions",
};

static int meson_nand_partition_relocate(struct mtd_info *master, u8 nr_parts,
			      struct mtd_partition *parts)
{
	u8 i = 0, bl_mode;
	loff_t reserved_part_blk_num = meson_rsv_get_block_cnt();
	u64 part_size, start_blk = 0, part_blk = 0;
	loff_t offset;
	int phys_erase_shift;
	loff_t adjust_offset = 0;
	struct mtd_partition *part = parts;

	if (!strncmp((char *)part->name, NAND_BOOT_NAME,
		     strlen((const char *)NAND_BOOT_NAME)) ||
	    !strcmp((char *)part->name, "bl2")) {
		part->offset = 0;
		if (!part->size)
			part->size = ((uint64_t)master->writesize * BOOT_TOTAL_PAGES);
		part++;
		nr_parts -= 1;
	}

	if (!nr_parts)
		goto out;

	phys_erase_shift = ffs(master->erasesize) - 1;

	adjust_offset = BOOT_TOTAL_PAGES * (loff_t)master->writesize;
	bl_mode = meson_nand_get_bootloader_mode();
	if (bl_mode == NAND_FIPMODE_ADVANCE) {
		/* get boot area entry form env */
		aml_nand_param_check_and_layout_init(master);
		/* bl2e, bl2x, ddrfip, devfip */
		for (i = BOOT_AREA_BL2E; i < BOOT_AREA_DEVFIP; i++) {
			part->offset =
				g_ssp.boot_entry[i].offset;
				part->size = (uint64_t)g_ssp.boot_entry[i].size *
					g_ssp.boot_backups;
			part++;
		}
		part->offset = g_ssp.boot_entry[BOOT_AREA_DEVFIP].offset;
		part->size = g_ssp.boot_entry[BOOT_AREA_DEVFIP].size *
					meson_nand_get_fipcopies();
		nr_parts -= 4;
	} else if (bl_mode == NAND_FIPMODE_DISCRETE) {
		/* tpl, which not skip bad block, support NAND_FIPMODE_DISCRETE only */
		part->offset = adjust_offset +
			reserved_part_blk_num *
			master->erasesize;
		part->size = meson_nand_get_fipsize() * meson_nand_get_fipcopies();
		nr_parts -= 1;
	} else {
		pr_err("%s: Invalid bootloader mode\n", __func__);
		return -EINVAL;
	};

	adjust_offset = part->offset + part->size;
	part++;

	while (--nr_parts) {
		if (master->size < adjust_offset) {
			pr_err("%s %d error: over the nand size!!!\n",
					__func__, __LINE__);
			return -EINVAL;
		}
		part->offset = adjust_offset;
		part_size = part->size;

		if (skip_bad_block) {
			offset = 0;
			start_blk = 0;
			part_blk = part_size >> phys_erase_shift;
			do {
				offset = adjust_offset + start_blk *
					master->erasesize;
				if (master->_block_isbad(master, offset) == NAND_FACTORY_BAD) {
					pr_info("%s %d factory bad addr=%llx\n",
							__func__, __LINE__, (u64)(offset >>
								phys_erase_shift));
					adjust_offset += master->erasesize;
					continue;
				}
				start_blk++;
			} while (start_blk < part_blk);
		}
		adjust_offset += part_size;
		part->size = adjust_offset - part->offset;
		part++;
	}

	/* last partition */
	part->offset = adjust_offset;
	part->size = master->size - part->offset;
out:
	return 0;
}

static int meson_parse_partitions(struct mtd_info *master,
				  const struct mtd_partition **pparts,
				  struct mtd_part_parser_data *data)
{
	struct mtd_partition *parts;
	struct device_node *mtd_node;
	struct device_node *ofpart_node;
	const char *partname;
	struct device_node *pp;
	u8 nr_parts, i;
	int ret = 0;
	bool dedicated = true;

	/* Pull of_node from the master device node */
	mtd_node = mtd_get_of_node(master);
	if (!mtd_node)
		return 0;

	ofpart_node = of_get_child_by_name(mtd_node, "partitions");
	if (!ofpart_node) {
		/*
		 * We might get here even when ofpart isn't used at all (e.g.,
		 * when using another parser), so don't be louder than
		 * KERN_DEBUG
		 */
		pr_debug("%s: 'partitions' subnode not found on %pOF. Trying to parse direct subnodes as partitions.\n",
			 master->name, mtd_node);
		ofpart_node = mtd_node;
		dedicated = false;
	}

	/* First count the subnodes */
	nr_parts = 0;
	for_each_child_of_node(ofpart_node,  pp) {
		if (!dedicated && node_has_compatible(pp))
			continue;

		nr_parts++;
	}

	if (nr_parts == 0) {
		pr_err("%s: not found partitions on %pOF.\n", __func__, ofpart_node);
		return 0;
	}

	parts = kcalloc(nr_parts, sizeof(*parts), GFP_KERNEL);
	if (!parts)
		return -ENOMEM;
	i = 0;
	for_each_child_of_node(ofpart_node,  pp) {
		const __be32 *reg;
		int len;
		int a_cells, s_cells;

		if (!dedicated && node_has_compatible(pp))
			continue;

		reg = of_get_property(pp, "reg", &len);
		if (!reg) {
			if (dedicated) {
				pr_err("%s: ofpart partition %pOF (%pOF) missing reg property.\n",
					 master->name, pp,
					 mtd_node);
				goto part_fail;
			} else {
				nr_parts--;
				continue;
			}
		}

		a_cells = of_n_addr_cells(pp);
		s_cells = of_n_size_cells(pp);
		if (len / 4 != a_cells + s_cells) {
			pr_err("%s: ofpart partition %pOF (%pOF) error parsing reg property.\n",
				 master->name, pp,
				 mtd_node);
			goto part_fail;
		}

		parts[i].offset = of_read_number(reg, a_cells);
		parts[i].size = of_read_number(reg + a_cells, s_cells);
		parts[i].of_node = pp;

		partname = of_get_property(pp, "label", &len);
		if (!partname)
			partname = of_get_property(pp, "name", &len);
		parts[i].name = partname;

		pr_debug("%s: size %llx\n", parts[i].name, parts[i].size);
		if (of_get_property(pp, "read-only", &len))
			parts[i].mask_flags |= MTD_WRITEABLE;

		if (of_get_property(pp, "lock", &len))
			parts[i].mask_flags |= MTD_POWERUP_LOCK;

		if (of_property_read_bool(pp, "slc-mode"))
			parts[i].add_flags |= MTD_SLC_ON_MLC_EMULATION;

		i++;
	}

	if (!nr_parts)
		goto part_none;

	ret = meson_nand_partition_relocate(master, nr_parts, parts);
	if (ret)
		goto part_none;

	*pparts = parts;
	return nr_parts;

part_fail:
	pr_err("%s: error parsing ofpart partition %pOF (%pOF)\n",
	       master->name, pp, mtd_node);
	ret = -EINVAL;
part_none:
	of_node_put(pp);
	kfree(parts);
	return ret;
}

struct mtd_part_parser meson_parts_parser = {
	.parse_fn = meson_parse_partitions,
	.name = "meson_ofpartitions",
};

static const char * const meson_mtd_types[] = {
	"cmdlinepart",
	"meson_ofpartitions",
	"meson-fastboot-partitions",
	NULL
};

#ifdef CONFIG_NAND_ENCRYPTION
static void mtd_loop_encrypted_partition(struct mtd_info *mtd)
{
	struct mtd_info *child, *master = mtd_get_master(mtd);
	struct mtd_partition part, *ppart;

	ppart = &part;
	mutex_lock(&master->master.partitions_lock);
	list_for_each_entry(child, &mtd->partitions, part.node) {
		ppart->offset = child->part.offset;
		ppart->size = child->part.size;
		ppart->name = child->name;
		set_region_encrypted(mtd, ppart, true);
	}
	mutex_unlock(&master->master.partitions_lock);
}
#endif

static void meson_nand_register_mtd_parser(void)
{
	static bool had_register;

	if (had_register)
		return;

	register_mtd_parser(&meson_fast_parser);
	register_mtd_parser(&meson_parts_parser);
	had_register = true;
}

int meson_add_mtd_partitions(struct mtd_info *mtd)
{
	meson_nand_register_mtd_parser();
	if (mtd_device_parse_register(mtd, meson_mtd_types, NULL, NULL, 0) != 0) {
		pr_err("%s() register partitions failed\n", __func__);
		return -1;
	}

#ifdef CONFIG_NAND_ENCRYPTION
	mtd_loop_encrypted_partition(mtd);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(meson_add_mtd_partitions);
