// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/math64.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <uapi/amlogic/hdmi_rx.h>
#include "../tvin_global.h"

/* Local Include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw_t3x.h"
#include "hdmi_rx_hw_t6x.h"
#include "hdmi_rx_frl.h"
const u32 fpll[] = {
	0x21200035,
	0x30200035,
	0x83afa82a,
	0x00040000,
	0x0b0da001, //0x0b0da001
	0x10200035,
	0x0b0da201, //0x0b0da201
};

#define VPU_OVER_SPEED 390
#define VPU_OVER_SPEED1 838

int err_cnt_max = 15;
int ecc_err_monitor;
int low_frl_rate_en;
int force_low_rate = 1;
int vga_level_low = 0x1111;
int vga_level_default = 0xffff;
int frl_update_edid_en[E_PORT_NUM];

enum frl_rate_e hdmirx_get_frl_rate(u8 port)
{
	if (rx[port].var.frl_rate > 0xf)
		rx[port].var.frl_rate &= 0xf;
	return rx[port].var.frl_rate;
}

//================== LTS 2============================
void rx_lts_2_flt_ready(u8 port)
{
	u8 data8;

	//inv_hal_rx_lock_enable(); for config PHY
	//??? only has SR_DP_CTL3, no SR_CP_CTL0
	//data8  = 0;
	//TODO hdmirx_wr_cor(int_ext,SR_DP_CTL0_DPHY_IVCRX,data8, port);

	//data8  = 0;
	//TODO hdmirx_wr_cor(int_ext,SR_CP_CTL0_DPHY_IVCRX,data8, port);

	//-------
	data8  = 0;
	data8 |= (1 << 4); //reg_dual_pipe_en
	data8 |= (1 << 2); //reg_acr_div2mode
	data8 |= (1 << 1); //reg_acr_clk_sel
	data8 |= (1 << 0); //reg_tmds_clk_sel  0:tmds_clk ;1: hdmi21_tmds_clk

	//inv_hal_rx_h21_ctrl_write(port,0x00);
	hdmirx_wr_cor(RX_H21_CTRL_PWD_IVCRX, data8, port);

	//Error counter gets enabled only after SR_SYNC is detected by setting GP1 register to 0x6
	//hal_flt_rx_enable_error_counter(port);
	hdmirx_wr_cor(H21RXSB_GP1_REGISTER_M42H_IVCRX, 0x6, port); //

	//-always set BIST restart error counter through th LT
	//hal_flt_rx_bist_counter_ctrl(port,true);
	data8  = 0;
	data8 |= (0 << 7); //reg_en_bist
	data8 |= (0 << 6); //reg_csel
	data8 |= (0 << 5); //reg_scram_bswap
	data8 |= (1 << 4); //reg_resync
	hdmirx_wr_cor(SR_BIST_CTL0_DDPHY_IVCRX, data8, port);

	//--clears error counter and valid bit
	//
	//hal_flt_rx_clear_error_counter(port);
	data8  = 0;
	data8 |= (1    << 7); //ri_rs_clr_ecc_src
	data8 |= (0    << 6); //ri_rs_sel_sync
	data8 |= (0    << 3); //ri_rs_misc_ctrl
	data8 |= (0    << 2); //ri_rs_chk_en
	data8 |= (1    << 0); //ri_rs_err_cnt_sel1
	hdmirx_wr_cor(H21RXSB_CTRL3_M42H_IVCRX, data8, port); //

	//------
	//hal_flt_ready_mod(port,true)
	//---[6] FLT_READY; tell source the sink is ready for link training
	data8 = 0;
	data8 = hdmirx_rd_cor(SCDCS_STATUS_FLAGS0_SCDC_IVCRX, port);
	hdmirx_wr_cor(SCDCS_STATUS_FLAGS0_SCDC_IVCRX, (data8 | 0x40), port);
}

int rx_lts_3_err_detect(u8 port)
{
	u8 bist0_err_cnt_0;
	u8 bist0_err_cnt_1;
	u8 bist0_err_cnt_2;
	u8 bist1_err_cnt_0;
	u8 bist1_err_cnt_1;
	u8 bist1_err_cnt_2;
	u8 bist2_err_cnt_0;
	u8 bist2_err_cnt_1;
	u8 bist2_err_cnt_2;
	u8 bist3_err_cnt_0;
	u8 bist3_err_cnt_1;
	u8 bist3_err_cnt_2;
	u8 bist0_err_cnt;
	u8 bist1_err_cnt;
	u8 bist2_err_cnt;
	u8 bist3_err_cnt;
	u8 data8;
	u8 frl_rate_sel;
	int ch_check = 0;
	//-------hal_frl_eq_update(port)
	//s_run_cal_adap(port,0x01,0x02)//TODO
	//
	//reset BIST error counter and restart BIST
	hdmirx_wr_cor(SR_BIST_CTL0_DDPHY_IVCRX, 0x90, port);
	hdmirx_wr_cor(SR_BIST_CTL2_DDPHY_IVCRX, 0x00, port);
	hdmirx_wr_cor(SR_BIST_CTL3_DDPHY_IVCRX, 0x00, port);
	//frl_debug,dont need to config digital phy
	//hdmirx_wr_cor(SR_SCDC_CTL1_DDPHY_IVCRX, 0x65, port);
	//hdmirx_wr_cor(SR_SCDC_CTL2_DDPHY_IVCRX, 0x87, port);
	hdmirx_wr_cor(SR_BIST_CTL0_DDPHY_IVCRX, 0x80, port);

	//data8  =  0;
	//data8 |= (0 << 0); //reg_LTS_Cmd[3:0]
	//data8 |= (0 << 4); //reg_LTS_req
	//data8 |= (0 << 5); //reg_SCDC_sel
	//hdmirx_wr_cor(SR_SCDC_CTL0_DDPHY_IVCRX, data8, port);
	//==============

	//-------hal_flt_rx_bist_counter_ctrl()
	//data8 =  hdmirx_rd_COR(int_ext,SR_BIST_CTL0_DDPHY_IVCRX,a_b_sel );
	//hdmirx_wr_cor(int_ext,SR_BIST_CTL0_DDPHY_IVCRX,(data8 | 0x10),a_b_sel, port);

	//-------s_frl_check_eq_err(p)
	//
	//collect bist error count for each lane
	//channel 0
	if (!hdmirx_poll_cor(SARAH_BIST_ST_2_DDPHY_IVCRX, 0 << 7, 0x7f, frl_sync_cnt, port))
		ch_check |= _BIT(0);
	usleep_range(500, 2000);
	bist0_err_cnt_0 = hdmirx_rd_cor(SARAH_BIST_ST_0_DDPHY_IVCRX, port);
	bist0_err_cnt_1 = hdmirx_rd_cor(SARAH_BIST_ST_1_DDPHY_IVCRX, port);
	bist0_err_cnt_2 = hdmirx_rd_cor(SARAH_BIST_ST_2_DDPHY_IVCRX, port);
	if (log_level & FRL_LOG)
		rx_pr("bist0:0x%x -0x%x-0x%x\n", bist0_err_cnt_0, bist0_err_cnt_1, bist0_err_cnt_2);
	bist0_err_cnt = ((bist0_err_cnt_2 & 0x7f) << 16) |
					 (bist0_err_cnt_1  << 8) | bist0_err_cnt_0;
	if (bist0_err_cnt > 0) {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **************Bist0 ERROR************\n");
		ch_check |= _BIT(0);
	} else {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **************Bist0 PASS************\n");
	}
	//channel 1
	if (!hdmirx_poll_cor(SARAH_BIST1_ST_2_DDPHY_IVCRX, 0 << 7, 0x7f, frl_sync_cnt, port))
		ch_check |= _BIT(1);
	usleep_range(500, 2000);
	bist1_err_cnt_0 = hdmirx_rd_cor(SARAH_BIST1_ST_0_DDPHY_IVCRX, port);
	bist1_err_cnt_1 = hdmirx_rd_cor(SARAH_BIST1_ST_1_DDPHY_IVCRX, port);
	bist1_err_cnt_2 = hdmirx_rd_cor(SARAH_BIST1_ST_2_DDPHY_IVCRX, port);
	if (log_level & FRL_LOG)
		rx_pr("bist1:0x%x-0x%x-0x%x\n", bist1_err_cnt_0, bist1_err_cnt_1, bist1_err_cnt_2);
	bist1_err_cnt = ((bist1_err_cnt_2 & 0x7f) << 16) |
					 (bist1_err_cnt_1 << 8) | bist1_err_cnt_0;
	if (bist1_err_cnt > 0) {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **********Bist1 ERROR************\n");
		ch_check |= _BIT(1);
	} else {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **************Bist1 PASS************\n");
	}
	//channel 2
	if (!hdmirx_poll_cor(SARAH_BIST2_ST_2_DDPHY_IVCRX, 0 << 7, 0x7f, frl_sync_cnt, port))
		ch_check |= _BIT(2);
	usleep_range(500, 2000);
	bist2_err_cnt_0 = hdmirx_rd_cor(SARAH_BIST2_ST_0_DDPHY_IVCRX, port);
	bist2_err_cnt_1 = hdmirx_rd_cor(SARAH_BIST2_ST_1_DDPHY_IVCRX, port);
	bist2_err_cnt_2 = hdmirx_rd_cor(SARAH_BIST2_ST_2_DDPHY_IVCRX, port);
	if (log_level & FRL_LOG)
		rx_pr("bist2:0x%x-0x%x-0x%x\n", bist2_err_cnt_0, bist2_err_cnt_1, bist2_err_cnt_2);
	bist2_err_cnt = ((bist2_err_cnt_2  &  0x7f) << 16) |
					 (bist2_err_cnt_1 << 8) | bist2_err_cnt_0;
	if (bist2_err_cnt > 0) {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **************Bist2 ERROR************\n");
		ch_check |= _BIT(2);
	} else {
		if (log_level & FRL_LOG)
			rx_pr("[FRL ERROR] **************Bist2 PASS************\n");
	}
	if (rx[port].var.frl_rate >= FRL_RATE_6G_4LANES) {
		//channel 3
		if (!hdmirx_poll_cor(SARAH_BIST3_ST_2_DDPHY_IVCRX, 0 << 7,
			0x7f, frl_sync_cnt, port))
			ch_check |= _BIT(3);
		usleep_range(500, 2000);

		bist3_err_cnt_0 = hdmirx_rd_cor(SARAH_BIST3_ST_0_DDPHY_IVCRX, port);
		bist3_err_cnt_1 = hdmirx_rd_cor(SARAH_BIST3_ST_1_DDPHY_IVCRX, port);
		bist3_err_cnt_2 = hdmirx_rd_cor(SARAH_BIST3_ST_2_DDPHY_IVCRX, port);
		if (log_level & FRL_LOG)
			rx_pr("bist3:0x%x-0x%x-0x%x\n", bist3_err_cnt_0,
				bist3_err_cnt_1, bist3_err_cnt_2);
		bist3_err_cnt = ((bist3_err_cnt_2 & 0x7f) << 16) |
						 (bist3_err_cnt_1 << 8) | bist3_err_cnt_0;
		if (bist3_err_cnt > 0) {
			if (log_level & FRL_LOG)
				rx_pr("[FRL ERROR] **************Bist3 ERROR************\n");
			ch_check |= _BIT(3);
		} else {
			if (log_level & FRL_LOG)
				rx_pr("[FRL ERROR] **************Bist3 PASS************\n");
		}
	}
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] **%s end************\n", __func__);
	//enable Rx hdmi21 module before Tx
	frl_rate_sel = (rx[port].var.frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;

	data8  = 0;
	data8 |= (0               << 6); //reg_debug_ctl
	data8 |= (0               << 5); //reg_filter_en
	//data8 |= (1               << 4); //reg_scramble_en
	data8 |= (frl_scrambler_en    << 4); //reg_scramble_en
	data8 |= (0               << 2); //reg_scdt_reset_mask
	data8 |= (frl_rate_sel    << 1); //reg_lane_sel_mode: 0-3Lane; 1:4Lane
	data8 |= (1               << 0); //reg_en
	hdmirx_wr_cor(H21RXSB_CTRL_M42H_IVCRX, data8, port); //
	return ch_check;
}

void rx_lts_3_ltp_req_send_0000(u8 port)
{
	//u8 data8;
	//================== done =============
	//if ltp_0/1/2/3 ==0 then training done; let the source know we're done training
	//hal_flt_ltp_req_write(port,FLT_CODE_NO_LTP,FLT_CODE_NO_LTP)
	//hal_flt_ltp_req_write()
	//[3:0]  ln0_ltp_req [7:4] ln1_ltp_req
	hdmirx_wr_cor(SCDCS_STATUS_FLAGS1_SCDC_IVCRX, 0x00, port);
	//[3:0]  ln2_ltp_req [7:4] ln3_ltp_req
	hdmirx_wr_cor(SCDCS_STATUS_FLAGS2_SCDC_IVCRX, 0x00, port);
	//data8 = hdmirx_rd_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, port);
	//rx_pr("upd flg=0x%x\n", data8);
	//hdmirx_wr_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, (data8 | 0x20), port);//
}

//======================= LTS P ========================
int rx_lts_p_syn_detect(u8 frl_rate, u8 port)
{
	u8  frl_transmission_detected = 0;
	u8  channel_lock;
	u8  channel_lock_shift;
	u8  lane_count;
	u8  frl_rate_sel;
	u8  data8;
	int ret = true;
	u32 i = 0;

	// P state summary: FRL training has passed
	//TODO inv_hal_rx_sarah_phy_clk_gen(port)

	//======start super block decoder ====
	//-----hal_frl_rx_enable_sb(port, true, p->frl_rate);
	//enable RS error counters

	frl_rate_sel = (frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;
	//frl_debug
	//hdmirx_wr_cor(H21RXSB_NMUL_M42H_IVCRX, odn_reg_n_mul, port);
	data8  = 0;
	data8 |= (0               << 6); //reg_debug_ctl
	data8 |= (0               << 5); //reg_filter_en
	//data8 |= (1               << 4); //reg_scramble_en
	data8 |= (frl_scrambler_en    << 4); //reg_scramble_en
	data8 |= (0               << 2); //reg_scdt_reset_mask
	data8 |= (frl_rate_sel    << 1); //reg_lane_sel_mode: 0-3Lane; 1:4Lane
	data8 |= (1               << 0); //reg_en
	hdmirx_wr_cor(H21RXSB_CTRL_M42H_IVCRX, data8, port);

	//-----hal_frl_enable_cpath(port)
	//use to set CPATH after link training
	//frl_debug,dont need config digital phy
	//hdmirx_wr_cor(SR_CP_CTL0_DPHY_IVCRX, 0x43, port);
	//hdmirx_wr_cor(SR_CP_CTL1_DPHY_IVCRX, 0x8E, port);

	//wait here to get lock on all lanes!!!

	//[0]clk_detected;[1]ch0_locked;[2]ch1_locked;[3]ch2_locked;[4]ln3_locked
	lane_count = (frl_rate == 0) ? 0 : (frl_rate <= FRL_RATE_6G_3LANES) ? 3 : 4;
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] **lane_count = %x**\n", lane_count);

	while (frl_transmission_detected == 0) {
		channel_lock = hdmirx_rd_cor(SCDCS_STATUS_FLAGS0_SCDC_IVCRX, port);
	    channel_lock_shift = (channel_lock >> 1) & 0xf;
		//if (log_level & FRL_LOG)
			//rx_pr("[FRL TRAINING] *channel_lock_shift = %x*\n", channel_lock_shift);

		if (lane_count == 3) {
			if (channel_lock_shift == 7)
				frl_transmission_detected = 1;
		} else if (lane_count == 4) {
			if (channel_lock_shift == 15)
				frl_transmission_detected = 1;
		}
		udelay(5);
		if (i++ > ext_cnt) {
			ret = false;
			break;
		}
	}
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] *channel_lock_shift = %x,i=%d*\n", channel_lock_shift, i);
	//======start super block decoder ====
	//-----hal_frl_rx_enable_sb(port, true, p->frl_rate);
	//enable RS error counters
	frl_rate_sel = (frl_rate > FRL_RATE_6G_3LANES) ? 1 : 0;

	data8  = 0;
	data8 |= (0 << 6); //reg_debug_ctl
	data8 |= (0 << 5); //reg_filter_en
	data8 |= (frl_scrambler_en << 4); //reg_scramble_en
	data8 |= (0 << 2); //reg_scdt_reset_mask
	data8 |= (frl_rate_sel << 1); //reg_lane_sel_mode: 0-3Lane; 1:4Lane
	data8 |= (1 << 0); //reg_en
	hdmirx_wr_cor(H21RXSB_CTRL_M42H_IVCRX, data8, port);

	//======= clear Link Training mode =========
	//----hal_flt_rx_clear_rscc(port)
	//wait at least 100ms for SR_SYNC to be enabled

	if (!hdmirx_poll_cor(H21RXSB_STATUS_M42H_IVCRX, 1 << 2, 0xfb, frl_sync_cnt, port)) //sb sync
		ret = false;
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] Polling sb_sync Done ************\n");

	if (!hdmirx_poll_cor(H21RXSB_STATUS_M42H_IVCRX, 1 << 3, 0xf7, frl_sync_cnt, port)) //sr sync
		ret = false;
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] Polling sr_sync Done ************\n");

	hdmirx_wr_cor(H21RXSB_CTRL3_M42H_IVCRX, 0x81, port); //
	hdmirx_wr_cor(H21RXSB_CTRL3_M42H_IVCRX, 0x09, port); //
	//
	//----hal_flt_rx_dpll_reset_toggle(port)
	hdmirx_wr_bits_cor(DPLL_CTRL2_DPLL_IVCRX, _BIT(1), 1, port);
	hdmirx_wr_bits_cor(DPLL_CTRL2_DPLL_IVCRX, _BIT(1), 1, port);
	//
	//----hal_flt_rx_hdmi2p1_lt_mode_set(port,false)
	//(NONE)
	//
	//----hal_flt_rx_bist_counter_ctrl(port,true)
	//clear the BIST error counters
	hdmirx_wr_cor(SR_BIST_CTL0_DDPHY_IVCRX, 0x10, port); //
	if (log_level & FRL_LOG)
		rx_pr("[FRL TRAINING] *%s End*\n", __func__);
	return ret;
}

void rx_lts_p_frl_start(u8 port)
{
	u8 data8;
	//====== tell source to send video =======
	//----hal_frl_start_mod(port,true)!!!!!!!!!!!!!!

	data8 = hdmirx_rd_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, port);
	hdmirx_wr_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, (data8 | 0x10), port);//frl_start
	hdmirx_wr_bits_cor(H21RXSB_INTR2_M42H_IVCRX, _BIT(4), 1, port);
	rx_set_frl_train_sts(E_FRL_TRAIN_FINISH, port);
}

void rx_flt_update_set(u8 port)
{
	u8 data8;

	data8 = hdmirx_rd_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, port);
	hdmirx_wr_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, (data8 | 0x20), port);
}

void hdmi_tx_rx_frl_training_main(u8 port)
{
	int ret;

	rx[port].var.frl_rate = hdmirx_rd_cor(SCDCS_CONFIG1_SCDC_IVCRX, port) & 0xf;
	//hdmirx_wr_bits_cor(SCDCS_SRC_TEST_CONFIG_SCDC_IVCRX, _BIT(5), 1, port);
	if (rx_info.chip_id == CHIP_ID_T3X)
		aml_phy_init_t3x(port);
	else
		aml_phy_init_t6x(port);
	hdmirx_wr_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, 0x0, port);//sink clear(=0) FRL_START
	hdmirx_wr_cor(SCDCS_STATUS_FLAGS1_SCDC_IVCRX, 0x65, port); //
	hdmirx_wr_cor(SCDCS_STATUS_FLAGS2_SCDC_IVCRX, 0x87, port); //
	rx_flt_update_set(port);
	if (!hdmirx_flt_update_cleared_wait(SCDCS_UPD_FLAGS_SCDC_IVCRX, port)) {
		if (log_level & FRL_LOG)
			rx_pr("polling time out a\n");
	}
	if (rx_info.chip_id == CHIP_ID_T3X)
		aml_phy_init_t3x(port);
	else
		aml_phy_init_t6x(port);
	if (rx[port].var.frl_rate == FRL_12G_4LANE && low_frl_rate_en == 2)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, VGA_GAIN,
		vga_level_low, port);
	hdmirx_wr_top(TOP_SW_RESET, 0x80, port);
	hdmirx_wr_top(TOP_SW_RESET, 0, port);
	hdmirx_wr_bits_cor(DPLL_CTRL2_DPLL_IVCRX, _BIT(1), 0, port);
	hdmirx_wr_bits_cor(DPLL_CTRL2_DPLL_IVCRX, _BIT(1), 1, port);
	usleep_range(20000, 21000);
	ret = rx_lts_3_err_detect(port);
	if (ret)
		rx_pr("link training result = 0x%x\n", ret);
	if (ret && low_frl_rate_en &&
		rx[port].var.frl_rate == FRL_12G_4LANE) {
		hdmirx_wr_cor(SCDCS_STATUS_FLAGS1_SCDC_IVCRX, 0xff, port);
		hdmirx_wr_cor(SCDCS_STATUS_FLAGS2_SCDC_IVCRX, 0xff, port);
		rx_flt_update_set(port);
		rx_set_frl_train_sts(E_FRL_TRAIN_FINISH, port);
		return;
	}
	rx_lts_3_ltp_req_send_0000(port);
	rx_flt_update_set(port);
	if (rx[port].var.frl_rate == FRL_12G_4LANE && low_frl_rate_en == 2)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, VGA_GAIN,
		vga_level_default, port);
	if (!hdmirx_flt_update_cleared_wait(SCDCS_UPD_FLAGS_SCDC_IVCRX, port)) {
		if (log_level & FRL_LOG)
			rx_pr("polling time out b\n");
	}
	rx_set_frl_train_sts(E_FRL_TRAIN_FINISH, port);
}

enum frl_train_sts_e rx_get_frl_train_sts(u8 port)
{
	if (port == E_PORT2)
		return frl_train_sts;
	else
		return frl_train_sts1;
}

void rx_set_frl_train_sts(enum frl_train_sts_e sts, u8 port)
{
	if (port == E_PORT2)
		frl_train_sts = sts;
	else
		frl_train_sts1 = sts;
}

bool is_frl_train_finished(u8 port)
{
	bool ret = false;

	if (rx_get_frl_train_sts(port) == E_FRL_TRAIN_FINISH)
		ret = true;
	return ret;
}

void rx_frl_train_handler(struct kthread_work *work)
{
	hdmi_tx_rx_frl_training_main(E_PORT2);
}

void rx_frl_train_handler_1(struct kthread_work *work)
{
	hdmi_tx_rx_frl_training_main(E_PORT3);
}

void rx_frl_train(u8 port)
{
	if (port == E_PORT2) {
		kthread_queue_work(&frl_worker, &frl_work);
		rx_set_frl_train_sts(E_FRL_TRAIN_START, port);
	} else {
		kthread_queue_work(&frl1_worker, &frl1_work);
		rx_set_frl_train_sts(E_FRL_TRAIN_START, port);
	}
}

void hdmirx_fpll_recovery(u8 port)
{
	switch (rx_info.chip_id) {
	case CHIP_ID_T3X:
		hdmirx_fpll_recovery_t3x(port);
		break;
	case CHIP_ID_T6X:
		hdmirx_fpll_recovery_t6x(port);
		break;
	default:
		break;
	}
}

void hdmirx_frl_config(u8 port)
{
	u8 data8;
	u32 data32;
	u32 frl_rate_sel;

	hdmirx_wr_cor(DPLL_CFG6_DPLL_IVCRX, 0x0, port);
	/* new for t3x */
	data32  = 0;
	data32 |= (0 << 31); //[   31] update_man
	data32 |= (0 << 30); //[30] load reverse
	data32 |= (2 << 27); //[29:27] delay cycle of update
	data32 |= (50 << 13); //[26:13] update wide
	data32 |= (200 << 0); //[12:0]  update hold width
	hdmirx_wr_top(TOP_TCR_CNTL, data32, port);

	hdmirx_wr_cor(DPLL_CFG6_DPLL_IVCRX, 0x0, port);
	hdmirx_wr_cor(H21RXSB_D2TH_M42H_IVCRX, 0x20, port);
	//clk ready threshold
	hdmirx_wr_cor(H21RXSB_DIFF1T_M42H_IVCRX, 0x20, port);

	//step adjust
	hdmirx_wr_cor(H21RXSB_STEPF1_M42H_IVCRX, 0x18, port); //fast step 0x1800
	hdmirx_wr_cor(H21RXSB_STEPFL1_IVCRX, 0x18, port);     //slow step 0x1800

	hdmirx_wr_cor(H21RXSB_RST_CTRL_M42H_IVCRX, 0x2, port);
	hdmirx_wr_cor(H21RXSB_CTH2_M42H_IVCRX, 0x60, port);   //clk en th
	hdmirx_wr_cor(H21RXSB_INTR2_MASK_M42H_IVCRX, 0x10, port); //rs error detect

	//hdmirx_wr_cor(H21RXSB_INTR3_MASK_M42H_IVCRX, 0x2, port);
	/* end */
	//used for debug,manual config mode
	hdmirx_wr_cor(SCDCS_CONFIG1_SCDC_IVCRX, rx[port].var.frl_rate, port);
	//============= H21RXSB_CTRL3 =================
	data8  = 0;
	data8 |= (0 << 7);  //[7]   ri_en_clr_ecc_src
	data8 |= (0 << 6);  //[6]   ri_rs_sel_sync
	data8 |= (1 << 3);  //[5:3] ri_rs_misc_ctrl
	data8 |= (0 << 2);  //[2]   ri_rs_chk_en
	data8 |= (0 << 0);  //[1:0] ri_rs_err_cnt_sel1
	hdmirx_wr_cor(H21RXSB_CTRL3_M42H_IVCRX, data8, port);

	//=============RX H21_CTRL=================
	data8  = 0;
	data8 |= (1 << 4); //reg_dual_pipe_en
	/* for dacr */
	data8 |= (1 << 2); //reg_acr_div2mode
	data8 |= (1 << 1); //reg_acr_clk_sel
	data8 |= (1 << 0); //reg_tmds_clk_sel  0:tmds_clk ;1: hdmi21_tmds_clk
	hdmirx_wr_cor(RX_H21_CTRL_PWD_IVCRX, data8, port);
	//=============H21RXSB_CTRL1=================
	data8  = 0;
	data8 |= (1 << 7);  //[7] pace_clk_en buf fixed
	data8 |= (0 << 2);  //[2] ri_rs_blk_ln_er
	data8 |= (0 << 1);  //[1] ri_read_rs_err
	data8 |= (1 << 0);  //[0] ri_rs_errcnt_en
	hdmirx_wr_cor(H21RXSB_CTRL1_M42H_IVCRX, data8, port);
	//=============H21RXSB_CTRL2=================
	data8  = 0;
	data8 |= (0 << 6);  //[7:6]ri_rs_chk_mode
	data8 |= (0 << 5);  //[5]  ri_accm_err_manu_clr
	data8 |= (1 << 4);  //[4]  ri_mask_sw_reset
	data8 |= (0 << 3);  //[3]  ri_en_dis_cnt_mask
	data8 |= (1 << 2);  //[2]  ri_en_dis_vld_mask
	data8 |= (0 << 1);  //[1]  ri_en_mask
	data8 |= (0 << 0);  //[0]  ri_rs_err_cnt_sel
	hdmirx_wr_cor(H21RXSB_CTRL2_M42H_IVCRX, data8, port);

	//=============H21RXSB_CTRL4=================
	data8  = 0;
	data8 |= (0 << 4);  //[4]    ri_rserr_cnt_ld_en
	data8 |= (0 << 3);  //[3]    ri_rserr_vld_ld_en
	data8 |= (0 << 2);  //[2]    ri_rserr_ssb_ctl_en
	data8 |= (0 << 0);  //[1:0]  ri_rserr_chk_mod2_ctrl
	hdmirx_wr_cor(H21RXSB_CTRL4_M42H_IVCRX, data8, port);

	//for cts test
	hdmirx_wr_bits_cor(H21RXSB_GCC_M42H_IVCRX, _BIT(6), 1, port);
	//=============H21RXSB_RS_ACC_ERR_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0xf0 << 0);  //[7:0] ri_accm_err_thr_lsb
	hdmirx_wr_cor(H21RXSB_RS_ACC_ERR_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_ACC_ERR_TSH_LSB1_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x00 << 0);  //[7:0] ri_accm_err_thr_lsb1
	hdmirx_wr_cor(H21RXSB_RS_ACC_ERR_TSH_LSB1_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_ACC_ERR_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x00 << 0);  //[7:0] ri_accm_err_thr_msb
	hdmirx_wr_cor(H21RXSB_RS_ACC_ERR_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_CNT2CHK_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x3c << 0);  //[7:0] ri_cnt2chk_rs_lsb
	hdmirx_wr_cor(H21RXSB_RS_CNT2CHK_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_CNT2CHK_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_cnt2chk_rs_msb
	hdmirx_wr_cor(H21RXSB_RS_CNT2CHK_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_FRAME_RS_ERR_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0xf << 0);  //[7:0] ri_frame_rs_err_thr_lsb
	hdmirx_wr_cor(H21RXSB_FRAME_RS_ERR_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_FRAME_RS_ERR_TSH_LSB1_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_frame_rs_err_thr_lsb1
	hdmirx_wr_cor(H21RXSB_FRAME_RS_ERR_TSH_LSB1_M42H_IVCRX, data8, port);

	//============= H21RXSB_FRAME_RS_ERR_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_frame_rs_err_thr_msb
	hdmirx_wr_cor(H21RXSB_FRAME_RS_ERR_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_CONS_RS_ERR_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x10 << 0);  //[7:0] ri_cons_ri_err_thr_lsb
	hdmirx_wr_cor(H21RXSB_CONS_RS_ERR_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_CONS_RS_ERR_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_cons_ri_err_thr_msb
	hdmirx_wr_cor(H21RXSB_CONS_RS_ERR_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_CONS_RS_ERR_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0xf0 << 0);  //[7:0] ri_cons_ri_err_thr_lsb
	hdmirx_wr_cor(H21RXSB_CONS_RS_ERR_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_CONS_RS_ERR_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_cons_ri_err_thr_msb
	hdmirx_wr_cor(H21RXSB_CONS_RS_ERR_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_FRAME_TSH_ACCM_RS_ERR_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x2 << 0);  //[7:0] ri_given_frame_lsb
	hdmirx_wr_cor(H21RXSB_FRAME_TSH_ACCM_RS_ERR_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_FRAME_TSH_ACCM_RS_ERR_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0   << 0);  //[7:0] ri_given_frame_msb
	hdmirx_wr_cor(H21RXSB_FRAME_TSH_ACCM_RS_ERR_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] ri_given_frame_err_thr_lsb
	hdmirx_wr_cor(H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB1_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x10 << 0);  //[7:0] ri_given_frame_err_thr_lsb1
	hdmirx_wr_cor(H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_LSB1_M42H_IVCRX, data8, port);

	//============= H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x00 << 0);  //[7:0] ri_given_frame_err_thr_msb
	hdmirx_wr_cor(H21RXSB_GIVEN_FRAME_RSERR_ACCM_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_ERRCNT_TSH_MSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x10 << 0);  //[7:0] reg_rserrcnt_tsh_msb
	hdmirx_wr_cor(H21RXSB_RS_ERRCNT_TSH_MSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_RS_ERRCNT_TSH_LSB_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 0);  //[7:0] reg_rserrcnt_tsh_lsb
	hdmirx_wr_cor(H21RXSB_RS_ERRCNT_TSH_LSB_M42H_IVCRX, data8, port);

	//============= H21RXSB_INTR1_MASK_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x0 << 6);  //[6] reg_intr1_mask6
	data8 |= (0x0 << 5);  //[5] reg_intr1_mask5
	data8 |= (0x0 << 4);  //[4] reg_intr1_mask4
	data8 |= (0x0 << 3);  //[3] reg_intr1_mask3
	data8 |= (0x0 << 2);  //[2] reg_intr1_mask2
	data8 |= (0x0 << 1);  //[1] reg_intr1_mask1
	data8 |= (0x1 << 0);  //[0] reg_intr1_mask0
	hdmirx_wr_cor(H21RXSB_INTR1_MASK_M42H_IVCRX, data8, port);

	/* t3x new */
	//============= H21RXSB_GP1_REGISTER_M42H_IVCRX=================
	data8 = 0;
	data8 |= (1 << 1); //[1] =0:use sb_checker sb_sync as frl_lock signal;=1 not use
	data8 |= (1 << 2); //[2] =1:use sb_checker sr_sync as frl_lock signal;=0 not use
	data8 |= (1 << 3); //[3] auto reset after overlap
	data8 |= (1 << 5); //[3] fifo reset enable both t_clk and l_clk
	hdmirx_wr_cor(H21RXSB_GP1_REGISTER_M42H_IVCRX, data8, port);

	//============= H21RXSB_GP3_REGISTER_M42H_IVCRX=================
	data8 = 0;
	data8 |= (1 << 5);   //[5] use sb_checker sb_sr_sync as frl_lock signal to frl pipe
	hdmirx_wr_cor(H21RXSB_GP3_REGISTER_M42H_IVCRX, data8, port);
	/* end */
	//============= H21RXSB_CTRL_M42H_IVCRX=================
	frl_rate_sel = (rx[port].var.frl_rate > FRL_6G_3LANE) ? 1 : 0;
	data8 = 0;
	data8 |= (0 << 6); //reg_debug_ctl
	data8 |= (1 << 5); //reg_filter_en
    //data8 |= (1 << 4); //reg_scramble_en
	data8 |= (frl_scrambler_en << 4); //reg_scramble_en
	data8 |= (0 << 2); //reg_scdt_reset_mask
	data8 |= (frl_rate_sel << 1); //reg_lane_sel_mode: 0-3Lane; 1:4Lane
	data8 |= (1 << 0); //reg_en
	hdmirx_wr_cor(H21RXSB_CTRL_M42H_IVCRX, data8, port); //

	//----- init th ---
	//default = 0x100000; sim time too long , make the value small.
	hdmirx_wr_cor(H21RXSB_ITH0_M42H_IVCRX, 0x0, port); //
	hdmirx_wr_cor(H21RXSB_ITH1_M42H_IVCRX, 0x0, port); //
	hdmirx_wr_cor(H21RXSB_ITH2_M42H_IVCRX, 0x2, port); //
	//tracking difference threshold high 8bit,0x10->0xf
	hdmirx_wr_cor(H21RXSB_D2TH_M42H_IVCRX, 0xf, port);
	//============= H21RXSB_CTRL6_M42H_IVCRX=================
	data8  = 0;
	data8 |= (0x1   << 0);  //[7:0] reg_lane_en_sel_scdcs_sb
	hdmirx_wr_cor(H21RXSB_CTRL6_M42H_IVCRX, data8, port); //0x1568

	hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(0), 0, port);

	//fpll default to avoid m valid not up
	//m valid based on tmds clk

	hdmirx_fpll_recovery(port);
}

bool is_fpll0_locked(void)
{
	bool lock_flg;

	if (rx_info.chip_id == CHIP_ID_T3X) {
		lock_flg = hdmirx_rd_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL0_CTRL0, _BIT(31));
	} else {
		lock_flg = (rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0) >> 31) & 0x1;
		if (log_level & FRL_LOG)
			rx_pr("lock bit = %d\n", lock_flg);
	}
	return lock_flg;
}

