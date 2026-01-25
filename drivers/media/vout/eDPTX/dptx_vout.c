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
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/clk.h>
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
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX_export.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX_notify.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/gki_module.h>
#include "dptx_common.h"
#include "dptx_reg_op.h"

#define DPTX_CDEV_NAME  "eDPTX"

unsigned char dptx_print_level = LOG_I;

static unsigned char dptx_global_init_flag;
static struct dptx_drv_s *dptx_driver[DPTX_MAX_DRV];

char eDP_propname[DPTX_MAX_DRV][32];
struct dptx_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct dptx_cdev_s *dptx_cdev;

struct dptx_boot_ctrl_s dptx_uboot_configs[DPTX_MAX_DRV] = {
	{
		.link = {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
		.user_debug_port_count = 0,
	}, {
		.link = {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
		.user_debug_port_count = 0,
	},
};

void dptx_print_helper(struct dptx_drv_s *dptx, u8 port, u8 pr_type, const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;
	char buffer[24];

	if (pr_type > dptx_print_level)
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	if (dptx) {
		if (port == 0xff)
			snprintf(buffer, sizeof(buffer), "%u]", dptx->idx);
		else
			snprintf(buffer, sizeof(buffer), "%u]-%u", dptx->idx, port);
	} else {
		if (port == 0xff)
			snprintf(buffer, sizeof(buffer), "-]");
		else
			snprintf(buffer, sizeof(buffer), "-]-%u", port);
	}

	if (pr_type == LOG_E)
		pr_err("[eDP:%s(err): %pV\n", buffer, &vaf);
	else
		pr_info("[eDP:%s: %pV\n", buffer, &vaf);

	va_end(args);
}

static struct dptx_drv_s *dptx_driver_add(u8 index)
{
	struct dptx_drv_s *dptx = NULL;

	if (index >= DPTX_MAX_DRV)
		return NULL;

	dptx = kzalloc(sizeof(*dptx), GFP_KERNEL);
	if (!dptx)
		return NULL;
	memset(dptx, 0, sizeof(struct dptx_drv_s));

	dptx->idx = index;

	return dptx;
}

struct dptx_drv_s *aml_get_dptx_driver(u8 index)
{
	if (index >= DPTX_MAX_DRV)
		return NULL;

	return dptx_driver[index];
}
EXPORT_SYMBOL(aml_get_dptx_driver);

static inline void dptx_vsync_handler(struct dptx_drv_s *dptx)
{
	//unsigned long long local_time[2];
	unsigned long flags = 0;
	//unsigned int temp;

	if (!dptx)
		return;

	spin_lock_irqsave(&dptx->isr_lock, flags);

	spin_unlock_irqrestore(&dptx->isr_lock, flags);
}

static irqreturn_t dptx_vsync1_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 1) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_vsync2_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 2) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_vsync3_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!dptx)
		return IRQ_HANDLED;

	if (!(dptx->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (dptx->viu_sel == 3) {
		dptx_vsync_handler(dptx);
		if (dptx->vsync_cnt++ >= 50000)
			dptx->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t dptx_HPD_isr(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;
	u8 curr_HPD_level, curr_irq;

	if (!dptx)
		return IRQ_HANDLED;

	DPTX_DBG(dptx, "HPD[%d] triggered. sta=0x%02x", irq, dptx->status);

	if (!(dptx->status & DPTX_STA_HPD_TRI_EN))
		return IRQ_HANDLED;

	curr_HPD_level = dptx_if_get_hpd_level(dptx, 0);
	curr_irq = dptx_if_get_hpd_irq(dptx, 0);

	DPTX_DBG(dptx, "HPD[%d] triggered. ignore=%d level=%u irq=0x%x", irq,
		dptx->status & DPTX_STA_HPD_TRI_EN ? 0 : 1, curr_HPD_level, curr_irq);

	if ((dptx->status & DPTX_STA_HPD_TRI_EN) && curr_irq == 0x1 &&
		((dptx->status & DPTX_STA_HPD_HIGH) ^ (curr_HPD_level << 3))) {
		return IRQ_WAKE_THREAD;
	}

	return IRQ_HANDLED;
}

static irqreturn_t dptx_HPD_post_handler(int irq, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	DPTX_PR(dptx, "%s-0x%x", __func__, dptx->status);

	dptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, dptx);

	if (dptx->status & DPTX_STA_HPD_HIGH)
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, dptx);
	else
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_OUT, dptx);

	return IRQ_HANDLED;
}

