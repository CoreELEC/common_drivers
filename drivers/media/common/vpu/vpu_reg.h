/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_REG_H__
#define __VPU_REG_H__
#include <linux/platform_device.h>

#define VPU_REG_END                  0xffff

/* ********************************
 * register define
 * *********************************
 */

#define AO_RTI_GEN_PWR_SLEEP0        0x03a
#define AO_RTI_GEN_PWR_ISO0          0x03b

/* HIU bus */

#define HHI_MEM_PD_REG0              0x40
#define HHI_VPU_MEM_PD_REG0          0x41
#define HHI_VPU_MEM_PD_REG1          0x42
#define HHI_VPU_MEM_PD_REG2          0x4d
#define HHI_VPU_MEM_PD_REG3          0x4e
#define HHI_VPU_MEM_PD_REG4          0x4c
#define HHI_VPU_MEM_PD_REG3_SM1      0x43
#define HHI_VPU_MEM_PD_REG4_SM1      0x44

/* pwrctrl reg */
#define PWRCTRL_PWR_ACK0             0x0000
#define PWRCTRL_PWR_ACK1             0x0001
#define PWRCTRL_PWR_OFF0             0x0004
#define PWRCTRL_PWR_OFF1             0x0005
#define PWRCTRL_ISO_EN0              0x0008
#define PWRCTRL_ISO_EN1              0x0009
#define PWRCTRL_FOCRST0              0x000c
#define PWRCTRL_FOCRST1              0x000d
#define PWRCTRL_MEM_PD5_SC2          0x0015
#define PWRCTRL_MEM_PD6_SC2          0x0016
#define PWRCTRL_MEM_PD7_SC2          0x0017
#define PWRCTRL_MEM_PD8_SC2          0x0018
#define PWRCTRL_MEM_PD9_SC2          0x0019

#define PWRCTRL_MEM_PD3_T5           0x00a
#define PWRCTRL_MEM_PD4_T5           0x00b
#define PWRCTRL_MEM_PD5_T5           0x00c
#define PWRCTRL_MEM_PD6_T5           0x00d
#define PWRCTRL_MEM_PD7_T5           0x00e

#define PWRCTRL_MEM_PD6_A4            0x0016
#define CLKCTRL_VOUTENC_CLK_CTRL_A4  0x0081

#define HHI_VPU_CLK_CNTL             0x6f
#define HHI_VAPBCLK_CNTL             0x7d
#define HHI_VPU_CLK_CTRL             0x6f

#define CLKCTRL_VPU_CLK_CTRL         0x003a
#define CLKCTRL_VAPBCLK_CTRL         0x003f

#define CLKCTRL_VPU_CLK_CTRL_A9      0x0118
#define CLKCTRL_VAPBCLK_CTRL_A9      0x011a

#define CLKCTRL_VOUTENC_CLK_CTRL     0x0046
#define CLKCTRL_VOUTENC_CLK_CTRL_C5  0x0118
#define CLKCTRL_VAPBCLK_CTRL_C5      0x011a

/* cbus */
#define RESET0_LEVEL                 0x0420
#define RESET1_LEVEL                 0x0421
#define RESET2_LEVEL                 0x0422
#define RESET3_LEVEL                 0x0423
#define RESET4_LEVEL                 0x0424
#define RESET5_LEVEL                 0x0425
#define RESET6_LEVEL                 0x0426
#define RESET7_LEVEL                 0x0427

/* vpu clk gate */
/* vcbus */
#define VPU_CLK_GATE                 0x2723

#define VPP_RDARB_MODE               0x3978
#define VPP_RDARB_REQEN_SLV          0x3979
#define VPU_RDARB_MODE_L1C1          0x2790
#define DI_RDARB_MODE_L1C1           0x2050
#define DI_RDARB_REQEN_SLV_L1C1      0x2051
#define DI_WRARB_MODE_L1C1           0x2054
#define DI_WRARB_REQEN_SLV_L1C1      0x2055
#define DI_RDARB_UGT_L1C1            0x205b
#define DI_WRARB_UGT_L1C1            0x205d
#define VPU_RDARB_MODE_L1C2          0x2799
#define VPU_RDARB_MODE_L2C1          0x279d
#define VPP_RDARB_REQEN_SLV_L2C1     0x279e
#define VPU_WRARB_MODE_L2C1          0x27a2
#define VPU_WRARB_REQEN_SLV_L2C1     0x27a3
#define VPU_RDARB_UGT_L2C1           0x27c2
#define VPU_WRARB_UGT_L2C1           0x27c3

