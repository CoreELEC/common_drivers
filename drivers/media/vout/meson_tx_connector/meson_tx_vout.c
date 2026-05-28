// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/printk.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#ifdef CONFIG_AMLOGIC_VOUT_SERVE

int meson_tx_set_current_vmode(enum vmode_e mode, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;

	if (!(mode & VMODE_INIT_BIT_MASK))
		TX_ERROR(&tx_dev->tx_log, "warning, echo /sys/class/display/mode is disabled\n");

	return 0;
}

int meson_tx_vout_set_state(int index, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;

	tx_dev->conn_dev.vout_state |= (1 << index);
	return 0;
}

int meson_tx_vout_clr_state(int index, void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;

	tx_dev->conn_dev.vout_state &= ~(1 << index);
	return 0;
}

int meson_tx_vout_get_state(void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;

	return tx_dev->conn_dev.vout_state;
}
#endif

void meson_tx_update_vinfo(struct meson_tx_dev *tx_dev)
{
	if (!tx_dev)
		return;

	//edidinfo_attach_to_vinfo(tx_dev);
	//update_vinfo_from_format_para(tx_dev);
}

void meson_tx_reset_vinfo(struct vinfo_s *tx_vinfo)
{
	tx_vinfo->name = "invalid";
	tx_vinfo->mode = VMODE_MAX;

	//edidinfo_detach_to_vinfo(tx_vinfo);
}

void meson_tx_vout_init(struct meson_tx_dev *tx_dev, struct vout_op_s *ops)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	char *vout_name;
	struct vout_server_s *vout_server;

	if (!tx_dev)
		return;

	vout_server = kzalloc(sizeof(*vout_server), GFP_KERNEL);
	if (!vout_server)
		return;

	vout_name = kzalloc(32, GFP_KERNEL);
	snprintf(vout_name, 32, "%s_vout_server", dev_name(tx_dev->pdev));

	tx_dev->conn_dev.vout_serv = vout_server;
	vout_server->connector_type = tx_dev->conn_dev.connector_type;
	vout_server->name  = vout_name;
	vout_server->data = tx_dev;
	vout_server->op.get_vinfo = ops->get_vinfo;
	vout_server->op.set_vmode = ops->set_vmode;
	vout_server->op.validate_vmode = ops->validate_vmode;
	vout_server->op.check_same_vmodeattr = ops->check_same_vmodeattr;
	vout_server->op.vmode_is_supported = ops->vmode_is_supported;
	vout_server->op.disable = ops->disable;
	vout_server->op.set_state = ops->set_state;
	vout_server->op.clr_state = ops->clr_state;
	vout_server->op.get_state = ops->get_state;
	vout_server->op.get_disp_cap = ops->get_disp_cap;
	vout_server->op.set_vframe_rate_hint = ops->set_vframe_rate_hint;
	vout_server->op.get_vframe_rate_hint = ops->get_vframe_rate_hint;
	vout_server->op.set_bist = ops->set_bist;
	vout_server->op.get_viu_mux = ops->get_viu_mux;
#ifdef CONFIG_PM
	vout_server->op.vout_suspend = ops->vout_suspend;
	vout_server->op.vout_resume = ops->vout_resume;
#endif
	vout_register_server(vout_server);
#endif
}

void meson_tx_vout_uninit(struct meson_tx_dev *tx_dev)
{
	struct vout_server_s *vout_serv = tx_dev->conn_dev.vout_serv;
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_unregister_server(vout_serv);
#endif
}

