// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqreturn.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/workqueue.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/usb/role.h>

#include "phy-meson-usb.h"
#include "../usb_main.h"

static int UDC_exist_flag = -1;

static int meson_u2phy_crg_otg_freeze(struct meson_uphy_instance *instance)
{
	return 0;
}

static int meson_u2phy_crg_otg_thaw(struct meson_uphy_instance *instance)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)instance->meson_uphy;

	if (mphy->usb_aml_regs)
		writel(mphy->otg_helper.pm_buf.usb_r1.d32,
			&mphy->usb_aml_regs->r1);
	writel(mphy->otg_helper.pm_buf.u2p_r2.d32,
			&mphy->u2p_aml_regs[0]->r2);
	writel(mphy->otg_helper.pm_buf.u2p_r0.d32,
			&mphy->u2p_aml_regs[0]->r0);

	/* Don't resync here because device_block_probing is still held.
	 * if hibernation fails,  PM_POST_HIBERNATION will handle.
	 */
	return 0;
}

static int meson_u2phy_crg_otg_restore(struct meson_uphy_instance *instance)
{
	return 0;
}

static int meson_u2phy_crg_otg_hibernation_prepare(struct amlogic_usb_v2 *mphy)
{
	union u2p_r2_v2 reg2;

	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);
	mphy->otg_helper.pm_buf.u2p_r2.d32 = reg2.d32;
	/* Quiesce otg func: clear&disable hwirq, cancel works. */
	if (mphy->otg_helper.controller_type == USB_OTG) {
		reg2.b.usb_iddig_irq = 0;
		reg2.b.iddig_en0 = 0;
		reg2.b.iddig_en1 = 0;
		reg2.b.iddig_th = 0;
		writel(reg2.d32, &mphy->u2p_aml_regs[0]->r2);
		cancel_delayed_work_sync(&mphy->otg_helper.work);
	}

	mphy->otg_helper.pm_buf.u2p_r0.d32 =
			readl(&mphy->u2p_aml_regs[0]->r0);
	if (mphy->usb_aml_regs)
		mphy->otg_helper.pm_buf.usb_r1.d32 = readl(&mphy->usb_aml_regs->r1);
	return 0;
}

static int meson_u2phy_crg_otg_post_hibernation(struct amlogic_usb_v2 *mphy)
{
	//union u2p_r2_v2 reg2;

	//reg2.d32 = readl((void __iomem *)((unsigned long)phy->usb2_phy_cfg + 8));
	//pr_err("line state: %d recover to %d\n",
	//	reg2.b.iddig_curr, phy->pm_buf.u2p_r2.b.iddig_curr);

	if (mphy->usb_aml_regs)
		writel(mphy->otg_helper.pm_buf.usb_r1.d32,
			&mphy->usb_aml_regs->r1);
	writel(mphy->otg_helper.pm_buf.u2p_r2.d32,
			&mphy->u2p_aml_regs[0]->r2);
	writel(mphy->otg_helper.pm_buf.u2p_r0.d32,
			&mphy->u2p_aml_regs[0]->r0);

	/* During STD the idpin line state could lose sync with controller state even the
	 * register has been restored. This happens when line state changes after std but
	 * before h-prepare which disables the irq, records linesate then cancels the unscheduled
	 * role-switch irq work.
	 * e.g.
	 * If the otg plug is removed before h-prepare, the restored state remains as host
	 * even when the otg plug is still removed.
	 * Try to resync after restore.
	 */
	if (mphy->otg_helper.controller_type == USB_OTG)
		schedule_delayed_work(&mphy->otg_helper.work, msecs_to_jiffies(100));

	return 0;
}

/* crg_gadget_exit&crg_gadget_init should be synchronous to prevent racing with crg_udc_pm_cb.
 * However dpm_prepare defer all probes until dpm_complete, the probe inside is still asynchronous.
 * Avoid this by using pm_cb.
 */
static int meson_u2phy_crg_otg_pm_cb(struct notifier_block *notifier,
			      unsigned long pm_event,
			      void *unused)
{
	struct amlogic_usb_v2 *mphy =
		container_of(notifier, struct amlogic_usb_v2, otg_helper.pm_notifier);

	dev_dbg(mphy->dev, "%s called. pm_event:%lu.\n", __func__, pm_event);

	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		meson_u2phy_crg_otg_hibernation_prepare(mphy);
		break;
	case PM_POST_HIBERNATION:
		meson_u2phy_crg_otg_post_hibernation(mphy);
		break;
	case PM_SUSPEND_PREPARE:
	case PM_POST_SUSPEND:
	case PM_RESTORE_PREPARE:
	case PM_POST_RESTORE:
	default:
		break;
	}
	return NOTIFY_DONE;
}

