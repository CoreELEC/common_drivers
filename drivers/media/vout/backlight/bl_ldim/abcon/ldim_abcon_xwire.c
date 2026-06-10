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
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
// #include "../../../lcd_common.h"
#include "../ldim_drv.h"
#include "../ldim_dev_drv.h"
#include "ldim_abcon.h"
#include "ldim_abcon_reg.h"

#define BLMCU_CLASS_NAME "abcon_xwire"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

enum sop_type_e {
	sop_cmd = 0,
	sop_dimming,
	sop_readback,
	sop_max,
};

struct regmap_s {
	unsigned char dim_addr;
	unsigned char sample_time;
	unsigned char duty_data;
	unsigned char data_format;
	unsigned char read_dim_addr;
	unsigned char read_reg_addr;
};

//for BX7D871
struct regmap_s regmap_conf_871 = {
	.dim_addr = 0x04,
	.sample_time = 0x01,
	.duty_data = 0x12,
	.data_format = 0x02,
	.read_dim_addr = 0x16,
	.read_reg_addr = 0x15,
};

//for BX7D852
struct regmap_s regmap_conf_852 = {
	.dim_addr = 0x01,
	.sample_time = 0x12,
	.duty_data = 0x0f,
	.data_format = 0x1d,
	.read_dim_addr = 0x0e,
	.read_reg_addr = 0x02,
};

struct abcon_xwire_s {
	struct regmap_s *regmap;
	unsigned char dev_type;
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned char bl_pwm_en;	/*0:default uboot have pwm, 1: uboot without pwm*/
	unsigned char init_sampletime;

	unsigned char act_lane_num;
	unsigned int fb_cnt;
	unsigned int fb_high_cnt_th;
	unsigned char sop_type;
	unsigned char dataformat;

	unsigned char samplepoint;
};

struct abcon_xwire_s *bl_xwire;

struct xwire_packet_s {
	//unsigned int packet_header_len;
	//unsigned int packet_header[6];

	unsigned int cmd_header;
	unsigned int cmd_data;

	unsigned int addr_header;
	unsigned int addr_data;

	unsigned int data_header;
	unsigned int data_data;

	unsigned int eop_header;
	unsigned int eop_data;
};

struct xwire_packet_s xwire_packet;

void ldim_abcon_xwire_write_packet(void)
{
	unsigned int len = 0;
	unsigned int *buf;
	unsigned char i = 0;
	unsigned char act_lane_num = abcon->act_lane_num;

	if (bl_xwire->sop_type == sop_dimming)
		buf = abcon_mem.ldc_seg;
	else
		buf = abcon_mem.wseg;

	if (!buf)
		return;

	//for autotrans dimming
	if (bl_xwire->sop_type == sop_dimming) {
		if (abcon->chip_type == ABCON_CHIP_T6W) {
			buf[len++] = 0x10 | abcon->act_lane;
		} else {
			buf[len++] = 0xffffc000;
			buf[len++] = 0xfffe4000 | abcon->act_lane;
		}
		abcon->autotrans_ready = 1;
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = xwire_packet.cmd_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = xwire_packet.cmd_data;

	if (bl_xwire->sop_type == sop_dimming) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = xwire_packet.addr_header;

		for (i = 0; i < act_lane_num; i++)
			buf[len++] = xwire_packet.addr_data;

		for (i = 0; i < act_lane_num; i++)
			buf[len++] = xwire_packet.data_header;

		//for (i = 0; i < act_lane_num; i++)
		//	buf[len++] = xwire_packet.data_data;

		for (i = 0; i < act_lane_num; i++)
			buf[len++] = xwire_packet.eop_header;

		for (i = 0; i < act_lane_num; i++)
			buf[len++] = xwire_packet.eop_data;
	}

	if (bl_xwire->sop_type == sop_dimming) {
		if (abcon->chip_type == ABCON_CHIP_T6W)
			buf[0] |= (len - 1) << 19;

		abcon_wr_reg_bits(0x04, len, 16, 13);//reg_abcon_ldc_seg_max_num
		//abcon_wr_reg_bits(0x00, 1, 1, 1);//reg_abcon_ldc_mod_start
		//abcon_wr_reg_bits(0x00, 0, 1, 1);//reg_abcon_ldc_mod_start
	} else {
		abcon_wr_reg_bits(0x04, len, 0, 13);//reg_abcon_sw_wseg_max_num
		abcon_wr_reg_bits(0x00, 1, 0, 1);//reg_abcon_sw_mod_start
		abcon_wr_reg_bits(0x00, 0, 0, 1);//reg_abcon_sw_mod_start
	}
}

