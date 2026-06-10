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
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>
// #include "../../../lcd_common.h"
#include "../ldim_drv.h"
#include "../ldim_dev_drv.h"
#include "ldim_abcon.h"
#include "ldim_abcon_reg.h"

#define BLMCU_CLASS_NAME "abcon_ospb"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

enum sop_type_e {
	sop_address = 0,
	sop_broadcast,
	sop_readback,
	sop_dimming,
	sop_feedback,
	sop_max,
};

enum ospb_head_e {
	HEAD_BROADCAST	= 0x7200,
	HEAD_ADDRESS	= 0x7400,
	HEAD_DIMMING	= 0x7100,
	HEAD_CONFIG		= 0x7300,
	HEAD_READBACK	= 0x7500,
	HEAD_MAX,
};

//8608/8108
enum dim_mode_e {
	PWM_10PWM_2DITHER = 0,
	DC_10DC_6DITHER,
	MIX_6DC_10PWM,
	MIX_10DC_6PWM,
	DC_10DC,
	DC_8DC_8DITHER,
	DIM_MODE_MAX
};

enum dos_mode_e {
	SINGLE = 0,
	CH_SICK_INT1,
	SHORT_INT,
	OPEN_INT,
	OTP_INT,
	CRC_INT,
	PWM_HIGH_TO_SMALL,
	CH_SICK_INT2,
	OH_FAULT,
	ID_FAULT,
	FAULT_CRC,
	ADDR_CHECK = 0xb,
	DIP_TIMEOUT,
	UART_COMM = 0xf,
};

#define FB_VALUE 0xF840
#define COMM_PARALLEL 0
#define COMM_SERIAL 1
#define UNLOCK_KEY 0xa5c9
#define LOCK_KEY 0x5ac3

struct abcon_ospb_s {
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned char bl_pwm_en;	/*0:default uboot have pwm, 1: uboot without pwm*/

	unsigned char act_lane_num;
	unsigned char dual_wire;
	unsigned char default_value;
	unsigned int fb_cnt;
	unsigned int fb_high_cnt_th;
	unsigned char sop_type;
	unsigned char comm_mode;//datalen[14], 0:parallel, 1:serial
	unsigned char dos_mode;
	unsigned char dimming_mode;
};

struct abcon_ospb_s *bl_ospb;

struct ospb_pkt_s {
	unsigned int pream_header;
	unsigned int pream_data;

	unsigned int head_header;
	unsigned int head_data;

	unsigned int addr_header;
	unsigned int addr_data;

	unsigned int sub_addr_header;
	unsigned int sub_addr_data;

	unsigned int fb_header;
	unsigned int fb_data;

	unsigned int datalen_header;
	unsigned int datalen_data;

	unsigned int data_header;
	unsigned int data_data;

	unsigned int crc_header;
	unsigned int crc_data;
};

struct ospb_pkt_s ospb_pkt;

