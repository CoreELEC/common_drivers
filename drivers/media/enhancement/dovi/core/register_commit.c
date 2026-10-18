/* Commit control configuration and pack composer, DM and LUT output.
 * Pointer offsets describe the LP64 ABI; scratch aliases preserve overlapping
 * local structs.
 */
#include "control_orchestration_bindings.h"
#include "fixed_access.h"
#include "control_target.h"
#include "control_registers.h"
#include "control_publish.h"
#include "control_workspace.h"

static int prepare_commit_sources(long *control, long source_parameters, unsigned char input_count,
                                  unsigned long primary_input, unsigned int *input_count_slot,
                                  struct dovi_commit_workspace *work, unsigned int *status)
{
    unsigned short primary0_x16;
    unsigned short primary1_x16;
    unsigned short primary0_y16;
    unsigned short primary2_x16;
    unsigned short primary1_y16;
    unsigned short primary2_y16;
    unsigned int primary2_y_candidate;
    unsigned int primary0_y_candidate;
    unsigned int primary_y_candidate;
    unsigned long blue_primary_index;
    long green_primary_index;
    long red_primary_index;
    int *primary0_x_slot;
    long hdr_source_address;
    int primary1_x_coordinate;
    int primary2_x_coordinate;
    unsigned short hdr_max_luminance16;
    unsigned int packet_identifier;