unsigned char ldim_abcon_xwire_readback(unsigned char reg, unsigned char dim, unsigned char lane)
{
	unsigned int readback_val = 0;
	unsigned int cmd_data = 0;

	if (!abcon_mem.rseg || !abcon_mem.wseg) {
		ABCONERR("%s, rseg or wseg is null!", __func__);
		return -1;
	}

	bl_xwire->sop_type = sop_readback;

	//step1, prepare read packet first
	abcon_mem.rseg[0] = 0x40000112;
	abcon_wr_reg_bits(0x05, 1, 16, 13);//reg_abcon_sw_rseg_max_num

	//step2, select read which lane
	abcon_wr_reg_bits(0x03, 1, 10, 4);//reg_abcon_act_lane_num

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, 1 << lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, 1 << lane, 0, 12);

	//step3, set write packet
	cmd_data = (5 << 14) | (bl_xwire->regmap->read_reg_addr << 9) | (reg << 1);
	abcon_mem.wseg[0] = 0x44000090;
	abcon_mem.wseg[1] = cmd_data;

	cmd_data = (4 << 14) | (bl_xwire->regmap->read_dim_addr << 9) | (dim << 1);
	abcon_mem.wseg[2] = 0x44000088;
	abcon_mem.wseg[3] = cmd_data;

	abcon_wr_reg_bits(0x04, 4, 0, 13);//reg_abcon_sw_wseg_max_num
	abcon_wr_reg_bits(0x00, 1, 0, 1);//reg_abcon_sw_mod_start
	abcon_wr_reg_bits(0x00, 0, 0, 1);//reg_abcon_sw_mod_start

	usleep_range(50, 100);
	readback_val = abcon_rd_reg(0x52);

	return readback_val;
}

void ldim_abcon_write_xwire_register(unsigned char regaddr, unsigned char value)
{
	unsigned int cmd_data = 0;

	bl_xwire->sop_type = sop_cmd;

	//sw reset
	//abcon_wr_reg_bits(0x00, 1, 5, 1);
	//abcon_wr_reg_bits(0x00, 0, 5, 1);

	//clr done flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	cmd_data = (5 << 14) | ((regaddr & 0x1f) << 9) | (value << 1);
	xwire_packet.cmd_header = 0x44000088;
	xwire_packet.cmd_data = cmd_data;

	ldim_abcon_xwire_write_packet();
	usleep_range(500, 1000);
}

void ldim_abcon_xwire_dimming(void)
{
	bl_xwire->sop_type = sop_dimming;
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	xwire_packet.cmd_header = 0x20000080;
	xwire_packet.cmd_data = 0xa0 | (bl_xwire->regmap->duty_data & 0x1f);

	xwire_packet.addr_header = 0x20000080;
	xwire_packet.addr_data = 0x01;

	xwire_packet.data_header = 0x1081 | (abcon->conf.ch_num << 14);
	//xwire_packet.data_data = value;

	xwire_packet.eop_header = 0x04000088;
	xwire_packet.eop_data = 0x00000000;

	ldim_abcon_xwire_write_packet();
}

void ldim_abcon_xwire_init_feedback(void)
{
	//todo
}

void ldim_abcon_xwire_get_feedback(void)
{
	//todo
}

