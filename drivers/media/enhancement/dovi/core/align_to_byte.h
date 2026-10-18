#ifndef DOVI_ALIGN_TO_BYTE_H
#define DOVI_ALIGN_TO_BYTE_H

#include "bitstream.h"

/* Aligns the current cache to the next byte boundary, refilling if needed. */
void align_to_byte(struct dovi_bitstream *state);

#endif
