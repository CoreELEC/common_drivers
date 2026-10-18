/* Kernel-state initialization and configuration commit. */
#include "dm_bindings.h"
#include "dm_layout.h"
#include "byte_utils.h"

/* The high trial bit is selected by the caller. All remaining candidates
 * fit16 bits, so their unsigned32-bit squares cannot overflow. */
static unsigned int complete_square_root(unsigned int root, unsigned int value)
{
    unsigned int bit;
    for (bit = 0x4000u; bit != 0; bit >>= 1) {
        unsigned int candidate = root | bit;
        if (candidate * candidate <= value)
            root = candidate;
    }
    return root;
}

unsigned short *dovi_dm_InitDmKs(unsigned long context_handle, const void *config,
                                 unsigned short *kernel_state)
{
    unsigned short *result;
    void *manager;

    result = (unsigned short *)0x0;
    if ((config != 0) && (kernel_state != (unsigned short *)0x0)) {
        manager = ((const struct dm_configuration *)config)->memory_manager;
        if (manager == (void *)0x0) {
            result = (unsigned short *)0x0;
        } else {
            *kernel_state = 0x2000;
            dovi_RegMemReq((unsigned int)(0xca80000), (unsigned int)(0x10),
                      (unsigned int)(
                          *(const int *)((const unsigned char *)config + 0x50) != 0),
                      kernel_state + 4, dovi_MemAssignCbFunFc, manager);
            *(unsigned long *)(kernel_state + 0x10) = context_handle;
            *(unsigned int *)(kernel_state + 0x2a0) = 0x1ff01ff;
            *(unsigned int *)(kernel_state + 0x6a2) = 0x1ff01ff;
            result = kernel_state;
        }
    }
    return result;
}

unsigned int dovi_dm_CheckMds(const void *metadata, const void *previous)
{
    if (dovi_MemEqByte(metadata, previous, DM_METADATA_COMPARE_BYTES))
        return MDS_CHG_NONE;
    return dovi_MemEqByte(metadata, previous, DM_METADATA_CONFIG_COMPARE_BYTES) ? MDS_CHG_TC
                                                                              : MDS_CHG_CFG;
}

