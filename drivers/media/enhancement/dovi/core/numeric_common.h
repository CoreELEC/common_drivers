/* Fixed-width numeric views shared by both reconstructed module versions. */
#ifndef DOVI_NUMERIC_COMMON_H
#define DOVI_NUMERIC_COMMON_H
#include "fixed_access.h"

/* Signed C values and raw register bits are separate views of the same fixed
 * widths. These adapters retain their callers' native scalar contracts. */
static inline long long numeric_add64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a + (unsigned long long)b);
}

static inline long long numeric_subtract64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a - (unsigned long long)b);
}

static inline long long numeric_multiply64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a * (unsigned long long)b);
}

static inline long long numeric_shift_left64(long long a, unsigned int n)
{
    return fixed_signed64((unsigned long long)a << (n & 63u));
}

static inline long long numeric_divide64(long long a, long long b)
{
    return fixed_signed64(fixed_raw_divide64((unsigned long long)a, (unsigned long long)b));
}

static inline int numeric_divide32(int a, int b)
{
    return fixed_signed32(fixed_raw_divide32((unsigned int)a, (unsigned int)b));
}

static inline int numeric_subtract32(int a, int b)
{
    return fixed_signed32((unsigned int)a - (unsigned int)b);
}

/* Byte accesses retain legal unaligned storage and alias reloads across widths. */

static inline unsigned int numeric_load_sign_extended16_at(const void *p, unsigned int i)
{
    return fixed_sign16(fixed_load_le16((const unsigned char *)p + i * 2u));
}

static inline unsigned int numeric_load32_at(const void *p, unsigned int i)
{
    return fixed_load_le32((const unsigned char *)p + i * 4u);
}

static inline int numeric_load_signed16_at(const void *p, unsigned int i)
{
    return fixed_signed16(numeric_load_sign_extended16_at(p, i));
}

static inline int numeric_load_signed32_at(const void *p, unsigned int i)
{
    return fixed_signed32(numeric_load32_at(p, i));
}

static inline long long numeric_load_signed64_at(const void *p, unsigned int i)
{
    return fixed_signed64(fixed_load_le64((const unsigned char *)p + i * 8u));
}

static inline void numeric_store16_at(void *p, unsigned int i, long long v)
{
    fixed_store_le16((unsigned char *)p + i * 2u, (unsigned int)v);
}

static inline void numeric_store32_at(void *p, unsigned int i, long long v)
{
    fixed_store_le32((unsigned char *)p + i * 4u, (unsigned int)v);
}

static inline void numeric_store64_at(void *p, unsigned int i, long long v)
{
    fixed_store_le64((unsigned char *)p + i * 8u, (unsigned long long)v);
}

/* Operations whose callers carry unsigned two's-complement register bits. */
static inline unsigned long long numeric_multiply_signed32(unsigned int a, unsigned int b)
{
    return fixed_sign32(a) * fixed_sign32(b);
}

static inline long long numeric_signed_index(unsigned int v)
{
    return v >> 31 ? -1 - (long long)(~v) : (long long)v;
}

static inline unsigned int numeric_saturate16(unsigned long long v)
{
    if (fixed_less64(v, 0xffffffffffff8000ull))
        return 0x8000;
    return fixed_less64(0x7fffull, v) ? 0x7fff : (unsigned int)v;
}
#endif
