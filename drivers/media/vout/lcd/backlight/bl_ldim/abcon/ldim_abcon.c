// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/amlogic/media/vout/lcd/lcd_resman.h>
#include <linux/amlogic/media/vout/lcd/aml_ldim.h>
#include "../../../lcd_reg.h"
#include "../ldim_reg.h"
#include "../ldim_drv.h"
#include "../ldim_dev_drv.h"
#include "ldim_abcon.h"
#include "ldim_abcon_reg.h"

struct abcon_mem_s abcon_mem;
struct abcon_s *abcon;

void ldim_abcon_swrst(void)
{
	//sw reset
	abcon_wr_reg_bits(0x00, 1, 5, 1);
	abcon_wr_reg_bits(0x00, 0, 5, 1);
	//clr done flag
	abcon_wr_reg_bits(0x00, 1, 4, 1);
	abcon_wr_reg_bits(0x00, 0, 4, 1);
}

int ldim_abcon_autotrans_start(void)
{
	if (abcon->autotrans_ready == 0) {
		ABCONPR("bcon autotrans_ready=0 !!\n");
		return 1;
	}

	if (abcon->chip_type == ABCON_CHIP_T6W) {
		//reg_abcon_hw_latch_en
		abcon_wr_reg_bits(0x40, 1, 16, 1);
	}

	//reg_abcon_pkt_header_en
	abcon_wr_reg_bits(0x00, 1, 6, 1);

	//reg_abcon_pkt_header_mode
	abcon_wr_reg_bits(0x06, 0, 20, 2);//packet header len = 1

	//reg_abcon_ldc_intr_on
	abcon_wr_reg_bits(0x02, 1, 16, 1);

	//reg_abcon_auto_trans_num
	abcon_wr_reg_bits(0x2a, 1, 10, 1);

	//reg_abcon_dc_dat_src_sel
	abcon_wr_reg_bits(0x2c, 0, 28, 1);//sw control id

	//reg_abcon_duty_zone_src_sel
	abcon_wr_reg_bits(0x2c, 0, 29, 1);//sw control id

	if (abcon->chip_type == ABCON_CHIP_T6W) {
		//reg_abcon_hw_latch_en
		abcon_wr_reg_bits(0x40, 0, 16, 1);
	}

	//reg_abcon_auto_trans_en
	abcon_wr_reg_bits(0x2b, 1, 20, 1);

	if (ldim_debug_print & LDIM_DBG_PR_BCON)
		ABCONPR("bcon autotrans start!!\n");

	return 0;
}

int ldim_abcon_autotrans_stop(void)
{
	//reg_abcon_pkt_header_en
	abcon_wr_reg_bits(0x00, 0, 6, 1);

	//reg_abcon_auto_trans_en
	abcon_wr_reg_bits(0x2b, 0, 20, 1);

	if (ldim_debug_print & LDIM_DBG_PR_BCON)
		ABCONPR("bcon autotrans stop!!\n");

	return 0;
}

