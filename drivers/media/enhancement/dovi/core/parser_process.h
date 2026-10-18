/* Process RPU input and publish parser sample events. */
#ifndef DOVI_PARSER_PROCESS_H
#define DOVI_PARSER_PROCESS_H
#include "parser_context.h"
#include "parser_contract.h"
#include "parser_access.h"
#include "rpu_pipeline.h"
static inline dovi_parser_output_fn parser_load_output_callback(const void *source)
{
    dovi_parser_output_fn value;
    parser_copy8(&value, source);
    return value;
}
/* Initialized parser uses the shared 0x70-byte prefix in parser_context.h.
 * Flag bit 3 supplies eight timestamp bytes; the input needs bitstream padding
 * and flags_out is writable. Null/uninitialized return 2/1. Decoder errors,
 * callback ordering, buffered draining and MEL flag accounting remain native.
 */
int dovi_md_parser_process(dv_md_parser_ctx_t *parser, unsigned int flags,
                           const void *timestamp, const unsigned char *input,
                           unsigned int size, int flush, int *flags_out);
#endif
