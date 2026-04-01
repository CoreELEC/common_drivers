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

#define NORMAL_MSG      (0 << 7)
#define BROADCAST_MSG   BIT(7)
#define BLOCK_DATA      (0 << 6)
#define SINGLE_DATA     BIT(6)
#define BLMCU_CLASS_NAME "blmcu"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

struct blmcu_s {
	struct spi_device *spi;
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned int rbuf_size;
	unsigned int tbuf_size;
	unsigned char header_cnt;
	unsigned char *header_data;
	unsigned char extend_cnt;
	unsigned char *extend_data;
	unsigned char adim_width;
	unsigned char pdim_width;
	unsigned short adim;	/*1byte 0x4d 30%duty */
	unsigned short pdim;	/*1byte 0xff */
	int *idim; /*current setting for each zone*/
	unsigned char datawidth;	/*0:8bit, 1:12bit, 2:16bit */
	unsigned char ext_len;
	unsigned int apl;
	unsigned int zone_num;
	unsigned int duty_size;
	unsigned char *rbuf;
	unsigned char *tbuf;

	/*for 2nd spi device*/
	struct spi_device *spi1;
	unsigned int rbuf_size1;
	unsigned int tbuf_size1;
	unsigned char *rbuf1;
	unsigned char *tbuf1;
};

struct blmcu_s *bl_mcu;

static int blmcu_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	LDIMPR("%s\n", __func__);

	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: delay for internal logic stable */
	lcd_delay_ms(dev_drv->hw_on_delay);

	/* step 3: Generate external VSYNC to VSYNC/PWM pin */
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);

	return 0;
}

static int blmcu_hw_init_off(struct ldim_dev_driver_s *dev_drv)
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
	unsigned int i;

	if (bl_mcu->vsync_cnt) //300 vsync print once
		return;
	if ((ldim_debug_print & LDIM_DBG_PR_DEV_DBG_INFO) == 0)
		return;

	LDIMPR("%s:\n", __func__);

	for (i = 0; i < bl_mcu->tbuf_size; i++)
		LDIMPR("tbuf[%d]: 0x%x\n", i, bl_mcu->tbuf[i]);

	/*for 2nd spi*/
	if (dev_drv->spi_conf.spi_dev_num == 2) {
		for (i = 0; i < bl_mcu->tbuf_size1; i++)
			LDIMPR("tbuf[%d]: 0x%x\n", i, bl_mcu->tbuf1[i]);
	}

	pr_info("\n");
}

