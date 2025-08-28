/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_COMMON_H
#define __HDMITX_HW_COMMON_H

#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_mode.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_format_para.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx.h>

/* hw_cntl cmd type */
#define CMD_TYPE_MASK				(0xFF << 24)
#define CMD_DDC_AUX_OFFSET			(0x10 << 24)
#define CMD_HDCP_OFFSET				(0x11 << 24)
#define CMD_AUX_PKT_OFFSET			(0x12 << 24)
#define CMD_AUX_AUD_OFFSET			(0x13 << 24)
#define CMD_ALLM_OFFSET				(0x14 << 24)
#define CMD_FRL_OFFSET				(0x15 << 24)
#define CMD_DSC_OFFSET				(0x16 << 24)
#define CMD_VRR_OFFSET				(0x17 << 24)
#define CMD_CORE_MISC_OFFSET		(0x18 << 24)
#define CMD_PLATFORM_CNTL_OFFSET	(0x19 << 24)
#define CMD_VPU_CNTL_OFFSET			(0x1a << 24)
#define CMD_MODE_FLOW_MISC_OFFSET	(0x1b << 24)
/* special offset for DPTX */
#define CMD_DPTX_OFFSET				(0x1 << 20)

/* 0x10 DDC for HDMITX */
#define DDC_PIN_MUX_OP				(CMD_DDC_AUX_OFFSET + 0x00)
	#define PIN_MUX 0x1
	#define PIN_UNMUX 0x2
#define DDC_RESET_EDID				(CMD_DDC_AUX_OFFSET + 0x01)
#define DDC_EDID_READ_DATA			(CMD_DDC_AUX_OFFSET + 0x02)
#define DDC_GET_CEDST				(CMD_DDC_AUX_OFFSET + 0x03)
#define DDC_GLITCH_FILTER_RESET		(CMD_DDC_AUX_OFFSET + 0x04)
#define DDC_I2C_RATE				(CMD_DDC_AUX_OFFSET + 0x05)
#define DDC_I2C_RESET				(CMD_DDC_AUX_OFFSET + 0x06)
#define DDC_SCDC_DIV40_SCRAMB		(CMD_DDC_AUX_OFFSET + 0x07)
	#define SET_SCDC_DIV40_SCRAMB 1
	#define CLEAR_SCDC_DIV40_SCRAMB 0
#define DDC_SCDC_STS_FLAG0          (CMD_DDC_AUX_OFFSET + 0x08)
#define DDC_SCDC_LN0_LN1_LTP        (CMD_DDC_AUX_OFFSET + 0x09)
#define DDC_SCDC_LN2_LN3_LTP        (CMD_DDC_AUX_OFFSET + 0x0a)
/* DDC for DPTX */
#define DP_DDC_AUX_CMD				(CMD_DDC_AUX_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x11 HDCP for HDMITX */
#define HDCP_ESM_RESET			(CMD_HDCP_OFFSET + 0x00)
#define HDCP_CLKDIS				(CMD_HDCP_OFFSET + 0x01)
#define HDCP_MODE_OP			(CMD_HDCP_OFFSET + 0x02)
	#define HDCP14_ON	0x1
	#define HDCP14_OFF	0x2
	#define HDCP22_ON	0x3
	#define HDCP22_OFF	0x4
