// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mutex.h>
#include <linux/reset.h>
#include <linux/amlogic/watch-key.h>

#define SARADC_REG0				0x00
#define SARADC_REG0_SAMPLING_STOP		BIT(14)
#define SARADC_REG0_ADC_EN			BIT(9)
#define SARADC_REG0_FIFO_CNT_IRQ_MASK		GENMASK(8, 4)
#define SARADC_REG0_FIFO_IRQ_EN			BIT(3)
#define SARADC_REG0_SAMPLE_START		BIT(2)
#define SARADC_REG0_CONTINUOUS_EN		BIT(1)
#define SARADC_REG0_SAMPLING_ENABLE		BIT(0)

#define SARADC_REG1				0x04
#define SARADC_REG1_MAX_INDEX_MASK		GENMASK(26, 24)
#define SARADC_REG1_ENTRY_SHIFT(_chan)		((_chan) * 3)
#define SARADC_REG1_ENTRY_MASK(_chan)		(GENMASK(2, 0) << ((_chan) * 3))

#define SARADC_REG2				0x08
#define SARADC_REG2_AVG_MODE_SHIFT(_chan)	(16 + ((_chan) * 2))
#define SARADC_REG2_AVG_MODE_MASK(_chan)	(GENMASK(17, 16) << ((_chan) * 2))
#define SARADC_REG2_NUM_SAMPLES_SHIFT(_chan)	(0 + ((_chan) * 2))
#define SARADC_REG2_NUM_SAMPLES_MASK(_chan)	(GENMASK(1, 0) << ((_chan) * 2))

#define SARADC_REG3				0x0c

#define SARADC_REG4				0x10
#define SARADC_REG4_ADC_RESET			BIT(25)		/* reset analog (a9 only) */

#define SARADC_REG5				0x14
#define SARADC_REG6				0x18
#define SARADC_REG7				0x1c
#define SARADC_REG8				0x20
#define SARADC_REG9				0x24
#define SARADC_REG10				0x28

#define SARADC_REG11				0x2c
#define SARADC_REG11_CHANNEL7_AUX_CTRL_MASK	GENMASK(27, 25)

#define SARADC_REG12				0x30
#define SARADC_REG13				0x34
#define SARADC_REG14				0x38

#define SARADC_STATUS0				0x3c
#define SARADC_STATUS0_BUSY_MASK		GENMASK(14, 12)
#define SARADC_STATUS0_FIFO_COUNT_MASK		GENMASK(7, 3)

#define SARADC_STATUS1				0x40
#define SARADC_STATUS2				0x44
#define SARADC_STATUS3				0x48
#define SARADC_STATUS4				0x4c
#define SARADC_STATUS5				0x50
#define SARADC_STATUS6				0x54
#define SARADC_STATUS7				0x58
#define SARADC_STATUS8				0x5c
#define SARADC_STATUS9				0x60
#define SARADC_STATUS10				0x64
#define SARADC_STATUS11				0x68
#define SARADC_STATUS12				0x6c
#define SARADC_STATUS13				0x70
#define SARADC_STATUS14				0x74
#define SARADC_STATUS15				0x78
#define SARADC_RDY				0x80

#define SARADC_CHANNEL_MAX			8
#define SARADC_MAX_FIFO_SIZE			16
#define SARADC_TIMEOUT				100	/* Millisecond */
#define SARADC_SOFT_AVG_NUM			8

#define SARADC_DEFAULT_CLOCK_FREQUENCY		1200000
#define SARADC_DEFAULT_TEST_CHANNEL		7
#define SARADC_DEFAULT_FIFO_DATA_WIDTH		16
#define SARADC_DEFAULT_VREF_VOLTAGE		1800000	/* Microvolt */

/* For IIO_VAL_FRACTIONAL use */
#define THOUSAND				1000

#define WORKAROUND_ID_NONE			0
#define WORKAROUND_ID_S7D			1
#define WORKAROUND_ID_A9			2

