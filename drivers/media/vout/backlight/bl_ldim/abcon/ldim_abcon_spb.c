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

#define BLMCU_CLASS_NAME "abcon_spb"

#define VSYNC_INFO_FREQUENT        300

static DEFINE_MUTEX(spi_mutex);
static DEFINE_MUTEX(dev_mutex);

enum sop_type_e {
	sop_address = 0,
	sop_broadcast,
	sop_readback,
	sop_dimming,
	sop_config,
	sop_feedback,
	sop_max,
};

struct abcon_spb_s {
	unsigned int dev_on_flag;
	unsigned short vsync_cnt;
	unsigned char bl_pwm_en;	/*0:default uboot have pwm, 1: uboot without pwm*/

	unsigned char act_lane_num;
	unsigned char dual_wire;
	unsigned char default_value;
	unsigned char dis_dip_combined;
	unsigned int fb_cnt;
	unsigned int fb_high_cnt_th;
	unsigned char sop_type;
	unsigned char dip_mode;
};

struct abcon_spb_s *bl_spb;

struct spb_packet_s {
	//unsigned int packet_header_len;
	//unsigned int packet_header[6];

	unsigned int preamble_header;
	unsigned int preamble_data[2];

	unsigned int sop_header;
	unsigned int sop_data;

	unsigned int addr_header;
	unsigned int addr_data;

	unsigned int data_header;
	unsigned int data_data;

	unsigned int crc_header;
	unsigned int crc_data;

	unsigned int eop_header;
	unsigned int eop_data;

	unsigned int pad_header;
	unsigned int pad_data;
};

struct spb_packet_s spb_packet;

