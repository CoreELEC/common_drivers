/* Integer Taylor approximations with native scale and result handling. */
#include "fixed_access.h"
#include "fixed_transcendentals.h"
#include "fixed_arithmetic.h"

static long long mul64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a * (unsigned long long)b);
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
    if (fixed_signed32(shift) > 31)
        shift = 31;
    result = fixed_asr32(result + 0x8e8u, shift);
    integral = (30u - norm) << (raw_scale_out & 31u);
    return fixed_signed32(result + integral);
}

int dovi_fxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in, unsigned int raw_scale_out)
{
    int value = fixed_signed32(raw_value), scale_in = fixed_signed32(raw_scale_in),
        scale_out = fixed_signed32(raw_scale_out);
    unsigned int a = ((unsigned int)value << ((32u - (unsigned int)scale_in) & 31u)) >> 1;
    int result, shift;
    if (scale_in > 31)
        scale_in = 31;
    result = fixed_signed32(fixed_power_polynomial(a));
    shift = fixed_signed32(30u - (unsigned int)(value >> ((unsigned int)scale_in & 31u)) -
                           (unsigned int)scale_out);
    return dovi_sshr_sss(result, shift);
}

int dovi_powTaylor(unsigned int value, unsigned int exponent)
{
    int l = dovi_fxLOG2Taylor(fixed_signed32(value), 26);
    long long product = mul64(fixed_signed32((unsigned int)l - 0x7c000000u), (long long)exponent);
    return dovi_fxPOW2Taylor(fixed_signed32((unsigned int)(product >> 31)), 26, 31);
}

long long dovi_LfxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in)
{
    int value = fixed_signed32(raw_value), scale_in = fixed_signed32(raw_scale_in);
    unsigned int a = ((unsigned int)value << ((32u - (unsigned int)scale_in) & 31u)) >> 1;
    int result, integer;
    unsigned int shift;
    if (scale_in > 31)
        scale_in = 31;
    integer = value >> ((unsigned int)scale_in & 31u);
    result = fixed_signed32(fixed_power_polynomial(a));
    shift = (unsigned int)integer + 2u;
    if (fixed_signed32(shift) > 0)
        return fixed_signed64((unsigned long long)(long long)result << (shift & 63u));
    return (long long)result >> (~((unsigned int)integer + 1u) & 63u);
}

int dovi_expNTaylor(unsigned int raw_value, unsigned int raw_scale_out)
{
    int value = fixed_signed32(raw_value), scale_out = fixed_signed32(raw_scale_out);
    long long product = (long long)value * 0x5c551d94;
    return dovi_fxPOW2Taylor(fixed_signed32((unsigned int)((unsigned long long)product >> 30)), 30,
                             scale_out);
}

unsigned long long dovi__powInSTaylor(unsigned long long raw_value, unsigned int raw_scale)
{
    long long value = fixed_signed64(raw_value);
    long long scale = fixed_signed64(raw_scale);
    int l = dovi_fxLOG2Taylor(
        fixed_signed32((unsigned int)(value >> ((unsigned long long)scale & 63u))), 26);
    int p = dovi_fxPOW2Taylor(
        fixed_signed32(
            (unsigned int)(mul64(fixed_signed32((unsigned int)l - 0x7c000000u), 0x14640000) >> 31)),
        26, 30);
    int factor = dovi_fxPOW2Taylor(0x14640000, 31, 27), i;
    /* Use ASR for the first product, then logical LSR in its loop. */
    unsigned long long result = (unsigned long long)(((long long)p * factor) >> 27);
    for (i = 2; i < fixed_signed32((unsigned int)scale); ++i)
        result = (result * (unsigned long long)(long long)factor) >> 27;
    return ((unsigned long long)(long long)factor * result) >> 26;
}

unsigned int dovi_ssqrt_s(unsigned int value)
{
    return fixed_sqrt_approximation(value);
}

int dovi_log10Taylor(unsigned int value)
{
    int l = fixed_signed32((unsigned int)dovi_fxLOG2Taylor(value, 26) - 0x7c000000u);
    return fixed_signed32((unsigned int)((unsigned long long)((long long)l * 0x268826a1) >> 31));
}
