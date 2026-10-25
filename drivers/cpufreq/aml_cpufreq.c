// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/device.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/pm_opp.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/cpumask.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/topology.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/regulator/driver.h>
#include "opp.h"
#include "aml_cpufreq.h"
#include <linux/energy_model.h>
#include <linux/cpu_cooling.h>
#include <linux/thermal.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/proc_fs.h>
#include <linux/amlogic/gki_module.h>
#include <linux/minmax.h>

static LIST_HEAD(cluster_list);
static DEFINE_MUTEX(cluster_list_lock);
static DEFINE_MUTEX(cpufreq_target_lock);

static unsigned int freqmax[CLUSTER_MAX];
static int freqmax0_param_v2(char *buff)
{
	if (!buff)
		return -EINVAL;
	/*Example: freqmax0=1800000*/
	if (kstrtoint(buff, 0, &freqmax[0]))
		return -EINVAL;

	return 1;
}

static int freqmax1_param_v2(char *buff)
{
	if (!buff)
		return -EINVAL;
	/*Example: freqmax1=1800000*/
	if (kstrtoint(buff, 0, &freqmax[1]))
		return -EINVAL;

	return 1;
}

__setup("freqmax0=", freqmax0_param_v2);
__setup("freqmax1=", freqmax1_param_v2);

static unsigned long max_freq[CLUSTER_MAX] = {
		0, /* freq for cluster 0 */
		0  /* freq for cluster 1 */
};

static int __init get_max_freq_cluster(char *str)
{
	char *cluster_option;
	char *cluster_index;
	int cluster;
	int ret;

	if (!str) {
		pr_info("[%s] no data\n", __func__);
		return -EINVAL;
	}

	while ((cluster_option = strsep(&str, ",")) != NULL) {
		cluster_index = strsep(&cluster_option, ":");

		ret = kstrtoint(cluster_index, 10, &cluster);
		if (ret != 0 || cluster < 0 || cluster >= CLUSTER_MAX || !cluster_option) {
			pr_info("[%s] invalid data - err %d, str %s\n",
				__func__, ret, cluster_index);
			return -EINVAL;
		}

		ret = kstrtoul(cluster_option, 10, &max_freq[cluster]);
		if (ret != 0) {
			pr_info("[%s] invalid data - err %d, str %s\n",
				__func__, ret, cluster_option);
			return -EINVAL;
		}

		/* in unit kHz */
		max_freq[cluster] *= 1000;
		pr_info("[%s] - max_freq[%d] : %lu\n", __func__, cluster, max_freq[cluster]);
	}

	return 0;
}
__setup("max_freq_cluster=", get_max_freq_cluster);

static struct cluster_data *find_cluster_data_by_cpu(int cpu)
{
	struct cluster_data *pos, *ret = NULL;

	mutex_lock(&cluster_list_lock);
	list_for_each_entry(pos, &cluster_list, node) {
		if (cpumask_test_cpu(cpu, pos->cpus)) {
			ret = pos;
			break;
		}
	}
	mutex_unlock(&cluster_list_lock);

	return ret;
}

static int aml_cpufreq_verify(struct cpufreq_policy_data *pd)
{
	int ret;
	unsigned int maxfreq = pd->cpu ? freqmax[1] : freqmax[0];

	ret = cpufreq_generic_frequency_table_verify(pd);
	if (ret)
		return ret;

	if (maxfreq && maxfreq >= pd->min && maxfreq < pd->max) {
		pr_info("policy%d: set policy->max to %u from bootargs\n", pd->cpu, maxfreq);
		pd->max = maxfreq;
	}
	return ret;
}

/* find the dsu opp state:
 * i=-1: follow cpu freq volt
 * i=0: use dsu_opp_table[0]; i=1: use dsu_opp_table[1]; ......
 */
static void get_suggested_dsuopp_by_cpuopp(struct cluster_data *data,
	struct opp_node *dsuopp, struct opp_node *cpuopp)
{
	int cnt = data->dsu_opp_cnt, i;
	struct cpudsu_threshold_node *table = data->dsu_opp_table;

	if (!data->dsuclk)
		return;
	for (i = cnt; i > 0; i--)
		if (cpuopp->rate >= table[i - 1].cpurate)
			break;
	i--;

	if (i < 0) {
		dsuopp->rate = cpuopp->rate;
		dsuopp->volt = cpuopp->volt;
		dsuopp->sram_volt = cpuopp->sram_volt;
	} else {
		dsuopp->rate = table[i].dsurate;
		dsuopp->volt = table[i].dsuvolt;
		dsuopp->sram_volt = table[i].dsu_sramvolt;
	}
}

static int aml_regulator_set(struct regulator *reg, int volt, int step_mv,
			int *pre_volt, bool scaling)
{
	int ret;

	if (!reg)
		return 0;

	if (volt == 0 || *pre_volt == 0)
		return 0;

	if (scaling == SCALING_UP)
		volt = clamp(volt, *pre_volt, *pre_volt + step_mv);
	else
		volt = clamp(volt, *pre_volt - step_mv, *pre_volt);
	ret = regulator_set_voltage(reg, volt, MAX_VOLTAGE);
	if (ret) {
		regulator_set_voltage(reg, *pre_volt, MAX_VOLTAGE);
		return ret;
	}

	*pre_volt = volt;
	return 0;
}

