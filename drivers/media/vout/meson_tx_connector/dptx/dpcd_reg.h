/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _DPCD_REG_H_
#define _DPCD_REG_H_

#include <linux/types.h>

#define EDID_LENGTH                            128
#define DDC_ADDR                               0x50
#define DDC_SEG_PTR                            0x30

#define DP_LINK_STATUS_SIZE                    6
#define DP_AUX_MAX_PAYLOAD_BYTES               16

#define DP_AUX_I2C_WRITE                       0x0
#define DP_AUX_I2C_READ                        0x1
#define DP_AUX_I2C_WRITE_STATUS_UPDATE         0x2
#define DP_AUX_I2C_MOT                         0x4
#define DP_AUX_NATIVE_WRITE                    0x8
#define DP_AUX_NATIVE_READ                     0x9

#define DP_AUX_NATIVE_REPLY_ACK                (0x0 << 0)
#define DP_AUX_NATIVE_REPLY_NACK               (0x1 << 0)
#define DP_AUX_NATIVE_REPLY_DEFER              (0x2 << 0)
#define DP_AUX_NATIVE_REPLY_MASK               (0x3 << 0)

#define DP_AUX_I2C_REPLY_ACK                   (0x0 << 2)
#define DP_AUX_I2C_REPLY_NACK                  (0x1 << 2)
#define DP_AUX_I2C_REPLY_DEFER                 (0x2 << 2)
#define DP_AUX_I2C_REPLY_MASK                  (0x3 << 2)

/* DPCD Field Address Mapping */

/* Receiver Capability */
/* 00000h DPCD_REV
 *    7:4 Major Revision Number
 *        10h = DPCD r1.0.
 *        11h = DPCD r1.1.
 *        12h = DPCD r1.2.
 *        13h = DPCD r1.3 (for eDP v1.4 DPRX only).
 *        14h = DPCD r1.4
 */
#define DP_DPCD_REV                            0x000
#define DP_DPCD_REV_10                         0x10
#define DP_DPCD_REV_11                         0x11
#define DP_DPCD_REV_12                         0x12
#define DP_DPCD_REV_13                         0x13
#define DP_DPCD_REV_14                         0x14

/* 00001h MAX_LINK_RATE
 *    7:0 Only four values are supported.
 *        06h = 1.62Gbps/lane.
 *        0Ah = 2.7Gbps/lane.
 *        14h = 5.4Gbps/lane.
 *        1Eh = 8.1Gbps/lane.
 */
#define DP_MAX_LINK_RATE                       0x001

/* 00002h MAX_LANE_COUNT
 *    4:0 Only three values are supported.
 *        1h = One lane (Lane 0 only).
 *        2h = Two lanes (Lanes 0 and 1 only).
 *        4h = Four lanes (Lanes 0, 1, 2, and 3).
 */
#define DP_MAX_LANE_COUNT                      0x002
#define DP_MAX_LANE_COUNT_MASK                 0x1f
#define DP_TPS3_SUPPORTED                      BIT(6) /* 1.2 */
#define DP_ENHANCED_FRAME_CAP                  BIT(7)

/* 00003h MAX_DOWNSPREAD
 *      0 MAX_DOWNSPREAD
 *          0 = No down-spread  1 = Up to 0.5% down-spread
 *      6 NO_AUX_TRANSACTION_LINK_TRAINING
 *          0 = DPTX shall issue AUX transactions to conduct Link Training
 *          1 = AUX transactions are not needed when the link configuration is already known
 *      7 TPS4_SUPPORTED   Link Training Pattern Sequence 4 (TPS4)
 *          0 = TPS4 is not supported
 *          1 = TPS4 is supported
 */
#define DP_MAX_DOWNSPREAD                      0x003
#define DP_SUP_MAX_DOWNSPREAD                  BIT(0)
#define DP_STREAM_REGENERATION_STATUS_CAP      BIT(1) /* 2.0 */
#define DP_NO_AUX_HANDSHAKE_LINK_TRAINING      BIT(6)
#define DP_TPS4_SUPPORTED                      BIT(7)

/* 00004h NORP & DP_PWR_VOLTAGE_CAP
 *      0 NORP Number of Receiver Ports
 *          0 = One receiver port
 *          1 = Two or more receiver ports
 *      5 5V_DP_PWR_CAP
 *      6 12V_DP_PWR_CAP
 *      7 18V_DP_PWR_CAP
 */
#define DP_NORP                                0x004

/* 00005h DOWN_STREAM_PORT_PRESENT
 *      0 DFP_PRESENT
 *          1 = Device is a Branch device and has downstream-facing port
 *    2:1 DFP_TYPE
 *          00 = DisplayPort.
 *          01 = Analog VGA or analog video over DVI-I.
 *          10 = DVI, HDMI, or DP++.
 *          11 = Others
 *      3 FORMAT_CONVERSION
 *          0 = This Branch device does not have a format conversion block
 *          1 = This DFP has a format conversion block
 *      4 DETAILED_CAP_INFO_AVAILABLE
 *          0 = DFP capability field is 1 byte per port
 *          1 = DFP capability field is 4 bytes per port
 */
#define DP_DOWNSTREAMPORT_PRESENT              0x005
#define DP_DWN_STRM_PORT_PRESENT               BIT(0)
#define DP_DWN_STRM_PORT_TYPE_MASK             0x06
#define DP_DWN_STRM_PORT_TYPE_DP               (0 << 1)
#define DP_DWN_STRM_PORT_TYPE_ANALOG           BIT(1)
#define DP_DWN_STRM_PORT_TYPE_TMDS             (2 << 1)
#define DP_DWN_STRM_PORT_TYPE_OTHER            (3 << 1)
#define DP_FORMAT_CONVERSION                   BIT(3)
#define DP_DETAILED_CAP_INFO_AVAILABLE         BIT(4) /* DPI */

/* 00006h MAIN_LINK_CHANNEL_CODING
 *      0 8b/10b
 *          1 = DisplayPort receiver supports the Main-Link channel coding
 *      1 128b/132b
 */
#define DP_MAIN_LINK_CHANNEL_CODING            0x006
#define DP_CAP_ANSI_8B10B                      BIT(0)
#define DP_CAP_ANSI_128B132B                   BIT(1) /* 2.0 */

/* 00007h DOWN_STREAM_PORT_COUNT
 *    3:0 DFP_COUNT
 *      6 MSA_TIMING_PAR_IGNORED
 *          0 = Sink device needs MSA timing parameters to be sent by the Source
 *              device for rendering the incoming video stream
 *          1 = Sink device is capable of rendering the incoming video stream
 *              without the above-mentioned MSA timing parameters
 *      7 OUI Support
 *          0 = OUI is not supported
 *          1 = OUI is supported
 */
#define DP_DOWN_STREAM_PORT_COUNT              0x007
#define DP_PORT_COUNT_MASK                     0x0f
#define DP_MSA_TIMING_PAR_IGNORED              BIT(6) /* eDP */
#define DP_OUI_SUPPORT                         BIT(7)

/* 00008h RECEIVE_PORT0_CAP_0
 *      1 LOCAL_EDID_PRESENT
 *          0 = This receiver port does not have a local EDID.
 *          1 = This receiver port has a local EDID
 *      2 ASSOCIATED_TO_PRECEDING_PORT
 *          0 = This port is used for the main isochronous stream. This bit shall
 *              always be cleared to 0 for Receiver Port 0.
 *          1 = This port is used for the secondary isochronous stream of the main
 *              stream received in the preceding port.
 *      3 HBLANK_EXPANSION_CAPABLE
 *          0 = DPRX is not capable of Horizontal Blanking Expansion operation.
 *          1 = DPRX is capable of Horizontal Blanking Expansion operation.
 *      4 BUFFER_SIZE_UNIT
 *          0 = Units are in pixel counts.
 *          1 = Units are in byte counts.
 *          Buffer_Size_in_Pixels = Buffer_Size_in_Bytes/Pixel_Depth_in_Bytes
 *      5 BUFFER_SIZE_PER_PORT
 *          0 = Buffer size is per-lane.
 *          1 = Buffer size is per-port and independent of the lane count.
 */
#define DP_RECEIVE_PORT_0_CAP_0                0x008
#define DP_LOCAL_EDID_PRESENT                  BIT(1)
#define DP_ASSOCIATED_TO_PRECEDING_PORT        BIT(2)
#define DP_HBLANK_EXPANSION_CAPABLE            BIT(3)

/* 00009h RECEIVE_PORT0_CAP_1
 *    7:0 BUFFER_SIZE
 *          Buffer size = (Value + 1) * 32.
 */
#define DP_RECEIVE_PORT_0_BUFFER_SIZE          0x009

/* 0000Ah RECEIVE_PORT1_CAP_0
 *          Receiver Port 1 Capability 0.
 *          Bit definition is identical to that of the RECEIVE_PORT0_CAP_0
 *          register (DPCD Address 00008h), but for Port 1.
 */
#define DP_RECEIVE_PORT_1_CAP_0                0x00a

/* 0000Bh RECEIVE_PORT1_CAP_1
 *          Receiver Port 1 Capability 1.
 *          Bit definition is identical to that of the RECEIVE_PORT0_CAP_1
 *          register (DPCD Address 00009h), but for Port 1.
 */
#define DP_RECEIVE_PORT_1_BUFFER_SIZE          0x00b

/* 0000Ch I2C Speed Control Capabilities Bit Map
 *        Bit or bits set to indicate I2C speed control capabilities.
 *        00000001b = 1Kbps.
 *        00000010b = 5Kbps.
 *        00000100b = 10Kbps.
 *        00001000b = 100Kbps.
 *        00010000b = 400Kbps.
 *        00100000b = 1Mbps.
 *        01000000b = RESERVED.
 *        10000000b = RESERVED
 */
#define DP_I2C_SPEED_CAP                       0x00c    /* DPI */
#define DP_I2C_SPEED_1K                        0x01
#define DP_I2C_SPEED_5K                        0x02
#define DP_I2C_SPEED_10K                       0x04
#define DP_I2C_SPEED_100K                      0x08
#define DP_I2C_SPEED_400K                      0x10
#define DP_I2C_SPEED_1M                        0x20

/* 0000Dh eDP_CONFIGURATION_CAP */
#define DP_EDP_CONFIGURATION_CAP               0x00d
#define DP_ALTERNATE_SCRAMBLER_RESET_CAP       BIT(0)
#define DP_FRAMING_CHANGE_CAP                  BIT(1)
#define DP_DPCD_DISPLAY_CONTROL_CAPABLE        BIT(3) /* edp v1.2 or higher */

/* 0000Eh TRAINING_AUX_RD_INTERVAL
 *    6:0 Link Status/Adjust Request read interval during Main-Link Training.
 *          00h = 100us for the Main-Link Clock Recovery phase; 400us for the
 *                Main-Link Channel Equalization phase.
 *          01h = 100us / 4ms
 *          02h = 100us / 8ms
 *          03h = 100us / 12ms
 *          04h = 100us / 16ms
 *      7 EXTENDED_RECEIVER_CAPABILITY_FIELD_PRESENT
 *          0 = Not present.
 *          1 = Present at DPCD Addresses 02200h through 022FFh.
 */
#define DP_TRAINING_AUX_RD_INTERVAL            0x00e
#define DP_TRAINING_AUX_RD_MASK                0x7F    /* DP 1.3 */
#define DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT BIT(7) /* DP 1.3 */

/* 0000Fh ADAPTOR_CAP
 *      0 FORCE_LOAD_SENSE_CAP
 *          0 = Does not support VGA force load adaptor sense mechanism.
 *          1 = Supports VGA force load adaptor sense mechanism.
 *      1 ALTERNATE_I2C_PATTERN_CAP
 *          0 = Does not support alternate I2C patterns.
 *          1 = Supports alternate I2C patterns.
 */
#define DP_ADAPTER_CAP                         0x00f   /* 1.2 */
#define DP_FORCE_LOAD_SENSE_CAP                BIT(0)
#define DP_ALTERNATE_I2C_PATTERN_CAP           BIT(1)

#define DP_SUPPORTED_LINK_RATES                0x010 /* eDP 1.4 */
#define DP_MAX_SUPPORTED_RATES                 8            /* 16-bit little-endian */

/* Multiple stream transport */
#define DP_FAUX_CAP                            0x020   /* 1.2 */
#define DP_FAUX_CAP_1                          BIT(0)

#define DP_SINK_VIDEO_FALLBACK_FORMATS         0x020   /* 2.0 */
#define DP_FALLBACK_1024x768_60HZ_24BPP        BIT(0)
#define DP_FALLBACK_1280x720_60HZ_24BPP        BIT(1)
#define DP_FALLBACK_1920x1080_60HZ_24BPP       BIT(2)

/* 00021h MSTM_CAP
 *      0   0 = Does not support MST mode and does not have a Branching Unit,
 *              and therefore does not support Sideband MSG handling.
 *          1 = Supports MST mode and has a Branching Unit, and therefore
 *              supports Sideband MSG handling.
 */
#define DP_MSTM_CAP                            0x021   /* 1.2 */
#define DP_MST_CAP                             BIT(0)
#define DP_SINGLE_STREAM_SIDEBAND_MSG          BIT(1) /* 2.0 */

/* 00022h NUMBER_OF_AUDIO_ENDPOINTS
 *        Number of audio endpoints available at this port.
 */
#define DP_NUMBER_OF_AUDIO_ENDPOINTS           0x022   /* 1.2 */

/* AV_SYNC_DATA_BLOCK                                  1.2 */
/* 00023h AV_SYNC_DATA_BLOCK AV_GRANULARITY
 *    3:0 AG_FACTOR
 *    7:4 VG_FACTOR
 */
#define DP_AV_GRANULARITY                      0x023
#define DP_AG_FACTOR_MASK                      (0xf << 0)
#define DP_AG_FACTOR_3MS                       (0 << 0)
#define DP_AG_FACTOR_2MS                       BIT(0)
#define DP_AG_FACTOR_1MS                       (2 << 0)
#define DP_AG_FACTOR_500US                     (3 << 0)
#define DP_AG_FACTOR_200US                     (4 << 0)
#define DP_AG_FACTOR_100US                     (5 << 0)
#define DP_AG_FACTOR_10US                      (6 << 0)
#define DP_AG_FACTOR_1US                       (7 << 0)
#define DP_VG_FACTOR_MASK                      (0xf << 4)
#define DP_VG_FACTOR_3MS                       (0 << 4)
#define DP_VG_FACTOR_2MS                       BIT(4)
#define DP_VG_FACTOR_1MS                       (2 << 4)
#define DP_VG_FACTOR_500US                     (3 << 4)
#define DP_VG_FACTOR_200US                     (4 << 4)
#define DP_VG_FACTOR_100US                     (5 << 4)

/* 00024h AV_SYNC_DATA_BLOCK
 *    7:0 AUD_DEC_LAT[7:0]
 */
#define DP_AUD_DEC_LAT0                        0x024
/* 00025h AV_SYNC_DATA_BLOCK
 *    7:0 AUD_DEC_LAT[15:8]
 */
#define DP_AUD_DEC_LAT1                        0x025

#define DP_AUD_PP_LAT0                         0x026
#define DP_AUD_PP_LAT1                         0x027

#define DP_VID_INTER_LAT                       0x028

#define DP_VID_PROG_LAT                        0x029

#define DP_REP_LAT                             0x02a

/* 0002Bh AV_SYNC_DATA_BLOCK
 *    7:0 AUD_DEL_INS[7:0]
 */
#define DP_AUD_DEL_INS0                        0x02b
/* 0002Ch AV_SYNC_DATA_BLOCK
 *    7:0 AUD_DEL_INS[15:8]
 */
#define DP_AUD_DEL_INS1                        0x02c
#define DP_AUD_DEL_INS2                        0x02d
/* End of AV_SYNC_DATA_BLOCK */

#define DP_RECEIVER_ALPM_CAP                   0x02e   /* eDP 1.4 */
#define DP_ALPM_CAP                            BIT(0)
#define DP_ALPM_PM_STATE_2A_SUPPORT            BIT(1) /* eDP 1.5 */
#define DP_ALPM_AUX_LESS_CAP                   BIT(2) /* eDP 1.5 */

#define DP_SINK_DEVICE_AUX_FRAME_SYNC_CAP      0x02f   /* eDP 1.4 */
#define DP_AUX_FRAME_SYNC_CAP                  BIT(0)

/* 00030h to 0003Fh GUID
 *        DPCD Address00030h contains the first octet (time_low, MSB)
 *        DPCD Address0003Fh contains the last octet (node(5))
 */
#define DP_GUID                                0x030   /* 1.2 */

#define DP_RX_GTC_VALUE_0                      0x054
#define DP_RX_GTC_VALUE_1                      0x055
#define DP_RX_GTC_VALUE_2                      0x056
#define DP_RX_GTC_VALUE_3                      0x057
#define DP_RX_GTC_MSTR_REQ                     0x058
#define DP_RX_GTC_FREQ_LOCK_DONE               0x059

