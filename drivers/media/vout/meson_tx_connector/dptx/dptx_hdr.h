/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HDR_H
#define __HDMITX_HDR_H

#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/vinfo.h>

enum hdr_mode {
	SDR_WITH_BT709 = 0,
	HDR10_WITH_BT2020,
	HDR10_WITH_BT709,
	HLG_WITH_BT2020,
	SDR_WITH_BT2020,
	OTHER,
};

/* vout server api for vpp */
void dptx_enable_tx_packet(void *tx_instance, union tx_packet *data);
void dptx_disable_tx_packet(void *tx_instance, union tx_packet *data);
void dptx_set_drm_pkt(void *tx_instance, struct master_display_info_s *data);

#endif
