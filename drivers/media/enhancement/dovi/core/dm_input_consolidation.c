/* Consolidation of input metadata and color transforms. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "dm_workspace.h"
#include "tone_controls.h"
#include "fixed_access.h"

/* Keep the intermediate stores: configuration and metadata may share the
 * context backing store, so dimensions are reloaded after each crop update. */
static void consolidate_active_area(unsigned int signal_index, void *config, unsigned int flags,
                                    const struct dm_metadata_format *metadata, long context)
{
    struct dm_active_area *area = (void *)(context + DM_CONTEXT_ACTIVE_AREA_OFFSET +
                                           (unsigned long)signal_index * sizeof(*area));
    struct dm_signal_environment *signal = dm_config_signal(config, signal_index);
    unsigned int *height = (void *)&signal->height;
    unsigned int *width = (void *)&signal->width;
    unsigned int bottom, right, top, left, enabled;
    unsigned short metadata_top, metadata_left;

    fixed_store_le64(&area->enabled, 0);
    bottom = *height;
    area->left = 0;
    area->bottom = bottom;
    area->right = *width;
    if (metadata == (void *)0 || (flags & 0x20) == 0 || (metadata->flags & 0x20) == 0)
        return;

    area->enabled = 1;
    metadata_top = metadata->active_top;
    area->top = metadata_top;
    bottom = *height - (unsigned int)metadata->active_bottom;
    area->bottom = bottom;
    metadata_left = metadata->active_left;
    area->left = metadata_left;
    right = *width - (unsigned int)metadata->active_right;
    area->right = right;
    top = metadata_top;
    if ((*height == top || (int)*height < (int)(unsigned int)metadata_top) ||
        (left = metadata_left, *width == left || (int)*width < (int)(unsigned int)metadata_left) ||
        ((int)bottom < 1 || (int)right < 1 || bottom <= top || (int)right <= (int)left)) {
        area->top = 0;
        area->left = 0;
        fixed_store_le64(&area->bottom, 0);
        if (*height != 0) {
            enabled = 1;
        } else {
            right = 0;
            enabled = right != *width;
        }
    } else {
        area->top = top;
        area->left = left;
        if ((int)*height <= (int)bottom)
            bottom = *height;
        area->bottom = bottom;
        if ((int)*width <= (int)right)
            right = *width;
        area->right = right;
        enabled = 1;
        if (metadata_top == 0 && bottom == *height && metadata_left == 0)
            enabled = right != *width;
    }
    area->enabled = enabled;
    area->metadata_present = 1;
}

static void consolidate_user_adjustments(unsigned int signal_index, const void *config,
                                         long context)
{
    const struct dm_user_adjustments *adjustments =
        (const void *)(context + DM_CONTEXT_USER_ADJUSTMENTS_OFFSET);
    struct dm_trim_adjustments *summary = (void *)(context + 0x43a0);
    struct dm_trim_adjustments *trim;
    long signal_context, white_point;
    int scaled, value;
    unsigned int input;
    unsigned long quotient;

    if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
        (signal_context = context,
         ((const struct dm_control *)config)->primary_signal != signal_index)) {
        signal_context = context + (unsigned long)signal_index * DM_CONTEXT_INPUT_STATE_STRIDE;
    }
    trim = (void *)(signal_context + 0x5c30);
    scaled = adjustments->contrast;
    input = scaled + 0x4000000;
    if (0x4000000 < scaled) {
        input = 0x8000000;
    }
    if (scaled < 0) {
        input = 0x4000000;
    }
    quotient = dovi_sdiv((unsigned long long)(0x10000000000000), (unsigned int)(input));
    trim->contrast_power = (int)quotient + -0x4000000;
    value = adjustments->contrast;
    scaled = 0;
    if (value < 1) {
        scaled = value >> 1;
    }
    if (value < -0x4000000) {
        scaled = -0x2000000;
    }
    trim->contrast_slope = scaled;
    value = adjustments->chroma_weight;
    scaled = value >> 1;
    if (0x4000000 < value) {
        scaled = 0x2000000;
    }
    if (value < -0x4000000) {
        scaled = -0x2000000;
    }
    trim->chroma_weight = scaled;
    value = adjustments->saturation_gain;
    scaled = value >> 1;
    if (0x4000000 < value) {
        scaled = 0x2000000;
    }
    if (value < -0x4000000) {
        scaled = -0x2000000;
    }
    trim->saturation_gain = scaled;
    scaled = adjustments->luminance_gain;
    if (0x3ffffff < scaled) {
        scaled = 0x4000000;
    }
    if (scaled < 1) {
        scaled = 0;
    }
    trim->luminance_gain = scaled;
    input = (unsigned int)adjustments->white_point;
    white_point = (long)(int)input * 4000;
    if (-1 < (int)input) {
        white_point = (unsigned long)input * 6000;
    }
    if (0x5dafffffff < white_point) {
        white_point = 0x5db0000000;
    }
    if (white_point < -0x3e8fffffff) {
        white_point = -0x3e90000000;
    }
    *(long *)(context + 0x1f490) = white_point;
    summary->contrast_power = trim->contrast_power >> 0xe;
    summary->contrast_slope = trim->contrast_slope >> 0xe;
    summary->chroma_weight = trim->chroma_weight >> 0xe;
    summary->saturation_gain = trim->saturation_gain >> 0xe;
    summary->luminance_gain = trim->luminance_gain >> 8;
}

