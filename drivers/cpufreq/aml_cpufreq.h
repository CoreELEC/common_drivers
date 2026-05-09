/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_CPUFREQ_H
#define __MESON_CPUFREQ_H

#define CPUCLK             "cpuclk"
#define DSUCLK             "dsuclk"

#define CLUSTER_MAX                    2
#define GET_DVFS_TABLE_INDEX           0x82000088
#define MAX_VOLTAGE	                   0x7fffffff

struct opp_node {
	unsigned long rate; //unit khz
	int volt; //unit uv
	int sram_volt; //unit uv
};

/*this is dsu vote related structure read from dts*/
struct cpudsu_threshold_node {
	unsigned long cpurate; //unit khz
	unsigned long dsurate; //unit khz
	int dsuvolt; //init uv
	int dsu_sramvolt; //init uv
};

enum cpufreq_scaling_mode {
	SCALING_UP,
	SCALING_DOWN,
	SCALING_NUM
};

struct cpufreq_vrule {
	int step_mv;
	int delay;
	int comp;
};

struct cluster_data {
	int clusterid; //cluster driver data id
	cpumask_var_t cpus; //cluster related cpus(online+offline)
	struct device *dev; //point to dvfs driver pdev->dev
	struct device_node *np; //dvfs cluster device node

	struct clk *cpuclk;
	struct clk *dsuclk;
	struct regulator *cpureg;
	struct regulator *cpu_sramreg;
	struct regulator *dsureg;
	struct regulator *dsu_sramreg;
	struct cpufreq_vrule *vrule;
	bool skip_volt_scaling;
	bool suspended;
	bool dsuclk_shared;

	int opp_cnt;
	struct opp_node *opp_table;
	struct cpufreq_frequency_table *freq_table;
	int dsu_opp_cnt;
	struct cpudsu_threshold_node *dsu_opp_table;
	struct cpufreq_policy *policy;     //cpufreq policy
	struct proc_dir_entry *procroot, *proccur;
	bool pdvfs_enabled;
	bool multi_psu;
	int table_index;
	int exit_index;  //policy exit frequency index in opp_table

	struct list_head node;
};

#endif /* __MESON_CPUFREQ_H */
