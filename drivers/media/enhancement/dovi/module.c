/* Module entry; each host adapter owns registration and failure policy. */
#include <linux/init.h>
#include <linux/module.h>
#include "host_adapter.h"

static int __init dovi_init(void)
{
    return dovi_host_init();
}

module_init(dovi_init);
/* Both hosts retain callbacks; neither adapter supports module removal. */
MODULE_LICENSE(DOVI_MODULE_LICENSE);
MODULE_DESCRIPTION(DOVI_MODULE_DESCRIPTION);
MODULE_VERSION(DOVI_MODULE_VERSION);
