/* Run decoder stages and produce queued RPU output. */
#include "rpu_pipeline.h"
#include "fixed_access.h"
#include "parser_access.h"
#include "bitstream.h"
#include "error_strings.h"
#include "rpu_header.h"
#include "rpu_validation.h"
#include "rpu_payload.h"
#include "detect_mel.h"
#include "dm_payload.h"
#include "rpu_buffering.h"
#include "parser_layout.h"
#include "parser_api.h"

typedef char require_int32[(sizeof(int) == 4 && sizeof(unsigned int) == 4) ? 1 : -1];
typedef char
    require_pointer64[(sizeof(void *) == 8 && sizeof(dovi_validation_log_fn) == 8) ? 1 : -1];

static dovi_validation_log_fn logger(const unsigned char *source)
{
    dovi_validation_log_fn value;
    parser_copy8(&value, source);
    return value;
}

static void outputs(unsigned char *decoder, void **header, void **payload, int *payload_size,
                    void **dm, int *dm_size)
{
    unsigned char *data = parser_load_pointer(decoder);
    *header = parser_load_pointer(data);
    *payload = parser_load_pointer(parser_load_pointer(decoder) + 8);
    data = parser_load_pointer(parser_load_pointer(decoder));
    *payload_size = fixed_signed32((unsigned int)data[0x133] * data[0x134] * 0x7038u);
    *dm = parser_load_pointer(parser_load_pointer(decoder) + 16);
    *dm_size = 0x10744;
}

static int logged_error(dovi_validation_log_fn log, unsigned int severity, int error, int line)
{
    if (log != 0 && severity > 2u)
        log("%s+%d: %s\n",
            PARSER_DECODER_DIAGNOSTIC_SOURCE,
            line, dovi_rpu_error_code_2_str(error));
    return error;
}

/* Identity fallback writes only the first partition. */
#if !PARSER_FATAL_ARRAY_LIMITS
static
#endif
void dovi_set_default_payload(void *opaque)
{
    unsigned char *root = parser_load_pointer(opaque);
    unsigned char *header = parser_load_pointer(root), *payload = parser_load_pointer(root + 8);
    unsigned int component, pivot;
    for (component = 0; component < 3; ++component) {
        for (pivot = 0; (int)pivot < (int)header[0x25 + component] - 1; ++pivot) {
            unsigned char *poly = payload + 0x5c + component * 0x30cu + pivot * 0x34u;
#if PARSER_FATAL_ARRAY_LIMITS
            if (pivot == 15u)
                parser_limit_trap();
#endif
            payload[component * 15u + pivot] = 0;
            payload[0x2d + component * 15u + pivot] = 0;
            poly[0] = 1;
            fixed_store_le32(poly + 0xc, 0);
            fixed_store_le32(poly + 0x20, 0);
            fixed_store_le32(poly + 0x10, 1);
            fixed_store_le32(poly + 0x24, 0);
        }
    }
#if PARSER_FATAL_ARRAY_LIMITS
    if (header[0x16])
        return;
    fixed_store_le64(payload + 0x6898, 0);
    fixed_store_le64(payload + 0x68a0, 0);
    payload[0x6846] = 0;
    payload[0x6844] = 0;
    payload[0x6845] = 0;
    payload[0x6874] = 0;
    payload[0x6875] = 0;
    fixed_store_le32(payload + 0x6878, 1);
    fixed_store_le64(payload + 0x687c, 0);
    fixed_store_le64(payload + 0x6884, 0);
    fixed_store_le64(payload + 0x688a, 0);
    fixed_store_le32(payload + 0x6894, 1);
    fixed_store_le64(payload + 0x68a6, 0);
    fixed_store_le32(payload + 0x68b0, 1);
    fixed_store_le64(payload + 0x68b4, 0);
    fixed_store_le64(payload + 0x68bc, 0);
    fixed_store_le32(payload + 0x68c4, 0);
#else
    if (header[0x16] == 0) {
        for (component = 0; component < 3; ++component) {
            unsigned char *nlq = payload + 0x6874u + component * 0x1cu;
            payload[0x6844 + component] = 0;
            nlq[0] = 0;
            nlq[1] = 0;
            fixed_store_le32(nlq + 4, 1);
            fixed_store_le32(nlq + 8, 0);
            fixed_store_le32(nlq + 0xc, 0);
            fixed_store_le32(nlq + 0x10, 0);
            fixed_store_le32(nlq + 0x14, 0);
            fixed_store_le32(nlq + 0x18, 0);
        }
    }
#endif
}

