/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_INTERLACE_H__
#define __DPSS_INTERLACE_H__

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/amlogic/media/vfm/vframe.h>

/**********************************************************
 * get DPSS driver's version:
 *	DPSS_DRV_V1	: t6w
 *	DPSS_DRV_V2	:
 **********************************************************/
#define DPSS_DRV_V1	(1)
#define DPSS_DRV_V2	(2)

//use get_status:unsigned int dpss_get_ver_flag(void);

/*dpss work mode bit */
#define DPSS_WORK_MODE_NR		0x01
#define DPSS_WORK_MODE_DDD		0x02
#define DPSS_WORK_MODE_LC_EVC		0x04
#define DPSS_WORK_MODE_DCT		0x08
#define DPSS_WORK_MODE_FRC		0x10
#define DPSS_WORK_MODE_HDR		0x20
#define DPSS_WORK_MODE_DI		0x40
#define DPSS_WORK_MODE_MAIN		0x80
#define DPSS_DD_CP_MODE			0x100
#define DPSS_WORK_MODE_BY_DPSS		0x800
#define DPSS_WORK_MODE_FRONT		0x8000

#define DPSS_PARA_UPDATE		0x1000000

#define DPSS_BYPASS_WIDTH 160
#define DPSS_BYPASS_HEIGHT 120

//----work mode 2-------
#define DPSS_DIRECT_MODE	0x1 // only for t6x P

enum dpss_buffer_mode {
	/*alloc output buffer by DPSS self*/
	DPSS_BUFFER_MODE_ALLOC_SELF = 0, //when input come
	DPSS_BUFFER_MODE_ALLOC_FAST = 1, //allocate when create
	DPSS_BUFFER_MODE_ALLOC_OTHER = 0x100,
};

enum nr_output_format {
	DPSS_OUTPUT_422 = 0,
	DPSS_OUTPUT_NV12 = 1,
	DPSS_OUTPUT_NV21 = 2,	/* ref to DIM_OUT_FORMAT_FIX_MASK */
	DPSS_OUTPUT_BYPASS	= 0x10000000, /*22-06-24*/
	DPSS_OUTPUT_TVP		= 0x20000000, /*21-03-02*/
	DPSS_OUTPUT_LINEAR	= 0x40000000,
	/*1:di output must linear, 0: determined by di,may be linear or block*/
	DPSS_OUTPUT_BY_DI_DEFINE	= 0x80000000,
	/* when use di's local buffer, can use this option,
	 * when use this option, blow define is no use
	 *	DI_OUTPUT_422
	 *	DI_OUTPUT_NV12
	 *	DI_OUTPUT_NV21
	 *	DI_OUTPUT_LINEAR
	 */
	DPSS_OUTPUT_MAX = 0x7FFFFFFF,
};

enum DPSS_CMD_ASY {
	DPSS_CMD_A_INS,
	DPSS_CMD_NR = 0x100,
	DPSS_CMD_FRC = 0x200,
	DPSS_CMD_DV = 0x300,
};

enum DPSS_STATE {
	DPSS_STATE_INS,
	DPSS_STATE_NR = 0x100,
	DPSS_STATE_FRC = 0x200,
	DPSS_STATE_DV = 0x300,
	DPSS_STATE_BUF = 0x400,
};

enum DPSS_ERRORTYPE {
	DPSS_ERR_NONE = 0,
	DPSS_ERR_UNDEFINED = 0x80000001,
	DPSS_ERR_REG_NO_IDLE_CH,
	DPSS_ERR_INDEX_OVERFLOW,
	DPSS_ERR_INDEX_NOT_ACTIVE,
	DPSS_ERR_IN_NO_SPACE,
	DPSS_ERR_UNSUPPORT,
	DPSS_ERR_MAX = 0x7FFFFFFF,
};

enum DPSS_VF_OWNER {
	DPSS_VF_OWNER_OTHER,
	DPSS_VF_OWNER_NR,
	DPSS_VF_OWNER_FRC,
	DPSS_VF_OWNER_ILLEGAL,
};

enum DPSS_BUF_STATE {
	DPSS_BUF_STATE_UNREADY,
	DPSS_BUF_STATE_READY,
	DPSS_BUF_STATE_FAIL,
	DPSS_BUF_STATE_MAX,
};

//cmd_asy  cmd
#define DPSS_CMD_ASY_WORKMODE	(1)
#define DPSS_CMD_ASY_2_WORKMODE	(2) // weak mode, not cause resolution change
#define DPSS_CMD_ENABLE_FRC	(3)

