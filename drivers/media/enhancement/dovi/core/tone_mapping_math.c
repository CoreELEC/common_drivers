/* Integer tone and saturation primitives. */
#include "fixed_access.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "tone_mapping_math_tables.h"
typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long long u64;
#define ONE 0x4000000u

static u32 min32(u32 a, u32 b)
{
    return fixed_less32(a, b) ? a : b;
}

static u32 max32(u32 a, u32 b)
{
    return fixed_less32(a, b) ? b : a;
}

static u64 min64(u64 a, u64 b)
{
    return fixed_less64(a, b) ? a : b;
}

static u64 mul(u32 a, u32 b)
{
    return fixed_sign32(a) * fixed_sign32(b);
}

static u32 qmul(u32 a, u32 b)
{
    return (u32)(mul(a, b) >> 26);
}

static u32 div32(u64 a, u32 b)
{
    return (u32)dovi_sdiv(a, b);
}

static u32 clamp32(u32 x, u32 high)
{
    return max32(0, min32(x, high));
}

static u32 clamp_product(u64 x)
{
    if (x >> 63)
        return 0;
    return fixed_less64(x, 0x20000004000000ull) ? (u32)(x >> 26) : 0x8000000u;
}

static u64 product128_shift26(u64 a, u64 b)
{
    /* The original expands signed 128-bit products into 32-bit limbs. Only
     * the low 64 bits after shifting by 26 survive its EXTR instruction. */
    u64 high, low;
    dovi_int64_mul(a, b, &high, &low);
    return (low >> 26) | (high << 38);
}

void dovi_ApplyLocalMs(const void *context, void *records)
{
    const byte *c = (const byte *)context;
    byte *r = (byte *)records, *field;
    u32 kind = fixed_load_le32(r), index = fixed_load_le32(r + 8);
    /* Convert the two's-complement byte displacement without signed overflow. */
    u64 displacement = fixed_sign32(index) * (kind == 2 ? 14u : 42u) + 24u;
    long long offset =
        displacement >> 63 ? -1 - (long long)(~displacement) : (long long)displacement;
    field = r + offset;
    if ((fixed_load_le16(field) & 0x1fffu) != 0x1fffu)
        return;
    fixed_store_le16(field, kind == 2 ? fixed_load_le32(c + 0x74c) - fixed_load_le32(c + 0x784)
                                      : fixed_load_le32(c + 0x7a0) - fixed_load_le32(c + 0x7f4));
}

#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void hlg_trap(void)
{
    __debugbreak();
    __assume(0);
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn)) static void hlg_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #1");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#else
#error "Provide the original nonreturning HLG interpolation trap."
#endif

void dovi_DeriveHlgEotfParam(void *output, u32 minimum, u32 maximum)
{
    byte *out = (byte *)output;
    u32 logged = dovi_fxLOG2Taylor(div32((u64)maximum << 9, 1000), 26), gamma, reciprocal, start;
    u64 exponent = fixed_sign32(logged) - 0x6c000000ull, powered, accumulator = 0, step;
    u32 i, coordinate, index, shifted, next, value;
    if (maximum + 0xf9bfffffu <= 0x18fffffeu)
        gamma = (u32)((fixed_asr64(exponent * 0x268826a1ull, 31) * 0x1ae1u) >> 26) + 0x4cccu;
    else
        gamma = (u32)((dovi_powInsExpsTaylor(0x11c6a7efu, 28, (u32)exponent, 26) * 0x4cccu) >> 32);
    reciprocal = div32(0x10000000u, gamma & 65535u);
    powered = dovi_powInsExpsTaylor(div32((u64)minimum << 30, maximum >> 1), 31, reciprocal, 14);
    start = (u32)dovi_ssqrt_s((u32)(powered >> 1) * 3u) >> 13;
    step = 0x4000000ull - (u64)(start << 8);
    for (i = 0; i < 256; ++i) {
        coordinate = start + (u32)(accumulator >> 16);
        index = coordinate >> 10;
        shifted = index << 16;
        if (fixed_less32(0x118ffffu, shifted))
            value = 0x6d640bau;
        else {
            if (((index >> 1) & 0x7fffu) > 140u)
                hlg_trap();
            next = fixed_asr32(shifted, 16) + 1u;
            if ((fixed_sign32(next) << 2) > 0x468u || (index & 65535u) > 281u)
                hlg_trap();
            value = hlg_inverse_oetf[index & 65535u];
            value +=
                (u32)(((u64)(hlg_inverse_oetf[next] - value) * ((coordinate >> 2) & 255u)) >> 8);
        }
        accumulator += step;
        fixed_store_le32(out + 0x30 + i * 4u, (u32)(((u64)value * maximum) >> 26));
    }
}