#define VENC_VDAC_TST_VAL            0x1b7f
#define VPP_DUMMY_DATA               0x1d00
#define VPU_VPU_PWM_V0               0x1ce0

#define VPU_VOUT_BLEND_DUMDATA       0x0011
#define VPP_VD1_MATRIX_OFFSET0_1     0x0289
#define VPU_VOUT_DTH_DATA            0x0103

#define VPU_VOUT_BLEND_DUMDATA_C5    0x0041
#define VOUT_OSD1_CSC_OFFSET0_1_C5   0x0189
#define VPU_VOUT_DTH_DATA_C5         0x0403

#define ENCP_DVI_HSO_BEGIN           0x1c30
#define S5_VPU_VPU_PWM_V0            0x2730

#define VPU_ARB_URG_CTRL            0x2747
#define VPU_ARB_UNBINDABLE_REG       0xffff
//bit29	surper urgent enable
//bit28	sideband enable
//bit15	arb rd3  sideband ctrl by vpp2 line fifo
//bit14	arb rd3  sideband ctrl by vpp1 line fifo
//bit13	arb rd3  sideband ctrl by rdma
//bit12	arb rd3  sideband ctrl by vpp0 line fifo
//bit9	arb wr0  sideband ctrl by vdin1 line fifo
//bit8	arb wr0  sideband ctrl by vdin0 line fifo
//bit7	arb rd2  sideband ctrl by vpp2 line fifo
//bit6	arb rd2  sideband ctrl by vpp1 line fifo
//bit5	arb rd2  sideband ctrl by rdma
//bit4	arb rd2  sideband ctrl by vpp0 line fifo
//bit3	arb rd0  sideband ctrl by vpp2 line fifo
//bit2	arb rd0  sideband ctrl by vpp1 line fifo
//bit1	arb rd0  sideband ctrl by rdma
//bit0	arb rd0  sideband ctrl by vpp0 line fifo

#define VPP_OFIFO_URG_CTRL          0x1dd8
//for a9 vpp1
#define VPP1_URGENT_CTRL            0x648e

//VPU_ARB_URG_CTRL
#define VPU_ARB_URG_CTRL1           0x2735
#define VPU_ARB_URG_CTRL1_S5        0x2741
#define VPU_ARB_URG_CTRL1_T3X       0x2735
//bit29	super urgent enable
//bit28	sideband enable
//bit13	arb wr0/arb wr2 sideband cntl by vdin1 line fifo
//bit12	arb wr0/arb wr2 sideband cntl by vdin0 line fifo
//bit11	arb rd3  sideband ctrl by vpp2 line fifo
//bit10	arb rd3  sideband ctrl by vpp1 line fifo
//bit9	arb rd3  sideband ctrl by rdma
//bit8	arb rd3  sideband ctrl by vpp0 line fifo
//bit7	arb rd2  sideband ctrl by vpp2 line fifo
//bit6	arb rd2  sideband ctrl by vpp1 line fifo
//bit5	arb rd2  sideband ctrl by rdma
//bit4	arb rd2  sideband ctrl by vpp0 line fifo
//bit3	arb rd0  sideband ctrl by vpp2 line fifo
//bit2	arb rd0  sideband ctrl by vpp1 line fifo
//bit1	arb rd0  sideband ctrl by rdma
//bit0	arb rd0  sideband ctrl by vpp0 line fifo
//simple describe:
//bit0: arb0 rd enable
//bit4: arb2 rd enable
//bit12:arb wr0 enable
//bit28:sideband enable

