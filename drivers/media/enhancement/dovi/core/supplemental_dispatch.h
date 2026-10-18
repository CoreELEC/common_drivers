/* Supplemental metadata dispatch and callback ownership. */
#ifndef DOVI_SUPPLEMENTAL_DISPATCH_H
#define DOVI_SUPPLEMENTAL_DISPATCH_H
#include "parser_process.h"
/* Nested pointers and refill padding must remain valid; calls are serialized.
 * The historical VUI argument is ignored. Timestamps read eight bytes when
 * flag bit 3 is set. Callback pointers and pointed-to payloads remain live.
 */
int dovi_rpu_decoder_push_VUI(void *decoder
#if PARSER_VUI_ARGUMENT
                              , const void *vui
#endif
);
int dovi_rpu_decoder_push_2086_payload(void *decoder, const unsigned char *input, unsigned int size);
int dovi_md_parser_process_vui(dv_md_parser_ctx_t *parser
#if PARSER_VUI_ARGUMENT
                              , const void *vui
#endif
);
int dovi_md_parser_process_mastering_display_colour_volume(dv_md_parser_ctx_t *parser,
                                                           const unsigned char *input,
                                                           unsigned int size);
int dovi_rpu_decoder_push_2094_payload(void *decoder, const unsigned char *input, unsigned int size);
int dovi_md_parser_process_user_data_registered_itu_t_t35(dv_md_parser_ctx_t *parser,
                                                          unsigned int flags, const void *timestamp,
                                                          const unsigned char *input, unsigned int size);
#endif
