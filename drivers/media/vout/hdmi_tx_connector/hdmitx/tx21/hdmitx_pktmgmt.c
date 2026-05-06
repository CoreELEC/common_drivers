// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <linux/compiler.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>

#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>

#include "hdmitx_hw_platform.h"
#include "hdmitx_hw_core.h"
#include "hdmitx_packet.h"
#include "hdmi_tx_connector/hdmitx_log.h"

static DEFINE_SPINLOCK(tpi_lock);
static void tpi_info_send(u8 sel, u8 *data, bool no_chksum_flag)
{
	u8 checksum = 0;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		for (i = 0; i < 31; i++)
			hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, 0);

		hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0);
		spin_unlock_irqrestore(&tpi_lock, flags);
		return;
	}

	if (!no_chksum_flag) {
		/* do checksum */
		data[3] = 0;
		for (i = 0; i < 31; i++)
			checksum += data[i];
		checksum = 0 - checksum;
		data[3] = checksum;
	}

	for (i = 0; i < 31; i++)
		hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, data[i]);
	hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0xe0); //TPI Info enable
	spin_unlock_irqrestore(&tpi_lock, flags);
}

/* packet no need checksum */
static void tpi_packet_send(u8 sel, u8 *data)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		for (i = 0; i < 31; i++)
			hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, 0);

		hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0);
		spin_unlock_irqrestore(&tpi_lock, flags);
		return;
	}

	for (i = 0; i < 31; i++)
		hdmitx21_wr_reg(TPI_INFO_B0_IVCTX + i, data[i]);
	hdmitx21_wr_reg(TPI_INFO_EN_IVCTX, 0xe0); //TPI Info enable
	spin_unlock_irqrestore(&tpi_lock, flags);
}

static int tpi_info_get(u8 sel, u8 *data)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&tpi_lock, flags);
	hdmitx21_wr_reg(TPI_INFO_FSEL_IVCTX, sel); // buf selection
	if (!data) {
		spin_unlock_irqrestore(&tpi_lock, flags);
		return -1;
	}
	if (hdmitx21_rd_reg(TPI_INFO_EN_IVCTX) == 0) {
		spin_unlock_irqrestore(&tpi_lock, flags);
		return 0;
	}
	for (i = 0; i < 31; i++)
		data[i] = hdmitx21_rd_reg(TPI_INFO_B0_IVCTX + i);
	data[31] = hdmitx21_rd_reg(TPI_INFO_EN_IVCTX);
	spin_unlock_irqrestore(&tpi_lock, flags);
	return 32; /* fixed value */
}

void hdmitx_dump_infoframe_packets(struct seq_file *s)
{
	int i, j;
	u8 body[32] = {0};

	for (i = 0; i < 17; i++) {
		tpi_info_get(i, body);
		body[31] = hdmitx21_rd_reg(TPI_INFO_EN_IVCTX);
		seq_printf(s, "dump hdmi infoframe[%d]\n", i);
		for (j = 0; j < 32; j += 8)
			seq_printf(s, "%02x%02x%02x%02x%02x%02x%02x%02x\n",
				body[j + 0], body[j + 1],
				body[j + 2], body[j + 3],
				body[j + 4], body[j + 5],
				body[j + 6], body[j + 7]);
		memset(body, 0, sizeof(body));
	}
}

