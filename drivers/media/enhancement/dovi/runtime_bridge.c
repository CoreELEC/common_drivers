/* Kernel services for control and metadata processing. */
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "core/control_memory.h"
#include "core/module_parser.h"

static void *dovi_kernel_vmalloc(unsigned long long size)
{
    return vmalloc((unsigned long)size);
}

static void *dovi_kernel_memset(void *destination, int value, unsigned long long size)
{
    return memset(destination, value, (unsigned long)size);
}

static void dovi_kernel_vfree(void *pointer)
{
    vfree(pointer);
}

struct dovi_module_imports dovi_module_os = {
    .vmalloc = dovi_kernel_vmalloc,
    .vfree = dovi_kernel_vfree,
    .printk = _printk,
};

struct dovi_control_os_services dovi_control_os = {
    .memset = dovi_kernel_memset,
    .printk = _printk,
    .dump_stack = dump_stack,
};
