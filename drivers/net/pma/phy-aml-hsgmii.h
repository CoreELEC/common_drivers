/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2026 Amlogic, Inc. All rights reserved.
 */

#ifndef AML_PHY_HSGMII_H_
#define AML_PHY_HSGMII_H_

/* AML_PHY_HSG_MAC_STATUS (0x094) */
union hsg_mac_status_reg {
	u32 val;
	struct {
		u32 rx_fsm_ready	: 1;
		u32 pcs_acquired_sync	: 1;
		u32 sgmii_link_spd	: 2;
		u32 pcs_ewrap		: 1;
		u32 pcs_en_cdet		: 1;
		u32 pcs_lck_ref		: 1;
		u32 reserved0		: 25;
	} reg;
};

/* AML_PHY_HSG_INIT_DEBUG (0x204) */
union hsg_init_debug_reg {
	u32 val;
	struct {
		u32 hsgmii_mode_rst_n	: 1;
		u32 hsgmii_clk_en	: 1;
		u32 phy_link_status_final	: 1;
		u32 reserved0		: 1;
		u32 hsgmii_speed	: 2;
		u32 phy_speed_final	: 2;
		u32 hsi_cstate		: 6;
		u32 reserved1		: 18;
	} reg;
};

/* AML_PHY_HSG_PLL_INIT_DEBUG (0x208) */
union hsg_pll_init_debug_reg {
	u32 val;
	struct {
		u32 pll0_lock		: 1;
		u32 pll1_lock		: 1;
		u32 reserved0		: 2;
		u32 pll0_fail_cnt	: 4;
		u32 pll1_fail_cnt	: 4;
		u32 reserved1		: 8;
		u32 pll_cstate		: 12;
	} reg;
};

/* AML_PHY_HSG_RX_CTRL_DEBUG (0x20c) */
union hsg_rx_ctrl_debug_reg {
	u32 val;
	struct {
		u32 pcs_acquired_sync_cdc: 1;
		u32 upcrx_cdlock_sync	: 1;
		u32 rx_fsm_ready	: 1;
		u32 reserved0		: 1;
		u32 lock_fail_cnt	: 4;
		u32 reserved1		: 8;
		u32 rx_fsm_cstate	: 15;
		u32 reserved2		: 1;
	} reg;
};

/* AML_PHY_HSG_RX_DEBUG (0x210) */
union hsg_rx_debug_reg {
	u32 val;
	struct {
		u32 upcrx_state		: 13;
		u32 reserved0		: 3;
		u32 upcrx_dbg_eq	: 5;
		u32 reserved1		: 11;
	} reg;
};

struct hsg_phy_stats {
	union hsg_mac_status_reg mac_status;
	union hsg_init_debug_reg init_dbg;
	union hsg_pll_init_debug_reg pll_dbg;
	union hsg_rx_ctrl_debug_reg rx_ctrl_dbg;
	union hsg_rx_debug_reg rx_dbg;
};

#define HSG_SPEED_1000M				0
#define HSG_SPEED_2500M				1
#define HSG_SPEED_10M				2
#define HSG_SPEED_100M				3

