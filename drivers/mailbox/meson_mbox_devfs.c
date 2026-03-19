// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/mailbox_client.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched/signal.h>
#include <linux/amlogic/aml_mbox.h>
#include <dt-bindings/mailbox/amlogic,mbox.h>
#include "meson_mbox_devfs.h"

#define DRIVER_NAME		"mbox-devfs"
#define MBOX_DEV_RX_BUF_LEN_DEFAULT (10)

struct aml_mbox_dev_rx_msg {
	unsigned int msg_cnt;
	unsigned int msg_free;
	struct aml_mbox_rx_data *rx_buf;
	unsigned int rx_buf_len;
	spinlock_t lock; //spin lock for RX ring buffer
};

struct aml_mbox_dev {
	struct list_head list;
	dev_t dev_t;
	struct cdev cdev;
	struct device *dev;
	struct device *p_dev;
	struct mbox_chan *mbox_chan;
	struct class *class;
	struct fasync_struct *async_queue;
	struct aml_mbox_dev_rx_msg *rx_msg;
	wait_queue_head_t waitq;
	const char *name;
	u32 dest;
	u32 mbox_nums;
};

struct aml_mbox_priv_data {
	u32 cmd;
	char data[MBOX_USER_SIZE];
} __packed;

struct aml_mbox_priv {
	struct aml_mbox_dev *aml_dev;
	struct aml_mbox_data *aml_ree2remote_data;
};

static int mbox_try_to_read_rxdata(struct aml_mbox_dev_rx_msg *rx_msg,
		struct aml_mbox_rx_data *rx_data)
{
	struct aml_mbox_rx_data *rx_packet;
	unsigned int msg_cnt;
	unsigned int idx;
	unsigned long flags;
	u32 rx_buf_len;

	spin_lock_irqsave(&rx_msg->lock, flags);
	msg_cnt = rx_msg->msg_cnt;
	if (!msg_cnt) {
		spin_unlock_irqrestore(&rx_msg->lock, flags);
		return 1;
	}

	rx_buf_len = rx_msg->rx_buf_len;
	idx = rx_msg->msg_free;
	if (idx >= msg_cnt)
		idx -= msg_cnt;
	else
		idx += rx_buf_len - msg_cnt;

	rx_packet = &rx_msg->rx_buf[idx];
	memcpy(rx_data, rx_packet, sizeof(*rx_data));
	rx_msg->msg_cnt--;
	spin_unlock_irqrestore(&rx_msg->lock, flags);
	return 0;
}

static ssize_t mbox_message_write(struct file *filp,
				  const char __user *userbuf,
				  size_t count, loff_t *ppos)
{
	struct aml_mbox_priv *aml_priv = filp->private_data;
	struct aml_mbox_dev *aml_dev = aml_priv->aml_dev;
	struct mbox_chan *mbox_chan = aml_dev->mbox_chan;
	struct aml_mbox_chan *aml_chan = mbox_chan->con_priv;
	struct aml_mbox_data *ree2remote_data = aml_priv->aml_ree2remote_data;
	struct aml_mbox_priv_data aml_priv_data;
	struct device *dev = aml_dev->p_dev;
	int ret = 0;

	if (count > MBOX_USER_SIZE) {
		dev_err(dev, "Msg len %zd over range dest %d\n", count, aml_dev->dest);
		return -EINVAL;
	}
	ret = copy_from_user(&aml_priv_data, userbuf, count);
	if (ret)
		return ret;

	ree2remote_data->txbuf = aml_priv_data.data;
	ree2remote_data->cmd = aml_priv_data.cmd;
	ree2remote_data->txsize = count - sizeof(u32);

	switch (aml_dev->dest) {
	case MAILBOX_AOCPU:
		ree2remote_data->rxsize = count - sizeof(u32);
		ree2remote_data->sync = MBOX_SYNC;
		break;
	case MAILBOX_DSP:
		ree2remote_data->rxsize = MBOX_DATA_SIZE;
		ree2remote_data->sync = MBOX_TSYNC;
		break;
	case MAILBOX_SECPU:
		ree2remote_data->txbuf = &aml_priv_data;
		ree2remote_data->rxsize = 0;
		ree2remote_data->txsize = count;
		ree2remote_data->sync = MBOX_SYNC;
		break;
	default:
		break;
	};

	mutex_lock(&aml_chan->mutex);
	ret = mbox_send_message(mbox_chan, ree2remote_data);
	if (ret < 0) {
		dev_err(dev, "Msg send fail %d dest %d\n", ret, aml_dev->dest);
		complete(&ree2remote_data->complete);
		mutex_unlock(&aml_chan->mutex);
		return ret;
	}

	if (ree2remote_data->sync == MBOX_SYNC)
		complete(&ree2remote_data->complete);
	mutex_unlock(&aml_chan->mutex);
	return count;
}

