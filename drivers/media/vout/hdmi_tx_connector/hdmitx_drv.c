// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/pinctrl/consumer.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/miscdevice.h>
#include <linux/of_gpio.h>
#include <linux/reboot.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/delay.h>
#include <linux/component.h>

#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_platform_linux.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <drm/amlogic/meson_drm_bind.h>

#ifdef CONFIG_AMLOGIC_DSC
#include <linux/amlogic/media/vout/dsc.h>
#endif

#include "hdmitx_log.h"
#include "hdmitx_boot_parameters.h"
#include "hdmitx_sysfs_common.h"
#include "hdmitx_module.h"
#include "hdmitx_compliance.h"
#include "hdmitx_dump.h"
#include "hdmitx_vout.h"
#include "hdmitx_hdr.h"
#include "hdmitx_audio.h"

#define HDMI_TX_COUNT 32
static struct class *hdmitx_class;

#ifdef CONFIG_OF
#ifdef CONFIG_AMLOGIC_HDMITX
static struct hdmitx_ops hdmitx20_ops = {
	.alloc_instance = hdmitx20_alloc_instance,
	.init_reg_map = hdmitx20_init_reg_map,
	.init_hw = hdmitx20_meson_init,
	.uninit_hw = hdmitx20_meson_uninit,
	.get_dbg_files = hdmitx20_get_dbg_files_s,
	.get_dbg_files_count = hdmitx20_get_dbg_files_count,
	.sw_debug_func = hdmitx20_sw_debug_func,
};

static struct amhdmitx_data_s amhdmitx_data_g12a = {
	.chip_type = MESON_CPU_ID_G12A,
	.chip_name = "g12a",
	.hdmitx_ops = &hdmitx20_ops,
};

static struct amhdmitx_data_s amhdmitx_data_g12b = {
	.chip_type = MESON_CPU_ID_G12B,
	.chip_name = "g12b",
	.hdmitx_ops = &hdmitx20_ops,
};

static struct amhdmitx_data_s amhdmitx_data_sm1 = {
	.chip_type = MESON_CPU_ID_SM1,
	.chip_name = "sm1",
	.hdmitx_ops = &hdmitx20_ops,
};

static struct amhdmitx_data_s amhdmitx_data_sc2 = {
	.chip_type = MESON_CPU_ID_SC2,
	.chip_name = "sc2",
	.hdmitx_ops = &hdmitx20_ops,
};

static struct amhdmitx_data_s amhdmitx_data_tm2 = {
	.chip_type = MESON_CPU_ID_TM2,
	.chip_name = "tm2",
	.hdmitx_ops = &hdmitx20_ops,
};
#endif
#ifdef CONFIG_AMLOGIC_HDMITX21
static struct hdmitx_ops hdmitx21_ops = {
	.alloc_instance = hdmitx21_alloc_instance,
	.init_reg_map = hdmitx21_init_reg_map,
	.init_hw = hdmitx21_meson_init,
	.uninit_hw = hdmitx21_meson_uninit,
	.get_dbg_files = hdmitx21_get_dbg_files_s,
	.get_dbg_files_count = hdmitx21_get_dbg_files_count,
	.sw_debug_func = hdmitx21_sw_debug_func,
};

