// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/notifier.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include <linux/amlogic/media/vout/lcd/aml_bl.h>
#include <linux/amlogic/aml_spi.h>
#include "ldim_drv.h"
#include "ldim_dev_drv.h"
#include "../../lcd_reg.h"
#include "../../lcd_common.h"
#include "../lcd_bl.h"
#include "ldim_reg.h"

#include <linux/amlogic/gki_module.h>

static DEFINE_MUTEX(ldim_dev_dbg_mutex);
static struct delayed_work ldim_dev_probe_dly_work;
static unsigned int ldim_dev_probe_retry_cnt;

struct bl_gpio_s ldim_gpio[BL_GPIO_NUM_MAX] = {
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
	{.probe_flag = 0, .register_flag = 0,},
};

struct spi_private_data spi_priv[3];
static struct spicc_controller_data ldim_spi_controller_data = {
	.use_ctrl_cs = 0,
	.use_dirspi = 1,
	.ccxfer_en = 0,
	.timing_en = 1,
	.tx_tuning = 0,
	.rx_tuning = 7,
	.dummy_ctl = 0,
	.ss_leading_gap = 2,
	.ss_trailing_gap = 2,
	.word_gap = 0,/*1:with gap, 0: no word_gap,active when no rxbuf*/
	//.priv = &ldim_spi_priv,
};

struct spicc_controller_data spi_controller_data[3];
static struct spi_board_info ldim_spi_info = {
	.modalias = "ldim_dev",
	.mode = SPI_MODE_0,
	.max_speed_hz = 1000000, /* 1MHz */
	.bus_num = 0, /* SPI bus No. */
	.chip_select = 0, /* the cs pin index on the spi bus */
	//.controller_data = &ldim_spi_controller_data,
};

static int ldim_dev_probe_flag;

struct ldim_dev_driver_s ldim_dev_drv = {
	.index = 0xff,
	.type = LDIM_DEV_TYPE_NORMAL,
	.en_gpio = LCD_EXT_GPIO_INVALID,
	.en_gpio_on = 1,
	.en_gpio_off = 0,
	.lamp_err_gpio = LCD_EXT_GPIO_INVALID,
	.fault_check = 0,
	.write_check = 0,
	.pinmux_flag = 0xff,
	.chip_cnt = 0,

	.init_loaded = 0,
	.cmd_size = 4,
	.init_on = NULL,
	.init_off = NULL,
	.init_on_cnt = 0,
	.init_off_cnt = 0,
	.boundary_x = NULL,
	.boundary_y = NULL,

	.ldim_pwm_config = {
		.drv_index = 0,
		.pwm_method = BL_PWM_POSITIVE,
		.pwm_port = BL_PWM_MAX,
		.pwm_duty_max = 4095,
		.pwm_duty_min = 0,
	},
	.analog_pwm_config = {
		.drv_index = 0,
		.pwm_method = BL_PWM_POSITIVE,
		.pwm_port = BL_PWM_MAX,
		.pwm_freq = 1000,
		.pwm_duty_max = 4095,
		.pwm_duty_min = 10,
	},
	.boost_conf = {
		.en = 0,
		.mode = 0,
		.i_l100 = 0,
		.i_l32 = 0,
		.kp_l100 = 0,
		.kp_l32 = 0,
		.i_cur = 0,
		.kp_cur = 0,
		.apl = 0,
		.pre_apl = 0,
	},

	.bl_row = 1,
	.bl_col = 1,
	.dim_min = 0x7f, /* min 3% duty */
	.dim_max = 0xfff,
	.dim_adim = 0xfff,
	.dim_pdim = 0xfff,

	.zone_num = 1,
	.bl_mapping = NULL,

	.pin = NULL,
	.abcon_conf = {
		.tx_clk = 1000,
		.rx_clk = 1000,
		.dev_type = 0,
		.chip_num = {3, 0, 0, 0},
		.ch_num = 3,
		.dimming_mode = 4,
		.gpio_o = {0xf10, 0},
		.gpio_i = {0x2, 0},
		.idle_level = 0,
		.fb_en = 0,
		.fb_det_int = 300,
		.fb_adj_th = 5,
		.fb_pwm_dir = 1,
		.fb_pwm_step = 1,
		.ctrl = 0x2,
	},
	.spi_conf = {
		.trig_mode = SPI_TRIG_VSYNC,
		.rx_en = 0,
		.spi_dev_num = 1,
		.use_ctrl_cs = 1,
		.cs2clk_delay = 0,
		.clk2cs_delay = 0,
		.vs2cs_delay = 0,
	},

	.pinmux_ctrl = NULL,
	.dim_range_update = NULL,
	.reg_write = NULL,
	.reg_read = NULL,

	.power_on = NULL,
	.power_off = NULL,
	.dev_transmit = NULL,
	.dev_transmit_dummy = NULL,
	.config_print = NULL,
	.dev_transmit_start = NULL,
	.dev_transmit_stop = NULL,
};

void ldim_gpio_probe(struct ldim_dev_driver_s *dev_drv, int index)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag) {
		if (ldim_debug_print)
			LDIMPR("gpio %s[%d] is already registered\n", ld_gpio->name, index);
		return;
	}

	/* init gpio flag */
	ld_gpio->probe_flag = 1;
	ld_gpio->register_flag = 0;
}

static int ldim_gpio_register(struct ldim_dev_driver_s *dev_drv, int index, int init_value)
{
	struct bl_gpio_s *ld_gpio;
	int value;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("%s: gpio index %d, exit\n", __func__, index);
		return -1;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag) {
		if (ldim_debug_print) {
			LDIMPR("%s: gpio %s[%d] is already registered\n",
			       __func__, ld_gpio->name, index);
		}
		return 0;
	}

	switch (init_value) {
	case BL_GPIO_OUTPUT_LOW:
		value = GPIOD_OUT_LOW;
		break;
	case BL_GPIO_OUTPUT_HIGH:
		value = GPIOD_OUT_HIGH;
		break;
	case BL_GPIO_INPUT:
	default:
		value = GPIOD_IN;
		break;
	}

	/* request gpio */
	ld_gpio->gpio = devm_gpiod_get_index(dev_drv->dev, "ldim_dev", index, value);
	if (IS_ERR(ld_gpio->gpio)) {
		LDIMERR("register gpio %s[%d]: %p, err: %d\n",
			ld_gpio->name, index, ld_gpio->gpio,
			IS_ERR(ld_gpio->gpio));
		return -1;
	}
	ld_gpio->register_flag = 1;
	if (ldim_debug_print) {
		LDIMPR("register gpio %s[%d]: %p, init value: %d\n",
		       ld_gpio->name, index, ld_gpio->gpio, init_value);
	}

	return 0;
}

