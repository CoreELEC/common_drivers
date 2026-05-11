// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_core_api_ext.h>

#include "dptx_log.h"
#include "dptx_internal.h"
#include "dptx_hw_opcode.h"
#include "dptx_aux_helper.h"

u8 edptx_get_hpd_level(u8 dp_core_idx)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return 0;

	return dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_GET_HPD_STATE, NULL, NULL);
}
EXPORT_SYMBOL(edptx_get_hpd_level);

u16 edptx_get_hpd_event(u8 dp_core_idx)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return 0;

	return dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_GET_HPD_EVENT_STATE, NULL, NULL);
}
EXPORT_SYMBOL(edptx_get_hpd_event);

void edptx_set_interrupt_mask(u8 dp_core_idx, u8 mask)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	u32 intr_mask = mask;

	if (!tx_comm)
		return;

	dptx_hw_cntl(tx_comm->base.tx_hw_base, DP_SET_IRQ_MASK, &intr_mask, NULL);
}
EXPORT_SYMBOL(edptx_set_interrupt_mask);

void edptx_reg_store_data(u8 dp_core_idx, uint32_t d0, uint32_t d1)
{
	DPTX_INFO("not support reg_store_data API\n");
}
EXPORT_SYMBOL(edptx_reg_store_data);

void edptx_reg_get_data(u8 dp_core_idx, uint32_t *d0, uint32_t *d1)
{
	DPTX_INFO("not support reg_get_data API\n");
}
EXPORT_SYMBOL(edptx_reg_get_data);

void edptx_reg_get_link(u8 dp_core_idx, uint32_t *link, u8 *lane)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return;

	if (link)
		dptx_hw_cntl(tx_comm->base.tx_hw_base, LINKCONF_GET_LINK_RATE, NULL, link);
	if (lane)
		dptx_hw_cntl(tx_comm->base.tx_hw_base, LINKCONF_GET_LANE_COUNT, NULL, lane);
}
EXPORT_SYMBOL(edptx_reg_get_link);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_write(u8 dp_core_idx, u32 addr, int len, u8 *buf)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	int accessed_bytes = 0;

	if (!tx_comm || !buf)
		return 1;

	accessed_bytes = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, buf, len);

	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_write);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_write_single(u8 dp_core_idx, u32 addr, u8 val)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	int accessed_bytes = 0;

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
u8 edptx_aux_read(u8 dp_core_idx, u32 addr, int len, u8 *buf)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	int accessed_bytes = 0;

	if (!tx_comm || !buf)
		return 1;

	accessed_bytes = dptx_aux_read_dpcd(tx_comm->tx_aux, addr, buf, len);
	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_read);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_i2c_op(u8 dp_core_idx, u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	int accessed_bytes = 0;

	if (!tx_comm || !data)
		return 1;

	accessed_bytes = dptx_dpcd_access(tx_comm->tx_aux, cmd_type, dev_addr, data, len);
	/* write successfully with accessed_bytes bytes */
	if (accessed_bytes > 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_i2c_op);

/* return 0 if write successfully, otherwise return non-zero */
u8 edptx_aux_read_edid(u8 dp_core_idx, u8 *buf, u16 len)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	int ret = 0;

	if (!tx_comm || !buf)
		return 1;

	ret = dptx_aux_read_edid_data(tx_comm->tx_aux, buf, len);
	if (ret == 0)
		return 0;
	return 1;
}
EXPORT_SYMBOL(edptx_aux_read_edid);

void edptx_set_msa(u8 dp_core_idx, struct msa_s *msa)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm || !msa)
		return;
	dptx_hw_cntl(tx_comm->base.tx_hw_base, EDP_SET_MSA, msa, NULL);
}
EXPORT_SYMBOL(edptx_set_msa);

void edptx_set_phy_config_to_core(u8 dp_core_idx, u8 use_preset)
{
	DPTX_INFO("should use combo phy for edptx!\n");
}
EXPORT_SYMBOL(edptx_set_phy_config_to_core);

