/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_RX_DRV_H__
#define __HDMI_RX_DRV_H__

#include <linux/workqueue.h>
#include <linux/extcon-provider.h>

#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/iomap.h>
#include <linux/cdev.h>
#include <linux/irqreturn.h>
#include <linux/input.h>
#include <uapi/amlogic/hdmi_rx.h>
#include "../tvin_global.h"
#include "../tvin_frontend.h"
//#include "hdmirx_repeater.h"
//#include "hdmi_rx_pktinfo.h"
#include "hdmi_rx_edid.h"
#include "hdmi_rx_drv_ext.h"
#include "hdmi_rx_diag.h"

//2023.03.01
//added interlaced mode protection
//2023.03.01
//optimize the judgment condition of unnormal timing
//2023.03.17
//fix emp pkt parse err
//2023.03.18
//disabled edid response after HPD low
//hdmirx earc
//add an ioctl to update single edid
//2023.04.28
//fix HDR10+ info error
//2023.5.6
//bring up sync to 5.15
//2023.5.17
//support filmmaker mode
//fix no edid result in system crash
//fix switch fail between edid 1.4 and 2.0.
//hdmi hbr no sound
//fix txhd2 merge to trunk problem
//2023.7.10 txhd2 bring up
//2023.7.21 fix phy/pll clk band
//2023.7.25 hdmirx suspend
//2023.7.27 phy setting and control pcs reset
//2023.8.8 fix edid switch issue
//2023.8.18 fix no sound when no aif pkt
//2023.8.25 fix t3x port4 get spd info err
//2023.8.29 hdcp cts
//2023 9.8 t3x reb bring up
//2023 9.13 add gcp mute cnt when fps change
//2023.9.22 t3x 480p aud clk settings
//2023.10.07 CTS2-79 for IP2.0
//2023.10.11 increase interval time of HPD low
//2023.10.27 hdmirx device vendor and product
//2023.11.3 add bist for debug
//2023.11.09 clr gcp write&the respective av mute related filed
//2023.11.16 rm gb check when dvi input
//2023.11.29 set main_port_open when resume
//2023.12.12 t3x no open port limit when reboot
//2024.01.04 fix soundless issue for 2.0 ip
//2024.01.08 support to get AVI info
//2024.01.10 optimize eq setting for 75m~115m frequency
//2024.01.11 fix EMP DDR write out of bounds
//2024.02.05 Fix t5d accessing illegal addresses
//2024.02.21 fix t3x hbr audio clk not correct issue
//2024.2.22 fix hdr flash
//2024.03.04 fix repeat issue
//2024.03.04 fix kernel panic on T7C without hdmirx
//2024.3.15 fix arc port hpd changed frequently
//2024.04.24 remove esd hpd
//2024.07.04 optimize pcs reset flow
//2024.07.10 correct 12g aud div
//2024.08.15 add avi filmmaker flag
//2024.08.28 add hdr10p licence detection
//2024.08.29 support dolbyvision unique drm source-led
//2024.08.30 disable hdcp when suspend
//2024.09.18 remove warning print
//2024.09.24 reduce rx boot print
//2024.09.30 update fsm when update edid
//2024.10.09 t3x/t7c hdmirx support std hibernate
//2024.11.15 add protect for fsm
//2024.11.29 optimize early suspend flow
//2025.01.02 optimize timer execution logic
//2025.01.16 t3 hdmirx support std hibernate
//2025.02.20 add 4k144 165 dsc timing support
//2025.03.05 fix no signal when on/off dlg on home
//2025.03.20 optimize early suspend flow
//2025.05.06 hdmi 422 to 444 for pre proc problem
//2025.05.19 change resume flow
//2025.06.05 Fix aspect ratio flow
//2025.06.20 use hrtimer to poll edid sts
//2025.06.26 modify acr_ref_clk config method
//2025.07.31 Fix t3x bright line issue on frl mode
//2025.08.12 Fixed info_show function array out-of-bounds issue
//2025.8.22 clean hdmirx log
//2025.09.01 optimize hdmirx class create flow
//2025.09.29 remove unused ioctl cmd
//2025.11.11 add vsync polarity support and cal for neg vstart
//2025.11.12 set VESA 640*480 aspect ratio to 4/3
//2025.11.18 add mute cnt when avi change
//2025.11.27 fix bits per pixel cal
//2025.11.28 fix fps error issue for tb49
//2025.12.16 fix PM-related DMC VIOLATION issues
//2025.12.24 add ioc for framerate limit
//2025.12.29 Fix issue that VTEM packages not delivered at high framerate
//2025.12.30 optimize the hpd control flow when resume
//2026.01.12 Fix the issue that TB49 displays pink
//2026.01.19 sync kernel5.15 and kernel6.12
//2026.01.27 remove dsc cut logic
//2026.01.29 Fix issue that devices not supporting EDID2.0 silent when EDID auto enabled.
//2026.01.30 fix issue with incorrect FPS display for QMS signal
//2026.03.03 fix set kthread priority error
//2026.03.06 add eq eye monitor
#define RX_DRV_VER "ver.2026/03/06"

/*print type*/
#define COR1_LOG	0x10000
#define	LOG_EN		0x01
#define VIDEO_LOG	0x02
#define AUDIO_LOG	0x04
#define HDCP_LOG	0x08
#define PACKET_LOG	0x10
#define EQ_LOG		0x20
#define REG_LOG		0x40
#define ECC_LOG		0x80
#define EDID_LOG	0x100
#define PHY_LOG		0x200
#define VSI_LOG		0x800
#define SIG_PROP_LOG	0x400
#define DBG_LOG		0x1000
#define IRQ_LOG		0x2000
#define COR_LOG		0x4000
#define DBG1_LOG    0x8000
#define HDCP_IRQ_LOG 0x40000000
#define PKT_IRQ_LOG 0x20000000


#define EDID_DATA_LOG	0x20000
#define RP_LOG		0x40000
#define FRL_LOG		0x80000
#define DUMP_AUD_LOG	0x100005

