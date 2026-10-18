/* Fixed-point color bases and forward element-wise copies. */
#include "color_basis.h"
#include "color_basis_impl.h"

void dovi_AssignV3I2I(const void *source, void *destination)
{
    color_assign_vector(source, destination);
}

void dovi_AssignM33I322I(const void *source, void *destination)
{
    color_assign_matrix32_to16(source, destination);
}

void dovi_AssignM33I2I(const void *source, void *destination)
{
    color_assign_matrix16(source, destination);
}

void dovi_GetRgb2LmsWpV3(const void *matrix, void *destination)
{
    color_rgb_to_lms_white(matrix, destination);
}

unsigned int dovi_GetIpt2LmsM33Fxp(void *output)
{
    color_assign_matrix16(color_ipt2_lms, output);
    return 15;
}

unsigned int dovi_GetLms2IptM33Fxp(void *output)
{
    color_assign_matrix16(color_lms2_ipt, output);
    return 12;
}

void dovi_GetCtM33(unsigned int value, void *destination, unsigned int scale)
{
    unsigned char *d = destination;
    unsigned int diagonal = (1u << (scale & 31)) - 2u * value;
    fixed_store_le32(d + 8, value);
    fixed_store_le32(d + 12, value);
    fixed_store_le32(d + 24, value);
    fixed_store_le32(d + 28, value);
    fixed_store_le32(d, diagonal);
    fixed_store_le32(d + 4, value);
    fixed_store_le32(d + 16, diagonal);
    fixed_store_le32(d + 20, value);
    fixed_store_le32(d + 32, diagonal);
}

void dovi_GetInvCtM33(unsigned int value, void *destination, unsigned int scale)
{
    unsigned char *d = destination;
    unsigned int base = 1u << (scale & 31), raw_denominator = base - 3u * value;
    long long denominator = (raw_denominator & 0x80000000u)
                                ? -(long long)(0x100000000ull - raw_denominator)
                                : (long long)raw_denominator;
    /* The even masked shift is at most 62: the numerator is always positive.
     * AArch64 SDIV returns zero for a zero divisor. Multiplication then wraps. */
    long long numerator = (long long)(1ull << ((2u * scale) & 63));
    unsigned int quotient = denominator ? (unsigned int)(numerator / denominator) : 0;
    unsigned int diagonal = fixed_asr32((base - value) * quotient, scale);
    unsigned int off = 0u - fixed_asr32(quotient * value, scale);
    fixed_store_le32(d + 32, diagonal);
    fixed_store_le32(d, diagonal);
    fixed_store_le32(d + 4, off);
    fixed_store_le32(d + 8, off);
    fixed_store_le32(d + 12, off);
    fixed_store_le32(d + 16, diagonal);
    fixed_store_le32(d + 20, off);
    fixed_store_le32(d + 24, off);
    fixed_store_le32(d + 28, off);
}

unsigned int dovi_GetXyz2LmsM33(void *destination)
{
    unsigned char *d = destination;
    fixed_store_le64(d, 0x0b524d3306676031ull);
    fixed_store_le64(d + 8, 0xfc61153efeb50535ull);
    fixed_store_le16(d + 20, 0x30bcu);
    fixed_store_le32(d + 16, 0x12a521f6u);
    d[22] = 0xbb;
    fixed_store_le64(d + 23, 0);
    fixed_store_le32(d + 32, 0x0eb10ca5u);
    d[31] = 0;
    return 28;
}

static void matrix16(void *destination, unsigned long long a, unsigned long long b, unsigned int c)
{
    unsigned char *d = destination;
    fixed_store_le64(d, a);
    fixed_store_le64(d + 8, b);
    fixed_store_le16(d + 16, c);
}

unsigned int dovi_GetLms2IctcpIoM33(void *d)
{
    matrix16(d, 0x19d2000008000800ull, 0xbc12460d1b5bcad3ull, 0xfde1);
    return 12;
}

unsigned int dovi_GetIctcpIo2LmsM33(void *d)
{
    matrix16(d, 0x7fff0e36011a7fffull, 0x47af7ffff1cafee6ull, 0xd6f6);
    return 15;
}

unsigned int dovi_GetLms2IctcpDmM33(void *d)
{
    matrix16(d, 0x0ce9000008000800ull, 0xbc12460d0dade56aull, 0xfde1);
    return 12;
}

unsigned int dovi_GetIctcpDm2LmsM33(void *d)
{
    matrix16(d, 0x4000071b011a4000ull, 0x47af4000f8e5fee6ull, 0xeb7b);
    return 14;
}
