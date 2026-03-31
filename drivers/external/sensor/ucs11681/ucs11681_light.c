// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include "ucs11681_light.h"

struct ucs11681_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};

struct ucs11681_data {
	struct i2c_client *client;
	struct ucs11681_platform_data pdata;
	struct input_dev *input_dev;
	struct timer_list polling_timer;
	struct work_struct ucs11681_work;
	struct workqueue_struct *ucs11681_wq;
};

struct ucs11681_data *ucs11681;
u8 read_data[2] = {0x00, 0x00}; /* Read Data buffer */
/* u16 als_data = 0x0000; */ /* ALS sensor data */
/* float lux_data = 0x0000; */ /* Lux data */

int debug_en = 0x002;
#define SEI_UCS11681_DEBUG_ENABLE

#ifdef SEI_UCS11681_DEBUG_ENABLE

#define SEI_UCS11681_FUNC_DEBUG(fmt, ...)		\
	pr_info("%s() -- %d " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define SEI_UCS11681_FUNC_DEBUG1(fmt, ...)		\
	do {						\
		if ((debug_en & 0x001) == 0x001)	\
			pr_info("%s() -- %d " fmt, __func__, __LINE__, ##__VA_ARGS__);	\
	} while (0)

#define SEI_UCS11681_FUNC_ENTER(fmt, ...)		\
	pr_info("%s() -- Enter(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__)

#define SEI_UCS11681_FUNC_EXIT(fmt, ...)		\
	pr_info("%s() -- Exit(%d) " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define SEI_UCS11681_FUNC_DEBUG(fmt, ...)
#define SEI_UCS11681_FUNC_DEBUG1(fmt, ...)
#define SEI_UCS11681_FUNC_ENTER(fmt, ...)
#define SEI_UCS11681_FUNC_EXIT(fmt, ...)
#endif
static int ucs11681_read_regs(struct ucs11681_data *ucs11681,
			      unsigned char sreg_addr, u8 length, u8 *bufs)
{
	int ret;
	struct i2c_msg msg[2];
	int retries = 0;

	msg[0].addr  = ucs11681->client->addr;
	msg[0].flags = !I2C_M_RD;
	msg[0].len   = 1;
	msg[0].buf   = &sreg_addr;

	msg[1].addr  = ucs11681->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = bufs;

	while (retries < 3) {
		ret = i2c_transfer(ucs11681->client->adapter, msg, 2);
		if (ret >= 0) {
			ret = 1;
			break;
		}
		retries++;
	}
	return ret;
}

static int __maybe_unused ucs11681_write_regs(struct ucs11681_data *ucs11681,
					      unsigned char sreg_addr, u8 length, u8 *bufs)
{
	int ret;
	struct i2c_msg msg;
	int i, retries = 0;
	u8 data[64];
	/* start register addr->write */
	data[0] = sreg_addr;

	for (i = 1; i <= length; i++)
		data[i] = bufs[i - 1];

	msg.addr = ucs11681->client->addr;
	msg.flags = !I2C_M_RD;
	msg.len = length + 1;
	msg.buf = data;

	while (retries < 3) {
		ret = i2c_transfer(ucs11681->client->adapter, &msg, 1);
		if (ret >= 0) {
			ret = 1;
			break;
		}
		retries++;
	}

	return ret;
}

/*
 * ucs11681 i2c write/read
 */
static int ucs11681_i2c_write(struct ucs11681_data *ucs11681,
		unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;
	//SEI_UCS11681_FUNC_ENTER("enter\n");

	while (cnt < 3) {
		ret = i2c_smbus_write_byte_data(ucs11681->client,
						reg_addr, reg_data);
		if (ret < 0) {
			SEI_UCS11681_FUNC_DEBUG("i2c_write %x=%u, cnt=%d error=%d\n",
						reg_addr, reg_data, cnt, ret);
		} else {
			break;
		}
		cnt++;
	}

	//SEI_UCS11681_FUNC_EXIT("exit\n");
	return ret;
}

static int __maybe_unused ucs11681_i2c_read(struct ucs11681_data *ucs11681,
					    unsigned char reg_addr,
					    unsigned int *reg_data)
{
	int data = -1;
	unsigned char cnt = 0;

	while (cnt < 3) {
		data = i2c_smbus_read_byte_data(ucs11681->client, reg_addr);
		if (data < 0) {
			SEI_UCS11681_FUNC_DEBUG("i2c_read cnt=%d error=%d\n",
						cnt, data);
		} else {
			*reg_data = data;
			break;
		}
		cnt++;
	}

	return data;
}

//=============================================================================
// Register table initialization
//=============================================================================
static int ucs11681_init_register(struct ucs11681_data *ucs11681)
{
	int ret;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	/* ALS wait time : 128 unit */
	ret = ucs11681_i2c_write(ucs11681, UCS11681_WAIT_TIME, 0xCF);
	/* ALS gain : 16x */
	ret = ucs11681_i2c_write(ucs11681, UCS11681_ALS_GAIN, 0x84);
	/* ALS integration time : 64T */
	ret = ucs11681_i2c_write(ucs11681, UCS11681_ALS_TIME, 0x03);
	/* ALS enable + wait */
	ret = ucs11681_i2c_write(ucs11681, UCS11681_SYSM_CTRL, 0x41);
	if (ret < 0)
		return ret;

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return ret;
}

static int read_ALS_data(struct ucs11681_data *ucs11681)
{
	int als_data = 0;

	/* ALS data */
	ucs11681_read_regs(ucs11681, UCS11681_ALS_DATA_L, 2, &read_data[0]);
	als_data = (read_data[1] << 8) + read_data[0];
	SEI_UCS11681_FUNC_DEBUG1("UCS11681 ALS data = %d\n", als_data);

	return als_data;
}

#ifdef ADD_UC11681_INPUT_DEVICE
static void ucs11681_report_accel_data(struct ucs11681_data *ucs11681)
{
	int als_data = read_ALS_data(ucs11681);

	// SEI_UCS11681_FUNC_DEBUG1("ucs11681_report_accel_data: %d\n", als_data);

	input_report_abs(ucs11681->input_dev, ABS_MISC, als_data);
	input_sync(ucs11681->input_dev);
}

static void ucs11681_timer_work(struct work_struct *work)
{
	struct ucs11681_data *ucs11681 = container_of(work, struct ucs11681_data, ucs11681_work);

	ucs11681_report_accel_data(ucs11681);
	mod_timer(&ucs11681->polling_timer, jiffies + msecs_to_jiffies(100));
}

static void ucs11681_timer_hand(struct timer_list *t)
{
	struct ucs11681_data *ucs11681 = from_timer(ucs11681, t, polling_timer);

	queue_work(ucs11681->ucs11681_wq, &ucs11681->ucs11681_work);
}

static int ucs11681_enable(struct ucs11681_data *ucs11681)
{
	int ret;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	ret = ucs11681_init_register(ucs11681);
	if (ret < 0)
		return ret;
	mod_timer(&ucs11681->polling_timer, jiffies + msecs_to_jiffies(100));

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return ret;
}

static void ucs11681_disable(struct ucs11681_data *ucs11681)
{
	int ret;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	ret = ucs11681_i2c_write(ucs11681, UCS11681_SYSM_CTRL, 0x00);
	if (ret < 0)
		SEI_UCS11681_FUNC_DEBUG("soft power off failed\n");

	SEI_UCS11681_FUNC_EXIT("exit\n");
}

static int ucs11681_input_open(struct input_dev *input)
{
	struct ucs11681_data *ucs11681;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	ucs11681 = input_get_drvdata(input);
	if (input->users)
		SEI_UCS11681_FUNC_DEBUG("input->users=%d\n", input->users);
	else
		SEI_UCS11681_FUNC_DEBUG("input->users2=%d\n", input->users);

	return ucs11681_enable(ucs11681);
}

static void ucs11681_input_close(struct input_dev *input)
{
	//struct ucs11681_data *ucs11681 = input_get_drvdata(input);
	SEI_UCS11681_FUNC_ENTER("enter\n");

	if (input->users)
		pr_info("input->users=%d\n", input->users);
	else
		pr_info("input->users2=%d\n", input->users);
	//ucs11681_disable(ucs11681);

	SEI_UCS11681_FUNC_EXIT("exit\n");
}

static void ucs11681_init_input_device(struct ucs11681_data *ucs11681, struct input_dev *input_dev)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MISC, 0, 0xffff, 0, 0);

	input_dev->name = "ucs11681_indev";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &ucs11681->client->dev;

	SEI_UCS11681_FUNC_EXIT("exit\n");
}

static int ucs11681_setup_input_device(struct ucs11681_data *ucs11681)
{
	struct input_dev *input_dev;
	int ret;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	input_dev = input_allocate_device();
	if (!input_dev) {
		SEI_UCS11681_FUNC_DEBUG("input device allocate failed\n");
		return -ENOMEM;
	}

	ucs11681->input_dev = input_dev;
	input_dev->open = ucs11681_input_open;
	input_dev->close = ucs11681_input_close;
	input_set_drvdata(input_dev, ucs11681);
	ucs11681_init_input_device(ucs11681, input_dev);

	ret = input_register_device(ucs11681->input_dev);
	SEI_UCS11681_FUNC_DEBUG("ret = %d\n", ret);
	if (ret) {
		SEI_UCS11681_FUNC_DEBUG("unable to register input device %s: %d\n",
					ucs11681->input_dev->name, ret);
		input_free_device(ucs11681->input_dev);
		return ret;
	}

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return 0;
}
#endif

#ifndef CONFIG_REDUCE_TIME_CONSUME
static int ucs11681_verify(struct ucs11681_data *ucs11681)
{
	int retval, product_id;
	unsigned char buf[2];

	SEI_UCS11681_FUNC_ENTER("enter\n");

	/* ALS data */
	retval = ucs11681_read_regs(ucs11681, UCS11681_PROD_ID_L, 2, &buf[0]);
	if (retval < 0) {
		SEI_UCS11681_FUNC_DEBUG("read err int source\n");
		goto out;
	}
	product_id = (buf[1] << 8) + buf[0];
	if (product_id == UCS11681_PROD_ID_VAL) {
		SEI_UCS11681_FUNC_DEBUG("match product id is success\n");
		return 0;
	}
	SEI_UCS11681_FUNC_DEBUG("match product id is fail\n");
	return -1;
out:
	//ucs11681_disable(ucs11681);

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return retval;
}
#endif

static int ucs11681_release(struct inode *inode, struct file *filp)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");
	return 0;
}

static int ucs11681_open(struct inode *inode, struct file *filp)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");
	return 0;
}

