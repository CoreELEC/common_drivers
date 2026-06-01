/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_FRC_REV_H__
#define __DPSS_FRC_REV_H__

// dpss_frc sync only frc
// dpss_frc add frc io ctrl
// dpss_frc: frc update mc_cut_en
// dpss_frc: modify register setting
// dpss: optimising stack memory size
// dpss: frc automatically configures n2m
// dpss: frc modify dae0 [1/1]
// dpss: sync dpss code to openlinux 0620 [1/1]
// dpss: dpss modify AFRC config [1/1]
// dpss: VD1 switching to MC displays abnormally. [1/1]
// dpss: modify dpss inp module [1/1]
// dpss: mc reset rdmif config [1/1]
// dpss: frc updates the n2m info to fw [1/2]
// dpss: modify mc link state when resolution changed [1/1]
// vpu: switch to vd1 link when video off [2/2]
// dpss: frc don't set fmt in dpss initial state [1/2]
// dpss: frc use rdma to update vfcd [1/1]
// dpss: dpss retains the mif address [1/1]
// dpss: frc bringup t6x [1/1]
// dpss: sync code 0731 [DO NOT MERGE TRUNK] [1/1]
// dpss: pre vsync use rdma [1/1]
// dpss: modify dae_wrpt_full_num [1/1]
// dpss: chg pre_vsync_offset
// dpss: enable frc only case
// dpss:20250910 chg n2m config setting
// dpss:20251028 set io_ctrl enable by self
// dpss:20251125 add frc bypass when dpss reset
// dpss:20260106 pre vsync rdma
// dpss:20260208 add game memc
// dpss:20260227 refine frc sw execution flow
// dpss:20260407 add hdr flag
// dpss:20260407 frc set fast pat mode"
// dpss:20260415 add dpss_flag_info
// dpss:20260424 add rdma reset when start
// dpss:20260424 frc set rev mode
// dpss:20260508 frc not support light chg in small win
// dpss:20260520 remove VFCD address usage from FRC
// dpss:20260521 light chg protect

#define DPSS_FRC_FW_VER			"20260610 add trick play"
#define DPSS_FRC_KERDRV_VER                  1000

//#define DPSS_FRC_DEVNO		1
//#define DPSS_FRC_NAME			"dpss_frc"
//#define DPSS_FRC_CLASS_NAME	"dpss_frc"

#endif