enum amlogic_saradc_avg_mode {
	NO_AVERAGING = 0x0,
	MEAN_AVERAGING = 0x1,
	MEDIAN_AVERAGING = 0x2,
};

enum amlogic_saradc_num_samples {
	ONE_SAMPLE = 0x0,
	TWO_SAMPLES = 0x1,
	FOUR_SAMPLES = 0x2,
	EIGHT_SAMPLES = 0x3,
};

struct amlogic_saradc_priv {
	struct iio_chan_spec iio_channels[SARADC_CHANNEL_MAX];
	struct regmap *regmap;
	struct clk *clk_src;
	struct clk *clk_core;
	struct clk *clk_gate;
	struct clk *clk_mux;
	u32 clock_frequency;
	u32 test_channel;
	u32 fifo_data_width;
	int info_offset;
	int info_scale;
	struct completion sample_done;
	/*
	 * This lock ensures that there is only one place to operate
	 * the controller at the same time.
	 */
	struct mutex lock;
	u8 workaround;
	struct reset_control *resets;
	bool hw_dis;
};

static const char * const test_voltage[] = {
	"gnd",
	"vdd/4",
	"vdd/2",
	"vdd*3/4",
	"vdd",
};

static void amlogic_saradc_set_averaging(struct amlogic_saradc_priv *priv, int address,
					 enum amlogic_saradc_avg_mode mode,
					 enum amlogic_saradc_num_samples samples)
{
	unsigned int val;

	val = samples << SARADC_REG2_NUM_SAMPLES_SHIFT(address);
	regmap_update_bits(priv->regmap, SARADC_REG2,
			   SARADC_REG2_NUM_SAMPLES_MASK(address), val);

	val = mode << SARADC_REG2_AVG_MODE_SHIFT(address);
	regmap_update_bits(priv->regmap, SARADC_REG2,
			   SARADC_REG2_AVG_MODE_MASK(address), val);
}

static void amlogic_saradc_enable_channel(struct amlogic_saradc_priv *priv,
					  int address, int index)
{
	u32 regval;

	regval = SARADC_REG1_ENTRY_MASK(index) & (address << (index * 3));
	regmap_update_bits(priv->regmap, SARADC_REG1,
			   SARADC_REG1_ENTRY_MASK(index), regval);
}

static int amlogic_saradc_wait_busy_clear(struct amlogic_saradc_priv *priv)
{
	u32 regval;
	int timeout = SARADC_TIMEOUT * 1000;

	do {
		usleep_range(1, 2);
		regmap_read(priv->regmap, SARADC_STATUS0, &regval);
	} while (FIELD_GET(SARADC_STATUS0_BUSY_MASK, regval) && timeout--);

	if (timeout < 0)
		return -ETIMEDOUT;

	return 0;
}

static void amlogic_saradc_clear_fifo(struct amlogic_saradc_priv *priv)
{
	unsigned int count, tmp;

	for (count = 0; count < SARADC_MAX_FIFO_SIZE; count++) {
		regmap_read(priv->regmap, SARADC_STATUS0, &tmp);
		count = FIELD_GET(SARADC_STATUS0_FIFO_COUNT_MASK, tmp);
		if (!count)
			break;

		regmap_read(priv->regmap, SARADC_STATUS3, &tmp);
	}
}

static int amlogic_saradc_read_fifo(struct amlogic_saradc_priv *priv, int *value)
{
	unsigned int count, tmp;

	if (!wait_for_completion_timeout(&priv->sample_done,
					 msecs_to_jiffies(SARADC_TIMEOUT)))
		return -ETIMEDOUT;

	regmap_read(priv->regmap, SARADC_STATUS0, &tmp);
	count = FIELD_GET(SARADC_STATUS0_FIFO_COUNT_MASK, tmp);
	if (count != 1)
		return -EINVAL;

	regmap_read(priv->regmap, SARADC_STATUS3, &tmp);

	*value = tmp & (BIT(priv->fifo_data_width) - 1);

	return 0;
}

