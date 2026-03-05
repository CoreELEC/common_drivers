/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _SGM415XX_CHARGER_H
#define _SGM415XX_CHARGER_H

#include <linux/i2c.h>

#define SGM415XX_MANUFACTURER	"SGMICRO"

/* define register */
#define SGM415XX_CHRG_CTRL_0	0x00
#define SGM415XX_CHRG_CTRL_1	0x01
#define SGM415XX_CHRG_CTRL_2	0x02
#define SGM415XX_CHRG_CTRL_3	0x03
#define SGM415XX_CHRG_CTRL_4	0x04
#define SGM415XX_CHRG_CTRL_5	0x05
#define SGM415XX_CHRG_CTRL_6	0x06
#define SGM415XX_CHRG_CTRL_7	0x07
#define SGM415XX_CHRG_STAT		0x08
#define SGM415XX_CHRG_FAULT		0x09
#define SGM415XX_CHRG_CTRL_a	0x0a
#define SGM415XX_CHRG_CTRL_b	0x0b
#define SGM415XX_CHRG_CTRL_c	0x0c
#define SGM415XX_CHRG_CTRL_d	0x0d
#define SGM415XX_INPUT_DET		0x0e
#define SGM415XX_CHRG_CTRL_f	0x0f

/* charge status flags */
#define SGM415XX_CHRG_EN		BIT(4)
#define SGM415XX_HIZ_EN			BIT(7)
#define SGM415XX_TERM_EN		BIT(7)
#define SGM415XX_VAC_OVP_MASK	GENMASK(7, 6)
#define SGM415XX_DPDM_ONGOING	BIT(7)
#define SGM415XX_VBUS_GOOD		BIT(7)

#define SGM415XX_BOOSTV			GENMASK(5, 4)
#define SGM415XX_BOOST_LIM		BIT(7)
#define SGM415XX_OTG_EN			BIT(5)

#define SGM415XX_INPUT_DET_DONE		BIT(7)

/* Part ID */
#define SGM415XX_PN_MASK			GENMASK(6, 3)

/* WDT TIMER SET  */
#define SGM415XX_WDT_TIMER_MASK			GENMASK(5, 4)
#define SGM415XX_WDT_TIMER_SHIFT		4
#define SGM415XX_WDT_TIMER_DISABLE		0
#define SGM415XX_WDT_TIMER_40S			1
#define SGM415XX_WDT_TIMER_80S			2
#define SGM415XX_WDT_TIMER_160S			3

#define SGM415XX_WDT_RST_MASK			BIT(6)

/* SAFETY TIMER SET */
#define SGM415XX_SAFETY_TIMER_MASK		GENMASK(3, 3)
#define SGM415XX_SAFETY_TIMER_DISABLE	0
#define SGM415XX_SAFETY_TIMER_EN		BIT(3)
#define SGM415XX_SAFETY_TIMER_5H		0
#define SGM415XX_SAFETY_TIMER_10H		BIT(2)

/* recharge voltage */
#define SGM415XX_VRECHARGE				BIT(0)
#define SGM415XX_VRECHRG_STEP_mV		100
#define SGM415XX_VRECHRG_OFFSET_mV		100

/* charge status */
#define SGM415XX_VSYS_STAT		BIT(0)
#define SGM415XX_THERM_STAT		BIT(1)
#define SGM415XX_PG_STAT		BIT(2)
#define SGM415XX_CHG_STAT_MASK	GENMASK(4, 3)
#define SGM415XX_CHG_STAT_SHIFT	3
#define SGM415XX_PRECHRG		1
#define SGM415XX_FAST_CHRG		2
#define SGM415XX_TERM_CHRG		3

/* charge type */
#define SGM415XX_VBUS_STAT_MASK		GENMASK(7, 5)
#define SGM415XX_VBUS_STAT_SHIFT	5
#define SGM415XX_NOT_CHRGING	0
#define SGM415XX_USB_SDP		1
#define SGM415XX_USB_CDP		2
#define SGM415XX_USB_DCP		3
#define SGM415XX_UNKNOWN		5
#define SGM415XX_NON_STANDARD	6
#define SGM415XX_OTG_MODE		7

/* fault Status */
#define SGM415XX_WDT_FAULT_MASK		BIT(7)
#define SGM415XX_BOOST_FAULT_MASK	BIT(6)
#define SGM415XX_CHRG_FAULT_MASK	GENMASK(5, 4)
#define SGM415XX_CHRG_FAULT_SHIFT	4
#define SGM415XX_BAT_FAULT_MASK		BIT(3)

/* TEMP Status */
#define SGM415XX_TEMP_MASK		GENMASK(2, 0)
#define SGM415XX_TEMP_NORMAL	0
#define SGM415XX_TEMP_WARM		2
#define SGM415XX_TEMP_COOL		3
#define SGM415XX_TEMP_COLD		5
#define SGM415XX_TEMP_HOT		6

/* precharge current */
#define SGM415XX_PRECHRG_CUR_MASK			GENMASK(7, 4)
#define SGM415XX_PRECHRG_CURRENT_STEP_uA	60000
#define SGM415XX_PRECHRG_I_MIN_uA			60000
#define SGM415XX_PRECHRG_I_MAX_uA			780000
#define SGM415XX_PRECHRG_I_DEF_uA			180000

