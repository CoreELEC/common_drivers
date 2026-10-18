#ifndef DOVI_NUMERIC_HELPERS_H
#define DOVI_NUMERIC_HELPERS_H

static inline unsigned int kernel_shift_left(unsigned int value, unsigned int bits)
{
    return value << (bits & 31u);
}

static inline int kernel_divide_max(int denominator)
{
    return denominator ? (int)(4294967295LL / denominator) : 0;
}

/* Output slots use unaligned little-endian storage. */
void dovi_GetRange(unsigned int bits, unsigned int range, void *minimum, void *extent);
void dovi_GetCvmBypassNormParam(unsigned int raw_bits, unsigned int range, void *offset,
                                void *scale);
void dovi_CommitKsUs(unsigned int chroma, const void *bits, void *output);

#endif
