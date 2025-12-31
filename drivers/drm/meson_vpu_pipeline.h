/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_VPU_TOPOLOGY_H
#define __MESON_VPU_TOPOLOGY_H

#include <linux/kfifo.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fourcc.h>
#include <dt-bindings/display/meson-drm-ids.h>
#include "vpu-hw/meson_vpu_reg.h"
#include "meson_drv.h"
#include "meson_vpu_util.h"
#include "meson_plane.h"
#include "meson_drm_rdma.h"
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif

#define MESON_OSD1 0
#define MESON_OSD2 1
#define MESON_OSD3 2
#define MESON_OSD4 3
#define MESON_OSD5 4
#define MESON_VIU2_OSD1 3
#define MESON_MAX_OSDS 5
#define MESON_MAX_VIDEOS 3
#define MESON_MAX_OSD_BLEND 3
#define MESON_MAX_OSD_TO_VPP 2
#define MESON_MAX_SCALERS 5
#define MESON_MAX_OSDBLENDS 2
#define MESON_MAX_HDRS 3
#define MESON_MAX_DBS 2
#define MESON_MAX_POSTBLEND 3
#define MESON_MAX_BLOCKS 32
#define MESON_MAX_CSCS 2
#define MESON_BLOCK_MAX_INPUTS 6
#define MESON_BLOCK_MAX_OUTPUTS 3
#define MESON_BLOCK_MAX_NAME_LEN 32
/*ratio base for scaler calc;maybe need bigger than 1000*/
#define RATIO_BASE 1000
#define RATIO_H_MIX 500
#define MESON_OSD_INPUT_W_LIMIT 3840
#define MESON_OSD_INPUT_H_LIMIT 2160

#define MAX_DIN_NUM 5
#define MAX_DOUT_NUM 3

#define VP_MAP_STRUCT_SIZE	120
#define BUFFER_NUM		4
#define MAX_DFS_PATH_NUM 4

#define MAX_CANVAS_NUM	3
#define MAX_CANVAS_FIFO_NUM  4

/*
 *according to reg description,scale down limit shold be 4bits=16
 *but test result is 10,it will display abnormal if bigger than 10.xx
 *have not get more info from others,so config it as 10 right now.
 *Todo:if you make sure and test okay,you can rechange it.
 */
#define MESON_OSD_SCALE_DOWN_LIMIT 10
#define MESON_OSD_SCALE_UP_LIMIT ((1 << 24) - 1)

#define SCALER_RATIO_X_CALC_DONE BIT(0)
#define SCALER_RATIO_Y_CALC_DONE BIT(1)
#define SCALER_IN_W_CALC_DONE BIT(2)
#define SCALER_IN_H_CALC_DONE BIT(3)
#define SCALER_OUT_W_CALC_DONE BIT(4)
#define SCALER_OUT_H_CALC_DONE BIT(5)

#define SCALER_INPUT_WIDTH_CHANGED BIT(0)
#define SCALER_INPUT_HEIGHT_CHANGED BIT(1)
#define SCALER_OUTPUT_WIDTH_CHANGED BIT(2)
#define SCALER_OUTPUT_HEIGHT_CHANGED BIT(3)
#define SCALER_OUTPUT_SCAN_MODE_CHANGED BIT(4)

#define SCALER_OVERLAP 32

#define VIU_OSD1_TCOLOR_AG3 0x1a1a
#define VIU_OSD2_TCOLOR_AG3 0x1a3a
#define VIU_OSD3_TCOLOR_AG3 0x3d87

#define MAX_POLICY_ID 64

extern u32 frame_seq[MESON_MAX_OSDS];

enum meson_policy_id {
	GFCD_ODD_SIZE = 0,
	GFCD_GLOBAL_ALPHA = 1,
	HDR_BEFORE_BLEND,
};

enum meson_osd_capability {
	AFBC_SUPPORT = 0,
	SCALER_SUPPORT = 1,
	LOCAL_SCALER_SUPPORT,
};

enum slice_index {
	OSD1_SLICE0,
	OSD2_SLICE1,
	OSD3_SLICE1,
};

enum vpp_index {
	VPP0,
	VPP1,
	VPP2,
	VPP_MAX,
};

enum meson_vpu_blk_type {
	MESON_BLK_OSD = 0,
	MESON_BLK_AFBC,
	MESON_BLK_SCALER,
	MESON_BLK_OSDBLEND,
	MESON_BLK_HDR,
	MESON_BLK_DOVI,
	MESON_BLK_VPPBLEND,
	MESON_BLK_VIDEO,
	MESON_BLK_SLICE2PPC,
	MESON_BLK_GFCD,
	MESON_BLK_CSC,
};

struct meson_vpu_pipeline;
struct meson_video_sub_pipeline;
struct meson_vpu_block;
struct meson_vpu_block_state;
struct meson_video_sub_pipeline_state;
struct meson_vpu_sub_pipeline_state;
struct meson_vpu_sub_pipeline;

