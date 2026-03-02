/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __TVIN_VDIN_DRV_H
#define __TVIN_VDIN_DRV_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/irqreturn.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/workqueue.h>
#include <linux/amlogic/gki_module.h>

/* Amlogic Headers */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include "../common/rdma/rdma.h"
#endif

/* v4l2 header */
#include <media/v4l2-common.h>
#include <linux/videodev2.h>
#include <uapi/amlogic/v4l2-ext/videodev2-ext.h>
#include <uapi/amlogic/v4l2-ext/v4l2-controls-ext.h>
#include <uapi/amlogic/linuxtv-ext-ver.h>
#include <linux/v4l2-dv-timings.h>
#include <media/v4l2-device.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-dv-timings.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-event.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

/* Local Headers */
#include "../tvin_frontend.h"
#include "vdin_vf.h"
#include "vdin_regs.h"
//#include "vdin_v4l2_if.h"

/* 20220124: vf->flag add pc mode flag to bypass ai_pq ai_sr */
/* 20220214: The desktop screenshot probability gray screen */
/* 20220216: t5d dmc write 1 set to supper urgent */
/* 20220222: reduce hdmi signal confirm stable time */
/* 20220224: keystone stuck when str stress test */
/* 20220301: 119.88 get duration value is 800 not correct */
/* 20220308: screen capture 720 and then 1080 fail */
/* 20220310: vdin and vrr/dlg support */
/* 20220314: get vdin frontend info */
/* 20220328: return the same vf repeat causes crash */
/* 20220331: starting state chg not send event */
/* 20220401: 59.94 duration need set to 1601 */
/* 20220402: add vdin v4l2 feature */
/* 20220408: get format convert when started */
/* 20220415: transmit freesync data */
/* 20220416: dv game mode picture display abnormal */
/* 20220422: linux system pc mode myself restart dec */
/* 20220425: keystone function modify */
/* 20220513: add game mode lock debug */
/* 20220516: csc chg flash and dv chg not event */
/* 20220518: dv data abnormal cause reboot */
/* 20220519: dv register isr not rdma write */
/* 20220525: source-led switch to sink-led problem */
/* 20220602: 100 and 120 switch send event */
/* 20220608: t7 screenshot picture abnormal when width greater than vdin1_line_buff_size */
/* 20220609: add get low_latency and dv_vision */
/* 20220610: vdin add dump data debug */
/* 20220615: vdin1 crash addr when str */
/* 20220617: allm and vrr all come in not send vrr */
/* 20220618: use game mode global variable in vdin_isr cause abnormal and
 * fix video lag in old video path caused by unknown disp_mode flag
 */
/* 20220622: loopback implementation */
/* 20220628: send Freesync type */
/* 20220628: use one buffer in game mode 2 when vrr/freesync signal */
/* 20220701: use reality in and out fps check mode */
/* 20220706: probability not into vrr */
/* 20220714: set secure memory not send frame when keystone */
/* 20220719: up set not support port crash */
/* 20220726: add 444 format manual set */
/* 20220804: vdin1 crash addr modify */
/* 20220811: state machine optimization */
/* 20220829: get correct base framerate */
/* 20220830: ioctl node need add more protect */
/* 20220907: add manual_change_csc debug */
/* 20220915: dv not support manual set game mode */
/* 20220927: panel reverse not into game switch */
/* 20220930: 4k dv not afbce avoid afbce done */
/* 20221010: support buffer keeper */
/* 20221018: smr state machine optimization */
/* 20221021: s5 vdin bringup */
/* 20221028: rx calls interface notify information */
/* 20221029: update prop info */
/* 20221101: vdin1 crash when screen recording and keystone */
/* 20221103: change pc mode variable to local */
/* 20221109: interlace min buffer and 23.97 send event */
/* 20221118: add path not lock and filter frame rate */
/* 20221126: tvafe donot dynamically check aspect ratio */
/* 20221129: memory free and id name optimize */
/* 20221209: support aux screen capture */
/* 20221215: fix s5 loopback venc0 issue */
/* 20230105: overseas project display ration control */
/* 20230112: modify for vdin0 v4l2 */
/* 20230216: add address debug info */
/* 20230302: source clk change 25hz calculate to 24hz */
/* 20230307: if not data input not print reminder */
/* 20230309: interlace signal not advance send frame */
/* 20230316: add avi ext_colorimetry callback api for vdin */
/* 20230321: not add vdin interrupt lock on tvin_notify_vdin_skip_frame */
/* 20230330: set whether to check the hdr sei error option */
/* 20230404: interlace not into game mode */
/* 20230417: provide uapi headers for user space */
/* 20230420: add osd only support for screencap */
/* 20230427: fix screen cap stress test crash issue */
/* 20230504: keystone interlace update dest value */
/* 20230526: 2560x1440 set bit to 8 */
/* 20230527: add vdin secure reg config interface */
/* 20230527: update vdin dump buffer method on kernel 5.15 */
/* 20230527: fix screencap abnormal under display 4k120 on t3x */
/* 20230529: bringup for txhd2 */
/* 20230608: vdin not clear ratio_control value */
/* 20230720: set vdin urgent for txhd2 */
/* 20240730: Change the vdin address access range to greater than 4GB, starting from s6 */
/* 20240529: bringup for t6d */
/* 20250417: bringup for t6x */

#define VDIN_VER "2025/05/08 t6x bringup"

//#define T3X_PXP_BRINGUP
//#define VDIN_BRINGUP_NO_VF
//#define VDIN_BRINGUP_NO_VLOCK
//#define VDIN_BRINGUP_NO_AML_VECM
//#define VDIN_BRINGUP_BYPASS_COLOR_CN_VT

#define K_FORCE_HV_SHRINK	0
#define VDIN_V4L2_INPUT_MAX	8
#define VDIN_DROP_FRAME_NUM_DEF	2
#define VDIN_RESET_PCS_CNT     5

#define VDIN_MAX_INSTANCE 1
#define YUV422_2PLANE_EN	0x1 //422_2plane_enable
#define YUV422_2PLANE_MANUAL	0x10 //for 422_2plane_enable debug

enum vdin_work_mode_e {
	VDIN_WORK_MD_NORMAL = 0,
	VDIN_WORK_MD_V4L = 1,
};

/*the counter of vdin*/
#define VDIN_MAX_DEVS		3

enum vdin_hw_ver_e {
	VDIN_HW_ORG = 0,
	VDIN_HW_SM1,
	VDIN_HW_TL1,
	/*
	 * tm2 vdin0/vdin1 all support up to 4k
	 */
	VDIN_HW_TM2,
	VDIN_HW_TM2_B,
	/*
	 * sc2, sm1 vdin0 up to 4k, vdin1 up to 1080P (write)
	 * no afbce
	 */
	VDIN_HW_SC2,
	VDIN_HW_T5,
	VDIN_HW_T5D,
	/*
	 * t7 removed canvas
	 * no DV meta data slicer
	 * dv 422 to 444, and to 10 bit 422 (fix shrink issue)
	 */
	VDIN_HW_T7,
	/*
	 * 1.remove vdin0,	2.v scaling down input frame h max is 2k
	 * 3.buff 2k		4.post matrix (hdr matrix)
	 */
	VDIN_HW_S4,
	VDIN_HW_S4D,
	VDIN_HW_T3,
	VDIN_HW_T5W,
	VDIN_HW_S5,
	VDIN_HW_T5M,
	VDIN_HW_T3X,
	VDIN_HW_TXHD2,
	VDIN_HW_S7,
	VDIN_HW_S7D,
	VDIN_HW_S6,
	VDIN_HW_T6D,
	VDIN_HW_T6W,
	VDIN_HW_T6X,
};

/* 20230607: game mode optimize and add debug */
/* 20230609: add get vdin status */
/* 20230710: bc302 filter unstable vsync and add debug */
/* 20230713: bc302 get field type */
/* 20230718: optimize get video format process */
/* 20230725: notify fps change event when not game mode */
/* 20230727: drop the first frame for vdin1 was not write finished */
/* 20230803: pc and game mode switch optimization */
/* 20231013: dv 444 low-latency need convert to 422 */
/* 20231022: add auto pc game and optimize tvin_update_vdin_prop */
/* 20231109: add freesync type rate low and source-led */
/* 20231117: compare in and out frame rate has problem */
/* 20231129: remove cdev_del and optimize game debug */
#define VDIN_VER_V1 "20231129: remove cdev_del and optimize game debug"

