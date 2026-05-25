// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/pm_domain.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
/* Amlogic Headers */
#include <linux/amlogic/major.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

/* Local Headers */
#include "vicp_log.h"
#include "vicp_module.h"

static struct vicp_device_s vicp_device;

int vicp_dev_init(struct vicp_device_s vicp_dev)
{
	vicp_device = vicp_dev;
	return 0;
}

int vicp_clock_config(int is_enable)
{
	int ret = 0;
	struct vicp_clock_config_s config;

	vicp_print(VICP_INFO, "enter %s: value = %d.\n", __func__, is_enable);

	config = vicp_device.clock_cfg;
	if (config.clk_count < 0) {
		pr_err("count clock-names err.\n");
		return -1;
	}

	if (config.enable == is_enable) {
		pr_err("same clock status, no need set again.\n");
		return 0;
	}

	vicp_device.clock_cfg.enable = is_enable;

	if (is_enable) {
		if (IS_ERR_OR_NULL(config.clk_gate)) {
			pr_err("vicp gate clock is null.\n");
			ret = -1;
		} else {
			clk_set_rate(config.clk_gate, config.gate_rate);
			clk_prepare_enable(config.clk_gate);
			vicp_print(VICP_INFO, "vicp gate clock is %luMHZ.\n",
				clk_get_rate(config.clk_gate) / 1000000);
			ret = 0;
		}

		if (config.clk_count > 1) {
			if (IS_ERR_OR_NULL(config.clk_vapb0)) {
				pr_err("vicp vapb0 clock is null.\n");
				ret = -1;
			} else {
				clk_set_rate(config.clk_vapb0, config.vapb0_rate);
				clk_prepare_enable(config.clk_vapb0);
				vicp_print(VICP_INFO, "vicp vapb0 clock is %luMHZ.\n",
					clk_get_rate(config.clk_vapb0) / 1000000);
				ret = 0;
			}
		}

		if (config.clk_count > 2) {
			if (IS_ERR_OR_NULL(config.clk_cve)) {
				pr_err("vicp cve clock is null.\n");
				ret = -1;
			} else {
				clk_set_rate(config.clk_cve, config.cve_rate);
				clk_prepare_enable(config.clk_cve);
				vicp_print(VICP_INFO, "vicp cve clock is %luMHZ.\n",
					clk_get_rate(config.clk_cve) / 1000000);
				ret = 0;
			}
		}
	} else {
		vicp_print(VICP_INFO, "debug, no need disab clock.\n");
		if (IS_ERR_OR_NULL(config.clk_gate)) {
			pr_err("vicp gate clock is null.\n");
			ret = -1;
		} else {
			clk_disable_unprepare(config.clk_gate);
			vicp_print(VICP_INFO, "disable vicp gate clock.\n");
			ret = 0;
		}

		if (config.clk_count > 1) {
			if (IS_ERR_OR_NULL(config.clk_vapb0)) {
				pr_err("vicp vapb0 clock is null.\n");
				ret = -1;
			} else {
				clk_disable_unprepare(config.clk_vapb0);
				vicp_print(VICP_INFO, "disable vicp vapb0 clock.\n");
				ret = 0;
			}
		}

		if (config.clk_count > 2) {
			if (IS_ERR_OR_NULL(config.clk_cve)) {
				pr_err("vicp cve clock is null.\n");
				ret = -1;
			} else {
				clk_disable_unprepare(config.clk_cve);
				vicp_print(VICP_INFO, "disable vicp cve clock.\n");
				ret = 0;
			}
		}
	}
	return ret;
}

int vicp_pwr_init(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
		return -1;
	}

	if (!of_property_read_bool(dev->of_node, "power-domains"))
		return -EINVAL;

	pm_runtime_enable(dev);
	pm_runtime_set_autosuspend_delay(dev, VICP_AUTO_POWER_OFF_DELAY);
	pm_runtime_use_autosuspend(dev);

	vicp_clock_config(0);

	return 0;
}

void vicp_pwr_remove(struct device *dev)
{
	if (IS_ERR_OR_NULL(dev)) {
		vicp_print(VICP_ERROR, "%s: invalid param.\n", __func__);
	} else {
		pm_runtime_disable(dev);
		dev_pm_domain_detach(dev, true);
	}
}

void vicp_runtime_pwr(int enable)
{
	int ret = -1;

	vicp_print(VICP_INFO, "enter %s: value = %d.\n", __func__, enable);

	if (enable) {
		vicp_clock_config(1);
		ret = pm_runtime_get_sync(&vicp_device.pdev->dev);
		if (ret < 0)
			vicp_print(VICP_ERROR, "runtime get power error\n");
	} else {
		vicp_clock_config(0);
		pm_runtime_mark_last_busy(&vicp_device.pdev->dev);
		pm_runtime_put_autosuspend(&vicp_device.pdev->dev);
	}
}

