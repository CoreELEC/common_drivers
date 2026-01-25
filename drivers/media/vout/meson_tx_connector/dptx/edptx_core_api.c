// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX.h>

#include "dptx_log.h"
#include "dptx_internal.h"
#include "dptx_hw_opcode.h"
#include "dptx_aux_helper.h"

u8 edptx_get_hpd_level(struct dptx_drv_s *dptx, u8 port)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return 0;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 0;

	return dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_GET_HPD_STATE, NULL, NULL);
}
EXPORT_SYMBOL(edptx_get_hpd_level);

u16 edptx_get_hpd_event(struct dptx_drv_s *dptx, u8 port)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return 0;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 0;

	return dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_GET_HPD_EVENT_STATE, NULL, NULL);
}
EXPORT_SYMBOL(edptx_get_hpd_event);

void edptx_set_interrupt_mask(struct dptx_drv_s *dptx, u8 port, u8 mask)
{
	struct dptx_common *tx_comm = NULL;
	u32 intr_mask = mask;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_SET_IRQ_MASK, &intr_mask, NULL);
}
EXPORT_SYMBOL(edptx_set_interrupt_mask);

void edptx_reg_store_data(struct dptx_drv_s *dptx, u8 port, uint32_t d0, uint32_t d1)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(tx_comm->base.tx_hw_base, EDP_REG_STORE_DATA, &d0, NULL);
}
EXPORT_SYMBOL(edptx_reg_store_data);

void edptx_reg_get_data(struct dptx_drv_s *dptx, u8 port, uint32_t *d0, uint32_t *d1)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	if (d0)
		dptx_hw_cntl(tx_comm->base.tx_hw_base, EDP_REG_GET_DATA, NULL, d0);
	if (d1)
		*d1 = 0;
}
EXPORT_SYMBOL(edptx_reg_get_data);

void edptx_reg_get_link(struct dptx_drv_s *dptx, u8 port, uint32_t *link, u8 *lane)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	if (link)
		dptx_hw_cntl(tx_comm->base.tx_hw_base, LINKCONF_GET_LINK_RATE, NULL, link);
	if (lane)
		dptx_hw_cntl(tx_comm->base.tx_hw_base, LINKCONF_GET_LANE_COUNT, NULL, lane);
}
EXPORT_SYMBOL(edptx_reg_get_link);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_write(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf)
{
	struct dptx_common *tx_comm = NULL;
	int accessed_bytes = 0;

	if (!dptx || !buf)
		return 1;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 1;

	accessed_bytes = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, buf, len);

	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_write);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_write_single(struct dptx_drv_s *dptx, u8 port, u32 addr, u8 val)
{
	struct dptx_common *tx_comm = NULL;
	int accessed_bytes = 0;

	if (!dptx)
		return 1;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 1;

	accessed_bytes = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, &val, 1);

	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_write_single);

/* return 0 if read successfully, otherwise return non-zero */
u8 edptx_aux_read(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf)
{
	struct dptx_common *tx_comm = NULL;
	int accessed_bytes = 0;

	if (!dptx || !buf)
		return 1;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 1;

	accessed_bytes = dptx_aux_read_dpcd(tx_comm->tx_aux, addr, buf, len);
	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_read);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_i2c_op(struct dptx_drv_s *dptx, u8 port, u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	struct dptx_common *tx_comm = NULL;
	int accessed_bytes = 0;

	if (!dptx || !data)
		return 1;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 1;

	accessed_bytes = dptx_dpcd_access(tx_comm->tx_aux, cmd_type, dev_addr, data, len);
	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_i2c_op);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_read_edid(struct dptx_drv_s *dptx, u8 port, u8 *buf, u16 len)
{
	struct dptx_common *tx_comm = NULL;
	int ret = 0;

	if (!dptx || !buf)
		return 1;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return 1;

	ret = dptx_aux_read_edid_data(tx_comm->tx_aux, buf, len);
	if (ret == 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_read_edid);

void edptx_set_msa(struct dptx_drv_s *dptx, u8 port)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;
	dptx_hw_cntl(tx_comm->base.tx_hw_base, EDP_SET_MSA, &dptx->msa, NULL);
}
EXPORT_SYMBOL(edptx_set_msa);

void edptx_set_phy_config_to_core(struct dptx_drv_s *dptx, u8 port, u8 use_preset)
{
	DPTX_INFO("should use combo phy for edptx!\n");
}
EXPORT_SYMBOL(edptx_set_phy_config_to_core);

void edptx_set_lane_config_to_core(struct dptx_drv_s *dptx, u8 port)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LINK_RATE,
		&dptx->sink.link[port]->link_rate, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LANE_COUNT,
		&dptx->sink.link[port]->lane_count, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_ENABLE_ENHANCED_FRAME,
		&dptx->sink.link[port]->enh_frame_en, NULL);
}
EXPORT_SYMBOL(edptx_set_lane_config_to_core);

