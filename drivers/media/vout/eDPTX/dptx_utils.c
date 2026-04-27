// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "dptx_reg_op.h"
#include "dptx_common.h"
#include <linux/delay.h>

u16 dptx_train_rd_intv[5] = {400, 4000, 8000, 12000, 16000};
u16 dptx_PSR_setup_time[8] = {330, 275, 220, 165, 110, 55, 0, 0};

char *eDP_ver_str[6] = {"1.1", "1.2", "1.3", "1.4a", "1.4b", "1.5"};
static char *eDP_PSR_str[5] = {
	"0", "PSR1", "PSR1+PSR2(SU)", "PSR1+PSR2(SU+Y-coor)", "PSR1+PSR2(SU+Y_coor+Early_trans)"};

u16 std_level_to_phy_dft_lut[10][3] = {
/*Vswing|Pre-emphasis: 0,               1,               2,               3 */
/* 0 */  {0x3, 0x3, 0x0}, {0x3, 0x7, 0x0}, {0x3, 0xb, 0x0}, {0x3, 0xf, 0x0},
/* 1 */  {0x7, 0x3, 0x0}, {0x7, 0x8, 0x0}, {0x8, 0xc, 0x0},
/* 2 */  {0xa, 0x4, 0x0}, {0xa, 0x8, 0x0},
/* 3 */  {0xf, 0x5, 0x0}
};

void dptx_delay_us(int us)
{
	if (!us)
		return;
	else if (us < 20)
		udelay(us);
	else if (us < 1000)
		usleep_range(us, us + 10);
	else if (us < 20000)
		usleep_range(us, us + 100);
	else
		msleep(us / 1000);
}

void dptx_delay_ms(int ms)
{
	if (!ms)
		return;
	else if (ms < 20)
		usleep_range(ms * 1000, ms * 1000 + 100);
	else
		msleep(ms);
}

//DPCD value (2bit) to ANACTRL phy value(0~0xf)
u8 dptx_vswing_ds_to_phy(struct dptx_drv_s *dptx, u8 ds_level)
{
	if (ds_level > 9) {
		DPTX_ERR(dptx, "%s level %u out of protocal limit", __func__, ds_level);
		return 0;
	}

	if (dptx->board_data.driver_strength_lut[0][0])
		return dptx->board_data.driver_strength_lut[ds_level][0];
	else
		return std_level_to_phy_dft_lut[ds_level][0];
}

u8 dptx_preem_ds_to_phy(struct dptx_drv_s *dptx, u8 ds_level)
{
	if (ds_level > 9) {
		DPTX_ERR(dptx, "%s level %u out of protocal limit", __func__, ds_level);
		return 0;
	}

	if (dptx->board_data.driver_strength_lut[0][0])
		return dptx->board_data.driver_strength_lut[ds_level][1];
	else
		return std_level_to_phy_dft_lut[ds_level][1];
}

u8 ds_to_DPCD_LANESET(u8 ds_level)
{
	//DPCD TRAINING_LANEx_SET
	//Bits 1:0 = VOLTAGE SWING SET
	//Bit 2 = MAX_SWING_REACHED
	//Bit 4:3 = PRE-EMPHASIS_SET
	//Bit 5 = MAX_PRE-EMPHASIS_REACHED

	switch (ds_level) {
	case DPTX_PHY_STA_VSW_0_PREEM_0:
		return (0x0 << 5 | 0x0 << 3 | 0x0 << 2 | 0x0);
	case DPTX_PHY_STA_VSW_0_PREEM_1:
		return (0x0 << 5 | 0x1 << 3 | 0x0 << 2 | 0x0);
	case DPTX_PHY_STA_VSW_0_PREEM_2:
		return (0x0 << 5 | 0x2 << 3 | 0x0 << 2 | 0x0);
	case DPTX_PHY_STA_VSW_0_PREEM_3:
		return (0x1 << 5 | 0x3 << 3 | 0x0 << 2 | 0x0);
	case DPTX_PHY_STA_VSW_1_PREEM_0:
		return (0x0 << 5 | 0x0 << 3 | 0x0 << 2 | 0x1);
	case DPTX_PHY_STA_VSW_1_PREEM_1:
		return (0x0 << 5 | 0x1 << 3 | 0x0 << 2 | 0x1);
	case DPTX_PHY_STA_VSW_1_PREEM_2:
		return (0x0 << 5 | 0x2 << 3 | 0x0 << 2 | 0x1);
	case DPTX_PHY_STA_VSW_2_PREEM_0:
		return (0x0 << 5 | 0x0 << 3 | 0x0 << 2 | 0x2);
	case DPTX_PHY_STA_VSW_2_PREEM_1:
		return (0x0 << 5 | 0x1 << 3 | 0x0 << 2 | 0x2);
	case DPTX_PHY_STA_VSW_3_PREEM_0:
		return (0x0 << 5 | 0x0 << 3 | 0x1 << 2 | 0x3);
	default:
		DPTX_ERR(NULL, "%s level %u out of protocal limit", __func__, ds_level);
		return 0;
	}
}

