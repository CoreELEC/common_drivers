// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#include "vpq_debug.h"
#include "vpq_printk.h"
#include "vpq_common.h"
#include "vpq_table_logic.h"
#include "modules/vpq_module_dump.h"

unsigned int log_lev;

#define STATE_AS_STRING(state) (((state) == 1) ? "enable" : "disable")

static void parse_param(char *pbuf, char **param)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[5] = " ";  /* space、\n 、\0 */
	char delim2[2] = "\n";

	ps = pbuf;
	strncat(delim1, delim2, sizeof(delim1) - strlen(delim1) - 1);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		param[n++] = token;
	}
}

static int atoi(const char *str)
{
	int n = 0;

	while (*str != '\0') {
		n = n * 10 + (*str - '0');
		str++;
	}

	return n;
}

ssize_t vpq_debug_cmd_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return 0;
}

ssize_t vpq_debug_cmd_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_log_lev_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	vpq_pr_dbg(lev_dbg, "current log_lev: %d\n", log_lev);

	return 0;
}

ssize_t vpq_log_lev_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "log_lev")) {
		if (kstrtouint(param[1], 10, &log_lev) < 0)
			goto free_buf;

		vpq_pr_dbg(lev_dbg, "log_lev %d\n", log_lev);
	}

free_buf:
	kfree(buf_org);
	return count;
}

static char *chip_type_tostring(enum vpq_chip_class_e chip_type)
{
	if (chip_type == CHIP_CLASS_NULL)
		return "NULL";
	else if (chip_type == VPQ_CHIP_TV)
		return "TV";
	else if (chip_type == VPQ_CHIP_STB)
		return "STB";
	else if (chip_type == VPQ_CHIP_SMT)
		return "SMT";
	else if (chip_type == VPQ_CHIP_AD)
		return "AD";
	else
		return "NULL";
}

static char *chip_id_tostring(enum vpq_chip_e chip_id)
{
	if (chip_id == CHIP_ID_NULL)
		return "NULL";
	else if (chip_id == VPQ_CHIP_T5W)
		return "T5W";
	else if (chip_id == VPQ_CHIP_T3)
		return "T3";
	else if (chip_id == VPQ_CHIP_T6W)
		return "T6W";
	else if (chip_id == VPQ_CHIP_T6X)
		return "T6X";
	else
		return "NULL";
}

ssize_t vpq_chip_infor_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	__maybe_unused enum vpq_chip_class_e chip_type = vpq_get_chip_type();
	__maybe_unused enum vpq_chip_e chip_id = vpq_get_chip_id();

	vpq_pr_dbg(lev_dbg, "current chip infor:\n"
		"  type:%s\n"
		"  id:%s\n",
		chip_type_tostring(chip_type), chip_id_tostring(chip_id));

	return 0;
}

ssize_t vpq_chip_infor_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_reg_table_ver_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	vpq_pr_dbg(lev_dbg, "current pq table ver infor\n"
		" project_version :%s\n"
		" chip_version    :%s\n"
		" table_version   :%s\n"
		" oem_model       :%s\n"
		" panel_index     :%s\n",
		pq_table_ver.project_version, pq_table_ver.chip_version,
		pq_table_ver.table_version, pq_table_ver.oem_model,
		pq_table_ver.panel_index);

	return 0;
}

ssize_t vpq_reg_table_ver_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_pq_setting_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return 0;
}