void edptx_psr1_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	DPTX_INFO("PSR1 function TODO\n");
}
EXPORT_SYMBOL(edptx_psr1_ctrl_set);

void edptx_psr2_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	DPTX_INFO("PSR2 function TODO\n");
}
EXPORT_SYMBOL(edptx_psr2_ctrl_set);

void edptx_set_scramble_reset(struct dptx_drv_s *dptx, u8 port, u8 sr_type)
{
	struct dptx_common *tx_comm = NULL;
	u32 value = 0;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;
	value = sr_type == 0 ? 0x01 : 0x00;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &value, NULL);

	value = sr_type == 2 ? 0x01 : 0x00;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, EDP_ALTERNATE_SCRAMBLE_RESET, &value, NULL);
}
EXPORT_SYMBOL(edptx_set_scramble_reset);

void edptx_transmitter_init(struct dptx_drv_s *dptx, u8 port)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, EDP_TRANSMITTER_INIT, NULL, NULL);
}
EXPORT_SYMBOL(edptx_transmitter_init);

void edptx_transmit_pattern(struct dptx_drv_s *dptx, u8 port, u8 pattern,
		u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	unsigned char dptx_ip_pat_sets[11] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
	u32 cos_80b[3] = {0};
	unsigned int reg_val;
	u32 sr_disable = 0;
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	if (pattern == DPTX_TPS_DISABLE ||
		pattern == DPTX_QUAL_PAT_DISABLE ||
		pattern == DPTX_SYMBOL_ERROR_MSR ||
		pattern == DPTX_HBR2_EYE)
		sr_disable = 0;
	else if (pattern == DPTX_TPS1 ||
		pattern == DPTX_TPS2 ||
		pattern == DPTX_TPS3 ||
		pattern == DPTX_TPS4 ||
		pattern == DPTX_D10P2 ||
		pattern == DPTX_PRBS7 ||
		pattern == DPTX_80BIT_CUSTOM ||
		pattern == DPTX_CP2520_2 ||
		pattern == DPTX_CP2520_3)
		sr_disable = 1;
	else
		sr_disable = 0;

	switch (pattern) {
	case DPTX_TPS_DISABLE:
	case DPTX_TPS1:
	case DPTX_TPS2:
	case DPTX_TPS3:
	case DPTX_TPS4:
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &sr_disable, NULL);
		reg_val = 0;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_QUAL_PATTERN, &reg_val, NULL);
		reg_val = dptx_ip_pat_sets[pattern];
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN_VALUE,
			&reg_val, NULL);
		break;
	case DPTX_QUAL_PAT_DISABLE:
	case DPTX_D10P2:
	case DPTX_SYMBOL_ERROR_MSR:
	case DPTX_PRBS7:
	case DPTX_HBR2_EYE:
	case DPTX_CP2520_2:
	case DPTX_CP2520_3:
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &sr_disable, NULL);
		reg_val = 0;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN_VALUE,
			&reg_val, NULL);
		reg_val = (lane_mask & BIT(0) ? dptx_ip_pat_sets[pattern] << 0  : 0) |
			  (lane_mask & BIT(1) ? dptx_ip_pat_sets[pattern] << 8  : 0) |
			  (lane_mask & BIT(2) ? dptx_ip_pat_sets[pattern] << 16 : 0) |
			  (lane_mask & BIT(3) ? dptx_ip_pat_sets[pattern] << 24 : 0);
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_QUAL_PATTERN,
			&reg_val, NULL);
		break;
	case DPTX_80BIT_CUSTOM:
		cos_80b[0] = cos_80b_0;
		cos_80b[1] = cos_80b_1;
		cos_80b[2] = cos_80b_2;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_80BIT_CUSTOM, &cos_80b, NULL);
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &sr_disable, NULL);
		reg_val = 0;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN_VALUE,
			&reg_val, NULL);
		reg_val = (lane_mask & BIT(0) ? dptx_ip_pat_sets[pattern] << 0  : 0) |
			  (lane_mask & BIT(1) ? dptx_ip_pat_sets[pattern] << 8  : 0) |
			  (lane_mask & BIT(2) ? dptx_ip_pat_sets[pattern] << 16 : 0) |
			  (lane_mask & BIT(3) ? dptx_ip_pat_sets[pattern] << 24 : 0);
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_QUAL_PATTERN,
			&reg_val, NULL);
		break;
	default:
		DPTX_ERROR("%s: %d invalid", __func__, pattern);
		break;
	}
}
EXPORT_SYMBOL(edptx_transmit_pattern);

void edptx_transmitter_output_set(struct dptx_drv_s *dptx, u8 port, u8 en)
{
	struct dptx_common *tx_comm = NULL;

	if (!dptx)
		return;

	tx_comm = dptx_get_edptx_inst(dptx->idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
}
EXPORT_SYMBOL(edptx_transmitter_output_set);

