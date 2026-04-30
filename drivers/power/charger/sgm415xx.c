// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ macros  */
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/regulator/machine.h>
#include <generated/uapi/linux/version.h>
#include <linux/usb/phy.h>
#include <linux/phy/phy.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#endif
#include <linux/miscdevice.h>
#include <linux/bitfield.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include "sgm415xx.h"
#include <linux/reboot.h>
#include <linux/iio/consumer.h>
#include <linux/iio/iio.h>

#define SGM_DRV_VERSION		"V1.2"
#define SGM_LOG		"CHARGER "

static int dbg_enable;
module_param_named(charger_dbg_level, dbg_enable, int, 0644);

#define SGM_INFO(fmt, args...) \
	do { \
		if (dbg_enable) { \
			pr_info("[CHARGER] " fmt, ##args); \
		} \
	} while (0)

#define SGM415XX_IOC_MAGIC		'S'
#define SGM415XX_CMD_READ_REG	_IOR(SGM415XX_IOC_MAGIC, 0, int)
#define SGM415XX_CMD_WRITE_REG	_IOW(SGM415XX_IOC_MAGIC, 1, int)
#define SGM415XX_CMD_SET_CHG_EN	_IOW(SGM415XX_IOC_MAGIC, 2, int)
#define SGM415XX_CMD_GET_STATUS	_IOR(SGM415XX_IOC_MAGIC, 3, int)
#define SGM415XX_CMD_SET_SHIP	_IOR(SGM415XX_IOC_MAGIC, 4, int)

struct sgm415xx_device {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *charger;
	struct mutex lock; //state lock
	struct mutex i2c_rw_lock; // i2c read/write lock

	int device_id;
	int chg_type;
	int psy_usb_type;
	enum SGM415XX_POWER_TYPE power_type;

	int set_vbus;
	int force_detect_count;
	int qc2_en;
	int qc3_en;

	struct sgm415xx_init_data init_data;
	struct sgm415xx_state state;
	u32 watchdog_timer;

	struct delayed_work charge_detect_delayed_work;
	struct delayed_work charge_monitor_work;
	struct delayed_work hvdcp_work;
	struct delayed_work force_detect_dwork;

	struct wakeup_source *charger_wakelock;
	struct miscdevice miscdev;

	int chg_stat_gpio;
	struct iio_channel *adc_ch;
};

/**********************************************************
 *
 *	 [I2C Slave Setting]
 *
 *********************************************************/

#define SGM415XX_REG_NUM	(0x0F)

static struct sgm_vendor_info s_chg_vendor[MAX_PN_ID] = {
	[SGM41513_PN_ID]   = { "sgm41513", 0},
	[SGM41513D_PN_ID]  = { "sgm41513D", BIT(3)},
	[SGM41515_PN_ID]   = { "sgm41515", 0},
	[SGM41515D_PN_ID]  = { "sgm41515D", BIT(3)},
	[SGM41516_PN_ID]   = { "sgm41516", (BIT(6) | BIT(5))},
	[SGM41516D_PN_ID]  = { "sgm41516D", (BIT(6) | BIT(5) | BIT(3))},
	[SGM41541_PN_ID]   = { "sgm41541", (BIT(6) | BIT(5))},
	[SGM41542_PN_ID]   = { "sgm41542", (BIT(6) | BIT(5) | BIT(3))},
	[SGM41543_PN_ID]   = { "sgm41543", BIT(6)},
	[SGM41543D_PN_ID]  = { "sgm41543D", (BIT(6) | BIT(3))},
	[SGM41512S_PN_ID]  = { "sgm41512S", (BIT(6) | BIT(5) | BIT(4) | BIT(3)) },
	[SGM41512SD_PN_ID] = { "sgm41512SD", (BIT(6) | BIT(5) | BIT(4))},
};

static const char *const sgm415xx_ntc_fault[] = {
	"Normal",
	"Reserved",
	"warm",
	"cool",
	"reserved",
	"cold",
	"hot",
};

#ifdef OTG_ENABLE
/* SGM415XX REG06 BOOST_LIM[5:4], uV */
static const unsigned int BOOST_VOLT_LIMIT[] = {
	4850000, 5000000, 5150000, 5300000
};

/* SGM415XX REG02 BOOST_LIM[7:7], uA */
static const unsigned int BOOST_CURRENT_LIMIT[MAX_PN_ID][2] = {
	{500000, 1200000}, {500000, 1200000}, {500000, 1200000}, {500000, 1200000},
	{1200000, 2000000}, {1200000, 2000000}, {1200000, 2000000}, {1200000, 2000000},
};
#endif

static const unsigned int IPRECHG_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

static const unsigned int ITERM_CURRENT_STABLE[] = {
	5000, 10000, 15000, 20000, 30000, 40000, 50000, 60000,
	80000, 100000, 120000, 140000, 160000, 180000, 200000, 240000
};

/**********************************************************
 *
 *	[I2C Function For Read/Write sgm415xx]
 *
 *********************************************************/
static int __sgm415xx_read_byte(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(sgm->client, reg);
	if (ret < 0) {
		pr_err(SGM_LOG "i2c read fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*data = (u8)ret;

	return 0;
}

static int __sgm415xx_write_byte(struct sgm415xx_device *sgm, int reg, u8 val)
{
	s32 ret;

	ret = i2c_smbus_write_byte_data(sgm->client, reg, val);
	if (ret < 0) {
		pr_err(SGM_LOG "i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
			val, reg, ret);
		return ret;
	}
	return 0;
}

static int sgm415xx_read_reg(struct sgm415xx_device *sgm, u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm415xx_read_byte(sgm, reg, data);
	mutex_unlock(&sgm->i2c_rw_lock);

	return ret;
}

static int sgm415xx_write_reg(struct sgm415xx_device *sgm, u8 reg, u8 val)
{
	int ret;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm415xx_write_byte(sgm, reg, val);
	mutex_unlock(&sgm->i2c_rw_lock);

	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

	return ret;
}

static int sgm415xx_update_bits(struct sgm415xx_device *sgm, u8 reg, u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&sgm->i2c_rw_lock);
	ret = __sgm415xx_read_byte(sgm, reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __sgm415xx_write_byte(sgm, reg, tmp);
	if (ret)
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);

out:
	mutex_unlock(&sgm->i2c_rw_lock);
	return ret;
}

/**********************************************************
 *
 *	 [Internal Function]
 *
 *********************************************************/

static int sgm415xx_set_watchdog_timer(struct sgm415xx_device *sgm, int time)
{
	u8 reg_val;

	if (time == 0)
		reg_val = SGM415XX_WDT_TIMER_DISABLE;
	else if (time == 40)
		reg_val = SGM415XX_WDT_TIMER_40S;
	else if (time == 80)
		reg_val = SGM415XX_WDT_TIMER_80S;
	else
		reg_val = SGM415XX_WDT_TIMER_160S;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_5, SGM415XX_WDT_TIMER_MASK,
			reg_val << SGM415XX_WDT_TIMER_SHIFT);
}

__maybe_unused static int sgm415xx_get_term_curr(struct sgm415xx_device *sgm, u32 *uA)
{
	int ret;
	u8 reg_val;
	int curr;
	int offset = SGM415XX_TERMCHRG_I_MIN_uA;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_3, &reg_val);
	if (ret)
		return ret;

	reg_val &= SGM415XX_TERMCHRG_CUR_MASK;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID)
		curr = ITERM_CURRENT_STABLE[reg_val];
	else
		curr = reg_val * SGM415XX_TERMCHRG_CURRENT_STEP_uA + offset;

	*uA = curr;
	return 0;
}

static int sgm415xx_set_term_curr(struct sgm415xx_device *sgm, u32 uA)
{
	u8 reg_val;

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		for (reg_val = 1; reg_val < 16 && uA >= ITERM_CURRENT_STABLE[reg_val]; reg_val++)
			;
		reg_val--;
	} else {
		if (uA < SGM415XX_TERMCHRG_I_MIN_uA)
			uA = SGM415XX_TERMCHRG_I_MIN_uA;
		else if (uA > SGM415XX_TERMCHRG_I_MAX_uA)
			uA = SGM415XX_TERMCHRG_I_MAX_uA;

		reg_val = (uA - SGM415XX_TERMCHRG_I_MIN_uA) / SGM415XX_TERMCHRG_CURRENT_STEP_uA;
	}

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3, SGM415XX_TERMCHRG_CUR_MASK, reg_val);
}

