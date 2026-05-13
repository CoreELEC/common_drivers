// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "../edptx_common.h"
#include "./edptx_IP_ctrls.h"

u8 edptx_if_aux_write(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_write))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_write
			(edp, port, addr, len, buf);
	return 0;
}

u8 edptx_if_aux_write_single(struct edptx_drv_s *edp, u8 port, u32 addr, u8 val)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_write_single))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_write_single
			(edp, port, addr, val);
	return 0;
}

u8 edptx_if_aux_read(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_read))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_read
			(edp, port, addr, len, buf);
	return 0;
}

u8 edptx_if_aux_i2c_op(struct edptx_drv_s *edp, u8 port,
			u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_i2c_op))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_i2c_op
			(edp, port, cmd_type, dev_addr, len, data);
	return 0;
}

u8 edptx_if_aux_read_edid(struct edptx_drv_s *edp, u8 port,
			u8 *data, u16 len)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_read_edid))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->aux_read_edid
			(edp, port, data, len);
	return 0;
}

void edptx_if_transmit_pattern(struct edptx_drv_s *edp, u8 port, u8 pattern, u8 lane,
	u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmit_pattern))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmit_pattern
			(edp, port, pattern, lane, cos_80b_0, cos_80b_1, cos_80b_2);
}

void edptx_if_set_MSA(struct edptx_drv_s *edp, u8 port)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->set_MSA))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->set_MSA(edp, port);
}

void edptx_if_path_reset(struct edptx_drv_s *edp, u8 port, u8 mask)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->path_reset))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->path_reset(edp, port, mask);
}

void edptx_if_set_lane_cfg(struct edptx_drv_s *edp, u8 port)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->lane_cfg_to_IP))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->lane_cfg_to_IP(edp, port);
}

void edptx_if_set_phy_cfg(struct edptx_drv_s *edp, u8 port, u8 lane_mask)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->phy_cfg_to_IP))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->phy_cfg_to_IP(edp, port, lane_mask);
}

void edptx_if_transmitter_init(struct edptx_drv_s *edp, u8 port)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmitter_init))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmitter_init(edp, port);
}

void edptx_if_transmitter_output(struct edptx_drv_s *edp, u8 port, u8 en)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmitter_output))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->transmitter_output(edp, port, en);
}

u8 edptx_if_get_hpd_level(struct edptx_drv_s *edp, u8 port)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->get_hpd_level))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->get_hpd_level(edp, port);
	return 0;
}

u16 edptx_if_get_hpd_irq(struct edptx_drv_s *edp, u8 port)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->get_hpd_irq))
		return ((struct edptx_if_ctrl_s *)edp->if_ctrls)->get_hpd_irq(edp, port);
	return 0;
}

void edptx_if_set_hpd_interrupt_mask(struct edptx_drv_s *edp, u8 port, u8 mask)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->set_hpd_interrupt_mask))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->set_hpd_interrupt_mask(edp, port, mask);
}

void edptx_if_scramble_reset_set(struct edptx_drv_s *edp, u8 port, u8 sr_type)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->scramble_reset_set))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->scramble_reset_set(edp, port, sr_type);
}

void edptx_if_PSR1_ctrl(struct edptx_drv_s *edp, u8 port, u8 flag)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->PSR1_SDP_ctrl))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->PSR1_SDP_ctrl(edp, port, flag);
}

void edptx_if_PSR2_ctrl(struct edptx_drv_s *edp, u8 port, u8 flag)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->PSR2_SDP_ctrl))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->PSR2_SDP_ctrl(edp, port, flag);
}

void edptx_if_reg_store(struct edptx_drv_s *edp, u8 port, u32 d0, u32 d1)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_store))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_store(edp, port, d0, d1);
}

void edptx_if_reg_store_get(struct edptx_drv_s *edp, u8 port, u32 *d0, u32 *d1)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_store_get))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_store_get(edp, port, d0, d1);
}

void edptx_if_link_get(struct edptx_drv_s *edp, u8 port, u32 *link_rate, u8 *lane)
{
	if (likely(((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_link_get))
		((struct edptx_if_ctrl_s *)edp->if_ctrls)->reg_link_get(edp,
								port, link_rate, lane);
}

void edptx_if_IP_probe(struct edptx_drv_s *edp)
{
	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		edp->if_ctrls = edptx_if_bind_t7(edp);
		break;
	case eDPTX_CHIP_A9:
		if (edp->board_data.use_ext_tx_conn_api)
			edp->if_ctrls = edptx_if_bind_tx_connector_api(edp);
		else
			edp->if_ctrls = edptx_if_bind_tr14(edp);
		break;
	default:
		edp->if_ctrls = NULL;
		eDP_ERR(edp, "%s: invalid chip type", __func__);
		return;
	}
}
