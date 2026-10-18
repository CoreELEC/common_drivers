/* Fixed-point conversion between full and narrow color ranges. */
#include "fixed_access.h"
#include "color_narrow.h"
#include "color_basis_impl.h"
#include "matrix_arithmetic.h"

static unsigned int sdiv32(unsigned int a, unsigned int b)
{
    return fixed_raw_divide32(a, b);
}

static unsigned long long product(unsigned int value, int coefficient)
{
    return fixed_sign32(value) * (unsigned long long)(long long)coefficient;
}

static unsigned int ratio(unsigned int depth, unsigned int width, unsigned int inverse)
{
    unsigned int mask = ~(0xffffffffu << (depth & 31));
    unsigned int shift = (depth - 8u) & 31;
    return inverse ? sdiv32(mask << 16, width << shift) : sdiv32((width << 16) << shift, mask);
}

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
    unsigned int factor = ratio(fixed_sign16(bits), 219, 0), i;
    unsigned long long temporary[9];
    for (i = 0; i < 9; ++i)
        temporary[i] = product(factor, color_lms2_ipt[i]);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 28, output);
}

unsigned int dovi_GetIpt2LmsM33NarrowFxp(unsigned int bits, void *output)
{
    unsigned int factor = ratio(fixed_sign16(bits), 219, 1), i;
    unsigned long long temporary[9];
    for (i = 0; i < 9; ++i)
        temporary[i] = product(factor, color_ipt2_lms[i]);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 31, output);
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

unsigned int dovi_GetLms2IctcpIoM33Narrow(unsigned int depth, void *matrix)
{
    unsigned int y, c;
    unsigned long long values[9];
    if ((depth >> 31) || depth < 9)
        depth = 8;
    y = ratio(depth, 219, 0);
    c = ratio(depth, 224, 0);
    values[0] = product(y, 2048);
    values[1] = values[0];
    values[2] = 0;
    values[3] = product(c, 6610);
    values[4] = product(c, -13613);
    values[5] = product(c, 7003);
    values[6] = product(c, 17933);
    values[7] = product(c, -17390);
    values[8] = product(c, -543);
    return dovi_Adjust64M33ScaleTo16M33(values, 28, matrix);
}

unsigned int dovi_GetIctcpIo2LmsM33Narrow(unsigned int depth, void *matrix)
{
    unsigned int y, c;
    unsigned long long values[9];
    depth = fixed_sign16(depth);
    if ((depth >> 31) || depth < 9)
        depth = 8;
    y = ratio(depth, 219, 1);
    c = ratio(depth, 224, 1);
    values[0] = product(y, 32767);
    values[1] = product(c, 282);
    values[2] = product(c, 3638);
    values[3] = values[0];
    values[4] = product(c, -282);
    values[5] = product(c, -3638);
    values[6] = values[0];
    values[7] = product(c, 18351);
    values[8] = product(c, -10506);
    return dovi_Adjust64M33ScaleTo16M33(values, 31, matrix);
}