void ldim_gpio_set(struct ldim_dev_driver_s *dev_drv, int index, int value)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return;
	}
	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		BLERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return;
	}
	if (ld_gpio->register_flag == 0) {
		ldim_gpio_register(dev_drv, index, value);
		return;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index, ld_gpio->gpio,
			PTR_ERR(ld_gpio->gpio));
		return;
	}

	switch (value) {
	case BL_GPIO_OUTPUT_LOW:
	case BL_GPIO_OUTPUT_HIGH:
		gpiod_direction_output(ld_gpio->gpio, value);
		break;
	case BL_GPIO_INPUT:
	default:
		gpiod_direction_input(ld_gpio->gpio);
		break;
	}
	if (ldim_debug_print) {
		LDIMPR("set gpio %s[%d] value: %d\n",
		       ld_gpio->name, index, value);
	}
}

unsigned int ldim_gpio_get(struct ldim_dev_driver_s *dev_drv, int index)
{
	struct bl_gpio_s *ld_gpio;

	if (index >= BL_GPIO_NUM_MAX) {
		LDIMERR("gpio index %d, exit\n", index);
		return -1;
	}

	ld_gpio = &ldim_gpio[index];
	if (ld_gpio->probe_flag == 0) {
		LDIMERR("%s: gpio [%d] is not probed, exit\n", __func__, index);
		return -1;
	}
	if (ld_gpio->register_flag == 0) {
		LDIMERR("%s: gpio %s[%d] is not registered\n",
			__func__, ld_gpio->name, index);
		return -1;
	}
	if (IS_ERR_OR_NULL(ld_gpio->gpio)) {
		LDIMERR("gpio %s[%d]: %p, err: %ld\n",
			ld_gpio->name, index,
			ld_gpio->gpio, PTR_ERR(ld_gpio->gpio));
		return -1;
	}

	return gpiod_get_value(ld_gpio->gpio);
}