bool is_fpll1_locked(void)
{
	bool lock_flg;

	if (rx_info.chip_id == CHIP_ID_T3X) {
		lock_flg = hdmirx_rd_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL1_CTRL0, _BIT(31));
	} else {
		lock_flg = (rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0) >> 31) & 0x1;
		if (log_level & FRL_LOG)
			rx_pr("lock bit = %d\n", lock_flg);
	}
	return lock_flg;
}

int is_fpll_locked(u8 port)
{
	if (port == E_PORT2)
		return is_fpll0_locked();
	else
		return is_fpll1_locked();
}

void rx_write_fpll(u32 pll_ctrl, u32 value)
{
	if (rx_info.chip_id == CHIP_ID_T3X)
		wr_reg_clk_ctl(pll_ctrl, value);
	else
		wr_reg_ana_ctl(pll_ctrl, value);
}

void rx_21_fpll_calculation(int f_rate, u8 port)
{
	u8 reg_valid_m;
	unsigned long o_req_m, flclk, tclk, temp;
	u8 pre_div;
	u64 temp1;
	u64 temp2;
	int cnt = 0;
	unsigned long data = 0;
	u32 pll_ctrl0, pll_ctrl1, pll_ctrl2, pll_ctrl3;
	u32 eq_stg2_temp = 0;
	u32 eq_pole_temp = 0;

	if (log_level & FRL_LOG)
		rx_pr("fpll cal,port=%d\n", port);

	if (rx_info.chip_id == CHIP_ID_T3X) {
		if (port == E_PORT2) {
			pll_ctrl0 = T3X_CLKCTRL_HDMI_PLL0_CTRL0;
			pll_ctrl1 = T3X_CLKCTRL_HDMI_PLL0_CTRL1;
			pll_ctrl2 = T3X_CLKCTRL_HDMI_PLL0_CTRL2;
			pll_ctrl3 = T3X_CLKCTRL_HDMI_PLL0_CTRL3;
		} else {
			pll_ctrl0 = T3X_CLKCTRL_HDMI_PLL1_CTRL0;
			pll_ctrl1 = T3X_CLKCTRL_HDMI_PLL1_CTRL1;
			pll_ctrl2 = T3X_CLKCTRL_HDMI_PLL1_CTRL2;
			pll_ctrl3 = T3X_CLKCTRL_HDMI_PLL1_CTRL3;
		}
	} else {
		if (port == E_PORT2) {
			pll_ctrl0 = T6X_ANACTRL_HDMI_PLL0_CTRL0;
			pll_ctrl1 = T6X_ANACTRL_HDMI_PLL0_CTRL1;
			pll_ctrl2 = T6X_ANACTRL_HDMI_PLL0_CTRL2;
			pll_ctrl3 = T6X_ANACTRL_HDMI_PLL0_CTRL3;
		} else {
			pll_ctrl0 = T6X_ANACTRL_HDMI_PLL1_CTRL0;
			pll_ctrl1 = T6X_ANACTRL_HDMI_PLL1_CTRL1;
			pll_ctrl2 = T6X_ANACTRL_HDMI_PLL1_CTRL2;
			pll_ctrl3 = T6X_ANACTRL_HDMI_PLL1_CTRL3;
		}
	}
	//0505 dbg config give_n
	/* config give_n to 0x2000(8192) */
	hdmirx_wr_cor(H21RXSB_GN2_M42H_IVCRX, (give_n >> 16) & 0xf, port);
	hdmirx_wr_cor(H21RXSB_GN1_M42H_IVCRX, (give_n >> 8) & 0xff, port);
	hdmirx_wr_cor(H21RXSB_GN0_M42H_IVCRX, (give_n >> 0) & 0xff, port);

	//0505 dbg config post_div
	hdmirx_wr_cor(H21RXSB_POSTDIV_M42H_IVCRX, 0x2, port);

	//0505 dbg config reg_n
	/* fpll ctrl0[20:16] */
	switch (f_rate) {
	case FRL_3G_3LANE:
		odn_reg_n_mul = 4;
		break;
	case FRL_6G_3LANE:
	case FRL_6G_4LANE:
		odn_reg_n_mul = 6;
		break;
	case FRL_8G_4LANE:
		odn_reg_n_mul = 8;
		break;
	case FRL_10G_4LANE:
		odn_reg_n_mul = 8;
		break;
	case FRL_12G_4LANE:
		odn_reg_n_mul = 10;
		break;
	default:
		odn_reg_n_mul = 6;
		break;
	}
	hdmirx_wr_cor(H21RXSB_NMUL_M42H_IVCRX, odn_reg_n_mul, port);

	//0505 dbg reset
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 1, port);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 0, port);
	//udelay(100);
	//wait valid_m
	while (cnt < valid_m_wait_max) {
		cnt++;
		reg_valid_m = hdmirx_rd_bits_cor(H21RXSB_STATUS_M42H_IVCRX, _BIT(0), port);

		//rx_pr("valid_m=%d\n", reg_valid_m);
		if (reg_valid_m)
			break;
		udelay(5);
	}
	cnt = 0;
	if (!reg_valid_m) {
		if (log_level & FRL_LOG)
			rx_pr("m not valid!-0x%x\n",
				hdmirx_rd_cor(H21RXSB_STATUS_M42H_IVCRX, port));
		return;
	}
	if (log_level & FRL_LOG)
		rx_pr("port-%d m valid\n", port);
	if (rx_info.chip_id == CHIP_ID_T6X && reg_valid_m &&
		rx_info.aml_phy_21.cable_tuning_en && f_rate == FRL_12G_4LANE) {
		eq_stg2_temp =
			((rx_info.aml_phy_21.eq_stg2 <= 0x7) ? rx_info.aml_phy_21.eq_stg2 : 0x2);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, BUF_BST,
			eq_stg2_temp, port);
		eq_pole_temp =
			((rx_info.aml_phy_21.eq_pole <= 0x7) ? rx_info.aml_phy_21.eq_pole : 0x1);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, LEQ_POLE,
			eq_pole_temp, port);
		rx_pr("cable tuning done\n");
	}
	//or get flclk based on frl_rate!!! todo
	flclk = rx_get_flclk(port);
	if (!flclk) {
		if (log_level & FRL_LOG)
			rx_pr("flclk invalid\n");
		return;
	}
	o_req_m = ((hdmirx_rd_cor(H21RXSB_REQM2_M42H_IVCRX, port) & 0Xf) << 16) |
		(hdmirx_rd_cor(H21RXSB_REQM1_M42H_IVCRX, port) << 8) |
		hdmirx_rd_cor(H21RXSB_REQM0_M42H_IVCRX, port);

	if (reg_valid_m && o_req_m == 0) {
		if (log_level & FRL_LOG)
			rx_pr("port-%d m not really valid\n", port);
		return;
	}
	/* tclk = flclk * o_req_m / give_n / post_div */
	temp1 = (u64)flclk * (u64)o_req_m;
	do_div(temp1, give_n);
	do_div(temp1, 2);
	tclk = (u32)temp1;
	//tclk = flclk * o_req_m / give_n / 2;
	//rx_pr("flclk * o_req_m=%ld\n", flclk * o_req_m);
	//rx_pr("flclk * o_req_m / 8192=%ld\n", flclk * o_req_m / 8192);
	//rx_pr("flclk * o_req_m / 8192 / 2=%ld\n", flclk * o_req_m / 8192 / 2);
	if (log_level & FRL_LOG)
		rx_pr("flclk = %ld, tclk=%ld\n", flclk, tclk);

	/*
	 * 3. get pre_div
	 */
	//min = 800 * MHz / tclk;
	//max = 1600 * MHz / tclk;
	temp = (800 + 1600) * KHz * 10 / 2 / (tclk / KHz);
	if (temp % 10)
		pre_div = (temp / 10) + 1;
	else
		pre_div = (temp / 10);
	if (log_level & FRL_LOG)
		rx_pr("temp=%lu,pre_div=0x%x\n", temp, pre_div);
	/* check vco */
	if (tclk / KHz * pre_div * 2 < 1600 * KHz) {
		pre_div += 1;
		if (log_level & FRL_LOG)
			rx_pr("error,vco<1.6G!\n");
	}
	if (tclk / KHz * pre_div * 2 > 3200 * KHz) {
		pre_div -= 1;
		if (log_level & FRL_LOG)
			rx_pr("error,vco>3.2G!\n");
	}
	/* fpll cntl2[9:4]*/
	if (pre_div >= 50) {
		pre_div = 64;
		hdmirx_wr_cor(H21RXSB_PREDIV_M42H_IVCRX, 0, port);
	} else {
		if (pre_div == 33)
			pre_div = 31;
		if (pre_div == 3)
			pre_div = pre_div - 1;
		hdmirx_wr_cor(H21RXSB_PREDIV_M42H_IVCRX, pre_div, port);
	}

	data = hdmirx_rd_cor(H21RXSB_NMUL_M42H_IVCRX, port);
	temp2 = (u64)data * (u64)tclk * (u64)pre_div * 2;
	do_div(temp2, (u32)flclk);
	data = (u32)temp2;
	data = data & 0x1ff;
	if (log_level & FRL_LOG)
		rx_pr("data:%lu\n", data);
	if (pre_div == 64) {
		rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16) | (4 << 10));
	} else if (pre_div == 2) {
		rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16) | (1 << 10));
	} else {
		rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16));
	}
	if (pre_div == 64) {
		rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16) | (4 << 10));
	} else if (pre_div == 2) {
		rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16) | (1 << 10));
	} else {
		rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
			(odn_reg_n_mul << 16));
	}
	rx_write_fpll(pll_ctrl1, (fpll[2] & 0xfff00000) | 0xb9000);
	if (pre_div < 64) {
		if (pre_div == 2)
			rx_write_fpll(pll_ctrl2, fpll[3] |
			(1 << 4));
		else
			rx_write_fpll(pll_ctrl2, fpll[3] |
				((pre_div & 0x1f) << 4));
	} else {
		rx_write_fpll(pll_ctrl2, fpll[3] |
			(4 << 4));
	}
	rx_write_fpll(pll_ctrl3, fpll[4] | (1 << 7));
	rx_write_fpll(pll_ctrl0, (fpll[5] & 0xfffffe00) | data |
	(odn_reg_n_mul << 16) |
	((pre_div == 64) ? (4 << 10) : ((pre_div == 2) ? (1 << 10) : 0)));
	rx_write_fpll(pll_ctrl3, fpll[6] | (1 << 7) | (1 << 19));
	udelay(10);
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 1, port);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 0, port);
	udelay(100);
	//wait valid_m
	while (cnt < valid_m_wait_max) {
		cnt++;
		reg_valid_m = hdmirx_rd_bits_cor(H21RXSB_STATUS_M42H_IVCRX, _BIT(0), port);

		if (reg_valid_m)
			break;
		udelay(5);
	}
	cnt = 0;
	if (!reg_valid_m) {
		if (log_level & FRL_LOG)
			rx_pr("m not valid!-0x%x\n",
				hdmirx_rd_cor(H21RXSB_STATUS_M42H_IVCRX, port));
		return;
	}
	/*
	 * 4. fpll config
	 */
	do {
		if (pre_div == 64) {
			rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16) | (4 << 10));
		} else if (pre_div == 2) {
			rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16) | (1 << 10));
		} else {
			rx_write_fpll(pll_ctrl0, (fpll[0] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16));
		}
		if (pre_div == 64) {
			rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16) | (4 << 10));
		} else if (pre_div == 2) {
			rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16) | (1 << 10));
		} else {
			rx_write_fpll(pll_ctrl0, (fpll[1] & 0xfffffe00) | data |
				(odn_reg_n_mul << 16));
		}
		rx_write_fpll(pll_ctrl1, fpll[2]);
		if (pre_div < 64) {
			if (pre_div == 2)
				rx_write_fpll(pll_ctrl2, fpll[3] |
				(1 << 4));
			else
				rx_write_fpll(pll_ctrl2, fpll[3] |
					((pre_div & 0x1f) << 4));
		} else {
			rx_write_fpll(pll_ctrl2, fpll[3] |
				(4 << 4));
		}

		rx_write_fpll(pll_ctrl3, fpll[4]);
		rx_write_fpll(pll_ctrl0, fpll[5] | ((odn_reg_n_mul << 16)) |
			((pre_div == 64) ? (6 << 10) : ((pre_div == 2) ? (1 << 10) : 0)));
		rx_write_fpll(pll_ctrl3, fpll[6]);
		udelay(10);
		if (cnt++ > 5) {
			rx_pr("fpll cfg err!\n");
			break;
		}
	} while (!is_fpll_locked(port));
	if (!is_fpll_locked(port) || !rx_get_overlap_sts(port) || !rx_get_clkready_sts(port)) {
		if (log_level & FRL_LOG) {
			rx_dump_reg_d_f_sts(port);
			rx_pr("post_div=%d\n", hdmirx_rd_cor(H21RXSB_POSTDIV_M42H_IVCRX, port));
			rx_pr("pre_div=%d\n", hdmirx_rd_cor(H21RXSB_PREDIV_M42H_IVCRX, port));
			rx_pr("reg_n=%d\n", hdmirx_rd_cor(H21RXSB_NMUL_M42H_IVCRX, port));
			rx_pr("o_req_m=%ld\n", o_req_m);
			rx_pr("vco=%ld\n", tclk * pre_div * 2);
			rx_pr("0x1525=0x%x", hdmirx_rd_cor(0x1525, port));
		}
	} else {
		rx_pr("fpll locked\n");
	}
}

