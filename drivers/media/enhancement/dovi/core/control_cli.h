/* Byte access preserves unaligned and overlapping CLI storage. */
#ifndef DOVI_CONTROL_CLI_H
#define DOVI_CONTROL_CLI_H
#include "fixed_access.h"
#include "control_cli_contract.h"

static inline unsigned int dovi_cli_read(const void *cli, unsigned int offset)
{
    return fixed_load_le32((const unsigned char *)cli + offset);
}

static inline void dovi_cli_write(void *cli, unsigned int offset, unsigned int value)
{
    fixed_store_le32((unsigned char *)cli + offset, value);
}

static inline unsigned int dovi_cli_priority(const void *cli)
{
    return dovi_cli_read(cli, DOVI_CLI_PRIORITY);
}

static inline void dovi_cli_set_priority(void *cli, unsigned int priority)
{
    dovi_cli_write(cli, DOVI_CLI_PRIORITY, priority);
}
#endif
