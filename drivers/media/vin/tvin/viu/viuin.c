// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/module.h>

#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/frame_sync/ptsserv.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h>

#include "../tvin_global.h"
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#define VIU_DEVICE_NAME "viuin"
#define VIU_CLS_NAME	"viuin"

#define MODULE_NAME "viuin"

#define AMVIUIN_DEC_START       1
#define AMVIUIN_DEC_STOP        0

static dev_t viu_devno;
static struct class *viu_class;

/* register */
#define VIU_MISC_CTRL1 0x1a07
#define VPU_VIU_VENC_MUX_CTRL 0x271a
#define ENCI_INFO_READ 0x271c
#define ENCP_INFO_READ 0x271d
#define ENCT_INFO_READ 0x271e
#define ENCL_INFO_READ 0x271f
#define VPU_VIU2VDIN_HDN_CTRL 0x2780
/* VIU2VDIN_HDN (0x2780) Register Bitfield Definition
 * Bit(s)   | R/W | Default | Description
 * 13:0     | RO  | 0x0     | hsize
 * 17:16    | RO  | 0x0     | flt_mode //0: no filter
 * 19:18    | RO  | 0x0     | dn_ratio //0->1:1; 1->1:2; 2->1:4
 * 20       | RO  | 0x0     | sw_reset
 * 23       | RO  | 0x0     | hs_pol
 * 24       | RO  | 0x0     | vskip_en
 * 25       | RO  | 0x0     | vskip_odd
 * 27:26    | RO  | 0x0     | venc_dn //0: 1/1 1: 1/2 downsample 2: 1/4 downsample
 * 28       | RO  | 0x0     | dout_seq //output seq 0: little endian 1: big endian
 * 29       | RO  | 0x0     | din_seq  //input seq  0: little endian 1: big endian
 * 31:30    | RO  | 0x0     | din_mode //input 0: 1pixel 1: 2pixel 2: 4pixel
 * 22:21    | RO  | 0x0     | dout_mode//output 0: 1pixel 1: 2pixel 2: 4pixel
 */

/* t7 enable hsync reg */
#define VIU_FRM_CTRL   0x1a51
#define VIU1_FRM_CTRL   0x1a8d
#define VIU2_FRM_CTRL   0x1a8e

/* ENCL/ENCT polarity, positive:begin > end, negative: begin < end */
#define ENCL_VIDEO_VSO_BLINE	0x1cb9
#define ENCL_VIDEO_VSO_ELINE	0x1cba

/* ENCP polarity, positive:begin > end, negative: begin < end */
#define ENCP_VIDEO_VSO_BLINE	0x1bab
#define ENCP_VIDEO_VSO_ELINE	0x1bac

/* ENCI polarity, always negative, no need control dynamically */

/*g12a new add*/
#define VPU_VIU_ASYNC_MASK 0x2781
#define VDIN_MISC_CTRL 0x2782
#define VPU_VIU_VDIN_IF_MUX_CTRL 0x2783
/* VPU_VIU_VDIN_IF_MUX_CTRL (0x2783) Register Bitfield Definition
 * Bit(s)   | R/W | Default | Description
 * 25:24    | R/W | 0x0     | vdin1_buf_gclk_ctrl
 * 23       | R/W | 0x0     | vdin_sw_reset
 * 22       | R/W | 0x0     | vdin_vs_pol
 * 21       | R/W | 0x0     | vdin_downsample_buffer_overflow_clear
 * 15       | RO  | 0x0     | vdin1_downsample_buffer_overflow_status
 * 14:8     | R/W | 0x0     | vdin1_sel_data_wrbak_src_data
 * 6:0      | R/W | 0x0     | vdin1_sel_clk_wrbak_src_clock
 */

#define VPU_VIU2VDIN1_HDN_CTRL 0x2784
#define VPU_VENCX_CLK_CTRL 0x2785
#define VPP_WR_BAK_CTRL 0x1df9
/* VPP_WRBAK_CTRL (0x1df9) Register Bitfield Definition
 * Bit(s) | R/W | Default | Description
 * 30     | R/W | 0x0     | postbld_dout_wrbak_enable
 * 29     | R/W | 0x1     | postbld_dout_display_enable
 * 28     | R/W | 0x1     | vpp_dout_display_enable
 * 27:16  | R/W | 0x0     | rbak_din_hblank_num
 * 13     | R/W | 0x0     | postbld_vd3_only_wrbak_enable
 * 12     | R/W | 0x0     | prebld_vd1_wrbak_enable
 * 11     | R/W | 0x0     | vpp_dout_wrbak_enable
 * 10     | R/W | 0x0     | postbld_osd2_wrbak_enable
 * 9      | R/W | 0x0     | postbld_osd1_wrbak_enable
 * 8      | R/W | 0x0     | postbld_vd2_wrbak_enable
 * 7:4    | R/W | 0x0     | wrbak1_channel_select_source
 * 3:0    | R/W | 0x0     | wrbak0_channel_select_source
 */

#define VPP1_WR_BAK_CTRL 0x5981
#define T6D_VPP1_WR_BAK_CTRL 0x6481

#define VPP2_WR_BAK_CTRL 0x59c1
#define VPU_422TO444_CTRL0 0x274b
#define VPU_422TO444_CTRL1 0x274c
#define VPU_422TO444_CTRL2 0x274d
#define WR_BACK_MISC_CTRL 0x1a0d

/* s5 */
#define VIU_WR_BAK_CTRL                             0x1a25
#define S5_VPP_POST_HOLD_CTRL                       0x1d1f
/* t3x */
#define VPP_POST_HOLD_CTRL_T3X		0x1d19
#define VPU_VIU_VDIN_IF_MUX_CTRL_T3X	0x2783
#define VPU_VIU2VDIN0_BUF_SIZE_T3X	0x2784
#define VDIN_LITE_CORE_MAX_PIXEL_CLOCK	(4096 * 2160 * 60UL)
/* txhd2 for keystone dump vdin1 */
#define VPP_MISC									0x1d26
/* t6x new add*/
#define VPU_VIU2VDIN0_BUF_SIZE	0x2737
#define VPP_WRBAK_HOLD_CTRL	0x1df4 /* reduction speed */
/* VPP_WRBAK_HOLD_CTRL (0x1df4) Register Bitfield Definition
 * Bit(s)   | R/W | Default | Description
 * 31:0     | W   | 0x0     | wrbak_hold_ctrl
 * 17       | W   | 0x0     | hold_en_postbld_dout
 * 16       | W   | 0x0     | hold_en_vpp_dout
 * 15:8     | W   | 0x0     | pass_num_aft_postbld
 * 7:0      | W   | 0x0     | hold_num_aft_postbld
 */