static ssize_t mbox_message_read(struct file *filp, char __user *userbuf,
				 size_t count, loff_t *ppos)
{
	struct aml_mbox_priv *aml_priv = filp->private_data;
	struct aml_mbox_dev *aml_dev = aml_priv->aml_dev;
	struct aml_mbox_data *ree2remote_data = aml_priv->aml_ree2remote_data;
	struct aml_mbox_rx_data aml_rx_data;
	struct device *dev = aml_dev->p_dev;
	struct aml_mbox_dev_rx_msg *rx_msg;
	int ret = 0;
	int rxsize = 0;
	void *rx_data_buf = NULL;
	DECLARE_WAITQUEUE(mbox_wq, current);

	if (aml_dev->dest != MAILBOX_ARM) {
		ret = wait_for_completion_killable(&ree2remote_data->complete);
		if (ret < 0) {
			dev_err(dev, "Read msg wait killed %d\n", ret);
			return -ENXIO;
		}
	}

	barrier();
	*ppos = 0;
	if (count > MBOX_USER_SIZE)
		count = MBOX_USER_SIZE;

	switch (aml_dev->dest) {
	case MAILBOX_ARM:
		rx_msg = aml_dev->rx_msg;
		if (IS_ERR_OR_NULL(rx_msg)) {
			dev_err(dev, "Err: Mbox rx_msg is NULL\n");
			return -EINVAL;
		}

		dev_dbg(dev, "%s: add current task to wait queue\n",
				__func__);
		add_wait_queue(&aml_dev->waitq, &mbox_wq);

		do {
			__set_current_state(TASK_INTERRUPTIBLE);

			ret = mbox_try_to_read_rxdata(rx_msg, &aml_rx_data);
			if (!ret) {
				rx_data_buf = aml_rx_data.buf;
				rxsize = aml_rx_data.size;
				break;
			}

			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}
			schedule();
		} while (1);

		__set_current_state(TASK_RUNNING);
		remove_wait_queue(&aml_dev->waitq, &mbox_wq);

		if (ret)
			return ret;
		break;
	default:
		rxsize = ree2remote_data->rxsize;
		rx_data_buf = ree2remote_data->rxbuf;
		if (!rx_data_buf) {
			dev_err(dev, "Error: RX data buf is NULL!\n");
			return -ENXIO;
		}
		break;
	}

	rxsize = count > rxsize ? rxsize : count;
	dev_dbg(dev, "%s: rxsize = %d\n", __func__, rxsize);
	ret = simple_read_from_buffer(userbuf, rxsize, ppos,
			rx_data_buf, MBOX_USER_SIZE);

	return ret;
}

