// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#include <linux/amlogic/media/registers/register_map.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/iomap.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/firmware.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif

#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/amprime_sl/prime_sl.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#include "amprime_sl.h"
#include "amprime_sl_regs_t6w.h"
#include "amprime_sl_regs_t6x.h"

/*======================================*/
#define AMPRIME_SL_NAME               "amprime_sl"
#define AMPRIME_SL_CLASS_NAME         "amprime_sl"
#define SEI_VERSION 1

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

#define NAL_UNIT_SEI 39
#define NAL_UNIT_SEI_SUFFIX 40
#define SEI_ITU_T_T35 4
#define PRIME_SL_T35_PROV_CODE 0x003A

#define MAX_PARAM   8

static const struct hdr_prime_sl_func_s *p_funcs;
struct prime_sl_device_data_s prime_sl_meson_dev;

struct amprime_sl_dev_s {
	dev_t devt;
	struct cdev cdev;
	dev_t devno;
	struct device *dev;
	struct class *clsp;
};

static struct amprime_sl_dev_s amprime_sl_dev;
static struct prime_t prime_sl_setting;

static u32 prime_sl_enable = ENABLE;
module_param(prime_sl_enable, uint, 0664);
MODULE_PARM_DESC(prime_sl_enable, "\n prime_sl_enable\n");

static u32 prime_sl_running; /* 0:module off; 1:module on */
static u32 prime_sl_probe; /* 0:not probe; 1:probe ok */

/* DA */
static u32 prime_sl_display_brightness = 550;
module_param(prime_sl_display_brightness, uint, 0664);
MODULE_PARM_DESC(prime_sl_display_brightness, "\n prime_sl_display_brightness\n");

static u32 prime_sl_output_curve = PRIME_SL_DISPLAY_OETF_BT1886;

#define DAT_OFF 0
#define DAT_LOW 1
#define DAT_MEDIAN 2
#define DAT_HIGH 3
static u32 prime_sl_display_adaptation_tuning = DAT_MEDIAN;
module_param(prime_sl_display_adaptation_tuning, uint, 0664);
MODULE_PARM_DESC(prime_sl_display_adaptation_tuning, "\n prime_sl_display_adaptation_tuning\n");

u32 prime_sl_debug;
/* use prime_sl_debug_run_mode for test */
/* 1:load fix setting mode; 2:test metadata mode; 3:skip mode*/
static u32 prime_sl_debug_run_mode;
static u32 prime_sl_hdr_mode = PRIME_SL_HDR_MODE_BYPASS_2020;
static u32 prime_sl_certification_mode; /* 0:off, 1:on */
static u32 prime_sl_encode_type; /* 0:HEVC, 1:AVC */
static bool is_prime_sl_frame;/* 0:no, 1:yes */
static u32 ampsl_dpss_display_counts;

void *prime_sl_reg_setting_buf;
u32 prime_sl_reg_setting_num = 225;
static struct prime_sl_t psl;
static u32 prime_sl_dpss_mode;

static const int shadow_gain[201] = {
	255, 255, 255, 253, 227, 208, 194, 182,
	172, 164, 157, 151, 145, 140, 136, 131,
	128, 124, 121, 118, 115, 112, 110, 107,
	105, 103, 101, 99,  97,  95,  94,  92,
	91,  89,  88,  86,  85,  84,  82,  81,
	80,  79,  78,  77,  76,  75,  74,  73,
	72,  71,  70,  69,  68,  67,  67,  66,
	65,  64,  64,  63,  62,  61,  61,  60,
	59,  59,  58,  58,  57,  56,  56,  55,
	55,  54,  54,  53,  53,  52,  52,  51,
	51,  50,  50,  49,  49,  48,  48,  47,
	47,  46,  46,  46,  45,  45,  44,  44,
	44,  43,  43,  42,  42,  42,  41,  41,
	41,  40,  40,  40,  39,  39,  39,  38,
	38,  38,  37,  37,  37,  36,  36,  36,
	36,  35,  35,  35,  34,  34,  34,  34,
	33,  33,  33,  32,  32,  32,  32,  31,
	31,  31,  31,  30,  30,  30,  30,  29,
	29,  29,  29,  29,  28,  28,  28,  28,
	27,	 27,  27,  27,  27,  26,  26,  26,
	26,	 25,  25,  25,  25,  25,  24,  24,
	24,	 24,  24,  23,  23,  23,  23,  23,
	23,	 22,  22,  22,  22,  22,  21,  21,
	21,	 21,  21,  21,  20,  20,  20,  20,
	20,	 20,  19,  19,  19,  19,  19,  19,
	18,
};

static void dbg_setting(struct prime_sl_t *prime_sl);
static void dbg_config(struct prime_cfg_t *cfg);
static void dbg_metadata(struct sl_hdr_metadata *pmetadata);

int SL_VSYNC_WR_MPEG_REG(u32 adr, u32 val)
{
	if (prime_sl_dpss_mode) {
		WRITE_VPP_REG(adr, val);
	} else {
#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		WRITE_VPP_REG(adr, val);
#else
		VSYNC_WR_MPEG_REG(adr, val);
#endif
	}
	return 0;
}

int SL_VSYNC_RD_MPEG_REG(u32 adr)
{
	if (prime_sl_dpss_mode) {
		READ_VPP_REG(adr);
	} else {
#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		READ_VPP_REG(adr);
#else
		VSYNC_RD_MPEG_REG(adr);
#endif
	}
	return 0;
}

int SL_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
	if (prime_sl_dpss_mode) {
		WRITE_VPP_REG_BITS(adr, val, start, len);
	} else {
#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
		WRITE_VPP_REG_BITS(adr, val, start, len);
#else
		VSYNC_WR_MPEG_REG_BITS(adr, val, start, len);
#endif
	}
	return 0;
}

bool is_meson_g12(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_G12)
		return true;
	else
		return false;
}

bool is_meson_tl1(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_TL1)
		return true;
	else
		return false;
}

bool is_meson_tm2(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_TM2)
		return true;
	else
		return false;
}

bool is_meson_sc2(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_SC2)
		return true;
	else
		return false;
}

bool is_meson_t7(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_T7)
		return true;
	else
		return false;
}

bool is_meson_s4d(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_S4D)
		return true;
	else
		return false;
}

bool is_meson_t5w(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_T5W)
		return true;
	else
		return false;
}

bool is_meson_t5m(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_T5M)
		return true;
	else
		return false;
}

bool is_meson_s7d(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_S7D)
		return true;
	else
		return false;
}

bool is_meson_s6(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_S6)
		return true;
	else
		return false;
}

bool is_meson_t6w(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_T6W)
		return true;
	else
		return false;
}

bool is_meson_t6x(void)
{
	if (prime_sl_meson_dev.cpu_id == _CPU_MAJOR_ID_T6X)
		return true;
	else
		return false;
}

bool is_prime_sl_stb_mode(void)
{
	if (is_meson_g12() || is_meson_tl1() ||
		is_meson_tm2() || is_meson_sc2() ||
		is_meson_t7() || is_meson_s4d() ||
		is_meson_s7d() || is_meson_s6())
		return true;
	else
		return false;
}

bool is_prime_sl_enable(void)
{
	return prime_sl_enable &&
		((prime_sl_probe && p_funcs) || prime_sl_debug_run_mode);
}
EXPORT_SYMBOL(is_prime_sl_enable);

bool is_prime_sl_on(void)
{
	return prime_sl_enable && prime_sl_running &&
		((prime_sl_probe && p_funcs) || prime_sl_debug_run_mode);
}
EXPORT_SYMBOL(is_prime_sl_on);

bool get_prime_sl_frame(void)
{
	return is_prime_sl_frame;
}
EXPORT_SYMBOL(get_prime_sl_frame);

void set_prime_sl_frame(bool val)
{
	is_prime_sl_frame = val;
}
EXPORT_SYMBOL(set_prime_sl_frame);

struct device *get_ampsl_device(void)
{
	return amprime_sl_dev.dev;
}

static int sink_support_hdr(const struct vinfo_s *vinfo)
{
	return sink_hdr_support(vinfo) & HDR_SUPPORT;
}

