/* Calculate parser and decoder storage from level and buffering mode. */
#include "parser_api.h"
#include "memory_query.h"

typedef char mem_require_u32[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char mem_require_u64[(sizeof(unsigned long long) == 8) ? 1 : -1];

/* All five words per level are retained although only 0, 1, and 4 are read. */
static const unsigned int level_limits[16][5] = {
    {1u, 1u, 9u, 2u, 0u},
    {4u, 4u, 11u, 2u, 3u},
    {8u, 8u, 17u, 2u, 7u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u},
    {0u, 0u, 0u, 0u, 0u}
};

static unsigned int read_le32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

unsigned long long dovi_rpu_decoder_mem_query(const unsigned char *config)
{
    unsigned int level = read_le32(config + 4u);
#if !PARSER_FATAL_ARRAY_LIMITS
    unsigned int payload_only = read_le32(config + 12u);
#endif
    unsigned int units;
    unsigned long long base;
#if PARSER_FATAL_ARRAY_LIMITS
    if (level >= 16u)
        parser_limit_trap();
#endif
    units = level_limits[level][4] + 2u;

    units *= level_limits[level][0];
    units *= level_limits[level][1];
#if PARSER_FATAL_ARRAY_LIMITS
    base = read_le32(config + 12u) == 0u ? 0x828f6ull : 0x437faull;
#else
    base = payload_only == 0u ? 0x828f6ull : 0x437faull;
#endif
    return (unsigned long long)units * 0x7038ull + base;
}

unsigned long long dovi_md_parser_mem_query(const unsigned char *config)
{
    return dovi_rpu_decoder_mem_query(config + 0x10u) + 0x70ull;
}
