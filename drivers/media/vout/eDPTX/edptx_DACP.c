// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "edptx_common.h"

/* Display Authentication and Content Protection Support
 * Method 1 : HDCP
 * Method 2 : eDP panel limit
 * Method 3a: eDP Alternate Scrambler Seed Reset (ASSR)
 */

static void dptx_set_eDP_ASSR(struct edptx_drv_s *edp, u8 port)
{
	u8 aux_data = 0x01;

	edptx_if_scramble_reset_set(edp, port, DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET);
	edptx_if_aux_write(edp, port, DPCD_eDP_CONFIGURATION_SET, 1, &aux_data);

	eDP_P_DBG(edp, port, "eDP ASSR en");
}

void edptx_set_content_protection(struct edptx_drv_s *edp, u8 port)
{
	if (edp->sink.link[port].cap.DACP_support & 0x4)
		dptx_set_eDP_ASSR(edp, port);
}
