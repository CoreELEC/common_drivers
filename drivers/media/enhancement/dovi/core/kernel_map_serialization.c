/* Pack and unpack the kernel-map binary buffer. */
#include "kernel_map_serialization.h"
#include "control_abi_recovered.h"
#include "byte_utils.h"
#include "fixed_access.h"

static void clear_bytes(unsigned char *destination, unsigned int length)
{
    while (length-- != 0)
        *destination++ = 0;
}

unsigned int dovi_DumpKsBinBuf(const void *source, void *output)
{
    const unsigned char *kernel = source;
    unsigned char *stream = output, *tail;
    fixed_store_le16(stream, fixed_load_le16(kernel));
    dovi_MemCpyByte(stream + 2, kernel + DM_KERNEL_KSDMCTRL_OFFSET, 12);
    dovi_MemCpyByte(stream + 0x0e, kernel + DM_KERNEL_KSUS_OFFSET, 62);
    dovi_MemCpyByte(stream + 0x4c, kernel + DM_KERNEL_KSDS_OFFSET, 42);
    dovi_MemCpyByte(stream + 0x76, kernel + DM_KERNEL_KSFRMFMTI_OFFSET, 28);
    dovi_MemCpyByte(stream + 0x92, kernel + DM_KERNEL_KSIMAP_OFFSET, 0x468);
    dovi_MemCpyByte(stream + 0x4fa, kernel + DM_KERNEL_KSTMAP_OFFSET, 0x1008);
    tail = stream + 0x1502;
    if (fixed_load_le32(kernel + DM_KERNEL_KSDMCTRL_OFFSET + 4) != 0) {
        dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSUSG_OFFSET, 62);
        tail += 62;
        if (kernel[DM_KERNEL_KSIMAPG_OFFSET] != 2) {
            dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSFRMFMTA_OFFSET, 28);
            dovi_MemCpyByte(tail + 28, kernel + DM_KERNEL_RANGEINVA_OFFSET, 4);
            tail += 32;
        }
        dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSFRMFMTG_OFFSET, 28);
        dovi_MemCpyByte(tail + 28, kernel + DM_KERNEL_KSIMAPG_OFFSET, 0x468);
        dovi_MemCpyByte(tail + 0x484, kernel + DM_KERNEL_KSTMAPG_OFFSET, 0x1008);
        tail += 0x148c;
    }
    dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSOMAP_OFFSET, 112);
    dovi_MemCpyByte(tail + 112, kernel + DM_KERNEL_KSFRMFMTO_OFFSET, 28);
    return (unsigned int)(tail + 140 - stream);
}