#define FRAME_RATE_MIN 20
#define FRAME_RATE_MAX 300

#define BOOT_INFO_NUM			10
#define BOOT_INFO_LEN			50

/* 50ms timer for hdmirx main loop (HDMI_STATE_CHECK_FREQ is 20) */

#define TIME_1MS 1000000
#define EDID_MIX_MAX_SIZE 64
#define ESM_KILL_WAIT_TIMES 10
#define pr_var(str, index) rx_pr("%5d %-30s = %#x\n", (index), #str, (str))
#define var_to_str(var) (#var)

#define PFIFO_SIZE 256
#define HDCP14_KEY_SIZE 368

/* sizeof(emp_buf) / sizeof(sizeof(struct pd_infoframe_s) + 1) = 1024/32 */
#define EMP_DSF_CNT_MAX 32

//#define SPECIAL_FUNC_EN
#ifdef SPECIAL_FUNC_EN
//bit0 portA bit1 portB bit2 portC
#define EDID_DETECT_PORT  2
#else
#define EDID_DETECT_PORT  7
#endif

#define PACKET_TYPE_MAX 256

//for dump i2c_monitor data to file
//#define I2C_MONITOR_DUMP_FILE

/* support dump audio sample packet from emp */
#define CONFIG_RX_AUDIO_TO_EMP

enum chip_id_e {
	CHIP_ID_NONE,
	CHIP_ID_GXTVBB,
	CHIP_ID_TXL,
	CHIP_ID_TXLX,
	CHIP_ID_TXHD,
	CHIP_ID_TL1,
	CHIP_ID_TM2,
	CHIP_ID_T5,
	CHIP_ID_T5D,
	CHIP_ID_T7,
	CHIP_ID_T3,
	CHIP_ID_T5W,
	CHIP_ID_T5M,
	CHIP_ID_TXHD2,
	CHIP_ID_T6D,
	CHIP_ID_T6W,
	CHIP_ID_T3X,
	CHIP_ID_T6X,
	CHIP_ID_A9,
};

enum phy_ver_e {
	PHY_VER_ORG,
	PHY_VER_TL1,
	PHY_VER_TM2,
	PHY_VER_T5,
	PHY_VER_T7,
	PHY_VER_T3,
	PHY_VER_T5W,
	PHY_VER_T5M,
	PHY_VER_TXHD2,
	PHY_VER_T6D,
	PHY_VER_T6W,
	PHY_VER_T3X,
	PHY_VER_T6X,
	PHY_VER_A9,
};

enum port_num_e {
	PORT_NUM_1 = 1,
	PORT_NUM_2 = 2,
	PORT_NUM_3,
	PORT_NUM_4,
};

struct meson_hdmirx_data {
	enum chip_id_e chip_id;
	enum phy_ver_e phy_ver;
	enum port_num_e port_num;
	struct ctrl *phyctrl;
};

struct hdmirx_dev_s {
	int                         index;
	dev_t                       devt;
	struct cdev                 cdev;
	struct device               *dev;
	struct tvin_parm_s          param[2]; //for main & sub port
	struct timer_list           timer;
	struct tvin_frontend_s		frontend;
	unsigned int			irq[4];
	char					irq_name[4][12];
	/* mutex for wwwreg W/R protection */
	struct mutex			rx_lock;
	struct clk *modet_clk;
	struct clk *cfg_clk;
	struct clk *acr_ref_clk;
	struct clk *audmeas_clk;
	struct clk *aud_out_clk;
	struct clk *esm_clk;
	struct clk *skp_clk;
	struct clk *meter_clk;
	struct clk *axi_clk;
	struct clk *cts_hdmirx_5m_clk;
	struct clk *cts_hdmirx_2m_clk;
	struct clk *cts_hdmirx_hdcp2x_eclk;
	const struct meson_hdmirx_data *data;
	struct input_dev *hdmirx_input_dev;
};

#define IOC_SPD_INFO  _BIT(0)
#define IOC_AUD_INFO  _BIT(1)
#define IOC_MPEGS_INFO _BIT(2)
#define IOC_AVI_INFO _BIT(3)
#define ALL_PORTS ((1 << E_PORT_NUM) - 1)

enum colorspace_e {
	E_COLOR_RGB,
	E_COLOR_YUV422,
	E_COLOR_YUV444,
	E_COLOR_YUV420,
};

enum colorrange_rgb_e {
	E_RGB_RANGE_DEFAULT,
	E_RGB_RANGE_LIMIT,
	E_RGB_RANGE_FULL,
};

enum colorrange_ycc_e {
	E_YCC_RANGE_LIMIT,
	E_YCC_RANGE_FULL,
	E_YCC_RANGE_RSVD,
};

enum colordepth_e {
	E_COLORDEPTH_8 = 8,
	E_COLORDEPTH_10 = 10,
	E_COLORDEPTH_12 = 12,
	E_COLORDEPTH_16 = 16,
};

enum port_sts_e {
	E_PORT0,
	E_PORT1,
	E_PORT2,
	E_PORT3,
	E_PORT_NUM,
	E_5V_LOST = 0xfd,
	E_INIT = 0xff,
};

enum data_ch_e {
	E_CH0,
	E_CH1,
	E_CH2,
	E_CH3,
	E_ALL_CH,
};
/* flag: need to request downstream re-auth */
#define HDCP_NEED_REQ_DS_AUTH 0x10
#define HDCP_VER_MASK 0xf

enum hdcp_version_e {
	HDCP_VER_NONE,
	HDCP_VER_14,
	HDCP_VER_22,
};

enum hdmirx_port_e {
	HDMIRX_PORT0,
	HDMIRX_PORT1,
	HDMIRX_PORT2,
	HDMIRX_PORT3,
};

