/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_CORE_API_EXT_H
#define __DPTX_CORE_API_EXT_H

struct dptx_drv_s;

u8 edptx_get_hpd_level(struct dptx_drv_s *dptx, u8 port);
u16 edptx_get_hpd_event(struct dptx_drv_s *dptx, u8 port);
void edptx_set_interrupt_mask(struct dptx_drv_s *dptx, u8 port, u8 mask);

void edptx_reg_store_data(struct dptx_drv_s *dptx, u8 port, uint32_t d0, uint32_t d1);
void edptx_reg_get_data(struct dptx_drv_s *dptx, u8 port, uint32_t *d0, uint32_t *d1);
void edptx_reg_get_link(struct dptx_drv_s *dptx, u8 port, uint32_t *link, u8 *lane);

u8 edptx_aux_write(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf);
u8 edptx_aux_write_single(struct dptx_drv_s *dptx, u8 port, u32 addr, u8 val);
u8 edptx_aux_read(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf);
u8 edptx_aux_i2c_op(struct dptx_drv_s *dptx, u8 port, u8 cmd_type, u32 dev_addr, u8 len, u8 *data);
u8 edptx_aux_read_edid(struct dptx_drv_s *dptx, u8 port, u8 *buf, u16 len);
void edptx_set_msa(struct dptx_drv_s *dptx, u8 port);
void edptx_set_phy_config_to_core(struct dptx_drv_s *dptx, u8 port, u8 use_preset);
void edptx_set_lane_config_to_core(struct dptx_drv_s *dptx, u8 port);
void edptx_psr1_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag);
void edptx_psr2_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag);
void edptx_set_scramble_reset(struct dptx_drv_s *dptx, u8 port, u8 sr_type);
void edptx_transmitter_init(struct dptx_drv_s *dptx, u8 port);
void edptx_transmitter_output_set(struct dptx_drv_s *dptx, u8 port, u8 en);
void edptx_transmit_pattern(struct dptx_drv_s *dptx, u8 port, u8 pattern,
	u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2);

#endif