void ldim_set_duty_pwm(struct bl_pwm_config_s *bl_pwm)
{
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;
	if (bl_pwm->pwm_duty_max == 0)
		return;

	if (bl_pwm->pwm_duty < bl_pwm->pwm_duty_min ||
	    bl_pwm->pwm_duty > bl_pwm->pwm_duty_max) {
		LDIMERR("set_duty_pwm: pwm_port %d: duty %d out of range [%d~%d]\n",
			bl_pwm->pwm_port, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
		bl_pwm->pwm_duty = bl_pwm->pwm_duty_save;
		return;
	}

	if (ldim_debug_print & LDIM_DBG_PR_PWM) {
		LDIMPR("set_duty_pwm: port_port: %d: duty=%d [%d~%d]\n",
			bl_pwm->pwm_port, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
	}

	bl_pwm_ctrl(bl_pwm, 1);
}

void ldim_pwm_off(struct bl_pwm_config_s *bl_pwm)
{
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return;

	bl_pwm_ctrl(bl_pwm, 0);
}

/* ****************************************************** */

static int ldim_pwm_custom_pinmux_ctrl(struct ldim_dev_driver_s *dev_drv, int status)
{
	char pinmux_str[LDIM_DEV_NAME_MAX + 5];
	int ret = 0, index = 0xff;

	if (dev_drv->ldim_pwm_config.pwm_port >= BL_PWM_MAX)
		return 0;

	if (status) {
		index = 0;
		sprintf(pinmux_str, "%s", dev_drv->pinmux_name);
	} else {
		index = 1;
		sprintf(pinmux_str, "%s_off", dev_drv->pinmux_name);
	}
	if (dev_drv->pinmux_flag == index) {
		LDIMPR("pinmux %s is already selected\n", pinmux_str);
		return 0;
	}

	/* request pwm pinmux */
	dev_drv->pin = devm_pinctrl_get_select(dev_drv->dev, pinmux_str);
	if (IS_ERR_OR_NULL(dev_drv->pin)) {
		LDIMERR("set pinmux %s error\n", pinmux_str);
		ret = -1;
	} else {
		if (ldim_debug_print)
			LDIMPR("set pinmux %s: 0x%p\n", pinmux_str, dev_drv->pin);
	}
	dev_drv->pinmux_flag = index;

	return ret;
}

static char *ldim_pinmux_str[] = {
	"ldim_pwm",               /* 0 */
	"ldim_pwm_vs",            /* 1 */
	"ldim_pwm_combo",         /* 2 */
	"ldim_pwm_vs_combo",      /* 3 */
	"ldim_pwm_off",           /* 4 */
	"ldim_pwm_combo_off",     /* 5 */
	"ldim_bcon_up",				/* 6 */
	"ldim_bcon_dn",				/* 7 */
	"ldim_bcon_off",			/* 8 */
	"custom",
};

static int ldim_pwm_pinmux_ctrl(struct ldim_dev_driver_s *dev_drv, int status)
{
	struct bl_pwm_config_s *bl_pwm;
	char *str;
	int ret = 0, index = 0xff;

	if (dev_drv->ldim_pwm_config.pwm_port >= BL_PWM_MAX)
		return 0;

	if (strcmp(dev_drv->pinmux_name, "invalid"))
		return ldim_pwm_custom_pinmux_ctrl(dev_drv, status);

	if (status) {
		if (dev_drv->type == LDIM_DEV_TYPE_ABCON) {
			if (dev_drv->abcon_conf.idle_level)
				index = 6;
			else
				index = 7;
		} else {
			bl_pwm = &dev_drv->ldim_pwm_config;
			if (bl_pwm->pwm_port == BL_PWM_VS)
				index = 1;
			else
				index = 0;
			bl_pwm = &dev_drv->analog_pwm_config;
			if (bl_pwm->pwm_port < BL_PWM_VS)
				index += 2;
		}
	} else {
		if (dev_drv->type == LDIM_DEV_TYPE_ABCON) {
			index = 8;
		} else {
			bl_pwm = &dev_drv->analog_pwm_config;
			if (bl_pwm->pwm_port < BL_PWM_VS)
				index = 5;
			else
				index = 4;
		}
	}

	str = ldim_pinmux_str[index];
	if (dev_drv->pinmux_flag == index) {
		LDIMPR("pinmux %s is already selected\n", str);
		return 0;
	}

	/* request pwm pinmux */
	dev_drv->pin = devm_pinctrl_get_select(dev_drv->dev, str);
	if (IS_ERR_OR_NULL(dev_drv->pin)) {
		LDIMERR("set pinmux %s error\n", str);
		ret = -1;
	} else {
		if (ldim_debug_print)
			LDIMPR("set pinmux %s: 0x%p\n", str, dev_drv->pin);
	}
	dev_drv->pinmux_flag = index;

	return ret;
}

static int ldim_pwm_vs_update(struct aml_ldim_driver_s *ldim_drv)
{
	struct bl_pwm_config_s *bl_pwm = &ldim_drv->dev_drv->ldim_pwm_config;
	unsigned int cnt;
	int ret = 0;

	if (bl_pwm->pwm_port != BL_PWM_VS)
		return 0;

	if (ldim_debug_print & LDIM_DBG_PR_PWM)
		LDIMPR("%s\n", __func__);

	cnt = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT) + 1;
	if (cnt != bl_pwm->pwm_cnt) {
		if (ldim_drv->conf->vsize == 1080)
			bl_pwm->pwm_phase = ldim_drv->dev_drv->pwm_phase / 2;
		else
			bl_pwm->pwm_phase = ldim_drv->dev_drv->pwm_phase;
		bl_pwm_config_init(bl_pwm);
		ldim_set_duty_pwm(bl_pwm);
	}

	return ret;
}

#define EXT_LEN_MAX   500
static void ldim_dev_init_table_dynamic_print(struct ldim_dev_driver_s *dev_drv, int flag)
{
	int i, j, k, max_len;
	unsigned char cmd_size;
	char *str;
	unsigned char *table;

	str = kcalloc(EXT_LEN_MAX, sizeof(char), GFP_KERNEL);
	if (!str) {
		LDIMERR("%s: str malloc error\n", __func__);
		return;
	}
	if (flag) {
		pr_info("power on:\n");
		table = dev_drv->init_on;
		max_len = dev_drv->init_on_cnt;
	} else {
		pr_info("power off:\n");
		table = dev_drv->init_off;
		max_len = dev_drv->init_off_cnt;
	}
	if (max_len == 0) {
		kfree(str);
		return;
	}
	if (!table) {
		LDIMERR("init_table %d is NULL\n", flag);
		kfree(str);
		return;
	}

	i = 0;
	while ((i + 1) < max_len) {
		if (table[i] == LCD_EXT_CMD_TYPE_END) {
			pr_info("0x%02x,%d,\n", table[i], table[i + 1]);
			break;
		}
		cmd_size = table[i + 1];

		k = snprintf(str, EXT_LEN_MAX, "  0x%02x,%d,",
			     table[i], cmd_size);
		if (cmd_size == 0)
			goto init_table_dynamic_print_next;
		if (i + 2 + cmd_size > max_len) {
			pr_info("cmd_size out of support\n");
			break;
		}

		if (table[i] == LCD_EXT_CMD_TYPE_DELAY) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"%d,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD) {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
		} else if (table[i] == LCD_EXT_CMD_TYPE_CMD_DELAY) {
			for (j = 0; j < (cmd_size - 1); j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
			snprintf(str + k, EXT_LEN_MAX,
				 "%d,", table[i + cmd_size + 1]);
		} else {
			for (j = 0; j < cmd_size; j++) {
				k += snprintf(str + k, EXT_LEN_MAX,
					"0x%02x,", table[i + 2 + j]);
			}
		}
init_table_dynamic_print_next:
		pr_info("%s\n", str);
		i += (cmd_size + 2);
	}

	kfree(str);
}

static void ldim_dev_config_print(struct aml_ldim_driver_s *ldim_drv)
{
	struct bl_pwm_config_s *bl_pwm;
	struct pwm_state pstate;
	struct spi_device *spi0, *spi1;
	struct spicc_controller_data *cdata0, *cdata1;
	struct spi_private_data *priv0, *priv1;
	int i, n, len = 0;
	char *str = NULL;

	LDIMPR("%s:\n", __func__);

	pr_info("valid_flag        = %d\n"
		"vsync_change_flag = %d\n\n",
		ldim_drv->valid_flag,
		ldim_drv->vsync_change_flag);
	if (!ldim_drv->dev_drv) {
		LDIMERR("%s: dev_drv is null\n", __func__);
		return;
	}

	pr_info("dev_index     = %d\n"
		"dev_name      = %s\n"
		"key_valid     = %d\n"
		"type          = %d\n"
		"en_gpio       = %d\n"
		"en_gpio_on    = %d\n"
		"en_gpio_off   = %d\n"
		"chip_cnt      = %d\n"
		"dim_min       = 0x%03x\n"
		"dim_max       = 0x%03x\n"
		"zone_num      = %d\n",
		ldim_drv->dev_drv->index,
		ldim_drv->dev_drv->name,
		ldim_drv->dev_drv->key_valid,
		ldim_drv->dev_drv->type,
		ldim_drv->dev_drv->en_gpio,
		ldim_drv->dev_drv->en_gpio_on,
		ldim_drv->dev_drv->en_gpio_off,
		ldim_drv->dev_drv->chip_cnt,
		ldim_drv->dev_drv->dim_min,
		ldim_drv->dev_drv->dim_max,
		ldim_drv->dev_drv->zone_num);
	n = ldim_drv->dev_drv->zone_num;
	len = (n * 4) + 50;
	str = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!str) {
		pr_info("%s: buf malloc error\n", __func__);
	} else {
		len = sprintf(str, "zone_mapping:\n  ");
		for (i = 0; i < n; i++) {
			len += sprintf(str + len, "%d,",
				ldim_drv->dev_drv->bl_mapping[i]);
		}
		pr_info("%s\n\n", str);
		kfree(str);
	}

	switch (ldim_drv->dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		spi0 = ldim_drv->dev_drv->spi_conf.spi_dev[0];
		if (spi0) {
			cdata0 = spi0->controller_data;
			priv0 = cdata0->priv;
		}

		spi1 = ldim_drv->dev_drv->spi_conf.spi_dev[1];
		if (spi1) {
			cdata1 = spi1->controller_data;
			priv1 = cdata1->priv;
		}

		pr_info("spi_pointer      = 0x%p\n"
			"spi_modalias     = %s\n"
			"spi_mode         = %d\n"
			"spi_max_speed_hz = %d\n"
			"spi_bus_num      = %d : %d\n"
			"spi_chip_select  = %d : %d\n"
			"use_ctrl_cs      = %d\n"
			"cs2clk_delay     = %d\n"
			"clk2cs_delay     = %d\n"
			"lamp_err_gpio    = %d\n"
			"fault_check      = %d\n"
			"write_check      = %d\n"
			"trig_mode        = %d\n"
			"spi_mode_bit     = 0x%x\n"
			"spi_tx_dma       = 0x%lx : 0x%lx\n"
			"spi_rx_dma       = 0x%lx : 0x%lx\n"
			"spi_xlen         = %d : %d\n"
			"spi_line_n       = %d\n\n",
			ldim_drv->dev_drv->spi_conf.spi_dev,
			ldim_drv->dev_drv->spi_conf.spi_info[0].modalias,
			ldim_drv->dev_drv->spi_conf.spi_info[0].mode,
			ldim_drv->dev_drv->spi_conf.spi_info[0].max_speed_hz,
			ldim_drv->dev_drv->spi_conf.spi_info[0].bus_num,
			ldim_drv->dev_drv->spi_conf.spi_info[1].bus_num,
			ldim_drv->dev_drv->spi_conf.spi_info[0].chip_select,
			ldim_drv->dev_drv->spi_conf.spi_info[1].chip_select,
			ldim_drv->dev_drv->spi_conf.use_ctrl_cs,
			ldim_drv->dev_drv->spi_conf.cs2clk_delay,
			ldim_drv->dev_drv->spi_conf.clk2cs_delay,
			ldim_drv->dev_drv->lamp_err_gpio,
			ldim_drv->dev_drv->fault_check,
			ldim_drv->dev_drv->write_check,
			ldim_drv->dev_drv->spi_conf.trig_mode,
			(spi0) ? spi0->controller->mode_bits : 0,
			(ulong)((spi0 && cdata0 && priv0) ? priv0->tx_dma : 0),
			(ulong)((spi1 && cdata1 && priv1) ? priv1->tx_dma : 0),
			(ulong)((spi0 && cdata0 && priv0) ? priv0->rx_dma : 0),
			(ulong)((spi1 && cdata1 && priv1) ? priv1->rx_dma : 0),
			(spi0 && cdata0 && priv0) ? priv0->xlen : 0,
			(spi1 && cdata1 && priv1) ? priv1->xlen : 0,
			ldim_drv->dev_drv->spi_conf.vs2cs_delay);
		break;
	case LDIM_DEV_TYPE_I2C:
		break;
	case LDIM_DEV_TYPE_NORMAL:
	default:
		break;
	}
	bl_pwm = &ldim_drv->dev_drv->ldim_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		pr_info("ldim_pwm_port:   %d\n"
			"ldim_pwm_pol:    %d\n"
			"ldim_pwm_freq:   %d\n"
			"ldim_pwm_duty:   %d\n"
			"ldim_pwm_cnt:    %d\n"
			"ldim_pwm_value:  %d\n",
			bl_pwm->pwm_port, bl_pwm->pwm_method,
			bl_pwm->pwm_freq, bl_pwm->pwm_duty,
			bl_pwm->pwm_cnt, bl_pwm->pwm_value);
		switch (bl_pwm->pwm_port) {
		case BL_PWM_VS:
			pr_info("ldim_pwm_phase:  %d\n", bl_pwm->pwm_phase);
			str = kcalloc(1024, sizeof(char), GFP_KERNEL);
			if (str) {
				bl_pwm_vs_reg_dump(str, 0, 1024);
				pr_info("%s\n\n", str);
				kfree(str);
			}
			break;
		default:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				pr_info("ldim_pwm invalid\n");
				break;
			}
			pr_info("ldim_pwm_pointer:    0x%p\n", bl_pwm->pwm_data.pwm);
			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pr_info("ldim_pwm state:\n"
				"  period:        %lld\n"
				"  duty_cycle:    %lld\n"
				"  polarity:      %d\n"
				"  enabled:       %d\n",
				pstate.period, pstate.duty_cycle,
				pstate.polarity, pstate.enabled);
			break;
		}
	}
	bl_pwm = &ldim_drv->dev_drv->analog_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		pr_info("\nanalog_pwm_port:   %d\n"
			"analog_pwm_pol:    %d\n"
			"analog_pwm_freq:   %d\n"
			"analog_pwm_duty:   %d [%d~%d]\n"
			"analog_pwm_cnt:    %d\n"
			"analog_pwm_value:  %d\n",
			bl_pwm->pwm_port, bl_pwm->pwm_method,
			bl_pwm->pwm_freq, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max,
			bl_pwm->pwm_cnt, bl_pwm->pwm_value);
		switch (bl_pwm->pwm_port) {
		case BL_PWM_VS:
			pr_info("analog_pwm_phase:  %d\n", bl_pwm->pwm_phase);
			str = kcalloc(1024, sizeof(char), GFP_KERNEL);
			if (str) {
				bl_pwm_vs_reg_dump(str, 0, 1024);
				pr_info("%s\n\n", str);
				kfree(str);
			}
			break;
		default:
			if (IS_ERR_OR_NULL(bl_pwm->pwm_data.pwm)) {
				pr_info("analog_pwm invalid\n");
				break;
			}
			pr_info("analog_pwm_pointer:  0x%p\n", bl_pwm->pwm_data.pwm);
			pwm_get_state(bl_pwm->pwm_data.pwm, &pstate);
			pr_info("analog_pwm state:\n"
				"  period:     %lld\n"
				"  duty_cycle: %lld\n"
				"  polarity:   %d\n"
				"  enabled:    %d\n",
				pstate.period, pstate.duty_cycle,
				pstate.polarity, pstate.enabled);
			break;
		}
	}
	pr_info("\npinmux_flag:         %d\n"
		"pinmux_pointer:      0x%p\n\n",
		ldim_drv->dev_drv->pinmux_flag,
		ldim_drv->dev_drv->pin);

	if (ldim_drv->dev_drv->cmd_size > 0) {
		pr_info("table_loaded:    %d\n"
			"cmd_size:        %d\n"
			"init_on_cnt:     %d\n"
			"init_off_cnt:    %d\n",
			ldim_drv->dev_drv->init_loaded,
			ldim_drv->dev_drv->cmd_size,
			ldim_drv->dev_drv->init_on_cnt,
			ldim_drv->dev_drv->init_off_cnt);
		if (ldim_drv->dev_drv->cmd_size == LCD_EXT_CMD_SIZE_DYNAMIC) {
			ldim_dev_init_table_dynamic_print(ldim_drv->dev_drv, 1);
			ldim_dev_init_table_dynamic_print(ldim_drv->dev_drv, 0);
		}
	}
}

