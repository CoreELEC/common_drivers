#ifndef DOVI_COLOR_PRIMARIES_H
#define DOVI_COLOR_PRIMARIES_H

/* Matrix and vector buffers use unaligned little-endian storage.
 * Scalar parameter and result widths retain the native integer contracts. */

unsigned int dovi_GetRgb2YuvM33Fxp(unsigned int definition, void *output);
unsigned int dovi_GetRgb2YuvM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output);
unsigned int dovi_GetYuv2RgbM33Fxp(unsigned int definition, void *output);
unsigned int dovi_GetYuv2RgbM33NarrowFxp(unsigned int definition, unsigned int raw_bits,
                                         void *output);

/* Positive last_index accesses last_index+1 signed 32-bit values and indices.
 * Nonpositive signed last_index does nothing; arrays may overlap. */
void dovi_SortDescend(void *values, void *indices, unsigned int last_index);
unsigned long long dovi_Xy2Cct(const void *xy32, void *cct64);
void dovi_XyY2Xyz(unsigned int x, unsigned int y, unsigned int luminance, void *xyz64);
unsigned int dovi_GetPrimaries(unsigned int id, void *primaries32);
void dovi_GetRgb2XyzM33ByPrims(unsigned int rx, unsigned int ry, unsigned int gx, unsigned int gy,
                               unsigned int bx, unsigned int by, unsigned int wx, unsigned int wy,
                               void *matrix32);
void dovi_GetRgb2XyzM33ByV8Prims(const void *primaries32, void *matrix32);

#endif
