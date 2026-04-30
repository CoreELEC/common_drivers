// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include<linux/time.h>
#include<linux/time64.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include "cw2015.h"

int cw_capacity = 100;
static int dbg_enable;
module_param_named(battery_dbg_level, dbg_enable, int, 0644);

#define cw_printk(fmt, args...) \
	do { \
		if (dbg_enable) { \
			pr_info("[BATTERY] " fmt, ##args); \
		} \
	} while (0)

/*Define CW2015 iic read function*/
static int cw_read(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 1, buf);
	return ret;
}

/*Define CW2015 iic write function*/
static int cw_write(struct i2c_client *client, unsigned char reg, unsigned char const buf[])
{
	int ret = 0;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 1, &buf[0]);
	return ret;
}

/*Define CW2015 iic read word function*/
static int cw_read_word(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
	return ret;
}

int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int i;
	unsigned char reset_val;

	cw_printk("[FGADC] test config_info = 0x%x\n",
		cw_bat->plat_data.cw_bat_config_info[0]);

	/* make sure no in sleep mode */
	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;

	reset_val = reg_val;
	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
		dev_err(&cw_bat->client->dev, "device in sleep mode, cannot update battery info\n");
		return -1;
	}

	/* update new battery info */
	for (i = 0; i < SIZE_BATINFO; i++) {
		cw_printk("0x%x\n", cw_bat->plat_data.cw_bat_config_info[i]);
		ret = cw_write(cw_bat->client, REG_BATINFO + i,
			(u8 *)&cw_bat->plat_data.cw_bat_config_info[i]);

		if (ret < 0)
			return ret;
	}

	reg_val = 0x00;
	reg_val |= CONFIG_UPDATE_FLG;
	reg_val &= 0x07;	/* clear ATHD */
	reg_val |= ATHD;	/* set ATHD */
	ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	msleep(50);

	/* reset */
	reg_val = 0x00;
	reset_val &= ~(MODE_RESTART);
	reg_val = reset_val | MODE_RESTART;
	ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0)
		return ret;

	usleep_range(10000, 11000);
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;

	msleep(100);
	cw_printk("cw2015 update config success!\n");

	return 0;
}

static int cw_init(struct cw_battery *cw_bat)
{
	int ret;
	int i;
	unsigned char reg_val = MODE_SLEEP; reg_val = MODE_SLEEP;

	if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
		reg_val = MODE_NORMAL;
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
		if (ret < 0)
			return ret;
	}

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if ((reg_val & 0xf8) != ATHD) {
		dev_info(&cw_bat->client->dev, "the new ATHD have not set\n");
		reg_val &= 0x07;	/* clear ATHD */
		reg_val |= ATHD;	/* set ATHD */
		ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
		if (ret < 0)
			return ret;
	}

	ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
	if (ret < 0)
		return ret;

	if (!(reg_val & CONFIG_UPDATE_FLG)) {
		cw_printk("update config flg is true, need update config\n");
		ret = cw_update_config_info(cw_bat);
		if (ret < 0) {
			dev_info(&cw_bat->client->dev,
				"update flag for new battery info have not set\n");
			return ret;
		}
	} else {
		for (i = 0; i < SIZE_BATINFO; i++) {
			ret = cw_read(cw_bat->client, (REG_BATINFO + i), &reg_val);
			if (ret < 0)
				return ret;

			if (cw_bat->plat_data.cw_bat_config_info[i] != reg_val)
				break;
		}

		if (i != SIZE_BATINFO) {
			reg_val = MODE_SLEEP;
			ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
			if (ret < 0)
				return ret;

			msleep(30);
			reg_val = MODE_NORMAL;
			ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
			if (ret < 0)
				return ret;

			cw_printk("config didn't match, need update config\n");
			ret = cw_update_config_info(cw_bat);
			if (ret < 0)
				return ret;
		}
	}

	usleep_range(10000, 11000);

	for (i = 0; i < 30; i++) {
		ret = cw_read(cw_bat->client, REG_SOC, &reg_val);
		if (ret < 0)
			return ret;
		else if (reg_val <= 0x64)
			break;
		msleep(120);
	}

	if (i >= 30) {
		reg_val = MODE_SLEEP;
		ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
		cw_printk("cw2015 input invalid power error, cw2015 join sleep mode\n");
		return -1;
	}

	cw_printk("cw2015 init success!\n");
	return 0;
}