static void ldim_abcon_ospb_write_pkt(void)
{
	unsigned int len = 0;
	unsigned int *buf;// = abcon_mem.wseg;
	unsigned char i = 0;
	unsigned char act_lane_num = abcon->act_lane_num;

	if (bl_ospb->sop_type == sop_dimming)
		buf = abcon_mem.ldc_seg;
	else
		buf = abcon_mem.wseg;

	if (bl_ospb->sop_type == sop_readback)
		act_lane_num = 1;

	if (!buf)
		return;

	//for autotrans dimming
	if (bl_ospb->sop_type == sop_dimming) {
		if (abcon->chip_type == ABCON_CHIP_T6W) {
			buf[len++] = 0x40 |
						(abcon->tx_scale << 9) |
						(bl_ospb->dual_wire << 8) |
						abcon->act_lane;
		} else {
			buf[len++] = 0xffffc000;
			buf[len++] = 0xffff0000 | abcon->act_lane;
		}
		abcon->autotrans_ready = 1;
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.pream_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.pream_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.head_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.head_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.addr_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.addr_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.sub_addr_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.sub_addr_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.fb_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.fb_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.datalen_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.datalen_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.data_header;

	if (bl_ospb->sop_type != sop_dimming) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = ospb_pkt.data_data;
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = ospb_pkt.crc_header;

	//for (i = 0; i < act_lane_num; i++)
	//	buf[len++] = ospb_pkt.crc_data;

	if (bl_ospb->sop_type == sop_dimming) {
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

static void ldim_abcon_ospb_broadcast(unsigned int regaddr,
	unsigned int value, unsigned char comm_mode)
{
	bl_ospb->sop_type = sop_broadcast;
	bl_ospb->dos_mode = SINGLE;

	//clr done flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	ospb_pkt.pream_header = 0x40002082;
	ospb_pkt.pream_data = 0x5555;

	ospb_pkt.head_header = 0x400020b0;
	ospb_pkt.head_data = HEAD_BROADCAST |
		(bl_ospb->dos_mode << 4) | (bl_ospb->dimming_mode << 1);

	ospb_pkt.addr_header = 0xc00020a0;
	ospb_pkt.addr_data = 0xff00;//fix value

	ospb_pkt.sub_addr_header = 0xc00020a0;
	ospb_pkt.sub_addr_data = regaddr & 0xff;

	ospb_pkt.fb_header = 0x400020a0;
	ospb_pkt.fb_data = FB_VALUE;

	ospb_pkt.datalen_header = 0xc00020a0;
	ospb_pkt.datalen_data = comm_mode << 14;

	ospb_pkt.data_header = 0x400020a0;
	ospb_pkt.data_data = value;

	ospb_pkt.crc_header = 0x4000208c;//hw_crc_en, without crc_data
	//ospb_pkt.crc_data = 0x400020c4;

	ldim_abcon_ospb_write_pkt();
	usleep_range(500, 1000);
}

static void ldim_abcon_ospb_address(void)
{
	bl_ospb->sop_type = sop_address;
	bl_ospb->comm_mode = COMM_SERIAL;//serial
	bl_ospb->dos_mode = ADDR_CHECK;

	//clr done flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	ospb_pkt.pream_header = 0x40002082;
	ospb_pkt.pream_data = 0x5555;

	ospb_pkt.head_header = 0x400020b0;
	ospb_pkt.head_data = HEAD_ADDRESS | (bl_ospb->dos_mode << 4);

	ospb_pkt.addr_header = 0xc00020a0;
	ospb_pkt.addr_data = (abcon->max_lane_dim) << 8 | 0x01;

	ospb_pkt.sub_addr_header = 0xc00020a0;
	ospb_pkt.sub_addr_data = 0x0;

	ospb_pkt.fb_header = 0x400020a0;
	ospb_pkt.fb_data = FB_VALUE;

	ospb_pkt.datalen_header = 0xc00020a0;
	ospb_pkt.datalen_data = bl_ospb->comm_mode << 14;

	ospb_pkt.data_header = 0x400020a0;
	ospb_pkt.data_data = 0x0;

	ospb_pkt.crc_header = 0x4000208c;//hw_crc_en, without crc_data
	//ospb_pkt.crc_data = 0x400020c4;

	ldim_abcon_ospb_write_pkt();
	usleep_range(1000, 2000);
}

static void ldim_abcon_ospb_dimming(void)
{
	bl_ospb->sop_type = sop_dimming;
	bl_ospb->comm_mode = COMM_PARALLEL;
	bl_ospb->dos_mode = CH_SICK_INT1;

	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);

	ospb_pkt.pream_header = 0x40002082;
	ospb_pkt.pream_data = 0x5555;

	ospb_pkt.head_header = 0x400020b0;
	ospb_pkt.head_data = HEAD_DIMMING |
		(bl_ospb->dos_mode << 4) | (bl_ospb->dimming_mode << 1);

	ospb_pkt.addr_header = 0xc00020a0;
	ospb_pkt.addr_data = (abcon->max_lane_dim - 1) << 8 | 0x01;

	ospb_pkt.sub_addr_header = 0xc00020a0;
	ospb_pkt.sub_addr_data = 0x0;

	ospb_pkt.fb_header = 0x400020a0;
	ospb_pkt.fb_data = FB_VALUE;

	ospb_pkt.datalen_header = 0xc00020a0;
	ospb_pkt.datalen_data =
		(abcon->max_lane_ch - 1) | (bl_ospb->comm_mode << 14);

	ospb_pkt.data_header = 0x10a1 | (abcon->max_lane_ch << 14);
	//ospb_pkt.data_data = 0x0;

	ospb_pkt.crc_header = 0x4000208c;//hw_crc_en, without crc_data
	//ospb_pkt.crc_data = 0x400020c4;

	ldim_abcon_ospb_write_pkt();
}

static void ldim_abcon_ospb_read_pkt(void)
{
	unsigned int len = 0;
	unsigned int *buf = abcon_mem.rseg;

	if (!buf)
		return;

	buf[len++] = ospb_pkt.pream_header;
	buf[len++] = ospb_pkt.pream_data;
	buf[len++] = ospb_pkt.head_header;
	buf[len++] = ospb_pkt.head_data;
	buf[len++] = ospb_pkt.addr_header;
	buf[len++] = ospb_pkt.addr_data;
	buf[len++] = ospb_pkt.sub_addr_header;
	buf[len++] = ospb_pkt.sub_addr_data;
	buf[len++] = ospb_pkt.fb_header;
	buf[len++] = ospb_pkt.fb_data;
	buf[len++] = ospb_pkt.datalen_header;
	buf[len++] = ospb_pkt.datalen_data;
	buf[len++] = ospb_pkt.data_header;
	buf[len++] = ospb_pkt.data_data;
	buf[len++] = ospb_pkt.crc_header;

	abcon_wr_reg_bits(0x05, len, 16, 13);//reg_abcon_sw_rseg_max_num
}

unsigned int ldim_abcon_ospb_readback(unsigned char reg,
	unsigned char chip, unsigned char lane)
{
	unsigned int readback_val = 0;

	bl_ospb->sop_type = sop_readback;
	bl_ospb->comm_mode = COMM_SERIAL;
	bl_ospb->dos_mode = SINGLE;

	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x5559, 16, 16);

	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, 1, 10, 4);// fix 1 lane

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, 1 << lane, 22, 4);//fix use one lane
	else
		abcon_wr_reg_bits(0x29, 1 << lane, 0, 12);//fix use one lane

	//prepare read packet first
	ospb_pkt.pream_header = 0x00002104;
	ospb_pkt.pream_data = 0x5555;

	ospb_pkt.head_header = 0x00002148;
	ospb_pkt.head_data = HEAD_READBACK | (bl_ospb->dos_mode << 4);

	ospb_pkt.addr_header = 0x04002148;
	ospb_pkt.addr_data = chip & 0xff;

	ospb_pkt.sub_addr_header = 0x04002148;
	ospb_pkt.sub_addr_data = reg & 0xff;

	ospb_pkt.fb_header = 0x00002148;
	ospb_pkt.fb_data = FB_VALUE;

	ospb_pkt.datalen_header = 0x04002148;
	ospb_pkt.datalen_data = bl_ospb->comm_mode << 14;

	ospb_pkt.data_header = 0x00002148;
	ospb_pkt.data_data = 0x0;

	ospb_pkt.crc_header = 0x00002150;//hw_crc_en, without crc_data
	//ospb_pkt.crc_data = 0x400020c4;
	ldim_abcon_ospb_read_pkt();

	//then set write packet
	ospb_pkt.pream_header = 0x40002082;
	ospb_pkt.pream_data = 0x5555;

	ospb_pkt.head_header = 0x400020b0;
	ospb_pkt.head_data = HEAD_READBACK | (bl_ospb->dos_mode << 4);

	ospb_pkt.addr_header = 0xc00020a0;
	ospb_pkt.addr_data = chip & 0xff;

	ospb_pkt.sub_addr_header = 0xc00020a0;
	ospb_pkt.sub_addr_data = reg & 0xff;

	ospb_pkt.fb_header = 0x400020a0;
	ospb_pkt.fb_data = FB_VALUE;

	ospb_pkt.datalen_header = 0xc00020a0;
	ospb_pkt.datalen_data = bl_ospb->comm_mode << 14;

	ospb_pkt.data_header = 0x400020a0;
	ospb_pkt.data_data = 0x0;

	ospb_pkt.crc_header = 0x4000208c;//hw_crc_en, without crc_data
	//ospb_pkt.crc_data = 0x400020c4;

	ldim_abcon_ospb_write_pkt();
	usleep_range(500, 1000);
	readback_val = abcon_rd_reg(0x52);

	return readback_val;
}