static u32 hermite(u32 position, u32 span, u32 left, u32 right, u32 slope_left, u32 slope_right)
{
    u32 t = div32(fixed_sign32(position) << 26, span), t2 = qmul(t, t), t3 = qmul(t2, t);
    u64 scaled_span = fixed_sign32(span) << 6;
    u32 tangent_left = (u32)fixed_asr64(scaled_span * fixed_sign32(t - 2u * t2 + t3), 32);
    u32 tangent_right = (u32)fixed_asr64(scaled_span * fixed_sign32(t3 - t2), 32);
    return qmul(left, 2u * t3 - 3u * t2 + ONE) + qmul(right, 3u * t2 - 2u * t3) +
           qmul(tangent_left, slope_left) + qmul(tangent_right, slope_right);
}

unsigned int dovi_TMIcore(const void *config, u32 value)
{
    const byte *p = (const byte *)config;
    u32 left = fixed_load_le32(p + 4), middle = fixed_load_le32(p + 8),
        right = fixed_load_le32(p + 12), sum = 0, low = 0, high = ONE;
    u32 span, scale, ratio, base, exponent, absolute, unit, shift, result;
    u64 displaced, numerator, denominator, powered = 0;
    if (!fixed_less32(fixed_load_le32(p), 4)) {
        low = fixed_load_le32(p + 0x40);
        high = fixed_load_le32(p + 0x44);
    }
    if (fixed_less32(left, value) && !fixed_less32(middle, value))
        sum = hermite(value - left, middle - left, fixed_load_le32(p + 0x2c),
                      fixed_load_le32(p + 0x30), fixed_load_le32(p + 0x48),
                      fixed_load_le32(p + 0x4c));
    if (fixed_less32(middle, value) && fixed_less32(value, right))
        sum += hermite(value - middle, right - middle, fixed_load_le32(p + 0x30),
                       fixed_load_le32(p + 0x34), fixed_load_le32(p + 0x4c),
                       fixed_load_le32(p + 0x50));
    if (!fixed_less32(left, value))
        sum += fixed_load_le32(p + 0x2c) + qmul(fixed_load_le32(p + 0x48), value - left);
    if (!fixed_less32(value, right))
        sum += fixed_load_le32(p + 0x34) + qmul(fixed_load_le32(p + 0x50), value - right);
    span = high - low;
    displaced = fixed_sign32(sum) - fixed_sign32(low);
    numerator = dovi_uabs_ls(fixed_asr64(fixed_sign32(span), 14)) * 0x7fffffffull;
    denominator = dovi_uabs_ls(fixed_asr64(displaced, 14));
    scale = clamp32(30u - dovi_unorm_s(div32(numerator, (u32)denominator)), 26);
    scale = max32(scale, 18);
    ratio = div32(displaced << (scale & 63u), span);
    shift = 26u - scale;
    base = fixed_asr32(fixed_load_le32(p + 0x58), shift) + qmul(fixed_load_le32(p + 0x54), ratio);
    if (base == 0x80000000u)
        base = 0x80000001u;
    exponent = min32(fixed_asr32(fixed_load_le32(p + 0x5c), shift), (2u << (scale & 31u)) - 1u);
    absolute = base >> 31 ? 0u - base : base;
    unit = 1u << (scale & 31u);
    shift = 31u - scale;
    if (absolute < unit) {
        u32 x = absolute << ((~scale) & 31u);
        if (fixed_less32(exponent, unit))
            result = dovi_powTaylor(x, exponent << (shift & 31u));
        else
            result = (u32)dovi_powInExp2Taylor(x, (u64)exponent << (shift & 63u));
        powered = fixed_sign32(fixed_asr32(result, shift));
    } else if (absolute < (4u << (scale & 31u))) {
        u64 x = (u64)absolute << (shift & 63u);
        if (fixed_less32(exponent, unit)) {
            result = (u32)dovi_powIn4Taylor(x, exponent << (shift & 31u));
            powered = fixed_sign32(fixed_asr32(result, 29u - scale));
        } else {
            result = (u32)dovi_powIn4Exp2Taylor(x, (u64)exponent << (shift & 63u));
            powered = fixed_asr64(fixed_sign32(result), 27u - scale);
        }
    } else if (absolute < (8u << (scale & 31u))) {
        powered = dovi_powIn8Exp2Taylor((u64)absolute << (shift & 63u), fixed_sign32(exponent)
                                                                            << (shift & 63u));
        powered = fixed_asr64(powered, 27u - scale);
    } else
        powered = fixed_asr64(dovi_powInsExpsTaylor(absolute, scale, exponent, scale), 32u - scale);
    if (base == 0)
        powered = 0;
    else if (base >> 31)
        powered = 0ull - powered;
    return clamp32(low + (u32)fixed_asr64(powered * fixed_sign32(span), scale), ONE);
}