enum map_addr_module_e {
	MAP_ADDR_MODULE_CBUS,
	MAP_ADDR_MODULE_HIU,
	MAP_ADDR_MODULE_HDMIRX_CAPB3,
	MAP_ADDR_MODULE_SEC_AHB,
	MAP_ADDR_MODULE_SEC_AHB2,
	MAP_ADDR_MODULE_APB4,
	MAP_ADDR_MODULE_TOP,
	MAP_ADDR_MODULE_CLK_CTRL,
	MAP_ADDR_MODULE_ANA_CTRL,
	MAP_ADDR_MODULE_COR,
	MAP_ADDR_MODULE_EE_VOLTAGE,
	MAP_ADDR_MODULE_NUM
};

struct rx_var_param {
	int pll_unlock_cnt;
	int pll_unlock_max;
	int pll_lock_cnt;
	int pll_lock_max;
	int dwc_rst_wait_cnt;
	int dwc_rst_wait_cnt_max;
	int sig_stable_cnt;
	int sig_frl_stable_cnt;
	int sig_stable_max;
	int sig_stable_err_cnt;//todo
	int sig_stable_err_max;
	int err_cnt_sum_max;
	int fpll_ready_cnt;
	int frl_lock_det_cnt;
	//bool clk_debug_en;
	u32 hpd_low_cnt;
	u32 hpd_high_cnt;
	/* increase time of hpd low, to avoid some source like */
	/* MTK box/KaiboerH9 i2c communicate error */
	int sig_unstable_cnt;
	int sig_unstable_max;
	bool vic_check_en;
	bool dvi_check_en;
	int sig_unready_cnt;
	int sig_unready_max;
	int fps_unready_cnt;
	int fps_unready_max;
	int diff_pixel_th;
	int diff_line_th;
	int diff_frame_th;
	u32 force_vic;
	u32 err_chk_en;
	int aud_sr_stb_max;
	int log_level;
	bool rx5v_debug_en;
	int clk_unstable_cnt;
	int clk_unstable_max;
	int clk_stable_cnt;
	int clk_stable_max;
	int ch_lock_unstable_cnt;
	int ch_lock_stable_cnt;
	int valid_m_stable_cnt;
	int valid_m_unstable_cnt;
	int valid_m_seen_after_training;
	int unnormal_wait_max;
	int wait_no_sig_max;
	/* No need to judge frame rate while checking timing stable,as there are
	 * some out-spec sources whose framerate change a lot(e.g:59.7~60.16hz).
	 * Other brands of tv can support this,we also need to support.
	 */
	int stable_check_lvl;
	/* If dvd source received the frequent pulses on HPD line,
	 * It will sent a length of dirty audio data sometimes.it's TX's issues.
	 * Compared with other brands TV, delay 1.5S to avoid this noise.
	 */
	int edid_update_delay;
	int skip_frame_cnt;
	u32 hdcp22_reauth_enable;
	unsigned int edid_update_flag;
	unsigned int downstream_hpd_flag;
	u32 hdcp22_stop_auth_enable;
	u32 hdcp22_esm_reset2_enable;
	int sm_pause;
	int pre_port;
	/* waiting time cannot be reduced */
	/* it will cause hdcp1.4 cts fail */
	int hdcp_none_wait_max;
	int esd_phy_rst_cnt;
	int esd_phy_rst_max;
	int frl_rst_cnt;
	int valid_m_rst_cnt;
	int cec_dev_info;
	bool term_flag;
	int clk_chg_cnt;
	int clk_chg_max;
	/* vpp mute when signal change, used
	 * in companion with vlock phase = 84
	 */
	u32 vpp_mute_enable;
	/* mute delay cnt before vpp unmute */
	int mute_cnt;
	u8 dbg_ve;
	/* after DE stable, start DE count */
	bool de_stable;
	u32 check_dsc_de_cnt;
	u32 avi_rcv_cnt;
	bool force_pattern;
	int frl_rate;
	int fpll_stable_cnt;
	int flt_update;
	int lock;
	/*
	 * Usually,for yuv422 input data is 8bit,need force 8bit,
	 * for some specific sources,the data is 10/12bit,
	 * do not need set colordepth override
	 */
	bool special_422_dev;
	u8 over_eotf;
};

struct rx_aml_phy {
	u32 rterm_val;
	bool rterm_flag;
	u32 rterm_dts_lvl;
	u8 rterm_dbg_lvl;
	int dfe_en;
	int ofst_en;
	int cdr_mode;
	int pre_int;
	int pre_int_en;
	int pre_int_21[4];
	int phy_bwth;
	int alirst_en;
	int tap1_byp;
	int eq_byp;
	int long_cable;
	int osc_mode;
	int pll_div;
	bool sqrst_en;
	int vga_dbg;
	int vga_dbg_delay;
	u8 eq_fix_val;
	int cdr_fr_en;
	int vga_tune;
	u32 force_sqo;
	/* add for t5 */
	int os_rate;
	u32 vga_gain;
	u32 eq_stg1;
	u32 eq_stg2;
	u8 eq_pole;
	u32 dfe_hold;
	u32 eq_hold;
	int eye_delay;
	u32 eq_retry;
	u32 tap2_byp;
	u32 long_bist_en;
	int reset_pcs_en;
	/* add for t5m */
	int eq_en;
	int tapx_value;
	int agc_enable;
	u32 afe_value;
	u32 dfe_value;
	u32 cdr_value;
	u32 eq_value;
	u32 misc2_value;
	u32 misc1_value;
	int phy_debug_en;
	int enhance_dfe_en_old;
	int enhance_dfe_en_new;
	int eye_height;
	int enhance_eq;
	int eq_level;
	int cdr_retry_en;
	int cdr_retry_max;
	int cdr_fr_en_auto;
	int hyper_gain_en;
	int eye_height_min;
	int tap0_err_check_en;
	int buf_gain;
	/* bit[3:0]:pll bw,bit[4:7]:phy bw, bit'8 enable bit */
	u32 force_bw;
	int eq_sslms_en;
	/* audio pll source: 0: analog pll,1: digital */
	bool dacr_en;
	/* for t6x 2.1 pll */
	u8 pll_bw_21;
	u8 cdr_ph_div;
	u8 cdr_pi_ofst;
	bool cable_tuning_en;
	int eq_adp_chk_en;
};

