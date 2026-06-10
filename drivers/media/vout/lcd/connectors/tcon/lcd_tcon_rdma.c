// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/kfifo.h>
#include "../../lcd_common.h"
#include "../../lcd_reg.h"
#include "lcd_tcon_rdma.h"

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#define RDMA_TABLE_SIZE (8 * (PAGE_SIZE))
#define TCON_RDMA_FIFO_ELEMENT_LEN 16  //element length

#define TCON_RDMA_REG_OPR_WR     0
#define TCON_RDMA_REG_OPR_WR_BIT 1

#define TCON_RDMA_CAP_WR BIT(0)
#define TCON_RDMA_CAP_RD BIT(1)

struct tcon_rdma_chip_s {
	unsigned int vcbus_ofs;
	unsigned int min_reg;
	unsigned int max_reg;
	unsigned int capacity;
};

struct tcon_reg_table_s {
	unsigned int opr;   //TCON_RDMA_REG_OPR_XXX
	unsigned int reg;   //tcon reg
	unsigned int vreg;  //vcbus_offset + reg
	unsigned int sbit;  //start bit
	unsigned int len;
	unsigned int val;
};

struct tcon_rdma_s {
	unsigned int en;
	unsigned int in_cnt;
	unsigned int out_cnt;
	u8 in_irq_cpuid;
	atomic_t in_irq_flag;
	struct tcon_rdma_chip_s *chip;

	DECLARE_KFIFO_PTR(reg_fifo, struct tcon_reg_table_s);

	spinlock_t lock;  //lock for tcon rdma
};

static struct tcon_rdma_s tcon_rdma;
static struct tcon_rdma_chip_s tcon_rdma_chip_t6d = {
	.vcbus_ofs = 0x00800000,
	.min_reg   = 0x100,
	.max_reg   = 0x599,
	.capacity  = TCON_RDMA_CAP_WR,
};

static struct tcon_rdma_chip_s tcon_rdma_chip_t6w = {
	.vcbus_ofs = 0x00790000,
	.min_reg   = 0x0,
	.max_reg   = 0x15ea,
	.capacity  = TCON_RDMA_CAP_WR,
};

static inline struct tcon_rdma_s *get_tcon_rdma(void)
{
	return &tcon_rdma;
}

int is_in_tcon_vsync_isr(u8 cur_cpuid)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();

	if (tcon_rdma->in_irq_cpuid != cur_cpuid)
		return 0;
	if (atomic_read(&tcon_rdma->in_irq_flag) > 0)
		return 1;
	else
		return 0;
}
EXPORT_SYMBOL(is_in_tcon_vsync_isr);

static inline bool is_valid_tcon_reg(struct tcon_rdma_s *tcon_rdma, u32 reg)
{
	return (reg >= tcon_rdma->chip->min_reg &&
			reg <= tcon_rdma->chip->max_reg);
}

int lcd_tcon_rdma_vs_handler(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	struct tcon_rdma_chip_s *chip = NULL;
	struct tcon_reg_table_s tcon_reg;
	unsigned long flags = 0;
	unsigned int val = 0;

	if (!tcon_rdma || !tcon_rdma->en)
		return -1;

	chip = tcon_rdma->chip;
	if (!chip || ((chip->capacity & TCON_RDMA_CAP_WR) == 0))
		return -1;

	tcon_rdma->in_irq_cpuid = smp_processor_id();

	spin_lock_irqsave(&tcon_rdma->lock, flags);
	atomic_set(&tcon_rdma->in_irq_flag, 1);
	while (kfifo_get(&tcon_rdma->reg_fifo, &tcon_reg) > 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
			LCDPR("cur rdma reg: opr=%u, reg=%#x, vreg=%#x, sbit=%u, len=%u, val=%#x\n",
				tcon_reg.opr, tcon_reg.reg, tcon_reg.vreg,
				tcon_reg.sbit, tcon_reg.len, tcon_reg.val);
		}
		tcon_rdma->out_cnt++;
		switch (tcon_reg.opr) {
		case TCON_RDMA_REG_OPR_WR:
			VSYNC_WR_MPEG_REG(tcon_reg.vreg, tcon_reg.val);
			break;
		case TCON_RDMA_REG_OPR_WR_BIT:
			if (chip->capacity & TCON_RDMA_CAP_RD) {
				VSYNC_WR_MPEG_REG_BITS(tcon_reg.vreg,
					tcon_reg.val, tcon_reg.sbit, tcon_reg.len);
			} else {
				val = lcd_tcon_read(pdrv, tcon_reg.reg);
				val = (val & (~(((1L << tcon_reg.len) - 1) << tcon_reg.sbit))) |
				    ((tcon_reg.val & ((1L << tcon_reg.len) - 1)) << tcon_reg.sbit);
				VSYNC_WR_MPEG_REG(tcon_reg.vreg, val);
			}
			break;
		default:
			break;
		}
	}
	spin_unlock_irqrestore(&tcon_rdma->lock, flags);
	atomic_set(&tcon_rdma->in_irq_flag, 0);

	return 0;
}

