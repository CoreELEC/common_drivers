/* Fixed-point PQ transfer functions. */
#include "fixed_access.h"
#include "pq_conversion.h"
#include "fixed_transcendentals.h"
#include "pq_conversion_tables.h"
#include "pq12_impl.h"

const unsigned int pq_depq1024Lut[1024] = {
#include "pq_inverse_values.inc"
};


#if defined(_MSC_VER)
void __debugbreak(void);
#pragma intrinsic(__debugbreak)

__declspec(noreturn) static void pq_trap(unsigned int immediate)
{
    (void)immediate;
    __debugbreak();
    __assume(0);
}
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((noreturn)) static void pq_trap(unsigned int immediate)
{
#if defined(__aarch64__)
    if (immediate == 1u)
        __asm__ volatile("brk #1");
    else
        __asm__ volatile("brk #0x5512");
    __builtin_unreachable();
#else
    (void)immediate;
    __builtin_trap();
#endif
}
#else
#error "Provide the original nonreturning PQ lookup traps."
#endif

static unsigned long long smull(unsigned int a, unsigned int b)
{
    return fixed_sign32(a) * fixed_sign32(b);
}

/* SDIV instruction semantics, not the separately recovered saturating sdiv(). */
static unsigned long long sdiv64(unsigned long long numerator, unsigned long long denominator)
{
    return fixed_raw_divide64(numerator, denominator);
}

static unsigned long long lut_product(unsigned int value, unsigned int *index)
{
    *index = dovi_FindNodeIdxL2PQS(value);
    if (*index >= 128u)
        pq_trap(0x5512);
    return (unsigned long long)pq_l2pqLutA[*index] * (value - pq_l2pqLutX[*index]);
}

static unsigned long long pq_forward_finish(unsigned long long magnitude, int negative)
{
    unsigned long long denominator =
        fixed_asr64(fixed_asr64(magnitude * 0x4ac00000ull, 26) + 0x80000000ull, 8);
    unsigned long long numerator =
        (fixed_asr64(magnitude * 0x4b680000ull, 34) << 23) + 0x358000000000ull;
    unsigned int logged = dovi_fxLOG2Taylor((unsigned int)sdiv64(numerator, denominator) << 7, 26);
    unsigned int powered =
        dovi_fxPOW2Taylor((unsigned int)(smull(logged + 0x84000000u, 0x4ed80000u) >> 31), 26, 30);
    unsigned int factor = dovi_fxPOW2Taylor(0x4ed80000u, 31, 27), i;
    unsigned long long result = fixed_asr64(smull(powered, factor), 27);
    result = fixed_asr64(result * result, 29);
    for (i = 0; i < 6; ++i)
        result = fixed_asr64(result * result, 31);
    if (negative)
        result = 0x620ull - result;
    return (result + 0x1000ull) >> 13;
}

static unsigned long long pq_inverse(unsigned int value, int extended)
{
    unsigned int shifted = value << 12, input = shifted;
    unsigned long long powered, divisor, numerator, result;
    if (extended && fixed_less32(shifted, 0x310u))
        input = 0x620u - shifted;
    if (!fixed_less32(input, 0x7f7dc000u))
        input = 0x7f7dc000u;
    powered = dovi_powIn4Taylor(fixed_sign32(input) << 1, 0x19f9b58u);
    if (fixed_less32((unsigned int)powered, 0x1ac00001u))
        return 0;
    divisor = 0x96d000000ull - fixed_asr64(smull((unsigned int)powered, 0x4ac00000u), 24);
    numerator = (powered << 33) - 0x3580000000000000ull;
    powered = dovi_powIn16Exp8Taylor(sdiv64(numerator, divisor), 0x32381aadcull);
    result = fixed_asr64(powered + 64u, 7) * 10000u;
    return extended && fixed_less32(shifted, 0x310u) ? 0ull - result : result;
}