enum vdin_irq_flg_e {
	VDIN_IRQ_FLG_NO_END = 1,
	VDIN_IRQ_FLG_IRQ_STOP = 2,
	VDIN_IRQ_FLG_FAKE_IRQ = 3,
	VDIN_IRQ_FLG_DROP_FRAME = 4,
	VDIN_IRQ_FLG_DV_CHK_SUM_ERR = 5,
	VDIN_IRQ_FLG_CYCLE_CHK,
	VDIN_IRQ_FLG_SIG_NOT_STABLE,
	VDIN_IRQ_FLG_FMT_TRANS_CHG,
	VDIN_IRQ_FLG_CSC_CHG,
	VDIN_IRQ_FLG_BUFF_SKIP, /* 10 */
	VDIN_IRQ_FLG_IGNORE_FRAME,
	VDIN_IRQ_FLG_SKIP_FRAME,
	VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR,
	VDIN_IRQ_FLG_NO_WR_FE,
	VDIN_IRQ_FLG_NO_NEXT_FE, /* 15 */
	VDIN_IRQ_FLG_SECURE_MD,
	VDIN_IRQ_FLG_VRR_CHG,
};

/*
 * t3x, vdin0/vdin1: normal core; vdin2: lite core
 * except for t3x, vdin0: normal core; vdin1: lite core
 */
enum vdin_hw_core_type_e {
	VDIN_HW_CORE_NORMAL = 0,
	VDIN_HW_CORE_LITE = 1,
	VDIN_HW_CORE_MAX = 2,
};

/* for config hw function support */
struct match_data_s {
	char *name;
	enum vdin_hw_ver_e hw_ver;
	bool de_tunnel_tunnel;
	/* tm2 verb :444 de-tunnel and wr mif 12 bit mode*/
	bool ipt444_to_422_12bit;
	bool vdin0_set_hdr;
	bool vdin1_set_hdr;
	u32 vdin0_en;
	u32 vdin1_en;
	u32 vdin2_en;
	u32 vdin0_line_buff_size;
	u32 vdin1_line_buff_size;
	u32 vdin2_line_buff_size;
	/* default is 4k */
	u32 vdin0_max_w_h;
};

/* #define VDIN_CRYSTAL               24000000 */
/* #define VDIN_MEAS_CLK              50000000 */
/* values of vdin_dev_t.flags */
#define VDIN_FLAG_NULL				0x00000000
#define VDIN_FLAG_DEC_INIT			0x00000001
#define VDIN_FLAG_DEC_STARTED		0x00000002
#define VDIN_FLAG_DEC_OPENED		0x00000004
#define VDIN_FLAG_DEC_REGISTERED	0x00000008
#define VDIN_FLAG_DEC_STOP_ISR		0x00000010
#define VDIN_FLAG_FORCE_UNSTABLE	0x00000020
#define VDIN_FLAG_FS_OPENED			0x00000100
#define VDIN_FLAG_SKIP_ISR			0x00000200
/*flag for vdin0 output*/
#define VDIN_FLAG_OUTPUT_TO_NR		0x00000400
/*flag for force vdin buffer recycle*/
#define VDIN_FLAG_FORCE_RECYCLE         0x00000800
/*flag for vdin scale down,color fmt conversion*/
#define VDIN_FLAG_MANUAL_CONVERSION     0x00001000
/*flag for vdin rdma enable*/
#define VDIN_FLAG_RDMA_ENABLE           0x00002000
/*flag for vdin snow on&off*/
#define VDIN_FLAG_SNOW_FLAG             0x00004000
/*flag for disable vdin sm*/
#define VDIN_FLAG_SM_DISABLE            0x00008000
/*flag for vdin suspend state*/
#define VDIN_FLAG_SUSPEND               0x00010000
/*flag for vdin-v4l2 debug*/
#define VDIN_FLAG_V4L2_DEBUG            0x00020000
/*flag for isr req&free*/
#define VDIN_FLAG_ISR_REQ               0x00040000
/* flag for rdma done in isr*/
#define VDIN_FLAG_RDMA_DONE             0x00080000
#define VDIN_FLAG_ISR_EN		0x00100000
/*flag for get vdin1 hist start interface*/
#define VDIN_FLAG_HIST_STARTED		0x00200000
#define VDIN_FLAG_DYN_RECONFIG		0x00400000
#define VDIN_FLAG_RDMA_READ_DONE	0x00800000
#define VDIN_FLAG_SUSPEND_STOP		0x01000000

/*values of vdin isr bypass check flag */
#define VDIN_BYPASS_STOP_CHECK          0x00000001
#define VDIN_BYPASS_CYC_CHECK           0x00000002
#define VDIN_BYPASS_VGA_CHECK           0x00000008

/*values of vdin game mode process flag */
/*enable*/
#define VDIN_GAME_MODE_0                (BIT0)
/*delay 1 frame, vdin have one frame buffer delay*/
#define VDIN_GAME_MODE_1                (BIT1)
/*delay 2 frame write, read at same buffer*/
#define VDIN_GAME_MODE_2                (BIT2)
/*when phase lock, will switch 2 to 1*/
#define VDIN_GAME_MODE_SWITCH_EN        (BIT3)
/* game 1 or game 2 */
#define VDIN_GAME_MODE_1_2        (VDIN_GAME_MODE_1 | VDIN_GAME_MODE_2)

/*flag for flush vdin buff*/
#define VDIN_FLAG_BLACK_SCREEN_ON	1
#define VDIN_FLAG_BLACK_SCREEN_OFF	0
/* size for rdma table */
#define RDMA_TABLE_SIZE			(PAGE_SIZE >> 3)
/* #define VDIN_DEBUG */
/* vdin_function_sel control bits start */
#define VDIN_SELF_STOP_START		BIT(0)
#define VDIN_V4L2_IOCTL_CHK		BIT(1)
#define VDIN_ADJUST_VLOCK		BIT(4)
#define VDIN_GAME_NOT_TANSFER		BIT(6) //control for tx output when game mode
#define VDIN_FORCE_444_NOT_CONVERT      BIT(7) //vdin 444 to 444 by default
#define VDIN_ONLY_SEND_WSS_VALUE	BIT(8) //vdin send aspect ratio value
#define VDIN_SET_DISPLAY_RATIO		BIT(9)
#define VDIN_NOT_DATA_INPUT_DROP	BIT(10)
#define VDIN_BYPASS_HDR_SEI_CHECK	BIT(11) //bypass Non-standard hdr stream detection
#define VDIN_INTERLACE_GAME_MODE	BIT(12)
#define VDIN_SACALE_4096_2_3840		BIT(13)
#define VDIN_CUTWIN_4096_2_3840		BIT(14)
#define VDIN_MUX_VDIN0_HIST		BIT(15) //sel vdin0 hist for txhd2
#define VDIN_SET_PCS_RESET		BIT(16) //for report_active abnormal callback rx pcs_reset
#define VDIN_AFBCE_DOLBY		BIT(17)
#define VDIN_INTERLACE_DROP_BOTTOM	BIT(18)
#define VDIN_HV_REVERSE_EN		BIT(19) //for hv_reverse is disabled by default
#define VDIN_AUTO_GAME_MODE		BIT(20)
#define VDIN_AUTO_PC_MODE		BIT(21)
#define VDIN_SET_DISPLAY_RATIO_TRANS	BIT(22) //vdin transparent transmission aspect ratio
#define VDIN_MEM_MEMSET_EN		BIT(23)
#define VDIN_CROP_SEL			BIT(24) //0-vdin cut win;1-vdin set src_crop in vf
#define VDIN_AFD_DISABLE		BIT(25) //0-enable AFD;1-disable AFD

/* vdin_function_sel control bits end */

#define VDIN_2K_SIZE			0x07800438 /* 0x780 = 1920 0x438 = 1080 */
#define VDIN_4K_SIZE			0x10000870 /* 0x1000 = 4096 0x870 = 2160 */

#define DURATION_VALUE_120_FPS		800
#define DURATION_VALUE_100_FPS		960
#define DURATION_VALUE_23_97_FPS	4004
// 23.97 Set this parameter to 23 to distinguish between 24
#define FPS_23_97_SET_TO_23		23
#define VDIN_SET_MODE_MAX_FRAME		60
#define VDIN_VRR_MIN_FRAME_RATE		25
#define VDIN_RDMA_UNDONE_MAX_CNT	10

