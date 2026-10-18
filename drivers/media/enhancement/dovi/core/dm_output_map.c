/* Output color-space and transfer maps. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_map_workspace.h"

int dovi_dm_CommitSigEnv2KsOMap(unsigned int signal_index, long config, long context)
{
    _Alignas(16) struct dm_signal_output_workspace scratch;
    long *kernel_slot;
    int *current_mode;
    int whitepoint_green;
    int whitepoint_blue;
    long signal_context;
    unsigned short off_diagonal;
    _Bool bypass_enabled;
    unsigned short matrix_word;
    unsigned int matrix_factor;
    unsigned int output_matrix_scale;
    unsigned int output_word;
    unsigned int transform_parameter;
    unsigned long wide_factor;
    unsigned char output_format_code;
    int conversion_value;
    long output_map;
    unsigned int target_minimum;
    unsigned int target_maximum;
    unsigned short *output_words;

    kernel_slot = (long *)(context + (long)(int)(signal_index - 1) * 8 + 0x1f918);
    output_word = *(unsigned int *)(config + 0x8c);
    scratch.lms_to_rgb[8] = 0;
    scratch.lms_to_rgb[0] = 0;
    scratch.lms_to_rgb[1] = 0;
    scratch.lms_to_rgb[2] = 0;
    scratch.lms_to_rgb[3] = 0;
    scratch.lms_to_rgb[4] = 0;
    scratch.lms_to_rgb[5] = 0;
    scratch.lms_to_rgb[6] = 0;
    scratch.lms_to_rgb[7] = 0;
    scratch.third_offset = 0;
    output_words = *(unsigned short **)(*kernel_slot + 0x15f8);
    scratch.offsets.pair = 0;
    if (*(int *)(config + 0x7c) - 3U < 2) {
        output_format_code = 3;
    } else {
        output_format_code = 0;
        if (*(int *)(config + 0x7c) != 0) {
            output_format_code = 1;
        }
    }
    *(unsigned char *)(output_words + 0x1e) = output_format_code;
    conversion_value = *(int *)(config + 0x7c);
    dovi_AssignM33I2I((const void *)((void *)(context + 0x43f4)), (void *)(output_words + 1));
    *output_words = (short)*(unsigned int *)(context + 0x4408);
    if (1 < conversion_value - 3U) {
        if (*(int *)(config + 0xc4) == 0) {
            if (*(int *)(config + 0x9c) == 0) {
                transform_parameter = dovi_GetRgb2LmsM33((unsigned int)(*(unsigned int *)(config + 0x98)),
                                       (void *)(scratch.lms_to_rgb));
                matrix_factor = dovi_InvM33((const void *)(scratch.lms_to_rgb), (void *)(scratch.lms_to_rgb),
                                (unsigned int)(transform_parameter), (unsigned int)(transform_parameter));
                transform_parameter = transform_parameter - matrix_factor;
            } else {
                transform_parameter = dovi_GetLms2RgbByV8PrimsM33Fxp((const void *)(config + 0xa0),
                                                   (void *)(scratch.lms_to_rgb));
            }
        } else {
            dovi_MemCpyByte((void *)(scratch.lms_to_rgb), (const void *)((void *)(config + 200)),
                       (int)(0x12));
            transform_parameter = *(unsigned int *)(config + 0xdc);
        }
        dovi_InvM33((const void *)(scratch.lms_to_rgb), (void *)((void *)(context + 0x440c)),
               (unsigned int)(transform_parameter), (unsigned int)(*(unsigned int *)(config + 0xdc)));
        matrix_word = dovi_GetLms2RgbCtWpM33Fxp(
            (const void *)(scratch.lms_to_rgb), (unsigned int)(transform_parameter),
            (unsigned int)(*(unsigned int *)(context + 0x43d8)),
            (unsigned long long)(context + 0x4420 + (long)(int)signal_index * 0x28),
            (const void *)(context + 0x4420), (unsigned int)(*(unsigned int *)(context + 0x442c)),
            (void *)(output_words + 0xb));
        output_words[10] = matrix_word;
        output_map = *(long *)(*kernel_slot + 0x15f8);
        if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
            (signal_context = context, *(unsigned int *)(config + 4) != signal_index)) {
            if (5 < signal_index)
                goto invalid_signal_index;
            signal_context = context + (unsigned long)signal_index * 0x1f78;
        }
        target_minimum = 0;
        target_maximum = 0x9c400000;
        if ((1 < *(int *)(config + 0x7c) - 3U) && (*(int *)(context + 0x43bc) != 1)) {
            target_minimum = *(unsigned int *)(signal_context + 0x455c);
            target_maximum = *(unsigned int *)(signal_context + 0x4560);
        }
        *(unsigned int *)(output_map + 0x28) = target_minimum;
        *(unsigned int *)(output_map + 0x2c) = target_maximum;
        if (*(int *)(config + 0x7c) - 1U < 2) {
            target_minimum = *(unsigned int *)(config + 0x108);
        } else {
            target_minimum = 1;
        }
        dovi_GetRange((unsigned int)(output_word), (unsigned int)(target_minimum), (void *)(output_words + 0x18),
                 (void *)(output_words + 0x19));
        wide_factor =
            dovi_sdiv((unsigned long long)((unsigned long)(unsigned short)output_words[0x19] << 0x10),
                 (unsigned int)(~(-1 << (unsigned long)(*(unsigned int *)(config + 0x8c) & 0x1f))));
        output_words[0x1c] = (short)output_word;
        fixed_store_le32(output_words + 0x1a, (int)wide_factor);
        bypass_enabled = 1;
        if (*(int *)(context + 0x43bc) != 1) {
            if (*(int *)(config + 0x7c) - 3U < 2) {
                bypass_enabled = 1;
            } else {
                bypass_enabled = 0;
                if (*(int *)(config + 0x158) != 0) {
                    bypass_enabled = *(int *)(config + 0x158) == 1;
                }
            }
        }
        *(_Bool *)(output_words + 0x1d) = bypass_enabled;
        if (*(int *)(config + 0x7c) == 1) {
            fixed_store_le64(output_words + 0x2a, 0);
            fixed_store_le32(output_words + 0x2e, 0);
            fixed_store_le64(output_words + 0x21, 0x1000000000000);
            fixed_store_le64(output_words + 0x25, 0x1000000000000);
            fixed_store_le32(output_words + 0x1f, 0x10000);
            goto set_bypass_denormalization;
        }
        if (*(int *)(config + 0x110) == 0) {
            if (*(int *)(config + 0x108) == 0) {
                transform_parameter = dovi_GetRgb2YuvM33NarrowFxp((unsigned int)(*(unsigned int *)(config + 0x10c)),
                                                (unsigned int)(*(unsigned int *)(config + 0x8c)),
                                                (void *)(output_words + 0x20));
                matrix_word = (unsigned short)transform_parameter;
            } else {
                transform_parameter = dovi_GetRgb2YuvM33Fxp((unsigned int)(*(unsigned int *)(config + 0x10c)),
                                          (void *)(output_words + 0x20));
                matrix_word = (unsigned short)transform_parameter;
            }
        } else {
            dovi_AssignM33I2I((const void *)((void *)(config + 0x114)), (void *)(output_words + 0x20));
            matrix_word = (unsigned short)*(unsigned int *)(config + 0x128);
        }
        output_words[0x1f] = matrix_word;
        if (*(int *)(config + 0x148) == 0) {
            if (*(int *)(config + 300) != 0) {
                dovi_AssignV3I2I((const void *)((void *)(config + 0x130)),
                            (void *)(&scratch.offsets.pair));
                conversion_value = 0xf;
                goto scale_explicit_offsets;
            }
            dovi_GetYuvRgbOffFxp((unsigned int)(*(unsigned int *)(config + 0x108)),
                            (unsigned int)(output_word), (void *)(&scratch.offsets.pair));
            fixed_store_le32(output_words + 0x2a, (unsigned int)scratch.offsets.pair);
            fixed_store_le32(output_words + 0x2c,
                             (*(unsigned int *)((unsigned char *)&scratch.offsets.pair + 4)));
            output_word = scratch.third_offset;
        } else {
            dovi_AssignV3I2I((const void *)((void *)(config + 0x14c)), (void *)(&scratch.offsets.pair));
            conversion_value = 0x10;
        scale_explicit_offsets:
            output_word = conversion_value - output_word;
            fixed_store_le32(output_words + 0x2c,
                             (*(unsigned int *)((unsigned char *)&scratch.offsets.pair + 4)) >>
                                 (unsigned long)(output_word & 0x1f));
            fixed_store_le32(output_words + 0x2a,
                             (unsigned int)scratch.offsets.pair >> (unsigned long)(output_word & 0x1f));
            output_word = scratch.third_offset >> (unsigned long)(output_word & 0x1f);
        }
        fixed_store_le32(output_words + 0x2e, output_word);
        goto set_bypass_denormalization;
    }
    scratch.wide_matrix[7] = 0;
    scratch.wide_matrix[8] = 0;
    scratch.wide_matrix[5] = 0;
    scratch.wide_matrix[6] = 0;
    transform_parameter = 0x28f;
    if (*(int *)(config + 0x7c) != 3) {
        transform_parameter = 0x51f;
    }
    scratch.wide_matrix[3] = 0;
    scratch.wide_matrix[4] = 0;
    scratch.wide_matrix[1] = 0;
    scratch.wide_matrix[2] = 0;
    scratch.wide_matrix[0] = 0;
    if (*(unsigned int *)(context + 0x43d8) == transform_parameter) {
        scratch.wide_matrix[0] = (long)*(int *)(context + 0x4420);
        scratch.wide_matrix[4] = (long)*(int *)(context + 0x4424);
        scratch.wide_matrix[2] = 0;
        scratch.wide_matrix[3] = 0;
        scratch.wide_matrix[6] = 0;
        scratch.wide_matrix[7] = 0;
        scratch.wide_matrix[1] = 0;
        scratch.wide_matrix[5] = 0;
        matrix_factor = *(unsigned int *)(context + (long)(int)signal_index * 0x28 + 0x442c);
        scratch.wide_matrix[8] = (long)*(int *)(context + 0x4428);
    } else {
        scratch.crosstalk.packed.last = 0;
        scratch.crosstalk.packed.pairs[0] = 0;
        scratch.crosstalk.packed.pairs[1] = 0;
        matrix_factor = dovi_RE_CT2RIGHT_M33((void *)(&scratch.crosstalk.packed.pairs[0]),
                                 (unsigned int)(*(unsigned int *)(context + 0x43d8)),
                                 (unsigned int)(transform_parameter));
        conversion_value = *(int *)(context + 0x4420);
        whitepoint_green = *(int *)(context + 0x4424);
        wide_factor =
            (unsigned long)*(unsigned int *)(context + (long)(int)signal_index * 0x28 + 0x442c);
        whitepoint_blue = *(int *)(context + 0x4428);
        scratch.wide_matrix[0] =
            (long)(int)(short)scratch.crosstalk.packed.pairs[0] * (long)conversion_value >> (wide_factor & 0x3f);
        scratch.wide_matrix[1] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[0] + 2)) *
                (long)whitepoint_green >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[6] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[1] + 4)) *
                (long)conversion_value >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[7] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[1] + 6)) *
                (long)whitepoint_green >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[3] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[0] + 6)) *
                (long)conversion_value >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[2] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[0] + 4)) *
                (long)whitepoint_blue >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[4] =
            (long)(int)(short)scratch.crosstalk.packed.pairs[1] * (long)whitepoint_green >> (wide_factor & 0x3f);
        scratch.wide_matrix[5] =
            (long)(int)(short)(*(
                unsigned short *)((unsigned char *)&scratch.crosstalk.packed.pairs[1] + 2)) *
                (long)whitepoint_blue >>
            (wide_factor & 0x3f);
        scratch.wide_matrix[8] =
            (long)(int)scratch.crosstalk.packed.last * (long)whitepoint_blue >> (wide_factor & 0x3f);
    }
    output_matrix_scale = dovi_Adjust64M33ScaleTo16M33((const void *)(&scratch.wide_matrix[0]),
                                     (unsigned int)(matrix_factor), (void *)(output_words + 0xb));
    matrix_factor = transform_parameter * -3 + 0x8000;
    wide_factor = dovi_sdiv((unsigned long long)((unsigned long)(transform_parameter * -0x8000 + 0x40000000)),
                  (unsigned int)(matrix_factor));
    matrix_word = (unsigned short)(wide_factor >> 1);
    wide_factor = dovi_sdiv((unsigned long long)((long)(int)(transform_parameter * -0x8000)), (unsigned int)(matrix_factor));
    off_diagonal = (unsigned short)(wide_factor >> 1);
    *(unsigned short *)(context + 0x440c) = matrix_word;
    *(unsigned short *)(context + 0x4414) = matrix_word;
    *(unsigned short *)(context + 0x441c) = matrix_word;
    *(unsigned short *)(context + 0x440e) = off_diagonal;
    *(unsigned short *)(context + 0x4410) = off_diagonal;
    *(unsigned short *)(context + 0x4412) = off_diagonal;
    *(unsigned short *)(context + 0x4416) = off_diagonal;
    *(unsigned short *)(context + 0x4418) = off_diagonal;
    *(unsigned short *)(context + 0x441a) = off_diagonal;
    output_words[10] = (short)output_matrix_scale;
    output_words[0x18] = 0;
    transform_parameter = *(unsigned int *)(config + 0x8c);
    *(unsigned char *)(output_words + 0x1d) = 1;
    output_words[0x1c] = (short)output_word;
    fixed_store_le64(output_words + 0x14, 0x9c40000000000000);
    fixed_store_le32(output_words + 0x1a, ~(-1 << (unsigned long)(transform_parameter & 0x1f))
                                         << (unsigned long)((0x10 - transform_parameter) & 0x1f));
    current_mode = (int *)(context + 0x43bc);
    output_map = *(long *)(*kernel_slot + 0x15f8);
    if ((*(int *)(context + 0x43b8) == *current_mode) ||
        (*(unsigned int *)(config + 4) != signal_index)) {
        if (5 < signal_index) {
        invalid_signal_index:
            dm_limit_trap();
        }
        context = context + (unsigned long)signal_index * 0x1f78;
    }
    target_minimum = 0;
    target_maximum = 0x9c400000;
    if ((1 < *(int *)(config + 0x7c) - 3U) && (*current_mode != 1)) {
        target_minimum = *(unsigned int *)(context + 0x455c);
        target_maximum = *(unsigned int *)(context + 0x4560);
    }
    *(unsigned int *)(output_map + 0x28) = target_minimum;
    *(unsigned int *)(output_map + 0x2c) = target_maximum;
    if (((*(int *)(config + 0x7c) == 4) && (*(int *)(config + 0x108) == 1)) &&
        (*(int *)(config + 0x90) != 0)) {
    load_full_ictcp_matrix:
        output_word = dovi_GetLms2IctcpIoM33((void *)(output_words + 0x20));
        matrix_word = (unsigned short)output_word;
    publish_ictcp_scale:
        output_words[0x1f] = matrix_word;
        if (*(int *)(config + 0x90) != 0) {
            output_words[0x23] = (short)((unsigned long)(long)(short)output_words[0x23] >> 1);
            output_words[0x24] = (short)((unsigned long)(long)(short)output_words[0x24] >> 1);
            output_words[0x25] = (short)((unsigned long)(long)(short)output_words[0x25] >> 1);
        }
    } else {
        *(unsigned char *)(output_words + 0x1e) = 0;
        if (*(int *)(config + 0x7c) == 4) {
            if (*(int *)(config + 0x108) == 1)
                goto load_full_ictcp_matrix;
            output_word = dovi_GetLms2IctcpIoM33Narrow((unsigned int)(*(unsigned int *)(config + 0x8c)),
                                             (void *)(output_words + 0x20));
            matrix_word = (unsigned short)output_word;
            goto publish_ictcp_scale;
        }
        if (*(int *)(config + 0x108) == 1) {
            output_word = dovi_GetLms2IptM33Fxp((void *)(output_words + 0x20));
            matrix_word = (unsigned short)output_word;
        } else {
            output_word = dovi_GetLms2IptM33NarrowFxp((unsigned int)(*(unsigned int *)(config + 0x8c)),
                                            (void *)(output_words + 0x20));
            matrix_word = (unsigned short)output_word;
        }
        output_words[0x1f] = matrix_word;
    }
    dovi_GetYuvRgbOffFxp((unsigned int)(*(unsigned int *)(config + 0x108)),
                    (unsigned int)(*(unsigned int *)(config + 0x8c)), (void *)(output_words + 0x2a));
set_bypass_denormalization:
    dovi_GetCvmBypassDnParam((unsigned int)(*(unsigned int *)(config + 0x8c)),
                        (unsigned int)(*(unsigned int *)(config + 0x108)), (void *)(output_words + 0x32),
                        (void *)(output_words + 0x30));

    return 0;
}

void dovi_dm_UpdateKsOMapTmm(unsigned int signal_index, long config, long context)
{
    int *current_mode;
    long output_map;
    unsigned int minimum;
    unsigned int maximum;

    current_mode = (int *)(context + 0x43bc);
    output_map =
        *(long *)(*(long *)(context + (long)(int)(signal_index - 1) * 8 + 0x1f918) + 0x15f8);
    if ((*(int *)(context + 0x43b8) == *current_mode) ||
        (*(unsigned int *)(config + 4) != signal_index)) {
        if (5 < signal_index) {
            dm_limit_trap();
        }
        context = context + (unsigned long)signal_index * 0x1f78;
    }
    minimum = 0;
    maximum = 0x9c400000;
    if ((1 < *(int *)(config + 0x7c) - 3U) && (*current_mode != 1)) {
        minimum = *(unsigned int *)(context + 0x455c);
        maximum = *(unsigned int *)(context + 0x4560);
    }
    fixed_store_le32((void *)(output_map + 0x28), minimum);
    fixed_store_le32((void *)(output_map + 0x2c), maximum);
    return;
}

unsigned long dovi_dm_Commit2Dm3KsOMapBypass(unsigned int bit_depth, long output_map)
{
    fixed_store_le64((void *)(output_map + 100), 0);
    fixed_store_le32((void *)(output_map + 0x6c), 0);
    *(unsigned char *)(output_map + 0x3c) = 3;
    fixed_store_le32((void *)(output_map + 0x60), ~(-1 << (unsigned long)(bit_depth & 0x1f)));
    return 0;
}
