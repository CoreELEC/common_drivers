/* Control processing with native settings and persistent storage. */
#ifndef DOVI_CONTROL_PATH_H
#define DOVI_CONTROL_PATH_H

#include "control_state.h"
#include "control_metadata.h"
#include "control_setting_layout.h"

/* These are persistent objects supplied by their owner. Arena
 * covers 20040 bytes, cli 112, metadata1/2 66626 each, composition 1800,
 * register1 900, register2 96 and register3 1024. initialized is a byte.
 * Distinct writable objects and full readable input backing are required.
 * The function ignores physical composition size and consumes 1800-byte
 * records until its equality-based termination condition is reached.
 */
struct control_path_storage {
    void *arena;
    void *cli;
    void *metadata1;
    void *metadata2;
    void *composition;
    void *register1;
    void *register2;
    void *register3;
    unsigned char initialized;
    int input_format;
    int output_format;
};

/* output covers the 13468-byte prefix described by control_setting_layout.h.
 * host_abi.h checks that prefix against the host output. FORMAT_INVALID clears only
 * initialized. Every successful ordinary call destroys the context without
 * clearing initialized.
 */
int dovi_control_path_with_storage(struct control_path_storage *storage, int input_format,
                                   int output_format, const char *composition, int composition_size,
                                   const char *metadata, int metadata_size, int priority,
                                   int bit_depth, int chroma_format, int yuv_range,
                                   int graphics_min, int graphics_max, int target_min,
                                   int target_max, int no_el, const void *hdr10, void *output);

#endif