/* Stay in TVIN_SIG_STATE_NOSIG for some
 * cycles => be sure TVIN_SIG_STATE_NOSIG
 */
#define NOSIG_MAX_CNT 8
		/* Stay in TVIN_SIG_STATE_UNSTABLE for some
		 * cycles => be sure TVIN_SIG_STATE_UNSTABLE
		 */
#define UNSTABLE_MAX_CNT 2/* 4 */
		/* Have signal for some cycles	=> exit TVIN_SIG_STATE_NOSIG */
#define EXIT_NOSIG_MAX_CNT 2/* 1 */
		/* No signal for some cycles  => back to TVAFE_STATE_NOSIG */
#define BACK_NOSIG_MAX_CNT 24 /* 8 */
		/* Signal unstable for some cycles => exit TVAFE_STATE_STABLE */
#define EXIT_STABLE_MAX_CNT 1
		/* Signal stable for some cycles  => back to TVAFE_STATE_STABLE */
		/* must >=500ms,for new api function */
#define BACK_STABLE_MAX_CNT 3
#define EXIT_PRESTABLE_MAX_CNT 50

#define IS_HDMI_SRC(src)	\
		({typeof(src) src_ = src; \
		 (((src_) >= TVIN_PORT_HDMI0) && \
		 ((src_) < TVIN_PORT_HDMI_MAX)); })

#define IS_CVBS_SRC(src)	\
		({typeof(src) src_ = src; \
		 (((src_) >= TVIN_PORT_CVBS0) && \
		 ((src_) <= TVIN_PORT_CVBS3)); })

#define H_SHRINK_TIMES_4k	4
#define V_SHRINK_TIMES_4k	4
#define H_SHRINK_TIMES_1080	2
#define V_SHRINK_TIMES_1080	2

/*vdin write mem color-depth support*/
enum vdin_wr_color_depth {
	VDIN_WR_COLOR_DEPTH_8BIT = 0x01,
	VDIN_WR_COLOR_DEPTH_9BIT = 0x02,
	VDIN_WR_COLOR_DEPTH_10BIT = 0x04,
	VDIN_WR_COLOR_DEPTH_12BIT = 0x08,
	/*TXL new add*/
	VDIN_WR_COLOR_DEPTH_10BIT_FULL_PACK_MODE = 0x10,
	VDIN_WR_COLOR_DEPTH_FORCE_MEM_YUV422_TO_YUV444 = 0x20,
};

/* also works for 420 */
#define VDIN_422_FULL_PK_EN			1
#define VDIN_422_FULL_PK_DIS			0

/* vdin afbce flag */
#define VDIN_AFBCE_EN                   (BIT0)
#define VDIN_AFBCE_EN_LOSSY             (BIT1)
#define VDIN_AFBCE_EN_422_12BIT         (BIT2)
#define VDIN_AFBCE_EN_4K                (BIT4)
#define VDIN_AFBCE_EN_1080P             (BIT5)
#define VDIN_AFBCE_EN_720P              (BIT6)
#define VDIN_AFBCE_EN_SMALL             (BIT7)

enum color_deeps_cfg_e {
	COLOR_DEEPS_AUTO = 0,
	COLOR_DEEPS_8BIT = 8,
	COLOR_DEEPS_10BIT = 10,
	COLOR_DEEPS_12BIT = 12,
	COLOR_DEEPS_MANUAL = 0x100,
};

enum vdin_color_deeps_e {
	VDIN_COLOR_DEEPS_8BIT = 8,
	VDIN_COLOR_DEEPS_9BIT = 9,
	VDIN_COLOR_DEEPS_10BIT = 10,
	VDIN_COLOR_DEEPS_12BIT = 12,
};

enum vdin_vf_put_md {
	VDIN_VF_PUT,
	VDIN_VF_RECYCLE,
};

#define VDIN_ISR_MONITOR_FLAG		BIT(0)
#define VDIN_ISR_MONITOR_EMP		BIT(1)
#define VDIN_ISR_MONITOR_RATIO		BIT(2)
#define VDIN_ISR_MONITOR_GAME		BIT(4)
#define VDIN_ISR_MONITOR_VS		BIT(5)
#define VDIN_ISR_MONITOR_VF		BIT(6)
#define VDIN_ISR_MONITOR_HDR_RAW_DATA	BIT(7)
#define VDIN_ISR_MONITOR_HDR_EMP_DATA	BIT(8)
#define VDIN_ISR_MONITOR_VRR_DATA	BIT(9)
#define VDIN_ISR_MONITOR_AFBCE		BIT(10)
#define VDIN_ISR_MONITOR_BUFFER		BIT(11)
#define VDIN_ISR_MONITOR_AFBCE_STA	BIT(12)
#define VDIN_ISR_MONITOR_WRITE_DONE	BIT(13)
#define VDIN_ISR_MONITOR_HDR_SEI_DATA	BIT(14)
#define VDIN_ISR_MONITOR_SCATTER_MEM	BIT(15)
#define VDIN_ISR_MONITOR_BLACK_BAR	BIT(16)
#define VDIN_ISR_MONITOR_HIST_INFO	BIT(17)
#define VDIN_ISR_MONITOR_CYCLE		BIT(19)

#define DBG_RX_UPDATE_VDIN_PROP		BIT(20)
#define VDIN_ISR_MONITOR_INPUT		BIT(21)
#define VDIN_ISR_MONITOR_RDMA		BIT(22)

#define VDIN_ISR_MONITOR_CFMT		BIT(23)
#define VDIN_ISR_MONITOR_SECURE		BIT(24)
#define VDIN_ISR_MONITOR_CROP		BIT(25)
#define VDIN_ISR_MONITOR_HDR_SBTM_DATA	BIT(26)

#define CURRENT_FRAME_GET_PROP	BIT(0)

//pattern 4 used for protect hdcp content,others for debug
#define VDIN_SECURE_PATTERN	4

/* *********************************************************************** */
/* *** enum definitions ********************************************* */
/* *********************************************************************** */
/*
 *YUV601:  SDTV BT.601            YCbCr (16~235, 16~240, 16~240)
 *YUV601F: SDTV BT.601 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *YUV709:  HDTV BT.709            YCbCr (16~235, 16~240, 16~240)
 *YUV709F: HDTV BT.709 Full_Range YCbCr ( 0~255,  0~255,  0~255)
 *RGBS:                       StudioRGB (16~235, 16~235, 16~235)
 *RGB:                              RGB ( 0~255,  0~255,  0~255)
 */
enum vdin_matrix_csc_e {
	VDIN_MATRIX_NULL = 0,
	VDIN_MATRIX_XXX_YUV601_BLACK,/*1*/
	VDIN_MATRIX_RGB_YUV601,
	VDIN_MATRIX_GBR_YUV601,
	VDIN_MATRIX_BRG_YUV601,
	VDIN_MATRIX_YUV601_RGB,/*5*/
	VDIN_MATRIX_YUV601_GBR,
	VDIN_MATRIX_YUV601_BRG,
	VDIN_MATRIX_RGB_YUV601F,
	VDIN_MATRIX_YUV601F_RGB,/*9*/
	VDIN_MATRIX_RGBS_YUV601,/*10*/
	VDIN_MATRIX_YUV601_RGBS,
	VDIN_MATRIX_RGBS_YUV601F,
	VDIN_MATRIX_YUV601F_RGBS,
	VDIN_MATRIX_YUV601F_YUV601,
	VDIN_MATRIX_YUV601_YUV601F,/*15*/
	VDIN_MATRIX_RGB_YUV709,
	VDIN_MATRIX_YUV709_RGB,
	VDIN_MATRIX_YUV709_GBR,
	VDIN_MATRIX_YUV709_BRG,
	VDIN_MATRIX_RGB_YUV709F,/*20*/
	VDIN_MATRIX_YUV709F_RGB,
	VDIN_MATRIX_RGBS_YUV709,
	VDIN_MATRIX_YUV709_RGBS,
	VDIN_MATRIX_RGBS_YUV709F,
	VDIN_MATRIX_YUV709F_RGBS,/*25*/
	VDIN_MATRIX_YUV709F_YUV709,
	VDIN_MATRIX_YUV709_YUV709F,
	VDIN_MATRIX_YUV601_YUV709,
	VDIN_MATRIX_YUV709_YUV601,
	VDIN_MATRIX_YUV601_YUV709F,/*30*/
	VDIN_MATRIX_YUV709F_YUV601,
	VDIN_MATRIX_YUV601F_YUV709,
	VDIN_MATRIX_YUV709_YUV601F,
	VDIN_MATRIX_YUV601F_YUV709F,
	VDIN_MATRIX_YUV709F_YUV601F,/*35*/
	VDIN_MATRIX_RGBS_RGB,
	VDIN_MATRIX_RGB_RGBS,
	VDIN_MATRIX_RGB2020_YUV2020,
	VDIN_MATRIX_YUV2020F_YUV2020,
	VDIN_MATRIX_MAX,
};

