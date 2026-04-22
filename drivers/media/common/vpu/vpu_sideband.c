// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include <linux/amlogic/aml_ddr_tool.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"
#include "vpu_sideband.h"

u32 vpu_sideband_en[5] = {0xff, 0xff, 0xff, 0xff, 0xff}; /* 1:enable, 0:disable, 0xff:auto */
u32 vpu_sideband_level_up;
u32 vpu_sideband_level_down;
u32 vpu_sideband_block_device;
struct dmc_side_band dmc_sb_setting;

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t7[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   0,   1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   4,   1},
	{RD3_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   8,   1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL,  1,   12,  1},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL,  1,   13,  1},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t3x[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL1_T3X,  1,   0,   1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL1_T3X,  1,   4,   1},
	{RD3_ARB_VPP0,   VPU_ARB_URG_CTRL1_T3X,  1,   8,   1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL1_T3X,  1,   12,  1},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL1_T3X,  1,   13,  1},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t6w[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   0,   1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   4,   1},
	{RD3_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   12,  1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL,  1,   8,   1},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL,  1,   9,   1},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t6x[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   0,  1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL,  1,   4,  1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL,  3,   8,  1},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL,  3,   9,  1},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_txhd2[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL_TXHD2,  1,   0,   1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL_TXHD2,  1,   2,   1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL_TXHD2,  3,   6,  2},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL_TXHD2,  3,   8,  2},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t5w[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL_T5W,  1,   0,   1},
	{RD1_ARB_VPP0,   VPU_ARB_URG_CTRL_T5W,  1,   2,   1},
	{RD2_ARB_VPP0, VPU_ARB_URG_CTRL_T5W,  1,   4,  1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL_T5W,  3,   6,  2},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL_T5W,  3,   8,  2},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t3[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL_T3,  1,   0,   1},
	{RD2_ARB_VPP0,   VPU_ARB_URG_CTRL_T3,  1,   4,   1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL_T3,  3,   8,  2},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL_T3,  3,   10,  2},
};

struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_s7[] = {
	{RD0_ARB_VPP0,   VPU_ARB_URG_CTRL_S7,  1,   0,   1},
	{RD1_ARB_VPP0,   VPU_ARB_URG_CTRL_S7,  1,   2,   1},
	{RD2_ARB_VPP0, VPU_ARB_URG_CTRL_S7,  1,   4,  1},
	{WR02_ARB_VDIN0, VPU_ARB_URG_CTRL_S7,  3,   6,  2},
	{WR02_ARB_VDIN1, VPU_ARB_URG_CTRL_S7,  3,   8,  2},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t7[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t3x[] = {
	{0,   VPP_OFIFO_URG_CTRL_T3X,  0,   0,   32},
	{1,   VPP_SLICE1_OFIFO_URG_CTRL_T3X,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t6w[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t6x[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_txhd2[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t5w[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t3[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_s7[] = {
	{0,   VPP_OFIFO_URG_CTRL,  0,   0,   32},
};

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (!token)
		return 0;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0 || !token)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

void set_vpu_sideband_init(void)
{
	u32 sideband_up = 0, sideband_down = 0;
	u32 sideband_level = 0;
	struct vpu_sideband_ctrl_s *sideband_ctrl_table;
	u32 vpp_ofifo_urg_ctrl = 0;

	if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3) {
		sideband_ctrl_table = vpu_conf.data->vpp_ofifo_urg_ctrl_table;
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
		//bit15； urgent_ctrl_en
		//fifo cnt < 256xsideband_down pixel
		//fifo cnt > 256xsideband_up pixel, sideband worked
		sideband_level = vpu_vcbus_read(vpp_ofifo_urg_ctrl);
		sideband_level |= 1 << 15;
		if (vpu_conf.data->chip_type == VPU_CHIP_T7) {
			sideband_up = 7;
			sideband_down = 6;
		} else if (vpu_conf.data->chip_type == VPU_CHIP_T3) {
			sideband_up = 16;
			sideband_down = 5;
		}
		sideband_level |= (sideband_up << 6 | sideband_down);
		//set vpu sideband level
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T3X) {
		sideband_ctrl_table = &vpu_conf.data->vpp_ofifo_urg_ctrl_table[0];
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
		//bit15； urgent_ctrl_en
		//fifo cnt < 256xsideband_down pixel
		//fifo cnt > 256xsideband_up pixel, sideband worked
		sideband_level = vpu_vcbus_read(vpp_ofifo_urg_ctrl);
		sideband_level |= 1 << 15;
		sideband_up = 8;
		sideband_down = 5;
		sideband_level |= (sideband_up << 6 | sideband_down);
		//set vpu sideband level
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
		//t3x has 2 slice
		sideband_ctrl_table = &vpu_conf.data->vpp_ofifo_urg_ctrl_table[1];
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T6W ||
		vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T6X ||
		vpu_conf.data->chip_type == VPU_CHIP_S7D ||
		vpu_conf.data->chip_type == VPU_CHIP_S6) {
		sideband_ctrl_table = vpu_conf.data->vpp_ofifo_urg_ctrl_table;
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
		//bit15； urgent_ctrl_en
		//fifo cnt < 256xsideband_down pixel
		//fifo cnt > 256xsideband_up pixel, sideband worked
		sideband_level = vpu_vcbus_read(vpp_ofifo_urg_ctrl);
		sideband_level |= 1 << 15;
		if (vpu_conf.data->chip_type == VPU_CHIP_T6W ||
			vpu_conf.data->chip_type == VPU_CHIP_T6X) {
			sideband_up = 16;
			sideband_down = 5;
		} else if (vpu_conf.data->chip_type == VPU_CHIP_S7 ||
			vpu_conf.data->chip_type == VPU_CHIP_S7D ||
			vpu_conf.data->chip_type == VPU_CHIP_S6) {
			//max 4k fifo
			sideband_up = 8;
			sideband_down = 5;
		}
		sideband_level |= (sideband_up << 6 | sideband_down);
		//set vpu sideband level
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_TXHD2 ||
		vpu_conf.data->chip_type == VPU_CHIP_T5W) {
		sideband_ctrl_table = vpu_conf.data->vpp_ofifo_urg_ctrl_table;
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
		//bit15； urgent_ctrl_en
		//fifo cnt < 256xsideband_down pixel
		//fifo cnt > 256xsideband_up pixel, sideband worked
		sideband_level = vpu_vcbus_read(vpp_ofifo_urg_ctrl);
		sideband_level |= 1 << 15;
		sideband_up = 8;
		sideband_down = 5;
		sideband_level |= (sideband_up << 6 | sideband_down);
		//set vpu sideband level
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
	}
}

void dmc_reg_setting(char rw, u32 block_device)
{
	char block_num = 0;
	int i, j = 0;
	int ret;

	if (block_device == 0) {
		dmc_sb_setting.block_bus[0] = (unsigned char)-1;
	} else {
		for (i = 0; i < 32; i++) {
			if (block_device & 0x1) {
				block_num++;
				dmc_sb_setting.block_bus[j++] = (unsigned char)i;
			}
			block_device = block_device >> 1;
			if (!block_device)
				break;
		}
	}
	dmc_sb_setting.rw = rw;
	dmc_sb_setting.block_num = block_num;
	pr_info("%s, rw=%d, block_num=%d,block_bus[0]=0x%x, j=%d;[j]=0x%x\n",
		__func__,
		rw,
		block_num,
		dmc_sb_setting.block_bus[0],
		j,
		dmc_sb_setting.block_bus[j]);
	ret = enable_side_band(&dmc_sb_setting);
}

static void dmc_sb_param_setting(u32 arb_port)
{
	if (vpu_conf.data->chip_type == VPU_CHIP_T5W) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 16;
			break;
		case 1:
			dmc_sb_setting.bus = 17;
			break;
		case 2:
			dmc_sb_setting.bus = 18;
			break;
		case 3:
			dmc_sb_setting.bus = 19;
			break;
		case 4:
			dmc_sb_setting.bus = 20;
			break;
		}
	} else if (vpu_conf.data->chip_type == VPU_CHIP_TXHD2) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 16;
			break;
		case 1:
			dmc_sb_setting.bus = 17;
			break;
		case 2:
			dmc_sb_setting.bus = 19;
			break;
		case 3:
			dmc_sb_setting.bus = 20;
			break;
		}
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T3) {
		dmc_sb_setting.dmc = 0;
		/*T3 only 1 port*/
		dmc_sb_setting.bus = 2;
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T6W) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 2;
			break;
		case 1:
			dmc_sb_setting.bus = 6;
			break;
		case 2:
			dmc_sb_setting.bus = 7;
			break;
		case 3:
			dmc_sb_setting.bus = 2;
			break;
		case 4:
			dmc_sb_setting.bus = 2;
			break;
		}
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3X) {
		dmc_sb_setting.dmc = 0;
		/*T7 only 1 port*/
		dmc_sb_setting.bus = 2;
	} else if (vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_S7D) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 1;
			break;
		case 1:
			dmc_sb_setting.bus = 2;
			break;
		case 2:
			//dmc_sb_setting.bus = 3;
			break;
		case 3:
			dmc_sb_setting.bus = 4;
			break;
		case 4:
			dmc_sb_setting.bus = 6;
			break;
		}
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T6X) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 1;
			break;
		case 1:
			dmc_sb_setting.bus = 2;
			break;
		}
	} else if (vpu_conf.data->chip_type == VPU_CHIP_S6) {
		dmc_sb_setting.dmc = 0;
		switch (arb_port) {
		case 0:
			dmc_sb_setting.bus = 1;
			break;
		case 1:
			dmc_sb_setting.bus = 2;
			break;
		case 2:
			dmc_sb_setting.bus = 3;
			break;
		case 3:
			dmc_sb_setting.bus = 2;
			break;
		case 4:
			dmc_sb_setting.bus = 2;
			break;
		}
	}
}

