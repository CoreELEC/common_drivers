// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "meson_tx_internal.h"
#include "dptx_internal.h"

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
struct vinfo_s *dptx_get_current_vinfo(void *data)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)data;
	struct vinfo_s *vinfo = &tx_dev->tx_vinfo;

	return vinfo;
}

static enum vmode_e dptx_validate_vmode(char *_mode, unsigned int frac, void *data)
{
	//TODO
	return VMODE_MAX;
}

static int dptx_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_DisplayPort)
		return true;
	else
		return false;
}

static int dptx_module_disable(enum vmode_e cur_vmod, void *data)
{
	//TODO
	return 0;
}
#endif

void dptx_vout_init(struct dptx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	struct vout_op_s dptx_vout_ops = {
		.get_vinfo	= dptx_get_current_vinfo,
		.set_vmode	= meson_tx_set_current_vmode,
		.validate_vmode = dptx_validate_vmode,
		.check_same_vmodeattr	= NULL,
		.vmode_is_supported = dptx_vmode_is_supported,
		.disable	= dptx_module_disable,
		.set_state	= meson_tx_vout_set_state,
		.clr_state	= meson_tx_vout_clr_state,
		.get_state	= meson_tx_vout_get_state,
		.get_disp_cap	= NULL,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = NULL,
		.vout_suspend = NULL,
		.vout_resume = NULL,
	};

	/* for eDPTX, register vout server by external edptx driver */
	if (tx_comm->is_edp && tx_comm->ext_edptx_drv_en)
		return;
	meson_tx_vout_init(&tx_comm->base, &dptx_vout_ops);
#endif
}

void dptx_vout_uninit(struct dptx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	/* for eDPTX, register vout server by external edptx driver */
	if (tx_comm->is_edp && tx_comm->ext_edptx_drv_en)
		return;
	meson_tx_vout_uninit(&tx_comm->base);
#endif
}