int get_prime_sl_hdr_process_policy(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	if (sink_support_hdr(vinfo)) {
		if (signal_transfer_characteristic == 16 &&
			(signal_color_primaries == 9 || signal_color_primaries == 2)) {
			prime_sl_hdr_mode = PRIME_SL_HDR_MODE_BYPASS_2020;/* SL-HDR2 */
		} else if (signal_transfer_characteristic == 14 ||
			signal_transfer_characteristic == 1 ||
			signal_transfer_characteristic == 2) {
			if (signal_color_primaries == 9)
				prime_sl_hdr_mode = PRIME_SL_HDR_MODE_BYPASS_2020;/* HDR1 BT2020 */
			else
				prime_sl_hdr_mode = PRIME_SL_HDR_MODE_709_TO_2020;/* HDR1 BT709 */
		} else {
			prime_sl_hdr_mode = PRIME_SL_HDR_MODE_709_TO_2020;/* HDR1 BT709 */
		}
	} else {
		if (signal_transfer_characteristic == 16 &&
			(signal_color_primaries == 9 || signal_color_primaries == 2)) {
			prime_sl_hdr_mode = PRIME_SL_HDR_MODE_2020_TO_709;/* SL-HDR2 */
		} else if (signal_transfer_characteristic == 14 ||
			signal_transfer_characteristic == 1 ||
			signal_transfer_characteristic == 2) {
			if (signal_color_primaries == 9)
				prime_sl_hdr_mode = PRIME_SL_HDR_MODE_2020_TO_709;/* HDR1 BT2020 */
			else
				prime_sl_hdr_mode = PRIME_SL_HDR_MODE_BYPASS_709;/* HDR1 BT709 */
		} else {
			prime_sl_hdr_mode = PRIME_SL_HDR_MODE_BYPASS_709;/* HDR1 BT709 */
		}
	}
	return prime_sl_hdr_mode;
}

int get_prime_sl_output_curve(void)
{
	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	if (sink_support_hdr(vinfo))
		prime_sl_output_curve = PRIME_SL_DISPLAY_OETF_PQ;
	else
		prime_sl_output_curve = PRIME_SL_DISPLAY_OETF_BT1886;
	return prime_sl_output_curve;
}

int get_prime_sl_display_brightness(void)
{
	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	if (prime_sl_certification_mode)
		return prime_sl_display_brightness;
	if (vinfo && vinfo->hdr_info.lumi_max && is_prime_sl_stb_mode()) {
		prime_sl_display_brightness = vinfo->hdr_info.lumi_max;
	} else {
		if (sink_support_hdr(vinfo))
			prime_sl_display_brightness = 1000;
		else
			prime_sl_display_brightness = 150;
	}
	return prime_sl_display_brightness;
}

#define SEI_VERSION 1

/* return false: invalid metadata */
bool is_valid_prime_sl_metadata(char *buf, int size)
{
	int i;
	int check_size = 8;

	if (size <= check_size)
		return false;
	for (i = 0; i <= check_size; i++) {
		if (buf[i])
			return true;
	}
	return false;
}
EXPORT_SYMBOL(is_valid_prime_sl_metadata);

/* just debug now */
#ifdef RAW_META
static int prime_sl_parser_sei_and_meta(char *p, unsigned int size)
{
	unsigned int i;
	struct sl_hdr_metadata *metadata = &prime_sl_setting.prime_metadata;

	if (size != sizeof(struct sl_hdr_metadata)) {
		pr_info("error metadata size\n");
		return 0;
	}

	metadata->part_id = parse_para(p);
	metadata->major_spec_version_id = parse_para(p);
	metadata->minor_spec_version_id = parse_para(p);
	metadata->payload_mode = parse_para(p);
	metadata->hdr_pic_colour_space = parse_para(p);
	metadata->hdr_display_colour_space = parse_para(p);
	metadata->hdr_display_max_luminance = parse_para(p);
	metadata->hdr_display_min_luminance = parse_para(p);
	metadata->sdr_pic_colour_space = parse_para(p);
	metadata->sdr_display_colour_space = parse_para(p);
	metadata->sdr_display_max_luminance = parse_para(p);
	metadata->sdr_display_min_luminance = parse_para(p);
	for (i = 0; i < 4; i++)
		metadata->matrix_coefficient[i] = parse_para(p);
	for (i = 0; i < 2; i++)
		metadata->chroma_to_luma_injection[i] = parse_para(p);
	for (i = 0; i < 3; i++)
		metadata->kcoefficient[i] = parse_para(p);
	if (!metadata->payload_mode) {
		metadata->u.variables.tm_input_signal_blacklevel_offset = parse_para(p);
		metadata->u.variables.tm_input_signal_whitelevel_offset = parse_para(p);
		metadata->u.variables.shadow_gain = parse_para(p);
		metadata->u.variables.highlight_gain = parse_para(p);
		metadata->u.variables.mid_tone_width_adj_factor = parse_para(p);
		metadata->u.variables.tm_output_finetuning_num_val = parse_para(p);
		for (i = 0; i < metadata->u.variables.tm_output_finetuning_num_val; i++) {
			metadata->u.variables.tm_output_finetuning_x[i] = parse_para(p);
			metadata->u.variables.tm_output_finetuning_y[i] = parse_para(p);
		}
		metadata->u.variables.saturation_gain_num_val = parse_para(p);
		for (i = 0; i < metadata->u.variables.saturation_gain_num_val; i++) {
			metadata->u.variables.saturation_gain_x[i] = parse_para(p);
			metadata->u.variables.saturation_gain_y[i] = parse_para(p);
		}
	} else {
		metadata->u.tables.luminance_mapping_num_val = parse_para(p);
		for (i = 0; i < metadata->u.tables.luminance_mapping_num_val; i++) {
			metadata->u.tables.luminance_mapping_x[i] = parse_para(p);
			metadata->u.tables.luminance_mapping_y[i] = parse_para(p);
		}
		metadata->u.tables.colour_correction_num_val = parse_para(p);
		for (i = 0; i < metadata->u.tables.colour_correction_num_val; i++) {
			metadata->u.tables.colour_correction_x[i] = parse_para(p);
			metadata->u.tables.colour_correction_y[i] = parse_para(p);
		}
	}
	return 0;
}
#endif

struct sei_parser_s {
	u8 *buffer_ptr;
	u32 buffer_size;
	u32 readbytes;
	u32 readbits;
	u32 zero_num;
	u32 cur_bytes;
	u32 next_bytes;
	bool err_flag;
};

/*LOAD_BUFFER_SIZE, nums of buffer read bytes*/
#define LOAD_BUFFER_SIZE 4
static struct sei_parser_s prime_sl_parser;

static int loadbyte(struct sei_parser_s *p, u8 *byte)
{
	*byte = 0;
	if (p->readbytes >= p->buffer_size) {
		p->err_flag = true;
		return -1;
	}

	*byte = p->buffer_ptr[p->readbytes];
	p->readbytes++;
#ifdef TMP_DISABLE
	if (*byte == 0x03 || *byte == 0x00) {
		if (p->zero_num == 2) {
			if (*byte == 0x03) {
				if (p->readbytes >= p->buffer_size) {
					p->err_flag = true;
					return -1;
				}
				*byte = p->buffer_ptr[p->readbytes];
				p->readbytes++;
			}
			p->zero_num = 0;
			if (*byte == 0)
				p->zero_num++;
		} else {
			if (*byte == 0x00)
				p->zero_num++;
			else
				p->zero_num = 0;
		}
	} else {
		p->zero_num = 0;
	}
#endif
	return 0;
}

static int loadbuffer(struct sei_parser_s *p, u32 *bytes)
{
	u8 byte;

	*bytes = 0;

	if (loadbyte(p, &byte))
		return -1;
	*bytes = byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -2;
	*bytes |= byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -3;
	*bytes |= byte;
	*bytes <<= 8;

	if (loadbyte(p, &byte))
		return -4;
	*bytes |= byte;
	return 0;
}

static int readbits(struct sei_parser_s *p, int bits)
{
	int value = 0;

	p->readbits += bits;

	if (p->readbits <= 32) {
		value = p->cur_bytes >> (32 - bits);
		p->cur_bytes <<= bits;
	} else {
		p->readbits -= 32;
		value = (p->cur_bytes >> (32 - bits)) |
			(p->next_bytes >> (32 - p->readbits));
		p->cur_bytes = p->next_bytes;
		loadbuffer(p, &p->next_bytes);
		p->cur_bytes <<= p->readbits;
	}

	return value;
}

#define readU(p, bits) readbits(p, bits)

#ifdef TMP_DISABLE
static u32 read_ue(struct sei_parser_s *p)
{
	u32 value, bit = 0, length = 0;

	bit = readbits(p, 1);

	if (!bit) {
		while (!bit) {
			bit = readbits(p, 1);
			length++;
		}
		value = readbits(p, length);
		value += (1 << length) - 1;
		return value;
	}
	return 0;
}
#endif

