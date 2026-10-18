/* Persistent host callback registration. */
#include <linux/module.h>
#include <linux/kernel.h>
#include "host_abi.h"
#include "core/module_parser.h"

/* The host retains this table for the lifetime of the module registration. */
static const struct dolby_vision_func_s host_callbacks = {
    .version_info = "dovi reconstructed P3i T4c scalar core",
    .metadata_parser_init = dovi_host_parser_init,
    .metadata_parser_reset = dovi_host_parser_reset,
    .metadata_parser_process = dovi_host_parser_process,
    .metadata_parser_release = dovi_metadata_parser_release,
    .control_path = dovi_host_control_path};

int dovi_register_callbacks(void)
{
    return register_dv_functions(&host_callbacks);
}