struct rx_aml_phy_21 {
	int phy_bwth;
	u32 vga_gain;
	u32 eq_stg1;
	u32 eq_stg2;
	int cdr_fr_en;
	u32 eq_hold;
	u32 eq_retry;
	u32 dfe_en;
	int dfe_hold;
};

enum scan_mode_e {
	E_UNKNOWN_SCAN,
	E_OVERSCAN,
	E_UNDERSCAN,
	E_FUTURE,
};

/**
 * @short HDMI RX controller video parameters
 *
 * For Auxiliary Video InfoFrame (AVI) details see HDMI 1.4a section 8.2.2
 */
struct rx_video_info {
	/** DVI detection status: DVI (true) or HDMI (false) */
	bool hw_dvi;

	u8 hdcp_type;
	/** bit'0:auth start  bit'1:enc state(0:not encrypted 1:encrypted) **/
	u8 hdcp14_state;
	/** 1:decrypted 0:encrypted **/
	u8 hdcp22_state;
	/** Deep color mode: 8, 10, 12 or 16 [bits per pixel] */
	u8 colordepth;
	/** Pixel clock frequency [kHz] */
	u32 pixel_clk;
	/** Refresh rate [0.01Hz] */
	u32 frame_rate;
	/** Interlaced */
	bool interlaced;
	u32 vsync_polarity;
	/** Vertical begin */
	u32 vbegin;
	/** Vertical end */
	u32 vend;
	/** Vertical active */
	u32 vactive;
	/** Vertical total */
	u32 vtotal;
	u32 vblank;
	u32 vfront;
	u32 vback;
	u32 vsync;
	u32 hsync_polarity;
	/** Horizontal offset */
	u32 hoffset;
	/** Horizontal active */
	u32 hactive;
	/** Horizontal total */
	u32 htotal;
	u32 hfront;
	u32 hback;
	u32 hblank;
	u32 hsync;
	/** AVI Y1-0, video format */
	u8 colorspace;
	/** AVI VIC6-0, video identification code */
	enum hdmi_vic_e hw_vic;
	/** AVI PR3-0, pixel repetition factor */
	u8 repeat;
	/* for sw info */
	enum hdmi_vic_e sw_vic;
	u8 sw_dvi;
	unsigned int it_content;
	enum tvin_cn_type_e cn_type;
	/** AVI Q1-0, RGB quantization range */
	unsigned int rgb_quant_range;
	/** AVI Q1-0, YUV quantization range */
	u8 yuv_quant_range;
	u8 active_valid;
	u8 active_ratio;
	u8 bar_valid;
	u8 scan_info;
	u8 colorimetry;
	u8 picture_ratio;
	u8 ext_colorimetry;
	u8 add_ext_colorimetry;
	u8 n_uniform_scale;
	u32 bar_end_top;
	u32 bar_start_bottom;
	u32 bar_end_left;
	u32 bar_start_right;
	bool sw_fp;
	bool sw_alternative;
	/* AVI ver4 byte15 bit[0,5] Resolution Identification*/
	u8 rid;
};

/** Receiver key selection size - 40 bits */
#define HDCP_BKSV_SIZE	(2 *  1)
/** Encrypted keys size - 40 bits x 40 keys */
#define HDCP_KEYS_SIZE	(2 * 40)

/*emp buffer config*/
#define DUMP_MODE_EMP	0
#define DUMP_MODE_TMDS	1
#define TMDS_BUFFER_SIZE	0x2000000 /*32M*/
#define EMP_BUFFER_SIZE		0x800 //2k
#define EMP_BUFF_MAX_PKT_CNT	32
#define TMDS_DATA_BUFFER_SIZE	0x200000

struct rx_edid_auto_mode {
	enum edid_ver_e edid_ver; //cur edid_ver
	enum edid_ver_e cfg; //cfg from ui
	bool need_update;
};

/**
 * @short HDMI RX controller HDCP configuration
 */
struct hdmi_rx_hdcp {
	/*hdcp auth state*/
	enum repeater_state_e state;
	/** Repeater mode else receiver only */
	bool repeat;
	bool hdcp14_ready;
	bool cascade_exceed;
	bool dev_exceed;
	unsigned char ds_hdcp_ver;
	bool stream_manage_rcvd;
	u32 topo_updated;
	u8 rpt_reauth_event;
	/*downstream depth*/
	unsigned char depth;
	/*downstream count*/
	u32 count;
	/** Key description seed */
	u32 seed;
	u32 delay;/*according to the timer,5s*/
	/**
	 * Receiver key selection
	 * @note 0: high order, 1: low order
	 */
	u32 bksv[HDCP_BKSV_SIZE];
	/**
	 * Encrypted keys
	 * @note 0: high order, 1: low order
	 */
	u32 keys[HDCP_KEYS_SIZE];
	enum hdcp_version_e hdcp_version;/* 0 no hdcp;1 hdcp14;2 hdcp22 */
	/* add for dv cts */
	enum hdcp_version_e hdcp_pre_ver;
	u8 stream_type;
	bool hdcp_source;
	unsigned char hdcp22_exception;/*esm exception code,reg addr :0x60*/
	u32 ake_init_cnt;
	u32 reauth_req_cnt;
};

static const unsigned int rx22_ext[] = {
	EXTCON_DISP_HDMI,
	EXTCON_NONE,
};

struct vsi_info_s {
	unsigned int identifier;
	unsigned char vd_fmt;
	unsigned char _3d_structure;
	unsigned char _3d_ext_data;
	bool low_latency;
	bool backlt_md_bit;
	unsigned int eff_tmax_pq;
	bool dv_allm;
	bool hdmi_allm;
	bool hdr_allm;
	bool src_tm_flag;
	bool hdr10plus;
	bool cuva_hdr;
	bool filmmaker;
	bool imax;
	bool qms_plus;
	u8 ccbpc;
	u32 vsi_state; //4K3D/VSI21/HDR10+/DV10/DV15/CUVA/filmmaker/imax/QMS+
	u8 emp_pkt_cnt;
	u8 timeout;
	u8 max_frl_rate;
	u8 sys_start_code;
	u8 cuva_version_code;
	enum hdmirx_packet_status_e pkt_status;
};