__maybe_unused static int sgm415xx_get_prechrg_curr(struct sgm415xx_device *sgm)
{
	int ret;
	u8 reg_val;
	int curr;
	int offset = SGM415XX_PRECHRG_I_MIN_uA;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_3, &reg_val);
	if (ret)
		return ret;

	reg_val = FIELD_GET(SGM415XX_PRECHRG_CUR_MASK, reg_val);

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID)
		curr = IPRECHG_CURRENT_STABLE[reg_val];
	else
		curr = reg_val * SGM415XX_PRECHRG_CURRENT_STEP_uA + offset;

	return curr;
}

static int sgm415xx_set_prechrg_curr(struct sgm415xx_device *sgm, int uA)
{
	u8 reg_val;

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		for (reg_val = 1; reg_val < 16 && uA >= IPRECHG_CURRENT_STABLE[reg_val]; reg_val++)
			;
		reg_val--;
	} else {
		if (uA < SGM415XX_PRECHRG_I_MIN_uA)
			uA = SGM415XX_PRECHRG_I_MIN_uA;
		else if (uA > SGM415XX_PRECHRG_I_MAX_uA)
			uA = SGM415XX_PRECHRG_I_MAX_uA;

		reg_val = (uA - SGM415XX_PRECHRG_I_MIN_uA) / SGM415XX_PRECHRG_CURRENT_STEP_uA;
	}

	reg_val = reg_val << 4;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_3, SGM415XX_PRECHRG_CUR_MASK, reg_val);
}

static int sgm415xx_get_vindpm_offset_os(struct sgm415xx_device *sgm)
{
	int ret;
	u8 reg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_f, &reg_val);
	if (ret)
		return ret;

	reg_val = reg_val & SGM415XX_VINDPM_OS_MASK;

	return reg_val;
}

#ifdef OTG_ENABLE
static int sgm415xx_enable_otg(struct sgm415xx_device *sgm, bool en)
{
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1,
		SGM415XX_OTG_EN, en ? SGM415XX_OTG_EN : 0);
}

__maybe_unused static int sgm415xx_set_boost_voltage_limit(struct sgm415xx_device *sgm, u32 uV)
{
	char reg_val = -1;
	int i = 0;

	while (i < 4) {
		if (uV == BOOST_VOLT_LIMIT[i]) {
			reg_val = i;
			break;
		}
		i++;
	}
	if (reg_val < 0)
		return reg_val;

	reg_val = reg_val << 4;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6, SGM415XX_BOOSTV, reg_val);
}

__maybe_unused static int sgm415xx_set_boost_current_limit(struct sgm415xx_device *sgm, u32 uA)
{
	int ret = 0;

	if (uA == BOOST_CURRENT_LIMIT[sgm->device_id][0])
		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM, 0);
	else if (uA == BOOST_CURRENT_LIMIT[sgm->device_id][1])
		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_BOOST_LIM, BIT(7));
	else
		dev_err(sgm->dev, "current isn't range\n");
	return ret;
}
#endif

static int sgm415xx_get_ichg_curr(struct sgm415xx_device *sgm, u32 *uA)
{
	int ret;
	u8 ichg;
	u32 curr;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_2, &ichg);
	if (ret) {
		dev_err(sgm->dev, "%s,fail\n", __func__);
		return ret;
	}

	ichg &= SGM415XX_ICHRG_I_MASK;
	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		if (ichg <= 0x8)
			curr = ichg * 5000;
		else if (ichg <= 0xF)
			curr = 40000 + (ichg - 0x8) * 10000;
		else if (ichg <= 0x17)
			curr = 110000 + (ichg - 0xF) * 20000;
		else if (ichg <= 0x20)
			curr = 270000 + (ichg - 0x17) * 30000;
		else if (ichg <= 0x30)
			curr = 540000 + (ichg - 0x20) * 60000;
		else if (ichg <= 0x3C)
			curr = 1500000 + (ichg - 0x30) * 120000;
		else
			curr = 3000000;
	} else {
		curr = ichg * SGM415XX_ICHRG_I_STEP_uA;
	}

	*uA = curr;
	return 0;
}

static int sgm415xx_set_ichrg_curr(struct sgm415xx_device *sgm, unsigned int uA)
{
	u8 reg_val;

	if (uA < SGM415XX_ICHRG_I_MIN_uA)
		uA = SGM415XX_ICHRG_I_MIN_uA;
	else if (uA > sgm->init_data.max_ichg)
		uA = sgm->init_data.max_ichg;

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		if (uA <= 40000)
			reg_val = uA / 5000;
		else if (uA <= 110000)
			reg_val = 0x08 + (uA - 40000) / 10000;
		else if (uA <= 270000)
			reg_val = 0x0F + (uA - 110000) / 20000;
		else if (uA <= 540000)
			reg_val = 0x17 + (uA - 270000) / 30000;
		else if (uA <= 1500000)
			reg_val = 0x20 + (uA - 540000) / 60000;
		else if (uA <= 2940000)
			reg_val = 0x30 + (uA - 1500000) / 120000;
		else
			reg_val = 0x3d;
	} else {
		reg_val = uA / SGM415XX_ICHRG_I_STEP_uA;
	}

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_2, SGM415XX_ICHRG_I_MASK, reg_val);
}

static int sgm415xx_set_chrg_volt(struct sgm415xx_device *sgm, u32 chrg_volt)
{
	u8 reg_val;

	if (chrg_volt < SGM415XX_VREG_V_MIN_uV)
		chrg_volt = SGM415XX_VREG_V_MIN_uV;
	else if (chrg_volt > SGM415XX_VREG_V_MAX_uV)
		chrg_volt = SGM415XX_VREG_V_MAX_uV;

	reg_val = (chrg_volt - SGM415XX_VREG_V_MIN_uV) / SGM415XX_VREG_V_STEP_uV;
	reg_val = reg_val << 3;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4, SGM415XX_VREG_V_MASK, reg_val);
}

static int sgm415xx_set_sys_min(struct sgm415xx_device *sgm, u32 sys_min)
{
	u8 reg_val;
	int i;
	int vol_buff[8] = {2600000, 2800000, 3000000, 3200000,
		3400000, 3500000, 3600000, 3700000};

	if (sys_min < 2600000)
		sys_min = 2600000;
	else if (sys_min > 3700000)
		sys_min = 3700000;

	for (i = 0; i < 8; i++) {
		if (vol_buff[i] >= sys_min)
			break;
	}
	reg_val = i << 1;
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, GENMASK(3, 1), reg_val);
}

static int sgm415xx_get_chrg_volt(struct sgm415xx_device *sgm, unsigned int *volt)
{
	int ret;
	u8 vreg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_4, &vreg_val);
	if (ret)
		return ret;

	vreg_val = FIELD_GET(SGM415XX_VREG_V_MASK, vreg_val);

	if (vreg_val == 15)
		*volt = 4352000; //default
	else if (vreg_val < 25)
		*volt = vreg_val * SGM415XX_VREG_V_STEP_uV + SGM415XX_VREG_V_MIN_uV;

	return 0;
}

static int sgm415xx_set_input_volt_lim(struct sgm415xx_device *sgm, unsigned int vindpm)
{
	unsigned int offset;
	u8 reg_val;
	u8 os_val;

	if (vindpm < SGM415XX_VINDPM_V_MIN_uV || vindpm > SGM415XX_VINDPM_V_MAX_uV)
		return -EINVAL;

	if (vindpm < 5900000) {
		os_val = 0;
		offset = 3900000;
	} else if (vindpm >= 5900000 && vindpm < 7500000) {
		os_val = 1;
		offset = 5900000; //uv
	} else if (vindpm >= 7500000 && vindpm < 10500000) {
		os_val = 2;
		offset = 7500000; //uv
	} else {
		os_val = 3;
		offset = 10500000; //uv
	}

	sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_f, SGM415XX_VINDPM_OS_MASK, os_val);

	reg_val = (vindpm - offset) / SGM415XX_VINDPM_STEP_uV;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_6, SGM415XX_VINDPM_V_MASK, reg_val);
}

static int sgm415xx_get_input_volt_lim(struct sgm415xx_device *sgm, u32 *uV)
{
	int ret;
	int offset;
	u8 vlim;
	int temp;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_6, &vlim);
	if (ret) {
		dev_err(sgm->dev, "%s,fail\n", __func__);
		return ret;
	}

	temp = sgm415xx_get_vindpm_offset_os(sgm);
	if (temp == 0)
		offset = 3900000; //uv
	else if (temp == 1)
		offset = 5900000;
	else if (temp == 2)
		offset = 7500000;
	else if (temp == 3)
		offset = 10500000;
	else
		return temp;

	*uV = offset + (vlim & 0x0F) * SGM415XX_VINDPM_STEP_uV;

	return 0;
}

