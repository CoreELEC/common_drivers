/* Update tone and saturation curves from their control inputs. */
#include "fixed_access.h"
#include "tone_curve.h"
#include "tone_controls.h"
#include "tone_parameters.h"
#include "tone_curve_lut.h"
#include "tone_maps.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "byte_utils.h"

static int trim_limit(int value)
{
    if (value < -0x8000000)
        return -0x8000000;
    return value > 0x8000000 ? 0x8000000 : value;
}

void dovi_CreateToneCurve2pxx(struct tone_curve_source *source, struct tone_curve_display *display,
                              struct tone_curve_pair *param1, struct tone_curve_pair *param2,
                              struct tone_curve_pair *param3, int *slope_over_rolloff,
                              short *tone_with_padding, short *tone)
{
    int trim_slope = trim_limit(source->trim_slope);
    int trim_power = trim_limit(source->trim_power);
    int trim_offset = trim_limit(source->trim_offset);
    unsigned short mode = display->mode;
    int crush = source->crush, mid = source->mid, clip = source->clip;
    int source_min = source->min_pq, diagonal = source->diagonal_inches;
    int source_max = source->max_pq;
    int target_min, target_mid, target_max, source_mid, slope;
    if (mode == 0) {
        unsigned long long magnitude = dovi_uabs_ls(((long long)display->mid_bias * 0x10aab) >> 12);
        int weight = magnitude < 0x7fffffffull ? (int)magnitude : 0x7fffffff;
        clip = fixed_signed32((unsigned int)weight +
                              (unsigned int)dovi_smpy_ss(0x7fffffff - weight, clip));
    }
    if (mode <= 1) {
        dovi_CalcMappedParams2pxx(crush, mid, clip, display, source_min, source_max, diagonal,
                                  &target_min, &target_mid, &target_max, &source_mid, &slope);
        calc_curve_params(crush, source_mid, clip, target_min, target_mid, target_max, slope,
                          display, param1, param2, param3, slope_over_rolloff);
    } else if (mode == 3) {
        return;
    }
    if (tone) {
        generate_tone_curve_lut(0, 0x7fffffff, param1, param2, param3, *slope_over_rolloff, display,
                                trim_slope, trim_power, trim_offset, tone_with_padding, tone, 512,
                                0);
    } else {
        generate_tone_curve_lut(0, 0x7fffffff, param1, param2, param3, *slope_over_rolloff, display,
                                trim_slope, trim_power, trim_offset, tone_with_padding + 1,
                                (short *)0, 512, 0);
        tone_with_padding[0] = tone_with_padding[1];
        tone_with_padding[513] = tone_with_padding[512];
        tone_with_padding[514] = tone_with_padding[512];
    }
}

void create_tone_curve_3pxx(struct tone_curve_source *source, struct tone_curve_display *display,
                            struct tone_curve_pair *param1, struct tone_curve_pair *param2,
                            struct tone_curve_pair *param3, int *slope_over_rolloff, short *tone_i,
                            short *tone_s, short *saturation_i, short *saturation_s, int *ratio,
                            short lut_size, short clamp_output)
{
    int trim_slope = trim_limit(source->trim_slope);
    int trim_power = trim_limit(source->trim_power);
    int trim_offset = trim_limit(source->trim_offset);
    int maximum = source->clip, minimum = source->crush, midpoint;
    int target_min, target_mid, target_max, source_mid, slope, source_target_ratio, screen_ratio;
    if (maximum < 0x4108b6b6)
        maximum = 0x4108b6b6;
    if (minimum > 0x13316bf8)
        minimum = 0x13316bf8;
    source->clip = maximum;
    midpoint = minimum + 0x15e00000;
    if (midpoint < source->mid)
        midpoint = source->mid;
    source->crush = minimum;
    if (maximum - 0x80000 < midpoint)
        midpoint = maximum - 0x80000;
    source->mid = midpoint;
    if (display->mode == 1) {
        calc_mapped_params(maximum, source, display, &target_min, &target_mid, &target_max,
                           &source_mid, &slope, &source_target_ratio, &screen_ratio);
        calc_curve_params(source->crush, source_mid, source->clip, target_min, target_mid,
                          target_max, slope, display, param1, param2, param3, slope_over_rolloff);
        generate_tone_curve_lut(source->crush, source->clip, param1, param2, param3,
                                *slope_over_rolloff, display, trim_slope, trim_power, trim_offset,
                                tone_i, (short *)0, lut_size, clamp_output);
    } else {
        unsigned long long magnitude = dovi_uabs_ls(((long long)display->mid_bias * 0x10aab) >> 12);
        int weight = magnitude < 0x7fffffffull ? (int)magnitude : 0x7fffffff;
        int clip_weight = 0x7fffffff - weight;
        int product = dovi_smpy_ss(clip_weight, source->clip);
        maximum = fixed_signed32((unsigned int)weight + (unsigned int)product);
        calc_mapped_params(maximum, source, display, &target_min, &target_mid, &target_max,
                           &source_mid, &slope, &source_target_ratio, &screen_ratio);
        calc_curve_params(source->crush, source_mid, maximum, target_min, target_mid, target_max,
                          slope, display, param1, param2, param3, slope_over_rolloff);
        generate_tone_curve_lut(source->crush, maximum, param1, param2, param3, *slope_over_rolloff,
                                display, trim_slope, trim_power, trim_offset, tone_i, (short *)0,
                                lut_size, clamp_output);
    }
    if (ratio == (int *)0) {
        (void)tone_map_s(source->intensity_weight, source_target_ratio, display->mode, tone_s,
                         lut_size);
        (void)saturation_map_s(source->chroma_vector_weight, source_target_ratio, display->mode,
                               saturation_s, lut_size);
        (void)saturation_map_i(display->min_pq, display->max_pq, source->min_pq,
                               source->saturation_gain, tone_i, display->mode, saturation_i,
                               lut_size);
    } else {
        *ratio = source_target_ratio;
    }
}