/* 00060h DSC SUPPORT
 *      0 DSC Support
 *          0 = Decompression using DSC is not supported.
 *          1 = Decompression using DSC is supported.
 */
#define DP_DSC_SUPPORT                         0x060   /* DP 1.4 */
#define DP_DSC_DECOMPRESSION_IS_SUPPORTED      BIT(0)
#define DP_DSC_PASSTHROUGH_IS_SUPPORTED        BIT(1)
#define DP_DSC_DYNAMIC_PPS_UPDATE_SUPPORT_COMP_TO_COMP    BIT(2)
#define DP_DSC_DYNAMIC_PPS_UPDATE_SUPPORT_UNCOMP_TO_COMP  BIT(3)

/* 00061h DSC ALGORITHM REVISION
 *    3:0 DSC Version Major
 *    7:4 DSC Version Minor
 */
#define DP_DSC_REV                             0x061
#define DP_DSC_MAJOR_MASK                      (0xf << 0)
#define DP_DSC_MINOR_MASK                      (0xf << 4)
#define DP_DSC_MAJOR_SHIFT                     0
#define DP_DSC_MINOR_SHIFT                     4

/* 00062h DSC RC BUFFER BLOCK SIZE
 *    1:0 RC Buffer Block Size / Rate control buffer block size.
 *          00 = 1KB.
 *          01 = 4KB.
 *          10 = 16KB.
 *          11 = 64KB.
 */
#define DP_DSC_RC_BUF_BLK_SIZE                 0x062
#define DP_DSC_RC_BUF_BLK_SIZE_1               0x0
#define DP_DSC_RC_BUF_BLK_SIZE_4               0x1
#define DP_DSC_RC_BUF_BLK_SIZE_16              0x2
#define DP_DSC_RC_BUF_BLK_SIZE_64              0x3

/* 00063h DSC RC BUFFER SIZE
 *    7:0 Each Rate Buffer Size, in Units of Blocks
 *          Buffer size (in blocks) = value + 1.
 */
#define DP_DSC_RC_BUF_SIZE                     0x063

/* 00064h DSC SLICE CAPABILITIES 1
 *      0 1_Slice_per_DP_DSC_Sink_Device
 *      1 2_Slices_per_DP_DSC_Sink_Device
 *      3 4_Slices_per_DP_DSC_Sink_Device
 *      4 6_Slices_per_DP_DSC_Sink_Device
 *      5 8_Slices_per_DP_DSC_Sink_Device
 *      6 10_Slices_per_DP_DSC_Sink_Device
 *      7 12_Slices_per_DP_DSC_Sink_Device
 */
#define DP_DSC_SLICE_CAP_1                     0x064
#define DP_DSC_1_PER_DP_DSC_SINK               BIT(0)
#define DP_DSC_2_PER_DP_DSC_SINK               BIT(1)
#define DP_DSC_4_PER_DP_DSC_SINK               BIT(3)
#define DP_DSC_6_PER_DP_DSC_SINK               BIT(4)
#define DP_DSC_8_PER_DP_DSC_SINK               BIT(5)
#define DP_DSC_10_PER_DP_DSC_SINK              BIT(6)
#define DP_DSC_12_PER_DP_DSC_SINK              BIT(7)

/* 00065h DSC LINE BUFFER BIT DEPTH
 *    3:0 Line Buffer Bit Depth
 *          0000 = 9 bits.
 *          0001 = 10 bits.
 *          0010 = 11 bits.
 *          0011 = 12 bits.
 *          0100 = 13 bits.
 *          0101 = 14 bits.
 *          0110 = 15 bits.
 *          0111 = 16 bits.
 *          1000 = 8 bits.
 */
#define DP_DSC_LINE_BUF_BIT_DEPTH              0x065
#define DP_DSC_LINE_BUF_BIT_DEPTH_MASK         (0xf << 0)
#define DP_DSC_LINE_BUF_BIT_DEPTH_9            0x0
#define DP_DSC_LINE_BUF_BIT_DEPTH_10           0x1
#define DP_DSC_LINE_BUF_BIT_DEPTH_11           0x2
#define DP_DSC_LINE_BUF_BIT_DEPTH_12           0x3
#define DP_DSC_LINE_BUF_BIT_DEPTH_13           0x4
#define DP_DSC_LINE_BUF_BIT_DEPTH_14           0x5
#define DP_DSC_LINE_BUF_BIT_DEPTH_15           0x6
#define DP_DSC_LINE_BUF_BIT_DEPTH_16           0x7
#define DP_DSC_LINE_BUF_BIT_DEPTH_8            0x8

/* 00066h DSC BLOCK PREDICTION SUPPORT
 *      0 Block Prediction Support
 *          0 = Block prediction is not supported.
 *          1 = Block prediction is supported.
 */
#define DP_DSC_BLK_PREDICTION_SUPPORT          0x066
#define DP_DSC_BLK_PREDICTION_IS_SUPPORTED     BIT(0)
#define DP_DSC_RGB_COLOR_CONV_BYPASS_SUPPORT   BIT(1)

/* 00067h MAXIMUM bits_per_pixel SUPPORTED BY THE DECOMPRESSOR
 */
#define DP_DSC_MAX_BITS_PER_PIXEL_LOW          0x067   /* eDP 1.4 */

/* 00068h MAXIMUM bits_per_pixel SUPPORTED BY THE DECOMPRESSOR
 */
#define DP_DSC_MAX_BITS_PER_PIXEL_HI           0x068   /* eDP 1.4 */
#define DP_DSC_MAX_BITS_PER_PIXEL_HI_MASK      (0x3 << 0)
#define DP_DSC_MAX_BPP_DELTA_VERSION_MASK      (0x3 << 5)        /* eDP 1.5 & DP 2.0 */
#define DP_DSC_MAX_BPP_DELTA_AVAILABILITY      BIT(7)        /* eDP 1.5 & DP 2.0 */

/* 00069h DSC DECODER COLOR FORMAT CAPABILITIES
 *      0 RGB Support
 *      1 YCbCr 4:4:4 Support
 *      2 YCbCr Simple 4:2:2 Support
 *      3 YCbCr Native 4:2:2 Support
 *      4 YCbCr Native 4:2:0 Support
 */
#define DP_DSC_DEC_COLOR_FORMAT_CAP            0x069
#define DP_DSC_RGB                             BIT(0)
#define DP_DSC_YCbCr444                        BIT(1)
#define DP_DSC_YCbCr422_Simple                 BIT(2)
#define DP_DSC_YCbCr422_Native                 BIT(3)
#define DP_DSC_YCbCr420_Native                 BIT(4)

/* 0006Ah DSC DECODER COLOR DEPTH CAPABILITIES
 *      1 8 Bits per Color Support
 *      2 10 Bits per Color Support
 *      3 12 Bits per Color Support
 */
#define DP_DSC_DEC_COLOR_DEPTH_CAP             0x06A
#define DP_DSC_8_BPC                           BIT(1)
#define DP_DSC_10_BPC                          BIT(2)
#define DP_DSC_12_BPC                          BIT(3)

/* 0006Bh Peak DSC Throughput (DSC Sink)
 *    3:0 Throughput Mode 0
 *          0 = Not supported.
 *          1 = 340MP/s.
 *          2 = 400MP/s.
 *          3 = 450MP/s.
 *          4 = 500MP/s.
 *          5 = 550MP/s.
 *          6 = 600MP/s.
 *          7 = 650MP/s.
 *          8 = 700MP/s.
 *          9 = 750MP/s.
 *          10 = 800MP/s.
 *          11 = 850MP/s.
 *          12 = 900MP/s.
 *          13 = 950MP/s.
 *          14 = 1000MP/s.
 *          15 = RESERVED.
 *    7:4 Throughput Mode 1
 *          0 = Not supported.
 *          1 = 340MP/s.
 *          2 = 400MP/s.
 *          3 = 450MP/s.
 *          4 = 500MP/s.
 *          5 = 550MP/s.
 *          6 = 600MP/s.
 *          7 = 650MP/s.
 *          8 = 700MP/s.
 *          9 = 750MP/s.
 *          10 = 800MP/s.
 *          11 = 850MP/s.
 *          12 = 900MP/s.
 *          13 = 950MP/s.
 *          14 = 1000MP/s.
 *          15 = RESERVED.
 */
#define DP_DSC_PEAK_THROUGHPUT                 0x06B
#define DP_DSC_THROUGHPUT_MODE_0_MASK          (0xf << 0)
#define DP_DSC_THROUGHPUT_MODE_0_SHIFT         0
#define DP_DSC_THROUGHPUT_MODE_0_UNSUPPORTED   0
#define DP_DSC_THROUGHPUT_MODE_0_340           BIT(0)
#define DP_DSC_THROUGHPUT_MODE_0_400           (2 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_450           (3 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_500           (4 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_550           (5 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_600           (6 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_650           (7 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_700           (8 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_750           (9 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_800           (10 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_850           (11 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_900           (12 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_950           (13 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_1000          (14 << 0)
#define DP_DSC_THROUGHPUT_MODE_0_170           (15 << 0) /* 1.4a */
#define DP_DSC_THROUGHPUT_MODE_1_MASK          (0xf << 4)
#define DP_DSC_THROUGHPUT_MODE_1_SHIFT         4
#define DP_DSC_THROUGHPUT_MODE_1_UNSUPPORTED   0
#define DP_DSC_THROUGHPUT_MODE_1_340           BIT(4)
#define DP_DSC_THROUGHPUT_MODE_1_400           (2 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_450           (3 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_500           (4 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_550           (5 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_600           (6 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_650           (7 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_700           (8 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_750           (9 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_800           (10 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_850           (11 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_900           (12 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_950           (13 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_1000          (14 << 4)
#define DP_DSC_THROUGHPUT_MODE_1_170           (15 << 4)

/* 0006Ch DSC Maximum Slice Width
 *    7:0 DSC Max Slice Width
 *          MaxSliceWidth = Number of pixels × 320. The minimum number
 *          allowed is 8 (2560 pixels).
 */
#define DP_DSC_MAX_SLICE_WIDTH                 0x06C
#define DP_DSC_MIN_SLICE_WIDTH_VALUE           2560
#define DP_DSC_SLICE_WIDTH_MULTIPLIER          320

/* 0006Dh DSC SLICE CAPABILITIES 2
 *      0 16 Slices_per_DP_DSC_Sink_Device
 *      1 20 Slices_per_DP_DSC_Sink_Device
 *      2 24 Slices_per_DP_DSC_Sink_Device
 */
#define DP_DSC_SLICE_CAP_2                     0x06D
#define DP_DSC_16_PER_DP_DSC_SINK              BIT(0)
#define DP_DSC_20_PER_DP_DSC_SINK              BIT(1)
#define DP_DSC_24_PER_DP_DSC_SINK              BIT(2)

/* 0006Fh BITS_PER_PIXEL_INCREMENT
 *    2:0 INCREMENT of bits_per_pixel SUPPORTED BY THE DECOMPRESSOR
 *          000 = 1/16bpp.
 *          001 = 1/8bpp.
 *          010 = 1/4bpp.
 *          011 = 1/2bpp.
 *          100 = 1bpp.
 */
#define DP_DSC_BITS_PER_PIXEL_INC              0x06F
#define DP_DSC_RGB_YCbCr444_MAX_BPP_DELTA_MASK 0x1f
#define DP_DSC_RGB_YCbCr420_MAX_BPP_DELTA_MASK 0xe0
#define DP_DSC_BITS_PER_PIXEL_1_16             0x0
#define DP_DSC_BITS_PER_PIXEL_1_8              0x1
#define DP_DSC_BITS_PER_PIXEL_1_4              0x2
#define DP_DSC_BITS_PER_PIXEL_1_2              0x3
#define DP_DSC_BITS_PER_PIXEL_1_1              0x4
#define DP_DSC_BITS_PER_PIXEL_MASK             0x7

/* 00070h ~ 0007Fh */
#define DP_PSR_SUPPORT                         0x070
#define DP_PSR_IS_SUPPORTED                    1
#define DP_PSR2_IS_SUPPORTED                   2            /* eDP 1.4 */
#define DP_PSR2_WITH_Y_COORD_IS_SUPPORTED      3            /* eDP 1.4a */
#define DP_PSR2_WITH_Y_COORD_ET_SUPPORTED      4            /* eDP 1.5, adopted eDP 1.4b SCR */

#define DP_PSR_CAPS                            0x071
#define DP_PSR_NO_TRAIN_ON_EXIT                1
#define DP_PSR_SETUP_TIME_330                  (0 << 1)
#define DP_PSR_SETUP_TIME_275                  BIT(1)
#define DP_PSR_SETUP_TIME_220                  (2 << 1)
#define DP_PSR_SETUP_TIME_165                  (3 << 1)
#define DP_PSR_SETUP_TIME_110                  (4 << 1)
#define DP_PSR_SETUP_TIME_55                   (5 << 1)
#define DP_PSR_SETUP_TIME_0                    (6 << 1)
#define DP_PSR_SETUP_TIME_MASK                 (7 << 1)
#define DP_PSR_SETUP_TIME_SHIFT                1
#define DP_PSR2_SU_Y_COORDINATE_REQUIRED       BIT(4)  /* eDP 1.4a */
#define DP_PSR2_SU_GRANULARITY_REQUIRED        BIT(5)  /* eDP 1.4b */
#define DP_PSR2_SU_AUX_FRAME_SYNC_NOT_NEEDED   BIT(6)/* eDP 1.5, adopted eDP 1.4b SCR */

#define DP_PSR2_SU_X_GRANULARITY               0x072 /* eDP 1.4b */
#define DP_PSR2_SU_Y_GRANULARITY               0x074 /* eDP 1.4b */

/*
 * 0x80-0x8f describe downstream port capabilities, but there are two layouts
 * based on whether DP_DETAILED_CAP_INFO_AVAILABLE was set.  If it was not,
 * each port's descriptor is one byte wide.  If it was set, each port's is
 * four bytes wide, starting with the one byte from the base info.  As of
 * DP interop v1.1a only VGA defines additional detail.
 */

/* offset 0 */
#define DP_DOWNSTREAM_PORT_0                   0x80
#define DP_DS_PORT_TYPE_MASK                   (7 << 0)
#define DP_DS_PORT_TYPE_DP                     0
#define DP_DS_PORT_TYPE_VGA                    1
#define DP_DS_PORT_TYPE_DVI                    2
#define DP_DS_PORT_TYPE_HDMI                   3
#define DP_DS_PORT_TYPE_NON_EDID               4
#define DP_DS_PORT_TYPE_DP_DUALMODE            5
#define DP_DS_PORT_TYPE_WIRELESS               6
#define DP_DS_PORT_HPD                         BIT(3)
#define DP_DS_NON_EDID_MASK                    (0xf << 4)
#define DP_DS_NON_EDID_720x480i_60             BIT(4)
#define DP_DS_NON_EDID_720x480i_50             (2 << 4)
#define DP_DS_NON_EDID_1920x1080i_60           (3 << 4)
#define DP_DS_NON_EDID_1920x1080i_50           (4 << 4)
#define DP_DS_NON_EDID_1280x720_60             (5 << 4)
#define DP_DS_NON_EDID_1280x720_50             (7 << 4)
/* offset 1 for VGA is maximum megapixels per second / 8 */
/* offset 1 for DVI/HDMI is maximum TMDS clock in Mbps / 2.5 */
/* offset 2 for VGA/DVI/HDMI */
#define DP_DS_MAX_BPC_MASK                     (3 << 0)
#define DP_DS_8BPC                             0
#define DP_DS_10BPC                            1
#define DP_DS_12BPC                            2
#define DP_DS_16BPC                            3
/* HDMI2.1 PCON FRL CONFIGURATION */
#define DP_PCON_MAX_FRL_BW                     (7 << 2)
#define DP_PCON_MAX_0GBPS                      (0 << 2)
#define DP_PCON_MAX_9GBPS                      BIT(2)
#define DP_PCON_MAX_18GBPS                     (2 << 2)
#define DP_PCON_MAX_24GBPS                     (3 << 2)
#define DP_PCON_MAX_32GBPS                     (4 << 2)
#define DP_PCON_MAX_40GBPS                     (5 << 2)
#define DP_PCON_MAX_48GBPS                     (6 << 2)
#define DP_PCON_SOURCE_CTL_MODE                BIT(5)

/* offset 3 for DVI */
#define DP_DS_DVI_DUAL_LINK                    BIT(1)
#define DP_DS_DVI_HIGH_COLOR_DEPTH             BIT(2)
/* offset 3 for HDMI */
#define DP_DS_HDMI_FRAME_SEQ_TO_FRAME_PACK     BIT(0)
#define DP_DS_HDMI_YCBCR422_PASS_THROUGH       BIT(1)
#define DP_DS_HDMI_YCBCR420_PASS_THROUGH       BIT(2)
#define DP_DS_HDMI_YCBCR444_TO_422_CONV        BIT(3)
#define DP_DS_HDMI_YCBCR444_TO_420_CONV        BIT(4)