static int sgm415xx_set_input_curr_lim(struct sgm415xx_device *sgm, unsigned int iindpm)
{
	u8 reg_val;

	if (iindpm < SGM415XX_IINDPM_I_MIN_uA || iindpm > sgm->init_data.max_ilim)
		return -EINVAL;

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		reg_val = (iindpm - SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
	} else {
		if (iindpm >= SGM415XX_IINDPM_I_MIN_uA && iindpm <= 3100000) //default
			reg_val = (iindpm - SGM415XX_IINDPM_I_MIN_uA) / SGM415XX_IINDPM_STEP_uA;
		else if (iindpm > 3100000 && iindpm < sgm->init_data.max_ilim)
			reg_val = 0x1E;
		else
			reg_val = SGM415XX_IINDPM_I_MASK;
	}
	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_0, SGM415XX_IINDPM_I_MASK, reg_val);
}

static int sgm415xx_get_input_curr_lim(struct sgm415xx_device *sgm, unsigned int *ilim)
{
	int ret;
	u8 reg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_0, &reg_val);
	if (ret) {
		dev_err(sgm->dev, "%s,fail\n", __func__);
		return ret;
	}

	if (sgm->device_id == SGM41513_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41515_PN_ID || sgm->device_id == SGM41515D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41512S_PN_ID) {
		*ilim = (reg_val & SGM415XX_IINDPM_I_MASK) *
			SGM415XX_IINDPM_STEP_uA + SGM415XX_IINDPM_I_MIN_uA;
	} else {
		if (SGM415XX_IINDPM_I_MASK == (reg_val & SGM415XX_IINDPM_I_MASK))
			*ilim =  sgm->init_data.max_ilim;
		else
			*ilim = (reg_val & SGM415XX_IINDPM_I_MASK) *
				SGM415XX_IINDPM_STEP_uA + SGM415XX_IINDPM_I_MIN_uA;
	}

	return 0;
}

static int sgm415xx_check_fault_status(struct sgm415xx_device *sgm, struct sgm415xx_state *state)
{
	int ret;
	u8 reg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_FAULT, &reg_val);
	if (ret) {
		dev_err(sgm->dev, "read fault fail\n");
		return ret;
	}

	state->wdt_fault = !!(reg_val & SGM415XX_WDT_FAULT_MASK);
	state->boost_fault = !!(reg_val & SGM415XX_BOOST_FAULT_MASK);
	state->chrg_fault = FIELD_GET(SGM415XX_CHRG_FAULT_MASK, reg_val);
	state->bat_fault = !!(reg_val & SGM415XX_BAT_FAULT_MASK);
	state->ntc_fault = FIELD_GET(SGM415XX_TEMP_MASK, reg_val);

	SGM_INFO("fault status = 0x%x\n", reg_val);

	return ret;
}

enum SGM415XX_POWER_TYPE sgm415xx_get_power_type(struct sgm415xx_device *sgm)
{
	if (sgm->state.vbus_type == 0 && sgm->state.vbus_pg == 0) {
		return POWER_TYPE_BATTERY;
	} else if ((sgm->state.vbus_type != 0) && (sgm->state.vbus_pg == 1)) {
		int val, ret;

		if (sgm->adc_ch) {
			ret = iio_read_channel_processed(sgm->adc_ch, &val);
			SGM_INFO("DCIN_DET adc_ch val = %dmV\n", val);
			if (ret < 0) {
				dev_err(sgm->dev, "ADC read failed\n");
				return ret;
			}
			if (val >= AC_POWER_THRESHOLD)
				return POWER_TYPE_VBUS_AC;
		}

		return POWER_TYPE_VBUS_DC;
	}

	return POWER_TYPE_UNKNOWN;
}

static int sgm415xx_get_state(struct sgm415xx_device *sgm, struct sgm415xx_state *state)
{
	int ret;
	u8 reg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &reg_val);
	if (ret) {
		ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_STAT, &reg_val);
		if (ret) {
			pr_err("%s read SGM415XX_CHRG_STAT fail\n", __func__);
			return ret;
		}
	}

	state->vbus_type = FIELD_GET(SGM415XX_VBUS_STAT_MASK, reg_val);
	state->chrg_stat = FIELD_GET(SGM415XX_CHG_STAT_MASK, reg_val);
	state->vbus_pg = !!(reg_val & SGM415XX_PG_STAT);
	state->therm_stat = !!(reg_val & SGM415XX_THERM_STAT);
	state->vsys_stat = !!(reg_val & SGM415XX_VSYS_STAT);
	ret = sgm415xx_get_power_type(sgm);
	if (ret >= 0)
		state->power_type = ret;

	ret = sgm415xx_check_fault_status(sgm, state);
	if (ret) {
		pr_err("read SGM415XX_CHRG_FAULT fail\n");
		return ret;
	}

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_a, &reg_val);
	if (ret) {
		pr_err("read SGM415XX_CHRG_CTRL_a fail\n");
		return ret;
	}
	state->vbus_gd = !!(reg_val & SGM415XX_VBUS_GOOD);

	return 0;
}

static int sgm415xx_set_charger_en(struct sgm415xx_device *sgm, int value)
{
	u8 val = (value) ? SGM415XX_CHRG_EN : 0;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_CHRG_EN, val);
}

static int sgm415xx_set_ship_mode(struct sgm415xx_device *sgm, int value)
{
	u8 val = (value) ? BIT(5) : 0;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7, BIT(5), val);
}

static int sgm415xx_set_recharge_volt(struct sgm415xx_device *sgm, int mV)
{
	u8 reg_val = (mV - SGM415XX_VRECHRG_OFFSET_mV) / SGM415XX_VRECHRG_STEP_mV;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_4, SGM415XX_VRECHARGE, reg_val);
}

__maybe_unused static int sgm415xx_set_wdt_rst(struct sgm415xx_device *sgm, bool is_rst)
{
	u8 val = (is_rst) ? SGM415XX_WDT_RST_MASK : 0;

	return sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_1, SGM415XX_WDT_RST_MASK, val);
}

static int sgm415xx_dump_register(struct sgm415xx_device *sgm)
{
	unsigned char i = 0;
	unsigned int ret = 0;
	unsigned char sgm415xx_reg[SGM415XX_REG_NUM + 1] = { 0 };

	SGM_INFO("[%s]\n", __func__);
	for (i = 0; i < SGM415XX_REG_NUM; i += 4) {
		ret = sgm415xx_read_reg(sgm, i, &sgm415xx_reg[i]);
		ret = sgm415xx_read_reg(sgm, i + 1, &sgm415xx_reg[i + 1]);
		ret = sgm415xx_read_reg(sgm, i + 2, &sgm415xx_reg[i + 2]);
		ret = sgm415xx_read_reg(sgm, i + 3, &sgm415xx_reg[i + 3]);
		if (ret != 0) {
			SGM_INFO("i2c transfor error\n");
			return 1;
		}
		SGM_INFO(" [0x%02x]=0x%02x [0x%02x]=0x%02x [0x%02x]=0x%02x [0x%02x]=0x%02x\n",
			i, sgm415xx_reg[i],
			i + 1, sgm415xx_reg[i + 1],
			i + 2, sgm415xx_reg[i + 2],
			i + 3, sgm415xx_reg[i + 3]);
	}

	return 0;
}

static bool sgm415xx_hw_chipid_detect(struct sgm415xx_device *sgm)
{
	int ret = 0;
	u8 val = 0;
	int index = 0;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_b, &val);
	if (ret < 0) {
		dev_err(sgm->dev, "read fail\n");
		return false;
	}
	val = val & SGM415XX_PN_MASK;

	SGM_INFO("device name %s\n", sgm->client->name);
	for (index = 0; index < MAX_PN_ID; index++) {
		if (s_chg_vendor[index].part_id == val &&
			strstr(s_chg_vendor[index].name, sgm->client->name)) {
			sgm->device_id = index;
			SGM_INFO("device name %s\n", s_chg_vendor[index].name);
			if (sgm->device_id == SGM41513_PN_ID ||
				sgm->device_id == SGM41513D_PN_ID ||
				sgm->device_id == SGM41515_PN_ID ||
				sgm->device_id == SGM41515D_PN_ID ||
				sgm->device_id == SGM41512SD_PN_ID ||
				sgm->device_id == SGM41512S_PN_ID) {
				sgm->init_data.max_ichg = 3000000;
				sgm->init_data.max_ilim = 3200000;
			} else {
				sgm->init_data.max_ichg = 3780000;
				sgm->init_data.max_ilim = 3800000;
			}
			return true;
		}
	}
	SGM_INFO("don't found device ,Reg[0x0B]=0x%x\n", val);
	return false;
}

