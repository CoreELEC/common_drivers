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

#define BLMCU_CLASS_NAME "abcon_owc"

#define VSYNC_INFO_FREQUENT        300
static DEFINE_MUTEX(dev_mutex);

enum sop_type_e {
	sop_broadcast = 0,
	sop_config,
	sop_dimming,
	sop_readback,
	sop_max,
};

struct abcon_owc_s {
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned char bl_pwm_en;	/*0:default uboot have pwm, 1: uboot without pwm*/

	unsigned char act_lane_num;
	unsigned int fb_cnt;
	unsigned int fb_high_cnt_th;
	unsigned char sop_type;
	unsigned char datalen;
	unsigned char dc_num;//per dim
	unsigned char pwm_num;//per dim
	unsigned char tot_dc_pwm_num;
	unsigned char *data;
	unsigned char testcnt;
};

struct abcon_owc_s *bl_owc;

struct owc_pkt_s {
	unsigned int pream_header;
	unsigned int pream_data;

	unsigned int addr_header;
	unsigned int addr_data;

	unsigned int status_header;
	unsigned int status_data;

	unsigned int reg_addr_header;
	unsigned int reg_addr_data;

	unsigned int datalen_header;
	unsigned int datalen_data;

	unsigned int data_header;
	unsigned int data_data;

	unsigned int end_header;
	unsigned int end_data;
};

struct owc_pkt_s owc_pkt;