//===============emp start
struct vtem_info_s {
	/* gaming-vrr & FVA */
	u8 vrr_en;
	u8 fva_factor_m1;

	/* qms-vrr */
	u8 m_const;
	u8 qms_en;
	u32 next_tfr;

	/* qms+ */
	u8 qms_plus_en;
	u32 ieee;

	u8 base_vfront;
	u16 base_framerate;
};

struct cvtem_info_s {
	bool dsc_flag;
	u8 dsc_info[192];
	u8 dsc_pkt_cnt;
};

struct sbtm_info_s {
	u8 flag;
	u8 sbtm_ver;
	u8 sbtm_mode;
	u8 sbtm_type;
	u8 grdm_min;
	u8 grdm_lum;
	u16 frm_pb_limit_int;
};

struct cuva_emds_s {
	bool flag;
	unsigned char *emds_addr;
	u8 cuva_emds_size;
};

struct dv_info_s {
	u8 dv_addr[1024];
	u8 dv_pkt_cnt;
};

struct emp_dsf_st {
	int pkt_cnt;
	u8 *pkt_addr;
};

struct rx_dsc_rc_range_parameters {
	u8 range_min_qp;
	u8 range_max_qp;
	signed char range_bpg_offset; /* only 6 bit signal variable */
};

struct rx_dsc_rc_parameter_set {
	unsigned int rc_model_size;
	u8 rc_edge_factor;
	u8 rc_quant_incr_limit0;
	u8 rc_quant_incr_limit1;
	u8 rc_tgt_offset_hi;
	u8 rc_tgt_offset_lo;
	u8 rc_buf_thresh[14]; /* config value need note >> 6 */
	struct rx_dsc_rc_range_parameters rc_range_parameters[15];
};

struct rx_dsc_pps_data_s {
	u8 dsc_version_major;
	u8 dsc_version_minor;
	u8 pps_identifier;
	u8 bits_per_component;
	u8 line_buf_depth;
	u8 block_pred_enable;
	u8 convert_rgb;
	u8 simple_422;
	u8 vbr_enable;
	unsigned int bits_per_pixel;
	unsigned int pic_height;
	unsigned int pic_width;
	unsigned int slice_height;
	unsigned int slice_width;
	unsigned int chunk_size;
	unsigned int initial_xmit_delay;
	unsigned int initial_dec_delay;
	u8 initial_scale_value;
	unsigned int scale_increment_interval;
	unsigned int scale_decrement_interval;
	u8 first_line_bpg_offset;
	unsigned int nfl_bpg_offset;
	unsigned int slice_bpg_offset;
	unsigned int initial_offset;
	unsigned int final_offset;
	u8 flatness_min_qp;
	u8 flatness_max_qp;
	struct rx_dsc_rc_parameter_set rc_parameter_set;
	u8 native_420;
	u8 native_422;
	u8 second_line_bpg_offset;
	unsigned int nsl_bpg_offset;
	unsigned int second_line_offset_adj;
	unsigned int hfront;
	unsigned int hsync;
	unsigned int hback;
	unsigned int hc_active_bytes;
};

//================emp end

#define CHANNEL_STATUS_SIZE   24

struct aud_info_s {
    /* info frame*/
	/*
	 *unsigned char cc;
	 *unsigned char ct;
	 *unsigned char ss;
	 *unsigned char sf;
	 */

	int coding_type;
	int channel_count;
	int sample_frequency;
	int sample_size;
	int coding_extension;
	int auds_ch_alloc;
	int auds_layout;

	/*
	 *int down_mix_inhibit;
	 *int level_shift_value;
	 */
	int aud_hbr_rcv;
	int aud_packet_received;
	/* aud mute by gcp_avmute or aud_spflat mute */
	bool aud_mute_en;
	bool afifo_cfg;
	/* channel status */
	unsigned char channel_status[CHANNEL_STATUS_SIZE];
	unsigned char channel_status_bak[CHANNEL_STATUS_SIZE];
	unsigned char ch_sts_bits;
    /**/
	unsigned int cts;
	unsigned int n;
	unsigned int arc;
	/**/
	int real_channel_num;
	int real_sample_size;
	int real_sr;
	u32 aud_clk;
	u8 ch_sts[7];
};

struct phy_sts {
	u32 aud_div;
	u32 pll_rate;
	u32 clk_rate;
	u32 phy_bw;
	u32 pll_bw;
	u32 cablesel;
	ulong timestap;
	u32 err_sum;
	u32 eq_data[256];
	u32 aud_div_1;
};

struct clk_msr {
	u32 cable_clk;
	u32 cable_clk_pre;
	u32 tmds_clk;
	u32 pixel_clk;
	u32 mpll_clk;
	u32 aud_pll;
	u32 adu_div;
	u32 esm_clk;
	u32 p_clk;
	u32 tclk;
	u32 t_clk_pre;
	u32 fpll_clk;
};

struct emp_info_s {
	unsigned int dump_mode;
	void __iomem *hw_addr;
	struct page *pg_addr;
	phys_addr_t p_addr_a;
	phys_addr_t p_addr_b;
	/*void __iomem *v_addr_a;*/
	/*void __iomem *v_addr_b;*/
	void __iomem *store_a;
	void __iomem *store_b;
	void __iomem *ready;
	unsigned long irq_cnt;
	unsigned int emp_pkt_cnt;
	unsigned int pre_emp_pkt_cnt;
	unsigned int tmds_pkt_cnt;
	bool end;
	u8 ogi_id;
	unsigned int emp_tagid;
	u8 emp_content_type;
	u8 data[128];
	u8 data_ver;
};

enum i2c_sample_mode_e {
	E_FUNC_SAMPLE,
	E_I2C_WAVE_SAMPLE,
	E_CEC_WAVE_SAMPLE
};

