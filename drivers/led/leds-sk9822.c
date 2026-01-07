// SPDX-License-Identifier: GPL-2.0-or-later
//
// Custom GPIO-based SK9822 RGB LED bit-banged SPI driver
//
// SPI-GPIO driver doesn't work because it is too fast (no delay on clock)
// We don't actually expose any SPI to userspace therefore
// having fat SPI and custom bit-banging is overkill
//
// Copyright (C) 2026 Team CoreELEC

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/sysfs.h>

#define MAX_SPEED_HZ 1000000 /* 1 MHz */
#define BITS_PER_WORD 8
#define NO_RGB_COLOR 0x00000000
#define MAX_BRIGHTNESS 9

struct sk9822 {
	struct device *dev;
	struct gpio_desc *gpio_sck;
	struct gpio_desc *gpio_mosi;
	u8 color_active[4]; /* brightness + RGB */
	u8 color_suspend[4];
	u8 color_shutdown[4];
};

/* to use bit-banging function from spi-bitbang-txrx.h directly
	 because we don't use linux kernel SPI the struct is not redefined here */
struct spi_device {
	struct sk9822 *sk;
};

#define SPI_CPOL _BITUL(1) /* clock polarity */

#define SPI_CONTROLLER_NO_RX BIT(1) /* can't do buffer read */
#define SPI_CONTROLLER_NO_TX BIT(2) /* can't do buffer write */

#define SPI_MASTER_NO_RX SPI_CONTROLLER_NO_RX
#define SPI_MASTER_NO_TX SPI_CONTROLLER_NO_TX

static inline void setsck(struct spi_device *spi, int is_on)
{
	struct sk9822 *sk = spi->sk;

	gpiod_set_value_cansleep(sk->gpio_sck, is_on);
}

static inline void setmosi(struct spi_device *spi, int is_on)
{
	struct sk9822 *sk = spi->sk;

	gpiod_set_value_cansleep(sk->gpio_mosi, is_on);
}

static inline int getmiso(struct spi_device *spi)
{
}

static inline void spidelay(unsigned int nsecs)
{
	ndelay(nsecs);
}

/* include function bitbang_txrx_be_cpha1() */
#include "../../../drivers/spi/spi-bitbang-txrx.h"

/* transfer function called for each spi message */
static void tx_spi_data(struct sk9822 *sk, u8 *tx_buf, unsigned int len)
{
	unsigned int nsecs = 1000000000 / (2 * MAX_SPEED_HZ); /* half period */
	unsigned cpol = SPI_CPOL;
	unsigned flags = SPI_MASTER_NO_RX;
	struct spi_device spi = { .sk = sk };
	int i;

	for (i = 0; i < len; i++) {
		bitbang_txrx_be_cpha1(&spi, nsecs, cpol, flags, tx_buf[i],
				      BITS_PER_WORD);
	}

	spidelay(nsecs); /* delay between transfers */
}

static void set_color(struct sk9822 *sk, u8 *color)
{
	u8 frames[9] = { 0x00 };

	/*
    0x00, 0x00, 0x00, 0x00,  0xe5, 0xff, 0x00, 0x00,  0x00
    ^                        ^       B     G     R    ^
    start frame              3b global + brightness   end frame

    using from userspace:
      echo "5 00 00 ff" >/sys/devices/platform/led/colors/active
  */

	frames[4] = 0xe0 | color[0]; /* global data + brightness */
	frames[5] = color[3]; /* B */
	frames[6] = color[2]; /* G */
	frames[7] = color[1]; /* R */

	tx_spi_data(sk, frames, sizeof(frames));
}

static ssize_t color_show(struct device *dev, struct device_attribute *attr,
			  char *buf);
static ssize_t color_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

static DEVICE_ATTR(active, 0644, color_show, color_store);
static DEVICE_ATTR(suspend, 0644, color_show, color_store);
static DEVICE_ATTR(shutdown, 0644, color_show, color_store);

static struct attribute *attrs[] = {
	&dev_attr_active.attr,
	&dev_attr_suspend.attr,
	&dev_attr_shutdown.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
	.name = "colors",
};

static const struct attribute_group *attr_groups[] = {
	&attr_group,
	NULL,
};

static void attr_to_color(struct sk9822 *sk, struct device_attribute *attr,
			  u8 **color)
{
	if (sysfs_streq(attr->attr.name, "active"))
		*color = sk->color_active;
	else if (sysfs_streq(attr->attr.name, "suspend"))
		*color = sk->color_suspend;
	else
		*color = sk->color_shutdown;
}

static ssize_t color_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct sk9822 *sk = dev_get_drvdata(dev);
	u8 *color;

	attr_to_color(sk, attr, &color);

	return sysfs_emit(buf, "%s color is %d %02x %02x %02x\n\n",
			  attr->attr.name, color[0], color[1], color[2],
			  color[3]);
}