static int _tpi_infoframe_wrrd(u8 wr, u16 info_type, u8 *body)
{
	u8 sel;
	bool no_chksum_flag = 0;

	/* the bank index is fixed */
	switch (info_type) {
	case HDMI_INFOFRAME_TYPE_AVI:
		sel = 0;
		break;
	case HDMI_INFOFRAME_EMP_VRR_GAME:
	case HDMI_INFOFRAME_EMP_VRR_QMS:
		sel = 1;
		no_chksum_flag = 1;
		break;
	case HDMI_INFOFRAME_TYPE_AUDIO:
		sel = 2;
		break;
	case HDMI_INFOFRAME_TYPE_SPD:
		sel = 3;
		break;
	case HDMI_INFOFRAME_EMP_VRR_SBTM:
		sel = 4;
		no_chksum_flag = 1;
		break;
	/* used for DV_VSIF / HDMI1.4b_VSIF */
	case HDMI_INFOFRAME_TYPE_VENDOR:
		sel = 5;
		break;
	case HDMI_INFOFRAME_TYPE_DRM:
		sel = 6;
		break;
	case HDMI_PACKET_TYPE_GCP:
		sel = 7;
		tpi_packet_send(sel, body);
		return 0;
	/* specially for HF-VSIF */
	case HDMI_INFOFRAME_TYPE_VENDOR2:
		sel = 8;
		break;
	case HDMI_INFOFRAME_TYPE_EMP:
		sel = 9;
		no_chksum_flag = 1;
		break;
	case HDMI_INFOFRAME_TYPE_SBTM:
		sel = 10;
		no_chksum_flag = 1;
		break;
	default:
		HDMITX_INFO("%s[%d] wrong info_type %d\n", __func__, __LINE__, info_type);
		return -1;
	}

	if (wr) {
		if (!body) {
			tpi_info_send(sel, NULL, no_chksum_flag);
			return 0;
		}
		/* do checksum */
		tpi_info_send(sel, body, no_chksum_flag);
		return 0;
	} else {
		return tpi_info_get(sel, body);
	}
}

void hdmitx_infoframe_send(u16 info_type, u8 *body)
{
	_tpi_infoframe_wrrd(1, info_type, body);
}

int hdmitx_infoframe_raw_get(u16 info_type, u8 *body)
{
	return _tpi_infoframe_wrrd(0, info_type, body);
}

#ifdef CONFIG_AMLOGIC_DSC
/* refer to VESA-DSC-1.2a.pdf Page 50 */
static void pps_data_map(u8 *body, struct dsc_pps_data_s *pps,
	struct hdmi_timing *timing)
{
	int i;
	int j;

	if (!body || !pps)
		return;

