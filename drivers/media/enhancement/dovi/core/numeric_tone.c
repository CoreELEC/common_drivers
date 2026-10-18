#include "numeric_tone.h"
#include "tone_mapping_math.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"

unsigned long long dovi_TmIFwd(unsigned int value, const void *context)
{
    return dovi_TMIcore((const unsigned char *)context + 0x1650, value << 11) >> 11;
}

unsigned long long dovi_TmSFwd(unsigned int value, const void *context)
{
    return dovi_TMScore((const unsigned char *)context + 0x1650, value << 11, 0, 0) >> 11;
}

unsigned int dovi_SmIFwd(unsigned int reference, unsigned int value, const void *context)
{
    return fixed_asr32(
        dovi_SMIcore((const unsigned char *)context + 0x1650, value << 11, reference << 11, 0), 11);
}

unsigned int dovi_SmSFwd(unsigned int value, const void *context)
{
    return fixed_asr32(dovi_SMScore((const unsigned char *)context + 0x1650, 0, value << 11, 0, 0),
                       11);
}

unsigned long long dovi_TmIBwd(unsigned int value, const void *context)
{
    return dovi_TMIcore((const unsigned char *)context + 0x1650, value << 11) >> 11;
}

unsigned int dovi_SmIBwd(unsigned int reference, unsigned int value, const void *context)
{
    return fixed_asr32(
        dovi_SMIcore((const unsigned char *)context + 0x1650, value << 11, reference << 11, 0), 11);
}

unsigned int dovi_SmSBwd(unsigned int value, const void *context)
{
    return fixed_asr32(dovi_SMScore((const unsigned char *)context + 0x1650, 0, value << 11, 1, 0),
                       11);
}

unsigned int dovi_TmSBwd(unsigned int value, const void *context)
{
    const unsigned char *c = context;
    unsigned int root = fixed_less32(0, value << 11) ? (unsigned int)dovi_ssqrt_s(value << 16) : 0;
    (void)dovi_sdiv(fixed_sign32(root) << 21, fixed_load_le32(c + 0x1764));
    return fixed_asr32(
        (unsigned int)(numeric_multiply_signed32(root, fixed_load_le32(c + 0x16cc)) >> 29) +
            0x4000000,
        11);
}

unsigned int dovi_HlgOotf_I(unsigned int value, const void *context)
{
    const unsigned char *c = context;
    unsigned long long light, power;
    unsigned int ratio;
    if (!fixed_load_le32(c + 0x18))
        return value;
    light = dovi_PQ2LXLut(value);
    ratio = (unsigned int)dovi_sdiv(light << 24, fixed_load_le32(c + 0x20));
    ratio = fixed_less32(0, ratio)
                ? (unsigned int)dovi_sdiv(dovi_PQ2LXLut(value) << 24, fixed_load_le32(c + 0x20))
                : 0;
    power = dovi_powInsExpsTaylor(ratio, 24, fixed_load_le32(c + 0x28), 14);
    return dovi_L2PQXLut((fixed_asr64(power, 7) * fixed_load_le32(c + 0x20)) >> 25);
}