#define HDCP_DISABLE			(CMD_HDCP_OFFSET + 0x03)
#define HDCP_RESET				(CMD_HDCP_OFFSET + 0x04)
#define HDCP_GET_AKSV			(CMD_HDCP_OFFSET + 0x05)
#define HDCP_GET_BKSV			(CMD_HDCP_OFFSET + 0x06)
#define HDCP_GET_AUTH_RESULT	(CMD_HDCP_OFFSET + 0x07)
/* reset hdcp param for suspend/plugout, not for mode switch */
#define HDCP_PARAM_RESET		(CMD_HDCP_OFFSET + 0x08)
#define HDCP14_KEY_LOAD			(CMD_HDCP_OFFSET + 0x09)
/* check hdcp22 support or not by DDC read */
#define HDCP22_GET_RX_VER		(CMD_HDCP_OFFSET + 0x0a)
#define HDCP_REQ_AUTH			(CMD_HDCP_OFFSET + 0x0b)
#define HDCP14_KEY_VALIDATE		(CMD_HDCP_OFFSET + 0x0c)
#define HDCP_MUX_INIT			(CMD_HDCP_OFFSET + 0x0e)
#define HDCP_14_LSTORE			(CMD_HDCP_OFFSET + 0x0f)
#define HDCP_22_LSTORE			(CMD_HDCP_OFFSET + 0x10)
#define HDCP14_GET_BCAPS_RP		(CMD_HDCP_OFFSET + 0x11)
#define HDCP14_GET_TOPO_INFO	(CMD_HDCP_OFFSET + 0x12)
#define HDCP_SET_TOPO_INFO		(CMD_HDCP_OFFSET + 0x13)
#define HDCP_GET_TOPO_INFO		(CMD_HDCP_OFFSET + 0x14)
#define HDCP14_SAVE_OBS			(CMD_HDCP_OFFSET + 0x15)
#define HDCP14_GET_RI			(CMD_HDCP_OFFSET + 0x16)
#define HDCP14_GET_BCAPS		(CMD_HDCP_OFFSET + 0x17)
#define HDCP14_GET_BSTATUS		(CMD_HDCP_OFFSET + 0x18)
#define HDCP14_GET_AN			(CMD_HDCP_OFFSET + 0x19)
#define HDCP22_GET_AXI_DMC_STS		(CMD_HDCP_OFFSET + 0x1a)
#define HDCP_ESM_KILL			(CMD_HDCP_OFFSET + 0x1b)
#define HDCP22_EN_DMC_REQ		(CMD_HDCP_OFFSET + 0x1c)

/* HDCP special for DPTX */
#define DP_HDCP_CMD				(CMD_HDCP_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x12 packet */
#define AUX_PKT_GET_GCP_CD				      (CMD_AUX_PKT_OFFSET + 0x00)
#define AUX_PKT_CONFIG_AVMUTE			      (CMD_AUX_PKT_OFFSET + 0x01)
	#define OFF_AVMUTE      0x0
	#define CLR_AVMUTE      0x1
	#define SET_AVMUTE      0x2
#define AUX_PKT_GET_AVMUTE_ST			      (CMD_AUX_PKT_OFFSET + 0x02)
#define AUX_PKT_SET_SPD_INFO			      (CMD_AUX_PKT_OFFSET + 0x03)
#define AUX_PKT_AVI_CONSTRUCT			      (CMD_AUX_PKT_OFFSET + 0x04)
#define AUX_PKT_GET_AVI_VIC				      (CMD_AUX_PKT_OFFSET + 0x05)
#define AUX_PKT_SET_AVI_VIC				      (CMD_AUX_PKT_OFFSET + 0x06)
#define AUX_PKT_GET_AVI_CS				      (CMD_AUX_PKT_OFFSET + 0x07)
#define AUX_PKT_SET_AVI_CS                    (CMD_AUX_PKT_OFFSET + 0x08)
#define AUX_PKT_CONF_AVI_BT2020			      (CMD_AUX_PKT_OFFSET + 0x09)
	#define CLR_AVI_BT2020	0
	#define SET_AVI_BT2020	1