	body[0] = ((pps->dsc_version_major & 0xf) << 4) |
		((pps->dsc_version_minor & 0xf) << 0);
	body[1] = pps->pps_identifier & 0xff;
	body[2] = 0x00; /* RESERVED */
	body[3] = ((pps->bits_per_component & 0xf) << 4) |
		((pps->line_buf_depth & 0xf) << 0);
	body[4] = ((pps->block_pred_enable & 0x1) << 5) |
		((pps->convert_rgb & 0x1) << 4) |
		((pps->simple_422 & 0x1) << 3) |
		((pps->vbr_enable & 0x1) << 2) |
		(((pps->bits_per_pixel >> 8) & 0x3) << 0);
	body[5] = pps->bits_per_pixel & 0xff;
	body[6] = (pps->pic_height >> 8) & 0xff;
	body[7] = pps->pic_height & 0xff;
	body[8] = (pps->pic_width >> 8) & 0xff;
	body[9] = pps->pic_width & 0xff;
	body[10] = (pps->slice_height >> 8) & 0xff;
	body[11] = pps->slice_height & 0xff;
	body[12] = (pps->slice_width >> 8) & 0xff;
	body[13] = pps->slice_width & 0xff;
	body[14] = (pps->chunk_size >> 8) & 0xff;
	body[15] = pps->chunk_size & 0xff;
	body[16] = (pps->initial_xmit_delay >> 8) & 0x3;
	body[17] = pps->initial_xmit_delay & 0xff;
	body[18] = (pps->initial_dec_delay >> 8) & 0xff;
	body[19] = pps->initial_dec_delay & 0xff;
	body[20] = 0x00; /* RESERVED */
	body[21] = pps->initial_scale_value & 0x3f; /* RESERVED */
	body[22] = (pps->scale_increment_interval >> 8) & 0xff;
	body[23] = pps->scale_increment_interval & 0xff;
	body[24] = (pps->scale_decrement_interval >> 8) & 0xf;
	body[25] = pps->scale_decrement_interval & 0xff;
	body[26] = 0x00; /* RESERVED */
	body[27] = pps->first_line_bpg_offset & 0x1f;
	body[28] = (pps->nfl_bpg_offset >> 8) & 0xff;
	body[29] = pps->nfl_bpg_offset & 0xff;
	body[30] = (pps->slice_bpg_offset >> 8) & 0xff;
	body[31] = pps->slice_bpg_offset & 0xff;
	body[32] = (pps->initial_offset >> 8) & 0xff;
	body[33] = pps->initial_offset & 0xff;
	body[34] = (pps->final_offset >> 8) & 0xff;
	body[35] = pps->final_offset & 0xff;
	body[36] = pps->flatness_min_qp & 0x1f;
	body[37] = pps->flatness_max_qp & 0x1f;
	body[38] = (pps->rc_parameter_set.rc_model_size >> 8) & 0xff;
	body[39] = pps->rc_parameter_set.rc_model_size & 0xff;
	body[40] = pps->rc_parameter_set.rc_edge_factor & 0xf;
	body[41] = pps->rc_parameter_set.rc_quant_incr_limit0 & 0x1f;
	body[42] = pps->rc_parameter_set.rc_quant_incr_limit1 & 0x1f;
	body[43] = ((pps->rc_parameter_set.rc_tgt_offset_hi & 0xf) << 4) |
		((pps->rc_parameter_set.rc_tgt_offset_lo & 0xf) << 0);
	for (i = 44, j = 0; i < 58; i++, j++)
		body[i] = pps->rc_parameter_set.rc_buf_thresh[j];
	for (i = 58, j = 0; i < 88; i += 2, j++) {
		u8 min_qp = pps->rc_parameter_set.rc_range_parameters[j].range_min_qp & 0x1f;
		u8 max_qp = pps->rc_parameter_set.rc_range_parameters[j].range_max_qp & 0x1f;
		u8 bpg_offset =
			pps->rc_parameter_set.rc_range_parameters[j].range_bpg_offset & 0x3f;

		body[i] = (min_qp << 3) | ((max_qp >> 2) & 0x7);
		body[i + 1] = ((max_qp & 0x3) << 6) | bpg_offset;
	}
	body[88] = ((pps->native_420 & 0x1) << 1) |
		((pps->native_422 & 0x1) << 0);
	body[89] = pps->second_line_bpg_offset & 0x1f;
	body[90] = (pps->nsl_bpg_offset >> 8) & 0xff;
	body[91] = pps->nsl_bpg_offset & 0xff;
	body[92] = (pps->second_line_offset_adj >> 8) & 0xff;
	body[93] = pps->second_line_offset_adj & 0xff;
	for (i = 94; i < 128; i++)
		body[i] = 0x00; /* RESERVED */
	body[128] = timing->h_front & 0xff;
	body[129] = (timing->h_front >> 8) & 0xff;
	body[130] = timing->h_sync & 0xff;
	body[131] = (timing->h_sync >> 8) & 0xff;
	body[132] = timing->h_back & 0xff;
	body[133] = (timing->h_back >> 8) & 0xff;
	body[134] = pps->hc_active_bytes & 0xff;
	body[135] = (pps->hc_active_bytes >> 8) & 0xff;
}

/* send DSC packet */
void hdmitx_dsc_cvtem_pkt_send(struct dsc_pps_data_s *pps,
	struct hdmi_timing *timing)
{
	int i;
	u8 data8;
	//dsc total send 6 packet as one emp, the actual payload byte is 136
	const u16 dsc_byte_num = (5 - 1) * 28 + 24;
	//all 6 DSF, and the remain bytes in last DSF should be cleared
	const u16 dsc_pkt_insert = (6 - 1) * 28 + 21;
	u8 body[6 * 28];
	struct dsc_offer_tx_data *dsc_data = container_of(pps, struct dsc_offer_tx_data, pps_data);
	struct hdmitx21_dev *hdev = container_of(dsc_data, struct hdmitx21_dev, dsc_data);

	memset(body, 0, sizeof(body));
	pps_data_map(body, pps, timing);

	// step1: DSC timing
	// DSC Vertical Blanking Lines Register
	// total_lines_fapa = reg_vb_le[0] ? (reg_vb_le+1)/2 : reg_vb_le/2;  ????
	// ***FAPA invalid after total_lines_fapa vblank lines ***
	// or vsync neg_edge when reg_fapa_fsm_proper_move=1
	hdmitx21_wr_reg(DSC_PKT_VB_LE_IVCTX, timing->v_blank); //reg_vb_le
	hdmitx21_wr_reg(DSC_PKT_SPARE_3_IVCTX, 0x2); //[1] reg_fapa_fsm_proper_move