static inline void ldim_data_mapping(struct aml_ldim_driver_s *ldim_drv,
	unsigned int *duty_buf, unsigned short *bl_mapping)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int i, j, val, apl, k;
	unsigned int zone_num = bl_mcu->zone_num;
	unsigned char is_need_mapping = dev_drv->spi_conf.data_pkt.datamapping_en;

	if (is_need_mapping == 0)
		return;

	j = 0;
	apl = 0;
	for (i = 0; i < zone_num; i++) {
		val = duty_buf[i];
		apl += val;
		if (bl_mcu->datawidth == 16) { //16bits (6+10)
			if (!bl_mcu->idim) {
				if (bl_mcu->vsync_cnt == 0)
					LDIMERR("%s:idim is null", __func__);
				return;
			}
			bl_mcu->rbuf[j] = ((bl_mcu->idim[i] << 2) | (val >> 10)) & 0xff;
			bl_mcu->rbuf[j + 1] = (val >> 2) & 0xff;
			j += 2;
		} else if (bl_mcu->datawidth == 12) { //12bits
			if (i % 2 == 0) {
				bl_mcu->rbuf[j] = (val >> 4) & 0xff;
				bl_mcu->rbuf[j + 1] = ((val & 0xf) << 4) & 0xff;
			} else {
				bl_mcu->rbuf[j + 1] |= (val >> 8) & 0xf;
				bl_mcu->rbuf[j + 2] = val & 0xff;
				j += 3;
			}
		} else { //8bit
			bl_mcu->rbuf[j] = (val >> 4) & 0xff;
			j++;
		}
	}

	bl_mcu->apl = apl / zone_num;

	if (bl_mcu->header_data) {
		for (i = 0; i < bl_mcu->header_cnt; i++)
			bl_mcu->tbuf[i] = (unsigned char)bl_mcu->header_data[i];
	}

	for (i = 0; i < bl_mcu->duty_size; i++)
		bl_mcu->tbuf[i + bl_mcu->header_cnt] = bl_mcu->rbuf[i];

	k = bl_mcu->duty_size + bl_mcu->header_cnt;
	switch (bl_mcu->extend_cnt) {
	case 3:// 3byte tail
		bl_mcu->tbuf[k + 0] = bl_mcu->pdim;
		bl_mcu->tbuf[k + 1] = bl_mcu->adim;
		bl_mcu->tbuf[k + 2] = (bl_mcu->apl >> 4) & 0xff;  //apl
		break;
	case 6:// 6byte tail
		bl_mcu->tbuf[k + 0] = bl_mcu->pdim;
		bl_mcu->tbuf[k + 1] = bl_mcu->adim;
		bl_mcu->tbuf[k + 2] = (bl_mcu->apl >> 8) & 0xff;  //apl
		bl_mcu->tbuf[k + 3] = bl_mcu->apl & 0xff;  //apl
		bl_mcu->tbuf[k + 4] = 0xff;  //reseve
		bl_mcu->tbuf[k + 5] = 0xff;  //reserve
		break;
	default:
		break;
	}

	k = bl_mcu->duty_size + bl_mcu->ext_len;
	for (i = k; i < bl_mcu->tbuf_size; i++)
		bl_mcu->tbuf[i] = 0;

	/*for 2nd spi*/
	/* here For reference only, need to be set according to the actual device spec!!!*/
	if (dev_drv->spi_conf.spi_dev_num == 2)
		memcpy(bl_mcu->tbuf1, bl_mcu->tbuf, bl_mcu->tbuf_size);
}

static int blmcu_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;

	if (!bl_mcu)
		return -1;

	if (bl_mcu->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_mcu->vsync_cnt = 0;

	if (bl_mcu->dev_on_flag == 0) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMPR("%s: on_flag=%d\n", __func__, bl_mcu->dev_on_flag);
		return 0;
	}
	if (len != dev_drv->zone_num) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: data len %d invalid\n", __func__, len);
		return -1;
	}
	if (!bl_mcu->rbuf) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: rbuf is null\n", __func__);
		return -1;
	}
	if (!bl_mcu->tbuf) {
		if (bl_mcu->vsync_cnt == 0)
			LDIMERR("%s: tbuf is null\n", __func__);
		return -1;
	}

	bl_mcu->pdim = (dev_drv->dim_pdim >> 4) & 0xff;

	ldim_data_mapping(ldim_drv, buf, dev_drv->bl_mapping);
	ldim_vs_debug_info(ldim_drv);

	switch (dev_drv->spi_conf.trig_mode) {
	case SPI_ASYNC:
		ret = ldim_spi_write_async(bl_mcu->spi, bl_mcu->tbuf, bl_mcu->rbuf,
		bl_mcu->tbuf_size, bl_mcu->tbuf_size);
		/*for test 2nd spi*/
		if (dev_drv->spi_conf.spi_dev_num == 2)
			ret |=  ldim_spi_write_async(bl_mcu->spi1, bl_mcu->tbuf1, bl_mcu->rbuf1,
		bl_mcu->tbuf_size1, bl_mcu->tbuf_size1);
		break;
	default:
		ret = ldim_spi_write_dma_trig(bl_mcu->spi, bl_mcu->tbuf, bl_mcu->rbuf,
		bl_mcu->tbuf_size, bl_mcu->tbuf_size);
		/*for test 2nd spi*/
		if (dev_drv->spi_conf.spi_dev_num == 2)
			ret |= ldim_spi_write_dma_trig(bl_mcu->spi1, bl_mcu->tbuf1, bl_mcu->rbuf1,
		bl_mcu->tbuf_size1, bl_mcu->tbuf_size1);
		break;
	}
	return ret;
}

static int blmcu_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int blmcu_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;
	return ret;
}

