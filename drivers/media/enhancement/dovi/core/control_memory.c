/* Memory requirements and pointer assignment for the control arena. */
#include "control_memory.h"
#include "control_runtime.h"
#include "dm_configuration.h"
#include "dm_frame_format.h"
#include "fixed_access.h"
typedef char require_widths[(sizeof(unsigned int) == 4 && sizeof(unsigned long long) == 8 &&
                             sizeof(void *) == 8)
                                ? 1
                                : -1];

void *dovi_InitMmg(void *manager)
{
    fixed_store_le32((unsigned char *)manager + DM_MMG_MDSEXTSIZE_OFFSET, DM_EXTENSIONS_BYTES);
    fixed_store_le32((unsigned char *)manager + DM_MMG_DMCTXTSIZE_OFFSET, DM_CONTEXT_BYTES);
    fixed_store_le32((unsigned char *)manager + DM_MMG_DMKSSIZE_OFFSET, DM_KERNEL_BYTES);
    return manager;
}

unsigned long long dovi_init_cp_mmg(void)
{
    unsigned char memory[DM_MMG_BYTES];
    unsigned int total;
    dovi_InitMmg(memory);
    total = fixed_load_le32((const unsigned char *)memory + DM_MMG_DMCTXTSIZE_OFFSET) +
            fixed_load_le32((const unsigned char *)memory + DM_MMG_MDSEXTSIZE_OFFSET) +
            fixed_load_le32((const unsigned char *)memory + DM_MMG_DMKSSIZE_OFFSET);
    return (unsigned long long)total + CONTROL_CONTEXT_BYTES;
}

int dovi_RegMemReq(unsigned int size, unsigned int alignment, unsigned int in_device,
                   void *parameter, control_memory_callback callback, void *manager)
{
    int count = (int)fixed_load_le32((const unsigned char *)manager + DM_MMG_REQNUM_OFFSET);
    unsigned char *record;
    if (count > 31) {
        dovi_control_services.memory.logger(
            "\001" "0### ASSERTION FAILED %s: %s: %d: %s\n",
            "/mnt/fileroot/yao.liu/androidq/hardware/amlogic/dolby_vision/dv_source_code/libs/dm/dlb_dm_process/src/CdmMmg.c",
            "RegMemReq", 38u, "hMmg->reqNum < MMG_REQ_ITEM_MAX");
        dovi_control_services.memory.dump_stack();
        count = (int)fixed_load_le32((const unsigned char *)manager + DM_MMG_REQNUM_OFFSET);
        if (count == 32)
            return -1;
    }
    if (alignment == 0)
        alignment = 4;
    /* This signed index is not bounds-checked. */
    record = (unsigned char *)manager + (long long)count * DM_MEMORY_REQUEST_BYTES;
    fixed_store_le32(record + DM_MEMORY_REQUEST_SIZE_OFFSET, size);
    fixed_store_le32(record + DM_MEMORY_REQUEST_ALIGN_OFFSET, alignment);
    fixed_store_le32(record + DM_MEMORY_REQUEST_INDEV_OFFSET, in_device);
    fixed_store_le64(record + DM_MEMORY_REQUEST_PCBPARAM_OFFSET, (unsigned long long)parameter);
    fixed_store_le64(record + DM_MEMORY_REQUEST_PCBFUN_OFFSET, (unsigned long long)callback);
    fixed_store_le64((unsigned char *)manager +
                         (long long)count * (long long)sizeof(unsigned long long) +
                         DM_MMG_MMGBUFS_OFFSET,
                     0);
    fixed_store_le32((unsigned char *)manager + DM_MMG_REQNUM_OFFSET, (unsigned int)count + 1u);
    return 0;
}

int dovi_MemAssignCbFunFc(void *pointer_field, void *buffer)
{
    fixed_store_le64((unsigned char *)pointer_field, (unsigned long long)buffer);
    return 0;
}

unsigned int dovi_GetMinFrmMemSize(unsigned int height, unsigned int width,
                                   unsigned int color_space, unsigned int chroma_format,
                                   unsigned int weave, unsigned int data_type)
{
    unsigned int sample_bytes = dm_frame_sample_bytes(data_type), luma_size, chroma_size;
    if (weave != 0)
        sample_bytes *= dm_frame_interleaved_components(color_space, chroma_format);
    luma_size = width * height * sample_bytes;
    /* The signed division rounds toward zero after 32-bit wrapping. */
    chroma_size = (unsigned int)(fixed_signed32(luma_size) / 4);
    if (chroma_format != 0)
        chroma_size = luma_size;
    if (weave == 0)
        luma_size = color_space == DM_FRAME_COLOR_RGBA ? (chroma_size + luma_size) * 2u
                                                     : luma_size + chroma_size * 2u;
    return luma_size;
}

DmCfgFxp_t *dovi_GetHDmCfg(DmCtxtFxp_t *context)
{
    return dm_selected_configuration(context);
}

DmKsFxp_t *dovi_GetHDmKs(DmCtxtFxp_t *context)
{
    return context->hDmKs;
}
