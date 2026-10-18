/* Generate auxiliary tone and saturation LUTs around the primary curve. */
/* Integer widths, wrapping and shifts follow the native arithmetic. */
#include "fixed_access.h"
#include "tone_maps.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "pq_conversion_tables.h"

static int vector_map(int weight, int ratio, unsigned short mode, short *output, short count)
{
    unsigned int norm, shift, accumulator = 0;
    int step, sign = mode == 0 ? 1 : -1, i;
    dovi_fx_assert(1, " QFTMS must be <= 15");
    norm = dovi_unorm_s(weight);
    step = dovi_sshr_sss(weight, fixed_signed32(9u - norm));
    if (mode > 1) {
        for (i = 0; i < count; ++i)
            output[i] = 4095;
        return 0;
    }
    shift = norm + 33u;
    if (shift > 63u)
        shift = 63u;
    for (i = 0; i < count; ++i) {
        long long scaled = ((long long)fixed_signed32(accumulator) * ratio) >> (shift & 63u);
        int half = -0x40000000, value;
        accumulator += (unsigned int)step;
        dovi_fx_assert((int)(~(norm + 33u) >> 31), "sshr_slu() called with negative shift value");
        if (scaled <= 2147483647ll && scaled >= (-2147483647ll - 1))
            half = fixed_signed32((unsigned int)scaled + 1u) >> 1;
        value = fixed_signed32(4095u - (unsigned int)sign * (unsigned int)half);
        if (value > 8191)
            value = 8191;
        if (value < 0)
            value = 0;
        output[i] = (short)value;
    }
    return 0;
}

int tone_map_s(int weight, int ratio, unsigned short mode, short *output, short count)
{
    return vector_map(weight, ratio, mode, output, count);
}

int saturation_map_s(int weight, int ratio, unsigned short mode, short *output, short count)
{
    return vector_map(weight, ratio, mode, output, count);
}

int saturation_map_i(int minimum, int maximum, int source_minimum, int gain, short *tone,
                     unsigned short mode, short *output, short count)
{
    int shift = count == 256 ? 4 : 3, i;
    if (!mode) {
        int low = minimum >> 19, high = maximum >> 19;
        int delta = low - (source_minimum >> 19);
        unsigned int reciprocal =
            low < high ? 0x20000000u / (unsigned int)(high - low) : 0x20000000u;
        for (i = 0; i < count; ++i) {
            int tone_value = tone[i], grid = fixed_signed16((unsigned int)i << shift);
            long long value = ((long long)(tone_value - grid + 4096) * gain) >> 28;
            short result = 0;
            if (delta > 0) {
                long long distance = high - tone_value, correction;
                unsigned long long lookup, corrected;
                unsigned int index, linear;
                if (distance < 0)
                    distance = 0;
                lookup = (unsigned long long)(((long long)fixed_signed32(reciprocal) * distance +
                                               8192) >>
                                              14);
                index = (unsigned int)(lookup >> 6) & 1023u;
                linear = pq_depq1024Lut[index];
                if (index != 1023u) {
                    unsigned int difference = pq_depq1024Lut[index + 1u] - linear;
                    linear += (unsigned int)(fixed_signed64((unsigned long long)difference *
                                                            (lookup & 63u)) >>
                                             6);
                }
                corrected = (unsigned long long)((long long)delta * 3) * linear;
                correction = fixed_signed64(corrected * 0x2c6734cull + 0x2000000000000ull) >> 50;
                if (correction > 4095) {
                    output[i] = 0;
                    continue;
                }
                value = fixed_signed64(
                    ((unsigned long long)value * (4096ull - (unsigned long long)correction) +
                     2048ull) >>
                    12);
            }
            result = fixed_signed32((unsigned int)value) <= 8191
                         ? fixed_signed16((unsigned int)value)
                         : 8191;
            output[i] = result;
        }
    } else if (mode == 1) {
        for (i = 0; i < count; ++i) {
            int denominator = (int)fixed_signed16((unsigned int)i << shift) - tone[i] + 4096;
            int reciprocal = denominator ? 0x1000000 / denominator : 0;
            int value = fixed_signed32((unsigned int)(((long long)reciprocal * gain) >> 28));
            if (value < 0)
                value = 0;
            if (value > 8191)
                value = 8191;
            output[i] = (short)value;
        }
    } else {
        for (i = 0; i < count; ++i)
            output[i] = 4095;
    }
    return 0;
}
