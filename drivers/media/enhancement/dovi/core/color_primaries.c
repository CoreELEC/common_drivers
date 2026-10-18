/* Fixed-point color primaries. */
#include "fixed_access.h"
#include "color_primaries.h"
#include "matrix_arithmetic.h"
#include "fixed_arithmetic.h"
typedef unsigned int u32;
typedef unsigned long long u64;
#include "color_primaries_tables.h"

static unsigned long long mul32(unsigned int a, unsigned int b)
{
    return fixed_sign32(a) * fixed_sign32(b);
}

static unsigned long long sdiv64(unsigned long long a, unsigned long long b)
{
    return fixed_raw_divide64(a, b);
}

static unsigned int sdiv32(unsigned int a, unsigned int b)
{
    return (unsigned int)sdiv64(fixed_sign32(a), fixed_sign32(b));
}

static unsigned long long udiv64(unsigned long long a, unsigned long long b)
{
    return b == 0 ? 0 : a / b;
}

unsigned int dovi_GetRgb2YuvM33Fxp(unsigned int definition, void *output)
{
    unsigned long long temporary[9];
    if (definition < 6u) {
        unsigned int kr = primary_kr[definition], kg = primary_kg[definition],
                     kb = primary_kb[definition];
        unsigned long long db = (unsigned int)(0x4000000u - kb), dr = 0x4000000ull - kr;
        unsigned long long green = (unsigned long long)(kg & 0xfffffffeu) << 25;
        temporary[0] = kr;
        temporary[1] = kg;
        temporary[2] = fixed_sign32(kb);
        temporary[3] = 0ull - udiv64((unsigned long long)(kr & 0xfffffffeu) << 25, db);
        temporary[4] = 0ull - udiv64(green, db);
        temporary[7] = 0ull - udiv64(green, dr);
        temporary[8] = 0ull - sdiv64(fixed_sign32(fixed_asr32(kb, 1)) << 26, dr);
    } else {
        temporary[0] = 0x1322d0eu;
        temporary[1] = 0x2591687u;
        temporary[2] = 0x74bc6au;
        temporary[3] = 0ull - 0xacc919u;
        temporary[4] = 0ull - 0x15336e5u;
        temporary[7] = 0ull - 0x1acbc6fu;
        temporary[8] = 0ull - 0x53438fu;
    }
    temporary[5] = 0x2000000;
    temporary[6] = 0x2000000;
    return dovi_Adjust64M33ScaleTo16M33(temporary, 26u, output);
}

unsigned int dovi_GetRgb2YuvM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output)
{
    unsigned int bits = raw_bits;
    unsigned char full[18];
    unsigned long long temporary[9];
    unsigned int scale, luma, chroma, i, mask;
    scale = dovi_GetRgb2YuvM33Fxp(definition, full);
    if (!fixed_less32(8u, bits))
        bits = 8;
    mask = ~(0xffffffffu << (bits & 31u));
    luma = sdiv32(0xdb0000u << ((bits - 8u) & 31u), mask);
    chroma = sdiv32(0xe00000u << ((bits - 8u) & 31u), mask);
    for (i = 0; i < 9u; ++i)
        temporary[i] = mul32(fixed_sign16(fixed_load_le16(full + i * 2u)), i < 3u ? luma : chroma);
    return dovi_Adjust64M33ScaleTo16M33(temporary, scale + 16u, output);
}

unsigned int dovi_GetYuv2RgbM33Fxp(unsigned int definition, void *output)
{
    unsigned long long temporary[9];
    temporary[0] = temporary[3] = temporary[6] = 0x4000000u;
    temporary[1] = temporary[8] = 0;
    if (definition < 6u) {
        unsigned int kr = primary_kr[definition], kb = primary_kb[definition];
        unsigned long long kg = primary_kg_wide[definition], r = 0x4000000ull - kr;
        temporary[2] = r * 2u;
        temporary[4] = sdiv64(0ull - (fixed_sign32(kb) * (unsigned int)(0x4000000u - kb) * 2u), kg);
        temporary[5] = 0ull - udiv64((unsigned long long)kr * (unsigned int)r * 2u, kg);
        temporary[7] = fixed_sign32(0x8000000u - kb * 2u);
    } else {
        temporary[2] = 0x59ba5e4u;
        temporary[4] = 0ull - 0x1606542u;
        temporary[5] = 0ull - 0x2db468au;
        temporary[7] = 0x716872cu;
    }
    return dovi_Adjust64M33ScaleTo16M33(temporary, 26u, output);
}