/* **************************************** */
static int dptx_io_open(struct inode *inode, struct file *file)
{
	struct dptx_drv_s *dptx;

	dptx = container_of(inode->i_cdev, struct dptx_drv_s, cdev);
	file->private_data = dptx;

	DPTX_PR(dptx, "%s %p", __func__, file);

	return 0;
}

static int dptx_io_release(struct inode *inode, struct file *file)
{
	struct dptx_drv_s *dptx;

	if (!file->private_data)
		return 0;

	dptx = (struct dptx_drv_s *)file->private_data;
	DPTX_PR(dptx, "%s %p", __func__, file);
	file->private_data = NULL;
	return 0;
}

static long dptx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	// void __user *argp = (void __user *)arg;
	int mcd_nr = -1;
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)file->private_data;

	if (!dptx)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	DPTX_PR(dptx, "%s: dir=0x%x, nr=0x%x\n", __func__, _IOC_DIR(cmd), mcd_nr);

	return 0;
}

static const struct file_operations dptx_fops = {
	.owner          = THIS_MODULE,
	.open           = dptx_io_open,
	.release        = dptx_io_release,
	.unlocked_ioctl = dptx_ioctl,
//#ifdef CONFIG_COMPAT
//	.compat_ioctl   = dptx_compat_ioctl,
//#endif
};

static int dptx_cdev_add(struct dptx_drv_s *dptx, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!dptx_cdev) {
		ret = 1;
		goto lcd_cdev_add_failed1;
	}

	devno = MKDEV(MAJOR(dptx_cdev->devno), dptx->idx);

	cdev_init(&dptx->cdev, &dptx_fops);
	dptx->cdev.owner = THIS_MODULE;
	ret = cdev_add(&dptx->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto lcd_cdev_add_failed0;
	}

	dptx->dev = device_create(dptx_cdev->class, parent, devno, NULL, "dptx%d", dptx->idx);
	if (IS_ERR_OR_NULL(dptx->dev)) {
		ret = 3;
		goto lcd_cdev_add_failed0;
	}

	dev_set_drvdata(dptx->dev, dptx);
	dptx->dev->of_node = parent->of_node;

	DPTX_PR(dptx, "%s OK", __func__);
	return 0;

lcd_cdev_add_failed0:
	cdev_del(&dptx->cdev);
lcd_cdev_add_failed1:
	DPTX_ERR(dptx, "%s: failed: %d", __func__, ret);
	return -1;
}

static void dptx_cdev_remove(struct dptx_drv_s *dptx)
{
	dev_t devno;

	if (!dptx_cdev || !dptx)
		return;

	devno = MKDEV(MAJOR(dptx_cdev->devno), dptx->idx);
	device_destroy(dptx_cdev->class, devno);
	cdev_del(&dptx->cdev);
}

static int dptx_global_init_once(struct platform_device *pdev)
{
	int ret;

	if (dptx_global_init_flag) {
		dptx_global_init_flag++;
		return 0;
	}
	dptx_global_init_flag++;

	dptx_notifier_init();

	dptx_cdev = kzalloc(sizeof(*dptx_cdev), GFP_KERNEL);
	if (!dptx_cdev)
		return -1;

	ret = alloc_chrdev_region(&dptx_cdev->devno, 0, DPTX_MAX_DRV, DPTX_CDEV_NAME);
	if (ret) {
		ret = 1;
		goto lcd_cdev_init_once_err;
	}

	dptx_cdev->class = class_create("eDPTX");
	if (IS_ERR_OR_NULL(dptx_cdev->class)) {
		ret = 2;
		goto lcd_cdev_init_once_err_1;
	}

	return 0;

lcd_cdev_init_once_err_1:
	unregister_chrdev_region(dptx_cdev->devno, DPTX_MAX_DRV);
lcd_cdev_init_once_err:
	kfree(dptx_cdev);
	dptx_cdev = NULL;
	DPTX_ERR(NULL, "%s: failed: %d", __func__, ret);
	return -1;
}

static void dptx_global_remove_once(void)
{
	if (dptx_global_init_flag > 1) {
		dptx_global_init_flag--;
		return;
	}
	dptx_global_init_flag--;

	dptx_notifier_remove();

	if (!dptx_cdev)
		return;

	class_destroy(dptx_cdev->class);
	unregister_chrdev_region(dptx_cdev->devno, DPTX_MAX_DRV);
	kfree(dptx_cdev);
	dptx_cdev = NULL;
}

