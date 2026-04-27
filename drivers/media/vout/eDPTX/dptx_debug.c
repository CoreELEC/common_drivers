// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX_notify.h>
#include "dptx_reg_op.h"
#include "dptx_debug.h"
#include "dptx_common.h"

static int dptx_debug_info_len(int num)
{
	int ret = 0;

	if (num >= (PR_BUF_MAX - 1))
		return 0;

	ret = PR_BUF_MAX - 1 - num;
	return ret;
}

/* ************ dptx_debug_info_show ************* */
static ssize_t dptx_act_timing_info_print(struct dptx_drv_s *dptx, char *buf)
{
	u32 len = 0, n;

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n,
		"Act Timing:\n"
		"  H: period:%4u act:%4u blank:%3u bp:%3u sync:%3u fp:%3u\n"
		"  V: period:%4u act:%4u blank:%3u bp:%3u sync:%3u fp:%3u\n"
		"  PCLK:%uHZ, fr:%u.%3uhz\n\n",
		dptx->act_timing.h_period,
		dptx->act_timing.h_act,
		dptx->act_timing.h_blank,
		dptx->act_timing.h_bp,
		dptx->act_timing.h_pw,
		dptx->act_timing.h_fp,
		dptx->act_timing.v_period,
		dptx->act_timing.v_act,
		dptx->act_timing.v_blank,
		dptx->act_timing.v_bp,
		dptx->act_timing.v_pw,
		dptx->act_timing.v_fp,
		dptx->act_timing.pclk,
		dptx->act_timing.fr1000 / 1000, dptx->act_timing.fr1000 % 1000);

	return len;
}

static ssize_t dptx_link_cfg_info_print(struct dptx_drv_s *dptx, u8 port, char *buf)
{
	u32 len = 0, n;

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n,
		"LINK status:\n"
		"max_link        %u lane %u MHz\n"
		"act_link        %u lane %u MHz\n"
		"DACP_support    %u\n"
		"training_mode   %u\n"
		"sync_clk_mode   %u\n",
		dptx->sink.link[port].cap.max_lane_count,
		dptx->sink.link[port].cap.max_link_rate * 270,
		dptx->sink.link[port].lane_count, dptx->sink.link[port].link_rate * 270,
		dptx->sink.link[port].cap.DACP_support,
		dptx->panel_data.eDP.training_mode,
		dptx->sink.link[port].cap.sync_clk_mode);

	return len;
}

static ssize_t dptx_phy_cfg_info_print(struct dptx_drv_s *dptx, char *buf)
{
	u32 len = 0, n;

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n,
		"PHY status:\n"
		"  vswing:  0x%x\n"
		"  Lane[0]: sta:%u, amp:%u, preem:%u, post_cur:%u\n"
		"  Lane[1]: sta:%u, amp:%u, preem:%u, post_cur:%u\n"
		"  Lane[2]: sta:%u, amp:%u, preem:%u, post_cur:%u\n"
		"  Lane[3]: sta:%u, amp:%u, preem:%u, post_cur:%u\n\n",
		dptx->phy_cfg.vswing,
		dptx->phy_cfg.lane[0].status, dptx->phy_cfg.lane[0].amp,
		dptx->phy_cfg.lane[0].preem,  dptx->phy_cfg.lane[0].post_cur,
		dptx->phy_cfg.lane[1].status, dptx->phy_cfg.lane[1].amp,
		dptx->phy_cfg.lane[1].preem,  dptx->phy_cfg.lane[1].post_cur,
		dptx->phy_cfg.lane[2].status, dptx->phy_cfg.lane[2].amp,
		dptx->phy_cfg.lane[2].preem,  dptx->phy_cfg.lane[2].post_cur,
		dptx->phy_cfg.lane[3].status, dptx->phy_cfg.lane[3].amp,
		dptx->phy_cfg.lane[3].preem,  dptx->phy_cfg.lane[3].post_cur);

	return len;
}

static ssize_t dptx_debug_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 len = 0, n;
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n, "eDP TX driver version: %s", DPTX_DRV_VERSION);

	len += dptx_phy_cfg_info_print(dptx, buf + len);

	len += dptx_act_timing_info_print(dptx, buf + len);

	len += dptx_link_cfg_info_print(dptx, 0, buf + len);

	return len;
}

/************* dptx_debug_info_show END **************/

/************* dptx_debug_reg_dump_show **************/
static struct dptx_debug_info_reg_s dptx_debug_info_reg_t7_0 = {
	.reg_pll        = dptx_regs_pll_t7_0,
	.reg_clk        = dptx_regs_clk_t7_0,
	.reg_combo_dphy = dptx_regs_combo_dphy_t7_0,
	.reg_encl       = dptx_reg_venc_t7,
	.reg_encl_if    = dptx_reg_venc_if_t7,
	.reg_encl_data  = dptx_reg_venc_data_t7,
	.reg_vpu        = dptx_reg_vpu_t7,
	.reg_analog_phy = dptx_regs_analog_phy_t7,
	.reg_dptx_IP    = dptx_reg_dptx_IP,
};