struct i2c_info_s {
	phys_addr_t phy_addr;
	struct page *pg_addr;
	enum i2c_sample_mode_e mode;
	u32 addr_base;
};

struct dump_params {
	int dump_type;
	unsigned int dump_frame;
};

struct buf_item_s {
	u32 aud_pkt_cnt;
	u32 total_pkt;
	u8 buf[2048][32];
}  __packed;

struct aud_to_emp_s {
	void *buf_a;
	void *buf_b;
	struct page *aud_pg;
	phys_addr_t aud_emp_a;
	phys_addr_t aud_emp_b;
	u8 flag;
	unsigned long irq_cnt;
	int dump_work_cnt;
	struct buf_item_s *buf;
	int buf_count;
};

struct spkts_rcvd_sts {
	u32 pkt_vsi_rcvd:1;
	u32 pkt_drm_rcvd:1;
	u32 pkt_spd_rcvd:1;
	u32 pkt_vtem_rcvd:1;
	u32 rsvd:28;
};

struct edid_capacity {
	u32 vrr:1;
	u32 allm:1;
	u32 qms:1;
	u32 hf_db:1;
	u32 vsdv_db:1;
	u32 hdr10p_db:1;
	u32 hdr_static_db:1;
	u32 hdr_dynamic_db:1;
	u32 freesync_db:1;
	u32 timing_4k:1;
	u32 rsvd:22;
};

enum e_colorimetry {
	E_NULL = 0,
	E_SMPTE_ST_170,
	E_BT_709,
	E_EXTENDED_VALID = 3,
	E_XVYCC_601 = 3,
	E_XVYCC_709,
	E_SYCC_601,
	E_OPYCC_601,
	E_OP_RGB,
	E_BT_2020_YCC,
	E_BI_2020_RGBORYCC,
	E_SMPTE_ST_2113_P3D65RGB = 10,
	E_SMPTE_ST_2113_P3DCIRGB,
	E_BT_2100,
};

enum hdmirx_event {
	HDMIRX_NONE_EVENT = 0,
};

enum pre_load_cfg_e {
	PRE_LOAD_DISABLE = 0,
	PRE_LOAD_ENABLE = 0x10,
};

struct pre_load_info_s {
	u32 cfg; //bit[3:0] port_sel; bit[4]1- enable, 0: disabled
	u8 usr_sel_port;
	u8 cur_load_port;
};

#define MAX_UEVENT_LEN 64
struct hdmirx_uevent {
	const enum hdmirx_event type;
	int state;
	const char *env;
};

enum hpd_control_e {
	CEC_CNTL_HPD = 1,
	AUD_CNTL_HPD = 2,
	EDID_CNTL_HPD = 4,
};

#define EDID_UPDATE_READY (AUD_CNTL_HPD | EDID_CNTL_HPD | CEC_CNTL_HPD)

int hdmirx_set_uevent(enum hdmirx_event type, int val);

struct rx_info_s {
	struct hdmirx_dev_s *hdmirx_dev;
	enum chip_id_e chip_id;
	enum phy_ver_e phy_ver;
	u8 port_num;
	u8 main_port;
	u8 sub_port;
	struct pre_load_info_s pre_load;
	u8 vp_cor0_port;
	u8 vp_cor1_port;
	bool boot_flag;
	bool main_port_open;
	bool pip_on;
	bool hdr10p_en;
	u8 vrr_min;
	u32 vrr_max;
	u32 arc_port;
	bool arc_5vsts;
	unsigned long timestamp;
	struct rx_aml_phy aml_phy;
	struct rx_aml_phy aml_phy_21;
	struct emp_info_s emp_buff_a; //for vid0
	struct emp_info_s emp_buff_b; //for vid1
	struct i2c_info_s i2c_buff;
	bool suspend_flag;
	//u8 edid_update_done;
	bool timer_flag;
	/* bit[3:0]:HDMI4/3/2/1 */
	u32 disable_port_num;
	u32 aud_sck_clk;
	u32 acr_rst_delay;
#ifdef CONFIG_RX_AUDIO_TO_EMP
	struct aud_to_emp_s aud_emp;
#endif
};

