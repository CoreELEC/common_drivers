/* Singleton storage and services for control, register and DM processing. */
#ifndef CONTROL_RUNTIME_H
#define CONTROL_RUNTIME_H

#include "control_path.h"

struct control_trim_services {
    void (*assertion)(const char *source, const char *function, int line, const char *condition);
    void (*dump_stack)(void);
};

struct control_runtime_services {
    int (*cpu_version)(int level);
    control_metadata_logger logger;
    struct control_memory_ops memory;
    struct control_trim_services trim;
};

/* Configure once before the host publishes callbacks. Services are copied;
 * their function addresses must stay valid. The control core and
 * chroma scratch state are singletons: calls must not overlap or reenter.
 * This does not allocate frame buffers: dovi_init_cp records an opaque
 * memory request. This path produces hardware registers, not CPU image data.
 */
extern struct control_runtime_services dovi_control_services;

int control_runtime_init(const struct control_runtime_services *services);

/* Same 17 arguments as the control callback. Formats use the core enum;
 * host_control.h translates the host format enum explicitly. Buffer requirements are
 * those of control_path.h. Output is the 13468-byte prefix.
 * FORMAT_INVALID (-1) clears the initialized flag.
 */
int control_runtime_process(int input_format, int output_format, const char *composition,
                            int composition_size, const char *metadata, int metadata_size,
                            int priority, int bit_depth, int chroma_format, int yuv_range,
                            int graphics_min, int graphics_max, int target_min, int target_max,
                            int no_el, const void *hdr10, void *output);

#endif
