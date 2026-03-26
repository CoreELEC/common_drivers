// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/in.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netlink.h>
#include <linux/amlogic/aml_phy_debug.h>
#include "stmmac.h"
#include "amlogic-wol.h"

#undef pr_fmt
#define pr_fmt(fmt) "amlogic-wol: " fmt

#define PAYLOAD_DATA_SIZE		(MBOX_DATA_SIZE - 4)

#define AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT	1

#pragma pack(push, 1)
struct mbox_payload {
	u8	ctrl;				/* Controlling the transfer buffer */
	u8	type;
	u8	len;				/* Payload data size */
	u8	mode;				/* This value is always 0x80 */
	u8	data[PAYLOAD_DATA_SIZE];
};

#pragma pack(pop)

#define CTRL_FLAG_WRITE			BIT(1)	/* 0: Read  1: Write */
#define CTRL_FLAG_START			BIT(2)
#define CTRL_FLAG_APPEND		BIT(3)
#define CTRL_FLAG_STOP			BIT(4)

#define DATA_TYPE_WKUP_REASON		0x01	/* Get wakeup reason */
#define DATA_TYPE_WKUP_SRC		0x02	/* Controlling the sources that can wake up */
#define DATA_TYPE_IPV4_ADDR		0x03	/* Set IPv4 address */
#define DATA_TYPE_OFFLOADSTATE		0x04	/* For mDNS offload */
#define DATA_TYPE_RESET_ALL		0x05	/* For mDNS offload */
#define DATA_TYPE_RECORD_KEY		0x06	/* For mDNS offload */
#define DATA_TYPE_ADD_RESPONSE		0x07	/* For mDNS offload */
#define DATA_TYPE_REMOVE_RESPONSE	0x08	/* For mDNS offload */
#define DATA_TYPE_RESET_HITCOUNT	0x09	/* For mDNS offload */
#define DATA_TYPE_RESET_MISSCOUNT	0x0a	/* For mDNS offload */
#define DATA_TYPE_ADD_PASST		0x0b	/* For mDNS offload */
#define DATA_TYPE_REMOVE_PASST		0x0c	/* For mDNS offload */
#define DATA_TYPE_PASST_BEHAVIOR	0x0d	/* For mDNS offload */
#define DATA_TYPE_MDNS_FRAME_SIZE	0x0e	/* For mDNS offload */
#define DATA_TYPE_MDNS_FRAME_INFO	0x0f	/* For mDNS offload */
#define DATA_TYPE_GET_PASST_STATUS	0x10	/* For mDNS offload */
#define DATA_TYPE_IPV4_MASK		0x11	/* Set IPv4 Mask */
#define DATA_TYPE_SET_WAKEUP_PORT_NUM	0x12	/* For wakeup port */
#define DATA_TYPE_SET_WAKEUP_PORT	0x13	/* For wakeup port */

#define WKUP_REASON_UNUSED		0x00
#define WKUP_REASON_MAGIC		0x01	/* Magic packet wakeup */
#define WKUP_REASON_MDNS		0x02	/* mDNS wakeup */
#define WKUP_REASON_PORT		0x03	/* Port wakeup */

#define WKUP_VALID			BIT(0)
#define WKUP_PROTO			BIT(1)
#define WKUP_TYPE			BIT(2)

static const char * const wakeup_names[] = {
	[WKUP_REASON_UNUSED]	= "unused",
	[WKUP_REASON_MAGIC]	= "magic",
	[WKUP_REASON_MDNS]	= "mdns",
	[WKUP_REASON_PORT]	= "port"
};

#define MDNS_LIST_CRITERIA_MAX          8
#define MDNS_RAW_DATA_LENGTH_MAX        492
#define MDNS_QNAME_LENGTH_MAX           80
#define MDNS_WAKE_PORT_MAX              16
#define NETLINK_TEST                    17
#define TYPE_SET_OFFLOAD_STATE          1
#define TYPE_ENABLE_NETLINK_SERVER      2
#define TYPE_IPV4_PEER_ADDR             3
#define TYPE_ADD_RESPONSE               4
#define TYPE_REMOVE_RESPONSE            5
#define TYPE_SET_PASSTHROUGH_BEHAVIOR   6
#define TYPE_SET_WAKEUP_PORT            7