static void amlogic_saradc_analog_reset(struct amlogic_saradc_priv *priv)
{
	switch (priv->workaround) {
	case WORKAROUND_ID_S7D:
		reset_control_reset(priv->resets);
		break;
	case WORKAROUND_ID_A9:
		regmap_update_bits(priv->regmap, SARADC_REG4, SARADC_REG4_ADC_RESET, 0);
		usleep_range(10, 20);
		regmap_update_bits(priv->regmap, SARADC_REG4, SARADC_REG4_ADC_RESET,
				   SARADC_REG4_ADC_RESET);
		break;
	}
}

static void amlogic_saradc_start_sample(struct amlogic_saradc_priv *priv)
{
	reinit_completion(&priv->sample_done);

	if (priv->workaround)
		usleep_range(50, 60);

	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_ADC_EN,
			   SARADC_REG0_ADC_EN);

	if (priv->workaround) {
		usleep_range(200, 210);
		amlogic_saradc_analog_reset(priv);
	}

	usleep_range(20, 30);

	regmap_update_bits_base(priv->regmap, SARADC_REG0, SARADC_REG0_SAMPLE_START,
				SARADC_REG0_SAMPLE_START, NULL, false, true);
}

static void amlogic_saradc_stop_sample(struct amlogic_saradc_priv *priv)
{
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_ADC_EN, 0);
}

static int amlogic_saradc_get_sample_once(struct iio_dev *indio_dev,
					  const struct iio_chan_spec *chan,
					  int *val, bool en_avg)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int ret;
	int raw;

	mutex_lock(&priv->lock);

	/* After the driver suspends, no controller operations are permitted. */
	if (priv->hw_dis) {
		mutex_unlock(&priv->lock);
		return -EINVAL;
	}

	/* Configure the averaging mode of the channels we use */
	amlogic_saradc_set_averaging(priv, chan->address,
				     en_avg ? MEDIAN_AVERAGING : NO_AVERAGING,
				     en_avg ? EIGHT_SAMPLES : ONE_SAMPLE);

	/* Set channel */
	amlogic_saradc_enable_channel(priv, chan->address, 0);

	amlogic_saradc_clear_fifo(priv);

	/* Sample */
	amlogic_saradc_start_sample(priv);
	ret = amlogic_saradc_read_fifo(priv, &raw);
	amlogic_saradc_stop_sample(priv);
	if (ret)
		goto fail;

	*val = (short)raw;

	mutex_unlock(&priv->lock);

	return IIO_VAL_INT;

fail:
	mutex_unlock(&priv->lock);
	return ret;
}

static int amlogic_saradc_get_sample(struct iio_dev *indio_dev,
				     const struct iio_chan_spec *chan,
				     int *val, bool en_avg)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int sum = 0;
	int tmp;
	int ret;
	int count;

	if (!priv->workaround || !en_avg)
		return amlogic_saradc_get_sample_once(indio_dev, chan, val, en_avg);

	for (count = 0; count < SARADC_SOFT_AVG_NUM; count++) {
		ret = amlogic_saradc_get_sample_once(indio_dev, chan, &tmp, false);
		if (ret < 0)
			return ret;
		sum += tmp;
	}

	*val = sum / SARADC_SOFT_AVG_NUM;

	return ret;
}

static int amlogic_saradc_get_sample_voltage(struct iio_dev *indio_dev,
					     const struct iio_chan_spec *chan,
					     int *val, bool en_avg)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int sample_value;
	int voltage;
	int ret;

	ret = amlogic_saradc_get_sample(indio_dev, chan, &sample_value, en_avg);
	if (ret < 0)
		return ret;

	/* Convert to voltage value (unit: millivolt) */
	voltage = sample_value + priv->info_offset;
	voltage = voltage > 0 ? voltage : 0;
	voltage = voltage * priv->info_scale;
	voltage = voltage < SARADC_DEFAULT_VREF_VOLTAGE ? voltage :
		  SARADC_DEFAULT_VREF_VOLTAGE;
	voltage /= THOUSAND;

	*val = voltage;

	return IIO_VAL_INT;
}

