/* Decoder prefix used by the MEL predicate. */
#ifndef DOVI_DETECT_MEL_H
#define DOVI_DETECT_MEL_H

#include "parser_contract.h"

#define DOVI_MEL_PAYLOAD_MIN_SIZE 0x68c8u

struct dovi_rpu_root_prefix {
    const void *header;
    const unsigned char *payload;
};

struct dovi_decoder_prefix {
    const struct dovi_rpu_root_prefix *root;
};

/*
 * Returns exactly 0 or 1. Payload must have at least
 * DOVI_MEL_PAYLOAD_MIN_SIZE readable bytes and must remain unchanged during
 * the call. No null/length checks: those would change the original contract.
 * MEL requires offset 0, VDR input maximum 1 + 0 fractional bits, and
 * zero linear-deadzone parameters in all three components of pivot zero.
 */
#if PARSER_DECODER_EXTENSION_API
int dovi_detect_mel_payload(const unsigned char *payload);
#endif
int dovi_detect_mel(const struct dovi_decoder_prefix *decoder);

#endif
