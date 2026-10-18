/* Native validation table words in their original row order. */
static const unsigned int level_limits[16][5] = {
    {1u, 1u, 9u, 2u, 0u},
    {4u, 4u, 11u, 2u, 3u},
    {8u, 8u, 17u, 2u, 7u}
};
/* Flat rows make cross-field sentinel searches defined C pointer arithmetic. */
static const unsigned int profile_limits[] = {
    /* Profile 0: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u,
    /* Profile 1: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, 1u, ~0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 1u, 0u,
    /* Profile 2: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, 3u, 1u, 2u, ~0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 1u, ~0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 1u, 2u, ~0u, 1u, 1u,
    /* Profile 3: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u,
    /* Profile 4: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u,
    /* Profile 5: mapping method[16], colour[16], chroma[4], resampling[2]. */
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, ~0u, 0u, 0u, 0u, 0u,
    /* Profile 6: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 7: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 8: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 9: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 10: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 11: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 12: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 13: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 14: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
    /* Profile 15: mapping method[16], colour[16], chroma[4], resampling[2]. */
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
    ~0u, 0u, 0u, 0u, 0u, 0u,
#if PARSER_FATAL_ARRAY_LIMITS
    /* The optional profile-16 row is retained behind the array-limit policy. */
    0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0x00000007u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
    0x00000000u, 0xffffffffu, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
#endif
};
