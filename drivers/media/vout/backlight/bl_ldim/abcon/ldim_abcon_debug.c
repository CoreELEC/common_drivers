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
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#include "../ldim_drv.h"
#include "../ldim_dev_drv.h"
#include "ldim_abcon.h"
#include "ldim_abcon_reg.h"

int ldim_abcon_mem_debug(char **parm, unsigned int wr, unsigned int type,
	unsigned int len, unsigned int start)
{
	unsigned int *buf;
	unsigned int n, val;

	if (type == 0) {
		buf = abcon_mem.ch_mapping;
		ABCONPR("ch_mapping buf\n");
	} else if (type == 1) {
		buf = abcon_mem.ldc_seg;
		ABCONPR("ldc_seg buf\n");
	} else if (type == 2) {
		buf = abcon_mem.swduty;
		ABCONPR("swduty buf\n");
	} else if (type == 3) {
		buf = abcon_mem.wseg;
		ABCONPR("wseg buf\n");
	} else if (type == 4) {
		buf = abcon_mem.rseg;
		ABCONPR("rseg buf\n");
	} else {
		ABCONERR("type =%d is wrong\n", type);
		return -1;
	}

	if (wr) {
		for (n = 0; n < len; n++) {
			if (kstrtouint(parm[5 + n], 0, &val) < 0) {
				ABCONERR("parm[%d] value is wrong\n", 5 + n);
				return -1;
			}
			buf[start + n] = val;
		}
	} else {
		for (n = 0; n < len; n++)
			pr_info("buf[%d] = 0x%08x\n", n + start, buf[start + n]);
	}

	return 0;
}

static ssize_t abcon_show(const struct class *class, const struct class_attribute *attr, char *buf)
{
	int len = 0;

	if (attr && attr->attr.name && !strcmp(attr->attr.name, "abcon_status")) {
		len = sprintf(buf,
			"ch_mapping_paddr=0x%llx, ch_mapping=0x%px\n"
			"ldc_seg_paddr=0x%llx, ldc_seg=0x%px\n"
			"swduty_paddr=0x%llx, swduty=0x%px\n"
			"wseg_paddr=0x%llx, wseg=0x%px\n"
			"rseg_paddr=0x%llx, rseg=0x%px\n"
			"act_lane=%d\n"
			"max_lane_dim=%d\n"
			"autotrans_ready=%d\n",
			(u64)abcon_mem.ch_mapping_paddr, abcon_mem.ch_mapping,
			(u64)abcon_mem.ldc_seg_paddr, abcon_mem.ldc_seg,
			(u64)abcon_mem.swduty_paddr, abcon_mem.swduty,
			(u64)abcon_mem.wseg_paddr, abcon_mem.wseg,
			(u64)abcon_mem.rseg_paddr, abcon_mem.rseg,
			abcon->act_lane,
			abcon->max_lane_dim,
			abcon->autotrans_ready);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t abcon_debug_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int val, val1;
	unsigned int wr, type, len, start;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(2000, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto abcon_debug_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "swrst")) {
		ldim_abcon_swrst();
		ABCONPR("sw reset done!\n");
	} else if (!strcmp(parm[0], "test_dc")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;
			abcon->test_dc = val;
		}
		ABCONPR("test_dc = 0x%x\n", abcon->test_dc);
	} else if (!strcmp(parm[0], "base_paddr")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;
			abcon_mem.base_paddr = val;
		}
		ABCONPR("base_paddr = 0x%llx\n", (u64)abcon_mem.base_paddr);
	} else if (!strcmp(parm[0], "tx_clk")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;
			abcon->conf.tx_clk = (unsigned int)val;
			ldim_abcon_set_txrx_clk(abcon->conf.tx_clk, abcon->conf.rx_clk);
		}
		ABCONPR("tx_clk = %d\n", abcon->conf.tx_clk);
	} else if (!strcmp(parm[0], "rx_clk")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.rx_clk = (unsigned int)val;
			ldim_abcon_set_txrx_clk(abcon->conf.tx_clk, abcon->conf.rx_clk);
		}
		ABCONPR("rx_clk = %d\n", abcon->conf.rx_clk);
	} else if (!strcmp(parm[0], "mem")) {
		if (!parm[4])
			goto abcon_debug_store_err;
		if (kstrtouint(parm[1], 0, &wr) < 0)
			goto abcon_debug_store_err;
		if (kstrtouint(parm[2], 0, &type) < 0)
			goto abcon_debug_store_err;
		if (kstrtouint(parm[3], 0, &len) < 0)
			goto abcon_debug_store_err;
		if (kstrtouint(parm[4], 0, &start) < 0)
			goto abcon_debug_store_err;
		ABCONPR("wr=%d, type=%d, len=%d, start=%d\n", wr, type, len, start);
		if (wr && (n != (len + 5))) {
			LDIMERR("len not match!\n");
			goto abcon_debug_store_err;
		}
		ldim_abcon_mem_debug(parm, wr, type, len, start);
	} else if (!strcmp(parm[0], "gpio_o")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_debug_store_err;

			abcon->conf.gpio_o[0] = val;
			abcon->conf.gpio_o[1] = val1;
			ldim_abcon_set_gpio(abcon);
		}
		ABCONPR("gpio_o = 0x%08x 0x%08x\n", abcon->conf.gpio_o[0], abcon->conf.gpio_o[1]);
	} else if (!strcmp(parm[0], "gpio_i")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_debug_store_err;

			abcon->conf.gpio_i[0] = val;
			abcon->conf.gpio_i[1] = val1;
			ldim_abcon_set_gpio(abcon);
		}
		ABCONPR("gpio_i = 0x%08x 0x%08x\n", abcon->conf.gpio_i[0], abcon->conf.gpio_i[1]);
	} else if (!strcmp(parm[0], "fb_en")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.fb_en = (unsigned char)val;
		}
		ABCONPR("fb_en = %d\n", abcon->conf.fb_en);
	} else if (!strcmp(parm[0], "fb_pwm_step")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.fb_pwm_step = (unsigned char)val;
		}
		ABCONPR("fb_pwm_step = %d\n", abcon->conf.fb_pwm_step);
	} else if (!strcmp(parm[0], "fb_det_int")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.fb_det_int = val;
		}
		ABCONPR("fb_det_int = %d\n", abcon->conf.fb_det_int);
	} else if (!strcmp(parm[0], "fb_adj_th")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.fb_adj_th = (unsigned char)val;
		}
		ABCONPR("fb_adj_th = %d\n", abcon->conf.fb_adj_th);
	} else if (!strcmp(parm[0], "fb_pwm_dir")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_debug_store_err;

			abcon->conf.fb_pwm_dir = (unsigned char)val;
		}
		ABCONPR("fb_pwm_dir = %d\n", abcon->conf.fb_pwm_dir);
	} else {
		LDIMERR("argument error!\n");
	}