    unsigned int source_number;
    unsigned char white_x_lo;
    unsigned char white_x_hi;
    unsigned char packet_type;
    unsigned char white_y_lo;
    unsigned char white_y_hi;
    unsigned char hdr_min_luminance_hi;
    unsigned char hdr_min_luminance_lo;
    unsigned short vsem_length16;
    unsigned __int128 primary0_x_product;
    unsigned __int128 primary0_y_product;
    unsigned __int128 primary1_x_product;
    unsigned __int128 primary1_y_product;
    unsigned __int128 primary2_x_product;
    unsigned __int128 primary2_y_product;
    unsigned __int128 white_x_product;
    unsigned __int128 white_y_product;
    _Bool has_metadata;
    unsigned short matrix_last16;
    unsigned int commit_status;
    unsigned int configuration_result;
    unsigned int configuration_word;
    int configuration_value;
    const unsigned char *packets;
    int *primary0_y_slot;
    char *packet_error;
    unsigned char packet_version;
    unsigned int *metadata_configuration;
    int *source_status_slot;
    unsigned int signal_word;
    unsigned int source_index32;
    unsigned long source_index;
    long source_address;
    long sink_white_x_word;
    long sink_luminance_pair;
    long signal_offset;
    int *bit_depth_slot;
    unsigned int primary_input_index;
    unsigned long signal_number;
    long signal_source_address;
    unsigned int primary1_x_compare;
    unsigned int primary_x_candidate;
    unsigned int primary2_x_compare;
    unsigned int primary1_y_compare;
    unsigned int *decoded_header;
    unsigned int *source_or_signal_words;
    int *input_mode_slot;
    unsigned int primary1_y_candidate;
    unsigned char primary_input_byte;
    primary_input_byte = (unsigned char)primary_input;
    commit_status = 0;
    primary_input_index = (unsigned int)primary_input_byte;
    if ((unsigned int)primary_input_byte < (unsigned int)input_count) {
        source_index = primary_input;
        do {
            source_index32 = (unsigned int)source_index;
            source_number = source_index32 + 1;
            source_or_signal_words = (unsigned int *)(source_parameters + source_index * 0xa0);
            *(unsigned int *)((long)control + (unsigned long)source_number * 4 + 0x928) = 1;
            packets = *(const unsigned char **)(source_or_signal_words + 0x14);
            if ((packets == (unsigned char *)0x0) &&
                (*(long *)(source_parameters + source_index * 0xa0 + 0x38) == 0)) {
                if (3 < source_index32)
                    dm_limit_trap();
                has_metadata = 0;
                input_mode_slot = (int *)(source_parameters + source_index * 0xa0 + 0xc);
                *(unsigned int *)((long)&work->source_flags.pairs[0] + source_index * 4) =
                    0xffffffff;
            select_source_status:
                source_status_slot = (int *)((long)&work->source_flags.pairs[0] + source_index * 4);
            load_source_metadata:
                source_address = source_parameters + source_index * 0xa0;
                decoded_header = (unsigned int *)work->decoded_headers[source_index];
                signal_word = *(unsigned int *)(source_address + 0x40);
                work->metadata[source_index] = *(long *)(source_address + 0x38);
            store_metadata_size:
                *(unsigned int *)((long)&work->metadata_sizes.pairs[0] + source_index * 4) = signal_word;
            } else {
                if (3 < source_index32)
                    dm_limit_trap();
                source_status_slot = (int *)((long)&work->source_flags.pairs[0] + source_index * 4);
                input_mode_slot = (int *)(source_parameters + source_index * 0xa0 + 0xc);
                configuration_value = *input_mode_slot;
                *source_status_slot = 0;
                if (configuration_value == 0) {
                    has_metadata = 1;
                    goto select_source_status;
                }
                if (configuration_value != 1) {
                use_existing_metadata:
                    has_metadata = 1;
                    goto load_source_metadata;
                }
                if (0x7c < source_index * 0x28)
                    goto trap_source_bounds;
                decoded_header = (unsigned int *)(&work->metadata_configuration[0] + source_index * 5);
                metadata_configuration = (unsigned int *)((long)&work->metadata_configuration[4] +
                                           source_index * 0x28 + 4);
                configuration_word = *(unsigned int *)(source_parameters + source_index * 0xa0 + 0x34);
                *metadata_configuration = configuration_word;
                *decoded_header = configuration_word;
                if (configuration_word != 0)
                    goto use_existing_metadata;
                work->decoded_headers[source_index] = (unsigned long)decoded_header;
                if (packets == (unsigned char *)0x0)
                    goto finish_packet_decoding;
                packet_type = *packets;
                if (packet_type == '\x7f') {
                    if ((unsigned char *)((long)control + 0x1ccc) == (unsigned char *)0x0) {
                        packet_error = "vsem_payload buffer is NULL\n";
                    } else {
                        configuration_value = dovi_vsem_unpack(packets, (unsigned char *)((long)control + 0x1ccc));
                        if (configuration_value == 0) {
                            if (*(char *)((long)control + 0x1cd6) != '\0') {
                                dovi_parse_vsif_from_PB4((void *)((long)control + 0x1cd9), decoded_header);
                                goto finish_packet_decoding;
                            }
                            vsem_length16 = fixed_load_le16((unsigned char *)control + 0x1cd1);
                            work->metadata[source_index] = (long)control + 0x1cd9;
                            configuration_value = ((unsigned int)(vsem_length16 >> 8) | (vsem_length16 & 0xff00ff) << 8) - 10;
                            *(int *)((long)&work->metadata_sizes.pairs[0] + source_index * 4) =
                                configuration_value;
                            goto select_decoded_metadata;
                        }
                        packet_error = "vsem_unpack error!\n";
                    }
                    _printk(packet_error);
                    commit_status = 0xfffffffb;
                    {
                        *status = commit_status;
                        return 0;
                    }
                }
                if (packet_type != 0x81) {
                    if (packet_type == 0x87) {
                        packet_version = packets[1];
                        if (packet_version == '\x01') {
                            if (packets[2] == '\x1a') {
                                primary0_x16 = fixed_load_le16(packets + 6);
                                white_x_lo = packets[0x12];
                                white_x_hi = packets[0x13];
                                white_y_lo = packets[0x14];
                                white_y_hi = packets[0x15];
                                primary1_x16 = fixed_load_le16(packets + 10);
                                primary0_y16 = fixed_load_le16(packets + 8);
                                work->temporary.primaries[0] =
                                    (unsigned long)CONCAT24(primary1_x16, (unsigned int)primary0_x16);
                                primary1_x_compare = (unsigned int)primary1_x16;
                                primary_x_candidate = (unsigned int)primary0_x16;
                                primary2_x16 = fixed_load_le16(packets + 0xe);
                                primary1_y16 = fixed_load_le16(packets + 0xc);
                                work->temporary.primaries[1] = CONCAT44(
                                    (*(unsigned int
                                           *)((unsigned char *)&work->temporary.primaries[1] + 4)),
                                    (unsigned int)primary2_x16);
                                primary2_y16 = fixed_load_le16(packets + 0x10);
                                work->primary_y[0] = (unsigned int)primary0_y16;
                                work->primary_y[1] = (unsigned int)primary1_y16;
                                work->primary_y[2] = (unsigned int)primary2_y16;
                                primary2_x_compare = (unsigned int)primary2_x16;
                                primary0_y_candidate = (unsigned int)primary0_y16;
                                primary1_y_compare = (unsigned int)primary1_y16;
                                primary2_y_candidate = (unsigned int)primary2_y16;
                                primary_y_candidate = (unsigned int)primary0_y16;
                                primary1_y_candidate = (unsigned int)primary1_y16;
                                if (((unsigned int)primary0_x16 < (unsigned int)primary1_x16 ||
                                     primary_x_candidate == primary1_x_compare) ||
                                    (primary_x_candidate < primary2_x16 || primary_x_candidate == primary2_x_compare)) {
                                    primary_x_candidate = (unsigned int)primary1_x16;
                                    if ((primary0_y_candidate < primary1_y_compare || primary_y_candidate == primary1_y_candidate) ||
                                        (primary_y_candidate < primary2_y_candidate || primary0_y_candidate == primary2_y_candidate)) {
                                        primary_y_candidate = 0;
                                        green_primary_index = 1;
                                        if (primary1_y_candidate < primary2_y_candidate || primary1_y_candidate == primary2_y16) {
                                            green_primary_index = 2;
                                        }
                                        primary2_y_candidate =
                                            (unsigned int)(primary_x_candidate < primary2_x_compare || primary1_x_compare == primary2_x_compare);
                                        red_primary_index = 1;
                                        if (primary_x_candidate < primary2_x_compare || primary1_x_compare == primary2_x_compare) {
                                            red_primary_index = 2;
                                        } else if (primary1_y_candidate <= primary2_y16) {
                                            blue_primary_index = 0;
                                            goto validate_sorted_primaries;
                                        }
                                    } else {
                                        green_primary_index = 0;
                                        primary_y_candidate = 1;
                                        if (primary2_x_compare <= primary1_x_compare && primary_x_candidate != primary2_x_compare) {
                                            red_primary_index = 1;
                                            blue_primary_index = 2;
                                            goto validate_sorted_primaries;
                                        }
                                        primary2_y_candidate = 1;
                                        red_primary_index = 2;
                                    }
                                select_blue_primary_index:
                                    blue_primary_index = (unsigned long)(primary_y_candidate & primary2_y_candidate);
                                } else {
                                    if (((unsigned int)primary1_y16 <= (unsigned int)primary0_y16 &&
                                         primary_y_candidate != primary1_y_candidate) &&
                                        (primary2_y16 <= primary_y_candidate && primary0_y_candidate != primary2_y_candidate)) {
                                        red_primary_index = 0;
                                        primary2_y_candidate = 0;
                                        green_primary_index = 0;
                                        primary_y_candidate = 1;
                                        goto select_blue_primary_index;
                                    }
                                    red_primary_index = 0;
                                    green_primary_index = 1;
                                    if (primary1_y_candidate < primary2_y_candidate || primary1_y_compare == primary2_y_candidate) {
                                        green_primary_index = 2;
                                    }
                                    blue_primary_index = 1;
                                    if (primary1_y_candidate >= primary2_y_candidate && primary1_y_compare != primary2_y_candidate) {
                                        blue_primary_index = 2;
                                    }
                                }
                            validate_sorted_primaries:
                                hdr_max_luminance16 = fixed_load_le16(packets + 0x16);
                                hdr_min_luminance_hi = packets[0x19];
                                hdr_min_luminance_lo = packets[0x18];
                                if (((((hdr_max_luminance16 == 0) ||
                                       ((*(unsigned int *)((long)&work->temporary.primaries[0] +
                                                           red_primary_index * 4) &
                                         0xfffe) != 0x8a26)) ||
                                      ((work->primary_y[red_primary_index] & 0xfffe) != 0x3976)) ||
                                     ((((*(unsigned int *)((long)&work->temporary.primaries[0] +
                                                           green_primary_index * 4) &
                                         0xfffe) != 0x2108 ||
                                        ((work->primary_y[green_primary_index] & 0xfffe) != 0x9b7e)) ||
                                       (((*(unsigned int *)((long)&work->temporary.primaries[0] +
                                                            blue_primary_index * 4) &
                                          0xfffe) != 0x19fa ||
                                         (((work->primary_y[blue_primary_index] & 0xfffe) != 0x846 ||
                                           ((white_x_lo & 0xfe) != 0xee)))))))) ||
                                    (((white_y_lo & 0xfe) != 0x30 || (packets[0x1a] < 0xfe)))) {
                                    *metadata_configuration = 0;
                                    *decoded_header = 0;
                                } else {
                                    has_metadata = (packets[0x1c] & 0xfe) == 0xaa;
                                    primary2_y_candidate = (unsigned int)has_metadata;
                                    *metadata_configuration = primary2_y_candidate;
                                    *decoded_header = primary2_y_candidate;
                                    if (has_metadata)
                                        goto finish_packet_decoding;
                                }
                                primary0_x_slot = (int *)(source_parameters + source_index * 0xa0 + 0x14);
                                if ((*primary0_x_slot == 0) &&
                                    (primary0_y_slot =
                                         (int *)(source_parameters + source_index * 0xa0 + 0x18),
                                     *primary0_y_slot == 0)) {
                                    hdr_source_address = source_parameters + source_index * 0xa0;
                                    primary1_x_coordinate =
                                        *(int *)((long)&work->temporary.primaries[0] + green_primary_index * 4);
                                    primary2_y_candidate = work->primary_y[green_primary_index];
                                    (*(unsigned long *)((unsigned char *)&primary0_x_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary0_x_product + 0)) =
                                        ((long)*(int *)((long)&work->temporary.primaries[0] +
                                                        red_primary_index * 4) &
                                         0x3fffffffffU)
                                        << 0x16;
                                    (*(unsigned long *)((unsigned char *)&primary0_y_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary0_y_product + 0)) =
                                        ((long)(int)work->primary_y[red_primary_index] & 0x3fffffffffU)
                                        << 0x16;
                                    primary2_x_coordinate =
                                        *(int *)((long)&work->temporary.primaries[0] + blue_primary_index * 4);
                                    primary_y_candidate = work->primary_y[blue_primary_index];
                                    *primary0_x_slot = SUB164(primary0_x_product * ZEXT816(0x14f8b588e368f085), 9);
                                    (*(unsigned long *)((unsigned char *)&primary1_x_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary1_x_product + 0)) =
                                        ((long)primary1_x_coordinate & 0x3fffffffffU) << 0x16;
                                    (*(unsigned long *)((unsigned char *)&primary1_y_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary1_y_product + 0)) =
                                        ((long)(int)primary2_y_candidate & 0x3fffffffffU) << 0x16;
                                    *primary0_y_slot = SUB164(primary0_y_product * ZEXT816(0x14f8b588e368f085), 9);
                                    (*(unsigned long *)((unsigned char *)&primary2_x_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary2_x_product + 0)) =
                                        ((long)primary2_x_coordinate & 0x3fffffffffU) << 0x16;
                                    (*(unsigned long *)((unsigned char *)&primary2_y_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&primary2_y_product + 0)) =
                                        ((long)(int)primary_y_candidate & 0x3fffffffffU) << 0x16;
                                    *(int *)(hdr_source_address + 0x1c) =
                                        SUB164(primary1_x_product * ZEXT816(0x14f8b588e368f085), 9);
                                    *(int *)(hdr_source_address + 0x20) =
                                        SUB164(primary1_y_product * ZEXT816(0x14f8b588e368f085), 9);
                                    *(int *)(hdr_source_address + 0x24) =
                                        SUB164(primary2_x_product * ZEXT816(0x14f8b588e368f085), 9);
                                    *(int *)(hdr_source_address + 0x28) =
                                        SUB164(primary2_y_product * ZEXT816(0x14f8b588e368f085), 9);
                                    (*(unsigned long *)((unsigned char *)&white_x_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&white_x_product + 0)) =
                                        (unsigned long)white_x_lo << 0x1a | (unsigned long)white_x_hi << 0x22;
                                    (*(unsigned long *)((unsigned char *)&white_y_product + 8)) = 0;
                                    (*(unsigned long *)((unsigned char *)&white_y_product + 0)) =
                                        (unsigned long)white_y_lo << 0x1a | (unsigned long)white_y_hi << 0x22;
                                    *(int *)(hdr_source_address + 0x2c) =
                                        SUB164(white_x_product * ZEXT816(0x14f8b588e3690), 8);
                                    *(int *)(hdr_source_address + 0x30) =
                                        SUB164(white_y_product * ZEXT816(0x14f8b588e3690), 8);
                                    *(int *)(hdr_source_address + 100) = (int)((unsigned int)hdr_min_luminance_hi << 0x1a |
                                                                   (unsigned int)hdr_min_luminance_lo << 0x12) /
                                                             10000;
                                    *(int *)(hdr_source_address + 0x68) =
                                        SUB164(ZEXT216(hdr_max_luminance16) * (unsigned __int128)0x40000 *
                                                   ZEXT816(0x68db8bac710cc),
                                               8);
                                }
                                goto finish_packet_decoding;
                            }
                            packet_version = '\x01';
                        }
                    } else {
                        packet_version = packets[1];
                    }
                reject_packet_header:
                    _printk("error EMP: %x, %x, %x, %x, %x, %x, %x\n", packet_type, packet_version, packets[2],
                            packets[3], packets[4], packets[5], packets[6]);
                    commit_status = 0xfffffffb;
                    {
                        *status = commit_status;
                        return 0;
                    }
                }
                packet_version = packets[1];
                if (packet_version != '\x01')
                    goto reject_packet_header;
                packet_identifier = (unsigned int)packets[6] << 0x10 | (unsigned int)packets[5] << 8 |
                         (unsigned int)packets[4];
                if (packet_identifier == 0xc03) {
                    if (((((packets[2] != '\x18') || (packets[9] != '\0')) ||
                          (packets[10] != '\0')) ||
                         ((((packets[0xb] != '\0' || (packets[0xc] != '\0')) ||
                            ((packets[0xd] != '\0' ||
                              ((packets[0xe] != '\0' || (packets[0xf] != '\0')))))) ||
                           (packets[0x10] != '\0')))) ||
                        (((((((packets[0x11] != '\0' || (packets[0x12] != '\0')) ||
                              (packets[0x13] != '\0')) ||
                             ((packets[0x14] != '\0' || (packets[0x15] != '\0')))) ||
                            (packets[0x16] != '\0')) ||
                           ((packets[0x17] != '\0' || (packets[0x18] != '\0')))) ||
                          ((packets[0x19] != '\0' ||
                            ((packets[0x1a] != '\0' || (packets[0x1b] != '\0'))))))))
                        goto reject_vsif_packet;
                } else {
                    if ((packet_identifier != 0xd046) || (packets[2] != '\x1b')) {
                    reject_vsif_packet:
                        commit_status = 0xfffffffb;
                        {
                            *status = commit_status;
                            return 0;
                        }
                    }
                    dovi_parse_vsif_from_PB4(packets + 7, decoded_header);
                }
            finish_packet_decoding:
                configuration_value = *(int *)((long)&work->metadata_sizes.pairs[0] + source_index * 4);
            select_decoded_metadata:
                if (configuration_value == 0) {
                    source_address = source_parameters + source_index * 0xa0;
                    has_metadata = 1;
                    commit_status = 0;
                    signal_word = *(unsigned int *)(source_address + 0x40);
                    work->metadata[source_index] = *(long *)(source_address + 0x38);
                    goto store_metadata_size;
                }
                commit_status = 0;
                has_metadata = 1;
            }
            source_address = source_parameters + source_index * 0xa0;
            *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xb4) =
                *(unsigned int *)(source_address + 4);
            *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x17) = *source_or_signal_words;
            bit_depth_slot = (int *)(source_address + 0x58);
            *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x1a) =
                (unsigned int)(*bit_depth_slot == 8);
            if (has_metadata) {
                if ((*input_mode_slot == 1) && (*decoded_header == 1)) {
                    source_address = source_parameters + source_index * 0xa0;
                    configuration_word = (unsigned int)(*(int *)(source_address + 0x60) != 0);
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xc4) = configuration_word;
                    configuration_value = (unsigned int)(*(int *)(source_address + 0x5c) != 0) << 1;
                    if (*(int *)(source_address + 0x5c) == 1) {
                        configuration_value = 1;
                    }
                    *(int *)(control + (unsigned long)source_number * 0x24 + 0x19) = configuration_value;
                    configuration_value = *(int *)(source_address + 0x5c);
                    configuration_result = 0;
                    if (configuration_value != 0) {
                        if (configuration_value == 1) {
                            configuration_result = 2;
                        } else {
                            configuration_result = (unsigned int)(configuration_value != 2);
                        }
                    }
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xcc) = configuration_result;
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x34) = 1;
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x2a) = configuration_word;
                    configuration_value = *bit_depth_slot;
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0x154) = 3;
                    *(int *)((long)control + (unsigned long)source_number * 0x120 + 0xd4) = configuration_value;
                    source_or_signal_words = (unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xe4);
                    configuration_word = decoded_header[9];
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0x10c) = 0;
                    if (configuration_word == 1) {
                        *source_or_signal_words = 0;
                        *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x1c) = 3;
                    } else {
                        *source_or_signal_words = 1;
                        control[(unsigned long)source_number * 0x24 + 0x1d] = control[0x294];
                        *(int *)(control + (unsigned long)source_number * 0x24 + 0x1e) =
                            (int)control[0x295];
                        fixed_store_le64((unsigned char *)control + (unsigned long)source_number * 0x120 +
                                             0xf4,
                                         fixed_load_le64((unsigned char *)control + 0x14ac));
                        sink_white_x_word = control[0x297];
                        *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xfc) =
                            *(unsigned int *)((long)control + 0x14b4);
                        *(int *)(control + (unsigned long)source_number * 0x24 + 0x20) = (int)sink_white_x_word;
                        *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0x104) =
                            *(unsigned int *)((long)control + 0x14bc);
                    }
                    sink_luminance_pair = fixed_signed64(fixed_load_le64((unsigned char *)control + 0x1494));
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x36) =
                        *(unsigned int *)((long)control + 0x149c);
                    control[(unsigned long)source_number * 0x24 + 0x35] = sink_luminance_pair;
                    configuration_value = *input_mode_slot;
                } else if (*(int *)(source_parameters + source_index * 0xa0 + 0x10) == 0) {
                reload_input_mode:
                    configuration_value = *input_mode_slot;
                } else {
                    source_address = source_parameters + source_index * 0xa0;
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0x10c) = 0;
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xe4) = 1;
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x1d) =
                        *(unsigned int *)(source_address + 0x14);
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xec) =
                        *(unsigned int *)(source_address + 0x18);
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x1e) =
                        *(unsigned int *)(source_address + 0x1c);
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xf4) =
                        *(unsigned int *)(source_address + 0x20);
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x1f) =
                        *(unsigned int *)(source_address + 0x24);
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xfc) =
                        *(unsigned int *)(source_address + 0x28);
                    *(unsigned int *)(control + (unsigned long)source_number * 0x24 + 0x20) =
                        *(unsigned int *)(source_address + 0x2c);
                    *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0x104) =
                        *(unsigned int *)(source_address + 0x30);
                    configuration_value = *input_mode_slot;
                }
                if (((configuration_value != 0) && (configuration_value != 2)) && ((configuration_value != 1 || (*decoded_header != 0))))
                    goto commit_without_metadata;
                if (*(int *)(source_parameters + source_index * 0xa0 + 0x44) == 0) {
                    dovi_dm_ParseMds(
                        (unsigned char *)work->metadata[source_index],
                        *(unsigned int *)((long)&work->metadata_sizes.pairs[0] + source_index * 4),
                        (unsigned int *)control[source_index + 1], source_number, input_count_slot, *control);
                    metadata_configuration = (unsigned int *)control[source_index + 1];
                } else {
                    metadata_configuration = (unsigned int *)0xffffffffffffffff;
                    control[source_index + 1] = -1;
                }
            } else {
                source_address = source_parameters + source_index * 0xa0;
                *(unsigned int *)((long)control + (unsigned long)source_number * 0x120 + 0xc4) =
                    (unsigned int)(*(int *)(source_address + 0x60) != 0);
                configuration_value = (unsigned int)(*(int *)(source_address + 0x5c) != 0) << 1;
                if (*(int *)(source_address + 0x5c) == 1) {
                    configuration_value = 1;
                }
                *(int *)(control + (unsigned long)source_number * 0x24 + 0x19) = configuration_value;
                configuration_value = *(int *)(source_address + 0x5c);
                signal_word = 0;
                if (configuration_value != 0) {
                    if (configuration_value == 1) {
                        signal_word = 2;
                    } else if (configuration_value != 2) {
                        signal_word = 1;
                    }
                }
                signal_number = (unsigned long)source_number;
                signal_offset = signal_number * 0x120;
                signal_source_address = source_parameters + source_index * 0xa0;
                *(unsigned int *)((long)control + signal_offset + 0xcc) = signal_word;
                signal_word = 3;
                if (*(int *)(signal_source_address + 0x6c) != 2) {
                    signal_word = 0;
                }
                if (*(int *)(signal_source_address + 0x6c) == 1) {
                    signal_word = 1;
                }
                *(unsigned int *)(control + signal_number * 0x24 + 0x34) = signal_word;
                *(unsigned int *)((long)control + signal_offset + 0x1a4) =
                    *(unsigned int *)(signal_source_address + 0x70);
                configuration_value = *(int *)(signal_source_address + 0x74);
                *(unsigned int *)((long)control + signal_offset + 0xe4) = 1;
                control[signal_number * 0x24 + 0x21] = 0x1a;
                *(unsigned int *)(control + signal_number * 0x24 + 0x2a) = (unsigned int)(configuration_value != 0);
                *(unsigned int *)(control + signal_number * 0x24 + 0x1d) =
                    *(unsigned int *)(signal_source_address + 0x14);
                *(unsigned int *)((long)control + signal_offset + 0xec) = *(unsigned int *)(signal_source_address + 0x18);
                *(unsigned int *)(control + signal_number * 0x24 + 0x1e) =
                    *(unsigned int *)(signal_source_address + 0x1c);
                *(unsigned int *)((long)control + signal_offset + 0xf4) = *(unsigned int *)(signal_source_address + 0x20);
                *(unsigned int *)(control + signal_number * 0x24 + 0x1f) =
                    *(unsigned int *)(signal_source_address + 0x24);
                *(unsigned int *)((long)control + signal_offset + 0xfc) = *(unsigned int *)(signal_source_address + 0x28);
                *(unsigned int *)(control + signal_number * 0x24 + 0x20) =
                    *(unsigned int *)(signal_source_address + 0x2c);
                signal_word = *(unsigned int *)(signal_source_address + 0x30);
                *(unsigned int *)(control + signal_number * 0x24 + 0x2b) = 1;
                *(unsigned int *)((long)control + signal_offset + 0x104) = signal_word;
                *(int *)(control + signal_number * 0x24 + 0x2e) = (int)*(short *)(signal_source_address + 0x98);
                *(unsigned short *)((long)control + signal_offset + 0x15c) =
                    *(unsigned short *)(signal_source_address + 0x78);
                *(unsigned short *)((long)control + signal_offset + 0x15e) =
                    *(unsigned short *)(signal_source_address + 0x7a);
                *(unsigned short *)(control + signal_number * 0x24 + 0x2c) =
                    *(unsigned short *)(signal_source_address + 0x7c);
                *(unsigned short *)((long)control + signal_offset + 0x162) =
                    *(unsigned short *)(signal_source_address + 0x7e);
                *(unsigned short *)((long)control + signal_offset + 0x164) =
                    *(unsigned short *)(signal_source_address + 0x80);
                *(unsigned short *)((long)control + signal_offset + 0x166) =
                    *(unsigned short *)(signal_source_address + 0x82);
                *(unsigned short *)(control + signal_number * 0x24 + 0x2d) =
                    *(unsigned short *)(signal_source_address + 0x84);
                *(unsigned short *)((long)control + signal_offset + 0x16a) =
                    *(unsigned short *)(signal_source_address + 0x86);
                matrix_last16 = *(unsigned short *)(signal_source_address + 0x88);
                *(unsigned int *)((long)control + signal_offset + 0x174) = 1;
                *(unsigned int *)(control + signal_number * 0x24 + 0x32) = 0;
                *(unsigned short *)((long)control + signal_offset + 0x16c) = matrix_last16;
                *(unsigned int *)(control + signal_number * 0x24 + 0x2f) =
                    *(unsigned int *)(signal_source_address + 0x8c);
                *(unsigned int *)((long)control + signal_offset + 0x17c) =
                    *(unsigned int *)(signal_source_address + 0x90);
                *(unsigned int *)(control + signal_number * 0x24 + 0x30) =
                    *(unsigned int *)(signal_source_address + 0x94);
                *(int *)((long)control + signal_offset + 0xd4) = *bit_depth_slot;
                configuration_word = *(unsigned int *)(signal_source_address + 100);
                *(unsigned int *)(control + signal_number * 0x24 + 0x35) = configuration_word;
                *(unsigned int *)((long)control + signal_offset + 0x1ac) =
                    *(unsigned int *)(signal_source_address + 0x68);
                configuration_result = dovi_LToPQ12(configuration_word);
                configuration_word = *(unsigned int *)((long)control + signal_offset + 0x1ac);
                *(short *)(control + signal_number * 0x24 + 0x36) = (short)configuration_result;
                configuration_word = dovi_LToPQ12(configuration_word);
                *(short *)((long)control + signal_offset + 0x1b2) = (short)configuration_word;
                *(int *)(control + (unsigned long)source_number + 0x11b) = *(int *)(signal_source_address + 0x9c) << 7;
                if (0xc < source_index << 2) {
                trap_source_bounds:
                    /* WARNING: Does not return */
                    dm_limit_trap();
                }
                if (*source_status_slot == 0)
                    goto reload_input_mode;
            commit_without_metadata:
                metadata_configuration = (unsigned int *)0x0;
            }
            configuration_result = dovi_dm_Commit2DmKs(source_number, input_count_slot, metadata_configuration, (unsigned int *)*control);
            configuration_word = 0;
            if (source_index32 != primary_input_index) {
                configuration_word = source_number;
            }
            commit_status = configuration_result | commit_status;
            if (configuration_word == primary_input_index) {
                configuration_word = configuration_word + 1;
            }
            source_index = (unsigned long)configuration_word;
        } while ((int)configuration_word < (int)(unsigned int)input_count);
    }
    *status = commit_status;
    return 1;
}