static unsigned int consolidate_input_color(unsigned int signal_index, int *config,
                                            unsigned int *flags, unsigned char *metadata,
                                            long context, unsigned int *color_state,
                                            struct dm_consolidation_workspace *work)
{
    struct dm_control *control = (void *)config;
    unsigned char metadata_primaries_index;
    unsigned int color_scale_or_normalization;
    unsigned long signal_scale_or_quotient;
    const int *primaries;
    unsigned long signal_slot;
    unsigned int white_green;
    int primary_coordinate_or_white_short;
    unsigned int input_flags;
    unsigned int primary_signal_or_white_red;
    int white_long;
    int adjusted_white_long;
    int adjusted_white_medium;
    int white_medium;
    long selected_context_or_cct;
    const int *input_primaries;
    unsigned int *normalization_primaries;
    const int *source;
    signal_slot = signal_index;
    work->source.pairs[2] = 0;
    work->source.pairs[3] = 0;
    input_flags = *flags;
    work->source.pairs[0] = (unsigned long)0x0;
    work->source.pairs[1] = 0;
    work->target.pairs[2] = 0;
    work->target.pairs[3] = 0;
    work->target.pairs[0] = (unsigned long)0x0;
    work->target.pairs[1] = 0;
    work->color_matrix.coefficients[8] = 0;
    work->color_matrix.pairs[0] = 0;
    work->color_matrix.pairs[1] = 0;
    work->temperature.pairs[4] = work->temperature.pairs[4] & 0xffffffff00000000;
    work->temperature.pairs[2] = 0;
    work->temperature.pairs[3] = 0;
    work->temperature.pairs[0] = 0;
    work->temperature.pairs[1] = 0;
    work->xyz.coefficients[8] = 0;
    work->xyz.pairs[2] = 0;
    work->xyz.pairs[3] = 0;
    work->xyz.pairs[0] = 0;
    work->xyz.pairs[1] = 0;
    work->white_xy = 0;
    work->white_xyz.words[2] = 0;
    work->white_xyz.pairs[0] = 0;
    work->rgb.coefficients[8] = 0;
    work->rgb.pairs[2] = 0;
    work->rgb.pairs[3] = 0;
    work->rgb.pairs[0] = 0;
    work->rgb.pairs[1] = 0;
    work->diagonal.words[2] = 0;
    work->diagonal.pairs[0] = 0;
    if ((((metadata == 0) || ((input_flags >> 9 & 1) == 0)) || ((metadata[1] >> 1 & 1) == 0)) ||
        ((int)color_state[0x1a] < 4)) {
        input_primaries = dm_config_signal(config, signal_index)->primaries;
        color_scale_or_normalization = dm_config_signal(config, signal_index)->primaries_scale;
    store_primaries_scale:
        color_state[0x19] = color_scale_or_normalization;
    } else {
        metadata_primaries_index = metadata[0x5a];
        color_scale_or_normalization = 0x1a;
        color_state[0x19] = 0x1a;
        if ((0x12 < (unsigned long)metadata_primaries_index) ||
            (input_primaries = (const int *)dovi_v8_primaries[metadata_primaries_index], input_primaries == 0)) {
            input_primaries = (int *)&work->source.pairs[0];
            work->target.pairs[0] =
                (unsigned long)CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x5e)),
                                        (int)fixed_signed16(fixed_load_le16(metadata + 0x5c)));
            work->source.pairs[0] =
                (unsigned long)CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x5e)),
                                        (int)fixed_signed16(fixed_load_le16(metadata + 0x5c)));
            work->target.pairs[1] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x62)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 0x60)));
            work->source.pairs[1] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x62)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 0x60)));
            work->target.pairs[2] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x66)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 100)));
            work->source.pairs[2] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x66)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 100)));
            color_scale_or_normalization = 0xf;
            work->target.pairs[3] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x6a)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 0x68)));
            work->source.pairs[3] = CONCAT44((int)fixed_signed16(fixed_load_le16(metadata + 0x6a)),
                                             (int)fixed_signed16(fixed_load_le16(metadata + 0x68)));
            goto store_primaries_scale;
        }
    }
    if (*(int *)(context + 0x43b8) == 0) {
        signal_scale_or_quotient = 0;
        source = config + 0x28;
    } else {
        primary_signal_or_white_red = control->primary_signal;
        signal_scale_or_quotient = signal_slot;
        source = input_primaries;
        if (primary_signal_or_white_red != signal_index) {
            selected_context_or_cct = context;
            if (*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) {
                if (5 < primary_signal_or_white_red)
                    dm_limit_trap();
                selected_context_or_cct = context + (unsigned long)primary_signal_or_white_red * 0x1f78;
            }
            signal_scale_or_quotient = 0;
            source = (int *)(selected_context_or_cct + 0xe2f0);
        }
    }
    color_state[0x34] = config[signal_scale_or_quotient * 0x48 + 0x30];
    if ((int)color_scale_or_normalization < 0x1a) {
        color_scale_or_normalization = 0x1a - color_scale_or_normalization;
        primary_coordinate_or_white_short = *input_primaries << (unsigned long)(color_scale_or_normalization & 0x1f);
        work->source.pairs[0] = (unsigned long)CONCAT44(
            (*(unsigned int *)((unsigned char *)&work->source.pairs[0] + 4)), primary_coordinate_or_white_short);
        work->source.pairs[0] =
            (unsigned long)CONCAT44(input_primaries[1] << (unsigned long)(color_scale_or_normalization & 0x1f), primary_coordinate_or_white_short);
        primary_coordinate_or_white_short = input_primaries[2] << (unsigned long)(color_scale_or_normalization & 0x1f);
        work->source.pairs[1] =
            CONCAT44((*(unsigned int *)((unsigned char *)&work->source.pairs[1] + 4)), primary_coordinate_or_white_short);
        work->source.pairs[1] = CONCAT44(input_primaries[3] << (unsigned long)(color_scale_or_normalization & 0x1f), primary_coordinate_or_white_short);
        primary_coordinate_or_white_short = input_primaries[4] << (unsigned long)(color_scale_or_normalization & 0x1f);
        work->source.pairs[2] =
            CONCAT44((*(unsigned int *)((unsigned char *)&work->source.pairs[2] + 4)), primary_coordinate_or_white_short);
        work->source.pairs[2] = CONCAT44(input_primaries[5] << (unsigned long)(color_scale_or_normalization & 0x1f), primary_coordinate_or_white_short);
        primary_coordinate_or_white_short = input_primaries[6] << (unsigned long)(color_scale_or_normalization & 0x1f);
        work->source.pairs[3] =
            CONCAT44((*(unsigned int *)((unsigned char *)&work->source.pairs[3] + 4)), primary_coordinate_or_white_short);
        work->source.pairs[3] = CONCAT44(input_primaries[7] << (unsigned long)(color_scale_or_normalization & 0x1f), primary_coordinate_or_white_short);
    }
    color_scale_or_normalization = dovi_GetRgb2LmsByV8PrimsM33Fxp((const void *)(input_primaries),
                                       (void *)(&work->color_matrix.pairs[0]));
    color_scale_or_normalization = dovi_APPLY_CT2RIGHT((const void *)(&work->color_matrix.pairs[0]), (unsigned int)(color_scale_or_normalization),
                            (unsigned int)(*(unsigned int *)(context + 0x43d8)),
                            (void *)((long)color_state + 0x2e));
    color_state[0x10] = (int)(short)color_scale_or_normalization;
    primaries = source;
    if ((int)color_state[0x34] < 0x1a) {
        color_scale_or_normalization = 0x1a - color_state[0x19];
        work->target.pairs[0] = (unsigned long)CONCAT44(source[1] << (unsigned long)(color_scale_or_normalization & 0x1f),
                                                        *source << (unsigned long)(color_scale_or_normalization & 0x1f));
        work->target.pairs[1] = CONCAT44(source[3] << (unsigned long)(color_scale_or_normalization & 0x1f),
                                         source[2] << (unsigned long)(color_scale_or_normalization & 0x1f));
        work->target.pairs[2] = CONCAT44(source[5] << (unsigned long)(color_scale_or_normalization & 0x1f),
                                         source[4] << (unsigned long)(color_scale_or_normalization & 0x1f));
        work->target.pairs[3] = CONCAT44(source[7] << (unsigned long)(color_scale_or_normalization & 0x1f),
                                         source[6] << (unsigned long)(color_scale_or_normalization & 0x1f));
        primaries = (int *)&work->target.pairs[0];
    }
    color_scale_or_normalization = dovi_GetRgb2LmsByV8PrimsM33Fxp((const void *)(primaries),
                                       (void *)(&work->color_matrix.pairs[0]));
    color_scale_or_normalization = dovi_APPLY_CT2RIGHT((const void *)(&work->color_matrix.pairs[0]), (unsigned int)(color_scale_or_normalization),
                            (unsigned int)(*(unsigned int *)(context + 0x43d8)),
                            (void *)((long)color_state + 0x9a));
    color_state[0x2b] = (int)(short)color_scale_or_normalization;
    *(long *)(color_state + 0x84) = context + 0x43dc;
    color_state[0x86] = *(unsigned int *)(context + 0x43f0);
    dovi_MemCpyByte((void *)(color_state + 0x11), (const void *)(input_primaries), (int)(0x20));
    dovi_MemCpyByte((void *)(color_state + 0x2c), (const void *)(source), (int)(0x20));
    dovi_GetCtM33((unsigned int)(*(int *)(context + 0x43d8) << 0xb),
             (void *)(&work->temperature.pairs[0]), (unsigned int)(0x1a));
    dovi_GetXyz2LmsM33((void *)(&work->xyz.pairs[0]));
    color_scale_or_normalization = dovi_GetXyz2LmsM33((void *)(&work->xyz.pairs[0]));
    if ((control->primary_signal == signal_index) &&
        (*(int *)(context + 0x43b8) != *(int *)(context + 0x43bc))) {
    derive_intended_white_point:
        selected_context_or_cct = 0x65a0000000;
        if ((((metadata != 0) && ((input_flags >> 0xb & 1) != 0)) && ((metadata[1] >> 3 & 1) != 0)) &&
            (3 < (int)color_state[0x1a])) {
            selected_context_or_cct =
                dovi_GetIntendedWpCct((unsigned int)(metadata[0x6d]), (unsigned int)(metadata[0x6c]),
                                 (unsigned int)(metadata[0x6e]));
        }
        selected_context_or_cct = *(long *)(context + 0x1f490) + selected_context_or_cct;
        if (0xc34fffffff < selected_context_or_cct) {
            selected_context_or_cct = 0xc350000000;
        }
        if (selected_context_or_cct < 0x2710000001) {
            selected_context_or_cct = 0x2710000000;
        }
        dovi_Cct2Xy((unsigned long long)(selected_context_or_cct), (void *)(&work->white_xy));
        dovi_Xy2Xyz((const void *)(&work->white_xy), (void *)(&work->white_xyz.pairs[0]));
        if (*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) {
            signal_scale_or_quotient = (unsigned long)(unsigned int)(int)(short)color_scale_or_normalization;
            white_long = (int)(((long)(int)(unsigned int)work->white_xyz.pairs[0] *
                                (long)(int)work->xyz.pairs[0] +
                            (long)(int)(*(
                                unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) *
                                (long)(int)(*(unsigned int *)((unsigned char *)&work->xyz.pairs[0] +
                                                              4)) +
                            (long)(int)work->white_xyz.words[2] * (long)(int)work->xyz.pairs[1]) >>
                           (signal_scale_or_quotient & 0x3f));
            white_medium =
                (int)(((long)(int)(*(unsigned int *)((unsigned char *)&work->xyz.pairs[1] + 4)) *
                           (long)(int)(unsigned int)work->white_xyz.pairs[0] +
                       (long)(int)work->xyz.pairs[2] *
                           (long)(int)(*(
                               unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) +
                       (long)(int)(*(unsigned int *)((unsigned char *)&work->xyz.pairs[2] + 4)) *
                           (long)(int)work->white_xyz.words[2]) >>
                      (signal_scale_or_quotient & 0x3f));
            primary_coordinate_or_white_short =
                (int)(((long)(int)work->xyz.pairs[3] *
                           (long)(int)(unsigned int)work->white_xyz.pairs[0] +
                       (long)(int)(*(unsigned int *)((unsigned char *)&work->xyz.pairs[3] + 4)) *
                           (long)(int)(*(
                               unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) +
                       (long)work->xyz.coefficients[8] * (long)(int)work->white_xyz.words[2]) >>
                      (signal_scale_or_quotient & 0x3f));
            color_scale_or_normalization = 0x1a - *(int *)(context + 0x442c);
            adjusted_white_long =
                (int)((unsigned long)((long)white_long * (long)(int)work->temperature.pairs[0] +
                                      (long)white_medium *
                                          (long)(int)(*(unsigned int *)((unsigned char *)&work
                                                                            ->temperature.pairs[0] +
                                                                        4)) +
                                      (long)primary_coordinate_or_white_short * (long)(int)work->temperature.pairs[1]) >>
                      0x1a);
            adjusted_white_medium =
                (int)((unsigned long)((long)white_long *
                                          (long)(int)(*(unsigned int *)((unsigned char *)&work
                                                                            ->temperature.pairs[1] +
                                                                        4)) +
                                      (long)white_medium * (long)(int)work->temperature.pairs[2] +
                                      (long)primary_coordinate_or_white_short *
                                          (long)(int)(*(unsigned int *)((unsigned char *)&work
                                                                            ->temperature.pairs[2] +
                                                                        4))) >>
                      0x1a);
            *(int *)(context + 0x1f454) = adjusted_white_long;
            *(int *)(context + 0x1f458) = adjusted_white_medium;
            primary_coordinate_or_white_short =
                (int)((unsigned long)((long)white_long * (long)(int)work->temperature.pairs[3] +
                                      (long)white_medium *
                                          (long)(int)(*(unsigned int *)((unsigned char *)&work
                                                                            ->temperature.pairs[3] +
                                                                        4)) +
                                      (long)primary_coordinate_or_white_short * (long)(int)work->temperature.pairs[4]) >>
                      0x1a);
            *(int *)(context + 0x1f45c) = primary_coordinate_or_white_short;
            *(int *)(context + 0x4420) = adjusted_white_long >> (color_scale_or_normalization & 0x1f);
            *(int *)(context + 0x4424) = adjusted_white_medium >> (color_scale_or_normalization & 0x1f);
            *(int *)(context + 0x4428) = primary_coordinate_or_white_short >> (color_scale_or_normalization & 0x1f);
        }
        if (((*(unsigned int *)(context + 0x1f424) == 0) ||
             (normalization_primaries = (unsigned int *)(context + 0x1f424), *(int *)(context + 0x1f428) == 0)) &&
            (normalization_primaries = (unsigned int *)(context + 0x1f3e4), *(int *)(context + 0x1f3cc) < 0x80)) {
            normalization_primaries = (unsigned int *)(context + 0x1f404);
        }
        dovi_GetRgb2XyzM33ByPrims(
            (unsigned int)(*normalization_primaries), (unsigned int)(normalization_primaries[1]), (unsigned int)(normalization_primaries[2]),
            (unsigned int)(normalization_primaries[3]), (unsigned int)(normalization_primaries[4]), (unsigned int)(normalization_primaries[5]),
            (unsigned int)work->white_xy,
            (unsigned int)((*(unsigned int *)((unsigned char *)&work->white_xy + 4))),
            (void *)(&work->rgb.pairs[0]));
        dovi_GetRgb2XyzDiagByV8Prims((const void *)(normalization_primaries), (void *)(&work->diagonal.pairs[0]));
        (*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 0)) =
            (int)work->rgb.pairs[0];
        (*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) =
            (int)work->rgb.pairs[2];
        work->white_xyz.words[2] = work->rgb.coefficients[8];
        signal_scale_or_quotient = dovi_sdiv((unsigned long long)((long)(int)work->rgb.pairs[0] << 0x1a),
                      (unsigned int)work->diagonal.pairs[0]);
        work->white_xyz.pairs[0] = CONCAT44(
            (*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)), (int)signal_scale_or_quotient);
        signal_scale_or_quotient = dovi_sdiv(
            (unsigned long long)((long)(int)(*(
                                     unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] +
                                                     4))
                                 << 0x1a),
            (unsigned int)((*(unsigned int *)((unsigned char *)&work->diagonal.pairs[0] + 4))));
        (*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) = (unsigned int)signal_scale_or_quotient;
        signal_scale_or_quotient = dovi_sdiv((unsigned long long)((long)(int)work->white_xyz.words[2] << 0x1a),
                      (unsigned int)(work->diagonal.words[2]));
        work->white_xyz.words[2] = (unsigned int)signal_scale_or_quotient & 0xffffc000;
        primary_signal_or_white_red = (unsigned int)work->white_xyz.pairs[0] & 0xffffc000;
        white_green = (*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)) & 0xffffc000;
        work->white_xyz.pairs[0] =
            CONCAT44((*(unsigned int *)((unsigned char *)&work->white_xyz.pairs[0] + 4)),
                     (unsigned int)work->white_xyz.pairs[0]) &
            0xffffc000ffffc000;
        color_scale_or_normalization = primary_signal_or_white_red;
        if ((int)primary_signal_or_white_red <= (int)white_green) {
            color_scale_or_normalization = white_green;
        }
        if ((int)color_scale_or_normalization <= (int)work->white_xyz.words[2]) {
            color_scale_or_normalization = work->white_xyz.words[2];
        }
        signal_scale_or_quotient = dovi_sdiv(
            (unsigned long long)((long)(int)(*(
                                     unsigned int *)((unsigned char *)&work->rgb.pairs[1] + 4)) *
                                     (long)(int)primary_signal_or_white_red +
                                 (long)(int)work->rgb.pairs[2] * (long)(int)white_green +
                                 (long)(int)(*(
                                     unsigned int *)((unsigned char *)&work->rgb.pairs[2] + 4)) *
                                     (long)(int)work->white_xyz.words[2]),
            (unsigned int)(color_scale_or_normalization));
        color_scale_or_normalization = (int)(unsigned int)signal_scale_or_quotient >> 8;
        color_state[0x5d7] = (unsigned int)signal_scale_or_quotient;
    } else {
        selected_context_or_cct = context + signal_slot * 0x28;
        if (config[signal_slot * 0x48 + 0x38] == 0) {
            _printk(dovi_dm_assertion_format,
                    "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmCmmtFxp.c",
                    "ApplyL9L11", 0x523, "pSrcSig->WpExt");
            dump_stack();
        }
        *(int *)(selected_context_or_cct + 0x4420) = config[signal_slot * 0x48 + 0x39];
        *(int *)(selected_context_or_cct + 0x4424) = config[signal_slot * 0x48 + 0x3a];
        *(int *)(selected_context_or_cct + 0x4428) = config[signal_slot * 0x48 + 0x3b];
        primary_signal_or_white_red = control->primary_signal;
        if (primary_signal_or_white_red == signal_index)
            goto derive_intended_white_point;
        if (*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) {
            if (5 < primary_signal_or_white_red)
                dm_limit_trap();
            color_scale_or_normalization = *(unsigned int *)(context + (unsigned long)primary_signal_or_white_red * 0x1f78 + 0xe320);
        } else {
            color_scale_or_normalization = *(unsigned int *)(context + 0xe320);
        }
    }
    color_state[0x38] = color_scale_or_normalization;
    return input_flags;
}