#define AUX_PKT_GET_AVI_COLORIMETRY           (CMD_AUX_PKT_OFFSET + 0x0a)
#define AUX_PKT_GET_AVI_EXTENDED_COLORIMETRY  (CMD_AUX_PKT_OFFSET + 0x0b)
#define AUX_PKT_CONF_AVI_Q01			      (CMD_AUX_PKT_OFFSET + 0x0c)
#define AUX_PKT_GET_AVI_Q01                   (CMD_AUX_PKT_OFFSET + 0x0d)
#define AUX_PKT_CONF_AVI_YQ01			      (CMD_AUX_PKT_OFFSET + 0x0e)
#define AUX_PKT_GET_AVI_YQ01                  (CMD_AUX_PKT_OFFSET + 0x0f)
#define AUX_PKT_CONF_AVI_CT				      (CMD_AUX_PKT_OFFSET + 0x10)
#define AUX_PKT_GET_AVI_CT_TYPE               (CMD_AUX_PKT_OFFSET + 0x11)
#define AUX_PKT_GET_AVI_ITC                   (CMD_AUX_PKT_OFFSET + 0x12)
#define AUX_PKT_CONF_AVI_ASPECT			      (CMD_AUX_PKT_OFFSET + 0x13)
#define AUX_PKT_GET_AVI_PICTURE_ASPECT        (CMD_AUX_PKT_OFFSET + 0x14)
#define AUX_PKT_CONF_AVI_SCAN			      (CMD_AUX_PKT_OFFSET + 0x15)
#define AUX_PKT_GET_AVI_SCAN			      (CMD_AUX_PKT_OFFSET + 0x16)
#define AUX_PKT_CLR_AVI					      (CMD_AUX_PKT_OFFSET + 0x17)
#define AUX_PKT_GET_HDR_ST				      (CMD_AUX_PKT_OFFSET + 0x18)
#define AUX_PKT_GET_AMDV_ST				      (CMD_AUX_PKT_OFFSET + 0x19)
#define AUX_PKT_GET_HDR10P_ST			      (CMD_AUX_PKT_OFFSET + 0x1a)
#define AUX_PKT_GET_CUVA_ST				      (CMD_AUX_PKT_OFFSET + 0x1b)
#define AUX_PKT_CLR_DV_VS10_SIG			      (CMD_AUX_PKT_OFFSET + 0x1c)
#define AUX_PKT_SET_VSIF1				      (CMD_AUX_PKT_OFFSET + 0x1d)
#define AUX_PKT_SET_VSIF2				      (CMD_AUX_PKT_OFFSET + 0x1e)
#define AUX_PKT_DIS_VSIF				      (CMD_AUX_PKT_OFFSET + 0x1f)
#define AUX_PKT_SET_DRM					      (CMD_AUX_PKT_OFFSET + 0x20)
#define AUX_PKT_SET_EMP_CUVA				  (CMD_AUX_PKT_OFFSET + 0x21)
#define AUX_PKT_SET_EMP_SBTM			      (CMD_AUX_PKT_OFFSET + 0x22)
#define AUX_PKT_CONF_EMP_VRR_QMS			  (CMD_AUX_PKT_OFFSET + 0x23)
#define AUX_PKT_CONF_EMP_VRR_GAME		      (CMD_AUX_PKT_OFFSET + 0x24)
#define AUX_PKT_GET_AVI_VIDEO_CODE            (CMD_AUX_PKT_OFFSET + 0x25)
#define AUX_PKT_GET_AVI_ACTIVE_ASPECT         (CMD_AUX_PKT_OFFSET + 0x26)
#define AUX_PKT_GET_AVI_NUPS                  (CMD_AUX_PKT_OFFSET + 0x27)
#define AUX_PKT_GET_AVI_PIXEL_REPEAT          (CMD_AUX_PKT_OFFSET + 0x28)
#define AUX_PKT_GET_AVI_TOP_BAR               (CMD_AUX_PKT_OFFSET + 0x29)
#define AUX_PKT_GET_AVI_BOTTOM_BAR            (CMD_AUX_PKT_OFFSET + 0x2a)
#define AUX_PKT_GET_AVI_LEFT_BAR              (CMD_AUX_PKT_OFFSET + 0x2b)
#define AUX_PKT_GET_AVI_RIGHT_BAR             (CMD_AUX_PKT_OFFSET + 0x2c)
#define AUX_PKT_GET_DRM_EOTF                  (CMD_AUX_PKT_OFFSET + 0x2d)
#define AUX_PKT_GET_AUDIO_CHANNEL             (CMD_AUX_PKT_OFFSET + 0x2e)
#define AUX_PKT_GET_AUDIO_CODING_TYPE         (CMD_AUX_PKT_OFFSET + 0x2f)
#define AUX_PKT_GET_AUDIO_SAMPLE_SIZE         (CMD_AUX_PKT_OFFSET + 0x30)
#define AUX_PKT_GET_AUDIO_CODING_FREQUENCY    (CMD_AUX_PKT_OFFSET + 0x31)
#define AUX_PKT_GET_AUDIO_CODING_TYPE_EXT     (CMD_AUX_PKT_OFFSET + 0x32)
#define AUX_PKT_GET_AUDIO_CHANNEL_ALLOCATION  (CMD_AUX_PKT_OFFSET + 0x33)
#define AUX_PKT_GET_AUDIO_LEVEL_SHIFT_VALUE   (CMD_AUX_PKT_OFFSET + 0x34)
#define AUX_PKT_GET_AUDIO_DOWNMIX_INHIBIT     (CMD_AUX_PKT_OFFSET + 0x35)
#define AUX_PKT_GET_AUDIO_N                   (CMD_AUX_PKT_OFFSET + 0x36)
#define AUX_PKT_GET_AUDIO_CTS                 (CMD_AUX_PKT_OFFSET + 0x37)
#define AUX_PKT_GET_AUDIO_LAYOUT              (CMD_AUX_PKT_OFFSET + 0x38)
#define AUX_PKT_GET_QMS_M_CONST	              (CMD_AUX_PKT_OFFSET + 0x39)
#define AUX_PKT_GET_QMS_NEXT_TFR              (CMD_AUX_PKT_OFFSET + 0x3a)
#define AUX_PKT_GET_QMS_BASE_REFRESH_RATE     (CMD_AUX_PKT_OFFSET + 0x3b)
#define AUX_PKT_GET_SPD_SDI                   (CMD_AUX_PKT_OFFSET + 0x3c)
#define AUX_PKT_GET_VSIF_IEEEOUI              (CMD_AUX_PKT_OFFSET + 0x3d)
#define AUX_PKT_GET_VSIF_VIC                  (CMD_AUX_PKT_OFFSET + 0x3e)
#define AUX_PKT_GET_VSIF_ALLM                 (CMD_AUX_PKT_OFFSET + 0x3f)
#define AUX_PKT_GET_VSIF_AMDV_SIGNAL          (CMD_AUX_PKT_OFFSET + 0x40)
#define AUX_PKT_GET_VSIF_AMDV_LOW_LATENCY     (CMD_AUX_PKT_OFFSET + 0x41)

