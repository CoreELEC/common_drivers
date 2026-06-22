/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_COMMON_H
#define __HDMITX_HW_COMMON_H

#include <linux/types.h>
#include <linux/platform_device.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_types.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_format_para.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx.h>

/* CN TYPE define */
enum {
	SET_CT_OFF = 0,
	SET_CT_GAME = 1,
	SET_CT_GRAPHICS = 2,
	SET_CT_PHOTO = 3,
	SET_CT_CINEMA = 4,
};

enum hdmi_ll_mode {
	HDMI_LL_MODE_AUTO = 0,
	HDMI_LL_MODE_DISABLE,
	HDMI_LL_MODE_ENABLE,
};

enum i2c_rate {
	DDC_I2C_25K,
	DDC_I2C_38K,
	DDC_I2C_50K,
	DDC_I2C_75K,
};

/* chip type */
enum amhdmitx_chip_e {
	MESON_CPU_ID_M8B = 0,
	MESON_CPU_ID_GXBB,
	MESON_CPU_ID_GXTVBB,
	MESON_CPU_ID_GXL,
	MESON_CPU_ID_GXM,
	MESON_CPU_ID_TXL,
	MESON_CPU_ID_TXLX,
	MESON_CPU_ID_AXG,
	MESON_CPU_ID_GXLX,
	MESON_CPU_ID_TXHD,
	MESON_CPU_ID_G12A,
	MESON_CPU_ID_G12B,
	MESON_CPU_ID_SM1,
	MESON_CPU_ID_TM2,
	MESON_CPU_ID_TM2B,
	MESON_CPU_ID_SC2,

	MESON_CPU_ID_T7,
	MESON_CPU_ID_S1A,
	MESON_CPU_ID_S5,
	MESON_CPU_ID_S7,
	MESON_CPU_ID_S7D,
	MESON_CPU_ID_S6,
	MESON_CPU_ID_A9,
	MESON_CPU_ID_MAX,
};

struct hdmitx_hw_common;

struct hdmitx_ops {
	struct hdmitx_common *(*alloc_instance)(struct device *device);
	int (*init_reg_map)(struct platform_device *pdev);
	void (*init_hw)(struct hdmitx_hw_common *tx_hw);
	void (*uninit_hw)(struct hdmitx_hw_common *tx_hw);
	struct hdmitx_dbg_files_s *(*get_dbg_files)(void);
	int (*get_dbg_files_count)(void);
	void (*sw_debug_func)(struct hdmitx_common *tx_comm, const char *cmd_str);
	void *(*hdcp_init)(void *hdcptx_comm);
	void (*pm_restore)(struct hdmitx_common *tx_comm);
};

struct amhdmitx_data_s {
	enum amhdmitx_chip_e chip_type;
	const char *chip_name;
	struct hdmitx_ops *hdmitx_ops;
};

/***********************************************************************
 *             HDMITX COMMON STRUCT & API
 **********************************************************************/
struct tx_cap {
	/* configure in dts file */
	enum frl_rate_enum  tx_max_frl_rate;
	/* default 600Mhz, if res_1080p, then 225Mhz */
	u32 tx_max_tmds_clk;
	/* soc support DSC or not */
	bool dsc_capable;
	/* DSC enable policy, refer to dsc_policy sysfs node */
	u8 dsc_policy;
};

struct hdmitx_hw_common {
	struct meson_tx_hw tx_hw_base;
	int (*setup_irq)(struct hdmitx_hw_common *tx_hw);
	int (*free_irq)(struct hdmitx_hw_common *tx_hw);
	int (*hw_cntl)(struct hdmitx_hw_common *tx_hw, u32 cmd,
		void *input_argv, void *output_struct);

	/* validate if vic is supported by hw ip/phy */
	int (*validate_mode)(struct hdmitx_hw_common *tx_hw, u32 vic, u32 max_refresh_rate);
	/* calc format para hw info */
	int (*calc_format_para)(struct hdmitx_hw_common *tx_hw, struct meson_tx_format_para *para);

	int (*set_aud_mode)(struct hdmitx_hw_common *tx_hw, struct aud_para *audio_param);
	int (*set_disp_mode)(struct hdmitx_hw_common *tx_hw,
			     struct meson_tx_format_para *video_para);

	/* set vrr rate */
	int (*set_vrr_rate)(struct hdmitx_hw_common *tx_hw, int rate, void *vrr_info);

	/* debug function */
	void (*debug_func)(struct hdmitx_hw_common *tx_hw, const char *cmd_str);
	/* dump packet information */
	int (*pkt_dump)(char *buf, int len);
	ssize_t (*get_clk)(char *buf, int len);

	u8 debug_hpd_lock;
	/* GPIO hpd/scl/sda members */
	int hdmitx_gpios_hpd;
	int hdmitx_gpios_scl;
	int hdmitx_gpios_sda;

	/* soc/hdmitx driver capability */
	struct tx_cap hdmi_tx_cap;
	/* phy state */
	unsigned char tmds_phy_op;
	/* scdc div40 state */
	bool pre_tmds_clk_div40;

	u32 dongle_mode:1;
	struct amhdmitx_data_s *chip_data;
};

void hdmitx_register_hw_event_callback(struct hdmitx_hw_common *hw_comm,
				       struct meson_hw_event_ops *ops);
int hdmitx_hw_cntl(struct hdmitx_hw_common *tx_hw,
	u32 cmd, void *input_argv, void *output_struct);
int hdmitx_hw_validate_mode(struct hdmitx_hw_common *tx_hw,
	u32 vic, u32 max_refresh_rate);
int hdmitx_hw_calc_format_para(struct hdmitx_hw_common *tx_hw,
	struct meson_tx_format_para *para);
int hdmitx_hw_set_phy(struct hdmitx_hw_common *tx_hw,
	int flag);
int hdmitx_hw_set_vrr_rate(struct hdmitx_hw_common *tx_hw, int rate, void *vrr_info);

enum hdmi_tf_type hdmitx_hw_get_hdr_st(struct hdmitx_hw_common *tx_hw);
enum hdmi_tf_type hdmitx_hw_get_dv_st(struct hdmitx_hw_common *tx_hw);
enum hdmi_tf_type hdmitx_hw_get_hdr10p_st(struct hdmitx_hw_common *tx_hw);

enum pkt_op {
	AVI_PKT,
	GAMUT_PKT,
	AUDIO_PKT,
	SPD_PKT,
	MPEG_PKT,
	VSIF_PKT,
	GEN_PKT,
	GEN2_PKT,
	GEN3_PKT,
	GEN4_PKT,
	GEN5_PKT,
	VTEM_PKT,
};

/*
 * HDMITX HPD HW related operations
 */
enum hpd_op {
	HPD_INIT_DISABLE_PULLUP,
	HPD_INIT_SET_FILTER,
	HPD_IS_HPD_MUXED,
	HPD_MUX_HPD,
	HPD_UNMUX_HPD,
	HPD_READ_HPD_GPIO,
};

/*utils functions shared for hdmitx hw module.*/

#endif