static ssize_t ldim_dev_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	int ret = 0;

	ldim_dev_config_print(ldim_drv);

	return ret;
}

static ssize_t ldim_dev_pwm_ldim_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;

	bl_pwm = &ldim_dev_drv.ldim_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		len += sprintf(buf + len,
			"ldim_pwm: freq=%d, phase=%d, pol=%d, duty=%d [%d~%d]\n",
			bl_pwm->pwm_freq, bl_pwm->pwm_phase, bl_pwm->pwm_method,
			bl_pwm->pwm_duty, bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
	}

	return len;
}

static void ldim_dev_pwm_debug(struct bl_pwm_config_s *bl_pwm, const char *buf, int dim_flag)
{
	unsigned int val = 0;
	int ret;

	switch (buf[0]) {
	case 'f': /* frequency */
		ret = sscanf(buf, "freq %d", &val);
		if (ret == 1) {
			bl_pwm->pwm_freq = val;
			bl_pwm_config_init(bl_pwm);
			ldim_set_duty_pwm(bl_pwm);
			if (ldim_debug_print) {
				LDIMPR("set ldim_pwm (port 0x%x): freq = %dHz\n",
				       bl_pwm->pwm_port, bl_pwm->pwm_freq);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
		break;
	case 'd': /* duty */
		ret = sscanf(buf, "duty %d", &val);
		if (ret == 1) {
			bl_pwm->pwm_duty = val;
			ldim_set_duty_pwm(bl_pwm);
			if (ldim_debug_print) {
				LDIMPR("set ldim_pwm (port 0x%x): duty = %d\n",
				       bl_pwm->pwm_port, bl_pwm->pwm_duty);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
		break;
	case 'p':
		if (buf[1] == 'o') { /* polarity */
			ret = sscanf(buf, "pol %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_method = val;
				bl_pwm_config_init(bl_pwm);
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): method = %d\n",
					       bl_pwm->pwm_port, bl_pwm->pwm_method);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		} else if (buf[1] == 'h') { /* phase */
			ret = sscanf(buf, "phase %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_phase = val;
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): phase = %d\n",
					       bl_pwm->pwm_port, bl_pwm->pwm_phase);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		}
		break;
	case 'm':
		if (buf[1] == 'a') { /* max */
			ret = sscanf(buf, "max %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_duty_max = val;
				if (ldim_dev_drv.dim_range_update && dim_flag)
					ldim_dev_drv.dim_range_update(&ldim_dev_drv);
				bl_pwm_config_init(bl_pwm);
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): duty_max = %d\n",
						bl_pwm->pwm_port, bl_pwm->pwm_duty_max);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		} else if (buf[1] == 'i') { /* min */
			ret = sscanf(buf, "min %d", &val);
			if (ret == 1) {
				bl_pwm->pwm_duty_min = val;
				if (ldim_dev_drv.dim_range_update && dim_flag)
					ldim_dev_drv.dim_range_update(&ldim_dev_drv);
				bl_pwm_config_init(bl_pwm);
				ldim_set_duty_pwm(bl_pwm);
				if (ldim_debug_print) {
					LDIMPR("set ldim_pwm (port 0x%x): duty_min = %d\n",
						bl_pwm->pwm_port, bl_pwm->pwm_duty_min);
				}
			} else {
				LDIMERR("invalid parameters\n");
			}
		}
		break;
	default:
		LDIMERR("wrong command\n");
		break;
	}
}

static ssize_t ldim_dev_pwm_ldim_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	struct bl_pwm_config_s *bl_pwm;

	bl_pwm = &ldim_dev_drv.ldim_pwm_config;
	if (bl_pwm->pwm_port >= BL_PWM_MAX)
		return count;

	ldim_dev_pwm_debug(bl_pwm, buf, 1);

	return count;
}