#define AML_PHY_HSG_RX_CFG0				0x000
#define AML_PHY_HSG_RX_CFG1				0x004
#define AML_PHY_HSG_RX_CFG2				0x008
#define AML_PHY_HSG_RX_CFG3				0x00c
#define AML_PHY_HSG_RX_CFG4				0x010
#define AML_PHY_HSG_RX_CFG5				0x014
#define AML_PHY_HSG_RX_CFG6				0x018
#define AML_PHY_HSG_RX_CFG7				0x01c
#define AML_PHY_HSG_RX_CFG8				0x020
#define AML_PHY_HSG_RX_CFG9				0x024
#define AML_PHY_HSG_RX_CFG10				0x028
#define AML_PHY_HSG_RX_FSM_CTRL				0x02c
#define  HSG_RX_FSM_CTRL_RESTART			BIT(0)
#define  HSG_RX_FSM_CTRL_AUTO_RESTART			BIT(1)
#define  HSG_RX_FSM_CTRL_RESTART_MODE			BIT(2)
#define  HSG_RX_FSM_CTRL_PCS_SYNC_BYPASS		BIT(3)
#define  HSG_RX_FSM_CTRL_EQ_TRAINING_BYPASS		BIT(4)
#define  HSG_RX_FSM_CTRL_DFE_BYPASS			BIT(5)
#define  HSG_RX_FSM_CTRL_EQ_MODE_INITIAL_MASK		GENMASK(9, 8)
#define  HSG_RX_FSM_CTRL_EQ_MODE_WORK_MASK		GENMASK(11, 10)
#define AML_PHY_HSG_PMA_TIMER_CFG0			0x030
#define AML_PHY_HSG_PMA_TIMER_CFG1			0x034
#define AML_PHY_HSG_PMA_TIMER_CFG2			0x038
#define AML_PHY_HSG_PMA_TIMER_CFG3			0x03c
#define AML_PHY_HSG_PMA_TIMER_CFG4			0x040
#define AML_PHY_HSG_PMA_TIMER_CFG5			0x044
#define AML_PHY_HSG_PMA_TIMER_CFG6			0x048
#define AML_PHY_HSG_PMA_TIMER_CFG7			0x04c
#define AML_PHY_HSG_PMA_TIMER_CFG8			0x050
#define AML_PHY_HSG_PMA_TIMER_CFG9			0x054
#define AML_PHY_HSG_PMA_TIMER_CFG10			0x058
#define AML_PHY_HSG_PMA_TIMER_CFG11			0x05c
#define AML_PHY_HSG_PMA_TIMER_CFG12			0x060
#define AML_PHY_HSG_PMA_TIMER_CFG13			0x064
#define AML_PHY_HSG_PMA_TIMER_CFG14			0x068
#define AML_PHY_HSG_PMA_TIMER_CFG15			0x06c
#define AML_PHY_HSG_PMA_TIMER_CFG16			0x070
#define AML_PHY_HSG_PMA_TIMER_CFG17			0x074
#define AML_PHY_HSG_PMA_TIMER_CFG18			0x078
#define AML_PHY_HSG_PMA_TIMER_CFG19			0x07c
#define AML_PHY_HSG_TX_CFG0				0x080
#define AML_PHY_HSG_TX_CFG1				0x084
#define AML_PHY_HSG_TX_CFG2				0x088
#define AML_PHY_HSG_TX_CFG3				0x08c
#define AML_PHY_HSG_INIT_CTRL				0x090
#define  HSG_INIT_CTRL_RESET				BIT(0)
#define  HSG_INIT_CTRL_PHY_LINK_STATUS			BIT(1)
#define  HSG_INIT_CTRL_SPEED_EN				BIT(2)
#define  HSG_INIT_CTRL_SPEED_MASK			GENMASK(4, 3)
#define  HSG_INIT_CTRL_INITIAL_MODE			BIT(5)
#define  HSG_INIT_CTRL_WAIT_SPEED_EN			BIT(6)
#define  HSG_INIT_CTRL_SPEED_DETECT_TIMER_MASK		GENMASK(31, 16)
#define  HSG_INIT_CTRL_SPEED_DETECT_TIMER_SHIFT		16
#define AML_PHY_HSG_MAC_STATUS				0x094
#define  HSG_MAC_STATUS_RX_FSM_READY			BIT(0)
#define  HSG_MAC_STATUS_PCS_ACQUIRED_SYNC		BIT(1)
#define  HSG_MAC_STATUS_SGMII_LINK_SPEED_MASK		GENMASK(3, 2)
#define  HSG_MAC_STATUS_PCS_EWRAP			BIT(4)
#define  HSG_MAC_STATUS_PCS_EN_CDET			BIT(5)
#define  HSG_MAC_STATUS_PCS_LCK_REF			BIT(6)
#define AML_PHY_HSG_MAC_CTRL				0x098
#define  HSG_MAC_CTRL_SIGNAL_DET_BYPASS			BIT(0)
#define  HSG_MAC_CTRL_TX_CLK_SW_RST			BIT(1)
#define  HSG_MAC_CTRL_RX_CLK_SW_RST			BIT(2)
#define  HSG_MAC_CTRL_TX_125M_CLK_SW_RST		BIT(3)
#define  HSG_MAC_CTRL_RX_125M_CLK_SW_RST		BIT(4)
#define AML_PHY_HSG_PLL_FSM_CTRL			0x09c
#define  HSG_PLL_FSM_CTRL_RESTART			BIT(0)
#define  HSG_PLL_FSM_CTRL_PLL_DONE			BIT(1)
#define  HSG_PLL_FSM_CTRL_RESTART_MODE			BIT(2)
#define AML_PHY_HSG_TIMER_PLL0_INIT			0x0a0
#define AML_PHY_HSG_TIMER_PLL0_OP1			0x0a4
#define AML_PHY_HSG_TIMER_PLL0_OP2			0x0a8
#define AML_PHY_HSG_TIMER_PLL0_OP3			0x0ac
#define AML_PHY_HSG_TIMER_PLL0_RESERVED			0x0b0
#define AML_PHY_HSG_TIMER_PLL1_INIT			0x0b4
#define AML_PHY_HSG_TIMER_PLL1_OP1			0x0b8
#define AML_PHY_HSG_TIMER_PLL1_OP2			0x0bc
#define AML_PHY_HSG_TIMER_PLL1_OP3			0x0c0
#define AML_PHY_HSG_TIMER_PLL1_RESERVED			0x0c4
#define AML_PHY_HSG_BYPASS_PLL_CTRL			0x0c8
#define  HSG_BYPASS_PLL_CTRL_DEBUG_MODE			BIT(0)
#define AML_PHY_HSG_PLL0_HCSL_L_REG0			0x0cc
#define AML_PHY_HSG_PLL0_L_REG0				0x0d0
#define AML_PHY_HSG_PLL0_L_REG1				0x0d4
#define AML_PHY_HSG_PLL1_L_REG0				0x0d8
#define AML_PHY_HSG_PLL1_L_REG1				0x0dc
#define AML_PHY_HSG_PLL1_L_REG2				0x0e0
#define AML_PHY_HSG_PLL0_HCSL_H_REG0			0x0e4
#define AML_PHY_HSG_PLL0_H_REG0				0x0e8
#define AML_PHY_HSG_PLL0_H_REG1				0x0ec
#define AML_PHY_HSG_PLL1_H_REG0				0x0f0
#define AML_PHY_HSG_PLL1_H_REG1				0x0f4
#define AML_PHY_HSG_PLL1_H_REG2				0x0f8
#define AML_PHY_HSG_PMA_CTRL				0x0fc
#define  HSG_PMA_CTRL_TX_TBI_INVERT			BIT(0)
#define  HSG_PMA_CTRL_RX_TBI_INVERT			BIT(1)
#define  HSG_PMA_CTRL_TX_POLARITY_INVERT		BIT(2)
#define  HSG_PMA_CTRL_RX_POLARITY_INVERT		BIT(3)
#define  HSG_PMA_CTRL_PMA_REMOTE_LOOPBACK		BIT(4)
#define AML_PHY_HSG_MDIO_CTRL				0x100
#define  HSG_MDIO_CTRL_MDIO_SWITCH			BIT(0)