	// step2: prepare packet data
	// setting generate packet
	// setting payload
	//payload [15:8] ===> pb5 length msb
	hdmitx21_wr_reg(DSC_PKT_INSERT_PAYLOAD_1_IVCTX, dsc_byte_num >> 8);
	//payload [7:0]  ===> pb6 length lsb
	hdmitx21_wr_reg(DSC_PKT_INSERT_PAYLOAD_0_IVCTX, dsc_byte_num & 0xff);
	hdmitx21_wr_reg(DSC_PKT_GEN_CTL_IVCTX, 0xb1); //[0] reg_source_en [6:5] fapa ctrl
	//[3] reg_send_pkt_cont (send data set for each frame if it is 1)
	//[0] reg_send_end_data_set
	hdmitx21_wr_reg(DSC_PKT_INSERT_CTL_1_IVCTX, 0x8);
	data8  = 0;
	data8 |= (0 << 6); //[7 : 6] rsvd
	data8 |= (0 << 5); //[    5] reg_set_new_in_m1_ds_pkt
	data8 |= (0 << 4); //[    4] reg_set_new_in_middle
	data8 |= (0 << 3); //[    3] reg_set_new_always
	data8 |= (1 << 2); //[    2] reg_to_err_ctrl
	data8 |= (0 << 1); //[    1] feg_fapa1_support. =0:FAPA start only after second active line.
	data8 |= (1 << 0); //[    0] reg_set_end_for_ml_ds
	hdmitx21_wr_reg(DSC_PKT_INSERT_CTL_2_IVCTX, data8);

	//HDR DE: reg_act_de = {reg_act_de_msb,reg_act_de_lsb};
	// ***FAPA valid after reg_act_de active lines***
	hdmitx21_wr_reg(DSC_PKT_ACT_DE_LO_IVCTX, timing->v_active & 0xff); //reg_act_de_lsb
	hdmitx21_wr_reg(DSC_PKT_ACT_DE_HI_IVCTX, (timing->v_active >> 8) & 0xff); //reg_act_de_msb

	//  step3: packet header and content
	hdmitx21_wr_reg(DSC_PKT_EM_HB0_IVCTX, 0x7f); // HB0 header
	hdmitx21_wr_reg(DSC_PKT_EM_PB0_IVCTX, (1 << 2) | (1 << 1)); // pb0 [7] new; [6] end
	hdmitx21_wr_reg(DSC_PKT_EM_PB2_IVCTX, 0x01); // pb2 ID
	hdmitx21_wr_reg(DSC_PKT_EM_PB3_IVCTX, 0x00); // pb3
	hdmitx21_wr_reg(DSC_PKT_EM_PB4_IVCTX, 0x02); // pb4

	//============== update DSC data ================
	hdmitx21_wr_reg(DSC_PKT_INSERT_CTRL_IVCTX, 0x1); // [0] reg_pkt_gen; [1]reg_pkt_gen_en
	hdmitx21_wr_reg(DSC_PKT_MEM_WADDR_RST_IVCTX, 0);
	hdmitx21_wr_reg(DSC_PKT_MEM_WADDR_RST_IVCTX, 1);
	hdmitx21_wr_reg(DSC_PKT_MEM_WADDR_RST_IVCTX, 0);

	/* fix packet send */
	hdmitx21_set_reg_bits(DSC_PKT_GEN_CTL_IVCTX, 1, 3, 1);
	usleep_range(100, 110);
	hdmitx21_set_reg_bits(DSC_PKT_GEN_CTL_IVCTX, 0, 3, 1);

	for (i = 0; i < dsc_pkt_insert; i++) {
		if (hdev->emp_verbose)
			HDMITX_INFO("body[%d]=0x%02x\n", i, body[i]);
		hdmitx21_wr_reg(DSC_PKT_MEM_WDATA_IVCTX, body[i]);
	}
	hdev->emp_verbose = 0;
	hdmitx21_wr_reg(DSC_PKT_INSERT_CTRL_IVCTX, 0x3);

	/* only for pkt send in vsync */
	//hdmitx21_wr_reg(DSC_PKT_INSERT_CTRL_IVCTX, 0x0); // [0] reg_pkt_gen; [1]reg_pkt_gen_en
	//hdmitx21_set_reg_bits(PCLK2TMDS_MISC1_IVCTX, 1, 4, 1); //[4] reg_bypass_video_path
}