static ssize_t ldim_dev_pwm_analog_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct bl_pwm_config_s *bl_pwm;
	ssize_t len = 0;

	bl_pwm = &ldim_dev_drv.analog_pwm_config;
	if (bl_pwm->pwm_port < BL_PWM_MAX) {
		len += sprintf(buf + len,
			"analog_pwm: freq=%d, pol=%d, duty=%d [%d~%d]\n",
			bl_pwm->pwm_freq, bl_pwm->pwm_method, bl_pwm->pwm_duty,
			bl_pwm->pwm_duty_min, bl_pwm->pwm_duty_max);
	}

	return len;
}

static ssize_t ldim_dev_pwm_analog_store(const struct class *class,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	struct bl_pwm_config_s *bl_pwm;

	bl_pwm = &ldim_dev_drv.analog_pwm_config;
	if (bl_pwm->pwm_port >= BL_PWM_VS)
		return count;

	ldim_dev_pwm_debug(bl_pwm, buf, 0);

	return count;
}

struct ldim_dev_dbg_reg_s {
	unsigned char chip_id;
	unsigned char reg;
	unsigned int rd_cnt;
};

static struct ldim_dev_dbg_reg_s ldim_dev_dbg_reg;

static ssize_t ldim_dev_reg_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	unsigned char *data;
	ssize_t len = 0;
	int i, ret;

	if (!ldim_dev_drv.reg_read)
		return sprintf(buf, "ldim dev_drv reg_read is null\n");

	mutex_lock(&ldim_dev_dbg_mutex);
	if (ldim_dev_dbg_reg.rd_cnt == 0) {
		mutex_unlock(&ldim_dev_dbg_mutex);
		return sprintf(buf, "ldim_dev_dbg_reg rd_cnt is 0\n");
	}

	data = kcalloc(ldim_dev_dbg_reg.rd_cnt, sizeof(unsigned char), GFP_KERNEL);
	if (!data) {
		mutex_unlock(&ldim_dev_dbg_mutex);
		return sprintf(buf, "ldim data buff malloc failed\n");
	}
	ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
				    ldim_dev_dbg_reg.reg, data, ldim_dev_dbg_reg.rd_cnt);
	if (ret) {
		len = sprintf(buf, "reg[0x%02x] read error\n", ldim_dev_dbg_reg.reg);
	} else {
		len += sprintf(buf + len, "chip_id[%d] reg read:\n",
			       ldim_dev_dbg_reg.chip_id);
		for (i = 0; i < ldim_dev_dbg_reg.rd_cnt; i++) {
			len += sprintf(buf + len, "reg[0x%02x] = 0x%02x\n",
				       ldim_dev_dbg_reg.reg + i, data[i]);
		}
	}
	kfree(data);

	mutex_unlock(&ldim_dev_dbg_mutex);
	return len;
}

