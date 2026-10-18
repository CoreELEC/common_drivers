/* Publish Dolby metadata to the host setting. */
#include "control_orchestration_bindings.h"
#include "control_setting_layout.h"
#include "control_publish.h"
#include "fixed_access.h"
#include "control_cli.h"

void dovi_publish_registers(int *setting_words, _Bool multi_core1, unsigned int primary_core,
                            unsigned char *graphics_lut)
{
    unsigned long word0;
    unsigned long word1;
    unsigned char debug_flags;
    unsigned long word2;
    unsigned long word3;
    unsigned long word4;
    unsigned char *lut_source;
    int final_register_word;
    unsigned long word5;
    final_register_word = DOVI_CORE1_U32_0080;
    word5 = DOVI_CORE1_U64_0078;
    word0 = DOVI_CORE1_U64_0070;
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x304,
                     DOVI_CORE1_U64_0068);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x30c, word0);
    word1 = DOVI_CORE1_U64_0050;
    word0 = DOVI_CORE1_U64_0048;
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x314, word5);
    setting_words[DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x31c)] = final_register_word;
    word2 = DOVI_CORE1_U64_0060;
    word5 = DOVI_CORE1_U64_0058;
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2e4, word0);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2ec, word1);
    word1 = DOVI_CORE1_U64_0030;
    word0 = DOVI_CORE1_U64_0028;
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2f4, word5);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2fc, word2);
    word2 = DOVI_CORE1_U64_0040;
    word5 = DOVI_CORE1_U64_0038;
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2c4, word0);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2cc, word1);
    word1 = DOVI_CORE1_U64_0020;
    word0 = CONCAT44(DOVI_CORE1_U32_001C, DOVI_CORE1_U32_0018);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2d4, word5);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2dc, word2);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2b4, word0);
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x2bc, word1);
    memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CORE1), &DOVI_CORE1_U8_00C8, 0x2b4);
    word0 = DOVI_CORE1_U64_03E8;
    if (multi_core1) {
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19d4,
                         DOVI_CORE1_U64_03E0);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19dc, word0);
        word5 = DOVI_CORE1_U64_0408;
        word1 = DOVI_CORE1_U64_03F8;
        word0 = DOVI_CORE1_U64_03F0;
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19f4,
                         DOVI_CORE1_U64_0400);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19fc, word5);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19e4, word0);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x19ec, word1);
        word5 = DOVI_CORE1_U64_0428;
        word1 = DOVI_CORE1_U64_0418;
        word0 = DOVI_CORE1_U64_0410;
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a14,
                         DOVI_CORE1_U64_0420);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a1c, word5);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a04, word0);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a0c, word1);
        word5 = DOVI_CORE1_U64_0440;
        word1 = DOVI_CORE1_U64_0438;
        word0 = DOVI_CORE1_U64_0430;
        setting_words[DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x1a3c)] = DOVI_CORE1_U32_0448;
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a2c, word1);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a34, word5);
        fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_CORE1 + 0x1a24, word0);
        memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x1720), &DOVI_CORE1_U8_0490,
               0x2b4);
    }
    word2 = DOVI_CORE2_U64_0070;
    word5 = DOVI_CORE2_U64_0068;
    word0 = DOVI_CORE2_U64_0060;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x40)) =
        DOVI_CORE2_U64_0058;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x48)) = word0;
    word1 = DOVI_CORE2_U64_0040;
    word0 = DOVI_CORE2_U64_0038;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x50)) = word5;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x58)) = word2;
    word2 = DOVI_CORE2_U64_0050;
    word5 = DOVI_CORE2_U64_0048;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x20)) = word0;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x28)) = word1;
    word1 = DOVI_CORE2_U64_0020;
    word0 = DOVI_CORE2_U64_0018;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x30)) = word5;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x38)) = word2;
    word2 = DOVI_CORE2_U64_0030;
    word5 = DOVI_CORE2_U64_0028;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2)) = word0;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x8)) = word1;
    word4 = DOVI_CORE3_U64_0070;
    word3 = DOVI_CORE3_U64_0068;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x10)) = word5;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG2 + 0x18)) = word2;
    word1 = DOVI_CORE3_U64_0050;
    word0 = DOVI_CORE3_U64_0048;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x50)) = word3;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x58)) = word4;
    word2 = DOVI_CORE3_U64_0060;
    word5 = DOVI_CORE3_U64_0058;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x60)) =
        DOVI_CORE3_U64_0078;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x30)) = word0;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x38)) = word1;
    word0 = DOVI_CORE3_U64_0028;
    word1 = DOVI_CORE3_U64_0020;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x40)) = word5;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x48)) = word2;
    word3 = DOVI_CORE3_U64_0040;
    word2 = DOVI_CORE3_U64_0038;
    word5 = DOVI_CORE3_U64_0030;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x10)) = word0;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x18)) = word5;
    word0 = DOVI_CORE3_U64_0018;
    debug_flags = (unsigned char)enable_debug;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x20)) = word2;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x28)) = word3;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3)) = word0;
    *(unsigned long *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_REG3 + 0x8)) = word1;
    if ((debug_flags >> 2 & 1) == 0) {
        if (!multi_core1)
            goto select_single_core1_lut;
    publish_both_core1_luts:
        memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x320), &dst_dm_lut, 0x1400);
        memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x1a40), &DOVI_DM_LUT_U8_1400,
               0x1400);
        lut_source = graphics_lut;
    } else {
        _printk("reg1[0].SRange 0x%x\n", DOVI_CORE1_U32_0018);
        if (multi_core1)
            goto publish_both_core1_luts;
    select_single_core1_lut:
        if (primary_core == 0) {
            lut_source = &dst_dm_lut;
        } else {
            memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CORE1 + 0x320), &dst_dm_lut,
                   0x1400);
            lut_source = &DOVI_DM_LUT_U8_1400;
        }
    }
    memcpy(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_DM_LUT2), lut_source, 0x1400);
}

