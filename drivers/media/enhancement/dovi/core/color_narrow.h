#ifndef DOVI_COLOR_NARROW_H
#define DOVI_COLOR_NARROW_H

/* Matrix and vector buffers use unaligned little-endian storage.
 * Scalar parameter and result widths retain the native integer contracts. */
/* Narrow matrix functions write nine little-endian signed 16-bit values.
 * Masked shifts and signed divide retain the native behavior at unusual bit depths. */

void dovi_GetYuvRgbOffFxp(unsigned int range, unsigned int bits, void *output);
unsigned int dovi_GetLms2IptM33NarrowFxp(unsigned int bits, void *output);
unsigned int dovi_GetIpt2LmsM33NarrowFxp(unsigned int bits, void *output);
/* Writes nine little-endian int64s. If kg is zero, entries four and five retain their bytes. */
void dovi_GetYuv2RgbFromKrKgKbM33(unsigned int kr, unsigned int kg, unsigned int kb, void *output);

unsigned int dovi_GetLms2IctcpIoM33Narrow(unsigned int depth, void *matrix);
unsigned int dovi_GetIctcpIo2LmsM33Narrow(unsigned int depth, void *matrix);

#endif
