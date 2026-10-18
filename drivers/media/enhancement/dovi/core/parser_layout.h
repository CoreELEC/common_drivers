/* Byte offsets used by the native metadata-parser interface. */
#ifndef DOVI_PARSER_LAYOUT_H
#define DOVI_PARSER_LAYOUT_H
#include "parser_context.h"

enum parser_offset {
    PARSER_INITIALIZED = 0x00, PARSER_EVENT_CALLBACK = 0x08,
    PARSER_CALLBACK_CONTEXT = 0x10, PARSER_LAST_ERROR = 0x18,
    PARSER_FLAGS = 0x1c, PARSER_DIRECT_MODE = 0x2c,
    PARSER_DECODER = 0x38, PARSER_TIMESTAMP = 0x40,
    PARSER_CURRENT_TIMESTAMP = 0x50, PARSER_LOGGER = 0x60,
    PARSER_SAMPLE_COUNT = 0x68, PARSER_DECODER_STORAGE = 0x70
};
enum decoder_offset {
    DECODER_ROOT = 0x00, DECODER_CONFIG = 0x08,
    DECODER_INPUT_STORAGE = 0x10, DECODER_SLOT_LENGTHS = 0x18,
    DECODER_INPUT_SLOT = 0x20, DECODER_OUTPUT_SLOT = 0x24,
    DECODER_FLUSH = 0x28, DECODER_DISABLE_CRC = 0x2c,
    DECODER_DIRECT_MODE = 0x30, DECODER_DISABLE_MEL = 0x34,
    DECODER_SLOT_COUNT = 0x38, DECODER_BITSTREAM = 0x40,
    DECODER_SEQUENCE_AVAILABLE = 0x78,
    DECODER_LOGGER = 0x315c8, DECODER_SEVERITY = 0x315d0,
    DECODER_FRAMING_STATE = 0x315d4, DECODER_PAYLOAD_STORAGE = 0x315d8,
    DECODER_PAYLOAD_SLOTS = 0x31658, DECODER_CURRENT_PAYLOAD = 0x316d0,
    DECODER_EVENT_CALLBACK = 0x316d8, DECODER_MEL = 0x316e0
};
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(offsetof(dv_md_parser_ctx_t, last_rc) == PARSER_LAST_ERROR, "parser error field");
_Static_assert(offsetof(dv_md_parser_ctx_t, p_rpu_dcd_ctx) == PARSER_DECODER, "parser decoder field");
_Static_assert(offsetof(dv_md_parser_ctx_t, ui64_sample) == PARSER_SAMPLE_COUNT, "parser sample field");
_Static_assert(sizeof(dv_md_parser_ctx_t) == PARSER_DECODER_STORAGE, "parser prefix extent");
#endif
#endif
