// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/vmalloc.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_mode.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include <linux/amlogic/clk_measure.h>

#include "dptx_log.h"
#include "dptx_internal.h"
#include "meson_tx_internal.h"
#include "dptx_link.h"
#include "dptx_hw_opcode.h"
#include "dpcd_reg.h"
#include "dptx_infoframe.h"
#include "dptx_timer.h"

conn_dev_to_txdev dptx_convert;

void dptx_register_dev_to_txdev_xlate(struct meson_tx_dev *base,
		conn_dev_to_txdev convert)
{
	dptx_convert  = convert;
}
EXPORT_SYMBOL(dptx_register_dev_to_txdev_xlate);

static int dptx_load_rx_cap_raw(struct dptx_common *tx_comm, char *raw_data)
{
	size_t str_len;
	int i;
	unsigned char *buf = NULL;
	size_t byte_len;
	unsigned char tmp[3];

	if (!tx_comm || !raw_data)
		return -1;

	str_len = strlen(raw_data);
	byte_len = (str_len - 1) / 2;
	if (byte_len > sizeof(tx_comm->base.edid_buf))
		return -1;
	buf = kzalloc(byte_len, GFP_KERNEL);
	if (!buf)
		return -1;

	/* convert the raw data to hex byte */
	for (i = 0; i < byte_len; i++) {
		tmp[0] = raw_data[i * 2];
		tmp[1] = raw_data[i * 2 + 1];
		tmp[2] = '\0';
		if (kstrtou8(tmp, 16, &buf[i])) {
			DPTX_INFO("%s convert %c%c error\n", __func__,
				raw_data[i * 2], raw_data[i * 2 + 1]);
			return -1;
		}
	}

	memset(tx_comm->base.edid_buf, 0, sizeof(tx_comm->base.edid_buf));
	memcpy(tx_comm->base.edid_buf, buf, byte_len);

	kfree(buf);
	DPTX_INFO("%s: %zu bytes loaded from string\n", __func__, str_len);
	return 0;
}

struct _clkmsr {
	int idx;
	char *name;
};

static const struct _clkmsr clkmsr_a9[] = {
	{0, "cts_sys_clk"},
	{1, "cts_axi_clk"},
	{49, "hdmi_vx1_pix_clk"},
	{50, "vid_pll_div_clk_out"},
	{51, "cts_enc0_if_clk"},
	{52, "cts_enc1_clk"},
	{53, "cts_enc0_clk"},
	{56, "vid_pll1_div_clk_out"},
	{61, "cts_vpu_clk"},
	{62, "cts_vpu_clkb"},
	{63, "cts_vpu_clkb_tmp"},
	{66, "cts_vapbclk"},
	{117, "aux_clk_o"},
	{130, "cts_dptx_apb2_clk"},
	{131, "cts_dptx_aud_clk"},
	{138, "pixel0_pll_clkout"},
	{139, "pixel1_pll_clkout"},
	{143, "audio_todptx_mclk"},
	{144, "dptx_lnk_clk"},
	{145, "edptx_lnk_clk_tx"}
};

static void dptx_get_clk(void)
{
	int i = 0;
	int len = ARRAY_SIZE(clkmsr_a9);

	for (i = 0; i < len; i++) {
		DPTX_INFO("[%d] %d %s\n", clkmsr_a9[i].idx,
			meson_clk_measure(clkmsr_a9[i].idx), clkmsr_a9[i].name);
	}
}

static enum map_addr_idx_e dptx_check_reg_type(char *type)
{
	enum map_addr_idx_e reg_type = REG_IDX_MAX;

	switch (type[0]) {
	case 'd':
		reg_type = DPTX_COR_REG_IDX;
		break;
	case 's':
		reg_type = SYSCTRL_REG_IDX;
		break;
	case 'a':
		reg_type = ANACTRL_REG_IDX;
		break;
	case 'w':
		reg_type = PWRCTRL_REG_IDX;
		break;
	case 'r':
		reg_type = RESETCTRL_REG_IDX;
		break;
	case 'c':
		reg_type = CLKCTRL_REG_IDX;
		break;
	case 'v':
		reg_type = VPUCTRL_REG_IDX;
		break;
	case 'p':
		reg_type = PADCTRL_REG_IDX;
		break;
	default:
		reg_type = REG_IDX_MAX;
		break;
	}