__maybe_unused static int get_charger_type(struct sgm415xx_device *sgm)
{
	int curr_in_limit = 0;

	switch (sgm->state.vbus_type) {
	case SGM415XX_USB_SDP:
		SGM_INFO("SGM415xx charger type: SDP\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_SDP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
		curr_in_limit = 500000;
		break;

	case SGM415XX_USB_CDP:
		SGM_INFO("SGM415xx charger type: CDP\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_CDP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
		curr_in_limit = 1000000;
		break;

	case SGM415XX_USB_DCP:
		SGM_INFO("SGM415xx charger type: DCP\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		curr_in_limit = 2000000;
		break;

	case SGM415XX_UNKNOWN:
		SGM_INFO("SGM415xx charger type: UNKNOWN\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
		curr_in_limit = 500000;
		if (sgm->force_detect_count < 2) { // retry detect
			dev_err(sgm->dev, "unknown type, force detect again\n");
			// start force dpdm after 100ms
			schedule_delayed_work(&sgm->force_detect_dwork, msecs_to_jiffies(100));
		}
		break;

	case SGM415XX_NON_STANDARD:
		SGM_INFO("SGM415xx charger type: NON_STANDARD\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_DCP;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB_DCP;
		curr_in_limit = 900000;
		break;

	case SGM415XX_OTG_MODE:
		SGM_INFO("SGM415xx charger type: OTG\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
		break;

	default:
		SGM_INFO("SGM415xx charger type: default\n");
		sgm->psy_usb_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		sgm->chg_type = POWER_SUPPLY_TYPE_USB;
		curr_in_limit = 500000;
		break;
	}
	//set charge parameters
	SGM_INFO("Update(vbus_type): curr_in_limit = %d\n", curr_in_limit);

	if (sgm->state.power_type == POWER_TYPE_VBUS_AC)
		curr_in_limit = 3200000;
	SGM_INFO("Update(power_type): curr_in_limit = %d\n", curr_in_limit);

	sgm415xx_set_input_curr_lim(sgm, curr_in_limit);

	return sgm->psy_usb_type;
}

/********************** QC Interface Start ***********************/

static int32_t sgm415xx_set_dpdm(struct sgm415xx_device *sgm, uint8_t dp_val, uint8_t dm_val)
{
	u8 reg_val_dp = 0;
	u8 reg_val_dm = 0;
	int ret;

	reg_val_dp = (dp_val << SGM415XX_DP_VOLT_SHIFT) & SGM415XX_DP_VSEL_MASK;
	reg_val_dm = (dm_val << SGM415XX_DM_VOLT_SHIFT) & SGM415XX_DM_VSEL_MASK;

	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d, SGM415XX_DP_VSEL_MASK, reg_val_dp);
	ret |= sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d, SGM415XX_DM_VSEL_MASK, reg_val_dm);

	return ret;
}

__maybe_unused static int sgm415xx_charging_set_hvdcp20(struct sgm415xx_device *sgm,
	uint32_t vbus_target)
{
	s32 ret = 0;

	switch (vbus_target) {
	case QC_20_5000mV:
		SGM_INFO("Set vbus target 5v\n");
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P0);
		sgm415xx_set_input_volt_lim(sgm, 4500000);
		break;
	case QC_20_9000mV:
		SGM_INFO("Set vbus target 9v\n");
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_0P6);
		sgm415xx_set_input_volt_lim(sgm, 8000000);
		break;
	case QC_20_12000mV:
		SGM_INFO("Set vbus target 12v\n");
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		sgm415xx_set_input_volt_lim(sgm, 11000000);
		break;
	default:
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_HIZ, SGM_DP_DM_VOL_HIZ);
		sgm415xx_set_input_volt_lim(sgm, 4500000);
		break;
	}

	return (ret < 0) ? ret : 0;
}

int32_t sgm415xx_charging_enable_hvdcp30(struct sgm415xx_device *sgm, bool enable)
{
	SGM_INFO("enable %d\n", enable);
	//enter continuous mode DP 0.6, DM 3.3
	if (enable)
		return sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
	else
		return sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P0, SGM_DP_DM_VOL_0P0);
}

int32_t sgm415xx_charging_set_hvdcp30(struct sgm415xx_device *sgm, bool increase)
{
	s32 ret = 0;

	SGM_INFO("increase %d\n", increase);
	if (increase) {
		//DP 3.3, DM 3.3
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_3P3, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		//TGLITCH CONT_CHANGE 100-200 us
		usleep_range(100, 200);
		//DP 0.6, DM 3.3
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		usleep_range(100, 200);
	} else {
		//DP 0.6, DM 3.3
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_0P6);
		if (ret < 0)
			return ret;

		usleep_range(100, 200);
		//DP 0.6, DM 3.3
		ret = sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_3P3);
		if (ret < 0)
			return ret;
		usleep_range(100, 200);
	}
	return 0;
}

static void sgm415xx_chg_type_hvdcp_work(struct work_struct *work)
{
	struct sgm415xx_device *sgm = container_of(work, struct sgm415xx_device, hvdcp_work.work);

	if (sgm->set_vbus == QC_20_5000mV ||
		sgm->set_vbus == QC_20_9000mV ||
		sgm->set_vbus == QC_20_12000mV)
		sgm415xx_charging_set_hvdcp20(sgm, sgm->set_vbus);
	else
		sgm415xx_charging_set_hvdcp20(sgm, 9);
	SGM_INFO("HVDCP\n");
}

/********************* end ************************/

/********************* PE START ************************/
static int sgm415xx_send_ta_current_pattern(struct sgm415xx_device *sgm, bool is_up)
{
	int ret = 0;
	int val;

	if (sgm->device_id == SGM41542_PN_ID || sgm->device_id == SGM41516D_PN_ID ||
		sgm->device_id == SGM41543D_PN_ID || sgm->device_id == SGM41513D_PN_ID ||
		sgm->device_id == SGM41512SD_PN_ID || sgm->device_id == SGM41515D_PN_ID) {
		if (is_up)
			val = SGM415XX_EN_PUMPX | SGM415XX_PUMPX_UP;
		else
			val = SGM415XX_EN_PUMPX | SGM415XX_PUMPX_DN;

		ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_d,
			SGM415XX_EN_PUMPX | SGM415XX_PUMPX_UP | SGM415XX_PUMPX_DN, val);
		if (ret < 0) {
			dev_err(sgm->dev, "increase or reduce voltage fail\n");
			return ret;
		} else {
			return 0;
		}
	} else {
		dev_err(sgm->dev, "not support\n");
		return 0;
	}
}

static int cptime[13][2];

static int dtime(int i)
{
	return 0;
}

