// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cleanup.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/usb-v2-common.h>
#include <linux/usb/role.h>

#include "../usb_main.h"
#include "core-meson.h"

struct dwc3_otg {
	struct device			*dev;
	struct phy *u2phy;
	struct platform_device		*dwc_dev;
	struct aml_dwc3 *dwc;
	struct device_node *dwc3_np;
	struct software_node *sw_node;
	struct amlogic_otg_helper otg_helper;
	enum usb_dr_mode dr_mode;
	bool is_drd;
	enum usb_role cur_role;
	bool dwc3_poped;
	struct usb_role_switch	*dwc3_core_role_sw;
};

#define DWC3_MESON_IDDIG_DEBOUNCE_MS 1000

static const char *const usb_dr_modes[] = {
	[USB_DR_MODE_UNKNOWN]		= "",
	[USB_DR_MODE_HOST]		= "host",
	[USB_DR_MODE_PERIPHERAL]	= "peripheral",
	[USB_DR_MODE_OTG]		= "otg",
};

static enum usb_dr_mode usb_get_dr_mode_from_string(const char *str)
{
	int ret;

	ret = match_string(usb_dr_modes, ARRAY_SIZE(usb_dr_modes), str);
	return (ret < 0) ? USB_DR_MODE_UNKNOWN : ret;
}

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
static int dwc3_meson_otg_u2p_init(struct amlogic_otg_helper	*otg)
{
	struct dwc3_otg *dwc3_otg =  container_of(otg, struct dwc3_otg,
								otg_helper);
	int ret = 0;
	union u2p_r2_v2 reg2;
	union u2p_r0_v2 reg0;

	/* The controller driver always probes before this otg driver,
	 * the phy reg won't be reset, so it is safe not to init here.
	 */
	//ret = phy_init(dwc3_otg->u2phy);
	//if (ret) {
	//	dev_err(dwc3_otg->dev, "init phy failed at %d.\n", ret);
	//	return ret;
	//}

	reg2.d32 = readl(&otg->u2p_aml_regs->r2);
	reg2.b.iddig_en0 = 1;
	reg2.b.iddig_en1 = 1;
	reg2.b.iddig_th = 255;
	writel(reg2.d32, &otg->u2p_aml_regs->r2);

	reg0.d32 = readl(&otg->u2p_aml_regs->r0);
	reg0.b.IDPULLUP0 = 1;
	/* vbus detect feature, not used but set for compatibility. */
	//reg0.b.DRVVBUS0 = 1;
	writel(reg0.d32, &otg->u2p_aml_regs->r0);
	/* ID DETECT: usb2_otg_aca_en set to 0 */
	//writel(readl(cfg + 0x54) & (~(1 << 2)), (cfg + 0x54));
	/* reg32_20[7] bypass_otg_det default 0. */

	ret = phy_set_mode(dwc3_otg->u2phy, PHY_MODE_USB_OTG);
	if (ret)
		dev_err(dwc3_otg->dev, "init phy failed at %d.\n", ret);

	return ret;
}

static void dwc3_meson_otg_work(struct work_struct *work)
{
	struct amlogic_otg_helper *otg = container_of(work, struct amlogic_otg_helper,
								work.work);
	struct dwc3_otg *dwc3_otg =  container_of(otg, struct dwc3_otg,
								otg_helper);
	union u2p_r2_v2 reg2;
	bool curr;
	int ret;

	if (otg->mode_work_flag == 1) {
		ret = cancel_delayed_work_sync(&otg->set_mode_work);
		otg->mode_work_flag = 0;
		if (ret) {
			dev_err(dwc3_otg->dev, "set mode work is pending, %s exit.\n", __func__);
			return;
		}
	}

	mutex_lock(otg->otg_mutex);
	reg2.d32 = readl(&otg->u2p_aml_regs->r2);
	reg2.b.usb_iddig_irq = 0;
	writel(reg2.d32, &otg->u2p_aml_regs->r2);
	curr = reg2.b.iddig_curr;
	if (reg2.b.iddig_curr == 0) {
		usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_HOST);
		dwc3_otg->cur_role = USB_ROLE_HOST;
	} else {
		usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_DEVICE);
		dwc3_otg->cur_role = USB_ROLE_DEVICE;
	}
	/* PHY has comb reset feature may reset otg related bits
	 * to default. Restore reg bits we concern.
	 */
	reg2.b.iddig_curr = curr;
	writel(reg2.d32, &otg->u2p_aml_regs->r2);
	dwc3_meson_otg_u2p_init(otg);

	dev_dbg(dwc3_otg->dev, "otg_work r0, r1, r2: 0x%x 0x%x 0x%x.\n",
			readl(&otg->u2p_aml_regs->r0),
			readl(&otg->u2p_aml_regs->r1),
			readl(&otg->u2p_aml_regs->r2));

	mutex_unlock(otg->otg_mutex);
}

