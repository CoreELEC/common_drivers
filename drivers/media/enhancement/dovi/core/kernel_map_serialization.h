/* Native kernel-state streams. */
#ifndef DOVI_KERNEL_MAP_SERIALIZATION_H
#define DOVI_KERNEL_MAP_SERIALIZATION_H

/* Packed little-endian kernel-map streams accept unaligned byte storage. */
/* Kernel objects contain 10928 bytes. The stream size depends on secondary
 * input and graphics color space and can reach 10872 bytes. Hardware output
 * can reach 10850 bytes. Source and destination require full, distinct backing;
 * no capacity is read.
 * Loading preserves frame dimensions and pitches, and uses the existing
 * graphics input-map color to select the optional alpha-frame record. */
unsigned int dovi_DumpKsBinBuf(const void *source, void *output);
unsigned int dovi_LoadKsBinBuf(void *output, const void *source);
void dovi_SetHardwareInterface(const void *kernel, void *registers);

#endif