//todo: consider boost mode, glb cur or local cur
void ldim_abcon_swduty_set(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	unsigned int *bl_matrix = ldim_drv->bl_matrix_cur;
	unsigned char *buf, *p;
	unsigned int fid;
	unsigned int row, col, n, zone_num, index;
	unsigned int temp = 0;
	int i, j, k, m, ext;
	unsigned int rowbuf, duty_size;
	unsigned int dc = 0;

	if (!dev_drv) {
		ABCONERR("%s: dev_drv buf is null\n", __func__);
		return;
	}

	if (!bl_matrix) {
		ABCONERR("%s: bl_matrix buf is null\n", __func__);
		return;
	}

	if (!abcon_mem.swduty) {
		ABCONERR("%s: swduty_vaddr is null\n", __func__);
		return;
	}

	row = dev_drv->bl_row;
	col = dev_drv->bl_col;
	zone_num = row * col;

	if (abcon->test_dc != 0xffff) {
		dc = abcon->test_dc & 0x3ff;
	} else if (ldim_drv->init_on_flag == 0) {
		dc = 0;
		memset(bl_matrix, 0, zone_num * sizeof(unsigned int));
	} else {
		dc = dev_drv->boost_conf.i_cur & 0x3ff;
	}

	if (abcon->chip_type == ABCON_CHIP_T6W) {
		rowbuf = 0x100; //256 Bytes(16*128bit) per row as t3x
		duty_size = LDC_DUTY_SIZE_T6W;
	} else {
		rowbuf = 0x100;
		duty_size = LDC_DUTY_SIZE_T6X;
	}

	fid = abcon_mem.swduty_fid;
	buf = (unsigned char *)(abcon_mem.swduty + (duty_size * fid >> 2));

	n = 3; /* 3bytes 2 seg */
	for (i = 0; i < row; i++) {
		p = buf + (i * rowbuf);
		m = 0;
		for (j = 0; j < col; j += 2) {
			ext = j / 10;
			index = i * col + j;
			temp = bl_matrix[index] & 0xfff;
			if ((index + 1) < zone_num)
				temp |= ((bl_matrix[index + 1] & 0xfff) << 12);

			for (k = 0; k < n; k++)
				p[m * n + k + ext] = (temp >> (k * 8)) & 0xff;
			m++;
		}
	}

	if (abcon->chip_type == ABCON_CHIP_DEF) {
		//reg_abcon_load_ldc_duty_start
		abcon_wr_reg_bits(0x00, 1, 2, 1);
		//usleep_range(1, 2);
		abcon_wr_reg_bits(0x00, 0, 2, 1);
		//usleep_range(100, 102);
	}

	//reg_abcon_ldc_gen_duty_zone
	abcon_wr_reg_bits(0x2c, fid, 30, 2);
	//reg_abcon_dc_ldc_glb_apl
	abcon_wr_reg_bits(0x11, dc, 0, 10);
}

int ldim_abcon_mem_init(struct ldim_dev_driver_s *dev_drv)
{
	dma_addr_t paddr = 0;
	unsigned int *vaddr;
	unsigned int size;
	unsigned int ch_offset, ldc_seg_offset, sw_duty_offset, wseg_offset, rseg_offset;

	if (!dev_drv) {
		ABCONERR("%s:dev_drv null!\n", __func__);
		return -1;
	}

	if (abcon->chip_type == ABCON_CHIP_T6W) {
		size = ABCON_OFFSET_END_T6W;
		ch_offset = CH_MAPPING_OFFSET_T6W;
		ldc_seg_offset = LDC_SEG_OFFSET_T6W;
		sw_duty_offset = SW_DUTY_OFFSET_T6W;
		wseg_offset = WSEG_OFFSET_T6W;
		rseg_offset = RSEG_OFFSET_T6W;
	} else {
		size = ABCON_OFFSET_END_T6X;
		ch_offset = CH_MAPPING_OFFSET_T6X;
		ldc_seg_offset = LDC_SEG_OFFSET_T6X;
		sw_duty_offset = SW_DUTY_OFFSET_T6X;
		wseg_offset = WSEG_OFFSET_T6X;
		rseg_offset = RSEG_OFFSET_T6X;
	}

	vaddr = dma_alloc_coherent(dev_drv->dev, size, &paddr, GFP_KERNEL);
	if (!vaddr) {
		ABCONERR("%s:cannot alloc memory!\n", __func__);
		return -1;
	}

	ABCONPR("abcon_base paddr=0x%llx, vaddr = 0x%px, size=0x%x\n", (u64)paddr, vaddr, size);

	abcon_mem.base_paddr = paddr;
	abcon_mem.base_vaddr = vaddr;

	abcon_mem.ch_mapping_paddr = abcon_mem.base_paddr + ch_offset;
	abcon_mem.ldc_seg_paddr = abcon_mem.base_paddr + ldc_seg_offset;
	abcon_mem.swduty_paddr = abcon_mem.base_paddr + sw_duty_offset;
	abcon_mem.wseg_paddr = abcon_mem.base_paddr + wseg_offset;
	abcon_mem.rseg_paddr = abcon_mem.base_paddr + rseg_offset;

	abcon_mem.ch_mapping = abcon_mem.base_vaddr;
	abcon_mem.ldc_seg = abcon_mem.base_vaddr + (ldc_seg_offset >> 2);
	abcon_mem.swduty = abcon_mem.base_vaddr + (sw_duty_offset >> 2);
	abcon_mem.wseg = abcon_mem.base_vaddr + (wseg_offset >> 2);
	abcon_mem.rseg = abcon_mem.base_vaddr + (rseg_offset >> 2);
	abcon_mem.swduty_fid = 0;

	ABCONPR("abcon_base paddr=0x%llx, total size=0x%x\n"
		"ch_mapping_paddr=0x%llx, ch_mapping=0x%px\n"
		"ldc_seg_paddr=0x%llx, ldc_seg=0x%px\n"
		"swduty_paddr=0x%llx, swduty=0x%px\n"
		"wseg_paddr=0x%llx, wseg=0x%px\n"
		"rseg_paddr=0x%llx, rseg=0x%px\n",
		(u64)paddr, size,
		(u64)abcon_mem.ch_mapping_paddr, abcon_mem.ch_mapping,
		(u64)abcon_mem.ldc_seg_paddr, abcon_mem.ldc_seg,
		(u64)abcon_mem.swduty_paddr, abcon_mem.swduty,
		(u64)abcon_mem.wseg_paddr, abcon_mem.wseg,
		(u64)abcon_mem.rseg_paddr, abcon_mem.rseg);

	return 0;
}