unsigned int dovi_GetYuv2RgbM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output)
{
    unsigned int bits = raw_bits;
    unsigned char full[18];
    unsigned long long temporary[9];
    unsigned int scale, luma, chroma, i, numerator;
    scale = dovi_GetYuv2RgbM33Fxp(definition, full);
    bits = fixed_sign16(bits);
    if (!fixed_less32(8u, bits))
        bits = 8;
    numerator = ~(0xffffffffu << (bits & 31u)) << 16;
    luma = sdiv32(numerator, 0xdbu << ((bits - 8u) & 31u));
    chroma = sdiv32(numerator, 0xe0u << ((bits - 8u) & 31u));
    for (i = 0; i < 9u; ++i)
        temporary[i] =
            mul32(fixed_sign16(fixed_load_le16(full + i * 2u)), i % 3u == 0 ? luma : chroma);
    return dovi_Adjust64M33ScaleTo16M33(temporary, scale + 16u, output);
}

void dovi_SortDescend(void *values, void *indices, unsigned int last_index)
{
    unsigned char *v = values, *p = (unsigned char *)indices;
    unsigned int remaining = last_index;
    if (remaining == 0 || (remaining >> 31) != 0)
        return;
    do {
        unsigned int i, changed = 0;
        for (i = 0; i < remaining; ++i) {
            unsigned char *a = v + (unsigned long long)i * 4u, *b = p + (unsigned long long)i * 4u;
            unsigned int left = fixed_load_le32(a), right = fixed_load_le32(a + 4);
            if (fixed_less32(left, right)) {
                fixed_store_le32(a + 4, left);
                fixed_store_le32(a, right);
                left = fixed_load_le32(b);
                right = fixed_load_le32(b + 4);
                fixed_store_le32(b, right);
                fixed_store_le32(b + 4, left);
                changed = 1;
            }
        }
        if (remaining < 2u || changed == 0)
            break;
        --remaining;
    } while (1);
}

unsigned long long dovi_Xy2Cct(const void *xy32, void *cct64)
{
    const unsigned char *xy = xy32;
    unsigned int x = fixed_load_le32(xy), y = fixed_load_le32(xy + 4), z = 0x4000000u - x - y;
    unsigned int denominator =
        (unsigned int)((mul32(x, 0x02c9dc21u) + mul32(y, 0x1df77c9cu) + mul32(z, 0x0e1ffd2fu)) >>
                       26);
    unsigned int ratio = (unsigned int)dovi_sdiv(
        mul32(x, 0x05bf9fcdu) + mul32(y, 0x0b2963fbu) + mul32(z, 0xff6cfcf0u), denominator);
    unsigned int reference, difference, divisor;
    unsigned long long result, cct;
    (void)dovi_sdiv(mul32(x, 0xfced3ca3u) + mul32(y, 0x119b8d8eu) + mul32(z, 0x0134bc46u),
                    denominator);
    reference = (unsigned int)dovi_sdiv(0x00150dfaeb631000ull, 0x0fcab7c4u);
    (void)dovi_sdiv(0x00150e10c9f07000ull, 0x0fcab7c4u);
    difference = ratio - reference;
    divisor = (unsigned int)(mul32(difference + 0x01555555u, 0x275b09c1u) >> 26) - 0x088fe2c1u;
    if (!fixed_less32(0x4000u, divisor))
        divisor = 0x4000u;
    result = dovi_sdiv(fixed_sign32(0x048e75d5u - difference) << 26, divisor);
    cct = mul32((unsigned int)result, 0x1968u);
    if (!fixed_less32((unsigned int)result, 0x07b0050au))
        cct = 0x000000c350000000ull;
    if (!fixed_less32(0x01899a9bu, (unsigned int)result))
        cct = 0x0000002710000000ull;
    fixed_store_le64(cct64, cct);
    return result;
}