void ldim_abcon_spb_write_packet(void)
{
	unsigned int len = 0;
	unsigned int *buf;
	unsigned char i = 0;
	unsigned char dix = 0;
	unsigned char act_lane_num = abcon->act_lane_num;

	if (bl_spb->sop_type == sop_dimming)
		buf = abcon_mem.ldc_seg;
	else
		buf = abcon_mem.wseg;

	if (!buf)
		return;

	//for autotrans dimming
	if (bl_spb->sop_type == sop_dimming) {
		if (bl_spb->dip_mode == 0 && bl_spb->dis_dip_combined == 1)
			dix = 0;
		else
			dix = 1;

		if (abcon->chip_type == ABCON_CHIP_T6W) {
			buf[len++] = 0x40 |
						(dix << 7) |
						(abcon->tx_scale << 9) |
						(bl_spb->dip_mode << 8) |
						abcon->act_lane;
		} else {
			buf[len++] = 0xffffc000;
			buf[len++] = 0xffff1000 | abcon->act_lane;
			buf[len++] = 0x10000 |
						(abcon->tx_scale << 19) |
						(bl_spb->dip_mode << 18) |
						(dix << 17) |
						abcon->max_lane_ch;
		}
		abcon->autotrans_ready = 1;
	}

	if (((bl_spb->sop_type == sop_address || bl_spb->sop_type == sop_readback) &&
		bl_spb->dual_wire == 1) ||
		bl_spb->dual_wire == 0 ||
		bl_spb->dip_mode == 0) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = spb_packet.preamble_header;

		for (i = 0; i < act_lane_num; i++) {
			buf[len++] = spb_packet.preamble_data[0];
			buf[len++] = spb_packet.preamble_data[1];
		}
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.sop_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.sop_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.addr_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.addr_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.data_header;

	if (bl_spb->sop_type != sop_dimming) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = spb_packet.data_data;
	}

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.crc_header;

	//for (i = 0; i < act_lane_num; i++)
	//	buf[len++] = spb_packet.crc_data;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.eop_header;

	for (i = 0; i < act_lane_num; i++)
		buf[len++] = spb_packet.eop_data;

	if ((bl_spb->sop_type == sop_broadcast ||
		bl_spb->sop_type == sop_config ||
		bl_spb->sop_type == sop_dimming) &&
		bl_spb->dual_wire == 1 &&
		bl_spb->dip_mode == 1) {
		for (i = 0; i < act_lane_num; i++)
			buf[len++] = spb_packet.pad_header;

		for (i = 0; i < act_lane_num; i++)
			buf[len++] = spb_packet.pad_data;
	}

	if (bl_spb->sop_type == sop_dimming) {
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

void ldim_abcon_spb_read_packet(void)
{
	unsigned int len = 0;
	unsigned int *buf = abcon_mem.rseg;

	if (!buf)
		return;

	buf[len++] = spb_packet.preamble_header;

	buf[len++] = spb_packet.preamble_data[0];
	buf[len++] = spb_packet.preamble_data[1];

	buf[len++] = spb_packet.sop_header;
	buf[len++] = spb_packet.sop_data;

	buf[len++] = spb_packet.addr_header;
	buf[len++] = spb_packet.addr_data;

	buf[len++] = spb_packet.data_header;
	//buf[len++] = spb_packet.data_data;

	buf[len++] = spb_packet.crc_header;
	//buf[len++] = spb_packet.crc_data;

	buf[len++] = spb_packet.eop_header;
	buf[len++] = spb_packet.eop_data;

	//reg_abcon_sw_rseg_max_num
	abcon_wr_reg_bits(0x05, len, 16, 13);
}

void ldim_abcon_spb_broadcast(unsigned int regaddr, unsigned int value)
{
	bl_spb->sop_type = sop_broadcast;

	//sw reset
	ldim_abcon_swrst();

	//reg_abcon_dual_line_mode
	if (bl_spb->dip_mode)
		abcon_wr_reg_bits(0x03, 1, 4, 1);//dual wire
	else
		abcon_wr_reg_bits(0x03, 0, 4, 1);//single wire

	//reg_abcon_dix_mode
	if (bl_spb->dis_dip_combined && bl_spb->dual_wire == 0)
		abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis*/
	else
		abcon_wr_reg_bits(0x03, 1, 5, 1);/*1:dip*/

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);
	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x9a9a, 16, 16);

	if (bl_spb->dual_wire && bl_spb->dip_mode) {
		//spb_packet.preamble_header = 0x70006082;
		//spb_packet.preamble_data[0] = 0x55555555;
		//spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x50002030;
		spb_packet.sop_data = 0x000c1f07;

		spb_packet.addr_header = 0x40002060;
		spb_packet.addr_data = (regaddr & 0x7f) << 8;

		spb_packet.data_header = 0x40002060;
		spb_packet.data_data = value;

		spb_packet.crc_header = 0x40002044;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002000;
		spb_packet.eop_data = 0x0000000d;

		spb_packet.pad_header = 0x3c002008;
		spb_packet.pad_data = 0x00000000;
	} else {
		spb_packet.preamble_header = 0x70006082;
		spb_packet.preamble_data[0] = 0x55555555;
		spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x500020b0;
		spb_packet.sop_data = 0x000c1f07;

		spb_packet.addr_header = 0x400020e0;
		spb_packet.addr_data = (regaddr & 0x7f) << 8;

		spb_packet.data_header = 0x400020e0;
		spb_packet.data_data = value;

		spb_packet.crc_header = 0x400020c4;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002088;
		spb_packet.eop_data = 0x0000000d;
	}

	ldim_abcon_spb_write_packet();
	usleep_range(1500, 2000);
}