static ssize_t ldim_dev_reg_store(const struct class *class,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	unsigned int reg = 0, val = 0, id = 0;
	unsigned char data, *rbuf;
	unsigned int i, ret;

	mutex_lock(&ldim_dev_dbg_mutex);
	if (buf[0] == 'w') {
		ret = sscanf(buf, "w %x %x %x", &id, &reg, &val);
		if (ret == 3) {
			if (!ldim_dev_drv.reg_write) {
				LDIMERR("ldim dev_drv reg_write is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = 1;
			data = (unsigned char)val;
			ldim_dev_drv.reg_write(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
					       ldim_dev_dbg_reg.reg, &data, 1);
			LDIMPR("write chip_id[%d] reg[0x%02x] = 0x%02x\n",
			       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg, data);
		} else {
			LDIMERR("invalid parameters\n");
		}
	} else if (buf[0] == 'r') {
		ret = sscanf(buf, "r %x %x", &id, &reg);
		if (ret == 2) {
			if (!ldim_dev_drv.reg_read) {
				LDIMERR("ldim dev_drv reg_read is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = 1;
			ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
						    ldim_dev_dbg_reg.reg, &data, 1);
			if (ret) {
				LDIMPR("chip_id[%d] reg[0x%02x] read error\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg);
			} else {
				LDIMPR("chip_id[%d] reg[0x%02x] = 0x%02x\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg, data);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
	} else if (buf[0] == 'd') {
		ret = sscanf(buf, "d %x %x %d", &id, &reg, &val);
		if (ret == 3) {
			if (!ldim_dev_drv.reg_read) {
				LDIMERR("ldim dev_drv reg_read is null\n");
				goto ldim_dev_reg_store_end;
			}
			if (reg > 0xff) {
				LDIMERR("invalid reg address: 0x%x\n", reg);
				goto ldim_dev_reg_store_end;
			}
			ldim_dev_dbg_reg.chip_id = (unsigned char)id;
			ldim_dev_dbg_reg.reg = (unsigned char)reg;
			ldim_dev_dbg_reg.rd_cnt = val;

			rbuf = kcalloc(ldim_dev_dbg_reg.rd_cnt, sizeof(unsigned char), GFP_KERNEL);
			if (!rbuf)
				goto ldim_dev_reg_store_end;
			ret = ldim_dev_drv.reg_read(&ldim_dev_drv, ldim_dev_dbg_reg.chip_id,
						    ldim_dev_dbg_reg.reg, rbuf,
						    ldim_dev_dbg_reg.rd_cnt);
			if (ret) {
				LDIMERR("chip_id[%d] reg[0x%02x] read error\n",
				       ldim_dev_dbg_reg.chip_id, ldim_dev_dbg_reg.reg);
			} else {
				LDIMPR("chip_id[%d] reg dump:\n", ldim_dev_dbg_reg.chip_id);
				for (i = 0; i < ldim_dev_dbg_reg.rd_cnt; i++) {
					LDIMPR("reg[0x%02x] = 0x%02x\n",
					       ldim_dev_dbg_reg.reg, rbuf[i]);
				}
			}
			kfree(rbuf);
		} else {
			LDIMERR("invalid parameters\n");
		}
	}

ldim_dev_reg_store_end:
	mutex_unlock(&ldim_dev_dbg_mutex);
	return count;
}

static ssize_t ldim_dev_spi_store(const struct class *class, const struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned int ret;

	if (buf[0] == 't') {/*trig_mode*/
		ret = sscanf(buf, "t %d", &val);
		if (ret == 1) {
			if (val < SPI_TRIG_MAX) {
				ldim_dev_drv.spi_conf.trig_mode = val;
				LDIMPR("set trig_mode = %d\n", ldim_dev_drv.spi_conf.trig_mode);
			} else {
				LDIMPR("set trig_mode = %d exceed SPI_TRIG_MAX\n", val);
			}
		} else {
			LDIMERR("invalid parameters\n");
		}
	} else if (buf[0] == 'r') {/*start*/
		ret = sscanf(buf, "i %d", &val);
		if (ret == 1)
			ldim_spi_dma_trig_start_all();
		else
			LDIMERR("invalid parameters\n");
	} else if (buf[0] == 'e') {/*stop*/
		ret = sscanf(buf, "i %d", &val);
		if (ret == 1)
			ldim_spi_dma_trig_stop_all();
		else
			LDIMERR("invalid parameters\n");
	}

	return count;
}

static struct class_attribute ldim_dev_class_attrs[] = {
	__ATTR(status, 0644, ldim_dev_show, NULL),
	__ATTR(pwm_ldim, 0644, ldim_dev_pwm_ldim_show, ldim_dev_pwm_ldim_store),
	__ATTR(pwm_analog, 0644, ldim_dev_pwm_analog_show, ldim_dev_pwm_analog_store),
	__ATTR(reg, 0644, ldim_dev_reg_show, ldim_dev_reg_store),
	__ATTR(spi, 0644, NULL, ldim_dev_spi_store)
};

static void ldim_dev_class_create(struct ldim_dev_driver_s *dev_drv)
{
	int i;

	dev_drv->class = class_create("ldim_dev");
	if (IS_ERR_OR_NULL(dev_drv->class)) {
		LDIMERR("create ldim_dev class fail\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(ldim_dev_class_attrs); i++) {
		if (class_create_file(dev_drv->class, &ldim_dev_class_attrs[i])) {
			LDIMERR("create ldim_dev class attribute %s fail\n",
				ldim_dev_class_attrs[i].attr.name);
		}
	}
}

static int ldim_dev_add_driver(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = -1;

	switch (dev_drv->type) {
	case LDIM_DEV_TYPE_SPI:
		ret = ldim_spi_driver_add(dev_drv);
		break;
	case LDIM_DEV_TYPE_ABCON:
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
	ret = ldim_abcon_dev_probe(ldim_drv);
#endif
		break;
	case LDIM_DEV_TYPE_NORMAL:
	default:
		break;
	}
	if (ret)
		return ret;

	ret = -1;
	if (strcmp(dev_drv->name, "iw7027") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7027
		ret = ldim_dev_iw7027_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "iw7039") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7039
		ret = ldim_dev_iw7039_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "blmcu") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_BLMCU
		ret = ldim_dev_blmcu_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "ob3350") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_OB3350
		ret = ldim_dev_ob3350_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "abcon_spb") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
		ret = ldim_dev_abcon_spb_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "abcon_ospb") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
		ret = ldim_dev_abcon_ospb_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "abcon_xwire") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
		ret = ldim_dev_abcon_xwire_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "abcon_owc") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
		ret = ldim_dev_abcon_owc_probe(ldim_drv);
#endif
	} else if (strcmp(dev_drv->name, "global") == 0) {
		ret = ldim_dev_global_probe(ldim_drv);
	} else {
		LDIMERR("invalid device name: %s\n", dev_drv->name);
	}

	if (ret) {
		LDIMERR("add device driver failed: %s(%d)\n",
			dev_drv->name, dev_drv->index);
	} else {
		ldim_dev_probe_flag = 1;
		LDIMPR("add device driver: %s(%d)\n",
		       dev_drv->name, dev_drv->index);
	}

	return ret;
}

static int ldim_dev_remove_driver(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = -1;

	if (ldim_dev_probe_flag) {
		if (strcmp(dev_drv->name, "iw7027") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7027
			ret = ldim_dev_iw7027_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "iw7039") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_IW7039
			ret = ldim_dev_iw7039_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "blmcu") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_BLMCU
			ret = ldim_dev_blmcu_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "ob3350") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_OB3350
			ret = ldim_dev_ob3350_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "abcon_spb") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
			ret = ldim_dev_abcon_spb_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "abcon_ospb") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
			ret = ldim_dev_abcon_ospb_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "abcon_xwire") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
			ret = ldim_dev_abcon_xwire_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "abcon_owc") == 0) {
#ifdef CONFIG_AMLOGIC_BL_LDIM_ABCON
			ret = ldim_dev_abcon_owc_remove(ldim_drv);
#endif
		} else if (strcmp(dev_drv->name, "global") == 0) {
			ret = ldim_dev_global_remove(ldim_drv);
		} else {
			LDIMERR("invalid device name: %s\n", dev_drv->name);
		}

		if (ret) {
			LDIMERR("remove device driver failed: %s(%d)\n",
				dev_drv->name, dev_drv->index);
		} else {
			ldim_dev_probe_flag = 0;
			LDIMPR("remove device driver: %s(%d)\n",
				dev_drv->name, dev_drv->index);
		}
	}

	switch (ldim_dev_drv.type) {
	case LDIM_DEV_TYPE_SPI:
		ldim_spi_driver_remove(dev_drv);
		break;
	default:
		break;
	}

	return ret;
}

