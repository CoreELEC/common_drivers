/* Fixed-point arithmetic with native argument and result widths. */
#include "fixed_access.h"
#include "fixed_arithmetic.h"


static unsigned long long unsigned_product_high(unsigned long long a, unsigned long long b)
{
    unsigned long long low = (unsigned long long)(unsigned int)a * (unsigned int)b;
    unsigned long long cross_a = (unsigned long long)(unsigned int)a * (b >> 32);
    unsigned long long cross_b = (unsigned long long)(unsigned int)b * (a >> 32);
    unsigned long long carry = (low >> 32) + (unsigned int)cross_a + (unsigned int)cross_b;
    return (a >> 32) * (b >> 32) + (cross_a >> 32) + (cross_b >> 32) + (carry >> 32);
}

int dovi_sadd_sss(unsigned int first, unsigned int second)
{
    return fixed_signed32(fixed_add(first, second));
}

int dovi_ssub_sss(unsigned int first, unsigned int second)
{
    return fixed_signed32(fixed_subtract(first, second));
}

unsigned int dovi_uabs_ss(unsigned int value)
{
    return fixed_abs32(value);
}

unsigned long long dovi_uabs_ls(unsigned long long value)
{
    return fixed_abs64(value);
}

unsigned long long dovi_smpy_ss(unsigned int first, unsigned int second)
{
    return fixed_multiply(first, second);
}

unsigned int dovi_unorm_s(unsigned int value)
{
    return fixed_normalize32(fixed_abs32(value));
}

unsigned long long dovi_unorm_u(unsigned long long value)
{
    /* The original cbz w0 returns without changing x0 in this case. */
    return (unsigned int)value == 0 ? value : fixed_normalize32((unsigned int)value);
}

unsigned long long dovi_unorm_u_lut(unsigned long long value)
{
    return (unsigned int)value == 0 ? value : fixed_normalize_lut32((unsigned int)value);
}

unsigned int dovi_unorm_sl(unsigned long long value)
{
    return fixed_normalize64(fixed_abs64(value));
}

unsigned int dovi_unorm_ul(unsigned long long value)
{
    return fixed_normalize64(value);
}

int dovi_sshl_ssu(unsigned int raw_value, unsigned int raw_shift)
{
    unsigned int value = raw_value, shift = raw_shift;
    unsigned long long wide;
    if ((shift & 0x80000000u) != 0)
        return fixed_signed32(0);
    if (shift >= 31u)
        shift = 31;
    wide = fixed_sign32(value) << shift;
    if (wide != fixed_sign32((unsigned int)wide))
        return fixed_signed32(0x7fffffffu + (value >> 31));
    return fixed_signed32((unsigned int)wide);
}

int dovi_sshr_sss(unsigned int raw_value, unsigned int raw_shift)
{
    unsigned int value = raw_value, shift = raw_shift;
    if ((shift & 0x80000000u) != 0)
        return fixed_signed32(dovi_sshl_ssu(value, 0u - shift));
    return fixed_signed32(fixed_asr32(value, shift < 31u ? shift : 31u));
}

unsigned long long dovi_sdiv_sls(unsigned long long numerator, unsigned int denominator)
{
    return denominator == 0 ? 0 : fixed_divide(numerator, denominator);
}

int dovi_abs_int32(unsigned int value)
{
    return fixed_signed32(fixed_abs32(value));
}

long long dovi_abs_int64(unsigned long long value)
{
    return fixed_signed64(fixed_abs64(value));
}

unsigned long long dovi_sdiv(unsigned long long numerator, unsigned int denominator)
{
    return denominator == 0 ? 0x7fffffffull : fixed_divide(numerator, denominator);
}

void dovi_uint64_mul(unsigned long long a, unsigned long long b, void *high, void *low)
{
    unsigned long long upper = unsigned_product_high(a, b);
    fixed_store_le64(low, a * b);
    fixed_store_le64(high, upper);
}

void dovi_int64_mul(unsigned long long a, unsigned long long b, void *high, void *low)
{
    unsigned long long upper = unsigned_product_high(a, b);
    upper -= b & (0ull - (a >> 63));
    upper -= a & (0ull - (b >> 63));
    fixed_store_le64(low, a * b);
    fixed_store_le64(high, upper);
}

unsigned long long dovi_uint64_lshift(unsigned long long a, unsigned long long b,
                                      unsigned int shift)
{
    return (a << ((0u - shift) & 63u)) | (b >> (shift & 63u));
}

unsigned long long dovi_int64_lshift(unsigned long long a, unsigned long long b,
                                     unsigned long long shift)
{
    return (a << (shift & 63u)) | fixed_asr64(b, (unsigned int)shift);
}
