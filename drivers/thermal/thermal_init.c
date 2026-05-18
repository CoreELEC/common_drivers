// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "cpucore_cooling.h"
#include "ddr_control.h"
#include "thermal_init.h"
#include <linux/amlogic/meson_cooldev.h>
#include <linux/amlogic/vir_tsens.h>

static int __init amlogic_thermal_init(void)
{
	int ret;

	ret = platform_driver_register(&(meson_tsensor_driver));
	if (ret)
		return ret;

	ret = platform_driver_register(&(amlogic_thermal_driver));
	if (ret)
		return ret;

	platform_driver_register(&vir_tsens_platdrv);
	platform_driver_register(&ddr_control_platdrv);
	platform_driver_register(&cpucore_cooling_platdrv);
	return platform_driver_register(&(meson_cooldev_platdrv));
}

static __exit void amlogic_thermal_exit(void)
{
	platform_driver_unregister(&meson_cooldev_platdrv);
	platform_driver_unregister(&cpucore_cooling_platdrv);
	platform_driver_unregister(&ddr_control_platdrv);
	platform_driver_unregister(&vir_tsens_platdrv);

	platform_driver_unregister(&amlogic_thermal_driver);
	platform_driver_unregister(&meson_tsensor_driver);
}

module_init(amlogic_thermal_init);
module_exit(amlogic_thermal_exit);