struct rx_s {
	/** HDMI RX received signal changed */
	u32 skip;
	/*avmute*/
	u32 avmute_skip;
	bool vpp_mute;
	bool cableclk_stb_flg;
	bool ch_lock_stb_flg;
	bool valid_m_stb_flg;
	u8 irq_flag;
	/** HDMI RX controller HDCP configuration */
	struct hdmi_rx_hdcp hdcp;
	/*report hpd status to app*/
	/* wrapper */
	unsigned int state;
	unsigned int fsm_ext_state;
	unsigned int pre_state;
	/* recovery mode */
	unsigned char err_rec_mode;
	unsigned char pre_5v_sts;
	unsigned char cur_5v_sts;
	bool no_signal;
	unsigned char err_cnt;
	u16 wait_no_sig_cnt;
	int aud_sr_stable_cnt;
	int aud_sr_unstable_cnt;
	u32 last_sw_vic;
	bool min_time_en;
	bool min_time_detect_done;
	unsigned long stable_timestamp;
	unsigned long unready_timestamp;
	/* info */
	struct rx_video_info pre;
	struct rx_video_info cur;
	struct aud_info_s aud_info;
	struct vsi_info_s vs_info_details;
	struct tvin_3d_meta_data_s threed_info;
	struct tvin_hdr_info_s hdr_info;
	u8 emp_type;
	enum hdmirx_packet_status_e spd_pkt_st;
	enum hdmirx_packet_status_e avi_pkt_st;
	struct vtem_info_s vtem_info;
	struct sbtm_info_s sbtm_info;
	struct cuva_emds_s emp_cuva_info;
	struct rx_dsc_pps_data_s dsc_pps_data;
	bool vsif_fmm_flag;
	bool avi_fmm_flag;
	u8 qms_plus_flag;
	u8 amdv_type;
	struct dv_info_s emp_dv_info;
	struct cvtem_info_s cvtem_info;
	u8 emp_vid_idx;
	struct emp_info_s *emp_info;
	u8 emp_dsf_cnt;
	bool emp_pkt_rev;
	bool new_emp_pkt;
	struct emp_dsf_st emp_dsf_info[EMP_DSF_CNT_MAX];
	unsigned char edid_mix_buf[EDID_MIX_MAX_SIZE];
	bool pkt_need_mute;
	unsigned int pwr_sts;
	/* for debug */
	/*struct pd_infoframe_s dbg_info;*/
	struct phy_sts phy;
	struct clk_msr clk;
	enum edid_ver_e edid_ver;
	u16 edid_size;
	bool sup_frl;
	u8 tx_type;
	bool arc_5vsts;
	u32 vsync_cnt;
	bool vrr_en;
	u8 free_sync_sts;
	u8 afifo_sts;
	u8 vpp_mute_cnt;
	u8 gcp_mute_cnt;
	u32 ecc_err;
	u32 ecc_pkt_cnt;
	u32 ecc_err_frames_cnt;
	u32 hdcp_reauth_cnt;
	bool ddc_filter_en;
	struct rx_var_param var;
	u8 last_hdcp22_state;
	struct rx_aml_phy aml_phy;
	struct rx_aml_phy aml_phy_21;
	bool dsc_flag;
	//struct spkts_rcvd_sts pkts_sts;
	struct rx_edid_auto_mode edid_type;
	bool spec_vendor_id;
	u32 irq_err_cnt;
	u32 de_err_cnt;
	int pkt_mini_interval[PACKET_TYPE_MAX]; //unit:frame
	int dump_aud_cnt;
	u32 phy_bw_pre;
	u32 pll_bw_pre;
	struct hdmirx_phy_status_s phy_sts;
	struct edid_capacity edid_cap;
	u32 frl_check;
	u32 frl_double_check;
	u32 frl_pix_cnt;
	u8 frl_fdet_check_step;
	u32 rx_sig_type;
	u32 rx_sig_type_pre;
	bool is_htotal_odd;
	u32 odd_htotal_max;
	u32 odd_htotal_min;
	u32 odd_cnt;
	u32 odd1_cnt;
	bool need_add_one;
	bool game_mode;
	bool irq_all_en;
	bool is_cta_signal;
	u8 hpd_cntl;
};

struct reg_map {
	unsigned int phy_addr;
	unsigned int size;
	void __iomem *p;
	int flag;
};

struct phy_port_data {
	int port_idx;
	struct work_struct work;
	struct workqueue_struct *aml_phy_wq;
};

struct work_data {
	struct work_struct work_wq;
	u8 port;
};

struct edid_delayed_work_data {
	struct delayed_work delayed_work;
	u8 port;
	unsigned int state[E_PORT_NUM];
	u32 edid_offset_cur[E_PORT_NUM];
};

struct edid_timer_data {
	struct hrtimer timer;
	u8 port;
	unsigned int state[E_PORT_NUM];
	u32 edid_offset_cur[E_PORT_NUM];
};

#define WHITE_LIST_SIZE 25
enum spec_dev_e {
	/* following devices need to switch to edid2.0 */
	SPEC_DEV_PS5,
	SPEC_DEV_XBOX,
	SPEC_DEV_PS,
	SPEC_DEV_XBOX_SERIES,
	/* following devices need to get SPD earlier */
	SPEC_DEV_PANASONIC,
	SPEC_DEV_CNT
};

enum spec_dev_type_e {
	DEV_UNKNOWN = 0x0,
	DEV_HDMI20 = 0x1,
	SPD_GET_EARLIER = 0x2,
	DEV_HDMI14 = 0x4,
	DEV_ABNORMAL_SCDC = 0x8,
	DEV_HDMI14_UNKNOWN_PORT = 0x10,
};

struct spec_dev_table_s {
	enum spec_dev_type_e dev_type;
	u8 spd_info[WHITE_LIST_SIZE];
};

struct edid_update_work_s {
	struct work_struct work;
	u8 port;
};

/* system */
extern struct delayed_work	eq_dwork;
extern struct workqueue_struct	*eq_wq;
extern struct work_data     scdc_dwork;
extern struct workqueue_struct *scdc_wq;
extern struct work_data print_dwork;
extern struct workqueue_struct	*print_wq;
extern struct delayed_work	esm_dwork;
extern struct workqueue_struct	*esm_wq;
extern struct delayed_work	repeater_dwork;
//extern struct work_struct	aml_phy_dwork;
//extern struct workqueue_struct	*aml_phy_wq;
extern struct work_struct     aml_earc_dwork;
extern struct workqueue_struct *aml_earc_wq;
extern struct work_struct	aml_phy_dwork_port0;
extern struct workqueue_struct	*aml_phy_wq_port0;
extern struct work_struct	aml_phy_dwork_port1;
extern struct workqueue_struct	*aml_phy_wq_port1;
extern struct work_struct	aml_phy_dwork_port2;
extern struct workqueue_struct	*aml_phy_wq_port2;
extern struct work_struct	aml_phy_dwork_port3;
extern struct workqueue_struct	*aml_phy_wq_port3;
extern struct work_struct     clkmsr_dwork;
extern struct workqueue_struct *clkmsr_wq;
extern struct edid_delayed_work_data edid_reset_work;
extern struct work_struct     earc_hpd_dwork;
extern struct workqueue_struct *earc_hpd_wq;
extern struct work_struct     vpu_dwork;
extern struct workqueue_struct *vpu_wq;

extern struct workqueue_struct	*repeater_wq;
extern struct edid_update_work_s edid_update_dwork;
extern struct workqueue_struct *edid_update_wq;

extern struct kthread_worker frl_worker;
extern struct task_struct *frl_worker_task;
extern struct kthread_work frl_work;

extern struct kthread_worker frl1_worker;
extern struct task_struct *frl1_worker_task;
extern struct kthread_work frl1_work;

extern struct kthread_worker phy_ofset_worker;
extern struct task_struct *phy_ofset_worker_task;
extern struct kthread_work phy_ofset_work;

