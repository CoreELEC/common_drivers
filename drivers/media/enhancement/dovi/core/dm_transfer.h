/* Transfer-function selection and fixed-point de-gamma. */
#ifndef DOVI_DM_TRANSFER_H
#define DOVI_DM_TRANSFER_H

/* Directions are forward=1, backward=-1 and pass-through=0. The primary
 * getter requires a valid context; the graphics getter also returns 2 when
 * configuration or secondary input is unavailable. */
/* Gain, offset, exponent and sample use their low16 bits; overflow_scale uses32. */
unsigned int dovi_dm_DeGamma(unsigned int gain, unsigned int offset, unsigned int exponent,
                            unsigned int overflow_scale, unsigned int sample);
int dovi_dm_GetTcLutDir(const void *context);
int dovi_dm_GetTcLutGDir(const void *context);

#endif
