/* Memory requirements and pointer assignment for the control arena. */
#ifndef DOVI_CONTROL_MEMORY_H
#define DOVI_CONTROL_MEMORY_H

#include "byte_utils.h"

struct dovi_control_os_services {
    void *(*memset)(void *destination, int value, unsigned long long count);
    int (*printk)(const char *format, ...);
    void (*dump_stack)(void);
};
/* Supplied by the embedding OS adapter; invoked services must be valid. */
extern struct dovi_control_os_services dovi_control_os;

/* Shared mutable display indices and immutable display configurations. */
extern unsigned char dovi_DisplayIdxTbl[400];
extern const unsigned char dovi_DisplayTbl[140];

typedef int (*control_memory_callback)(void *parameter, void *buffer);

/* Requests retain parameter and callback addresses without invoking them.
 * MemAssignCbFunFc stores the buffer pointer in an eight-byte field through
 * byte accesses; the destination field has no alignment requirement. */
void *dovi_InitMmg(void *manager);
int dovi_RegMemReq(unsigned int size, unsigned int alignment, unsigned int in_device,
                   void *parameter, control_memory_callback callback, void *manager);
int dovi_MemAssignCbFunFc(void *pointer_field, void *buffer);
void dovi_MemZeroByte(void *destination, unsigned long long count);
int dovi_UpdateCompactFrmMemRowPitch(void *frame);
unsigned int dovi_GetMinFrmMemSize(unsigned int height, unsigned int width,
                                   unsigned int color_space, unsigned int chroma_format,
                                   unsigned int weave, unsigned int data_type);
int dovi_SyncIoFrmFmtWithMds(const void *metadata, void *frame);
unsigned int dovi_GetRunMode(unsigned int index, const void *control);
unsigned int dovi_GetActiveVs10RunMode(const void *control);
void *dovi_GetHDmCfg(void *control);
void *dovi_GetHDmKs(unsigned int index, const void *control);
int dovi_UpdateKsInFrmMemRowPitch(unsigned int luma, unsigned int chroma, unsigned int index,
                                  void *control);
int dovi_InitCdmUtil(void);

unsigned long long dovi_init_cp_mmg(int instances);

#endif