enum vdin_matrix_sel_e {
	VDIN_SEL_MATRIX0 = 0,
	VDIN_SEL_MATRIX1,
	VDIN_SEL_MATRIX_HDR,/*after, equal g12a have*/
};

static inline const char
*vdin_fmt_convert_str(enum vdin_format_convert_e fmt_cvt)
{
	switch (fmt_cvt) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
		return "FMT_CONVERT_YUV_YUV422";
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
		return "FMT_CONVERT_YUV_YUV444";
	case VDIN_FORMAT_CONVERT_YUV_RGB:
		return "FMT_CONVERT_YUV_RGB";
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
		return "FMT_CONVERT_RGB_YUV422";
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		return "FMT_CONVERT_RGB_YUV444";
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		return "FMT_CONVERT_RGB_RGB";
	case VDIN_FORMAT_CONVERT_YUV_NV12:
		return "VDIN_FORMAT_CONVERT_YUV_NV12";
	case VDIN_FORMAT_CONVERT_YUV_NV21:
		return "VDIN_FORMAT_CONVERT_YUV_NV21";
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		return "VDIN_FORMAT_CONVERT_RGB_NV12";
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		return "VDIN_FORMAT_CONVERT_RGB_NV21";
	default:
		return "FMT_CONVERT_NULL";
	}
}

struct vdin_measure_ctl_s {
	bool is_clk_enabled;
	struct mutex msr_lock;/* for measure clk control */
	struct clk *msr_clk;
	u32 inuse;
};

struct vdin_set_canvas_addr_s {
	long paddr;
	int  size;

	struct dma_buf *dma_buffer;
	struct dma_buf_attachment *dmabuf_attach;
	struct sg_table *sg_table;
	unsigned long vfmem_start;
	int fd;
	int index;
};
extern struct vdin_set_canvas_addr_s vdin_set_canvas_addr[VDIN_CANVAS_MAX_CNT];

struct vdin_vf_info {
	int index;
	unsigned int crc;

	/* [0]:vdin get frame time,
	 * [1]: vdin put frame time
	 * [2]: receiver get frame time
	 */
	long long ready_clock[3];/* ns */
};

/* debug control bits for scatter memory */
#define DBG_SCT_CTL_DIS			(BIT(0)) /* disable sct memory */
#define DBG_SCT_CTL_NO_FREE_TAIL	(BIT(1)) /* do not free tail */
#define DBG_SCT_CTL_NO_FREE_WR_LIST	(BIT(2)) /* do not free vf mem in wr list */

#define DBG_REG_LENGTH			10

#define DBG_REG_START_READ		(BIT(0))
#define DBG_REG_START_WRITE		(BIT(1))
#define DBG_REG_START_SET_BIT		(BIT(2))
#define DBG_REG_START_READ_WRITE	(BIT(3))
#define DBG_REG_ISR_READ		(BIT(4))
#define DBG_REG_ISR_WRITE		(BIT(5))
#define DBG_REG_ISR_SET_BIT		(BIT(6))
#define DBG_REG_ISR_READ_WRITE		(BIT(7))
#define DBG_REG_TOP_FUNCTION		(BIT(8))
#define DBG_REG_CONVERT_SYNC		(BIT(9))

/***vdin_dbg_en control for print***/
#define DBG_VDIN1_HIST		(BIT(1)) /*print vdin1 HIST_IOC */
#define DBG_VDIN_DUMP		(BIT(4)) /*print vdin dump (close to avoid watchdog timeout) */
#define VDIN_DBG_CNTL_IOCTL	(BIT(10))
#define VDIN_DBG_CNTL_FLUSH	(BIT(11))

struct vdin_slt_test_s {
	bool en;
	bool vf_check_result;
	u32 vf_ori_crc;
	u32 vf_last_crc;
	u32 vf_pass_cnt;
};

enum vdin_memset_dbg_flag {
	YUV422_8B_FULL = 1,
	YUV422_8B_LIMIT,
	YUV422_10B_FULL,
	YUV422_10B_LIMIT,
	RGB_8B_FULL,
	RGB_8B_LIMIT
};

struct dump_info_s {
	u32 highmem_flag;
	u32 width;
	u32 height;
	u32 sizeimage;
	u32 remain_map_size;
	u32 remain_span;
	u64 phy_addr;
	u64 map_addr;
	u64 map_size;
	void *vir_addr;
	struct mutex mutex;/*for dump finish then user layer to dq*/
	struct codec_mm_scatter *sc;
	u32 sc_page_cnt;
};

/*******for debug **********/
struct vdin_debug_s {
	struct tvin_cutwin_s cutwin;
	struct vdin_slt_test_s slt_test;
	struct dump_info_s dump_info_afbc_head;
	struct dump_info_s dump_info_afbc_table;
	struct dump_info_s dump_info_afbc_body;
	struct dump_info_s dump_info_sct_body;
	struct dump_info_s dump_info_mif;
	unsigned int vdin_recycle_num;/* debug for vdin recycle frame by self */
	unsigned int dbg_print_cntl;/* debug for vdin print control */
	unsigned int dbg_pattern;
	unsigned int dbg_device_id;
	unsigned int dbg_sct_ctl;
	unsigned int dbg_wrmif_444;
	unsigned int dbg_de_interlanced_ctl;
	unsigned int dbg_force_convert;
	unsigned int force_afbce_422_12bit;/* 0-auto,1-force enable,2-force disable */
	unsigned int force_fmt;
	unsigned int sar_width;
	unsigned int sar_height;
	unsigned int ratio_control;
	unsigned int dbg_rw_reg_en;
	unsigned int dbg_reg_addr[DBG_REG_LENGTH];
	unsigned int dbg_reg_val[DBG_REG_LENGTH];
	unsigned int dbg_reg_bit[DBG_REG_LENGTH];
	/* bit0:scl_mode;bit1:bypass dsc;bit2:bypass sc */
	unsigned int dbg_dv_hw5;
	unsigned int hconv_mode;
	unsigned int v4l2_buff_area;
	unsigned int matrix_pattern_mode;
	unsigned int dbg_force_type;
	unsigned int rgb_info_x;
	unsigned int rgb_info_y;
	unsigned int rgb_info_r;
	unsigned int rgb_info_g;
	unsigned int rgb_info_b;
	unsigned int dv_dbg_log;
	unsigned int dv_dbg_log_du;
	unsigned int sleep_time;
	unsigned int sm_debug_enable;
	unsigned int vdin_isr_monitor;
	unsigned int vdin_isr_flag; /* flag which affect the skip field */
	unsigned short scaling4h;/* for vertical scaling */
	unsigned short scaling4w;/* for horizontal scaling */
	unsigned short dest_cfmt;/* for color fmt conversion */
	unsigned short vdin1_line_buff;
	unsigned short manual_change_csc;
	unsigned short change_get_drm;
	unsigned char dbg_sel_mat; /* 0x10:mat0,0x11:mat1,0x12:hdr */
	unsigned char yuv422_10l[5];
	unsigned char yuv422_10f[5];
	unsigned char yuv422_8l[4];
	unsigned char yuv422_8f[4];
	unsigned char rgb_8f[3];
	unsigned char rgb_8l[3];
	/* vdin1 hdr set bypass */
	bool vdin1_set_hdr_bypass;
	bool dbg_force_shrink_en;
	bool bypass_update_prop;
	bool bypass_pc_mode;//bypass pc mode set
	bool bypass_game_mode;//bypass game mode set
	bool invert_secure;
	bool bypass_game_frc;//bypass game_frc set
	bool pause_mif_dec;
	bool pause_afbce_dec;
	bool bypass_filter_vsync;
	bool force_bypass_tunnel;
	bool conversion;
	bool ip_pat_en;
	bool invalid_input_en;
	bool rgb_info_enable;
	bool vdin_time_en;
	bool invert_top_bot;
	bool bypass_secure_check;
	bool skip_samba_csc;
	int vdin_memset_dbg_en;
	int vdin_memset_en;
	int vdin_ctl_dbg;
	int vdin_dbg_en;
	int vdin_delay_num;
	int sct_print_ctl;
	enum vdin_memset_dbg_flag flag;
	enum vdin_vf_put_md vdin_frame_work_mode;
	enum tvin_port_e port;
	unsigned int ignore_vdin_ioctl;
	unsigned int force_disp_mode;
	unsigned int reg_addr;
	unsigned int dbg_force_duration;
	bool bypass_game_dyn_fmt;
	bool force_pause_en;
	unsigned int vdin_manual_req_mem_size;
};