#pragma pack(push, 1)
struct match_criteria {
	u16 type;
	u16 offset;
};

#pragma pack(pop)

#pragma pack(push, 1)
struct mdns_protocol_data {
	s32 hit_counter;
	u8 valid;
	u8 list_len;
	u16 raw_len;
	u16 web_port;
	u16 srv_port;
	struct match_criteria list_criteria[MDNS_LIST_CRITERIA_MAX];
	u8 raw_offload_packet[MDNS_RAW_DATA_LENGTH_MAX];
};

#pragma pack(pop)

#pragma pack(push, 1)
struct mdns_passthrough {
	u8 valid;
	u8 qname_len;
	u8 qname[MDNS_QNAME_LENGTH_MAX];
};

#pragma pack(pop)

enum passthrough_behavior_enum {
	FORWARD_ALL,
	DROP_ALL,
	PASSTHROUGH_LIST,
};

enum wp_proto {
	PROTO_TCP = 0,
	PROTO_UDP = 1,
};

enum wp_type {
	PORT_LOCAL = 0,
	PORT_REMOTE = 1,
};

struct wake_up_port {
	u8 flags;
	u16 port;
};

static DEFINE_MUTEX(class_mutex);
static int dev_count;
#if defined(AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT)
static struct device *select_dev;
#endif /* AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT */

static int handle_offload_msg(struct amlogic_wol *wol, unsigned char *puc, const char *mode);
static u8 set_mdns_notify_bl30(struct amlogic_wol *wol, int flag);

static inline void __mbox_data_swap(struct amlogic_wol *wol, void *wdata, u32 wlen,
				    void *rdata, u32 rlen)
{
	int ret;

	ret = aml_mbox_transfer_data(wol->mbox, MBOX_CMD_SET_ETHERNET_WOL,
				     wdata, wlen, rdata, rlen, MBOX_SYNC);
	if (ret < 0)
		pr_err("mbox transfer failed\n");
}

static void __maybe_unused __mbox_notify(struct amlogic_wol *wol, u8 type)
{
	struct mbox_payload payload = {
		.mode	= 0x80,
		.ctrl	= CTRL_FLAG_START | CTRL_FLAG_STOP,
		.type	= type,
		.len	= 0,
	};

	mutex_lock(&wol->lock);
	__mbox_data_swap(wol, &payload, sizeof(payload), NULL, 0);
	mutex_unlock(&wol->lock);
}

static void __maybe_unused __mbox_data_write(struct amlogic_wol *wol, u8 type,
					     const void *data, u32 len)
{
	u32 offset = 0;
	u32 left_size;
	struct mbox_payload payload = {
		.mode	= 0x80,
		.ctrl	= CTRL_FLAG_START | CTRL_FLAG_WRITE,
		.type	= type,
	};

	mutex_lock(&wol->lock);
	while (offset < len) {
		left_size = len - offset;
		payload.len = (u8)(left_size > PAYLOAD_DATA_SIZE ?
				   PAYLOAD_DATA_SIZE : left_size);
		memcpy(payload.data, (const u8 *)data + offset, payload.len);
		payload.ctrl |= (offset + payload.len) == len ? CTRL_FLAG_STOP : CTRL_FLAG_APPEND;
		__mbox_data_swap(wol, &payload, sizeof(payload), NULL, 0);
		offset += payload.len;
		payload.ctrl = CTRL_FLAG_WRITE;
	}
	mutex_unlock(&wol->lock);
}

static void __maybe_unused __mbox_data_read(struct amlogic_wol *wol, u8 type, void *data, u32 len)
{
	u32 offset = 0;
	u32 left_size;
	struct mbox_payload payload = {
		.mode	= 0x80,
		.ctrl	= CTRL_FLAG_START,
		.type	= type,
	};

	mutex_lock(&wol->lock);
	while (offset < len) {
		left_size = len - offset;
		payload.len = (u8)(left_size > PAYLOAD_DATA_SIZE ?
				   PAYLOAD_DATA_SIZE : left_size);
		payload.ctrl |= (offset + payload.len) == len ? CTRL_FLAG_STOP : CTRL_FLAG_APPEND;
		__mbox_data_swap(wol, &payload, sizeof(payload), &payload, sizeof(payload));
		memcpy((u8 *)data + offset, payload.data, payload.len);
		offset += payload.len;
		payload.ctrl = 0;
	}
	mutex_unlock(&wol->lock);
}