static int aml_cpufreq_set_voltage(struct cluster_data *cdata,
					struct opp_node *oppset, struct opp_node *dsuset)
{
	struct regulator *proc_reg, *sram_reg;
	struct regulator *dproc_reg, *dsram_reg;
	struct cpufreq_vrule *vrule = cdata->vrule;
	int prev_vproc = 0, prev_vsram = 0;
	int dprev_vproc = 0, dprev_vsram = 0;
	int cpu_gap = 0, dsu_gap = 0, m_gap, udelay_time, m_step;
	int retry = 20, ret;
	int volt, step_mv;

	proc_reg = cdata->cpureg;
	sram_reg = cdata->cpu_sramreg;
	if (proc_reg && sram_reg) {
		prev_vproc = regulator_get_voltage(proc_reg);
		prev_vsram = regulator_get_voltage(sram_reg);
		regulator_set_voltage(sram_reg, prev_vsram, MAX_VOLTAGE);
		regulator_set_voltage(proc_reg, prev_vproc, MAX_VOLTAGE);
		cpu_gap = max(abs(prev_vsram - oppset->sram_volt),
					abs(prev_vproc - oppset->volt));
	}

	dproc_reg = cdata->dsureg;
	dsram_reg = cdata->dsu_sramreg;
	if (dproc_reg && dsram_reg) {
		dprev_vproc = regulator_get_voltage(dproc_reg);
		dprev_vsram = regulator_get_voltage(dsram_reg);
		regulator_set_voltage(dsram_reg, dprev_vsram, MAX_VOLTAGE);
		regulator_set_voltage(dproc_reg, dprev_vproc, MAX_VOLTAGE);
		dsu_gap = max(abs(dprev_vsram - dsuset->sram_volt),
					abs(dprev_vproc - dsuset->volt));
	}

	m_gap = max(cpu_gap, dsu_gap);
	m_step = (m_gap + vrule->step_mv - 1) / vrule->step_mv;
	if (m_step == 0) {
		udelay(vrule->delay);
		return 0;
	}
	udelay_time = m_step * vrule->comp;
	step_mv = vrule->step_mv;
	do {
		if (prev_vsram <= oppset->sram_volt && prev_vproc <= oppset->volt) {
			volt = oppset->sram_volt;
			ret = aml_regulator_set(sram_reg, volt, step_mv,
						&prev_vsram, SCALING_UP);
			if (ret)
				return ret;
			volt = oppset->volt;
			ret = aml_regulator_set(proc_reg, volt, step_mv,
						&prev_vproc, SCALING_UP);
			if (ret)
				return ret;
		} else {
			volt = oppset->volt;
			ret = aml_regulator_set(proc_reg, volt, step_mv,
						&prev_vproc, SCALING_DOWN);
			if (ret)
				return ret;
			volt = oppset->sram_volt;
			ret = aml_regulator_set(sram_reg, volt, step_mv,
						&prev_vsram, SCALING_DOWN);
			if (ret)
				return ret;
		}

		if (dprev_vsram <= dsuset->sram_volt && dprev_vproc <= dsuset->volt) {
			volt = dsuset->sram_volt;
			ret = aml_regulator_set(dsram_reg, volt, step_mv,
						&dprev_vsram, SCALING_UP);
			if (ret)
				return ret;
			volt = dsuset->volt;
			ret = aml_regulator_set(dproc_reg, volt, step_mv,
						&dprev_vproc, SCALING_UP);
			if (ret)
				return ret;
		} else {
			volt = dsuset->volt;
			ret = aml_regulator_set(dproc_reg, volt, step_mv,
						&dprev_vproc, SCALING_DOWN);
			if (ret)
				return ret;
			volt = dsuset->sram_volt;
			ret = aml_regulator_set(dsram_reg, volt, step_mv,
						&dprev_vsram, SCALING_DOWN);
			if (ret)
				return ret;
		}
		/*
		 * Delay to wait for regulator stable
		 */
		udelay(vrule->delay);
		if (--retry < 0) {
			pr_err("cluster id %d\n", cdata->clusterid);
			pr_err("prev_vproc %d prev_vsram %d set_vproc %d set_vsram %d\n",
				prev_vproc, prev_vsram, oppset->volt, oppset->sram_volt);
			pr_err("dprev_vproc %d dprev_vsram %d set_dvproc %d set_dvsram %d\n",
				dprev_vproc, dprev_vsram, dsuset->volt, dsuset->sram_volt);
		}
	} while (--m_step);
	if (udelay_time < 60)
		udelay(udelay_time);
	else
		usleep_range(udelay_time - 10, udelay_time);

	return 0;
}

static int aml_cpufreq_set_clk(struct cluster_data *cdata,
			struct opp_node *oppset, struct opp_node *dsuset)
{
	int ret;