static ssize_t color_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct sk9822 *sk = dev_get_drvdata(dev);
	u8 *color;
	u8 brightness;
	u8 red;
	u8 green;
	u8 blue;
	int ret;

	ret = sscanf(buf, "%d %02x %02x %02x", &brightness, &red, &green,
		     &blue);
	if (ret != 4) {
		dev_err(dev, "Wrong number of arguments\n");
		return -EINVAL;
	}

	/* adjust max brightness */
	if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;

	attr_to_color(sk, attr, &color);

	color[0] = brightness; /* brightness */
	color[1] = red; /* R */
	color[2] = green; /* G */
	color[3] = blue; /* B */

	dev_info(dev, "%s color stored as %d %02x %02x %02x\n", attr->attr.name,
		 brightness, red, green, blue);

	/* apply if active color */
	if (color == sk->color_active)
		set_color(sk, sk->color_active);

	return count;
}

static void parse_dt_color(struct device *dev, const char *name, u8 *dst,
			   size_t size)
{
	int ret;

	ret = of_property_read_u8_array(dev->of_node, name, dst, size);
	if (ret < 0) {
		dev_warn(dev, "Failed to get %s GPIO: %d\n", name, ret);
		memcpy(dst, NO_RGB_COLOR, size);
	}

	if (dst[0] > MAX_BRIGHTNESS)
		dst[0] = MAX_BRIGHTNESS;
}

static void parse_dt_gpio(struct device *dev, const char *name,
			  struct gpio_desc **gpio, enum gpiod_flags flags)
{
	*gpio = devm_gpiod_get(dev, name, flags);
	if (IS_ERR(*gpio))
		dev_err(dev, "Failed to get %s GPIO: %d\n", name,
			PTR_ERR(*gpio));
}

static int parse_dt(struct sk9822 *sk)
{
	/*
    SPI mode 3
    the clock starts with a high-level pulse (CPOL = 1)
    and data is sampled on the trailing (rising) edge of the clock signal (CPHA = 1)
  */
	parse_dt_gpio(sk->dev, "sck", &(sk->gpio_sck), GPIOD_OUT_HIGH);
	if (IS_ERR(sk->gpio_sck))
		return PTR_ERR(sk->gpio_sck);

	parse_dt_gpio(sk->dev, "mosi", &(sk->gpio_mosi), GPIOD_OUT_LOW);
	if (IS_ERR(sk->gpio_mosi))
		return PTR_ERR(sk->gpio_mosi);

	parse_dt_color(sk->dev, "active-color", sk->color_active,
		       sizeof(sk->color_active));
	parse_dt_color(sk->dev, "suspend-color", sk->color_suspend,
		       sizeof(sk->color_suspend));
	parse_dt_color(sk->dev, "shutdown-color", sk->color_shutdown,
		       sizeof(sk->color_shutdown));

	return 0;
}

static int probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sk9822 *sk;
	int ret;

	dev_info(dev, "Probing SK9822 SPI driver\n");

	sk = devm_kzalloc(dev, sizeof(*sk), GFP_KERNEL);
	if (!sk)
		return -ENOMEM;

	sk->dev = dev;
	platform_set_drvdata(pdev, sk);

	ret = parse_dt(sk);
	if (ret)
		return ret;

	set_color(sk, sk->color_active);

	dev_info(dev, "Probe ok\n");
	return 0;
}

static int remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sk9822 *sk = platform_get_drvdata(pdev);

	dev_info(dev, "Remove\n");
	set_color(sk, sk->color_shutdown);
	return 0;
}

static void shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sk9822 *sk = platform_get_drvdata(pdev);

	dev_info(dev, "Shutdown\n");
	set_color(sk, sk->color_shutdown);
}

static int suspend(struct device *dev)
{
	struct sk9822 *sk = dev_get_drvdata(dev);

	dev_info(dev, "Suspend\n");
	set_color(sk, sk->color_suspend);
	return 0;
}

static int resume(struct device *dev)
{
	struct sk9822 *sk = dev_get_drvdata(dev);

	dev_info(dev, "Resume\n");
	set_color(sk, sk->color_active);
	return 0;
}

static SIMPLE_DEV_PM_OPS(pm_ops, suspend, resume);

static const struct of_device_id dt_match[] = {
	{ .compatible = "amlogic,sk9822_led" },
	{}
};

MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver sk9822_driver = {
  .driver = {
    .name = "sk9822-spi",
    .owner = THIS_MODULE,
    .of_match_table = of_match_ptr(dt_match),
    .dev_groups = attr_groups,
    .pm = &pm_ops,
  },
  .probe = probe,
  .remove = remove,
  .shutdown = shutdown,
};

module_platform_driver(sk9822_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Team CoreELEC");
MODULE_DESCRIPTION("SK9822 SPI RGB LED driver");