static void dwc3_meson_set_m_work(struct work_struct *work)
{
	struct amlogic_otg_helper *otg = container_of(work, struct amlogic_otg_helper,
								set_mode_work.work);
	struct dwc3_otg *dwc3_otg =  container_of(otg, struct dwc3_otg,
								otg_helper);
	union u2p_r2_v2 reg2;

	mutex_lock(otg->otg_mutex);
	otg->mode_work_flag = 0;
	reg2.d32 = readl(&otg->u2p_aml_regs->r2);
	reg2.b.usb_iddig_irq = 0;
	writel(reg2.d32, &otg->u2p_aml_regs->r2);

	if (reg2.b.iddig_curr == 0) {
		dwc3_otg->cur_role = USB_ROLE_HOST;
		usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_HOST);
	} else {
		dwc3_otg->cur_role = USB_ROLE_DEVICE;
		usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_DEVICE);
	}
	mutex_unlock(otg->otg_mutex);
}

static irqreturn_t dwc3_meson_otg_u2p_irq(int irq, void *dev)
{
	struct amlogic_otg_helper *otg = (struct amlogic_otg_helper *)dev;
	union u2p_r2_v2 reg2;

	reg2.d32 = readl(&otg->u2p_aml_regs->r2);
	reg2.b.usb_iddig_irq = 0;
	writel(reg2.d32, &otg->u2p_aml_regs->r2);

	schedule_delayed_work(&otg->work, 0);

	return IRQ_HANDLED;
}

static void dwc3_meson_hw_otg_deinit(struct amlogic_otg_helper	*otg)
{
	if (otg->irqline)
		synchronize_irq(otg->irqline);

	cancel_delayed_work_sync(&otg->set_mode_work);
	cancel_delayed_work_sync(&otg->work);
}

static int dwc3_meson_hw_otg_init(struct amlogic_otg_helper	*otg)
{
	int ret = 0;
	struct dwc3_otg *dwc3_otg =  container_of(otg, struct dwc3_otg,
								otg_helper);

	dwc3_meson_otg_u2p_init(otg);

	otg->otg_mutex = devm_kzalloc(dwc3_otg->dev, sizeof(*otg->otg_mutex),
				GFP_KERNEL);
	if (!otg->otg_mutex)
		return -ENOMEM;
	mutex_init(otg->otg_mutex);
	INIT_DELAYED_WORK(&otg->work, dwc3_meson_otg_work);

	otg->irqline = of_irq_get(dwc3_otg->dev->of_node, 0);
	if (otg->irqline < 0)
		return -ENODEV;
	ret = devm_request_irq(dwc3_otg->dev, otg->irqline, dwc3_meson_otg_u2p_irq,
			 IRQF_SHARED, "amlogic_usb_iddig", otg);

	if (ret) {
		pr_err("request of irq%d failed\n", otg->irqline);
		ret = -EBUSY;
		return ret;
	}

	/* The usb2_phy_cfg iddig bit is default 0 and may not change to 1 instantly
	 * with otg connecter plugged at boot time. Defer the mode init here to avoid
	 * excess host mode init.
	 */
	otg->mode_work_flag = 1;
	INIT_DELAYED_WORK(&otg->set_mode_work, dwc3_meson_set_m_work);
	schedule_delayed_work(&otg->set_mode_work, msecs_to_jiffies(DWC3_MESON_IDDIG_DEBOUNCE_MS));

	pr_debug("otg r0~r2 0x%x 0x%x 0x%x.\n",
			readl(&otg->u2p_aml_regs->r0),
			readl(&otg->u2p_aml_regs->r1),
			readl(&otg->u2p_aml_regs->r2));
	return ret;
}
#endif

static void dwc3_meson_dr_deconfig(struct dwc3_otg	*dwc3_otg)
{
	if (!dwc3_otg->is_drd)
		return;

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
	dwc3_meson_hw_otg_deinit(&dwc3_otg->otg_helper);
#endif
}

static int dwc3_otg_dr_configure(struct dwc3_otg	*dwc3_otg)
{
	if (!dwc3_otg->is_drd)
		return 0;

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
	int ret = 0;

	ret = dwc3_meson_hw_otg_init(&dwc3_otg->otg_helper);
	if (ret) {
		dev_err(dwc3_otg->dev, "hw_otg_init failed %d.\n", ret);
		dwc3_meson_hw_otg_deinit(&dwc3_otg->otg_helper);
	}

	return ret;
#else
	dev_err(dwc3_otg->dev, "USB role not supported, cannot set otg mode\n");
	return -EOPNOTSUPP;
#endif
}