unsigned long dovi_dm_CommitCfg(unsigned int signal_index, long config, unsigned char *context)
{
    long output_signal_address;
    int first_gd_weight;
    char kernel_mode;
    unsigned int signal_flags;
    unsigned char change_flags_or_depth;
    int gd_product_or_second_weight;
    int second_gd_product;
    unsigned long luminance_ratio;
    unsigned int gd_enable_or_range_inverse;
    unsigned int rate_or_luminance_ratio;
    unsigned int rate_scale_or_square_root;
    long signal_config_address;
    long frame_address_or_range;
    long kernel_state_address;

    if (4 < signal_index) {
        dm_limit_trap();
    }
    kernel_state_address = *(long *)(context + (long)(int)signal_index * 8 + 0x1f910);
    signal_flags = *(unsigned int *)(context + (long)(int)signal_index * 0x2c8 + 0x904);
    if (*(int *)(context + 4) != 0) {
        change_flags_or_depth = *context;
        goto commit_changed_configuration;
    }
    if (*(unsigned int *)(config + 4) == signal_index) {
        rate_or_luminance_ratio = *(unsigned int *)(config + (unsigned long)signal_index * 0x120 + 0x178);
        context[0x1f4a8] = 1;
        context[0x1f4a9] = 0;
        context[0x1f4aa] = 0;
        context[0x1f4ab] = 0;
        gd_enable_or_range_inverse = *(unsigned int *)(config + 0x8b8);
        context[0x1f4b0] = 1;
        context[0x1f4b1] = 0;
        context[0x1f4b2] = 0;
        context[0x1f4b3] = 0;
        context[0x1f4b4] = 0;
        context[0x1f4b5] = 0;
        context[0x1f4b6] = 0x90;
        context[0x1f4b7] = 1;
        *(unsigned int *)(context + 0x1f4ac) = gd_enable_or_range_inverse;
        if ((int)rate_or_luminance_ratio < 2) {
            rate_or_luminance_ratio = 1;
        }
        *(unsigned int *)(context + 0x1f4c0) = *(unsigned int *)(config + 0x164);
        rate_scale_or_square_root = 0;
        if (rate_or_luminance_ratio != 0) {
            rate_scale_or_square_root = 0x18000 / rate_or_luminance_ratio;
        }
        *(unsigned int *)(context + 0x1f4c4) = *(unsigned int *)(config + 0x8bc);
        gd_product_or_second_weight = *(int *)(config + 0x8d0) * rate_scale_or_square_root;
        first_gd_weight = gd_product_or_second_weight >> 0xc;
        *(int *)(context + 0x1f4c8) = first_gd_weight;
        if (0x1000fff < gd_product_or_second_weight) {
            first_gd_weight = 0x1000;
        }
        second_gd_product = *(int *)(config + 0x8d4) * rate_scale_or_square_root;
        gd_product_or_second_weight = second_gd_product >> 0xc;
        if (0x1000fff < second_gd_product) {
            gd_product_or_second_weight = 0x1000;
        }
        *(int *)(context + 0x1f4c8) = first_gd_weight;
        *(int *)(context + 0x1f4cc) = gd_product_or_second_weight;
        context[0x1f4e0] = 0xef;
        context[0x1f4e1] = 5;
        context[0x1f4e2] = 0;
        context[0x1f4e3] = 0;
        context[0x1f4e4] = 0x95;
        context[0x1f4e5] = 3;
        context[0x1f4e6] = 0;
        context[0x1f4e7] = 0;
        rate_or_luminance_ratio = *(unsigned int *)(config + 0x160);
        if (rate_or_luminance_ratio == 0) {
            rate_or_luminance_ratio = 0xfa000000;
            rate_scale_or_square_root = 0x8000;
            context[0x1f4e8] = 0;
            context[0x1f4e9] = 0;
            context[0x1f4ea] = 0;
            context[0x1f4eb] = 0xfa;
        } else {
            if (rate_or_luminance_ratio < 0x41) {
                rate_or_luminance_ratio = 0x40;
            }
            luminance_ratio = 0;
            if ((unsigned long)rate_or_luminance_ratio != 0) {
                luminance_ratio = ((unsigned long)*(unsigned int *)(config + 0x164) << 0x12) /
                        (unsigned long)rate_or_luminance_ratio;
            }
            if (luminance_ratio >> 0x14 < 0x19) {
                rate_or_luminance_ratio = 0x1900000;
                context[0x1f4e8] = 0;
                context[0x1f4e9] = 0;
                context[0x1f4ea] = 0x90;
                context[0x1f4eb] = 1;
            } else {
                rate_or_luminance_ratio = (unsigned int)luminance_ratio;
                if (0xfa000000 < luminance_ratio) {
                    rate_or_luminance_ratio = 0xfa000000;
                }
                *(unsigned int *)(context + 0x1f4e8) = rate_or_luminance_ratio;
                if (rate_or_luminance_ratio >> 0x1e != 0) {
                    rate_scale_or_square_root = 0x8000;
                    goto normalize_luminance_ratio;
                }
            }
            rate_scale_or_square_root = 0;
        }
    normalize_luminance_ratio:
        context[0x1f4dc] = 0;
        context[0x1f4dd] = 0;
        context[0x1f4de] = 0;
        context[0x1f4df] = 0;
        rate_scale_or_square_root = complete_square_root(rate_scale_or_square_root, rate_or_luminance_ratio);
        *(unsigned int *)(context + 0x1f4ec) = rate_scale_or_square_root << 9;
        *(unsigned int *)(context + 0x1f4fc) = *(unsigned int *)(config + 0x8c8);
        context[0x1f500] = 0;
        context[0x1f501] = 0;
        context[0x1f502] = 0;
        context[0x1f503] = 0;
        *(unsigned int *)(context + 0x1f504) = *(unsigned int *)(config + 0x8cc);
        if (*(int *)(config + 8) == 0) {
            context[0x1f4ac] = 0;
            context[0x1f4ad] = 0;
            context[0x1f4ae] = 0;
            context[0x1f4af] = 0;
        }
    }
    *(long *)(kernel_state_address + 0x10) = *(long *)(kernel_state_address + 8) + 0x4380000;
    *(long *)(kernel_state_address + 0x18) = *(long *)(kernel_state_address + 8) + 0x8700000;
    change_flags_or_depth = *context;
commit_changed_configuration:
    if ((change_flags_or_depth & 1) != 0) {
        output_signal_address = config + 100;
        frame_address_or_range = output_signal_address + (long)(int)signal_index * 0x120;
        dovi_dm_CommitCdm2KsFrmFmt(frame_address_or_range, signal_flags,
                                   (unsigned int *)(kernel_state_address + 0xbc));
        dovi_CommitKsUs((const void *)(frame_address_or_range), signal_flags, (void *)(kernel_state_address + 0x28));
        signal_config_address = config + (long)(int)signal_index * 0x120;
        if (*(int *)(signal_config_address + 0x68) != 0) {
            frame_address_or_range = config + (long)(int)signal_index * 0x34 + 0x604;
        }
        dovi_dm_CommitCdm2KsFrmFmt(frame_address_or_range, (unsigned int)(0),
                                   (unsigned int *)(kernel_state_address + 0x1548));
        if (*(int *)(signal_config_address + 0x7c) == 2) {
            *(unsigned char *)(kernel_state_address + 0xd8) = 1;
        }
        change_flags_or_depth = *(unsigned char *)(kernel_state_address + 0x1553);
        if (change_flags_or_depth == 0) {
            gd_enable_or_range_inverse = 0x10000;
        } else {
            frame_address_or_range = (long)(~(-1 << (unsigned long)(change_flags_or_depth & 0x1f))
                            << (unsigned long)((0x10 - change_flags_or_depth) & 0x1f));
            gd_enable_or_range_inverse = 0;
            if (frame_address_or_range != 0) {
                gd_enable_or_range_inverse = (unsigned int)(0xffffffff / frame_address_or_range);
            }
        }
        *(unsigned int *)(kernel_state_address + 0x1564) = gd_enable_or_range_inverse;
        kernel_mode = (*(int *)(config + 0xc) == 2) << 1;
        if (*(int *)(config + 0xc) == 1) {
            kernel_mode = '\x01';
        }
        *(char *)(kernel_state_address + 0xb4) = kernel_mode;
        *(unsigned int *)(kernel_state_address + 0xb8) = *(unsigned int *)(config + 0x60);
        *(long *)(kernel_state_address + 0x15f8) =
            *(long *)(context + (long)(*(int *)(config + 4) + -1) * 8 + 0x1f918) + 0x1568;
        dovi_CommitKsDs((const void *)(output_signal_address), (void *)(kernel_state_address + 0x66));
        dovi_dm_CommitCdm2KsFrmFmt(output_signal_address, (unsigned int)(0),
                                   (unsigned int *)(kernel_state_address + 0x15d8));
    }
    return 0;
}

