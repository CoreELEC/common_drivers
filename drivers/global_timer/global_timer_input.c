// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/major.h>
#include <linux/list.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/gpio/consumer.h>
#include <linux/list.h>
#include <linux/regmap.h>
#include <linux/clk.h>
#include <linux/hrtimer.h>
#include <linux/cdev.h>
#include <linux/amlogic/glb_timer.h>
#include "main.h"

#define DRIVER_NAME	"global_timer_input"

static struct regmap_config meson_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.fast_io = true,
};

struct meson_glb_timer_input_pdata {
	void __iomem *reg;
	u32 first_irq_ranges[2];
	struct regmap *regmap;
	struct platform_device *pdev;
};

struct meson_glb_timer_input_pdata *glb_timer_input_padata;

/**
 * global_timer_input_gpio_configure
 *
 * @id: the input source id
 * @trigger_type: the trigger type, rising edge, falling edge
 * (can be found on include/linux/global_timer.h)
 * @returns: source id  on success or appropriate error code
 */
int meson_global_timer_input_gpio_configure(int virq,
					enum meson_glb_srcsel_flag trigger_type)
{
	struct regmap *regmap;
	u16 id;
	u32 hwirq;
	u32 start;
	u32 end;

	if (!glb_timer_input_padata) {
		pr_err("glb timer input driver no probe yet\n");
		return -ENODEV;
	}

	regmap = glb_timer_input_padata->regmap;
	start = glb_timer_input_padata->first_irq_ranges[0];
	end = glb_timer_input_padata->first_irq_ranges[1];

	id = gpio_irq_get_channel_idx(virq, &hwirq);
	if (id == 0xffff)
		return id;

	if (hwirq  < start || hwirq > end)
		id += end - start + 1;

	/* hold high bit when read low bit */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(30), BIT(30));

	/* set trigger method */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   GENMASK(17, 16),
			   (trigger_type & GENMASK(1, 0)) << 16);

	/* set if oneshot mode */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(29), ((trigger_type & BIT(2)) << 29));

	/* enable source */
	regmap_update_bits(regmap, SRC_OFFSET(TRIG_SRC0_CTRL0, id),
			   BIT(31), BIT(31));
	return id;
}
EXPORT_SYMBOL_GPL(meson_global_timer_input_gpio_configure);

/**
 * global_timer_input_gpio_get_snapshot
 *
 * @id: the input source id
 * @returns: the global timer 64 bit snapshot value
 */
u64 meson_global_timer_input_gpio_get_snapshot(int id)
{
	u32 ts_l;
	u32 ts_h;
	u64 ts;
	struct regmap *regmap;

	if (!glb_timer_input_padata) {
		pr_info("input driver not ready!\n");
		return 0;
	}

	regmap = glb_timer_input_padata->regmap;

	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_L, id), &ts_l);
	regmap_read(regmap, SRC_OFFSET(TRIG_SRC0_TS_H, id), &ts_h);

	ts = ts_h;

	return (ts << 32) | ts_l;
}
EXPORT_SYMBOL_GPL(meson_global_timer_input_gpio_get_snapshot);

static int meson_glb_timer_input_probe(struct platform_device *pdev)
{
	resource_size_t size;
	struct resource res;
	const char *name = NULL;
	struct meson_glb_timer_input_pdata *glb_timer_input;

	glb_timer_input = devm_kzalloc(&pdev->dev, sizeof(*glb_timer_input),
				       GFP_KERNEL);

	if (!glb_timer_input)
		return -ENOMEM;

	platform_set_drvdata(pdev, glb_timer_input);
	glb_timer_input->pdev = pdev;

	if (of_address_to_resource(pdev->dev.of_node, 0, &res))
		return -EINVAL;

	glb_timer_input->reg = devm_ioremap_resource(&pdev->dev, &res);
	if (IS_ERR(glb_timer_input->reg))
		return PTR_ERR(glb_timer_input->reg);

	size = resource_size(&res);

	of_property_read_string_index(pdev->dev.of_node, "reg-names", 0, &name);
	meson_regmap_config.max_register = size - 4;
	meson_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
						  "%pOFn-%s",
						  pdev->dev.of_node, name);
	if (!meson_regmap_config.name)
		return -ENOMEM;

	glb_timer_input->regmap = devm_regmap_init_mmio(&pdev->dev,
							glb_timer_input->reg,
							&meson_regmap_config);
	if (IS_ERR(glb_timer_input->regmap))
		return PTR_ERR(glb_timer_input->regmap);

	of_property_read_u32_array(pdev->dev.of_node, "first_irq_ranges",
				   glb_timer_input->first_irq_ranges, 2);

	glb_timer_input_padata = glb_timer_input;

	return 0;
}

static void meson_glb_timer_input_remove(struct platform_device *pdev)
{
}

static const struct of_device_id meson_glb_timer_input_dt_match[] = {
	{
		.compatible     = "amlogic,meson-glb-timer-gpio-input",
	},
	{}
};

static struct platform_driver meson_glb_timer_input_driver = {
	.probe = meson_glb_timer_input_probe,
	.remove = __exit_p(meson_glb_timer_input_remove),
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = meson_glb_timer_input_dt_match,
	},
};

module_platform_driver_only_define(meson_glb_timer_input_driver);
MODULE_LICENSE("GPL v2");
