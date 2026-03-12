// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/typec.h>
#include <linux/usb/typec_dp.h>
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_mux.h>
#include <linux/amlogic/typec_mux_meson.h>

/* MUX control register */
#define MESON_REG_MUX		0x00
#define MESON_POLARITY_INVERTED		BIT(6)
#define MESON_UDP_SEL_MASK		GENMASK(8, 7)
#define MESON_USB		0x1
#define MESON_DP		0x2
#define MESON_DP_USB		0x3
#define MESON_USBCDP_ALT_MUX_MASK		GENMASK(5, 0)
#define MESON_USBCDP_ALT_POLARITY_NORMAL		0x20
#define MESON_USBCDP_ALT_POLARITY_INVERTED		0x10
#define MESON_USBCDP_ALT_POLARITY_DP4		0x38

struct typec_mux_meson_state {
	enum typec_orientation orient;
	unsigned long typec_pins_state;
	struct typec_displayport_data dp_state_data;
};

struct typec_mux_meson {
	struct device *dev;
	struct regmap *regmap;
	struct typec_switch_dev *sw;
	struct typec_mux_dev *mux;
	struct typec_mux_meson_state state;
	/* Prevent excessive notify. */
	unsigned long old_alt_mode;
};

static BLOCKING_NOTIFIER_HEAD(typec_mux_meson_notifier_list);

/**
 * typec_mux_meson_register_notify - register a notifier callback whenever a
 * typec_mux_meson change happens.
 * @nb: pointer to the notifier block for the callback events.
 *
 * These changes are typec devices being added or removed.
 */
void typec_mux_meson_register_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_register(&typec_mux_meson_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(typec_mux_meson_register_notify);

/**
 * typec_mux_meson_unregister_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * typec_mux_meson_register_notify() must have been previously called
 * for this function to work properly.
 */
void typec_mux_meson_unregister_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&typec_mux_meson_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(typec_mux_meson_unregister_notify);

static inline void typec_mux_meson_notify_orient(struct typec_mux_meson *cmux_meson)
{
	dev_dbg(cmux_meson->dev, "%s %d\n", __func__, cmux_meson->state.orient);

	blocking_notifier_call_chain(&typec_mux_meson_notifier_list, TYPEC_MESON_ORIENT,
			&cmux_meson->state.orient);
}

static inline void typec_mux_meson_notify_pin_state(struct typec_mux_meson *cmux_meson)
{
	dev_dbg(cmux_meson->dev, "%s %lu\n", __func__, cmux_meson->state.typec_pins_state);

	blocking_notifier_call_chain(&typec_mux_meson_notifier_list, TYPEC_MESON_PIN_STATE,
			&cmux_meson->state.typec_pins_state);
}

static inline void typec_mux_meson_notify_dp_state_data(struct typec_mux_meson *cmux_meson)
{
	dev_dbg(cmux_meson->dev, "%s status %x conf %x\n", __func__,
		cmux_meson->state.dp_state_data.status,
		cmux_meson->state.dp_state_data.conf);

	blocking_notifier_call_chain(&typec_mux_meson_notifier_list, TYPEC_MESON_DP_STATE_DATA,
			&cmux_meson->state.dp_state_data);
}

static int typec_meson_switch_set(struct typec_switch_dev *sw, enum typec_orientation orientation)
{
	struct typec_mux_meson *cmux_meson = typec_switch_get_drvdata(sw);

	dev_dbg(cmux_meson->dev, "%s %d\n", __func__, orientation);

	switch (orientation) {
	case TYPEC_ORIENTATION_NORMAL:
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_POLARITY_INVERTED, 0);
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_USBCDP_ALT_MUX_MASK, MESON_USBCDP_ALT_POLARITY_NORMAL);
		break;
	case TYPEC_ORIENTATION_REVERSE:
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_POLARITY_INVERTED, MESON_POLARITY_INVERTED);
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_USBCDP_ALT_MUX_MASK, MESON_USBCDP_ALT_POLARITY_INVERTED);
		break;
	case TYPEC_ORIENTATION_NONE:
		fallthrough;
	default:
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_POLARITY_INVERTED, 0);
		regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_USBCDP_ALT_MUX_MASK, 0);
		break;
	}

	cmux_meson->state.orient = orientation;
	typec_mux_meson_notify_orient(cmux_meson);

	return 0;
}

