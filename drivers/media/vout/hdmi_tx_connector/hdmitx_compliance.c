// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>

/* Base Block, Vendor/Product Information, byte[8]~[17] */
struct edid_venddat_t {
	unsigned char data[10];
};

static struct edid_venddat_t vendor_6g[] = {
	/* SAMSUNG UA55KS7300JXXZ */
	{ {0x4c, 0x2d, 0x3b, 0x0d, 0x00, 0x06, 0x00, 0x01, 0x01, 0x1a} },
	/* DENON DENON AVR-X1400H */
	{ {0x11, 0xEE, 0x70, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x1F} },
	/* DENON DENON AVR-X2600H */
	{ {0x11, 0xEE, 0x65, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00, 0x1D} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_ratio[] = {
	/* Mi L55M2-AA */
	{ {0x61, 0xA4, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x19} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_null_pkt[] = {
	/* changhong LT32876 */
	{ {0x0d, 0x04, 0x21, 0x90, 0x01, 0x00, 0x00, 0x00, 0x20, 0x12} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_phy_delay[] = {
	/* WHALEY WTV55K1J */
	{ {0x5e, 0x96, 0x11, 0x55, 0x01, 0x00, 0x00, 0x00, 0x10, 0x1a} }
	/* Add new vendor data here */
};

static struct edid_venddat_t hdr_delay_id[] = {
	/* HUAWEI */
	{ {0x22, 0xf6, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x1d} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_hdcp22_non_std_tv[] = {
	/* Ko_gan KALED50XU9010SKA */
	{ {0x63, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x1B} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_audio_ddp_pop_tv[] = {
	/* Sony 65x8566e */
	{ {0x4d, 0xd9, 0x03, 0xf9, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1b} },
	{ {0x4d, 0xd9, 0x03, 0xf3, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1a} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_audio_delay[] = {
	/* TCL: 32L8H */
	{ {0x50, 0x6c, 0x20, 0x09, 0x01, 0x00, 0x00, 0x00, 0x14, 0x1c} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_hdcp_delay[] = {
	/* Mi L55M5-EC */
	{ {0x61, 0xa4, 0x4a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x1c} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_shield_hdr[] = {
	/* BLACKLINK */
	{ {0x63, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x21} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_scan_non_std[] = {
	/* LG OLED55BXPCA */
	{ {0x1e, 0x6d, 0xc0, 0xc0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1e} },
	/* SONY KD-75Z8H */
	{ {0x4d, 0xd9, 0x04, 0xdc, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1e} },
	/* SONY 65X8566E */
	{ {0x4d, 0xd9, 0x03, 0xf3, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1a} },
	/* SONY KD-55X9500H */
	{ {0x4d, 0xd9, 0x04, 0xa2, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1e} },
	/* SONY KD-55X9500G */
	{ {0x4d, 0xd9, 0x04, 0xa2, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1d} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_hdcp_ver_retry[] = {
	/* Samsung UA43TU8000JXXZ */
	{ {0x4c, 0x2d, 0x16, 0x70, 0x00, 0x0e, 0x00, 0x01, 0x01, 0x1e} },
	{ {0x4c, 0x2d, 0x17, 0x70, 0x00, 0x0e, 0x00, 0x01, 0x01, 0x1e} },
	/* LG OLED42C4PCA */
	{ {0x1e, 0x6d, 0xcd, 0x82, 0x01, 0x01, 0x01, 0x01, 0x01, 0x22} }
	/* Add new vendor data here */
};

static struct edid_venddat_t vendor_disable_delay[] = {
	/* TCL 50EP660 */
	{ {0x63, 0x18, 0x51, 0x28, 0x00, 0x00, 0x01, 0x00, 0x14, 0x1d} }
	/* Add new vendor data here */
};

/* HDMIPLL_CTRL3/4 under 4k50/60hz 6G mode should use the setting
 * witch is used under 4k59.94hz, specially for SAMSUNG UA55KS7300JXXZ
 * flash screen/no signal issue on SM1/SC2
 */
bool hdmitx_find_vendor_6g(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_6g); i++) {
		if (memcmp(&edid_buf[8], vendor_6g[i].data,
			   sizeof(vendor_6g[i].data)) == 0)
			return true;
	}
	return false;
}

/* need to forcely change clk ratio for such TV when suspend/resume box */
bool hdmitx_find_vendor_ratio(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_ratio); i++) {
		if (memcmp(&edid_buf[8], vendor_ratio[i].data,
			   sizeof(vendor_ratio[i].data)) == 0)
			return true;
	}
	return false;
}

/* need to forcely enable null packet for such TV */
bool hdmitx_find_vendor_null_pkt(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_null_pkt); i++) {
		if (memcmp(&edid_buf[8], vendor_null_pkt[i].data,
		    sizeof(vendor_null_pkt[i].data)) == 0)
			return true;
	}
	return false;
}

/* need add delay after disable phy */
bool hdmitx_find_vendor_phy_delay(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_phy_delay); i++) {
		if (memcmp(&edid_buf[8], vendor_phy_delay[i].data,
		    sizeof(vendor_phy_delay[i].data)) == 0)
			return true;
	}
	return false;
}

/* On Huawei TVs, HDR will cause a flickering screen,
 * and HDR PKT needs to be moved behind VSYNC on S7 or later SOCs
 */
bool hdmitx_find_hdr_pkt_delay_to_vsync(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(hdr_delay_id); i++) {
		if (memcmp(&edid_buf[8], hdr_delay_id[i].data,
			sizeof(hdr_delay_id[i].data)) == 0)
			return true;
	}
	return false;
}

/* hdcp2.2 non-standard TV, it's easily get hdp2.2 auth failed
 * when switch mode, need to mask its hdcp2.2 & 4k capability,
 * or use special workaround method to recover hdcp2.2 auth
 */
bool hdmitx_find_vendor_hdcp22_non_std(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_hdcp22_non_std_tv); i++) {
		if (memcmp(&edid_buf[8], vendor_hdcp22_non_std_tv[i].data,
		    sizeof(vendor_hdcp22_non_std_tv[i].data)) == 0)
			return true;
	}
	return false;
}

/* On sony 65x8566e TV,
 * when plugin there is video and no audio,
 * then the audio output, there maybe noise sound under ddp format
 */
bool hdmitx_find_vendor_audio_ddp_pop(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_audio_ddp_pop_tv); i++) {
		if (memcmp(&edid_buf[8], vendor_audio_ddp_pop_tv[i].data,
		    sizeof(vendor_audio_ddp_pop_tv[i].data)) == 0) {
			return true;
		}
	}
	return false;
}

/*
 * there will be no signal or display black screen when switch
 * from 4k24/25/30hz 444/rgb,8bit to 444/rgb,10/12bit, need to add
 * longer wait after mode set and before hdcp auth
 */
bool hdmitx_find_vendor_hdcp_delay(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;

	for (i = 0; i < ARRAY_SIZE(vendor_hdcp_delay); i++) {
		if (memcmp(&edid_buf[8], vendor_hdcp_delay[i].data,
		    sizeof(vendor_hdcp_delay[i].data)) == 0)
			return true;
	}
	return false;
}

/* On TCL-32L8H, need more time switch dd to pcm */
bool hdmitx_find_vendor_audio_delay(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;

	for (i = 0; i < ARRAY_SIZE(vendor_audio_delay); i++) {
		if (memcmp(&edid_buf[8], vendor_audio_delay[i].data,
		    sizeof(vendor_audio_delay[i].data)) == 0)
			return true;
	}
	return false;
}

/*
 * special TV to mask the HDR capabilities at 422,12bit.
 */
bool hdmitx_find_vendor_shield_hdr(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;

	for (i = 0; i < ARRAY_SIZE(vendor_shield_hdr); i++) {
		if (memcmp(&edid_buf[8], vendor_shield_hdr[i].data,
			sizeof(vendor_shield_hdr[i].data)) == 0)
			return true;
	}
	return false;
}

/* scan non-standard TV, it doesn't display full screen when detect avi scan_info =
 * under_scan, instead it displays with black board around. and need to set
 * avi scan_info = NO_DATA to recover normally.
 */
bool hdmitx_find_vendor_scan_non_std(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_scan_non_std); i++) {
		if (memcmp(&edid_buf[8], vendor_scan_non_std[i].data,
		    sizeof(vendor_scan_non_std[i].data)) == 0)
			return true;
	}
	return false;
}

/*
 * for special TV set, when read its hdcp version,
 * it will feedback hdcp2.2 not supported, but later
 * return hdcp2.2 supported. it will cause STB
 * do hdcp1.4 authentication instead of hdcp2.2.
 */
bool hdmitx_find_vendor_hdcp_ver_retry(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;

	for (i = 0; i < ARRAY_SIZE(vendor_hdcp_ver_retry); i++) {
		if (memcmp(&edid_buf[8], vendor_hdcp_ver_retry[i].data,
			sizeof(vendor_hdcp_ver_retry[i].data)) == 0)
			return true;
	}
	return false;
}

/* On TCL 50EP660 TV, need to add delay after disable output,
 * otherwise it may take long time to recover display after switch mode
 */
bool hdmitx_find_vendor_disable_delay(unsigned char *edid_buf)
{
	int i;

	if (!edid_buf)
		return false;
	for (i = 0; i < ARRAY_SIZE(vendor_disable_delay); i++) {
		if (memcmp(&edid_buf[8], vendor_disable_delay[i].data,
			sizeof(vendor_disable_delay[i].data)) == 0)
			return true;
	}
	return false;
}