void set_vpu_sideband_enable(u32 arb_port, u32 port_enable)
{
	static u32 enable_pre[5];
	static bool enable[5] = {false};
	int i;
	struct vpu_sideband_ctrl_s *sideband_ctrl_table = NULL;

	if (arb_port >= 5)
		return;
	if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3X ||
		vpu_conf.data->chip_type == VPU_CHIP_T6W  ||
		vpu_conf.data->chip_type == VPU_CHIP_TXHD2 ||
		vpu_conf.data->chip_type == VPU_CHIP_T5W ||
		vpu_conf.data->chip_type == VPU_CHIP_T3 ||
		vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T6X ||
		vpu_conf.data->chip_type == VPU_CHIP_S7D ||
		vpu_conf.data->chip_type == VPU_CHIP_S6) {
		if (vpu_sideband_en[arb_port] != 0xff)
			port_enable = vpu_sideband_en[arb_port];
		else if (port_enable == 0xff)
			return;

		if (port_enable != enable_pre[arb_port]) {
			u32 dmc_enable = 0;

			enable[arb_port] = port_enable;

			for (i = 0; i < 5; i++) {
				if (enable[i] != 0) {
					dmc_enable = 1;
					break;
				}
			}
			//enable sideband
			sideband_ctrl_table = &vpu_conf.data->vpu_arb_urg_ctrl_table[arb_port];

			//enable arb port vpp0 ofifo
			if (port_enable)
				vpu_vcbus_setb(sideband_ctrl_table->reg, sideband_ctrl_table->val,
					sideband_ctrl_table->bit, sideband_ctrl_table->len);

			dmc_sb_param_setting(arb_port);

			if (!dmc_enable) {
				vpu_vcbus_setb(sideband_ctrl_table->reg,
					0, 28, 1);
				disable_side_band(dmc_sb_setting.dmc, dmc_sb_setting.bus);
			} else {
				vpu_vcbus_setb(sideband_ctrl_table->reg,
					1, 28, 1);
				dmc_reg_setting(3, vpu_sideband_block_device);
			}
			VPUPR("sideband_enable:dmc_enable=0x%x\n",
				dmc_enable);
			VPUPR("arb_port=%d,port_enable=%d,enable_pre=0x%x,vpu_sideband_en=0x%x\n",
				arb_port, port_enable,
				enable_pre[arb_port], vpu_sideband_en[arb_port]);
		}
		enable_pre[arb_port] = port_enable;
	}
}
EXPORT_SYMBOL(set_vpu_sideband_enable);

