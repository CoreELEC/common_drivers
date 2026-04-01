// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/aml_spi.h>
#include "../../lcd_common.h"
#include "ldim_drv.h"
#include "ldim_dev_drv.h"

#define BROADCAST_SAME   0x00
#define BROADCAST_DIFF   0x7F
#define IW7039_CLASS_NAME "iw7039"

#define IW7039_REG_MAX             0x100
#define IW7039_REG_BRIGHTNESS_CHK  0x00
#define IW7039_REG_BRIGHTNESS      0x01
#define IW7039_REG_CHIPID          0x7f
#define IW7039_CHIPID              0x28

#define VSYNC_INFO_FREQUENT        300

/* for spi xfer */
static spinlock_t spi_lock;
static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

struct iw7039_s {
	struct spi_device *spi;
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned short fault_cnt;
	unsigned int fault_cnt_save;
	unsigned int fault_check_cnt;
	unsigned int reg_buf_size;
	unsigned int tbuf_size;
	unsigned int rbuf_size;
	int *iset;

	unsigned short *reg_buf; /* local dimming driver smr api usage */

	/* spi api internal used, don't use outside!!! */
	unsigned char *tbuf;
	unsigned char *rbuf;
};

struct iw7039_s *bl_iw7039;

unsigned short init_00[32] = {
	0x0802, 0x03a2, 0x0000, 0x0ffe, 0x3ffb, 0x43a7, 0x4585, 0x8190,
	0x0100, 0xa140, 0x0000, 0x0834, 0x0044, 0x5840, 0x3737, 0xcc82,
	0x3fff, 0xffff, 0x01e1, 0x01e1, 0x0118, 0xc000, 0x0000, 0x0000,
	0xffff, 0xffff, 0x0000, 0x01ff, 0x4fff, 0x1fff, 0x3fff, 0xffff
};

unsigned short init_buf[32];

