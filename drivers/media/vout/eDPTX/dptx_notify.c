// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX_notify.h>
#include "dptx_common.h"

static BLOCKING_NOTIFIER_HEAD(dptx_block_notifier_list);

int dptx_notifier_register(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&dptx_block_notifier_list, nb);
}
EXPORT_SYMBOL(dptx_notifier_register);

int dptx_notifier_unregister(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&dptx_block_notifier_list, nb);
}
EXPORT_SYMBOL(dptx_notifier_unregister);

int dptx_notifier_call_chain(unsigned long event, void *v)
{
	return blocking_notifier_call_chain(&dptx_block_notifier_list, event, v);
}
EXPORT_SYMBOL_GPL(dptx_notifier_call_chain);

static int dptx_backlight_off_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_BACKLIGHT_OFF))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_backlight(dptx, (event & DPTX_EVENT_BACKLIGHT_OFF) ? 0 : 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_backlight_off_event_notifier_nb = {
	.notifier_call = dptx_backlight_off_notifier,
	.priority = DPTX_PRIORITY_BACKLIGHT_OFF,
};

static int dptx_backlight_on_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_BACKLIGHT_ON))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_OK;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_OK;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_backlight(dptx, (event & DPTX_EVENT_BACKLIGHT_OFF) ? 0 : 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_backlight_on_event_notifier_nb = {
	.notifier_call = dptx_backlight_on_notifier,
	.priority = DPTX_PRIORITY_BACKLIGHT_ON,
};

static int dptx_mute_event_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_SCREEN_BLACK))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_screen_mute(dptx, 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_mute_on_event_notifier_nb = {
	.notifier_call = dptx_mute_event_notifier,
	.priority = DPTX_PRIORITY_SCREEN_BLACK,
};

static int dptx_unmute_event_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_SCREEN_RESTORE))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_screen_mute(dptx, 0);

	return NOTIFY_OK;
}

static struct notifier_block dptx_mute_off_event_notifier_nb = {
	.notifier_call = dptx_unmute_event_notifier,
	.priority = DPTX_PRIORITY_SCREEN_RESTORE,
};

static int dptx_link_on_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_LINK_ON))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_link(dptx, 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_link_on_event_notifier_nb = {
	.notifier_call = dptx_link_on_notifier,
	.priority = DPTX_PRIORITY_LINK_ON,
};

static int dptx_link_off_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_LINK_OFF))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_link(dptx, 0);

	return NOTIFY_OK;
}

static struct notifier_block dptx_link_off_event_notifier_nb = {
	.notifier_call = dptx_link_off_notifier,
	.priority = DPTX_PRIORITY_LINK_OFF,
};

static int dptx_driver_ready_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_TX_READY))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & DPTX_STA_PROBE_DONE))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_driver_ready(dptx, 1);
	dptx_notify_set_venc(dptx, 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_driver_close_event_notifier_nb = {
	.notifier_call = dptx_driver_ready_notifier,
	.priority = DPTX_PRIORITY_TX_READY,
};

static int dptx_driver_close_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_TX_CLOSE))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_driver_ready(dptx, 0);
	dptx_notify_set_venc(dptx, 0);

	return NOTIFY_OK;
}

static struct notifier_block dptx_driver_ready_event_notifier_nb = {
	.notifier_call = dptx_driver_close_notifier,
	.priority = DPTX_PRIORITY_TX_CLOSE,
};

static int dptx_HPD_check_event_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_CHECK))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_check_HPD(dptx);

	return NOTIFY_OK;
}

static struct notifier_block dptx_HPD_check_event_notifier_nb = {
	.notifier_call = dptx_HPD_check_event_notifier,
	.priority = DPTX_PRIORITY_HPD_CHECK,
};

static int dptx_HPD_trigger_en_notifier(struct notifier_block *nb, unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_RESTORE))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(NULL, "%s: data is null", __func__);
		return NOTIFY_OK;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_OK;

	DPTX_DBG(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_HPD_trigger(dptx, 1);

	return NOTIFY_OK;
}

static struct notifier_block dptx_HPD_restore_event_notifier_nb = {
	.notifier_call = dptx_HPD_trigger_en_notifier,
	.priority = DPTX_PRIORITY_HPD_RESTORE,
};

static int dptx_HPD_trigger_ignore_notifier(struct notifier_block *nb,
					    unsigned long event, void *data)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)data;

	if (!(event & DPTX_EVENT_HPD_IGNORE))
		return NOTIFY_DONE;
	if (!dptx) {
		DPTX_ERR(0, LOG_E, "%s: data is null", __func__);
		return NOTIFY_DONE;
	}
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY)))
		return NOTIFY_DONE;

	DPTX_PR(dptx, "%s: 0x%lx", __func__, event);

	dptx_notify_set_HPD_trigger(dptx, 0);

	return NOTIFY_OK;
}

static struct notifier_block dptx_HPD_ignore_event_notifier_nb = {
	.notifier_call = dptx_HPD_trigger_ignore_notifier,
	.priority = DPTX_PRIORITY_HPD_IGNORE,
};

int dptx_notifier_init(void)
{
	int ret = 0;

	ret = dptx_notifier_register(&dptx_backlight_off_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_backlight_off_event_notifier_nb failed");
	ret = dptx_notifier_register(&dptx_backlight_on_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_backlight_on_event_notifier_nb failed\n");
	ret = dptx_notifier_register(&dptx_mute_on_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_mute_on_event_notifier_nb failed");
	ret = dptx_notifier_register(&dptx_mute_off_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_mute_off_event_notifier_nb failed\n");
	ret = dptx_notifier_register(&dptx_link_on_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_link_on_event_notifier_nb failed");
	ret = dptx_notifier_register(&dptx_link_off_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_link_off_event_notifier_nb failed\n");
	ret = dptx_notifier_register(&dptx_driver_close_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_driver_close_event_notifier_nb failed");
	ret = dptx_notifier_register(&dptx_driver_ready_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_driver_ready_event_notifier_nb failed\n");
	ret = dptx_notifier_register(&dptx_HPD_check_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_HPD_check_event_notifier_nb failed\n");
	ret = dptx_notifier_register(&dptx_HPD_restore_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_HPD_restore_event_notifier_nb failed");
	ret = dptx_notifier_register(&dptx_HPD_ignore_event_notifier_nb);
	if (ret)
		DPTX_ERR(NULL, "regist dptx_HPD_ignore_event_notifier_nb failed\n");
	return 0;
}

void dptx_notifier_remove(void)
{
	dptx_notifier_unregister(&dptx_backlight_off_event_notifier_nb);
	dptx_notifier_unregister(&dptx_backlight_on_event_notifier_nb);

	dptx_notifier_unregister(&dptx_mute_on_event_notifier_nb);
	dptx_notifier_unregister(&dptx_mute_off_event_notifier_nb);

	dptx_notifier_unregister(&dptx_link_on_event_notifier_nb);
	dptx_notifier_unregister(&dptx_link_off_event_notifier_nb);

	dptx_notifier_unregister(&dptx_driver_close_event_notifier_nb);
	dptx_notifier_unregister(&dptx_driver_ready_event_notifier_nb);

	dptx_notifier_unregister(&dptx_HPD_check_event_notifier_nb);

	dptx_notifier_unregister(&dptx_HPD_restore_event_notifier_nb);
	dptx_notifier_unregister(&dptx_HPD_ignore_event_notifier_nb);

	//aml_lcd_notifier_unregister(&lcd_vlock_param_nb);
}