void set_vpu_sideband_level_set(u32 sideband_up, u32 sideband_down)
{
	struct vpu_sideband_ctrl_s *sideband_ctrl_table = NULL;
	static u32 sideband_up_pre, sideband_down_pre;

	sideband_ctrl_table = &vpu_conf.data->vpp_ofifo_urg_ctrl_table[0];

	if (sideband_ctrl_table &&
		(sideband_up_pre != sideband_up ||
		sideband_down_pre != sideband_down)) {
		u32 sideband_level = 0;
		u32 vpp_ofifo_urg_ctrl = 0;
		struct vpu_sideband_ctrl_s *sideband_ctrl_table = NULL;

		//set vpu sideband level
		//bit15； urgent_ctrl_en
		//fifo cnt < 256xsideband_down pixel
		//fifo cnt > 256xsideband_up pixel, sideband worked
		sideband_ctrl_table = &vpu_conf.data->vpp_ofifo_urg_ctrl_table[0];
		vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;

		sideband_level = vpu_vcbus_read(vpp_ofifo_urg_ctrl);
		sideband_level &= ~0xfff;
		sideband_level |= 1 << 15;
		sideband_level |= (sideband_up << 6 | sideband_down);
		vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
		if (vpu_conf.data->chip_type == VPU_CHIP_T3X) {
			//set vpu slice2 sideband level
			sideband_ctrl_table = &vpu_conf.data->vpp_ofifo_urg_ctrl_table[1];
			vpp_ofifo_urg_ctrl = sideband_ctrl_table->reg;
			vpu_vcbus_write(vpp_ofifo_urg_ctrl, sideband_level);
		}
	}
}
EXPORT_SYMBOL(set_vpu_sideband_level_set);

ssize_t vpu_sideband_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (vpu_conf.data->chip_type == VPU_CHIP_T5W) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu read1 arb for vpp0 line fifo\n"
		"bit2: 4 vpu read2 arb for vpp0 line fifo\n"
		"bit3: 8 vpu write0 arb for vdin0 line fifo\n"
		"bit4: 0x10 vpu write1 arb for vdin1 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_TXHD2) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu read1 arb for vpp0 line fifo\n"
		"bit2: 4 vpu write0 arb for vdin0 line fifo\n"
		"bit3: 8 vpu write1 arb for vdin1 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3X ||
		vpu_conf.data->chip_type == VPU_CHIP_T6W ||
		vpu_conf.data->chip_type == VPU_CHIP_S6) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu read2 arb for vpp0 line fifo\n"
		"bit2: 4 vpu read3 arb for vpp0 line fifo\n"
		"bit3: 8 vpu write0/2 arb for vdin0 line fifo\n"
		"bit4: 0x10 vpu write0/2 arb for vdin1 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_T3) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu read2 arb for vpp0 line fifo\n"
		"bit2: 4 vpu write0 arb for vdin0 line fifo\n"
		"bit3: 8 vpu write1 arb for vdin1 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);
	} else if (vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_S7D) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu read1 arb for vpp0 line fifo\n"
		"bit4: 8 vpu write0 arb for vdin0 line fifo\n"
		"bit8: 0x10 vpu write1 arb for vdin1 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);

	} else if (vpu_conf.data->chip_type == VPU_CHIP_T6X) {
		const char *vpu_arb_port_str = {
		"Usage:(every time enable one bit)\n"
		"bit0: 1 vpu write/read0 arb for vpp0 line fifo\n"
		"bit1: 2 vpu write/read1 arb for vpp0 line fifo\n"
		"echo 'port enable' > /sys/class/vpu/vpu_sideband_en\n"
		};
		len += sprintf(buf + len, "%s\n", vpu_arb_port_str);
	}
	len += sprintf(buf + len, "vpu_sideband_en[0]: 0x%x\n", vpu_sideband_en[0]);
	len += sprintf(buf + len, "vpu_sideband_en[1]: 0x%x\n", vpu_sideband_en[1]);
	len += sprintf(buf + len, "vpu_sideband_en[2]: 0x%x\n", vpu_sideband_en[2]);
	len += sprintf(buf + len, "vpu_sideband_en[3]: 0x%x\n", vpu_sideband_en[3]);
	len += sprintf(buf + len, "vpu_sideband_en[4]: 0x%x\n", vpu_sideband_en[4]);
	return len;
}

