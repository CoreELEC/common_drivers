// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/component.h>
#include <linux/vmalloc.h>
#include <linux/of_address.h>
#include <linux/notifier.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>
#include <drm/amlogic/meson_drm_bind.h>

#include "meson_tx_internal.h"
#include "dptx_log.h"
#include "dptx_internal.h"
#include "dptx_hw_opcode.h"
#include "dp20/dptx_hdcp_hw.h"
#include "meson_tx_hw_task.h"

#define DEVICE_NAME_DP "dptx"
#define DPTX_DEV_COUNT 2
struct dptx_common *global_tx_comm[DPTX_DEV_COUNT] = {NULL};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void dptx_early_suspend(struct early_suspend *es)
{
	struct dptx_common *tx_comm = NULL;
	u8 en = 0;

	if (!es)
		return;

	tx_comm = es->param;
	DPTX_DEBUG("early suspend %s\n",
		tx_comm && tx_comm->base.dev ? dev_name(tx_comm->base.dev) : "");
	/* disable phy and core output */
	meson_tx_phy_power_off(tx_comm->base.tx_hw_base->tx_phy);
	if (tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable)
		tx_comm->hdcptx_comm.hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
	/*
	 * disable the input/output of vc_id channel
	 * TODO: for MST, need disable all channel
	 */
	en = (0x0 << 0) | (tx_comm->base.sw_fmt_para.vc_id << 4);
	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
}

static void dptx_late_resume(struct early_suspend *es)
{
	struct dptx_common *tx_comm = NULL;

	if (!es)
		return;

	tx_comm = es->param;
	DPTX_DEBUG("late resume %s\n",
		tx_comm && tx_comm->base.dev ? dev_name(tx_comm->base.dev) : "");
}
#endif

#ifdef CONFIG_PM
static int dptx_pm_suspend(struct device *dev)
{
	if (!dev)
		return -EINVAL;

	DPTX_DEBUG("suspend %s\n", dev_name(dev));

	return 0;
}

static int dptx_pm_resume(struct device *dev)
{
	if (!dev)
		return -EINVAL;

	DPTX_DEBUG("resume %s\n", dev_name(dev));

	return 0;
}

static int dptx_pm_restore(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops dptx_pm = {
	.suspend	= dptx_pm_suspend,
	.resume		= dptx_pm_resume,
	/* no application for now */
	.restore    = dptx_pm_restore,
};
#endif

static int dptx_bind_drm(struct device *dev, struct device *master,
			   void *data)
{
	int drm_dptx_id, type;
	struct dptx_common *tx_common = dev_get_drvdata(dev);
	struct meson_drm_bound_data *bound_data = data;

	/* external edptx driver enabled, use external edptx driver bind ops */
	if (tx_common->is_edp && tx_common->ext_edptx_drv_en)
		return -1;

	type = (tx_common->is_edp ? DRM_MODE_CONNECTOR_MESON_TX_EDP_A :
		DRM_MODE_CONNECTOR_MESON_TX_DP_A)
		+ tx_common->enc_idx;

	if (bound_data->connector_component_bind) {
		drm_dptx_id = bound_data->connector_component_bind
			(bound_data->drm, type,
			&tx_common->base.conn_dev);
		DPTX_INFO("%s dp [%d]\n", __func__, drm_dptx_id);
		tx_common->drm_dptx_id = drm_dptx_id;
	} else {
		DPTX_INFO("no bind func from drm.\n");
	}

	return 0;
}

static void dptx_unbind_drm(struct device *dev, struct device *master,
			      void *data)
{
	int type;
	struct dptx_common *tx_common = dev_get_drvdata(dev);
	struct meson_drm_bound_data *bound_data = data;

	/* external edptx driver enabled, use external edptx driver unbind ops */
	if (tx_common->is_edp && tx_common->ext_edptx_drv_en)
		return;

	type = tx_common->is_edp ? DRM_MODE_CONNECTOR_eDP : DRM_MODE_CONNECTOR_DisplayPort;
	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm, type,
			&tx_common->base.conn_dev);
		DPTX_INFO("%s success\n", __func__);
	} else {
		DPTX_INFO("no unbind func.\n");
	}
}

/*drm component bind ops*/
static const struct component_ops dptx_bind_ops = {
	.bind	= dptx_bind_drm,
	.unbind	= dptx_unbind_drm,
};