#define VPU_ARB_URG_CTRL_T3X           0x2747
//not used
#define VPP_OFIFO_URG_CTRL_T3X         0x2505
//bit15:  urgent_ctrl_en
//bit12:  urg_init_value
//bit6-11: unurgent_value
//bit0-5: urgent_vlaue
#define VPP_SLICE1_OFIFO_URG_CTRL_T3X   0x2605
//bit15:  urgent_ctrl_en
//bit12:  urg_init_value
//bit6-11: unurgent_value
//bit0-5: urgent_vlaue

//t3x dmc regs:
   //bit 8.   enable dmc request of axibus chan 7.	NNA Async interface.
   //bit 7.   enable dmc request of axibus chan 7.	VGE Async interface.
   //bit 6.   enable dmc request of axibus chan 6.	GPU async interface.
   //bit 5.   enable dmc request of axibus chan 5.	reserved for dmc_test.
   //bit 4.   enable dmc request of axibus chan 4.	system Async interface.
   //bit 3.   enable dmc request of axibus chan 3.	DOS Async interface.
   //bit 2.   enable dmc request of axibus chan 2.	VPU Async interface.
   //bit 1.   enable dmc request of axibus chan 1.	FRC Async interface.
   //bit 0.   enable dmc request of axibus chan 0.	CPU/A55   async interface.

#define DMC_AXI2_CHAN_CTRL1                        0x008a
//t3x
   //bit 31:29. not used.
   //bit 28:20. when write/read side band signal used to block other request.
   //configure which master we can block. each bit for  one master.
    //Note. don't block vpu itself and  CPU or other urgent request.
   //bit 19. use side band write urgent control signal to control AWQOS.
   //1: enabe. 0: disable.
   //bit 18. use side band read  urgent control signal to control ARQOS.
   //1: enabe. 0: disable.
   //bit 17. use side band write urgent control signal to block other master request.
   //1: enable. 0 disable.
   //bit 16. use side band read urgent control signal to block other master request.
   //1: enable. 0 disable.
   //bit 15:12.  the AWQOS value when side band write urgent
   //control signal = 1 while bit 19 enabled.
   //bit 11:8.   the ARQOS value when side band read  urgent
   //control signal = 1 while bit 18 enabled.
   //bit 7:0.    Not used.

//t7c
//t7c dmc regs:
   //bit 8.   enable dmc request of axibus chan 7.	reserved.
   //bit 7.   enable dmc request of axibus chan 7.	device2/isp/demux/dsp Async interface.
   //bit 6.   enable dmc request of axibus chan 6.	DOS/GE2D interface.
   //bit 5.   enable dmc request of axibus chan 5.	reserved for dmc_test.
   //bit 4.   enable dmc request of axibus chan 4.	device/usb/pcie/hdmi rx Async interface.
   //bit 3.   enable dmc request of axibus chan 3.	NNA Async interface.
   //bit 2.   enable dmc request of axibus chan 2.	VPU Async interface.
   //bit 1.   enable dmc request of axibus chan 1.	reserved.
   //bit 0.   enable dmc request of axibus chan 0.	CPU/A73/A53/GPU async interface.

//bit 31:20. not used.
//bit 19. use side band write urgent control signal to control AWQOS.  1: enabe. 0: disable.
//bit 18. use side band read urgent control signal to control ARQOS.  1: enabe. 0: disable.
//bit 17. use side band write urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 16. use side band read urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 15:12.  the AWQOS value when side band write urgent control signal = 1 while bit 19 enabled.
//bit 11:8.   the ARQOS value when side band read urgent control signal = 1 while bit 18 enabled.
//bit 7:0.	  when write/read side band signal used to block other request.
//configure which master we can block.each bit for  one master.
//Note. don't block VPU and CPU or other urgent request.