static void ldim_abcon_owc_write_pkt(unsigned char lanenum, unsigned char *data)
{
	unsigned int len = 0;
	unsigned int *buf;// = abcon_mem.wseg;
	unsigned char i = 0, j = 0;
	unsigned char act_lane_num = lanenum;
	unsigned char temp = 0;
	unsigned short valid = 0;

	if (bl_owc->sop_type == sop_dimming)
		buf = abcon_mem.ldc_seg;
	else
		buf = abcon_mem.wseg;

	if (bl_owc->sop_type == sop_readback)
		act_lane_num = 1;

	if (!buf)
		return;

	//for autotrans dimming
	if (bl_owc->sop_type == sop_dimming) {
		if (abcon->chip_type == ABCON_CHIP_T6W) {
			buf[len++] = 0x10 | abcon->act_lane;
		} else {
			buf[len++] = 0xffffc000;
			buf[len++] = 0xfffe4000 | abcon->act_lane;
		}
		abcon->autotrans_ready = 1;
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.pream_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.pream_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.addr_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.addr_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.status_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.status_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.reg_addr_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.reg_addr_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.datalen_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.datalen_data;

	if (bl_owc->sop_type == sop_dimming) {
		for (i = 0; i < act_lane_num; i++) {
			valid = abcon->act_lane & (1 << i);
			if (valid == 0)
				j = j + 1;

			buf[len++] = 0xc81 |
			((abcon->conf.chip_num[j] * bl_owc->tot_dc_pwm_num) << 14);
			j++;
		}
	} else {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = owc_pkt.data_header;
	}

	if (bl_owc->sop_type != sop_dimming) {
		for (j = 0; j <= owc_pkt.datalen_data; j++)
			for (i = 0; i < act_lane_num; i++)
				buf[len++] = data[j];
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = owc_pkt.end_header;

	if (abcon->conf.dev_type == 2) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = owc_pkt.end_data;
	} else {
		temp = (abcon->max_lane_dim / 32) + 1;
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = owc_pkt.end_data;
	}

	if (bl_owc->sop_type == sop_dimming) {
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

static void ldim_abcon_owc_broadcast(unsigned char reg_addr, unsigned char data)
{
	bl_owc->sop_type = sop_broadcast;

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	if (abcon->conf.dev_type == 2) {
		//xp7104, 2 *bit1
		owc_pkt.pream_header = 0x08002082;
		owc_pkt.pream_data = 0x3;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.pream_header = 0x04002082;
		owc_pkt.pream_data = 0x1;
	}

	owc_pkt.addr_header = 0x20002080;
	owc_pkt.addr_data = 0xff;//write same regs with same data

	owc_pkt.status_header = 0x60002080;
	owc_pkt.status_data = 0x00;

	owc_pkt.reg_addr_header = 0x20002080;
	owc_pkt.reg_addr_data = reg_addr & 0x7f;

	owc_pkt.datalen_header = 0x20002080;
	owc_pkt.datalen_data = 0;

	owc_pkt.data_header = 0x4880;// | (abcon->act_lane) << 14;
	//owc_pkt.data_data = value;

	if (abcon->conf.dev_type == 2) {
		//xp7104, 8 * bit0
		owc_pkt.end_header = 0x20002088;
		owc_pkt.end_data = 0x0;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.end_header = 0x2088 | (abcon->max_lane_dim << 26);
		owc_pkt.end_data = 0x0;
	}

	ldim_abcon_owc_write_pkt(abcon->act_lane_num, &data);
	usleep_range(50, 100);
}

static void ldim_abcon_owc_dimming(void)
{
	unsigned char data = 0;

	bl_owc->sop_type = sop_dimming;

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	if (abcon->conf.dev_type == 2) {
		//xp7104, 2 *bit1
		owc_pkt.pream_header = 0x08002082;
		owc_pkt.pream_data = 0x3;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.pream_header = 0x04002082;
		owc_pkt.pream_data = 0x1;
	}

	owc_pkt.addr_header = 0x20002080;
	owc_pkt.addr_data = 0x01;

	owc_pkt.status_header = 0x60002080;
	owc_pkt.status_data = 0x00;

	owc_pkt.reg_addr_header = 0x20002080;
	owc_pkt.reg_addr_data = 0;

	owc_pkt.datalen_header = 0x20002080;
	owc_pkt.datalen_data = (bl_owc->datalen - 1) & 0x3f;

	//owc_pkt.data_header = 0x881 | (24 << 14);
	//((abcon->act_lane_num * datalen * abcon->max_lane_dim) << 14);
	//owc_pkt.data_data = value;

	if (abcon->conf.dev_type == 2) {
		//xp7104, 8 * bit0
		owc_pkt.end_header = 0x20002088;
		owc_pkt.end_data = 0x0;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.pream_header = 0x2088 | (abcon->max_lane_dim << 26);
		owc_pkt.pream_data = 0x0;
	}

	ldim_abcon_owc_write_pkt(abcon->act_lane_num, &data);
}

static void ldim_abcon_owc_lane_config(unsigned char lane, unsigned char chip,
	unsigned char reg_addr, unsigned char len, unsigned char *data, unsigned char wr)
{
	unsigned int temp = 0;

	bl_owc->sop_type = sop_config;

	//select write which lane
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, 1, 10, 4);//one lane each time

	temp = 1 << lane;
	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, temp, 22, 4);
	else
		abcon_wr_reg_bits(0x29, temp, 0, 12);

	if (abcon->conf.dev_type == 2) {
		//xp7104, 2 *bit1
		owc_pkt.pream_header = 0x08002082;
		owc_pkt.pream_data = 0x3;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.pream_header = 0x04002082;
		owc_pkt.pream_data = 0x1;
	}

	owc_pkt.addr_header = 0x20002080;
	owc_pkt.addr_data = wr | (chip << 1);

	owc_pkt.status_header = 0x60002080;
	owc_pkt.status_data = 0x00;

	owc_pkt.reg_addr_header = 0x20002080;
	owc_pkt.reg_addr_data = reg_addr & 0x7f;

	owc_pkt.datalen_header = 0x20002080;
	owc_pkt.datalen_data = len - 1;

	owc_pkt.data_header = 0x880 | (len << 14);
	//owc_pkt.data_data = value;

	if (abcon->conf.dev_type == 2) {
		//xp7104, 8 * bit0
		owc_pkt.end_header = 0x20002088;
		owc_pkt.end_data = 0x0;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.end_header = 0x2088 | (abcon->max_lane_dim << 26);
		owc_pkt.end_data = 0x0;
	}

	ldim_abcon_owc_write_pkt(1, data);
	usleep_range(50, 100);
}

static void ldim_abcon_owc_read_pkt(unsigned char *data)
{
	unsigned int len = 0, j = 0;
	unsigned int *buf = abcon_mem.rseg;

	if (!buf)
		return;

	buf[len++] = owc_pkt.pream_header;
	buf[len++] = owc_pkt.pream_data;
	buf[len++] = owc_pkt.addr_header;
	buf[len++] = owc_pkt.addr_data;
	buf[len++] = owc_pkt.status_header;
	buf[len++] = owc_pkt.status_data;
	buf[len++] = owc_pkt.reg_addr_header;
	buf[len++] = owc_pkt.reg_addr_data;
	buf[len++] = owc_pkt.datalen_header;
	buf[len++] = owc_pkt.datalen_data;
	buf[len++] = owc_pkt.data_header;

	for (j = 0; j <= owc_pkt.datalen_data; j++)
		buf[len++] = data[j];

	buf[len++] = owc_pkt.end_header;
	buf[len++] = owc_pkt.end_data;

	abcon_wr_reg_bits(0x05, len, 16, 13);//reg_abcon_sw_rseg_max_num
}

void ldim_abcon_owc_readback(unsigned char lane, unsigned char dim,
	unsigned char reg_addr, unsigned char len, unsigned char *data)
{
	unsigned int rdata, ro_rbuf_depth, rd_done;
	unsigned int i = 0;

	bl_owc->sop_type = sop_readback;

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, 1, 10, 4);// fix 1 lane

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, 1 << lane, 22, 4);//fix use lane 0
	else
		abcon_wr_reg_bits(0x29, 1 << lane, 0, 12);//fix use lane 0

	//prepare read packet first
	if (abcon->conf.dev_type == 2) {
		//xp7104, 2 *bit1
		owc_pkt.pream_header = 0x10004104;
		owc_pkt.pream_data = 0x3;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.pream_header = 0x08004104;
		owc_pkt.pream_data = 0x1;
	}

	owc_pkt.addr_header = 0x40004100;
	owc_pkt.addr_data = dim << 1;//bit0 = 0

	owc_pkt.status_header = 0xc0004100;
	owc_pkt.status_data = 0x00;

	owc_pkt.reg_addr_header = 0x40004100;
	owc_pkt.reg_addr_data = reg_addr & 0x7f;

	owc_pkt.datalen_header = 0x40004100;
	owc_pkt.datalen_data = len - 1;

	owc_pkt.data_header = 0x1100 | (len << 15);
	//owc_pkt.data_data = value;

	if (abcon->conf.dev_type == 2) {
		//xp7104, 8 * bit0
		owc_pkt.end_header = 0x40004110;
		owc_pkt.end_data = 0x0;
	} else {
		//xp7008, 1 * bit1
		owc_pkt.end_header = 0x4110 | (abcon->max_lane_dim << 27);
		owc_pkt.end_data = 0x0;
	}

	ldim_abcon_owc_read_pkt(data);

	//then set write packet
	ldim_abcon_owc_lane_config(lane, dim, reg_addr, len, data, 1);

	usleep_range(500, 1000);
	if (abcon->chip_type == ABCON_CHIP_T6W) {
		rdata = abcon_rd_reg(0x50);
		rdata = abcon_rd_reg(0x50);
		rdata = abcon_rd_reg(0x50);
		rdata = abcon_rd_reg(0x50);
		rdata = abcon_rd_reg(0x50);
		rdata = abcon_rd_reg(0x50);
		rd_done = (rdata & 0x40) >> 6;
		rdata = abcon_rd_reg(0x53);
		ro_rbuf_depth = (rdata & 0x7c0) >> 6;
		ABCONPR("rdata=0x%x, ro_rbuf_depth=0x%x, rd_done=%d\n",
		rdata, ro_rbuf_depth, rd_done);
		if (rd_done) {
			for (i = 0; i < ro_rbuf_depth; i = i + 1) {
				data[i] = abcon_rd_reg(0x52);
				ABCONPR("rdata=0x%x\n", rdata);
			}
		}
	} else {
		rdata = abcon_rd_reg(0x63);
		rdata = abcon_rd_reg(0x63);
		rdata = abcon_rd_reg(0x63);
		rdata = abcon_rd_reg(0x63);
		rdata = abcon_rd_reg(0x63);
		ro_rbuf_depth = (rdata & 0x7c0) >> 6;
		rd_done = (rdata & 0x2) >> 1;
		ABCONPR("rdata=0x%x, ro_rbuf_depth=0x%x, rd_done=%d\n",
		rdata, ro_rbuf_depth, rd_done);
		if (rd_done) {
			for (i = 0; i < ro_rbuf_depth; i = i + 1) {
				data[i] = abcon_rd_reg(0x62);
				ABCONPR("rdata=0x%x\n", rdata);
			}
		}
	}
}

