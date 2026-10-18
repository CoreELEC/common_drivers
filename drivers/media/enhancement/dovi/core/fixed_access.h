/* Fixed-width signed views and explicit unaligned little-endian accesses. */
#ifndef DOVI_FIXED_ACCESS_H
#define DOVI_FIXED_ACCESS_H
#include "fixed_bits.h"

/* Raw register views require 32-bit int and 64-bit long long scalar widths. */
typedef char fixed_require_scalar_widths[
    (sizeof(unsigned int) == 4 && sizeof(unsigned long long) == 8) ? 1 : -1];

static inline int fixed_signed32(unsigned int value)
{
    return value <= 0x7fffffffu ? (int)value : -1 - (int)~value;
}

static inline short fixed_signed16(unsigned int value)
{
    value &= 0xffffu;
    return (short)(value <= 0x7fffu ? (int)value : (int)value - 0x10000);
}

static inline long long fixed_signed64(unsigned long long value)
{
    return value <= 0x7fffffffffffffffull ? (long long)value : -1 - (long long)~value;
}

static inline unsigned int fixed_sign16(unsigned int value)
{
    return (value & 0x7fffu) - (value & 0x8000u);
}

static inline unsigned int fixed_load_le16(const void *pointer)
{
    const unsigned char *bytes = pointer;
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static inline unsigned int fixed_load_le32(const void *pointer)
{
    const unsigned char *bytes = pointer;
    return fixed_load_le16(bytes) | (fixed_load_le16(bytes + 2) << 16);
}

static inline unsigned long long fixed_load_le64(const void *pointer)
{
    const unsigned char *bytes = pointer;
    return fixed_load_le32(bytes) | ((unsigned long long)fixed_load_le32(bytes + 4) << 32);
}

static inline unsigned int fixed_load_be16(const void *pointer)
{
    const unsigned char *bytes = pointer;
    return ((unsigned int)bytes[0] << 8) | (unsigned int)bytes[1];
}

static inline unsigned int fixed_load_be32(const void *pointer)
{
    const unsigned char *bytes = pointer;
    return (fixed_load_be16(bytes) << 16) | fixed_load_be16(bytes + 2);
}

static inline void fixed_store_le16(void *pointer, unsigned int value)
{
    unsigned char *bytes = pointer;
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
}

static inline void fixed_store_le32(void *pointer, unsigned int value)
{
    unsigned char *bytes = pointer;
    fixed_store_le16(bytes, value);
    fixed_store_le16(bytes + 2, value >> 16);
}

static inline void fixed_store_le64(void *pointer, unsigned long long value)
{
    unsigned char *bytes = pointer;
    fixed_store_le32(bytes, (unsigned int)value);
    fixed_store_le32(bytes + 4, (unsigned int)(value >> 32));
}
#endif
