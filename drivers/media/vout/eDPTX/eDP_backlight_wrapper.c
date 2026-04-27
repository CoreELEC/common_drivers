// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "dptx_reg_op.h"
#include "dptx_common.h"
#include <linux/delay.h>
#include <linux/backlight.h>

static int aml_eDP_bl_wrapper_get_brightness(struct backlight_device *bd)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)bl_get_data(bd);

	DPTX_PR(dptx, "%s: get brightness=%d", dptx->bl_wrapper_dev->props.brightness);

	return 60;
}

static int aml_eDP_bl_wrapper_update_status(struct backlight_device *bd)
{
	struct dptx_drv_s *dptx = (struct dptx_drv_s *)bl_get_data(bd);

	DPTX_PR(dptx, "%s: update brightness=%d", dptx->bl_wrapper_dev->props.brightness);

	return 0;
}

static const struct backlight_ops aml_eDPTX_bl_wrapper_ops = {
	.get_brightness = aml_eDP_bl_wrapper_get_brightness,
	.update_status  = aml_eDP_bl_wrapper_update_status,
};

void eDP_bl_wrapper_regist(struct dptx_drv_s *dptx)
{
	struct backlight_device *bldev;
	struct backlight_properties props;
	char bl_name[16];
	// int ret;

	sprintf(bl_name, "eDPTX-%c", 'A' + dptx->idx);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = 100;
	props.brightness = 80;

	bldev = devm_backlight_device_register(&dptx->pdev->dev, bl_name, &dptx->pdev->dev,
					  dptx, &aml_eDPTX_bl_wrapper_ops, &props);

	if (IS_ERR(bldev)) {
		DPTX_ERR(dptx, "failed to register backlight wrapper");
		// ret = PTR_ERR(bldev);
		// goto err;
	}

	dptx->bl_wrapper_dev = bldev;
}