static void ldim_abcon_init_registers_for_owc(void)
{
	//swrst
	ldim_abcon_swrst();

	//reg_abcon_wire_mode
	abcon_wr_reg_bits(0x22, 0, 30, 2);
	//reg_abcon_rx_scale_sel
	abcon_wr_reg_bits(0x03, 1, 1, 1);
	//reg_abcon_dual_line_mode
	abcon_wr_reg_bits(0x03, 0, 4, 1);

	//only 12bit pwm
	//abcon_wr_reg(0x1a, 0x7b000000);
	//abcon_wr_reg(0x1b, 0x00c00180);

	//12bit pwm + 8 dc
	abcon_wr_reg(0x1a, 0x7e000000);
	abcon_wr_reg(0x1b, 0x20c40180);

	//reg_abcon_pwm_num_per_dim
	abcon_wr_reg_bits(0x40, bl_owc->pwm_num, 0, 8);
	//reg_abcon_dc_num_per_dim
	abcon_wr_reg_bits(0x40, bl_owc->dc_num, 8, 8);

	//init feedback registers
	//ldim_abcon_owc_init_feedback();
}

static void ldim_abcon_init_owc(void)
{
	ldim_abcon_owc_broadcast(0x6f, 0xb0);
	ldim_abcon_owc_broadcast(0x69, 0xff);//set pwm mode

	ldim_abcon_owc_broadcast(0x0c, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x0d, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x0e, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x0f, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x10, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x11, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x12, 0x5e);//22ma max60ma
	ldim_abcon_owc_broadcast(0x13, 0x5e);//22ma max60ma
}

