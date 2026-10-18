/* Expose metadata host callbacks and retain native parser state. */
#include "module_parser.h"
#include "module_parser_context.h"
#include "parser_process.h"
#include "parser_state.h"
#include "parser_access.h"
#include "fixed_access.h"
#include "error_strings.h"
#include "parser_lifecycle.h"
#include "supplemental_dispatch.h"

#define PREFIX "\001" "6[dovi_gen_5_15_2026]: "

static void process_error(int result)
{
    dovi_module_os.printk(PREFIX "dv_md_parser_process failed %s\n",
                          dovi_rpu_error_code_2_str(parser_clamp_error(result)));
}

void dovi_metadata_parser_release(void **context)
{
    struct dovi_module_parser_context *ctx = *context;
    if (!ctx)
        return;
    if (ctx->metadata_sequence) {
        dovi_module_os.vfree(ctx->metadata_sequence);
        ctx->metadata_sequence = 0;
    }
    if (ctx->parser) {
        dovi_md_parser_destroy(ctx->parser);
        dovi_module_os.vfree(ctx->parser);
        ctx->parser = 0;
    }
    if (*context) {
        dovi_module_os.vfree(*context);
        *context = 0;
    }
}

int dovi_metadata_parser_reset(void *context)
{
    struct dovi_module_parser_context *ctx = context;
    int result;
    if (!ctx) {
        dovi_module_os.printk(PREFIX "metadata_parser_reset null param\n");
        return -1;
    }
    result = dovi_md_parser_reset(ctx->parser, 1);
    if (result == 0) {
        dovi_module_frame_nr = 0;
        return 0;
    }
    dovi_module_os.printk(PREFIX "dv_md_parser_reset(rc='%s') failed\n",
                          dovi_rpu_error_code_2_str(result));
    return -1;
}

int dovi_metadata_parser_process(void *context, const unsigned char *input, unsigned int size,
                                 void *composer_output, unsigned int *composer_size,
                                 void *metadata_output, unsigned int *metadata_size,
                                 unsigned int flags, int input_kind)
{
    struct dovi_module_parser_context *ctx = context;
    int output_flags = 0;
    unsigned int length = 0u - 5u;
    int result;
    if (!ctx) {
        dovi_module_os.printk(PREFIX "metadata_parser_process null param\n");
        return -1;
    }
    parser_zero_bytes(&ctx->composer_size, 8);
    ctx->input = input;
    ctx->composer_output = composer_output;
    ctx->metadata_output = metadata_output;
    if ((unsigned int)input_kind - 1u < 2u) {
        result = dovi_md_parser_process_vui(ctx->parser);
        if (result != 0)
            process_error(result);
        result = dovi_md_parser_process_mastering_display_colour_volume(
            ctx->parser, input + 0x1c, fixed_load_le32(input + 0x18));
        if (result != 0)
            process_error(result);
        result = dovi_md_parser_process_user_data_registered_itu_t_t35(
            ctx->parser, 0, 0, input + 0x120, fixed_load_le32(input + 0x11c));
        if (result == 0)
            goto success;
        result = parser_clamp_error(result);
        process_error(result);
    } else {
        length = parser_trimmed_payload_size(input, size);
        result = dovi_md_parser_process(ctx->parser, 0, 0, input + 5, length, (int)(flags & 1u),
                                        &output_flags);
        if (result == 0)
            goto success;
        result = parser_clamp_error(result);
    }
    dovi_module_os.printk(PREFIX "dv_md_parser_process %d:%02x,%02x (rc='%s') failed\n",
                          fixed_signed32(length), (unsigned int)input[5], (unsigned int)input[6],
                          dovi_rpu_error_code_2_str(result));
    return -1;
success:
    *composer_size = ctx->composer_size;
    *metadata_size = ctx->metadata_size;
    return fixed_signed32(output_flags);
}
