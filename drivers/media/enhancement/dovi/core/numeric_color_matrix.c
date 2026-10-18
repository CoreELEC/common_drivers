/* Fixed-point numeric color matrix operations. */
#include "numeric_color_matrix.h"
#include "numeric_common.h"
#include "matrix_arithmetic.h"
#include "fixed_arithmetic.h"
#include "color_primaries.h"
#include "color_basis.h"
#include "control_memory.h"
#include "numeric_tables.h"

static unsigned int de_left(const void *matrix, unsigned int matrix_scale, unsigned int cross_talk,
                            void *output, int rounded)
{
    unsigned long long temporary[9];
    unsigned int denominator = 0x8000u - 3 * cross_talk, round = fixed_asr32(denominator + 1, 1),
                 row, col;
    for (row = 0; row < 3; row++) {
        unsigned int sum = numeric_load_sign_extended16_at(matrix, row * 3) +
                           numeric_load_sign_extended16_at(matrix, row * 3 + 1) +
                           numeric_load_sign_extended16_at(matrix, row * 3 + 2);
        for (col = 0; col < 3; col++) {
            unsigned long long value =
                fixed_sign32(numeric_load_sign_extended16_at(matrix, row * 3 + col)) * 0x8000 -
                numeric_multiply_signed32(sum, cross_talk);
            unsigned long long bias =
                fixed_less64(0, value) ? fixed_sign32(round) : 0ull - fixed_sign32(round);
            temporary[row * 3 + col] = fixed_sign32(
                (unsigned int)dovi_sdiv(value * 0x1000 + (rounded ? bias : 0), denominator));
        }
    }
    return rounded ? dovi_Adjust64M33ScaleTo16M33Rounded(temporary, matrix_scale + 12, output)
                   : dovi_Adjust64M33ScaleTo16M33(temporary, matrix_scale + 12, output);
}

static void apply_right(const void *matrix, unsigned int cross_talk,
                        unsigned long long temporary[9])
{
    unsigned int row, col, factor = 0x8000 - 3 * cross_talk;
    for (col = 0; col < 3; col++) {
        unsigned int sum = numeric_load_sign_extended16_at(matrix, col) +
                           numeric_load_sign_extended16_at(matrix, col + 3) +
                           numeric_load_sign_extended16_at(matrix, col + 6);
        for (row = 0; row < 3; row++)
            temporary[row * 3 + col] =
                numeric_multiply_signed32(numeric_load_sign_extended16_at(matrix, row * 3 + col),
                                          factor) +
                numeric_multiply_signed32(sum, cross_talk);
    }
}

static void xyz_to_lms(const unsigned long long matrix[9], unsigned long long temporary[9],
                       int table)
{
    unsigned int col;
    for (col = 0; col < 3; col++) {
        temporary[col] = fixed_asr64(matrix[col] * 0x6676031 + matrix[col + 3] * 0xb524d33 -
                                         matrix[col + 6] * 0x14afacb,
                                     23);
        temporary[col + 3] =
            fixed_asr64(0ull - matrix[col] * 0x39eeac2 + matrix[col + 3] * 0x12a521f6 +
                            matrix[col + 6] * 0xbb30bc,
                        23);
        temporary[col + 6] = table && col != 1 ? (matrix[col + 6] * 0xeb10ca5) >> 23
                                               : fixed_asr64(matrix[col + 6] * 0xeb10ca5, 23);
    }
}

void dovi_Yuv2RgbOffsetRgbFxp(const void *matrix, unsigned int y_offset, unsigned int uv_offset,
                              void *output)
{
    const unsigned char *input = matrix;
    unsigned char *out = output;
    unsigned int i;
    for (i = 0; i < 3u; ++i) {
        unsigned int y = fixed_sign16(fixed_load_le16(input + i * 6u));
        unsigned int uv = fixed_sign16(fixed_load_le16(input + i * 6u + 2u)) +
                          fixed_sign16(fixed_load_le16(input + i * 6u + 4u));
        unsigned long long value = fixed_sign32(y) * y_offset + fixed_sign32(uv) * uv_offset;
        fixed_store_le32(out + i * 4u, (unsigned int)fixed_asr64(value, 16u));
    }
}