/* vpu block ops */
struct meson_vpu_block_ops {
	int (*check_state)(struct meson_vpu_block *vblk,
			   struct meson_vpu_block_state *state,
			   struct meson_vpu_sub_pipeline_state *mvps);
	int (*check_video_state)(struct meson_vpu_block *vblk,
			   struct meson_vpu_block_state *state,
		struct meson_video_sub_pipeline_state *mvps);
	void (*update_state)(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *new_state,
			     struct meson_vpu_block_state *old_state,
			     struct meson_vpu_sub_pipeline_state *mvsps);
	void (*update_video_state)(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *new_state,
			     struct meson_vpu_block_state *old_state,
			     struct meson_video_sub_pipeline_state *mvsps);
	void (*detect_reset)(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *new_state);
	void (*enable)(struct meson_vpu_block *vblk,
		       struct meson_vpu_block_state *new_state);
	void (*disable)(struct meson_vpu_block *vblk,
			    struct meson_vpu_sub_pipeline *sub_pipeline);
	void (*disable_video)(struct meson_vpu_block *vblk,
			    struct meson_video_sub_pipeline *sub_pipeline);
	void (*dump_register)(struct drm_printer *p, struct meson_vpu_block *vblk);
	void (*init)(struct meson_vpu_block *vblk);
	void (*init_register)(struct meson_vpu_block *vblk,
			struct meson_vpu_block_state *state);
	void (*fini)(struct meson_vpu_block *vblk);
};

struct meson_vpu_block_link {
	struct meson_vpu_block *link;
	u8 id;
	u8 port;
	int edges_active;
	int edges_visited;
};

struct meson_rdma_done_detect {
	u32 reg;
	u32 val;
};

/* vpu block */
struct meson_vpu_block {
	struct drm_private_obj obj;
	char name[MESON_BLOCK_MAX_NAME_LEN];

	enum meson_vpu_blk_type type;
	u8 id;
	/*index in same type objects array,update to object_index*/
	u8 index;
	u8 max_inputs;
	u8 max_outputs;
	u8 avail_inputs;
	u8 avail_outputs;
	u8 init_done;
	unsigned long inputs_mask;
	unsigned long outputs_mask;
	unsigned long capability;
	struct meson_vpu_block_link inputs[MESON_BLOCK_MAX_INPUTS];
	struct meson_vpu_block_link outputs[MESON_BLOCK_MAX_OUTPUTS];
	struct meson_vpu_block_ops *ops;
	struct meson_vpu_pipeline *pipeline;
};

struct meson_vpu_block_state {
	struct drm_private_state obj;
	struct meson_vpu_block *pblk;
	struct meson_vpu_sub_pipeline *sub;
	u32 inputs_mask;

	u32 inputs_changed;
	u32 outputs_changed;
	u32 checked;

	u32 state_changed;

	struct meson_vpu_block_link inputs[MESON_BLOCK_MAX_INPUTS];
	struct meson_vpu_block_link outputs[MESON_BLOCK_MAX_OUTPUTS];
	int in_stack;
	int active;

};

enum osd_process_unit {
	MIF_MODE,
	MALI_AFBC,
	GFCD_AFBC,
	GFCD_AFRC,
};

struct meson_vpu_osd_layer_info {
	/* record the original src and dst info */
	u32 ori_src_x;
	u32 ori_src_y;
	u32 ori_src_w;
	u32 ori_src_h;
	u32 ori_dst_w;
	u32 ori_dst_h;
	int ori_dst_x;
	int ori_dst_y;

	/* record the modified src and dst info, they may be the same as the original info */
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;

	u32 fb_w;
	u32 fb_h;
	u32 hdisplay;
	u32 vdisplay;
	u32 src_w_offset4hdr;
	u32 zorder;
	u32 byte_stride;
	u32 pixel_format;
	u64 phy_addr;
	u32 plane_index;
	u32 uhd_plane_index;
	u32 enable;
	u32 ratio_x;/*input_w/output_w*/
	u32 afbc_inter_format;
	u32 afbc_en;
	enum osd_process_unit process_unit;
	u32 afrc_cu_bits;
	u32 fb_size;
	u32 pixel_blend;
	u32 rotation;
	u32 blend_bypass;
	u32 global_alpha;
	u32 scaling_filter;
	u32 crtc_index;
	u32 read_ports;
	u32 status_changed;
	int sec_en;
	u32 *palette_arry;
	int gfcd_vskip;
	int gfcd_hskip;
	u16 htotal;
	/**
	 * @clock:
	 *
	 * Pixel clock in kHz.
	 */
	int clock;		/* in kHz */
	struct osd_dimm_info osd_dimm;
};

enum osd_mif_acc_mode {
	CANVAS_MODE,
	LINEAR_AFBC,
	LINEAR_MIF,
};

struct meson_vpu_osd {
	struct meson_vpu_block base;
	struct osd_mif_reg_s *reg;
	int mif_acc_mode;
	int viu2_hold_line;
	u32 mali_src_en_switch;
	u32 is_viu2_osd;
	const struct meson_drm_format_info **infos;
	int format_swap;
	/*t6d t6w t6d a9 ARGB2101010 BGRA1010102*/
	/*t6x ARGB2101010 BGRA1010102*/
	int format_swap_AR30_BA30;
	DECLARE_KFIFO(canvas_q, u32, MAX_CANVAS_FIFO_NUM);
	bool has_gfcd;
	bool no_afbc_ctrl;
	bool gfcd_global_alpha_policy;
};