ssize_t lcd_tcon_rdma_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	unsigned long flags = 0;
	ssize_t size = 0;

	if (!tcon_rdma || !tcon_rdma->chip) {
		LCD_ERR(NULL, "Invalid tcon rdma\n");
		goto __rdma_dbg_show_exit;
	}

	spin_lock_irqsave(&tcon_rdma->lock, flags);
	size += sprintf(buf + size, "TCON RDMA info:\n");
	size += sprintf(buf + size, "  Enable  = %d\n", tcon_rdma->en);
	size += sprintf(buf + size, "  Capacity= %#x\n", tcon_rdma->chip->capacity);
	size += sprintf(buf + size, "\nTCON FIFO info:\n");
	size += sprintf(buf + size, "  EMPTY   = %d\n",
		kfifo_is_empty(&tcon_rdma->reg_fifo));
	size += sprintf(buf + size, "  FULL    = %d\n",
		kfifo_is_full(&tcon_rdma->reg_fifo));
	size += sprintf(buf + size, "  avail cnt = %d\n",
		kfifo_avail(&tcon_rdma->reg_fifo));
	size += sprintf(buf + size, "  used cnt  = %d\n",
		kfifo_len(&tcon_rdma->reg_fifo));
	size += sprintf(buf + size, "  fifo size = %d\n",
		kfifo_size(&tcon_rdma->reg_fifo));
	size += sprintf(buf + size, "  IN  cnt = %d\n", tcon_rdma->in_cnt);
	size += sprintf(buf + size, "  OUT cnt = %d\n", tcon_rdma->out_cnt);
	spin_unlock_irqrestore(&tcon_rdma->lock, flags);

__rdma_dbg_show_exit:
	return size;
}

