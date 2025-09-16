// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/slab.h>
#include "ddr_port.h"

/*
 * NOTE:
 * Port "DEVICE" is total name for small bandwidth device.
 * There are many small bandwidth devices such as nand/arb/parser
 * connected to dmc under port "device", for better configure of
 * these devices, re-number them with start ID of 32
 *
 * EXAMPLE:
 *
 *            DMC CONTROLLER
 *                   |
 *   ---------------------------------
 *   |    |    | ..... |       | ... |
 *  arm  mali vpu    device   vdec  hevc
 *                     |
 *         ------------------------
 *         |    |     | ... |     |
 *       emmc  ge2d  usb  audio  spicc
 */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct ddr_port_desc ddr_port_desc_m8b[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HEVC"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2"),
	PORT_DESC(.port_id = 11, .port_name = "VDEC"),
	PORT_DESC(.port_id = 12, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 14, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 15, .port_name = "DEVICE"),
	/* start of each device */
	PORT_DESC(.port_id = 15, .sub_id =  1, .port_name = "NAND"),
	PORT_DESC(.port_id = 15, .sub_id =  2, .port_name = "BLKMV"),
	PORT_DESC(.port_id = 15, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 15, .sub_id =  4, .port_name = "USB0"),
	PORT_DESC(.port_id = 15, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 15, .sub_id =  6, .port_name = "GE2D SOURCE1"),
	PORT_DESC(.port_id = 15, .sub_id =  7, .port_name = "GE2D SOURCE2"),
	PORT_DESC(.port_id = 15, .sub_id =  8, .port_name = "GE2D DEST"),
	PORT_DESC(.port_id = 15, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 15, .sub_id = 10, .port_name = "SANA"),
	PORT_DESC(.port_id = 15, .sub_id = 11, .port_name = "SDIO2"),
	PORT_DESC(.port_id = 15, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 15, .sub_id = 13, .port_name = "ETHERNET")
};

static struct ddr_port_desc ddr_port_desc_gxbb[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "BLKMV"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_gxl[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_gxm[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .port_name = "H265ENC"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_gxlx[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "H265ENC/TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_axg[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "PCIE-A"),
	PORT_DESC(.port_id =  2, .port_name = "PCIE-B"),
	PORT_DESC(.port_id =  3, .port_name = "GE2D"),
	PORT_DESC(.port_id =  4, .port_name = "AUDIO"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SPICC1")
};

static struct ddr_port_desc ddr_port_desc_txl[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .port_name = "DEMOD")
};

static struct ddr_port_desc ddr_port_desc_txlx[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .port_name = "DEMOD")
};

static struct ddr_port_desc ddr_port_desc_txhd[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  9, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 10, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 11, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 12, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 13, .port_name = "VDEC"),
	PORT_DESC(.port_id = 14, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 15, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  1, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .port_name = "AUDIO OUT"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .port_name = "AUDIO IN"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .port_name = "AIU"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .port_name = "DEMOD")
};

static struct ddr_port_desc ddr_port_desc_tl1[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  3, .bus =  2, .port_name = "HDCP_HDMI"),
	PORT_DESC(.port_id =  4, .bus =  3, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .bus =  7, .port_name = "DEMOD")
};

static struct ddr_port_desc ddr_port_desc_c1[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "DSPA"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "DSPB"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "WAVE"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "GDC"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "GE2D"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "ISP"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "NNA"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "JPEG"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "SPICC_A"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "SPICC_B"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "SD_EMMCA"),
	PORT_DESC(.port_id = 13, .bus = 13, .port_name = "SD_EMMCB"),
	PORT_DESC(.port_id = 14, .bus = 14, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 15, .bus = 15, .port_name = "DMA"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "NAND_EMMCC"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "USB_AHB"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "USB_AXI"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "I2C"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "DMC_TEST")
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct ddr_port_desc ddr_port_desc_a1[] __initdata = {
	PORT_DESC(.port_id =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .port_name = "DSPA"),
	PORT_DESC(.port_id =  2, .port_name = "DSPB"),
	PORT_DESC(.port_id =  4, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  5, .port_name = "USB_AHB"),
	PORT_DESC(.port_id =  6, .port_name = "PROD_I2C"),
	PORT_DESC(.port_id =  7, .port_name = "USB HOST"),
	/* start of each device */
	PORT_DESC(.port_id = 4, .sub_id =  1, .port_name = "SPICC"),
	PORT_DESC(.port_id = 4, .sub_id =  3, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 4, .sub_id =  5, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 4, .sub_id =  6, .port_name = "DMA")
};

static struct ddr_port_desc ddr_port_desc_c2[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "TEE"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "REE"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "JTAG/PROD"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "RICV"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "DSPA"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "JPEG"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "WAVE"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "GDC"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "GE2D SRC0/DEST"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "GE2D SRC1"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "USB Devi"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "USB Host"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "ISP"),
	PORT_DESC(.port_id = 13, .bus = 13, .port_name = "NNA"),
	PORT_DESC(.port_id = 14, .bus = 14, .port_name = "VAD"),
	PORT_DESC(.port_id = 15, .bus = 15, .port_name = "DMC TEST"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "ETH"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "SPICC_A"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "SPICC_B"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "SDEMMC_A"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "SDEMMC_B"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "DMA"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "SDEMMC_C"),
	PORT_DESC(.port_id = 24, .bus = 24, .port_name = "undefine")
};

