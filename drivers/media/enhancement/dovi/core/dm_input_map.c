/* Input color-space and transfer maps. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

_Bool dovi_dm_CanCclrBypassCsc(int color_space, int range, int bypass)
{
    return (color_space == 4 && range == 1) && bypass != 0;
}

void dovi_dm_CommitMds2KsIMap(int signal_index, unsigned char *metadata, long runtime_signal,
                              long csc_control, unsigned char *input_map)
{
    _Alignas(16) struct dm_metadata_input_workspace scratch;
    unsigned char input_color;
    unsigned int power_value;
    unsigned char metadata_color;
    unsigned short gamma;
    unsigned short gamma_gain;
    unsigned short gamma_offset;
    unsigned short matrix_scale;
    int color_scale_or_range;
    unsigned long range_inverse_or_gamma_input;
    unsigned int chroma_offset;
    unsigned int luma_offset_or_gamma_value;
    unsigned int gamma_log_value;
    unsigned long gamma_log_index;
    long gamma_byte_offset;
    unsigned char nonzero_input_color;
    unsigned short gamma_sample;
    unsigned int ipt_variant_or_gamma_range;

    scratch.rgb_matrix.packed.last_coefficient = 0;
    scratch.rgb_matrix.packed.words[0] = 0;
    scratch.rgb_matrix.packed.words[1] = 0;
    scratch.rgb_matrix.packed.words[2] = 0;
    scratch.rgb_matrix.packed.words[3] = 0;
    if ((metadata == (unsigned char *)0x0) || ((*metadata & 1) == 0))
        goto return_input_map;
    metadata_color = metadata[0x49];
    ipt_variant_or_gamma_range = 0;
    nonzero_input_color = 3;
    if (metadata_color == 1) {
        nonzero_input_color = 1;
    }
    input_color = 0;
    if (metadata_color != 0) {
        input_color = nonzero_input_color;
    }
    *input_map = input_color;
    if (metadata[0x49] == 2) {
        ipt_variant_or_gamma_range = (short)fixed_signed16(fixed_load_le16(metadata + 0x2a)) * 3 +
                 (short)fixed_signed16(fixed_load_le16(metadata + 0x2c)) * 0x61;
        if ((((int)ipt_variant_or_gamma_range < 0) && (fixed_signed32(fixed_load_le32(metadata + 0x1c)) == 0)) &&
            (fixed_signed32(fixed_load_le32(metadata + 0x20)) !=
             fixed_signed32(fixed_load_le32(metadata + 0x24)))) {
            ipt_variant_or_gamma_range = 1;
            goto select_metadata_color_matrix;
        }
        ipt_variant_or_gamma_range = ipt_variant_or_gamma_range >> 0x1f;
        *input_map = 0;
    copy_metadata_matrix:
        fixed_store_le16(input_map + 2, (unsigned short)fixed_load_le16(metadata + 6));
        fixed_store_le16(input_map + 4, (unsigned short)fixed_load_le16(metadata + 8));
        fixed_store_le16(input_map + 6, (unsigned short)fixed_load_le16(metadata + 10));
        fixed_store_le16(input_map + 8, (unsigned short)fixed_load_le16(metadata + 0xc));
        fixed_store_le16(input_map + 10, (unsigned short)fixed_load_le16(metadata + 0xe));
        fixed_store_le16(input_map + 0xc, (unsigned short)fixed_load_le16(metadata + 0x10));
        fixed_store_le16(input_map + 0xe, (unsigned short)fixed_load_le16(metadata + 0x12));
        fixed_store_le16(input_map + 0x10, (unsigned short)fixed_load_le16(metadata + 0x14));
        fixed_store_le16(input_map + 0x12, (unsigned short)fixed_load_le16(metadata + 0x16));
        fixed_store_le16(input_map + 0x14, (unsigned short)fixed_load_le16(metadata + 0x18));
        luma_offset_or_gamma_value = fixed_load_le32(metadata + 0x1c);
        chroma_offset = fixed_load_le32(metadata + 0x20);
    commit_color_and_transfer:
        dovi_Yuv2RgbOffsetRgbFxp((const void *)(input_map + 4), (unsigned int)(luma_offset_or_gamma_value),
                            (unsigned int)(chroma_offset), (void *)(input_map + 0x18));
        scratch.rgb_matrix.packed.words[0] = fixed_load_le32(metadata + 0x2a);
        scratch.rgb_matrix.packed.words[3] = fixed_load_le32(metadata + 0x36);
        scratch.rgb_matrix.packed.last_coefficient =
            (unsigned short)fixed_load_le16(metadata + 0x3a);
        scratch.rgb_matrix.packed.words[1] =
            (unsigned int)(unsigned long)fixed_load_le64(metadata + 0x2e);
        scratch.rgb_matrix.packed.words[2] =
            (unsigned int)((unsigned long)(unsigned long)fixed_load_le64(metadata + 0x2e) >> 0x20);
        if (metadata[0x49] == 2) {
            color_scale_or_range = 0x28f;
            if (ipt_variant_or_gamma_range != 0) {
                color_scale_or_range = 0x51f;
            }
            color_scale_or_range =
                dovi_dm_CommitIptRgb2LmsM33((int)(signal_index), (int)(color_scale_or_range), (long)(csc_control),
                                            (short *)((short *)(input_map + 0x432)));
            matrix_scale = (unsigned short)color_scale_or_range;
        } else {
            matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp(
                (const void *)(scratch.rgb_matrix.coefficients),
                (unsigned int)((unsigned short)fixed_load_le16(metadata + 0x28)),
                (unsigned int)(*(unsigned int *)(csc_control + 0x38)),
                (const void *)(csc_control + (long)signal_index * 0x28 + 0x80),
                (unsigned int)(*(unsigned int *)(csc_control + (long)signal_index * 0x28 + 0x8c)),
                (void *)(input_map + 0x432));
        }
        fixed_store_le16(input_map + 0x430, matrix_scale);
        color_scale_or_range = (unsigned int)(metadata[0x4b] != 0) << 1;
        if (metadata[0x4b] == 1) {
            color_scale_or_range = 1;
        }
        dovi_GetRange((unsigned int)(metadata[0x48]), (unsigned int)(color_scale_or_range), (void *)(input_map + 0x24),
                 (void *)(input_map + 0x26));
        range_inverse_or_gamma_input = dovi_sdiv((unsigned long long)(0xffffffff),
                      (unsigned int)((unsigned int)(unsigned short)fixed_load_le16(input_map + 0x26)
                                     << (unsigned long)(0x10 - metadata[0x48] & 0x1f)));
        fixed_store_le32(input_map + 0x28, (int)range_inverse_or_gamma_input);
        fixed_store_le16(input_map + 0x2c, (unsigned short)metadata[0x48]);
        gamma = (unsigned short)fixed_load_le16(metadata + 0x3c);
        if (gamma == 0xfffe) {
            input_map[0x2e] = 0;
            dovi_DeriveHlgEotfParam((void *)(input_map),
                               (unsigned int)(*(unsigned int *)(runtime_signal + 8)),
                               (unsigned int)(*(unsigned int *)(runtime_signal + 0xc)));
        } else if (gamma == 0xffff) {
            input_map[0x2e] = 1;
        } else if ((unsigned short)fixed_load_le16(metadata + 0x4e) - 0xe6f < 3) {
            if ((unsigned short)fixed_load_le16(metadata + 0x4c) - 0x3c < 3) {
                gamma_byte_offset = 0;
                do {
                    fixed_store_le32(input_map + gamma_byte_offset + 0x30,
                                     dovi_pulsar_gamma_to_linear[gamma_byte_offset / 4]);
                    gamma_byte_offset = gamma_byte_offset + 4;
                } while (gamma_byte_offset != 0x400);
            } else {
            generate_gamma_lut:
                gamma_byte_offset = 0;
                gamma_sample = 0;
                gamma_gain = (unsigned short)fixed_load_le16(metadata + 0x3e);
                gamma_offset = (unsigned short)fixed_load_le16(metadata + 0x40);
                ipt_variant_or_gamma_range = fixed_load_le32(metadata + 0x44);
                do {
                    range_inverse_or_gamma_input = (unsigned long)gamma_offset + (unsigned long)gamma_sample;
                    gamma_log_index = range_inverse_or_gamma_input >> (range_inverse_or_gamma_input >> 0x10);
                    gamma_log_value = (unsigned int)gamma_log_index;
                    if ((0x1000f < gamma_log_value) || (gamma_log_index = gamma_log_index >> 4, (int)gamma_log_index == 0x1000)) {
                        dm_limit_trap();
                    }
                    gamma_log_value = ((unsigned int)dovi_log10_value[gamma_log_index] -
                              (dovi_log10_rate[gamma_log_index] * (gamma_log_value & 0xf) >> 8)) *
                             (unsigned int)gamma;
                    if (gamma_log_value >> 0x1b < 5) {
                        power_value = (unsigned int)((unsigned long)
                                                       dovi_power10_value[gamma_log_value >> 0x1a]
                                                                         [gamma_log_value >> 0x10 & 0x3ff] *
                                                   (unsigned long)ipt_variant_or_gamma_range >>
                                               0x12);
                        if ((range_inverse_or_gamma_input & 0xffff0000) == 0) {
                            power_value = dovi_power10_value[gamma_log_value >> 0x1a][gamma_log_value >> 0x10 & 0x3ff];
                        }
                        luma_offset_or_gamma_value =
                            (unsigned int)((unsigned long)gamma_gain * (unsigned long)power_value >>
                                           ((unsigned long)(dovi_power10_scale[gamma_log_value >> 0x1a] -
                                                            0x10) &
                                            0x3f));
                    } else {
                        luma_offset_or_gamma_value = 0;
                    }
                    fixed_store_le32(input_map + gamma_byte_offset + 0x30, luma_offset_or_gamma_value);
                    gamma_sample = gamma_sample + 0x100;
                    gamma_byte_offset = gamma_byte_offset + 4;
                } while (gamma_byte_offset != 0x400);
            }
        } else {
            if ((2 < (unsigned short)fixed_load_le16(metadata + 0x4e) - 0x820) ||
                (2 < (unsigned short)fixed_load_le16(metadata + 0x4c) - 0x3c))
                goto generate_gamma_lut;
            gamma_byte_offset = 0;
            do {
                fixed_store_le32(input_map + gamma_byte_offset + 0x30, dovi_sdr_gamma_to_linear[gamma_byte_offset / 4]);
                gamma_byte_offset = gamma_byte_offset + 4;
            } while (gamma_byte_offset != 0x400);
        }
        dovi_AssignM33I2I((const void *)((void *)(csc_control + 0x3c)), (void *)(input_map + 0x446));
        fixed_store_le16(input_map + 0x444, (short)*(unsigned int *)(csc_control + 0x50));
    } else {
    select_metadata_color_matrix:
        if (metadata_color < 2) {
            if (metadata_color != 1)
                goto copy_metadata_matrix;
            luma_offset_or_gamma_value = 0;
            chroma_offset = 0;
            matrix_scale =
                (unsigned short)(1
                                 << (unsigned long)((unsigned short)fixed_load_le16(input_map + 2) &
                                                    0x1f));
            fixed_store_le16(input_map + 4, matrix_scale);
            fixed_store_le32(input_map + 6, 0);
            fixed_store_le16(input_map + 10, 0);
            fixed_store_le16(input_map + 0xc, matrix_scale);
            fixed_store_le32(input_map + 0xe, 0);
            fixed_store_le16(input_map + 0x12, 0);
            fixed_store_le16(input_map + 0x14, matrix_scale);
            goto commit_color_and_transfer;
        }
    }
    dovi_GetCvmBypassNormParam((unsigned int)(metadata[0x48]),
                          (unsigned int)(fixed_signed32(fixed_load_le32(metadata + 0x1c)) == 0),
                          (void *)(input_map + 0x45c), (void *)(input_map + 0x458));
return_input_map:

    return;
}

int dovi_dm_CommitIptRgb2LmsM33(int signal_index, int color_space, long config, short *matrix)
{
    int scaled_coefficient_11;
    int scaled_diagonal;
    int row0_reciprocal_or_coefficient_01;
    int scaled_coefficient_20;
    int scaled_coefficient_21;
    int second_diagonal_or_coefficient_10;
    unsigned int channel_scale;
    short original_coefficient_00;
    short original_coefficient_01;
    int diagonal_or_coefficient_02;
    unsigned int matrix_or_channel_scale;
    int row1_reciprocal_or_coefficient_12;
    int row2_reciprocal;
    unsigned long channel_reciprocal;
    unsigned long diagonal_reciprocal;
    long white_point_address;
    int matrix_scale;
    long scale_address;

    white_point_address = config + (long)signal_index * 0x28;
    if (*(unsigned int *)(config + 0x38) == color_space) {
        scale_address = config + (long)signal_index * 0x28;
        channel_reciprocal = dovi_sdiv(
            (unsigned long long)(1L
                                 << ((unsigned long)(unsigned int)(*(int *)(scale_address + 0x8c) << 1) &
                                     0x3f)),
            (unsigned int)(*(unsigned int *)(white_point_address + 0x80)));
        diagonal_reciprocal = dovi_sdiv(
            (unsigned long long)(1L
                                 << ((unsigned long)(unsigned int)(*(int *)(scale_address + 0x8c) << 1) &
                                     0x3f)),
            (unsigned int)(*(unsigned int *)(white_point_address + 0x84)));
        second_diagonal_or_coefficient_10 = (int)diagonal_reciprocal;
        diagonal_reciprocal = dovi_sdiv(
            (unsigned long long)(1L
                                 << ((unsigned long)(unsigned int)(*(int *)(scale_address + 0x8c) << 1) &
                                     0x3f)),
            (unsigned int)(*(unsigned int *)(white_point_address + 0x88)));
        matrix_scale = *(int *)(scale_address + 0x8c);
        diagonal_or_coefficient_02 = (int)channel_reciprocal;
        if (0x7ffe < (int)channel_reciprocal) {
            diagonal_or_coefficient_02 = 0x7fff;
        }
        matrix[1] = 0;
        matrix[2] = 0;
        matrix[3] = 0;
        if (diagonal_or_coefficient_02 < -0x7fff) {
            diagonal_or_coefficient_02 = -0x8000;
        }
        if (0x7ffe < second_diagonal_or_coefficient_10) {
            second_diagonal_or_coefficient_10 = 0x7fff;
        }
        matrix[5] = 0;
        matrix[6] = 0;
        matrix[7] = 0;
        *matrix = (short)diagonal_or_coefficient_02;
        if (second_diagonal_or_coefficient_10 < -0x7fff) {
            second_diagonal_or_coefficient_10 = -0x8000;
        }
        diagonal_or_coefficient_02 = (int)diagonal_reciprocal;
        if (0x7ffe < diagonal_or_coefficient_02) {
            diagonal_or_coefficient_02 = 0x7fff;
        }
        matrix[4] = (short)second_diagonal_or_coefficient_10;
        if (diagonal_or_coefficient_02 < -0x7fff) {
            diagonal_or_coefficient_02 = -0x8000;
        }
        matrix[8] = (short)diagonal_or_coefficient_02;
    } else {
        matrix_or_channel_scale = dovi_RE_CT2RIGHT_M33((void *)(matrix), (unsigned int)(color_space),
                                 (unsigned int)(*(unsigned int *)(config + 0x38)));
        matrix_scale = (int)(short)matrix_or_channel_scale;
        scale_address = config + (long)signal_index * 0x28;
        channel_reciprocal = dovi_sdiv(
            (unsigned long long)(1L
                                 << ((unsigned long)(unsigned int)(*(int *)(scale_address + 0x8c) << 1) &
                                     0x3f)),
            (unsigned int)(*(unsigned int *)(white_point_address + 0x80)));
        matrix_or_channel_scale = *(unsigned int *)(scale_address + 0x8c);
        row0_reciprocal_or_coefficient_01 = (int)channel_reciprocal;
        original_coefficient_00 = *matrix;
        original_coefficient_01 = matrix[1];
        diagonal_or_coefficient_02 = row0_reciprocal_or_coefficient_01 * matrix[2] >> (matrix_or_channel_scale & 0x1f);
        channel_reciprocal = dovi_sdiv((unsigned long long)(1L << ((unsigned long)(matrix_or_channel_scale << 1) & 0x3f)),
                      (unsigned int)(*(unsigned int *)(white_point_address + 0x84)));
        channel_scale = *(unsigned int *)(scale_address + 0x8c);
        row1_reciprocal_or_coefficient_12 = (int)channel_reciprocal;
        second_diagonal_or_coefficient_10 = row1_reciprocal_or_coefficient_12 * matrix[3] >> (channel_scale & 0x1f);
        scaled_coefficient_11 = row1_reciprocal_or_coefficient_12 * matrix[4] >> (channel_scale & 0x1f);
        row1_reciprocal_or_coefficient_12 = row1_reciprocal_or_coefficient_12 * matrix[5] >> (channel_scale & 0x1f);
        channel_reciprocal = dovi_sdiv((unsigned long long)(1L << ((unsigned long)(channel_scale << 1) & 0x3f)),
                      (unsigned int)(*(unsigned int *)(white_point_address + 0x88)));
        channel_scale = *(unsigned int *)(scale_address + 0x8c);
        scaled_diagonal = row0_reciprocal_or_coefficient_01 * original_coefficient_00 >> (matrix_or_channel_scale & 0x1f);
        row2_reciprocal = (int)channel_reciprocal;
        row0_reciprocal_or_coefficient_01 = row0_reciprocal_or_coefficient_01 * original_coefficient_01 >> (matrix_or_channel_scale & 0x1f);
        if (0x7ffe < scaled_diagonal) {
            scaled_diagonal = 0x7fff;
        }
        scaled_coefficient_20 = row2_reciprocal * matrix[6] >> (channel_scale & 0x1f);
        if (scaled_diagonal < -0x7fff) {
            scaled_diagonal = -0x8000;
        }
        if (0x7ffe < row0_reciprocal_or_coefficient_01) {
            row0_reciprocal_or_coefficient_01 = 0x7fff;
        }
        scaled_coefficient_21 = row2_reciprocal * matrix[7] >> (channel_scale & 0x1f);
        if (row0_reciprocal_or_coefficient_01 < -0x7fff) {
            row0_reciprocal_or_coefficient_01 = -0x8000;
        }
        if (0x7ffe < diagonal_or_coefficient_02) {
            diagonal_or_coefficient_02 = 0x7fff;
        }
        *matrix = (short)scaled_diagonal;
        scaled_diagonal = row2_reciprocal * matrix[8] >> (channel_scale & 0x1f);
        if (diagonal_or_coefficient_02 < -0x7fff) {
            diagonal_or_coefficient_02 = -0x8000;
        }
        if (0x7ffe < second_diagonal_or_coefficient_10) {
            second_diagonal_or_coefficient_10 = 0x7fff;
        }
        matrix[1] = (short)row0_reciprocal_or_coefficient_01;
        if (second_diagonal_or_coefficient_10 < -0x7fff) {
            second_diagonal_or_coefficient_10 = -0x8000;
        }
        if (0x7ffe < scaled_coefficient_11) {
            scaled_coefficient_11 = 0x7fff;
        }
        matrix[2] = (short)diagonal_or_coefficient_02;
        if (scaled_coefficient_11 < -0x7fff) {
            scaled_coefficient_11 = -0x8000;
        }
        if (0x7ffe < row1_reciprocal_or_coefficient_12) {
            row1_reciprocal_or_coefficient_12 = 0x7fff;
        }
        matrix[3] = (short)second_diagonal_or_coefficient_10;
        if (row1_reciprocal_or_coefficient_12 < -0x7fff) {
            row1_reciprocal_or_coefficient_12 = -0x8000;
        }
        if (0x7ffe < scaled_coefficient_20) {
            scaled_coefficient_20 = 0x7fff;
        }
        matrix[4] = (short)scaled_coefficient_11;
        if (scaled_coefficient_20 < -0x7fff) {
            scaled_coefficient_20 = -0x8000;
        }
        if (0x7ffe < scaled_coefficient_21) {
            scaled_coefficient_21 = 0x7fff;
        }
        matrix[5] = (short)row1_reciprocal_or_coefficient_12;
        if (scaled_coefficient_21 < -0x7fff) {
            scaled_coefficient_21 = -0x8000;
        }
        if (0x7ffe < scaled_diagonal) {
            scaled_diagonal = 0x7fff;
        }
        matrix[6] = (short)scaled_coefficient_20;
        if (scaled_diagonal < -0x7fff) {
            scaled_diagonal = -0x8000;
        }
        matrix[7] = (short)scaled_coefficient_21;
        matrix[8] = (short)scaled_diagonal;
    }
    return matrix_scale;
}

void dovi_dm_CommitIpt2KsIMap(int signal_index, long csc_control, int color_space,
                              unsigned int bit_depth, int range, int bypass,
                              unsigned char *input_map)
{
    _Alignas(16) struct dm_ipt_input_workspace scratch;
    unsigned short matrix_scale;
    unsigned int matrix_scale_result;
    int color_space_or_matrix_scale;
    unsigned long range_inverse;

    scratch.offsets.words[2] = 0;
    scratch.offsets.pairs[0] = 0;
    scratch.normalization = 0;
    *input_map = 0;
    if (color_space == 4) {
        if (range == 0) {
            matrix_scale_result = dovi_GetIctcpIo2LmsM33Narrow((unsigned int)(bit_depth), (void *)(input_map + 4));
            fixed_store_le16(input_map + 2, (short)matrix_scale_result);
        } else {
            matrix_scale_result = dovi_GetIctcpIo2LmsM33((void *)(input_map + 4));
            fixed_store_le16(input_map + 2, (short)matrix_scale_result);
        }
        if (bypass == 0) {
            color_space_or_matrix_scale = 0x51f;
        } else {
            color_space_or_matrix_scale = 0x51f;
            fixed_store_le16(input_map + 10, (short)fixed_signed16(fixed_load_le16(input_map + 10))
                                                 << 1);
            fixed_store_le16(input_map + 0xc,
                             (short)fixed_signed16(fixed_load_le16(input_map + 0xc)) << 1);
            fixed_store_le16(input_map + 0xe,
                             (short)fixed_signed16(fixed_load_le16(input_map + 0xe)) << 1);
        }
    } else {
        if (range == 0) {
            matrix_scale_result = dovi_GetIpt2LmsM33NarrowFxp((unsigned int)(bit_depth), (void *)(input_map + 4));
            matrix_scale = (unsigned short)matrix_scale_result;
        } else {
            matrix_scale_result = dovi_GetIpt2LmsM33Fxp((void *)(input_map + 4));
            matrix_scale = (unsigned short)matrix_scale_result;
        }
        color_space_or_matrix_scale = 0x28f;
        fixed_store_le16(input_map + 2, matrix_scale);
    }
    dovi_GetCvmBypassNormParam((unsigned int)(bit_depth), (unsigned int)(range),
                          (void *)(&scratch.offsets.pairs[0]), (void *)(&scratch.normalization));
    dovi_Yuv2RgbOffsetRgbFxp(
        (const void *)(input_map + 4), (unsigned int)(scratch.offsets.pairs[0] & 0xffffffff),
        (unsigned int)((*(unsigned int *)((unsigned char *)&scratch.offsets.pairs[0] + 4))),
        (void *)(input_map + 0x18));
    fixed_store_le16(input_map + 0x24, 0);
    range_inverse = dovi_sdiv((unsigned long long)(0xffffffff),
                 (unsigned int)(scratch.normalization << (unsigned long)(0x10 - bit_depth & 0x1f)));
    fixed_store_le32(input_map + 0x28, (int)range_inverse);
    input_map[0x2e] = 1;
    color_space_or_matrix_scale = dovi_dm_CommitIptRgb2LmsM33((int)(signal_index), (int)(color_space_or_matrix_scale), (long)(csc_control),
                                        (short *)((short *)(input_map + 0x432)));
    fixed_store_le16(input_map + 0x430, (short)color_space_or_matrix_scale);
    dovi_AssignM33I2I((const void *)((void *)(csc_control + 0x3c)), (void *)(input_map + 0x446));

    return;
}

unsigned long dovi_dm_CommitSigEnv2KsIMap(int *signal, long csc_control, unsigned char *input_map)
{
    _Alignas(16) struct dm_signal_input_workspace scratch;
    unsigned int gamma_log_value;
    unsigned short gamma_exponent;
    int signal_index;
    unsigned int gamma_normalization_shift;
    unsigned int gamma_index_or_power;
    unsigned short matrix_scale;
    unsigned long range_inverse;
    int color_offset_or_range;
    int chroma_offset;
    unsigned int gamma_value;
    unsigned char input_color;
    long gamma_byte_offset;
    unsigned int gamma_sample;
    unsigned int depth_gamma_offset_or_matrix_scale;
    unsigned int matrix_scale_or_gamma_gain;

    scratch.rgb_to_lms[8] = 0;
    depth_gamma_offset_or_matrix_scale = signal[10];
    scratch.rgb_to_lms[0] = 0;
    scratch.rgb_to_lms[1] = 0;
    scratch.rgb_to_lms[2] = 0;
    scratch.rgb_to_lms[3] = 0;
    scratch.rgb_to_lms[4] = 0;
    scratch.rgb_to_lms[5] = 0;
    scratch.rgb_to_lms[6] = 0;
    scratch.rgb_to_lms[7] = 0;
    scratch.offsets.words[2] = 0;
    scratch.offsets.pairs[0] = 0;
    if (signal[6] - 3U < 2) {
        input_color = 3;
    } else {
        input_color = 0;
        if (signal[6] != 0) {
            input_color = 1;
        }
    }
    signal_index = *signal;
    *input_map = input_color;
    color_offset_or_range = signal[6];
    if (color_offset_or_range - 3U < 2) {
        if (((color_offset_or_range != 4) || (signal[0x29] != 1)) || (signal[0xb] == 0)) {
            dovi_dm_CommitIpt2KsIMap((int)(*signal), (long)(csc_control), (int)(color_offset_or_range),
                                     (unsigned int)(signal[10]), (int)(signal[0x29]),
                                     (int)(signal[0xb]), (unsigned char *)(input_map));
        }
    } else {
        if (color_offset_or_range == 0) {
            if (signal[0x2b] == 0) {
                if (signal[0x29] == 0) {
                    matrix_scale_or_gamma_gain =
                        dovi_GetYuv2RgbM33NarrowFxp((unsigned int)(signal[0x2a]),
                                               (unsigned int)(signal[10]), (void *)(input_map + 4));
                    matrix_scale = (unsigned short)matrix_scale_or_gamma_gain;
                } else {
                    matrix_scale_or_gamma_gain =
                        dovi_GetYuv2RgbM33Fxp((unsigned int)(signal[0x2a]), (void *)(input_map + 4));
                    matrix_scale = (unsigned short)matrix_scale_or_gamma_gain;
                }
            } else {
                dovi_AssignM33I2I((const void *)(signal + 0x2c), (void *)(input_map + 4));
                matrix_scale = (unsigned short)signal[0x31];
            }
            fixed_store_le16(input_map + 2, matrix_scale);
            if (signal[0x32] == 0) {
                dovi_GetYuvRgbOffFxp((unsigned int)(signal[0x29]), (unsigned int)(depth_gamma_offset_or_matrix_scale),
                                (void *)(&scratch.offsets.pairs[0]));
            } else {
                dovi_AssignV3I2I((const void *)(signal + 0x33), (void *)(&scratch.offsets.pairs[0]));
            }
            color_offset_or_range = (int)scratch.offsets.pairs[0] << 0x10;
            chroma_offset = (*(unsigned int *)((unsigned char *)&scratch.offsets.pairs[0] + 4)) << 0x10;
            scratch.offsets.pairs[0] = CONCAT44(chroma_offset, color_offset_or_range);
        } else {
            color_offset_or_range = 0;
            chroma_offset = 0;
            fixed_store_le64(input_map + 10, 0x20000000);
            fixed_store_le64(input_map + 2, 0x2000000d);
            fixed_store_le32(input_map + 0x12, 0x20000000);
        }
        dovi_Yuv2RgbOffsetRgbFxp((const void *)(input_map + 4), (unsigned int)(color_offset_or_range),
                            (unsigned int)(chroma_offset), (void *)(input_map + 0x18));
        if (signal[6] - 1U < 2) {
            color_offset_or_range = signal[0x29];
        } else {
            color_offset_or_range = 1;
        }
        dovi_GetRange((unsigned int)(depth_gamma_offset_or_matrix_scale), (unsigned int)(color_offset_or_range), (void *)(input_map + 0x24),
                 (void *)(input_map + 0x26));
        range_inverse = dovi_sdiv((unsigned long long)(0xffffffff),
                     (unsigned int)((unsigned int)(unsigned short)fixed_load_le16(input_map + 0x26)
                                    << (unsigned long)(0x10 - depth_gamma_offset_or_matrix_scale & 0x1f)));
        fixed_store_le16(input_map + 0x2c, (short)depth_gamma_offset_or_matrix_scale);
        fixed_store_le32(input_map + 0x28, (int)range_inverse);
        input_map[0x2e] = signal[0x3d] == 1;
        if (signal[0x3d] == 3) {
            input_map[0x2e] = 0;
            dovi_DeriveHlgEotfParam((void *)(input_map), (unsigned int)(signal[0x3f]),
                               (unsigned int)(signal[0x40]));
        } else if (signal[0x3d] == 0) {
            matrix_scale_or_gamma_gain = (unsigned int)*(unsigned short *)(signal + 0x42);
            scratch.gamma[2] = 0;
            depth_gamma_offset_or_matrix_scale = (unsigned int)*(unsigned short *)((long)signal + 0x10a);
            if (*(unsigned short *)(signal + 0x42) == 0 &&
                *(unsigned short *)((long)signal + 0x10a) == 0) {
                scratch.gamma[0] = 0;
                scratch.gamma[1] = 0;
                dovi_GetGammaABG((unsigned int)(signal[0x3e]), (unsigned long long)(signal[0x40]),
                            (unsigned long long)(signal[0x3f]), (void *)(&scratch.gamma[1]),
                            (void *)(&scratch.gamma[0]), (void *)(&scratch.gamma[2]));
                depth_gamma_offset_or_matrix_scale = scratch.gamma[0];
                matrix_scale_or_gamma_gain = scratch.gamma[1];
            } else {
                scratch.gamma[2] = signal[0x43];
            }
            gamma_byte_offset = 0;
            gamma_sample = 0;
            gamma_exponent = *(unsigned short *)(signal + 0x3e);
            do {
                gamma_log_value = (depth_gamma_offset_or_matrix_scale & 0xffff) + (gamma_sample & 0xffff);
                gamma_normalization_shift = gamma_log_value >> 0x10;
                gamma_log_value = gamma_log_value >> (unsigned long)gamma_normalization_shift;
                if ((0x1000f < gamma_log_value) || (gamma_index_or_power = gamma_log_value >> 4, gamma_index_or_power == 0x1000)) {
                    dm_limit_trap();
                }
                gamma_log_value = ((unsigned int)dovi_log10_value[gamma_index_or_power] -
                         (dovi_log10_rate[gamma_index_or_power] * (gamma_log_value & 0xf) >> 8)) *
                        (unsigned int)gamma_exponent;
                if (gamma_log_value >> 0x1b < 5) {
                    gamma_index_or_power =
                        (unsigned int)((unsigned long)dovi_power10_value[gamma_log_value >> 0x1a]
                                                                        [gamma_log_value >> 0x10 & 0x3ff] *
                                           (unsigned long)scratch.gamma[2] >>
                                       0x12);
                    if (gamma_normalization_shift == 0) {
                        gamma_index_or_power = dovi_power10_value[gamma_log_value >> 0x1a][gamma_log_value >> 0x10 & 0x3ff];
                    }
                    gamma_value =
                        (unsigned int)((unsigned long)(matrix_scale_or_gamma_gain & 0xffff) * (unsigned long)gamma_index_or_power >>
                                       ((unsigned long)(dovi_power10_scale[gamma_log_value >> 0x1a] - 0x10) &
                                        0x3f));
                } else {
                    gamma_value = 0;
                }
                fixed_store_le32(input_map + gamma_byte_offset + 0x30, gamma_value);
                gamma_sample = gamma_sample + 0x100;
                gamma_byte_offset = gamma_byte_offset + 4;
            } while (gamma_byte_offset != 0x400);
        }
        if (signal[0x18] == 0) {
            if (signal[0xe] == 0) {
                depth_gamma_offset_or_matrix_scale = dovi_GetRgb2LmsM33((unsigned int)(signal[0xd]), (void *)(scratch.rgb_to_lms));
            } else {
                depth_gamma_offset_or_matrix_scale = dovi_GetRgb2LmsByV8PrimsM33Fxp((const void *)(signal + 0xf),
                                                   (void *)(scratch.rgb_to_lms));
            }
        } else {
            dovi_MemCpyByte((void *)(scratch.rgb_to_lms), (const void *)(signal + 0x19), (int)(0x12));
            depth_gamma_offset_or_matrix_scale = signal[0x1e];
        }
        matrix_scale = dovi_GetRgb2LmsCtWpM33Fxp(
            (const void *)(scratch.rgb_to_lms), (unsigned int)(depth_gamma_offset_or_matrix_scale),
            (unsigned int)(signal[0x27]), (const void *)(csc_control + (long)signal_index * 0x28 + 0x80),
            (unsigned int)(*(unsigned int *)(csc_control + (long)*signal * 0x28 + 0x8c)),
            (void *)(input_map + 0x432));
        fixed_store_le16(input_map + 0x430, matrix_scale);
        dovi_AssignM33I2I((const void *)((void *)(csc_control + 0x3c)), (void *)(input_map + 0x446));
        fixed_store_le16(input_map + 0x444, (short)*(unsigned int *)(csc_control + 0x50));
    }
    dovi_GetCvmBypassNormParam((unsigned int)(signal[10]), (unsigned int)(signal[0x29]),
                          (void *)(input_map + 0x45c), (void *)(input_map + 0x458));

    return 0;
}

unsigned long dovi_dm_Commit2Dm3KsIMapBypass(unsigned int bit_depth, unsigned char *input_map)
{
    unsigned int normalization;
    long sample_max;

    fixed_store_le32(input_map + 0x464, 0);
    *input_map = 3;
    fixed_store_le64(input_map + 0x45c, 0);
    if (bit_depth == 0) {
        normalization = 0;
    } else {
        sample_max = (long)~(-1 << (unsigned long)(bit_depth & 0x1f));
        normalization = 0;
        if (sample_max != 0) {
            normalization = (unsigned int)(0xffffffff / sample_max);
        }
    }
    fixed_store_le32(input_map + 0x458, normalization);
    return 0;
}
