// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/vout_notify.h>
#include "hdmitx_dump.h"
#include "hdmitx_hdr.h"
#include "hdmitx_check_valid.h"

int dump_hdmitx_basic_config(struct seq_file *s, void *p)
{
	struct hdmitx_common *tx_comm = NULL;
	struct hdmitx_hw_common *hw_comm  = NULL;
	struct meson_tx_format_para *para = NULL;
	struct tx_timing *timing = NULL;
	char fmt_attr[16] = {0};
	u8 *conf;
	u8 bksv_buf[5];
	int cs, cd, i;
	char buf[512];
	enum hdmi_vic vic;
	enum hdmi_tf_type type = HDMI_NONE;
	u32 hdcp_ret = 0;
	u32 hdcp_lstore = 0;

	if (!s) {
		HDMITX_ERROR("[%s]: s is NULL\n", __func__);
		return -EINVAL;
	}
	tx_comm = s->private;
	hw_comm = tx_comm->tx_hw;
	para = &tx_comm->fmt_para;
	timing = &para->timing;
	seq_puts(s, "\n--------basic config--------\n");
	seq_puts(s, "******hdmi video config******\n");
	seq_printf(s, "display_mode in:%s\n", get_vout_mode_internal());

	vic = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_AVI_VIC, NULL, NULL);
	seq_printf(s, "display_mode out:%s\n", meson_tx_mode_get_timing_name(vic));

	hdmitx_format_para_rebuild_fmt_attr_str(para, fmt_attr, sizeof(fmt_attr));
	seq_printf(s, "attr in:%s\n\r", fmt_attr);
	seq_puts(s, "attr out:");
	cs = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_AVI_CS, NULL, NULL);
	switch (cs & 0x3) {
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
	}
	seq_printf(s, "%s,", conf);

	cd = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_GCP_CD, NULL, NULL);
	switch (cd) {
	case 4:
		conf = "8bit";
		break;
	case 5:
		conf = "10bit";
		break;
	case 6:
		conf = "12bit";
		break;
	case 7:
		conf = "16bit";
		break;
	default:
		conf = "reserved";
	}
	seq_printf(s, "%s\n", conf);
	/* check DV or HDR10+ */
	seq_puts(s, "hdr_status:");
	type = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_HDR10P_ST, NULL, NULL);
	if (type) {
		if (type == HDMI_HDR10P_DV_VSIF)
			seq_puts(s, "HDR10Plus-VSIF");
	} else {
		type = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_AMDV_ST, NULL, NULL);
		if (type == HDMI_DV_VSIF_STD)
			seq_puts(s, "DolbyVision-Std");
		else if (type == HDMI_DV_VSIF_LL)
			seq_puts(s, "DolbyVision-Lowlatency");
	}
	/* check HDR */
	if (type == HDMI_NONE) {
		type = hdmitx_hw_cntl(hw_comm, AUX_PKT_GET_HDR_ST, NULL, NULL);
		if (type == HDMI_HDR_SMPTE_2084)
			seq_puts(s, "HDR10-GAMMA_ST2084");
		else if (type == HDMI_HDR_HLG)
			seq_puts(s, "HDR10-GAMMA_HLG");
		else if (type == HDMI_HDR_HDR)
			seq_puts(s, "HDR10-others");
		else if (type == HDMI_HDR_SDR)
			seq_puts(s, "SDR");
	}
	/* default is SDR */
	if (type == HDMI_NONE)
		seq_puts(s, "SDR");
	seq_puts(s, "\n");
	seq_printf(s, "cur_VIC: %d\n", tx_comm->fmt_para.tx_hw_para.hdmitx_hw_para.vic);
	hdmitx_format_para_print(&tx_comm->fmt_para, buf);
	seq_printf(s, "%s\n", buf);

	seq_puts(s, "******hdcp******\n");
	seq_puts(s, "hdcp mode:");
	switch (tx_comm->hdcptx_comm.hdcp_mode) {
	case 1:
		seq_puts(s, "14");
		break;
	case 2:
		seq_puts(s, "22");
		break;
	default:
		seq_puts(s, "off");
		break;
	}
	if (tx_comm->hdcptx_comm.hdcp_mode > 0) {
		hdcp_ret = hdmitx_hw_cntl(hw_comm,
			HDCP_GET_AUTH_RESULT, NULL, NULL);
		if (hdcp_ret == 1)
			seq_puts(s, ": succeed\n");
		else
			seq_puts(s, ": fail\n");
	}

	seq_puts(s, "hdcp_lstore:");
	hdcp_lstore = tx_comm->hdcptx_comm.hdcp_lstore;
	if (hdcp_lstore < 0x10) {
		hdcp_lstore = 0;
		if (hdmitx_hw_cntl(hw_comm, HDCP_14_LSTORE, NULL, NULL))
			hdcp_lstore |= BIT(0);
		if (hdmitx_hw_cntl(hw_comm, HDCP_22_LSTORE, NULL, NULL))
			hdcp_lstore |= BIT(1);
	}
	if ((hdcp_lstore & 0x3) == 0x3) {
		seq_puts(s, "14+22\n");
	} else {
		if (hdcp_lstore & 0x1)
			seq_puts(s, "14\n");
		if (hdcp_lstore & 0x2)
			seq_puts(s, "22\n");
		if ((hdcp_lstore & 0xf) == 0)
			seq_puts(s, "00\n");
	}

	seq_puts(s, "hdcp_ver:");
	hdmitx_get_hdcp_ver(tx_comm, buf, 512);
	seq_printf(s, "%s\n", buf);
	hdmitx_hw_cntl(hw_comm, HDCP_GET_BKSV, NULL, bksv_buf);
	seq_puts(s, "HDCP14 BKSV: ");
	for (i = 0; i < 5; i++)
		seq_printf(s, "%02x", bksv_buf[i]);

	seq_printf(s, "hdcp_ctl_lvl:%d\n", tx_comm->hdcptx_comm.hdcp_ctl_lvl);

	seq_puts(s, "******hdmi_pll******\n");
	seq_printf(s, "sspll:%d\n", tx_comm->sspll);

	meson_tx_hdr_dump(tx_comm->hdr_state, s);

	seq_puts(s, "***current timing param***\n");
	seq_printf(s, "vic: %d\n", timing->vic);
	seq_printf(s, "name: %s\n", timing->name);
	seq_printf(s, "enc_idx: %d\n", tx_comm->enc_idx);
	if (timing->sname)
		seq_printf(s, "sname: %s\n", timing->sname);
	seq_printf(s, "pi_mode: %c\n", timing->pi_mode ? 'P' : 'I');
	seq_printf(s, "h/v_freq: %d/%d\n", timing->h_freq, timing->v_freq);
	seq_printf(s, "pixel_freq: %d\n", timing->pixel_freq);
	seq_printf(s, "h_total: %d\n", timing->h_total);
	seq_printf(s, "h_blank: %d\n", timing->h_blank);
	seq_printf(s, "h_front: %d\n", timing->h_front);
	seq_printf(s, "h_sync: %d\n", timing->h_sync);
	seq_printf(s, "h_back: %d\n", timing->h_back);
	seq_printf(s, "h_active: %d\n", timing->h_active);
	seq_printf(s, "v_total: %d\n", timing->v_total);
	seq_printf(s, "v_blank: %d\n", timing->v_blank);
	seq_printf(s, "v_front: %d\n", timing->v_front);
	seq_printf(s, "v_sync: %d\n", timing->v_sync);
	seq_printf(s, "v_back: %d\n", timing->v_back);
	seq_printf(s, "v_active: %d\n", timing->v_active);
	seq_printf(s, "v_sync_ln: %d\n", timing->v_sync_ln);
	seq_printf(s, "h/v_pol: %d/%d\n", timing->h_pol, timing->v_pol);
	seq_printf(s, "h/v_pict: %d/%d\n", timing->h_pict, timing->v_pict);
	seq_printf(s, "h/v_pixel: %d/%d\n", timing->h_pixel, timing->v_pixel);

	seq_puts(s, "***hdmi audio config***\n");
	if (tx_comm->cur_audio_param.aud_output_en)
		conf = "on";
	else
		conf = "off";
	seq_printf(s, "audio config: %s\n", conf);
	seq_printf(s, "type: %u, chnum: %u, samrate: %u, samsize: %u\n",
		tx_comm->hdmiaud_config_data.type,
		tx_comm->hdmiaud_config_data.chs,
		tx_comm->hdmiaud_config_data.rate,
		tx_comm->hdmiaud_config_data.size);

	switch (tx_comm->cur_audio_param.aud_src_if) {
	case AUD_SRC_IF_SPDIF:
		conf = "SPDIF";
		break;
	case AUD_SRC_IF_I2S:
		conf = "I2S";
		break;
	case AUD_SRC_IF_TDM:
		conf = "TDM";
		break;
	default:
		conf = "none";
	}
	seq_printf(s, "audio source: %s\n", conf);

	switch (tx_comm->cur_audio_param.type) {
	case CT_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case CT_PCM:
		conf = "L-PCM";
		break;
	case CT_AC_3:
		conf = "AC-3";
		break;
	case CT_MPEG1:
		conf = "MPEG1";
		break;
	case CT_MP3:
		conf = "MP3";
		break;
	case CT_MPEG2:
		conf = "MPEG2";
		break;
	case CT_AAC:
		conf = "AAC";
		break;
	case CT_DTS:
		conf = "DTS";
		break;
	case CT_ATRAC:
		conf = "ATRAC";
		break;
	case CT_ONE_BIT_AUDIO:
		conf = "One Bit Audio";
		break;
	case CT_DD_P:
		conf = "Dolby Digital+";
		break;
	case CT_DTS_HD:
		conf = "DTS_HD";
		break;
	case CT_MAT:
		conf = "MAT";
		break;
	case CT_DST:
		conf = "DST";
		break;
	case CT_WMA:
		conf = "WMA";
		break;
	default:
		conf = "MAX";
	}
	seq_printf(s, "audio type: %s\n", conf);

	switch (tx_comm->cur_audio_param.chs) {
	case CC_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case CC_2CH:
		conf = "2 channels";
		break;
	case CC_3CH:
		conf = "3 channels";
		break;
	case CC_4CH:
		conf = "4 channels";
		break;
	case CC_5CH:
		conf = "5 channels";
		break;
	case CC_6CH:
		conf = "6 channels";
		break;
	case CC_7CH:
		conf = "7 channels";
		break;
	case CC_8CH:
		conf = "8 channels";
		break;
	default:
		conf = "MAX";
	}
	seq_printf(s, "audio channel num: %s\n", conf);

	switch (tx_comm->cur_audio_param.rate) {
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
		break;
	case FS_768K:
		conf = "768kHz";
		break;
	default:
		conf = "MAX";
	}
	seq_printf(s, "audio sample rate: %s\n", conf);

	switch (tx_comm->cur_audio_param.size) {
	case SS_REFER_TO_STREAM:
		conf = "refer to stream header";
		break;
	case SS_16BITS:
		conf = "16bit";
		break;
	case SS_20BITS:
		conf = "20bit";
		break;
	case SS_24BITS:
		conf = "24bit";
		break;
	default:
		conf = "MAX";
	}
	seq_printf(s, "audio sample size: %s\n", conf);

	seq_puts(s, "***efuse_config***\n");
	seq_printf(s, "FEAT_DISABLE_HDMI_60HZ = %d\n",
		tx_comm->efuse_dis_hdmi_4k60);
	seq_printf(s, "FEAT_DISABLE_OUTPUT_4K = %d\n\r",
		tx_comm->efuse_dis_output_4k);
	seq_printf(s, "FEAT_DISABLE_HDCP_TX_22 = %d\n\r",
		tx_comm->hdcptx_comm.efuse_dis_hdcp_tx22);
	seq_printf(s, "FEAT_DISABLE_HDMI_TX_3D = %d\n\r",
		tx_comm->efuse_dis_hdmi_tx3d);
	seq_printf(s, "FEAT_DISABLE_HDMI = %d\n\r",
		tx_comm->hdcptx_comm.efuse_dis_hdcp_tx14);

	seq_puts(s, "***dts_config_data***\n");
	seq_printf(s, "pxp_mode: %d, dongle_mode: %d\n\r",
		tx_comm->pxp_mode, tx_comm->tx_hw->dongle_mode);
	seq_printf(s, "res_1080p: %d, max_refresh_rate: %d, tx_max_frl_rate: %d\n\r",
		tx_comm->res_1080p, tx_comm->max_refresh_rate,
		tx_comm->tx_hw->hdmi_tx_cap.tx_max_frl_rate);
	seq_printf(s, "hdcp_ctl_lvl: %d, hdcp_type_policy: %d, hdcp_rpt_en: %d\n\r",
		tx_comm->hdcptx_comm.hdcp_ctl_lvl, tx_comm->hdcp_type_policy,
		tx_comm->hdcptx_comm.hdcp_rpt_en);
	seq_printf(s, "hdcp_ctl_lvl: %d, hdcp_type_policy: %d, hdcp_rpt_en: %d\n\r",
		tx_comm->hdcptx_comm.hdcp_ctl_lvl, tx_comm->hdcp_type_policy,
		tx_comm->hdcptx_comm.hdcp_rpt_en);
	seq_printf(s, "cedst_en: %d, rxsense_policy: %d\n\r",
		tx_comm->cedst_en, tx_comm->rxsense_policy);
	seq_printf(s, "hdr_8bit_en: %d, enc_idx: %d arc_rx_en: %d\n\r",
		tx_comm->hdr_state->hdr_8bit_en, tx_comm->enc_idx, tx_comm->arc_rx_en);

	seq_printf(s, "hdmitx already_used_state = %d\n\r",
		tx_comm->base.already_used_state);

	return 0;
}

