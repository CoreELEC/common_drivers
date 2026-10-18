/* Reset decoder and parser state and expose status. */
#include "parser_context.h"
#include "parser_state.h"
#include "fixed_access.h"
#include "parser_access.h"
#include "parser_layout.h"
#include "parser_api.h"

typedef char require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char require_u64_pointer[(sizeof(void *) == 8 && sizeof(unsigned long long) == 8) ? 1 : -1];


static void clear_status_words(unsigned char *decoder)
{
    unsigned int i;
    for (i = 0; i < fixed_load_le32(decoder + DECODER_SLOT_COUNT); ++i) {
        unsigned char *words = parser_load_pointer(decoder + DECODER_SLOT_LENGTHS);
        fixed_store_le32(words + (unsigned long long)i * 4u, 0);
    }
}

int dovi_rpu_decoder_reset(void *ctx
#if PARSER_RESET_FLAGS
                            , unsigned int flags
#endif
)
{
    unsigned char *decoder = ctx;
    if (fixed_load_le32(decoder + DECODER_DIRECT_MODE) != 0) {
#if PARSER_RESET_FLAGS
        unsigned char *root;
#endif
        parser_zero_bytes(decoder + DECODER_INPUT_SLOT, 8);
        fixed_store_le32(decoder + DECODER_FRAMING_STATE, 4);
        fixed_store_le32(decoder + DECODER_SLOT_COUNT, 1);
        fixed_store_le32(parser_load_pointer(decoder + DECODER_SLOT_LENGTHS), 0);
        decoder[DECODER_MEL] = 0;
#if PARSER_RESET_FLAGS
        if (flags == 1) {
            root = parser_load_pointer(decoder + DECODER_ROOT);
            parser_zero_bytes(parser_load_pointer(root), 0x136u);
            parser_zero_bytes(parser_load_pointer(root + 0x10u), 0x10744u);
            parser_zero_bytes(parser_load_pointer(decoder + DECODER_PAYLOAD_STORAGE), 0xe070u);
        }
#endif
    } else {
        unsigned char *config;
        unsigned int status_count, slot_count, i;
        fixed_store_le32(decoder + DECODER_FRAMING_STATE, 0);
        parser_zero_bytes(decoder + DECODER_INPUT_SLOT, 8);
        clear_status_words(decoder);
        status_count = fixed_load_le32(decoder + DECODER_SLOT_COUNT);
        config = parser_load_pointer(decoder + DECODER_CONFIG);
        fixed_store_le32(decoder + DECODER_SEQUENCE_AVAILABLE, 0);

        /* -1 -> zero slots; 0..14 -> one through fifteen slots. */
        slot_count = fixed_load_le32(config + 0x678u) + 1u;
        for (i = 0; i < slot_count; ++i) {
#if PARSER_FATAL_ARRAY_LIMITS
            if (i == 15u)
                parser_limit_trap();
#endif
            parser_copy8(decoder + DECODER_PAYLOAD_SLOTS + (unsigned long long)i * 8u,
                         decoder + DECODER_PAYLOAD_STORAGE + (unsigned long long)i * 8u);
        }
        fixed_store_le32(decoder + DECODER_FRAMING_STATE, 0);
        parser_zero_bytes(decoder + DECODER_OUTPUT_SLOT, 8);
        parser_copy8(decoder + DECODER_CURRENT_PAYLOAD,
                     decoder + DECODER_PAYLOAD_STORAGE + (unsigned long long)slot_count * 8u);
        fixed_store_le32(decoder + DECODER_INPUT_SLOT, status_count - 1u);
        if (status_count != 0)
            clear_status_words(decoder);
        decoder[DECODER_MEL] = 0;
    }
    return 0;
}

int dovi_md_parser_reset(dv_md_parser_ctx_t *ctx
#if PARSER_RESET_FLAGS
                         , unsigned int flags
#endif
)
{
    unsigned char *parser = (unsigned char *)ctx;
    void *decoder;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    if (parser[PARSER_INITIALIZED] == 0)
        return DV_MD_PARSER_UNINITIALIZE;
    decoder = parser_load_pointer(parser + PARSER_DECODER);
    if (decoder != 0)
        dovi_rpu_decoder_reset(decoder
#if PARSER_RESET_FLAGS
                                , flags
#endif
        );
    parser_zero_bytes(parser + PARSER_SAMPLE_COUNT, 8);
    return 0;
}

int dovi_rpu_decoder_flush(void *ctx)
{
    fixed_store_le32((unsigned char *)ctx + DECODER_FLUSH, 1);
    return 0;
}

int dovi_md_parser_get_last_err(const void *context)
{
    return context == 0 ? DV_MD_PARSER_INTERNAL_ERR
                        : fixed_signed32(fixed_load_le32((const unsigned char *)context + PARSER_LAST_ERROR));
}

void *dovi_md_parser_get_rpu_dcd_ctx(const void *context)
{
    const unsigned char *parser = context;
    if (parser == 0 || parser[PARSER_INITIALIZED] == 0)
        return 0;
    return parser_load_pointer(parser + PARSER_DECODER);
}
