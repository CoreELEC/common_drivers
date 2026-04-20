/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DDR_CONTROL_H__
#define __DDR_CONTROL_H__

#include <linux/thermal.h>
#include <linux/device.h>

#define DDR_DEFAULT_POLLING_MS 500

struct ddr_caddr {
	void   __iomem *wr_addr;
	u32 wr_mask;
	u32 wr_offset;
	u32 *table;
	u32 tab_cnt;
};

struct ddr_cstep {
	int temp;
	struct ddr_caddr *caddr_tab;
	u32 caddr_cnt;
};

struct init_tab {
	void   __iomem *vaddr;
	u32 val;
};

struct ddr_thermal {
	struct thermal_zone_device *tzd;
	u32 hyst_temp;
};

struct ddr_dmc {
	void   __iomem *rd_addr;
	u32 mask;
	u32 offset;
};

struct ddr_control {
	u32 init_cnt;
	struct init_tab *init_tab;
	u32 cd_type;
	int con_cnt;
	void *condition;
	u32 cstep_cnt;
	struct ddr_cstep *ddr_cstep;
};

struct control_device {
	struct device *dev;
	struct ddr_control *ddr_control;
	u32 last_idx;
	struct delayed_work poll_dwork;
	u32 poll_time_ms;
};

extern struct platform_driver ddr_control_platdrv;

int ddr_control_device(struct control_device *control_dev);
void control_poll_work(struct work_struct *work);
void ddr_control_disable(void);
#endif
