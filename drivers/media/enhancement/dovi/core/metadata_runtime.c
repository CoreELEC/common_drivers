/* Expose metadata host callbacks and retain native parser state. */
#include "module_parser.h"
#include "module_parser_context.h"
#include "parser_process.h"
#include "parser_state.h"
#include "parser_access.h"
#include "fixed_access.h"
#include "error_strings.h"
#include "parser_info.h"

struct dovi_module_parser_storage dovi_module_parser_state;

void dovi_metadata_parser_release(void)
{
}

int dovi_metadata_parser_process(const char *input, int size, char *composer_output,
                                 int *composer_size, char *metadata_output, int *metadata_size,
                                 unsigned char eos)
{
    msg_log_func_t logger = dovi_module_parser_state.logger;
    struct metadata_init_storage *storage = &dovi_module_parser_state.storage;
    struct metadata_counters *counters = &dovi_module_parser_state.counters;
    int flags = 0;
    unsigned int length;
    struct dovi_module_parser_context *ctx = &storage->ctx;
    counters->comp_size = 0;
    counters->md_size = 0;
    ctx->input = (const unsigned char *)input;
    ctx->composer_output = (unsigned char *)composer_output;
    ctx->metadata_output = (unsigned char *)metadata_output;
    length = parser_trimmed_payload_size((const unsigned char *)input, (unsigned int)size);
    storage->rc = (int)dovi_md_parser_process(ctx->parser, 0, 0, (const unsigned char *)input + 5,
                                              length, (int)eos, &flags);
    if (storage->rc == 0) {
        *composer_size = counters->comp_size;
        *metadata_size = counters->md_size;
        (void)dovi_md_parser_reset(ctx->parser);
        return flags;
    }
    logger("\001" "6dv_md_parser_process %d:%02x,%02x (rc='%s') failed\n", fixed_signed32(length),
           (unsigned char)input[5], (unsigned char)input[6],
           dovi_md_parser_get_errstr(storage->rc));
    return -1;
}

int dovi_metadata_parser_reset(int flag)
{
    msg_log_func_t logger = dovi_module_parser_state.logger;
    struct metadata_init_storage *storage = &dovi_module_parser_state.storage;
    (void)flag;
    storage->rc = (int)dovi_md_parser_reset(storage->ctx.parser);
    if (storage->rc == 0)
        return 0;
    logger("\001" "6dv_md_parser_reset(rc='%s') failed\n", dovi_rpu_error_code_2_str(storage->rc));
    return -1;
}