static struct amhdmitx_data_s amhdmitx_data_t7 = {
	.chip_type = MESON_CPU_ID_T7,
	.chip_name = "t7",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_s5 = {
	.chip_type = MESON_CPU_ID_S5,
	.chip_name = "s5",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_s1a = {
	.chip_type = MESON_CPU_ID_S1A,
	.chip_name = "s1a",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_s7 = {
	.chip_type = MESON_CPU_ID_S7,
	.chip_name = "s7",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_s7d = {
	.chip_type = MESON_CPU_ID_S7D,
	.chip_name = "s7d",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_s6 = {
	.chip_type = MESON_CPU_ID_S6,
	.chip_name = "s6",
	.hdmitx_ops = &hdmitx21_ops,
};

static struct amhdmitx_data_s amhdmitx_data_a9 = {
	.chip_type = MESON_CPU_ID_A9,
	.chip_name = "a9",
	.hdmitx_ops = &hdmitx21_ops,
};
#endif

static const struct of_device_id meson_amhdmitx_of_match[] = {
#ifdef CONFIG_AMLOGIC_HDMITX
	{
		.compatible	 = "amlogic, amhdmitx-g12a",
		.data = &amhdmitx_data_g12a,
	},
	{
		.compatible	 = "amlogic, amhdmitx-g12b",
		.data = &amhdmitx_data_g12b,
	},
	{
		.compatible	 = "amlogic, amhdmitx-sm1",
		.data = &amhdmitx_data_sm1,
	},
	{
		.compatible	 = "amlogic, amhdmitx-sc2",
		.data = &amhdmitx_data_sc2,
	},
	{
		.compatible	 = "amlogic, amhdmitx-tm2",
		.data = &amhdmitx_data_tm2,
	},
#endif
#ifdef CONFIG_AMLOGIC_HDMITX21
	{
		.compatible	 = "amlogic, amhdmitx-t7",
		.data = &amhdmitx_data_t7,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s5",
		.data = &amhdmitx_data_s5,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s1a",
		.data = &amhdmitx_data_s1a,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s7",
		.data = &amhdmitx_data_s7,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s7d",
		.data = &amhdmitx_data_s7d,
	},
	{
		.compatible	 = "amlogic, amhdmitx-s6",
		.data = &amhdmitx_data_s6,
	},
	{
		.compatible	 = "amlogic, amhdmitx-a9",
		.data = &amhdmitx_data_a9,
	},
#endif
	{},
};
#else
#define meson_amhdmitx_dt_match NULL
#endif

static int get_dt_vend_init_data(struct device_node *np,
				 struct vendor_info_data *vend)
{
	int ret;

	ret = of_property_read_string(np, "vendor_name",
				      (const char **)&vend->vendor_name);
	if (ret)
		HDMITX_INFO("not find vendor name\n");

	ret = of_property_read_u32(np, "vendor_id", &vend->vendor_id);
	if (ret)
		HDMITX_INFO("not find vendor id\n");

	ret = of_property_read_string(np, "product_desc",
				      (const char **)&vend->product_desc);
	if (ret)
		HDMITX_INFO("not find product desc\n");
	return 0;
}

/*
 * amhdmitx_clktree_probe
 * get clktree info from dts
 */
static void amhdmitx_clktree_probe(struct device *hdmitx_dev, struct hdmitx_common *tx_comm)
{
	struct clk *hdmi_clk_vapb, *hdmi_clk_vpu;
	struct clk *hdcp22_tx_skp, *hdcp22_tx_esm;
	struct clk *venci_top_gate, *venci_0_gate, *venci_1_gate;
	struct clk *cts_hdmi_axi_clk;

	hdmi_clk_vapb = devm_clk_get(hdmitx_dev, "hdmi_vapb_clk");
	if (!IS_ERR(hdmi_clk_vapb)) {
		tx_comm->hdmitx_clk_tree.hdmi_clk_vapb = hdmi_clk_vapb;
		clk_prepare_enable(tx_comm->hdmitx_clk_tree.hdmi_clk_vapb);
	}

	hdmi_clk_vpu = devm_clk_get(hdmitx_dev, "hdmi_vpu_clk");
	if (!IS_ERR(hdmi_clk_vpu)) {
		tx_comm->hdmitx_clk_tree.hdmi_clk_vpu = hdmi_clk_vpu;
		clk_prepare_enable(tx_comm->hdmitx_clk_tree.hdmi_clk_vpu);
	}

	hdcp22_tx_skp = devm_clk_get(hdmitx_dev, "hdcp22_tx_skp");
	if (!IS_ERR(hdcp22_tx_skp))
		tx_comm->hdmitx_clk_tree.hdcp22_tx_skp = hdcp22_tx_skp;

	hdcp22_tx_esm = devm_clk_get(hdmitx_dev, "hdcp22_tx_esm");
	if (!IS_ERR(hdcp22_tx_esm))
		tx_comm->hdmitx_clk_tree.hdcp22_tx_esm = hdcp22_tx_esm;

	venci_top_gate = devm_clk_get(hdmitx_dev, "venci_top_gate");
	if (!IS_ERR(venci_top_gate))
		tx_comm->hdmitx_clk_tree.venci_top_gate = venci_top_gate;

	venci_0_gate = devm_clk_get(hdmitx_dev, "venci_0_gate");
	if (!IS_ERR(venci_0_gate))
		tx_comm->hdmitx_clk_tree.venci_0_gate = venci_0_gate;

	venci_1_gate = devm_clk_get(hdmitx_dev, "venci_1_gate");
	if (!IS_ERR(venci_1_gate))
		tx_comm->hdmitx_clk_tree.venci_1_gate = venci_1_gate;

	cts_hdmi_axi_clk = devm_clk_get(hdmitx_dev, "cts_hdmi_axi_clk");
	if (!IS_ERR(cts_hdmi_axi_clk))
		tx_comm->hdmitx_clk_tree.cts_hdmi_axi_clk = cts_hdmi_axi_clk;
}

static int amhdmitx_get_dt_info(struct platform_device *pdev, struct hdmitx_common *tx_comm)
{
	struct pinctrl *pin;
	int ret;
#ifdef CONFIG_OF
	int val;
	phandle phandler;
	struct device_node *init_data;
#else
	struct hdmi_config_platform_data *hdmi_pdata;
#endif

	u32 refresh_rate_limit = 0;
	struct hdmitx_hw_common *hw_comm;

	hw_comm = tx_comm->tx_hw;

	/* HDMITX pinctrl config for hdp and ddc */
	if (pdev->dev.of_node) {
		tx_comm->pdev = &pdev->dev;
		pin = devm_pinctrl_get(&pdev->dev);
		if (!pin) {
			HDMITX_INFO("get pin control fail\n");
			return -1;
		}

		tx_comm->pinctrl_hpd = pinctrl_lookup_state(pin, "hdmitx_hpd");
		if (IS_ERR(tx_comm->pinctrl_hpd)) {
			HDMITX_ERROR("no default of pinctrl state\n");
		} else {
			ret = pinctrl_select_state(pin, tx_comm->pinctrl_hpd);
			if (ret < 0)
				HDMITX_ERROR("failed to select hpd pinctrl state\n");
		}

		tx_comm->pinctrl_ddc = pinctrl_lookup_state(pin, "hdmitx_ddc");
		if (IS_ERR(tx_comm->pinctrl_ddc)) {
			HDMITX_ERROR("no hdmitx_ddc of pinctrl state\n");
		} else {
			ret = pinctrl_select_state(pin, tx_comm->pinctrl_ddc);
			if (ret < 0)
				HDMITX_ERROR("failed to select hdmitx_ddc pinctrl state\n");
		}
	} else {
		HDMITX_INFO("node null\n");
	}
	hw_comm->hdmitx_gpios_hpd = of_get_named_gpio(pdev->dev.of_node,
		"hdmitx-gpios-hpd", 0);
	if (hw_comm->hdmitx_gpios_hpd < 0)
		HDMITX_ERROR("get hdmitx-gpios-hpd error\n");
	hw_comm->hdmitx_gpios_scl = of_get_named_gpio(pdev->dev.of_node,
		"hdmitx-gpios-scl", 0);
	if (hw_comm->hdmitx_gpios_scl < 0)
		HDMITX_ERROR("get hdmitx-gpios-scl error\n");
	hw_comm->hdmitx_gpios_sda = of_get_named_gpio(pdev->dev.of_node,
		"hdmitx-gpios-sda", 0);
	if (hw_comm->hdmitx_gpios_sda < 0)
		HDMITX_ERROR("get hdmitx-gpios-sda error\n");

#ifdef CONFIG_OF
	if (pdev->dev.of_node) {
		int dongle_mode = 0;
		int pxp_mode = 0;

		memset(&tx_comm->config_data, 0,
		       sizeof(struct hdmi_config_platform_data));

		if (hw_comm->chip_data->chip_type == MESON_CPU_ID_TM2 ||
			hw_comm->chip_data->chip_type == MESON_CPU_ID_TM2B) {
			/* diff revA/B of TM2 chip */
			if (is_meson_rev_b()) {
				hw_comm->chip_data->chip_type = MESON_CPU_ID_TM2B;
				hw_comm->chip_data->chip_name = "tm2b";
			} else {
				hw_comm->chip_data->chip_type = MESON_CPU_ID_TM2;
				hw_comm->chip_data->chip_name = "tm2";
			}
		}
		if (hw_comm->chip_data->chip_type == MESON_CPU_ID_S5)
			hw_comm->hdmi_tx_cap.dsc_capable = true;
		else
			hw_comm->hdmi_tx_cap.dsc_capable = false;
		/* Get hdmi_rext information */
		ret = of_property_read_u32(pdev->dev.of_node, "hdmi_rext", &val);
		tx_comm->hdmi_rext = val;
		if (!ret)
			HDMITX_INFO("hdmi_rext: %d\n", val);
		/* Get pxp_mode information */
		ret = of_property_read_u32(pdev->dev.of_node, "pxp_mode",
					   &pxp_mode);
		tx_comm->pxp_mode = pxp_mode;
		if (!ret)
			HDMITX_INFO("pxp_mode: %d\n", tx_comm->pxp_mode);
		/* Get dongle_mode information */
		ret = of_property_read_u32(pdev->dev.of_node, "dongle_mode",
					   &dongle_mode);
		hw_comm->dongle_mode = !!dongle_mode;
		if (!ret)
			HDMITX_INFO("hdmitx_device.dongle_mode: %d\n",
				hw_comm->dongle_mode);
		/* Get res_1080p information */
		ret = of_property_read_u32(pdev->dev.of_node, "res_1080p",
					   &tx_comm->res_1080p);
		tx_comm->res_1080p = !!tx_comm->res_1080p;
		/*
		 * the max tmds cap is 600Mhz by default,
		 * if soc limit to 1080p maximum, then the
		 * max tmds cap is 225Mhz
		 */
		if (tx_comm->res_1080p)
			hw_comm->hdmi_tx_cap.tx_max_tmds_clk = 225;
		else
			hw_comm->hdmi_tx_cap.tx_max_tmds_clk = 600;
		ret = of_property_read_u32(pdev->dev.of_node, "max_refresh_rate",
					   &refresh_rate_limit);
		if (ret == 0 && refresh_rate_limit > 0)
			tx_comm->max_refresh_rate = refresh_rate_limit;
		/* Get repeater_tx information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "repeater_tx", &val);
		if (!ret)
			tx_comm->hdcptx_comm.hdcp_rpt_en = val;
		else
			tx_comm->hdcptx_comm.hdcp_rpt_en = 0;
		/* Get repeater_tx information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "hdmi_repeater", &val);
		if (!ret)
			tx_comm->hdmi_repeater = val;
		else
			tx_comm->hdmi_repeater = 1;
		/* if it's not hdmi repeater, then should not support hdcp repeater */
		if (tx_comm->hdmi_repeater == 0)
			tx_comm->hdcptx_comm.hdcp_rpt_en = 0;
		/* Get cedst_en information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "cedst_en", &val);
		if (!ret)
			tx_comm->cedst_en = !!val;
		/* Get hdr_8bit_en information */
		ret = of_property_read_u32(pdev->dev.of_node, "hdr_8bit_en", &val);
		if (!ret)
			tx_comm->hdr_8bit_en = !!val;
		/* Get hdcp_type_policy information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "hdcp_type_policy", &val);
		if (!ret) {
			tx_comm->hdcp_type_policy = 0;
			if (val == 2)
				tx_comm->hdcp_type_policy = 2;
			if (val == 1)
				tx_comm->hdcp_type_policy = 1;
			if (val == 3)
				tx_comm->hdcp_type_policy = -1;
		}
		/* not support FRL by default, unless enabled in dts */
		hw_comm->hdmi_tx_cap.tx_max_frl_rate = FRL_NONE;
		ret = of_property_read_u32(pdev->dev.of_node, "tx_max_frl_rate", &val);
		if (!ret) {
			if (val > FRL_12G4L)
				HDMITX_INFO("wrong tx_max_frl_rate %d\n", val);
			else
				hw_comm->hdmi_tx_cap.tx_max_frl_rate = val;
		}
		/* hdcp ctrl 0:sysctrl, 1: drv, 2: linux app */
		ret = of_property_read_u32(pdev->dev.of_node,
			   "hdcp_ctl_lvl", &tx_comm->hdcptx_comm.hdcp_ctl_lvl);
		if (ret)
			tx_comm->hdcptx_comm.hdcp_ctl_lvl = 0;
		HDMITX_INFO("hdcp_ctl_lvl[%d]\n", tx_comm->hdcptx_comm.hdcp_ctl_lvl);
		/* Get vendor information */
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vend-data", &val);
		if (ret)
			HDMITX_INFO("not find match init-data\n");
		if (ret == 0) {
			phandler = val;
			init_data = of_find_node_by_phandle(phandler);
			if (!init_data)
				HDMITX_INFO("not find device node\n");
			tx_comm->config_data.vend_data =
			kzalloc(sizeof(struct vendor_info_data), GFP_KERNEL);
			if (!(tx_comm->config_data.vend_data))
				HDMITX_INFO("not allocate memory\n");
			ret = get_dt_vend_init_data
			(init_data, tx_comm->config_data.vend_data);
			if (ret)
				HDMITX_INFO("not find vend_init_data\n");
		}

		/* is_preferred_frac_mode 0: not support, 1: support */
		ret = of_property_read_u32(pdev->dev.of_node, "frac_enable", &val);
		if (!ret)
			tx_comm->frac_enable = val;
		else
			tx_comm->frac_enable = 0;
		HDMITX_INFO("frac_enable[%d]\n", tx_comm->frac_enable);

		/* Get reg information */
		ret = hw_comm->chip_data->hdmitx_ops->init_reg_map(pdev);
		if (ret < 0)
			HDMITX_ERROR("ERROR: hdmitx io_remap fail!\n");

		ret = of_property_read_u32(pdev->dev.of_node,
					   "enc_idx", &val);
		if (!ret && val == 2)
			tx_comm->enc_idx = 2;
		else if (!ret && val == 1)
			tx_comm->enc_idx = 1;
		else
			tx_comm->enc_idx = 0;
		HDMITX_INFO("enc_idx %d\n", tx_comm->enc_idx);

		ret = of_property_read_u32(pdev->dev.of_node,
					   "clk_source_idx", &val);
		if (!ret && val == 1)
			tx_comm->clk_source_idx = 1;
		else
			tx_comm->clk_source_idx = 0;
		HDMITX_INFO("clk_source_idx %d\n", tx_comm->clk_source_idx);
	}
#else
	hdmi_pdata = pdev->dev.platform_data;
	if (!hdmi_pdata) {
		HDMITX_INFO("not get platform data\n");
		r = -ENOENT;
	} else {
		HDMITX_INFO("get hdmi platform data\n");
	}
#endif
	tx_comm->irq_hpd = platform_get_irq_byname(pdev, "hdmitx_hpd");
	if (tx_comm->irq_hpd == -ENXIO) {
		HDMITX_ERROR("%s: ERROR: hdmitx hpd irq No not found\n",
			__func__);
			return -ENXIO;
	}
	HDMITX_DEBUG("hpd irq = %d\n", tx_comm->irq_hpd);

	tx_comm->irq_hdmitx_vsync = platform_get_irq_byname(pdev, "hdmitx_vsync");
	if (tx_comm->irq_hdmitx_vsync == -ENXIO) {
		HDMITX_ERROR("%s: ERROR: hdmitx vsync irq No not found\n", __func__);
		return -ENXIO;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "arc_rx_en", &val);
	if (!ret)
		tx_comm->arc_rx_en = val;
	else
		tx_comm->arc_rx_en = 0;
	amhdmitx_clktree_probe(&pdev->dev, tx_comm);
	return ret;
}

/*****************************
 *	hdmitx driver file_operations
 *
 ******************************/
static int amhdmitx_open(struct inode *node, struct file *file)
{
	struct hdmitx_common *tx_comm;

	/* Get the per-device structure that contains this cdev */
	tx_comm = container_of(node->i_cdev, struct hdmitx_common, cdev);
	file->private_data = tx_comm;

	return 0;
}

static int amhdmitx_release(struct inode *node, struct file *file)
{
	return 0;
}

static const struct file_operations amhdmitx_fops = {
	.owner	= THIS_MODULE,
	.open	 = amhdmitx_open,
	.release  = amhdmitx_release,
};

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static void hdmitx_early_suspend(struct early_suspend *h)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)h->param;
	bool need_rst_ratio;
	u32 arg = 0;

	need_rst_ratio = hdmitx_find_vendor_ratio(tx_comm->edid_buf);
	mutex_lock(&tx_comm->hdmimode_mutex);

	tx_comm->qms_log_id = 0;
	HDMITX_INFO(SYS "Early Suspend\n");

	/* step2: clear ready status/disable phy/packets/hdcp HW */
	hdmitx_common_output_disable(tx_comm,
		true, true, true, false);

	/*
	 * under suspend, driver should not respond to mode setting,
	 * as it may cause logic abnormal, most importantly,
	 * it will enable hdcp and occupy DDC channel with high
	 * priority, though there's protection in system control,
	 * driver still need protection in case of old android version
	 *
	 * Using the suspend_flag as a condition: when hdmitx
	 * enters early suspend, it calls hdmitx_common_output_disable api,
	 * sends a uevent, and then sets suspend_flag to true.
	 * When DRM suspend is executed, hdmitx_common_output_disable api
	 * is called again, but since suspend_flag is now true,
	 * the uevent is not sent again, allowing the system to enter
	 * the suspend state normally
	 */
	tx_comm->suspend_flag = true;
	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_PARAM_RESET, NULL, NULL);
	/* step3: SW: post uevent to system */
	hdmitx_set_uevent(tx_comm, HDMITX_HDCPPWR_EVENT, HDMI_SUSPEND);
	hdmitx_set_uevent(tx_comm, HDMITX_AUDIO_EVENT, 0);

	if (need_rst_ratio) {
		arg = CLEAR_SCDC_DIV40_SCRAMB;
		hdmitx_hw_cntl(tx_comm->tx_hw, DDC_SCDC_DIV40_SCRAMB, (void *)&arg, NULL);
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

static void hdmitx_late_resume(struct early_suspend *h)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)h->param;

	mutex_lock(&tx_comm->hdmimode_mutex);
	hdmitx_common_late_resume(tx_comm);
	HDMITX_INFO(SYS "Late Resume\n");
	mutex_unlock(&tx_comm->hdmimode_mutex);

	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
}

/*
 * Note: HPLL is disabled when suspend/shutdown, and should
 * not be called when reboot/early suspend, otherwise
 * there will be no vsync for drm.
 */
static int hdmitx_reboot_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct hdmitx_common *tx_comm = container_of(nb, struct hdmitx_common, reboot_nb);
	u32 arg = TMDS_PHY_DISABLE;

	hdmitx_common_avmute_locked(tx_comm, SET_AVMUTE, AVMUTE_PATH_HDMITX);
	hdmitx_hw_cntl(tx_comm->tx_hw, PLATFORM_PHY_OP, (void *)&arg, NULL);
	tx_comm->ready = 0;

	/* disable frl/dsc/vrr */
	hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_DISABLE_21_WORK, NULL, NULL);

	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_DISABLE, NULL, NULL);
	hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_PARAM_RESET, NULL, NULL);

	if (tx_comm->rxsense_policy)
		cancel_delayed_work(&tx_comm->work_rxsense);
	if (tx_comm->cedst_en)
		cancel_delayed_work(&tx_comm->work_cedst);

	return NOTIFY_OK;
}