void ldim_abcon_set_txrx_clk(unsigned int txclk, unsigned int rxclk)
{
	unsigned char bmc_enc;
	unsigned char enc_bit_num;
	unsigned int txscale, rxscale;

	switch (abcon->conf.dev_type) {
	case 0://hyasic
		bmc_enc = 1;
		enc_bit_num = 2;
		break;
	case 1:
	case 2://xsignal
	case 3://xianxin
		bmc_enc = 0;
		enc_bit_num = 4;
		break;
	default:
		bmc_enc = 1;
		enc_bit_num = 2;
		break;
	}

	if (txclk == 0)
		txclk = 1000; //default 1Mhz
	if (rxclk == 0)
		rxclk = 1000; //default 1Mhz
	txscale = 1000000 / (abcon->conf.tx_clk * enc_bit_num * 6);
	rxscale = 1000000 / (abcon->conf.rx_clk * enc_bit_num * 6);
	abcon->tx_scale = txscale;

	abcon_wr_reg_bits(0x05, bmc_enc, 31, 1);//reg_abcon_bmc_enc
	abcon_wr_reg_bits(0x05, bmc_enc, 13, 1);//reg_abcon_bmc_dec

	abcon_wr_reg_bits(0x16, enc_bit_num, 13, 3);//reg_abcon_enc_bit_num

	//reg_abcon_tx_sck_set
	abcon_wr_reg_bits(0x02, ((txscale / 2) - 1), 0, 8);

	abcon_wr_reg_bits(0x01, txscale, 0, 10);//reg_abcon_tx_scale
	abcon_wr_reg_bits(0x01, txscale, 20, 10);//reg_abcon_tx_scale0
	abcon_wr_reg_bits(0x17, txscale, 0, 10);//reg_abcon_tx_scale1
	abcon_wr_reg_bits(0x17, txscale, 10, 10);//reg_abcon_tx_scale2
	abcon_wr_reg_bits(0x17, txscale, 20, 10);//reg_abcon_tx_scale3

	abcon_wr_reg_bits(0x01, rxscale, 10, 10);//reg_abcon_rx_scale
	abcon_wr_reg_bits(0x18, rxscale, 11, 10);//reg_abcon_rx_scale0
	abcon_wr_reg_bits(0x18, rxscale, 21, 10);//reg_abcon_rx_scale1
	abcon_wr_reg_bits(0x19, rxscale, 0, 10);//reg_abcon_rx_scale2
	abcon_wr_reg_bits(0x19, rxscale, 10, 10);//reg_abcon_rx_scale3
}

