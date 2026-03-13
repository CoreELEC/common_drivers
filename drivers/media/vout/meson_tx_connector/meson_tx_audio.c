// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/mutex.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_audio.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include "meson_tx_audio.h"

/* list of tx audio connectors */
static LIST_HEAD(tx_conn_list);
static DEFINE_MUTEX(tx_conn_mutex);

int meson_tx_audio_register(struct meson_tx_audio *tx_aud, void *inst_data, int id)
{
	struct meson_tx_audio *tx_aud0;

	mutex_lock(&tx_conn_mutex);
	list_for_each_entry(tx_aud0, &tx_conn_list, conn_list) {
		if (tx_aud0->id == tx_aud->id) {
			mutex_unlock(&tx_conn_mutex);
			return -1; /* already register */
		}
	}
	list_add_tail(&tx_aud->conn_list, &tx_aud0->conn_list);
	mutex_unlock(&tx_conn_mutex);
	return 0;
}

struct meson_tx_audio_ops *meson_tx_get_audio_ops(int id)
{
	struct meson_tx_audio *tx_aud0;

	list_for_each_entry(tx_aud0, &tx_conn_list, conn_list) {
		if (tx_aud0->id == id)
			return tx_aud0->ops;
	}
	return NULL;
}
EXPORT_SYMBOL(meson_tx_get_audio_ops);

static struct meson_tx_audio *meson_tx_get_audio_tx(int id)
{
	struct meson_tx_audio *tx_aud0;

	list_for_each_entry(tx_aud0, &tx_conn_list, conn_list) {
		if (tx_aud0->id == id)
			return tx_aud0;
	}
	return NULL;
}

int meson_tx_register_earc_hpd_cb(int id,
	void (*hpd_callback)(bool hpd, void *hpd_data), void *hpd_data)
{
	struct meson_tx_audio *tx_aud0 = meson_tx_get_audio_tx(id);

	if (!tx_aud0)
		return -1;
	tx_aud0->hpd_callback = hpd_callback;
	tx_aud0->hpd_data = hpd_data;
	return 0;
}
EXPORT_SYMBOL(meson_tx_register_earc_hpd_cb);
