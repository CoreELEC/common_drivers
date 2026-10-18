/* Shared fixed-width primitives; public wrappers retain their result widths. */
#ifndef DOVI_FIXED_BITS_H
#define DOVI_FIXED_BITS_H

static inline unsigned long long fixed_sign32(unsigned int value)
{
    return (unsigned long long)value | ((0ull - (value >> 31)) << 32);
}

/* Shift counts retain AArch64 register masking, including a zero shift. */
static inline unsigned int fixed_asr32(unsigned int value, unsigned int shift)
{
    shift &= 31u;
    if (shift == 0)
        return value;
    return (value >> shift) | ((0u - (value >> 31)) << (32u - shift));
}

static inline unsigned long long fixed_asr64(unsigned long long value, unsigned int shift)
{
    shift &= 63u;
    if (shift == 0)
        return value;
    return (value >> shift) | ((0ull - (value >> 63)) << (64u - shift));
}

static inline int fixed_less32(unsigned int first, unsigned int second)
{
    return (first ^ 0x80000000u) < (second ^ 0x80000000u);
}

static inline int fixed_less64(unsigned long long first, unsigned long long second)
{
    return (first ^ 0x8000000000000000ull) < (second ^ 0x8000000000000000ull);
}

static inline unsigned int fixed_add(unsigned int a, unsigned int b)
{
    unsigned int sum = a + b;
    if ((~(a ^ b) & (a ^ sum) & 0x80000000u) != 0)
        return 0x7fffffffu + (a >> 31);
    return sum;
}

static inline unsigned int fixed_subtract(unsigned int a, unsigned int b)
{
    unsigned int difference = a - b;
    if (((a ^ b) & (a ^ difference) & 0x80000000u) != 0)
        return 0x7fffffffu + (a >> 31);
    return difference;
}

static inline unsigned int fixed_abs32(unsigned int value)
{
    if (value == 0x80000000u)
        return 0x7fffffffu;
    return value >> 31 ? 0u - value : value;
}

static inline unsigned long long fixed_abs64(unsigned long long value)
{
    if (value == 0x8000000000000000ull)
        return 0x7fffffffffffffffull;
    return value >> 63 ? 0ull - value : value;
}

static inline unsigned int fixed_normalize32(unsigned int value)
{
    unsigned int shifts = 0;
    if (value == 0)
        return 0;
    while ((value & 0x80000000u) == 0) {
        value <<= 1;
        ++shifts;
    }
    return shifts == 0 ? 0 : shifts - 1u;
}

static inline unsigned int fixed_normalize64(unsigned long long value)
{
    unsigned int shifts = 0;
    if (value == 0)
        return 0;
    while ((value & 0x8000000000000000ull) == 0) {
        value <<= 1;
        ++shifts;
    }
    return shifts == 0 ? 0 : shifts - 1u;
}

static inline unsigned long long fixed_multiply(unsigned int a, unsigned int b)
{
    return (fixed_sign32(a) * fixed_sign32(b)) >> 31;
}

/* Raw signed divide first wraps INT64_MIN/-1, then saturates to signed32.
 * An in-range negative quotient retains its complete sign-extended bits. */
static inline unsigned long long fixed_divide(unsigned long long numerator,
                                              unsigned int denominator)
{
    unsigned long long magnitude = numerator >> 63 ? 0ull - numerator : numerator;
    unsigned int divisor = denominator >> 31 ? 0u - denominator : denominator;
    unsigned long long quotient;
    if (divisor == 0)
        return 0;
    quotient = magnitude / divisor;
    if (((numerator >> 63) ^ (denominator >> 31)) != 0)
        quotient = 0ull - quotient;
    if (quotient != fixed_sign32((unsigned int)quotient))
        quotient = 0x7fffffffull + (quotient >> 63);
    return quotient;
}

static inline unsigned int fixed_normalize_lut32(unsigned int value)
{
    static const unsigned int lut[16] = {
        0u, 3u, 2u, 2u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u
    };
    unsigned int shifts = 0;
    if (!value)
        return 0;
    if (value < 0x10000u)
        shifts = 16;
    else
        value >>= 16;
    if (value < 0x100u)
        shifts += 8;
    else
        value >>= 8;
    if (value < 0x10u)
        shifts += 4;
    else
        value >>= 4;
    return lut[value] + shifts;
}

static inline unsigned int fixed_log_polynomial(unsigned int value)
{
    unsigned int result = (unsigned int)fixed_multiply(value, 0xfe64613fu);
    result = (unsigned int)fixed_multiply(value, result + 0x7a2972du);
    result = (unsigned int)fixed_multiply(value, result + 0xee6c8ee6u);
    result = (unsigned int)fixed_multiply(value, result + 0x1d275dc5u);
    result = (unsigned int)fixed_multiply(value, result + 0xd213873au);
    return (unsigned int)fixed_multiply(value, result + 0x5c518466u);
}

/* Normalization is0..30 and the accumulator is positive below2^63 for
 * every raw32 input. See numerics.md for the recorded coefficient bounds. */
static inline unsigned int fixed_sqrt_approximation(unsigned int value)
{
    unsigned int norm = fixed_normalize32(value), a = value << (norm & 31u);
    unsigned int square = (unsigned int)fixed_multiply(a, a);
    unsigned int cube = (unsigned int)fixed_multiply(square, a);
    unsigned int fourth = (unsigned int)fixed_multiply(cube, a);
    unsigned long long accumulator =
        (unsigned long long)a * 0x52aa9432u + fixed_sign32(square) * fixed_sign32(0xc6f3beebu) +
        fixed_sign32(cube) * 0x1f11c934u + fixed_sign32(fourth) * fixed_sign32(0xf88b70b4u) +
        0x0762247a00000000ull;
    unsigned long long result = fixed_asr64(accumulator, (norm >> 1) + 30u);
    if (result > 0x7fffffffull)
        result = 0x7fffffffull;
    return norm & 1u ? (unsigned int)fixed_multiply((unsigned int)result, 0x5a82799au)
                     : (unsigned int)result;
}

static inline unsigned int fixed_power_polynomial(unsigned int value)
{
    unsigned int p = (unsigned int)fixed_multiply(value, 0x29080u);
    p = (unsigned int)fixed_multiply(value, p + 0x164080u);
    p = (unsigned int)fixed_multiply(value, p + 0x9d8f80u);
    p = (unsigned int)fixed_multiply(value, p + 0x38d3180u);
    p = (unsigned int)fixed_multiply(value, p + 0xf5fe000u);
    p = (unsigned int)fixed_multiply(value, p + 0x2c5c8b80u);
    return p + 0x40000000u;
}

/* Raw AArch64 signed divide, including zero and INT64_MIN/-1. */
static inline unsigned long long fixed_raw_divide64(unsigned long long numerator,
                                                    unsigned long long denominator)
{
    unsigned long long a = numerator >> 63 ? 0ull - numerator : numerator;
    unsigned long long b = denominator >> 63 ? 0ull - denominator : denominator;
    unsigned long long q = b ? a / b : 0;
    return (numerator ^ denominator) >> 63 ? 0ull - q : q;
}

static inline unsigned int fixed_raw_divide32(unsigned int numerator, unsigned int denominator)
{
    return (unsigned int)fixed_raw_divide64(fixed_sign32(numerator), fixed_sign32(denominator));
}
#endif