struct vdin_dv_s {
	struct vframe_provider_s dv_vf_provider;
	struct tvin_dv_vsif_raw_s dv_vsif_raw;
	struct delayed_work dv_dwork;
	unsigned int dv_cur_index;
	unsigned int dv_next_index;
	unsigned int dolby_input;
	unsigned int dv_dma_size;
	dma_addr_t dv_dma_paddr;
	void *dv_dma_vaddr;
	ulong dv_dma_handle;
	void *temp_meta_data;
	dma_addr_t meta_data_raw_p_buffer0;/*for t7*/
	void *meta_data_raw_v_buffer0;/*for t7*/
	void *meta_data_raw_buffer1;/*for t7*/
	unsigned int dv_flag_cnt;/*cnt for no dv input*/
	u8 dv_flag; /*is signal dolby version 1:vsif 2:emp */
	bool dv_config;
	bool dv_path_idx;
	bool dv_crc_check;/*0:fail;1:ok*/
	bool update_axi_addr;
	unsigned int dv_mem_allocated;
	struct tvin_dv_vsif_s dv_vsif;/*dolby vsi info*/
	bool low_latency;
	unsigned int chg_cnt;
	unsigned int allm_chg_cnt;
	unsigned int allm1_chg_cnt;
};

struct vdin_hdr_s {
	u8 rawdata[HDR_RAW_DATA_SIZE];
	u8 empbuf[HDR_EMP_DATA_SIZE];
	u8 emp_size;
};

struct vdin_dv_hw5_s {
	/* bit0:scl_mode;bit1:bypass dsc;bit2:bypass sc */
	unsigned int hw5_ctl;
	unsigned int dw_out_w;
	unsigned int dw_out_h;
	unsigned int dw_dfmt;
	bool is_auto_mute_needed;
};

struct vdin_afbce_s {
	unsigned int  frame_head_size;/*1 frame head size*/
	unsigned int  frame_table_size;/*1 frame table size*/
	unsigned int  frame_body_size;/*1 frame body size*/
	unsigned long head_paddr;
	unsigned long table_paddr;
	unsigned int  frame_head_size_y;/*1 frame head size*/
	unsigned int  frame_table_size_y;/*1 frame table size*/
	unsigned int  frame_body_size_y;/*1 frame body size*/
	unsigned long head_paddr_c;
	unsigned long table_paddr_c;
	/*every frame head addr*/
	unsigned long fm_head_paddr[VDIN_CANVAS_MAX_CNT];
	/*every frame tab addr*/
	unsigned long fm_table_paddr[VDIN_CANVAS_MAX_CNT];
	/*every body head addr*/
	unsigned long fm_body_paddr[VDIN_CANVAS_MAX_CNT];
	void *fm_table_vir_paddr[VDIN_CANVAS_MAX_CNT];
};

enum vdin_game_mode_chg_e {
	VDIN_GAME_MODE_UN_CHG = 0,
	VDIN_GAME_MODE_OFF_TO_ON,
	VDIN_GAME_MODE_ON_TO_OFF,
	VDIN_GAME_MODE_1_TO_2,
	VDIN_GAME_MODE_2_TO_1,
	VDIN_GAME_MODE_CHK,
	VDIN_GAME_MODE_NUM
};

struct vdin_dts_config_s {
	/* whether check write done flag */
	bool chk_write_done_en;
	bool urgent_en;
	bool v4l_en;
	bool keystone_sel;
	unsigned int afbce_flag_cfg;
	bool dynamic_crop_en;
	unsigned int canvas_config_mode;
	/* 0: canvas_config in driver probe; 1: start config; 2: auto config*/
	unsigned int max_buf_num;
	unsigned int min_buf_num;
	bool cm_enable;
	int vdin_det_idle_wait;
	bool vdin_get_prop_in_vs_en;
	bool vdin_get_prop_in_sm_en;
	bool viu_hw_irq;
	int game_mode_switch_frames;
	int game_mode_phlock_switch_frames; /*min num is 5 by 1080p60hz input test*/
	unsigned int vdin_vrr_switch_cnt;
	int sct_cache_size;
	u32 vdin_re_config;
	u32 vdin_re_cfg_drop_cnt;
	int back_nosig_max_cnt;
	int atv_unstable_in_cnt;
	int atv_unstable_out_cnt;
	int hdmi_unstable_out_cnt;
	int hdmi_stable_out_cnt;
	/* new add in gxtvbb@20160523,reason:
	 *gxtvbb add atv snow config,the config will affect signal detect.
	 *if atv_stable_out_cnt < 100,the signal state will change
	 *after switch source to atv or after atv search
	 */
	int atv_stable_out_cnt;
	/* new add in gxtvbb@20160613,reason:
	 *gxtvbb add atv snow config,the config will affect signal detect.
	 *ensure after fmt change,the new fmt can be detect in time!
	 */
	int atv_stable_fmt_check_cnt;
	int atv_prestable_out_cnt;
	int other_stable_out_cnt;
	int other_unstable_out_cnt;
	int other_unstable_in_cnt;
	int nosig_in_cnt;
	int nosig2_unstable_cnt;
	unsigned int vdin_dv_chg_cnt;
	unsigned int vdin_hdr_chg_cnt;
	unsigned int vdin_vrr_chg_cnt;
	unsigned int vdin_qms_chg_cnt;
	unsigned int sct_remain_size;
	unsigned int vdin_mut_cnt;
	unsigned int dv_mask;
};

struct vdin_s5_s {
	unsigned short h_scale_out;
	unsigned short v_scale_out;
};

struct vdin_t3x_s {
	unsigned short src_in_h_size;
	unsigned short src_in_v_size;
	unsigned short win_cut_out_h_size;
	unsigned short win_cut_out_v_size;
	unsigned short pp_out_h_size;
	unsigned short pp_out_v_size;
	unsigned short wr_mif_h_size;
	unsigned short wr_mif_v_size;
};

struct vdin_v4l2_stat_s {
	/* frame drop due to framerate control */
	unsigned int drop_divide;
	unsigned int done_cnt;
	unsigned int que_cnt;
	unsigned int dque_cnt;
};

struct vdin_v4l2_dbg_ctl_s {
	unsigned int dbg_pix_fmt;
};

struct vdin_frame_stat_s {
	/* frame drop due to framerate control */
	unsigned int write_done_check;
	unsigned int afbce_abnormal_cnt;
	unsigned int afbce_normal_cnt;
	unsigned int wmif_abnormal_cnt;
	unsigned int wmif_normal_cnt;
	unsigned int wr_done_irq_cnt;
	unsigned int meta_wr_done_irq_cnt;
	/* Continuous cycle error counting */
	unsigned int cycle_err_cnt_con;
	unsigned int rdma_manual_cnt;
};

struct vdin_v4l2_s {
	int divide;
	unsigned int secure_flg:1;
	enum v4l2_ext_capture_location l;
	struct vdin_v4l2_stat_s stats;
};

struct vdin_vrr_s {
	unsigned int vdin_vrr_en_flag;
	struct tvin_vtem_data_s vtem_data;
	struct tvin_spd_data_s spd_data;
	unsigned int vrr_chg_cnt;
	unsigned int freesync_chg_cnt;
	unsigned int vrr_mode;
	/* vrr_en in frame_lock_policy */
	bool frame_lock_vrr_en;
	/* app get this status */
	enum vdin_vrr_mode_e pre_vrr_status;
	/* driver detect status */
	enum vdin_vrr_mode_e cur_vrr_status;
	unsigned int qms_chg_cnt;
	unsigned int qms_plus_chg_cnt;
};