#define VPU_VIU2VDIN0_HALF_CTRL	0x272d /* vpp >4k2k 60 cutwin */
#define VPP_PATH_CTRL		0x1d0f
/* VPP_PATH_CTRL (0x1d0f) Register Bitfield Definition
 * Bit(s)   | R/W | Default | Description
 * 31:24    | W   | 0x0     | pass_num_bef_postbld
 * 23:16    | W   | 0x0     | hold_num_bef_postbld
 * 11       | W   | 0x0     | hold_en_postbld_osd2
 * 10       | W   | 0x0     | hold_en_postbld_osd1
 * 9        | W   | 0x0     | hold_en_postbld_vd2
 * 8        | W   | 0x0     | hold_en_vd1_swop
 * 3        | W   | 0x1     | reg_osd2_disp_en
 * 2        | W   | 0x1     | reg_osd1_disp_en
 * 1        | W   | 0x1     | reg_vd2_disp_en
 * 0        | W   | 0x1     | reg_vd1_swop_disp_en
 */

static unsigned int vsync_enter_line_curr;
static unsigned int vsync_enter_line_max;
static unsigned int vsync_enter_line_max_threshold = 10000;
static unsigned int vsync_enter_line_min_threshold = 10000;
static unsigned int vsync_enter_line_threshold_overflow_count;
static unsigned short open_cnt;

static unsigned int viu_dbg_en;

struct viuin_s {
	unsigned int flag;
	struct device *dev;
	struct device_node *node;
	int index;
	struct vframe_prop_s *prop;
	/*add for tvin frontend*/
	struct tvin_frontend_s frontend;
	struct vdin_parm_s parm;
	unsigned int enc_info_addr;
};

static inline u32 rd_viu(u32 reg)
{
	return (u32)aml_read_vcbus(reg);
}

static inline void wr_viu(u32 reg,
			  const u32 val)
{
	aml_write_vcbus(reg, val);
}