void rx_21_fpll_cfg(int f_rate, u8 port)
{
	rx_21_fpll_calculation(f_rate, port);
}

void rx_21_dump_fpll(u8 port)
{
	if (rx_info.chip_id == CHIP_ID_T3X) {
		if (port == E_PORT2)
			rx_21_dump_fpll_0_t3x();
		else if (port == E_PORT3)
			rx_21_dump_fpll_1_t3x();
	} else {
		if (port == E_PORT2)
			rx_21_dump_fpll_0_t6x();
		else if (port == E_PORT3)
			rx_21_dump_fpll_1_t6x();
	}
}

unsigned long rx_get_flclk(u8 port)
{
	unsigned long clk = 0;

	switch (rx_info.chip_id) {
	case CHIP_ID_T3X:
		if (port == E_PORT2)
			clk = meson_clk_measure_with_precision(9, 32);
		else if (port == E_PORT3)
			clk = meson_clk_measure_with_precision(11, 32);
		break;
	case CHIP_ID_T6X:
		if (port == E_PORT2)
			clk = meson_clk_measure_with_precision(9, 32);
		else if (port == E_PORT3)
			clk = meson_clk_measure_with_precision(13, 32);
		break;
	default:
		break;
	}
	return clk;
}

void rx_dump_reg_d_f_sts(u8 port)
{
	u32 data32;

	data32 = hdmirx_rd_top(TOP_FPLL21_STAT1, port);
	rx_pr("req_f=%d\n", (data32 >> 12) & 0x7fffff);
	rx_pr("req_d=%d\n", data32 & 0x1ff);
}