/* scatter start */
enum vdin_mem_type_e {
	VDIN_MEM_TYPE_CONTINUOUS = 0,
	VDIN_MEM_TYPE_SCT,
	VDIN_MEM_TYPE_MAX
};

struct vdin_msc_stat_s {
	unsigned int cur_page_cnt;
	unsigned int compressed_page_cnt;
	unsigned int curr_tt_size;
	unsigned int max_size; /* max for one frame */
//	unsigned int sum_max_tt_size;
	unsigned int max_tt_size2;
	unsigned char curr_nub;
	unsigned char max_nub;
	unsigned char mts_pst_ready;
	unsigned char mts_pst_display;
	unsigned char mts_pst_back;
	unsigned char mts_pst_free;
	unsigned char mts_sct_rcc;
	unsigned char mts_sct_ready;
	unsigned char mts_sct_used;
};

/* vdin scatter memory */
struct vdin_msct_top_s {
	void *box;
	unsigned int max_buf_num;
	unsigned int mmu_4k_number; /* mmu 4k number in full size */
	unsigned int buffer_size_nub; /* 4k number per frame */
	unsigned int tail_cnt;
	bool	sct_stop_flag;
	/* statistics info*/
	unsigned int que_work_cnt;
	unsigned int worker_run_cnt;

	struct mutex lock_ready; /* for sct ready */
	struct vdin_msc_stat_s sct_stat[VDIN_CANVAS_MAX_CNT];
	u64 sc_start_time;
	struct vf_entry *vfe;
	/* For getting the compressed afbc size */
	unsigned int cur_frame_idx;
	unsigned int last_frame_idx;
};
/* scatter end */

struct vdin_lossy_comp_param_s {
	u32 lossy_mode;//0:quan_loosy 1:cr_loosy 2:fixed ratio
	u32 quant_diff_root_leave;
	u32 burst_length_add_en;
	u32 burst_length_add_value;
	u32 ofset_burst4_en;
	u32 cr_lossy_ratio;
	u32 cr_lossy_fixed_ratio;//Byte
	u32 cr_lossy_target_byte;//24~48
	u32 mmu_en;
};

struct vdin_local_variable_s {
	bool is_rgba_en;
};

struct vdin_dev_s {
	struct cdev cdev;
	struct device *dev;
	struct tvin_parm_s parm;
	struct tvin_format_s *fmt_info_p;
	struct vf_pool *vfp;
	struct tvin_frontend_s *frontend;
	struct tvin_frontend_s	vdin_frontend;
	struct tvin_sig_property_s pre_prop;
	struct tvin_sig_property_s prop;
	struct vframe_provider_s vf_provider;
	struct tvin_misc_info_s misc_info;
	struct vdin_dv_s dv;
	struct vdin_hdr_s hdr;
	struct delayed_work vlock_dwork;
	struct vdin_afbce_s *afbce_info;
	struct tvin_to_vpp_info_s vdin2vpp_info;
	/*vdin event*/
	struct vdin_event_info event_info;
	struct vdin_event_info pre_event_info;
	/*struct extcon_dev *extcon_event;*/
	struct delayed_work event_dwork;
	struct vdin_vrr_s vrr_data;
	struct vdin_s5_s s5_data;

	 /* 0:from gpio A,1:from csi2 , 2:gpio B*/
	enum bt_path_e bt_path;
	const struct match_data_s *dtdata;
	struct timer_list timer;
	spinlock_t isr_lock;/*isr lock*/
	spinlock_t hist_lock;/*pq history lock*/
	struct mutex fe_lock;/*front end lock*/
	struct clk *msr_clk;
	unsigned int msr_clk_val;

	struct vdin_debug_s debug;
	enum vdin_format_convert_e format_convert;
	unsigned int mif_fmt;/*enum vdin_mif_fmt*/
	enum vdin_color_deeps_e source_bitdepth;/* afbce or main mif */
	enum vdin_color_deeps_e source_bitdepth_dw;/* dw or main mif */
	enum vdin_matrix_csc_e csc_idx;
	struct vf_entry *curr_wr_vfe;//current isr config addr because RDMA next isr write this addr
	struct vf_entry *last_wr_vfe;//last isr config addr because RDMA current isr write this addr
	unsigned int vdin_delay_vfe2rd_list;
	unsigned int curr_field_type;
	unsigned int curr_field_type_dw;/* mif type if dw enabled */
	unsigned int curr_dv_flag;
	unsigned int drop_hdr_set_sts;
	u64 cur_us; //isr cur_us
	u64 pre_us; //isr pre_us

	char name[15];
	/* bit0 TVIN_PARM_FLAG_CAP bit31: TVIN_PARM_FLAG_WORK_ON */
	/* bit[1] 1:already TVIN_IOC_START_DEC OK 0:already TVIN_IOC_STOP_DEC OK*/
	/* bit[2] 1:already TVIN_IOC_OPEN OK 0:already TVIN_IOC_CLOSE OK*/
	unsigned int flags;
	unsigned int flags_isr;
	unsigned int index;
	unsigned int index_export;
	enum vdin_hw_core_type_e hw_core;
	struct vpu_dev_s *vpu_dev_clk_gate;

	unsigned int vdin_max_pixel_clk;
	unsigned long secure_mem_start;
	unsigned int secure_mem_size;
	unsigned long mem_start;
	unsigned int mem_size;
	unsigned int mem_size_bak;/* backup */
	unsigned long vf_mem_start[VDIN_CANVAS_MAX_CNT];
	struct page *vf_venc_pages[VDIN_CANVAS_MAX_CNT];
	struct codec_mm_s *vf_codec_mem[VDIN_CANVAS_MAX_CNT];

	/* save secure handle */
	unsigned int secure_handle;
	unsigned int secure_type;
	bool secure_en;
	bool secure_video;
	bool mem_ta_access;
	bool support_secure;
	bool mem_protected;
	bool mem_registered;
	unsigned int vf_mem_size;
	unsigned int vf_mem_size_small;/* double write use */
	unsigned int frame_size;
	unsigned int frame_size_bak;/* backup */
	unsigned int vf_mem_max_cnt;/*real buffer number*/
	unsigned int frame_buff_num;/*dts config data*/
	unsigned int frame_buff_num_bak;/* backup */
	unsigned int h_active;
	unsigned int v_active;
	unsigned int dest_h_active; /* backend buffer size */
	unsigned int dest_v_active;
	unsigned int crop_h;
	unsigned int crop_v;
	unsigned int h_shrink_out;/* double write use */
	unsigned int v_shrink_out;/* double write use */
	unsigned int h_shrink_times;/* double write use */
	unsigned int v_shrink_times;/* double write use */
	unsigned int h_active_org;/*vdin scaler in*/
	unsigned int v_active_org;/*vdin scaler in*/
	int cur_axis[4];
	int last_axis[4];
	unsigned int canvas_h;
	unsigned int canvas_w;
	unsigned int canvas_active_w;/* mif stride */
	unsigned int canvas_align_w;
	unsigned int canvas_max_size;
	unsigned int canvas_max_num;
	unsigned int canvas_max_stride;
	unsigned int canvas_max_num_bak;/* backup */
	unsigned int vf_canvas_id[VDIN_CANVAS_MAX_CNT];
	/*before G12A:32byte(256bit)align;
	 *after G12A:64byte(512bit)align
	 */
	unsigned int canvas_align;
	unsigned int chroma_size;
	int tuner_id;
	int irq;
	unsigned int rdma_irq;
	int vpu_crash_irq;
	int wr_done_irq;
	int vdin2_meta_wr_done_irq;
	int is_lcd_mute;
	char irq_name[12];
	char vpu_crash_irq_name[20];
	char wr_done_irq_name[20];
	char vdin2_meta_wr_done_irq_name[20];
	/* address offset(vdin0/vdin1/...) */
	unsigned int addr_offset;

	unsigned int unstable_flag;
	struct vdin_frame_stat_s stats;
	unsigned int stamp;
	unsigned int h_cnt64;
	unsigned int cycle;
	unsigned int start_time;/* ms vdin start time */
	int rdma_handle;
	int rdma_read_handle;
	/*for unreliable vsync interrupt check*/
	unsigned long long vs_time_stamp;
	unsigned int unreliable_vs_cnt;
	unsigned int unreliable_vs_cnt_pre;
	unsigned int unreliable_vs_idx;
	unsigned int unreliable_vs_time[10];