/* ************************************************************* */
static int dptx_irq_init(struct dptx_drv_s *dptx)
{
	//init_completion(&pdrv->vsync_done);
	if (dptx->vsync_irq[0]) {
		snprintf(dptx->vsync_name[0], 15, "dptx%d_vsync", dptx->idx);
		if (request_irq(dptx->vsync_irq[0], dptx_vsync1_isr, IRQF_SHARED,
				dptx->vsync_name[0], (void *)dptx)) {
			DPTX_ERR(dptx, "can't request %s", dptx->vsync_name[0]);
		} else {
			DPTX_PR(dptx, "%s requested", dptx->vsync_name[0]);
		}
	}

	if (dptx->vsync_irq[1]) {
		snprintf(dptx->vsync_name[1], 15, "dptx%d_vsync2", dptx->idx);
		if (request_irq(dptx->vsync_irq[1], dptx_vsync2_isr, IRQF_SHARED,
				dptx->vsync_name[1], (void *)dptx)) {
			DPTX_ERR(dptx, "can't request %s", dptx->vsync_name[1]);
		} else {
			DPTX_PR(dptx, "%s requested", dptx->vsync_name[1]);
		}
	}

	if (dptx->vsync_irq[2]) {
		snprintf(dptx->vsync_name[2], 15, "dptx%d_vsync3", dptx->idx);
		if (request_irq(dptx->vsync_irq[2], dptx_vsync3_isr, IRQF_SHARED,
				dptx->vsync_name[2], (void *)dptx)) {
			DPTX_ERR(dptx, "can't request %s", dptx->vsync_name[2]);
		} else {
			DPTX_PR(dptx, "%s requested", dptx->vsync_name[2]);
		}
	}

	if (dptx->HPD_irq) {
		snprintf(dptx->HPD_name, 12, "DPTX%d-HPD", dptx->idx);
		if (request_threaded_irq(dptx->HPD_irq,
				dptx_HPD_isr, dptx_HPD_post_handler, IRQF_SHARED,
				dptx->HPD_name, (void *)dptx)) {
			DPTX_ERR(dptx, "can't request %s", dptx->HPD_name);
		} else {
			DPTX_PR(dptx, "%s requested", dptx->HPD_name);
		}
	}

	///* add timer to monitor hpll frequency */
	//timer_setup(&pdrv->vs_none_timer, &lcd_vsync_none_timer_handler, 0);
	///* pdrv->vs_none_timer.data = NULL; */
	//pdrv->vs_none_timer.expires = jiffies + LCD_VSYNC_NONE_INTERVAL;
	///*add_timer(&pdrv->vs_none_timer);*/
	///*LCDPR("add vs_none_timer handler\n"); */

	return 0;
}

static void dptx_irq_serve_remove(struct dptx_drv_s *dptx)
{
	if (dptx->vsync_irq[0])
		free_irq(dptx->vsync_irq[0], (void *)dptx);
	if (dptx->vsync_irq[1])
		free_irq(dptx->vsync_irq[1], (void *)dptx);
	if (dptx->vsync_irq[2])
		free_irq(dptx->vsync_irq[2], (void *)dptx);
	if (dptx->HPD_irq)
		free_irq(dptx->HPD_irq, (void *)dptx);

	//if (pdrv->vsync_none_timer_flag) {
	//	del_timer_sync(&pdrv->vs_none_timer);
	//	pdrv->vsync_none_timer_flag = 0;
	//}
}

static int dptx_config_remove(struct dptx_drv_s *dptx)
{
	dptx_vout_server_deinit(dptx);

	dptx_debug_remove(dptx);

	return 0;
}

static void dptx_bootup_config_init(struct dptx_drv_s *dptx)
{
	u32 link_rate = 0;
	u8 lane, port;
	u32 temp;

	dptx_if_link_get(dptx, 0, &link_rate, &lane);
	dptx_if_reg_store_get(dptx, 0, &temp, NULL);

	dptx->status |= dptx->viu_sel  ? DPTX_STA_DRV_READY : 0;
	dptx->status |= temp & BIT(12) ? DPTX_STA_DISP_ON   : 0;
	dptx->status |= link_rate      ? DPTX_STA_LINK_ON   : 0;

	if (dptx_uboot_configs[dptx->idx].user_debug_port_count == 3)
		dptx->sink.port_mask = 0xf;
	else if (dptx_uboot_configs[dptx->idx].user_debug_port_count == 2)
		dptx->sink.port_mask = 0x3;
	else if (dptx_uboot_configs[dptx->idx].user_debug_port_count == 1)
		dptx->sink.port_mask = 0x1;

	if (dptx_uboot_configs[dptx->idx].user_hpd_debug)
		dptx->setting.user_hpd_ignore = 255;

	for (port = 0; port < 4; port++) {
		if (dptx->sink.port_mask & BIT(port)) {
			if (!dptx->sink.link[port])
				dptx->sink.link[port] =
					kzalloc(sizeof(struct dptx_link_cfg_s), GFP_KERNEL);
			if (!dptx->sink.link[port]) {
				DPTX_ERR(dptx, "link[%d] malloc failed", port);
				continue;
			}
			memset(dptx->sink.link[port], 0, sizeof(struct dptx_link_cfg_s));
		} else {
			if (!dptx->sink.link[port])
				continue;
			kfree(dptx->sink.link[port]);
			dptx->sink.link[port] = NULL;
			DPTX_PR(dptx, "clean unused link[%d]", port);
			continue;
		}

		dptx_if_link_get(dptx, port, &link_rate, &lane);

		dptx->sink.link[port]->link_rate = link_rate;
		dptx->sink.link[port]->lane_count = lane;

		link_rate *= 27;
		DPTX_P_DBG(dptx, port, "uboot: %u lane, %u.%u GHz",
			dptx->sink.link[port]->lane_count, link_rate / 100, link_rate % 100);
	}

	DPTX_PR(dptx, "uboot: port:0x%x, sta=0x%x", dptx->sink.port_mask, dptx->status);
}

