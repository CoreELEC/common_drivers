/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_VOUT_H
#define __HDMITX_VOUT_H

#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>

struct vinfo_s *hdmitx_get_current_vinfo(void *data);
void hdmitx_update_vinfo(struct hdmitx_common *tx_comm);
void hdmitx_reset_vinfo(struct vinfo_s *tx_vinfo);
void hdmitx_vout_init(struct hdmitx_common *tx_comm, struct hdmitx_hw_common *tx_hw);
void hdmitx_vout_uninit(struct hdmitx_common *tx_comm);
void hdmitx_update_vinfo_duration(struct hdmitx_common *tx_comm);

#endif
