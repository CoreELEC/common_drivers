/* Bitstream state shared by the RPU and metadata readers. */
#ifndef DOVI_BITSTREAM_H
#define DOVI_BITSTREAM_H

struct dovi_bitstream {
    unsigned int cache;        /* +0: next input bits, MSB first */
    unsigned int deficit;      /* +4: signed 32-bit refill position */
    const unsigned char *next; /* +8: next two-byte refill */
    const unsigned char *tail; /* +16: final byte / adjusted stop position */
    unsigned int tail_bits;    /* +24: bits preceding final stop bit */
    unsigned int bits_read;    /* +28: wrapping counter */
};

/*
 * These functions use an unchecked reader with two-byte cache refills. Supply a writable 32-byte state and a readable, padded input buffer;
 * at least four initial bytes and every two-byte refill must be accessible.
 * The original is not a bounds-checked reader. size is the nominal byte length
 * and must be >= 2 here. The unsigned fixed-width reader accepts byte-sized
 * widths 0..255, as passed by the header parser; outside 1..32 its
 * masked-shift behavior is preserved, not a conventional wide-integer read.
 * The signed fixed-width reader expects widths 1..32. Exp-Golomb
 * decoding caps the leading-zero count at 31, including when no separator
 * is encountered.
 */
void dovi_bitstream_init(struct dovi_bitstream *state, const unsigned char *input,
                         unsigned int size);
unsigned int dovi_read_u_1(struct dovi_bitstream *state);
unsigned int dovi_read_u_v(unsigned int width, struct dovi_bitstream *state);
int dovi_read_i_v(unsigned int width, struct dovi_bitstream *state);
unsigned int dovi_read_ue_v(struct dovi_bitstream *state);
int dovi_read_se_v(struct dovi_bitstream *state);

#endif
