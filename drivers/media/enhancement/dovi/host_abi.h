/* Host declarations and layouts consumed by the Dolby Vision C core. */
#ifndef DOVI_HOST_ABI_H
#define DOVI_HOST_ABI_H

#include <linux/stddef.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "amdolby_vision.h"
#include "core/host_control.h"
#include "core/control_setting_layout.h"

#define DOVI_HOST_OFFSET(type, member, expected) \
    _Static_assert(offsetof(type, member) == (expected), #type "." #member " offset")

_Static_assert(sizeof(void *) == 8, "host requires 64-bit pointers");
_Static_assert(FORMAT_INVALID == -1 && FORMAT_DOVI == 0 && FORMAT_HDR10 == 1 && FORMAT_SDR == 2 &&
                   FORMAT_DOVI_LL == 3 && FORMAT_SDR_2020 == 6,
               "STB 2.4 host signal formats");
_Static_assert(sizeof(struct dolby_vision_func_s) == 0x30, "STB 2.4 callback table size");
DOVI_HOST_OFFSET(struct dolby_vision_func_s, version_info, 0x00);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_init, 0x08);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_reset, 0x10);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_process, 0x18);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, metadata_parser_release, 0x20);
DOVI_HOST_OFFSET(struct dolby_vision_func_s, control_path, 0x28);
_Static_assert(sizeof(struct dovi_setting_s) == 13480, "STB 2.4 host output size");
DOVI_HOST_OFFSET(struct dovi_setting_s, comp_reg, CONTROL_OUTPUT_COMP_REG_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dm_reg1, CONTROL_OUTPUT_DM_REG1_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dm_reg2, CONTROL_OUTPUT_DM_REG2_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dm_reg3, CONTROL_OUTPUT_DM_REG3_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dm_lut1, CONTROL_OUTPUT_DM_LUT1_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dm_lut2, CONTROL_OUTPUT_DM_LUT2_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, md_reg3, CONTROL_OUTPUT_MD_REG3_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, hdr_info, CONTROL_OUTPUT_HDR_INFO_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, src_format, CONTROL_OUTPUT_SRC_FORMAT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dst_format, CONTROL_OUTPUT_DST_FORMAT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, el_flag, CONTROL_OUTPUT_EL_FLAG_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, el_halfsize_flag, CONTROL_OUTPUT_EL_HALFSIZE_FLAG_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, video_width, CONTROL_OUTPUT_VIDEO_WIDTH_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, video_height, CONTROL_OUTPUT_VIDEO_HEIGHT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, g_format, CONTROL_OUTPUT_G_FORMAT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, g_bitdepth, CONTROL_OUTPUT_G_BITDEPTH_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dovi2hdr10_nomapping,
                 CONTROL_OUTPUT_DOVI2HDR10_NOMAPPING_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, use_ll_flag, CONTROL_OUTPUT_USE_LL_FLAG_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, ll_rgb_desired, CONTROL_OUTPUT_LL_RGB_DESIRED_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, diagnostic_enable, CONTROL_OUTPUT_DIAGNOSTIC_ENABLE_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, diagnostic_mux_select,
                 CONTROL_OUTPUT_DIAGNOSTIC_MUX_SELECT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, dovi_ll_enable, CONTROL_OUTPUT_DOVI_LL_ENABLE_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, vout_width, CONTROL_OUTPUT_VOUT_WIDTH_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, vout_height, CONTROL_OUTPUT_VOUT_HEIGHT_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, vsvdb_tbl, CONTROL_OUTPUT_VSVDB_TBL_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, ext_md, CONTROL_OUTPUT_EXT_MD_OFFSET);
DOVI_HOST_OFFSET(struct dovi_setting_s, vsvdb_len, CONTROL_OUTPUT_BYTES);
DOVI_HOST_OFFSET(struct dovi_setting_s, vsvdb_changed, CONTROL_OUTPUT_BYTES + 4);
DOVI_HOST_OFFSET(struct dovi_setting_s, mode_changed, CONTROL_OUTPUT_BYTES + 8);

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

#undef DOVI_HOST_OFFSET

/* The 4.9 host uses single-instance callback signatures. */
void *dovi_host_parser_init(int flags);
int dovi_host_parser_reset(int flags);
int dovi_host_parser_process(char *rpu, int rpu_size, char *composer, int *composer_size,
                             char *metadata, int *metadata_size, bool end_of_stream);
int dovi_host_control_path(enum signal_format_enum input_format,
                           enum signal_format_enum output_format, char *composition,
                           int composition_size, char *metadata, int metadata_size,
                           enum priority_mode_enum priority, int bit_depth, int chroma_format,
                           int yuv_range, int graphics_min, int graphics_max, int target_min,
                           int target_max, int no_el, struct hdr10_parameter *hdr10,
                           struct dovi_setting_s *output);
int dovi_register_callbacks(void);

#endif