static int dwc3_otg_mode_configure(struct dwc3_otg	*dwc3_otg)
{
	const char *dr_mode;
	int err = 0;

	err = of_property_read_string(dwc3_otg->dev->of_node, "dr_mode", &dr_mode);
	if (err < 0)
		dwc3_otg->dr_mode = USB_DR_MODE_UNKNOWN;
	else
		dwc3_otg->dr_mode = usb_get_dr_mode_from_string(dr_mode);

	if (dwc3_otg->dr_mode == USB_DR_MODE_UNKNOWN)
		return -EINVAL;

	dwc3_otg->is_drd = dwc3_otg->dr_mode == USB_DR_MODE_OTG;

	if (dwc3_otg->is_drd) {
		err = dwc3_otg_dr_configure(dwc3_otg);
	} else {
		if (dwc3_otg->dr_mode == USB_DR_MODE_HOST) {
			dwc3_otg->cur_role = USB_ROLE_HOST;
			usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_HOST);
		} else if (dwc3_otg->dr_mode == USB_DR_MODE_PERIPHERAL) {
			dwc3_otg->cur_role = USB_ROLE_DEVICE;
			usb_role_switch_set_role(dwc3_otg->dwc3_core_role_sw, USB_ROLE_DEVICE);
		}
	}

	return err;
}

static int dwc3_otg_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct dwc3_otg	*dwc3_otg;
	struct resource *res;
	struct amlogic_otg_helper	*otg;
	int			ret;

	dwc3_otg = devm_kzalloc(dev, sizeof(*dwc3_otg), GFP_KERNEL);
	if (!dwc3_otg)
		return -ENOMEM;

	platform_set_drvdata(pdev, dwc3_otg);

	dwc3_otg->dev = dev;

	dwc3_otg->u2phy = devm_phy_get(dwc3_otg->dev, "usb2-phy");
	if (IS_ERR(dwc3_otg->u2phy)) {
		dev_err(dwc3_otg->dev, "Cannot get phy, check dts node?\n");
		return PTR_ERR(dwc3_otg->u2phy);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	otg = &dwc3_otg->otg_helper;
	otg->u2p_aml_regs = devm_ioremap(dwc3_otg->dev, res->start, resource_size(res));
	if (IS_ERR_OR_NULL(dwc3_otg->otg_helper.u2p_aml_regs)) {
		dev_err(dwc3_otg->dev, "Cannot get phy reg, check dts reg?\n");
		return -EINVAL;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto disable_rpm;

	dwc3_otg->dwc3_core_role_sw = usb_role_switch_get(dwc3_otg->dev);
	if (IS_ERR_OR_NULL(dwc3_otg->dwc3_core_role_sw)) {
		dev_err(dwc3_otg->dev, "get role-switch----fail\n");
		//ret = PTR_ERR(dwc3_otg->dwc3_core_role_sw);
		ret = -EPROBE_DEFER;
		goto disable_rpm;
	}

	ret = dwc3_otg_mode_configure(dwc3_otg);
	if (ret < 0)
		goto put_usb_role;

	device_set_wakeup_capable(dev, true);
	pm_runtime_put(dev);

	return 0;

put_usb_role:
	usb_role_switch_put(dwc3_otg->dwc3_core_role_sw);
disable_rpm:
	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);

	return ret;
}

static void dwc3_otg_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dwc3_otg *dwc3_otg = platform_get_drvdata(pdev);

	pm_runtime_get_sync(dev);
	dwc3_meson_dr_deconfig(dwc3_otg);
	usb_role_switch_put(dwc3_otg->dwc3_core_role_sw);

	pm_runtime_disable(dev);
	pm_runtime_put_noidle(dev);
}

static int dwc3_otg_pm_suspend(struct device *dev)
{
//	struct dwc3_meson *dwc3_meson = dev_get_drvdata(dev);

	dev_dbg(dev, "dwc3 meson pm suspend.\n");

	return 0;
}

static int dwc3_otg_pm_resume(struct device *dev)
{
//	struct dwc3_meson *dwc3_meson = dev_get_drvdata(dev);

	dev_dbg(dev, "dwc3 meson pm resume.\n");

	return 0;
}

static int dwc3_otg_runtime_suspend(struct device *dev)
{
	return 0;
}

static int dwc3_otg_runtime_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops dwc3_otg_dev_pm_ops = {
	SYSTEM_SLEEP_PM_OPS(dwc3_otg_pm_suspend, dwc3_otg_pm_resume)
	RUNTIME_PM_OPS(dwc3_otg_runtime_suspend, dwc3_otg_runtime_resume,
		       NULL)
};

static const struct of_device_id dwc3_otg_of_match[] = {
	{ .compatible = "amlogic,dwc3-meson-drd", },
	{},
};
MODULE_DEVICE_TABLE(of, dwc3_otg_of_match);

static struct platform_driver dwc3_otg_driver = {
	.probe		= dwc3_otg_probe,
	.remove_new	= dwc3_otg_remove,
	.driver		= {
		.name	= "meson-dwc3-otg",
		.pm	= pm_ptr(&dwc3_otg_dev_pm_ops),
		.of_match_table	= dwc3_otg_of_match,
	},
};

int __init meson_dwc3_otg_switch_init(void)
{
	return platform_driver_register(&dwc3_otg_driver);
}

void __exit meson_dwc3_otg_switch_exit(void)
{
	platform_driver_unregister(&dwc3_otg_driver);
}
