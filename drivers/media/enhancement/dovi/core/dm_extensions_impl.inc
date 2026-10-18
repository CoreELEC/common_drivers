/* Sort, replace and remove extension records without copying padding. */
typedef unsigned char byte;
typedef unsigned int u32;
typedef char require_uint32[(sizeof(u32) == 4) ? 1 : -1];
typedef char require_pointer64[(sizeof(void *) == 8) ? 1 : -1];

static void copy_record(byte *dest, const byte *source)
{
    u32 i;
    for (i = 0; i < 4u; ++i)
        dest[i] = source[i];
    dest[4] = source[4];
    for (i = 6; i < 0x106u; ++i)
        dest[i] = source[i];
}

/* Read the sentinel, including when position == count. */
static byte scratch_type(const byte *scratch, u32 position)
{
    return ((const volatile byte *)scratch)[position * 0x108u + 4u];
}

static u32 sort_existing(byte *dm, byte *scratch)
{
    u32 copied = 0, type;
    for (type = 1; type < 256u; ++type) {
        u32 i;
        for (i = 0; i < dm[0x4a]; ++i) {
            const byte *record = dm + 0x4c + i * 0x108u;
            if (record[4] == type) {
                copy_record(scratch + copied * 0x108u, record);
                ++copied;
            }
        }
        if (copied == dm[0x4a])
            break;
    }
    return copied;
}

void dovi_remove_dm_extensions(void *dm, const unsigned char *remove, void *scratch)
{
    byte *d = (byte *)dm;
    byte *s = (byte *)scratch;
    u32 count = sort_existing(d, s);
    u32 position = 0, written = 0, type;

    for (type = 1; type < 256u; ++type) {
        if (scratch_type(s, position) == type && position < count) {
            do {
                if (remove[type] == 0) {
                    copy_record(d + 0x4c + (written & 255u) * 0x108u, s + position * 0x108u);
                    ++written;
                }
                ++position;
            } while (scratch_type(s, position) == type && position < count);
        }
        if ((position & 255u) == count)
            break;
    }
    d[0x4a] = (byte)written;
}

void dovi_update_dm_extensions(void *dm, const void *blocks, unsigned int count, void *scratch)
{
    byte *d = (byte *)dm;
    const byte *input = (const byte *)blocks;
    byte *s = (byte *)scratch;
    u32 old_count = sort_existing(d, s);
    u32 old_position = 0, written = 0, used_new = 0, type;

    for (type = 1; type < 256u; ++type) {
        u32 i;
        int replaced = 0;
        if (count != 0 && count < 0x80000000u) {
            for (i = 0; i < count; ++i) {
                const byte *record = input + (unsigned long long)i * 0x108u;
                if (record[4] != type)
                    continue;
                /* The type is read before the boundary condition. */
                while (scratch_type(s, old_position) == type && old_position < old_count)
                    ++old_position;
                copy_record(d + 0x4c + (written & 255u) * 0x108u, record);
                ++written;
                ++used_new;
                replaced = 1;
            }
        }
        if (!replaced && scratch_type(s, old_position) == type && old_position < old_count) {
            do {
                copy_record(d + 0x4c + (written & 255u) * 0x108u, s + old_position * 0x108u);
                ++written;
                ++old_position;
            } while (scratch_type(s, old_position) == type && old_position < old_count);
        }
        if (used_new == count && old_position == old_count)
            break;
    }
    d[0x4a] = (byte)written;
}
