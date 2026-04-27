// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/edptx_core_api_ext.h>
#include "../dptx_reg_op.h"
#include "./dptx_IP_ctrls.h"
#include "../dptx_common.h"

struct dptx_if_ctrl_s dptx_if_dp14 = {
#ifdef CONFIG_AMLOGIC_DPTX
	.aux_write = edptx_aux_write,
	.aux_write_single = edptx_aux_write_single,
	.aux_read = edptx_aux_read,
	.aux_i2c_op = edptx_aux_i2c_op,
	.aux_read_edid = edptx_aux_read_edid,

	.transmit_pattern = edptx_transmit_pattern,
	.set_MSA = edptx_set_msa,

	.path_reset = NULL,

	.lane_cfg_to_IP = edptx_set_lane_config_to_core,
	.phy_cfg_to_IP = edptx_set_phy_config_to_core,

	.transmitter_init = edptx_transmitter_init,
	.transmitter_output = edptx_transmitter_output_set,

	.get_hpd_level = edptx_get_hpd_level,
	.get_hpd_irq = edptx_get_hpd_event,
	.set_hpd_interrupt_mask = edptx_set_interrupt_mask,

	.scramble_reset_set = edptx_set_scramble_reset,

	.PSR1_SDP_ctrl = edptx_psr1_ctrl_set,
	.PSR2_SDP_ctrl = edptx_psr2_ctrl_set,

	.reg_store = edptx_reg_store_data,
	.reg_store_get = edptx_reg_get_data,
	.reg_link_get = edptx_reg_get_link,
#else
	.aux_write = NULL,
	.aux_write_single = NULL,
	.aux_read = NULL,
	.aux_i2c_op = NULL,

	.transmit_pattern = NULL,
	.set_MSA = NULL,

	.path_reset = NULL,

	.lane_cfg_to_IP = NULL,
	.phy_cfg_to_IP = NULL,

	.transmitter_init = NULL,
	.transmitter_output = NULL,

	.get_hpd_level = NULL,
	.get_hpd_irq = NULL,
	.set_hpd_interrupt_mask = NULL,

	.scramble_reset_set = NULL,
#endif
};

struct dptx_if_ctrl_s *dptx_if_bind_tx_connector_api(struct dptx_drv_s *dptx)
{
	return &dptx_if_dp14;
}