//T3
 //bit 7.	enable dmc request of axibus chan 7.  NIC4 include NNA debus,
 //dsp, aocpu, audio, dmc, etc.
 //bit 6.	enable dmc request of axibus chan 6.  NIC3 include ge2d, hevc,vdec, hcodec etc.
 //bit 5.	enable dmc request of axibus chan 5.  reserved for dmc_test.
 //bit 4.	enable dmc request of axibus chan 4.  NIC2 include PCIE, usb,
 //emmc eth spicc audma etc.
 //bit 3.	enable dmc request of axibus chan 3.  NNA
 //bit 2.	enable dmc request of axibus chan 2.  NIC5 VPU channel. include VPU, FRC.
 //bit 1.	enable dmc request of axibus chan 1.  Mali GPU.
 //bit 0.	enable dmc request of axibus chan 0.  CPU/A53	async interface.

  //bit 31:20. not used.
  //bit 23. use FRC side bank sleep enable.
  //bit 22. use FRC side band signal enable;
  //bit 21. use vpu side band sleep enable.
  //bit 20. use VPU side band signal enable;
  //bit 19. use side band write urgent control signal to control AWQOS.  1: enabe. 0: disable.
  //bit 18. use side band read urgent control signal to control ARQOS.	1: enabe. 0: disable.
  //bit 17. use side band write urgent control signal to block other master request.
  //1: enable. 0 disable.
  //bit 16. use side band read urgent control signal to block other master request.
  //1: enable. 0 disable.
  //bit 15:12.	the AWQOS value when side band write urgent control signal = 1 while bit 19 enabled.
  //bit 11:8.	the ARQOS value when side band read urgent control signal = 1 while bit 18 enabled.
  //bit 7:0.	when write/read side band signal used to block other request.
  //configure which master we can block. each bit for  one master.
  //Note. don't block VPU and CPU or other urgent request.

//t6w
//bit 11.  enable dmc request of axibus chan 11. AMFC Async interface.
//bit 10.  enable dmc request of axibus chan 10. System/periphs Async interface.
//bit 9.   enable dmc request of axibus chan 9.  ge2d Async interface.
//bit 8.   enable dmc request of axibus chan 8.  HEVC Async interface.
//bit 7.   enable dmc request of axibus chan 7.  VPU3 read Async interface.
//bit 6.   enable dmc request of axibus chan 6.  VPU2 read interface.
//bit 5.   enable dmc request of axibus chan 5.  reserved for dmc_test.
//bit 4.   enable dmc request of axibus chan 4.  VPU2 write Async interface.
//bit 3.   enable dmc request of axibus chan 3.  VPU1 Async interface.
//bit 2.   enable dmc request of axibus chan 2.  VPU0 Async interface.
//bit 1.   enable dmc request of axibus chan 1.  GPU Async interface.
//bit 0.   enable dmc request of axibus chan 0.  CPU Async interface

#define T6W_DMC_AXI2_CHAN_CTRL1                        0x008c
#define T6W_DMC_AXI3_CHAN_CTRL1                        0x0092
#define T6W_DMC_AXI4_CHAN_CTRL1                        0x0097
#define T6W_DMC_AXI6_CHAN_CTRL1                        0x00a2
#define T6W_DMC_AXI7_CHAN_CTRL1                        0x00a7
//bit 31     cfg_wrdata_mode
//bit 30:28. not used.
//bit 27:16. when write/read side band signal used to block other request.
//configure which master we can block. each bit for  one master.
//Note. don't block vpu itself and  CPU or other urgent request.
//bit 15:12. Not used.
//bit 11.   use side band write urgent control signal to control AWQOS.  1: enabe. 0: disable.
//bit 10.   use side band read  urgent control signal to control ARQOS.  1: enabe. 0: disable.
//bit 9.    use side band write urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 8.    use side band read urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 7:4.  the AWQOS value when side band write urgent control signal = 1 while bit 19 enabled.
//bit 3:0.  the ARQOS value when side band read  urgent control signal = 1 while bit 18 enabled.