/* state device number */
static int spi_device_number(struct spi_device *spi, unsigned short chip_cnt)
{
	unsigned char *tbuf;
	int ret;
	unsigned short chip_id = 0x7E;

	if (!bl_iw7039 || !bl_iw7039->tbuf) {
		LDIMERR("%s: bl_iw7039 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7039->tbuf;

	tbuf[0] = 0x80 | chip_id;
	tbuf[1] = chip_cnt & 0x7f;
	tbuf[2] = 0;
	tbuf[3] = 0;
	tbuf[4] = 0;
	tbuf[5] = 0;

	ret = ldim_spi_write(spi, tbuf, 6);

	return ret;
}

/* write same data to all device */
static int spi_wregs_all(struct spi_device *spi, unsigned int chip_cnt,
			 unsigned short reg, unsigned short *data_buf, int tlen)
{
	unsigned char *tbuf;
	int n, xlen, ret, i, j;

	if (!bl_iw7039 || !bl_iw7039->tbuf) {
		LDIMERR("%s: bl_iw7039 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7039->tbuf;

	if (tlen == 0) {
		LDIMERR("%s: tlen is 0\n", __func__);
		return -1;
	}
	n = 1 + (chip_cnt - 1) / 16;/*dummy count*/
	if (tlen == 1)
		xlen = 2 + 1 + n;
	else
		xlen = 2 + tlen + n;
	xlen = 2 * xlen;//16-->8bit
	if (bl_iw7039->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7039->tbuf_size);
		return -1;
	}

	if (tlen == 1) {
		tbuf[0] = 0x80 | BROADCAST_SAME;
		tbuf[1] = 0x01;
		tbuf[2] = reg >> 8;
		tbuf[3] = reg & 0xff;
		tbuf[4] = data_buf[0] >> 8;
		tbuf[5] = data_buf[0] & 0xff;
		memset(&tbuf[6], 0, 2 * n);
	} else {
		tbuf[0] = 0x80 | BROADCAST_SAME;
		tbuf[1] = tlen;
		tbuf[2] = reg >> 8;
		tbuf[3] = reg & 0xff;
		j = 4;
		for (i = 0; i < tlen; i++) {
			tbuf[j++] = data_buf[i] >> 8;
			tbuf[j++] = data_buf[i] & 0xff;
		}
		memset(&tbuf[j], 0, 2 * n);
	}
	ret = ldim_spi_write(spi, tbuf, xlen);

	return ret;
}

/* write diff data to all device */
static int spi_wregs_duty(struct spi_device *spi, unsigned int chip_cnt, unsigned char trig_mode,
			  unsigned short reg, unsigned short *data_buf, int tlen)
{
	unsigned char *tbuf, *rbuf;
	int i, j, k, p, n, xlen, ret;

	if (!bl_iw7039 || !bl_iw7039->tbuf) {
		LDIMERR("%s: bl_iw7039 or tbuf is null\n", __func__);
		return -1;
	}
	tbuf = bl_iw7039->tbuf;
	rbuf = bl_iw7039->rbuf;

	if (tlen == 0) {
		LDIMERR("%s: tlen is 0\n", __func__);
		return -1;
	}
	n = 1 + (chip_cnt - 1) / 16;
	if (tlen == 1)
		xlen = 2 + chip_cnt + n;
	else
		xlen = 2 + tlen * chip_cnt + n;
	xlen = xlen * 2;
	xlen = ldim_spi_dma_cycle_align_byte(xlen);
	if (bl_iw7039->tbuf_size < xlen) {
		LDIMERR("%s: tbuf_size %d is not enough\n", __func__, bl_iw7039->tbuf_size);
		return -1;
	}
	if (bl_iw7039->rbuf_size < xlen) {
		LDIMERR("%s: rbuf_size %d is not enough\n", __func__, bl_iw7039->rbuf_size);
		return -1;
	}

	if (tlen == 1) {
		tbuf[0] = 0x80 | BROADCAST_DIFF;
		tbuf[1] = 0x01;
		tbuf[2] = reg >> 8;
		tbuf[3] = reg & 0xff;
		j = 4;
		for (i = 0; i < chip_cnt; i++) {
			tbuf[j++] = data_buf[0] >> 8;
			tbuf[j++] = data_buf[0] & 0xff;
		}
		memset(&tbuf[j], 0, 2 * n);
	} else {
		tbuf[0] = 0x80 | BROADCAST_DIFF;
		tbuf[1] = tlen;
		tbuf[2] = reg >> 8;
		tbuf[3] = reg & 0xff;
		j = 4;
		p = 0;
		for (i = 0; i < chip_cnt; i++) {
			for (k = 0; k < tlen; k++) {
				tbuf[j++] = data_buf[p] >> 8;
				tbuf[j++] = data_buf[p] & 0xff;
				p++;
			}
		}
		memset(&tbuf[j], 0, 2 * n);
	}

	switch (trig_mode) {
	case SPI_ASYNC:
		ret = ldim_spi_write_async(spi, tbuf, rbuf, xlen, bl_iw7039->tbuf_size);
		break;
	default:
		ret = ldim_spi_write_dma_trig(spi, tbuf, rbuf, xlen, bl_iw7039->tbuf_size);
		break;
	}

	return ret;
}

static int iw7039_reg_write_all(struct ldim_dev_driver_s *dev_drv,
				unsigned short reg, unsigned short *buf, unsigned int len)
{
	int ret;

	mutex_lock(&spi_mutex);

	ret = spi_wregs_all(bl_iw7039->spi, dev_drv->chip_cnt, reg, buf, len);
	if (ret)
		LDIMERR("%s: reg 0x%x, len %d error\n", __func__, reg, len);

	mutex_unlock(&spi_mutex);

	return ret;
}

static int iw7039_reg_write_duty(struct ldim_dev_driver_s *dev_drv,
				 unsigned short reg, unsigned short *buf, unsigned int len)
{
	int ret;
	unsigned long flags = 0;

	spin_lock_irqsave(&spi_lock, flags);

	ret = spi_wregs_duty(bl_iw7039->spi, dev_drv->chip_cnt, dev_drv->trig_mode, reg, buf, len);
	if (ret)
		LDIMERR("%s: reg 0x%x, len %d error\n", __func__, reg, len);

	spin_unlock_irqrestore(&spi_lock, flags);

	return ret;
}

static int ldim_power_cmd_dynamic_size(struct ldim_dev_driver_s *dev_drv)
{
	unsigned char *table;
	int i = 0, j, step = 0, max_len = 0;
	unsigned char type, cmd_size;
	int delay_ms, ret = 0;
	unsigned short val = 0;

	table = dev_drv->init_on;
	max_len = dev_drv->init_on_cnt;

	while ((i + 1) < max_len) {
		type = table[i];
		if (type == LCD_EXT_CMD_TYPE_END)
			break;
		if (ldim_debug_print) {
			LDIMPR("%s: step %d: type=0x%02x, cmd_size=%d\n",
			       __func__, step, type, table[i + 1]);
		}
		cmd_size = table[i + 1];
		if (cmd_size == 0)
			goto power_cmd_dynamic_next;
		if ((i + 2 + cmd_size) > max_len)
			break;

		if (type == LCD_EXT_CMD_TYPE_NONE) {
			/* do nothing */
		} else if (type == LCD_EXT_CMD_TYPE_DELAY) {
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += table[i + 2 + j];
			if (delay_ms > 0)
				lcd_delay_ms(delay_ms);
		} else if (type == LCD_EXT_CMD_TYPE_CMD) {
			val = (table[i + 3] << 8) | table[i + 4];
			ret = iw7039_reg_write_all(dev_drv, table[i + 2], &val, 1);
			udelay(1);
		} else {
			LDIMERR("%s: type 0x%02x invalid\n", __func__, type);
		}
power_cmd_dynamic_next:
		i += (cmd_size + 2);
		step++;
	}

	return ret;
}

static int iw7039_power_on_init(struct ldim_dev_driver_s *dev_drv)
{
	int ret = 0;

	if (dev_drv->cmd_size < 1) {
		LDIMERR("%s: cmd_size %d is invalid\n",
			__func__, dev_drv->cmd_size);
		return -1;
	}
	if (!dev_drv->init_on) {
		LDIMERR("%s: init_on is null\n", __func__);
		return -1;
	}

	if (dev_drv->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC)
		ret = ldim_power_cmd_dynamic_size(dev_drv);

	return ret;
}

static int iw7039_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	unsigned short temp[2];
	int i;

	LDIMPR("start init on iw7039 hw\n");

	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: delay for internal logic stable */
	lcd_delay_ms(10);

	spi_device_number(bl_iw7039->spi, dev_drv->chip_cnt);

	iw7039_reg_write_all(dev_drv, 0x00, init_00, 0x20);

	temp[0] = 0xa5ff;
	iw7039_reg_write_all(dev_drv, 0xa0, temp, 1);

	for (i = 0; i < 32; i++)
		init_buf[i] = (unsigned short)bl_iw7039->iset[i];
	iw7039_reg_write_all(dev_drv, 0x20, init_buf, 0x20);

	for (i = 0; i < 32; i++)
		init_buf[i] = 0xfff;
	iw7039_reg_write_all(dev_drv, 0x60, init_buf, 0x20);

	temp[0] = 0xa533;
	iw7039_reg_write_all(dev_drv, 0xa0, temp, 1);

	lcd_delay_ms(50);

	temp[0] = 0x0803;
	iw7039_reg_write_all(dev_drv, 0x00, temp, 1);

	lcd_delay_ms(500);

	if (i == 0xffff)
		iw7039_power_on_init(dev_drv);
	/* step 15: calibration done */
	LDIMPR("%s: calibration done\n", __func__);

	return 0;
}

static int iw7039_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static void ldim_vs_debug_info(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, k, zone_max;

	if (bl_iw7039->vsync_cnt)
		return;
	if ((ldim_debug_print & LDIM_DBG_PR_DEV_DBG_INFO) == 0)
		return;

	zone_max = bl_iw7039->reg_buf_size;
	for (i = 0; i < dev_drv->zone_num; i++) {
		j = dev_drv->bl_mapping[i];
		if (j >= zone_max) {
			LDIMERR("mapping[%d]=%d invalid, max %d\n",
			       i, j, zone_max);
			return;
		}
		k = (j / 32) * 96 + j % 32;
		LDIMPR("zone[%d]: reg_buf:[%d]=0x%03x : 0x%03x : 0x%03x\n", i, j,
		bl_iw7039->reg_buf[k], bl_iw7039->reg_buf[k + 32], bl_iw7039->reg_buf[k + 64]);
	}
	pr_info("\n");
}

static inline void ldim_data_mapping(struct aml_ldim_driver_s *ldim_drv, unsigned int *duty_buf,
				     unsigned int max, unsigned int min,
				     unsigned int zone_num,
				     unsigned short *mapping)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, k, val, zone_max;
	int iled;

	if (dev_drv->boost_conf.en)
		iled = dev_drv->boost_conf.i_cur;
	else
		iled = dev_drv->boost_conf.i_l100;

	zone_max = bl_iw7039->reg_buf_size;
	for (i = 0; i < zone_num; i++) {
		val = min + ((duty_buf[i] * (max - min)) / LD_DATA_MAX);
		j = mapping[i];
		if (j >= zone_max) {
			if (bl_iw7039->vsync_cnt == 0) {
				LDIMPR("%s: mapping[%d]=%d invalid, max %d\n",
				       __func__, i, j, zone_max);
			}
			return;
		}
		k = (j / 32) * 96 + j % 32;
		if (dev_drv->boost_conf.mode == 2) {
			bl_iw7039->reg_buf[k] = bl_iw7039->iset[i];
			bl_iw7039->reg_buf[k + 64] = val;
		} else {//mode == 1
			bl_iw7039->reg_buf[k] = iled * 1023 / 66;
			bl_iw7039->reg_buf[k + 64] = val;
		}
	}
}

static int iw7039_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;

	if (!bl_iw7039)
		return -1;

	if (bl_iw7039->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_iw7039->vsync_cnt = 0;

	if (bl_iw7039->dev_on_flag == 0) {
		if (bl_iw7039->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_iw7039->dev_on_flag);
		return 0;
	}
	if (len != dev_drv->zone_num) {
		if (bl_iw7039->vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (!bl_iw7039->reg_buf) {
		if (bl_iw7039->vsync_cnt == 0)
			LDIMERR("%s: reg_buf is null\n", __func__);
		return -1;
	}

	ldim_data_mapping(ldim_drv, buf, dev_drv->dim_max, dev_drv->dim_min,
		dev_drv->zone_num, dev_drv->bl_mapping);
	//ldim_data_mapping_mix_boost(ldim_drv);
	ldim_vs_debug_info(ldim_drv);

	iw7039_reg_write_duty(dev_drv, 0x20, bl_iw7039->reg_buf, 96);

	return 0;
}

static int iw7039_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7039)
		return -1;

	if (bl_iw7039->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_iw7039->vsync_cnt = 0;

	return 0;
}

static int iw7039_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;

	if (dev_drv->fault_check == 0)
		return 0;

	if (!bl_iw7039)
		return 0;
	if (bl_iw7039->dev_on_flag == 0) {
		if (bl_iw7039->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_iw7039->dev_on_flag);
		return 0;
	}

	mutex_lock(&dev_mutex);
	//step1: check dev_drv->lamp_err_gpio
	//step2: if error, change bl_iw7039->dev_on_flag to 0, and reset iw7039
	//step3: ret = -1, so main driver can refresh bl_duty after reset dev
	mutex_unlock(&dev_mutex);

	return ret;
}

static int iw7039_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7039)
		return -1;

	if (bl_iw7039->dev_on_flag) {
		LDIMPR("%s: iw7039 is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	ldim_spi_async_busy_clear();
	iw7039_hw_init_on(ldim_drv->dev_drv);
	bl_iw7039->dev_on_flag = 1;
	bl_iw7039->vsync_cnt = 0;
	bl_iw7039->fault_cnt = 0;
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7039_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7039)
		return -1;

	mutex_lock(&dev_mutex);
	bl_iw7039->dev_on_flag = 0;
	iw7039_hw_init_off(ldim_drv->dev_drv);
	ldim_spi_async_busy_clear();
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int iw7039_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

//	LDIMPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static ssize_t iw7039_show(const struct class *class, const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_iw7039)
		return sprintf(buf, "bl_iw7039 is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (!strcmp(attr->attr.name, "iw7039_status")) {
		len = sprintf(buf, "iw7039 status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n"
				"fault_cnt      = %d\n"
				"reg_buf_size   = %d\n"
				"tbuf_size      = %d\n"
				"rbuf_size      = %d\n"
				"en_on          = %d\n"
				"en_off         = %d\n"
				"dim_max        = 0x%03x\n"
				"dim_min        = 0x%03x\n",
				dev_drv->index,
				bl_iw7039->dev_on_flag,
				bl_iw7039->vsync_cnt,
				bl_iw7039->fault_cnt,
				bl_iw7039->reg_buf_size,
				bl_iw7039->tbuf_size,
				bl_iw7039->rbuf_size,
				dev_drv->en_gpio_on,
				dev_drv->en_gpio_off,
				dev_drv->dim_max,
				dev_drv->dim_min);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t iw7039_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	unsigned int val;
	int i;

	dev_drv = ldim_drv->dev_drv;
	if (!strcmp(attr->attr.name, "init")) {
		mutex_lock(&dev_mutex);
		iw7039_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(attr->attr.name, "duty")) {
		if (kstrtouint(buf, 0, &val) < 0)
			goto iw7039_store_err;

		for (i = 0; i < 32; i++)
			init_buf[i] = (unsigned short)val;
		iw7039_reg_write_all(dev_drv, 0x60, init_buf, 0x20);

	} else if (!strcmp(attr->attr.name, "iset")) {
		if (kstrtouint(buf, 0, &val) < 0)
			goto iw7039_store_err;

		for (i = 0; i < 32; i++)
			init_buf[i] = (unsigned short)val;
		iw7039_reg_write_all(dev_drv, 0x20, init_buf, 0x20);

	} else {
		LDIMERR("argument error!\n");
	}
	return count;
iw7039_store_err:
	LDIMERR("%s: invalid args\n", __func__);
	return -1;
}

static struct class_attribute iw7039_class_attrs[] = {
	__ATTR(init, 0644, NULL, iw7039_store),
	__ATTR(duty, 0644, NULL, iw7039_store),
	__ATTR(iset, 0644, NULL, iw7039_store),
	__ATTR(iw7039_status, 0644, iw7039_show, NULL),
};

static int iw7039_ldim_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = iw7039_power_on;
	dev_drv->power_off = iw7039_power_off;
	dev_drv->dev_transmit = iw7039_transmit;
	dev_drv->dev_transmit_dummy = iw7039_transmit_dummy;
	dev_drv->dev_fb_handle = iw7039_fb_handle;
	dev_drv->config_update = iw7039_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_iw7039_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	int n, i;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}
	if (!dev_drv->spi_dev[0]) {
		LDIMERR("%s: spi_dev[0] is null\n", __func__);
		return -1;
	}

	if (!dev_drv->spi_dev[1] && dev_dev->spi_dev_num == 2) {
		LDIMERR("%s: spi_dev[1] is null\n", __func__);
		return -1;
	}
	if (dev_drv->chip_cnt == 0) {
		LDIMERR("%s: chip_cnt is 0\n", __func__);
		return -1;
	}

	bl_iw7039 = kzalloc(sizeof(*bl_iw7039), GFP_KERNEL);
	if (!bl_iw7039) {
		LDIMERR("%s: bl_iw7039 alloc failed\n", __func__);
		return -1;
	}

	bl_iw7039->spi = dev_drv->spi_dev[0];
	bl_iw7039->dev_on_flag = 0;
	bl_iw7039->vsync_cnt = 0;
	bl_iw7039->fault_cnt = 0;
	bl_iw7039->iset = dev_drv->boost_conf.iset;

	/* 32 channel each device */
	bl_iw7039->reg_buf_size = 32 * dev_drv->chip_cnt * 3;
	bl_iw7039->reg_buf = kcalloc(bl_iw7039->reg_buf_size,
		sizeof(unsigned short), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7039->reg_buf)
		goto ldim_dev_iw7039_probe_err0;

	/* spi transfer buffer: header + reg_max_cnt * chip_cnt */
	bl_iw7039->tbuf_size = IW7039_REG_MAX + bl_iw7039->reg_buf_size * 2;
	if (dev_drv->spi_dev[0]->controller->dma_alignment) {
		n = bl_iw7039->tbuf_size;
		bl_iw7039->tbuf_size = ldim_spi_dma_cycle_align_byte(n);
	}
	bl_iw7039->tbuf = kcalloc(bl_iw7039->tbuf_size,
		sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7039->tbuf)
		goto ldim_dev_iw7039_probe_err1;

	/* spi transfer buffer: header + reg_max_cnt + chip_cnt + dev_id_max(=chip_cnt) */
	bl_iw7039->rbuf_size = bl_iw7039->tbuf_size;
	bl_iw7039->rbuf = kcalloc(bl_iw7039->rbuf_size,
		sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_iw7039->rbuf)
		goto ldim_dev_iw7039_probe_err2;

	/*set spi_xlen equal as tbuf_size*/
	cdata = bl_iw7039->spi->controller_data;
	if (!cdata) {
		LDIMERR("%s:  controller_data is null\n", __func__);
		goto ldim_dev_iw7039_probe_err3;
	}

	priv = cdata->priv;
	if (priv) {
		priv->xlen = bl_iw7039->tbuf_size;
	} else {
		LDIMERR("%s:  priv is null\n", __func__);
		goto ldim_dev_iw7039_probe_err3;
	}

	ldim_drv->trig_param.spi_tbuf_size = bl_iw7039->tbuf_size;
	// update ldc line_n isr for ldim driver
	ldim_trig_line_update(ldim_drv);
	iw7039_ldim_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(iw7039_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &iw7039_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					iw7039_class_attrs[i].attr.name);
			}
		}
	}

	if (dev_drv->trig_mode == SPI_ASYNC)
		spin_lock_init(&spi_lock);

	if (bdrv->state & BL_STATE_BL_ON)
		bl_iw7039->dev_on_flag = 1; /* default enable in uboot */

	LDIMPR("%s ok\n", __func__);
	return 0;

ldim_dev_iw7039_probe_err3:
	kfree(bl_iw7039->rbuf);
	bl_iw7039->rbuf_size = 0;
ldim_dev_iw7039_probe_err2:
	kfree(bl_iw7039->tbuf);
	bl_iw7039->tbuf_size = 0;
ldim_dev_iw7039_probe_err1:
	kfree(bl_iw7039->reg_buf);
	bl_iw7039->reg_buf_size = 0;
ldim_dev_iw7039_probe_err0:
	kfree(bl_iw7039);
	bl_iw7039 = NULL;
	LDIMERR("%s fail\n", __func__);
	return -1;
}

int ldim_dev_iw7039_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_iw7039)
		return 0;

	kfree(bl_iw7039->rbuf);
	bl_iw7039->rbuf_size = 0;
	kfree(bl_iw7039->tbuf);
	bl_iw7039->tbuf_size = 0;
	kfree(bl_iw7039->reg_buf);
	bl_iw7039->reg_buf_size = 0;
	kfree(bl_iw7039);
	bl_iw7039 = NULL;

	return 0;
}