void hdmitx_dsc_cvtem_pkt_disable(void)
{
	hdmitx21_wr_reg(DSC_PKT_INSERT_CTRL_IVCTX, 0x0);
}

irqreturn_t hdmitx_emp_vsync_handler(struct hdmitx21_dev *hdev)
{
	struct dsc_offer_tx_data dsc_data;
	struct hdmi_timing *timing;

	if (!hdev->tx_comm.fmt_para.dsc_en || hdev->emp_no == 0)
		return IRQ_HANDLED;

	if (hdev->emp_no > 0)
		hdev->emp_no--;

	timing = &hdev->tx_comm.fmt_para.timing;
	hdmitx_get_dsc_data(&dsc_data);
	hdmitx_dsc_cvtem_pkt_send(&dsc_data.pps_data, timing);
	return IRQ_HANDLED;
}
#endif

/*
 * now this interface should be not used, otherwise need
 * adjust as hdmi_vend_infoframe_rawset fistly
 */
void hdmi_vend_infoframe_set(struct hdmitx_common *tx_comm, struct hdmi_vendor_infoframe *info)
{
	u8 body[31] = {0};

	if (!tx_comm) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	if (!info) {
		if (tx_comm->rxcap.ifdb_present)
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
		else
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, NULL);
		return;
	}

	hdmi_vendor_infoframe_pack(info, body, sizeof(body));
	if (tx_comm->rxcap.ifdb_present)
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
	else
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, body);
}

/*
 * refer to DV Consumer Decoder for Source Devices
 * System Development Guide Kit version chapter 4.4.8 Game
 * content signaling:
 * 1.if DV sink device that supports ALLM with
 * InfoFrame Data Block (IFDB), HF-VSIF with ALLM_Mode = 1
 * should comes after Dolby VSIF with L11_MD_Present = 1 and
 * Content_Type[3:0] = 0x2(case B1)
 * 2.DV sink device that supports ALLM without
 * InfoFrame Data Block (IFDB), Dolby VSIF with L11_MD_Present
 * = 1 and Content_Type[3:0] = 0x2 should comes after HF-VSIF
 * with  ALLM_Mode = 1(case B2), or should only send Dolby VSIF,
 * not send HF-VSIF(case A)
 */
/* only used for DV_VSIF / HDMI1.4b_VSIF / CUVA_VSIF / HDR10+ VSIF */
void hdmi_vend_infoframe_rawset(struct hdmitx_common *tx_comm, u8 *body)
{
	if (!tx_comm) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	if (!body) {
		if (!tx_comm->rxcap.ifdb_present)
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, NULL);
		else
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
		return;
	}

	if (tx_comm->rxcap.ifdb_present &&
		tx_comm->rxcap.additional_vsif_num >= 1) {
		/* dolby cts case93 B1 */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
	} else if (!tx_comm->rxcap.ifdb_present) {
		/* dolby cts case92 B2 */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	} else {
		/* case89 ifdb_present but no additional_vsif, should not send HF-VSIF */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, NULL);
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
	}
}

/* only used for HF-VSIF */
void hdmi_vend_infoframe2_rawset(struct hdmitx_common *tx_comm, u8 *body)
{
	if (!tx_comm) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return;
	}

	if (!body) {
		if (!tx_comm->rxcap.ifdb_present)
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, NULL);
		else
			hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, NULL);
		return;
	}

	if (tx_comm->rxcap.ifdb_present &&
		tx_comm->rxcap.additional_vsif_num >= 1) {
		/* dolby cts case93 B1 */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	} else if (!tx_comm->rxcap.ifdb_present) {
		/* dolby cts case92 B2 */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR, body);
	} else {
		/* case89 ifdb_present but no additional_vsif, currently
		 * no DV-VSIF enabled, then send HF-VSIF
		 */
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	}
}

/* refer DV HDMI 1.4b/2.0/2.1 Transmission
 * 1.DV source device signals the video-timing
 * format by using the CTA VICs in its AVI InfoFrame
 * 2.DV source must not simultaneously transmit
 * a DV and any form of H14b VSIF, even for the case
 * of 4Kp24/25/30
 */
