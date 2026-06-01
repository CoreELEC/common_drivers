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
#include <linux/math64.h>
#include <asm/uaccess.h>

#include "ucs116e0_light.h"

struct ucs116e0_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
};

struct ucs116e0_data {
	struct i2c_client *client;
	struct ucs116e0_platform_data pdata;
	struct input_dev *input_dev;
	struct timer_list polling_timer;
	struct work_struct ucs116e0_work;
	struct workqueue_struct *ucs116e0_wq;
	u8 sub_gain_reg;
	u8 pga_gain_reg;
	u8 inte_time_reg;
	u8 als_dark_coef;
	u8 als_win_loss;
};

struct ucs116e0_data *ucs116e0;
static uint8_t read_data[3] = {0x00, 0x00, 0x00};

static int debug_en;
#define SEI_UCS116E0_DEBUG_ENABLE

#ifdef SEI_UCS116E0_DEBUG_ENABLE
#define SEI_UCS116E0_DEBUG(fmt, args...) \
	printk(KERN_INFO "[ucs116e0][%s][%d] " fmt "\n", __func__, __LINE__, ##args)
#define SEI_UCS116E0_DEBUG1(fmt, args...) \
	do { if ((debug_en & 0x001) == 0x001) \
		printk(KERN_INFO "[ucs116e0][%s][%d] " fmt "\n", __func__, __LINE__, ##args); \
	} while (0)
#else
#define SEI_UCS116E0_DEBUG(fmt, args...)
#define SEI_UCS116E0_DEBUG1(fmt, args...)
#endif

static int ucs116e0_read_regs(struct ucs116e0_data *dev, unsigned char sreg_addr,
			       uint8_t length, uint8_t *bufs)
{
	int ret;
	struct i2c_msg msg[2];
	int retries = 0;

	msg[0].addr  = dev->client->addr;
	msg[0].flags = !I2C_M_RD;
	msg[0].len   = 1;
	msg[0].buf   = &sreg_addr;

	msg[1].addr  = dev->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = bufs;

	while (retries < 3) {
		ret = i2c_transfer(dev->client->adapter, msg, 2);
		if (ret >= 0) {
			ret = 1;
			break;
		}
		pr_err("[ucs116e0_read_regs] continue...\n");
		retries++;
	}
	if (retries >= 3)
		pr_err("[ucs116e0_read_regs] error, retries=%d\n", retries);

	return ret;
}

static int __maybe_unused ucs116e0_write_regs(struct ucs116e0_data *dev,
					      unsigned char sreg_addr,
					      uint8_t length, uint8_t *bufs)
{
	int ret;
	struct i2c_msg msg;
	int i, retries = 0;
	uint8_t data[64];

	data[0] = sreg_addr;
	for (i = 1; i <= length; i++)
		data[i] = bufs[i - 1];

	msg.addr  = dev->client->addr;
	msg.flags = !I2C_M_RD;
	msg.len   = length + 1;
	msg.buf   = data;

	while (retries < 3) {
		ret = i2c_transfer(dev->client->adapter, &msg, 1);
		if (ret >= 0) {
			ret = 1;
			break;
		}
		pr_err("[ucs116e0_write_regs] continue...\n");
		retries++;
	}
	if (retries >= 3)
		pr_err("[ucs116e0_write_regs] error, retries=%d\n", retries);

	return ret;
}

static int ucs116e0_i2c_write(struct ucs116e0_data *dev,
			      unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < 5) {
		ret = i2c_smbus_write_byte_data(dev->client, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		msleep(5);
	}
	return ret;
}

static int __maybe_unused ucs116e0_i2c_read(struct ucs116e0_data *dev,
					    unsigned char reg_addr,
					    unsigned int *reg_data)
{
	int data = -1;
	unsigned char cnt = 0;

	while (cnt < 5) {
		data = i2c_smbus_read_byte_data(dev->client, reg_addr);
		if (data < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt, data);
		} else {
			*reg_data = data;
			break;
		}
		cnt++;
		msleep(5);
	}
	return data;
}

/*
 * Register initialization sequence from official UCapteur vendor pseudo code:
 *   INT_FLAG     = 0x00  (clear POR flag, default is 0x80 on power-on)
 *   SUB_GAIN     = 0x07  (ALS_SUB_GAIN=x128, CLEAR_SUB_GAIN=x1)
 *   INTE_TIME    = 0x3F  (64T * 2ms = 128ms integration time)
 *   ALS_CLR_GAIN = 0x04  (ALS_GAIN=x16, CLEAR_GAIN=x1)
 *   SYSM_CTRL    = 0x01  (EN_ALS=1, enable ALS)
 */
static int ucs116e0_init_register(struct ucs116e0_data *dev)
{
	int ret;

	ret = ucs116e0_i2c_write(dev, UCS116E0_INT_FLAG,     0x00);
	if (ret < 0)
		return ret;
	ret = ucs116e0_i2c_write(dev, UCS116E0_SUB_GAIN,     UCS116E0_INIT_SUB_GAIN);
	if (ret < 0)
		return ret;
	ret = ucs116e0_i2c_write(dev, UCS116E0_INTE_TIME,    UCS116E0_INIT_INTE_TIME);
	if (ret < 0)
		return ret;
	ret = ucs116e0_i2c_write(dev, UCS116E0_ALS_CLR_GAIN, UCS116E0_INIT_ALS_CLR_GAIN);
	if (ret < 0)
		return ret;
	ret = ucs116e0_i2c_write(dev, UCS116E0_SYSM_CTRL,    0x01);
	if (ret < 0)
		return ret;

	dev->sub_gain_reg = UCS116E0_INIT_SUB_GAIN;
	dev->pga_gain_reg = UCS116E0_INIT_ALS_CLR_GAIN;
	dev->inte_time_reg = UCS116E0_INIT_INTE_TIME;
	dev->als_dark_coef = 0x80;
	dev->als_win_loss = 0x40;

	{
		unsigned int reg_val;

		if (ucs116e0_i2c_read(dev, UCS116E0_ALS_COEF, &reg_val) >= 0)
			dev->als_dark_coef = reg_val;
		if (ucs116e0_i2c_read(dev, UCS116E0_ALS_WIN_LOSS, &reg_val) >= 0)
			dev->als_win_loss = reg_val;
	}

	return ret;
}

/*
 * Read 23-bit ALS data from registers 0x20/0x21/0x22.
 * Must read L byte first; M and H are latched when L is accessed.
 */
static int read_ALS_data(struct ucs116e0_data *dev)
{
	int als_data;

	ucs116e0_read_regs(dev, UCS116E0_ALS_DATA_L, 3, &read_data[0]);
	als_data = ((read_data[2] & 0x7F) << 16) | (read_data[1] << 8) | read_data[0];
	SEI_UCS116E0_DEBUG1("ALS data = %d", als_data);
	return als_data;
}

/*
 * Read 23-bit CLEAR channel data from registers 0x24/0x25/0x26.
 */
static int read_CLEAR_data(struct ucs116e0_data *dev)
{
	int clr_data;

	ucs116e0_read_regs(dev, UCS116E0_CLEAR_DATA_L, 3, &read_data[0]);
	clr_data = ((read_data[2] & 0x7F) << 16) | (read_data[1] << 8) | read_data[0];
	SEI_UCS116E0_DEBUG1("CLEAR data = %d", clr_data);
	return clr_data;
}

/*
 * Read 23-bit DARK channel data from registers 0x1C/0x1D/0x1E.
 */
static int read_DARK_data(struct ucs116e0_data *dev)
{
	int dark_data;

	ucs116e0_read_regs(dev, UCS116E0_DARK_DATA_L, 3, &read_data[0]);
	dark_data = ((read_data[2] & 0x7F) << 16) | (read_data[1] << 8) | read_data[0];
	SEI_UCS116E0_DEBUG1("DARK data = %d", dark_data);
	return dark_data;
}

static u32 ucs116e0_pga_gain_factor(u8 als_clr_gain_reg)
{
	u8 setting = als_clr_gain_reg & 0x07;

	if (setting > 4)
		setting = 4;

	return 1U << setting;
}

static u32 ucs116e0_sub_gain_factor(u8 sub_gain_reg)
{
	u8 setting = sub_gain_reg & 0x0F;

	if (setting > 7)
		setting = 7;

	return 1U << setting;
}

static int read_compensated_ALS_data(struct ucs116e0_data *dev)
{
	int als_raw, dark_raw, als_comp;

	als_raw = read_ALS_data(dev);
	dark_raw = read_DARK_data(dev);
	als_comp = als_raw - (dev->als_dark_coef * dark_raw / 128);
	if (als_comp < 0)
		als_comp = 0;

	SEI_UCS116E0_DEBUG1("ALS raw = %d, dark = %d, comp = %d",
			   als_raw, dark_raw, als_comp);

	return als_comp;
}

static int ucs116e0_calc_lux(struct ucs116e0_data *dev, int als_comp)
{
	u64 lux;
	u64 denom;
	u32 pga_gain, sub_gain, inte_ms, win_loss;

	if (als_comp <= 0)
		return 0;

	pga_gain = ucs116e0_pga_gain_factor(dev->pga_gain_reg);
	sub_gain = ucs116e0_sub_gain_factor(dev->sub_gain_reg);
	inte_ms = (dev->inte_time_reg + 1) * UCS116E0_INTE_TIME_UNIT_MS;
	win_loss = dev->als_win_loss ? dev->als_win_loss : 64;

	denom = (u64)pga_gain * sub_gain * inte_ms * win_loss *
		UCS116E0_LUX_COEF_DEN;
	if (!denom)
		return 0;

	lux = (u64)als_comp * UCS116E0_LUX_COEF_NUM * 64ULL * 128ULL;
	lux = div64_u64(lux, denom);

	if (lux > INT_MAX)
		return INT_MAX;

	return (int)lux;
}

static int read_lux_data(struct ucs116e0_data *dev)
{
	int als_comp = read_compensated_ALS_data(dev);
	int lux = ucs116e0_calc_lux(dev, als_comp);

	SEI_UCS116E0_DEBUG1("lux data = %d", lux);
	return lux;
}

static void ucs116e0_report_als_data(struct ucs116e0_data *dev)
{
	int als_data = read_ALS_data(dev);

	SEI_UCS116E0_DEBUG1("als_data = %d", als_data);
	input_report_abs(dev->input_dev, ABS_MISC, als_data);
	input_sync(dev->input_dev);
}

static void ucs116e0_timer_work(struct work_struct *work)
{
	struct ucs116e0_data *dev =
		container_of(work, struct ucs116e0_data, ucs116e0_work);

	ucs116e0_report_als_data(dev);
	mod_timer(&dev->polling_timer, jiffies + msecs_to_jiffies(200));
}

static void ucs116e0_timer_hand(struct timer_list *t)
{
	struct ucs116e0_data *dev = from_timer(dev, t, polling_timer);

	queue_work(dev->ucs116e0_wq, &dev->ucs116e0_work);
}

static int ucs116e0_enable(struct ucs116e0_data *dev)
{
	int ret = ucs116e0_init_register(dev);

	if (ret < 0)
		return ret;
	mod_timer(&dev->polling_timer, jiffies + msecs_to_jiffies(200));
	return ret;
}

static void ucs116e0_disable(struct ucs116e0_data *dev)
{
	int ret;

	ret = ucs116e0_i2c_write(dev, UCS116E0_SYSM_CTRL, 0x00);
	if (ret < 0)
		dev_err(&dev->client->dev, "soft power off failed\n");
}

static int ucs116e0_input_open(struct input_dev *input)
{
	struct ucs116e0_data *dev = input_get_drvdata(input);

	if (input->users)
		pr_info("%s: input->users=%d\n", __func__, input->users);
	return ucs116e0_enable(dev);
}

static void ucs116e0_input_close(struct input_dev *input)
{
	SEI_UCS116E0_DEBUG("enter disable");
}

static void ucs116e0_init_input_device(struct ucs116e0_data *dev,
					struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	/* ALS: 23-bit max = 8,388,607 */
	input_set_abs_params(input_dev, ABS_MISC, 0, 8388607, 0, 0);
	input_dev->name = "ucs116e0_indev";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &dev->client->dev;
}

static int ucs116e0_setup_input_device(struct ucs116e0_data *dev)
{
	struct input_dev *input_dev;
	int ret;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&dev->client->dev, "input device allocate failed\n");
		return -ENOMEM;
	}
	dev->input_dev = input_dev;
	input_dev->open  = ucs116e0_input_open;
	input_dev->close = ucs116e0_input_close;
	input_set_drvdata(input_dev, dev);
	ucs116e0_init_input_device(dev, input_dev);

	ret = input_register_device(dev->input_dev);
	if (ret) {
		dev_err(&dev->client->dev,
			"unable to register input device %s: %d\n",
			dev->input_dev->name, ret);
		input_free_device(dev->input_dev);
		return ret;
	}
	return 0;
}