static struct ddr_port_desc ddr_port_desc_g12a[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "H265ENC"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_g12b[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM-ADDR-E"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "H265ENC"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "NNA"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "GDC"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "MIPI ISP"),
	PORT_DESC(.port_id = 13, .bus = 13, .port_name = "ARM-ADDR-F"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_sm1[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "H265ENC"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "NNA"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA")
};

static struct ddr_port_desc ddr_port_desc_tm2[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "DSPA"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "DSPB"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "NNA"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "PCIE1"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "PARSER1"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .bus =  7, .port_name = "DEMODE")
};

static struct ddr_port_desc ddr_port_desc_t5[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "PARSER1"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AO CPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .bus =  7, .port_name = "DEMODE")
};

static struct ddr_port_desc ddr_port_desc_t7[] __initdata = {
	/* CCI */
	PORT_DESC(.port_id =  1, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  2, .bus =  0, .port_name = "ARM_A73"),
	PORT_DESC(.port_id =  3, .bus =  0, .port_name = "MALI0"),
	PORT_DESC(.port_id =  4, .bus =  0, .port_name = "MALI1"),
	PORT_DESC(.port_id =  5, .bus =  0, .port_name = "CCI"),
	/* NIC1 */
	PORT_DESC(.port_id =  8, .bus =  3, .port_name = "NNA0"),
	PORT_DESC(.port_id =  9, .bus =  3, .port_name = "NNA1"),
	PORT_DESC(.port_id = 10, .bus =  3, .port_name = "NNA2"),
	PORT_DESC(.port_id = 11, .bus =  3, .port_name = "NNA3"),
	/* NIC2 */
	PORT_DESC(.port_id = 16, .bus =  4, .port_name = "HDMI_RX"),
	PORT_DESC(.port_id = 17, .bus =  4, .port_name = "USB2"),
	PORT_DESC(.port_id = 18, .bus =  4, .port_name = "USB3"),
	PORT_DESC(.port_id = 19, .bus =  4, .port_name = "PCIE"),
	PORT_DESC(.port_id = 24, .bus =  4, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 25, .bus =  4, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 26, .bus =  4, .port_name = "SDEMMCA"),
	PORT_DESC(.port_id = 27, .bus =  4, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 28, .bus =  4, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 29, .bus =  4, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 34, .bus =  4, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 35, .bus =  4, .port_name = "AFIFO"),
	PORT_DESC(.port_id = 36, .bus =  4, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 37, .bus =  4, .port_name = "SPICC3"),
	PORT_DESC(.port_id = 38, .bus =  4, .port_name = "SPICC4"),
	PORT_DESC(.port_id = 39, .bus =  4, .port_name = "SPICC5"),
	/* NIC3 */
	PORT_DESC(.port_id = 48, .bus =  6, .port_name = "GE2D"),
	PORT_DESC(.port_id = 49, .bus =  6, .port_name = "GDC"),
	PORT_DESC(.port_id = 50, .bus =  6, .port_name = "DEWARP"),
	PORT_DESC(.port_id = 51, .bus =  6, .port_name = "VDEC"),
	PORT_DESC(.port_id = 52, .bus =  6, .port_name = "HEVC_F"),
	PORT_DESC(.port_id = 53, .bus =  6, .port_name = "HEVC_B"),
	PORT_DESC(.port_id = 54, .bus =  6, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 55, .bus =  6, .port_name = "WAVE521"),
	/* NIC4 */
	PORT_DESC(.port_id = 63, .bus =  7, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 64, .bus =  7, .port_name = "ISP0"),
	PORT_DESC(.port_id = 65, .bus =  7, .port_name = "ISP1"),
	PORT_DESC(.port_id = 66, .bus =  7, .port_name = "ISP2"),
	PORT_DESC(.port_id = 67, .bus =  7, .port_name = "ISP3"),
	PORT_DESC(.port_id = 68, .bus =  7, .port_name = "DSPA"),
	PORT_DESC(.port_id = 69, .bus =  7, .port_name = "DSPB"),
	PORT_DESC(.port_id = 70, .bus =  7, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 71, .bus =  7, .port_name = "JTAG"),
	PORT_DESC(.port_id = 72, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 73, .bus =  7, .port_name = "DMA"),
	/* NIC5 */
	PORT_DESC(.port_id = 80, .bus =  2, .port_name = "VPU0"),
	PORT_DESC(.port_id = 81, .bus =  2, .port_name = "VPU1"),
	PORT_DESC(.port_id = 82, .bus =  2, .port_name = "VPU2")
};

static struct ddr_port_desc ddr_port_desc_t3[] __initdata = {
	PORT_DESC(.port_id =  2, .bus =  0, .port_name = "ARM_A55"),
	PORT_DESC(.port_id =  4, .bus =  0, .port_name = "MALI"),
	/* NoC_dev */
	PORT_DESC(.port_id = 16, .bus =  4, .port_name = "HDMI_RX"),
	PORT_DESC(.port_id = 17, .bus =  4, .port_name = "USB2"),
	PORT_DESC(.port_id = 18, .bus =  4, .port_name = "USB3"),
	PORT_DESC(.port_id = 19, .bus =  4, .port_name = "PCIE"),
	PORT_DESC(.port_id = 20, .bus =  4, .port_name = "TVFE"),
	PORT_DESC(.port_id = 21, .bus =  4, .port_name = "USB2_DEVICE"),
	PORT_DESC(.port_id = 27, .bus =  4, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 28, .bus =  4, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 34, .bus =  4, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 35, .bus =  4, .port_name = "AFIFO"),
	PORT_DESC(.port_id = 36, .bus =  4, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 37, .bus =  4, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 38, .bus =  4, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 39, .bus =  4, .port_name = "SPICC2"),
	/* NoC_dos */
	PORT_DESC(.port_id = 48, .bus =  6, .port_name = "GE2D"),
	PORT_DESC(.port_id = 51, .bus =  6, .port_name = "VDEC"),
	PORT_DESC(.port_id = 52, .bus =  6, .port_name = "HEVC_F"),
	PORT_DESC(.port_id = 53, .bus =  6, .port_name = "HEVC_B"),
	PORT_DESC(.port_id = 54, .bus =  6, .port_name = "HCODEC"),
	/* NoC_sys */
	PORT_DESC(.port_id = 63, .bus =  7, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 64, .bus =  7, .port_name = "NNA"),
	PORT_DESC(.port_id = 68, .bus =  7, .port_name = "DSP"),
	PORT_DESC(.port_id = 70, .bus =  7, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 71, .bus =  7, .port_name = "JTAG"),
	PORT_DESC(.port_id = 72, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 73, .bus =  7, .port_name = "DMA"),
	/* NoC_vpu */
	PORT_DESC(.port_id = 80, .bus =  2, .port_name = "VPU0"),
	PORT_DESC(.port_id = 81, .bus =  2, .port_name = "VPU1"),
	PORT_DESC(.port_id = 82, .bus =  2, .port_name = "VPU2"),
	PORT_DESC(.port_id = 83, .bus =  2, .port_name = "FRC0"),
	PORT_DESC(.port_id = 84, .bus =  2, .port_name = "FRC1"),
	PORT_DESC(.port_id = 85, .bus =  2, .port_name = "FRC2")
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
static struct ddr_port_desc ddr_port_desc_s4[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .bus =  7, .port_name = "DEMOD")
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct ddr_port_desc ddr_port_desc_sc2[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "PCIE"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDCP"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC FRONT"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB3.0"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "HEVC BACK"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "H265ENC"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "DSPA"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 22, .bus = 22, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "SD_EMMC_A"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETHERNET")
};

static struct ddr_port_desc ddr_port_desc_p1[] __initdata = {
	/* TOP*/
	PORT_DESC(.port_id =  2, .bus =  0, .port_name = "ARM_A76/A55"),
	PORT_DESC(.port_id =  5, .bus =  1, .port_name = "SMMU"),
	/* NIC_NNA_0 */
	PORT_DESC(.port_id =  8, .bus =  2, .port_name = "NNA_A"),
	PORT_DESC(.port_id =  9, .bus =  2, .port_name = "NNA_B"),
	PORT_DESC(.port_id = 10, .bus =  2, .port_name = "NNA_C"),
	/* NIC_NNA_1 */
	PORT_DESC(.port_id = 11, .bus =  3, .port_name = "NNA_D"),
	PORT_DESC(.port_id = 12, .bus =  3, .port_name = "NNA_E"),
	PORT_DESC(.port_id = 13, .bus =  3, .port_name = "NNA_F"),
	/* NIC_ISP */
	PORT_DESC(.port_id = 16, .bus =  4, .port_name = "ISP_A"),
	PORT_DESC(.port_id = 17, .bus =  4, .port_name = "ISP_B"),
	PORT_DESC(.port_id = 18, .bus =  4, .port_name = "ISP_C"),
	PORT_DESC(.port_id = 19, .bus =  4, .port_name = "ISP_D"),
	PORT_DESC(.port_id = 20, .bus =  4, .port_name = "ISP_E"),
	PORT_DESC(.port_id = 21, .bus =  4, .port_name = "ISP_F"),
	PORT_DESC(.port_id = 22, .bus =  4, .port_name = "ISP_G"),
	/* NIC_FDLE*/
	PORT_DESC(.port_id = 32, .bus =  6, .port_name = "MOP_A"),
	PORT_DESC(.port_id = 33, .bus =  6, .port_name = "MOP_B"),
	PORT_DESC(.port_id = 34, .bus =  6, .port_name = "DEP_A"),
	PORT_DESC(.port_id = 35, .bus =  6, .port_name = "DEP_B"),
	/* NIC_VFE*/
	PORT_DESC(.port_id = 40, .bus =  8, .port_name = "VFE"),
	PORT_DESC(.port_id = 41, .bus =  8, .port_name = "GE2D"),
	PORT_DESC(.port_id = 42, .bus =  8, .port_name = "DEWARP_A"),
	PORT_DESC(.port_id = 43, .bus =  8, .port_name = "DEWARP_B"),
	PORT_DESC(.port_id = 44, .bus =  8, .port_name = "DEWARP_C"),
	/* NIC_SYS*/
	PORT_DESC(.port_id = 48, .bus =  7, .port_name = "USB3_A"),
	PORT_DESC(.port_id = 49, .bus =  7, .port_name = "USB3_B"),
	PORT_DESC(.port_id = 50, .bus =  7, .port_name = "USB3_C"),
	PORT_DESC(.port_id = 51, .bus =  7, .port_name = "PCIE"),
	PORT_DESC(.port_id = 67, .bus =  7, .port_name = "M4"),
	PORT_DESC(.port_id = 68, .bus =  7, .port_name = "DSP_A"),
	PORT_DESC(.port_id = 69, .bus =  7, .port_name = "DSP_B"),
	PORT_DESC(.port_id = 70, .bus =  7, .port_name = "AO_CPU"),
	PORT_DESC(.port_id = 71, .bus =  7, .port_name = "JTAG"),
	PORT_DESC(.port_id = 72, .bus =  7, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 73, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 74, .bus =  7, .port_name = "SDEMMCA"),
	PORT_DESC(.port_id = 75, .bus =  7, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 76, .bus =  7, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 77, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 82, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 83, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 84, .bus =  7, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 85, .bus =  7, .port_name = "SPICC3"),
	PORT_DESC(.port_id = 86, .bus =  7, .port_name = "SPICC4"),
	PORT_DESC(.port_id = 87, .bus =  7, .port_name = "SPICC5"),
	PORT_DESC(.port_id = 88, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 96, .bus =  7, .port_name = "EMMC"),
	PORT_DESC(.port_id = 97, .bus =  7, .port_name = "DMA")
};

static struct ddr_port_desc ddr_port_desc_t5w[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A55"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDMI"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "USB"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE0"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "TVFE"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "DEVICE1"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "USB0"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "USB1"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "AUCPU"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "AO2DDR"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "SANA"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "ARBTODDR"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "SD_EMMC_B"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "AIFIFO"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SD_EMMC_C"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus =  7, .port_name = "SPICC0")
};

