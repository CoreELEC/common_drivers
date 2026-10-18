/* Integer Taylor approximations with native scale and result handling. */
#include "fixed_access.h"
#include "fixed_transcendentals.h"
#include "fixed_arithmetic.h"

typedef unsigned int u32;
#include "fixed_transcendentals_tables.h"

static unsigned long long mul32(unsigned int a, unsigned int b)
{
    return fixed_sign32(a) * fixed_sign32(b);
}

static unsigned int integer_part(unsigned int value, unsigned int fraction)
{
    if ((fraction >> 31) != 0)
        return fraction;
    return fixed_asr32(value, fraction < 31u ? fraction : 31u);
}

static unsigned int logarithmic_product(unsigned int value, unsigned long long exponent)
{
    return (unsigned int)((fixed_sign32(dovi_fxLOG2Taylor(value, 26u) - 0x7c000000u) * exponent) >>
                          31);
}

static unsigned int power_scaled(unsigned int product, unsigned int bias)
{
    return dovi_sshr_sss(fixed_power_polynomial((product & 0x3ffffffu) << 5),
                         bias - fixed_asr32(product, 26));
}

static unsigned int correction(unsigned long long exponent, unsigned int bias)
{
    return dovi_sshr_sss(fixed_power_polynomial((unsigned int)exponent & 0x7fffffffu),
                         bias + ((unsigned int)(exponent >> 31) & 1u));
}

static unsigned long long pow_repeated_correction(unsigned long long value,
                                                  unsigned long long exponent,
                                                  unsigned int input_shift)
{
    unsigned long long e = fixed_asr64(exponent, 1);
    unsigned int p = power_scaled(logarithmic_product((unsigned int)(value >> input_shift), e), 0);
    unsigned int c = correction(e, 3u), i;
    unsigned long long v = fixed_asr64(mul32(c, p), 27);
    for (i = 1; i + 1u < input_shift; ++i)
        v = fixed_asr64(v * fixed_sign32(c), 27);
    v = fixed_asr64(v * fixed_sign32(c), 30);
    return fixed_asr64(v * v, 27);
}

static unsigned long long wide_square_shift(unsigned long long value, unsigned int shift)
{
    unsigned long long high, low;
    dovi_uint64_mul(value, value, &high, &low);
    return (low >> shift) | (high << (64u - shift));
}

void dovi_fx_assert(int test, const char *message)
{
    (void)test;
    (void)message;
}

int dovi_fxLOG2Taylor(unsigned int raw_value, unsigned int raw_scale_out)
{
    unsigned int norm, a, result, shift, integral;
    if (raw_value == 0u)
        return fixed_signed32(1u << (raw_scale_out & 31u));
    norm = dovi_unorm_s(raw_value);
    a = ((raw_value << (norm & 31u)) & 0x3fffffffu) << 1;
    result = fixed_log_polynomial(a);
    shift = 30u - raw_scale_out;
    result = (shift >> 31) != 0 ? shift : fixed_asr32(result + 0x8e8u, shift < 31u ? shift : 31u);
    integral = (raw_scale_out >> 31) != 0 ? 0 : (30u - norm) << (raw_scale_out & 31u);
    return fixed_signed32(result + integral);
}

int dovi_fxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in, unsigned int raw_scale_out)
{
    unsigned int value = raw_value, scale_in = raw_scale_in, scale_out = raw_scale_out;
    unsigned int integer = integer_part(value, scale_in);
    unsigned int result = fixed_power_polynomial((value << ((0u - scale_in) & 31u)) >> 1);
    return fixed_signed32(dovi_sshr_sss(result, 30u - scale_out - integer));
}

int dovi_powTaylor(unsigned int value, unsigned int exponent)
{
    return fixed_signed32(power_scaled(logarithmic_product(value, exponent), 0xffffffffu));
}

long long dovi_LfxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in)
{
    unsigned int value = raw_value, scale_in = raw_scale_in;
    unsigned int integer = integer_part(value, scale_in), shift = integer + 2u;
    unsigned long long result =
        fixed_sign32(fixed_power_polynomial((value << ((0u - scale_in) & 31u)) >> 1));
    if (shift != 0 && (shift >> 31) == 0)
        return fixed_signed64(result << (shift & 63u));
    return fixed_signed64(fixed_asr64(result, 0u - shift));
}