int hdmirx_info_show(struct seq_file *s, void *v)
{
	int i, j;
	int num;
	int block_num = 0;
	char *buf = NULL;
	int len;
	struct hdmitx_common *tx_comm = NULL;
	struct rx_cap *prxcap = NULL;
	const struct dv_info *dv = NULL;
	const struct hdr_info *hdr = NULL;
	const struct cuva_info *cuva = NULL;
	const struct hdr10_plus_info *hdr10p = NULL;
	const struct sbtm_info *sbtm = NULL;
	const struct tx_timing *timing = NULL;
	const char *mode_name;
	enum hdmi_vic vic;
	int vic_len = 0;
	int *edid_vics = NULL;
	enum hdmi_vic prefer_vic = HDMI_0_UNKNOWN;
	int hdr10plus_supported = 0;

	if (!s) {
		HDMITX_ERROR("%s s is NULL\n", __func__);
		return 0;
	}
	tx_comm = s->private;
	prxcap = &tx_comm->base.rxcap;
	dv = &prxcap->dv_info;
	hdr = &prxcap->hdr_info;
	cuva = &hdr->cuva_info;
	hdr10p = &hdr->hdr10plus_info;
	sbtm = &hdr->sbtm_info;
	vic_len = prxcap->VIC_count + VESA_MAX_TIMING;
	edid_vics = vmalloc(vic_len * sizeof(int));
	len = 2048;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf) {
		HDMITX_ERROR("%s kcalloc failed\n", __func__);
		return 0;
	}

	seq_puts(s, "************hdmirx_info************\n");

	seq_puts(s, "******hpd_edid_parsing******\n");
	seq_printf(s, "hpd: %d", tx_comm->base.hpd_state);

	seq_puts(s, "\n******rawedid******\n");

	block_num = meson_tx_edid_valid_block_num(tx_comm->base.edid_buf);
	if (block_num <= 8)
		num = block_num * 128;
	else
		num = 8 * 128;

	for (i = 0; i < num; i++)
		seq_printf(s, "%02x",
			tx_comm->base.edid_buf[i]);

	seq_puts(s, "\n");

	seq_puts(s, "\nedid_parsing:");
	if (meson_tx_edid_check_valid(&tx_comm->base.rxcap,
		tx_comm->base.edid_buf))
		seq_puts(s, "ok\n");
	else
		seq_puts(s, "ng\n");

	seq_puts(s, "\n******edid******\n");
	seq_puts(s, "sink type:");
	if (!tx_comm->base.hpd_state)
		seq_puts(s, " none\n");
	else if (tx_comm->base.rxcap.vsdb_phy_addr.b)
		seq_puts(s, " repeater\n");
	else
		seq_puts(s, " sink\n");
	meson_tx_edid_print_rx_cap(&tx_comm->base.rxcap, buf, len);
	seq_printf(s, buf);
	memset(buf, 0, len * sizeof(char));

	seq_puts(s, "\n******dc_cap******\n");

	/* DVI case, only rgb,8bit */
	if (prxcap->ieeeoui != HDMI_IEEE_OUI) {
		seq_puts(s, "rgb,8bit\n");
	} else {
		if (prxcap->dc_36bit_420)
			seq_puts(s, "420,12bit\n");
		if (prxcap->dc_30bit_420)
			seq_puts(s, "420,10bit\n");

		for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
			if (prxcap->y420_vic[i]) {
				seq_puts(s, "420,8bit\n");
				break;
			}
		}

		if (prxcap->native_Mode & CAP_BIT5_YCBCR_444_MASK) {
			if (prxcap->dc_y444) {
				if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
					seq_puts(s, "444,12bit\n");
				if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
					seq_puts(s, "444,10bit\n");
			}
			seq_puts(s, "444,8bit\n");
		}
		/* y422, not check dc */
		if (prxcap->native_Mode & CAP_BIT4_YCBCR_422_MASK)
			seq_puts(s, "422,12bit\n");

		if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
			seq_puts(s, "rgb,12bit\n");
		if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
			seq_puts(s, "rgb,10bit\n");
		seq_puts(s, "rgb,8bit\n");
	}

	seq_puts(s, "\n******disp_cap******\n");
	mutex_lock(&tx_comm->base.valid_mode_mutex);
	memset(edid_vics, 0, vic_len * sizeof(int));
	/* step1: only select VIC which is supported in EDID */
	/* copy edid vic list */
	if (prxcap->VIC_count > 0)
		memcpy(edid_vics, prxcap->VIC, sizeof(int) * prxcap->VIC_count);
	for (i = 0; i < VESA_MAX_TIMING && prxcap->vesa_timing[i]; i++)
		edid_vics[prxcap->VIC_count + i] = prxcap->vesa_timing[i];
	for (i = 0; i < vic_len; i++) {
		vic = edid_vics[i];
		if (vic == HDMI_0_UNKNOWN)
			continue;
		prefer_vic = hdmitx_get_prefer_vic(tx_comm, vic);
		/* if mode_best_vic is support by RX, try 16x9 first */
		if (prefer_vic != vic) {
			HDMITX_DEBUG("%s: check prefer vic:%d exist, ignore [%d].\n",
					__func__, prefer_vic, vic);
			continue;
		}
		timing = meson_tx_mode_vic_to_timing(vic);
		if (!timing) {
			/* HDMITX_ERROR("%s: unsupport vic [%d]\n", __func__, vic); */
			continue;
		}
		/* step2, check if VIC is supported by SOC hdmitx */
		if (hdmitx_common_validate_vic(tx_comm, vic) != 0) {
			/* HDMITX_ERROR("%s: vic[%d] over range.\n", __func__, vic); */
			continue;
		}
		/*
		 * step3, build format with basic mode/attr and check
		 * if it's supported by EDID/hdmitx_cap
		 */
		if (hdmitx_common_check_valid_para_of_vic(tx_comm, vic) != 0) {
			/* HDMITX_ERROR("%s: vic[%d] check fmt attr failed.\n", __func__, vic); */
			continue;
		}
		mode_name = timing->sname ? timing->sname : timing->name;
		seq_printf(s, "%s", mode_name);
		if (vic == prxcap->native_vic)
			seq_puts(s, "*\n");
		else
			seq_puts(s, "\n");
	}
	vfree(edid_vics);
	mutex_unlock(&tx_comm->base.valid_mode_mutex);

	seq_puts(s, "\n******dv_cap******\n");
	if (dv->ieeeoui != DV_IEEE_OUI || dv->block_flag != CORRECT) {
		seq_puts(s, "The Rx don't support DolbyVision\n");
	} else {
		seq_puts(s, "DolbyVision RX support list:\n");

		if (dv->ver == 0) {
			seq_printf(s, "VSVDB Version: V%d\n", dv->ver);
			seq_printf(s, "2160p%shz: 1\n", dv->sup_2160p60hz ? "60" : "30");
			seq_puts(s, "Support mode:\n");
			seq_puts(s, "  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				seq_puts(s, "  DV_YCbCr_422_12BIT\n");
		}
		if (dv->ver == 1) {
			seq_printf(s, "VSVDB Version: V%d(%d-byte)\n",
				dv->ver, dv->length + 1);
			if (dv->length == 0xB) {
				seq_printf(s, "2160p%shz: 1\n",
					dv->sup_2160p60hz ? "60" : "30");
			seq_puts(s, "Support mode:\n");
			seq_puts(s, "  DV_RGB_444_8BIT\n");
			if (dv->sup_yuv422_12bit)
				seq_puts(s, "  DV_YCbCr_422_12BIT\n");
			if (dv->low_latency == 0x01)
				seq_puts(s, "  LL_YCbCr_422_12BIT\n");
			}

			if (dv->length == 0xE) {
				seq_printf(s, "2160p%shz: 1\n",
					dv->sup_2160p60hz ? "60" : "30");
				seq_puts(s, "Support mode:\n");
				seq_puts(s, "  DV_RGB_444_8BIT\n");
				if (dv->sup_yuv422_12bit)
					seq_puts(s, "  DV_YCbCr_422_12BIT\n");
			}
		}
		if (dv->ver == 2) {
			seq_printf(s, "VSVDB Version: V%d\n", dv->ver);
			seq_printf(s, "2160p%shz: 1\n",
				dv->sup_2160p60hz ? "60" : "30");
			seq_printf(s, "Parity: %d\n", dv->parity);
			seq_puts(s, "Support mode:\n");
			if (dv->Interface != 0x00 && dv->Interface != 0x01) {
				seq_puts(s, "  DV_RGB_444_8BIT\n");
				if (dv->sup_yuv422_12bit)
					seq_puts(s, "  DV_YCbCr_422_12BIT\n");
			}
			seq_puts(s, "  LL_YCbCr_422_12BIT\n");
			if (dv->Interface == 0x01 || dv->Interface == 0x03) {
				if (dv->sup_10b_12b_444 == 0x1)
					seq_puts(s, "  LL_RGB_444_10BIT\n");
				if (dv->sup_10b_12b_444 == 0x2)
					seq_puts(s, "  LL_RGB_444_12BIT\n");
			}
		}
		seq_printf(s,
			"IEEEOUI: 0x%06x\n", dv->ieeeoui);
		seq_printf(s, "EMP: %d\n", dv->dv_emp_cap);
		seq_puts(s, "VSVDB: ");
		for (i = 0; i < (dv->length + 1); i++)
			seq_printf(s, "%02x", dv->rawdata[i]);
		seq_puts(s, "\n");
	}

	seq_puts(s, "\n******hdr_cap******\n");
	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
			hdr10p->application_version != 0xFF)
		hdr10plus_supported = 1;
	seq_printf(s, "HDR10Plus Supported: %d\n",
		hdr10plus_supported);
	seq_puts(s, "HDR Static Metadata:\n");
	seq_puts(s, "    Supported EOTF:\n");
	seq_printf(s, "        Traditional SDR: %d\n",
		!!(hdr->hdr_support & 0x1));
	seq_printf(s, "        Traditional HDR: %d\n",
		!!(hdr->hdr_support & 0x2));
	seq_printf(s, "        SMPTE ST 2084: %d\n",
		!!(hdr->hdr_support & 0x4));
	seq_printf(s, "        Hybrid Log-Gamma: %d\n",
		!!(hdr->hdr_support & 0x8));
	seq_printf(s, "    Supported SMD type1: %d\n",
		hdr->static_metadata_type1);
	seq_puts(s, "    Luminance Data\n");
	seq_printf(s, "        Max: %d\n",
		hdr->lumi_max);
	seq_printf(s, "        Avg: %d\n",
		hdr->lumi_avg);
	seq_printf(s, "        Min: %d\n\n",
		hdr->lumi_min);
	seq_puts(s, "HDR Dynamic Metadata:");

	for (i = 0; i < 4; i++) {
		if (hdr->dynamic_info[i].type == 0)
			continue;
		seq_printf(s, "\n    metadata_version: %x\n",
			hdr->dynamic_info[i].type);
		seq_printf(s, "        support_flags: %x\n",
			hdr->dynamic_info[i].support_flags);
		seq_puts(s, "        optional_fields:");
		if (hdr->dynamic_info[i].of_len >= 3)
			for (j = 0; j < (hdr->dynamic_info[i].of_len - 3); j++)
				seq_printf(s, " %x", hdr->dynamic_info[i].optional_fields[j]);
	}

	seq_printf(s, "\n\ncolorimetry_data: %x\n",
		hdr->colorimetry_support);
	if (cuva->ieeeoui == CUVA_IEEEOUI) {
		seq_puts(s, "CUVA supported: 1\n");
		seq_printf(s, "  system_start_code: %u\n", cuva->system_start_code);
		seq_printf(s, "  version_code: %u\n", cuva->version_code);
		seq_printf(s, "  display_maximum_luminance: %u\n", cuva->display_max_lum);
		seq_printf(s, "  display_minimum_luminance: %u\n", cuva->display_min_lum);
		seq_printf(s, "  monitor_mode_support: %u\n", cuva->monitor_mode_sup);
		seq_printf(s, "  rx_mode_support: %u\n", cuva->rx_mode_sup);
		for (i = 0; i < (cuva->length + 1); i++)
			seq_printf(s, "%02x", cuva->rawdata[i]);
		seq_puts(s, "\n");
	}
	/* sbtm capability show */
	if (sbtm->sbtm_support) {
		seq_puts(s, "SBTM supported: 1\n");
		if (sbtm->max_sbtm_ver)
			seq_printf(s, "  Max_SBTM_Ver: 0x%x\n",
				sbtm->max_sbtm_ver);
		if (sbtm->grdm_support)
			seq_printf(s, "  grdm_support: 0x%x\n",
				sbtm->grdm_support);
		if (sbtm->drdm_ind)
			seq_printf(s, "  drdm_ind: 0x%x\n",
				sbtm->drdm_ind);
		if (sbtm->hgig_cat_drdm_sel)
			seq_printf(s, "  hgig_cat_drdm_sel: 0x%x\n",
				sbtm->hgig_cat_drdm_sel);
		if (sbtm->use_hgig_drdm)
			seq_printf(s, "  use_hgig_drdm: 0x%x\n",
				sbtm->use_hgig_drdm);
		if (sbtm->maxrgb)
			seq_printf(s, "  maxrgb: 0x%x\n", sbtm->maxrgb);
		if (sbtm->gamut)
			seq_printf(s, "  gamut: 0x%x\n", sbtm->gamut);
		if (sbtm->red_x)
			seq_printf(s, "  red_x: 0x%x\n", sbtm->red_x);
		if (sbtm->red_y)
			seq_printf(s, "  red_y: 0x%x\n", sbtm->red_y);
		if (sbtm->green_x)
			seq_printf(s, "  green_x: 0x%x\n", sbtm->green_x);
		if (sbtm->green_y)
			seq_printf(s, "  green_y: 0x%x\n", sbtm->green_y);
		if (sbtm->blue_x)
			seq_printf(s, "  blue_x: 0x%x\n", sbtm->blue_x);
		if (sbtm->blue_y)
			seq_printf(s, "  blue_y: 0x%x\n", sbtm->blue_y);
		if (sbtm->white_x)
			seq_printf(s, "  white_x: 0x%x\n", sbtm->white_x);
		if (sbtm->white_y)
			seq_printf(s, "  white_y: 0x%x\n", sbtm->white_y);
		if (sbtm->min_bright_10)
			seq_printf(s, "  min_bright_10: 0x%x\n",
				sbtm->min_bright_10);
		if (sbtm->peak_bright_100)
			seq_printf(s, "  peak_bright_100: 0x%x\n",
				sbtm->peak_bright_100);
		if (sbtm->p0_exp)
			seq_printf(s, "  p0_exp: 0x%x\n", sbtm->p0_exp);
		if (sbtm->p0_mant)
			seq_printf(s, "  p0_mant: 0x%x\n", sbtm->p0_mant);
		if (sbtm->peak_bright_p0)
			seq_printf(s, "  peak_bright_p0: 0x%x\n", sbtm->peak_bright_p0);
		if (sbtm->p1_exp)
			seq_printf(s, "  p1_exp: 0x%x\n", sbtm->p1_exp);
		if (sbtm->p1_mant)
			seq_printf(s, "  p1_mant: 0x%x\n", sbtm->p1_mant);
		if (sbtm->peak_bright_p1)
			seq_printf(s, "  peak_bright_p1: 0x%x\n",
				sbtm->peak_bright_p1);
		if (sbtm->p2_exp)
			seq_printf(s, "  p2_exp: 0x%x\n", sbtm->p2_exp);
		if (sbtm->p2_mant)
			seq_printf(s, "  p2_mant: 0x%x\n", sbtm->p2_mant);
		if (sbtm->peak_bright_p2)
			seq_printf(s, "  peak_bright_p2: 0x%x\n", sbtm->peak_bright_p2);
		if (sbtm->p3_exp)
			seq_printf(s, "  p3_exp: 0x%x\n", sbtm->p3_exp);
		if (sbtm->p3_mant)
			seq_printf(s, "  p3_mant: 0x%x\n", sbtm->p3_mant);
		if (sbtm->peak_bright_p3)
			seq_printf(s, "  peak_bright_p3: 0x%x\n", sbtm->peak_bright_p3);
	}

	seq_puts(s, "\n******aud_cap******\n");
	meson_tx_show_aud_cap(prxcap, buf, len);
	seq_printf(s, buf);

	seq_printf(s, "content_cap: game(%d)/cinema(%d)/photo(%d)/graphics(%d), allm_cap: %d\n",
		prxcap->cnc3, prxcap->cnc2, prxcap->cnc1, prxcap->cnc0, prxcap->allm);
	kfree(buf);

	return 0;
}

