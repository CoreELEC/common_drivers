/*
 * Driver for manage data between config.ini and bootloader blob bl30
 *
 * Copyright (C) 2022 Team CoreELEC
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "bl30_manager.h"
#include <linux/amlogic/media/registers/cpu_version.h>
#include "gpio_data.h"

static u32 remotewakeup = 0xffffffff;
static u32 remotewakeupmask = 0xffffffff;
static u32 decode_type = 0;
static u32 enable_system_power = 0;
static char gpio[16] = {0};
static uint32_t gpiopower = 0xffff;

struct platform_device *bl30_pdev = NULL;

static int __init remote_wakeup_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &remotewakeup);
	if (ret) {
		remotewakeup = 0xffffffff;
		return -EINVAL;
	}

	return 0;
}
__setup("remotewakeup=", remote_wakeup_setup);

static int __init remote_wakeupmask_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &remotewakeupmask);
	if (ret) {
		remotewakeupmask = 0xffffffff;
		return -EINVAL;
	}

	return 0;
}
__setup("remotewakeupmask=", remote_wakeupmask_setup);

static int __init remote_decode_type_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 16, &decode_type);
	if (ret) {
		decode_type = 0x0;
		return -EINVAL;
	}

	return 0;
}
__setup("decode_type=", remote_decode_type_setup);

static int __init enable_system_power_setup(char *str)
{
	int ret;

	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(str, 10, &enable_system_power);
	if (ret) {
		enable_system_power = 0;
		return -EINVAL;
	}

	return 0;
}
__setup("enable_system_power=", enable_system_power_setup);

static int __init set_gpio_power(char *str)
{
	if (str == NULL) {
		pr_info("%s no string\n", __func__);
		return -EINVAL;
	}

	memcpy(gpio, str, strlen(str));

	return 0;
}
__setup("gpiopower=", set_gpio_power);

static ssize_t setup_bl30_store(struct class *cla,
			     struct class_attribute *attr,
			     const char *buf, size_t count)

{
	/* mbox request channel */
	struct mbox_chan *bl30_mbox_chan = aml_mbox_request_channel_byname(&bl30_pdev->dev, "ree2aocpu");
	pr_info("%s: Do setup BL30 blob\n", DRIVER_NAME);
	pr_info("%s: IR remote wake-up code: 0x%x\n", DRIVER_NAME, remotewakeup);
	pr_info("%s: IR remote wake-up code protocol: 0x%x\n", DRIVER_NAME, decode_type);
	pr_info("%s: IR remote wake-up code mask: 0x%x\n", DRIVER_NAME, remotewakeupmask);
	pr_info("%s: enable 5V system power on suspend/power off state: %d\n", DRIVER_NAME
		, enable_system_power);
	pr_info("%s: gpiopower: %d (%s)\n", DRIVER_NAME, gpiopower, gpio);

	if (!IS_ERR_OR_NULL(bl30_mbox_chan)) {
		// uboot 2015
		aml_mbox_transfer_data_old(bl30_mbox_chan, MBOX_CMD_SET_USR_DATA, AML_MBOX_CL_REMOTE,
			(void *)&remotewakeup, sizeof(remotewakeup),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data_old(bl30_mbox_chan, MBOX_CMD_SET_USR_DATA, AML_MBOX_CL_IRPROTO,
			(void *)&decode_type, sizeof(decode_type),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data_old(bl30_mbox_chan, MBOX_CMD_SET_USR_DATA, AML_MBOX_CL_REMOTE_MASK,
			(void *)&remotewakeupmask, sizeof(remotewakeupmask),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data_old(bl30_mbox_chan, MBOX_CMD_SET_USR_DATA, AML_MBOX_CL_5V_SYSTEM_POWER,
			(void *)&enable_system_power, sizeof(enable_system_power),
			NULL, 0, MBOX_SYNC);

		// uboot 2019
		aml_mbox_transfer_data(bl30_mbox_chan, MBOX_CMD_SET_REMOTE,
			(void *)&remotewakeup, sizeof(remotewakeup),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data(bl30_mbox_chan, MBOX_CMD_SET_IR_PROTOCOL,
			(void *)&decode_type, sizeof(decode_type),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data(bl30_mbox_chan, MBOX_CMD_SET_REMOTE_MASK,
			(void *)&remotewakeupmask, sizeof(remotewakeupmask),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data(bl30_mbox_chan, MBOX_CMD_SET_USB_POWER,
			(void *)&enable_system_power, sizeof(enable_system_power),
			NULL, 0, MBOX_SYNC);
		aml_mbox_transfer_data(bl30_mbox_chan, MBOX_CMD_SET_GPIO_POWER,
			(void *)&gpiopower, sizeof(gpiopower),
			NULL, 0, MBOX_SYNC);

		devm_kfree(&bl30_pdev->dev, bl30_mbox_chan->cl);
	}

	return count;
}

static CLASS_ATTR_WO(setup_bl30);

static struct class *bl30_manager_class;

static int bl30_manager_probe(struct platform_device *pdev)
{
	int ret;
	int i, x;

	pr_info("%s: driver probe\n", DRIVER_NAME);

	// backup platform device
	bl30_pdev = pdev;

	if (strlen(gpio) > 0) {
		for (i = 0; i < sizeof(bl30_gpios) / sizeof(bl30_gpios_soc_t); i++) {
			if (bl30_gpios[i].cpuid == get_cpu_type()) {
				for (x = 0; x < sizeof(bl30_gpios[i].gpio) / sizeof(bl30_gpio_t); x++) {
					if (!strcmp(bl30_gpios[i].gpio[x].name, gpio)) {
						gpiopower = bl30_gpios[i].gpio[x].number;
						break;
					}
				}
			}
		}
	}

	bl30_manager_class = class_create(THIS_MODULE, DRIVER_NAME);
	ret = class_create_file(bl30_manager_class, &class_attr_setup_bl30);
	if (ret)
		pr_err("%s: create class fail.\n", DRIVER_NAME);

	return ret;
}

static const struct of_device_id bl30_manager_dt_match[] = {
	{ .compatible = "coreelec,bl30_manager" },
	{},
};

static struct platform_driver bl30_manager_driver = {
	.probe		= bl30_manager_probe,
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table	= bl30_manager_dt_match,
	},
};

static int __init bl30_manager_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&bl30_manager_driver);
	if (ret < 0)
		pr_info("%s: unable to register platform driver\n", DRIVER_NAME);

	return ret;
}

static void __exit bl30_manager_exit(void)
{
	platform_driver_unregister(&bl30_manager_driver);
	class_unregister(bl30_manager_class);
	pr_info("%s: driver exited\n", DRIVER_NAME);
}

module_init(bl30_manager_init);
module_exit(bl30_manager_exit);

MODULE_AUTHOR("Portisch, Team CoreELEC");
MODULE_DESCRIPTION("Handle data between config.ini and bootloader blob bl30");
MODULE_LICENSE("GPL");