unsigned int dovi_GetRgb2LmsByDefM33(unsigned int definition, void *output)
{
    return dovi_GetRgb2LmsM33(definition, output);
}

unsigned int dovi_GetLms2RgbByDefM33Fxp(unsigned int definition, void *output)
{
    unsigned int scale = dovi_GetRgb2LmsM33(definition, output);
    return scale - dovi_InvM33(output, output, scale, scale);
}

unsigned int dovi_GetRgb2LmsByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output)
{
    unsigned int rx = raw_rx;
    unsigned int ry = raw_ry;
    unsigned int gx = raw_gx;
    unsigned int gy = raw_gy;
    unsigned int bx = raw_bx;
    unsigned int by = raw_by;
    unsigned int wx = raw_wx;
    unsigned int wy = raw_wy;
    unsigned int raw[9] = {0}, i;
    unsigned long long matrix[9], temporary[9];
    dovi_GetRgb2XyzM33ByPrims(rx, ry, gx, gy, bx, by, wx, wy, raw);
    for (i = 0; i < 9; i++)
        matrix[i] = fixed_sign32(raw[i]);
    xyz_to_lms(matrix, temporary, 0);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 31, output);
}

unsigned int dovi_GetLms2RgbByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output)
{
    unsigned int scale = dovi_GetRgb2LmsByPrimsM33Fxp(
        raw_rx, raw_ry, raw_gx, raw_gy, raw_bx, raw_by, raw_wx, raw_wy, output);
    return scale - dovi_InvM33(output, output, scale, scale);
}

unsigned int dovi_GetRgb2LmsCtWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       unsigned int raw_cross_talk, const void *white_point,
                                       unsigned int white_point_scale, void *output)
{
    unsigned int cross_talk = raw_cross_talk;
    unsigned long long temporary[9];
    unsigned int i, white[3], result;
    apply_right(matrix, cross_talk, temporary);
    result = dovi_Adjust64M33ScaleTo16M33Rounded(temporary, matrix_scale + 15, output);
    for (i = 0; i < 3; i++)
        white[i] = numeric_load32_at(white_point, i);
    for (i = 0; i < 9; i++)
        fixed_store_le16(
            (unsigned char *)output + i * 2,
            (unsigned int)dovi_sdiv(fixed_sign32(numeric_load_sign_extended16_at(output, i))
                                        << (white_point_scale & 63),
                                    white[i / 3]));
    return result;
}

unsigned int dovi_GetLms2RgbCtWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       unsigned int raw_cross_talk, unsigned long long unused,
                                       const void *white_point, unsigned int white_point_scale,
                                       void *output)
{
    unsigned int cross_talk = raw_cross_talk;
    unsigned int result = dovi_DE_CT2LEFT_rounded(matrix, matrix_scale, cross_talk, output), col,
                 row;
    (void)unused;
    for (col = 0; col < 3; col++)
        for (row = 0; row < 3; row++)
            fixed_store_le16((unsigned char *)output + (row * 3 + col) * 2,
                             numeric_saturate16(fixed_asr64(
                                 numeric_multiply_signed32(
                                     numeric_load32_at(white_point, col),
                                     numeric_load_sign_extended16_at(output, row * 3 + col)),
                                 white_point_scale)));
    return result;
}

unsigned int dovi_GetRgb2LmsWpByDefV3Fxp(unsigned int definition, void *output)
{
    unsigned char matrix[18] = {0};
    unsigned int scale = dovi_GetRgb2LmsByDefM33(definition, matrix);
    dovi_GetRgb2LmsWpV3(matrix, output);
    return scale;
}