static int meson_u2phy_crg_otg_init(struct amlogic_usb_v2 *mphy)
{
	union usb_r1_v2 r1 = {.d32 = 0};
	union u2p_r2_v2 reg2;

	if (mphy->usb_aml_regs) {
		r1.d32 = readl(&mphy->usb_aml_regs->r1);
		r1.b.u3h_fladj_30mhz_reg = 0x20;
		writel(r1.d32, &mphy->usb_aml_regs->r1);
	}

	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);
	reg2.b.iddig_en0 = 1;
	reg2.b.iddig_en1 = 1;
	reg2.b.iddig_th = 255;
	writel(reg2.d32, &mphy->u2p_aml_regs[0]->r2);

	return 0;
}

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
static int meson_u2phy_crg_otg_role_switch_set(struct usb_role_switch *sw,
				    enum usb_role role)
{
	struct amlogic_usb_v2	*mphy = usb_role_switch_get_drvdata(sw);
	u32 mode;
	int ret;

	dev_dbg(mphy->dev, " MODE=%u, %s\n", role, __func__);

	switch (role) {
	case USB_ROLE_HOST:
		mode = USB_HOST_ONLY;
		break;
	case USB_ROLE_DEVICE:
		mode = USB_DEVICE_ONLY;
		break;
	default:
		if (mphy->role_switch_default_mode == USB_HOST_ONLY)
			mode = USB_HOST_ONLY;
		else
			mode = USB_DEVICE_ONLY;
		break;
	}

	if (mode == USB_HOST_ONLY) {
		crg_gadget_exit();
		crg_init();
	} else {
		crg_exit();
		crg_gadget_init();
		if (UDC_exist_flag != 1) {
			if (crg_udc_get_UDC_name()) {
				ret = crg_otg_write_UDC(crg_udc_get_UDC_name());
				if (ret == 0 || ret == -EBUSY)
					UDC_exist_flag = 1;
			}
		}
	}

	return 0;
}

static enum usb_role meson_u2phy_crg_otg_role_switch_get(struct usb_role_switch *sw)
{
	struct amlogic_usb_v2	*mphy = usb_role_switch_get_drvdata(sw);
	enum usb_role role;

	switch (mphy->current_mode) {
	case MESON_USB_MODE_HOST:
		role = USB_ROLE_HOST;
		break;
	case MESON_USB_MODE_DEVICE:
		role = USB_ROLE_DEVICE;
		break;
	//case PHY_MODE_USB_OTG:
		//role = USB_OTG;
		//break;
	default:
		if (mphy->role_switch_default_mode == USB_HOST_ONLY)
			role = USB_ROLE_HOST;
		else
			role = USB_ROLE_DEVICE;
		break;
	}
	return role;
}

static int meson_u2phy_crg_otg_setup_role_switch(struct amlogic_usb_v2 *mphy)
{
	struct usb_role_switch_desc meson_role_switch = {NULL};
	int ret = 0;

	meson_role_switch.fwnode = dev_fwnode(mphy->dev->parent);
	meson_role_switch.allow_userspace_control = true;
	meson_role_switch.set = meson_u2phy_crg_otg_role_switch_set;
	meson_role_switch.get = meson_u2phy_crg_otg_role_switch_get;
	meson_role_switch.name = "drd";
	meson_role_switch.driver_data = mphy;
	mphy->role_sw = usb_role_switch_register(mphy->dev->parent, &meson_role_switch);
	if (IS_ERR(mphy->role_sw)) {
		ret = PTR_ERR(mphy->role_sw);
		mphy->role_sw = NULL;
	}

	return ret;
}
#endif

static int meson_u2phy_crg_otg_set_mode(struct amlogic_usb_v2 *mphy, enum meson_uphy_mode mode)
{
	int ret = 0;
	struct meson_uphy_instance *inst = dev_get_drvdata(mphy->dev);
	const struct meson_uphy_pdata *pdata = inst->domain_pool->pdata;

	switch (mode) {
	case MESON_USB_MODE_HOST:
#if (IS_ENABLED(CONFIG_USB_ROLE_SWITCH))
		ret = usb_role_switch_set_role(mphy->role_sw, USB_ROLE_HOST);
#else
		crg_gadget_exit();
		crg_init();
#endif
		break;
	case MESON_USB_MODE_DEVICE:
#if (IS_ENABLED(CONFIG_USB_ROLE_SWITCH))
		ret = usb_role_switch_set_role(mphy->role_sw, USB_ROLE_DEVICE);
#else
		crg_exit();
		crg_gadget_init();
		if (UDC_exist_flag != 1) {
			if (crg_udc_get_UDC_name()) {
				ret = crg_otg_write_UDC(crg_udc_get_UDC_name());
				if (ret == 0 || ret == -EBUSY)
					UDC_exist_flag = 1;
			}
		}
#endif
		break;
	case MESON_USB_MODE_OTG:
		meson_u2phy_crg_otg_init(mphy);
		ret = pdata->u2phy_ops->set_mode(mphy, MESON_USB_MODE_OTG);
		break;
	default:
		dev_err(mphy->dev, "%s: unknown phy mode.\n", __func__);
		ret = -EINVAL;
		break;
	}

	if (ret)
		dev_err(mphy->dev, "failed to set %s ret %d\n",
			IS_ENABLED(CONFIG_USB_ROLE_SWITCH) ? "role" : "mode", ret);
	return ret;
}