void dovi_publish_dolby_metadata(int *setting_words)
{
    unsigned short partial_word16;
    unsigned int partial_word24;
    unsigned int metadata_bytes;
    unsigned int packed_word;
    unsigned int extensions_remaining;
    void *extension_output;
    unsigned int extension_mask;
    int metadata_size;
    unsigned long output_remaining;
    unsigned long extension_length;
    long control_data_length;
    long raw_offset;
    unsigned long bounded_copy_bytes;
    unsigned long payload_remaining;
    unsigned long metadata_length;
    unsigned long extension_offset;
    unsigned char content_type, white_point, byte2, byte3;
    unsigned int *availability = (unsigned int *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD));
    metadata_bytes = dovi_dm_GetMdsOutput(&md_hdmi_buf, 0x1fd, 0, 0, 1, (void *)cp);
    control_data_length = dovi_get_dolby_control_data(
        &cp, *(void **)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_OUTPUT_CTRL_DATA)),
        setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CTRL_DATA_TYPE),
        setting_words + DOVI_SETTING_WORD(DOVI_SETTING_OUTPUT_VSIF));
    setting_words[DOVI_SETTING_WORD(DOVI_SETTING_OUTPUT_CTRL_DATA_LEN)] = (int)control_data_length;
    metadata_size = metadata_bytes + 5;
    if (-1 < (int)(metadata_bytes + 2)) {
        metadata_size = metadata_bytes + 2;
    }
    *availability = 0;
    setting_words[DOVI_SETTING_WORD(DOVI_SETTING_MD_REG3 + 0x800)] = (metadata_size >> 2) + 1;
    setting_words[DOVI_SETTING_WORD(DOVI_SETTING_MD_REG3)] = (unsigned int)md_hdmi_buf | metadata_bytes
                                                                                             << 8;
    if (1 < (int)metadata_bytes) {
        metadata_length = (unsigned long)metadata_bytes;
        raw_offset = 0;
        output_remaining = 0x1ff;
        do {
            payload_remaining = (metadata_length + output_remaining) - 0x201;
            bounded_copy_bytes = output_remaining;
            if (payload_remaining <= output_remaining) {
                bounded_copy_bytes = payload_remaining;
            }
            if (2 < bounded_copy_bytes) {
                bounded_copy_bytes = 3;
            }
            if (output_remaining == bounded_copy_bytes)
                dm_limit_trap();
            packed_word = (unsigned int)(unsigned char)(&DOVI_HDMI_U8_0001)[raw_offset];
            if (metadata_length - 2 == (unsigned long)raw_offset) {
            store_last_metadata_word:
                *(unsigned int *)((long)setting_words + raw_offset + 0x44b4) = packed_word;
                break;
            }
            partial_word16 = CONCAT11((&DOVI_HDMI_U8_0002)[raw_offset], (&DOVI_HDMI_U8_0001)[raw_offset]);
            packed_word = (unsigned int)partial_word16;
            if (metadata_length - 3 == (unsigned long)raw_offset)
                goto store_last_metadata_word;
            partial_word24 = CONCAT12((&DOVI_HDMI_U8_0003)[raw_offset], partial_word16);
            packed_word = (unsigned int)partial_word24;
            if (metadata_length - 4 == (unsigned long)raw_offset)
                goto store_last_metadata_word;
            if (0x1fc < raw_offset + 1U)
                dm_limit_trap();
            output_remaining = output_remaining - 4;
            *(unsigned int *)((long)setting_words + raw_offset + 0x44b4) =
                CONCAT13((&DOVI_HDMI_U8_0004)[raw_offset], partial_word24);
            raw_offset = raw_offset + 4;
        } while (metadata_length - 1 != (unsigned long)raw_offset);
    }
    extensions_remaining = (unsigned int)DOVI_HDMI_U8_0046;
    if (DOVI_HDMI_U8_0046 != 0) {
        extension_offset = 0x47;
        do {
            if (((0x1ff < extension_offset) || (extension_offset - 0x1ff < 0xfffffffffffffe00)) ||
                ((extension_offset - 0x1fe < 0xfffffffffffffe00 ||
                  ((extension_offset - 0x1fd < 0xfffffffffffffe00 ||
                    (extension_offset - 0x1fc < 0xfffffffffffffe00))))))
                dm_limit_trap();
            extension_length = (long)(int)((unsigned int)(&md_hdmi_buf)[extension_offset] << 0x18) |
                     (unsigned long)(unsigned char)(&DOVI_HDMI_U8_0001)[extension_offset] << 0x10 |
                     (unsigned long)(unsigned char)(&DOVI_HDMI_U8_0002)[extension_offset] << 8 |
                     (unsigned long)(unsigned char)(&DOVI_HDMI_U8_0003)[extension_offset];
            metadata_size = (int)extension_length;
            switch ((&DOVI_HDMI_U8_0004)[extension_offset]) {
            case 1:
                if (5 < metadata_size) {
                    metadata_size = 6;
                }
                extension_mask = *availability | 1;
                extension_output = setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD + 0x4);
                break;
            case 2:
                if (0xd < metadata_size) {
                    metadata_size = 0xe;
                }
                extension_mask = *availability | 2;
                extension_output = (unsigned char *)setting_words + DOVI_SETTING_EXT_MD + 0xa;
                break;
            case 3:
                goto advance_extension;
            case 4:
                if (3 < metadata_size) {
                    metadata_size = 4;
                }
                extension_mask = *availability | 4;
                extension_output = setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD + 0x18);
                break;
            case 5:
                if (7 < metadata_size) {
                    metadata_size = 8;
                }
                extension_mask = *availability | 8;
                extension_output = setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD + 0x1c);
                break;
            case 6:
                if (7 < metadata_size) {
                    metadata_size = 8;
                }
                extension_mask = *availability | 0x10;
                extension_output = setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD + 0x24);
                break;
            default:
                if ((&DOVI_HDMI_U8_0004)[extension_offset] == 255) {
                    if (5 < metadata_size) {
                        metadata_size = 6;
                    }
                    extension_mask = *availability | 0x80000000;
                    extension_output =
                        setting_words + DOVI_SETTING_WORD(DOVI_SETTING_EXT_MD + 0x2c);
                    break;
                }
                goto advance_extension;
            }
            *availability = extension_mask;
            memcpy(extension_output, &DOVI_HDMI_U8_0005 + extension_offset, (long)metadata_size);
        advance_extension:
            extension_offset = extension_offset + 5 + extension_length;
            extensions_remaining = extensions_remaining - 1;
        } while (extensions_remaining != 0);
    }
    content_type = src_content_type;
    *(unsigned char *)(setting_words + DOVI_SETTING_WORD(DOVI_SETTING_CONTENT_INFO)) = content_type;
    white_point = src_desired_white_point;
    *(unsigned char *)((long)setting_words + DOVI_SETTING_CONTENT_INFO + 1) = white_point;
    byte2 = src_l11_byte2;
    *(unsigned char *)((long)setting_words + DOVI_SETTING_CONTENT_INFO + 2) = byte2;
    byte3 = src_l11_byte3;
    *(unsigned char *)((long)setting_words + DOVI_SETTING_CONTENT_INFO + 3) = byte3;
    if (((unsigned char)enable_debug >> 1 & 1) != 0) {
        _printk("src content %x %x %x %x\n", content_type, white_point, byte2, byte3);
    }
}

