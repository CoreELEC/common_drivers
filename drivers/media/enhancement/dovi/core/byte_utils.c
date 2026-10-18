/* Forward byte copying and equality checks for packed control data. */
#include "byte_utils.h"
#include "fixed_access.h"

typedef char byte_utils_int32[(sizeof(int) == 4) ? 1 : -1];

void dovi_MemCpyByte(void *destination, const void *source, int count)
{
    unsigned char *dst = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;
    int remaining = count;

    while (remaining > 0) {
        *dst++ = *src++;
        --remaining;
    }
}

int dovi_MemEqByte(const void *left, const void *right, int count)
{
    const unsigned char *left_bytes = (const unsigned char *)left;
    const unsigned char *right_bytes = (const unsigned char *)right;
    int remaining = count;

    while (remaining > 0) {
        unsigned char left_byte = *left_bytes++;
        unsigned char right_byte = *right_bytes++;
        --remaining;
        if (left_byte != right_byte)
            return 0;
    }
    return 1;
}
