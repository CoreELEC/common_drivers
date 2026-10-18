/* Host declarations and layouts consumed by the Dolby Vision C core. */
#ifndef DOVI_HOST_ABI_H
#define DOVI_HOST_ABI_H

#include <linux/kernel.h>
#include <linux/stddef.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "amdv.h"
#include "core/control_setting_layout.h"

#define DOVI_HOST_OFFSET(type, member, expected) \
    _Static_assert(offsetof(type, member) == (expected), #type "." #member " offset")

_Static_assert(sizeof(void *) == 8, "host requires 64-bit pointers");
_Static_assert(sizeof(enum dv_type_enum) == 4, "STB 2.6 parser input kind width");
_Static_assert(DV_TYPE_DOVI == 0 && DV_TYPE_ATSC == 1 && DV_TYPE_DVB == 2,
               "STB 2.6 parser input kinds");
_Static_assert(FORMAT_INVALID == -1 && FORMAT_DOVI == 0 && FORMAT_HDR10 == 1 && FORMAT_SDR == 2 &&
                   FORMAT_DOVI_LL == 3 && FORMAT_HLG == 4 && FORMAT_SDR_2020 == 6,
               "STB 2.6 signal formats");

/* The host reads the complete table, including unsupported TV/HW5 slots. */
_Static_assert(sizeof(struct dolby_vision_func_s) == 0x70, "STB 2.6 callback table size");
DOVI_HOST_OFFSET(struct dolby_vision_func_s, version_info, 0x00);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_init, 0x08);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_reset, 0x10);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_process, 0x18);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_release, 0x20);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, control_path, 0x28);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, tv_control_path, 0x30);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, multi_control_path, 0x38);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, multi_mp_init, 0x40);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, multi_mp_reset, 0x48);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, multi_mp_process, 0x50);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, multi_mp_release, 0x58);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, tv_hw5_control_path, 0x60);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, tv_hw5_control_path_analyzer, 0x68);

/* The core accesses these host-owned buffers using the STB 2.6 binary layout. */
_Static_assert(sizeof(struct private_info_s) == DOVI_SETTING_INPUT_STRIDE, "STB 2.6 input stride");
DOVI_HOST_OFFSET(struct private_info_s, valid, DOVI_INPUT_VALID);
DOVI_HOST_OFFSET(struct private_info_s, src_format, DOVI_INPUT_SRC_FORMAT);
DOVI_HOST_OFFSET(struct private_info_s, el_flag, DOVI_INPUT_EL_FLAG);
DOVI_HOST_OFFSET(struct private_info_s, el_halfsize_flag, DOVI_INPUT_EL_HALFSIZE_FLAG);
DOVI_HOST_OFFSET(struct private_info_s, video_width, DOVI_INPUT_VIDEO_WIDTH);
DOVI_HOST_OFFSET(struct private_info_s, video_height, DOVI_INPUT_VIDEO_HEIGHT);
DOVI_HOST_OFFSET(struct private_info_s, set_bit_depth, DOVI_INPUT_SET_BIT_DEPTH);
DOVI_HOST_OFFSET(struct private_info_s, set_chroma_format, DOVI_INPUT_SET_CHROMA_FORMAT);
DOVI_HOST_OFFSET(struct private_info_s, set_yuv_range, DOVI_INPUT_SET_YUV_RANGE);
DOVI_HOST_OFFSET(struct private_info_s, color_format, DOVI_INPUT_COLOR_FORMAT);
DOVI_HOST_OFFSET(struct private_info_s, in_comp, DOVI_INPUT_IN_COMP);
DOVI_HOST_OFFSET(struct private_info_s, in_comp_size, DOVI_INPUT_IN_COMP_SIZE);
DOVI_HOST_OFFSET(struct private_info_s, in_md, DOVI_INPUT_IN_MD);
DOVI_HOST_OFFSET(struct private_info_s, in_md_size, DOVI_INPUT_IN_MD_SIZE);
DOVI_HOST_OFFSET(struct private_info_s, vsem_if, DOVI_INPUT_VSEM_IF);
DOVI_HOST_OFFSET(struct private_info_s, vsem_if_size, DOVI_INPUT_VSEM_IF_SIZE);
DOVI_HOST_OFFSET(struct private_info_s, input_mode, DOVI_INPUT_INPUT_MODE);
DOVI_HOST_OFFSET(struct private_info_s, use_primaries_for_dv, DOVI_INPUT_USE_PRIMARIES_FOR_DV);
DOVI_HOST_OFFSET(struct private_info_s, p_hdr10_param, DOVI_INPUT_P_HDR10_PARAM);

