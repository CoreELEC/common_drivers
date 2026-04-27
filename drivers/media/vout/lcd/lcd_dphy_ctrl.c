// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_reg.h"
#include "lcd_common.h"

static void lcd_lane_map_chip_init(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg)
{
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TXHD2:
	case LCD_CHIP_T6D:
	case LCD_CHIP_A9:
		phy_cfg->lane_offset = 0;
		phy_cfg->lane_mask = 0x3ff;
		break;
	case LCD_CHIP_T7:
		if (pdrv->index == 0) {
			phy_cfg->lane_offset = 0;
			phy_cfg->lane_mask = 0xffff;
		} else if (pdrv->index == 1) {
			phy_cfg->lane_offset = 8;
			phy_cfg->lane_mask = 0xff;
		} else {
			phy_cfg->lane_offset = 5;
			phy_cfg->lane_mask = 0x3ff;
		}
		break;
	case LCD_CHIP_T3X:
		if (pdrv->index == 0) {
			phy_cfg->lane_offset = 0;
			phy_cfg->lane_mask = 0xffff;
		} else if (pdrv->index == 1) {
			phy_cfg->lane_offset = 8;
			phy_cfg->lane_mask = 0xff;
		}
		break;
	case LCD_CHIP_T6X:
		phy_cfg->lane_offset = 0;
		phy_cfg->lane_mask = 0xffff;
		break;
	default: //common 12lane
		phy_cfg->lane_offset = 0;
		phy_cfg->lane_mask = 0xfff;
		break;
	}
}

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
static int lcd_lane_vbyone_ch_data_map(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
			unsigned char *data_map)
{
	unsigned char ch_slice[][8] = {
		{0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7},
		{0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf}
	};
	unsigned int lane_cnt, slice_cnt, lane_pre_slice;
	int ch_idx, slice_idx, i;

	//generate ch slice data map table
	lane_cnt = dev_p->dev_cfg.control.vbyone_cfg.lane_count;
	slice_cnt = dev_p->dev_cfg.control.vbyone_cfg.slice;
	lane_pre_slice = (lane_cnt + slice_cnt - 1) / slice_cnt;
	if (lane_cnt >= CH_LANE_MAX) {
		LCD_ERR(pdrv, "%s: vbyone lane_cnt %d err", __func__, lane_cnt);
		return -1;
	}

	for (i = 0; i < lane_cnt; i++) {
		slice_idx = i / lane_pre_slice;
		ch_idx = i % lane_pre_slice;
		if (slice_idx >= 2 || ch_idx >= 8) {
			LCD_ERR(pdrv, "%s: slice_idx %d or ch_idx %d err",
				__func__, slice_idx, ch_idx);
			return -1;
		}
		data_map[i] = ch_slice[slice_idx][ch_idx];
	}
	for (; i < CH_LANE_MAX; i++)
		data_map[i] = 0xff;

	return 0;
}
#endif

//ch_swap for reg
static void lcd_lane_sel_to_ch_swap(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct phy_config_s *phy_cfg = &dev_p->dev_cfg.phy_cfg;
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	unsigned char data_map[CH_LANE_MAX];
	int ret;
#endif
	unsigned int data_sel, valid_flag = 0, i, bit;

	phy_cfg->ch_swap0 = 0xffffffff;
	phy_cfg->ch_swap1 = 0xffffffff;
	switch (dev_p->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		ret = lcd_lane_vbyone_ch_data_map(pdrv, dev_p, data_map);
		if (ret)
			return;
		for (i = 0; i < phy_cfg->lane_num; i++) {
			if (phy_cfg->ch_ctrl[i].sel >= CH_LANE_MAX)
				data_sel = 0xff;
			else
				data_sel = data_map[phy_cfg->ch_ctrl[i].sel];
			if (data_sel == 0xff)
				data_sel = 0xf;
			else
				valid_flag |= (1 << i);
			if (i < 8) {
				bit = i * 4;
				phy_cfg->ch_swap0 &= ~(0xf << bit);
				phy_cfg->ch_swap0 |= data_sel << bit;
			} else if (i < 16) {
				bit = (i - 8) * 4;
				phy_cfg->ch_swap1 &= ~(0xf << bit);
				phy_cfg->ch_swap1 |= data_sel << bit;
			}
		}
		break;
#endif
	default:
		for (i = 0; i < phy_cfg->lane_num; i++) {
			data_sel = phy_cfg->ch_ctrl[i].sel;
			if (data_sel == 0xff)
				data_sel = 0xf;
			else
				valid_flag |= (1 << i);
			if (i < 8) {
				bit = i * 4;
				phy_cfg->ch_swap0 &= ~(0xf << bit);
				phy_cfg->ch_swap0 |= data_sel << bit;
			} else if (i < 16) {
				bit = (i - 8) * 4;
				phy_cfg->ch_swap1 &= ~(0xf << bit);
				phy_cfg->ch_swap1 |= data_sel << bit;
			}
		}
		break;
	}

	phy_cfg->lane_valid = (valid_flag & phy_cfg->lane_mask) << phy_cfg->lane_offset;
	LCD_DEV_DBG(pdrv, dev_p->idx, "%s: lane_num:%d, ch_swap:0x%08x,0x%08x, lane_valid:0x%x",
		__func__, phy_cfg->lane_num,
		phy_cfg->ch_swap0, phy_cfg->ch_swap1, phy_cfg->lane_valid);
}

static void lcd_ch_swap_to_lane_sel(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
		struct phy_config_s *phy_cfg)
{
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	unsigned char data_map[CH_LANE_MAX];
	int ret, j;
#endif
	unsigned int sel = 0xff, valid_flag = 0;
	unsigned int i, bit;

