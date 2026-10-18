/* Accumulate composer and metadata output from parser sample events. */
#include "metadata_callbacks.h"
#include "module_parser.h"
#include "module_parser_context.h"
#include "parser_state.h"
#include "composer_conversion.h"
#include "composer_validation.h"
#include "dm_serialization.h"
#include "error_strings.h"
#include "parser_access.h"
#include "fixed_access.h"

#if PARSER_ALLOCATED_APPLICATION_STORAGE
#define PREFIX "\001" "6[dovi_gen_5_15_2026]: "
unsigned int dovi_module_frame_nr;
#else
#define PREFIX "\001" "6"
#endif

void dovi_md_parser_evt_handler_cb(void *context, dv_md_parser_evt_t event,
                                   const void *timestamp, const void *header,
                                   const void *payload, int payload_size,
                                   const void *dm, int dm_size, int output_result)
{
    struct dovi_module_parser_context *ctx;
    dovi_module_log_fn logger;
    unsigned char *frame_counter, *composer_size, *metadata_size;
    int result, length;
    unsigned int offset;
#if PARSER_ALLOCATED_APPLICATION_STORAGE
    ctx = context;
    logger = dovi_module_os.printk;
    frame_counter = (unsigned char *)&dovi_module_frame_nr;
    composer_size = (unsigned char *)&ctx->composer_size;
    metadata_size = (unsigned char *)&ctx->metadata_size;
#else
    struct dovi_module_parser_storage *state = context;
    ctx = &state->storage.ctx;
    logger = state->logger;
    frame_counter = (unsigned char *)&state->counters.frame_nr;
    composer_size = (unsigned char *)&state->counters.comp_size;
    metadata_size = (unsigned char *)&state->counters.md_size;
#endif
    (void)timestamp;
    (void)payload_size;
    (void)dm_size;
    (void)output_result;
    fixed_store_le32(frame_counter, fixed_load_le32(frame_counter) + 1u);
    if (event != MD_PARSER_SAMPLE_AVAIL_EVT) {
        if (event != MD_PARSER_ERROR_EVT)
            return;
        result = parser_clamp_error(dovi_md_parser_get_last_err(ctx->parser));
        goto error;
    }
    parser_zero_bytes(ctx->composer_config, sizeof(ctx->composer_config));
    dovi_comp_rpu_2_main_cfg(header, payload, ctx->composer_config);
    result = dovi_validate_composer_config(ctx->composer_config, fixed_load_le32(frame_counter),
                                           ctx->config.pf_msg_log, 7);
    if (result != 0)
        goto error;
    offset = fixed_load_le32(composer_size);
    if (offset > 0x18f8u) {
        logger(PREFIX "comp_size too big\n");
        return;
    }
    parser_copy_bytes(ctx->composer_output + fixed_signed32(offset), ctx->composer_config,
                      sizeof(ctx->composer_config));
    fixed_store_le32(composer_size, fixed_load_le32(composer_size) + 0x708u);
    result = dovi_validate_dm_payload(dm, fixed_load_le32(frame_counter), ctx->config.pf_msg_log, 7);
    if (result != 0)
        goto error;
    length = 0x10442;
    result = dovi_dm_rpu_payload_2_byte_sequence(dm, ctx->metadata_sequence, &length);
    if (result != 0)
        goto error;
    offset = fixed_load_le32(metadata_size);
    if (fixed_signed32(offset) > fixed_signed32(0x400u - (unsigned int)length)) {
        logger(PREFIX "md_size too big\n");
        return;
    }
    parser_copy_bytes(ctx->metadata_output + fixed_signed32(offset), ctx->metadata_sequence,
#if PARSER_SIGN_EXTEND_METADATA_COPY_LENGTH
                      (unsigned long long)(long long)length);
#else
                      (unsigned int)length);
#endif
    fixed_store_le32(metadata_size, fixed_load_le32(metadata_size) + (unsigned int)length);
    return;
error:
    logger(PREFIX "%s\n", dovi_rpu_error_code_2_str(result));
}
