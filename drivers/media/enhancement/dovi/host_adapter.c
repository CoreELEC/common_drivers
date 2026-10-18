/* Module lifecycle and host callback adapters. */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "host_abi.h"
#include "host_adapter.h"
#include "core/module_parser.h"
#include "core/shared_data.h"
#include "core/control_path.h"

static int registration_status = -EINPROGRESS;
module_param(registration_status, int, 0444);
MODULE_PARM_DESC(registration_status, "Host registration result; a failure requires reboot");

int dovi_host_control_path(struct m_dovi_setting_s *setting)
{
    return dovi_multi_control_path((int *)setting);
}

void *dovi_host_parser_init(int flags)
{
    (void)flags; /* STB 2.6 ignores this host argument. */
    return dovi_metadata_parser_init();
}

int dovi_host_parser_reset(void *context, int flags)
{
    (void)flags;
    return dovi_metadata_parser_reset(context);
}

int dovi_host_parser_process(void *context, char *rpu, int rpu_size, char *composer,
                             int *composer_size, char *metadata, int *metadata_size,
                             bool end_of_stream, enum dv_type_enum input_kind)
{
    return dovi_metadata_parser_process(context, (unsigned char *)rpu, (unsigned int)rpu_size,
                                        composer, (unsigned int *)composer_size, metadata,
                                        (unsigned int *)metadata_size, (unsigned int)end_of_stream,
                                        (int)input_kind);
}

/* Extend the embedded version strings before publishing the host callbacks. */
int dovi_source_init(void)
{
    char *sign = dovi_retained_section_2.version_signature;
    char *chip = dovi_retained_section_2.chip_name;
    unsigned long sign_length = strlen(sign), chip_length = strlen(chip);

    if (((unsigned int)chip_length + 3u) >> 2 < 5u) {
        if ((unsigned int)chip_length > 20u)
            __builtin_trap();
        snprintf(chip + (int)chip_length, 511ul - (long)(int)chip_length,
                 dovi_retained_section_3.diagnostics.string_format,
                 dovi_retained_section_3.diagnostics.default_chip_name);
    }
    if (((unsigned int)sign_length + 79u) >> 9 == 0) {
        if ((unsigned int)sign_length > 512u)
            __builtin_trap();
        snprintf(sign + (int)sign_length, 511ul - (long)(int)sign_length,
                 dovi_retained_section_3.diagnostics.version_signature_format, chip,
                 dovi_retained_section_3.version,
                 dovi_retained_section_3.diagnostics.build_description);
    }
    _printk(dovi_retained_section_3.diagnostics.module_init_message,
            dovi_retained_section_3.diagnostics.default_chip_name);
    /* The host version omits the AML_DV_INFO1: storage marker. */
    return dovi_register_callbacks(sign + sizeof("AML_DV_INFO1:") - 1);
}

int __init dovi_host_init(void)
{
    registration_status = dovi_source_init();
    if (registration_status)
        pr_err(
            "dovi: host registration failed (%d); keeping module resident because the host may retain callbacks; reboot required\n",
            registration_status);
    else
        pr_info("dovi: STB 2.6 C implementation registered\n");
    return 0;
}