	switch (dev_p->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		ret = lcd_lane_vbyone_ch_data_map(pdrv, dev_p, data_map);
		if (ret)
			return;
		for (i = 0; i < phy_cfg->lane_num; i++) {
			if (i < 8) {
				bit = i * 4;
				sel = (phy_cfg->ch_swap0 >> bit) & 0xf;
			} else if (i < 16) {
				bit = (i - 8) * 4;
				sel = (phy_cfg->ch_swap1 >> bit) & 0xf;
			}

			phy_cfg->ch_ctrl[i].sel = 0xff;
			for (j = 0; j < CH_LANE_MAX; j++) {
				if (sel == data_map[j]) {
					phy_cfg->ch_ctrl[i].sel = j;
					valid_flag |= (1 << i);
					break;
				}
			}
		}
		break;
#endif
	default:
		for (i = 0; i < phy_cfg->lane_num; i++) {
			if (i < 8) {
				bit = i * 4;
				sel = (phy_cfg->ch_swap0 >> bit) & 0xf;
			} else if (i < 16) {
				bit = (i - 8) * 4;
				sel = (phy_cfg->ch_swap1 >> bit) & 0xf;
			}

			if (sel == 0xf) {
				phy_cfg->ch_ctrl[i].sel = 0xff;
			} else {
				phy_cfg->ch_ctrl[i].sel = sel;
				valid_flag |= (1 << i);
			}
		}
		break;
	}

	phy_cfg->lane_valid = (valid_flag & phy_cfg->lane_mask) << phy_cfg->lane_offset;
	LCD_DBG(pdrv, "%s: lane_num:%d, ch_swap:0x%08x,0x%08x, lane_valid:0x%x", __func__,
		phy_cfg->lane_num, phy_cfg->ch_swap0, phy_cfg->ch_swap1, phy_cfg->lane_valid);
}

#ifdef CONFIG_AMLOGIC_LCD_LVDS

struct lvds_valid_port_s {
	unsigned char s0;
	unsigned char e0;
	unsigned char s1;
	unsigned char e1;
};

static int lcd_lvds_get_valid_port_num(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
				       struct lvds_valid_port_s *valid_port)
{
	unsigned char lcd_bits;

	if (!valid_port)
		return -1;
	lcd_bits = dev_p->dev_cfg.timing.base_timing->lcd_bits;

	/* lvds swap */
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3X:
	case LCD_CHIP_T6X:
		if (lcd_bits == 30) {
			valid_port->s0 = 0;
			valid_port->e0 = 5;
			valid_port->s1 = 8;
			valid_port->e1 = 0xd;
		} else if (lcd_bits == 18) {
			valid_port->s0 = 0;
			valid_port->e0 = 3;
			valid_port->s1 = 8;
			valid_port->e1 = 0xb;
		} else { //24bit
			valid_port->s0 = 0;
			valid_port->e0 = 4;
			valid_port->s1 = 8;
			valid_port->e1 = 0xc;
		}
		break;
	default:
		if (lcd_bits == 30) {
			valid_port->s0 = 0;
			valid_port->e0 = 5;
			valid_port->s1 = 6;
			valid_port->e1 = 0xb;
		} else if (lcd_bits == 18) {
			valid_port->s0 = 0;
			valid_port->e0 = 3;
			valid_port->s1 = 6;
			valid_port->e1 = 9;
		} else { //24bit
			valid_port->s0 = 0;
			valid_port->e0 = 4;
			valid_port->s1 = 6;
			valid_port->e1 = 0xa;
		}
		break;
	}

	return 0;
}

static void lcd_lvds_lane_init(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
			unsigned int *map0, unsigned int *map1)
{
	struct phy_config_s *phy_cfg = &dev_p->dev_cfg.phy_cfg;
	struct lvds_valid_port_s valid_port;
	unsigned char temp, i, n;
	int ret;

	LCD_DBG(pdrv, "%s: [0x%8x][0x%8x]", __func__, map0, map1);

	// 12-12 channel:
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d4_a:5 d0_b:6 d1_b:7 d2_b:8 clk_b:9 d3_b:a d4_b:b
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a DIF_CH5:d4_a
	// DIF_CH6:d0_b DIF_CH7:d1_b DIF_CH8:d2_b DIF_CH9:clk_b DIF_CH10:d3_b DIF_CH11:d4_b
						// port_swap, lane_reverse
	unsigned int ch_map_6lane[2] = {0x76543210, 0xba98}; // 0, 0

	// 12-12 channel:
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d4_a:5 d0_b:6 d1_b:7 d2_b:8 clk_b:9 d3_b:a d4_b:b
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a
	// DIF_CH5:d0_b DIF_CH6:d1_b DIF_CH7:d2_b DIF_CH8:clk_b DIF_CH9:d3_b
	// DIF_CH10:d4_a/invalid  DIF_CH11:d4_b/invalid
	unsigned int ch_map_6_to_5lane[2] = {0x87643210, 0xb5a9}; // 0, 0

	// 10-12 channel: //2k chips
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d0_b:6 d1_b:7 d2_b:8 clk_b:9 d3_b:a
	// 5 & b: null
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a
	// DIF_CH5:d0_b DIF_CH6:d1_b DIF_CH7:d2_b DIF_CH8:clk_b DIF_CH9:d3_b
	unsigned int ch_map_5lane[2] = {0x87643210, 0xb5a9}; // 0, 0

	// 12-16 channel:
	// d0_a:0 d1_a:1 d2_a:2 clk_a:3 d3_a:4 d4_a:5 d0_b:8 d1_b:9 d2_b:a clk_b:b d3_b:c d4_b:d
	// DIF_CH0:d0_a DIF_CH1:d1_a DIF_CH2:d2_a DIF_CH3:clk_a DIF_CH4:d3_a DIF_CH5:d4_a
	// DIF_CH6:invalid DIF_CH7:invalid
	// DIF_CH8:d0_b DIF_CH9:d1_b DIF_CH10:d2_b DIF_CH11:clk_b DIF_CH12:d3_b DIF_CH13:d4_b
	// DIF_CH14:invalid DIF_CH15:invalid
	unsigned int ch_map_8_to_6lane[2] = {0x76543210, 0xfedcba98};  // 0, 0

	ret = lcd_lvds_get_valid_port_num(pdrv, dev_p, &valid_port);
	if (ret)
		return;

	/* lvds swap */
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_T5W:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T3:
	case LCD_CHIP_T6W:
		*map0 = ch_map_6lane[0];
		*map1 = ch_map_6lane[1];
		break;
	case LCD_CHIP_T5D:
	case LCD_CHIP_TXHD2:
	case LCD_CHIP_T6D:
	case LCD_CHIP_A9:
		*map0 = ch_map_5lane[0];
		*map1 = ch_map_5lane[1];
		break;
	case LCD_CHIP_T7:
		//don't support port_swap and lane_reverse when single port
		if (pdrv->index == 2 && dev_p->dev_cfg.control.lvds_cfg.dual_port) {
			*map0 = ch_map_6_to_5lane[0];
			*map1 = ch_map_6_to_5lane[1];
		} else if (pdrv->index == 1) {
			*map0 = 0xf43210ff;
			*map1 = 0xffff;
		} else { // (drv==2, single port) || drv==0
			*map0 = 0xfff43210;
			*map1 = 0xffff;
		}
		break;
	case LCD_CHIP_T3X:
	case LCD_CHIP_T6X:
		*map0 = ch_map_8_to_6lane[0];
		*map1 = ch_map_8_to_6lane[1];
		break;
	default:
		return;
	}

	LCD_DBG(pdrv, "%s: dual_port=%d, valid_port s0=0x%x, e0=0x%x, s1=0x%x, e1=0x%x\n",
		__func__, dev_p->dev_cfg.control.lvds_cfg.dual_port,
		valid_port.s0, valid_port.e0, valid_port.s1, valid_port.e1);
	for (i = 0; i < phy_cfg->lane_num; i++) {
		if (i < 8) {
			n = i * 4;
			temp = (*map0 >> n) & 0xf;
			if (temp >= valid_port.s0 && temp <= valid_port.e0)
				continue; //port0 valid lane
			if (dev_p->dev_cfg.control.lvds_cfg.dual_port &&
			    (temp >= valid_port.s1 && temp <= valid_port.e1)) {
				continue; //port1 valid lane
			}
			*map0 |= (0xf << n);
			continue;
		}
		if (i < 16) {
			n = (i - 8) * 4;
			temp = (*map1 >> n) & 0xf;
			if (temp >= valid_port.s0 && temp <= valid_port.e0)
				continue; //port0 valid lane
			if (dev_p->dev_cfg.control.lvds_cfg.dual_port &&
			    (temp >= valid_port.s1 && temp <= valid_port.e1)) {
				continue; //port1 valid lane
			}
			*map1 |= (0xf << n);
		}
	}
}