_Static_assert(sizeof(struct m_dovi_setting_s) == DOVI_SETTING_BYTES,
               "STB 2.6 control buffer size");
DOVI_HOST_OFFSET(struct m_dovi_setting_s, num_input, DOVI_SETTING_NUM_INPUT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, num_video, DOVI_SETTING_NUM_VIDEO);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, pri_input, DOVI_SETTING_PRI_INPUT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, enable_multi_core1, DOVI_SETTING_ENABLE_MULTI_CORE1);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, enable_debug, DOVI_SETTING_ENABLE_DEBUG);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, set_priority, DOVI_SETTING_SET_PRIORITY);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dovi2hdr10_nomapping, DOVI_SETTING_DOVI2HDR10_NOMAPPING);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, use_ll_flag, DOVI_SETTING_USE_LL_FLAG);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, ll_rgb_desired, DOVI_SETTING_LL_RGB_DESIRED);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, vout_width, DOVI_SETTING_VOUT_WIDTH);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, vout_height, DOVI_SETTING_VOUT_HEIGHT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, vsvdb_tbl, DOVI_SETTING_VSVDB_TBL);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, vsvdb_len, DOVI_SETTING_VSVDB_LEN);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, vsvdb_changed, DOVI_SETTING_VSVDB_CHANGED);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, mode_changed, DOVI_SETTING_MODE_CHANGED);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dst_format, DOVI_SETTING_DST_FORMAT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, input, DOVI_SETTING_INPUT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, set_graphic_min_lum, DOVI_SETTING_SET_GRAPHIC_MIN_LUM);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, set_graphic_max_lum, DOVI_SETTING_SET_GRAPHIC_MAX_LUM);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, set_target_min_lum, DOVI_SETTING_SET_TARGET_MIN_LUM);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, set_target_max_lum, DOVI_SETTING_SET_TARGET_MAX_LUM);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, core1, DOVI_SETTING_CORE1);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dm_reg2, DOVI_SETTING_DM_REG2);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dm_reg3, DOVI_SETTING_DM_REG3);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dm_lut2, DOVI_SETTING_DM_LUT2);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, md_reg3, DOVI_SETTING_MD_REG3);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, hdr_info, DOVI_SETTING_HDR_INFO);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, diagnostic_enable, DOVI_SETTING_DIAGNOSTIC_ENABLE);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, diagnostic_mux_select,
                 DOVI_SETTING_DIAGNOSTIC_MUX_SELECT);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, dovi_ll_enable, DOVI_SETTING_DOVI_LL_ENABLE);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, ext_md, DOVI_SETTING_EXT_MD);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, output_vsif, DOVI_SETTING_OUTPUT_VSIF);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, output_ctrl_data, DOVI_SETTING_OUTPUT_CTRL_DATA);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, output_ctrl_data_len, DOVI_SETTING_OUTPUT_CTRL_DATA_LEN);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, ctrl_data_type, DOVI_SETTING_CTRL_DATA_TYPE);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, content_info, DOVI_SETTING_CONTENT_INFO);
DOVI_HOST_OFFSET(struct m_dovi_setting_s, reserved, DOVI_SETTING_RESERVED);

/* Nested payloads and register banks accessed by the control core. */
_Static_assert(sizeof(struct hdr10_parameter) == 0x1c, "hdr10_parameter size");
DOVI_HOST_OFFSET(struct hdr10_parameter, min_display_mastering_lum, 0x0);
DOVI_HOST_OFFSET(struct hdr10_parameter, max_display_mastering_lum, 0x4);
DOVI_HOST_OFFSET(struct hdr10_parameter, r_x, 0x8);
DOVI_HOST_OFFSET(struct hdr10_parameter, r_y, 0xa);
DOVI_HOST_OFFSET(struct hdr10_parameter, g_x, 0xc);
DOVI_HOST_OFFSET(struct hdr10_parameter, g_y, 0xe);
DOVI_HOST_OFFSET(struct hdr10_parameter, b_x, 0x10);
DOVI_HOST_OFFSET(struct hdr10_parameter, b_y, 0x12);
DOVI_HOST_OFFSET(struct hdr10_parameter, w_x, 0x14);
DOVI_HOST_OFFSET(struct hdr10_parameter, w_y, 0x16);
DOVI_HOST_OFFSET(struct hdr10_parameter, max_content_light_level, 0x18);
DOVI_HOST_OFFSET(struct hdr10_parameter, max_frame_avg_light_level, 0x1a);