/*
 * VESA DP-to-HDMI PCON Specification adds caps for colorspace
 * conversion in DFP cap DPCD 83h. Sec6.1 Table-3.
 * Based on the available support the source can enable
 * color conversion by writing into PROTOCOL_COVERTER_CONTROL_2
 * DPCD 3052h.
 */
#define DP_DS_HDMI_BT601_RGB_YCBCR_CONV        BIT(5)
#define DP_DS_HDMI_BT709_RGB_YCBCR_CONV        BIT(6)
#define DP_DS_HDMI_BT2020_RGB_YCBCR_CONV       BIT(7)

#define DP_MAX_DOWNSTREAM_PORTS                0x10

/* DP Forward error Correction Registers */
/* 00090h FEC_CAPABILITY
 *      0 FEC_CAPABLE
 *      1 UNCORRECTED_BLOCK_ERROR_COUNT_CAPABLE
 *      2 CORRECTED_BLOCK_ERROR_COUNT_CAPABLE
 *      3 BIT_ERROR_COUNT_CAPABLE
 */
#define DP_FEC_CAPABILITY                      0x090    /* 1.4 */
#define DP_FEC_CAPABLE                         BIT(0)
#define DP_FEC_UNCORR_BLK_ERROR_COUNT_CAP      BIT(1)
#define DP_FEC_CORR_BLK_ERROR_COUNT_CAP        BIT(2)
#define DP_FEC_BIT_ERROR_COUNT_CAP             BIT(3)
#define DP_FEC_CAPABILITY_1                    0x091   /* 2.0 */

/* DP-HDMI2.1 PCON DSC ENCODER SUPPORT */
#define DP_PCON_DSC_ENCODER_CAP_SIZE           0xD        /* 0x92 through 0x9E */
#define DP_PCON_DSC_ENCODER                    0x092
#define DP_PCON_DSC_ENCODER_SUPPORTED          BIT(0)
#define DP_PCON_DSC_PPS_ENC_OVERRIDE           BIT(1)

/* DP-HDMI2.1 PCON DSC Version */
#define DP_PCON_DSC_VERSION                    0x093
#define DP_PCON_DSC_MAJOR_MASK                 (0xF << 0)
#define DP_PCON_DSC_MINOR_MASK                 (0xF << 4)
#define DP_PCON_DSC_MAJOR_SHIFT                0
#define DP_PCON_DSC_MINOR_SHIFT                4

/* DP-HDMI2.1 PCON DSC RC Buffer block size */
#define DP_PCON_DSC_RC_BUF_BLK_INFO            0x094
#define DP_PCON_DSC_RC_BUF_BLK_SIZE            (0x3 << 0)
#define DP_PCON_DSC_RC_BUF_BLK_1KB             0
#define DP_PCON_DSC_RC_BUF_BLK_4KB             1
#define DP_PCON_DSC_RC_BUF_BLK_16KB            2
#define DP_PCON_DSC_RC_BUF_BLK_64KB            3

/* DP-HDMI2.1 PCON DSC RC Buffer size */
#define DP_PCON_DSC_RC_BUF_SIZE                0x095

/* DP-HDMI2.1 PCON DSC Slice capabilities-1 */
#define DP_PCON_DSC_SLICE_CAP_1                0x096
#define DP_PCON_DSC_1_PER_DSC_ENC              (0x1 << 0)
#define DP_PCON_DSC_2_PER_DSC_ENC              (0x1 << 1)
#define DP_PCON_DSC_4_PER_DSC_ENC              (0x1 << 3)
#define DP_PCON_DSC_6_PER_DSC_ENC              (0x1 << 4)
#define DP_PCON_DSC_8_PER_DSC_ENC              (0x1 << 5)
#define DP_PCON_DSC_10_PER_DSC_ENC             (0x1 << 6)
#define DP_PCON_DSC_12_PER_DSC_ENC             (0x1 << 7)

#define DP_PCON_DSC_BUF_BIT_DEPTH              0x097
#define DP_PCON_DSC_BIT_DEPTH_MASK             (0xF << 0)
#define DP_PCON_DSC_DEPTH_9_BITS               0
#define DP_PCON_DSC_DEPTH_10_BITS              1
#define DP_PCON_DSC_DEPTH_11_BITS              2
#define DP_PCON_DSC_DEPTH_12_BITS              3
#define DP_PCON_DSC_DEPTH_13_BITS              4
#define DP_PCON_DSC_DEPTH_14_BITS              5
#define DP_PCON_DSC_DEPTH_15_BITS              6
#define DP_PCON_DSC_DEPTH_16_BITS              7
#define DP_PCON_DSC_DEPTH_8_BITS               8

#define DP_PCON_DSC_BLOCK_PREDICTION           0x098
#define DP_PCON_DSC_BLOCK_PRED_SUPPORT         (0x1 << 0)

#define DP_PCON_DSC_ENC_COLOR_FMT_CAP          0x099
#define DP_PCON_DSC_ENC_RGB                    (0x1 << 0)
#define DP_PCON_DSC_ENC_YUV444                 (0x1 << 1)
#define DP_PCON_DSC_ENC_YUV422_S               (0x1 << 2)
#define DP_PCON_DSC_ENC_YUV422_N               (0x1 << 3)
#define DP_PCON_DSC_ENC_YUV420_N               (0x1 << 4)

#define DP_PCON_DSC_ENC_COLOR_DEPTH_CAP        0x09A
#define DP_PCON_DSC_ENC_8BPC                   (0x1 << 1)
#define DP_PCON_DSC_ENC_10BPC                  (0x1 << 2)
#define DP_PCON_DSC_ENC_12BPC                  (0x1 << 3)

#define DP_PCON_DSC_MAX_SLICE_WIDTH            0x09B

/* DP-HDMI2.1 PCON DSC Slice capabilities-2 */
#define DP_PCON_DSC_SLICE_CAP_2                0x09C
#define DP_PCON_DSC_16_PER_DSC_ENC             (0x1 << 0)
#define DP_PCON_DSC_20_PER_DSC_ENC             (0x1 << 1)
#define DP_PCON_DSC_24_PER_DSC_ENC             (0x1 << 2)

/* DP-HDMI2.1 PCON HDMI TX Encoder Bits/pixel increment */
#define DP_PCON_DSC_BPP_INCR                   0x09E
#define DP_PCON_DSC_BPP_INCR_MASK              (0x7 << 0)
#define DP_PCON_DSC_ONE_16TH_BPP               0
#define DP_PCON_DSC_ONE_8TH_BPP                1
#define DP_PCON_DSC_ONE_4TH_BPP                2
#define DP_PCON_DSC_ONE_HALF_BPP               3
#define DP_PCON_DSC_ONE_BPP                    4

/* DP Extended DSC Capabilities */
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_0     0x0a0   /* DP 1.4a SCR */
#define DP_DSC_BRANCH_OVERALL_THROUGHPUT_1     0x0a1
#define DP_DSC_BRANCH_MAX_LINE_WIDTH           0x0a2

/* DFP Capability Extension */
#define DP_DFP_CAPABILITY_EXTENSION_SUPPORT    0x0a3        /* 2.0 */

#define DP_PANEL_REPLAY_CAP                    0x0b0  /* DP 2.0 */
#define DP_PANEL_REPLAY_SUPPORT                BIT(0)
#define DP_PANEL_REPLAY_SU_SUPPORT             BIT(1)
#define DP_PANEL_REPLAY_EARLY_TRANSPORT_SUPPORT BIT(2) /* eDP 1.5 */

#define DP_PANEL_PANEL_REPLAY_CAPABILITY       0xb1
#define DP_PANEL_PANEL_REPLAY_SU_GRANULARITY_REQUIRED BIT(5)

#define DP_PANEL_PANEL_REPLAY_X_GRANULARITY    0xb2
#define DP_PANEL_PANEL_REPLAY_Y_GRANULARITY    0xb4

/* Link Configuration */
/* 00100h LINK_BW_SET
 *        Main-Link Bandwidth Setting = Value * 0.27Gbps/lane.
 *          06h = 1.62Gbps/lane
 *          0Ah = 2.7Gbps/lane
 *          14h = 5.4Gbps/lane
 *          1Eh = 8.1Gbps/lane
 */
#define DP_LINK_BW_SET                         0x100
#define DP_LINK_RATE_TABLE                     0x00    /* eDP 1.4 */
#define DP_LINK_BW_1_62                        0x06
#define DP_LINK_BW_2_7                         0x0a
#define DP_LINK_BW_5_4                         0x14    /* 1.2 */
#define DP_LINK_BW_8_1                         0x1e    /* 1.4 */
#define DP_LINK_BW_10                          0x01    /* 2.0 128b/132b Link Layer */
#define DP_LINK_BW_13_5                        0x04    /* 2.0 128b/132b Link Layer */
#define DP_LINK_BW_20                          0x02    /* 2.0 128b/132b Link Layer */

/* 00101h LANE_COUNT_SET
 *    4:0 LANE_COUNT_SET
 *          1h = 1 lane (Lane 0 only).
 *          2h = 2 lanes (Lanes 0 and 1 only).
 *          4h = 4 lanes (Lanes 0, 1, 2, and 3).
 *      5 POST_LT_ADJ_REQ_GRANTED
 *      7 ENHANCED_FRAME_EN
 *          0 = Enhanced Framing symbol sequence is not enabled.
 *          1 = Enhanced Framing symbol sequence for BS and SR is enabled.
 */
#define DP_LANE_COUNT_SET                      0x101
#define DP_LANE_COUNT_MASK                     0x0f
#define DP_LANE_COUNT_ENHANCED_FRAME_EN        BIT(7)

/* 00102h TRAINING_PATTERN_SET
 * For DPCD r1.4:
 *    3:0 TRAINING_PATTERN_SELECT
 *        Link Training Pattern Selection
 *          0000 = Training not in progress (or disabled).
 *          0001 = Link Training Pattern Sequence 1.
 *          0010 = Link Training Pattern Sequence 2.
 *          0011 = Link Training Pattern Sequence 3.
 *          0111 = Link Training Pattern Sequence 4.
 *      4 RECOVERED_CLOCK_OUT_EN
 *          0 = Recovered clock output from a test pad of DPRX is not enabled.
 *          1 = Recovered clock output from a test pad of DPRX enabled.
 *      5 SCRAMBLING_DISABLE
 *          0 = DPTX scrambles data symbols before transmission.
 *          1 = DPTX disables scrambler and transmits all symbols without scrambling.
 *    7:6 SYMBOL_ERROR_COUNT_SEL
 *          00 = Count Disparity and Illegal Symbol errors
 *          01 = Count Disparity errors only
 *          10 = Count Illegal Symbol errors only
 * For DPCD r1.2 and DPCD r1.3:
 *    1:0 TRAINING_PATTERN_SELECT
 *        Link Training Pattern Sequence selection
 *          00 = Training not in progress (or disabled).
 *          01 = Link Training Pattern Sequence 1.
 *          10 = Link Training Pattern Sequence 2.
 *          11 = Link Training Pattern Sequence 3.
 *    3:2 Replaced with per-lane control in each Lane’s LINK_QUAL_PATTERN_SET
 *        fields in the LINK_QUAL_LANEx_SET register(s) (DPCD Address(es) 0010Bh
 *        through 0010Eh, bits 2:0).
 * For DPCD r1.1:
 *    1:0 TRAINING_PATTERN_SELECT
 *          00 = Training not in progress (or disabled).
 *          01 = Link Training Pattern Sequence 1.
 *          10 = Link Training Pattern Sequence 2.
 *    3:2 LINK_QUAL_PATTERN_SET
 *          00 = Link quality test pattern not transmitted
 *          01 = D10.2 test pattern (unscrambled) transmitted
 *               (same as Link Training Pattern Sequence 1)
 *          10 = Symbol Error Rate measurement pattern transmitted
 *          11 = PRBS7 transmitted
 */
#define DP_TRAINING_PATTERN_SET                0x102
#define DP_TRAINING_PATTERN_DISABLE            0
#define DP_TRAINING_PATTERN_1                  1
#define DP_TRAINING_PATTERN_2                  2
#define DP_TRAINING_PATTERN_2_CDS              3            /* 2.0 E11 */
#define DP_TRAINING_PATTERN_3                  3            /* 1.2 */
#define DP_TRAINING_PATTERN_4                  7       /* 1.4 */
#define DP_TRAINING_PATTERN_MASK               0x3
#define DP_TRAINING_PATTERN_MASK_1_4           0xf

/* DPCD 1.1 only. For DPCD >= 1.2 see per-lane DP_LINK_QUAL_LANEn_SET */
#define DP_LINK_QUAL_PATTERN_11_DISABLE        (0 << 2)
#define DP_LINK_QUAL_PATTERN_11_D10_2          BIT(2)
#define DP_LINK_QUAL_PATTERN_11_ERROR_RATE     (2 << 2)
#define DP_LINK_QUAL_PATTERN_11_PRBS7          (3 << 2)
#define DP_LINK_QUAL_PATTERN_11_MASK           (3 << 2)

#define DP_RECOVERED_CLOCK_OUT_EN              BIT(4)
#define DP_LINK_SCRAMBLING_DISABLE             BIT(5)

#define DP_SYMBOL_ERROR_COUNT_BOTH             (0 << 6)
#define DP_SYMBOL_ERROR_COUNT_DISPARITY        BIT(6)
#define DP_SYMBOL_ERROR_COUNT_SYMBOL           (2 << 6)
#define DP_SYMBOL_ERROR_COUNT_MASK             (3 << 6)

/* 00103h TRAINING_LANE0_SET
 *    1:0 VOLTAGE SWING SET
 *          00 = Voltage Swing Level 0.
 *          01 = Voltage Swing Level 1.
 *          10 = Voltage Swing Level 2.
 *          11 = Voltage Swing Level 3.
 *      2 MAX_SWING_REACHED
 *    4:3 PRE-EMPHASIS_SET
 *          00 = Pre-emphasis Level 0.
 *          01 = Pre-emphasis Level 1.
 *          10 = Pre-emphasis Level 2.
 *          11 = Pre-emphasis Level 3.
 *      5 MAX_PRE-EMPHASIS_REACHED
 *        The transmitter shall support at least three levels of pre-emphasis (Levels 0, 1,
 *        and 2). May also support Pre-emphasis Level 3.
 */
#define DP_TRAINING_LANE0_SET                  0x103
#define DP_TRAINING_LANE1_SET                  0x104
#define DP_TRAINING_LANE2_SET                  0x105
#define DP_TRAINING_LANE3_SET                  0x106

#define DP_TRAIN_VOLTAGE_SWING_MASK            0x3
#define DP_TRAIN_VOLTAGE_SWING_SHIFT           0
#define DP_TRAIN_MAX_SWING_REACHED             BIT(2)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_0         (0 << 0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_1         BIT(0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_2         (2 << 0)
#define DP_TRAIN_VOLTAGE_SWING_LEVEL_3         (3 << 0)

#define DP_TRAIN_PRE_EMPHASIS_MASK             (3 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_0              (0 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_1              BIT(3)
#define DP_TRAIN_PRE_EMPH_LEVEL_2              (2 << 3)
#define DP_TRAIN_PRE_EMPH_LEVEL_3              (3 << 3)

#define DP_TRAIN_PRE_EMPHASIS_SHIFT            3
#define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED      BIT(5)

#define DP_TX_FFE_PRESET_VALUE_MASK            (0xf << 0) /* 2.0 128b/132b Link Layer */

/* 00107h DOWNSPREAD_CTRL
 *      4 SPREAD_AMP
 *          0 = Main-Link signal is not down-spread.
 *          1 = Main-Link signal is down-spread by equal to or less than 0.5% with
 *              a modulation frequency in the range of 30 to 33kHz.
 *      7 MSA_TIMING_PAR_IGNORE_EN
 *          0 = Source device sends valid data for MSA timing parameters
 *          1 = Source device may send invalid data for the above-mentioned MSA timing
 *              parameters. The Sink device shall ignore these parameters and regenerate the
 *              incoming video stream without depending on these parameters.
 */
#define DP_DOWNSPREAD_CTRL                     0x107
#define DP_SPREAD_AMP_0_5                      BIT(4)
#define DP_FIXED_VTOTAL_AS_SDP_EN_IN_PR_ACTIVE BIT(6)
#define DP_MSA_TIMING_PAR_IGNORE_EN            BIT(7) /* eDP */

/* 00108h MAIN_LINK_CHANNEL_CODING_SET
 *      0 SET 8b/10b
 */
#define DP_MAIN_LINK_CHANNEL_CODING_SET        0x108
#define DP_SET_ANSI_8B10B                      BIT(0)
#define DP_SET_ANSI_128B132B                   BIT(1)

