#ifndef DOVI_PIVOT_VALUES_H
#define DOVI_PIVOT_VALUES_H

/*
 * root+0 points to a writable 0x136-byte little-endian header. Objects are
 * accessible, disjoint and serialized. Delta accumulation wraps to 16 bits;
 * depth uses a masked shift and signed comparison. Returns 0 or -13, retaining
 * partial writes. Fatal array limits follow the native parser contract.
 * Counts 0 and 1 both copy and validate the first input value.
 */
int dovi_assign_pivot_values(void *root);
int dovi_assign_nlq_pivot_values(void *root);

#endif