void ldim_abcon_init_registers_for_xwire(void)
{
	//reg_abcon_clr_done_flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_mul_ldc_duty_num
	abcon_wr_reg_bits(0x02, abcon->max_lane_dim, 24, 8);

	//reg_abcon_pin_dir_level
	abcon_wr_reg_bits(0x05, 1, 30, 1);
	//reg_abcon_wire_mode
	abcon_wr_reg_bits(0x22, 0, 30, 2);
	//reg_abcon_xianxin_dim_addr_inc
	abcon_wr_reg_bits(0x2a, 1, 11, 1);
	//reg_abcon_xianxin_dim_addr_init
	abcon_wr_reg_bits(0x2a, 0, 0, 11);

	//reg_abcon_dim_addr_seg_pos
	abcon_wr_reg_bits(0x2c, 1, 1, 11);//according addr seg position
	//reg_abcon_dim_addr_auto_update
	abcon_wr_reg_bits(0x2c, 1, 0, 1);

	//reg_abcon_ldc_pkt_num_lane0
	abcon_wr_reg_bits(0x3b, abcon->conf.chip_num[0], 0, 8);
	//reg_abcon_ldc_pkt_num_lane1
	abcon_wr_reg_bits(0x3b, abcon->conf.chip_num[1], 8, 8);
	//reg_abcon_ldc_pkt_num_lane2
	abcon_wr_reg_bits(0x3b, abcon->conf.chip_num[2], 16, 8);
	//reg_abcon_ldc_pkt_num_lane3
	abcon_wr_reg_bits(0x3b, abcon->conf.chip_num[3], 24, 8);

	if (abcon->chip_type == ABCON_CHIP_DEF) {
		//reg_abcon_ldc_pkt_num_lane4
		abcon_wr_reg_bits(0x45, abcon->conf.chip_num[4], 0, 8);
		//reg_abcon_ldc_pkt_num_lane5
		abcon_wr_reg_bits(0x45, abcon->conf.chip_num[5], 8, 8);
		//reg_abcon_ldc_pkt_num_lane6
		abcon_wr_reg_bits(0x45, abcon->conf.chip_num[6], 16, 8);
		//reg_abcon_ldc_pkt_num_lane7
		abcon_wr_reg_bits(0x45, abcon->conf.chip_num[7], 24, 8);
		//reg_abcon_ldc_pkt_num_lane8
		abcon_wr_reg_bits(0x46, abcon->conf.chip_num[8], 0, 8);
		//reg_abcon_ldc_pkt_num_lane9
		abcon_wr_reg_bits(0x46, abcon->conf.chip_num[9], 8, 8);
		//reg_abcon_ldc_pkt_num_lane10
		abcon_wr_reg_bits(0x46, abcon->conf.chip_num[10], 16, 8);
		//reg_abcon_ldc_pkt_num_lane11
		abcon_wr_reg_bits(0x46, abcon->conf.chip_num[11], 24, 8);
	}

	//reg_abcon_rx_wait_tx
	abcon_wr_reg_bits(0x40, 1, 30, 1);

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	//reg_abcon_pin_dir_sel_time
	//abcon_wr_reg_bits(0x43, abcon->max_lane_dim * 9000 / 6, 0, 20);
	abcon_wr_reg_bits(0x43, 0xfffff, 0, 20);//set maximum, about 6ms
	//reg_abcon_pin_dir_del_start
	abcon_wr_reg_bits(0x00, 1, 8, 1);
	abcon_wr_reg_bits(0x00, 0, 8, 1);
	bl_delay_ms(8);

	abcon_wr_reg(0x1a, 0x7f000000);
	abcon_wr_reg(0x1b, 0x20c32940);//6dc+10pwm

	//init feedback registers
	ldim_abcon_xwire_init_feedback();
}