/* only used for DV_VSIF / CUVA VSIF / HDMI1.4b_VSIF / HDR10+ VSIF */
int hdmi_vend_infoframe_get(struct hdmitx_common *tx_comm, u8 *body)
{
	int ret;

	if (!tx_comm || !body)
		return 0;

	if (tx_comm->rxcap.ifdb_present &&
			tx_comm->rxcap.additional_vsif_num >= 1) {
		/* dolby cts case93 B1 */
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR, body);
	} else if (!tx_comm->rxcap.ifdb_present) {
		/* dolby cts case92 B2 */
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR2, body);
	} else {
		/* case89 */
		ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_VENDOR, body);
	}
	return ret;
}

/* not used */
void hdmi_avi_infoframe_set(struct hdmi_avi_infoframe *info)
{
	u8 buffer[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	/* refer to CTA-861-H Page 69 */
	if (info->video_code >= 128)
		info->version = 0x3;
	else
		info->version = 0x2;
	hdmi_avi_infoframe_pack_renew(info, buffer, sizeof(buffer));
	/* the hardware writes the buffer data to the register */
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, buffer);
}

void hdmi_avi_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AVI, body);
}

int hdmi_avi_infoframe_get(u8 *body)
{
	int ret;

	if (!body)
		return 0;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AVI, body);
	return ret;
}

int hdmi_avi_infoframe_config(struct hdmitx_common *tx_comm, u32 avi_cmd, u8 val)
{
	struct hdmi_avi_infoframe *frame = NULL;
	struct hdmi_format_para *para = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("hdr: [%s]: null tx_instance param\n", __func__);
		return -EINVAL;
	}

	frame = &tx_comm->infoframe.avi.avi;
	para = &tx_comm->fmt_para;

	switch (avi_cmd) {
	case AUX_PKT_SET_AVI_CS:
		frame->colorspace = val;
		break;
	case AUX_PKT_CONF_AVI_BT2020:
		if (val == SET_AVI_BT2020) {
			frame->colorimetry = HDMI_COLORIMETRY_EXTENDED;
			frame->extended_colorimetry = HDMI_EXTENDED_COLORIMETRY_BT2020;
			HDMITX_DEBUG_PACKET("avi_colorimetry: bt2020\n");
		}
		if (val == CLR_AVI_BT2020) {
			if (para->timing.v_total <= 576) {
				/* SD formats */
				frame->colorimetry = HDMI_COLORIMETRY_ITU_601;
				frame->extended_colorimetry = 0;
				HDMITX_DEBUG_PACKET("avi_colorimetry: bt601\n");
			} else {
				frame->colorimetry = HDMI_COLORIMETRY_ITU_709;
				frame->extended_colorimetry = 0;
				HDMITX_DEBUG_PACKET("avi_colorimetry: bt709\n");
			}
		}
		break;
	case AUX_PKT_CONF_AVI_Q01:
		frame->quantization_range = val;
		break;
	case AUX_PKT_CONF_AVI_YQ01:
		frame->ycc_quantization_range = val;
		break;
	case AUX_PKT_SET_AVI_VIC:
		frame->video_code = val;
		break;
	case AUX_PKT_CONF_AVI_ASPECT:
		frame->picture_aspect = val;
		break;
	case AUX_PKT_CONF_AVI_CT:
		frame->itc = (val >> 4) & 0x1;
		val = val & 0xf;
		if (val == SET_CT_OFF)
			frame->content_type = 0;
		else if (val == SET_CT_GRAPHICS)
			frame->content_type = 0;
		else if (val == SET_CT_PHOTO)
			frame->content_type = 1;
		else if (val == SET_CT_CINEMA)
			frame->content_type = 2;
		else if (val == SET_CT_GAME)
			frame->content_type = 3;
		break;
	case AUX_PKT_CONF_AVI_SCAN:
		frame->scan_mode = val;
		break;
	default:
		break;
	}
	hdmi_avi_infoframe_set(frame);
	return 0;
}

int hdmi_spd_infoframe_get(u8 *body)
{
	int ret;

	if (!body)
		return 0;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_SPD, body);
	return ret;
}