static int ucs116e0_verify(struct ucs116e0_data *dev)
{
	int retval, product_id;
	unsigned char buf[2];

	retval = ucs116e0_read_regs(dev, UCS116E0_PROD_ID_L, 2, &buf[0]);
	if (retval < 0) {
		dev_err(&dev->client->dev, "read product id error\n");
		goto out;
	}
	product_id = (buf[1] << 8) | buf[0];
	if (product_id == UCS116E0_PROD_ID_VAL) {
		SEI_UCS116E0_DEBUG("product id match: 0x%04x", product_id);
		return 0;
	}
	dev_err(&dev->client->dev,
		"product id mismatch: got 0x%04x, expected 0x%04x\n",
		product_id, UCS116E0_PROD_ID_VAL);
	return -ENODEV;
out:
	ucs116e0_disable(dev);
	return retval;
}

static int ucs116e0_release(struct inode *inode, struct file *filp)
{
	SEI_UCS116E0_DEBUG("enter");
	return 0;
}

static int ucs116e0_open(struct inode *inode, struct file *filp)
{
	SEI_UCS116E0_DEBUG("enter");
	return 0;
}

/* sysfs: read ALS channel */
static ssize_t ucs116e0_als_data_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int als_data = read_ALS_data(ucs116e0);

	return snprintf(buf, PAGE_SIZE, "%d\n", als_data);
}