static void lcd_lvds_lane_swap(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct lvds_valid_port_s valid_port;
	struct phy_config_s *phy_cfg = &dev_p->dev_cfg.phy_cfg;
	unsigned char port_a[CH_LANE_MAX], port_b[CH_LANE_MAX], temp_port[CH_LANE_MAX];
	unsigned char temp, i, n;
	int ret;

	LCD_DBG(pdrv, "%s\n", __func__);

	ret = lcd_lvds_get_valid_port_num(pdrv, dev_p, &valid_port);
	if (ret)
		return;

	memset(port_a, 0xff, CH_LANE_MAX);
	memset(port_b, 0xff, CH_LANE_MAX);
	memset(temp_port, 0xff, CH_LANE_MAX);
	for (i = 0; i < phy_cfg->lane_num; i++) {
		if (phy_cfg->ch_ctrl[i].sel_dft >= valid_port.s0 &&
		    phy_cfg->ch_ctrl[i].sel_dft <= valid_port.e0) {
			n = phy_cfg->ch_ctrl[i].sel_dft - valid_port.s0;
			port_a[n] = i;
		} else if (phy_cfg->ch_ctrl[i].sel_dft >= valid_port.s1 &&
			   phy_cfg->ch_ctrl[i].sel_dft <= valid_port.e1) {
			n = phy_cfg->ch_ctrl[i].sel_dft - valid_port.s1;
			port_b[n] = i;
		}
	}

	if (dev_p->dev_cfg.control.lvds_cfg.port_swap) {
		for (i = 0; i < phy_cfg->lane_num; i++) {
			temp = port_a[i];
			port_a[i] = port_b[i];
			port_b[i] = temp;
		}
	}
	if (dev_p->dev_cfg.control.lvds_cfg.lane_reverse) {
		for (i = valid_port.s0; i <= valid_port.e0; i++)
			temp_port[valid_port.e0 - i] = port_a[i - valid_port.s0];
		for (i = valid_port.s0; i <= valid_port.e0; i++)
			port_a[i - valid_port.s0] = temp_port[i - valid_port.s0];

		for (i = valid_port.s1; i <= valid_port.e1; i++)
			temp_port[valid_port.e1 - i] = port_b[i - valid_port.s1];
		for (i = valid_port.s1; i <= valid_port.e1; i++)
			port_b[i - valid_port.s1] = temp_port[i - valid_port.s1];
	}

	for (i = valid_port.s0; i <= valid_port.e0; i++) {
		n = port_a[i - valid_port.s0];
		if (n >= CH_LANE_MAX)
			continue;
		phy_cfg->ch_ctrl[n].sel = i;
	}
	for (i = valid_port.s1; i <= valid_port.e1; i++) {
		n = port_b[i - valid_port.s1];
		if (n >= CH_LANE_MAX)
			continue;
		phy_cfg->ch_ctrl[n].sel = i;
	}
}
#endif