static int amlogic_saradc_iio_info_read_raw(struct iio_dev *indio_dev,
					    const struct iio_chan_spec *chan,
					    int *val, int *val2, long mask)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		return amlogic_saradc_get_sample(indio_dev, chan, val, false);

	case IIO_CHAN_INFO_AVERAGE_RAW:
		return amlogic_saradc_get_sample(indio_dev, chan, val, true);

	case IIO_CHAN_INFO_PROCESSED:
		return amlogic_saradc_get_sample_voltage(indio_dev, chan,
							 val, true);

	case IIO_CHAN_INFO_OFFSET:
		*val = priv->info_offset;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = priv->info_scale;
		*val2 = THOUSAND;
		return IIO_VAL_FRACTIONAL;
	}

	return -EINVAL;
}

static void amlogic_saradc_set_test_input(struct iio_dev *indio_dev, int sel)
{
	u32 regval;
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	regval = FIELD_PREP(SARADC_REG11_CHANNEL7_AUX_CTRL_MASK, sel);
	regmap_update_bits(priv->regmap, SARADC_REG11,
			   SARADC_REG11_CHANNEL7_AUX_CTRL_MASK, regval);
	usleep_range(10, 20);
}

static int amlogic_saradc_get_test_input(struct iio_dev *indio_dev)
{
	u32 regval;
	int sel;
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	regmap_read(priv->regmap, SARADC_REG11, &regval);
	sel = FIELD_GET(SARADC_REG11_CHANNEL7_AUX_CTRL_MASK, regval);

	return sel;
}

static int amlogic_saradc_self_calib(struct iio_dev *indio_dev)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int raw_gnd;
	int raw_vref;
	int ret;
	struct iio_chan_spec *chan = &priv->iio_channels[priv->test_channel];

	/* Set to GND */
	amlogic_saradc_set_test_input(indio_dev, 0);
	ret = amlogic_saradc_get_sample(indio_dev, chan, &raw_gnd, true);
	if (ret < 0) {
		dev_err(indio_dev->dev.parent, "self-calibration failed: sample error\n");
		return ret;
	}

	/* Set to VREF */
	amlogic_saradc_set_test_input(indio_dev, 4);
	ret = amlogic_saradc_get_sample(indio_dev, chan, &raw_vref, true);
	if (ret < 0) {
		dev_err(indio_dev->dev.parent, "self-calibration failed: sample error\n");
		return ret;
	}

	priv->info_offset = -raw_gnd;
	priv->info_scale = SARADC_DEFAULT_VREF_VOLTAGE / (raw_vref - raw_gnd);

	if (priv->info_scale <= 0) {
		dev_err(indio_dev->dev.parent, "self-calibration failed: gnd=%d, vref=%d\n",
			raw_gnd, raw_vref);
		return -EINVAL;
	}

	return 0;
}

static int amlogic_saradc_init(struct iio_dev *indio_dev)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	struct device_node *np = indio_dev->dev.parent->of_node;
	char buf[32];
	u32 reg_value;
	int index;
	int ret;

	ret = clk_set_parent(priv->clk_mux, priv->clk_src);
	if (ret) {
		dev_err(indio_dev->dev.parent,
			"failed to set mux parent to src\n");
		return ret;
	}

	ret = clk_set_rate(priv->clk_gate, priv->clock_frequency);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to set adc clock rate\n");
		return ret;
	}

	usleep_range(5, 10);

	/* Load register configuration from DT [REG0 - REG14] */
	for (index = 0; index <= 14; index++) {
		/* Generate attribute names */
		snprintf(buf, sizeof(buf), "amlogic,reg%d-init", index);
		/* If it exists, configure it to the register */
		ret = of_property_read_u32(np, buf, &reg_value);
		if (!ret)
			regmap_write(priv->regmap, index << 2, reg_value);
	}

	/* ADC is disabled by default */
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_FIFO_IRQ_EN, 0);
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_STOP,
			   SARADC_REG0_SAMPLING_STOP);
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_ENABLE, 0);
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_ADC_EN, 0);

	return 0;
}