/* 00109h I2C Speed Control/Status Bit Map
 *    7:0   00000001b = 1Kbps.
 *          00000010b = 5Kbps.
 *          00000100b = 10Kbps.
 *          00001000b = 100Kbps.
 *          00010000b = 400Kbps.
 *          00100000b = 1Mbps.
 */
#define DP_I2C_SPEED_CONTROL_STATUS            0x109   /* DPI */
/* bitmask as for DP_I2C_SPEED_CAP */

/* 0010Ah eDP_CONFIGURATION_SET
 *        only for eDP sinks
 *      0 ALTERNATE_SCRAMBLER_RESET_ENABLE
 *      7 PANEL_SELF_TEST_ENABLE
 */
#define DP_EDP_CONFIGURATION_SET               0x10a
#define DP_ALTERNATE_SCRAMBLER_RESET_ENABLE    BIT(0)
#define DP_FRAMING_CHANGE_ENABLE               BIT(1)
#define DP_PANEL_SELF_TEST_ENABLE              BIT(7)

/* 0010Bh LINK_QUAL_LANE0_SET
 *    2:0 LINK_QUAL_PATTERN_SET
 *          000 = Link quality test pattern not transmitted.
 *          001 = D10.2 test pattern (unscrambled) transmitted (same as Link Training Pattern
 *                Sequence 1).
 *          010 = Symbol Error Rate Measurement Pattern transmitted.
 *          011 = PRBS7 transmitted.
 *          100 = 80-bit custom pattern transmitted.
 *          101 = CP2520 Pattern 1.
 *          110 = CP2520 Pattern 2.
 *          111 = CP2520 Pattern 3 (which is TPS4).
 */
#define DP_LINK_QUAL_LANE0_SET                 0x10b   /* DPCD >= 1.2 */
#define DP_LINK_QUAL_LANE1_SET                 0x10c
#define DP_LINK_QUAL_LANE2_SET                 0x10d
#define DP_LINK_QUAL_LANE3_SET                 0x10e
#define DP_LINK_QUAL_PATTERN_DISABLE           0
#define DP_LINK_QUAL_PATTERN_D10_2             1
#define DP_LINK_QUAL_PATTERN_ERROR_RATE        2
#define DP_LINK_QUAL_PATTERN_PRBS7             3
#define DP_LINK_QUAL_PATTERN_80BIT_CUSTOM      4
#define DP_LINK_QUAL_PATTERN_CP2520_PAT_1      5
#define DP_LINK_QUAL_PATTERN_CP2520_PAT_2      6
#define DP_LINK_QUAL_PATTERN_CP2520_PAT_3      7
/* DP 2.0 UHBR10, UHBR13.5, UHBR20 */
#define DP_LINK_QUAL_PATTERN_128B132B_TPS1     0x08
#define DP_LINK_QUAL_PATTERN_128B132B_TPS2     0x10
#define DP_LINK_QUAL_PATTERN_PRSBS9            0x18
#define DP_LINK_QUAL_PATTERN_PRSBS11           0x20
#define DP_LINK_QUAL_PATTERN_PRSBS15           0x28
#define DP_LINK_QUAL_PATTERN_PRSBS23           0x30
#define DP_LINK_QUAL_PATTERN_PRSBS31           0x38
#define DP_LINK_QUAL_PATTERN_CUSTOM            0x40
#define DP_LINK_QUAL_PATTERN_SQUARE            0x48
#define DP_LINK_QUAL_PATTERN_SQUARE_PRESHOOT_DISABLED            0x49
#define DP_LINK_QUAL_PATTERN_SQUARE_DEEMPHASIS_DISABLED          0x4a
#define DP_LINK_QUAL_PATTERN_SQUARE_PRESHOOT_DEEMPHASIS_DISABLED 0x4b

#define DP_TRAINING_LANE0_1_SET2               0x10f
#define DP_TRAINING_LANE2_3_SET2               0x110
#define DP_LANE02_POST_CURSOR2_SET_MASK        (3 << 0)
#define DP_LANE02_MAX_POST_CURSOR2_REACHED     BIT(2)
#define DP_LANE13_POST_CURSOR2_SET_MASK        (3 << 4)
#define DP_LANE13_MAX_POST_CURSOR2_REACHED     BIT(6)

/* 00111h MSTM_CTRL
 *      0 MST_EN
 *          0 = UFP shall transmit audio/visual data in Single-Stream Transport (SST) mode.
 *          1 = UFP shall transmit audio/visual data in Multi-Stream Transport (MST) mode.
 *      1 UP_REQ_EN
 *          0 = Prohibits the Downstream DPRX from originating/forwarding an UP_REQ
 *              message transaction.
 *          1 = Allows the Downstream DPRX to originating/forwarding an UP_REQ message
 *              transaction.
 *      2 UPSTREAM_IS_SRC
 *          0 = Upstream device is either a Source device predating DP v1.2 or
 *              a Branch device.
 *          1 = Set to 1 by a DP Source device to indicate to the downstream device the
 *              presence of a Source device, not a Branch device.
 */
#define DP_MSTM_CTRL                           0x111   /* 1.2 */
#define DP_MST_EN                              BIT(0)
#define DP_UP_REQ_EN                           BIT(1)
#define DP_UPSTREAM_IS_SRC                     BIT(2)

/* 00112h AUDIO_DELAY[7:0]
 */
#define DP_AUDIO_DELAY0                        0x112   /* 1.2 */
#define DP_AUDIO_DELAY1                        0x113
#define DP_AUDIO_DELAY2                        0x114

/* 00115h LINK_RATE_SET and TX_GTC_CAPABILITY
 *    2:0 LINK_RATE_SET
 *      3 TX_GTC_CAP
 *          0 = DPTX does not have the GTC capability.
 *          1 = DPTX has the GTC capability.
 *      4 TX_GTC_SLAVE_CAP
 *          0 = DPTX cannot operate as a GTC Slave.
 *          1 = DPTX can operate as a GTC Slave.
 */
#define DP_LINK_RATE_SET                       0x115   /* eDP 1.4 */
#define DP_LINK_RATE_SET_SHIFT                 0
#define DP_LINK_RATE_SET_MASK                  (7 << 0)
#define DP_TX_GTC_CAPABILITY                   0x115   /* DP 1.4 */
#define TX_GTC_CAP                             BIT(3)
#define TX_GTC_SLAVE_CAP                       BIT(4)

#define DP_RECEIVER_ALPM_CONFIG                0x116   /* eDP 1.4 */
#define DP_ALPM_ENABLE                         BIT(0)
#define DP_ALPM_LOCK_ERROR_IRQ_HPD_ENABLE      BIT(1) /* eDP 1.5 */
#define DP_ALPM_MODE_AUX_LESS                  BIT(2) /* eDP 1.5 */

#define DP_SINK_DEVICE_AUX_FRAME_SYNC_CONF     0x117   /* eDP 1.4 */
#define DP_AUX_FRAME_SYNC_ENABLE               BIT(0)
#define DP_IRQ_HPD_ENABLE                      BIT(1)

/* 00118h UPSTREAM_DEVICE_DP_PWR_NEED
 *      0 DP_PWR_NOT_NEEDED_BY_UPSTREAM_DEVICE
 *          0 = The upstream device needs DP_PWR provided by DPRX device through
 *              DP_PWR connector pin for operation.
 *          1 = The upstream device does not need DP_PWR provided by DPRX device
 *              through DP_PWR connector pin for operation.
 */
#define DP_UPSTREAM_DEVICE_DP_PWR_NEED         0x118   /* 1.2 */
#define DP_PWR_NOT_NEEDED                      BIT(0)

/* 00120h FEC_CONFIGURATION
 *      0 FEC_READY
 *    3:1 FEC_ERROR_COUNT_SEL
 *          000 = FEC_ERROR_COUNT_DIS.
 *          001 = UNCORRECTED_BLOCK_ERROR_COUNT.
 *          010 = CORRECTED_BLOCK_ERROR_COUNT.
 *          011 = BIT_ERROR_COUNT.
 *    5:4 LANE_SELECT
 *          00 = Lane 0.
 *          01 = Lane 1.
 *          10 = Lane 2.
 *          11 = Lane 3.
 */
#define DP_FEC_CONFIGURATION                   0x120    /* 1.4 */
#define DP_FEC_READY                           BIT(0)
#define DP_FEC_ERR_COUNT_SEL_MASK              (7 << 1)
#define DP_FEC_ERR_COUNT_DIS                   (0 << 1)
#define DP_FEC_UNCORR_BLK_ERROR_COUNT          BIT(1)
#define DP_FEC_CORR_BLK_ERROR_COUNT            (2 << 1)
#define DP_FEC_BIT_ERROR_COUNT                 (3 << 1)
#define DP_FEC_LANE_SELECT_MASK                (3 << 4)
#define DP_FEC_LANE_0_SELECT                   (0 << 4)
#define DP_FEC_LANE_1_SELECT                   BIT(4)
#define DP_FEC_LANE_2_SELECT                   (2 << 4)
#define DP_FEC_LANE_3_SELECT                   (3 << 4)

#define DP_SDP_ERROR_DETECTION_CONFIGURATION   0x121        /* DP 2.0 E11 */
#define DP_SDP_CRC16_128B132B_EN               BIT(0)

#define DP_TX_GTC_VALUE0                       0x154
#define DP_TX_GTC_VALUE1                       0x155
#define DP_TX_GTC_VALUE2                       0x156
#define DP_TX_GTC_VALUE3                       0x157

#define DP_RX_GTC_VALUE_PHASE_SKEW_EN          0x158
#define GTC_VALUE_PHASE_SKEW_EN                BIT(0)

#define DP_TX_GTC_PHASE_SKEW_OFFSET0           0x15a
#define DP_TX_GTC_PHASE_SKEW_OFFSET1           0x15b

#define DP_AUX_FRAME_SYNC_VALUE                0x15c   /* eDP 1.4 */
#define DP_AUX_FRAME_SYNC_VALID                BIT(0)

/* 00160h DSC ENABLE
 *      0 Decompression Enable
 *          0 = Decompression is not enabled.
 *          1 = Decompression is enabled.
 */
#define DP_DSC_ENABLE                          0x160   /* DP 1.4 */
#define DP_DECOMPRESSION_EN                    BIT(0)
#define DP_DSC_PASSTHROUGH_EN                  BIT(1)
#define DP_DSC_CONFIGURATION                   0x161        /* DP 2.0 */

#define DP_PSR_EN_CFG                          0x170
#define DP_PSR_ENABLE                          BIT(0)
#define DP_PSR_MAIN_LINK_ACTIVE                BIT(1)
#define DP_PSR_CRC_VERIFICATION                BIT(2)
#define DP_PSR_FRAME_CAPTURE                   BIT(3)
#define DP_PSR_SU_REGION_SCANLINE_CAPTURE      BIT(4) /* eDP 1.4a */
#define DP_PSR_IRQ_HPD_WITH_CRC_ERRORS         BIT(5) /* eDP 1.4a */
#define DP_PSR_ENABLE_PSR2                     BIT(6) /* eDP 1.4a */
#define DP_PSR_ENABLE_SU_REGION_ET             BIT(7) /* eDP 1.5 */

/* 001A0h ADAPTOR_CTRL
 *      0 FORCE_LOAD_SENSE
 */
#define DP_ADAPTER_CTRL                        0x1a0
#define DP_ADAPTER_CTRL_FORCE_LOAD_SENSE       BIT(0)

/* 001A1h BRANCH_DEVICE_CTRL
 *      0 Hot Plug/Hot Unplug Event Notification Type
 *          0 = HPD Long Pulse used for upstream notification (default).
 *          1 = IRQ_HPD used for upstream notification.
 */
#define DP_BRANCH_DEVICE_CTRL                  0x1a1
#define DP_BRANCH_DEVICE_IRQ_HPD               BIT(0)

#define PANEL_REPLAY_CONFIG                    0x1b0  /* DP 2.0 */
#define DP_PANEL_REPLAY_ENABLE                    BIT(0)
#define DP_PANEL_REPLAY_VSC_SDP_CRC_EN            BIT(1) /* eDP 1.5 */
#define DP_PANEL_REPLAY_UNRECOVERABLE_ERROR_EN    BIT(3)
#define DP_PANEL_REPLAY_RFB_STORAGE_ERROR_EN      BIT(4)
#define DP_PANEL_REPLAY_ACTIVE_FRAME_CRC_ERROR_EN BIT(5)
#define DP_PANEL_REPLAY_SU_ENABLE                 BIT(6)
#define DP_PANEL_REPLAY_ENABLE_SU_REGION_ET       BIT(7) /* DP 2.1 */

#define PANEL_REPLAY_CONFIG2                      0x1b1 /* eDP 1.5 */
#define DP_PANEL_REPLAY_SINK_REFRESH_RATE_UNLOCK_GRANTED BIT(0)
#define DP_PANEL_REPLAY_CRC_VERIFICATION                 BIT(1)
#define DP_PANEL_REPLAY_SU_Y_GRANULARITY_EXTENDED_EN     BIT(2)
#define DP_PANEL_REPLAY_SU_Y_GRANULARITY_EXTENDED_VAL_SEL_SHIFT 3
#define DP_PANEL_REPLAY_SU_Y_GRANULARITY_EXTENDED_VAL_SEL_MASK  (0xf << 3)
#define DP_PANEL_REPLAY_SU_REGION_SCANLINE_CAPTURE              BIT(7)

/* 001C0h PAYLOAD_ALLOCATE_SET
 *    6:0 VC Payload ID to Be Allocated
 *          ID of 0 indicates that the time slots are unallocated.
 */
#define DP_PAYLOAD_ALLOCATE_SET                0x1c0
#define DP_PAYLOAD_ALLOCATE_START_TIME_SLOT    0x1c1
#define DP_PAYLOAD_ALLOCATE_TIME_SLOT_COUNT    0x1c2

/* Link/Sink Device Status */
/* 00200h SINK_COUNT
 *  7,5:0 Total number of the Sink devices within this Sink device and those
 *        connected to the DFPs of this device.
 *      6 CP_READY
 */
#define DP_SINK_COUNT                          0x200
/* prior to 1.2 bit 7 was reserved mbz */
#define DP_GET_SINK_COUNT(x) ( \
{ \
	typeof(x) __x = x; \
	((((__x) & 0x80) >> 1) | ((__x) & 0x3f)) \
)
#define DP_SINK_CP_READY                       BIT(6)

/* 00201h DEVICE_SERVICE_IRQ_VECTOR
 *      1 AUTOMATED_TEST_REQUEST
 *          1 = Source device shall read DPCD Addresses 00218h through
 *              002BFh for the requested link test.
 *      2 CP_IRQ
 *      3 MCCS_IRQ
 *      4 DOWN_REP_MSG_RDY
 *      5 UP_REQ_MSG_RDY
 *      6 SINK_SPECIFIC_IRQ
 */
#define DP_DEVICE_SERVICE_IRQ_VECTOR           0x201
#define DP_REMOTE_CONTROL_COMMAND_PENDING      BIT(0)
#define DP_AUTOMATED_TEST_REQUEST              BIT(1)
#define DP_CP_IRQ                              BIT(2)
#define DP_MCCS_IRQ                            BIT(3)
#define DP_DOWN_REP_MSG_RDY                    BIT(4) /* 1.2 MST */
#define DP_UP_REQ_MSG_RDY                      BIT(5) /* 1.2 MST */
#define DP_SINK_SPECIFIC_IRQ                   BIT(6)

/* 00202h LANE0_1_STATUS
 *        Lane 0 and Lane 1 Status.
 *      0 LANE0_CR_DONE
 *      1 LANE0_CHANNEL_EQ_DONE
 *      2 LANE0_SYMBOL_LOCKED
 *      4 LANE1_CR_DONE
 *      5 LANE1_CHANNEL_EQ_DONE
 *      6 LANE1_SYMBOL_LOCKED
 */
#define DP_LANE0_1_STATUS                      0x202
#define DP_LANE2_3_STATUS                      0x203
#define DP_LANE_CR_DONE                        BIT(0)
#define DP_LANE_CHANNEL_EQ_DONE                BIT(1)
#define DP_LANE_SYMBOL_LOCKED                  BIT(2)

#define DP_CHANNEL_EQ_BITS \
		(DP_LANE_CR_DONE | \
		DP_LANE_CHANNEL_EQ_DONE | \
		DP_LANE_SYMBOL_LOCKED)

/* 00204h LANE_ALIGN_STATUS_UPDATED
 *      0 INTERLANE_ALIGN_DONE
 *      1 POST_LT_ADJ_REQ_IN_PROGRESS
 *      6 DOWNSTREAM_PORT_STATUS_CHANGED
 *      7 LINK_STATUS_UPDATED
 */