void lcd_mlvds_phy_ckdi_config(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct phy_config_s *phy_cfg = &dev_p->dev_cfg.phy_cfg;
	unsigned int clk_a_sel, clk_b_sel, pi_clk_sel = 0;
	int i;

	/* pi_clk select */
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TL1:
	case LCD_CHIP_TM2:
		/* mlvds channel:    //tx 12 channels
		 *    0: clk_a
		 *    1: d0_a
		 *    2: d1_a
		 *    3: d2_a
		 *    4: d3_a
		 *    5: d4_a
		 *    6: clk_b
		 *    7: d0_b
		 *    8: d1_b
		 *    9: d2_b
		 *   10: d3_b
		 *   11: d4_b
		 */
		clk_a_sel = 0x0;
		clk_b_sel = 0x6;
		break;
	default:
		/* mlvds channel:    //tx 8 channels
		 *    0: d0_a
		 *    1: d1_a
		 *    2: d2_a
		 *    3: clk_a
		 *    4: d0_b
		 *    5: d1_b
		 *    6: d2_b
		 *    7: clk_b
		 */
		clk_a_sel = 0x3;
		clk_b_sel = 0x7;
		break;
	}
	for (i = 0; i < phy_cfg->lane_num; i++) {
		if (phy_cfg->ch_ctrl[i].sel == clk_a_sel ||
		    phy_cfg->ch_ctrl[i].sel == clk_b_sel)
			pi_clk_sel |= (1 << i);
	}

	phy_cfg->ckdi = pi_clk_sel;
	LCD_DBG(pdrv, "mlvds ckdi=0x%03x", phy_cfg->ckdi);
}

void lcd_lane_map_preset(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct phy_config_s *phy_cfg;
	unsigned int lane_map0 = 0xffffffff, lane_map1 = 0xffffffff;
	unsigned int bit, sel;
	int i;

	if (!pdrv || !dev_p)
		return;
	phy_cfg = &dev_p->dev_cfg.phy_cfg;

	lcd_lane_map_chip_init(pdrv, phy_cfg);

	switch (dev_p->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_LVDS
	case LCD_LVDS:
		lcd_lvds_lane_init(pdrv, dev_p, &lane_map0, &lane_map1);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		if (dev_p->dev_cfg.control.vbyone_cfg.lane_count == 1) {
			lane_map0 = 0xfffffff0;
			lane_map1 = 0xffffffff;
		} else if (dev_p->dev_cfg.control.vbyone_cfg.lane_count == 2) {
			lane_map0 = 0xffffff10;
			lane_map1 = 0xffffffff;
		} else if (dev_p->dev_cfg.control.vbyone_cfg.lane_count == 4) {
			lane_map0 = 0xffff3210;
			lane_map1 = 0xffffffff;
		} else if (dev_p->dev_cfg.control.vbyone_cfg.lane_count == 16) {
			lane_map0 = 0x76543210;
			lane_map1 = 0xfedcba98;
		} else { //8lane
			lane_map0 = 0x76543210;
			lane_map1 = 0xffffffff;
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		lane_map0 = dev_p->dev_cfg.control.mlvds_cfg.channel_sel0;
		lane_map1 = dev_p->dev_cfg.control.mlvds_cfg.channel_sel1;
		break;
	case LCD_P2P:
		lane_map0 = dev_p->dev_cfg.control.p2p_cfg.channel_sel0;
		lane_map1 = dev_p->dev_cfg.control.p2p_cfg.channel_sel1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		if (dev_p->dev_cfg.control.mipi_cfg.lane_num == 1) {
			lane_map0 = 0xfffff2f0;
			lane_map1 = 0xffffffff;
		} else if (dev_p->dev_cfg.control.mipi_cfg.lane_num == 2) {
			lane_map0 = 0xfffff210;
			lane_map1 = 0xffffffff;
		} else if (dev_p->dev_cfg.control.mipi_cfg.lane_num == 3) {
			lane_map0 = 0xffff3210;
			lane_map1 = 0xffffffff;
		} else {
			lane_map0 = 0xfff43210;
			lane_map1 = 0xffffffff;
		}
		if (dev_p->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0))
			lane_map1 = lane_map0;
		break;
#endif
	default:
		break;
	}

	for (i = 0; i < phy_cfg->lane_num; i++) {
		if (i < 8) {
			bit = i * 4;
			sel = (lane_map0 >> bit) & 0xf;
		} else if (i < 16) {
			bit = (i - 8) * 4;
			sel = (lane_map1 >> bit) & 0xf;
		}

		if (dev_p->dev_cfg.basic.lcd_type == LCD_VBYONE &&
		    dev_p->dev_cfg.control.vbyone_cfg.lane_count == 16) {
			phy_cfg->ch_ctrl[i].sel = sel;
			phy_cfg->ch_ctrl[i].sel_dft = sel;
		} else {
			if (sel == 0xf) {
				phy_cfg->ch_ctrl[i].sel = 0xff;
				phy_cfg->ch_ctrl[i].sel_dft = 0xff;
			} else {
				phy_cfg->ch_ctrl[i].sel = sel;
				phy_cfg->ch_ctrl[i].sel_dft = sel;
			}
		}
	}

	lcd_lane_sel_to_ch_swap(pdrv, dev_p);
	if (dev_p->dev_cfg.basic.lcd_type == LCD_MLVDS)
		lcd_mlvds_phy_ckdi_config(pdrv, dev_p);
}

void lcd_lane_map_update(struct aml_lcd_drv_s *pdrv)
{
	if (!pdrv->curr_dev)
		return;

#ifdef CONFIG_AMLOGIC_LCD_LVDS
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_LVDS)
		lcd_lvds_lane_swap(pdrv, pdrv->curr_dev);
#endif
	lcd_lane_sel_to_ch_swap(pdrv, pdrv->curr_dev);

	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MLVDS)
		lcd_mlvds_phy_ckdi_config(pdrv, pdrv->curr_dev);
}