struct osd_zorder_s {
	int plane_index;
	int zorder;
};

struct meson_vpu_osd_state {
	struct meson_vpu_block_state base;

	u64 phy_addr;
	u32 index;
	u32 enable;
	u32 color_key_enable;
	u32 dimm_enable;
	u32 mode_3d_enable;

	u32 color_key;
	u32 alpha;
	u32 global_alpha;
	u32 dimm_color;
	u32 pixel_format;
	u32 zorder;
	u32 byte_stride;
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;
	u32 fb_w;
	u32 fb_h;
	int s_mode;
	int r_mode;
	u32 plane_index;
	u32 fb_size;
	u32 pixel_blend;
	enum osd_process_unit process_unit;
	u32 afbc_en;
	u32 rotation;
	u32 blend_bypass;
	u32 crtc_index;
	u32 read_ports;
	int sec_en;
	u32 *palette_arry;
	struct osd_dimm_info osd_dimm;
};

struct meson_vpu_video_layer_info {
	/* record the original src and dst info */
	u32 ori_src_x;
	u32 ori_src_y;
	u32 ori_src_w;
	u32 ori_src_h;
	u32 ori_dst_w;
	u32 ori_dst_h;
	int ori_dst_x;
	int ori_dst_y;

	/* record the modified src and dst info, they may be the same as the original info */
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;

	u32 fb_w;
	u32 fb_h;
	u32 zorder;
	u32 byte_stride;
	u32 pixel_format;
	u64 phy_addr[2];
	u32 plane_index;
	u32 enable;
	u32 ratio_x;/*input_w/output_w*/
	u32 fb_size[2];
	u32 pixel_blend;
	u32 crtc_index;
	u32 rotation;
	u32 repeat_frame;
	struct vframe_s *vf;
	struct dma_buf *dmabuf[DRM_FORMAT_MAX_PLANES];
	struct dma_fence *in_fence;
	bool is_uvm;
	u32 status_changed;
	int sec_en;
	u32 signal_fmt;
	u32 frame_flag;
};

struct meson_vpu_disable_work {
	struct work_struct work;
	struct meson_vpu_video *video;
	struct dma_buf *dmabuf[DRM_FORMAT_MAX_PLANES];
	u32 idx;
};

struct meson_vpu_video {
	struct meson_vpu_block base;
	struct vframe_provider_s vprov;
	char vfm_map_id[VP_MAP_STRUCT_SIZE];
	char vfm_map_chain[VP_MAP_STRUCT_SIZE];
	DECLARE_KFIFO(ready_q, struct vframe_s *, BUFFER_NUM);
	DECLARE_KFIFO(free_q, struct vframe_s *, BUFFER_NUM);
	DECLARE_KFIFO(display_q, struct vframe_s *, BUFFER_NUM);
	struct vframe_s vframe[BUFFER_NUM];
	u32 video_path_reg;
	bool video_enabled;
	struct dma_fence *fence;
	struct dma_buf *dmabuf[DRM_FORMAT_MAX_PLANES];
	struct dma_fence *present_fence;
	struct list_head vfm_node[MESON_MAX_VIDEO];
	struct meson_vpu_disable_work worker;
	struct task_struct *disable_thread;
	bool disable_thread_should_stop;
	wait_queue_head_t disable_wq;
	atomic_t work_pending;

	/* capability of video plane */
	u32 video_cap;
	/*
	 * minimal source buffer size of video plane
	 * returned in format "width<< 16 | height"
	 */
	u32 src_min_size;
	/*
	 * maximal source buffer size of video plane
	 * returned in format "width<< 16 | height"
	 */
	u32 src_max_size;

	u64 frame_cnt;
};

struct meson_vpu_video_state {
	struct meson_vpu_block_state base;

	u32 index;
	u32 enable;
	u32 color_key_enable;
	u32 dimm_enable;
	u32 mode_3d_enable;

	u32 crtc_index;
	u32 color_key;
	u32 alpha;
	u32 global_alpha;
	u32 dimm_color;
	u64 phy_addr[2];
	u32 pixel_format;
	u32 zorder;
	u32 byte_stride;
	u32 src_x;
	u32 src_y;
	u32 src_w;
	u32 src_h;
	u32 dst_w;
	u32 dst_h;
	int dst_x;
	int dst_y;
	u32 fb_w;
	u32 fb_h;
	int s_mode;
	int r_mode;
	u32 plane_index;
	u32 fb_size[2];
	u32 pixel_blend;
	u32 afbc_en;
	u32 repeat_frame;
	u32 rotation;
	struct vframe_s *vf;
	struct dma_buf *dmabuf[DRM_FORMAT_MAX_PLANES];
	struct dma_fence *in_fence;
	bool is_uvm;
	int sec_en;
	u32 signal_fmt;
	u32 frame_flag;
};

