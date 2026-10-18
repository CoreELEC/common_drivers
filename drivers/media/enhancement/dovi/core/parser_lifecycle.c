/* Initialize and destroy metadata parser state. */
#include "parser_lifecycle.h"
#include "parser_layout.h"
#include "parser_process.h"
#include "decoder_lifecycle.h"
#include "error_strings.h"
#include "fixed_access.h"

#if !PARSER_RESET_FLAGS
/* Context and decoder arena remain one static object in STB 2.4. */
static union {
    dv_md_parser_ctx_t context;
    unsigned char bytes[MD_PARSER_SINGLETON_BYTES];
} md_parser;

dv_md_parser_ctx_t *dv_md_parser_create(void)
{
    parser_zero_bytes(md_parser.bytes, sizeof(dv_md_parser_ctx_t));
    return &md_parser.context;
}
#endif

int dovi_md_parser_init(dv_md_parser_ctx_t *parser, const dv_md_parser_conf_t *config)
{
    unsigned char *bytes = (unsigned char *)parser;
    const unsigned char *configuration = (const unsigned char *)config;
    void *decoder;
    dovi_parser_log_fn log;
    int result;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    fixed_store_le32(bytes + PARSER_LAST_ERROR, DV_MD_PARSER_OK);
    bytes[PARSER_INITIALIZED] = 0;
    decoder = bytes + PARSER_DECODER_STORAGE;
    parser_store_pointer(bytes + PARSER_DECODER, decoder);
    parser_zero_bytes(bytes + PARSER_SAMPLE_COUNT, 8);
    parser_copy8(bytes + PARSER_EVENT_CALLBACK, configuration);
    parser_copy8(bytes + PARSER_CALLBACK_CONTEXT, configuration + 8);
    parser_copy_bytes(bytes + PARSER_FLAGS, configuration + 0x10, sizeof(rpu_init_flags_t));
    parser_copy8(bytes + PARSER_LOGGER, configuration + 0x30);
    log = parser_load_logger(configuration + 0x30);
    dovi_rpu_decoder_set_msg_log(decoder, log, S_RPU_NOTICE);
    if (parser_load_output_callback(configuration) == 0) {
        if (log != 0)
            log("Dolby Vision metadata parser configured event notification callack is NULL\n");
        fixed_store_le32(bytes + PARSER_LAST_ERROR, DV_MD_PARSER_INVALID_ARGUMENT);
        return DV_MD_PARSER_INVALID_ARGUMENT;
    }
    result = dovi_rpu_decoder_init(decoder, configuration + 0x10);
    if (result < 0) {
        fixed_store_le32(bytes + PARSER_LAST_ERROR, (unsigned int)result);
        log = parser_load_logger(bytes + PARSER_LOGGER);
        if (log != 0)
            log("Error: rpu_decoder_init failed: %s\n", dovi_rpu_error_code_2_str(result));
        return result;
    }
    bytes[PARSER_INITIALIZED] = 1;
    return DV_MD_PARSER_OK;
}

int dovi_md_parser_destroy(dv_md_parser_ctx_t *parser)
{
    unsigned char *bytes = (unsigned char *)parser;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    if (parser_load_pointer(bytes + PARSER_DECODER) != 0)
        parser_zero_bytes(bytes + PARSER_DECODER, 8);
    bytes[PARSER_INITIALIZED] = 0;
    return DV_MD_PARSER_OK;
}
