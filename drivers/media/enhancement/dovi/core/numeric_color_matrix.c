/* Fixed-point numeric color matrix operations. */
#include "numeric_color_matrix.h"
#include "numeric_common.h"
#include "matrix_arithmetic.h"
#include "color_primaries.h"
#include "color_basis.h"

static const int xyz2_lms[9] = {
    0x6676031, 0xb524d33, -0x14afacb, -0x39eeac2, 0x12a521f6, 0xbb30bc, 0, 0,
    0xeb10ca5};

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
    int matrix[9];
    get_rgb2_xyz_m33(definition, matrix);
    return m33_times_m33_as(matrix, output);
}

short dovi_GetLms2RgbByDefM33Fxp(unsigned int definition, void *output)
{
    short scale = dovi_GetRgb2LmsByDefM33(definition, output);
    return fixed_signed16((unsigned int)scale -
                          (unsigned int)dovi_InvM33(output, output, scale, scale));
}

unsigned int dovi_GetRgb2LmsByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output)
{
    int rx = fixed_signed32(raw_rx);
    int ry = fixed_signed32(raw_ry);
    int gx = fixed_signed32(raw_gx);
    int gy = fixed_signed32(raw_gy);
    int bx = fixed_signed32(raw_bx);
    int by = fixed_signed32(raw_by);
    int wx = fixed_signed32(raw_wx);
    int wy = fixed_signed32(raw_wy);
    int matrix[9], u, v, z, factors[3], coords[6];
    int rb = numeric_subtract32(rx, bx), gb = numeric_subtract32(gx, bx),
        rby = numeric_subtract32(ry, by), gby = numeric_subtract32(gy, by);
    int d = fixed_signed32(
        (unsigned int)(numeric_subtract64((long long)rb * gby, (long long)rby * gb) >> 26));
    unsigned int i;
    if (!d) {
        u = 0;
        v = 0;
        z = 0x4000000;
    } else {
        u = fixed_signed32((unsigned int)numeric_divide64(
            numeric_subtract64((long long)gby * numeric_subtract32(wx, bx),
                               (long long)gb * numeric_subtract32(wy, by)),
            d));
        v = fixed_signed32((unsigned int)numeric_divide64(
            numeric_subtract64((long long)rb * numeric_subtract32(wy, by),
                               (long long)rby * numeric_subtract32(wx, bx)),
            d));
        z = numeric_subtract32(numeric_subtract32(0x4000000, u), v);
    }
    factors[0] = fixed_signed32((unsigned int)numeric_divide64(numeric_shift_left64(u, 26), wy));
    factors[1] = fixed_signed32((unsigned int)numeric_divide64(numeric_shift_left64(v, 26), wy));
    factors[2] = fixed_signed32((unsigned int)numeric_divide64(numeric_shift_left64(z, 26), wy));
    coords[0] = rx;
    coords[1] = ry;
    coords[2] = gx;
    coords[3] = gy;
    coords[4] = bx;
    coords[5] = by;
    for (i = 0; i < 3; ++i) {
        matrix[i] = fixed_signed32(
            (unsigned int)((unsigned long long)((long long)factors[i] * coords[i * 2]) >> 26));
        matrix[3 + i] = fixed_signed32(
            (unsigned int)((unsigned long long)((long long)factors[i] * coords[i * 2 + 1]) >> 26));
        matrix[6 + i] = fixed_signed32(
            (unsigned int)((unsigned long long)((long long)factors[i] *
                                                numeric_subtract32(
                                                    numeric_subtract32(0x4000000, coords[i * 2]),
                                                    coords[i * 2 + 1])) >>
                           26));
    }
    return m33_times_m33_as(matrix, output);
}

short dovi_GetLms2RgbByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry, unsigned int raw_gx,
                                   unsigned int raw_gy, unsigned int raw_bx, unsigned int raw_by,
                                   unsigned int raw_wx, unsigned int raw_wy, void *output)
{
    short scale = dovi_GetRgb2LmsByPrimsM33Fxp(
        raw_rx, raw_ry, raw_gx, raw_gy, raw_bx, raw_by, raw_wx, raw_wy, output);
    return fixed_signed16((unsigned int)scale -
                          (unsigned int)dovi_InvM33(output, output, scale, scale));
}