static struct ddr_port_desc ddr_port_desc_c3_prot[] __initdata = {
	PORT_DESC(.port_id = 2, .port_name = "ARM_A35"),
	PORT_DESC(.port_id = 8, .port_name = "NNA_DDR"),
	PORT_DESC(.port_id = 9, .port_name = "NNA_SRAM"),
	PORT_DESC(.port_id = 10, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 11, .port_name = "VP9000E"),
	PORT_DESC(.port_id = 16, .port_name = "ISP_A"),
	PORT_DESC(.port_id = 17, .port_name = "ISP_B"),
	PORT_DESC(.port_id = 18, .port_name = "ISP_C"),
	PORT_DESC(.port_id = 19, .port_name = "VOUT"),
	PORT_DESC(.port_id = 40, .port_name = "CEV_A"),
	PORT_DESC(.port_id = 41, .port_name = "CEV_B"),
	PORT_DESC(.port_id = 42, .port_name = "CEV_C"),
	PORT_DESC(.port_id = 43, .port_name = "GE2D"),
	PORT_DESC(.port_id = 44, .port_name = "DEWARP_A"),
	PORT_DESC(.port_id = 48, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 71, .port_name = "JTAG"),
	PORT_DESC(.port_id = 72, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 73, .port_name = "ETH"),
	PORT_DESC(.port_id = 74, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 80, .port_name = "EMMC"),
	PORT_DESC(.port_id = 81, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 82, .port_name = "SDIO_B"),
	PORT_DESC(.port_id = 83, .port_name = "SECU_TOP"),
	PORT_DESC(.port_id = 85, .port_name = "AUDIO")
};

static struct ddr_port_desc ddr_port_desc_c3_mon[] __initdata = {
	PORT_DESC(.port_id = 0, .bus =  0, .port_name = "ARM_A35"),
	PORT_DESC(.port_id = 1, .bus =  1, .port_name = "ISP"),
	PORT_DESC(.port_id = 2, .bus =  2, .port_name = "CVE"),
	PORT_DESC(.port_id = 3, .bus =  3, .port_name = "NNA"),
	PORT_DESC(.port_id = 4, .bus =  4, .port_name = "VC9000E"),
	PORT_DESC(.port_id = 6, .bus =  6, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 7, .bus =  7, .port_name = "HCODEC"),
};

static struct ddr_port_desc ddr_port_desc_t5m[] __initdata = {
	/* TOP*/
	PORT_DESC(.port_id =  2, .bus = 0x8080, .port_name = "ARM_A55"),
	PORT_DESC(.port_id =  4, .bus = 0x8181, .port_name = "MALI"),
	/* NIC_SYS */
	PORT_DESC(.port_id =  8, .bus = 0x8888, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 16, .bus = 0x8888, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 17, .bus = 0x8888, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 18, .bus = 0x8888, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 19, .bus = 0x8888, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 32, .bus = 0x8888, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 33, .bus = 0x8888, .port_name = "ETH"),
	PORT_DESC(.port_id = 36, .bus = 0x8888, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 37, .bus = 0x8888, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 38, .bus = 0x8888, .port_name = "DMA"),
	PORT_DESC(.port_id = 54, .bus = 0x8888, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 56, .bus = 0x8888, .port_name = "USB2_0"),
	PORT_DESC(.port_id = 57, .bus = 0x8888, .port_name = "USB2_1"),
	PORT_DESC(.port_id = 58, .bus = 0x8888, .port_name = "USB3"),
	PORT_DESC(.port_id = 59, .bus = 0x8888, .port_name = "TVFE"),
	PORT_DESC(.port_id = 60, .bus = 0x8888, .port_name = "HDMIRX"),
	PORT_DESC(.port_id = 61, .bus = 0x8888, .port_name = "JTAG"),
	/* NIC_VPU */
	PORT_DESC(.port_id = 62, .bus = 0x0082, .port_name = "VPU0"),
	PORT_DESC(.port_id = 64, .bus = 0x0083, .port_name = "VPU1"),
	PORT_DESC(.port_id = 66, .bus = 0x8200, .port_name = "VPU2"),
	/* NIC_EMEC*/
	PORT_DESC(.port_id = 68, .bus = 0x8400, .port_name = "FRC0"),
	PORT_DESC(.port_id = 70, .bus = 0x8600, .port_name = "FRC1"),
	PORT_DESC(.port_id = 72, .bus = 0x8700, .port_name = "FRC2"),
	/* NIC_DOS*/
	PORT_DESC(.port_id = 74, .bus = 0x0086, .port_name = "HEVC"),
	PORT_DESC(.port_id = 82, .bus = 0x0084, .port_name = "VDEC"),
	PORT_DESC(.port_id = 90, .bus = 0x8387, .port_name = "HCODEC"),
	/* NIC_GE2D*/
	PORT_DESC(.port_id = 98, .bus = 0x8387, .port_name = "GE2D"),
};