struct meson_vpu_afbc {
	struct meson_vpu_block base;
	struct afbc_osd_reg_s *afbc_regs;
	struct afbc_status_reg_s *status_regs;
	u32 num_of_4k_osd;
	int shift_bits;
	int start_surface;
	int end_surface;
};

struct meson_vpu_afbc_state {
	struct meson_vpu_block_state base;

	u32 format;
	u32 inter_format;
};

/*
 * from s7 soc, scaler hw add alpha premulti process
 * for input, and add alpha divide process for output
 */
enum alpha_proc_mode {
	NO_ALPHA_PROC,
	ALPHA_PROC,
};

struct meson_vpu_scaler {
	struct meson_vpu_block base;
	struct osd_scaler_reg_s *reg;
	u32 linebuffer;/*base pixel*/
	u32 bank_length;/*base line*/
	enum alpha_proc_mode alpha_mode;
	enum scaler1_pos scaler1_position;
	bool latch;
};

struct meson_vpu_scaler_state {
	struct meson_vpu_block_state base;

	u32 crtc_index;
	u32 free_scale_mode;
	u32 input_width;
	u32 input_height;
	u32 output_width;
	u32 output_height;
	u32 input_width_offset;
	u32 output_width_offset;
	u32 ratio_x;
	u32 ratio_y;
	u32 scan_mode_out;
	u32 state_changed;
	u32 free_scale_enable;
	u32 scaler_filter_mode;
	u32 global;
};

struct meson_vpu_scaler_param {
	u32 input_width;
	u32 input_height;
	u32 output_width;
	u32 output_height;
	u32 input_width_offset;
	u32 output_width_offset;

	//u32 ratio_x;
	//u32 ratio_y;

	u32 ratio_w_num;
	u32 ratio_w_den;
	u32 ratio_h_num;
	u32 ratio_h_den;

	/*calc_done_mask:
	 *bit0:ratio_x,
	 *bit1:ratio_y
	 *bit2:input_width
	 *bit3:input_height
	 *bit4:output_width
	 *bit5:output_height
	 */
	u32 calc_done_mask;
	/*
	 *bit0:plane0
	 *bit1:plane1
	 *bit2:plane2
	 *bit*:plane*
	 */
	u32 plane_mask;
	u32 enable;
	u32 before_osdblend;
	u32 global;
};

struct meson_vpu_osdblend {
	struct meson_vpu_block base;
	struct osdblend_reg_s *reg;
	bool gfcd_global_alpha_policy;
};

struct meson_vpu_osdblend_state {
	struct meson_vpu_block_state base;

	u32 input_num;
	/*bit0/bit1/bit2-->osd0/osd1/osd2*/
	u32 input_osd_mask;
	/*Din mask:bit0:DIN0;bit1:DIN1;bit2:DIN2;bit3:DIN3*/
	u32 input_mask;
	/*Din0~3 select which input channel or osd(0/1/2)*/
	u32 din_channel_mux[MAX_DIN_NUM];
	/*osd(0/1/2) go through osdblend to dout0 or dout1*/
	u32 dout_mux[MAX_DIN_NUM];
	/*scope position before mux,tied with osd0/osd1/osd2*/
	struct osd_scope_s din_channel_scope[MAX_DIN_NUM];
	/*sub-blend0 and sub-blend1 size*/
	u32 input_width[MESON_MAX_OSD_BLEND];
	u32 input_height[MESON_MAX_OSD_BLEND];
	/*0:din0-->blend0;1:din0-->Dout0,bypass OsdBlend*/
	u32 din0_switch;
	/*0:din3-->blend1;1:din3-->Dout1,bypass OsdBlend*/
	u32 din3_switch;
	/*0:blend1-->blend2;1:blend1-->Dout1,bypass Blend2*/
	u32 blend1_switch;

};

struct meson_vpu_hdr {
	struct meson_vpu_block base;
	struct hdr_reg_s *reg;
};

struct meson_vpu_hdr_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_db {
	struct meson_vpu_block base;
};

struct meson_vpu_db_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_postblend {
	struct meson_vpu_block base;
	struct postblend_reg_s *reg;
	struct postblend1_reg_s *reg1;
	bool postblend_path_mask;
	bool memc_go_field;
};

struct meson_vpu_postblend_state {
	struct meson_vpu_block_state base;
	struct osd_scope_s postblend_scope[MESON_MAX_OSD_TO_VPP];
};

struct meson_vpu_slice2ppc {
	struct meson_vpu_block base;
	struct slice2ppc_reg_s *reg;
};

struct meson_vpu_slice2ppc_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_gfcd {
	struct meson_vpu_block base;
	struct gfcd_reg_s *reg;
	u32 num_surface;
	u32 gfbc_err_cnt;
	bool support_global_alpha;
	u32 start_surface;
	u32 end_surface;
};

struct meson_vpu_gfcd_state {
	struct meson_vpu_block_state base;
};

struct meson_vpu_csc {
	struct meson_vpu_block base;
	struct csc_reg_s *reg;
};

struct meson_vpu_csc_state {
	struct meson_vpu_block_state base;
};

