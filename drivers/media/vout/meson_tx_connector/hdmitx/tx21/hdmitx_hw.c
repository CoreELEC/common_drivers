// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/kthread.h>

#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/amlogic/clk_measure.h>
#include "hdmitx_hw_core.h"
#include "hdmitx_hw_platform.h"
#include "hdmitx_compliance.h"
#include "hdmitx_packet.h"
#include "hdmitx_audio.h"
#include "hdmitx_module.h"
#include "hdmitx_hdr.h"
#include "hdmitx_hdcp.h"
#include "meson_tx_event_mgr.h"
#include "meson_tx_internal.h"

#define yuv2rgb  1
#define rgb2yuv  2

#define CUVA_EMP_OFFSET  8
#define CUVA_EMP_0_END   21
#define CUVA_EMP_1_START 24
#define CUVA_EMP_1_END   53
#define CUVA_EMP_2_START 56

static struct hdmitx21_dev *tx21_dev;

static void hdmi_phy_suspend(void);
static void hdmi_phy_wakeup(struct hdmitx21_dev *hdev);
static void hdmitx_set_phy(struct hdmitx21_dev *hdev);
static void hdmitx_set_hw(struct hdmitx21_dev *hdev);
static void hdmitx_csc_config(u8 input_color_format,
			      u8 output_color_format,
			      u8 color_depth);
static int hdmitx_hdmi_dvi_config(struct hdmitx21_dev *hdev,
				  u32 dvi_mode);

static int hdmitx_set_dispmode(struct hdmitx_hw_common *tx_hw, struct meson_tx_format_para *para);
static int hdmitx_set_audmode(struct hdmitx_hw_common *tx_hw,
			      struct aud_para *audio_param);
static void hdmitx_debug(struct hdmitx_hw_common *tx_hw, const char *buf);
static int hdmitx21_hw_cntl(struct hdmitx_hw_common *tx_hw, u32 cmd,
			      void *input_argv, void *output_struct);
static enum frl_rate_enum get_current_frl_rate(void);
static void audio_mute_op(bool flag);
static void hdmitx_set_div40(u32 div40);

static DEFINE_MUTEX(aud_mutex);
static void pkt_send_position_change(u32 enable_mask, u8 mov_val);
static void vpu_hdmi_fmt_config(struct hdmitx21_dev *hdev);
static void vpu_hdmi_setting_config(struct hdmitx21_dev *hdev);
static ssize_t hdmitx_get_clk(char *buf, int len);
static int hdmitx_pkt_dump(char *buf, int len);

static int hdmitx21_pre_enable_mode(struct hdmitx_common *tx_comm);
static int hdmitx21_post_enable_mode(struct hdmitx_common *tx_comm);
static int hdmitx21_enable_mode(struct hdmitx_common *tx_comm);
static void hdmitx21_vp_cms_csc0_multi_csc_config(u32 data);
static void hdcptx_21_ctrl_ops_init(struct hdcptx_common *hdcptx_comm);

#define EDID_RAM_ADDR_SIZE	 (8)

/* HSYNC polarity: active high */
#define HSYNC_POLARITY	 1
/* VSYNC polarity: active high */
#define VSYNC_POLARITY	 1
/* Pixel format: 0=RGB444; 1=YCbCr444; 2=Rsrv; 3=YCbCr422. */
#define TX_INPUT_COLOR_FORMAT   HDMI_COLORSPACE_YUV444
/* Pixel range: 0=16-235/240; 1=16-240; 2=1-254; 3=0-255. */
#define TX_INPUT_COLOR_RANGE	0
/* Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit. */
#define TX_COLOR_DEPTH		 COLORDEPTH_24B

struct _hdmi_clkmsr {
	int idx;
	char *name;
};

static const struct _hdmi_clkmsr hdmiclkmsr_t7[] = {
	{51, "hdmi_vid_pll_clk"},
	{55, "cts_vpu_clk_buf"},
	{59, "cts_hdmi_tx_pixel_clk"},
	{61, "cts_vpu_clk"},
	{62, "cts_vpu_clkb"},
	{63, "cts_vpu_clkb_tmp"},
	{64, "cts_vpu_clkc"},
	{76, "hdmitx_tmds_clk"},
	{77, "cts_hdmitx_sys_clk"},
	{78, "cts_hdmitx_fe_clk"},
	{80, "cts_hdmitx_prif_clk"},
	{81, "cts_hdmitx_200m_clk"},
	{82, "cts_hdmitx_aud_clk"},
	{83, "cts_hdmitx_pnx_clk"},
	{100, "cts_hdmi_aud_pll_clk"},
	{101, "cts_hdmi_acr_ref_clk"},
	{102, "cts_hdmi_meter_clk"},
	{103, "cts_hdmi_vid_clk"},
	{104, "cts_hdmi_aud_clk"},
	{104, "cts_hdmi_dsd_clk"},
	{219, "cts_enc0_if_clk"},
	{220, "cts_enc2_clk"},
	{221, "cts_enc1_clk"},
	{222, "cts_enc0_clk"},
};

static const struct _hdmi_clkmsr hdmiclkmsr_s1a[] = {
	{51, "cts_enci_clk"},
	{52, "cts_encp_clk"},
	{53, "cts_encl_clk"},
	{59, "cts_hdmi_tx_pixel_clk"},
	{61, "cts_vpu_clk"},
	{62, "cts_vpu_clkb"},
	{63, "cts_vpu_clkb_tmp"},
	{64, "cts_vpu_clkc"},
	{76, "hdmitx_tmds_clk"},
	{77, "cts_hdmitx_sys_clk"},
	{78, "cts_hdmitx_fe_clk"},
	{80, "cts_hdmitx_prif_clk"},
	{81, "cts_hdmitx_200m_clk"},
	{82, "cts_hdmitx_aud_clk"},
	{84, "audio_tohdmitx_mclk"},
	{85, "audio_tohdmitx_bclk"},
	{86, "audio_tohdmitx_lrclk"},
	{87, "audio__tohdmitx_spdif_clk"},
};

static const struct _hdmi_clkmsr hdmiclkmsr_s5[] = {
	{4, "fpll_tmds_clk"},
	{8, "fpll_pixel_clk"},
	{16, "vid_pll0_clk"},
	{63, "cts_vpu_clk"},
	{64, "cts_vpu_clkb"},
	{66, "cts_vapbclk"},
	{68, "cts_hdmi_tx_pixel_clk"},
	{69, "cts_hdmi_tx_pnx_clk"},
	{70, "cts_hdmi_tx_fe_clk"},
	{71, "cts_hdmitx_aud_clk"},
	{72, "cts_hdmitx_200m_clk"},
	{73, "cts_hdmitx_prif_clk"},
	{74, "cts_hdmitx_sys_clk"},
	{75, "audio_tohdmitx_mclk"},
	{76, "audio_tohdmi_spdif_clk"},
	{79, "htx_aes_clk"},
	{82, "audio_tohdmitx_bclk"},
	{89, "htx_tmds20_clk"},
	{90, "htx_ch_clk"},
	{91, "htx_phy_clk1618"},
	{92, "cts_htx_tmds_clk"},
	{93, "hdmi_clk_todig"},
	{94, "hdmi_clk_out2"},
	{95, "htx_spll_ref_clk_in"},
	{217, "o_tohdmitx_bclk"},
	{218, "o_tohdmitx_mclk"},
	{219, "o_tohdmitx_spdif_clk"},
};

static const struct _hdmi_clkmsr hdmiclkmsr_s7[] = {
	{19, "hifi0_pll_clk"},
	{21, "hifi1_pll_clk"},
	{49, "hdmi_vx1_pix_clk"},
	{50, "vid_pll_div_clk_out"},
	{51, "cts_enci_clk"},
	{52, "cts_encp_clk"},
	{53, "cts_encl_clk"},
	{59, "cts_hdmi_tx_pixel_clk"},
	{61, "cts_vpu_clk"},
	{62, "cts_vpu_clkb"},
	{63, "cts_vpu_clkb_tmp"},
	{64, "cts_vpu_clkc"},
	{76, "hdmitx_tmds_clk"},
	{77, "cts_hdmitx_sys_clk"},
	{78, "cts_hdmitx_fe_clk"},
	{80, "cts_hdmitx_prif_clk"},
	{81, "cts_hdmitx_200m_clk"},
	{82, "cts_hdmitx_aud_clk"},
	{84, "audio_tohdmitx_mclk"},
	{85, "audio_tohdmitx_bclk"},
	{86, "audio_tohdmitx_lrclk"},
	{87, "audio__tohdmitx_spdif_clk"},
};

/* only for hpd level */
int hdmitx21_hpd_hw_op(struct hdmitx21_hw *tx21_hw, enum hpd_op cmd)
{
	if (cmd == HPD_READ_HPD_GPIO) {
		switch (tx21_hw->base->chip_data->chip_type) {
		case MESON_CPU_ID_S5:
			return !!(hd21_read_reg(PADCTRL_GPIOH_I) & (1 << 2));
		case MESON_CPU_ID_S1A:
			return !!(hd21_read_reg(PADCTRL_GPIOH_I_S1A) & (1 << 2));
		case MESON_CPU_ID_S7:
			return !!(hd21_read_reg(PADCTRL_GPIOH_I_S7) & (1 << 2));
		case MESON_CPU_ID_S7D:
			return !!(hd21_read_reg(PADCTRL_GPIOH_I_S7D) & (1 << 2));
		case MESON_CPU_ID_S6:
			return !!(hd21_read_reg(PADCTRL_GPIOH_I_S6) & (1 << 2));
		case MESON_CPU_ID_T7:
		default:
			return !!(hd21_read_reg(PADCTRL_GPIOW_I) & (1 << 15));
		}
	}
	return 0;
}

int hdmitx21_ddc_hw_op(struct hdmitx21_hw *tx21_hw, enum ddc_op cmd)
{
	switch (tx21_hw->base->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	default:
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(hdmitx21_ddc_hw_op);

static void config_avmute(u32 val)
{
	HDMITX_DEBUG(HW "avmute set to %d\n", val);
	switch (val) {
	/* This code is required for proper generation of GCP's in ES1 */
	case SET_AVMUTE:
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 3, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 3, 1);
		break;
	case CLR_AVMUTE:
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 3, 1);
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 7, 1);
		break;
	case OFF_AVMUTE:
	default:
		break;
	}
}

static int read_avmute(void)
{
	return 0;
}

/* reset HDMITX APB & TX */
void hdmitx21_sys_reset(struct hdmitx21_hw *tx21_hw)
{
	switch (tx21_hw->base->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S1A:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		hdmitx21_sys_reset_t7();
		break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case MESON_CPU_ID_S5:
		hdmitx21_sys_reset_s5();
		break;
	case MESON_CPU_ID_S7:
		hdmitx21_sys_reset_s7();
		break;
#endif
	default:
		break;
	}
}

/*
 * use PLL/phy state
 * note that PHY_CTRL0 may be enabled for band_gap in plugin top half,
 * so should not use band_gap setting bits for uboot state decision
 */
static bool hdmitx21_uboot_already_display(struct hdmitx21_hw *tx21_hw)
{
	bool tmds_pixel_clk_en = false;

	/*
	 * if (hdev->pxp_mode)
	 *return 0;
	 */
	if (tx21_hw->base->chip_data->chip_type == MESON_CPU_ID_S5)
		tmds_pixel_clk_en = (hdmitx21_rd_reg(HDMITX_TOP_CLK_CNTL) & 0x7) == 0x7;
	else
		tmds_pixel_clk_en = (hdmitx21_rd_reg(HDMITX_TOP_CLK_CNTL) & 0x3) == 0x3;
	if (!tmds_pixel_clk_en)
		return false;

	if (hd21_read_reg(ANACTRL_HDMIPHY_CTRL0))
		return true;
	return false;
}

static enum hdmi_color_depth _get_colordepth(void)
{
	u32 data;
	u8 val;
	enum hdmi_color_depth depth = COLORDEPTH_24B;

	data = hdmitx21_rd_reg(P2T_CTRL_IVCTX);
	HDMITX_DEBUG("P2T_CTRL_IVCTX = %d\n", data);
	if (data & (1 << 7)) {
		val = data & 0x3;
		switch (val) {
		case 1:
			depth = COLORDEPTH_30B;
			break;
		case 2:
			depth = COLORDEPTH_36B;
			break;
		case 3:
			depth = COLORDEPTH_48B;
			break;
		case 0:
		default:
			depth = COLORDEPTH_24B;
			break;
		}
	}

	return depth;
}

static int hdmitx_get_scan_info_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->scan_mode;

	return ret;
}

static int hdmitx_get_colorimetry_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->colorimetry;

	return ret;
}

static int hdmitx_get_extended_colorimetry_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0) {
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	} else {
		/* colorimetry 3: extended */
		if (avi->colorimetry == HDMI_COLORIMETRY_EXTENDED)
			ret = avi->extended_colorimetry;
		else
			ret = -1;
	}
	return ret;
}

static int hdmitx_get_picture_aspect_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->picture_aspect;

	return ret;
}

static int hdmitx_get_active_aspect_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->active_aspect;

	return ret;
}

static int hdmitx_get_quantization_range_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->quantization_range;

	return ret;
}

static int hdmitx_get_nups_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->nups;

	return ret;
}

static int hdmitx_get_ycc_quantization_range_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->ycc_quantization_range;

	return ret;
}

static bool hdmitx_get_itc_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->itc;

	return ret;
}

static int hdmitx_get_content_type_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->content_type;

	return ret;
}

static int hdmitx_get_pixel_repeat_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->pixel_repeat;

	return ret;
}

static int hdmitx_get_top_bar_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->top_bar;

	return ret;
}

static int hdmitx_get_bottom_bar_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->bottom_bar;

	return ret;
}

static int hdmitx_get_left_bar_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->left_bar;

	return ret;
}

static int hdmitx_get_right_bar_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->right_bar;

	return ret;
}

static int hdmitx_get_vic_from_avi(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;

	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing avi failed %d\n", ret);
	else
		ret = avi->video_code;

	return ret;
}

static enum hdmi_vic _get_vic_from_vsif(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	enum hdmi_vic hdmi4k_vic = HDMI_0_UNKNOWN;
	union hdmi_infoframe *infoframe = &hdev->tx21_hw.infoframe->vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;

	ret = hdmi_vend_infoframe_get(tx_comm, body);
	if (ret == -1 || ret == 0)
		return hdmi4k_vic;
	ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
	if (ret >= 0) {
		if (vendor->oui != HDMI_IEEE_OUI)
			return hdmi4k_vic;
		switch (vendor->vic) {
		case 1:
			hdmi4k_vic = HDMI_95_3840x2160p30_16x9;
			break;
		case 2:
			hdmi4k_vic = HDMI_94_3840x2160p25_16x9;
			break;
		case 3:
			hdmi4k_vic = HDMI_93_3840x2160p24_16x9;
			break;
		case 4:
			hdmi4k_vic = HDMI_98_4096x2160p24_256x135;
			break;
		default:
			break;
		}
	}
	return hdmi4k_vic;
}

static unsigned int hdmitx_get_ieeeoui_from_vsif(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx21_hw.infoframe->vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;

	ret = hdmi_vend_infoframe_get(tx_comm, body);
	if (ret == -1 || ret == 0)
		return 0;

	vendor->oui = body[6] << 16 | body[5] << 8 | body[4];

	return vendor->oui;
}

static unsigned int hdmitx_get_allm_from_vsif(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx21_hw.infoframe->vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	unsigned int allm = 0;

	ret = hdmi_vend_infoframe_get(tx_comm, body);
	if (ret == -1 || ret == 0)
		return allm;
	vendor->oui = body[6] << 16 | body[5] << 8 | body[4];
	if (vendor->oui == HDMI_FORUM_IEEE_OUI)
		allm = (body[8] & 0x2) >> 1;

	return allm;
}

static unsigned int hdmitx_get_amdv_signal_from_vsif(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx21_hw.infoframe->vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	unsigned int amdv_signal = 0;

	ret = hdmi_vend_infoframe_get(tx_comm, body);
	if (ret == -1 || ret == 0)
		return amdv_signal;
	vendor->oui = body[6] << 16 | body[5] << 8 | body[4];
	if (vendor->oui == DOVI_IEEEOUI)
		amdv_signal = (body[7] & 0x2) >> 1;

	return amdv_signal;
}

static unsigned int hdmitx_get_amdv_low_latency_from_vsif(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx21_hw.infoframe->vend;
	struct hdmi_vendor_infoframe *vendor = &infoframe->vendor.hdmi;
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	unsigned int amdv_low_latency = 0;

	ret = hdmi_vend_infoframe_get(tx_comm, body);
	if (ret == -1 || ret == 0)
		return amdv_low_latency;
	vendor->oui = body[6] << 16 | body[5] << 8 | body[4];
	if (vendor->oui == DOVI_IEEEOUI)
		amdv_low_latency = body[7] & 0x1;

	return amdv_low_latency;
}

static int hdmitx_get_eotf_from_drm(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.drm;
	struct hdmi_drm_infoframe *drm = &infoframe->drm;

	ret = hdmi_drm_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_drm_infoframe_unpack_renew(drm, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing drm failed %d\n", ret);
	else
		ret = drm->eotf;

	return ret;
}

static int hdmitx_get_n_from_audio(struct hdmitx21_dev *hdev)
{
	unsigned int n1 = 0;
	unsigned int n2 = 0;
	unsigned int n3 = 0;
	unsigned int n = 0;

	n1 = hdmitx21_rd_reg(N_SVAL1_IVCTX);
	n2 = hdmitx21_rd_reg(N_SVAL2_IVCTX);
	n3 = hdmitx21_rd_reg(N_SVAL3_IVCTX);

	n = n3 << 16 | n2 << 8 | n1;

	return n;
}

static int hdmitx_get_cts_from_audio(struct hdmitx21_dev *hdev)
{
	unsigned int cts1 = 0;
	unsigned int cts2 = 0;
	unsigned int cts3 = 0;
	unsigned int cts = 0;

	cts1 = hdmitx21_rd_reg(CTS_TXHVAL1_IVCTX);
	cts2 = hdmitx21_rd_reg(CTS_TXHVAL2_IVCTX);
	cts3 = hdmitx21_rd_reg(CTS_TXHVAL3_IVCTX);

	cts = cts3 << 16 | cts2 << 8 | cts1;

	return cts;
}

static int hdmitx_get_channel_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->channels;

	return ret;
}

static int hdmitx_get_coding_type_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->coding_type;

	return ret;
}

static int hdmitx_get_sample_size_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->sample_size;

	return ret;
}

static int hdmitx_get_sample_frequency_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->sample_frequency;

	return ret;
}

static int hdmitx_get_coding_type_ext_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->coding_type_ext;

	return ret;
}

static int hdmitx_get_channel_allocation_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->channel_allocation;

	return ret;
}

static int hdmitx_get_level_shift_value_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->level_shift_value;

	return ret;
}

static int hdmitx_get_downmix_inhibit_from_audio(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.aud;
	struct hdmi_audio_infoframe *audio = &infoframe->audio;

	ret = hdmi_audio_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_audio_infoframe_unpack_renew(audio, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing audio failed %d\n", ret);
	else
		ret = audio->downmix_inhibit;

	return ret;
}

static char hdmitx_get_sdi_from_spd(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &hdev->tx_comm.infoframe.spd;
	struct hdmi_spd_infoframe *spd = &infoframe->spd;

	ret = hdmi_spd_infoframe_get(body);
	if (ret == -1 || ret == 0)
		return -1;
	ret = hdmi_spd_infoframe_unpack_renew(spd, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing spd failed %d\n", ret);
	else
		ret = spd->sdi;

	return ret;
}

static int hdmitx_get_base_refresh_rate_from_emp(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};

	ret = hdmi_emp_infoframe_get(EMP_TYPE_VRR_QMS, body);
	if (ret == -1 || ret == 0)
		return -1;

	ret = ((body[12] & 0x3) << 8) | body[13];

	return ret;
}

static int hdmitx_get_next_tfr_from_emp(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};

	ret = hdmi_emp_infoframe_get(EMP_TYPE_VRR_QMS, body);
	if (ret == -1 || ret == 0)
		return -1;

	ret = (body[12] & 0xf8) >> 3;

	return ret;
}

static int hdmitx_get_m_const_from_emp(struct hdmitx21_dev *hdev)
{
	int ret;
	u8 body[32] = {0};

	ret = hdmi_emp_infoframe_get(EMP_TYPE_VRR_QMS, body);
	if (ret == -1 || ret == 0)
		return -1;

	ret = (body[10] & 0x2) >> 1;

	return ret;
}

static void hdmi_hwp_init(struct hdmitx21_dev *hdev, bool reset)
{
	u32 data32;

	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 20, 1);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1);
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 4, 1);
		hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 7, 8, 3);
		/* tmds clk enable */
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, 1, 24, 1);
	}

	if (!hdmitx21_is_basic_clk_en())
		hdmitx21_set_default_clk();

	/* Bring HDMITX MEM output of power down */
	hd21_set_reg_bits(PWRCTRL_MEM_PD11, 0, 8, 8);
	/* Bring out of reset */
	hdmitx21_wr_reg(HDMITX_TOP_SW_RESET, 0);
	/* Test after initial out of reset, cannot write to IP register, unless enable access */
	hdmitx21_wr_reg(INTR3_MASK_IVCTX, 0xff);
	/* enable access core reg */
	hdmitx21_wr_reg(HDMITX_TOP_SEC_SCRATCH, 1);

	if (!reset && hdmitx21_uboot_already_display(&hdev->tx21_hw)) {
		HDMITX_INFO("uboot already enabled hdmitx\n");
		/* enable fifo intr if uboot hdmitx output ready */
		hdev->ignore_fifo_intr5 = false;
		hdmitx_fifo_flow_enable_intr(1);
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate = get_current_frl_rate();
		if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate >
			hdev->hw_comm.hdmi_tx_cap.tx_max_frl_rate)
			HDMITX_ERROR("current frl_rate %d is larger than tx_max_frl_rate %d\n",
				hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate,
				hdev->hw_comm.hdmi_tx_cap.tx_max_frl_rate);
		#ifdef CONFIG_AMLOGIC_DSC
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en = get_dsc_en();
		#endif
		/* enable audio default */
		audio_mute_op(1);
		return;
	}
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
		// --------------------------------------------------------
		// Program core_pin_mux to enable HDMI pins
		// --------------------------------------------------------
		// [23:20] GPIOW_13_SEL=1 for hdmitx_sda
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 20, 4);
		// [27:24] GPIOW_14_SEL=1 for hdmitx_scl
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 24, 4);
		// [31:28] GPIOW_15_SEL=1 for hdmitx_hpd
		hd21_set_reg_bits(PADCTRL_PIN_MUX_REGN, 1, 28, 4);
	}

	// [8]      hdcp_topology_err
	// [7]      rxsense_fall
	// [6]      rxsense_rise
	// [5]      err_i2c_timeout_pls
	// [4]      hs_intr
	// [3]      core_aon_intr_rise
	// [2]      hpd_fall
	// [1]      hpd_rise
	// [0]      core_pwd_intr_rise
	hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT_CLR, 0x000001ff);

	// Enable internal pixclk, tmds_clk, spdif_clk, i2s_clk, cecclk
	// [   31] free_clk_en
	// [   13] aud_mclk_sel: 0=Use i2s_mclk; 1=Use spdif_clk. For ACR.
	// [   12] i2s_ws_inv
	// [   11] i2s_clk_inv
	// [    9] tmds_clk_inv
	// [    8] pixel_clk_inv
	// [    3] i2s_clk_enable
	// [    2] tmds_clk_enable
	// [    1] tmds_clk_enable
	// [ 0] pixel_clk_enable
	data32 = 0;
	data32 |= (0 << 31);
	data32 |= ((1 - 0) << 13);
	data32 |= (0 << 12);
	data32 |= (0 << 11);
	data32 |= (0 << 9);
	data32 |= (0 << 8);
	data32 |= (1 << 3);
	data32 |= (1 << 2);
	data32 |= (1 << 1);
	data32 |= (1 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_CLK_CNTL,  data32);
	data32 = 0;
	data32 |= (1 << 8);  // [  8] hdcp_topology_err
	data32 |= (1 << 7);  // [  7] rxsense_fall
	data32 |= (1 << 6);  // [  6] rxsense_rise
	data32 |= (1 << 5);  // [  5] err_i2c_timeout_pls
	data32 |= (1 << 4);  // [  4] hs_intr
	data32 |= (1 << 3);  // [  3] core_aon_intr_rise
	data32 |= (1 << 2);  // [  2] hpd_fall_intr
	data32 |= (1 << 1);  // [  1] hpd_rise_intr
	data32 |= (1 << 0);  // [ 0] core_pwd_intr_rise
	hdmitx21_set_bit(HDMITX_TOP_INTR_MASK, BIT(2) | BIT(1), 1);

	//--------------------------------------------------------------------------
	// Configure E-DDC interface
	//--------------------------------------------------------------------------
	data32 = 0;
	data32 |= (1 << 24); // [26:24] infilter_ddc_intern_clk_divide
	data32 |= (0 << 16); // [23:16] infilter_ddc_sample_clk_divide
	hdmitx21_wr_reg(hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5 ?
		HDMITX_S5_TOP_INFILTER : HDMITX_T7_TOP_INFILTER, data32);
	hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 2, 0, 2);
	hdmitx21_set_reg_bits(PCLK2TMDS_MISC0_IVCTX, 0, 0, 2); /* Original DE generation logic */
	/* Control signals for repeat count */
	hdmitx21_set_reg_bits(HBLANK_REKEY_CONTROL_IVCTX, 1, 6, 1);

	/* for 480/576 ddp case, Internal circuit detected Hsync polarity: Use legacy Tx logic */
	hdmitx21_set_reg_bits(HDPLL_FIX0_IVCTX, 0, 0, 1);
	audio_mute_op(1); /* enable audio default */
}

static bool hdmitx_uboot_audio_en(void)
{
	u32 data;

	data = hdmitx21_rd_reg(AUD_EN_IVCTX);
	HDMITX_INFO("%s[%d] data = 0x%x\n", __func__, __LINE__, data);
	return (data & 1) == 1;
}

static bool soc_resolution_limited(const struct tx_timing *timing, u32 res_v)
{
	if (!timing)
		return 0;

	if (timing->v_active > res_v)
		return 0;
	return 1;
}

static bool soc_fresh_rate_limited(const struct tx_timing *timing, u32 vsync)
{
	if (!timing)
		return 0;

	if (timing->v_freq / 1000 > vsync)
		return 0;
	return 1;
}

/* VIC is supported by SOC/IP level */
static int hdmitx_validate_mode(struct hdmitx_hw_common *tx_hw, u32 vic, u32 max_refresh_rate)
{
	int ret = 0;
	const struct tx_timing *timing;
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx21_hw *tx21_hw = &hdev->tx21_hw;

	if (!tx_hw || !tx_hw->chip_data)
		return -EINVAL;

	/*hdmitx21 VESA mode is not supported yet*/
	if (vic == HDMI_0_UNKNOWN || vic > HDMI_CEA_VIC_END)
		return -EINVAL;

	timing = meson_tx_mode_vic_to_timing(vic);
	if (!timing)
		return -EINVAL;

	switch (tx21_hw->base->chip_data->chip_type) {
	case MESON_CPU_ID_S5:
		/* for S5, the MAX capabilities are 8K60, and 4k120, and below */
		ret = (soc_resolution_limited(timing, 4320) &&
			soc_fresh_rate_limited(timing, 60)) ||
			(soc_resolution_limited(timing, 2160) &&
			   soc_fresh_rate_limited(timing, max_refresh_rate));
		break;
	case MESON_CPU_ID_S1A:
		ret = soc_resolution_limited(timing, 1080) && soc_fresh_rate_limited(timing, 60);
		break;
	default:
		ret = (soc_resolution_limited(timing, 2160) &&
			soc_fresh_rate_limited(timing, 60)) ||
			(soc_resolution_limited(timing, 1080) &&
			   soc_fresh_rate_limited(timing, max_refresh_rate));
		break;
	}
	return (ret == 1) ? 0 : -EINVAL;
}

static int hdmitx21_calc_format_para(struct hdmitx_hw_common *tx_hw,
	struct meson_tx_format_para *para)
{
	enum frl_rate_enum tx_max_frl_rate;
	u8 dsc_policy;

	if (!tx_hw || !para)
		return -EINVAL;
	tx_max_frl_rate = tx_hw->hdmi_tx_cap.tx_max_frl_rate;
	dsc_policy = tx_hw->hdmi_tx_cap.dsc_policy;
	/* check current tx para with TMDS mode */
	para->tx_hw_para.hdmitx_hw_para.tmds_clk = hdmitx_calc_tmds_clk(para->timing.pixel_freq,
		para->cs, para->cd);

	if (para->tx_hw_para.hdmitx_hw_para.tmds_clk > 340000) {
		para->tx_hw_para.hdmitx_hw_para.scrambler_en = 1;
		para->tx_hw_para.hdmitx_hw_para.tmds_clk_div40 = 1;
	} else {
		para->tx_hw_para.hdmitx_hw_para.scrambler_en = 0;
		para->tx_hw_para.hdmitx_hw_para.tmds_clk_div40 = 0;
	}

	if (para->tx_hw_para.hdmitx_hw_para.tmds_clk > 600000 && tx_max_frl_rate == FRL_NONE)
		return -EINVAL;

	/* check current tx para with FRL mode */
	para->tx_hw_para.hdmitx_hw_para.frl_rate =
		hdmitx_select_frl_rate(&para->tx_hw_para.hdmitx_hw_para.dsc_en,
			dsc_policy, para->timing.vic, para->cs, para->cd);

	return 0;
}

static void hdmitx_init_gate_bit_mask(struct hdmitx21_dev *hdev)
{
	if (!hdev)
		return;
	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S5:
		hdev->tx21_hw.gate_bit_mask = 0xffc7f;
		break;
	case MESON_CPU_ID_S6:
		hdev->tx21_hw.gate_bit_mask = 0x01c7f;
		break;
	case MESON_CPU_ID_S7:
		hdev->tx21_hw.gate_bit_mask = 0x01c7f;
		break;
	case MESON_CPU_ID_S7D:
		hdev->tx21_hw.gate_bit_mask = 0x01c7f;
		break;
	default:
		hdev->tx21_hw.gate_bit_mask = 0;
		break;
	}
}

/* used for status check when hdmi output setting done */
static int hdmitx21_status_check(void *data)
{
	int clk[3];
	u32 idx[3];
	struct hdmitx21_dev *h21_dev = (struct hdmitx21_dev *)data;
	struct hdmitx_hw_common *hw_comm = &h21_dev->hw_comm;
	u32 arg;

	if (hw_comm->chip_data->chip_type != MESON_CPU_ID_S5)
		return 0;

	/* for S5, here need check the clk index 89 & 16 */
	/* cts_htx_tmds_clk */
	idx[0] = 92;
	/* vid_pll0_clk */
	idx[1] = 16;
	/* htx_tmds20_clk */
	idx[2] = 89;

	while (1) {
		msleep_interruptible(1000);
		if (!h21_dev->tx_comm.ready)
			continue;
		/* skip FRL mode */
		if (hdmitx21_hw_cntl(hw_comm, FRL_GET_MODE, NULL, NULL))
			continue;
		clk[0] = meson_clk_measure(idx[0]);
		clk[1] = meson_clk_measure(idx[1]);
		if (clk[0] && clk[1])
			continue;

		if (!clk[0]) {
			HDMITX_DEBUG("the clock[%d] is %d\n", idx[0], clk[0]);
			arg = idx[0];
			hdmitx21_hw_cntl(hw_comm, PLATFORM_CLK_DIV_RST, (void *)&arg, NULL);
			HDMITX_DEBUG("reset the clock div for %d\n", idx[0]);
			HDMITX_INFO("the clock[%d] is %d\n", idx[0], meson_clk_measure(idx[0]));
			HDMITX_INFO("the clock[%d] is %d\n", idx[2], meson_clk_measure(idx[2]));
		}
		if (!clk[1]) {
			HDMITX_DEBUG("the clock[%d] is %d\n", idx[1], clk[1]);
			arg = idx[1];
			hdmitx21_hw_cntl(hw_comm, PLATFORM_CLK_DIV_RST, (void *)&arg, NULL);
			HDMITX_DEBUG("reset the clock div for %d\n", idx[1]);
			HDMITX_INFO("the clock[%d] is %d\n", idx[1], meson_clk_measure(idx[1]));
		}
		/* resend the SCDC/DIV40 config */
		if (!clk[0] || !clk[1]) {
			clk[0] = meson_clk_measure(idx[0]);
			if (clk[0] >= 340000000)
				arg = 1;
			else
				arg = 0;
			hdmitx21_hw_cntl(hw_comm, DDC_SCDC_DIV40_SCRAMB, (void *)&arg, NULL);
		}
	}
	return 0;
}

static void amhdmitx_infoframe_init(struct hdmitx_common *tx_comm)
{
	int ret = 0;

	ret = hdmi_vendor_infoframe_init(&tx_comm->infoframe.vend.vendor.hdmi);
	if (ret)
		HDMITX_INFO("%s[%d] init vendor infoframe failed %d\n", __func__, __LINE__, ret);
	hdmi_avi_infoframe_init(&tx_comm->infoframe.avi.avi);

	hdmi_audio_infoframe_init(&tx_comm->infoframe.aud.audio);
	hdmi_drm_infoframe_init(&tx_comm->infoframe.drm.drm);
}