int dovi_rpu_decoder_process_buffer(void *opaque, const unsigned char *input, unsigned int size,
                                    void **header, void **payload, int *payload_size, void **dm,
                                    int *dm_size)
{
    unsigned char *decoder = opaque;
    dovi_validation_log_fn log = logger(decoder + DECODER_LOGGER);
    unsigned int severity = fixed_load_le32(decoder + DECODER_SEVERITY), length;
    unsigned char *data, *rh, *old;
    int result;
    parser_store_pointer(parser_load_pointer(decoder) + 8, parser_load_pointer(decoder + DECODER_CURRENT_PAYLOAD));
    dovi_bitstream_init((struct dovi_bitstream *)(decoder + DECODER_BITSTREAM), input, size);
    result = dovi_rpu_header_read(decoder);
    if (result < 0)
        return logged_error(log, severity, result, PARSER_HEADER_ERROR_LINE);
    length = (unsigned int)result;
    result = dovi_validate_level(parser_load_pointer(decoder), logger(decoder + DECODER_LOGGER),
                                 fixed_load_le32(decoder + DECODER_SEVERITY));
    if (result < 0)
        return result;
    result = dovi_validate_spatial_filter(parser_load_pointer(decoder), logger(decoder + DECODER_LOGGER),
                                          fixed_load_le32(decoder + DECODER_SEVERITY));
    if (result < 0)
        return result;
    data = parser_load_pointer(decoder);
    rh = parser_load_pointer(data);
    if (fixed_signed32(fixed_load_le32(parser_load_pointer(decoder + 8) + 0x66c)) <
        (int)((unsigned int)rh[0x133] * rh[0x134])) {
        /* The native diagnostic omits the %d argument; retain that call contract. */
        if (log != 0 && severity > 2u)
            log("ERROR: Payload buffer too small for %d partitions\n"
#if PARSER_FATAL_ARRAY_LIMITS
                , (unsigned int)rh[0x133] * rh[0x134]
#endif
            );
        return -2;
    }
    if (rh[0] == 2 && rh[0x19] == 0) {
        result = dovi_rpu_payload_read(decoder);
        if (result < 0)
            return logged_error(log, severity, result, PARSER_PAYLOAD_ERROR_LINE);
        length += (unsigned int)result;
        data = parser_load_pointer(decoder);
    }
    result =
        dovi_validate_profile(data, logger(decoder + DECODER_LOGGER), fixed_load_le32(decoder + DECODER_SEVERITY));
    if (result < 0)
        return result;
    if (fixed_load_le32(decoder + DECODER_DISABLE_MEL) == 0 &&
        dovi_detect_mel((const struct dovi_decoder_prefix *)decoder) != 0)
        parser_load_pointer(parser_load_pointer(decoder))[0x16] = 1;
    data = parser_load_pointer(decoder);
    rh = parser_load_pointer(data);
    if (rh[0x18] != 0) {
        result = dovi_dm_data_payload_read(decoder);
        if (result < 0)
            return logged_error(log, severity, result, PARSER_DM_ERROR_LINE);
        length += (unsigned int)result;
        data = parser_load_pointer(decoder);
        rh = parser_load_pointer(data);
    }
    if (rh[0x19] == 0) {
        unsigned char *slot;
#if PARSER_FATAL_ARRAY_LIMITS
        if (rh[0x1b] >= 15u)
            parser_limit_trap();
#endif
        slot = decoder + DECODER_PAYLOAD_SLOTS + rh[0x1b] * 8u;
        old = parser_load_pointer(slot);
        parser_store_pointer(slot, parser_load_pointer(decoder + DECODER_CURRENT_PAYLOAD));
        parser_store_pointer(decoder + DECODER_CURRENT_PAYLOAD, old);
        rh = parser_load_pointer(data);
#if PARSER_FATAL_ARRAY_LIMITS
        if (rh[0x1b] >= 15u)
            parser_limit_trap();
#endif
        parser_store_pointer(data + 8, parser_load_pointer(decoder + DECODER_PAYLOAD_SLOTS + rh[0x1b] * 8u));
    } else if (((fixed_load_le32(decoder + 0x7c) >> (rh[0x1b] & 31u)) & 1u) == 0) {
        dovi_set_default_payload(decoder);
    } else {
#if PARSER_FATAL_ARRAY_LIMITS
        if (rh[0x1a] >= 15u)
            parser_limit_trap();
#endif
        parser_store_pointer(data + 8, parser_load_pointer(decoder + DECODER_PAYLOAD_SLOTS + rh[0x1a] * 8u));
    }
    if (parser_load_pointer(decoder + DECODER_CURRENT_PAYLOAD) == 0 ||
        parser_load_pointer(parser_load_pointer(decoder) + 8) == 0) {
        if (log != 0 && severity > 2u)
            log("Error: payload buffer not allocated\n");
        return -2;
    }
    outputs(decoder, header, payload, payload_size, dm, dm_size);
    return fixed_signed32(length);
}