/* Aux special for DPTX */
#define DP_AUX_CMD					(CMD_AUX_PKT_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x13 audio for hdmitx */
#define AUDIO_RESET					(CMD_AUX_AUD_OFFSET + 0x0)
#define AUDIO_ACR_CTRL				(CMD_AUX_AUD_OFFSET + 0x1)
	#define ENABLE_AUDIO_ACR  1
	#define DISABLE_AUDIO_ACR 0
#define AUDIO_PREPARE				(CMD_AUX_AUD_OFFSET + 0x2)
#define AUDIO_MUTE_OP				(CMD_AUX_AUD_OFFSET + 0x3)
	#define AUDIO_MUTE		0x1
	#define AUDIO_UNMUTE	0x2
#define AUDIO_GET_UBOOT_ST			(CMD_AUX_AUD_OFFSET + 0x4)
/* Audio special for DPTX */
#define DP_AUDIO_CMD				(CMD_AUX_AUD_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x14 ALLM */
#define ALLM_CONFIG					(CMD_ALLM_OFFSET + 0x00)
	#define ENABLE_ALLM         1
	#define DISABLE_ALLM        0

/* 0x15 FRL */
#define FRL_GET_MODE			(CMD_FRL_OFFSET + 0x0)
#define FRL_CLR_MODE			(CMD_FRL_OFFSET + 0x1)
#define FRL_GET_RX_READY_ST		(CMD_FRL_OFFSET + 0x2)
#define FRL_DISABLE_WORK		(CMD_FRL_OFFSET + 0x3)
/* training special for DPTX */
#define DP_TRAINING_CMD			(CMD_FRL_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x16 DSC */
#define DSC_GET_TX_EN			(CMD_DSC_OFFSET + 0x0)
#define DP_DSC_CMD				(CMD_DSC_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x17 VRR */
#define VRR_REGISTER			(CMD_VRR_OFFSET + 0x00)
#define QMS_GET_INFO			(CMD_VRR_OFFSET + 0x01)
#define GAME_GET_INFO			(CMD_VRR_OFFSET + 0x02)
#define EMP_GET_INFO			(CMD_VRR_OFFSET + 0x03)