static ssize_t als_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	/* Read ALS Data */
	int als_data = read_ALS_data(ucs11681);  /* ALS data */

	SEI_UCS11681_FUNC_DEBUG("UCS11681 ALS data = %d", als_data);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", als_data);
	return len;
}

static const struct file_operations ucs11681_fops = {
	.owner		= THIS_MODULE,
	.open		= ucs11681_open,
	.release	= ucs11681_release,
};

static struct miscdevice ucs11681_device = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "ucs11681",
	.fops		= &ucs11681_fops,
};

static ssize_t debug_en_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int databuf[1] = {0};

	if (kstrtoint(buf, 10, &databuf[0]) == 0)
		debug_en = databuf[0];

	SEI_UCS11681_FUNC_DEBUG("%s() -- %d; debug_en = %d\n", __func__,
				__LINE__, debug_en);

	return count;
}

static ssize_t debug_en_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", debug_en);

	return len;
}

static DEVICE_ATTR_RO(als_data);
static DEVICE_ATTR_RW(debug_en);

static struct attribute *ucs11681_attributes[] = {
	&dev_attr_als_data.attr,
	&dev_attr_debug_en.attr,
	NULL
};

static struct attribute_group ucs11681_attribute_group = {
	.attrs = ucs11681_attributes
};