void hdmitx_common_debugfs_init(struct hdmitx_common *tx_comm)
{
	struct dentry *entry;
	struct hdmitx_dbg_files_s *dbg_files;
	struct hdmitx_hw_common *hw_comm = NULL;
	int count = 0;
	int i;

	if (!tx_comm || !tx_comm->tx_hw || !tx_comm->tx_hw->chip_data ||
		!tx_comm->tx_hw->chip_data->hdmitx_ops) {
		HDMITX_ERROR("[%s]: invalid param\n", __func__);
		return;
	}
	hw_comm = tx_comm->tx_hw;
	dbg_files = hw_comm->chip_data->hdmitx_ops->get_dbg_files();
	count = hw_comm->chip_data->hdmitx_ops->get_dbg_files_count();
	if (!tx_comm->hdmitx_file_dbgfs)
		tx_comm->hdmitx_file_dbgfs = debugfs_create_dir(DEVICE_NAME, NULL);

	if (!tx_comm->hdmitx_file_dbgfs) {
		HDMITX_ERROR("can't create %s debugfs dir\n", DEVICE_NAME);
		return;
	}

	for (i = 0; i < count; i++) {
		entry = debugfs_create_file(dbg_files[i].name,
			dbg_files[i].mode,
			tx_comm->hdmitx_file_dbgfs, tx_comm,
			dbg_files[i].fops);
		if (!entry)
			HDMITX_ERROR("debugfs create file %s failed\n",
				dbg_files[i].name);
	}
}