#define PEOFFTIME 40
#define PEONTIME 90
static int sgm415xx_send_ta20_current_pattern(struct sgm415xx_device *sgm, u32 chr_vol)
{
	int value;
	int i, j = 0;
	int flag;

	sgm415xx_set_input_curr_lim(sgm, 4200000);
	sgm415xx_set_ichrg_curr(sgm, 600000);

	usleep_range(1000, 1200);

	value = (chr_vol - 5500000) / 500000;
	sgm415xx_set_input_curr_lim(sgm, 100000); // set ibus 100mA
	mdelay(70);

	j++;
	for (i = 4; i >= 0; i--) {
		flag = value & (1 << i);
		if (flag == 0) {
			sgm415xx_set_input_curr_lim(sgm, 700000); // ibus = 700mA

			mdelay(PEOFFTIME);

			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				SGM_INFO("set ta20 fail1: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
			}
			j++;
			sgm415xx_set_input_curr_lim(sgm, 100000);
			mdelay(PEONTIME);

			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				SGM_INFO("set ta20 fail2: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
			}
			j++;
		} else {
			sgm415xx_set_input_curr_lim(sgm, 700000); // ibus = 700mA
			mdelay(PEONTIME);

			cptime[j][0] = PEONTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 90 || cptime[j][1] > 115) {
				SGM_INFO("set ta20 fail3: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
			}
			j++;
			sgm415xx_set_input_curr_lim(sgm, 100000);
			mdelay(PEOFFTIME);

			cptime[j][0] = PEOFFTIME;
			cptime[j][1] = dtime(j);
			if (cptime[j][1] < 30 || cptime[j][1] > 65) {
				SGM_INFO("set ta20 fail4: idx:%d target:%d actual:%d\n",
					i, PEOFFTIME, cptime[j][1]);
			}
			j++;
		}
	}

	sgm415xx_set_input_curr_lim(sgm, 700000); // ibus = 700mA
	mdelay(160);

	cptime[j][0] = 160;
	cptime[j][1] = dtime(j);
	if (cptime[j][1] < 150 || cptime[j][1] > 240) {
		SGM_INFO("charging_set_ta20_current_pattern fail5: idx:%d target:%d actual:%d\n",
			i, PEOFFTIME, cptime[j][1]);
	}
	j++;
	sgm415xx_set_input_curr_lim(sgm, 100000);
	mdelay(30);
	sgm415xx_set_input_curr_lim(sgm, 700000); // ibus = 700mA

	SGM_INFO("%d %d: %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		chr_vol, value, cptime[1][0], cptime[2][0], cptime[3][0],
		cptime[4][0], cptime[5][0], cptime[6][0], cptime[7][0],
		cptime[8][0], cptime[9][0], cptime[10][0], cptime[11][0]);
	SGM_INFO("%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d!!\n",
		cptime[1][1], cptime[2][1], cptime[3][1], cptime[4][1],
		cptime[5][1], cptime[6][1], cptime[7][1], cptime[8][1],
		cptime[9][1], cptime[10][1], cptime[11][1]);
	mdelay(100);
	return 0;
}

__maybe_unused static int sgm415xx_set_ta20_reset(struct sgm415xx_device *sgm)
{
	int curr;

	sgm415xx_get_input_curr_lim(sgm, &curr);

	sgm415xx_set_input_curr_lim(sgm, 100000);
	msleep(300);
	return sgm415xx_set_input_curr_lim(sgm, curr);
}

/********************* PE END ************************/

static enum power_supply_property sgm415xx_power_supply_props[] = {
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT
};

static int sgm415xx_property_is_writeable(struct power_supply *psy,
	enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
		return true;
	default:
		return false;
	}
}

static int sgm415xx_charger_set_property(struct power_supply *psy,
	enum power_supply_property prop,
	const union power_supply_propval *val)
{
	struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);
	int ret = -EINVAL;

	switch (prop) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sgm415xx_set_input_curr_lim(sgm, val->intval);
		break;
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm415xx_set_input_volt_lim(sgm, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		ret = sgm415xx_set_charger_en(sgm, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm415xx_set_ichrg_curr(sgm, (u32)val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm415xx_set_chrg_volt(sgm, (u32)val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sgm415xx_set_term_curr(sgm, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int sgm415xx_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	struct sgm415xx_device *sgm = power_supply_get_drvdata(psy);
	struct sgm415xx_state state;
	int ret = 0;
	u32 value;

	mutex_lock(&sgm->lock);
	ret = sgm415xx_get_state(sgm, &state);
	state = sgm->state;
	mutex_unlock(&sgm->lock);
	if (ret)
		return ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.vbus_type || state.vbus_type == SGM415XX_OTG_MODE)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (!state.chrg_stat)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_stat == SGM415XX_TERM_CHRG)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (state.chrg_stat) {
		case SGM415XX_PRECHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM415XX_FAST_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case SGM415XX_TERM_CHRG:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		case SGM415XX_NOT_CHRGING:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		default:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		}
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SGM415XX_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = s_chg_vendor[sgm->device_id].name;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.vbus_pg;
		break;

	case POWER_SUPPLY_PROP_TYPE:
		val->intval = sgm->chg_type;
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = get_charger_type(sgm);
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (sgm->state.boost_fault)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else
			val->intval = POWER_SUPPLY_HEALTH_GOOD;

		switch (state.ntc_fault) {
		case SGM415XX_TEMP_HOT:
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
			break;
		case SGM415XX_TEMP_WARM:
		case SGM415XX_TEMP_COOL:
		case SGM415XX_TEMP_COLD:
			val->intval = POWER_SUPPLY_HEALTH_COLD;
			break;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		ret = sgm415xx_get_input_volt_lim(sgm, &value);
		if (ret < 0)
			return ret;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sgm415xx_get_input_curr_lim(sgm, &value);
		if (ret < 0)
			return ret;

		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sgm415xx_get_chrg_volt(sgm, &value);
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = sgm415xx_get_term_curr(sgm, &value);
		val->intval = value;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sgm415xx_get_ichg_curr(sgm, &value);
		val->intval = value;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static int sgm415xx_force_dpdm(struct sgm415xx_device *sgm)
{
	int ret;

	ret = sgm415xx_update_bits(sgm, SGM415XX_CHRG_CTRL_7,
		SGM415XX_IINDET_EN, SGM415XX_IINDET_EN);
	if (ret < 0) {
		dev_err(sgm->dev, "update bit fail, ret = %d\n", ret);
		return ret;
	}

	SGM_INFO("successful\n");
	return ret;
}

__maybe_unused static bool sgm415xx_dpdm_detect_is_done(struct sgm415xx_device *sgm)
{
	u8 chrg_flag, ret, val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_INPUT_DET, &chrg_flag);
	if (ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
		return false;
	}

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_7, &val);
	if (ret) {
		dev_err(sgm->dev, "Check DPDM detecte error\n");
		return false;
	}

	if ((chrg_flag & SGM415XX_INPUT_DET_DONE) && (val & SGM415XX_DPDM_ONGOING))
		return true;
	else
		return false;
}

static void charger_monitor_work_func(struct work_struct *work)
{
	int ret = 0;
	struct sgm415xx_device *sgm = NULL;
	struct delayed_work *charge_monitor_work = NULL;
	struct sgm415xx_state state;
	bool last_bat_fault;
	u8 last_ntc_fault;

	charge_monitor_work = container_of(work, struct delayed_work, work);
	if (!charge_monitor_work) {
		pr_err(SGM_LOG "Cann't get charge_monitor_work\n");
		return;
	}
	sgm = container_of(charge_monitor_work, struct sgm415xx_device, charge_monitor_work);
	if (!sgm) {
		pr_err(SGM_LOG "Cann't get sgm\n");
		return;
	}

	last_ntc_fault = sgm->state.ntc_fault;
	last_bat_fault = sgm->state.bat_fault;
	mutex_lock(&sgm->lock);
	ret = sgm415xx_get_state(sgm, &state);
	sgm->state = state;
	mutex_unlock(&sgm->lock);

	get_charger_type(sgm);
	if (!sgm->state.vbus_gd || sgm->state.ntc_fault || sgm->state.bat_fault) {
		SGM_INFO("vbus_gd:%d,ntc_fault:%s,bat_fault:%d,capacity:%d, disable charge\n",
			sgm->state.vbus_gd,
			sgm415xx_ntc_fault[sgm->state.ntc_fault],
			sgm->state.bat_fault,
			cw_capacity);
		sgm415xx_set_charger_en(sgm, 0);
	} else if ((last_ntc_fault && !state.ntc_fault) ||
			(last_bat_fault && !state.bat_fault)) {
		sgm415xx_set_charger_en(sgm, 1);
	}

	if (!state.vbus_gd && state.power_type == POWER_TYPE_BATTERY) {
		SGM_INFO("Vbus not vbus_gd\n");
		if (cw_capacity == 0) {
			msleep(3000);
			pr_emerg(SGM_LOG "battery capacity: %d, run shutdown!\n", cw_capacity);
			kernel_power_off();
		}
		goto OUT;
	}
	sgm415xx_dump_register(sgm);

OUT:
	schedule_delayed_work(&sgm->charge_monitor_work, 10 * HZ);
}

static void charger_detect_work_func(struct work_struct *work)
{
	struct delayed_work *charge_detect_delayed_work = NULL;
	struct sgm415xx_device *sgm = NULL;
	struct sgm415xx_state state;
	bool prev_vbus_gd;
	int ret = 0;

	charge_detect_delayed_work = container_of(work, struct delayed_work, work);
	if (!charge_detect_delayed_work) {
		pr_err(SGM_LOG "Cann't get charge_detect_delayed_work\n");
		return;
	}
	sgm = container_of(charge_detect_delayed_work,
		struct sgm415xx_device,
		charge_detect_delayed_work);
	if (!sgm) {
		pr_err(SGM_LOG "Cann't get sgm415xx_device\n");
		return;
	}

	prev_vbus_gd = sgm->state.vbus_gd;
	mutex_lock(&sgm->lock);
	sgm415xx_get_state(sgm, &state);
	sgm->state = state;
	mutex_unlock(&sgm->lock);

	if (!prev_vbus_gd && state.vbus_gd) {
		SGM_INFO("adapter/usb inserted\n");
		sgm->force_detect_count = 0;
	} else if ((prev_vbus_gd && !state.vbus_gd)) {
		get_charger_type(sgm);
		if (sgm->qc2_en) {
			ret = sgm415xx_charging_set_hvdcp20(sgm, QC_20_5000mV);
			if (ret < 0)
				pr_err(SGM_LOG "set qc to 5V failed while typec plug out!\n");
		}
		sgm->force_detect_count = 0;
		SGM_INFO("adapter/usb removed\n");
		goto err;
	}

	if (!state.vbus_gd || state.ntc_fault || state.bat_fault) {
		dev_err(sgm->dev, "vbus_gd:%d,ntc_fault:%s,bat_fault:%d, disable charge\n",
			state.vbus_gd,
			sgm415xx_ntc_fault[state.ntc_fault],
			state.bat_fault);
		sgm415xx_set_charger_en(sgm, 0);
		goto err;
	}
	if (!state.vbus_pg) {
		dev_err(sgm->dev, "Vbus not vbus_pg\n");
		goto err;
	}

	if (SGM41513D_PN_ID == sgm->device_id || SGM41516D_PN_ID == sgm->device_id ||
		SGM41543D_PN_ID == sgm->device_id || SGM41542_PN_ID == sgm->device_id ||
		SGM41515D_PN_ID == sgm->device_id || SGM41512SD_PN_ID == sgm->device_id)
		get_charger_type(sgm);

	//enable charge
	sgm415xx_set_charger_en(sgm, 1);

err:
	//release wakelock
	power_supply_changed(sgm->charger);
	SGM_INFO("Relax wakelock\n");
	__pm_relax(sgm->charger_wakelock);
}

static void sgm415xx_force_detection_work(struct work_struct *work)
{
	struct delayed_work *force_detect_delayed_work = NULL;
	struct sgm415xx_device *sgm = NULL;
	int ret;

	force_detect_delayed_work = container_of(work, struct delayed_work, work);
	if (!force_detect_delayed_work) {
		SGM_INFO("Cann't get force_detect_delayed_work\n");
		return;
	}

	sgm = container_of(force_detect_delayed_work, struct sgm415xx_device, force_detect_dwork);
	if (!sgm) {
		SGM_INFO("Cann't get sgm415xx_device\n");
		return;
	}

	if (!sgm->state.vbus_gd) {
		SGM_INFO("vbus not good\n");
		return;
	}

	ret = sgm415xx_force_dpdm(sgm);
	if (ret) {
		SGM_INFO("force dpdm failed(%d)\n", ret);
		return;
	}

	sgm->force_detect_count++;

	mdelay(500);

	if (sgm->force_detect_count >= 2) {
		SGM_INFO("stop the force_detect_work by timeout.\n");
		cancel_delayed_work(&sgm->force_detect_dwork);
	}
}

/********************creat devices note start*************************************************/
static ssize_t registers_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct sgm415xx_device *sgm415xx = dev_get_drvdata(dev);
	u8 tmpbuf[30];
	int idx = 0;
	u8 addr;
	u8 val;
	int len;
	int ret;

	for (addr = 0x0; addr <= SGM415XX_CHRG_CTRL_f; addr++) {
		ret = sgm415xx_read_reg(sgm415xx, addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, 30,
				"Reg[%.2X] = 0x%.2x\n",
				addr,
				val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t registers_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sgm415xx_device *sgm415xx = dev_get_drvdata(dev);
	unsigned int reg;
	int ret;
	int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= SGM415XX_CHRG_CTRL_f)
		sgm415xx_write_reg(sgm415xx, (unsigned char)reg, (unsigned char)val);

	return count;
}

static ssize_t sgmtest_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t sgmtest_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);
	unsigned int type, vol;
	int ret;

	ret = sscanf(buf, "%d %d", &type, &vol);
	if (ret == 2 && type == 1) { //QC2.0 test
		if (vol <= 0 || vol > 12) {
			sgm->qc2_en = 0;
			dev_err(sgm->dev, "Input qc2 voltage error.\n");
			return ret;
		}

		sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_HIZ);
		schedule_delayed_work(&sgm->hvdcp_work, msecs_to_jiffies(1500));
		if (vol == 5)
			sgm->set_vbus = QC_20_5000mV;
		else if (vol == 9)
			sgm->set_vbus = QC_20_9000mV;
		else if (vol == 12)
			sgm->set_vbus = QC_20_12000mV;
		sgm->qc2_en = 1;
	} else if (ret == 2 && type >= 2) { //QC3.0 test
		if (vol > 0) {
			if (sgm->qc3_en == 0 && vol == 1) {
				sgm415xx_set_dpdm(sgm, SGM_DP_DM_VOL_0P6, SGM_DP_DM_VOL_HIZ);
				mdelay(1500);
				sgm415xx_charging_enable_hvdcp30(sgm, true);
				mdelay(1);
			}
			if (vol == 2)
				ret = sgm415xx_charging_set_hvdcp30(sgm, true); //increase 200mV
			else if (vol == 3)
				ret = sgm415xx_charging_set_hvdcp30(sgm, false); //decrease 200mV

			if (ret) {
				dev_err(sgm->dev, "QC3.0 set vol error!\n");
				return ret;
			}
			sgm->qc3_en = 1;
		} else {
			sgm->qc3_en = 0;
		}
	} else if (ret == 2 && type == 0) {
		if (vol == 1)
			sgm415xx_send_ta_current_pattern(sgm, 1);  //PE1.0 increase
		else if (vol == 2)
			sgm415xx_send_ta_current_pattern(sgm, 0);  //PE1.0 decree
		else if (vol == 3)
			sgm415xx_send_ta20_current_pattern(sgm, 7500000);  //PE2.0
		else
			sgm415xx_set_ta20_reset(sgm); //PE reset
	} //PE test
	return count;
}

