#ifndef DOVI_EXT_BLOCKS_H
#define DOVI_EXT_BLOCKS_H

#include "parser_contract.h"
#include "bitstream.h"

/*
 * Records have stride 0x108, a little-endian length at +0, level at +4 and
 * payload at +6. Decoder+8 points to mutable configuration and +0x40 embeds
 * the bitstream. Supply stable, disjoint objects and accessible padded input.
 * Unknown payload limits, alignment and callback interfaces follow the parser
 * contract. Fixed levels preserve their unchecked minimum-length behavior.
 * The list reader updates count only on success; the payload reader receives
 * the selected record index, rather than an output/removal flag.
 * The list reader returns 0 or -2; the payload reader returns 0 or -1.
 */
#if PARSER_DECODER_EXTENSION_API
int dovi_ext_blocks_read(void *decoder, void *blocks, unsigned char *count, unsigned int kind);
int dovi_ext_block_payload_read(void *decoder, void *block, void *bits,
                                const unsigned char *block_index);
#else
int dovi_ext_blocks_read(void *decoder, void *blocks, unsigned char *count, unsigned int kind);
int dovi_ext_block_payload_read(void *context, void *event_callback_slot, void *record,
                                struct dovi_bitstream *bits, const unsigned char *block_index);
#endif

#endif