/* vpu pipeline */
struct rdma_reg_ops {
	u32 (*rdma_read_reg)(u32 addr);
	int (*rdma_write_reg)(u32 addr, u32 val);
	int (*rdma_write_reg_bits)(u32 addr, u32 val, u32 start, u32 len);
	u32 (*dummy_read_reg)(u32 addr);
	int (*dummy_write_reg)(u32 addr, u32 val);
	int (*dummy_write_reg_bits)(u32 addr, u32 val, u32 start, u32 len);
};

struct meson_vpu_state_check {
	u32 vpu_mafbc_header_buf_addr_low_s;
	u32 viu_osd_blk1_cfg_w4;
	u64 phy_addr;
	int update;
	int index;
	bool afbc_en;
	u32 mif_acc_mode;
	u32 canvas_index;
};

struct meson_vpu_sub_pipeline {
	struct drm_private_obj obj;
	int index;
	struct meson_vpu_pipeline *pipeline;
	struct drm_display_mode mode;
	struct rdma_reg_ops *reg_ops;
	enum vmode_e vmode;
	struct meson_vpu_state_check status[MESON_MAX_OSDS];
	struct am_meson_fb *logo_fb;
};

struct meson_video_sub_pipeline {
	struct drm_private_obj obj;
	int index;
	struct meson_vpu_pipeline *pipe;
};

struct meson_vpu_pipeline_ops {
	int (*check_pipeline_path)(int *combination, int num_planes,
				  struct meson_vpu_sub_pipeline_state *mvps,
				  struct drm_atomic_state *state);
	int (*set_pipeline_para)(int *combination, int num_planes,
				  struct meson_vpu_sub_pipeline_state *mvps,
				  struct drm_atomic_state *state);
};

struct meson_vpu_pipeline {
	struct meson_vpu_sub_pipeline *subs[MESON_MAX_CRTC];
	struct meson_video_sub_pipeline video_subs[MESON_MAX_CRTC];
	struct meson_vpu_osd *osds[MESON_MAX_OSDS];
	struct meson_vpu_video *video[MESON_MAX_VIDEOS];
	struct meson_vpu_afbc *afbc_osds[MESON_MAX_OSDS];
	struct meson_vpu_scaler *scalers[MESON_MAX_SCALERS];
	struct meson_vpu_osdblend *osdblend[MESON_MAX_OSDBLENDS];
	struct meson_vpu_hdr *hdrs[MESON_MAX_HDRS];
	struct meson_vpu_db *dbs[MESON_MAX_DBS];
	struct meson_vpu_postblend *postblends[MESON_MAX_POSTBLEND];
	struct meson_vpu_slice2ppc *slice2ppc;
	struct meson_vpu_gfcd *gfcd[MESON_MAX_OSDS];
	struct meson_vpu_csc *csc[MESON_MAX_CSCS];
	struct meson_vpu_pipeline_ops *ops;
	/*reference enum meson_osd_capability*/
	u64 osd_capability[MESON_MAX_OSDS];
	u32 num_osds;
	u32 num_video;
	u32 num_afbc_osds;
	u32 num_scalers;
	u32 num_hdrs;
	u32 num_osdblends;
	u32 num_dbs;
	u32 num_postblend;
	u32 num_gfcd;
	u32 num_csc;
	u8 osd_version;
	u32 osd_axi_sel;

	struct meson_drm *priv;
	struct meson_vpu_block **mvbs;
	u32 vpu_blocks_type;
	int num_blocks;
	/* save runtime crtc index of osd&video */
	int osd_crtc_index[MESON_MAX_OSDS];
	int video_crtc_index[MESON_MAX_VIDEOS];
};

struct meson_vpu_stack {
	int num_blocks;
	int top;
	struct meson_vpu_block *stack[MESON_MAX_BLOCKS];
};

struct meson_vpu_traverse {
	struct meson_vpu_block *path[MAX_DFS_PATH_NUM][MESON_MAX_BLOCKS];
	int num_path;
};

struct meson_vpu_sub_pipeline_state {
	struct drm_private_state obj;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd_layer_info plane_info[MESON_MAX_OSDS];
	struct meson_vpu_video_layer_info video_plane_info[MESON_MAX_VIDEOS];
	u64 enable_blocks;
	u32 index;
	u32 num_plane;
	u32 num_plane_video;
	u32 osd_out_hsize_raw;
	u32 osd_out_hsize_real;
	u32 scaler_din_hsize[MESON_MAX_OSDS];
	u32 scaler_dout_hsize[MESON_MAX_OSDS];
	u32 scaler_din_vsize[MESON_MAX_OSDS];
	u32 scaler_dout_vsize[MESON_MAX_OSDS];
	u32 slice_dout_hsize[MESON_MAX_OSDS];
	u32 hwincut_bgn[MESON_MAX_OSDS];
	u32 hwincut_end[MESON_MAX_OSDS];
	u32 slice_x_st[MESON_MAX_OSDS];
	u32 slice_x_end[MESON_MAX_OSDS];
	u32 init_phase[MESON_MAX_OSDS];
	u32 blend_dout_hsize[2];
	u32 blend_dout_vsize[2];
	u32 slice2ppc_hsize;
	u32 slice2ppc_vsize;
	u32 zorder_plane_index[MESON_MAX_OSDS];
	u32 ratio_plane_index[MESON_MAX_OSDS];
	struct meson_vpu_scaler_param scaler_param[MESON_MAX_SCALERS];
	/*pre_osd_scope is before DIN*/
	struct osd_scope_s osd_scope_pre[MAX_DIN_NUM];
	/*used for gfcd odd input width*/
	u32 osd_scope_width_offset[MAX_DIN_NUM];
	u32 osdblend_input_width_offset;
	int vpp_scope_x;
	int vpp_scope_y;
	int more_4k;
	int more_60;
	u32 osdblend_output_width;
	u32 osdblend_output_height;