int lcd_lane_sel_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg)
{
	unsigned int offset;

	if (!pdrv || !pdrv->curr_dev || !phy_cfg)
		return -1;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_T5D:
	case LCD_CHIP_TXHD2:
		phy_cfg->ch_swap0 = lcd_vcbus_read(P2P_CH_SWAP0);
		phy_cfg->ch_swap1 = lcd_vcbus_read(P2P_CH_SWAP1);
		break;
	case LCD_CHIP_T7:
	case LCD_CHIP_T5W:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6D:
	case LCD_CHIP_T6W:
	case LCD_CHIP_A9:
		phy_cfg->ch_swap0 = lcd_vcbus_read(P2P_CH_SWAP0_T7 + offset);
		phy_cfg->ch_swap1 = lcd_vcbus_read(P2P_CH_SWAP1_T7 + offset);
		break;
	case LCD_CHIP_T3X: /* reg P2P_CH_SWAP can't readback, just use phy_cfg value */
	case LCD_CHIP_T6X:
		phy_cfg->ch_swap0 = pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap0;
		phy_cfg->ch_swap1 = pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap1;
		break;
	default:
		break;
	}
	lcd_ch_swap_to_lane_sel(pdrv, pdrv->curr_dev, phy_cfg);

	return 0;
}

void lcd_lane_map_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset, channel_sel0, channel_sel1;

	offset = pdrv->data->offset_venc_if[pdrv->index];
	channel_sel0 = pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap0;
	channel_sel1 = pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap1;

	// lane_swap_set
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TM2:
	case LCD_CHIP_T5D:
	case LCD_CHIP_TXHD2:
		lcd_vcbus_write(P2P_CH_SWAP0, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1, channel_sel1);
		break;
	case LCD_CHIP_T7: /* when MIPI-DSI using dual-port, swap B to venc1 */
		offset = pdrv->data->offset_venc_if[pdrv->index];
		lcd_vcbus_write(P2P_CH_SWAP0_T7 + offset, pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7 + offset, pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap1);

		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI &&
		    pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0)) {
			offset = pdrv->data->offset_venc_if[1];
			lcd_vcbus_write(P2P_CH_SWAP0_T7 + offset,
						pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap0);
			lcd_vcbus_write(P2P_CH_SWAP1_T7 + offset,
						pdrv->curr_dev->dev_cfg.phy_cfg.ch_swap1);
		}
		break;
	case LCD_CHIP_T5W:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6D:
	case LCD_CHIP_T6W:
	case LCD_CHIP_A9:
		lcd_vcbus_write(P2P_CH_SWAP0_T7 + offset, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7 + offset, channel_sel1);
		break;
	case LCD_CHIP_T3X: /* reg P2P_CH_SWAP can't readback, so print value when write reg */
	case LCD_CHIP_T6X:
		lcd_vcbus_write(P2P_CH_SWAP0_T7 + offset, channel_sel0);
		lcd_vcbus_write(P2P_CH_SWAP1_T7 + offset, channel_sel1);
		LCD_PR(pdrv, "%s: P2P_CH_SWAP0=0x%x, P2P_CH_SWAP1=0x%x",
			__func__, channel_sel0, channel_sel1);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