static inline void wr_bits_viu(u32 reg,
			       const u32 value,
			       const u32 start,
			       const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			      ~(((1L << (len)) - 1) << (start))) |
			      (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 rd_bits_viu(u32 reg,
			      const u32 start,
			      const u32 len)
{
	u32 val;

	val = ((aml_read_vcbus(reg) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static int viuin_support(struct tvin_frontend_s *fe, enum tvin_port_e port)
{
	if ((port >= TVIN_PORT_VIU1 && port < TVIN_PORT_VIU1_MAX) ||
	    (port >= TVIN_PORT_VIU2 && port < TVIN_PORT_VIU2_MAX) ||
	    (port >= TVIN_PORT_VIU3 && port < TVIN_PORT_VIU3_MAX) ||
	    (port >= TVIN_PORT_VENC && port < TVIN_PORT_VENC_MAX) ||
	    port == TVIN_PORT_MIPI || port == TVIN_PORT_ISP)
		return 0;
	else
		return -1;
}

void viuin_check_venc_line(struct viuin_s *devp_local)
{
	unsigned int vencv_line_cur, cnt;

	cnt = 0;
	do {
		vencv_line_cur =
			(rd_viu(devp_local->enc_info_addr) >> 16) & 0x1fff;
		udelay(9);
		cnt++;
		if (cnt > 100000)
			break;
	} while (vencv_line_cur != 1);

	if (vencv_line_cur != 1)
		pr_info("**************%s,vencv_line_cur:%d,cnt:%d***********\n",
			__func__, vencv_line_cur, cnt);
}

void viuin_select_loopback_path(void)
{
	const struct vinfo_s *vinfo = NULL;

	vinfo = get_current_vinfo();
	unsigned long vinfo_total_size;

	/* set VPU_VIU_VDIN_IF_MUX_CTRL */
	wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL_T3X, 0x8080808);
	/* sel TVIN_PORT_VIU1_WB0_VPP */
	wr_bits_viu(VPP_WR_BAK_CTRL, 6, 0, 4);
	wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
	if (is_meson_t6x_cpu()) {
		wr_bits_viu(VPP_WR_BAK_CTRL, 1, 11, 1);
		if (vinfo) {
			vinfo_total_size = (unsigned long)vinfo->width
						* vinfo->height
						* vinfo->std_duration;
			if (vinfo_total_size > VDIN_LITE_CORE_MAX_PIXEL_CLOCK) {
				wr_viu(VPU_VIU2VDIN0_BUF_SIZE,
					(vinfo->height << 16) | ((vinfo->width / 2) << 0));
				wr_viu(VPU_VIU2VDIN_HDN_CTRL,
					(0x4420 << 16) | ((vinfo->width / 2) << 0));
			} else {
				wr_viu(VPU_VIU2VDIN0_BUF_SIZE,
					(vinfo->height << 16) | (vinfo->width << 0));
			}
		} else {
			pr_err("%s() vinfo is null!!", __func__);
		}
	}
	wr_bits_viu(WR_BACK_MISC_CTRL, 1, 0, 1);/*vd0 hsync*/
	wr_bits_viu(WR_BACK_MISC_CTRL, 0, 1, 1);/*vd1 hsync*/
	/* enable write back hsync */
	wr_bits_viu(VIU_FRM_CTRL, 3, 24, 2);
}
EXPORT_SYMBOL(viuin_select_loopback_path);

void viuin_clear_loopback_path(void)
{
	wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_t6x_cpu()) {
		wr_bits_viu(VPP_WR_BAK_CTRL, 0, 0, 23);
		wr_bits_viu(VPP_WR_BAK_CTRL, 0, 30, 1);
	} else {
		wr_viu(VPP_WR_BAK_CTRL, 0);
	}
#else
	 wr_viu(VPP_WR_BAK_CTRL, 0);
#endif
}
EXPORT_SYMBOL(viuin_clear_loopback_path);

static void viuin_set_vdin_if_mux_ctrl(enum tvin_port_e port)
{
	unsigned int viu_mux = 0;
	unsigned int viu_sel = 0;

	switch (port) {
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_VIDEO:
	case TVIN_PORT_VIU1_WB0_VD1:
	case TVIN_PORT_VIU1_WB0_VD2:
	case TVIN_PORT_VIU1_WB0_OSD1:
	case TVIN_PORT_VIU1_WB0_OSD2:
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
	case TVIN_PORT_VIU1_WB0_VPP:
		viu_mux = VIU_MUX_SEL_WB0;
		break;
	case TVIN_PORT_VIU1_WB1_VDIN_BIST:
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_WB1_VIDEO:
	case TVIN_PORT_VIU1_WB1_VD1:
	case TVIN_PORT_VIU1_WB1_VD2:
	case TVIN_PORT_VIU1_WB1_OSD1:
	case TVIN_PORT_VIU1_WB1_OSD2:
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_VPP:
		viu_mux = VIU_MUX_SEL_WB1;
		break;
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VENC1:
		viu_mux = VIU_MUX_SEL_ENCL;
		break;
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VENC2:
		viu_mux = VIU_MUX_SEL_ENCI;
		break;
	case TVIN_PORT_VIU2_ENCP:
	case TVIN_PORT_VENC0:
		viu_mux = VIU_MUX_SEL_ENCP;
		break;
	case TVIN_PORT_VIU2_VD1:
	case TVIN_PORT_VIU2_OSD1:
	case TVIN_PORT_VIU2_VPP:
		viu_mux = VIU_MUX_SEL_WB2;
		break;
	case TVIN_PORT_VIU3_VD1:
	case TVIN_PORT_VIU3_OSD1:
	case TVIN_PORT_VIU3_VPP:
		viu_mux = VIU_MUX_SEL_WB3;
		break;
	default:
		viu_mux = 0;
		break;
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		viu_sel = 1;
	} else {
		/*old chip*/
		switch (port & ~0xff) {
		case TVIN_PORT_VIU1:
			viu_sel = 1;
			break;
		case TVIN_PORT_VIU2:
			viu_sel = 2;
			break;
		default:
			break;
		}
	}

	if (viu_sel == 1) {
		if (is_meson_t3x_cpu()) {
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL_T3X, viu_mux, 0, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL_T3X, viu_mux, 8, 7);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7)) {
			/* for vdi6 , vdin source 7*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 0, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 0, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 8, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 8, 7);
			/* for vdi8 , vdin source 9*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 16, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 16, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 24, 7);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 24, 7);
		} else {
			/* for vdi6 , vdin source 7*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 0, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 0, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 8, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 8, 5);
			/* for vdi8 , vdin source 9*/
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 16, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 24, 5);
			wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 24, 5);
		}
	} else if (viu_sel == 2) {
		/* for vdi6 , vdin source 7*/
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 0, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 0, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 8, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 8, 5);
		/* for vdi8 , vdin source 9*/
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 16, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 16, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0, 24, 5);
		wr_bits_viu(VPU_VIU_VDIN_IF_MUX_CTRL, viu_mux, 24, 5);
	} else {
		wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void viuin_set_wr_bak_ctrl_s5(enum tvin_port_e port)
{
	const struct vinfo_s *vinfo = NULL;

	vinfo = get_current_vinfo();
	if (!vinfo)
		pr_info("%s vinfo == NULL\n", __func__);
	else
		pr_info("cur_enc_ppc = %d\n", vinfo->cur_enc_ppc);

	/* wr_bak use 4ppc always on S5 */
	wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 3, 21, 2);
	wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 30, 2);

	switch (port) {
	/* wr_bak_chan1_sel wb_chan_sel*/
	//wr_bak_chan0_sel : 0:vd1 1:vd2 2:vd3 3:osd1 4:osd2 5:post_blend_out;
	case TVIN_PORT_VIU1_WB0_VD1:
		wr_bits_viu(VIU_WR_BAK_CTRL, 0, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VD2:
		wr_bits_viu(VIU_WR_BAK_CTRL, 1, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD1:
		wr_bits_viu(VIU_WR_BAK_CTRL, 3, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD2:
		wr_bits_viu(VIU_WR_BAK_CTRL, 4, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VPP://s5 no vppout
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
		wr_bits_viu(VIU_WR_BAK_CTRL, 5, 0, 4);
		if (vinfo && vinfo->cur_enc_ppc == 4) { //4 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 0, 26, 2);
			//wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 30, 2);
		} else if (vinfo && vinfo->cur_enc_ppc == 2) { //2 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 26, 2);
			//wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 30, 2);
		} else { //1 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 0, 26, 2);
			//wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 30, 2);
		}
		break;
	case TVIN_PORT_VIU1_VIDEO:
		wr_bits_viu(VIU_WR_BAK_CTRL, 7, 0, 4);
		break;
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_WB1_VD1:
	case TVIN_PORT_VIU1_WB1_VD2:
	case TVIN_PORT_VIU1_WB1_OSD1:
	case TVIN_PORT_VIU1_WB1_OSD2:
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_VPP:
	case TVIN_PORT_VIU1_WB1_VIDEO:
		break;
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VIU2_ENCP:
		wr_bits_viu(VIU_WR_BAK_CTRL, 6, 0, 4);

		/* increase h banking in case vdin afifo overflow
		 * pre chip has 8bits
		 * tm2_revb increased 4bits, all 12bit
		 */
		wr_bits_viu(VIU_WR_BAK_CTRL, 0xff, 16, 8);
		break;
	case TVIN_PORT_VIU2_VD1:
		wr_bits_viu(VPP1_WR_BAK_CTRL, 1, 0, 2);
		break;
	case TVIN_PORT_VIU2_OSD1:
		wr_bits_viu(VPP1_WR_BAK_CTRL, 2, 0, 2);
		break;
	case TVIN_PORT_VIU2_VPP:
		wr_bits_viu(VPP1_WR_BAK_CTRL, 3, 0, 2);
		break;
	case TVIN_PORT_VIU3_VD1:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 1, 0, 2);
		break;
	case TVIN_PORT_VIU3_OSD1:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 2, 0, 2);
		break;
	case TVIN_PORT_VIU3_VPP:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 3, 0, 2);
		break;
	case TVIN_PORT_VENC0:
	case TVIN_PORT_VENC1:
	case TVIN_PORT_VENC2:
		if (vinfo && vinfo->cur_enc_ppc == 4) { //4 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 26, 2);
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 30, 2);
		} else if (vinfo && vinfo->cur_enc_ppc == 2) { //2 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 26, 2);
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 30, 2);
		} else { //1 slice
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 0, 26, 2);
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 0, 30, 2);
		}
		break;
	default:
		wr_bits_viu(VIU_WR_BAK_CTRL, 0, 0, 4);
		wr_bits_viu(VIU_WR_BAK_CTRL, 0, 4, 4);
		wr_bits_viu(VPP1_WR_BAK_CTRL, 0, 0, 2);
		wr_bits_viu(VPP2_WR_BAK_CTRL, 0, 0, 2);
		break;
	}
}