#define AML_PHY_HSG_DEBUG_SEL				0x200
#define AML_PHY_HSG_INIT_DEBUG				0x204
#define  HSG_INIT_DEBUG_RST_N				BIT(0)
#define  HSG_INIT_DEBUG_CLK_EN				BIT(1)
#define  HSG_INIT_DEBUG_PHY_LINK_STATUS			BIT(2)
#define  HSG_INIT_DEBUG_HSGMII_SPEED_MASK		GENMASK(5, 4)
#define  HSG_INIT_DEBUG_PHY_SPEED_MASK			GENMASK(7, 6)
#define  HSG_INIT_DEBUG_HSI_CSTATE_MASK			GENMASK(13, 8)
#define AML_PHY_HSG_PLL_INIT_DEBUG			0x208
#define  HSG_PLL_INIT_DEBUG_PLL0_LOCK			BIT(0)
#define  HSG_PLL_INIT_DEBUG_PLL1_LOCK			BIT(1)
#define  HSG_PLL_INIT_DEBUG_PLL0_FAIL_CNT_MASK		GENMASK(7, 4)
#define  HSG_PLL_INIT_DEBUG_PLL1_FAIL_CNT_MASK		GENMASK(11, 8)
#define  HSG_PLL_INIT_DEBUG_PLL_CSTATE_MASK		GENMASK(31, 20)
#define AML_PHY_HSG_RX_CTRL_DEBUG			0x20c
#define  HSG_RX_CTRL_DEBUG_PCS_SYNC_CDC			BIT(0)
#define  HSG_RX_CTRL_DEBUG_UPCRX_CDRLOCK_SYNC		BIT(1)
#define  HSG_RX_CTRL_DEBUG_RX_FSM_READY			BIT(2)
#define  HSG_RX_CTRL_DEBUG_LOCK_FAIL_CNT_MASK		GENMASK(7, 4)
#define  HSG_RX_CTRL_DEBUG_RX_FSM_CSTATE_MASK		GENMASK(30, 16)
#define AML_PHY_HSG_RX_DEBUG				0x210
#define  HSG_RX_DEBUG_UPCRX_STATE_MASK			GENMASK(12, 0)
#define  HSG_RX_DEBUG_UPCRX_DBG_EQ_MASK			GENMASK(20, 16)

#endif