static struct dptx_debug_info_reg_s dptx_debug_info_reg_t7_1 = {
	.reg_pll        = dptx_regs_pll_t7_1,
	.reg_clk        = dptx_regs_clk_t7_1,
	.reg_combo_dphy = dptx_regs_combo_dphy_t7_1,
	.reg_encl       = dptx_reg_venc_t7,
	.reg_encl_if    = dptx_reg_venc_if_t7,
	.reg_encl_data  = dptx_reg_venc_data_t7,
	.reg_vpu        = dptx_reg_vpu_t7,
	.reg_analog_phy = dptx_regs_analog_phy_t7,
	.reg_dptx_IP    = dptx_reg_dptx_IP,
};

static struct dptx_debug_info_reg_s dptx_debug_info_reg_a9_0 = {
	.reg_pll        = dptx_regs_pll_a9_0,
	.reg_clk        = dptx_regs_clk_a9_0,
	.reg_combo_dphy = dptx_regs_combo_dphy_a9_0,
	.reg_encl       = dptx_reg_venc_a9,
	.reg_encl_if    = dptx_reg_venc_if_a9,
	.reg_encl_data  = dptx_reg_venc_data_a9,
	.reg_vpu        = dptx_reg_vpu_a9,
	.reg_analog_phy = dptx_regs_analog_phy_a9,
	.reg_dptx_IP    = dptx_reg_dptx_IP_tr14,
};

static ssize_t dptx_regs_pr(struct dptx_drv_s *dptx, u8 port, u8 reg_t,
			    struct reg_sets_s *reg_sets, char *buf)
{
	u8 idx, str_pos = 0, reg_temp = 0;
	u32 reg_addr, reg_val, len = 0, n;

	for (idx = 0; reg_sets[idx].addr != DPTX_REG_END; idx++) {
		if (strlen(reg_sets[idx].name) > str_pos)
			str_pos = strlen(reg_sets[idx].name);
	}
	// str_pos++;

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n, "%s regs:\n", dptx_reg_type_name[reg_t]);

	for (idx = 0; reg_sets[idx].addr != DPTX_REG_END; idx++) {
		switch (reg_t) {
		case DPTX_REG_TYPE_VENC:
			reg_addr = reg_sets[idx].addr + dptx->data->offset_venc[dptx->idx];
			reg_val  = dptx_vcbus_read(reg_addr);
			break;
		case DPTX_REG_TYPE_VENC_IF:
			reg_addr = reg_sets[idx].addr + dptx->data->offset_venc_if[dptx->idx];
			reg_val  = dptx_vcbus_read(reg_addr);
			break;
		case DPTX_REG_TYPE_VENC_DATA:
			reg_addr = reg_sets[idx].addr + dptx->data->offset_venc_data[dptx->idx];
			reg_val  = dptx_vcbus_read(reg_addr);
			break;
		case DPTX_REG_TYPE_VPU:
			reg_addr = reg_sets[idx].addr;
			reg_val  = dptx_vcbus_read(reg_addr);
			break;
		case DPTX_REG_TYPE_PLL:
		case DPTX_REG_TYPE_PHY:
			reg_addr = reg_sets[idx].addr;
			reg_val  = dptx_ana_read(reg_addr);
			break;
		case DPTX_REG_TYPE_CLK:
			reg_addr = reg_sets[idx].addr;
			reg_val  = dptx_clk_read(reg_addr);
			break;
		case DPTX_REG_TYPE_DP_IP:
			reg_addr = reg_sets[idx].addr;
			reg_val  = edptx_reg_read(dptx, port, reg_addr);
			break;
		case DPTX_REG_TYPE_COMBO_DPHY:
			reg_addr = reg_sets[idx].addr;
			reg_val  = dptx_combo_dphy_read(dptx, reg_addr);
			break;
		case DPTX_REG_TYPE_DPCD_RECEIVER_CAP:
		case DPTX_REG_TYPE_DPCD_LINK_CONFIG:
		case DPTX_REG_TYPE_DPCD_LINK_STATUS:
			reg_addr = reg_sets[idx].addr;
			reg_val = 0;
			if (dptx_if_aux_read(dptx, port, reg_addr, 1, &reg_temp))
				break;
			reg_val = reg_temp;
			break;
		default:
			reg_addr = 0;
			reg_val  = 0;
			break;
		}
		n = dptx_debug_info_len(len);
		len += snprintf(buf + len, n,
			"%-*s [0x%04x]=0x%08x\n", str_pos, reg_sets[idx].name, reg_addr, reg_val);
	}

	return len;
}

