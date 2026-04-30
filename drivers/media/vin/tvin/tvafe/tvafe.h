/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _TVAFE_H
#define _TVAFE_H

/* Standard Linux Headers */
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/mutex.h>

#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "tvafe_general.h"   /* For Kernel used only */
#include "tvafe_cvd.h"       /* For Kernel used only */

/* ************************************************* */
/* *** macro definitions ********************************************* */
/* *********************************************************** */
/* 20211225: tvafe change clamp config */
/* 20220415: pq_reg_trust_table add register mask */
/* 20220430: PALM CC not display */
/* 20220513: add electrical performance debug adjust command */
/* 20220602: call avin read crash after shutdown */
/* 20220613: tvafe shutdown not close clk stuck */
/* 20220708: ntsc-m set 0x2e to default value */
/* 20220812: avin 0% white cannot detect signal */
/* 20221105: av detect failure and av black screen */
/* 20230106: av format switch flower screen */
/* 20230304: ntsc 50HZ will crash */
/* 20230310: adjust electrical value */
/* 20230322: memset vbi memory */
/* 20230423: Set the default value of comp_level_adj to 3 */
/* 20230423: white level detection is enabled by default */
/* 20230509: adjust nostd_stable_cnt */
/* 20230602: chip bringup */
/* 20230607: optimized snow config */
/* 20230609: chip bringup */
/* 20230804: atv add filter format and set saturation fail */
/* 20241007: wss function optimization */
/* 20241105: fmt detection optimization */
/* 20241217: removed unnecessary reset */
/* 20241227: modify pal-n adc setting. fix double shadow issue */
/* 20250115: adjust the image positions of various formats */
/* 20250317: fix white field recognition problem */
/* 20250604: remove softreset in av; try manual fmt when manual fmt is not null */
/* 20250618: return no signal when entering a empty channel */
/* 20250625: ATV snow function protect */
/* 20250711: No skip frame when ATV searching */
/* 20250804: Reduces av format check time */
/* 20250805: remove clear state in dec_stop */
/* 20250820: drop 40 frames before wss stable */
/* 20250829: Determine the standard based on the audio format */
/* 20251224: Notify vdin when config format changes */
/* 20251225：Increase PGA adjustment frequency */
/* 20260114：remove ntscm and palm switch flow, reduce pal threshold */
/* 20260205: Fix pattern flickering during switching */
/* 20260430: Clear try_format_cnt after frequency channel search completes */
#define TVAFE_VER "0260430: Clear try_format_cnt after frequency channel search completes"

/* used to set the flag of tvafe_dev_s */
#define TVAFE_FLAG_DEV_OPENED 0x00000010
#define TVAFE_FLAG_DEV_STARTED 0x00000020
#define TVAFE_FLAG_DEV_SNOW_FLAG 0x00000040
#define TVAFE_POWERDOWN_IN_IDLE 0x00000080

/*used to flag port opend for avdetect config*/
#define TVAFE_PORT_AV1 0x1
#define TVAFE_PORT_AV2 0x2

#define TVAFE_WSS_FUNCTION		BIT(0)
/* smc main cmd */
#define TVAFE_SMC_CMD	0x8200008e
/* smc sub id */
#define TVAFE_GET_MACROV_STS	1

/************************************************************ */
/* *** enum definitions ********************************************* */
/************************************************************ */

/************************************************************* */
/* *** structure definitions ********************************************* */
/************************************************************* */
/* tvafe module structure */
struct tvafe_info_s {
	struct tvin_parm_s parm;
	struct tvafe_cvd2_s cvd2;
	/*WSS INFO for av/atv*/
	u8 aspect_ratio;
	unsigned char	active_ratio;	/* active aspect ratio */
	unsigned int aspect_ratio_cnt;
};

#define TVAFE_AUTO_CDTO    BIT(0)
#define TVAFE_AUTO_HS      BIT(1)
#define TVAFE_AUTO_VS      BIT(2)
#define TVAFE_AUTO_DE      BIT(3)
#define TVAFE_AUTO_3DCOMB  BIT(4)
#define TVAFE_AUTO_PGA     BIT(5)
#define TVAFE_AUTO_HS_MODE BIT(6)
#define TVAFE_AUTO_VS_MODE BIT(7)

extern bool disable_api;
extern bool force_stable;
extern bool tvafe_atv_search_channel;
extern bool tvafe_signal_stable;
extern bool tvafe_mode;
extern bool demod_is_pal;
extern struct tvafe_avin_det_s *av_dev;
extern bool tvafe_start_flag;

extern enum tvafe_no_std_config_e force_nostd;

#define TVAFE_DBG_NORMAL		BIT(0)
#define TVAFE_DBG_ISR			BIT(4)
#define TVAFE_DBG_SMR			BIT(8)
#define TVAFE_DBG_SMR2			BIT(9)
#define TVAFE_DBG_SMR3			BIT(10)
#define TVAFE_DBG_SMR4			BIT(11)
#define TVAFE_DBG_NON_STD		BIT(12)
#define TVAFE_DBG_NON_STD2		BIT(13)
#define TVAFE_DBG_AUTO_VS		BIT(14)
#define TVAFE_DBG_AUTO_HS		BIT(15)
#define TVAFE_DBG_WSS			BIT(16)
#define TVAFE_DBG_WSS1			BIT(17)
#define TVAFE_DBG_HORSTP_REGBASE	BIT(18)
extern unsigned int tvafe_dbg_print;