static ssize_t info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct amlogic_wol *wol = dev_get_drvdata(dev);
	struct stmmac_priv *priv = container_of(wol, struct stmmac_priv, wol);
	struct net_device *ndev = priv->dev;

	return sysfs_emit(buf, "net=%s\n", netdev_name(ndev));
}
static DEVICE_ATTR_RO(info);

static ssize_t wakeup_src_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int len = 0;
	int index;
	struct amlogic_wol *wol = dev_get_drvdata(dev);

	/* Synchronize from BL30 */
	__mbox_data_read(wol, DATA_TYPE_WKUP_SRC, &wol->wakeup_src, 4);

	for (index = 1; index < ARRAY_SIZE(wakeup_names); index++) {
		if (!(wol->wakeup_src & BIT(index)))
			continue;
		len += sysfs_emit_at(buf, len, "%s,", wakeup_names[index]);
	}
	if (len > 0 && buf[len - 1] == ',')
		buf[len - 1] = '\n';

	return len;
}

static ssize_t wakeup_src_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *nbuf;
	char *p;
	char *name;
	int index;
	struct amlogic_wol *wol = dev_get_drvdata(dev);

	nbuf = kzalloc(count + 1, GFP_KERNEL);
	if (!nbuf)
		return -ENOMEM;
	p = nbuf;

	memcpy(nbuf, buf, count);
	strreplace(nbuf, '\n', '\0');

	/* clear all sources */
	wol->wakeup_src = 0;

	while (1) {
		name = strsep(&p, ",");
		if (!name || !strlen(name))
			break;

		for (index = 1; index < ARRAY_SIZE(wakeup_names); index++) {
			if (!strcmp(wakeup_names[index], name))
				break;
		}

		if (index == ARRAY_SIZE(wakeup_names))
			pr_warn("unknown wakeup source: %s\n", name);
		else
			wol->wakeup_src |= BIT(index);
	}

	kfree(nbuf);

	/* Synchronize to BL30 */
	__mbox_data_write(wol, DATA_TYPE_WKUP_SRC, &wol->wakeup_src, 4);

	return count;
}
static DEVICE_ATTR_RW(wakeup_src);

static int __get_wakeup_reason(struct amlogic_wol *wol)
{
	u8 reason = 0;

	__mbox_data_read(wol, DATA_TYPE_WKUP_REASON, &reason, 1);
	if (reason >= ARRAY_SIZE(wakeup_names)) {
		pr_err("unknown wakeup reason: 0x%02x\n", reason);
		return -EINVAL;
	}

	return reason;
}

static ssize_t wakeup_reason_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct amlogic_wol *wol = dev_get_drvdata(dev);
	int ret = __get_wakeup_reason(wol);

	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%s\n", wakeup_names[ret]);
}
static DEVICE_ATTR_RO(wakeup_reason);

static ssize_t mdnsoffload_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	(void)dev;
	(void)attr;
	(void)buf;

	return 0;
}

static ssize_t mdnsoffload_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct amlogic_wol *wol = dev_get_drvdata(dev);

	wol->mdnsoffload_result = handle_offload_msg(wol, (unsigned char *)buf, "SYSFS");
	print_hex_dump(KERN_DEBUG, "mdnsoffload: ", DUMP_PREFIX_OFFSET, 16, 1, buf, count, true);

	return count;
}
static DEVICE_ATTR_RW(mdnsoffload);

static ssize_t mdnsoffload_result_show(struct device *dev, struct device_attribute *attr,
				       char *buf)
{
	struct amlogic_wol *wol = dev_get_drvdata(dev);
	int ret = __get_wakeup_reason(wol);

	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%d\n", wol->mdnsoffload_result);
}
static DEVICE_ATTR_RO(mdnsoffload_result);

static struct attribute *wol_dev_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_wakeup_src.attr,
	&dev_attr_wakeup_reason.attr,
	&dev_attr_mdnsoffload.attr,
	&dev_attr_mdnsoffload_result.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wol_dev);

