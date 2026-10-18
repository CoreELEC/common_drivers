/* Shared matrix precision and narrowing. The caller selects its packing rule. */
#ifndef DOVI_MATRIX_SCALE_IMPL_H
#define DOVI_MATRIX_SCALE_IMPL_H
#include "fixed_access.h"

static inline unsigned long long matrix_read64(const unsigned char *p)
{
    unsigned long long value = 0;
    unsigned int i;
    for (i = 0; i < 8; ++i)
        value |= (unsigned long long)p[i] << (i * 8u);
    return value;
}

static inline unsigned long long matrix_asr64(unsigned long long value, unsigned int shift)
{
    shift &= 63u;
    return shift == 0 ? value : (value >> shift) | ((0ull - (value >> 63)) << (64u - shift));
}

static inline int matrix_less64(unsigned long long a, unsigned long long b)
{
    return (a ^ 0x8000000000000000ull) < (b ^ 0x8000000000000000ull);
}

static inline unsigned int matrix_precision(const void *input)
{
    const unsigned char *matrix = input;
    unsigned long long maximum = 0;
    unsigned int i, precision;
    for (i = 0; i < 9; ++i) {
        unsigned long long value = matrix_read64(matrix + i * 8u);
        if (value == 0x8000000000000000ull)
            value = 0x7fffffffffffffffull;
        else if (value >> 63)
            value = 0ull - value;
        if (value > maximum)
            maximum = value;
    }
    /* The signed comparison skips precision 63 for magnitudes above 2^62. */
    for (precision = 0; precision < 64; ++precision)
        if (!matrix_less64(1ull << precision, maximum))
            break;
    return precision;
}

enum matrix_packing { MATRIX_TRUNCATE, MATRIX_SATURATE };

static inline unsigned int matrix_scale(const void *input, unsigned int scale, void *destination,
                                        int rounded, enum matrix_packing packing)
{
    const unsigned char *matrix = input;
    unsigned char *output = destination;
    unsigned int precision = matrix_precision(input);
    unsigned int shift = precision > 15u ? precision - 15u : 0, i;
    unsigned long long bias = shift != 0 ? matrix_asr64((1ull << (shift & 63u)) + 1u, 1) : 0;
    for (i = 0; i < 9; ++i) {
        /* Reload after each store: output may overlap future input words. */
        unsigned long long value = matrix_read64(matrix + i * 8u);
        if (shift != 0) {
            if (rounded)
                value += value != 0 && (value >> 63) == 0 ? bias : 0ull - bias;
            value = matrix_asr64(value, shift);
        }
        if (packing == MATRIX_SATURATE) {
            if (!matrix_less64(value, 0x7fffull))
                value = 0x7fffull;
            if (!matrix_less64(0xffffffffffff8000ull, value))
                value = 0xffffffffffff8000ull;
        }
        output[i * 2u] = (unsigned char)value;
        output[i * 2u + 1u] = (unsigned char)(value >> 8);
    }
    return scale - shift;
}

/* Inversion computes every cofactor before output stores, so the output may
 * overlap the input. Native scale arguments are interpreted through low16 bits.
 * The reduction loop is shared; final rounded coefficient packing differs. */
static inline unsigned int matrix_clamp16(unsigned long long value)
{
    if (!fixed_less64(value, 0x7fffull))
        value = 0x7fffull;
    if (!fixed_less64(0xffffffffffff8000ull, value))
        value = 0xffffffffffff8000ull;
    return (unsigned int)value;
}

static inline unsigned int matrix_inverse(const void *input, void *output, unsigned int input_scale,
                                          unsigned int output_scale, enum matrix_packing packing)
{
    const unsigned char *input_bytes = input;
    unsigned char *output_bytes = output;
    unsigned long long coefficients[9], determinant;
    unsigned int temporary[9], row, i, positive = 0, negative = 0xffffffffu;
    unsigned int shift = fixed_sign16(input_scale) + fixed_sign16(output_scale) + 3u;
    unsigned int reduction = 0, previous = 0;
    for (i = 0; i < 9u; ++i)
        coefficients[i] = fixed_sign32(fixed_sign16(fixed_load_le16(input_bytes + i * 2u)));
    determinant =
        (coefficients[8] * coefficients[4] - coefficients[7] * coefficients[5]) * coefficients[0] +
        (coefficients[6] * coefficients[5] - coefficients[3] * coefficients[8]) * coefficients[1] +
        (coefficients[3] * coefficients[7] - coefficients[6] * coefficients[4]) * coefficients[2];
    if (determinant == 0)
        return 0xffffu;
    for (row = 0; row < 3u; ++row) {
        unsigned int first = ((row + 1u) % 3u) * 3u;
        unsigned int second = ((row + 2u) % 3u) * 3u;
        unsigned long long cofactors[3];
        cofactors[0] = coefficients[second + 2u] * coefficients[first + 1u] -
                       coefficients[second + 1u] * coefficients[first + 2u];
        cofactors[1] = coefficients[second] * coefficients[first + 2u] -
                       coefficients[first] * coefficients[second + 2u];
        cofactors[2] = coefficients[first] * coefficients[second + 1u] -
                       coefficients[second] * coefficients[first + 1u];
        for (i = 0; i < 3u; ++i) {
            unsigned long long quotient =
                fixed_raw_divide64(cofactors[i] << (shift & 63u), determinant);
            unsigned int value = (unsigned int)(((quotient >> 2) + 1u) >> 1);
            temporary[i * 3u + row] = value;
            if (!fixed_less32(value, positive))
                positive = value;
            if (!fixed_less32(negative, value))
                negative = value;
        }
    }
    if (!(fixed_less32(positive, 0x8000u) && fixed_less32(0xffff7fffu, negative))) {
        unsigned int top, old_negative;
        do {
            old_negative = negative;
            previous = reduction;
            top = positive >> 16;
            positive >>= 1;
            negative = fixed_asr32(negative, 1);
            ++reduction;
        } while (top != 0 || fixed_less32(old_negative, 0xffff0000u));
    }
    if (reduction == 0 || (previous & 0xffffu) > 0x7ffeu) {
        for (i = 0; i < 9u; ++i)
            fixed_store_le16(output_bytes + i * 2u, packing == MATRIX_SATURATE
                                                        ? matrix_clamp16(fixed_sign32(temporary[i]))
                                                        : temporary[i]);
    } else {
        unsigned int count = (reduction & 0xffffu) - 1u;
        for (i = 0; i < 9u; ++i) {
            unsigned int value = fixed_asr32(temporary[i], count) + 1u;
            unsigned int packed = value >> 1;
            if (packing == MATRIX_SATURATE) {
                if (!fixed_less32(value, 0x10000u))
                    packed = 0x7fffu;
                if (!fixed_less32(0xfffeffffu, value))
                    packed = 0xffff8000u;
            }
            fixed_store_le16(output_bytes + i * 2u, packed);
        }
    }
    return reduction;
}

#endif