unsigned int dovi_TMScore(const void *config, u32 value, void *sqrt_output, u32 mode)
{
    const byte *p = (const byte *)config;
    u32 fraction = mode ? 29u : 31u, root = 0, ratio, factor;
    u64 cube, term, coefficient, limited;
    if (!fixed_less32(value, 1))
        root = fixed_asr32((u32)dovi_ssqrt_s(value << ((fraction - 26u) & 31u)), mode != 0);
    if (sqrt_output)
        fixed_store_le32((byte *)sqrt_output, root);
    ratio = div32(fixed_sign32(root) << ((52u - fraction) & 63u), fixed_load_le32(p + 0x114));
    cube = product128_shift26(mul(ratio, ratio) >> 26, fixed_sign32(ratio));
    coefficient = fixed_sign32(fixed_load_le32(p + 0x118));
    term = product128_shift26(cube, coefficient) + ONE;
    /* Preserve this conditional even when coefficient+ONE exceeds ONE. */
    limited = fixed_less64(term, coefficient + ONE) ? coefficient + ONE : min64(term, ONE);
    factor = (u32)fixed_asr64(mul(fixed_load_le32(p + 0x7c), root) << 2, fraction) + ONE;
    return clamp32(qmul((u32)limited, factor), 0x8000000u);
}

unsigned int dovi_SMIcore(const void *config, u32 value, u32 reference, u32 mode)
{
    const byte *p = (const byte *)config;
    u32 pivot = fixed_load_le32(p + 0x40), delta = value - pivot,
        den = pivot - fixed_load_le32(p + 4), ratio, a, b, factor;
    u64 weight, curve, inverse, multiplier, term;
    ratio = div32(fixed_less32(delta, 0x4000u) ? 0x10000000000ull : (u64)delta << 26,
                  max32(den, 0x4000));
    if (mode) {
        factor = ONE;
        weight = ONE;
    } else {
        a = min32(ratio, ONE) - ONE;
        b = qmul(a, a);
        factor = (fixed_less32(ratio, ONE) ? 0u - b : b) + ONE;
        if (!fixed_less32(0x64000u, reference))
            weight = 0;
        else {
            weight = ONE;
            if ((reference >> 16) <= 0xe0u) {
                a = ONE - div32((u64)(reference - 0x64000u) << 12, 0x36b);
                weight = fixed_asr64(0x400000000000000ull - ((mul(a, a) >> 26) << 32), 32);
            }
        }
    }
    a = max32(value, 0x4000) - ONE;
    b = max32(reference, 0x4000) - ONE;
    curve = fixed_asr64((mul(a, a) >> 26) * fixed_sign32(a), 26) + ONE;
    den = (u32)(((mul(b, b) >> 26) * fixed_sign32(b)) >> 26) + ONE;
    ratio = div32(curve << 26, den);
    if (ratio == 0x7fffffffu) {
        ratio = div32(curve << 22, den);
        multiplier = fixed_sign32(ratio) << 4;
        inverse = fixed_sign32(fixed_asr32(div32(0x10000000000000ull, ratio), 4));
    } else {
        multiplier = fixed_sign32(ratio);
        inverse = fixed_sign32(div32(0x10000000000000ull, ratio));
    }
    if (!fixed_less64(inverse, weight))
        weight = min64(inverse, ONE);
    term = fixed_asr64(weight * multiplier, 26) *
           fixed_asr64(mul(fixed_load_le32(p + 0x80), factor), 26);
    return clamp_product(term);
}