static void viuin_set_wr_bak_ctrl_t3x(enum tvin_port_e port)
{
	bool fixed_din_mode = true;/* vpp or venc may have 1 or 2slice input */
	const struct vinfo_s *vinfo = NULL;

	vinfo = get_current_vinfo();
	if (!vinfo) {
		pr_info("%s vinfo == NULL\n", __func__);
		return;
	} else {
		pr_info("ppc:%d,w:%d,h:%d,dur:%d\n", vinfo->cur_enc_ppc,
			vinfo->width, vinfo->height, vinfo->std_duration);
		wr_viu(VPU_VIU2VDIN_HDN_CTRL, vinfo->width);
	}

	unsigned long vinfo_total_size = (unsigned long)vinfo->width
						* vinfo->height
						* vinfo->std_duration;

	switch (port) {
	/* wr_bak_chan1_sel wb_chan_sel*/
	//wr_bak_chan0_sel : 0:vd1 1:vd2 2:vd3 3:osd1 4:osd2 5:post_blend_out;
	case TVIN_PORT_VIU1_WB0_VD1:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_VD1, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VD2:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_VD2, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VD3:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_VD3, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD1:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_OSD1, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD2:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_OSD2, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD3:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_OSD3, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VPP:
		fixed_din_mode = false;
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_OUT, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP0_POST_BLD_OUT, 0, 4);
		break;
	case TVIN_PORT_VIU1_VIDEO:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP_POST_VD1_BEFORE, 0, 4);
		break;
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_WB1_VD1:
	case TVIN_PORT_VIU1_WB1_VD2:
	case TVIN_PORT_VIU1_WB1_OSD1:
	case TVIN_PORT_VIU1_WB1_OSD2:
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
	case TVIN_PORT_VIU1_WB1_VPP:
	case TVIN_PORT_VIU1_WB1_VIDEO:
		break;
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VIU2_ENCP:
	case TVIN_PORT_VIU2_VD1:
	case TVIN_PORT_VIU2_OSD1:
	case TVIN_PORT_VIU2_VPP:
		wr_bits_viu(VIU_WR_BAK_CTRL, T3X_WRBACK_VPP1_POST_BLD_OUT, 0, 4);
		break;
	case TVIN_PORT_VIU3_VD1:
	case TVIN_PORT_VIU3_OSD1:
	case TVIN_PORT_VIU3_VPP:
		break;
	case TVIN_PORT_VENC0:
	case TVIN_PORT_VENC1:
	case TVIN_PORT_VENC2:
		fixed_din_mode = false;
		break;
	default:
		pr_info("do not support prot = %#x\n", port);
		wr_bits_viu(VIU_WR_BAK_CTRL, 9, 0, 4);
		break;
	}

	if (vinfo && vinfo_total_size > VDIN_LITE_CORE_MAX_PIXEL_CLOCK) { //need skip
		/* din_mode,2ppc */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 30, 2);
		/* vskip:1 */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 24, 1);
		/* dout_mode:2 */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 2, 21, 2);
		/* dn_ratio:1 */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 18, 2);
	} else if (!fixed_din_mode) {
		if (vinfo->cur_enc_ppc == 2)
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 30, 2);/* 2ppc */
		else
			wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 0, 30, 2);/* 1ppc */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 3, 21, 2);/* 4ppc */
	} else { //din_mode fixed 2 slice input
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 1, 30, 2);/* 2ppc */
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, 3, 21, 2);/* 4ppc */
	}

}
#endif

static void viuin_set_wr_bak_ctrl(enum tvin_port_e port)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_s5_cpu()) {
		viuin_set_wr_bak_ctrl_s5(port);
		return;
	} else if (is_meson_t3x_cpu()) {
		viuin_set_wr_bak_ctrl_t3x(port);
		return;
	}
#endif

	switch (port) {
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_WB0_VD1:
		wr_bits_viu(VPP_WR_BAK_CTRL, 1, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_VD2:
		wr_bits_viu(VPP_WR_BAK_CTRL, 2, 0, 4);
		break;
	case TVIN_PORT_VIU1_WB0_OSD1:
		wr_bits_viu(VPP_WR_BAK_CTRL, 3, 0, 4);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if ((is_meson_txhd2_cpu() || is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_a9_cpu()) &&
			rd_bits_viu(VPP_MISC, 27, 1))
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 11, 1);
		if (is_meson_t6x_cpu()) {/* enable wrbak */
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 9, 1);
			if (rd_bits_viu(VPP_MISC, 27, 1))
				wr_bits_viu(VPP_WR_BAK_CTRL, 0, 28, 1);
		}
#endif
		break;
	case TVIN_PORT_VIU1_WB0_OSD2:
		wr_bits_viu(VPP_WR_BAK_CTRL, 4, 0, 4);
		if (is_meson_t6x_cpu())
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 10, 1);
		break;
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
		wr_bits_viu(VPP_WR_BAK_CTRL, 5, 0, 4);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if ((is_meson_txhd2_cpu() || is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_a9_cpu()) &&
			rd_bits_viu(VPP_MISC, 27, 1))
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 11, 1);
		if (is_meson_t6x_cpu()) {/* enable wrbak */
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 30, 1);
			if (rd_bits_viu(VPP_MISC, 27, 1))
				wr_bits_viu(VPP_WR_BAK_CTRL, 0, 28, 1);
		}