void ldim_abcon_spb_config(unsigned int lane, unsigned int dim,
	unsigned int regaddr, unsigned int value)
{
	bl_spb->sop_type = sop_config;

	//sw reset
	ldim_abcon_swrst();

	//reg_abcon_dual_line_mode
	if (bl_spb->dip_mode)
		abcon_wr_reg_bits(0x03, 1, 4, 1);//dual wire
	else
		abcon_wr_reg_bits(0x03, 0, 4, 1);//single wire

	//reg_abcon_dix_mode
	if (bl_spb->dis_dip_combined && bl_spb->dual_wire == 0)
		abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis*/
	else
		abcon_wr_reg_bits(0x03, 1, 5, 1);/*1:dip*/

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, 1, 10, 4);//config write 1 lane each

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, 1 << lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, 1 << lane, 0, 12);
	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x955a, 16, 16);

	if (bl_spb->dual_wire && bl_spb->dip_mode) {
		//spb_packet.preamble_header = 0x70006082;
		//spb_packet.preamble_data[0] = 0x55555555;
		//spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x50002030;
		spb_packet.sop_data = 0x000c6727;

		spb_packet.addr_header = 0x40002060;
		spb_packet.addr_data = ((regaddr & 0x7f) << 8) | (dim & 0xff);

		spb_packet.data_header = 0x40002060;
		spb_packet.data_data = value;

		spb_packet.crc_header = 0x40002044;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002000;
		spb_packet.eop_data = 0x0000000d;

		spb_packet.pad_header = 0x3c002008;
		spb_packet.pad_data = 0x00000000;
	} else {
		spb_packet.preamble_header = 0x70006082;
		spb_packet.preamble_data[0] = 0x55555555;
		spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x500020b0;
		spb_packet.sop_data = 0x000c6727;

		spb_packet.addr_header = 0x400020e0;
		spb_packet.addr_data = ((regaddr & 0x7f) << 8) | (dim & 0xff);

		spb_packet.data_header = 0x400020e0;
		spb_packet.data_data = value;

		spb_packet.crc_header = 0x400020c4;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002088;
		spb_packet.eop_data = 0x0000000d;
	}

	ldim_abcon_spb_write_packet();
	usleep_range(1500, 2000);
}

void ldim_abcon_spb_address(void)
{
	bl_spb->sop_type = sop_address;

	//sw reset
	ldim_abcon_swrst();

	//reg_abcon_dual_line_mode
	abcon_wr_reg_bits(0x03, 0, 4, 1);//force single wire mode

	//reg_abcon_dix_mode
	if (bl_spb->dis_dip_combined && bl_spb->dual_wire == 1)
		abcon_wr_reg_bits(0x03, 1, 5, 1);/*1:dip*/
	else
		abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis*/

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);
	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x959f, 16, 16);

	spb_packet.preamble_header = 0x70006082;
	spb_packet.preamble_data[0] = 0x55555555;
	spb_packet.preamble_data[1] = 0x05555555;

	spb_packet.sop_header = 0x500020b0;
	spb_packet.sop_data = 0x000c6711;

	if (bl_spb->dual_wire)
		bl_spb->dip_mode = 1;
	spb_packet.addr_header = 0x400020e0;
	spb_packet.addr_data = 0x00000001 | (bl_spb->dip_mode << 14);

	spb_packet.data_header = 0x400020e0;
	spb_packet.data_data = 0;

	spb_packet.crc_header = 0x400020c4;//hw_crc_en, without crc_data
	//spb_packet.crc_data = 0x400020c4;

	spb_packet.eop_header = 0x14002088;
	spb_packet.eop_data = 0x0000000d;

	ldim_abcon_spb_write_packet();
	usleep_range(1500, 2000);
}

void ldim_abcon_spb_dimming(void)
{
	bl_spb->sop_type = sop_dimming;

	//reg_abcon_dual_line_mode
	if (bl_spb->dip_mode)
		abcon_wr_reg_bits(0x03, 1, 4, 1);//dual wire
	else
		abcon_wr_reg_bits(0x03, 0, 4, 1);//single wire

	//reg_abcon_dix_mode
	if (bl_spb->dis_dip_combined && bl_spb->dual_wire == 0)
		abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis*/
	else
		abcon_wr_reg_bits(0x03, 1, 5, 1);/*1:dip*/

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, abcon->act_lane, 22, 4);
	else
		abcon_wr_reg_bits(0x29, abcon->act_lane, 0, 12);
	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x999f, 16, 16);

	if (bl_spb->dual_wire && bl_spb->dip_mode) {
		//spb_packet.preamble_header = 0x70006082;
		//spb_packet.preamble_data[0] = 0x55555555;
		//spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x50002030;
		spb_packet.sop_data = 0x000c6311;

		spb_packet.addr_header = 0x40002060;
		spb_packet.addr_data = 0x00002001;

		spb_packet.data_header = 0x00001061 | ((abcon->max_lane_ch & 0xfff) << 14);
		//spb_packet.data_data = value;

		spb_packet.crc_header = 0x40002044;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002000;
		spb_packet.eop_data = 0x0000000d;

		spb_packet.pad_header = 0x3c002008;
		spb_packet.pad_data = 0x00000000;
	} else {
		spb_packet.preamble_header = 0x70006082;
		spb_packet.preamble_data[0] = 0x55555555;
		spb_packet.preamble_data[1] = 0x05555555;

		spb_packet.sop_header = 0x500020b0;
		spb_packet.sop_data = 0x000c6311;

		spb_packet.addr_header = 0x400020e0;
		spb_packet.addr_data = 0x00002001;

		spb_packet.data_header = 0x10e1 | ((abcon->max_lane_ch & 0xfff) << 14);
		//spb_packet.data_data = value;

		spb_packet.crc_header = 0x400020c4;//hw_crc_en, without crc_data
		//spb_packet.crc_data = 0x400020c4;

		spb_packet.eop_header = 0x14002088;
		spb_packet.eop_data = 0x0000000d;
	}

	ldim_abcon_spb_write_packet();
}