	bool cma_config_en;
	/*cma_config_flag:
	 *bit0: (1:share with codec_mm;0:cma alone)
	 *bit8: (1:discontinuous alloc way;0:continuous alloc way)
	 */
	unsigned int cma_config_flag;
	unsigned int cma_config_flag_bak;
#ifdef CONFIG_CMA
	struct platform_device	*this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/*BYTE*/
	unsigned int cma_mem_alloc;
	/*cma_mem_mode:0:according to input size and output fmt;
	 **1:according to input size and output fmt force as YUV444
	 */
	unsigned int cma_mem_mode;
	unsigned int force_yuv444_malloc;
#endif
	/* bit0: enable/disable; bit4: luma range info */
	bool csc_cfg;
	/* duration of current timing */
	unsigned int duration;
	/* duration of current frame */
	unsigned int cur_duration;
	/* color-depth for vdin write */
	/*vdin write mem color depth support:
	 *bit0:support 8bit
	 *bit1:support 9bit
	 *bit2:support 10bit
	 *bit3:support 12bit
	 *bit4:support yuv422 10bit full pack mode (from txl new add)
	 */
	enum vdin_wr_color_depth color_depth_support;
	/*color depth config
	 *0:auto config as frontend
	 *8:force config as 8bit
	 *10:force config as 10bit
	 *12:force config as 12bit
	 */
	enum color_deeps_cfg_e color_depth_config;
	/* new add from txl:color depth mode for 10bit
	 *1: full pack mode;config 10bit as 10bit
	 *0: config 10bit as 12bit
	 */
	unsigned int full_pack;
	/* yuv422 malloc policy for vdin0 debug:
	 *1: force yuv422 memory alloc up to yuv444 10bit(2.5->4)
	 *0: use yuv422 memory alloc by default
	 */
	unsigned int force_malloc_yuv_422_to_444;
	/* output_color_depth:
	 * when tv_input is 4k50hz_10bit or 4k60hz_10bit,
	 * choose output color depth from dts
	 */
	unsigned int output_color_depth;
	/* cut window config */
	bool cut_window_cfg;
	/* only supported when mif output */
	bool auto_cut_window_en;
	/*
	 *0:vdin out limit range
	 *1:vdin out full range
	 */
	unsigned int color_range_mode;
	/*
	 *game_mode:
	 *bit0:enable/disable
	 *bit1:for true bypass and put vframe in advance one vsync
	 *bit2:for true bypass and put vframe in advance two vsync,
	 *vdin & vpp read/write same buffer may happen
	 */
	unsigned int game_mode;
	enum vdin_game_mode_chg_e game_mode_chg;
	unsigned char af_num;/* for one buffer mode */
	unsigned int game_mode_dly;
	unsigned int auto_game_mode_val;

	int chg_drop_frame_cnt;
	unsigned int vdin_pc_mode;
	unsigned int vrr_mode;
	unsigned int vrr_on_add_cnt;
	unsigned int vrr_off_add_cnt;
	unsigned int rdma_enable;
	/* afbce_mode: (amlogic frame buff compression encoder)
	 * 0: normal mode, not use afbce
	 * 1: use afbce non-mmu mode: head/body addr set by code
	 * 2: use afbce mmu mode: head set by code, body addr assigning by hw
	 */
	/*afbce_flag:
	 *bit[0]: enable afbce
	 *bit[1]: enable afbce_lossy
	 *bit[4]: afbce enable for 4k
	 *bit[5]: afbce enable for 1080p
	 *bit[6]: afbce enable for 720p
	 *bit[7]: afbce enable for other small resolution
	 */
	unsigned int afbce_flag;
	unsigned int afbce_mode_pre;
	unsigned int afbce_mode;
	unsigned int afbce_valid;
	unsigned int afbce_mmu_num;
	struct vdin_lossy_comp_param_s cr_lossy_param;

	unsigned int cfg_dma_buf; /* for drm -> vdin */
	unsigned int set_canvas_manual; /* for hwc -> vdin (keystone) */
	unsigned int keystone_vframe_ready;
	struct vf_entry *keystone_entry[VDIN_CANVAS_MAX_CNT];
	bool pre_h_scale_en;
	bool v_shrink_en;
	bool double_wr_cfg;
	bool double_wr;
	bool double_wr_10bit_sup;
	bool black_bar_enable;
	bool hist_bar_enable;
	bool rdma_not_register;
	bool auto_game_flag;
	bool interlace_drop_bottom;
	bool h_skip_en;
	bool v_skip_en;
	bool dv_is_not_std;
	bool bypass_tunnel;
	bool baddr_en;/*0-use canvas;1-use phy addr*/
	bool is_vfce_en;
	bool is_422_12bit_enabled;
	bool force_unlock;
	bool vinfo_over_pixel_clk;
	bool input_over_pixel_clk;
	bool pause_dec_once;
	bool re_cfg;
	unsigned int mute_cnt;
	unsigned int ignore_frames;
	/*use frame rate to cal duration*/
	unsigned int use_frame_rate;
	unsigned int irq_cnt;
	unsigned int vpu_crash_cnt;
	unsigned int frame_cnt;
	unsigned int vdin_drop_num;
	unsigned int put_frame_cnt;
	unsigned int rdma_irq_cnt;
	unsigned int rdma_read_irq_cnt;
	unsigned int vdin_irq_flag;
	unsigned int vdin_reset_flag;
	unsigned int vdin_dev_ssize;
	wait_queue_head_t queue;
	struct dentry *dbg_root;	/*dbg_fs*/

	/*atv non-std signal,force drop the field if previous already dropped*/
	unsigned int interlace_force_drop;
	unsigned int frame_drop_num;
	unsigned int vframe_wr_en;
	unsigned int vframe_wr_en_pre;
	unsigned int pause_dec;

	/*signal change counter*/
	unsigned int sg_chg_afd_cnt;
	unsigned int sg_chg_fps_cnt;

	unsigned int matrix_pattern_mode;
	unsigned int pause_num;
	unsigned int hv_reverse_en;

	/*v4l interface ---- start*/
	enum vdin_work_mode_e work_mode;

	struct v4l2_device v4l2_dev;
	struct video_device video_dev;
	struct vb2_queue vb_queue;
	struct v4l2_format v4l2_fmt;

	struct mutex lock;/*v4l lock*/
	//struct mutex ioctl_lock;/*vl2 io control lock*/
	spinlock_t list_head_lock; /*v4l2 list lock*/
	struct list_head buf_list;	/* buffer list head */
	struct vdin_vb_buff *cur_buff;	/* vdin video frame buffer */
	/*struct v4l2_fh fh;*/
	unsigned long vf_mem_c_start[VDIN_CANVAS_MAX_CNT];/* Y/C non-contiguous mem */
	unsigned int dbg_v4l_pause;
	unsigned int dbg_v4l_no_vdin_event;
	struct vdin_v4l2_dbg_ctl_s v4l2_dbg_ctl;
	struct vdin_set_canvas_addr_s st_vdin_set_canvas_addr[VDIN_CANVAS_MAX_CNT][VDIN_MAX_PLANES];
	bool vdin_set_canvas_flag;
	enum tvin_port_e v4l2_port_cur;
	unsigned int v4l2_port_num;
	enum tvin_port_e v4l2_port[VDIN_V4L2_INPUT_MAX];
	unsigned int afbce_flag_backup;
	unsigned int v4l2_req_buf_num;
	struct vdin_v4l2_s vdin_v4l2;
	//struct v4l2_ctrl_handler ctrl_handler;
	//struct v4l2_ctrl *p_ctrl;
	struct v4l2_ext_capture_capability_info ext_cap_cap_info;
	struct v4l2_ext_capture_plane_info ext_cap_plane_info;
	struct v4l2_ext_capture_video_win_info ext_cap_video_win_info;
	struct v4l2_ext_capture_freeze_mode ext_cap_freezee_mode;
	struct v4l2_ext_capture_plane_prop ext_cap_plane_prop;
	/*v4l interface ---- end*/