	if (!cdata->cpuclk)
		return 0;

	ret = clk_set_rate(cdata->cpuclk, oppset->rate * 1000);
	if (ret)
		return ret;

	if (!cdata->dsuclk)
		return 0;

	if (cdata->dsuclk_shared)
		clk_set_min_rate(cdata->dsuclk, dsuset->rate * 1000);
	ret = clk_set_rate(cdata->dsuclk, dsuset->rate * 1000);
	if (ret)
		return ret;
	return 0;
}

static void aml_dsu_freq_volt_set(struct cluster_data *cdata,
	struct opp_node *dsuopp, enum cpufreq_scaling_mode mode)
{
	if (!cdata->dsuclk)
		return;
	if (cdata->dsureg && mode == SCALING_UP)
		regulator_set_voltage(cdata->dsureg, dsuopp->volt, MAX_VOLTAGE);

	if (cdata->dsuclk_shared)
		clk_set_min_rate(cdata->dsuclk, dsuopp->rate * 1000);
	clk_set_rate(cdata->dsuclk, dsuopp->rate * 1000);

	if (cdata->dsureg && mode == SCALING_DOWN)
		regulator_set_voltage(cdata->dsureg, dsuopp->volt, MAX_VOLTAGE);
}

static int aml_dvfs_algorithm_default(struct cluster_data *data,
					unsigned int index)
{
	struct opp_node oppold, *oppnew;
	struct opp_node dsuopp;
	int ret = 0;

	oppold.rate = clk_get_rate(data->cpuclk) / 1000;
	oppold.volt = data->cpureg ? regulator_get_voltage(data->cpureg) : 0;
	oppold.sram_volt = data->cpu_sramreg ? regulator_get_voltage(data->cpu_sramreg) : 0;
	oppnew = &data->opp_table[index];
	pr_debug("[cluster%d]scaling from[%lu %d] to [%lu %d]\n", data->clusterid, oppold.rate,
		oppold.volt, oppnew->rate, oppnew->volt);

	get_suggested_dsuopp_by_cpuopp(data, &dsuopp, oppnew);

	if (oppnew->rate > oppold.rate) {
		if (data->cpureg && regulator_set_voltage(data->cpureg, oppnew->volt,
			MAX_VOLTAGE)) {
			pr_err("scale voltage up to %duv failed. ret=%d\n", oppnew->volt, ret);
			goto out;
		}
		aml_dsu_freq_volt_set(data, &dsuopp, SCALING_UP);
	}

	ret = clk_set_rate(data->cpuclk, oppnew->rate * 1000);
	if (ret) {
		if (oppnew->rate > oppold.rate) {
			get_suggested_dsuopp_by_cpuopp(data, &dsuopp, &oppold);
			aml_dsu_freq_volt_set(data, &dsuopp, SCALING_DOWN);
			if (data->cpureg)
				regulator_set_voltage(data->cpureg, oppold.volt, MAX_VOLTAGE);
		}
		goto out;
	}
	if (oppnew->rate < oppold.rate) {
		aml_dsu_freq_volt_set(data, &dsuopp, SCALING_DOWN);
		if (data->cpureg && regulator_set_voltage(data->cpureg, oppnew->volt, MAX_VOLTAGE))
			goto out;
	}
	pr_debug("[cluster%d]cpuget[%lu %d],dsuget[%lu %d]\n",
		data->clusterid, clk_get_rate(data->cpuclk) / 1000,
		data->cpureg ? regulator_get_voltage(data->cpureg) : 0,
		clk_get_rate(data->dsuclk) / 1000, data->dsureg ?
		regulator_get_voltage(data->dsureg) : 0);
out:
	return ret;
}

static int __aml_cpufreq_set_target(struct cluster_data *data, unsigned int index)
{
	struct opp_node oppold, *oppnew;
	struct opp_node dsuopp;
	int ret = 0;

	oppold.rate = clk_get_rate(data->cpuclk) / 1000;
	oppold.volt = data->cpureg ? regulator_get_voltage(data->cpureg) : 0;
	oppold.sram_volt = data->cpu_sramreg ? regulator_get_voltage(data->cpu_sramreg) : 0;
	oppnew = &data->opp_table[index];
	pr_debug("[cluster%d]scaling from[%lu %d] to [%lu %d]\n", data->clusterid, oppold.rate,
		oppold.volt, oppnew->rate, oppnew->volt);

	get_suggested_dsuopp_by_cpuopp(data, &dsuopp, oppnew);

	if (oppnew->rate > oppold.rate) {
		ret = aml_cpufreq_set_voltage(data, oppnew, &dsuopp);
		if (ret)
			return ret;
	}
	ret = aml_cpufreq_set_clk(data, oppnew, &dsuopp);
	if (ret) {
		get_suggested_dsuopp_by_cpuopp(data, &dsuopp, &oppold);
		if (oppnew->rate > oppold.rate)
			aml_cpufreq_set_voltage(data, &oppold, &dsuopp);
		return ret;
	}
	if (oppnew->rate < oppold.rate) {
		ret = aml_cpufreq_set_voltage(data, oppnew, &dsuopp);
		if (ret)
			return ret;
	}
	pr_debug("[cluster%d]cpuget[%lu %d],dsuget[%lu %d]\n",
		data->clusterid, clk_get_rate(data->cpuclk) / 1000,
		data->cpureg ? regulator_get_voltage(data->cpureg) : 0,
		clk_get_rate(data->dsuclk) / 1000, data->dsureg ?
		regulator_get_voltage(data->dsureg) : 0);
	return 0;
}