static void update_commit_source_flags(long *control, long source_parameters,
                                       unsigned long primary_input, const unsigned char *vsvdb,
                                       struct dovi_commit_workspace *work)
{
    unsigned int extension_length_pairs;
    int input_mode;
    int source_transfer;
    unsigned char auxiliary_byte2;
    unsigned int source_signal_code;
    long metadata_address;
    const unsigned char *last_auxiliary_extension;
    unsigned int extensions_remaining;
    const unsigned char *selected_auxiliary_extension;
    const unsigned char *extension;
    int *decoded_header;
    if (*(int *)((long)&work->source_flags.pairs[0] + primary_input * 4) == 0) {
        *(unsigned int *)((long)control + 0xd24) = 0;
        if (vsvdb != (unsigned char *)0x0) {
            *(unsigned int *)((long)control + 0x1464) = 0;
            fixed_store_le32((unsigned char *)control + 0x1467, 0);
            input_mode = *(int *)(source_parameters + primary_input * 0xa0 + 0xc);
            if (input_mode == 0) {
            scan_auxiliary_extensions:
                metadata_address = work->metadata[primary_input];
                if ((metadata_address != 0) && (extensions_remaining = (unsigned int)*(unsigned char *)(metadata_address + 0x46),
                                      *(unsigned char *)(metadata_address + 0x46) != 0)) {
                    extension = (const unsigned char *)(metadata_address + 0x47);
                    last_auxiliary_extension = (unsigned char *)0x0;
                    do {
                        extension_length_pairs = (fixed_load_le32(extension) & 0xff00ff00) >> 8 |
                                (fixed_load_le32(extension) & 0xff00ff) << 8;
                        selected_auxiliary_extension = extension + 5;
                        if ((signed char)fixed_load_le32(extension + 4) != -1) {
                            selected_auxiliary_extension = last_auxiliary_extension;
                        }
                        extension = extension + 5 + (extension_length_pairs >> 0x10 | extension_length_pairs << 0x10);
                        extensions_remaining = extensions_remaining - 1;
                        last_auxiliary_extension = selected_auxiliary_extension;
                    } while (extensions_remaining != 0);
                    if (selected_auxiliary_extension != (unsigned char *)0x0) {
                        *(unsigned int *)((long)control + 0x1464) = 1;
                        *(unsigned char *)(control + 0x28d) = *selected_auxiliary_extension;
                        *(unsigned char *)((long)control + 0x1469) = selected_auxiliary_extension[1];
                        auxiliary_byte2 = selected_auxiliary_extension[2];
                        goto store_auxiliary_byte2;
                    }
                }
            } else if (input_mode == 1) {
                decoded_header = (int *)work->decoded_headers[primary_input];
                if (*decoded_header == 0)
                    goto scan_auxiliary_extensions;
                if (decoded_header[6] == 0)
                    return;
                *(unsigned int *)((long)control + 0x1464) = 1;
                *(char *)(control + 0x28d) = (signed char)decoded_header[7];
                *(unsigned char *)((long)control + 0x1469) =
                    *(unsigned char *)((long)decoded_header + 0x1d);
                auxiliary_byte2 = *(unsigned char *)((long)decoded_header + 0x1e);
            store_auxiliary_byte2:
                *(unsigned char *)((long)control + 0x146a) = auxiliary_byte2;
            }
        }
    } else {
        source_transfer = *(int *)(source_parameters + primary_input * 0xa0 + 0x6c);
        if (source_transfer == 2) {
            source_signal_code = 4;
        } else {
            source_signal_code = 1;
            if (source_transfer != 1) {
                if (*(int *)(source_parameters + primary_input * 0xa0 + 0x58) == 8) {
                    source_signal_code = 2;
                } else {
                    source_signal_code = 8;
                }
            }
        }
        *(unsigned int *)((long)control + 0xd24) = source_signal_code;
    }
}