_Static_assert(sizeof(struct hdr10_infoframe) == 0x1d, "hdr10_infoframe size");
DOVI_HOST_OFFSET(struct hdr10_infoframe, type_code, 0x0);
DOVI_HOST_OFFSET(struct hdr10_infoframe, version_number, 0x1);
DOVI_HOST_OFFSET(struct hdr10_infoframe, len_of_info_frame, 0x2);
DOVI_HOST_OFFSET(struct hdr10_infoframe, data_byte1, 0x3);
DOVI_HOST_OFFSET(struct hdr10_infoframe, data_byte2, 0x4);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_0_lsb, 0x5);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_0_msb, 0x6);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_0_lsb, 0x7);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_0_msb, 0x8);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_1_lsb, 0x9);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_1_msb, 0xa);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_1_lsb, 0xb);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_1_msb, 0xc);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_2_lsb, 0xd);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_x_2_msb, 0xe);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_2_lsb, 0xf);
DOVI_HOST_OFFSET(struct hdr10_infoframe, primaries_y_2_msb, 0x10);
DOVI_HOST_OFFSET(struct hdr10_infoframe, white_point_x_lsb, 0x11);
DOVI_HOST_OFFSET(struct hdr10_infoframe, white_point_x_msb, 0x12);
DOVI_HOST_OFFSET(struct hdr10_infoframe, white_point_y_lsb, 0x13);
DOVI_HOST_OFFSET(struct hdr10_infoframe, white_point_y_msb, 0x14);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_display_mastering_lum_lsb, 0x15);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_display_mastering_lum_msb, 0x16);
DOVI_HOST_OFFSET(struct hdr10_infoframe, min_display_mastering_lum_lsb, 0x17);
DOVI_HOST_OFFSET(struct hdr10_infoframe, min_display_mastering_lum_msb, 0x18);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_content_light_level_lsb, 0x19);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_content_light_level_msb, 0x1a);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_frame_avg_light_level_lsb, 0x1b);
DOVI_HOST_OFFSET(struct hdr10_infoframe, max_frame_avg_light_level_msb, 0x1c);

_Static_assert(sizeof(struct ext_level_1) == 0x6, "ext_level_1 size");
DOVI_HOST_OFFSET(struct ext_level_1, min_pq_h, 0x0);
DOVI_HOST_OFFSET(struct ext_level_1, min_pq_l, 0x1);
DOVI_HOST_OFFSET(struct ext_level_1, max_pq_h, 0x2);
DOVI_HOST_OFFSET(struct ext_level_1, max_pq_l, 0x3);
DOVI_HOST_OFFSET(struct ext_level_1, avg_pq_h, 0x4);
DOVI_HOST_OFFSET(struct ext_level_1, avg_pq_l, 0x5);

_Static_assert(sizeof(struct ext_level_2) == 0xe, "ext_level_2 size");
DOVI_HOST_OFFSET(struct ext_level_2, target_max_pq_h, 0x0);
DOVI_HOST_OFFSET(struct ext_level_2, target_max_pq_l, 0x1);
DOVI_HOST_OFFSET(struct ext_level_2, trim_slope_h, 0x2);
DOVI_HOST_OFFSET(struct ext_level_2, trim_slope_l, 0x3);
DOVI_HOST_OFFSET(struct ext_level_2, trim_off_h, 0x4);
DOVI_HOST_OFFSET(struct ext_level_2, trim_off_l, 0x5);
DOVI_HOST_OFFSET(struct ext_level_2, trim_power_h, 0x6);
DOVI_HOST_OFFSET(struct ext_level_2, trim_power_l, 0x7);
DOVI_HOST_OFFSET(struct ext_level_2, trim_chroma_weight_h, 0x8);
DOVI_HOST_OFFSET(struct ext_level_2, trim_chroma_weight_l, 0x9);
DOVI_HOST_OFFSET(struct ext_level_2, trim_sat_gain_h, 0xa);
DOVI_HOST_OFFSET(struct ext_level_2, trim_sat_gain_l, 0xb);
DOVI_HOST_OFFSET(struct ext_level_2, ms_weight_h, 0xc);
DOVI_HOST_OFFSET(struct ext_level_2, ms_weight_l, 0xd);

_Static_assert(sizeof(struct ext_level_4) == 0x4, "ext_level_4 size");
DOVI_HOST_OFFSET(struct ext_level_4, anchor_pq_h, 0x0);
DOVI_HOST_OFFSET(struct ext_level_4, anchor_pq_l, 0x1);
DOVI_HOST_OFFSET(struct ext_level_4, anchor_power_h, 0x2);
DOVI_HOST_OFFSET(struct ext_level_4, anchor_power_l, 0x3);

