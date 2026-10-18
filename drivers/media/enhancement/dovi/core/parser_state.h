/* Reset decoder and parser state and expose status. */
#ifndef DOVI_PARSER_STATE_H
#define DOVI_PARSER_STATE_H
#include "parser_lifecycle.h"
#define DOVI_DECODER_STATE_MIN_SIZE 0x316e1u
#define DOVI_PARSER_STATE_MIN_SIZE 0x70u
/* Objects use the shared little-endian layout and bytewise access. Referenced
 * buffers must be valid and disjoint; calls are serialized. Buffered reset
 * copies max_prev_rpu_ids+1 payload pointers. Reset retains direct-mode
 * payload state; there is no reset-flags argument.
 */
int dovi_rpu_decoder_reset(void *decoder
#if PARSER_RESET_FLAGS
                            , unsigned int flags
#endif
);
int dovi_md_parser_reset(dv_md_parser_ctx_t *parser
#if PARSER_RESET_FLAGS
                         , unsigned int flags
#endif
);
int dovi_rpu_decoder_flush(void *decoder);
int dovi_md_parser_get_last_err(const void *context);
void *dovi_md_parser_get_rpu_dcd_ctx(const void *context);
#endif
