// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#endif
#include "../dpss/sys_def.h"
#include <linux/amlogic/media/di/di_pq.h>
#include "d_define.h"
#include "d_pq_mgr.h"
#include "dpss_pq_drv.h"
unsigned int pq_update;
module_param_named(pq_update, pq_update, uint, 0664);

bool sw_dct;
module_param_named(sw_dct, sw_dct, bool, 0664);

static struct di_tuning_reg_s di_demosquito_param[REG_DM_MAX] = {
	{0xD251, 4, 7, 8},
	{0xD25A, 24, 29, 1},
	{0xD25A, 16, 21, 1},
	{0xD25A, 8, 13, 2},
	{0xD25A, 0, 5, 3},
	{0xD259, 24, 29, 4},
	{0xD259, 16, 21, 6},
	{0xD259, 8, 13, 8},
	{0xD259, 0, 5, 10},
	{0xD258, 24, 29, 12},
	{0xD258, 16, 21, 16},
	{0xD258, 8, 13, 20},
	{0xD258, 0, 5, 24},
	{0xD257, 24, 29, 30},
	{0xD257, 16, 21, 38},
	{0xD257, 8, 13, 48},
	{0xD257, 0, 5, 60},
	{0xD25C, 24, 29, 28},
	{0xD25C, 16, 21, 24},
	{0xD25C, 8, 13, 18},
	{0xD25C, 0, 5, 18},
	{0xD25B, 24, 29, 22},
	{0xD25B, 16, 21, 28},
	{0xD25B, 8, 13, 32},
	{0xD25B, 0, 5, 32},
	{0xD25D, 0, 3, 15},
	{0xA2D0, 8, 17, 120},
	{0xA2D0, 0, 5, 32},
	{0xD250, 16, 21, 14},
	{0xD250, 8, 13, 8},
	{0xD250, 0, 5, 2},
	{0xD251, 12, 17, 10},
	{0xD252, 16, 21, 16},
	{0xD252, 8, 13, 48},
	{0xD252, 4, 7, 4},
	{0xD252, 0, 3, 4},
	{0xD253, 16, 21, 8},
	{0xD253, 8, 13, 28},
	{0xD253, 4, 7, 4},
	{0xD253, 0, 3, 2},
	{0xD254, 20, 27, 5},
	{0xD254, 12, 19, 15},
	{0xD254, 4, 11, 23},
	{0xD254, 0, 3, 1},
};

static struct di_tuning_reg_s di_dnr_param[REG_DNR_MAX] = {
	{0xD235, 16, 23, 128},
	{0xD235, 8, 15, 64},
	{0xD235, 0, 7, 0},
	{0xD237, 24, 29, 8},
	{0xD237, 20, 23, 2},
	{0xD237, 12, 17, 0},
	{0xD237, 8, 11, 0},
	{0xD23A, 4, 9, 8},
	{0xD23A, 0, 3, 0},
	{0xD22F, 4, 7, 8},
	{0xD22F, 0, 3, 0},
	{0xD230, 24, 29, -10},
	{0xD230, 16, 21, -8},
	{0xD230, 8, 13, -6},
	{0xD230, 0, 5, -4},
	{0xD231, 0, 3, 6},
	{0xD226, 28, 31, 8},
	{0xD226, 24, 27, 9},
	{0xD226, 20, 23, 10},
	{0xD226, 16, 19, 11},
	{0xD226, 12, 15, 12},
	{0xD226, 8, 11, 13},
	{0xD226, 4, 7, 14},
	{0xD226, 0, 3, 15},
	{0xD225, 24, 29, 28},
	{0xD225, 16, 21, 32},
	{0xD225, 8, 13, 26},
	{0xD225, 0, 5, 20},
	{0xD224, 24, 29, 16},
	{0xD224, 16, 21, 22},
	{0xD224, 8, 13, 30},
	{0xD224, 0, 5, 38},
	{0xD231, 5, 5, 1},
	{0xD231, 4, 4, 0},
	{0xD221, 24, 29, 40},
	{0xD221, 16, 21, 40},
	{0xD221, 8, 13, 40},
	{0xD221, 0, 5, 40},
	{0xD220, 24, 29, 40},
	{0xD220, 16, 21, 40},
	{0xD220, 8, 13, 40},
	{0xD220, 0, 5, 40},
};

