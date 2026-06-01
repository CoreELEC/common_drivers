// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/reboot.h>

struct reg_info {
	void __iomem *reg;
	u32 value;
	u32 mask;
};

struct amlogic_pdd_monitor {
	struct miscdevice miscdev;
	wait_queue_head_t waitq;
	unsigned long status;		/* Internal status bits */
#define _PDDM_DEV_OPEN		0	/* Opened ? */
#define _PDDM_IRQ_TRIGGER	1	/* IRQ triggered ? */
	bool reboot_en;
	struct work_struct reboot_work;
	bool table_cfg_en;
	struct reg_info *tables;
	u32 table_count;
};

static inline struct amlogic_pdd_monitor *pdd_monitor_get_priv(struct file *file)
{
	return container_of(file->private_data, struct amlogic_pdd_monitor, miscdev);
}

static int pdd_monitor_open(struct inode *inode, struct file *file)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	/* The pdd_monitor is single open! */
	if (test_and_set_bit(_PDDM_DEV_OPEN, &priv->status))
		return -EBUSY;

	return 0;
}

static int pdd_monitor_release(struct inode *inode, struct file *file)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	clear_bit(_PDDM_DEV_OPEN, &priv->status);

	return 0;
}

static ssize_t pdd_monitor_write(struct file *file, const char __user *buffer,
				 size_t count, loff_t *ppos)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	clear_bit(_PDDM_IRQ_TRIGGER, &priv->status);
	*ppos = 0;

	return count;
}

static ssize_t pdd_monitor_read(struct file *file, char __user *buffer, size_t count,
				loff_t *ppos)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);
	char *value = test_bit(_PDDM_IRQ_TRIGGER, &priv->status) ? "1\n" : "0\n";
	size_t len = strlen(value);
	size_t copy_size = len < count ? len : count;

	if (*ppos)
		return 0;

	if (copy_to_user(buffer, value, copy_size) == 0) {
		*ppos = copy_size;
		return copy_size;
	}

	return  0;
}

static __poll_t pdd_monitor_poll(struct file *file, poll_table *wait)
{
	struct amlogic_pdd_monitor *priv = pdd_monitor_get_priv(file);

	poll_wait(file, &priv->waitq, wait);

	if (test_bit(_PDDM_IRQ_TRIGGER, &priv->status))
		return EPOLLIN | EPOLLRDNORM;

	return 0;
}

static const struct file_operations pdd_monitor_fops = {
	.owner		= THIS_MODULE,
	.open		= pdd_monitor_open,
	.release	= pdd_monitor_release,
	.read		= pdd_monitor_read,
	.write		= pdd_monitor_write,
	.poll		= pdd_monitor_poll,
};

static void reboot_work_handler(struct work_struct *work)
{
	pr_warn("!!REBOOT NOW!!\n");
	kernel_restart("Power-down detected");
}

static irqreturn_t amlogic_pdd_monitor_irq(int irq, void *data)
{
	struct device *dev = data;
	struct amlogic_pdd_monitor *priv = dev_get_drvdata(dev);
	int index;
	struct reg_info *info;
	u32 tmp;

	if (priv->table_cfg_en) {
		for (index = 0; index < priv->table_count; index++) {
			info = &priv->tables[index];
			tmp = readl(info->reg) & ~info->mask;
			tmp |= info->value & info->mask;
			writel(tmp, info->reg);
		}
	}

	set_bit(_PDDM_IRQ_TRIGGER, &priv->status);
	wake_up_interruptible(&priv->waitq);
	dev_warn(dev, "DETECTED DOWN\n");

	if (priv->reboot_en)
		schedule_work(&priv->reboot_work);

	return IRQ_HANDLED;
}