static void init_sei_parser(struct sei_parser_s *p, u8 *raw_meta, u32 meta_size)
{
	p->buffer_ptr = raw_meta;
	/*size read by metadata should be multiple of LOAD_BUFFER_SIZE*/
	p->buffer_size =
		((meta_size + LOAD_BUFFER_SIZE - 1) / LOAD_BUFFER_SIZE) * LOAD_BUFFER_SIZE;
	p->readbits = 0;
	p->readbytes = 0;
	p->zero_num = 0;
	p->cur_bytes = 0;
	p->next_bytes = 0;
	p->err_flag = false;
	loadbuffer(p, &p->cur_bytes);
	loadbuffer(p, &p->next_bytes);
}

static bool is_parser_success(struct sei_parser_s *p)
{
	return p->err_flag ? false : true;
}

#define VER_1_4_0

static void fill_metadata_recovery(struct sl_hdr_metadata *metadata,
		int target_display_luminance,
		int colour_primaries,
		int transfer_characteristics)
{
	u8 lhdr_idx = target_display_luminance / 50;
	int s_gain = (target_display_luminance == 100) ? 115 : shadow_gain[lhdr_idx];

	if (transfer_characteristics == 12 ||
	    transfer_characteristics == 16) {
		metadata->part_id = 2;
		metadata->chroma_to_luma_injection[1] = 0;
		metadata->u.variables.saturation_gain_num_val = 0;
	} else {
		metadata->part_id = 1;
		metadata->chroma_to_luma_injection[1] =
			(colour_primaries == 1) ? 0 : 1638;
		metadata->u.variables.saturation_gain_num_val = 1;
		metadata->u.variables.saturation_gain_x[0] = 0;
		metadata->u.variables.saturation_gain_y[0] =
			(colour_primaries == 1) ? 115 : 118;
	}

	metadata->payload_mode = 0;

	if (colour_primaries == 1) {
		metadata->hdr_pic_colour_space = 0;
		metadata->hdr_display_colour_space = 0;
		metadata->matrix_coefficient[0] = 915;
		metadata->matrix_coefficient[1] = 464;
		metadata->matrix_coefficient[2] = 392;
		metadata->matrix_coefficient[3] = 987;
	} else {
		metadata->hdr_pic_colour_space = 1;
		metadata->hdr_display_colour_space = 2;
		metadata->matrix_coefficient[0] = 889;
		metadata->matrix_coefficient[1] = 470;
		metadata->matrix_coefficient[2] = 366;
		metadata->matrix_coefficient[3] = 994;
	}

	metadata->hdr_display_max_luminance =
		(target_display_luminance == 100) ? 1000 : target_display_luminance;
	metadata->hdr_display_min_luminance = 0;
	metadata->sdr_pic_colour_space = metadata->hdr_pic_colour_space;
	metadata->sdr_display_colour_space = metadata->hdr_display_colour_space;
	metadata->sdr_display_max_luminance = 100;
	metadata->sdr_display_min_luminance = 0;
	metadata->kcoefficient[0] = 0;
	metadata->kcoefficient[1] = 0;
	metadata->kcoefficient[2] = 0;
	metadata->chroma_to_luma_injection[0] = 0;
	metadata->u.variables.tm_input_signal_blacklevel_offset = 0;
	metadata->u.variables.tm_input_signal_whitelevel_offset = 0;
	metadata->u.variables.shadow_gain = s_gain;
	metadata->u.variables.highlight_gain = 255;
	metadata->u.variables.mid_tone_width_adj_factor = 64;
	metadata->u.variables.tm_output_finetuning_num_val = 0;
}