static int dptx_get_dt_info(struct platform_device *pdev,
	struct dptx_common *tx_comm, struct dptx_hw_common *hw_comm)
{
	int ret;
	u32 dev_index = 0;
	u32 irq_index;
	u32 val = 0;
	u32 i;
	struct resource res;
	struct device_node *np = pdev->dev.of_node;
	struct dptx_cap *tx_cap = NULL;

	if (!tx_comm || !hw_comm)
		return -1;

	if (!pdev->dev.of_node)
		return -1;
	ret = of_property_read_u32(pdev->dev.of_node, "dev_index", &dev_index);
	if (ret) {
		DPTX_INFO("%s: no index exist, default to 0\n", __func__);
		tx_comm->dev_idx = 0;
	} else {
		if (dev_index >= DPTX_DEV_COUNT) {
			DPTX_ERROR("%s: index %d exceed maximum count\n", __func__, dev_index);
			return -1;
		}

		tx_comm->dev_idx = dev_index;
	}
	hw_comm->dev_idx = tx_comm->dev_idx;

	/* get tx capability from dts */
	tx_cap = kzalloc(sizeof(*tx_cap), GFP_KERNEL);
	if (!tx_cap)
		return -ENOMEM;
	tx_comm->base.tx_cap = tx_cap;
	ret = of_property_read_u32(pdev->dev.of_node, "max_link_rate", &val);
	if (ret) {
		DPTX_INFO("%s: no max_link_rate, default to 5400000\n", __func__);
		tx_cap->max_link_rate = 54000000;
	} else {
		tx_cap->max_link_rate = val;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "max_lane_count", &val);
	if (ret) {
		DPTX_INFO("%s: no max_lane_count, default to 4\n", __func__);
		tx_cap->max_lane_count = 4;
	} else {
		tx_cap->max_lane_count = val;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "max_fresh_rate", &val);
	if (ret) {
		DPTX_INFO("%s: no max_fresh_rate, default to 60\n", __func__);
		tx_cap->max_fresh_rate = 60;
	} else {
		tx_cap->max_fresh_rate = val;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "max_h_active", &val);
	if (ret) {
		DPTX_INFO("%s: no max_h_active, default to 3840\n", __func__);
		tx_cap->max_h_active = 3840;
	} else {
		tx_cap->max_h_active = val;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "max_v_active", &val);
	if (ret) {
		DPTX_INFO("%s: no max_v_active, default to 2160\n", __func__);
		tx_cap->max_v_active = 2160;
	} else {
		tx_cap->max_v_active = val;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "pxp_mode", &val);
	if (ret)
		tx_comm->base.pxp_mode = 0;
	else
		tx_comm->base.pxp_mode = !!val;
	hw_comm->hw_base.pxp_mode = tx_comm->base.pxp_mode;
	DPTX_INFO("%s: pxp_mode:%d\n", __func__, tx_comm->base.pxp_mode);

	/* edptx core control by external edptx drv by default */
	ret = of_property_read_u32(pdev->dev.of_node, "ext_edptx_en", &val);
	if (ret)
		tx_comm->ext_edptx_drv_en = 1;
	else
		tx_comm->ext_edptx_drv_en = !!val;

	ret = of_property_read_u32(pdev->dev.of_node, "enc_idx", &val);
	if (!ret && val == 2)
		tx_comm->enc_idx = 2;
	else if (!ret && val == 1)
		tx_comm->enc_idx = 1;
	else
		tx_comm->enc_idx = 0;
	DPTX_INFO("enc_idx %d\n", tx_comm->enc_idx);

	hw_comm->hw_base.regs_region = kzalloc(sizeof(*hw_comm->hw_base.regs_region) * REG_IDX_MAX,
		GFP_KERNEL);
	if (!hw_comm->hw_base.regs_region) {
		DPTX_ERROR("cannot alloc memory for regs_region\n");
		ret = -ENOMEM;
		goto regs_region_alloc_fail;
	}
	/* get the core, vpu, ana, ... etc address from dts */
	for (i = 0; i < REG_IDX_MAX; i++) {
		if (of_address_to_resource(np, i, &res)) {
			DPTX_ERROR("not get regbase index %d\n", i);
			ret = -ENODEV;
			goto regs_region_alloc_fail;
		}
		hw_comm->hw_base.regs_region[i].phy_addr = res.start;
		hw_comm->hw_base.regs_region[i].size = resource_size(&res);
		hw_comm->hw_base.regs_region[i].p = devm_ioremap(&pdev->dev, res.start,
					     resource_size(&res));
		if (IS_ERR(hw_comm->hw_base.regs_region[i].p)) {
			ret = -ENOMEM;
			goto regs_region_alloc_fail;
		}

		DPTX_DEBUG("Mapped Addr: 0x%x\n", res.start);
	}

