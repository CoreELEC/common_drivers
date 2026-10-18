#ifndef DOVI_PQ_CONVERSION_H
#define DOVI_PQ_CONVERSION_H

/* Fixed-point PQ transfer functions and table interpolators. */
/* LToPQ12 remains separate from control-local dovi_luminance_to_pq12. */

unsigned long long dovi_L2PqX(unsigned long long raw_linear);
unsigned int dovi_PQ16ToL(unsigned int pq);
unsigned int dovi_PQ12ToL(unsigned int pq);
/* Control luminance uses Q18 and saturates the rounded endpoint to 4095. */
unsigned int dovi_LToPQ12(unsigned int luminance);

unsigned int dovi_FindNodeIdxL2PQS(unsigned int value);
unsigned int dovi_LToPQ16(unsigned int value);
unsigned int dovi_LToPQ18(unsigned int value);
unsigned long long dovi_Pq2LX(unsigned int value);
unsigned long long dovi_L2Pq(unsigned long long value);
unsigned long long dovi_Pq2L(unsigned int value);
/* PQ2LXLut retains the original fatal BRK for signed INT32_MIN. */
unsigned long long dovi_PQ2LXLut(unsigned int value);
unsigned int dovi_L2PQXLut(unsigned long long value);

#endif
