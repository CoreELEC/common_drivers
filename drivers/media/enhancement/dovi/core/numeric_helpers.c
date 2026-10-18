/* Fixed-point range, normalization and resampling helpers. */
#include "numeric_helpers.h"
#include "fixed_bits.h"
#include "color_narrow.h"
#include "fixed_access.h"

void dovi_GetRange(unsigned int bits, unsigned int range, void *minimum, void *extent)
{
    unsigned int unit;

    if (range == 0u) {
        unit = kernel_shift_left(1u, bits - 8u) & 65535u;
        fixed_store_le16(minimum, unit * 16u);
        fixed_store_le16(extent, unit * 235u - fixed_load_le16(minimum));
    } else if (range == 2u) {
        unit = kernel_shift_left(1u, bits - 8u) & 65535u;
        fixed_store_le16(minimum, unit);
        fixed_store_le16(extent, kernel_shift_left(1u, bits - 10u) * 1019u - unit);
    } else {
        fixed_store_le16(minimum, 0u);
        fixed_store_le16(extent, kernel_shift_left(1u, bits) - 1u);
    }
}

void dovi_GetCvmBypassNormParam(unsigned int raw_bits, unsigned int range, void *offset,
                                void *scale)
{
    int bits = fixed_signed32(raw_bits);
    dovi_GetYuvRgbOffFxp(range, bits, offset);
    if (range != 0u)
        fixed_store_le32(
            scale, bits == 0
                       ? 0x7fffffff
                       : kernel_divide_max((int)(kernel_shift_left(1u, (unsigned int)bits) - 1u)));
    else
        fixed_store_le32(scale, bits < 8 ? 0x12b404a
                                         : kernel_divide_max((int)kernel_shift_left(
                                               219u, (unsigned int)bits - 8u)));
}

void dovi_CommitKsUs(unsigned int chroma, const void *bits, void *output)
{
    unsigned char *out = output;
    unsigned int depth;

    out[0] = chroma == 0u ? 0 : chroma == 1u ? 1 : 2;
    fixed_store_le16(out + 2u, 0u);
    depth = fixed_load_le32((const unsigned char *)bits + (0u));
    fixed_store_le16(out + 6u, 1u);
    fixed_store_le16(out + 4u, kernel_shift_left(1u, depth) - 1u);
    fixed_store_le16(out + 8u, 8u);
    fixed_store_le16(out + 10u, 64u);
    fixed_store_le16(out + 42u, 4u);
    fixed_store_le16(out + 26u, 192u);
    fixed_store_le16(out + 12u, 192u);
    fixed_store_le16(out + 28u, 64u);
    fixed_store_le16(out + 44u, 12u);
    fixed_store_le16(out + 46u, 22u);
    fixed_store_le16(out + 48u, 94u);
    fixed_store_le16(out + 50u, 0xfdf4u);
    fixed_store_le16(out + 52u, 0x998u);
    fixed_store_le16(out + 54u, 0x998u);
    fixed_store_le16(out + 56u, 0xfdf4u);
    fixed_store_le16(out + 58u, 94u);
    fixed_store_le16(out + 60u, 22u);
}
