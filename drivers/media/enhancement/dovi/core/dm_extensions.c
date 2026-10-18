/* Display-metadata extension record updates. */
#include "dm_extensions.h"

#include "dm_extensions_impl.inc"

void dovi_append_dm_extensions(void *dm, const void *blocks, unsigned char count)
{
    byte *d = (byte *)dm;
    const byte *input = (const byte *)blocks;
    u32 original_count = d[0x4a], i;
    for (i = 0; i < count; ++i)
        copy_record(d + 0x4c + (original_count + i) * 0x108u, input + i * 0x108u);
    d[0x4a] = (byte)(d[0x4a] + count);
}