static struct ddr_port_desc ddr_port_desc_s5[] __initdata = {
	/* TOP*/
	PORT_DESC(.port_id =  2, .bus =  0, .port_name = "ARM_A76/A55"),
	PORT_DESC(.port_id =  4, .bus =  0, .port_name = "MALI"),
	/* NIC_NPU */
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "ADLA_4T_0"),
	PORT_DESC(.port_id =  9, .bus =  8, .port_name = "ADLA_4T_1"),
	/* NIC_SYS */
	PORT_DESC(.port_id = 16, .bus =  4, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 24, .bus =  4, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 25, .bus =  4, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 26, .bus =  4, .port_name = "SPICC2"),
	PORT_DESC(.port_id = 28, .bus =  4, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 29, .bus =  4, .port_name = "ETHERNET"),
	PORT_DESC(.port_id = 33, .bus =  4, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 35, .bus =  4, .port_name = "SDEMMCA"),
	PORT_DESC(.port_id = 36, .bus =  4, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 37, .bus =  4, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 38, .bus =  4, .port_name = "DMA"),
	PORT_DESC(.port_id = 54, .bus =  4, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 56, .bus =  4, .port_name = "USB3_A"),
	PORT_DESC(.port_id = 57, .bus =  4, .port_name = "USB3_B"),
	PORT_DESC(.port_id = 58, .bus =  4, .port_name = "USB2"),
	PORT_DESC(.port_id = 59, .bus =  4, .port_name = "PCIE0"),
	PORT_DESC(.port_id = 60, .bus =  4, .port_name = "PCIE1"),
	/* NIC_VPU */
	PORT_DESC(.port_id = 64, .bus =  2, .port_name = "VPU0"),
	PORT_DESC(.port_id = 66, .bus =  2, .port_name = "VPU1"),
	PORT_DESC(.port_id = 68, .bus =  2, .port_name = "VPU2"),
	PORT_DESC(.port_id = 70, .bus =  2, .port_name = "VPU3"),
	/* NIC_DOS*/
	PORT_DESC(.port_id = 72, .bus =  3, .port_name = "HEVC0_F"),
	PORT_DESC(.port_id = 80, .bus =  3, .port_name = "HEVC1_B0"),
	PORT_DESC(.port_id = 81, .bus =  3, .port_name = "HEVC2_B1"),
	PORT_DESC(.port_id = 88, .bus =  3, .port_name = "VDEC"),
	PORT_DESC(.port_id = 96, .bus =  3, .port_name = "HCODEC"),
	/* NIC_VGE*/
	PORT_DESC(.port_id = 104, .bus =  7, .port_name = "GE2D"),
	PORT_DESC(.port_id = 106, .bus =  7, .port_name = "VIDEO_CMPR"),
	PORT_DESC(.port_id = 108, .bus =  7, .port_name = "VC9000E")
};

static struct ddr_port_desc ddr_port_desc_s6[] __initdata = {
	/* TOP */
	PORT_DESC(.port_id =   0, .bus =  0, .port_name = "ARM"),
	PORT_DESC(.port_id =   1, .bus =  1, .port_name = "VPU0"),
	PORT_DESC(.port_id =   2, .bus =  2, .port_name = "VPU1"),
	PORT_DESC(.port_id =   3, .bus =  3, .port_name = "VPU2"),
	PORT_DESC(.port_id =   6, .bus =  6, .port_name = "HEVC_B"),
	PORT_DESC(.port_id =   7, .bus =  7, .port_name = "HEVC_F"),
	PORT_DESC(.port_id =   8, .bus =  8, .port_name = "VDEC"),
	PORT_DESC(.port_id =   9, .bus =  9, .port_name = "MTE"),
	PORT_DESC(.port_id =  11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id =  12, .bus = 12, .port_name = "MALI"),
	PORT_DESC(.port_id =  13, .bus = 13, .port_name = "VGE"),
	PORT_DESC(.port_id =  14, .bus = 14, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  15, .bus = 15, .port_name = "DSP"),
	PORT_DESC(.port_id =  16, .bus = 16, .port_name = "AMFC"),
	PORT_DESC(.port_id =  17, .bus = 17, .port_name = "PCIE"),
	PORT_DESC(.port_id =  18, .bus = 18, .port_name = "NNA"),
	PORT_DESC(.port_id =  19, .bus = 19, .port_name = "VC9000E"),

	/* SUB DEVICE */
	PORT_DESC(.port_id = 14, .sub_id =  8, .bus = 14, .port_name = "SDIO_B"),
	PORT_DESC(.port_id = 14, .sub_id =  9, .bus = 14, .port_name = "EMMC"),
	PORT_DESC(.port_id = 14, .sub_id = 12, .bus = 14, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 14, .sub_id = 16, .bus = 14, .port_name = "AUCPU"),
	PORT_DESC(.port_id = 14, .sub_id = 17, .bus = 14, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 14, .sub_id = 19, .bus = 14, .port_name = "USB3DRD"),
	PORT_DESC(.port_id = 14, .sub_id = 24, .bus = 14, .port_name = "SPICCX2"),
	PORT_DESC(.port_id = 14, .sub_id = 25, .bus = 14, .port_name = "ETH"),
	PORT_DESC(.port_id = 14, .sub_id = 48, .bus = 14, .port_name = "AUDIO"),

	/* SUB VGE */
	PORT_DESC(.port_id = 13, .sub_id =  0, .bus = 13, .port_name = "VICP"),
	PORT_DESC(.port_id = 13, .sub_id =  1, .bus = 13, .port_name = "GE2D"),
	PORT_DESC(.port_id = 13, .sub_id =  2, .bus = 13, .port_name = "DEWRAP")
};

static struct ddr_port_desc ddr_port_desc_t3x[] __initdata = {
	/* TOP*/
	PORT_DESC(.port_id =  2, .bus =  0, .port_name = "ARM_A76/A55"),
	PORT_DESC(.port_id =  4, .bus =  0, .port_name = "MALI"),
	/* NIC_NPU */
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "ADLA_4T_0"),
	PORT_DESC(.port_id =  9, .bus =  8, .port_name = "ADLA_4T_1"),
	/* NIC_SYS */
	PORT_DESC(.port_id = 16, .bus =  4, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 19, .bus =  4, .port_name = "I2C_MONITOR"),
	PORT_DESC(.port_id = 21, .bus =  4, .port_name = "SPI_SG0"),
	PORT_DESC(.port_id = 22, .bus =  4, .port_name = "SPI_SG1"),
	PORT_DESC(.port_id = 23, .bus =  4, .port_name = "USB3_B"),
	PORT_DESC(.port_id = 32, .bus =  4, .port_name = "AUDMA"),
	PORT_DESC(.port_id = 33, .bus =  4, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 35, .bus =  4, .port_name = "SDEMMCA"),
	PORT_DESC(.port_id = 36, .bus =  4, .port_name = "SDEMMCB"),
	PORT_DESC(.port_id = 37, .bus =  4, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 38, .bus =  4, .port_name = "DMA"),
	PORT_DESC(.port_id = 54, .bus =  4, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 56, .bus =  4, .port_name = "HDMIRX"),
	PORT_DESC(.port_id = 57, .bus =  4, .port_name = "USB3_A"),
	PORT_DESC(.port_id = 58, .bus =  4, .port_name = "USB2"),
	PORT_DESC(.port_id = 59, .bus =  4, .port_name = "PCIE0"),
	PORT_DESC(.port_id = 60, .bus =  4, .port_name = "DSPA"),
	PORT_DESC(.port_id = 61, .bus =  4, .port_name = "JTAG"),
	PORT_DESC(.port_id = 62, .bus =  4, .port_name = "ETH"),
	PORT_DESC(.port_id = 63, .bus =  4, .port_name = "TVFE"),
	/* NIC_VPU */
	PORT_DESC(.port_id = 64, .bus =  2, .port_name = "VPU0"),
	PORT_DESC(.port_id = 66, .bus =  2, .port_name = "VPU1"),
	PORT_DESC(.port_id = 68, .bus =  2, .port_name = "VPU2"),
	PORT_DESC(.port_id = 70, .bus =  2, .port_name = "VPU3"),
	/* NIC_DOS*/
	PORT_DESC(.port_id = 72, .bus =  3, .port_name = "HEVC0_F"),
	PORT_DESC(.port_id = 80, .bus =  3, .port_name = "HEVC1_B0"),
	PORT_DESC(.port_id = 81, .bus =  3, .port_name = "HEVC2_B1"),
	PORT_DESC(.port_id = 88, .bus =  3, .port_name = "VDEC"),
	PORT_DESC(.port_id = 96, .bus =  3, .port_name = "HCODEC"),
	/* NIC_VGE*/
	PORT_DESC(.port_id = 104, .bus =  7, .port_name = "GE2D"),
	PORT_DESC(.port_id = 106, .bus =  7, .port_name = "VIDEO_CMPR"),
	PORT_DESC(.port_id = 116, .bus =  7, .port_name = "DEMOD"),
	/* NIC_FRC*/
	PORT_DESC(.port_id = 117, .bus =  1, .port_name = "FRC0"),
	PORT_DESC(.port_id = 119, .bus =  1, .port_name = "FRC1"),
	PORT_DESC(.port_id = 121, .bus =  1, .port_name = "FRC2"),
	PORT_DESC(.port_id = 123, .bus =  1, .port_name = "FRC3")
};