struct tvafe_user_param_s {
	unsigned int cutwindow_val_h[5];
	unsigned int cutwindow_val_v[5];
	unsigned int horizontal_dir0[5];
	unsigned int horizontal_dir1[5];
	unsigned int horizontal_stp0[5];
	unsigned int horizontal_stp1[5];
	unsigned int cutwindow_val_vs_ve;
	unsigned int cdto_adj_hcnt_th;
	unsigned int cdto_adj_ratio_p;
	unsigned int cdto_adj_offset_p;
	unsigned int cdto_adj_ratio_n;
	unsigned int cdto_adj_offset_n;
	unsigned int auto_adj_en;
	unsigned int vline_chk_cnt;
	unsigned int hline_chk_cnt;
	unsigned int nostd_vs_th;
	unsigned int nostd_no_vs_th;
	unsigned int nostd_vs_cntl;
	unsigned int nostd_vloop_tc;
	unsigned int force_vs_th_flag;
	unsigned int nostd_stable_cnt;
	unsigned int nostd_dmd_clp_step;
	unsigned int skip_vf_num;
	unsigned int unlock_cnt_max;
	unsigned int avout_en;
	unsigned int nostd_bypass_iir;
	unsigned int low_amp_level;
	unsigned int macrovision;

	/* debug */
	unsigned int cutwin_test_en;
	unsigned int cutwin_test_hcnt;
	unsigned int cutwin_test_vcnt;
	unsigned int cutwin_test_hcut;
	unsigned int cutwin_test_vcut;
};

/* tvafe device structure */
struct tvafe_dev_s {
	unsigned int index;
	dev_t devt;
	struct cdev cdev;
	struct device *dev;

	struct mutex afe_mutex; /* tvafe_dev_s lock */
	struct timer_list timer;

	struct tvin_frontend_s frontend;
	unsigned int flags;
	/* bit4: TVAFE_FLAG_DEV_OPENED */
	/* bit5: TVAFE_FLAG_DEV_STARTED */
	struct tvafe_pin_mux_s *pinmux;
	/* pin mux setting from board config */
	/* cvd2 memory */
	struct tvafe_cvd2_mem_s mem;

	struct tvafe_info_s tvafe;

	const unsigned int (*acd_table)[ACD_REG_NUM + 1];
	struct tvafe_reg_table_s **pq_conf;

	unsigned int cma_config_en;
	/*cma_config_flag:1:share with codec_mm;0:cma alone*/
	unsigned int cma_config_flag;
#ifdef CONFIG_CMA
	struct platform_device *this_pdev;
	struct page *venc_pages;
	unsigned int cma_mem_size;/* BYTE */
	unsigned int cma_mem_alloc;
#endif
	unsigned int frame_skip_enable;
	unsigned int sizeof_tvafe_dev_s;
	unsigned int tvafe_function_sel;
	unsigned int tvafe_ratio_cnt;
	unsigned int tvafe_ratio_effect_cnt;
	unsigned int def_ntsc_crop;
	unsigned int def_pai_i_crop;
	u8 tvafe_dbg;
};

struct tvafe_image_position_s {
	int cvd_hstart;
	int cvd_hwidth;
	int cvd_vstart;
	int cvd_vheight;
	int acd_delay;
	int acd_debypass;
	int acd_af_delay;
	int acd_hstart;
	int acd_hend;
	int acd_vstart;
	int acd_vend;
};

bool tvafe_get_snow_cfg(void);
void tvafe_set_snow_cfg(bool cfg);

struct tvafe_user_param_s *tvafe_get_user_param(void);
struct tvafe_dev_s *tvafe_get_dev(void);

int tvafe_reg_read(unsigned int reg, unsigned int *val);
int tvafe_reg_write(unsigned int reg, unsigned int val);
int tvafe_vbi_reg_read(unsigned int reg, unsigned int *val);
int tvafe_vbi_reg_write(unsigned int reg, unsigned int val);
int tvafe_hiu_reg_read(unsigned int reg, unsigned int *val);
int tvafe_hiu_reg_write(unsigned int reg, unsigned int val);
int tvafe_device_create_file(struct device *dev);
void tvafe_remove_device_files(struct device *dev);
int tvafe_pq_config_probe(struct meson_tvafe_data *tvafe_data);
void cvd_set_shift_cnt(enum tvafe_cvd2_shift_cnt_e src, unsigned int val);
unsigned int cvd_get_shift_cnt(enum tvafe_cvd2_shift_cnt_e src);
int tvafe_bringup_detect_signal(struct tvafe_dev_s *devp, enum tvin_port_e port);

#endif  /* _TVAFE_H */