void ldim_abcon_set_gpio(struct abcon_s *abcon)
{
	if (abcon->chip_type == ABCON_CHIP_T6W) {
		//reg_abcon_gpio_o_sel0
		abcon_wr_reg_bits(0x25, abcon->conf.gpio_o[0], 0, 32);
		//reg_abcon_gpio_o_sel1
		abcon_wr_reg_bits(0x26, abcon->conf.gpio_o[1], 0, 16);

		//reg_abcon_gpio_i_sel0
		abcon_wr_reg_bits(0x23, abcon->conf.gpio_i[0], 0, 32);
		//reg_abcon_gpio_i_sel1
		abcon_wr_reg_bits(0x24, abcon->conf.gpio_i[1], 0, 16);
	} else {
		//reg_abcon_gpio_o_sel0
		abcon_wr_reg_bits(0x25, abcon->conf.gpio_o[0], 0, 29);
		//reg_abcon_gpio_o_sel1
		abcon_wr_reg_bits(0x26, abcon->conf.gpio_o[1], 0, 29);
		//reg_abcon_gpio_o_sel2
		abcon_wr_reg_bits(0x4a, abcon->conf.gpio_o[2], 0, 29);

		//reg_abcon_gpio_i_sel0
		abcon_wr_reg_bits(0x23, abcon->conf.gpio_i[0], 0, 29);
		//reg_abcon_gpio_i_sel1
		abcon_wr_reg_bits(0x49, abcon->conf.gpio_i[1], 0, 29);
		//reg_abcon_gpio_i_sel2
		abcon_wr_reg_bits(0x4b, abcon->conf.gpio_i[2], 0, 29);
	}
}