/* return 0: not find valid meta; 1: find meta and parser success; < 0: parser fail */
static int parser_metadata_from_sei(struct sei_parser_s *p, bool hevc)
{
	int i, m;
	struct sl_hdr_metadata *metadata = &prime_sl_setting.prime_metadata;

	memset(metadata, 0, sizeof(struct sl_hdr_metadata));

	/* itu_t_t35_country_code */
	m = readU(p, 8);
	if (m != 0xB5) {
		pr_sl(2, "Invalid itu_t_t35_country_code:%x\n", m);
		return is_parser_success(p) ? 0 : -1;
	}

	/* terminal_provider_code */
	m = readU(p, 16);
	if (m != 0x3A) {
		pr_sl(2, "Invalid terminal_provider_code:%x\n", m);
		return is_parser_success(p) ? 0 : -1;
	}

	/* terminal_provider_oriented_code_message_idc */
	m = readU(p, 8);
	if (m != 0 && m != 1) {
		pr_sl(1, "Invalid terminal_provider_oriented_code_message_idc:%d\n", m);
		return -1;
	}
	prime_sl_encode_type = m;

	/* sl_hdr_mode_value_minus1 */
	metadata->part_id = readU(p, 4) + 1;
	if (metadata->part_id != 1 && metadata->part_id != 2) {
		pr_sl(1, "Invalid sl_hdr_mode_value_minus1(part_id):%d\n",
			metadata->part_id);
		return -1;
	}

	/* sl_hdr_spec_major_version_idc */
	metadata->major_spec_version_id = readU(p, 4);
	if (metadata->major_spec_version_id > SEI_VERSION) {
		pr_sl(1, "Unsupported sl_hdr_spec_major_version_idc:%d\n",
			metadata->major_spec_version_id);
		return -1;
	}

	/* sl_hdr_spec_minor_version_idc */
	metadata->minor_spec_version_id = readU(p, 7);
	pr_sl(0x20, "metadata->minor_spec_version_id:%d\n",
		metadata->minor_spec_version_id);

	/* sl_hdr_cancel_flag */
	m = readU(p, 1);
	if (m) {
		pr_sl(1, "Unsupported sl_hdr_cancel_flag:%d\n", m);
		return -1;
	}

	if (!prime_sl_encode_type) {
		/* sl_hdr_persistence_flag */
		pr_sl(1, "265 encode type:%d\n", m);
		m = readU(p, 1);
		if (m) {
			pr_sl(1, "Unsupported sl_hdr_persistence_flag:%d\n", m);
			return -1;
		}
	} else {
		/* sl_hdr_repetition_period */
		pr_sl(1, "264 encode type:%d\n", m);
		m = readU(p, 17);
		if (m) {
			pr_sl(1, "Unsupported sl_hdr_repetition_period:%d\n", m);
			return -1;
		}
	}

	/* original_picture_info_present_flag */
	m = readU(p, 1);
	if (m) {
		/* original_picture_primaries */
		readU(p, 8);
		/* original_picture_max_luminance */
		readU(p, 16);
		/* original_picture_min_luminance */
		readU(p, 16);
	}

	/* target_picture_info_present_flag */
	m = readU(p, 1);
	if (!m) {
		pr_sl(1, "Unsupported target_picture_info_present_flag:%d\n", m);
		return -1;
	}

	/* src_mdcv_info_present_flag */
	m = readU(p, 1);
	if (!m) {
		pr_sl(1, "Unsupported src_mdcv_info_present_flag:%d\n", m);
		return -1;
	}

	/* sl_hdr_extension_present_flag */
	readU(p, 1);

	/* sl_hdr_payload_mode */
	metadata->payload_mode = readU(p, 3);
	if (metadata->payload_mode != 0 && metadata->payload_mode != 1) {
		pr_sl(1, "Invalid sl_hdr_payload_mode:%d\n",
			metadata->payload_mode);
		return -1;
	}

	/* target_picture_primaries */
	m = readU(p, 8);
	if (m != 1 && m != 9) {
		pr_sl(1, "Invalid target_picture_primaries:%d\n", m);
		return -1;
	}
	metadata->sdr_pic_colour_space = m == 1 ? 0 : 1;

	/* target_picture_max_luminance */
	metadata->sdr_display_max_luminance = readU(p, 16);
	if (metadata->sdr_display_max_luminance != 100) {
		pr_sl(1, "Invalid target_picture_max_luminance:%d\n",
			metadata->sdr_display_max_luminance);
		return -1;
	}

	/* target_picture_min_luminance */
	metadata->sdr_display_min_luminance = readU(p, 16);
	if (metadata->sdr_display_min_luminance) {
		pr_sl(1, "Invalid target_picture_max_luminance:%d\n",
			metadata->sdr_display_min_luminance);
		return -1;
	}

	/* src_mdcv_primaries_x[0] */
	m = readU(p, 16);
	if (m == 15000) {
		/* 0.300 * 50000 */
		metadata->hdr_display_colour_space = 0;
	} else if (m == 8500) {
		/* 0.170 * 50000 */
		metadata->hdr_display_colour_space = 1;
	} else if (m == 13250) {
		/* 0.265 * 50000 */
		metadata->hdr_display_colour_space = 2;
	} else {
		pr_sl(1, "Invalid src_mdcv_primaries_x[0]:%d\n", m);
		return -1;
	}

	metadata->hdr_pic_colour_space = (metadata->hdr_display_colour_space ||
		 metadata->sdr_pic_colour_space) ? 1 : 0;
	if (metadata->sdr_pic_colour_space == metadata->hdr_pic_colour_space)
		metadata->sdr_display_colour_space = metadata->hdr_display_colour_space;
	else
		metadata->sdr_display_colour_space = 0;

	/* src_mdcv_primaries_y[0] */
	m = readU(p, 16);
	/* src_mdcv_primaries_x[1] */
	m = readU(p, 16);
	/* src_mdcv_primaries_y[1] */
	m = readU(p, 16);
	/* src_mdcv_primaries_x[2] */
	m = readU(p, 16);
	/* src_mdcv_primaries_y[2] */
	m = readU(p, 16);
	/* src_mdcv_ref_white_x */
	m = readU(p, 16);
	/* src_mdcv_ref_white_y */
	m = readU(p, 16);

	/* src_mdcv_max_mastering_luminance */
	m = readU(p, 16);
	metadata->hdr_display_max_luminance = 50 * ((m + 25) / 50);
	if (metadata->hdr_display_max_luminance > 10000)
		metadata->hdr_display_max_luminance = 10000;

	/* src_mdcv_min_mastering_luminance */
	m = readU(p, 16);
	metadata->hdr_display_min_luminance = (m + 5000) / 10000;

	/* matrix_coefficient_value[i] */
	for (i = 0; i < 4; i++) {
		metadata->matrix_coefficient[i] = readU(p, 16);
		if (metadata->matrix_coefficient[i] > 1023) {
			pr_sl(1, "Invalid matrix_coefficient[%d]:%d\n",
				i, metadata->matrix_coefficient[i]);
			return -1;
		}
	}

	/* chroma_to_luma_injection[i] */
	for (i = 0; i < 2; i++) {
		metadata->chroma_to_luma_injection[i] = readU(p, 16);
		if (metadata->chroma_to_luma_injection[i] > 8191) {
			pr_sl(1, "Invalid chroma_to_luma_injection[%d]:%d\n",
				i, metadata->chroma_to_luma_injection[i]);
			return -1;
		}
	}

	/* k_coefficient_value[i] */
	for (i = 0; i < 3; i++)
		metadata->kcoefficient[i] = readU(p, 8);

	if (metadata->kcoefficient[0] > 63 || metadata->kcoefficient[1] > 127) {
		pr_sl(1, "Invalid kCoefficient[i]:%d %d %d\n",
			metadata->kcoefficient[0],
			metadata->kcoefficient[1],
			metadata->kcoefficient[2]);
		return -1;
	}

	if (!metadata->payload_mode) {
		/* tone_mapping_input_signal_black_level_offset */
		metadata->u.variables.tm_input_signal_blacklevel_offset = readU(p, 8);
		/* tone_mapping_input_signal_white_level_offset */
		metadata->u.variables.tm_input_signal_whitelevel_offset = readU(p, 8);
		/* shadow_gain_control */
		metadata->u.variables.shadow_gain = readU(p, 8);
		/* highlight_gain_control */
		metadata->u.variables.highlight_gain = readU(p, 8);
		/* mid_tone_width_adjustment_factor */
		metadata->u.variables.mid_tone_width_adj_factor = readU(p, 8);
		/* tone_mapping_output_fine_tuning_num_val */
		metadata->u.variables.tm_output_finetuning_num_val = readU(p, 4);
		if (metadata->u.variables.tm_output_finetuning_num_val > 10) {
			pr_sl(1, "Invalid tone_mapping_output_fine_tuning_num_val:%d\n",
				metadata->u.variables.tm_output_finetuning_num_val);
			return -1;
		}
		/* saturation_gain_num_val */
		metadata->u.variables.saturation_gain_num_val = readU(p, 4);
		if (metadata->u.variables.saturation_gain_num_val > 6) {
			pr_sl(1, "Invalid saturation_gain_num_val:%d\n",
				metadata->u.variables.saturation_gain_num_val);
			return -1;
		}
		for (i = 0; i < metadata->u.variables.tm_output_finetuning_num_val; i++) {
			/* tone_mapping_output_fine_tuning_x[i] */
			metadata->u.variables.tm_output_finetuning_x[i] = readU(p, 8);
			/* tone_mapping_output_fine_tuning_y[i] */
			metadata->u.variables.tm_output_finetuning_y[i] = readU(p, 8);
		}
		for (i = 0; i < metadata->u.variables.saturation_gain_num_val; i++) {
			/* saturation_gain_x[i] */
			metadata->u.variables.saturation_gain_x[i] = readU(p, 8);
			/* saturation_gain_y[i] */
			metadata->u.variables.saturation_gain_y[i] = readU(p, 8);
		}
	} else {
		/* lm_uniform_sampling_flag */
		m = readU(p, 1);
		/* luminance_mapping_num_val */
		metadata->u.tables.luminance_mapping_num_val = readU(p, 7);
		if (metadata->u.tables.luminance_mapping_num_val != 65) {
			pr_sl(1, "Unsupported luminance_mapping_num_val:%d\n",
				metadata->u.tables.luminance_mapping_num_val);
			return -1;
		}
		for (i = 0; i < metadata->u.tables.luminance_mapping_num_val; i++) {
			if (!m) {
				/* luminance_mapping_x[i] */
				metadata->u.tables.luminance_mapping_x[i] = readU(p, 16);
				if (metadata->u.tables.luminance_mapping_x[i] > 8192) {
					pr_sl(1, "Invalid luminance_mapping_x[%d]:%d\n",
						i, metadata->u.tables.luminance_mapping_x[i]);
					return -1;
				}
			} else {
				metadata->u.tables.luminance_mapping_x[i] = i << 7;
			}
			/* luminance_mapping_y[i] */
			metadata->u.tables.luminance_mapping_y[i] = readU(p, 16);
			if (metadata->u.tables.luminance_mapping_y[i] > 8191) {
				pr_sl(1, "Invalid luminance_mapping_y[%d]:%d\n",
					i, metadata->u.tables.luminance_mapping_y[i]);
				return -1;
			}
		}
		/* cc_uniform_sampling_flag */
		m = readU(p, 1);
		/* colour_correction_num_val */
		metadata->u.tables.colour_correction_num_val = readU(p, 7);
		if (metadata->u.tables.colour_correction_num_val != 65) {
			pr_sl(1, "Unsupported colour_correction_num_val\n");
			return -1;
		}
		for (i = 0; i < metadata->u.tables.colour_correction_num_val; i++) {
			if (!m) {
				/* colour_correction_x[i] */
				metadata->u.tables.colour_correction_x[i] = readU(p, 16);
				if (metadata->u.tables.colour_correction_x[i] > 2048) {
					pr_sl(1, "Invalid colour_correction_x[%d]:%d\n",
						i, metadata->u.tables.colour_correction_x[i]);
					return -1;
				}
			} else {
				metadata->u.tables.colour_correction_x[i] = i << 5;
			}
			/* colour_correction_y[i] */
			metadata->u.tables.colour_correction_y[i] = readU(p, 16);
			if (metadata->u.tables.colour_correction_y[i] > 2047) {
				pr_sl(1, "Invalid colour_correction_y[%d]:%d\n",
					i, metadata->u.tables.colour_correction_y[i]);
				return -1;
			}
		}
	}

	if (metadata->sdr_pic_colour_space < metadata->hdr_pic_colour_space) {
		if (metadata->part_id == 1) {
			metadata->hdr_pic_colour_space = 0;
			metadata->hdr_display_colour_space = 0;
		}

		if (metadata->part_id == 2)
			metadata->sdr_pic_colour_space = 1;
	}
	return 1;
}