void dptx_pinmux_set(struct dptx_drv_s *dptx, u8 status)
{
	struct pinctrl *pin_sel;
	//unsigned int index;

	DPTX_PR(dptx, "%s: %u", __func__, status);

	pin_sel = devm_pinctrl_get_select(dptx->dev, status ? "DPTX-ON" : "DPTX-OFF");
	if (IS_ERR(pin_sel))
		DPTX_ERR(dptx, "%s %u error", __func__, status);
	else
		DPTX_PR(dptx, "%s %u ok (0x%px)", __func__, status, pin_sel);
}

static u8 dptx_panel_config_load_timing(struct dptx_drv_s *dptx, const struct device_node *child)
{
	char snode[24];
	unsigned char i, j;
	unsigned int tmp;
	int lenp, ret;
	u32 para[12];

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u", i);

		ret = of_property_read_u32_array(child, snode, &para[0], 12);
		if (ret)
			return i;
		dptx->sink.timing[i].h_period = (u16)para[0];
		dptx->sink.timing[i].h_act    = (u16)para[1];
		dptx->sink.timing[i].h_pw     = (u16)para[2];
		dptx->sink.timing[i].h_bp     = (u16)para[3];
		dptx->sink.timing[i].ctrl    |= (u16)para[4] ? CTRL_HSYNC_POS : 0;
		dptx->sink.timing[i].v_period = (u16)para[5];
		dptx->sink.timing[i].v_act    = (u16)para[6];
		dptx->sink.timing[i].v_pw     = (u16)para[7];
		dptx->sink.timing[i].v_bp     = (u16)para[8];
		dptx->sink.timing[i].ctrl    |= (u16)para[9] ? CTRL_VSYNC_POS : 0;
		dptx->sink.timing[i].h_blank  =
			dptx->sink.timing[i].h_period - dptx->sink.timing[i].h_act;
		dptx->sink.timing[i].v_blank  =
			dptx->sink.timing[i].v_period - dptx->sink.timing[i].v_act;
		dptx->sink.timing[i].h_fp = dptx->sink.timing[i].h_blank -
			(dptx->sink.timing[i].h_pw + dptx->sink.timing[i].h_bp);
		dptx->sink.timing[i].v_fp = dptx->sink.timing[i].v_blank -
			(dptx->sink.timing[i].v_pw + dptx->sink.timing[i].v_bp);
		tmp = (u16)para[10];
		if (tmp == 30)
			dptx->sink.timing[i].cfmt = DPTX_CFMT_RGB_10bit;
		else if (tmp == 18)
			dptx->sink.timing[i].cfmt = DPTX_CFMT_RGB_6bit;
		else if (tmp == 12)
			dptx->sink.timing[i].cfmt = DPTX_CFMT_Y_only_12bit;
		else if (tmp == 10)
			dptx->sink.timing[i].cfmt = DPTX_CFMT_Y_only_10bit;
		else if (tmp == 8)
			dptx->sink.timing[i].cfmt = DPTX_CFMT_Y_only_8bit;
		else
			dptx->sink.timing[i].cfmt = DPTX_CFMT_RGB_8bit;

		tmp = (u16)para[11];
		dptx->sink.timing[i].fr1000 =  tmp * 1000L;

		tmp = dptx->sink.timing[i].v_period * dptx->sink.timing[i].h_period * tmp;
		dptx->sink.timing[i].pclk = tmp;

		DPTX_PR(dptx, "Panel Config add Timing[%u]: %ux%u@%uhz", i,
			dptx->sink.timing[i].h_act, dptx->sink.timing[i].v_act,
			dptx->sink.timing[i].fr1000 / 1000);
		dptx->sink.dt_cnt++;

		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-fr", i);

		// ret = of_property_read_u32(child, snode, &val);
		lenp = of_property_count_elems_of_size(child, snode, sizeof(unsigned int));
		if (lenp > 0) {
			lenp = lenp > 12 ? 12 : lenp;
			ret = of_property_read_u32_array(child, snode, &para[0], lenp);
			if (ret)
				return i;
			// DPTX_PR(dptx, "lenp=%d", lenp);
			for (j = 0; j < (lenp / sizeof(u32)); j++) {
				if (dptx->sink.timing[i].vmode_add_fr[j] == 0) {
					dptx->sink.timing[i].vmode_add_fr[j] = (u16)para[j];
					DPTX_DBG(dptx, "Panel Config Timing[%u] add fr[%u]=%u",
						i, j, dptx->sink.timing[i].vmode_add_fr[j]);
					break;
				}
			}
		}
		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-range", i);

		ret = of_property_read_u32_array(child, snode, &para[0], 2);
		if (!ret) {
			dptx->sink.timing[i].frame_rate_range[0] = (u16)para[0];
			dptx->sink.timing[i].frame_rate_range[1] = (u16)para[1];
			DPTX_DBG(dptx, "Panel Config Timing[%u] range=[%u~%u]", i,
				dptx->sink.timing[i].frame_rate_range[0],
				dptx->sink.timing[i].frame_rate_range[1]);
		}

		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-range-limit", i);
		ret = of_property_read_u32_array(child, snode, &para[0], 6);
		if (!ret) {
			dptx->sink.timing[i].v_period_range[0] = (u16)para[2];
			dptx->sink.timing[i].v_period_range[1] = (u16)para[3];
			dptx->sink.timing[i].pclk_range[0] = (u16)para[4];
			dptx->sink.timing[i].pclk_range[1] = (u16)para[5];
			DPTX_DBG(dptx, "Panel Config Timing[%u] limit vl=[%u~%u] pclk=[%u~%u]", i,
				dptx->sink.timing[i].v_period_range[0],
				dptx->sink.timing[i].v_period_range[1],
				dptx->sink.timing[i].pclk_range[0],
				dptx->sink.timing[i].pclk_range[1]);
		}
	}
	return i;
}

