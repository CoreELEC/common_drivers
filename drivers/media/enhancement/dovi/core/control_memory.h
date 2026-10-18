/* Memory requirements and pointer assignment for the control arena. */
#ifndef DOVI_CONTROL_MEMORY_H
#define DOVI_CONTROL_MEMORY_H

#include "control_abi_recovered.h"
#include "dm_layout.h"

typedef int (*control_memory_callback)(void *parameter, void *buffer);

struct control_memory_ops {
    int (*logger)(const char *format, const char *file, const char *function, unsigned int line,
                  const char *expression);
    void (*dump_stack)(void);
};

/* Requests retain parameter and callback addresses without invoking them.
 * MemAssignCbFunFc stores the buffer pointer in an eight-byte field through
 * byte accesses; the destination field has no alignment requirement. */
void *dovi_InitMmg(void *manager);
unsigned long long dovi_init_cp_mmg(void);
int dovi_RegMemReq(unsigned int size, unsigned int alignment, unsigned int in_device,
                   void *parameter, control_memory_callback callback, void *manager);
int dovi_MemAssignCbFunFc(void *pointer_field, void *buffer);

unsigned int dovi_GetMinFrmMemSize(unsigned int height, unsigned int width,
                                   unsigned int color_space, unsigned int chroma_format,
                                   unsigned int weave, unsigned int data_type);
DmCfgFxp_t *dovi_GetHDmCfg(DmCtxtFxp_t *context);
DmKsFxp_t *dovi_GetHDmKs(DmCtxtFxp_t *context);

#endif