int dovi_rpu_decoder_get_output(void *opaque, void **header, void **payload, int *payload_size,
                                void **dm, int *dm_size)
{
    unsigned char *decoder = opaque;
    unsigned int index;
    int result;
    if (fixed_load_le32(decoder + 0x315c0) != 0 && fixed_load_le32(decoder + 0x315bc) != 0 &&
        fixed_load_le32(decoder + 0x315b8) != 0) {
        outputs(decoder, header, payload, payload_size, dm, dm_size);
        fixed_store_le32(decoder + 0x315b8, 0);
        return 1;
    }
    index = fixed_load_le32(decoder + DECODER_OUTPUT_SLOT);
    if (index == fixed_load_le32(decoder + DECODER_INPUT_SLOT) && fixed_load_le32(decoder + DECODER_DIRECT_MODE) == 0 &&
        fixed_load_le32(decoder + DECODER_FLUSH) == 0)
        return -20;
    if (fixed_load_le32(parser_load_pointer(decoder + DECODER_SLOT_LENGTHS) + index * 4u) == 0)
        return fixed_load_le32(decoder + DECODER_FLUSH) != 0 ? -21 : -20;
    if (fixed_load_le32(decoder + DECODER_SEVERITY) > 6u) {
        dovi_dump_rpu_buffer_info(decoder);
        index = fixed_load_le32(decoder + DECODER_OUTPUT_SLOT);
    }
    result = dovi_rpu_decoder_process_buffer(
        decoder, parser_load_pointer(decoder + 0x10) + (unsigned long long)index * 0x1000u,
        fixed_load_le32(parser_load_pointer(decoder + DECODER_SLOT_LENGTHS) + index * 4u), header, payload,
        payload_size, dm, dm_size);
    index = fixed_load_le32(decoder + DECODER_OUTPUT_SLOT);
    fixed_store_le32(parser_load_pointer(decoder + DECODER_SLOT_LENGTHS) + index * 4u, 0);
    index = fixed_load_le32(decoder + DECODER_OUTPUT_SLOT);
    fixed_store_le32(decoder + DECODER_OUTPUT_SLOT, (fixed_load_le32(decoder + DECODER_SLOT_COUNT) - 1u) & (index + 1u));
    return result;
}