u8 dptx_v_p_to_ds(u8 vsw, u8 preem)
{
	if (vsw == 0 && preem < 4)
		return 0 + preem;
	else if (vsw == 1 && preem < 3)
		return 4 + preem;
	else if (vsw == 2 && preem < 2)
		return 7 + preem;
	else if (vsw == 3 && preem < 1)
		return 9 + preem;

	DPTX_ERR(NULL, "%s (vsw=%u preem=%u) out of protocal limit", __func__, vsw, preem);
	return 0;
}

u8 dptx_ds_to_vswing(u8 ds)
{
	if (ds < 4)
		return 0;
	if (ds < 7)
		return 1;
	if (ds < 9)
		return 2;
	return 3;
}

u8 dptx_ds_to_preem(u8 ds)
{
	if (ds == 3)
		return 3;
	if (ds == 2 || ds == 6)
		return 2;
	if (ds == 1 || ds == 5 || ds == 8)
		return 1;
	return 0;
}

void dptx_link_cfg_dft(struct dptx_drv_s *dptx, u8 port)
{
	// dptx->sink.link[port]->max_lane_count        = dptx->setting.user_lane_count;
	// dptx->sink.link[port]->max_link_rate         = dptx->setting.user_link_rate;
	// dptx->sink.link[port]->TPS_support           = 0; // TPS2
	// dptx->sink.link[port]->DACP_support          = 0;
	// dptx->sink.link[port].cap.link_cap               = 0;
	// dptx->sink.link[port]->train_rd_intv = 4; // 4ms
	// dptx->sink.link[port]->dev_type              = 1; // eDP
	// dptx->sink.link[port]->DPCD_reg_func         = 0;
	// dptx->sink.link[port]->enhanced_framing_en   = 1;
}

