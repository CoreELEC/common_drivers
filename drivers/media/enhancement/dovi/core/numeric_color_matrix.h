#ifndef DOVI_NUMERIC_COLOR_MATRIX_H
#define DOVI_NUMERIC_COLOR_MATRIX_H

/* Matrix and vector buffers use unaligned little-endian storage.
 * Scalar parameter and result widths retain the native integer contracts. */

void dovi_Yuv2RgbOffsetRgbFxp(const void *matrix, unsigned int y_offset, unsigned int uv_offset,
                              void *output);
unsigned int dovi_GetRgb2LmsByDefM33(unsigned int definition, void *output);
short dovi_GetLms2RgbByDefM33Fxp(unsigned int definition, void *output);
unsigned int dovi_GetRgb2LmsByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output);
short dovi_GetLms2RgbByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry, unsigned int raw_gx,
                                   unsigned int raw_gy, unsigned int raw_bx, unsigned int raw_by,
                                   unsigned int raw_wx, unsigned int raw_wy, void *output);
short dovi_GetRgb2LmsCtWpM33Fxp(const void *matrix, short matrix_scale, unsigned int raw_cross_talk,
                                const void *white_point, short white_point_scale, void *output);
short dovi_GetLms2RgbCtWpM33Fxp(const void *matrix, short matrix_scale, unsigned int raw_cross_talk,
                                const void *white_point, short white_point_scale, void *output);
unsigned int dovi_GetRgb2LmsWpByDefV3Fxp(unsigned int definition, void *output);
short dovi_GetRgb2LmsWpM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                              short white_point_scale, void *output);
short dovi_GetRgb2LmsWpCtM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                                short white_point_scale, unsigned int raw_cross_talk, void *output);
short dovi_GetLms2RgbWpCtM33Fxp(const void *matrix, short matrix_scale, const void *white_point,
                                short white_point_scale, unsigned int raw_cross_talk, void *output);

short m33_times_m33_as(const void *input, void *output);
short dovi_GetLms2RgbCtWpM33Fxp32(const void *matrix, short matrix_scale, int cross_talk,
                                  const void *white_point, short white_point_scale, void *output);

#endif