static int aml_cpufreq_set_target(struct cpufreq_policy *policy,
				    unsigned int index)
{
	struct cluster_data *data = policy->driver_data;
	int ret;

	mutex_lock(&cpufreq_target_lock);
	if (data->multi_psu)
		ret = __aml_cpufreq_set_target(data, index);
	else
		ret = aml_dvfs_algorithm_default(data, index);
	mutex_unlock(&cpufreq_target_lock);

	return ret;
}

static unsigned int aml_cpufreq_get_rate(unsigned int cpu)
{
	struct cpufreq_policy policy;
	struct cluster_data *data;
	u32 rate = 0;

	if (!cpufreq_get_policy(&policy, cpu)) {
		data = policy.driver_data;
		rate = clk_get_rate(data->cpuclk) / 1000;
		pr_debug("%s: cpu: %d, cluster: %d, freq: %u\n",
			 __func__, cpu, data->clusterid, rate);
	}
	return rate;
}

static void free_clock(struct clk *clock)
{
	if (!IS_ERR_OR_NULL(clock))
		clk_put(clock);
}

static void free_regulator(struct regulator *reg)
{
	if (!IS_ERR_OR_NULL(reg))
		devm_regulator_put(reg);
}

static bool aml_setup_clock(struct cluster_data *cdata)
{
	bool ret = false;

	cdata->cpuclk = of_clk_get_by_name(cdata->np, CPUCLK);
	if (PTR_ERR_OR_ZERO(cdata->cpuclk))
		goto out;
	cdata->dsuclk = of_clk_get_by_name(cdata->np, DSUCLK);
	if (PTR_ERR_OR_ZERO(cdata->dsuclk))
		cdata->dsuclk = NULL;
	if (!cdata->dsuclk && of_property_count_strings(cdata->np, "clock-names") == 2)
		ret = false;
	else
		ret = true;

	if (cdata->dsuclk)
		cdata->dsuclk_shared = of_property_read_bool(cdata->np, "dsuclk_shared");
out:
	return ret;
}

static bool aml_setup_regulator(struct cluster_data *cdata)
{
	bool ret = false;
	char regname[25] = {0};

	cdata->skip_volt_scaling = of_property_read_bool(cdata->np, "skip_volt_scaling");
	if (cdata->skip_volt_scaling) {
		cdata->cpureg = NULL;
		cdata->dsureg = NULL;
		ret = true;
		goto out;
	}
	sprintf(regname, "cluster%d-cpu", cdata->clusterid);
	cdata->cpureg = devm_regulator_get_optional(cdata->dev, regname);
	if (PTR_ERR_OR_ZERO(cdata->cpureg)) {
		pr_debug("%s:failed to get %s regulator, ret=%ld\n", __func__, regname,
		       PTR_ERR(cdata->cpureg));
		cdata->cpureg = NULL;
		goto out;
	}

	sprintf(regname, "cluster%d-cpu-sram", cdata->clusterid);
	cdata->cpu_sramreg = devm_regulator_get_optional(cdata->dev, regname);
	if (PTR_ERR_OR_ZERO(cdata->cpu_sramreg)) {
		pr_debug("%s:failed to get %s regulator, ret=%ld\n", __func__,
			regname, PTR_ERR(cdata->cpu_sramreg));
		cdata->cpu_sramreg = NULL;
	}

	sprintf(regname, "cluster%d-dsu", cdata->clusterid);
	cdata->dsureg = devm_regulator_get_optional(cdata->dev, regname);
	if (PTR_ERR_OR_ZERO(cdata->dsureg)) {
		pr_debug("%s:failed to get %s regulator, ret=%ld\n", __func__, regname,
		       PTR_ERR(cdata->dsureg));
		cdata->dsureg = NULL;
		sprintf(regname, "cluster%d-dsu-supply", cdata->clusterid);
		if (!cdata->dsureg && of_find_property(cdata->dev->of_node, regname, NULL))
			goto out;
	}

	sprintf(regname, "cluster%d-dsu-sram", cdata->clusterid);
	cdata->dsu_sramreg = devm_regulator_get_optional(cdata->dev, regname);
	if (PTR_ERR_OR_ZERO(cdata->dsu_sramreg)) {
		pr_debug("%s:failed to get %s regulator, ret=%ld\n", __func__, regname,
		       PTR_ERR(cdata->dsu_sramreg));
		cdata->dsu_sramreg = NULL;
	}
	ret = true;
out:
	return ret;
}