//TXHD2
 //bit 23.	enable dmc request of ambus chan 7. GE2D interface. Async interface.
 //bit 22.	enable dmc request of ambus chan 6. Reserved.
 //bit 21.	enable dmc request of ambus chan 5. DOS VDEC  interface   aSync interface.
 //bit 20.	enable dmc request of ambus chan 4. VPU write interface 1  aSync interface.
 //bit 19.	enable dmc request of ambus chan 3. VPU write interface 0  aSync interface.
 //bit 18.	enable dmc request of ambus chan 2. VPU read interface 2.	aSync interface.
 //bit 17.	enable dmc request of ambus chan 1. VPU read interface 1.	aSync interface.
 //bit 16.	enable dmc request of ambus chan 0. VPU read interface 0.  aSync interface.
 //bit 9	enable dmc request of axibus chan 9.  reserved.
 //bit 8.	enable dmc request of axibus chan 8   usb2host	async interface.
 //bit 7.	enable dmc request of axibus chan 7.  DEVICE.	 Async interface.
 //bit 6.	enable dmc request of axibus chan 6.  USB2drd	Async interface.
 //bit 5.	enable dmc request of axibus chan 5.  reserved for dmc_test.
 //bit 4.	enable dmc request of axibus chan 4.  hevc front Async interface.
 //bit 3.	enable dmc request of axibus chan 3.  HDCP/HDMI   Async interface.
 //bit 2.	enable dmc request of axibus chan 2.  Mali1  async
 //bit 1.	enable dmc request of axibus chan 1.  Mali0 .  async interface.
 //bit 0.	enable dmc request of axibus chan 0.  CPU/A53	async interface.

//T5W
   //bit 23.  enable dmc request of ambus chan 7. Reserved for GE2D interface. Async interface.
   //bit 22.  enable dmc request of ambus chan 6. DOS HCODEC  interface   Sync interface.
   //bit 21.  enable dmc request of ambus chan 5. DOS VDEC	interface	Sync interface.
   //bit 20.  enable dmc request of ambus chan 4. VPU write interface 1  Sync interface.
   //bit 19.  enable dmc request of ambus chan 3. VPU write interface 0  Sync interface.
   //bit 18.  enable dmc request of ambus chan 2. VPU read interface 2.   Sync interface.
   //bit 17.  enable dmc request of ambus chan 1. VPU read interface 1.   Sync interface.
   //bit 16.  enable dmc request of ambus chan 0. VPU read interface 0.  Sync interface.
   //bit 9	  enable dmc request of axibus chan 9.	wave  async interface.
   //bit 8.   enable dmc request of axibus chan 8	hevc_b	async interface.
   //bit 7.   enable dmc request of axibus chan 7.	DEVICE.    Async interface.
   //bit 6.   enable dmc request of axibus chan 6.	USB   Async interface.
   //bit 5.   enable dmc request of axibus chan 5.	reserved for dmc_test.
   //bit 4.   enable dmc request of axibus chan 4.	hevc front Async interface.
   //bit 3.   enable dmc request of axibus chan 3.	HDCP/HDMI	Async interface.
   //bit 2.   enable dmc request of axibus chan 2.	pcie  async
   //bit 1.   enable dmc request of axibus chan 1.	Mali .	async interface.
   //bit 0.   enable dmc request of axibus chan 0.	CPU/A53   async interface.

//TXHD2(no vpu read2), T5W
#define DMC_AM0_CHAN_CTRL1                         0x0062
 //bit 31:    side band signal used as block other request.
 //bit 30 :   side band urgent  increase enable.
 //bit 29 :   side band urgent decrease urgent enable.
 //bit 23:16 : when bit 31 enabled, block the ambus related bits read request.
 //bit 15:0  : when bit 31 enabled, block the axi bus related bits read request.

#define DMC_AM0_CHAN_CTRL2                         0x0063
 //bit 31:24  not used.
 //bit 23:16 : when side band signal used as block other request, and side bank signal is high,
 //block the ambus related bits write request.
 //bit 15:0  : when side band signal used as block other request, and side bank signal is high,
 //block the axi bus related bits write request.

#define DMC_AM1_CHAN_CTRL1                         0x0066
 //bit 31:    side band signal used as block other request.
 //bit 30 :   side band urgent  increase enable.
 //bit 29 :   side band urgent decrease urgent enable.
 //bit 23:16 : when bit 31 enabled, block the ambus related bits read request.
 //bit 15:0  : when bit 31 enabled, block the axi bus related bits read request.

#define DMC_AM1_CHAN_CTRL2                         0x0067
 //bit 31:24  not used.
 //bit 23:16 : when side band signal used as block other request, and side bank signal is high,
 //block the ambus related bits write request.
 //bit 15:0  : when side band signal used as block other request, and side bank signal is high,
 //block the axi bus related bits write request.