/* sysfs: read CLEAR channel */
static ssize_t ucs116e0_clear_data_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int clr_data = read_CLEAR_data(ucs116e0);

	return snprintf(buf, PAGE_SIZE, "%d\n", clr_data);
}

/* sysfs: read DARK channel */
static ssize_t ucs116e0_dark_data_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int dark_data = read_DARK_data(ucs116e0);

	return snprintf(buf, PAGE_SIZE, "%d\n", dark_data);
}

/* sysfs: read lux value */
static ssize_t lux_data_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int lux_data = read_lux_data(ucs116e0);

	return snprintf(buf, PAGE_SIZE, "%d\n", lux_data);
}

static ssize_t ucs116e0_debug_en_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int databuf[1] = {0};

	if (sscanf(buf, "%d", &databuf[0]) == 1)
		debug_en = databuf[0];
	pr_info("%s() debug_en = %d\n", __func__, debug_en);
	return count;
}

static ssize_t ucs116e0_debug_en_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", debug_en);
}

static const struct file_operations ucs116e0_fops = {
	.owner   = THIS_MODULE,
	.open    = ucs116e0_open,
	.release = ucs116e0_release,
};

static struct miscdevice ucs116e0_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = "ucs116e0",
	.fops  = &ucs116e0_fops,
};

