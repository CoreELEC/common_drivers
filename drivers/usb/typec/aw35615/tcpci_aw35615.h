/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 **** Copyright (C), 2020-2022, Awinic.All rights reserved. ************
 *******************************************************************************
 * File Name     : tcpci_aw35615.h
 * Author        : awinic
 * Date          : 2022-12-26
 * Description   : .h file function description
 * Version       : 1.0
 * Function List :
 ******************************************************************************/

#ifndef AW35615_REG_H
#define AW35615_REG_H
#include <linux/version.h>
#include <linux/kernel.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#define AW_KERNEL_VER_OVER_5_10_0
#endif

/*
 * When the device is SNK, BC_LVL interrupt is used to monitor cc pins
 * for the current capability offered by the SRC. As AW35615 chip fires
 * the BC_LVL interrupt on PD signalings, cc lvl should be handled after
 * a delay to avoid measuring on PD activities. The delay is slightly
 * longer than PD_T_PD_DEBOUNCE (10-20ms).
 */
#define T_BC_LVL_DEBOUNCE_DELAY_MS		(30)
#define NEGOTIATED_REV					(2)

#define AW35615_VID						(0x90)

#define LOG_BUFFER_ENTRIES				(1024)
#define LOG_BUFFER_ENTRY_SIZE			(128)