static struct ddr_port_desc ddr_port_desc_txhd2[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "MALI0"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "MALI1"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "HDMI"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "USB2HOST"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 17, .bus = 17, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id = 18, .bus = 18, .port_name = "VPU2 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 20, .bus = 20, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	PORT_DESC(.port_id = 23, .bus = 23, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus =  7, .port_name = "ARB0"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "AFIFO"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus =  7, .port_name = "PARSER1"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus =  7, .port_name = "PARSER0"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus =  7, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus =  7, .port_name = "SDEMMCC"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus =  7, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id = 15, .bus =  7, .port_name = "DEMOD")
};

static struct ddr_port_desc ddr_port_desc_s7[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "HEVC"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "VDEC"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "MALI"),
	PORT_DESC(.port_id = 13, .bus = 13, .port_name = "GE2D"),
	/* start of each device */
	PORT_DESC(.port_id = 10, .sub_id =  0, .bus = 10, .port_name = "SDIO_B"),
	PORT_DESC(.port_id = 10, .sub_id =  1, .bus = 10, .port_name = "EMMC"),
	PORT_DESC(.port_id = 10, .sub_id =  2, .bus = 10, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 10, .sub_id =  3, .bus = 10, .port_name = "ETH"),
	PORT_DESC(.port_id = 10, .sub_id =  4, .bus = 10, .port_name = "SECU_DMA"),
	PORT_DESC(.port_id = 10, .sub_id =  7, .bus = 10, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 10, .sub_id =  8, .bus = 10, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 10, .sub_id =  9, .bus = 10, .port_name = "USB_X2H"),
	PORT_DESC(.port_id = 10, .sub_id = 10, .bus = 10, .port_name = "SPICCX2"),
	PORT_DESC(.port_id = 10, .sub_id = 11, .bus = 10, .port_name = "AUCPU"),
};

static struct ddr_port_desc ddr_port_desc_s7d[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  1, .bus =  1, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "VPU1 READ"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "VPU1 WRITE"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "HEVC"),
	PORT_DESC(.port_id =  8, .bus =  8, .port_name = "VDEC"),
	PORT_DESC(.port_id =  9, .bus =  9, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 10, .bus = 10, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 12, .bus = 12, .port_name = "MALI"),
	PORT_DESC(.port_id = 13, .bus = 13, .port_name = "GE2D"),
	PORT_DESC(.port_id = 14, .bus = 14, .port_name = "AMFC"),
	/* start of each device */
	PORT_DESC(.port_id = 10, .sub_id =  0, .bus = 10, .port_name = "SDIO_B"),
	PORT_DESC(.port_id = 10, .sub_id =  1, .bus = 10, .port_name = "EMMC"),
	PORT_DESC(.port_id = 10, .sub_id =  2, .bus = 10, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 10, .sub_id =  3, .bus = 10, .port_name = "ETH"),
	PORT_DESC(.port_id = 10, .sub_id =  4, .bus = 10, .port_name = "SECU_DMA"),
	PORT_DESC(.port_id = 10, .sub_id =  7, .bus = 10, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 10, .sub_id =  8, .bus = 10, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 10, .sub_id =  9, .bus = 10, .port_name = "USB_X2H"),
	PORT_DESC(.port_id = 10, .sub_id = 10, .bus = 10, .port_name = "SPICCX2"),
	PORT_DESC(.port_id = 10, .sub_id = 11, .bus = 10, .port_name = "AUCPU"),
};

static struct ddr_port_desc ddr_port_desc_t6d[] __initdata = {
	PORT_DESC(.port_id =  0, .bus = 0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus = 1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus = 2, .port_name = "VPU0"),
	PORT_DESC(.port_id =  3, .bus = 3, .port_name = "VPU1"),
	PORT_DESC(.port_id =  4, .bus = 4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  6, .bus = 6, .port_name = "GE2D"),
	PORT_DESC(.port_id =  7, .bus = 7, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  8, .bus = 8, .port_name = "AMFC"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus = 7, .port_name = "EMMC"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus = 7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id =  3, .bus = 7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus = 7, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 7, .sub_id =  5, .bus = 7, .port_name = "IOTM"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus = 7, .port_name = "TVFE"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus = 7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id =  8, .bus = 7, .port_name = "HDMI"),
	PORT_DESC(.port_id = 7, .sub_id =  9, .bus = 7, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus = 7, .port_name = "USB2_0"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus = 7, .port_name = "USB2_1"),
	PORT_DESC(.port_id = 7, .sub_id = 12, .bus = 7, .port_name = "USB2_2"),
	PORT_DESC(.port_id = 7, .sub_id = 13, .bus = 7, .port_name = "I2CMON"),
	PORT_DESC(.port_id = 7, .sub_id = 14, .bus = 7, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 7, .sub_id = 16, .bus = 7, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 18, .bus = 7, .port_name = "DAP"),
};

static struct ddr_port_desc ddr_port_desc_t6w[] __initdata = {
	PORT_DESC(.port_id =  0, .bus = 0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus = 1, .port_name = "MALI"),
	PORT_DESC(.port_id =  2, .bus = 2, .port_name = "VPU0"),
	PORT_DESC(.port_id =  3, .bus = 3, .port_name = "VPU1"),
	PORT_DESC(.port_id =  4, .bus = 4, .port_name = "VPU2_WRITE"),
	PORT_DESC(.port_id =  6, .bus = 6, .port_name = "VPU2_READ"),
	PORT_DESC(.port_id =  7, .bus = 7, .port_name = "VPU3_READ"),
	PORT_DESC(.port_id =  8, .bus = 8, .port_name = "HEVC"),
	PORT_DESC(.port_id =  9, .bus = 9, .port_name = "GE2D"),
	PORT_DESC(.port_id =  10, .bus = 10, .port_name = "DEVICE"),
	PORT_DESC(.port_id =  11, .bus = 11, .port_name = "AMFC"),
	/* start of each device */
	PORT_DESC(.port_id = 10, .sub_id =  0, .bus = 10, .port_name = "EMMC"),
	PORT_DESC(.port_id = 10, .sub_id =  2, .bus = 10, .port_name = "DMA"),
	PORT_DESC(.port_id = 10, .sub_id =  3, .bus = 10, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 10, .sub_id =  4, .bus = 10, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 10, .sub_id =  5, .bus = 10, .port_name = "IOTM"),
	PORT_DESC(.port_id = 10, .sub_id =  6, .bus = 10, .port_name = "TVFE"),
	PORT_DESC(.port_id = 10, .sub_id =  7, .bus = 10, .port_name = "ETH"),
	PORT_DESC(.port_id = 10, .sub_id =  8, .bus = 10, .port_name = "HDMI"),
	PORT_DESC(.port_id = 10, .sub_id =  9, .bus = 10, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 10, .sub_id = 10, .bus = 10, .port_name = "USB2_0"),
	PORT_DESC(.port_id = 10, .sub_id = 11, .bus = 10, .port_name = "USB2_1"),
	PORT_DESC(.port_id = 10, .sub_id = 12, .bus = 10, .port_name = "USB2_2"),
	PORT_DESC(.port_id = 10, .sub_id = 13, .bus = 10, .port_name = "I2CMON"),
	PORT_DESC(.port_id = 10, .sub_id = 14, .bus = 10, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 10, .sub_id = 16, .bus = 10, .port_name = "SPICC"),
	PORT_DESC(.port_id = 10, .sub_id = 18, .bus = 10, .port_name = "DAP"),
	PORT_DESC(.port_id = 10, .sub_id = 19, .bus = 10, .port_name = "BCON"),
};

