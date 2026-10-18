/* Publish Dolby metadata to the host setting. */
#include "control_publish.h"
#include "control_runtime.h"
#include "fixed_access.h"

static unsigned int read32(const void *object, unsigned int offset)
{
    return fixed_load_le32((const unsigned char *)object + offset);
}

static void write32(void *object, unsigned int offset, unsigned int value)
{
    fixed_store_le32((unsigned char *)object + offset, value);
}

static void copy_bytes(void *destination, const void *source, unsigned int count)
{
    unsigned char *d = (unsigned char *)destination;
    const unsigned char *s = (const unsigned char *)source;
    unsigned int i;
    for (i = 0; i < count; ++i)
        d[i] = s[i];
}

static unsigned int extension_length(const unsigned char *extension)
{
    return ((unsigned int)extension[0] << 24) | ((unsigned int)extension[1] << 16) |
           ((unsigned int)extension[2] << 8) | (unsigned int)extension[3];
}

static void pack_byte(unsigned char **destination, unsigned int *word, unsigned int *shift,
                      unsigned char value)
{
    *word |= (unsigned int)value << *shift;
    *shift += 8;
    if (*shift == 32) {
        write32(*destination, 0, *word);
        *destination += 4;
        *shift = 0;
        *word = 0;
    }
}

/* Size is calculated before any raw or extension payload output is written.
 * Overflow retains those previous output bytes after clearing availability.
 * The first metadata byte shares word0 with the serialized length; subsequent
 * bytes start with scene_refresh_flag in word1.
 */
void dovi_publish_dolby_metadata(void *output, const unsigned char *metadata)
{
    unsigned int count = metadata[70], size = 71, i, j, words, word, shift;
    unsigned char *raw = (unsigned char *)output + CONTROL_OUTPUT_MD_REG3_OFFSET;
    unsigned char *ext = (unsigned char *)output + CONTROL_OUTPUT_EXT_MD_OFFSET;
    unsigned char *destination;
    for (i = 0; i < count; ++i)
        size += 5 + extension_length(metadata + 71 + i * 261);
    /* AArch64 signed divide-by-four lowering rounds toward zero. */
    words = (unsigned int)((int)(size + 2) / 4 + 1);
    write32(raw, 2048, words);
    if (words > 512) {
        dovi_control_services.logger("error!! core3 md overflow %d!!\n", words);
        write32(raw, 2048, 512);
        return;
    }
    write32(raw, 0, (size << 8) | metadata[0]);
    destination = raw + 4;
    word = metadata[1];
    shift = 8;
    for (j = 2; j < 71; ++j)
        pack_byte(&destination, &word, &shift, metadata[j]);
    for (i = 0; i < (unsigned int)metadata[70]; ++i) {
        const unsigned char *record = metadata + 71 + i * 261;
        unsigned int length = extension_length(record), limit = 0;
        unsigned int offset = 0, mask = 0;
        for (j = 0; j < 5; ++j)
            pack_byte(&destination, &word, &shift, record[j]);
        switch (record[4]) {
        case 1:
            limit = 6;
            offset = 4;
            mask = 1;
            break;
        case 2:
            limit = 14;
            offset = 10;
            mask = 2;
            break;
        case 4:
            limit = 4;
            offset = 24;
            mask = 4;
            break;
        case 5:
            limit = 8;
            offset = 28;
            mask = 8;
            break;
        case 6:
            limit = 8;
            offset = 36;
            mask = 16;
            break;
        case 255:
            limit = 6;
            offset = 44;
            mask = 0x80000000u;
            break;
        default:
            break;
        }
        if (mask != 0) {
            write32(ext, 0, read32(ext, 0) | mask);
            if ((int)length < (int)limit)
                limit = length;
            copy_bytes(ext + offset, record + 5, limit);
        }
        if ((int)length > 0)
            for (j = 0; j < length; ++j)
                pack_byte(&destination, &word, &shift, record[5 + j]);
    }
    if (shift != 0)
        write32(destination, 0, word);
}