static void ldim_abcon_ospb_init_feedback(void)
{
	bl_ospb->sop_type = sop_feedback;
	//reg_abcon_fb_en
	abcon_wr_reg_bits(0x37, abcon->conf.fb_en, 20, 1);
	//reg_abcon_fb_sta_immed
	abcon_wr_reg_bits(0x37, 1, 16, 1);
	//reg_abcon_fb_tim_res, 100us sampling interval, 100000/6=16666
	abcon_wr_reg_bits(0x38, 16666, 16, 16);
	//reg_abcon_pre_fb_time
	abcon_wr_reg_bits(0x38, 1, 0, 16);
	//reg_abcon_post_fb_time
	abcon_wr_reg_bits(0x39, 1, 0, 16);
	//reg_abcon_fb_read_time, expect sampling count
	abcon_wr_reg_bits(0x39, bl_ospb->fb_cnt, 16, 16);
}

static void ldim_abcon_ospb_get_feedback(void)
{
	unsigned int fb_cnt[4];
	unsigned int temp, i, val = 0;

	if (!abcon->conf.fb_en)
		return;

	temp = abcon_rd_reg(0x54);
	fb_cnt[0] = temp & 0xffff;
	fb_cnt[1] = (temp >> 16) & 0xffff;
	temp = abcon_rd_reg(0x55);
	fb_cnt[2] = temp & 0xffff;
	fb_cnt[3] = (temp >> 16) & 0xffff;

	for (i = 0; i < abcon->act_lane_num; i++) {
		if (fb_cnt[i] > bl_ospb->fb_high_cnt_th)
			val = 1;
	}

	if (val)
		abcon->fb_pos_cnt++;
	else
		abcon->fb_neg_cnt++;

	//toggle fb sampling
	//reg_abcon_fb_sta_immed
	abcon_wr_reg_bits(0x00, 1, 11, 1);
	abcon_wr_reg_bits(0x00, 1, 11, 1);
	abcon_wr_reg_bits(0x00, 0, 11, 1);

	if (ldim_debug_print & LDIM_DBG_PR_DEV_DBG_INFO)
		ABCONPR("fb cnt %d:%d:%d:%d, val=%d, fb_pos_cnt=%d, fb_neg_cnt=%d\n",
		fb_cnt[0], fb_cnt[1], fb_cnt[2], fb_cnt[3], val,
		abcon->fb_pos_cnt, abcon->fb_neg_cnt);
}