bool rx_get_overlap_sts(u8 port)
{
	bool ret;

	ret = hdmirx_rd_bits_cor(H21RXSB_INTR2_M42H_IVCRX, _BIT(4), port);
	hdmirx_wr_bits_cor(H21RXSB_INTR2_M42H_IVCRX, _BIT(4), 1, port);
	return ret;
}

bool rx_get_clkready_sts(u8 port)
{
	return hdmirx_rd_bits_cor(H21RXSB_STATUS_M42H_IVCRX, _BIT(1), port);
}

bool rx_get_valid_m_sts(u8 port)
{
	return hdmirx_rd_bits_cor(H21RXSB_STATUS_M42H_IVCRX, _BIT(0), port);
}

bool is_fpll_err(u8 port)
{
	bool ret = true;

	rx_21_fpll_cfg(rx[port].var.frl_rate, port);
	mdelay(1);
	if (rx_get_clkready_sts(port))
		ret = rx_get_overlap_sts(port);
	return ret;
}

unsigned int decimaltogray_frl(unsigned int x)
{
	return x ^ (x >> 1);
}

void rx_8g_retry_cfg(u8 port)
{
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_PH_DIV, 5, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_VAL1, 0x9, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN, 0x1, port);
}

static const u8 eq_stg1_vals[4] = {0x10, 0x12, 0x14, 0x16};
static const u16 vga_gain_vals[4] = {0xAAAA, 0x8888, 0x6666, 0x4444};
static const u8 cdr_pi_ofst_vals[3] = {0x26, 0x28, 0x24};
static const u8 eq_pole_vals[4] = {2, 3, 4, 1};

