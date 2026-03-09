// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#define MESON_PIO_NAME "meson_pio"

#include <linux/amlogic/pio/meson_pio.h>

static struct pio_hw *s_pio;

struct pio_hw *pio_get_instance(u32 instance)
{
	return s_pio;
}

static struct regmap_config meson_pio_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static inline int meson_pio_mmio(struct platform_device *pdev, struct pio_hw *pio)
{
	resource_size_t size;
	const char *name = NULL;
	int ret = 0;

	pio->reg = devm_of_iomap(&pdev->dev, pdev->dev.of_node, 0, &size);
	if (IS_ERR(pio->reg))
		return PTR_ERR(pio->reg);

	ret = of_property_read_string_index(pdev->dev.of_node, "reg-names", 0, &name);
	if (ret)
		return ret;

	meson_pio_regmap_config.max_register = size - 4;
	meson_pio_regmap_config.name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "%pOFn-%s",
						      pdev->dev.of_node, name);
	if (!meson_pio_regmap_config.name)
		return -ENOMEM;

	pio->regmap = devm_regmap_init_mmio(&pdev->dev, pio->reg, &meson_pio_regmap_config);
	if (IS_ERR(pio->regmap))
		return PTR_ERR(pio->regmap);
	return 0;
}

static inline int meson_pio_irq_parser(struct device_node *node, struct pio_hw *pio)
{
	int ret = 0;
	int i;

	if (pio->version == 0) {
		for (i = 0; i < PIO_IRQ_NUM; i++) {
			ret = of_irq_get(node, i);
			if (ret < 0)
				return ret;
			pio->irq[i] = ret;
		}
	}

	return ret;
}

static inline int meson_pio_dma_init(struct pio_hw *pio, struct platform_device *pdev)
{
	char chan_name[4];
	int ret = 0;
	int i;
	struct pio_dma *dma;

	dma = devm_kmalloc_array(&pdev->dev, NUM_PIO_STATE_MACHINES * 2,
				 sizeof(struct pio_dma), GFP_KERNEL);
	if (!dma)
		return -ENOMEM;

	for (i = 0; i < NUM_PIO_STATE_MACHINES; i++) {
		pio->dma_tx[i] = &dma[i];
		pio->dma_rx[i] = &dma[NUM_PIO_STATE_MACHINES + i];

		sprintf(chan_name, "tx%d", i);
		pio->dma_tx[i]->chan = dma_request_chan(&pdev->dev, chan_name);
		if (IS_ERR(pio->dma_tx[i]->chan)) {
			ret = PTR_ERR(pio->dma_tx[i]->chan);
			dev_err(&pdev->dev, "can't request DMA tx channel %s, %d\n",
				chan_name, ret);
			ret = -ENOENT;
			goto rq_failed;
		}
		pio->dma_tx[i]->dma_transfer_dir = DMA_MEM_TO_DEV;
		pio->dma_tx[i]->dma_data_dir = DMA_TO_DEVICE;
		pio->dma_tx[i]->pio = pio;
		pio->dma_tx[i]->enable = true;
		init_completion(&pio->dma_tx[i]->dma_complete);

		sprintf(chan_name, "rx%d", i);
		pio->dma_rx[i]->chan = dma_request_chan(&pdev->dev, chan_name);
		if (IS_ERR(pio->dma_rx[i]->chan)) {
			ret = PTR_ERR(pio->dma_rx[i]->chan);
			dev_err(&pdev->dev, "can't request DMA rx channel %s,%d\n",
				chan_name, ret);
			ret = -ENOENT;
			goto rq_failed;
		}
		pio->dma_rx[i]->dma_transfer_dir = DMA_DEV_TO_MEM;
		pio->dma_rx[i]->dma_data_dir = DMA_FROM_DEVICE;
		pio->dma_rx[i]->pio = pio;
		pio->dma_rx[i]->enable = true;
		init_completion(&pio->dma_rx[i]->dma_complete);
	}

	pio->has_dma = true;
	return 0;

rq_failed:
	devm_kfree(&pdev->dev, dma);
	for (i = 0; i < NUM_PIO_STATE_MACHINES; i++) {
		if (pio->dma_rx[i])
			pio->dma_rx[i] = NULL;

		if (pio->dma_tx[i])
			pio->dma_tx[i] = NULL;
	}

	return ret;
}

static int meson_pio_probe(struct platform_device *pdev)
{
	struct pio_hw *pio;
	int ret = 0;
	u64 clk_rate;

	pio = devm_kzalloc(&pdev->dev, sizeof(*pio), GFP_KERNEL);
	if (!pio)
		return -ENOMEM;

	pio->clk = devm_clk_get(&pdev->dev, "pio_clk");
	if (IS_ERR(pio->clk)) {
		dev_err(&pdev->dev, "Can't find pio clk!\n");
		return PTR_ERR(pio->clk);
	}

	ret = clk_prepare_enable(pio->clk);
	if (ret) {
		dev_err(&pdev->dev, "clk enable failed\n");
		return ret;
	}

	clk_rate = clk_get_rate(pio->clk);
	pio->clk_rate = clk_rate;

	platform_set_drvdata(pdev, pio);
	pio->pdev = pdev;

	/* regmap mmio */
	ret = meson_pio_mmio(pdev, pio);
	if (ret)
		return ret;

	mutex_init(&pio->lock);

	ret = of_property_read_u8(pdev->dev.of_node, "amlogic,pio_version", &pio->version);
	if (ret < 0)
		pio->version = 0;

	ret = meson_pio_irq_parser(pdev->dev.of_node, pio);
	if (ret < 0)
		return ret;

	ret = of_property_read_bool(pdev->dev.of_node, "amlogic,support-dma");
	if (ret)
		if (meson_pio_dma_init(pio, pdev) < 0)
			pr_info("DMA request failed or not support !\n");

#if IS_ENABLED(CONFIG_AMLOGIC_PIO_TEST)
	ret = pio_class_register();
	if (ret)
		return ret;
#endif
	s_pio = pio;

	return 0;
}

static const struct of_device_id meson_pio_dt_match[] = {
	{
		.compatible = "amlogic,pio",
	},
	{}
};

static struct platform_driver meson_pio_driver = {
	.probe = meson_pio_probe,
	.driver = {
		.name = MESON_PIO_NAME,
		.of_match_table = meson_pio_dt_match,
	},
};

module_platform_driver(meson_pio_driver);
MODULE_LICENSE("GPL v2");