static struct di_tuning_reg_s di_dct_param[REG_DCT_MAX] = {
	{0xD700, 29, 30, 3},
	{0xD700, 27, 28, 3},
	{0xB934, 31, 31, 1},
	{0xB934, 30, 30, 0},
	{0xB934, 24, 29, 0},
	{0xB934, 16, 23, 128},
	{0xB934, 12, 15, 8},
	{0xB934, 0, 8, 0},
	{0xD716, 24, 31, 10},
	{0xD716, 16, 23, 18},
	{0xD716, 15, 15, 1},
	{0xD716, 13, 14, 2},
	{0xD716, 8, 12, 10},
	{0xD717, 12, 13, 2},
	{0xD717, 0, 11, 0},
	{0xD718, 31, 31, 1},
	{0xD718, 30, 30, 0},
	{0xD718, 29, 29, 0},
	{0xD718, 28, 28, 0},
	{0xD718, 26, 27, 2},
	{0xD718, 24, 25, 1},
	{0xD718, 16, 23, 64},
	{0xD718, 0, 8, 0},
	{0xD719, 16, 23, 64},
	{0xD719, 0, 8, 0},
	{0xD71A, 24, 29, 8},
	{0xD71A, 16, 21, 16},
	{0xD71A, 8, 13, 32},
	{0xD71A, 0, 5, 32},
	{0xD71B, 24, 29, 32},
	{0xD71B, 16, 21, 32},
	{0xD71B, 8, 13, 32},
	{0xD71B, 0, 5, 32},
	{0xD71C, 0, 5, 32},
};

static struct di_tuning_reg_s di_dblk_param[REG_DBLK_MAX] = {
	{0xD108, 16, 19, 15},
	{0xD11B, 8, 13, 63},
	{0xD11B, 0, 5, 63},
	{0xD10C, 24, 29, 48},
	{0xD10C, 16, 21, 56},
	{0xD10C, 8, 15, 80},
	{0xD10C, 0, 7, 100},
	{0xD10E, 24, 29, 48},
	{0xD10E, 16, 21, 56},
	{0xD10E, 8, 15, 80},
	{0xD10E, 0, 7, 100},
};

static struct di_tuning_reg_s di_xlr_param[REG_XLR_MAX] = {
	{0xD023, 3, 3, 1},
	{0xD023, 2, 2, 1},
	{0xD120, 24, 29, 6},
	{0xD120, 16, 23, 64},
	{0xD120, 8, 13, 12},
	{0xD120, 0, 4, 3},
	{0xD121, 16, 23, 5},
	{0xD121, 8, 15, 10},
	{0xD121, 0, 7, 20},
	{0xD122, 24, 31, 20},
	{0xD122, 8, 16, 8},
	{0xD122, 0, 5, 3},
	{0xD123, 6, 7, 3},
	{0xD123, 4, 5, 1},
	{0xD123, 0, 0, 0},
};