	/*some traverse help structure*/
	struct meson_vpu_stack osd_stack[MESON_MAX_OSDS];

	/*store traverse result for every path*/
	struct meson_vpu_traverse osd_traverse[MESON_MAX_OSDS];

	u32 plane_index[MESON_MAX_OSDS];
	u32 video_plane_index[MESON_MAX_VIDEOS];
	u32 din_index[MAX_DIN_NUM];
	u32 dout_index[MAX_DIN_NUM];
	u32 scaler_cnt[MAX_DIN_NUM];
	struct meson_vpu_block *scale_blk[MESON_MAX_OSDS][MESON_MAX_SCALERS];
	u32 dout_zorder[MAX_DOUT_NUM];
	u32 global_afbc;
	int sec_src;
	int vsync_enabled;
	enum drm_pm_state pm_state;
};

struct meson_video_sub_pipeline_state {
	struct drm_private_state obj;
	int index;
	struct meson_vpu_pipeline *pipe;
	struct meson_video_sub_pipeline *sub_pipe;
	struct meson_vpu_video_layer_info video_plane_info[MESON_MAX_VIDEOS];
	u32 num_plane_video;
	u64 enable_blocks;
	u32 video_plane_index[MESON_MAX_VIDEOS];
};

#define to_osd_block(x) container_of(x, struct meson_vpu_osd, base)
#define to_afbc_block(x) container_of(x, struct meson_vpu_afbc, base)
#define to_scaler_block(x) container_of(x, struct meson_vpu_scaler, base)
#define to_osdblend_block(x) container_of(x, struct meson_vpu_osdblend, base)
#define to_hdr_block(x) container_of(x, struct meson_vpu_hdr, base)
#define to_db_block(x) container_of(x, struct meson_vpu_db, base)
#define to_postblend_block(x) container_of(x, struct meson_vpu_postblend, base)
#define to_video_block(x) container_of(x, struct meson_vpu_video, base)
#define to_slice2ppc_block(x) container_of(x, struct meson_vpu_slice2ppc, base)
#define to_gfcd_block(x) container_of(x, struct meson_vpu_gfcd, base)
#define to_csc_block(x) container_of(x, struct meson_vpu_csc, base)

#define to_osd_state(x) container_of(x, struct meson_vpu_osd_state, base)
#define to_afbc_state(x) container_of(x, struct meson_vpu_afbc_state, base)
#define to_scaler_state(x) container_of(x, struct meson_vpu_scaler_state, base)
#define to_osdblend_state(x) container_of(x, \
		struct meson_vpu_osdblend_state, base)
#define to_hdr_state(x) container_of(x, struct meson_vpu_hdr_state, base)
#define to_db_state(x) container_of(x, struct meson_vpu_db_state, base)
#define to_postblend_state(x) container_of(x, \
		struct meson_vpu_postblend_state, base)
#define to_slice2ppc_state(x) container_of(x, \
		struct meson_vpu_slice2ppc_state, base)
#define to_video_state(x) container_of(x, struct meson_vpu_video_state, base)
#define to_gfcd_state(x) container_of(x, struct meson_vpu_gfcd_state, base)
#define to_csc_state(x) container_of(x, struct meson_vpu_csc_state, base)

#define priv_to_block(x) container_of(x, struct meson_vpu_block, obj)
#define priv_to_block_state(x) container_of(x, \
		struct meson_vpu_block_state, obj)
#define priv_to_pipeline(x) container_of(x, struct meson_vpu_pipeline, obj)
#define priv_to_sub_pipeline(x) container_of(x, struct meson_vpu_sub_pipeline, obj)
#define priv_to_sub_pipeline_state(x) container_of(x, \
				struct meson_vpu_sub_pipeline_state, obj)
#define priv_to_video_sub_pipe(x) container_of(x, struct meson_video_sub_pipeline, obj)
#define priv_to_video_sub_pipe_state(x) container_of(x, \
		struct meson_video_sub_pipeline_state, obj)

int video_pipeline_block_check(struct meson_video_sub_pipeline *pipeline,
		       struct drm_atomic_state *state, int plane_index);
int video_pipeline_block_update(struct meson_video_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state, int plane_index);
int vpu_pipeline_osd_check(struct meson_vpu_sub_pipeline *sub_pipeline,
		       struct drm_atomic_state *state);
int vpu_pipeline_osd_update(struct meson_vpu_sub_pipeline *sub_pipeline,
			struct drm_atomic_state *old_state);