static unsigned int get_cpufreq_table_index(u64 function_id,
					    u64 arg0, u64 arg1, u64 arg2)
{
	struct arm_smccc_res res;

	arm_smccc_smc((unsigned long)function_id,
		      (unsigned long)arg0,
		      (unsigned long)arg1,
		      (unsigned long)arg2,
		      0, 0, 0, 0, &res);
	return res.a0;
}

static int get_opp_index_of_policy_exit(struct cluster_data *data)
{
	struct opp_node *opp_table = data->opp_table;
	int i;

	for (i = 0; i < data->opp_cnt; i++) {
		if (opp_table[i].volt != opp_table[0].volt)
			break;
	}

	return i > 0 ? i - 1 : i;
}

static bool aml_setup_opptable(struct cluster_data *data)
{
	struct device_node *np;
	struct device *cpudev;
	struct opp_table *opptable;
	struct dev_pm_opp *opp;
	bool ret = false;
	int table_index = 0, i = 0, j, cpu;
	u32 rate, rate1, volt, nums;
	char dsutable_name[20] = {0};
	int dsuopp_len;

	np = data->np;
	data->pdvfs_enabled = of_property_read_bool(np, "pdvfs_enabled");
	if (data->pdvfs_enabled)
		table_index = get_cpufreq_table_index(GET_DVFS_TABLE_INDEX,
					      data->clusterid, 0, 0);
	data->table_index = table_index;

	/*one of the cluster cpus may be removed from device tree for isolation,
	 *so we need to get opp-table from other cpus of the cluster
	 */
	for_each_cpu(cpu, data->cpus) {
		cpudev = get_cpu_device(cpu);
		if (dev_pm_opp_of_add_table_indexed(cpudev, table_index))
			continue;
		else
			break;
	}

	if (dev_pm_opp_init_cpufreq_table(cpudev, &data->freq_table)) {
		pr_err("%s:failed to init cpufreq table,cluster:%d\n", __func__, data->clusterid);
		goto out;
	}

	for (j = 0; data->freq_table[j].frequency != CPUFREQ_TABLE_END &&
	     max_freq[data->clusterid]; j++) {
		if (data->freq_table[j].frequency > max_freq[data->clusterid]) {
			pr_info("dvfs [%s] - cluster %d freq %d\n",
				__func__, data->clusterid,
				data->freq_table[j].frequency);

			data->freq_table[j].frequency = CPUFREQ_TABLE_END;
		}
	}
	data->opp_cnt = dev_pm_opp_get_opp_count(cpudev);
	data->opp_table = kmalloc_array(data->opp_cnt, sizeof(*data->opp_table), GFP_KERNEL);
	opptable = dev_pm_opp_get_opp_table(cpudev);
	if (!data->opp_table || !opptable)
		goto out;
	mutex_lock(&opptable->lock);
	list_for_each_entry(opp, &opptable->opp_list, node) {
		if (!opp->available)
			continue;
		pr_debug("%lu %lu\n", opp->rates[0], opp->supplies[0].u_volt);
		data->opp_table[i].rate = opp->rates[0] / 1000;
		data->opp_table[i].volt = opp->supplies[0].u_volt;
		if (of_property_read_bool(opp->np, "opp-microvolt-sram")) {
			of_property_read_u32(opp->np, "opp-microvolt-sram", &volt);
			data->opp_table[i].sram_volt = volt;
		}
		i++;
	}
	dev_pm_opp_put_opp_table(opptable);
	mutex_unlock(&opptable->lock);
	data->exit_index = get_opp_index_of_policy_exit(data);

	if (!data->dsuclk)
		return true;

	if (data->dsureg) {
		dsuopp_len = 3;
		sprintf(dsutable_name, "dsu-opp-table%d", data->table_index);
	} else {
		dsuopp_len = 2;
		sprintf(dsutable_name, "dsu-opp-table");
	}

	if (!of_property_read_u32(np, "dsu-opp-nums", &nums)) {
		dsuopp_len = nums;
		sprintf(dsutable_name, "dsu-opp-table%d", data->table_index);
	}

	data->dsu_opp_cnt = of_property_count_u32_elems(np, dsutable_name) / dsuopp_len;
	if (data->dsu_opp_cnt <= 0)
		goto out;
	data->dsu_opp_table = kmalloc_array(data->dsu_opp_cnt,
		sizeof(*data->dsu_opp_table), GFP_KERNEL);
	if (IS_ERR(data->dsu_opp_table)) {
		pr_err("failed to alloc dsu_opp_table.\n");
		data->dsu_opp_table = NULL;
		goto out;
	}
	for (i = 0; i < data->dsu_opp_cnt; i++) {
		if (of_property_read_u32_index(np, dsutable_name, dsuopp_len * i, &rate))
			goto out;
		if (of_property_read_u32_index(np, dsutable_name, dsuopp_len * i + 1, &rate1))
			goto out;
		data->dsu_opp_table[i].cpurate = rate;
		data->dsu_opp_table[i].dsurate = rate1;
		if (data->dsureg) {
			if (of_property_read_u32_index(np, dsutable_name, dsuopp_len * i + 2,
				&volt))
				goto out;
			data->dsu_opp_table[i].dsuvolt = volt;
		}
		if (data->dsu_sramreg) {
			if (of_property_read_u32_index(np, dsutable_name, dsuopp_len * i + 2 + 1,
				&volt))
				goto out;
			data->dsu_opp_table[i].dsu_sramvolt = volt;
		}
	}

	return true;
out:
	return ret;
}