unsigned int ldim_abcon_spb_readback(unsigned char reg, unsigned char chip)
{
	unsigned int readback_val = 0;

	bl_spb->sop_type = sop_readback;

	//reg_abcon_crc_value
	abcon_wr_reg_bits(0x10, 0x5559, 16, 16);

	//reg_abcon_dual_line_mode
	abcon_wr_reg_bits(0x03, 0, 4, 1);//force single wire mode

	//reg_abcon_dix_mode
	if (bl_spb->dis_dip_combined && bl_spb->dual_wire == 1)
		abcon_wr_reg_bits(0x03, 1, 5, 1);/*1:dip*/
	else
		abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis*/

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, 1, 10, 4);// fix 1 lane
	//reg_abcon_act_lane
	if (abcon->chip_type == ABCON_CHIP_T6W)
		abcon_wr_reg_bits(0x2a, 1, 22, 4);//fix use lane 0
	else
		abcon_wr_reg_bits(0x29, 1, 0, 12);

	//prepare read packet first
	spb_packet.preamble_header = 0xe000c105;
	spb_packet.preamble_data[0] = 0x55555555;
	spb_packet.preamble_data[1] = 0x05555555;

	spb_packet.sop_header = 0xa0004141;
	spb_packet.sop_data = 0x000ce738;

	spb_packet.addr_header = 0x800041c1;
	spb_packet.addr_data = ((reg & 0x7f) << 8) | (chip & 0xff);

	spb_packet.data_header = 0x800041c2;//without seg_check_en
	//spb_packet.data_data = data;

	spb_packet.crc_header = 0x80004188;
	//spb_packet.crc_data = 0x400020c4;

	spb_packet.eop_header = 0x28004111;
	spb_packet.eop_data = 0x0000000d;
	ldim_abcon_spb_read_packet();

	//then set write packet
	spb_packet.preamble_header = 0x70006082;
	spb_packet.preamble_data[0] = 0x55555555;
	spb_packet.preamble_data[1] = 0x05555555;

	spb_packet.sop_header = 0x500020b0;
	spb_packet.sop_data = 0x000ce738;

	spb_packet.addr_header = 0x400020e0;
	spb_packet.addr_data = ((reg & 0x7f) << 8) | (chip & 0xff);

	spb_packet.data_header = 0x400020e0;
	spb_packet.data_data = 0;

	spb_packet.crc_header = 0x400020c4;//hw_crc_en, without crc_data
	//spb_packet.crc_data = 0x400020c4;

	spb_packet.eop_header = 0x14002088;
	spb_packet.eop_data = 0x0000000d;

	ldim_abcon_spb_write_packet();
	usleep_range(500, 1000);
	readback_val = abcon_rd_reg(0x52);

	return readback_val;
}

void ldim_abcon_spb_init_feedback(void)
{
	bl_spb->sop_type = sop_feedback;
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
	abcon_wr_reg_bits(0x39, bl_spb->fb_cnt, 16, 16);
}

void ldim_abcon_spb_get_feedback(void)
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
		if (fb_cnt[i] > bl_spb->fb_high_cnt_th)
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