	/* scatter start */
	enum vdin_mem_type_e mem_type;
	struct work_struct	sct_work;
	struct workqueue_struct	*wq;
	struct vdin_msct_top_s	msct_top;
	/* scatter end */
	unsigned int tx_fmt;
	unsigned int vd1_fmt;
	unsigned int dbg_force_stop_frame_num;
	unsigned int force_disp_skip_num;
	unsigned int dbg_dump_frames;
	unsigned int dbg_stop_dec_delay;
	unsigned int vout_base_fps; /* get vinfo out base fps value */
	unsigned int vin_base_fps; /*get vin base fps only vrr and freesync has this value*/
	unsigned int vinfo_std_duration; /* get vinfo out real time fps value */
	unsigned int vdin_std_duration; /* get in real time fps value */
	unsigned int dbg_no_swap_en:1;
	/* 0:auto;1:force on;2:force off */
	unsigned int dbg_force_one_buffer;
	unsigned int dbg_afbce_monitor:8;
	unsigned int dbg_fr_ctl:7;/* disable write done check */
	unsigned int vdin_drop_ctl_cnt;/* drop frames casued by dbg_drop_ctl */
	unsigned int vdin_function_sel;
	unsigned int self_stop_start;
	unsigned int quit_flag;
	unsigned int vdin_stable_cnt;
	unsigned int vdin_nosig_cnt;
	struct task_struct *kthread;
	struct completion thd_completion;
	struct vf_entry *vfe_tmp;
	struct vdin_dts_config_s dts_config;
	unsigned int common_divisor;
	unsigned int vrr_frame_rate_min;
	unsigned int err_active;
	unsigned int vdin_input_data_threshold;
	unsigned int report_size_abnormal_cnt;/* drop frames casued by report_hv abnormal*/
	enum tvin_port_type_e port_type;
	unsigned int vdin_drop_cnt;
	unsigned int vdin_isr_drop;
	unsigned int vdin_isr_drop_num;
	unsigned int fs_open_cnt;
	unsigned int rdma_undone_cnt;
	unsigned int delay_line_num;
	unsigned int dv_work_dolby;
	int vsync_reset_mask;
	unsigned int rdma_read_undone_cnt;
	struct vdin_dv_hw5_s dv_hw5;
	struct vdin_local_variable_s local_var;
	bool is_one_buffer;
	bool enable_reset;
	bool vdin_get_prop_in_fe_en;
	bool work_mode_simple;
	struct reg_handle reg_hnd;
	unsigned int vdin_game_frc;
	unsigned int yuv422_2plane_en;
	unsigned int get_vdin_mem_size_flag;
	unsigned int game_frc_mute_time;
};

extern unsigned int max_ignore_frame_cnt;
extern unsigned int skip_frame_debug;
extern unsigned int vdin0_afbce_debug_force;
extern unsigned int vdin_dv_de_scramble;

extern bool vf_log_enable;
extern bool vf_log_fe;
extern bool vf_log_be;
extern unsigned int vf_list_dbg;
extern unsigned int vf_move_print_cnt;

extern struct param_entry vdin_params[];

struct vframe_provider_s *vf_get_provider_by_name(const char *provider_name);
extern bool enable_reset;
extern unsigned int dolby_size_byte;
extern u32 vdin_cfg_444_to_422_wmif_en;
extern struct vdin_hist_s vdin1_hist;

struct vframe_provider_s *vf_get_provider_by_name(const
						  char *provider_name);
char *vf_get_receiver_name(const char *provider_name);
int start_tvin_service(int no, struct vdin_parm_s *para);
int start_tvin_capture_ex(int dev_num, enum port_vpp_e port, struct vdin_parm_s  *para);
int stop_tvin_service(int no);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int vdin_reg_v4l2(struct vdin_v4l2_ops_s *v4l2_ops);
void vdin_unregister_v4l2(void);
int vdin_afbce_compression_ratio_monitor(struct vdin_dev_s *devp, struct vf_entry *vfe);
#endif

int vdin_create_class_files(struct class *vdin_class);
void vdin_remove_class_files(struct class *vdin_class);
int vdin_create_device_files(struct device *dev);
void vdin_remove_device_files(struct device *dev);
int vdin_open_fe(enum tvin_port_e port, int index,
		 struct vdin_dev_s *devp);
void vdin_close_fe(struct vdin_dev_s *devp);
int vdin_start_dec(struct vdin_dev_s *devp);
void vdin_self_start_dec(struct vdin_dev_s *devp);
void vdin_stop_dec(struct vdin_dev_s *devp);
void vdin_self_stop_dec(struct vdin_dev_s *devp);
void vdin_double_write_confirm(struct vdin_dev_s *devp);
irqreturn_t vdin_isr_simple(int irq, void *dev_id);
irqreturn_t vdin_isr(int irq, void *dev_id);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
irqreturn_t vdin_v4l2_isr(int irq, void *dev_id);
#endif

void ldim_get_matrix(int *data, int reg_sel);
void ldim_set_matrix(int *data, int reg_sel);
void tvafe_snow_config(unsigned int on_off);
void tvafe_snow_config_clamp(unsigned int on_off);
void vdin_vf_reg(struct vdin_dev_s *devp);
void vdin_vf_unreg(struct vdin_dev_s *devp);
void vdin_pause_dec(struct vdin_dev_s *devp, unsigned int type);
void vdin_resume_dec(struct vdin_dev_s *devp, unsigned int type);

void vdin_debugfs_init(struct vdin_dev_s *devp);
void vdin_debugfs_exit(struct vdin_dev_s *devp);
void vdin_dump_frames(struct vdin_dev_s *devp);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void vdin_dbg_access_reg(struct vdin_dev_s *devp, unsigned int update_site);
#else
static inline void vdin_dbg_access_reg(struct vdin_dev_s *devp, unsigned int update_site)
{
}
#endif

bool vlock_get_phlock_flag(void);
bool vlock_get_vlock_flag(void);
bool frame_lock_vrr_lock_status(void);

u32 vlock_get_phase_en(u32 enc_idx);

struct vdin_dev_s *vdin_get_dev(unsigned int index);
void vdin_mif_config_init(struct vdin_dev_s *devp);
void vdin_drop_frame_info(struct vdin_dev_s *devp, char *info);
int vdin_create_debug_files(struct device *dev);
void vdin_remove_debug_files(struct device *dev);
void vdin_vpu_dev_register(struct vdin_dev_s *devp);
void vdin_vpu_clk_gate_on_off(struct vdin_dev_s *devp, unsigned int on);
void vdin_vpu_clk_mem_pd(struct vdin_dev_s *devp, unsigned int on);
void vdin_afbce_vpu_clk_mem_pd(struct vdin_dev_s *devp, unsigned int on);
bool vdin_support_axis_change(struct vdin_dev_s *devp);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int vdin_v4l2_probe(struct platform_device *pdev,
		    struct vdin_dev_s *devp);
int vdin_v4l2_if_isr(struct vdin_dev_s *pdev, struct vframe_s *vfp);
#endif

void vdin_frame_write_ctrl_set(struct vdin_dev_s *devp,
				struct vf_entry *vfe, bool rdma_en);
irqreturn_t vdin_write_done_isr(int irq, void *dev_id);
void vdin_game_mode_chg(struct vdin_dev_s *devp,
	unsigned int old_mode, unsigned int new_mode);
void vdin_frame_lock_check(struct vdin_dev_s *devp, int state);
void vdin_v4l2_init(struct vdin_dev_s *devp, struct platform_device *pl_dev);
void vdin_pause_hw_write(struct vdin_dev_s *devp, bool rdma_en);
void vdin_resume_hw_write(struct vdin_dev_s *devp, bool rdma_en);
void vdin_reg_dmc_notifier(unsigned int index);
void vdin_unreg_dmc_notifier(unsigned int index);
void vdin_update_vf_flag(struct vdin_dev_s *devp, struct vf_entry *vfe);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int vdin_dump_one_buf_mem_user(void *output_buf, struct vdin_dev_s *devp,
									unsigned int buf_num);
void vdin_dump_one_afbce_mem_user(void *output_head_ptr, void *output_table_ptr,
void *output_body_ptr, struct vdin_dev_s *devp, unsigned int buf_num);
#endif
u32 get_video_enabled(u8 layer_id);
bool vdin_get_video_ready_state(enum tvin_port_e port);
void vdin_dyn_fmt(struct vdin_dev_s *devp);
void vdin_get_secure_state(struct vdin_dev_s *devp);
unsigned int vdin_check_secure_write_error(struct vdin_dev_s *devp);
void rx_mute_vpp(u8 port_type);
#endif /* __TVIN_VDIN_DRV_H */

