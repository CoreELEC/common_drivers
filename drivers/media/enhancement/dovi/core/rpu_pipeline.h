/* RPU decoder orchestration. */
#ifndef DOVI_RPU_PIPELINE_H
#define DOVI_RPU_PIPELINE_H
#include "parser_contract.h"

/* Original 64-bit little-endian layout, disjoint stable ordinary-memory
 * objects, padded input, and nonmutating compatible callbacks are required.
 * These functions neither allocate storage nor validate pointer lifetimes.
 * Buffer ownership rotates among decoder+0x31658..+0x316d0 and root+8.
 * Queued slots are consumed on processing errors; indices advance by a mask.
 * Five distinct output slots are required, retaining the unchecked API.
 */
#if PARSER_FATAL_ARRAY_LIMITS
void dovi_set_default_payload(void *decoder);
#endif
int dovi_rpu_decoder_process_buffer(void *decoder, const unsigned char *input, unsigned int size,
                                    void **header, void **payload, int *payload_size, void **dm,
                                    int *dm_size);
int dovi_rpu_decoder_get_output(void *decoder, void **header, void **payload, int *payload_size,
                                void **dm, int *dm_size);

#endif