ssize_t vpu_sideband_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		u32 arb_port = 0;

		if (vpu_conf.data->chip_type == VPU_CHIP_T5W ||
			vpu_conf.data->chip_type == VPU_CHIP_T6W ||
			vpu_conf.data->chip_type == VPU_CHIP_T7 ||
			vpu_conf.data->chip_type == VPU_CHIP_T3X ||
			vpu_conf.data->chip_type == VPU_CHIP_S7 ||
			vpu_conf.data->chip_type == VPU_CHIP_S7D ||
			vpu_conf.data->chip_type == VPU_CHIP_S6) {
			if (parsed[0] & 1)
				arb_port = 0;
			else if (parsed[0] & 2)
				arb_port = 1;
			else if (parsed[0] & 4)
				arb_port = 2;
			else if (parsed[0] & 8)
				arb_port = 3;
			else if (parsed[0] & 0x10)
				arb_port = 4;
		} else if (vpu_conf.data->chip_type == VPU_CHIP_TXHD2 ||
			vpu_conf.data->chip_type == VPU_CHIP_T3) {
			if (parsed[0] & 1)
				arb_port = 0;
			else if (parsed[0] & 2)
				arb_port = 1;
			else if (parsed[0] & 4)
				arb_port = 2;
			else if (parsed[0] & 8)
				arb_port = 3;
		} else if (vpu_conf.data->chip_type == VPU_CHIP_T6X) {
			if (parsed[0] & 1)
				arb_port = 0;
			else if (parsed[0] & 2)
				arb_port = 1;
		}
		vpu_sideband_en[arb_port] = parsed[1];

		set_vpu_sideband_enable(arb_port, parsed[1]);
	} else {
		pr_err("echo port_bit  sideband_en > sideband_en\n");
		return -EINVAL;
	}
	return count;
}

ssize_t vpu_sideband_level_show(const struct class *class,
				    const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "vpu sideband level up/down(%d~%d)\n",
		vpu_sideband_level_up,
		vpu_sideband_level_down);
}

ssize_t vpu_sideband_level_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para(buf, 2, parsed) == 2)) {
		vpu_sideband_level_up = parsed[0];
		vpu_sideband_level_down = parsed[1];
		set_vpu_sideband_level_set(vpu_sideband_level_up, vpu_sideband_level_down);
	} else {
		pr_err("echo sideband_level_up  sideband_level_down > vpu_sideband_level\n");
		return -EINVAL;
	}
	return count;
}

