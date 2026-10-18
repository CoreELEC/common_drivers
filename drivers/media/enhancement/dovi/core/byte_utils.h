#ifndef DOVI_BYTE_UTILS_H
#define DOVI_BYTE_UTILS_H

/* Forward byte copy retains overlap propagation.
 * Negative signed counts become very large 64-bit counts and are not hardened. */
void dovi_MemCpyByte(void *destination, const void *source, int count);
int dovi_MemEqByte(const void *left, const void *right, int count);

#endif