static DEVICE_ATTR(als_data,   S_IWUSR | S_IRUGO, ucs116e0_als_data_show,   NULL);
static DEVICE_ATTR(clear_data, S_IWUSR | S_IRUGO, ucs116e0_clear_data_show, NULL);
static DEVICE_ATTR(dark_data,  S_IWUSR | S_IRUGO, ucs116e0_dark_data_show,  NULL);
static DEVICE_ATTR_RO(lux_data);
static DEVICE_ATTR(debug_en,   S_IWUSR | S_IRUGO, ucs116e0_debug_en_show,   ucs116e0_debug_en_store);

static struct attribute *ucs116e0_attributes[] = {
	&dev_attr_als_data.attr,
	&dev_attr_clear_data.attr,
	&dev_attr_dark_data.attr,
	&dev_attr_lux_data.attr,
	&dev_attr_debug_en.attr,
	NULL
};

static struct attribute_group ucs116e0_attribute_group = {
	.attrs = ucs116e0_attributes
};

static int ucs116e0_i2c_probe(struct i2c_client *client)
{
	struct ucs116e0_platform_data *pdata;
	int ret;

	SEI_UCS116E0_DEBUG("enter, I2C Address: 0x%02x", client->addr);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ucs116e0_platform_data),
				     GFP_KERNEL);
	} else {
		pdata = NULL;
	}

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "client is not i2c capable\n");
		return -ENXIO;
	}
	if (!pdata) {
		dev_err(&client->dev, "platform data is NULL\n");
		return -EINVAL;
	}

	ucs116e0 = kzalloc(sizeof(*ucs116e0), GFP_KERNEL);
	if (!ucs116e0) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}
	ucs116e0->client = client;
	ucs116e0->pdata  = *pdata;
	ucs116e0->sub_gain_reg = UCS116E0_INIT_SUB_GAIN;
	ucs116e0->pga_gain_reg = UCS116E0_INIT_ALS_CLR_GAIN;
	ucs116e0->inte_time_reg = UCS116E0_INIT_INTE_TIME;
	ucs116e0->als_dark_coef = 0x80;
	ucs116e0->als_win_loss = 0x40;

	ret = ucs116e0_verify(ucs116e0);
	if (ret < 0) {
		dev_dbg(&client->dev,
			"ucs116e0: product id not match, defer to other driver\n");
		kfree(ucs116e0);
		return ret;
	}

	i2c_set_clientdata(client, ucs116e0);

	ret = ucs116e0_setup_input_device(ucs116e0);
	if (ret < 0)
		goto err_free;

	ret = misc_register(&ucs116e0_device);
	if (ret) {
		pr_err("ucs116e0: misc_register failed\n");
		goto err_input;
	}

	ret = sysfs_create_group(&ucs116e0_device.this_device->kobj,
				 &ucs116e0_attribute_group);
	if (ret) {
		pr_err("%s: sysfs_create_group error\n", __func__);
		goto err_misc;
	}

	INIT_WORK(&ucs116e0->ucs116e0_work, ucs116e0_timer_work);
	ucs116e0->ucs116e0_wq = create_singlethread_workqueue("ucs116e0_wq");
	timer_setup(&ucs116e0->polling_timer, ucs116e0_timer_hand, 0);

	SEI_UCS116E0_DEBUG("probe success");
	return 0;