/* 0x18 core misc */
#define CORE_MISC_SUSPEND_RESUME_CNTL			(CMD_CORE_MISC_OFFSET + 0x0)
	#define HDMITX_EARLY_SUSPEND	0x1
	#define HDMITX_LATE_RESUME		0x2
/* only for hw20 top clk pattern */
#define CORE_MISC_TMDS_CLK_DIV40		(CMD_CORE_MISC_OFFSET + 0x01)
#define CORE_MISC_GET_OUTPUT_ST			(CMD_CORE_MISC_OFFSET + 0x02)
#define CORE_MISC_SET_HDMI_DVI_MODE		(CMD_CORE_MISC_OFFSET + 0x03)
	#define HDMI_MODE	0x1
	#define DVI_MODE	0x2
#define CORE_MISC_GET_HDMI_DVI_MODE		(CMD_CORE_MISC_OFFSET + 0x04)
#define CORE_MISC_VP_CMS_CSC			(CMD_CORE_MISC_OFFSET + 0x05)
#define CORE_MISC_HW_INIT				(CMD_CORE_MISC_OFFSET + 0x06)
#define CORE_MISC_TRIGGER_HPD			(CMD_CORE_MISC_OFFSET + 0x07)
#define DP_CORE_MISC_CMD			(CMD_CORE_MISC_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x19 CMD_PLATFORM */
/* GPIO PIN */
#define PLATFORM_GET_HPD_GPI_ST			(CMD_PLATFORM_CNTL_OFFSET + 0x01)
/* hpll/clk */
#define CMD_PLATFORM_CLK_CNTL_OFFSET	(CMD_PLATFORM_CNTL_OFFSET + (0x1 << 16))
#define PLATFORM_DIS_HPLL				(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x01)
#define PLATFORM_ESM_CLK_CTRL			(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x02)
#define PLATFORM_CLK_DIV_RST			(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x03)
#define PLATFORM_HDMI_CLKS_CTRL			(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x04)
#define PLATFORM_GET_CLKMSR				(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x05)
#define PLATFORM_GET_TMDS_CLK			(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x06)
#define PLATFORM_GET_PIXEL_CLK			(CMD_PLATFORM_CLK_CNTL_OFFSET + 0x07)
/* phy */
#define CMD_PLATFORM_PHY_CNTL_OFFSET	(CMD_PLATFORM_CNTL_OFFSET + (0x2 << 16))
#define PLATFORM_PHY_OP					(CMD_PLATFORM_PHY_CNTL_OFFSET + 0x00)
	#define TMDS_PHY_NONE		0x0
	#define TMDS_PHY_ENABLE		0x1
	#define TMDS_PHY_DISABLE	0x2