#define DP_LANE_ALIGN_STATUS_UPDATED              0x204
#define DP_INTERLANE_ALIGN_DONE                   BIT(0)
#define DP_128B132B_DPRX_EQ_INTERLANE_ALIGN_DONE  BIT(2) /* 2.0 E11 */
#define DP_128B132B_DPRX_CDS_INTERLANE_ALIGN_DONE BIT(3) /* 2.0 E11 */
#define DP_128B132B_LT_FAILED                     BIT(4) /* 2.0 E11 */
#define DP_DOWNSTREAM_PORT_STATUS_CHANGED         BIT(6)
#define DP_LINK_STATUS_UPDATED                    BIT(7)

/* 00205h SINK_STATUS
 *      0 RECEIVE_PORT_0_STATUS
 *          0 = Sink device is out of synchronization.
 *          1 = Sink device is in synchronization.
 *      1 RECEIVE_PORT_1_STATUS
 */
#define DP_SINK_STATUS                         0x205
#define DP_RECEIVE_PORT_0_STATUS               BIT(0)
#define DP_RECEIVE_PORT_1_STATUS               BIT(1)
#define DP_STREAM_REGENERATION_STATUS          BIT(2) /* 2.0 */
#define DP_INTRA_HOP_AUX_REPLY_INDICATION      BIT(3) /* 2.0 */

/* 00206h ADJUST_REQUEST_LANE0_1
 *    1:0 VOLTAGE_SWING_LANE0
 *          00 = Level 0
 *          01 = Level 1
 *          10 = Level 2
 *          11 = Level 3
 *    3:2 PRE-EMPHASIS_LANE0
 *    5:4 VOLTAGE_SWING_LANE1
 *    7:6 PRE-EMPHASIS_LANE1
 */
#define DP_ADJUST_REQUEST_LANE0_1              0x206
#define DP_ADJUST_REQUEST_LANE2_3              0x207
#define DP_ADJUST_VOLTAGE_SWING_LANE0_MASK     0x03
#define DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT    0
#define DP_ADJUST_PRE_EMPHASIS_LANE0_MASK      0x0c
#define DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT     2
#define DP_ADJUST_VOLTAGE_SWING_LANE1_MASK     0x30
#define DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT    4
#define DP_ADJUST_PRE_EMPHASIS_LANE1_MASK      0xc0
#define DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT     6

/* DP 2.0 128b/132b Link Layer */
#define DP_ADJUST_TX_FFE_PRESET_LANE0_MASK     (0xf << 0)
#define DP_ADJUST_TX_FFE_PRESET_LANE0_SHIFT    0
#define DP_ADJUST_TX_FFE_PRESET_LANE1_MASK     (0xf << 4)
#define DP_ADJUST_TX_FFE_PRESET_LANE1_SHIFT    4

#define DP_ADJUST_REQUEST_POST_CURSOR2         0x20c
#define DP_ADJUST_POST_CURSOR2_LANE0_MASK      0x03
#define DP_ADJUST_POST_CURSOR2_LANE0_SHIFT     0
#define DP_ADJUST_POST_CURSOR2_LANE1_MASK      0x0c
#define DP_ADJUST_POST_CURSOR2_LANE1_SHIFT     2
#define DP_ADJUST_POST_CURSOR2_LANE2_MASK      0x30
#define DP_ADJUST_POST_CURSOR2_LANE2_SHIFT     4
#define DP_ADJUST_POST_CURSOR2_LANE3_MASK      0xc0
#define DP_ADJUST_POST_CURSOR2_LANE3_SHIFT     6

/* 00218h TEST_REQUEST
 *      0 TEST_LINK_TRAINING
 *      1 TEST_PATTERN
 *      2 TEST_EDID_READ
 *      3 PHY_TEST_PATTERN
 */
#define DP_TEST_REQUEST                        0x218
#define DP_TEST_LINK_TRAINING                  BIT(0)
#define DP_TEST_LINK_VIDEO_PATTERN             BIT(1)
#define DP_TEST_LINK_EDID_READ                 BIT(2)
#define DP_TEST_LINK_PHY_TEST_PATTERN          BIT(3) /* DPCD >= 1.1 */
#define DP_TEST_LINK_FAUX_PATTERN              BIT(4) /* DPCD >= 1.2 */
#define DP_TEST_LINK_AUDIO_PATTERN             BIT(5) /* DPCD >= 1.2 */
#define DP_TEST_LINK_AUDIO_DISABLED_VIDEO      BIT(6) /* DPCD >= 1.2 */

/* 00219h TEST_LINK_RATE
 *    7:0   06h = 1.62Gbps/lane.
 *          0Ah = 2.7Gbps/lane.
 *          14h = 5.4Gbps/lane.
 *          1Eh = 8.1Gbps/lane.
 */
#define DP_TEST_LINK_RATE                      0x219
#define DP_LINK_RATE_162                       (0x6)
#define DP_LINK_RATE_27                        (0xa)

/* 00220h TEST_LANE_COUNT
 *    4:0   01h = One lane.
 *          02h = Two lanes.
 *          04h = Four lanes.
 */
#define DP_TEST_LANE_COUNT                     0x220

/* 00221h TEST_PATTERN
 *          00h = No test pattern transmitted.
 *          01h = Color ramps.
 *          02h = Black and white vertical lines.
 *          03h = Color square.
 */
#define DP_TEST_PATTERN                        0x221
#define DP_NO_TEST_PATTERN                     0x0
#define DP_COLOR_RAMP                          0x1
#define DP_BLACK_AND_WHITE_VERTICAL_LINES      0x2
#define DP_COLOR_SQUARE                        0x3

/* 00222h TEST_H_TOTAL
 */
#define DP_TEST_H_TOTAL_HI                     0x222
#define DP_TEST_H_TOTAL_LO                     0x223

/* 00224h TEST_V_TOTAL
 */
#define DP_TEST_V_TOTAL_HI                     0x224
#define DP_TEST_V_TOTAL_LO                     0x225

#define DP_TEST_H_START_HI                     0x226
#define DP_TEST_H_START_LO                     0x227

#define DP_TEST_V_START_HI                     0x228
#define DP_TEST_V_START_LO                     0x229

/* 0022Ah TEST_HSYNC
 *      7 TEST_HSYNC_POLARITY
 *    6:0 TEST_HSYNC_WIDTH14:8
 */
#define DP_TEST_HSYNC_HI                       0x22A
#define DP_TEST_HSYNC_POLARITY                 BIT(7)
#define DP_TEST_HSYNC_WIDTH_HI_MASK            (127 << 0)
#define DP_TEST_HSYNC_WIDTH_LO                 0x22B

#define DP_TEST_VSYNC_HI                       0x22C
#define DP_TEST_VSYNC_POLARITY                 BIT(7)
#define DP_TEST_VSYNC_WIDTH_HI_MASK            (127 << 0)
#define DP_TEST_VSYNC_WIDTH_LO                 0x22D

#define DP_TEST_H_WIDTH_HI                     0x22E
#define DP_TEST_H_WIDTH_LO                     0x22F

#define DP_TEST_V_HEIGHT_HI                    0x230
#define DP_TEST_V_HEIGHT_LO                    0x231

/* 00232h TEST_MISC
 */
#define DP_TEST_MISC0                          0x232
#define DP_TEST_SYNC_CLOCK                     BIT(0)
#define DP_TEST_COLOR_FORMAT_MASK              (3 << 1)
#define DP_TEST_COLOR_FORMAT_SHIFT             1
#define DP_COLOR_FORMAT_RGB                    (0 << 1)
#define DP_COLOR_FORMAT_YCbCr422               BIT(1)
#define DP_COLOR_FORMAT_YCbCr444               (2 << 1)
#define DP_TEST_DYNAMIC_RANGE_VESA             (0 << 3)
#define DP_TEST_DYNAMIC_RANGE_CEA              BIT(3)
#define DP_TEST_YCBCR_COEFFICIENTS             BIT(4)
#define DP_YCBCR_COEFFICIENTS_ITU601           (0 << 4)
#define DP_YCBCR_COEFFICIENTS_ITU709           BIT(4)
#define DP_TEST_BIT_DEPTH_MASK                 (7 << 5)
#define DP_TEST_BIT_DEPTH_SHIFT                5
#define DP_TEST_BIT_DEPTH_6                    (0 << 5)
#define DP_TEST_BIT_DEPTH_8                    BIT(5)
#define DP_TEST_BIT_DEPTH_10                   (2 << 5)
#define DP_TEST_BIT_DEPTH_12                   (3 << 5)
#define DP_TEST_BIT_DEPTH_16                   (4 << 5)

/* 00233h TEST_MISC
 */
#define DP_TEST_MISC1                          0x233
#define DP_TEST_REFRESH_DENOMINATOR            BIT(0)
#define DP_TEST_INTERLACED                     BIT(1)

/* 00234h TEST_REFRESH_RATE_NUMERATOR
 *        Refresh rate = TEST_REFRESH_RATE_NUMERATOR / TEST_REFRESH_DENOMINATOR
 */
#define DP_TEST_REFRESH_RATE_NUMERATOR         0x234

#define DP_TEST_CRC_R_CR                       0x240
#define DP_TEST_CRC_G_Y                        0x242
#define DP_TEST_CRC_B_CB                       0x244

/* 00246h TEST_SINK_MISC
 *    3:0 TEST_CRC_COUNT
 *      5 TEST_CRC_SUPPORTED
 */
#define DP_TEST_SINK_MISC                      0x246
#define DP_TEST_CRC_SUPPORTED                  BIT(5)
#define DP_TEST_COUNT_MASK                     0xf

/* 00248h PHY_TEST_PATTERN
 *    2:0 PHY_TEST_PATTERN_SEL
 *          000 = No test pattern selected.
 *          001 = D10.2 without scrambling.
 *          010 = Symbol_Error_Measurement_Count.
 *          011 = PRBS7.
 *          100 = 80-bit custom pattern transmitted.
 *          101 = CP2520 Pattern 1.
 *          110 = CP2520 Pattern 2.
 *          111 = CP2520 Pattern 3 (which is TPS4).
 */
#define DP_PHY_TEST_PATTERN                    0x248
#define DP_PHY_TEST_PATTERN_SEL_MASK           0x7
#define DP_PHY_TEST_PATTERN_NONE               0x0
#define DP_PHY_TEST_PATTERN_D10_2              0x1
#define DP_PHY_TEST_PATTERN_ERROR_COUNT        0x2
#define DP_PHY_TEST_PATTERN_PRBS7              0x3
#define DP_PHY_TEST_PATTERN_80BIT_CUSTOM       0x4
#define DP_PHY_TEST_PATTERN_CP2520             0x5

#define DP_PHY_SQUARE_PATTERN                  0x249

#define DP_TEST_HBR2_SCRAMBLER_RESET           0x24A

/* 00250h TEST_80BIT_CUSTOM_PATTERN
 *    7:0 TEST_80BIT_CUSTOM_PATTERN Value 7:0
 */
#define DP_TEST_80BIT_CUSTOM_PATTERN_7_0       0x250
#define DP_TEST_80BIT_CUSTOM_PATTERN_15_8      0x251
#define DP_TEST_80BIT_CUSTOM_PATTERN_23_16     0x252
#define DP_TEST_80BIT_CUSTOM_PATTERN_31_24     0x253
#define DP_TEST_80BIT_CUSTOM_PATTERN_39_32     0x254
#define DP_TEST_80BIT_CUSTOM_PATTERN_47_40     0x255
#define DP_TEST_80BIT_CUSTOM_PATTERN_55_48     0x256
#define DP_TEST_80BIT_CUSTOM_PATTERN_63_56     0x257
#define DP_TEST_80BIT_CUSTOM_PATTERN_71_64     0x258
#define DP_TEST_80BIT_CUSTOM_PATTERN_79_72     0x259

/* 0025Ah CONTINUOUS_80BIT_PATTERN_FROM_DPRX_AUX_CH_CAPABILITY
 *      0 CONTINUOUS_80BIT_PATTERN_FROM_DPRX_AUX_CH_CAP
 */
#define DP_TEST_80BIT_AUX_CH_CAP               0x25A
#define DP_TEST_80BIT_AUX_CH_CAP_SUP           BIT(0)

/* 0025Bh DP_TEST_80BIT_PTTN_AUXCH_CTRL
 *      0 CONTINUOUS_80BIT_PATTERN_FROM_DPRX_AUX_CH_EN
 *    2:1 DURATION_CTRL  00/15seconds 01/30s 10/60s 11/120s
 */
#define DP_TEST_80BIT_PTTN_AUXCH_CTRL           0x25B
#define DP_TEST_80BIT_PTTN_AUXCH_CTRL_EN        BIT(0)
#define DP_TEST_DURATION_CTRL                   GENMASK(2, 1)

/* 00260h TEST_RESPONSE
 *      0 TEST_ACK
 *      1 TEST_NAK
 *      2 TEST_EDID_CHECKSUM_WRITE
 */
#define DP_TEST_RESPONSE                       0x260
#define DP_TEST_ACK                            BIT(0)
#define DP_TEST_NAK                            BIT(1)
#define DP_TEST_EDID_CHECKSUM_WRITE            BIT(2)

/* 00261h TEST_EDID_CHECKSUM
 *        In TEST_EDID mode, the checksum of the last EDID block that was read is written here.
 */
#define DP_TEST_EDID_CHECKSUM                  0x261

/* 00270h TEST_SINK
 *      0 TEST_SINK_START
 *    5:4 PHY_SINK_TEST_LANE_SEL
 *      7 PHY_SINK_TEST_LANE_EN
 */
#define DP_TEST_SINK                           0x270
#define DP_TEST_SINK_START                     BIT(0)

/* 00271h TEST_AUDIO_MODE
 *    3:0 TEST_AUDIO_SAMPLING_RATE
 *    7:4 TEST_AUDIO_CHANNEL_COUNT
 */
#define DP_TEST_AUDIO_MODE                     0x271

/* 00272h TEST_AUDIO_PATTERN_TYPE
 *    7:0 TEST_AUDIO_PATTERN
 *          00h = Operator-defined.
 *          01h = Sawtooth Pattern.
 */
#define DP_TEST_AUDIO_PATTERN_TYPE             0x272

/* 00273h TEST_AUDIO_PERIOD_CH_1
 *    3:0 TEST_AUDIO_PERIOD_CH1
 *          0000 = Unused.
 *          0001 = 3 Samples.
 *          0010 = 6 Samples.
 *          0011 = 12 Samples.
 *          0100 = 24 Samples.
 *          0101 = 48 Samples.
 *          0110 = 96 Samples.
 *          0111 = 192 Samples.
 *          1000 = 384 Samples.
 *          1001 = 768 Samples.
 *          1010 = 1536 Samples.
 */
#define DP_TEST_AUDIO_PERIOD_CH1               0x273
#define DP_TEST_AUDIO_PERIOD_CH2               0x274
#define DP_TEST_AUDIO_PERIOD_CH3               0x275
#define DP_TEST_AUDIO_PERIOD_CH4               0x276
#define DP_TEST_AUDIO_PERIOD_CH5               0x277
#define DP_TEST_AUDIO_PERIOD_CH6               0x278
#define DP_TEST_AUDIO_PERIOD_CH7               0x279
#define DP_TEST_AUDIO_PERIOD_CH8               0x27A

/* 00280h FEC_STATUS
 *      0 FEC_DECODE_EN_DETECTED
 *      1 FEC_DECODE_DIS_DETECTED
 */
#define DP_FEC_STATUS                          0x280    /* 1.4 */
#define DP_FEC_DECODE_EN_DETECTED              BIT(0)
#define DP_FEC_DECODE_DIS_DETECTED             BIT(1)

/* 00281h FEC_ERROR_COUNT
 *    7:0 FEC_ERROR_COUNT7:0
 */
#define DP_FEC_ERROR_COUNT_LSB                 0x0281    /* 1.4 */

/* 00282h FEC_ERROR_COUNT
 *    6:0 FEC_ERROR_COUNT14:8
 *      7 FEC_ERROR_COUNT_VALID
 */
#define DP_FEC_ERROR_COUNT_MSB                 0x0282    /* 1.4 */
#define DP_FEC_ERROR_COUNT_MASK                0x7F
#define DP_FEC_ERR_COUNT_VALID                 BIT(7)

/* 002C0h PAYLOAD_TABLE_UPDATE_STATUS
 *      0 VC Payload ID Table Updated
 *          0 = Not updated since the last time that this bit was cleared.
 *          1 = Updated, cleared to 0 when the DP Source device writes 1.
 *      1 ACT Handled
 *          0 = ACT is not handled since the last time that this bit was read.
 *          1 = ACT is handled, cleared to 0 when the VC Payload ID Table
 *              Updated bit (bit 0) is set to 1.
 */
#define DP_PAYLOAD_TABLE_UPDATE_STATUS         0x2c0   /* 1.2 MST */
#define DP_PAYLOAD_TABLE_UPDATED               BIT(0)
#define DP_PAYLOAD_ACT_HANDLED                 BIT(1)

