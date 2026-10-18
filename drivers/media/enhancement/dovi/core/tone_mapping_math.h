#ifndef DOVI_TONE_MAPPING_MATH_H
#define DOVI_TONE_MAPPING_MATH_H

/* Raw register bit patterns and original little-endian layouts. Contexts are
 * caller-owned and externally serialized. No added range corrections.
 * ApplyLocalMs reads context+0x74c/0x784 or +0x7a0/0x7f4, then conditionally
 * writes a halfword at records+0x18+signed(records[2])*stride (14 or 42).
 */
void dovi_ApplyLocalMs(const void *context, void *records);
/* Writes exactly 256 words at output+0x30 on ordinary completion. Original
 * interpolation bounds retain BRK #1 after any earlier partial writes. */
void dovi_DeriveHlgEotfParam(void *output, unsigned int minimum, unsigned int maximum);
unsigned int dovi_TMIcore(const void *config, unsigned int value);
/* sqrt_output is optional; when present, its four-byte write precedes later
 * config reads and may overlap config storage as in the original. */
unsigned int dovi_TMScore(const void *config, unsigned int value, void *sqrt_output,
                          unsigned int mode);
unsigned int dovi_SMIcore(const void *config, unsigned int value, unsigned int reference,
                          unsigned int mode);
unsigned int dovi_SMScore(const void *config, const void *sqrt_input, unsigned int value,
                          unsigned int clamp_mode, unsigned int mode);
unsigned int dovi_CalcCuttoff(unsigned int low, unsigned int value, unsigned int high,
                              unsigned int target_low, unsigned int target_high);
#endif