#endif
		break;
	case TVIN_PORT_VIU1_WB0_VPP:
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (is_meson_t6x_cpu()) {/* enable wrbak */
			wr_bits_viu(VPP_WR_BAK_CTRL, 6, 0, 4);
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 11, 1);
			if (rd_bits_viu(VPP_MISC, 27, 1)) {
				wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
				wr_bits_viu(VPP_WR_BAK_CTRL, 0, 28, 1);
			} else {
				wr_bits_viu(VPP_WR_BAK_CTRL, 0x00, 16, 8);
			}
			break;
		}
		if ((is_meson_txhd2_cpu() || is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_a9_cpu()) &&
			rd_bits_viu(VPP_MISC, 27, 1))
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 11, 1);
#endif
		wr_bits_viu(VPP_WR_BAK_CTRL, 6, 0, 4);
		/* increase h banking in case vdin afifo overflow
		 * pre chip has 8bits
		 * tm2_revb increased 4bits, all 12bit
		 */
		wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
		break;
	case TVIN_PORT_VIU1_VIDEO:
		wr_bits_viu(VPP_WR_BAK_CTRL, 7, 0, 4);
		if (is_meson_t6x_cpu())
			wr_bits_viu(VPP_WR_BAK_CTRL, 1, 12, 1);
		break;
	/* wr_bak_chan1_sel wb_chan_sel*/
	case TVIN_PORT_VIU1_WB1_VD1:
		wr_bits_viu(VPP_WR_BAK_CTRL, 1, 4, 4);
		break;
	case TVIN_PORT_VIU1_WB1_VD2:
		wr_bits_viu(VPP_WR_BAK_CTRL, 2, 4, 4);
		break;
	case TVIN_PORT_VIU1_WB1_OSD1:
		wr_bits_viu(VPP_WR_BAK_CTRL, 3, 4, 4);
		break;
	case TVIN_PORT_VIU1_WB1_OSD2:
		wr_bits_viu(VPP_WR_BAK_CTRL, 4, 4, 4);
		break;
	case TVIN_PORT_VIU1_WB1_POST_BLEND:
		wr_bits_viu(VPP_WR_BAK_CTRL, 5, 4, 4);
		break;
	case TVIN_PORT_VIU1_WB1_VPP:
		wr_bits_viu(VPP_WR_BAK_CTRL, 6, 4, 4);

		/* increase h banking in case vdin afifo overflow
		 * pre chip has 8bits
		 * tm2_revb increased 4bits, all 12bit
		 */
		wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
		break;
	case TVIN_PORT_VIU1_WB1_VIDEO:
		wr_bits_viu(VPP_WR_BAK_CTRL, 7, 4, 4);
		break;
	case TVIN_PORT_VIU2_ENCL:
	case TVIN_PORT_VIU2_ENCI:
	case TVIN_PORT_VIU2_ENCP:
		wr_bits_viu(VPP_WR_BAK_CTRL, 6, 0, 4);

		/* increase h banking in case vdin afifo overflow
		 * pre chip has 8bits
		 * tm2_revb increased 4bits, all 12bit
		 */
		wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
		break;
	case TVIN_PORT_VIU2_VD1:
		if (is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_t6x_cpu() || is_meson_a9_cpu())
			wr_bits_viu(T6D_VPP1_WR_BAK_CTRL, 1, 0, 2);
		else
			wr_bits_viu(VPP1_WR_BAK_CTRL, 1, 0, 2);
		break;
	case TVIN_PORT_VIU2_OSD1:
		if (is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_t6x_cpu() || is_meson_a9_cpu())
			wr_bits_viu(T6D_VPP1_WR_BAK_CTRL, 2, 0, 2);
		else
			wr_bits_viu(VPP1_WR_BAK_CTRL, 2, 0, 2);
		break;
	case TVIN_PORT_VIU2_VPP:
		if (is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_t6x_cpu() || is_meson_a9_cpu())
			wr_bits_viu(T6D_VPP1_WR_BAK_CTRL, 3, 0, 2);
		else
			wr_bits_viu(VPP1_WR_BAK_CTRL, 3, 0, 2);
		break;
	case TVIN_PORT_VIU3_VD1:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 1, 0, 2);
		break;
	case TVIN_PORT_VIU3_OSD1:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 2, 0, 2);
		break;
	case TVIN_PORT_VIU3_VPP:
		wr_bits_viu(VPP2_WR_BAK_CTRL, 3, 0, 2);
		break;
	case TVIN_PORT_VENC0:
	case TVIN_PORT_VENC1:
	case TVIN_PORT_VENC2:
		wr_bits_viu(VPP_WR_BAK_CTRL, 6, 0, 4);

		/* increase h banking in case vdin afifo overflow
		 * pre chip has 8bits
		 * tm2_revb increased 4bits, all 12bit
		 */
		wr_bits_viu(VPP_WR_BAK_CTRL, 0xff, 16, 8);
		break;
	default:
		wr_bits_viu(VPP_WR_BAK_CTRL, 0, 0, 4);
		wr_bits_viu(VPP_WR_BAK_CTRL, 0, 4, 4);
		if (is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
			is_meson_t6x_cpu() || is_meson_a9_cpu())
			wr_bits_viu(T6D_VPP1_WR_BAK_CTRL, 0, 0, 2);
		else
			wr_bits_viu(VPP1_WR_BAK_CTRL, 0, 0, 2);
		wr_bits_viu(VPP2_WR_BAK_CTRL, 0, 0, 2);
		break;
	}
}

static struct device *viu_create_device(struct device *parent, int minor)
{
	dev_t devno = MKDEV(MAJOR(viu_devno), minor);

	return device_create(viu_class, parent, devno, NULL, "%s%d",
			VIU_DEVICE_NAME, minor);
}

static void viu_delete_device(int minor)
{
	dev_t devno = MKDEV(MAJOR(viu_devno), minor);

	device_destroy(viu_class, devno);
}

void viu_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

static ssize_t viu_param_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int len = 0;
	struct viuin_s *devp;

	devp = dev_get_drvdata(dev);

	len += sprintf(buf + len, "vsync_enter_line_curr = %d\n",
		       vsync_enter_line_curr);
	len += sprintf(buf + len, "vsync_enter_line_max = %d\n",
		       vsync_enter_line_max);
	len += sprintf(buf + len, "vsync_enter_line_max_threshold = %d\n",
		       vsync_enter_line_max_threshold);
	len += sprintf(buf + len, "vsync_enter_line_min_threshold = %d\n",
		       vsync_enter_line_min_threshold);

	return len;
}

static ssize_t viu_param_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len)
{
	char *buf_orig, *parm[47] = {NULL};
	long val = 0;
	struct viuin_s *devp;

	devp = dev_get_drvdata(dev);

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	viu_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "vsync_enter_line_curr")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			vsync_enter_line_curr = val;
			pr_info("vsync_enter_line_curr: %d\n", vsync_enter_line_curr);
		}
	} else if (!strcmp(parm[0], "vsync_enter_line_max_threshold")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			vsync_enter_line_max_threshold = val;
			pr_info("vsync_enter_line_max_threshold: %d\n",
				vsync_enter_line_max_threshold);
		}
	} else if (!strcmp(parm[0], "vsync_enter_line_min_threshold")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			vsync_enter_line_min_threshold = val;
			pr_info("vsync_enter_line_min_threshold: %d\n",
				vsync_enter_line_min_threshold);
		}
	} else if (!strcmp(parm[0], "vsync_enter_line_max")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			vsync_enter_line_max = val;
			pr_info("vsync_enter_line_max: %d\n", vsync_enter_line_max);
		}
	} else if (!strcmp(parm[0], "viu_dbg_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			viu_dbg_en = val;
			pr_info("viu_dbg_en: %d\n", viu_dbg_en);
		}
	}

	return len;
}
static DEVICE_ATTR_RW(viu_param);