void dovi_publish_hdr_metadata(int *setting_words, int *target_parameters,
                               union dovi_hdr_infoframe *infoframe)
{
    unsigned int primary_input;
    unsigned int *hdr_parameters;
    unsigned int primary0_xy;
    if (3 < dovi_cli_read(&cli_param, DOVI_CLI_PRIMARY_INPUT))
        dm_limit_trap();
    get_info_frame(&cp, target_parameters,
                   (&DOVI_SOURCE_U64_0038)[(unsigned long)dovi_cli_read(&cli_param, DOVI_CLI_PRIMARY_INPUT) * 0x14],
                   infoframe->bytes);
    primary_input = setting_words[DOVI_SETTING_WORD(DOVI_SETTING_PRI_INPUT)];
    if (2 < primary_input)
        dm_limit_trap();
    if ((setting_words[DOVI_INPUT_WORD((unsigned long)primary_input, DOVI_INPUT_SRC_FORMAT)] ==
         1) &&
        ((enable_debug >> 3 & 1) == 0)) {
        hdr_parameters =
            *(unsigned int **)(setting_words + DOVI_INPUT_WORD((unsigned long)primary_input,
                                                               DOVI_INPUT_P_HDR10_PARAM));
        primary0_xy = hdr_parameters[2];
        infoframe->fields.primary1_x = *(unsigned short *)(hdr_parameters + 3);
        infoframe->fields.primary1_y = *(unsigned short *)((long)hdr_parameters + 0xe);
        infoframe->fields.primary0_x = (unsigned short)primary0_xy;
        infoframe->fields.primary0_y_lo = (unsigned char)((unsigned int)primary0_xy >> 0x10);
        infoframe->fields.primary0_y_hi = (unsigned char)((unsigned int)primary0_xy >> 0x18);
        infoframe->fields.primary2_x = *(unsigned short *)(hdr_parameters + 4);
        infoframe->fields.white_x = *(unsigned short *)(hdr_parameters + 5);
        infoframe->fields.white_y = *(unsigned short *)((long)hdr_parameters + 0x16);
        infoframe->fields.primary2_y_lo =
            (unsigned char)*(unsigned short *)((long)hdr_parameters + 0x12);
        infoframe->fields.primary2_y_hi =
            (unsigned char)((unsigned short)*(unsigned short *)((long)hdr_parameters + 0x12) >> 8);
        infoframe->fields.max_luminance_lo =
            (unsigned char)((unsigned int)hdr_parameters[1] / 10000);
        infoframe->fields.max_luminance_hi =
            (unsigned char)((unsigned int)hdr_parameters[1] / 0x271000);
        infoframe->fields.min_luminance_lo = (unsigned char)*hdr_parameters;
        infoframe->fields.min_luminance_hi = (unsigned char)((unsigned int)*hdr_parameters >> 8);
        infoframe->fields.max_content_light_lo = *(unsigned char *)(hdr_parameters + 6);
        infoframe->fields.max_content_light_hi = *(unsigned char *)((long)hdr_parameters + 0x19);
        infoframe->fields.max_frame_average_lo = *(unsigned char *)((long)hdr_parameters + 0x1a);
        infoframe->fields.max_frame_average_hi = *(unsigned char *)((long)hdr_parameters + 0x1b);
    }
    if ((enable_debug >> 1 & 1) != 0) {
        _printk("HDR10 out primary %x %x %x %x %x %x\n", infoframe->fields.primary0_x,
                CONCAT11(infoframe->fields.primary0_y_hi, infoframe->fields.primary0_y_lo),
                infoframe->fields.primary1_x, infoframe->fields.primary1_y,
                infoframe->fields.primary2_x,
                CONCAT11(infoframe->fields.primary2_y_hi, infoframe->fields.primary2_y_lo));
        _printk("HDR10 out white_point %x, %x\n", infoframe->fields.white_x,
                infoframe->fields.white_y);
        _printk("HDR10 out min %d, max %d\n",
                CONCAT11(infoframe->fields.min_luminance_hi, infoframe->fields.min_luminance_lo),
                CONCAT11(infoframe->fields.max_luminance_hi, infoframe->fields.max_luminance_lo));
        _printk("HDR10 out MCLL %d, MPALL %d\n",
                CONCAT11(infoframe->fields.max_content_light_hi,
                         infoframe->fields.max_content_light_lo),
                CONCAT11(infoframe->fields.max_frame_average_hi,
                         infoframe->fields.max_frame_average_lo));
    }
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_HDR_INFO,
                     fixed_load_le64(infoframe->bytes));
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_HDR_INFO + 8,
                     fixed_load_le64(infoframe->bytes + 8));
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_HDR_INFO + 16,
                     fixed_load_le64(infoframe->bytes + 16));
    fixed_store_le64((unsigned char *)setting_words + DOVI_SETTING_HDR_INFO + 21,
                     fixed_load_le64(infoframe->bytes + 21));
}

