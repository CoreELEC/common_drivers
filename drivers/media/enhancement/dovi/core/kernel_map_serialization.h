/* Native kernel-state streams. */
#ifndef DOVI_KERNEL_MAP_SERIALIZATION_H
#define DOVI_KERNEL_MAP_SERIALIZATION_H

/* Packed little-endian kernel-map streams accept unaligned byte storage. */
/* The stream has 0x15d0 bytes and includes the indirectly owned output map.
 * Source and destination require full, distinct backing; no capacity is read.
 * The kernel allocation covers 0x1608 bytes. Its output-map pointer at 0x15f8
 * must already address 112 readable bytes for Dump or writable bytes for Load.
 * Load clears that existing map before restoring the stream map at kernel+0x1568
 * and retargeting the output-map pointer to this internal storage.
 * Loading preserves native dimensions, pitches and selected input type/depth
 * fields, and restores output dimensions from the primary-input dimensions. */
unsigned int dovi_DumpKsBinBuf(const void *source, void *output);
unsigned int dovi_LoadKsBinBuf(void *output, const void *source);
#endif
