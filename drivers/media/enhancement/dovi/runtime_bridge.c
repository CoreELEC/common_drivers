/* Kernel services for control and metadata processing. */
#include <linux/kernel.h>
#include <linux/amlogic/cpu_version.h>

#include "runtime_bridge.h"
#include "core/control_runtime.h"

static int control_log(const char *format, unsigned int length)
{
    return printk(format, length);
}

static int memory_log(const char *format, const char *file, const char *function, unsigned int line,
                      const char *expression)
{
    return printk(format, file, function, line, expression);
}

static void trim_assertion(const char *file, const char *function, int line, const char *expression)
{
    pr_err("dovi reconstructed: %s:%d %s: %s\n", file, line, function, expression);
}

static int cpu_version(int level)
{
    return get_meson_cpu_version(level);
}

int dovi_runtime_configure(void)
{
    const struct control_runtime_services services = {
        .cpu_version = cpu_version,
        .logger = control_log,
        .memory = {.logger = memory_log, .dump_stack = dump_stack},
        .trim = {.assertion = trim_assertion, .dump_stack = dump_stack}};
    return control_runtime_init(&services);
}
