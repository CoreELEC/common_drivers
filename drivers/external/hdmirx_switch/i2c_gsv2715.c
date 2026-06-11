// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/mutex.h>

#define DRIVER_NAME "gsv2715"
#define DEV_NAME    "gsv2715"

#define IOC_MAGIC   'g'
#define GSV_READ    _IOWR(IOC_MAGIC, 1, struct gsv_reg)
#define GSV_WRITE   _IOW(IOC_MAGIC,  2, struct gsv_reg)
#define GSV_DETECT  _IOR(IOC_MAGIC,  3, int)
#define MAX_BUF_LEN 256

struct gsv_reg {
	u16 reg;
	u16 len;          // 数据长度
	u8  buf[MAX_BUF_LEN];
};

struct gsv_dev {
	struct i2c_client *client;
	struct gpio_desc *int_gpio;
	/* protect ioctl / i2c access */
	struct mutex lock;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
};

/* ================= 16bit I2C读 ================= */
static int gsv_read(struct i2c_client *client, u16 reg, u8 *buf, u16 len)
{
	struct i2c_msg msgs[2];
	u8 addr[2] = { reg >> 8, reg & 0xff };

	msgs[0].addr  = client->addr;
	msgs[0].flags = 0;
	msgs[0].len   = 2;
	msgs[0].buf   = addr;

	msgs[1].addr  = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = len;
	msgs[1].buf   = buf;

	if (i2c_transfer(client->adapter, msgs, 2) != 2)
		return -EIO;

	return 0;
}

/* ================= 16bit I2C写 ================= */
static int gsv_write(struct i2c_client *client, u16 reg, u8 *buf, u16 len)
{
	u8 *tx;
	int ret;

	tx = kmalloc(len + 2, GFP_KERNEL);
	if (!tx)
		return -ENOMEM;

	tx[0] = reg >> 8;
	tx[1] = reg & 0xff;
	memcpy(&tx[2], buf, len);

	ret = i2c_master_send(client, tx, len + 2);

	kfree(tx);

	if (ret != len + 2)
		return -EIO;

	return 0;
}

static int gsv_detect(struct i2c_client *client)
{
	struct i2c_msg msg;
	u8 dummy = 0;
	int ret;

	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 1;
	msg.buf   = &dummy;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret == 1)
		return 0;
	else
		return -ENODEV;
}

/* ================= ioctl ================= */
static long gsv_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct gsv_dev *gsv = file->private_data;
	int ret = 0;

	mutex_lock(&gsv->lock);

	switch (cmd) {
	case GSV_READ:
	case GSV_WRITE: {
		struct gsv_reg data;

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			ret = -EFAULT;
			break;
		}

		if (data.len == 0 || data.len > MAX_BUF_LEN) {
			ret = -EINVAL;
			break;
		}

		if (cmd == GSV_READ) {
			ret = gsv_read(gsv->client, data.reg, data.buf, data.len);
			if (!ret && copy_to_user((void __user *)arg, &data, sizeof(data)))
				ret = -EFAULT;
		} else {
			ret = gsv_write(gsv->client, data.reg, data.buf, data.len);
		}
		break;
	}

	case GSV_DETECT: {
		int present;
		int ret_detect;

		ret_detect = gsv_detect(gsv->client);

		if (ret_detect == 0) {
			present = 1;
		} else if (ret_detect == -ENODEV)  {
			present = 0;
		} else {
			ret = ret_detect;
			break;
		}

		if (put_user(present, (int __user *)arg))
			ret = -EFAULT;

		break;
	}

	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&gsv->lock);
	return ret;
}

static int gsv_open(struct inode *inode, struct file *file)
{
	struct gsv_dev *gsv =
		container_of(inode->i_cdev, struct gsv_dev, cdev);
	file->private_data = gsv;
	return 0;
}

static const struct file_operations gsv_fops = {
	.owner          = THIS_MODULE,
	.open           = gsv_open,
	.unlocked_ioctl = gsv_ioctl,
};

/*
 *static ssize_t gpio_val_show(struct device *dev,
 *                         struct device_attribute *attr, char *buf)
 *{
 *    struct gsv_dev *gsv = dev_get_drvdata(dev);
 *    return sprintf(buf, "%d\n", gpiod_get_value(gsv->int_gpio));
 *}
 *
 *static ssize_t gpio_val_store(struct device *dev,
 *                          struct device_attribute *attr,
 *                          const char *buf, size_t count)
 *{
 *    struct gsv_dev *gsv = dev_get_drvdata(dev);
 *    int val;
 *
 *    kstrtoint(buf, 10, &val);
 *    gpiod_set_value(gsv->int_gpio, val);
 *
 *    return count;
 *}

 *static DEVICE_ATTR_RW(gpio_val);
 */

static int gsv_probe(struct i2c_client *client)
{
	struct gsv_dev *gsv;
    //u8 val = 0;
	//u8 test_buf[4];

	dev_info(&client->dev, "gsv2715 probe start\n");

	gsv = devm_kzalloc(&client->dev, sizeof(*gsv), GFP_KERNEL);
	if (!gsv)
		return -ENOMEM;

	gsv->client = client;
/*
 *    gsv->int_gpio = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_LOW);
 *    if (IS_ERR(gsv->int_gpio)) {
 *		dev_err(&client->dev, "reset gpio failed\n");
 *		return PTR_ERR(gsv->int_gpio);
 *    }
 */
	mutex_init(&gsv->lock);
/*
 *	if (!gsv_read(client, 0xFF3C, test_buf, 4)) {
 *	dev_info(&client->dev,
 *		"I2C test: Chip Name Is :0x%02x 0x%02x, Bin Version Is :0x%02x 0x%02x.\n",
 *		test_buf[0], test_buf[1],
 *		test_buf[2], test_buf[3]);
 *	} else {
 *		dev_err(&client->dev, "I2C test failed\n");
 *	}
 */
    /* char device */
	alloc_chrdev_region(&gsv->devt, 0, 1, DEV_NAME);
	cdev_init(&gsv->cdev, &gsv_fops);
	cdev_add(&gsv->cdev, gsv->devt, 1);

	gsv->class = class_create("gsv2715");
	device_create(gsv->class, NULL, gsv->devt, gsv, DEV_NAME);

    /* sysfs */
    //device_create_file(&client->dev, &dev_attr_gpio_val);

	i2c_set_clientdata(client, gsv);

	dev_info(&client->dev, "gsv2715 probe success\n");
	return 0;
}

static void gsv_remove(struct i2c_client *client)
{
	struct gsv_dev *gsv = i2c_get_clientdata(client);

    //device_remove_file(&client->dev, &dev_attr_gpio_val);
	device_destroy(gsv->class, gsv->devt);
	class_destroy(gsv->class);
	cdev_del(&gsv->cdev);
	unregister_chrdev_region(gsv->devt, 1);
}

static const struct of_device_id gsv_of_match[] = {
	{ .compatible = "amlogic,gsv2715" },
	{}
};
MODULE_DEVICE_TABLE(of, gsv_of_match);

static struct i2c_driver gsv_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = gsv_of_match,
	},
	.probe = gsv_probe,
	.remove = gsv_remove,
};

module_i2c_driver(gsv_driver);

MODULE_LICENSE("GPL");