extern wait_queue_head_t tx_wait_queue;

extern struct tasklet_struct rx_tasklet;
extern struct device *hdmirx_dev;
extern struct rx_s rx[4];
extern struct rx_info_s rx_info;
extern char boot_info[BOOT_INFO_NUM][BOOT_INFO_LEN];
extern int boot_info_num;
//extern struct phy_port_data aml_phy_dwork;
//extern u8 port_idx;

extern struct tvin_latency_s latency_info;
extern struct reg_map rx_reg_maps[MAP_ADDR_MODULE_NUM];
extern bool downstream_repeat_support;

void rx_tasklet_handler(unsigned long arg);
void skip_frame(unsigned int cnt, u8 port, char *str);

/* reg */

/* packets */
extern unsigned int packet_fifo_cfg;
extern unsigned int *pd_fifo_buf_a;
extern unsigned int *pd_fifo_buf_b;

/* hotplug */
extern unsigned int pwr_sts;
extern int pre_port;
void hotplug_wait_query(void);
void rx_send_hpd_pulse(u8 port);

/* irq */
void rx_irq_en(u8 enable, u8 port);
irqreturn_t irq_handler(int irq, void *params);
irqreturn_t irq0_handler(int irq, void *params);
irqreturn_t irq1_handler(int irq, void *params);
irqreturn_t irq2_handler(int irq, void *params);
irqreturn_t irq3_handler(int irq, void *params);

void cecb_irq_handle(void);

/* user interface */
//extern int it_content;
//extern int rgb_quant_range;
//extern int yuv_quant_range;
extern u32 en_4k_timing;
extern int cec_dev_en;
extern bool dev_is_apple_tv_v2;
extern u32 en_4096_2_3840;
extern u32 en_4k_2_2k;
extern u32 ops_port;
extern int hdmi_cec_en;
extern int vdin_drop_frame_cnt;
extern u32 rpt_edid_selection;
extern u32 rpt_only_mode;
extern u32 vrr_func_en;
extern u32 allm_func_en;
extern u32 qms_func_en;
extern u32 hpd_low_cec_off;
/* debug */
extern bool hdcp_enable;
extern int log_level;
extern int sm_pause;
extern int suspend_pddq_sel;
extern bool video_stable_to_esm;
extern u32 pwr_sts_to_esm;
extern bool enable_hdcp22_esm_log;
extern bool esm_reset_flag;
extern bool esm_auth_fail_en;
extern bool esm_error_flag;
extern bool hdcp22_stop_auth;
extern bool hdcp22_esm_reset2;
extern int esm_recovery_mode;
extern u32 dbg_pkt;
extern u32 disable_hdr;
extern int rx_phy_level;
extern int vdin_reset_pcs_en;
extern u32 rx_5v_wake_up_en;
extern char edid_cur[];
extern int vpp_mute_cnt;
extern int gcp_mute_cnt;
extern int gcp_mute_flag[4];
extern int def_trim_value;
extern u32 edid_auto_sel;
extern int audio_dump_frame;
extern struct edid_timer_data edid_timer;
extern ktime_t interval;
extern int ee_voltage_val;
extern int ee_voltage_en;
extern int force_dsc_4ppc;
extern int over_clk_flag;
extern int dsc_update_en;
#ifdef CONFIG_AMLOGIC_MEDIA_VRR
extern struct notifier_block vrr_notify;
#endif
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
extern bool early_suspend_flag;
#endif
#ifdef CONFIG_AMLOGIC_HDMITX
extern struct notifier_block tx_notify;
void hdmitx_update_latency_info(struct tvin_latency_s *latency_info);
#endif
void __weak hdmitx_update_latency_info(struct tvin_latency_s *latency_info)
{
}

u8 rx_get_port_type(u8 port);
bool rx_is_pip_on(void);
int rx_set_global_variable(const char *buf, int size);
void rx_get_global_variable(const char *buf);
int rx_sprintf(int *index, const char *format, ...);
int rx_pr(const char *fmt, ...);
unsigned int hdmirx_hw_dump_reg(unsigned char *buf, int size);
unsigned int hdmirx_show_info(unsigned char *buf, int size, u8 port);
unsigned int hdmirx_show_info_t3x(unsigned char *buf, int size);
bool is_aud_fifo_error(void);
bool is_aud_pll_error_20(void);
bool is_aud_pll_error_21(void);

int hdmirx_debug(const char *buf, int size);
void dump_reg(u8 port);
void dump_edid_reg(u32 size);
void rx_debug_loadkey(u8 port);
void rx_debug_load22key(u8 port);
int rx_debug_wr_reg(const char *buf, char *tmpbuf, int i, u8 port);
int rx_debug_rd_reg(const char *buf, char *tmpbuf, u8 port);
int aml_vrr_atomic_notifier_register(struct notifier_block *nb);
/* repeater */
bool hdmirx_repeat_support(void);

/* edid-hdcp14 */
extern unsigned int downstream_hpd_flag;
void hdmirx_fill_edid_buf(const char *buf, int size);
void hdmirx_fill_edid_with_port_buf(const char *buf, int size);
void hdmirx_fill_key_buf(const char *buf, int size);
extern int rx_audio_block_len;
extern u8 rx_audio_block[MAX_AUDIO_BLK_LEN];

/* for other modules */
void rx_is_hdcp22_support(void);
int rx_hdcp22_send_uevent(int val);

/* for cec set tx_type */
void register_cec_rx_notify(cec_spd_callback callback);
void rx_alloc_aud_ddr(u8 port);
void rx_free_aud_ddr(u8 port);
unsigned int first_bit_set(u32 data);

//#define RX_VER0 "ver.2021/06/21"
//1. added colorspace detection
//2. add afifo detection
//3. use top de-repeat

//#define RX_VER0 "ver.2021/07/06"
//waiting i2c idle before pull dow HPD

// fix vsif irq issue
//#define RX_VER0 "ver.2021/07/13"

#endif
