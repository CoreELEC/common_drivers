/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_COMMON_H_
#define __DPTX_COMMON_H_

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/math64.h>

/* 20240704: lcd tcon support user info */
#define DPTX_DRV_VERSION    "20250820"

//extern unsigned long clk_util_clk_msr(unsigned long clk_mux);

static inline u32 count_bit(u32 n)
{
	u32 c = 0;

	for (; n; ++c)
		n &= n - 1;
	return c;
}

static inline unsigned long long dptx_div(unsigned long long num, u32 den)
{
	unsigned long long ret;

	ret = div_u64(num, den);
	return ret;
}

static inline unsigned long long dptx_div_around(unsigned long long num, u32 den)
{
	unsigned long long ret;

	num = num + (den >> 1);
	ret = div_u64(num, den);
	return ret;
}

#define dptx_diff(a, b) (((a) >= (b)) ? ((a) - (b)) : ((b) - (a)))

#define CAP_COMP(X, Y) ({typeof(X) x_ = (X); typeof(Y) y_ = (Y); (x_ < y_) ? x_ : y_; })

enum eDP_link_rate_e {
	eDP_LINK_RATE_RBR    = 0x06,
	eDP_LINK_RATE_2P16G  = 0x08,
	eDP_LINK_RATE_HBR    = 0x0a,
	eDP_LINK_RATE_3P24G  = 0x0c,
	eDP_LINK_RATE_4P32G  = 0x0f,
	eDP_LINK_RATE_HBR2   = 0x14,
	eDP_LINK_RATE_HBR3   = 0x1e,

	eDP_LINK_RATE_UBR10  = 0x1,
	eDP_LINK_RATE_UBR135 = 0x2,
	eDP_LINK_RATE_UBR20  = 0x3,
	eDP_LINK_RATE_INVALID = 0,
};

#define eDPTX_AUX_CMD_WRITE            0x8
#define eDPTX_AUX_CMD_READ             0x9
#define eDPTX_AUX_CMD_I2C_WRITE        0x0
#define eDPTX_AUX_CMD_I2C_WRITE_MOT    0x4
#define eDPTX_AUX_CMD_I2C_READ         0x1
#define eDPTX_AUX_CMD_I2C_READ_MOT     0x5
#define eDPTX_AUX_CMD_I2C_WRITE_STATUS 0x2

extern struct edptx_boot_ctrl_s dptx_uboot_configs[eDPTX_MAX_DRV];

struct edptx_aux_req_s {
	u32 address; //20bit for DPCD and 8 bit for I2C
	u8 cmd_code;
	u8 byte_cnt; //up to 16 word
	u8 *data;
};

/* VENC */
unsigned int dptx_get_encl_line_cnt(struct edptx_drv_s *edp);
unsigned int dptx_get_max_line_cnt(struct edptx_drv_s *edp);
void dptx_debug_test(struct edptx_drv_s *edp, u8 num);
void dptx_set_venc_timing(struct edptx_drv_s *edp);
void dptx_set_venc(struct edptx_drv_s *edp);
void edptx_venc_enable(struct edptx_drv_s *edp, u8 flag);
void edptx_mute_set(struct edptx_drv_s *edp, u8 flag);
int edptx_venc_probe(struct edptx_drv_s *edp);

/* ANALOG PHY */
void edptx_phy_enable(struct edptx_drv_s *edp, u8 port);
void edptx_phy_disable(struct edptx_drv_s *edp, u8 port);
void edptx_phy_set_lane(struct edptx_drv_s *edp, u8 port, u8 lane_mask);
void edptx_phy_probe(struct edptx_drv_s *edp);

/* CLK */
void dptx_clk_config_print(struct edptx_drv_s *edp);
void edptx_clk_set_link_clk(struct edptx_drv_s *edp, u8 port, u8 dptx_link_rate);
void dptx_clk_set_vid_clk(struct edptx_drv_s *edp, u32 pixel_clk);
void dptx_clk_set_ssc(struct edptx_drv_s *edp, u8 status);
void edptx_clk_config_probe(struct edptx_drv_s *edp);
void dptx_clk_init(void);

/* IP-interface */
u8 edptx_if_aux_write(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf);
u8 edptx_if_aux_write_single(struct edptx_drv_s *edp, u8 port, u32 addr, u8 val);
u8 edptx_if_aux_read(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf);
u8 edptx_if_aux_i2c_op(struct edptx_drv_s *edp, u8 port,
					u8 cmd_type, u32 dev_addr, u8 len, u8 *data);
u8 edptx_if_aux_read_edid(struct edptx_drv_s *edp, u8 port, u8 *data, u16 len);
void edptx_if_transmit_pattern(struct edptx_drv_s *edp, u8 port, u8 pattern, u8 lane,
	u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2);
