#include "trim_reduction.h"
#include "fixed_arithmetic.h"
#include "fixed_transcendentals.h"
#include "pq_conversion.h"
#include "byte_utils.h"
#include "trim_tables.inc"

typedef char trim_pointer_is_64[(sizeof(void *) == 8) ? 1 : -1];
typedef char trim_int_is_32[(sizeof(int) == 4) ? 1 : -1];

static int s32(unsigned int x)
{
    return x <= 0x7fffffffu ? (int)x : -1 - (int)~x;
}

static unsigned int le32(const unsigned char *p)
{
    return p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int be16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

static const unsigned char *target_from(const void *address)
{
    const unsigned char *target;
    dovi_MemCpyByte(&target, address, 8);
    return target;
}

static unsigned char *record(unsigned char *md, int index)
{
    return md + 71 + index * 261;
}

static void copy_record(unsigned char *destination, const unsigned char *source)
{
    int i;
    for (i = 0; i < 261; ++i)
        destination[i] = source[i];
}

/* Volatile stores prevent Clang from turning these local clears into
 * an implicit libc memset in the freestanding aggregate build. */
static void clear_ints(int *destination, int count)
{
    volatile int *p = destination;
    int i;
    for (i = 0; i < count; ++i)
        p[i] = 0;
}

/* Adjacent equal values swap, so duplicate keys reverse during the shrinking
 * bubble passes. Retaining this order matters to trim selection. */
static void sort_triplets(int count, int *indices, int *keys, int *l10)
{
    int end, i, changed, tmp;
    for (end = count - 1; end > 0; --end) {
        changed = 0;
        for (i = 0; i < end; ++i)
            if (keys[i + 1] <= keys[i]) {
                tmp = keys[i];
                keys[i] = keys[i + 1];
                keys[i + 1] = tmp;
                tmp = indices[i];
                indices[i] = indices[i + 1];
                indices[i + 1] = tmp;
                if (l10) {
                    tmp = l10[i];
                    l10[i] = l10[i + 1];
                    l10[i + 1] = tmp;
                }
                changed = 1;
            }
        if (!changed)
            break;
    }
}

int sort_l2_by_tmax_lum(const void *metadata, int *indices, int *max_pq)
{
    const unsigned char *md = (const unsigned char *)metadata, *r;
    int i, count = 0;
    for (i = 0; i < md[70]; ++i) {
        r = md + 71 + i * 261;
        if (r[4] == 2) {
            indices[count] = i;
            max_pq[count] = (int)be16(r + 5);
            ++count;
        }
    }
    sort_triplets(count, indices, max_pq, 0);
    return count;
}

static int compact_levels(unsigned char *md, int legacy)
{
    int i, write = 0, count = md[70], level;
    for (i = 0; i < count; ++i) {
        level = record(md, i)[4];
        if (legacy ? (level == 3 || (level >= 8 && level <= 11) || level == 254) : level == 2)
            continue;
        copy_record(record(md, write), record(md, i));
        ++write;
    }
    md[70] = (unsigned char)write;
    return write;
}

/* A -1 key denotes a trim with no matching target display. The known-endpoint
 * requirement applies to the first/last selections, not interval matching. */
static void select_pair(int count, const int *indices, const int *keys, const int *l10, int target,
                        int require_known_endpoint, int mark, int *remove)
{
    int i, last = count - 1;
    if (count <= 0)
        return;
    for (i = 0; i < count; ++i) {
        if (mark) {
            remove[indices[i]] = 1;
            if (l10 && l10[i] != -1)
                remove[l10[i]] = 1;
        }
        if (keys[last] < target && (!require_known_endpoint || keys[last] != -1)) {
            remove[indices[last]] = 0;
            if (l10 && l10[last] != -1)
                remove[l10[last]] = 0;
        } else if (target <= keys[0] && (!require_known_endpoint || keys[0] != -1)) {
            remove[indices[0]] = 0;
            if (l10 && l10[0] != -1)
                remove[l10[0]] = 0;
        } else if (i > 0 && keys[i - 1] < target && target <= keys[i]) {
            remove[indices[i - 1]] = 0;
            remove[indices[i]] = 0;
            if (l10) {
                if (l10[i - 1] != -1)
                    remove[l10[i - 1]] = 0;
                if (l10[i] != -1)
                    remove[l10[i]] = 0;
            }
        }
    }
}

static int display_index(int id)
{
    int i;
    for (i = 0; i < 7; ++i)
        if (trim_display_table[i][0] == id)
            return i;
    return -1;
}

static int level10_index(unsigned char *md, int id)
{
    int i;
    for (i = 0; i < md[70]; ++i)
        if (record(md, i)[4] == 10 && record(md, i)[5] == id)
            return i;
    return -1;
}

int reduce_trims(const void *target_pointer_address, void *metadata)
{
    unsigned char *md = (unsigned char *)metadata, *r, *r10;
    const unsigned char *target = target_from(target_pointer_address);
    int indices[16], l10[16], max_pq[16], dist[16], remove[255];
    int i, j, count = 0, mode = 0, display, idx, sink = s32(le32(target + 52)), primaries[8], write,
              total;
    const int *prim;
    unsigned int linear, distance;
    clear_ints(indices, 16);
    clear_ints(l10, 16);
    clear_ints(max_pq, 16);
    clear_ints(dist, 16);
    clear_ints(remove, 255);
    l10[0] = -1;
    if (le32(target + 24) <= 1u)
        compact_levels(md, 1);
    else {
        for (i = 0; i < md[70]; ++i)
            if (record(md, i)[4] == 254) {
                mode = record(md, i)[6] >= 2;
                break;
            }
    }
    if (!mode) {
        count = sort_l2_by_tmax_lum(md, indices, max_pq);
        select_pair(count, indices, max_pq, 0, sink, 0, 1, remove);
    } else {
        compact_levels(md, 0);
        for (i = 0; i < md[70]; ++i) {
            r = record(md, i);
            if (r[4] != 8)
                continue;
            indices[count] = i;
            display = display_index(r[5]);
            if (display != -1) {
                l10[count] = -1;
                max_pq[count] = trim_display_table[display][2] / 64;
            } else if ((idx = level10_index(md, r[5])) != -1) {
                l10[count] = idx;
                max_pq[count] = (int)be16(record(md, idx) + 6);
            } else
                max_pq[count] = -1;
            ++count;
        }
        sort_triplets(count, indices, max_pq, l10);
        select_pair(count, indices, max_pq, l10, sink, 1, 1, remove);
        target = target_from(target_pointer_address);
        linear = dovi_PQ12ToL((unsigned short)le32(target + 52));
        distance = dovi_dm_FindChromaDist(target + 56, linear,
                                          s32(le32(target_from(target_pointer_address) + 52) << 6));
        count = 0;
        for (i = 0; i < md[70]; ++i) {
            r = record(md, i);
            if (r[4] != 8)
                continue;
            indices[count] = i;
            display = display_index(r[5]);
            if (display != -1) {
                l10[count] = -1;
                linear = (unsigned int)trim_display_table[display][1];
                prim = trim_primaries_table[trim_display_table[display][3]];
                dist[count] =
                    s32(dovi_dm_FindChromaDist(prim, linear, trim_display_table[display][2]));
            } else if ((idx = level10_index(md, r[5])) != -1) {
                l10[count] = idx;
                r10 = record(md, idx);
                j = (int)be16(r10 + 6);
                linear = dovi_PQ12ToL((unsigned short)j);
                if (r10[10] < 19)
                    prim = trim_primaries_table[r10[10]];
                else {
                    for (idx = 0; idx < 8; ++idx)
                        primaries[idx] = (int)(be16(r10 + 11 + idx * 2) << 11);
                    prim = primaries;
                }
                dist[count] = s32(dovi_dm_FindChromaDist(prim, linear, s32((unsigned int)j << 6)));
            } else
                dist[count] = -1;
            ++count;
        }
        sort_triplets(count, indices, dist, l10);
        select_pair(count, indices, dist, l10, s32(distance), 1, 0, remove);
    }
    total = md[70];
    write = 0;
    for (i = 0; i < total; ++i)
        if (remove[i] != 1) {
            copy_record(record(md, write), record(md, i));
            ++write;
        }
    md[70] = (unsigned char)write;
    return 0;
}
