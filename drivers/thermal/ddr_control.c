// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/printk.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/bitfield.h>
#include <linux/of_device.h>
#include <dt-bindings/thermal/ddr-params.h>
#include "ddr_control.h"

static bool ddr_control_enable = 1;
module_param(ddr_control_enable, bool, 0644);

void ddr_control_disable(void)
{
	ddr_control_enable = 0;
}

static u32 ddr_get_temperature(const struct ddr_control *control, int idx)
{
	struct ddr_cstep *ddr_cstep = control->ddr_cstep;

	if (control->cstep_cnt < 1)
		return 0;
	if (idx >= control->cstep_cnt)
		idx = control->cstep_cnt - 1;

	return ddr_cstep[idx].temp;
}

static int ddr_get_dmc_step(struct control_device *cdev)
{
	const struct ddr_control *ddr_control = cdev->ddr_control;
	const struct ddr_dmc *dmc_ctrl = ddr_control->condition;
	int idx, step;
	int vote_idx = 0;
	u32 cur_temp, pre_temp = 0;

	for (idx = 0; idx < ddr_control->con_cnt; idx++) {
		step = (readl(dmc_ctrl[idx].rd_addr) & dmc_ctrl[idx].mask);
		step = step >> dmc_ctrl[idx].offset;
		cur_temp = ddr_get_temperature(ddr_control, step);
		dev_dbg(cdev->dev, "step %d cur temp %d\n", step, cur_temp);
		if (cur_temp >= pre_temp) {
			pre_temp = cur_temp;
			vote_idx = step;
		}
	}
	return vote_idx;
}

static int ddr_get_thermal_step(struct control_device *cdev)
{
	const struct ddr_control *ddr_control = cdev->ddr_control;
	const struct ddr_thermal *thermal = ddr_control->condition;
	struct ddr_cstep *ddr_cstep = ddr_control->ddr_cstep;
	int temp = 0, ret, idx;
	int last_idx = cdev->last_idx;

	if (ddr_control->cstep_cnt == 0)
		return -EINVAL;

	ret = thermal_zone_get_temp(thermal->tzd, &temp);
	if (ret) {
		if (ret != -EAGAIN)
			dev_err(cdev->dev, "fail to read temperature %d\n", ret);
		return ret;
	}

	for (idx = 0; idx < ddr_control->cstep_cnt; idx++) {
		if (temp < ddr_cstep[idx].temp)
			break;
	}

	if (idx == ddr_control->cstep_cnt)
		idx = ddr_control->cstep_cnt - 1;

	if (last_idx > idx && last_idx < ddr_control->cstep_cnt) {
		if ((ddr_cstep[last_idx].temp - thermal->hyst_temp) < temp)
			return last_idx;
	}
	return idx;
}

static int ddr_update_caddr(struct ddr_cstep *ddr_cstep, int idx)
{
	struct ddr_caddr *ddr_caddr;
	u32 val;
	int a_idx;

	for (a_idx = 0; a_idx < ddr_cstep->caddr_cnt; a_idx++) {
		ddr_caddr = &ddr_cstep->caddr_tab[a_idx];
		val = readl(ddr_caddr->wr_addr);
		val &= ~ddr_caddr->wr_mask;
		val |= ((ddr_caddr->table[idx] << ddr_caddr->wr_offset)
			& ddr_caddr->wr_mask);

		writel(val, ddr_caddr->wr_addr);
	}
	return 0;
}

int ddr_control_device(struct control_device *cdev)
{
	const struct ddr_control *ddr_control = cdev->ddr_control;
	struct ddr_cstep *ddr_cstep = ddr_control->ddr_cstep;
	int idx;

	if (!ddr_control_enable)
		return 0;

	switch (ddr_control->cd_type) {
	case DDR_DMC:
		idx = ddr_get_dmc_step(cdev);
		if (idx < 0)
			return idx;
		break;
	case DDR_THERMAL:
		idx = ddr_get_thermal_step(cdev);
		if (idx < 0)
			return idx;
		break;
	default:
		dev_err(cdev->dev, "control state error\n");
		return -EINVAL;
	};
	if (idx == ddr_control->cstep_cnt) {
		dev_err(cdev->dev, "Not match any thermal threshold\n");
		return -EINVAL;
	}

	if (idx == cdev->last_idx)
		return 0;
	cdev->last_idx	= idx;

	ddr_update_caddr(ddr_cstep, idx);

	return 0;
}

void control_poll_work(struct work_struct *work)
{
	struct control_device *cdev = container_of(work,
					struct control_device, poll_dwork.work);

	ddr_control_device(cdev);
	mod_delayed_work(system_freezable_power_efficient_wq,
			&cdev->poll_dwork,
			msecs_to_jiffies(cdev->poll_time_ms));
}