#ifdef OTG_ENABLE
__maybe_unused static ssize_t otgen_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	u8 val = 0;
	int ret = 0;
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &val);
	if (ret) {
		dev_err(sgm->dev, "read SGM415XX_CHRG_CTRL_1 fail!\n");
		return ret;
	}

	if (val & SGM415XX_OTG_EN)
		SGM_INFO("OTG Enabled\n");
	else
		SGM_INFO("OTG Disabled\n");
	return 0;
}

__maybe_unused static ssize_t otgen_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);
	unsigned int en;
	int ret;

	ret = kstrtou32(buf, 10, &en);
	if (ret == 0 && en == 1) {
		sgm415xx_set_charger_en(sgm, 0);
		/* OTG setting 5V/1.2A */
		ret = sgm415xx_set_boost_voltage_limit(sgm, 5000000);
		if (ret) {
			dev_err(sgm->dev, "set OTG voltage error!\n");
			return ret;
		}

		ret = sgm415xx_set_boost_current_limit(sgm, 1200000);
		if (ret) {
			dev_err(sgm->dev, "set OTG current error!\n");
			return ret;
		}

		ret = sgm415xx_enable_otg(sgm, 1);
		if (ret) {
			dev_err(sgm->dev, "set OTG enable error!\n");
			return ret;
		}
	} else if (ret == 0 && en == 0) {
		ret = sgm415xx_enable_otg(sgm, 0);
		if (ret) {
			dev_err(sgm->dev, "set OTG disable error!\n");
			return ret;
		}
	}
	return count;
}
#endif

static ssize_t ship_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);
	int ret = 0;
	int mode = 0;
	u8 reg_val;

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_7, &reg_val);
	if (ret == 0) {
		mode = FIELD_GET(BIT(5), reg_val);

		ret = sprintf(buf, "ship_mode = %d\n", mode);
		SGM_INFO("ship mode:%d\n", mode);
	}

	return ret;
}

static ssize_t ship_mode_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);
	unsigned long val;
	int ret;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	ret = sgm415xx_set_ship_mode(sgm, val);
	if (ret) {
		dev_err(sgm->dev, "set ship mode error!\n");
		return ret;
	}

	return count;
}

static ssize_t power_type_show(struct device *dev,
							  struct device_attribute *attr, char *buf)
{
	struct sgm415xx_device *sgm = dev_get_drvdata(dev);
	int ret = 0;

