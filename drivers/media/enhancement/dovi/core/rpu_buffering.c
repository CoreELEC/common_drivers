/* Accumulate RPU bytes into decoder slots and report buffer state. */
#include "rpu_buffering.h"
#include "fixed_access.h"
#include "parser_access.h"
#include "parser_layout.h"
#include "parser_api.h"

typedef char require_uint32[(sizeof(unsigned int) == 4 && sizeof(int) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8 && sizeof(rpu_msg_log_func_t) == 8) ? 1 : -1];

static rpu_msg_log_func_t read_log(const unsigned char *p)
{
    rpu_msg_log_func_t value;
    parser_copy8(&value, p);
    return value;
}

void dovi_dump_rpu_buffer_info(void *decoder)
{
    unsigned char *decoder_bytes = decoder;
    if (read_log(decoder_bytes + DECODER_LOGGER) != 0) {
        unsigned long long i;
        unsigned int j;
        read_log(decoder_bytes + DECODER_LOGGER)("RPU buffer:\n");
        read_log(decoder_bytes + DECODER_LOGGER)("Level   ");
        for (i = 0; i < fixed_load_le32(decoder_bytes + DECODER_SLOT_COUNT); ++i)
            read_log(decoder_bytes + DECODER_LOGGER)(
                " %4d", fixed_signed32(
                            fixed_load_le32(parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) + i * 4u)));
        read_log(decoder_bytes + DECODER_LOGGER)("\n");
        read_log(decoder_bytes + DECODER_LOGGER)("Buffer  ");
        for (j = 0; j < fixed_load_le32(decoder_bytes + DECODER_SLOT_COUNT); ++j)
            read_log(decoder_bytes + DECODER_LOGGER)(" %4d", fixed_signed32(j));
        read_log(decoder_bytes + DECODER_LOGGER)("\n");
    }
}

static unsigned int current_length(const unsigned char *decoder_bytes)
{
    return fixed_load_le32(parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) +
                           (unsigned long long)fixed_load_le32(decoder_bytes + DECODER_INPUT_SLOT) * 4u);
}

static void append_byte(unsigned char *decoder_bytes, unsigned char value)
{
    unsigned int slot = fixed_load_le32(decoder_bytes + DECODER_INPUT_SLOT);
    unsigned char *length =
        parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) + (unsigned long long)slot * 4u;
    unsigned char *buffer = parser_load_pointer(decoder_bytes + DECODER_INPUT_STORAGE);
    unsigned int previous = fixed_load_le32(length);
    fixed_store_le32(length, previous + 1u);
#if PARSER_FATAL_ARRAY_LIMITS
    if (previous > 0xfffu)
        parser_limit_trap();
    buffer[(unsigned long long)slot * 0x1000u + previous] = value;
#else
    (buffer + (unsigned long long)slot * 0x1000u)[fixed_signed32(previous)] = value;
#endif
}

int dovi_rpu_decoder_push_data(void *decoder, const unsigned char *data, int length,
                               int flush)
{
    unsigned char *decoder_bytes = decoder;
    unsigned int remaining;
    if (decoder_bytes == 0 || length < 0 || (data == 0 && length != 0))
        return -7;
    if (flush != 0)
        fixed_store_le32(decoder_bytes + DECODER_FLUSH, 1);
    if (length != 0) {
        if (fixed_load_le32(decoder_bytes + DECODER_DIRECT_MODE) != 0) {
            unsigned int count = (unsigned int)length;
            while (data[(long long)fixed_signed32(count - 1u)] == 0)
                --count;
            parser_copy_bytes(parser_load_pointer(decoder_bytes + DECODER_INPUT_STORAGE) +
                                  (unsigned long long)fixed_load_le32(decoder_bytes + DECODER_INPUT_SLOT) *
                                      0x1000u,
                              data, (unsigned long long)(long long)fixed_signed32(count));
            fixed_store_le32(parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) +
                                 (unsigned long long)fixed_load_le32(decoder_bytes + DECODER_INPUT_SLOT) * 4u,
                             count);
        } else {
            for (remaining = (unsigned int)length; remaining != 0; --remaining, ++data) {
                unsigned int state = fixed_load_le32(decoder_bytes + DECODER_FRAMING_STATE);
                unsigned char value = (!PARSER_FATAL_ARRAY_LIMITS || state <= (unsigned int)RBSP_FOUND_0000) ? *data : 0;
                switch (state) {
                case RBSP_SEARCH_RPU:
                    if (value == 0)
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_START_00);
                    break;
                case RBSP_FOUND_START_00:
                    fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE,
                                     value == 0 ? (unsigned int)RBSP_FOUND_START_0000
                                                : (unsigned int)RBSP_SEARCH_RPU);
                    break;
                case RBSP_FOUND_START_0000:
                    fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE,
                                     value == 0   ? (unsigned int)RBSP_FOUND_START_0000
                                     : value == 1 ? (unsigned int)RBSP_FOUND_START_000001
                                                  : (unsigned int)RBSP_SEARCH_RPU);
                    break;
                case RBSP_FOUND_START_000001:
                    if (value != 0x19) {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_SEARCH_RPU);
                    } else {
                        unsigned int slot = (fixed_load_le32(decoder_bytes + DECODER_SLOT_COUNT) - 1u) &
                                            (fixed_load_le32(decoder_bytes + DECODER_INPUT_SLOT) + 1u);
                        fixed_store_le32(decoder_bytes + DECODER_INPUT_SLOT, slot);
                        if (fixed_load_le32(parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) +
                                            (unsigned long long)slot * 4u) != 0) {
                            dovi_dump_rpu_buffer_info(decoder_bytes);
                            return -22;
                        }
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_RPU);
                    }
                    break;
                case RBSP_FOUND_RPU:
                    if (value == 0)
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_00);
                    else
                        append_byte(decoder_bytes, value);
                    break;
                case RBSP_FOUND_00:
                    if (value == 0) {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_0000);
                    } else {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_RPU);
                        append_byte(decoder_bytes, 0);
                        append_byte(decoder_bytes, value);
                    }
                    break;
                case RBSP_FOUND_0000:
                    if (value == 0) {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_START_0000);
                    } else if (value == 1) {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_START_000001);
                    } else if (value == 2) {
                        return -1;
                    } else if (value == 3) {
                        append_byte(decoder_bytes, 0);
                        append_byte(decoder_bytes, 0);
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_RPU);
                    } else {
                        fixed_store_le32(decoder_bytes + DECODER_FRAMING_STATE, RBSP_FOUND_RPU);
                        append_byte(decoder_bytes, 0);
                        append_byte(decoder_bytes, 0);
                        append_byte(decoder_bytes, value);
                    }
                    break;
                default:
                    break;
                }
                if (fixed_signed32(current_length(decoder_bytes)) > 0x1000) {
                    dovi_dump_rpu_buffer_info(decoder_bytes);
                    return -22;
                }
            }
        }
    }
    if (fixed_load_le32(decoder_bytes + DECODER_SEVERITY) > 6u)
        dovi_dump_rpu_buffer_info(decoder_bytes);
    return 0;
}

int dovi_rpu_decoder_last_rpu_output(const void *decoder)
{
    const unsigned char *decoder_bytes = decoder;
    if (fixed_load_le32(parser_load_pointer(decoder_bytes + DECODER_SLOT_LENGTHS) +
                        (unsigned long long)fixed_load_le32(decoder_bytes + DECODER_OUTPUT_SLOT) * 4u) != 0)
        return 0;
    return fixed_load_le32(decoder_bytes + DECODER_FLUSH) != 0;
}