#define PLATFORM_RXSENSE				(CMD_PLATFORM_PHY_CNTL_OFFSET + 0x01)
#define PLATFORM_GET_PHY_STAT			(CMD_PLATFORM_PHY_CNTL_OFFSET + 0x02)
#define DP_PLATFORM_CNTL_CMD			(CMD_PLATFORM_CNTL_OFFSET + CMD_DPTX_OFFSET + 0x0)

/* 0x1a VPU */
#define VPU_VIDEO_MUTE_OP			(CMD_VPU_CNTL_OFFSET + 0x00)
	#define VIDEO_NONE_OP	0x0
	#define VIDEO_MUTE		0x1
	#define VIDEO_UNMUTE	0x2
/* set value as COLORSPACE_RGB444, YUV422, YUV444, YUV420 */
#define VPU_CONFIG_CSC				(CMD_VPU_CNTL_OFFSET + 0x1)
	#define CSC_Y444_8BIT		0x1
	#define CSC_Y422_12BIT		0x2
	#define CSC_RGB_8BIT		0x3
	#define CSC_UPDATE_AVI_CS	0x10
/* set CSC_ENABLE when DV_STD && Adaptive HDR */
#define VPU_CONFIG_CSC_EN			(CMD_VPU_CNTL_OFFSET + 0x2)
	#define CSC_ENABLE		1
	#define CSC_DISABLE		0
#define VPU_CONFIG_DITHER			(CMD_VPU_CNTL_OFFSET + 0x3)

/* 0x1b mode flow misc */
#define MODE_FLOW_PRIVATE_PLUGOUT_PROCESS	(CMD_MODE_FLOW_MISC_OFFSET + 0x00)
#define MODE_FLOW_HPD_IRQ_TOP_HALF			(CMD_MODE_FLOW_MISC_OFFSET + 0x01)
#define MODE_FLOW_PRE_ENABLE_MODE			(CMD_MODE_FLOW_MISC_OFFSET + 0x02)
#define MODE_FLOW_ENABLE_MODE				(CMD_MODE_FLOW_MISC_OFFSET + 0x03)
#define MODE_FLOW_POST_ENABLE_MODE			(CMD_MODE_FLOW_MISC_OFFSET + 0x04)
#define MODE_FLOW_DISABLE_21_WORK			(CMD_MODE_FLOW_MISC_OFFSET + 0x05)

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
	int (*setup_irq)(struct hdmitx_hw_common *tx_hw);
	int (*free_irq)(struct hdmitx_hw_common *tx_hw);
	int (*hw_cntl)(struct hdmitx_hw_common *tx_hw, u32 cmd,
		void *input_argv, void *output_struct);

	/* validate if vic is supported by hw ip/phy */
	int (*validate_mode)(struct hdmitx_hw_common *tx_hw, u32 vic, u32 max_refresh_rate);
	/* calc format para hw info */
	int (*calc_format_para)(struct hdmitx_hw_common *tx_hw, struct hdmi_format_para *para);

	int (*set_aud_mode)(struct hdmitx_hw_common *tx_hw, struct aud_para *audio_param);
	int (*set_disp_mode)(struct hdmitx_hw_common *tx_hw, struct hdmi_format_para *video_para);

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

	/* save the lastest plug_in time from interrupt */
	u64 hw_sequence_id;
	u32 dongle_mode:1;
	struct amhdmitx_data_s *chip_data;
};

int hdmitx_hw_cntl(struct hdmitx_hw_common *tx_hw,
	u32 cmd, void *input_argv, void *output_struct);
int hdmitx_hw_validate_mode(struct hdmitx_hw_common *tx_hw,
	u32 vic, u32 max_refresh_rate);
int hdmitx_hw_calc_format_para(struct hdmitx_hw_common *tx_hw,
	struct hdmi_format_para *para);
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
