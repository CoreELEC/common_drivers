/* Bind and initialize metadata callback storage and parser configuration. */
#include "module_parser.h"
#include "module_parser_context.h"
#include "memory_query.h"
#include "parser_access.h"
#include "parser_lifecycle.h"
#include "error_strings.h"

#define PREFIX "\001" "6[dovi_gen_5_15_2026]: "

void *dovi_metadata_parser_init(void)
{
    struct dovi_module_parser_context *ctx = dovi_module_os.vmalloc(sizeof(*ctx));
    void *allocation;
    unsigned long long size;
    int result;
    if (!ctx) {
        dovi_module_os.printk(PREFIX "Error malloc app_ctx_t\n");
        return 0;
    }
    parser_zero_bytes(ctx, sizeof(*ctx));
    allocation = dovi_module_os.vmalloc(0x10442);
    ctx->metadata_sequence = allocation;
    if (!allocation) {
        dovi_module_os.printk(PREFIX "Error malloc dm_byte_sequence\n");
        goto fail;
    }
    parser_zero_bytes(allocation, 0x10442);
    parser_zero_bytes(&ctx->config.flags, sizeof(ctx->config.flags));
    ctx->config.flags.profile = RPU_MAIN_PROFILE;
    ctx->config.flags.payload_only = 1;
    size = dovi_md_parser_mem_query((const unsigned char *)&ctx->config);
    allocation = dovi_module_os.vmalloc(size);
    ctx->parser = allocation;
    if (!allocation) {
        dovi_module_os.printk(PREFIX "Error malloc h_md_parser\n");
        goto fail;
    }
    parser_zero_bytes(allocation, size);
    ctx->config.p_md_parser_cb_ctx = ctx;
    ctx->config.pf_md_parser_cb = dovi_md_parser_evt_handler_cb;
    parser_zero_bytes(&ctx->config.pf_msg_log, sizeof(ctx->config.pf_msg_log));
    result = dovi_md_parser_init(ctx->parser, &ctx->config);
    if (result == 0) {
        dovi_module_os.printk(PREFIX "Creating DV mp success\n");
        return ctx;
    }
    dovi_module_os.printk(PREFIX "dv_md_parser_init(rc='%s') failed)\n",
                          dovi_rpu_error_code_2_str(parser_clamp_error(result)));
fail:
    dovi_module_os.printk(PREFIX "Error creating DV metadata parser\n");
    if (ctx->metadata_sequence) {
        dovi_module_os.vfree(ctx->metadata_sequence);
        ctx->metadata_sequence = 0;
    }
    if (ctx->parser) {
        dovi_md_parser_destroy(ctx->parser);
        dovi_module_os.vfree(ctx->parser);
        ctx->parser = 0;
    }
    dovi_module_os.vfree(ctx);
    return 0;
}
