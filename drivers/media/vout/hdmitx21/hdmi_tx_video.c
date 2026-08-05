// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include "hdmi_tx_module.h"
#include "hdmi_tx.h"
#include "hw/common.h"

#define to_hdmitx21_dev(x)	container_of(x, struct hdmitx_dev, tx_comm)

static void hdmitx_set_spd_info(struct hdmitx_dev *hdmitx_device);

static void construct_avi_packet(struct hdmitx_dev *hdev)
{
	struct hdmi_avi_infoframe *info = &hdev->infoframes.avi.avi;
	struct hdmi_format_para *para = &hdev->tx_comm.fmt_para;
	enum hdmi_picture_aspect pic_ar = hdmitx_mode_get_vic_aspect_ratio(para->timing.vic);

	hdmi_avi_infoframe_init(info);
	info->version = 2;
	info->colorspace = para->cs;
	/* underscan */
	info->scan_mode = HDMI_SCAN_MODE_UNDERSCAN;
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
		if (!hdev->frl_rate)
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
	hdmi_avi_infoframe_set(info);
}

/************************************
 *	hdmitx protocol level interface
 *************************************/

/*
 * HDMI Identifier = HDMI_IEEE_OUI 0x000c03
 * If not, treated as a DVI Device
 */
static int is_dvi_device(struct rx_cap *prxcap)
{
	if (prxcap->ieeeoui != HDMI_IEEE_OUI)
		return 1;
	else
		return 0;
}

int hdmitx21_set_display(struct hdmitx_dev *hdev, enum hdmi_vic videocode)
{
	struct hdmi_format_para *param = &hdev->tx_comm.fmt_para;
	int ret = -1;

	if (videocode >= HDMITX_VESA_OFFSET) {
		param->cs = HDMI_COLORSPACE_RGB;
		param->cd = COLORDEPTH_24B;
		HDMITX_INFO("VESA only support RGB format\n");
	}

	if (hdev->tx_hw.base.setdispmode(&hdev->tx_hw.base) >= 0) {
		construct_avi_packet(hdev);

		/*
		 * HDMI CT 7-33 DVI Sink, no HDMI VSDB nor any
		 * other VSDB, No GB or DI expected
		 * TMDS_MODE[hdmi_config]
		 * 0: DVI Mode	   1: HDMI Mode
		 */
		if (is_dvi_device(&hdev->tx_comm.rxcap)) {
			HDMITX_INFO("Sink is DVI device\n");
			hdmitx_hw_cntl_config(&hdev->tx_hw.base,
				CONF_HDMI_DVI_MODE, DVI_MODE);
		} else {
			HDMITX_INFO("Sink is HDMI device\n");
			hdmitx_hw_cntl_config(&hdev->tx_hw.base,
				CONF_HDMI_DVI_MODE, HDMI_MODE);
		}
		if (videocode == HDMI_95_3840x2160p30_16x9 ||
		    videocode == HDMI_94_3840x2160p25_16x9 ||
		    videocode == HDMI_93_3840x2160p24_16x9 ||
		    videocode == HDMI_98_4096x2160p24_256x135) {
			if (!hdev->frl_rate)
				hdmitx_common_setup_vsif_packet(&hdev->tx_comm,
					VT_HDMI14_4K, 1, NULL);
		} else if ((!hdev->tx_comm.flag_3dfp) && (!hdev->tx_comm.flag_3dtb) &&
			 (!hdev->tx_comm.flag_3dss))
			/* For non-4kx2k mode setting */
			hdmitx_common_setup_vsif_packet(&hdev->tx_comm,
					VT_HDMI14_4K, 0, NULL);
		else
			;
		/* if TV support traditional SDR, then enable hdr.sdr packet by default */
		if (hdev->tx_comm.rxcap.hdr_info2.hdr_support & 0x1) {
			struct master_display_info_s data = {0};

			data.features = 0x00010100;
			hdev->tx_comm.vdev->fresh_tx_hdr_pkt(&data);
		}
		if (hdev->tx_comm.allm_mode) {
			hdmitx_common_setup_vsif_packet(&hdev->tx_comm, VT_ALLM, 1, NULL);
			hdmitx_hw_cntl_config(&hdev->tx_hw.base, CONF_CT_MODE, SET_CT_OFF);
		} else {
			hdmitx_hw_cntl_config(&hdev->tx_hw.base, CONF_CT_MODE,
				hdev->tx_comm.ct_mode | hdev->tx_comm.it_content << 4);
		}
		hdmitx_set_spd_info(hdev);
		ret = 0;
	}

	return ret;
}