void dovi_XyY2Xyz(unsigned int x, unsigned int y, unsigned int luminance, void *xyz64)
{
    unsigned char *out = xyz64;
    unsigned long long xx, zz;
    fixed_store_le64(out + 8, luminance);
    xx = sdiv64((unsigned long long)luminance * fixed_sign32(x), fixed_sign32(y));
    zz = sdiv64((unsigned long long)luminance * fixed_sign32(0x8000u - x - y), fixed_sign32(y));
    fixed_store_le64(out, xx);
    fixed_store_le64(out + 16, zz);
}

#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void primary_bounds_trap(void)
{
    __debugbreak();
    __assume(0);
}
#else
__attribute__((noreturn)) static void primary_bounds_trap(void)
{
#if defined(__aarch64__)
    __asm__ volatile("brk #1");
    __builtin_unreachable();
#else
    __builtin_trap();
#endif
}
#endif

unsigned int dovi_GetPrimaries(unsigned int id, void *primaries32)
{
    unsigned char *out = primaries32;
    unsigned int offset = id * 32u, i;
    if (id > 18u)
        return 0xffffffffu;
    for (i = 0; i < 32u; ++i) {
        if (i != 0 && offset + i >= 641u)
            primary_bounds_trap();
        out[i] =
            (unsigned char)(primary_primaries[(offset + i) / 4u] >> (((offset + i) & 3u) * 8u));
    }
    return 26u;
}

void dovi_GetRgb2XyzM33ByPrims(unsigned int rx, unsigned int ry, unsigned int gx, unsigned int gy,
                               unsigned int bx, unsigned int by, unsigned int wx, unsigned int wy,
                               void *matrix32)
{
    unsigned int a = rx - bx, b = gy - by, c = ry - by, d = gx - bx;
    unsigned long long determinant = mul32(b, a) - mul32(c, d), r = 0, g = 0, rs, gs, bs;
    unsigned long long denominator = fixed_sign32((unsigned int)(determinant >> 26));
    unsigned int values[9], i;
    if ((determinant & 0x03fffffffc000000ull) != 0) {
        r = sdiv64(mul32(b, wx - bx) - mul32(d, wy - by), denominator);
        g = sdiv64(mul32(a, wy - by) - mul32(c, wx - bx), denominator);
    }
    rs = sdiv64(r << 26, fixed_sign32(wy));
    gs = sdiv64(g << 26, fixed_sign32(wy));
    bs = sdiv64(0x0010000000000000ull - ((r + g) << 26), fixed_sign32(wy));
    values[0] = (unsigned int)((rs * fixed_sign32(rx)) >> 26);
    values[1] = (unsigned int)((gs * fixed_sign32(gx)) >> 26);
    values[2] = (unsigned int)((bs * fixed_sign32(bx)) >> 26);
    values[3] = (unsigned int)((rs * fixed_sign32(ry)) >> 26);
    values[4] = (unsigned int)((gs * fixed_sign32(gy)) >> 26);
    values[5] = (unsigned int)((bs * fixed_sign32(by)) >> 26);
    values[6] = (unsigned int)((rs * fixed_sign32(0x4000000u - rx - ry)) >> 26);
    values[7] = (unsigned int)((gs * fixed_sign32(0x4000000u - gx - gy)) >> 26);
    values[8] = (unsigned int)((bs * fixed_sign32(0x4000000u - bx - by)) >> 26);
    for (i = 0; i < 9u; ++i)
        fixed_store_le32((unsigned char *)matrix32 + i * 4u, values[i]);
}

void dovi_GetRgb2XyzM33ByV8Prims(const void *primaries32, void *matrix32)
{
    const unsigned char *p = primaries32;
    unsigned int v[8], i;
    for (i = 0; i < 8u; ++i)
        v[i] = fixed_load_le32(p + i * 4u);
    dovi_GetRgb2XyzM33ByPrims(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], matrix32);
}
