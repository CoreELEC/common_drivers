/* Scalar widths and fatal bounds checks for the LP64 processing interfaces. */
#ifndef DOVI_DM_TYPES_H
#define DOVI_DM_TYPES_H
typedef unsigned char byte;
typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef _Bool bool;
#define true 1
#define false 0
_Static_assert(sizeof(long) == 8 && sizeof(void *) == 8, "AArch64 LP64 required");
#define CONCAT44(a, b) (((unsigned long)(unsigned int)(a) << 32) | (unsigned int)(b))

/* Bounds and integrity failures do not return. */
static inline __attribute__((noreturn)) void dm_limit_trap(void)
{
    __builtin_trap();
}

static inline __attribute__((noreturn)) void dm_integrity_trap(void)
{
    __builtin_trap();
}
#endif
