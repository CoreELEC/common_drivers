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
#include <linux/amlogic/media/vout/eDPTX/DPTX.h>
#include "../dptx_common.h"
#include "./dptx_IP_ctrls.h"

u8 dptx_if_aux_write(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_write))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_write
			(dptx, port, addr, len, buf);
	return 0;
}

u8 dptx_if_aux_write_single(struct dptx_drv_s *dptx, u8 port, u32 addr, u8 val)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_write_single))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_write_single
			(dptx, port, addr, val);
	return 0;
}

u8 dptx_if_aux_read(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_read))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_read
			(dptx, port, addr, len, buf);
	return 0;
}

u8 dptx_if_aux_i2c_op(struct dptx_drv_s *dptx, u8 port,
			u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_i2c_op))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_i2c_op
			(dptx, port, cmd_type, dev_addr, len, data);
	return 0;
}

u8 dptx_if_aux_read_edid(struct dptx_drv_s *dptx, u8 port,
			u8 *data, u16 len)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_read_edid))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->aux_read_edid
			(dptx, port, data, len);
	return 0;
}

void dptx_if_transmit_pattern(struct dptx_drv_s *dptx, u8 port, u8 pattern, u8 lane,
	u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmit_pattern))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmit_pattern
			(dptx, port, pattern, lane, cos_80b_0, cos_80b_1, cos_80b_2);
}

void dptx_if_set_MSA(struct dptx_drv_s *dptx, u8 port)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->set_MSA))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->set_MSA(dptx, port);
}

void dptx_if_path_reset(struct dptx_drv_s *dptx, u8 port, u8 mask)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->path_reset))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->path_reset(dptx, port, mask);
}

void dptx_if_set_lane_cfg(struct dptx_drv_s *dptx, u8 port)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->lane_cfg_to_IP))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->lane_cfg_to_IP(dptx, port);
}

void dptx_if_set_phy_cfg(struct dptx_drv_s *dptx, u8 port, u8 lane_mask)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->phy_cfg_to_IP))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->phy_cfg_to_IP(dptx, port, lane_mask);
}

void dptx_if_transmitter_init(struct dptx_drv_s *dptx, u8 port)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmitter_init))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmitter_init(dptx, port);
}

void dptx_if_transmitter_output(struct dptx_drv_s *dptx, u8 port, u8 en)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmitter_output))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->transmitter_output(dptx, port, en);
}

u8 dptx_if_get_hpd_level(struct dptx_drv_s *dptx, u8 port)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->get_hpd_level))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->get_hpd_level(dptx, port);
	return 0;
}

u16 dptx_if_get_hpd_irq(struct dptx_drv_s *dptx, u8 port)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->get_hpd_irq))
		return ((struct dptx_if_ctrl_s *)dptx->if_ctrls)->get_hpd_irq(dptx, port);
	return 0;
}

void dptx_if_set_hpd_interrupt_mask(struct dptx_drv_s *dptx, u8 port, u8 mask)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->set_hpd_interrupt_mask))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->set_hpd_interrupt_mask(dptx, port, mask);
}

void dptx_if_scramble_reset_set(struct dptx_drv_s *dptx, u8 port, u8 sr_type)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->scramble_reset_set))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->scramble_reset_set(dptx, port, sr_type);
}

void dptx_if_PSR1_ctrl(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->PSR1_SDP_ctrl))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->PSR1_SDP_ctrl(dptx, port, flag);
}

void dptx_if_PSR2_ctrl(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->PSR2_SDP_ctrl))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->PSR2_SDP_ctrl(dptx, port, flag);
}

void dptx_if_reg_store(struct dptx_drv_s *dptx, uint8_t port, u32 d0, u32 d1)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_store))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_store(dptx, port, d0, d1);
}

void dptx_if_reg_store_get(struct dptx_drv_s *dptx, uint8_t port, u32 *d0, u32 *d1)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_store_get))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_store_get(dptx, port, d0, d1);
}

void dptx_if_link_get(struct dptx_drv_s *dptx, uint8_t port, uint32_t *link_rate, uint8_t *lane)
{
	if (likely(((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_link_get))
		((struct dptx_if_ctrl_s *)dptx->if_ctrls)->reg_link_get(dptx,
								port, link_rate, lane);
}

void dptx_if_IP_probe(struct dptx_drv_s *dptx)
{
	switch (dptx->data->chip_type) {
	case DPTX_CHIP_T7:
		dptx->if_ctrls = dptx_if_bind_t7(dptx);
		break;
	case DPTX_CHIP_A9:
		dptx->if_ctrls = dptx_if_bind_DP14(dptx);
		break;
	default:
		dptx->if_ctrls = NULL;
		DPTX_ERR(dptx, "%s: invalid chip type", __func__);
		return;
	}
}