int viu_create_dev_class_files(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_viu_param);

	if (ret < 0)
		pr_err("%s: failed\n", __func__);
	return ret;
}

/*g12a/g12b and before: use viu_loop encl/encp*/
/*tl1: use viu_loop vpp */
static int viuin_open(struct tvin_frontend_s *fe, enum tvin_port_e port,
	enum tvin_port_type_e port_type)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);
	unsigned int viu_mux = 0;
	unsigned long vinfo_total_size;
	const struct vinfo_s *vinfo = NULL;

	if (!fe->private_data)
		return -EINVAL;

	vinfo = get_current_vinfo();

	if (vinfo)
		vinfo_total_size = (unsigned long)vinfo->width
					* vinfo->height
					* vinfo->std_duration;

	if (!memcpy(&devp->parm, fe->private_data,
		    sizeof(struct vdin_parm_s))) {
		pr_info("[viuin..]%s memcpy error.\n", __func__);
		return -1;
	}

	switch (rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
	case 0: /* ENCL */
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A))
			viu_mux = 0x4;
		else
			viu_mux = 0x8;
		/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x88,4,8); */
		devp->enc_info_addr = ENCL_INFO_READ;
		break;
	case 1: /* ENCI */
		viu_mux = 0x1;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x11,4,8); */
		devp->enc_info_addr = ENCI_INFO_READ;
		break;
	case 2: /* ENCP */
		viu_mux = 0x2;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x22,4,8); */
		devp->enc_info_addr = ENCP_INFO_READ;
		break;
	case 3: /* ENCT */
		viu_mux = 0x4;/* wr_bits(VPU_VIU_VENC_MUX_CTRL,0x44,4,8); */
		devp->enc_info_addr = ENCT_INFO_READ;
		break;
	default:
		break;
	}
	/*no need check here, will timeout sometimes*/
	/*viuin_check_venc_line(devp);*/
	if (port == TVIN_PORT_VIU1_VIDEO && !is_meson_s5_cpu() && !is_meson_t3x_cpu()) {/*old chip*/
		/* enable hsync for vdin loop */
		wr_bits_viu(VIU_MISC_CTRL1, 1, 28, 1);
		viu_mux = 0x4;
	}

	/* enable write back hsync */
	if  (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) && !is_meson_s5_cpu() && !is_meson_t3x_cpu()) {
		wr_bits_viu(VIU_FRM_CTRL, 3, 24, 2);
		wr_bits_viu(VIU1_FRM_CTRL, 3, 24, 2);
		wr_bits_viu(VIU2_FRM_CTRL, 3, 24, 2);
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_gxbb_cpu() || is_meson_gxm_cpu() || is_meson_gxl_cpu()) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
		if (devp->parm.v_active == 2160 && devp->parm.frame_rate > 30)
			/* 1/2 down scaling */
			wr_viu(VPU_VIU2VDIN_HDN_CTRL, 0x40f00);
#endif
	} else if (is_meson_s5_cpu()) {
		wr_viu(VPU_VIU2VDIN_HDN_CTRL, devp->parm.h_active);
		//wr_viu(S5_VPP_POST_HOLD_CTRL, 0xc77f0412);
	} else if (is_meson_t3x_cpu()) {
		wr_viu(VPU_VIU2VDIN0_BUF_SIZE_T3X,
			(devp->parm.v_active << 16) | (devp->parm.h_active << 0));
	} else if (is_meson_t6x_cpu()) {
		/* wrbak config change */
		wr_viu(VPU_VIU2VDIN0_BUF_SIZE,
			(devp->parm.v_active << 16) | (devp->parm.h_active << 0));

		if (!devp->parm.over_pixel_clock) {
			wr_bits_viu(VPP_PATH_CTRL, 2, 24, 8);
			wr_bits_viu(VPP_PATH_CTRL, 1, 16, 8);
			wr_bits_viu(VPP_PATH_CTRL, 1, 8, 1);
		}

		if (devp->parm.over_pixel_clock)
			wr_viu(VPU_VIU2VDIN_HDN_CTRL, (0x4420 << 16) | (devp->parm.h_active << 0));
		else
			wr_viu(VPU_VIU2VDIN_HDN_CTRL, (0x4020 << 16) | (devp->parm.h_active << 0));
	} else
#endif
	{
		wr_bits_viu(VPU_VIU2VDIN_HDN_CTRL, devp->parm.h_active, 0, 14);
	}

	/* select loopback path */
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		/* set VPU_VIU_VDIN_IF_MUX_CTRL */
		viuin_set_vdin_if_mux_ctrl(port);
		/* set VPP_WR_BAK_CTRL */
		viuin_set_wr_bak_ctrl(port);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (!is_meson_s5_cpu() && !is_meson_t3x_cpu()) {
			/* wr_back hsync en */
			if (port >= TVIN_PORT_VIU1_VIDEO &&
			    port <= TVIN_PORT_VIU1_WB0_POST_BLEND) {
				wr_bits_viu(WR_BACK_MISC_CTRL, 1, 0, 1);/*vd0 hsync*/
				wr_bits_viu(WR_BACK_MISC_CTRL, 0, 1, 1);/*vd1 hsync*/
			} else {
				wr_bits_viu(WR_BACK_MISC_CTRL, 0xf, 0, 4);
			}
		}
#endif
	} else {
		wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, viu_mux, 4, 4);
		wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, viu_mux, 8, 4);
	}
	devp->flag = 0;
	open_cnt++;
	return 0;
}

static void viuin_close(struct tvin_frontend_s *fe, enum tvin_port_type_e port_type)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	/*no need check here, will timeout sometimes*/
	/*viuin_check_venc_line(devp);*/
	memset(&devp->parm, 0, sizeof(struct vdin_parm_s));
	/*close the venc to vdin path*/
	if (open_cnt)
		open_cnt--;
	if (open_cnt == 0) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
			wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (is_meson_t6x_cpu()) {
				wr_bits_viu(VPP_WR_BAK_CTRL, 0, 0, 23);
				wr_bits_viu(VPP_WR_BAK_CTRL, 0, 30, 1);
				if (!rd_bits_viu(VPP_WR_BAK_CTRL, 28, 1))
					wr_bits_viu(VPP_WR_BAK_CTRL, 1, 28, 1);
				wr_viu(VPU_VIU2VDIN0_HALF_CTRL, 0);
			} else {
				wr_viu(VPP_WR_BAK_CTRL, 0);
			}