static void hdmitx21_private_data_init(struct hdmitx21_dev *h21_dev)
{
	struct hdmitx_common *tx_comm = &h21_dev->tx_comm;

	h21_dev->tx_comm.hdcptx_comm.need_filter_hdcp_off = false;
	/* default 6S */
	h21_dev->tx_comm.hdcptx_comm.filter_hdcp_off_period = 6;

	/* ll mode init values */
	h21_dev->tx_comm.ll_enabled_in_auto_mode = false;
	h21_dev->tx_comm.ll_user_set_mode = HDMI_LL_MODE_AUTO;
	/* vrr function init*/
	h21_dev->vrr_dbg_vframe = -1;

	h21_dev->dfm_type = -1;
	h21_dev->csc_type = 1;
	/* enable analog frequency division by default on S7 and later chip */
	if (h21_dev->tx_comm.tx_hw->chip_data->chip_type >= MESON_CPU_ID_S7)
		h21_dev->tx21_hw.clk_analog_path = 1;

	hdmitx_init_gate_bit_mask(h21_dev);
	/* s5 init status check task */
	if (tx_comm->tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
		tx_comm->task_clk_check = kthread_run(hdmitx21_status_check, (void *)h21_dev,
					"kthread_hdmi_clk_check");
		/* s5 support dsc */
		tx_comm->tx_hw->hdmi_tx_cap.dsc_capable = true;
	}

	amhdmitx_infoframe_init(tx_comm);
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	/* hdcp init */
	tx_comm->hdcptx_comm.bind_hw_comm = (void *)tx_comm->tx_hw;
	tx_comm->hdcptx_comm.bind_tx_comm = (void *)tx_comm;
	tx_comm->hdcptx_comm.event_mgr = (void *)tx_comm->base.event_mgr;
	tx_comm->hdcptx_comm.chip_type = tx_comm->tx_hw->chip_data->chip_type;
	hdcptx_21_ctrl_ops_init(&tx_comm->hdcptx_comm);
	tx_comm->hdcptx_priv =
		tx_comm->tx_hw->chip_data->hdmitx_ops->hdcp_init(&tx_comm->hdcptx_comm);
	tx_comm->hdcptx_comm.task_hdcp = kthread_run(hdmitx_hdcp_stat_monitor_task,
		tx_comm, "kthread_hdcp");
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/* init vrr */
	hdmitx21_vrr_init(h21_dev);
#endif
}

/*
 * note: move global_tx_hw to function parameters
 */
void hdmitx21_meson_init(struct hdmitx_hw_common *hw_comm)
{
	struct hdmitx21_dev *h21_dev = container_of(hw_comm, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &h21_dev->tx_comm;
	u32 arg = CLR_AVMUTE;

	h21_dev->tx21_hw.base = hw_comm;
	h21_dev->tx21_hw.infoframe = &tx_comm->infoframe;

	hw_comm->hw_cntl = hdmitx21_hw_cntl;
	hw_comm->validate_mode = hdmitx_validate_mode;
	hw_comm->calc_format_para = hdmitx21_calc_format_para;
	hw_comm->set_aud_mode = hdmitx_set_audmode;
	hw_comm->setup_irq = hdmitx_setup_irqs;
	hw_comm->debug_func = hdmitx_debug;
	hw_comm->set_disp_mode = hdmitx_set_dispmode;
	hw_comm->get_clk = hdmitx_get_clk;
	hw_comm->pkt_dump = hdmitx_pkt_dump;
	hw_comm->set_vrr_rate = hdmitx_set_vrr_rate;
	hdmitx21_private_data_init(h21_dev);
	hdmi_hwp_init(h21_dev, false);
	hdmitx21_hw_cntl(hw_comm, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
	/* load init audio fmt for HW info */
	hdmitx_audio_init(tx_comm);
}

static void phy_hpll_off(void)
{
	hdmi_phy_suspend();
}

static void hdmitx_phy_pre_init(struct hdmitx21_dev *hdev)
{
	if (!hdev || !hdev->tx_comm.tx_hw->chip_data)
		return;

	/* only need for s5 */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		hdmitx_s5_phy_pre_init(hdev);
#endif
	}
}

static void set_phy_by_mode(u32 mode)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S1A:
		hdmitx_set_phy_by_mode_s1a(mode);
		break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case MESON_CPU_ID_T7:
		hdmitx_set_phy_by_mode_t7(mode);
		break;
	case MESON_CPU_ID_S5:
		hdmitx_set_s5_phypara(hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate,
			hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk);
		break;
	case MESON_CPU_ID_S7D:
		hdmitx_set_phy_by_mode_s7d(mode);
		break;
	case MESON_CPU_ID_S7:
		hdmitx_set_phy_by_mode_s7(mode);
		break;
	case MESON_CPU_ID_S6:
		hdmitx_set_phy_by_mode_s6(mode);
		break;
#endif
	default:
		HDMITX_INFO("%s: Not match chip ID\n", __func__);
		break;
	}
}

static void hdmitx_set_phy(struct hdmitx21_dev *hdev)
{
	u32 phy_addr = 0;

	if (!hdev)
		return;

	phy_addr = ANACTRL_HDMIPHY_CTRL0;
	hd21_write_reg(phy_addr, 0x0);

	phy_addr = ANACTRL_HDMIPHY_CTRL1;

/* P_HHI_HDMI_PHY_CNTL1 bit[1]: enable clock	bit[0]: soft reset */
#define RESET_HDMI_PHY() \
do { \
	hd21_set_reg_bits(phy_addr, 0xf, 0, 4); \
	mdelay(2); \
	hd21_set_reg_bits(phy_addr, 0xe, 0, 4); \
	mdelay(2); \
} while (0)

	hd21_set_reg_bits(phy_addr, 0x0390, 16, 16);
	hd21_set_reg_bits(phy_addr, 0x1, 17, 1);
	hd21_set_reg_bits(phy_addr, 0x0, 17, 1);
	hd21_set_reg_bits(phy_addr, 0x0, 0, 4);
	msleep(20);
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
	RESET_HDMI_PHY();
#undef RESET_HDMI_PHY

	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk > 450000)
		set_phy_by_mode(HDMI_PHYPARA_6G);
	else if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk > 380000)
		set_phy_by_mode(HDMI_PHYPARA_4p5G);
	else if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk > 300000)
		set_phy_by_mode(HDMI_PHYPARA_3p7G);
	else if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk > 150000)
		set_phy_by_mode(HDMI_PHYPARA_3G);
	else if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.tmds_clk > 40000)
		set_phy_by_mode(HDMI_PHYPARA_LT3G);
	else
		set_phy_by_mode(HDMI_PHYPARA_DEF);
}

/* vid_pll_clk for master clk
 * RA bit[18:16] source 0: vid_pll0_clk
 * RG[7:0] XD0
 * RA[2:0] DIV4/2/1_EN
 */
static void set_vid_clk_div(u32 div)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 0, 16, 3);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div - 1, 0, 8);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 7, 0, 3);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D) {
		//bit[18:16]:sel clk source.3'h0:vid_pll_clk, 3'h3:vid_pix_clk
		//[49]hdmi_vx1_pix_clk
		if (hdev->tx21_hw.clk_analog_path)
			hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 3, 16, 3);// select vid_pix_clk
	}
}

/* RD bit[5]:pixel clk gate enable */
static void set_hdmi_tx_pixel_div(u32 div)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 5, 1);
	//for s7
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6) {
		if (hdev->tx_comm.fmt_para.cs == HDMI_COLORSPACE_YUV420) {
			hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 1, 1);
			hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 1, 16, 1);
		} else {
			hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, 0, 16, 4);
		}
	}
}

/*
 * RG [27:24] ENCP_CLK_SEL no need for S5? S5 use RE[15:12] as div
 * RA bit[19]: CLK_EN0
 */
static void set_encp_div(u32 div)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, div, 24, 4);
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, 19, 1);
}

/*
 * RD bit[2] ENCP CLK GATE, but for clk tree of S5,
 * RD bit[3] is for gate of ENC, not bit[2].
 */
static void hdmitx_enable_encp_clk(void)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 2, 1);
}

static void hdmitx_enable_enci_clk(void)
{
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_DIV, 0, 31, 1);	//dummy & cvbs will set 1
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 0, 1);
}

static void set_hdmitx_fe_clk(void)
{
	u32 tmp = 0;
	u32 vid_clk_cntl2;
	u32 vid_clk_div;
	u32 hdmi_clk_cntl;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	/* RD bit[9]: Gclk_hdmi_tx_fe_clk */
	vid_clk_cntl2 = CLKCTRL_VID_CLK0_CTRL2;
	/* RG [27:24] ENCP_CLK_SEL no need for S5? */
	vid_clk_div = CLKCTRL_VID_CLK0_DIV;
	/* register wrong for S5, CLKCTRL_ENC0_HDMI_CLK_CTRL?
	 * CLKCTRL_HDMI_CLK_CTRL bit[23:20] reserved in S5
	 */
	hdmi_clk_cntl = CLKCTRL_HDMI_CLK_CTRL;

	hd21_set_reg_bits(vid_clk_cntl2, 1, 9, 1);
	/* divider same as encp */
	tmp = (hd21_read_reg(vid_clk_div) >> 24) & 0xf;
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S1A)
		hd21_set_reg_bits(hdmi_clk_cntl, 0, 20, 4);
	else
		hd21_set_reg_bits(hdmi_clk_cntl, tmp, 20, 4);
}

static void _hdmitx21_set_clk(void)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	set_vid_clk_div(1);
	set_hdmi_tx_pixel_div(1);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S1A ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6)
		set_encp_div(0);
	else
		set_encp_div(1);
	set_hdmitx_fe_clk();
}

void enable_crt_video_encl2(u32 enable, u32 in_sel)
{
	//encl_clk_sel:hi_viid_clk_div[15:12]
	hd21_set_reg_bits(CLKCTRL_VIID_CLK2_DIV, in_sel, 12, 4);
	if (in_sel <= 4) { //V1
	//#if (SDF_CORNER == 0 || SDF_CORNER == 2)    //ss_corner
	//      hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL, 1, 16, 3);  //sel div4 : 500M
	//#endif
		hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL, 3, 0, 2);
	} else {
		hd21_set_reg_bits(CLKCTRL_VIID_CLK2_CTRL, 1, in_sel - 8, 1);
	}
	//gclk_encl_clk:hi_vid_clk_cntl2[3]
	hd21_set_reg_bits(CLKCTRL_VID_CLK2_CTRL2, enable, 3, 1); /* cts_enc2_clk */
}

//Enable CLK_ENCL
void enable_crt_video_encl(u32 enable, u32 in_sel)
{
	/* RE bit[15:12] encl_clk_sel, 0: vid_pll0_clk */
	//encl_clk_sel:hi_viid_clk_div[15:12]
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, in_sel,  12, 4);
	if (in_sel <= 4) { //V1
		//#if (SDF_CORNER == 0 || SDF_CORNER == 2)    //ss_corner
		//      hd21_set_reg_bits(CLKCTRL_VID_CLK_CTRL, 1, 16, 3);  //sel div4 : 500M
		//#endif
		/* RA bit[1:0] div2/1 */
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL, 1, in_sel, 1);
	} else {
		hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, in_sel - 8, 1);
	}
	/* RD bit[3]: ENCL gated clock */
	//gclk_encl_clk:hi_vid_clk_cntl2[3]
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, enable, 3, 1);
}

//Enable CLK_ENCP, no use for S5, already configure previously
void enable_crt_video_encp(u32 enable, u32 in_sel)
{
	enable_crt_video_encl(enable, in_sel);
}

/* check the h_total with depth
 * for example, VIC4, 720p60hz
 * htotal will be 1650/8bit, 2062.5/10bit, 2475/12bit under tmds
 * htotal will be 825/8bit, 1031.25/10bit, 1237.5/12bit under frl
 * which will has the fraction.
 * Under such case, the GCP phase will be dynamic value
 */
static bool is_deep_htotal_frac(bool frl_mode, u32 h_total,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd)
{
	if (frl_mode) {
		if (cs == HDMI_COLORSPACE_YUV420) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 4)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 16)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 8)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV444 || cs == HDMI_COLORSPACE_RGB) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 2)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 8)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 4)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV422) {
			if (h_total % 2)
				return 1;
		}
	} else {
		if (cs == HDMI_COLORSPACE_YUV420) {
			if (cd == COLORDEPTH_24B) {
				if (h_total % 2)
					return 1;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 8)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 4)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV444 || cs == HDMI_COLORSPACE_RGB) {
			if (cd == COLORDEPTH_24B) {
				return 0;
			} else if (cd == COLORDEPTH_30B) {
				if (h_total * 5 % 4)
					return 1;
			} else if (cd == COLORDEPTH_36B) {
				if (h_total * 3 % 2)
					return 1;
			}
		} else if (cs == HDMI_COLORSPACE_YUV422) {
			return 0;
		}
	}
	return 0;
}

static bool is_deep_phase_unstable(enum hdmi_colorspace cs, enum hdmi_color_depth cd)
{
	u8 gcp_cur_st = (hdmitx21_rd_reg(GCP_CUR_STAT_IVCTX) >> 5) & 0x3;

	HDMITX_DEBUG("%s[%d] gcp_cur_st %d\n", __func__, __LINE__, gcp_cur_st);
	if (cs == HDMI_COLORSPACE_YUV422) {
		if (gcp_cur_st != 0)
			return 1;
	} else {
		if (cd == COLORDEPTH_36B) {
			if (gcp_cur_st != 0x2)
				return 1;
		} else {
			if (gcp_cur_st)
				return 1;
		}
	}

	return 0;
}

//Enable HDMI_TX_PIXEL_CLK
//Note: when in_sel == 15, select tcon_clko
void enable_crt_video_hdmi(u32 enable, u32 in_sel, u8 enc_sel)
{
	u32 data32;
	u32 addr_vid_clk02;
	u32 addr_viid_clk02;
	u32 addr_vid_clk022;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	addr_vid_clk02 = (enc_sel == 0) ? CLKCTRL_VID_CLK0_CTRL : CLKCTRL_VID_CLK2_CTRL;
	addr_viid_clk02 = (enc_sel == 0) ? CLKCTRL_VIID_CLK0_CTRL : CLKCTRL_VIID_CLK2_CTRL;
	addr_vid_clk022 = (enc_sel == 0) ? CLKCTRL_VID_CLK0_CTRL2 : CLKCTRL_VID_CLK2_CTRL2;

	if (in_sel <= 4) { //V1
		if (in_sel == 1)
			// If 420 mode, need to turn on div1_clk for hdmi_tx_fe_clk
			// For hdmi_tx_fe_clk and hdmi_tx_pnx_clk
			hd21_set_reg_bits(addr_vid_clk02, 3, 0, 2);
	} else if (in_sel <= 9) { //V2
		// For hdmi_tx_pixel_clk
		hd21_set_reg_bits(addr_viid_clk02, 1, in_sel - 8, 1);
	}

	/* RD bit[10,9,5] for pnx/fe/pixel clk gate */
	// Enable hdmi_tx_pnx_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 10, 1);
	// Enable hdmi_tx_fe_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 9, 1);
	// Enable hdmi_tx_pixel_clk
	hd21_set_reg_bits(addr_vid_clk022, enable, 5, 1);

	/* RF */
	// [22:21] clk_sl: 0=enc0_hdmi_tx_pnx_clk, 1=enc2_hdmi_tx_pnx_clk.
	// [   20] clk_en for hdmi_tx_pnx_clk
	// [19:16] clk_div for hdmi_tx_pnx_clk
	// [14:13] clk_sl: 0=enc0_hdmi_tx_fe_clk, 1=enc2_hdmi_tx_fe_clk.
	// [   12] clk_en for hdmi_tx_fe_clk
	// [11: 8] clk_div for hdmi_tx_fe_clk
	// [ 6: 5] clk_sl: 0=enc0_hdmi_tx_pixel_clk, 1=enc2_hdmi_tx_pixel_clk.
	// [    4] clk_en for hdmi_tx_pixel_clk
	// [ 3: 0] clk_div for hdmi_tx_pixel_clk

	/* this bit13 of CLKCTRL_ENC_HDMI_CLK_CTRL will select source from
	 * enc0_hdmi_tx_fe_clk, while it need to select source from gp2
	 */
	data32 = 0;
	data32 = (enc_sel << 21) |
		 (0 << 20) |
		 (0 << 16) |
		 (enc_sel << 13) |
		 (0 << 12) |
		 (0 << 8) |
		 (enc_sel << 5) |
		 (0 << 4) |
		 (0 << 0);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type >= MESON_CPU_ID_S5 &&
	    (para->cs == HDMI_COLORSPACE_YUV420 &&
	    !para->tx_hw_para.hdmitx_hw_para.frl_rate)) {
		data32 |= (1 << 0); /* pixel_clk DIV */
		data32 |= (1 << 8); /* fe_clk DIV */
		data32 |= (1 << 16); /* pnx_clk DIV */
	}
	hd21_write_reg(CLKCTRL_ENC_HDMI_CLK_CTRL, data32);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 20, 1);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1);
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 4, 1);
}   // enable_crt_video_hdmi

//Enable CLK_ENCP
void enable_crt_video_encp2(u32 enable, u32 in_sel)
{
	enable_crt_video_encl2(enable, in_sel);
}

/* master_clk selects source from vid_pll0_clk or vid_pll1_clk(fpll_pixel_clk)
 * under FRL mode, no matter DSC or non-DSC, select fpll_pixel_clk
 * it's used for pixel/pnx/enc/fe clk under frl & non-DSC mode;
 * and used for pixel/pnx clk under frl & DSC mode (enc_clk mux to v2_master_clk
 * and fe_clk mux to GP2_pll, refer to hdmitx_mux_gp2_pll);
 */
static void hdmitx_mux_vid_pll_clk(struct hdmitx21_dev *hdev)
{
	/* RA bit[18:16] vid_pll_clk source: 0 vid_pll0_clk, 4 vid_pll1_clk */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL,
			hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate ? 4 : 0, 16, 3);
}

/* In the DSC mode, the v2_master_clk will choose gp2_pll_clk */
/* this mux is for enc_clk & fe_clk
 * note enc_clk need go through CRT_VIDEO, while fe_clk can
 * directly mux to gp2_pll
 */
static void hdmitx_mux_gp2_pll(struct hdmitx21_dev *hdev)
{
	/* RB bit[18:16] V2_cntl_clk_in_sel
	 * bit[19] V2_cntl_clk_en0, bit[0]:V2_cntl_div1_en
	 * select gp2_pll_clk->v2_master_clk->enc_clk div1
	 */
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 16, 3);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 19, 1);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_CTRL, 1, 0, 1);

	/* RE bit[15:12] enc_clk mux to v2_master_clk
	 * bit[7:0] V2_cntl_xd0
	 * bit[16] V2_cntl_clk_div_en
	 */
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 8, 12, 4);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 0, 0, 8);
	hd21_set_reg_bits(CLKCTRL_VIID_CLK0_DIV, 1, 16, 1);

	/* RD bit[3] ENC gate enable */
	hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, 1, 3, 1);

	/* RF fe_clk mux to gp2_pll_clk */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 13, 2); /* mux */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 1, 12, 1); /* en */
	hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, 0, 8, 4); /* div */
}

static void set_hdmitx_enc_idx(unsigned int val)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, CONF_ENC_IDX, 1, !!val, 0, 0, 0, 0, &res);
}

//new future for s1a,s7 and s7d
static void vpu_hdmi_set_matrix_ycbcr2rgb(void)
{
	//regVPP_MATRIX_COEF00_01 =VPU_HDMI_MATRIX_COEF00_01;
	//regVPP_MATRIX_COEF02_10 =VPU_HDMI_MATRIX_COEF02_10;
	//regVPP_MATRIX_COEF11_12 =VPU_HDMI_MATRIX_COEF11_12;
	//regVPP_MATRIX_COEF20_21 =VPU_HDMI_MATRIX_COEF20_21;
	//regVPP_MATRIX_COEF22 =VPU_HDMI_MATRIX_COEF22;
	//regVPP_MATRIX_PRE_OFFSET0_1 = VPU_HDMI_MATRIX_PRE_OFFSET0_1;
	//regVPP_MATRIX_PRE_OFFSET2 = VPU_HDMI_MATRIX_PRE_OFFSET2;
	//regVPP_MATRIX_OFFSET0_1 =VPU_HDMI_MATRIX_OFFSET0_1;
	//regVPP_MATRIX_OFFSET2 =VPU_HDMI_MATRIX_OFFSET2;
	//regVPP_MATRIX_EN_CTRL = VPU_HDMI_MATRIX_EN_CTRL;

	HDMITX_INFO("ycbcr2rgb matrix\n");
	//ycbcr not full range, 601 conversion		/* vd1/post */
	hd21_write_reg(VPU_HDMI_MATRIX_PRE_OFFSET0_1,
		(0x1e00 << 16) | 0x1fc0);
	hd21_write_reg(VPU_HDMI_MATRIX_PRE_OFFSET2, 0x1e00);

	/*
	 * -512  [1577    1024    0   ]   [Cr] [64]   [R]
	 * -64   [-469    1024    -188] * [Y ] [64] = [G]
	 * -512  [0       1024    1858]   [Cb] [64]   [B]
	 */
	hd21_write_reg(VPU_HDMI_MATRIX_COEF00_01, (0x0615 << 16) | 0x0400);
	hd21_write_reg(VPU_HDMI_MATRIX_COEF02_10, (0x0000 << 16) | 0x1e2b);
	hd21_write_reg(VPU_HDMI_MATRIX_COEF11_12, (0x0400 << 16) | 0x1f44);
	hd21_write_reg(VPU_HDMI_MATRIX_COEF20_21, (0x0000 << 16) | 0x0400);
	hd21_write_reg(VPU_HDMI_MATRIX_COEF22, 0x0742);
	hd21_write_reg(VPU_HDMI_MATRIX_OFFSET0_1, (0x0040 << 16) | 0x0040);
	hd21_write_reg(VPU_HDMI_MATRIX_OFFSET2, 0x0040);

	//enable matrix_ycbcr2rgb
	hd21_set_reg_bits(VPU_HDMI_FMT_CTRL, 3, 0, 2);
}

/* set to 1 only for cvtem packet test */
static int emp_dbg_en;

void hdmitx_soft_reset(u32 bits_nr)
{
	if (bits_nr & BIT(0)) {
		/* 18or10to20 fifos Software reset */
		hdmitx21_set_reset_reg_bit(PWD_SRST_IVCTX, 2);
	}
	if (bits_nr & BIT(1)) {
		/* Software Reset. Reset all internal logic */
		hdmitx21_set_reset_reg_bit(PWD_SRST_IVCTX, 0);
	}
	if (bits_nr & BIT(2)) {
		/* reset for the cipher engine */
		hdmitx21_set_reset_reg_bit(HDCP_CTRL_IVCTX, 2);
	}
	if (bits_nr & BIT(3)) {
		/* HW TPI State Machine Reset */
		hdmitx21_set_reset_reg_bit(AON_CYP_CTL_IVCTX, 3);
	}
	if (bits_nr & BIT(4)) {
		/* Software Reset for hdcp2x logic only */
		hdmitx21_set_reset_reg_bit(HDCP2X_TX_SRST_IVCTX, 5);
	}
	if (bits_nr & BIT(5)) {
		/* PCLK to TCLK Video FIFO Software reset */
		hdmitx21_set_reset_reg_bit(PWD_SRST_IVCTX, 1);
		/* clear pfifo intr */
		hdmitx21_set_reg_bits(INTR2_SW_TPI_IVCTX, 1, 1, 1);
	}
}

//new future s7d & s6
static void vpu_hdmi_set_matrix_rgb2ycbcr(void)
{
	HDMITX_INFO("rgb2ycbcr matrix\n");

	//ycbcr not full range, 601 conversion
	if (1) {
		//[0.439	 -0.368 -0.071]   [R]	[Cr]
		//[0.257	 0.504	 0.098] * [G]	[Y ]
		//[-0.148	 -0.291  0.439]   [B] = [Cb]
		hd21_write_reg(VPU_HDMI_MATRIX_PRE_OFFSET0_1, 0x0);
		hd21_write_reg(VPU_HDMI_MATRIX_PRE_OFFSET2, 0x0);
		hd21_write_reg(VPU_HDMI_MATRIX_COEF00_01, (0x1c2 << 16) | 0x1e87);
		hd21_write_reg(VPU_HDMI_MATRIX_COEF02_10, (0x1fb7 << 16) | 0x107);
		hd21_write_reg(VPU_HDMI_MATRIX_COEF11_12, (0x204 << 16) | 0x64);
		hd21_write_reg(VPU_HDMI_MATRIX_COEF20_21, (0x1f68 << 16) | 0x1ed6);
		hd21_write_reg(VPU_HDMI_MATRIX_COEF22, 0x1c2);
		hd21_write_reg(VPU_HDMI_MATRIX_OFFSET0_1, ((0x0200)  << 16) | (0x40)); //vd1/post
		hd21_write_reg(VPU_HDMI_MATRIX_OFFSET2, (0x0200));
	} else {
		//ycbcr full range, 601 conversion
	}
	//enable matrix_rgb2ycbcr
	hd21_set_reg_bits(VPU_HDMI_FMT_CTRL, 0, 0, 2);
}

static int CSC_type = 1;
module_param(CSC_type, int, 0644);
MODULE_PARM_DESC(CSC_type, "for choose VPU_HDMI_if function");

/* need enable phy to digital and keep tmds clk */
static void hdmitx_set_phy_todig(struct hdmitx21_dev *hdev)
{
	switch (hdev->tx21_hw.base->chip_data->chip_type) {
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0, 1, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0, 3, 1);
		usleep_range(100, 110);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 0, 2);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 3, 1);
		break;
	case MESON_CPU_ID_T7:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 0, 2);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 3, 1);
		break;
	default:
		/* pr_info("not match chip type to enable phy to dig\n"); */
		return;
	}
	HDMITX_INFO("enable phy to dig\n");
}

static int hdmitx_set_dispmode(struct hdmitx_hw_common *tx_hw, struct meson_tx_format_para *para)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	u32 data32;
	enum hdmi_vic vic;

	if (!tx_hw || !para) {
		HDMITX_ERROR("%s NULL HW instance or format para\n", __func__);
		return -1;
	}
	vic = para->timing.vic;
	hdmitx_fifo_flow_enable_intr(0);
	/* gp2 setting has been set for fe/enc for dsc*/
	hdmitx21_set_clk(hdev);

	hdmitx_phy_pre_init(hdev);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S1A ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6)
		_hdmitx21_set_clk();

	/* [92]cts_htx_tmds clk config */
	hdmitx_set_clkdiv(hdev);
	hdmitx_mux_vid_pll_clk(hdev);

	if (hdev->tx_comm.enc_idx == 2) {
		set_hdmitx_enc_idx(2);
		hd21_set_reg_bits(VPU_DISP_VIU2_CTRL, 1, 29, 1);
		//hd21_set_reg_bits(VPU_VIU_VENC_MUX_CTRL, 2, 2, 2);
	}
	/* dsc program step8.1: Make sure VENC timing gen is disabled. */
	hdmitx21_venc_en(0, 0);
	hdmitx21_venc_en(0, 1);
	hd21_set_reg_bits(VPU_HDMI_SETTING, 0, (hdev->tx_comm.enc_idx == 0) ? 0 : 1, 1);
	/* dsc program step8.3: Program VPU/HDMI setting. */
	/* below crt_video_encp* is ENC for T7 */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
		if (hdev->tx_comm.enc_idx == 0)
			enable_crt_video_encp(1, 0);
		else
			enable_crt_video_encp2(1, 0);
	}
	/* set divider for pixel/pnx/fe clk and gate */
	/* it will override above set for enc/fe for DSC */
	enable_crt_video_hdmi(1,
			      (TX_INPUT_COLOR_FORMAT == HDMI_COLORSPACE_YUV420) ? 1 : 0,
			      hdev->tx_comm.enc_idx);
	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en)
		hdmitx_mux_gp2_pll(hdev);
	// configure GCP
	/* for 8bit depth or y422: non-merge gcp mode + clr_avmute,
	 * for dc mode: merge gcp mode + clr_avmute
	 */
	if (para->cs == HDMI_COLORSPACE_YUV422 ||
		para->cd == COLORDEPTH_24B ||
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
		hdmitx21_set_reg_bits(GCP_CNTL_IVCTX, 0, 0, 1);
		/* hdmi_gcppkt_manual_set(1); */
	} else {
		/* hdmi_gcppkt_manual_set(0); */
		hdmitx21_set_reg_bits(GCP_CNTL_IVCTX, 1, 0, 1);
	}
	// --------------------------------------------------------
	// Enable viu vsync interrupt, enable hdmitx interrupt, enable htx_hdcp22 interrupt
	// --------------------------------------------------------
	hd21_write_reg(VPU_VENC_CTRL, 1);
	if (hdev->tx_comm.enc_idx == 2) {
		// Enable VENC2 to HDMITX path
		hd21_set_reg_bits(SYSCTRL_VPU_SECURE_REG0, 1, 16, 1);
	}

	// --------------------------------------------------------
	// Set TV encoder for HDMI
	// --------------------------------------------------------
	//              enc_index   output_type enable)
	// only 480i / 576i use the ENCI
	if (para->timing.pi_mode == 0 &&
	    (para->timing.v_active == 480 || para->timing.v_active == 576)) {
		hd21_write_reg(VPU_VENC_CTRL, 0); // sel enci timming
		hdmitx_set_tv_enci_new(hdev, hdev->tx_comm.enc_idx, vic, 1);
		/* for s1a, s7 */
		hdmitx_enable_enci_clk();
	} else {
		hd21_write_reg(VPU_VENC_CTRL, 1); // sel encp timming
		hdmitx_set_tv_encp_new(hdev, hdev->tx_comm.enc_idx, vic, 1);
		/* for s1a, s7 */
		hdmitx_enable_encp_clk();
	}

	// --------------------------------------------------------
	// Configure video format timing for HDMI:
	// Based on the corresponding settings in set_tv_enc.c, calculate
	// the register values to meet the timing requirements defined in CEA-861-D
	// --------------------------------------------------------
	HDMITX_DEBUG("configure hdmitx video format timing\n");

	vpu_hdmi_fmt_config(hdev);

	hdmitx21_dither_config(hdev);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
		hdmi_hwp_init(hdev, true);

	// Set this timer very small on purpose, to test the new function
	hdmitx21_wr_reg(HDMITX_TOP_I2C_BUSY_CNT_MAX,  30);

	data32 = 0;
	data32 |= (1 << 31); // [   31] cntl_hdcp14_min_size_v_en
	data32 |= (240 << 16); // [28:16] cntl_hdcp14_min_size_v
	data32 |= (1 << 15); // [   15] cntl_hdcp14_min_size_h_en
	data32 |= (640 << 0);  // [13: 0] cntl_hdcp14_min_size_h
	hdmitx21_wr_reg(HDMITX_TOP_HDCP14_MIN_SIZE, data32);

	data32 = 0;
	data32 |= (1 << 31); // [   31] cntl_hdcp22_min_size_v_en
	data32 |= (1080 << 16); // [28:16] cntl_hdcp22_min_size_v
	data32 |= (1 << 15); // [   15] cntl_hdcp22_min_size_h_en
	data32 |= (1920 << 0);  // [13: 0] cntl_hdcp22_min_size_h
	hdmitx21_wr_reg(HDMITX_TOP_HDCP22_MIN_SIZE, data32);

	hdmitx_soft_reset(RESET_SOFTWARE_LOGIC | RESET_CIPHER_ENGINE |
		RESET_HW_TPI | RESET_HDCP2X_LOGIC);
	//[4] reg_bypass_video_path
	// For non-DSC, set to bit4 as 0
	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en)
		hdmitx21_set_reg_bits(PCLK2TMDS_MISC1_IVCTX, 1, 4, 1);
	else
		hdmitx21_set_reg_bits(PCLK2TMDS_MISC1_IVCTX, 0, 4, 1);

	/* only for s5, this register source is off on other chips */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
		/* block hsync, this is need to enable when in DSC mode */
		if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en)
			hdmitx21_set_reg_bits(H21TXSB_CTRL_1_IVCTX, 1, 2, 1);
		else
			hdmitx21_set_reg_bits(H21TXSB_CTRL_1_IVCTX, 0, 2, 1);
	}

	hdmitx_set_hw(hdev);

	/* dsc program step8.4: Configure VENC timing gen to be slave mode
	 * (receive hs/vs sync signal from DSC encoder timing gen)
	 */
	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en)
		hd21_set_reg_bits(ENCP_VIDEO_SYNC_MODE, 1, 4, 1);
	else
		hd21_set_reg_bits(ENCP_VIDEO_SYNC_MODE, 0, 4, 1);

	/* move enable venc to the end */
	/* if (para->timing.pi_mode == 0 && */
		/* (para->timing.v_active == 480 || para->timing.v_active == 576)) */
		/* hdmitx21_venc_en(1, 0); */
	/* else */
		/* hdmitx21_venc_en(1, 1); */

	vpu_hdmi_setting_config(hdev);

	/* for s7/s1a/s7d/s6, ycbcr -> rgb */
	if (para->cs == HDMI_COLORSPACE_RGB) {
		switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
		case MESON_CPU_ID_S1A:
		case MESON_CPU_ID_S7:
			vpu_hdmi_set_matrix_ycbcr2rgb();
			break;
		case MESON_CPU_ID_S7D:
		case MESON_CPU_ID_S6:
			if (hdev->csc_type == yuv2rgb)
				vpu_hdmi_set_matrix_ycbcr2rgb();
			else if (hdev->csc_type == rgb2yuv)
				vpu_hdmi_set_matrix_rgb2ycbcr();
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_AMLOGIC_DSC
	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
		hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &para->timing);
		/* dsc program step8.5: Program DSC settings. */
		/* dsc program step8.6: Enable DSC encoder timing gen 'reg_tmg_en' */
		aml_dsc_enable(true);
	}

	if (emp_dbg_en) {
		/* only for cvtem packet debug test */
		hdmitx_get_dsc_data(&hdev->dsc_data);
		hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &para->timing);
	}