int vpu_topology_populate(struct meson_vpu_pipeline *pipeline);
int vpu_topology_init(struct platform_device *pdev, struct meson_drm *private);
int vpu_pipeline_check(struct meson_vpu_sub_pipeline *pipeline,
		       struct drm_atomic_state *state);

int vpu_osd_pipeline_update(struct meson_vpu_sub_pipeline *pipeline,
			struct drm_atomic_state *old_state);
void vpu_pipeline_init(struct meson_vpu_pipeline *pipeline);
void vpu_pipeline_fini(struct meson_vpu_pipeline *pipeline);
void vpu_pipeline_resume_init(struct meson_vpu_pipeline *pipeline);

int vpu_pipeline_read_scanout_pos(struct meson_vpu_pipeline *pipeline,
			int *vpos, int *hpos, int crtc_index);
void vpu_pipeline_prepare_update(struct meson_vpu_pipeline *pipeline,
	int vdisplay, int vrefresh, int crtc_index);
void vpu_pipeline_finish_update(struct meson_vpu_pipeline *pipeline, int crtc_index);
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
void video_dummy_data_set(u64 crtc_bgcolor, bool crtc_bgcolor_flag);
#endif
/* meson_vpu_pipeline_private.c */
struct meson_vpu_block_state *
meson_vpu_block_get_state(struct meson_vpu_block *block,
			  struct drm_atomic_state *state);
struct meson_vpu_block_state *
meson_vpu_block_get_old_state(struct meson_vpu_block *mvb,
			struct drm_atomic_state *state);
struct meson_vpu_block_state *
meson_vpu_block_get_new_state(struct meson_vpu_block *mvb,
			struct drm_atomic_state *state);
struct meson_vpu_sub_pipeline_state *
meson_vpu_pipeline_get_state(struct meson_vpu_sub_pipeline *sub_pipeline,
			     struct drm_atomic_state *state);

struct meson_video_sub_pipeline_state *
meson_video_pipeline_get_old_state(struct meson_video_sub_pipeline *video_pipe,
			     struct drm_atomic_state *state);
struct meson_video_sub_pipeline_state *
meson_video_pipeline_get_new_state(struct meson_video_sub_pipeline *video_pipe,
			     struct drm_atomic_state *state);
struct meson_video_sub_pipeline_state *
meson_video_pipeline_get_state(struct meson_video_sub_pipeline *video_pipe,
			     struct drm_atomic_state *state);

int meson_vpu_block_state_init(struct meson_drm *private,
			       struct meson_vpu_pipeline *pipeline);

int combination_traverse(struct meson_vpu_sub_pipeline_state *mvps,
			 struct drm_atomic_state *state);
int vpu_pipeline_traverse(struct meson_vpu_sub_pipeline_state *mvps,
			  struct drm_atomic_state *state);
int vpu_pipeline_check_osdblend(u32 *out_port, int num_planes,
				struct meson_vpu_sub_pipeline_state *mvps,
					struct drm_atomic_state *state);
int video_pipeline_check_block(struct meson_video_sub_pipeline_state *mvps,
				   struct drm_atomic_state *state, int plane_index);
void vpu_pipeline_check_finish_reg(int crtc_index, struct meson_drm *priv);
void vpu_pipeline_detect_reset(struct meson_vpu_sub_pipeline *sub_pipeline);
void vpu_pipeline_detect_status(struct meson_vpu_sub_pipeline *sub_pipeline);

struct meson_vpu_sub_pipeline_state *
meson_vpu_pipeline_get_old_state(struct meson_vpu_sub_pipeline *pipeline,
			     struct drm_atomic_state *state);
struct meson_vpu_sub_pipeline_state *
meson_vpu_pipeline_get_new_state(struct meson_vpu_sub_pipeline *pipeline,
			     struct drm_atomic_state *state);
void sort_osd_by_zorder(struct osd_zorder_s *din, u32 osd_num);
const struct meson_drm_format_info *meson_drm_format_info(struct meson_vpu_block *vblk, u32 format,
							  bool afbc_en);

void meson_commit_reenter_inc(struct meson_drm *priv,
			int crtc_index, int flag);
void meson_commit_reenter_dec(struct meson_drm *priv,
			int crtc_index, int flag);

extern struct rdma_reg_ops common_reg_ops[3];
extern struct rdma_reg_ops g12b_reg_ops[2];
extern struct rdma_reg_ops c5_reg_ops[1];

/*
 * for g12a/s4/s4d/sc2 soc
 */
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
extern struct meson_vpu_block_ops video_ops;
#endif
extern struct meson_vpu_block_ops osd_ops;
extern struct meson_vpu_block_ops afbc_ops;
extern struct meson_vpu_block_ops scaler_ops;
extern struct meson_vpu_block_ops osdblend_ops;
extern struct meson_vpu_block_ops hdr_ops;
extern struct meson_vpu_block_ops db_ops;
extern struct meson_vpu_block_ops postblend_ops;
extern struct meson_vpu_block_ops csc_ops;

/*
 * for g12b/sm1 soc with dual display
 */