static void apply_retry_config(u8 port, u16 index)
{
	if (index < 4) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_VAL1,
			eq_stg1_vals[index], port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN,
			0x1, port);
		rx_pr("port %d retry EQ_STG1 = 0x%02x\n", port, eq_stg1_vals[index]);
	} else if (index < 8) {
		if (index == 4) {
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN, 0x0, port);
			rx_pr("port %d restore EQ (EQ_BYP_EN=0)\n", port);
		}
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, VGA_GAIN,
			decimaltogray_frl(vga_gain_vals[index - 4]), port);
		rx_pr("port %d retry VGA_GAIN = 0x%04x\n", port, vga_gain_vals[index - 4]);
	} else if (index < 11) {
		if (index == 8)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, VGA_GAIN,
			decimaltogray_frl(0xaaaa), port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_PI_OFST,
					cdr_pi_ofst_vals[index - 8], port);
		rx_pr("port %d retry CDR_PI_OFST = 0x%02x\n", port, cdr_pi_ofst_vals[index - 8]);
	} else {
		if (index == 11) {
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR,
				CDR_PI_OFST, 0x24, port);
			rx_pr("port %d restore CDR_PI_OFST (if needed)\n", port);
		}
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, LEQ_POLE,
			eq_pole_vals[index - 11], port);
		rx_pr("port %d retry EQ_POLE = %d\n", port, eq_pole_vals[index - 11]);
	}
}