#define DP_VC_PAYLOAD_ID_SLOT_1                0x2c1   /* 1.2 MST */
#define DP_VC_PAYLOAD_ID_SLOT_2                0x2c2   /* 1.2 MST */
#define DP_VC_PAYLOAD_ID_SLOT_3                0x2c3   /* 1.2 MST */

/* 00300h IEEE_OUI
 * For IEEE OUI 00-1B-C5, this field is cleared to 00h
 */
/* Source Device-specific */
#define DP_SOURCE_OUI                          0x300

/* 00400h IEEE_OUI
 * For IEEE OUI 00-1B-C5, this field is cleared to 00h.
 */
/* Sink Device-specific */
#define DP_SINK_OUI                            0x400

/* Branch Device-specific */
#define DP_BRANCH_OUI                          0x500
#define DP_BRANCH_ID                           0x503
#define DP_BRANCH_REVISION_START               0x509
#define DP_BRANCH_HW_REV                       0x509
#define DP_BRANCH_SW_REV                       0x50A

/* Link/Sink Device Power Control */
/* 00600h SET_POWER & SET_DP_PWR_VOLTAGE
 *    2:0 SET_POWER_STATE
 *          001 = Set local Sink device and all downstream Sink devices to D0 (normal
 *                operation mode).
 *          010 = Set local Sink device and all downstream Sink devices to D3 (power-down
 *                mode).
 *          101 = Set Main-Link for local Sink device and all downstream Sink devices to D3
 *                (power-down mode), keep AUX block fully powered, ready to reply within a
 *                Response Timeout period of 300us.
 */
#define DP_SET_POWER                           0x600
#define DP_SET_POWER_D0                        0x1
#define DP_SET_POWER_D3                        0x2
#define DP_SET_POWER_MASK                      0x3
#define DP_SET_POWER_D3_AUX_ON                 0x5

/* eDP-specific */
#define DP_EDP_DPCD_REV                        0x700    /* eDP 1.2 */
#define DP_EDP_11                              0x00
#define DP_EDP_12                              0x01
#define DP_EDP_13                              0x02
#define DP_EDP_14                              0x03
#define DP_EDP_14a                             0x04    /* eDP 1.4a */
#define DP_EDP_14b                             0x05    /* eDP 1.4b */

#define DP_EDP_GENERAL_CAP_1                   0x701
#define DP_EDP_TCON_BACKLIGHT_ADJUSTMENT_CAP   BIT(0)
#define DP_EDP_BACKLIGHT_PIN_ENABLE_CAP        BIT(1)
#define DP_EDP_BACKLIGHT_AUX_ENABLE_CAP        BIT(2)
#define DP_EDP_PANEL_SELF_TEST_PIN_ENABLE_CAP  BIT(3)
#define DP_EDP_PANEL_SELF_TEST_AUX_ENABLE_CAP  BIT(4)
#define DP_EDP_FRC_ENABLE_CAP                  BIT(5)
#define DP_EDP_COLOR_ENGINE_CAP                BIT(6)
#define DP_EDP_SET_POWER_CAP                   BIT(7)

#define DP_EDP_BACKLIGHT_ADJUSTMENT_CAP            0x702
#define DP_EDP_BACKLIGHT_BRIGHTNESS_PWM_PIN_CAP    BIT(0)
#define DP_EDP_BACKLIGHT_BRIGHTNESS_AUX_SET_CAP    BIT(1)
#define DP_EDP_BACKLIGHT_BRIGHTNESS_BYTE_COUNT     BIT(2)
#define DP_EDP_BACKLIGHT_AUX_PWM_PRODUCT_CAP       BIT(3)
#define DP_EDP_BACKLIGHT_FREQ_PWM_PIN_PASSTHRU_CAP BIT(4)
#define DP_EDP_BACKLIGHT_FREQ_AUX_SET_CAP          BIT(5)
#define DP_EDP_DYNAMIC_BACKLIGHT_CAP               BIT(6)
#define DP_EDP_VBLANK_BACKLIGHT_UPDATE_CAP         BIT(7)

#define DP_EDP_GENERAL_CAP_2                   0x703
#define DP_EDP_OVERDRIVE_ENGINE_ENABLED        BIT(0)
#define DP_EDP_PANEL_LUMINANCE_CONTROL_CAPABLE BIT(4)

#define DP_EDP_GENERAL_CAP_3                   0x704    /* eDP 1.4 */
#define DP_EDP_X_REGION_CAP_MASK               (0xf << 0)
#define DP_EDP_X_REGION_CAP_SHIFT              0
#define DP_EDP_Y_REGION_CAP_MASK               (0xf << 4)
#define DP_EDP_Y_REGION_CAP_SHIFT              4

#define DP_EDP_DISPLAY_CONTROL_REGISTER        0x720
#define DP_EDP_BACKLIGHT_ENABLE                BIT(0)
#define DP_EDP_BLACK_VIDEO_ENABLE              BIT(1)
#define DP_EDP_FRC_ENABLE                      BIT(2)
#define DP_EDP_COLOR_ENGINE_ENABLE             BIT(3)
#define DP_EDP_VBLANK_BACKLIGHT_UPDATE_ENABLE  BIT(7)

#define DP_EDP_BACKLIGHT_MODE_SET_REGISTER            0x721
#define DP_EDP_BACKLIGHT_CONTROL_MODE_MASK            (3 << 0)
#define DP_EDP_BACKLIGHT_CONTROL_MODE_PWM             (0 << 0)
#define DP_EDP_BACKLIGHT_CONTROL_MODE_PRESET          BIT(0)
#define DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD            (2 << 0)
#define DP_EDP_BACKLIGHT_CONTROL_MODE_PRODUCT         (3 << 0)
#define DP_EDP_BACKLIGHT_FREQ_PWM_PIN_PASSTHRU_ENABLE BIT(2)
#define DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE          BIT(3)
#define DP_EDP_DYNAMIC_BACKLIGHT_ENABLE               BIT(4)
#define DP_EDP_REGIONAL_BACKLIGHT_ENABLE              BIT(5)
#define DP_EDP_UPDATE_REGION_BRIGHTNESS               BIT(6) /* eDP 1.4 */
#define DP_EDP_PANEL_LUMINANCE_CONTROL_ENABLE         BIT(7)

#define DP_EDP_BACKLIGHT_BRIGHTNESS_MSB        0x722
#define DP_EDP_BACKLIGHT_BRIGHTNESS_LSB        0x723

#define DP_EDP_PWMGEN_BIT_COUNT                0x724
#define DP_EDP_PWMGEN_BIT_COUNT_CAP_MIN        0x725
#define DP_EDP_PWMGEN_BIT_COUNT_CAP_MAX        0x726
#define DP_EDP_PWMGEN_BIT_COUNT_MASK           (0x1f << 0)

#define DP_EDP_BACKLIGHT_CONTROL_STATUS        0x727

#define DP_EDP_BACKLIGHT_FREQ_SET              0x728
#define DP_EDP_BACKLIGHT_FREQ_BASE_KHZ         27000

#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MSB      0x72a
#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_MID      0x72b
#define DP_EDP_BACKLIGHT_FREQ_CAP_MIN_LSB      0x72c

#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MSB      0x72d
#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_MID      0x72e
#define DP_EDP_BACKLIGHT_FREQ_CAP_MAX_LSB      0x72f

#define DP_EDP_DBC_MINIMUM_BRIGHTNESS_SET      0x732
#define DP_EDP_DBC_MAXIMUM_BRIGHTNESS_SET      0x733
#define DP_EDP_PANEL_TARGET_LUMINANCE_VALUE    0x734

#define DP_EDP_REGIONAL_BACKLIGHT_BASE         0x740    /* eDP 1.4 */
#define DP_EDP_REGIONAL_BACKLIGHT_0            0x741    /* eDP 1.4 */

#define DP_EDP_MSO_LINK_CAPABILITIES           0x7a4    /* eDP 1.4 */
#define DP_EDP_MSO_NUMBER_OF_LINKS_MASK        (7 << 0)
#define DP_EDP_MSO_NUMBER_OF_LINKS_SHIFT       0
#define DP_EDP_MSO_INDEPENDENT_LINK_BIT        BIT(3)

/* Sideband MSG Buffers */
/* 01000h DOWN_REQ
 *        Write/Read
 */
#define DP_SIDEBAND_MSG_DOWN_REQ_BASE          0x1000   /* 1.2 MST */
/* 01200h UP_REQ
 *        Write/Read
 */
#define DP_SIDEBAND_MSG_UP_REP_BASE            0x1200   /* 1.2 MST */
/* 01400h DOWN_REP
 *        Read Only
 */
#define DP_SIDEBAND_MSG_DOWN_REP_BASE          0x1400   /* 1.2 MST */
/* 01600h UP_REP
 *        Read Only
 */
#define DP_SIDEBAND_MSG_UP_REQ_BASE            0x1600   /* 1.2 MST */

/* 02002h SINK_COUNT_ESI
 * Refer to 00200h
 */
/* DPRX Event Status Indicator */
#define DP_SINK_COUNT_ESI                      0x2002   /* same as 0x200 */

/* 02003h DEVICE_SERVICE_IRQ_VECTOR_ESI0
 * Refer to 00201h
 */
#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI0      0x2003   /* same as 0x201 */

/* 02004h DEVICE_SERVICE_IRQ_VECTOR_ESI1
 *      0 RX_GTC_MSTR_REQ_STATUS_CHANGE
 *      1 LOCK_ACQUISITION_REQUEST
 *      2 CEC_IRQ
 */
#define DP_DEVICE_SERVICE_IRQ_VECTOR_ESI1      0x2004   /* 1.2 */
#define DP_RX_GTC_MSTR_REQ_STATUS_CHANGE       BIT(0)
#define DP_LOCK_ACQUISITION_REQUEST            BIT(1)
#define DP_CEC_IRQ                             BIT(2)

/* 02005h LINK_SERVICE_IRQ_VECTOR_ESI0
 *      0 RX_CAP_CHANGED
 *      1 LINK_STATUS_CHANGED
 *      2 STREAM_STATUS_CHANGED
 *      3 HDMI_LINK_STATUS_CHANGED
 *      4 CONNECTED_OFF_ENTRY_REQUESTED
 */
#define DP_LINK_SERVICE_IRQ_VECTOR_ESI0        0x2005   /* 1.2 */
#define RX_CAP_CHANGED                         BIT(0)
#define LINK_STATUS_CHANGED                    BIT(1)
#define STREAM_STATUS_CHANGED                  BIT(2)
#define HDMI_LINK_STATUS_CHANGED               BIT(3)
#define CONNECTED_OFF_ENTRY_REQUESTED          BIT(4)
#define DP_TUNNELING_IRQ                       BIT(5)

#define DP_PSR_ERROR_STATUS                    0x2006
#define DP_PSR_LINK_CRC_ERROR                  BIT(0)
#define DP_PSR_RFB_STORAGE_ERROR               BIT(1)
#define DP_PSR_VSC_SDP_UNCORRECTABLE_ERROR     BIT(2) /* eDP 1.4 */

#define DP_PSR_ESI                             0x2007
#define DP_PSR_CAPS_CHANGE                     BIT(0)

#define DP_PSR_STATUS                          0x2008
#define DP_PSR_SINK_INACTIVE                   0
#define DP_PSR_SINK_ACTIVE_SRC_SYNCED          1
#define DP_PSR_SINK_ACTIVE_RFB                 2
#define DP_PSR_SINK_ACTIVE_SINK_SYNCED         3
#define DP_PSR_SINK_ACTIVE_RESYNC              4
#define DP_PSR_SINK_INTERNAL_ERROR             7
#define DP_PSR_SINK_STATE_MASK                 0x07

#define DP_SYNCHRONIZATION_LATENCY_IN_SINK     0x2009 /* edp 1.4 */
#define DP_MAX_RESYNC_FRAME_COUNT_MASK         (0xf << 0)
#define DP_MAX_RESYNC_FRAME_COUNT_SHIFT        0
#define DP_LAST_ACTUAL_SYNCHRONIZATION_LATENCY_MASK  (0xf << 4)
#define DP_LAST_ACTUAL_SYNCHRONIZATION_LATENCY_SHIFT 4

#define DP_LAST_RECEIVED_PSR_SDP               0x200a /* eDP 1.2 */
#define DP_PSR_STATE_BIT                       BIT(0) /* eDP 1.2 */
#define DP_UPDATE_RFB_BIT                      BIT(1) /* eDP 1.2 */
#define DP_CRC_VALID_BIT                       BIT(2) /* eDP 1.2 */
#define DP_SU_VALID                            BIT(3) /* eDP 1.4 */
#define DP_FIRST_SCAN_LINE_SU_REGION           BIT(4) /* eDP 1.4 */
#define DP_LAST_SCAN_LINE_SU_REGION            BIT(5) /* eDP 1.4 */
#define DP_Y_COORDINATE_VALID                  BIT(6) /* eDP 1.4a */

#define DP_RECEIVER_ALPM_STATUS                0x200b  /* eDP 1.4 */
#define DP_ALPM_LOCK_TIMEOUT_ERROR             BIT(0)

#define DP_LANE0_1_STATUS_ESI                  0x200c /* status same as 0x202 */
#define DP_LANE2_3_STATUS_ESI                  0x200d /* status same as 0x203 */
#define DP_LANE_ALIGN_STATUS_UPDATED_ESI       0x200e /* status same as 0x204 */
#define DP_SINK_STATUS_ESI                     0x200f /* status same as 0x205 */

#define DP_PANEL_REPLAY_ERROR_STATUS           0x2020  /* DP 2.1*/
#define DP_PANEL_REPLAY_LINK_CRC_ERROR                BIT(0)
#define DP_PANEL_REPLAY_RFB_STORAGE_ERROR             BIT(1)
#define DP_PANEL_REPLAY_VSC_SDP_UNCORRECTABLE_ERROR   BIT(2)

#define DP_SINK_DEVICE_PR_AND_FRAME_LOCK_STATUS 0x2022  /* DP 2.1 */
#define DP_SINK_DEVICE_PANEL_REPLAY_STATUS_MASK (7 << 0)
#define DP_SINK_FRAME_LOCKED_SHIFT              3
#define DP_SINK_FRAME_LOCKED_MASK               (3 << 3)
#define DP_SINK_FRAME_LOCKED_STATUS_VALID_SHIFT 5
#define DP_SINK_FRAME_LOCKED_STATUS_VALID_MASK  BIT(5)

/* Extended Receiver Capability: See DP_DPCD_REV for definitions */
#define DP_DP13_DPCD_REV                       0x2200

/* 02210h DPRX_FEATURE_ENUMERATION_LIST
 *      0 GTC_CAP
 *      1 SST_SPLIT_SDP_CAP
 *      2 AV_SYNC_CAP
 *      3 VSC_SDP_EXTENSION_FOR_COLORIMETRY_SUPPORTED
 *      4 VSC_EXT_VESA_SDP_SUPPORTED
 *      5 VSC_EXT_VESA_SDP_CHAINING_SUPPORTED
 *      6 VSC_EXT_CEA_SDP_SUPPORTED
 *      7 VSC_EXT_CEA_SDP_CHAINING_SUPPORTED
 */
#define DP_DPRX_FEATURE_ENUMERATION_LIST         0x2210  /* DP 1.3 */
#define DP_GTC_CAP                               BIT(0)  /* DP 1.3 */
#define DP_SST_SPLIT_SDP_CAP                     BIT(1)  /* DP 1.4 */
#define DP_AV_SYNC_CAP                           BIT(2)  /* DP 1.3 */
#define DP_VSC_SDP_EXT_FOR_COLORIMETRY_SUPPORTED BIT(3)  /* DP 1.3 */
#define DP_VSC_EXT_VESA_SDP_SUPPORTED            BIT(4)  /* DP 1.4 */
#define DP_VSC_EXT_VESA_SDP_CHAINING_SUPPORTED   BIT(5)  /* DP 1.4 */
#define DP_VSC_EXT_CEA_SDP_SUPPORTED             BIT(6)  /* DP 1.4 */
#define DP_VSC_EXT_CEA_SDP_CHAINING_SUPPORTED    BIT(7)  /* DP 1.4 */

#define DP_DPRX_FEATURE_ENUMERATION_LIST_CONT_1  0x2214 /* 2.0 E11 */
#define DP_ADAPTIVE_SYNC_SDP_SUPPORTED           BIT(0)
#define DP_ADAPTIVE_SYNC_SDP_OPERATION_MODE      GENMASK(1, 0)
#define DP_ADAPTIVE_SYNC_SDP_LENGTH              GENMASK(5, 0)
#define DP_AS_SDP_FIRST_HALF_LINE_OR_3840_PIXEL_CYCLE_WINDOW_NOT_SUPPORTED BIT(1)
#define DP_VSC_EXT_SDP_FRAMEWORK_VERSION_1_SUPPORTED  BIT(4)

#define DP_128B132B_SUPPORTED_LINK_RATES       0x2215 /* 2.0 */
#define DP_UHBR10                              BIT(0)
#define DP_UHBR20                              BIT(1)
#define DP_UHBR13_5                            BIT(2)