#define AW_REG_DEVICE_ID				(0x01)
#define AW_REG_SWITCHES0				(0x02)
#define AW_REG_SWITCHES0_CC2_PU_EN		BIT(7)
#define AW_REG_SWITCHES0_CC1_PU_EN		BIT(6)
#define AW_REG_SWITCHES0_VCONN_CC2		BIT(5)
#define AW_REG_SWITCHES0_VCONN_CC1		BIT(4)
#define AW_REG_SWITCHES0_MEAS_CC2		BIT(3)
#define AW_REG_SWITCHES0_MEAS_CC1		BIT(2)
#define AW_REG_SWITCHES0_CC2_PD_EN		BIT(1)
#define AW_REG_SWITCHES0_CC1_PD_EN		BIT(0)
#define AW_REG_SWITCHES1				(0x03)
#define AW_REG_SWITCHES1_POWERROLE		BIT(7)
#define AW_REG_SWITCHES1_SPECREV1		BIT(6)
#define AW_REG_SWITCHES1_SPECREV0		BIT(5)
#define AW_REG_SWITCHES1_DATAROLE		BIT(4)
#define AW_REG_SWITCHES1_AUTO_GCRC		BIT(2)
#define AW_REG_SWITCHES1_TXCC2_EN		BIT(1)
#define AW_REG_SWITCHES1_TXCC1_EN		BIT(0)
#define AW_REG_MEASURE					(0x04)
#define AW_REG_MEASURE_MDAC5			BIT(7)
#define AW_REG_MEASURE_MDAC4			BIT(6)
#define AW_REG_MEASURE_MDAC3			BIT(5)
#define AW_REG_MEASURE_MDAC2			BIT(4)
#define AW_REG_MEASURE_MDAC1			BIT(3)
#define AW_REG_MEASURE_MDAC0			BIT(2)
#define AW_REG_MEASURE_VBUS				BIT(1)
#define AW_REG_MEASURE_XXXX5			BIT(0)
#define AW_REG_CONTROL0					(0x06)
#define AW_REG_CONTROL0_TX_FLUSH		BIT(6)
#define AW_REG_CONTROL0_INT_MASK		BIT(5)
#define AW_REG_CONTROL0_HOST_CUR_MASK	(0xC)
#define AW_REG_CONTROL0_HOST_CUR_HIGH	(0xC)
#define AW_REG_CONTROL0_HOST_CUR_MED	(0x8)
#define AW_REG_CONTROL0_HOST_CUR_DEF	(0x4)
#define AW_REG_CONTROL0_TX_START		BIT(0)
#define AW_REG_CONTROL1					(0x07)
#define AW_REG_CONTROL1_ENSOP2DB		BIT(6)
#define AW_REG_CONTROL1_ENSOP1DB		BIT(5)
#define AW_REG_CONTROL1_BIST_MODE2		BIT(4)
#define AW_REG_CONTROL1_RX_FLUSH		BIT(2)
#define AW_REG_CONTROL1_ENSOP2			BIT(1)
#define AW_REG_CONTROL1_ENSOP1			BIT(0)
#define AW_REG_CONTROL2					(0x08)
#define AW_REG_CONTROL2_MODE			BIT(1)
#define AW_REG_CONTROL2_MODE_MASK		(0x6)
#define AW_REG_CONTROL2_MODE_DFP		(0x6)
#define AW_REG_CONTROL2_MODE_UFP		(0x4)
#define AW_REG_CONTROL2_MODE_DRP		(0x2)
#define AW_REG_CONTROL2_MODE_NONE		(0x0)
#define AW_REG_CONTROL2_TOGGLE			BIT(0)
#define AW_REG_CONTROL3					(0x09)
#define AW_REG_CONTROL3_SEND_HARDRESET	BIT(6)
#define AW_REG_CONTROL3_BIST_TMODE		BIT(5)
#define AW_REG_CONTROL3_AUTO_HARDRESET	BIT(4)
#define AW_REG_CONTROL3_AUTO_SOFTRESET	BIT(3)
#define AW_REG_CONTROL3_N_RETRIES		BIT(1)
#define AW_REG_CONTROL3_N_RETRIES_MASK	(0x6)
#define AW_REG_CONTROL3_N_RETRIES_3		(0x6)
#define AW_REG_CONTROL3_N_RETRIES_2		(0x4)
#define AW_REG_CONTROL3_N_RETRIES_1		(0x2)
#define AW_REG_CONTROL3_AUTO_RETRY		BIT(0)
#define AW_REG_MASK						(0x0A)
#define AW_REG_MASK_VBUSOK				BIT(7)
#define AW_REG_MASK_ACTIVITY			BIT(6)
#define AW_REG_MASK_COMP_CHNG			BIT(5)
#define AW_REG_MASK_CRC_CHK				BIT(4)
#define AW_REG_MASK_ALERT				BIT(3)
#define AW_REG_MASK_WAKE				BIT(2)
#define AW_REG_MASK_COLLISION			BIT(1)
#define AW_REG_MASK_BC_LVL				BIT(0)
#define AW_REG_POWER					(0x0B)
#define AW_REG_POWER_PWR				BIT(0)
#define AW_REG_POWER_PWR_LOW			(0x1)
#define AW_REG_POWER_PWR_MEDIUM			(0x3)
#define AW_REG_POWER_PWR_HIGH			(0x7)
#define AW_REG_POWER_PWR_ALL			(0xF)
#define AW_REG_RESET					(0x0C)
#define AW_REG_RESET_PD_RESET			BIT(1)
#define AW_REG_RESET_SW_RESET			BIT(0)
#define AW_REG_MASKA					(0x0E)
#define AW_REG_MASKA_OCP_TEMP			BIT(7)
#define AW_REG_MASKA_TOGDONE			BIT(6)
#define AW_REG_MASKA_SOFTFAIL			BIT(5)
#define AW_REG_MASKA_RETRYFAIL			BIT(4)
#define AW_REG_MASKA_HARDSENT			BIT(3)
#define AW_REG_MASKA_TX_SUCCESS			BIT(2)
#define AW_REG_MASKA_SOFTRESET			BIT(1)
#define AW_REG_MASKA_HARDRESET			BIT(0)
#define AW_REG_MASKB					(0x0F)
#define AW_REG_MASKB_GCRCSENT			BIT(0)
#define AW_REG_CONTROL4					(0x10)
#define AW_REG_EN_PAR_CFG				BIT(1)
#define AW_REG_TOG_EXIT_AUD				BIT(0)
#define AW_REG_STATUS0A					0x3C
#define AW_REG_STATUS0A_SOFTFAIL		BIT(5)
#define AW_REG_STATUS0A_RETRYFAIL		BIT(4)
#define AW_REG_STATUS0A_POWER			BIT(2)
#define AW_REG_STATUS0A_RX_SOFT_RESET	BIT(1)
#define AW_REG_STATUS0A_RX_HARD_RESET	BIT(0)
#define AW_REG_STATUS1A					(0x3D)
#define AW_REG_STATUS1A_TOGSS			BIT(3)
#define AW_REG_STATUS1A_TOGSS_RUNNING	(0x0)
#define AW_REG_STATUS1A_TOGSS_SRC1		(0x1)
#define AW_REG_STATUS1A_TOGSS_SRC2		(0x2)
#define AW_REG_STATUS1A_TOGSS_SNK1		(0x5)
#define AW_REG_STATUS1A_TOGSS_SNK2		(0x6)
#define AW_REG_STATUS1A_TOGSS_AA		(0x7)
#define AW_REG_STATUS1A_TOGSS_POS		(3)
#define AW_REG_STATUS1A_TOGSS_MASK		(0x7)
#define AW_REG_STATUS1A_RXSOP2DB		BIT(2)
#define AW_REG_STATUS1A_RXSOP1DB		BIT(1)
#define AW_REG_STATUS1A_RXSOP			BIT(0)
#define AW_REG_INTERRUPTA				(0x3E)
#define AW_REG_INTERRUPTA_OCP_TEMP		BIT(7)
#define AW_REG_INTERRUPTA_TOGDONE		BIT(6)
#define AW_REG_INTERRUPTA_SOFTFAIL		BIT(5)
#define AW_REG_INTERRUPTA_RETRYFAIL		BIT(4)
#define AW_REG_INTERRUPTA_HARDSENT		BIT(3)
#define AW_REG_INTERRUPTA_TX_SUCCESS	BIT(2)
#define AW_REG_INTERRUPTA_SOFTRESET		BIT(1)
#define AW_REG_INTERRUPTA_HARDRESET		BIT(0)
#define AW_REG_INTERRUPTB				(0x3F)
#define AW_REG_INTERRUPTB_GCRCSENT		BIT(0)
#define AW_REG_STATUS0					(0x40)
#define AW_REG_STATUS0_VBUSOK			BIT(7)
#define AW_REG_STATUS0_ACTIVITY			BIT(6)
#define AW_REG_STATUS0_COMP				BIT(5)
#define AW_REG_STATUS0_CRC_CHK			BIT(4)
#define AW_REG_STATUS0_ALERT			BIT(3)
#define AW_REG_STATUS0_WAKE				BIT(2)
#define AW_REG_STATUS0_BC_LVL_MASK		(0x03)
#define AW_REG_STATUS0_BC_LVL_0_200		(0x0)
#define AW_REG_STATUS0_BC_LVL_200_600	(0x1)
#define AW_REG_STATUS0_BC_LVL_600_1230	(0x2)
#define AW_REG_STATUS0_BC_LVL_1230_MAX	(0x3)
#define AW_REG_STATUS0_BC_LVL1			BIT(1)
#define AW_REG_STATUS0_BC_LVL0			BIT(0)
#define AW_REG_STATUS1					(0x41)
#define AW_REG_STATUS1_RXSOP2			BIT(7)
#define AW_REG_STATUS1_RXSOP1			BIT(6)
#define AW_REG_STATUS1_RX_EMPTY			IT(5)
#define AW_REG_STATUS1_RX_FULL			BIT(4)
#define AW_REG_STATUS1_TX_EMPTY			BIT(3)
#define AW_REG_STATUS1_TX_FULL			BIT(2)
#define AW_REG_INTERRUPT				(0x42)
#define AW_REG_INTERRUPT_VBUSOK			BIT(7)
#define AW_REG_INTERRUPT_ACTIVITY		BIT(6)
#define AW_REG_INTERRUPT_COMP_CHNG		BIT(5)
#define AW_REG_INTERRUPT_CRC_CHK		BIT(4)
#define AW_REG_INTERRUPT_ALERT			BIT(3)
#define AW_REG_INTERRUPT_WAKE			BIT(2)
#define AW_REG_INTERRUPT_COLLISION		BIT(1)
#define AW_REG_INTERRUPT_BC_LVL			BIT(0)
#define AW_REG_FIFOS					(0x43)