	if (sgm->state.power_type == POWER_TYPE_BATTERY)
		ret = sprintf(buf, "BAT\n");
	else if (sgm->state.power_type == POWER_TYPE_VBUS_DC)
		ret = sprintf(buf, "DC\n");
	else if (sgm->state.power_type == POWER_TYPE_VBUS_AC)
		ret = sprintf(buf, "AC\n");
	else
		ret = sprintf(buf, "UNKNOWN\n");

	SGM_INFO("power_type:%d\n", sgm->state.power_type);

	return ret;
}

static DEVICE_ATTR_RW(registers);
static DEVICE_ATTR_RW(sgmtest);
static DEVICE_ATTR_RW(ship_mode);
#ifdef OTG_ENABLE
static DEVICE_ATTR_RW(otgen);
#endif
DEVICE_ATTR_RO(power_type);

static void sgm415xx_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
	device_create_file(dev, &dev_attr_sgmtest);
#ifdef OTG_ENABLE
	device_create_file(dev, &dev_attr_otgen);
#endif
	device_create_file(dev, &dev_attr_ship_mode);
	device_create_file(dev, &dev_attr_power_type);
}

/***************************creat devices note end***************************/

static irqreturn_t sgm415xx_irq_handler_thread(int irq, void *private)
{
	struct sgm415xx_device *sgm = private;

	if (!sgm->charger_wakelock->active)
		__pm_stay_awake(sgm->charger_wakelock);

	if (!work_busy(&sgm->charge_detect_delayed_work.work)) {
		SGM_INFO("work is idle\n");
		schedule_delayed_work(&sgm->charge_detect_delayed_work, 400);
	} else {
		SGM_INFO("work is busy\n");
	}

	return IRQ_HANDLED;
}

static char *sgm415xx_charger_supplied_to[] = {
	"main-battery",
};

static struct power_supply_desc sgm415xx_power_supply_desc = {
	.name = "charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.usb_types = BIT(POWER_SUPPLY_USB_TYPE_SDP) |
		BIT(POWER_SUPPLY_USB_TYPE_CDP) |
		BIT(POWER_SUPPLY_USB_TYPE_DCP) |
		BIT(POWER_SUPPLY_USB_TYPE_UNKNOWN),
	.properties = sgm415xx_power_supply_props,
	.num_properties = ARRAY_SIZE(sgm415xx_power_supply_props),
	.get_property = sgm415xx_charger_get_property,
	.set_property = sgm415xx_charger_set_property,
	.property_is_writeable = sgm415xx_property_is_writeable,
};

static int sgm415xx_power_supply_init(struct sgm415xx_device *sgm,
	struct device *dev)
{
	struct power_supply_config psy_cfg = { .drv_data = sgm,
						.of_node = dev->of_node, };

	psy_cfg.supplied_to = sgm415xx_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sgm415xx_charger_supplied_to);

	sgm->charger = devm_power_supply_register(sgm->dev,
		&sgm415xx_power_supply_desc,
		&psy_cfg);
	if (IS_ERR(sgm->charger))
		return -EINVAL;

	return 0;
}

static int sgm415xx_hw_init(struct sgm415xx_device *sgm)
{
	int ret = 0;
	struct power_supply_battery_info bat_info = {0};

	bat_info.constant_charge_current_max_ua = 3000000;
	bat_info.constant_charge_voltage_max_uv = SGM415XX_VREG_V_DEF_uV;
	bat_info.precharge_current_ua = SGM415XX_PRECHRG_I_DEF_uA;
	bat_info.charge_term_current_ua = SGM415XX_TERMCHRG_I_DEF_uA;

	sgm415xx_set_watchdog_timer(sgm, 0);
	sgm415xx_set_charger_en(sgm, 0);
	ret = sgm415xx_set_ichrg_curr(sgm, bat_info.constant_charge_current_max_ua);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_prechrg_curr(sgm, bat_info.precharge_current_ua);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_chrg_volt(sgm, bat_info.constant_charge_voltage_max_uv);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_term_curr(sgm, bat_info.charge_term_current_ua);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_input_volt_lim(sgm, sgm->init_data.vlim);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_input_curr_lim(sgm, sgm->init_data.ilim);
	if (ret)
		goto err_out;

	ret = sgm415xx_set_recharge_volt(sgm, 200);//100~200mv
	if (ret)
		goto err_out;

	ret = sgm415xx_set_sys_min(sgm, sgm->init_data.sys_min);
	if (ret)
		goto err_out;

	dev_notice(sgm->dev, "ichrg:%d prechrg:%d chrg_vol:%d term:%d",
		bat_info.constant_charge_current_max_ua,
		bat_info.precharge_current_ua,
		bat_info.constant_charge_voltage_max_uv,
		bat_info.charge_term_current_ua);
	dev_notice(sgm->dev, "input vlim:%d input ilim:%d rec volt200mv sys_min:%d",
		sgm->init_data.vlim,
		sgm->init_data.ilim,
		sgm->init_data.sys_min);

	return 0;

err_out:
	return ret;
}

static int sgm415xx_parse_dt(struct sgm415xx_device *sgm)
{
	int ret;
	int irq_gpio = 0, irqn = 0;

	ret = device_property_read_u32(sgm->dev,
		"sgm,input-voltage-limit-microvolt",
		&sgm->init_data.vlim);
	if (ret)
		sgm->init_data.vlim = 4500000;

	if (sgm->init_data.vlim > SGM415XX_VINDPM_V_MAX_uV ||
		sgm->init_data.vlim < SGM415XX_VINDPM_V_MIN_uV)
		return -EINVAL;

	ret = device_property_read_u32(sgm->dev,
		"sgm,input-current-limit-microamp",
		&sgm->init_data.ilim);
	if (ret)
		sgm->init_data.ilim = 500000;

	if (sgm->init_data.ilim > sgm->init_data.max_ilim ||
		sgm->init_data.ilim < SGM415XX_IINDPM_I_MIN_uA)
		return -EINVAL;

	ret = device_property_read_u32(sgm->dev, "sgm,sys_min_voltage", &sgm->init_data.sys_min);
	if (ret)
		sgm->init_data.sys_min = 3700000;

	if (sgm->init_data.sys_min > 3700000 ||  sgm->init_data.sys_min < 2600000) {
		dev_err(sgm->dev, ":sys_min:%d is out of limitation\n",
			sgm->init_data.sys_min);
		return -EINVAL;
	}

	irq_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,intr_gpio", 0);
	if (!gpio_is_valid(irq_gpio)) {
		dev_err(sgm->dev, ":%d gpio get failed\n", irq_gpio);
		return -EINVAL;
	}

	ret = gpio_request(irq_gpio, "sgm415xx irq pin");
	if (ret) {
		dev_err(sgm->dev, ":%d gpio request failed\n", irq_gpio);
		return ret;
	}

	gpio_direction_input(irq_gpio);
	irqn = gpio_to_irq(irq_gpio);
	if (irqn < 0) {
		dev_err(sgm->dev, ":%d gpio_to_irq failed\n", irqn);
		return irqn;
	}
	sgm->client->irq = irqn;

	/*charge state pin*/
	sgm->chg_stat_gpio = of_get_named_gpio(sgm->dev->of_node, "sgm,chg-stat-gpio", 0);
	if (!gpio_is_valid(sgm->chg_stat_gpio)) {
		dev_err(sgm->dev, ":%d gpio get failed\n", sgm->chg_stat_gpio);
		return -EINVAL;
	}
	ret = gpio_request(sgm->chg_stat_gpio, "sgm chg stat pin");
	if (ret) {
		dev_err(sgm->dev, ":%d gpio request failed\n", sgm->chg_stat_gpio);
		return ret;
	}
	gpio_direction_input(sgm->chg_stat_gpio);

	return 0;
}

#ifdef OTG_ENABLE
__maybe_unused static int sgm415xx_is_enabled_otg(void)
{
	u8 temp = 0;
	int ret = 0;
	struct sgm415xx_device *sgm = charger_get_data(s_chg_dev);

	ret = sgm415xx_read_reg(sgm, SGM415XX_CHRG_CTRL_1, &temp);
	return (temp & SGM415XX_OTG_EN) ? 1 : 0;
}
#endif

static int sgm415xx_open(struct inode *inode, struct file *file)
{
	struct miscdevice *misc = file->private_data;
	struct sgm415xx_device *sgm;

	sgm = container_of(misc, struct sgm415xx_device, miscdev);
	file->private_data = sgm;

	return 0;
}