void ldim_abcon_init_xwire(void)
{
	char i;

	if (bl_xwire->dev_type) {//871
		if (bl_xwire->init_sampletime == 0) {
			//set txrx clk as 500Khz, due to dimmer default set is 500Khz
			ldim_abcon_set_txrx_clk(500, 500);
			//set sample point
			ldim_abcon_write_xwire_register(0x01, 0x0b);
			bl_xwire->init_sampletime = 1;
		}

		//set data format
		ldim_abcon_write_xwire_register(0x02, 0xa4);

		//command set
		ldim_abcon_write_xwire_register(0x03, 0x01);

		//send address command
		for (i = 0; i < abcon->max_lane_dim; i++)
			ldim_abcon_write_xwire_register(bl_xwire->regmap->dim_addr,
			1 + i);

		//set sample point
		//ldim_abcon_write_xwire_register(0x01, 0x0f);//0x0b
		//set txrx clk
		//ldim_abcon_set_txrx_clk(abcon->conf.tx_clk, abcon->conf.rx_clk);
	} else {//852
		if (bl_xwire->init_sampletime == 0) {
			//set txrx clk as 500Khz, due to dimmer default set is 500Khz
			ldim_abcon_set_txrx_clk(330, 330);
			//set sample point
			ldim_abcon_write_xwire_register(0x12, 0x17);
			bl_xwire->init_sampletime = 1;
		}

		//set data format
		ldim_abcon_write_xwire_register(0x1d, 0xa5);

		//set pwm en
		ldim_abcon_write_xwire_register(0x03, 0x0f);

		//command set
		ldim_abcon_write_xwire_register(0x00, 0x01);

		//send address command
		for (i = 0; i < abcon->max_lane_dim; i++)
			ldim_abcon_write_xwire_register(bl_xwire->regmap->dim_addr,
			1 + i);

		//set sample point
		//ldim_abcon_write_xwire_register(0x01, 0x08);
		//set txrx clk
		//ldim_abcon_set_txrx_clk(abcon->conf.tx_clk, abcon->conf.rx_clk);
	}

	ABCONPR("abcon init xwire done!\n");
}

static int abcon_xwire_hw_init_on(struct ldim_dev_driver_s *dev_drv)
{
	/* step 1: system power_on */
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_on);

	/* step 2: delay for internal logic stable */
	bl_delay_ms(dev_drv->hw_on_delay);

	/* step 3: Generate external VSYNC to VSYNC/PWM pin */
	ldim_set_duty_pwm(&dev_drv->ldim_pwm_config);
	ldim_set_duty_pwm(&dev_drv->analog_pwm_config);
	dev_drv->pinmux_ctrl(dev_drv, 1);

	ldim_abcon_swrst();

	bl_delay_ms(10);

	//init abcon common
	ldim_abcon_init_common_registers(abcon);

	//init abcon register for xwire
	ldim_abcon_init_registers_for_xwire();

	//init xwire registers
	ldim_abcon_init_xwire();

	bl_delay_ms(2);
	//ldim_abcon_swrst();

	ABCONPR("abcon hw init on done!\n");
	return 0;
}

static int abcon_xwire_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static int abcon_xwire_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	int ret = 0;

	if (!bl_xwire)
		return -1;

	if (bl_xwire->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_xwire->vsync_cnt = 0;

	if (bl_xwire->dev_on_flag == 0) {
		if (bl_xwire->vsync_cnt == 0)
			ABCONPR("%s: on_flag=%d\n", __func__, bl_xwire->dev_on_flag);
		return 0;
	}

	ldim_abcon_swduty_set(ldim_drv);

	if (abcon->autotrans_ready == 0)
		ldim_abcon_xwire_dimming();

	return ret;
}

static int abcon_xwire_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int abcon_xwire_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	if (bl_xwire->dev_on_flag == 0)
		return 0;

	if (abcon->conf.fb_en == 0)
		return 0;

	if (abcon->fb_det_cnt++ < abcon->conf.fb_det_int)
		return 0;
	abcon->fb_det_cnt = 0;

	ldim_abcon_xwire_get_feedback();
	ldim_abcon_fb_pwm(ldim_drv);

	return ret;
}

static int abcon_xwire_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	return ret;
}