/* return 0: not find valid sei; 1: find sei and parser success; < 0: parser fail */
static int parser_prime_sei(struct sei_parser_s *parser,
		char *sei_buf, u32 size)
{
	char *p = sei_buf;
	char *p_sei;
	u16 header;
	u16 nal_unit_type;
	u16 payload_type, payload_size;
	int ret = 0;

	if (size < 2)
		return ret;

	if (*p == 0xB5	&& ((*(p + 1) << 8) | *(p + 2)) == PRIME_SL_T35_PROV_CODE) {
		p_sei = p;
		init_sei_parser(parser, p_sei, size - (p_sei - sei_buf));
		ret = parser_metadata_from_sei(parser, true);
		if (ret < 0) {
			pr_sl(0x20,
				"Invalid or Unsupported metadata : recovery mode is used\n");
			return 0;
		}
		return ret;
	}

	header = *p++;
	header <<= 8;
	header += *p++;
	nal_unit_type = header >> 9;
	if (nal_unit_type != NAL_UNIT_SEI &&
	    nal_unit_type != NAL_UNIT_SEI_SUFFIX)
		return 0;

	while (p + 4 <= sei_buf + size) {
		payload_type = *p++;
		payload_size = *p++;
		if (payload_size == 0xff)
			payload_size += *p++;
		if (p + payload_size <= sei_buf + size) {
			switch (payload_type) {
			case SEI_ITU_T_T35:
				p_sei = p;
				init_sei_parser(parser, p_sei, size - (p_sei - sei_buf));
				ret = parser_metadata_from_sei(parser, true);
				if (ret < 0) {
					pr_sl(0x20,
						"Invalid or Unsupported metadata : recovery mode is used\n");
					return 0;
				}
				break;
			default:
				break;
			}
		}
		p += payload_size;
	}
	return ret;
}

static int prime_sl_parser_metadata(struct vframe_s *vf)
{
	struct provider_aux_req_s req;
	struct prime_cfg_t *cfg = &prime_sl_setting.cfg;
	struct sei_parser_s *parser = &prime_sl_parser;
	unsigned int size = 0, type = 0;
	int ret = -1, i, j, len;
	char *p, *meta_buf;
	bool prime_sl_video = false;

	if (!vf || vf->source_type != VFRAME_SOURCE_TYPE_OTHERS)
		return ret;

	cfg->width = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compWidth : vf->width;
	cfg->height = (vf->type & VIDTYPE_COMPRESS) ?
		vf->compHeight : vf->height;
	cfg->bit_depth = vf->bitdepth;
	cfg->yuv_range = (vf->signal_type >> 25) & 0x1;
	cfg->display_brightness = get_prime_sl_display_brightness();
	cfg->display_oetf = get_prime_sl_output_curve();
	switch (prime_sl_display_adaptation_tuning) {
	case DAT_OFF:
		cfg->display_adaptation_tuning = 65536;
		break;
	case DAT_LOW:
		cfg->display_adaptation_tuning = 81002;
		break;
	case DAT_MEDIAN:
		cfg->display_adaptation_tuning = 131072;
		break;
	case DAT_HIGH:
		cfg->display_adaptation_tuning = 212074;
		break;
	default:
		pr_sl(1, "Invalid display_adaptation_tuning value\n");
	}

	req.vf = vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	req.low_latency = 0;
	if (get_vframe_src_fmt(vf) ==
	    VFRAME_SIGNAL_FMT_HDR10PRIME) {
		size = 0;
		req.aux_buf = (char *)get_sei_from_src_fmt(vf, &size);
		req.aux_size = size;
		prime_sl_video = true;
		if (!req.aux_buf || !req.aux_size ||
			!is_valid_prime_sl_metadata(req.aux_buf, req.aux_size)) {
			set_prime_sl_frame(0);
			if (signal_transfer_characteristic == 0x10)
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_HDR10;
			else
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_SDR;
			pr_sl(1, "empty or invalid metadata , update fmt\n");
			prime_sl_video = false;
			return ret;
		}
	} else {
		vf_notify_provider_by_name("vdec.h265.00",
			   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
			   (void *)&req);
		if (!req.aux_buf)
			vf_notify_provider_by_name("decoder",
				   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				   (void *)&req);
	}

	if (req.aux_buf && req.aux_size &&
	    (prime_sl_debug & 0x10)) {
		meta_buf = req.aux_buf;
		for (i = 0; i < req.aux_size + 8; i += 8) {
			len = req.aux_size - i;
			if (len < 8) {
				pr_sl(0x10, "\t i = %02d", i);
				for (j = 0; j < len; j++)
					pr_sl(0x10, "%02x ", meta_buf[i + j]);
				pr_sl(0x10, "\n");
			} else {
				pr_sl(0x10,
					"\t i = %02d: %02x %02x %02x %02x %02x %02x %02x %02x\n",
					i,
					meta_buf[i],
					meta_buf[i + 1],
					meta_buf[i + 2],
					meta_buf[i + 3],
					meta_buf[i + 4],
					meta_buf[i + 5],
					meta_buf[i + 6],
					meta_buf[i + 7]);
			}
		}
	}

	if (req.aux_buf && req.aux_size) {
		u32 count = 0;
		u32 offest = 0;
		bool done = false, success = false;
		int iret;

		p = req.aux_buf;
		while (p < req.aux_buf + req.aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			offest += 8;
			if (offest + size > req.aux_size) {
				pr_err("%s exception: t:%x, s:%d, p:%px, c:%d, offt:%d, aux:%px, s:%d\n",
					__func__,
					type, size, p, count, offest,
					req.aux_buf, req.aux_size);
				break;
			}
			if (type == 0x02000000) {
				iret = parser_prime_sei(parser, p, size);
				if (iret) {
					success = (iret > 0) ? true : false;
					done = true;
					pr_sl(1, "parser meta data done. %s\n",
						success ? "success" : "fail");
					break;
				}
			}
			count++;
			offest += size;
			p += size;
		}
		/* assume that it's a prime_sl video or parser fail */
		if (!success && (prime_sl_video || done)) {
			fill_metadata_recovery(&prime_sl_setting.prime_metadata,
				prime_sl_display_brightness,
				signal_color_primaries,
				signal_transfer_characteristic);
			ret = 0;
			pr_sl(1, "use recovery mode for prime meta\n");
		} else if (success) {
			ret = 0;
		}
	}
	return ret;
}

void prime_sl_pre_check(struct vframe_s *vf)
{
	if (vf && vf->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10PRIME) {
		int size = 0;
		char *aux_buf = (char *)get_sei_from_src_fmt(vf, &size);
		int aux_size = size;

		if (!aux_buf || !aux_size ||
			!is_valid_prime_sl_metadata(aux_buf, aux_size)) {
			set_prime_sl_frame(0);
			vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_HDR10;
		} else {
			if (!get_prime_sl_frame())
				set_prime_sl_frame(1);
		}
	} else if (get_prime_sl_frame() && vf &&
		vf->src_fmt.fmt != VFRAME_SIGNAL_FMT_HDR10PRIME) {
		set_prime_sl_frame(0);
	}
}
EXPORT_SYMBOL(prime_sl_pre_check);

void prime_sl_process(struct vframe_s *vf)
{
	static struct vframe_s *last_vf;
	bool new_vf = false;

	/* skip mode */
	if (prime_sl_debug_run_mode == 3)
		return;

	if (is_meson_t6w() || is_meson_t6x()) {
		prime_sl_dpss_mode = dpss_mode;
		pr_sl(0x200, "%s:dpss_mode from vecm =%d\n", __func__, dpss_mode);
	} else {
		prime_sl_dpss_mode = 0;
	}

	/* load fixed txt setting mode */
	if (prime_sl_debug_run_mode == 1) {
		pr_sl(1, "use fixed txt setting mode\n");
		if (vf) {
			if (!prime_sl_running)
				dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
			prime_sl_set_reg(&psl);
			prime_sl_running = 1;
		} else {
			prime_sl_close();
			prime_sl_running = 0;
			set_prime_sl_frame(0);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		}
		return;
	}

	if (!prime_sl_probe || !prime_sl_enable || !p_funcs)
		return;

	if (!prime_sl_dpss_mode) {
		if (vf && vf->src_fmt.fmt != VFRAME_SIGNAL_FMT_HDR10PRIME) {
			if (prime_sl_running) {
				prime_sl_close();
				prime_sl_running = 0;
			}
			return;
		}

		if (is_amdv_on())
			return;

		if (last_vf != vf && vf)
			new_vf = true;
		last_vf = vf;

		if (prime_sl_enable && p_funcs && new_vf) {
			if (!prime_sl_parser_metadata(vf)) {
				p_funcs->prime_metadata_parser_process(&prime_sl_setting);
				if (!prime_sl_running)
					dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
				if (prime_sl_debug & 0x200) {
					dbg_metadata(&prime_sl_setting.prime_metadata);
					dbg_config(&prime_sl_setting.cfg);
					dbg_setting(&prime_sl_setting.prime_sl);
				}
				prime_sl_set_reg(&prime_sl_setting.prime_sl);
				prime_sl_running = 1;
			} else {
				if (prime_sl_running) {
					prime_sl_close();
					prime_sl_running = 0;
				}
			}
		} else if (prime_sl_running &&
		(!get_video_enabled(0) || !vf)) {
			/* exit play primesl need close */
			prime_sl_close();
			prime_sl_running = 0;
			set_prime_sl_frame(0);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		}
	}
}
EXPORT_SYMBOL(prime_sl_process);