u8 dptx_DPCD_capability_to_link_cfg(struct dptx_drv_s *dptx, u8 port)
{
	u8 ad[16];

	//dptx_reg_print(dptx);

	memset(ad, 0, sizeof(u8) * 16);
	if (!(dptx->sink.port_mask & BIT(port))) {
		DPTX_P_ERR(dptx, port, "sink cfg port [%d] invalid", port);
		return 1;
	}
	if (dptx_if_aux_read(dptx, port, DPCD_DPCD_REV, 16, ad)) {
		DPTX_P_ERR(dptx, port, "fail to get DPCD capability");
		return 1;
	}

	dptx->sink.link[port].cap.DPCD_reg_func = ad[0xe] & BIT(7) ? BIT(0) : 0;
	if (dptx->sink.link[port].cap.DPCD_reg_func & BIT(0)) {
		DPTX_P_DBG(dptx, port, "extend receiver cap exist");
		if (dptx_if_aux_read(dptx, port, DPCD_EXD_DPCD_REV, 16, ad)) {
			DPTX_P_ERR(dptx, port, "fail to get exd-DPCD capability");
			dptx->sink.link[port].cap.DPCD_reg_func = 0;
		}
	}
	//DPCD_REVISION: 0x0000
	dptx->sink.link[port].cap.DPCD_ver        = ad[0];
	//DPCD_MAX_LINK_RATE: 0x0001
	dptx->sink.link[port].cap.max_link_rate   = ad[1];
	//DPCD_MAX_LANE_COUNT: 0x0002
	dptx->sink.link[port].cap.max_lane_count = ad[2] & 0xf;
	dptx->sink.link[port].cap.link_cap       = ad[2] & BIT(5) ? BIT(7) : 0; //POST_LT_ADJ_REQ
	dptx->sink.link[port].cap.link_cap      |= ad[2] & BIT(6) ? BIT(0) : 0; //TPS3
	dptx->sink.link[port].cap.link_cap      |= ad[2] & BIT(7) ? BIT(4) : 0; //enh_frame
	//DPCD_MAX_DOWNSPREAD: 0x0003
	dptx->sink.link[port].cap.link_cap      |= ad[3] & BIT(0) ? BIT(5) : 0; // down ss
	dptx->sink.link[port].cap.link_cap      |= ad[3] & BIT(7) ? BIT(1) : 0; // TPS4
	dptx->sink.link[port].cap.link_cap      |= ad[3] & BIT(6) ? BIT(6) : 0; // no AUX LT
	//NORP & DP_PWR_VOLTAGE_CAP: 0x0004
	// dptx->sink.link[port].cap.NORP           = (ad[4] & BIT(0)) + 1;
	//MAIN_LINK_CHANNEL_CODING_CAP: 0x0006
	dptx->sink.link[port].cap.coding_cap     = ad[6] & 0x3;
	//DOWN_STREAM_PORT_COUNT: 0x0007
	dptx->sink.link[port].cap.msa_ignore     = ad[7] & BIT(6) ? BIT(0) : 0; // MSA_PAR
	//RECEIVE_PORT0_CAP_0: 0x0008
	//eDP_CONFIGURATION_CAP: 0x000d
	if (ad[0xd]) {
		dptx->sink.link[port].cap.DACP_support   = ad[0xd] & BIT(0) ? BIT(2) : 0; //ASSR
		dptx->sink.link[port].cap.DPCD_reg_func |= ad[0xd] & BIT(3) ? BIT(1) : 0; //eDP
		// dptx->sink.link[port].dev_type = 1;
	}
	//8b/10b_TRAINING_AUX_RD_INTERVAL: 0x000e
	dptx->sink.link[port].cap.train_rd_intv = ad[0xe] & 0x7f;

	if (dptx->sink.link[port].cap.DPCD_reg_func & BIT(0)) {
		if (dptx_if_aux_read(dptx, port, DPCD_EXD_DPRX_FEATURE_ENUMERATION_LIST, 6, ad)) {
			DPTX_P_ERR(dptx, port, "fail to get exd-DPCD+ capability");
		} else {
			//adptive sync
			dptx->sink.link[port].cap.msa_ignore |= ad[4] & BIT(0) ? BIT(1) : 0;
			// dptx->sink.link[port].cap.max_link_rate_UHBR = ad[5];
		}
	}

	if (dptx->sink.link[port].cap.DPCD_reg_func & BIT(1)) { //eDP DPCD
		if (dptx_if_aux_read(dptx, port, DPCD_eDP_DPCD_REV, 5, ad)) {
			DPTX_P_ERR(dptx, port, "fail to get eDP-DPCD capability");
			dptx->sink.link[port].cap.DPCD_reg_func &= ~BIT(1);
			goto eDP_DPCD_CAP_GET_DONE;
		}

		dptx->sink.link[port].cap.eDP.ver = ad[0];
		dptx->sink.link[port].cap.eDP.bl_ctrl_cap |= ad[1] & BIT(0) ? BIT(0) : 0;//TCON
		dptx->sink.link[port].cap.eDP.bl_ctrl_cap |= ad[1] & BIT(1) ? BIT(1) : 0;//pin en
		dptx->sink.link[port].cap.eDP.bl_ctrl_cap |= ad[1] & BIT(2) ? BIT(2) : 0;//aux en
		dptx->sink.link[port].cap.eDP.test_cap    |= ad[1] & BIT(3) ? BIT(0) : 0;//pin en
		dptx->sink.link[port].cap.eDP.test_cap    |= ad[1] & BIT(4) ? BIT(1) : 0;//aux en
		dptx->sink.link[port].cap.eDP.ctrl_cap    |= ad[1] & BIT(5) ? BIT(0) : 0;//frc
		dptx->sink.link[port].cap.eDP.ctrl_cap    |= ad[1] & BIT(6) ? BIT(1) : 0;//color
		dptx->sink.link[port].cap.eDP.ctrl_cap    |= ad[1] & BIT(7) ? BIT(2) : 0;//power
		dptx->sink.link[port].cap.eDP.bl_adj_cap   = ad[2];
		dptx->sink.link[port].cap.eDP.od_cap      |= ad[3] & BIT(0) ? BIT(0) : 0;//od
		dptx->sink.link[port].cap.eDP.od_cap      |= ad[3] & BIT(3) ? BIT(1) : 0;//od-aux
		dptx->sink.link[port].cap.eDP.bl_ctrl_cap |= ((ad[3] >> 1) & 0x3) << 4;// bl_reg
		dptx->sink.link[port].cap.eDP.bl_ctrl_cap |= ad[3] & BIT(4) ? BIT(3) : 0;//lm-c
		dptx->sink.link[port].cap.eDP.bl_rgn_y     = (ad[4] >> 4)  + 1;
		dptx->sink.link[port].cap.eDP.bl_rgn_x     = (ad[4] & 0xf) + 1;
	}
eDP_DPCD_CAP_GET_DONE:

	if (!dptx_if_aux_read(dptx, port, DPCD_DSC_SUPPORT, 16, ad)) {
		DPTX_P_DBG(dptx, port, "get DSC capability %x,%x", ad[0], ad[1]);
		dptx->sink.link[port].cap.dsc.cap                  = ad[0];
		dptx->sink.link[port].cap.dsc.algm_ver             = ad[1];
		dptx->sink.link[port].cap.dsc.buf_block_size       = ad[2];
		dptx->sink.link[port].cap.dsc.slice_cap1           = ad[3];
		dptx->sink.link[port].cap.dsc.buf_bit_depth        = ad[4];
		dptx->sink.link[port].cap.dsc.feature_spt          = ad[5];
		dptx->sink.link[port].cap.dsc.max_bpp_0            = ad[6];
		dptx->sink.link[port].cap.dsc.max_bpp_1            = ad[7];
		dptx->sink.link[port].cap.dsc.color_fmt_cap        = ad[8];
		dptx->sink.link[port].cap.dsc.color_dep_cap        = ad[9];
		dptx->sink.link[port].cap.dsc.peak_throughput      = ad[0xa];
		dptx->sink.link[port].cap.dsc.max_slice_width      = ad[0xb];
		dptx->sink.link[port].cap.dsc.slice_cap2           = ad[0xc];
		dptx->sink.link[port].cap.dsc.bpp_delta_increment0 = ad[0xd];
		dptx->sink.link[port].cap.dsc.bpp_delta_increment1 = ad[0xf];
	}

	if (!dptx_if_aux_read(dptx, port, DPCD_eDP_PSR_CAP_SUPPORT_and_VERSION, 16, ad)) {
		DPTX_P_DBG(dptx, port, "get eDP PSR capability %x-%x-%x", ad[0], ad[1], ad[2]);
		dptx->sink.link[port].cap.eDP.PSR_cap        = ad[0];
		dptx->sink.link[port].cap.eDP.PSR_setup_time = (ad[1] >> 1) & 0x7;
		dptx->sink.link[port].cap.eDP.PSR_req       |= ad[1] & BIT(0) ? BIT(0) : 0;
		dptx->sink.link[port].cap.eDP.PSR_req       |= ad[1] & BIT(4) ? BIT(1) : 0;
		dptx->sink.link[port].cap.eDP.PSR_req       |= ad[1] & BIT(5) ? BIT(2) : 0;
		dptx->sink.link[port].cap.eDP.PSR_req       |= ad[1] & BIT(6) ? BIT(3) : 0;

		dptx->sink.link[port].cap.eDP.PSR2_SU_X_GRANULARITY  = ad[3];
		dptx->sink.link[port].cap.eDP.PSR2_SU_X_GRANULARITY  =
			dptx->sink.link[port].cap.eDP.PSR2_SU_X_GRANULARITY << 8;
		dptx->sink.link[port].cap.eDP.PSR2_SU_X_GRANULARITY |= ad[2];
		dptx->sink.link[port].cap.eDP.PSR2_SU_Y_GRANULARITY  = ad[4];
	}

	if (dptx_print_level <= LOG_I) {
		DPTX_P_PR(dptx, port,
			"sink DPCD[%01x.%01x] Cap: %u lane - %u.%uGHz MSA-ign:%u Adpt-Sync:%u",
			dptx->sink.link[port].cap.DPCD_ver >> 4,
			dptx->sink.link[port].cap.DPCD_ver & 0xf,
			dptx->sink.link[port].cap.max_lane_count,
			(dptx->sink.link[port].cap.max_link_rate * 27) / 100,
			(dptx->sink.link[port].cap.max_link_rate * 27) % 100,
			dptx->sink.link[port].cap.msa_ignore & BIT(0) ? 1 : 0,
			dptx->sink.link[port].cap.msa_ignore & BIT(1) ? 1 : 0);
		return 0;
	}

	DPTX_P_PR(dptx, port, "sink link DPCD Capability:\n"
		" - DPCD ver: %01x.%01x (exd DPCD = %u)\n"
		" - link:     %u lane, %u.%uGHz\n"
		" - LT:       [TPS3:%u, TPS4:%u, ss:%u, post_cur:%u, no_train:%u EQ-LT_rd:%uus]\n"
		" - coding:   [8b/10b:%u, 128b/132b:%u]\n"
		" - feature:  enh-frame:%u, MSA-ign:%u Adpt-Sync:%u\n"
		" - DACP:     HDCP = %u, eDP-ASSR = %u",
		dptx->sink.link[port].cap.DPCD_ver >> 4, dptx->sink.link[port].cap.DPCD_ver & 0xf,
		dptx->sink.link[port].cap.DPCD_reg_func & BIT(0) ? 1 : 0,
		dptx->sink.link[port].cap.max_lane_count,
		(dptx->sink.link[port].cap.max_link_rate * 27) / 100,
		(dptx->sink.link[port].cap.max_link_rate * 27) % 100,
		dptx->sink.link[port].cap.link_cap & BIT(0) ? 1 : 0,    //[0]=TPS3
		dptx->sink.link[port].cap.link_cap & BIT(1) ? 1 : 0,    //[1]=TPS4
		dptx->sink.link[port].cap.link_cap & BIT(5) ? 1 : 0,    //[5]=ss
		dptx->sink.link[port].cap.link_cap & BIT(7) ? 1 : 0,    //[7]=POST_LT_ADJ_REQ
		dptx->sink.link[port].cap.link_cap & BIT(6) ? 1 : 0,    //[6]=no-train
		dptx_train_rd_intv[dptx->sink.link[port].cap.train_rd_intv],
		dptx->sink.link[port].cap.coding_cap & BIT(0) ? 1 : 0,
		dptx->sink.link[port].cap.coding_cap & BIT(1) ? 1 : 0,
		dptx->sink.link[port].cap.link_cap & BIT(4) ? 1 : 0,    //[4]=enh_frame
		dptx->sink.link[port].cap.msa_ignore & BIT(0) ? 1 : 0,
		dptx->sink.link[port].cap.msa_ignore & BIT(1) ? 1 : 0,
		dptx->sink.link[port].cap.DACP_support & BIT(0) ? 1 : 0,
		dptx->sink.link[port].cap.DACP_support & BIT(2) ? 1 : 0);
	if (dptx->sink.link[port].cap.DPCD_reg_func & BIT(1)) {
		pr_info(" - eDP DPCD: %u (eDP %s)\n"
			"   - BL:     TCON-bl:%u aux:%u dynamic:%u bl-vsync:%u\n"
			"   - OD:     %u (aux ctrl: %u)\n",
			dptx->sink.link[port].cap.DPCD_reg_func & BIT(1) ? 1 : 0,
			eDP_ver_str[dptx->sink.link[port].cap.eDP.ver],
			dptx->sink.link[port].cap.eDP.bl_ctrl_cap & BIT(0) ? 1 : 0,
			dptx->sink.link[port].cap.eDP.bl_ctrl_cap & BIT(1) ? 1 : 0,
			dptx->sink.link[port].cap.eDP.bl_ctrl_cap & BIT(6) ? 1 : 0,
			dptx->sink.link[port].cap.eDP.bl_ctrl_cap & BIT(7) ? 1 : 0,
			dptx->sink.link[port].cap.eDP.od_cap & BIT(0) ? 1 : 0,
			dptx->sink.link[port].cap.eDP.od_cap & BIT(1) ? 1 : 0);
		if (dptx->sink.link[port].cap.eDP.PSR_cap) {
			pr_info("   - PSR:    %s\n"
				"     - setup: %uus\n"
				"     - req:   [LT_on_exit:%u Y-coor:%u non-FS:%u]\n"
				"     - GRANULARITY: req=%u [%u, %u]\n",
				eDP_PSR_str[dptx->sink.link[port].cap.eDP.PSR_cap],
				dptx_PSR_setup_time[dptx->sink.link[port].cap.eDP.PSR_setup_time],
				dptx->sink.link[port].cap.eDP.PSR_req & BIT(0) ? 1 : 0,
				dptx->sink.link[port].cap.eDP.PSR_req & BIT(1) ? 1 : 0,
				dptx->sink.link[port].cap.eDP.PSR_req & BIT(3) ? 1 : 0,
				dptx->sink.link[port].cap.eDP.PSR_req & BIT(2) ? 1 : 0,
				dptx->sink.link[port].cap.eDP.PSR2_SU_X_GRANULARITY,
				dptx->sink.link[port].cap.eDP.PSR2_SU_Y_GRANULARITY
			);
		}
	}
	if (dptx->sink.link[port].cap.dsc.cap) {
		pr_info(" - DSC:      %lu (%u.%u)\n"
			"   - Slice:  1:%c 2:%c 4:%c 6:%c 8:%c 10:%c 12:%c\n"
			"   - CFmt:   RGB:%c YUV444:%c YUV422(Sim):%c YUV422(Nat):%c YUV420:%c\n"
			"   - Cbit:   8b:%c 10b:%c 12b:%c\n", // new to DP2.0/eDP1.5
			dptx->sink.link[port].cap.dsc.cap & BIT(0),
			dptx->sink.link[port].cap.dsc.algm_ver & 0xf,
			dptx->sink.link[port].cap.dsc.algm_ver >> 4,
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(0) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(1) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(3) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(4) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(5) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(6) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.slice_cap1 & BIT(7) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_fmt_cap & BIT(0) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_fmt_cap & BIT(1) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_fmt_cap & BIT(2) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_fmt_cap & BIT(3) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_fmt_cap & BIT(4) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_dep_cap & BIT(1) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_dep_cap & BIT(2) ? 'Y' : 'N',
			dptx->sink.link[port].cap.dsc.color_dep_cap & BIT(3) ? 'Y' : 'N'
		);
	}
	return 0;
}