unsigned long get_buf_phy_addr(u32 buf_fd)
{
	struct dma_buf *dbuf = NULL;
	unsigned long phy_addr = 0;
	struct sg_table *table = NULL;
	struct page *page = NULL;
	struct dma_buf_attachment *attach = NULL;

	dbuf = dma_buf_get(buf_fd);
	if (IS_ERR_OR_NULL(dbuf)) {
		pr_err("%s: get phyaddr failed: fd is %d.\n", __func__, buf_fd);
		return -EINVAL;
	}

	attach = dma_buf_attach(dbuf, vicp_device.dev);
	if (IS_ERR_OR_NULL(attach)) {
		dma_buf_put(dbuf);
		pr_err("%s: attach err\n", __func__);
		return -EINVAL;
	}

	table = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("%s: get table failed.\n", __func__);
		dma_buf_detach(dbuf, attach);
		return -EINVAL;
	}

	page = sg_page(table->sgl);
	phy_addr = PFN_PHYS(page_to_pfn(page));
	dma_buf_unmap_attachment(attach, table, DMA_BIDIRECTIONAL);
	dma_buf_detach(dbuf, attach);
	dma_buf_put(dbuf);

	return phy_addr;
}

int config_vicp_param(struct vicp_data_info_s *vicp_data_info,
	struct vicp_data_config_s *data_config)
{
	if (IS_ERR_OR_NULL(vicp_data_info) || IS_ERR_OR_NULL(data_config)) {
		pr_err("%s: NULL param, please check.\n", __func__);
		return -1;
	}

	data_config->input_data.is_vframe = false;
	data_config->input_data.data_dma.buf_addr = get_buf_phy_addr(vicp_data_info->src_buf_fd);
	data_config->input_data.data_dma.buf_stride_w = vicp_data_info->src_buf_alisg_w;
	data_config->input_data.data_dma.buf_stride_h = vicp_data_info->src_buf_alisg_h;
	data_config->input_data.data_dma.color_format = vicp_data_info->src_color_fmt;
	data_config->input_data.data_dma.color_depth = vicp_data_info->src_color_depth;
	data_config->input_data.data_dma.data_width = vicp_data_info->src_data_w;
	data_config->input_data.data_dma.data_height = vicp_data_info->src_data_h;
	data_config->input_data.data_dma.plane_count = 2;
	data_config->input_data.data_dma.endian = vicp_data_info->src_endian;
	data_config->input_data.data_dma.need_swap_cbcr = vicp_data_info->src_swap_cbcr;

	data_config->output_data.fbc_out_en = false;
	data_config->output_data.mif_out_en = true;
	data_config->output_data.mif_color_fmt = vicp_data_info->dst_color_fmt;
	data_config->output_data.mif_color_dep = vicp_data_info->dst_color_depth;
	data_config->output_data.phy_addr[0] = get_buf_phy_addr(vicp_data_info->dst_buf_fd);
	data_config->output_data.stride[0] = vicp_data_info->dst_buf_w;
	data_config->output_data.width = vicp_data_info->dst_buf_w;
	data_config->output_data.height = vicp_data_info->dst_buf_h;
	data_config->output_data.endian = vicp_data_info->dst_endian;
	data_config->output_data.need_swap_cbcr = vicp_data_info->dst_swap_cbcr;
	data_config->output_data.out_sig_fmt = VFRAME_SIGNAL_FMT_SDR;

	data_config->data_option.crop_info.left = vicp_data_info->crop_x;
	data_config->data_option.crop_info.top = vicp_data_info->crop_y;
	data_config->data_option.crop_info.width = vicp_data_info->crop_w;
	data_config->data_option.crop_info.height = vicp_data_info->crop_h;
	data_config->data_option.output_axis.left = vicp_data_info->output_x;
	data_config->data_option.output_axis.top = vicp_data_info->output_y;
	data_config->data_option.output_axis.width = vicp_data_info->output_w;
	data_config->data_option.output_axis.height = vicp_data_info->output_h;
	data_config->data_option.rotation_mode = vicp_data_info->rotation_mode;
	data_config->data_option.rdma_enable = vicp_data_info->rdma_enable;
	data_config->data_option.security_enable = vicp_data_info->security_enable;
	data_config->data_option.shrink_mode = vicp_data_info->shrink_mode;
	data_config->data_option.skip_mode = vicp_data_info->skip_mode;
	data_config->data_option.input_source_count = vicp_data_info->input_source_count;
	data_config->data_option.input_source_number = vicp_data_info->input_source_number;

	return 0;
}
