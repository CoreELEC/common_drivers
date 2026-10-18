/* Initialize and destroy metadata parser state. */
#ifndef DOVI_PARSER_LIFECYCLE_H
#define DOVI_PARSER_LIFECYCLE_H
#include "parser_context.h"
#include "parser_access.h"
#include "parser_contract.h"
#define DOVI_PARSER_INIT_CONFIG_SIZE 0x38u

static inline dovi_parser_log_fn parser_load_logger(const void *source)
{
    dovi_parser_log_fn value;
    parser_copy8(&value, source);
    return value;
}
#if !PARSER_RESET_FLAGS
/* Static STB 2.4 storage; create clears only the 0x70-byte parser prefix. */
#define MD_PARSER_SINGLETON_BYTES 334048u
dv_md_parser_ctx_t *dv_md_parser_create(void);
#endif
/* Parser includes decoder storage at +0x70. Configuration and arena are
 * disjoint and stable; access is bytewise and does not require arena alignment.
 * A null parser returns 2 without reading config. Missing callback returns 9
 * after installing the decoder logger/severity. Destroy clears the initialized
 * byte and decoder pointer without freeing or clearing the arena.
 */
int dovi_md_parser_init(dv_md_parser_ctx_t *parser, const dv_md_parser_conf_t *config);
int dovi_md_parser_destroy(dv_md_parser_ctx_t *parser);
#endif
