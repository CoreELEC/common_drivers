// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_core_api_ext.h>
#include "../dptx_reg_op.h"
#include "./dptx_IP_ctrls.h"
#include "../dptx_common.h"

#ifdef CONFIG_AMLOGIC_DPTX
static u8 _aux_write(struct dptx_drv_s *dptx, u8 port,
				u32 addr, int len, u8 *buf)
{
	if (!buf)
		return 1;

	return edptx_aux_write(dptx->idx, addr, len, buf);
}

static u8 _aux_write_single(struct dptx_drv_s *dptx, u8 port, u32 addr, u8 val)
{
	return edptx_aux_write_single(dptx->idx, addr, val);
}

static u8 _aux_read(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf)
{
	if (!buf)
		return 1;

	return edptx_aux_read(dptx->idx, addr, len, buf);
}

static u8 _aux_i2c_op(struct dptx_drv_s *dptx, u8 port,
			u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	return edptx_aux_i2c_op(dptx->idx, cmd_type, dev_addr, len, data);
}

static u8 _aux_read_edid(struct dptx_drv_s *dptx, u8 port, u8 *data, u16 len)
{
	return edptx_aux_read_edid(dptx->idx, data, len);
}

static void dptx_transmit_pattern(struct dptx_drv_s *dptx, u8 port, u8 pattern,
		u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	edptx_transmit_pattern(dptx->idx, pattern,
		lane_mask, cos_80b_0, cos_80b_1, cos_80b_2);
}

static void dptx_set_msa(struct dptx_drv_s *dptx, u8 port)
{
	unsigned int hactive, vactive, htotal, vtotal, hsw, hbp, vsw, vbp;
	unsigned int bpp, bit_depth, sync_mode;
	/* unsigned int symbol_count, user_data_count, tu_cfg; */
	unsigned int m_vid; /*pclk/1000 */
	unsigned int n_vid; /*162000, 270000, 540000 */
	unsigned int cfmt;
	u8 tu_size;
	u8 port_cnt = dptx->sink.port_mask == 0x3 ? 2 : 1;
	struct msa_s *msa_data = &dptx->msa;

	hactive = dptx->act_timing.h_act    / port_cnt;
	htotal  = dptx->act_timing.h_period / port_cnt;
	hsw     = dptx->act_timing.h_pw     / port_cnt;
	hbp     = dptx->act_timing.h_bp     / port_cnt;
	vactive = dptx->act_timing.v_act;
	vtotal  = dptx->act_timing.v_period;
	vsw     = dptx->act_timing.h_pw;
	vbp     = dptx->act_timing.v_bp;

	m_vid = dptx_div_around(dptx->act_timing.pclk / port_cnt, 1000);
	if (dptx->sink.link[port].link_rate == DP_LINK_RATE_HBR2)
		n_vid = 540000;
	else if (dptx->sink.link[port].link_rate == DP_LINK_RATE_HBR)
		n_vid = 270000;
	else
		n_vid = 162000;

	switch (dptx->act_timing.cfmt) {
	case DPTX_CFMT_RGB_8bit:
		bit_depth = 8; cfmt = 0; bpp = 24; break;
	case DPTX_CFMT_RGB_10bit:
		bit_depth = 10; cfmt = 0; bpp = 30; break;
	case DPTX_CFMT_RGB_12bit:
		bit_depth = 12; cfmt = 0; bpp = 36; break;
	case DPTX_CFMT_YCbCr422_8bit:
		bit_depth = 8; cfmt = 1; bpp = 16; break;
	case DPTX_CFMT_YCbCr422_10bit:
		bit_depth = 10; cfmt = 1; bpp = 20; break;
	case DPTX_CFMT_YCbCr422_12bit:
		bit_depth = 12; cfmt = 1; bpp = 24; break;
	case DPTX_CFMT_YCbCr444_8bit:
		bit_depth = 8; cfmt = 2; bpp = 24; break;
	case DPTX_CFMT_YCbCr444_10bit:
		bit_depth = 10; cfmt = 2; bpp = 30; break;
	case DPTX_CFMT_YCbCr444_12bit:
		bit_depth = 12; cfmt = 2; bpp = 36; break;
	case DPTX_CFMT_RGB_6bit:
		bit_depth = 6; cfmt = 0; bpp = 18; break;
	case DPTX_CFMT_invalid:
	default:
		bit_depth = 0x0; cfmt = 0; bpp = 18; break;
	}

	// sync_mode = dptx->sink.link[port].cap.sync_clk_mode;
	sync_mode = 0;

	// m_vid is ppp in kHz // by experience
	if (m_vid > 800000) // > 800M
		tu_size = 32;
	else if (m_vid > 200000) // > 200M
		tu_size = 48;
	else
		tu_size = 64;

	/* symbol_count = ((hactive * bpp) + 7) / 8; */
	/* user_data_count = (symbol_count + dptx->sink.link[port].lane_count - 1) / */
		/* dptx->sink.link[port].lane_count; */

	memset(msa_data, 0, sizeof(struct msa_s));
	msa_data->m_vid = m_vid;
	msa_data->n_vid = n_vid;
	msa_data->h_total = htotal;
	msa_data->v_total = vtotal;
	msa_data->hsync_pol = 1;
	msa_data->vsync_pol = 1;
	msa_data->hsw = hsw;
	msa_data->vsw = vsw;
	msa_data->h_active = hactive;
	msa_data->v_active = vactive;
	msa_data->hbp = hbp;
	msa_data->vbp = vbp;
	msa_data->sync_clk_mode = sync_mode;
	msa_data->pixel_freq = dptx->act_timing.pclk;
	msa_data->link_rate = dptx->sink.link[port].link_rate;
	msa_data->lane_count = dptx->sink.link[port].lane_count;
	msa_data->interlace_mode = 0;
	msa_data->stereo_video_attr = 0;
	msa_data->color_format = cfmt;
	/* VESA range */
	msa_data->dynamic_range = 0;
	/* BT601 */
	msa_data->ycbcr_colorimetry = 0;
	msa_data->bit_depth = bit_depth;
	msa_data->tu_size = tu_size;
	/* msa_data->user_data_count = user_data_count; */
	/* msa_data.user_sync_polarity = 0x7; */
	/* msa_data.user_control = 1; */
	/* msa_data.user_data_count = user_data_count; */
	edptx_set_msa(dptx->idx, msa_data);
}