static int amlogic_pdd_monitor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int irq;
	int ret;
	struct amlogic_pdd_monitor *priv;
	int index;
	u32 u32_count;
	u32 *u32_array;
	u32 address;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->table_cfg_en = of_property_read_bool(dev->of_node, "amlogic,table-cfg-enable");
	if (priv->table_cfg_en) {
		u32_count = of_property_count_u32_elems(dev->of_node, "amlogic,tables");
		if (u32_count > 0 && !(u32_count % 3))
			priv->table_count = u32_count / 3;
		else
			return dev_err_probe(dev, -EINVAL,
					     "%d elems were found, but the format is incorrect\n",
					     u32_count);

		priv->tables = devm_kcalloc(&pdev->dev, priv->table_count,
					    sizeof(*priv->tables), GFP_KERNEL);
		if (!priv->tables)
			return -ENOMEM;
		u32_array = kmalloc_array(u32_count, sizeof(u32), GFP_KERNEL);
		if (!u32_array)
			return -ENOMEM;

		ret = of_property_read_u32_array(dev->of_node, "amlogic,tables", u32_array,
						 u32_count);
		if (ret) {
			kfree(u32_array);
			return ret;
		}

		for (index = 0; index < priv->table_count; index++) {
			address = u32_array[index * 3];
			priv->tables[index].reg = devm_ioremap(dev, address, sizeof(u32));
			if (!priv->tables[index].reg) {
				kfree(u32_array);
				return dev_err_probe(dev, -ENOMEM,
						     "Address where ioremap failed: 0x%08x\n",
						     address);
			}
			priv->tables[index].value = u32_array[index * 3 + 1];
			priv->tables[index].mask = u32_array[index * 3 + 2];
			dev_dbg(dev, "table[%d]: reg[0x%08x(%p)], value[0x%08x], mask[0x%08x]\n",
				index, address, priv->tables[index].reg,
				priv->tables[index].value, priv->tables[index].mask);
		}

		kfree(u32_array);
		dev_info(dev, "A total of %d tables were parsed\n", priv->table_count);
	}

	priv->reboot_en = of_property_read_bool(dev->of_node, "amlogic,reboot-enable");
	INIT_WORK(&priv->reboot_work, reboot_work_handler);

	init_waitqueue_head(&priv->waitq);
	priv->status = 0;

	dev_set_drvdata(&pdev->dev, priv);

	irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!irq)
		return dev_err_probe(dev, -EINVAL, "Failed to map interrupts\n");
	dev_info(dev, "IRQ number: %d\n", irq);

	ret = devm_request_irq(dev, irq, amlogic_pdd_monitor_irq,
			       IRQF_SHARED, dev_name(dev), dev);
	if (ret)
		return ret;

	priv->miscdev.fops = &pdd_monitor_fops;
	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = "pdd-monitor";
	priv->miscdev.parent = dev;
	ret = misc_register(&priv->miscdev);
	if (ret)
		return dev_err_probe(dev, ret, "Unable to register misc device\n");

	return 0;
}

static void amlogic_pdd_monitor_remove(struct platform_device *pdev)
{
	struct amlogic_pdd_monitor *priv = dev_get_drvdata(&pdev->dev);

	misc_deregister(&priv->miscdev);
}

static const struct of_device_id amlogic_pdd_monitor_of_match[] = {
	{ .compatible = "amlogic,pdd-monitor", },
	{ /* sentinel */ },
};

static struct platform_driver amlogic_pdd_monitor_driver = {
	.probe = amlogic_pdd_monitor_probe,
	.remove = amlogic_pdd_monitor_remove,
	.driver = {
		.name = "amlogic-pdd-monitor",
		.of_match_table = amlogic_pdd_monitor_of_match,
	},
};

int __init amlogic_pdd_monitor_driver_init(void)
{
	return platform_driver_register(&amlogic_pdd_monitor_driver);
}

void __exit amlogic_pdd_monitor_driver_exit(void)
{
	platform_driver_unregister(&amlogic_pdd_monitor_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic power down detection monitor driver");