#endif

	/* enable decouple fifo and venc tmg */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
		hd21_set_reg_bits(VPU_HDMI_SETTING, 1, 0, 1);
	usleep_range(1000, 1005);
	if (para->timing.pi_mode == 0 &&
		(para->timing.v_active == 480 || para->timing.v_active == 576)) {
		hdmitx21_venc_en(1, 0);
	} else {
		hdmitx21_venc_en(1, 1);
	}
	/* when the mode setting fails, it is necessary to ensure
	 * that vsync is enabled so that subsequent actions are not
	 * blocked. Phy cannot be enabled to avoid affecting TV
	 */
	if (hdev->tx_comm.skip_phy_setting)
		return -1;

	hdmitx_set_phy_todig(hdev);
	/*
	 * when GCP phase is a fix value, here need check the phase is stable or not
	 * otherwise it may cause display flash and abnormal issue
	 */
	{
		enum hdmi_colorspace cs = para->cs;
		enum hdmi_color_depth cd = para->cd;
		unsigned int h_total = para->timing.h_total;
		bool h_unstable = 0;
		int loop = 20;

		/*check the GCP phase is a fix value*/
		h_unstable = is_deep_htotal_frac(0, h_total, cs, cd);
		HDMITX_INFO("%s[%d] frl_rate %d htotal %d cs %d cd %d h_unstable %d\n",
			__func__, __LINE__, get_current_frl_rate(), h_total, cs, cd, h_unstable);
		if (!h_unstable) {
			while (loop--) {
				hdmitx21_poll_reg(SYS_STAT_IVCTX, 1 << 0, ~(1 << 0),
						HZ / 100);
				if (is_deep_phase_unstable(cs, cd)) {
					/* reset pfifo */
					hdmitx_soft_reset(RESET_PFIFO);
					continue;
				} else {
					break;
				}
			}
			if (loop < 0)
				HDMITX_ERROR("%s[%d] phase is still unstable\n", __func__,
				__LINE__);
		}
	}

	hdmitx_set_phy(hdev);
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		hdmitx_dfm_cfg(0, 0);
		if (para->tx_hw_para.hdmitx_hw_para.frl_rate) {
			u32 tri_bytes_per_line = 0;
			bool ret = 0;
			int hc_active = 0;

			if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
#ifdef CONFIG_AMLOGIC_DSC
				hc_active = dsc_get_hc_active_by_mode(hdev->dsc_data.dsc_mode);
				/* the pixel_clk is for dual pixel, need double when calculate */
				ret = frl_check_full_bw(HDMI_COLORSPACE_YUV444, COLORDEPTH_24B,
					hdev->dsc_data.cts_hdmi_tx_pixel_clk / 1000 * 2, hc_active,
					para->tx_hw_para.hdmitx_hw_para.frl_rate,
					&tri_bytes_per_line);
#endif
			} else {
				ret = frl_check_full_bw(para->cs, para->cd,
					para->timing.pixel_freq, para->timing.h_active,
					para->tx_hw_para.hdmitx_hw_para.frl_rate,
					&tri_bytes_per_line);
			}
			/* manual mode */
			if (hdev->dfm_type == 1) {
				hdmitx_dfm_cfg(1, tri_bytes_per_line);
			} else if (hdev->dfm_type == 2) {
				hdmitx_dfm_cfg(2, 0);
			} else if (hdev->dfm_type == 0) {
				hdmitx_dfm_cfg(0, 0);
			} else {
				if (ret)
					hdmitx_dfm_cfg(1, tri_bytes_per_line);
				else
					hdmitx_dfm_cfg(2, 0);
			}
			HDMITX_INFO("%s hc_active: %d, full_bw: %d, tri_byte: %d, dfm_type: %d\n",
				__func__, hc_active, ret, tri_bytes_per_line, hdev->dfm_type);
		} else {
			HDMITX_INFO("cts_htx_tmds_clk:%d, hdmi_clk_todig:%d, do 10to20 fifo rst\n",
				 meson_clk_measure(92), meson_clk_measure(93));
			hdmitx_soft_reset(RESET_18OR10TO20);
			/* for mode switch flow, add 2ms delay after fifo reset
			 * before enable fifo intr, to prevent fifo reset operation
			 * itself trigger new fifo over/under flow
			 */
			msleep_interruptible(2);
			hdev->ignore_fifo_intr5 = false;
			/* there's fifo intr state clear before enable fifo intr */
			hdmitx_fifo_flow_enable_intr(1);
		}

		/* fix dsc snow screen issue and dsc cts HFR1-85,Iter-04 snow screen:
		 * reset pfifo before training
		 */
		if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
			hdmitx_soft_reset(RESET_PFIFO);
			/* clear pfifo intr */
			hdmitx21_set_reg_bits(INTR2_SW_TPI_IVCTX, 1, 1, 1);
		}
		if (hdev->tx_comm.base.rxcap.max_frl_rate &&
			para->tx_hw_para.hdmitx_hw_para.frl_rate)
			frl_tx_training_handler(hdev);
#endif
	} else {
		hdmitx_fifo_flow_enable_intr(1);
	}
	return 0;
}

static enum hdmi_tf_type hdmitx21_get_cur_hdr_st(void)
{
	int ret;
	u8 body[32] = {0};
	enum hdmi_tf_type tf_type = HDMI_NONE;
	enum hdmi_eotf type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	union hdmi_infoframe info;
	struct hdmi_drm_infoframe *drm = &info.drm;

	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_DRM, body);
	if (ret == -1 || ret == 0) {
		type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	} else {
		ret = hdmi_infoframe_unpack(&info, body, sizeof(body));
		if (ret == 0)
			type = drm->eotf;
	}
	if (type == HDMI_EOTF_SMPTE_ST2084)
		tf_type = HDMI_HDR_SMPTE_2084;
	else if (type == HDMI_EOTF_BT_2100_HLG)
		tf_type = HDMI_HDR_HLG;
	else if (type == HDMI_EOTF_TRADITIONAL_GAMMA_SDR)
		tf_type = HDMI_HDR_SDR;
	else if (type == HDMI_EOTF_TRADITIONAL_GAMMA_HDR)
		tf_type = HDMI_HDR_HDR;
	else
		tf_type = HDMI_HDR_TYPE;
	return tf_type;
}

/* used for DV VSIF/HDR10+/CUVA check
 * note: DV_VSIF/HDR10+_VSIF/HDMI14_VSIF/CUVA and HF_VSIF use
 * different VSIF per DV CTS spec, use ifdb_present and
 * additional_vsif_num to decide the position of HF_VSIF
 */
static bool hdmitx_vsif_en(u8 *body)
{
	int ret;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	if (hdev->tx_comm.base.rxcap.ifdb_present &&
			hdev->tx_comm.base.rxcap.additional_vsif_num >= 1)
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR, body);
	else if (!hdev->tx_comm.base.rxcap.ifdb_present)
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	else
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR, body);
	if (ret == -1 || ret == 0)
		return 0;
	else
		return 1;
}

static enum hdmi_tf_type hdmitx21_get_cur_dv_st(void)
{
	int ret;
	u8 body[32] = {0};
	enum hdmi_tf_type type = HDMI_NONE;
	union hdmi_infoframe info;
	/* struct hdmi_vendor_infoframe *vend = (struct hdmi_vendor_infoframe *)&info; */
	struct hdmi_avi_infoframe *avi = (struct hdmi_avi_infoframe *)&info;
	unsigned int ieee_code = 0;
	unsigned int size = 0;
	unsigned int amdv_signal = 0;
	enum hdmi_colorspace cs = 0;

	if (!hdmitx_vsif_en(body))
		return type;

	/* ret = hdmi_infoframe_unpack(&info, body, sizeof(body)); */
	/* if (ret) */
		/* return type; */
	/* ieee_code = vend->oui; */
	/* size = vend->length; */

	/* check Packet type */
	if (body[0] != 0x81)
		return type;

	size = body[2];
	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	amdv_signal = body[7] & 0x3;

	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AVI, body);
	if (ret <= 0)
		return type;
	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret)
		return type;
	cs = avi->colorspace;

	if (ieee_code == HDMI_IEEE_OUI && size == 0x18) {
		if (cs == HDMI_COLORSPACE_RGB)
			type = HDMI_DV_VSIF_STD;
	} else if (ieee_code == DOVI_IEEEOUI && size == 0x1b) {
		/* When outputting DV_LL, cs needs to be 422,
		 * Dolby_Vision_Signal (bit1) is 1,
		 * and Low_Latency (bit0) is 1
		 */
		if (cs == HDMI_COLORSPACE_YUV422 && amdv_signal == 0x3)
			type = HDMI_DV_VSIF_LL;
		/* When outputting DV_STD, cs needs to be rgb,
		 * Dolby_Vision_Signal (bit1) is 1,
		 * and Low_Latency (bit0) is 0
		 */
		if (cs == HDMI_COLORSPACE_RGB && amdv_signal == 0x2)
			type = HDMI_DV_VSIF_STD;
	}
	return type;
}

static enum hdmi_tf_type hdmitx21_get_cur_hdr10p_st(void)
{
	/* int ret; */
	unsigned int ieee_code = 0;
	u8 body[32] = {0};
	enum hdmi_tf_type type = HDMI_NONE;
	/* union hdmi_infoframe info; */
	/* struct hdmi_vendor_infoframe *vend = (struct hdmi_vendor_infoframe *)&info; */

	if (!hdmitx_vsif_en(body))
		return type;

	/* ret = hdmi_infoframe_unpack(&info, body, sizeof(body)); */
	/* if (ret) */
		/* return type; */
	/* ieee_code = vend->oui; */

	/* check Packet type */
	if (body[0] != 0x81)
		return type;

	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	if (ieee_code == HDR10PLUS_IEEEOUI)
		type = HDMI_HDR10P_DV_VSIF;

	return type;
}

static enum hdmi_tf_type hdmitx21_get_cur_cuva_st(void)
{
	/* int ret; */
	unsigned int ieee_code = 0;
	u8 body[32] = {0};
	enum hdmi_tf_type type = HDMI_NONE;
	int val;

	/*
	 * Only when sending cuva emp, the value of register D_HDR_INSERT_CTRL_IVCTX
	 * will be written to 0x3
	 */
	val = hdmitx21_rd_reg(D_HDR_INSERT_CTRL_IVCTX);
	if (val == 0x3)
		return HDMI_CUVA_TYPE;

	if (!hdmitx_vsif_en(body))
		return type;

	/* check Packet type */
	if (body[0] != 0x81)
		return type;

	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	if (ieee_code == CUVA_IEEEOUI)
		type = HDMI_CUVA_TYPE;

	return type;
}

unsigned int hdmitx21_get_vendor_infoframe_ieee(void)
{
	unsigned int ieee_code = 0;
	u8 body[32] = {0};

	if (!hdmitx_vsif_en(body))
		return 0;
	/* check Packet type */
	if (body[0] != 0x81)
		return 0;

	ieee_code = body[4] | (body[5] << 8) | (body[6] << 16);
	return ieee_code;
}

#define GET_OUTCHN_NO(a)	(((a) >> 4) & 0xf)
#define GET_OUTCHN_MSK(a)	((a) & 0xf)

static void set_aud_info_pkt(struct aud_para *audio_param)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	struct hdmi_audio_infoframe *info = &hdev->tx_comm.infoframe.aud.audio;
	u8 aud_output_i2s_ch = hdev->tx_comm.cur_audio_param.aud_output_i2s_ch;

	hdmi_audio_infoframe_init(info);
	info->channels = audio_param->chs + 1;
	if (GET_OUTCHN_NO(aud_output_i2s_ch))
		info->channels = GET_OUTCHN_NO(aud_output_i2s_ch);
	info->channel_allocation = 0;
	/* Refer to Stream Header */
	info->coding_type = 0;
	info->sample_frequency = 0;
	info->sample_size = 0;

	/* Refer to Audio Coding Type (CT) field in Data Byte 1 */
	/* info->coding_type_ext = 0; */
	/* not defined */
	/* info->downmix_inhibit = 0; */
	/* info->level_shift_value = 0; */

	switch (audio_param->type) {
	case CT_MAT:
	case CT_DTS_HD_MA:
		/* CC: 8ch, copy from hdmitx20 */
		/* info->channels = 7 + 1; */
		info->channel_allocation = 0x13;
		break;
	case CT_PCM:
		/* Refer to CEA861-D P90, only even channels */
		switch (audio_param->chs + 1) {
		case 2:
			info->channel_allocation = 0;
			break;
		case 4:
			info->channel_allocation = 0x3;
			break;
		case 6:
			info->channel_allocation = 0xb;
			break;
		case 8:
			info->channel_allocation = 0x13;
			break;
		default:
			break;
		}
		switch (GET_OUTCHN_NO(aud_output_i2s_ch)) {
		case 2:
			info->channel_allocation = 0x00;
			break;
		case 4:
			info->channel_allocation =  0x03;
			break;
		case 6:
			info->channel_allocation = 0x0b;
			break;
		case 8:
			info->channel_allocation = 0x13;
			break;
		default:
			break;
		}
		break;
	case CT_DTS:
	case CT_DTS_HD:
	default:
		/* CC: 2ch, copy from hdmitx20 */
		/* info->channels = 1 + 1; */
		info->channel_allocation = 0;
		break;
	}
	hdmi_audio_infoframe_set(info);
}

u32 hdmi21_get_frl_aud_n_paras(enum hdmi_audio_fs fs,
			       u32 frl_rate)
{
	u32 base32k_n = 6048;
	u32 base44k1_n = 5292;
	u32 base48k_n = 5760;

	switch (frl_rate) {
	case 1:
		base32k_n = 6048;
		base44k1_n = 5292;
		base48k_n = 5760;
		break;
	case 2:
	case 3:
		base32k_n = 4032;
		base44k1_n = 5292;
		base48k_n = 6048;
		break;
	case 4:
		base32k_n = 4032;
		base44k1_n = 3969;
		base48k_n = 6048;
		break;
	case 5:
		base32k_n = 3456;
		base44k1_n = 3969;
		base48k_n = 5184;
		break;
	case 6:
		base32k_n = 3072;
		base44k1_n = 3969;
		base48k_n = 4752;
		break;
	default:
		break;
	}
	switch (fs) {
	case FS_32K:
		return base32k_n * 1;
	case FS_44K1:
		return base44k1_n * 1;
	case FS_88K2:
		return base44k1_n * 2;
	case FS_176K4:
		return base44k1_n * 4;
	case FS_48K:
		return base48k_n * 1;
	case FS_96K:
		return base48k_n * 2;
	case FS_192K:
		return base48k_n * 4;
	default:
		return base48k_n;
	}
}

static void set_aud_acr_pkt(struct aud_para *audio_param)
{
	u32 aud_n_para;
	u32 char_rate = 0;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	struct meson_tx_format_para *para = NULL;

	if (!hdev)
		return;
	para = &hdev->tx_comm.fmt_para;
	if (!para)
		return;
	if (para->tx_hw_para.hdmitx_hw_para.vic == HDMI_0_UNKNOWN)
		return;

	/* if current mode is 59.94hz not 60hz, char_rate will shift down 0.1% */
	char_rate = para->timing.pixel_freq;

	if (para->cs == HDMI_COLORSPACE_YUV422)
		aud_n_para = hdmitx_hw_get_audio_n_paras(audio_param->rate,
			COLORDEPTH_24B, char_rate);
	else if (para->cs == HDMI_COLORSPACE_YUV420)
		aud_n_para = hdmitx_hw_get_audio_n_paras(audio_param->rate,
			para->cd, char_rate / 2);
	else
		aud_n_para = hdmitx_hw_get_audio_n_paras(audio_param->rate,
			para->cd, char_rate);
	if (para->tx_hw_para.hdmitx_hw_para.frl_rate)
		aud_n_para = hdmi21_get_frl_aud_n_paras(audio_param->rate,
			para->tx_hw_para.hdmitx_hw_para.frl_rate);
	hdmitx21_set_reg_bits(ACR_CTS_CLK_DIV_IVCTX,
		para->tx_hw_para.hdmitx_hw_para.frl_rate ? 1 : 0, 4, 1);
	/* N must multiples 4 for DD+ */
	switch (audio_param->type) {
	case CT_DOLBY_D:
		aud_n_para *= 4;
		break;
	default:
		break;
	}
	HDMITX_INFO("audio: aud_n_para = %d\n", aud_n_para);
	hdmitx21_wr_reg(N_SVAL1_IVCTX, (aud_n_para >> 0) & 0xff); //N_SVAL1
	hdmitx21_wr_reg(N_SVAL2_IVCTX, (aud_n_para >> 8) & 0xff); //N_SVAL2
	hdmitx21_wr_reg(N_SVAL3_IVCTX, (aud_n_para >> 16) & 0xff); //N_SVAL3
}

/*
 * audio_mute_op for audio mute/unmute hdmi
 * flag:
 * 0x00		audio mute
 * 0x01		audio unmute
 */
static void audio_mute_op(bool flag)
{
	mutex_lock(&aud_mutex);
	if (flag == 0) {
		hdmitx21_set_reg_bits(AUD_EN_IVCTX, 0, 0, 1);
		hdmitx21_set_reg_bits(AUDP_TXCTRL_IVCTX, 1, 7, 1);
		hdmitx21_set_reg_bits(TPI_AUD_CONFIG_IVCTX, 1, 4, 1);
	} else {
		hdmitx21_set_reg_bits(AUD_EN_IVCTX, 1, 0, 1);
		hdmitx21_set_reg_bits(AUDP_TXCTRL_IVCTX, 0, 7, 1);
		hdmitx21_set_reg_bits(TPI_AUD_CONFIG_IVCTX, 0, 4, 1);
	}
	HDMITX_INFO("audio: state %s\n", flag == 0 ? "AUDIO_MUTE" : "AUDIO_UNMUTE");
	mutex_unlock(&aud_mutex);
}

static int hdmitx_set_audmode(struct hdmitx_hw_common *tx_hw, struct aud_para *audio_param)
{
	u32 data32;
	bool hbr_audio = false;
	u8 div_n = 1;
	u8 i2s_line_mask = 0;
	u8 aud_output_i2s_ch;
	struct hdmitx21_hw *tx21_hw = get_hdmitx21_hw_instance();
	struct hdmitx21_dev *hdev = container_of(tx21_hw, struct hdmitx21_dev, tx21_hw);

	if (!tx_hw || !audio_param)
		return -1;

	aud_output_i2s_ch = audio_param->aud_output_i2s_ch;
	HDMITX_INFO("audio: set audio\n");
	mutex_lock(&aud_mutex);
	memcpy(&hdev->tx_comm.hdmiaud_config_data, audio_param, sizeof(struct aud_para));
	hdmitx21_set_reg_bits(AIP_RST_IVCTX, 1, 0, 1);
	if (audio_param->type == CT_MAT || audio_param->type == CT_DTS_HD_MA) {
		hbr_audio = true;
		if (audio_param->aud_src_if != AUD_SRC_IF_I2S)
			HDMITX_INFO("audio: warning: hbr with non-i2s\n");
	}
	if (audio_param->type == CT_DOLBY_D)
		div_n = 4;

	/* audio asynchronous sample clock, for spdif */
	hdmitx21_set_audioclk(true);

	HDMITX_DEBUG_AUDIO("audio: audio_param->chs = %d\n", audio_param->chs);
	hdmitx21_set_reg_bits(SPDIF_SSMPL2_IVCTX, 0, 5, 1);

	/*
	 * aud_mclk_sel: Select to use which clock for ACR measurement.
	 * 0 = Use i2s_mclk;
	 * 1 = Use spdif_clk.
	 */
	hdmitx21_set_reg_bits(HDMITX_TOP_CLK_CNTL, 1 - audio_param->aud_src_if, 13, 1);

	HDMITX_DEBUG_AUDIO("audio: aud_src_if = %d\n", audio_param->aud_src_if);

	/*
	 * config I2S
	 */
	hdmitx21_wr_reg(AIP_HDMI2MHL_IVCTX, 0x00);
	hdmitx21_wr_reg(PKT_FILTER_0_IVCTX, 0x02);
	hdmitx21_wr_reg(ASRC_IVCTX, 0x00);
	hdmitx21_wr_reg(VP_INPUT_SYNC_ADJUST_CONFIG_IVCTX, 0x01);

	data32 = 0;
	if (hbr_audio) {
		/* hbr no layout, see hdmi1.4 spec table 5-28 */
		data32 = (0 << 1);
	} else if (audio_param->aud_src_if == 1) {
		unsigned char mask = GET_OUTCHN_MSK(aud_output_i2s_ch);
		/* multi-channel lpcm use layout 1 */
		if (audio_param->type == CT_PCM && audio_param->chs >= 2)
			data32 = (1 << 1);
		else
			data32 = (0 << 1);
		if (mask) {
			if (mask == 8 || mask == 4 || mask == 2 || mask == 1)
				data32 = (0 << 1);
			else
				data32 = (1 << 1);
		}
	} else {
		data32 = (0 << 1);
	}
	hdmitx21_wr_reg(AUDP_TXCTRL_IVCTX, data32 & 0xff);

	set_aud_acr_pkt(audio_param);
	/*
	 * FREQ
	 *  00:mclk=128*Fs;
	 *  01:mclk=256*Fs;
	 *  10:mclk=384*Fs;
	 *  11:mclk=512*Fs;
	 */
	hdmitx21_wr_reg(FREQ_SVAL_IVCTX, 0);

	/*
	 * [7:6] reg_tpi_spdif_sample_size:
	 *  0=Refer to stream header;
	 *  1=16-bit;
	 *  2=20-bit;
	 *  3=24-bit
	 * [  4] reg_tpi_aud_mute
	 */
	data32 = 0;
	data32 |= (3 << 6);
	data32 |= (0 << 4);
	hdmitx21_wr_reg(TPI_AUD_CONFIG_IVCTX, data32);

	/* for i2s: 2~8ch lpcm, hbr */
	if (audio_param->aud_src_if == 1) {
		unsigned char mask = GET_OUTCHN_MSK(aud_output_i2s_ch);
		int i = 0;
		int j = 0;
		const unsigned char map[] = {
			[1] = 0,
			[2] = 1,
			[4] = 2,
			[8] = 3,
		};

		hdmitx21_wr_reg(I2S_IN_MAP_IVCTX, 0xE4);
		if (mask) {
			for (j = 0; j < 4; j++) {
				if (mask & (1 << j)) {
					hdmitx21_set_reg_bits(I2S_IN_MAP_IVCTX,
							map[1 << j], i << 1, 2);
					i++;
				}
			}
		}

		hdmitx21_wr_reg(I2S_IN_CTRL_IVCTX, 0x20);
		hdmitx21_wr_reg(I2S_IN_SIZE_IVCTX, 0x0b);
		/*
		 * channel status: for i2s hbr/pcm
		 * actually audio module only notify 4 bytes
		 */
		hdmitx21_wr_reg(I2S_CHST0_IVCTX, audio_param->status[0]);
		hdmitx21_wr_reg(I2S_CHST1_IVCTX, audio_param->status[1]);
		hdmitx21_wr_reg(I2S_CHST2_IVCTX, audio_param->status[2]);
		hdmitx21_wr_reg(I2S_CHST3_IVCTX, audio_param->status[3]);
		hdmitx21_wr_reg(I2S_CHST4_IVCTX, audio_param->status[4]);
	}
	data32 = 0;
	/*
	 * [  6] i2s2dsd_en
	 * [5:0] aud_err_thresh
	 */
	data32 |= (0 << 6);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(SPDIF_ERTH_IVCTX, data32);

	/*
	 * [7:4] I2S_EN SD0~SD3
	 * [  3] DSD_EN
	 * [  2] HBRA_EN
	 * [  1] SPID_EN  Enable later in test.c, otherwise initial junk data will be sent
	 * [  0] PKT_EN
	 */
	data32 = 0;
	if (audio_param->aud_src_if == 1) {
		if (audio_param->chs == 2 - 1) {
			i2s_line_mask = 1;
		} else if (audio_param->chs == 4 - 1) {
			/* SD0/1 */
			i2s_line_mask = 0x3;
		} else if (audio_param->chs == 6 - 1) {
			/* SD0/1/2 */
			i2s_line_mask = 0x7;
		} else if (audio_param->chs == 8 - 1) {
			/* SD0/1/2/3 */
			i2s_line_mask = 0xf;
		}
		if (GET_OUTCHN_MSK(aud_output_i2s_ch))
			i2s_line_mask = GET_OUTCHN_MSK(aud_output_i2s_ch);
		data32 |= (i2s_line_mask << 4);
		data32 |= (0 << 3);
		data32 |= (hbr_audio << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);
		hdmitx21_wr_reg(AUD_MODE_IVCTX, data32);
	} else {
		hdmitx21_wr_reg(AUD_MODE_IVCTX, 0x2);
	}

	hdmitx21_wr_reg(AUD_EN_IVCTX, 0x03);

	set_aud_info_pkt(audio_param);
	if (audio_param->fifo_rst)
		hdmitx21_hw_cntl(tx_hw, AUDIO_RESET, NULL, NULL);
	hdmitx21_set_reg_bits(AIP_RST_IVCTX, 0, 0, 1);
	mutex_unlock(&aud_mutex);
	if (hdmitx_find_vendor_audio_delay(hdev->tx_comm.base.edid_buf)) {
		HDMITX_INFO("special tv need more time when set audio mode\n");
		msleep_interruptible(10);
	}
	audio_mute_op(audio_param->aud_output_en);
	return 0;
}

static void hdmitx_free_irq(struct hdmitx_hw_common *tx_hw)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);

	free_irq(hdev->tx_comm.irq_hpd, (void *)hdev);
	free_irq(hdev->tx_comm.irq_hdmitx_vsync, (void *)hdev);
}

void hdmitx21_meson_uninit(struct hdmitx_hw_common *hw_comm)
{
	struct hdmitx21_dev *h21_dev = container_of(hw_comm, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &h21_dev->tx_comm;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;
#endif

	hdmitx_free_irq(hw_comm);
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	if (hdcptx_comm->task_hdcp)
		kthread_stop(hdcptx_comm->task_hdcp);
	if (hdcptx_comm->hdcptx_api->hdcptx_uninit)
		hdcptx_comm->hdcptx_api->hdcptx_uninit(tx_comm->hdcptx_priv);
#endif

	HDMITX_DEBUG("hdmitx21 uninit\n");

	hdmitx_fifo_flow_enable_intr(0);
	phy_hpll_off();
	hdmitx21_hpd_hw_op(&h21_dev->tx21_hw, HPD_UNMUX_HPD);

	if (hw_comm->chip_data->chip_type == MESON_CPU_ID_S5)
		kthread_stop(tx_comm->task_clk_check);
}

static ssize_t hdmitx_get_clk(char *buf, int size)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	static const struct _hdmi_clkmsr *hdmiclkmsr;
	int i = 0, len = 0, pos = 0;

	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
		hdmiclkmsr = hdmiclkmsr_t7;
		len = ARRAY_SIZE(hdmiclkmsr_t7);
		break;
	case MESON_CPU_ID_S1A:
		hdmiclkmsr = hdmiclkmsr_s1a;
		len = ARRAY_SIZE(hdmiclkmsr_s1a);
		break;
	case MESON_CPU_ID_S5:
		hdmiclkmsr = hdmiclkmsr_s5;
		len = ARRAY_SIZE(hdmiclkmsr_s5);
		break;
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		hdmiclkmsr = hdmiclkmsr_s7;
		len = ARRAY_SIZE(hdmiclkmsr_s7);
		break;
	default:
		break;
	}
	for (i = 0; i < len; i++)
		pos += snprintf(buf + pos, size - pos,
			"[%d] %lld %s\n", hdmiclkmsr[i].idx,
			meson_clk_measure(hdmiclkmsr[i].idx),
			hdmiclkmsr[i].name);
	return pos;
}

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
static bool is_tmds_clk_en(enum amhdmitx_chip_e chip_type)
{
	unsigned int tmds_clk_idx = 0xffff;

	switch (chip_type) {
	case MESON_CPU_ID_S5:
		tmds_clk_idx = 92;
		break;
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S1A:
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
		tmds_clk_idx = 76;
		break;
	default:
		break;
	}
	if (tmds_clk_idx == 0xffff)
		return false;
	if (meson_clk_measure(tmds_clk_idx) < 25000000)
		return false;

	return true;
}

static int hdmitx_tmds_enable(void *data)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;
	int set_mode_ret = 1;

	if (!tx_comm || !tx_comm->tx_hw) {
		HDMITX_ERROR("NULL tx_comm instance!\n", __func__);
		return -1;
	}

	/* set default display mode firstly if no tmds clk */
	if (!is_tmds_clk_en(tx_comm->tx_hw->chip_data->chip_type)) {
		set_mode_ret = hdmitx_common_build_format_para(tx_comm,
			&tx_comm->fmt_para,
			HDMI_4_1280x720p60_16x9, 1,
			HDMI_COLORSPACE_RGB, COLORDEPTH_24B, HDMI_QUANTIZATION_RANGE_FULL);
		if (set_mode_ret < 0) {
			HDMITX_ERROR("%s format para build fail\n", __func__);
			return -1;
		}
		set_mode_ret = hdmitx21_hw_cntl(tx_comm->tx_hw, MODE_FLOW_ENABLE_MODE, NULL, NULL);
		if (set_mode_ret < 0) {
			HDMITX_ERROR("mode enable fail\n");
			return -1;
		}
	}
	return set_mode_ret;
}

static int hdmitx_tmds_disable(void *data)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;

	if (!tx_comm || !tx_comm->tx_hw) {
		HDMITX_ERROR("NULL tx_comm instance!\n", __func__);
		return -1;
	}
	return hdmitx_common_disable_mode(tx_comm, NULL);
}
#endif

/* check clk status when plug out in case no vsync */
static void hdmitx21_vid_pll_clk_check(struct hdmitx_hw_common *tx_hw)
{
	int clk[3];
	u32 idx[3];
	u32 arg;

	if (!tx_hw)
		return;
	if (tx_hw->chip_data->chip_type != MESON_CPU_ID_S5)
		return;
	/*
	 * frl mode use fpll or gp2 pll, and won't go through
	 * vid_clk0_div_top/tmds20_clk_div_top, no need to check.
	 */
	if (hdmitx21_hw_cntl(tx_hw, FRL_GET_MODE, NULL, NULL))
		return;

	/* for S5, here need check the clk index 92 & 16 */
	/* cts_htx_tmds_clk */
	idx[0] = 92;
	/* vid_pll0_clk */
	idx[1] = 16;
	/* htx_tmds20_clk */
	idx[2] = 89;

	clk[0] = meson_clk_measure(idx[0]);
	clk[1] = meson_clk_measure(idx[1]);
	if (clk[0] && clk[1])
		return;

	if (!clk[0]) {
		HDMITX_DEBUG("%s the clock[%d] is %d\n", __func__, idx[0], clk[0]);
		arg = idx[0];
		hdmitx21_hw_cntl(tx_hw, PLATFORM_CLK_DIV_RST, (void *)&arg, NULL);
		HDMITX_INFO("after reset the clock[%d] is %d\n", idx[0], meson_clk_measure(idx[0]));
	}
	if (!clk[1]) {
		HDMITX_DEBUG("%s the clock[%d] is %d\n",  __func__, idx[1], clk[1]);
		arg = idx[1];
		hdmitx21_hw_cntl(tx_hw, PLATFORM_CLK_DIV_RST, (void *)&arg, NULL);
		HDMITX_INFO("after reset the clock[%d] is %d\n", idx[1], meson_clk_measure(idx[1]));
	}
}

#define READ2REG(reg) ({ \
	typeof(reg) _reg = (reg); \
	(hdmitx21_rd_reg(_reg) & 0xff) + \
	((hdmitx21_rd_reg(_reg + 1) & 0xff) << 8); })

static void pix_capture_dump(struct hdmitx21_dev *hdev)
{
	u32 reg;
	u32 val;
	int avi_packet = hdmitx21_rd_reg(TPI_AVI_BYTE1_IVCTX);
	/*
	 * bit 6:5
	 * 00 rgb
	 * 01 yuv422
	 * 10 yuv444
	 * 11 yuv420
	 */
	int avi_index = ((avi_packet & 0x60) ? 1 : 0);

	if (hdev->tx_comm.tx_hw->chip_data->chip_type >= MESON_CPU_ID_S7)
		hdmitx21_set_reg_bits(HDMITX_TOP_CLK_GATE, 1, 1, 1);/* enable fdet gate */
	hdmitx21_wr_reg(VP_FDET_CLEAR_IVCTX, 0);
	hdmitx21_wr_reg(VP_FDET_STATUS_IVCTX, 0);
	mdelay(100);/* For 23.976hz, at least 2 frames */

	/* read pixcap value */
	reg = VP_CMS_CSC0_PIXCAP_IN_Y_IVCTX;
	val = READ2REG(reg);
	if (val) {
		if (avi_index)
			HDMITX_INFO("pixcap_in_y [%x] 0x%x %d\n", reg, val, val);
		else
			HDMITX_INFO("pixcap_in_r [%x] 0x%x %d\n", reg, val, val);
	}

	reg = VP_CMS_CSC0_PIXCAP_IN_CB_IVCTX;
	val = READ2REG(reg);
	if (val) {
		if (avi_index)
			HDMITX_INFO("pixcap_in_CB [%x] 0x%x %d\n", reg, val, val);
		else
			HDMITX_INFO("pixcap_in_g [%x] 0x%x %d\n", reg, val, val);
	}

	reg = VP_CMS_CSC0_PIXCAP_IN_CR_IVCTX;
	val = READ2REG(reg);
	if (val) {
		if (avi_index)
			HDMITX_INFO("pixcap_in_CR [%x] 0x%x %d\n", reg, val, val);
		else
			HDMITX_INFO("pixcap_in_b [%x] 0x%x %d\n", reg, val, val);
	}

	reg = VP_OUTPUT_MAPPING_IVCTX;
	val = READ2REG(reg);
	if (val)
		HDMITX_INFO("vp__output_mapping [%x] 0x%x %d\n", reg, val, val);

	if (hdev->tx_comm.tx_hw->chip_data->chip_type >= MESON_CPU_ID_S7)
		hdmitx21_set_reg_bits(HDMITX_TOP_CLK_GATE, 0, 1, 1);/* disable fdet gate */
}