static int opptable_show(struct seq_file *m, void *v)
{
	struct cluster_data *data = m->private;
	int i;

	seq_printf(m, "pdvfs_en: %d\n", data->pdvfs_enabled);
	seq_printf(m, "table_index: %d\n", data->table_index);

	seq_puts(m, "opp table:\n");
	for (i = 0; i < data->opp_cnt; i++) {
		if (data->cpu_sramreg)
			seq_printf(m, "%lu %d %d\n", data->opp_table[i].rate,
				data->opp_table[i].volt, data->opp_table[i].sram_volt);
		else
			seq_printf(m, "%lu %d\n", data->opp_table[i].rate,
				data->opp_table[i].volt);
	}

	if (!data->dsuclk)
		return 0;
	seq_puts(m, "dsu_opp table:\n");
	for (i = 0; i < data->dsu_opp_cnt; i++) {
		if (data->dsureg && data->dsu_sramreg)
			seq_printf(m, "%lu %lu %d %d\n", data->dsu_opp_table[i].cpurate,
				data->dsu_opp_table[i].dsurate, data->dsu_opp_table[i].dsuvolt,
				data->dsu_opp_table[i].dsu_sramvolt);
		else if (data->dsureg)
			seq_printf(m, "%lu %lu %d\n", data->dsu_opp_table[i].cpurate,
				data->dsu_opp_table[i].dsurate, data->dsu_opp_table[i].dsuvolt);
		else
			seq_printf(m, "%lu %lu\n", data->dsu_opp_table[i].cpurate,
				data->dsu_opp_table[i].dsurate);
	}
	return 0;
}

static int aml_opptable_open(struct inode *inode, struct file *file)
{
	return single_open(file, opptable_show, pde_data(inode));
}

static const struct proc_ops aml_opptable_fops = {
	.proc_open = aml_opptable_open,
	.proc_read = seq_read,
	.proc_release = single_release,
};

static void aml_create_proc_files(struct cluster_data *data)
{
	char policy_name[10] = {0};

	snprintf(policy_name, sizeof(policy_name), "policy%u", cpumask_first(data->cpus));
	data->proccur = proc_mkdir(policy_name, data->procroot);
	if (data->proccur)
		proc_create_data("opp-table", 0444, data->proccur,
			&aml_opptable_fops, data);
}

static void aml_validate_cpu_voltage(struct cluster_data *data)
{
	int cnt = data->opp_cnt;
	int dsu_cnt = data->dsu_opp_cnt;
	int max = data->opp_table[cnt - 1].volt;
	int min = data->opp_table[0].volt;
	int cpuvolt_old, dsuvolt_old, volt;

	if (data->skip_volt_scaling)
		return;

	cpuvolt_old = regulator_get_voltage(data->cpureg);
	if (cpuvolt_old > max)
		cpuvolt_old = max;
	else if (cpuvolt_old < min)
		cpuvolt_old = min;

	regulator_set_voltage(data->cpureg, cpuvolt_old, MAX_VOLTAGE);

	if (data->cpu_sramreg) {
		max = data->opp_table[cnt - 1].sram_volt;
		min = data->opp_table[0].sram_volt;
		volt = clamp(regulator_get_voltage(data->cpu_sramreg), min, max);
		regulator_set_voltage(data->cpu_sramreg, volt, MAX_VOLTAGE);
	}

	if (!data->dsureg)
		return;

	min = data->opp_table[0].volt;
	dsuvolt_old = regulator_get_voltage(data->dsureg);
	if (dsuvolt_old > data->dsu_opp_table[dsu_cnt - 1].dsuvolt)
		dsuvolt_old = data->dsu_opp_table[dsu_cnt - 1].dsuvolt;
	else if (dsuvolt_old < min)
		dsuvolt_old = min;

	regulator_set_voltage(data->dsureg, dsuvolt_old, MAX_VOLTAGE);

	if (!data->dsu_sramreg)
		return;
	max = data->dsu_opp_table[dsu_cnt - 1].dsu_sramvolt;
	min = data->opp_table[0].sram_volt;
	volt = clamp(regulator_get_voltage(data->dsu_sramreg), min, max);
	regulator_set_voltage(data->dsu_sramreg, volt, MAX_VOLTAGE);
}