#define LDIM_DEV_PROBE_WAIT_TIMEOUT    16000
static void ldim_dev_probe_func(struct work_struct *work)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	struct spicc_controller_data *cdata;
	struct spi_private_data *priv;
	struct spi_conf_s *spi_conf = &ldim_dev_drv.spi_conf;
	unsigned int val, i;
	int ret = -1;

	if (ldim_drv->valid_flag == 0) {
		if (ldim_dev_probe_retry_cnt++ < LDIM_DEV_PROBE_WAIT_TIMEOUT) {
			lcd_queue_delayed_work(&ldim_dev_probe_dly_work, 10);
			return;
		}
	}
	if (ldim_drv->valid_flag == 0)
		return;

	if (!pdrv) {
		LDIMERR("%s: pdrv is null\n", __func__);
		return;
	}

	ldim_dev_drv.index = ldim_drv->conf->dev_index;
	if (ldim_dev_drv.index == 0xff) {
		if (ldim_debug_print)
			LDIMPR("%s: invalid index\n", __func__);
		return;
	}

	/* get configs */
	ldim_drv->dev_drv = &ldim_dev_drv;
	ldim_dev_drv.bl_row = ldim_drv->conf->seg_row;
	ldim_dev_drv.bl_col = ldim_drv->conf->seg_col;
	ldim_dev_drv.ldim_pwm_config.pwm_duty_max = 4095;
	ldim_dev_drv.analog_pwm_config.pwm_duty_max = 4095;
	val = ldim_dev_drv.bl_row * ldim_dev_drv.bl_col;
	ldim_dev_drv.zone_num = val;
	ldim_dev_drv.bl_mapping = kcalloc(val * 2, sizeof(unsigned short), GFP_KERNEL);
	if (!ldim_dev_drv.bl_mapping)
		goto ldim_dev_probe_func_fail0;

	/*preset spi_info*/
	for (i = 0; i < 3; i++) {
		memcpy(&spi_conf->spi_info[i],
			&ldim_spi_info, sizeof(struct spi_board_info));
		spi_conf->spi_info[i].controller_data = &spi_controller_data[i];
		cdata = spi_conf->spi_info[i].controller_data;
		memcpy(cdata, &ldim_spi_controller_data, sizeof(struct spicc_controller_data));
		cdata->priv = &spi_priv[i];
		priv = cdata->priv;
		priv->dev_idx = i;
		LDIMPR("%s: dev_idx:%d, cdata=0x%p, priv=0x%p\n", __func__,
		priv->dev_idx, cdata, priv);
	}

	ret = ldim_dev_get_config(&ldim_dev_drv, ldim_dev_drv.dev->of_node,
				  ldim_dev_drv.index);
	if (ret)
		goto ldim_dev_probe_func_fail1;

	if (ldim_dev_drv.boost_conf.mode) {
		ldim_dev_drv.boost_conf.iset = kcalloc(ldim_dev_drv.zone_num,
			sizeof(unsigned int), GFP_KERNEL);
		if (!ldim_dev_drv.boost_conf.iset)
			goto ldim_dev_probe_func_fail1;
		if (ldim_drv->fw && ldim_drv->fw->param)
			ldim_drv->fw->param->ext_boost = &ldim_dev_drv.boost_conf;
	}

	if (ldim_dev_drv.boundary_x) {
		if (ldim_drv->fw && ldim_drv->fw->param)
			ldim_drv->fw->param->conf->boundary_x = ldim_dev_drv.boundary_x;
		LDIMPR("%s: use boundary_x\n", __func__);
	}

	if (ldim_dev_drv.boundary_y) {
		if (ldim_drv->fw && ldim_drv->fw->param)
			ldim_drv->fw->param->conf->boundary_y = ldim_dev_drv.boundary_y;
		LDIMPR("%s: use boundary_y\n", __func__);
	}

	ldim_dev_drv.pwm_phase = ldim_dev_drv.ldim_pwm_config.pwm_phase;
	ldim_dev_drv.pinmux_ctrl = ldim_pwm_pinmux_ctrl;
	ldim_dev_drv.pwm_vs_update = ldim_pwm_vs_update;
	ldim_dev_drv.config_print = ldim_dev_config_print;

	ldim_drv->trig_param.spi_trig_line = spi_conf->vs2cs_delay;
	ldim_drv->trig_param.spi_tx_clk = spi_conf->spi_info[0].max_speed_hz;
	ldim_drv->trig_param.cs2clk_delay_ns = spi_conf->cs2clk_delay * 1000; //ns
	ldim_drv->trig_param.line_total_cnt = pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;
	ldim_drv->trig_param.line_time_ns = pdrv->curr_dev->dev_cfg.timing.act_timing.line_time_ns;

	ldim_dev_class_create(&ldim_dev_drv);
	ret = ldim_dev_add_driver(ldim_drv);
	if (ret)
		goto ldim_dev_probe_func_fail2;
	ldim_pwm_pinmux_ctrl(&ldim_dev_drv, 1);
	ldim_set_duty_pwm(&ldim_dev_drv.ldim_pwm_config);

	/*dma_alloc_coherent spi_tx_buf / spi_rx_buf*/
	if (ldim_dev_drv.type == LDIM_DEV_TYPE_SPI) {
		for (i = 0; i < spi_conf->spi_dev_num; i++) {
			cdata = spi_conf->spi_dev[i]->controller_data;
			priv = cdata->priv;
			priv->tx_buf =
			dma_alloc_coherent(spi_conf->spi_dev[i]->controller->dev.parent,
				priv->xlen, &priv->tx_dma, GFP_KERNEL | GFP_DMA);
			if (!priv->tx_buf) {
				LDIMERR("%s: priv->tx_buf %d is error\n", __func__, i);
				goto ldim_dev_probe_func_fail2;
			}
			if (spi_conf->rx_en) {
				priv->rx_buf =
				dma_alloc_coherent(spi_conf->spi_dev[i]->controller->dev.parent,
					priv->xlen, &priv->rx_dma, GFP_KERNEL | GFP_DMA);
				if (!priv->rx_buf) {
					LDIMERR("%s: priv->rx_buf %d is error\n", __func__, i);
					goto ldim_dev_probe_func_fail3;
				}
			} else {
				priv->rx_buf = NULL;
				priv->rx_dma = 0;
			}

			LDIMPR("%s spi_dev_idx:%d, tx_dma=0x%lx, rx_dma=0x%lx\n", __func__,
			i, (ulong)priv->tx_dma, (ulong)priv->rx_dma);
		}
		if (spi_conf->trig_mode == SPI_TRIG_LINE_N)
			ldim_wr_vcbus(VPP_INT_LINE_NUM, spi_conf->vs2cs_delay);
	}

	/* init ldim function */
	ldim_drv->init();

	LDIMPR("%s: ok\n", __func__);
	return;