/* Tokens defined for the AW35615 TX FIFO */
enum aw35615_txfifo_tokens {
	AW35615_TKN_TXON = 0xA1,
	AW35615_TKN_SYNC1 = 0x12,
	AW35615_TKN_SYNC2 = 0x13,
	AW35615_TKN_SYNC3 = 0x1B,
	AW35615_TKN_RST1 = 0x15,
	AW35615_TKN_RST2 = 0x16,
	AW35615_TKN_PACKSYM = 0x80,
	AW35615_TKN_JAMCRC = 0xFF,
	AW35615_TKN_EOP = 0x14,
	AW35615_TKN_TXOFF = 0xFE,
};

enum toggling_mode {
	TOGGLING_MODE_OFF,
	TOGGLING_MODE_DRP,
	TOGGLING_MODE_SNK,
	TOGGLING_MODE_SRC,
};

enum src_current_status {
	SRC_CURRENT_DEFAULT,
	SRC_CURRENT_MEDIUM,
	SRC_CURRENT_HIGH,
};

static const u8 ra_mda_value[] = {
	[SRC_CURRENT_DEFAULT] = 4,	/* 210mV */
	[SRC_CURRENT_MEDIUM] = 9,	/* 420mV */
	[SRC_CURRENT_HIGH] = 18,	/* 798mV */
};