void dptx_link_policy_maker(struct dptx_drv_s *dptx, u8 port)
{
	//lane_count
	dptx->sink.link[port].lane_count =
		CAP_COMP(dptx->data->lane_count[dptx->idx][port],
			dptx->sink.link[port].cap.max_lane_count);
	if (dptx->panel_data.eDP.lane_count)
		dptx->sink.link[port].lane_count = dptx->panel_data.eDP.lane_count;
	if (dptx->setting.user_lane_count)
		dptx->sink.link[port].lane_count =
			CAP_COMP(dptx->sink.link[port].lane_count, dptx->setting.user_lane_count);

	if (!(dptx->sink.link[port].lane_count == 1 || dptx->sink.link[port].lane_count == 2 ||
	      dptx->sink.link[port].lane_count == 4))
		dptx->sink.link[port].lane_count = 1;

	//link_rate
	dptx->sink.link[port].link_rate =
		CAP_COMP(dptx->data->link_rate[dptx->idx][port],
			dptx->sink.link[port].cap.max_link_rate);
	if (dptx->panel_data.eDP.link_rate)
		dptx->sink.link[port].link_rate = dptx->panel_data.eDP.link_rate;

	if (dptx->setting.user_link_rate)
		dptx->sink.link[port].link_rate =
			CAP_COMP(dptx->sink.link[port].link_rate, dptx->setting.user_link_rate);

	if (dptx->sink.link[port].link_rate < 0x6)
		dptx->sink.link[port].link_rate = 0x6;
	//down ssc
	dptx->sink.link[port].ssc_en = dptx->sink.link[port].cap.link_cap & BIT(5) ? 1 : 0;

	//enh_frame
	dptx->sink.link[port].enh_frame_en = dptx->sink.link[port].cap.link_cap & BIT(4) ? 1 : 0;
}