void prime_sl_process_for_dpss(struct vframe_s *vf)
{
	static struct vframe_s *last_vf;
	bool new_vf = false;

	/* skip mode */
	if (prime_sl_debug_run_mode == 3)
		return;

	if (is_meson_t6w() || is_meson_t6x()) {
		prime_sl_dpss_mode = dpss_mode;
		pr_sl(0x200, "%s:dpss_mode from vecm =%d\n", __func__, dpss_mode);
	} else {
		prime_sl_dpss_mode = 0;
	}
	/* load fixed txt setting mode */
	if (prime_sl_debug_run_mode == 1) {
		pr_sl(1, "use fixed txt setting mode\n");
		if (vf) {
			if (!prime_sl_running)
				dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
			prime_sl_set_reg(&psl);
			prime_sl_running = 1;
		} else {
			prime_sl_close();
			prime_sl_running = 0;
			set_prime_sl_frame(0);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		}
		return;
	}

	if (!prime_sl_probe || !prime_sl_enable || !p_funcs)
		return;

	if (vf && vf->src_fmt.fmt != VFRAME_SIGNAL_FMT_HDR10PRIME) {
		if (prime_sl_running) {
			prime_sl_close();
			prime_sl_running = 0;
		}
		return;
	}

	if (prime_sl_dpss_mode) {
		if (vf && vf->src_fmt.fmt != VFRAME_SIGNAL_FMT_HDR10PRIME) {
			if (prime_sl_running) {
				prime_sl_close();
				prime_sl_running = 0;
				ampsl_dpss_display_counts = 0;
			}
			return;
		}

		if (last_vf != vf && vf) {
			new_vf = true;
			ampsl_dpss_display_counts++;
		}
		last_vf = vf;

		if (prime_sl_enable && p_funcs && new_vf) {
			if (!prime_sl_parser_metadata(vf)) {
				p_funcs->prime_metadata_parser_process(&prime_sl_setting);
				if (!prime_sl_running)
					dv_mem_power_on(VPU_PRIME_DOLBY_RAM);
				if (prime_sl_debug & 0x200) {
					dbg_metadata(&prime_sl_setting.prime_metadata);
					dbg_config(&prime_sl_setting.cfg);
					dbg_setting(&prime_sl_setting.prime_sl);
				}
				prime_sl_set_reg(&prime_sl_setting.prime_sl);
				prime_sl_running = 1;
			} else {
				if (prime_sl_running) {
					prime_sl_close();
					prime_sl_running = 0;
				}
			}
		} else if (prime_sl_running &&
		(!get_video_enabled(0) || !vf)) {
			/* exit play primesl need close */
			prime_sl_close();
			prime_sl_running = 0;
			set_prime_sl_frame(0);
			dv_mem_power_off(VPU_PRIME_DOLBY_RAM);
		}
	}
}
EXPORT_SYMBOL(prime_sl_process_for_dpss);

static void dbg_setting(struct prime_sl_t *prime_sl)
{
	unsigned int i;

	pr_info("\t legacy_mode_en\t %d\n", prime_sl->legacy_mode_en);
	pr_info("\t clip_en\t %d\n", prime_sl->clip_en);
	pr_info("\t reg_gclk_ctrl\t %d\n", prime_sl->reg_gclk_ctrl);
	pr_info("\t gclk_ctrl\t %d\n", prime_sl->gclk_ctrl);
	pr_info("\t primesl_en\t %d\n",
		prime_sl->primesl_en);
	pr_info("\t inv_chroma_ratio\t %d\n",
		prime_sl->inv_chroma_ratio);
	pr_info("\t inv_y_ratio\t %d\n", prime_sl->inv_y_ratio);
	pr_info("\t l_headroom\t %d\n", prime_sl->l_headroom);
	pr_info("\t footroom\t %d\n", prime_sl->footroom);
	pr_info("\t c_headroom\t %d\n", prime_sl->c_headroom);
	pr_info("\t mub\t %d\n",
		prime_sl->mub);
	pr_info("\t mua\t %d\n", prime_sl->mua);

	for (i = 0; i < 7; i++)
		pr_info("\t oct[%d]\t %d\n", i, prime_sl->oct[i]);
	for (i = 0; i < 3; i++)
		pr_info("\t d_lut_threshold[%d]\t %d\n", i,
		prime_sl->d_lut_threshold[i]);
	for (i = 0; i < 4; i++)
		pr_info("\t d_lut_step[%d]\t %d\n", i, prime_sl->d_lut_step[i]);
	for (i = 0; i < 9; i++)
		pr_info("\t rgb2yuv[%d]\t %d\n", i, prime_sl->rgb2yuv[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_c[%d]\t %d\n", i, prime_sl->lut_c[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_p[%d]\t %d\n", i, prime_sl->lut_p[i]);
	for (i = 0; i < 65; i++)
		pr_info("\t lut_d[%d]\t %d\n", i, prime_sl->lut_d[i]);
}

static void dbg_config(struct prime_cfg_t *cfg)
{
	pr_info("\t width\t %d\n", cfg->width);
	pr_info("\t height\t %d\n", cfg->height);
	pr_info("\t bit_depth\t %d\n", cfg->bit_depth);
	pr_info("\t display_oetf\t %d\n", cfg->display_oetf);
	pr_info("\t yuv_range\t %d\n", cfg->yuv_range);
	pr_info("\t display_brightness\t %d\n",
		cfg->display_brightness);
	pr_info("\t display_adaptation_tuning \t %d\n",
		cfg->display_adaptation_tuning);
}

static void dbg_metadata(struct sl_hdr_metadata *pmetadata)
{
	unsigned int i;

	pr_info("\t partID\t %d\n", pmetadata->part_id);
	pr_info("\t majorSpecVersionID\t %d\n",
		pmetadata->major_spec_version_id);
	pr_info("\t minorSpecVersionID\t %d\n",
		pmetadata->minor_spec_version_id);
	pr_info("\t payloadMode\t %d\n",
		pmetadata->payload_mode);
	pr_info("\t hdrPicColourSpace\t %d\n",
		pmetadata->hdr_pic_colour_space);
	pr_info("\t hdrDisplayColourSpace\t %d\n",
		pmetadata->hdr_display_colour_space);
	pr_info("\t hdrDisplayMaxLuminance\t %d\n",
		pmetadata->hdr_display_max_luminance);
	pr_info("\t hdrDisplayMinLuminance\t %d\n",
		pmetadata->hdr_display_min_luminance);
	pr_info("\t sdrPicColourSpace\t %d\n",
		pmetadata->sdr_pic_colour_space);
	pr_info("\t sdrDisplayColourSpace\t %d\n",
		pmetadata->sdr_display_colour_space);
	pr_info("\t sdrDisplayMaxLuminance\t %d\n",
		pmetadata->sdr_display_max_luminance);
	pr_info("\t sdrDisplayMinLuminance\t %d\n",
		pmetadata->sdr_display_min_luminance);
	for (i = 0; i < 4; i++)
		pr_info("\t matrix_coefficient[%d]\t %d\n",
			i, pmetadata->matrix_coefficient[i]);
	for (i = 0; i < 2; i++)
		pr_info("\t chroma_to_luma_injection[%d]\t %d\n",
			i, pmetadata->chroma_to_luma_injection[i]);
	for (i = 0; i < 3; i++)
		pr_info("\t kcoefficient[%d]\t %d\n",
			i, pmetadata->kcoefficient[i]);
	pr_info("\t tm_input_signal_blacklevel_offset\t %d\n",
		pmetadata->u.variables.tm_input_signal_blacklevel_offset);
	pr_info("\t tm_input_signal_whitelevel_offset\t %d\n",
		pmetadata->u.variables.tm_input_signal_whitelevel_offset);
	pr_info("\t shadow_gain\t %d\n",
		pmetadata->u.variables.shadow_gain);
	pr_info("\t highlight_gain\t %d\n",
		pmetadata->u.variables.highlight_gain);
	pr_info("\t mid_tone_width_adj_factor\t %d\n",
		pmetadata->u.variables.mid_tone_width_adj_factor);
	pr_info("\t tm_output_finetuning_num_val\t %d\n",
		pmetadata->u.variables.tm_output_finetuning_num_val);
	for (i = 0; i < 10; i++)
		pr_info("\t tm_output_finetuning_x[%d]\t %d\n",
			i, pmetadata->u.variables.tm_output_finetuning_x[i]);
	for (i = 0; i < 10; i++)
		pr_info("\t tm_output_finetuning_y[%d]\t %d\n",
			i, pmetadata->u.variables.tm_output_finetuning_y[i]);
	pr_info("\t saturation_gain_num_val\t%d\n",
		pmetadata->u.variables.saturation_gain_num_val);
	for (i = 0; i < 6; i++) {
		pr_info("\t saturation_gain_x[%d]\t %d\n",
			i, pmetadata->u.variables.saturation_gain_x[i]);
		pr_info("\t saturation_gain_y[%d]\t %d\n",
			i, pmetadata->u.variables.saturation_gain_y[i]);
	}
}

void parse_param_amprime_sl(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
		if (n >= MAX_PARAM)
			break;
	}
}