static int typec_meson_mux_set(struct typec_mux_dev *mux, struct typec_mux_state *state)
{
	struct typec_mux_meson *cmux_meson = typec_mux_get_drvdata(mux);
	unsigned int val;
	int ret;

	dev_dbg(cmux_meson->dev, "%s %lu\n", __func__, state->mode);

	if (state->mode >= TYPEC_STATE_MODAL && (!state->alt ||
	    state->alt->svid != USB_TYPEC_DP_SID))
		return -EINVAL;

	switch (state->mode) {
	case TYPEC_STATE_USB:
		val = FIELD_PREP(MESON_UDP_SEL_MASK, MESON_USB);
		cmux_meson->old_alt_mode = 0;
		break;
	case TYPEC_DP_STATE_C:
		fallthrough;
	case TYPEC_DP_STATE_E:
		val = FIELD_PREP(MESON_UDP_SEL_MASK, MESON_DP);
		break;
	case TYPEC_DP_STATE_D:
		val = FIELD_PREP(MESON_UDP_SEL_MASK, MESON_DP_USB);
		break;
	case TYPEC_STATE_SAFE:
		cmux_meson->old_alt_mode = 0;
		fallthrough;
	default:
		val = 0;
		break;
	}

	if (val == FIELD_PREP(MESON_UDP_SEL_MASK, MESON_DP)) {
		ret = regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				MESON_USBCDP_ALT_MUX_MASK, MESON_USBCDP_ALT_POLARITY_DP4);
		if (ret)
			goto err;
	}

	ret = regmap_update_bits(cmux_meson->regmap, MESON_REG_MUX,
				  MESON_UDP_SEL_MASK, val);
	if (ret)
		goto err;

	cmux_meson->state.typec_pins_state = state->mode;

	if (state->alt && state->alt->svid == USB_TYPEC_DP_SID) {
		struct typec_displayport_data *dp_sdata = state->data;

		if (state->mode >= TYPEC_DP_STATE_A &&
			state->mode <= TYPEC_DP_STATE_F &&
			cmux_meson->old_alt_mode != state->mode) {
			cmux_meson->old_alt_mode = state->mode;
			typec_mux_meson_notify_pin_state(cmux_meson);
		}

		if (dp_sdata) {
			memcpy(&cmux_meson->state.dp_state_data, dp_sdata, sizeof(*dp_sdata));
			/* DP_STATUS_IRQ_HPD DP_STATUS_HPD_STATE. */
			typec_mux_meson_notify_dp_state_data(cmux_meson);
			return 0;
		}
	}

err:
	return ret;
}

static const struct regmap_config typec_mux_meson_regmap = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = 0x1,
};

static int typec_mux_meson_probe(struct platform_device *pdev)
{
	struct typec_switch_desc sw_desc = { };
	struct typec_mux_desc mux_desc = { };
	struct device *dev = &pdev->dev;
	struct typec_mux_meson *cmux_meson;
	void __iomem *base;
	int ret;

	cmux_meson = devm_kzalloc(dev, sizeof(*cmux_meson), GFP_KERNEL);
	if (!cmux_meson)
		return -ENOMEM;

	cmux_meson->dev = dev;

	base = devm_platform_ioremap_resource(pdev, 0); //0xFE0086DC
	if (IS_ERR(base))
		return PTR_ERR(base);

	cmux_meson->regmap = devm_regmap_init_mmio(cmux_meson->dev,
					base,
					&typec_mux_meson_regmap);
	if (IS_ERR(cmux_meson->regmap))
		return dev_err_probe(dev, PTR_ERR(cmux_meson->regmap),
				     "Failed to init regmap\n");

	/* Initialize as USB mode with default (non-inverted) polarity */
	ret = regmap_write(cmux_meson->regmap, MESON_REG_MUX, MESON_USB);
	if (ret)
		return dev_err_probe(dev, ret, "Cannot set mode to USB\n");

	sw_desc.drvdata = cmux_meson;
	sw_desc.fwnode = dev_fwnode(dev);
	sw_desc.set = typec_meson_switch_set;

	cmux_meson->sw = typec_switch_register(dev, &sw_desc);
	if (IS_ERR(cmux_meson->sw))
		return dev_err_probe(dev, PTR_ERR(cmux_meson->sw),
				     "failed to register typec switch\n");

	mux_desc.drvdata = cmux_meson;
	mux_desc.fwnode = dev_fwnode(dev);
	mux_desc.set = typec_meson_mux_set;

	cmux_meson->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(cmux_meson->mux)) {
		typec_switch_unregister(cmux_meson->sw);
		return dev_err_probe(dev, PTR_ERR(cmux_meson->mux),
				     "failed to register typec mux\n");
	}

	platform_set_drvdata(pdev, cmux_meson);

	return 0;
}

static void typec_mux_meson_remove(struct platform_device *pdev)
{
	struct typec_mux_meson *cmux_meson = platform_get_drvdata(pdev);

	typec_mux_unregister(cmux_meson->mux);
	typec_switch_unregister(cmux_meson->sw);
}

#ifdef CONFIG_OF
static const struct of_device_id of_typec_mux_meson_match[] = {
	{
		.compatible = "amlogic,typec-mux-meson"
	},
	{
		.compatible = "amlogic, typec-mux-meson"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_typec_mux_meson_match);
#endif

static struct platform_driver typec_mux_meson_driver = {
	.probe		= typec_mux_meson_probe,
	.remove		= typec_mux_meson_remove,
	.driver		= {
		.name	= "typec-mux-meson",
		.of_match_table	= of_match_ptr(of_typec_mux_meson_match),
	},
};

int __init typec_mux_meson_init(void)
{
	return platform_driver_register(&typec_mux_meson_driver);
}

void __exit typec_mux_meson_exit(void)
{
	platform_driver_unregister(&typec_mux_meson_driver);
}

module_platform_driver(typec_mux_meson_driver);

MODULE_AUTHOR("Amlogic");
MODULE_DESCRIPTION("Meson Type-C USB alternate mode mux driver");
MODULE_LICENSE("GPL");