static void meson_u2phy_crg_otg_work(struct work_struct *work)
{
	struct amlogic_usb_v2 *mphy = container_of(work, struct amlogic_usb_v2,
								otg_helper.work.work);
	union u2p_r2_v2 reg2;
	bool curr;

	if (mphy->otg_helper.mode_work_flag == 1) {
		cancel_delayed_work_sync(&mphy->otg_helper.set_mode_work);
		mphy->otg_helper.mode_work_flag = 0;
	}
	mutex_lock(mphy->otg_helper.otg_mutex);
	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);
	curr = reg2.b.iddig_curr;
	if (reg2.b.iddig_curr == 0)
		meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_HOST);
	else
		meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_DEVICE);

	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);
	reg2.b.usb_iddig_irq = 0;

	/* PHY has comb reset feature may reset otg related bits
	 * to default. Restore reg bits we concern.
	 */
	reg2.b.iddig_curr = curr;
	writel(reg2.d32, &mphy->u2p_aml_regs[0]->r2);
	meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_OTG);

	dev_dbg(mphy->dev, "otg_work r0, r1, r2: 0x%x 0x%x 0x%x.\n",
			readl(&mphy->u2p_aml_regs[0]->r0),
			readl(&mphy->u2p_aml_regs[0]->r1),
			readl(&mphy->u2p_aml_regs[0]->r2));

	mutex_unlock(mphy->otg_helper.otg_mutex);
}

static void meson_u2phy_crg_otg_set_m_work(struct work_struct *work)
{
	struct amlogic_usb_v2 *mphy = container_of(work, struct amlogic_usb_v2,
								otg_helper.set_mode_work.work);
	union u2p_r2_v2 reg2;

	mutex_lock(mphy->otg_helper.otg_mutex);
	mphy->otg_helper.mode_work_flag = 0;
	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);

	if (reg2.b.iddig_curr == 0)
		meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_HOST);
	else
		meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_DEVICE);

	reg2.b.usb_iddig_irq = 0;
	writel(reg2.d32, &mphy->u2p_aml_regs[0]->r2);
	mutex_unlock(mphy->otg_helper.otg_mutex);
}

static irqreturn_t meson_u2phy_crg_otg_detect_irq(int irq, void *dev)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)dev;
	union u2p_r2_v2 reg2;

	reg2.d32 = readl(&mphy->u2p_aml_regs[0]->r2);
	reg2.b.usb_iddig_irq = 0;
	writel(reg2.d32, &mphy->u2p_aml_regs[0]->r2);

	schedule_delayed_work(&mphy->otg_helper.work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

int meson_u2phy_crg_otg_parse(struct device *dev, struct meson_uphy_instance *instance)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)instance->meson_uphy;
	int irq;
	int retval;
	u32 val;

	mphy->otg_helper.otg_port = true;

	retval = of_property_read_u32(dev->of_node, "controller-type",
								&mphy->otg_helper.controller_type);
	if (retval < 0)
		mphy->otg_helper.controller_type = USB_NORMAL;

	if (mphy->otg_helper.controller_type == USB_OTG)
		mphy->otg_helper.hwotg = true;

	dev_info(dev, "controller_type is %d\n"
				"crg_force_device_mode is %d\n"
				"otg is %d\n",
				mphy->otg_helper.controller_type,
				get_otg_mode(), mphy->otg_helper.hwotg);

	mphy->otg_helper.mode_work_flag = 0;
	mphy->otg_helper.otg_mutex = kzalloc(sizeof(*mphy->otg_helper.otg_mutex),
				GFP_KERNEL);
	if (!mphy->otg_helper.otg_mutex)
		return -ENOMEM;

	mutex_init(mphy->otg_helper.otg_mutex);

	INIT_DELAYED_WORK(&mphy->otg_helper.work, meson_u2phy_crg_otg_work);

	mphy->otg_helper.pm_notifier.notifier_call = meson_u2phy_crg_otg_pm_cb;
	register_pm_notifier(&mphy->otg_helper.pm_notifier);

	if (mphy->otg_helper.hwotg) {
		/* m31 */
		if (mphy->phy3_cfg) {
			val = readl(mphy->phy3_cfg + 0x8);
			val |= 1;
			writel(val, mphy->phy3_cfg + 0x8);
		}

		irq = of_irq_get(dev->of_node, 0);
		if (irq < 0)
			return -ENODEV;
		mphy->otg_helper.irqline = irq;
		retval = request_irq(irq, meson_u2phy_crg_otg_detect_irq,
				 IRQF_SHARED, "amlogic_botg_detect", mphy);

		if (retval) {
			dev_err(dev, "request of irq%d failed\n", irq);
			retval = -EBUSY;
			return retval;
		}
	}

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
	if (get_otg_mode())
		mphy->role_switch_default_mode = USB_DEVICE_ONLY;
	else
		mphy->role_switch_default_mode = USB_HOST_ONLY;
	if (ROLE_SWITCH_FLAG) {
		retval = meson_u2phy_crg_otg_setup_role_switch(mphy);
		if (retval) {
			dev_err(dev, "setup role switch failed\n");
			return retval;
		}
	}