#if defined(AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT)
/* Change the selected device. */
static ssize_t cfg_select_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t len)
{
	char dev_name[64];
	struct device *dev;

	snprintf(dev_name, sizeof(dev_name), "%s", buf);

	if (!strstrip(dev_name))
		return -EINVAL;

	mutex_lock(&class_mutex);
	/* Select none */
	if (!strlen(dev_name)) {
		select_dev = NULL;
		goto out;
	}
	/* Select device by name */
	dev = class_find_device_by_name(class, dev_name);
	if (!dev) {
		mutex_unlock(&class_mutex);
		return -ENODEV;
	}
	select_dev = dev;
out:
	mutex_unlock(&class_mutex);

	return len;
}

/* Show the currently selected device. */
static ssize_t cfg_select_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	mutex_lock(&class_mutex);
	if (select_dev)
		len = sysfs_emit(buf, "%s\n", dev_name(select_dev));
	mutex_unlock(&class_mutex);

	return len;
}

/* List all available devices. */
static ssize_t cfg_list_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	struct class_dev_iter iter;
	struct device *dev;
	ssize_t len = 0;

	mutex_lock(&class_mutex);
	class_dev_iter_init(&iter, class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter)))
		len += sysfs_emit_at(buf, len, "%s\n", dev_name(dev));
	class_dev_iter_exit(&iter);
	mutex_unlock(&class_mutex);

	return len;
}

#define AML_WOL_CLASS_FUNC_STORE(_name)							\
static ssize_t __##_name##_store(const struct class *class,				\
			const struct class_attribute *attr,				\
			const char *buf, size_t len)					\
{											\
	ssize_t ret = -EACCES;								\
											\
	mutex_lock(&class_mutex);							\
	if (select_dev)									\
		ret = _name##_store(select_dev, NULL, buf, len);			\
	mutex_unlock(&class_mutex);							\
											\
	return ret;									\
}

#define AML_WOL_CLASS_FUNC_SHOW(_name)							\
static ssize_t __##_name##_show(const struct class *class,				\
			const struct class_attribute *attr, char *buf)			\
{											\
	ssize_t ret = -EACCES;								\
											\
	mutex_lock(&class_mutex);							\
	if (select_dev)									\
		ret = _name##_show(select_dev, NULL, buf);				\
	mutex_unlock(&class_mutex);							\
											\
	return ret;									\
}

#define AML_WOL_CLASS_PROXY_RW(_name)							\
	AML_WOL_CLASS_FUNC_STORE(_name)							\
	AML_WOL_CLASS_FUNC_SHOW(_name)							\
	static struct class_attribute class_attr_##_name =				\
		__ATTR(_name, 0644, __##_name##_show, __##_name##_store)

#define AML_WOL_CLASS_PROXY_RO(_name)							\
	AML_WOL_CLASS_FUNC_SHOW(_name)							\
	static struct class_attribute class_attr_##_name = {				\
		.attr = { .name = __stringify(_name), .mode = 0444 },			\
		.show = __##_name##_show,						\
	}

#define AML_WOL_CLASS_PROXY_WO(_name)							\
	AML_WOL_CLASS_FUNC_STORE(_name)							\
	static struct class_attribute class_attr_##_name = {				\
		.attr = { .name = __stringify(_name), .mode = 0200 },			\
		.store = __##_name##_store,						\
	}

static CLASS_ATTR_RW(cfg_select);
static CLASS_ATTR_RO(cfg_list);

/* Compatibility with legacy interface */
AML_WOL_CLASS_PROXY_RW(wakeup_src);
AML_WOL_CLASS_PROXY_RO(wakeup_reason);
AML_WOL_CLASS_PROXY_RW(mdnsoffload);
AML_WOL_CLASS_PROXY_RO(mdnsoffload_result);

static struct attribute *wol_class_attrs[] = {
	&class_attr_cfg_select.attr,
	&class_attr_cfg_list.attr,
	&class_attr_wakeup_src.attr,
	&class_attr_wakeup_reason.attr,
	&class_attr_mdnsoffload.attr,
	&class_attr_mdnsoffload_result.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wol_class);