void edptx_if_set_MSA(struct edptx_drv_s *edp, u8 port);
#define DPTX_RESET_COMBO_DPHY          BIT(0)
#define DPTX_RESET_eDP_PIPE            BIT(1)
#define DPTX_RESET_eDP_CTRL            BIT(2)
#define DPTX_RESET_AUX_CLK_DIVIDER     BIT(3)
#define DPTX_RESET_PHY                 BIT(4)
#define DPTX_RESET_VENC                BIT(5)
#define DPTX_RESET_ALL                 0xff
void edptx_if_path_reset(struct edptx_drv_s *edp, u8 port, u8 mask);
void edptx_if_set_lane_cfg(struct edptx_drv_s *edp, u8 port);
void edptx_if_set_phy_cfg(struct edptx_drv_s *edp, u8 port, u8 lane_mask);
void edptx_if_transmitter_init(struct edptx_drv_s *edp, u8 port);

#define DPTX_IP_TRANSMITTER_OUTPUT_OFF              0
#define DPTX_IP_TRANSMITTER_OUTPUT_MAIN_STREAM_OFF  1
#define DPTX_IP_TRANSMITTER_OUTPUT_ON               2
void edptx_if_transmitter_output(struct edptx_drv_s *edp, u8 port, u8 en);
u8 edptx_if_get_hpd_level(struct edptx_drv_s *edp, u8 port);
u16 edptx_if_get_hpd_irq(struct edptx_drv_s *edp, u8 port);

#define DPTX_IRQ_REPLY_TIMEOUT_MASK    BIT(3)
#define DPTX_IRQ_REPLY_RECEIVED_MASK   BIT(2)
#define DPTX_IRQ_HPD_EVENT_MASK        BIT(1)
#define DPTX_IRQ_HPD_IRQ_EVENT         BIT(0)
void edptx_if_set_hpd_interrupt_mask(struct edptx_drv_s *edp, u8 port, u8 mask);

#define DPTX_SCRAMBLE_RESET_OFF              0
#define DPTX_SCRAMBLE_RESET_ON               1
#define DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET  2
void edptx_if_scramble_reset_set(struct edptx_drv_s *edp, u8 port, u8 sr_type);
void edptx_if_PSR1_ctrl(struct edptx_drv_s *edp, u8 port, u8 flag);
void edptx_if_PSR2_ctrl(struct edptx_drv_s *edp, u8 port, u8 flag);
void edptx_if_reg_store(struct edptx_drv_s *edp, u8 port, u32 d0, u32 d1);
void edptx_if_reg_store_get(struct edptx_drv_s *edp, u8 port, u32 *d0, u32 *d1);
void edptx_if_link_get(struct edptx_drv_s *edp, u8 port, u32 *link_rate, u8 *lane);

void edptx_if_IP_probe(struct edptx_drv_s *edp);
/* IP-interface END */

/* dptx_link_training.c */
struct DPTX_test_pat_s {
	char *name;
	u8 data;
	bool SR_disable, encoding_en;
};

extern struct DPTX_test_pat_s DP_test_pat[];
#define DPTX_LINK_TRAINING_AUTO      0
#define DPTX_FAST_LINK_TRAINING      1
#define DPTX_FULL_LINK_TRAINING      2
int edptx_link_training(struct edptx_drv_s *edp);
int edptx_full_lt(struct edptx_drv_s *edp);
int edptx_fast_lt(struct edptx_drv_s *edp);

/* dptx_EDID_DisplayID.c */
void dptx_edid_print_raw(unsigned char *_buf);  //?
void dptx_edid_print_parsed(struct edptx_drv_s *edp, u8 port); //?
int edptx_EDID_probe(struct edptx_drv_s *edp, u8 check_crc);
// int dptx_EDID_load_dts_probe(struct edptx_drv_s *edp);

/* dptx_vmode_mgr.c */
u32 edptx_print_vmode(struct edptx_drv_s *edp, char *c_buf, u8 print_flag);
void edptx_vmode_manage(struct edptx_drv_s *edp);

/* dptx_utils.c */
extern u16 dptx_train_rd_intv[5];
extern u16 dptx_PSR_setup_time[8];
void edptx_delay_us(int us);
void edptx_delay_ms(int ms);
u8 dptx_vswing_ds_to_phy(struct edptx_drv_s *edp, u8 ds_level);
u8 dptx_preem_ds_to_phy(struct edptx_drv_s *edp, u8 ds_level);
u8 ds_to_DPCD_LANESET(u8 ds_level);
u8 dptx_ds_to_vswing(u8 ds);
u8 dptx_ds_to_preem(u8 ds);
u8 dptx_v_p_to_ds(u8 vsw, u8 preem);
void dptx_link_cfg_dft(struct edptx_drv_s *edp, u8 port);
u8 edptx_DPCD_capability_to_link_cfg(struct edptx_drv_s *edp, u8 port);
void dptx_link_policy_maker(struct edptx_drv_s *edp, u8 port);
u8 dptx_vid_band_width_check(struct edptx_drv_s *edp, u32 pclk, u8 bpp);

int dptx_connector_check(struct edptx_drv_s *edp);
int dptx_outputmode_check(struct edptx_drv_s *edp, char *mode);