static long mbox_message_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int mbox_message_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
	struct aml_mbox_dev *aml_dev = container_of(cdev, struct aml_mbox_dev, cdev);
	struct device *dev = aml_dev->p_dev;
	struct aml_mbox_priv *aml_priv;
	struct aml_mbox_data *ree2remote_data;

	aml_priv = devm_kzalloc(dev, sizeof(*aml_priv), GFP_KERNEL);
	if (IS_ERR_OR_NULL(aml_priv))
		return -ENOMEM;

	dev_dbg(dev, "Open dev name: %s\n", aml_dev->name);

	switch (aml_dev->dest) {
	case MAILBOX_AOCPU:
	case MAILBOX_DSP:
	case MAILBOX_SECPU:
		ree2remote_data = devm_kzalloc(dev, sizeof(*ree2remote_data),
					       GFP_KERNEL);
		if (IS_ERR_OR_NULL(ree2remote_data)) {
			devm_kfree(dev, aml_priv);
			return -ENOMEM;
		}

		ree2remote_data->rxbuf = devm_kzalloc(dev, MBOX_USER_SIZE,
						      GFP_KERNEL);
		if (IS_ERR_OR_NULL(ree2remote_data->rxbuf)) {
			devm_kfree(dev, aml_priv);
			devm_kfree(dev, ree2remote_data);
			return -ENOMEM;
		}

		init_completion(&ree2remote_data->complete);
		aml_priv->aml_ree2remote_data = ree2remote_data;
		break;
	default:
		break;
	};

	aml_priv->aml_dev = aml_dev;
	filp->private_data = aml_priv;
	return 0;
}

static int mbox_message_release(struct inode *inode, struct file *filp)
{
	struct aml_mbox_priv *aml_priv = filp->private_data;
	struct aml_mbox_dev *aml_dev = aml_priv->aml_dev;
	struct device *dev = aml_dev->p_dev;

	if (aml_dev->async_queue) {
		fasync_helper(-1, filp, 0, &aml_dev->async_queue);
		dev_dbg(dev, "%s SIGIO removed from async queue\n",
				aml_dev->name);
	}

	filp->private_data = NULL;

	return 0;
}

static int mbox_message_fasync(int fd, struct file *filp, int on)
{
	int ret = 0;
	struct aml_mbox_priv *aml_priv = filp->private_data;
	struct aml_mbox_dev *aml_dev = aml_priv->aml_dev;
	struct device *dev = aml_dev->p_dev;

	if (aml_dev->dest == MAILBOX_ARM) {
		dev_dbg(dev, "%s register mbox message fasync signal\n",
				aml_dev->name);
		ret = fasync_helper(fd, filp, on, &aml_dev->async_queue);
	}

	return ret;
}

static const struct file_operations mbox_message_ops = {
	.write		= mbox_message_write,
	.read		= mbox_message_read,
	.open		= mbox_message_open,
	.release	= mbox_message_release,
	.unlocked_ioctl = mbox_message_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= mbox_message_ioctl,
#endif
	.fasync		= mbox_message_fasync,
};

static int mbox_remote2ree_notify(struct aml_mbox_dev *aml_dev, void *mssg)
{
	struct device *dev = aml_dev->p_dev;
	struct aml_mbox_rx_data *aml_rx_data;
	struct aml_mbox_rx_data *rx_packet;
	struct aml_mbox_dev_rx_msg *rx_msg;
	u32 cmd;
	u32 rx_buf_len;
	unsigned int idx;
	unsigned long flags;

	aml_rx_data = mssg;
	cmd = aml_rx_data->cmd;
	dev_dbg(dev, "%s: cmd = 0x%x\n", __func__, (unsigned int)cmd);

	rx_msg = aml_dev->rx_msg;
	spin_lock_irqsave(&rx_msg->lock, flags);
	rx_buf_len = rx_msg->rx_buf_len;
	if (rx_msg->msg_cnt < rx_buf_len) {
		idx = rx_msg->msg_free;
		rx_packet = &rx_msg->rx_buf[idx];
		memset(rx_packet, 0, sizeof(*rx_packet));
		memcpy(rx_packet, aml_rx_data, sizeof(*rx_packet));
		rx_msg->msg_cnt++;
		if (idx == rx_buf_len - 1)
			rx_msg->msg_free = 0;
		else
			rx_msg->msg_free++;

		spin_unlock_irqrestore(&rx_msg->lock, flags);
		dev_dbg(dev, "Data copied to RX buffer(count: %d/%d)\n",
				rx_msg->msg_cnt, rx_buf_len);

		wake_up_interruptible(&aml_dev->waitq);
		kill_fasync(&aml_dev->async_queue, SIGIO, POLL_IN);
		dev_dbg(dev, "%s: %s wake up task or notify it to read data\n",
				__func__, aml_dev->name);
	} else {
		spin_unlock_irqrestore(&rx_msg->lock, flags);
		dev_err(dev, "Err: dev %s RX buffer full, msg dropped\n",
				aml_dev->name);
	}

	return 0;
}