static int ucs11681_i2c_probe(struct i2c_client *client)
{
	struct ucs11681_platform_data *pdata = NULL;
	int ret;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ucs11681_platform_data),
				     GFP_KERNEL);
	}
	if (!pdata) {
		SEI_UCS11681_FUNC_DEBUG("platform data is NULL; exiting\n");
		return -EINVAL;
	}

	ucs11681 = kzalloc(sizeof(*ucs11681), GFP_KERNEL);
	if (!ucs11681) {
		SEI_UCS11681_FUNC_DEBUG("failed to allocate memory for module data\n");
		return -ENOMEM;
	}
	/* SEI_UCS11681_FUNC_DEBUG("kzalloc success\n"); */
	ucs11681->client = client;
	ucs11681->pdata = *pdata;
#ifndef CONFIG_REDUCE_TIME_CONSUME
	ret = ucs11681_verify(ucs11681);
	if (ret < 0) {
		// maybe need to do something
		SEI_UCS11681_FUNC_DEBUG("device not recognized\n");
		kfree(ucs11681);
		return ret;
	}
#endif
	i2c_set_clientdata(client, ucs11681);

	ret = ucs11681_init_register(ucs11681);
	if (ret < 0)
		return ret;

#ifdef ADD_UC11681_INPUT_DEVICE
	ucs11681_setup_input_device(ucs11681);