int dovi_expNTaylor(unsigned int raw_value, unsigned int raw_scale_out)
{
    unsigned int value = raw_value, scale_out = raw_scale_out;
    unsigned int p = (unsigned int)(mul32(value, 0x5c551d94u) >> 30);
    return fixed_signed32(dovi_sshr_sss(fixed_power_polynomial((p & 0x3fffffffu) << 1),
                                        30u - scale_out - fixed_asr32(p, 30)));
}

unsigned long long dovi__powInSTaylor(unsigned long long raw_value, unsigned int raw_scale)
{
    unsigned long long value = raw_value;
    unsigned int scale = raw_scale;
    unsigned int p = dovi_fxPOW2Taylor(
        logarithmic_product((unsigned int)(value >> (scale & 63u)), 0x14640000u), 26, 30);
    unsigned int c = dovi_fxPOW2Taylor(0x14640000u, 31, 27), count = scale - 2u;
    unsigned long long v = fixed_asr64(mul32(c, p), 27);
    do {
        v = (v * fixed_sign32(c)) >> 27;
        --count;
    } while (count != 0);
    return (v * fixed_sign32(c)) >> 26;
}

unsigned int dovi_ssqrt_s(unsigned int value)
{
    return fixed_sqrt_approximation(value);
}

unsigned long long dovi_log10Taylor(unsigned int value)
{
    return mul32(dovi_fxLOG2Taylor(value, 26u) - 0x7c000000u, 0x268826a1u) >> 31;
}

unsigned long long dovi_powIn4Taylor(unsigned long long value, unsigned int exponent)
{
    unsigned int p = power_scaled(logarithmic_product((unsigned int)(value >> 2), exponent), 0);
    unsigned int c = correction(exponent, 1u);
    unsigned int v = (unsigned int)(mul32(p, c) >> 29);
    return mul32(v, c) >> 30;
}

unsigned long long dovi_powInExp2Taylor(unsigned int value, unsigned long long exponent)
{
    unsigned int p = power_scaled(logarithmic_product(value, exponent >> 1), 0xffffffffu);
    return mul32(p, p) >> 31;
}

unsigned long long dovi_powIn4Exp2Taylor(unsigned long long value, unsigned long long exponent)
{
    unsigned long long e = fixed_asr64(exponent, 1);
    unsigned int p = power_scaled(logarithmic_product((unsigned int)(value >> 2), e), 0);
    unsigned int c = correction(e, 3u), v = (unsigned int)(mul32(p, c) >> 27);
    v = (unsigned int)(mul32(v, c) >> 30);
    return mul32(v, v) >> 27;
}

unsigned long long dovi_powIn4Exp4Taylor(unsigned long long value, unsigned long long exponent)
{
    unsigned long long e = fixed_asr64(exponent, 2);
    unsigned int p = power_scaled(logarithmic_product((unsigned int)(value >> 2), e), 0);
    unsigned int c = correction(e, 3u);
    unsigned long long v = fixed_asr64(mul32(c, p), 27);
    v = fixed_asr64(v * fixed_sign32(c), 30);
    v = fixed_asr64(v * v, 27);
    return fixed_asr64(v * v, 23);
}

unsigned long long dovi_powIn8Exp2Taylor(unsigned long long value, unsigned long long exponent)
{
    return pow_repeated_correction(value, exponent, 3);
}

unsigned long long dovi_powIn32Exp2Taylor(unsigned long long value, unsigned long long exponent)
{
    return pow_repeated_correction(value, exponent, 5);
}

unsigned long long dovi_powInsExpsTaylor(unsigned int value, unsigned int input_fraction,
                                         unsigned int exponent, unsigned int exponent_fraction)
{
    unsigned int logarithm, clipped, shift;
    unsigned long long product, p;
    if (value == 0)
        return 0;
    logarithm = dovi_fxLOG2Taylor(value, 26u) - (input_fraction << 26);
    product = fixed_asr64(mul32(logarithm, exponent), exponent_fraction);
    clipped = (unsigned int)product;
    if (!fixed_less64(product, 0x80000000ull))
        clipped = 0x7fffffffu;
    if (!fixed_less64(0xffffffff7fffffffull, product))
        clipped = 0x80000000u;
    shift = fixed_asr32(clipped, 26);
    p = fixed_sign32(fixed_power_polynomial((clipped & 0x3ffffffu) << 5));
    if ((clipped ^ 0x80000000u) > (0xfbffffffu ^ 0x80000000u))
        return p << ((shift + 2u) & 63u);
    return fixed_asr64(p, 0xfffffffeu - shift);
}