static struct early_suspend hdmitx_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10,
	.suspend = hdmitx_early_suspend,
	.resume = hdmitx_late_resume,
};
#endif

static int meson_hdmitx_bind(struct device *dev,
			      struct device *master, void *data)
{
	int drm_hdmitx_id;
	struct hdmitx_common *tx_common = dev_get_drvdata(dev);
	struct meson_drm_bound_data *bound_data = data;

	if (bound_data->connector_component_bind) {
		drm_hdmitx_id = bound_data->connector_component_bind
			(bound_data->drm,
			DRM_MODE_CONNECTOR_MESON_HDMIA_A + tx_common->enc_idx,
			&tx_common->base);
		HDMITX_DEBUG("%s hdmi [%d]\n", __func__, drm_hdmitx_id);
		tx_common->drm_hdmitx_id = drm_hdmitx_id;
	} else {
		HDMITX_ERROR("no bind func from drm.\n");
	}

	return 0;
}

static void meson_hdmitx_unbind(struct device *dev,
				 struct device *master, void *data)
{
	struct hdmitx_common *tx_common = dev_get_drvdata(dev);
	struct meson_drm_bound_data *bound_data = data;

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			DRM_MODE_CONNECTOR_MESON_HDMIA_A + tx_common->enc_idx,
			&tx_common->base);
		HDMITX_INFO("%s success\n", __func__);
	} else {
		HDMITX_ERROR("no unbind func.\n");
	}
}