short dovi_GetRgb2LmsCtWpM33Fxp(const void *matrix, short matrix_scale, unsigned int raw_cross_talk,
                                const void *white_point, short white_point_scale, void *output)
{
    int cross_talk = fixed_signed32(raw_cross_talk);
    unsigned int i, j;
    int factor = fixed_signed32(0x8000u - (unsigned int)cross_talk * 3u);
    long long columns[3], temporary[9];
    for (j = 0; j < 3; ++j)
        columns[j] = (long long)(numeric_load_signed16_at(matrix, j) +
                                 numeric_load_signed16_at(matrix, j + 3) +
                                 numeric_load_signed16_at(matrix, j + 6)) *
                     cross_talk;
    for (i = 0; i < 3; ++i) {
        int white = numeric_load_signed32_at(white_point, i);
        for (j = 0; j < 3; ++j)
            temporary[i * 3 + j] = numeric_divide64(
                numeric_shift_left64(
                    numeric_add64(columns[j],
                                  (long long)factor * numeric_load_signed16_at(matrix, i * 3 + j)),
                    16),
                white);
    }
    return dovi_Adjust64M33ScaleTo16M33(
        temporary,
        fixed_signed16((unsigned int)matrix_scale + 31u - (unsigned int)white_point_scale), output);
}

short dovi_GetLms2RgbCtWpM33Fxp(const void *matrix, short matrix_scale, unsigned int raw_cross_talk,
                                const void *white_point, short white_point_scale, void *output)
{
    int cross_talk = fixed_signed32(raw_cross_talk);
    unsigned int i, j;
    int factor = fixed_signed32(0x8000u - (unsigned int)cross_talk * 3u);
    long long rows[3], temporary[9];
    for (i = 0; i < 3; ++i)
        rows[i] = (long long)(numeric_load_signed16_at(matrix, i * 3) +
                              numeric_load_signed16_at(matrix, i * 3 + 1) +
                              numeric_load_signed16_at(matrix, i * 3 + 2)) *
                  cross_talk;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 3; ++j) {
            if (!factor)
                temporary[i * 3 + j] = 0;
            else {
                long long value = numeric_divide64(
                    numeric_shift_left64(
                        numeric_subtract64((long long)numeric_load_signed16_at(matrix, i * 3 + j) *
                                               0x8000,
                                           rows[i]),
                        16),
                    factor);
                temporary[i * 3 + j] =
                    numeric_multiply64(numeric_load_signed32_at(white_point, j), value);
            }
        }
    return dovi_Adjust64M33ScaleTo16M33(
        temporary,
        fixed_signed16((unsigned int)matrix_scale + 16u + (unsigned int)white_point_scale), output);
}

unsigned int dovi_GetRgb2LmsWpByDefV3Fxp(unsigned int definition, void *output)
{
    unsigned char matrix[18] = {0};
    unsigned int scale = dovi_GetRgb2LmsByDefM33(definition, matrix);
    dovi_GetRgb2LmsWpV3(matrix, output);
    return scale;
}

short dovi_GetRgb2LmsWpM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                              short white_point_scale, void *output)
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
    return dovi_Adjust64M33ScaleTo16M33(
        temporary,
        fixed_signed16((unsigned int)matrix_scale - (unsigned int)white_point_scale + 16u), output);
}

short dovi_GetRgb2LmsWpCtM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                                short white_point_scale, unsigned int raw_cross_talk, void *output)
{
    int cross_talk = fixed_signed32(raw_cross_talk);
    long long columns[3], temporary[9];
    unsigned int row, col;
    int factor = fixed_signed32(0x8000u - (unsigned int)cross_talk * 3u);
    int white0 = numeric_load_signed32_at(white_point, 0);
    for (col = 0; col < 3; ++col) {
        int white1, white2;
        if (!white0 || !(white1 = numeric_load_signed32_at(white_point, 1)) ||
            !(white2 = numeric_load_signed32_at(white_point, 2)))
            columns[col] = 0;
        else {
            long long sum = numeric_add64(
                numeric_divide64(numeric_shift_left64(numeric_load_signed32_at(matrix, col), 16),
                                 white0),
                numeric_divide64(
                    numeric_shift_left64(numeric_load_signed32_at(matrix, col + 3), 16), white1));
            sum = numeric_add64(
                sum,
                numeric_divide64(
                    numeric_shift_left64(numeric_load_signed32_at(matrix, col + 6), 16), white2));
            columns[col] = numeric_multiply64(cross_talk, sum);
        }
    }
    for (row = 0; row < 3; ++row) {
        int white = numeric_load_signed32_at(white_point, row);
        for (col = 0; col < 3; ++col)
            temporary[row * 3 + col] =
                white
                    ? numeric_add64(numeric_divide64(
                                        numeric_shift_left64(
                                            numeric_multiply64(factor, numeric_load_signed32_at(
                                                                           matrix, row * 3 + col)),
                                            16),
                                        white),
                                    columns[col])
                    : 0;
    }
    return dovi_Adjust64M33ScaleTo16M33(
        temporary,
        fixed_signed16((unsigned int)matrix_scale + 31u - (unsigned int)white_point_scale), output);
}