#endif

	ret = misc_register(&ucs11681_device);
	if (ret) {
		SEI_UCS11681_FUNC_DEBUG("ucs11681: misc_register failed\n");
	} else {
		ret = sysfs_create_group(&ucs11681_device.this_device->kobj,
					 &ucs11681_attribute_group);
		if (ret)
			SEI_UCS11681_FUNC_DEBUG("sysfs_create_group error\n");
		SEI_UCS11681_FUNC_DEBUG("misc_register success\n");
	}

#ifdef ADD_UC11681_INPUT_DEVICE
	INIT_WORK(&ucs11681->ucs11681_work, ucs11681_timer_work);
	ucs11681->ucs11681_wq = create_singlethread_workqueue("ucs11681_wq");
	timer_setup(&ucs11681->polling_timer, ucs11681_timer_hand, 0);

	SEI_UCS11681_FUNC_DEBUG("setup_timer success\n");
#endif

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return 0;
}

static void ucs11681_i2c_remove(struct i2c_client *client)
{
	struct ucs11681_data *ucs11681 = i2c_get_clientdata(client);

	SEI_UCS11681_FUNC_ENTER("enter\n");

#ifdef ADD_UC11681_INPUT_DEVICE
	del_timer(&ucs11681->polling_timer);
	input_unregister_device(ucs11681->input_dev);
#endif

	sysfs_remove_group(&ucs11681_device.this_device->kobj, &ucs11681_attribute_group);

	misc_deregister(&ucs11681_device);

	if (ucs11681->pdata.exit)
		ucs11681->pdata.exit();

	kfree(ucs11681);

	SEI_UCS11681_FUNC_EXIT("exit\n");
}

static int ucs11681_pm_suspend(struct device *dev)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");

#ifdef ADD_UC11681_INPUT_DEVICE
	struct i2c_client *client = to_i2c_client(dev);
	struct ucs11681_data *ucs11681 = i2c_get_clientdata(client);

	del_timer(&ucs11681->polling_timer);
	struct input_dev *input_dev = ucs11681->input_dev;

	mutex_lock(&input_dev->mutex);
	if (input_dev->users) {
		SEI_UCS11681_FUNC_DEBUG("ucs11681:ucs11681 pm suspend enter disable\n");
		ucs11681_disable(ucs11681);
	}
	mutex_unlock(&input_dev->mutex);
#endif

	SEI_UCS11681_FUNC_EXIT("exit\n");

	return 0;
}

static int ucs11681_pm_resume(struct device *dev)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");

#ifdef ADD_UC11681_INPUT_DEVICE
	struct i2c_client *client = to_i2c_client(dev);
	struct ucs11681_data *ucs11681 = i2c_get_clientdata(client);
	struct input_dev *input_dev = ucs11681->input_dev;

	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
		ucs11681_enable(ucs11681);
	mutex_unlock(&input_dev->mutex);
#endif

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return 0;
}

static const struct dev_pm_ops ucs11681_pm_ops = {
	.suspend = ucs11681_pm_suspend,
	.resume  = ucs11681_pm_resume,
};

static const struct i2c_device_id ucs11681_i2c_id[] = {
	{ "ucs11681", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ucs11681_i2c_id);

static const struct of_device_id ucs11681_dt_match[] = {
	{ .compatible = "sei,ucs11681" },
	{ },
};

static struct i2c_driver ucs11681_i2c_driver = {
	.driver = {
		.name = "ucs11681",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ucs11681_dt_match),
		.pm = &ucs11681_pm_ops,
	},
	.probe = ucs11681_i2c_probe,
	.remove = ucs11681_i2c_remove,
	.id_table = ucs11681_i2c_id,
};

static int __init ucs11681_i2c_init(void)
{
	int ret = 0;

	SEI_UCS11681_FUNC_ENTER("enter\n");

	ret = i2c_add_driver(&ucs11681_i2c_driver);
	if (ret) {
		SEI_UCS11681_FUNC_DEBUG("fail to add ucs11681 device into i2c\n");
		return ret;
	}

	SEI_UCS11681_FUNC_EXIT("exit\n");
	return 0;
}

static void __exit ucs11681_i2c_exit(void)
{
	SEI_UCS11681_FUNC_ENTER("enter\n");

	i2c_del_driver(&ucs11681_i2c_driver);

	SEI_UCS11681_FUNC_EXIT("exit\n");
}

module_init(ucs11681_i2c_init);
module_exit(ucs11681_i2c_exit);

MODULE_AUTHOR("Wanglg <lean@seirobotics.net>");
MODULE_DESCRIPTION("ucs11681 Light Sensor Driver");
MODULE_LICENSE("GPL v2");
