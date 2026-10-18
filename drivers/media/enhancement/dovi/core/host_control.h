/* Host signal-format adapter; structure layout is checked by the kernel glue. */
#ifndef HOST_CONTROL_H
#define HOST_CONTROL_H

/* The host's 13480-byte output includes a 13468-byte core prefix.
 * The three trailing words and the host-owned src/dst format fields are
 * preserved. HLG, HDR10PLUS, MVC, CUVA and unknown formats return -1 before
 * any core state/output is changed. Input -1 invokes the reset path
 * without dereferencing other pointers, including the host output.
 * Ordinary calls retain the buffer contracts in control_path.h.
 */
int host_control_process(int input_format, int output_format, const char *composition,
                         int composition_size, const char *metadata, int metadata_size,
                         int priority, int bit_depth, int chroma_format, int yuv_range,
                         int graphics_min, int graphics_max, int target_min, int target_max,
                         int no_el, const void *hdr10, void *output);

#endif