struct di_pq_table_s di_pq_table[DI_PAGE_MODULE_MAX] = {
	/*xxx reg*/
	{
		0x10, REG_DM_MAX,
		{/*index 0*/
			{
				{
					{0xd251, 1, 0xf, 8, 4},
					{0xd25a, 1, 0x3f, 1, 24},
					{0xd25a, 1, 0x3f, 1, 16},
					{0xd25a, 1, 0x3f, 2, 8},
					{0xd25a, 1, 0x3f, 3, 0},
					{0xd259, 1, 0x3f, 4, 24},
					{0xd259, 1, 0x3f, 6, 16},
					{0xd259, 1, 0x3f, 8, 8},
					{0xd259, 1, 0x3f, 10, 0},
					{0xd258, 1, 0x3f, 12, 24},
					{0xd258, 1, 0x3f, 16, 16},
					{0xd258, 1, 0x3f, 20, 8},
					{0xd258, 1, 0x3f, 24, 0},
					{0xd257, 1, 0x3f, 30, 24},
					{0xd257, 1, 0x3f, 38, 16},
					{0xd257, 1, 0x3f, 48, 8},
					{0xd257, 1, 0x3f, 60, 0},
					{0xd25c, 1, 0x3f, 28, 24},
					{0xd25c, 1, 0x3f, 24, 16},
					{0xd25c, 1, 0x3f, 18, 8},
					{0xd25c, 1, 0x3f, 18, 0},
					{0xd25b, 1, 0x3f, 22, 24},
					{0xd25b, 1, 0x3f, 28, 16},
					{0xd25b, 1, 0x3f, 32, 8},
					{0xd25b, 1, 0x3f, 32, 0},
					{0xd25d, 1, 0xf,  15, 0},
					{0xa2d0, 1, 0x3ff, 120, 8},
					{0xa2d0, 1, 0x3f, 32, 0},
					{0xd250, 1, 0x3f, 14, 16},
					{0xd250, 1, 0x3f, 8, 8},
					{0xd250, 1, 0x3f, 2, 0},
					{0xd251, 1, 0x3f, 10, 12},
					{0xd252, 1, 0x3f, 16, 16},
					{0xd252, 1, 0x3f, 48, 8},
					{0xd252, 1, 0xf, 4, 4},
					{0xd252, 1, 0xf, 4, 0},
					{0xd253, 1, 0x3f, 8, 16},
					{0xd253, 1, 0x3f, 28, 8},
					{0xd253, 1, 0xf, 4, 4},
					{0xd253, 1, 0xf, 2, 0},
					{0xd254, 1, 0xff, 5, 20},
					{0xd254, 1, 0xff, 15, 12},
					{0xd254, 1, 0xff, 23, 4},
					{0xd254, 1, 0xf, 1, 0},
				},
			},
		},
	},
	/*xxx reg*/
	{
		0x12, REG_DBLK_MAX,
		{/*index 0*/
			{
				{
					{0xd108, 1, 0xf, 15, 16},
					{0xd11b, 1, 0x3f, 63, 8},
					{0xd11b, 1, 0x3f, 63, 0},
					{0xd10c, 1, 0x3f, 48, 24},
					{0xd10c, 1, 0x3f, 56, 16},
					{0xd10c, 1, 0xff, 80, 8},
					{0xd10c, 1, 0xff, 100, 0},
					{0xd10e, 1, 0x3f, 48, 24},
					{0xd10e, 1, 0x3f, 56, 16},
					{0xd10e, 1, 0xff, 80, 8},
					{0xd10e, 1, 0xff, 100, 0},
				},
			},
		},
	},
	/*local xxx reg*/
	{
		0x14, REG_DNR_MAX,
		{/*index 0*/
			{
				{
					{0xd235, 1, 0xff, 160, 16},
					{0xd235, 1, 0xff, 64, 8},
					{0xd235, 1, 0xff, 0, 0},
					{0xd237, 1, 0x3f, 8, 24},
					{0xd237, 1, 0xf, 2, 20},
					{0xd237, 1, 0x3f, 0, 12},
					{0xd237, 1, 0xf, 0, 8},
					{0xd23a, 1, 0x3f, 8, 4},
					{0xd23a, 1, 0xf, 0, 0},
					{0xd22f, 1, 0xf, 14, 4},
					{0xd22f, 1, 0xf, 6, 0},
					{0xd230, 1, 0x3f, -10, 24},
					{0xd230, 1, 0x3f, -4, 16},
					{0xd230, 1, 0x3f, 3, 8},
					{0xd230, 1, 0x3f, 10, 0},
					{0xd231, 1, 0xf, 8, 0},
					{0xd226, 1, 0xf, 8, 28},
					{0xd226, 1, 0xf, 9, 24},
					{0xd226, 1, 0xf, 10, 20},
					{0xd226, 1, 0xf, 11, 16},
					{0xd226, 1, 0xf, 12, 12},
					{0xd226, 1, 0xf, 13, 8},
					{0xd226, 1, 0xf, 14, 4},
					{0xd226, 1, 0xf, 15, 0},
					{0xd225, 1, 0x3f, 28, 24},
					{0xd225, 1, 0x3f, 32, 16},
					{0xd225, 1, 0x3f, 26, 8},
					{0xd225, 1, 0x3f, 20, 0},
					{0xd224, 1, 0x3f, 16, 24},
					{0xd224, 1, 0x3f, 22, 16},
					{0xd224, 1, 0x3f, 30, 8},
					{0xd224, 1, 0x3f, 38, 0},
					{0xd231, 1, 0x1, 1, 5},
					{0xd231, 1, 0x1, 0, 4},
					{0xd221, 1, 0x3f, 40, 24},
					{0xd221, 1, 0x3f, 40, 16},
					{0xd221, 1, 0x3f, 40, 8},
					{0xd221, 1, 0x3f, 40, 0},
					{0xd220, 1, 0x3f, 40, 24},
					{0xd220, 1, 0x3f, 40, 16},
					{0xd220, 1, 0x3f, 40, 8},
					{0xd220, 1, 0x3f, 40, 0},
				},
			},
		},
	},
	{
		0x16, REG_DCT_MAX,
		{/*index 0*/
			{
				{
					{0xD700, 1, 0x3, 3, 29},
					{0xD700, 1, 0x3, 3, 27},
					{0xB934, 1, 0x1, 1, 31},
					{0xB934, 1, 0x1, 0, 30},
					{0xB934, 1, 0x3f, 0, 24},
					{0xB934, 1, 0xff, 128, 16},
					{0xB934, 1, 0xf, 8, 12},
					{0xB934, 1, 0x1ff, 0, 0},
					{0xD716, 1, 0xff, 10, 24},
					{0xD716, 1, 0xff, 18, 16},
					{0xD716, 1, 0x1, 1, 15},
					{0xD716, 1, 0x3, 2, 13},
					{0xD716, 1, 0x1f, 10, 8},
					{0xD717, 1, 0x3, 2, 12},
					{0xD717, 1, 0xfff, 0, 0},
					{0xD718, 1, 0x1, 1, 31},
					{0xD718, 1, 0x1, 0, 30},
					{0xD718, 1, 0x1, 0, 29},
					{0xD718, 1, 0x1, 0, 28},
					{0xD718, 1, 0x3, 2, 26},
					{0xD718, 1, 0x3, 1, 24},
					{0xD718, 1, 0xff, 64, 16},
					{0xD718, 1, 0x1ff, 0, 0},
					{0xD719, 1, 0xff, 64, 16},
					{0xD719, 1, 0x1ff, 0, 0},
					{0xD71A, 1, 0x3f, 8, 24},
					{0xD71A, 1, 0x3f, 16, 16},
					{0xD71A, 1, 0x3f, 32, 8},
					{0xD71A, 1, 0x3f, 32, 0},
					{0xD71B, 1, 0x3f, 32, 24},
					{0xD71B, 1, 0x3f, 32, 16},
					{0xD71B, 1, 0x3f, 32, 8},
					{0xD71B, 1, 0x3f, 32, 0},
					{0xD71C, 1, 0x3f, 32, 0},
				},
			},
		},
	},
	{
		0x18, REG_XLR_MAX,
		{/*index 0*/
			{
				{
					{0XD023, 1, 0x1, 1, 3},
					{0XD023, 1, 0x1, 1, 2},
					{0XD120, 1, 0x3f, 6, 24},
					{0XD120, 1, 0xff, 64, 16},
					{0XD120, 1, 0xf, 12, 8},
					{0XD120, 1, 0x1f, 3, 0},
					{0XD121, 1, 0xff, 5, 16},
					{0XD121, 1, 0xff, 10, 8},
					{0XD121, 1, 0xff, 20, 0},
					{0XD122, 1, 0xff, 20, 24},
					{0XD122, 1, 0xff, 8, 8},
					{0XD122, 1, 0x3f, 3, 0},
					{0XD123, 1, 0x3, 3, 6},
					{0XD123, 1, 0x3, 1, 4},
					{0XD123, 1, 0x1, 0, 0},
				},
			},
		},
	},
};

