// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/memblock.h>
#include <linux/compat.h>
#include <linux/amlogic/gki_module.h>
#include <linux/arm-smccc.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/amlogic/uboot_env.h>
#include <linux/amlogic/secmon.h>

static dev_t env_devno;
static struct cdev *env_cdev;

static int env_encrypt(unsigned int size, unsigned int flags)
{
	struct arm_smccc_res res;

	arm_smccc_smc(SECURITY_ENV_ENCRYPT, size, flags, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static int env_open(struct inode *i, struct file *f)
{
	return 0;
}

static int env_release(struct inode *i, struct file *f)
{
	return 0;
}

static long env_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	void *shmem_in;
	void *shmem_out;
	unsigned char *p;
	int size, off = 0, ret;
	env_ioc_args_t eargs = {};
	int data_size;
	unsigned int flags = ENV_DATA_FIRST;
	unsigned int magic = AMLOGIC_ENV_ENCRYPT_MAGIC;
	unsigned char data_magic[4] = {};

	ret = copy_from_user(&eargs, (void *)arg, sizeof(eargs));
	if (ret < 0) {
		pr_debug("%s, getargs failed:%d, arg:%lx\n", __func__, ret, arg);
		return -EIO;
	}

	pr_debug("cmd:%x, ENV IOC args, size:%d, encrypt:%d, data:%llx",
		cmd, eargs.size, eargs.encrypt, eargs.data);

	switch (cmd) {
	case ENV_IOC_ENCRYPT:
		if (eargs.encrypt > ENV_ENCRYPT || !eargs.data || eargs.size > SZ_16M) {
			pr_err("%s, wrong env args\n", __func__);
			return -EINVAL;
		}

		shmem_in      = get_meson_sm_input_base();
		shmem_out     = get_meson_sm_output_base();
		data_size     = eargs.size - sizeof(magic);
		p             = (unsigned char *)((unsigned long)eargs.data);

		ret = copy_from_user(data_magic, p + data_size, sizeof(magic));
		if (ret < 0)
			return -EIO;

		/* check if data is encrypted */
		if (eargs.encrypt == ENV_DECRYPT && memcmp(data_magic, &magic, sizeof(magic))) {
			pr_err("%s, decrypt magic[%02x %02x %02x %02x] not match\n",
				__func__, data_magic[0], data_magic[1],
				data_magic[2], data_magic[3]);
			return -EINVAL;
		}
		while (off < data_size) {
			if (data_size - off >= PAGE_SIZE)
				size = PAGE_SIZE;
			else
				size = data_size - off;

			if (off + size == data_size)
				flags |= ENV_DATA_LAST;

			pr_debug("%s, off:%08x, size:%x, enc:%d, flag:%x\n",
				__func__, off, size, eargs.encrypt, flags);
			meson_sm_mutex_lock();
			ret = copy_from_user(shmem_in, p + off, size);
			if (ret < 0) {
				pr_debug("%s, get data failed:%d\n", __func__, ret);
				goto error;
			}
			ret = env_encrypt(size, eargs.encrypt | flags);
			if (ret) {
				pr_err("%s, env encrypt failed, off:%08x, size:%x, enc:%d, flag:%x, ret:%d\n",
					__func__, off, size, eargs.encrypt, flags, ret);
				goto error;
			}
			ret = copy_to_user(p + off, shmem_out, size);
			if (ret < 0) {
				pr_debug("%s, put data failed:%d\n", __func__, ret);
				goto error;
			}
			meson_sm_mutex_unlock();
			off += size;
			flags = 0;
		}
		/* padding magic */
		ret = copy_to_user(p + data_size, &magic, sizeof(magic));
		if (ret < 0)
			return ret;
		break;
	default:
		break;
	}

	return 0;
error:
	meson_sm_mutex_unlock();
	return -EIO;
}

#ifdef CONFIG_COMPAT
static long env_compact_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	arg = (ulong)compat_ptr(arg);
	return env_ioctl(f, cmd, arg);
}
#endif

const struct file_operations env_fops = {
	.owner		= THIS_MODULE,
	.open		= env_open,
	.release	= env_release,
	.unlocked_ioctl = env_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= env_compact_ioctl,
#endif
};

static struct class env_class = {
	.name = "env",
};

int __init env_init_module(void)
{
	int r;
	struct device *dev;

	r = class_register(&env_class);
	if (r)
		return -ENODEV;

	r = alloc_chrdev_region(&env_devno, 0, 1, "env_enc");
	if (r < 0)
		return -ENODEV;
	pr_info("%s, dev no:%d\n", __func__, env_devno);
	env_cdev = kzalloc(sizeof(*env_cdev), GFP_KERNEL);
	if (!env_cdev)
		return -ENOMEM;

	cdev_init(env_cdev, &env_fops);
	env_cdev->owner = THIS_MODULE;
	r = cdev_add(env_cdev, env_devno, 1);
	if (r)
		return -ENODEV;

	dev = device_create(&env_class, NULL, MKDEV(MAJOR(env_devno), 0), NULL, "env_enc");
	if (IS_ERR_OR_NULL(dev))
		return -EINVAL;

	return 0;
}

void __exit env_exit_module(void)
{
	if (env_cdev)
		cdev_del(env_cdev);
	if (env_devno)
		unregister_chrdev_region(env_devno, 1);
	kfree(env_cdev);
}
module_init(env_init_module);
module_exit(env_exit_module);
MODULE_AUTHOR("tao.zeng@amlogic.com>");
MODULE_LICENSE("GPL");

