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

enum DP_link_rate_e {
	DP_LINK_RATE_RBR    = 0x06,
	DP_LINK_RATE_2P16G  = 0x08,
	DP_LINK_RATE_HBR    = 0x0a,
	DP_LINK_RATE_3P24G  = 0x0c,
	DP_LINK_RATE_4P32G  = 0x0f,
	DP_LINK_RATE_HBR2   = 0x14,
	DP_LINK_RATE_HBR3   = 0x1e,

	DP_LINK_RATE_UBR10  = 0x1,
	DP_LINK_RATE_UBR135 = 0x2,
	DP_LINK_RATE_UBR20  = 0x3,
	DP_LINK_RATE_INVALID = 0,
};

#define DPTX_AUX_CMD_WRITE            0x8
#define DPTX_AUX_CMD_READ             0x9
#define DPTX_AUX_CMD_I2C_WRITE        0x0
#define DPTX_AUX_CMD_I2C_WRITE_MOT    0x4
#define DPTX_AUX_CMD_I2C_READ         0x1
#define DPTX_AUX_CMD_I2C_READ_MOT     0x5
#define DPTX_AUX_CMD_I2C_WRITE_STATUS 0x2

extern struct dptx_boot_ctrl_s dptx_uboot_configs[eDPTX_MAX_DRV];

struct dptx_aux_req_s {
	u32 address; //20bit for DPCD and 8 bit for I2C
	u8 cmd_code;
	u8 byte_cnt; //up to 16 word
	u8 *data;
};

/* VENC */
unsigned int dptx_get_encl_line_cnt(struct dptx_drv_s *dptx);
unsigned int dptx_get_max_line_cnt(struct dptx_drv_s *dptx);
void dptx_debug_test(struct dptx_drv_s *dptx, u8 num);
void dptx_set_venc_timing(struct dptx_drv_s *dptx);
void dptx_set_venc(struct dptx_drv_s *dptx);
void dptx_venc_enable(struct dptx_drv_s *dptx, u8 flag);
void dptx_mute_set(struct dptx_drv_s *dptx, u8 flag);
int dptx_venc_probe(struct dptx_drv_s *dptx);

/* ANALOG PHY */
void dptx_phy_enable(struct dptx_drv_s *dptx, u8 port);
void dptx_phy_disable(struct dptx_drv_s *dptx, u8 port);
void dptx_phy_set_lane(struct dptx_drv_s *dptx, u8 port, u8 lane_mask);
void dptx_phy_probe(struct dptx_drv_s *dptx);

/* CLK */
void dptx_clk_config_print(struct dptx_drv_s *dptx);
void dptx_clk_set_link_clk(struct dptx_drv_s *dptx, u8 port, u8 dptx_link_rate);
void dptx_clk_set_vid_clk(struct dptx_drv_s *dptx, u32 pixel_clk);
void dptx_clk_set_ssc(struct dptx_drv_s *dptx, u8 status);
void dptx_clk_config_probe(struct dptx_drv_s *dptx);
void dptx_clk_init(void);

/* IP-interface */
u8 dptx_if_aux_write(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf);
u8 dptx_if_aux_write_single(struct dptx_drv_s *dptx, u8 port, u32 addr, u8 val);
u8 dptx_if_aux_read(struct dptx_drv_s *dptx, u8 port, u32 addr, int len, u8 *buf);
u8 dptx_if_aux_i2c_op(struct dptx_drv_s *dptx, u8 port,
					u8 cmd_type, u32 dev_addr, u8 len, u8 *data);
u8 dptx_if_aux_read_edid(struct dptx_drv_s *dptx, u8 port, u8 *data, u16 len);
void dptx_if_transmit_pattern(struct dptx_drv_s *dptx, u8 port, u8 pattern, u8 lane,
	u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2);
void dptx_if_set_MSA(struct dptx_drv_s *dptx, u8 port);
#define DPTX_RESET_COMBO_DPHY          BIT(0)
#define DPTX_RESET_eDP_PIPE            BIT(1)
#define DPTX_RESET_eDP_CTRL            BIT(2)
#define DPTX_RESET_AUX_CLK_DIVIDER     BIT(3)
#define DPTX_RESET_PHY                 BIT(4)
#define DPTX_RESET_VENC                BIT(5)
#define DPTX_RESET_ALL                 0xff
void dptx_if_path_reset(struct dptx_drv_s *dptx, u8 port, u8 mask);
void dptx_if_set_lane_cfg(struct dptx_drv_s *dptx, u8 port);
void dptx_if_set_phy_cfg(struct dptx_drv_s *dptx, u8 port, u8 lane_mask);
void dptx_if_transmitter_init(struct dptx_drv_s *dptx, u8 port);