void lcd_mipi_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	LCD_DBG(pdrv, "%s: %u", __func__, on_off);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (on_off == 0)
			break;

		if (pdrv->index == 0) {
			if (pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0)) {
				//Bit 5: reg_phy1_pll_sel
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0x1, 5, 1);
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x0, 16, 10);
			}

			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x0, 0, 10);
		} else if (pdrv->index == 1) {
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x0, 16, 10);
		} else {
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		lcd_lane_map_set(pdrv);
		break;
	case LCD_CHIP_TXHD2:
		if (on_off) {
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x0, 0, 10);
			lcd_lane_map_set(pdrv);
		}
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_LVDS
void lcd_lvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds = 0, bit_data_in_edp = 0, bit_lane_sel = 0;
	unsigned int val_lane_sel = 0, len_lane_sel = 0;
	unsigned int dual_port, phy_div;

	phy_div = pdrv->curr_dev->dev_cfg.control.lvds_cfg.dual_port ? 2 : 1;
	dual_port = pdrv->curr_dev->dev_cfg.control.lvds_cfg.dual_port & 0x1;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (pdrv->index == 0) { /* lane0~lane4 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			// should only t3x drv0 has dual port
			val_lane_sel = dual_port ? 0x5550555 : 0x155;
			len_lane_sel = dual_port ? 32 : 10;
		} else if (pdrv->index == 1) { /* lane10~lane14 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 20;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
		} else { // should only t7 has drv2
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1;
			bit_data_in_lvds = 4;
			bit_data_in_edp = 0xff;
			bit_lane_sel = 10;
			// dual_port ? lane5~lane14 : lane5~lane9
			val_lane_sel = dual_port ? 0xaaaaa : 0x2aa;
			len_lane_sel = dual_port ? 20 : 10;
		}

		if (on_off) {
			// sel dphy data_in
			if (bit_data_in_edp < 0xff)
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, val_lane_sel,
				bit_lane_sel, len_lane_sel);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (1 << 5));
			/* set cntl_ser_en:  8-channel */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0x3ff, 16, 10);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3X:
		if (pdrv->index == 0) { /* lane0~lane4 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			// should only t3x drv0 has dual port
			val_lane_sel = dual_port ? 0x5550555 : 0x155;
			len_lane_sel = dual_port ? 32 : 10;
		} else { /* lane10~lane14 */
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 10;
			val_lane_sel = 0x155;
			len_lane_sel = 10;
		}

		if (on_off) {
			// sel dphy data_in
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, val_lane_sel,
				bit_lane_sel, len_lane_sel);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (1 << 5));
			/* set cntl_ser_en:  8-channel */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0xffff, 16, 16);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);
			/* t3x: pn swap */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 1, 2, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (1 << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	case LCD_CHIP_TXHD2:
		reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2;
		reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1_TXHD2;
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (1 << 6));
			/* set cntl_ser_en:  8-channel */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0xfff, 16, 12);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, 0xc1000000);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T6D:
		if (on_off) {
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, 0xc3000000);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		if (on_off) {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0x3000000);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	case LCD_CHIP_A9:
		if (on_off) {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL0_A9, 1, 0, 1);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1_A9, 3, 0, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL2_A9, 0x55555, 0, 20);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 0, 2);
		}
		break;
	default:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (1 << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1,
				(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
void lcd_vbyone_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int div_sel, phy_div, lane_num, lane_sel = 0;
	unsigned int reg_dphy_tx_ctrl0, reg_dphy_tx_ctrl1;
	unsigned int bit_data_in_lvds = 0, bit_data_in_edp = 0, bit_lane_sel = 0;
	unsigned int bit_fifo_clk = 0, cntl_ser_mask = 0, cntl_ser_bit = 0;

	LCD_DBG(pdrv, "%s: %u", __func__, on_off);

	phy_div = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.phy_div;
	lane_num = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.lane_count;

	if (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits == 18)
		div_sel = 0;
	else if (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits == 24)
		div_sel = 2;
	else // pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits == 30
		div_sel = 3;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		if (pdrv->index == 0) {
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			lane_sel = 0x5555;
		} else { // drv1
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 16;
			lane_sel = 0xaaaa;
		}

		if (on_off) {
			// sel dphy data_in
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x5555, bit_lane_sel, 16);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (div_sel << 5));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0xff, 16, 8);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3X:
		if (pdrv->index == 0) {
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1;
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			bit_lane_sel = 0;
			lane_sel = 0x5555;
		} else { // drv1
			reg_dphy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			reg_dphy_tx_ctrl1 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1;
			bit_data_in_lvds = 2;
			bit_data_in_edp = 3;
			bit_lane_sel = 16;
			lane_sel = 0xaaaa;
		}

		if (on_off) {
			// sel dphy data_in
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			// sel dphy lane
			if (pdrv->index == 0 && lane_num > 8)  // T3X 16-lane
				lcd_combo_dphy_write(pdrv, COMBO_DPHY_CNTL1, 0x55555555);
			else  // lane8~15 sel [1]: mux to phy0 lane8~15, [2]: mux to phy1 lane0~7
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1,
					lane_sel, bit_lane_sel, 16);
			/* set fifo_clk_sel: div 7 */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl0, (div_sel << 5));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0xffff, 16, 16);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, reg_dphy_tx_ctrl1, (1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 1, 7, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl1, 0, 6, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, reg_dphy_tx_ctrl0, 0, 16, 16);
		}
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		if (pdrv->index == 0) {
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
			cntl_ser_bit = 12;
			cntl_ser_mask = 0xfff;
			bit_fifo_clk = 1;
		} else { // drv1
			reg_dphy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			reg_dphy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
			cntl_ser_bit = 4;
			cntl_ser_mask = 0xf;
			bit_fifo_clk = 3;
		}

		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(reg_dphy_tx_ctrl0, (div_sel << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(reg_dphy_tx_ctrl0, cntl_ser_mask, 16, cntl_ser_bit);
			/* pn swap */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(reg_dphy_tx_ctrl1, (1 << 30) | (bit_fifo_clk << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(reg_dphy_tx_ctrl1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(reg_dphy_tx_ctrl0, 0, 16, 12);
		}
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		if (on_off) {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0x3000000);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	case LCD_CHIP_A9:
		if (on_off) {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL0_A9, 1, 0, 1);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1_A9, 3, 0, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL2_A9, 0x55555, 0, 20);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 0, 2);
		}
		break;
	default:
		if (on_off) {
			/* set fifo_clk_sel: div 7 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (div_sel << 6));
			/* set cntl_ser_en:  8-channel to 1 */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1,
				(1 << 30) | ((phy_div - 1) << 25) | (1 << 24));
			/* decoupling fifo write enable after fifo enable */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_TCON
void lcd_mlvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int div_sel;

	LCD_DBG(pdrv, "%s: %u", __func__, on_off);

	/* phy_div: 0=div6, 1=div 7, 2=div8, 3=div10 */
	if (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits == 18)
		div_sel = 0;
	else // lcd_bits == 24
		div_sel = 2;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (div_sel << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	case LCD_CHIP_TXHD2:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_combo_dphy_write(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2,
						(div_sel << 6));
			/* serializer_en[27:16] */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2,
						0xfff, 16, 12);
			/* pn swap[2] */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2,
						1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_combo_dphy_write(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1_TXHD2,
						(1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1_TXHD2,
				1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1_TXHD2,
						0, 30, 2);
			/* disable lane */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2,
						0, 16, 12);
		}
		break;
	case LCD_CHIP_T6D:
		if (on_off) {
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, 0xc3000000);
			// lane_swap_set
			lcd_lane_map_set(pdrv);
		} else {
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		if (on_off) {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0x3000000);
			// lane_swap_set
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	default:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (div_sel << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}

void lcd_p2p_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	unsigned int p2p_type, phy_div, bit_data_in_lvds, bit_data_in_edp;

	LCD_DBG(pdrv, "%s: %u", __func__, on_off);

	/* phy_div: 0=div6, 1=div 7, 2=div8, 3=div10 */
	p2p_type = pdrv->curr_dev->dev_cfg.control.p2p_cfg.p2p_type & 0x1f;
	switch (p2p_type) {
	case P2P_CHPI: /* 8/10 coding */
	case P2P_USIT:
		phy_div = 3;
		break;
	default:
		phy_div = 2;
		break;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL0, (phy_div << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 1, 2, 1);

			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(ANACTRL_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	case LCD_CHIP_T3X:
		if (on_off) {
			// sel dphy data_in
			bit_data_in_lvds = 0;
			bit_data_in_edp = 1;
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 0, bit_data_in_edp, 1);
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL0, 1, bit_data_in_lvds, 1);
			/*
			 * sel dphy lane.
			 * For lane8~15 sel [1]: mux to phy0 lane8~15, [2]: mux to phy1 lane0~7
			 */
			if (pdrv->curr_dev->dev_cfg.control.p2p_cfg.lane_num > 8)
				lcd_combo_dphy_write(pdrv, COMBO_DPHY_CNTL1, 0x55555555);
			else
				lcd_combo_dphy_setb(pdrv, COMBO_DPHY_CNTL1, 0x5555, 0, 16);
			/* set fifo_clk_sel: div */
			lcd_combo_dphy_write(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0,
						(phy_div << 5));
			/* set cntl_ser_en:  all-channel to 1 */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0,
						0xffff, 16, 16);
			/* pn swap */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, 1, 2, 1);
			/* decoupling fifo enable, gated clock enable */
			lcd_combo_dphy_write(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1,
						(1 << 6) | (1 << 0));
			/* decoupling fifo write enable after fifo enable */
			lcd_combo_dphy_setb(pdrv, COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1, 1, 7, 1);

			lcd_lane_map_set(pdrv);
		}
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		if (on_off) {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0x3000000);
			lcd_lane_map_set(pdrv);
		} else {
			lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0);
		}
		break;
	default:
		if (on_off) {
			/* fifo_clk_sel[7:6]: 0=div6, 1=div 7, 2=div8, 3=div10 */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL0, (phy_div << 6));
			/* serializer_en[27:16] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0xfff, 16, 12);
			/* pn swap[2] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 2, 1);
			/* fifo enable[30], phy_clock gating[24] */
			lcd_ana_write(HHI_LVDS_TX_PHY_CNTL1, (1 << 30) | (1 << 24));
			/* fifo write enable[31] */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 1, 31, 1);

			lcd_lane_map_set(pdrv);
		} else {
			/* disable fifo */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			/* disable lane */
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 16, 12);
		}
		break;
	}
}
#endif

