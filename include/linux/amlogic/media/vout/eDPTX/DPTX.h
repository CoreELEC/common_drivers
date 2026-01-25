/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_eDPTX_DPTX_H
#define _AMLOGIC_eDPTX_DPTX_H

#include <linux/types.h>
#include <linux/cdev.h>
#include <drm/amlogic/meson_connector_dev.h>
//#include <drm/drm_dp_helper.h>
#include <linux/platform_device.h>
#include <linux/amlogic/aml_gpio_consumer.h>
#include <linux/pinctrl/consumer.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif
#include <linux/amlogic/media/vout/eDPTX/DPTX_timing.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>

//#define LCD_DEBUG_INFO
extern u8 dptx_print_level;
//bit[15:0]
#define LOG_E      0 // log level ERROR & Key
#define LOG_I      1 // log level Informative
#define LOG_V      2 // log level Verbose
#define LOG_A      3 // log level All

#define DPTX_ERR(drv, fmt, ...)     \
		dptx_print_helper(drv, 0xff, LOG_E, fmt, ##__VA_ARGS__)
#define DPTX_PR(drv, fmt, ...)      \
		dptx_print_helper(drv, 0xff, LOG_I, fmt, ##__VA_ARGS__)
#define DPTX_DBG(drv, fmt, ...)     \
		dptx_print_helper(drv, 0xff, LOG_V, fmt, ##__VA_ARGS__)
#define DPTX_DBG_ADV(drv, fmt, ...) \
		dptx_print_helper(drv, 0xff, LOG_A, fmt, ##__VA_ARGS__)
#define DPTX_P_ERR(drv, port, fmt, ...)   \
		dptx_print_helper(drv, port, LOG_E, fmt, ##__VA_ARGS__)
#define DPTX_P_PR(drv, port, fmt, ...)    \
		dptx_print_helper(drv, port, LOG_I, fmt, ##__VA_ARGS__)
#define DPTX_P_DBG(drv, port, fmt, ...)   \
		dptx_print_helper(drv, port, LOG_V, fmt, ##__VA_ARGS__)
#define DPTX_P_DBG2(drv, port, fmt, ...)   \
		dptx_print_helper(drv, port, LOG_A, fmt, ##__VA_ARGS__)

#define PR_BUF_MAX              (4 * 1024)

#define DPTX_MAX_DRV              2
#define DPTX_MAX_PORT             4
#define DPTX_MAX_VOUT             3

/* ******** clk_ctrl ******** */
#define CLK_CTRL_LEVEL              28 /* [30:28] */
#define CLK_CTRL_FRAC_SHIFT         24 /* [24] */
#define CLK_CTRL_FRAC               0  /* [23:0] */

#define RGB_DELAY                   13
#define PRE_DE_DELAY                8

/* test pattern index */
#define DPTX_TPS_DISABLE      0
#define DPTX_TPS1             1
#define DPTX_TPS2             2
#define DPTX_TPS3             3
#define DPTX_TPS4             4
#define DPTX_QUAL_PAT_DISABLE 5
#define DPTX_D10P2            6
#define DPTX_SYMBOL_ERROR_MSR 7
#define DPTX_PRBS7            8
#define DPTX_80BIT_CUSTOM     9
#define DPTX_HBR2_EYE         10
#define DPTX_CP2520_1         DPTX_HBR2_EYE
#define DPTX_CP2520_2         11
#define DPTX_CP2520_3         12
#define DPTX_TRP_CR           DPTX_TPS1
#define DPTX_TRP_EQ           0xf0

enum dptx_chip_e {
	DPTX_CHIP_T7,
	DPTX_CHIP_A9,
	DPTX_CHIP_MAX,
};

struct dptx_chip_data_s {
	enum dptx_chip_e chip_type;
	const char *chip_name;
	u8  drv_max;
	u32 offset_venc[DPTX_MAX_DRV];     // 0x600
	u32 offset_venc_if[DPTX_MAX_DRV];  // 0x500
	u32 offset_venc_data[DPTX_MAX_DRV];// 0x100
	u8 venc_clk_msr_id[DPTX_MAX_DRV];

	u32 link_rate[DPTX_MAX_DRV][DPTX_MAX_PORT];
	u32 lane_count[DPTX_MAX_DRV][DPTX_MAX_PORT];
	u8  TPS_support;
	u8  DACP_support;
	u32 pixel_clk_limit;
};

struct dptx_boot_ctrl_s {
	u8 link[DPTX_MAX_PORT][2];
	u8 vmode_sel_idx;
	u8 vmode_cfmt_sel;
	u8 tx_prepared;
	u8 disp_on;
	u8 uboot_edid_crc;
	u8 user_debug_port_count;
	u8 user_hpd_debug;
};

struct dptx_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

#define MOD_LEN_MAX        30

struct dptx_venc_cfg_s {
	u8 pll_flag;
	u8 clk_mode;
	u8 ppc;
	u8 clk_change; /* internal used */
	u8 ss_level;
	u8 ss_freq;
	u8 ss_mode;

	unsigned int enc_clk;
	unsigned long long bit_rate; /* Hz */

	unsigned int hstart;
	unsigned int hend;
	unsigned int vstart;
	unsigned int vend;
	u8 pre_de_h;
	u8 pre_de_v;

	unsigned short de_hs_addr;
	unsigned short de_he_addr;
	unsigned short de_vs_addr;
	unsigned short de_ve_addr;

	unsigned short hs_hs_addr;
	unsigned short hs_he_addr;
	unsigned short hs_vs_addr;
	unsigned short hs_ve_addr;

	unsigned short vs_hs_addr;
	unsigned short vs_he_addr;
	unsigned short vs_vs_addr;
	unsigned short vs_ve_addr;

	unsigned short pre_h_de_start;
	unsigned short pre_h_de_end;
	unsigned short pre_v_de_start;
	unsigned short pre_v_de_end;
	unsigned short pre_hso_start;
	unsigned short pre_hso_end;
	unsigned short pre_vso_hstart;
	unsigned short pre_vso_hend;
	unsigned short pre_vso_start;
	unsigned short pre_vso_end;
};

struct dptx_phy_cfg_s {
	u32 flag;
	u32 vswing;
	struct dptx_phy_lane_s {
		u8 status;
		u8 amp;
		u8 preem;
		u8 post_cur;
	} lane[4];
	u16 level_to_phy_lut[10][3];
};

#define DPTX_TYPE_DP          0
#define DPTX_TYPE_eDP         1

#define LCD_CLK_SS_BIT_FREQ             0
#define LCD_CLK_SS_BIT_MODE             4

#define DPTX_GPIO_MAX                    0xff
#define DPTX_GPIO_OUTPUT_LOW             0
#define DPTX_GPIO_OUTPUT_HIGH            1
#define DPTX_GPIO_INPUT                  2

#define DPTX_VENC_1PPC          1
#define DPTX_VENC_2PPC          2
#define DPTX_VENC_4PPC          4

struct aux_irq_semaphore_s {
	raw_spinlock_t lock;
	bool v;
};

struct dptx_clk_cfg_s {
	u64 fin;
	u64 fout;
	u8 clk_src; // 0=PLL, 1=FIX_PLL, 2=LINK_CLK
	void *pll_data;

	/* pll parameters */
	unsigned int pll_id;
	unsigned int pll_offset;
	unsigned int pll_od_fb;
	u16 pll_m;
	u16 pll_n;
	u64 pll_fvco;
	u8 pll_od_sel[3];
	u8 pll_tcon_div_sel;

	unsigned int pll_frac;
	unsigned int pll_frac_half_shift;
	unsigned long long pll_fout;
	unsigned int pll_div_fout;

	unsigned int ss_level;
	unsigned int ss_dep_sel;
	unsigned int ss_str_m;
	unsigned int ss_ppm;
	unsigned int ss_freq;
	unsigned int ss_mode;
	unsigned int ss_en;

	u8 pll_clk_div_sel;
	u8 div0;
	u8 div1;
	u8 xd;
	u8 vclk_sel;

	// unsigned int err_fmin;
	unsigned int done;
};

#define DPTX_DRV_TIMING_MAX  8
struct dptx_edid_info_s {
	struct edid *drm_edid;
	u8 manufacturer_id[4];  //[8:9]2byte
	u16 product_id;         //[10:11]2byte
	u32 product_sn;         //[12:15]4byte
	u8 week;                //[16]1byte
	u8 year;                //[17]1byte + 1990
	u16 version;            //[18:19]2byte
	u32 established_timing; //[35:37]3byte
	// u32 standard_timing1;   //[38:45]4byte
	// u32 standard_timing2;   //[46:53]4byte
	u16 cfmt_support;       // each bit to CFMT
	u16 display_size[2];    //mm
	u8 max_bpc;

	char name[14];
	char serial_num[14];
	char asc_string[14];
	// unsigned int VIC_val[8];
	struct dptx_edid_range_limit_s {
		u16 vfreq[2];
		u16 hfreq[2];
		u32 pclk[2];
		u16 h_blank_min, v_blank_min;
	} range;

	u8 ext_flag;  //[126]1byte
	u8 did_version;

	u8 edid_crc;
};

struct dptx_link_cfg_s {
	u8 DPCD_ver;
	u8 max_lane_count;
	u8 max_link_rate;
	u8 max_link_rate_UHBR; // [0]=10G [1]=20G [2]=13.5G
	u8 link_cap;          // [0]=TPS3 [1]=TPS4 [7]=POST_LT_ADJ_REQ [6]=no-train [5]=ss
	u8 DACP_support;      // [0]=M1(HDCP) [1]=M2(Panel Limit) [2]=M3a(eDP ASSR)
	u8 train_rd_interval;
	u8 NORP;              //Number of Receiver Ports
	u8 dev_type;          // 0=DP; 1=eDP
	u8 DPCD_reg_func;     // [0]=ext-DPCD [1]=eDP-DPCD [2]=DPCD_BL [3]=DPCD_TOUCH
	u8 coding_cap;        // [0]=8b/10b [0]=128b/132b
	u8 msa_ignore;        // [0]=msa [1]=adpt-sync_SDP
	u8 enh_frame_en;

	struct eDP_cap_s {
		u8 ver;
		u8 bl_ctrl_cap; // [0]=TCON_bl [1]=pin_en [2]=aux_en [3]=lm_cap [4:5]=bl_reg
		u8 bl_adj_cap;  // as 00702h
			// [0]=bl_pwm [1]=bl_aux [2]=bl-2byte [3]=aux-pwm_combo
			// [4]=bl_pwm_freq_pass-thru [5]=aux_pwm_freq [6]=dynamic_bl [7]=bl-vsync
		u8 bl_rgn_x;
		u8 bl_rgn_y;
		u8 od_cap;    // [0]=od_cap [1]=od_aux
		u8 test_cap;  // [0]=pin [1]=aux
		u8 ctrl_cap;  // [0]=frc [1]=color_eng [2]=set_power

		u8 PSR_cap;   // as 00702h
			// 0=n 1=PSR1 2=1+PSR2(SU) 3=2+PSR2(Y-coor) 4=3+PSR2(early_transport)
		u8 PSR_setup_time; // as 00071h
		u8 PSR_req; // [0]=LT_need_when_1_exit
			// [1]Y-coor need on SU [2]=Update Granularity [3]=no_fr_sync on SU
		u16 PSR2_SU_X_GRANULARITY;
		u8 PSR2_SU_Y_GRANULARITY;
	} eDP_cap;

	struct DSC_cap_s {
		u8 cap;            // as 00060h
		u8 algm_ver;       // as 00061h
		u8 buf_block_size; // as 00062h
		u8 slice_cap1;     // as 00064h
		u8 buf_bit_depth;  // as 00065h
		u8 feature_spt;    // as 00066h
		u8 max_bpp_0;      // as 00067h
		u8 max_bpp_1;      // as 00068h
		u8 color_fmt_cap;  // as 00069h
		u8 color_dep_cap;  // as 0006ah
		u8 peak_throughput;// as 0006bh
		u8 max_slice_width;// as 0006ch
		u8 slice_cap2;     // as 0006dh
		u8 bpp_delta_increment0;// as 0006eh
		u8 bpp_delta_increment1;// as 0006fh
	} dsc_cap;

	u8 sync_clk_mode;

	/* internal used */
	// unsigned char coding_support;
	u8 training_mode;
	u8 preset_ds[4];

	u8 HPD_level;
	u8 irq_sta;

	u8 link_rate;
	u8 lane_count;
	u8 link_rate_update;

	u8 phy_update;
	u8 curr_ds[4];
	u8 adj_req_ds[4];
};

#define DPTX_DRV_VMODE_MAX   64
#define VMODE_FLAG_VALID     BIT(0)
#define VMODE_FLAG_FR_RANGE  BIT(1)
#define VMODE_FLAG_PREFERRED BIT(2)
// #define VMODE_FLAG_BW_ENOUGH BIT(3)
struct dptx_vmode_mgr_s {
	struct dptx_vmode_s {
		u32 fr100_int;
		u8 fr_adv; // 0=fr_int; 1=fr_frac; 0xff=raw fr
		u8 base_dtd_idx;
		u8 flag;
		u16 cfmt_support;
	} vmodes[DPTX_DRV_VMODE_MAX];
	u8 vmode_sel_idx;
	u8 vmode_cfmt_sel;
};

#define DPTX_STA_PROBE_DONE   BIT(0)
#define DPTX_STA_DRV_READY    BIT(1)
#define DPTX_STA_HPD_TRI_EN   BIT(2)
#define DPTX_STA_HPD_HIGH     BIT(3)
#define DPTX_STA_LINK_ON      BIT(4)
#define DPTX_STA_DISP_ON      BIT(5)

#define DPTX_GPIO_NAME_MAX    12

/* per dp1.4a spec Table 2-94 & Table 2-96 */
struct msa_s {
	u32 m_vid;
	u32 n_vid;

	u16 h_total;
	u16 v_total;
	u8 hsync_pol:1;
	u8 vsync_pol:1;
	u16 hsw;
	u16 vsw;
	u16 h_active;
	u16 v_active;
	u16 hbp;
	u16 vbp;

	/* mics0/1 */
	u8 sync_clk_mode:1;
	/* Unit: 1000, used only under sync_clk mode */
	unsigned int pixel_freq;
	/* 0x6/0xa/0x14/0x1e, used only under sync_clk mode */
	u8 link_rate;
	/* for SRCX_USER_DATA_COUNT calc */
	u8 lane_count;

	u8 interlace_mode:1;
	/* for 3D */
	u8 stereo_video_attr:2;
	/* 0:rgb, 1:y422, 2:y444, 3:reserved */
	u8 color_format:2;
	/* 0: VESA range, 1: CEA range */
	u8 dynamic_range;
	/* 0: BT601, 1: BT709, others: reserved for future */
	u8 ycbcr_colorimetry;
	/* 6/8/10/12/16 bits per color component */
	u8 bit_depth;

	/* SRC0_TRANSFER_UNIT_CONFIG */
	u32 tu_config;
	/* SRC0_USER_SYNC_POLARITY */
	u32 user_sync_polarity;
	/* SRC0_USER _CONTROL */
	u32 user_control;
};

struct dptx_drv_s {
	u8 idx;
	u8 status;
	u8 mode; // 0=DP; 1=eDP
	u8 viu_sel; // 1=vout; 2=vou2; 3=vout3
	u8 uboot_edid_crc;
	// atomic_t aux_irq_sta;

	struct dptx_user_set_s {
		char dev_name[32];
		u8 use_aux_bl_as_possible;  //dft: 0
		u8 user_port_mask;  //dft: 1
		u8 dual_port_type;  //dft: 1
		u8 user_link_rate;  //dft: 4
		u8 user_lane_count; //dft: 0xfff
		u8 user_panel_config;
		u8 user_color_format;
		u8 user_preset_timing;
		u16 user_hpd_ignore;
		u16 user_vmode_sel;
		u16 user_disable_PSR;
	} setting;

	struct gpio_desc *PWR_gpio;
	struct gpio_desc *bl_gpio;
	struct pinctrl *pin_c;

	//char PWR_gpio_name[DPTX_GPIO_NAME_MAX];

	int vsync_irq[DPTX_MAX_VOUT];
	int HPD_irq;
	char vsync_name[DPTX_MAX_VOUT][16];
	char HPD_name[12];
	u16 vsync_cnt;

	struct dptx_chip_data_s *data;
	struct dptx_reg_map_s *reg_map;

	struct dptx_phy_cfg_s phy_cfg; // !move to link
	struct dptx_clk_cfg_s vid_clk;
	struct dptx_clk_cfg_s link_clk; // !move to link
	struct dptx_venc_cfg_s venc_cfg;

	struct dptx_vmode_mgr_s vmode_mgr;
	u8 next_vmd_idx;

	struct dptx_detail_timing_s act_timing;
	struct msa_s msa;
	struct dptx_sink_dev_s {
		char sink_name[32];
		u8 port_mask;
		u8 hpd_mask;
		u8 pwr_gpio_mask;
		u8 dt_cnt;
		struct dptx_detail_timing_s timing[DPTX_DRV_TIMING_MAX];
		struct dptx_edid_info_s edid;
		struct dptx_link_cfg_s *link[4];
	} sink;

	struct cdev cdev;
	struct device *dev;
	struct platform_device *pdev;
	struct backlight_device *bl_wrapper_dev;

	struct vout_server_s vout_server;
	spinlock_t isr_lock; //interrupt lock
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *vpu_dev;
#endif
	struct vinfo_s vinfo;

	void *if_ctrls;

	struct connector_hpd_cb drm_hpd_cb;
	struct drm_panel *panel;
};

void dptx_print_helper(struct dptx_drv_s *dptx, u8 port, u8 pr_type, const char *fmt, ...);

#endif