static ssize_t dptx_debug_reg_dump_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	u32 len = 0;
	u8 port = 0;
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	struct dptx_debug_info_reg_s *debug_info_reg = NULL;

	switch (dptx->data->chip_type) {
	case eDPTX_CHIP_T7:
		if (dptx->idx == 0)
			debug_info_reg = &dptx_debug_info_reg_t7_0;
		else if (dptx->idx == 1)
			debug_info_reg = &dptx_debug_info_reg_t7_1;
		else
			return 0;
		break;
	case eDPTX_CHIP_A9:
		debug_info_reg = &dptx_debug_info_reg_a9_0;
		break;
	default:
		return 0;
	}

	DPTX_PR(dptx, "eDPTX driver regs:");

	if (debug_info_reg->reg_pll)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_PLL, debug_info_reg->reg_pll, buf + len);
	if (debug_info_reg->reg_clk)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_CLK, debug_info_reg->reg_clk, buf + len);
	if (debug_info_reg->reg_combo_dphy)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_COMBO_DPHY, debug_info_reg->reg_combo_dphy, buf + len);
	if (debug_info_reg->reg_encl)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_VENC, debug_info_reg->reg_encl, buf + len);
	if (debug_info_reg->reg_encl_if)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_VENC_IF, debug_info_reg->reg_encl_if, buf + len);
	if (debug_info_reg->reg_encl_data)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_VENC_DATA, debug_info_reg->reg_encl_data, buf + len);
	if (debug_info_reg->reg_vpu)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_VPU, debug_info_reg->reg_vpu, buf + len);
	if (debug_info_reg->reg_analog_phy)
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_PHY, debug_info_reg->reg_analog_phy, buf + len);
	if (debug_info_reg->reg_dptx_IP) {
		len += dptx_regs_pr(dptx, port,
			DPTX_REG_TYPE_DP_IP, debug_info_reg->reg_dptx_IP, buf + len);
		if (dptx->sink.port_mask == 0x3)
			len += dptx_regs_pr(dptx, 1,
				DPTX_REG_TYPE_DP_IP, debug_info_reg->reg_dptx_IP, buf + len);
	}

	return len;
}

void dptx_debug_reset(struct dptx_drv_s *dptx, u8 port_mask, u8 reset_part)
{
	u8 port;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_path_reset(dptx, port, reset_part);
	}
}

/************* dptx_debug_reg_dump_show END **************/

/************* dptx_debug_DPCD_dump_show **************/
static ssize_t dptx_debug_DPCD_dump_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	u32 len = 0;
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);

	if (dptx->status & DPTX_STA_LINK_ON) {
		len += dptx_regs_pr(dptx, 0,
			DPTX_REG_TYPE_DPCD_RECEIVER_CAP, dptx_reg_DPCD_receiver_cap, buf + len);
		len += dptx_regs_pr(dptx, 0,
			DPTX_REG_TYPE_DPCD_LINK_CONFIG, dptx_reg_DPCD_link_config, buf + len);
		len += dptx_regs_pr(dptx, 0,
			DPTX_REG_TYPE_DPCD_LINK_STATUS, dptx_reg_DPCD_link_status, buf + len);
	}

	return len;
}

/************* dptx_debug_DPCD_dump_show END **************/

/* ************ dptx_debug_vmode_show/store ************* */
static ssize_t dptx_debug_vmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u32 len = 0, n;
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);

	n = dptx_debug_info_len(len);
	len += snprintf(buf + len, n, "eDP vmode list:\n");

	len += dptx_print_vmode(dptx, buf + len, 0xff);

	return len;
}

static ssize_t dptx_debug_vmode_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	return count;
}

/* ************ dptx_debug_vmode_show END************* */

/* ************ dptx_debug_vinfo_show ************* */
static ssize_t dptx_debug_vinfo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

/* ************ dptx_debug_vinfo_show END************* */

/* ************ dptx_debug_help_show ************* */
static ssize_t dptx_debug_help_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	static const char *dptx_common_usage_str = {
		"Usage:\n"
		"  echo <0|1>   > HPD    -- enable/disable HPD trigger\n"
		"  echo <0|1>   > plug   -- force simulate plug/unplug\n"
		"  echo <index> > vmode  -- switch vmode(debug)\n"
		"  echo <index> > timing\n"
		"  cat help|HPD|vinfo|edid|timing|vmode|dump|plug\n"
	};
	u32 n;

	n = dptx_debug_info_len(0);
	return snprintf(buf, n, "%s\n", dptx_common_usage_str);
}

/* ************ dptx_debug_help_show END************* */