#define DPTX_BW_CHECK_LIMIT_PERCENT       95 //95%
u8 dptx_vid_band_width_check(struct dptx_drv_s *dptx, u32 pclk, u8 bpp)
{
	u32 bw_req, bw_cal, bw_load, min_bw_per_port = U32_MAX;
	u8 port, valid_port = 0;

	bw_req = pclk / 1000000 + 1;

	if (bw_req > dptx->data->pixel_clk_limit) {
		DPTX_DBG(dptx, "%s: vid(%uMHz) over chip limit (%u)",
			__func__, bw_req, dptx->data->pixel_clk_limit);
		return 0;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		//if (dptx->sink.link[port].link_rate == DP_LINK_RATE_UBR10)
		//	bw_cal = 10000;
		//else if (dptx->sink.link[port].link_rate == DP_LINK_RATE_UBR135)
		//	bw_cal = 13500;
		//else if (dptx->sink.link[port].link_rate == DP_LINK_RATE_UBR20)
		//	bw_cal = 20000;
		//else
		bw_cal = dptx->sink.link[port].link_rate * 270;
		bw_cal *= dptx->sink.link[port].lane_count;
		bw_cal = bw_cal * 8 / 10; // assume 8b10b
		if (bw_cal < min_bw_per_port)
			min_bw_per_port = bw_cal;
		valid_port++;
	}

	if (valid_port == 0) {
		DPTX_ERR(dptx, "port invalid");
		return 0;
	}

	bw_req = (pclk / 1000000 + 1) * bpp;

	bw_load = bw_req * 100 / (min_bw_per_port * valid_port);

	if (bw_load > DPTX_BW_CHECK_LIMIT_PERCENT) {
		DPTX_DBG(dptx, "%s: vid(%uMHz) / %u link (min %uMHz) = bw_load(%u%%), over limit",
			__func__, bw_req, valid_port, min_bw_per_port, bw_load);
		return 0;
	}
	return 1;
}