/*
 * HDMI 1.4a/1.4b 3D signaling (HDMI Vendor Specific InfoFrame, PacketType
 * 0x81, IEEE OUI 0x000C03, HDMI_Video_Format = 3'b010) as required by
 * "High-Definition Multimedia Interface Specification Version 1.4a,
 * Extraction of 3D Signaling Portion", section 8.2.3 / Table 8-10..8-13.
 *
 * NOTE: this must NOT be routed through hdmi_vend_infoframe_rawset().
 * That helper decides between the HDMI_INFOFRAME_TYPE_VENDOR (classic
 * VSIF, packet buffer sel=5) and HDMI_INFOFRAME_TYPE_VENDOR2 (HF-VSIF,
 * packet buffer sel=8, see hw/hdmi_tx_pktmgmt.c) buffers purely based on
 * rxcap.ifdb_present / additional_vsif_num, i.e. it implements the Dolby
 * Vision CTS coexistence rules for *simultaneous* classic-VSIF + HF-VSIF
 * transmission (see the "dolby cts case89/92/93" comments in
 * hdmi_tx_infoframe.c). The InfoFrame Data Block (IFDB) that
 * ifdb_present is derived from is a CTA-861-G / HDMI 2.1 EDID data
 * block; legacy HDMI 1.4a 3D sinks (the class of device this packet is
 * actually for) never expose it, so rxcap.ifdb_present is always false
 * for them and every call used to fall into the "!ifdb_present" branch,
 * which places the packet into the HF-VSIF buffer instead of the
 * classic VSIF buffer that 3D-only sinks actually parse. Send the
 * legacy 3D VSIF unconditionally on the classic VSIF buffer instead,
 * exactly as hdmitx20/hdmi_tx_video.c::hdmi_set_3d() always does.
 */
int hdmi21_set_3d(struct hdmitx_dev *hdev, int type, u32 param)
{
	u8 body[31] = {0};
	u8 *ven_db = &body[4]; /* body[3] = PB0 (checksum), body[4] = PB1 */

	body[0] = 0x81; /* HB0: Packet Type */
	body[1] = 0x01; /* HB1: Version */
	body[2] = 0x6;  /* HB2: Length (Nv) */

	if (type == T3D_DISABLE) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
	} else {
		ven_db[0] = GET_OUI_BYTE0(HDMI_IEEE_OUI);
		ven_db[1] = GET_OUI_BYTE1(HDMI_IEEE_OUI);
		ven_db[2] = GET_OUI_BYTE2(HDMI_IEEE_OUI);
		ven_db[3] = 0x40;        /* PB4: HDMI_Video_Format = 3'b010 */
		ven_db[4] = type << 4;   /* PB5: 3D_Structure */
		ven_db[5] = param << 4;  /* PB6: 3D_Ext_Data */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
	}
	return 0;
}

/* Set Source Product Descriptor InfoFrame */
static void hdmitx_set_spd_info(struct hdmitx_dev *hdev)
{
	struct vendor_info_data *vend_data;
	struct hdmi_spd_infoframe *info = &hdev->infoframes.spd.spd;

	if (hdev->config_data.vend_data) {
		vend_data = hdev->config_data.vend_data;
	} else {
		HDMITX_DEBUG_VIDEO("packet: can\'t get vendor data\n");
		return;
	}

	hdmi_spd_infoframe_init(info,
		vend_data->vendor_name,
		vend_data->product_desc);
	hdmi_spd_infoframe_set(info);
}