void rx_read_err_cnt(u8 port)
{
	u32 ch0, ch1, ch2, ch3;

	rx_get_error_cnt(&ch0, &ch1, &ch2, &ch3, port);
	if (rx[port].state >= FSM_FRL_FPLL_CFG) {
		if (ch0 || ch1 || ch2 || ch3) {
			rx[port].err_cnt++;
			if (ch0)
				rx[port].err_cnt_ch0++;
			if (ch1)
				rx[port].err_cnt_ch1++;
			if (ch2)
				rx[port].err_cnt_ch2++;
			if (ch3)
				rx[port].err_cnt_ch3++;
			if (log_level & FRL_LOG)
				rx_pr("err_cnt %d\n", rx[port].err_cnt);
			if (rx[port].err_cnt >= err_cnt_max) {
				if (rx[port].var.frl_rate == FRL_8G_4LANE) {
					if (rx[port].err_cnt_ch0 &&
						rx[port].err_cnt_ch1 &&
						rx[port].err_cnt_ch2 &&
						rx[port].err_cnt_ch3) {
						rx_8g_retry_cfg(port);
						rx[port].err_cnt_ch0 = 0;
						rx[port].err_cnt_ch1 = 0;
						rx[port].err_cnt_ch2 = 0;
						rx[port].err_cnt_ch3 = 0;
						rx[port].err_cnt = 0;
					}
				}
			}

			if (rx[port].err_cnt >= err_cnt_max &&
				rx[port].var.frl_rate == FRL_12G_4LANE) {
				if (rx[port].retry_cfg_idx == INVALID_RETRY_INDEX) {
					if (force_low_rate) {
						frl_update_edid_en[port] = 1;
						hdmi_rx_frl_rate_update(port, FRL_8G_4LANE);
						rx[port].state = FSM_HPD_LOW;
						rx[port].retry_cfg_idx = INVALID_RETRY_INDEX;
					} else {
						rx[port].retry_cfg_idx = 0;
						apply_retry_config(port, 0);
					}
				} else if (rx[port].retry_cfg_idx < NUM_RETRY_CONFIGS - 1) {
					rx[port].retry_cfg_idx++;
					apply_retry_config(port, rx[port].retry_cfg_idx);
				} else {
					hdmi_rx_frl_rate_update(port, FRL_8G_4LANE);
					rx[port].state = FSM_HPD_LOW;
					rx[port].retry_cfg_idx = INVALID_RETRY_INDEX;
				}
				rx[port].err_cnt = 0;
				rx[port].err_cnt_ch0 = 0;
				rx[port].err_cnt_ch1 = 0;
				rx[port].err_cnt_ch2 = 0;
				rx[port].err_cnt_ch3 = 0;
			}
			if (ecc_err_monitor) {
				rx_pr("port %d err cnt:%d,%d,%d,%d\n", port, ch0, ch1, ch2, ch3);
				rx_pr("ced update flag 0x%x\n",
				hdmirx_rd_cor(SCDCS_UPD_FLAGS_SCDC_IVCRX, port));
			}
		}
	}
}