static unsigned int rd32(const void *p, unsigned int at)
{
    return fixed_load_le32((const unsigned char *)p + at);
}

static unsigned short rd16(const void *p, unsigned int at)
{
    return (unsigned short)fixed_load_le16((const unsigned char *)p + at);
}

int create_tone_curve_changed(const void *tc, void *kernel_tmap, void *used)
{
    struct tone_curve_source source;
    struct tone_curve_display display;
    struct tone_curve_pair p1, p2, p3;
    int slope, result;
    unsigned int direction = rd32(tc, 56), selected = direction == 1u ? 28u : 0u;
    unsigned char *tmap = kernel_tmap;
    display.min_pq = fixed_signed32((unsigned int)rd16(tc, 28) << 19);
    display.max_pq = fixed_signed32((unsigned int)rd16(tc, 30) << 19);
    display.min_bias = fixed_signed32((unsigned int)fixed_signed16(rd16(tc, 104)) << 19);
    display.mid_bias = fixed_signed32((unsigned int)fixed_signed16(rd16(tc, 108)) << 19);
    display.max_bias = fixed_signed32((unsigned int)fixed_signed16(rd16(tc, 106)) << 19);
    display.contrast = fixed_signed32((unsigned int)fixed_signed16(rd16(tc, 110)) << 19);
    display.brightness = fixed_signed32((unsigned int)fixed_signed16(rd16(tc, 112)) << 19);
    display.diagonal_inches = rd16(tc, 32);
    display.bright_preserve = fixed_signed32(rd32(tc, 84));
    display.rolloff = fixed_signed32(rd32(tc, 64));
    display.rolloff_inverse = fixed_signed32(rd32(tc, 68));
    display.mode = (unsigned short)(direction == 1u || direction == 2u ? direction : 0u);
    source.crush = fixed_signed32((unsigned int)rd16(tc, selected + 20u) << 19);
    source.mid = fixed_signed32((unsigned int)rd16(tc, selected + 24u) << 19);
    source.clip = fixed_signed32((unsigned int)rd16(tc, selected + 22u) << 19);
    source.min_pq = fixed_signed32((unsigned int)rd16(tc, 0) << 19);
    source.max_pq = fixed_signed32((unsigned int)rd16(tc, 2) << 19);
    source.key_weight = fixed_signed32(rd32(tc, 72) << 4);
    source.trim_slope = fixed_signed32(rd32(tc, 120) << 16);
    source.trim_offset = fixed_signed32(rd32(tc, 124) << 16);
    source.trim_power = fixed_signed32(rd32(tc, 128) << 16);
    source.chroma_weight = 0;
    source.saturation_gain = fixed_signed32(rd32(tc, 136) << 16);
    source.diagonal_inches = rd16(tc, 4);
    source.intensity_weight = fixed_signed32(rd32(tc, 76) << 4);
    source.chroma_vector_weight = fixed_signed32(rd32(tc, 80) << 4);
    create_tone_curve_3pxx(&source, &display, &p1, &p2, &p3, &slope, (short *)(tmap + 4),
                           (short *)(tmap + 1028), (short *)(tmap + 2056), (short *)(tmap + 3080),
                           (int *)0, 512, (short)(direction == 2u ? 0 : 1));
    dovi_MemCpyByte(used, tc, 160);
    result =
        fixed_signed16(rd16(tmap, 4)) ==
                fixed_signed16(rd16(tmap, 4u + (unsigned int)(fixed_signed16(rd16(tmap, 0)) * 2)))
            ? 0x100000
            : 1;
    return result;
}

int dovi_CreateToneCurve(const void *tc, void *kernel_tmap, void *used)
{
    if (dovi_MemEqByte(used, tc, 160))
        return 0;
    return create_tone_curve_changed(tc, kernel_tmap, used);
}