static int check_chrg_usb_psy(struct device *dev, void *data)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct cw_battery *cw_bat = (struct cw_battery *)data;

	if (psy->desc->type == POWER_SUPPLY_TYPE_USB) {
		cw_bat->chrg_usb_psy = psy;
		return 1;
	}
	return 0;
}

static int check_chrg_ac_psy(struct device *dev, void *data)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct cw_battery *cw_bat = (struct cw_battery *)data;

	if (psy->desc->type == POWER_SUPPLY_TYPE_MAINS) {
		cw_bat->chrg_ac_psy = psy;
		return 1;
	}
	return 0;
}

static void get_chrg_psy(struct cw_battery *cw_bat)
{
	if (!cw_bat->chrg_usb_psy)
		power_supply_for_each_device(&cw_bat, check_chrg_usb_psy);
	if (!cw_bat->chrg_ac_psy)
		power_supply_for_each_device(&cw_bat, check_chrg_ac_psy);
}

static int get_charge_state(struct cw_battery *cw_bat)
{
	union power_supply_propval val;
	int ret = -ENODEV;
	int usb_online = 0;
	int ac_online = 0;
	struct power_supply *chrg_usb_psy;
	struct power_supply *chrg_ac_psy;

	if (!cw_bat->chrg_usb_psy || !cw_bat->chrg_ac_psy)
		get_chrg_psy(cw_bat);

	chrg_usb_psy = cw_bat->chrg_usb_psy;
	chrg_ac_psy = cw_bat->chrg_ac_psy;
	if (chrg_usb_psy) {
		ret = chrg_usb_psy->desc->get_property(chrg_usb_psy,
								POWER_SUPPLY_PROP_ONLINE, &val);
		if (!ret)
			usb_online = val.intval;
	}
	if (chrg_ac_psy) {
		ret = chrg_ac_psy->desc->get_property(chrg_ac_psy,
								POWER_SUPPLY_PROP_ONLINE, &val);
		if (!ret)
			ac_online = val.intval;
	}

	if (ac_online || usb_online)
		return 1;

	return 0;
}

static int cw_por(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reset_val;

	reset_val = MODE_SLEEP;
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;
	reset_val = MODE_NORMAL;
	usleep_range(10000, 11000);
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;
	ret = cw_init(cw_bat);
	if (ret)
		return ret;
	return 0;
}

static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int ui_100 = UI_FULL;
	int remainder = 0;
	int real_SOC = 0;
	int digit_SOC = 0;
	int UI_SOC = 0;
	int ret;
	unsigned char reg_val[2];
	static int reset_loop;
	static int charging_zero_loop;

	ret = cw_read_word(cw_bat->client, REG_SOC, reg_val);
	if (ret < 0)
		return ret;

	real_SOC = reg_val[0];
	digit_SOC = reg_val[1];

	/*case 1 : avoid IC error, read SOC > 100*/
	if (real_SOC < 0 || real_SOC > 100) {
		cw_printk("Error:  real_SOC = %d\n", real_SOC);
		reset_loop++;
		if (reset_loop > (BATTERY_CAPACITY_ERROR / queue_delayed_work_time)) {
			cw_por(cw_bat);
			reset_loop = 0;
		}
		return cw_bat->capacity;
	}

	reset_loop = 0;

	/* case 2 : avoid IC error, battery SOC is 0% when long time charging */
	if (cw_bat->charger_mode > 0 && real_SOC == 0) {
		charging_zero_loop++;
		if (charging_zero_loop > BATTERY_CHARGING_ZERO / queue_delayed_work_time) {
			cw_por(cw_bat);
			charging_zero_loop = 0;
		}
	}
	if (charging_zero_loop != 0)
		charging_zero_loop = 0;

	UI_SOC = ((real_SOC * 256 + digit_SOC) * 100) / (ui_100 * 256);
	remainder = (((real_SOC * 256 + digit_SOC) * 100 * 100) / (ui_100 * 256)) % 100;
	/*case 3 : avoid swing*/
	if (UI_SOC >= 100) {
		UI_SOC = 100;
	} else {
		if ((remainder > 70 || remainder < 30) && (UI_SOC >= (cw_bat->capacity - 1)) &&
				(UI_SOC <= (cw_bat->capacity + 1)))
			UI_SOC = cw_bat->capacity;
	}

#ifdef CONFIG_PM
	if (cw_bat->suspend_resume_mark == 1)
		cw_bat->suspend_resume_mark = 0;
#endif
	return UI_SOC;
}