#define DMC_AM2_CHAN_CTRL1                         0x006a
  //bit 31:    side band signal used as block other request.
  //bit 30 :   side band urgent  increase enable.
  //bit 29 :   side band urgent decrease urgent enable.
  //bit 23:16 : when bit 31 enabled, block the ambus related bits read request.
  //bit 15:0  : when bit 31 enabled, block the axi bus related bits read request.

#define DMC_AM2_CHAN_CTRL2                         0x006b
  //bit 31:24  not used.
  //bit 23:16 : when side band signal used as block other request,
  //and side bank signal is high, block the ambus related bits write request.
  //bit 15:0  : when side band signal used as block other request,
  //and side bank signal is high, block the axi bus related bits write request.

#define DMC_AM3_CHAN_CTRL1                         0x006e
 //bit 31:    side band signal used as block other request.
 //bit 30 :   side band urgent  increase enable.
 //bit 29 :   side band urgent decrease urgent enable.
 //bit 23:16 : when bit 31 enabled, block the ambus related bits read request.
 //bit 15:0  : when bit 31 enabled, block the axi bus related bits read request.

#define DMC_AM3_CHAN_CTRL2                         0x006f
 //bit 31:24  not used.
 //bit 23:16 : when side band signal used as block other request, and side bank signal is high,
 //block the ambus related bits write request.
 //bit 15:0  : when side band signal used as block other request, and side bank signal is high,
 //block the axi bus related bits write request.

#define DMC_AM4_CHAN_CTRL1                         0x0072
 //bit 31:    side band signal used as block other request.
 //bit 30 :   side band urgent  increase enable.
 //bit 29 :   side band urgent decrease urgent enable.
 //bit 23:16 : when bit 31 enabled, block the ambus related bits read request.
 //bit 15:0  : when bit 31 enabled, block the axi bus related bits read request.

#define DMC_AM4_CHAN_CTRL2                         0x0073
 //bit 31:24  not used.
 //bit 23:16 : when bit 31 enabled, block the ambus related bits write request.
 //bit 15:0  : when bit 31 enabled, block the axi bus related bits write request.

//S7
 //bit 13.	 enable dmc request of axibus chan 13.	GE2D Async interface.
 //bit 12.	 enable dmc request of axibus chan 12.	GPU Async interface.
 //bit 11.	 enable dmc request of axibus chan 11.	DEMUX async interface.
 //bit 10.	 enable dmc request of axibus chan 10.	system Async interface.
 //bit 9.	enable dmc request of axibus chan 9.  HCODEC Async interface.
 //bit 8.	enable dmc request of axibus chan 8.  VDEC Async interface.
 //bit 7.	enable dmc request of axibus chan 7.  HEVC Async interface.
 //bit 6.	enable dmc request of axibus chan 6.  VPU4 async interface.
 //bit 5.	enable dmc request of axibus chan 5.  reserved for dmc_test.
 //bit 4.	enable dmc request of axibus chan 4.  VPU3 Async interface.
 //bit 3.	enable dmc request of axibus chan 3.  VPU2 Async interface.
 //bit 2.	enable dmc request of axibus chan 2.  VPU1 Async interface.
 //bit 1.	enable dmc request of axibus chan 1.  VPU0 Async interface.
 //bit 0.	enable dmc request of axibus chan 0.  CPU/A55	async interface.
 //DMC CLK and RESET domain register. please check DMC_SEC_APB_CTRLx register for access details.

#define S7_DMC_AXI1_CHAN_CTRL1                        0x0086
//bit 30     cfg_wrdata_mode
//bit 30:29. not used.
//bit 29:16. when write/read side band signal used to block other request.
//configure which master we can block. each bit for  one master.
//Note. don't block vpu itself and  CPU or other urgent request.
//bit 15:12. Not used.
//bit 11.   use side band write urgent control signal to control AWQOS.  1: enabe. 0: disable.
//bit 10.   use side band read  urgent control signal to control ARQOS.  1: enabe. 0: disable.
//bit 9.    use side band write urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 8.    use side band read urgent control signal to block other master request.
//1: enable. 0 disable.
//bit 7:4.  the AWQOS value when side band write urgent control signal = 1 while bit 19 enabled.
//bit 3:0.  the ARQOS value when side band read  urgent control signal = 1 while bit 18 enabled.