static void lcd_tcon_rdma_cmd_help(void)
{
	pr_info("echo w/write <reg> <hex_val> > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("echo wb <reg> <start bit> <len> <hex_val> > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("echo r/read <reg> > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("echo en/enable > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("echo dis/disable > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("echo rst/reset > /sys/class/aml_tcon/tcon/tcon_rdma\n");
	pr_info("cat /sys/class/aml_tcon/tcon/tcon_rdma\n");
}

ssize_t lcd_tcon_rdma_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
#define __MAX_PARAM 16
	struct aml_lcd_drv_s *pdrv = dev_get_drvdata(dev);
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	char *buf_orig = NULL, *parm[__MAX_PARAM];
	unsigned int reg = 0, val = 0, sbit = 0, len = 0;
	unsigned long flags = 0;
	int ret = -1;

	if (!tcon_rdma || !tcon_rdma->chip) {
		LCD_ERR(NULL, "Invalid tcon rdma\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;

	lcd_debug_parse_param(buf_orig, parm, __MAX_PARAM);
	if (!strcmp(parm[0], "w") ||
			!strcmp(parm[0], "write")) {
		ret = kstrtouint(parm[1], 16, &reg);
		if (ret) {
			LCD_ERR(NULL, "Invalid reg\n");
			goto __tcon_rdma_store_exit;
		}
		ret = kstrtouint(parm[2], 16, &val);
		if (ret) {
			LCD_ERR(NULL, "Invalid val\n");
			goto __tcon_rdma_store_exit;
		}
		lcd_tcon_rdma_wr(pdrv, reg, val);
	} else if (!strcmp(parm[0], "wb")) {  //write bit
		ret = kstrtouint(parm[1], 16, &reg);
		if (ret) {
			LCD_ERR(NULL, "Invalid reg\n");
			goto __tcon_rdma_store_exit;
		}
		ret = kstrtouint(parm[2], 10, &sbit);
		if (ret) {
			LCD_ERR(NULL, "Invalid sbit\n");
			goto __tcon_rdma_store_exit;
		}
		ret = kstrtouint(parm[3], 10, &len);
		if (ret) {
			LCD_ERR(NULL, "Invalid len\n");
			goto __tcon_rdma_store_exit;
		}
		ret = kstrtouint(parm[4], 16, &val);
		if (ret) {
			LCD_ERR(NULL, "Invalid val\n");
			goto __tcon_rdma_store_exit;
		}
		lcd_tcon_rdma_wr_bits(pdrv, reg, val, sbit, len);
	} else if (!strcmp(parm[0], "r") ||
			!strcmp(parm[0], "read")) {
		ret = kstrtouint(parm[1], 16, &reg);
		if (ret) {
			LCD_ERR(NULL, "Invalid reg\n");
			goto __tcon_rdma_store_exit;
		}
		LCDPR("RD[%#x]=%#x\n", reg, lcd_tcon_read(pdrv, reg));
	} else if (!strcmp(parm[0], "en") ||
			!strcmp(parm[0], "enable")) {
		tcon_rdma->en = 1;
		LCDPR("Set tcon rdma enable\n");
	} else if (!strcmp(parm[0], "dis") ||
			!strcmp(parm[0], "disable")) {
		tcon_rdma->en = 0;
		LCDPR("Set tcon rdma disable\n");
	} else if (!strcmp(parm[0], "rst") ||
			!strcmp(parm[0], "reset")) {
		spin_lock_irqsave(&tcon_rdma->lock, flags);
		kfifo_reset(&tcon_rdma->reg_fifo);
		tcon_rdma->in_cnt = 0;
		tcon_rdma->out_cnt = 0;
		spin_unlock_irqrestore(&tcon_rdma->lock, flags);
		LCDPR("Clear fifo and counter\n");
	} else {
		LCD_ERR(NULL, "Not support cmd: %s\n", parm[0]);
		lcd_tcon_rdma_cmd_help();
	}

__tcon_rdma_store_exit:
	kfree(buf_orig);

	return count;
#undef __MAX_PARAM
}

int lcd_tcon_rdma_wr(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	struct tcon_reg_table_s tcon_reg;
	unsigned long flags = 0;

	if (!tcon_rdma || !tcon_rdma->en)
		return -1;

	if (!is_valid_tcon_reg(tcon_rdma, reg)) {
		LCD_ERR(NULL, "Invalid reg: %#x\n", reg);
		return -1;
	}

	memset(&tcon_reg, 0, sizeof(tcon_reg));
	tcon_reg.opr = TCON_RDMA_REG_OPR_WR;
	tcon_reg.reg = reg;
	tcon_reg.vreg = tcon_rdma->chip->vcbus_ofs + (reg << 2);
	tcon_reg.val = val;
	spin_lock_irqsave(&tcon_rdma->lock, flags);
	if (!kfifo_put(&tcon_rdma->reg_fifo, tcon_reg))
		LCD_ERR(NULL, "Tcon fifo full\n");
	else
		tcon_rdma->in_cnt++;
	spin_unlock_irqrestore(&tcon_rdma->lock, flags);
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("WR rdma reg: opr=%d, reg=%#x, sbit=%d, len=%d, val=%#x\n",
			tcon_reg.opr, tcon_reg.reg, tcon_reg.sbit,
			tcon_reg.len, tcon_reg.val);
	}

	return 0;
}

int lcd_tcon_rdma_wr_bits(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val,
		const u32 start, const u32 len)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	struct tcon_reg_table_s tcon_reg;
	unsigned long flags = 0;

	if (!tcon_rdma || !tcon_rdma->en)
		return -1;

	if (!is_valid_tcon_reg(tcon_rdma, reg)) {
		LCD_ERR(NULL, "Invalid reg: %#x\n", reg);
		return -1;
	}

	memset(&tcon_reg, 0, sizeof(tcon_reg));
	tcon_reg.opr = TCON_RDMA_REG_OPR_WR_BIT;
	tcon_reg.reg = reg;
	tcon_reg.vreg = tcon_rdma->chip->vcbus_ofs + (reg << 2);
	tcon_reg.sbit = start;
	tcon_reg.len = len;
	tcon_reg.val = val;
	spin_lock_irqsave(&tcon_rdma->lock, flags);
	if (!kfifo_put(&tcon_rdma->reg_fifo, tcon_reg))
		LCD_ERR(NULL, "Tcon fifo full\n");
	else
		tcon_rdma->in_cnt++;
	spin_unlock_irqrestore(&tcon_rdma->lock, flags);
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("WR rdma reg: opr=%d, reg=%#x, sbit=%d, len=%d, val=%#x\n",
			tcon_reg.opr, tcon_reg.reg, tcon_reg.sbit,
			tcon_reg.len, tcon_reg.val);
	}

	return 0;
}

int lcd_tcon_rdma_init(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rdma_chip_s *rdma_chip = NULL;
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	int ret = -1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T6D:
		rdma_chip = &tcon_rdma_chip_t6d;
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		rdma_chip = &tcon_rdma_chip_t6w;
		break;
	default:
		break;
	}

	if (!rdma_chip) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("not support tcon rdma\n");
		goto __tcon_rdma_init_exit;
	}

	tcon_rdma->chip = rdma_chip;
	tcon_rdma->in_cnt = 0;
	tcon_rdma->out_cnt = 0;

	ret = kfifo_alloc(&tcon_rdma->reg_fifo,
		TCON_RDMA_FIFO_ELEMENT_LEN,
		GFP_KERNEL);
	if (ret) {
		LCD_ERR(NULL, "rdma fifo buffer alloc fail\n");
		goto __tcon_rdma_init_exit;
	}
	spin_lock_init(&tcon_rdma->lock);
	atomic_set(&tcon_rdma->in_irq_flag, 0);
	tcon_rdma->en = 1;

	LCDPR("Tcon rdma is ready\n");