#define DP_128B132B_TRAINING_AUX_RD_INTERVAL          0x2216 /* 2.0 */
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_1MS_UNIT BIT(7)
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK     0x7f
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_400_US   0x00
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_4_MS     0x01
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_8_MS     0x02
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_12_MS    0x03
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_16_MS    0x04
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_32_MS    0x05
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_64_MS    0x06

#define DP_TEST_264BIT_CUSTOM_PATTERN_7_0      0x2230
#define DP_TEST_264BIT_CUSTOM_PATTERN_263_256  0x2250

/* DSC Extended Capability Branch Total DSC Resources */
#define DP_DSC_SUPPORT_AND_DSC_DECODER_COUNT       0x2260        /* 2.0 */
#define DP_DSC_DECODER_COUNT_MASK                  (0b111 << 5)
#define DP_DSC_DECODER_COUNT_SHIFT                 5
#define DP_DSC_MAX_SLICE_COUNT_AND_AGGREGATION_0   0x2270        /* 2.0 */
#define DP_DSC_DECODER_0_MAXIMUM_SLICE_COUNT_MASK  BIT(0)
#define DP_DSC_DECODER_0_AGGREGATION_SUPPORT_MASK  (0b111 << 1)
#define DP_DSC_DECODER_0_AGGREGATION_SUPPORT_SHIFT 1

/* Protocol Converter Extension */
/* HDMI CEC tunneling over AUX DP 1.3 section 5.3.3.3.1 DPCD 1.4+ */
/* 03000h CEC_TUNNELING_CAPABILITY
 *      0 CEC_TUNNELING_CAPABLE
 *      1 CEC_SNOOPING_CAPABLE
 *      2 CEC_MULTIPLE_LA_CAPABLE
 */
#define DP_CEC_TUNNELING_CAPABILITY            0x3000
#define DP_CEC_TUNNELING_CAPABLE               BIT(0)
#define DP_CEC_SNOOPING_CAPABLE                BIT(1)
#define DP_CEC_MULTIPLE_LA_CAPABLE             BIT(2)

/* 03001h CEC_TUNNELING_CONTROL
 *      0 CEC_TUNNELING_ENABLE
 *      1 CEC_SNOOPING_ENABLE
 */
#define DP_CEC_TUNNELING_CONTROL               0x3001
#define DP_CEC_TUNNELING_ENABLE                BIT(0)
#define DP_CEC_SNOOPING_ENABLE                 BIT(1)

/* 03002h CEC_RX_MESSAGE_INFO
 *    3:0 CEC_RX_MESSAGE_LEN
 *      4 CEC_RX_MESSAGE_HPD_STATE
 *      5 CEC_RX_MESSAGE_HPD_LOST
 *      6 CEC_RX_MESSAGE_ACKED
 *      7 CEC_RX_MESSAGE_ENDED
 */
#define DP_CEC_RX_MESSAGE_INFO                 0x3002
#define DP_CEC_RX_MESSAGE_LEN_MASK             (0xf << 0)
#define DP_CEC_RX_MESSAGE_LEN_SHIFT            0
#define DP_CEC_RX_MESSAGE_HPD_STATE            BIT(4)
#define DP_CEC_RX_MESSAGE_HPD_LOST             BIT(5)
#define DP_CEC_RX_MESSAGE_ACKED                BIT(6)
#define DP_CEC_RX_MESSAGE_ENDED                BIT(7)

/* 03003h CEC_TX_MESSAGE_INFO
 *    3:0 CEC_TX_MESSAGE_LEN
 *    6:4 CEC_TX_RETRY_COUNT
 *      7 CEC_TX_MESSAGE_SEND / CEC_TX MESSAGE_BUFFER_UNAVAILABLE
 */
#define DP_CEC_TX_MESSAGE_INFO                 0x3003
#define DP_CEC_TX_MESSAGE_LEN_MASK             (0xf << 0)
#define DP_CEC_TX_MESSAGE_LEN_SHIFT            0
#define DP_CEC_TX_RETRY_COUNT_MASK             (0x7 << 4)
#define DP_CEC_TX_RETRY_COUNT_SHIFT            4
#define DP_CEC_TX_MESSAGE_SEND                 BIT(7)

/* 03004h CEC_TUNNELING_IRQ_FLAGS
 *      0 CEC_RX_MESSAGE_INFO_VALID
 *      1 CEC_RX_MESSAGE_OVERFLOW
 *      4 CEC_TX_MESSAGE_SENT
 *      5 CEC_TX_LINE_ERROR
 *      6 CEC_TX_ADDRESS_NACK_ERROR
 *      7 CEC_TX_DATA_NACK_ERROR
 */
#define DP_CEC_TUNNELING_IRQ_FLAGS             0x3004
#define DP_CEC_RX_MESSAGE_INFO_VALID           BIT(0)
#define DP_CEC_RX_MESSAGE_OVERFLOW             BIT(1)
#define DP_CEC_TX_MESSAGE_SENT                 BIT(4)
#define DP_CEC_TX_LINE_ERROR                   BIT(5)
#define DP_CEC_TX_ADDRESS_NACK_ERROR           BIT(6)
#define DP_CEC_TX_DATA_NACK_ERROR              BIT(7)

/* 0300Eh ~ 0300Fh CEC_LOGICAL_ADDRESS_MASK
 *   14:0 CEC_LOGICAL_ADDRESS_MASK
 *     15 CEC_LOGICAL_ADDRESS_MASK
 */
#define DP_CEC_LOGICAL_ADDRESS_MASK            0x300E /* 0x300F word */
#define DP_CEC_LOGICAL_ADDRESS_0               BIT(0)
#define DP_CEC_LOGICAL_ADDRESS_1               BIT(1)
#define DP_CEC_LOGICAL_ADDRESS_2               BIT(2)
#define DP_CEC_LOGICAL_ADDRESS_3               BIT(3)
#define DP_CEC_LOGICAL_ADDRESS_4               BIT(4)
#define DP_CEC_LOGICAL_ADDRESS_5               BIT(5)
#define DP_CEC_LOGICAL_ADDRESS_6               BIT(6)
#define DP_CEC_LOGICAL_ADDRESS_7               BIT(7)
#define DP_CEC_LOGICAL_ADDRESS_MASK_2          0x300F /* 0x300E word */
#define DP_CEC_LOGICAL_ADDRESS_8               BIT(0)
#define DP_CEC_LOGICAL_ADDRESS_9               BIT(1)
#define DP_CEC_LOGICAL_ADDRESS_10              BIT(2)
#define DP_CEC_LOGICAL_ADDRESS_11              BIT(3)
#define DP_CEC_LOGICAL_ADDRESS_12              BIT(4)
#define DP_CEC_LOGICAL_ADDRESS_13              BIT(5)
#define DP_CEC_LOGICAL_ADDRESS_14              BIT(6)
#define DP_CEC_LOGICAL_ADDRESS_15              BIT(7)

/* 03010h ~ 0301Fh CEC_RX_MESSAGE_BUFFER */
#define DP_CEC_RX_MESSAGE_BUFFER               0x3010
/* 03020h ~ 0302Fh CEC_TX_MESSAGE_BUFFER */
#define DP_CEC_TX_MESSAGE_BUFFER               0x3020
#define DP_CEC_MESSAGE_BUFFER_LENGTH           0x10

/* PCON CONFIGURE-1 FRL FOR HDMI SINK */
#define DP_PCON_HDMI_LINK_CONFIG_1             0x305A
#define DP_PCON_ENABLE_MAX_FRL_BW              (7 << 0)
#define DP_PCON_ENABLE_MAX_BW_0GBPS            0
#define DP_PCON_ENABLE_MAX_BW_9GBPS            1
#define DP_PCON_ENABLE_MAX_BW_18GBPS           2
#define DP_PCON_ENABLE_MAX_BW_24GBPS           3
#define DP_PCON_ENABLE_MAX_BW_32GBPS           4
#define DP_PCON_ENABLE_MAX_BW_40GBPS           5
#define DP_PCON_ENABLE_MAX_BW_48GBPS           6
#define DP_PCON_ENABLE_SOURCE_CTL_MODE         BIT(3)
#define DP_PCON_ENABLE_CONCURRENT_LINK         BIT(4)
#define DP_PCON_ENABLE_SEQUENTIAL_LINK         (0 << 4)
#define DP_PCON_ENABLE_LINK_FRL_MODE           BIT(5)
#define DP_PCON_ENABLE_HPD_READY               BIT(6)
#define DP_PCON_ENABLE_HDMI_LINK               BIT(7)

/* PCON CONFIGURE-2 FRL FOR HDMI SINK */
#define DP_PCON_HDMI_LINK_CONFIG_2             0x305B
#define DP_PCON_MAX_LINK_BW_MASK               (0x3F << 0)
#define DP_PCON_FRL_BW_MASK_9GBPS              BIT(0)
#define DP_PCON_FRL_BW_MASK_18GBPS             BIT(1)
#define DP_PCON_FRL_BW_MASK_24GBPS             BIT(2)
#define DP_PCON_FRL_BW_MASK_32GBPS             BIT(3)
#define DP_PCON_FRL_BW_MASK_40GBPS             BIT(4)
#define DP_PCON_FRL_BW_MASK_48GBPS             BIT(5)
#define DP_PCON_FRL_LINK_TRAIN_EXTENDED        BIT(6)
#define DP_PCON_FRL_LINK_TRAIN_NORMAL          (0 << 6)

/* PCON HDMI LINK STATUS */
#define DP_PCON_HDMI_TX_LINK_STATUS            0x303B
#define DP_PCON_HDMI_TX_LINK_ACTIVE            BIT(0)
#define DP_PCON_FRL_READY                      BIT(1)

/* PCON HDMI POST FRL STATUS */
#define DP_PCON_HDMI_POST_FRL_STATUS           0x3036
#define DP_PCON_HDMI_LINK_MODE                 BIT(0)
#define DP_PCON_HDMI_MODE_TMDS                 0
#define DP_PCON_HDMI_MODE_FRL                  1
#define DP_PCON_HDMI_FRL_TRAINED_BW            (0x3F << 1)
#define DP_PCON_FRL_TRAINED_BW_9GBPS           BIT(1)
#define DP_PCON_FRL_TRAINED_BW_18GBPS          BIT(2)
#define DP_PCON_FRL_TRAINED_BW_24GBPS          BIT(3)
#define DP_PCON_FRL_TRAINED_BW_32GBPS          BIT(4)
#define DP_PCON_FRL_TRAINED_BW_40GBPS          BIT(5)
#define DP_PCON_FRL_TRAINED_BW_48GBPS          BIT(6)

#define DP_PROTOCOL_CONVERTER_CONTROL_0        0x3050 /* DP 1.3 */
#define DP_HDMI_DVI_OUTPUT_CONFIG              BIT(0) /* DP 1.3 */
#define DP_PROTOCOL_CONVERTER_CONTROL_1        0x3051 /* DP 1.3 */
#define DP_CONVERSION_TO_YCBCR420_ENABLE       BIT(0) /* DP 1.3 */
#define DP_HDMI_EDID_PROCESSING_DISABLE        BIT(1) /* DP 1.4 */
#define DP_HDMI_AUTONOMOUS_SCRAMBLING_DISABLE  BIT(2) /* DP 1.4 */
#define DP_HDMI_FORCE_SCRAMBLING               BIT(3) /* DP 1.4 */
#define DP_PROTOCOL_CONVERTER_CONTROL_2        0x3052 /* DP 1.3 */
#define DP_CONVERSION_TO_YCBCR422_ENABLE       BIT(0) /* DP 1.3 */
#define DP_PCON_ENABLE_DSC_ENCODER             BIT(1)
#define DP_PCON_ENCODER_PPS_OVERRIDE_MASK      (0x3 << 2)
#define DP_PCON_ENC_PPS_OVERRIDE_DISABLED      0
#define DP_PCON_ENC_PPS_OVERRIDE_EN_PARAMS     1
#define DP_PCON_ENC_PPS_OVERRIDE_EN_BUFFER     2
#define DP_CONVERSION_RGB_YCBCR_MASK           (7 << 4)
#define DP_CONVERSION_BT601_RGB_YCBCR_ENABLE   BIT(4)
#define DP_CONVERSION_BT709_RGB_YCBCR_ENABLE   BIT(5)
#define DP_CONVERSION_BT2020_RGB_YCBCR_ENABLE  BIT(6)

/* PCON Downstream HDMI ERROR Status per Lane */
#define DP_PCON_HDMI_ERROR_STATUS_LN0          0x3037
#define DP_PCON_HDMI_ERROR_STATUS_LN1          0x3038
#define DP_PCON_HDMI_ERROR_STATUS_LN2          0x3039
#define DP_PCON_HDMI_ERROR_STATUS_LN3          0x303A
#define DP_PCON_HDMI_ERROR_COUNT_MASK          (0x7 << 0)
#define DP_PCON_HDMI_ERROR_COUNT_THREE_PLUS    BIT(0)
#define DP_PCON_HDMI_ERROR_COUNT_TEN_PLUS      BIT(1)
#define DP_PCON_HDMI_ERROR_COUNT_HUNDRED_PLUS  BIT(2)

/* PCON HDMI CONFIG PPS Override Buffer
 * Valid Offsets to be added to Base : 0-127
 */
#define DP_PCON_HDMI_PPS_OVERRIDE_BASE         0x3100

/* PCON HDMI CONFIG PPS Override Parameter: Slice height
 * Offset-0 8LSBs of the Slice height.
 * Offset-1 8MSBs of the Slice height.
 */
#define DP_PCON_HDMI_PPS_OVRD_SLICE_HEIGHT     0x3180

/* PCON HDMI CONFIG PPS Override Parameter: Slice width
 * Offset-0 8LSBs of the Slice width.
 * Offset-1 8MSBs of the Slice width.
 */
#define DP_PCON_HDMI_PPS_OVRD_SLICE_WIDTH      0x3182

/* PCON HDMI CONFIG PPS Override Parameter: bits_per_pixel
 * Offset-0 8LSBs of the bits_per_pixel.
 * Offset-1 2MSBs of the bits_per_pixel.
 */
#define DP_PCON_HDMI_PPS_OVRD_BPP              0x3184

/* 02200h through 022FFh DPCD Extended Receiver Capability Field
 * The Extended Receiver Capability field at DPCD Addresses 02200h through 022FFh is valid
 * with DPCD r1.4 (and higher).
 */
#define DP_DP13_DPCD_REV                    0x2200

/* HDCP 1.3 and HDCP 2.2 */
/* 68000h ~ 68FFFh HDCP v1.3 */
#define DP_AUX_HDCP_BKSV                       0x68000
#define DP_AUX_HDCP_RI_PRIME                   0x68005
#define DP_AUX_HDCP_AKSV                       0x68007
#define DP_AUX_HDCP_AN                         0x6800C
#define DP_AUX_HDCP_V_PRIME(h)                 (0x68014 + (h) * 4)
#define DP_AUX_HDCP_BCAPS                      0x68028
#define DP_BCAPS_REPEATER_PRESENT              BIT(1)
#define DP_BCAPS_HDCP_CAPABLE                  BIT(0)
#define DP_AUX_HDCP_BSTATUS                    0x68029
#define DP_BSTATUS_REAUTH_REQ                  BIT(3)
#define DP_BSTATUS_LINK_FAILURE                BIT(2)
#define DP_BSTATUS_R0_PRIME_READY              BIT(1)
#define DP_BSTATUS_READY                       BIT(0)
#define DP_AUX_HDCP_BINFO                      0x6802A
#define DP_AUX_HDCP_KSV_FIFO                   0x6802C
#define DP_AUX_HDCP_AINFO                      0x6803B

