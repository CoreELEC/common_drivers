#ifndef DOVI_SUPPLEMENTAL_METADATA_H
#define DOVI_SUPPLEMENTAL_METADATA_H

/* decoder+0 points to {header, payload, DM metadata}; +0x40 embeds the reader.
 * All objects must be valid, disjoint and stable; input must be padded.
 * ST2094 returns zero when unrecognized, otherwise its original adjusted bit
 * count. Mastering returns the absolute wrapping reader bit counter.
 * Metadata counts remain bytes. Local loop counters are wide; record indices
 * and published counts are explicitly byte masked. The extension reader only
 * reads the selected index. Native unchecked backing requirements are retained.
 */
int dovi_st2094_10_data_read(void *decoder);
int dovi_mastering_display_colour_volume_read(void *decoder);

#endif