static void hdmitx_debug(struct hdmitx_hw_common *tx_hw, const char *buf)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	char tmpbuf[128];
	int i = 0;
	int ret;
	unsigned long adr = 0;
	unsigned long value = 0;
	unsigned int enable_mask;
	unsigned int mov_val;
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;
	struct vinfo_s *vinfo = &hdev->tx_comm.hdmitx_vinfo;
	struct hdcptx_common *hdcptx_comm = &hdev->tx_comm.hdcptx_comm;

	if (!buf)
		return;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	if (strncmp(tmpbuf, "test_edid", 9) == 0) {
		hdmitx21_hw_cntl(&hdev->hw_comm, DDC_RESET_EDID, NULL, NULL);
		hdmitx21_hw_cntl(&hdev->hw_comm, DDC_EDID_READ_DATA, NULL, NULL);
		return;
	} else if (strncmp(tmpbuf, "i2c_reactive", 11) == 0) {
		hdmitx21_hw_cntl(&hdev->hw_comm, DDC_I2C_RESET, NULL, NULL);
		return;
	} else if (strncmp(tmpbuf, "bist", 4) == 0) {
		if (strncmp(tmpbuf + 4, "off", 3) == 0) {
			if (vinfo->viu_mux == VIU_MUX_ENCI) {
				hd21_write_reg(ENCI_TST_EN, 0);
			} else {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 0);
			}
			hdev->tx_comm.bist_lock = 0;
			return;
		}
		hdev->tx_comm.bist_lock = 1;
		/* for 480i/576i mode */
		if (vinfo->viu_mux == VIU_MUX_ENCI) {
			if (strncmp(tmpbuf + 4, "blank", 5) == 0) {
				hd21_write_reg(ENCI_TST_MDSEL, 0);
				hd21_write_reg(ENCI_TST_Y, 0x0);
				hd21_write_reg(ENCI_TST_CB, 0x200);
				hd21_write_reg(ENCI_TST_CR, 0x200);
				hd21_write_reg(ENCI_TST_EN, 1);
				return;
			}
			if (strncmp(tmpbuf + 4, "red", 3) == 0) {
				hd21_write_reg(ENCI_TST_MDSEL, 0);
				hd21_write_reg(ENCI_TST_Y, 0x200);
				hd21_write_reg(ENCI_TST_CB, 0x0);
				hd21_write_reg(ENCI_TST_CR, 0x3ff);
				hd21_write_reg(ENCI_TST_EN, 1);
				return;
			}
			if (strncmp(tmpbuf + 4, "green", 5) == 0) {
				hd21_write_reg(ENCI_TST_MDSEL, 0);
				hd21_write_reg(ENCI_TST_Y, 0x200);
				hd21_write_reg(ENCI_TST_CB, 0x0);
				hd21_write_reg(ENCI_TST_CR, 0x0);
				hd21_write_reg(ENCI_TST_EN, 1);
				return;
			}
			if (strncmp(tmpbuf + 4, "blue", 4) == 0) {
				hd21_write_reg(ENCI_TST_MDSEL, 0);
				hd21_write_reg(ENCI_TST_Y, 0x200);
				hd21_write_reg(ENCI_TST_CB, 0x3ff);
				hd21_write_reg(ENCI_TST_CR, 0x0);
				hd21_write_reg(ENCI_TST_EN, 1);
				return;
			}
			/* nearly DE_BEGIN */
			hd21_write_reg(ENCI_TST_CLRBAR_STRT, 0x112);
			/* 1440 / 8 = 0xb4 */
			hd21_write_reg(ENCI_TST_CLRBAR_WIDTH, 0xb4);
			hd21_write_reg(ENCI_TST_Y, 0x200);
			hd21_write_reg(ENCI_TST_CB, 0x200);
			hd21_write_reg(ENCI_TST_CR, 0x200);
			hd21_write_reg(ENCI_TST_EN, 1);
			if (strncmp(tmpbuf + 4, "line", 4) == 0)
				hd21_write_reg(ENCI_TST_MDSEL, 2);
			else if (strncmp(tmpbuf + 4, "dot", 3) == 0)
				hd21_write_reg(ENCI_TST_MDSEL, 3);
			else
				hd21_write_reg(ENCI_TST_MDSEL, 1);
			return;
		}
		/* for encp including 1080i */
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
		hd21_write_reg(VENC_VIDEO_TST_EN, 1);
		if (strncmp(tmpbuf + 4, "line", 4) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 2);
			return;
		}
		if (strncmp(tmpbuf + 4, "dot", 3) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 3);
			return;
		}
		if (strncmp(tmpbuf + 4, "start", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, value);
			return;
		}
		if (strncmp(tmpbuf + 4, "shift", 5) == 0) {
			ret = kstrtoul(tmpbuf + 9, 10, &value);
			hd21_write_reg(VENC_VIDEO_TST_VDCNT_STSET, value);
			return;
		}
		if (strncmp(tmpbuf + 4, "auto", 4) == 0) {
			const struct tx_timing *t;

			t = &para->timing;
			value = t->h_active;
			/* when FRL works, here will be half rate */
			if (para->tx_hw_para.hdmitx_hw_para.frl_rate) {
				value /= 2;
				if (para->cs == HDMI_COLORSPACE_YUV420)
					value /= 2;
			}
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, 0x113);
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 1);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, value / 8);
			return;
		}
		if (strncmp(tmpbuf + 4, "blank", 5) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			return;
		}
		if (strncmp(tmpbuf + 4, "red", 3) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			return;
		}
		if (strncmp(tmpbuf + 4, "green", 5) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			return;
		}
		if (strncmp(tmpbuf + 4, "blue", 4) == 0) {
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x200);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x0);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			return;
		}
		if ((strncmp(tmpbuf + 4, "X", 1) == 0) || (strncmp(tmpbuf + 4, "x", 1) == 0)) {
			const struct tx_timing *t;
			u32 width = 1920;
			u32 height = 1080;

			if (hdev->tx_comm.tx_hw->chip_data->chip_type < MESON_CPU_ID_S1A) {
				HDMITX_INFO("s5 or later support x pattern\n");
				return;
			}
			t = &para->timing;
			width = t->h_active;
			/* when FRL works, here will be half rate */
			if (para->tx_hw_para.hdmitx_hw_para.frl_rate) {
				width /= 2;
				if (para->cs == HDMI_COLORSPACE_YUV420 ||
					para->tx_hw_para.hdmitx_hw_para.dsc_en)
					width /= 2;
			}
			height = t->v_active;
			hd21_write_reg(VENC_VIDEO_TST_Y, 0x3ff);
			hd21_write_reg(VENC_VIDEO_TST_CB, 0x1);
			hd21_write_reg(VENC_VIDEO_TST_CR, 0x1);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, height);
			hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, width);
			hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
			hd21_write_reg(VENC_VIDEO_TST_MDSEL, 4);
			hd21_write_reg(VENC_VIDEO_TST_EN, 1);
			return;
		}
		hd21_write_reg(VENC_VIDEO_TST_MDSEL, 1);
		value = 1920;
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		hd21_write_reg(VENC_VIDEO_TST_CLRBAR_STRT, 0x113);
		hd21_write_reg(VENC_VIDEO_TST_CLRBAR_WIDTH, value / 8);
		return;
	} else if (strncmp(tmpbuf, "pbist", 5) == 0) {
		if (strncmp(tmpbuf + 5, "en", 2) == 0)
			hdmitx21_pbist_config(hdev,
				hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.vic, 1);
		if (strncmp(tmpbuf + 5, "off", 3) == 0)
			hdmitx21_pbist_config(hdev,
				hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.vic, 0);
	} else if (strncmp(tmpbuf, "test_audio", 10) == 0) {
		hdmitx_set_audmode(&hdev->hw_comm, NULL);
	} else if (strncmp(tmpbuf, "hpd_lock", 8) == 0) {
		if (tmpbuf[8] == '1') {
			hdev->hw_comm.debug_hpd_lock = 1;
			HDMITX_INFO("lock hpd\n");
		} else {
			hdev->hw_comm.debug_hpd_lock = 0;
			HDMITX_INFO("unlock hpd\n");
		}
		return;
	} else if (tmpbuf[0] == 'w') {
		unsigned long read_back = 0;

		ret = kstrtoul(tmpbuf + 2, 16, &adr);
		ret = kstrtoul(buf + i + 1, 16, &value);
		/* scdc register write */
		if (buf[1] == 's') {
			scdc21_wr_sink(adr, value);
			HDMITX_INFO("scdc wr 0x%lx 0x%lx\n", adr, value);
			return;
		}
		if (buf[1] == 'h') {
			hdmitx21_wr_reg((unsigned int)adr, (unsigned int)value);
			read_back = hdmitx21_rd_reg(adr);
		}
		HDMITX_INFO("write %lx to %s reg[%lx]\n", value, "HDMI", adr);
		/* read back in order to check writing is OK or NG. */
		HDMITX_INFO("Read Back %s reg[%lx]=%lx\n", "HDMI",
			adr, read_back);
	} else if (tmpbuf[0] == 'r') {
		u8 val;

		ret = kstrtoul(tmpbuf + 2, 16, &adr);
		/* scdc register read */
		if (buf[1] == 's') {
			scdc21_rd_sink(adr, &val);
			HDMITX_INFO("scdc rd 0x%lx 0x%x\n", adr, val);
			return;
		}
		if (buf[1] == 'h')
			value = hdmitx21_rd_reg(adr);
		HDMITX_INFO("%s reg[%lx]=%lx\n", "HDMI", adr, value);
	} else if (strncmp(tmpbuf, "set_div40", 9) == 0) {
		/* echo 1 > div40, force send 1:40 tmds bit clk ratio
		 * echo 0 > div40, send 1:10 tmds bit clk ratio if scdc_present
		 * echo 2 > div40, force send 1:10 tmds bit clk ratio
		 */
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		if (value != 0 && value != 1 && value != 2) {
			HDMITX_ERROR("set div40 value in 0 ~ 2\n");
		} else {
			HDMITX_INFO("set div40 to %lu\n", value);
			hdmitx21_hw_cntl(&hdev->hw_comm,
				DDC_SCDC_DIV40_SCRAMB, (void *)&value, NULL);
		}
	} else if (strncmp(buf, "pkt_move", 8) == 0) {
		ret = sscanf(buf, "pkt_move %x %x", &enable_mask, &mov_val);
		if (ret == 2)
			pkt_send_position_change(enable_mask, mov_val);
	} else if (strncmp(tmpbuf, "div40", 5) == 0) {
		if (strncmp(tmpbuf + 5, "1", 1) == 0)
			hdmitx_set_div40(1);
		if (strncmp(tmpbuf + 5, "0", 1) == 0)
			hdmitx_set_div40(0);
	} else if (strncmp(tmpbuf, "pll_clk_config", 14) == 0) {
		if (strncmp(tmpbuf + 14, "1", 1) == 0) {
			hdev->tx21_hw.clk_analog_path = 1;
			HDMITX_INFO("clk_analog_path = %d\n", hdev->tx21_hw.clk_analog_path);
		} else if (strncmp(tmpbuf + 14, "0", 1) == 0) {
			hdev->tx21_hw.clk_analog_path = 0;
			HDMITX_INFO("clk_analog_path = %d\n",  hdev->tx21_hw.clk_analog_path);
		}
	} else if (strncmp(tmpbuf, "hdcp_ver", 8) == 0) {
		HDMITX_INFO("hdcp_22_capable :%d\n",
			hdcptx_comm->hdcptx_api->get_hdcprx_ver(hdcptx_comm));
	} else if (strncmp(tmpbuf, "emp_test", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
#ifdef CONFIG_AMLOGIC_DSC
		if (value == 0) {
			hdmitx_dsc_cvtem_pkt_disable();
		} else {
			hdmitx_get_dsc_data(&hdev->dsc_data);
			hdmitx_dsc_cvtem_pkt_send(&hdev->dsc_data.pps_data, &para->timing);
		}
		HDMITX_INFO("dsc emp test %s\n", value ? "enable" : "disable");
#endif
	} else if (strncmp(tmpbuf, "venc", 4) == 0) {
		ret = kstrtoul(tmpbuf + 4, 10, &value);
		HDMITX_INFO("venc :%d\n", !!value);
		hd21_set_reg_bits(VPU_HDMI_SETTING, !!value,
			(hdev->tx_comm.enc_idx == 0) ? 0 : 1, 1);
		usleep_range(1000, 1005);
		hdmitx21_venc_en(!!value, 1);
	} else if (strncmp(tmpbuf, "dsc_mux", 7) == 0) {
		ret = kstrtoul(tmpbuf + 7, 10, &value);
		HDMITX_INFO("dsc_mux :%d\n", !!value);
		hd21_set_reg_bits(VPU_HDMI_SETTING, !!value, 31, 1);
	} else if (strncmp(tmpbuf, "slice_num", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		HDMITX_INFO("force vpp_post slice_num :%lu\n", value);
		vinfo->cur_enc_ppc = value;
	} else if (strncmp(tmpbuf, "emp_dbg_en", 10) == 0) {
		ret = kstrtoul(tmpbuf + 10, 10, &value);
		HDMITX_INFO("emp_dbg_en :%d\n", !!value);
		emp_dbg_en = !!value;
	} else if (strncmp(tmpbuf, "clk_reg", 7) == 0) {
		HDMITX_INFO("RA: CLKCTRL_VID_CLK0_CTRL         0xfe0000c0: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_CTRL));
		HDMITX_INFO("RB: CLKCTRL_VIID_CLK0_CTRL        0xfe0000d0: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VIID_CLK0_CTRL));
		HDMITX_INFO("RC: CLKCTRL_ENC0_HDMI_CLK_CTRL    0xfe0000d4: 0x%08x\n",
			hd21_read_reg(CLKCTRL_ENC0_HDMI_CLK_CTRL));
		HDMITX_INFO("RD: CLKCTRL_VID_CLK0_CTRL2        0xfe0000c4: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_CTRL2));
		HDMITX_INFO("RE: CLKCTRL_VIID_CLK0_DIV         0xfe0000cc: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VIID_CLK0_DIV));
		HDMITX_INFO("RF: CLKCTRL_ENC_HDMI_CLK_CTRL     0xfe0000dc: 0x%08x\n",
			hd21_read_reg(CLKCTRL_ENC_HDMI_CLK_CTRL));
		HDMITX_INFO("RG: CLKCTRL_VID_CLK0_DIV          0xfe0000c8: 0x%08x\n",
			hd21_read_reg(CLKCTRL_VID_CLK0_DIV));
		HDMITX_INFO("RH: CLKCTRL_HDMI_PLL_TMDS_CLK_DIV 0xfe000218: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HDMI_PLL_TMDS_CLK_DIV));
		HDMITX_INFO("RI: CLKCTRL_HDMI_VID_PLL_CLK_DIV  0xfe000204: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HDMI_VID_PLL_CLK_DIV));
		HDMITX_INFO("RJ: CLKCTRL_HTX_CLK_CTRL1         0xfe000120: 0x%08x\n",
			hd21_read_reg(CLKCTRL_HTX_CLK_CTRL1));
	} else if (strncmp(tmpbuf, "hdcp_reauth", 11) == 0) {
		ret = kstrtoul(tmpbuf + 11, 16, &value);
		if (ret != 0)
			return;
		HDMITX_INFO("hdcp reauth: %lu\n", value);
		hdmitx_reauth_request(value & 0xFF);
	} else if (strncmp(tmpbuf, "dfm_type", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdev->dfm_type = value;
		HDMITX_INFO("dfm_type :%d\n", hdev->dfm_type);
	} else if (strncmp(tmpbuf, "csc_type", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdev->csc_type = value;
		HDMITX_INFO("csc_type :%d\n", hdev->csc_type);
	} else if (strncmp(tmpbuf, "gate_bit_mask", 13) == 0) {
		ret = kstrtoul(tmpbuf + 13, 16, &value);
		hdev->tx21_hw.gate_bit_mask = value;
		HDMITX_INFO("gate_bit_mask :0x%x\n", hdev->tx21_hw.gate_bit_mask);
	} else if (strncmp(tmpbuf, "scan_info", 9) == 0) {
		ret = kstrtoul(tmpbuf + 9, 10, &value);
		hdmi_avi_infoframe_config(&hdev->tx_comm, AUX_PKT_CONF_AVI_SCAN, value);
		HDMITX_INFO("config scan info: %d\n", value);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (strncmp(buf, "t_vrr", 5) == 0) {
		/*
		 * t_vrr14000: game, 40.00hz
		 * t_vrr23000: qms, tfr 30hz
		 * t_vrr00: exit game/qms mode
		 */
		struct vrr_setting_info data;
		unsigned long rate;

		data.type = buf[5] - '0';
		if (kstrtoul(buf + 6, 10, &rate) == 0)
			hdmitx_set_vrr_rate(tx_hw, (int)rate, &data);
#endif
	} else if (strncmp(tmpbuf, "pix_capture", 11) == 0) {
		int pixel_ivctx, line_ivctx, ret;
		int pixel_height, pixel_low, line_high, line_low;

		ret = sscanf(tmpbuf, "pix_capture.%d.%d", &pixel_ivctx, &line_ivctx);
		if (ret)
			HDMITX_INFO("pix_capture x:%d y:%d\n", pixel_ivctx, line_ivctx);
		else
			HDMITX_INFO("Failed to parse input correctly.\n");
		pixel_height = (pixel_ivctx >> 8) & 0xFF;
		pixel_low = pixel_ivctx & 0xFF;
		line_high = (line_ivctx >> 8) & 0xFF;
		line_low = line_ivctx & 0xFF;

		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN_CONFIG_IVCTX, 1);
		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN_CONTROL_IVCTX, 1);
		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN_PIXEL_IVCTX, pixel_low);
		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN_PIXEL_IVCTX + 1, pixel_height);
		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN__LINE__IVCTX, line_low);
		hdmitx21_wr_reg(VP_CMS_CSC0_PIXCAP_IN__LINE__IVCTX + 1, line_high);
		pix_capture_dump(hdev);
	}
}

/*
 * refer to hdmi2.1b spec 10.4.1.4
 * bit0: scrambling_enable
 *		0, disable
 *		1, enable
 * bit1: TMDS_Bit_Clock_Ratio
 *		0, 1/10
 *		1, 1/40
 */
static void hdmitx_set_scdc_div40(u32 div40)
{
	u32 addr = DDC_SCDCDIV_ADDR;
	u32 scram_en = 0;
	u32 clk_ratio = 0;
	u32 data = 0;

	if (div40) {
		scram_en = 1;
		clk_ratio = 1;
	}

	data |= scram_en;
	data |= (clk_ratio << 1);
	scdc21_wr_sink(addr, data);
}

static void set_t7_top_div40(u32 div40)
{
	u32 data32;

	// Enable normal output to PHY
	if (div40) {
		data32 = 0;
		data32 |= (0 << 16); // [25:16] tmds_clk_pattern[19:10]
		data32 |= (0 << 0);  // [ 9: 0] tmds_clk_pattern[ 9: 0]
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_01, data32);

		data32 = 0;
		data32 |= (0x3ff << 16); // [25:16] tmds_clk_pattern[39:30]
		data32 |= (0x3ff << 0);  // [ 9: 0] tmds_clk_pattern[29:20]
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_23, data32);
	} else {
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}
	hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	// [14:12] tmds_sel: 0=output 0; 1=output normal data;
	//                   2=output PRBS; 4=output shift pattern
	// [11: 8] shift_pattern
	// [ 4: 0] prbs_pattern
	data32 = 0;
	data32 |= (1 << 12);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_BIST_CNTL, data32);
	if (div40)
		hdmitx21_wr_reg(HDMITX_T7_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static void set_s5_top_div40(u32 div40, u32 frl_mode)
{
	u32 data32;

	// Enable normal output to PHY
	if (div40) {
		data32 = 0;
		data32 |= (0 << 16); // [25:16] tmds_clk_pattern[19:10]
		data32 |= (0 << 0);  // [ 9: 0] tmds_clk_pattern[ 9: 0]
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_01, data32);

		data32 = 0;
		data32 |= (0x3ff << 16); // [25:16] tmds_clk_pattern[39:30]
		data32 |= (0x3ff << 0);  // [ 9: 0] tmds_clk_pattern[29:20]
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_23, data32);
	} else {
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_01, 0x001f001f);
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_23, 0x001f001f);
	}
	hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_CNTL, 0x1);
	// [18:16] tmds_sel: 0=output 0; 1=output normal data;
	//                   2=output PRBS; 4=output shift pattern
	// [11: 8] shift_pattern
	// [ 4: 0] prbs_pattern
	data32 = 0;
	data32 |= (1 << 16);
	data32 |= (0 << 8);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_BIST_CNTL, data32);

	hdmitx21_set_reg_bits(HDMITX_TOP_BIST_CNTL, frl_mode ? 1 : 0, 19, 1);

	if (div40)
		hdmitx21_wr_reg(HDMITX_S5_TOP_TMDS_CLK_PTTN_CNTL, 0x2);
}

static void set_top_div40(u32 div40)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S5:
		set_s5_top_div40(div40, hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate);
		break;
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S1A:
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
	default:
		set_t7_top_div40(div40);
		break;
	}
}

static void hdmitx_set_div40(u32 div40)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	if (div40)
		hdmitx_set_scdc_div40(1);
	else if (hdev->tx_comm.base.rxcap.scdc_present ||
		hdev->tx_comm.tx_hw->pre_tmds_clk_div40)
		hdmitx_set_scdc_div40(0);
	else
		HDMITX_INFO("warn: SCDC not present, should not send 1:10\n");
	set_top_div40(div40);
	hdmitx21_wr_reg(SCRCTL_IVCTX, (1 << 5) | !!div40);
	hdev->tx_comm.tx_hw->pre_tmds_clk_div40 = !!div40;
}

static int hdmitx_hdmi_dvi_config(struct hdmitx21_dev *hdev,
				  u32 dvi_mode)
{
	if (dvi_mode == 1)
		hdmitx_csc_config(TX_INPUT_COLOR_FORMAT, HDMI_COLORSPACE_RGB,
			hdev->tx_comm.fmt_para.cd);

	return 0;
}

static int hdmitx_get_hdmi_dvi_config(struct hdmitx21_dev *hdev)
{
	/* TODO */
	return HDMI_MODE;
}

static void hdmitx21_vp_conf(unsigned char color_depth, unsigned char output_color_format)
{
	u8 data8 = 0;
	u32 data32 = 0;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	/*
	 * For S1A and later chips, delete the vp core hardware and move the yuv to rgb
	 * function to vpu_hdmi_if block
	 */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
		if (output_color_format == HDMI_COLORSPACE_RGB)
			hdmitx_sync_input_vpp_info(&hdev->tx_comm);
		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB), 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= ((6 - color_depth) << 4);
		data8 |= ((6 - color_depth) << 2);
		data8 |= ((6 - color_depth) << 0);
		hdmitx21_wr_reg(VP_INPUT_MASK_IVCTX, data8);
	}
	// [11: 9] select_cr: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//					  4={cr[3:0],y[3:0],cb[3:0]};
	//					  5={y[3:0],cr[3:0],cb[3:0]};
	//					  6={cb[3:0],y[3:0],cr[3:0]};
	//					  7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 8: 6] select_cb: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//					  4={cr[3:0],y[3:0],cb[3:0]};
	//					  5={y[3:0],cr[3:0],cb[3:0]};
	//					  6={cb[3:0],y[3:0],cr[3:0]};
	//					  7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 5: 3] select_y : 0=y; 1=cb; 2=Cr; 3={y[11:4],cb[7:4]};
	//					  4={cb[3:0],cr[3:0],y[3:0]};
	//					  5={cr[3:0],cb[3:0],y[3:0]};
	//					  6={y[3:0],cb[3:0],cr[3:0]};
	//					  7={y[3:0],cr[3:0],cb[3:0]}.
	// [	2] reverse_cr
	// [	1] reverse_cb
	// [ 0] reverse_y
	//Output 422
	if (output_color_format == HDMI_COLORSPACE_YUV422) {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (4 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (2 << 4);
		data8 |= (2 << 2);
		data8 |= (2 << 0);
	} else {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (1 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (0 << 4);
	}
	//mapping for yuv422 12bit
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX, data32 & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX + 1, (data32 >> 8) & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MASK_IVCTX, data8);
}

#define IT_CONTENT		1
static void hdmi_tx_enable_ll_mode(bool enable)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	enum hdmi_vic vic = tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic;
	u32 arg;

	if (enable) {
		if (tx_comm->base.rxcap.allm) {
			/*
			 * DV CTS case89 requirement: if IFDB with no
			 * additional VSIF support in EDID, then only
			 * send DV-VSIF, not send HF-VSIF
			 */
			if (hdmitx_dv_en(tx_comm->tx_hw) &&
				(tx_comm->base.rxcap.ifdb_present &&
					tx_comm->base.rxcap.additional_vsif_num < 1)) {
				HDMITX_INFO("%s: can't send HF-VSIF, ifdb: %d, vsif_num: %d\n",
					__func__, tx_comm->base.rxcap.ifdb_present,
					tx_comm->base.rxcap.additional_vsif_num);
				return;
			}
			tx_comm->allm_mode = 1;
			HDMITX_INFO("%s: enabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, tx_comm->base.rxcap.allm,
				tx_comm->base.rxcap.cnc3);
			hdmitx_common_setup_vsif_packet(tx_comm, VT_ALLM, 1, NULL);
			tx_comm->ct_mode = 0;
			tx_comm->it_content = 0;
			arg = SET_CT_OFF;
			hdmitx21_hw_cntl(&hdev->hw_comm, AUX_PKT_CONF_AVI_CT, (void *)&arg, NULL);
		} else if (tx_comm->base.rxcap.cnc3) {
			/* disable ALLM first if enabled */
			if (tx_comm->allm_mode == 1) {
				tx_comm->allm_mode = 0;
				HDMITX_INFO("%s: first dis-ALLM, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, tx_comm->base.rxcap.allm,
					tx_comm->base.rxcap.cnc3);
				hdmitx_common_setup_vsif_packet(tx_comm, VT_ALLM, 0, NULL);
				if (hdmitx_edid_get_hdmi14_4k_vic(vic) > 0 &&
					!hdmitx_dv_en(tx_comm->tx_hw) &&
					!hdmitx_hdr10p_en(tx_comm->tx_hw))
					hdmitx_common_setup_vsif_packet(tx_comm,
						VT_HDMI14_4K, 1, NULL);
				/*
				 * if not hdmi1.4 4k, need to sent > 4 frames and shorter than 1S
				 * HF-VSIF with allm_mode = 0, and then disable HF-VSIF according
				 * 10.2.1 HF-VSIF Transitions in hdmi2.1a. TODO:
				 */
			}
			tx_comm->ct_mode = 1;
			tx_comm->it_content = 1;
			HDMITX_INFO("%s: enabling GAME Mode, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, tx_comm->base.rxcap.allm,
				tx_comm->base.rxcap.cnc3);
			arg = SET_CT_GAME | IT_CONTENT << 4;
			hdmitx21_hw_cntl(&hdev->hw_comm, AUX_PKT_CONF_AVI_CT,
				(void *)&arg, NULL);
		} else {
			/* for safety, clear ALLM/HDMI1.X GAME if enabled */
			/* disable ALLM */
			if (tx_comm->allm_mode == 1) {
				tx_comm->allm_mode = 0;
				HDMITX_INFO("%s: disabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, tx_comm->base.rxcap.allm,
					tx_comm->base.rxcap.cnc3);
				hdmitx_common_setup_vsif_packet(tx_comm,
					VT_ALLM, 0, NULL);
				if (hdmitx_edid_get_hdmi14_4k_vic(vic) > 0 &&
					!hdmitx_dv_en(tx_comm->tx_hw) &&
					!hdmitx_hdr10p_en(tx_comm->tx_hw))
					hdmitx_common_setup_vsif_packet(tx_comm,
						VT_HDMI14_4K, 1, NULL);
			}
			/* clear content type */
			if (tx_comm->ct_mode == 1) {
				tx_comm->ct_mode = 0;
				tx_comm->it_content = 0;
				HDMITX_INFO("%s:disabling GAME Mode, enable:%d, allm:%d, cnc3:%d\n",
					__func__, enable, tx_comm->base.rxcap.allm,
					tx_comm->base.rxcap.cnc3);
				arg = SET_CT_OFF;
				hdmitx21_hw_cntl(&hdev->hw_comm, AUX_PKT_CONF_AVI_CT,
					(void *)&arg, NULL);
			}
		}
	} else {
		/* disable ALLM */
		if (tx_comm->allm_mode == 1) {
			tx_comm->allm_mode = 0;
			HDMITX_INFO("%s: disabling ALLM, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, tx_comm->base.rxcap.allm,
				tx_comm->base.rxcap.cnc3);
			hdmitx_common_setup_vsif_packet(tx_comm, VT_ALLM, 0, NULL);
			if (hdmitx_edid_get_hdmi14_4k_vic(vic) > 0 &&
				!hdmitx_dv_en(tx_comm->tx_hw) &&
				!hdmitx_hdr10p_en(tx_comm->tx_hw))
				hdmitx_common_setup_vsif_packet(tx_comm, VT_HDMI14_4K, 1, NULL);
		}
		/* clear content type */
		if (tx_comm->ct_mode == 1) {
			tx_comm->ct_mode = 0;
			tx_comm->it_content = 0;
			HDMITX_INFO("%s: disabling GAME Mode, enable:%d, allm:%d, cnc3:%d\n",
				__func__, enable, tx_comm->base.rxcap.allm,
				tx_comm->base.rxcap.cnc3);
			arg = SET_CT_OFF;
			hdmitx21_hw_cntl(&hdev->hw_comm, AUX_PKT_CONF_AVI_CT, (void *)&arg, NULL);
		}
	}
}

static enum frl_rate_enum get_current_frl_rate(void)
{
	u8 rate = hdmitx21_rd_reg(FRL_LINK_RATE_CONFIG_IVCTX) & 0xf;

	if (rate >= FRL_RATE_MAX)
		rate = FRL_NONE;

	return rate;
}

static int hdmitx_tmds_rxsense(void)
{
	int ret = 0;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S1A:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 1, 16, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 23, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 0, 24, 1);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 20, 3);
		ret = hd21_read_reg(ANACTRL_HDMIPHY_CTRL2) & 0x1;
		return ret;
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 19, 1);
		ret = !!((hd21_read_reg(ANACTRL_HDMIPHY_CTRL2) & 0xf) == 0xf);
		return ret;
	case MESON_CPU_ID_S5:
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, 1, 19, 1);
		ret = !!(hd21_read_reg(ANACTRL_HDMIPHY_CTRL2) & 0xf);
		return ret;
	default:
		break;
	}
	if (!(hdmitx21_hw_cntl(&hdev->hw_comm, PLATFORM_GET_HPD_GPI_ST, NULL, NULL)))
		return 0;
	return ret;
}

/*Check from SCDC Status_Flags_0/1 */
/* 0 means TMDS ok */
static int hdmitx_tmds_cedst(struct hdmitx21_dev *hdev)
{
	return scdc21_status_flags(hdev);
}

static bool hdmitx_frl_ready(void)
{
	enum frl_rate_enum tx_frl_rate = get_current_frl_rate();
	enum frl_rate_enum rx_frl_rate;

	/* not check frl rate under TMDS output */
	if (tx_frl_rate == FRL_NONE)
		return true;
	scdc_tx_frl_get_rx_rate((u8 *)&rx_frl_rate);
	/* check frl_rate between TX and RX */
	if (tx_frl_rate != rx_frl_rate) {
		HDMITX_ERROR("tx_frl_rate: %d, rx_frl_rate: %d\n",
			tx_frl_rate, rx_frl_rate);
		return false;
	} else {
		return true;
	}
}

