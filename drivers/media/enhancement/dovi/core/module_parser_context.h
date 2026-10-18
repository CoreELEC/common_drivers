/* Metadata-parser context and backing storage for the module adapter. */
#ifndef DOVI_MODULE_PARSER_CONTEXT_H
#define DOVI_MODULE_PARSER_CONTEXT_H
#include "parser_context.h"

/* Metadata application context. The output payloads and composer_config
 * remain opaque here because the initializer only clears/binds them.
 */
struct dovi_module_parser_context {
    const unsigned char *input;
    unsigned char *composer_output;
    unsigned char *metadata_output;
    struct rpu_data_header_s *rpu_data_header;
    struct vdr_rpu_data_payload_s *vdr_rpu_data_payload;
    struct vdr_dm_data_payload_s *vdr_dm_data_payload;
    unsigned char composer_config[0x708];
    unsigned char *metadata_sequence;
    dv_md_parser_conf_t config;
    dv_md_parser_ctx_t *parser;
};

/* This view preserves contiguous static storage and padding,
 * including bytes that the callback does not clear.
 */
struct metadata_init_storage {
    struct dovi_module_parser_context ctx;
    unsigned char rpu_data_header[310];
    unsigned char header_padding[2];
    unsigned char vdr_rpu_data_payload[28728];
    unsigned char vdr_dm_data_payload[67396];
    unsigned char dm_payload_padding[4];
    unsigned char metadata_sequence[66626];
    unsigned char sequence_padding[2];
    int rc;
};

/* Persistent event counters. frame_nr is incremented for every
 * event, including errors and unknown events, and is never reset by process.
 */
struct metadata_counters {
    unsigned int frame_nr;
    int comp_size;
    int md_size;
};

/* Static app storage and persistent counters share one parser/decoder.
 * Initialization replaces the callback context and logger, preserving counters.
 * Input/output pointers retain the unchecked metadata callback contracts.
 */
struct dovi_module_parser_storage {
    struct metadata_init_storage storage;
    struct metadata_counters counters;
    msg_log_func_t logger;
};

extern struct dovi_module_parser_storage dovi_module_parser_state;

#define PARSER_CONTEXT_OFFSET(member, offset) \
    _Static_assert(__builtin_offsetof(struct dovi_module_parser_context, member) == (offset), \
                   "parser context: " #member)
PARSER_CONTEXT_OFFSET(input, 0x000);
PARSER_CONTEXT_OFFSET(composer_output, 0x008);
PARSER_CONTEXT_OFFSET(metadata_output, 0x010);
PARSER_CONTEXT_OFFSET(rpu_data_header, 0x018);
PARSER_CONTEXT_OFFSET(vdr_rpu_data_payload, 0x020);
PARSER_CONTEXT_OFFSET(vdr_dm_data_payload, 0x028);
PARSER_CONTEXT_OFFSET(composer_config, 0x030);
PARSER_CONTEXT_OFFSET(metadata_sequence, 0x738);
PARSER_CONTEXT_OFFSET(config, 0x740);
PARSER_CONTEXT_OFFSET(parser, 0x778);
_Static_assert(sizeof(struct dovi_module_parser_context) == 0x780, "parser context size");
#undef PARSER_CONTEXT_OFFSET
_Static_assert(offsetof(struct metadata_init_storage, rpu_data_header) == 0x780, "header offset");
_Static_assert(offsetof(struct metadata_init_storage, vdr_rpu_data_payload) == 0x8b8,
               "rpu payload offset");
_Static_assert(offsetof(struct metadata_init_storage, vdr_dm_data_payload) == 0x78f0,
               "dm payload offset");
_Static_assert(offsetof(struct metadata_init_storage, metadata_sequence) == 0x18038,
               "sequence offset");
_Static_assert(offsetof(struct metadata_init_storage, rc) == 0x2847c, "rc offset");
_Static_assert(sizeof(struct metadata_init_storage) == 0x28480, "metadata storage size");
_Static_assert(offsetof(struct dovi_module_parser_storage, storage) == 0x000,
               "module storage starts with metadata storage");
_Static_assert(offsetof(struct metadata_init_storage, ctx) == 0x000,
               "metadata storage starts with event context");
#endif