__tcon_rdma_init_exit:
	if (ret < 0)
		kfifo_free(&tcon_rdma->reg_fifo);
	return ret;
}

void lcd_tcon_rdma_remove(struct aml_lcd_drv_s *pdrv)
{
	struct tcon_rdma_s *tcon_rdma = get_tcon_rdma();
	unsigned long flags = 0;

	if (!tcon_rdma || !tcon_rdma->en)
		return;

	spin_lock_irqsave(&tcon_rdma->lock, flags);
	kfifo_free(&tcon_rdma->reg_fifo);
	tcon_rdma->en = 0;
	tcon_rdma->chip = NULL;
	atomic_set(&tcon_rdma->in_irq_flag, 0);
	spin_unlock_irqrestore(&tcon_rdma->lock, flags);
}

#else

int lcd_tcon_rdma_init(struct aml_lcd_drv_s *pdrv)
{
	return -1;
}

int lcd_tcon_rdma_wr(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val)
{
	return -1;
}

int lcd_tcon_rdma_wr_bits(struct aml_lcd_drv_s *pdrv, u32 reg, const u32 val,
		const u32 start, const u32 len)
{
	return -1;
}

ssize_t lcd_tcon_rdma_dbg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

ssize_t lcd_tcon_rdma_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	return count;
}

int lcd_tcon_rdma_vs_handler(struct aml_lcd_drv_s *pdrv)
{
	return -1;
}

void lcd_tcon_rdma_remove(struct aml_lcd_drv_s *pdrv)
{
}

int is_in_tcon_vsync_isr(u8 cur_cpuid)
{
	return 0;
}
EXPORT_SYMBOL(is_in_tcon_vsync_isr);

#endif