static const u8 rd_mda_value[] = {
	[SRC_CURRENT_DEFAULT] = 38,	/* 1638mV */
	[SRC_CURRENT_MEDIUM] = 38,	/* 1638mV */
	[SRC_CURRENT_HIGH] = 61,	/* 2604mV */
};

struct aw35615_chip {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct tcpm_port *tcpm_port;
	struct tcpc_dev tcpc_dev;

	struct regulator *vbus;
#ifdef MESON_AW
	struct gpio_desc *vbus_gpio;
#endif

	spinlock_t irq_lock;
	struct work_struct irq_work;
	bool irq_suspended;
	bool irq_while_suspended;
	int gpio_int_n;
	int gpio_int_n_irq;
	struct extcon_dev *extcon;

	struct workqueue_struct *wq;
	struct delayed_work bc_lvl_handler;

	/* lock for sharing chip states */
	struct mutex lock;

	/* chip status */
	enum toggling_mode toggling_mode;
	enum src_current_status src_current_status;
	bool intr_togdone;
	bool intr_bc_lvl;
	bool intr_comp_chng;

	/* port status */
	bool vconn_on;
	bool vbus_on;
	bool charge_on;
	bool vbus_present;
	enum typec_cc_polarity cc_polarity;
	enum typec_cc_status cc1;
	enum typec_cc_status cc2;
	u32 snk_pdo[PDO_MAX_OBJECTS];

#ifdef CONFIG_DEBUG_FS
	struct dentry *dentry;
	/* lock for log buffer access */
	struct mutex logbuffer_lock;
	int logbuffer_head;
	int logbuffer_tail;
	u8 *logbuffer[LOG_BUFFER_ENTRIES];
#endif
};

#define AWINIC_DEBUG
#ifdef AWINIC_DEBUG
#define AWINIC_LOG_NAME "aw35615"
#define AW_LOG(format, arg...)	 pr_info("[%s] %s %d: " format, AWINIC_LOG_NAME, \
		 __func__, __LINE__, ##arg)
#else
#define AW_LOG(format, arg...)
#endif

#endif
