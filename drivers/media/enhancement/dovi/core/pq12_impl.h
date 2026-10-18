/* Shared 12-bit PQ interpolation. The caller chooses the upper endpoint. */
#ifndef DOVI_PQ12_IMPL_H
#define DOVI_PQ12_IMPL_H

static inline unsigned int pq12_node(unsigned int value, const unsigned int *positions)
{
    unsigned int index = 0, step, candidate;
    for (step = 64; step != 0; step >>= 1) {
        candidate = index + step;
        if (value >= positions[candidate]) {
            index = candidate;
            if (value == positions[candidate])
                break;
        }
    }
    return index;
}

static inline unsigned int linear_to_pq12(unsigned int value, const unsigned int *positions,
                                          const unsigned int *slopes,
                                          const unsigned int *intercepts, unsigned int maximum)
{
    unsigned int index = pq12_node(value, positions), result;
    unsigned long long product = (unsigned long long)(value - positions[index]) * slopes[index];
    result = (((intercepts[index] + (unsigned int)(product >> 25)) & 0xffffu) + 8u) >> 4;
    return result > maximum ? maximum : result;
}

static inline unsigned int pq12_to_linear(unsigned int value, const unsigned int *values)
{
    unsigned int index, result;
    value &= 65535u;
    if (value > 4095u)
        value = 4095u;
    index = value >> 2;
    result = values[index];
    if (index != 1023u)
        result +=
            (unsigned int)(((unsigned long long)(values[index + 1u] - result) * (value & 3u)) >> 2);
    return result;
}

static inline unsigned int pq16_to_linear(unsigned int value, const unsigned int *values)
{
    unsigned int index, result;
    value &= 65535u;
    index = value >> 6;
    result = values[index];
    if (index != 1023u)
        result +=
            (unsigned int)(((unsigned long long)(values[index + 1u] - result) * (value & 63u)) >>
                           6);
    return result;
}
#endif