struct dptx_color_format_s edp_cfmt_tb[DPTX_COLOR_TYPE_MAX] = {
	{DPTX_CFMT_RGB_6bit,        18, "RGB-6b"},
	{DPTX_CFMT_RGB_8bit,        24, "RGB-8b"},
	{DPTX_CFMT_RGB_10bit,       30, "RGB-10b"},
	{DPTX_CFMT_RGB_12bit,       36, "RGB-12b"},
	{DPTX_CFMT_YCbCr422_8bit,   16, "YUV422-8b"},
	{DPTX_CFMT_YCbCr422_10bit,  20, "YUV422-10b"},
	{DPTX_CFMT_YCbCr422_12bit,  24, "YUV422-12b"},
	{DPTX_CFMT_YCbCr444_8bit,   24, "YUV444-8b"},
	{DPTX_CFMT_YCbCr444_10bit,  30, "YUV444-10b"},
	{DPTX_CFMT_YCbCr444_12bit,  36, "YUV444-12b"},
	{DPTX_CFMT_YCbCr420_8bit,   12, "YUV420-8b"},
	{DPTX_CFMT_YCbCr420_10bit,  15, "YUV420-10b"},
	{DPTX_CFMT_YCbCr420_12bit,  18, "YUV420-12b"},
	{DPTX_CFMT_Y_only_8bit,      8, "Y-8b"},
	{DPTX_CFMT_Y_only_10bit,    10, "Y-10b"},
	{DPTX_CFMT_Y_only_12bit,    12, "Y-12b"},
	{DPTX_CFMT_invalid,          1, "invalid"},
};

