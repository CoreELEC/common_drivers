#ifndef DOVI_PQ_CONVERSION_H
#define DOVI_PQ_CONVERSION_H

/* Fixed-point PQ transfer functions and table interpolators. */
/* LToPQ12 remains separate from control-local dovi_luminance_to_pq12. */

int dovi_L2PqX(unsigned long long raw_linear);
unsigned int dovi_PQ16ToL(unsigned int pq);
unsigned int dovi_PQ12ToL(unsigned int pq);
/* Control luminance uses Q18 and preserves the rounded 4096 endpoint. */
unsigned int dovi_LToPQ12(unsigned int luminance);

unsigned short dovi_LToPQ15(unsigned int luminance);

#endif