static void hdmitx_disable_frl_work(struct hdmitx_common *tx_comm)
{
	if (tx_comm->tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
		frl_tx_stop();
		hdmitx_set_frl_rate_none(tx_comm);
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/* hdmi21 specific functions disable */
static void hdmitx_disable_21_work(struct hdmitx_common *tx_comm)
{
#ifdef CONFIG_AMLOGIC_DSC
	struct hdmitx21_dev *hdev = container_of(tx_comm, struct hdmitx21_dev, tx_comm);
#endif

	/* old chip not have this function */
	if (tx_comm->tx_hw->chip_data->chip_type < MESON_CPU_ID_T7)
		return;
	frl_tx_stop();
	hdmitx_set_frl_rate_none(tx_comm);
	hdmitx_vrr_disable();
#ifdef CONFIG_AMLOGIC_DSC
	if (tx_comm->tx_hw->hdmi_tx_cap.dsc_capable) {
		aml_dsc_enable(false);
		hdmitx_dsc_cvtem_pkt_disable();
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en = 0;
	}
#endif
}
#endif

/* Set Source Product Descriptor InfoFrame */
static void hdmitx_set_spd_info(struct hdmitx21_dev *hdev)
{
	int ret;
	struct hdmi_spd_infoframe spd_info = {0};
	struct vendor_info_data *vend_data;

	if (hdev->tx_comm.config_data.vend_data) {
		vend_data = hdev->tx_comm.config_data.vend_data;
	} else {
		HDMITX_DEBUG_VIDEO("packet: can\'t get vendor data\n");
		return;
	}
	if (!vend_data->vendor_name || !vend_data->product_desc)
		return;

	ret = hdmi_spd_infoframe_init(&spd_info, vend_data->vendor_name, vend_data->product_desc);
	if (ret < 0)
		HDMITX_DEBUG_VIDEO("packet: init spd failed\n");
	spd_info.sdi = HDMI_SPD_SDI_DSTB; /* fixed value for ott/stb */
	hdmi_spd_infoframe_set(&spd_info);
}

static void hdmitx_construct_avi_packet(struct hdmitx21_dev *hdev)
{
	struct hdmi_avi_infoframe *info = &hdev->tx_comm.infoframe.avi.avi;
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;
	struct rx_cap *prxcap = &hdev->tx_comm.base.rxcap;
	enum hdmi_scan_mode scan_mode = HDMI_SCAN_MODE_UNDERSCAN;
	enum hdmi_picture_aspect pic_ar = meson_tx_mode_get_vic_aspect_ratio(para->timing.vic);

	hdmi_avi_infoframe_init(info);
	info->version = 2;
	info->colorspace = para->cs;
	if (para->timing.v_active <= 576)
		info->colorimetry = HDMI_COLORIMETRY_ITU_601;
	else
		info->colorimetry = HDMI_COLORIMETRY_ITU_709;
	if (pic_ar == HDMI_PICTURE_ASPECT_4_3)
		info->picture_aspect = HDMI_PICTURE_ASPECT_4_3;
	else if (pic_ar == HDMI_PICTURE_ASPECT_NONE)
		info->picture_aspect = HDMI_PICTURE_ASPECT_NONE;
	else
		info->picture_aspect = HDMI_PICTURE_ASPECT_16_9;
	info->active_aspect = HDMI_ACTIVE_ASPECT_PICTURE;
	info->itc = 0;
	info->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_XV_YCC_601;
	info->quantization_range = HDMI_QUANTIZATION_RANGE_LIMITED;
	info->nups = HDMI_NUPS_UNKNOWN;
	info->video_code = para->timing.vic;
	if (para->timing.vic == HDMI_95_3840x2160p30_16x9 ||
	    para->timing.vic == HDMI_94_3840x2160p25_16x9 ||
	    para->timing.vic == HDMI_93_3840x2160p24_16x9 ||
	    para->timing.vic == HDMI_98_4096x2160p24_256x135)
		/* HDMI Spec V1.4b P151 */
		if (!para->tx_hw_para.hdmitx_hw_para.frl_rate)
			/* TODO, clear under FRL */
			info->video_code = 0;
	/* refer to CTA-861-H Page 69 */
	if (info->video_code >= 128)
		info->version = 3;
	info->ycc_quantization_range = HDMI_YCC_QUANTIZATION_RANGE_LIMITED;
	info->content_type = HDMI_CONTENT_TYPE_GRAPHICS;
	info->pixel_repeat = 0;
	if (para->timing.pi_mode == 0) {
		/* interlaced modes */
		if (para->timing.h_active == 1440)
			info->pixel_repeat = 1;
		if (para->timing.h_active == 2880)
			info->pixel_repeat = 3;
	}
	info->top_bar = 0;
	info->bottom_bar = 0;
	info->left_bar = 0;
	info->right_bar = 0;
	/* NO_DATA for special TV set, and underscan for other TV sets */
	if (hdmitx_find_vendor_scan_non_std(hdev->tx_comm.base.edid_buf))
		info->scan_mode = HDMI_SCAN_MODE_NONE;
	else if (hdev->tx_comm.scan_info == HDMI_SCAN_MODE_RESERVED)
		info->scan_mode = hdmitx_check_scan_info(prxcap, scan_mode, para->timing.vic);
	else
		info->scan_mode = hdev->tx_comm.scan_info;

	hdmi_avi_infoframe_set(info);
}

static enum hdmi_vic get_vic_from_pkt(struct hdmitx21_hw *tx21_hw)
{
	enum hdmi_vic vic = HDMI_0_UNKNOWN;
	struct hdmitx21_dev *hdev = container_of(tx21_hw, struct hdmitx21_dev, tx21_hw);

	/* TODO: VESA mode */
	vic = hdmitx21_rd_reg(TPI_AVI_BYTE4_IVCTX) & 0xff;
	if (vic == HDMI_0_UNKNOWN)
		vic = _get_vic_from_vsif(hdev);

	return vic;
}

static enum hdmi_colorspace get_cs_from_pkt(struct hdmitx21_hw *tx21_hw)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &tx21_hw->infoframe->avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;
	enum hdmi_colorspace cs = HDMI_COLORSPACE_RESERVED6;

	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AVI, body);
	if (ret == -1 || ret == 0) {
		HDMITX_INFO("hdmitx21: AVI not enabled %d\n", ret);
		return cs;
	}

	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0)
		HDMITX_ERROR("hdmitx21: parsing AVI failed %d\n", ret);
	else
		cs = avi->colorspace;

	return cs;
}

static enum hdmi_color_depth get_cd_from_pkt(struct hdmitx21_hw *tx21_hw)
{
	int ret;
	u8 body[32] = {0};
	union hdmi_infoframe *infoframe = &tx21_hw->infoframe->avi;
	struct hdmi_avi_infoframe *avi = &infoframe->avi;
	enum hdmi_color_depth cd = COLORDEPTH_RESERVED;
	enum hdmi_colorspace cs = HDMI_COLORSPACE_RESERVED6;

	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AVI, body);
	if (ret == -1 || ret == 0) {
		HDMITX_INFO("hdmitx21: AVI not enabled %d\n", ret);
		return cd;
	}

	ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
	if (ret < 0) {
		HDMITX_ERROR("hdmitx21: parsing AVI failed %d\n", ret);
	} else {
		cs = avi->colorspace;
		cd = _get_colordepth();
		if (cs == HDMI_COLORSPACE_YUV422)
			cd = COLORDEPTH_36B;
	}

	return cd;
}

static unsigned short get_vrr_en_from_pkt(enum emp_type type)
{
	int ret;
	u8 body[32] = {0};
	unsigned int next_tfr = 0;
	unsigned int qms_en = 0;
	unsigned int game_en = 0;
	unsigned int emp_en = 0;

	memset(body, 0, sizeof(body));
	/* game and qms use the same buff */
	ret = hdmi_emp_infoframe_get(EMP_TYPE_VRR_QMS, body);
	if (ret <= 0)
		return 0;

	switch (type) {
	case EMP_TYPE_NONE:
		/* emp enable and emp head */
		if (body[31] == 0xe0 &&
			body[0] == HDMI_INFOFRAME_TYPE_EMP)
			emp_en = 1;
		return emp_en;
	case EMP_TYPE_VRR_QMS:
		qms_en = !!(body[10] & BIT(2));
		if (qms_en)
			next_tfr = (body[12] >> 3) & 0xf;
		if (qms_en && next_tfr)
			return 1;
		break;
	case EMP_TYPE_VRR_GAME:
		game_en = !!(body[10] & BIT(0));
		return game_en;
	default:
		break;
	}

	return 0;
}

static void hdmi_phy_suspend(void)
{
	u32 phy_cntl0 = ANACTRL_HDMIPHY_CTRL0;
	u32 phy_cntl3 = ANACTRL_HDMIPHY_CTRL3;
	u32 phy_cntl5 = ANACTRL_HDMIPHY_CTRL5;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S1A ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6)
		phy_cntl5 = ANACTRL_HDMIPHY_CTRL5;
	else
		phy_cntl5 = ANACTRL_HDMIPHY_CTRL6;
	/* When HDMI PHY is turned off, the arc power
	 * supply module still need to work, otherwise
	 * hdmi arc disables, no audio during HEACT 5-18
	 */
	if (!hdev->tx_comm.suspend_flag && hdev->tx_comm.arc_rx_en)
		hd21_write_reg(phy_cntl0, 0x10000);
	else
		hd21_write_reg(phy_cntl0, 0x0);
	/* keep PHY_CNTL3 bit[1:0] as 0b11,
	 * otherwise may cause HDCP22 boot failed
	 */
	/* for s7 need keep PHY_CNTL3 bit[3:0] as 1011(B)
	 * keep tmds_clk, because hdcp14 certification requires tmds_clk,
	 * otherwise it may poll fail lead to crash.
	 */
	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S7:
		hd21_write_reg(phy_cntl3, 0xB);
		hd21_write_reg(phy_cntl5, 0x800);
		break;
	case MESON_CPU_ID_S7D:
		hd21_write_reg(phy_cntl3, 0xC1B);
		hd21_write_reg(phy_cntl5, 0x0);
		break;
	case MESON_CPU_ID_S6:
		/*
		 * s6 needed to optimize power consumption at early suspend,
		 * writing 0 to PHY_CNTL3 can optimize power consumption,
		 * and disable tmds_clk
		 */
		hd21_write_reg(phy_cntl3, 0x0);
		hd21_write_reg(phy_cntl5, 0x0);
		break;
	case MESON_CPU_ID_S5:
		/* keep clk_todig enabled to optimise 10to20 fifo over/under flow issue */
		hdmitx_s5_phy_keep_clk_todig(true);
		break;
	default:
		hd21_write_reg(phy_cntl3, 0x3);
		hd21_write_reg(phy_cntl5, 0x800);
		break;
	}
}

static void hdmi_phy_wakeup(struct hdmitx21_dev *hdev)
{
	hdmitx_set_phy(hdev);
}

static int hdmitx21_check_input_argv(u32 cmd, void *input_argv)
{
	if (!input_argv) {
		HDMITX_ERROR("cmd[0x%x] null input arg\n", cmd);
		return -1;
	}
	return 0;
}

static int hdmitx21_check_output_argv(u32 cmd, void *output_struct)
{
	if (!output_struct) {
		HDMITX_ERROR("cmd[0x%x] null output_struct\n", cmd);
		return -1;
	}
	return 0;
}

static int hdmitx21_hw_cntl_ddc(struct hdmitx_hw_common *tx_hw, u32 cmd,
			      void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_DDC_AUX_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}
	switch (cmd) {
	case DDC_PIN_MUX_OP:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == PIN_MUX)
			hdmitx21_ddc_hw_op(&hdev->tx21_hw, DDC_MUX_DDC);
		break;
	case DDC_RESET_EDID:
		break;
	case DDC_EDID_READ_DATA:
		hdmitx21_read_edid(tx_comm);
		break;
	case DDC_GLITCH_FILTER_RESET:
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 6, 1);
		/*keep resetting DDC for some time*/
		usleep_range(1000, 2000);
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 6, 1);
		/*wait recover for resetting DDC*/
		usleep_range(1000, 2000);
		break;
	/*
	 * AON_CYP_CTL_IVCTX: i2c clk div, default value = 0x2, means 75k
	 * DDC_DELAY_CNT_IVCTX: DDC I2C delay counter, defalut 0x26
	 * config i2c 50k: AON_CYP_CTL_IVCTX as default, DDC_DELAY_CNT_IVCTX as 0x3d
	 */
	case DDC_I2C_RATE:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		switch (arg) {
		case DDC_I2C_25K:
			/* config i2c 25k */
			hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 3, 0, 2);
			hdmitx21_wr_reg(DDC_DELAY_CNT_IVCTX, 0x3d);
			break;
		case DDC_I2C_38K:
			/* config i2c 37.5k */
			hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 3, 0, 2);
			hdmitx21_wr_reg(DDC_DELAY_CNT_IVCTX, 0x26);
			break;
		case DDC_I2C_75K:
			/* config i2c 75k */
			hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 2, 0, 2);
			hdmitx21_wr_reg(DDC_DELAY_CNT_IVCTX, 0x26);
			break;
		case DDC_I2C_50K:
		default:
			/* config i2c 50k */
			hdmitx21_set_reg_bits(AON_CYP_CTL_IVCTX, 2, 0, 2);
			hdmitx21_wr_reg(DDC_DELAY_CNT_IVCTX, 0x3d);
			break;
		}
		break;
	case DDC_SCDC_DIV40_SCRAMB:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == 1) {
			hdmitx_set_scdc_div40(1);
		} else if (arg == 0) {
			if (tx_comm->base.rxcap.scdc_present)
				hdmitx_set_scdc_div40(0);
			else
				HDMITX_INFO("SCDC not present, should not send 1:10\n");
		} else {
			/* force send 1:10 tmds bit clk ratio, for echo 2 > div40 */
			hdmitx_set_scdc_div40(0);
		}
		tx_comm->tx_hw->pre_tmds_clk_div40 = (arg == 1);
		break;
	case DDC_GET_CEDST:
		return hdmitx_tmds_cedst(hdev);
	case DDC_I2C_RESET:
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 1, 9, 1);
		usleep_range(1000, 2000);
		hdmitx21_set_reg_bits(HDMITX_TOP_SW_RESET, 0, 9, 1);
		usleep_range(1000, 2000);
		break;
	case DDC_SCDC_STS_FLAG0:
		ret = hdmitx21_rd_reg(SCDC_STS_FLAG0_IVCTX);
		break;
	case DDC_SCDC_LN0_LN1_LTP:
		ret = hdmitx21_rd_reg(FRL_LTP_OVR_VAL0_IVCTX);
		break;
	case DDC_SCDC_LN2_LN3_LTP:
		ret = hdmitx21_rd_reg(FRL_LTP_OVR_VAL1_IVCTX);
		break;
	default:
		break;
	}
	return ret;
}

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
static int hdmitx21_hw_cntl_hdcp(struct hdmitx_hw_common *tx_hw, u32 cmd,
			      void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;
	struct hdcptx_api *hdcptx_api = hdcptx_comm->hdcptx_api;
	u32 arg;
	int ret = 0;
	int hdcprx_ver = 0;
	u8 *ksv_byte = NULL;
	u8 *bstatus = NULL;
	u8 *an = NULL;

	if ((cmd & CMD_TYPE_MASK) != CMD_HDCP_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}
	switch (cmd) {
	case HDCP_SET_TOPO_INFO:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (hdcptx_api->hdcptx22_topo_ctrl)
			hdcptx_api->hdcptx22_topo_ctrl(hdcptx_comm, SET_HDCP22_TOPO, !!arg);
		break;
	case HDCP_GET_TOPO_INFO:
		if (hdcptx_api->hdcptx22_topo_ctrl)
			return (int)hdcptx_api->hdcptx22_topo_ctrl(&tx_comm->hdcptx_comm,
				GET_HDCP22_TOPO, 0);
		break;
	case HDCP_GET_AKSV:
		ret = hdmitx21_check_output_argv(cmd, output_struct);
		if (ret < 0)
			break;
		ksv_byte = (u8 *)output_struct;
		if (hdcptx_api->hdcptx14_get_aksv)
			hdcptx_api->hdcptx14_get_aksv(ksv_byte, 5);
		break;
	case HDCP_GET_BKSV:
		ret = hdmitx21_check_output_argv(cmd, output_struct);
		if (ret < 0)
			break;
		ksv_byte = (u8 *)output_struct;
		if (hdcptx_api->hdcptx14_get_bksv)
			hdcptx_api->hdcptx14_get_bksv(hdcptx_comm, ksv_byte, 5);
		break;
	case HDCP14_GET_AN:
		ret = hdmitx21_check_output_argv(cmd, output_struct);
		if (ret < 0)
			break;
		an = (u8 *)output_struct;
		if (hdcptx_api->hdcptx14_get_an)
			hdcptx_api->hdcptx14_get_an(an, 8);
		break;
	case HDCP14_GET_BCAPS:
		ret = hdmitx21_rd_reg(TPI_DS_BCAPS_IVCTX);
		return ret;
	case HDCP14_GET_BSTATUS:
		ret = hdmitx21_check_output_argv(cmd, output_struct);
		if (ret < 0)
			break;
		bstatus = (u8 *)output_struct;
		if (hdcptx_api->hdcptx14_get_bstatus)
			hdcptx_api->hdcptx14_get_bstatus(bstatus);
		break;
	case HDCP14_GET_RI:
		if (hdcptx_api->hdcptx14_get_ri)
			return hdcptx_api->hdcptx14_get_ri();
		break;
	case HDCP_14_LSTORE:
		if (hdcptx_api->get_hdcptx_lstore)
			return hdcptx_api->get_hdcptx_lstore(hdcptx_comm, 1);
		break;
	case HDCP_22_LSTORE:
		if (hdcptx_api->get_hdcptx_lstore)
			return hdcptx_api->get_hdcptx_lstore(hdcptx_comm, 2);
		break;
	case HDCP22_GET_RX_VER:
		if (hdcptx_api->get_hdcprx_ver)
			hdcprx_ver = hdcptx_api->get_hdcprx_ver(hdcptx_comm);
		return (hdcprx_ver == 2) ? true : false;
	case HDCP_GET_AUTH_RESULT:
		if (hdcptx_comm->hdcp_mode == 1) {
			if (hdcptx_api->get_hdcptx_result)
				ret = hdcptx_api->get_hdcptx_result(hdcptx_comm, 1);
		} else if (hdcptx_comm->hdcp_mode == 2) {
			if (hdcptx_api->get_hdcptx_result)
				ret = hdcptx_api->get_hdcptx_result(hdcptx_comm, 2);
		}
		return ret;
	case HDCP_REQ_AUTH:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u8 *)input_argv);
		if (hdcptx_api->hdcptx_reauth_req)
			return hdcptx_api->hdcptx_reauth_req(tx_comm->hdcptx_priv, arg & 0xFF);
		break;
	case HDCP14_KEY_LOAD:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((bool *)input_argv);
		hdcptx_comm->hdcp14_key_loaded = !!arg;
		break;
	case HDCP_PARAM_RESET:
		if (hdcptx_api->hdcptx_reset_param)
			hdcptx_api->hdcptx_reset_param(tx_comm->hdcptx_priv);
		break;
	case HDCP_DISABLE:
		if (hdcptx_api->hdcptx_disable)
			hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
		break;
	case HDCP_ENABLE:
		if (tx_comm->hdcptx_comm.hdcp_ctl_lvl == 0) {
			if (hdcptx_api->hdcptx_enable)
				hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv,
				tx_comm->hdcptx_comm.hdcptx_start_type, 0);
		} else {
			/* for drm stop/disconnect state judgment*/
			tx_comm->hdcptx_comm.hdcp_state_mode = HDCP_STATE_START;
			drm_hdmitx_common_hdcp_enable(tx_comm,
				tx_comm->hdcptx_comm.hdcptx_start_type);
		}
		break;
	default:
		break;
	}
	return ret;
}
#endif