unsigned int di_read_from_db_table(enum di_page_module_e module,
	int index, int nub)
{
	unsigned int val = 0;

	if (module > DI_PAGE_MODULE_MAX - 1 ||
		index > DI_PAGE_TBL_COUNT_MAX - 1)
		return val;

	if (nub < di_pq_table[module].count) {
		val = di_pq_table[module].page[index].reg[nub].val;
		if (pq_update == 2)
			pr_info("M%d: nub:%2d val:%2d,\n", module, nub, val);
	}
	return val;
}

void di_write_data_table(enum di_page_module_e module,
	int index)
{
	int i = 0;
	int val = 0;
	int tmp = 0;
	int tmp0 = 0;
	int mask = 0;
	unsigned int addr = 0;
	unsigned int start = 0;

	if (pq_update == 1)
		return;

	if (module > DI_PAGE_MODULE_MAX - 1 ||
		index > DI_PAGE_TBL_COUNT_MAX - 1)
		return;

	for (i = 0; i < di_pq_table[module].count; i++) {
		if (!di_pq_table[module].page[index].reg[i].update)
			continue;

		val = di_pq_table[module].page[index].reg[i].val;
		mask = di_pq_table[module].page[index].reg[i].mask_type;
		start = di_pq_table[module].page[index].reg[i].start_bit;
		addr =  di_pq_table[module].page[index].reg[i].addr;

		if (module == DI_PAGE_MODULE_DMS && i < DPSS_DB_DM_NUM)
			di_db_dm[i].val = di_pq_table[module].page[index].reg[i].val;
		if (module == DI_PAGE_MODULE_DCT && i < DPSS_DB_DM_NUM)
			di_db_dct[i].val = di_pq_table[module].page[index].reg[i].val;
		tmp = rd(addr);
		tmp0 = tmp;
		tmp &= ~(mask << start);
		tmp |= ((val & mask) << start);
		if (addr != VPU_NR_ENABLE)
			wr(addr, tmp);
		if (pq_update == 2)
			pr_info("M%d:tab[%2d][0x%04x] 0x%08x -> 0x%08x, st:%2d, val:%2d\n",
				module, i, addr, tmp0, tmp, start, val);
		di_pq_table[module].page[index].reg[i].update = 0;
	}
}

