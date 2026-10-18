/* Control processing with native settings and persistent storage.
 * Pointer offsets describe the LP64 ABI; scratch aliases preserve overlapping
 * local structs.
 */
#include "control_orchestration_bindings.h"
#include "fixed_access.h"
#include "control_cli.h"
#include "control_setting_layout.h"
#include "control_publish.h"
#include "control_workspace.h"

int dovi_multi_control_path(int *setting)

{
    int *setting_words = setting;
    _Alignas(16) struct dovi_multi_workspace scratch;
    long metadata_read_offset;
    unsigned long *metadata_output;
    const unsigned char *metadata_input_bytes;
    unsigned long metadata_pair0;
    unsigned long metadata_pair1;
    _Bool multi_core1;
    int result;
    int first_input_valid;
    int initialization_status;
    int input_count;
    int source_width;
    int graphics_valid;
    int graphics_input_mode;
    int composition_offset;
    int metadata_size;
    unsigned int debug_flags;
    unsigned int valid_source_count;
    unsigned int video_count;
    unsigned int output_format;
    size_t control_context_size;
    int second_input_valid;
    int composition_size;
    int metadata_bytes_available;
    unsigned int primary_input_debug;
    unsigned int source_format;
    unsigned int *hdr10_metadata;
    int graphics_count;
    int metadata_input_offset;
    unsigned long metadata_pair2;
    unsigned int *hdr10_debug_words;
    long source_byte_offset;
    long hdr10_debug_address;
    long metadata_source_address;
    long vsem_packets_address;
    unsigned long metadata_slot_offset;
    unsigned int video_core_count;
    unsigned int completion_index;
    unsigned int metadata_available32;
    unsigned int extension_index;
    long input_record_offset;
    long input_index;
    long pending_input_index;
    int input_metadata_size;
    int extension_destination_offset;
    unsigned int metadata_size_written;
    unsigned int extension_header_word;
    unsigned long metadata_write_offset;
    int extension_length;
    unsigned long extension_copy_size;
    int *source_words;
    int *vsvdb_words;
    unsigned long source_index;
    unsigned char *graphics_lut;

    scratch.payloads[6] = 0;
    scratch.payloads[7] = 0;
    scratch.payloads[4] = 0;
    scratch.payloads[5] = 0;
    scratch.payloads[2] = 0;
    scratch.payloads[3] = 0;
    scratch.payloads[0] = 0;
    scratch.payloads[1] = 0;
    scratch.payload_sizes[0xc] = 0;
    scratch.payload_sizes[0xd] = 0;
    scratch.payload_sizes[0xe] = 0;
    scratch.payload_sizes[0xf] = 0;
    scratch.payload_sizes[8] = 0;
    scratch.payload_sizes[9] = 0;
    scratch.payload_sizes[10] = 0;
    scratch.payload_sizes[0xb] = 0;
    scratch.payload_sizes[4] = 0;
    scratch.payload_sizes[5] = 0;
    scratch.payload_sizes[6] = 0;
    scratch.payload_sizes[7] = 0;
    scratch.payload_sizes[0] = 0;
    scratch.payload_sizes[1] = 0;
    scratch.payload_sizes[2] = 0;
    scratch.payload_sizes[3] = 0;
    scratch.infoframe.fields.min_luminance_hi = 0;
    scratch.infoframe.fields.max_content_light_lo = 0;
    scratch.infoframe.fields.max_content_light_hi = 0;
    scratch.infoframe.fields.max_frame_average_lo = 0;
    scratch.infoframe.fields.max_frame_average_hi = 0;
    scratch.infoframe.fields.primary0_y_hi = 0;
    scratch.infoframe.fields.primary1_x = 0;
    scratch.infoframe.fields.primary1_y = 0;
    scratch.infoframe.fields.primary2_x = 0;
    scratch.infoframe.fields.primary2_y_lo = 0;
    scratch.infoframe.fields.primary2_y_hi = 0;
    scratch.infoframe.fields.white_x = 0;
    scratch.infoframe.fields.white_y = 0;
    scratch.infoframe.fields.max_luminance_lo = 0;
    scratch.infoframe.fields.max_luminance_hi = 0;
    scratch.infoframe.fields.min_luminance_lo = 0;
    (*(unsigned long *)scratch.infoframe.bytes) = 0;
    scratch.infoframe.fields.primary0_x = 0;
    scratch.infoframe.fields.primary0_y_lo = 0;
    scratch.target.words[28] = 0;
    scratch.target.pairs[12] = 0;
    scratch.target.pairs[13] = 0;
    scratch.target.pairs[10] = 0;
    scratch.target.pairs[11] = 0;
    scratch.target.pairs[8] = 0;
    scratch.target.pairs[9] = 0;
    scratch.target.pairs[6] = 0;
    scratch.target.pairs[7] = 0;
    scratch.target.pairs[4] = 0;
    scratch.target.pairs[5] = 0;
    scratch.target.pairs[2] = 0;
    scratch.target.pairs[3] = 0;
    scratch.target.words[0] = 0;
    scratch.target.words[1] = 0;
    scratch.target.pairs[1] = 0;
    scratch.initialization[0] = 0;
    scratch.input_configuration.words[2] = 0;
    scratch.input_configuration.pairs[0] = 0;
    if (setting_words == (int *)0x0) {
        result = 0;
        goto return_result;
    }
    if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_INPUT)] == 0) {
        if (enable_debug != 0) {
            _printk("reset cp\n");
        }
        memset(&reg1, 0, 0x790);
        memset(&reg2, 0, 0x158);
        memset(&reg3, 0, 0x400);
        memset(&dst_dm_lut, 0, 0x5000);
        result = 0;
        ctx_init = 0;
        src_content_type = 0;
        src_desired_white_point = 0;
        src_l11_byte2 = 0;
        src_l11_byte3 = 0;
        goto return_result;
    }
    video_count = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_VIDEO)];
    debug_flags = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_ENABLE_DEBUG)];
    graphics_count = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_INPUT)] - video_count;
    enable_debug = debug_flags;
    memset(&src_param, 0, 0x280);
    memset(&src_comp_metadata, 0, 0xe10);
    memset(&dm_metadata_buf, 0, 0x2000);
    memset(&cli_param, 0, 0x1d2c);
    first_input_valid = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE +
                                             DOVI_INPUT_VALID)];
    if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_ENABLE_MULTI_CORE1)] == 0) {
        if (first_input_valid == 0) {
            second_input_valid = setting_words[DOVI_SETTING_WORD(
                DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_VALID)];
            goto select_secondary_video;
        }
    select_primary_video_only:
        video_core_count = 1;
    configure_selected_inputs:
        dovi_cli_write(&cli_param, DOVI_CLI_INPUT_COUNT, graphics_count + video_core_count);
        dovi_cli_write(&cli_param, DOVI_CLI_PRIMARY_INPUT, 0);
        multi_core1 = 0;
        primary_input_debug = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_PRI_INPUT)];
        scratch.primary_core = video_core_count;
    } else {
        second_input_valid = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_VALID)];
        if (first_input_valid == 0) {
        select_secondary_video:
            video_core_count = (unsigned int)(second_input_valid != 0);
            goto configure_selected_inputs;
        }
        if (second_input_valid == 0)
            goto select_primary_video_only;
        dovi_cli_write(&cli_param, DOVI_CLI_INPUT_COUNT, graphics_count + 2);
        scratch.primary_core = 0;
        video_core_count = 2;
        dovi_cli_write(&cli_param, DOVI_CLI_PRIMARY_INPUT, setting_words[DOVI_SETTING_WORD(DOVI_SETTING_PRI_INPUT)]);
        multi_core1 = 1;
        primary_input_debug = dovi_cli_read(&cli_param, DOVI_CLI_PRIMARY_INPUT);
    }
    if ((debug_flags & 1) != 0) {
        _printk("valid %d-%d-%d, pri_input %d, ll %d\n", first_input_valid,
                setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE +
                                                DOVI_INPUT_VALID)],
                setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                                                DOVI_INPUT_VALID)],
                primary_input_debug, setting_words[DOVI_SETTING_WORD(DOVI_SETTING_USE_LL_FLAG)]);
    }
    dovi_cli_set_priority(&cli_param, setting_words[DOVI_SETTING_WORD(DOVI_SETTING_SET_PRIORITY)]);
    dovi_cli_write(&cli_param, DOVI_CLI_DOVI2HDR10_NOMAPPING, setting_words[DOVI_SETTING_WORD(DOVI_SETTING_DOVI2HDR10_NOMAPPING)]);
    dovi_cli_write(&cli_param, DOVI_CLI_INPUT_FLAGS_RESERVED, 0);
    dovi_cli_write(&cli_param, DOVI_CLI_INIT_FLAGS_RESERVED, 0);
    dovi_cli_write(&cli_param, DOVI_CLI_LEVEL11_ENABLE, setting_words[DOVI_SETTING_WORD(DOVI_SETTING_RESERVED)]);
    ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_CONTENT_TYPE] = (unsigned char)setting_words[DOVI_SETTING_WORD(DOVI_SETTING_RESERVED + 0x4)];
    ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_WHITE_POINT] = (unsigned char)setting_words[DOVI_SETTING_WORD(DOVI_SETTING_RESERVED + 0x8)];
    ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_BYTE2] = (unsigned char)setting_words[DOVI_SETTING_WORD(DOVI_SETTING_RESERVED + 0xc)];
    ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_BYTE3] =
        (unsigned char)setting_words[DOVI_SETTING_WORD(DOVI_SETTING_RESERVED + 0x10)];
    if ((ctx_init & 1) == 0) {
        control_context_size = dovi_init_cp_mmg(dovi_cli_read(&cli_param, DOVI_CLI_INPUT_COUNT));
        memset(&cp, 0, control_context_size);
        scratch.initialization[0] = dovi_cli_read(&cli_param, DOVI_CLI_INIT_FLAGS_RESERVED);
        initialization_status = dovi_init_cp(&cp, dovi_cli_read(&cli_param, DOVI_CLI_INPUT_COUNT), scratch.initialization);
        if (initialization_status != 0) {
            _printk(dovi_retained_section_3.diagnostics.control_init_failure);
            result = -1;
            goto return_result;
        }
        ctx_init = 1;
    }
    dovi_content_info_cb_handle = dovi_cb_content_info;
    if (dovi_cli_read(&cli_param, DOVI_CLI_DOVI2HDR10_NOMAPPING) != 0) {
        DOVI_CONTROL_U32_144C = 0;
    }
    if (dovi_cli_read(&cli_param, DOVI_CLI_LEVEL11_ENABLE) != 0) {
        dovi_set_L11_hdmi_tx(&cp, 2, ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_CONTENT_TYPE], ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_WHITE_POINT], ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_BYTE2],
                        ((unsigned char *)&cli_param)[DOVI_CLI_LEVEL11_BYTE3]);
    }
    DOVI_CONTROL_U16_0D20 = 0;
    fixed_store_le64((unsigned char *)dovi_retained_section_1.control_path_context + 0x1454, 0);
    input_count = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_INPUT)];
    (*(unsigned short *)((unsigned char *)&scratch.input_configuration.bytes + 0)) =
        CONCAT11((signed char)dovi_cli_read(&cli_param, DOVI_CLI_PRIMARY_INPUT), (signed char)dovi_cli_read(&cli_param, DOVI_CLI_INPUT_COUNT));
    scratch.input_configuration.pairs[0] =
        CONCAT44(dovi_cli_priority(&cli_param), *(unsigned int *)scratch.input_configuration.bytes);
    scratch.input_configuration.words[2] = dovi_cli_read(&cli_param, DOVI_CLI_INPUT_FLAGS_RESERVED);
    if (0 < input_count) {
        composition_size = setting_words[DOVI_SETTING_WORD(
            DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_IN_COMP_SIZE)];
        scratch.payloads[4] =
            *(long *)(setting_words +
                      DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE +
                                        DOVI_INPUT_IN_COMP));
        input_metadata_size = setting_words[DOVI_SETTING_WORD(
            DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_IN_MD_SIZE)];
        scratch.payload_sizes[4] = composition_size;
        scratch.payload_sizes[0] = input_metadata_size;
        if (composition_size < 1 || scratch.payloads[4] == 0) {
            scratch.payloads[4] = 0;
        }
        if ((*(long *)(setting_words +
                       DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE +
                                         DOVI_INPUT_IN_MD)) == 0) ||
            (input_metadata_size < 1)) {
            scratch.payloads[0] = 0;
        } else {
            scratch.payloads[0] =
                *(long *)(setting_words +
                          DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 0 * DOVI_SETTING_INPUT_STRIDE +
                                            DOVI_INPUT_IN_MD));
        }
        if (input_count != 1) {
            if ((setting_words[DOVI_SETTING_WORD(
                     DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_SRC_FORMAT)] ==
                 3) ||
                (setting_words[DOVI_SETTING_WORD(
                     DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_SRC_FORMAT)] ==
                 0)) {
                scratch.payloads[5] =
                    *(long *)(setting_words +
                              DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE +
                                                DOVI_INPUT_IN_COMP));
                scratch.payloads[1] =
                    *(long *)(setting_words +
                              DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE +
                                                DOVI_INPUT_IN_MD));
                scratch.payload_sizes[5] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_IN_COMP_SIZE)];
                scratch.payload_sizes[1] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 1 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_IN_MD_SIZE)];
                if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                    1 * DOVI_SETTING_INPUT_STRIDE +
                                                    DOVI_INPUT_IN_COMP_SIZE)] < 1 ||
                    scratch.payloads[5] == 0) {
                    scratch.payloads[5] = 0;
                }
                if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                    1 * DOVI_SETTING_INPUT_STRIDE +
                                                    DOVI_INPUT_IN_MD_SIZE)] < 1 ||
                    scratch.payloads[1] == 0) {
                    scratch.payloads[1] = 0;
                }
            }
            if (input_count != 2) {
                if ((setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                     2 * DOVI_SETTING_INPUT_STRIDE +
                                                     DOVI_INPUT_SRC_FORMAT)] == 3) ||
                    (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                     2 * DOVI_SETTING_INPUT_STRIDE +
                                                     DOVI_INPUT_SRC_FORMAT)] == 0)) {
                    scratch.payloads[6] =
                        *(long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                                    2 * DOVI_SETTING_INPUT_STRIDE +
                                                                    DOVI_INPUT_IN_COMP));
                    scratch.payloads[2] =
                        *(long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                                    2 * DOVI_SETTING_INPUT_STRIDE +
                                                                    DOVI_INPUT_IN_MD));
                    scratch.payload_sizes[6] = setting_words[DOVI_SETTING_WORD(
                        DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                        DOVI_INPUT_IN_COMP_SIZE)];
                    scratch.payload_sizes[2] = setting_words[DOVI_SETTING_WORD(
                        DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                        DOVI_INPUT_IN_MD_SIZE)];
                    if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                        2 * DOVI_SETTING_INPUT_STRIDE +
                                                        DOVI_INPUT_IN_COMP_SIZE)] < 1 ||
                        scratch.payloads[6] == 0) {
                        scratch.payloads[6] = 0;
                    }
                    if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                        2 * DOVI_SETTING_INPUT_STRIDE +
                                                        DOVI_INPUT_IN_MD_SIZE)] < 1 ||
                        scratch.payloads[2] == 0) {
                        scratch.payloads[2] = 0;
                    }
                }
                if (input_count != 3)
                    goto trap_invalid_input_count;
            }
        }
    }
    if ((int)video_count < 1) {
        valid_source_count = 0;
    } else {
        input_record_offset = 0;
        valid_source_count = 0;
        do {
            if (input_record_offset == 0x138)
                goto trap_invalid_input_count;
            if (*(int *)((long)setting_words + input_record_offset + 0x60) != 0) {
                source_format = *(unsigned int *)((long)setting_words + input_record_offset + 100);
                source_index = (unsigned long)valid_source_count;
                source_words = (int *)(&src_param + source_index * 0x28);
                if ((source_format < 9) && ((1 << (unsigned long)(source_format & 0x1f) & 0x116U) != 0)) {
                    if (4 < valid_source_count)
                        goto trap_invalid_input_count;
                    dovi_set_default_src_param(source_words, source_format);
                }
                if (3 < valid_source_count)
                    goto trap_invalid_input_count;
                (&DOVI_SOURCE_U32_0058)[source_index * 0x28] =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0x74);
                (&DOVI_SOURCE_U32_005C)[source_index * 0x28] =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0x78);
                (&DOVI_SOURCE_U32_0074)[source_index * 0x28] =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0x7c);
                (&DOVI_SOURCE_U32_000C)[source_index * 0x28] =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0xb4);
                (&DOVI_SOURCE_U32_0060)[source_index * 0x28] =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0x80);
                if ((source_format < 9) && ((1 << (unsigned long)(source_format & 0x1f) & 0x116U) != 0)) {
                    dovi_update_src_param(source_words, source_format);
                }
                source_index = (unsigned long)valid_source_count;
                *source_words = 0x780;
                *(unsigned int *)(&DOVI_SOURCE_U8_0004 + source_index * 0xa0) = 0x438;
                if (*(int *)((long)setting_words + input_record_offset + 100) == 1) {
                    hdr10_metadata = *(unsigned int **)((long)setting_words + input_record_offset + 0xc0);
                    if (((((((unsigned short)hdr10_metadata[2] != 0) &&
                            (*(short *)((long)hdr10_metadata + 10) != 0)) &&
                           ((short)hdr10_metadata[3] != 0)) &&
                          ((*(short *)((long)hdr10_metadata + 0xe) != 0 &&
                            ((short)hdr10_metadata[4] != 0)))) &&
                         ((*(short *)((long)hdr10_metadata + 0x12) != 0 &&
                           (((short)hdr10_metadata[5] != 0 &&
                             (*(short *)((long)hdr10_metadata + 0x16) != 0)))))) &&
                        (hdr10_metadata[1] != 0)) {
                        if (((unsigned char)enable_debug >> 4 & 1) != 0) {
                            source_byte_offset = source_index * 0xa0;
                            *(int *)(&DOVI_SOURCE_U8_0014 + source_byte_offset) =
                                SUB164(ZEXT216((unsigned short)hdr10_metadata[2]) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_0018 + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   10)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_001C + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0xc)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_0020 + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0xe)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_0024 + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0x10)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_0028 + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0x12)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_002C + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0x14)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            *(int *)(&DOVI_SOURCE_U8_0030 + source_byte_offset) =
                                SUB164(ZEXT216(*(unsigned short *)(*(long *)((long)setting_words +
                                                                             input_record_offset + 0xc0) +
                                                                   0x16)) *
                                           (unsigned __int128)0x4000000 * ZEXT816(0x14f8b588e3690),
                                       8);
                            hdr10_metadata =
                                *(unsigned int **)((long)setting_words + input_record_offset + 0xc0);
                        }
                        *(int *)(&DOVI_SOURCE_U8_0064 + source_index * 0xa0) =
                            SUB164(ZEXT416(*hdr10_metadata) * (unsigned __int128)0x40000 *
                                       ZEXT816(0x68db8bac710cc),
                                   8);
                        *(int *)(&DOVI_SOURCE_U8_0068 + source_index * 0xa0) =
                            SUB164(ZEXT416(*(unsigned int *)(*(long *)((long)setting_words +
                                                                       input_record_offset + 0xc0) +
                                                             4)) *
                                       (unsigned __int128)0x40000 * ZEXT816(0x68db8bac710cc),
                                   8);
                    }
                    if (((unsigned char)enable_debug >> 1 & 1) != 0) {
                        hdr10_debug_address = *(long *)((long)setting_words + input_record_offset + 0xc0);
                        _printk(
                            "HDR10 in primary %x %x %x %x %x %x %x %x\n",
                            *(unsigned short *)(hdr10_debug_address + 8), *(unsigned short *)(hdr10_debug_address + 10),
                            *(unsigned short *)(hdr10_debug_address + 0xc), *(unsigned short *)(hdr10_debug_address + 0xe),
                            *(unsigned short *)(hdr10_debug_address + 0x10), *(unsigned short *)(hdr10_debug_address + 0x12),
                            *(unsigned short *)(hdr10_debug_address + 0x14), *(unsigned short *)(hdr10_debug_address + 0x16));
                        hdr10_debug_words = *(unsigned int **)((long)setting_words + input_record_offset + 0xc0);
                        _printk("HDR10 in min %d, max %d\n", *hdr10_debug_words, hdr10_debug_words[1]);
                    }
                }
                *(unsigned int *)(&DOVI_SOURCE_U8_0010 + source_index * 0xa0) =
                    *(unsigned int *)((long)setting_words + input_record_offset + 0xb8);
                source_width = *(int *)((long)setting_words + input_record_offset + 0x6c);
                if ((source_width != 0) && (*(int *)((long)setting_words + input_record_offset + 0x70) != 0)) {
                    *source_words = source_width;
                    *(unsigned int *)(&DOVI_SOURCE_U8_0004 + source_index * 0xa0) =
                        *(unsigned int *)((long)setting_words + input_record_offset + 0x70);
                }
                valid_source_count = valid_source_count + 1;
            }
            input_record_offset = input_record_offset + 0x68;
        } while ((unsigned long)video_count * 0x68 - input_record_offset != 0);
    }
    if (valid_source_count != video_core_count) {
        _printk("please check video num %d %d\n", valid_source_count, video_core_count);
    }
    if (graphics_count < 1) {
        graphics_valid = 0;
    } else {
        if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                                            DOVI_INPUT_VALID)] == 0) {
            graphics_valid = 0;
        } else {
            graphics_input_mode = setting_words[DOVI_SETTING_WORD(
                DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_INPUT_MODE)];
            (&DOVI_SOURCE_U32_000C)[(unsigned long)video_core_count * 0x28] = graphics_input_mode;
            if (graphics_input_mode == 2) {
                source_index = (unsigned long)video_core_count;
                if (setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                    2 * DOVI_SETTING_INPUT_STRIDE +
                                                    DOVI_INPUT_SRC_FORMAT)] != 0) {
                    dovi_set_default_src_param(&src_param + (unsigned long)video_core_count * 0x28,
                                          setting_words[DOVI_SETTING_WORD(
                                              DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                                              DOVI_INPUT_SRC_FORMAT)]);
                }
                (&DOVI_SOURCE_U32_0058)[source_index * 0x28] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_SET_BIT_DEPTH)];
                (&DOVI_SOURCE_U32_005C)[source_index * 0x28] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE +
                    DOVI_INPUT_SET_CHROMA_FORMAT)];
                (&DOVI_SOURCE_U32_0074)[source_index * 0x28] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_SET_YUV_RANGE)];
                (&DOVI_SOURCE_U32_000C)[(unsigned long)video_core_count * 0x28] =
                    setting_words[DOVI_SETTING_WORD(DOVI_SETTING_INPUT +
                                                    2 * DOVI_SETTING_INPUT_STRIDE +
                                                    DOVI_INPUT_INPUT_MODE)];
                (&src_param)[(unsigned long)video_core_count * 0x28] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_VIDEO_WIDTH)];
                *(int *)(&DOVI_SOURCE_U8_0004 + source_index * 0xa0) = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_VIDEO_HEIGHT)];
                if ((0 < setting_words[DOVI_SETTING_WORD(DOVI_SETTING_SET_GRAPHIC_MIN_LUM)]) &&
                    (0 < setting_words[DOVI_SETTING_WORD(DOVI_SETTING_SET_GRAPHIC_MAX_LUM)])) {
                    *(int *)(&DOVI_SOURCE_U8_0064 + source_index * 0xa0) =
                        SUB164(ZEXT416((unsigned int)setting_words[DOVI_SETTING_WORD(
                                   DOVI_SETTING_SET_GRAPHIC_MIN_LUM)]) *
                                   (unsigned __int128)0x40000 * ZEXT816(0x68db8bac710cc),
                               8);
                    *(int *)(&DOVI_SOURCE_U8_0068 + source_index * 0xa0) =
                        (int)((unsigned long)((long)setting_words[DOVI_SETTING_WORD(
                                                  DOVI_SETTING_SET_GRAPHIC_MAX_LUM)]
                                              << 0x12) /
                              10000);
                }
                graphics_valid = 1;
                (&DOVI_SOURCE_U32_0060)[source_index * 0x28] = setting_words[DOVI_SETTING_WORD(
                    DOVI_SETTING_INPUT + 2 * DOVI_SETTING_INPUT_STRIDE + DOVI_INPUT_COLOR_FORMAT)];
            } else {
                graphics_valid = 1;
            }
        }
        if (graphics_count != 1) {
        trap_invalid_input_count:
            /* WARNING: Does not return */
            dm_limit_trap();
        }
    }
    if (graphics_valid != graphics_count) {
        _printk("please check grapihc num %d %d\n");
    }
    output_format = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_DST_FORMAT)];
    vsvdb_words = setting_words + DOVI_SETTING_WORD(DOVI_SETTING_VSVDB_TBL);
    if (output_format != 0) {
        vsvdb_words = (int *)0x0;
    }
    if ((output_format < 9) && ((1 << (unsigned long)(output_format & 0x1f) & 0x106U) != 0)) {
        dovi_set_default_target_param(scratch.target.words, output_format);
    }
    scratch.target.pairs[1] =
        fixed_load_le64((const unsigned char *)setting_words + DOVI_SETTING_USE_LL_FLAG);
    scratch.target.words[0] = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_VOUT_WIDTH)];
    scratch.target.pairs[4] = CONCAT44(1, (unsigned int)scratch.target.pairs[4]);
    if ((scratch.target.words[0] == 0) ||
        (scratch.target.words[1] = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_VOUT_HEIGHT)],
         scratch.target.words[1] == 0)) {
        scratch.target.words[1] = 0x438;
        scratch.target.words[0] = 0x780;
    }
    graphics_lut = &DOVI_DM_LUT_U8_2800;
    do {
        if (dovi_cli_read(&cli_param, DOVI_CLI_PRIORITY_OVERRIDE_RESERVED) == 1) {
            dovi_cli_set_priority(&cli_param, 1);
        }
        input_count = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_INPUT)];
        if (input_count < 1) {
            valid_source_count = 0;
            completion_index = 0;
        } else {
            input_index = 0;
            valid_source_count = 0;
            do {
                if (input_index == 3)
                    goto trap_invalid_input_count;
                if (setting_words[DOVI_INPUT_WORD(input_index, DOVI_INPUT_VALID)] != 0) {
                    if (3 < valid_source_count)
                        goto trap_invalid_input_count;
                    source_index = (unsigned long)valid_source_count;
                    (&DOVI_SOURCE_U64_0050)[source_index * 0x14] = 0;
                    (&DOVI_SOURCE_U64_0038)[source_index * 0x14] = 0;
                    (&DOVI_SOURCE_U64_0048)[source_index * 0x14] = 0;
                    (&DOVI_SOURCE_U32_0040)[source_index * 0x28] = 0;
                    if ((scratch.payloads[input_index + 4] != 0) &&
                        (setting_words[DOVI_INPUT_WORD(input_index, DOVI_INPUT_INPUT_MODE)] == 0)) {
                        composition_offset = scratch.payload_sizes[input_index + 0xc];
                        memcpy(&src_comp_metadata + input_index * 0x708,
                               (void *)(scratch.payloads[input_index + 4] + (long)composition_offset), 0x708);
                        scratch.payload_sizes[input_index + 0xc] = composition_offset + 0x708;
                        (&DOVI_SOURCE_U64_0048)[source_index * 0x14] =
                            (unsigned long)(&src_comp_metadata + input_index * 0x708);
                    }
                    metadata_source_address = scratch.payloads[input_index];
                    if (metadata_source_address != 0) {
                        metadata_slot_offset = input_index * 0x1000;
                        metadata_size = scratch.payload_sizes[input_index];
                        metadata_input_offset = scratch.payload_sizes[input_index + 8];
                        metadata_output = &dm_metadata_buf + input_index * 0x200;
                        metadata_available32 = metadata_size - metadata_input_offset;
                        metadata_write_offset = (unsigned long)metadata_available32;
                        if (metadata_available32 < 0x47) {
                            memcpy(metadata_output, (void *)(metadata_source_address + metadata_input_offset),
                                   metadata_write_offset);
                            scratch.payload_sizes[input_index + 8] = metadata_size;
                            metadata_input_offset = metadata_size;
                            if (metadata_available32 != 0)
                                goto consume_metadata_extensions;
                            metadata_size_written = 0;
                        } else {
                            metadata_input_bytes = (const unsigned char *)(metadata_source_address + metadata_input_offset);
                            metadata_write_offset = 0x47;
                            scratch.payload_sizes[input_index + 8] = metadata_input_offset + 0x47;
                            *metadata_output = fixed_load_le64(metadata_input_bytes);
                            metadata_pair1 = fixed_load_le64(metadata_input_bytes + 32);
                            metadata_pair0 = fixed_load_le64(metadata_input_bytes + 8);
                            metadata_pair2 = fixed_load_le64(metadata_input_bytes + 16);
                            (&DOVI_DM_METADATA_U64_0018)[input_index * 0x200] =
                                fixed_load_le64(metadata_input_bytes + 24);
                            (&DOVI_DM_METADATA_U64_0020)[input_index * 0x200] = metadata_pair1;
                            (&DOVI_DM_METADATA_U64_0008)[input_index * 0x200] = metadata_pair0;
                            (&DOVI_DM_METADATA_U64_0010)[input_index * 0x200] = metadata_pair2;
                            metadata_pair0 = fixed_load_le64(metadata_input_bytes + 48);
                            metadata_pair1 = fixed_load_le64(metadata_input_bytes + 56);
                            metadata_pair2 = fixed_load_le64(metadata_input_bytes + 40);
                            fixed_store_le64((unsigned char *)&DOVI_DM_METADATA_U64_0038 + metadata_slot_offset +
                                                 7,
                                             fixed_load_le64(metadata_input_bytes + 0x3f));
                            (&DOVI_DM_METADATA_U64_0030)[input_index * 0x200] = metadata_pair0;
                            (&DOVI_DM_METADATA_U64_0038)[input_index * 0x200] = metadata_pair1;
                            (&DOVI_DM_METADATA_U64_0028)[input_index * 0x200] = metadata_pair2;
                            metadata_input_offset = metadata_input_offset + 0x47;
                        consume_metadata_extensions:
                            metadata_size_written = (unsigned int)metadata_write_offset;
                            if (0x2000 < (input_index << 0xc | 0x46U))
                                goto trap_metadata_bounds;
                            if ((&DOVI_DM_METADATA_U8_0046)[metadata_slot_offset] != '\0') {
                                extension_index = 0;
                                do {
                                    metadata_bytes_available = metadata_size - metadata_input_offset;
                                    if (4 < metadata_bytes_available) {
                                        unsigned char *extension_output;
                                        metadata_read_offset = (long)metadata_input_offset;
                                        metadata_input_offset = metadata_input_offset + 5;
                                        extension_output = (unsigned char *)metadata_output +
                                                           (long)(int)metadata_write_offset;
                                        metadata_write_offset =
                                            (unsigned long)((int)metadata_write_offset + 5);
                                        scratch.payload_sizes[input_index + 8] = metadata_input_offset;
                                        extension_header_word = fixed_load_le32(
                                            (const void *)(metadata_source_address + metadata_read_offset));
                                        metadata_bytes_available = metadata_size - metadata_input_offset;
                                        extension_output[4] =
                                            *(unsigned char *)(metadata_source_address + metadata_read_offset + 4);
                                        fixed_store_le32(extension_output, extension_header_word);
                                    }
                                    extension_destination_offset = (int)metadata_write_offset;
                                    if (((((metadata_slot_offset + (long)(extension_destination_offset + -5)) >> 0xd != 0) ||
                                          ((metadata_slot_offset + (long)(extension_destination_offset + -4)) >> 0xd != 0)) ||
                                         ((metadata_slot_offset + (long)(extension_destination_offset + -3)) >> 0xd != 0)) ||
                                        ((metadata_slot_offset + (long)(extension_destination_offset + -2)) >> 0xd != 0))
                                        goto trap_metadata_bounds;
                                    extension_copy_size =
                                        (long)(int)((unsigned int)*(
                                                        unsigned char *)((long)metadata_output +
                                                                         (long)(extension_destination_offset + -5))
                                                    << 0x18) |
                                        (unsigned long)*(unsigned char *)((long)metadata_output +
                                                                          (long)(extension_destination_offset + -4))
                                            << 0x10 |
                                        (unsigned long)*(unsigned char *)((long)metadata_output +
                                                                          (long)(extension_destination_offset + -3))
                                            << 8 |
                                        (unsigned long)*(unsigned char *)((long)metadata_output +
                                                                          (long)(extension_destination_offset + -2));
                                    extension_length = (int)extension_copy_size;
                                    if (extension_length <= metadata_bytes_available) {
                                        memcpy((void *)((long)metadata_output + (long)extension_destination_offset),
                                               (void *)(metadata_source_address + metadata_input_offset), extension_copy_size);
                                        metadata_write_offset =
                                            (unsigned long)(unsigned int)(extension_length +
                                                                          extension_destination_offset);
                                        metadata_input_offset = extension_length + metadata_input_offset;
                                        scratch.payload_sizes[input_index + 8] = metadata_input_offset;
                                    }
                                    metadata_size_written = (unsigned int)metadata_write_offset;
                                    extension_index = extension_index + 1;
                                } while (extension_index <
                                         (unsigned char)(&DOVI_DM_METADATA_U8_0046)[metadata_slot_offset]);
                            }
                            if (0x2000 < (metadata_slot_offset | 0x3e))
                                goto trap_metadata_bounds;
                            *(unsigned char *)((long)&DOVI_DM_METADATA_U64_0038 + metadata_slot_offset + 6) = 1;
                        }
                        (&DOVI_SOURCE_U32_0040)[source_index * 0x28] = metadata_size_written;
                        (&DOVI_SOURCE_U64_0038)[source_index * 0x14] = (unsigned long)metadata_output;
                    }
                    vsem_packets_address = *(long *)(setting_words + input_index * 0x1a + 0x2a);
                    if ((vsem_packets_address == 0) || (setting_words[input_index * 0x1a + 0x2c] < 1)) {
                        vsem_packets_address = 0;
                    }
                    (&DOVI_SOURCE_U64_0050)[source_index * 0x14] = vsem_packets_address;
                    valid_source_count = valid_source_count + 1;
                    input_count = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_NUM_INPUT)];
                }
                input_index = input_index + 1;
            } while (input_index < input_count);
            completion_index = (unsigned int)input_index;
        }
        if (valid_source_count != dovi_cli_read(&cli_param, DOVI_CLI_INPUT_COUNT)) {
            _printk("please check valid num_input %d %d\n", valid_source_count);
        }
        if (((unsigned char)enable_debug >> 2 & 1) != 0) {
            if ((int)valid_source_count < 1) {
                completion_index = 0;
            } else {
                _printk("cli_param[%d] md %p,len %d,color %d,yuv_range %d\n", 0,
                        DOVI_SOURCE_U64_0038, DOVI_SOURCE_U32_0040, DOVI_SOURCE_U32_0060,
                        DOVI_SOURCE_U32_0074);
                if (valid_source_count == 1) {
                    completion_index = 1;
                } else {
                    _printk("cli_param[%d] md %p,len %d,color %d,yuv_range %d\n", 1,
                            DOVI_SOURCE_U64_00D8, DOVI_SOURCE_U32_00E0, DOVI_SOURCE_U32_0100,
                            DOVI_SOURCE_U32_0114);
                    if (valid_source_count == 2) {
                        completion_index = 2;
                    } else {
                        _printk("cli_param[%d] md %p,len %d,color %d,yuv_range %d\n", 2,
                                DOVI_SOURCE_U64_0178, DOVI_SOURCE_U32_0180, DOVI_SOURCE_U32_01A0,
                                DOVI_SOURCE_U32_01B4);
                        if (valid_source_count == 3) {
                            completion_index = 3;
                        } else {
                            _printk("cli_param[%d] md %p,len %d,color %d,yuv_range %d\n", 3,
                                    DOVI_SOURCE_U64_0218, DOVI_SOURCE_U32_0220,
                                    DOVI_SOURCE_U32_0240, DOVI_SOURCE_U32_0254);
                            if (valid_source_count != 4)
                                goto trap_invalid_input_count;
                            completion_index = 4;
                        }
                    }
                }
            }
        }
        result = dovi_commit_reg(&cp, scratch.input_configuration.bytes,
                            dovi_retained_section_1.source_parameters, scratch.target.words,
                            (unsigned char *)vsvdb_words, dovi_retained_section_1.core1_registers,
                            dovi_retained_section_1.core2_registers,
                            dovi_retained_section_1.core3_registers,
                            dovi_retained_section_1.output_dm_lut);
        if (output_format == 0) {
            setting_words[DOVI_SETTING_WORD(DOVI_SETTING_DIAGNOSTIC_ENABLE)] = (int)fixed_load_le64(
                (unsigned char *)dovi_retained_section_1.control_path_context + 0x1454);
            setting_words[DOVI_SETTING_WORD(DOVI_SETTING_DIAGNOSTIC_MUX_SELECT)] = fixed_load_le32(
                (unsigned char *)dovi_retained_section_1.control_path_context + 0x1458);
            setting_words[DOVI_SETTING_WORD(DOVI_SETTING_DOVI_LL_ENABLE)] = DOVI_CONTROL_U32_1CC8;
        }
        dovi_publish_registers(setting_words, multi_core1, scratch.primary_core, graphics_lut);
        if (output_format == 0) {
            dovi_publish_dolby_metadata(setting_words);
        } else if (output_format == 1) {
            dovi_publish_hdr_metadata(setting_words, scratch.target.words, &scratch.infoframe);
        }
        if (3 < completion_index)
            goto trap_invalid_input_count;
        pending_input_index = (long)(int)completion_index;
        if (0x10 < (unsigned long)(pending_input_index * 4)) {
        trap_metadata_bounds:
            /* WARNING: Does not return */
            dm_limit_trap();
        }
        if (scratch.payload_sizes[pending_input_index] == scratch.payload_sizes[pending_input_index + 8])
            break;
        if (0x20 < (unsigned long)(pending_input_index * 8))
            goto trap_metadata_bounds;
    } while ((scratch.payloads[pending_input_index + 4] == 0) ||
             (scratch.payload_sizes[pending_input_index + 4] != scratch.payload_sizes[pending_input_index + 0xc]));
    if (get_cpu_type_from_media() == 0)
        (void)get_meson_cpu_version();
return_result:
    return result;
}