/*drm component bind ops*/
static const struct component_ops meson_hdmitx_bind_ops = {
	.bind	= meson_hdmitx_bind,
	.unbind	= meson_hdmitx_unbind,
};

int hdmitx_bind_meson_drm(struct device *device)
{
	return component_add(device, &meson_hdmitx_bind_ops);
}

int hdmitx_unbind_meson_drm(struct device *device)
{
	component_del(device, &meson_hdmitx_bind_ops);

	return 0;
}

static int amhdmitx_probe(struct platform_device *pdev)
{
	int r, ret = 0;
	struct device *device = &pdev->dev;
	struct device *dev;
	struct hdmitx_common *tx_comm;
	struct hdmitx_hw_common *hw_comm;
	struct hdmitx_tracer *tx_tracer;
	struct hdmitx_event_mgr *tx_uevent_mgr;
	struct amhdmitx_data_s *chip_data;
	const struct of_device_id *match;
	bool hpd_state;

	HDMITX_INFO("amhdmitx_probe_start\n");

	/* Get chip type and name information */
	match = of_match_device(meson_amhdmitx_of_match, device);
	if (!match) {
		HDMITX_INFO("%s: no match table\n", __func__);
		return -1;
	}
	chip_data = (struct amhdmitx_data_s *)match->data;
	HDMITX_DEBUG("chip_type:%d chip_name:%s\n",
		chip_data->chip_type,
		chip_data->chip_name);
	/* allocate address space */
	tx_comm = chip_data->hdmitx_ops->alloc_instance(device);
	if (!tx_comm)
		return -ENOMEM;
	hw_comm = tx_comm->tx_hw;
	hw_comm->chip_data = chip_data;
	dev_set_drvdata(device, tx_comm);

	hdmitx_common_init(tx_comm, hw_comm);
	/* note that dts config will override the default config when init */
	ret = amhdmitx_get_dt_info(pdev, tx_comm);

	r = alloc_chrdev_region(&tx_comm->hdmitx_id, 0, HDMI_TX_COUNT,
				DEVICE_NAME);
	cdev_init(&tx_comm->cdev, &amhdmitx_fops);
	tx_comm->cdev.owner = THIS_MODULE;
	r = cdev_add(&tx_comm->cdev, tx_comm->hdmitx_id, HDMI_TX_COUNT);

	hdmitx_class = class_create(DEVICE_NAME);
	if (IS_ERR(hdmitx_class)) {
		unregister_chrdev_region(tx_comm->hdmitx_id, HDMI_TX_COUNT);
		return -1;
	}
	/* kernel>=2.6.27 */
	dev = device_create(hdmitx_class, NULL, tx_comm->hdmitx_id, tx_comm,
			    "amhdmitx%d", MINOR(tx_comm->hdmitx_id));
	if (!dev) {
		HDMITX_ERROR("device_create create error\n");
		class_destroy(hdmitx_class);
		r = -EEXIST;
		return r;
	}
	tx_comm->hdtx_dev = dev;

#ifdef CONFIG_AMLOGIC_VPU
	if (tx_comm->tx_hw->chip_data->chip_type <	MESON_CPU_ID_T7) {
		tx_comm->encp_vpu_dev = vpu_dev_register(VPU_VENCP, DEVICE_NAME);
		tx_comm->enci_vpu_dev = vpu_dev_register(VPU_VENCI, DEVICE_NAME);
		/* vpu gate/mem ctrl for hdmitx, since TM2B */
		tx_comm->hdmi_vpu_dev = vpu_dev_register(VPU_HDMI, DEVICE_NAME);
	}
#endif
	/* platform related functions */
	tx_uevent_mgr = hdmitx_event_mgr_create(pdev, tx_comm->hdtx_dev);
	hdmitx_event_mgr_suspend(tx_uevent_mgr, false);
	hdmitx_common_attch_platform_data(tx_comm,
		HDMITX_PLATFORM_UEVENT, tx_uevent_mgr);
	tx_tracer = hdmitx_tracer_create(tx_uevent_mgr);
	hdmitx_common_attch_platform_data(tx_comm,
		HDMITX_PLATFORM_TRACER, tx_tracer);
	tx_comm->reboot_nb.notifier_call = hdmitx_reboot_notifier;
	register_reboot_notifier(&tx_comm->reboot_nb);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	hdmitx_early_suspend_handler.param = tx_comm;
	register_early_suspend(&hdmitx_early_suspend_handler);
#endif
	/* init hw */
	hw_comm->chip_data->hdmitx_ops->init_hw(hw_comm);
	/* bind drm before hdmi event */
	hdmitx_bind_meson_drm(device);
	/* init power_uevent state */
	hdmitx_set_uevent(tx_comm, HDMITX_HDCPPWR_EVENT, HDMI_WAKEUP);
	/* reset EDID/vinfo */
	if (!tx_comm->forced_edid) {
		hdmitx_edid_buffer_clear(tx_comm->edid_buf, sizeof(tx_comm->edid_buf));
		hdmitx_edid_rxcap_clear(&tx_comm->rxcap);
	}
	/*
	 * hpd process of bootup stage, need to be done in probe
	 * as other client modules may need the hpd/edid info.
	 * use mutex to prevent hpd irq bottom half concurrency.
	 */
	mutex_lock(&tx_comm->hdmimode_mutex);
	/* enable irq firstly before any hpd handler to prevent missing irq. */
	hw_comm->setup_irq(hw_comm);
	/* actions in top half of plug intr */
	hpd_state = !!hdmitx_hw_cntl(hw_comm,
		PLATFORM_GET_HPD_GPI_ST, NULL, NULL);
	hdmitx_hw_cntl(hw_comm, MODE_FLOW_HPD_IRQ_TOP_HALF, (void *)&hpd_state, NULL);
	/* actions in bottom half of plug intr */
	if (hpd_state)
		hdmitx_process_plugin(tx_comm, true, false);
	else
		hdmitx_process_plugout(tx_comm);
	/*
	 * When Sink-led output, the Color Space read from AVI Packet is RGB,
	 * but the input to HDMITX is YUV444, so it is necessary to judge that
	 * the Color Space is configured to be YUV444 when the current output
	 * is Sink-led, this logic judgment needs to be executed after parsing
	 * the EDID, refer to the SWPL-180597 for details
	 *
	 * load fmt para from hw info
	 */
	hdmitx_common_init_bootup_format_para(tx_comm, &tx_comm->fmt_para);
	/* load init hdr state from HW info */
	hdmitx_hdr_state_init(tx_comm);
	/* only when hpd is high does it need to update vinfo and send the div40 signal */
	if (hpd_state)
		hdmitx_bootup_post_process(tx_comm);

	/* after unlock, now can take actions of bottom half of hpd irq */
	mutex_unlock(&tx_comm->hdmimode_mutex);
	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);

	/* everything is ready, create sysfs here */
	hdmitx_sysfs_common_create(dev, tx_comm, hw_comm);
	hdmitx_common_profs_init(tx_comm);
	hdmitx_common_debugfs_init(tx_comm);
	HDMITX_INFO("amhdmitx_probe_end\n");

	return r;
}