void ldim_abcon_set_channel(struct abcon_s *abcon)
{
	unsigned int temp = 0;

	if (abcon->chip_type == ABCON_CHIP_T6W) {
		//reg_abcon_abcon_total_chnl_num
		abcon_wr_reg_bits(0x05, abcon->tot_ch_num, 0, 13);

		//reg_abcon_lane0_chnl_num
		abcon_wr_reg_bits(0x12, abcon->lane_ch[0], 0, 11);
		//reg_abcon_lane1_chnl_num
		abcon_wr_reg_bits(0x12, abcon->lane_ch[1], 16, 11);
		//reg_abcon_lane2_chnl_num
		abcon_wr_reg_bits(0x13, abcon->lane_ch[2], 0, 11);
		//reg_abcon_lane3_chnl_num
		abcon_wr_reg_bits(0x13, abcon->lane_ch[3], 16, 11);

		//reg_abcon_max_lane_chnl_num
		if (abcon->conf.dev_type == 3)
			abcon_wr_reg_bits(0x18, abcon->conf.ch_num, 0, 11);//xianxin special
		else
			abcon_wr_reg_bits(0x18, abcon->max_lane_ch, 0, 11);
	} else {
		temp = 0;
		//reg_abcon_lane0_chnl_num
		abcon_wr_reg_bits(0x12, temp, 0, 13);
		temp = abcon->lane_ch[0];
		//reg_abcon_lane1_chnl_num
		abcon_wr_reg_bits(0x12, temp, 16, 13);
		temp += abcon->lane_ch[1];
		//reg_abcon_lane2_chnl_num
		abcon_wr_reg_bits(0x13, temp, 0, 13);
		temp += abcon->lane_ch[2];
		//reg_abcon_lane3_chnl_num
		abcon_wr_reg_bits(0x13, temp, 16, 13);
		temp += abcon->lane_ch[3];
		//reg_abcon_lane4_chnl_num
		abcon_wr_reg_bits(0x14, temp, 0, 13);
		temp += abcon->lane_ch[4];
		//reg_abcon_lane5_chnl_num
		abcon_wr_reg_bits(0x14, temp, 16, 13);
		temp += abcon->lane_ch[5];
		//reg_abcon_lane6_chnl_num
		abcon_wr_reg_bits(0x30, temp, 0, 13);
		temp += abcon->lane_ch[6];
		//reg_abcon_lane7_chnl_num
		abcon_wr_reg_bits(0x30, temp, 16, 13);
		temp += abcon->lane_ch[7];
		//reg_abcon_lane8_chnl_num
		abcon_wr_reg_bits(0x31, temp, 0, 13);
		temp += abcon->lane_ch[8];
		//reg_abcon_lane9_chnl_num
		abcon_wr_reg_bits(0x31, temp, 16, 13);
		temp += abcon->lane_ch[9];
		//reg_abcon_lane10_chnl_num
		abcon_wr_reg_bits(0x32, temp, 0, 13);
		temp += abcon->lane_ch[10];
		//reg_abcon_lane11_chnl_num
		abcon_wr_reg_bits(0x32, temp, 16, 13);

		//reg_abcon_max_lane_chnl_num
		if (abcon->conf.dev_type == 3)
			abcon_wr_reg_bits(0x05, abcon->conf.ch_num, 0, 13);//xianxin special
		else
			abcon_wr_reg_bits(0x05, abcon->max_lane_ch, 0, 13);
	}
}

void ldim_abcon_init_common_registers(struct abcon_s *abcon)
{
	if (abcon->chip_type == ABCON_CHIP_DEF) {
		//reg_abcon_duty_row_num
		abcon_wr_reg_bits(0x36, abcon->seg_row, 0, 7);
		//reg_abcon_duty_col_num
		abcon_wr_reg_bits(0x36, abcon->seg_col, 7, 7);
		//reg_abcon_bypass_dimming_format
		abcon_wr_reg_bits(0x29, 1, 14, 1);
	}

	//rdmif little endian
	abcon_wr_reg(0x20, 0x00000040);

	//reg_abcon_manu_num
	abcon_wr_reg_bits(0x03, abcon->conf.dev_type, 14, 4);

	//set txrx clk
	ldim_abcon_set_txrx_clk(abcon->conf.tx_clk, abcon->conf.rx_clk);

	//reg_abcon_mapping_table_baddr
	abcon_wr_reg(0x0a, abcon_mem.ch_mapping_paddr >> 2);

	//reg_abcon_duty_baddr
	abcon_wr_reg(0x07, abcon_mem.swduty_paddr >> 2);

	//reg_abcon_sw_wseg_baddr
	abcon_wr_reg(0x08, abcon_mem.wseg_paddr >> 2);
	//reg_abcon_ldc_seg_baddr
	abcon_wr_reg(0x09, abcon_mem.ldc_seg_paddr >> 2);

	//reg_abcon_sw_rseg_baddr
	abcon_wr_reg(0x15, abcon_mem.rseg_paddr >> 2);

	//reg_abcon_auto_trans_baddr
	abcon_wr_reg(0x28, abcon_mem.ldc_seg_paddr >> 2);

	//reg_abcon_act_lane_num
	abcon_wr_reg_bits(0x03, abcon->act_lane_num, 10, 4);

	//set gpio
	ldim_abcon_set_gpio(abcon);

	//set channel
	ldim_abcon_set_channel(abcon);

	if (abcon->chip_type == ABCON_CHIP_DEF) {
		//reg_abcon_duty_zone_size
		//LDC_DUTY_SIZE_T6X >> 2 = 0x4000 >> 2 = 0x1000
		//todo: according the actual size in use
		abcon_wr_reg_bits(0x35, 0x1000, 19, 13);
	} else {
		//reg_abcon_duty_zone_ofst_sft
		//0x800 = 1 << 11, baddr >> 2, 11-2 = 9, fixed size
		abcon_wr_reg_bits(0x35, 9, 20, 4);
	}
	//reg_abcon_duty_stride
	abcon_wr_reg_bits(0x0b, 64, 16, 13);

	//reg_abcon_sw_wseg_stride
	abcon_wr_reg_bits(0x0b, 0x1ffff, 0, 13);
	//reg_abcon_ldc_seg_stride
	abcon_wr_reg_bits(0x0c, 0x1ffff, 0, 13);
	//reg_abcon_mapping_stride
	abcon_wr_reg_bits(0x0c, 0x1ffff, 16, 13);
	//reg_abcon_sw_rseg_stride
	abcon_wr_reg_bits(0x16, 0x1ffff, 0, 13);
}