_Static_assert(sizeof(struct ext_level_5) == 0x8, "ext_level_5 size");
DOVI_HOST_OFFSET(struct ext_level_5, active_area_left_off_h, 0x0);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_left_off_l, 0x1);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_right_off_h, 0x2);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_right_off_l, 0x3);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_top_off_h, 0x4);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_top_off_l, 0x5);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_bot_off_h, 0x6);
DOVI_HOST_OFFSET(struct ext_level_5, active_area_bot_off_l, 0x7);

_Static_assert(sizeof(struct ext_level_6) == 0x8, "ext_level_6 size");
DOVI_HOST_OFFSET(struct ext_level_6, max_display_mastering_lum_h, 0x0);
DOVI_HOST_OFFSET(struct ext_level_6, max_display_mastering_lum_l, 0x1);
DOVI_HOST_OFFSET(struct ext_level_6, min_display_mastering_lum_h, 0x2);
DOVI_HOST_OFFSET(struct ext_level_6, min_display_mastering_lum_l, 0x3);
DOVI_HOST_OFFSET(struct ext_level_6, max_content_light_level_h, 0x4);
DOVI_HOST_OFFSET(struct ext_level_6, max_content_light_level_l, 0x5);
DOVI_HOST_OFFSET(struct ext_level_6, max_frame_avg_light_level_h, 0x6);
DOVI_HOST_OFFSET(struct ext_level_6, max_frame_avg_light_level_l, 0x7);

_Static_assert(sizeof(struct ext_level_255) == 0x6, "ext_level_255 size");
DOVI_HOST_OFFSET(struct ext_level_255, run_mode, 0x0);
DOVI_HOST_OFFSET(struct ext_level_255, run_version, 0x1);
DOVI_HOST_OFFSET(struct ext_level_255, dm_debug_0, 0x2);
DOVI_HOST_OFFSET(struct ext_level_255, dm_debug_1, 0x3);
DOVI_HOST_OFFSET(struct ext_level_255, dm_debug_2, 0x4);
DOVI_HOST_OFFSET(struct ext_level_255, dm_debug_3, 0x5);

_Static_assert(sizeof(struct ext_md_s) == 0x34, "ext_md_s size");
DOVI_HOST_OFFSET(struct ext_md_s, avail_level_mask, 0x0);
DOVI_HOST_OFFSET(struct ext_md_s, level_1, 0x4);
DOVI_HOST_OFFSET(struct ext_md_s, level_2, 0xa);
DOVI_HOST_OFFSET(struct ext_md_s, level_4, 0x18);
DOVI_HOST_OFFSET(struct ext_md_s, level_5, 0x1c);
DOVI_HOST_OFFSET(struct ext_md_s, level_6, 0x24);
DOVI_HOST_OFFSET(struct ext_md_s, level_255, 0x2c);

_Static_assert(sizeof(struct composer_reg_ipcore) == 0x2b4, "composer_reg_ipcore size");
DOVI_HOST_OFFSET(struct composer_reg_ipcore, composer_mode, 0x0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, vdr_resolution, 0x4);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bit_depth, 0x8);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, coefficient_log2_denominator, 0xc);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_num_pivots_y, 0x10);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_pivot, 0x14);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_order, 0x28);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_coefficient_y, 0x2c);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_nlq_offset_y, 0x8c);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_coefficient_y, 0x90);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mapping_idc_u, 0x9c);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_num_pivots_u, 0xa0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_pivot_u, 0xa4);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_order_u, 0xb0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_coefficient_u, 0xb4);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mmr_coefficient_u, 0xe4);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mmr_order_u, 0x194);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_nlq_offset_u, 0x198);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_coefficient_u, 0x19c);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mapping_idc_v, 0x1a8);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_num_pivots_v, 0x1ac);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_pivot_v, 0x1b0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_order_v, 0x1bc);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, bl_coefficient_v, 0x1c0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mmr_coefficient_v, 0x1f0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, mmr_order_v, 0x2a0);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_nlq_off_v, 0x2a4);
DOVI_HOST_OFFSET(struct composer_reg_ipcore, el_coefficient_v, 0x2a8);