void hdmitx_common_sw_debug_func(struct hdmitx_common *tx_comm, const char *buf)
{
	char tmpbuf[64];
	char *tmpbuf2;
	int i = 0;
	int ret;
	unsigned long value = 0;
	struct ced_cnt *ced = &tx_comm->ced_cnt;
	struct scdc_locked_st *ch_st = &tx_comm->ch_locked_st;
	enum frl_rate_enum frl_rate;
	struct hdmitx_hw_common *tx_hw = tx_comm->tx_hw;
	struct master_display_info_s data = {0};
	struct hdr10plus_para hdr_data = {0x1, 0x2, 0x3};
	struct cuva_hdr_vs_emds_para cuva_emp_data = {0x1, 0x2, 0x3};
	struct cuva_hdr_vsif_para cuva_vsif_data = {0x1, 0x1, 0x1};
	struct dv_vsif_para vsif_para = {0};
	static char test_fmt_attr[16];

	while ((buf[i]) && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	if (strncmp(tmpbuf, "edid_check", 10) == 0) {
		if (strncmp(tmpbuf + 10, "=0", 2) == 0)
			tx_comm->rxcap.edid_check = 0;
		else if (strncmp(tmpbuf + 10, "=1", 2) == 0)
			tx_comm->rxcap.edid_check = 1;
		else if (strncmp(tmpbuf + 10, "=2", 2) == 0)
			tx_comm->rxcap.edid_check = 2;
		else if (strncmp(tmpbuf + 10, "=3", 2) == 0)
			tx_comm->rxcap.edid_check = 3;
		HDMITX_INFO("edid_check = %d\n", tx_comm->rxcap.edid_check);
		return;
	} else if (strncmp(tmpbuf, "chkfmt", 6) == 0) {
		hdmitx_mode_print_all_mode_table();
		return;
	} else if (strncmp(tmpbuf, "hdcp_mode", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 16, &value);
		if (ret == 0 && value <= 2)
			tx_comm->hdcptx_comm.test_hdcp_mode = value - 0;
		HDMITX_INFO("test hdcptx_comm_mode: %d\n", tx_comm->hdcptx_comm.test_hdcp_mode);
	} else if (strncmp(tmpbuf, "drm_hdcp_op", 11) == 0) {
		ret = kstrtoul(tmpbuf + 11, 16, &value);
		if (value == 0 && tx_comm->hdcptx_comm.drm_hdcp_disable)
			tx_comm->hdcptx_comm.drm_hdcp_disable(tx_comm);
		else if (value == 1 && tx_comm->hdcptx_comm.drm_hdcp_enable)
			tx_comm->hdcptx_comm.drm_hdcp_enable(tx_comm,
				tx_comm->hdcptx_comm.test_hdcp_mode);
		else if (value == 2 && tx_comm->hdcptx_comm.drm_hdcp_disconnect)
			tx_comm->hdcptx_comm.drm_hdcp_disconnect(tx_comm);
	} else if (strncmp(tmpbuf, "drm_hdcp_ver", 12) == 0) {
		HDMITX_INFO("test drm_hdcp_ver: %d\n",
			drm_hdmitx_common_get_rx_hdcp_cap(tx_comm));
	} else if (strncmp(tmpbuf, "avmute_frame", 12) == 0) {
		ret = kstrtoul(tmpbuf + 12, 10, &value);
		tx_comm->debug_param.avmute_frame = value;
		HDMITX_INFO("avmute_frame = %lu\n", value);
	} else if (strncmp(tmpbuf, "disable_delay", 13) == 0) {
		ret = kstrtoul(tmpbuf + 13, 10, &value);
		tx_comm->debug_param.disable_delay_ms = value;
		HDMITX_INFO("disable_delay_ms = %lu\n", value);
	} else if (strncmp(tmpbuf, "dis_dmc_req", 11) == 0) {
		ret = kstrtoul(tmpbuf + 11, 10, &value);
		tx_comm->debug_param.dis_dmc_req = !!value;
		HDMITX_INFO("dis_dmc_req = %lu\n", value);
	} else if (strncmp(tmpbuf, "config_csc_en", 13) == 0) {
		ret = kstrtoul(tmpbuf + 13, 0, &value);
		HDMITX_INFO("config_csc_en to %lu\n", value);
		if (value == 0)
			tx_comm->config_csc_en = false;
		else if (value == 1)
			tx_comm->config_csc_en = true;
	} else if (strncmp(tmpbuf, "edid_parse", 10) == 0) {
		if (tmpbuf[10] == '1') {
			/*
			 * Enable edid parse in hdmitx debug function command
			 * echo edid_parse1 > /sys/class/amhdmitx/amhdmitx0/debug
			 */
			tx_comm->edid_parse_dbg = true;
			HDMITX_INFO("edid_parse_dbg = %d\n",
					tx_comm->edid_parse_dbg);
		} else if (tmpbuf[10] == '0') {
			/*
			 * Disable edid parse in hdmitx debug function command
			 * echo edid_parse0 > /sys/class/amhdmitx/amhdmitx0/debug
			 */
			tx_comm->edid_parse_dbg = false;
			HDMITX_INFO("edid_parse_dbg = %d\n",
					tx_comm->edid_parse_dbg);
		}
	} else if (strncmp(tmpbuf, "cedst_count", 11) == 0) {
		frl_rate = hdmitx_hw_cntl(tx_hw, FRL_GET_MODE, NULL, NULL);
		if (!frl_rate && !ch_st->clock_detected)
			HDMITX_INFO("clock undetected\n");
		if (!ch_st->ch0_locked)
			HDMITX_INFO("CH0 unlocked\n");
		if (!ch_st->ch1_locked)
			HDMITX_INFO("CH1 unlocked\n");
		if (!ch_st->ch2_locked)
			HDMITX_INFO("CH2 unlocked\n");
		if (frl_rate && !ch_st->ch3_locked)
			HDMITX_INFO("CH3 unlocked\n");
		if (ced->ch0_valid && ced->ch0_cnt)
			HDMITX_INFO("CH0 ErrCnt 0x%x\n", ced->ch0_cnt);
		if (ced->ch1_valid && ced->ch1_cnt)
			HDMITX_INFO("CH1 ErrCnt 0x%x\n", ced->ch1_cnt);
		if (ced->ch2_valid && ced->ch2_cnt)
			HDMITX_INFO("CH2 ErrCnt 0x%x\n", ced->ch2_cnt);
		if (frl_rate >= FRL_6G4L && ced->ch3_valid && ced->ch3_cnt)
			HDMITX_INFO("CH3 ErrCnt 0x%x\n", ced->ch3_cnt);
		if (frl_rate && ced->rs_c_valid && ced->rs_c_cnt)
			HDMITX_INFO("RSCC ErrCnt 0x%x\n", ced->rs_c_cnt);
		memset(ced, 0, sizeof(*ced));
	} else if (strncmp(tmpbuf, "fake_plug", 9) == 0) {
		bool update_hpd = false;

		HDMITX_INFO("fake plug %s\n", tmpbuf);
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		mutex_lock(&tx_comm->hdmimode_mutex);
		/*
		 * before write 0 to fake_plug, echo hpd_lock1 > debug to ignore
		 * hpd event; before write 1 to fake_plug, echo hpd_lock0 > debug
		 * to respond hpd event normally
		 */
		switch (value) {
		case 0:
			/* Fake Plugout */
			tx_comm->hpd_state = 0;
			update_hpd = true;
			break;
		case 1:
			/*
			 * Fake Plugin
			 * If real HPD is low:
			 *	Do not send notify (wait for TV to pull HPD high)
			 * If real HPD is high:
			 *	Assume EDID remains unchanged
			 *	Notify system/DRM to configure HDMI mode
			 */
			if ((hdmitx_hw_cntl(tx_hw, PLATFORM_GET_HPD_GPI_ST, NULL, NULL) == 1) ||
				tx_comm->pxp_mode) {
				tx_comm->hpd_state = 1;
				update_hpd = true;
			}
			break;
		default:
			HDMITX_INFO("invalid input: only 0 or 1 is supported\n");
			break;
		}
		if (update_hpd) {
			hdmitx_common_notify_hpd_status(tx_comm, false);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			/* notify to drm hdmi */
			hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
			HDMITX_INFO("hpd_state: %d\n", tx_comm->hpd_state);
		} else {
			mutex_unlock(&tx_comm->hdmimode_mutex);
			HDMITX_INFO("do not send notify\n");
		}
	} else if (strncmp(tmpbuf, "hdr_mute_frame", 14) == 0) {
		if (kstrtoul(tmpbuf + 14, 10, &value) == 0)
			tx_comm->hdr_mute_frame = value;
		HDMITX_INFO("hdr_mute_frame: %d\n", tx_comm->hdr_mute_frame);
		return;
	} else if (strncmp(tmpbuf, "is_hdcp_cts_te", 14) == 0) {
		/* is hdcp cts test equipment */
		HDMITX_INFO("%d\n", hdmitx_edid_only_support_sd(&tx_comm->rxcap));
	} else if (strncmp(tmpbuf, "config", 6) == 0) {
		tmpbuf2 = tmpbuf + 6;
		HDMITX_INFO("config: %s\n", tmpbuf2);

		if (strncmp(tmpbuf2, "info", 4) == 0) {
			HDMITX_INFO("%x %x %x %x %x %x\n",
				hdmitx_hw_get_hdr_st(tx_hw),
				hdmitx_hw_get_dv_st(tx_hw),
				hdmitx_hw_get_hdr10p_st(tx_hw),
				hdmitx_hdr_en(tx_hw),
				hdmitx_dv_en(tx_hw),
				hdmitx_hdr10p_en(tx_hw)
				);
		} else if (strncmp(tmpbuf2, "sdr_hdr_dov", 11) == 0) {
			/*
			 * firstly stay at SDR state, then send hdr->dv packet to
			 * emulate SDR->HDR->DV switch, DRM-TX-47
			 */
			/* step1: SDR-->HDR */
			data.features = 0x00091000;
			hdmitx_set_drm_pkt(tx_comm, &data);
			/* mute_us = mute_frames * hdmitx_get_frame_duration(); */
			/* usleep_range(mute_us, mute_us + 10); */
			/* step2: HDR->DV_LL */
			vsif_para.ver = 0x1;
			vsif_para.length = 0x1b;
			vsif_para.ver2_l11_flag = 0;
			vsif_para.vers.ver2.low_latency = 1;
			vsif_para.vers.ver2.dobly_vision_signal = 1;
			hdmitx_set_vsif_pkt(tx_comm, 4, 0, &vsif_para, false);
		} else if (strncmp(tmpbuf2, "sdr", 3) == 0) {
			data.features = 0x00010100;
			hdmitx_set_drm_pkt(tx_comm, &data);
		} else if (strncmp(tmpbuf2, "hdr", 3) == 0) {
			data.features = 0x00091000;
			hdmitx_set_drm_pkt(tx_comm, &data);
		} else if (strncmp(tmpbuf2, "sbtm", 4) == 0) {
			struct vtem_sbtm_st sbtm = {
				.sbtm_ver = 0x2,
				.sbtm_mode = 0x3,
				.sbtm_type = 0x1,
				.grdm_min = 0x1,
				.grdm_lum = 2,
				/* MD2/3 */
				.frmpblimitint = 0xdcba,
			};
			hdmitx_set_sbtm_pkt(tx_comm, &sbtm);
		} else if (strncmp(tmpbuf2, "hlg", 3) == 0) {
			data.features = 0x00091200;
			hdmitx_set_drm_pkt(tx_comm, &data);
		} else if (strncmp(tmpbuf2, "vsif", 4) == 0) {
			if (tmpbuf2[4] == '1' && tmpbuf2[5] == '1') {
				/* DV STD */
				vsif_para.ver = 0x1;
				vsif_para.length = 0x1b;
				vsif_para.ver2_l11_flag = 0;
				vsif_para.vers.ver2.low_latency = 0;
				vsif_para.vers.ver2.dobly_vision_signal = 1;
				hdmitx_set_vsif_pkt(tx_comm, 1, 1, &vsif_para, false);
			} else if (tmpbuf2[4] == '1' && tmpbuf2[5] == '0') {
				/* DV STD packet, but dolby_vision_signal bit cleared */
				vsif_para.ver = 0x1;
				vsif_para.length = 0x1b;
				vsif_para.ver2_l11_flag = 0;
				vsif_para.vers.ver2.low_latency = 0;
				vsif_para.vers.ver2.dobly_vision_signal = 0;
				hdmitx_set_vsif_pkt(tx_comm, 1, 1, &vsif_para, false);
			} else if (tmpbuf2[4] == '4' && tmpbuf2[5] == '1') {
				/* DV LL */
				vsif_para.ver = 0x1;
				vsif_para.length = 0x1b;
				vsif_para.ver2_l11_flag = 0;
				vsif_para.vers.ver2.low_latency = 1;
				vsif_para.vers.ver2.dobly_vision_signal = 1;
				hdmitx_set_vsif_pkt(tx_comm, 4, 0, &vsif_para, false);
			}  else if (tmpbuf2[4] == '4' && tmpbuf2[5] == '0') {
				/* DV LL packet, but dolby_vision_signal bit cleared */
				vsif_para.ver = 0x1;
				vsif_para.length = 0x1b;
				vsif_para.ver2_l11_flag = 0;
				vsif_para.vers.ver2.low_latency = 1;
				vsif_para.vers.ver2.dobly_vision_signal = 0;
				hdmitx_set_vsif_pkt(tx_comm, 4, 0, &vsif_para, false);
			} else if (tmpbuf2[4] == '0') {
				/* exit DV to SDR */
				hdmitx_set_vsif_pkt(tx_comm, 0, 0, NULL, true);
			}
		} else if (strncmp(tmpbuf2, "hdr10+", 6) == 0) {
			hdmitx_set_hdr10plus_pkt(tx_comm, 1, &hdr_data);
		} else if (strncmp(tmpbuf2, "cuva_vsif", 9) == 0) {
			hdmitx_set_cuva_hdr_vsif(tx_comm, &cuva_vsif_data);
		} else if (strncmp(tmpbuf2, "cuva_emp", 8) == 0) {
			hdmitx_set_cuva_hdr_vs_emds(tx_comm, &cuva_emp_data);
		}
	} else if (strncmp(tmpbuf, "test_attr", 9) == 0) {
		/* for pxp test */
		strncpy(test_fmt_attr, tmpbuf + 9, sizeof(test_fmt_attr));
		test_fmt_attr[15] = '\0';
		HDMITX_INFO("set test attr: %s\n", test_fmt_attr);
	} else if (strncmp(tmpbuf, "disp_mode", 9) == 0) {
		ret = set_disp_mode_debug(tx_comm, tmpbuf + 9, test_fmt_attr);
		if (ret < 0)
			HDMITX_ERROR("%s: set mode[%s]+[%s] failed\n",
				__func__, tmpbuf + 9, test_fmt_attr);
	} else if (strncmp(tmpbuf, "sspll", 5) == 0) {
		ret = kstrtoul(tmpbuf + 5, 10, &value);
		tx_comm->sspll = value & 0xFF;
		HDMITX_INFO("set sspll: %d\n", tx_comm->sspll);
	} else if (strncmp(tmpbuf, "force_frac", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		tx_comm->force_frac_mode = value & 0x3;
		HDMITX_INFO("set force_frac_mode: 0x%x\n", tx_comm->force_frac_mode);
	} else if (strncmp(tmpbuf, "venc", 4) == 0) {
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		tx_comm->enc_idx = value;
		HDMITX_INFO("set venc index: %d\n", tx_comm->enc_idx);
	}
}

static void amhdmitx_remove(struct platform_device *pdev)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(&pdev->dev);
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	struct device *dev = tx_comm->hdtx_dev;

	/* remove sysfs before uninit */
	hdmitx_sysfs_common_destroy(dev);

	/* unbind from drm */
	hdmitx_unbind_meson_drm(&pdev->dev);
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&hdmitx_early_suspend_handler);
#endif
	unregister_reboot_notifier(&tx_comm->reboot_nb);
	cancel_work_sync(&tx_comm->work_hdr);
	cancel_work_sync(&tx_comm->work_hdr_unmute);
	cancel_delayed_work(&tx_comm->work_hpd_plugout);
	cancel_delayed_work(&tx_comm->work_hpd_plugin);
	cancel_delayed_work(&tx_comm->work_internal_intr);
	destroy_workqueue(tx_comm->hdmi_hpd_wq);
	cancel_delayed_work(&tx_comm->work_rxsense);
	destroy_workqueue(tx_comm->rxsense_wq);
	cancel_delayed_work(&tx_comm->work_cedst);
	destroy_workqueue(tx_comm->cedst_wq);

	if (hw_comm->chip_data->hdmitx_ops->uninit_hw)
		hw_comm->chip_data->hdmitx_ops->uninit_hw(hw_comm);
	tx_comm->hpd_event = 0xff;
	hdmitx_vout_uninit();

#if IS_ENABLED(CONFIG_AMLOGIC_SND_SOC)
	aout_unregister_client(&tx_comm->hdmitx_notifier_nb_a);
#endif

	/* Remove the cdev */
	cdev_del(&tx_comm->cdev);
	device_destroy(hdmitx_class, tx_comm->hdmitx_id);
	class_destroy(hdmitx_class);

	unregister_chrdev_region(tx_comm->hdmitx_id, HDMI_TX_COUNT);
	hdmitx_common_destroy(tx_comm);
}