unsigned int dovi_LoadKsBinBuf(void *output, const void *source)
{
    unsigned char *kernel = output;
    const unsigned char *stream = source, *tail;
    unsigned char geometry[4][20];
    static const unsigned int frames[4] = {
        DM_KERNEL_KSFRMFMTI_OFFSET, DM_KERNEL_KSFRMFMTG_OFFSET,
        DM_KERNEL_KSFRMFMTA_OFFSET, DM_KERNEL_KSFRMFMTO_OFFSET
    };
    unsigned int i;
    for (i = 0; i < 4; ++i) {
        dovi_MemCpyByte(geometry[i], kernel + frames[i], 8);
        dovi_MemCpyByte(geometry[i] + 8, kernel + frames[i] + 16, 12);
    }
    fixed_store_le16(kernel, 0);
    clear_bytes(kernel + DM_KERNEL_KSDMCTRL_OFFSET, 12);
    clear_bytes(kernel + DM_KERNEL_KSFRMFMTI_OFFSET, 28);
    clear_bytes(kernel + DM_KERNEL_KSUS_OFFSET, 62);
    clear_bytes(kernel + DM_KERNEL_KSIMAP_OFFSET, 0x468);
    clear_bytes(kernel + DM_KERNEL_KSTMAP_OFFSET, 0x1008);
    clear_bytes(kernel + DM_KERNEL_KSOMAP_OFFSET, 112);
    clear_bytes(kernel + DM_KERNEL_KSFRMFMTO_OFFSET, 28);
    fixed_store_le16(kernel, fixed_load_le16(stream));
    dovi_MemCpyByte(kernel + DM_KERNEL_KSDMCTRL_OFFSET, stream + 2, 12);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSUS_OFFSET, stream + 0x0e, 62);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSDS_OFFSET, stream + 0x4c, 42);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSFRMFMTI_OFFSET, stream + 0x76, 28);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSIMAP_OFFSET, stream + 0x92, 0x468);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSTMAP_OFFSET, stream + 0x4fa, 0x1008);
    tail = stream + 0x1502;
    if (fixed_load_le32(kernel + DM_KERNEL_KSDMCTRL_OFFSET + 4) != 0) {
        dovi_MemCpyByte(kernel + DM_KERNEL_KSUSG_OFFSET, tail, 62);
        tail += 62;
        if (kernel[DM_KERNEL_KSIMAPG_OFFSET] != 2) {
            dovi_MemCpyByte(kernel + DM_KERNEL_KSFRMFMTA_OFFSET, tail, 28);
            dovi_MemCpyByte(kernel + DM_KERNEL_RANGEINVA_OFFSET, tail + 28, 4);
            tail += 32;
        }
        dovi_MemCpyByte(kernel + DM_KERNEL_KSFRMFMTG_OFFSET, tail, 28);
        dovi_MemCpyByte(kernel + DM_KERNEL_KSIMAPG_OFFSET, tail + 28, 0x468);
        dovi_MemCpyByte(kernel + DM_KERNEL_KSTMAPG_OFFSET, tail + 0x484, 0x1008);
        tail += 0x148c;
    }
    dovi_MemCpyByte(kernel + DM_KERNEL_KSOMAP_OFFSET, tail, 112);
    dovi_MemCpyByte(kernel + DM_KERNEL_KSFRMFMTO_OFFSET, tail + 112, 28);
    for (i = 0; i < 4; ++i) {
        dovi_MemCpyByte(kernel + frames[i], geometry[i], 8);
        dovi_MemCpyByte(kernel + frames[i] + 16, geometry[i] + 8, 12);
    }
    /* Output dimensions follow the retained primary-input dimensions. */
    dovi_MemCpyByte(kernel + DM_KERNEL_KSFRMFMTO_OFFSET, geometry[0], 8);
    return (unsigned int)(tail + 140 - stream);
}

void dovi_SetHardwareInterface(const void *source, void *output)
{
    const unsigned char *kernel = source;
    unsigned char *registers = output, *tail = registers + 0x14ec;
    dovi_MemCpyByte(registers, kernel + DM_KERNEL_KSDMCTRL_OFFSET, 12);
    dovi_MemCpyByte(registers + 12, kernel + DM_KERNEL_KSUS_OFFSET, 62);
    dovi_MemCpyByte(registers + 0x4a, kernel + DM_KERNEL_KSDS_OFFSET, 42);
    dovi_MemCpyByte(registers + 0x74, kernel + DM_KERNEL_KSFRMFMTI_OFFSET, 28);
    dovi_MemCpyByte(registers + 0x7c, kernel + DM_KERNEL_KSIMAP_OFFSET, 0x468);
    dovi_MemCpyByte(registers + 0x4e4, kernel + DM_KERNEL_KSTMAP_OFFSET, 0x1008);
    if (fixed_load_le32(kernel + DM_KERNEL_KSDMCTRL_OFFSET + 4) != 0) {
        dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSUSG_OFFSET, 62);
        tail += 62;
        if (kernel[DM_KERNEL_KSIMAPG_OFFSET] != 2) {
            dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSFRMFMTA_OFFSET, 28);
            dovi_MemCpyByte(tail + 28, kernel + DM_KERNEL_RANGEINVA_OFFSET, 4);
            tail += 32;
        }
        dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSFRMFMTG_OFFSET, 28);
        dovi_MemCpyByte(tail + 28, kernel + DM_KERNEL_KSIMAPG_OFFSET, 0x468);
        dovi_MemCpyByte(tail + 0x484, kernel + DM_KERNEL_KSTMAPG_OFFSET, 0x1008);
        tail += 0x148c;
    }
    dovi_MemCpyByte(tail, kernel + DM_KERNEL_KSOMAP_OFFSET, 112);
    dovi_MemCpyByte(tail + 112, kernel + DM_KERNEL_KSFRMFMTO_OFFSET, 28);
}