err_misc:
	misc_deregister(&ucs116e0_device);
err_input:
	input_unregister_device(ucs116e0->input_dev);
err_free:
	kfree(ucs116e0);
	return ret;
}

static void ucs116e0_i2c_remove(struct i2c_client *client)
{
	struct ucs116e0_data *dev = i2c_get_clientdata(client);

	pr_info("ucs116e0_i2c_remove\n");
	del_timer(&dev->polling_timer);
	cancel_work_sync(&dev->ucs116e0_work);
	destroy_workqueue(dev->ucs116e0_wq);
	sysfs_remove_group(&ucs116e0_device.this_device->kobj,
			   &ucs116e0_attribute_group);
	misc_deregister(&ucs116e0_device);
	input_unregister_device(dev->input_dev);

	if (dev->pdata.exit)
		dev->pdata.exit();

	kfree(dev);
	return;
}

static int ucs116e0_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucs116e0_data *sensor = i2c_get_clientdata(client);
	struct input_dev *input_dev = sensor->input_dev;

	del_timer(&sensor->polling_timer);
	mutex_lock(&input_dev->mutex);
	if (input_dev->users) {
		pr_info("ucs116e0: suspend, disable\n");
		ucs116e0_disable(sensor);
	}
	mutex_unlock(&input_dev->mutex);
	return 0;
}

static int ucs116e0_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ucs116e0_data *sensor = i2c_get_clientdata(client);
	struct input_dev *input_dev = sensor->input_dev;

	mutex_lock(&input_dev->mutex);
	if (input_dev->users)
		ucs116e0_enable(sensor);
	mutex_unlock(&input_dev->mutex);
	return 0;
}

static const struct dev_pm_ops ucs116e0_pm_ops = {
	.suspend = ucs116e0_pm_suspend,
	.resume  = ucs116e0_pm_resume,
};

static const struct i2c_device_id ucs116e0_i2c_id[] = {
	{ "ucs116e0", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ucs116e0_i2c_id);

static const struct of_device_id ucs116e0_dt_match[] = {
	{ .compatible = "sei,ucs116e0" },
	{ },
};

static struct i2c_driver ucs116e0_i2c_driver = {
	.driver = {
		.name          = "ucs116e0",
		.owner         = THIS_MODULE,
		.of_match_table = of_match_ptr(ucs116e0_dt_match),
		.pm            = &ucs116e0_pm_ops,
	},
	.probe    = ucs116e0_i2c_probe,
	.remove   = ucs116e0_i2c_remove,
	.id_table = ucs116e0_i2c_id,
};

static int __init ucs116e0_i2c_init(void)
{
	int ret;

	pr_info("ucs116e0_i2c_init\n");
	ret = i2c_add_driver(&ucs116e0_i2c_driver);
	if (ret)
		pr_err("fail to add ucs116e0 device into i2c\n");
	return ret;
}
module_init(ucs116e0_i2c_init);

static void __exit ucs116e0_i2c_exit(void)
{
	pr_info("ucs116e0_i2c_exit\n");
	i2c_del_driver(&ucs116e0_i2c_driver);
}
module_exit(ucs116e0_i2c_exit);

MODULE_AUTHOR("SEI <sei@seirobotics.net>");
MODULE_DESCRIPTION("UCS116E0 23-bit ALS/CLEAR/DARK Light Sensor Driver");
MODULE_LICENSE("GPL v2");