struct dptx_detail_timing_s DPTX_SafeMode_640x480_timing = {
	.name = "SafeMode_480P",

	.h_size = 400,
	.v_size = 300,

	.h_period = 800,
	.h_act    = 640,
	.h_blank  = 160,
	.h_bp     = 48,
	.h_pw     = 86,
	.h_fp     = 16,

	.v_period = 525,
	.v_act    = 480,
	.v_blank  = 45,
	.v_bp     = 33,
	.v_pw     = 2,
	.v_fp     = 10,

	//.h_border = 0,
	//.v_border = 0,

	.ctrl = 0,

	.pclk = 25000000,
	.fr1000 = 60000,

	.cfmt = DPTX_CFMT_RGB_6bit,

	.v_period_range = {0, 0},
	.pclk_range = {0, 0},
	.frame_rate_range = {0, 0},
	//.vfreq_vrr_range = {0, 0},
	.sync_duration_num = 60,
	.sync_duration_den = 1,
};

struct dptx_vmode_s DPTX_SafeMode_640x480_vmode = {
	.fr100_int = 6000,
	.fr_adv = 0xff,
	.base_dtd_idx = 0xff,
	.flag = VMODE_FLAG_VALID,
	.cfmt_support = 1 << DPTX_CFMT_RGB_6bit,
};