ldim_dev_probe_func_fail3:
	kfree(priv->tx_buf);
ldim_dev_probe_func_fail2:
	kfree(ldim_dev_drv.boost_conf.iset);
ldim_dev_probe_func_fail1:
	kfree(ldim_dev_drv.bl_mapping);
ldim_dev_probe_func_fail0:
	pr_info("%s: failed\n", __func__);
}

static int ldim_dev_probe(struct platform_device *pdev)
{
	ldim_dev_probe_flag = 0;
	ldim_dev_drv.pdev = pdev;
	ldim_dev_drv.dev = &pdev->dev;
	/* set drvdata */
	platform_set_drvdata(pdev, &ldim_dev_drv);

	ldim_dev_probe_retry_cnt = 0;
	INIT_DELAYED_WORK(&ldim_dev_probe_dly_work, ldim_dev_probe_func);
	lcd_queue_delayed_work(&ldim_dev_probe_dly_work, 0);

	return 0;
}

static __maybe_unused void ldim_dev_remove(struct platform_device *pdev)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();

	cancel_delayed_work(&ldim_dev_probe_dly_work);
	if (ldim_dev_drv.index != 0xff)
		ldim_dev_remove_driver(ldim_drv);

	/* free drvdata */
	platform_set_drvdata(pdev, NULL);
	LDIMPR("%s\n", __func__);

	return;
}

static void ldim_dev_shutdown(struct platform_device *pdev)
{
	cancel_delayed_work(&ldim_dev_probe_dly_work);
}

#ifdef CONFIG_OF
static const struct of_device_id ldim_dev_dt_match[] = {
	{
		.compatible = "amlogic, ldim_dev",
	},
	{}
};
#endif

static struct platform_driver ldim_dev_platform_driver = {
	.driver = {
		.name  = "ldim_dev",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = ldim_dev_dt_match,
#endif
	},
	.probe   = ldim_dev_probe,
	.remove  = __exit_p(ldim_dev_remove),
	.shutdown = ldim_dev_shutdown,
};

int __init ldim_dev_init(void)
{
	if (platform_driver_register(&ldim_dev_platform_driver)) {
		LDIMPR("failed to register ldim_dev driver module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit ldim_dev_exit(void)
{
	platform_driver_unregister(&ldim_dev_platform_driver);
}

//MODULE_DESCRIPTION("LDIM device Driver for LCD Backlight");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Amlogic, Inc.");

