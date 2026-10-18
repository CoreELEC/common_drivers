/* Byte copies and pointer representations used by metadata parser adapters. */
#ifndef DOVI_PARSER_ACCESS_H
#define DOVI_PARSER_ACCESS_H

static inline void parser_copy_bytes(void *destination, const void *source,
                                     unsigned long long count)
{
    unsigned char *out = destination;
    const unsigned char *in = source;
    while (count-- != 0)
        *out++ = *in++;
}

static inline void parser_zero_bytes(void *destination, unsigned long long count)
{
    volatile unsigned char *out = destination;
    while (count-- != 0)
        *out++ = 0;
}

static inline void parser_copy8(void *destination, const void *source)
{
    parser_copy_bytes(destination, source, 8);
}

/* Object and function pointer bytes are copied without integer-pointer casts.
 * Callers retain their typed callback declarations and reload them as required. */
static inline unsigned char *parser_load_pointer(const void *source)
{
    void *value;
    parser_copy8(&value, source);
    return value;
}

static inline void parser_store_pointer(void *destination, const void *value)
{
    parser_copy8(destination, &value);
}

/* The host adapter discards five prefix bytes and scans at most 63 trailing
 * zeros. Short inputs retain the native readable-prefix requirement.
 */
static inline unsigned int parser_trimmed_payload_size(const unsigned char *input,
                                                      unsigned int size)
{
    unsigned int cursor_bits = size - 1u;
    unsigned int limit_bits = size - 64u;
    int cursor = (cursor_bits & 0x80000000u) ? -1 - (int)~cursor_bits : (int)cursor_bits;
    int limit = (limit_bits & 0x80000000u) ? -1 - (int)~limit_bits : (int)limit_bits;
    while (cursor > limit && input[cursor] == 0) {
        cursor_bits -= 1u;
        cursor = (cursor_bits & 0x80000000u) ? -1 - (int)~cursor_bits : (int)cursor_bits;
    }
    return cursor_bits - 4u;
}

static inline int parser_clamp_error(int result)
{
    return result < 12 ? result : 12;
}
#endif