//...
#define DPSS_FLG_BYPASS	          (0x00001) //this flag is set on input vframe
#define DPSS_FLG_DDD	          (0x00002)
#define DPSS_FLG_HDR	          (0x00004)
#define DPSS_FLG_OUT_DONE         (0x00008) //this flag is set in pipeline means do dpss done
#define DPSS_FLG_OUT_BYPASS       (0x00010) //this flag is set in pipeline means not do dpss
#define DPSS_FLG_MODULE_BYPASS    (0x00020) //this flag is set in pipeline means bypass dpss module
#define DPSS_FLG_SET_DPSS_PROCESS (0x00040) //this flag is set in dpss_process means receive the vf
#define DPSS_FLG_HDR_CHG          (0x00080)
#define DPSS_FLG_FRONT	(0x00100)
#define DPSS_FLG_NEW_TRIG	      (0x10000) //trig re-setting by input
/************************************************/

struct dpss_in_vf_info {
	u32 flag; /*etc.*/
	unsigned int idx_m;
};

struct dpss_out_vf_info {
	u32 flag; /*etc.*/
	u32 crc;
	unsigned int idx_m;
	unsigned int idx_s;
	unsigned int idx_f;
};

/************************************************/
struct dpss_vf_s_buf_s {
	//yuv422 10 yuv 420 10...
	//afbc or not
	unsigned char q_type;
	unsigned char q_index;
	unsigned char type;
	unsigned char plane_num;
	u32 width;
	u32 height;
	u32 compWidth;
	u32 compHeight;
	struct canvas_config_s canvas0_config[2];
};

/************************************************/

struct dpss_sub_init_parm { //di_init_parm
	enum dpss_buffer_mode buffer_mode;
	enum nr_output_format output_format;
	unsigned int width;
	unsigned int height;
	unsigned int is_interlace;
	unsigned int rotate_flag;    //VFRAME_FLAG_MIRROR_H or VFRAME_FLAG_MIRROR_V
	unsigned int duration;
};

struct frc_init_parm {
	enum dpss_buffer_mode buffer_mode;
	bool do_game_frc;
	unsigned int width;
	unsigned int height;
	unsigned int duration;
	unsigned int in_frm_rate;
	unsigned int out_frm_rate;
};

struct lc_evc_init_parm {
};

/*defind by dpss caller*/
enum INPUT_VF_STATUS {
	DPSS_INPUT_VF_NONE = 0,
	DPSS_INPUT_VF_IN_DPSS = 1,
	DPSS_INPUT_VF_OUT_DPSS = 2,
};

struct pp_info_t {
	u32 flag;
	bool queued;
	bool dropped;
	bool dummy;
	struct file *src_file;
	u32 status;
	u32 main_index;  /*for dpss in, make sure increment*/
};

/*vf->dpss_flg*/
#define VFRAME_DPSS_FLAG_BYPASS		1

struct dpss_operations_s {
	/*The input data has been processed and returned to the caller*/
	void *arg;
	enum DPSS_ERRORTYPE (*empty_input_done)(void *arg, struct vframe_s *vfm);
	/* The output buffer has filled in the data and returned it
	 * to the caller.
	 */
	enum DPSS_ERRORTYPE (*fill_output_done)(void *arg, struct vframe_s *vfm);
	enum DPSS_ERRORTYPE (*get_input_vf_info)(void *arg, struct vframe_s *vfm,
				struct dpss_in_vf_info *status);
	enum DPSS_ERRORTYPE (*clear_data)(void *arg, struct vframe_s *vfm);
};

struct dpss_init_parm {
	u32 dps_work_mode;
	void *caller_data;
	struct dpss_sub_init_parm di_parm; //di_init_parm
	struct frc_init_parm frc_parm;
	struct lc_evc_init_parm lc_evc_parm;
	struct dpss_operations_s ops;
};

/**********************************************************
 * If flag is DI_BUFFERFLAG_ENDOFFRAME,
 * di_buffer is invalid,
 * only notify di that there will be no more input
 *********************************************************/
#define DI_BUFFERFLAG_ENDOFFRAME 0x00000001

#ifdef _HIS_CODE_
struct ins_mng_s {	/*ary add*/
	unsigned int	code;
	unsigned int	ch;
	unsigned int	index;

	unsigned int	type;
	struct page	*pages;
};

#define DIM_OUT_FORMAT_FIX_MASK		(0xffff)
#endif /* _HIS_CODE_ */