unsigned long long dovi_L2PqX(unsigned long long raw_linear)
{
    unsigned long long linear = raw_linear;
    unsigned long long magnitude = linear >> 63 ? 0ull - linear : linear;
    unsigned long long scaled = magnitude << 7, quotient = sdiv64(scaled, 10000), result, factor;
    unsigned int scale, powered, base, i;
    if (fixed_less64(scaled, 0x138800000000ull))
        result = fixed_sign32(dovi_powTaylor((unsigned int)quotient, 0x14640000u));
    else if ((quotient >> 33) == 0) {
        powered = dovi_fxLOG2Taylor((unsigned int)(quotient >> 2), 26);
        powered = dovi_fxPOW2Taylor((unsigned int)(smull(powered + 0x84000000u, 0x14640000u) >> 31),
                                    26, 30);
        base = dovi_fxPOW2Taylor(0x14640000u, 31, 29);
        result = fixed_asr64(smull((unsigned int)(smull(powered, base) >> 29), base), 28);
    } else if ((quotient >> 39) == 0) {
        scale = (quotient >> 35) == 0 ? 4u : (quotient >> 37) == 0 ? 6u : 8u;
        powered = dovi_fxLOG2Taylor((unsigned int)(quotient >> scale), 26);
        powered = dovi_fxPOW2Taylor((unsigned int)(smull(powered + 0x84000000u, 0x14640000u) >> 31),
                                    26, 30);
        base = dovi_fxPOW2Taylor(0x14640000u, 31, 27);
        factor = fixed_sign32(base);
        result = fixed_asr64(smull(base, powered), 27);
        for (i = 1; i < scale; ++i)
            result = (result * factor) >> (i + 1u == scale ? 26 : 27);
    } else if ((quotient >> 47) == 0) {
        scale = (quotient >> 41) == 0   ? 10u
                : (quotient >> 43) == 0 ? 12u
                : (quotient >> 45) == 0 ? 14u
                                        : 16u;
        result = dovi__powInSTaylor(quotient, scale);
    } else
        result = 0x59b5885411ull;
    return pq_forward_finish(result, (int)(linear >> 63));
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
    /* STB 2.6 control saturates the same rounded endpoint to 4095. */
    return linear_to_pq12(luminance, pq_l2pqLutX, pq_l2pqLutA, pq_l2pqLutB, 4095u);
}

unsigned int dovi_FindNodeIdxL2PQS(unsigned int value)
{
    return pq12_node(value, pq_l2pqLutX);
}

unsigned int dovi_LToPQ16(unsigned int value)
{
    unsigned int index;
    unsigned long long product = lut_product(value, &index);
    return pq_l2pqLutB[index] + (unsigned int)(product >> 25);
}

unsigned int dovi_LToPQ18(unsigned int value)
{
    unsigned int index;
    unsigned long long product = lut_product(value, &index);
    return (pq_l2pqLutB[index] << 2) + (unsigned int)(product >> 23);
}

unsigned long long dovi_L2Pq(unsigned long long value)
{
    unsigned long long scaled = value << 7;
    unsigned int magnitude = fixed_less64(scaled, 0x138800000000ull)
                                 ? dovi_powTaylor((unsigned int)sdiv64(scaled, 10000), 0x14640000u)
                                 : 0x14640000u;
    return pq_forward_finish(fixed_sign32(magnitude), 0);
}

unsigned long long dovi_Pq2LX(unsigned int value)
{
    return pq_inverse(value, 1);
}

unsigned long long dovi_Pq2L(unsigned int value)
{
    return pq_inverse(value, 0);
}

unsigned long long dovi_PQ2LXLut(unsigned int value)
{
    unsigned int magnitude = value >> 31 ? 0u - value : value;
    unsigned int coordinate = (unsigned int)((smull(magnitude, 0x2aaabu) + 0x20000ull) >> 18);
    unsigned int index, next;
    unsigned long long result;
    if (!fixed_less32(coordinate, 65535u))
        coordinate = 65535u;
    if (coordinate & 0xf0000u)
        pq_trap(0x5512);
    index = (coordinate >> 4) & 65535u;
    if (index > 4095u)
        pq_trap(1);
    result = pq_Pq2LXLut[index];
    next = (coordinate >> 4) << 16;
    if (next != 0xfff0000u) {
        next = (unsigned int)fixed_asr64(fixed_sign32(next), 16) + 1u;
        if (next > 4095u)
            pq_trap(0x5512);
        result += fixed_asr64((pq_Pq2LXLut[next] - result) * (coordinate & 15u), 4);
    }
    return value >> 31 ? 0ull - result : result;
}

unsigned int dovi_L2PQXLut(unsigned long long value)
{
    unsigned long long magnitude = value >> 63 ? 0ull - value : value;
    unsigned int index = fixed_less64(magnitude, 0xd1db96full) ? 0u : 256u, step, candidate, result;
    if (magnitude != 0xd1db96full) {
        for (step = 128; step != 0; step >>= 1) {
            candidate = index + step;
            if (!fixed_less64(magnitude, pq_l2pqXLutX[candidate])) {
                index = candidate;
                if (magnitude == pq_l2pqXLutX[candidate])
                    break;
            }
        }
    }
    if (index >= 512u)
        pq_trap(0x5512);
    result = pq_l2pqXLutB[index] +
             (unsigned int)fixed_asr64((magnitude - pq_l2pqXLutX[index]) * pq_l2pqXLutA[index], 62);
    return value >> 63 ? 0u - result : result;
}