ssize_t vpq_pq_setting_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[20];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "bri")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_brightness(value);
	} else if (!strcmp(param[0], "con")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_contrast(value);
	} else if (!strcmp(param[0], "sat")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_saturation(value);
	} else if (!strcmp(param[0], "hue")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_hue(value);
	} else if (!strcmp(param[0], "sharp")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_sharpness(value);
	} else if (!strcmp(param[0], "bri_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_brightness_post(value);
	} else if (!strcmp(param[0], "con_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_contrast_post(value);
	} else if (!strcmp(param[0], "sat_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_saturation_post(value);
	} else if (!strcmp(param[0], "hue_post")) {
		if (kstrtoint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_hue_post(value);
	} else if (!strcmp(param[0], "rgb_ogo")) {
		struct vpq_rgb_ogo_s rgb_ogo = {0};

		if (kstrtoint(param[1], 10, &rgb_ogo.r_pre_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[2], 10, &rgb_ogo.g_pre_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[3], 10, &rgb_ogo.b_pre_offset) < 0)
			goto free_buf;
		if (kstrtouint(param[4], 10, &rgb_ogo.r_gain) < 0)
			goto free_buf;
		if (kstrtouint(param[5], 10, &rgb_ogo.g_gain) < 0)
			goto free_buf;
		if (kstrtouint(param[6], 10, &rgb_ogo.b_gain) < 0)
			goto free_buf;
		if (kstrtoint(param[7], 10, &rgb_ogo.r_post_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[8], 10, &rgb_ogo.g_post_offset) < 0)
			goto free_buf;
		if (kstrtoint(param[9], 10, &rgb_ogo.b_post_offset) < 0)
			goto free_buf;

		vpq_set_rgb_ogo(&rgb_ogo);
	} else if (!strcmp(param[0], "dnlp")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_dnlp_mode(value);
	} else if (!strcmp(param[0], "lc")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_lc_mode(value);
	} else if (!strcmp(param[0], "cms")) {
		struct vpq_cms_s cms_param = {0};

		if (kstrtouint(param[1], 10, &cms_param.color_type) < 0)
			goto free_buf;
		if (kstrtouint(param[2], 10, &cms_param.color_9) < 0)
			goto free_buf;
		if (kstrtouint(param[3], 10, &cms_param.color_14) < 0)
			goto free_buf;
		if (kstrtouint(param[4], 10, &cms_param.cms_type) < 0)
			goto free_buf;
		if (kstrtoint(param[5], 10, &cms_param.value) < 0)
			goto free_buf;

		vpq_set_color_customize(&cms_param);
	} else if (!strcmp(param[0], "blkext")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_black_stretch(value);
	} else if (!strcmp(param[0], "blue_stretch")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_blue_stretch(value);
	} else if (!strcmp(param[0], "aipq")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_aipq_mode(value);
	} else if (!strcmp(param[0], "aisr")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_aisr_mode(value);
	} else if (!strcmp(param[0], "csc_type")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_csc_type(value);
	} else if (!strcmp(param[0], "chroma_coring")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_chroma_coring(value);
	} else if (!strcmp(param[0], "amdv_pic_mode_id")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_amdv_pic_mode_id(value);
	} else if (!strcmp(param[0], "amdv_dark_detail")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_amdv_dark_detail(value);
	} else if (!strcmp(param[0], "amdv_light_sensor")) {
		struct vpq_light_sensor_s light_sensor = {0};

		if (kstrtouint(param[1], 10, &light_sensor.flag) < 0)
			goto free_buf;
		if (kstrtouint(param[2], 10, &light_sensor.t_frontLux) < 0)
			goto free_buf;
		if (kstrtouint(param[3], 10, &light_sensor.t_rearLum) < 0)
			goto free_buf;

		vpq_set_amdv_light_sensor(&light_sensor);
	} else if (!strcmp(param[0], "amdv_precision_detail")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_amdv_precision_detail(value);
	}  else if (!strcmp(param[0], "memc_en")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_memc_on_off(value);
	}  else if (!strcmp(param[0], "memc_deblur")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_memc_deblur_level(value);
	}  else if (!strcmp(param[0], "memc_dejudder")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_memc_dejudder_level(value);
	}  else if (!strcmp(param[0], "memc_demo")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_set_memc_demo_mode(value);
	} else {
		//vpq_pr_dbg(lev_dbg, "cmd error\n");
	}

free_buf:
	kfree(buf_org);
	return count;
}

ssize_t vpq_pq_module_cfg_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	vpq_pr_dbg(lev_dbg,
		"show hal pq modules cfg:\n"
		" pq_en           :%s\n"
		" vadj1_en        :%s\n"
		" vd1_ctrst_en    :%s\n"
		" vadj2_en        :%s\n"
		" post_ctrst_en   :%s\n"
		" pregamma_en     :%s\n"
		" gamma_en        :%s\n"
		" wb_en           :%s\n"
		" dnlp_en         :%s\n"
		" lc_en           :%s\n"
		" black_ext_en    :%s\n"
		" blue_stretch_en :%s\n"
		" chroma_cor_en   :%s\n"
		" sharpness0_en   :%s\n"
		" sharpness1_en   :%s\n"
		" cm_en           :%s\n"
		" lut3d_en        :%s\n"
		" dejaggy_sr0_en  :%s\n"
		" dejaggy_sr1_en  :%s\n"
		" dering_sr0_en   :%s\n"
		" dering_sr1_en   :%s\n"
		" di_en           :%s\n"
		" mcdi_en         :%s\n"
		" deblock_en      :%s\n"
		" demosquito_en   :%s\n"
		" smoothplus_en   :%s\n"
		" nr_en           :%s\n"
		" hdrtmo_en       :%s\n"
		" ai_en           :%s\n"
		" aisr_en         :%s\n",
		STATE_AS_STRING(pq_module_cfg.pq_en),
		STATE_AS_STRING(pq_module_cfg.vadj1_en),
		STATE_AS_STRING(pq_module_cfg.vd1_ctrst_en),
		STATE_AS_STRING(pq_module_cfg.vadj2_en),
		STATE_AS_STRING(pq_module_cfg.post_ctrst_en),
		STATE_AS_STRING(pq_module_cfg.pregamma_en),
		STATE_AS_STRING(pq_module_cfg.gamma_en),
		STATE_AS_STRING(pq_module_cfg.wb_en),
		STATE_AS_STRING(pq_module_cfg.dnlp_en),
		STATE_AS_STRING(pq_module_cfg.lc_en),
		STATE_AS_STRING(pq_module_cfg.black_ext_en),
		STATE_AS_STRING(pq_module_cfg.blue_stretch_en),
		STATE_AS_STRING(pq_module_cfg.chroma_cor_en),
		STATE_AS_STRING(pq_module_cfg.sharpness0_en),
		STATE_AS_STRING(pq_module_cfg.sharpness1_en),
		STATE_AS_STRING(pq_module_cfg.cm_en),
		STATE_AS_STRING(pq_module_cfg.lut3d_en),
		STATE_AS_STRING(pq_module_cfg.dejaggy_sr0_en),
		STATE_AS_STRING(pq_module_cfg.dejaggy_sr1_en),
		STATE_AS_STRING(pq_module_cfg.dering_sr0_en),
		STATE_AS_STRING(pq_module_cfg.dering_sr1_en),
		STATE_AS_STRING(pq_module_cfg.di_en),
		STATE_AS_STRING(pq_module_cfg.mcdi_en),
		STATE_AS_STRING(pq_module_cfg.deblock_en),
		STATE_AS_STRING(pq_module_cfg.demosquito_en),
		STATE_AS_STRING(pq_module_cfg.smoothplus_en),
		STATE_AS_STRING(pq_module_cfg.nr_en),
		STATE_AS_STRING(pq_module_cfg.hdrtmo_en),
		STATE_AS_STRING(pq_module_cfg.ai_en),
		STATE_AS_STRING(pq_module_cfg.aisr_en));

	return 0;
}

ssize_t vpq_pq_module_cfg_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_pq_module_status_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct vpq_pq_state_s pq_state = {0};

	vpq_get_pq_module_status(&pq_state);

	vpq_pr_dbg(lev_dbg,
		"show vpp module status:\n"
		" pq           :%s\n"
		" vadj1        :%s\n"
		" vd1_ctrst    :%s\n"
		" vadj2        :%s\n"
		" post_ctrst   :%s\n"
		" pregamma     :%s\n"
		" gamma        :%s\n"
		" wb           :%s\n"
		" dnlp         :%s\n"
		" lc           :%s\n"
		" black_ext    :%s\n"
		" blue_stretch :%s\n"
		" chroma_cor   :%s\n"
		" sharpness0   :%s\n"
		" sharpness1   :%s\n"
		" cm           :%s\n"
		" lut3d        :%s\n"
		" dejaggy_sr0  :%s\n"
		" dejaggy_sr1  :%s\n"
		" dering_sr0   :%s\n"
		" dering_sr1   :%s\n",
		STATE_AS_STRING(pq_state.pq_en),
		STATE_AS_STRING(pq_state.pq_cfg.vadj1_en),
		STATE_AS_STRING(pq_state.pq_cfg.vd1_ctrst_en),
		STATE_AS_STRING(pq_state.pq_cfg.vadj2_en),
		STATE_AS_STRING(pq_state.pq_cfg.post_ctrst_en),
		STATE_AS_STRING(pq_state.pq_cfg.pregamma_en),
		STATE_AS_STRING(pq_state.pq_cfg.gamma_en),
		STATE_AS_STRING(pq_state.pq_cfg.wb_en),
		STATE_AS_STRING(pq_state.pq_cfg.dnlp_en),
		STATE_AS_STRING(pq_state.pq_cfg.lc_en),
		STATE_AS_STRING(pq_state.pq_cfg.black_ext_en),
		STATE_AS_STRING(pq_state.pq_cfg.blue_stretch_en),
		STATE_AS_STRING(pq_state.pq_cfg.chroma_cor_en),
		STATE_AS_STRING(pq_state.pq_cfg.sharpness0_en),
		STATE_AS_STRING(pq_state.pq_cfg.sharpness1_en),
		STATE_AS_STRING(pq_state.pq_cfg.cm_en),
		STATE_AS_STRING(pq_state.pq_cfg.lut3d_en),
		STATE_AS_STRING(pq_state.pq_cfg.dejaggy_sr0_en),
		STATE_AS_STRING(pq_state.pq_cfg.dejaggy_sr1_en),
		STATE_AS_STRING(pq_state.pq_cfg.dering_sr0_en),
		STATE_AS_STRING(pq_state.pq_cfg.dering_sr1_en));

	return 0;
}

ssize_t vpq_pq_module_status_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "pq_module")) {
		enum vpq_module_e module = VPQ_MODULE_VADJ1;

		if (!strcmp(param[1], "vadj1"))
			module = VPQ_MODULE_VADJ1;
		else if (!strcmp(param[1], "vadj2"))
			module = VPQ_MODULE_VADJ2;
		else if (!strcmp(param[1], "pregamma"))
			module = VPQ_MODULE_PREGAMMA;
		else if (!strcmp(param[1], "gamma"))
			module = VPQ_MODULE_GAMMA;
		else if (!strcmp(param[1], "wb"))
			module = VPQ_MODULE_WB;
		else if (!strcmp(param[1], "dnlp"))
			module = VPQ_MODULE_DNLP;
		else if (!strcmp(param[1], "ccoring"))
			module = VPQ_MODULE_CCORING;
		else if (!strcmp(param[1], "sr0"))
			module = VPQ_MODULE_SR0;
		else if (!strcmp(param[1], "sr1"))
			module = VPQ_MODULE_SR1;
		else if (!strcmp(param[1], "lc"))
			module = VPQ_MODULE_LC;
		else if (!strcmp(param[1], "cm"))
			module = VPQ_MODULE_CM;
		else if (!strcmp(param[1], "ble"))
			module = VPQ_MODULE_BLE;
		else if (!strcmp(param[1], "bls"))
			module = VPQ_MODULE_BLS;
		else if (!strcmp(param[1], "lut3d"))
			module = VPQ_MODULE_LUT3D;
		else if (!strcmp(param[1], "dejaggy_sr0"))
			module = VPQ_MODULE_DEJAGGY_SR0;
		else if (!strcmp(param[1], "dejaggy_sr1"))
			module = VPQ_MODULE_DEJAGGY_SR1;
		else if (!strcmp(param[1], "dering_sr0"))
			module = VPQ_MODULE_DERING_SR0;
		else if (!strcmp(param[1], "dering_sr1"))
			module = VPQ_MODULE_DERING_SR1;
		else if (!strcmp(param[1], "all"))
			module = VPQ_MODULE_ALL;
		else
			module = VPQ_MODULE_ALL;

		if (kstrtouint(param[2], 10, &value) < 0)
			goto free_buf;

		vpq_pr_dbg(lev_dbg, "pq_module_status %d %d\n", module, value);
		vpq_set_pq_module_status(module, value);
	}

