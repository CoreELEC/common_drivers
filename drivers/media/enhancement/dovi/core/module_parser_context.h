/* Metadata-parser context and backing storage for the module adapter. */
#ifndef DOVI_MODULE_PARSER_CONTEXT_H
#define DOVI_MODULE_PARSER_CONTEXT_H
#include "module_parser.h"
#include "parser_context.h"

struct dovi_module_parser_context {
    const unsigned char *input;
    unsigned char *composer_output;
    unsigned char *metadata_output;
    unsigned char composer_config[0x708];
    unsigned char *metadata_sequence;
    dv_md_parser_conf_t config;
    dv_md_parser_ctx_t *parser;
    unsigned int composer_size;
    unsigned int metadata_size;
};

#define PARSER_CONTEXT_OFFSET(member, offset) \
    _Static_assert(__builtin_offsetof(struct dovi_module_parser_context, member) == (offset), \
                   "parser context: " #member)
PARSER_CONTEXT_OFFSET(input, 0x000);
PARSER_CONTEXT_OFFSET(composer_output, 0x008);
PARSER_CONTEXT_OFFSET(metadata_output, 0x010);
PARSER_CONTEXT_OFFSET(composer_config, 0x018);
PARSER_CONTEXT_OFFSET(metadata_sequence, 0x720);
PARSER_CONTEXT_OFFSET(config.pf_md_parser_cb, 0x728);
PARSER_CONTEXT_OFFSET(config.p_md_parser_cb_ctx, 0x730);
PARSER_CONTEXT_OFFSET(config.flags, 0x738);
PARSER_CONTEXT_OFFSET(config.pf_msg_log, 0x758);
PARSER_CONTEXT_OFFSET(parser, 0x760);
PARSER_CONTEXT_OFFSET(composer_size, 0x768);
PARSER_CONTEXT_OFFSET(metadata_size, 0x76c);
_Static_assert(sizeof(struct dovi_module_parser_context) == DOVI_MODULE_PARSER_CONTEXT_SIZE,
               "parser context size");
#undef PARSER_CONTEXT_OFFSET
#endif
