/* Bind and initialize metadata callback storage and parser configuration. */
#include "module_parser.h"
#include "module_parser_context.h"
#include "metadata_callbacks.h"
#include "memory_query.h"
#include "parser_state.h"
#include "parser_info.h"
#include "parser_access.h"
#include "parser_lifecycle.h"

struct dovi_module_parser_context *dovi_metadata_parser_init(dovi_module_log_fn logger, int flag)
{
    struct metadata_init_storage *storage = &dovi_module_parser_state.storage;
    struct dovi_module_parser_context *ctx = &storage->ctx;
    dovi_module_parser_state.logger = logger;
    (void)flag;
    parser_zero_bytes(ctx, sizeof(*ctx));
    ctx->rpu_data_header = (struct rpu_data_header_s *)storage->rpu_data_header;
    parser_zero_bytes(storage->rpu_data_header, 0x136u);
    ctx->vdr_rpu_data_payload = (struct vdr_rpu_data_payload_s *)storage->vdr_rpu_data_payload;
    parser_zero_bytes(storage->vdr_rpu_data_payload, 0x7038u);
    ctx->vdr_dm_data_payload = (struct vdr_dm_data_payload_s *)storage->vdr_dm_data_payload;
    parser_zero_bytes(storage->vdr_dm_data_payload, 0x10744u);
    ctx->metadata_sequence = storage->metadata_sequence;
    parser_zero_bytes(storage->metadata_sequence, 0x10442u);
    ctx->config.flags.profile = RPU_MAIN_PROFILE;
    ctx->config.flags.payload_only = 1;
    (void)dovi_md_parser_mem_query((const unsigned char *)&ctx->config);
    ctx->parser = dv_md_parser_create();
    if (ctx->parser == 0) {
        logger("\001" "6Error creating Dolby Vision metadata parser instance\n");
        return 0;
    }
    ctx->config.pf_md_parser_cb = dovi_md_parser_evt_handler_cb;
    ctx->config.pf_msg_log = logger;
    ctx->config.p_md_parser_cb_ctx = ctx;
    storage->rc = (int)dovi_md_parser_init(ctx->parser, &ctx->config);
    if (storage->rc != DV_MD_PARSER_OK) {
        logger("\001" "6dv_md_parser_init(rc='%s') failed)\n",
               dovi_md_parser_get_errstr(storage->rc));
        return 0;
    }
    logger("\001" "6Creating Dolby Vision metadata parser instance success\n");
    return ctx;
}