static struct ddr_port_desc ddr_port_desc_t6x[] __initdata = {
	PORT_DESC(.port_id =  0, .bus_id = 0, .bus = 0, .mdc_bus = 0, .port_name = "ARM"),
	PORT_DESC(.port_id =  8, .bus_id = 1, .bus = 1, .mdc_bus = 0, .port_name = "VPU0"),
	PORT_DESC(.port_id = 16, .bus_id = 2, .bus = 2, .mdc_bus = 0, .port_name = "VPU2"),
	PORT_DESC(.port_id = 24, .bus_id = 3, .bus = 3, .mdc_bus = 0, .port_name = "MALI"),
	PORT_DESC(.port_id = 25, .bus_id = 4, .bus = 3, .mdc_bus = 1, .port_name = "HEVC"),
	PORT_DESC(.port_id = 26, .bus_id = 5, .bus = 3, .mdc_bus = 2, .port_name = "GE2D"),
	PORT_DESC(.port_id = 27, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 28, .bus_id = 7, .bus = 3, .mdc_bus = 4, .port_name = "AMFC"),
	PORT_DESC(.port_id = 29, .bus_id = 8, .bus = 3, .mdc_bus = 5, .port_name = "HCODEC"),
	PORT_DESC(.port_id = 30, .bus_id = 9, .bus = 3, .mdc_bus = 6, .port_name = "TEST"),
	/* start of each device */
	PORT_DESC(.port_id = 27, .sub_id =  0, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "EMMC"),
	PORT_DESC(.port_id = 27, .sub_id =  2, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "DMA"),
	PORT_DESC(.port_id = 27, .sub_id =  3, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 27, .sub_id =  4, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 27, .sub_id =  5, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "CSS/IOTM"),
	PORT_DESC(.port_id = 27, .sub_id =  6, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "TVFE"),
	PORT_DESC(.port_id = 27, .sub_id =  7, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "ETH"),
	PORT_DESC(.port_id = 27, .sub_id =  8, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "HDMIRX"),
	PORT_DESC(.port_id = 27, .sub_id =  9, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 27, .sub_id = 10, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "USB3_0"),
	PORT_DESC(.port_id = 27, .sub_id = 11, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "USB2_1"),
	PORT_DESC(.port_id = 27, .sub_id = 12, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "USB2_2"),
	PORT_DESC(.port_id = 27, .sub_id = 13, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "I2CMON"),
	PORT_DESC(.port_id = 27, .sub_id = 14, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 27, .sub_id = 15, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "AUCPU"),
	PORT_DESC(.port_id = 27, .sub_id = 16, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "SPISG"),
	PORT_DESC(.port_id = 27, .sub_id = 19, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "BCON"),
	PORT_DESC(.port_id = 27, .sub_id = 21, .bus_id = 6, .bus = 3, .mdc_bus = 3, .port_name = "DAP"),
};

static struct ddr_port_desc ddr_port_desc_c4[] __initdata = {
	PORT_DESC(.port_id =  0, .bus = 0, .port_name = "ARM"),
	PORT_DESC(.port_id =  1, .bus = 1, .port_name = "ISP"),
	PORT_DESC(.port_id =  2, .bus = 2, .port_name = "CVE_GE2D"),
	PORT_DESC(.port_id =  3, .bus = 3, .port_name = "NNA"),
	PORT_DESC(.port_id =  4, .bus = 4, .port_name = "VEDIO_ENCODER"),
	PORT_DESC(.port_id =  6, .bus = 6, .port_name = "JPEG_ENCODER"),
	PORT_DESC(.port_id =  7, .bus = 7, .port_name = "DEVICE"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id = 0, .bus = 7, .port_name = "AMFC"),
	PORT_DESC(.port_id = 7, .sub_id = 1, .bus = 7, .port_name = "SD_EMMC"),
	PORT_DESC(.port_id = 7, .sub_id = 2, .bus = 7, .port_name = "DMA"),
	PORT_DESC(.port_id = 7, .sub_id = 3, .bus = 7, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 7, .sub_id = 6, .bus = 7, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 7, .sub_id = 8, .bus = 7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id = 9, .bus = 7, .port_name = "SDIO_B"),
	PORT_DESC(.port_id = 7, .sub_id = 10, .bus = 7, .port_name = "SPICC"),
	PORT_DESC(.port_id = 7, .sub_id = 11, .bus = 7, .port_name = "ETH"),
};

static struct ddr_port_desc ddr_port_desc_a9_prot[] __initdata = {
	{ .port_id =  0, .bus =  0, .port_name = "ARM_A78"       },
	{ .port_id =  1, .bus =  1, .port_name = "ARM_A55"       },
	{ .port_id =  2, .bus =  2, .port_name = "VPU0"          },
	{ .port_id =  3, .bus =  3, .port_name = "VPU1"          },
	{ .port_id =  4, .bus =  4, .port_name = "NNA"           },
	{ .port_id =  5, .bus =  5, .port_name = "DMC_TEST"      },
	{ .port_id =  6, .bus =  6, .port_name = "HEVC_B"        },
	{ .port_id =  7, .bus =  7, .port_name = "HEVC_F"        },
	{ .port_id =  8, .bus =  8, .port_name = "MIPI"          },
	{ .port_id = 11, .bus = 11, .port_name = "DEMUX"         },
	{ .port_id = 12, .bus = 12, .port_name = "MALI"          },
	{ .port_id = 16, .bus =  9, .port_name = "CVE"           },
	{ .port_id = 17, .bus =  9, .port_name = "GE2D"          },
	{ .port_id = 18, .bus =  9, .port_name = "OPP"           },
	{ .port_id = 19, .bus =  9, .port_name = "VID_CMPR"      },
	{ .port_id = 20, .bus = 13, .port_name = "PCIE_IF1"      },
	{ .port_id = 21, .bus = 13, .port_name = "USB3B"         },
	{ .port_id = 22, .bus = 13, .port_name = "PCIE_IF0"      },
	{ .port_id = 23, .bus = 13, .port_name = "USB2T"         },
	{ .port_id = 24, .bus = 14, .port_name = "HCODEC"        },
	{ .port_id = 25, .bus = 14, .port_name = "VC9000E"       },
	{ .port_id = 28, .bus = 15, .port_name = "DSP"           },
	{ .port_id = 29, .bus = 15, .port_name = "HDMIRX"        },
	{ .port_id = 30, .bus = 15, .port_name = "AMFC"          },
	{ .port_id = 32, .bus = 10, .port_name = "AM_AXI"        },
	{ .port_id = 33, .bus = 10, .port_name = "DMA"           },
	{ .port_id = 34, .bus = 10, .port_name = "USBDRDA"       },
	{ .port_id = 35, .bus = 10, .port_name = "ETH"           },
	{ .port_id = 36, .bus = 10, .port_name = "SDIOB"         },
	{ .port_id = 37, .bus = 10, .port_name = "EMMC"          },
	{ .port_id = 38, .bus = 10, .port_name = "USB3A"         },
	{ .port_id = 39, .bus = 10, .port_name = "AUDIO"         },
	{ .port_id = 40, .bus = 10, .port_name = "PP_DMA"        },
	{ .port_id = 48, .bus = 10, .port_name = "SPICCX2"       },
	{ .port_id = 49, .bus = 10, .port_name = "SDIOA"         },
};