static int blmcu_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	LDIMPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static int blmcu_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_mcu)
		return -1;
	if (bl_mcu->dev_on_flag) {
		LDIMPR("%s: blmcu is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	blmcu_hw_init_on(ldim_drv->dev_drv);
	bl_mcu->dev_on_flag = 1;
	bl_mcu->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static int blmcu_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_mcu)
		return -1;

	mutex_lock(&dev_mutex);
	bl_mcu->dev_on_flag = 0;
	blmcu_hw_init_off(ldim_drv->dev_drv);
	ldim_spi_async_busy_clear();
	mutex_unlock(&dev_mutex);

	LDIMPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t blmcu_show(const struct class *class, const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_mcu)
		return sprintf(buf, "bl_mcu is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (!strcmp(attr->attr.name, "blmcu_status")) {
		len = sprintf(buf, "blmcu status:\n"
				"dev_index  = %d\n"
				"on_flag    = %d\n"
				"vsync_cnt  = %d\n"
				"spi        = 0x%p : 0x%p\n"
				"tbuf_size  = %d : %d\n"
				"rbuf_size  = %d : %d\n"
				"header_cnt = %d\n"
				"extend_cnt = %d\n"
				"adim_width = %d\n"
				"pdim_width = %d\n"
				"adim       = %d\n"
				"pdim       = %d\n"
				"datawidth  = %d\n"
				"ext_len    = %d\n"
				"apl	    = 0x%x\n"
				"en_on      = %d\n"
				"en_off     = %d\n"
				"dim_max    = 0x%03x\n"
				"dim_min    = 0x%03x\n",
				dev_drv->index,
				bl_mcu->dev_on_flag,
				bl_mcu->vsync_cnt,
				bl_mcu->spi, bl_mcu->spi1,
				bl_mcu->tbuf_size, bl_mcu->tbuf_size1,
				bl_mcu->rbuf_size, bl_mcu->rbuf_size1,
				bl_mcu->header_cnt,
				bl_mcu->extend_cnt,
				bl_mcu->adim_width,
				bl_mcu->pdim_width,
				bl_mcu->adim,
				bl_mcu->pdim,
				bl_mcu->datawidth,
				bl_mcu->ext_len,
				bl_mcu->apl,
				dev_drv->en_gpio_on,
				dev_drv->en_gpio_off,
				dev_drv->dim_max,
				dev_drv->dim_min);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t blmcu_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int val, val1, i;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(128, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto blmcu_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "init")) {
		mutex_lock(&dev_mutex);
		blmcu_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "adim")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto blmcu_store_err;
			bl_mcu->adim = (unsigned char)val;
		}
		LDIMPR("adim: 0x%x\n", bl_mcu->adim);
	} else if (!strcmp(parm[0], "pdim")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto blmcu_store_err;
			bl_mcu->pdim = (unsigned char)val;
		}
		LDIMPR("pdim: 0x%x\n", bl_mcu->pdim);
	} else if (!strcmp(parm[0], "header_data")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto blmcu_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto blmcu_store_err;
			bl_mcu->header_data[val] = val1;
		}
		for (i = 0; i < bl_mcu->header_cnt; i++)
			LDIMPR("header_data[%d]: 0x%x\n", i, bl_mcu->header_data[i]);
	} else if (!strcmp(parm[0], "extend_data")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto blmcu_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto blmcu_store_err;
			bl_mcu->extend_data[val] = val1;
		}
		for (i = 0; i < bl_mcu->extend_cnt; i++)
			LDIMPR("extend_data[%d]: 0x%x\n", i, bl_mcu->extend_data[i]);
	} else {
		LDIMERR("argument error!\n");
	}

blmcu_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

blmcu_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute blmcu_class_attrs[] = {
	__ATTR(init, 0644, NULL, blmcu_store),
	__ATTR(blmcu_status, 0644, blmcu_show, NULL),
};

