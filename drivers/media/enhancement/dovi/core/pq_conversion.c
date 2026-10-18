/* Fixed-point PQ transfer functions. */
#include "fixed_access.h"
#include "pq_conversion.h"
#include "fixed_transcendentals.h"
#include "pq_conversion_tables.h"
#include "pq12_impl.h"

const unsigned int pq_l2pqLutA[128] = {
#include "pq_luminance_slopes.inc"
};
const unsigned int pq_l2pqLutB[128] = {
#include "pq_luminance_intercepts.inc"
};
const unsigned int pq_l2pqLutX[128] = {
#include "pq_luminance_positions.inc"
};
const unsigned int pq_depq1024Lut[1024] = {
#include "pq_inverse_values.inc"
};

static long long mul64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a * (unsigned long long)b);
}

static long long add64(long long a, long long b)
{
    return fixed_signed64((unsigned long long)a + (unsigned long long)b);
}

static long long lsl64(long long a, unsigned int n)
{
    return fixed_signed64((unsigned long long)a << (n & 63u));
}

static long long div64(long long a, long long b)
{
    return fixed_signed64(fixed_raw_divide64((unsigned long long)a, (unsigned long long)b));
}

int dovi_L2PqX(unsigned long long raw_linear)
{
    long long linear = fixed_signed64(raw_linear);
    unsigned long long magnitude =
        linear < 0 ? 0ull - (unsigned long long)linear : (unsigned long long)linear;
    /* LSL followed by signed magic division implements signed /10000. */
    long long input = div64(fixed_signed64(magnitude << 7), 10000), powered, denominator, numerator,
              result;
    int t, exponent;
    unsigned int i;
    if (input < 0x80000000ll)
        powered = dovi_powTaylor((unsigned int)input, 0x14640000u);
    else if (input < 0x200000000ll) {
        t = dovi_fxLOG2Taylor(fixed_signed32((unsigned int)((unsigned long long)input >> 2)), 26);
        t = dovi_fxPOW2Taylor(
            fixed_signed32(
                (unsigned int)(mul64(fixed_signed32((unsigned int)t - 0x7c000000u), 0x14640000) >>
                               31)),
            26, 30);
        exponent = dovi_fxPOW2Taylor(0x14640000, 31, 29);
        powered = ((long long)exponent *
                   fixed_signed32((unsigned int)(((long long)t * exponent) >> 29))) >>
                  28;
    } else {
        if (input < 0x800000000ll)
            exponent = 4;
        else if (input < 0x2000000000ll)
            exponent = 6;
        else if (input < 0x8000000000ll)
            exponent = 8;
        else if (input < 0x20000000000ll)
            exponent = 10;
        else if (input < 0x80000000000ll)
            exponent = 12;
        else if (input < 0x200000000000ll)
            exponent = 14;
        else if (input < 0x800000000000ll)
            exponent = 16;
        else {
            t = fixed_signed32(0x8864a180u);
            goto final_power;
        }
        powered = fixed_signed64(dovi__powInSTaylor(input, exponent));
    }
    denominator = add64(mul64(powered, 0x4ac00000) >> 26, 0x80000000ll) >> 8;
    numerator = add64(mul64(powered, 0x4b680000) >> 26, 0x6b000000ll) >> 8;
    t = fixed_signed32((unsigned int)div64(lsl64(numerator, 23), denominator) << 7);
final_power:
    t = dovi_fxLOG2Taylor(t, 26);
    t = dovi_fxPOW2Taylor(
        fixed_signed32(
            (unsigned int)(mul64(fixed_signed32((unsigned int)t - 0x7c000000u), 0x4ed80000) >> 31)),
        26, 30);
    exponent = dovi_fxPOW2Taylor(0x4ed80000, 31, 27);
    result = ((long long)t * exponent) >> 27;
    result = mul64(result, result) >> 29;
    for (i = 0; i < 6; ++i)
        result = mul64(result, result) >> 31;
    if (linear < 0)
        result = fixed_signed64(0x620ull - (unsigned long long)result);
    return fixed_signed32((unsigned int)(((unsigned long long)result + 0x1000ull) >> 13));
}

unsigned int dovi_PQ16ToL(unsigned int pq)
{
    return pq16_to_linear(pq, pq_depq1024Lut);
}

unsigned int dovi_PQ12ToL(unsigned int pq)
{
    return pq12_to_linear(pq, pq_depq1024Lut);
}

unsigned int dovi_LToPQ12(unsigned int luminance)
{
    /* STB 2.4 control preserves the rounded 4096 endpoint. */
    return linear_to_pq12(luminance, pq_l2pqLutX, pq_l2pqLutA, pq_l2pqLutB, 4096u);
}

unsigned short dovi_LToPQ15(unsigned int luminance)
{
    unsigned int index = pq12_node(luminance, pq_l2pqLutX);
    unsigned long long product =
        (unsigned long long)(luminance - pq_l2pqLutX[index]) * pq_l2pqLutA[index];
    return (unsigned short)(((pq_l2pqLutB[index] + (unsigned int)(product >> 25)) >> 1) & 0x7fffu);
}