#endif

	if (mphy->otg_helper.hwotg == 0) {
		if (get_otg_mode() || mphy->otg_helper.controller_type == USB_DEVICE_ONLY) {
#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
			mphy->role_switch_default_mode = USB_DEVICE_ONLY;
#endif
			meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_DEVICE);
		} else if (mphy->otg_helper.controller_type == USB_HOST_ONLY) {
#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
			mphy->role_switch_default_mode = USB_HOST_ONLY;
#endif
			meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_HOST);
		}
	} else {
		/* The hw otg configuration is slightly different. The otg helper must firstly
		 * configure the phy reg to enable otg irq but the otg config in the reg maybe
		 * reset during phy init called by controller driver afterwards so init the phy
		 * here then do the otg stuffs.
		 */
		retval = instance->phy->ops->init(instance->phy);
		if (retval)
			dev_err(dev, "init phy failed at %s.\n", __func__);

		/* The otg driver firstly touches the phy reg. The reset maybe low at boot. */
//		meson_u2phy_hold_reset(mphy, true);

		meson_u2phy_crg_otg_set_mode(mphy, MESON_USB_MODE_OTG);

		/* The usb2_phy_cfg iddig bit is default 0 and may not change to 1 instantly
		 * with otg connecter plugged at boot time. Defer the mode init here to avoid
		 * excess host mode init.
		 */
		mphy->otg_helper.mode_work_flag = 1;
		INIT_DELAYED_WORK(&mphy->otg_helper.set_mode_work, meson_u2phy_crg_otg_set_m_work);
		schedule_delayed_work(&mphy->otg_helper.set_mode_work, msecs_to_jiffies(1000));

		dev_dbg(mphy->dev, "otg r0~r2 0x%x 0x%x 0x%x.\n",
				readl(&mphy->u2p_aml_regs[0]->r0),
				readl(&mphy->u2p_aml_regs[0]->r1),
				readl(&mphy->u2p_aml_regs[0]->r2));
	}

	instance->pm_ops.freeze = meson_u2phy_crg_otg_freeze;
	instance->pm_ops.thaw = meson_u2phy_crg_otg_thaw;
	instance->pm_ops.restore = meson_u2phy_crg_otg_restore;
	/* todo: PHY has comb reset feature may reset otg related bits
	 * to default at suspend. Restore reg bits we concern at resume.
	 */

	return 0;
}

void meson_u2phy_crg_otg_remove(struct meson_uphy_instance *instance)
{
	struct amlogic_usb_v2 *mphy = (struct amlogic_usb_v2 *)instance->meson_uphy;

	dev_dbg(mphy->dev, "%s entered.\n", __func__);

	if (!mphy) {
		pr_err("%s non meson_uphy!", __func__);
		return;
	}

	if (!mphy->otg_helper.otg_port) {
		dev_dbg(mphy->dev, "%s not meson_uphy, skip.\n", __func__);
		return;
	}

	dev_dbg(mphy->dev, "%s phy dev ptr: %px.\n", __func__, mphy->dev);

	unregister_pm_notifier(&mphy->otg_helper.pm_notifier);

	if (mphy->otg_helper.irqline)
		free_irq(mphy->otg_helper.irqline, mphy);

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
	if (mphy->role_sw)
		usb_role_switch_unregister(mphy->role_sw);
#endif

	if (mphy->otg_helper.set_mode_work.work.func)
		cancel_delayed_work_sync(&mphy->otg_helper.set_mode_work);
	if (mphy->otg_helper.work.work.func)
		cancel_delayed_work_sync(&mphy->otg_helper.work);

	kfree(mphy->otg_helper.otg_mutex);

	if (mphy->current_mode == MESON_USB_MODE_HOST)
		crg_exit();

	if (mphy->current_mode == MESON_USB_MODE_DEVICE)
		crg_gadget_exit();
}

