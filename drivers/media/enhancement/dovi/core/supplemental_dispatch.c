/* Supplemental metadata dispatch. */
#include "fixed_access.h"
#include "parser_access.h"
#include "supplemental_dispatch.h"
#include "supplemental_metadata.h"
#include "parser_process.h"
#include "parser_lifecycle.h"
#include "parser_layout.h"
#include "bitstream.h"

typedef char require_abi[(sizeof(void *) == 8 && sizeof(unsigned int) == 4 &&
                          sizeof(unsigned long long) == 8 && sizeof(int) == 4 &&
                          sizeof(dovi_parser_output_fn) == 8 && sizeof(dovi_parser_log_fn) == 8)
                             ? 1
                             : -1];

int dovi_rpu_decoder_push_VUI(void *opaque
#if PARSER_VUI_ARGUMENT
                              , const void *vui
#endif
)
{
    unsigned char *decoder = opaque;
    unsigned char *metadata;
    static const unsigned int ycc[9] = {
        0x2566u, 0u, 0x35eau, 0x2566u, 0xf9fcu, 0xeb1cu,
        0x2566u, 0x44cau, 0u
    };
    static const unsigned int lms[9] = {
        0x1c36u, 0x2243u, 0x186u, 0xa5eu, 0x308eu, 0x514u,
        0u, 0x1a6u, 0x3e5au
    };
    unsigned int i;
#if PARSER_VUI_ARGUMENT
    (void)vui;
#endif
    if (decoder == 0)
        return -7;
    metadata = parser_load_pointer(parser_load_pointer(decoder) + 0x10);
    for (i = 0; i < 9; ++i) {
        fixed_store_le16(metadata + 4 + i * 2, ycc[i]);
        fixed_store_le16(metadata + 0x24 + i * 2, lms[i]);
    }
    fixed_store_le32(metadata + 0x18, 0x1000000u);
    fixed_store_le32(metadata + 0x1c, 0x8000000u);
    fixed_store_le32(metadata + 0x20, 0x8000000u);
    fixed_store_le16(metadata + 0x36, 0xffffu);
    fixed_store_le16(metadata + 0x38, 0);
    fixed_store_le16(metadata + 0x3a, 0);
    fixed_store_le32(metadata + 0x3c, 0);
    metadata[0x40] = 12;
    metadata[0x41] = 0;
    metadata[0x42] = 0;
    metadata[0x43] = 1;
    fixed_store_le16(metadata + 0x48, 42);
    fixed_store_le32(decoder + 0x315c0, 1);
    return 0;
}

int dovi_rpu_decoder_push_2086_payload(void *opaque, const unsigned char *input, unsigned int size)
{
    unsigned char *decoder = opaque;
    int result;
    if (decoder == 0)
        return -7;
    if (size == 0)
        return 0;
    if (input == 0)
        return -7;
    dovi_bitstream_init((struct dovi_bitstream *)(decoder + 0x40), input, size);
    result = dovi_mastering_display_colour_volume_read(decoder);
    if (result < 0)
        return result;
    fixed_store_le32(decoder + 0x315bc, 1);
    return 0;
}

int dovi_md_parser_process_vui(dv_md_parser_ctx_t *context
#if PARSER_VUI_ARGUMENT
                              , const void *vui
#endif
)
{
    unsigned char *parser = (unsigned char *)context;
    int result;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    result = dovi_rpu_decoder_push_VUI(parser_load_pointer(parser + PARSER_DECODER)
#if PARSER_VUI_ARGUMENT
                                       , vui
#endif
    );
    if (result != 0)
        fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
    return result;
}

int dovi_md_parser_process_mastering_display_colour_volume(dv_md_parser_ctx_t *context,
                                                           const unsigned char *input,
                                                           unsigned int size)
{
    unsigned char *parser = (unsigned char *)context;
    int result;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    result = dovi_rpu_decoder_push_2086_payload(parser_load_pointer(parser + PARSER_DECODER), input, size);
    if (result != 0)
        fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
    return result;
}

int dovi_rpu_decoder_push_2094_payload(void *opaque, const unsigned char *input, unsigned int size)
{
    unsigned char *decoder = opaque;
    int result;
    if (decoder == 0)
        return -7;
    if (size == 0)
        return 0;
    if (input == 0)
        return -7;
    parser_store_pointer(parser_load_pointer(decoder) + 8, parser_load_pointer(decoder + 0x316d0));
    dovi_bitstream_init((struct dovi_bitstream *)(decoder + 0x40), input, size);
    result = dovi_st2094_10_data_read(decoder);
    return result < 0 ? result : 0;
}

int dovi_md_parser_process_user_data_registered_itu_t_t35(
    dv_md_parser_ctx_t *context, unsigned int flags, const void *timestamp,
    const unsigned char *input, unsigned int size)
{
    unsigned char *parser = (unsigned char *)context;
    void *header = 0, *payload = 0, *dm = 0;
    int payload_size = 0, dm_size = 0, result;
    if (parser == 0)
        return DV_MD_PARSER_INVALID_CONTEXT_HANDLE;
    if (parser[PARSER_INITIALIZED] == 0)
        return DV_MD_PARSER_UNINITIALIZE;
    if ((flags & 8u) != 0 && timestamp != 0) {
        unsigned long long value = fixed_load_le64(timestamp);
        if (fixed_load_le64(parser + PARSER_SAMPLE_COUNT) == 0 ||
            value != fixed_load_le64(parser + PARSER_TIMESTAMP)) {
            fixed_store_le64(parser + PARSER_TIMESTAMP, value);
        } else {
            dovi_parser_log_fn log = parser_load_logger(parser + PARSER_LOGGER);
            if (log != 0)
                log("Fatal Error: consecutive SEI user data registered ITU.T t35 message with same PTS\n");
        }
    }
    fixed_store_le64(parser + PARSER_SAMPLE_COUNT,
                     fixed_load_le64(parser + PARSER_SAMPLE_COUNT) + 1u);
    result = dovi_rpu_decoder_push_2094_payload(parser_load_pointer(parser + PARSER_DECODER), input, size);
    if (result != 0) {
        fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
        return result;
    }
    for (;;) {
        header = 0;
        payload = 0;
        dm = 0;
        result = dovi_rpu_decoder_get_output(parser_load_pointer(parser + PARSER_DECODER),
                                             &header, &payload, &payload_size, &dm, &dm_size);
        if (result <= 0)
            break;
        parser_load_output_callback(parser + PARSER_EVENT_CALLBACK)(
            parser_load_pointer(parser + PARSER_CALLBACK_CONTEXT), MD_PARSER_SAMPLE_AVAIL_EVT,
            parser + PARSER_TIMESTAMP, header, payload, payload_size, dm, dm_size, result);
    }
    if ((unsigned int)result + 21u <= 1u)
        return DV_MD_PARSER_OK;
    fixed_store_le32(parser + PARSER_LAST_ERROR, (unsigned int)result);
    parser_load_output_callback(parser + PARSER_EVENT_CALLBACK)(
        parser_load_pointer(parser + PARSER_CALLBACK_CONTEXT), MD_PARSER_ERROR_EVT,
        0, 0, 0, 0, 0, 0, 0);
    return result;
}