unsigned int dovi_GetRgb2LmsWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                     const void *white_point, unsigned int white_point_scale,
                                     void *output)
{
    long long temporary[9];
    unsigned int row, col;
    for (row = 0; row < 3; ++row) {
        int white = numeric_load_signed32_at(white_point, row);
        for (col = 0; col < 3; ++col)
            temporary[row * 3 + col] =
                white ? numeric_divide64(numeric_shift_left64(
                                             numeric_load_signed16_at(matrix, row * 3 + col), 16),
                                         white)
                      : 0;
    }
    return dovi_Adjust64M33ScaleTo16M33(temporary, matrix_scale - white_point_scale + 16, output);
}

unsigned int dovi_GetRgb2LmsWpCtM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       const void *white_point, unsigned int white_point_scale,
                                       unsigned int raw_cross_talk, void *output)
{
    unsigned int cross_talk = raw_cross_talk;
    unsigned long long temporary[9], columns[3] = {0};
    unsigned int white[3], row, col, factor = 0x8000 - 3 * cross_talk;
    for (row = 0; row < 3; row++)
        white[row] = numeric_load32_at(white_point, row);
    if (white[0] && white[1] && white[2])
        for (col = 0; col < 3; col++)
            for (row = 0; row < 3; row++)
                columns[col] +=
                    fixed_raw_divide64(fixed_sign32(numeric_load32_at(matrix, row * 3 + col)) << 16,
                                       fixed_sign32(white[row]));
    for (row = 0; row < 3; row++)
        for (col = 0; col < 3; col++)
            temporary[row * 3 + col] =
                white[row]
                    ? fixed_raw_divide64((fixed_sign32(factor) << 16) *
                                             fixed_sign32(numeric_load32_at(matrix, row * 3 + col)),
                                         fixed_sign32(white[row])) +
                          columns[col] * fixed_sign32(cross_talk)
                    : 0;
    return dovi_Adjust64M33ScaleTo16M33(temporary, matrix_scale - white_point_scale + 31, output);
}

unsigned int dovi_GetLms2RgbWpCtM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       const void *white_point, unsigned int white_point_scale,
                                       unsigned int raw_cross_talk, void *output)
{
    unsigned int cross_talk = raw_cross_talk;
    unsigned long long temporary[9];
    unsigned int row, col, denominator = 0x8000 - 3 * cross_talk, white[3];
    for (col = 0; col < 3; col++)
        white[col] = numeric_load32_at(white_point, col);
    for (row = 0; row < 3; row++) {
        unsigned int sum = numeric_load32_at(matrix, row * 3) * white[0] +
                           numeric_load32_at(matrix, row * 3 + 1) * white[1] +
                           numeric_load32_at(matrix, row * 3 + 2) * white[2];
        for (col = 0; col < 3; col++)
            temporary[row * 3 + col] = fixed_raw_divide64(
                (numeric_multiply_signed32(white[col], numeric_load32_at(matrix, row * 3 + col)) *
                     0x8000 -
                 numeric_multiply_signed32(sum, cross_talk)) *
                    0x10000,
                fixed_sign32(denominator));
    }
    return dovi_Adjust64M33ScaleTo16M33(temporary, matrix_scale + white_point_scale + 16, output);
}

unsigned int dovi_DE_CT2LEFT(const void *matrix, unsigned int matrix_scale, unsigned int cross_talk,
                             void *output)
{
    return de_left(matrix, matrix_scale, cross_talk, output, 0);
}

unsigned int dovi_DE_CT2LEFT_rounded(const void *matrix, unsigned int matrix_scale,
                                     unsigned int cross_talk, void *output)
{
    return de_left(matrix, matrix_scale, cross_talk, output, 1);
}

unsigned int dovi_RE_DE_CT2LEFT_M33(void *output, unsigned int from, unsigned int to)
{
    unsigned long long temporary[9];
    unsigned int denominator = 0x8000 - 3 * to, i;
    unsigned int diagonal = (unsigned int)dovi_sdiv(
        fixed_sign32((to + 2 * from) * 0xffff8000u + 0x40000000), denominator);
    unsigned int off = (unsigned int)dovi_sdiv(fixed_sign32((from - to) * 0x8000), denominator);
    for (i = 0; i < 9; i++)
        temporary[i] = fixed_sign32(i % 4 ? off : diagonal);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 15, output);
}