	return reg_type;
}

static ssize_t dptx_debug_store(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	const char *delim = " ";
	char *token;
	char *cur = (char *)buf;
	unsigned int addr, val;
	struct meson_tx_dev *tx_dev = dptx_convert(dev);
	struct dptx_common *tx_comm = to_dptx_common(tx_dev);
	struct tx_timing *timing_list = NULL;
	int cnt = 0;
	int ret = 0;
	static char test_color_attr[16];
	struct reg_access reg_acc;

	token = strsep(&cur, delim);
	if (token && strncmp(token, "load", 4) == 0) {
		/* load raw EDID/displayID data to buffer */
		token = strsep(&cur, delim);
		if (!token) {
			DPTX_INFO("[%s] cmd format: load raw_data\n",
				__func__);
			return count;
		}
		dptx_load_rx_cap_raw(tx_comm, token);
		meson_tx_clear_rx_cap(&tx_comm->base.rxcap);
		meson_tx_edid_parse(&tx_comm->base.rxcap, tx_comm->base.edid_buf,
			tx_comm->base.edid_parse_mask);
		display_id_cap_print(&tx_comm->base.rxcap.disp_id_cap);
	} else if (token && strncmp(token, "print_info", 10) == 0) {
		DPTX_INFO("drm_dptx_id: %d\n", tx_comm->dev_idx);
		for (cnt = 0; cnt < 128 * 2; cnt++)
			DPTX_INFO("EDID[0x%x] = 0x%x\n", cnt, tx_comm->base.edid_buf[cnt]);
	} else if (token && strncmp(token, "r", 1) == 0) {
		reg_acc.reg_type = dptx_check_reg_type(token + 1);
		if (reg_acc.reg_type == REG_IDX_MAX)
			return count;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		reg_acc.rd_wr_type = 0;
		reg_acc.addr = addr;
		meson_tx_hw_cntl(tx_dev->tx_hw_base, CORE_MISC_REG_RD_WR,
		     &reg_acc, NULL);
		DPTX_INFO("read reg:0x%x val:0x%x\n", addr, reg_acc.val);
	} else if (token && strncmp(token, "w", 1) == 0) {
		reg_acc.reg_type = dptx_check_reg_type(token + 1);
		if (reg_acc.reg_type == REG_IDX_MAX)
			return count;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		reg_acc.rd_wr_type = 1;
		reg_acc.addr = addr;
		reg_acc.val = val;
		meson_tx_hw_cntl(tx_dev->tx_hw_base, CORE_MISC_REG_RD_WR,
			&reg_acc, NULL);
		DPTX_INFO("write reg:0x%x val:0x%x\n", addr, val);
	} else if (token && strncmp(token, "dump", 4) == 0) {
		reg_acc.reg_type = dptx_check_reg_type(token + 4);
		if (reg_acc.reg_type == REG_IDX_MAX)
			return count;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &addr) < 0)
			return count;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		reg_acc.rd_wr_type = 2;
		reg_acc.addr = addr;
		reg_acc.val = val;
		meson_tx_hw_cntl(tx_dev->tx_hw_base, CORE_MISC_REG_RD_WR,
			&reg_acc, NULL);
		DPTX_INFO("dump reg: 0x%x~0x%x\n", addr, val);
	} else if (token && strncmp(token, "hpd_filter_ms", 13) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)	{
			DPTX_ERROR("%s invalid hpd_filter_ms\n", __func__);
			return count;
		}
		tx_comm->hw_comm->hpd_in_filter_ms = val;
		DPTX_INFO("hpd_in_filter_ms %d\n", val);
	} else if (token && strncmp(token, "fake_plug", 9) == 0) {
		int hw_hpd_st = 0;

		token = strsep(&cur, delim);
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, PLATFORM_GET_HPD_GPI_ST, NULL, &hw_hpd_st);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		DPTX_INFO("fake hpd plug %s\n", val ? "in" : "out");
		/* fake plug should be used under HPD is high */
		if (val == 0) {
			meson_tx_plugout_handler(&tx_comm->base);
		} else if (val == 1) {
			if (hw_hpd_st == 1 || tx_dev->pxp_mode)
				meson_tx_plugin_handler(&tx_comm->base);
			else
				DPTX_INFO("HPD is low, no plug handler\n");
		}
	} else if (token && strncmp(token, "hpd_over", 8) == 0) {
		int hw_hpd_st = 0;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_HPD_OVER, &val, NULL);
		if (val & 0x2) {
			DPTX_INFO("hpd_override enable, force hpd %s\n", val & 0x1 ? "in" : "out");
		} else {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base,
				PLATFORM_GET_HPD_GPI_ST, NULL, &hw_hpd_st);
			DPTX_INFO("hpd_override disable, actual hpd_state: %d\n", hw_hpd_st);
		}
	} else if (token && strncmp(token, "get_modes", 8) == 0) {
		cnt = dptx_get_mode_list(tx_comm, &timing_list);
		if (cnt > 0) {
			/* meson_dptx_mode_probed_add(count, timing_list, connector); */
			vfree(timing_list);
			timing_list = NULL;
		}
		if (tx_comm->base.rxcap.disp_id_cap.version)
			display_id_cap_print(&tx_comm->base.rxcap.disp_id_cap);
		DPTX_INFO("%s get mode list count: %d\n", __func__, cnt);
	} else if (token && strncmp(token, "test_attr", 9) == 0) {
		/* step1: for pxp test color attribute, for example: "y444,10bit,f" */
		token = strsep(&cur, delim);
		if (!token) {
			strcpy(test_color_attr, "rgb,8bit");
			DPTX_ERROR("%s invalid attr, set default to rgb,8bit\n", __func__);
			return count;
		}
		strncpy(test_color_attr, token, sizeof(test_color_attr));
		test_color_attr[15] = '\0';
		DPTX_INFO("set test_color_attr: %s\n", test_color_attr);
	} else if (token && strncmp(token, "colorimetry", 11) == 0) {
		u32 colorimetry = 0;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &colorimetry) < 0)	{
			DPTX_ERROR("%s set colorimetry\n", __func__);
			return count;
		}
		tx_dev->sw_fmt_para.colorimetry = colorimetry & 0xFF;
		DPTX_INFO("set colorimetry: %d\n", colorimetry);
	} else if (token && strncmp(token, "test_timing_idx", 15) == 0) {
		/* step2.1 build format para with test_attr and test_mode */
		const struct tx_timing *match_timing = NULL;
		struct tx_timing *tmp_timing = NULL;
		struct meson_tx_format_para *sw_para = NULL;
		struct dptx_hw_fmt_para *hw_para = NULL;
		enum hdmi_color_depth cd = COLORDEPTH_24B;
		enum hdmi_colorspace cs = HDMI_COLORSPACE_RGB;
		enum hdmi_quantization_range cr = HDMI_QUANTIZATION_RANGE_LIMITED;
		u32 timing_idx = 0;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &timing_idx) < 0)	{
			DPTX_ERROR("%s invalid timing mode\n", __func__);
			return count;
		}

		/* return timing in mode list, not check tx/rx cap */
		match_timing = meson_tx_mode_index_to_timing(timing_idx);
		if (!match_timing) {
			DPTX_ERROR("no timing with timing_idx(%d)\n", timing_idx);
			return count;
		}
		tmp_timing = kzalloc(sizeof(*tmp_timing), GFP_KERNEL);
		if (!tmp_timing)
			return count;
		memcpy(tmp_timing, match_timing, sizeof(struct tx_timing));

		if (strstr(test_color_attr, "6bit"))
			cd = COLORDEPTH_18B;
		else if (strstr(test_color_attr, "8bit"))
			cd = COLORDEPTH_24B;
		else if (strstr(test_color_attr, "10bit"))
			cd = COLORDEPTH_30B;
		else if (strstr(test_color_attr, "12bit"))
			cd = COLORDEPTH_36B;
		else
			cd = COLORDEPTH_24B;

		if (strstr(test_color_attr, "y444"))
			cs = HDMI_COLORSPACE_YUV444;
		else if (strstr(test_color_attr, "y422"))
			cs = HDMI_COLORSPACE_YUV422;
		else if (strstr(test_color_attr, "y420"))
			cs = HDMI_COLORSPACE_YUV420;
		else
			cs = HDMI_COLORSPACE_RGB;

		/* "f" means full */
		if (strstr(test_color_attr, "f"))
			cr = HDMI_QUANTIZATION_RANGE_FULL;
		else
			cr = HDMI_QUANTIZATION_RANGE_LIMITED;

		sw_para = &tx_dev->sw_fmt_para;
		hw_para = &sw_para->tx_hw_para.dptx_hw_para;

		/* build SW/HW format param */
		if (meson_tx_format_para_init(&tx_comm->base, tmp_timing, 0, cs, cd, cr, sw_para)) {
			DPTX_ERROR("%s build SW para failed\n", __func__);
			kfree(tmp_timing);
			return count;
		}
		if (dptx_calc_hw_fmt_para(tx_dev, sw_para, hw_para)) {
			DPTX_ERROR("%s build HW para failed\n", __func__);
			kfree(tmp_timing);
			return count;
		}
		DPTX_INFO("%s build test format: %s cs[%d] cd[%d] cr[%d]\n",
			__func__, tmp_timing->name, cs, cd, cr);
		kfree(tmp_timing);
	} else if (token && strncmp(token, "clk_path", 8) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;
		/*
		 * step2.2 build clk path
		 * bit[3:0]: venc_idx, bit[7:4]: hdmi_if_idx
		 */
		meson_tx_build_clk_param(tx_dev, &tx_dev->sw_fmt_para,
			val & 0xF, (val >> 4) & 0xF);
		DPTX_INFO("%s build clk_path: enc_idx: %x, hdmi_if_idx: %x\n",
			__func__, val & 0xF, (val >> 4) & 0xF);
	} else if (token && strncmp(token, "pre_enable", 10) == 0) {
		/* step3: dptx_pre_mode_enable */
		dptx_hw_cntl(tx_dev->tx_hw_base, MODE_FLOW_PRE_ENABLE_MODE,
			&tx_dev->sw_fmt_para, NULL);
		DPTX_INFO("%s dptx_pre_mode_enable\n", __func__);
	} else if (token && strncmp(token, "force_pattern", 13) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		tx_comm->hw_comm->force_tps_pattern = val;
		DPTX_INFO("%s force training pattern: 0x%x\n", __func__, val);
	} else if (token && strncmp(token, "link_train", 10) == 0) {
		/* step4: link training */
		ret = dptx_link_training(tx_comm->link_train);
		if (ret == 0) {
			dptx_update_link_fmt_para(tx_comm, &tx_dev->sw_fmt_para);
			DPTX_INFO("%s update link rate:0x%x, lane count:0x%x\n",
				__func__, tx_dev->sw_fmt_para.tx_hw_para.dptx_hw_para.link_rate,
				tx_dev->sw_fmt_para.tx_hw_para.dptx_hw_para.lane_count);
		}
		DPTX_INFO("%s link training: %s\n", __func__, ret == 0 ? "pass" : "failed");
	} else if (token && strncmp(token, "lt_timeout_ms", 13) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;

		tx_comm->link_train->timeout_ms = val;
		DPTX_INFO("%s dptx link train timeout: %dms\n", __func__, val);
	} else if (token && strncmp(token, "lt_force_lr", 11) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		tx_comm->link_train->force_lr = val;
		DPTX_INFO("%s force link rate: 0x%x\n", __func__, val);
	} else if (token && strncmp(token, "lt_force_lc", 11) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		tx_comm->link_train->force_lc = val;
		DPTX_INFO("%s force lane count: %d\n", __func__, val);
	} else if (token && strncmp(token, "lt_retry_cnt", 12) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		tx_comm->link_train->lt_retry_cnt = val;
		DPTX_INFO("%s link training retry times: %d\n", __func__, val);
	} else if (token && strncmp(token, "vid_clk_sync", 12) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		tx_comm->hw_comm->vid_clk_sync_mode = !!val;
		DPTX_INFO("%s set vid_clk_sync_mode: %d\n", __func__, !!val);
	} else if (token && strncmp(token, "video_mode_set", 14) == 0) {
		/* step5: set VPU format & CORE */
		dptx_hw_cntl(tx_dev->tx_hw_base, MODE_FLOW_ENABLE_MODE, &tx_dev->sw_fmt_para, NULL);
		DPTX_INFO("%s dptx_mode_enable\n", __func__);
	} else if (token && strncmp(token, "clkmsr", 6) == 0) {
		dptx_get_clk();
	} else if (token && strncmp(token, "aux", 3) == 0) {
		token = strsep(&cur, delim);
		if (token && strncmp(token, "sink_count", 10) == 0) {
			ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_SINK_COUNT,
				tx_comm->link_sink_status, sizeof(tx_comm->link_sink_status));
			if (ret < 0) {
				DPTX_INFO("read dpcd cap failed, ret:%d\n", ret);
			} else {
				DPTX_INFO("read dpcd ret length: %d\n", ret);
				for (cnt = 0; cnt < ret; cnt++)
					DPTX_INFO("aux[%d] = 0x%x\n",
						cnt, tx_comm->link_sink_status[cnt]);
			}
		} else if (token && strncmp(token, "dpcd", 4) == 0) {
			ret = dptx_aux_read_dpcd_caps(tx_comm->tx_aux,
				tx_dev->dpcd_buf, sizeof(tx_dev->dpcd_buf));
			if (ret < 0) {
				DPTX_INFO("read dpcd cap failed, ret%d\n", ret);
			} else {
				DPTX_INFO("read dpcd ret: %d\n", ret);
				for (cnt = 0; cnt < ARRAY_SIZE(tx_dev->dpcd_buf); cnt++)
					DPTX_INFO("aux[%d] = 0x%x\n", cnt, tx_dev->dpcd_buf[cnt]);
			}
		} else if (token && strncmp(token, "rd", 2) == 0) {
			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &addr) < 0)
				return count;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;
			DPTX_INFO("aux read offset: 0x%x, size: 0x%x\n", addr, val);
			ret = dptx_aux_read_dpcd(tx_comm->tx_aux, addr,
				tx_dev->dpcd_buf, val);
			if (ret < 0) {
				DPTX_INFO("read dpcd cap failed, ret: %d\n", ret);
			} else {
				DPTX_INFO("read dpcd ret length: %d\n", ret);
				for (cnt = 0; cnt < ret; cnt++)
					DPTX_INFO("aux[%d] = 0x%x\n", cnt, tx_dev->dpcd_buf[cnt]);
			}
		}  else if (token && strncmp(token, "wr", 2) == 0) {
			/*
			 * write two bytes 0x14 0x4 to AUX address 00100h
			 * echo aux wr 0x110 2 0x14 0x4 > debug
			 */
			int i;
			u32 len;
			u32 data[16] = {0};
			u8  aux_data[16] = {0};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &addr) < 0)
				return count;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &len) < 0)
				return count;
			DPTX_INFO("aux write offset: 0x%x, length: 0x%x\n", addr, len);
			len = len < 16 ? len : 16; /* max write length is 16 */
			for (i = 0; i < len; i++) {
				token = strsep(&cur, delim);
				if (!token || kstrtouint(token, 16, &data[i]) < 0)
					break;
				aux_data[i] = data[i] & 0xff;
			}
			ret = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, aux_data, len);
			if (ret < 0) {
				DPTX_INFO("write dpcd cap failed, ret: %d\n", ret);
			} else {
				for (i = 0; i < len; i++)
					DPTX_INFO("  write 0x%02x to aux[0x%05x]\n",
						aux_data[i], addr + i);
			}
		} else if (token && strncmp(token, "block_mode", 10) == 0) {
			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;

			dptx_hw_cntl(tx_dev->tx_hw_base, DP_AUX_BLK_MODE_SET,
				&val, NULL);
			DPTX_INFO("%s dptx aux block mode: %d\n", __func__, !!val);
		} else if (token && strncmp(token, "aux_retry_times", 15) == 0) {
			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &val) < 0)
				return count;

			tx_comm->tx_aux->aux_retry_times = val;
			DPTX_INFO("%s dptx aux_retry_times: %d\n", __func__, !!val);
		} else if (token && strncmp(token, "read_edid_cnt_once", 18) == 0) {
			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &val) < 0)
				return count;

			tx_comm->tx_aux->edid_read_cnt_once = val;
			DPTX_INFO("%s dptx edid_read_cnt_once: %d\n", __func__, !!val);
		} else if (token && strncmp(token, "aux_timeout_ms", 14) == 0) {
			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &val) < 0)
				return count;

			tx_comm->tx_aux->aux_timeout_ms = val;
			DPTX_INFO("%s dptx aux_timeout: %dms\n", __func__, val);
		} else if (token && strncmp(token, "edid", 4) == 0) {
			ret = dptx_aux_read_edid_data(tx_comm->tx_aux, tx_comm->base.edid_buf,
				sizeof(tx_comm->base.edid_buf));
			if (ret < 0) {
				DPTX_ERROR("read edid failed\n");
			} else {
				for (cnt = 0; cnt < 128 * 2; cnt++)
					DPTX_INFO("EDID[0x%x] = 0x%x\n",
						cnt, tx_comm->base.edid_buf[cnt]);
			}
		} else if (token && strncmp(token, "wr", 2) == 0) {
			/*
			 * write two bytes 0x14 0x4 to AUX address 00100h
			 * echo aux wr 0x110 2 0x14 0x4 > debug
			 */
			int i;
			u32 len;
			u32 data[16] = {0};
			u8  aux_data[16] = {0};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &addr) < 0)
				return count;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &len) < 0)
				return count;
			DPTX_INFO("aux write offset: 0x%x, length: 0x%x\n", addr, len);
			len = len < 16 ? len : 16; /* max write length is 16 */
			for (i = 0; i < len; i++) {
				token = strsep(&cur, delim);
				if (!token || kstrtouint(token, 16, &data[i]) < 0)
					break;
				aux_data[i] = data[i] & 0xff;
			}
			ret = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, aux_data, len);
			if (ret < 0) {
				DPTX_INFO("write dpcd cap failed, ret: %d\n", ret);
			} else {
				for (i = 0; i < len; i++)
					DPTX_INFO("  write 0x%02x to aux[0x%05x]\n",
						aux_data[i], addr + i);
			}
		}
	} else if (token && strncmp(token, "mst", 3) == 0) {
		token = strsep(&cur, delim);
		if (token && strncmp(token, "init", 4) == 0) {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_INIT, NULL, NULL);
		} else if (token && strncmp(token, "alloc", 5) == 0) {
			struct mst_conf_vcps_param param = {0};
			u32 temp;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &temp) < 0)
				return count;
			param.vc_id = temp > 1 ? 0 : temp;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &temp) < 0)
				return count;
			param.pixel_clock = temp;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &temp) < 0)
				return count;
			param.cs = temp;

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 10, &temp) < 0)
				return count;
			param.cd = temp;

			DPTX_INFO("mst param: %d %d %d %d\n", param.vc_id, param.pixel_clock,
				param.cs, param.cd);
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_ALLOC_PAYLOAD, &param,
				NULL);
		} else if (token && strncmp(token, "update", 6) == 0) {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_UPDATE_TABLE, NULL, NULL);
		} else if (token && strncmp(token, "0to1", 4) == 0) {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_0_TO_1, NULL, NULL);
		} else if (token && strncmp(token, "1to0", 4) == 0) {
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_1_TO_0, NULL, NULL);
		} else if (token && strncmp(token, "comp", 4) == 0) {
			char c;

			token = strsep(&cur, delim);
			c = *token;
			if (c != 'd' || c != 's' || c != 'a')
				c = 'a';
			dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_MST_COMPARE_0_1, &c, NULL);
		}
	} else if (token && strncmp(token, "sdp_dump", 8) == 0) {
		dptx_hw_cntl(tx_dev->tx_hw_base, AUX_PKT_SDP_INFO_DUMP,
			&tx_dev->sw_fmt_para.vc_id, NULL);
	} else if (token && strncmp(token, "sdp_info", 8) == 0) {
		token = strsep(&cur, delim);
		if (token && strncmp(token, "vsif", 4) == 0) {
			DPTX_INFO("not supported currently\n");
		} else if (token && strncmp(token, "avi", 3) == 0) {
			u8 hb[3] = {0x82, 0x02, 0x0d};
			/* 0xcd is checksum byte, avi payload byte length = 13 */
			u8 pb[28] = {0xcd, 0x52, 0xe8, 0x64, 0x04, 0x0,
				0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;

			if (val)
				dptx_avi_infoframe_rawset(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, hb, pb);
			else
				dptx_avi_infoframe_set(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, NULL);
		} else if (token && strncmp(token, "spd", 3) == 0) {
			u8 hb[3] = {0x83, 0x01, 0x19};
			/* 0x56 is checksum byte, spd payload byte length = 25 */
			u8 pb[28] = {0x56, 0x43, 0x55, 0x53, 0x5f, 0x4e,
				0x41, 0x4d, 0x45, 0x50, 0x52, 0x4f, 0x44,
				0x55, 0x43, 0x54, 0x5f, 0x4e, 0x41, 0x4d,
				0x45, 0x00, 0x00, 0x00, 0x00, 0x01};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;

			if (val)
				dptx_spd_infoframe_rawset(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, hb, pb);
			else
				dptx_spd_infoframe_set(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, NULL);
		} else if (token && strncmp(token, "audio", 5) == 0) {
			u8 hb[3] = {0x84, 0x01, 0x0a};
			/* 0x70 is checksum byte, audio infoframe payload byte length = 10 */
			u8 pb[28] = {0x70, 0x01, 0x00, 0x00, 0x00,
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;

			if (val)
				dptx_audio_infoframe_rawset(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, hb, pb);
			else
				dptx_audio_infoframe_set(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, NULL);
		} else if (token && strncmp(token, "drm", 3) == 0) {
			u8 hb[3] = {0x87, 0x01, 0x1a};
			/* 0x0d is checksum byte, drm infoframe payload byte length = 26 */
			u8 pb[28] = {0x0d, 0x02, 0x00, 0x34, 0x21,	0xaa,
				0x9b, 0x96, 0x19, 0xfc, 0x08, 0x48, 0x8a,
				0x08, 0x39, 0x13, 0x3d, 0x42, 0x40, 0xe8,
				0x03, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00};

			token = strsep(&cur, delim);
			if (!token || kstrtouint(token, 16, &val) < 0)
				return count;

			if (val)
				dptx_drm_infoframe_rawset(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, hb, pb);
			else
				dptx_drm_infoframe_set(tx_comm->hw_comm,
					tx_dev->sw_fmt_para.vc_id, NULL);
		}
		DPTX_INFO("set sdp info test\n");
	} else if (token && strncmp(token, "adaptive_sync", 13) == 0) {
		bool enable = false;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 16, &val) < 0)
			return count;

		/* 0: disable adaptive sync, 1: enable if DPRX support, 2: force enable */
		if (val == 0) {
			enable = false;
		} else if (val == 1) {
			/* DPCD cap[00007h] and DPCD extended cap[2214h] */
			if ((tx_dev->dpcd_buf[7] & 0x40) == 0x40 &&
				(tx_dev->dpcd_buf[14] & 0x1) == 0x1)
				enable = true;
			else
				enable = false;
		} else {
			enable = true;
		}
		if (enable)
			dptx_hw_cntl(tx_dev->tx_hw_base, DP_ADAPTIVE_SYNC_EN,
				&tx_dev->sw_fmt_para.vc_id, NULL);
		else
			dptx_hw_cntl(tx_dev->tx_hw_base, DP_ADAPTIVE_SYNC_DIS,
				&tx_dev->sw_fmt_para.vc_id, NULL);
		DPTX_INFO("adaptive_sync enable: %d\n", enable);
	} else if (token && strncmp(token, "timer", 5) == 0) {
		struct dptx_timer_handler *handlers = tx_comm->hw_comm->timer_manager->handlers;
		/* timer type */
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &addr) < 0)
			return count;
		if (addr >= 4) {
			DPTX_ERROR("timer type(%d) not supported\n", addr);
			return count;
		}
		handlers[addr].cfg.timer_type = addr;
		DPTX_INFO("test timer type = %d\n", addr);

		/* timer wait_type */
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		if (val > TIMER_REPEAT_ISR) {
			DPTX_ERROR("timer wait_type(%d) not supported\n", val);
			return count;
		}
		handlers[addr].cfg.wait_type = val;
		DPTX_INFO("test timer wait_type = %d\n", val);

		/* timer wait_us */
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		handlers[addr].cfg.us = val;
		DPTX_INFO("test timer wait_us = %d\n", val);

		/* timer repeat times */
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		handlers[addr].cfg.repeater_times = val;
		DPTX_INFO("test timer repeater_times = %d\n", val);

		/* timer enable/disable */
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		DPTX_INFO("test timer %s\n", val ? "enable" : "disable");
		if (val > 0)
			dptx_timer_start(tx_comm->hw_comm, &handlers[addr].cfg);
		else
			dptx_timer_stop(tx_comm->hw_comm, &handlers[addr].cfg);
	} else if (token && strncmp(token, "tx_output", 9) == 0) {
		u8 en = 0;

		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		en = val & 0xFF;
		DPTX_INFO("tx output %s\n", en ? "enable" : "disable");
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, DP_TRANSMITTER_OUTPUT_EN, &en, NULL);
	} else if (token && strncmp(token, "hdcp_mode", 9) == 0) {
		token = strsep(&cur, delim);
		if (!token || kstrtouint(token, 10, &val) < 0)
			return count;
		if (val > 2) {
			DPTX_ERROR("invalid hdcp_mode: %d\n", val);
			return count;
		}
		DPTX_INFO("set hdcp_mode: %d, set mode to take effect\n", val);
		tx_comm->hdcptx_comm.hdcp_mode = val;
		tx_comm->hw_comm->hdcp_sec_en = val;
	}
	return count;
}

static struct device_attribute dptx_attrs[] = {
	__ATTR(debug, 0200, NULL, dptx_debug_store),
};

int dptx_create_sysfs(struct meson_tx_dev *tx_dev)
{
	int i;
	int ret = 0;
	struct dptx_common *tx_comm = to_dptx_common(tx_dev);

	for (i = 0; i < ARRAY_SIZE(dptx_attrs); i++) {
		ret = device_create_file(tx_dev->dev, &dptx_attrs[i]);
		if (ret) {
			DPTX_ERROR("dptx_dev[%d] create attribute %s fail\n",
				tx_comm->dev_idx, dptx_attrs[i].attr.name);
			break;
		}
	}
	return ret;
}

void dptx_remove_sysfs(struct meson_tx_dev *tx_dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dptx_attrs); i++)
		device_remove_file(tx_dev->dev, &dptx_attrs[i]);
}