static void pack_commit_input_registers(long *control, long source_parameters,
                                        unsigned char input_count, long core1_registers,
                                        long core2_registers, long display_luts,
                                        struct dovi_commit_workspace *work)
{
    long last_pivot_address;
    unsigned short matrix_tail_high;
    unsigned short matrix_tail_low;
    int core1_slot;
    unsigned int *source_words;
    unsigned long source_index;
    long register_address;
    unsigned int input_mode;
    unsigned int pivot_index;
    unsigned int fraction_mask32;
    unsigned int graphics_flags;
    unsigned int packed_output_word;
    long *composer_configuration;
    unsigned int *input_mode_slot;
    long channel_address;
    unsigned long coefficient_denom;
    long composer_address;
    long source_address;
    long kernel_address;
    unsigned long nlq_coefficient;
    unsigned long nlq_coefficient_u;
    int pivot_value;
    int source_flag;
    long pivot_count_offset;
    unsigned long integer_mask64;
    int core2_slot;
    if (input_count != 0) {
        source_index = 0;
        core2_slot = 0;
        core1_slot = 0;
        do {
            source_words = (unsigned int *)(source_parameters + source_index * 0xa0);
            register_address = core1_registers + (long)core1_slot * 0x3c8;
            input_mode_slot = source_words + 3;
            input_mode = *input_mode_slot;
            if (input_mode == 1) {
                control[0x1b7] = 0;
                fixed_store_le64((unsigned char *)control + 0xe24, 0x100000000);
                fixed_store_le64((unsigned char *)control + 0xd4c, 0x3ff00000000);
                control[0x1ae] = 0x3ff00000000;
                *(unsigned int *)((long)control + 0xde4) = 1;
                control[0x1a6] = 0xa0000000c;
                fixed_store_le64((unsigned char *)control + 0xf44, 0x180600000000);
                control[0x1b8] = 0x100000000;
                control[0x1a7] = 0x1700000000;
                fixed_store_le64((unsigned char *)control + 0xe84, 0x100000000);
                *(unsigned int *)((long)control + 0xe04) = 1;
                fixed_store_le16((unsigned char *)control + 0x11cd, 1);
                fixed_store_le64((unsigned char *)control + 0xfa4, 0x180600000000);
                *(unsigned int *)((long)control + 0xee4) = 0;
                control[0x1a8] = 0x200000002;
                fixed_store_le64((unsigned char *)control + 0xd94, 0x3ff00000000);
                *(unsigned int *)((long)control + 0x1004) = 0;
                *(unsigned int *)(control + 0x1a9) = 2;
            select_synthetic_composer:
                *(unsigned int *)(control + 0x1dd) = 1;
                *(unsigned int *)(control + 0x201) = 0x1806;
                composer_configuration = control + 0x1a6;
            encode_selected_composer:
            {
                const unsigned char *source_dimensions =
                    (const unsigned char *)(source_parameters + source_index * 0xa0);
                unsigned int width = fixed_load_le32(source_dimensions);
                unsigned int height = fixed_load_le32(source_dimensions + 4);
                (void)dovi_encode_composer_registers(composer_configuration, (void *)register_address, width, height);
            }
                input_mode = *input_mode_slot;
            } else if (input_mode == 0) {
                if (source_index < 4) {
                    if (*(int *)((long)&work->source_flags.pairs[0] + source_index * 4) != 0) {
                        fixed_store_le64((unsigned char *)control + 0xe24, 0x100000000);
                        fixed_store_le64((unsigned char *)control + 0xd4c, 0x3ff00000000);
                        control[0x1b7] = 0;
                        control[0x1ae] = 0x3ff00000000;
                        control[0x1a6] = 0xa0000000c;
                        fixed_store_le64((unsigned char *)control + 0xf44, 0x180600000000);
                        *(unsigned int *)((long)control + 0xde4) = 1;
                        control[0x1a7] = 0x1700000000;
                        fixed_store_le64((unsigned char *)control + 0xe84, 0x100000000);
                        control[0x1b8] = 0x100000000;
                        fixed_store_le16((unsigned char *)control + 0x11cd, 1);
                        fixed_store_le64((unsigned char *)control + 0xfa4, 0x180600000000);
                        *(unsigned int *)((long)control + 0xe04) = 1;
                        control[0x1a8] = 0x200000002;
                        fixed_store_le64((unsigned char *)control + 0xd94, 0x3ff00000000);
                        *(unsigned int *)((long)control + 0xee4) = 0;
                        *(unsigned int *)(control + 0x1a9) = 2;
                        *(unsigned int *)((long)control + 0x1004) = 0;
                        goto select_synthetic_composer;
                    }
                    if (*(int *)control[0x1a3] != 0) {
                        composer_address = *(long *)(source_parameters + source_index * 0xa0 + 0x48);
                        if (*(int *)(composer_address + 4) == 8) {
                            pivot_count_offset = 0x10;
                            *(unsigned int *)(composer_address + 4) = 10;
                            channel_address = composer_address;
                            do {
                                pivot_index = *(unsigned int *)(composer_address + pivot_count_offset);
                                if (pivot_index == 0)
                                    dm_limit_trap();
                                pivot_value = *(int *)(channel_address + 0x1c) << 2;
                                *(int *)(channel_address + 0x1c) = pivot_value;
                                if (pivot_index == 1) {
                                    pivot_index = 0;
                                } else {
                                    pivot_value = *(int *)(channel_address + 0x20) << 2;
                                    *(int *)(channel_address + 0x20) = pivot_value;
                                    if (pivot_index < 3) {
                                        pivot_index = 1;
                                    } else {
                                        pivot_value = *(int *)(channel_address + 0x24) << 2;
                                        *(int *)(channel_address + 0x24) = pivot_value;
                                        if (pivot_index == 3) {
                                            pivot_index = 2;
                                        } else {
                                            pivot_value = *(int *)(channel_address + 0x28) << 2;
                                            *(int *)(channel_address + 0x28) = pivot_value;
                                            if (pivot_index < 5) {
                                                pivot_index = 3;
                                            } else {
                                                pivot_value = *(int *)(channel_address + 0x2c) << 2;
                                                *(int *)(channel_address + 0x2c) = pivot_value;
                                                if (pivot_index == 5) {
                                                    pivot_index = 4;
                                                } else {
                                                    *(int *)(channel_address + 0x30) =
                                                        *(int *)(channel_address + 0x30) << 2;
                                                    pivot_index = *(unsigned int *)(composer_address + pivot_count_offset);
                                                    if (pivot_index < 7) {
                                                        pivot_index = pivot_index - 1;
                                                        if (8 < pivot_index)
                                                            dm_limit_trap();
                                                        pivot_value =
                                                            *(int *)(channel_address +
                                                                     (unsigned long)pivot_index * 4 +
                                                                     0x1c);
                                                    } else {
                                                        pivot_value = *(int *)(channel_address + 0x34) << 2;
                                                        *(int *)(channel_address + 0x34) = pivot_value;
                                                        if (pivot_index == 7) {
                                                            pivot_index = 6;
                                                        } else {
                                                            pivot_value = *(int *)(channel_address + 0x38) << 2;
                                                            *(int *)(channel_address + 0x38) = pivot_value;
                                                            if (pivot_index < 9) {
                                                                pivot_index = 7;
                                                            } else {
                                                                pivot_value = *(int *)(channel_address + 0x3c)
                                                                         << 2;
                                                                *(int *)(channel_address + 0x3c) = pivot_value;
                                                                if (pivot_index != 9)
                                                                    dm_limit_trap();
                                                                pivot_index = 8;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                last_pivot_address = channel_address + (unsigned long)pivot_index * 4;
                                pivot_count_offset = pivot_count_offset + 4;
                                channel_address = channel_address + 0x24;
                                *(int *)(last_pivot_address + 0x1c) = pivot_value + 3;
                            } while (pivot_count_offset != 0x1c);
                        }
                        if (*(int *)(composer_address + 8) == 8) {
                            coefficient_denom = (unsigned long)*(unsigned int *)(composer_address + 0xc);
                            integer_mask64 = ~0ul << (coefficient_denom & 0x3f);
                            fraction_mask32 = ~0u << (*(unsigned int *)(composer_address + 0xc) & 0x1f);
                            nlq_coefficient = ((long)*(int *)(composer_address + 0x4ac) << (coefficient_denom & 0x3f) |
                                      (unsigned long)*(unsigned int *)(composer_address + 0x4d0)) >>
                                     2;
                            *(int *)(composer_address + 0x4a0) = *(int *)(composer_address + 0x4a0) << 2;
                            *(unsigned int *)(composer_address + 0x4d0) =
                                (unsigned int)nlq_coefficient & (fraction_mask32 ^ 0xffffffff);
                            nlq_coefficient_u = ((long)*(int *)(composer_address + 0x4b8) << (coefficient_denom & 0x3f) |
                                      (unsigned long)*(unsigned int *)(composer_address + 0x4dc)) >>
                                     2;
                            *(int *)(composer_address + 0x4ac) = (int)((nlq_coefficient & integer_mask64) >> (coefficient_denom & 0x3f));
                            *(int *)(composer_address + 0x4a4) = *(int *)(composer_address + 0x4a4) << 2;
                            *(int *)(composer_address + 0x4b8) = (int)((nlq_coefficient_u & integer_mask64) >> (coefficient_denom & 0x3f));
                            nlq_coefficient = ((long)*(int *)(composer_address + 0x4c4) << (coefficient_denom & 0x3f) |
                                      (unsigned long)*(unsigned int *)(composer_address + 0x4e8)) >>
                                     2;
                            *(unsigned int *)(composer_address + 0x4dc) =
                                (unsigned int)nlq_coefficient_u & (fraction_mask32 ^ 0xffffffff);
                            *(int *)(composer_address + 0x4a8) = *(int *)(composer_address + 0x4a8) << 2;
                            *(unsigned int *)(composer_address + 8) = 10;
                            *(int *)(composer_address + 0x4c4) = (int)((nlq_coefficient & integer_mask64) >> (coefficient_denom & 0x3f));
                            *(unsigned int *)(composer_address + 0x4e8) =
                                (unsigned int)nlq_coefficient & (fraction_mask32 ^ 0xffffffff);
                        }
                    }
                    composer_configuration = *(long **)(source_parameters + source_index * 0xa0 + 0x48);
                    goto encode_selected_composer;
                }
                dm_limit_trap();
            }
            if (input_mode == 2) {
                work->temporary.primaries[2] = 0;
                work->temporary.primaries[3] = 0;
                work->temporary.primaries[0] = 0;
                work->temporary.primaries[1] = 0;
                dovi_GetPrimaries(1, &work->temporary.primaries[0]);
                if (*(int *)(source_parameters + source_index * 0xa0 + 0x68) == 0x1900000) {
                    if ((((*(int *)(source_parameters + source_index * 0xa0 + 0x14) ==
                           (int)work->temporary.primaries[0]) &&
                          (*(unsigned int *)(source_parameters + source_index * 0xa0 + 0x18) ==
                           (*(unsigned int *)((unsigned char *)&work->temporary.primaries[0] +
                                              4)))) &&
                         (*(int *)(source_parameters + source_index * 0xa0 + 0x1c) ==
                          (int)work->temporary.primaries[1])) &&
                        (((*(unsigned int *)(source_parameters + source_index * 0xa0 + 0x20) ==
                               (*(unsigned int *)((unsigned char *)&work->temporary.primaries[1] +
                                                  4)) &&
                           (*(int *)(source_parameters + source_index * 0xa0 + 0x24) ==
                            (int)work->temporary.primaries[2])) &&
                          ((*(unsigned int *)(source_parameters + source_index * 0xa0 + 0x28) ==
                                (*(unsigned int *)((unsigned char *)&work->temporary.primaries[2] +
                                                   4)) &&
                            (*(int *)(source_parameters + source_index * 0xa0 + 0x2c) ==
                             (int)work->temporary.primaries[3])))))) {
                        graphics_flags = (unsigned int)(*(unsigned int *)(source_parameters +
                                                                  source_index * 0xa0 + 0x30) ==
                                                (*(unsigned int *)((unsigned char *)&work->temporary
                                                                       .primaries[3] +
                                                                   4)))
                                 << 4;
                    } else {
                        graphics_flags = 0;
                    }
                } else {
                    graphics_flags = 0;
                }
                if (3 < source_index)
                    dm_limit_trap();
                source_address = source_parameters + source_index * 0xa0;
                kernel_address = control[source_index + 5];
                register_address = core2_registers + (long)core2_slot * 0xac;
                core2_slot = core2_slot + 1;
                *(unsigned int *)(register_address + 4) =
                    graphics_flags | (unsigned int)(*(int *)(source_address + 0x60) == 0) << 1 |
                    (unsigned int)((*(unsigned int *)(source_address + 0x5c) & 0xfffffffe) != 2) << 2;
                *(unsigned int *)(register_address + 0x18) = CONCAT22(*(unsigned short *)(kernel_address + 0xfc),
                                                            *(unsigned short *)(kernel_address + 0xfe));
                *(unsigned int *)(register_address + 0x1c) = *(unsigned int *)(kernel_address + 0x100);
                *(unsigned int *)(register_address + 0x20) = *(unsigned int *)(kernel_address + 0xdc);
                *(unsigned int *)(register_address + 0x24) = *(unsigned int *)(kernel_address + 0xe0);
                *(unsigned int *)(register_address + 0x28) = *(unsigned int *)(kernel_address + 0xe4);
                *(unsigned int *)(register_address + 0x2c) = *(unsigned int *)(kernel_address + 0xe8);
                *(unsigned int *)(register_address + 0x30) = CONCAT22(*(unsigned short *)(kernel_address + 0xda),
                                                            *(unsigned short *)(kernel_address + 0xec));
                *(unsigned int *)(register_address + 0x34) = *(unsigned int *)(kernel_address + 0xf0);
                *(unsigned int *)(register_address + 0x38) = *(unsigned int *)(kernel_address + 0xf4);
                *(unsigned int *)(register_address + 0x3c) = *(unsigned int *)(kernel_address + 0xf8);
                *(unsigned int *)(register_address + 0x40) = 0;
                *(unsigned int *)(register_address + 0x44) = (unsigned int)*(unsigned char *)(kernel_address + 0x106);
                *(unsigned int *)(register_address + 0x48) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x50a);
                *(unsigned int *)(register_address + 0x4c) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x50e);
                *(unsigned int *)(register_address + 0x50) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x512);
                *(unsigned int *)(register_address + 0x54) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x516);
                *(unsigned int *)(register_address + 0x58) = CONCAT22(*(unsigned short *)(kernel_address + 0x508),
                                                            *(unsigned short *)(kernel_address + 0x51a));
                *(unsigned int *)(register_address + 0x5c) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x51e);
                *(unsigned int *)(register_address + 0x60) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x522);
                *(unsigned int *)(register_address + 100) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x526);
                *(unsigned int *)(register_address + 0x68) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x52a);
                *(unsigned int *)(register_address + 0x6c) = CONCAT22(*(unsigned short *)(kernel_address + 0x51c),
                                                            *(unsigned short *)(kernel_address + 0x52e));
                *(unsigned int *)(register_address + 0x70) = 0;
                packed_output_word = *(unsigned int *)(kernel_address + 0xc0) | *(int *)(kernel_address + 0xbc) << 0xd;
            } else {
                if (3 < source_index)
                    dm_limit_trap();
                core1_slot = core1_slot + 1;
                source_flag = *(int *)((long)&work->source_flags.pairs[0] + source_index * 4);
                kernel_address = control[source_index + 5];
                *(unsigned int *)(register_address + 4) = 0;
                *(unsigned int *)(register_address + 4) = (unsigned int)(input_mode == 1 || source_flag != 0) |
                                                (unsigned int)(*(char *)(kernel_address + 0xd8) == '\x03')
                                                    << 1;
                *(unsigned int *)(register_address + 0x18) = CONCAT22(*(unsigned short *)(kernel_address + 0xfc),
                                                            *(unsigned short *)(kernel_address + 0xfe));
                *(unsigned int *)(register_address + 0x1c) = *(unsigned int *)(kernel_address + 0x100);
                *(unsigned int *)(register_address + 0x20) = *(unsigned int *)(kernel_address + 0x530);
                *(unsigned int *)(register_address + 0x24) =
                    *(unsigned int *)(kernel_address + 0x534) | *(int *)(kernel_address + 0x538) << 0x10;
                *(unsigned int *)(register_address + 0x28) = *(unsigned int *)(kernel_address + 0x53c);
                *(unsigned int *)(register_address + 0x2c) = *(unsigned int *)(kernel_address + 0xdc);
                *(unsigned int *)(register_address + 0x30) = *(unsigned int *)(kernel_address + 0xe0);
                *(unsigned int *)(register_address + 0x34) = *(unsigned int *)(kernel_address + 0xe4);
                *(unsigned int *)(register_address + 0x38) = *(unsigned int *)(kernel_address + 0xe8);
                *(unsigned int *)(register_address + 0x3c) = CONCAT22(*(unsigned short *)(kernel_address + 0xda),
                                                            *(unsigned short *)(kernel_address + 0xec));
                *(unsigned int *)(register_address + 0x40) = *(unsigned int *)(kernel_address + 0xf0);
                *(unsigned int *)(register_address + 0x44) = *(unsigned int *)(kernel_address + 0xf4);
                *(unsigned int *)(register_address + 0x48) = *(unsigned int *)(kernel_address + 0xf8);
                *(unsigned int *)(register_address + 0x4c) = (unsigned int)*(unsigned char *)(kernel_address + 0x106);
                *(unsigned int *)(register_address + 0x50) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x50a);
                *(unsigned int *)(register_address + 0x54) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x50e);
                *(unsigned int *)(register_address + 0x58) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x512);
                *(unsigned int *)(register_address + 0x5c) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x516);
                *(unsigned int *)(register_address + 0x60) = CONCAT22(*(unsigned short *)(kernel_address + 0x508),
                                                            *(unsigned short *)(kernel_address + 0x51a));
                *(unsigned int *)(register_address + 100) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x51e);
                *(unsigned int *)(register_address + 0x68) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x522);
                *(unsigned int *)(register_address + 0x6c) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x526);
                *(unsigned int *)(register_address + 0x70) =
                    fixed_load_le32((const unsigned char *)kernel_address + 0x52a);
                matrix_tail_low = *(unsigned short *)(kernel_address + 0x52e);
                matrix_tail_high = *(unsigned short *)(kernel_address + 0x51c);
                *(unsigned long *)(register_address + 0x78) = 0;
                *(unsigned int *)(register_address + 0x80) = 0;
                packed_output_word = CONCAT22(matrix_tail_high, matrix_tail_low);
            }
            *(unsigned int *)(register_address + 0x74) = packed_output_word;
            dovi_encode_display_luts((void *)display_luts,
                                     (const unsigned char *)control[source_index + 5] + 0x540,
                                     (const unsigned char *)control[source_index + 5] + 0xd8);
            source_index = source_index + 1;
            display_luts = display_luts + 0x1400;
        } while (source_index != input_count);
    }
}

