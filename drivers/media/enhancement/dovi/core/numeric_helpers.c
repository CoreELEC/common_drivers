/* Fixed-point range, normalization and resampling helpers. */
#include "numeric_helpers.h"
#include "numeric_common.h"
#include "fixed_arithmetic.h"
#include "control_memory.h"
#include "color_narrow.h"
typedef unsigned int u32;
typedef unsigned long long u64;
#include "color_primaries_tables.h"

unsigned int dovi_ConvertLvl2LvlBit(unsigned int level)
{
    if (level >> 31)
        return 0;
    if (level > 27u) {
        if (level - 256u < 0xfffffffcu)
            return 0;
        level -= 224u;
    }
    return 1u << (level & 31u);
}

unsigned int dovi_SetLvl(unsigned int bits, unsigned int level)
{
    return bits | dovi_ConvertLvl2LvlBit(level);
}

unsigned int dovi_ClearLvl(unsigned int bits, unsigned int level)
{
    return bits & ~dovi_ConvertLvl2LvlBit(level);
}

unsigned int dovi_HaveLvlSet(unsigned int bits, unsigned int level)
{
    return (bits & dovi_ConvertLvl2LvlBit(level)) != 0;
}

void dovi_SortAscend(void *values, void *indices, unsigned int last)
{
    unsigned char *a = values, *b = indices;
    unsigned int i, changed;

    if (last == 0 || last >> 31)
        return;
    do {
        changed = 0;
        for (i = 0; i < last; ++i) {
            unsigned int x = fixed_load_le32(a + 4ull * i), y = fixed_load_le32(a + 4ull * i + 4);

            if (fixed_less32(y, x)) {
                fixed_store_le32(a + 4ull * i + 4, x);
                fixed_store_le32(a + 4ull * i, y);
                x = fixed_load_le32(b + 4ull * i);
                y = fixed_load_le32(b + 4ull * i + 4);
                fixed_store_le32(b + 4ull * i, y);
                fixed_store_le32(b + 4ull * i + 4, x);
                changed = 1;
            }
        }
        if (last < 2u || !changed)
            break;
        --last;
    } while (1);
}

void dovi_GetRange(unsigned int bits, unsigned int range, void *minimum, void *extent)
{
    unsigned int lo, span;

    if (range == 0) {
        fixed_store_le16(minimum, 0x10u << ((bits - 8u) & 31));
        fixed_store_le16(extent, 0xdbu << ((bits - 8u) & 31));
        return;
    }
    if (range == 1) {
        fixed_store_le16(minimum, 0);
        fixed_store_le16(extent, ~(0xffffffffu << (bits & 31)));
        return;
    }
    lo = (range == 2 ? 1u : 0x10u) << ((bits - 8u) & 31);
    span = range == 2 ? 0x3fbu << ((bits - 10u) & 31) : ~(0xffffffffu << (bits & 31));
    fixed_store_le16(minimum, lo);
    fixed_store_le16(extent, span - lo);
}

static unsigned int bypass(unsigned int depth, unsigned int full, void *offset)
{
    dovi_GetYuvRgbOffFxp(full, depth, offset);
    if (full)
        return ~(0xffffffffu << (depth & 31));
    if (fixed_less32(depth, 9u))
        depth = 8;
    return 0xdbu << ((depth - 8u) & 31);
}

void dovi_GetCvmBypassNormParam(unsigned int raw_bits, unsigned int range, void *offset,
                                void *scale)
{
    unsigned int bits = raw_bits;
    unsigned int divisor = bypass(bits, range, offset);

    fixed_store_le32(scale, range && bits == 0 ? 0x7fffffffu
                                               : (unsigned int)fixed_raw_divide64(
                                                     0xffffffffull, fixed_sign32(divisor)));
}

void dovi_GetCvmBypassDnParam(unsigned int depth, unsigned int full, void *offset, void *factor)
{
    fixed_store_le32(factor, bypass(depth, full, offset));
}

void dovi_CommitKsUs(const void *format, unsigned int flags, void *output)
{
    const unsigned char *f = format;
    unsigned char *o = output;
    unsigned int kind;

    if (!(flags & 2u)) {
        kind = fixed_load_le32(f + 0x1c);
        fixed_store_le16(o + 2, 0);
        o[0] = (unsigned char)(kind == 0 ? 0 : kind == 1 ? 1 : 2);
        fixed_store_le16(o + 4, ~(0xffffffffu << (fixed_load_le32(f + 0x28) & 31)));
    } else
        fixed_store_le16(o + 2, 0);
    fixed_store_le32(o + 0x1a, 0x4000c0);
    fixed_store_le64(o + 6, 0xc0004000080001ull);
    fixed_store_le32(o + 0x2a, 0xc0004);
    fixed_store_le64(o + 0x2e, 0x998fdf4005e0016ull);
    fixed_store_le64(o + 0x36, 0x16005efdf40998ull);
}