static int blmcu_ldim_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = blmcu_power_on;
	dev_drv->power_off = blmcu_power_off;
	dev_drv->dev_transmit = blmcu_transmit;
	dev_drv->dev_transmit_dummy = blmcu_transmit_dummy;
	dev_drv->dev_fb_handle = blmcu_fb_handle;
	dev_drv->config_update = blmcu_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_blmcu_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	struct data_pkt_s data_pkt;
	int i, n;

	if (!dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return -1;
	}
	if (!dev_drv->spi_conf.spi_dev[0]) {
		LDIMERR("%s: spi_dev[0] is null\n", __func__);
		return -1;
	}

	if (!dev_drv->spi_conf.spi_dev[1] && dev_drv->spi_conf.spi_dev_num == 2) {
		LDIMERR("%s: spi_dev[1] is null\n", __func__);
		return -1;
	}

	bl_mcu = kzalloc(sizeof(*bl_mcu), GFP_KERNEL);
	if (!bl_mcu) {
		LDIMERR("%s: bl_mcu is error\n", __func__);
		return -1;
	}

	data_pkt = dev_drv->spi_conf.data_pkt;
	bl_mcu->spi = dev_drv->spi_conf.spi_dev[0];
	bl_mcu->spi1 = dev_drv->spi_conf.spi_dev[1];
	bl_mcu->dev_on_flag = 0;
	bl_mcu->vsync_cnt = 0;
	bl_mcu->zone_num = dev_drv->zone_num;
	bl_mcu->header_cnt = data_pkt.header_cnt;
	bl_mcu->header_data = data_pkt.header_data;
	bl_mcu->extend_cnt = data_pkt.extend_cnt;
	bl_mcu->extend_data = data_pkt.extend_data;
	bl_mcu->adim_width = data_pkt.adim_width;
	bl_mcu->pdim_width = data_pkt.pdim_width;
	bl_mcu->adim = data_pkt.adim;
	bl_mcu->pdim = data_pkt.pdim;
	bl_mcu->ext_len = bl_mcu->header_cnt + bl_mcu->extend_cnt;
	bl_mcu->datawidth = bl_mcu->adim_width + bl_mcu->pdim_width;

	if (bl_mcu->datawidth == 16)
		bl_mcu->duty_size = 2 * bl_mcu->zone_num;
	else if (bl_mcu->datawidth == 12)
		bl_mcu->duty_size = (bl_mcu->zone_num * 3 + 1) / 2;
	else	//8bit
		bl_mcu->tbuf_size = bl_mcu->zone_num;

	/*packet length*/
	bl_mcu->tbuf_size = bl_mcu->duty_size + bl_mcu->ext_len;

	LDIMPR("blmcu_probe: bl_mcu zone_num=%d,header_cnt=%d, extend_cnt=%d,\n"
		"adim_width=%d, pdim_width=%d, duty_size=%d, tbuf_size=%d\n",
		bl_mcu->zone_num, bl_mcu->header_cnt, bl_mcu->extend_cnt,
		bl_mcu->adim_width, bl_mcu->pdim_width, bl_mcu->duty_size, bl_mcu->tbuf_size);

	if (dev_drv->spi_conf.spi_dev[0]->controller->dma_alignment) {
		n = bl_mcu->tbuf_size;
		bl_mcu->tbuf_size = ldim_spi_dma_cycle_align_byte(n);
		LDIMPR("%s: bl_mcu->tbuf_size is %d --> %d\n", __func__, n, bl_mcu->tbuf_size);
	}

	bl_mcu->rbuf_size = bl_mcu->tbuf_size;
	bl_mcu->rbuf = kcalloc(bl_mcu->rbuf_size, sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_mcu->rbuf) {
		LDIMERR("%s: bl_mcu->rbuf is error\n", __func__);
		goto ldim_dev_blmcu_probe_err0;
	}

	bl_mcu->tbuf = kcalloc(bl_mcu->tbuf_size, sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
	if (!bl_mcu->tbuf) {
		LDIMERR("%s: bl_mcu->tbuf is error\n", __func__);
		goto ldim_dev_blmcu_probe_err1;
	}

	/*set spi_xlen equal as tbuf_size*/
	cdata = bl_mcu->spi->controller_data;
	if (!cdata) {
		LDIMERR("%s:  controller_data is null\n", __func__);
		goto ldim_dev_blmcu_probe_err2;
	}
	priv = cdata->priv;
	if (priv) {
		priv->xlen = bl_mcu->tbuf_size;
	} else {
		LDIMERR("%s:  priv is null\n", __func__);
		goto ldim_dev_blmcu_probe_err2;
	}

	/* for 2nd spi */
	/* here For reference only, need to be set according to the actual device spec!!!*/
	if (dev_drv->spi_conf.spi_dev_num == 2) {
		bl_mcu->tbuf_size1 = bl_mcu->tbuf_size;
		bl_mcu->rbuf_size1 = bl_mcu->tbuf_size1;
		bl_mcu->rbuf1 = kcalloc(bl_mcu->rbuf_size1,
			sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
		if (!bl_mcu->rbuf1) {
			LDIMERR("%s: bl_mcu->rbuf1 is error\n", __func__);
			goto ldim_dev_blmcu_probe_err2;
		}

		bl_mcu->tbuf1 = kcalloc(bl_mcu->tbuf_size1,
			sizeof(unsigned char), GFP_KERNEL | GFP_DMA);
		if (!bl_mcu->tbuf1) {
			LDIMERR("%s: bl_mcu->tbuf1 is error\n", __func__);
			goto ldim_dev_blmcu_probe_err3;
		}

		cdata = bl_mcu->spi1->controller_data;
		if (!cdata) {
			LDIMERR("%s:  controller_data1 is null\n", __func__);
			goto ldim_dev_blmcu_probe_err4;
		}

		priv = cdata->priv;
		if (priv) {
			priv->xlen = bl_mcu->tbuf_size1;
		} else {
			LDIMERR("%s:  priv1 is null\n", __func__);
			goto ldim_dev_blmcu_probe_err4;
		}
	} else {
		bl_mcu->tbuf_size1 = 0;
		bl_mcu->rbuf_size1 = 0;
		bl_mcu->spi1 = NULL;
	}

	/*set idim*/
	bl_mcu->idim = dev_drv->boost_conf.iset;

	ldim_drv->trig_param.spi_tbuf_size = max(bl_mcu->tbuf_size, bl_mcu->tbuf_size1);
	// update ldc line_n isr for ldim driver
	ldim_trig_line_update(ldim_drv);
	blmcu_ldim_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(blmcu_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &blmcu_class_attrs[i])) {
				LDIMERR("create ldim_dev class attribute %s fail\n",
					blmcu_class_attrs[i].attr.name);
			}
		}
	}

	if (bdrv->state & BL_STATE_BL_ON)
		bl_mcu->dev_on_flag = 1; /* default enable in uboot */

	ldim_pr("%s ok\n", __func__);
	return 0;

