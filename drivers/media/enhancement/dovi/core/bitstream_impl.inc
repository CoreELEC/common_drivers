/* MSB-first bitstream reads with two-byte cache refills. */

typedef char uint_is_32_bits[(sizeof(unsigned int) == 4) ? 1 : -1];
typedef char int_is_32_bits[(sizeof(int) == 4) ? 1 : -1];
typedef char pointer_is_64_bits[(sizeof(void *) == 8) ? 1 : -1];
typedef char bitstream_size_is_32[(sizeof(struct dovi_bitstream) == 32) ? 1 : -1];

static int positive32(unsigned int value)
{
    return value != 0 && value < 0x80000000u;
}

static int signed32(unsigned int value)
{
    return (value & 0x80000000u) ? -1 - (int)~value : (int)value;
}

static unsigned int asr1(unsigned int value)
{
    return (value >> 1) | (value & 0x80000000u);
}

static unsigned int read_be16(struct dovi_bitstream *state)
{
    const unsigned char *cursor = state->next;
    /* Compare numeric addresses without assuming both pointers share a C array. */
    if ((unsigned long long)cursor > (unsigned long long)state->tail + 4u)
        cursor = state->tail;
    state->next = cursor + 2;
    return ((unsigned int)cursor[0] << 8) | cursor[1];
}

static void refill(struct dovi_bitstream *state)
{
    unsigned int chunk = read_be16(state);
    state->cache |= chunk << (state->deficit & 31u);
    state->deficit -= 16u;
}

/* Consume at most 16 bits, retaining the ARM64 variable-shift masking. */
static unsigned int take_small(unsigned int width, struct dovi_bitstream *state)
{
    unsigned int value;
    if (positive32(state->deficit))
        refill(state);
    value = state->cache >> ((0u - width) & 31u);
    state->cache <<= width & 31u;
    state->deficit += width;
    return value;
}

void dovi_bitstream_init(struct dovi_bitstream *state, const unsigned char *input,
                         unsigned int size)
{
    unsigned int last, trailing;
    state->cache = ((unsigned int)input[0] << 24) | ((unsigned int)input[1] << 16) |
                   ((unsigned int)input[2] << 8) | input[3];
    state->deficit = 0xfffffff0u;
    state->next = input + 4;
    state->tail = input + size - 1u;
    state->tail_bits = 8;
    state->bits_read = 0;
    last = input[size - 1u];
    if (last == 0) {
        state->tail = input + size - 2u;
        return;
    }
    for (trailing = 0; (last & (1u << trailing)) == 0; ++trailing) {
    }
    state->tail_bits = 7u - trailing;
}

unsigned int dovi_read_u_1(struct dovi_bitstream *state)
{
    unsigned int value = take_small(1, state);
    state->bits_read += 1u;
    return value;
}

unsigned int dovi_read_u_v(unsigned int width, struct dovi_bitstream *state)
{
    unsigned int value;
    if (width <= 16u) {
        value = take_small(width, state);
    } else {
        value = take_small(16, state);
        value = (value << ((width - 16u) & 31u)) | take_small(width - 16u, state);
    }
    state->bits_read += width;
    return value;
}

int dovi_read_i_v(unsigned int width, struct dovi_bitstream *state)
{
    unsigned int value = dovi_read_u_v(width, state);
    value |= 0u - (value & (1u << ((width - 1u) & 31u)));
    return signed32(value);
}

/* Refill while locating the separator, with a 31-zero limit. */
static unsigned int leading_zeros(struct dovi_bitstream *state)
{
    unsigned int zeros = 0;
    if (positive32(state->deficit))
        refill(state);
    while ((state->cache & 0x80000000u) == 0 && zeros < 31u) {
        unsigned int old_deficit = state->deficit;
        state->cache <<= 1;
        state->deficit += 1u;
        if (old_deficit < 0x7fffffffu)
            refill(state);
        ++zeros;
    }
    return zeros;
}

unsigned int dovi_read_ue_v(struct dovi_bitstream *state)
{
    unsigned int zeros = leading_zeros(state), suffix;
    if (zeros == 0) {
        state->cache <<= 1;
        state->deficit += 1u;
        suffix = 0;
    } else if (zeros <= 16u) {
        unsigned int remaining = state->cache << 1;
        suffix = remaining >> ((0u - zeros) & 31u);
        state->cache = remaining << zeros;
        state->deficit += zeros + 1u;
    } else {
        unsigned int high = (state->cache << 1) >> 16;
        state->cache <<= 17;
        state->deficit += 17u;
        if (positive32(state->deficit))
            refill(state);
        suffix = (high << (zeros - 16u)) | (state->cache >> (48u - zeros));
        state->cache <<= zeros - 16u;
        state->deficit += zeros - 16u;
    }
    state->bits_read += zeros * 2u + 1u;
    return suffix + ((1u << zeros) - 1u);
}

int dovi_read_se_v(struct dovi_bitstream *state)
{
    unsigned int zeros = leading_zeros(state), value = 0;
    unsigned int old_deficit = state->deficit;
    state->cache <<= 1;
    state->deficit += 1u;
    /* Unlike read_ue_v, signed decoding prefetches after the separator. */
    if (old_deficit < 0x7fffffffu)
        refill(state);
    if (zeros != 0) {
        unsigned int suffix;
        if (zeros <= 16u) {
            suffix = state->cache >> ((0u - zeros) & 31u);
            state->cache <<= zeros;
            state->deficit += zeros;
        } else {
            unsigned int high = state->cache >> 16;
            state->cache <<= 16;
            state->deficit += 16u;
            if (positive32(state->deficit))
                refill(state);
            suffix = (high << (zeros - 16u)) | (state->cache >> (48u - zeros));
            state->cache <<= zeros - 16u;
            state->deficit += zeros - 16u;
        }
        value = suffix + (1u << zeros);
        value = ((value - 1u) & 1u) ? asr1(value) : 0u - asr1(value - 1u);
    }
    state->bits_read += zeros * 2u + 1u;
    return signed32(value);
}
