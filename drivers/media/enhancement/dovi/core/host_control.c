#include "host_control.h"
#include "control_runtime.h"

static int host_format_to_core(int format)
{
    if (format >= -1 && format <= 3)
        return format;
    if (format == 6)
        return 4;
    return -2;
}

int host_control_process(int input_format, int output_format, const char *composition,
                         int composition_size, const char *metadata, int metadata_size,
                         int priority, int bit_depth, int chroma_format, int yuv_range,
                         int graphics_min, int graphics_max, int target_min, int target_max,
                         int no_el, const void *hdr10, void *output)
{
    int core_input = host_format_to_core(input_format);
    int core_output = host_format_to_core(output_format);
    if (input_format != -1 && (core_input < 0 || core_output < 0))
        return -1;
    return control_runtime_process(core_input, core_output, composition, composition_size, metadata,
                                   metadata_size, priority, bit_depth, chroma_format, yuv_range,
                                   graphics_min, graphics_max, target_min, target_max, no_el, hdr10,
                                   output);
}
