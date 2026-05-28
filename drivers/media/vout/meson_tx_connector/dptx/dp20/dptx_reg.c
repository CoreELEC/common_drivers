// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include "dptx20_platform_reg.h"
#include "dptx_log.h"
#include "dptx_hw.h"
#include "dptx20_reg.h"
#include "dptx_hw_opcode.h"

//todo hdcp read/write reg
/* dptx core register read/write */
int dptx20_reg_write(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset, u32 value)
{
	if (io_type != CORE_LEVEL) {
		DPTX_ERROR("%s check reg 0x%x\n", __func__, offset);
		return -1;
	}
	DPTX_DEBUG_REG("%s offset = 0x%x, value = 0x%x\n", __func__, offset, value);
	writel(value, tx_comm->hw_base.regs_region[DPTX_COR_REG_IDX].p + offset);
	return 0;
}

u32 dptx20_reg_read(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset)
{
	if (io_type != CORE_LEVEL) {
		DPTX_ERROR("%s check reg 0x%x\n", __func__, offset);
		return 0;
	}

	return readl(tx_comm->hw_base.regs_region[DPTX_COR_REG_IDX].p + offset);
}

void dptx20_seq_rd_reg(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset, u8 *buf, u16 cnt)
{
	int i = 0;

	while (cnt--) {
		buf[i] = dptx20_reg_read(tx_comm, io_type, offset + i);
		i++;
	}
}

void dptx20_set_reg_bits(struct dptx_hw_common *tx_comm, u32 io_type,
	u32 addr, u32 value, u32 offset, u32 len)
{
	u32 data32 = 0;

	data32 = dptx20_reg_read(tx_comm, io_type, addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	dptx20_reg_write(tx_comm, io_type, addr, data32);
}

static inline void __iomem *get_reg_addr(struct dptx_hw_common *tx_comm, u32 idx)
{
	WARN_ON(!tx_comm);

	if (idx >= REG_IDX_MAX)
		return NULL;
	return tx_comm->hw_base.regs_region[idx].p;
}

/* platform register read/write */
u32 dptx20_rd_plt_reg(struct dptx_hw_common *tx_comm, u32 vaddr)
{
	u32 val = 0;
	void __iomem *reg;

	reg = get_reg_addr(tx_comm, vaddr >> BASE_REG_OFFSET) +
		(vaddr & ((1 << BASE_REG_OFFSET) - 1));
	WARN_ON(!reg);
	DPTX_DEBUG_REG("Rd32[0x%08x] 0x%08x\n", vaddr, val);

	return readl(reg);
}

void dptx20_wr_plt_reg(struct dptx_hw_common *tx_comm, u32 vaddr, u32 val)
{
	void __iomem *reg;

	reg = get_reg_addr(tx_comm, vaddr >> BASE_REG_OFFSET) +
		(vaddr & ((1 << BASE_REG_OFFSET) - 1));
	WARN_ON(!reg);
	writel(val, reg);
	DPTX_DEBUG_REG("Wr32[0x%08x] 0x%08x\n", vaddr, val);
}

void dptx20_set_plt_reg_bits(struct dptx_hw_common *tx_comm,
	u32 addr, u32 value, u32 offset, u32 len)
{
	u32 data32 = 0;

	data32 = dptx20_rd_plt_reg(tx_comm, addr);
	data32 &= ~(((1 << len) - 1) << offset);
	data32 |= (value & ((1 << len) - 1)) << offset;
	dptx20_wr_plt_reg(tx_comm, addr, data32);
}

