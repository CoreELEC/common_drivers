// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/gfp.h>
#include <linux/compat.h>
#include <asm/current.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/cdev.h>

#include "usbci.h"

#define usbcam_dbg(fmt, args...) \
do {\
	if (debug_usbci)\
		pr_info("usbci: " fmt, ## args);\
} while (0)

static int debug_usbci;

MODULE_PARM_DESC(debug_usbci, "\n\t\t Enable usbci debug information");
module_param(debug_usbci, int, 0644);

#define MAX_CI_DEVICES		2

static dev_t aml_usbcam_devt;
static struct class *aml_usbcam_ci_class;
static struct aml_usbcam *command_devices[MAX_CI_DEVICES];
static struct aml_usbcam *media_devices[MAX_CI_DEVICES];
static DEFINE_MUTEX(ci_devices_mutex);

static unsigned long aml_get_usbcam_version(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;
	unsigned int driver_version = USBCAM_MODULE_DRIVER_VERSION;

	usbcam_dbg("get usbcam version\n");

	if (!usbcam_dev || !arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((unsigned int *)arg, &driver_version, sizeof(unsigned int));

	return ret;
}

static unsigned long aml_get_usbcam_info(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;

	usbcam_dbg("get usbcam info\n");

	if (!usbcam_dev || !arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((struct aml_usbcam_info *)arg, &usbcam_dev->usbcam_module_info,
							sizeof(struct aml_usbcam_info));

	return ret;
}

static unsigned long aml_get_usbcam_capabilities(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;

	usbcam_dbg("get usbcam capabilities\n");

	if (!usbcam_dev || !arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_to_user((struct aml_usbcam_module_capabilities *)arg,
				&usbcam_dev->usbcam_module_capabilities,
				sizeof(struct aml_usbcam_module_capabilities));

	return ret;
}

static unsigned long aml_cancel_transfer(struct file *filp, struct aml_usbcam *usbcam_dev)
{
	usbcam_dbg("cancel transfer\n");

	if (!usbcam_dev) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}
	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		if (atomic_read(&usbcam_dev->read_urb->use_count)) {
			usbcam_dbg("unlink the read urb\n");
			usb_unlink_urb(usbcam_dev->read_urb);
		}
	}
	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		if (atomic_read(&usbcam_dev->write_urb->use_count)) {
			usbcam_dbg("unlink the write urb\n");
			usb_unlink_urb(usbcam_dev->write_urb);
		}
	}
	return 0;
}

static unsigned long aml_get_usbcam_state(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;
	unsigned int status;

	usbcam_dbg("get usbcam state\n");

	if (!usbcam_dev || !arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	status = usbcam_dev->device_state;

	ret = copy_to_user((unsigned int *)arg, &status, sizeof(unsigned int));

	return ret;
}

static unsigned long aml_set_usbcam_state(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;

	usbcam_dbg("set usbcam state\n");

	if (!usbcam_dev || !arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	ret = copy_from_user(&usbcam_dev->device_state, (unsigned int *)arg,
							sizeof(unsigned int));

	return ret;
}

static unsigned long aml_check_device_presence(struct aml_usbcam *usbcam_dev, unsigned long arg)
{
	int ret;
	int is_present = 0;

	usbcam_dbg("check presence\n");

	if (!arg) {
		usbcam_dbg("invalid parameter\n");
		return -EINVAL;
	}

	if (usbcam_dev && usbcam_dev->device_state != DEVICE_DISCONNECT)
		is_present = 1;

	ret = copy_to_user((int *)arg, &is_present, sizeof(int));

	return 0;
}

static int aml_intf_open(struct inode *node, struct file *filp)
{
	int ret = 0;
	int err_val = 0;
	struct aml_usbcam *usbcam_dev = NULL;

	usbcam_dbg("intf open\n");

	if (!node || !filp) {
		ret = -EINVAL;
		err_val = -1;
		goto error_handle;
	}

	usbcam_dev = container_of(node->i_cdev, struct aml_usbcam, cdev);
	usbcam_dev->device_status = DEVICE_STATUS_RUNNING;

	mutex_lock(&usbcam_dev->open_mux);

	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		usbcam_dev->read_open_status = READ_OPEN;
		if (usbcam_dev->read_open_ref >= 1) {
			mutex_unlock(&usbcam_dev->open_mux);
			ret = -EPERM;
			err_val = -3;
		} else {
			usbcam_dev->read_open_ref++;
		}
	}

	if (ret < 0)
		goto error_handle;

	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		usbcam_dev->write_open_status = WRITE_OPEN;
		if (usbcam_dev->write_open_ref >= 1) {
			if ((filp->f_flags & 0x03) == O_RDWR)
				usbcam_dev->read_open_ref--;
			mutex_unlock(&usbcam_dev->open_mux);
			ret = -EPERM;
			err_val = -4;
		} else {
			usbcam_dev->write_open_ref++;
		}
	}

	if (ret < 0)
		goto error_handle;

	mutex_unlock(&usbcam_dev->open_mux);

	kref_get(&usbcam_dev->ref_count);

	filp->private_data = usbcam_dev;
	usbcam_dbg("intf open success\n");
	return 0;

error_handle:
	usbcam_dbg("intf open failed! err_val:%d\n", err_val);
	return ret;
}

static void aml_intf_read_complete(struct urb *urb)
{
	int status;
	ulong flag;
	struct aml_usbcam *usbcam_dev = NULL;

	if (!urb) {
		usbcam_dbg("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	usbcam_dev->in_transfor = 0;
	if (status) {
		usbcam_dev->read_len = 0;
		usbcam_dev->read_err = status;
		usbcam_dev->read_status = READ_STATUS_ERROR;
		usbcam_dbg("urb %p read urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->read_len = urb->actual_length;
		usbcam_dev->read_err = 0;
		usbcam_dev->read_status = READ_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	wake_up_interruptible(&usbcam_dev->read_wq);
}

static int aml_read_usb_submit(struct aml_usbcam *usbcam_dev)
{
	int pipe;
	int ret = 0;

	if (usb_endpoint_is_int_in(usbcam_dev->in)) {
		pipe = usb_rcvintpipe(usbcam_dev->usbdev, usbcam_dev->in->bEndpointAddress);
		usb_fill_int_urb(usbcam_dev->read_urb, usbcam_dev->usbdev, pipe,
						NULL, usbcam_dev->buf_size,
						aml_intf_read_complete, usbcam_dev,
						usbcam_dev->in->bInterval);
	} else if (usb_endpoint_is_bulk_in(usbcam_dev->in)) {
		pipe = usb_rcvbulkpipe(usbcam_dev->usbdev, usbcam_dev->in->bEndpointAddress);
		usb_fill_bulk_urb(usbcam_dev->read_urb, usbcam_dev->usbdev, pipe,
						NULL, usbcam_dev->buf_size,
						aml_intf_read_complete, usbcam_dev);
	}

	usbcam_dev->read_urb->transfer_dma = usbcam_dev->read_dma;
	usbcam_dev->read_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	ret = usb_submit_urb(usbcam_dev->read_urb, GFP_KERNEL);
	if (ret) {
		usbcam_dbg("submit urb %p failed, ret = %d", usbcam_dev->read_urb, ret);
		return ret;
	}

	return 0;
}

static ssize_t aml_intf_read(struct file *filp, char *buff, size_t size, loff_t *ppos)
{
	int ret = 0;
	int err_val = 0;
	ulong flag;
	size_t available, transfor, chunk;
	struct aml_usbcam *usbcam_dev = NULL;

	if (!filp || !buff || !ppos) {
		ret = -EINVAL;
		err_val = -1;
		goto error_handle;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		ret = -ENODEV;
		err_val = -2;
		goto error_handle;
	}

	if (!usbcam_dev->intf) {
		ret = -ENODEV;
		err_val = -3;
		goto error_handle;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (usbcam_dev->read_status == READ_STATUS_READY) {
		available = usbcam_dev->read_len - usbcam_dev->in_transfor;
		if (!available)
			usbcam_dev->read_status = READ_STATUS_EMPTY;
	}

	if (usbcam_dev->read_status == READ_STATUS_EMPTY ||
		usbcam_dev->read_status == READ_STATUS_WAIT_SUBMIT) {
		if (usbcam_dev->read_status == READ_STATUS_EMPTY) {
			usbcam_dev->read_status = READ_STATUS_WAIT_SUBMIT;
			spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
			ret = aml_read_usb_submit(usbcam_dev);
			if (ret) {
				usbcam_dbg("submit %p failed, ret = %d", usbcam_dev->read_urb, ret);
				err_val = -4;
				goto error_handle;
			}
		} else {
			spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
		}

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			err_val = -5;
			goto error_handle;
		}
		ret = wait_event_interruptible(usbcam_dev->read_wq,
				(usbcam_dev->read_status == READ_STATUS_READY) ||
				(usbcam_dev->read_status == READ_STATUS_ERROR) ||
				(usbcam_dev->read_open_status == READ_CLOSE));
		if (ret < 0) {
			err_val = -6;
			goto error_handle;
		}
	} else {
		spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
	}

	if (usbcam_dev->read_open_status == READ_CLOSE) {
		usbcam_dbg(" read_open_status %d\n", usbcam_dev->read_open_status);
		return 0;
	}

	if (usbcam_dev->read_status == READ_STATUS_ERROR) {
		usbcam_dev->read_status = READ_STATUS_EMPTY;
		ret = usbcam_dev->read_err;
		err_val = -7;
		goto error_handle;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	available = usbcam_dev->read_len - usbcam_dev->in_transfor;
	transfor = usbcam_dev->in_transfor;
	chunk = min(size, available);
	usbcam_dev->in_transfor += chunk;
	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	if (!usbcam_dev->read_buf) {
		ret = -EFAULT;
		err_val = -8;
		goto error_handle;
	}

	ret = copy_to_user(buff, usbcam_dev->read_buf + transfor, chunk);
	if (ret) {
		err_val = -9;
		goto error_handle;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (!(usbcam_dev->read_len - usbcam_dev->in_transfor))
		usbcam_dev->read_status = READ_STATUS_EMPTY;

	spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);

	return chunk;

error_handle:
	usbcam_dbg("intf read failed! err_val:%d ret:%d\n", err_val, ret);
	return ret;
}

static void aml_intf_write_empty_complete(struct urb *urb)
{
	int status;
	ulong flag;
	struct aml_usbcam *usbcam_dev = NULL;

	if (!urb) {
		usbcam_dbg("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (status) {
		usbcam_dev->write_len = 0;
		usbcam_dev->write_err = status;
		usbcam_dev->write_status = WRITE_STATUS_ERROR;
		usbcam_dbg("urb %p write urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->write_len = urb->actual_length;
		usbcam_dev->write_err = 0;
		usbcam_dev->write_status = WRITE_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	wake_up_interruptible(&usbcam_dev->write_wq);
}

static int aml_write_usb_submit_empty(struct aml_usbcam *usbcam_dev, size_t size)
{
	int pipe;
	int retval = 0;
	ulong flag;

	if (!usbcam_dev->intf) {
		usbcam_dbg("packet write failed,interface is deregistered\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	usbcam_dev->write_status = WRITE_STATUS_WAIT_EMPTY_DONE;
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);

	if (usb_endpoint_is_int_out(usbcam_dev->out)) {
		pipe = usb_sndintpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_int_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
			pipe, NULL, 0, aml_intf_write_empty_complete, usbcam_dev,
			usbcam_dev->out->bInterval);
	} else {
		pipe = usb_sndbulkpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_bulk_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
			pipe, NULL, 0, aml_intf_write_empty_complete, usbcam_dev);
	}

	retval = usb_submit_urb(usbcam_dev->write_urb, GFP_KERNEL);
	if (retval) {
		usbcam_dbg("submit zero-length packet urb %p failed, return = %d\n",
						usbcam_dev->write_urb, retval);
		return retval;
	}

	return 0;
}

static int aml_write_submit_empty_thread(void *data)
{
	struct aml_usbcam *usbcam_dev = data;
	size_t ret = 0;

	while (1) {
		ret = wait_event_interruptible(usbcam_dev->write_wq,
			(usbcam_dev->device_status == DEVICE_STATUS_DISCONNECT) ||
			(usbcam_dev->write_status == WRITE_STATUS_EMPTY));
		if (usbcam_dev->device_status == DEVICE_STATUS_DISCONNECT) {
			usbcam_dbg("device status is disconnect\n");
			break;
		}
		if (usbcam_dev->write_status != WRITE_STATUS_WAIT_TS_DONE &&
			usbcam_dev->write_status != WRITE_STATUS_WAIT_EMPTY_DONE) {
			aml_write_usb_submit_empty(usbcam_dev, usbcam_dev->write_size);
		} else {
			usbcam_dbg("urb already submitted,unable to repeat submit\n");
			return -EAGAIN;
		}
	}
	return ret;
}

static void aml_intf_write_complete(struct urb *urb)
{
	int status;
	ulong flag;
	struct aml_usbcam *usbcam_dev = NULL;

	if (!urb) {
		usbcam_dbg("invalid parameter\n");
		return;
	}

	status = urb->status;
	usbcam_dev = urb->context;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (status) {
		usbcam_dev->write_len = 0;
		usbcam_dev->write_err = status;
		usbcam_dev->write_status = WRITE_STATUS_ERROR;
		usbcam_dbg("urb %p write urb error, status %d\n", urb, status);
	} else {
		usbcam_dev->write_len = urb->actual_length;
		usbcam_dev->write_err = 0;

		if ((usbcam_dev->write_size % le16_to_cpu(usbcam_dev->out->wMaxPacketSize) == 0))
			usbcam_dev->write_status = WRITE_STATUS_EMPTY;
		else
			usbcam_dev->write_status = WRITE_STATUS_READY;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	wake_up_interruptible(&usbcam_dev->write_wq);
}

static int aml_write_usb_submit(struct aml_usbcam *usbcam_dev, size_t size)
{
	int pipe;
	int retval = 0;
	ulong flag;

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	usbcam_dev->write_status = WRITE_STATUS_WAIT_TS_DONE;
	usbcam_dev->write_size = size;
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
	if (usb_endpoint_is_int_out(usbcam_dev->out)) {
		pipe = usb_sndintpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_int_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
							pipe, NULL, size,
							aml_intf_write_complete, usbcam_dev,
							usbcam_dev->out->bInterval);
	} else {
		pipe = usb_sndbulkpipe(usbcam_dev->usbdev,
				usbcam_dev->out->bEndpointAddress);

		usb_fill_bulk_urb(usbcam_dev->write_urb, usbcam_dev->usbdev,
							pipe, NULL, size,
							aml_intf_write_complete, usbcam_dev);
	}

	usbcam_dev->write_urb->transfer_dma = usbcam_dev->write_dma;
	usbcam_dev->write_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	retval = usb_submit_urb(usbcam_dev->write_urb, GFP_KERNEL);
	if (retval) {
		usbcam_dbg("submit urb %p failed, return = %d\n",
						usbcam_dev->write_urb, retval);
		return retval;
	}

	return 0;
}

static ssize_t aml_intf_write(struct file *filp, const char *buff, size_t size, loff_t *ppos)
{
	int ret = 0;
	int err_val = 0;
	struct aml_usbcam *usbcam_dev = NULL;
	size_t writesize;
	ulong flag;

	if (!filp || !buff || !ppos) {
		ret = -EINVAL;
		err_val = -1;
		goto error_handle;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		ret = -ENODEV;
		err_val = -2;
		goto error_handle;
	}

	if (!usbcam_dev->intf) {
		ret = -ENODEV;
		err_val = -3;
		goto error_handle;
	}

	if (usbcam_dev->write_status != WRITE_STATUS_READY) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			err_val = -4;
			goto error_handle;
		}

		ret = wait_event_interruptible(usbcam_dev->write_wq,
				(usbcam_dev->write_status == WRITE_STATUS_READY) ||
				(usbcam_dev->write_status == WRITE_STATUS_ERROR) ||
				(usbcam_dev->write_open_status == WRITE_CLOSE));
		if (ret < 0) {
			err_val = -5;
			goto error_handle;
		}
	}

	if (usbcam_dev->write_open_status == WRITE_CLOSE) {
		ret = 0;
		err_val = -6;
		goto error_handle;
	}

	spin_lock_irqsave(&usbcam_dev->write_lock, flag);
	if (usbcam_dev->write_status == WRITE_STATUS_ERROR) {
		usbcam_dev->write_status = WRITE_STATUS_READY;
		spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);
		ret = usbcam_dev->write_err;
		err_val = -7;
		goto error_handle;
	}
	spin_unlock_irqrestore(&usbcam_dev->write_lock, flag);

	if (!usbcam_dev->write_buf) {
		ret = -EFAULT;
		err_val = -8;
		goto error_handle;
	}

	size = min(size, usbcam_dev->buf_size);
	if (copy_from_user(usbcam_dev->write_buf, buff, size)) {
		ret = -EFAULT;
		err_val = -9;
		goto error_handle;
	}

	writesize = size;

	if (usbcam_dev->write_status != WRITE_STATUS_WAIT_TS_DONE &&
		usbcam_dev->write_status != WRITE_STATUS_WAIT_EMPTY_DONE) {
		ret = aml_write_usb_submit(usbcam_dev, size);//bulk transfer
		if (ret) {
			err_val = -10;
			goto error_handle;
		}
	} else {
		ret = -EAGAIN;
		err_val = -11;
		goto error_handle;
	}

	usbcam_dbg("intf write success\n");
	return writesize;

error_handle:
	usbcam_dbg("intf write failed! err_val:%d ret:%d\n", err_val, ret);
	return ret;
}

static void aml_usbcam_delete(struct kref *ref)
{
	struct aml_usbcam *usbcam_dev = NULL;

	if (!ref) {
		usbcam_dbg("invalid parameter");
		return;
	}

	usbcam_dev = container_of(ref, struct aml_usbcam, ref_count);
	usbcam_dev->device_status = DEVICE_STATUS_DISCONNECT;

	usb_put_dev(usbcam_dev->usbdev);

	usb_kill_urb(usbcam_dev->read_urb);
	usb_kill_urb(usbcam_dev->write_urb);

	if (usbcam_dev->read_buf)
		usb_free_coherent(usbcam_dev->usbdev, usbcam_dev->buf_size,
					usbcam_dev->read_buf, usbcam_dev->read_dma);
	if (usbcam_dev->write_buf)
		usb_free_coherent(usbcam_dev->usbdev, usbcam_dev->buf_size,
					usbcam_dev->write_buf, usbcam_dev->write_dma);

	usb_free_urb(usbcam_dev->read_urb);
	usb_free_urb(usbcam_dev->write_urb);

	wake_up_interruptible(&usbcam_dev->write_wq);
	wake_up_interruptible(&usbcam_dev->read_wq);

	if (!IS_ERR_OR_NULL(usbcam_dev->thread))
		kthread_stop(usbcam_dev->thread);
	kfree(usbcam_dev);
}

static int aml_intf_release(struct inode *inode, struct file *filp)
{
	struct aml_usbcam *usbcam_dev = NULL;
	int ret;

	if (!inode || !filp) {
		ret = -EINVAL;
		goto error_handle;
	}

	usbcam_dev = filp->private_data;
	if (!usbcam_dev) {
		ret = -ENODEV;
		goto error_handle;
	}

	mutex_lock(&usbcam_dev->open_mux);

	if ((filp->f_flags & 0x03) == O_RDONLY || (filp->f_flags & 0x03) == O_RDWR) {
		usbcam_dev->read_open_status = READ_CLOSE;
		if (atomic_read(&usbcam_dev->read_urb->use_count))
			usb_unlink_urb(usbcam_dev->read_urb);

		if (usbcam_dev->read_open_ref >= 1) {
			usbcam_dev->read_open_ref--;
			usbcam_dbg("interface read fd release successfully\n");
		} else {
			usbcam_dbg("interface read fd is released before\n");
		}
	}

	if ((filp->f_flags & 0x03) == O_RDWR || (filp->f_flags & 0x03) == O_WRONLY) {
		usbcam_dev->write_open_status = WRITE_CLOSE;
		if (atomic_read(&usbcam_dev->write_urb->use_count))
			usb_unlink_urb(usbcam_dev->write_urb);

		if (usbcam_dev->write_open_ref >= 1) {
			usbcam_dev->write_open_ref--;
			usbcam_dbg("interface write fd release successfully\n");
		} else {
			usbcam_dbg("interface write fd is released before\n");
		}
	}

	mutex_unlock(&usbcam_dev->open_mux);

	wake_up_interruptible(&usbcam_dev->read_wq);
	wake_up_interruptible(&usbcam_dev->write_wq);

	usbcam_dev->write_err = 0;
	usbcam_dev->read_err = 0;

	kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);

	return 0;

error_handle:
	usbcam_dbg("intf release failed! ret:%d\n", ret);
	return ret;
}

static long aml_intf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct aml_usbcam *usbcam_dev = NULL;

	usbcam_dev = filp->private_data;

	if (!usbcam_dev) {
		usbcam_dbg("no device connected\r\n");
		ret = -ENODEV;
		return ret;
	}

	usbcam_dbg("ioctl op:\n");
	switch (cmd) {
	case AML_USBCAM_IOC_GET_DRIVER_VERSION:
		ret = aml_get_usbcam_version(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_GET_INFO:
		ret = aml_get_usbcam_info(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_RESET:
		usbcam_dbg("reset\n");
		ret = usb_lock_device_for_reset(usbcam_dev->usbdev, NULL);
		usb_reset_device(usbcam_dev->usbdev);
		usb_unlock_device(usbcam_dev->usbdev);
		break;
	case AML_USBCAM_IOC_CANCEL_TRANSFER:
		ret = aml_cancel_transfer(filp, usbcam_dev);
		break;
	case AML_USBCAM_IOC_MODULE_CAPABILITIES:
		ret = aml_get_usbcam_capabilities(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_GET_MODULE_STATE:
		ret = aml_get_usbcam_state(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_SET_MODULE_STATE:
		ret = aml_set_usbcam_state(usbcam_dev, arg);
		break;
	case AML_USBCAM_IOC_CHECK_DEVICE_PRESENCE:
		ret = aml_check_device_presence(usbcam_dev, arg);
		break;
	default:
		usbcam_dbg("unknown op type %08x", cmd);
		ret = -1;
		break;
	}

	if (ret)
		usbcam_dbg("failed! ret: %d", ret);

	return ret;
}

#ifdef CONFIG_COMPAT
static long aml_intf_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long translated_arg;

	translated_arg = (unsigned long)compat_ptr(arg);
	ret = aml_intf_ioctl(filp, cmd, translated_arg);

	return ret;
}
#endif

static __poll_t aml_intf_poll(struct file *file, struct poll_table_struct *wait)
{
	struct aml_usbcam *usbcam_dev;
	__poll_t mask = 0;
	size_t ret = 0;
	ulong flag;

	usbcam_dev = file->private_data;

	if (!usbcam_dev) {
		usbcam_dbg("invalid parameter\n");
		return -EPOLLERR;
	}

	spin_lock_irqsave(&usbcam_dev->read_lock, flag);
	if (usbcam_dev->read_status == READ_STATUS_READY) {
		if (!(usbcam_dev->read_len - usbcam_dev->in_transfor))
			usbcam_dev->read_status = READ_STATUS_EMPTY;
	}

	if (usbcam_dev->read_status == READ_STATUS_EMPTY ||
		usbcam_dev->read_status == READ_STATUS_WAIT_SUBMIT) {
		if (usbcam_dev->read_status == READ_STATUS_EMPTY) {
			usbcam_dev->read_status = READ_STATUS_WAIT_SUBMIT;
			spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
			ret = aml_read_usb_submit(usbcam_dev);
			if (ret)
				usbcam_dbg("poll submit failed");
		} else {
			spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
		}
	} else {
		spin_unlock_irqrestore(&usbcam_dev->read_lock, flag);
	}

	poll_wait(file, &usbcam_dev->read_wq, wait);
	poll_wait(file, &usbcam_dev->write_wq, wait);

	if (usbcam_dev->write_status == WRITE_STATUS_READY)
		mask |= EPOLLOUT | EPOLLWRNORM;

	if (usbcam_dev->read_status == READ_STATUS_READY)
		mask |= EPOLLIN | EPOLLRDNORM;

	if (usbcam_dev->write_status == WRITE_STATUS_ERROR ||
			usbcam_dev->read_status == READ_STATUS_ERROR)
		mask |= EPOLLERR;

	return mask;
}

static const struct file_operations aml_intf_fops = {
	.owner =		THIS_MODULE,
	.open =			aml_intf_open,
	.release =		aml_intf_release,
	.read =			aml_intf_read,
	.write =		aml_intf_write,
	.poll =			aml_intf_poll,
	//.mmap =		aml_intf_mmap,
	.unlocked_ioctl =	aml_intf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl =		aml_intf_compat_ioctl,
#endif
};

static int aml_usbcam_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	int ret = -ENOMEM;
	int extrasize = 0;
	int len, temp;
	int idx = -1;
	int i;
	int err_val = 0;
	dev_t devno;
	unsigned char intf_num;
	unsigned char descriptorlen = 0;
	unsigned char descriptortype = 0;
	unsigned char *extratemp = NULL;

	const char *dev_name_fmt;
	struct usb_device *pdev = NULL;
	struct usb_interface_assoc_descriptor *assoc_desc = NULL;
	struct aml_usbcam *usbcam_dev = NULL;
	struct aml_usbcam **dev_array;

	if (!intf || !id) {
		err_val = -1;
		ret = -EINVAL;
		goto error;
	}

	pdev = interface_to_usbdev(intf);
	intf_num = intf->cur_altsetting->desc.bInterfaceNumber;

	assoc_desc = pdev->actconfig->intf_assoc[0];
	usbcam_dbg("assoc_desc = %p\n", assoc_desc);
	if (!assoc_desc) {
		err_val = -2;
		ret = -ENODEV;
		goto error;
	}

	if (!(assoc_desc->bFunctionClass == IAD_FUNCTION_CLASS &&
		assoc_desc->bFunctionSubClass == IAD_FUNCTION_SUBCLASS &&
		assoc_desc->bFunctionProtocol == IAD_FUNCTION_PROTOCOL) &&
		!(assoc_desc->bFunctionClass == CI20_IAD_FUNCTION_CLASS &&
		assoc_desc->bFunctionSubClass == CI20_IAD_FUNCTION_SUBCLASS &&
		assoc_desc->bFunctionProtocol == CI20_IAD_FUNCTION_PROTOCOL)) {
		err_val = -3;
		ret = -ENODEV;
		goto error;
	}

	if (intf_num < assoc_desc->bFirstInterface ||
		intf_num > assoc_desc->bFirstInterface + assoc_desc->bInterfaceCount) {
		err_val = -4;
		ret = -ENODEV;
		goto error;
	}

	usbcam_dev = kzalloc(sizeof(*usbcam_dev), GFP_KERNEL);
	if (!usbcam_dev) {
		err_val = -5;
		ret = -ENOMEM;
		goto error;
	}

	if ((intf->cur_altsetting->desc.bInterfaceClass == COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == COMMAND_INTF_PROTOCOL) ||
		(intf->cur_altsetting->desc.bInterfaceClass == CI20_COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_COMMAND_INTF_PROTOCOL)) {
		usbcam_dev->device_type = DEVICE_COMMAND;

		usbcam_dev->buf_size = USBCAM_CMD_BUFFER_SIZE;
		usbcam_dev->read_buf = usb_alloc_coherent(pdev, usbcam_dev->buf_size,
							GFP_KERNEL, &usbcam_dev->read_dma);
		if (!usbcam_dev->read_buf) {
			err_val = -6;
			ret = -ENOMEM;
			goto error;
		}
		usbcam_dev->write_buf = usb_alloc_coherent(pdev, usbcam_dev->buf_size,
							GFP_KERNEL, &usbcam_dev->write_dma);
		if (!usbcam_dev->write_buf) {
			err_val = -7;
			ret = -ENOMEM;
			goto error;
		}

		if (intf->cur_altsetting->desc.bInterfaceClass == CI20_COMMAND_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_COMMAND_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_COMMAND_INTF_PROTOCOL) {
			usbcam_dev->usbcam_module_info.is_ci20_detected = TRUE;
			usbcam_dbg("is_ci20_detected = %d\n",
						usbcam_dev->usbcam_module_info.is_ci20_detected);
		}

	} else if ((intf->cur_altsetting->desc.bInterfaceClass == MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == MEDIA_INTF_PROTOCOL) ||
		(intf->cur_altsetting->desc.bInterfaceClass == CI20_MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_MEDIA_INTF_PROTOCOL)) {
		usbcam_dev->device_type = DEVICE_MEDIA;

		usbcam_dev->buf_size = USBCAM_MEDIA_BUFFER_SIZE;
		usbcam_dev->read_buf = usb_alloc_coherent(pdev, usbcam_dev->buf_size,
							GFP_KERNEL, &usbcam_dev->read_dma);
		if (!usbcam_dev->read_buf) {
			err_val = -8;
			ret = -ENOMEM;
			goto error;
		}
		usbcam_dev->write_buf = usb_alloc_coherent(pdev, usbcam_dev->buf_size,
							GFP_KERNEL, &usbcam_dev->write_dma);
		if (!usbcam_dev->write_buf) {
			err_val = -9;
			ret = -ENOMEM;
			goto error;
		}

		if (intf->cur_altsetting->desc.bInterfaceClass == CI20_MEDIA_INTF_CLASS &&
		intf->cur_altsetting->desc.bInterfaceSubClass == CI20_MEDIA_INTF_SUBCLASS &&
		intf->cur_altsetting->desc.bInterfaceProtocol == CI20_MEDIA_INTF_PROTOCOL) {
			usbcam_dev->usbcam_module_info.is_ci20_detected = TRUE;
			usbcam_dbg("is_ci20_detected = %d\n",
						usbcam_dev->usbcam_module_info.is_ci20_detected);
		}
	}

	extratemp = pdev->actconfig->extra;
	extrasize = pdev->actconfig->extralen;
	while (extrasize > 0) {
		descriptorlen = *extratemp;
		descriptortype = *(extratemp + 1);
		if (descriptorlen > extrasize || descriptorlen < 2) {
			err_val = -10;
			goto error;
		}
		if (descriptortype != 0x41 || descriptorlen != 6) {
			usbcam_dbg("extra data contains an %s descriptor of type 0x%02X",
			descriptortype == 0x0B ? "interface association" : "unexpected",
									descriptortype);
			extratemp += descriptorlen;
			extrasize -= descriptorlen;
		} else {
			//0x41: compatibility descriptor type, 6:compatibility descriptor length
			usbcam_dev->usbcam_module_info.ci_compatibility = *(extratemp + 2) << 24 |
				*(extratemp + 3) << 16 | *(extratemp + 4) << 8 | *(extratemp + 5);

			if ((usbcam_dev->usbcam_module_info.ci_compatibility & 0x07) != 2) {
				usbcam_dbg("this driver no support ARCH value:%d",
				usbcam_dev->usbcam_module_info.ci_compatibility & 0x07);
				err_val = -11;
				goto error;
			}
			break;
		}
	}

	usbcam_dev->usbdev = usb_get_dev(interface_to_usbdev(intf));
	usbcam_dev->intf = intf;

	usbcam_dbg("out endpoint is\n");
	if (usb_endpoint_dir_out(&intf->cur_altsetting->endpoint[0].desc)) {
		usbcam_dbg(" endpoint[0]!\n");
		usbcam_dev->out = &intf->cur_altsetting->endpoint[0].desc;
	} else if (usb_endpoint_dir_out(&intf->cur_altsetting->endpoint[1].desc)) {
		usbcam_dbg(" endpoint[1]!\n");
		usbcam_dev->out = &intf->cur_altsetting->endpoint[1].desc;
	} else {
		usbcam_dbg(" not found\n");
	}

	usbcam_dbg("in endpoint is\n");
	if (usb_endpoint_dir_in(&intf->cur_altsetting->endpoint[0].desc)) {
		usbcam_dbg(" endpoint[0]!\n");
		usbcam_dev->in = &intf->cur_altsetting->endpoint[0].desc;
	} else if (usb_endpoint_dir_in(&intf->cur_altsetting->endpoint[1].desc)) {
		usbcam_dbg(" endpoint[1]!\n");
		usbcam_dev->in = &intf->cur_altsetting->endpoint[1].desc;
	} else {
		usbcam_dbg(" not found\n");
	}

	usbcam_dbg("endpoint_type(in) = %d\n", usb_endpoint_type(usbcam_dev->in));
	usbcam_dbg("endpoint_type(out) = %d\n", usb_endpoint_type(usbcam_dev->out));

	usbcam_dbg("bulk_out(out) = %d\n", usb_endpoint_is_bulk_out(usbcam_dev->out));
	usbcam_dbg("int_out(out) = %d\n", usb_endpoint_is_int_out(usbcam_dev->out));

	usbcam_dbg("is_bulk_in(in) = %d\n", usb_endpoint_is_bulk_in(usbcam_dev->in));
	usbcam_dbg("is_int_in(in) = %d\n", usb_endpoint_is_int_in(usbcam_dev->in));

	if ((!usb_endpoint_is_bulk_out(usbcam_dev->out) &&
		!usb_endpoint_is_int_out(usbcam_dev->out)) ||
		(!usb_endpoint_is_bulk_in(usbcam_dev->in) &&
		!usb_endpoint_is_int_in(usbcam_dev->in))) {
		usbcam_dbg("mismatched endpoint type in interface\n");
		err_val = -12;
		goto error;
	}

	usbcam_dev->read_open_ref = 0;
	usbcam_dev->write_open_ref = 0;
	mutex_init(&usbcam_dev->open_mux);

	usbcam_dev->usbcam_module_info.vendor_id = le16_to_cpu(pdev->descriptor.idVendor);
	usbcam_dev->usbcam_module_info.product_id = le16_to_cpu(pdev->descriptor.idProduct);

	if (pdev->manufacturer) {
		usbcam_dbg("pdev->manufacturer = %s\n", pdev->manufacturer);
		temp = strlen(pdev->manufacturer);
		len = temp < AML_USBCAM_MAX_NAME_LEN ? temp : (AML_USBCAM_MAX_NAME_LEN - 1);
		strncpy(usbcam_dev->usbcam_module_capabilities.ci_manufacturer_name,
							pdev->manufacturer, len);
		usbcam_dev->usbcam_module_capabilities.ci_manufacturer_name[len] = '\0';
	} else {
		err_val = -13;
		ret = -ENOMEM;
		goto error;
	}

	if (pdev->product) {
		usbcam_dbg("pdev->product = %s\n", pdev->product);
		temp = strlen(pdev->product);
		len = temp < AML_USBCAM_MAX_NAME_LEN ? temp : (AML_USBCAM_MAX_NAME_LEN - 1);
		strncpy(usbcam_dev->usbcam_module_capabilities.ci_product_name,
								pdev->product, len);
		usbcam_dev->usbcam_module_capabilities.ci_product_name[len] = '\0';
	} else {
		err_val = -14;
		ret = -ENOMEM;
		goto error;
	}

	usbcam_dev->usbcam_module_capabilities.ci_plus_supported = TRUE;
	usbcam_dev->usbcam_module_capabilities.op_profile_supported = TRUE;

	kref_init(&usbcam_dev->ref_count);
	mutex_init(&usbcam_dev->read_mux);
	mutex_init(&usbcam_dev->write_mux);

	init_waitqueue_head(&usbcam_dev->read_wq);
	init_waitqueue_head(&usbcam_dev->write_wq);

	spin_lock_init(&usbcam_dev->read_lock);
	spin_lock_init(&usbcam_dev->write_lock);

	usbcam_dev->thread = kthread_run(aml_write_submit_empty_thread, usbcam_dev, "%s",
							"aml_write_submit_empty_thread");
	if (IS_ERR(usbcam_dev->thread)) {
		err_val = -15;
		ret = PTR_ERR(usbcam_dev->thread);
		goto error;
	}

	usbcam_dev->read_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbcam_dev->read_urb) {
		err_val = -16;
		ret = -ENOMEM;
		goto error;
	}

	usbcam_dev->write_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!usbcam_dev->write_urb) {
		err_val = -17;
		ret = -ENOMEM;
		goto error_free_read_urb;
	}

	usbcam_dev->read_status = READ_STATUS_EMPTY;
	usbcam_dev->device_status = DEVICE_STATUS_MATCH;
	usbcam_dev->device_state = DEVICE_CONNECT;

	usb_set_intfdata(intf, usbcam_dev);

	if (usbcam_dev->device_type == DEVICE_COMMAND) {
		dev_name_fmt = "cimodule_command%d";
		dev_array = command_devices;
	} else {
		dev_name_fmt = "cimodule_media%d";
		dev_array = media_devices;
	}

	mutex_lock(&ci_devices_mutex);
	for (i = 0; i < MAX_CI_DEVICES; i++) {
		if (!dev_array[i]) {
			idx = i;
			dev_array[i] = usbcam_dev;
			break;
		}
	}
	mutex_unlock(&ci_devices_mutex);

	if (idx < 0) {
		err_val = -18;
		ret = -ENOSPC;
		goto error_clear_intfdata;
	}
	usbcam_dev->dev_idx = idx;

	if (usbcam_dev->device_type == DEVICE_COMMAND)
		devno = MKDEV(MAJOR(aml_usbcam_devt), 2 * idx);
	else
		devno = MKDEV(MAJOR(aml_usbcam_devt), 2 * idx + 1);

	cdev_init(&usbcam_dev->cdev, &aml_intf_fops);
	usbcam_dev->cdev.owner = THIS_MODULE;

	ret = cdev_add(&usbcam_dev->cdev, devno, 1);
	if (ret) {
		err_val = -19;
		goto error_clear_dev_array;
	}

	usbcam_dev->dev = device_create(aml_usbcam_ci_class, &intf->dev, devno,
					NULL, dev_name_fmt, idx);
	if (IS_ERR(usbcam_dev->dev)) {
		err_val = -20;
		ret = PTR_ERR(usbcam_dev->dev);
		goto error_del_cdev;
	}

	usbcam_dbg("probe success\n");
	return 0;

error_del_cdev:
	cdev_del(&usbcam_dev->cdev);
error_clear_dev_array:
	mutex_lock(&ci_devices_mutex);
	if (usbcam_dev->device_type == DEVICE_COMMAND)
		command_devices[usbcam_dev->dev_idx] = NULL;
	else
		media_devices[usbcam_dev->dev_idx] = NULL;
	mutex_unlock(&ci_devices_mutex);
error_clear_intfdata:
	usb_set_intfdata(intf, NULL);
error_free_read_urb:
	usb_free_urb(usbcam_dev->read_urb);
	usbcam_dev->read_urb = NULL;
error:
	usbcam_dbg("probe failed! err_val:%d ret:%d\n", err_val, ret);
	if (usbcam_dev) {
		kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);
		if (usbcam_dev->read_buf)
			usb_free_coherent(usbcam_dev->usbdev, usbcam_dev->buf_size,
						  usbcam_dev->read_buf, usbcam_dev->read_dma);
		if (usbcam_dev->write_buf)
			usb_free_coherent(usbcam_dev->usbdev, usbcam_dev->buf_size,
						  usbcam_dev->write_buf, usbcam_dev->write_dma);
		kfree(usbcam_dev);
	}
	return ret;
}

static void aml_usbcam_disconnect(struct usb_interface *intf)
{
	struct aml_usbcam *usbcam_dev = NULL;
	dev_t devno;
	int idx;

	usbcam_dev = usb_get_intfdata(intf);
	if (!usbcam_dev)
		return;

	idx = usbcam_dev->dev_idx;
	if (usbcam_dev->device_type == DEVICE_COMMAND)
		devno = MKDEV(MAJOR(aml_usbcam_devt), 2 * idx);
	else
		devno = MKDEV(MAJOR(aml_usbcam_devt), 2 * idx + 1);

	device_destroy(aml_usbcam_ci_class, devno);
	cdev_del(&usbcam_dev->cdev);

	mutex_lock(&ci_devices_mutex);
	if (usbcam_dev->device_type == DEVICE_COMMAND)
		command_devices[idx] = NULL;
	else
		media_devices[idx] = NULL;
	mutex_unlock(&ci_devices_mutex);


	usb_set_intfdata(usbcam_dev->intf, NULL);
	usbcam_dev->intf = NULL;

	kref_put(&usbcam_dev->ref_count, aml_usbcam_delete);
}

static int aml_usbcam_pre_reset(struct usb_interface *intf)
{
	return 0;
}

static int aml_usbcam_post_reset(struct usb_interface *intf)
{
	return 0;
}

static int aml_usbcam_suspend(struct usb_interface *intf, pm_message_t msg)
{
	struct aml_usbcam *usbcam_dev = NULL;

	if (!intf)
		return 0;

	dev_set_uevent_suppress(&intf->dev, true);

	usbcam_dev = usb_get_intfdata(intf);
	usb_kill_urb(usbcam_dev->read_urb);
	usb_kill_urb(usbcam_dev->write_urb);

	return 0;
}

static int aml_usbcam_resume(struct usb_interface *intf)
{
	struct aml_usbcam *usbcam_dev = NULL;
	int ret;
	int retval = 0;

	if (!intf)
		return 0;
	dev_set_uevent_suppress(&intf->dev, false);

	usbcam_dev = usb_get_intfdata(intf);
	ret = usb_reset_device(usbcam_dev->usbdev);
	if (ret)
		return ret;

	if (usbcam_dev->read_open_status == READ_OPEN) {
		retval = usb_submit_urb(usbcam_dev->read_urb, GFP_KERNEL);
		if (retval)
			return retval;
	}
	if (usbcam_dev->write_open_status == WRITE_OPEN) {
		retval = usb_submit_urb(usbcam_dev->write_urb, GFP_KERNEL);
		if (retval)
			return retval;
	}

	return 0;
}

static struct usb_driver aml_usbcam_driver = {
	.name		= "aml_usbcam",
	.id_table	= aml_usbcam_table,
	.probe		= aml_usbcam_probe,
	.disconnect	= aml_usbcam_disconnect,
	.pre_reset	= aml_usbcam_pre_reset,
	.post_reset	= aml_usbcam_post_reset,
	.suspend	= aml_usbcam_suspend,
	.resume		= aml_usbcam_resume,
};

static int __init aml_usbcam_init(void)
{
	int ret;
	int err_val;

	usbcam_dbg("usbcam init!\n");

	ret = alloc_chrdev_region(&aml_usbcam_devt, 0, MAX_CI_DEVICES * 2, "aml_usbcam_ci");
	if (ret < 0) {
		err_val = -1;
		goto error_handle;
	}

	aml_usbcam_ci_class = class_create("aml_usbcam_ci");
	if (IS_ERR(aml_usbcam_ci_class)) {
		unregister_chrdev_region(aml_usbcam_devt, MAX_CI_DEVICES * 2);
		ret = PTR_ERR(aml_usbcam_ci_class);
		err_val = -2;
		goto error_handle;
	}

	return usb_register(&aml_usbcam_driver);

error_handle:
	usbcam_dbg("init failed! err_val:%d ret:%d\n", err_val, ret);
	return ret;
}

static void __exit aml_usbcam_exit(void)
{
	usbcam_dbg("usbcam exit!\n");

	class_destroy(aml_usbcam_ci_class);
	unregister_chrdev_region(aml_usbcam_devt, MAX_CI_DEVICES * 2);

	usb_deregister(&aml_usbcam_driver);
}

module_init(aml_usbcam_init);
module_exit(aml_usbcam_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AMLOGIC USB CAM MODULE DRIVER");