static int abcon_xwire_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_xwire)
		return -1;
	if (bl_xwire->dev_on_flag) {
		ABCONPR("%s: abcon_xwire is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	abcon_xwire_hw_init_on(ldim_drv->dev_drv);
	bl_xwire->dev_on_flag = 1;
	bl_xwire->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static int abcon_xwire_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_xwire)
		return -1;

	mutex_lock(&dev_mutex);
	bl_xwire->dev_on_flag = 0;
	abcon->autotrans_ready = 0;
	abcon_xwire_hw_init_off(ldim_drv->dev_drv);
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t abcon_xwire_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_xwire)
		return sprintf(buf, "bl_xwire is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (attr && attr->attr.name && !strcmp(attr->attr.name, "abcon_xwire_status")) {
		len = sprintf(buf, "abcon_xwire status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n",
				dev_drv->index,
				bl_xwire->dev_on_flag,
				bl_xwire->vsync_cnt);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t abcon_xwire_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int val, val1, val2, val3;
	int n = 0;
	char *buf_orig, *ps, *token;
	char **parm = NULL;
	char str[3] = {' ', '\n', '\0'};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;
	parm = kcalloc(128, sizeof(char *), GFP_KERNEL);
	if (!parm)
		goto abcon_xwire_store_end;

	ps = buf_orig;
	while (1) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "init")) {
		mutex_lock(&dev_mutex);
		abcon_xwire_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "init_com")) {
		mutex_lock(&dev_mutex);
		//init abcon common
		ldim_abcon_init_common_registers(abcon);
		//init abcon register for xwire
		ldim_abcon_init_registers_for_xwire();
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "readback")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_xwire_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_xwire_store_err;
			if (kstrtouint(parm[3], 0, &val2) < 0)
				goto abcon_xwire_store_err;
			val3 = ldim_abcon_xwire_readback((unsigned char)val,
				(unsigned char)val1, (unsigned char)val2);

			ABCONPR("readback reg=0x%x, chip=0x%x, lane=%d, val=0x%08x\n",
			val, val1, val2, val3);
		}
	} else if (!strcmp(parm[0], "wreg")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_xwire_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_xwire_store_err;
			ldim_abcon_write_xwire_register(val, val1);

			ABCONPR("broadcast reg=0x%x, val=0x%08x\n", val, val1);
		}
	} else {
		ABCONERR("argument error!\n");
	}

abcon_xwire_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_xwire_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute abcon_xwire_class_attrs[] = {
	__ATTR(xwire, 0644, NULL, abcon_xwire_store),
	__ATTR(xwire_status, 0644, abcon_xwire_show, NULL),
};

static int abcon_xwire_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = abcon_xwire_power_on;
	dev_drv->power_off = abcon_xwire_power_off;
	dev_drv->dev_transmit = abcon_xwire_transmit;
	dev_drv->dev_transmit_dummy = abcon_xwire_transmit_dummy;
	dev_drv->dev_fb_handle = abcon_xwire_fb_handle;
	dev_drv->config_update = abcon_xwire_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_abcon_xwire_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int i;

	if (!dev_drv) {
		ABCONERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	bl_xwire = kzalloc(sizeof(*bl_xwire), GFP_KERNEL);
	if (!bl_xwire) {
		ABCONERR("%s: bl_xwire is error\n", __func__);
		return -1;
	}

	bl_xwire->dev_type = abcon->conf.ctrl & 0x1;

	if (bl_xwire->dev_type)
		bl_xwire->regmap = &regmap_conf_871;
	else
		bl_xwire->regmap = &regmap_conf_852;

	bl_xwire->dev_on_flag = 0;
	bl_xwire->vsync_cnt = 0;
	bl_xwire->dataformat = 0;//to do
	bl_xwire->init_sampletime = 0;

	//samplepoint is about ((1000000/txclk)/2)/63.581
	bl_xwire->samplepoint = (1000000 / abcon->conf.tx_clk) >> 7;

	//init abcon register for xwire
	//ldim_abcon_init_registers_for_xwire();

	abcon_xwire_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(abcon_xwire_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &abcon_xwire_class_attrs[i])) {
				ABCONERR("create ldim_dev abcon xwire class attribute %s fail\n",
					abcon_xwire_class_attrs[i].attr.name);
			}
		}
	}
	if (bdrv->state & BL_STATE_BL_ON)
		bl_xwire->dev_on_flag = 1; /* default enable in uboot */

	abcon_pr("%s ok\n", __func__);
	return 0;
}

int ldim_dev_abcon_xwire_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_xwire)
		return 0;

	kfree(bl_xwire);
	bl_xwire = NULL;

	return 0;
}