static void _amhdmitx_suspend(struct hdmitx_common *tx_comm)
{
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	bool arg = false;

	/*
	 * if HPD is high before suspend, and there were hpd
	 * plugout -> in event happened in deep suspend stage,
	 * now resume and stay in early resume stage, still
	 * need to respond to plugin irq and read/update EDID.
	 * so clear last_hpd_handle_done_stat for re-enter
	 * plugin handle. Note there may be re-enter plugout/in
	 * handler under suspend
	 */
	tx_comm->last_hpd_handle_done_stat = HDMI_TX_NONE;
	if (tx_comm->tx_hw->chip_data->chip_type < MESON_CPU_ID_T7) {
		/* drm tx22 enters AUTH_STOP, don't do hdcp22 IP reset */
		if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0)
			return;

		hdmitx_hw_cntl(hw_comm, PLATFORM_DIS_HPLL, NULL, NULL);
		if (tx_comm->debug_param.dis_dmc_req) {
			arg = false;
			hdmitx_hw_cntl(hw_comm, HDCP22_EN_DMC_REQ, (void *)&arg, NULL);
			arg = true;
			hdmitx_hw_cntl(hw_comm, HDCP_RESET, (void *)&arg, NULL);
		} else {
			arg = false;
			hdmitx_hw_cntl(hw_comm, HDCP_RESET, (void *)&arg, NULL);
		}
		arg = false;
		hdmitx_hw_cntl(hw_comm, PLATFORM_ESM_CLK_CTRL, (void *)&arg, NULL);
		HDMITX_INFO("amhdmitx: suspend and reset hdcp\n");
	}
}