static int cw_get_voltage(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val[2];
	u16 value16, value16_1, value16_2, value16_3;
	int voltage;

	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0)
		return ret;
	value16 = (reg_val[0] << 8) + reg_val[1];

	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0)
		return ret;
	value16_1 = (reg_val[0] << 8) + reg_val[1];

	ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
	if (ret < 0)
		return ret;
	value16_2 = (reg_val[0] << 8) + reg_val[1];

	if (value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	if (value16_1 > value16_2) {
		value16_3 = value16_1;
		value16_1 = value16_2;
		value16_2 = value16_3;
	}

	if (value16 > value16_1) {
		value16_3 = value16;
		value16 = value16_1;
		value16_1 = value16_3;
	}

	voltage = value16_1 * 625 / 2048;

	if (DOUBLE_SERIES_BATTERY)
		voltage = voltage * 2;
	return voltage;
}

/*This function called when get RRT from cw2015*/
static void cw_get_time_to_empty(struct cw_battery *cw_bat)
{
	int ret;
	u8 reg_val;
	unsigned char alrt;

	ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
	if (ret < 0)
		return;
	alrt = reg_val & 0x80;
	//printk("ATHD = %2x ,alrt = %2x ---cw_bat->capacity =%d\n"
	//,(ATHD >> 3),alrt,cw_bat->capacity);
	if (alrt > 0 && cw_bat->capacity > (ATHD >> 3)) {
		reg_val = reg_val & 0x7f;
		ret = cw_write(cw_bat->client, REG_RRT_ALERT, &reg_val);
		if (ret)
			pr_err("cw2015 write REG_RRT_ALERT clr reg fail\n");
		else
			cw_bat->bat_change = 1;
	}
}

static void cw_update_charge_status(struct cw_battery *cw_bat)
{
	int cw_charger_mode;

	cw_charger_mode = get_charge_state(cw_bat);
	if (cw_bat->charger_mode != cw_charger_mode) {
		cw_bat->charger_mode = cw_charger_mode;
		cw_bat->bat_change = 1;
		if (cw_charger_mode)
			cw_bat->charge_count++;
	}
}

static void cw_update_capacity(struct cw_battery *cw_bat)
{
	cw_capacity = cw_get_capacity(cw_bat);
	if (cw_capacity >= 0 && cw_capacity <= 100 &&
		cw_bat->capacity != cw_capacity) {
		cw_bat->capacity = cw_capacity;
		cw_bat->bat_change = 1;
	}
}

static void cw_update_vol(struct cw_battery *cw_bat)
{
	int ret;

	ret = cw_get_voltage(cw_bat);
	if (ret >= 0 && cw_bat->voltage != ret) {
		cw_bat->voltage = ret;
		cw_bat->bat_change = 1;
	}
}

static void cw_update_status(struct cw_battery *cw_bat)
{
	int status;

	if (cw_bat->charger_mode > 0) {
		if (cw_bat->capacity >= 100)
			status = POWER_SUPPLY_STATUS_FULL;
		else
			status = POWER_SUPPLY_STATUS_CHARGING;
	} else {
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	}

	if (cw_bat->status != status) {
		cw_bat->status = status;
		cw_bat->bat_change = 1;
	}
}

static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret;
	u8 reg_val;
	int i = 0;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	/* Add for battery swap start */
	ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
	if (ret < 0) {
		cw_bat->capacity = 100;
		cw_bat->voltage = 4200;
		cw_bat->bat_change = 1;
	} else {
		if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
			for (i = 0; i < 5; i++) {
				if (cw_por(cw_bat) == 0)
					break;
			}
		}
		cw_update_capacity(cw_bat);
		cw_update_vol(cw_bat);
		cw_update_charge_status(cw_bat);
		cw_update_status(cw_bat);
		cw_get_time_to_empty(cw_bat);
	}
	/* Add for battery swap end */
	cw_printk("capacity = %d\n", cw_bat->capacity);
	cw_printk("voltage = %d\n", cw_bat->voltage);
	cw_printk("charger_mod = %d\n", cw_bat->charger_mode);
	cw_printk("status = %d\n", cw_bat->status);
	cw_printk("bat_change = %d\n", cw_bat->bat_change);
	cw_printk("charge_count = %d\n", cw_bat->charge_count);

#ifdef CONFIG_PM
	if (cw_bat->suspend_resume_mark == 1)
		cw_bat->suspend_resume_mark = 0;