#define S7_DMC_AXI2_CHAN_CTRL1                        0x008a
#define S7_DMC_AXI3_CHAN_CTRL1                        0x008e
#define S7_DMC_AXI4_CHAN_CTRL1                        0x0092
#define S7_DMC_AXI5_CHAN_CTRL1                        0x0096

#define VPU_ARB_URG_CTRL_TXHD2            0x2747
#define VPU_ARB_URG_CTRL_T5W              0x2747
//bit29    surper urgent enable
//bit28    sideband enable
//bit 11 arb rd2  sideband ctrl by vpp2 line fifo
//bit 10 arb rd0  sideband ctrl by vpp2 line fifo
//bit 9  arb wr1  sideband ctrl by  vdin1 line fifo
//bit 8  arb wr1  sideband ctrl by  vdin0 line fifo
//bit 7  arb wr0  sideband ctrl by  vdin1 line fifo
//bit 6  arb wr0  sideband ctrl by  vdin0 line fifo
//bit 5  arb rd2  sideband ctrl by rdma
//bit 4  arb rd2  sideband ctrl by vpp0 line fifo
//bit 3  arb rd1  sideband ctrl by rdma
//bit 2  arb rd1  sideband ctrl by vpp1 line fifo
//bit 1  arb rd0  sideband ctrl by rdma
//bit 0  arb rd0  sideband ctrl by vpp1 line fifo

#define VPU_ARB_URG_CTRL_T3                        0x2747
//bit29   surper urgent enable
//bit28   sideband urgent enable
//bit27:28  super_urgent_enable
//bit17:12  super_urgent_offset
//bit11   arb1_wr_vdin1_lfifo
//bit10   arb1_wr_vdin0_lfifo
//bit9     arb0_wr_vdin1_lfifo
//bit8     arb0_wr_vdin0_lfifo
//bit7     arb2_rd_vpp2_ofifo
//bit6     arb2_rd_vpp1_ofifo
//bit5     arb2_rd_rdma
//bit4     arb2_rd_vpp0_ofifo
//bit3     arb0_rd_vpp2_ofifo
//bit2     arb0_rd_vpp1_ofifo
//bit1     arb0_rd_rdma
//bit0     arb0_rd_vpp0_ofifo

#define VPU_ARB_URG_CTRL_S7                           0x2747
//bit29    surper urgent enable
//bit28    sideband enable
//bit 11 arb rd2  sideband ctrl by vpp2 line fifo
//bit 10 arb rd0  sideband ctrl by vpp2 line fifo
//bit 9  arb wr1  sideband ctrl by  vdin1 line fifo
//bit 8  arb wr1  sideband ctrl by  vdin0 line fifo
//bit 7  arb wr0  sideband ctrl by  vdin1 line fifo
//bit 6  arb wr0  sideband ctrl by  vdin0 line fifo
//bit 5  arb rd2  sideband ctrl by rdma
//bit 4  arb rd2  sideband ctrl by vpp0 line fifo
//bit 3  arb rd1  sideband ctrl by rdma
//bit 2  arb rd1  sideband ctrl by vpp1 line fifo
//bit 1  arb rd0  sideband ctrl by rdma
//bit 0  arb rd0  sideband ctrl by vpp1 line fifo