/*
 * there's corner case:
 * when deep suspend, RTC wakeup kernel-->
 * hdmi plugout/in interrupt-->
 * plugin bottom handle, edid...
 * however it may re-enter RTC suspend and
 * disable hdmitx clk during hdmi register
 * access in plugin bottom handler, cause
 * system hard lock and crash. so need to keep
 * basic hdmitx clk enabled when suspend
 */
#ifdef CONFIG_PM
static int amhdmitx_suspend(struct platform_device *pdev,
			    pm_message_t state)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(&pdev->dev);
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	bool arg = false;

	hdmitx_hw_cntl(hw_comm, PLATFORM_HDMI_CLKS_CTRL, (void *)&arg, NULL);

	/*
	 * after suspend, VPU power domain will be powered off,
	 * so hdcp1.4 key otp/crc need to be loaded again
	 */
	hdmitx_hw_cntl(hw_comm, HDCP14_KEY_LOAD, (void *)&arg, NULL);
	hdmitx_event_mgr_suspend(tx_comm->event_mgr, true);
	_amhdmitx_suspend(tx_comm);

	HDMITX_INFO("amhdmitx: suspend\n");
	return 0;
}

static int amhdmitx_resume(struct platform_device *pdev)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(&pdev->dev);
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;
	bool arg = true;

	HDMITX_INFO("amhdmitx: resume\n");
	hdmitx_hw_cntl(tx_hw_base, PLATFORM_HDMI_CLKS_CTRL, (void *)&arg, NULL);
	/*
	 * Since the S7 chip, in order to optimize power consumption, it will turn off and on the
	 * vpu power domain when standby and wakes up.When it is turned off, the reg of the relevant
	 * modules will be reset. When it wakes up,the hdmitx driver needs to reinitialize the
	 * required top register.
	 */
	if (tx_hw_base->chip_data->chip_type >= MESON_CPU_ID_S7)
		hdmitx_hw_cntl(tx_hw_base, CORE_MISC_HW_INIT, (void *)&arg, NULL);
	mutex_lock(&tx_comm->hdmimode_mutex);
	hdmitx_event_mgr_suspend(tx_comm->event_mgr, false);
	/* need to update EDID in case TV changed during suspend */
	tx_comm->hpd_state = !!(hdmitx_hw_cntl(tx_hw_base, PLATFORM_GET_HPD_GPI_ST, NULL, NULL));
	if (tx_comm->hpd_state)
		hdmitx_process_plugin(tx_comm, false, false);
	else
		hdmitx_process_plugout(tx_comm);
	mutex_unlock(&tx_comm->hdmimode_mutex);

	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
	/*
	 * may resume after start hdcp22, i2c
	 * reactive will force mux to hdcp14
	 */
	if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0)
		return 0;
	hdmitx_hw_cntl(tx_hw_base, PLATFORM_ESM_CLK_CTRL, (void *)&arg, NULL);
	if (tx_comm->debug_param.dis_dmc_req)
		hdmitx_hw_cntl(tx_hw_base, HDCP22_EN_DMC_REQ, (void *)&arg, NULL);

	if (tx_hw_base->chip_data->chip_type < MESON_CPU_ID_G12A)
		hdmitx_hw_cntl(tx_hw_base, DDC_I2C_RESET, NULL, NULL);
	return 0;
}