static void mbox_ao2ree_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct device *dev = cl->dev;
	struct aml_mbox_dev *aml_devs;
	struct aml_mbox_dev *aml_dev;
	u32 mbox_nums;
	int idx;

	aml_devs = dev_get_drvdata(dev);
	mbox_nums = aml_devs->mbox_nums;
	for (idx = 0; idx < mbox_nums; idx++) {
		aml_dev = &aml_devs[idx];
		if (!strcmp(aml_dev->name, "aocpu2ree"))
			break;
	}

	if (idx == mbox_nums) {
		dev_err(dev, "Can't find aocpu2ree dev\n");
		return;
	}

	mbox_remote2ree_notify(aml_dev, mssg);
}

static void mbox_ao2ree_btclk_sync_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct device *dev = cl->dev;
	struct aml_mbox_dev *aml_devs;
	struct aml_mbox_dev *aml_dev;
	u32 mbox_nums;
	int idx;

	aml_devs = dev_get_drvdata(dev);
	mbox_nums = aml_devs->mbox_nums;
	for (idx = 0; idx < mbox_nums; idx++) {
		aml_dev = &aml_devs[idx];
		if (!strcmp(aml_dev->name, "aocpu2ree_btclk_sync"))
			break;
	}

	if (idx == mbox_nums) {
		dev_err(dev, "Can't find aocpu2ree_btclk_sync dev\n");
		return;
	}

	mbox_remote2ree_notify(aml_dev, mssg);
}

static void mbox_dspa2ree_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct device *dev = cl->dev;
	struct aml_mbox_dev *aml_devs;
	struct aml_mbox_dev *aml_dev;
	u32 mbox_nums;
	int idx;

	aml_devs = dev_get_drvdata(dev);
	mbox_nums = aml_devs->mbox_nums;
	for (idx = 0; idx < mbox_nums; idx++) {
		aml_dev = &aml_devs[idx];
		if (!strcmp(aml_dev->name, "dspa2ree"))
			break;
	}

	if (idx == mbox_nums) {
		dev_err(dev, "Can't find dspa2ree dev\n");
		return;
	}

	mbox_remote2ree_notify(aml_dev, mssg);
}

static void mbox_dspb2ree_rx_callback(struct mbox_client *cl, void *mssg)
{
	struct device *dev = cl->dev;
	struct aml_mbox_dev *aml_devs;
	struct aml_mbox_dev *aml_dev;
	u32 mbox_nums;
	int idx;

	aml_devs = dev_get_drvdata(dev);
	mbox_nums = aml_devs->mbox_nums;
	for (idx = 0; idx < mbox_nums; idx++) {
		aml_dev = &aml_devs[idx];
		if (!strcmp(aml_dev->name, "dspb2ree"))
			break;
	}

	if (idx == mbox_nums) {
		dev_err(dev, "Can't find dspb2ree dev\n");
		return;
	}

	mbox_remote2ree_notify(aml_dev, mssg);
}

static void mbox_cdev_cleanup(struct class *mbox_class, dev_t devt,
		struct aml_mbox_dev *mbox_devs, int count)
{
	int i;

	if (!IS_ERR_OR_NULL(mbox_devs)) {
		for (i = 0; i < count; i++) {
			if (!IS_ERR_OR_NULL(mbox_devs[i].mbox_chan))
				mbox_free_channel(mbox_devs[i].mbox_chan);

			if (!IS_ERR_OR_NULL(mbox_devs[i].dev))
				device_destroy(mbox_class, mbox_devs[i].dev_t);

			cdev_del(&mbox_devs[i].cdev);
		}
	}