void edptx_set_lane_config_to_core(u8 dp_core_idx, u8 link_rate, u8 lane_count, u8 enh_frame_en)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LINK_RATE,
		&link_rate, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LANE_COUNT,
		&lane_count, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_ENABLE_ENHANCED_FRAME,
		&enh_frame_en, NULL);
}
EXPORT_SYMBOL(edptx_set_lane_config_to_core);

void edptx_psr1_ctrl_set(u8 dp_core_idx, u8 flag)
{
	DPTX_INFO("PSR1 function TODO\n");
}
EXPORT_SYMBOL(edptx_psr1_ctrl_set);

void edptx_psr2_ctrl_set(u8 dp_core_idx, u8 flag)
{
	DPTX_INFO("PSR2 function TODO\n");
}
EXPORT_SYMBOL(edptx_psr2_ctrl_set);

void edptx_set_scramble_reset(u8 dp_core_idx, u8 sr_type)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	u32 value = 0;

	if (!tx_comm)
		return;
	value = sr_type == DPTX_SCRAMBLE_RESET_OFF ? 0x01 : 0x00;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &value, NULL);

	value = sr_type == DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET ? 0x01 : 0x00;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, EDP_ALTERNATE_SCRAMBLE_RESET, &value, NULL);
}
EXPORT_SYMBOL(edptx_set_scramble_reset);

void edptx_transmitter_init(u8 dp_core_idx)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, EDP_TRANSMITTER_INIT, NULL, NULL);
}
EXPORT_SYMBOL(edptx_transmitter_init);

void edptx_transmit_pattern(u8 dp_core_idx, u8 pattern,
		u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	unsigned char dptx_ip_pat_sets[13] = {
		/* TRAINING_PATTERN_SET: off, TPS1, TPS2, TPS3, TPS4 */
		0x00, 0x01, 0x02, 0x03, 0x04,
		/* LINK_QUAL_PATTERN_SET: off, D10.2, SERm, PRBS7, 80b_cos,
		 * CP2520-1, CP2520-2, CP2520-3
		 */
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x6, 0x7};
	u32 cos_80b[3] = {0};
	unsigned int reg_val;
	u32 sr_disable = 0;
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);

	if (!tx_comm)
		return;

	if (pattern == eDPTX_TPS_DISABLE ||
		pattern == eDPTX_QUAL_PAT_DISABLE ||
		pattern == eDPTX_SYMBOL_ERROR_MSR ||
		pattern == eDPTX_HBR2_EYE)
		sr_disable = 0;
	else if (pattern == eDPTX_TPS1 ||
		pattern == eDPTX_TPS2 ||
		pattern == eDPTX_TPS3 ||
		pattern == eDPTX_TPS4 ||
		pattern == eDPTX_D10P2 ||
		pattern == eDPTX_PRBS7 ||
		pattern == eDPTX_80BIT_CUSTOM ||
		pattern == eDPTX_CP2520_2 ||
		pattern == eDPTX_CP2520_3)
		sr_disable = 1;
	else
		sr_disable = 0;

	switch (pattern) {
	case eDPTX_TPS_DISABLE:
	case eDPTX_TPS1:
	case eDPTX_TPS2:
	case eDPTX_TPS3:
	case eDPTX_TPS4:
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_SCRAMBLE_DISABLE, &sr_disable, NULL);
		reg_val = 0;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_QUAL_PATTERN, &reg_val, NULL);
		reg_val = dptx_ip_pat_sets[pattern];
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN_VALUE,
			&reg_val, NULL);
		break;
	case eDPTX_QUAL_PAT_DISABLE:
	case eDPTX_D10P2:
	case eDPTX_SYMBOL_ERROR_MSR:
	case eDPTX_PRBS7:
	case eDPTX_HBR2_EYE:
	case eDPTX_CP2520_2:
	case eDPTX_CP2520_3:
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
	case eDPTX_80BIT_CUSTOM:
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

void edptx_transmitter_output_set(u8 dp_core_idx, u8 en)
{
	struct dptx_common *tx_comm = dptx_get_edptx_inst(dp_core_idx);
	if (!tx_comm)
		return;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
}
EXPORT_SYMBOL(edptx_transmitter_output_set);