static const char *amprime_sl_debug_usage_str = {
	"Usage:\n"
	"echo metadata > /sys/class/amprime_sl/debug; get metadata info\n"
	"echo setting > /sys/class/amprime_sl/debug; get reg set info\n"
	"echo prime_sl_running > /sys/class/amprime_sl/debug; get module working status\n"
	"echo prime_sl_probe > /sys/class/amprime_sl/debug; get module probe status\n"
	"echo prime_sl_output_curve > /sys/class/amprime_sl/debug; get HDR module process curve\n"
	"echo prime_sl_debug_run_mode value > /sys/class/amprime_sl/debug; set work mode\n"
	"echo prime_sl_hdr_mode > /sys/class/amprime_sl/debug; get HDR module process status\n"
	"echo prime_sl_certification_mode value > /sys/class/amprime_sl/debug; set cert mode\n"
	"echo prime_sl_debug value > /sys/class/amprime_sl/debug; set debug level\n"
};

static ssize_t amprime_sl_debug_show
		(const struct class *cla,
		 const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",  amprime_sl_debug_usage_str);
}

static ssize_t amprime_sl_debug_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	long val = 0;
	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amprime_sl(buf_orig, (char **)&parm);
	if (!strcmp(parm[0], "metadata")) {
		dbg_metadata(&prime_sl_setting.prime_metadata);
		dbg_config(&prime_sl_setting.cfg);
	} else if (!strcmp(parm[0], "setting")) {
		dbg_setting(&prime_sl_setting.prime_sl);
	} else if (!strcmp(parm[0], "prime_sl_running")) {
		pr_info("prime_sl_running = %d\n", prime_sl_running);
	} else if (!strcmp(parm[0], "prime_sl_probe")) {
		pr_info("prime_sl_probe = %d\n", prime_sl_probe);
	} else if (!strcmp(parm[0], "prime_sl_output_curve")) {
		pr_info("prime_sl_output_curve = %d\n", prime_sl_output_curve);
	} else if (!strcmp(parm[0], "prime_sl_debug_run_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		prime_sl_debug_run_mode = val;
		pr_info("set prime_sl_debug_run_mode = %d\n", prime_sl_debug_run_mode);
	} else if (!strcmp(parm[0], "prime_sl_hdr_mode")) {
		pr_info("prime_sl_hdr_mode = %d\n", prime_sl_hdr_mode);
	} else if (!strcmp(parm[0], "prime_sl_certification_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		prime_sl_certification_mode = val;
		pr_info("set prime_sl_certification_mode = %d\n",
			prime_sl_certification_mode);
	} else if (!strcmp(parm[0], "prime_sl_debug")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			return -EINVAL;
		prime_sl_debug = val;
		pr_info("set prime_sl_debug = %d\n",
			prime_sl_debug);
	} else {
		pr_info("unsupport commands\n");
	}

	kfree(buf_orig);
	return count;
}

#define MAX_PATH_LEN 256
/* path: "/vendor/lib/firmware/ampsl/" */
#define FIRMWARE_DIR "ampsl"
int ampsl_load_file(char *fw_name, void **dst_buf)
{
	int ret = 0;
	const struct firmware *fw = NULL;
	char name[MAX_PATH_LEN] = {0};
	char ampsl_name[MAX_PATH_LEN] = {0};
	char *tmp = NULL;
	struct device *dev = NULL;

	dev = get_ampsl_device();

	if (!fw_name || !dev) {
		pr_sl(1, "NULL param, %s (%d)\n", __func__, __LINE__);
		return -EINVAL;
	}

	/*remove directory, only need file name*/
	tmp = strrchr(fw_name, '/');
	if (tmp)
		strncpy(name, tmp + 1, MAX_PATH_LEN - 1);
	else
		strncpy(name, fw_name, MAX_PATH_LEN - 1);

	snprintf(ampsl_name, (MAX_PATH_LEN - 1), "%s/%s",
		 FIRMWARE_DIR, name);

	ret = request_firmware(&fw, ampsl_name, dev);
	if (ret < 0) {
		pr_sl(1, "Error : %d can't load the %s.\n", ret, ampsl_name);
		return -ENOENT;
	}

	if (fw->size <= 0) {
		pr_sl(1, "size error, wrong firmware or no enough mem.\n");
		ret = -ENOMEM;
		goto release;
	}

	*dst_buf = vmalloc(fw->size + 2);
	if (!(*dst_buf)) {
		ret = -ENOMEM;
		goto release;
	}
	memset(*dst_buf, 0, fw->size + 2);
	memcpy(*dst_buf, (char *)fw->data, fw->size);

	pr_info("load file size: %zd, name: %s.\n",
		fw->size, name);

release:
	release_firmware(fw);
	return ret;
}

#define MAX_READ_SIZE 256
static char cur_line[MAX_READ_SIZE];
static bool ampsl_read_one_line(char **text_buf, char *line_buf)
{
	char *line_end;
	size_t line_len = 0;
	bool eof_flag = false;

	if (!text_buf || !(*text_buf) || !line_buf) {
		pr_info("error,line_buf %p, text_buf %p\n", line_buf, text_buf);
		return true;
	}

	line_buf[0] = '\0';
	while (*line_buf == '\0') {
		line_end = strnchr(*text_buf, 128, '\n');
		if (!line_end) {
			line_len = strlen(*text_buf);
			eof_flag = true;
		} else {
			line_len = (size_t)(line_end - *text_buf);
		}
		memcpy(line_buf, *text_buf, line_len);
		line_buf[line_len] = '\0';
		if (line_len > 0 && line_buf[line_len - 1] == '\r')
			line_buf[line_len - 1] = '\0';

		*text_buf = *text_buf + line_len + 1;
		while (isspace(*line_buf))
			line_buf++;

		if (**text_buf == '\0')
			eof_flag = true;

		if (eof_flag)
			break;
	}

	return eof_flag;
}

/*is_reg: true: reg; false: lut*/
int ampsl_read_txt_to_buf(char *reg_txt, void *reg_buf, int reg_num)
{
	char *ptr_line;
	char *last_space;
	int count = 0;
	int ret = 0;
	bool eof_flag = false;
	u32 *p_buf = (u32 *)reg_buf;
	u32 num;

	if (!reg_txt || !reg_buf)
		return count;

	while (!eof_flag) {
		eof_flag = ampsl_read_one_line(&reg_txt, (char *)&cur_line);
		ptr_line = cur_line;
		if (eof_flag && (strlen(cur_line) == 0))
			break;
		last_space = strrchr(ptr_line, ' ');
		if (!last_space) {
			pr_sl(1, "error line format\n");
			return -1;
		}
		ret = sscanf(last_space + 1, "%d", &num);
		if (ret == 1)
			p_buf[count++] = num;
		else
			pr_sl(1, "fail to extract num\n");
	}
	if (reg_num == count)
		pr_info("successfully load all setting regs!\n");
	return count;
}

void ampsl_assign_to_struct(struct prime_sl_t *psl, const unsigned int *data)
{
	int idx = 0;
	int i = 0;

	psl->inv_chroma_ratio = data[idx++];
	psl->inv_y_ratio = data[idx++];
	psl->l_headroom = data[idx++];
	psl->footroom = data[idx++];
	psl->c_headroom = data[idx++];
	psl->mub = data[idx++];
	psl->mua = data[idx++];
	for (i = 0; i < 7; i++)
		psl->oct[i] = data[idx++];
	for (i = 0; i < 3; i++)
		psl->d_lut_threshold[i] = data[idx++];
	for (i = 0; i < 4; i++)
		psl->d_lut_step[i] = data[idx++];
	for (i = 0; i < 9; i++)
		psl->rgb2yuv[i] = data[idx++];
	for (i = 0; i < 65; i++)
		psl->lut_c[i] = data[idx++];
	for (i = 0; i < 65; i++)
		psl->lut_p[i] = data[idx++];
	for (i = 0; i < 65; i++)
		psl->lut_d[i] = data[idx++];
}

static ssize_t amprime_sl_load_reg_file_show
	(const struct class *cla,
	 const struct class_attribute *attr,
	 char *buf)
{
	return 0;
}

/*para1:file type, para2: reg_file_name*/
static ssize_t amprime_sl_load_reg_file_store
		(const struct class *cla,
		 const struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[MAX_PARAM] = {NULL};
	void *prime_sl_setting_txt = NULL;
	int txt_line = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amprime_sl(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "prime_sl_setting")) {
		ampsl_load_file(parm[1], &prime_sl_setting_txt);
		if (!prime_sl_reg_setting_buf) {
			prime_sl_reg_setting_buf = vmalloc(prime_sl_reg_setting_num * sizeof(u32));
			if (!prime_sl_reg_setting_buf) {
				if (prime_sl_setting_txt)
					vfree(prime_sl_setting_txt);
				return count;
			}
		}
		txt_line = ampsl_read_txt_to_buf(prime_sl_setting_txt,
			prime_sl_reg_setting_buf, prime_sl_reg_setting_num);
	}
	if (txt_line == prime_sl_reg_setting_num)
		ampsl_assign_to_struct(&psl, (unsigned int *)prime_sl_reg_setting_buf);
	if (prime_sl_setting_txt)
		vfree(prime_sl_setting_txt);
	return count;
}