void ldim_abcon_init_registers_for_spb(void)
{
	//reg_abcon_clr_done_flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);

	//reg_abcon_wire_mode
	if (bl_spb->dual_wire) {
		if (bl_spb->dis_dip_combined)
			abcon_wr_reg_bits(0x22, 1, 30, 2);
		else
			abcon_wr_reg_bits(0x22, 2, 30, 2);
	} else {
		if (bl_spb->dis_dip_combined)
			abcon_wr_reg_bits(0x22, 0, 30, 2);
		else
			abcon_wr_reg_bits(0x22, 2, 30, 2);
	}

	//reg_frc_crc_val_en
	abcon_wr_reg_bits(0x03, 1, 0, 1);
	//reg_abcon_rx_scale_sel
	abcon_wr_reg_bits(0x03, 1, 1, 1);
	//reg_abcon_hw_crc
	abcon_wr_reg_bits(0x03, 1, 3, 1);
	//reg_abcon_dual_line_mode
	abcon_wr_reg_bits(0x03, bl_spb->dual_wire, 4, 1);
	//reg_abcon_dix_mode
	abcon_wr_reg_bits(0x03, 0, 5, 1);/*0:dis, 1:dip*/
	//reg_abcon_default_value
	abcon_wr_reg_bits(0x03, bl_spb->default_value, 8, 1);
	//reg_abcon_rx_preamble_num
	abcon_wr_reg_bits(0x03, 60, 24, 8);
	//reg_abcon_crc_poly
	abcon_wr_reg(0x0d, 0x00008005);
	//reg_abcon_crc_init
	abcon_wr_reg(0x0e, 0xffffffff);
	//reg_abcon_crc_xorout
	//abcon_wr_reg(0x0f, 0xffffffff);

	//abcon_wr_reg(0x10, 0x999f0000);
	//abcon_wr_reg(0x11, 0x70f081ff);

	abcon_wr_reg(0x1a, 0x7f000000);
	abcon_wr_reg(0x1b, 0x20c32940);//6dc+10pwm	//todo

	//init feedback registers
	ldim_abcon_spb_init_feedback();
}

void ldim_abcon_init_spb(void)
{
	char i;

	//set dos as communications
	ldim_abcon_spb_broadcast(0x44, 0x8000);

	//send address command
	for (i = 0; i < 3; i++)
		ldim_abcon_spb_address();

	//unlock
	for (i = 0; i < 3; i++) {
		ldim_abcon_spb_broadcast(0x78, 0xa5c9);
		ldim_abcon_spb_broadcast(0x7c, 0xa5c9);
	}

	//dos clk
	ldim_abcon_spb_broadcast(0x5b, 0x0021);
	if (bl_spb->dis_dip_combined)
		ldim_abcon_spb_config(0, 1, 0x5b, 0x0121);

	//set iset range
	ldim_abcon_spb_broadcast(0x48, 0x6037);

	//dis dither
	ldim_abcon_spb_broadcast(0x4a, 0x0099);

	//en dos
	ldim_abcon_spb_broadcast(0x4c, 0x0029);

	//set pwm clk,
	ldim_abcon_spb_broadcast(0x4e, 0x9e07);

	//headroom
	ldim_abcon_spb_broadcast(0x5a, 0x0001);

	//ch enable
	ldim_abcon_spb_broadcast(0x40, 0x000f);

	//config done, sic ref
	ldim_abcon_spb_broadcast(0x46, 0x8842);

	//clr error flag
	ldim_abcon_spb_broadcast(0x60, 0x001f);

	//dos sick feedback, dimming mode
	ldim_abcon_spb_broadcast(0x44, 0xa100);
}

static int abcon_spb_hw_init_on(struct ldim_dev_driver_s *dev_drv)
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

	//init abcon register for spb
	ldim_abcon_init_registers_for_spb();

	//init spb registers
	ldim_abcon_init_spb();
	bl_delay_ms(1);

	ldim_abcon_swrst();

	return 0;
}

static int abcon_spb_hw_init_off(struct ldim_dev_driver_s *dev_drv)
{
	ldim_gpio_set(dev_drv, dev_drv->en_gpio, dev_drv->en_gpio_off);
	dev_drv->pinmux_ctrl(dev_drv, 0);
	ldim_pwm_off(&dev_drv->ldim_pwm_config);
	ldim_pwm_off(&dev_drv->analog_pwm_config);
	abcon->autotrans_ready = 0;
	bl_spb->dip_mode = 0;

	return 0;
}

