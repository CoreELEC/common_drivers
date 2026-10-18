/* Module lifecycle and host callback adapters. */
#include <linux/module.h>
#include <linux/kernel.h>

#include "host_abi.h"
#include "host_adapter.h"
#include "runtime_bridge.h"
#include "core/module_parser.h"

static int metadata_log(const char *format, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, format);
    result = vprintk(format, arguments);
    va_end(arguments);
    return result;
}

void *dovi_host_parser_init(int flags)
{
    return dovi_metadata_parser_init(metadata_log, flags);
}

int dovi_host_parser_reset(int flags)
{
    return dovi_metadata_parser_reset(flags);
}

int dovi_host_parser_process(char *rpu, int rpu_size, char *composer, int *composer_size,
                             char *metadata, int *metadata_size, bool end_of_stream)
{
    return dovi_metadata_parser_process(rpu, rpu_size, composer, composer_size, metadata,
                                        metadata_size, (unsigned char)end_of_stream);
}

int dovi_host_control_path(enum signal_format_enum input_format,
                           enum signal_format_enum output_format, char *composition,
                           int composition_size, char *metadata, int metadata_size,
                           enum priority_mode_enum priority, int bit_depth, int chroma_format,
                           int yuv_range, int graphics_min, int graphics_max, int target_min,
                           int target_max, int no_el, struct hdr10_parameter *hdr10,
                           struct dovi_setting_s *output)
{
    return host_control_process((int)input_format, (int)output_format, composition,
                                composition_size, metadata, metadata_size, (int)priority, bit_depth,
                                chroma_format, yuv_range, graphics_min, graphics_max, target_min,
                                target_max, no_el, hdr10, output);
}

int __init dovi_host_init(void)
{
    int result = dovi_runtime_configure();
    if (result != 0)
        return result;
    return dovi_register_callbacks();
}