void dovi_CommitKsDs(const void *format, void *output)
{
    const unsigned char *f = format;
    unsigned char *o = output;
    unsigned int kind = fixed_load_le32(f + 0x1c), depth;

    fixed_store_le16(o + 2, 0);
    o[0] = (unsigned char)(kind == 0 ? 0 : kind == 1 ? 1 : 2);
    depth = fixed_load_le32(f + 0x28);
    fixed_store_le32(o + 0x2a, 0xc0007);
    fixed_store_le64(o + 6, 0x80008000080001ull);
    fixed_store_le64(o + 0x2e, 0x2f0000000bull);
    fixed_store_le64(o + 0x3e, 0xfefa000004ccull);
    fixed_store_le64(o + 0x36, 0x80004cc0000fefaull);
    fixed_store_le16(o + 4, ~(0xffffffffu << (depth & 31)));
    fixed_store_le64(o + 0x44, 0xb0000002f0000ull);
}

unsigned long long dovi_Interp1DFxp(const void *table, unsigned int last, unsigned int shift,
                                    unsigned int mask, unsigned int value)
{
    const unsigned char *t = table;
    unsigned int index, left, right;

    if (!value || value >> 31)
        return fixed_sign16(fixed_load_le16(t));
    index = value >> (shift & 31);
    if (!fixed_less32(index, last))
        return fixed_sign16(fixed_load_le16(t + 2 * numeric_signed_index(last)));
    left = fixed_sign16(fixed_load_le16(t + 2ull * index));
    right = fixed_sign16(fixed_load_le16(t + 2ull * (index + 1u)));
    return fixed_asr32((right - left) * (value & mask), shift) + left;
}

unsigned int dovi_Interp1DForInverseLUTFxp(const void *table, const void *indices,
                                           unsigned int last, unsigned int value)
{
    const unsigned char *t = table, *p = indices;
    unsigned int lo = 0xffffffffu, hi, left, delta, slope;

    if (fixed_less32(value, fixed_sign16(fixed_load_le16(t))))
        return fixed_load_le32(p) << 3;
    do {
        hi = lo + 2u;
        if (fixed_less32(last, hi))
            return fixed_load_le32(p + 4 * numeric_signed_index(last)) << 3;
        ++lo;
    } while (!fixed_less32(value, fixed_sign16(fixed_load_le16(t + 2 * numeric_signed_index(hi)))));
    left = fixed_sign16(fixed_load_le16(t + 2 * numeric_signed_index(lo)));
    delta = fixed_load_le32(p + 4 * numeric_signed_index(hi)) -
            fixed_load_le32(p + 4 * numeric_signed_index(lo));
    slope = (unsigned int)dovi_sdiv(
        fixed_sign32(delta) << 19,
        fixed_sign16(fixed_load_le16(t + 2 * numeric_signed_index(hi))) - left);
    return fixed_asr32((value - left) * slope, 16) +
           (fixed_load_le32(p + 4 * numeric_signed_index(lo)) << 3);
}

static unsigned int inverse(const unsigned char *table, unsigned int last, unsigned int value)
{
    unsigned int lo = 0, hi = fixed_sign16(last), mid, den, num, result;

    if (!fixed_less32(fixed_load_le32(table), value))
        return 0;
    if (!fixed_less32(value, fixed_load_le32(table + 4 * numeric_signed_index(hi))))
        return 0x7fff;
    do {
        mid = (hi + fixed_sign16(lo)) >> 1;
        if (!fixed_less32(value,
                          fixed_load_le32(table + 4 * numeric_signed_index(fixed_sign16(mid)))))
            lo = mid;
        else
            hi = mid;
        lo = fixed_sign16(lo);
        hi = fixed_sign16(hi);
    } while (fixed_less32(lo + 1u, hi));
    den = fixed_load_le32(table + 4 * numeric_signed_index(hi)) -
          fixed_load_le32(table + 4 * numeric_signed_index(lo));
    num = (value - fixed_load_le32(table + 4 * numeric_signed_index(lo))) << 15;
    result = hi << 6;
    if (den)
        result = fixed_asr32(fixed_raw_divide32(num, den), 9) + (lo << 6);
    return result;
}

unsigned int dovi_InvLUT1DNorm(const void *table, unsigned int last, unsigned int value)
{
    return inverse(table, last, value);
}

unsigned int dovi_InvLUT1D(unsigned int value, unsigned int low, unsigned int high,
                           const void *table, unsigned int last)
{
    return low + fixed_asr32(fixed_sign16(inverse(table, last, value)) * (high - low), 15);
}

void *dovi_GetDisplay(unsigned int index)
{
    index &= 255u;
    return index < 50u ? (void *)fixed_load_le64(dovi_DisplayIdxTbl + 8u * index) : (void *)0;
}

const void *dovi_GetPrimariesPtr(unsigned int index)
{
    return index < 19u ? primary_primaries + index * 8u : (const void *)0;
}
