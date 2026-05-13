/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DPTX_IP_CTRLS_H_
#define _DPTX_IP_CTRLS_H_

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "../edptx_common.h"

//Source: Table 2-58: uPacket TX AUX CH State and Event Descriptions
//#define DPTX_AUX_REPLY_WAIT_TIMEOUT 5   //times
#define DPTX_AUX_REPLY_WAIT_TIMEOUT 120   //times
#define DPTX_AUX_REPLY_WAIT_TIMER   5   //us (protocol: 400us, check for each 50us, check 10 times)
#define DPTX_AUX_NO_REPLY_TIMEOUT   1   //ms
#define DPTX_AUX_NO_REPLY_RETRY     10   //times
//#define DPTX_AUX_NO_REPLY_RETRY     5   //times
#define AUX_STATUS_REPLY_ERROR         BIT(3)
#define AUX_STATUS_REQUEST_IN_PROGRESS BIT(2)
#define AUX_STATUS_REPLY_IN_PROGRESS   BIT(1)
#define AUX_STATUS_REPLY_RECEIVED      BIT(0)

#define AUX_REPLY_CODE_ACK       0x0
#define AUX_REPLY_CODE_AUX_NACK  0x1
#define AUX_REPLY_CODE_AUX_Defer 0x2
#define AUX_REPLY_CODE_I2C_NACK  0x4
#define AUX_REPLY_CODE_I2C_Defer 0x8

struct edptx_if_ctrl_s {
	u8 (*aux_write)(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf);
	u8 (*aux_write_single)(struct edptx_drv_s *edp, u8 port, u32 addr, u8 val);
	u8 (*aux_read)(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf);
	u8 (*aux_i2c_op)(struct edptx_drv_s *edp, u8 port,
			u8 cmd_type, u32 dev_addr, u8 len, u8 *data);
	u8 (*aux_read_edid)(struct edptx_drv_s *edp, u8 port, u8 *data, u16 len);

	void (*transmit_pattern)(struct edptx_drv_s *edp, u8 port, u8 pattern, u8 lane_mask,
		u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2);
	void (*set_MSA)(struct edptx_drv_s *edp, u8 port);

	void (*path_reset)(struct edptx_drv_s *edp, u8 port, u8 mask);

	void (*lane_cfg_to_IP)(struct edptx_drv_s *edp, u8 port);
	void (*phy_cfg_to_IP)(struct edptx_drv_s *edp, u8 port, u8 lane_mask);

	void (*transmitter_init)(struct edptx_drv_s *edp, u8 port);
	void (*transmitter_output)(struct edptx_drv_s *edp, u8 port_mask, u8 en);

	u8 (*get_hpd_level)(struct edptx_drv_s *edp, u8 port);
	/* hotplug out/ in irq state, 1: irq set, 0: irq not set */
	u16 (*get_hpd_irq)(struct edptx_drv_s *edp, u8 port);
	void (*set_hpd_interrupt_mask)(struct edptx_drv_s *edp, u8 port, u8 mask);

	void (*scramble_reset_set)(struct edptx_drv_s *edp, u8 port, u8 sr_type);

	void (*PSR1_SDP_ctrl)(struct edptx_drv_s *edp, u8 port, u8 flag);
	void (*PSR2_SDP_ctrl)(struct edptx_drv_s *edp, u8 port, u8 flag);

	void (*reg_store)(struct edptx_drv_s *edp, u8 port, u32 d0, u32 d1);
	void (*reg_store_get)(struct edptx_drv_s *edp, u8 port, u32 *d0, u32 *d1);
	void (*reg_link_get)(struct edptx_drv_s *edp, u8 port, u32 *link_rate, u8 *lane);
};

struct edptx_if_ctrl_s *edptx_if_bind_t7(struct edptx_drv_s *edp);
struct edptx_if_ctrl_s *edptx_if_bind_tr14(struct edptx_drv_s *edp);
struct edptx_if_ctrl_s *edptx_if_bind_tx_connector_api(struct edptx_drv_s *edp);

#endif