_Static_assert(sizeof(struct dm_reg_ipcore1) == 0x6c, "dm_reg_ipcore1 size");
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, s_range, 0x0);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, s_range_inverse, 0x4);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, frame_fmt1, 0x8);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, frame_fmt2, 0xc);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, frame_pixel_def, 0x10);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_coeff1, 0x14);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_coeff2, 0x18);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_coeff3, 0x1c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_coeff4, 0x20);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_coeff5, 0x24);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_off1, 0x28);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_off2, 0x2c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, y2rgb_off3, 0x30);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, eotf, 0x34);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, a2b_coeff1, 0x38);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, a2b_coeff2, 0x3c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, a2b_coeff3, 0x40);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, a2b_coeff4, 0x44);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, a2b_coeff5, 0x48);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_coeff1, 0x4c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_coeff2, 0x50);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_coeff3, 0x54);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_coeff4, 0x58);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_coeff5, 0x5c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, c2d_off, 0x60);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, active_left_top, 0x64);
DOVI_HOST_OFFSET(struct dm_reg_ipcore1, active_bottom_right, 0x68);

_Static_assert(sizeof(struct dm_reg_ipcore2) == 0x60, "dm_reg_ipcore2 size");
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, s_range, 0x0);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, s_range_inverse, 0x4);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_coeff1, 0x8);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_coeff2, 0xc);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_coeff3, 0x10);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_coeff4, 0x14);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_coeff5, 0x18);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_off1, 0x1c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_off2, 0x20);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, y2rgb_off3, 0x24);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, frame_fmt, 0x28);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, eotf, 0x2c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, a2b_coeff1, 0x30);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, a2b_coeff2, 0x34);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, a2b_coeff3, 0x38);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, a2b_coeff4, 0x3c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, a2b_coeff5, 0x40);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_coeff1, 0x44);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_coeff2, 0x48);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_coeff3, 0x4c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_coeff4, 0x50);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_coeff5, 0x54);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, c2d_off, 0x58);
DOVI_HOST_OFFSET(struct dm_reg_ipcore2, vdr_res, 0x5c);

_Static_assert(sizeof(struct dm_reg_ipcore3) == 0x68, "dm_reg_ipcore3 size");
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, d2c_coeff1, 0x0);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, d2c_coeff2, 0x4);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, d2c_coeff3, 0x8);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, d2c_coeff4, 0xc);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, d2c_coeff5, 0x10);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, b2a_coeff1, 0x14);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, b2a_coeff2, 0x18);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, b2a_coeff3, 0x1c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, b2a_coeff4, 0x20);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, b2a_coeff5, 0x24);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, eotf_param1, 0x28);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, eotf_param2, 0x2c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, ipt_scale, 0x30);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, ipt_off1, 0x34);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, ipt_off2, 0x38);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, ipt_off3, 0x3c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, output_range1, 0x40);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, output_range2, 0x44);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_coeff_reg1, 0x48);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_coeff_reg2, 0x4c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_coeff_reg3, 0x50);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_coeff_reg4, 0x54);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_coeff_reg5, 0x58);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_off0, 0x5c);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_off1, 0x60);
DOVI_HOST_OFFSET(struct dm_reg_ipcore3, rgb2yuv_off2, 0x64);

_Static_assert(sizeof(struct dm_lut_ipcore) == 0x1400, "dm_lut_ipcore size");
DOVI_HOST_OFFSET(struct dm_lut_ipcore, tm_lut_i, 0x0);
DOVI_HOST_OFFSET(struct dm_lut_ipcore, tm_lut_s, 0x400);
DOVI_HOST_OFFSET(struct dm_lut_ipcore, sm_lut_i, 0x800);
DOVI_HOST_OFFSET(struct dm_lut_ipcore, sm_lut_s, 0xc00);
DOVI_HOST_OFFSET(struct dm_lut_ipcore, g_2_l, 0x1000);

_Static_assert(sizeof(struct md_reg_ipcore3) == 0x804, "md_reg_ipcore3 size");
DOVI_HOST_OFFSET(struct md_reg_ipcore3, raw_metadata, 0x0);
DOVI_HOST_OFFSET(struct md_reg_ipcore3, size, 0x800);

_Static_assert(sizeof(struct dovi_setting_video_s) == 0x1720, "dovi_setting_video_s size");
DOVI_HOST_OFFSET(struct dovi_setting_video_s, comp_reg, 0x0);
DOVI_HOST_OFFSET(struct dovi_setting_video_s, dm_reg, 0x2b4);
DOVI_HOST_OFFSET(struct dovi_setting_video_s, dm_lut, 0x320);

#undef DOVI_HOST_OFFSET

int dovi_host_control_path(struct m_dovi_setting_s *setting);
void *dovi_host_parser_init(int flags);
int dovi_host_parser_reset(void *context, int flags);
int dovi_host_parser_process(void *context, char *rpu, int rpu_size, char *composer,
                             int *composer_size, char *metadata, int *metadata_size,
                             bool end_of_stream, enum dv_type_enum input_kind);
int dovi_register_callbacks(const char *version_info);

#endif
