/* Fixed-point conversion between full and narrow color ranges. */
#include "color_narrow.h"
#include "color_basis_impl.h"
#include "numeric_common.h"
#include "matrix_arithmetic.h"

void dovi_GetYuvRgbOffFxp(unsigned int range, unsigned int bits, void *output)
{
    unsigned char *out = output;
    unsigned int chroma = 1u << ((bits - 1u) & 31u);
    fixed_store_le32(out, range ? 0u : 1u << ((bits - 4u) & 31u));
    fixed_store_le32(out + 4u, chroma);
    fixed_store_le32(out + 8u, chroma);
}

unsigned int dovi_GetLms2IptM33NarrowFxp(unsigned int bits, void *output)
{
    return rgb2_yuv_m33_full2_narrow(color_lms2_ipt, 12, fixed_signed16(bits), output);
}

unsigned int dovi_GetIpt2LmsM33NarrowFxp(unsigned int bits, void *output)
{
    return yuv2_rgb_m33_narrow2_full(color_ipt2_lms, 15, fixed_signed16(bits), output);
}

void dovi_GetYuv2RgbFromKrKgKbM33(unsigned int kr, unsigned int kg, unsigned int kb, void *output)
{
    unsigned char *out = output;
    unsigned int red = (unsigned int)kr, green = (unsigned int)kg, blue = (unsigned int)kb;
    fixed_store_le64(out, 0x4000000);
    fixed_store_le64(out + 8u, 0);
    fixed_store_le64(out + 16u, fixed_sign32((0x4000000u - red) * 2u));
    fixed_store_le64(out + 24u, 0x4000000);
    if (green != 0u) {
        unsigned long long red_product = fixed_sign32(red) * fixed_sign32(0x4000000u - red) * 2ull;
        unsigned long long blue_product =
            fixed_sign32(blue) * fixed_sign32(0x4000000u - blue) * 2ull;
        fixed_store_le64(out + 32u, fixed_raw_divide64(0ull - blue_product, fixed_sign32(green)));
        fixed_store_le64(out + 40u, fixed_raw_divide64(0ull - red_product, fixed_sign32(green)));
    }
    fixed_store_le64(out + 48u, 0x4000000);
    fixed_store_le64(out + 56u, fixed_sign32(0x8000000u - 2u * blue));
    fixed_store_le64(out + 64u, 0);
}

short rgb2_yuv_m33_full2_narrow(const void *input, short scale, int bits, void *output)
{
    int y = 0xdbdb, uv = 0xe0e0;
    unsigned int i;
    long long temporary[9];
    if (bits >= 8) {
        int denominator = fixed_signed32((1u << ((unsigned int)bits & 31u)) - 1u);
        y = numeric_divide32(fixed_signed32(0xdb0000u << (((unsigned int)bits - 8u) & 31u)),
                             denominator);
        uv = numeric_divide32(fixed_signed32(0xe00000u << (((unsigned int)bits - 8u) & 31u)),
                              denominator);
    }
    for (i = 0; i < 9; ++i)
        temporary[i] = (long long)numeric_load_signed16_at(input, i) * (i < 3 ? y : uv);
    return dovi_Adjust64M33ScaleTo16M33(temporary, fixed_signed16((unsigned int)scale + 16u),
                                        output);
}

short yuv2_rgb_m33_narrow2_full(const void *input, short scale, short bits, void *output)
{
    unsigned int i, shift = ((unsigned int)(int)bits - 8u) & 31u;
    int numerator = fixed_signed32(((1u << ((unsigned int)(int)bits & 31u)) - 1u) * 0x10000u);
    int y = numeric_divide32(numerator, fixed_signed32(0xdbu << shift));
    int uv = numeric_divide32(numerator, fixed_signed32(0xe0u << shift));
    long long temporary[9];
    for (i = 0; i < 9; ++i)
        temporary[i] = (long long)numeric_load_signed16_at(input, i) * (i % 3 ? uv : y);
    return dovi_Adjust64M33ScaleTo16M33(temporary, fixed_signed16((unsigned int)scale + 16u),
                                        output);
}