static int hdmitx21_hw_cntl_pkt(struct hdmitx_hw_common *tx_hw, u32 cmd,
			      void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	u32 arg;
	int ret = 0;
	u8 *pkt_byte = NULL;
	struct emp_packet_st sbtm_emp;
	struct vtem_sbtm_st *sbtm_para;
	struct emp_packet_st *qms_emp = NULL;
	struct emp_packet_st *game_emp = NULL;
	int md_data_length = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_AUX_PKT_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case AUX_PKT_CONFIG_AVMUTE:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		config_avmute(arg);
		break;
	case AUX_PKT_GET_AVMUTE_ST:
		return read_avmute();
	case AUX_PKT_CLR_AVI:
		break;
	case AUX_PKT_CONF_AVI_ASPECT:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_SET_AVI_VIC, arg >> 2);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_ASPECT,
			arg & 0x3);
		break;
	case AUX_PKT_CONF_AVI_BT2020:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_BT2020, arg);
		break;
	case AUX_PKT_SET_AVI_VIC:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_SET_AVI_VIC, arg & 0xff);
		break;
	case AUX_PKT_GET_AVI_COLORIMETRY:
		ret = hdmitx_get_colorimetry_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_EXTENDED_COLORIMETRY:
		ret = hdmitx_get_extended_colorimetry_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_VIDEO_CODE:
		ret = hdmitx_get_vic_from_avi(hdev);
		break;
	case AUX_PKT_CONF_AVI_SCAN:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_SCAN, arg & 0x3);
		break;
	case AUX_PKT_GET_AVI_SCAN:
		ret = hdmitx_get_scan_info_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_Q01:
		ret = hdmitx_get_quantization_range_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_YQ01:
		ret = hdmitx_get_ycc_quantization_range_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_PICTURE_ASPECT:
		ret = hdmitx_get_picture_aspect_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_ACTIVE_ASPECT:
		ret = hdmitx_get_active_aspect_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_NUPS:
		ret = hdmitx_get_nups_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_ITC:
		ret = hdmitx_get_itc_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_CT_TYPE:
		ret = hdmitx_get_content_type_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_PIXEL_REPEAT:
		ret = hdmitx_get_pixel_repeat_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_TOP_BAR:
		ret = hdmitx_get_top_bar_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_BOTTOM_BAR:
		ret = hdmitx_get_bottom_bar_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_LEFT_BAR:
		ret = hdmitx_get_left_bar_from_avi(hdev);
		break;
	case AUX_PKT_GET_AVI_RIGHT_BAR:
		ret = hdmitx_get_right_bar_from_avi(hdev);
		break;
	case AUX_PKT_CLR_DV_VS10_SIG:
		break;
	case AUX_PKT_SET_AVI_CS:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_SET_AVI_CS, arg);
		break;
	case AUX_PKT_CONF_AVI_Q01:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_Q01, arg);
		break;
	case AUX_PKT_CONF_AVI_YQ01:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_YQ01, arg);
		break;
	case AUX_PKT_CONF_AVI_CT:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmi_avi_infoframe_config(tx_comm, AUX_PKT_CONF_AVI_CT, arg);
		break;
	case AUX_PKT_SET_SPD_INFO:
		hdmitx_set_spd_info(hdev);
		break;
	case AUX_PKT_SET_VSIF1:
	case AUX_PKT_SET_VSIF2:
		if (!input_argv) {
			if (cmd == AUX_PKT_SET_VSIF2)
				hdmi_vend_infoframe2_rawset(tx_comm, NULL);
			else
				hdmi_vend_infoframe_rawset(tx_comm, NULL);
			break;
		}
		pkt_byte = (u8 *)input_argv;
		/* vsif_db[0] is checksum, requires software calculation */
		/* TODO: memcpy(&vsif_db[1], DB, 27); */
		if (cmd == AUX_PKT_SET_VSIF2)
			hdmi_vend_infoframe2_rawset(tx_comm, pkt_byte);
		else
			hdmi_vend_infoframe_rawset(tx_comm, pkt_byte);
		break;
	case AUX_PKT_GET_VSIF_IEEEOUI:
		ret = hdmitx_get_ieeeoui_from_vsif(hdev);
		break;
	case AUX_PKT_GET_VSIF_VIC:
		ret = _get_vic_from_vsif(hdev);
		break;
	case AUX_PKT_GET_VSIF_ALLM:
		ret = hdmitx_get_allm_from_vsif(hdev);
		break;
	case AUX_PKT_GET_VSIF_AMDV_SIGNAL:
		ret = hdmitx_get_amdv_signal_from_vsif(hdev);
		break;
	case AUX_PKT_GET_VSIF_AMDV_LOW_LATENCY:
		ret = hdmitx_get_amdv_low_latency_from_vsif(hdev);
		break;
	case AUX_PKT_SET_DRM:
		if (!input_argv) {
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, NULL);
			break;
		}
		pkt_byte = (u8 *)input_argv;
		/* drm_db[0] is checksum, requires software calculation */
		/* TODO: memcpy(&drm_db[1], DB, 26); */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, pkt_byte);
		break;
	case AUX_PKT_GET_DRM_EOTF:
		ret = hdmitx_get_eotf_from_drm(hdev);
		break;
	case AUX_PKT_GET_AUDIO_N:
		ret = hdmitx_get_n_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_CTS:
		ret = hdmitx_get_cts_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_LAYOUT:
		/*
		 * bit1
		 * HDMI Audio Packet layout indicator:
		 *   0 - Layout 0 (2-channel) (default)
		 *   1 - Layout 1 (Up to 8-channel)
		 */
		ret = hdmitx21_rd_reg(AUDP_TXCTRL_IVCTX) & 0x2;
		break;
	case AUX_PKT_GET_AUDIO_CHANNEL:
		ret = hdmitx_get_channel_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_CODING_TYPE:
		ret = hdmitx_get_coding_type_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_SAMPLE_SIZE:
		ret = hdmitx_get_sample_size_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_CODING_FREQUENCY:
		ret = hdmitx_get_sample_frequency_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_CODING_TYPE_EXT:
		ret = hdmitx_get_coding_type_ext_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_CHANNEL_ALLOCATION:
		ret = hdmitx_get_channel_allocation_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_LEVEL_SHIFT_VALUE:
		ret = hdmitx_get_level_shift_value_from_audio(hdev);
		break;
	case AUX_PKT_GET_AUDIO_DOWNMIX_INHIBIT:
		ret = hdmitx_get_downmix_inhibit_from_audio(hdev);
		break;
	case AUX_PKT_GET_SPD_SDI:
		ret = hdmitx_get_sdi_from_spd(hdev);
		break;
	case AUX_PKT_SET_EMP_CUVA:
		if (!input_argv) {
			hdmitx_dhdr_send(NULL, 0);
			break;
		}
		pkt_byte = (u8 *)input_argv;
		/* Data_Set_Length_MSB and Data_Set_Length_LSB */
		md_data_length = pkt_byte[8] << 8 | pkt_byte[9];
		/*
		 * 1. no_used of vs_emds[0] and vs_emds[1] also need to be counted into
		 * the length for data alignment
		 * 2. HB0 HB1 HB2 of vs_emds[1] and vs_emds[2] also need to be counted into
		 * the length for data alignment
		 */
		hdmitx_dhdr_send(pkt_byte, md_data_length + CUVA_EMP_OFFSET);
		break;
	case AUX_PKT_GET_QMS_BASE_REFRESH_RATE:
		ret = hdmitx_get_base_refresh_rate_from_emp(hdev);
		break;
	case AUX_PKT_GET_QMS_M_CONST:
		ret = hdmitx_get_m_const_from_emp(hdev);
		break;
	case AUX_PKT_GET_QMS_NEXT_TFR:
		ret = hdmitx_get_next_tfr_from_emp(hdev);
		break;
	case AUX_PKT_SET_EMP_SBTM:
		if (!input_argv) {
			hdmi_emp_infoframe_set(EMP_TYPE_SBTM, NULL);
			break;
		}
		sbtm_para = (struct vtem_sbtm_st *)(input_argv);
		memset(&sbtm_emp, 0, sizeof(sbtm_emp));
		sbtm_emp.type = EMP_TYPE_SBTM;
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_HEADER_INIT,
			HDMI_INFOFRAME_TYPE_EMP);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_HEADER_FIRST, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_HEADER_LAST, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_HEADER_SEQ_INDEX, 0);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_DS_TYPE, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SYNC, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_VFR, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_AFR, 0);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_NEW, 0);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_END, 0);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_ORG_ID, 1);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_DATA_SET_TAG, 3);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_DATA_SET_LENGTH, 4);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_VER, sbtm_para->sbtm_ver);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_MODE, sbtm_para->sbtm_mode);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_TYPE, sbtm_para->sbtm_type);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_GRDM_MIN,
			sbtm_para->grdm_min);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_GRDM_LUM,
			sbtm_para->grdm_lum);
		hdmi_emp_frame_set_member(&sbtm_emp, CONF_SBTM_FRMPBLIMITINT,
			sbtm_para->frmpblimitint);
		hdmi_emp_infoframe_set(EMP_TYPE_SBTM, &sbtm_emp);
		break;
	case AUX_PKT_CONF_EMP_VRR_QMS:
		if (!input_argv) {
			hdmi_emp_infoframe_set(EMP_TYPE_VRR_QMS, NULL);
			break;
		}
		qms_emp = (struct emp_packet_st *)(input_argv);
		hdmi_emp_infoframe_set(EMP_TYPE_VRR_QMS, qms_emp);
		break;
	case AUX_PKT_CONF_EMP_VRR_GAME:
		if (!input_argv) {
			hdmi_emp_infoframe_set(EMP_TYPE_VRR_GAME, NULL);
			break;
		}
		game_emp = (struct emp_packet_st *)(input_argv);
		hdmi_emp_infoframe_set(EMP_TYPE_VRR_GAME, game_emp);
		break;
	case AUX_PKT_AVI_CONSTRUCT:
		hdmitx_construct_avi_packet(hdev);
		break;
	case AUX_PKT_GET_AVI_VIC:
		return (int)get_vic_from_pkt(&hdev->tx21_hw);
	case AUX_PKT_GET_AVI_CS:
		return (int)get_cs_from_pkt(&hdev->tx21_hw);
	case AUX_PKT_GET_GCP_CD:
		return (int)get_cd_from_pkt(&hdev->tx21_hw);
	case AUX_PKT_GET_HDR_ST:
		return hdmitx21_get_cur_hdr_st();
	case AUX_PKT_GET_AMDV_ST:
		return hdmitx21_get_cur_dv_st();
	case AUX_PKT_GET_HDR10P_ST:
		return hdmitx21_get_cur_hdr10p_st();
	case AUX_PKT_GET_CUVA_ST:
		return hdmitx21_get_cur_cuva_st();
	case PLATFORM_GET_TMDS_CLK:
		if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
			ret = meson_clk_measure(92);
		else
			ret = meson_clk_measure(76);
		break;
	case PLATFORM_GET_PIXEL_CLK:
		if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
			ret = meson_clk_measure(68);
		else
			ret = meson_clk_measure(59);
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_audio(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_AUX_AUD_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}
	switch (cmd) {
	case AUDIO_MUTE_OP:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		audio_mute_op(arg == AUDIO_MUTE ? 0 : 1);
		break;
	case AUDIO_GET_UBOOT_ST:
		return hdmitx_uboot_audio_en();
	case AUDIO_PREPARE:
		/* mute aud sample */
		hdmitx21_set_reg_bits(AUDP_TXCTRL_IVCTX, 1, 7, 1);
		break;
	case AUDIO_ACR_CTRL:
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_allm(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_ALLM_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case ALLM_CONFIG:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == ENABLE_ALLM)
			hdmi_tx_enable_ll_mode(true);
		else if (arg == DISABLE_ALLM)
			hdmi_tx_enable_ll_mode(false);
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_frl(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_FRL_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case FRL_GET_MODE:
		return (int)get_current_frl_rate();
	case FRL_CLR_MODE:
		hdmitx21_set_reg_bits(FRL_LINK_RATE_CONFIG_IVCTX, 0, 0, 4);
		break;
	case FRL_GET_RX_READY_ST:
		return (int)hdmitx_frl_ready();
	case FRL_DISABLE_WORK:
		hdmitx_disable_frl_work(tx_comm);
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_dsc(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_DSC_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case DSC_GET_TX_EN:
#ifdef CONFIG_AMLOGIC_DSC
		return get_dsc_en();
#else
		return 0;
#endif
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_vrr(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	unsigned short qms_en;
	unsigned short game_en;
	unsigned short emp_en;
	unsigned short value_brr;
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_VRR_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case VRR_REGISTER:
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == 0)
			hdmitx_unregister_vrr(tx_comm->enc_idx);
		else
			hdmitx_register_vrr(hdev);
#endif
		break;
	case QMS_GET_INFO:
		value_brr = (unsigned short)get_vic_from_pkt(&hdev->tx21_hw);
		qms_en = (unsigned short)get_vrr_en_from_pkt(EMP_TYPE_VRR_QMS);
		return (qms_en << 16) | value_brr;
	case GAME_GET_INFO:
		game_en = (unsigned short)get_vrr_en_from_pkt(EMP_TYPE_VRR_GAME);
		return game_en;
	case EMP_GET_INFO:
		emp_en = (unsigned short)get_vrr_en_from_pkt(EMP_TYPE_NONE);
		return emp_en;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_core_misc(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_CORE_MISC_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case CORE_MISC_SET_HDMI_DVI_MODE:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmitx_hdmi_dvi_config(hdev, (arg == DVI_MODE) ? 1 : 0);
		break;
	case CORE_MISC_GET_HDMI_DVI_MODE:
		ret = hdmitx_get_hdmi_dvi_config(hdev);
		break;
	case CORE_MISC_HW_INIT:
		if (tx_hw->chip_data->chip_type >= MESON_CPU_ID_S7) {
			ret = hdmitx21_check_input_argv(cmd, input_argv);
			if (ret < 0)
				break;
			arg = *((bool *)input_argv);
			hdmi_hwp_init(hdev, !!arg);
		}
		break;
	case CORE_MISC_VP_CMS_CSC:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmitx21_vp_cms_csc0_multi_csc_config(arg);
		break;
	case CORE_MISC_TRIGGER_HPD:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == 1)
			hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT, 1 << 1);
		else
			hdmitx21_wr_reg(HDMITX_TOP_INTR_STAT, 1 << 2);
		break;
	case CORE_MISC_GET_OUTPUT_ST:
		return hdmitx21_uboot_already_display(&hdev->tx21_hw);
	case CORE_MISC_EVENT_PROCESS:
		hdmitx_core_intr_process(&hdev->tx_comm);
		break;
	case CORE_MISC_HDCP_SUSPEND:
		arg = false;
		hdmitx_hw_cntl(tx_hw, HDCP_DISABLE, NULL, NULL);
		hdmitx_hw_cntl(tx_hw, PLATFORM_HDMI_CLKS_CTRL, (void *)&arg, NULL);
		break;
	case CORE_MISC_DISABLE_ALL_IRQ:
		hdmitx21_disable_all_irq();
		break;
	case CORE_MISC_SUSPEND_RESUME_CNTL:
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_platform(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_PLATFORM_CNTL_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case PLATFORM_CLK_DIV_RST:
		/* TO confirm if only for S5 */
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		hdmitx21_s5_clk_div_rst(arg);
#endif
		break;
	case PLATFORM_GET_HPD_GPI_ST:
		return hdmitx21_hpd_hw_op(&hdev->tx21_hw, HPD_READ_HPD_GPIO);
	case PLATFORM_PHY_OP:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == TMDS_PHY_ENABLE) {
			hdmitx_phy_pre_init(hdev);
			hdmi_phy_wakeup(hdev);
			hdmitx_fifo_flow_enable_intr(1);
		} else if (arg == TMDS_PHY_DISABLE) {
			/* may be executed in vsync interrupt */
			hdmitx_fifo_flow_enable_intr(0);
			hdev->ignore_fifo_intr5 = true;
			hdmi_phy_suspend();
		}
		break;
	case PLATFORM_RXSENSE:
		return hdmitx_tmds_rxsense();
	case PLATFORM_HDMI_CLKS_CTRL:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((bool *)input_argv);
		hdmitx21_clks_gate_ctrl(!!arg);
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_vpu(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	u32 arg;
	int ret = 0;
	char *connector_type = NULL;
	int type_val = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_VPU_CNTL_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case VPU_VIDEO_MUTE_OP:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		connector_type = get_uboot_connector0_type();
		if (connector_type)
			type_val = convert_connector_type_to_val(connector_type);
		if (arg == VIDEO_MUTE) {
			if (tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
				/* T7 use vpp mute pattern when hdmi is the main screen */
				if (type_val == DRM_MODE_CONNECTOR_MESON_HDMIA_A ||
					type_val == DRM_MODE_CONNECTOR_MESON_HDMIA_C) {
					set_output_mute(true);
				} else {
					hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
					hd21_write_reg(VENC_VIDEO_TST_EN, 1);
					hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
					hd21_write_reg(VENC_VIDEO_TST_Y, 0x0);
					hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
					hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
				}
			} else {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 1);
				hd21_write_reg(VENC_VIDEO_TST_MDSEL, 0);
				hd21_write_reg(VENC_VIDEO_TST_Y, 0x0);
				hd21_write_reg(VENC_VIDEO_TST_CB, 0x200);
				hd21_write_reg(VENC_VIDEO_TST_CR, 0x200);
			}
		} else if (arg == VIDEO_UNMUTE) {
			if (tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
				/* T7 use vpp mute pattern when hdmi is the main screen*/
				if (type_val == DRM_MODE_CONNECTOR_MESON_HDMIA_A ||
					type_val == DRM_MODE_CONNECTOR_MESON_HDMIA_C) {
					set_output_mute(false);
				} else {
					hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
					hd21_write_reg(VENC_VIDEO_TST_EN, 0);
				}
			} else {
				hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 1, 3, 1);
				hd21_write_reg(VENC_VIDEO_TST_EN, 0);
			}
		}
		break;
	case VPU_CONFIG_CSC_EN:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		if (arg == CSC_ENABLE)
			tx_comm->hdr_state->config_csc_en = true;
		if (arg == CSC_DISABLE)
			tx_comm->hdr_state->config_csc_en = false;
		break;
	case VPU_CONFIG_CSC:
		if (!tx_comm->hdr_state->config_csc_en) {
			hdev->tx_comm.hdr_state->csc_config_in_next_frame = false;
			break;
		}
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((u32 *)input_argv);
		/* the csc process is executed in the vsync interrupt */
		tx_comm->hdr_state->csc_config_in_next_frame = true;
		tx_comm->hdr_state->csc_delay_frame = 0;
		tx_comm->hdr_state->output_color_format = arg;
		break;
	case VPU_CONFIG_DITHER:
		hdmitx21_dither_config(hdev);
		break;
	default:
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl_flow_misc(struct hdmitx_hw_common *tx_hw, u32 cmd,
				  void *input_argv, void *output_struct)
{
	struct hdmitx21_dev *hdev = container_of(tx_hw, struct hdmitx21_dev, hw_comm);
	struct hdmitx_common *tx_comm = &hdev->tx_comm;
	u32 arg;
	int ret = 0;

	if ((cmd & CMD_TYPE_MASK) != CMD_MODE_FLOW_MISC_OFFSET) {
		HDMITX_ERROR("%s cmd[0x%x] wrong cmd type\n", __func__, cmd);
		return -1;
	}

	switch (cmd) {
	case MODE_FLOW_HPD_IRQ_TOP_HALF:
		ret = hdmitx21_check_input_argv(cmd, input_argv);
		if (ret < 0)
			break;
		arg = *((bool *)input_argv);
		hdmitx_hpd_irq_top_half_process(hdev, !!arg);
		break;
	case MODE_FLOW_PRIVATE_PLUGOUT_PROCESS:
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
		if (tx_comm->hdcptx_comm.hdcptx_api->hdcptx_reset_param)
			tx_comm->hdcptx_comm.hdcptx_api->hdcptx_reset_param(tx_comm->hdcptx_priv);
#endif
		/* for vsync loss when HPD loss */
		hdmitx21_vid_pll_clk_check(tx_comm->tx_hw);
		/*
		 * Reset the ll_enabled_in_auto_mode flag used for auto mode
		 * status. If we are in auto mode, gaming signal should be enabled
		 * when the request arrives again from the input device or playback
		 * and not on hotplug.
		 */
		tx_comm->ll_enabled_in_auto_mode = false;
		break;
	case MODE_FLOW_PRE_ENABLE_MODE:
		return hdmitx21_pre_enable_mode(tx_comm);
	case MODE_FLOW_ENABLE_MODE:
		return hdmitx21_enable_mode(tx_comm);
	case MODE_FLOW_POST_ENABLE_MODE:
		return hdmitx21_post_enable_mode(tx_comm);
	case MODE_FLOW_DISABLE_21_WORK:
		hdmitx_disable_21_work(tx_comm);
		break;
	}
	return ret;
}

static int hdmitx21_hw_cntl(struct hdmitx_hw_common *tx_hw, u32 cmd,
			      void *input_argv, void *output_struct)
{
	int ret = 0;
	u32 cmd_type = cmd & CMD_TYPE_MASK;

	switch (cmd_type) {
	case CMD_DDC_AUX_OFFSET:
		ret = hdmitx21_hw_cntl_ddc(tx_hw, cmd, input_argv, output_struct);
		break;
	case  CMD_HDCP_OFFSET:
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
		ret = hdmitx21_hw_cntl_hdcp(tx_hw, cmd, input_argv, output_struct);
#endif
		break;
	case CMD_AUX_PKT_OFFSET:
		ret = hdmitx21_hw_cntl_pkt(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_AUX_AUD_OFFSET:
		ret = hdmitx21_hw_cntl_audio(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_ALLM_OFFSET:
		ret = hdmitx21_hw_cntl_allm(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_FRL_OFFSET:
		ret = hdmitx21_hw_cntl_frl(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_DSC_OFFSET:
		ret = hdmitx21_hw_cntl_dsc(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_VRR_OFFSET:
		ret = hdmitx21_hw_cntl_vrr(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_CORE_MISC_OFFSET:
		ret = hdmitx21_hw_cntl_core_misc(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_PLATFORM_CNTL_OFFSET:
		ret = hdmitx21_hw_cntl_platform(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_VPU_CNTL_OFFSET:
		ret = hdmitx21_hw_cntl_vpu(tx_hw, cmd, input_argv, output_struct);
		break;
	case CMD_MODE_FLOW_MISC_OFFSET:
		ret = hdmitx21_hw_cntl_flow_misc(tx_hw, cmd, input_argv, output_struct);
		break;
	}
	return ret;
}

static int hdmi_move_hdr_pkt(bool flag)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	enum hdmi_vic vic = hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.vic;
	const struct tx_timing *timing;
	u8 move_val = 0;

	/* Only S7 and later SOCs have this function */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7 ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S7D ||
			hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S6) {
		if (flag) {
			timing = meson_tx_mode_vic_to_timing(vic);
			if (timing) {
				move_val = timing->v_front + timing->v_sync + 1;
				HDMITX_INFO("vic = %d, move_val = %d\n", vic, move_val);
				/* Move HDR PKT behind VSYNC */
				pkt_send_position_change(0x40, move_val);
			}
		} else {
			/* Restore HDR PKT to default position */
			hdmitx21_wr_reg(PKT_AUTO_0_IVCTX,
					hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) | 0x40);
			hdmitx21_wr_reg(PKT_LOC_GEN_IVCTX, 0);
		}
	}
	return 0;
}

#define COLORDEPTH_24B            4

#define HDMI_COLOR_FORMAT_RGB 0
#define HDMI_COLORSPACE_YUV422           1
#define HDMI_COLOR_FORMAT_444           2
#define HDMI_COLORSPACE_YUV420           3

#define HDMI_COLOR_RANGE_LIM 0
#define HDMI_COLOR_RANGE_FUL            1

#define HDMI_AUDIO_PACKET_SMP 0x02
#define HDMI_AUDIO_PACKET_1BT 0x07
#define HDMI_AUDIO_PACKET_DST 0x08
#define HDMI_AUDIO_PACKET_HBR 0x09
#define HDMI_AUDIO_PACKET_MUL 0x0e

static void hdmitx_enable_null_pkt(struct hdmitx21_dev *hdev)
{
	u32 *edid_ptr = (u32 *)hdev->tx_comm.base.edid_buf;
	struct arm_smccc_res res;

	arm_smccc_smc(HDCPTX_IOOPR, CHECK_SPEC_EDID, edid_ptr[2],
				edid_ptr[3], edid_ptr[4], 0, 0, 0, &res);
}

/*
 * color_depth: Pixel bit width: 4=24-bit; 5=30-bit; 6=36-bit; 7=48-bit.
 * input_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420.
 * input_color_range: 0=limited; 1=full.
 * output_color_range: 0=limited; 1=full.
 * output_color_format: 0=RGB444; 1=YCbCr422; 2=YCbCr444; 3=YCbCr420
 */
static void config_hdmi21_tx(struct hdmitx21_dev *hdev)
{
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;
	u8 color_depth = COLORDEPTH_24B;
	u8 input_color_format = HDMI_COLORSPACE_YUV444;
	u8 input_color_range = HDMI_QUANTIZATION_RANGE_LIMITED;
	u8 output_color_format = HDMI_COLORSPACE_YUV444;
	u8 output_color_range = HDMI_QUANTIZATION_RANGE_LIMITED;
	u32 active_pixels = 1920;
	u32 active_lines = 1080;
	u32 data32;
	u8 data8;
	u8 csc_en;
	u8 dp_color_depth = 0;

	color_depth = para->cd;
	output_color_format = para->cs;
	active_pixels = para->timing.h_active;
	active_lines = para->timing.v_active;
	dp_color_depth = (output_color_format == HDMI_COLORSPACE_YUV422) ?
				COLORDEPTH_24B : color_depth;

	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en)
		dp_color_depth = COLORDEPTH_24B;
	HDMITX_INFO("configure hdmitx21\n");
	hdmitx21_wr_reg(HDMITX_TOP_SW_RESET, 0);
	/* tmds_clk_div40 & scrambler_en already calculate in building format_para */
	if (hdev->tx_comm.pxp_mode)
		hdmitx_set_div40(0);
	else
		hdmitx_set_div40(para->tx_hw_para.hdmitx_hw_para.tmds_clk_div40);
	/*
	 * Glitch-filter HPD and RxSense
	 * [31:28] rxsense_glitch_width: Filter out glitch width <= hpd_glitch_width
	 * [27:16] rxsense_valid_width: Filter out signal stable width <= hpd_valid_width*1024
	 * [15:12] hpd_glitch_width: Filter out glitch width <= hpd_glitch_width
	 * [11: 0] hpd_valid_width: Filter out signal stable width <= hpd_valid_width*1024
	 */
	data32 = 0;
	data32 |= (8 << 28);
	data32 |= (0 << 16);
	data32 |= (7 << 12);
	data32 |= (0 << 0);
	hdmitx21_wr_reg(HDMITX_TOP_HPD_FILTER,    data32);

	/*
	 * config video
	 * [1:0]color depth.
	 *  00:8bpp;
	 *  01:10bpp;
	 *  10:12bpp;
	 *  11:16bpp
	 * [7]  deep color enable bit
	 */
	data8 = 0;
	data8 |= (dp_color_depth & 0x03);
	data8 |= (((dp_color_depth != 4) ? 1 : 0) << 7);
	data8 |= (para->tx_hw_para.hdmitx_hw_para.frl_rate ? 1 : 0) << 3;
	data8 |= (para->tx_hw_para.hdmitx_hw_para.frl_rate ? 1 : 0) << 4;
	hdmitx21_wr_reg(P2T_CTRL_IVCTX, data8);
	data32 = 0;
	/* [  5] reg_hdmi2_on */
	data32 |= (1 << 5);
	data32 |= (para->tx_hw_para.hdmitx_hw_para.scrambler_en & 0x01 << 0);
	hdmitx21_wr_reg(SCRCTL_IVCTX, data32 & 0xff);
	hdmitx21_set_reg_bits(FRL_LINK_RATE_CONFIG_IVCTX,
		para->tx_hw_para.hdmitx_hw_para.frl_rate, 0, 4);

	hdmitx21_wr_reg(SW_RST_IVCTX, 0);
	hdmitx21_wr_reg(CLK_DIV_CNTRL_IVCTX,
		para->tx_hw_para.hdmitx_hw_para.frl_rate ? 0 : 1);
	hdmitx21_wr_reg(CLKPWD_IVCTX, 0xf4);
	hdmitx21_wr_reg(SOC_FUNC_SEL_IVCTX, 0x01);
	/* [1] enable hdmi */
	hdmitx21_wr_reg(TEST_TXCTRL_IVCTX, 0x02);
	hdmitx21_wr_reg(CLKRATIO_IVCTX, 0x8a);

	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_A9) {
		/* reg_null_pkt_en,Null packet Enable. */
		hdmitx21_set_reg_bits(P2T_CTRL_IVCTX, 1, 6, 1);
		/* send packet as fixed slot */
		hdmitx21_set_reg_bits(TMDS_CTRL4_IVCTX, 1, 1, 1);
		HDMITX_INFO("send packet as fixed slot\n");
	} else {
		hdmitx_enable_null_pkt(hdev);
	}

	//---------------
	//config vp core
	// For S1A and later chips, delete the vp core hardware and move the yuv to rgb
	// function to vpu_hdmi_if block
	//---------------
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5 ||
		hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_T7) {
		csc_en = (input_color_format != output_color_format ||
			  input_color_range  != output_color_range) ? 1 : 0;
		//some common register are configuration here TODO why config this value
		hdmitx21_wr_reg(VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX, 0x00);
		hdmitx21_wr_reg((VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
		hdmitx21_wr_reg(VP_CMS_CSC1_MULTI_CSC_CONFIG_IVCTX, 0x00);
		hdmitx21_wr_reg((VP_CMS_CSC1_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
		if (output_color_format == HDMI_COLORSPACE_RGB)
			hdmitx_sync_input_vpp_info(&hdev->tx_comm);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB), 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= ((6 - color_depth) << 4);
		data8 |= ((6 - color_depth) << 2);
		data8 |= ((6 - color_depth) << 0);
		hdmitx21_wr_reg(VP_INPUT_MASK_IVCTX, data8);
	}
	// [11: 9] select_cr: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//                    4={cr[3:0],y[3:0],cb[3:0]};
	//                    5={y[3:0],cr[3:0],cb[3:0]};
	//                    6={cb[3:0],y[3:0],cr[3:0]};
	//                    7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 8: 6] select_cb: 0=y; 1=cb; 2=Cr; 3={cr[11:4],cb[7:4]};
	//                    4={cr[3:0],y[3:0],cb[3:0]};
	//                    5={y[3:0],cr[3:0],cb[3:0]};
	//                    6={cb[3:0],y[3:0],cr[3:0]};
	//                    7={y[3:0],cb[3:0],cr[3:0]}.
	// [ 5: 3] select_y : 0=y; 1=cb; 2=Cr; 3={y[11:4],cb[7:4]};
	//                    4={cb[3:0],cr[3:0],y[3:0]};
	//                    5={cr[3:0],cb[3:0],y[3:0]};
	//                    6={y[3:0],cb[3:0],cr[3:0]};
	//                    7={y[3:0],cr[3:0],cb[3:0]}.
	// [    2] reverse_cr
	// [    1] reverse_cb
	// [ 0] reverse_y
	//Output 422
	if (para->cs == HDMI_COLORSPACE_YUV422) {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (4 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (2 << 4);
		data8 |= (2 << 2);
		data8 |= (2 << 0);
	} else {
		data32 = 0;
		data32 |= (2 << 9);
		data32 |= (1 << 6);
		data32 |= (0 << 3);
		data32 |= (0 << 2);
		data32 |= (0 << 1);
		data32 |= (0 << 0);

		// [5:4] disable_lsbs_cr / [3:2] disable_lsbs_cb / [1:0] disable_lsbs_y
		// 0=12bit; 1=10bit(disable 2-LSB),
		// 2=8bit(disable 4-LSB), 3=6bit(disable 6-LSB)
		data8 = 0;
		data8 |= (0 << 4);
	}
	//mapping for yuv422 12bit
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX, data32 & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MAPPING_IVCTX + 1, (data32 >> 8) & 0xff);
	hdmitx21_wr_reg(VP_OUTPUT_MASK_IVCTX, data8);

	//---------------
	// config Packet
	//---------------
	hdmitx21_wr_reg(VTEM_CTRL_IVCTX, 0x04); //[2] reg_vtem_ctrl
	hdmitx21_wr_reg(GEN5_CTRL_IVCTX, 0x04); //[7:2] reg_gen5_ctrl

	//drm,emp packet
	hdmitx21_wr_reg(HDMITX_TOP_HS_INTR_CNTL, 0x010); //set TX hs_int h_cnt
	//tx_program_drm_emp(int_ext,active_lines,blank_lines,128);

	//--------------------------------------------------------------------------
	// Configure HDCP
	//--------------------------------------------------------------------------

	data32 = 0;
	data32 |= (0xef << 22); // [29:20] channel2 override
	data32 |= (0xcd << 12); // [19:10] channel1 override
	data32 |= (0xab << 2);  // [ 9: 0] channel0 override
	hdmitx21_wr_reg(HDMITX_TOP_SEC_VIDEO_OVR, data32);

	hdmitx21_wr_reg(HDMITX_TOP_HDCP14_MIN_SIZE, 0);
	hdmitx21_wr_reg(HDMITX_TOP_HDCP22_MIN_SIZE, 0);

	data32 = 0;
	data32 |= (active_lines << 16); // [30:16] cntl_vactive
	data32 |= (active_pixels << 0);  // [14: 0] cntl_hactive
	hdmitx21_wr_reg(HDMITX_TOP_HV_ACTIVE, data32);
	hdmitx_soft_reset(RESET_18OR10TO20 | RESET_PFIFO);
	if (hdev->tx_comm.base.rxcap.ieeeoui == HDMI_IEEE_OUI)
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 1, 0, 1);
	else
		hdmitx21_set_reg_bits(TPI_SC_IVCTX, 0, 0, 1);
	/*
	 * On Huawei TVs, HDR will cause a flickering screen,
	 * and HDR PKT needs to be moved behind VSYNC on S7 or S7D
	 */
	if (hdmitx_find_hdr_pkt_delay_to_vsync(hdev->tx_comm.base.edid_buf))
		hdmi_move_hdr_pkt(true);
	else
		hdmi_move_hdr_pkt(false);
}

static void hdmitx_csc_config(u8 input_color_format,
			      u8 output_color_format,
			      u8 color_depth)
{
	u8 conv_en;
	u8 csc_scale;
	u8 rgb_ycc_indicator;
	unsigned long csc_coeff_a1, csc_coeff_a2, csc_coeff_a3, csc_coeff_a4;
	unsigned long csc_coeff_b1, csc_coeff_b2, csc_coeff_b3, csc_coeff_b4;
	unsigned long csc_coeff_c1, csc_coeff_c2, csc_coeff_c3, csc_coeff_c4;
	unsigned long data32;

	conv_en = (((input_color_format  == HDMI_COLORSPACE_RGB) ||
		(output_color_format == HDMI_COLORSPACE_RGB)) &&
		(input_color_format  != output_color_format)) ? 1 : 0;

	if (conv_en) {
		if (output_color_format == HDMI_COLORSPACE_RGB) {
			csc_coeff_a1 = 0x2000;
			csc_coeff_a2 = 0x6926;
			csc_coeff_a3 = 0x74fd;
			csc_coeff_a4 = (color_depth == COLORDEPTH_24B) ?
				0x010e :
			(color_depth == COLORDEPTH_30B) ? 0x043b :
			(color_depth == COLORDEPTH_36B) ? 0x10ee :
			(color_depth == COLORDEPTH_48B) ? 0x10ee : 0x010e;
		csc_coeff_b1 = 0x2000;
		csc_coeff_b2 = 0x2cdd;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x7e9a :
			(color_depth == COLORDEPTH_30B) ? 0x7a65 :
			(color_depth == COLORDEPTH_36B) ? 0x6992 :
			(color_depth == COLORDEPTH_48B) ? 0x6992 : 0x7e9a;
		csc_coeff_c1 = 0x2000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x38b4;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x7e3b :
			(color_depth == COLORDEPTH_30B) ? 0x78ea :
			(color_depth == COLORDEPTH_36B) ? 0x63a6 :
			(color_depth == COLORDEPTH_48B) ? 0x63a6 : 0x7e3b;
		csc_scale = 1;
	} else {
		/* input_color_format == HDMI_COLORSPACE_RGB */
		csc_coeff_a1 = 0x2591;
		csc_coeff_a2 = 0x1322;
		csc_coeff_a3 = 0x074b;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x6535;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x7acc;
		csc_coeff_b4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_coeff_c1 = 0x6acd;
		csc_coeff_c2 = 0x7534;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = (color_depth == COLORDEPTH_24B) ? 0x0200 :
			(color_depth == COLORDEPTH_30B) ? 0x0800 :
			(color_depth == COLORDEPTH_36B) ? 0x2000 :
			(color_depth == COLORDEPTH_48B) ? 0x2000 : 0x0200;
		csc_scale = 0;
	}
	} else {
		csc_coeff_a1 = 0x2000;
		csc_coeff_a2 = 0x0000;
		csc_coeff_a3 = 0x0000;
		csc_coeff_a4 = 0x0000;
		csc_coeff_b1 = 0x0000;
		csc_coeff_b2 = 0x2000;
		csc_coeff_b3 = 0x0000;
		csc_coeff_b4 = 0x0000;
		csc_coeff_c1 = 0x0000;
		csc_coeff_c2 = 0x0000;
		csc_coeff_c3 = 0x2000;
		csc_coeff_c4 = 0x0000;
		csc_scale = 1;
	}

	data32 = 0;
	/* [7:4] csc_color_depth */
	data32 |= (color_depth << 4);
	/* [1:0] cscscale */
	data32 |= (csc_scale << 0);

	/* set csc in video path */
	/* set rgb_ycc indicator */
	switch (output_color_format) {
	case HDMI_COLORSPACE_RGB:
		rgb_ycc_indicator = 0x0;
		break;
	case HDMI_COLORSPACE_YUV422:
		rgb_ycc_indicator = 0x1;
		break;
	case HDMI_COLORSPACE_YUV420:
		rgb_ycc_indicator = 0x3;
		break;
	case HDMI_COLORSPACE_YUV444:
	default:
		rgb_ycc_indicator = 0x2;
		break;
	}
}

static void hdmitx21_vp_cms_csc0_multi_csc_config(u32 data)
{
	if (data == 0) {
		/* disable */
		hdmitx21_wr_reg(VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX, 0);
		hdmitx21_wr_reg((VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX + 1), 0x08);
	} else {
		hdmitx21_wr_reg(VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX, 0x1 | (data & 0xff));
		hdmitx21_wr_reg((VP_CMS_CSC0_MULTI_CSC_CONFIG_IVCTX + 1), 0x08 | (data >> 8));
	}
}

static void hdmitx_set_hw(struct hdmitx21_dev *hdev)
{
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	HDMITX_INFO("config hdmitx IP vic = %d cd: %d cs: %d\n",
		para->timing.vic, para->cd, para->cs);

	config_hdmi21_tx(hdev);
}

/*
 * For the ENCP_VIDEO_MAX_LNCNT, it will always start as 0
 * when set this register, here will minus 1
 * and get the value, here will plus 1
 */
void hdmitx_vrr_set_maxlncnt(u32 max_lcnt)
{
	/* max_lcnt can't be 0 for VRR */
	if (!max_lcnt)
		return;

	hd21_write_reg(ENCP_VIDEO_MAX_LNCNT, max_lcnt - 1);
}

u32 hdmitx_vrr_get_maxlncnt(void)
{
	return hd21_read_reg(ENCP_VIDEO_MAX_LNCNT) + 1;
}

int hdmitx21_read_phy_status(void)
{
	int phy_value = 0;

	phy_value = !!(hd21_read_reg(ANACTRL_HDMIPHY_CTRL0) & 0xffff);

	return phy_value;
}

void hdmitx21_dither_config(struct hdmitx21_dev *hdev)
{
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;
	u32 data32;

	// [    2] inv_hsync_b
	// [    3] inv_vsync_b
	// [    4] hdmi_dith_en_b. For 10-b to 8-b.
	// [    5] hdmi_dith_md_b. For 10-b to 8-b.
	// [ 9: 6] hdmi_dith10_b. For 10-b to 8-b.
	// [   10] hdmi_round_en_b. For 10-b to 8-b.
	// [21:12] hdmi_dith_new_b. For 10-b to 8-b.
	data32 = 0;
	data32 = (0 << 2) |
		(0 << 3) |
		(0 << 4) |
		(0 << 5) |
		(0 << 6) |
		(((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
		(0 << 12);
	if (para->cd == COLORDEPTH_24B && !para->timing.pixel_repetition_factor &&
		!hdmitx_dv_en(&hdev->hw_comm))
		data32 |= (1 << 4);
	hd21_write_reg(VPU_HDMI_DITH_CNTL, data32);
}

/*
 * for emds pkt cuva receiver mode
 * Step 1: hdmitx_cuva_dhdr_init
 * When setting the mode, write all zeros to the CUVA EMP hardware buffer
 */
void hdmitx_cuva_dhdr_init(struct hdmitx_common *tx_comm)
{
	int i;
	struct meson_tx_hdr *tx_hdr = NULL;

	if (!tx_comm)
		return;

	tx_hdr = tx_comm->hdr_state;
	/* in cuva receiver mode, set to 1 when cuva signal needs to be output */
	tx_hdr->cuva_dhdr_reset = false;

	switch (tx_comm->tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S5:
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		/* when booting, write all zero data to cuva dhdr buffer, just once */
		pr_info("cuva d_hdr_init\n");
		/* [0]reg_pkt_gen; [1]reg_pkt_gen_en */
		hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x1);
		/* step3: prepare packet data */
		/* payload[15:8] pb5 msb */
		hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_1_IVCTX, 0);
		/* payload[7:0] pb6 lsb */
		hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_0_IVCTX, 84 & 0xff);

		/* write zero data */
		for (i = 0; i < 84; i++)
			hdmitx21_wr_reg(D_HDR_MEM_WDATA_IVCTX, 0);

		/* reset d_hdr */
		hdmitx21_set_reg_bits(D_HDR_GEN_CTL_IVCTX, 1, 3, 1);
		hdmitx21_set_reg_bits(D_HDR_GEN_CTL_IVCTX, 0, 3, 1);

		/* [0]reg_pkt_gen; [1]reg_pkt_gen_en */
		hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x0);
		break;
	default:
		break;
	}
}

/*
 * for emds pkt cuva receiver mode
 * Step 2: hdmitx_cuva_dhdr_reset
 * Ensure that data from each frame in the software is updated in the
 * hardware buffer and prevent dirty data
 */
void hdmitx_cuva_dhdr_reset(struct hdmitx_common *tx_comm)
{
	struct meson_tx_hdr *tx_hdr = NULL;

	if (!tx_comm)
		return;

	tx_hdr = tx_comm->hdr_state;

	switch (tx_comm->tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S5:
	case MESON_CPU_ID_S7:
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		/* only in cuva receiver mode, reset d_hdr when cuva signal needs to be output */
		if (tx_hdr->cuva_dhdr_reset) {
			/* reset d_hdr */
			hdmitx21_set_reg_bits(D_HDR_GEN_CTL_IVCTX, 1, 3, 1);
			hdmitx21_set_reg_bits(D_HDR_GEN_CTL_IVCTX, 0, 3, 1);
		}
		break;
	default:
		break;
	}
}

/*
 * for emds pkt cuva receiver mode
 * Step 3: hdmitx_dhdr_send
 * Send the data from the hardware buffer out each frame
 */
void hdmitx_dhdr_send(u8 *body, int size)
{
	u32 data;
	int i;
	int active_lines;
	int blank_lines;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S1A)
		return;
	if (!body) {
		hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0);
		return;
	}

	active_lines = para->timing.v_active;
	blank_lines = para->timing.v_blank;

	//  step1: hdr timing
	hdmitx21_wr_reg(D_HDR_VB_LE_IVCTX, (blank_lines & 0xff)); //reg_vb_le default 0x20
	hdmitx21_wr_reg(D_HDR_SPARE_3_IVCTX, 0x2); //[1] reg_fapa_fsm_proper_move

	//setting send mlds data  payload =0
	hdmitx21_wr_reg(D_HDR_GEN_CTL_IVCTX, 0xb1); //[0] reg_source_en [6:5] fapa ctrl
	//[0] reg_send_end_data_set [3] reg_send_pkt_cont (send data set for each frame if it is 1)
	hdmitx21_wr_reg(D_HDR_INSERT_CTL_1_IVCTX, 0x8);
	data  = 0;
	data |= (0 << 6); //[7 : 6] rsvd
	data |= (0 << 5); //[    5] reg_set_new_in_mlds_pkt
	data |= (0 << 4); //[    4] reg_set_new_in_middle
	data |= (0 << 3); //[    3] reg_set_new_always
	data |= (1 << 2); //[    2] reg_to_err_ctrl
	data |= (0 << 1); //[    1] feg_fapa1_support. =0:FAPA start only after second active line.
	data |= (1 << 0); //[    0] reg_set_end_for_mlds
	hdmitx21_wr_reg(D_HDR_INSERT_CTL_2_IVCTX, data);

	hdmitx21_wr_reg(D_HDR_ACT_DE_LO_IVCTX, (active_lines & 0xff)); //reg_act_de_lsb
	hdmitx21_wr_reg(D_HDR_ACT_DE_HI_IVCTX, (active_lines >> 8)); //reg_act_de_msb

	/* step2: packet header and content */
	/* HB0 header */
	hdmitx21_wr_reg(D_HDR_EM_HB0_IVCTX, *body++);
	/* HB1 and HB2 */
	body += 2;
	/* PB0 [7] new; [6] end */
	hdmitx21_wr_reg(D_HDR_EM_PB0_IVCTX, *body++);
	/* PB1 */
	body++;
	/* PB2 ID */
	hdmitx21_wr_reg(D_HDR_EM_PB2_IVCTX, *body++);
	hdmitx21_wr_reg(D_HDR_EM_PB3_IVCTX, *body++);
	hdmitx21_wr_reg(D_HDR_EM_PB4_IVCTX, *body++);
	/* set length 0x38 */
	/* payload[15:8] pb5 msb */
	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_1_IVCTX, *body++);
	/* payload[7:0] pb6 lsb */
	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_0_IVCTX, *body++);

	/*
	 * If set to 1'b1; d_hdr memory address is reset. Before setting of reg_pkt_gen;
	 * need to toggle from 0->1 and 1->0
	 */
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x0);
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x1);
	hdmitx21_wr_reg(D_HDR_MEM_WADDR_RST_IVCTX, 0x0);
	/* [0]reg_pkt_gen; [1]reg_pkt_gen_en */
	hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x1);

	for (i = 0; i < size; i++) {
		data = *body++;
		/*
		 * 1. no_used of vs_emds[0] and vs_emds[1] do not need to be written to
		 * the hardware reg
		 * 2. HB0, HB1, and HB2 of vs_emds[1] and vs_emds[2] are written by hardware and
		 * do not require software operation
		 */
		if ((i <= CUVA_EMP_1_START && i >= CUVA_EMP_0_END) ||
				(i <= CUVA_EMP_2_START && i >= CUVA_EMP_1_END))
			continue;
		hdmitx21_wr_reg(D_HDR_MEM_WDATA_IVCTX, data);
	}
	hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 0x3);
}