/* termination current */
#define SGM415XX_TERMCHRG_CUR_MASK			GENMASK(3, 0)
#define SGM415XX_TERMCHRG_CURRENT_STEP_uA	60000
#define SGM415XX_TERMCHRG_I_MIN_uA			60000
#define SGM415XX_TERMCHRG_I_MAX_uA			960000
#define SGM415XX_TERMCHRG_I_DEF_uA			180000

/* charge current */
#define SGM415XX_ICHRG_I_MASK		GENMASK(5, 0)
#define SGM415XX_ICHRG_I_STEP_uA	60000
#define SGM415XX_ICHRG_I_MIN_uA		0

/* charge voltage */
#define SGM415XX_VREG_V_MASK		GENMASK(7, 3)
#define SGM415XX_VREG_V_MAX_uV		4624000
#define SGM415XX_VREG_V_MIN_uV		3856000
#define SGM415XX_VREG_V_DEF_uV		4208000
#define SGM415XX_VREG_V_STEP_uV		32000

/* iindpm current */
#define SGM415XX_IINDPM_I_MASK		GENMASK(4, 0)
#define SGM415XX_IINDPM_I_MIN_uA	100000

#define SGM415XX_IINDPM_STEP_uA		100000
#define SGM415XX_IINDPM_DEF_uA		2400000

/* vindpm voltage */
#define SGM415XX_VINDPM_V_MASK		GENMASK(3, 0)
#define SGM415XX_VINDPM_V_MIN_uV	3900000
#define SGM415XX_VINDPM_V_MAX_uV	12000000
#define SGM415XX_VINDPM_STEP_uV		100000
#define SGM415XX_VINDPM_DEF_uV		4500000
#define SGM415XX_VINDPM_OS_MASK		GENMASK(1, 0)

/* DP DM SEL */
#define SGM415XX_DP_VSEL_MASK		GENMASK(4, 3)
#define SGM415XX_DM_VSEL_MASK		GENMASK(2, 1)

#define SGM415XX_DP_VOLT_SHIFT		3
#define SGM415XX_DM_VOLT_SHIFT		1

/* PUMPX SET */
#define SGM415XX_EN_PUMPX			BIT(7)
#define SGM415XX_PUMPX_UP			BIT(6)
#define SGM415XX_PUMPX_DN			BIT(5)

#define SGM415XX_IINDET_EN			BIT(7)

//SGM41512S
#define SGM415XX_CHRG_CTRL_10		0x10
#define SGM415XX_BOOST_LIM_SEL		BIT(6)
#define SGM415XX_BC12_AUTO_DIS		BIT(5)
#define SGM415XX_BATOCP_SET			BIT(4)
#define SGM415XX_BATOCP_DEG			GENMASK(3, 2)
#define SGM415XX_OTG_FREQ			BIT(1)
#define SGM415XX_ITERM_PRE_SEL		BIT(0)

#define AC_POWER_THRESHOLD 300//mV, the value above this threshold is AC
#define DC_POWER_THRESHOLD 50//mV, the value under this threshold is DC

extern int cw_capacity;

enum Chg_Part_id {
	SGM41513_PN_ID,
	SGM41513D_PN_ID,
	SGM41515_PN_ID,
	SGM41515D_PN_ID,
	SGM41516_PN_ID,
	SGM41516D_PN_ID,
	SGM41541_PN_ID,
	SGM41542_PN_ID,
	SGM41543_PN_ID,
	SGM41543D_PN_ID,
	SGM41512S_PN_ID,
	SGM41512SD_PN_ID,
	MAX_PN_ID,
};

struct sgm_vendor_info {
	char *name;
	u8 part_id;
};

struct sgm415xx_init_data {
	u32 ichg;	/* charge current		*/
	u32 ilim;	/* input current		*/
	u32 vreg;	/* regulation voltage		*/
	u32 iterm;	/* termination current		*/
	u32 iprechg;	/* precharge current		*/
	u32 vlim;	/* minimum system voltage limit */
	u32 sys_min;	/* system min output voltage */
	u32 max_ichg;
	u32 max_vreg;
	u32 max_ilim;
};

enum {
	SGM_DP_DM_VOL_HIZ,
	SGM_DP_DM_VOL_0P0,
	SGM_DP_DM_VOL_0P6,
	SGM_DP_DM_VOL_3P3,
};

enum SGM41544_QC_VOLT {
	QC_20_5000mV,
	QC_20_9000mV,
	QC_20_12000mV,
};

enum SGM415XX_POWER_TYPE {
	POWER_TYPE_BATTERY,
	POWER_TYPE_VBUS_DC,
	POWER_TYPE_VBUS_AC,
	POWER_TYPE_UNKNOWN
};

struct sgm415xx_state {
	u8 vbus_type;	 //REG0x08 D[7:5]
	u8 chrg_stat;	 //REG0x08 D[4:3]
	bool online;	 //REG0x08 D[2]
	bool vsys_stat;  //REG0x08 D[0]
	bool therm_stat; //REG0x08 D[1]
	bool wdt_fault;  //REG0x09 D[7]
	bool boost_fault;//REG0x09 D[6]
	u8 chrg_fault;	 //REG0x09 D[5:4]
	bool bat_fault;  //REG0x09 D[3]
	u8 ntc_fault;	 //REG0x09 D[2:0]
	bool vbus_gd;	 //REG0x0a D[7]
	enum SGM415XX_POWER_TYPE power_type;
};

struct sgm415xx_reg {
	u8 reg_addr;
	u8 value;
};

#endif /* _SGM415XX_CHARGER_H */