void ldim_abcon_load_ch_mapping(struct ldim_dev_driver_s *dev_drv, unsigned int len)
{
	if (dev_drv->bl_mapping && abcon_mem.ch_mapping) {
		//copy mapping data to ch_mapping buf
		memcpy(abcon_mem.ch_mapping, dev_drv->bl_mapping, len);

		if (abcon->chip_type == ABCON_CHIP_T6W) {
			//reg_abcon_load_mapping_start
			abcon_wr_reg_bits(0x00, 1, 2, 1);
			usleep_range(1, 2);
			abcon_wr_reg_bits(0x00, 0, 2, 1);
		}
		ABCONPR("abcon load ch mapping done, total size=0x%x\n", len);
		usleep_range(500, 1000);
	}
}

void ldim_abcon_fb_pwm(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	struct bl_pwm_config_s *bl_pwm = &dev_drv->ldim_pwm_config;

	if (abcon->conf.fb_en == 0)
		return;

	if (!bl_pwm)
		return;

	if (abcon->fb_pos_cnt > abcon->conf.fb_adj_th)
		abcon->fb_val = 1;//VLED need lower
	else if (abcon->fb_neg_cnt > abcon->conf.fb_adj_th)
		abcon->fb_val = 0;//VLED need higher
	else
		return;

	abcon->fb_pos_cnt = 0;
	abcon->fb_neg_cnt = 0;

	if (abcon->fb_val) {//VLED need lower
		if (abcon->conf.fb_pwm_dir)
			bl_pwm->pwm_duty += abcon->conf.fb_pwm_step;
		else
			bl_pwm->pwm_duty -= abcon->conf.fb_pwm_step;
	} else {//VLED need higher
		if (abcon->conf.fb_pwm_dir)
			bl_pwm->pwm_duty -= abcon->conf.fb_pwm_step;
		else
			bl_pwm->pwm_duty += abcon->conf.fb_pwm_step;
	}

	ldim_set_duty_pwm(bl_pwm);

	if (ldim_debug_print & LDIM_DBG_PR_BCON)
		ABCONPR("fb_adj_th:%d, fb_val:%d, fb_pwm_dir:%d, pwm_step:%d, pwm_duty:%d\n",
		abcon->conf.fb_adj_th, abcon->fb_val, abcon->conf.fb_pwm_dir,
		abcon->conf.fb_pwm_step, bl_pwm->pwm_duty);
}