void edptx_set_phy_config(struct edptx_drv_s *edp, u8 port, u8 use_preset);
void edptx_set_lane_config(struct edptx_drv_s *edp, u8 port);
void edptx_setup_PSR1(struct edptx_drv_s *edp, u8 port, u8 flag);
void edptx_setup_PSR2(struct edptx_drv_s *edp, u8 port, u8 flag);

/* ************* DPTX VMODE related ************/
struct dptx_vmode_s *dptx_get_vmode(struct edptx_drv_s *edp, u8 th);
struct dptx_vmode_s *dptx_get_optimum_vmode(struct edptx_drv_s *edp);
void edptx_vmode_apply_to_act_timing(struct edptx_drv_s *edp, struct dptx_vmode_s *vmd);
void edptx_act_timing_apply(struct edptx_drv_s *edp);
void edptx_list_support_vmode(struct edptx_drv_s *edp);
void edptx_user_set_vmode(struct edptx_drv_s *edp, u8 vmd_idx);
u8 edptx_vmode_str_check(struct edptx_drv_s *edp, char *vmd_str);

int edptx_timing_config_restore(struct edptx_drv_s *edp);

extern struct edptx_detail_timing_s DPTX_SafeMode_640x480_timing;
extern struct dptx_vmode_s DPTX_SafeMode_640x480_vmode;

void edptx_pinmux_set(struct edptx_drv_s *edp, u8 status);
void edptx_gpios_get(struct edptx_drv_s *edp, u8 status);
void edptx_HPD_trigger_set(struct edptx_drv_s *edp, u8 en);

void edptx_act_timing_to_venc_config(struct edptx_drv_s *edp);

u8 __str_add_vmode(struct edptx_drv_s *edp, char *buf, struct dptx_vmode_s *vmd_p, u8 fr);

#define DPTX_PHY_STA_VSW_0_PREEM_0     0
#define DPTX_PHY_STA_VSW_0_PREEM_1     1
#define DPTX_PHY_STA_VSW_0_PREEM_2     2
#define DPTX_PHY_STA_VSW_0_PREEM_3     3
#define DPTX_PHY_STA_VSW_1_PREEM_0     4
#define DPTX_PHY_STA_VSW_1_PREEM_1     5
#define DPTX_PHY_STA_VSW_1_PREEM_2     6
#define DPTX_PHY_STA_VSW_2_PREEM_0     7
#define DPTX_PHY_STA_VSW_2_PREEM_1     8
#define DPTX_PHY_STA_VSW_3_PREEM_0     9
#define DPTX_PHY_STA_DISABLE           0xff

void edptx_vout_notify_mode_change_pre(struct edptx_drv_s *edp);
void edptx_vout_notify_mode_change(struct edptx_drv_s *edp);

////! Content Protect
void edptx_set_content_protection(struct edptx_drv_s *edp, u8 port);

void edptx_drv_check_HPD(struct edptx_drv_s *edp);
void edptx_driver_ready(struct edptx_drv_s *edp);
void edptx_driver_panel_power_ctrl(struct edptx_drv_s *edp, u8 en);
void edptx_drv_start(struct edptx_drv_s *edp);
void edptx_drv_disp_on(struct edptx_drv_s *edp);
void edptx_drv_disp_off(struct edptx_drv_s *edp);
void edptx_driver_close(struct edptx_drv_s *edp);
void edptx_drv_PSR1_en(struct edptx_drv_s *edp, u8 port_mask, u8 en);
void edptx_drv_PSR2_en(struct edptx_drv_s *edp, u8 port_mask, u8 en);

//void dptx_info_print(struct edptx_drv_s *edp);
//void dptx_reg_print(struct edptx_drv_s *edp);

//dptx_debug.c
int edptx_debug_probe(struct edptx_drv_s *edp);
int edptx_debug_remove(struct edptx_drv_s *edp);
void edptx_debug_reset(struct edptx_drv_s *edp, u8 port_mask, u8 reset_part);

//dptx_notify.c
int edptx_notifier_init(void);
void edptx_notifier_remove(void);
int edptx_notifier_call_chain(unsigned long event, void *v);

//dptx_drv.c -- notify call
void edptx_notify_check_HPD(struct edptx_drv_s *edp);
void edptx_notify_set_backlight(struct edptx_drv_s *edp, u8 en);
void edptx_notify_set_screen_mute(struct edptx_drv_s *edp, u8 en);
void edptx_notify_set_link(struct edptx_drv_s *edp, u8 en);
void edptx_notify_set_driver_ready(struct edptx_drv_s *edp, u8 en);
void edptx_notify_set_venc(struct edptx_drv_s *edp, u8 en);
void edptx_notify_set_HPD_trigger(struct edptx_drv_s *edp, u8 en);

//dptx_vout_server.c
void edptx_vout_server_init(struct edptx_drv_s *edp);
void edptx_vout_server_deinit(struct edptx_drv_s *edp);

//dptx_drm.c
int edptx_drm_add(struct device *dev);
void edptx_drm_remove(struct device *dev);

void edptx_bl_wrapper_regist(struct edptx_drv_s *edp);

#endif