static u8 dptx_panel_config_load_from_dts(struct dptx_drv_s *dptx)
{
	const struct device_node *np, *child;
	u16 val;
	int ret;
	const char *prop_name = NULL;

	np = of_find_node_by_name(NULL, "eDP_Groups");
	if (!np) {
		DPTX_ERR(dptx, "failed to get eDP_Groups");
		return 1;
	}

	prop_name = eDP_propname[dptx->idx];
	if (!prop_name) {
		DPTX_PR(dptx, "%s: not load panel_type", __func__);
		return 1;
	}

	child = of_get_child_by_name(np, prop_name);
	if (!child) {
		DPTX_ERR(dptx, "[%s] is not in eDP_Groups", prop_name);
		return 1;
	}

	ret = of_property_read_u16(child, "HPD_ignore", &val);
	if (!ret) {
		dptx->setting.user_hpd_ignore = (unsigned char)val;
		if (dptx->setting.user_hpd_ignore)
			DPTX_PR(dptx, "HPD ignore: %ums", dptx->setting.user_hpd_ignore);
	}

	ret = of_property_read_u16(child, "assigned-link-rate", &val);
	if (!ret) {
		dptx->setting.user_link_rate = (unsigned char)val;
		if (dptx->setting.user_link_rate)
			DPTX_PR(dptx, "assigned link rate: %u", dptx->setting.user_link_rate);
	}
	ret = of_property_read_u16(child, "assigned-lane-count", &val);
	if (!ret) {
		dptx->setting.user_lane_count = (unsigned char)val;
		if (dptx->setting.user_lane_count)
			DPTX_PR(dptx, "assigned lane count: %u", dptx->setting.user_lane_count);
	} else {
		dptx->setting.user_lane_count = 0xff;
	}

	ret = of_property_read_u16(child, "use-aux-backlight", &val);
	if (!ret) {
		dptx->setting.use_aux_bl_as_possible = (unsigned char)val;
		if (dptx->setting.use_aux_bl_as_possible)
			DPTX_PR(dptx, "AUX backlight: %u", dptx->setting.use_aux_bl_as_possible);
	} else {
		dptx->setting.use_aux_bl_as_possible = 0;
	}

	ret = of_property_read_u16(child, "assigned-port-count", &val);
	if (!ret && (unsigned char)val) {
		if ((unsigned char)val == 4)
			dptx->sink.port_mask = 0xf;
		else if ((unsigned char)val == 2)
			dptx->sink.port_mask = 0x3;
		else
			dptx->sink.port_mask = 0x1;
		DPTX_PR(dptx, "assigned port mask: %u", dptx->sink.port_mask);
	} else {
		dptx->sink.port_mask = 0x1;
	}

	ret = of_property_read_u16(child, "assigned-vmode", &val);
	if (!ret) {
		dptx->setting.user_vmode_sel = (unsigned char)val;
		if (dptx->setting.user_vmode_sel)
			DPTX_PR(dptx, "assigned vmode idx: %u", dptx->setting.user_vmode_sel);
	} else {
		dptx->setting.user_vmode_sel = 0;
	}

	ret = of_property_read_u16(child, "assigned-disable-PSR", &val);
	if (!ret) {
		dptx->setting.user_disable_PSR = (unsigned char)val;
		if (dptx->setting.user_disable_PSR)
			DPTX_PR(dptx, "assigned disable PSR: %u", dptx->setting.user_disable_PSR);
	} else {
		dptx->setting.user_disable_PSR = 0;
	}

	ret = of_property_read_u16(child, "assigned-preset-timing", &val);
	if (!ret) {
		dptx->setting.user_preset_timing = (unsigned char)val;
		if (dptx->setting.user_preset_timing)
			DPTX_PR(dptx, "assigned preset timing: %u", dptx->setting.user_disable_PSR);
	} else {
		dptx->setting.user_preset_timing = 0;
	}
	dptx_panel_config_load_timing(dptx, child);

	return 0;
}

