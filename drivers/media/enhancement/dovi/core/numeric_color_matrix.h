#ifndef DOVI_NUMERIC_COLOR_MATRIX_H
#define DOVI_NUMERIC_COLOR_MATRIX_H

/* Matrix and vector buffers use unaligned little-endian storage.
 * Scalar parameter and result widths retain the native integer contracts. */

void dovi_Yuv2RgbOffsetRgbFxp(const void *matrix, unsigned int y_offset, unsigned int uv_offset,
                              void *output);
unsigned int dovi_GetRgb2LmsByDefM33(unsigned int definition, void *output);
unsigned int dovi_GetLms2RgbByDefM33Fxp(unsigned int definition, void *output);
unsigned int dovi_GetRgb2LmsByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output);
unsigned int dovi_GetLms2RgbByPrimsM33Fxp(unsigned int raw_rx, unsigned int raw_ry,
                                          unsigned int raw_gx, unsigned int raw_gy,
                                          unsigned int raw_bx, unsigned int raw_by,
                                          unsigned int raw_wx, unsigned int raw_wy, void *output);
unsigned int dovi_GetRgb2LmsCtWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       unsigned int raw_cross_talk, const void *white_point,
                                       unsigned int white_point_scale, void *output);
unsigned int dovi_GetLms2RgbCtWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       unsigned int raw_cross_talk, unsigned long long unused,
                                       const void *white_point, unsigned int white_point_scale,
                                       void *output);
unsigned int dovi_GetRgb2LmsWpByDefV3Fxp(unsigned int definition, void *output);
unsigned int dovi_GetRgb2LmsWpM33Fxp(const void *matrix, unsigned int matrix_scale,
                                     const void *white_point, unsigned int white_point_scale,
                                     void *output);
unsigned int dovi_GetRgb2LmsWpCtM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       const void *white_point, unsigned int white_point_scale,
                                       unsigned int raw_cross_talk, void *output);
unsigned int dovi_GetLms2RgbWpCtM33Fxp(const void *matrix, unsigned int matrix_scale,
                                       const void *white_point, unsigned int white_point_scale,
                                       unsigned int raw_cross_talk, void *output);

unsigned int dovi_DE_CT2LEFT(const void *matrix, unsigned int matrix_scale, unsigned int cross_talk,
                             void *output);
unsigned int dovi_DE_CT2LEFT_rounded(const void *matrix, unsigned int matrix_scale,
                                     unsigned int cross_talk, void *output);
unsigned int dovi_RE_DE_CT2LEFT_M33(void *output, unsigned int from, unsigned int to);
unsigned int dovi_APPLY_CT2RIGHT(const void *matrix, unsigned int matrix_scale,
                                 unsigned int cross_talk, void *output);
unsigned int dovi_RE_CT2RIGHT_M33(void *output, unsigned int old, unsigned int cross_talk);
void dovi_GetRgb2XyzDiagByPrims(unsigned int rx, unsigned int ry, unsigned int gx, unsigned int gy,
                                unsigned int bx, unsigned int by, unsigned int wx, unsigned int wy,
                                void *output);
void dovi_GetRgb2XyzDiagByV8Prims(const void *primaries, void *output);
unsigned int dovi_GetRgb2LmsM33(unsigned int definition, void *output);
unsigned int dovi_GetRgb2LmsByV8PrimsM33Fxp(const void *primaries, void *output);
unsigned int dovi_GetLms2RgbByV8PrimsM33Fxp(const void *primaries, void *output);

#endif