ldim_dev_blmcu_probe_err4:
	kfree(bl_mcu->tbuf1);
	bl_mcu->tbuf_size1 = 0;
ldim_dev_blmcu_probe_err3:
	kfree(bl_mcu->rbuf1);
	bl_mcu->rbuf_size1 = 0;
ldim_dev_blmcu_probe_err2:
	kfree(bl_mcu->tbuf);
	bl_mcu->tbuf_size = 0;
ldim_dev_blmcu_probe_err1:
	kfree(bl_mcu->rbuf);
	bl_mcu->rbuf_size = 0;
ldim_dev_blmcu_probe_err0:
	kfree(bl_mcu);
	bl_mcu = NULL;
	return -1;
}

int ldim_dev_blmcu_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_mcu)
		return 0;

	kfree(bl_mcu->rbuf);
	bl_mcu->rbuf_size = 0;
	kfree(bl_mcu->tbuf);
	bl_mcu->tbuf_size = 0;
	if (ldim_drv->dev_drv && ldim_drv->dev_drv->spi_conf.spi_dev_num == 2) {
		kfree(bl_mcu->rbuf1);
		bl_mcu->rbuf_size1 = 0;
		kfree(bl_mcu->tbuf1);
		bl_mcu->tbuf_size1 = 0;
	}
	kfree(bl_mcu);
	bl_mcu = NULL;

	return 0;
}
