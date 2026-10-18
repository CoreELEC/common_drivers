/* Persistent host callback registration. */
#include "host_abi.h"
#include "core/module_parser.h"

/* The host can inspect the two HW5 slots after duplicate registration and
 * retains this pointer even on some failures. Keep all 112 bytes resident. */
static struct dolby_vision_func_s host_callbacks = {
    .multi_control_path = dovi_host_control_path,
    .multi_mp_init = dovi_host_parser_init,
    .multi_mp_reset = dovi_host_parser_reset,
    .multi_mp_process = dovi_host_parser_process,
    .multi_mp_release = dovi_metadata_parser_release,
    .tv_hw5_control_path = NULL,
    .tv_hw5_control_path_analyzer = NULL,
};

int dovi_register_callbacks(const char *version_info)
{
    host_callbacks.version_info = version_info;
    return register_dv_functions(&host_callbacks);
}