	if (devt)
		unregister_chrdev_region(devt, count);

	if (!IS_ERR_OR_NULL(mbox_class))
		class_destroy(mbox_class);
}

static int mbox_cdev_init(struct device *dev)
{
	struct class *mbox_class = NULL;
	struct aml_mbox_dev *mbox_devs = NULL;
	struct aml_mbox_dev *mbox_dev = NULL;
	struct aml_mbox_rx_data *mbox_rx_buf = NULL;
	dev_t dev_t = 0;
	u32 idx = 0;
	int ret = 0;
	int mbox_nums = 0;
	unsigned int rx_buf_len = 0;

	dev_dbg(dev, "mbox devfs init start\n");
	ret = of_property_read_u32(dev->of_node,
			     "mbox-nums", &mbox_nums);
	if (ret < 0) {
		dev_dbg(dev, "No mbox-nums\n");
		return -EINVAL;
	}

	mbox_class = class_create("mbox_devfs");
	if (IS_ERR_OR_NULL(mbox_class)) {
		dev_dbg(dev, "Failed to create class: %ld\n", PTR_ERR(mbox_class));
		ret = PTR_ERR(mbox_class);
		goto cleanup;
	}

	ret = alloc_chrdev_region(&dev_t, 0, mbox_nums, DRIVER_NAME);
	if (ret < 0) {
		dev_dbg(dev, "alloc dev_t failed\n");
		goto cleanup;
	}

	mbox_devs = devm_kzalloc(dev, sizeof(*mbox_dev) * mbox_nums, GFP_KERNEL);
	if (IS_ERR_OR_NULL(mbox_devs)) {
		dev_dbg(dev, "Failed to alloc mbox_devs\n");
		ret = -ENOMEM;
		goto cleanup;
	}

	for (idx = 0; idx < mbox_nums; idx++) {
		mbox_dev = &mbox_devs[idx];
		mbox_dev->p_dev = dev;
		mbox_dev->class = mbox_class;
		mbox_dev->dev_t = MKDEV(MAJOR(dev_t), idx);
		cdev_init(&mbox_dev->cdev, &mbox_message_ops);
		ret = cdev_add(&mbox_dev->cdev, mbox_dev->dev_t, 1);
		if (ret) {
			dev_dbg(dev, "mbox fail to add cdev\n");
			goto cleanup;
		}

		if (of_property_read_string_index(dev->of_node,
						  "mbox-names", idx, &mbox_dev->name)) {
			ret = -ENOMEM;
			dev_dbg(dev, "%s get mbox[%d] name fail\n",
				__func__, idx);
			goto cleanup;
		}

		if (of_property_read_u32_index(dev->of_node, "mbox-dests",
					       idx, &mbox_dev->dest)) {
			ret = -ENOMEM;
			dev_dbg(dev, "%s get mbox[%d] dest fail\n",
				__func__, idx);
			goto cleanup;
		}

		mbox_dev->dev = device_create(mbox_class, NULL, mbox_dev->dev_t,
					mbox_dev, "%s", mbox_dev->name);
		if (IS_ERR_OR_NULL(mbox_dev->dev)) {
			ret = PTR_ERR(mbox_dev->dev);
			dev_dbg(dev, "Failed to create device: %d\n", ret);
			goto cleanup;
		}
		mbox_dev->mbox_chan = aml_mbox_request_channel_byidx(dev, idx);
		if (IS_ERR_OR_NULL(mbox_dev->mbox_chan)) {
			ret = PTR_ERR(mbox_dev->dev);
			dev_dbg(dev, "Failed to request mbox chan: %d\n", ret);
			goto cleanup;
		}
		mbox_dev->mbox_nums = mbox_nums;

		if (!strcmp(mbox_dev->name, "aocpu2ree"))
			mbox_dev->mbox_chan->cl->rx_callback = mbox_ao2ree_rx_callback;
		else if (!strcmp(mbox_dev->name, "aocpu2ree_btclk_sync"))
			mbox_dev->mbox_chan->cl->rx_callback = mbox_ao2ree_btclk_sync_rx_callback;
		else if (!strcmp(mbox_dev->name, "dspa2ree"))
			mbox_dev->mbox_chan->cl->rx_callback = mbox_dspa2ree_rx_callback;
		else if (!strcmp(mbox_dev->name, "dspb2ree"))
			mbox_dev->mbox_chan->cl->rx_callback = mbox_dspb2ree_rx_callback;

		if (mbox_dev->dest == MAILBOX_ARM) {
			mbox_dev->rx_msg = devm_kzalloc(dev,
					sizeof(*mbox_dev->rx_msg), GFP_KERNEL);
			if (IS_ERR_OR_NULL(mbox_dev->rx_msg)) {
				ret = -ENOMEM;
				goto cleanup;
			}

			ret = of_property_read_u32(dev->of_node,
					"mbox-rx-queue-length", &rx_buf_len);
			if (ret) {
				rx_buf_len = MBOX_DEV_RX_BUF_LEN_DEFAULT;
			} else {
				if (rx_buf_len < 1 || rx_buf_len > 100) {
					dev_dbg(dev, "Err: RX buf len should be: 1 <= len <= 100\n");
					rx_buf_len = MBOX_DEV_RX_BUF_LEN_DEFAULT;
				}
			}
			dev_dbg(dev, "%s RX buf length: %d\n", mbox_dev->name,
					rx_buf_len);

			mbox_rx_buf = devm_kzalloc(dev,
					sizeof(*mbox_rx_buf) * rx_buf_len,
					GFP_KERNEL);
			if (IS_ERR_OR_NULL(mbox_rx_buf)) {
				ret = -ENOMEM;
				goto cleanup;
			}
			mbox_dev->rx_msg->rx_buf = mbox_rx_buf;
			mbox_dev->rx_msg->msg_cnt = 0;
			mbox_dev->rx_msg->msg_free = 0;
			mbox_dev->rx_msg->rx_buf_len = rx_buf_len;
			spin_lock_init(&mbox_dev->rx_msg->lock);
			init_waitqueue_head(&mbox_dev->waitq);
		}
	}
	dev_set_drvdata(dev, mbox_devs);
	dev_dbg(dev, "mbox devfs init done\n");
	return 0;

cleanup:
	mbox_cdev_cleanup(mbox_class, dev_t, mbox_devs, idx + 1);
	return ret;
}