static void pkt_send_position_change(u32 enable_mask, u8 mov_val)
{
	if (enable_mask & 0x1) {
		HDMITX_DEBUG("enable to change AVI packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xfe);
		hdmitx21_wr_reg(PKT_LOC_AVI_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change AVI packet send position end\n");
	}
	if (enable_mask & 0x2) {
		HDMITX_DEBUG("enable to change GAMUT packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xfd);
		hdmitx21_wr_reg(PKT_LOC_GAMUT_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GAMUT packet send position end\n");
	}
	if (enable_mask & 0x4) {
		HDMITX_DEBUG("enable to change AUD packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xfb);
		hdmitx21_wr_reg(PKT_LOC_AUD_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change AUD packet send position end\n");
	}
	if (enable_mask & 0x8) {
		HDMITX_DEBUG("enable to change SPD packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xf7);
		hdmitx21_wr_reg(PKT_LOC_SPD_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change SPD packet send position end\n");
	}
	if (enable_mask & 0x10) {
		HDMITX_DEBUG("enable to change MPEG packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xef);
		hdmitx21_wr_reg(PKT_LOC_MPEG_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change MPEG packet send position end\n");
	}
	if (enable_mask & 0x20) {
		HDMITX_DEBUG("enable to change VSIF packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xdf);
		hdmitx21_wr_reg(PKT_LOC_VSIF_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change VSIF packet send position end\n");
	}
	if (enable_mask & 0x40) {
		HDMITX_DEBUG("enable to change GEN packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xbf);
		hdmitx21_wr_reg(PKT_LOC_GEN_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GEN packet send position end\n");
	}
	if (enable_mask & 0x80) {
		HDMITX_DEBUG("enable to change GEN2 packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_0_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0x7f);
		hdmitx21_wr_reg(PKT_LOC_GEN2_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GEN2 packet send position end\n");
	}
	if (enable_mask & 0x100) {
		HDMITX_DEBUG("enable to change GEN3 packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_1_IVCTX, hdmitx21_rd_reg(PKT_AUTO_0_IVCTX) & 0xe);
		hdmitx21_wr_reg(PKT_LOC_GEN3_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GEN3 packet send position end\n");
	}
	if (enable_mask & 0x200) {
		HDMITX_DEBUG("enable to change GEN4 packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_1_IVCTX, hdmitx21_rd_reg(PKT_AUTO_1_IVCTX) & 0xd);
		hdmitx21_wr_reg(PKT_LOC_GEN4_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GEN4 packet send position end\n");
	}
	if (enable_mask & 0x400) {
		HDMITX_DEBUG("enable to change GEN5 packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_1_IVCTX, hdmitx21_rd_reg(PKT_AUTO_1_IVCTX) & 0xb);
		hdmitx21_wr_reg(PKT_LOC_GEN5_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change GEN5 packet send position end\n");
	}
	if (enable_mask & 0x800) {
		HDMITX_DEBUG("enable to change VTEM packet send position begin\n");
		hdmitx21_wr_reg(PKT_AUTO_1_IVCTX, hdmitx21_rd_reg(PKT_AUTO_1_IVCTX) & 0x7);
		hdmitx21_wr_reg(PKT_LOC_VTEM_IVCTX, mov_val);
		HDMITX_DEBUG("enable to change VTEM packet send position end\n");
	}
}

void hdmitx21_write_dhdr_sram(void)
{
	u8 data8;
	int i, h;

	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_1_IVCTX, 0xff); //payload [15:8] ==> pb5 length msb
	hdmitx21_wr_reg(D_HDR_INSERT_PAYLOAD_0_IVCTX, 0xff); //payload [7:0] ==> pb6 length lsb
	hdmitx21_wr_reg(D_HDR_GEN_CTL_IVCTX, 1); //mux src path
	hdmitx21_wr_reg(D_HDR_MEM_READ_EN_IVCTX, 1); //open x_fifo debug path
	HDMITX_DEBUG("write start\n");

	hdmitx21_wr_reg(D_HDR_INSERT_CTRL_IVCTX, 1); //open register write enable
	for (h = 0; h < 64; h++)  {
		for (i = 0; i < 28; i++) {
			data8 = i + h;
			hdmitx21_wr_reg(D_HDR_MEM_WDATA_IVCTX, data8);
		}
	}
	HDMITX_DEBUG("write end\n");
}

void hdmitx21_read_dhdr_sram(void)
{
	u8 rd_data8;
	int i, h;

	HDMITX_DEBUG("read start\n");

	hdmitx21_set_reg_bits(D_HDR_GEN_CTL_IVCTX, 1, 3, 1); //reset
	hdmitx21_wr_reg(D_HDR_GEN_CTL_IVCTX, 1); //mux src path
	hdmitx21_wr_reg(D_HDR_MEM_READ_EN_IVCTX, 1); //open x_fifo debug path
	for (h = 0; h < 64; h++) {
		//read address, open reg_x_fifo_rd, start read xdata; loop for raddr +1
		hdmitx21_rd_reg(D_HDR_MEM_X_FIFO_IVCTX);
		for (i = 0; i < 28; i++) {
			//loop read address, every read for addr + 1
			rd_data8 = hdmitx21_rd_reg(D_HDR_MEM_XDATA_IVCTX);
			HDMITX_DEBUG("data[%d] = 0x%x\n", i, rd_data8);
		}
	}
	hdmitx21_wr_reg(D_HDR_MEM_READ_EN_IVCTX, 0);  //close x_fifo debug path
	HDMITX_DEBUG("read end\n");
}

static int hdmitx_pkt_dump(char *buf, int len)
{
	unsigned int reg_val;
	unsigned int reg_addr;
	unsigned char *conf;
	int ret, i;
	u8 body[32] = {0};
	int pos = 0;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	union hdmi_infoframe *infoframe = NULL;
	struct hdmi_avi_infoframe *avi = NULL;
	struct hdmi_drm_infoframe *drm = NULL;
	struct hdmi_audio_infoframe *audio = NULL;
	unsigned int ieeeoui = 0;

	//GCP PKT
	pos += snprintf(buf + pos, len - pos, "hdmitx gcp reg config\n");
	reg_addr = GCP_CUR_STAT_IVCTX;
	reg_val = hdmitx21_rd_reg(reg_addr);
	pos += snprintf(buf + pos, len - pos, "GCP.clear_avmute: %d\n", (reg_val & 0x2) >> 1);
	pos += snprintf(buf + pos, len - pos, "GCP.set_avmute: %d\n", reg_val & 0x1);
	switch ((reg_val & 0x1C) >> 2) {
	case 4:
		conf = "24bit";
		break;
	case 5:
		conf = "30bit";
		break;
	case 6:
		conf = "36bit";
		break;
	case 7:
		conf = "48bit";
		break;
	default:
		conf = "reserved";
	}
	pos += snprintf(buf + pos, len - pos, "GCP.color_depth: %s\n", conf);
	pos += snprintf(buf + pos, len - pos, "GCP.dc_phase_st: %d\n", (reg_val & 0x60) >> 5);
	pos += snprintf(buf + pos, len - pos, "\n");
	//AVI PKT
	infoframe = &hdev->tx_comm.infoframe.avi;
	avi = &infoframe->avi;
	ret = hdmi_avi_infoframe_get(body);
	if (ret == -1) {
		pos += snprintf(buf + pos, len - pos, "AVI body get error\n");
	} else if (ret == 0) {
		pos += snprintf(buf + pos, len - pos, "AVI PKT not enable\n");
	} else {
		ret = hdmi_avi_infoframe_unpack_renew(avi, body, sizeof(body));
		if (ret < 0) {
			pos += snprintf(buf + pos, len - pos,
				"hdmitx21: parsing avi failed %d\n", ret);
		} else {
			pos += snprintf(buf + pos, len - pos, "AVI.type: 0x%x\n", avi->type);
			pos += snprintf(buf + pos, len - pos, "AVI.version: %d\n", avi->version);
			pos += snprintf(buf + pos, len - pos, "AVI.length: %d\n", avi->length);

			switch (avi->colorspace) {
			case 0:
				conf = "RGB";
				break;
			case 1:
				conf = "422";
				break;
			case 2:
				conf = "444";
				break;
			case 3:
				conf = "420";
				break;
			default:
				conf = "reserved";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.colorspace: %s\n", conf);
			switch (avi->scan_mode) {
			case 0:
				conf = "none";
				break;
			case 1:
				conf = "overscan";
				break;
			case 2:
				conf = "underscan";
				break;
			default:
				conf = "reserved";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.scan: %s\n", conf);
			switch (avi->colorimetry) {
			case 0:
				conf = "none";
				break;
			case 1:
				conf = "BT.601";
				break;
			case 2:
				conf = "BT.709";
				break;
			default:
				conf = "Extended";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.colorimetry: %s\n", conf);
			if (avi->colorimetry == HDMI_COLORIMETRY_EXTENDED) {
				switch (avi->extended_colorimetry) {
				case 0:
					conf = "xvYCC601";
					break;
				case 1:
					conf = "xvYCC709";
					break;
				case 2:
					conf = "sYCC601";
					break;
				case 3:
					conf = "Adobe_YCC601";
					break;
				case 4:
					conf = "Adobe_RGB";
					break;
				case 5:
				case 6:
					conf = "BT.2020";
					break;
				default:
					conf = "xvYCC601";
				}
				pos += snprintf(buf + pos, PAGE_SIZE,
				"AVI.extended_colorimetry: %s\n", conf);
			}
			switch (avi->picture_aspect) {
			case 0:
				conf = "none";
				break;
			case 1:
				conf = "4:3";
				break;
			case 2:
				conf = "16:9";
				break;
			case 3:
				conf = "64:27";
				break;
			case 4:
				conf = "256:135";
				break;
			default:
				conf = "reserved";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.picture_aspect: %s\n", conf);
			switch (avi->active_aspect) {
			case 8:
				conf = "Same as picture_aspect";
				break;
			case 9:
				conf = "4:3";
				break;
			case 10:
				conf = "16:9";
				break;
			case 11:
				conf = "14:9";
				break;
			default:
				conf = "Same as picture_aspect";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.active_aspect: %s\n", conf);
			switch (avi->quantization_range) {
			case 0:
				conf = "default";
				break;
			case 1:
				conf = "limited";
				break;
			case 2:
				conf = "full";
				break;
			default:
				conf = "reserved";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.quantization_range: %s\n", conf);
			if (avi->itc)
				conf = "enable";
			else
				conf = "disable";
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.itc: %s\n", conf);
			switch (avi->nups) {
			case 0:
				conf = "unknown";
				break;
			case 1:
				conf = "horiz";
				break;
			case 2:
				conf = "vert";
				break;
			case 3:
				conf = "horiz and vert";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.nups: %s\n", conf);
			pos += snprintf(buf + pos,
				PAGE_SIZE, "AVI.video_code: %d\n", avi->video_code);
			switch (avi->ycc_quantization_range) {
			case 0:
			default:
				conf = "limited";
				break;
			case 1:
				conf = "full";
			}
			pos += snprintf(buf + pos, PAGE_SIZE,
				"AVI.ycc_quantization_range: %s\n", conf);
			switch (avi->content_type) {
			case 0:
				conf = "graphics";
				break;
			case 1:
				conf = "photo";
				break;
			case 2:
				conf = "cinema";
				break;
			case 3:
				conf = "game";
			}
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.content_type: %s\n", conf);
			pos += snprintf(buf + pos, PAGE_SIZE,
				"AVI.pixel_repetition: %d\n", avi->pixel_repeat);
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.top_bar: %d\n", avi->top_bar);
			pos += snprintf(buf + pos, PAGE_SIZE,
				"AVI.bottom_bar: %d\n", avi->bottom_bar);
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.left_bar: %d\n", avi->left_bar);
			pos += snprintf(buf + pos, PAGE_SIZE, "AVI.right: %d\n", avi->right_bar);
		}
	}
	pos += snprintf(buf + pos, len - pos, "\n");
	//ACR PKT
	pos += snprintf(buf + pos, len - pos, "ACR.N1=0x%x\n", hdmitx21_rd_reg(N_SVAL1_IVCTX));
	pos += snprintf(buf + pos, len - pos, "ACR.N2=0x%x\n", hdmitx21_rd_reg(N_SVAL2_IVCTX));
	pos += snprintf(buf + pos, len - pos, "ACR.N3=0x%x\n", hdmitx21_rd_reg(N_SVAL3_IVCTX));
	pos += snprintf(buf + pos, PAGE_SIZE, "\n");

	//DRM PKT
	infoframe = &hdev->tx_comm.infoframe.drm;
	drm = &infoframe->drm;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_DRM, body);
	if (ret == -1) {
		pos += snprintf(buf + pos, len - pos, "DRM body get error\n");
	} else if (ret == 0) {
		pos += snprintf(buf + pos, len - pos, "DRM PKT not enable\n");
	} else {
		ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
		if (ret < 0) {
			pos += snprintf(buf + pos, len - pos, "parsing DRM failed %d\n", ret);
		} else {
			pos += snprintf(buf + pos, len - pos, "DRM.type: 0x%x\n", drm->type);
			pos += snprintf(buf + pos, len - pos, "DRM.version: %d\n", drm->version);
			pos += snprintf(buf + pos, len - pos, "DRM.length: %d\n", drm->length);

			switch (drm->eotf) {
			case 0:
				conf = "sdr";
				break;
			case 1:
				conf = "hdr";
				break;
			case 2:
				conf = "ST 2084";
				break;
			case 3:
				conf = "HLG";
				break;
			default:
				conf = "sdr";
			}
			pos += snprintf(buf + pos, len - pos, "DRM.eotf: %s\n", conf);

			switch (drm->metadata_type) {
			case 0:
				conf = "static metadata";
				break;
			default:
				conf = "reserved";
			}
			pos += snprintf(buf + pos, len - pos, "DRM.metadata_id: %s\n", conf);
			pos += snprintf(buf + pos, len - pos, "display_primaries:\n");
			for (i = 0; i < 3; i++) {
				pos += snprintf(buf + pos, len - pos, "x:%d, y:%d\n",
					drm->display_primaries[i].x, drm->display_primaries[i].y);
			}
			pos += snprintf(buf + pos, len - pos, "white_point: x:%d, y:%d\n",
				drm->white_point.x, drm->white_point.y);
			pos += snprintf(buf + pos, len - pos,
				"DRM.max_lum : %d\n", drm->max_display_mastering_luminance);
			pos += snprintf(buf + pos, len - pos,
				"DRM.min_lum : %d\n", drm->min_display_mastering_luminance);
			pos += snprintf(buf + pos, len - pos, "DRM.max_cll : %d\n", drm->max_cll);
			pos += snprintf(buf + pos, len - pos, "DRM.max_fall : %d\n", drm->max_fall);
		}
	}
	pos += snprintf(buf + pos, len - pos, "\n");

	/* VSIF PKT */
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR, body);
	if (ret == -1) {
		pos += snprintf(buf + pos, len - pos, "VSIF body get error\n");
	} else if (ret == 0) {
		pos += snprintf(buf + pos, len - pos, "VSIF PKT not enable\n");
	} else {
		ieeeoui = body[6] << 16 | body[5] << 8 | body[4];
		switch (ieeeoui) {
		case HDMI_IEEE_OUI:
			conf = "hdmi_ieee_oui";
			break;
		case HDMI_FORUM_IEEE_OUI:
			conf = "hdmi_forum_ieee_oui";
			break;
		case DOVI_IEEEOUI:
			conf = "amdv_ieee_oui";
			break;
		case CUVA_IEEEOUI:
			conf = "cuva_ieee_oui";
			break;
		case HDR10PLUS_IEEEOUI:
			conf = "hdr10plus_ieee_oui";
			break;
		default:
			conf = "invalid_ieee_oui";
			break;
		}
		pos += snprintf(buf + pos, len - pos, "VSIF.ieee_oui: %s (0x%06x)\n",
				conf, ieeeoui);
		if (ieeeoui == HDMI_IEEE_OUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF.hdmi_vic: %d\n", body[8]);
		} else if (ieeeoui == HDMI_FORUM_IEEE_OUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF.allm: %d\n",
					(body[8] & 0x2) >> 1);
		} else if (ieeeoui == DOVI_IEEEOUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF.amdv_signal: %d\n",
					(body[7] & 0x2) >> 1);
			pos += snprintf(buf + pos, len - pos, "VSIF.amdv_low_latency: %d\n",
					body[7] & 0x1);
		}
	}
	pos += snprintf(buf + pos, len - pos, "\n");

	/* VSIF1 PKT */
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	if (ret == -1) {
		pos += snprintf(buf + pos, len - pos, "VSIF1 body get error\n");
	} else if (ret == 0) {
		pos += snprintf(buf + pos, len - pos, "VSIF1 PKT not enable\n");
	} else {
		ieeeoui = body[6] << 16 | body[5] << 8 | body[4];
		switch (ieeeoui) {
		case HDMI_IEEE_OUI:
			conf = "hdmi_ieee_oui";
			break;
		case HDMI_FORUM_IEEE_OUI:
			conf = "hdmi_forum_ieee_oui";
			break;
		case DOVI_IEEEOUI:
			conf = "amdv_ieee_oui";
			break;
		case CUVA_IEEEOUI:
			conf = "cuva_ieee_oui";
			break;
		case HDR10PLUS_IEEEOUI:
			conf = "hdr10plus_ieee_oui";
			break;
		default:
			conf = "invalid_ieee_oui";
			break;
		}
		pos += snprintf(buf + pos, len - pos, "VSIF1.ieee_oui: %s (0x%06x)\n",
				conf, ieeeoui);
		if (ieeeoui == HDMI_IEEE_OUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF1.hdmi_vic: %d\n", body[8]);
		} else if (ieeeoui == HDMI_FORUM_IEEE_OUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF1.allm: %d\n",
					(body[8] & 0x2) >> 1);
		} else if (ieeeoui == DOVI_IEEEOUI) {
			pos += snprintf(buf + pos, len - pos, "VSIF1.amdv_signal: %d\n",
					(body[7] & 0x2) >> 1);
			pos += snprintf(buf + pos, len - pos, "VSIF1.amdv_low_latency: %d\n",
					body[7] & 0x1);
		}
	}
	pos += snprintf(buf + pos, len - pos, "\n");

	/* AUDIO PKT */
	infoframe = &hdev->tx_comm.infoframe.aud;
	audio = &infoframe->audio;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AUDIO, body);
	if (ret == -1) {
		pos += snprintf(buf + pos, len - pos, "AUDIO body get error\n");
	} else if (ret == 0) {
		pos += snprintf(buf + pos, len - pos, "AUDIO PKT not enable\n");
	} else {
		ret = hdmi_infoframe_unpack(infoframe, body, sizeof(body));
		if (ret < 0) {
			HDMITX_ERROR("parsing AUDIO failed %d\n", ret);
		} else {
			pos += snprintf(buf + pos, len - pos, "AUDI.type: 0x%x\n", audio->type);
			pos += snprintf(buf + pos, len - pos, "AUDI.version: %d\n", audio->version);
			pos += snprintf(buf + pos, len - pos, "AUDI.length: %d\n", audio->length);
			switch (audio->coding_type) {
			case 0:
				conf = "refer to stream header";
				break;
			case 1:
				conf = "L-PCM";
				break;
			case 2:
				conf = "AC-3";
				break;
			case 3:
				conf = "MPEG1";
				break;
			case 4:
				conf = "MP3";
				break;
			case 5:
				conf = "MPEG2";
				break;
			case 6:
				conf = "AAC";
				break;
			case 7:
				conf = "DTS";
				break;
			case 8:
				conf = "ATRAC";
				break;
			case 9:
				conf = "One Bit Audio";
				break;
			case 10:
				conf = "Dolby Digital+";
				break;
			case 11:
				conf = "DTS_HD";
				break;
			case 12:
				conf = "MAT";
				break;
			case 13:
				conf = "DST";
				break;
			case 14:
				conf = "WMA";
				break;
			default:
				conf = "MAX";
			}
			pos += snprintf(buf + pos, len - pos, "AUDI.coding_type: %s\n", conf);
			pos += snprintf(buf + pos, len - pos,
				"AUDI.channel_count: %d\n", audio->channels + 1);
			switch (audio->sample_frequency) {
			case FS_REFER_TO_STREAM:
				conf = "refer to stream header";
				break;
			case FS_32K:
				conf = "32kHz";
				break;
			case FS_44K1:
				conf = "44.1kHz";
				break;
			case FS_48K:
				conf = "48kHz";
				break;
			case FS_88K2:
				conf = "88.2kHz";
				break;
			case FS_96K:
				conf = "96kHz";
				break;
			case FS_176K4:
				conf = "176.4kHz";
				break;
			case FS_192K:
				conf = "192kHz";
			}
			pos += snprintf(buf + pos, len - pos, "AUDI.sample_frequency: %s\n", conf);
			switch (audio->sample_size) {
			case SS_16BITS:
				conf = "16bit";
				break;
			case SS_20BITS:
				conf = "20bit";
				break;
			case SS_24BITS:
				conf = "24bit";
				break;
			case SS_REFER_TO_STREAM:
			default:
				conf = "refer to stream header";
				break;
			}
			pos += snprintf(buf + pos, len - pos, "AUDI.sample_size: %s\n", conf);
			pos += snprintf(buf + pos, len - pos,
				"AUDI.channel_allocation: %d\n", audio->channel_allocation);
			pos += snprintf(buf + pos, len - pos,
				"AUDI.level_shift_value: %d\n", audio->level_shift_value);
			pos += snprintf(buf + pos, len - pos,
				"AUDI.down_mix_enable: %d\n", audio->downmix_inhibit);
		}
	}
	reg_val = hdmitx21_rd_reg(AUD_MODE_IVCTX);  //AUD_MODE
	if (reg_val & BIT(0))
		pos += snprintf(buf + pos, len - pos, "AUDI.mode: spdif\n");
	if (reg_val & BIT(0))
		pos += snprintf(buf + pos, len - pos, "AUDI.mode: hbra\n");
	if (reg_val & BIT(0))
		pos += snprintf(buf + pos, len - pos, "AUDI.mode: dsd\n");
	if (reg_val & 0xf0)
		pos += snprintf(buf + pos, len - pos, "AUDI.mode: i2s\n");
	//AUDIO SAMPLE
	return pos;
}

static void vpu_hdmi_fmt_config(struct hdmitx21_dev *hdev)
{
	u32 data32;
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	// [ 1: 0] hdmi_vid_fmt. 0=444; 1=convert to 422; 2=convert to 420.
	// [ 3: 2] chroma_dnsmp_h. 0=use pixel 0; 1=use pixel 1; 2=use average.
	// [    4] dith_en. 1=enable dithering before HDMI TX input.
	// [    5] hdmi_dith_md: random noise selector.
	// [ 9: 6] hdmi_dith10_cntl.
	// [   10] hdmi_round_en. 1= enable 12-b rounded to 10-b.
	// [   11] tunnel_en
	// [21:12] hdmi_dith_new
	// [23:22] chroma_dnsmp_v. 0=use line 0; 1=use line 1; 2=use average.
	// [27:24] pix_repeat
	data32 = 0;
	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S1A:
		//bit[1,0] = 3 enable ycbcr2rgb
		data32 = (para->cs == HDMI_COLORSPACE_RGB) ?
			  3 : ((para->cs == HDMI_COLORSPACE_YUV422) ? 1 : 0) |
			  (2 << 2) |
			  (0 << 4) |
			  (0 << 5) |
			  (0 << 6) |
			  (((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
			  (0 << 11) |
			  (0 << 12) |
			  (2 << 22) |
			  (0 << 24);
		break;
	case MESON_CPU_ID_S7:
		//bit[1,0] = 3 enable ycbcr2rgb
		data32 = (((para->cs == HDMI_COLORSPACE_RGB) ? 3 :
			(para->cs == HDMI_COLORSPACE_YUV420) ? 2 :
			(para->cs == HDMI_COLORSPACE_YUV422) ? 1 : 0) << 0) |
			  (2 << 2) |
			  (0 << 4) |
			  (0 << 5) |
			  (0 << 6) |
			  (((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
			  (0 << 11) |
			  (0 << 12) |
			  (2 << 22) |
			  (0 << 24);
		break;
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		// [   31] for s7d: cntl_hdmi_matrix_en. 1=enable(yuv2rgb or rgb2yuv);
		// VPU_HDMI_FMT_CTRL in every chips need check
		//bit[1,0] = 3 enable ycbcr2rgb
		data32 = (((para->cs == HDMI_COLORSPACE_YUV420) ? 2 :
			(para->cs == HDMI_COLORSPACE_YUV422) ? 1 : 0) << 0) |
			  (2 << 2) |
			  (0 << 4) |
			  (0 << 5) |
			  (0 << 6) |
			  (((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
			  (0 << 11) |
			  (0 << 12) |
			  (2 << 22) |
			  (0 << 24) |
			  (((para->cs == HDMI_COLORSPACE_RGB) ? 1 : 0) << 31);
		break;
	case MESON_CPU_ID_T7:
	case MESON_CPU_ID_S5:
	default:
		data32 = (((para->cs == HDMI_COLORSPACE_YUV420) ? 2 :
			  (para->cs == HDMI_COLORSPACE_YUV422) ? 1 : 0) << 0) |
			  (2 << 2) |
			  (0 << 4) |
			  (0 << 5) |
			  (0 << 6) |
			  (((para->cd == COLORDEPTH_24B) ? 1 : 0) << 10) |
			  (0 << 11) |
			  (0 << 12) |
			  (2 << 22) |
			  (0 << 24);
		if (para->tx_hw_para.hdmitx_hw_para.frl_rate &&
			para->cs == HDMI_COLORSPACE_YUV420)
			data32 |= 3 << 0; // 3:420 dual port
		break;
	}
	hd21_write_reg(VPU_HDMI_FMT_CTRL, data32);
}

static void vpu_hdmi_setting_config(struct hdmitx21_dev *hdev)
{
	u32 data32;
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	// [    0] src_sel_enci
	// [    1] src_sel_encp
	// [    2] inv_hsync. 1=Invert Hsync polarity.
	// [    3] inv_vsync. 1=Invert Vsync polarity.
	// [    4] inv_dvi_clk. 1=Invert clock to external DVI,
	//         (clock inversion exists at internal HDMI).
	// YUV420. Output Y1Y0C to hdmitx.
	// YUV444/422/RGB. Output CrYCb, CY0, or RGB to hdmitx.
	// [ 7: 5] comp_map_post. Data from vfmt is CrYCb(444), CY0(422), CY0Y1(420) or RGB,
	//         map the data to desired format before go to hdmitx:
	// 0=output {2, 1,0};
	// 1=output {1,0,2};
	// 2=output {1,2,0};
	// 3=output {0,2,1};
	// 4=output {0, 1,2};
	// 5=output {2.0,1};
	// 6,7=Rsrv.
	// [11: 8] wr_rate_pre. 0=A write every clk1; 1=A write every 2 clk1; ...;
	//                      15=A write every 16 clk1.
	// [15:12] rd_rate_pre. 0=A read every clk2; 1=A read every 2 clk2; ...;
	//                      15=A read every 16 clk2.
	// RGB. Output RGB to vfmt.
	// YUV. Output CrYCb to vfmt.
	// [18:16] comp_map_pre. Data from VENC is YCbCr or RGB, map data to desired
	//                       format before go to vfmt:
	// 0=output YCbCr(RGB);
	// 1=output CbCrY(GBR);
	// 2=output CbYCr(GRB);
	// 3=output CrYCb(BRG);
	// 4=output CrCbY(BGR);
	// 5=output YCrCb(RBG);
	// 6,7=Rsrv.
	// [23:20] wr_rate_post. 0=A write every clk1; 1=A write every 2 clk1; ...;
	//                       15=A write every 16 clk1.
	// [27:24] rd_rate_post. 0=A read every clk2; 1=A read every 2 clk2; ...;
	//                       15=A read every 16 clk2.
	data32 = 0;
	switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
	case MESON_CPU_ID_S1A:
	case MESON_CPU_ID_S7:
		if (para->timing.pi_mode == 0 &&
			(para->timing.v_active == 480 || para->timing.v_active == 576))
			data32 |= 1;
		else
			data32 |= 2;
		data32 |= (para->timing.h_pol << 2);
		data32 |= (para->timing.v_pol << 3);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 4 : 0) << 5);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 1 : 0) << 20);
		break;
	case MESON_CPU_ID_S7D:
	case MESON_CPU_ID_S6:
		if (para->timing.pi_mode == 0 &&
			(para->timing.v_active == 480 || para->timing.v_active == 576))
			data32 |= 1;
		else
			data32 |= 2;
		data32 |= (para->timing.h_pol << 2);
		data32 |= (para->timing.v_pol << 3);
		//bit[5]  = 0 output CrYCb(BRG)
		//bit[16] = 1 comp_map_pre
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 4 : 0) << 5);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 1 : 0) << 20);
		break;
	case MESON_CPU_ID_S5:
		data32 |= (0 << 0);
		data32 |= (((para->cs != HDMI_COLORSPACE_YUV420 &&
					para->tx_hw_para.hdmitx_hw_para.frl_rate) ? 1 : 0) << 1);
		data32 |= (para->timing.h_pol << 2);
		data32 |= (para->timing.v_pol << 3);
		data32 |= (0 << 4);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 4 : 0) << 5);
		if (!para->tx_hw_para.hdmitx_hw_para.frl_rate && para->cs == HDMI_COLORSPACE_YUV420)
			data32 |= (1 << 8);
		data32 |= (0 << 12);
		data32 |= (0 << 16);
		data32 |= (0 << 20);
		data32 |= (0 << 24);
		if (para->tx_hw_para.hdmitx_hw_para.frl_rate)
			data32 |= ((para->cs == HDMI_COLORSPACE_YUV420 ? 2 : 1) << 28);
		break;
	case MESON_CPU_ID_T7:
		if (hdev->tx_comm.enc_idx == 0)
			data32 |= (1 << 0);
		else
			data32 |= (1 << 1);
		data32 |= (para->timing.h_pol << 2);
		data32 |= (para->timing.v_pol << 3);
		data32 |= (0 << 4);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 4 : 0) << 5);
		data32 |= (0 << 8);
		data32 |= (0 << 12);
		data32 |= (((TX_INPUT_COLOR_FORMAT == HDMI_COLORSPACE_RGB) ? 0 : 3) << 16);
		data32 |= (((para->cs == HDMI_COLORSPACE_YUV420) ? 1 : 0) << 20);
		data32 |= (0 << 24);
		data32 |= (0 << 28);
		break;
	default:
		break;
	}
	hd21_write_reg(VPU_HDMI_SETTING, data32);

	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
		/* for dsc y420/y444, no need comp_map_post */
		hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 5, 3);
		if (para->cs == HDMI_COLORSPACE_YUV422)
			hd21_set_reg_bits(VPU_HDMI_SETTING, 5, 5, 3);
		/* no up_sample and data split */
		hd21_set_reg_bits(ENCP_VIDEO_MODE_ADV, 0, 0, 3);
		hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 1, 1);
		/* 4ppc to dsc module */
		hd21_set_reg_bits(VPU_HDMI_SETTING, 2, 28, 2);
	}

	/* recommend flow: dsc mux->dsc configure/dsc_enc_en/dsc_tmg_en->venc_enable */
	if (hdev->tx_comm.tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
		hd21_set_reg_bits(VPU_HDMI_SETTING,
			!!para->tx_hw_para.hdmitx_hw_para.dsc_en, 31, 1);
}

void hdmitx21_color_convert(struct hdmitx21_dev *hdev, u32 output_color_format)
{
	u32 data32 = 0;
	struct meson_tx_format_para *para = &hdev->tx_comm.fmt_para;

	/* Y422,12bit to Y444,8bit */
	if ((output_color_format & 0xF) == CSC_Y444_8BIT) {
		/* 1.VPU_HDMI_FMT*/
		data32 = 0;
		data32 = (2 << 2) |
			(1 << 10) |
			(2 << 22);
		hd21_write_reg(VPU_HDMI_FMT_CTRL, data32);
		/* 2.VPU_HDMI_DITH */
		data32 = 0;
		data32 = (1 << 10);
		if (hdmitx_dv_en(&hdev->hw_comm) == 0 &&
			!para->timing.pixel_repetition_factor)
			data32 |= (1 << 4);
		hd21_write_reg(VPU_HDMI_DITH_CNTL, data32);
		/* 3.vp config */
		hdmitx21_vp_conf(COLORDEPTH_24B, HDMI_COLORSPACE_YUV444);
		HDMITX_DEBUG("csc to Y444_8BIT\n");
	} else if ((output_color_format & 0xF) == CSC_RGB_8BIT) {
		/* Y422,12bit to RGB,8bit */
		/* 1.VPU_HDMI_FMT*/
		data32 = 0;
		switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
		case MESON_CPU_ID_S1A:
		case MESON_CPU_ID_S7:
			data32 = (3 << 0) |
			(2 << 2) |
			(1 << 10) |
			(2 << 22);
			break;
		case MESON_CPU_ID_S7D:
		case MESON_CPU_ID_S6:
			data32 = (2 << 2) |
				(1 << 10) |
				(2 << 22) |
				(1 << 31);
			break;
		case MESON_CPU_ID_T7:
		case MESON_CPU_ID_S5:
			data32 = (2 << 2) |
				(1 << 10) |
				(2 << 22);
			break;
		default:
			break;
		}
		hd21_write_reg(VPU_HDMI_FMT_CTRL, data32);
		/* 2.VPU_HDMI_DITH */
		data32 = 0;
		data32 = (1 << 10);
		if (hdmitx_dv_en(&hdev->hw_comm) == 0 &&
			!para->timing.pixel_repetition_factor)
			data32 |= (1 << 4);
		hd21_write_reg(VPU_HDMI_DITH_CNTL, data32);
		/* 3.vp config/ yuv->rgb*/
		switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
		case MESON_CPU_ID_S1A:
		case MESON_CPU_ID_S7:
		case MESON_CPU_ID_S7D:
		case MESON_CPU_ID_S6:
			hdmitx21_vp_conf(COLORDEPTH_24B, HDMI_COLORSPACE_RGB);
			vpu_hdmi_set_matrix_ycbcr2rgb();
			break;
		case MESON_CPU_ID_T7:
		case MESON_CPU_ID_S5:
			hdmitx21_vp_conf(COLORDEPTH_24B, HDMI_COLORSPACE_RGB);
			break;
		default:
			break;
		}
		HDMITX_DEBUG("csc to RGB_8BIT\n");
	} else if ((output_color_format & 0xF) == CSC_Y422_12BIT) {
		/* RGB/Y444,8bit to Y422,12bit */
		/* 1.VPU_HDMI_FMT*/
		data32 = 0;
		data32 = (1 << 0) |
			(2 << 2) |
			(0 << 10) |
			(2 << 22);
		hd21_write_reg(VPU_HDMI_FMT_CTRL, data32);
		/* 2.VPU_HDMI_DITH */
		data32 = 0;
		hd21_write_reg(VPU_HDMI_DITH_CNTL, data32);
		/* 3.vp config */
		hdmitx21_vp_conf(COLORDEPTH_36B, HDMI_COLORSPACE_YUV422);
		/* 4. VPU_HDMI_SETTING */
		switch (hdev->tx_comm.tx_hw->chip_data->chip_type) {
		case MESON_CPU_ID_S1A:
		case MESON_CPU_ID_S7:
			if (para->cs == HDMI_COLORSPACE_RGB)
				hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 5, 1);
			break;
		case MESON_CPU_ID_S7D:
		case MESON_CPU_ID_S6:
			if (para->cs == HDMI_COLORSPACE_RGB)
				hd21_set_reg_bits(VPU_HDMI_SETTING, 0, 16, 1);
			break;
		default:
			break;
		}
		/* 4.avi cs */
		if (output_color_format & CSC_UPDATE_AVI_CS)
			hdmi_avi_infoframe_config(&hdev->tx_comm,
				AUX_PKT_SET_AVI_CS, HDMI_COLORSPACE_YUV422);
		HDMITX_DEBUG("csc to Y444_12BIT\n");
	} else {
		HDMITX_DEBUG("csc not support/implemented yet\n");
	}
}

void hdmitx21_csc_update_avi_infoframe(struct hdmitx21_dev *hdev, u32 output_color_format)
{
	/* Y422,12bit to Y444,8bit */
	if ((output_color_format & 0xF) == CSC_Y444_8BIT) {
		/* 4.avi cs */
		if (output_color_format & CSC_UPDATE_AVI_CS)
			hdmi_avi_infoframe_config(&hdev->tx_comm,
				AUX_PKT_SET_AVI_CS, HDMI_COLORSPACE_YUV444);
	} else if ((output_color_format & 0xF) == CSC_RGB_8BIT) {
		/* Y422,12bit to RGB,8bit */
		/* 5.avi cs */
		if (output_color_format & CSC_UPDATE_AVI_CS)
			hdmi_avi_infoframe_config(&hdev->tx_comm, AUX_PKT_SET_AVI_CS,
				HDMI_COLORSPACE_RGB);
	} else if ((output_color_format & 0xF) == CSC_Y422_12BIT) {
		/* RGB/Y444,8bit to Y422,12bit */
		/* 4.avi cs */
		if (output_color_format & CSC_UPDATE_AVI_CS)
			hdmi_avi_infoframe_config(&hdev->tx_comm,
				AUX_PKT_SET_AVI_CS, HDMI_COLORSPACE_YUV422);
	} else {
		HDMITX_DEBUG("avi not support/implemented yet\n");
	}
}

void hdmitx_set_frl_rate_none(struct hdmitx_common *tx_comm)
{
	u8 data;
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;

	/* such as T7 unsupport FRL, skip frl flow */
	if (hw_comm->hdmi_tx_cap.tx_max_frl_rate == FRL_NONE)
		return;

	if (tx_comm->base.rxcap.max_frl_rate > FRL_NONE &&
		tx_comm->base.rxcap.scdc_present == 1 &&
		hdmitx21_hw_cntl(hw_comm, FRL_GET_MODE, NULL, NULL) > FRL_NONE) {
		scdc_tx_frl_cfg1_set(0);
		data = scdc_tx_update_flags_get();
		if (data & FLT_UPDATE)
			scdc_tx_update_flags_set(FLT_UPDATE);
		hdmitx21_hw_cntl(hw_comm, FRL_CLR_MODE, NULL, NULL);
	}
}

struct hdmitx21_hw *get_hdmitx21_hw_instance(void)
{
	struct hdmitx21_dev *hdev = get_hdmitx21_device();

	return &hdev->tx21_hw;
}

struct hdmitx_common *hdmitx21_alloc_instance(struct device *device)
{
	struct hdmitx21_dev *h21_dev;

	h21_dev = devm_kzalloc(device, sizeof(*h21_dev), GFP_KERNEL);
	if (!h21_dev)
		return NULL;
	h21_dev->tx_comm.tx_hw = &h21_dev->hw_comm;
	h21_dev->tx_comm.hdmi_init = HDMITX21;
	tx21_dev = h21_dev;
	return &h21_dev->tx_comm;
}

struct hdmitx21_dev *get_hdmitx21_device(void)
{
	return tx21_dev;
}

void hdmitx21_sw_debug_func(struct hdmitx_common *tx_comm, const char *buf)
{
	char tmpbuf[128];
	int i = 0;
	int ret;
	unsigned long value = 0;
	struct hdmitx21_dev *hdev = container_of(tx_comm, struct hdmitx21_dev, tx_comm);
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;

	while ((buf[i]) && (buf[i] != ',') && (buf[i] != ' ')) {
		tmpbuf[i] = buf[i];
		i++;
	}
	tmpbuf[i] = 0;

	if (strncmp(tmpbuf, "mask_qms", 8) == 0) {
		if (tmpbuf[8] == '1')
			tx_comm->edid_mask_qms = 1;
		if (tmpbuf[8] == '0')
			tx_comm->edid_mask_qms = 0;
		HDMITX_INFO("mask edid qms %d\n", tx_comm->edid_mask_qms);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (strncmp(tmpbuf, "frl", 3) == 0) {
		if (strncmp(tmpbuf + 3, "stop", 4) == 0) {
			frl_tx_stop();
			return;
		}
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate = tmpbuf[3] - '0';
		frl_tx_training_handler(hdev);
		return;
#endif
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	} else if (strncmp(tmpbuf, "hdcp_", 5) == 0) {
		if (hdcptx_comm->hdcptx_api->hdcptx_debug)
			hdcptx_comm->hdcptx_api->hdcptx_debug(tx_comm->hdcptx_priv, tmpbuf + 5);
		else
			HDMITX_INFO("no hdcp debug function\n");
#endif
	} else if (strncmp(tmpbuf, "aud_mute", 8) == 0) {
		ret = kstrtoul(tmpbuf + 8, 10, &value);
		hdmitx_ext_set_audio_output(value);
		HDMITX_INFO("aud_mute :%lu\n", value);
	} else if (strncmp(tmpbuf, "get_output_mute", 15) == 0) {
		HDMITX_INFO("VPP output mute :%d\n", get_output_mute());
	} else if (strncmp(tmpbuf, "set_output_mute", 15) == 0) {
		ret = kstrtoul(tmpbuf + 15, 10, &value);
		set_output_mute(!!value);
		HDMITX_INFO("set VPP output mute :%d\n", !!value);
	}  else if (strncmp(tmpbuf, "vrr_mode", 8) == 0) {
		switch (hdev->vrr_mode) {
		case T_VRR_GAME:
			HDMITX_INFO("%s\n", "game-vrr");
			break;
		case T_VRR_QMS:
			HDMITX_INFO("%s\n", "qms-vrr");
			break;
		default:
			HDMITX_INFO("%s\n", "none");
			break;
		}
	} else if (strncmp(tmpbuf, "frl_rate", 8) == 0) {
		/* forced FRL rate setting */
		if (strncmp(&tmpbuf[8], "_w", 2) == 0) {
			ret = kstrtoul(tmpbuf + 10, 10, &value);
			if (!hdev->tx_comm.base.rxcap.max_frl_rate)
				HDMITX_INFO("rx not support FRL\n");
			if (value > FRL_12G4L) {
				HDMITX_ERROR("set frl_rate in 0 ~ 6\n");
				return;
			}
			hdev->manual_frl_rate = value;
			HDMITX_INFO("set tx frl_rate as %d\n", value);
			if (hdev->manual_frl_rate > hdev->tx_comm.base.rxcap.max_frl_rate)
				HDMITX_INFO("larger than rx max_frl_rate %d\n",
					hdev->tx_comm.base.rxcap.max_frl_rate);
		} else if (strncmp(&tmpbuf[8], "_r", 2) == 0) {
			HDMITX_INFO("%d\n",
				hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate);
			switch (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate) {
			case FRL_3G3L:
				HDMITX_INFO("FRL_3G3L\n");
				break;
			case FRL_6G3L:
				HDMITX_INFO("FRL_6G3L\n");
				break;
			case FRL_6G4L:
				HDMITX_INFO("FRL_6G4L\n");
				break;
			case FRL_8G4L:
				HDMITX_INFO("FRL_8G4L\n");
				break;
			case FRL_10G4L:
				HDMITX_INFO("FRL_10G4L\n");
				break;
			case FRL_12G4L:
				HDMITX_INFO("FRL_12G4L\n");
				break;
			default:
				break;
			}
		}
	} else if (strncmp(tmpbuf, "dsc_en", 6) == 0) {
		HDMITX_INFO("dsc enable: %d\n",
			hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en);
	} else if (strncmp(tmpbuf, "dsc_policy", 10) == 0) {
		/*
		 * dsc_policy:
		 * 0 automatically enable dsc if necessary, but not support 8k Y444/rgb,12bit
		 * or 4k100/120hz Y444/rgb,12bit
		 * 1 force enable dsc for mode that can be supported both with dsc/non-dsc
		 * 2 force enable dsc for mode test for new dsc mode(debug only)
		 * 3 forcely filter out dsc mode output by valid_mode_check
		 * 4 automatically enable dsc if necessary, include those mentioned in 0
		 */
		if (strncmp(&tmpbuf[10], "_w", 2) == 0) {
			ret = kstrtoul(tmpbuf + 12, 10, &value);
			if (value != 0 && value != 1 && value != 2 && value != 3 && value != 4) {
				HDMITX_INFO("set dsc_policy in 0~4\n");
				return;
			}
			hdev->hw_comm.hdmi_tx_cap.dsc_policy = value;
			HDMITX_INFO("set dsc_policy as %d\n", value);
		} else if (strncmp(&tmpbuf[10], "_r", 2) == 0) {
			HDMITX_INFO("%d\n", hdev->hw_comm.hdmi_tx_cap.dsc_policy);
		}
	} else if (strncmp(tmpbuf, "vrr_dbg_vframe", 14) == 0) {
		ret = kstrtoul(tmpbuf + 14, 10, &value);
		hdev->vrr_dbg_vframe = value;
		HDMITX_INFO("vrr_dbg_vframe :%d\n", hdev->vrr_dbg_vframe);
	} else if (strncmp(tmpbuf, "emp_no", 6) == 0) {
		ret = kstrtoul(tmpbuf + 6, 10, &value);
		hdev->emp_no = value;
		HDMITX_INFO("emp_no :%d\n", hdev->emp_no);
	} else if (strncmp(tmpbuf, "emp_verbose", 11) == 0) {
		ret = kstrtoul(tmpbuf + 11, 10, &value);
		hdev->emp_verbose = value;
		HDMITX_INFO("emp_verbose :%d\n", hdev->emp_verbose);
	}
}

void hdmitx21_pm_restore(struct hdmitx_common *tx_comm)
{
	struct hdmitx_hw_common *tx_hw_base = NULL;
	bool arg = false;
	bool hpd_state;

	if (!tx_comm || !tx_comm->tx_hw) {
		HDMITX_ERROR("%s: tx_comm or tx_hw is NULL\n", __func__);
		return;
	}
	tx_hw_base = tx_comm->tx_hw;

	HDMITX_INFO("%s restore\n", __func__);
	mutex_lock(&tx_comm->base.set_mode_mutex);
	/*
	 * if hdmitx init and display already, then should not do
	 * HW init again as it may change clk settings, otherwise
	 * need to do hw init as did in driver probe in case HW is
	 * in power down or unknown state
	 */
	hdmitx_hw_cntl(tx_hw_base, CORE_MISC_HW_INIT, (void *)&arg, NULL);
	/*
	 * after suspend to disk and before resume, it may change TV set,
	 * need to update EDID/HPD/fmt_para by current HW status
	 * as which did in driver probe
	 */
	hpd_state = !!hdmitx_hw_cntl(tx_hw_base, PLATFORM_GET_HPD_GPI_ST, NULL, NULL);
	hdmitx_hw_cntl(tx_hw_base, MODE_FLOW_HPD_IRQ_TOP_HALF, (void *)&hpd_state, NULL);
	/* actions in bottom half of plug intr */
	/* need to parse EDID as vinfo need edid information */
	if (hpd_state) {
		tx_comm->base.parse_edid_by_tx_cfg = true;
		meson_tx_plugin_unlocked(&tx_comm->base);
		tx_comm->base.parse_edid_by_tx_cfg = false;
	} else {
		meson_tx_plugout_unlocked(&tx_comm->base);
	}
	/* load fmt para from hw info */
	hdmitx_common_init_bootup_format_para(tx_comm, &tx_comm->fmt_para);
	/* load init hdr state for HW info */
	hdmitx_hdr_load_hw_state(tx_comm);
	hdmitx_bootup_post_process(tx_comm);
	mutex_unlock(&tx_comm->base.set_mode_mutex);

	/* notify to drm hdmi */
	meson_tx_fire_drm_hpd_cb_unlocked(&tx_comm->base);
}

static void hdmitx_pre_display_init(struct hdmitx21_dev *hdev)
{
	u8 update_flags = 0;
	u32 arg;

	if (hdev->tx_comm.base.rxcap.max_frl_rate > FRL_NONE &&
		hdev->tx_comm.base.rxcap.scdc_present == 1 &&
		/* hdmitx21_hw_cntl(&hdev->hw_comm, FRL_GET_MODE, NULL, NULL)&& */
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate == FRL_NONE) {
		/*
		 * refer to LTS:L Source in FRL link training procedure:
		 * for FRL->legacy TMDS mode(LTS:4->LTS:L),
		 * or source init TMDS mode(LTS:1->LTS:L)
		 * 1. IF (SCDC_Present = 1)
		 * Source shall clear (=0) FRL_Rate indicating TMDS.
		 * IF FLT_update is currently set, Source shall clear
		 * FLT_update by writing "1".
		 * 2.Source shall start legacy TMDS operation when its
		 * content is ready.
		 */
		scdc_tx_frl_cfg1_set(0);
		update_flags = scdc_tx_update_flags_get();
		if ((update_flags & FLT_UPDATE) != 0)
			scdc_tx_update_flags_set(update_flags);
		arg = TMDS_PHY_DISABLE;
		hdmitx21_hw_cntl(&hdev->hw_comm, PLATFORM_PHY_OP, (void *)&arg, NULL);
	} else if (hdev->tx_comm.base.rxcap.max_frl_rate > FRL_NONE &&
		hdev->tx_comm.base.rxcap.scdc_present == 1 &&
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate > FRL_NONE &&
		hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate < FRL_RATE_MAX &&
		hdmitx21_hw_cntl(&hdev->hw_comm, FRL_GET_MODE, NULL, NULL) == FRL_NONE) {
		/*
		 * refer to LTS:L Source in FRL link training procedure:
		 * for case(LTS:L->LTS:2) switch from TMDS mode to FRL mode
		 *
		 * IF (Max_FRL_Rate > 0) AND (SCDC_Present = 1) AND (SCDC Sink Version != 0)
		 * IF Source chooses to operate in FRL mode
		 * Source should use AV mute
		 * Source shall stop TMDS transmission
		 * Source shall EXIT to LTS:2
		 * END IF
		 * END IF
		 */
		/* AV mute is set by system earlier */
		arg = TMDS_PHY_DISABLE;
		hdmitx21_hw_cntl(&hdev->hw_comm, PLATFORM_PHY_OP, (void *)&arg, NULL);
	} else if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate == FRL_NONE &&
		hdmitx21_hw_cntl(&hdev->hw_comm, FRL_GET_MODE, NULL, NULL) == FRL_NONE) {
		/*
		 * for cases switch between TMDS modes
		 * per hdmi2.1 spec chapter 6.1.3.2, when source change
		 * tmds_bit_clk_ratio, should follow the steps:
		 * 1.disable tmds clk/data
		 * 2.change tmds_bit_clk_ratio 0 <-> 1
		 * 3.resume tmds clk/data transmission in 1~100 ms
		 */
		arg = TMDS_PHY_DISABLE;
		hdmitx21_hw_cntl(&hdev->hw_comm, PLATFORM_PHY_OP, (void *)&arg, NULL);
	}
	/*
	 * for cases switch between FRL modes: todo
	 * may refer to LTS:P Source in FRL link training procedure
	 * LTS:P->LTS:2
	 * IF Source initiates request for retraining
	 * Source should use AV mute if video is active
	 * Source shall stop FRL transmission including Gap-character-only transmission
	 * Source shall EXIT to LTS:2
	 */

	/* clear vsif/avi */
}

static int hdmitx21_pre_enable_mode(struct hdmitx_common *tx_comm)
{
	struct hdmitx21_dev *hdev = container_of(tx_comm, struct hdmitx21_dev, tx_comm);
	struct meson_tx_format_para *para = &tx_comm->fmt_para;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	int hdcptx_mode = 0;
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;
#endif
#ifdef CONFIG_AMLOGIC_DSC
	struct dsc_notifier_data_s dsc_notifier_data;
	int ret = -1;
#endif
	enum frl_rate_enum source_test_frl_rate = FRL_NONE;
	/*
	 * disable hdcp before set mode if hdcp enabled.
	 * normally hdcp is disabled before setting mode
	 * when disable phy, but for special case of bootup,
	 * if mode changed as it's different with uboot mode,
	 * hdcp is not stopped firstly, and may hdcp fail
	 */
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	if (hdcptx_comm->hdcptx_api->get_hdcptx_mode)
		hdcptx_mode = hdcptx_comm->hdcptx_api->get_hdcptx_mode(tx_comm->hdcptx_priv);
	if (hdcptx_mode != 0) {
		if (hdcptx_comm->hdcptx_api->hdcptx_disable)
			hdcptx_comm->hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
	}
#endif

	/* below is only for FRL/DSC */
	if (tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate == FRL_NONE)
		return 0;

	if (tx_comm->base.rxcap.max_frl_rate) {
		u8 sink_ver = scdc_tx_sink_version_get();
		u8 test_cfg = 0;

		if (!sink_ver)
			HDMITX_INFO("sink version %d\n", sink_ver);
		scdc_tx_source_version_set(1);
		test_cfg = scdc_tx_source_test_cfg_get();
		/* per 2.1 spec, if both are set, then treat as if both are cleared */
		if ((test_cfg & FRL_MAX) && (test_cfg & DSC_FRL_MAX)) {
			HDMITX_INFO("warning: both FRL_MAX and DSC_FRL_MAX are set, ignore\n");
		} else if (test_cfg & FRL_MAX) {
			source_test_frl_rate = min(tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate,
				hdev->tx_comm.base.rxcap.max_frl_rate);
			HDMITX_INFO("CTS: choose frl_max %d\n", source_test_frl_rate);
		} else if (test_cfg & DSC_FRL_MAX) {
			source_test_frl_rate = min(tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate,
				hdev->tx_comm.base.rxcap.dsc_max_frl_rate);
			HDMITX_INFO("CTS: choose dsc_frl_max %d\n", source_test_frl_rate);
		}
		if (para->tx_hw_para.hdmitx_hw_para.frl_rate >
			tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate)
			HDMITX_ERROR("Current frl_rate %d is larger than tx_max_frl_rate %d\n",
				para->tx_hw_para.hdmitx_hw_para.frl_rate,
				tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate);
	} else {
		para->tx_hw_para.hdmitx_hw_para.frl_rate = FRL_NONE;
		para->tx_hw_para.hdmitx_hw_para.dsc_en = 0;
	}

	/* source_test_frl_rate has the highest priority */
	if (source_test_frl_rate > FRL_NONE && source_test_frl_rate < FRL_RATE_MAX)
		para->tx_hw_para.hdmitx_hw_para.frl_rate = source_test_frl_rate;

#ifdef CONFIG_AMLOGIC_DSC
	if (hdev->tx_comm.fmt_para.tx_hw_para.hdmitx_hw_para.dsc_en) {
		/*
		 * notify hdmitx format to dsc, and dsc module will
		 * calculate pps data and venc/pixel clock
		 */
		dsc_notifier_data.pic_width = para->timing.h_active;
		dsc_notifier_data.pic_height = para->timing.v_active;
		dsc_notifier_data.color_format = para->cs;
		/*
		 * note: for y422 need set bpc to 8 in pps,
		 * otherwise y422 iter in cts HFR1-85 will fail
		 */
		if (para->cs == HDMI_COLORSPACE_YUV422)
			dsc_notifier_data.bits_per_component = 8;
		else if (para->cd == COLORDEPTH_24B)
			dsc_notifier_data.bits_per_component = 8;
		else if (para->cd == COLORDEPTH_30B)
			dsc_notifier_data.bits_per_component = 10;
		else if (para->cd == COLORDEPTH_36B)
			dsc_notifier_data.bits_per_component = 12;
		else
			dsc_notifier_data.bits_per_component = 8;
		dsc_notifier_data.fps = para->timing.v_freq;
		ret = aml_set_dsc_input_param(&dsc_notifier_data);
		if (ret < 0) {
			HDMITX_ERROR("[%s] set dsc input param error\n", __func__);
		} else {
			hdmitx_get_dsc_data(&hdev->dsc_data);
			HDMITX_INFO("dsc provide enc0_clk: %d, cts_hdmi_pixel_clk: %d\n",
				hdev->dsc_data.enc0_clk,
				hdev->dsc_data.cts_hdmi_tx_pixel_clk);
		}
	}
#endif
	/* if manual_frl_rate is true, set to force frl_rate */
	if (hdev->manual_frl_rate) {
		para->tx_hw_para.hdmitx_hw_para.frl_rate = hdev->manual_frl_rate;
		HDMITX_INFO("manually frl rate %d\n", para->tx_hw_para.hdmitx_hw_para.frl_rate);
	}

	hdmitx_pre_display_init(hdev);
	return 0;
}

static int hdmitx21_enable_mode(struct hdmitx_common *tx_comm)
{
	int ret = 0;

	/* if vic is HDMI_UNKNOWN, hdmitx_set_display will disable HDMI */
	ret = hdmitx_set_display(tx_comm, tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic);

	if (tx_comm->tx_hw->set_aud_mode)
		tx_comm->tx_hw->set_aud_mode(tx_comm->tx_hw, &tx_comm->cur_audio_param);

	return ret;
}

static int hdmitx21_post_enable_mode(struct hdmitx_common *tx_comm)
{
	struct cuva_info *cuva = &tx_comm->base.rxcap.hdr_info.cuva_info;
	struct meson_tx_format_para *para = &tx_comm->fmt_para;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	int hdcprx_ver = 0;
	bool hdcp22_lstore = false;
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;
	struct hdcptx_api *hdcptx_api = hdcptx_comm->hdcptx_api;
#endif

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	/*
	 * wait for TV detect signal stable,
	 * otherwise hdcp may easily auth fail
	 */
	if (hdcptx_comm && hdcptx_comm->not_restart_hdcp) {
		/* self clear */
		hdcptx_comm->not_restart_hdcp = false;
		HDMITX_INFO("special mode switch, not start hdcp\n");
	} else {
		/*
		 * below is only for tmds mode, for FRL mode
		 * hdcp is started after training passed
		 */
		if (para->tx_hw_para.hdmitx_hw_para.frl_rate == FRL_NONE) {
			if (hdcptx_api->get_hdcptx_lstore)
				hdcp22_lstore = hdcptx_api->get_hdcptx_lstore(hdcptx_comm,
					HDCP_VER_HDCP2X);
			if (hdcp22_lstore) {
				if (hdcptx_api->get_hdcprx_ver)
					hdcprx_ver = hdcptx_api->get_hdcprx_ver(hdcptx_comm);
				hdcptx_comm->dw_hdcp22_cap = (hdcprx_ver == 2) ? true : false;
			}
			/*
			 * 0: for hdmitx driver control hdcp
			 * 1/2: drm driver or app control hdcp
			 */
			if (hdmitx_find_vendor_hdcp_delay(tx_comm->base.edid_buf))
				hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv,
					tx_comm->hdcptx_comm.hdcptx_start_type,
					msecs_to_jiffies(750));
			else
				hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv,
					tx_comm->hdcptx_comm.hdcptx_start_type,
					msecs_to_jiffies(500));
		}
	}
#endif

	/*
	 * Step 1: When setting the mode, if the TV supports CUVA receiver mode,
	 * write all zeros to the CUVA EMP hardware buffer
	 */
	if (cuva->rx_mode_sup)
		hdmitx_cuva_dhdr_init(tx_comm);

	return 0;
}

static DEFINE_MUTEX(vid_mute_mutex);
void hdmitx21_video_mute_op(struct hdmitx_hw_common *hw_comm, u32 flag, unsigned int path)
{
	static unsigned int vid_mute_path;
	u32 arg;

	mutex_lock(&vid_mute_mutex);
	if (flag == 0)
		vid_mute_path |= path;
	else
		vid_mute_path &= ~path;

	if (flag == 0) {
		HDMITX_INFO("%s: VID_MUTE path=0x%x\n", __func__, path);
		arg = VIDEO_MUTE;
		hdmitx21_hw_cntl(hw_comm, VPU_VIDEO_MUTE_OP, (void *)&arg, NULL);
	} else {
		/* unmute only if none of the paths are muted */
		if (vid_mute_path == 0) {
			HDMITX_INFO("%s: VID_UNMUTE path=0x%x\n", __func__, path);
			arg = VIDEO_UNMUTE;
			hdmitx21_hw_cntl(hw_comm, VPU_VIDEO_MUTE_OP, (void *)&arg, NULL);
		}
	}
	mutex_unlock(&vid_mute_mutex);
}

#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN

/* TODO, no mutex */
static void hdmitx21_set_hdcp_mode(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcptx_comm instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;

	if (tx_comm->hdcptx_comm.hdcptx_start_type == HDCP_START_NONE) {
		if (hdcptx_comm->hdcptx_api->hdcptx_enable)
			hdcptx_comm->hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv, 1, 0);
	}
	if (tx_comm->hdcptx_comm.hdcptx_start_type == HDCP_START_HDCP1X) {
		if (hdcptx_comm->hdcptx_api->hdcptx_enable)
			hdcptx_comm->hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv, 2, 0);
	}
	if (tx_comm->hdcptx_comm.hdcptx_start_type == HDCP_START_HDCP2X) {
		if (hdcptx_comm->hdcptx_api->hdcptx_enable)
			hdcptx_comm->hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv, 0, 0);
	}
}

/*************DRM connector API**************/
static int hdmitx21_get_hdcp_ver(struct hdmitx_common *tx_comm, char *buf, int len)
{
	int pos = 0;
	struct hdcptx_common *hdcptx_comm = NULL;
	struct hdcptx_api *hdcptx_api = NULL;
	int hdcprx_ver = 0;
	int hdcptx_mode = 0;
	bool hdcp22_lstore = false;

	if (!tx_comm || !tx_comm->hdcptx_comm.hdcptx_api) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return pos;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	hdcptx_api = hdcptx_comm->hdcptx_api;

	if (!hdcptx_comm->hpd_state) {
		HDMITX_HDCP_INFO("%s: hpd low, just return 14\n", __func__);
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "14\n\r");
		return pos;
	}
	if (hdcptx_api->get_hdcptx_mode)
		hdcptx_mode = hdcptx_api->get_hdcptx_mode(tx_comm->hdcptx_priv);
	if (hdcptx_comm->dw_hdcp22_cap) {
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "22\n\r");
	} else {
		/*
		 * on hotplug case
		 * note that, when do hdcp repeater1.4 CTS,
		 * hdcp port access will affect item 3a-02 Irregular
		 * procedure: (First part of authentication) HDCP port
		 * access. Refer to hdcp1.4 cts spec: "If DUT does
		 * not read an HDCP register past 4 seconds after
		 * the previous attempt, then FAIL". after read hdcp
		 * version soon after plugin(access failed as TE
		 * not ack), our hdmitx side should keep retrying
		 * in 4S. but source TE start hdcp auth with
		 * hdmirx side too late(more than 4S), as hdcp auth
		 * of hdmitx side is started by hdmirx side, it will
		 * time out the access of hdcp port of 4 second.
		 * so for repeater CTS, should not read hdcp version
		 * whenever you want. Here add protect to only read
		 * hdcp version when currently not in hdmirx channel.
		 * special customer want to read downstream hdcp version
		 * after hotplug, and only send 4K output when EDID
		 * support 4K && support hdcp2.2. so add is_4k_sink()
		 * decision, it won't affect hdcp repeater CTS.
		 */
		if (hdcptx_need_ctrl_by_upstream(hdcptx_comm->hdcp_rpt_en)) {
			HDMITX_HDCP_INFO("%s: currently should not read hdcp version\n", __func__);
		} else if (hdcptx_mode == 0) {
			if (hdcptx_api->get_hdcptx_lstore)
				hdcp22_lstore = hdcptx_api->get_hdcptx_lstore(hdcptx_comm,
					HDCP_VER_HDCP2X);
			if (hdcp22_lstore) {
				if (hdcptx_api->get_hdcprx_ver)
					hdcprx_ver = hdcptx_api->get_hdcprx_ver(hdcptx_comm);
				if (hdcprx_ver == 2) {
					pos += snprintf(buf + pos, PAGE_SIZE - pos, "22\n\r");
					hdcptx_comm->dw_hdcp22_cap = 1;
				}
			}
			HDMITX_HDCP_INFO("%s: hdcp_mode: 0, rx_hdcp22_cap = %d\n",
				__func__, hdcptx_comm->dw_hdcp22_cap);
		}
	}
	/* Here, must assume RX support HDCP14, otherwise affect 1A-03 */
	pos += snprintf(buf + pos, PAGE_SIZE - pos, "14\n\r");

	return pos;
}

static int hdmitx21_get_tx_hdcp_cap(struct hdmitx_common *tx_comm)
{
	unsigned int hdcptx_cap = 0;

	if (!tx_comm) {
		HDMITX_ERROR("get hdcp cap NULL tx_comm instance\n");
		return 0;
	}

	/* check hdcp key load status */
	hdcptx_cap = hdmitx_common_get_key_store(tx_comm);
	return hdcptx_cap;
}

/* hdcp functions for linux/drm start */
/* should sync with hdmitx21_enable_hdcp() and hdmitx_start_hdcp_handler()
 * hdmi mode setting/hdcp enable or disable should be mutexed.
 * time delay may be needed after hdmi mode setting and before hdcp enable
 * so that hdcp auth is conducted under TV signal detection stable.
 */
static void drm_hdmitx_hdcp_enable(struct hdmitx_common *tx_comm, int hdcp_type)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx_api *hdcptx_api;

	if (!tx_comm) {
		HDMITX_ERROR("NULL tx_comm or NULL hdcptx_comm instance\n");
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	hdcptx_api = hdcptx_comm->hdcptx_api;

	mutex_lock(&tx_comm->base.set_mode_mutex);
	hdcptx_comm->hdcp_mode = hdcp_type;
	HDMITX_HDCP_INFO("%s: %d\n", __func__, hdcp_type);
	if (!tx_comm->ready || !tx_comm->base.hpd_state) {
		HDMITX_ERROR("%s hdmitx ready:%d. hpd: %d, skip hdcp auth\n",
			__func__, tx_comm->ready, tx_comm->base.hpd_state);
		mutex_unlock(&tx_comm->base.set_mode_mutex);
		return;
	}
	if (tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.frl_rate) {
		HDMITX_HDCP_INFO("%s hdcp en for frl is on hdmitx side, skip\n", __func__);
		mutex_unlock(&tx_comm->base.set_mode_mutex);
		return;
	}
	/*
	 * hdcp auth may pass->fail->pass as hdmi signal detection
	 * need some time on TV side, so need postpone auth time to
	 * wait TV side signal detection stable
	 */
	if (hdcptx_comm->hdcp_debug_delay)
		hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv, hdcp_type,
			hdcptx_comm->hdcp_debug_delay);
	else if (hdmitx_find_vendor_hdcp_delay(tx_comm->base.edid_buf))
		hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv,
			hdcp_type, 750);
	else
		hdcptx_api->hdcptx_enable(tx_comm->hdcptx_priv,
			hdcp_type, 500);
	mutex_unlock(&tx_comm->base.set_mode_mutex);
}

static void drm_hdmitx_hdcp_disable(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL private_hdcp instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;

	mutex_lock(&tx_comm->base.set_mode_mutex);
	if (hdcptx_comm->hdcptx_api->hdcptx_disable)
		hdcptx_comm->hdcptx_api->hdcptx_disable(tx_comm->hdcptx_priv);
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	HDMITX_HDCP_INFO("%s\n", __func__);
	mutex_unlock(&tx_comm->base.set_mode_mutex);
}

static void drm_hdmitx_hdcp_disconnect(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	mutex_lock(&tx_comm->base.set_mode_mutex);
	/* hdcp is disabled when hdmitx driver plugout, no need disable again */
	tx_comm->hdcptx_comm.hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
	HDMITX_HDCP_INFO("%s\n", __func__);
	mutex_unlock(&tx_comm->base.set_mode_mutex);
}

static void drm_hdmitx_hdcp_init(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;

	if (!tx_comm || !hdcptx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.data = NULL;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;

	HDMITX_DEBUG_HDCP("%s\n", __func__);
}

static void drm_hdmitx_hdcp_exit(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;

	if (!tx_comm || !hdcptx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.data = NULL;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;
	HDMITX_DEBUG_HDCP("%s\n", __func__);
}

/* hdcp functions for linux/drm end */

static void hdcp21_video_mute_op(void *data, u32 flag, unsigned int path)
{
	struct hdmitx_hw_common *hw_comm = (struct hdmitx_hw_common *)data;

	if (!hw_comm) {
		HDMITX_ERROR("NULL hw_comm instance\n");
		return;
	}
	hdmitx21_video_mute_op(hw_comm, flag, path);
}

static void hdcp21_audio_mute_op(void *data, u32 flag, unsigned int path)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;

	if (!tx_comm) {
		HDMITX_ERROR("NULL tx_comm instance\n");
		return;
	}
	hdmitx_audio_mute_op(tx_comm, flag, path);
}

static int hdcp21_avmute_locked(void *data,
	int mute_flag, int mute_path_hint)
{
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;

	if (!tx_comm) {
		HDMITX_ERROR("NULL tx_comm instance\n");
		return -EINVAL;
	}
	return hdmitx_common_avmute_locked(tx_comm, mute_flag, mute_path_hint);
}

static int hdcp21_event_mgr_notify(void *data, unsigned long state, void *arg)
{
	struct meson_tx_event_mgr *event_mgr = (struct meson_tx_event_mgr *)data;

	if (!event_mgr) {
		HDMITX_ERROR("NULL event_mgr instance\n");
		return -EINVAL;
	}
	return meson_tx_event_mgr_notify(event_mgr, state, arg);
}

static void hdcptx_21_ctrl_ops_init(struct hdcptx_common *hdcptx_comm)
{
	if (!hdcptx_comm) {
		HDMITX_ERROR("NULL hdcptx_comm instance\n");
		return;
	}

	hdcptx_comm->drm_hdcp_init = drm_hdmitx_hdcp_init;
	hdcptx_comm->drm_hdcp_exit = drm_hdmitx_hdcp_exit;
	hdcptx_comm->drm_hdcp_enable = drm_hdmitx_hdcp_enable;
	hdcptx_comm->drm_hdcp_disable = drm_hdmitx_hdcp_disable;
	hdcptx_comm->drm_hdcp_disconnect = drm_hdmitx_hdcp_disconnect;

	hdcptx_comm->set_hdcp_mode = hdmitx21_set_hdcp_mode;
	hdcptx_comm->get_hdcp_ver = hdmitx21_get_hdcp_ver;
	hdcptx_comm->get_tx_hdcp_cap = hdmitx21_get_tx_hdcp_cap;

	hdcptx_comm->video_mute_op = hdcp21_video_mute_op;
	hdcptx_comm->audio_mute_op = hdcp21_audio_mute_op;
	hdcptx_comm->avmute_op = hdcp21_avmute_locked;
	hdcptx_comm->send_event = hdcp21_event_mgr_notify;
	hdcptx_comm->tmds_enable = hdmitx_tmds_enable;
	hdcptx_comm->tmds_disable = hdmitx_tmds_disable;
	hdcptx_comm->ddcm_read = hdmitx_ddcm_read;
	hdcptx_comm->ddcm_write = hdmitx_ddcm_write;
}
#endif
