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
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_export.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_notify.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/gki_module.h>
#include "edptx_common.h"
#include "edptx_reg_op.h"

#define eDPTX_CDEV_NAME  "eDPTX"

unsigned char edptx_print_level = LOG_I;

static unsigned char dptx_global_init_flag;
static struct edptx_drv_s *edp_driver[eDPTX_MAX_DRV];

char eDP_propname[eDPTX_MAX_DRV][32];
struct dptx_cdev_s {
	dev_t           devno;
	struct class    *class;
};

static struct dptx_cdev_s *dptx_cdev;

struct edptx_boot_ctrl_s dptx_uboot_configs[eDPTX_MAX_DRV] = {
	{
		.link = {{0, 0}, {0, 0}},
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
		.user_debug_port_count = 0,
	}, {
		.link = {{0, 0}, {0, 0}},
		.vmode_sel_idx = 0,
		.vmode_cfmt_sel = 0,
		.tx_prepared = 0,
		.disp_on = 0,
		.uboot_edid_crc = 0,
		.user_debug_port_count = 0,
	},
};

void edptx_print_helper(struct edptx_drv_s *edp, u8 port, u8 pr_type, const char *fmt, ...)
{
	va_list args;
	struct va_format vaf;
	char buffer[24];

	if (pr_type > edptx_print_level)
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;

	if (edp) {
		if (port == 0xff)
			snprintf(buffer, sizeof(buffer), "%u]", edp->idx);
		else
			snprintf(buffer, sizeof(buffer), "%u]-%u", edp->idx, port);
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

static struct edptx_drv_s *edptx_driver_add(u8 index)
{
	struct edptx_drv_s *edp = NULL;

	if (index >= eDPTX_MAX_DRV)
		return NULL;

	edp = kzalloc(sizeof(*edp), GFP_KERNEL);
	if (!edp)
		return NULL;
	memset(edp, 0, sizeof(struct edptx_drv_s));

	edp->idx = index;

	return edp;
}

struct edptx_drv_s *aml_get_edptx_driver(u8 index)
{
	if (index >= eDPTX_MAX_DRV)
		return NULL;

	return edp_driver[index];
}
EXPORT_SYMBOL(aml_get_edptx_driver);

static inline void edptx_vsync_handler(struct edptx_drv_s *edp)
{
	//unsigned long long local_time[2];
	unsigned long flags = 0;
	//unsigned int temp;

	if (!edp)
		return;

	spin_lock_irqsave(&edp->isr_lock, flags);

	spin_unlock_irqrestore(&edp->isr_lock, flags);
}

static irqreturn_t edptx_vsync1_isr(int irq, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return IRQ_HANDLED;

	if (!(edp->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (edp->viu_sel == 1) {
		edptx_vsync_handler(edp);
		if (edp->vsync_cnt++ >= 50000)
			edp->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t edptx_vsync2_isr(int irq, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return IRQ_HANDLED;

	if (!(edp->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (edp->viu_sel == 2) {
		edptx_vsync_handler(edp);
		if (edp->vsync_cnt++ >= 50000)
			edp->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t edptx_vsync3_isr(int irq, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return IRQ_HANDLED;

	if (!(edp->status & DPTX_STA_DRV_READY))
		return IRQ_HANDLED;

	if (edp->viu_sel == 3) {
		edptx_vsync_handler(edp);
		if (edp->vsync_cnt++ >= 50000)
			edp->vsync_cnt = 0;
	}
	return IRQ_HANDLED;
}

static irqreturn_t edptx_HPD_isr(int irq, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;
	u8 curr_HPD_level, curr_irq;

	if (!edp)
		return IRQ_HANDLED;

	eDP_DBG(edp, "HPD[%d] triggered. sta=0x%02x", irq, edp->status);

	if (!(edp->status & DPTX_STA_HPD_TRI_EN))
		return IRQ_HANDLED;

	curr_HPD_level = edptx_if_get_hpd_level(edp, 0);
	curr_irq = edptx_if_get_hpd_irq(edp, 0);

	eDP_DBG(edp, "HPD[%d] triggered. ignore=%d level=%u irq=0x%x", irq,
		edp->status & DPTX_STA_HPD_TRI_EN ? 0 : 1, curr_HPD_level, curr_irq);

	if ((edp->status & DPTX_STA_HPD_TRI_EN) && curr_irq == 0x1 &&
		((edp->status & DPTX_STA_HPD_HIGH) ^ (curr_HPD_level << 3))) {
		return IRQ_WAKE_THREAD;
	}

	return IRQ_HANDLED;
}

static irqreturn_t edptx_HPD_post_handler(int irq, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	eDP_PR(edp, "%s-0x%x", __func__, edp->status);

	edptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, edp);

	if (edp->status & DPTX_STA_HPD_HIGH)
		edptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, edp);
	else
		edptx_notifier_call_chain(DPTX_EVENT_PLUG_OUT, edp);

	return IRQ_HANDLED;
}

/* **************************************** */
static int edptx_io_open(struct inode *inode, struct file *file)
{
	struct edptx_drv_s *edp;

	edp = container_of(inode->i_cdev, struct edptx_drv_s, cdev);
	file->private_data = edp;

	eDP_PR(edp, "%s %p", __func__, file);

	return 0;
}

static int edptx_io_release(struct inode *inode, struct file *file)
{
	struct edptx_drv_s *edp;

	if (!file->private_data)
		return 0;

	edp = (struct edptx_drv_s *)file->private_data;
	eDP_PR(edp, "%s %p", __func__, file);
	file->private_data = NULL;
	return 0;
}

static long edptx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	// void __user *argp = (void __user *)arg;
	int mcd_nr = -1;
	struct edptx_drv_s *edp = (struct edptx_drv_s *)file->private_data;

	if (!edp)
		return -EFAULT;

	mcd_nr = _IOC_NR(cmd);
	eDP_PR(edp, "%s: dir=0x%x, nr=0x%x\n", __func__, _IOC_DIR(cmd), mcd_nr);

	return 0;
}

static const struct file_operations dptx_fops = {
	.owner          = THIS_MODULE,
	.open           = edptx_io_open,
	.release        = edptx_io_release,
	.unlocked_ioctl = edptx_ioctl,
//#ifdef CONFIG_COMPAT
//	.compat_ioctl   = dptx_compat_ioctl,
//#endif
};

static int edptx_cdev_add(struct edptx_drv_s *edp, struct device *parent)
{
	dev_t devno;
	int ret = 0;

	if (!dptx_cdev) {
		ret = 1;
		goto lcd_cdev_add_failed1;
	}

	devno = MKDEV(MAJOR(dptx_cdev->devno), edp->idx);

	cdev_init(&edp->cdev, &dptx_fops);
	edp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&edp->cdev, devno, 1);
	if (ret) {
		ret = 2;
		goto lcd_cdev_add_failed0;
	}

	edp->dev = device_create(dptx_cdev->class, parent, devno, NULL, "edptx%d", edp->idx);
	if (IS_ERR_OR_NULL(edp->dev)) {
		ret = 3;
		goto lcd_cdev_add_failed0;
	}

	dev_set_drvdata(edp->dev, edp);
	edp->dev->of_node = parent->of_node;

	eDP_PR(edp, "%s OK", __func__);
	return 0;

lcd_cdev_add_failed0:
	cdev_del(&edp->cdev);
lcd_cdev_add_failed1:
	eDP_ERR(edp, "%s: failed: %d", __func__, ret);
	return -1;
}

static void dptx_cdev_remove(struct edptx_drv_s *edp)
{
	dev_t devno;

	if (!dptx_cdev || !edp)
		return;

	devno = MKDEV(MAJOR(dptx_cdev->devno), edp->idx);
	device_destroy(dptx_cdev->class, devno);
	cdev_del(&edp->cdev);
}

static int dptx_global_init_once(struct platform_device *pdev)
{
	int ret;

	if (dptx_global_init_flag) {
		dptx_global_init_flag++;
		return 0;
	}
	dptx_global_init_flag++;

	edptx_notifier_init();

	dptx_cdev = kzalloc(sizeof(*dptx_cdev), GFP_KERNEL);
	if (!dptx_cdev)
		return -1;

	ret = alloc_chrdev_region(&dptx_cdev->devno, 0, eDPTX_MAX_DRV, eDPTX_CDEV_NAME);
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
	unregister_chrdev_region(dptx_cdev->devno, eDPTX_MAX_DRV);
lcd_cdev_init_once_err:
	kfree(dptx_cdev);
	dptx_cdev = NULL;
	eDP_ERR(NULL, "%s: failed: %d", __func__, ret);
	return -1;
}

static void dptx_global_remove_once(void)
{
	if (dptx_global_init_flag > 1) {
		dptx_global_init_flag--;
		return;
	}
	dptx_global_init_flag--;

	edptx_notifier_remove();

	if (!dptx_cdev)
		return;

	class_destroy(dptx_cdev->class);
	unregister_chrdev_region(dptx_cdev->devno, eDPTX_MAX_DRV);
	kfree(dptx_cdev);
	dptx_cdev = NULL;
}

/* ************************************************************* */
static int edptx_irq_init(struct edptx_drv_s *edp)
{
	//init_completion(&pdrv->vsync_done);
	if (edp->vsync_irq[0]) {
		snprintf(edp->vsync_name[0], 15, "eDP%d_vsync", edp->idx);
		if (request_irq(edp->vsync_irq[0], edptx_vsync1_isr, IRQF_SHARED,
				edp->vsync_name[0], (void *)edp)) {
			eDP_ERR(edp, "can't request %s", edp->vsync_name[0]);
		} else {
			eDP_PR(edp, "%s requested", edp->vsync_name[0]);
		}
	}

	if (edp->vsync_irq[1]) {
		snprintf(edp->vsync_name[1], 15, "eDP%d_vsync2", edp->idx);
		if (request_irq(edp->vsync_irq[1], edptx_vsync2_isr, IRQF_SHARED,
				edp->vsync_name[1], (void *)edp)) {
			eDP_ERR(edp, "can't request %s", edp->vsync_name[1]);
		} else {
			eDP_PR(edp, "%s requested", edp->vsync_name[1]);
		}
	}

	if (edp->vsync_irq[2]) {
		snprintf(edp->vsync_name[2], 15, "eDP%d_vsync3", edp->idx);
		if (request_irq(edp->vsync_irq[2], edptx_vsync3_isr, IRQF_SHARED,
				edp->vsync_name[2], (void *)edp)) {
			eDP_ERR(edp, "can't request %s", edp->vsync_name[2]);
		} else {
			eDP_PR(edp, "%s requested", edp->vsync_name[2]);
		}
	}

	if (edp->HPD_irq) {
		snprintf(edp->HPD_name, 12, "eDP%d-0-HPD", edp->idx);
		if (request_threaded_irq(edp->HPD_irq,
				edptx_HPD_isr, edptx_HPD_post_handler, IRQF_SHARED,
				edp->HPD_name, (void *)edp)) {
			eDP_ERR(edp, "can't request %s", edp->HPD_name);
		} else {
			eDP_PR(edp, "%s requested", edp->HPD_name);
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

static void edptx_irq_serve_remove(struct edptx_drv_s *edp)
{
	if (edp->vsync_irq[0])
		free_irq(edp->vsync_irq[0], (void *)edp);
	if (edp->vsync_irq[1])
		free_irq(edp->vsync_irq[1], (void *)edp);
	if (edp->vsync_irq[2])
		free_irq(edp->vsync_irq[2], (void *)edp);
	if (edp->HPD_irq)
		free_irq(edp->HPD_irq, (void *)edp);

	//if (pdrv->vsync_none_timer_flag) {
	//	del_timer_sync(&pdrv->vs_none_timer);
	//	pdrv->vsync_none_timer_flag = 0;
	//}
}

static int edptx_config_remove(struct edptx_drv_s *edp)
{
	edptx_vout_server_deinit(edp);

	dptx_debug_remove(edp);

	return 0;
}

static void edptx_bootup_config_init(struct edptx_drv_s *edp)
{
	u32 link_rate = 0;
	u8 lane, port;
	u32 temp;

	edptx_if_link_get(edp, 0, &link_rate, &lane);
	edptx_if_reg_store_get(edp, 0, &temp, NULL);

	edp->status |= edp->viu_sel  ? DPTX_STA_DRV_READY : 0;
	edp->status |= temp & BIT(12) ? DPTX_STA_DISP_ON   : 0;
	edp->status |= link_rate      ? DPTX_STA_LINK_ON   : 0;

	if (dptx_uboot_configs[edp->idx].user_debug_port_count == 3)
		edp->sink.port_mask = 0xf;
	else if (dptx_uboot_configs[edp->idx].user_debug_port_count == 2)
		edp->sink.port_mask = 0x3;
	else if (dptx_uboot_configs[edp->idx].user_debug_port_count == 1)
		edp->sink.port_mask = 0x1;

	if (dptx_uboot_configs[edp->idx].user_hpd_debug)
		edp->setting.user_hpd_ignore = 255;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;

		memset(&edp->sink.link[port], 0, sizeof(struct edptx_link_cfg_s));
		edptx_if_link_get(edp, port, &link_rate, &lane);

		edp->sink.link[port].link_rate = link_rate;
		edp->sink.link[port].lane_count = lane;

		link_rate *= 27;
		eDP_P_PR(edp, port, "uboot: %u lane, %u.%u GHz",
			edp->sink.link[port].lane_count, link_rate / 100, link_rate % 100);
	}

	eDP_PR(edp, "uboot: port:0x%x, sta=0x%x", edp->sink.port_mask, edp->status);
}

void edptx_pinmux_set(struct edptx_drv_s *edp, u8 status)
{
	struct pinctrl *pin_sel;
	//unsigned int index;

	eDP_PR(edp, "%s: %u", __func__, status);

	pin_sel = devm_pinctrl_get_select(edp->dev, status ? "edptx_0_hpd_on" : "edptx_0_hpd_off");
	if (IS_ERR(pin_sel))
		eDP_ERR(edp, "%s %u error", __func__, status);
	else
		eDP_PR(edp, "%s %u ok (0x%px)", __func__, status, pin_sel);
}

void edptx_gpios_get(struct edptx_drv_s *edp, u8 status)
{
	eDP_PR(edp, "%s: %u", __func__, status);

	edp->board_data.edptx_vcc_gpio_desc =
		devm_gpiod_get(edp->dev, "edptx_vcc", GPIOD_OUT_LOW);
	if (IS_ERR(edp->board_data.edptx_vcc_gpio_desc)) {
		eDP_ERR(edp, "regist vcc gpio: %p, err: %d",
		       edp->board_data.edptx_vcc_gpio_desc,
		       IS_ERR(edp->board_data.edptx_vcc_gpio_desc));
	}

	edp->board_data.edptx_bist_gpio_desc =
		devm_gpiod_get(edp->dev, "edptx_bist", GPIOD_OUT_LOW);
	if (IS_ERR(edp->board_data.edptx_bist_gpio_desc)) {
		eDP_ERR(edp, "regist bist gpio: %p, err: %d",
		       edp->board_data.edptx_bist_gpio_desc,
		       IS_ERR(edp->board_data.edptx_bist_gpio_desc));
	}
}

static u8 edptx_panel_config_load_timing(struct edptx_drv_s *edp, const struct device_node *child)
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
		edp->panel_data.timing[i].h_period = (u16)para[0];
		edp->panel_data.timing[i].h_act    = (u16)para[1];
		edp->panel_data.timing[i].h_pw     = (u16)para[2];
		edp->panel_data.timing[i].h_bp     = (u16)para[3];
		edp->panel_data.timing[i].ctrl    |= (u16)para[4] ? CTRL_HSYNC_POS : 0;
		edp->panel_data.timing[i].v_period = (u16)para[5];
		edp->panel_data.timing[i].v_act    = (u16)para[6];
		edp->panel_data.timing[i].v_pw     = (u16)para[7];
		edp->panel_data.timing[i].v_bp     = (u16)para[8];
		edp->panel_data.timing[i].ctrl    |= (u16)para[9] ? CTRL_VSYNC_POS : 0;
		edp->panel_data.timing[i].h_blank  =
			edp->panel_data.timing[i].h_period - edp->panel_data.timing[i].h_act;
		edp->panel_data.timing[i].v_blank  =
			edp->panel_data.timing[i].v_period - edp->panel_data.timing[i].v_act;
		edp->panel_data.timing[i].h_fp = edp->panel_data.timing[i].h_blank -
			(edp->panel_data.timing[i].h_pw + edp->panel_data.timing[i].h_bp);
		edp->panel_data.timing[i].v_fp = edp->panel_data.timing[i].v_blank -
			(edp->panel_data.timing[i].v_pw + edp->panel_data.timing[i].v_bp);
		tmp = (u16)para[10];
		if (tmp == 30)
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_RGB_10bit;
		else if (tmp == 18)
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_RGB_6bit;
		else if (tmp == 12)
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_Y_only_12bit;
		else if (tmp == 10)
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_Y_only_10bit;
		else if (tmp == 8)
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_Y_only_8bit;
		else
			edp->panel_data.timing[i].cfmt = DPTX_CFMT_RGB_8bit;

		tmp = (u16)para[11];
		edp->panel_data.timing[i].fr1000 =  tmp * 1000L;

		tmp *= edp->panel_data.timing[i].v_period * edp->panel_data.timing[i].h_period;
		edp->panel_data.timing[i].pclk = tmp;

		eDP_PR(edp, "Panel Config add Timing[%u]: %ux%u@%uhz", i,
			edp->panel_data.timing[i].h_act, edp->panel_data.timing[i].v_act,
			edp->panel_data.timing[i].fr1000 / 1000);
		edp->panel_data.timing_cnt++;

		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-fr", i);

		// ret = of_property_read_u32(child, snode, &val);
		lenp = of_property_count_elems_of_size(child, snode, sizeof(unsigned int));
		if (lenp > 0) {
			lenp = lenp > 12 ? 12 : lenp;
			ret = of_property_read_u32_array(child, snode, &para[0], lenp);
			if (ret)
				return i;
			// eDP_PR(edp, "lenp=%d", lenp);
			for (j = 0; j < (lenp / sizeof(u32)); j++) {
				if (edp->panel_data.timing[i].vmode_add_fr[j] == 0) {
					edp->panel_data.timing[i].vmode_add_fr[j] = (u16)para[j];
					eDP_DBG(edp, "Panel Config Timing[%u] add fr[%u]=%u",
						i, j, edp->panel_data.timing[i].vmode_add_fr[j]);
					break;
				}
			}
		}
		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-range", i);

		ret = of_property_read_u32_array(child, snode, &para[0], 2);
		if (!ret) {
			edp->panel_data.timing[i].frame_rate_range[0] = (u16)para[0];
			edp->panel_data.timing[i].frame_rate_range[1] = (u16)para[1];
			eDP_DBG(edp, "Panel Config Timing[%u] range=[%u~%u]", i,
				edp->panel_data.timing[i].frame_rate_range[0],
				edp->panel_data.timing[i].frame_rate_range[1]);
		}

		memset(snode, 0, 24 * sizeof(char));
		sprintf(snode, "timing-%u-range-limit", i);
		ret = of_property_read_u32_array(child, snode, &para[0], 6);
		if (!ret) {
			edp->panel_data.timing[i].v_period_range[0] = (u16)para[2];
			edp->panel_data.timing[i].v_period_range[1] = (u16)para[3];
			edp->panel_data.timing[i].pclk_range[0] = (u16)para[4];
			edp->panel_data.timing[i].pclk_range[1] = (u16)para[5];
			eDP_DBG(edp, "Panel Config Timing[%u] limit vl=[%u~%u] pclk=[%u~%u]", i,
				edp->panel_data.timing[i].v_period_range[0],
				edp->panel_data.timing[i].v_period_range[1],
				edp->panel_data.timing[i].pclk_range[0],
				edp->panel_data.timing[i].pclk_range[1]);
		}
	}
	return i;
}

static u8 edptx_panel_config_load_from_dts(struct edptx_drv_s *edp)
{
	const struct device_node *np, *child;
	u16 val;
	int ret;
	const char *prop_name = NULL;

	np = of_find_node_by_name(NULL, "Panel_Groups");
	if (!np) {
		eDP_ERR(edp, "failed to get Panel_Groups");
		return 1;
	}

	prop_name = eDP_propname[edp->idx];
	if (!prop_name) {
		eDP_PR(edp, "%s: not load panel_type", __func__);
		return 1;
	}

	child = of_get_child_by_name(np, prop_name);
	if (!child) {
		eDP_ERR(edp, "[%s] is not in Panel_Groups", prop_name);
		return 1;
	}

	ret = of_property_read_u16(child, "HPD_ignore", &val);
	if (!ret) {
		edp->setting.user_hpd_ignore = (unsigned char)val;
		if (edp->setting.user_hpd_ignore)
			eDP_PR(edp, "HPD ignore: %ums", edp->setting.user_hpd_ignore);
	}
	edp->setting.user_hpd_ignore = 100;

	ret = of_property_read_u16(child, "assigned-link-rate", &val);
	if (!ret) {
		edp->setting.user_link_rate = (unsigned char)val;
		if (edp->setting.user_link_rate)
			eDP_PR(edp, "assigned link rate: %u", edp->setting.user_link_rate);
	}
	ret = of_property_read_u16(child, "assigned-lane-count", &val);
	if (!ret) {
		edp->setting.user_lane_count = (unsigned char)val;
		if (edp->setting.user_lane_count)
			eDP_PR(edp, "assigned lane count: %u", edp->setting.user_lane_count);
	} else {
		edp->setting.user_lane_count = 0xff;
	}

	ret = of_property_read_u16(child, "use-aux-backlight", &val);
	if (!ret) {
		edp->setting.use_aux_bl_as_possible = (unsigned char)val;
		if (edp->setting.use_aux_bl_as_possible)
			eDP_PR(edp, "AUX backlight: %u", edp->setting.use_aux_bl_as_possible);
	} else {
		edp->setting.use_aux_bl_as_possible = 0;
	}

	ret = of_property_read_u16(child, "assigned-port-count", &val);
	if (!ret && (unsigned char)val) {
		if ((unsigned char)val == 4)
			edp->sink.port_mask = 0xf;
		else if ((unsigned char)val == 2)
			edp->sink.port_mask = 0x3;
		else
			edp->sink.port_mask = 0x1;
		eDP_PR(edp, "assigned port mask: %u", edp->sink.port_mask);
	} else {
		edp->sink.port_mask = 0x1;
	}

	ret = of_property_read_u16(child, "assigned-vmode", &val);
	if (!ret) {
		edp->setting.user_vmode_sel = (unsigned char)val;
		if (edp->setting.user_vmode_sel)
			eDP_PR(edp, "assigned vmode idx: %u", edp->setting.user_vmode_sel);
	} else {
		edp->setting.user_vmode_sel = 0;
	}

	ret = of_property_read_u16(child, "assigned-disable-PSR", &val);
	if (!ret) {
		edp->setting.user_disable_PSR = (unsigned char)val;
		if (edp->setting.user_disable_PSR)
			eDP_PR(edp, "assigned disable PSR: %u", edp->setting.user_disable_PSR);
	} else {
		edp->setting.user_disable_PSR = 0;
	}

	ret = of_property_read_u16(child, "assigned-preset-timing", &val);
	if (!ret) {
		edp->setting.user_preset_timing = (unsigned char)val;
		if (edp->setting.user_preset_timing)
			eDP_PR(edp, "assigned preset timing: %u", edp->setting.user_disable_PSR);
	} else {
		edp->setting.user_preset_timing = 0;
	}
	edptx_panel_config_load_timing(edp, child);

	return 0;
}

int dptx_config_load_from_dts(struct edptx_drv_s *edp)
{
	const struct device_node *np;
	unsigned short para[30];
	unsigned char i;
	int ret = 0;

	if (!edp->dev->of_node) {
		eDP_ERR(edp, "dev of_node is null");
		return -1;
	}
	np = edp->dev->of_node;

	memset(&para[0], 0, 30 * sizeof(unsigned char));
	ret = of_property_read_u16_array(np, "drive-strength-lut", &para[0], 30);
	if (ret) {
		eDP_ERR(edp, "DPCD level to PHY LUT not set");
	} else {
		for (i = 0; i < 30; i++)
			edp->board_data.driver_strength_lut[i / 10][i % 3] = para[i];
	}

	ret = of_property_read_u16(np, "edptx_use_external_tx_conn_api", &para[0]);
	if (!ret) {
		edp->board_data.use_ext_tx_conn_api = (u8)para[0];
		if (edp->board_data.use_ext_tx_conn_api)
			eDP_PR(edp, "use ext tx conn API!");
	} else {
		edp->board_data.use_ext_tx_conn_api = 0;
	}

	if (edp->board_data.use_ext_tx_conn_api) {
		ret = of_property_read_u16_array(np, "edptx_external_tx_conn_api_hw_id",
						&para[0], eDPTX_MAX_PORT);
		for (i = 0; i < eDPTX_MAX_PORT; i++)
			edp->board_data.ext_tx_conn_api_idx[i] = ret ? i : para[i];
		eDP_PR(edp, "use ext tx conn API! [%u]", edp->board_data.ext_tx_conn_api_idx[0]);
	}

	i = edptx_panel_config_load_from_dts(edp);
	if (i) {
		eDP_PR(edp, "not loading panel config");
		edp->setting.user_link_rate = 0;
		edp->setting.user_lane_count = 0;
		edp->sink.port_mask = 0x1;
		edp->setting.user_vmode_sel = 0;
		edp->setting.user_disable_PSR = 0;
		edp->setting.user_color_format = 0xff;
	}
	return 0;
}

static int dptx_config_probe(struct edptx_drv_s *edp, struct platform_device *pdev)
{
	int ret = 0;

	ret = dptx_config_load_from_dts(edp);
	if (ret)
		return -1;

	edp->vsync_irq[0] = platform_get_irq_byname(pdev, "vsync");
	edp->vsync_irq[1] = platform_get_irq_byname(pdev, "vsync2");
	edp->vsync_irq[2] = platform_get_irq_byname(pdev, "vsync3");
	edp->HPD_irq      = platform_get_irq_byname(pdev, "edptx_0_hpd");

	dptx_clk_config_probe(edp);
	edptx_phy_probe(edp);
	edptx_venc_probe(edp);
	dptx_debug_probe(edp);
	edptx_if_IP_probe(edp);

	edptx_vout_server_init(edp); // get viu sel here

	edptx_bootup_config_init(edp);

	/* lock pinmux as when dptx is used ?? or link on */
	if (edp->viu_sel || edp->status & DPTX_STA_LINK_ON) {
		edptx_gpios_get(edp, 1);

		edptx_driver_panel_power_ctrl(edp, 1);

		edptx_pinmux_set(edp, 1);

		edptx_drv_check_HPD(edp);

		dptx_timing_config_restore(edp);
	}

	//lcd_vrr_dev_register(edp);

	edptx_irq_init(edp);

	/* add notifier for video sync_duration info refresh */
	edptx_vout_notify_mode_change(edp);

	edptx_bl_wrapper_regist(edp);

	dptx_drm_add(edp->dev);

	edptx_drv_check_HPD(edp);
	// if (edp->viu_sel & 0xf)
	dptx_HPD_trigger_set(edp, 1);

	return 0;
}

#ifdef CONFIG_OF
static struct edptx_chip_data_s edptx_data_t7 = {
	.chip_type = eDPTX_CHIP_T7,
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

static struct edptx_chip_data_s edptx_data_a9 = {
	.chip_type = eDPTX_CHIP_A9,
	.chip_name = "A9",
	//.reg_map_table = &lcd_reg_t7[0],
	.drv_max = 1,
	.offset_venc      = {0x0},
	.offset_venc_if   = {0x0},
	.offset_venc_data = {0x0},
	.venc_clk_msr_id  = {53},
	.link_rate        = {{DP_LINK_RATE_HBR2, 0}},
	.lane_count       = {{4, 0}},
	.TPS_support      = BIT(0) | BIT(1),
	.DACP_support     = BIT(2),
	.pixel_clk_limit  = 667,
};

static const struct of_device_id edptx_dt_match_table[] = {
	{
		.compatible = "amlogic, eDPTX-T7",
		.data = &edptx_data_t7,
	},
	{
		.compatible = "amlogic, eDPTX-A9",
		.data = &edptx_data_a9,
	},
	{}
};
#endif

static int edptx_probe(struct platform_device *pdev)
{
	struct edptx_drv_s *edp;
	const struct of_device_id *match;
	struct edptx_chip_data_s *pdata;
	unsigned int index = 0;
	int ret = 0;

	dptx_global_init_once(pdev);

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "index", &index);
	if (ret) {
		eDP_ERR(NULL, "no index exist");
		return 0;
	}
	if (index >= eDPTX_MAX_DRV) {
		eDP_ERR(NULL, "%s: invalid index %d", __func__, index);
		return -1;
	}

	match = of_match_device(edptx_dt_match_table, &pdev->dev);
	if (!match) {
		eDP_ERR(NULL, "%s: no match table", __func__);
		return -1;
	}
	pdata = (struct edptx_chip_data_s *)match->data;
	pr_info("[eDP:%u]: %s: driver version: %s(%d-%s)", index, __func__,
		DPTX_DRV_VERSION, pdata->chip_type, pdata->chip_name);
	if (index >= pdata->drv_max) {
		eDP_ERR(NULL, "%s: invalid index", __func__);
		return -1;
	}

	edp = edptx_driver_add(index);
	if (!edp)
		goto lcd_probe_err_0;
	/* set drvdata */
	edp_driver[index] = edp;
	edp->data = pdata;
	platform_set_drvdata(pdev, edp);
	edp->pdev = pdev;

#ifdef CONFIG_AMLOGIC_VPU
	/*vpu dev register for lcd*/
	edp->vpu_dev = vpu_dev_register(VPU_VENCL, eDPTX_CDEV_NAME);
#endif
	ret = dptx_ioremap(edp, pdev);
	if (ret)
		goto lcd_probe_err_1;

	spin_lock_init(&edp->isr_lock);

	ret = edptx_cdev_add(edp, &pdev->dev);
	if (ret)
		goto lcd_probe_err_2;

	ret = dptx_config_probe(edp, pdev);
	if (ret)
		goto lcd_probe_err_2;

	edp->status |= DPTX_STA_PROBE_DONE;

	eDP_PR(edp, "%s ok, status:0x%02x", __func__, edp->status);

	return 0;

lcd_probe_err_2:
	dptx_cdev_remove(edp);
lcd_probe_err_1:
	platform_set_drvdata(pdev, NULL);
	edp_driver[index] = NULL;
	kfree(edp);
lcd_probe_err_0:
	return ret;
}

static void edptx_remove(struct platform_device *pdev)
{
	struct edptx_drv_s *edp = platform_get_drvdata(pdev);
	u8 index;

	if (!edp)
		return;

	edp->status &= ~DPTX_STA_PROBE_DONE;

	dptx_drm_remove(edp->dev);

	index = edp->idx;

	edptx_irq_serve_remove(edp);
	dptx_cdev_remove(edp);
	dptx_debug_remove(edp);
	edptx_config_remove(edp);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);

	kfree(edp->reg_map);
	kfree(edp);
	edp_driver[index] = NULL;
	dptx_global_remove_once();

	pr_info("[eDP:%u]: %s done", index, __func__);
}

static int edptx_resume(struct platform_device *pdev)
{
	struct edptx_drv_s *edp = platform_get_drvdata(pdev);

	if (!edp)
		return 0;

	edptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)edp);

	eDP_PR(edp, "%s finished (0x%x)", __func__, edp->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static int edptx_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct edptx_drv_s *edp = platform_get_drvdata(pdev);

	if (!edp)
		return 0;

	if (edp->status & DPTX_STA_LINK_ON)
		edptx_notifier_call_chain(DPTX_EVENT_DISP_OFF, (void *)edp);

	edptx_notifier_call_chain(DPTX_EVENT_UNPREPARE, (void *)edp);

	eDP_PR(edp, "%s finished (0x%x)", __func__, edp->status);

	//mutex_unlock(&lcd_power_mutex);
	return 0;
}

static void edptx_shutdown(struct platform_device *pdev)
{
	struct edptx_drv_s *edp = platform_get_drvdata(pdev);

	if (!edp)
		return;

	eDP_PR(edp, "%s (0x%x)", __func__, edp->status);

	edptx_notifier_call_chain(DPTX_EVENT_DISABLE, (void *)edp);
}

static struct platform_driver eDP_platform_driver = {
	.probe    = edptx_probe,
	.remove   = edptx_remove,
	.suspend  = edptx_suspend,
	.resume   = edptx_resume,
	.shutdown = edptx_shutdown,
	.driver = {
		.name = "meson-eDPTX",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(edptx_dt_match_table),
#endif
	},
};

int __init eDPTX_init(void)
{
	if (platform_driver_register(&eDP_platform_driver)) {
		eDP_ERR(NULL, "failed to register eDPTX driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit eDPTX_TX_exit(void)
{
	platform_driver_unregister(&eDP_platform_driver);
}

static int edptx_bootargs_setup(char *str)
{
	int ret = 0;
	unsigned int data32 = 0;
	const char *ptr, *ptr2;
	char temp_str[12];

	if (!str)
		return -EINVAL;

	ptr = strstr(str, ",");
	if (!ptr || ptr == str) {
		eDP_ERR(NULL, "%s: invalid boot ctrl str: %s", __func__, str);
		return -EINVAL;
	}
	snprintf(temp_str, (ptr - str > 11) ? 11 : ptr - str + 1, "%s", str);
	ret = kstrtouint(temp_str, 16, &data32);
	if (ret) {
		eDP_ERR(NULL, "%s: invalid data", __func__);
		return -EINVAL;
	}

	edptx_print_level = data32 & 0x3;

	ptr++;
	ptr2 = strstr(ptr, ",");
	if (ptr2) {
		if (ptr2 > ptr)
			snprintf(eDP_propname[0], ptr2 - ptr + 1, "%s", ptr);
		ptr2++;
		if (str + strlen(str) > ptr2)
			snprintf(eDP_propname[1], 32, "%s", ptr2);
	}

	eDP_PR(NULL, "%s: data:0x%x, %s,%s", __func__, data32,
				eDP_propname[0], eDP_propname[1]);
	return 1;
}

__setup("eDPTX=", edptx_bootargs_setup);

struct edptx_drv_s *aml_edptx_get_driver(u8 drv_idx)
{
	if (drv_idx >= eDPTX_MAX_DRV)
		return NULL;

	return edp_driver[drv_idx];
}
EXPORT_SYMBOL(aml_edptx_get_driver);

void aml_edptx_regist_hpd_cb(struct edptx_drv_s *edp, struct connector_hpd_cb *hpd_cb)
{
	edp->drm_hpd_cb.callback = hpd_cb->callback;
	edp->drm_hpd_cb.data = hpd_cb->data;
}
EXPORT_SYMBOL(aml_edptx_regist_hpd_cb);

struct edid *aml_edptx_get_raw_edid(struct edptx_drv_s *edp)
{
	return edp->sink.edid.drm_edid;
}
EXPORT_SYMBOL(aml_edptx_get_raw_edid);

int aml_edptx_get_hpd_state(u8 drv_idx)
{
	struct edptx_drv_s *edp = aml_edptx_get_driver(drv_idx);

	if (!edp)
		return 0;
	return edp->status & DPTX_STA_HPD_HIGH ? 1 : 0;
}
EXPORT_SYMBOL(aml_edptx_get_hpd_state);

//MODULE_DESCRIPTION("Meson DisplayPort TX Driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");