static void ldim_abcon_init_registers_for_ospb(void)
{
	ABCONPR("begin to init bcon register for ospb\n");
	//reg_abcon_clr_done_flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_wire_mode
	abcon_wr_reg_bits(0x22, 0, 30, 2);
	//reg_frc_crc_val_en
	abcon_wr_reg_bits(0x03, 0, 0, 1);//disable
	//reg_abcon_rx_scale_sel
	abcon_wr_reg_bits(0x03, 1, 1, 1);
	//reg_abcon_hw_crc
	abcon_wr_reg_bits(0x03, 1, 3, 1);
	//reg_abcon_dual_line_mode
	abcon_wr_reg_bits(0x03, bl_ospb->dual_wire, 4, 1);
	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_default_value
	abcon_wr_reg_bits(0x03, bl_ospb->default_value, 8, 1);
	//reg_abcon_rx_preamble_num
	abcon_wr_reg_bits(0x03, 16, 24, 8);
	//reg_abcon_crc_poly
	abcon_wr_reg(0x0d, 0x00008005);
	//reg_abcon_crc_init
	abcon_wr_reg(0x0e, 0xffff);
	//reg_abcon_crc_xorout
	abcon_wr_reg(0x0f, 0);

	abcon_wr_reg(0x11, 0x70f081ff);

	abcon_wr_reg(0x1a, 0x7f000000);
	abcon_wr_reg(0x1b, 0x20c32940);//6dc+10pwm

	//init feedback registers
	ldim_abcon_ospb_init_feedback();

	ABCONPR("init bcon register for ospb done\n");
}