short dovi_GetLms2RgbWpCtM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                                short white_point_scale, unsigned int raw_cross_talk, void *output)
{
    int cross_talk = fixed_signed32(raw_cross_talk);
    long long rows[3], temporary[9];
    unsigned int row, col;
    int factor = fixed_signed32(0x8000u - (unsigned int)cross_talk * 3u);
    int white0 = numeric_load_signed32_at(white_point, 0),
        white1 = numeric_load_signed32_at(white_point, 1),
        white2 = numeric_load_signed32_at(white_point, 2);
    for (row = 0; row < 3; ++row) {
        unsigned int sum =
            (unsigned int)white1 * (unsigned int)numeric_load_signed32_at(matrix, row * 3 + 1) +
            (unsigned int)white0 * (unsigned int)numeric_load_signed32_at(matrix, row * 3) +
            (unsigned int)white2 * (unsigned int)numeric_load_signed32_at(matrix, row * 3 + 2);
        rows[row] = (long long)fixed_signed32(sum) * cross_talk;
    }
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            if (!factor)
                temporary[row * 3 + col] = 0;
            else {
                long long value = numeric_multiply64(
                    numeric_multiply64(numeric_load_signed32_at(matrix, row * 3 + col),
                                       numeric_load_signed32_at(white_point, col)),
                    0x8000);
                temporary[row * 3 + col] = numeric_divide64(
                    numeric_shift_left64(numeric_subtract64(value, rows[row]), 16), factor);
            }
        }
    }
    return dovi_Adjust64M33ScaleTo16M33(
        temporary,
        fixed_signed16((unsigned int)matrix_scale + 16u + (unsigned int)white_point_scale), output);
}

short m33_times_m33_as(const void *input, void *output)
{
    long long temporary[9];
    unsigned int row, col;
    for (row = 0; row < 3; ++row) {
        int a = xyz2_lms[row * 3], b = xyz2_lms[row * 3 + 1], c = xyz2_lms[row * 3 + 2];
        for (col = 0; col < 3; ++col)
            temporary[row * 3 + col] =
                numeric_add64(numeric_add64((long long)numeric_load_signed32_at(input, col + 3) * b,
                                            (long long)numeric_load_signed32_at(input, col) * a),
                              (long long)numeric_load_signed32_at(input, col + 6) * c) >>
                23;
    }
    return dovi_Adjust64M33ScaleTo16M33(temporary, 31, output);
}

short dovi_GetLms2RgbCtWpM33Fxp32(const void *matrix, short matrix_scale, int cross_talk,
                                  const void *white_point, short white_point_scale, void *output)
{
    long long rows[3], temporary[9];
    unsigned int row, col, reduction = 0;
    short precision;
    int factor = fixed_signed32(0x8000u - (unsigned int)cross_talk * 3u);
    for (row = 0; row < 3; ++row)
        rows[row] = (long long)(numeric_load_signed16_at(matrix, row * 3) +
                                numeric_load_signed16_at(matrix, row * 3 + 1) +
                                numeric_load_signed16_at(matrix, row * 3 + 2)) *
                    cross_talk;
    for (row = 0; row < 3; ++row) {
        for (col = 0; col < 3; ++col) {
            if (!factor)
                temporary[row * 3 + col] = 0;
            else {
                long long value = numeric_divide64(
                    numeric_shift_left64(
                        numeric_subtract64(
                            (long long)numeric_load_signed16_at(matrix, row * 3 + col) * 0x8000,
                            rows[row]),
                        16),
                    factor);
                temporary[row * 3 + col] =
                    numeric_multiply64(numeric_load_signed32_at(white_point, col), value);
            }
        }
    }
    precision = dovi_FindInt64M33Precision(temporary);
    if (precision > 30)
        reduction = (unsigned int)precision - 30u;
    for (row = 0; row < 9; ++row)
        numeric_store32_at(output, row, temporary[row] >> (reduction & 63u));
    return fixed_signed16((unsigned int)matrix_scale + 16u + (unsigned int)white_point_scale -
                          reduction);
}