static long sgm415xx_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct sgm415xx_device *sgm = file->private_data;
	struct sgm415xx_reg reg;
	unsigned int value;
	int ret;

	switch (cmd) {
	case SGM415XX_CMD_READ_REG:
		if (copy_from_user(&reg, (struct sgm415xx_reg __user *)arg, sizeof(reg)))
			return -EFAULT;
		ret = sgm415xx_read_reg(sgm, reg.reg_addr, &reg.value);
		if (ret < 0)
			return ret;
		if (copy_to_user((struct sgm415xx_reg __user *)arg, &reg, sizeof(reg)))
			return -EFAULT;
		break;

	case SGM415XX_CMD_WRITE_REG:
		if (copy_from_user(&reg, (struct sgm415xx_reg __user *)arg, sizeof(reg)))
			return -EFAULT;
		ret = sgm415xx_write_reg(sgm, reg.reg_addr, reg.value);
		if (ret < 0)
			return ret;
		break;

	case SGM415XX_CMD_SET_CHG_EN:
		if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
			return -EFAULT;

		ret = sgm415xx_set_charger_en(sgm, value);
		break;

	case SGM415XX_CMD_GET_STATUS:
	{
		struct sgm415xx_state state;

		mutex_lock(&sgm->lock);
		ret = sgm415xx_get_state(sgm, &state);
		sgm->state = state;
		mutex_unlock(&sgm->lock);
		if (ret < 0)
			return ret;

		if (copy_to_user((struct sgm415xx_state __user *)arg, &state, sizeof(state)))
			return -EFAULT;
		break;
	}
	case SGM415XX_CMD_SET_SHIP:
		if (copy_from_user(&value, (void __user *)arg, sizeof(value)))
			return -EFAULT;

		ret = sgm415xx_set_ship_mode(sgm, value);
		break;

	default:
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations sgm415xx_fops = {
	.owner			= THIS_MODULE,
	.open			= sgm415xx_open,
	.unlocked_ioctl = sgm415xx_ioctl,
};

static int sgm415xx_driver_probe(struct i2c_client *client)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct sgm415xx_device *sgm;
	char *name = NULL;

	sgm = devm_kzalloc(dev, sizeof(*sgm), GFP_KERNEL);
	if (!sgm)
		return -ENOMEM;

	sgm->client = client;
	sgm->dev = dev;

	sgm->adc_ch = devm_iio_channel_get(sgm->dev, NULL);
	if (IS_ERR(sgm->adc_ch)) {
		ret = PTR_ERR(sgm->adc_ch);
		if (ret == -EPROBE_DEFER) {
			dev_info(sgm->dev, "ADC not ready, defer probe\n");
			return -EPROBE_DEFER;
		}
		dev_err(sgm->dev, "Failed to get ADC channel: %d\n", ret);
		return ret;
	}

	mutex_init(&sgm->lock);
	mutex_init(&sgm->i2c_rw_lock);

	i2c_set_clientdata(client, sgm);

	ret = sgm415xx_hw_chipid_detect(sgm);
	if (!ret)
		goto err_out;

	ret = sgm415xx_parse_dt(sgm);
	if (ret)
		goto err_out;

	name = devm_kasprintf(sgm->dev, GFP_KERNEL, "%s", "sgm415xx suspend wakelock");
	sgm->charger_wakelock = wakeup_source_register(sgm->dev, name);

	INIT_DELAYED_WORK(&sgm->charge_detect_delayed_work, charger_detect_work_func);
	INIT_DELAYED_WORK(&sgm->charge_monitor_work, charger_monitor_work_func);
	INIT_DELAYED_WORK(&sgm->hvdcp_work, sgm415xx_chg_type_hvdcp_work);
	INIT_DELAYED_WORK(&sgm->force_detect_dwork, sgm415xx_force_detection_work);
	if (client->irq) {
		ret = devm_request_threaded_irq(dev, client->irq, NULL,
			sgm415xx_irq_handler_thread,
			IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT,
			dev_name(&client->dev), sgm);
		if (ret) {
			pr_err(SGM_LOG "Failed to register irq\n");
			goto err_out;
		}
		enable_irq_wake(client->irq);
	}

	ret = sgm415xx_power_supply_init(sgm, dev);
	if (ret) {
		pr_err(SGM_LOG "Failed to register power supply\n");
		goto err_out;
	}

	ret = sgm415xx_hw_init(sgm);
	if (ret) {
		dev_err(dev, "Cannot initialize the chip.\n");
		goto err_out;
	}

	sgm->state.power_type = POWER_TYPE_UNKNOWN;
	sgm->miscdev.minor = MISC_DYNAMIC_MINOR;
	sgm->miscdev.name  = "sgm415xx";
	sgm->miscdev.fops  = &sgm415xx_fops;
	if (misc_register(&sgm->miscdev)) {
		pr_err(SGM_LOG "miscdevice register failed\n");
		return -EINVAL;
	}

	mutex_lock(&sgm->lock);
	sgm415xx_get_state(sgm, &sgm->state);
	mutex_unlock(&sgm->lock);

	if (sgm->state.vbus_gd && !sgm->state.ntc_fault && !sgm->state.bat_fault)
		sgm415xx_set_charger_en(sgm, 1);

	sgm415xx_create_device_node(sgm->dev);
	schedule_delayed_work(&sgm->charge_monitor_work, 100);
	return 0;

err_out:
	dev_err(sgm->dev, "sgm415xx probe failed!\n");
	devm_kfree(&client->dev, sgm);
	return -ENODEV;
}

static void sgm415xx_charger_remove(struct i2c_client *client)
{
	struct sgm415xx_device *sgm = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&sgm->charge_monitor_work);

#ifdef OTG_ENABLE
	regulator_unregister(sgm->otg_rdev);
#endif

	power_supply_unregister(sgm->charger);

	mutex_destroy(&sgm->lock);
	mutex_destroy(&sgm->i2c_rw_lock);
}

static void sgm415xx_charger_shutdown(struct i2c_client *client)
{
	int ret = 0;
	struct sgm415xx_device *sgm = i2c_get_clientdata(client);

	ret = sgm415xx_set_charger_en(sgm, 0);
	if (ret)
		pr_err(SGM_LOG "Failed to disable charger, ret = %d\n", ret);

	pr_info(SGM_LOG "sgm415xx charger shutdown\n");
}

static const struct i2c_device_id sgm415xx_i2c_ids[] = {
	{ "sgm41541", 0 },
	{ "sgm41542", 1 },
	{ "sgm41543", 2 },
	{ "sgm41543D", 3 },
	{ "sgm41513", 4 },
	{ "sgm41513A", 5 },
	{ "sgm41513D", 6 },
	{ "sgm41516", 7 },
	{ "sgm41516D", 8 },
	{ "sgm41515", 9 },
	{ "sgm41515D", 10 },
	{ "sgm41512S", 11 },
	{ "sgm41512SD", 12 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sgm415xx_i2c_ids);

static const struct of_device_id sgm415xx_of_match[] = {
	{ .compatible = "sgm,sgm41541", },
	{ .compatible = "sgm,sgm41542", },
	{ .compatible = "sgm,sgm41543", },
	{ .compatible = "sgm,sgm41543D", },
	{ .compatible = "sgm,sgm41513", },
	{ .compatible = "sgm,sgm41513A", },
	{ .compatible = "sgm,sgm41513D", },
	{ .compatible = "sgm,sgm41516", },
	{ .compatible = "sgm,sgm41516D", },
	{ .compatible = "sgm,sgm41515", },
	{ .compatible = "sgm,sgm41515D", },
	{ .compatible = "sgm,sgm41512S", },
	{ .compatible = "sgm,sgm41512SD", },
	{ },
};
MODULE_DEVICE_TABLE(of, sgm415xx_of_match);

static struct i2c_driver sgm415xx_driver = {
	.driver = {
		.name = "sgm415xx-charger",
		.of_match_table = sgm415xx_of_match,
	},
	.probe = sgm415xx_driver_probe,
	.remove = sgm415xx_charger_remove,
	.shutdown = sgm415xx_charger_shutdown,
	.id_table = sgm415xx_i2c_ids,
};

int __init sgm415xx_charger_init(void)
{
	return i2c_add_driver(&(sgm415xx_driver));
}

void __exit sgm415xx_charger_exit(void)
{
	i2c_del_driver(&(sgm415xx_driver));
}

MODULE_AUTHOR("qhq <Allen_qin@sg-micro.com>");
MODULE_DESCRIPTION("sgm415xx charger driver");
MODULE_LICENSE("GPL");
