#ifndef DOVI_MEMORY_QUERY_H
#define DOVI_MEMORY_QUERY_H

#define DOVI_RPU_MEMORY_CONFIG_MIN_SIZE 0x10u
#define DOVI_MD_MEMORY_CONFIG_MIN_SIZE 0x20u
#define DOVI_MEMORY_QUERY_LEVEL_COUNT 16u

/*
 * The decoder reads little-endian u32 values at config+4 (level) and config+12
 * (payload-only mode). The parser wrapper passes config+16 to the decoder.
 * Callers provide readable configuration and levels 0..15. With
 * PARSER_FATAL_ARRAY_LIMITS, a level >=16 traps; otherwise indexing is unchecked.
 * These queries do not validate pointers or lengths.
 */
unsigned long long dovi_rpu_decoder_mem_query(const unsigned char *config);
unsigned long long dovi_md_parser_mem_query(const unsigned char *config);

#endif