static void aml_setup_policy_rest(struct cluster_data *data)
{
	struct device_node *np = data->np;
	struct cpufreq_policy *policy = data->policy;

	if (of_property_read_u32(np, "transition_delay_us", &policy->transition_delay_us))
		policy->transition_delay_us = 10000;
	if (of_property_read_u32(np, "suspend-freq", &policy->suspend_freq))
		policy->suspend_freq = data->opp_table[data->opp_cnt - 1].rate;

	policy->cur = clk_get_rate(data->cpuclk) / 1000;
	cpumask_copy(policy->cpus, data->cpus);
}

static int aml_cpufreq_init(struct cpufreq_policy *policy)
{
	struct cluster_data *data = find_cluster_data_by_cpu(policy->cpu);

	data->policy = policy;
	policy->driver_data = data;
	policy->freq_table = data->freq_table;

	aml_create_proc_files(data);
	aml_setup_policy_rest(data);
	pr_info("cpufreq v2 policy%d init succeed.\n", cpumask_first(data->cpus));

	return 0;
}

static void destroy_aml_cpufreq_proc_files(struct cpufreq_policy *policy)
{
	char policy_name[10] = {0};
	struct cluster_data *data = policy->driver_data;

	snprintf(policy_name, sizeof(policy_name), "policy%u", cpumask_first(data->cpus));
	remove_proc_entry("opp-table", data->proccur);
	remove_proc_entry(policy_name, data->procroot);
}

static void aml_cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cluster_data *data = policy->driver_data;

	if (!data->suspended)
		aml_cpufreq_set_target(policy, data->exit_index);
	destroy_aml_cpufreq_proc_files(policy);
}

static int aml_cpufreq_suspend(struct cpufreq_policy *policy)
{
	struct cluster_data *data = policy->driver_data;

	data->suspended = true;
	if (policy->cdev)
		dev_set_uevent_suppress(&policy->cdev->device, true);

	return cpufreq_generic_suspend(policy);
}

static int aml_cpufreq_resume(struct cpufreq_policy *policy)
{
	struct cluster_data *data = policy->driver_data;
	struct cpufreq_frequency_table *pos;
	unsigned int original_freq = policy->suspend_freq;

	data->suspended = false;
	if (policy->cdev)
		dev_set_uevent_suppress(&policy->cdev->device, false);

	/* a step through the entry one below restores the full speed */
	cpufreq_for_each_entry(pos, policy->freq_table) {
		if (pos->frequency == original_freq)
			break;
		policy->suspend_freq = pos->frequency;
	}
	cpufreq_generic_suspend(policy);
	policy->suspend_freq = original_freq;

	return cpufreq_generic_suspend(policy);
}

static void aml_cpufreq_ready(struct cpufreq_policy *policy)
{
	/*
	 * NOTE: Some chips only adjust the frequency but not the voltage,
	 * which may lead to the same "cost" calculated in the energy model.
	 * In the energy model, a frequency with the same "cost" and lower
	 * frequency is labeled as "EM_PERF_STATE_INEFFICIENT".
	 * policy->efficiencies_available is also set to true. By default,
	 * cpufreq will filter out frequencies marked as inefficient when
	 * calculating the frequency for the next adjustment. This causes
	 * cpufreq to remain at the highest frequency even when cpufreq governor
	 * is set to "schedutil" and the load is low.
	 *
	 * Here, policy->efficiencies_available is force set to false to avoid
	 * this situation.
	 */
	policy->efficiencies_available = false;
}

static struct cpufreq_driver aml_cpufreq_driver = {
	.name			= "aml-cpufreq",
	.flags			= CPUFREQ_IS_COOLING_DEV |
					CPUFREQ_HAVE_GOVERNOR_PER_POLICY |
					CPUFREQ_NEED_INITIAL_FREQ_CHECK |
					CPUFREQ_NEED_UPDATE_LIMITS,
	.verify			= aml_cpufreq_verify,
	.target_index	= aml_cpufreq_set_target,
	.get			= aml_cpufreq_get_rate,
	.init			= aml_cpufreq_init,
	.exit			= aml_cpufreq_exit,
	.attr			= cpufreq_generic_attr,
	.suspend		= aml_cpufreq_suspend,
	.resume			= aml_cpufreq_resume,
	.register_em	= cpufreq_register_em_with_opp,
	.ready			= aml_cpufreq_ready,
};

static int aml_cpuclk_ready(struct device_node *np)
{
	int ret;
	struct clk *cpuclk;

	cpuclk = of_clk_get_by_name(np, CPUCLK);
	ret = PTR_ERR_OR_ZERO(cpuclk);
	if (ret == -EPROBE_DEFER)
		pr_err("cpu clock not ready, retry!\n");
	else if (ret)
		pr_err("failed to get clock:%d!\n", ret);
	else
		clk_put(cpuclk);

	return ret;
}

static int aml_cpureg_ready(struct device *dev, struct device_node *np, int clusterid)
{
	int ret;
	struct regulator *cpureg;
	char cpureg_name[15] = {0};

	if (of_property_read_bool(np, "skip_volt_scaling"))
		return 0;
	sprintf(cpureg_name, "cluster%d-cpu", clusterid);
	cpureg = devm_regulator_get_optional(dev, cpureg_name);
	ret = PTR_ERR_OR_ZERO(cpureg);
	if (ret == -EPROBE_DEFER)
		pr_err("cpu regulator not ready, retry!\n");
	else if (ret)
		pr_err("failed to get cpu regulator:%d!\n", ret);
	else
		devm_regulator_put(cpureg);

	return ret;
}

