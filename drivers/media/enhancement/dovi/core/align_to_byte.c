/* Advance the bitstream to a byte boundary, refilling its cache as needed. */
#include "align_to_byte.h"

typedef char align_u32_check[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char align_ptr_check[(sizeof(void *) == 8) ? 1 : -1];
typedef char align_state_check[(sizeof(struct dovi_bitstream) == 32) ? 1 : -1];

void align_to_byte(struct dovi_bitstream *state)
{
    unsigned int deficit = state->deficit;

    if (deficit != 0u && deficit < 0x80000000u) {
        const unsigned char *cursor = state->next;
        unsigned int chunk;

        if ((unsigned long long)cursor > (unsigned long long)state->tail + 4u)
            cursor = state->tail;
        chunk = ((unsigned int)cursor[0] << 8) | cursor[1];
        state->next = cursor + 2;
        state->cache |= chunk << (deficit & 31u);
        deficit -= 16u;
        state->deficit = deficit;
    }

    if ((deficit & 7u) != 0u) {
        unsigned int cache = state->cache;
        do {
            cache <<= 1;
            deficit += 1u;
        } while ((deficit & 7u) != 0u);
        state->cache = cache;
        state->deficit = deficit;
    }
}
