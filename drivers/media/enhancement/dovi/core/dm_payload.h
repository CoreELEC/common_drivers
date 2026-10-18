#ifndef DOVI_DM_PAYLOAD_H
#define DOVI_DM_PAYLOAD_H

#include "parser_contract.h"

#define DOVI_DM_PAYLOAD_SIZE 0x10744u
#define DOVI_DM_PAYLOAD_DECODER_MIN_SIZE 0x316e0u

/*
 * decoder+0 points to {header, payload, DM}; decoder+8 points to writable
 * configuration and +0x40 holds the bitstream. Supply accessible disjoint
 * decoder/root/header/configuration/DM arenas and padded input as in
 * bitstream.h. Extension scratch requires sentinel backing; writes can exceed
 * the nominal 255 records before the count saturates.
 *
 * Returns a wrapping signed bit count or an extension-reader error. Reuse-mode
 * accounting follows parser_contract.h. Partial writes, byte truncation and
 * unchecked extension storage are preserved.
 */
int dovi_dm_data_payload_read(void *decoder);

#if !PARSER_DECODER_EXTENSION_API
void store_md(void *decoder, const void *dm);
#endif

#endif
