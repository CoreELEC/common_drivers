#ifndef DOVI_MATRIX_ARITHMETIC_H
#define DOVI_MATRIX_ARITHMETIC_H

/* Matrix and vector buffers use unaligned little-endian storage.
 * Scalar parameter and result widths retain the native integer contracts. */
/* Input/output storage may overlap; stores retain the native order. */

unsigned int dovi_FindInt64M33Precision(const void *input);
short dovi_Adjust64M33ScaleTo16M33(const void *input, short scale, void *output);
/* Scale arguments use signed 16-bit values. Singular matrices return the
 * 16-bit pattern 0xffff without modifying output; otherwise return the reduction count. */
short dovi_InvM33(const void *input, void *output, unsigned int input_scale,
                  unsigned int output_scale);

short dovi_M33TimesM33(const void *first, short first_scale, const void *second, short second_scale,
                       void *output);

#endif
