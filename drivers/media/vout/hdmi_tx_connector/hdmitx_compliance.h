/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_COMPLIANCE_H
#define __HDMITX_COMPLIANCE_H

bool hdmitx_find_vendor_6g(unsigned char *edid_buf);
bool hdmitx_find_vendor_ratio(unsigned char *edid_buf);
bool hdmitx_find_vendor_null_pkt(unsigned char *edid_buf);
bool hdmitx_find_vendor_phy_delay(unsigned char *edid_buf);
bool hdmitx_find_hdr_pkt_delay_to_vsync(unsigned char *edid_buf);
bool hdmitx_find_vendor_hdcp22_non_std(unsigned char *edid_buf);
bool hdmitx_find_vendor_audio_ddp_pop(unsigned char *edid_buf);
bool hdmitx_find_vendor_hdcp_delay(unsigned char *edid_buf);
bool hdmitx_find_vendor_audio_delay(unsigned char *edid_buf);
bool hdmitx_find_vendor_shield_hdr(unsigned char *edid_buf);
bool hdmitx_find_vendor_scan_non_std(unsigned char *edid_buf);
bool hdmitx_find_vendor_hdcp_ver_retry(unsigned char *edid_buf);
bool hdmitx_find_vendor_disable_delay(unsigned char *edid_buf);

#endif