static int amlogic_saradc_hw_enable(struct iio_dev *indio_dev)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int ret;
	u32 regval;

	mutex_lock(&priv->lock);

	ret = clk_prepare_enable(priv->clk_core);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to enable core clk\n");
		goto err_core;
	}

	ret = clk_prepare_enable(priv->clk_gate);
	if (ret) {
		dev_err(indio_dev->dev.parent, "failed to enable gate clk\n");
		goto err_gate;
	}

	usleep_range(5, 10);

	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_ENABLE,
			   SARADC_REG0_SAMPLING_ENABLE);

	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_STOP, 0);

	/* An interrupt will be generated when there is data in the FIFO */
	regval = FIELD_PREP(SARADC_REG0_FIFO_CNT_IRQ_MASK, 1);
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_FIFO_CNT_IRQ_MASK, regval);

	/* Enable FIFO interrupt */
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_FIFO_IRQ_EN,
			   SARADC_REG0_FIFO_IRQ_EN);

	amlogic_saradc_clear_fifo(priv);

	priv->hw_dis = false;

	mutex_unlock(&priv->lock);

	return 0;

err_gate:
	clk_disable_unprepare(priv->clk_core);
err_core:
	mutex_unlock(&priv->lock);
	return ret;
}

static int amlogic_saradc_hw_disable(struct iio_dev *indio_dev)
{
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	mutex_lock(&priv->lock);

	/* Disable FIFO interrupt */
	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_FIFO_IRQ_EN, 0);

	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_STOP,
			   SARADC_REG0_SAMPLING_STOP);
	amlogic_saradc_wait_busy_clear(priv);

	regmap_update_bits(priv->regmap, SARADC_REG0,
			   SARADC_REG0_SAMPLING_ENABLE, 0);

	clk_disable_unprepare(priv->clk_gate);
	clk_disable_unprepare(priv->clk_core);

	priv->hw_dis = true;

	mutex_unlock(&priv->lock);

	return 0;
}

static ssize_t test_input_show(struct device *dev, struct device_attribute *attr,
			       char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);
	int sel = amlogic_saradc_get_test_input(indio_dev);
	int len = 0;
	int index;

	len += sprintf(buf + len, "test channel: %d\n", priv->test_channel);
	len += sprintf(buf + len, "current: %d ", sel);
	if (sel < ARRAY_SIZE(test_voltage))
		len += sprintf(buf + len, "(%s)",  test_voltage[sel]);
	len += sprintf(buf + len, "\n\n");

	for (index = 0; index < ARRAY_SIZE(test_voltage); index++)
		len += sprintf(buf + len, "%d: %s\n", index, test_voltage[index]);

	return len;
}

static ssize_t test_input_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	int sel;

	if (kstrtoint(buf, 0, &sel) != 0)
		return -EINVAL;
	if (sel >= ARRAY_SIZE(test_voltage))
		return -EINVAL;

	amlogic_saradc_set_test_input(indio_dev, sel);

	return count;
}

static IIO_DEVICE_ATTR(test_input, 0644, test_input_show, test_input_store, -1);

static struct attribute *amlogic_saradc_attrs[] = {
	&iio_dev_attr_test_input.dev_attr.attr,
	NULL,
};

static const struct attribute_group amlogic_saradc_attr_group = {
	.attrs = amlogic_saradc_attrs,
};

static const struct iio_info amlogic_saradc_iio_info = {
	.read_raw = amlogic_saradc_iio_info_read_raw,
	.attrs = &amlogic_saradc_attr_group,
};