static int abcon_spb_transmit(struct aml_ldim_driver_s *ldim_drv, unsigned int *buf,
		      unsigned int len)
{
	int ret = 0;

	if (!bl_spb)
		return -1;

	if (bl_spb->vsync_cnt++ >= VSYNC_INFO_FREQUENT)
		bl_spb->vsync_cnt = 0;

	if (bl_spb->dev_on_flag == 0) {
		if (bl_spb->vsync_cnt == 0)
			ABCONPR("%s: on_flag=%d\n", __func__, bl_spb->dev_on_flag);
		return 0;
	}

	ldim_abcon_swduty_set(ldim_drv);

	if (abcon->autotrans_ready == 0)
		ldim_abcon_spb_dimming();

	return ret;
}

static int abcon_spb_transmit_dummy(struct aml_ldim_driver_s *ldim_drv)
{
	return 0;
}

static int abcon_spb_fb_handle(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	if (bl_spb->dev_on_flag == 0)
		return 0;

	if (abcon->fb_det_cnt++ < abcon->conf.fb_det_int)
		return 0;
	abcon->fb_det_cnt = 0;

	ldim_abcon_spb_get_feedback();
	ldim_abcon_fb_pwm(ldim_drv);

	return ret;
}

static int abcon_spb_config_update(struct aml_ldim_driver_s *ldim_drv)
{
	int ret = 0;

	ABCONPR("%s: func_en = %d\n", __func__, ldim_drv->func_en);
	return ret;
}

static int abcon_spb_power_on(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_spb)
		return -1;
	if (bl_spb->dev_on_flag) {
		ABCONPR("%s: abcon_spb is already on, exit\n", __func__);
		return 0;
	}

	mutex_lock(&dev_mutex);
	abcon_spb_hw_init_on(ldim_drv->dev_drv);
	bl_spb->dev_on_flag = 1;
	bl_spb->vsync_cnt = 0;
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static int abcon_spb_power_off(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_spb)
		return -1;

	mutex_lock(&dev_mutex);
	bl_spb->dev_on_flag = 0;
	abcon->autotrans_ready = 0;
	abcon_spb_hw_init_off(ldim_drv->dev_drv);
	mutex_unlock(&dev_mutex);

	ABCONPR("%s: ok\n", __func__);
	return 0;
}

static ssize_t abcon_spb_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv;
	int len = 0;

	if (!bl_spb)
		return sprintf(buf, "bl_spb is null\n");
	dev_drv = ldim_drv->dev_drv;

	if (attr && attr->attr.name && !strcmp(attr->attr.name, "abcon_spb_status")) {
		len = sprintf(buf, "abcon_spb status:\n"
				"dev_index      = %d\n"
				"on_flag        = %d\n"
				"vsync_cnt      = %d\n",
				dev_drv->index,
				bl_spb->dev_on_flag,
				bl_spb->vsync_cnt);
	} else {
		return sprintf(buf, "invalid node\n");
	}

	return len;
}

