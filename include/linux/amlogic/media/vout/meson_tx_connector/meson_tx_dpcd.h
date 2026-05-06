/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef __MESON_TX_DPCD_H_
#define __MESON_TX_DPCD_H_

#include <linux/types.h>

#define DPTX_RECEIVER_CAP_SIZE                   0x100

struct dpcd_cap {
	/* DPCD version, not DP version. 10h: r1.0, ... 14h: r1.4 */
	u8 dpcd_ver;
	/* 0x06/0xa/0x14/0x1e for 1.62/2.7/5.4/8.1Gbps per lane */
	u8 max_link_rate;
	/* 4, 2, or 1 lanes */
	u8 max_lane_count;
};

#endif