static struct class_attribute amprime_sl_class_attrs[] = {
	__ATTR(debug, 0644, amprime_sl_debug_show, amprime_sl_debug_store),
	__ATTR(load_primesl_reg_file, 0644,
	       amprime_sl_load_reg_file_show,
	       amprime_sl_load_reg_file_store),
	__ATTR_NULL
};

int register_prime_functions(const struct hdr_prime_sl_func_s *func)
{
	int ret = -1;

	if (!p_funcs && func) {
		pr_info("*** %s. version %s ***\n",
			__func__, func->version_info);
		ret = 0;
		p_funcs = func;
		p_funcs->prime_api_init();
	}
	return ret;
}
EXPORT_SYMBOL(register_prime_functions);

int unregister_prime_functions(void)
{
	int ret = -1;

	if (p_funcs) {
		pr_info("*** %s ***\n", __func__);
		p_funcs->prime_api_exit();
		p_funcs = NULL;
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(unregister_prime_functions);

static struct prime_sl_device_data_s prime_sl_g12 = {
	.cpu_id = _CPU_MAJOR_ID_G12,
};

static struct prime_sl_device_data_s prime_sl_tl1 = {
	.cpu_id = _CPU_MAJOR_ID_TL1,
};

static struct prime_sl_device_data_s prime_sl_tm2 = {
	.cpu_id = _CPU_MAJOR_ID_TM2,
};

static struct prime_sl_device_data_s prime_sl_sc2 = {
	.cpu_id = _CPU_MAJOR_ID_SC2,
};

static struct prime_sl_device_data_s prime_sl_t7 = {
	.cpu_id = _CPU_MAJOR_ID_T7,
};

static struct prime_sl_device_data_s prime_sl_t3 = {
	.cpu_id = _CPU_MAJOR_ID_T3,
};

static struct prime_sl_device_data_s prime_sl_s4d = {
	.cpu_id = _CPU_MAJOR_ID_S4D,
};

static struct prime_sl_device_data_s prime_sl_t5w = {
	.cpu_id = _CPU_MAJOR_ID_T5W,
};

static struct prime_sl_device_data_s prime_sl_t5m = {
	.cpu_id = _CPU_MAJOR_ID_T5M,
};

static struct prime_sl_device_data_s prime_sl_s7d = {
	.cpu_id = _CPU_MAJOR_ID_S7D,
};

static struct prime_sl_device_data_s prime_sl_s6 = {
	.cpu_id = _CPU_MAJOR_ID_S6,
};

static struct prime_sl_device_data_s prime_sl_t6w = {
	.cpu_id = _CPU_MAJOR_ID_T6W,
};

static struct prime_sl_device_data_s prime_sl_t6x = {
	.cpu_id = _CPU_MAJOR_ID_T6X,
};

static const struct of_device_id amprime_sl_match[] = {
	{
		.compatible = "amlogic, prime_sl_g12",
		.data = &prime_sl_g12,
	},
	{
		.compatible = "amlogic, prime_sl_tl1",
		.data = &prime_sl_tl1,
	},
	{
		.compatible = "amlogic, prime_sl_tm2",
		.data = &prime_sl_tm2,
	},
	{
		.compatible = "amlogic, prime_sl_sc2",
		.data = &prime_sl_sc2,
	},
	{
		.compatible = "amlogic, prime_sl_t7",
		.data = &prime_sl_t7,
	},
	{
		.compatible = "amlogic, prime_sl_t3",
		.data = &prime_sl_t3,
	},
	{
		.compatible = "amlogic, prime_sl_s4d",
		.data = &prime_sl_s4d,
	},
	{
		.compatible = "amlogic, prime_sl_t5w",
		.data = &prime_sl_t5w,
	},
	{
		.compatible = "amlogic, prime_sl_t5m",
		.data = &prime_sl_t5m,
	},
	{
		.compatible = "amlogic, prime_sl_s7d",
		.data = &prime_sl_s7d,
	},
	{
		.compatible = "amlogic, prime_sl_s6",
		.data = &prime_sl_s6,
	},
	{
		.compatible = "amlogic, prime_sl_t6w",
		.data = &prime_sl_t6w,
	},
	{
		.compatible = "amlogic, prime_sl_t6x",
		.data = &prime_sl_t6x,
	},
	{},
};

static int amprime_sl_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned int i = 0;
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct prime_sl_device_data_s *prime_sl_meson;
		struct device_node *of_node = pdev->dev.of_node;

		match = of_match_node(amprime_sl_match, of_node);
		if (match) {
			prime_sl_meson =
				(struct prime_sl_device_data_s *)match->data;
			if (prime_sl_meson) {
				memcpy(&prime_sl_meson_dev, prime_sl_meson,
					sizeof(struct prime_sl_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	}
	memset(devp, 0, (sizeof(struct amprime_sl_dev_s)));
	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMPRIME_SL_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(AMPRIME_SL_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	for (i = 0;  amprime_sl_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp,
			&amprime_sl_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}
/*	cdev_init(&devp->cdev, &amprime_sl_fops);
 *	devp->cdev.owner = THIS_MODULE;
 *	ret = cdev_add(&devp->cdev, devp->devno, 1);
 *	if (ret)
 *		goto fail_add_cdev;
 */
	devp->dev = device_create(devp->clsp, NULL, devp->devno,
		NULL, AMPRIME_SL_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
	prime_sl_probe = 1;
	pr_info("%s: probe ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amprime_sl.] : amprime_sl device create error.\n");
 /*	cdev_del(&devp->cdev);
  *
  *fail_add_cdev:
  *	pr_info("[amprime_sl.] : amprime_sl add device error.\n");
  */
fail_class_create_file:
	pr_info("[amprime_sl.] : amprime_sl class create file error.\n");
	for (i = 0; amprime_sl_class_attrs[i].attr.name; i++)
		class_remove_file(devp->clsp,
			&amprime_sl_class_attrs[i]);
	class_destroy(devp->clsp);
fail_create_class:
	pr_info("[amprime_sl.] : amprime_sl class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
fail_alloc_region:
	pr_info("[amprime_sl.] : amprime_sl alloc error.\n");
	return ret;
}

static __maybe_unused void amprime_sl_remove(struct platform_device *pdev)
{
	struct amprime_sl_dev_s *devp = &amprime_sl_dev;

	prime_sl_probe = 0;
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
}

static struct platform_driver amprime_sl_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "amprime_sl",
		.of_match_table = amprime_sl_match,
	},
	.probe = amprime_sl_probe,
	.remove = __exit_p(amprime_sl_remove),
};

int __init amprime_sl_init(void)
{
	if (platform_driver_register(&amprime_sl_driver)) {
		pr_info("failed to register amprime_sl module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit amprime_sl_exit(void)
{
	platform_driver_unregister(&amprime_sl_driver);
}
