/* Shared control state, module signatures and fixed-point lookup tables. */
#include "shared_data.h"

struct dovi_control_storage dovi_retained_section_1 __attribute__((aligned(8)));
struct dovi_signature_storage dovi_retained_section_2 __attribute__((aligned(16))) = {
    .version_signature = "AML_DV_INFO1:",
    .chip_name = "chip_name = ",
};

const struct dovi_constant_storage dovi_retained_section_3 __attribute__((aligned(16))) = {
    .version = "[stb:2.6:e]-[v1.0]-",
    .diagnostics = {
        .assertion_false = "0",
        .default_chip_name = "s6",
        .string_format = "%s",
        .module_init_message = "\0016[dovi_gen_5_15_2026]: *** amlogic_dolby_vision_init dv: %s ***\n",
        .version_signature_format = " %s %s%s",
        .assertion_format = "\0010### ASSERTION FAILED %s: %s: %d: %s\n",
        .build_description = "[Tue Jun 2 07:47:00 UTC 2026]-[FEL Supporter]-[]-[]-[]-[0]",
        .control_init_failure = "\0013[dovi_gen_5_15_2026]: Control exuecution library initialization failed\n",
    },
    .level2_luma_trim = {
        0x00000001u, 0x00000002u, 0x00000003u, 0x00000006u,
    },
    .level8_luma_trim = {
        0x00000001u, 0x00000002u, 0x00000003u, 0x00000006u, 0x00000007u, 0x00000008u, 0x00000000u,
    },
    .level8_chroma_trim = {
        0x00000004u, 0x00000005u, 0x00000009u, 0x0000000au, 0x0000000bu, 0x0000000cu, 0x0000000du, 0x0000000eu,
        0x0000000fu, 0x00000010u, 0x00000011u, 0x00000012u, 0x00000013u, 0x00000014u,
    },
    .level2_chroma_trim = {
        0x00000004u, 0x00000005u,
    },
    .pulsar_gamma_to_linear = {
#include "gamma_pulsar_values.inc"
    },
    .sdr_gamma_to_linear = {
#include "gamma_sdr_values.inc"
    },
    .log10_value = {
#include "tone_log10_values.inc"
    },
    .log10_rate = {
#include "tone_log10_rates.inc"
    },
    .power10_value = {
#include "tone_power10_values.inc"
    },
    .power10_scale = {
        0x13, 0x16, 0x19, 0x1d, 0x20, 0x20, 0x20, 0x20,
        0x20, 0x20,
    },
    .linear_to_pq_slopes = {
#include "pq_luminance_slopes.inc"
    },
    .linear_to_pq_intercepts = {
#include "pq_luminance_intercepts.inc"
    },
    .linear_to_pq_positions = {
#include "pq_luminance_positions.inc"
    },
    .version8_primaries = {
        {
            0x02b851ecu, 0x0147ae14u, 0x010f5c29u, 0x02c28f5cu, 0x0099999au, 0x003d70a4u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x028f5c29u, 0x0151eb85u, 0x01333333u, 0x02666666u, 0x0099999au, 0x003d70a4u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02d4fdf4u, 0x012b020cu, 0x00ae147bu, 0x033020c5u, 0x008624ddu, 0x002f1aa0u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02851eb8u, 0x015c28f6u, 0x013d70a4u, 0x026147aeu, 0x009eb852u, 0x0047ae14u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x028f5c29u, 0x0151eb85u, 0x0128f5c3u, 0x02666666u, 0x0099999au, 0x003d70a4u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b851ecu, 0x0147ae14u, 0x010f5c29u, 0x02c28f5cu, 0x0099999au, 0x003d70a4u, 0x01418937u, 0x01676c8bu,
        },
        {
            0x02f05532u, 0x010faaceu, 0x00000000u, 0x04000000u, 0x00001a37u, 0xffb126e9u, 0x0149667bu, 0x0159c62au,
        },
        {
            0x02eb851fu, 0x011eb852u, 0x008f5c29u, 0x036b851fu, 0x00666666u, 0xffcccccdu, 0x0140346eu, 0x0150e560u,
        },
        {
            0x0310624eu, 0x0119999au, 0x00e66666u, 0x03333333u, 0x005b22d1u, 0xffa6e979u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02c5a1cbu, 0x01374bc7u, 0x00d4fdf4u, 0x030b4396u, 0x00963886u, 0x0035f6fdu, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02bf2e49u, 0x013be76du, 0x00ec8b44u, 0x02c28f5cu, 0x0098793eu, 0x004154cau, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b65fd9u, 0x01468db9u, 0x00f22d0eu, 0x02d1b717u, 0x0090624eu, 0x003212d7u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b851ecu, 0x0147ae14u, 0x010f5c29u, 0x02c28f5cu, 0x0099999au, 0x003d70a4u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02d119ceu, 0x012d0e56u, 0x00e88ce7u, 0x02e66666u, 0x0090ff97u, 0x0034d6a1u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b2b021u, 0x013da512u, 0x00e28241u, 0x02d7f62bu, 0x009ba5e3u, 0x003f62b7u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b8d4feu, 0x0146c227u, 0x010240b8u, 0x02ae48e9u, 0x008f0d84u, 0x0038bac7u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02bc3611u, 0x013be76du, 0x011566cfu, 0x028d1b71u, 0x009758e2u, 0x003c5048u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02b381d8u, 0x0146f694u, 0x010ded29u, 0x02bbe76du, 0x009bc01au, 0x0040346eu, 0x0140346eu, 0x0150e560u,
        },
        {
            0x02cadabau, 0x0128c155u, 0x00b9c0ecu, 0x02e02752u, 0x009b573fu, 0x003a0f91u, 0x0140346eu, 0x0150e560u,
        },
        {
            0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u,
        },
    },
};