static void consolidate_input_trims(unsigned int signal_index, int *config, unsigned int *flags,
                                    unsigned char *metadata, long context,
                                    unsigned int *input_state, unsigned int *active_area,
                                    int *trim_summary, struct dm_consolidation_workspace *work)
{
    struct dm_control *control = (void *)config;
    short high_anchor;
    unsigned short low_anchor;
    short middle_anchor;
    _Bool luma_trim_changed;
    _Bool bounds_or_repeat;
    _Bool trim_level_or_chroma_pending;
    _Bool different_luma_endpoints;
    _Bool interpolated_luma;
    unsigned int active_area_enabled;
    int interpolation_field_or_anchor;
    unsigned long interpolation_scratch;
    unsigned long chroma_constant;
    unsigned long trim_field_bytes;
    unsigned int target_trim_or_adjustment;
    long trim_value_offset;
    unsigned long field_offset_or_chroma_constant;
    unsigned int source_trim_or_luma_adjustment;
    int luma_slope_adjustment_or_anchor;
    unsigned int input_flags;
    unsigned int trim_count_or_value;
    unsigned long *source_trim;
    int luma_offset_adjustment;
    unsigned long *target_trim;
    _Bool second_pass;
    int luma_power_adjustment;
    int trim_enable_adjustment;
    unsigned int saturation_gain;
    long fields_remaining;
    unsigned long field_index_or_remaining_bytes;
    unsigned long remaining_table_bytes;
    unsigned int chroma_weight;
    unsigned int target_trim_or_luma_power;
    unsigned char *metadata_trim;
    unsigned int *color_state;
    const unsigned int *trim_field_indices;
    unsigned int source_component_or_luma_adjustment;
    interpolation_field_or_anchor = 1;
    input_flags = *flags;
    active_area_enabled = *active_area;
    if ((int)input_state[0x1a] < 4) {
        if (((metadata != 0) && ((input_flags >> 2 & 1) != 0)) && ((*metadata >> 2 & 1) != 0)) {
            metadata_trim = metadata + 0x84;
            color_state = input_state + 0x5d;
            if (color_state != 0)
                goto interpolate_metadata_trims;
        }
    apply_default_luma_trims:
        input_state[0x3c] = config[0x1e2];
        trim_count_or_value = config[0x1e3];
        input_state[0x3d] = trim_count_or_value;
        source_component_or_luma_adjustment = config[0x1e4];
        source_trim_or_luma_adjustment = trim_count_or_value * 2 - 0x1000;
        if (0x1fff < (int)source_trim_or_luma_adjustment) {
            source_trim_or_luma_adjustment = 0x2000;
        }
        input_state[0x3e] = source_component_or_luma_adjustment;
        trim_count_or_value = config[0x1e5];
        source_component_or_luma_adjustment = source_component_or_luma_adjustment * 2;
        if ((int)source_trim_or_luma_adjustment < 1) {
            source_trim_or_luma_adjustment = 0;
        }
        if (0xfff < (int)source_component_or_luma_adjustment) {
            source_component_or_luma_adjustment = 0x1000;
        }
        input_state[0x3f] = trim_count_or_value;
        target_trim_or_adjustment = config[0x1e8];
        target_trim_or_luma_power = trim_count_or_value * 2 - 0x1000;
        if ((int)source_component_or_luma_adjustment < -0xfff) {
            source_component_or_luma_adjustment = 0xfffff000;
        }
        input_state[0x42] = target_trim_or_adjustment;
        if (0x1fff < (int)target_trim_or_luma_power) {
            target_trim_or_luma_power = 0x2000;
        }
        saturation_gain = config[0x1e9];
        if ((int)target_trim_or_luma_power < 1) {
            target_trim_or_luma_power = 0;
        }
        input_state[0x43] = saturation_gain;
        input_state[0x44] = config[0x1ea];
        trim_count_or_value = config[0x1e2];
        input_state[0x3e] = source_component_or_luma_adjustment;
        input_state[0x3f] = target_trim_or_luma_power;
        input_state[0x3c] = trim_count_or_value;
        input_state[0x3d] = source_trim_or_luma_adjustment;
        source_trim_or_luma_adjustment = *(int *)(context + 0x43a4) + source_trim_or_luma_adjustment;
        input_state[0x3d] = source_trim_or_luma_adjustment;
        target_trim_or_luma_power = *trim_summary + target_trim_or_luma_power;
        input_state[0x3f] = target_trim_or_luma_power;
        saturation_gain = *(int *)(context + 0x43ac) + saturation_gain;
        trim_level_or_chroma_pending = 0;
        input_state[0x43] = saturation_gain;
        if (interpolation_field_or_anchor == 0) {
            chroma_weight = input_state[0x40];
            trim_count_or_value = input_state[0x41];
        } else {
        apply_default_chroma_trims:
            chroma_weight = config[0x1e6];
            input_state[0x40] = chroma_weight;
            trim_count_or_value = config[0x1e7];
            input_state[0x41] = trim_count_or_value;
            input_state[0x45] = config[0x1eb];
            input_state[0x46] = config[0x1ec];
            input_state[0x47] = config[0x1ed];
            input_state[0x48] = config[0x1ee];
            input_state[0x49] = config[0x1ef];
            input_state[0x4a] = config[0x1f0];
            input_state[0x4b] = config[0x1f1];
            input_state[0x4c] = config[0x1f2];
            input_state[0x4d] = config[499];
            input_state[0x4e] = config[500];
            input_state[0x4f] = config[0x1f5];
            input_state[0x50] = config[0x1f6];
            trim_count_or_value = *(int *)(context + 0x43a8) + trim_count_or_value;
        }
        if (0x1fff < (int)source_trim_or_luma_adjustment) {
            source_trim_or_luma_adjustment = 0x2000;
        }
        if ((int)source_trim_or_luma_adjustment < 1) {
            source_trim_or_luma_adjustment = 0;
        }
        if (0x1fff < (int)target_trim_or_luma_power) {
            target_trim_or_luma_power = 0x2000;
        }
        if ((int)target_trim_or_luma_power < 1) {
            target_trim_or_luma_power = 0;
        }
        if (0xfff < (int)chroma_weight) {
            chroma_weight = 0x1000;
        }
        input_state[0x3d] = source_trim_or_luma_adjustment;
        input_state[0x3e] = source_component_or_luma_adjustment;
        if ((int)chroma_weight < -0xfff) {
            chroma_weight = 0xfffff000;
        }
        if (0x17ff < (int)trim_count_or_value) {
            trim_count_or_value = 0x1800;
        }
        if ((int)trim_count_or_value < 0x801) {
            trim_count_or_value = 0x800;
        }
        if (0x7ff < (int)saturation_gain) {
            saturation_gain = 0x800;
        }
        input_state[0x3f] = target_trim_or_luma_power;
        input_state[0x40] = chroma_weight;
        if ((int)saturation_gain < -0x7ff) {
            saturation_gain = 0xfffff800;
        }
        input_state[0x41] = trim_count_or_value;
        input_state[0x43] = saturation_gain;
        if (!trim_level_or_chroma_pending) {
            input_state[0x42] = target_trim_or_adjustment - 0x1000;
        }
    } else {
        if (((metadata == 0) || ((input_flags >> 8 & 1) == 0)) || ((metadata[1] & 1) == 0))
            goto apply_default_luma_trims;
        metadata_trim = metadata + 0x110;
        color_state = input_state + 0x3c;
        if (color_state == 0)
            goto apply_default_luma_trims;
    interpolate_metadata_trims:
        trim_level_or_chroma_pending = fixed_signed32(fixed_load_le32(metadata_trim)) != 8;
        trim_field_bytes = 0x1c;
        if (trim_level_or_chroma_pending) {
            trim_field_bytes = 0x10;
        }
        trim_field_indices = dovi_level8_luma_trim;
        if (trim_level_or_chroma_pending) {
            trim_field_indices = dovi_level2_luma_trim;
        }
        trim_count_or_value = 7;
        if (trim_level_or_chroma_pending) {
            trim_count_or_value = 4;
        }
        work->color_matrix.pairs[0] = work->color_matrix.pairs[0] & 0xffffffff00000000;
        trim_level_or_chroma_pending = 1;
        work->xyz.coefficients[8] = 0;
        work->xyz.pairs[2] = 0;
        work->xyz.pairs[3] = 0;
        work->xyz.pairs[0] = 0;
        work->xyz.pairs[1] = 0;
        work->rgb.coefficients[8] = 0;
        work->rgb.pairs[2] = 0;
        work->rgb.pairs[3] = 0;
        work->rgb.pairs[0] = 0;
        work->rgb.pairs[1] = 0;
        work->white_xyz.pairs[0] = work->white_xyz.pairs[0] & 0xffffffff00000000;
        work->diagonal.pairs[0] = work->diagonal.pairs[0] & 0xffffffff00000000;
        work->source.trim = 0;
        work->target.trim = 0;
        work->weight = 0;
        work->temperature.pairs[3] = 0;
        work->temperature.pairs[4] = 0;
        work->temperature.pairs[1] = 0;
        work->temperature.pairs[2] = 0;
        work->temperature.pairs[0] = 0;
        dovi_dm_BuildLumaXInfo((int)(1), (unsigned long)(0), (long)config, (unsigned int)(input_flags),
                               (int)(0), (long)metadata, (long)input_state, (int *)metadata_trim,
                               (long)&work->rgb.pairs[0], (long)&work->xyz.pairs[0],
                               (unsigned int *)&work->color_matrix.pairs[0],
                               (int *)&work->white_xyz.pairs[0]);
        interpolation_field_or_anchor = dovi_dm_BuildInterpInfo(
            (long)config, (long)metadata, (int *)metadata_trim, (int *)&work->rgb.pairs[0],
            (long)&work->xyz.pairs[0], (int)work->color_matrix.pairs[0],
            (int)((unsigned int)work->white_xyz.pairs[0]), (unsigned int *)&work->diagonal.pairs[0],
            (long)&work->temperature.pairs[0], (long *)(&work->source.trim),
            (long *)(&work->target.trim));
        target_trim_or_luma_power = 0;
        source_component_or_luma_adjustment = 0;
        different_luma_endpoints = work->source.trim != work->target.trim;
        interpolated_luma = interpolation_field_or_anchor == 0;
        luma_trim_changed = different_luma_endpoints || interpolated_luma;
        source_trim_or_luma_adjustment = 0;
        if (!different_luma_endpoints && !interpolated_luma) {
            source_trim_or_luma_adjustment = trim_count_or_value;
        }
        field_index_or_remaining_bytes = (unsigned long)source_trim_or_luma_adjustment;
        interpolation_scratch = 0 /* unused caller-saved x1 */;
        chroma_constant = 1;
        source_trim = work->source.trim;
        target_trim = work->target.trim;
        second_pass = 0;
        do {
            if ((unsigned int)field_index_or_remaining_bytes < trim_count_or_value) {
                field_offset_or_chroma_constant = field_index_or_remaining_bytes * 4;
                fields_remaining = trim_count_or_value - field_index_or_remaining_bytes;
                remaining_table_bytes = trim_field_bytes + field_index_or_remaining_bytes * -4;
                field_index_or_remaining_bytes = trim_field_bytes + field_index_or_remaining_bytes * -4;
                do {
                    if ((((long)field_offset_or_chroma_constant < 0) || (trim_field_bytes < field_offset_or_chroma_constant)) ||
                        ((bounds_or_repeat = field_index_or_remaining_bytes < 4, field_index_or_remaining_bytes = field_index_or_remaining_bytes - 4,
                          bounds_or_repeat || ((trim_field_bytes < field_offset_or_chroma_constant ||
                                      (bounds_or_repeat = remaining_table_bytes < 4, remaining_table_bytes = remaining_table_bytes - 4, bounds_or_repeat)))))) {
                        dm_limit_trap();
                    }
                    interpolation_field_or_anchor = (int)trim_field_indices[field_offset_or_chroma_constant / sizeof(*trim_field_indices)];
                    if ((fixed_signed32(fixed_load_le32(metadata_trim)) == 2) && (interpolation_field_or_anchor < 3)) {
                        source_trim_or_luma_adjustment = source_component_or_luma_adjustment;
                        target_trim_or_adjustment = target_trim_or_luma_power;
                        if (interpolation_field_or_anchor == 1) {
                            source_component_or_luma_adjustment =
                                (*(unsigned short *)((unsigned char *)&work->temperature.pairs[0] +
                                                     4)) -
                                0x800;
                            source_trim_or_luma_adjustment =
                                (*(unsigned short *)((unsigned char *)&work->temperature.pairs[0] +
                                                     2)) +
                                0x800;
                            if (source_trim != &work->temperature.pairs[0]) {
                                source_trim_or_luma_adjustment =
                                    fixed_load_le16((const unsigned char *)source_trim + 2) + 0x800;
                                source_component_or_luma_adjustment =
                                    fixed_load_le16((const unsigned char *)source_trim + 4) - 0x800;
                            }
                            target_trim_or_luma_power = source_component_or_luma_adjustment;
                            target_trim_or_adjustment = source_trim_or_luma_adjustment;
                            if ((target_trim != source_trim) &&
                                (target_trim_or_luma_power =
                                     (*(unsigned short
                                            *)((unsigned char *)&work->temperature.pairs[0] + 4)) -
                                     0x800,
                                 target_trim_or_adjustment =
                                     (*(unsigned short
                                            *)((unsigned char *)&work->temperature.pairs[0] + 2)) +
                                     0x800,
                                 target_trim != &work->temperature.pairs[0])) {
                                target_trim_or_luma_power =
                                    fixed_load_le16((const unsigned char *)target_trim + 4) - 0x800;
                                target_trim_or_adjustment =
                                    fixed_load_le16((const unsigned char *)target_trim + 2) + 0x800;
                            }
                        }
                    } else {
                        trim_value_offset = (long)interpolation_field_or_anchor * 2;
                        source_trim_or_luma_adjustment =
                            (unsigned int)fixed_load_le16((const unsigned char *)source_trim + trim_value_offset);
                        target_trim_or_adjustment =
                            (unsigned int)fixed_load_le16((const unsigned char *)target_trim + trim_value_offset);
                    }
                    fields_remaining = fields_remaining + -1;
                    field_offset_or_chroma_constant = field_offset_or_chroma_constant + 4;
                    target_trim_or_adjustment = (target_trim_or_adjustment - source_trim_or_luma_adjustment) * (unsigned int)work->diagonal.pairs[0];
                    interpolation_scratch = (unsigned long)target_trim_or_adjustment;
                    color_state[interpolation_field_or_anchor] = source_trim_or_luma_adjustment + ((int)target_trim_or_adjustment >> 0xc);
                } while (fields_remaining != 0);
                field_index_or_remaining_bytes = (unsigned long)trim_count_or_value;
            }
            field_offset_or_chroma_constant = chroma_constant;
            if (trim_level_or_chroma_pending) {
                trim_level_or_chroma_pending = fixed_signed32(fixed_load_le32(metadata_trim)) != 8;
                trim_field_bytes = 0x38;
                if (trim_level_or_chroma_pending) {
                    trim_field_bytes = 8;
                }
                trim_field_indices = dovi_level8_chroma_trim;
                if (trim_level_or_chroma_pending) {
                    trim_field_indices = dovi_level2_chroma_trim;
                }
                trim_count_or_value = 0xe;
                if (trim_level_or_chroma_pending) {
                    trim_count_or_value = 2;
                }
                dovi_dm_BuildChromaXInfo((int)(1), (unsigned long)(interpolation_scratch), (long)config,
                                         (unsigned int)(input_flags), (int)(0), (long)metadata,
                                         (long)input_state, (int *)metadata_trim,
                                         (long)&work->rgb.pairs[0], (long)&work->xyz.pairs[0],
                                         (unsigned int *)&work->color_matrix.pairs[0],
                                         (unsigned int *)&work->white_xyz.pairs[0]);
                interpolation_field_or_anchor = dovi_dm_BuildInterpInfo(
                    (long)config, (long)metadata, (int *)metadata_trim, (int *)&work->rgb.pairs[0],
                    (long)&work->xyz.pairs[0], (int)work->color_matrix.pairs[0],
                    (int)((unsigned int)work->white_xyz.pairs[0]),
                    (unsigned int *)&work->diagonal.pairs[0], (long)&work->temperature.pairs[0],
                    (long *)(&work->source.trim), (long *)(&work->target.trim));
                field_offset_or_chroma_constant = 0;
                field_index_or_remaining_bytes = 0;
                source_trim = work->source.trim;
                target_trim = work->target.trim;
                if ((work->source.trim == work->target.trim) && (interpolation_field_or_anchor != 0))
                    break;
            }
            trim_level_or_chroma_pending = 0;
            bounds_or_repeat = !second_pass;
            interpolation_scratch = field_offset_or_chroma_constant;
            chroma_constant = field_offset_or_chroma_constant;
            second_pass = 1;
        } while (bounds_or_repeat);
        interpolation_field_or_anchor = (int)chroma_constant;
        if ((int)input_state[0x1a] < 4) {
            if (luma_trim_changed) {
                input_state[0x3d] = input_state[0x5e];
                input_state[0x3e] = input_state[0x5f];
                input_state[0x3f] = config[0x1de] + input_state[0x60];
                input_state[0x42] = config[0x1e1] + input_state[99] * 2;
                input_state[0x43] = config[0x1e9];
                input_state[0x44] = config[0x1ea];
            }
            if (interpolation_field_or_anchor == 0) {
                input_state[0x40] = config[0x1df] + input_state[0x61];
                input_state[0x41] = config[0x1e0] + input_state[0x62];
                input_state[0x45] = config[0x1eb];
                input_state[0x46] = config[0x1ec];
                input_state[0x47] = config[0x1ed];
                input_state[0x48] = config[0x1ee];
                input_state[0x49] = config[0x1ef];
                input_state[0x4a] = config[0x1f0];
                input_state[0x4b] = config[0x1f1];
                input_state[0x4c] = config[0x1f2];
                input_state[0x4d] = config[499];
                input_state[0x4e] = config[500];
                input_state[0x4f] = config[0x1f5];
                trim_count_or_value = config[0x1f6];
            store_last_chroma_trim:
                input_state[0x50] = trim_count_or_value;
            }
        } else {
            if (luma_trim_changed) {
                source_trim_or_luma_adjustment = input_state[0x3d];
                luma_slope_adjustment_or_anchor = config[0x1f8];
                trim_count_or_value = input_state[0x3e];
                source_component_or_luma_adjustment = input_state[0x3f];
                input_state[0x3d] = luma_slope_adjustment_or_anchor + source_trim_or_luma_adjustment;
                luma_offset_adjustment = config[0x1f9];
                input_state[0x3e] = luma_offset_adjustment + trim_count_or_value;
                luma_power_adjustment = config[0x1fa];
                input_state[0x3f] = luma_power_adjustment + source_component_or_luma_adjustment;
                input_state[0x42] = config[0x1fd] + input_state[0x42] * 2;
                input_state[0x43] = config[0x1fe] + input_state[0x43];
                input_state[0x44] = config[0x1ff] + input_state[0x44];
                trim_enable_adjustment = config[0x1f7];
                input_state[0x3e] = (luma_offset_adjustment + trim_count_or_value) * 2;
                input_state[0x3f] = (luma_power_adjustment + source_component_or_luma_adjustment) * 2 - 0x1000;
                input_state[0x3c] = trim_enable_adjustment + input_state[0x3c];
                input_state[0x3d] = (luma_slope_adjustment_or_anchor + source_trim_or_luma_adjustment) * 2 - 0x1000;
            }
            if (interpolation_field_or_anchor == 0) {
                input_state[0x40] = config[0x1fb] + input_state[0x40];
                input_state[0x41] = config[0x1fc] + input_state[0x41];
                input_state[0x45] = config[0x200] + input_state[0x45] * 2;
                input_state[0x46] = config[0x201] + input_state[0x46] * 2;
                input_state[0x47] = config[0x202] + input_state[0x47] * 2;
                input_state[0x48] = config[0x203] + input_state[0x48] * 2;
                input_state[0x49] = config[0x204] + input_state[0x49] * 2;
                input_state[0x4a] = config[0x205] + input_state[0x4a] * 2;
                input_state[0x4b] = config[0x206] + input_state[0x4b];
                input_state[0x4c] = config[0x207] + input_state[0x4c];
                input_state[0x4d] = config[0x208] + input_state[0x4d];
                input_state[0x4e] = config[0x209] + input_state[0x4e];
                input_state[0x4f] = config[0x20a] + input_state[0x4f];
                trim_count_or_value = config[0x20b] + input_state[0x50];
                goto store_last_chroma_trim;
            }
        }
        if (luma_trim_changed) {
            trim_count_or_value = input_state[0x3d];
            source_trim_or_luma_adjustment = input_state[0x3e];
            source_component_or_luma_adjustment = input_state[0x3f];
            if (0x1fff < (int)trim_count_or_value) {
                trim_count_or_value = 0x2000;
            }
            if ((int)trim_count_or_value < 1) {
                trim_count_or_value = 0;
            }
            if (0xfff < (int)source_trim_or_luma_adjustment) {
                source_trim_or_luma_adjustment = 0x1000;
            }
            if ((int)source_trim_or_luma_adjustment < -0xfff) {
                source_trim_or_luma_adjustment = 0xfffff000;
            }
            if (0x1fff < (int)source_component_or_luma_adjustment) {
                source_component_or_luma_adjustment = 0x2000;
            }
            if ((int)source_component_or_luma_adjustment < 1) {
                source_component_or_luma_adjustment = 0;
            }
            input_state[0x3d] = trim_count_or_value;
            input_state[0x3e] = source_trim_or_luma_adjustment;
            input_state[0x3f] = source_component_or_luma_adjustment;
            input_state[0x3d] = *(int *)(context + 0x43a4) + trim_count_or_value;
            input_state[0x3f] = *trim_summary + source_component_or_luma_adjustment;
            input_state[0x43] = input_state[0x43] + *(int *)(context + 0x43ac);
        }
        if (interpolation_field_or_anchor == 0) {
            trim_count_or_value = input_state[0x41] + *(int *)(context + 0x43a8);
        } else {
            trim_count_or_value = input_state[0x41];
        }
        source_trim_or_luma_adjustment = input_state[0x3d];
        source_component_or_luma_adjustment = input_state[0x3e];
        if (0x1fff < (int)source_trim_or_luma_adjustment) {
            source_trim_or_luma_adjustment = 0x2000;
        }
        if ((int)source_trim_or_luma_adjustment < 1) {
            source_trim_or_luma_adjustment = 0;
        }
        if (0xfff < (int)source_component_or_luma_adjustment) {
            source_component_or_luma_adjustment = 0x1000;
        }
        target_trim_or_luma_power = input_state[0x3f];
        target_trim_or_adjustment = input_state[0x40];
        if ((int)source_component_or_luma_adjustment < -0xfff) {
            source_component_or_luma_adjustment = 0xfffff000;
        }
        if (0x1fff < (int)target_trim_or_luma_power) {
            target_trim_or_luma_power = 0x2000;
        }
        input_state[0x3d] = source_trim_or_luma_adjustment;
        input_state[0x3e] = source_component_or_luma_adjustment;
        if ((int)target_trim_or_luma_power < 1) {
            target_trim_or_luma_power = 0;
        }
        if (0xfff < (int)target_trim_or_adjustment) {
            target_trim_or_adjustment = 0x1000;
        }
        if ((int)target_trim_or_adjustment < -0xfff) {
            target_trim_or_adjustment = 0xfffff000;
        }
        saturation_gain = input_state[0x43];
        if (0x17ff < (int)trim_count_or_value) {
            trim_count_or_value = 0x1800;
        }
        if ((int)trim_count_or_value < 0x801) {
            trim_count_or_value = 0x800;
        }
        input_state[0x3f] = target_trim_or_luma_power;
        input_state[0x40] = target_trim_or_adjustment;
        if (0x7ff < (int)saturation_gain) {
            saturation_gain = 0x800;
        }
        input_state[0x41] = trim_count_or_value;
        if ((int)saturation_gain < -0x7ff) {
            saturation_gain = 0xfffff800;
        }
        input_state[0x43] = saturation_gain;
        if (!different_luma_endpoints && !interpolated_luma)
            goto apply_default_luma_trims;
        target_trim_or_adjustment = input_state[0x42] - 0x1000;
        input_state[0x42] = target_trim_or_adjustment;
        if (interpolation_field_or_anchor != 0) {
            trim_level_or_chroma_pending = 1;
            input_state[0x3c] = config[0x1e2];
            goto apply_default_chroma_trims;
        }
    }
    trim_count_or_value = (unsigned int)(unsigned short)input_state[0xb];
    if (0xffe < trim_count_or_value) {
        trim_count_or_value = 0xfff;
    }
    if ((unsigned short)input_state[0xb] == 0) {
        trim_count_or_value = 1;
    }
    middle_anchor = (short)trim_count_or_value;
    interpolation_field_or_anchor = input_state[0x44] + (unsigned int)*(unsigned short *)((long)input_state + 0x2a);
    low_anchor = (unsigned short)input_state[10];
    if (trim_count_or_value <= (unsigned short)input_state[10]) {
        low_anchor = middle_anchor - 1;
    }
    luma_slope_adjustment_or_anchor = interpolation_field_or_anchor;
    if (0xfff < interpolation_field_or_anchor) {
        luma_slope_adjustment_or_anchor = 0x1000;
    }
    high_anchor = (short)luma_slope_adjustment_or_anchor;
    if (interpolation_field_or_anchor <= (int)trim_count_or_value) {
        high_anchor = middle_anchor + 1;
    }
    *(short *)(input_state + 0xb) = middle_anchor;
    trim_count_or_value = 1;
    *(unsigned short *)(input_state + 10) = low_anchor;
    *(short *)((long)input_state + 0x2a) = high_anchor;
    if ((metadata != 0) && ((input_flags >> 5 & 1) != 0)) {
        trim_count_or_value = (unsigned int)((*metadata & 0x20) == 0);
    }
    if (active_area_enabled != 0) {
        trim_count_or_value = 1;
    }
    input_state[100] = trim_count_or_value;
    if ((control->use_metadata == 0) || (*(int *)(context + 0x43b8) != 1)) {
        if (trim_count_or_value != 0) {
            if (-1 < (int)source_component_or_luma_adjustment) {
                source_component_or_luma_adjustment = 0;
            }
            input_state[0x3e] = source_component_or_luma_adjustment;
        }
    } else {
        input_state[100] = 0;
    }

    return;
}