static int mbox_devfs_probe(struct platform_device *pdev)
{
	return mbox_cdev_init(&pdev->dev);
}

static void mbox_devfs_remove(struct platform_device *pdev)
{
	struct aml_mbox_dev *mbox_devs = dev_get_drvdata(&pdev->dev);
	u32 mbox_nums = mbox_devs->mbox_nums;
	dev_t base_dev = mbox_devs[0].dev_t;
	struct class *mbox_class = mbox_devs[0].class;
	int i;

	for (i = 0; i < mbox_nums; i++) {
		device_destroy(mbox_class, mbox_devs[i].dev_t);
		cdev_del(&mbox_devs[i].cdev);
	}

	class_destroy(mbox_class);
	unregister_chrdev_region(base_dev, mbox_nums);
	platform_set_drvdata(pdev, NULL);
}

static const struct of_device_id mbox_of_match[] = {
	{ .compatible = "amlogic, mbox-devfs" },
	{},
};

static struct platform_driver mbox_devfs_drvier = {
	.probe = mbox_devfs_probe,
	.remove = mbox_devfs_remove,
	.driver = {
		.owner		= THIS_MODULE,
		.name		= DRIVER_NAME,
		.of_match_table = mbox_of_match,
	},
};

int __init mbox_devfs_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&mbox_devfs_drvier);
	return ret;
}

void __exit mbox_devfs_exit(void)
{
	platform_driver_unregister(&mbox_devfs_drvier);
}