void clr_frl_fifo_status(u8 port)
{
	u32 hdmirx_top_intr_stat;

	hdmirx_wr_cor(VP_FDET_IRQ_STATUS_VID_IVCRX, 0xff, port);
	udelay(1);
	hdmirx_wr_cor(VP_FDET_IRQ_STATUS_VID_IVCRX + 1, 0xff, port);
	udelay(1);
	hdmirx_wr_cor(VP_FDET_IRQ_STATUS_VID_IVCRX + 2, 0xff, port);
	hdmirx_top_intr_stat = hdmirx_rd_top(TOP_INTR_STAT, port);
	hdmirx_wr_top(TOP_INTR_STAT_CLR, hdmirx_top_intr_stat, port);
}

void rx_rcc_err_frl_config(u8 port)
{
	hdmirx_wr_bits_cor(DPLL_CTRL0_DPLL_IVCRX, MSK(3, 0), 0x7, port);
	hdmirx_wr_bits_cor(DPLL_CTRL0_DPLL_IVCRX, MSK(3, 0), 0x0, port);
	hdmirx_wr_cor(SCDCS_1S_IN_20MS_CNT_SCDC_IVCRX, err_cnt, port);
}

void rx_switch_to_self_hsync(u8 port, bool en)
{
	hdmirx_wr_bits_cor(RX_PWD_CTRL_PWD_IVCRX, _BIT(3), en, port);
	hdmirx_wr_bits_cor(HSYNC_GEN_EVEN_CTRL1_PWD_IVCRX, _BIT(5), en, port);
	hdmirx_wr_bits_cor(HSYNC_GEN_EVEN_CTRL1_PWD_IVCRX, _BIT(7), en, port);
	hdmirx_wr_cor(HSYNC_GEN_EVEN_CTRL2_PWD_IVCRX, 2, port);
	hdmirx_wr_bits_cor(HSYNC_GEN_ODD_CTRL1_PWD_IVCRX, _BIT(5), en, port);
	hdmirx_wr_bits_cor(HSYNC_GEN_ODD_CTRL1_PWD_IVCRX, _BIT(7), en, port);
	hdmirx_wr_cor(HSYNC_GEN_ODD_CTRL2_PWD_IVCRX, 2, port);
	hdmirx_wr_cor(HSYNC_GEN_CTRL0, 0x0, port);
	hdmirx_wr_cor(HSYNC_GEN_CTRL0, 0x1, port);
	if (port == rx_info.main_port)
		hdmirx_wr_bits_top_common_1(TOP_VID_CNTL2, _BIT(31), en);
}

