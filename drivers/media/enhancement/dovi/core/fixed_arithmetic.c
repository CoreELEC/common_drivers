/* Fixed-point arithmetic with native argument and result widths. */
#include "fixed_access.h"
#include "fixed_arithmetic.h"

static long long lsl64(long long a, unsigned int n)
{
    return fixed_signed64((unsigned long long)a << (n & 63u));
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

int dovi_smpy_ss(unsigned int first, unsigned int second)
{
    return fixed_signed32((unsigned int)fixed_multiply(first, second));
}

unsigned int dovi_unorm_s(unsigned int value)
{
    return fixed_normalize32(fixed_abs32(value));
}

unsigned int dovi_unorm_u(unsigned int value)
{
    return fixed_normalize32(value);
}

unsigned int dovi_unorm_u_lut(unsigned int value)
{
    return fixed_normalize_lut32(value);
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
    int value = fixed_signed32(raw_value), shift = fixed_signed32(raw_shift);
    unsigned long long result;
    int low, high;
    if (shift > 31)
        shift = 31;
    result = (unsigned long long)(long long)value << ((unsigned int)shift & 63u);
    low = fixed_signed32((unsigned int)result);
    high = fixed_signed32((unsigned int)(result >> 32));
    return high == (low >> 31) ? low : (value < 0 ? (-2147483647 - 1) : 2147483647);
}

int dovi_sshr_sss(unsigned int raw_value, unsigned int raw_shift)
{
    int value = fixed_signed32(raw_value), shift = fixed_signed32(raw_shift);
    unsigned int amount;
    long long result;
    if (shift >= 0)
        return value >> (shift > 31 ? 31 : shift);
    amount = 0u - (unsigned int)shift;
    if (fixed_signed32(amount) > 31)
        amount = 31;
    result = lsl64(value, amount);
    if (fixed_signed32((unsigned int)((unsigned long long)result >> 32)) !=
        (fixed_signed32((unsigned int)result) >> 31))
        return value < 0 ? (-2147483647 - 1) : 2147483647;
    return fixed_signed32((unsigned int)result);
}

int dovi_sdiv_sls(unsigned long long numerator, unsigned int denominator)
{
    return fixed_signed32((unsigned int)fixed_divide(numerator, denominator));
}

int dovi_abs_int32(unsigned int value)
{
    return fixed_signed32(fixed_abs32(value));
}

long long dovi_abs_int64(unsigned long long value)
{
    return fixed_signed64(fixed_abs64(value));
}