void dovi_dm_CommitFinalized(int *config, long context)
{
    unsigned char primary_flag;
    _Bool first_signal_marked;
    int signal_index;
    int last_enabled_signal;
    int signal_count;
    long kernel_state_address;

    if ((config != (int *)0x0) && (context != 0)) {
        signal_count = *config;
        if (0 < signal_count) {
            last_enabled_signal = 0;
            first_signal_marked = 0;
            signal_index = 1;
            do {
                if (config[(long)signal_index + 0x238] != 0) {
                    kernel_state_address = *(long *)(context + (long)(signal_index + -1) * 8 + 0x1f918);
                    if (signal_count == 1) {
                        *(unsigned char *)(kernel_state_address + 0xb5) = 7;
                    } else {
                        primary_flag = (signal_index == config[1]) << 1;
                        *(unsigned char *)(kernel_state_address + 0xb5) = primary_flag;
                        last_enabled_signal = signal_index;
                        if (first_signal_marked) {
                            first_signal_marked = 1;
                        } else {
                            first_signal_marked = 1;
                            *(unsigned char *)(kernel_state_address + 0xb5) = primary_flag | 1;
                        }
                    }
                }
                signal_count = *config;
                signal_index = signal_index + 1;
            } while (signal_index <= signal_count);
            if (last_enabled_signal != 0) {
                kernel_state_address = *(long *)(context + (long)(last_enabled_signal + -1) * 8 + 0x1f918);
                *(unsigned char *)(kernel_state_address + 0xb5) = *(unsigned char *)(kernel_state_address + 0xb5) | 4;
            }
        }
        *(unsigned int *)(context + 4) = 1;
        dovi_MemCpyByte((void *)(context + 8), (const void *)(config), (int)(0x8f8));
        *(unsigned int *)(context + 0x43b8) = *(unsigned int *)(context + 0x43bc);
        if (config[2] == 0) {
            *(unsigned int *)(context + 0x1f4ac) = 0;
        }
    }
    return;
}