unsigned int dovi_SMScore(const void *config, const void *sqrt_input, u32 value, u32 clamp_mode,
                          u32 mode)
{
    const byte *p = (const byte *)config;
    u32 fraction = mode ? 29u : 31u, root = 0, ratio, factor, coefficient, limited;
    u64 cube, product, term;
    if (sqrt_input)
        root = fixed_load_le32((const byte *)sqrt_input);
    else if (!fixed_less32(value, 1))
        root = (u32)dovi_ssqrt_s(value << ((fraction - 26u) & 31u));
    ratio = div32(fixed_sign32(root) << ((52u - fraction) & 63u), fixed_load_le32(p + 0x114));
    cube = product128_shift26(mul(ratio, ratio) >> 26, fixed_sign32(ratio));
    coefficient = fixed_load_le32(p + 0x11c);
    product = cube * fixed_sign32(coefficient);
    term = fixed_asr64(product, 26) + ONE;
    factor = (u32)fixed_asr64(mul(root, fixed_load_le32(p + 0x7c)) * 3u, fraction) + ONE;
    if (clamp_mode == 1)
        return clamp_product(fixed_sign32(factor) * term);
    /* The original comparison uses the unshifted product, not term. */
    limited = fixed_less64(product, ONE) ? (u32)term : ONE;
    if (fixed_less64(term, fixed_sign32(coefficient + ONE)))
        limited = coefficient + ONE;
    return clamp32(qmul(factor, limited), 0x8000000u);
}

unsigned int dovi_CalcCuttoff(u32 low, u32 value, u32 high, u32 target_low, u32 target_high)
{
    u32 center = target_low + 0x666666u, left = min32(target_low, low),
        right = max32(target_high, high);
    u32 upper = max32(center, target_high - 0x666666u),
        lower = min32(center, target_low + 0x147ae1u);
    u32 span_left = center - left, span_right = right - center, slope_left, slope_right,
        slope_middle, tmp, den;
    u64 square;
    slope_left =
        max32(0, div32((fixed_sign32(center) - fixed_sign32(lower)) * 0xc000000u, span_left));
    slope_right =
        max32(0, div32((fixed_sign32(upper) - fixed_sign32(center)) * 0xc000000u, span_right));
    slope_middle = slope_left < slope_right ? slope_left : slope_right;
    slope_middle = min32(slope_middle, (target_high - target_low) * 2u);
    slope_middle = min32(slope_middle, ONE);
    square = mul(center - lower, center - lower);
    den = qmul(span_left, span_left);
    if (slope_left > ONE)
        slope_left = ONE;
    tmp = div32(square, den);
    if (fixed_less32(tmp, slope_left))
        slope_left = div32(square, den);
    square = mul(upper - center, upper - center);
    den = qmul(span_right, span_right);
    if (slope_right > ONE)
        slope_right = ONE;
    tmp = div32(square, den);
    if (fixed_less32(tmp, slope_right))
        slope_right = div32(square, den);
    if (fixed_less32(center, value))
        return hermite(value - center, span_right, center, upper, slope_middle, slope_right);
    return hermite(value - left, span_left, lower, center, slope_left, slope_middle);
}