	if (tx_comm->is_edp == 0) {
		irq_index = platform_get_irq_byname(pdev, "apb_dp_int");
		if (irq_index == -ENXIO) {
			DPTX_ERROR("%s: apb_dp_int not found\n", __func__);
			ret = -ENXIO;
			goto regs_region_alloc_fail;
		}
		hw_comm->dptx_irq_id = irq_index;
		irq_index = platform_get_irq_byname(pdev, "hdcp2tx_intr");
		if (irq_index == -ENXIO) {
			DPTX_ERROR("%s: hdcp2tx_intr not found\n", __func__);
			ret = -ENXIO;
			// TODO temp mark, otherwise no /sys/class/drm/[CARD]
			//goto regs_region_alloc_fail;
		}
		hw_comm->hdcp2tx_irq_id = irq_index;
	} else {
		irq_index = platform_get_irq_byname(pdev, "apb_int_edptx");
		if (irq_index == -ENXIO) {
			DPTX_ERROR("%s: apb_int_edptx not found\n", __func__);
			ret = -ENXIO;
			goto regs_region_alloc_fail;
		}
		hw_comm->dptx_irq_id = irq_index;
	}
	return 0;

regs_region_alloc_fail:
	kfree(tx_cap);

	return ret;
}

static struct meson_tx_phy *meson_tx_probe_phy(struct device *dev, struct meson_tx_hw *tx_hw)
{
	struct platform_device *phy_pdev;
	struct device_node *phy_node;
	struct meson_tx_phy *tx_phy = NULL;

	phy_node = of_parse_phandle(dev->of_node, "tx_phy", 0);
	if (!phy_node) {
		dev_err(dev, "cannot find phy device\n");
		return NULL;
	}

	phy_pdev = of_find_device_by_node(phy_node);
	if (phy_pdev)
		tx_phy = platform_get_drvdata(phy_pdev);

	of_node_put(phy_node);

	if (!phy_pdev) {
		dev_err(dev, "%s: phy driver is not ready\n", __func__);
		return NULL;
	}
	if (!tx_phy) {
		put_device(&phy_pdev->dev);
		dev_err(dev, "%s: phy driver is not ready\n", __func__);
		return NULL;
	}

	return tx_phy;
}

static struct meson_tx_clk *meson_tx_probe_clk(struct device *dev)
{
	struct platform_device *clk_pdev;
	struct device_node *clk_node;
	struct meson_tx_clk *tx_clk = NULL;

	clk_node = of_parse_phandle(dev->of_node, "tx_clk", 0);
	if (!clk_node) {
		dev_err(dev, "cannot find tx_clk node\n");
		return NULL;
	}

	clk_pdev = of_find_device_by_node(clk_node);
	if (clk_pdev)
		tx_clk = platform_get_drvdata(clk_pdev);
	of_node_put(clk_node);

	if (!clk_pdev) {
		dev_err(dev, "%s: tx_clk driver is not ready\n", __func__);
		return NULL;
	}
	if (!tx_clk) {
		put_device(&clk_pdev->dev);
		dev_err(dev, "%s: tx_clk driver is not ready\n", __func__);
		return NULL;
	}

	return tx_clk;
}

static void dptx_hdcp_init(struct dptx_common *tx_comm)
{
	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_HDCP_INIT, tx_comm, NULL);
}

static void dptx_hdcp_uninit(struct dptx_common *tx_comm)
{
	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_HDCP_UNINIT, tx_comm, NULL);
}