void hdmi_spd_infoframe_set(struct hdmi_spd_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SPD, NULL);
		return;
	}

	hdmi_spd_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SPD, body);
}

int hdmi_audio_infoframe_get(u8 *body)
{
	int ret;

	if (!body)
		return 0;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_AUDIO, body);
	return ret;
}

void hdmi_audio_infoframe_set(struct hdmi_audio_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	hdmi_audio_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, body);
}

void hdmi_audio_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_AUDIO, body);
}

int hdmi_drm_infoframe_get(u8 *body)
{
	int ret;

	if (!body)
		return 0;
	ret = hdmitx_infoframe_raw_get(HDMI_INFOFRAME_TYPE_DRM, body);
	return ret;
}

void hdmi_drm_infoframe_set(struct hdmi_drm_infoframe *info)
{
	u8 body[31] = {0};

	if (!info) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	hdmi_drm_infoframe_pack(info, body, sizeof(body));
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, body);
}

void hdmi_drm_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_DRM, body);
}

/* for only 8bit, not used */
void hdmi_gcppkt_manual_set(bool en)
{
	u8 body[31] = {0};

	body[0] = HDMI_PACKET_TYPE_GCP;
	if (en)
		hdmitx_infoframe_send(HDMI_PACKET_TYPE_GCP, body);
	else
		hdmitx_infoframe_send(HDMI_PACKET_TYPE_GCP, NULL);
}

int hdmi_emp_infoframe_get(enum emp_type type, u8 *body)
{
	int ret;
	u16 pkt_type;

	if (!body)
		return -1;

	pkt_type = (HDMI_INFOFRAME_TYPE_EMP << 8) | type;
	ret = hdmitx_infoframe_raw_get(pkt_type, (u8 *)body);
	return ret;
}

/*
 * EMP packets is different as other packets
 * no checksum, the successive packets in a video frame...
 */
/* only use hw cmd send emp packet */
void hdmi_emp_infoframe_set(enum emp_type type, struct emp_packet_st *info)
{
	u8 body[31] = {0};
	u8 *md;
	u16 pkt_type;

	pkt_type = (HDMI_INFOFRAME_TYPE_EMP << 8) | type;
	if (!info) {
		hdmitx_infoframe_send(pkt_type, NULL);
		return;
	}

	/* head body, 3bytes */
	body[0] = info->header.header;
	body[1] = info->header.first << 7 | info->header.last << 6;
	body[2] = info->header.seq_idx;
	/* packet body, 28bytes */
	body[3] = info->body.emp0.new << 7 |
		info->body.emp0.end << 6 |
		info->body.emp0.ds_type << 4 |
		info->body.emp0.afr << 3 |
		info->body.emp0.vfr << 2 |
		info->body.emp0.sync << 1;
	/* RSVD */
	body[4] = 0;
	body[5] = info->body.emp0.org_id;
	body[6] = info->body.emp0.ds_tag >> 8 & 0xff;
	body[7] = info->body.emp0.ds_tag & 0xff;
	body[8] = info->body.emp0.ds_length >> 8 & 0xff;
	body[9] = info->body.emp0.ds_length & 0xff;
	md = &body[10];
	switch (info->type) {
	case EMP_TYPE_VRR_GAME:
		md[0] = info->body.emp0.md.game_md.fva_factor_m1 << 4 |
			info->body.emp0.md.game_md.vrr_en << 0;
		md[1] = info->body.emp0.md.game_md.base_vfront;
		md[2] = info->body.emp0.md.game_md.brr_rate >> 8 & 3;
		md[3] = info->body.emp0.md.game_md.brr_rate & 0xff;
		break;
	case EMP_TYPE_VRR_QMS:
		md[0] = info->body.emp0.md.qms_md.qms_en << 2 |
			info->body.emp0.md.qms_md.m_const << 1;
		md[1] = info->body.emp0.md.qms_md.base_vfront;
		md[2] = info->body.emp0.md.qms_md.next_tfr << 3 |
			(info->body.emp0.md.qms_md.brr_rate >> 8 & 3);
		md[3] = info->body.emp0.md.qms_md.brr_rate & 0xff;
		break;
	case EMP_TYPE_SBTM:
		md[0] = info->body.emp0.md.sbtm_md.sbtm_ver << 0;
		md[1] = info->body.emp0.md.sbtm_md.sbtm_mode << 0 |
			info->body.emp0.md.sbtm_md.sbtm_type << 2 |
			info->body.emp0.md.sbtm_md.grdm_min << 4 |
			info->body.emp0.md.sbtm_md.grdm_lum << 6;
		md[2] = (info->body.emp0.md.sbtm_md.frmpblimitint >> 8) & 0x3f;
		md[3] = info->body.emp0.md.sbtm_md.frmpblimitint & 0xff;
		break;
	default:
		break;
	}
	hdmitx_infoframe_send(pkt_type, body);
}