static const struct regmap_config amlogic_saradc_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static const struct of_device_id amlogic_saradc_of_match[] = {
	{ .compatible = "amlogic,saradc", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, amlogic_saradc_of_match);

static irqreturn_t amlogic_saradc_fifo_irq(int irq, void *data)
{
	struct iio_dev *indio_dev = data;
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	complete(&priv->sample_done);

	return IRQ_HANDLED;
}

static int amlogic_saradc_probe(struct platform_device *pdev)
{
	struct amlogic_saradc_priv *priv;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct iio_dev *indio_dev;
	void __iomem *base;
	int index;
	int ret;
	int fifo_irq;
	struct resource *res;
	struct amlogic_watchkey *wk;

	wk = devm_of_amlogic_watchkey_get(dev);
	if (!IS_ERR(wk)) {
		ret = amlogic_watchkey_disable(wk);
		dev_info(&pdev->dev, "watchkey exists, disable it: %s\n", ret ? "fail" : "done");
	} else if (PTR_ERR(wk) == -EPROBE_DEFER) {
		return PTR_ERR(wk);
	}

	indio_dev = devm_iio_device_alloc(dev, sizeof(*priv));
	if (!indio_dev)
		return dev_err_probe(dev, -ENOMEM, "failed allocating iio device\n");

	priv = iio_priv(indio_dev);
	init_completion(&priv->sample_done);

	indio_dev->name = dev_name(dev);
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->info = &amlogic_saradc_iio_info;

	/* initialize the structure for each channel */
	for (index = 0; index < SARADC_CHANNEL_MAX; index++) {
		priv->iio_channels[index].type = IIO_VOLTAGE;
		priv->iio_channels[index].indexed = 1;
		priv->iio_channels[index].channel = index;
		priv->iio_channels[index].address = index;
		priv->iio_channels[index].info_mask_separate =
					BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_AVERAGE_RAW) |
					BIT(IIO_CHAN_INFO_PROCESSED);
		priv->iio_channels[index].info_mask_shared_by_type =
					BIT(IIO_CHAN_INFO_SCALE) |
					BIT(IIO_CHAN_INFO_OFFSET);
	}
	indio_dev->channels = priv->iio_channels;
	indio_dev->num_channels = SARADC_CHANNEL_MAX;

	ret = of_property_read_u32(np, "amlogic,clock-frequency",
				   &priv->clock_frequency);
	if (ret)
		priv->clock_frequency = SARADC_DEFAULT_CLOCK_FREQUENCY;
	dev_dbg(dev, "clock-frequency: %u\n", priv->clock_frequency);

	ret = of_property_read_u32(np, "amlogic,test-channel",
				   &priv->test_channel);
	if (ret || priv->test_channel >= SARADC_CHANNEL_MAX)
		priv->test_channel = SARADC_DEFAULT_TEST_CHANNEL;
	dev_dbg(dev, "test-channel: %u\n", priv->test_channel);

	ret = of_property_read_u32(np, "amlogic,fifo-data-width",
				   &priv->fifo_data_width);
	if (ret)
		priv->fifo_data_width = SARADC_DEFAULT_FIFO_DATA_WIDTH;
	dev_dbg(dev, "fifo-data-width: %u\n", priv->fifo_data_width);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res) {
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base))
			return PTR_ERR(base);

		priv->regmap = devm_regmap_init_mmio(dev, base,
						     &amlogic_saradc_regmap_config);
		if (IS_ERR(priv->regmap))
			return PTR_ERR(priv->regmap);
	} else {
		priv->regmap = dev_get_regmap(dev->parent, NULL);
		if (!priv->regmap)
			return dev_err_probe(dev, -EINVAL, "Couldn't get parent's regmap\n");
	}

	fifo_irq = irq_of_parse_and_map(dev->of_node, 0);
	if (!fifo_irq)
		return dev_err_probe(dev, -EINVAL, "failed to get fifo irq\n");

	ret = devm_request_irq(dev, fifo_irq, amlogic_saradc_fifo_irq,
			       IRQF_SHARED, dev_name(dev), indio_dev);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request fifo irq\n");

	priv->clk_src = devm_clk_get(dev, "src");
	if (IS_ERR(priv->clk_src))
		return dev_err_probe(dev, PTR_ERR(priv->clk_src), "failed to get src clk\n");

	priv->clk_core = devm_clk_get(dev, "core");
	if (IS_ERR(priv->clk_core))
		return dev_err_probe(dev, PTR_ERR(priv->clk_core), "failed to get core clk\n");

	priv->clk_gate = devm_clk_get(dev, "gate");
	if (IS_ERR(priv->clk_gate)) {
		ret = PTR_ERR(priv->clk_gate);
		if (ret == -ENOENT)
			priv->clk_gate = NULL;
		else
			return dev_err_probe(dev, ret, "failed to get gate clk\n");
	}

	priv->clk_mux = devm_clk_get(dev, "mux");
	if (IS_ERR(priv->clk_mux)) {
		ret = PTR_ERR(priv->clk_mux);
		if (ret == -ENOENT)
			priv->clk_mux = NULL;
		else
			return dev_err_probe(dev, ret, "failed to get mux clk\n");
	}

	if (of_device_is_compatible(np, "amlogic,s7d-saradc")) {
		priv->resets = devm_reset_control_get(dev, "analog");
		if (IS_ERR(priv->resets))
			return dev_err_probe(dev, PTR_ERR(priv->resets), "failed to get resets\n");
		dev_info(dev, "Apply S7D workaround\n");
		priv->workaround = WORKAROUND_ID_S7D;
	} else if (of_device_is_compatible(np, "amlogic,a9-saradc")) {
		dev_info(dev, "Apply A9 workaround\n");
		priv->workaround = WORKAROUND_ID_A9;
	}

	ret = amlogic_saradc_init(indio_dev);
	if (ret)
		return ret;

	mutex_init(&priv->lock);

	ret = amlogic_saradc_hw_enable(indio_dev);
	if (ret)
		goto err;

	platform_set_drvdata(pdev, indio_dev);

	ret = amlogic_saradc_self_calib(indio_dev);
	if (ret)
		goto err_hw;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto err_hw;

	return 0;