static int abcon_owc_hw_init_on(struct ldim_dev_driver_s *dev_drv)
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

	//init abcon register for owc
	ldim_abcon_init_registers_for_owc();

	//init owc dimmer registers
	ldim_abcon_init_owc();
	bl_delay_ms(1);

	ldim_abcon_swrst();
	return 0;
}

static int abcon_owc_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static int abcon_owc_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	int ret = 0;

	if (!bl_owc)
		return -1;

	if (bl_owc->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_owc->vsync_cnt = 0;

	if (bl_owc->dev_on_flag == 0) {
		if (bl_owc->vsync_cnt == 0)
			ABCONPR("%s: on_flag=%d\n", __func__, bl_owc->dev_on_flag);
		return 0;
	}

	ldim_abcon_swduty_set(ldim_drv);

	if (abcon->autotrans_ready == 0)
		ldim_abcon_owc_dimming();

	return ret;
}

static int abcon_owc_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int abcon_owc_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	if (bl_owc->dev_on_flag == 0)
		return 0;

	if (abcon->conf.fb_en == 0)
		return 0;

	if (abcon->fb_det_cnt++ < abcon->conf.fb_det_int)
		return 0;
	abcon->fb_det_cnt = 0;

	//ldim_abcon_owc_get_feedback();
	//ldim_abcon_fb_pwm(ldim_drv);

	return ret;
}

