#ifndef DOVI_CONTROL_REGISTERS_H
#define DOVI_CONTROL_REGISTERS_H
#include "composer_registers_abi.h"

/* Width occupies the low bits; height is shifted by 13 without range masking.
 * The constant-zero result retains the audited STB 2.4 helper contract.
 * Loads and stores stay interleaved for STB 2.6's overlapping source/output
 * views. The STB 2.4 callers provide disjoint composer and register buffers.
 * Buffers may be unaligned and must cover their native ABI sizes.
 * DM registers, reserved words and unused composer fields remain untouched. */
unsigned short dovi_encode_composer_registers(const void *composer, void *registers,
                                              unsigned int width, unsigned int height);

void dovi_encode_display_luts(void *output, const void *tone_map, const void *input_map);
#endif
