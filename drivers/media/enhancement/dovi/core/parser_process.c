/* Dispatch metadata input to the decoder and deliver parser events. */
#include "parser_process.h"
#include "parser_layout.h"
#include "rpu_buffering.h"
#include "fixed_access.h"

static int equal_timestamp(const void *left, const void *right)
{
    const unsigned char *a = left, *b = right;
    unsigned int index;
    for (index = 0; index < 8; ++index)
        if (a[index] != b[index])
            return 0;
    return 1;
}

static void sample(unsigned char *parser, void *header, void *payload, int payload_size,
                   void *dm, int dm_size, int result)
{
    parser_load_output_callback(parser + PARSER_EVENT_CALLBACK)(
        parser_load_pointer(parser + PARSER_CALLBACK_CONTEXT), MD_PARSER_SAMPLE_AVAIL_EVT,
        parser + PARSER_TIMESTAMP, header, payload, payload_size, dm, dm_size, result);
}

static void error(unsigned char *parser, int result)
{
    fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
    parser_load_output_callback(parser + PARSER_EVENT_CALLBACK)(
        parser_load_pointer(parser + PARSER_CALLBACK_CONTEXT), MD_PARSER_ERROR_EVT,
        0, 0, 0, 0, 0, 0, 0);
}

int dovi_md_parser_process(dv_md_parser_ctx_t *context, unsigned int flags,
                           const void *timestamp, const unsigned char *input,
                           unsigned int size, int flush, int *flags_out)
{
    unsigned char *parser = (unsigned char *)context;
    void *header = 0, *payload = 0, *dm = 0;
    int payload_size = 0, dm_size = 0, result, output;
    unsigned int output_flags;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    if (parser[PARSER_INITIALIZED] == 0)
        return DV_MD_PARSER_UNINITIALIZE;
    if ((flags & 8u) != 0 && timestamp != 0 &&
        !equal_timestamp(timestamp, parser + PARSER_CURRENT_TIMESTAMP)) {
        parser_copy8(parser + PARSER_TIMESTAMP, parser + PARSER_CURRENT_TIMESTAMP);
        parser_copy8(parser + PARSER_CURRENT_TIMESTAMP, timestamp);
    }
    if (fixed_load_le32(parser + PARSER_DIRECT_MODE) != 0) {
        result = dovi_rpu_decoder_process_buffer(parser_load_pointer(parser + PARSER_DECODER),
                                                 input, size, &header, &payload, &payload_size,
                                                 &dm, &dm_size);
        if (result > 0) {
            sample(parser, header, payload, payload_size, dm, dm_size, result);
            result = 0;
        } else {
            error(parser, result);
        }
    } else {
        result = dovi_rpu_decoder_push_data(parser_load_pointer(parser + PARSER_DECODER),
                                            input, fixed_signed32(size), flush);
        if (result != 0) {
            fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
        } else {
            do {
                header = 0;
                payload = 0;
                dm = 0;
                output = dovi_rpu_decoder_get_output(parser_load_pointer(parser + PARSER_DECODER),
                                                     &header, &payload, &payload_size, &dm, &dm_size);
                if (output > 0) {
                    if (dovi_rpu_decoder_last_rpu_output(parser_load_pointer(parser + PARSER_DECODER)))
                        parser_copy8(parser + PARSER_TIMESTAMP, parser + PARSER_CURRENT_TIMESTAMP);
                    sample(parser, header, payload, payload_size, dm, dm_size, output);
                } else {
                    if ((unsigned int)output + 21u <= 1u)
                        break;
                    error(parser, output);
                    result = output;
                }
#if !PARSER_DRAIN_AFTER_ERROR
                if (output <= 0)
                    break;
#endif
            } while ((unsigned int)output + 19u < 0xfffffffeu);
        }
    }
    output_flags = fixed_load_le32(flags_out);
    #if PARSER_DRAIN_AFTER_ERROR
    output_flags = (output_flags & ~1u) |
                   parser_load_pointer(parser + PARSER_DECODER)[DECODER_MEL];
#else
    output_flags = (output_flags & ~1u) |
                   (parser_load_pointer(parser + PARSER_DECODER)[DECODER_MEL] != 0);
#endif
    fixed_store_le32(flags_out, output_flags);
    return result;
}