/* ************ dptx_debug_plug_store ************* */
static ssize_t dptx_debug_plug_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	unsigned int temp = 1;

	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		DPTX_ERR(dptx, "invalid data");
		return -EINVAL;
	}
	if (temp == 2) {
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, dptx);
	} else if (temp) {
		dptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, dptx);
		if (dptx->status & DPTX_STA_HPD_HIGH)
			dptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, dptx);
	} else {
		dptx_notifier_call_chain(DPTX_EVENT_PLUG_OUT, dptx);
	}

	return count;
}

/* ************ dptx_debug_plug_store END************* */

/* ************ dptx_debug_enable_store ************* */
static ssize_t dptx_debug_enable_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	unsigned int temp = 1;

	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		DPTX_ERR(dptx, "invalid data");
		return -EINVAL;
	}
	if (temp)
		dptx_notifier_call_chain(DPTX_EVENT_ENABLE, dptx);
	else
		dptx_notifier_call_chain(DPTX_EVENT_DISABLE, dptx);

	return count;
}

/* ************ dptx_debug_enable_store END************* */

/* ************ dptx_debug_pattern_store ************* */
static ssize_t dptx_debug_pattern_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	unsigned int temp = 1;

	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		DPTX_ERR(dptx, "invalid data");
		return -EINVAL;
	}
	dptx_debug_test(dptx, temp);

	return count;
}

/* ************ dptx_debug_pattern_store END**************/

/* ************ dptx_debug_training_store ************* */
static ssize_t dptx_debug_training_store(struct device *dev, struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	unsigned int temp = 1;

	int ret = 0;

	ret = kstrtouint(buf, 10, &temp);
	if (ret) {
		DPTX_ERR(dptx, "invalid data");
		return -EINVAL;
	}
	if (temp == 1)
		__dptx_fast_link_training(dptx);
	else if (temp == 2)
		__dptx_full_link_training(dptx);

	return count;
}

/* ************ dptx_debug_training_store END**************/

/************* dptx_debug_status_show **************/
static ssize_t dptx_debug_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dptx_drv_s *dptx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf, PR_BUF_MAX,
			"DPTX-%u status:\n"
			" TX-ready    : %c\n"
			" HPD-trigger : %c\n"
			" HPD high    : %c\n"
			" link-on     : %c\n"
			" display-on  : %c\n",
			dptx->idx,
			dptx->status & DPTX_STA_DRV_READY  ? '1' : '0',
			dptx->status & DPTX_STA_HPD_TRI_EN ? '1' : '0',
			dptx->status & DPTX_STA_HPD_HIGH   ? '1' : '0',
			dptx->status & DPTX_STA_LINK_ON    ? '1' : '0',
			dptx->status & DPTX_STA_DISP_ON    ? '1' : '0');

	return len;
}

/************* dptx_debug_status_show END**************/

/***** LCD debug file operation ******/
static struct device_attribute dptx_debug_attrs[] = {
	//__ATTR(HPD,    0644, dptx_debug_HPD_show,    dptx_debug_HPD_store),
	__ATTR(status,   0444, dptx_debug_status_show,    NULL),
	__ATTR(plug,     0200, NULL,                      dptx_debug_plug_store),
	__ATTR(enable,   0200, NULL,                      dptx_debug_enable_store),
	__ATTR(pattern,  0200, NULL,                      dptx_debug_pattern_store),
	//__ATTR(timing, 0644, dptx_debug_timing_show, dptx_debug_timing_store),
	__ATTR(info,     0444, dptx_debug_info_show,      NULL),
	__ATTR(reg_dump, 0444, dptx_debug_reg_dump_show,  NULL),
	__ATTR(DPCD,     0444, dptx_debug_DPCD_dump_show, NULL),
	__ATTR(vinfo,    0444, dptx_debug_vinfo_show,     NULL),
	__ATTR(help,     0444, dptx_debug_help_show,      NULL),
	//__ATTR(vinfo,  0444, dptx_debug_vinfo_show,     NULL),
	__ATTR(training, 0200, NULL,                      dptx_debug_training_store),
	//__ATTR(edid,   0444, dptx_debug_edid_show,      NULL),
	__ATTR(vmode,    0444, dptx_debug_vmode_show,     dptx_debug_vmode_store),
};

int dptx_debug_probe(struct dptx_drv_s *dptx)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(dptx_debug_attrs); i++) {
		if (device_create_file(dptx->dev, &dptx_debug_attrs[i])) {
			DPTX_ERR(dptx, "create debug attribute %s fail",
				dptx_debug_attrs[i].attr.name);
		}
	}
	return 0;
}

int dptx_debug_remove(struct dptx_drv_s *dptx)
{
	u8 i;

	for (i = 0; i < ARRAY_SIZE(dptx_debug_attrs); i++)
		device_remove_file(dptx->dev, &dptx_debug_attrs[i]);

	return 0;
}