/*Internal functions*/
static void di_update_data_page_reg(struct di_tuning_page_s *pdata)
{
	int i, j;
	int reg_count = 0;

	struct di_tuning_reg_s *preg_list;
	struct di_tuning_reg_s *pcur_reg;
	struct di_pq_page_s *pcur_page;

	if (!pdata)
		return;

	if (!pdata->preg_list || pdata->reg_count == 0)
		return;

	preg_list = pdata->preg_list;

	for (i = 0; i < DI_PAGE_MODULE_MAX; i++) {
		if (di_pq_table[i].page_addr == pdata->page_addr) {
			pcur_page = &di_pq_table[i].page[0];
			reg_count = di_pq_table[i].count;
			break;
		}
	}

	for (i = 0; i < pdata->reg_count; i++) {
		pcur_reg = preg_list + i;
		for (j = 0; j < reg_count; j++) {
			if (pcur_page->reg[j].addr == pcur_reg->reg_addr &&
				pcur_page->reg[j].start_bit == pcur_reg->bit_start) {
				pcur_page->reg[j].val = pcur_reg->val;
				pcur_page->reg[j].update = 1;
				break;
			}
		}
	}
}

int di_set_demosquito_reg(struct di_demosquito_param_s *pdata)
{
	int i;
	struct di_tuning_page_s page;

	if (!pdata)
		return -1;

	for (i = 0; i < REG_DM_MAX; i++)
		di_demosquito_param[i].val = pdata->param[i];

	page.page_addr = di_pq_table[DI_PAGE_MODULE_DMS].page_addr;
	page.reg_count = REG_DM_MAX;
	page.page_idx = 0;
	page.preg_list = &di_demosquito_param[0];

	di_update_data_page_reg(&page);
	return 0;
}

int di_set_dblk_reg(struct di_dblk_param_s *pdata)
{
	int i;
	struct di_tuning_page_s page;

	if (!pdata)
		return -1;

	for (i = 0; i < REG_DBLK_MAX; i++)
		di_dblk_param[i].val = pdata->param[i];

	page.page_addr = di_pq_table[DI_PAGE_MODULE_DBLOCK].page_addr;
	page.reg_count = REG_DBLK_MAX;
	page.page_idx = 0;
	page.preg_list = &di_dblk_param[0];

	di_update_data_page_reg(&page);
	return 0;
}

int di_set_dnr_reg(struct di_dnr_param_s *pdata)
{
	int i;
	struct di_tuning_page_s page;

	if (!pdata)
		return -1;

	for (i = 0; i < REG_DNR_MAX; i++)
		di_dnr_param[i].val = pdata->param[i];

	page.page_addr = di_pq_table[DI_PAGE_MODULE_DNR].page_addr;
	page.reg_count = REG_DNR_MAX;
	page.page_idx = 0;
	page.preg_list = &di_dnr_param[0];

	di_update_data_page_reg(&page);
	return 0;
}

int di_set_dct_reg(struct di_dct_param_s *pdata)
{
	int i;
	struct di_tuning_page_s page;

	if (!pdata)
		return -1;

	for (i = 0; i < REG_DCT_MAX; i++)
		di_dct_param[i].val = pdata->param[i];

	page.page_addr = di_pq_table[DI_PAGE_MODULE_DCT].page_addr;
	page.reg_count = REG_DCT_MAX;
	page.page_idx = 0;
	page.preg_list = &di_dct_param[0];

	w_reg_bit(VPU_NR_SCENCE_CHANGE_FLAG, 0x1, 0, 1);//dpss_tnr_en = 0;
	sw_dct = 1;
	di_update_data_page_reg(&page);
	return 0;
}

int di_set_xlr_reg(struct di_xlr_param_s *pdata)
{
	int i;
	struct di_tuning_page_s page;

	if (!pdata)
		return -1;

	for (i = 0; i < REG_XLR_MAX; i++)
		di_xlr_param[i].val = pdata->param[i];

	page.page_addr = di_pq_table[DI_PAGE_MODULE_XLR].page_addr;
	page.reg_count = REG_XLR_MAX;
	page.page_idx = 0;
	page.preg_list = &di_xlr_param[0];

	di_update_data_page_reg(&page);
	return 0;
}

