#ifndef DOVI_DM_SERIALIZATION_H
#define DOVI_DM_SERIALIZATION_H

/*
 * Serialize the 0x10744-byte display-metadata payload into a big-endian base
 * and extension sequence. *length is signed input capacity and becomes the
 * written length only on success. Failure is -12 and retains partial writes.
 * Input, output and length are stable, accessible and disjoint. Unknown
 * extension lengths are unchecked and can require backing beyond the 256-byte
 * record payload. Dynamic levels can write before a later capacity check.
 */
int dovi_dm_rpu_payload_2_byte_sequence(const void *payload, void *bytes, int *length);

/*
 * Native records have count at +0x4a and stride 0x108; packed records have
 * count at +0x46 and stride 0x105. Both objects need complete, disjoint backing.
 * Payload-to-struct conversion writes unknown lengths and payloads at the
 * current output index without writing the level or advancing that index.
 * Its level-10 lengths are 5/21 although the payload writes cover 6/22 bytes.
 * Unknown lengths are unchecked. These converters are not lossless inverses
 * for arbitrary records.
 */
void dovi_dm_rpu_payload_2_struct(const void *payload, void *packed);
void dovi_dm_struct_2_rpu_payload(void *payload, const void *packed);
#endif