static ssize_t abcon_spb_store(const struct class *class, const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	struct aml_ldim_driver_s *ldim_drv = aml_ldim_get_driver();
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int val, val1, val2;
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
		goto abcon_spb_store_end;

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
		abcon_spb_hw_init_on(dev_drv);
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "init_com")) {
		mutex_lock(&dev_mutex);
		//init abcon common
		ldim_abcon_init_common_registers(abcon);
		//init abcon register for spb
		ldim_abcon_init_registers_for_spb();
		mutex_unlock(&dev_mutex);
	} else if (!strcmp(parm[0], "readback")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_spb_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_spb_store_err;
			val2 = ldim_abcon_spb_readback((unsigned char)val, (unsigned char)val1);

			ABCONPR("readback reg=0x%x, chip=0x%x, val=0x%08x\n", val, val1, val2);
		}
	} else if (!strcmp(parm[0], "broadcast")) {
		if (parm[2]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_spb_store_err;
			if (kstrtouint(parm[2], 0, &val1) < 0)
				goto abcon_spb_store_err;
			ldim_abcon_spb_broadcast(val, val1);

			ABCONPR("broadcast reg=0x%x, val=0x%08x\n", val, val1);
		}
	} else if (!strcmp(parm[0], "address")) {
		ldim_abcon_spb_address();
	} else if (!strcmp(parm[0], "fb_high_cnt_th")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_spb_store_err;

			bl_spb->fb_high_cnt_th = val;
		}
		ABCONPR("fb_high_cnt_th =%d\n", bl_spb->fb_high_cnt_th);
	} else if (!strcmp(parm[0], "dual_wire")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_spb_store_err;

			bl_spb->dual_wire = val;
		}
		ABCONPR("dual_wire =%d\n", bl_spb->dual_wire);
	} else if (!strcmp(parm[0], "dis_dip_combined")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &val) < 0)
				goto abcon_spb_store_err;

			bl_spb->dis_dip_combined = val;
		}
		ABCONPR("dis_dip_combined =%d\n", bl_spb->dis_dip_combined);
	} else {
		ABCONERR("argument error!\n");
	}

abcon_spb_store_end:
	kfree(buf_orig);
	kfree(parm);
	return count;

abcon_spb_store_err:
	pr_info("invalid cmd!!!\n");
	kfree(buf_orig);
	kfree(parm);
	return count;
}

static struct class_attribute abcon_spb_class_attrs[] = {
	__ATTR(spb, 0644, NULL, abcon_spb_store),
	__ATTR(spb_status, 0644, abcon_spb_show, NULL),
};

static int abcon_spb_dev_update(struct ldim_dev_driver_s *dev_drv)
{
	dev_drv->power_on = abcon_spb_power_on;
	dev_drv->power_off = abcon_spb_power_off;
	dev_drv->dev_transmit = abcon_spb_transmit;
	dev_drv->dev_transmit_dummy = abcon_spb_transmit_dummy;
	dev_drv->dev_fb_handle = abcon_spb_fb_handle;
	dev_drv->config_update = abcon_spb_config_update;

	dev_drv->reg_write = NULL;
	dev_drv->reg_read = NULL;
	return 0;
}

int ldim_dev_abcon_spb_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct aml_bl_drv_s *bdrv = aml_bl_get_driver(0);
	int i;

	if (!dev_drv) {
		ABCONERR("%s: dev_drv is null\n", __func__);
		return -1;
	}

	bl_spb = kzalloc(sizeof(*bl_spb), GFP_KERNEL);
	if (!bl_spb) {
		ABCONERR("%s: bl_spb is error\n", __func__);
		return -1;
	}

	bl_spb->dev_on_flag = 0;
	bl_spb->vsync_cnt = 0;

	bl_spb->fb_cnt = 10;
	bl_spb->fb_high_cnt_th = 8;

	bl_spb->dual_wire = abcon->conf.ctrl & 0x1;
	bl_spb->default_value = (abcon->conf.ctrl >> 1) & 0x1;
	bl_spb->dis_dip_combined = (abcon->conf.ctrl >> 2) & 0x1;

	//default uboot set dip_mode already
	if (bl_spb->dual_wire)
		bl_spb->dip_mode = 1;

	//init abcon register for spb
	ldim_abcon_init_registers_for_spb();

	abcon_spb_dev_update(dev_drv);

	if (dev_drv->class) {
		for (i = 0; i < ARRAY_SIZE(abcon_spb_class_attrs); i++) {
			if (class_create_file(dev_drv->class, &abcon_spb_class_attrs[i])) {
				ABCONERR("create ldim_dev abcon spb class attribute %s fail\n",
					abcon_spb_class_attrs[i].attr.name);
			}
		}
	}
	if (bdrv->state & BL_STATE_BL_ON)
		bl_spb->dev_on_flag = 1; /* default enable in uboot */

	abcon_pr("%s ok\n", __func__);
	return 0;
}

int ldim_dev_abcon_spb_remove(struct aml_ldim_driver_s *ldim_drv)
{
	if (!bl_spb)
		return 0;

	kfree(bl_spb);
	bl_spb = NULL;

	return 0;
}