int dovi_commit_reg(void *context, const unsigned char *input_configuration, void *sources,
               const int *target_parameters, const unsigned char *vsvdb,
               void *register1, void *register2, void *register3, void *luts)

{
    long *control = context;
    long source_parameters = (long)sources;
    long core1_registers = (long)register1;
    long core2_registers = (long)register2;
    unsigned long core3_registers = (unsigned long)register3;
    long display_luts = (long)luts;
    _Alignas(16) struct dovi_commit_workspace scratch;
    unsigned char primary_input_byte;
    unsigned char input_count;
    unsigned int status;
    int metadata_size;
    unsigned long *metadata_payload;
    unsigned int *input_count_slot;
    unsigned long primary_input;
    primary_input_byte = input_configuration[1];
    primary_input = (unsigned long)primary_input_byte;
    scratch.metadata_sizes.pairs[0] = 0;
    scratch.metadata_sizes.pairs[1] = 0;
    scratch.metadata[2] = 0;
    scratch.metadata[3] = 0;
    scratch.source_flags.pairs[0] = 0xffffffff;
    scratch.source_flags.pairs[1] = 0;
    input_count = *input_configuration;
    scratch.metadata[0] = 0;
    scratch.metadata[1] = 0;
    scratch.metadata_configuration[18] = 0;
    scratch.metadata_configuration[19] = 0;
    input_count_slot = (unsigned int *)(control + 9);
    *input_count_slot = (unsigned int)input_count;
    scratch.metadata_configuration[16] = 0;
    scratch.metadata_configuration[17] = 0;
    scratch.metadata_configuration[14] = 0;
    scratch.metadata_configuration[15] = 0;
    scratch.metadata_configuration[12] = 0;
    scratch.metadata_configuration[13] = 0;
    scratch.metadata_configuration[10] = 0;
    scratch.metadata_configuration[11] = 0;
    scratch.metadata_configuration[8] = 0;
    scratch.metadata_configuration[9] = 0;
    scratch.metadata_configuration[6] = 0;
    scratch.metadata_configuration[7] = 0;
    scratch.metadata_configuration[4] = 0;
    scratch.metadata_configuration[5] = 0;
    scratch.metadata_configuration[2] = 0;
    scratch.metadata_configuration[3] = 0;
    scratch.metadata_configuration[0] = 0;
    scratch.metadata_configuration[1] = 0;
    scratch.decoded_headers[3] = 0;
    scratch.decoded_headers[2] = 0;
    scratch.decoded_headers[1] = 0;
    scratch.decoded_headers[0] = 0;
    control[0x28e] = (long)target_parameters;
    *(unsigned int *)((long)control + 0x4c) = primary_input_byte + 1;
    *(unsigned int *)((long)control + 0x54) = (unsigned int)(input_count != 1);
    status = dovi_decode_display_block(control, vsvdb);
    if (status != 0)
        goto return_status;
    dovi_configure_output_target(control, input_configuration, target_parameters, vsvdb);
    if (!prepare_commit_sources(control, source_parameters, input_count, primary_input, input_count_slot,
                                &scratch, &status))
        return status;
    dovi_dm_CommitFinalized((int *)input_count_slot, *control);
    if (3 < (unsigned int)primary_input) {
        /* WARNING: Does not return */
        dm_limit_trap();
    }
    update_commit_source_flags(control, source_parameters, primary_input, vsvdb, &scratch);
    pack_commit_input_registers(control, source_parameters, input_count, core1_registers,
                                core2_registers, display_luts, &scratch);
    if (vsvdb == (unsigned char *)0x0) {
        metadata_payload = (unsigned long *)0x0;
        metadata_size = 0;
    } else {
        memset(&scratch.temporary.primaries[0], 0, 0x1fd);
        metadata_size = dovi_dm_GetMdsOutput((unsigned char *)&scratch.temporary.primaries[0], 0x1fd, 0, 0, 1,
                              (void *)*control);
        metadata_payload = &scratch.temporary.primaries[0];
    }
    dmKs2dmreg_core3(control, control[primary_input + 5], core3_registers, metadata_payload, metadata_size);
    dovi_notify_content_info(control, sources, primary_input,
                             (int *)&scratch.source_flags.pairs[0], scratch.metadata,
                             scratch.decoded_headers);
return_status:
    return status;
}