static ssize_t sideband_block_device_show_t7c(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A73/A53/GPU async interface\n"
	"bit1: reserved\n"
	"bit2: VPU Async interface\n"
	"bit3: NNA Async interface\n"
	"bit4: device/usb/pcie/hdmi rx Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: DOS/GE2D interface\n"
	"bit7: device2/isp/demux/dsp Async interface\n"
	"bit8: reserved\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: %d\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_t3x(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A55   async interface\n"
	"bit1: FRC Async interface\n"
	"bit2: VPU Async interface\n"
	"bit3: DOS Async interface\n"
	"bit4: system Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: GPU async interface\n"
	"bit7: VGE Async interface\n"
	"bit8: NNA Async interface\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_t6w(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU async interface\n"
	"bit1: GPU Async interface\n"
	"bit2: VPU0 Async interface\n"
	"bit3: VPU1 Async interface\n"
	"bit4: VPU2 write Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: VPU2 read async interface\n"
	"bit7: VPU3 Async interface\n"
	"bit8: HEVC Async interface\n"
	"bit9: ge2d Async interface\n"
	"bit10: system/periphs Async interface\n"
	"bit11: AMFC Async interface\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_t5w(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A53   async interface\n"
	"bit1: Mali.  async interface\n"
	"bit2: pcie  async\n"
	"bit3: HDCP/HDMI   Async interface\n"
	"bit4: hevc front Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: USB Async interface\n"
	"bit7: DEVICE. Async interface\n"
	"bit8: hevc_b async interface\n"
	"bit9: wave  async\n"
	"bit16: VPU read interface 0.\n"
	"bit17: VPU read interface 1.\n"
	"bit18: VPU read interface 2.\n"
	"bit19: VPU write interface 0.\n"
	"bit20: VPU write interface 1.\n"
	"bit21: DOS VDEC  interface.\n"
	"bit22: DOS HCODEC  interface.\n"
	"bit23: GE2D interface.\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_txhd2(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A53   async interface\n"
	"bit1: Mali0 .  async interface\n"
	"bit2: Mali1  async\n"
	"bit3: HDCP/HDMI   Async interface\n"
	"bit4: hevc front Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: USB2drd   Async interface\n"
	"bit7: DEVICE.    Async interface\n"
	"bit8: usb2host  async interface\n"
	"bit9: reserved\n"
	"bit16: VPU read interface 0.\n"
	"bit17: VPU read interface 1.\n"
	"bit18: VPU read interface 2.\n"
	"bit19: VPU write interface 0.\n"
	"bit20: VPU write interface 1.\n"
	"bit21: DOS VDEC  interface.\n"
	"bit22: Reserved.\n"
	"bit23: GE2D interface.\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_t3(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A53   async interface\n"
	"bit1: Mali GPU\n"
	"bit2: NIC5 VPU channel. include VPU, FRC\n"
	"bit3: NNA\n"
	"bit4: NIC2 include PCIE, usb, emmc eth spicc audma etc\n"
	"bit5: reserved for dmc_test\n"
	"bit6: NIC3 include ge2d, hevc,vdec, hcodec etc\n"
	"bit7: NIC4 include NNA debus, dsp, aocpu, audio, dmc, etc\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_s7(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A55   async interface\n"
	"bit1: VPU0 Async interface\n"
	"bit2: VPU1 Async interface\n"
	"bit3: VPU2 Async interface\n"
	"bit4: VPU3 Async interface\n"
	"bit5: reserved for dmc_test\n"
	"bit6: VPU4 async interface\n"
	"bit7: HEVC Async interface\n"
	"bit8: VDEC Async interface\n"
	"bit9: HCODEC Async interface\n"
	"bit10: system Async interface.\n"
	"bit11: DEMUX async interface.\n"
	"bit12: GPU Async interface.\n"
	"bit13: GE2D Async interface.\n"
	"echo 'value' > /sys/class/vpu/sideband_block_device\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_t6x(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU/A55   async interface\n"
	"bit1: VPU0 Async interface\n"
	"bit2: VPU1 Async interface\n"
	"bit3: Mali/SYS Async interface\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

static ssize_t sideband_block_device_show_s6(char *buf)
{
	ssize_t len = 0;

	const char *axi_device_usage_str = {
	"Usage:\n"
	"bit0: CPU  interface\n"
	"bit1: VPU0 interface\n"
	"bit2: VPU1 interface\n"
	"bit3: VPU2 interface\n"
	"bit6: HEVC_B  interface\n"
	"bit7: HEVC_F interface\n"
	"bit8: VDEC interface\n"
	"bit9: MTE interface\n"
	"bit11: DEMUX  interface\n"
	"bit12: MALI interface\n"
	"bit13: VGE interface(VICP/GE2D/DEWARP)\n"
	"bit14: OTHER DEVICE interface\n"
	"bit15: DSP  interface\n"
	"bit16: AMFC interface\n"
	"bit17: PCIE interface\n"
	"bit18: NNA interface\n"
	"bit19: vc9000E interface\n"
	};
	len += sprintf(buf + len, "%s\n", axi_device_usage_str);
	len += sprintf(buf + len, "sideband_block_device: 0x%x\n", vpu_sideband_block_device);
	return len;
}

void set_vpu_sideband_block_device(u32 block_device)
{
	if (vpu_conf.data->chip_type == VPU_CHIP_T6W ||
		vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_TXHD2 ||
		vpu_conf.data->chip_type == VPU_CHIP_T5W ||
		vpu_conf.data->chip_type == VPU_CHIP_T6X ||
		vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3X ||
		vpu_conf.data->chip_type == VPU_CHIP_T3 ||
		vpu_conf.data->chip_type == VPU_CHIP_S6) {
		dmc_reg_setting(3, block_device);
	}
}
EXPORT_SYMBOL(set_vpu_sideband_block_device);

ssize_t vpu_sideband_block_device_show(const struct class *class,
				    const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (vpu_conf.data->chip_type == VPU_CHIP_T7)
		len = sideband_block_device_show_t7c(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_T3X)
		len = sideband_block_device_show_t3x(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_T6W)
		len = sideband_block_device_show_t6w(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_TXHD2)
		len = sideband_block_device_show_txhd2(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_T5W)
		len = sideband_block_device_show_t5w(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_T3)
		len = sideband_block_device_show_t3(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_S7 ||
		vpu_conf.data->chip_type == VPU_CHIP_S7D)
		len = sideband_block_device_show_s7(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_T6X)
		len = sideband_block_device_show_t6x(buf);
	else if (vpu_conf.data->chip_type == VPU_CHIP_S6)
		len = sideband_block_device_show_s6(buf);
	return len;
}

ssize_t vpu_sideband_block_device_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int ret;
	unsigned int res;

	ret = kstrtoint(buf, 0, &res);
	vpu_sideband_block_device = res;
	set_vpu_sideband_block_device(res);

	return count;
}

