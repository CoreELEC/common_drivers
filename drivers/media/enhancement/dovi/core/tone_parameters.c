/* Derive fixed-point parameters before sampling the tone/saturation LUTs. */
#include "tone_curve.h"
#include "tone_parameters.h"
#include "numeric_tone.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"

void dovi_CopyDataFromTcCtrlToTC(unsigned int *parameters, long control)
{
    int value_at_0080;
    unsigned int value_at_0024;

    *parameters = *(unsigned int *)(control + 0x68);
    parameters[1] = (unsigned int)*(unsigned short *)(control + 0x28) << 0xe;
    parameters[2] = (unsigned int)*(unsigned short *)(control + 0x2c) << 0xe;
    parameters[3] = (unsigned int)*(unsigned short *)(control + 0x2a) << 0xe;
    parameters[4] = *(int *)(control + 0x10) << 0xe;
    parameters[5] = *(int *)(control + 0x14) << 0xe;
    parameters[0x10] = *(int *)(control + 0x7c) << 0xe;
    value_at_0080 = *(int *)(control + 0x80);
    parameters[0x3c] = 0;
    parameters[0x11] = value_at_0080 << 0xe;
    parameters[0x15] = *(int *)(control + 0xf4) << 0xe;
    parameters[0x16] = *(int *)(control + 0xf8) << 0xe;
    parameters[0x17] = *(int *)(control + 0xfc) << 0xe;
    parameters[0x1e] = *(int *)(control + 0x10c) << 0xe;
    parameters[0x1f] = *(int *)(control + 0x100) << 0xe;
    parameters[0x20] = *(int *)(control + 0x104) << 0xe;
    parameters[0x21] = (int)*(short *)(control + 0x2e)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x22] = (int)*(short *)(control + 0x30)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x23] = (int)*(short *)(control + 0x32)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x24] = (int)*(short *)(control + 0x34)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x25] = (int)*(short *)(control + 0x36)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x26] = (int)*(short *)(control + 0x38)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x27] = (int)*(short *)(control + 0x3a)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x28] = (int)*(short *)(control + 0x3c)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x29] = (int)*(short *)(control + 0x3e)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x40) & 0x1f);
    parameters[0x2a] = (int)*(short *)(control + 0x9a)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x2b] = (int)*(short *)(control + 0x9c)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x2c] = (int)*(short *)(control + 0x9e)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x2d] = (int)*(short *)(control + 0xa0)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x2e] = (int)*(short *)(control + 0xa2)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x2f] = (int)*(short *)(control + 0xa4)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x30] = (int)*(short *)(control + 0xa6)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x31] = (int)*(short *)(control + 0xa8)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x32] = (int)*(short *)(control + 0xaa)
                       << (unsigned long)(0x1aU - *(int *)(control + 0xac) & 0x1f);
    parameters[0x33] = (int)**(short **)(control + 0x210)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x34] = (int)*(short *)(*(long *)(control + 0x210) + 2)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x35] = (int)*(short *)(*(long *)(control + 0x210) + 4)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x36] = (int)*(short *)(*(long *)(control + 0x210) + 6)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x37] = (int)*(short *)(*(long *)(control + 0x210) + 8)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x38] = (int)*(short *)(*(long *)(control + 0x210) + 10)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x39] = (int)*(short *)(*(long *)(control + 0x210) + 0xc)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x3a] = (int)*(short *)(*(long *)(control + 0x210) + 0xe)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[0x3b] = (int)*(short *)(*(long *)(control + 0x210) + 0x10)
                       << (unsigned long)(0x1aU - *(int *)(control + 0x218) & 0x1f);
    parameters[6] = *(unsigned int *)(*(long *)(control + 0x1b0) + 0x20);
    parameters[7] = *(unsigned int *)(*(long *)(control + 0x1b0) + 8);
    parameters[8] = *(unsigned int *)(*(long *)(control + 0x1b0) + 0xc);
    value_at_0024 = *(unsigned int *)(*(long *)(control + 0x1b0) + 0x24);
    *(long *)(parameters + 0x48) = control + 0x220;
    parameters[10] = value_at_0024;
    return;
}