static struct ddr_port_desc ddr_port_desc_a9_mon[] __initdata = {
	{ .port_id =  0, .bus =  0, .port_name = "ARM_A78"       },
	{ .port_id =  1, .bus =  1, .port_name = "ARM_A55"       },
	{ .port_id =  2, .bus =  2, .port_name = "VPU0"          },
	{ .port_id =  3, .bus =  3, .port_name = "VPU1"          },
	{ .port_id =  4, .bus =  4, .port_name = "NNA"           },
	{ .port_id =  5, .bus =  4, .port_name = "DMC_TEST"      },
	{ .port_id =  6, .bus =  6, .port_name = "HEVC_B"        },
	{ .port_id =  7, .bus =  7, .port_name = "HEVC_F"        },
	{ .port_id =  8, .bus =  8, .port_name = "MIPI"          },
	{ .port_id =  9, .bus =  9, .port_name = "VGE"           },
	{ .port_id = 10, .bus = 10, .port_name = "DEVICE"        },
	{ .port_id = 11, .bus = 11, .port_name = "DEMUX"         },
	{ .port_id = 12, .bus = 12, .port_name = "MALI"          },
	{ .port_id = 13, .bus = 13, .port_name = "PP_DMC"        },
	{ .port_id = 14, .bus = 14, .port_name = "HC_VC"         },
	{ .port_id = 15, .bus = 15, .port_name = "ADH_MMC"       },
	/* start of VGE sub */
	{ .port_id = 32, .bus =  9, .port_name = "CVE"           },
	{ .port_id = 33, .bus =  9, .port_name = "GE2D"          },
	{ .port_id = 34, .bus =  9, .port_name = "OPP"           },
	{ .port_id = 35, .bus =  9, .port_name = "VID_CMPR"      },
	/* start of DEVICE sub */
	{ .port_id = 48, .bus = 10, .port_name = "AM_AXI"        },
	{ .port_id = 49, .bus = 10, .port_name = "DMA"           },
	{ .port_id = 50, .bus = 10, .port_name = "USBDRDA"       },
	{ .port_id = 51, .bus = 10, .port_name = "ETH"           },
	{ .port_id = 52, .bus = 10, .port_name = "SDIOB"         },
	{ .port_id = 53, .bus = 10, .port_name = "EMMC"          },
	{ .port_id = 54, .bus = 10, .port_name = "USB3A"         },
	{ .port_id = 55, .bus = 10, .port_name = "AUDIO"         },
	{ .port_id = 56, .bus = 10, .port_name = "PP_DMA"        },
	{ .port_id = 56, .bus = 10, .port_name = "SPICCX2"       },
	{ .port_id = 58, .bus = 10, .port_name = "SDIOA"         },
	/* start of PP_DMC sub */
	{ .port_id = 64, .bus = 13, .port_name = "PCIE_IF1"      },
	{ .port_id = 65, .bus = 13, .port_name = "USB3B"         },
	{ .port_id = 66, .bus = 13, .port_name = "PCIE_IF0"      },
	{ .port_id = 67, .bus = 13, .port_name = "USB2T"         },
	/* start of TAHOE sub */
	{ .port_id = 80, .bus = 14, .port_name = "HCODEC"        },
	{ .port_id = 81, .bus = 14, .port_name = "VC9000E"       },
	/* start of ADH_MMC sub */
	{ .port_id = 96, .bus = 15, .port_name = "DSP"           },
	{ .port_id = 97, .bus = 15, .port_name = "HDMIRX"        },
	{ .port_id = 98, .bus = 15, .port_name = "AMFC"          },
};
#endif
static struct ddr_port_desc ddr_port_desc_s1a[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A35"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "GE2D"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "HEVC"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE"),
	PORT_DESC(.port_id = 11, .bus = 11, .port_name = "DEMUX"),
	PORT_DESC(.port_id = 16, .bus = 16, .port_name = "VPU0 READ"),
	PORT_DESC(.port_id = 19, .bus = 19, .port_name = "VPU0 WRITE"),
	PORT_DESC(.port_id = 21, .bus = 21, .port_name = "VDEC"),
	/* start of each device */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "DEMOD"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 7, .sub_id =  4, .bus =  7, .port_name = "SDEMMC"),
	PORT_DESC(.port_id = 7, .sub_id =  6, .bus =  7, .port_name = "USB"),
	PORT_DESC(.port_id = 7, .sub_id =  7, .bus =  7, .port_name = "DMA")
};

static struct ddr_port_desc ddr_port_desc_a4_prot[] __initdata = {
	PORT_DESC(.port_id = 2, .port_name = "ARM_A53"),
	PORT_DESC(.port_id = 19, .port_name = "VOUT"),
	PORT_DESC(.port_id = 40, .port_name = "AOCPU"),
	PORT_DESC(.port_id = 41, .port_name = "AUDIO_VAD"),
	PORT_DESC(.port_id = 71, .port_name = "JTAG"),
	PORT_DESC(.port_id = 72, .port_name = "SPICC0"),
	PORT_DESC(.port_id = 73, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 74, .port_name = "ETH"),
	PORT_DESC(.port_id = 80, .port_name = "EMMC"),
	PORT_DESC(.port_id = 81, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 82, .port_name = "SECU_TOP"),
	PORT_DESC(.port_id = 83, .port_name = "USB_X2H"),
	PORT_DESC(.port_id = 84, .port_name = "USB2DRD"),
	PORT_DESC(.port_id = 85, .port_name = "AUDIO")
};

static struct ddr_port_desc ddr_port_desc_a4_mon[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  2, .bus =  2, .port_name = "VOUT"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "DSP"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "DEVICE1"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE0"),
	/* start of each device0 */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "SPICC0"),
	/* start of each device1 */
	PORT_DESC(.port_id = 6, .sub_id =  8, .bus =  6, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 6, .sub_id =  9, .bus =  6, .port_name = "SEC_TOP"),
	PORT_DESC(.port_id = 6, .sub_id = 10, .bus =  6, .port_name = "EMMC"),
	PORT_DESC(.port_id = 6, .sub_id = 11, .bus =  6, .port_name = "USB"),
	PORT_DESC(.port_id = 6, .sub_id = 12, .bus =  6, .port_name = "AUDIO"),
	PORT_DESC(.port_id = 6, .sub_id = 13, .bus =  6, .port_name = "USB_X2H"),
	/* start of each dsp */
	PORT_DESC(.port_id = 4, .sub_id = 16, .bus =  4, .port_name = "AUDIO_VAD"),
	PORT_DESC(.port_id = 4, .sub_id = 17, .bus =  4, .port_name = "AOCPU")
};

static struct ddr_port_desc ddr_port_desc_a5[] __initdata = {
	PORT_DESC(.port_id =  0, .bus =  0, .port_name = "ARM_A53"),
	PORT_DESC(.port_id =  3, .bus =  3, .port_name = "NNA"),
	PORT_DESC(.port_id =  4, .bus =  4, .port_name = "DSP"),
	PORT_DESC(.port_id =  5, .bus =  5, .port_name = "TEST"),
	PORT_DESC(.port_id =  6, .bus =  6, .port_name = "DEVICE1"),
	PORT_DESC(.port_id =  7, .bus =  7, .port_name = "DEVICE0"),

	/* start of each device0 */
	PORT_DESC(.port_id = 7, .sub_id =  0, .bus =  7, .port_name = "SPICC1"),
	PORT_DESC(.port_id = 7, .sub_id =  1, .bus =  7, .port_name = "ETH"),
	PORT_DESC(.port_id = 7, .sub_id =  2, .bus =  7, .port_name = "SPICC0"),

	/* start of each device1 */
	PORT_DESC(.port_id = 6, .sub_id =  8, .bus =  6, .port_name = "SDIO_A"),
	PORT_DESC(.port_id = 6, .sub_id =  9, .bus =  6, .port_name = "SEC_TOP"),
	PORT_DESC(.port_id = 6, .sub_id = 10, .bus =  6, .port_name = "EMMC"),
	PORT_DESC(.port_id = 6, .sub_id = 11, .bus =  6, .port_name = "USB"),
	PORT_DESC(.port_id = 6, .sub_id = 12, .bus =  6, .port_name = "AUDIO")
};

static struct ddr_port_desc *chip_ddr_port;
static unsigned int chip_ddr_port_num __initdata;
static struct ddr_port_desc *ddr_port_src __initdata;

int __init ddr_find_port_desc(int cpu_type, struct ddr_port_desc **desc)
{
	return ddr_find_port_desc_type(cpu_type, desc, 0);
}