#define DOVI_LAYOUT_CHECK(type, field, offset) \
    _Static_assert(__builtin_offsetof(struct type, field) == (offset), #type "." #field)
_Static_assert(sizeof(void *) == 8 && sizeof(unsigned long) == 8, "LP64 data layout");
_Static_assert(sizeof(struct dovi_control_storage) == 210856, "dovi_control_storage size");
DOVI_LAYOUT_CHECK(dovi_control_storage, content_callback_slot, 0);
DOVI_LAYOUT_CHECK(dovi_control_storage, enable_debug, 8);
DOVI_LAYOUT_CHECK(dovi_control_storage, source_content_type, 12);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_0000d, 13);
DOVI_LAYOUT_CHECK(dovi_control_storage, desired_white_point, 16);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_00011, 17);
DOVI_LAYOUT_CHECK(dovi_control_storage, level11_byte2, 20);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_00015, 21);
DOVI_LAYOUT_CHECK(dovi_control_storage, level11_byte3, 24);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_00019, 25);
DOVI_LAYOUT_CHECK(dovi_control_storage, control_path_context, 32);
DOVI_LAYOUT_CHECK(dovi_control_storage, core1_registers, 166200);
DOVI_LAYOUT_CHECK(dovi_control_storage, core2_registers, 168136);
DOVI_LAYOUT_CHECK(dovi_control_storage, core3_registers, 168480);
DOVI_LAYOUT_CHECK(dovi_control_storage, output_dm_lut, 169504);
DOVI_LAYOUT_CHECK(dovi_control_storage, context_initialized, 189984);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_2e621, 189985);
DOVI_LAYOUT_CHECK(dovi_control_storage, source_parameters, 189992);
DOVI_LAYOUT_CHECK(dovi_control_storage, composer_metadata, 190632);
DOVI_LAYOUT_CHECK(dovi_control_storage, dm_metadata, 194232);
DOVI_LAYOUT_CHECK(dovi_control_storage, cli_parameters, 202424);
DOVI_LAYOUT_CHECK(dovi_control_storage, hdmi_metadata, 209892);
DOVI_LAYOUT_CHECK(dovi_control_storage, reserved_335e1, 210401);
DOVI_LAYOUT_CHECK(dovi_control_storage, display_index_slot, 210408);
DOVI_LAYOUT_CHECK(dovi_control_storage, xyz_to_lms_slot, 210808);
DOVI_LAYOUT_CHECK(dovi_control_storage, ictcp_luma_slot, 210844);
DOVI_LAYOUT_CHECK(dovi_control_storage, ictcp_chroma_slot, 210848);
DOVI_LAYOUT_CHECK(dovi_control_storage, frame_number_slot, 210852);
_Static_assert(sizeof(struct dovi_signature_storage) == 532, "signature storage size");
DOVI_LAYOUT_CHECK(dovi_signature_storage, version_signature, 0);
DOVI_LAYOUT_CHECK(dovi_signature_storage, chip_name, 512);
_Static_assert(sizeof(struct dovi_diagnostic_strings) == 255, "diagnostic storage size");
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, assertion_false, 0);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, default_chip_name, 2);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, string_format, 5);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, module_init_message, 8);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, version_signature_format, 74);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, assertion_format, 83);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, build_description, 122);
DOVI_LAYOUT_CHECK(dovi_diagnostic_strings, control_init_failure, 181);
_Static_assert(sizeof(struct dovi_constant_storage) == 70156, "constant storage size");
DOVI_LAYOUT_CHECK(dovi_constant_storage, version, 0);
DOVI_LAYOUT_CHECK(dovi_constant_storage, diagnostics, 20);
DOVI_LAYOUT_CHECK(dovi_constant_storage, level2_luma_trim, 276);
DOVI_LAYOUT_CHECK(dovi_constant_storage, level8_luma_trim, 292);
DOVI_LAYOUT_CHECK(dovi_constant_storage, level8_chroma_trim, 320);
DOVI_LAYOUT_CHECK(dovi_constant_storage, level2_chroma_trim, 376);
DOVI_LAYOUT_CHECK(dovi_constant_storage, pulsar_gamma_to_linear, 384);
DOVI_LAYOUT_CHECK(dovi_constant_storage, sdr_gamma_to_linear, 1408);
DOVI_LAYOUT_CHECK(dovi_constant_storage, log10_value, 2432);
DOVI_LAYOUT_CHECK(dovi_constant_storage, log10_rate, 10624);
DOVI_LAYOUT_CHECK(dovi_constant_storage, power10_value, 27008);
DOVI_LAYOUT_CHECK(dovi_constant_storage, power10_scale, 67968);
DOVI_LAYOUT_CHECK(dovi_constant_storage, linear_to_pq_slopes, 67980);
DOVI_LAYOUT_CHECK(dovi_constant_storage, linear_to_pq_intercepts, 68492);
DOVI_LAYOUT_CHECK(dovi_constant_storage, linear_to_pq_positions, 69004);
DOVI_LAYOUT_CHECK(dovi_constant_storage, version8_primaries, 69516);
#undef DOVI_LAYOUT_CHECK