free_buf:
	kfree(buf_org);
	return count;
}

static char *src_type_tostring(enum vpq_source_type_e src_type)
{
	if (src_type == VPQ_SRC_TYPE_MPEG)
		return "MPEG";
	else if (src_type == VPQ_SRC_TYPE_DTV)
		return "DTV";
	else if (src_type == VPQ_SRC_TYPE_CVBS)
		return "AV";
	else if (src_type == VPQ_SRC_TYPE_HDMI)
		return "HDMI";
	else
		return "NULL";
}

static char *port_type_tostring(enum vpq_hdmi_port_e hdmi_port)
{
	if (hdmi_port == VPQ_HDMI_PORT_0)
		return "HDMI1";
	else if (hdmi_port == VPQ_HDMI_PORT_1)
		return "HDMI2";
	else if (hdmi_port == VPQ_HDMI_PORT_2)
		return "HDMI3";
	else if (hdmi_port == VPQ_HDMI_PORT_3)
		return "HDMI4";
	else
		return "NULL";
}

static char *hdr_type_tostring(enum vpq_hdr_type_e hdr_type)
{
	if (hdr_type == VPQ_HDR_TYPE_HDR10)
		return "HDR10";
	else if (hdr_type == VPQ_HDR_TYPE_HDR10PLUS)
		return "HDR10+";
	else if (hdr_type == VPQ_HDR_TYPE_AMDV)
		return "DV";
	else if (hdr_type == VPQ_HDR_TYPE_PRIMESL)
		return "PRIMESL";
	else if (hdr_type == VPQ_HDR_TYPE_HLG)
		return "HLG";
	else if (hdr_type == VPQ_HDR_TYPE_SDR)
		return "SDR";
	else if (hdr_type == VPQ_HDR_TYPE_MVC)
		return "MVC";
	else
		return "NONE";
}