/* DP HDCP2.2 parameter offsets in DPCD address space */
/* 69000h ~ 69FFFh HDCP v1.3 */
#define DP_HDCP_2_2_REG_RTX_OFFSET             0x69000
#define DP_HDCP_2_2_REG_TXCAPS_OFFSET          0x69008
#define DP_HDCP_2_2_REG_CERT_RX_OFFSET         0x6900B
#define DP_HDCP_2_2_REG_RRX_OFFSET             0x69215
#define DP_HDCP_2_2_REG_RX_CAPS_OFFSET         0x6921D
#define DP_HDCP_2_2_REG_EKPUB_KM_OFFSET        0x69220
#define DP_HDCP_2_2_REG_EKH_KM_WR_OFFSET       0x692A0
#define DP_HDCP_2_2_REG_M_OFFSET               0x692B0
#define DP_HDCP_2_2_REG_HPRIME_OFFSET          0x692C0
#define DP_HDCP_2_2_REG_EKH_KM_RD_OFFSET       0x692E0
#define DP_HDCP_2_2_REG_RN_OFFSET              0x692F0
#define DP_HDCP_2_2_REG_LPRIME_OFFSET          0x692F8
#define DP_HDCP_2_2_REG_EDKEY_KS_OFFSET        0x69318
#define DP_HDCP_2_2_REG_RIV_OFFSET             0x69328
#define DP_HDCP_2_2_REG_RXINFO_OFFSET          0x69330
#define DP_HDCP_2_2_REG_SEQ_NUM_V_OFFSET       0x69332
#define DP_HDCP_2_2_REG_VPRIME_OFFSET          0x69335
#define DP_HDCP_2_2_REG_RECV_ID_LIST_OFFSET    0x69345
#define DP_HDCP_2_2_REG_V_OFFSET               0x693E0
#define DP_HDCP_2_2_REG_SEQ_NUM_M_OFFSET       0x693F0
#define DP_HDCP_2_2_REG_K_OFFSET               0x693F3
#define DP_HDCP_2_2_REG_STREAM_ID_TYPE_OFFSET  0x693F5
#define DP_HDCP_2_2_REG_MPRIME_OFFSET          0x69473
#define DP_HDCP_2_2_REG_RXSTATUS_OFFSET        0x69493
#define DP_RXSTATUS_LINK_INTEGRITY_FAILURE     BIT(4)
#define DP_RXSTATUS_REAUTH_REQ                 BIT(3)
#define DP_RXSTATUS_PAIRING_AVAILABLE          BIT(2)
#define DP_RXSTATUS_H_AVAILABLE                BIT(1)
#define DP_RXSTATUS_READY                      BIT(0)

#define DP_HDCP_2_2_REG_STREAM_TYPE_OFFSET     0x69494
#define DP_HDCP_2_2_REG_DBG_OFFSET             0x69518

/* DP-tunneling */
#define DP_TUNNELING_OUI                       0xe0000
#define DP_TUNNELING_OUI_BYTES                 3

#define DP_TUNNELING_DEV_ID                    0xe0003
#define DP_TUNNELING_DEV_ID_BYTES              6

#define DP_TUNNELING_HW_REV                    0xe0009
#define DP_TUNNELING_HW_REV_MAJOR_SHIFT        4
#define DP_TUNNELING_HW_REV_MAJOR_MASK         (0xf << DP_TUNNELING_HW_REV_MAJOR_SHIFT)
#define DP_TUNNELING_HW_REV_MINOR_SHIFT        0
#define DP_TUNNELING_HW_REV_MINOR_MASK         (0xf << DP_TUNNELING_HW_REV_MINOR_SHIFT)

#define DP_TUNNELING_SW_REV_MAJOR              0xe000a
#define DP_TUNNELING_SW_REV_MINOR              0xe000b

#define DP_TUNNELING_CAPABILITIES              0xe000d
#define DP_IN_BW_ALLOCATION_MODE_SUPPORT       BIT(7)
#define DP_PANEL_REPLAY_OPTIMIZATION_SUPPORT   BIT(6)
#define DP_TUNNELING_SUPPORT                   BIT(0)

#define DP_IN_ADAPTER_INFO                     0xe000e
#define DP_IN_ADAPTER_NUMBER_BITS              7
#define DP_IN_ADAPTER_NUMBER_MASK              ((1 << DP_IN_ADAPTER_NUMBER_BITS) - 1)

#define DP_USB4_DRIVER_ID                      0xe000f
#define DP_USB4_DRIVER_ID_BITS                 4
#define DP_USB4_DRIVER_ID_MASK                 ((1 << DP_USB4_DRIVER_ID_BITS) - 1)

#define DP_USB4_DRIVER_BW_CAPABILITY              0xe0020
#define DP_USB4_DRIVER_BW_ALLOCATION_MODE_SUPPORT BIT(7)

#define DP_IN_ADAPTER_TUNNEL_INFORMATION       0xe0021
#define DP_GROUP_ID_BITS                       3
#define DP_GROUP_ID_MASK                       ((1 << DP_GROUP_ID_BITS) - 1)

#define DP_BW_GRANULARITY                      0xe0022
#define DP_BW_GRANULARITY_MASK                 0x3

#define DP_ESTIMATED_BW                        0xe0023
#define DP_ALLOCATED_BW                        0xe0024

#define DP_TUNNELING_STATUS                    0xe0025
#define DP_BW_ALLOCATION_CAPABILITY_CHANGED    BIT(3)
#define DP_ESTIMATED_BW_CHANGED                BIT(2)
#define DP_BW_REQUEST_SUCCEEDED                BIT(1)
#define DP_BW_REQUEST_FAILED                   BIT(0)

#define DP_TUNNELING_MAX_LINK_RATE             0xe0028

#define DP_TUNNELING_MAX_LANE_COUNT            0xe0029
#define DP_TUNNELING_MAX_LANE_COUNT_MASK       0x1f

#define DP_DPTX_BW_ALLOCATION_MODE_CONTROL          0xe0030
#define DP_DISPLAY_DRIVER_BW_ALLOCATION_MODE_ENABLE BIT(7)
#define DP_UNMASK_BW_ALLOCATION_IRQ                 BIT(6)

#define DP_REQUEST_BW                          0xe0031
#define MAX_DP_REQUEST_BW                      255

/* LTTPR: Link Training (LT)-tunable PHY Repeaters */
/* F0000h LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV
 *    3:0 Minor Revision Number
 *    7:4 Major Revision Number
 */
#define DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV 0xf0000 /* 1.3 */
/* F0001h MAX_LINK_RATE_PHY_REPEATER
 *    7:0 MAX_LINK_RATE
 */
#define DP_MAX_LINK_RATE_PHY_REPEATER                       0xf0001 /* 1.4a */

/* F0002h PHY_REPEATER_CNT
 *    7:0   80h = 1 PHY Repeater are present.
 *          40h = 2 PHY Repeaters are present.
 *          20h = 3 PHY Repeaters are present.
 *          01h = 8 PHY Repeaters are present.
 */
#define DP_PHY_REPEATER_CNT                                 0xf0002 /* 1.3 */

/* F0003h PHY_REPEATER_MODE
 *    7:0   55h = Transparent mode (default).
 *          AAh = Non-transparent mode.
 */
#define DP_PHY_REPEATER_MODE                                0xf0003 /* 1.3 */

/* F0004h MAX_LANE_COUNT_PHY_REPEATER
 *    4:0 MAX_LANE_COUNT
 */
#define DP_MAX_LANE_COUNT_PHY_REPEATER                      0xf0004 /* 1.4a */
#define DP_Repeater_FEC_CAPABILITY                          0xf0004 /* 1.4 */

/* F0005h PHY_REPEATER_EXTENDED_WAKE_TIMEOUT
 *    6:0 EXTENDED_WAKE_TIMEOUT_REQUEST
 *      7 EXTENDED_WAKE_TIMEOUT_GRANT
 */
#define DP_PHY_REPEATER_EXTENDED_WAIT_TIMEOUT               0xf0005 /* 1.4a */

/* F0006h MAIN_LINK_CHANNEL_CODING_PHY_REPEATER
 *      0 128b132b_DP_SUPPORTED
 */
#define DP_MAIN_LINK_CHANNEL_CODING_PHY_REPEATER            0xf0006 /* 2.0 */
#define DP_PHY_REPEATER_128B132B_SUPPORTED                  BIT(0)

/* See DP_128B132B_SUPPORTED_LINK_RATES for values */
/* F0007h PHY_REPEATER_128b132b_DP_RATES
 *      0 10 Gbps per Lane Support
 *      1 20 Gbps per Lane Support
 *      2 13.5 Gbps per Lane Support
 */
#define DP_PHY_REPEATER_128B132B_RATES                     0xf0007 /* 2.0 */

/* F0008h EQ Done per LTTPR
 *      0 LTTPR1 shall set this bit after its DFP successfully achieves LANEx_CHANNEL_EQ_DONE
 *      1 LTTPR2 shall set this bit after its DFP successfully achieves LANEx_CHANNEL_EQ_DONE
 *      7 LTTPR8 shall set this bit after its DFP successfully achieves LANEx_CHANNEL_EQ_DONE
 */
#define DP_PHY_REPEATER_EQ_DONE                            0xf0008 /* 2.0 E11 */

#define __DP_LTTPR1_BASE                                   0xf0010 /* 1.3 */
#define __DP_LTTPR2_BASE                                   0xf0060 /* 1.3 */
#define DP_LTTPR_BASE(dp_phy) \
		(__DP_LTTPR1_BASE + (__DP_LTTPR2_BASE - __DP_LTTPR1_BASE) * \
		((dp_phy) - DP_PHY_LTTPR1))

#define DP_LTTPR_REG(dp_phy, lttpr1_reg) \
		(DP_LTTPR_BASE(dp_phy) - DP_LTTPR_BASE(DP_PHY_LTTPR1) + (lttpr1_reg))

/* F0010h TRAINING_PATTERN_SET_PHY_REPEATER1
 */
#define DP_TRAINING_PATTERN_SET_PHY_REPEATER1               0xf0010 /* 1.3 */
#define DP_TRAINING_PATTERN_SET_PHY_REPEATER(dp_phy) \
		DP_LTTPR_REG(dp_phy, DP_TRAINING_PATTERN_SET_PHY_REPEATER1)

/* F0011h TRAINING_LANE0_SET_PHY_REPEATER1
 * For 8b/10b DP Link Layer
 *    1:0 VOLTAGE_SWING_SET
 *      2 MAX_SWING_REACHED
 *    4:3 PRE_EMPHASIS_SET
 *      5 MAX_PRE_EMPHASIS_REACHED
 * For 128b/132b DP Link Layer
 *    3:0 TX_FFE_PRESET_VALUE
 */
#define DP_TRAINING_LANE0_SET_PHY_REPEATER1                 0xf0011 /* 1.3 */
#define DP_TRAINING_LANE0_SET_PHY_REPEATER(dp_phy) \
		DP_LTTPR_REG(dp_phy, DP_TRAINING_LANE0_SET_PHY_REPEATER1)

/* F0012h TRAINING_LANE1_SET_PHY_REPEATER1 */
#define DP_TRAINING_LANE1_SET_PHY_REPEATER1                 0xf0012 /* 1.3 */

/* F0012h TRAINING_LANE2_SET_PHY_REPEATER1 */
#define DP_TRAINING_LANE2_SET_PHY_REPEATER1                 0xf0013 /* 1.3 */

/* F0012h TRAINING_LANE3_SET_PHY_REPEATER1 */
#define DP_TRAINING_LANE3_SET_PHY_REPEATER1                 0xf0014 /* 1.3 */
/* F0020h TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1
 *    6:0 TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1
 */
#define DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1           0xf0020 /* 1.4a */
#define DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy) \
		DP_LTTPR_REG(dp_phy, DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1)

/* F0021h TRANSMITTER_CAPABILITY_PHY_REPEATER1
 * For 8b/10b DP Link Layer
 *      0 VOLTAGE_SWING_LEVEL_3_SUPPORTED
 *      1 PRE_EMPHASIS_LEVEL_3_SUPPORTED
 */
#define DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1             0xf0021 /* 1.4a */
#define DP_VOLTAGE_SWING_LEVEL_3_SUPPORTED                  BIT(0)
#define DP_PRE_EMPHASIS_LEVEL_3_SUPPORTED                   BIT(1)

#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1  0xf0022 /* 2.0 */
#define DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy)        \
		DP_LTTPR_REG(dp_phy, DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1)
/* see DP_128B132B_TRAINING_AUX_RD_INTERVAL for values */

/* F0030h LANE0_1_STATUS_PHY_REPEATER1 */
#define DP_LANE0_1_STATUS_PHY_REPEATER1                      0xf0030 /* 1.3 */
#define DP_LANE0_1_STATUS_PHY_REPEATER(dp_phy) \
		DP_LTTPR_REG(dp_phy, DP_LANE0_1_STATUS_PHY_REPEATER1)

/* F0031h LANE2_3_STATUS_PHY_REPEATER1 */
#define DP_LANE2_3_STATUS_PHY_REPEATER1                      0xf0031 /* 1.3 */

/* F0032h LANE_ALIGN_STATUS_UPDATED_PHY_REPEATER1
 *      0 INTERLANE_ALIGN_DONE
 *      5 AUX_LESS_ALPM_LOCK_TIMEOUT_ERROR_STATUS
 *          0 = No error (default).
 *          1 = AUX-less ALPM lock timeout error. Error status persists until cleared.
 *      7 LINK_STATUS_UPDATED
 */
#define DP_LANE_ALIGN_STATUS_UPDATED_PHY_REPEATER1           0xf0032 /* 1.3 */

/* F0033h ADJUST_REQUEST_LANE0_1_PHY_REPEATER1
 * For 8b/10b DP Link Layer
 *    1:0 VOLTAGE_SWING_LANE0
 *    3:2 PRE_EMPHASIS_LANE0
 *    5:4 VOLTAGE_SWING_LANE1
 *    7:6 PRE_EMPHASIS_LANE1
 * For 128b/132b DP Link Layer
 *    3:0 TX_FFE_PRESET_VALUE_LANE0
 *    7:4 TX_FFE_PRESET_VALUE_LANE1
 */
#define DP_ADJUST_REQUEST_LANE0_1_PHY_REPEATER1              0xf0033 /* 1.3 */
#define DP_ADJUST_REQUEST_LANE2_3_PHY_REPEATER1              0xf0034 /* 1.3 */

/* F0035h ~ F0036h SYMBOL_ERROR_COUNT_LANE0_PHY_REPEATER1 */
#define DP_SYMBOL_ERROR_COUNT_LANE0_PHY_REPEATER1            0xf0035 /* 1.3 */

/* F0037h ~ F0038h SYMBOL_ERROR_COUNT_LANE1_PHY_REPEATER1 */
#define DP_SYMBOL_ERROR_COUNT_LANE1_PHY_REPEATER1            0xf0037 /* 1.3 */

/* F0039h ~ F003ah SYMBOL_ERROR_COUNT_LANE2_PHY_REPEATER1 */
#define DP_SYMBOL_ERROR_COUNT_LANE2_PHY_REPEATER1            0xf0039 /* 1.3 */

/* F003bh ~ F003ch SYMBOL_ERROR_COUNT_LANE2_PHY_REPEATER1 */
#define DP_SYMBOL_ERROR_COUNT_LANE3_PHY_REPEATER1            0xf003b /* 1.3 */

/* F003Dh ~ F003Fh LT-tunable PHY Repeater IEEE_OUI */
#define DP_OUI_PHY_REPEATER1                                 0xf003d /* 1.3 */
#define DP_OUI_PHY_REPEATER(dp_phy) \
		DP_LTTPR_REG(dp_phy, DP_OUI_PHY_REPEATER1)

#define __DP_FEC1_BASE                                       0xf0290 /* 1.4 */
#define __DP_FEC2_BASE                                       0xf0298 /* 1.4 */
#define DP_FEC_BASE(dp_phy) \
		(__DP_FEC1_BASE + ((__DP_FEC2_BASE - __DP_FEC1_BASE) * \
		((dp_phy) - DP_PHY_LTTPR1)))

#define DP_FEC_REG(dp_phy, fec1_reg) \
		(DP_FEC_BASE(dp_phy) - DP_FEC_BASE(DP_PHY_LTTPR1) + (fec1_reg))

/* F0290h FEC_STATUS_PHY_REPEATER1
 *      0 FEC_DECODE_EN_DETECTED
 *      1 FEC_DECODE_DIS_DETECTED
 *      2 FEC_RUNNING_INDICATOR
 */
#define DP_FEC_STATUS_PHY_REPEATER1                          0xf0290 /* 1.4 */
#define DP_FEC_STATUS_PHY_REPEATER(dp_phy) \
		DP_FEC_REG(dp_phy, DP_FEC_STATUS_PHY_REPEATER1)

/* F0291h FEC_ERROR_COUNT_PHY_REPEATER1
 *    7:0 FEC_ERROR_COUNT_PHY_REPEATER1_7:0
 */
/* F0292h FEC_ERROR_COUNT_PHY_REPEATER1
 *    6:0 FEC_ERROR_COUNT_PHY_REPEATER1_14:8
 *      0 FEC_ERROR_COUNT_VALID
 */
#define DP_FEC_ERROR_COUNT_PHY_REPEATER1                     0xf0291 /* 1.4 */

/* F0294h FEC_CAPABILITY_0_PHY_REPEATER1
 *      0 FEC_CAPABLE
 *          0 = LTTPR is agnostic to FEC operation.
 *          1 = LTTPR performs FEC decode at its UFP and FEC encode at its DFP.
 *    7:1 Bit definitions are identical to the FEC_CAPABILITY_0 register
 */
#define DP_FEC_CAPABILITY_PHY_REPEATER1                      0xf0294 /* 1.4a */

#define DP_LTTPR_MAX_ADD                                     0xf02ff /* 1.4 */

#define DP_DPCD_MAX_ADD                                      0xfffff /* 1.4 */

#endif /* _DPCD_REG_H_ */