#endif /* AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT */

static struct class wol_class = {
	.name =         "aml_wol",
#if defined(AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT)
	.class_groups = wol_class_groups,
#endif /* AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT */
};

static void __set_ipv4_address(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	__be32 ip, ip_mask;
	char ip_str[16];
	bool found = false;

	rcu_read_lock();
	in_dev = __in_dev_get_rcu(ndev);
	if (in_dev) {
		/* Find available IPv4 addresses */
		in_dev_for_each_ifa_rcu(ifa, in_dev) {
			ip = ifa->ifa_local;
			ip_mask = ifa->ifa_mask;
			snprintf(ip_str, sizeof(ip_str), "%pI4", &ip);
			found = true;
			break;
		}
	}
	rcu_read_unlock();

	if (found) {
		__mbox_data_write(wol, DATA_TYPE_IPV4_ADDR, &ip, 4);
		__mbox_data_write(wol, DATA_TYPE_IPV4_MASK, &ip_mask, 4);
	}
}

bool amlogic_wol_wakeup_src_not_empty(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;
	bool retval = false;
	int index;

	/* Synchronize from BL30 */
	__mbox_data_read(wol, DATA_TYPE_WKUP_SRC, &wol->wakeup_src, 4);

	for (index = 1; index < ARRAY_SIZE(wakeup_names); index++) {
		if (wol->wakeup_src & BIT(index)) {
			retval = true;
			break;
		}
	}

	return retval;
}
EXPORT_SYMBOL_GPL(amlogic_wol_wakeup_src_not_empty);

void amlogic_wol_wakeup_src_clr_all(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;

	wol->wakeup_src = 0;

	/* Synchronize to BL30 */
	__mbox_data_write(wol, DATA_TYPE_WKUP_SRC, &wol->wakeup_src, 4);
}
EXPORT_SYMBOL_GPL(amlogic_wol_wakeup_src_clr_all);

void amlogic_wol_wakeup_src_set_all(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;
	int index;

	wol->wakeup_src = 0;

	for (index = 1; index < ARRAY_SIZE(wakeup_names); index++)
		wol->wakeup_src |= BIT(index);

	/* Synchronize to BL30 */
	__mbox_data_write(wol, DATA_TYPE_WKUP_SRC, &wol->wakeup_src, 4);
}
EXPORT_SYMBOL_GPL(amlogic_wol_wakeup_src_set_all);

void amlogic_wol_enter(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;

	__set_ipv4_address(device);
	set_mdns_notify_bl30(wol, 1);
}
EXPORT_SYMBOL_GPL(amlogic_wol_enter);

bool amlogic_wol_exit(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;

	int ret = __get_wakeup_reason(wol);
	bool report_event = false;

	set_mdns_notify_bl30(wol, 0);

	if (ret < 0)
		goto out;
	pr_debug("%s wakeup\n", wakeup_names[ret]);

	switch (ret) {
	case WKUP_REASON_MAGIC:
		report_event = true;
		break;
	case WKUP_REASON_MDNS:
		report_event = false;
		break;
	case WKUP_REASON_PORT:
		report_event = false;
		break;
	}

out:
	return report_event;
}
EXPORT_SYMBOL_GPL(amlogic_wol_exit);

int amlogic_wol_setup(struct device *device, struct mbox_chan *mbox_chan)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;
	struct aml_eth_priv *eth_priv = &priv->eth_priv;
	struct device *dev;
	int ret = 0;

	mutex_lock(&class_mutex);

	/* Create a common class located in the /sys/class/aml_wol directory. */
	if (!dev_count) {
		ret = class_register(&wol_class);
		if (ret)
			goto err0;
	}
	dev_count++;

	/* Create a separate group for each MAC. */
	dev = device_create_with_groups(&wol_class, NULL /* parent dev */, MKDEV(0, 0), wol,
					wol_dev_groups, "%s", dev_name(device));
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err1;
	}
	wol->c_dev = dev;

#if defined(AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT)
	/* Set the first device as the default. */
	if (dev_count == 1)
		select_dev = dev;
