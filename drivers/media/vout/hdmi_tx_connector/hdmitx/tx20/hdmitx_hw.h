/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX20_HW_H
#define __HDMI_TX20_HW_H

#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/miscdevice.h>

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include <drm/amlogic/meson_connector_dev.h>

#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_hw_common.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>

#include "hdmi_tx_connector/hdmitx_log.h"
#include "hdmitx_ddc.h"
#include "hdmitx_reg.h"

struct hdmitx20_hw {
	struct hdmitx_hw_common *base;
};

struct hdmitx20_dev {
	struct hdmitx_common tx_comm;
	struct hdmitx_hw_common hw_comm;

	struct hdmitx20_hw tx20_hw;
	unsigned char hdcp_max_exceed_state;
	unsigned int hdcp_max_exceed_cnt;
	unsigned int max_exceed;
	struct timer_list hdcp_timer;

	int hdcp_hpd_stick;	/* 1 not init & reset at plugout */
	struct delayed_work work_do_hdcp;
};

struct hdmitx20_hw *get_hdmitx20_hw_instance(void);
struct hdmitx20_dev *get_hdmitx20_device(void);

int hdmitx_hdcp_opr(unsigned int val);

unsigned int hdmitx_rd_reg(unsigned int addr);
int hdmitx_hpd_hw_op(enum hpd_op cmd);

int hdmitx_hpd_hw_op_gxbb(enum hpd_op cmd);
int read_hpd_gpio_gxbb(void);
int hdmitx_ddc_hw_op_gxbb(enum ddc_op cmd);

int hdmitx_hpd_hw_op_gxtvbb(enum hpd_op cmd);
int read_hpd_gpio_gxtvbb(void);
int hdmitx_ddc_hw_op_gxtvbb(enum ddc_op cmd);

int hdmitx_hpd_hw_op_gxl(enum hpd_op cmd);
int read_hpd_gpio_gxl(void);
int hdmitx_ddc_hw_op_gxl(enum ddc_op cmd);
void set_gxl_hpll_clk_out(unsigned int frac_rate, unsigned int clk);
void set_hpll_sspll_gxl(enum hdmi_vic vic);
void set_hpll_sspll_g12a(enum hdmi_vic vic);

void set_hpll_od1_gxl(unsigned int div);
void set_hpll_od2_gxl(unsigned int div);
void set_hpll_od3_gxl(unsigned int div);

void set_g12a_hpll_clk_out(unsigned int frac_rate, unsigned int clk);
void set_hpll_od1_g12a(unsigned int div);
void set_hpll_od2_g12a(unsigned int div);
void set_hpll_od3_g12a(unsigned int div);
int hdmitx_hpd_hw_op_g12a(enum hpd_op cmd);
int hdmitx_ddc_hw_op_g12a(enum ddc_op cmd);

int read_hpd_gpio_txlx(void);
int hdmitx_hpd_hw_op_txlx(enum hpd_op cmd);
int hdmitx_ddc_hw_op_txlx(enum ddc_op cmd);
unsigned int hdmitx_get_format_txlx(void);
void hdmitx_set_format_txlx(unsigned int val);
void hdmitx_sys_reset_txlx(void);

void hdmitx_phy_band_gap_en_tm2(void);
void hdmitx_phy_band_gap_en_g12(void);
void hdmitx_phy_band_gap_en_sc2(void);

void set_phy_by_mode_g12(unsigned int mode);
void set_phy_by_mode_tm2(unsigned int mode);
void set_phy_by_mode_sm1(unsigned int mode);
void set_phy_by_mode_sc2(unsigned int mode);
void set_phy_by_mode_gxl(unsigned int mode);
void set_phy_by_mode_gxbb(unsigned int mode);

void hdmitx_sys_reset_sc2(void);

void set_sc2_hpll_clk_out(unsigned int frac_rate, unsigned int clk);
void set_hpll_od1_sc2(unsigned int div);
void set_hpll_od2_sc2(unsigned int div);
void set_hpll_od3_sc2(unsigned int div);

void set_hpll_sspll_sc2(enum hdmi_vic vic);

void set_vmode_enc_hw(struct hdmitx20_dev *hdev);
#endif
