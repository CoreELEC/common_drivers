#ifndef DOVI_RPU_PAYLOAD_H
#define DOVI_RPU_PAYLOAD_H

/*
 * decoder+0 points to {header, payload}; decoder+8 points to writable
 * configuration and +0x40 holds struct dovi_bitstream. Payload partitions have
 * stride 0x7038. Logging callback/level are at +0x315c8/+0x315d0 and the MEL flag
 * is at +0x316e0. Objects must be accessible, disjoint and stable; input is
 * padded as in bitstream.h. Coefficient counts and indices are unchecked.
 *
 * Returns a wrapping signed bit count or a negative parser error. Partial
 * writes, byte truncation, stale skipped fields and prediction reads before
 * validation are preserved. Fatal limits follow parser_contract.h.
 */
int dovi_rpu_payload_read(void *decoder);

#endif