int ldim_abcon_dev_probe(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;
	int ret = 0;
	unsigned int temp = 0;
	unsigned int i = 0;

	//disable ldim_pwm_vs irq
	free_irq(ldim_drv->ldim_pwm_vs_irq, (void *)&ldim_drv->ldim_pwm_vs_irq);

	abcon = kzalloc(sizeof(*abcon), GFP_KERNEL);
	if (!abcon) {
		LDIMERR("%s: abcon malloc failed\n", __func__);
		return -1;
	}

	ret = abcon_ioremap(dev_drv->pdev);
	if (ret)
		return -1;

	//abcon->conf = dev_drv->abcon_conf;
	memcpy(&abcon->conf, &dev_drv->abcon_conf, sizeof(struct abcon_conf_s));
	if (ldim_drv->data->ldc_chip_type == LDC_T6W)
		abcon->chip_type = ABCON_CHIP_T6W;
	else
		abcon->chip_type = ABCON_CHIP_DEF;
	abcon->seg_row = dev_drv->bl_row;
	abcon->seg_col = dev_drv->bl_col;
	ABCONPR("seg_row:col=%d:%d, chip_type=%d\n",
		abcon->seg_row, abcon->seg_col, abcon->chip_type);

	abcon->tot_ch_num = 0;
	abcon->act_lane = 0;
	abcon->act_lane_num = 0;
	abcon->max_lane_ch = 0;
	abcon->max_lane_dim = 0;
	abcon->fb_val = 0;
	abcon->fb_det_cnt = 0;
	abcon->fb_pos_cnt = 0;
	abcon->fb_neg_cnt = 0;
	abcon->test_dc = 0xffff;
	abcon->autotrans_ready = 0;

	for (i = 0; i < 12; i++) {
		abcon->lane_ch[i] = abcon->conf.ch_num * abcon->conf.chip_num[i];
		abcon->tot_ch_num += abcon->lane_ch[i];
		if (abcon->lane_ch[i] > abcon->max_lane_ch)
			abcon->max_lane_ch = abcon->lane_ch[i];
		if (abcon->lane_ch[i]) {
			abcon->act_lane_num += 1;
			abcon->act_lane |= 1 << i;
		}
	}
	abcon->max_lane_dim = abcon->max_lane_ch / abcon->conf.ch_num;

	ABCONPR("lane_ch:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d\n"
		"tot_ch_num:%d, act_lane:0x%x,act_lane_num:%d,max_lane_ch:%d,max_lane_dim=%d\n",
		abcon->lane_ch[0], abcon->lane_ch[1],
		abcon->lane_ch[2], abcon->lane_ch[3],
		abcon->lane_ch[4], abcon->lane_ch[5],
		abcon->lane_ch[6], abcon->lane_ch[7],
		abcon->lane_ch[8], abcon->lane_ch[9],
		abcon->lane_ch[10], abcon->lane_ch[11],
		abcon->tot_ch_num, abcon->act_lane, abcon->act_lane_num,
		abcon->max_lane_ch, abcon->max_lane_dim);

	//set abcon clk ctrl
	temp = (1 << 7) |	/*bcon clk_sel*/
			(1 << 6) |	/*mux0 en*/
			(0 << 0);	/*bcon clk_div*/
	abcon_clk_wr_reg(CLKCTRL_BCON_CLK_CNTL, temp);

	//sw rst
	ldim_abcon_swrst();

	//memory init
	ret =  ldim_abcon_mem_init(dev_drv);
	if (ret)
		return -1;

	/*init register*/
	ldim_abcon_init_common_registers(abcon);

	ldim_abcon_debug_probe(dev_drv);

	//for auto trans, need positive vs
	if (abcon->chip_type == ABCON_CHIP_T6W) {
		//reg_ambilight_vs_pol
		ldim_wr_reg_bits(LDC_AMBIGHT_STAT_NUM, 1, 11, 1);
	}

	dev_drv->dev_transmit_start = ldim_abcon_autotrans_start;
	dev_drv->dev_transmit_stop = ldim_abcon_autotrans_stop;

	return ret;
}

int ldim_abcon_dev_remove(struct aml_ldim_driver_s *ldim_drv)
{
	struct ldim_dev_driver_s *dev_drv = ldim_drv->dev_drv;

	ldim_abcon_debug_remove(dev_drv);
	return 0;
}
