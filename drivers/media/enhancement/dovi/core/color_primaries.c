/* Fixed-point color primaries. */
#include "color_primaries.h"
#include "numeric_common.h"
#include "matrix_arithmetic.h"
#include "color_narrow.h"

static const int rgb2_xyz[7][9] = {
    {32653223, 17828657, 13302136, 15366222, 46421786, 5320854, 0, 3027507, 70057920},
    {29874840, 18598175, 11561694, 14058748, 48425437, 4624677, 0, 3158180, 60891589},
    {27675078, 23997078, 12111860, 14269962, 47994157, 4844744, 1297269, 7999026, 63789132},
    {42745531, 9705076, 11333410, 17629512, 45499680, 3979670, 0, 1883926, 71201501},
    {63924709, 0, 6286, 23083197, 48866399, -4840733, 0, 0, 67701112},
    {42815966, 14408531, 6559519, 19592686, 55287036, -7770858, 187789, -4498591, 77396230},
    {40734599, 11639933, 13460112, 20063310, 39354061, 7691492, 0, 4434260, 74992053},
};

unsigned int dovi_GetRgb2YuvM33Fxp(unsigned int definition, void *output)
{
    long long temporary[9];
    if (definition < 6) {
        int matrix[9], rr, bb;
        get_rgb2_xyz_m33(definition, matrix);
        temporary[0] = matrix[3];
        temporary[1] = matrix[4];
        temporary[2] = matrix[5];
        bb = numeric_subtract32(0x4000000, matrix[5]);
        rr = numeric_subtract32(0x4000000, matrix[3]);
        temporary[3] = -numeric_divide64(numeric_shift_left64(matrix[3] >> 1, 26), bb);
        temporary[4] = -numeric_divide64(numeric_shift_left64(matrix[4] >> 1, 26), bb);
        temporary[8] = -numeric_divide64(numeric_shift_left64(matrix[5] >> 1, 26), rr);
        temporary[7] = -numeric_divide64(numeric_shift_left64(matrix[4] >> 1, 26), rr);
    } else {
        temporary[0] = 0x1322d0e;
        temporary[1] = 0x2591687;
        temporary[2] = 0x74bc6a;
        temporary[3] = -0xacc919;
        temporary[4] = -0x15336e5;
        temporary[7] = -0x1acbc6f;
        temporary[8] = -0x53438f;
    }
    temporary[5] = 0x2000000;
    temporary[6] = 0x2000000;
    return dovi_Adjust64M33ScaleTo16M33(temporary, 26u, output);
}

unsigned int dovi_GetRgb2YuvM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output)
{
    int bits = fixed_signed32(raw_bits);
    short full[9], scale = dovi_GetRgb2YuvM33Fxp(definition, full);
    return rgb2_yuv_m33_full2_narrow(full, scale, bits, output);
}

unsigned int dovi_GetYuv2RgbM33Fxp(unsigned int definition, void *output)
{
    long long temporary[9];
    if (definition > 5) {
        temporary[0] = 0x4000000;
        temporary[1] = 0;
        temporary[2] = 0x59ba5e4;
        temporary[3] = 0x4000000;
        temporary[4] = -0x1606542;
        temporary[5] = -0x2db468a;
        temporary[6] = 0x4000000;
        temporary[7] = 0x716872c;
        temporary[8] = 0;
    } else {
        int matrix[9];
        get_rgb2_xyz_m33(definition, matrix);
        dovi_GetYuv2RgbFromKrKgKbM33(matrix[3], matrix[4], matrix[5], temporary);
    }
    return dovi_Adjust64M33ScaleTo16M33(temporary, 26u, output);
}

unsigned int dovi_GetYuv2RgbM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output)
{
    int bits = fixed_signed32(raw_bits);
    short full[9], scale = dovi_GetYuv2RgbM33Fxp(definition, full);
    return yuv2_rgb_m33_narrow2_full(full, scale, fixed_signed16((unsigned int)bits), output);
}

void get_rgb2_xyz_m33(unsigned int definition, void *output)
{
    unsigned int i;
    for (i = 0; i < 9; ++i)
        numeric_store32_at(output, i, definition < 7 ? rgb2_xyz[definition][i] : 0);
}