abcon_debug_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_debug_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static ssize_t abcon_reg_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int val;
	unsigned int reg, temp, i, size;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(2000, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto abcon_reg_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "rv")) {
		if (!parm[1])
			goto abcon_reg_store_err;
		if (kstrtouint(parm[1], 0, &reg) < 0)
			goto abcon_reg_store_err;
		val = abcon_rd_reg(reg);
		//dbg_attr.data = val;
		pr_info("reg 0x%04x = 0x%08x\n", reg, val);
	} else if (!strcmp(parm[0], "dv")) {
		if (!parm[2])
			goto abcon_reg_store_err;
		if (kstrtouint(parm[1], 0, &reg) < 0)
			goto abcon_reg_store_err;
		if (kstrtouint(parm[2], 0, &size) < 0)
			goto abcon_reg_store_err;
		for (i = 0; i < size; i++) {
			val = abcon_rd_reg(reg + i);
			pr_info("reg 0x%04x = 0x%08x\n", (reg + i), val);
		}
	} else if (!strcmp(parm[0], "wv")) {
		if (!parm[2])
			goto abcon_reg_store_err;
		if (kstrtouint(parm[1], 0, &reg) < 0)
			goto abcon_reg_store_err;
		if (kstrtouint(parm[2], 0, &val) < 0)
			goto abcon_reg_store_err;
		abcon_wr_reg(reg, val);
		temp = abcon_rd_reg(reg);
		pr_info("write reg 0x%04x = 0x%08x, readback 0x%08x\n", reg, val, temp);
	} else {
		LDIMERR("argument error!\n");
	}

abcon_reg_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_reg_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute abcon_class_attrs[] = {
	__ATTR(abcon_status, 0644, abcon_show, NULL),
	__ATTR(abcon_debug, 0644, NULL, abcon_debug_store),
	__ATTR(abcon_reg, 0644, NULL, abcon_reg_store),
};

int ldim_abcon_debug_probe(struct ldim_dev_driver_s *dev_drv)
{
	int i, ret = 0;

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(abcon_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &abcon_class_attrs[i])) {
				ABCONERR("create abcon class attribute %s fail\n",
					abcon_class_attrs[i].attr.name);
			}
		}
	}

	return ret;
}

int ldim_abcon_debug_remove(struct ldim_dev_driver_s *dev_drv)
{
	return 0;
}