err_hw:
	amlogic_saradc_hw_disable(indio_dev);
err:
	mutex_destroy(&priv->lock);
	return ret;
}

static void amlogic_saradc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct amlogic_saradc_priv *priv = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);

	amlogic_saradc_hw_disable(indio_dev);

	mutex_destroy(&priv->lock);
}

static int __maybe_unused amlogic_saradc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	return amlogic_saradc_hw_disable(indio_dev);
}

static int __maybe_unused amlogic_saradc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	return amlogic_saradc_hw_enable(indio_dev);
}

static SIMPLE_DEV_PM_OPS(amlogic_saradc_pm_ops,
			 amlogic_saradc_suspend, amlogic_saradc_resume);

static struct platform_driver amlogic_saradc_driver = {
	.probe		= amlogic_saradc_probe,
	.remove		= amlogic_saradc_remove,
	.driver		= {
		.name	= "amlogic-saradc",
		.of_match_table = amlogic_saradc_of_match,
		.pm = &amlogic_saradc_pm_ops,
	},
};

int __init amlogic_saradc_driver_init(void)
{
	return platform_driver_register(&amlogic_saradc_driver);
}

void __exit amlogic_saradc_driver_exit(void)
{
	platform_driver_unregister(&amlogic_saradc_driver);
}

MODULE_AUTHOR("Huqiang Qin <huqiang.qin@amlogic.com>");
MODULE_DESCRIPTION("Amlogic SAR ADC driver");
MODULE_LICENSE("GPL v2");