u8 __str_add_vmode(struct dptx_drv_s *dptx, char *buf, struct dptx_vmode_s *vmd_p, u8 fr)
{
	u8 i, use_short = 0;
	u16 vmd_h, vmd_v, vmd_fr;
	// struct dptx_vmode_s *vmd;
	struct dptx_detail_timing_s *vmd_timing;
	struct V_name_s { u16 h, v; u8 fr[5]; } V_name_table[] = {
		{3840, 2160, {0xff,}},
		{1920, 1080, {0xff,}},
		{1366,  768, { 60, 59, 50, 48, 47}},
		{1280,  720, { 60, 50,  0,  0,  0}},
	};

	if (vmd_p->base_dtd_idx >= DPTX_DRV_TIMING_MAX)
		vmd_timing = &DPTX_SafeMode_640x480_timing;
	else
		vmd_timing = &dptx->panel_data.timing[vmd_p->base_dtd_idx];

	vmd_h = vmd_timing->h_act;
	vmd_v = vmd_timing->v_act;
	vmd_fr = vmd_p->fr_adv == 1 ?
			(vmd_p->fr100_int * 10 / 1001) : ((vmd_p->fr100_int + 50) / 100);

	for (i = 0; i < 5 * ARRAY_SIZE(V_name_table); i++) {
		if (V_name_table[i / 5].h == vmd_h && V_name_table[i / 5].v == vmd_v) {
			if (V_name_table[i / 5].fr[i % 5] == 0xff ||
			    V_name_table[i / 5].fr[i % 5] == vmd_fr) {
				use_short = 1;
				break;
			}
		}
	}

	i = use_short ? sprintf(buf,    "%up",        vmd_v) :
			sprintf(buf, "%ux%up", vmd_h, vmd_v);
	if (fr)
		i += sprintf(buf + i, "%huhz", vmd_fr);

	return i;
}