ssize_t vpq_src_infor_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct vpq_signal_info_s sig_info = {0};

	vpq_get_signal_info(&sig_info);

	vpq_pr_dbg(lev_dbg,
		"current signal infor:\n"
		" source              :%s\n"
		" hdmi port           :%s\n"
		" signal fmt(vdin)    :0x%x\n"
		" trans fmt(vdin)     :%d\n"
		" height              :%d\n"
		" width               :%d\n"
		" fps                 :%dhz\n"
		" hdr type            :%s\n"
		" dv                  :%d\n"
		" game                :%d\n"
		" pc                  :%d\n"
		" scan_mode           :%s\n",
		src_type_tostring(sig_info.src_type),
		port_type_tostring(sig_info.hdmi_port),
		sig_info.sig_fmt,
		sig_info.trans_fmt,
		sig_info.height,
		sig_info.width,
		sig_info.fps,
		hdr_type_tostring(sig_info.hdr_type),
		sig_info.is_amdv,
		sig_info.is_game,
		sig_info.is_pc,
		(sig_info.scan_mode == VPQ_SCAN_MODE_PROGRESSIVE) ? "P" :
		((sig_info.scan_mode == VPQ_SCAN_MODE_INTERLACED) ? "I" : "NULL"));

	return 0;
}