#endif

	if (cw_bat->bat_change == 1) {
		power_supply_changed(cw_bat->rk_bat);
		cw_bat->bat_change = 0;
	}
	cw_printk("bat_change = %d\n", cw_bat->bat_change);

	queue_delayed_work(cw_bat->battery_workqueue,
			&cw_bat->battery_delay_work,
				msecs_to_jiffies(cw_bat->monitor_sec));
}

static int cw_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;
	struct cw_battery *cw_bat;

	cw_bat = power_supply_get_drvdata(psy);
	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = cw_bat->capacity;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = cw_bat->voltage <= 0 ? 0 : 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = cw_bat->voltage * 1000;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static enum power_supply_property cw_battery_properties[] = {
	POWER_SUPPLY_PROP_CAPACITY,
    /* POWER_SUPPLY_PROP_STATUS, */
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static int cw_init_data(struct cw_battery *cw_bat)
{
	unsigned char reg_SOC[2];
	int real_SOC = 0;
	int digit_SOC = 0;
	int ret = -1;
	int UI_SOC = 0;

	ret = cw_read_word(cw_bat->client, REG_SOC, reg_SOC);

	if (ret < 0)
		return ret;

	real_SOC = reg_SOC[0];
	digit_SOC = reg_SOC[1];
	UI_SOC = ((real_SOC * 256 + digit_SOC) * 100) / (UI_FULL * 256);

	if (UI_SOC >= 100)
		UI_SOC = 100;

	cw_update_vol(cw_bat);
	cw_update_charge_status(cw_bat);
	cw_bat->capacity = UI_SOC;
	cw_update_status(cw_bat);
	return 0;
}

static const struct power_supply_desc cw2015_bat_desc = {
	.name		= "battery",
	.type		= POWER_SUPPLY_TYPE_BATTERY,
	.properties	= cw_battery_properties,
	.num_properties	= ARRAY_SIZE(cw_battery_properties),
	.get_property	= cw_battery_get_property,
};

#ifdef CONFIG_OF
static int cw2015_parse_dt(struct cw_battery *cw_bat)
{
	struct device *dev = &cw_bat->client->dev;
	struct device_node *node = dev->of_node;
	struct property *prop;
	int length;
	u32 value;
	int ret;
	struct cw_bat_platform_data *data = &cw_bat->plat_data;
	struct gpio_desc *hw_id0_io;
	struct gpio_desc *hw_id1_io;
	int hw_id0_val;
	int hw_id1_val;

	if (!node)
		return -ENODEV;

	memset(data, 0, sizeof(*data));

	ret = of_property_read_u32(node, "hw_id_check", &value);
	if (!ret && value) {
		hw_id0_io = gpiod_get_optional(dev, "hw-id0", GPIOD_IN);
		if (!hw_id0_io)
			return -EINVAL;
		if (IS_ERR(hw_id0_io))
			return PTR_ERR(hw_id0_io);

		hw_id0_val = gpiod_get_value(hw_id0_io);
		gpiod_put(hw_id0_io);

		hw_id1_io = gpiod_get_optional(dev, "hw-id1", GPIOD_IN);
		if (!hw_id1_io)
			return -EINVAL;
		if (IS_ERR(hw_id1_io))
			return PTR_ERR(hw_id1_io);

		hw_id1_val = gpiod_get_value(hw_id1_io);
		gpiod_put(hw_id1_io);

		/*
		 * ID1 = 0, ID0 = 1 : Battery
		 * ID1 = 1, ID0 = 0 : Dual Battery
		 * ID1 = 0, ID0 = 0 : Adapter
		 */
		if (hw_id0_val == 1 && hw_id1_val == 0)
			cw_bat->dual_battery = 0;
		else if (hw_id0_val == 0 && hw_id1_val == 1)
			cw_bat->dual_battery = 1;
		else
			return -EINVAL;
	}

	/* determine the number of config info */
	prop = of_find_property(node, "bat_config_info", &length);
	if (!prop)
		return -EINVAL;

	length /= sizeof(u32);

	if (length > 0) {
		size_t size = sizeof(*data->cw_bat_config_info) * length;

		data->cw_bat_config_info = devm_kzalloc(dev, size, GFP_KERNEL);
		if (!data->cw_bat_config_info)
			return -ENOMEM;

		ret = of_property_read_u32_array(node, "bat_config_info",
						data->cw_bat_config_info, length);
		if (ret < 0)
			return ret;
	}

	cw_bat->bat_mode = MODE_BATTARY;
	cw_bat->monitor_sec = DEFAULT_MONITOR_SEC * TIMER_MS_COUNTS;

	ret = of_property_read_u32(node, "divider_res1", &value);
	if (ret < 0)
		value = 0;
	data->divider_res1 = value;

	ret = of_property_read_u32(node, "divider_res2", &value);
	if (ret < 0)
		value = 0;
	data->divider_res2 = value;

	ret = of_property_read_u32(node, "virtual_power", &value);
	if (ret < 0)
		value = 0;
	cw_bat->bat_mode = value;

	ret = of_property_read_u32(node, "monitor_sec", &value);
	if (ret < 0)
		dev_err(dev, "monitor_sec missing!\n");
	else
		cw_bat->monitor_sec = value * TIMER_MS_COUNTS;
	pr_info("cw_bat->monitor_sec = %d\n", cw_bat->monitor_sec);

	ret = of_property_read_u32(node, "design_capacity", &value);
	if (ret < 0) {
		dev_err(dev, "design_capacity missing!\n");
		data->design_capacity = 2000;
	} else {
		data->design_capacity = value;
	}
	return 0;
}
#else
static int cw2015_parse_dt(struct cw_battery *cw_bat)
{
	return -ENODEV;
}
#endif

static int cw_bat_probe(struct i2c_client *client)
{
	int ret;
	int loop = 0;
	struct cw_battery *cw_bat;
	struct power_supply_config psy_cfg = {0};

	cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
	if (!cw_bat)
		return -ENOMEM;

	i2c_set_clientdata(client, cw_bat);
	cw_bat->client = client;

	ret = cw2015_parse_dt(cw_bat);
	if (ret < 0) {
		dev_err(&client->dev, "failed to find cw2015 platform data\n");
		return -1;
	}

	cw_bat->capacity = 1;
	cw_bat->voltage = 0;
	cw_bat->status = 0;
	cw_bat->suspend_resume_mark = 0;
	cw_bat->charger_mode = NO_CHARGING;
	cw_bat->bat_change = 0;

	ret = cw_init(cw_bat);
	while ((loop++ < 3) && (ret != 0)) {
		msleep(200);
		ret = cw_init(cw_bat);
	}
	if (ret) {
		cw_printk("cw2015 init fail!\n");
		return ret;
	}

	ret = cw_init_data(cw_bat);
	if (ret) {
		cw_printk("cw2015 init data fail!\n");
		return ret;
	}

	psy_cfg.drv_data = cw_bat;

	cw_bat->rk_bat = power_supply_register(&client->dev, &cw2015_bat_desc, &psy_cfg);
	if (IS_ERR(cw_bat->rk_bat)) {
		dev_err(&cw_bat->client->dev, "power supply register rk_bat error\n");
		return -1;
	}

	cw_bat->battery_workqueue = create_singlethread_workqueue("rk_battery");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->battery_workqueue,
			&cw_bat->battery_delay_work, msecs_to_jiffies(50));

	dev_info(&cw_bat->client->dev, "cw2015/cw2013 driver v1.2 probe success\n");
	return 0;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	//read_persistent_clock(&cw_bat->suspend_time_before);
	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cw_bat->suspend_resume_mark = 1;
	//read_persistent_clock(&cw_bat->after);
	cw_bat->after = timespec64_sub(cw_bat->after,
				cw_bat->suspend_time_before);
	queue_delayed_work(cw_bat->battery_workqueue,
				&cw_bat->battery_delay_work, msecs_to_jiffies(2));
	return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
	.suspend  = cw_bat_suspend,
	.resume   = cw_bat_resume,
};
#endif

static void cw_bat_remove(struct i2c_client *client)
{
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cancel_delayed_work(&cw_bat->battery_delay_work);
}

static const struct i2c_device_id cw_bat_id_table[] = {
	{"cw201x", 0},
	{}
};

static struct i2c_driver cw_bat_driver = {
	.driver = {
		.name = "cw201x",
#ifdef CONFIG_PM
		.pm = &cw_bat_pm_ops,
#endif
	},
	.probe = cw_bat_probe,
	.remove = cw_bat_remove,
	.id_table = cw_bat_id_table,
};

int __init cw2015_battery_init(void)
{
	return i2c_add_driver(&cw_bat_driver);
}

void __exit cw2015_battery_exit(void)
{
	i2c_del_driver(&cw_bat_driver);
}

MODULE_AUTHOR("xhc<xhc@rock-chips.com>");
MODULE_DESCRIPTION("cw2015/cw2013 battery driver");
MODULE_LICENSE("GPL");