#define DPTX_IP_TRANSMITTER_OUTPUT_OFF              0
#define DPTX_IP_TRANSMITTER_OUTPUT_MAIN_STREAM_OFF  1
#define DPTX_IP_TRANSMITTER_OUTPUT_ON               2
void dptx_if_transmitter_output(struct dptx_drv_s *dptx, u8 port, u8 en);
u8 dptx_if_get_hpd_level(struct dptx_drv_s *dptx, u8 port);
u16 dptx_if_get_hpd_irq(struct dptx_drv_s *dptx, u8 port);

#define DPTX_IRQ_REPLY_TIMEOUT_MASK    BIT(3)
#define DPTX_IRQ_REPLY_RECEIVED_MASK   BIT(2)
#define DPTX_IRQ_HPD_EVENT_MASK        BIT(1)
#define DPTX_IRQ_HPD_IRQ_EVENT         BIT(0)
void dptx_if_set_hpd_interrupt_mask(struct dptx_drv_s *dptx, u8 port, u8 mask);

#define DPTX_SCRAMBLE_RESET_OFF              0
#define DPTX_SCRAMBLE_RESET_ON               1
#define DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET  2
void dptx_if_scramble_reset_set(struct dptx_drv_s *dptx, u8 port, u8 sr_type);
void dptx_if_PSR1_ctrl(struct dptx_drv_s *dptx, u8 port, u8 flag);
void dptx_if_PSR2_ctrl(struct dptx_drv_s *dptx, u8 port, u8 flag);
void dptx_if_reg_store(struct dptx_drv_s *dptx, u8 port, u32 d0, u32 d1);
void dptx_if_reg_store_get(struct dptx_drv_s *dptx, u8 port, u32 *d0, u32 *d1);
void dptx_if_link_get(struct dptx_drv_s *dptx, u8 port, u32 *link_rate, u8 *lane);

void dptx_if_IP_probe(struct dptx_drv_s *dptx);
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
int __dptx_link_training(struct dptx_drv_s *dptx);
int __dptx_full_link_training(struct dptx_drv_s *dptx);
int __dptx_fast_link_training(struct dptx_drv_s *dptx);

/* dptx_EDID_DisplayID.c */
void dptx_edid_print_raw(unsigned char *_buf);  //?
void dptx_edid_print_parsed(struct dptx_drv_s *dptx, u8 port); //?
int edptx_EDID_probe(struct dptx_drv_s *dptx, u8 check_crc);
// int dptx_EDID_load_dts_probe(struct dptx_drv_s *dptx);

/* dptx_vmode_mgr.c */
u32 dptx_print_vmode(struct dptx_drv_s *dptx, char *c_buf, u8 print_flag);
void dptx_vmode_manage(struct dptx_drv_s *dptx);

/* dptx_utils.c */
extern u16 dptx_train_rd_intv[5];
extern u16 dptx_PSR_setup_time[8];
void dptx_delay_us(int us);
void dptx_delay_ms(int ms);
u8 dptx_vswing_ds_to_phy(struct dptx_drv_s *dptx, u8 ds_level);
u8 dptx_preem_ds_to_phy(struct dptx_drv_s *dptx, u8 ds_level);
u8 ds_to_DPCD_LANESET(u8 ds_level);
u8 dptx_ds_to_vswing(u8 ds);
u8 dptx_ds_to_preem(u8 ds);
u8 dptx_v_p_to_ds(u8 vsw, u8 preem);
void dptx_link_cfg_dft(struct dptx_drv_s *dptx, u8 port);
u8 dptx_DPCD_capability_to_link_cfg(struct dptx_drv_s *dptx, u8 port);
void dptx_link_policy_maker(struct dptx_drv_s *dptx, u8 port);
u8 dptx_vid_band_width_check(struct dptx_drv_s *dptx, u32 pclk, u8 bpp);

int dptx_connector_check(struct dptx_drv_s *dptx);
int dptx_outputmode_check(struct dptx_drv_s *dptx, char *mode);