int dptx_config_load_from_dts(struct dptx_drv_s *dptx)
{
	const struct device_node *np;
	//const char *str = "none";
	unsigned short para[30];
	unsigned char i;
	int ret = 0;

	if (!dptx->dev->of_node) {
		DPTX_ERR(dptx, "dev of_node is null");
		return -1;
	}
	np = dptx->dev->of_node;

	//dptx->PWR_gpio = devm_gpiod_get(dptx->dev, "edptx_vcc", GPIOD_OUT_HIGH);
	//if (IS_ERR(dptx->PWR_gpio)) {
	//	DPTX_ERR(dptx, "regist PWR gpio: %p, err: %d",
	//	       dptx->PWR_gpio, IS_ERR(dptx->PWR_gpio));
	//}

	memset(&para[0], 0, 30 * sizeof(unsigned char));
	ret = of_property_read_u16_array(np, "drive-strength-lut", &para[0], 30);
	if (ret) {
		DPTX_ERR(dptx, "DPCD level to PHY LUT not set");
	} else {
		for (i = 0; i < 30; i++)
			dptx->phy_cfg.level_to_phy_lut[i / 10][i % 3] = para[i];
	}

	i = dptx_panel_config_load_from_dts(dptx);
	if (i) {
		DPTX_PR(dptx, "not loading panel config");
		dptx->setting.user_link_rate = 0;
		dptx->setting.user_lane_count = 0;
		dptx->sink.port_mask = 0x1;
		dptx->setting.user_vmode_sel = 0;
		dptx->setting.user_disable_PSR = 0;
		dptx->setting.user_color_format = 0xff;
	}
	return 0;
}

static int dptx_config_probe(struct dptx_drv_s *dptx, struct platform_device *pdev)
{
	int ret = 0;

	ret = dptx_config_load_from_dts(dptx);
	if (ret)
		return -1;

	dptx->vsync_irq[0] = platform_get_irq_byname(pdev, "vsync");
	dptx->vsync_irq[1] = platform_get_irq_byname(pdev, "vsync2");
	dptx->vsync_irq[2] = platform_get_irq_byname(pdev, "vsync3");
	dptx->HPD_irq      = platform_get_irq_byname(pdev, "DPTX-HPD");

	dptx_clk_config_probe(dptx);
	dptx_phy_probe(dptx);
	dptx_venc_probe(dptx);
	dptx_debug_probe(dptx);
	dptx_if_IP_probe(dptx);

	dptx_vout_server_init(dptx); // get viu sel here

	dptx_bootup_config_init(dptx);

	/* lock pinmux as when dptx is used ?? or link on */
	if (dptx->viu_sel || dptx->status & DPTX_STA_LINK_ON) {
		dptx_driver_panel_power_ctrl(dptx, 1);

		dptx_pinmux_set(dptx, 1);

		dptx_drv_check_HPD(dptx);

		dptx_timing_config_restore(dptx);
	}

	//lcd_vrr_dev_register(dptx);

	dptx_irq_init(dptx);

	/* add notifier for video sync_duration info refresh */
	dptx_vout_notify_mode_change(dptx);

	eDP_bl_wrapper_regist(dptx);

	dptx_drm_add(dptx->dev);

	dptx_drv_check_HPD(dptx);
	// if (dptx->viu_sel & 0xf)
	dptx_HPD_trigger_set(dptx, 1);

	return 0;
}