bool rx_is_switch_to_analog_clk(u8 port)
{
	//to do, now only 4k120 100 yuv 420 has this problem.
	if (rx[port].var.frl_rate)
		return true;
	else
		return false;
}

void rx_switch_to_analog_clk(u8 port)
{
	hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(0), 1, port);
	if (port == E_PORT2) {
		if (rx[port].pre.colorspace == E_COLOR_YUV422 ||
			rx[port].dsc_flag) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x0);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x0);
		} else if (rx[port].pre.colordepth == E_COLORDEPTH_12) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x2);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x2);
		} else if (rx[port].pre.colordepth == E_COLORDEPTH_10) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x1);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x1);
		} else {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x0);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0,
				MSK(3, 13), 0x0);
		}
	} else {
		if (rx[port].pre.colorspace == E_COLOR_YUV422 ||
			rx[port].dsc_flag) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x0);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x0);
		} else if (rx[port].pre.colordepth == E_COLORDEPTH_12) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x2);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x2);
		} else if (rx[port].pre.colordepth == E_COLORDEPTH_10) {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x1);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x1);
		} else {
			if (rx_info.chip_id == CHIP_ID_T3X)
				hdmirx_wr_bits_clk_ctl(T3X_CLKCTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x0);
			else
				wr_bits_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0,
				MSK(3, 13), 0x0);
		}
	}
}

void rx_clr_f_det(bool en, u8 port)
{
	hdmirx_wr_bits_cor(VP_FDET_CLEAR_VID_IVCRX, _BIT(0), en, port);
	hdmirx_wr_cor(RX_DEPACK2_INTR0_DP0B_IVCRX, 0xff, port);
}

void rx_valid_m_reset(u8 port)
{
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 1, port);
	hdmirx_wr_bits_cor(RX_PWD_SRST2_PWD_IVCRX, _BIT(7), 0, port);
}

void rx_set_dsc_hdmi_cntl(unsigned int val)
{
	hdmirx_wr_top_common(TOP_DSC_HDMI_CNTL, val);
}

void set_dsc_clk_cntl(int clk_select, int clk)
{
	if (rx_info.chip_id != CHIP_ID_T6X)
		return;
	if (clk_select == H_DSC_PIX_PLL) {
		if (clk > VPU_OVER_SPEED * MHz) {
			over_clk_flag |= _BIT(0);
			schedule_work(&vpu_dwork);
		}
	} else if (clk_select == H_HDMI) {
		if (clk > VPU_OVER_SPEED1 * MHz) {
			over_clk_flag |= _BIT(1);
			schedule_work(&vpu_dwork);
		}
	}
	if (clk_select == H_VPU_CLK_DIV_2)
		wr_reg_clk_ctl(CLKCTRL_DSC_CLK_CTRL, 0x343);
	else if (clk_select == H_FPLL_DIV3)
		wr_reg_clk_ctl(CLKCTRL_DSC_CLK_CTRL, 0x140);
	else if (clk_select == H_DSC_PIX_PLL)
		wr_reg_clk_ctl(CLKCTRL_DSC_CLK_CTRL, 0x1c0);
	else
		wr_reg_clk_ctl(CLKCTRL_DSC_CLK_CTRL, 0x2c0);
}