void dovi_curve_prepare_parameters(void *control, void *parameters)
{
    int source_low;
    unsigned int upper_interval;
    int lower_branch_work;
    int target_mid_work;
    int mapped_upper_bound;
    unsigned int tone_denominator;
    int cutoff_work;
    int upper_branch_work;
    int target_component_work;
    int tone_value;
    unsigned int target_cp;
    int mode_at_00d8;
    int curve_mode;
    unsigned long fixed_result;
    unsigned int target_intensity;
    unsigned int target_ct;
    unsigned long l_component_work;
    unsigned int source_upper_interval;
    int curve_delta_work;
    unsigned long m_linear_product;
    unsigned int denominator;
    unsigned long s_linear_product;
    unsigned int mapped_lower_interval;
    unsigned long linear_luminance;
    int mapped_low, mapped_mid, mapped_high;

    dovi_CopyDataFromTcCtrlToTC(parameters, (long)control);
    tone_value = *(int *)((long)control + 0x1658);
    curve_mode = *(int *)((long)control + 0xe4);
    mode_at_00d8 = *(int *)((long)control + 0xd8);
    if (0x3ffbfff < tone_value) {
        tone_value = 0x3ffc000;
    }
    curve_delta_work = *(int *)((long)control + 0x1654);
    lower_branch_work = *(int *)((long)control + 0x1694);
    if (tone_value < 0x4001) {
        tone_value = 0x4000;
    }
    target_mid_work = *(int *)((long)control + 0x165c);
    target_component_work = *(int *)((long)control + 0x1690);
    upper_branch_work = curve_delta_work;
    if (tone_value + -0x4000 <= curve_delta_work) {
        upper_branch_work = tone_value + -0x4000;
    }
    source_low = 0;
    if (-1 < curve_delta_work) {
        source_low = upper_branch_work;
    }
    curve_delta_work = target_mid_work;
    if (0x3ffffff < target_mid_work) {
        curve_delta_work = 0x4000000;
    }
    upper_branch_work = tone_value + 0x4000;
    if (tone_value + 0x4000 <= target_mid_work) {
        upper_branch_work = curve_delta_work;
    }
    if (curve_mode == 1) {
        target_mid_work = *(int *)((long)control + 0x1660);
        mapped_upper_bound = *(int *)((long)control + 0x1664);
        cutoff_work = dovi_CalcCuttoff(source_low, tone_value, upper_branch_work, target_mid_work, mapped_upper_bound);
        source_upper_interval = upper_branch_work - tone_value;
        tone_denominator = tone_value - source_low;
        mapped_lower_interval = source_upper_interval;
        if (mapped_upper_bound - cutoff_work <= (int)source_upper_interval) {
            mapped_lower_interval = mapped_upper_bound - cutoff_work;
        }
        curve_delta_work = source_low - tone_value;
        mapped_mid = tone_value;
        if ((int)(mapped_upper_bound - mapped_lower_interval) <= tone_value) {
            mapped_mid = mapped_upper_bound - mapped_lower_interval;
        }
        mapped_lower_interval = tone_denominator;
        if (cutoff_work - target_mid_work <= (int)tone_denominator) {
            mapped_lower_interval = cutoff_work - target_mid_work;
        }
        cutoff_work = tone_value;
        if (tone_value <= (int)(mapped_lower_interval + target_mid_work)) {
            cutoff_work = mapped_lower_interval + target_mid_work;
        }
        mapped_mid = (cutoff_work - tone_value) + mapped_mid;
        if (mapped_mid <= target_mid_work + 0x4000) {
            mapped_mid = target_mid_work + 0x4000;
        }
        if (mapped_upper_bound + -0x4000 <= mapped_mid) {
            mapped_mid = mapped_upper_bound + -0x4000;
        }
        mapped_low = mapped_mid + curve_delta_work;
        if (mapped_mid + curve_delta_work <= target_mid_work) {
            mapped_low = target_mid_work;
        }
        mapped_high = mapped_mid + source_upper_interval;
        if (mapped_upper_bound <= (int)(mapped_mid + source_upper_interval)) {
            mapped_high = mapped_upper_bound;
        }
        mapped_lower_interval = mapped_mid - mapped_low;
        denominator = mapped_high - mapped_mid;
    } else {
        source_upper_interval = upper_branch_work - tone_value;
        tone_denominator = tone_value - source_low;
        curve_delta_work = source_low - tone_value;
        mapped_lower_interval = tone_denominator;
        denominator = source_upper_interval;
        mapped_low = source_low;
        mapped_mid = tone_value;
        mapped_high = upper_branch_work;
    }
    upper_branch_work = dovi_CalcCuttoff(source_low, tone_value, upper_branch_work, target_component_work, lower_branch_work);
    upper_interval = source_upper_interval;
    if (lower_branch_work - upper_branch_work <= (int)source_upper_interval) {
        upper_interval = lower_branch_work - upper_branch_work;
    }
    target_mid_work = tone_value;
    if ((int)(lower_branch_work - upper_interval) <= tone_value) {
        target_mid_work = lower_branch_work - upper_interval;
    }
    if (upper_branch_work - target_component_work <= (int)tone_denominator) {
        tone_denominator = upper_branch_work - target_component_work;
    }
    upper_branch_work = tone_value;
    if (tone_value <= (int)(tone_denominator + target_component_work)) {
        upper_branch_work = tone_denominator + target_component_work;
    }
    target_mid_work = (upper_branch_work - tone_value) + target_mid_work;
    if (target_mid_work <= target_component_work + 0x4000) {
        target_mid_work = target_component_work + 0x4000;
    }
    if (lower_branch_work + -0x4000 <= target_mid_work) {
        target_mid_work = lower_branch_work + -0x4000;
    }
    tone_value = target_mid_work + curve_delta_work;
    if (target_mid_work + curve_delta_work <= target_component_work) {
        tone_value = target_component_work;
    }
    curve_delta_work = target_mid_work + source_upper_interval;
    if (lower_branch_work <= (int)(target_mid_work + source_upper_interval)) {
        curve_delta_work = lower_branch_work;
    }
    fixed_result = dovi_sdiv(((long)target_mid_work - (long)tone_value) * 0xc000000, mapped_lower_interval);
    lower_branch_work = (int)fixed_result;
    fixed_result = dovi_sdiv(((long)curve_delta_work - (long)target_mid_work) * 0xc000000, denominator);
    upper_branch_work = (int)fixed_result;
    fixed_result =
        dovi_sdiv((-(unsigned long)((unsigned int)(target_mid_work - tone_value) >> 0x1f) & 0xfc00000000000000) |
                      (unsigned long)(unsigned int)(target_mid_work - tone_value) << 0x1a,
                  mapped_lower_interval);
    fixed_result = (unsigned long)((long)(int)fixed_result * (long)(int)fixed_result) >> 0x1a;
    if ((long)lower_branch_work <= (long)fixed_result) {
        fixed_result = (long)lower_branch_work;
    }
    target_component_work = (*(int *)((long)control + 0x16c8) - mapped_mid) + 0x4000000 + target_mid_work;
    if (upper_branch_work <= lower_branch_work) {
        lower_branch_work = upper_branch_work;
    }
    *(int *)((long)control + 0x1698) = (int)fixed_result;
    if (target_component_work < 1) {
        target_component_work = 0;
    }
    if (target_component_work <= lower_branch_work) {
        lower_branch_work = target_component_work;
    }
    *(int *)((long)control + 0x169c) = lower_branch_work;
    if ((curve_mode != 1) && (0x3ffffff < upper_branch_work)) {
        upper_branch_work = 0x4000000;
    }
    fixed_result =
        dovi_sdiv((-(unsigned long)((unsigned int)(curve_delta_work - target_mid_work) >> 0x1f) & 0xfc00000000000000) |
                      (unsigned long)(unsigned int)(curve_delta_work - target_mid_work) << 0x1a,
                  denominator);
    fixed_result = (unsigned long)((long)(int)fixed_result * (long)(int)fixed_result) >> 0x1a;
    if ((long)upper_branch_work <= (long)fixed_result) {
        fixed_result = (long)upper_branch_work;
    }
    *(int *)((long)control + 0x16a0) = (int)fixed_result;
    lower_branch_work = mapped_high;
    if (0x3ffffff < mapped_high) {
        lower_branch_work = 0x4000000;
    }
    fixed_result = dovi_Pq2LX(lower_branch_work >> 8);
    linear_luminance = fixed_result >> 6;
    if ((long)fixed_result < 0x5000000) {
        linear_luminance = DOVI_CURVE_MIN_LINEAR_LUMINANCE;
    }
    l_component_work = (long)linear_luminance * (long)*(int *)((long)control + 0x16d4);
    m_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x16e0);
    s_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x16ec);
    fixed_result = 0;
    if (-1 < (long)l_component_work) {
        fixed_result = l_component_work >> 0x14 & 0xfffffffffc0;
    }
    fixed_result = dovi_L2PqX(fixed_result);
    lower_branch_work = (int)fixed_result;
    fixed_result = 0;
    if (-1 < (long)m_linear_product) {
        fixed_result = m_linear_product >> 0x14 & 0xfffffffffc0;
    }
    fixed_result = dovi_L2PqX(fixed_result);
    upper_branch_work = (int)fixed_result;
    fixed_result = 0;
    if (-1 < (long)s_linear_product) {
        fixed_result = s_linear_product >> 0x14 & 0xfffffffffc0;
    }
    fixed_result = dovi_L2PqX(fixed_result);
    target_component_work = (int)fixed_result;
    *(int *)((long)control + 0x16b0) =
        (int)((unsigned long)((long)*(int *)((long)control + 0x171c) * (long)lower_branch_work +
                              (long)*(int *)((long)control + 0x1720) * (long)upper_branch_work +
                              (long)*(int *)((long)control + 0x1724) * (long)target_component_work) >>
              0x12);
    *(int *)((long)control + 0x16b4) =
        (int)((unsigned long)((long)*(int *)((long)control + 0x1728) * (long)lower_branch_work +
                              (long)*(int *)((long)control + 0x172c) * (long)upper_branch_work +
                              (long)*(int *)((long)control + 0x1730) * (long)target_component_work) >>
              0x12);
    *(int *)((long)control + 0x16b8) =
        (int)((unsigned long)((long)*(int *)((long)control + 0x1734) * (long)lower_branch_work +
                              (long)*(int *)((long)control + 0x1738) * (long)upper_branch_work +
                              (long)*(int *)((long)control + 0x173c) * (long)target_component_work) >>
              0x12);
    if (curve_mode == 1) {
        lower_branch_work = curve_delta_work;
        if (0x3ffffff < curve_delta_work) {
            lower_branch_work = 0x4000000;
        }
        fixed_result = dovi_Pq2LX(lower_branch_work >> 8);
        linear_luminance = fixed_result >> 6;
        if ((long)fixed_result < 0x5000000) {
            linear_luminance = DOVI_CURVE_MIN_LINEAR_LUMINANCE;
        }
        l_component_work = (long)linear_luminance * (long)*(int *)((long)control + 0x16f8);
        m_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x1704);
        s_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x1710);
        fixed_result = 0;
        if (-1 < (long)l_component_work) {
            fixed_result = l_component_work >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        lower_branch_work = (int)fixed_result;
        fixed_result = 0;
        if (-1 < (long)m_linear_product) {
            fixed_result = m_linear_product >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        upper_branch_work = (int)fixed_result;
        fixed_result = 0;
        if (-1 < (long)s_linear_product) {
            fixed_result = s_linear_product >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        target_component_work = (int)fixed_result;
        target_intensity = (unsigned int)(((long)*(int *)((long)control + 0x171c) * (long)lower_branch_work +
                                 (long)*(int *)((long)control + 0x1720) * (long)upper_branch_work +
                                 (long)*(int *)((long)control + 0x1724) * (long)target_component_work) >>
                                0x12);
        target_ct = (unsigned int)(((long)*(int *)((long)control + 0x1728) * (long)lower_branch_work +
                                 (long)*(int *)((long)control + 0x172c) * (long)upper_branch_work +
                                 (long)*(int *)((long)control + 0x1730) * (long)target_component_work) >>
                                0x12);
        target_cp = (unsigned int)(((long)*(int *)((long)control + 0x1734) * (long)lower_branch_work +
                                 (long)*(int *)((long)control + 0x1738) * (long)upper_branch_work +
                                 (long)*(int *)((long)control + 0x173c) * (long)target_component_work) >>
                                0x12);
    } else {
        fixed_result = dovi_Pq2LX(*(int *)((long)control + 0x1694) >> 8);
        linear_luminance = fixed_result >> 6;
        if ((long)fixed_result < 0x5000000) {
            linear_luminance = DOVI_CURVE_MIN_LINEAR_LUMINANCE;
        }
        l_component_work = (long)linear_luminance * (long)*(int *)((long)control + 0x16f8);
        m_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x1704);
        s_linear_product = (long)linear_luminance * (long)*(int *)((long)control + 0x1710);
        fixed_result = 0;
        if (-1 < (long)l_component_work) {
            fixed_result = l_component_work >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        lower_branch_work = (int)fixed_result;
        fixed_result = 0;
        if (-1 < (long)m_linear_product) {
            fixed_result = m_linear_product >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        upper_branch_work = (int)fixed_result;
        fixed_result = 0;
        if (-1 < (long)s_linear_product) {
            fixed_result = s_linear_product >> 0x14 & 0xfffffffffc0;
        }
        fixed_result = dovi_L2PqX(fixed_result);
        target_component_work = (int)fixed_result;
        target_intensity =
            (unsigned int)((unsigned long)((long)*(int *)((long)control + 0x171c) * (long)lower_branch_work +
                                           (long)*(int *)((long)control + 0x1720) * (long)upper_branch_work +
                                           (long)*(int *)((long)control + 0x1724) * (long)target_component_work) >>
                           0x12);
        target_ct =
            (unsigned int)((unsigned long)((long)*(int *)((long)control + 0x1728) * (long)lower_branch_work +
                                           (long)*(int *)((long)control + 0x172c) * (long)upper_branch_work +
                                           (long)*(int *)((long)control + 0x1730) * (long)target_component_work) >>
                           0x12);
        target_cp =
            (unsigned int)((unsigned long)((long)*(int *)((long)control + 0x1734) * (long)lower_branch_work +
                                           (long)*(int *)((long)control + 0x1738) * (long)upper_branch_work +
                                           (long)*(int *)((long)control + 0x173c) * (long)target_component_work) >>
                           0x12);
    }
    *(unsigned int *)((long)control + 0x16bc) = target_intensity;
    *(unsigned int *)((long)control + 0x16c4) = target_cp;
    *(unsigned int *)((long)control + 0x16c0) = target_ct;
    *(int *)((long)control + 0x167c) = tone_value;
    *(int *)((long)control + 0x1658) = mapped_mid;
    tone_denominator = (unsigned int)((unsigned long)((long)*(int *)((long)control + 0x16b4) *
                                               (long)*(int *)((long)control + 0x16b4) +
                                           (long)*(int *)((long)control + 0x16b8) *
                                               (long)*(int *)((long)control + 0x16b8)) >>
                           0x1a);
    *(int *)((long)control + 0x1654) = mapped_low;
    *(int *)((long)control + 0x165c) = mapped_high;
    *(int *)((long)control + 0x1680) = target_mid_work;
    *(int *)((long)control + 0x1684) = curve_delta_work;
    *(unsigned int *)((long)control + 0x1764) = tone_denominator;
    if ((int)tone_denominator < 1) {
        tone_value = 0;
    } else {
        tone_value = dovi_ssqrt_s(tone_denominator >> 1);
        tone_value = tone_value >> 2;
    }
    *(int *)((long)control + 0x1764) = tone_value;
    if (mode_at_00d8 == 1) {
        tone_denominator = *(unsigned int *)((long)control + 0x16b0);
        if (*(int *)((long)control + 0x1668) != 0) {
            tone_value = dovi_HlgOotf_I((int)tone_denominator >> 10, parameters);
            tone_denominator = tone_value << 10;
        }
    } else {
        tone_denominator = *(unsigned int *)((long)control + 0x16b0);
        if (mode_at_00d8 != 2) {
            tone_value = dovi_HlgOotf_I((int)tone_denominator >> 10, parameters);
            tone_denominator = dovi_TMIcore(parameters, tone_value << 10);
        }
    }
    if ((int)tone_denominator < 0x4001) {
        tone_denominator = 0x4000;
    }
    fixed_result = dovi_sdiv((long)*(int *)((long)control + 0x16bc) << 0x1a, tone_denominator);
    tone_value = (int)fixed_result;
    if (curve_mode == 1) {
        curve_delta_work = (tone_value + -0x4000000) >> 1;
    } else {
        curve_delta_work = 0;
        if (tone_value < 0x4000000) {
            curve_delta_work = tone_value + -0x4000000;
        }
    }
    *(int *)((long)control + 0x1768) = curve_delta_work;
    if ((mode_at_00d8 == 2) || ((mode_at_00d8 == 1 && (*(int *)((long)control + 0x1668) == 0)))) {
        tone_value = 0x4000000;
    } else {
        tone_value = dovi_SMIcore(parameters, tone_denominator, *(unsigned int *)((long)control + 0x16b0), 0);
    }
    tone_denominator = (unsigned int)((unsigned long)((long)tone_value * (long)*(int *)((long)control + 0x1764)) >>
                           0x1a);
    fixed_result = (long)*(int *)((long)control + 0x16c0) * (long)*(int *)((long)control + 0x16c0) +
             (long)*(int *)((long)control + 0x16c4) * (long)*(int *)((long)control + 0x16c4);
    if ((int)tone_denominator < 0x4001) {
        tone_denominator = 0x4000;
    }
    if ((long)fixed_result < 0x10000000000000) {
        if ((long)fixed_result < 1) {
            fixed_result = (unsigned long)*(unsigned int *)((long)control + 0x176c);
        } else {
            fixed_result = dovi_ssqrt_s(fixed_result >> 0x15);
            *(int *)((long)control + 0x176c) = (int)fixed_result;
        }
        fixed_result = (-(fixed_result >> 0x1f & 1) & 0xffe0000000000000) | (fixed_result & 0xffffffff) << 0x15;
    } else {
        fixed_result = dovi_ssqrt_s(fixed_result >> 0x1b);
        l_component_work = fixed_result >> 2 & 0x3fffffff;
        fixed_result = l_component_work << 0x1a;
        *(int *)((long)control + 0x176c) = (int)l_component_work;
    }
    fixed_result = dovi_sdiv(fixed_result, tone_denominator);
    tone_value = (int)fixed_result;
    if (curve_mode == 1) {
        curve_mode = (tone_value + -0x4000000) >> 1;
    } else {
        curve_mode = tone_value + -0x4000000;
        if (0x4000000 < tone_value) {
            curve_mode = 0;
        }
    }
    tone_value = *(int *)((long)control + 0x1764);
    *(int *)((long)control + 0x176c) = curve_mode;
    if (tone_value < 0x4001) {
        tone_value = 0x4000;
    }
    *(int *)((long)control + 0x1764) = tone_value;
}