extern struct meson_vpu_block_ops g12b_osd_ops;
extern struct meson_vpu_block_ops g12b_postblend_ops;

/*
 * for t7/t3/t5w/t5m soc with multi encoder
 */
extern struct meson_vpu_block_ops t7_osd_ops;
extern struct meson_vpu_block_ops t7_afbc_ops;
extern struct meson_vpu_block_ops t7_hdr_ops;
extern struct meson_vpu_block_ops t7_postblend_ops;
extern struct meson_vpu_block_ops t3_afbc_ops;
extern struct meson_vpu_block_ops t3_scaler_ops;
extern struct meson_vpu_block_ops t3_postblend_ops;
extern struct meson_vpu_block_ops t5w_postblend_ops;


/*
 * for s5/t3x soc with slice block
 */
extern struct meson_vpu_block_ops s5_osd_ops;
extern struct meson_vpu_block_ops s5_afbc_ops;
extern struct meson_vpu_block_ops s5_scaler_ops;
extern struct meson_vpu_block_ops s5_hdr_ops;
extern struct meson_vpu_block_ops s5_osdblend_ops;
extern struct meson_vpu_block_ops s5_postblend_ops;
extern struct meson_vpu_block_ops slice2ppc_ops;
extern struct meson_vpu_block_ops t3x_osd_ops;
extern struct meson_vpu_block_ops t3x_osdblend_ops;
extern struct meson_vpu_block_ops t3x_afbc_ops;
extern struct meson_vpu_block_ops t3x_postblend_ops;

/*
 * for s1a/txhd2 soc
 */
extern struct meson_vpu_block_ops s1a_osd_ops;
extern struct meson_vpu_block_ops txhd2_osd_ops;
extern struct meson_vpu_block_ops txhd2_osdblend_ops;
extern struct meson_vpu_block_ops txhd2_postblend_ops;

/*
 * for s7 soc
 */
extern struct meson_vpu_block_ops s7_osd_ops;
extern struct meson_vpu_block_ops s7_afbc_ops;
extern struct meson_vpu_block_ops s7_scaler_ops;
extern struct meson_vpu_block_ops s7_postblend_ops;

/*
 * for s7d soc
 */
extern struct meson_vpu_block_ops gfcd_ops;
extern struct meson_vpu_block_ops s7d_hdr_ops;
extern struct meson_vpu_block_ops s7d_osd_ops;
extern struct meson_vpu_block_ops s7d_afbc_ops;
extern struct meson_vpu_block_ops s7d_osdblend_ops;

/*
 * for s6 soc
 */
extern struct meson_vpu_block_ops s6_osd_ops;
extern struct meson_vpu_block_ops s6_scaler_ops;
extern struct meson_vpu_block_ops s6_afbc_ops;
extern struct meson_vpu_block_ops s6_postblend_ops;

/*
 * for t6d/t6w soc
 */
extern struct meson_vpu_block_ops t6d_osd_ops;
extern struct meson_vpu_block_ops t6d_scaler_ops;
extern struct meson_vpu_block_ops t6d_postblend_ops;
extern struct meson_vpu_block_ops t6w_osdblend_ops;

/*
 * for C5 soc
 */
extern struct meson_vpu_block_ops c5_osd_ops;
extern struct meson_vpu_block_ops c5_scaler_ops;
extern struct meson_vpu_block_ops c5_postblend_ops;

extern struct meson_vpu_pipeline_ops g12a_vpu_pipeline_ops;
extern struct meson_vpu_pipeline_ops t7_vpu_pipeline_ops;
extern struct meson_vpu_pipeline_ops s5_vpu_pipeline_ops;

extern struct meson_plane_supported_formats osd_formats;
extern struct meson_plane_supported_formats osd_formats_t3x;
extern struct meson_plane_supported_formats osd_formats_t5m;
extern struct meson_plane_supported_formats osd_formats_s1a;
extern struct meson_plane_supported_formats osd_formats_s7d;
extern struct meson_plane_supported_formats osd_formats_t6d;
extern struct meson_plane_supported_formats video_formats;

extern struct meson_vpu_block_ops a9_osd_ops;
extern struct meson_vpu_block_ops a9_gfcd_ops;
extern struct meson_vpu_block_ops a9_scaler_ops;
extern struct meson_vpu_block_ops a9_postblend_ops;
extern struct meson_vpu_block_ops a9_osdblend_ops;

extern u32 s5_reg_table_cached[VPU_CACHED_REG_TABLE_SIZE];
extern u32 s7_reg_table_cached[VPU_CACHED_REG_TABLE_SIZE];

extern struct meson_drm_rdma_table rdma_tbl[MESON_MAX_CRTC];
int init_overwrite(struct device *dev);
#define MESON_MAX_OVERWRITE_REG 256
extern u32 *overwrite_reg;
extern u32 *overwrite_val;
extern u8 *overwrite_crtc_idx;
extern int overwrite_enable;
extern int reg_num;
#ifdef CONFIG_DEBUG_FS
void meson_crtc_debugfs_late_init(struct drm_crtc *crtc);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
extern void *osd_secure_op[VPP_TOP_MAX];
#endif

#endif