static int amhdmitx_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	HDMITX_DEBUG("%s suspend\n", __func__);
	return amhdmitx_suspend(pdev, PMSG_SUSPEND);
}

static int amhdmitx_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	HDMITX_DEBUG("%s resume\n", __func__);
	return amhdmitx_resume(pdev);
}

static int amhdmitx_pm_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct hdmitx_common *tx_comm = dev_get_drvdata(&pdev->dev);
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;
	bool arg = false;

	if (tx_hw_base->chip_data->chip_type < MESON_CPU_ID_T7)
		return -1;
	HDMITX_INFO("%s restore\n", __func__);
	mutex_lock(&tx_comm->hdmimode_mutex);
	/*
	 * if hdmitx init and display already, then should not do
	 * HW init again as it may change clk settings, otherwise
	 * need to do hw init as did in driver probe in case HW is
	 * in power down or unknown state
	 */
	hdmitx_hw_cntl(tx_hw_base, CORE_MISC_HW_INIT, (void *)&arg, NULL);
	/*
	 * after suspend to disk and before resume, it may change TV set,
	 * need to update EDID/HPD/fmt_para by current HW status
	 * as which did in driver probe
	 */
	tx_comm->hpd_state = !!hdmitx_hw_cntl(tx_hw_base, PLATFORM_GET_HPD_GPI_ST, NULL, NULL);
	arg = !!tx_comm->hpd_state;
	hdmitx_hw_cntl(tx_hw_base, MODE_FLOW_HPD_IRQ_TOP_HALF, (void *)&arg, NULL);
	/* actions in bottom half of plug intr */
	/* need to parse EDID as vinfo need edid information */
	if (tx_comm->hpd_state)
		hdmitx_process_plugin(tx_comm, true, false);
	else
		hdmitx_process_plugout(tx_comm);
	/* load fmt para from hw info */
	hdmitx_common_init_bootup_format_para(tx_comm, &tx_comm->fmt_para);
	/* load init hdr state for HW info */
	hdmitx_hdr_state_init(tx_comm);
	hdmitx_bootup_post_process(tx_comm);
	mutex_unlock(&tx_comm->hdmimode_mutex);

	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
	return 0;
}

const struct dev_pm_ops hdmitx_pm = {
	.suspend	= amhdmitx_pm_suspend,
	.resume		= amhdmitx_pm_resume,
	/* no application for now */
	.restore    = amhdmitx_pm_restore,
};

#endif

static void amhdmitx_shutdown(struct platform_device *pdev)
{
	struct hdmitx_common *tx_comm = dev_get_drvdata(&pdev->dev);
	bool arg = false;

	_amhdmitx_suspend(tx_comm);

	if (tx_comm->tx_hw->chip_data->chip_type >= MESON_CPU_ID_T7) {
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_DISABLE, NULL, NULL);
		hdmitx_hw_cntl(tx_comm->tx_hw, PLATFORM_HDMI_CLKS_CTRL, (void *)&arg, NULL);
	}
	cancel_work_sync(&tx_comm->work_hdr);
	cancel_work_sync(&tx_comm->work_hdr_unmute);
	cancel_delayed_work(&tx_comm->work_hpd_plugout);
	cancel_delayed_work(&tx_comm->work_hpd_plugin);
	cancel_delayed_work(&tx_comm->work_internal_intr);
	cancel_delayed_work(&tx_comm->work_rxsense);
	cancel_delayed_work(&tx_comm->work_cedst);
}

static struct platform_driver amhdmitx_driver = {
	.probe	 = amhdmitx_probe,
	.remove	 = amhdmitx_remove,
#ifdef CONFIG_PM
	.suspend	= amhdmitx_suspend,
	.resume	 = amhdmitx_resume,
#endif
	.shutdown = amhdmitx_shutdown,
	.driver	 = {
		.name = DEVICE_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(meson_amhdmitx_of_match),
#ifdef CONFIG_PM
		.pm = &hdmitx_pm,
#endif
	}
};

int  __init amhdmitx_init(void)
{
	struct hdmitx_boot_param *param = get_hdmitx_boot_params();

	if (param->init_state & INIT_FLAG_NOT_LOAD) {
		HDMITX_INFO("INIT_FLAG_NOT_LOAD");
		return 0;
	}

	return platform_driver_register(&amhdmitx_driver);
}

void __exit amhdmitx_exit(void)
{
	HDMITX_INFO("amhdmitx exit\n");
	platform_driver_unregister(&amhdmitx_driver);
}