static void ldim_abcon_init_ospb(void)
{
	char i = 0, loop_cnt = 1;

	ABCONPR("begin to init ospb registers!\n");

	//unlock
	for (i = 0; i < loop_cnt; i++) {
		ldim_abcon_ospb_broadcast(0x4f, 2 << 13, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x78, UNLOCK_KEY, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x7c, UNLOCK_KEY, COMM_SERIAL);
	}

	//send address command
	for (i = 0; i < loop_cnt; i++)
		ldim_abcon_ospb_address();

	for (i = 0; i < loop_cnt; i++) {
		ldim_abcon_ospb_broadcast(0x41, 0x0004, COMM_PARALLEL); // tp as 10bit
		ldim_abcon_ospb_broadcast(0x42, 0x2000, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x48, 0x8a34, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x4a, 0x0011, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x4c, 0x0c28, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x4e, 0x9f01, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x47, 0x0007, COMM_PARALLEL);
	}

	for (i = 0; i < loop_cnt; i++) {
		ldim_abcon_ospb_broadcast(0x40, 0x00ff, COMM_PARALLEL); //channel enable
		ldim_abcon_ospb_broadcast(0x46, 0x8842, COMM_PARALLEL); //
		ldim_abcon_ospb_broadcast(0x60, 0x003f, COMM_PARALLEL); // fault clear
	}

	for (i = 0; i < loop_cnt; i++) {
		ldim_abcon_ospb_broadcast(0x78, UNLOCK_KEY, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x4E, 0x8d01, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x46, 0x8040, COMM_PARALLEL);
		ldim_abcon_ospb_broadcast(0x78, LOCK_KEY, COMM_PARALLEL);
	}

	ldim_abcon_ospb_broadcast(0x2f, 0x0000, COMM_PARALLEL);

	ABCONPR("init ospb registers end!\n");
}

static int abcon_ospb_hw_init_on(struct ldim_dev_driver_s *dev_drv)
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

	//init abcon register for ospb
	ldim_abcon_init_registers_for_ospb();

	//init ospb registers
	ldim_abcon_init_ospb();
	bl_delay_ms(1);

	ldim_abcon_swrst();
	return 0;
}

static int abcon_ospb_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);

	return 0;
}

static int abcon_ospb_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	int ret = 0;

	if (!bl_ospb)
		return -1;

	if (bl_ospb->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_ospb->vsync_cnt = 0;

	if (bl_ospb->dev_on_flag == 0) {
		if (bl_ospb->vsync_cnt == 0)
			ABCONPR("%s: on_flag=%d\n", __func__, bl_ospb->dev_on_flag);
		return 0;
	}

	ldim_abcon_swduty_set(ldim_drv);

	if (abcon->autotrans_ready == 0)
		ldim_abcon_ospb_dimming();

	return ret;
}

static int abcon_ospb_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int abcon_ospb_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	if (bl_ospb->dev_on_flag == 0)
		return 0;

	if (abcon->fb_det_cnt++ < abcon->conf.fb_det_int)
		return 0;
	abcon->fb_det_cnt = 0;

	ldim_abcon_ospb_get_feedback();
	ldim_abcon_fb_pwm(ldim_drv);

	return ret;
}