unsigned int dovi_APPLY_CT2RIGHT(const void *matrix, unsigned int matrix_scale,
                                 unsigned int cross_talk, void *output)
{
    unsigned long long temporary[9];
    apply_right(matrix, cross_talk, temporary);
    return dovi_Adjust64M33ScaleTo16M33Rounded(temporary, matrix_scale + 15, output);
}

unsigned int dovi_RE_CT2RIGHT_M33(void *output, unsigned int old, unsigned int cross_talk)
{
    unsigned long long temporary[9];
    unsigned int i, denominator = 0x8000 - 3 * old, factor = 0x8000 - 3 * cross_talk;
    unsigned int diagonal =
        (unsigned int)dovi_sdiv(numeric_multiply_signed32(factor, 0x8000 - old), denominator) +
        cross_talk;
    unsigned int off =
        (unsigned int)dovi_sdiv(numeric_multiply_signed32(factor, 0u - old), denominator) +
        cross_talk;
    for (i = 0; i < 9; i++)
        temporary[i] = fixed_sign32(i % 4 ? off : diagonal);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 15, output);
}

void dovi_GetRgb2XyzDiagByPrims(unsigned int rx, unsigned int ry, unsigned int gx, unsigned int gy,
                                unsigned int bx, unsigned int by, unsigned int wx, unsigned int wy,
                                void *output)
{
    unsigned int matrix[9] = {0};
    dovi_GetRgb2XyzM33ByPrims(rx, ry, gx, gy, bx, by, wx, wy, matrix);
    fixed_store_le32(output, matrix[0]);
    fixed_store_le32((unsigned char *)output + 4, matrix[4]);
    fixed_store_le32((unsigned char *)output + 8, matrix[8]);
}

void dovi_GetRgb2XyzDiagByV8Prims(const void *primaries, void *output)
{
    dovi_GetRgb2XyzDiagByPrims(numeric_load32_at(primaries, 0), numeric_load32_at(primaries, 1),
                               numeric_load32_at(primaries, 2), numeric_load32_at(primaries, 3),
                               numeric_load32_at(primaries, 4), numeric_load32_at(primaries, 5),
                               numeric_load32_at(primaries, 6), numeric_load32_at(primaries, 7),
                               output);
}

unsigned int dovi_GetRgb2LmsM33(unsigned int definition, void *output)
{
    unsigned long long matrix[9] = {0}, temporary[9];
    unsigned int i;
    if (definition < 7)
        for (i = 0; i < 9; i++)
            matrix[i] = numeric_xyz_defs[i * 7 + definition];
    else {
        dovi_control_os.printk(
            "\0010### ASSERTION FAILED %s: %s: %d: %s\n",
            "../dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmUtilFxp.c",
            "GetRgb2XyzM33", 0x46d, "rgbDef < CRgbDefNum");
        dovi_control_os.dump_stack();
    }
    xyz_to_lms(matrix, temporary, 1);
    return dovi_Adjust64M33ScaleTo16M33(temporary, 31, output);
}

unsigned int dovi_GetRgb2LmsByV8PrimsM33Fxp(const void *primaries, void *output)
{
    return dovi_GetRgb2LmsByPrimsM33Fxp(
        numeric_load32_at(primaries, 0), numeric_load32_at(primaries, 1),
        numeric_load32_at(primaries, 2), numeric_load32_at(primaries, 3),
        numeric_load32_at(primaries, 4), numeric_load32_at(primaries, 5),
        numeric_load32_at(primaries, 6), numeric_load32_at(primaries, 7), output);
}

unsigned int dovi_GetLms2RgbByV8PrimsM33Fxp(const void *primaries, void *output)
{
    return dovi_GetLms2RgbByPrimsM33Fxp(
        numeric_load32_at(primaries, 0), numeric_load32_at(primaries, 1),
        numeric_load32_at(primaries, 2), numeric_load32_at(primaries, 3),
        numeric_load32_at(primaries, 4), numeric_load32_at(primaries, 5),
        numeric_load32_at(primaries, 6), numeric_load32_at(primaries, 7), output);
}