struct dpss_status {
	unsigned int status;
	enum DPSS_BUF_STATE buf_status;
	unsigned int game_mode;
	unsigned int game_frc_locked;
};

struct dpss_cmd_a_s {
	unsigned int para;
};

/**
 * @brief  dpss_create_instance  creat dps instance
 *
 * @param[in]  parm    Pointer of parm structure
 *
 * @return      dps index for success, or fail type if < 0
 */
int dpss_create_instance(struct dpss_init_parm *parm);

/**
 * @brief  dpss_set_parameter  set parameter to dps for init
 *
 * @param[in]  index   instance index
 *
 * @return      0 for success, or fail type if < 0
 */

int dpss_destroy_instance(int index);

/**
 * @brief  dpss_empty_input_buffer  send input date to dps
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 *
 * @return      Success or fail type
 */
enum DPSS_ERRORTYPE dpss_empty_input_buffer(int index, struct vframe_s *vfm);

/**
 * @brief  dpss_fill_output_buffer  send output buffer to dps
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 * @ 2019-12-30:discuss with jintao
 * @ when pre + post + local mode,
 * @	use this function put post buf to dps
 * @return      Success or fail type
 */

enum DPSS_ERRORTYPE dpss_fill_output_buffer(int index, struct vframe_s *vfm);

/**
 * @brief  dpss_empty_out_buffer  send output buffer to dps
 *
 * @param[in]  index   instance index
 * @param[in]  buffer  Pointer of buffer structure
 * @	use this function put post buf to dpss
 * @		-canvas0_config[0].width : to cfg
 * @		-canvas0_config[0].phy_addr : address
 * @		-canvas0_config[0].block_mode = 0 fix
 * @		-canvas0_config[0].endian = 0 fix
 * @return      Success or fail type
 */
enum DPSS_ERRORTYPE dpss_empty_out_buffer(int index, struct vframe_s *vfm);

/**
 * @brief  dpss_get_state  Get the state of di by instance index
 *
 * @param[in]   index   instance index
 * @param[in]  cmd_state  cmd
 * @param[in]  vfm  Pointer of vframe, option
 * @param[out]  state  return state

 * @return      0 for success, or fail type if < 0
 */
int dpss_get_state(int index, enum DPSS_STATE cmd_state,
		   struct vframe_s *vfm, struct dpss_status *state);

/**
 * @brief  dpss_asy_cmd
 *
 * @param[in]  index        channel
 * @param[in]  cmd_asy  cmd
 * @param[in]  para  parameter
 *
 * @return      0 for success, or fail type if < 0
 * ex:
 *	cmd_asy:DPSS_CMD_ASY_2_WORKMODE, para->para = DPSS_DIRECT_MODE
 */
int dpss_cmd_asy(int index, unsigned int cmd_asy, struct dpss_cmd_a_s *para);

/**
 * @brief  dpss_release_keep_buf  release buf after instance destroy
 *
 * @param[in]  buffer        di output buffer
 *
 * @return      0 for success, or fail type if < 0
 */
int dpss_release_keep_buf(struct vframe_s *vfm);

/**
 * @brief  dpss_get_vf_info  after fill_output_done to get more info
 *
 * @param[in]  vf        vf from fill_output_done
 *
 * @param[out]  dpss_vf_info     same info not in vf
 *
 * @return      0 for success, or fail type if < 0
 */
int dpss_get_vf_info(struct vframe_s *vf, struct dpss_out_vf_info *dpss_out_info);

/**
 * @brief  dpss_bypass
 *
 * @param[in]  vfm
 *
 * @return      0 for do dpss,or bypass dpss
 */

unsigned char  dpss_is_bypass(struct dpss_init_parm *parm, struct vframe_s *vf);
bool check_need_do_game_frc(struct dpss_init_parm *parm, struct vframe_s *vf);
void dpss_game_frc_lock(struct dpss_init_parm *parm);

//dpss to-do: vpp link api

int DPSS_A_WR_MPEG_REG(u32 adr, u32 val);
int DPSS_A_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 DPSS_A_RD_MPEG_REG(u32 adr);
int DPSS_B_WR_MPEG_REG(u32 adr, u32 val);
int DPSS_B_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 DPSS_B_RD_MPEG_REG(u32 adr);
void dpss_v_rdma_config_a(void);
void dpss_v_rdma_config_b(void);

//frc-dp:Fast forward video
void get_eos_from_dp(bool en);
//detect speed-up playback
void dp_dynamic_detect_play_speed(bool en);

#endif	/*__DPSS_INTERLACE_H__*/
