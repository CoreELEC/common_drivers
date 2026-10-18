#ifndef DOVI_RESAMPLING_FILTERS_H
#define DOVI_RESAMPLING_FILTERS_H

/*
 * The little-endian decoder embeds a bitstream at +0x40. Decoder+0 points to
 * {header, payload}; decoder+8 points to mutable configuration. Payload
 * partitions have stride 0x7038. Supply accessible, disjoint objects and
 * padded bitstream input. Coefficient counts and indices are unchecked.
 * Chroma decoding returns 0 or -2 for an invalid predicted partition.
 * Diagnostics use the callback at +0x315c8 and unsigned level at +0x315d0.
 */
int dovi_rpu_data_chroma_resampling_filter_1D_exp_read(void *decoder, int partition,
                                                       unsigned int raw_component,
                                                       unsigned int raw_direction);

#endif