void lcd_dphy_ctrl_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off)
{
	if (!pdrv->curr_dev)
		return;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		lcd_mipi_dphy_set(pdrv, on_off);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_LVDS
	case LCD_LVDS:
		lcd_lvds_dphy_set(pdrv, on_off);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_dphy_set(pdrv, on_off);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		lcd_mlvds_dphy_set(pdrv, on_off);
		break;
	case LCD_P2P:
		lcd_p2p_dphy_set(pdrv, on_off);
		break;
#endif
	default:
		break;
	}
}

//0=low, 1=high, 2=prbs, 0xff=clean
void lcd_dphy_set_data(struct aml_lcd_drv_s *pdrv, int data)
{
	unsigned int reg_phy_tx_ctrl0;

	LCD_PR(pdrv, "%s: %d%s", __func__, data, data == 0xff ? "(off)" : "");

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
		switch (pdrv->index) {
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			break;
		case 2:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0;
			break;
		default:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			break;
		}
		switch (data) {
		case 0:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 1:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 2:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 13, 1);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 12, 1);
			break;
		default:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		}
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		switch (pdrv->index) {
		case 1:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			break;
		default:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
		}
		switch (data) {
		case 0:
			lcd_ana_setb(reg_phy_tx_ctrl0, 3, 8, 2);
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 10, 2);
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 1:
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 2);
			lcd_ana_setb(reg_phy_tx_ctrl0, 3, 10, 2);
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 2:
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_ana_setb(reg_phy_tx_ctrl0, 1, 13, 1);
			lcd_ana_setb(reg_phy_tx_ctrl0, 1, 12, 1);
			break;
		default:
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_ana_setb(reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		}
		break;
	case LCD_CHIP_T3X:
		switch (pdrv->index) {
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			break;
		default:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
		}
		switch (data) {
		case 0:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 1:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 2:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 13, 1);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 12, 1);
			break;
		default:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		}
		break;
	case LCD_CHIP_TXHD2:
		reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2;
		switch (data) {
		case 0:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 1:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 10, 2);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		case 2:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 13, 1);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 1, 12, 1);
			break;
		default:
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 4);
			lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 12, 2);
			break;
		}
		break;
	case LCD_CHIP_T6D:
		switch (data) {
		case 0:
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 3, 26, 2);
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 28, 2);
			break;
		case 1:
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 26, 2);
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 3, 28, 2);
			break;
		case 2:
			LCD_PR(pdrv, "not support");
			break;
		default:
			lcd_ana_setb(ANACTRL_LVDS_TX_PHY_CNTL1, 0, 26, 4);
			break;
		}
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		switch (data) {
		case 0:
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 3, 26, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 28, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			break;
		case 1:
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 26, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 3, 28, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			break;
		case 2:
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 26, 4);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 1, 30, 1);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 1, 31, 1);
			break;
		default:
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 26, 4);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL1, 0, 30, 2);
			break;
		}
		break;
	default:
		switch (data) {
		case 0:
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 3, 8, 2);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 10, 2);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 12, 2);
			break;
		case 1:
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 2);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 3, 10, 2);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 12, 2);
			break;
		case 2:
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 4);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 13, 1);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 1, 12, 1);
			break;
		default:
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 4);
			lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 12, 2);
			break;
		}
		break;
	}
}