#else
			wr_viu(VPP_WR_BAK_CTRL, 0);
#endif
		} else {
			wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 8, 4);
			wr_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 4, 4);
		}
	}
	if (rd_viu(VPU_VIU2VDIN_HDN_CTRL) != 0)
		wr_viu(VPU_VIU2VDIN_HDN_CTRL, 0x0);
}

static void viuin_start(struct tvin_frontend_s *fe, enum tvin_sig_fmt_e fmt,
	enum tvin_port_type_e port_type)
{
	/* do something the same as start_amvdec_viu_in */
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->flag && AMVIUIN_DEC_START) {
		pr_info("[viuin..]%s viu_in is started already.\n", __func__);
		return;
	}
	vsync_enter_line_max = 0;
	vsync_enter_line_threshold_overflow_count = 0;
	devp->flag = AMVIUIN_DEC_START;
}

static void viuin_stop(struct tvin_frontend_s *fe, enum tvin_port_e port,
	enum tvin_port_type_e port_type)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->flag && AMVIUIN_DEC_START)
		devp->flag = AMVIUIN_DEC_STOP;
	else
		pr_info("[viuin..]%s viu in dec isn't start.\n", __func__);
	//pr_info("%s %d Disable VIU to VDIN\n", __func__, __LINE__);
	wr_viu(VPU_VIU_VDIN_IF_MUX_CTRL, 0);
	wr_viu(VPU_VIU2VDIN0_BUF_SIZE, 0);
	if (is_meson_t6x_cpu()) {
		if (rd_bits_viu(VPP_PATH_CTRL, 24, 8))
			wr_bits_viu(VPP_PATH_CTRL, 0, 24, 8);
		if (rd_bits_viu(VPP_PATH_CTRL, 16, 8))
			wr_bits_viu(VPP_PATH_CTRL, 0, 16, 8);
		if (rd_bits_viu(VPP_PATH_CTRL, 8, 1))
			wr_bits_viu(VPP_PATH_CTRL, 0, 8, 1);
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/* txhd2 keystone path close */
	if ((is_meson_txhd2_cpu() || is_meson_t6d_cpu() || is_meson_t6w_cpu() ||
		is_meson_t6x_cpu() || is_meson_a9_cpu()) && rd_bits_viu(VPP_WR_BAK_CTRL, 11, 1))
		wr_bits_viu(VPP_WR_BAK_CTRL, 0, 11, 1);
#endif
}

static int viuin_isr(struct tvin_frontend_s *fe, unsigned int hcnt64,
	enum tvin_port_type_e port_type)
{
	int curr_port;

	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	curr_port = rd_bits_viu(VPU_VIU_VENC_MUX_CTRL, 0, 2);

	vsync_enter_line_curr = (rd_viu(devp->enc_info_addr) >> 16) & 0x1fff;
	if (vsync_enter_line_curr > vsync_enter_line_max)
		vsync_enter_line_max = vsync_enter_line_curr;
	if (vsync_enter_line_max_threshold > vsync_enter_line_min_threshold &&
	    curr_port == 0) {
		if (vsync_enter_line_curr > vsync_enter_line_max_threshold ||
		    vsync_enter_line_curr < vsync_enter_line_min_threshold) {
			vsync_enter_line_threshold_overflow_count++;
			return TVIN_BUF_SKIP;
		}
	}
	return 0;
}

static struct tvin_decoder_ops_s viu_dec_ops = {
	.owner = THIS_MODULE,
	.support            = viuin_support,
	.open               = viuin_open,
	.start              = viuin_start,
	.stop               = viuin_stop,
	.close              = viuin_close,
	.decode_isr         = viuin_isr,
};

static void viuin_sig_property(struct tvin_frontend_s *fe,
			       struct tvin_sig_property_s *prop,
			       enum tvin_port_type_e port_type)
{
	static const struct vinfo_s *vinfo;
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);
	unsigned int line_begin = 0, line_end = 0;
	unsigned int venc_offset = 0;

	switch (devp->parm.port) {
	case TVIN_PORT_VIU1_VIDEO:
	case TVIN_PORT_VIU1_WB0_POST_BLEND:
	case TVIN_PORT_VIU2_VD1:
		prop->color_format = TVIN_YUV444;
		break;
	case TVIN_PORT_VIU1:
	case TVIN_PORT_VIU2:
	case TVIN_PORT_VIU1_WB0_VPP:
	case TVIN_PORT_VIU1_WB1_VPP:
		vinfo = get_current_vinfo();
		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VIU2_VPP:
	#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vinfo = get_current_vinfo2();
		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		pr_info("color_format:%#x\n", prop->color_format);
	#else
		prop->color_format = TVIN_YUV444;
		pr_info("def color_format:%#x\n", prop->color_format);
	#endif
		break;

	/* ENCL/ENCI/ENCP is only for viu2 loopback currently
	 * though hw also support viu1 loopback through ENC
	 */
	case TVIN_PORT_VIU2_ENCL:
		vinfo = get_current_vinfo2();
		line_begin = rd_viu(ENCL_VIDEO_VSO_BLINE);
		line_end = rd_viu(ENCL_VIDEO_VSO_ELINE);

		if (line_begin < line_end)
			prop->polarity_vs = 1; /* negative */
		else if (line_begin > line_end)
			prop->polarity_vs = 0; /* positive */
		else
			pr_err("error: TVIN_PORT_VIU2_ENCL line begin = end\n");

		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VIU2_ENCI:
		vinfo = get_current_vinfo2();

		/* always negative */
		prop->polarity_vs = 1; /* negative */
		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VIU2_ENCP:
		vinfo = get_current_vinfo2();
		line_begin = rd_viu(ENCP_VIDEO_VSO_BLINE);
		line_end = rd_viu(ENCP_VIDEO_VSO_ELINE);

		if (line_begin < line_end)
			prop->polarity_vs = 1; /* negative */
		else if (line_begin > line_end)
			prop->polarity_vs = 0; /* positive */
		else
			pr_err("error: TVIN_PORT_VIU2_ENCP line begin = end\n");

		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;
		break;
	case TVIN_PORT_VENC0:
	case TVIN_PORT_VENC1:
	case TVIN_PORT_VENC2:
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (is_meson_s5_cpu())
			vinfo = get_current_vinfo();
		else
#endif
			vinfo = get_current_vinfo2();

		if (devp->parm.port == TVIN_PORT_VENC1)
			venc_offset = 0x600;
		else if (devp->parm.port == TVIN_PORT_VENC2)
			venc_offset = 0x800;
		else
			venc_offset = 0;
		if ((vinfo->viu_mux & 0xf) == VIU_MUX_ENCL) {
			line_begin = rd_viu(ENCL_VIDEO_VSO_BLINE + venc_offset);
			line_end = rd_viu(ENCL_VIDEO_VSO_ELINE + venc_offset);
		} else if ((vinfo->viu_mux & 0xf) == VIU_MUX_ENCI) {
			prop->polarity_vs = 1;
		} else if ((vinfo->viu_mux & 0xf) == VIU_MUX_ENCP) {
			line_begin = rd_viu(ENCP_VIDEO_VSO_BLINE + venc_offset);
			line_end = rd_viu(ENCP_VIDEO_VSO_ELINE + venc_offset);
		}

		if (line_begin < line_end || cpu_after_eq(MESON_CPU_MAJOR_ID_T7))
			prop->polarity_vs = 1; /* negative */
		else if (line_begin > line_end)
			prop->polarity_vs = 0; /* positive */
		else
			pr_err("error: TVIN_PORT_VIU2_ENCP line begin = end\n");

		prop->color_format = (enum tvin_color_fmt_e)vinfo->viu_color_fmt;

		pr_info("%s cfmt=%d;line:%d,%d,ppc:%d,name:%s,mode:%d,w:%d,h:%d\n",
			__func__, prop->color_format, line_begin, line_end,
			vinfo->cur_enc_ppc, vinfo->name, vinfo->mode,
			vinfo->width, vinfo->height);
		break;
	default:
		prop->color_format = devp->parm.cfmt;
		break;
	}

	prop->dest_cfmt = devp->parm.dfmt;
	prop->decimation_ratio = 0;
}