void dovi_notify_content_info(void *context, const void *sources, unsigned long primary,
                              const int *source_flags, const void *metadata_slots,
                              const void *header_slots)
{
    long *control = context;
    long source_parameters = (long)sources;
    const long *metadata = metadata_slots;
    const unsigned long *headers = header_slots;
    unsigned int size_bytes;
    unsigned char content_type;
    dovi_content_info_callback notify;
    int white_point;
    const unsigned char *white;
    unsigned char extra2;
    unsigned int has_content;
    long payload;
    const unsigned char *content;
    unsigned int remaining;
    const unsigned char *byte2;
    const unsigned char *extension;
    int *header;

    notify = dovi_content_info_cb_handle;
    if (source_flags[primary] == 0) {
        payload = metadata[primary];
        if ((*(int *)(source_parameters + primary * 0xa0 + 0xc) != 1) ||
            (header = (int *)headers[primary], *header == 0)) {
            if ((payload != 0) && (remaining = (unsigned int)*(unsigned char *)(payload + 0x46),
                                   *(unsigned char *)(payload + 0x46) != 0)) {
                extension = (const unsigned char *)(payload + 0x47);
                do {
                    content = extension + 5;
                    if ((signed char)fixed_load_le32(extension + 4) == '\v') {
                        white = extension + 6;
                        byte2 = extension + 7;
                        extension = extension + 8;
                        goto notify_level11;
                    }
                    remaining = remaining - 1;
                    size_bytes = (fixed_load_le32(extension) & 0xff00ff00) >> 8 |
                                 (fixed_load_le32(extension) & 0xff00ff) << 8;
                    extension = content + (size_bytes >> 0x10 | size_bytes << 0x10);
                } while (remaining != 0);
            }
            goto clear_content;
        }
        if (header[6] == 0)
            goto clear_content;
        content = (unsigned char *)((long)header + 0x1f);
        white = (const unsigned char *)(header + 8);
        byte2 = (unsigned char *)((long)header + 0x21);
        extension = (const unsigned char *)header + 0x22;
    notify_level11:
        remaining = fixed_load_le32(extension);
        extra2 = *byte2;
        white_point = fixed_signed32(fixed_load_le32(white));
        content_type = *content;
        (*notify)(content_type, (signed char)white_point, extra2, (signed char)remaining);
        has_content = 1;
    } else {
    clear_content:
        if ((int)control[0x28c] != 0) {
            (*notify)(0, 0, 0, 0);
        }
        has_content = 0;
    }
    *(unsigned int *)(control + 0x28c) = has_content;
}