static int abcon_owc_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	ABCONPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static int abcon_owc_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_owc)
		return -1;
	if (bl_owc->dev_on_flag) {
		ABCONPR("%s: abcon_owc is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	abcon_owc_hw_init_on(ldim_drv->dev_drv);
	bl_owc->dev_on_flag = 1;
	bl_owc->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static int abcon_owc_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_owc)
		return -1;

	mutex_lock(&dev_mutex);
	bl_owc->dev_on_flag = 0;
	abcon->autotrans_ready = 0;
	abcon_owc_hw_init_off(ldim_drv->dev_drv);
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t abcon_owc_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_owc)
		return sprintf(buf, "bl_owc is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (attr && attr->attr.name && !strcmp(attr->attr.name, "abcon_owc_status")) {
		len = sprintf(buf, "abcon_owc status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n",
				dev_drv->index,
				bl_owc->dev_on_flag,
				bl_owc->vsync_cnt);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t abcon_owc_store(const struct class *class, const struct class_attribute *attr,
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
		goto abcon_owc_store_end;

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
		abcon_owc_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "init_com")) {
		mutex_lock(&dev_mutex);
		//init abcon common
		ldim_abcon_init_common_registers(abcon);
		//init abcon register for owc
		ldim_abcon_init_registers_for_owc();
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "config")) {
		if (parm[4]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[3], 0, &val2) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[4], 0, &val3) < 0)
				goto abcon_owc_store_err;

			ABCONPR("readback lane=0x%x, chip=0x%x, reg_addr=%d, len=0x%08x\n",
				val, val1, val2, val3);
			ldim_abcon_owc_lane_config(val, val1, val2, val3, bl_owc->data, 1);
		}
	} else if (!strcmp(parm[0], "readback")) {
		if (parm[4]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[3], 0, &val2) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[4], 0, &val3) < 0)
				goto abcon_owc_store_err;

			ABCONPR("readback lane=0x%x, chip=0x%x, reg_addr=%d, len=0x%08x\n",
				val, val1, val2, val3);
			ldim_abcon_owc_readback(val, val1, val2, val3, bl_owc->data);
		}
	} else if (!strcmp(parm[0], "broadcast")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_owc_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_owc_store_err;
			ldim_abcon_owc_broadcast(val, val1);
			ABCONPR("broadcast reg=0x%x, val=0x%08x\n", val, val1);
		}
	} else if (!strcmp(parm[0], "dimming")) {
		ldim_abcon_owc_dimming();
	} else if (!strcmp(parm[0], "testdim")) {
		//ldim_abcon_owc_dimming();
		for (n = 0; n < bl_owc->testcnt; n++) {
			abcon_wr_reg_bits(0x00, 1, 1, 1);//reg_abcon_ldc_mod_start
			abcon_wr_reg_bits(0x00, 0, 1, 1);//reg_abcon_ldc_mod_start
			usleep_range(1000, 2000);
		}
	} else if (!strcmp(parm[0], "testcnt")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_owc_store_err;
			bl_owc->testcnt = val;
		}
		ABCONPR("testcnt=%d\n", val);
	} else {
		ABCONERR("argument error!\n");
	}

abcon_owc_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_owc_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute abcon_owc_class_attrs[] = {
	__ATTR(owc, 0644, NULL, abcon_owc_store),
	__ATTR(owc_status, 0644, abcon_owc_show, NULL),
};

static int abcon_owc_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = abcon_owc_power_on;
	dev_drv->power_off = abcon_owc_power_off;
	dev_drv->dev_transmit = abcon_owc_transmit;
	dev_drv->dev_transmit_dummy = abcon_owc_transmit_dummy;
	dev_drv->dev_fb_handle = abcon_owc_fb_handle;
	dev_drv->config_update = abcon_owc_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_abcon_owc_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int i;

	if (!dev_drv) {
		ABCONERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	bl_owc = kzalloc(sizeof(*bl_owc), GFP_KERNEL);
	if (!bl_owc) {
		ABCONERR("%s: bl_owc is error\n", __func__);
		return -1;
	}

	bl_owc->data = kcalloc(16, sizeof(unsigned char), GFP_KERNEL);
	if (!bl_owc->data) {
		ABCONERR("%s: bl_owc->data alloc error\n", __func__);
		return -1;
	}

	bl_owc->dev_on_flag = 0;
	bl_owc->vsync_cnt = 0;

	bl_owc->fb_cnt = 10;
	bl_owc->fb_high_cnt_th = 8;
	bl_owc->testcnt = 3;

	bl_owc->datalen = abcon->conf.ctrl & 0xff;
	bl_owc->dc_num = (abcon->conf.ctrl >> 8) & 0xf;
	bl_owc->pwm_num = (abcon->conf.ctrl >> 12) & 0xf;
	bl_owc->tot_dc_pwm_num = bl_owc->dc_num + bl_owc->pwm_num;
	ABCONPR("ctrl=0x%x, datalen=%d, dc_num=%d, pwm_num=%d\n",
	abcon->conf.ctrl, bl_owc->datalen, bl_owc->dc_num, bl_owc->pwm_num);

	//init abcon register for owc
	ldim_abcon_init_registers_for_owc();

	abcon_owc_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(abcon_owc_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &abcon_owc_class_attrs[i])) {
				ABCONERR("create ldim_dev abcon owc class attribute %s fail\n",
					abcon_owc_class_attrs[i].attr.name);
			}
		}
	}
	if (bdrv->state & BL_STATE_BL_ON)
		bl_owc->dev_on_flag = 1; /* default enable in uboot */

	abcon_pr("%s ok\n", __func__);
	return 0;
}

int ldim_dev_abcon_owc_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_owc)
		return 0;

	kfree(bl_owc);
	bl_owc = NULL;

	return 0;
}