//same as s7
//#define VPU_ARB_URG_CTRL_S7D                           0x2747
//bit31:30	reserved
//bit29	reg_super_urgent_enable
//bit28	reg_sideband_enable
//bit27:26	reg_di_axi_wr_super_urgent_offset
//bit25:24	reg_axi0_wr_super_urgent_offset
//bit23:22	reg_axi2_rd_super_urgent_offset
//bit21:20	reg_di_axi_rd_super_urgent_offset
//bit19:18	reg_axi0_rd_super_urgent_offset
//bit17	reg_axi2_wr_super_urgent_enable
//bit16	reg_di_axi_wr_super_urgent_enable
//bit15	reg_axi0_wr_super_urgent_enable
//bit14	reg_axi2_rd_super_urgent_enable
//bit13	reg_di_axi_rd_super_urgent_enable
//bit12	reg_axi0_rd_super_urgent_enable
//bit11	reg_axi2_rd_vpp1_ofifo_urgent_enable
//bit10	reg_axi0_rd_vpp1_ofifo_urgent_enable
//bit9	reg_axi1_wr_vdin1_lfifo_urgent_enable
//bit8	reg_axi1_wr_vdin0_lfifo_urgent_enable
//bit7	reg_axi0_wr_vdin1_lfifo_urgent_enable
//bit6	reg_axi0_wr_vdin0_lfifo_urgent_enable
//bit5	reg_axi2_rd_rdma_urgent_enable
//bit4	reg_axi2_rd_vpp0_ofifo_urgent_enable
//bit3	reg_axi1_rd_rdma_urgent_enable
//bit2	reg_axi1_rd_vpp0_ofifo_urgent_enable
//bit1	reg_axi0_rd_rdma_urgent_enable
//bit0	reg_axi0_rd_vpp0_ofifo_urgent_enable

#define VPU_ARB_URG_CTRL_A9                           0x2747
//bit29    surper urgent enable
//bit28    sideband enable
//bit27:26 reg_di_sxi_wr_super_urgent_offset
//bit25:24 reg_axi0_wr_super_urgent_offset
//bit23:22 reg_axi2_rd_super_urgent_offset
//bit21:20 reg_di_axi_rd_super_urgent_offset
//bit 19:18 reg_axi0_rd_super_urgent_offset
//bit 17 reg_axi2_wr_super_urgent_enable
//bit 16 reg_di_axi_wr_super_urgent_enable
//bit 15 reg_axi0_wr_super_urgent_enable
//bit 14 reg_axi2_rd_super_urgent_enable
//bit 13 reg_di_axi_rd_super_urgent_enable
//bit 12 reg_axi0_rd_super_urgent_enable
//bit 11 reg_axi2_wr_vdin1_lfifo_urgent_enable
//bit 10 reg_axi2_wr_vdin0_lfifo_urgent_enable
//bit 9  reg_axi0_wr_vdin1_lfifo_urgent_enable
//bit 8  reg_axi0_wr_vdin0_lfifo_urgent_enable
//bit 7  reg_axi2_rd_vpp1_ofifo_urgent_enable
//bit 6  reg_axi2_rd_rdma_urgent_enable
//bit 5  arb rd2  sideband ctrl by rdma
//bit 4  reg_axi2_rd_vpp0_ofifo_urgent_enable
//bit 3  reg_axi0_rd_vpp2_ofifo_urgent_enable
//bit 2  reg_axi0_rd_vpp1_ofifo_urgent_enable
//bit 1  reg_axi0_rd_rdma_urgent_enable
//bit 0  reg_axi0_rd_vpp0_ofifo_urgent_enable

#define VPU_VOUT_BLEND_DUMDATA_A4    0x0041
#define VPP_VD1_MATRIX_OFFSET0_1_A4  0x0189
#define VPU_VOUT_DTH_DATA_A4         0x0403

int vpu_ioremap(struct platform_device *pdev, int *reg_map_table);

unsigned int vpu_clk_read(unsigned int _reg);
void vpu_clk_write(unsigned int _reg, unsigned int _value);
void vpu_clk_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len);
unsigned int vpu_clk_getb(unsigned int _reg,
			  unsigned int _start, unsigned int _len);
void vpu_clk_set_mask(unsigned int _reg, unsigned int _mask);
void vpu_clk_clr_mask(unsigned int _reg, unsigned int _mask);

unsigned int vpu_pwrctrl_read(unsigned int _reg);
unsigned int vpu_pwrctrl_getb(unsigned int _reg,
			      unsigned int _start, unsigned int _len);

unsigned int vpu_ao_read(unsigned int _reg);
void vpu_ao_write(unsigned int _reg, unsigned int _value);
void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len);
void vpu_dmc0_write(unsigned int _reg, unsigned int _value);
unsigned int vpu_dmc0_read(unsigned int _reg);
void vpu_dmc0_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len);
void vpu_dmc1_write(unsigned int _reg, unsigned int _value);
unsigned int vpu_dmc1_read(unsigned int _reg);
void vpu_dmc1_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len);
#endif