/* type 0:dmc_monitor 1:ddr_bandwidth */
int __init ddr_find_port_desc_type(int cpu_type, struct ddr_port_desc **desc, int type)
{
	int desc_size = -EINVAL;

	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case DMC_TYPE_M8B:
		*desc = ddr_port_desc_m8b;
		desc_size = ARRAY_SIZE(ddr_port_desc_m8b);
		break;

	case DMC_TYPE_GXBB:
		*desc = ddr_port_desc_gxbb;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxbb);
		break;

	case DMC_TYPE_GXL:
		*desc = ddr_port_desc_gxl;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxl);
		break;

	case DMC_TYPE_GXM:
		*desc = ddr_port_desc_gxm;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxm);
		break;

	case DMC_TYPE_TXL:
		*desc = ddr_port_desc_txl;
		desc_size = ARRAY_SIZE(ddr_port_desc_txl);
		break;

	case DMC_TYPE_TXLX:
		*desc = ddr_port_desc_txlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_txlx);
		break;

	case DMC_TYPE_AXG:
		*desc = ddr_port_desc_axg;
		desc_size = ARRAY_SIZE(ddr_port_desc_axg);
		break;

	case DMC_TYPE_GXLX:
		*desc = ddr_port_desc_gxlx;
		desc_size = ARRAY_SIZE(ddr_port_desc_gxlx);
		break;

	case DMC_TYPE_TXHD:
		*desc = ddr_port_desc_txhd;
		desc_size = ARRAY_SIZE(ddr_port_desc_txhd);
		break;

	case DMC_TYPE_TL1:
		*desc = ddr_port_desc_tl1;
		desc_size = ARRAY_SIZE(ddr_port_desc_tl1);
		break;

	case DMC_TYPE_C1:
		*desc = ddr_port_desc_c1;
		desc_size = ARRAY_SIZE(ddr_port_desc_c1);
		break;
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_A1:
		*desc = ddr_port_desc_a1;
		desc_size = ARRAY_SIZE(ddr_port_desc_a1);
		break;

	case DMC_TYPE_C2:
		*desc = ddr_port_desc_c2;
		desc_size = ARRAY_SIZE(ddr_port_desc_c2);
		break;

	case DMC_TYPE_G12A:
		*desc = ddr_port_desc_g12a;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12a);
		break;

	case DMC_TYPE_G12B:
		*desc = ddr_port_desc_g12b;
		desc_size = ARRAY_SIZE(ddr_port_desc_g12b);
		break;

	case DMC_TYPE_SM1:
		*desc = ddr_port_desc_sm1;
		desc_size = ARRAY_SIZE(ddr_port_desc_sm1);
		break;

	case DMC_TYPE_TM2:
		*desc = ddr_port_desc_tm2;
		desc_size = ARRAY_SIZE(ddr_port_desc_tm2);
		break;

	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		*desc = ddr_port_desc_t5;
		desc_size = ARRAY_SIZE(ddr_port_desc_t5);
		break;

	case DMC_TYPE_T7:
		*desc = ddr_port_desc_t7;
		desc_size = ARRAY_SIZE(ddr_port_desc_t7);
		break;

	case DMC_TYPE_T3:
		*desc = ddr_port_desc_t3;
		desc_size = ARRAY_SIZE(ddr_port_desc_t3);
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	case DMC_TYPE_S4:
		*desc = ddr_port_desc_s4;
		desc_size = ARRAY_SIZE(ddr_port_desc_s4);
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_SC2:
		*desc = ddr_port_desc_sc2;
		desc_size = ARRAY_SIZE(ddr_port_desc_sc2);
		break;

	case DMC_TYPE_P1:
		*desc = ddr_port_desc_p1;
		desc_size = ARRAY_SIZE(ddr_port_desc_p1);
		break;

	case DMC_TYPE_T5W:
		*desc = ddr_port_desc_t5w;
		desc_size = ARRAY_SIZE(ddr_port_desc_t5w);
		break;

	case DMC_TYPE_C3:
		if (type) {
			*desc = ddr_port_desc_c3_mon;
			desc_size = ARRAY_SIZE(ddr_port_desc_c3_mon);
		} else {
			*desc = ddr_port_desc_c3_prot;
			desc_size = ARRAY_SIZE(ddr_port_desc_c3_prot);
		}
		break;

	case DMC_TYPE_T5M:
		*desc = ddr_port_desc_t5m;
		desc_size = ARRAY_SIZE(ddr_port_desc_t5m);
		break;

	case DMC_TYPE_S5:
		*desc = ddr_port_desc_s5;
		desc_size = ARRAY_SIZE(ddr_port_desc_s5);
		break;

	case DMC_TYPE_S6:
		*desc = ddr_port_desc_s6;
		desc_size = ARRAY_SIZE(ddr_port_desc_s6);
		break;

	case DMC_TYPE_T3X:
		*desc = ddr_port_desc_t3x;
		desc_size = ARRAY_SIZE(ddr_port_desc_t3x);
		break;

	case DMC_TYPE_TXHD2:
		*desc = ddr_port_desc_txhd2;
		desc_size = ARRAY_SIZE(ddr_port_desc_txhd2);
		break;

	case DMC_TYPE_A4:
		if (type) {
			*desc = ddr_port_desc_a4_mon;
			desc_size = ARRAY_SIZE(ddr_port_desc_a4_mon);
		} else {
			*desc = ddr_port_desc_a4_prot;
			desc_size = ARRAY_SIZE(ddr_port_desc_a4_prot);
		}
		break;

	case DMC_TYPE_A5:
		*desc = ddr_port_desc_a5;
		desc_size = ARRAY_SIZE(ddr_port_desc_a5);
		break;

	case DMC_TYPE_S7:
		*desc = ddr_port_desc_s7;
		desc_size = ARRAY_SIZE(ddr_port_desc_s7);
		break;
	case DMC_TYPE_S7D:
		*desc = ddr_port_desc_s7d;
		desc_size = ARRAY_SIZE(ddr_port_desc_s7d);
		break;
	case DMC_TYPE_T6D:
		*desc = ddr_port_desc_t6d;
		desc_size = ARRAY_SIZE(ddr_port_desc_t6d);
		break;
	case DMC_TYPE_T6W:
		*desc = ddr_port_desc_t6w;
		desc_size = ARRAY_SIZE(ddr_port_desc_t6w);
		break;
	case DMC_TYPE_T6X:
		*desc = ddr_port_desc_t6x;
		desc_size = ARRAY_SIZE(ddr_port_desc_t6x);
		break;
	case DMC_TYPE_C4:
		*desc = ddr_port_desc_c4;
		desc_size = ARRAY_SIZE(ddr_port_desc_c4);
		break;
	case DMC_TYPE_A9:
		if (type) {
			*desc = ddr_port_desc_a9_mon;
			desc_size = ARRAY_SIZE(ddr_port_desc_a9_mon);
		} else {
			*desc = ddr_port_desc_a9_prot;
			desc_size = ARRAY_SIZE(ddr_port_desc_a9_prot);
		}
		break;
#endif
	case DMC_TYPE_S1A:
		*desc = ddr_port_desc_s1a;
		desc_size = ARRAY_SIZE(ddr_port_desc_s1a);
		break;
	default:
		return -EINVAL;
	}

	if (chip_ddr_port && *desc == ddr_port_src && desc_size == chip_ddr_port_num) {
		*desc = chip_ddr_port;
		return chip_ddr_port_num;
	}

	/* using once */
	chip_ddr_port = kcalloc(desc_size, sizeof(*chip_ddr_port), GFP_KERNEL);
	if (!chip_ddr_port)
		return -ENOMEM;
	memcpy(chip_ddr_port, *desc, sizeof(struct ddr_port_desc) * desc_size);
	chip_ddr_port_num = desc_size;
	ddr_port_src = *desc;
	*desc = chip_ddr_port;

	return desc_size;
}

unsigned long dmc_rw(unsigned long addr, unsigned long value, int rw)
{
	struct arm_smccc_res smccc;

	arm_smccc_smc(DMC_MON_RW, addr, value, rw, 0, 0, 0, 0, &smccc);
	return smccc.a0;
}
