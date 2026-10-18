/* Initialize decoder storage and install callbacks. */
#include "decoder_lifecycle.h"
#include "fixed_access.h"
#include "parser_access.h"
#include "parser_layout.h"
#include "parser_api.h"

typedef char decoder_lifecycle_uint32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char decoder_lifecycle_uint64[(sizeof(unsigned long long) == 8) ? 1 : -1];
typedef char decoder_lifecycle_pointer64[(sizeof(void *) == 8) ? 1 : -1];
typedef char decoder_lifecycle_callbacks64
    [(sizeof(rpu_msg_log_func_t) == 8 && sizeof(dec_event_callback_t) == 8) ? 1 : -1];
typedef char decoder_lifecycle_severity32[(sizeof(rpu_print_severity_t) == 4) ? 1 : -1];

static const unsigned int level_limits[16][5] = {
    {1u, 1u, 9u, 2u, 0u}, {4u, 4u, 11u, 2u, 3u},
    {8u, 8u, 17u, 2u, 7u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}, {0u, 0u, 0u, 0u, 0u}
};

int dovi_rpu_decoder_init(void *decoder, const unsigned char *config)
{
    unsigned char *arena = (unsigned char *)decoder;
    unsigned int payload_only, input_slots, i, max_prev, partitions, level;
    const unsigned int *limits;
    unsigned char *input_buffer, *slot_levels, *rpu_context, *rpu_data, *rpu_header, *dm_payload,
        *partition_payload;

    parser_zero_bytes(arena, 0x316e8u);
    payload_only = fixed_load_le32(config + 0xc);
    fixed_store_le32(arena + DECODER_DIRECT_MODE, payload_only);
    fixed_store_le32(arena + DECODER_DISABLE_MEL, fixed_load_le32(config + 0x14));
    fixed_store_le32(arena + DECODER_DISABLE_CRC, fixed_load_le32(config + 0x8));
    input_slots = payload_only == 0 ? 64u : 1u;
    fixed_store_le32(arena + DECODER_SLOT_COUNT, input_slots);

    input_buffer = arena + 0x316e8;
    slot_levels = input_buffer + input_slots * 0x1000u;
    parser_store_pointer(arena + DECODER_INPUT_STORAGE, input_buffer);
    parser_store_pointer(arena + DECODER_SLOT_LENGTHS, slot_levels);
    /* Clear precisely 4100 bytes, even with 64 input slots. */
    parser_zero_bytes(input_buffer, 0x1004u);
    rpu_context = slot_levels + input_slots * 4u;
    parser_store_pointer(arena + DECODER_CONFIG, rpu_context);
    parser_zero_bytes(rpu_context, 0x87cu);
    fixed_store_le32(arena + DECODER_SEQUENCE_AVAILABLE, 0);
    fixed_store_le32(arena + 0x7c, 0);
    fixed_store_le32(arena + 0x80, 0);

    level = fixed_load_le32(config + 4);
#if PARSER_FATAL_ARRAY_LIMITS
    if (level >= 16u)
        parser_limit_trap();
#endif
    limits = level_limits[level];
    partitions = limits[0] * limits[1];
    max_prev = limits[4];
    fixed_store_le32(rpu_context + 0x66c, partitions);
    fixed_store_le32(rpu_context + 0x670, limits[2]);
    fixed_store_le32(rpu_context + 0x674, limits[3]);
    fixed_store_le32(rpu_context + 0x678, max_prev);

    rpu_data = rpu_context + 0x87c;
    parser_store_pointer(arena, rpu_data);
    parser_zero_bytes(rpu_data, 0x18u);
    rpu_header = rpu_data + 0x18;
    parser_store_pointer(rpu_data, rpu_header);
    parser_zero_bytes(rpu_header, 0x136u);
    dm_payload = rpu_header + 0x136;
    parser_store_pointer(rpu_data + 0x10, dm_payload);
    parser_zero_bytes(dm_payload, 0x10744u);
    partition_payload = dm_payload + 0x10744;
    parser_store_pointer(arena + DECODER_PAYLOAD_STORAGE, partition_payload);
    /* Fixed two-payload clear; larger partition arenas retain caller bytes. */
    parser_zero_bytes(partition_payload, 0xe070u);

    for (i = 1; i < max_prev + 2u; ++i) {
        unsigned int partition_offset = i * partitions;
        parser_store_pointer(arena + DECODER_PAYLOAD_STORAGE + i * 8u,
                             partition_payload + (unsigned long long)partition_offset * 0x7038u);
    }
    for (i = 0; i < max_prev + 1u; ++i) {
        unsigned int partition_offset = i * partitions;
        parser_store_pointer(arena + DECODER_PAYLOAD_SLOTS + i * 8u,
                             partition_payload + (unsigned long long)partition_offset * 0x7038u);
    }
    parser_store_pointer(arena + DECODER_CURRENT_PAYLOAD,
                         partition_payload +
                             (unsigned long long)((max_prev + 1u) * partitions) * 0x7038u);

    fixed_store_le32(arena + DECODER_FRAMING_STATE, payload_only == 0 ? 0u : 4u);
    fixed_store_le32(arena + DECODER_INPUT_SLOT, payload_only == 0 ? input_slots - 1u : 0u);
    fixed_store_le32(arena + DECODER_OUTPUT_SLOT, 0);
    for (i = 0; i < input_slots; ++i)
        fixed_store_le32(slot_levels + i * 4u, 0);
    return 0;
}

int dovi_rpu_decoder_cleanup(void *decoder)
{
    (void)decoder;
    return 0;
}

void dovi_rpu_decoder_set_msg_log(void *decoder, rpu_msg_log_func_t msg_log,
                                  unsigned int print_severity)
{
    parser_copy8((unsigned char *)decoder + DECODER_LOGGER, &msg_log);
    fixed_store_le32((unsigned char *)decoder + DECODER_SEVERITY, print_severity);
}

void dovi_register_dec_event_callback(void *decoder, dec_event_callback_t callback)
{
    parser_copy8((unsigned char *)decoder + DECODER_EVENT_CALLBACK, &callback);
}
