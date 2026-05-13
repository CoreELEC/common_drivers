// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_notify.h>
#include "edptx_common.h"

static BLOCKING_NOTIFIER_HEAD(edptx_block_notifier_list);

int edptx_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&edptx_block_notifier_list, nb);
}
EXPORT_SYMBOL(edptx_notifier_register);

int edptx_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&edptx_block_notifier_list, nb);
}
EXPORT_SYMBOL(edptx_notifier_unregister);

int edptx_notifier_call_chain(unsigned long event, void *v)
{
	return blocking_notifier_call_chain(&edptx_block_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(edptx_notifier_call_chain);

static int edptx_backlight_off_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_BACKLIGHT_OFF))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_backlight(edp, (event & DPTX_EVENT_BACKLIGHT_OFF) ? 0 : 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_backlight_off_event_notifier_nb = {
	.notifier_call = edptx_backlight_off_notifier,
	.priority = DPTX_PRIORITY_BACKLIGHT_OFF,
};

static int edptx_backlight_on_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_BACKLIGHT_ON))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_OK;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_OK;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_backlight(edp, (event & DPTX_EVENT_BACKLIGHT_OFF) ? 0 : 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_backlight_on_event_notifier_nb = {
	.notifier_call = edptx_backlight_on_notifier,
	.priority = DPTX_PRIORITY_BACKLIGHT_ON,
};

static int edptx_mute_event_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_SCREEN_BLACK))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_screen_mute(edp, 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_mute_on_event_notifier_nb = {
	.notifier_call = edptx_mute_event_notifier,
	.priority = DPTX_PRIORITY_SCREEN_BLACK,
};

static int edptx_unmute_event_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_SCREEN_RESTORE))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_screen_mute(edp, 0);

	return NOTIFY_OK;
}

static struct notifier_block edptx_mute_off_event_notifier_nb = {
	.notifier_call = edptx_unmute_event_notifier,
	.priority = DPTX_PRIORITY_SCREEN_RESTORE,
};

static int edptx_link_on_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_LINK_ON))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_link(edp, 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_link_on_event_notifier_nb = {
	.notifier_call = edptx_link_on_notifier,
	.priority = DPTX_PRIORITY_LINK_ON,
};

static int edptx_link_off_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_LINK_OFF))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_link(edp, 0);

	return NOTIFY_OK;
}

static struct notifier_block edptx_link_off_event_notifier_nb = {
	.notifier_call = edptx_link_off_notifier,
	.priority = DPTX_PRIORITY_LINK_OFF,
};

static int edptx_driver_ready_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_TX_READY))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & DPTX_STA_PROBE_DONE))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_driver_ready(edp, 1);
	edptx_notify_set_venc(edp, 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_driver_close_event_notifier_nb = {
	.notifier_call = edptx_driver_ready_notifier,
	.priority = DPTX_PRIORITY_TX_READY,
};

static int edptx_driver_close_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_TX_CLOSE))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_driver_ready(edp, 0);
	edptx_notify_set_venc(edp, 0);

	return NOTIFY_OK;
}

static struct notifier_block edptx_driver_ready_event_notifier_nb = {
	.notifier_call = edptx_driver_close_notifier,
	.priority = DPTX_PRIORITY_TX_CLOSE,
};

static int edptx_HPD_check_event_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_CHECK))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_check_HPD(edp);

	return NOTIFY_OK;
}

static struct notifier_block edptx_HPD_check_event_notifier_nb = {
	.notifier_call = edptx_HPD_check_event_notifier,
	.priority = DPTX_PRIORITY_HPD_CHECK,
};

static int edptx_HPD_trigger_en_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_RESTORE))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_OK;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_OK;

	eDP_DBG(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_HPD_trigger(edp, 1);

	return NOTIFY_OK;
}

static struct notifier_block edptx_HPD_restore_event_notifier_nb = {
	.notifier_call = edptx_HPD_trigger_en_notifier,
	.priority = DPTX_PRIORITY_HPD_RESTORE,
};

static int edptx_HPD_trigger_ignore_notifier(struct notifier_block *nb,
					    unsigned long event, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_IGNORE))
		return NOTIFY_DONE;
	if (!edp) {
		eDP_ERR(0, LOG_E, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(edp->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	eDP_PR(edp, "%s: 0x%lx", __func__, event);

	edptx_notify_set_HPD_trigger(edp, 0);

	return NOTIFY_OK;
}

static struct notifier_block edptx_HPD_ignore_event_notifier_nb = {
	.notifier_call = edptx_HPD_trigger_ignore_notifier,
	.priority = DPTX_PRIORITY_HPD_IGNORE,
};

int edptx_notifier_init(void)
{
	int ret = 0;

	ret = edptx_notifier_register(&edptx_backlight_off_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist backlight_off_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_backlight_on_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist backlight_on_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_mute_on_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist mute_on_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_mute_off_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist mute_off_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_link_on_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist link_on_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_link_off_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist link_off_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_driver_close_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist driver_close_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_driver_ready_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist driver_ready_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_HPD_check_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist HPD_check_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_HPD_restore_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist HPD_restore_event_notifier_nb failed");
	ret = edptx_notifier_register(&edptx_HPD_ignore_event_notifier_nb);
	if (ret)
		eDP_ERR(NULL, "regist HPD_ignore_event_notifier_nb failed");
	return 0;
}

void edptx_notifier_remove(void)
{
	edptx_notifier_unregister(&edptx_backlight_off_event_notifier_nb);
	edptx_notifier_unregister(&edptx_backlight_on_event_notifier_nb);

	edptx_notifier_unregister(&edptx_mute_on_event_notifier_nb);
	edptx_notifier_unregister(&edptx_mute_off_event_notifier_nb);

	edptx_notifier_unregister(&edptx_link_on_event_notifier_nb);
	edptx_notifier_unregister(&edptx_link_off_event_notifier_nb);

	edptx_notifier_unregister(&edptx_driver_close_event_notifier_nb);
	edptx_notifier_unregister(&edptx_driver_ready_event_notifier_nb);

	edptx_notifier_unregister(&edptx_HPD_check_event_notifier_nb);

	edptx_notifier_unregister(&edptx_HPD_restore_event_notifier_nb);
	edptx_notifier_unregister(&edptx_HPD_ignore_event_notifier_nb);

	//aml_lcd_notifier_unregister(&lcd_vlock_param_nb);
}