static int abcon_ospb_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	ABCONPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static int abcon_ospb_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_ospb)
		return -1;
	if (bl_ospb->dev_on_flag) {
		ABCONPR("%s: abcon_ospb is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	abcon_ospb_hw_init_on(ldim_drv->dev_drv);
	bl_ospb->dev_on_flag = 1;
	bl_ospb->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static int abcon_ospb_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_ospb)
		return -1;

	mutex_lock(&dev_mutex);
	bl_ospb->dev_on_flag = 0;
	abcon->autotrans_ready = 0;
	abcon_ospb_hw_init_off(ldim_drv->dev_drv);
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t abcon_ospb_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_ospb)
		return sprintf(buf, "bl_ospb is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (attr && attr->attr.name && !strcmp(attr->attr.name, "abcon_ospb_status")) {
		len = sprintf(buf, "abcon_ospb status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n",
				dev_drv->index,
				bl_ospb->dev_on_flag,
				bl_ospb->vsync_cnt);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t abcon_ospb_store(const struct class *class, const struct class_attribute *attr,
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
		goto abcon_ospb_store_end;

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
		abcon_ospb_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "init_com")) {
		mutex_lock(&dev_mutex);
		//init abcon common
		ldim_abcon_init_common_registers(abcon);
		//init abcon register for ospb
		ldim_abcon_init_registers_for_ospb();
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "readback")) {
		if (parm[3]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_ospb_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_ospb_store_err;
			if (kstrtouint(parm[3], 0, &val2) < 0)
				goto abcon_ospb_store_err;
			//switch to serial first
			ldim_abcon_ospb_broadcast(0x2f, 0x0000, COMM_SERIAL);
			usleep_range(100, 200);
			val3 = ldim_abcon_ospb_readback((unsigned char)val,
				(unsigned char)val1, (unsigned char)val2);

			ABCONPR("readback reg=0x%x, chip=0x%x, lane=%d, val=0x%08x\n",
				val, val1, val2, val3);
		}
	} else if (!strcmp(parm[0], "broadcast")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_ospb_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_ospb_store_err;
			ldim_abcon_ospb_broadcast(val, val1, COMM_PARALLEL);

			ABCONPR("broadcast reg=0x%x, val=0x%08x\n", val, val1);
		}
	} else if (!strcmp(parm[0], "address")) {
		ldim_abcon_ospb_address();
	} else if (!strcmp(parm[0], "fb_high_cnt_th")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_ospb_store_err;

			bl_ospb->fb_high_cnt_th = val;
		}
		ABCONPR("fb_high_cnt_th =%d\n", bl_ospb->fb_high_cnt_th);
	} else if (!strcmp(parm[0], "dimming_mode")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_ospb_store_err;

			bl_ospb->dimming_mode = val;
		}
		ABCONPR("dimming_mode =%d\n", bl_ospb->dimming_mode);
	} else {
		ABCONERR("argument error!\n");
	}

abcon_ospb_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_ospb_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute abcon_ospb_class_attrs[] = {
	__ATTR(ospb, 0644, NULL, abcon_ospb_store),
	__ATTR(ospb_status, 0644, abcon_ospb_show, NULL),
};

static int abcon_ospb_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = abcon_ospb_power_on;
	dev_drv->power_off = abcon_ospb_power_off;
	dev_drv->dev_transmit = abcon_ospb_transmit;
	dev_drv->dev_transmit_dummy = abcon_ospb_transmit_dummy;
	dev_drv->dev_fb_handle = abcon_ospb_fb_handle;
	dev_drv->config_update = abcon_ospb_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_abcon_ospb_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int i;

	if (!dev_drv) {
		ABCONERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	bl_ospb = kzalloc(sizeof(*bl_ospb), GFP_KERNEL);
	if (!bl_ospb) {
		ABCONERR("%s: bl_ospb is error\n", __func__);
		return -1;
	}

	bl_ospb->dev_on_flag = 0;
	bl_ospb->vsync_cnt = 0;

	bl_ospb->fb_cnt = 10;
	bl_ospb->fb_high_cnt_th = 8;

	bl_ospb->dual_wire = abcon->conf.ctrl & 0x1;
	bl_ospb->default_value = (abcon->conf.ctrl >> 1) & 0x1;
	bl_ospb->dimming_mode = abcon->conf.dimming_mode;
	ABCONPR("ctrl=0x%x, dual_wire=%d, default_value=%d, dimming_mode=%d\n",
	abcon->conf.ctrl, bl_ospb->dual_wire,
	bl_ospb->default_value, bl_ospb->dimming_mode);

	//init abcon register for ospb
	ldim_abcon_init_registers_for_ospb();

	abcon_ospb_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(abcon_ospb_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &abcon_ospb_class_attrs[i])) {
				ABCONERR("create ldim_dev abcon ospb class attribute %s fail\n",
					abcon_ospb_class_attrs[i].attr.name);
			}
		}
	}
	if (bdrv->state & BL_STATE_BL_ON)
		bl_ospb->dev_on_flag = 1; /* default enable in uboot */

	abcon_pr("%s ok\n", __func__);
	return 0;
}

int ldim_dev_abcon_ospb_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_ospb)
		return 0;

	kfree(bl_ospb);
	bl_ospb = NULL;

	return 0;
}