static void dptx_reset(struct dptx_drv_s *dptx, u8 port, u8 mask)
{
}

static void dptx_set_lane_config_to_IP(struct dptx_drv_s *dptx, u8 port)
{
	edptx_set_lane_config_to_core(dptx->idx, dptx->sink.link[port].link_rate,
		dptx->sink.link[port].lane_count, dptx->sink.link[port].enh_frame_en);
}

static void dptx_set_phy_config_to_IP(struct dptx_drv_s *dptx, u8 port, u8 use_preset)
{
}

static void dptx_transmitter_init(struct dptx_drv_s *dptx, u8 port)
{
	edptx_transmitter_init(dptx->idx);
}

static void dptx_transmitter_output_set(struct dptx_drv_s *dptx, u8 port, u8 en)
{
	edptx_transmitter_output_set(dptx->idx, en);
}

static u8 dptx_get_hpd_level(struct dptx_drv_s *dptx, u8 port)
{
	return edptx_get_hpd_level(dptx->idx);
}

static u16 dptx_get_hpd_irq(struct dptx_drv_s *dptx, u8 port)
{
	return edptx_get_hpd_event(dptx->idx);
}

static void dptx_interrupt_mask_set(struct dptx_drv_s *dptx, u8 port, u8 mask)
{
	edptx_set_interrupt_mask(dptx->idx, mask);
}

static void dptx_set_scramble_reset(struct dptx_drv_s *dptx, u8 port, u8 sr_type)
{
	edptx_set_scramble_reset(dptx->idx, sr_type);
}

static void dptx_PSR1_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	edptx_psr1_ctrl_set(dptx->idx, flag);
}

static void dptx_PSR2_ctrl_set(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	edptx_psr2_ctrl_set(dptx->idx, flag);
}

static void dptx_reg_get_link(struct dptx_drv_s *dptx, u8 port, u32 *link, u8 *lane)
{
	edptx_reg_get_link(dptx->idx, link, lane);
}
#endif

struct dptx_if_ctrl_s dptx_if_dp14 = {
#ifdef CONFIG_AMLOGIC_DPTX
	.aux_write = _aux_write,
	.aux_write_single = _aux_write_single,
	.aux_read = _aux_read,
	.aux_i2c_op = _aux_i2c_op,
	.aux_read_edid = _aux_read_edid,

	.transmit_pattern = dptx_transmit_pattern,
	.set_MSA = dptx_set_msa,

	.path_reset = dptx_reset,

	.lane_cfg_to_IP = dptx_set_lane_config_to_IP,
	.phy_cfg_to_IP = dptx_set_phy_config_to_IP,

	.transmitter_init = dptx_transmitter_init,
	.transmitter_output = dptx_transmitter_output_set,

	.get_hpd_level = dptx_get_hpd_level,
	.get_hpd_irq = dptx_get_hpd_irq,
	.set_hpd_interrupt_mask = dptx_interrupt_mask_set,

	.scramble_reset_set = dptx_set_scramble_reset,

	.PSR1_SDP_ctrl = dptx_PSR1_ctrl_set,
	.PSR2_SDP_ctrl = dptx_PSR2_ctrl_set,

	.reg_store = NULL,
	.reg_store_get = NULL,
	.reg_link_get = dptx_reg_get_link,
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