#ifdef CONFIG_OF
static struct dptx_chip_data_s dptx_data_t7 = {
	.chip_type = DPTX_CHIP_T7,
	.chip_name = "T7",
	//.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 2,
	.offset_venc      = {0x0, 0x600},
	.offset_venc_if   = {0x0, 0x500},
	.offset_venc_data = {0x0, 0x100},
	.venc_clk_msr_id  = {222, 221},

	.link_rate    = {{DP_LINK_RATE_HBR, DP_LINK_RATE_HBR}, {DP_LINK_RATE_HBR, 0}},
	.lane_count   = {{4, 4}, {4, 0}},
	.TPS_support  = BIT(0),
	.DACP_support = BIT(2),
	.pixel_clk_limit = 667,
};

static struct dptx_chip_data_s dptx_data_a9 = {
	// .chip_type = DPTX_CHIP_T7,
	.chip_name = "A9",
	//.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 2,
	.offset_venc      = {0x0},
	.offset_venc_if   = {0x0},
	.offset_venc_data = {0x0},
	// .venc_clk_msr_id  = {222, 221},
	.link_rate        = {{DP_LINK_RATE_HBR2, 0}, {DP_LINK_RATE_HBR2, 0}},
	.lane_count       = {{4, 0}, {4, 0}},
	.TPS_support      = BIT(0),
	.DACP_support     = BIT(2),
	.pixel_clk_limit  = 667,
};

static const struct of_device_id dptx_dt_match_table[] = {
	{
		.compatible = "amlogic, eDPTX-T7",
		.data = &dptx_data_t7,
	},
	{
		.compatible = "amlogic, eDPTX-A9",
		.data = &dptx_data_a9,
	},
	{}
};
#endif

static int dptx_probe(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx;
	const struct of_device_id *match;
	struct dptx_chip_data_s *pdata;
	unsigned int index = 0;
	int ret = 0;

	dptx_global_init_once(pdev);

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		DPTX_ERR(NULL, "no index exist");
		return 0;
	}
	if (index >= DPTX_MAX_DRV) {
		DPTX_ERR(NULL, "%s: invalid index %d", __func__, index);
		return -1;
	}

	match = of_match_device(dptx_dt_match_table, &pdev->dev);
	if (!match) {
		DPTX_ERR(NULL, "%s: no match table", __func__);
		return -1;
	}
	pdata = (struct dptx_chip_data_s *)match->data;
	pr_info("[eDP:%u]: %s: driver version: %s(%d-%s)", index, __func__,
		DPTX_DRV_VERSION, pdata->chip_type, pdata->chip_name);
	if (index >= pdata->drv_max) {
		DPTX_ERR(NULL, "%s: invalid index", __func__);
		return -1;
	}

	dptx = dptx_driver_add(index);
	if (!dptx)
		goto lcd_probe_err_0;
	/* set drvdata */
	dptx_driver[index] = dptx;
	dptx->data = pdata;
	platform_set_drvdata(pdev, dptx);
	dptx->pdev = pdev;

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for lcd*/
	dptx->vpu_dev = vpu_dev_register(VPU_VENCL, DPTX_CDEV_NAME);
#endif
	ret = dptx_ioremap(dptx, pdev);
	if (ret)
		goto lcd_probe_err_1;

	spin_lock_init(&dptx->isr_lock);

	ret = dptx_cdev_add(dptx, &pdev->dev);
	if (ret)
		goto lcd_probe_err_2;

	ret = dptx_config_probe(dptx, pdev);
	if (ret)
		goto lcd_probe_err_2;

	dptx->status |= DPTX_STA_PROBE_DONE;

	DPTX_PR(dptx, "%s ok, status:0x0%2x", __func__, dptx->status);

	return 0;

lcd_probe_err_2:
	dptx_cdev_remove(dptx);
lcd_probe_err_1:
	platform_set_drvdata(pdev, NULL);
	dptx_driver[index] = NULL;
	kfree(dptx);
lcd_probe_err_0:
	return ret;
}

