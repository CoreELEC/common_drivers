/* Shared LMS/IPT coefficients and forward element-wise color copies. */
#ifndef DOVI_COLOR_BASIS_IMPL_H
#define DOVI_COLOR_BASIS_IMPL_H
#include "fixed_access.h"

static const short color_lms2_ipt[9] = {
    1638, 1638, 819, 18248, -19870, 1622, 3300, 1463, -4763
};
static const short color_ipt2_lms[9] = {
    32767, 3196, 6725, 32767, -3731, 4365, 32767, 1069, -22180
};

static inline void color_assign_vector(const void *source, void *destination)
{
    const unsigned char *s = source;
    unsigned char *d = destination;
    unsigned int i;
    for (i = 0; i < 3; ++i)
        fixed_store_le32(d + i * 4u, fixed_load_le32(s + i * 4u));
}

static inline void color_assign_matrix16(const void *source, void *destination)
{
    const unsigned char *s = source;
    unsigned char *d = destination;
    unsigned int i;
    for (i = 0; i < 9; ++i)
        fixed_store_le16(d + i * 2u, fixed_load_le16(s + i * 2u));
}

static inline void color_assign_matrix32_to16(const void *source, void *destination)
{
    const unsigned char *s = source;
    unsigned char *d = destination;
    unsigned int i;
    for (i = 0; i < 9; ++i)
        fixed_store_le16(d + i * 2u, fixed_load_le32(s + i * 4u));
}

static inline void color_rgb_to_lms_white(const void *matrix, void *destination)
{
    const unsigned char *s = matrix;
    unsigned char *d = destination;
    unsigned int row;
    for (row = 0; row < 3; ++row) {
        unsigned int a = fixed_sign16(fixed_load_le16(s + row * 6u));
        unsigned int b = fixed_sign16(fixed_load_le16(s + row * 6u + 2u));
        unsigned int c = fixed_sign16(fixed_load_le16(s + row * 6u + 4u));
        fixed_store_le32(d + row * 4u, a + b + c);
    }
}

#endif
