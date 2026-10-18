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

void get_rgb2_xyz_m33(unsigned int definition, void *output);

#endif