static void dptx_remove(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);
	u8 index;

	if (!dptx)
		return;

	dptx->status &= ~DPTX_STA_PROBE_DONE;

	dptx_drm_remove(dptx->dev);

	index = dptx->idx;

	dptx_irq_serve_remove(dptx);
	dptx_cdev_remove(dptx);
	dptx_debug_remove(dptx);
	dptx_config_remove(dptx);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);

	kfree(dptx->reg_map);
	kfree(dptx);
	dptx_driver[index] = NULL;
	dptx_global_remove_once();

	pr_info("[eDP:%u]: %s done", index, __func__);
}

static int dptx_resume(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return 0;

	dptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)dptx);

	DPTX_PR(dptx, "%s finished (0x%x)", __func__, dptx->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int dptx_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return 0;

	if (dptx->status & DPTX_STA_LINK_ON)
		dptx_notifier_call_chain(DPTX_EVENT_DISP_OFF, (void *)dptx);

	dptx_notifier_call_chain(DPTX_EVENT_UNPREPARE, (void *)dptx);

	DPTX_PR(dptx, "%s finished (0x%x)", __func__, dptx->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static void dptx_shutdown(struct platform_device *pdev)
{
	struct dptx_drv_s *dptx = platform_get_drvdata(pdev);

	if (!dptx)
		return;

	DPTX_PR(dptx, "%s", __func__);

	dptx_notifier_call_chain(DPTX_EVENT_DISABLE, (void *)dptx);
}

static struct platform_driver eDP_platform_driver = {
	.probe    = dptx_probe,
	.remove   = dptx_remove,
	.suspend  = dptx_suspend,
	.resume   = dptx_resume,
	.shutdown = dptx_shutdown,
	.driver = {
		.name = "Meson-eDPTX",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dptx_dt_match_table),
#endif
	},
};

int __init eDPTX_init(void)
{
	if (platform_driver_register(&eDP_platform_driver)) {
		DPTX_ERR(NULL, "failed to register DisplayPort driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit eDPTX_TX_exit(void)
{
	platform_driver_unregister(&eDP_platform_driver);
}

static int __dptx_bootargs_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	const char *ptr, *ptr2;
	char temp_str[12];

	if (!str)
		return -EINVAL;

	ptr = strstr(str, ",");
	if (!ptr || ptr == str) {
		DPTX_ERR(NULL, "%s: invalid boot ctrl str: %s", __func__, str);
		return -EINVAL;
	}
	snprintf(temp_str, (ptr - str > 11) ? 11 : ptr - str + 1, "%s", str);
	ret = kstrtouint(temp_str, 16, &data32);
	if (ret) {
		DPTX_ERR(NULL, "%s: invalid data", __func__);
		return -EINVAL;
	}

	dptx_print_level = data32 & 0x3;

	ptr++;
	ptr2 = strstr(ptr, ",");
	if (ptr2) {
		if (ptr2 > ptr)
			snprintf(eDP_propname[0], ptr2 - ptr + 1, "%s", ptr);
		ptr2++;
		if (str + strlen(str) > ptr2)
			snprintf(eDP_propname[1], 32, "%s", ptr2);
	}

	DPTX_PR(NULL, "%s: data:0x%x, %s,%s", __func__, data32,
				eDP_propname[0], eDP_propname[1]);
	return 1;
}

__setup("eDPTX=", __dptx_bootargs_setup);

struct dptx_drv_s *aml_dptx_get_driver(u8 drv_idx)
{
	if (drv_idx >= DPTX_MAX_DRV)
		return NULL;

	return dptx_driver[drv_idx];
}
EXPORT_SYMBOL(aml_dptx_get_driver);

void aml_dptx_regist_hpd_cb(struct dptx_drv_s *dptx, struct connector_hpd_cb *hpd_cb)
{
	dptx->drm_hpd_cb.callback = hpd_cb->callback;
	dptx->drm_hpd_cb.data = hpd_cb->data;
}
EXPORT_SYMBOL(aml_dptx_regist_hpd_cb);

struct edid *aml_dptx_get_raw_edid(struct dptx_drv_s *dptx)
{
	return dptx->sink.edid.drm_edid;
}
EXPORT_SYMBOL(aml_dptx_get_raw_edid);

int aml_eDPTX_get_hpd_state(u8 drv_idx)
{
	struct dptx_drv_s *dptx = aml_dptx_get_driver(drv_idx);

	if (!dptx)
		return 0;
	return dptx->status & DPTX_STA_HPD_HIGH ? 1 : 0;
}
EXPORT_SYMBOL(aml_eDPTX_get_hpd_state);

//MODULE_DESCRIPTION("Meson DisplayPort TX Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