int lcd_dphy_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int len = 0;
	struct reg_name_set_s *reg_dphy_table = NULL, *reg_ctrl_table = NULL, *reg_ch_swap = NULL;
	unsigned int size_dphy = 0, size_ctrl = 0, size_ch_swap = 0, reg_bus, reg_offset;
	struct reg_name_set_s ch_swap_reg_dft[] = {
		{P2P_CH_SWAP0, "P2P_CH_SWAP0"},
		{P2P_CH_SWAP1, "P2P_CH_SWAP1"}
	};
	struct reg_name_set_s ch_swap_reg_t7[] = {
		{P2P_CH_SWAP0_T7, "P2P_CH_SWAP0"},
		{P2P_CH_SWAP1_T7, "P2P_CH_SWAP1"}
	};
	struct reg_name_set_s dphy_reg_dft[] = {
		{HHI_LVDS_TX_PHY_CNTL0, "HHI_LVDS_TX_PHY_CNTL0"},
		{HHI_LVDS_TX_PHY_CNTL1, "HHI_LVDS_TX_PHY_CNTL1"}
	};
	struct reg_name_set_s dphy_ctrl_reg_t7[] = {
		{COMBO_DPHY_CNTL0, "COMBO_DPHY_CNTL0"},
		{COMBO_DPHY_CNTL1, "COMBO_DPHY_CNTL1"}
	};
	struct reg_name_set_s dphy_reg_t7[] = {
		{COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0, "COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1, "COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0, "COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1, "COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0, "COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1, "COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1"}
	};
	struct reg_name_set_s dphy_reg_t3[] = {
		{ANACTRL_LVDS_TX_PHY_CNTL0, "ANACTRL_LVDS_TX_PHY_CNTL0"},
		{ANACTRL_LVDS_TX_PHY_CNTL1, "ANACTRL_LVDS_TX_PHY_CNTL1"},
		{ANACTRL_LVDS_TX_PHY_CNTL2, "ANACTRL_LVDS_TX_PHY_CNTL2"},
		{ANACTRL_LVDS_TX_PHY_CNTL3, "ANACTRL_LVDS_TX_PHY_CNTL3"}
	};
	struct reg_name_set_s dphy_reg_txhd2[] = {
		{COMBO_DPHY_CNTL0_TXHD2,                  "COMBO_DPHY_CNTL0"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0_TXHD2, "COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0"},
		{COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1_TXHD2, "COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1"}
	};
	struct reg_name_set_s dphy_reg_a9[] = {
		{ANACTRL_LVDS_TX_PHY_CNTL0_A9, "ANACTRL_LVDS_TX_PHY_CNTL0"},
		{ANACTRL_LVDS_TX_PHY_CNTL1_A9, "ANACTRL_LVDS_TX_PHY_CNTL1"},
		{ANACTRL_LVDS_TX_PHY_CNTL2_A9, "ANACTRL_LVDS_TX_PHY_CNTL2"},
	};

	if (!pdrv)
		return 0;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_TM2:
		reg_ch_swap = ch_swap_reg_dft;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_dft);
		reg_dphy_table = dphy_reg_dft;
		size_dphy = ARRAY_SIZE(dphy_reg_dft);
		reg_bus = LCD_REG_DBG_HHI_BUS;
		break;
	case LCD_CHIP_T5D:
		reg_ch_swap = ch_swap_reg_dft;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_dft);
		reg_dphy_table = dphy_reg_dft;
		size_dphy = ARRAY_SIZE(dphy_reg_dft);
		reg_bus = LCD_REG_DBG_ANA_BUS;
		break;
	case LCD_CHIP_T5W:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_dphy_table = dphy_reg_dft;
		size_dphy = ARRAY_SIZE(dphy_reg_dft);
		reg_bus = LCD_REG_DBG_ANA_BUS;
		break;
	case LCD_CHIP_T7:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_ctrl_table = dphy_ctrl_reg_t7;
		size_ctrl = ARRAY_SIZE(dphy_ctrl_reg_t7);
		if (pdrv->index == 2) {
			reg_dphy_table = &dphy_reg_t7[4];
			size_dphy = 2;
		} else if (pdrv->index == 1) {
			reg_dphy_table = &dphy_reg_t7[2];
			size_dphy = 2;
		} else {
			reg_dphy_table = &dphy_reg_t7[0];
			size_dphy = 2;
		}
		reg_bus = LCD_REG_DBG_COMBOPHY_BUS;
		break;
	case LCD_CHIP_T3:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		if (pdrv->index) {
			reg_dphy_table = &dphy_reg_t3[2];
			size_dphy = 2;
		} else {
			reg_dphy_table = &dphy_reg_t3[0];
			size_dphy = 2;
		}
		reg_bus = LCD_REG_DBG_ANA_BUS;
		break;
	case LCD_CHIP_T5M:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_dphy_table = &dphy_reg_t3[0];
		size_dphy = 2;
		reg_bus = LCD_REG_DBG_ANA_BUS;
		break;
	case LCD_CHIP_T3X:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_ctrl_table = dphy_ctrl_reg_t7;
		size_ctrl = ARRAY_SIZE(dphy_ctrl_reg_t7);
		if (pdrv->index) {
			reg_dphy_table = &dphy_reg_t7[2];
			size_dphy = 2;
		} else {
			reg_dphy_table = &dphy_reg_t7[0];
			size_dphy = 2;
		}
		reg_bus = LCD_REG_DBG_COMBOPHY_BUS;
		break;
	case LCD_CHIP_TXHD2:
		reg_ch_swap = ch_swap_reg_dft;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_dft);
		reg_dphy_table = dphy_reg_txhd2;
		size_dphy = ARRAY_SIZE(dphy_reg_txhd2);
		reg_bus = LCD_REG_DBG_COMBOPHY_BUS;
		break;
	case LCD_CHIP_T6D:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_dphy_table = dphy_reg_t3;
		size_dphy = 2;
		reg_bus = LCD_REG_DBG_ANA_BUS;
		break;
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_dphy_table = dphy_reg_a9;
		size_dphy = 2;
		reg_bus = LCD_REG_DBG_VX1_LVDS_CTRL_BUS;
		break;
	case LCD_CHIP_A9:
		reg_ch_swap = ch_swap_reg_t7;
		size_ch_swap = ARRAY_SIZE(ch_swap_reg_t7);
		reg_dphy_table = dphy_reg_a9;
		size_dphy = 3;
		reg_bus = LCD_REG_DBG_VX1_LVDS_CTRL_BUS;
		break;
	default:
		return 0;
	}

	if (reg_ch_swap) {
		reg_offset = pdrv->data->offset_venc_if[pdrv->index];
		len += str_add_reg_sets(pdrv, buf + len, len + offset,
			LCD_REG_DBG_VC_BUS, reg_offset,
			reg_ch_swap, size_ch_swap);
	}
	if (reg_ctrl_table) {
		len += str_add_reg_sets(pdrv, buf + len, len + offset, reg_bus, 0,
			reg_ctrl_table, size_ctrl);
	}
	len += str_add_reg_sets(pdrv, buf + len, len + offset, reg_bus, 0,
			reg_dphy_table, size_dphy);

	return len;
}