static int dpphy_to_core_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct dptx_common *tx_comm = container_of(nb, struct dptx_common, dpphy_to_core_nb);
	struct meson_hw_event_ops *event_ops = tx_comm->hw_comm->hw_base.event_ops;
	u32 args = 0;
	u32 flip = 0;
	u32 pin_state = 0;
	struct typec_dp_state usb_dp_state = {0};

	switch (action) {
	case MESON_TX_CONN_DPPHY_HPD_PLUGIN:
		if (tx_comm->hw_comm->hpd_lock) {
			DPTX_INFO("hpd locked\n");
			return NOTIFY_OK;
		}
		/* HPD rising */
		event_ops->queue_event(event_ops->data, HPD_PLUGIN,
			tx_comm->hw_comm->hpd_in_filter_ms);
		break;
	case MESON_TX_CONN_DPPHY_HPD_PLUGOUT:
		if (tx_comm->hw_comm->hpd_lock) {
			DPTX_INFO("hpd locked\n");
			return NOTIFY_OK;
		}
		/* HPD falling */
		event_ops->cancel_event(event_ops->data, HPD_PLUGIN, false);
		event_ops->queue_event(event_ops->data, HPD_PLUGOUT, 0);
		break;
	case MESON_TX_CONN_DPPHY_HPD_IRQ:
		event_ops->queue_event(event_ops->data, IRQ_HPD_TASK, 0);
		break;
	case MESON_TX_CONN_DPPHY_LANE_REMAP:
		usb_dp_state = *(struct typec_dp_state *)data;
		if (usb_dp_state.flip == TYPEC_ORIENTATION_REVERSE)
			flip = 1;
		if (usb_dp_state.pin_state == TYPEC_DP_STATE_E)
			pin_state = 2;
		args = (flip << 8) | (pin_state & 0xf);
		dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_LINK_LANE_REMAP, (void *)&args, NULL);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int dptx_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct dptx_common *tx_comm;
	struct meson_tx_hw *tx_hw;
	const struct meson_tx_plat_data *plat_data;
	struct device *device = &pdev->dev;

	struct meson_tx_phy *tx_phy;
	struct meson_tx_clk *tx_clk;

	/* get config from device tree match */
	plat_data = of_device_get_match_data(&pdev->dev);
	if (!plat_data)
		return -ENOMEM;

	tx_hw = plat_data->alloc_tx_hw();
	if (!tx_hw)
		return -ENOMEM;

	tx_comm = kzalloc(sizeof(*tx_comm), GFP_KERNEL);
	if (!tx_comm) {
		ret = -ENOMEM;
		goto dptx_common_alloc_fail;
	}
	tx_comm->is_edp	= plat_data->is_edp;
	tx_comm->base.pdev = device;
	tx_comm->hw_comm = to_dptx_hw_common(tx_hw);
	tx_hw->lane_remap = -1;

	/* get config from dts */
	ret = dptx_get_dt_info(pdev, tx_comm, tx_comm->hw_comm);
	if (ret < 0)
		goto dptx_get_dt_info_fail;

	/* dptx common init */
	ret = dptx_common_init(tx_comm, tx_comm->hw_comm);
	if (ret < 0)
		goto dptx_common_init_fail;

	ret = tx_hw->init_tx_hw(tx_comm, tx_hw);
	if (ret < 0)
		goto dptx_init_tx_hw_fail;

	/* dptx probe phy */
	tx_phy = meson_tx_probe_phy(device, tx_hw);
	if (!tx_phy) {
		ret = -ENODEV;
		goto meson_tx_probe_phy_fail;
	}
	/* dptx tx hw setup tx_phy */
	meson_tx_hw_setup_phy(tx_hw, tx_phy);

	tx_clk = meson_tx_probe_clk(device);
	if (!tx_clk) {
		ret = -ENODEV;
		goto meson_tx_probe_clk_fail;
	}
	meson_tx_hw_setup_clk(tx_hw, tx_clk);

	dptx_hdcp_init(tx_comm);
	dev_set_drvdata(device, tx_comm);

	/* eDP doesn't have typeC connector, so only DPTx needs the notifier */
	if (!tx_comm->is_edp) {
		tx_comm->dpphy_to_core_nb.notifier_call = dpphy_to_core_notifier;
		meson_tx_dpphy_register_notify(&tx_comm->dpphy_to_core_nb);
	}

	/* In multiple DPTX scenarios, the probe will be executed multiple times,
	 * so a global handle cannot be used for registration here.
	 */
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (!tx_comm->is_edp || !tx_comm->hw_comm->ext_edptx_ctrl_en) {
		tx_comm->es.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
		tx_comm->es.suspend = dptx_early_suspend;
		tx_comm->es.resume  = dptx_late_resume;
		tx_comm->es.param   = tx_comm;

		register_early_suspend(&tx_comm->es);
		tx_comm->es_registered = true;
	}
