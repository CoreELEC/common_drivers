// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "edptx_reg_op.h"
#include "edptx_common.h"
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>

static int aml_eDP_bl_wrapper_get_brightness(struct backlight_device *bd)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)bl_get_data(bd);

	eDP_PR(edp, "%s: get brightness=%d", __func__, edp->bl_wrapper_dev->props.brightness);

	return 60;
}

static int aml_eDP_bl_wrapper_update_status(struct backlight_device *bd)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)bl_get_data(bd);

	eDP_PR(edp, "%s: update brightness=%d", __func__, edp->bl_wrapper_dev->props.brightness);

	return 0;
}

static const struct backlight_ops aml_edptx_bl_wrapper_ops = {
	.get_brightness = aml_eDP_bl_wrapper_get_brightness,
	.update_status  = aml_eDP_bl_wrapper_update_status,
};

void edptx_bl_wrapper_regist(struct edptx_drv_s *edp)
{
	struct backlight_device *bldev;
	struct backlight_properties props;
	char bl_name[16];
	// int ret;

	sprintf(bl_name, "eDPTX-%c", 'A' + edp->idx);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.power = FB_BLANK_UNBLANK; /* full on */
	props.max_brightness = 100;
	props.brightness = 80;

	bldev = devm_backlight_device_register(&edp->pdev->dev, bl_name, &edp->pdev->dev,
						edp, &aml_edptx_bl_wrapper_ops, &props);

	if (IS_ERR(bldev)) {
		eDP_ERR(edp, "failed to register backlight wrapper");
		// ret = PTR_ERR(bldev);
		// goto err;
	}

	edp->bl_wrapper_dev = bldev;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	if (edp->viu_sel && edp->setting.backlight_index < 0xff) {
		//lcd_resource_add(pdrv, LCD_RES_BACKLIGHT, pconf->backlight_index);
		//aml_bl_index_add(edp->idx,
		//	edp->vout_server.connector_type, edp->setting.backlight_index);
		aml_bl_index_add(edp->idx, 0x10 | edp->idx, edp->setting.backlight_index);
	}
#endif
}