/*
 * this is only configuring the EMP frame body, not send by HW
 * then call hdmi_emp_infoframe_set to send out
 */
void hdmi_emp_frame_set_member(struct emp_packet_st *info,
	enum emp_component_conf conf, u32 val)
{
	if (!info)
		return;

	switch (conf) {
	case CONF_HEADER_INIT:
		/* fixed value */
		info->header.header = HDMI_INFOFRAME_TYPE_EMP;
		break;
	case CONF_HEADER_LAST:
		info->header.last = !!val;
		break;
	case CONF_HEADER_FIRST:
		info->header.first = !!val;
		break;
	case CONF_HEADER_SEQ_INDEX:
		info->header.seq_idx = val;
		break;
	case CONF_SYNC:
		info->body.emp0.sync = !!val;
		break;
	case CONF_VFR:
		info->body.emp0.vfr = !!val;
		break;
	case CONF_AFR:
		info->body.emp0.afr = !!val;
		break;
	case CONF_DS_TYPE:
		info->body.emp0.ds_type = val & 3;
		break;
	case CONF_END:
		info->body.emp0.end = !!val;
		break;
	case CONF_NEW:
		info->body.emp0.new = !!val;
		break;
	case CONF_ORG_ID:
		info->body.emp0.org_id = val;
		break;
	case CONF_DATA_SET_TAG:
		info->body.emp0.ds_tag = val;
		break;
	case CONF_DATA_SET_LENGTH:
		info->body.emp0.ds_length = val;
		break;
	case CONF_VRR_EN:
		info->body.emp0.md.game_md.vrr_en = !!val;
		break;
	case CONF_FACTOR_M1:
		info->body.emp0.md.game_md.fva_factor_m1 = val;
		break;
	case CONF_QMS_EN:
		info->body.emp0.md.qms_md.qms_en = !!val;
		break;
	case CONF_M_CONST:
		info->body.emp0.md.qms_md.m_const = !!val;
		break;
	case CONF_BASE_VFRONT:
		info->body.emp0.md.qms_md.base_vfront = val;
		break;
	case CONF_NEXT_TFR:
		info->body.emp0.md.qms_md.next_tfr = val;
		break;
	case CONF_BASE_REFRESH_RATE:
		info->body.emp0.md.qms_md.brr_rate = val & 0x3ff;
		break;
	case CONF_SBTM_VER:
		info->body.emp0.md.sbtm_md.sbtm_ver = val & 0xf;
		break;
	case CONF_SBTM_MODE:
		info->body.emp0.md.sbtm_md.sbtm_mode = val & 0x3;
		break;
	case CONF_SBTM_TYPE:
		info->body.emp0.md.sbtm_md.sbtm_type = val & 0x3;
		break;
	case CONF_SBTM_GRDM_MIN:
		info->body.emp0.md.sbtm_md.grdm_min = val & 0x3;
		break;
	case CONF_SBTM_GRDM_LUM:
		info->body.emp0.md.sbtm_md.grdm_lum = val & 0x3;
		break;
	case CONF_SBTM_FRMPBLIMITINT:
		info->body.emp0.md.sbtm_md.frmpblimitint = val & 0x3fff;
		break;
	default:
		break;
	}
}

/* internal API */
void hdmi_sbtm_infoframe_rawset(u8 *hb, u8 *pb)
{
	u8 body[31] = {0};

	if (!hb || !pb) {
		hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SBTM, NULL);
		return;
	}

	memcpy(body, hb, 3);
	memcpy(&body[3], pb, 28);
	hdmitx_infoframe_send(HDMI_INFOFRAME_TYPE_SBTM, body);
}
