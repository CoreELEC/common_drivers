/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_HDCP_H__
#define __HDMITX_HW_HDCP_H__

bool hdcptx_ddc_check_busy(void);
void hdcptx_init_reg(void);
u8 hdcptx1_ds_cap_status_get(void);
u16 hdcptx1_get_prime_ri(void);
u8 hdcptx1_ksv_v_get(void);
void hdcptx1_query_aksv(struct hdcp_ksv_t *p_val);
void hdcptx1_protection_enable(bool en);
void hdcptx1_intermed_ri_check_enable(bool en);
void hdcptx1_encryption_update(bool en);
void hdcptx1_auth_start(void);
void hdcptx1_auth_stop(void);
u8 hdcptx1_copp_status_get(void);
void hdcptx1_bcaps_get(u8 *p_bcaps_status);
bool hdcptx1_ds_rptr_capability(void);
void hdcptx1_ds_an_read(u8 *p_an, u8 an_bytes);
void hdcptx1_ds_aksv_read(u8 *p_aksv, u8 ksv_bytes);
void hdcptx1_bstatus_get(u8 *p_ds_bstatus);
void hdcptx1_get_ds_ksvlists(u8 **p_ksv, u8 count);
bool hdcptx1_load_key(void);
void hdcptx1_ds_bksv_read(struct hdcptx_common *hdcptx_comm, u8 *p_bksv, u8 ksv_bytes);

bool hdcptx2_ds_rptr_capability(void);
void hdcptx2_encryption_update(bool en);
void hdcptx2_csm_send(struct hdcp_csm_t *csm_msg);
void hdcptx2_rpt_smng_xfer_start(void);
bool hdcptx2_auth_status(void);
void hdcptx2_auth_stop(void);
void hdcptx2_reauth_send(void);
u8 hdcptx2_topology_get(void);
u8 hdcptx2_rpt_dev_cnt_get(void);
u8 hdcptx2_rpt_depth_get(void);
void hdcptx2_ds_rpt_rcvid_list_read(u8 *p_rpt_rcv_id, u8 dev_count, u8 bytes_to_read);
void hdcptx2_ds_rcv_id_read(u8 *p_rcv_id);
void hdcptx2_src_auth_start(u8 content_type);
u8 hdcp2x_get_state_st(void);
void hdcptx_en_aes_dual_pipe(bool en);
void hdcptx_ddc_toggle_sw_tpi(void);
bool get_hdcptx_lstore(struct hdcptx_common *hdcptx_comm, int hdcp_type);
bool get_hdcptx_result(struct hdcptx_common *hdcptx_comm, int hdcp_type);
bool hdcptx22_topo_ctrl(struct hdcptx_common *hdcptx_comm, int cmd, int topo_type);
int hdcptx_get_hdcprx_ver(struct hdcptx_common *hdcptx_comm);
/* hdcp clk gate related */
void hdmitx21_ctrl_hdcp_gate(enum amhdmitx_chip_e chip_type, int hdcp_mode, bool en);
u32 hdmitx21_get_gate_status(void);

#endif