#endif

	component_add(device, &dptx_bind_ops);
	global_tx_comm[tx_comm->dev_idx] = tx_comm;

	DPTX_INFO("%s for dev_index[%d]\n", __func__, tx_comm->dev_idx);
	return 0;

meson_tx_probe_clk_fail:
meson_tx_probe_phy_fail:
	tx_comm->base.tx_hw_base->uninit_tx_hw(tx_comm, tx_comm->base.tx_hw_base);
dptx_init_tx_hw_fail:
	dptx_common_uninit(tx_comm);
dptx_common_init_fail:
dptx_get_dt_info_fail:
	kfree(tx_comm);
	tx_comm = NULL;
dptx_common_alloc_fail:
	plat_data->free_tx_hw(tx_hw);

	return ret;
}

static void dptx_remove(struct platform_device *pdev)
{
	const struct meson_tx_plat_data *plat_data;
	struct device *device = &pdev->dev;
	struct dptx_common *tx_comm = dev_get_drvdata(device);
	/* get config from device tree match */
	plat_data = of_device_get_match_data(&pdev->dev);

	if (!tx_comm)
		return;

	meson_tx_dpphy_unregister_notify(&tx_comm->dpphy_to_core_nb);
	component_del(device, &dptx_bind_ops);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (tx_comm->es_registered) {
		unregister_early_suspend(&tx_comm->es);
		tx_comm->es_registered = false;
	}
#endif
	meson_tx_phy_exit(tx_comm->hw_comm->hw_base.tx_phy);
	dev_set_drvdata(device, NULL);
	dptx_hdcp_uninit(tx_comm);
	dptx_common_uninit(tx_comm);
	tx_comm->base.tx_hw_base->uninit_tx_hw(tx_comm, tx_comm->base.tx_hw_base);
	plat_data->free_tx_hw(tx_comm->base.tx_hw_base);

	DPTX_INFO("%s for dev_index[%d]\n", __func__, tx_comm->dev_idx);
	kfree(tx_comm);
	tx_comm = NULL;
}

/* only for external eDPTX driver. dptx core and
 * edptx core may be both used for edptx,
 * so use idx to get the corresponding edptx
 * instance in such case, or get the first edptx
 * instance if dptx core is not used for edptx.
 */
struct dptx_common *dptx_get_edptx_inst(u8 idx)
{
	u8 i = 0;
	struct dptx_common *tx_comm = NULL;

	if (idx >= DPTX_DEV_COUNT)
		return NULL;

	/* find the edptx with index in instance list */
	if (global_tx_comm[idx] && global_tx_comm[idx]->is_edp)
		return global_tx_comm[idx];

	/* find the first edptx in instance list */
	for (i = 0; i < DPTX_DEV_COUNT; i++) {
		if (global_tx_comm[i] && global_tx_comm[i]->is_edp) {
			tx_comm	= global_tx_comm[i];
			break;
		}
	}

	return tx_comm;
}

static struct meson_tx_plat_data dptx_plat_data_a9 = {
	.alloc_tx_hw	= dptx20_alloc_tx_hw,
	.free_tx_hw		= dptx20_free_tx_hw,
	.is_edp			= 0,
};

static struct meson_tx_plat_data edp_plat_data_a9 = {
	.alloc_tx_hw	= dptx20_alloc_tx_hw,
	.free_tx_hw		= dptx20_free_tx_hw,
	.is_edp			= 1,
};

static const struct of_device_id dptx_of_match[] = {
	{
		.compatible	= "amlogic, dptx-t7c",
		.data		= &dptx_plat_data_a9,
	},
	{
		.compatible	= "amlogic, dptx-a9",
		.data		= &dptx_plat_data_a9,
	},
	{
		.compatible	= "amlogic, dptx-core-a9",
		.data		= &edp_plat_data_a9,
	},
	{},
};

static struct platform_driver dptx_driver = {
	.probe	 = dptx_probe,
	.remove	 = dptx_remove,
	.driver	 = {
		.name = DEVICE_NAME_DP,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(dptx_of_match),
#ifdef CONFIG_PM
		.pm = &dptx_pm,
#endif
	}
};

int  __init dptx_init(void)
{
	return platform_driver_register(&dptx_driver);
}

void __exit dptx_exit(void)
{
	DPTX_INFO("%s\n", __func__);
	platform_driver_unregister(&dptx_driver);
}