static void aml_cpufreq_voltage_rule(struct cluster_data *c_data)
{
	struct cpufreq_vrule *vrule;
	u32 value[3];
	int ret;

	vrule = devm_kzalloc(c_data->dev, sizeof(*vrule), GFP_KERNEL);
	c_data->vrule = vrule;
	ret = of_property_read_u32_array(c_data->np, "cpufreq-voltage-rules", value, 3);
	if (ret) {
		vrule->step_mv = 30000;
		vrule->delay = 40;
		vrule->comp = 20;
		return;
	}
	vrule->step_mv = value[0];
	vrule->delay = value[1];
	vrule->comp = value[2];
}

static int aml_cpufreq_probe(struct platform_device *pdev)
{
	struct proc_dir_entry *root = NULL;
	struct device_node *cluster_np;
	struct cluster_data *clusterdata;
	char cluster_name[10] = {0};
	int ret = -1, i = 0, j, count;

	while (1) {
		sprintf(cluster_name, "cluster%d", i);
		cluster_np = of_get_child_by_name(pdev->dev.of_node, cluster_name);
		if (!cluster_np)
			break;

		ret = aml_cpuclk_ready(cluster_np);
		if (ret)
			return ret;

		ret = aml_cpureg_ready(&pdev->dev, cluster_np, i);
		if (ret)
			return ret;
		i++;
	}
	count = i;
	if (count == 0)
		goto out;

	clusterdata = devm_kzalloc(&pdev->dev, sizeof(*clusterdata) * count, GFP_KERNEL);
	if (!clusterdata)
		goto out;

	root = proc_mkdir("aml_cpufreq", NULL);
	if (!root)
		goto out;

	for (i = 0; i < count; i++) {
		sprintf(cluster_name, "cluster%d", i);
		cluster_np = of_get_child_by_name(pdev->dev.of_node, cluster_name);

		clusterdata[i].clusterid = i;
		clusterdata[i].np = cluster_np;
		clusterdata[i].dev = &pdev->dev;
		clusterdata[i].procroot = root;
		/*setup cluster related cpus*/
		of_property_for_each_u32(cluster_np, "cluster_cores", j) {
			if (cpu_present(j)) {
				cpumask_set_cpu(j, clusterdata[i].cpus);
				pr_debug("cpu%d->cluster%d\n", j,
					clusterdata[i].clusterid);
			}
		}

		if (!aml_setup_clock(&clusterdata[i]))
			goto free;

		if (!aml_setup_regulator(&clusterdata[i]))
			goto free;

		if (!aml_setup_opptable(&clusterdata[i]))
			goto free;

		aml_validate_cpu_voltage(&clusterdata[i]);
		clusterdata[i].multi_psu = of_property_read_bool(cluster_np, "multi-psu");
		if (clusterdata[i].multi_psu)
			aml_cpufreq_voltage_rule(&clusterdata[i]);

		mutex_lock(&cluster_list_lock);
		list_add_tail(&clusterdata[i].node, &cluster_list);
		mutex_unlock(&cluster_list_lock);
	}

	ret = cpufreq_register_driver(&aml_cpufreq_driver);
	if (ret)
		pr_err("%s: Failed to register platform driver, err: %d\n", __func__, ret);
	else
		goto out;
free:
	for (i = 0; i < count; i++) {
		kfree(clusterdata[i].opp_table);
		kfree(clusterdata[i].dsu_opp_table);
		free_regulator(clusterdata[i].cpureg);
		free_regulator(clusterdata[i].dsureg);
		free_clock(clusterdata[i].cpuclk);
		free_clock(clusterdata[i].dsuclk);
	}
	devm_kfree(&pdev->dev, clusterdata);
	proc_remove(root);
out:
	return ret;
}

static void aml_cpufreq_remove(struct platform_device *pdev)
{
	cpufreq_unregister_driver(&aml_cpufreq_driver);
}

static const struct of_device_id aml_cpufreq_dt_match[] = {
	{	.compatible = "amlogic, aml-cpufreq",
	},
	{},
};

static struct platform_driver aml_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-aml",
		.owner  = THIS_MODULE,
		.of_match_table = aml_cpufreq_dt_match,
	},
	.probe		= aml_cpufreq_probe,
	.remove_new		= aml_cpufreq_remove,
};

static int __init aml_cpufreq_v2_init(void)
{
	return platform_driver_register(&aml_cpufreq_platdrv);
}

static void __exit aml_cpufreq_v2_exit(void)
{
	platform_driver_unregister(&aml_cpufreq_platdrv);
}

module_init(aml_cpufreq_v2_init);
module_exit(aml_cpufreq_v2_exit);
MODULE_LICENSE("GPL v2");