void __dptx_set_phy_config(struct dptx_drv_s *dptx, u8 port, u8 use_preset);
void edptx_set_lane_config(struct dptx_drv_s *dptx, u8 port);
void dptx_eDP_PSR1(struct dptx_drv_s *dptx, u8 port, u8 flag);
void dptx_eDP_PSR2(struct dptx_drv_s *dptx, u8 port, u8 flag);

/* ************* DPTX VMODE related ************/
struct dptx_vmode_s *dptx_get_vmode(struct dptx_drv_s *dptx, u8 th);
struct dptx_vmode_s *dptx_get_optimum_vmode(struct dptx_drv_s *dptx);
void dptx_vmode_apply_to_act_timing(struct dptx_drv_s *dptx, struct dptx_vmode_s *vmd);
void dptx_act_timing_apply(struct dptx_drv_s *dptx);
void dptx_list_support_vmode(struct dptx_drv_s *dptx);
void dptx_user_set_vmode(struct dptx_drv_s *dptx, u8 vmd_idx);
u8 dptx_vmode_str_check(struct dptx_drv_s *dptx, char *vmd_str);

int dptx_timing_config_restore(struct dptx_drv_s *dptx);

extern struct dptx_detail_timing_s DPTX_SafeMode_640x480_timing;
extern struct dptx_vmode_s DPTX_SafeMode_640x480_vmode;

void dptx_HPD_trigger_set(struct dptx_drv_s *dptx, u8 en);

void dptx_act_timing_to_venc_config(struct dptx_drv_s *dptx);

u8 __str_add_vmode(struct dptx_drv_s *dptx, char *buf, struct dptx_vmode_s *vmd_p, u8 fr);

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

void dptx_vout_notify_mode_change_pre(struct dptx_drv_s *dptx);
void dptx_vout_notify_mode_change(struct dptx_drv_s *dptx);

////! Content Protect
void dptx_set_content_protection(struct dptx_drv_s *dptx, u8 port);

void edptx_drv_check_HPD(struct dptx_drv_s *dptx);
void dptx_driver_ready(struct dptx_drv_s *dptx);
void dptx_driver_panel_power_ctrl(struct dptx_drv_s *dptx, u8 en);
void dptx_drv_start(struct dptx_drv_s *dptx);
void edptx_drv_disp_on(struct dptx_drv_s *dptx);
void edptx_drv_disp_off(struct dptx_drv_s *dptx);
void dptx_driver_close(struct dptx_drv_s *dptx);
void dptx_drv_eDP_PSR1_en(struct dptx_drv_s *dptx, u8 port_mask, u8 en);
void dptx_drv_eDP_PSR2_en(struct dptx_drv_s *dptx, u8 port_mask, u8 en);

//void dptx_info_print(struct dptx_drv_s *dptx);
//void dptx_reg_print(struct dptx_drv_s *dptx);

//dptx_debug.c
int dptx_debug_probe(struct dptx_drv_s *dptx);
int dptx_debug_remove(struct dptx_drv_s *dptx);
void dptx_debug_reset(struct dptx_drv_s *dptx, u8 port_mask, u8 reset_part);

//dptx_notify.c
int dptx_notifier_init(void);
void dptx_notifier_remove(void);
int dptx_notifier_call_chain(unsigned long event, void *v);

//dptx_drv.c -- notify call
void dptx_notify_check_HPD(struct dptx_drv_s *dptx);
void dptx_notify_set_backlight(struct dptx_drv_s *dptx, u8 en);
void dptx_notify_set_screen_mute(struct dptx_drv_s *dptx, u8 en);
void dptx_notify_set_link(struct dptx_drv_s *dptx, u8 en);
void dptx_notify_set_driver_ready(struct dptx_drv_s *dptx, u8 en);
void dptx_notify_set_venc(struct dptx_drv_s *dptx, u8 en);
void dptx_notify_set_HPD_trigger(struct dptx_drv_s *dptx, u8 en);

//dptx_vout_server.c
void dptx_vout_server_init(struct dptx_drv_s *dptx);
void dptx_vout_server_deinit(struct dptx_drv_s *dptx);

//dptx_drm.c
int dptx_drm_add(struct device *dev);
void dptx_drm_remove(struct device *dev);

void eDP_bl_wrapper_regist(struct dptx_drv_s *dptx);

#endif