void hdmitx_common_debugfs_destroy(struct hdmitx_common *tx_comm)
{
	if (!tx_comm->hdmitx_file_dbgfs)
		debugfs_remove(tx_comm->hdmitx_file_dbgfs);
	tx_comm->hdmitx_file_dbgfs = NULL;
}

void hdmitx_common_profs_init(struct hdmitx_common *tx_comm)
{
	struct proc_dir_entry *entry;
	struct hdmitx_dbg_files_s *dbg_files;
	struct hdmitx_hw_common *hw_comm = NULL;
	int count = 0;
	int i;

	if (!tx_comm || !tx_comm->tx_hw || !tx_comm->tx_hw->chip_data ||
		!tx_comm->tx_hw->chip_data->hdmitx_ops) {
		HDMITX_ERROR("[%s]: invalid param\n", __func__);
		return;
	}
	hw_comm = tx_comm->tx_hw;
	dbg_files = hw_comm->chip_data->hdmitx_ops->get_dbg_files();
	count = hw_comm->chip_data->hdmitx_ops->get_dbg_files_count();
	if (!tx_comm->hdmitx_proc_dbgfs)
		tx_comm->hdmitx_proc_dbgfs = proc_mkdir("amhdmitx", NULL);
	if (!tx_comm->hdmitx_proc_dbgfs)
		HDMITX_ERROR("Error creating proc entry");

	for (i = 0; i < count; i++) {
		entry = proc_create_data(dbg_files[i].name,
			dbg_files[i].mode,
			tx_comm->hdmitx_proc_dbgfs,
			dbg_files[i].pops, tx_comm);
		if (!entry)
			HDMITX_ERROR("debugfs create file %s failed\n",
				dbg_files[i].name);
	}
}

void hdmitx_common_profs_destroy(struct hdmitx_common *tx_comm)
{
	struct hdmitx_dbg_files_s *dbg_files;
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	int count = 0;
	int i;

	dbg_files = hw_comm->chip_data->hdmitx_ops->get_dbg_files();
	count = hw_comm->chip_data->hdmitx_ops->get_dbg_files_count();

	for (i = 0; i < count; i++)
		remove_proc_entry(dbg_files[i].name, tx_comm->hdmitx_proc_dbgfs);

	remove_proc_entry("amhdmitx", NULL);
}
