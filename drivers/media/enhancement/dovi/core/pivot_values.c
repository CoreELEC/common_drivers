/* Accumulate RPU mapping and nonlinear-quantizer pivot positions. */
#include "pivot_values.h"
#include "parser_api.h"
#include "fixed_access.h"

typedef char pivot_requires_64_bit_pointers[(sizeof(void *) == 8) ? 1 : -1];
typedef char pivot_requires_32_bit_uint[(sizeof(unsigned int) == 4) ? 1 : -1];

static int accumulate(unsigned char *header, unsigned int source, unsigned int destination,
                      unsigned int count, unsigned int depth)
{
    unsigned int limit = 1u << (depth & 31u);
    unsigned int value = fixed_load_le16(header + source);
    unsigned int i;

    fixed_store_le16(header + destination, value);
    if ((limit & 0x80000000u) != 0u || value >= limit)
        return -13;
    for (i = 1; i < count; ++i) {
#if PARSER_FATAL_ARRAY_LIMITS
        if (i == 16)
            parser_limit_trap();
#endif
        value = (fixed_load_le16(header + source + i * 2u) +
                 fixed_load_le16(header + destination + (i - 1u) * 2u)) &
                0xffffu;
        fixed_store_le16(header + destination + i * 2u, value);
        if (value >= limit)
            return -13;
    }
    return 0;
}

int dovi_assign_pivot_values(void *root)
{
    unsigned char *header = (unsigned char *)fixed_load_le64(root);
    unsigned int component;

    for (component = 0; component < 3u; ++component) {
        int result = accumulate(header, 0x28u + component * 0x20u, 0x88u + component * 0x20u,
                                header[0x25u + component], header[0x0fu]);
        if (result != 0)
            return result;
    }
    return 0;
}

int dovi_assign_nlq_pivot_values(void *root)
{
    unsigned char *header = (unsigned char *)fixed_load_le64(root);
    return accumulate(header, 0xf2u, 0x112u, header[0xf1u], header[0x10u]);
}
