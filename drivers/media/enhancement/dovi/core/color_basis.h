#ifndef DOVI_COLOR_BASIS_H
#define DOVI_COLOR_BASIS_H

/* Byte-addressed color data requires no alignment. Copies load each element
 * before storing it; row sums load a whole row before storing the result. */
/* Copy three 32-bit little-endian elements in forward order. */
void dovi_AssignV3I2I(const void *source, void *destination);
void dovi_AssignM33I322I(const void *source, void *destination);
void dovi_AssignM33I2I(const void *source, void *destination);
void dovi_GetRgb2LmsWpV3(const void *matrix, void *destination);
unsigned int dovi_GetIpt2LmsM33Fxp(void *output);
unsigned int dovi_GetLms2IptM33Fxp(void *output);

#endif