static bool viu_check_frame_skip(struct tvin_frontend_s *fe, enum tvin_port_type_e port_type)
{
	struct viuin_s *devp = container_of(fe, struct viuin_s, frontend);

	if (devp->parm.skip_count > 0) {
		devp->parm.skip_count--;
		return true;
	}

	return false;
}

static struct tvin_state_machine_ops_s viu_sm_ops = {
	.get_sig_property = viuin_sig_property,
	.check_frame_skip = viu_check_frame_skip,
};

static int viuin_probe(struct platform_device *pdev)
{
	struct viuin_s *viuin_devp;
	int ret = 0;

	viuin_devp = kmalloc(sizeof(*viuin_devp), GFP_KERNEL);
	if (!viuin_devp)
		return -ENOMEM;
	memset(viuin_devp, 0, sizeof(struct viuin_s));
	sprintf(viuin_devp->frontend.name, "%s", VIU_DEVICE_NAME);
	if (!tvin_frontend_init(&viuin_devp->frontend,
				&viu_dec_ops, &viu_sm_ops, 0)) {
		if (tvin_reg_frontend(&viuin_devp->frontend))
			pr_info("[viuin..]%s register viu frontend error.\n",
				__func__);
	}

	viuin_devp->index = 0;
	viuin_devp->dev = viu_create_device(&pdev->dev, viuin_devp->index);
	if (IS_ERR_OR_NULL(viuin_devp->dev)) {
		pr_err("viu_create_device failed\n");
		ret = PTR_ERR(viuin_devp->dev);
		return ret;
	}
	ret = viu_create_dev_class_files(viuin_devp->dev);
	if (ret < 0)
		goto fail_create_dev_file;
	/* set drvdata */
	dev_set_drvdata(viuin_devp->dev, viuin_devp);
	platform_set_drvdata(pdev, viuin_devp);
	pr_info("[viuin..]%s probe ok.\n", __func__);
	return 0;
fail_create_dev_file:
	viu_delete_device(viuin_devp->index);
return ret;
}

void viuin_remove(struct platform_device *pdev)
{
	struct viuin_s *devp = platform_get_drvdata(pdev);

	if (devp) {
		device_remove_file(devp->dev, &dev_attr_viu_param);
		viu_delete_device(devp->index);
		/* free drvdata */
		dev_set_drvdata(devp->dev, NULL);
		tvin_unreg_frontend(&devp->frontend);
		kfree(devp);
	}
}

static struct platform_driver viuin_driver = {
	.probe	= viuin_probe,
	.remove	= viuin_remove,
	.driver	= {
		.name	= VIU_DEVICE_NAME,
	}
};

static struct platform_device *viuin_device;

int __init viuin_init_module(void)
{
	int ret = 0;

	pr_info("[viuin..]%s viuin module init\n", __func__);

	ret = alloc_chrdev_region(&viu_devno, 0, 1, VIU_DEVICE_NAME);
	if (ret < 0) {
		pr_err("%s: failed to allocate major number\n", __func__);
		goto fail_alloc_cdev_region;
	}

	viu_class = class_create(VIU_CLS_NAME);
	if (IS_ERR_OR_NULL(viu_class)) {
		ret = PTR_ERR(viu_class);
		pr_err("%s: failed to create class or ret=NULL\n", __func__);
		goto fail_class_create;
	}
	viuin_device = platform_device_alloc(VIU_DEVICE_NAME, 0);
	if (!viuin_device) {
		pr_err("[viuin..]%s failed to alloc viuin_device.\n",
		       __func__);
		return -ENOMEM;
	}

	if (platform_device_add(viuin_device)) {
		platform_device_put(viuin_device);
		pr_err("[viuin..]%sfailed to add viuin_device.\n", __func__);
		return -ENODEV;
	}
	if (platform_driver_register(&viuin_driver)) {
		pr_err("[viuin..]%sfailed to register viuin driver.\n",
		       __func__);
		platform_device_del(viuin_device);
		platform_device_put(viuin_device);
		return -ENODEV;
	}

	pr_info("[viuin..]%s done\n", __func__);
	return 0;
fail_class_create:
	unregister_chrdev_region(viu_devno, 1);
fail_alloc_cdev_region:
	return ret;

}

void __exit viuin_exit_module(void)
{
	pr_info("[viuin..]%s viuin module remove.\n", __func__);
	class_destroy(viu_class);
	unregister_chrdev_region(viu_devno, 1);
	platform_driver_unregister(&viuin_driver);
	platform_device_unregister(viuin_device);
}

//MODULE_DESCRIPTION("AMLOGIC viu input driver");
//MODULE_LICENSE("GPL");
//MODULE_VERSION("3.0.0");
