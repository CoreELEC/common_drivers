/* Packed control metadata and HDR infoframe helpers. */
#ifndef DOVI_CONTROL_PACKETS_H
#define DOVI_CONTROL_PACKETS_H

/* Objects use control_abi_recovered.h offsets and must be distinct.
 * target has 88 bytes; hdr10 has 28. metadata is a packed 71-byte base followed by count
 * 261-byte extension records. infoframe has 29 writable bytes. Unrecognized
 * input formats change only the first five infoframe bytes. */
int dovi_get_info_frame(const void *state, const void *target, int input_format, const void *hdr10,
                        const void *metadata, void *infoframe);

/* Requires a valid ctx->h_dm configuration: control_get_stb_tmm failure is
 * ignored and would leave the copied local PQ words uninitialized.
 * Only bytes 0..70 of the packed metadata base are changed. */
void get_fixed_metadata(const void *ctx, void *dm);

/* Outputs are optional and are written in this order:
 * min_pq, min, max_pq, max. Null or invalid dm returns -1 without output writes.
 * Valid dm requires readable pDmCfgAct/hDmKs for requested luminance outputs. */
int control_get_stb_tmm(unsigned short *min_pq, unsigned int *min, unsigned short *max_pq,
                        unsigned int *max, const void *dm);

/* Both output-format records contain 16 bytes and are required. The selected
 * configuration must be readable: GetDmCfg failure is ignored. Unrecognized
 * values retain their fields. Output records may overlap. */
int dovi_get_output_format(const void *state, void *hdmi_format, void *core_format);
/* Return the byte count from the native kernel-state stream serializer. */
unsigned int dovi_get_dm_kernel_buf(const void *state, void *output);

#endif