#endif

	wol->dev = device;
	wol->mbox = mbox_chan;
	mutex_init(&wol->lock);

	/* Bridging up to the old sysfs control node */
	eth_priv->wol_ops.not_empty = amlogic_wol_wakeup_src_not_empty;
	eth_priv->wol_ops.clr_all = amlogic_wol_wakeup_src_clr_all;
	eth_priv->wol_ops.set_all = amlogic_wol_wakeup_src_set_all;

	mutex_unlock(&class_mutex);

	return 0;

err1:
	if (dev_count == 1) {
		class_unregister(&wol_class);
		dev_count--;
	}
err0:
	mutex_unlock(&class_mutex);

	pr_err("amlogic wol failed, ret: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_wol_setup);

void amlogic_wol_remove(struct device *device)
{
	struct net_device *ndev = dev_get_drvdata(device);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct amlogic_wol *wol = &priv->wol;
	struct aml_eth_priv *eth_priv = &priv->eth_priv;

	mutex_lock(&class_mutex);

	if (wol->c_dev) {
#if defined(AMLOGIC_WOL_LEGACY_INTERFACE_SUPPORT)
		if (select_dev == wol->c_dev)
			select_dev = NULL;
#endif
		device_unregister(wol->c_dev);
		wol->c_dev = NULL;
	}

	if (dev_count) {
		dev_count--;
		if (!dev_count)
			class_unregister(&wol_class);
	}

	memset(&eth_priv->wol_ops, 0, sizeof(eth_priv->wol_ops));
	wol->dev = NULL;
	wol->mbox = NULL;

	mutex_unlock(&class_mutex);
}
EXPORT_SYMBOL_GPL(amlogic_wol_remove);

static u8 mdns_set_offload_state(struct amlogic_wol *wol, u8 enable)
{
	u8 ret = 0;

	__mbox_data_write(wol, DATA_TYPE_OFFLOADSTATE, &enable, 1);
	__mbox_data_read(wol, DATA_TYPE_OFFLOADSTATE, &ret, 1);
	return ret;
}

static void mdns_remove_response(struct amlogic_wol *wol, int record_key)
{
	__mbox_data_write(wol, DATA_TYPE_RECORD_KEY, &record_key, 4);
	__mbox_notify(wol, DATA_TYPE_REMOVE_RESPONSE);
}

static int __maybe_unused mdns_get_reset_hitcount(struct amlogic_wol *wol, int record_key)
{
	int hit_counter = -1;

	__mbox_data_write(wol, DATA_TYPE_RECORD_KEY, &record_key, 4);
	__mbox_data_read(wol, DATA_TYPE_RESET_HITCOUNT, &hit_counter, 4);
	return hit_counter;
}

static int __maybe_unused mdns_get_reset_misscount(struct amlogic_wol *wol)
{
	int miss_counter = -1;

	__mbox_data_read(wol, DATA_TYPE_RESET_MISSCOUNT, &miss_counter, 4);
	return miss_counter;
}

static void __maybe_unused mdns_set_passthrough_behavior(struct amlogic_wol *wol, u8 behavior)
{
	__mbox_data_write(wol, DATA_TYPE_PASST_BEHAVIOR, &behavior, 1);
}

static void __maybe_unused mdns_reset_all(struct amlogic_wol *wol)
{
	__mbox_notify(wol, DATA_TYPE_RESET_ALL);
}

static int mdns_get_mdns_frame(struct amlogic_wol *wol, u8 *buf, u16 buf_len)
{
	u16 frame_size = 0;

	__mbox_data_read(wol, DATA_TYPE_MDNS_FRAME_SIZE, &frame_size, 1);
	__mbox_data_read(wol, DATA_TYPE_MDNS_FRAME_INFO, buf,
			 buf_len > frame_size ? frame_size : buf_len);

	return frame_size;
}

static int mdns_add_response(struct amlogic_wol *wol, u8 *offload_data, u16 offload_size,
			      struct match_criteria *criteria_data,
			      u8 criteria_size, u16 web_port, u16 srv_port)
{
	int ret = -1;
	struct mdns_protocol_data protocol_data;

	protocol_data.hit_counter = 0;
	protocol_data.valid = true;
	protocol_data.list_len = criteria_size;
	protocol_data.raw_len = offload_size;
	protocol_data.web_port = web_port;
	protocol_data.srv_port = srv_port;
	memcpy(protocol_data.list_criteria, criteria_data,
		protocol_data.list_len * sizeof(struct match_criteria));
	memcpy(protocol_data.raw_offload_packet, offload_data, protocol_data.raw_len);

	__mbox_data_write(wol, DATA_TYPE_ADD_RESPONSE, &protocol_data, sizeof(protocol_data));
	__mbox_data_read(wol, DATA_TYPE_RECORD_KEY, &ret, 4);
	return ret;
}

static u8 __maybe_unused mdns_add_passthrough(struct amlogic_wol *wol, u8 *qname, u8 len)
{
	u8 ret = 0;
	struct mdns_passthrough passthrough_data;

	passthrough_data.valid = true;
	passthrough_data.qname_len = len;
	memcpy(passthrough_data.qname, qname, passthrough_data.qname_len);

	__mbox_data_write(wol, DATA_TYPE_ADD_PASST, &passthrough_data, sizeof(passthrough_data));
	__mbox_data_read(wol, DATA_TYPE_GET_PASST_STATUS, &ret, 1);
	return ret;
}

static void __maybe_unused mdns_remove_passthrough(struct amlogic_wol *wol, u8 *qname, u8 len)
{
	struct mdns_passthrough passthrough_data;

	passthrough_data.valid = true;
	passthrough_data.qname_len = len;
	memcpy(passthrough_data.qname, qname, passthrough_data.qname_len);

	__mbox_data_write(wol, DATA_TYPE_REMOVE_PASST, &passthrough_data,
			  sizeof(passthrough_data));
}

static void __maybe_unused mdns_set_wakeup_port(struct amlogic_wol *wol, struct wake_up_port *wp,
						u32 port_num)
{
	__mbox_data_write(wol, DATA_TYPE_SET_WAKEUP_PORT_NUM, &port_num, 1);
	__mbox_data_write(wol, DATA_TYPE_SET_WAKEUP_PORT, wp,
			  sizeof(struct wake_up_port) * port_num);
}

static void mdns_netlink_recv_msg(struct sk_buff *skb)
{
	struct sock *sk = skb->sk;
	struct amlogic_wol *wol = (struct amlogic_wol *)sk->sk_user_data;
	struct sk_buff *skb_out;
	struct nlmsghdr *nlh;
	int msg_size;
	unsigned char *msg;
	unsigned char *puc;
	int len, type;
	char outmsg[32] = "Done!";
	int outmsg_size = strlen(outmsg);
	int pid;
	int res;

	if (!wol) {
		pr_err("netlink recv failed, sk_user_data is null\n");
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	pid = nlh->nlmsg_pid; /* pid of sending process */
	msg = (unsigned char *)nlmsg_data(nlh);
	puc = msg;
	type = *((int *)puc);
	len = *((int *)(puc + 4));

	msg_size = strlen(msg);

	// create reply
	skb_out = nlmsg_new(msg_size, 0);
	if (!skb_out) {
		pr_err("%s: Failed to allocate new skb\n", __func__);
		return;
	}

	// put received message into reply
	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
	if (!nlh) {
		pr_err("%s: Failed to put nlmsg\n", __func__);
		kfree_skb(skb_out);
		return;
	}
	NETLINK_CB(skb_out).dst_group = 0; /* not in mcast group */

	res = handle_offload_msg(wol, puc, "NETLINK");
	snprintf(outmsg, sizeof(outmsg), (res < 0) ? "Unknown message" : "Done: %d", res);
	outmsg_size = strlen(outmsg);
	strncpy(nlmsg_data(nlh), outmsg, outmsg_size);

	res = nlmsg_unicast(wol->sk, skb_out, pid);
	if (res < 0)
		pr_err("%s: Error while sending skb to user\n", __func__);
}

static int mdns_netlink_init(struct amlogic_wol *wol)
{
	struct netlink_kernel_cfg cfg = {
		.input = mdns_netlink_recv_msg,
	};
	if (wol->sk)
		return 0;

	wol->sk = netlink_kernel_create(&init_net, NETLINK_TEST, &cfg);
	if (!wol->sk) {
		pr_err("%s: Error creating socket.\n", __func__);
		return -10;
	}
	wol->sk->sk_user_data = wol;

	return 0;
}

static int handle_offload_msg(struct amlogic_wol *wol, unsigned char *puc, const char *mode)
{
	int len, type;

	type = *((int *)puc);
	len = *((int *)(puc + 4));

#ifdef CONFIG_MDNS_DEBUG
	print_hex_dump(KERN_DEBUG, "OFFLOAD-MSG: ", DUMP_PREFIX_OFFSET, 16, 1, puc + 8, len, true);
#endif

	if (type == TYPE_SET_OFFLOAD_STATE && len == 1) {
		return set_mdns_notify_bl30(wol, puc[8]);
	} else if (type == TYPE_ENABLE_NETLINK_SERVER && len == 1) {
		if (puc[8] == 1)
			mdns_netlink_init(wol);
	} else if (type == TYPE_ADD_RESPONSE) {
		int response_len = *((int *)(puc + 8));
		unsigned char *response;
		unsigned char *q = puc + 12;
		int i;
		int numberOfCriteria;
		struct match_criteria *list;
		unsigned short web_port, srv_port;

		response = q;
		q += response_len;
		numberOfCriteria = *((int *)q);
		q += 4;
		list = (struct match_criteria *)q;
		for (i = 0; i < numberOfCriteria; i++)
			q += sizeof(struct match_criteria);

		web_port = *((unsigned short *)q);
		q += 2;
		srv_port = *((unsigned short *)q);

		return mdns_add_response(wol, response, response_len,
			list, numberOfCriteria, web_port, srv_port);
	} else if (type == TYPE_REMOVE_RESPONSE && len == 1) {
		unsigned char val = puc[8];

		mdns_remove_response(wol, val);
	} else if (type == TYPE_SET_PASSTHROUGH_BEHAVIOR && len == 1) {
		mdns_set_passthrough_behavior(wol, puc[8]);
	} else if (type == TYPE_SET_WAKEUP_PORT) {
		struct wake_up_port *wp = NULL;
		int i, port_num;
		unsigned char *q = puc + 12;

		port_num = *((u32 *)(puc + 8));
		if (port_num <= 0 || port_num > MDNS_WAKE_PORT_MAX) {
			pr_err("%s: Invalid port number: %d\n", __func__, port_num);
			return -1;
		}
		wp = kcalloc(port_num, sizeof(struct wake_up_port), GFP_KERNEL);
		if (!wp)
			return -ENOMEM;

		for (i = 0; i < port_num; i++) {
			unsigned char protocol, matcher;
			unsigned short port;

			protocol = *((unsigned char *)q);
			q += 1;
			matcher = *((unsigned char *)q);
			q += 1;
			port = *((unsigned short *)q);
			q += 2;
			wp[i].flags = WKUP_VALID;
			if (protocol == PROTO_UDP)
				wp[i].flags |= WKUP_PROTO;
			if (matcher == PORT_REMOTE)
				wp[i].flags |= WKUP_TYPE;
			wp[i].port = port;
		}
		mdns_set_wakeup_port(wol, wp, port_num);
		kfree(wp);
	} else {
		pr_err("%s: unsupported type: %d\n", mode, type);
		return -1;
	}

	return 0;
}

static u8 set_mdns_notify_bl30(struct amlogic_wol *wol, int flag)
{
	int ret;
	u8 set_ret;
	int miss_counter;
	int hit_counter;
	int record_key = 0;
	u8 behavior = PASSTHROUGH_LIST;
	char buf[1024] = { 0 };

	if (flag) {
		mdns_set_passthrough_behavior(wol, behavior);
		miss_counter = mdns_get_reset_misscount(wol);
		hit_counter = mdns_get_reset_hitcount(wol, record_key);
		set_ret = mdns_set_offload_state(wol, 1);
	} else {
		set_ret = mdns_set_offload_state(wol, 0);
		ret = mdns_get_mdns_frame(wol, buf, ARRAY_SIZE(buf));
		print_hex_dump(KERN_DEBUG, "MDNS frame: ",
			DUMP_PREFIX_OFFSET, 16, 1, buf, ret, true);
	}

	return set_ret;
}