unsigned long long dovi_log10TaylorB(unsigned int value)
{
    return mul32(dovi_fxLOG2Taylor(value, 26u), 0x268826a1u) >> 31;
}

unsigned int dovi_expNTaylorIn2(unsigned int value)
{
    unsigned int p = (unsigned int)(mul32(value, 0x2e2a8ecau) >> 29);
    return dovi_sshr_sss(fixed_power_polynomial((p & 0x1fffffffu) << 2), 2u - fixed_asr32(p, 29));
}

unsigned int dovi_expNTaylorIn8(unsigned int value)
{
    unsigned int p = (unsigned int)(mul32(value, 0xb8aa3b2u) >> 27);
    return dovi_sshr_sss(fixed_power_polynomial((p & 0x7ffffffu) << 4), 11u - fixed_asr32(p, 27));
}

#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void angle_limit_trap(void)
{
    __debugbreak();
    __assume(0);
}

__declspec(noreturn) static void angle_bounds_trap(void)
{
    __debugbreak();
    __assume(0);
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn)) static void angle_limit_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #0x5512");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}

__attribute__((noreturn)) static void angle_bounds_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #1");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#else
#error "Provide nonreturning angle bounds traps."
#endif

unsigned int dovi_ATan2(unsigned int y, unsigned int x)
{
    unsigned int ay = dovi_uabs_ss(y), ax = dovi_uabs_ss(x);
    unsigned int ratio, index, next, angle;
    int vertical = ax < ay;
    ratio =
        (unsigned int)dovi_sdiv((unsigned long long)(vertical ? ax : ay) << 18, vertical ? ay : ax);
    if ((ratio ^ 0x80000000u) >= (0x3ffffu ^ 0x80000000u))
        ratio = 0x3ffffu;
    index = (ratio >> 11) & 0xffffu;
    next = ((ratio >> 11) & 0xffffu);
    next = (next | ((0u - ((next >> 15) & 1u)) << 16)) + 1u;
    if (index > 128u || next > 128u)
        angle_limit_trap();
    if (index > 128u)
        angle_bounds_trap();
    angle = arc_tan_lut[index] +
            (unsigned int)(mul32(ratio & 0x7ffu, arc_tan_lut[next] - arc_tan_lut[index]) >> 11);
    if (vertical) {
        if (y != 0 && (y >> 31) == 0)
            return x != 0 && (x >> 31) == 0 ? 0x6487fu - angle : 0x6487fu + angle;
        return x != 0 && (x >> 31) == 0 ? angle - 0x6487fu : 0u - 0x6487fu - angle;
    }
    if (x != 0 && (x >> 31) == 0)
        return (y >> 31) != 0 ? 0u - angle : angle;
    return y != 0 && (y >> 31) == 0 ? 0xc90feu - angle : angle - 0xc90feu;
}

unsigned long long dovi_powIn2Exp128Taylor(unsigned long long value, unsigned long long exponent)
{
    unsigned long long e = fixed_asr64(exponent, 7), v;
    unsigned int p = dovi_fxPOW2Taylor(logarithmic_product((unsigned int)(value >> 1), e), 26, 30);
    unsigned int c = dovi_fxPOW2Taylor((unsigned int)e, 31, 27), i;
    v = fixed_asr64(mul32(c, p), 27);
    v = fixed_asr64(v * v, 29);
    for (i = 0; i < 6u; ++i)
        v = fixed_asr64(v * v, 31);
    return v;
}

unsigned long long dovi_powIn16Exp8Taylor(unsigned long long value, unsigned long long exponent)
{
    unsigned long long e = fixed_asr64(exponent, 3), v;
    unsigned int p = dovi_fxPOW2Taylor(logarithmic_product((unsigned int)(value >> 4), e), 26, 30);
    unsigned int c = dovi_fxPOW2Taylor((unsigned int)e, 31, 27);
    v = fixed_asr64(mul32(c, p), 27);
    v = (v * fixed_sign32(c)) >> 27;
    v = (v * fixed_sign32(c)) >> 27;
    v *= fixed_sign32(c);
    if ((v >> 57) == 0) {
        v >>= 27;
        v = (v * v) >> 29;
        v = (v * v) >> 31;
        return (v * v) >> 31;
    }
    v = wide_square_shift(v >> 27, 29);
    v = wide_square_shift(v, 31);
    return wide_square_shift(v, 31);
}