void dovi_dm_ConsolidateInput(unsigned int signal_index, int *config, unsigned int *flags,
                              unsigned char *metadata, long context)
{
    _Alignas(16) struct dm_consolidation_workspace work;
    struct dm_control *control = (void *)config;
    int *trim_summary;
    unsigned int *active_area;
    unsigned int primary_signal_or_run_mode;
    int previous_mode;
    unsigned long signal_slot;
    long primary_context_address;
    unsigned int *input_state;
    int current_mode_or_metadata_version;
    unsigned int input_flags;
    int reloaded_previous_mode;
    unsigned int *tone_lut_last;
    long selected_input_address;
    unsigned int *color_state;
    signal_slot = (unsigned long)signal_index;

    trim_summary = (int *)(context + 0x43a0);
    previous_mode = *(int *)(context + 0x43b8);
    current_mode_or_metadata_version = *(int *)(context + 0x43bc);
    color_state = (unsigned int *)(context + 0x44e8);
    if ((previous_mode == current_mode_or_metadata_version) || (control->primary_signal != signal_index)) {
        input_state = (unsigned int *)(context + signal_slot * 0x1f78 + 0x44e8);
        if (control->use_metadata != 0)
            goto retain_metadata_flags;
    mask_disabled_metadata_flags:
        input_flags = 0xfffff2eb;
        if (control->primary_signal != signal_index || previous_mode != 1) {
            input_flags = 0xfffff7ff;
        }
        input_flags = input_flags & *flags;
        *flags = input_flags;
        current_mode_or_metadata_version = *(int *)(context + 0x43bc);
        reloaded_previous_mode = *(int *)(context + 0x43b8);
    } else {
        input_state = color_state;
        if (control->use_metadata == 0)
            goto mask_disabled_metadata_flags;
    retain_metadata_flags:
        input_flags = *flags;
        reloaded_previous_mode = previous_mode;
    }
    if ((reloaded_previous_mode == current_mode_or_metadata_version) || (control->primary_signal != signal_index)) {
        selected_input_address = context + signal_slot * 0x1f78 + 0x44e8;
        if (metadata != 0)
            goto select_metadata_fields;
    select_configured_fields:
        *(int *)(selected_input_address + 0xdc) = config[signal_slot * 2 + 0x224];
        current_mode_or_metadata_version = config[signal_slot * 2 + 0x225];
    } else {
        selected_input_address = context + 0x44e8;
        if (metadata == 0)
            goto select_configured_fields;
    select_metadata_fields:
        if (((input_flags >> 0x1e & 1) == 0) || ((metadata[3] >> 6 & 1) == 0))
            goto select_configured_fields;
        *(unsigned int *)(selected_input_address + 0xdc) = (unsigned int)metadata[0x6f];
        current_mode_or_metadata_version = metadata[0x70] + 2;
        if (2 < metadata[0x70]) {
            current_mode_or_metadata_version = 4;
        }
    }
    *(int *)(selected_input_address + 0x68) = current_mode_or_metadata_version;
    *(int *)(selected_input_address + 0xd4) = config[0x225];
    primary_signal_or_run_mode = control->primary_signal;
    if (primary_signal_or_run_mode == signal_index) {
        if (((metadata == 0) || (-1 < (int)input_flags)) ||
            (-1 < fixed_signed32(fixed_load_le32(metadata)))) {
            primary_signal_or_run_mode = config[signal_slot * 0x48 + 0x5d];
        } else {
            primary_signal_or_run_mode = (unsigned int)metadata[0x71];
        }
    } else {
        primary_context_address = context;
        if (*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) {
            if (5 < primary_signal_or_run_mode)
                dm_limit_trap();
            primary_context_address = context + (unsigned long)primary_signal_or_run_mode * 0x1f78;
        }
        primary_signal_or_run_mode = *(unsigned int *)(primary_context_address + 0x45c0);
    }
    *(unsigned int *)(selected_input_address + 0xd8) = primary_signal_or_run_mode;
    active_area = (unsigned int *)(context + signal_slot * sizeof(struct dm_active_area) +
                                   DM_CONTEXT_ACTIVE_AREA_OFFSET);
    consolidate_active_area(signal_index, config, input_flags, (const void *)metadata, context);
    if (input_state[0x36] == 2)
        return;
    tone_lut_last = (unsigned int *)(context + 0x1dfc0);
    if (0x7f < (int)input_state[0x37]) {
        *flags = *flags & 0xfffffdef;
    }
    consolidate_user_adjustments(signal_index, config, context);
    if ((*(int *)(context + 0x43b8) == *(int *)(context + 0x43bc)) ||
        (control->primary_signal != signal_index)) {
        color_state = (unsigned int *)(context + signal_slot * 0x1f78 + 0x44e8);
    }
    input_flags =
        consolidate_input_color(signal_index, config, flags, metadata, context, color_state, &work);
    dovi_prepare_tone_control(signal_index, config, flags, metadata, context, color_state, &work,
                              input_state, tone_lut_last, previous_mode, input_flags);
    consolidate_input_trims(signal_index, config, flags, metadata, context, input_state,
                            active_area, trim_summary, &work);
}