ssize_t vpq_src_infor_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

char *csctype[29][2] = {
	//csc type                      value
	{"NULL",                        "0"},
	{"RGB_YUV601",                  "1"},
	{"RGB_YUV601F",                 "2"},
	{"RGB_YUV709",                  "3"},
	{"RGB_YUV709F",                 "4"},
	{"YUV601_RGB",                  "16"},
	{"YUV601_YUV601F",              "17"},
	{"YUV601_YUV709",               "18"},
	{"YUV601_YUV709F",              "19"},
	{"YUV601F_RGB",                 "20"},
	{"YUV601F_YUV601",              "21"},
	{"YUV601F_YUV709",              "22"},
	{"YUV601F_YUV709F",             "23"},
	{"YUV709_RGB",                  "32"},
	{"YUV709_YUV601",               "33"},
	{"YUV709_YUV601F",              "34"},
	{"YUV709_YUV709F",              "35"},
	{"YUV709F_RGB",                 "36"},
	{"YUV709F_YUV601",              "37"},
	{"YUV709F_YUV709",              "38"},
	{"YUV601L_YUV709L",             "39"},
	{"YUV709L_YUV601L",             "40"},
	{"YUV709F_YUV601F",             "41"},
	{"BT2020YUV_BT2020RGB",         "64"},
	{"BT2020RGB_709RGB",            "65"},
	{"BT2020RGB_CUSRGB",            "66"},
	{"BT2020YUV_BT2020RGB_DYNAMIC", "80"},
	{"BT2020YUV_BT2020RGB_CUVA",    "81"},
	{"\0"},
};

ssize_t vpq_other_infor_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned char i = 0;

	for (i = 0; i < 29; i++) {
		if (strcmp(csctype[i][0], "\0") == 0) {
			vpq_pr_dbg(lev_dbg, "show csc_type: NULL\n");
			break;
		}

		if (vpq_get_csc_type() == atoi(csctype[i][1])) {
			vpq_pr_dbg(lev_dbg, "current csc_type: %s\n", csctype[i][0]);
			break;
		}
	}

	return 0;
}

ssize_t vpq_other_infor_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_hist_avg_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	struct vpq_hist_ave_s hist_avg = {0};

	vpq_get_hist_avg(&hist_avg);
	vpq_pr_dbg(lev_dbg, "current hist avg:\n"
		" sum    :%d\n"
		" height :%d\n"
		" width  :%d\n"
		" ave    :%d\n",
		hist_avg.sum, hist_avg.height, hist_avg.width, hist_avg.ave);

	return 0;
}

ssize_t vpq_hist_avg_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	return count;
}

ssize_t vpq_dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return 0;
}

ssize_t vpq_dump_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	char *buf_org;
	char *param[8];
	int value = 0;

	buf_org = kstrdup(buf, GFP_KERNEL);
	if (!buf_org)
		return -ENOMEM;

	parse_param(buf_org, param);

	if (!strcmp(param[0], "dump_data")) {
		if (kstrtouint(param[1], 10, &value) < 0)
			goto free_buf;

		vpq_pr_dbg(lev_dbg, "dump_module %d\n", value);
		vpq_module_dump_pq_table(value);
	}

free_buf:
	kfree(buf_org);
	return count;
}
