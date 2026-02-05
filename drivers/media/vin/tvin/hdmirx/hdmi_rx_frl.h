/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_RX_FRL_H__
#define __HDMI_RX_FRL_H__

#define H_VPU_CLK_DIV_2 1
#define H_FPLL_DIV3 2
#define H_DSC_PIX_PLL 3
#define H_HDMI 4
#define FRL_CH_LOCK 0xe
#define SCDC_FRL_RATE_ADDR 0x31

void rx_lts_2_flt_ready(u8 port);
int rx_lts_p_syn_detect(u8 frl_rate, u8 port);
void rx_lts_p_frl_start(u8 port);
void hdmi_tx_rx_frl_training_main(u8 port);
void rx_set_frl_train_sts(enum frl_train_sts_e sts, u8 port);
bool is_frl_train_finished(u8 port);
void rx_frl_train_handler(struct kthread_work *work);
void rx_frl_train_handler_1(struct kthread_work *work);
void rx_frl_train(u8 port);
void hdmirx_frl_config(u8 port);
void rx_21_fpll_cfg(int f_rate, u8 port);
void rx_21_dump_fpll(u8 port);
unsigned long rx_get_flclk(u8 port);
void rx_dump_reg_d_f_sts(u8 port);
bool rx_get_clkready_sts(u8 port);
bool rx_get_valid_m_sts(u8 port);
bool is_fpll_err(u8 port);
void rx_read_ecc_err(u8 port);
void clr_frl_fifo_status(u8 port);
void rx_rcc_err_frl_config(u8 port);
void rx_switch_to_self_hsync(u8 port, bool en);
bool rx_is_switch_to_analog_clk(u8 port);
void rx_switch_to_analog_clk(u8 port);
void rx_clr_f_det(bool en, u8 port);
bool rx_get_overlap_sts(u8 port);
void rx_valid_m_reset(u8 port);

#endif
